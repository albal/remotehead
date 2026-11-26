/*
 * HFP call audio <-> browser bridge.
 *
 * Phone audio arrives from the Bluetooth stack as decoded 16-bit PCM through the HCI
 * data path (CVSD: 8 kHz, mSBC: 16 kHz). It is buffered and streamed to the browser over
 * a WebSocket. Microphone audio from the browser comes back over the same socket and is
 * handed to the stack when it asks for outgoing SCO data.
 *
 * Outgoing packets are paced by incoming ones: every byte of phone audio received earns
 * one byte of "credit" to send, so we transmit at exactly the SCO link's rate. When the
 * browser has not supplied enough microphone audio we send silence instead.
 */
#include "audio_bridge.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cJSON.h"

#define TAG "AUDIO_BRIDGE"

#if CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI && CONFIG_HTTPD_WS_SUPPORT

#define SPK_RING_SIZE        16384  // ~500 ms at 16 kHz, phone -> browser
#define MIC_RING_SIZE        8192   // ~250 ms at 16 kHz, browser -> phone
#define MIC_PREBUFFER_MS     60     // jitter buffer before mic audio starts flowing
#define TX_CHUNK_SIZE        1280   // max bytes per WebSocket binary frame
#define TX_INTERVAL_MS       20
#define MAX_RX_FRAME         8192
#define OUT_CREDIT_MAX       2048

typedef struct {
    uint8_t *buf;
    size_t size;
    size_t head;   // write position
    size_t tail;   // read position
    size_t count;  // bytes stored
    portMUX_TYPE lock;
} pcm_ring_t;

static pcm_ring_t s_spk_ring = { .lock = portMUX_INITIALIZER_UNLOCKED };
static pcm_ring_t s_mic_ring = { .lock = portMUX_INITIALIZER_UNLOCKED };

static portMUX_TYPE s_credit_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_out_credit = 0;
static bool s_mic_primed = false;

static httpd_handle_t s_server = NULL;
static volatile int s_ws_fd = -1;
static volatile bool s_state_dirty = false;
static volatile bool s_audio_connected = false;
static volatile uint32_t s_sample_rate = 8000;
static esp_bd_addr_t s_peer_bda;
static bool s_have_peer = false;

// --- Ring buffer helpers (overwrite oldest data when full) ---

static bool ring_alloc(pcm_ring_t *r, size_t size)
{
    r->buf = malloc(size);
    r->size = size;
    r->head = r->tail = r->count = 0;
    return r->buf != NULL;
}

static void ring_clear(pcm_ring_t *r)
{
    portENTER_CRITICAL(&r->lock);
    r->head = r->tail = r->count = 0;
    portEXIT_CRITICAL(&r->lock);
}

static size_t ring_count(pcm_ring_t *r)
{
    portENTER_CRITICAL(&r->lock);
    size_t c = r->count;
    portEXIT_CRITICAL(&r->lock);
    return c;
}

static void ring_write(pcm_ring_t *r, const uint8_t *data, size_t len)
{
    if (!r->buf || len == 0) {
        return;
    }
    if (len > r->size) {
        data += len - r->size;
        len = r->size;
    }
    portENTER_CRITICAL(&r->lock);
    size_t free_space = r->size - r->count;
    if (len > free_space) {
        size_t drop = len - free_space;
        r->tail = (r->tail + drop) % r->size;
        r->count -= drop;
    }
    size_t first = r->size - r->head;
    if (first > len) {
        first = len;
    }
    memcpy(r->buf + r->head, data, first);
    memcpy(r->buf, data + first, len - first);
    r->head = (r->head + len) % r->size;
    r->count += len;
    portEXIT_CRITICAL(&r->lock);
}

static size_t ring_read(pcm_ring_t *r, uint8_t *out, size_t max_len)
{
    if (!r->buf) {
        return 0;
    }
    portENTER_CRITICAL(&r->lock);
    size_t len = r->count < max_len ? r->count : max_len;
    len &= ~(size_t)1; // keep whole 16-bit samples
    size_t first = r->size - r->tail;
    if (first > len) {
        first = len;
    }
    memcpy(out, r->buf + r->tail, first);
    memcpy(out + first, r->buf, len - first);
    r->tail = (r->tail + len) % r->size;
    r->count -= len;
    portEXIT_CRITICAL(&r->lock);
    return len;
}

// --- Bluetooth HCI audio callbacks ---

