idf_component_register(
    SRCS "main.c"
         "app_ui.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "app_logic.c"
         "app_prefs.c"
         "app_i18n.c"
         "app_time.c"
         "app_tone.c"
         "app_meow_logic.c"
         "app_meow_rhythm.c"
         "app_meow_run.c"
         "app_meow_match.c"
         "app_meow.c"
         "app_meow_web.c"
         "qrcode.c"
         "app_meow_pet.c"
         "app_meow_good.c"
         "app_meow_souv.c"
         "app_meow_haz.c"
         "app_meow_ico.c"
         "app_meow_stage.c"
         "app_meow_set.c"
         "app_ui.c"
         "app_meow_link.c"
         "app_ota_logic.c"
         "app_ota.c"
         "fonts/lv_font_cjk_12.c"
    INCLUDE_DIRS "."
    REQUIRES bsp nvs_flash esp_netif lwip
             driver bt esp_event
             app_update esp_app_format esp_http_client esp_http_server
             mbedtls esp-tls
)
if(DEFINED APP_OTA_CHANNEL AND APP_OTA_CHANNEL)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        "APP_OTA_CHANNEL=\"${APP_OTA_CHANNEL}\"")
endif()
