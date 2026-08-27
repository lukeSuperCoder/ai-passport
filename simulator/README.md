# Meow desktop simulator

The simulator runs the complete single-player Meow pet game on macOS/Linux
through SDL2. Hardware-facing BSP calls are replaced by deterministic host
mocks. Pet care, inventory, all five mini-games, trips, evolution,
achievements, collections, settings, and save persistence are available.

Networking is deliberately outside this build: Wi-Fi, BLE visits/battles,
the phone web form, and firmware OTA are disabled.

## Prerequisites (macOS)

```bash
xcode-select --install
brew install cmake ninja sdl2 pkg-config
```

## Build and run

```bash
cmake --fresh -S simulator -B simulator/build -G Ninja
cmake --build simulator/build
./simulator/build/ai_passport_sim
```

The first configure downloads the pinned LVGL `v9.5.0` source into the build
directory. Use Up/Down to navigate and Enter to select. Holding any key for at
least 700 ms emits the matching long-press action. Key-down/key-up events are
also forwarded, so the real-time mini-games retain their device controls.

Game and preference state is saved to `meow-simulator-save.bin` in the
directory where the simulator is launched. Delete that file to simulate a
factory-reset installation.

The mocks provide a stable 82%/3970 mV battery, the host clock, ADC values
matching the three-button resistor ladder, and silent tone playback. They test
application behavior, not electrical characteristics or audio quality.

To diagnose host memory errors:

```bash
cmake -S simulator -B simulator/build -G Ninja -DSIM_ENABLE_SANITIZERS=ON
cmake --build simulator/build
```
