#include "app_rtc.h"

#include "app_web.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "walkie.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "app_rtc";

#define RTC_MAX 4
#define RTC_BUF 4096

/* 设备只做信令。浏览器用 RTCPeerConnection 互传 Opus,C3 装不下 DTLS-SRTP。
 * getUserMedia 只要安全源:http://IP:8080 上 mediaDevices 是 undefined,
 * 麦克风页跳 https://IP:8443,不占 443。 */
static const char PAGE[] =
    "<!doctype html><html lang=en><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>Passport WebRTC</title>"
    "<style>"
    "body{margin:0;background:#1689E8;font-family:sans-serif;color:#17202A}"
    "main{max-width:480px;margin:24px auto;background:#F4F4EA;"
    "padding:16px;border:4px solid #17202A}"
    "h1{margin:0 0 8px;font-size:22px}"
    "p{margin:0 0 12px;color:#0872C9;font-size:14px}"
    "button{width:100%;padding:22px;font-size:20px;font-weight:700;"
    "background:#FFD928;border:3px solid #17202A}"
    "button.on{background:#E23D28;color:#fff}"
    "a{color:#0872C9}"
    "</style>"
    "<main><h1>WebRTC</h1><p id=st>hold PTT · same Wi-Fi</p>"
    "<button id=ptt type=button>PTT</button>"
    "<p><a id=hs></a></p>"
    "<p><a href=/>Home</a> · <a href=/w>Walkie</a></p></main>"
    "<script>"
    "const ICE=[{urls:'stun:stun.l.google.com:19302'}];"
    "let id=0,ws,stream,pcs={},iceq={};"
    "const st=document.getElementById('st');"
    "const ptt=document.getElementById('ptt');"
    "function send(o){if(ws&&ws.readyState===1)ws.send(JSON.stringify(o))}"
    "function npeer(){return Object.keys(pcs).length}"
    "function show(){st.textContent='you '+id+' · '+npeer()+' peer'}"
    "function gum(){const d=navigator.mediaDevices;"
    "return d&&d.getUserMedia&&d.getUserMedia.bind(d)}"
    "function tlsUrl(){return 'https://'+location.hostname+':8443'+location.pathname}"
    "function micHint(){const ua=navigator.userAgent||'';"
    "if(/MicroMessenger|AlipayClient|DingTalk/i.test(ua))"
    "return 'open in Safari/Chrome, not WeChat';"
    "if(location.protocol!=='https:')return 'https not up · tap link';"
    "return 'mic unavailable'}"
    "async function mic(){"
    "if(stream)return stream;"
    "const g=gum();if(!g)throw new Error(micHint());"
    "stream=await g("
    "{audio:{echoCancellation:true,noiseSuppression:true},video:false});"
    "stream.getAudioTracks().forEach(t=>t.enabled=false);return stream}"
    "function talk(on){"
    "ptt.classList.toggle('on',!!on);"
    "if(stream)stream.getAudioTracks().forEach(t=>t.enabled=!!on)}"
    "function bye(pid){"
    "if(pcs[pid]){try{pcs[pid].close()}catch(e){}delete pcs[pid]}"
    "delete iceq[pid];const a=document.getElementById('r'+pid);"
    "if(a)a.remove();show()}"
    "function flush(pid){"
    "const pc=pcs[pid],q=iceq[pid];if(!pc||!q||!pc.remoteDescription)return;"
    "q.splice(0).forEach(c=>pc.addIceCandidate(c).catch(()=>{}))}"
    "async function mkpc(pid,offer){"
    "let pc=pcs[pid];"
    "if(!pc){"
    "pc=new RTCPeerConnection({iceServers:ICE});pcs[pid]=pc;"
    "pc.onicecandidate=e=>{if(e.candidate&&e.candidate.candidate)"
    "send({t:'c',to:pid,f:id,s:e.candidate.candidate,m:e.candidate.sdpMid||'0',"
    "x:e.candidate.sdpMLineIndex|0})};"
    "pc.onconnectionstatechange=()=>{const s=pc.connectionState;"
    "if(s==='failed'||s==='disconnected'||s==='closed')bye(pid)};"
    "pc.ontrack=e=>{let a=document.getElementById('r'+pid);"
    "if(!a){a=document.createElement('audio');a.id='r'+pid;a.autoplay=true;"
    "a.setAttribute('playsinline','');document.body.appendChild(a)}"
    "a.srcObject=e.streams[0];a.play().catch(()=>{})};"
    "(await mic()).getTracks().forEach(t=>pc.addTrack(t,stream));show()}"
    "if(offer){await pc.setRemoteDescription({type:'offer',sdp:offer});flush(pid);"
    "const a=await pc.createAnswer();await pc.setLocalDescription(a);"
    "send({t:'a',to:pid,f:id,s:a.sdp})}"
    "else{const o=await pc.createOffer();await pc.setLocalDescription(o);"
    "send({t:'o',to:pid,f:id,s:o.sdp})}return pc}"
    "function link(pid){if(!pid||pid===id||pcs[pid])return;"
    "if(id<pid)mkpc(pid,0).catch(e=>st.textContent=String(e))}"
    "async function onmsg(e){"
    "const m=JSON.parse(e.data);"
    "if(m.t==='hi'){id=m.id;show();(m.p||[]).forEach(link)}"
    "else if(m.t==='j'){show();link(m.id)}"
    "else if(m.t==='bye')bye(m.id);"
    "else if(m.t==='o')mkpc(m.f,m.s).catch(e=>st.textContent=String(e));"
    "else if(m.t==='a'){const pc=pcs[m.f];if(pc){"
    "pc.setRemoteDescription({type:'answer',sdp:m.s}).then(()=>flush(m.f))}}"
    "else if(m.t==='c'){const c={candidate:m.s,sdpMid:m.m,sdpMLineIndex:m.x};"
    "const pc=pcs[m.f];if(pc&&pc.remoteDescription)pc.addIceCandidate(c).catch(()=>{});"
    "else (iceq[m.f]=iceq[m.f]||[]).push(c)}}"
    "function openws(){"
    "if(ws&&ws.readyState<2)return;"
    "ws=new WebSocket((location.protocol==='https:'?'wss:':'ws:')+'//'+location.host+'/rtc/ws');"
    "ws.onmessage=onmsg;ws.onclose=()=>{ws=null;setTimeout(()=>{if(stream)openws()},1200)}}"
    "async function join(){await mic();openws()}"
    "function down(e){e.preventDefault();join().then(()=>talk(1))"
    ".catch(err=>st.textContent=err&&err.message||'mic blocked')}"
    "function up(e){e.preventDefault();talk(0)}"
    "ptt.addEventListener('mousedown',down);ptt.addEventListener('mouseup',up);"
    "ptt.addEventListener('mouseleave',up);"
    "ptt.addEventListener('touchstart',down,{passive:false});"
    "ptt.addEventListener('touchend',up);"
    "(async()=>{"
    "async function goW(){try{const j=await(await fetch('/w/s')).json();"
    "if(j.on)location.replace('/w')}catch(e){}}"
    "goW();setInterval(goW,1500);"
    "if(gum()||location.protocol==='https:'){openws();return}"
    "const ua=navigator.userAgent||'';"
    "if(/MicroMessenger|AlipayClient|DingTalk/i.test(ua)){"
    "st.textContent='open in Safari/Chrome, not WeChat';return}"
    "const a=document.getElementById('hs');"
    "if(a){a.href=tlsUrl();a.textContent=tlsUrl()}"
    "let tls=0;try{tls=(await(await fetch('/s')).json()).tls}catch(e){}"
    "if(tls)location.replace(tlsUrl());"
    "else st.textContent=micHint()"
    "})();"
    "</script>";

