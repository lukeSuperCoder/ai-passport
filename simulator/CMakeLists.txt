cmake_minimum_required(VERSION 3.20)
project(ai_passport_simulator LANGUAGES C CXX)

include(CTest)

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
    ${APP_DIR}/app_i18n.c
    ${APP_DIR}/assets/station_scene.c
    ${APP_DIR}/fonts/app_font_zh_14.c
    ${APP_DIR}/fonts/app_font_zh_20.c
    ${APP_DIR}/ui_pixel.c
    ${APP_DIR}/ui_pixel_math.c
    ${APP_DIR}/game/game_state.c
    ${APP_DIR}/game/game_content.c
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

if(BUILD_TESTING)
    add_executable(test_game_state
        ../tests/test_game_state.c
        ${APP_DIR}/game/game_state.c
        ${APP_DIR}/game/game_content.c
    )
    target_include_directories(test_game_state PRIVATE ${APP_DIR})
    target_compile_options(test_game_state PRIVATE -Wall -Wextra -Werror)
    add_test(NAME game_state COMMAND test_game_state)

    add_executable(test_save_service
        ../tests/test_save_service.c
        ${APP_DIR}/game/game_state.c
        ${APP_DIR}/game/game_content.c
        ${APP_DIR}/services/save_service.c
    )
    target_include_directories(test_save_service PRIVATE ${APP_DIR})
    target_compile_options(test_save_service PRIVATE -Wall -Wextra -Werror)
    add_test(NAME save_service COMMAND test_save_service)

    add_executable(test_clock_service_sim
        ../tests/test_clock_service_sim.c
        src/clock_service_sim.c
    )
    target_include_directories(test_clock_service_sim PRIVATE ${APP_DIR})
    target_compile_options(test_clock_service_sim PRIVATE -Wall -Wextra -Werror)
    add_test(NAME clock_service_sim COMMAND test_clock_service_sim)

    add_executable(test_ui_pixel_math
        ../tests/test_ui_pixel_math.c
        ${APP_DIR}/ui_pixel_math.c
    )
    target_include_directories(test_ui_pixel_math PRIVATE ${APP_DIR})
    target_compile_options(test_ui_pixel_math PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ui_pixel_math COMMAND test_ui_pixel_math)

    add_executable(test_i18n
        ../tests/test_i18n.c
        ${APP_DIR}/app_i18n.c
        ${APP_DIR}/game/game_content.c
    )
    target_include_directories(test_i18n PRIVATE ${APP_DIR})
    target_compile_options(test_i18n PRIVATE -Wall -Wextra -Werror)
    add_test(NAME i18n COMMAND test_i18n)

    add_test(NAME simulator_navigation COMMAND ai_passport_sim)
    set_tests_properties(simulator_navigation PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;TIME_STATION_SCRIPT=DOLDDOLDDDOLDDDDOL"
        PASS_REGULAR_EXPRESSION "page_schedule.*page_farm.*page_travel.*page_backpack.*Scripted simulator run completed"
        TIMEOUT 10
    )
    add_test(NAME simulator_deep_navigation COMMAND ai_passport_sim)
    set_tests_properties(simulator_deep_navigation PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;TIME_STATION_SCRIPT=DODDDODLLDDOODOLLDDDDODDODDDOLL"
        PASS_REGULAR_EXPRESSION "page_kitchen.*page_farm_detail.*page_buildings.*Scripted simulator run completed"
        TIMEOUT 10
    )
    add_test(NAME simulator_navigation_stress COMMAND ai_passport_sim)
    set_tests_properties(simulator_navigation_stress PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;TIME_STATION_SCRIPT=DOLDDOLDDDOLDDDDOL;TIME_STATION_SCRIPT_REPEAT=100"
        PASS_REGULAR_EXPRESSION "Scripted simulator run completed"
        TIMEOUT 30
    )
    add_test(NAME simulator_backpack_navigation COMMAND ai_passport_sim)
    set_tests_properties(simulator_backpack_navigation PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;TIME_STATION_SCRIPT=DDDDOOLDODOLDDOLDOLDODOLL"
        PASS_REGULAR_EXPRESSION "page_items.*page_partners.*page_tasks.*page_album.*page_settings.*Scripted simulator run completed"
        TIMEOUT 10
    )
    add_test(NAME simulator_event_choice COMMAND ai_passport_sim)
    set_tests_properties(simulator_event_choice PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;TIME_STATION_SCRIPT=DOODOL"
        PASS_REGULAR_EXPRESSION "page_event.*page_schedule.*Scripted simulator run completed"
        TIMEOUT 10
    )

    add_test(NAME simulator_forest_navigation COMMAND ai_passport_sim)
    set_tests_properties(simulator_forest_navigation PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;TIME_STATION_SCRIPT=DODDODL"
        PASS_REGULAR_EXPRESSION "page_forest.*page_schedule.*Scripted simulator run completed"
        TIMEOUT 10
    )

    add_test(NAME simulator_up_navigation COMMAND ai_passport_sim)
    set_tests_properties(simulator_up_navigation PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;TIME_STATION_SCRIPT=UO"
        PASS_REGULAR_EXPRESSION "page_backpack.*Scripted simulator run completed"
        TIMEOUT 10
    )

    add_test(NAME simulator_schedule_up_navigation COMMAND ai_passport_sim)
    set_tests_properties(simulator_schedule_up_navigation PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;TIME_STATION_SCRIPT=DOUO"
        PASS_REGULAR_EXPRESSION "page_schedule.*page_kitchen.*Scripted simulator run completed"
        TIMEOUT 10
    )

    add_test(NAME simulator_language_switch COMMAND ai_passport_sim)
    set_tests_properties(simulator_language_switch PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;TIME_STATION_SCRIPT=DDDDODDDDDODDDOO"
        PASS_REGULAR_EXPRESSION "page_settings.*Scripted simulator run completed"
        TIMEOUT 10
    )

    if(SIM_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        foreach(test_target test_game_state test_save_service
                            test_clock_service_sim test_ui_pixel_math)
            target_compile_options(${test_target} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${test_target} PRIVATE
                -fsanitize=address,undefined)
        endforeach()
    endif()
endif()
