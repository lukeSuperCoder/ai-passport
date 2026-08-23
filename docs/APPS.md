# On-device apps and settings

English | [简体中文](APPS.zh_CN.md)

This page describes the home apps, Settings pages, button behavior, and values that survive power loss on the current `main` firmware. Pins and BSP limits stay in the [README](../README.md) and the [AI Hardware Development Guide](AI_HARDWARE_DEVELOPMENT_GUIDE.md).

The UI language is **English** or **简体中文**, switched in Settings. Names below match the English UI.

## Buttons

`UP`, `DOWN`, and `OK` share the GPIO0 ADC ladder. Global rules:

| Action | Effect |
| --- | --- |
| Short `UP` / `DOWN` | Move the cursor |
| Short `OK` | Open, toggle, or confirm |
| Long `OK` | Back from a sub-page; lock from Home |
| Any key while asleep | Wake; show the lock screen if Lock is on |
| Short any key on the lock screen | Unlock |

While Walkie is running: hold `UP` to talk, release to stop; short `OK` ends the session. Sleep and lock do not interrupt an active walkie session.

The header shows local time, battery, Bluetooth, and Wi-Fi. Gray = radio on but idle, yellow = connecting or advertising, white = connected or ANCS ready. Hidden means that radio is off.

## Home

Short `OK` opens one of:

1. **Alerts**
2. **Walkie**
3. **Weather**
4. **TOTP**
5. **Settings**

## Phone web input

After Wi-Fi has an IP, the device serves:

| URL | Use |
| --- | --- |
| `http://<ip>:8080/` | Scan page: send text into the current field (password, keyword, city, TOTP secret) |
| `https://<ip>:8443/w` | Walkie mic page (accept the self-signed certificate) |
| `https://<ip>:8443/rtc` | WebRTC walkie (accept the self-signed certificate) |

Do not use ports 80 or 443, and do not use WeChat's in-app browser. Use Safari or Chrome. **QR** on an input page shows the HTTP `:8080` code. One POST is at most 160 characters. After the phone sends, press `OK` on the device to write, or long-press to drop.

---

## Alerts

Needs **Settings → Bluetooth** and iPhone ANCS: pair, then enable Share Notifications. A Mac or second Passport can bond, but only an iPhone pushes ANCS.

Every ANCS record goes into the recent log (16 in RAM, 10 shown). Full-screen popups follow keywords:

- **No keywords:** every notification pops at normal (`N`) priority and plays the Message tone.
- **With keywords:** title / subtitle / body must contain a keyword. ASCII matching is case-insensitive; CJK matches the original bytes.
- `!` (high) plays the Alert tone; `N` plays the Message tone.
- SMS-style codes, when parsed, are shown large in the list.

| Action | Effect |
| --- | --- |
| `UP` / `DOWN` | Browse recent alerts |
| Long `UP` | Delete the focused row |
| Bottom **Settings** + `OK` | Alert settings |
| Overlay `OK` | Dismiss; auto-hide still applies |

### Alert settings

| Item | Meaning |
| --- | --- |
| Back | Return to recent alerts |
| + Preset | `code`, `otp`, `verify`, `验证码`, `校验码`, `动态码`. At most 8 keywords, 24 characters each |
| + Custom | On-device keyboard, or scan the web page |
| Existing keyword | `OK` cycles `N` → `!` → delete |
| Auto-hide | `off` / `5s` / `10s` / `20s`. `off` waits for `OK` |

Defaults: no keywords, auto-hide 10 s. Keywords persist in NVS. Notification bodies do not.

---

## Walkie

Two Passports must use the same mode and channel. Leaving the page stops the session.

| Item | Values | Default | Notes |
| --- | --- | --- | --- |
| Mode | WebRTC / Bluetooth | WebRTC | WebRTC needs LAN Wi-Fi; Bluetooth uses BLE |
| Channel | 1–8 | 1 | Peers on other channels are ignored |
| Start | — | — | Hold `UP` to talk; `OK` to stop |

WebRTC requires a Wi-Fi IP. Bluetooth mode requires Bluetooth on. A phone on the same LAN can join at `https://<ip>:8443/w` or `/rtc` after trusting the self-signed cert. The device is signaling and audio only; there is no cloud relay.