typedef struct {
    int fd;
    uint8_t id;
    httpd_handle_t hd;
} rtc_peer_t;

static rtc_peer_t s_peer[RTC_MAX];
static uint8_t s_next_id = 1;
static uint8_t *s_buf;
static SemaphoreHandle_t s_mu;
static StaticSemaphore_t s_mu_buf;

static void mu_take(void)
{
    if (s_mu) xSemaphoreTake(s_mu, portMAX_DELAY);
}

static void mu_give(void)
{
    if (s_mu) xSemaphoreGive(s_mu);
}

static int json_int(const char *s, int n, const char *key)
{
    char pat[12];
    int pl = snprintf(pat, sizeof(pat), "\"%s\":", key);
    if (pl <= 0 || pl >= (int)sizeof(pat)) return 0;
    for (int i = 0; i + pl < n; i++) {
        if (memcmp(s + i, pat, (size_t)pl) != 0) continue;
        const char *p = s + i + pl;
        const char *end = s + n;
        while (p < end && *p == ' ') p++;
        if (p >= end || *p < '0' || *p > '9') return 0;
        int v = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            v = v * 10 + (*p - '0');
            p++;
        }
        return v;
    }
    return 0;
}

static void ws_send(httpd_handle_t hd, int fd, const char *s, size_t n)
{
    if (!hd || fd < 0 || !s || n == 0) return;
    httpd_ws_frame_t ws = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)s,
        .len = n,
    };
    if (httpd_ws_send_frame_async(hd, fd, &ws) != ESP_OK) {
        ESP_LOGW(TAG, "send fd=%d fail", fd);
    }
}

