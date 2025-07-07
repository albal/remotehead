cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "test_main.c" "test_utils.c" "test_http_handlers.c" "test_nvs_utils.c"
    INCLUDE_DIRS "." "../../main"
    REQUIRES unity esp_http_server bt esp_event nvs_flash json freertos log esp_timer esp_netif esp_wifi lwip driver spiffs esp_ringbuf
)
