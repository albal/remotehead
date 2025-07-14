# Real-time Audio Streaming Feature

## Overview

The RemoteHead project now includes real-time audio streaming capabilities that allow you to stream live phone call audio from the ESP32 to a web browser via WebSocket connections. This feature enables monitoring or participating in phone calls directly from a web interface.

## Architecture

The audio streaming feature follows this data flow:

1. **Phone Call Audio Source** → A live phone call on a paired mobile device
2. **Bluetooth HFP Link** → Phone streams call audio to ESP32 (typically mono, 8kHz/16kHz 16-bit PCM)
3. **ESP32 Firmware** → Captures HFP audio and relays it via WebSocket
4. **Web Client** → Receives audio data and plays it using Web Audio API

## Implementation Details

### ESP32 Firmware Changes

#### Audio Data Callback
```c
// Registers callback to receive raw PCM audio from HFP
esp_hf_client_register_data_callback(hfp_audio_data_callback);

// Callback function that receives audio data and broadcasts to WebSocket clients
void hfp_audio_data_callback(const uint8_t *data, uint32_t len);
```

#### WebSocket Server
- **Endpoint**: `/ws`
- **Protocol**: WebSocket over HTTP
- **Data Format**: Binary frames containing raw PCM audio data
- **Broadcast**: Sends audio data to all connected WebSocket clients

### Web Client Implementation

#### Audio Streaming Interface
- **Main Page**: Enhanced control center with audio streaming access
- **Streaming Page**: `/audio_streaming.html` - Dedicated interface for audio streaming
- **WebSocket Client**: Connects to ESP32 WebSocket endpoint
- **Audio Processing**: Web Audio API for low-latency playback

#### Key Features
- Real-time audio visualization
- Volume control
- Connection status monitoring
- Audio buffer management
- Latency monitoring
- Test tone generation

## Usage

### Starting Audio Streaming

1. **Connect Hardware**:
   - Pair ESP32 with phone via Bluetooth
   - Ensure ESP32 is connected to WiFi network
   - Access web interface via ESP32 IP address

2. **Access Streaming Interface**:
   - Navigate to the main control page
   - Click "Launch Audio Streaming" button
   - Or directly access `/audio_streaming.html`

3. **Connect Audio Stream**:
   - Click "Connect Audio" button
   - Allow microphone permissions if prompted
   - WebSocket connection will be established

4. **Monitor Call Audio**:
   - Make or receive a phone call on paired device
   - Audio will automatically stream to web browser
   - Adjust volume and monitor audio visualization

### Configuration

#### Audio Parameters
- **Sample Rate**: 8000 Hz (HFP standard)
- **Channels**: 1 (Mono)
- **Bit Depth**: 16-bit PCM
- **Buffer Size**: Configurable (default: 10 buffers)

#### WebSocket Configuration
- **URL**: `ws://[ESP32_IP]/ws`
- **Protocol**: WebSocket
- **Frame Type**: Binary for audio data
- **Connection**: Persistent during streaming

## Technical Considerations

### Performance Requirements
- **CPU Load**: High - simultaneous Bluetooth and WiFi processing
- **Memory Usage**: Moderate - audio buffering and WebSocket management
- **Radio Coexistence**: Challenging - requires careful WiFi/Bluetooth coordination

### Latency Factors
- **Bluetooth HFP**: ~20-50ms inherent latency
- **WiFi Network**: Variable depending on network conditions
- **Audio Processing**: ~10-20ms for Web Audio API processing
- **Total Latency**: Typically 50-100ms end-to-end

### Quality Considerations
- **Audio Quality**: Limited by HFP codec (8kHz mono)
- **Network Stability**: WiFi jitter affects audio continuity
- **Buffer Management**: Automatic overflow protection
- **Error Handling**: Graceful degradation on connection issues

## Troubleshooting

### Common Issues

1. **No Audio Stream**:
   - Check Bluetooth HFP connection
   - Verify phone call is active
   - Ensure WebSocket connection is established

2. **Audio Dropouts**:
   - Check WiFi signal strength
   - Reduce network congestion
   - Adjust buffer size if needed

3. **High Latency**:
   - Use 5GHz WiFi if available
   - Minimize network hops
   - Check for WiFi interference

4. **Connection Errors**:
   - Verify ESP32 IP address
   - Check firewall settings
   - Ensure WebSocket support in browser

### Debug Information
- **Connection Status**: Displayed in web interface
- **Audio Metrics**: Latency, buffer status, sample rate
- **Console Logs**: Detailed debugging in browser console
- **ESP32 Logs**: Serial output for firmware debugging

## Browser Compatibility

### Supported Browsers
- Chrome/Chromium 66+
- Firefox 60+
- Safari 12+
- Edge 79+

### Required Features
- WebSocket support
- Web Audio API
- ArrayBuffer support
- Modern JavaScript (ES6+)

## Security Considerations

### Network Security
- **Local Network**: Operates within local WiFi network
- **No Encryption**: Audio data transmitted unencrypted over WiFi
- **Access Control**: No authentication required for WebSocket access

### Privacy Considerations
- **Audio Monitoring**: All connected clients receive audio stream
- **Call Privacy**: Consider legal implications of call monitoring
- **Data Retention**: No audio data is stored on ESP32

## Future Enhancements

### Planned Features
- **Audio Encryption**: Encrypt WebSocket audio data
- **Authentication**: Add access control for streaming
- **Codec Support**: Support for additional audio codecs
- **Recording**: Optional audio recording capability
- **Multi-Room**: Stream to multiple devices simultaneously

### Performance Optimizations
- **Adaptive Buffering**: Dynamic buffer size adjustment
- **Compression**: Real-time audio compression
- **Network Optimization**: Improve WiFi coexistence
- **Power Management**: Reduce power consumption during streaming

## Development Notes

### Code Organization
- **Main Firmware**: `main/main.c` - Core audio streaming logic
- **WebSocket Handler**: `websocket_handler()` - WebSocket connection management
- **Audio Callback**: `hfp_audio_data_callback()` - HFP audio processing
- **Web Interface**: `spiffs/audio_streaming.html` - Client-side implementation

### Testing
- **Unit Tests**: `test/main/test_audio_streaming.c` - Core functionality tests
- **Integration Tests**: Manual testing with actual hardware
- **Performance Tests**: Latency and quality measurements

### Build Requirements
- **ESP-IDF**: Version 5.x required for WebSocket support
- **Components**: HTTP Server, WebSocket, HFP Client, WiFi
- **Flash Size**: Minimum 4MB for web interface and firmware

## API Reference

### WebSocket API
```javascript
// Connect to audio stream
const ws = new WebSocket('ws://esp32-ip/ws');
ws.binaryType = 'arraybuffer';

// Handle audio data
ws.onmessage = (event) => {
    const audioData = new Int16Array(event.data);
    // Process audio data...
};
```

### ESP32 API
```c
// Register audio callback
esp_err_t esp_hf_client_register_data_callback(hfp_audio_data_callback);

// Send audio data via WebSocket
esp_err_t httpd_ws_send_frame_async(server, client_fd, &audio_frame);
```

## License

This audio streaming feature is part of the RemoteHead project and follows the same licensing terms as the main project.