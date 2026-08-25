# Tamagotchi (this firmware)

English | [简体中文](APPS.zh_CN.md)

This `demo/tamagotchi` image is a **standalone pet game**. It boots into the pet. Alerts, Walkie, Weather, and TOTP are not in this image. **Language, Wi-Fi, Bluetooth, clock, screen, sound, and firmware update are set inside the game** — do not flash `main` first. After the first flash, the first boot toasts **hold OK to go back**.

Pins and BSP limits stay in the [README](../README.md) and the [AI Hardware Development Guide](AI_HARDWARE_DEVELOPMENT_GUIDE.md).

## Buttons

`UP`, `DOWN`, and `OK` share the GPIO0 ADC ladder.

| Action | Effect |
| --- | --- |
| Short `UP` / `DOWN` | Bottom tabs: Home / Bag / Game / Dex / Set. After OK, move among that page's actions. In the bag and dex, move among entries in the current category |
| Short `OK` | Open the current tab. Inside: do the highlighted act. Home shortcuts (icons only): Feed / Bath / Heal / Light. The first three cycle usable goods in catalog order (Feed includes drinks, Bath includes sweep). A toast shows `Used rice ball x1` plus Full/Clean/Health/Mood deltas. Light works while the pet sleeps; opening Home at night starts on the lamp. Settings has language, name, Wi-Fi, Bluetooth, clock, screen, sound, update, and Reset game (clears bag, finds, and scores; the pet is gone). Bag uses durable items by category. Game: UP/DOWN flips Catch Fish / Rhythm Master / Kitten Run / Match 3 / Bag Trip, OK starts; Bag Trip packs food then leaves. Minigames and a finished trip share one result page (rewards, one chime, screen on); OK leaves |
| Long `OK` | Leave the tab's inner selection, leave a settings page, or leave the play minigame |
| Long `UP` / `DOWN` | In the bag, switch Food / Meds / Gear; in the dex, switch Pets / Goals / Items / Finds; in Bag Trip, long UP removes one packed copy, long DOWN leaves. On a keyboard (name or Wi-Fi password), hold to keep moving and then jump a row |
| Catch-fish | `UP` = left, `DOWN` = right |
| Rhythm Master | `UP` / `DOWN` tap or hold; long `OK` ends and opens the result page |
| Kitten Run | `UP` = left, `DOWN` = right; the current pet stays at the bottom while the track scrolls down. An item (one of the 16 goods, equal chance) lands on one lane every 3s; catching it plays a pop and shows `name +1` at the top. From 20s, a 30% obstacle roll every 2s, then +30% every 10s until 50s. From 30s, item and obstacle gaps shrink 10% every 10s, freeze at 70% after 60s; from 60s fall speed +10% every 10s, freeze at +40% after 100s. Hit an obstacle or hold `OK` to settle |
| Match 3 | `UP` moves vertically, `DOWN` horizontally; hold jumps to that axis end. OK selects; while selected you can only step to a neighbor and OK swaps. Lines of 3+ vanish in place (neighbors stay), then items above fall down, +10 and +1s each. A failed swap reverts. 3 goods per board; no moves left starts the next with score and leftover time kept. 60s start; time-up settles. Hold OK unsels, or quits if nothing is selected |
| Visit / Battle search | `OK` cancels |
| Egg | A new egg starts the 60s hatch right away. Rename later in Settings. Settings stay available while it hatches |
| After the pet is gone | `OK` on **New egg** starts a new egg; Settings stay available |
| BLE pairing code | `OK` confirm, `DOWN` reject |
| Any key while the panel sleeps | Wake (first click is consumed) |

The header shows the pet name and level, plus local time and battery in the two pills (clock where the mock has coins, battery where it has gems). Gray Wi-Fi / Bluetooth sit between them when that radio is on.

The game is full-screen cream, matching the Ardot 240×320 mock: circular pet with an XP ring, fullness / clean / health / mood bars with numbers, four home shortcuts (Feed / Bath / Heal / Light), and a five-tab bar (Home, Bag, Game, Dex, Set). The stage badge (egg / heart / sprout / bolt / star) is a transparent sticker below the name, upper-left of the pet, with the numeric level under it and current/needed XP under the level. Pets stay original shaded mascots. The bag tab is Food / Meds / Gear, not a coin store. Dex uses the same category pills as the bag: Pets (species you have reached), Goals, Items (goods you currently own), and Finds (souvenirs from Bag Trip). Play lives on the Game tab only.

## Life cycle

One game minute is **60 real seconds**. The egg hatches after a fixed **60 real seconds**, not game minutes.

| Stage | After (game minutes) | Min level | On evolve |
| --- | --- | --- | --- |
| Egg | 60 real seconds | — | Baby: fullness 80 / clean 90 / HP 90 / mood 70, Lv.1 |
| Baby | 40 | 4 | Child: 90 / 85 / 95 / 80, plus fruit and water |
| Child | 120 | 8 | Teen: 95 / 85 / 100 / 85, plus vitamin and soap |
| Teen | 240 | 14 | Adult: 100 / 90 / 100 / 90, plus vitamin and towel |

