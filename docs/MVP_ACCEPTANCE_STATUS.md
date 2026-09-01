# Time Station MVP acceptance status

Updated: 2026-09-01

This file records observed results, not intended behavior. Hardware-dependent
items remain unverified until they are run on a FoloToy AI Passport.

## Current result

| Area | Status | Evidence |
| --- | --- | --- |
| Pure C game rules | PASS | `test_game_state`; includes offline caps, economy, crops, recipes, research, quality, relationships, events, travel, buildings, and the Spring 14 no-deadlock path |
| Save safety | PASS | `test_save_service`; includes v14 round trip, interrupted writes, CRC rejection, A/B fallback, and v13-to-v14 migration defaults |
| Simulator navigation | PASS | 6 scripted UI tests, including 100 repeated top-level traversals, deep pages, event choice, and forest-duration selection |
| Host memory safety | PASS | game and save tests run with AddressSanitizer and UndefinedBehaviorSanitizer |
| ESP-IDF build | NOT RUN | ESP-IDF 5.5.3 / `idf.py` is not installed in the current shell |
| Device tests | NOT RUN | No physical ESP32-C3 board is available in this task |

The complete CTest suite currently contains 10 tests and passes 10/10.

## Implemented playable scope

- Five top-level modules and three-button navigation.
- Four pets, four crops, five recipes, six buildings, six farm plots, and three
  travel goals.
- Reception, farming, cooking, recipe research, quality dishes, 30-minute and
  two-hour forest expeditions, building, travel, selling, partner interaction,
  and rest recovery.
- A 25-second cooking heat-control game with a moving indicator, one attempt per
  dish, deterministic quality bonus, timeout settlement, and timer cleanup.
- Ten-stage Spring story progression, 65 validated event definitions, six
  visitor progress records, event history, and Spring 7/14 milestones.
- Seven-day offline cap, weather/calendar settlement, task completion notices,
  and deterministic replay inputs.
- Explicit v14 serialization in an A/B, CRC-protected save format with legacy
  payload decoding.

## Remaining simulator-capable MVP work

These items are present in the product and technical design but are not yet
implemented as complete playable features:

1. Player cancellation of active timed tasks with deterministic material refund.
2. Day/night loop music, pet/UI sound effects, and their silent-failure path.
3. Read-only asset pack v1, font-subset pipeline, and release asset-size report.
4. Simulator injection for battery failure, audio failure, save corruption, and
   direct development time jumps. Trusted startup time injection already exists.
5. A development diagnostics page showing live heap/DMA blocks, save sequence,
   state size, and resource-pack version.
6. Automated text-overflow/image comparison coverage. Current UI tests validate
   page reachability and memory-safe repeated transitions, not pixel fidelity.

## Required device acceptance

- Run `idf.py build` under ESP-IDF 5.5.3 and record image size, free DRAM, largest
  8-bit block, and largest DMA-capable block.
- Verify the custom 8 MB partition table and A/B save writes on Flash.
- Verify display orientation, RGB order, edges, backlight, and repeated LVGL page
  transitions without visible corruption or continuous heap loss.
- Verify all three button voltages and click/long-click behavior on the ADC ladder.
- Verify playback stability and silent degradation when the codec is unavailable.
- Verify battery readings and the missing-CW2017 fallback.
- Run the complete Spring 14 flow after real power cycles and confirm rewards are
  neither lost nor duplicated.