// Phone -> us. Runs in the Bluetooth stack task: must not block.
static void hf_incoming_data_cb(const uint8_t *buf, uint32_t len)
{
    if (s_ws_fd >= 0) {
        ring_write(&s_spk_ring, buf, len);
    }

    portENTER_CRITICAL(&s_credit_lock);
    s_out_credit += len;
    if (s_out_credit > OUT_CREDIT_MAX) {
        s_out_credit = OUT_CREDIT_MAX;
    }
    portEXIT_CRITICAL(&s_credit_lock);

    esp_hf_client_outgoing_data_ready();
}

// Us -> phone. The stack keeps calling this until we return something other than len.
static uint32_t hf_outgoing_data_cb(uint8_t *buf, uint32_t len)
{
    bool allowed = false;
    portENTER_CRITICAL(&s_credit_lock);
    if (s_out_credit >= len) {
        s_out_credit -= len;
        allowed = true;
    }
    portEXIT_CRITICAL(&s_credit_lock);
    if (!allowed) {
        return 0;
    }

    size_t got = 0;
    if (!s_mic_primed) {
        size_t prebuffer = (s_sample_rate * 2 * MIC_PREBUFFER_MS) / 1000;
        if (ring_count(&s_mic_ring) >= prebuffer) {
            s_mic_primed = true;
        }
    }
    if (s_mic_primed) {
        got = ring_read(&s_mic_ring, buf, len);
        if (got < len) {
            s_mic_primed = false; // underrun: rebuild the jitter buffer
        }
    }
    if (got < len) {
        memset(buf + got, 0, len - got);
    }
    return len;
}

// --- WebSocket ---

static esp_err_t ws_send(int fd, httpd_ws_type_t type, const uint8_t *data, size_t len)
{
    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = type,
        .payload = (uint8_t *)data,
        .len = len,
    };
    return httpd_ws_send_data(s_server, fd, &frame);
}

static void send_state(int fd)
{
    char msg[128];
    bool connected = s_audio_connected;
    snprintf(msg, sizeof(msg),
             "{\"type\":\"audio_state\",\"state\":\"%s\",\"codec\":\"%s\",\"sample_rate\":%lu}",
             connected ? "connected" : "disconnected",
             s_sample_rate == 16000 ? "mSBC" : "CVSD",
             (unsigned long)s_sample_rate);
    ws_send(fd, HTTPD_WS_TYPE_TEXT, (const uint8_t *)msg, strlen(msg));
}

static void drop_client(int fd)
{
    if (s_ws_fd == fd) {
        s_ws_fd = -1;
    }
    if (s_server) {
        httpd_sess_trigger_close(s_server, fd);
    }
}

// All WebSocket sends happen here so text and binary frames never interleave.
static void audio_ws_tx_task(void *arg)
{
    static uint8_t chunk[TX_CHUNK_SIZE];
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(TX_INTERVAL_MS));

        int fd = s_ws_fd;
        if (fd < 0 || s_server == NULL) {
            continue;
        }
        if (httpd_ws_get_fd_info(s_server, fd) != HTTPD_WS_CLIENT_WEBSOCKET) {
            continue;
        }

        if (s_state_dirty) {
            s_state_dirty = false;
            send_state(fd);
        }

        size_t n;
        while ((n = ring_read(&s_spk_ring, chunk, sizeof(chunk))) > 0) {
            if (ws_send(fd, HTTPD_WS_TYPE_BINARY, chunk, n) != ESP_OK) {
                ESP_LOGW(TAG, "WebSocket send failed, dropping client fd=%d", fd);
                drop_client(fd);
                break;
            }
        }
    }
}

static void handle_text_command(const char *text)
{
    cJSON *root = cJSON_Parse(text);
    if (!root) {
        return;
    }
    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    if (cJSON_IsString(cmd) && cmd->valuestring) {
        if (strcmp(cmd->valuestring, "connect_audio") == 0) {
            if (s_have_peer) {
                ESP_LOGI(TAG, "Browser requested audio transfer to headset");
                esp_hf_client_connect_audio(s_peer_bda);
            }
        } else if (strcmp(cmd->valuestring, "disconnect_audio") == 0) {
            if (s_have_peer) {
                ESP_LOGI(TAG, "Browser requested audio transfer back to phone");
                esp_hf_client_disconnect_audio(s_peer_bda);
            }
        } else if (strcmp(cmd->valuestring, "get_state") == 0) {
            s_state_dirty = true;
        }
    }
    cJSON_Delete(root);
}

