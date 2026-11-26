#include "ota_update.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_desc.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"

#define TAG "OTA"

#define OTA_BUF_SIZE 4096
#define OTA_RECV_MAX_TIMEOUTS 5
#define OTA_RESTART_DELAY_MS 1500

static volatile bool s_update_in_progress = false;

static esp_err_t send_json(httpd_req_t *req, const char *status, const char *json)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static void restart_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(OTA_RESTART_DELAY_MS));
    ESP_LOGI(TAG, "Restarting into new firmware");
    esp_restart();
}

static esp_err_t do_update(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    if (target == NULL) {
        ESP_LOGE(TAG, "No OTA partition available");
        return send_json(req, "500 Internal Server Error",
                         "{\"error\":\"No spare OTA partition. Flash the current partition table over USB once to enable Wi-Fi updates.\"}");
    }

    size_t total = req->content_len;
    if (total == 0) {
        return send_json(req, "400 Bad Request", "{\"error\":\"Empty request body; send the firmware .bin file.\"}");
    }
    if (total > target->size) {
        ESP_LOGE(TAG, "Image too large: %u bytes, slot is %" PRIu32, (unsigned)total, target->size);
        return send_json(req, "413 Payload Too Large", "{\"error\":\"Firmware image is larger than the OTA partition.\"}");
    }

    ESP_LOGI(TAG, "Receiving %u byte image; running from '%s', writing to '%s' at 0x%08" PRIx32,
             (unsigned)total, running ? running->label : "?", target->label, target->address);

    char *buf = malloc(OTA_BUF_SIZE);
    if (buf == NULL) {
        return send_json(req, "500 Internal Server Error", "{\"error\":\"Out of memory.\"}");
    }

    esp_err_t err;
    esp_ota_handle_t handle = 0;
    bool ota_started = false;
    size_t received = 0;
    int timeouts = 0;
    const char *fail_status = "500 Internal Server Error";
    const char *fail_json = NULL;

    while (received < total) {
        size_t want = total - received;
        if (want > OTA_BUF_SIZE) {
            want = OTA_BUF_SIZE;
        }
        int n = httpd_req_recv(req, buf, want);
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts <= OTA_RECV_MAX_TIMEOUTS) {
            continue;
        }
        if (n <= 0) {
            ESP_LOGE(TAG, "Receive failed after %u bytes (%d)", (unsigned)received, n);
            fail_status = "408 Request Timeout";
            fail_json = "{\"error\":\"Upload interrupted.\"}";
            goto fail;
        }
        timeouts = 0;

        if (!ota_started) {
            // Check the image header before erasing anything.
            const size_t desc_end = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
            if ((size_t)n < desc_end || (uint8_t)buf[0] != ESP_IMAGE_HEADER_MAGIC) {
                fail_status = "400 Bad Request";
                fail_json = "{\"error\":\"Not an ESP32 application image. Upload remotehead.bin, not the combined flash image.\"}";
                goto fail;
            }
            const esp_image_header_t *hdr = (const esp_image_header_t *)buf;
            if (hdr->chip_id != CONFIG_IDF_FIRMWARE_CHIP_ID) {
                fail_status = "400 Bad Request";
                fail_json = "{\"error\":\"Firmware was built for a different chip.\"}";
                goto fail;
            }
            const esp_app_desc_t *new_desc = (const esp_app_desc_t *)
                (buf + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));
            if (new_desc->magic_word != ESP_APP_DESC_MAGIC_WORD) {
                fail_status = "400 Bad Request";
                fail_json = "{\"error\":\"Image has no application description; is this the right file?\"}";
                goto fail;
            }
            ESP_LOGI(TAG, "New firmware: project '%s' version '%s'", new_desc->project_name, new_desc->version);
            if (strncmp(new_desc->project_name, esp_app_get_description()->project_name,
                        sizeof(new_desc->project_name)) != 0) {
                fail_status = "400 Bad Request";
                fail_json = "{\"error\":\"Image is for a different project.\"}";
                goto fail;
            }

            err = esp_ota_begin(target, total, &handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
                fail_json = "{\"error\":\"Could not prepare the OTA partition.\"}";
                goto fail;
            }
            ota_started = true;
        }

        err = esp_ota_write(handle, buf, n);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            fail_json = "{\"error\":\"Writing firmware to flash failed.\"}";
            goto fail;
        }
        received += n;
    }

    free(buf);
    buf = NULL;

    err = esp_ota_end(handle);
    ota_started = false;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        return send_json(req, "400 Bad Request",
                         err == ESP_ERR_OTA_VALIDATE_FAILED
                             ? "{\"error\":\"Firmware image failed verification (corrupt or truncated).\"}"
                             : "{\"error\":\"Finishing the update failed.\"}");
    }

    err = esp_ota_set_boot_partition(target);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        return send_json(req, "500 Internal Server Error", "{\"error\":\"Could not select the new firmware for boot.\"}");
    }

    ESP_LOGI(TAG, "Update written (%u bytes). Restarting shortly.", (unsigned)received);
    send_json(req, "200 OK", "{\"message\":\"Firmware updated. Device is restarting.\"}");
    xTaskCreate(restart_task, "ota_restart", 2048, NULL, 5, NULL);
    return ESP_OK;

fail:
    if (ota_started) {
        esp_ota_abort(handle);
    }
    free(buf);
    send_json(req, fail_status, fail_json ? fail_json : "{\"error\":\"Update failed.\"}");
    // Returning ESP_FAIL closes the socket, discarding any unread body.
    return ESP_FAIL;
}

static esp_err_t update_post_handler(httpd_req_t *req)
{
    if (s_update_in_progress) {
        return send_json(req, "409 Conflict", "{\"error\":\"An update is already in progress.\"}");
    }
    s_update_in_progress = true;
    esp_err_t ret = do_update(req);
    s_update_in_progress = false;
    return ret;
}

static const httpd_uri_t update_uri = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = update_post_handler,
    .user_ctx = NULL,
};

esp_err_t ota_update_register(httpd_handle_t server)
{
    return httpd_register_uri_handler(server, &update_uri);
}

void ota_update_mark_app_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (running && esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
            ESP_LOGI(TAG, "New firmware confirmed; rollback cancelled");
        } else {
            ESP_LOGE(TAG, "Failed to confirm new firmware");
        }
    }
}

const char *ota_update_running_version(void)
{
    return esp_app_get_description()->version;
}
