#include "app_meow_link.h"

#include "bsp_ble.h"
#include "bsp_wifi.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"

#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static const char *TAG = "meow_link";

#define WIRE_MAGIC 0x324F454Du
#define UDP_PORT   19511
#define SEEK_US    8000000ll

static const ble_uuid128_t UUID_MW = BLE_UUID128_INIT(
    0x01, 0x00, 0x57, 0x4f, 0x45, 0x4d, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x4d, 0x45, 0x4f, 0x57);
static const ble_uuid128_t UUID_TX = BLE_UUID128_INIT(
    0x02, 0x00, 0x57, 0x4f, 0x45, 0x4d, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x4d, 0x45, 0x4f, 0x57);
static const ble_uuid128_t UUID_RX = BLE_UUID128_INIT(
    0x03, 0x00, 0x57, 0x4f, 0x45, 0x4d, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x4d, 0x45, 0x4f, 0x57);

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t type;
    uint8_t stage;
    uint8_t hunger;
    uint8_t happy;
    uint8_t health;
    uint8_t sick;
    uint8_t form;
    uint8_t weight;
    uint8_t rng;
    uint8_t pad;
    uint16_t age_min;
    uint8_t id[6];
} wire_t;

static uint16_t s_tx_handle;
static uint16_t s_sub[4];
static int s_sub_n;
static bool s_noted;
static uint16_t s_cent;
static uint16_t s_peer_tx, s_peer_rx, s_svc_lo, s_svc_hi;
static uint8_t s_own[6];
static ble_addr_t s_want;
static bool s_have_want;
static bool s_busy;
static int s_kind;
static int64_t s_t0;
static int64_t s_udp_last;
static int s_udp = -1;
static volatile int s_got;
static wire_t s_rx;
static wire_t s_tx;

static void send_pkt(void);
static int disc_event(struct ble_gap_event *event, void *arg);

static int access_cb(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn;
    (void)attr;
    (void)arg;
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t z = 0;
        return os_mbuf_append(ctxt->om, &z, 1);
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR ||
        ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
        uint16_t n = OS_MBUF_PKTLEN(ctxt->om);
        if (n < sizeof(wire_t)) return 0;
        wire_t w;
        memset(&w, 0, sizeof(w));
        os_mbuf_copydata(ctxt->om, 0, sizeof(w), &w);
        if (w.magic == WIRE_MAGIC && memcmp(w.id, s_own, 6) != 0) {
            s_rx = w;
            s_got = 1;
        }
        return 0;
    }
    return 0;
}

static const struct ble_gatt_svc_def s_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_MW.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &UUID_TX.u,
                .access_cb = access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_tx_handle,
            },
            {
                .uuid = &UUID_RX.u,
                .access_cb = access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 },
        },
    },
    { 0 },
};

static void add_sub(uint16_t conn)
{
    for (int i = 0; i < s_sub_n; i++) if (s_sub[i] == conn) return;
    if (s_sub_n < 4) s_sub[s_sub_n++] = conn;
}

static void del_sub(uint16_t conn)
{
    for (int i = 0; i < s_sub_n; i++) {
        if (s_sub[i] == conn) {
            s_sub[i] = s_sub[--s_sub_n];
            return;
        }
    }
}

static void set_beacon(bool looking)
{
    uint8_t mfg[4] = { 0x54, 0x4D, (uint8_t)s_kind, looking ? 1 : 0 };
    bsp_ble_set_scan_mfg(mfg, sizeof(mfg));
    bsp_ble_refresh_adv();
}

static void fill_tx(const app_meow_t *pet)
{
    app_meow_snap_t s;
    memset(&s_tx, 0, sizeof(s_tx));
    app_meow_snap(pet, &s);
    s_tx.magic = WIRE_MAGIC;
    s_tx.type = (uint8_t)s_kind;
    s_tx.stage = s.stage;
    s_tx.hunger = s.hunger;
    s_tx.happy = s.happy;
    s_tx.health = s.health;
    s_tx.sick = s.sick;
    s_tx.form = s.form;
    s_tx.pad = s.species;
    s_tx.weight = s.weight;
    s_tx.rng = s.rng;
    s_tx.age_min = s.age_min;
    memcpy(s_tx.id, s_own, 6);
}

