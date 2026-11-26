#ifndef AUDIO_BRIDGE_H
#define AUDIO_BRIDGE_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_hf_client_api.h"

/*
 * Bridges HFP (SCO) call audio to/from a web browser over a WebSocket at /audio.
 *
 * Protocol (single browser client at a time):
 *   device -> browser, text:   {"type":"audio_state","state":"connected"|"disconnected",
 *                               "codec":"CVSD"|"mSBC","sample_rate":8000|16000}
 *   device -> browser, binary: phone audio, 16-bit signed little-endian mono PCM at sample_rate
 *   browser -> device, binary: microphone audio, same format
 *   browser -> device, text:   {"cmd":"connect_audio"} / {"cmd":"disconnect_audio"}
 */

void audio_bridge_init(void);
void audio_bridge_set_peer(const esp_bd_addr_t bda);
void audio_bridge_on_audio_state(esp_hf_client_audio_state_t state);

esp_err_t audio_bridge_register_ws(httpd_handle_t server);
void audio_bridge_on_server_stop(void);

/* Use as httpd_config_t.close_fn so the bridge notices the browser disconnecting. */
void audio_bridge_close_fn(httpd_handle_t hd, int sockfd);

#endif // AUDIO_BRIDGE_H