Adult form is a **random species** (six possible adults, plus two teen forms). Care score weights the odds toward healthier species, but about one in five evolutions can still land on a surprise. Baby and child each have a fixed species; the egg is species 0. Diet also feeds the care score: salad, fruit, and water count as healthy; burgers and cola count as junk. High weight or staying dirty pulls evolution down. Waiting is not enough: age without the level gate leaves the pet in its current stage.

Stats are 0–100. XP comes from feed, bath, heal, play, walk, visit, and battle; each stage caps the level (baby 5 / child 12 / teen 18 / adult 99). While awake, fullness and cleanliness drop on a fixed clock: intervals shrink by stage and a little more every 8 levels, and the drop size grows with stage. Health does not tick down on a timer. Each awake minute rolls a chance from hunger, dirt, poop, and low mood; once sick, further rolls can cut HP. A hunger tick at 0 always hits the stomach and HP. Mood is not on its own clock: each minute it walks toward the average of fullness, clean, and HP, pulled down by sick, poop, or lights left on at night. Care and play give a short bump. Two piles cause worms. The matching medicine restores extra HP; vitamins also help. HP 0 ends the run.

| Item | Main deltas |
| --- | --- |
| Burger | fullness +32, mood +4, dirt +6, weight, poop |
| Rice ball | fullness +22, mood +2, poop |
| Salad | fullness +14, mood +8, HP +4, slight clean |
| Fruit | fullness +12, mood +10, HP +2 |
| Water | fullness +8, HP +2, slight clean |
| Cola | fullness +10, mood +12, slight dirt, weight |
| Matching med | HP +18, +12 more if it matches |
| Vitamin | HP +14, mood +6 |
| Soap / towel | large clean, mood +8 / +10 |
| Trash / tissue | clear poop; tissue also cleans |

Loot has four tiers: common (rice ball, water, tissue, trash bag), uncommon, rare, and prize (deworm, gold shovel). Higher level shifts weight toward rare and prize drops.

Each danger (hunger, mood, HP, sick, poop, night-light) has **three call levels** at **50% / 30% / 10%**. Fullness, mood, and HP use those percentages directly. The device beeps once / twice / three times when a level is newly reached, shows e.g. `Hunger 30%`, and a red frame only while that toast is up. After the beep the panel can sleep; a 30% / 10% call wakes again every 3 minutes to beep, then sleeps.

If the clock is valid (NTP or Date & Time in Settings), the pet sleeps **21:00–08:00**. Care except Light is refused while it sleeps. Leave the lights off at night or mood suffers.

Power-off catch-up is capped at **8 real hours**.

## Care

| Action | What it does |
| --- | --- |
| Feed | Restores fullness from the food (burger +32, rice ball +22; salad / fruit fill less but raise mood); junk meals schedule poop and add weight; spends 1 **durability** |
| Drink | Water +8 fullness and a little clean; cola +10 fullness and +12 mood; drinks do not poop. Water and cola live in the Food category |
| Clean | Clears poop; small mood bump; spends durability on a trash bag or tissue |
| Bath | Clears poop and dirt, and can raise mood even if the floor is already clean; body wash or towel |
| Play | Full-screen Catch Fish: UP left, DOWN right, +30 per fish and +50 for a gold koi. Every 100 points: one random supply, fall +10%, paddle −10%. From 300, a fish has a 30% chance to weave left/right (stays on screen) and a 30% chance to be a koi, then +30% each 100. Speed, paddle, weave, and koi chance freeze at 500. The Game tab also has **Rhythm Master**: two falling lanes, tap or hold `UP`/`DOWN`. Tap Good 15 / Perfect 20, hold Good 20 / Perfect 25. Both games roll one random supply per 100 points; the result page stacks matching items. OK leaves. A third card is **Kitten Run**: three lanes, the current pet pinned at the bottom, UP left / DOWN right. An item appears every 3s (equal chance among all 16 goods); touching it banks that good, plays a pop, and shows `name +1` at the top. From 20s, a 30% obstacle roll every 2s, then +30% every 10s, freezing at 90% after 50s. From 30s, item and obstacle gaps shrink 10% every 10s and freeze at 70% after 60s; from 60s fall speed +10% every 10s and freeze at +40% after 100s. Hitting an obstacle ends the run; the result page only grants goods collected this game. A fourth card is **Match 3**: 6×6 board, three random goods each board, UP vertical / DOWN horizontal, hold to jump. OK picks a cell, then only a neighbor can swap; lines of 3+ vanish in place (neighbors stay), then items above fall down, +10 and +1s each, with one random supply per 100 points. No moves left starts the next; score and leftover time carry. Starts at 60s. A fifth card is **Bag Trip**: pick owned food and how many to pack. Better or more food means a longer trip and more loot than you packed (rice-ball gain is the unit); loot per gain tapers. Packing 6+ gives a chance at a souvenir in the Finds dex. Home shows a bag while they are away; care is refused, but the other minigames still work. Return opens the same result page, chimes once, and wakes the screen |
| Pet | +12 mood if not max |
| Walk | +10 mood if not max; −8 fullness; always finds 1 supply if the bag has room. A **gold shovel** finds an extra drop and spends shovel durability |
| Heal | Clears sick and can restore HP; the matching medicine restores extra HP |
| Light | Toggles the room lamp (works while sleeping) |
| Bed | Lights off and a nap (works while sleeping; daytime nap lasts until the next clock minute) |