static void send_pkt(void)
{
    if (!s_busy) return;
    const uint8_t *p = (const uint8_t *)&s_tx;
    size_t n = sizeof(s_tx);
    if (s_cent && s_peer_rx) {
        ble_gattc_write_no_rsp_flat(s_cent, s_peer_rx, p, n);
    }
    for (int i = 0; i < s_sub_n; i++) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(p, n);
        if (!om) break;
        int rc = ble_gatts_notify_custom(s_sub[i], s_tx_handle, om);
        if (rc != 0) os_mbuf_free_chain(om);
    }
}

static int disc_chr_cb(uint16_t conn, const struct ble_gatt_error *err,
                       const struct ble_gatt_chr *chr, void *arg)
{
    (void)arg;
    if (!chr || (err && err->status != 0 && err->status != BLE_HS_EDONE)) return 0;
    if (err && err->status == BLE_HS_EDONE) {
        if (s_peer_tx) {
            uint8_t cccd[2] = { 0x01, 0x00 };
            ble_gattc_write_flat(conn, s_peer_tx + 1, cccd, sizeof(cccd), NULL, NULL);
        }
        send_pkt();
        return 0;
    }
    if (ble_uuid_cmp(&chr->uuid.u, &UUID_TX.u) == 0) s_peer_tx = chr->val_handle;
    if (ble_uuid_cmp(&chr->uuid.u, &UUID_RX.u) == 0) s_peer_rx = chr->val_handle;
    return 0;
}

static int disc_svc_cb(uint16_t conn, const struct ble_gatt_error *err,
                       const struct ble_gatt_svc *svc, void *arg)
{
    (void)arg;
    if (err && err->status == BLE_HS_EDONE) {
        if (s_svc_lo) ble_gattc_disc_all_chrs(conn, s_svc_lo, s_svc_hi, disc_chr_cb, NULL);
        return 0;
    }
    if (!svc || (err && err->status != 0)) return 0;
    if (ble_uuid_cmp(&svc->uuid.u, &UUID_MW.u) == 0) {
        s_svc_lo = svc->start_handle;
        s_svc_hi = svc->end_handle;
    }
    return 0;
}

static int cent_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            if (s_noted) {
                bsp_ble_note_app_conn(-1);
                s_noted = false;
            }
            s_cent = 0;
            bsp_ble_refresh_adv();
            return 0;
        }
        s_cent = event->connect.conn_handle;
        s_peer_tx = s_peer_rx = s_svc_lo = s_svc_hi = 0;
        ble_gattc_exchange_mtu(s_cent, NULL, NULL);
        ble_gattc_disc_all_svcs(s_cent, disc_svc_cb, NULL);
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        s_cent = 0;
        s_peer_tx = s_peer_rx = 0;
        if (s_noted) {
            bsp_ble_note_app_conn(-1);
            s_noted = false;
        }
        bsp_ble_refresh_adv();
        return 0;
    case BLE_GAP_EVENT_NOTIFY_RX: {
        uint16_t n = OS_MBUF_PKTLEN(event->notify_rx.om);
        if (n < sizeof(wire_t)) return 0;
        wire_t w;
        memset(&w, 0, sizeof(w));
        os_mbuf_copydata(event->notify_rx.om, 0, sizeof(w), &w);
        if (w.magic == WIRE_MAGIC && memcmp(w.id, s_own, 6) != 0) {
            s_rx = w;
            s_got = 1;
        }
        return 0;
    }
    default:
        return 0;
    }
}

static bool uuid_in_fields(const struct ble_hs_adv_fields *f)
{
    for (int i = 0; i < f->num_uuids128; i++) {
        if (ble_uuid_cmp(&f->uuids128[i].u, &UUID_MW.u) == 0) return true;
    }
    return false;
}