static esp_err_t ws_audio_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        // Handshake complete. Only one browser gets audio at a time; the newest wins.
        int fd = httpd_req_to_sockfd(req);
        int old = s_ws_fd;
        s_ws_fd = fd;
        if (old >= 0 && old != fd) {
            ESP_LOGI(TAG, "New audio client fd=%d replaces fd=%d", fd, old);
            httpd_sess_trigger_close(req->handle, old);
        }
        ring_clear(&s_spk_ring);
        ring_clear(&s_mic_ring);
        s_mic_primed = false;
        s_state_dirty = true;
        ESP_LOGI(TAG, "Audio WebSocket client connected, fd=%d", fd);
        return ESP_OK;
    }

    httpd_ws_frame_t frame = { 0 };
    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    if (frame.len == 0) {
        return ESP_OK;
    }
    if (frame.len > MAX_RX_FRAME) {
        ESP_LOGW(TAG, "WebSocket frame too large (%u bytes)", (unsigned)frame.len);
        return ESP_FAIL;
    }

    uint8_t *buf = malloc(frame.len + 1);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    frame.payload = buf;
    ret = httpd_ws_recv_frame(req, &frame, frame.len);
    if (ret == ESP_OK) {
        if (frame.type == HTTPD_WS_TYPE_BINARY) {
            if (s_audio_connected && httpd_req_to_sockfd(req) == s_ws_fd) {
                ring_write(&s_mic_ring, buf, frame.len & ~(size_t)1);
            }
        } else if (frame.type == HTTPD_WS_TYPE_TEXT) {
            buf[frame.len] = '\0';
            handle_text_command((const char *)buf);
        }
    }
    free(buf);
    return ret;
}

static const httpd_uri_t ws_audio_uri = {
    .uri = "/audio",
    .method = HTTP_GET,
    .handler = ws_audio_handler,
    .user_ctx = NULL,
    .is_websocket = true,
};

// --- Public API ---

void audio_bridge_init(void)
{
    if (!ring_alloc(&s_spk_ring, SPK_RING_SIZE) || !ring_alloc(&s_mic_ring, MIC_RING_SIZE)) {
        ESP_LOGE(TAG, "Failed to allocate audio buffers; browser audio disabled");
        return;
    }
    esp_err_t err = esp_hf_client_register_data_callback(hf_incoming_data_cb, hf_outgoing_data_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register HFP data callbacks: %s", esp_err_to_name(err));
        return;
    }
    xTaskCreatePinnedToCore(audio_ws_tx_task, "audio_ws_tx", 3072, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "Browser audio bridge ready (WebSocket /audio)");
}

void audio_bridge_set_peer(const esp_bd_addr_t bda)
{
    memcpy(s_peer_bda, bda, sizeof(esp_bd_addr_t));
    s_have_peer = true;
}

void audio_bridge_on_audio_state(esp_hf_client_audio_state_t state)
{
    switch (state) {
        case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED:
            s_sample_rate = 8000;
            s_audio_connected = true;
            break;
        case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC:
            s_sample_rate = 16000;
            s_audio_connected = true;
            break;
        case ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED:
            s_audio_connected = false;
            break;
        default:
            return; // CONNECTING: nothing to do yet
    }
    ring_clear(&s_spk_ring);
    ring_clear(&s_mic_ring);
    s_mic_primed = false;
    portENTER_CRITICAL(&s_credit_lock);
    s_out_credit = 0;
    portEXIT_CRITICAL(&s_credit_lock);
    s_state_dirty = true;
    ESP_LOGI(TAG, "Call audio %s (%lu Hz)", s_audio_connected ? "connected" : "disconnected",
             (unsigned long)s_sample_rate);
}

esp_err_t audio_bridge_register_ws(httpd_handle_t server)
{
    s_server = server;
    s_ws_fd = -1;
    return httpd_register_uri_handler(server, &ws_audio_uri);
}

void audio_bridge_on_server_stop(void)
{
    s_ws_fd = -1;
    s_server = NULL;
}

void audio_bridge_close_fn(httpd_handle_t hd, int sockfd)
{
    if (sockfd == s_ws_fd) {
        ESP_LOGI(TAG, "Audio WebSocket client disconnected, fd=%d", sockfd);
        s_ws_fd = -1;
    }
    close(sockfd);
}

#else // !(CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI && CONFIG_HTTPD_WS_SUPPORT)

void audio_bridge_init(void)
{
    ESP_LOGW(TAG, "Browser audio disabled: needs CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI and CONFIG_HTTPD_WS_SUPPORT");
}
void audio_bridge_set_peer(const esp_bd_addr_t bda) { (void)bda; }
void audio_bridge_on_audio_state(esp_hf_client_audio_state_t state) { (void)state; }
esp_err_t audio_bridge_register_ws(httpd_handle_t server) { (void)server; return ESP_ERR_NOT_SUPPORTED; }
void audio_bridge_on_server_stop(void) {}
void audio_bridge_close_fn(httpd_handle_t hd, int sockfd) { (void)hd; close(sockfd); }

#endif
