# Desktop simulator

The simulator runs the real LVGL application on macOS/Linux through SDL2.
The current branch starts the Time Station MVP vertical slice; hardware-facing
BSP calls are replaced by deterministic host mocks.

## Prerequisites (macOS)

```bash
xcode-select --install
brew install cmake ninja sdl2 pkg-config
```

## Build and run

```bash
cmake -S simulator -B simulator/build -G Ninja
cmake --build simulator/build
./simulator/build/ai_passport_sim
```

The first configure downloads the pinned LVGL `v9.5.0` source into the build
directory. Use Up/Down to select Station or Schedule, Enter to open or claim,
and hold Enter for at least 700 ms to return to the station.

The first vertical slice intentionally starts with a deterministic six-hour
reception report. Open Schedule and claim the 60-coin reward to exercise the
state reducer and page lifecycle.

## Playable MVP loop

The simulator now exposes the complete top-level navigation:

- **Inn** shows real time, spring day, weather, time-of-day colors, traveler
  dialogue, pending reports, and story-event indicators. The bottom menu hides
  after five idle seconds; press a direction key to reveal it.
- **Plan** claims offline rewards, resolves story choices, sends Amai to the
  forest, and opens the five-recipe kitchen. During cooking, press OK again to
  spend a companion action and halve the remaining time.
- **Farm** opens each plot and selects wheat, carrot, strawberry, or herb. Four
  plots are initially available; repairing the sink unlocks plots five and six.
- **Trip** sends Amai and Atuan to Mistpine Forest after Spring 8 and signpost
  repair. Their relationship changes the rare-material result.
- **Bag** contains items and dish sales, partner talk, six buildings, the ten
  main quests and event history, album progress, and persistent settings.

A normal play loop is: claim the welcome report, plant crops, advance the
injected clock, claim the harvest, cook a dish, assist the cooking task, advance
time again, claim and sell the dish, then spend the proceeds on buildings.
Use a later `TIME_STATION_NOW` value on each restart to advance timed work.

The simulator uses timestamp `22600` by default. Inject a later trusted time to
test offline settlement without waiting:

```bash
TIME_STATION_NOW=44200 ./simulator/build/ai_passport_sim
```

Delete `simulator/build/time_station.save` to create a fresh simulator save.

For repeatable headless smoke tests, set `SDL_VIDEODRIVER=dummy` and provide a
`TIME_STATION_SCRIPT`. The characters are `U`/`D` (direction), `O` (OK), and
`L` (long OK). This example opens and returns from all five top-level modules:

```bash
SDL_VIDEODRIVER=dummy TIME_STATION_SCRIPT=DOLDDOLDDDOLDDDDOL \
  ./simulator/build/ai_passport_sim
```

The CTest suite also traverses Farm Detail, Kitchen recipes, Backpack, and
Buildings. It is therefore the preferred regression command after UI changes.

## Host logic tests

All host logic and scripted UI navigation tests can be run together:

```bash
cmake -S simulator -B simulator/build -G Ninja
cmake --build simulator/build
ctest --test-dir simulator/build --output-on-failure
```

Individual compiler commands are retained below for minimal environments.

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_game_state.c main/game/game_state.c \
  -o /tmp/test_game_state
/tmp/test_game_state

cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_save_service.c main/game/game_state.c main/services/save_service.c \
  -o /tmp/test_save_service
/tmp/test_save_service

cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_clock_service_sim.c simulator/src/clock_service_sim.c \
  -o /tmp/test_clock_service_sim
/tmp/test_clock_service_sim
```

The mocks currently provide a stable 82%/3970 mV battery, ADC values matching
the three-button resistor ladder, and timed in-memory audio input/output. The
Time Station entry point initializes playback-only audio, so capture calls
return `ESP_ERR_NOT_SUPPORTED`, matching the device BSP contract. They
test application behavior, not electrical characteristics or audio quality.

To diagnose host memory errors:

```bash
cmake -S simulator -B simulator/build -G Ninja -DSIM_ENABLE_SANITIZERS=ON
cmake --build simulator/build
```
