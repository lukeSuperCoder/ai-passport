# Desktop simulator

The simulator runs the real LVGL menu and demo pages on macOS/Linux through
SDL2. Hardware-facing BSP calls are replaced by deterministic host mocks.

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
directory. Use Up/Down to navigate, Enter to select, and hold Enter for at
least 700 ms to return to the menu.

The mocks currently provide a stable 82%/3970 mV battery, ADC values matching
the three-button resistor ladder, and timed in-memory audio input/output. They
test application behavior, not electrical characteristics or audio quality.

To diagnose host memory errors:

```bash
cmake -S simulator -B simulator/build -G Ninja -DSIM_ENABLE_SANITIZERS=ON
cmake --build simulator/build
```