static bool mfg_match(const struct ble_hs_adv_fields *f)
{
    if (!f->mfg_data || f->mfg_data_len < 4) return false;
    if (f->mfg_data[0] != 0x54 || f->mfg_data[1] != 0x4D) return false;
    if (f->mfg_data[2] != (uint8_t)s_kind) return false;
    return f->mfg_data[3] != 0;
}

static int disc_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        bsp_ble_refresh_adv();
        return 0;
    }
    if (event->type != BLE_GAP_EVENT_DISC) return 0;
    if (!s_busy || s_cent || s_have_want) return 0;
    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) != 0) {
        return 0;
    }
    if (!uuid_in_fields(&fields) || !mfg_match(&fields)) return 0;
    if (memcmp(event->disc.addr.val, s_own, 6) == 0) return 0;
    if (memcmp(event->disc.addr.val, s_own, 6) <= 0) return 0;
    s_want = event->disc.addr;
    s_have_want = true;
    ble_gap_disc_cancel();
    if (!s_noted) {
        bsp_ble_note_app_conn(1);
        s_noted = true;
    }
    int rc = ble_gap_connect(bsp_ble_own_addr_type(), &s_want, 8000, NULL, cent_event, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "connect rc=%d", rc);
        bsp_ble_note_app_conn(-1);
        s_noted = false;
        s_have_want = false;
        bsp_ble_refresh_adv();
    }
    return 0;
}

static void on_gap(void *ev)
{
    struct ble_gap_event *event = ev;
    if (!event) return;
    if (event->type == BLE_GAP_EVENT_SUBSCRIBE) {
        if (s_tx_handle && event->subscribe.attr_handle == s_tx_handle) {
            if (event->subscribe.cur_notify) {
                add_sub(event->subscribe.conn_handle);
                send_pkt();
            } else {
                del_sub(event->subscribe.conn_handle);
            }
        }
        return;
    }
    if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        del_sub(event->disconnect.conn.conn_handle);
    }
}

static void udp_close(void)
{
    if (s_udp >= 0) {
        close(s_udp);
        s_udp = -1;
    }
}

static void udp_open(void)
{
    udp_close();
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return;
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(UDP_PORT);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&a, sizeof(a)) != 0) {
        close(fd);
        return;
    }
    s_udp = fd;
}

static void udp_send(void)
{
    if (s_udp < 0 || !s_busy) return;
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(UDP_PORT);
    a.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sendto(s_udp, &s_tx, sizeof(s_tx), 0, (struct sockaddr *)&a, sizeof(a));
}

static void udp_recv(void)
{
    if (s_udp < 0 || !s_busy || s_got) return;
    wire_t w;
    struct sockaddr_in from;
    socklen_t fl = sizeof(from);
    int n = recvfrom(s_udp, &w, sizeof(w), 0, (struct sockaddr *)&from, &fl);
    if (n != (int)sizeof(w)) return;
    if (w.magic != WIRE_MAGIC) return;
    if (memcmp(w.id, s_own, 6) == 0) return;
    if (w.type != (uint8_t)s_kind) return;
    s_rx = w;
    s_got = 1;
}

static void stop_seek(void)
{
    s_busy = false;
    s_have_want = false;
    if (bsp_ble_stack_up()) {
        ble_gap_disc_cancel();
        if (s_cent) {
            ble_gap_terminate(s_cent, BLE_ERR_REM_USER_CONN_TERM);
            s_cent = 0;
        } else if (s_noted) {
            bsp_ble_note_app_conn(-1);
            s_noted = false;
        }
    } else {
        s_cent = 0;
        s_noted = false;
    }
    s_kind = 0;
    set_beacon(false);
    udp_close();
}