static void broadcast(const char *s, size_t n, int skip)
{
    int fds[RTC_MAX];
    httpd_handle_t hds[RTC_MAX];
    int nfd = 0;
    mu_take();
    for (int i = 0; i < RTC_MAX; i++) {
        if (s_peer[i].fd >= 0 && s_peer[i].fd != skip) {
            fds[nfd] = s_peer[i].fd;
            hds[nfd++] = s_peer[i].hd;
        }
    }
    mu_give();
    for (int i = 0; i < nfd; i++) ws_send(hds[i], fds[i], s, n);
}

static uint8_t alloc_id(void)
{
    for (int n = 0; n < 255; n++) {
        if (s_next_id == 0) s_next_id = 1;
        uint8_t id = s_next_id++;
        bool used = false;
        for (int i = 0; i < RTC_MAX; i++) {
            if (s_peer[i].id == id) { used = true; break; }
        }
        if (!used) return id;
    }
    return 0;
}

static void peer_gone(void *ctx)
{
    rtc_peer_t *p = ctx;
    uint8_t id = 0;
    int fd = -1;
    mu_take();
    if (p) {
        id = p->id;
        fd = p->fd;
        p->fd = -1;
        p->id = 0;
        p->hd = NULL;
    }
    mu_give();
    if (id) {
        char msg[32];
        int n = snprintf(msg, sizeof(msg), "{\"t\":\"bye\",\"id\":%u}", id);
        if (n > 0) broadcast(msg, (size_t)n, fd);
        ESP_LOGI(TAG, "peer %u leave", id);
    }
}

static rtc_peer_t *add_peer(int fd, httpd_handle_t hd)
{
    mu_take();
    rtc_peer_t *slot = NULL;
    for (int i = 0; i < RTC_MAX; i++) {
        if (s_peer[i].fd < 0) { slot = &s_peer[i]; break; }
    }
    uint8_t id = slot ? alloc_id() : 0;
    if (slot && id) {
        slot->fd = fd;
        slot->id = id;
        slot->hd = hd;
    } else {
        slot = NULL;
    }
    mu_give();
    return slot;
}

static void send_hello(httpd_req_t *req, rtc_peer_t *me)
{
    uint8_t ids[RTC_MAX];
    int n = 0;
    mu_take();
    for (int i = 0; i < RTC_MAX; i++) {
        if (s_peer[i].fd >= 0 && s_peer[i].id && s_peer[i].id != me->id) {
            ids[n++] = s_peer[i].id;
        }
    }
    mu_give();
    char msg[96];
    size_t used = (size_t)snprintf(msg, sizeof(msg), "{\"t\":\"hi\",\"id\":%u,\"p\":[", me->id);
    for (int i = 0; i < n && used + 8 < sizeof(msg); i++) {
        used += (size_t)snprintf(msg + used, sizeof(msg) - used, "%s%u", i ? "," : "", ids[i]);
    }
    if (used + 3 < sizeof(msg)) {
        memcpy(msg + used, "]}", 3);
        used += 2;
    }
    httpd_ws_frame_t ws = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)msg,
        .len = used,
    };
    httpd_ws_send_frame(req, &ws);
}

