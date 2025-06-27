idf_component_register(
    SRCS "test_main.c"
         "test_http_handlers.c" 
         "test_nvs_utils.c"
         "test_utils.c"
    INCLUDE_DIRS "."
    REQUIRES unity esp_wifi esp_http_server cjson nvs_flash
)