Audio is 8 kHz, 20 ms frames, ADPCM. Mode and channel are stored in NVS.

---

## Weather

Needs Wi-Fi. The main page shows temperature, feels-like, humidity, wind, UV, and a clothing hint. `UP` / `DOWN` step through 4 days starting today. `OK` opens settings. The lock screen can show one cached weather line.

Fetch order: China Weather (`citykey` presets only) → Open-Meteo → wttr.in. A custom name is geocoded with Open-Meteo first.

### Weather settings

| Item | Values | Default |
| --- | --- | --- |
| City | Cycle the preset list | Shanghai |
| Custom city | Keyboard or web, up to 32 characters | — |
| Interval | 15 / 30 / 60 / 180 minutes | 30 |
| Units | °C·km/h or °F·mph | Metric |
| Refresh | Fetch now | — |
| Done | Save and return | — |

Presets: Shanghai, Beijing, Guangzhou, Shenzhen, Hangzhou, Chengdu, Wuhan, Nanjing, Xian, Hong Kong, Taipei, Tokyo, Singapore, London, New York.

Sleep pauses fetches. City, coordinates, interval, and units persist in NVS.

---

## TOTP

Standard TOTP (HMAC-SHA1). Set the clock in **Settings → Date & Time** to roughly after 2023-11, or the page asks to sync time first.

| Field | Rules |
| --- | --- |
| App (issuer) | Up to 24 characters |
| Account (label) | Up to 32 characters |
| Secret | Base32, or a full `otpauth://totp/...` URI; up to 64 characters. The web page is easier than the keyboard |

The URI may include `digits` (6 or 8, default 6) and `period` (15–120 s, default 30). Accounts are grouped by issuer and stored in NVS. Long `UP` deletes after a confirm page.

---

## Settings

| Item | Role |
| --- | --- |
| Language | `OK` toggles English / 简体中文 immediately |
| Wi-Fi | Scan, join, forget, web QR |
| Bluetooth | Power, advertising, bonds, forget |
| Date & Time | NTP or manual clock |
| Screen | Brightness, sleep, lock |
| Sound | Mute, volume, tones |
| Hardware | Read-only system info |
| Log | Ring buffer of ESP logs |

The footer is `Powered By Pax-z`.

### Language

Default English. Only `lang` is written to NVS.

### Wi-Fi

2.4 GHz STA only, up to 16 APs. `*` means a password is required. Credentials are stored only after an IP lease.

| Item | Default | Notes |
| --- | --- | --- |
| Power | ON | Off disconnects and hides the header icon |
| Auto | ON | Rejoin the saved network after boot or drop. About 3 auth failures stop retries |
| QR code | — | Shown once an IP exists; opens `http://<ip>:8080/` |
| AP list | — | Open networks join immediately; others open the password keyboard (saved password is prefilled). `GO` connects, `BK` returns |
| Rescan | — | Scan again |
| Forget | — | Drop the saved SSID/password and stop auto-reconnect |

SSID up to 32 characters, password up to 64. The password page can also use the phone web form.

### Bluetooth

The advertised name looks like `Passport-B4EC`. At most 2 concurrent links and 6 bonds. ANCS is iPhone-only.

| Item | Default | Notes |
| --- | --- | --- |
| Power | ON | Off stops advertising and disconnects |
| Stop adv | ON | Stop advertising once every bonded peer is connected; still advertise when there are no bonds |
| Peer list | — | `*` is connected. Select a row, then Forget sel |
| Advertise | — | Manual discoverability; ignored when both link slots are full |
| Forget sel | — | Remove the highlighted peer |

Pairing shows a 6-digit code. Numeric comparison: `OK` accepts, `DOWN` rejects. After a new bond the state may be **Enable notify** until the iPhone shares notifications; advertising can continue for a second host.

Power, quiet, and peer names persist in NVS. Bond keys are stored by NimBLE.

### Date & Time

The timezone is fixed at **CST-8** (UTC+8).

| Item | Default | Notes |
| --- | --- | --- |
| NTP | OFF | Runs when Wi-Fi is connected. **sync** appears after success |
| Server | `pool.ntp.org` | Cycles `pool.ntp.org`, `time.apple.com`, `ntp.aliyun.com`, `time.windows.com` |
| Year / Month / Day / Hour / Minute | — | Manual dials; year 2024–2038 |
| Save | — | Commit the dials to the system clock |