static esp_err_t send_page(httpd_req_t *req)
{
    if (app_web_redirect_https(req, "/rtc")) return ESP_OK;
    /* /rtc 是浏览器互相对讲。设备对讲已经开时,改去 /w 才能跟喇叭/麦克风通话。 */
    if (walkie_busy() && walkie_mode() == WALKIE_MODE_WEBRTC) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/w");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        int fd = httpd_req_to_sockfd(req);
        rtc_peer_t *me = add_peer(fd, req->handle);
        if (!me) {
            ESP_LOGW(TAG, "room full");
            return ESP_FAIL;
        }
        req->sess_ctx = me;
        req->free_ctx = peer_gone;
        send_hello(req, me);
        char join[32];
        int n = snprintf(join, sizeof(join), "{\"t\":\"j\",\"id\":%u}", me->id);
        if (n > 0) broadcast(join, (size_t)n, fd);
        ESP_LOGI(TAG, "peer %u join fd=%d", me->id, fd);
        return ESP_OK;
    }

    httpd_ws_frame_t pkt = { 0 };
    esp_err_t e = httpd_ws_recv_frame(req, &pkt, 0);
    if (e != ESP_OK) return e;
    if (pkt.type == HTTPD_WS_TYPE_CLOSE) return ESP_OK;
    if (!s_buf || pkt.len == 0 || pkt.len >= RTC_BUF) return ESP_OK;
    pkt.payload = s_buf;
    e = httpd_ws_recv_frame(req, &pkt, RTC_BUF - 1);
    if (e != ESP_OK) return e;
    if (pkt.type != HTTPD_WS_TYPE_TEXT) return ESP_OK;
    s_buf[pkt.len] = 0;
    int to = json_int((const char *)s_buf, (int)pkt.len, "to");
    if (to <= 0) return ESP_OK;
    int fd = -1;
    httpd_handle_t hd = NULL;
    mu_take();
    for (int i = 0; i < RTC_MAX; i++) {
        if (s_peer[i].id == (uint8_t)to) {
            fd = s_peer[i].fd;
            hd = s_peer[i].hd;
            break;
        }
    }
    mu_give();
    if (fd >= 0) ws_send(hd, fd, (const char *)s_buf, pkt.len);
    return ESP_OK;
}

static const httpd_uri_t URI_PAGE = {
    .uri = "/rtc", .method = HTTP_GET, .handler = send_page,
};
#if CONFIG_HTTPD_WS_SUPPORT
static const httpd_uri_t URI_WS = {
    .uri = "/rtc/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true,
};
#endif

void app_rtc_attach(void *httpd)
{
    if (!httpd) return;
    if (!s_mu) s_mu = xSemaphoreCreateMutexStatic(&s_mu_buf);
    if (!s_buf) {
        for (int i = 0; i < RTC_MAX; i++) {
            s_peer[i].fd = -1;
            s_peer[i].id = 0;
            s_peer[i].hd = NULL;
        }
        s_buf = malloc(RTC_BUF);
        if (!s_buf) {
            ESP_LOGE(TAG, "buf alloc fail");
            return;
        }
    }
    httpd_register_uri_handler(httpd, &URI_PAGE);
#if CONFIG_HTTPD_WS_SUPPORT
    httpd_register_uri_handler(httpd, &URI_WS);
#endif
}

void app_rtc_detach(void)
{
    for (int i = 0; i < RTC_MAX; i++) {
        s_peer[i].fd = -1;
        s_peer[i].id = 0;
        s_peer[i].hd = NULL;
    }
    free(s_buf);
    s_buf = NULL;
}
