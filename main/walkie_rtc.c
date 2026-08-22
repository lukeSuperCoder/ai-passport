#include "walkie_rtc.h"

#include "app_i18n.h"
#include "app_web.h"
#include "bsp_ble.h"
#include "bsp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "walkie.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char *TAG = "walkie_rtc";
#define WK_PORT 19900

static const char PAGE[] =
    "<!doctype html><html lang=en><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>Passport Walkie</title>"
    "<style>"
    "body{margin:0;background:#1689E8;font-family:sans-serif;color:#17202A}"
    "main{max-width:480px;margin:24px auto;background:#F4F4EA;"
    "padding:16px;border:4px solid #17202A}"
    "h1{margin:0 0 8px;font-size:22px}"
    "p{margin:0 0 12px;color:#0872C9;font-size:14px}"
    "button{width:100%;padding:22px;font-size:20px;font-weight:700;"
    "background:#FFD928;border:3px solid #17202A}"
    "button.on{background:#E23D28;color:#fff}"
    "</style>"
    "<main><h1>Walkie</h1><p id=st>…</p>"
    "<button id=ptt type=button>PTT</button>"
    "<p><a id=hs></a></p>"
    "<p><a href=/rtc>WebRTC</a></p></main>"
    "<script>"
    "const ST=[7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,"
    "50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,"
    "307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,"
    "1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,"
    "5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,"
    "18500,20350,22385,24623,27086,29794,32767];"
    "const IX=[-1,-1,-1,-1,2,4,6,8];"
    "function sat(v){return v>32767?32767:v<-32768?-32768:v|0}"
    "function encN(s,x){let step=ST[s.i],diff=x-s.p,n=0;"
    "if(diff<0){n=8;diff=-diff}let m=4,d=step;"
    "for(let k=0;k<3;k++){if(diff>=d){n|=m;diff-=d}d>>=1;m>>=1}"
    "let del=((n&7)*2+1)*step>>3;s.p=sat(n&8?s.p-del:s.p+del);"
    "s.i+=IX[n&7];if(s.i<0)s.i=0;if(s.i>88)s.i=88;return n}"
    "function decN(s,n){let step=ST[s.i],diff=step>>3;"
    "if(n&4)diff+=step;if(n&2)diff+=step>>1;if(n&1)diff+=step>>2;"
    "s.p=sat(n&8?s.p-diff:s.p+diff);s.i+=IX[n&7];"
    "if(s.i<0)s.i=0;if(s.i>88)s.i=88;return s.p}"
    "function enc(s,pcm){const o=new Uint8Array(pcm.length/2);"
    "for(let i=0;i<o.length;i++){const lo=encN(s,pcm[i*2]),hi=encN(s,pcm[i*2+1]);"
    "o[i]=lo|(hi<<4)}return o}"
    "function dec(s,b){const o=new Int16Array(b.length*2);"
    "for(let i=0;i<b.length;i++){o[i*2]=decN(s,b[i]&15);o[i*2+1]=decN(s,b[i]>>4)}"
    "return o}"
    "function pack(ch,seq,fl,st,ad){const u=new Uint8Array(10+ad.length);"
    "u[0]=87;u[1]=75;u[2]=1;u[3]=ch;u[4]=seq;u[5]=fl;"
    "u[6]=st.p&255;u[7]=(st.p>>8)&255;u[8]=st.i&255;"
    "u.set(ad,10);return u}"
    "function unpack(u){if(u.length<10||u[0]!==87||u[1]!==75)return null;"
    "return{ch:u[3],fl:u[5],p:((u[6]|(u[7]<<8))<<16)>>16,i:(u[8]<<24)>>24,ad:u.subarray(10)}}"
    "let ws,ac,seq=0,talk=0,ch=1,encS={p:0,i:0},proc,src,micE='';"
    "const ptt=document.getElementById('ptt');"
    "const st=document.getElementById('st');"
    "function play(pcm){if(!ac)return;const b=ac.createBuffer(1,pcm.length,8000);"
    "const d=b.getChannelData(0);for(let i=0;i<pcm.length;i++)d[i]=pcm[i]/32768;"
    "const n=ac.createBufferSource();n.buffer=b;n.connect(ac.destination);n.start()}"
    "function open(){if(ws&&ws.readyState<2)return;"
    "ws=new WebSocket((location.protocol==='https:'?'wss:':'ws:')+'//'+location.host+'/w/ws');"
    "ws.binaryType='arraybuffer';"
    "ws.onmessage=e=>{const u=new Uint8Array(e.data);const f=unpack(u);if(!f||talk)return;"
    "if(f.fl&2)return;const s={p:f.p,i:f.i};play(dec(s,f.ad))};"
    "ws.onclose=()=>setTimeout(open,1500)}"
    "function gum(){const d=navigator.mediaDevices;"
    "return d&&d.getUserMedia&&d.getUserMedia.bind(d)}"
    "function tlsUrl(){return 'https://'+location.hostname+':8443'+location.pathname}"
    "function micHint(){const ua=navigator.userAgent||'';"
    "if(/MicroMessenger|AlipayClient|DingTalk/i.test(ua))"
    "return 'open in Safari/Chrome, not WeChat';"
    "if(location.protocol!=='https:')return 'https not up · tap link';"
    "return 'mic unavailable'}"
    "async function mic(){ac=ac||new AudioContext();if(ac.state==='suspended')await ac.resume();"
    "const g=gum();if(!g)throw new Error(micHint());"
    "const stream=await g({audio:true,video:false});"
    "src=ac.createMediaStreamSource(stream);"
    "const buf=ac.sampleRate>=16000?2048:1024;"
    "proc=ac.createScriptProcessor(buf,1,1);"
    "let acc=new Float32Array(0);"
    "proc.onaudioprocess=ev=>{if(!talk||!ws||ws.readyState!==1)return;"
    "const inn=ev.inputBuffer.getChannelData(0);"
    "const n=new Float32Array(acc.length+inn.length);n.set(acc);n.set(inn,acc.length);acc=n;"
    "const need=Math.round(8000*160/ac.sampleRate)*0+160;"
    "const ratio=ac.sampleRate/8000;"
    "while(acc.length>=ratio*160){const pcm=new Int16Array(160);"
    "for(let i=0;i<160;i++){const x=i*ratio,i0=x|0,a=acc[i0]||0,b=acc[i0+1]||a;"
    "const v=a+(b-a)*(x-i0);pcm[i]=Math.max(-32768,Math.min(32767,v*32767))}"
    "acc=acc.subarray(Math.floor(ratio*160));"
    "const snap={p:encS.p,i:encS.i};const ad=enc(encS,pcm);"
    "ws.send(pack(ch,seq++&255,5,snap,ad))}};"
    "src.connect(proc);proc.connect(ac.destination);}"
    "function down(e){e.preventDefault();talk=1;ptt.classList.add('on');"
    "mic().catch(err=>{micE=err&&err.message||micHint();st.textContent=micE})}"
    "function up(e){e.preventDefault();talk=0;ptt.classList.remove('on')}"
    "ptt.addEventListener('mousedown',down);ptt.addEventListener('mouseup',up);"
    "ptt.addEventListener('mouseleave',up);"
    "ptt.addEventListener('touchstart',down,{passive:false});"
    "ptt.addEventListener('touchend',up);"
    "async function tick(){try{const j=await(await fetch('/w/s')).json();"
    "ch=j.ch||1;document.documentElement.lang=j.lang||'en';"
    "st.textContent=(j.on?'ch '+j.ch+' · '+j.peers+' peer':'start Walkie on device')+"
    "(j.name?(' · '+j.name):'')+(micE?(' · '+micE):'');"
    "if(!gum()&&location.protocol!=='https:'&&j.tls)location.replace(tlsUrl());"
    "if(j.on)open()}"
    "catch(e){st.textContent=micE||'offline'}}"
    "if(!gum()&&location.protocol!=='https:'){"
    "const a=document.getElementById('hs');"
    "if(a){a.href=tlsUrl();a.textContent=tlsUrl()}"
    "micE=micHint();st.textContent=micE}"
    "if(gum()||location.protocol==='https:')open();"
    "tick();setInterval(tick,2000);"
    "</script>";

