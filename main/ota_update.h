#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

// Over-the-air firmware update support.
//
// POST /update with the raw application image (build/remotehead.bin) as the
// request body. The image is written to the inactive OTA slot, verified, set
// as the boot partition and the device restarts into it.
//
// With CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE the new image boots in a
// "pending verify" state; ota_update_mark_app_valid() confirms it. If the new
// firmware resets before that happens, the bootloader rolls back to the
// previous image.

// Register the /update handler (must be called before any "/*" catch-all).
esp_err_t ota_update_register(httpd_handle_t server);

// Confirm the running image is good so the bootloader will not roll it back.
// Safe to call on every boot; does nothing if the image is already confirmed.
void ota_update_mark_app_valid(void);

// Firmware version string of the running image (from the app description).
const char *ota_update_running_version(void);
