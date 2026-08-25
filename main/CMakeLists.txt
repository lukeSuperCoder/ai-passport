idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "app_logic.c"
         "app_prefs.c"
         "app_i18n.c"
         "app_time.c"
         "app_tone.c"
         "app_tama_logic.c"
         "app_tama_rhythm.c"
         "app_tama_run.c"
         "app_tama_match.c"
         "app_tama.c"
         "app_tama_pet.c"
         "app_tama_good.c"
         "app_tama_souv.c"
         "app_tama_haz.c"
         "app_tama_ico.c"
         "app_tama_stage.c"
         "app_tama_set.c"
         "app_ui.c"
         "app_tama_link.c"
         "app_ota_logic.c"
         "app_ota.c"
         "fonts/lv_font_cjk_12.c"
    INCLUDE_DIRS "."
    REQUIRES bsp nvs_flash esp_netif lwip
             driver bt esp_event
             app_update esp_app_format esp_http_client mbedtls esp-tls
)
if(DEFINED APP_OTA_CHANNEL AND APP_OTA_CHANNEL)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        "APP_OTA_CHANNEL=\"${APP_OTA_CHANNEL}\"")
endif()
