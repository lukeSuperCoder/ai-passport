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
    src/platform_mock.c
    src/nvs_mock.c
    src/app_services_mock.c
    ${APP_DIR}/app_ui.c
    ${APP_DIR}/ui_pixel.c
    ${APP_DIR}/ui_pixel_math.c
    ${APP_DIR}/app_i18n.c
    ${APP_DIR}/app_logic.c
    ${APP_DIR}/app_prefs.c
    ${APP_DIR}/app_meow_logic.c
    ${APP_DIR}/app_meow_rhythm.c
    ${APP_DIR}/app_meow_run.c
    ${APP_DIR}/app_meow_match.c
    ${APP_DIR}/app_meow.c
    ${APP_DIR}/app_meow_pet.c
    ${APP_DIR}/app_meow_good.c
    ${APP_DIR}/app_meow_souv.c
    ${APP_DIR}/app_meow_haz.c
    ${APP_DIR}/app_meow_ico.c
    ${APP_DIR}/app_meow_stage.c
    ${APP_DIR}/app_meow_set.c
    ${APP_DIR}/fonts/lv_font_cjk_12.c
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
target_compile_definitions(ai_passport_sim PRIVATE APP_DESKTOP_SIM=1)

if(SIM_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ai_passport_sim PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ai_passport_sim PRIVATE -fsanitize=address,undefined)
endif()