TOTP and the lock-screen clock use this clock. NTP on/off and the server index persist. A manual time is not stored by itself; without NTP it is lost across power loss.

### Screen

| Item | Values | Default | Notes |
| --- | --- | --- | --- |
| Brightness | 10%–100% in steps of 10 | 50% | Applied immediately |
| Sleep | never / 15s / 30s / 60s / 120s | 30s | Backlight off, panel sleep, Wi-Fi radio off, automatic light sleep while BLE stays up. ANCS can wake the display without bringing Wi-Fi back; a keypress reconnects Wi-Fi if it is enabled. Walkie, notification overlays, and web input reset the idle timer. |
| Lock | ON / OFF | ON | After sleep or long `OK` on Home: date, large clock, weather, battery |
| Stay lit | ON / OFF | OFF | Keep the backlight on while locked |

Any short press unlocks.

### Sound

Mute forces output to 0. Otherwise volume is 0–100 in steps of 10.

| Item | Values | Default |
| --- | --- | --- |
| Mute | ON / OFF | OFF |
| Volume | 0%–100% | 70% |
| Message | off / beep / double / chime | beep |
| Alert | off / triple / alarm | alarm |

Changing a tone plays a preview (unless muted).

### Hardware

Read-only: chip, CPU, flash / partition, RAM / heap / minimum heap / largest block, PSRAM (none on this board), Wi-Fi / Bluetooth MAC, BLE name, LCD, battery, ES8311, CW2017, die temperature, uptime, reset reason, IDF / app / LVGL versions. `UP` / `DOWN` scroll.

### Log

Captures `ESP_LOG*` into 48 lines × 32 characters. `UP` / `DOWN` scroll; the view follows the tail until you scroll away. `OK` clears. Not stored in NVS.

---

## Persistence

App preferences live in NVS namespace `app`. Wi-Fi and Bluetooth flags use their own namespaces. The NVS partition is 24 KB and is shared with the system.

### `app`

| Key | Type | Meaning | Factory default |
| --- | --- | --- | --- |
| `lang` | u8 | 0=English, 1=简体中文 | 0 |
| `bl` | u8 | Backlight 10–100 | 50 |
| `sleep` | u16 | Sleep seconds, 0=never | 30 |
| `lck` | u8 | Lock screen | 1 |
| `lstay` | u8 | Stay lit while locked | 0 |
| `vol` | u8 | Volume 0–100 | 70 |
| `mute` | u8 | Mute | 0 |
| `tmsg` | u8 | Message tone 0–3 | 1 (beep) |
| `talert` | u8 | Alert tone 0 / 4 / 5 | 5 (alarm) |
| `ntp` | u8 | NTP | 0 |
| `ntps` | u8 | NTP server index 0–3 | 0 |
| `hide` | u8 | Alert auto-hide seconds | 10 |
| `wxc` | str | City name | `Shanghai` |
| `wxlat` / `wxlon` | i32 | Latitude/longitude × 10000 | 312304 / 1214737 |
| `wxiv` | u16 | Weather interval minutes | 30 |
| `wxu` | u8 | 1=imperial | 0 |
| `wkch` | u8 | Walkie channel 1–8 | 1 |
| `wkmd` | u8 | 0=WebRTC, 1=Bluetooth | 0 |
| `kwn` + `kw` | u8 + blob | Up to 8 keywords | empty |
| `totpv` / `totpn` / `totp` | u8 / u16 / blob | TOTP accounts, format v2 | empty |

### `bsp_wifi`

| Key | Meaning | Default |
| --- | --- | --- |
| `en` | Radio | ON |
| `auto` | Auto-reconnect | ON |
| `ssid` / `pass` | Last network that got an IP | none |

### `bsp_ble`

| Key | Meaning | Default |
| --- | --- | --- |
| `en` | Radio | ON |
| `quiet` | Stop advertising when every bond is connected | ON |
| `pnames` | Display names for bonded peers | empty |

Erasing NVS (`nvs_flash_erase` or a blank part) restores the table above and forgets Wi-Fi credentials and BLE bonds.
