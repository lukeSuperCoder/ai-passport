cmake_minimum_required(VERSION 3.20)
project(ai_passport_simulator LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SIM_ENABLE_SANITIZERS "Enable Address/UndefinedBehavior Sanitizers" OFF)

include(FetchContent)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
find_package(Threads REQUIRED)

# Keep this aligned with components/bsp/idf_component.yml.
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(lvgl)
target_link_libraries(lvgl PUBLIC PkgConfig::SDL2)

set(APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../main")
set(BSP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../components/bsp/include")

add_executable(ai_passport_sim
    src/main.c
    src/bsp_mock.c
    src/freertos_mock.c
    src/app_persistence_file.c
    src/clock_service_sim.c
    src/telemetry_sim.c
    ${APP_DIR}/app_ui.c
    ${APP_DIR}/ui_pixel.c
    ${APP_DIR}/ui_pixel_math.c
    ${APP_DIR}/game/game_state.c
    ${APP_DIR}/services/save_service.c
    ${APP_DIR}/demo_display.c
    ${APP_DIR}/demo_button.c
    ${APP_DIR}/demo_audio.c
    ${APP_DIR}/demo_battery.c
)

target_include_directories(ai_passport_sim PRIVATE
    include
    ${APP_DIR}
    ${BSP_INCLUDE_DIR}
)
target_link_libraries(ai_passport_sim PRIVATE
    lvgl::lvgl
    PkgConfig::SDL2
    Threads::Threads
)
target_compile_options(ai_passport_sim PRIVATE -Wall -Wextra -Wpedantic)

if(SIM_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ai_passport_sim PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ai_passport_sim PRIVATE -fsanitize=address,undefined)
endif()