static int apply_rx(app_meow_t *pet)
{
    if (s_rx.magic != WIRE_MAGIC || s_rx.type != (uint8_t)s_kind) {
        return APP_MEOW_LINK_FAIL;
    }
    if (s_kind == APP_MEOW_KIND_VISIT) {
        if (app_meow_visit(pet) != APP_MEOW_OK) return APP_MEOW_LINK_FAIL;
        return APP_MEOW_LINK_VISIT;
    }
    app_meow_snap_t you = {
        .stage = s_rx.stage,
        .hunger = s_rx.hunger,
        .happy = s_rx.happy,
        .health = s_rx.health,
        .sick = s_rx.sick,
        .form = s_rx.form,
        .species = s_rx.pad,
        .weight = s_rx.weight,
        .rng = s_rx.rng,
        .age_min = s_rx.age_min,
    };
    int r = app_meow_fight(pet, &you);
    if (r > 0) return APP_MEOW_LINK_WIN;
    if (r == 0) return APP_MEOW_LINK_DRAW;
    if (r == -1) return APP_MEOW_LINK_LOSE;
    return APP_MEOW_LINK_FAIL;
}

static void start_scan(void)
{
    if (!bsp_ble_enabled() || !bsp_ble_stack_up()) return;
    struct ble_gap_disc_params dp;
    memset(&dp, 0, sizeof(dp));
    dp.itvl = BLE_GAP_SCAN_FAST_INTERVAL_MIN;
    dp.window = BLE_GAP_SCAN_FAST_WINDOW;
    dp.passive = 0;
    dp.filter_duplicates = 1;
    int rc = ble_gap_disc(bsp_ble_own_addr_type(), 4000, &dp, disc_event, NULL);
    if (rc == BLE_HS_EBUSY) {
        ble_gap_adv_stop();
        rc = ble_gap_disc(bsp_ble_own_addr_type(), 4000, &dp, disc_event, NULL);
    }
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGW(TAG, "scan rc=%d", rc);
        bsp_ble_refresh_adv();
    }
}

void app_meow_link_prepare(void)
{
    bsp_ble_set_extra_svcs(s_svcs);
    bsp_ble_set_scan_uuid128(UUID_MW.value);
    bsp_ble_set_gap_cb(on_gap);
}

void app_meow_link_start(void)
{
    esp_read_mac(s_own, ESP_MAC_BT);
    s_kind = 0;
    set_beacon(false);
    if (bsp_ble_enabled()) bsp_ble_ensure_advertising();
}

bool app_meow_link_seek(app_meow_t *pet, int kind)
{
    if (s_busy) app_meow_link_cancel();
    if (!pet || (kind != APP_MEOW_KIND_VISIT && kind != APP_MEOW_KIND_FIGHT)) return false;
    if (!app_meow_can_link(pet)) return false;
    bool wifi_ok = bsp_wifi_enabled() && bsp_wifi_state() == BSP_WIFI_CONNECTED;
    if (!bsp_ble_enabled()) {
        (void)bsp_ble_set_enabled(true);
    }
    if (!bsp_ble_enabled() && !wifi_ok) return false;

    s_kind = kind;
    s_busy = true;
    s_got = 0;
    s_have_want = false;
    s_t0 = esp_timer_get_time();
    s_udp_last = 0;
    fill_tx(pet);
    if (wifi_ok) udp_open();
    if (bsp_ble_enabled()) {
        set_beacon(true);
        bsp_ble_ensure_advertising();
        start_scan();
    }
    udp_send();
    return true;
}

void app_meow_link_cancel(void)
{
    s_got = 0;
    stop_seek();
}

bool app_meow_link_busy(void)
{
    return s_busy;
}

int app_meow_link_poll(app_meow_t *pet)
{
    if (!s_busy) return APP_MEOW_LINK_IDLE;
    udp_recv();
    if (s_got) {
        int r = apply_rx(pet);
        send_pkt();
        stop_seek();
        s_got = 0;
        return r;
    }
    if (esp_timer_get_time() - s_t0 > SEEK_US) {
        stop_seek();
        return APP_MEOW_LINK_NONE;
    }
    if (esp_timer_get_time() - s_udp_last > 400000) {
        s_udp_last = esp_timer_get_time();
        udp_send();
        send_pkt();
        if (!s_cent && !s_have_want) start_scan();
    }
    return APP_MEOW_LINK_WAIT;
}
