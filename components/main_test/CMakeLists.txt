idf_component_register(
    SRCS "test_main.c" "test_utils.c" "test_http_handlers.c" "test_nvs_utils.c"
    INCLUDE_DIRS "."
    REQUIRES unity nvs_flash esp_http_server cjson esp_wifi
)