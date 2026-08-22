#include "walkie_ble.h"

#include "bsp_ble.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"
#include "walkie.h"

#include <string.h>

static const char *TAG = "walkie_ble";

static const ble_uuid128_t UUID_WK = BLE_UUID128_INIT(
    0x01, 0x00, 0x55, 0xfa, 0xa1, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x4b, 0x57, 0x54, 0x50, 0x53, 0x50);
static const ble_uuid128_t UUID_TX = BLE_UUID128_INIT(
    0x02, 0x00, 0x55, 0xfa, 0xa1, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x4b, 0x57, 0x54, 0x50, 0x53, 0x50);
static const ble_uuid128_t UUID_RX = BLE_UUID128_INIT(
    0x03, 0x00, 0x55, 0xfa, 0xa1, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x4b, 0x57, 0x54, 0x50, 0x53, 0x50);

static uint16_t s_tx_handle;
static uint16_t s_sub[4];
static int s_sub_n;
static int s_ch;
static bool s_on;
static bool s_noted;
static uint16_t s_cent;
static uint16_t s_peer_tx;
static uint16_t s_peer_rx;
static uint16_t s_svc_start, s_svc_end;
static uint8_t s_own[6];
static ble_addr_t s_want;
static bool s_have_want;

static int walkie_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
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
        if (n == 0 || n > WALKIE_FRAME_N + 8) return 0;
        uint8_t buf[WALKIE_FRAME_N + 8];
        os_mbuf_copydata(ctxt->om, 0, n, buf);
        walkie_rx_bytes(buf, n);
        return 0;
    }
    return 0;
}

static const struct ble_gatt_svc_def s_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_WK.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &UUID_TX.u,
                .access_cb = walkie_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_tx_handle,
            },
            {
                .uuid = &UUID_RX.u,
                .access_cb = walkie_access,
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

static void set_beacon(bool active)
{
    uint8_t mfg[4] = { 0x53, 0x50, (uint8_t)s_ch, active ? 1 : 0 };
    bsp_ble_set_scan_mfg(mfg, sizeof(mfg));
    bsp_ble_refresh_adv();
}

static int disc_chr_cb(uint16_t conn, const struct ble_gatt_error *err,
                       const struct ble_gatt_chr *chr, void *arg)
{
    (void)arg;
    if (!chr || (err && err->status != 0 && err->status != BLE_HS_EDONE)) {
        return 0;
    }
    if (err && err->status == BLE_HS_EDONE) {
        if (s_peer_tx) {
            uint8_t cccd[2] = { 0x01, 0x00 };
            ble_gattc_write_flat(conn, s_peer_tx + 1, cccd, sizeof(cccd), NULL, NULL);
        }
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
        if (s_svc_start) {
            ble_gattc_disc_all_chrs(conn, s_svc_start, s_svc_end, disc_chr_cb, NULL);
        }
        return 0;
    }
    if (!svc || (err && err->status != 0)) return 0;
    if (ble_uuid_cmp(&svc->uuid.u, &UUID_WK.u) == 0) {
        s_svc_start = svc->start_handle;
        s_svc_end = svc->end_handle;
    }
    return 0;
}

static int cent_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            ESP_LOGW(TAG, "central connect fail %d", event->connect.status);
            if (s_noted) {
                bsp_ble_note_app_conn(-1);
                s_noted = false;
            }
            s_cent = 0;
            bsp_ble_refresh_adv();
            return 0;
        }
        s_cent = event->connect.conn_handle;
        s_peer_tx = s_peer_rx = s_svc_start = s_svc_end = 0;
        ble_gattc_exchange_mtu(s_cent, NULL, NULL);
        ble_gattc_disc_all_svcs(s_cent, disc_svc_cb, NULL);
        walkie_note_peer("BLE");
        ESP_LOGI(TAG, "central linked handle=%u", s_cent);
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "central drop");
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
        if (n == 0 || n > WALKIE_FRAME_N + 8) return 0;
        uint8_t buf[WALKIE_FRAME_N + 8];
        os_mbuf_copydata(event->notify_rx.om, 0, n, buf);
        walkie_rx_bytes(buf, n);
        return 0;
    }
    default:
        return 0;
    }
}

static bool uuid_in_fields(const struct ble_hs_adv_fields *f)
{
    for (int i = 0; i < f->num_uuids128; i++) {
        if (ble_uuid_cmp(&f->uuids128[i].u, &UUID_WK.u) == 0) return true;
    }
    return false;
}

static bool mfg_match(const struct ble_hs_adv_fields *f)
{
    if (!f->mfg_data || f->mfg_data_len < 4) return false;
    if (f->mfg_data[0] != 0x53 || f->mfg_data[1] != 0x50) return false;
    if (f->mfg_data[2] != (uint8_t)s_ch) return false;
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
    if (s_cent || s_have_want) return 0;
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
                walkie_note_peer("BLE");
            } else {
                del_sub(event->subscribe.conn_handle);
            }
        }
        return;
    }
    if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        del_sub(event->disconnect.conn.conn_handle);
    }
    if (event->type == BLE_GAP_EVENT_NOTIFY_RX && s_on) {
        if (s_cent && event->notify_rx.conn_handle == s_cent) return;
        if (s_tx_handle && event->notify_rx.attr_handle == s_tx_handle) return;
    }
}

void walkie_ble_prepare(void)
{
    bsp_ble_set_extra_svcs(s_svcs);
    bsp_ble_set_scan_uuid128(UUID_WK.value);
    bsp_ble_set_gap_cb(on_gap);
}

esp_err_t walkie_ble_start(int ch)
{
    s_ch = walkie_ch_clamp(ch);
    s_on = true;
    s_sub_n = 0;
    s_cent = 0;
    s_have_want = false;
    s_peer_tx = s_peer_rx = 0;
    if (!bsp_ble_enabled()) {
        if (bsp_ble_set_enabled(true) != ESP_OK) {
            ESP_LOGE(TAG, "BLE enable failed inited?");
            s_on = false;
            return ESP_ERR_INVALID_STATE;
        }
    }
    esp_read_mac(s_own, ESP_MAC_BT);
    set_beacon(true);
    bsp_ble_ensure_advertising();

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
        ESP_LOGW(TAG, "scan rc=%d (wait as peripheral)", rc);
        bsp_ble_refresh_adv();
    }
    return ESP_OK;
}

void walkie_ble_stop(void)
{
    s_on = false;
    if (!bsp_ble_stack_up()) {
        s_cent = 0;
        s_have_want = false;
        s_peer_tx = s_peer_rx = 0;
        s_sub_n = 0;
        if (s_noted) {
            s_noted = false;
        }
        return;
    }
    ble_gap_disc_cancel();
    if (s_cent) {
        ble_gap_terminate(s_cent, BLE_ERR_REM_USER_CONN_TERM);
        s_cent = 0;
    } else if (s_noted) {
        bsp_ble_note_app_conn(-1);
        s_noted = false;
    }
    s_have_want = false;
    s_sub_n = 0;
    set_beacon(false);
}

void walkie_ble_send(const uint8_t *p, size_t n)
{
    if (!s_on || !p || n == 0) return;
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

bool walkie_ble_linked(void)
{
    return s_cent != 0 || s_sub_n > 0;
}