static int s_sock = -1;
static int s_ch = 1;
static bool s_on;
static httpd_handle_t s_httpd;
static int s_ws_fd = -1;

static uint32_t local_ip(void)
{
    char buf[20];
    if (bsp_wifi_ip(buf, sizeof(buf)) != ESP_OK) return 0;
    return (uint32_t)inet_addr(buf);
}

esp_err_t walkie_rtc_start(int ch)
{
    if (bsp_wifi_state() != BSP_WIFI_CONNECTED) return ESP_ERR_INVALID_STATE;
    s_ch = walkie_ch_clamp(ch);
    if (s_sock >= 0) {
        s_on = true;
        return ESP_OK;
    }
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd < 0) {
        ESP_LOGE(TAG, "udp socket errno=%d heap=%u", errno,
                 (unsigned)esp_get_free_heap_size());
        return ESP_FAIL;
    }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(WK_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "udp bind errno=%d", errno);
        close(fd);
        return ESP_FAIL;
    }
    s_sock = fd;
    s_on = true;
    ESP_LOGI(TAG, "udp :%d ch=%d", WK_PORT, s_ch);
    return ESP_OK;
}

void walkie_rtc_stop(void)
{
    s_on = false;
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
}

void walkie_rtc_send(const uint8_t *p, size_t n)
{
    if (!s_on || s_sock < 0 || !p || n == 0) return;
    struct sockaddr_in to;
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(WK_PORT);
    to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sendto(s_sock, p, n, 0, (struct sockaddr *)&to, sizeof(to));
#if CONFIG_HTTPD_WS_SUPPORT
    if (s_httpd && s_ws_fd >= 0) {
        httpd_ws_frame_t ws = {
            .type = HTTPD_WS_TYPE_BINARY,
            .payload = (uint8_t *)p,
            .len = n,
        };
        if (httpd_ws_send_frame_async(s_httpd, s_ws_fd, &ws) != ESP_OK) {
            s_ws_fd = -1;
        }
    }
#endif
}