## World

Visit and Battle live under **World**. `OK` opens the list; choose Visit or Battle to search for ~8s. Both Passports must pick the same action. Nearby BLE is enough; on the same LAN, UDP on the saved STA works too. Egg, dead, and sleeping pets cannot link. `OK` cancels the search.

Visit: both pets get +10 mood if not already max, and each finds a gift. Battle compares power (stage, HP, mood, hunger, sick, neglected form); a tie uses the pets’ RNG. Winner +12 mood and loot, loser −10, draw unchanged (sometimes a leftover).

## Bag

The third tab is the backpack, in three categories:

| Category | Items |
| --- | --- |
| Food | Burger, rice ball, salad, fruit. Drinks: water, cola |
| Meds | Deworm, stomach, cold med, cough syrup, vitamin |
| Gear | Body wash, trash bag, tissue, gold shovel, towel |

Each kind stacks to 999 and can be used. **Every copy has durability** (the open copy loses durability first; at 0 the stack loses one). Hatch starts with rice balls, fruit, water, body wash, a trash bag, tissue, stomach med, and cold med. Home Feed / Bath / Heal cycle usable goods in catalog order (Feed: burger → rice ball → salad → fruit → water → cola; Bath: soap → trash bag → tissue → towel; Heal: deworm → stomach → cold → cough → vitamin) and skip items that would do nothing right now. Using an item toasts `Used rice ball x1` plus Full/Clean/Health/Mood deltas. You can also use one from the bag. Long-press UP / DOWN to change category. The gold shovel can dig from the bag, and on a walk it finds an extra drop. Walk, Visit, a Battle win, and sometimes Play/Draw add supplies.

A leftover iPhone pairing overlay can still appear; ANCS is not a guest visit.

## Dex

The fourth tab has four categories, with the same controls as the bag (short UP/DOWN among cards, long UP/DOWN to change category, OK toasts a two-line blurb, no name). Each category pill shows lit / total:

| Category | What it shows |
| --- | --- |
| Pets | Egg plus ten forms, unlocked by the current run |
| Goals | Hatched, joyful, grown, full, clean, fit, plus Catch Fish / Rhythm Master at 100 |
| Items | All sixteen goods (lit when you currently own them) |
| Finds | Eight trip souvenirs (shell, ticket, and so on; kept across a new egg) |

## Settings

Last tab. Always available, including on the egg and after the pet is gone.

| Page | What it does |
| --- | --- |
| Language | English / 简体中文 (default), saved to NVS |
| Name | Change the pet name (empty GO uses the default baby name; hold OK back). The name stays after every evolve |
| Wi-Fi | Power, auto-reconnect, scan, join (3-key password), forget |
| Bluetooth | Power, quiet, advertise, forget a bond |
| Date & Time | NTP on/off and server, or set year/month/day/hour/minute |
| Screen | Brightness 10–100%; auto-sleep never / 15 / 30 / 60 / 120 s |
| Sound | Mute; volume 0–100% |
| Update | Current and latest version; Check; Install after confirm. Downloads the **app** image from `latest.json` (`url`, not `*-factory.bin`). Wi-Fi also checks in the background and shows **OK install / DOWN later**. Pet NVS is kept. First USB flash of this partition table: `idf.py flash` (do not erase), or the release `*-factory.bin` at `0x0`. |

State is stored in NVS (`app` / `tama` for the pet; the same `app` namespace for prefs; `ota_skip` remembers a deferred version).

Each demo has its own channel (`ota/channel`, this branch is `demo/tamagotchi`). Devices only fetch `ota/demo/tamagotchi/latest.json` from `main` and refuse a manifest whose `channel` does not match. Tag `demo-tamagotchi-vX.Y.Z` (or run **Release firmware** on this branch) builds this image only and updates that JSON — other demos are left alone. The release has two bins: `FoloToy-AI-Passport-demo-tamagotchi.bin` (OTA app, what Settings → Update installs) and `FoloToy-AI-Passport-demo-tamagotchi-factory.bin` (full flash at `0x0`). Do not release on every commit.

## Idle sleep (device)

This is device light sleep, not the pet’s bedtime. After the auto-sleep interval from Settings (or never, if that was 0):

- backlight off, panel sleep, LVGL tick stopped
- Wi-Fi and Bluetooth radios off; waking a key does **not** reconnect Wi-Fi (open Wi-Fi / Update, or start Visit / Battle)
- GPIO0 wakes the CPU; a 30% / 10% call can also wake to beep

While the menu is idle the UI paints at 1 Hz and the CPU floor drops to 40 MHz. Minigames restore 4 Hz / 80 MHz.

The device does **not** light-sleep while looking for a peer, while a call toast is still playing, while Wi-Fi is scanning/joining, while a firmware check/download is running, or while a BLE pairing code is waiting.