void walkie_rtc_poll(void)
{
    if (!s_on || s_sock < 0) return;
    uint8_t buf[WALKIE_FRAME_N + 16];
    struct sockaddr_in from;
    uint32_t self = local_ip();
    for (int i = 0; i < 8; i++) {
        socklen_t sl = sizeof(from);
        int n = recvfrom(s_sock, buf, sizeof(buf), MSG_DONTWAIT,
                         (struct sockaddr *)&from, &sl);
        if (n <= 0) break;
        if (self && from.sin_addr.s_addr == self) continue;
        walkie_rx_bytes(buf, (size_t)n);
    }
}

int walkie_rtc_ws_n(void)
{
    return s_ws_fd >= 0 ? 1 : 0;
}

static esp_err_t send_page(httpd_req_t *req)
{
    if (app_web_redirect_https(req, "/w")) return ESP_OK;
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

static void json_esc(char *o, size_t n, const char *s)
{
    size_t j = 0;
    if (!s) s = "";
    for (; *s && j + 2 < n; s++) {
        if (*s == '"' || *s == '\\') {
            if (j + 3 >= n) break;
            o[j++] = '\\';
        }
        if ((uint8_t)*s < 32) continue;
        o[j++] = *s;
    }
    o[j] = 0;
}

static esp_err_t send_status(httpd_req_t *req)
{
    char name[40], ip[20], json[256];
    json_esc(name, sizeof(name), bsp_ble_name());
    ip[0] = 0;
    bsp_wifi_ip(ip, sizeof(ip));
    int on = walkie_busy() && walkie_mode() == WALKIE_MODE_WEBRTC;
    snprintf(json, sizeof(json),
             "{\"on\":%d,\"ch\":%d,\"peers\":%d,\"name\":\"%s\","
             "\"ip\":\"%s\",\"lang\":\"%s\",\"tls\":%d}",
             on, walkie_channel(), walkie_peer_n(), name, ip, app_lang_html(),
             app_web_https_up() ? 1 : 0);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

#if CONFIG_HTTPD_WS_SUPPORT
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        s_ws_fd = httpd_req_to_sockfd(req);
        s_httpd = req->handle;
        walkie_note_peer("Web");
        return ESP_OK;
    }
    httpd_ws_frame_t pkt = { 0 };
    esp_err_t e = httpd_ws_recv_frame(req, &pkt, 0);
    if (e != ESP_OK) return e;
    if (pkt.len == 0 || pkt.len > WALKIE_FRAME_N + 16) return ESP_OK;
    uint8_t buf[WALKIE_FRAME_N + 16];
    pkt.payload = buf;
    e = httpd_ws_recv_frame(req, &pkt, pkt.len);
    if (e != ESP_OK) return e;
    if (pkt.type == HTTPD_WS_TYPE_BINARY) walkie_rx_bytes(buf, pkt.len);
    return ESP_OK;
}
#endif

static const httpd_uri_t URI_PAGE = {
    .uri = "/w", .method = HTTP_GET, .handler = send_page,
};
static const httpd_uri_t URI_ST = {
    .uri = "/w/s", .method = HTTP_GET, .handler = send_status,
};
#if CONFIG_HTTPD_WS_SUPPORT
static const httpd_uri_t URI_WS = {
    .uri = "/w/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true,
};
#endif

void walkie_web_attach(void *httpd)
{
    if (!httpd) return;
    httpd_register_uri_handler(httpd, &URI_PAGE);
    httpd_register_uri_handler(httpd, &URI_ST);
#if CONFIG_HTTPD_WS_SUPPORT
    httpd_register_uri_handler(httpd, &URI_WS);
#endif
}

void walkie_web_detach(void)
{
    s_httpd = NULL;
    s_ws_fd = -1;
}
