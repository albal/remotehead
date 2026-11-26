// Browser side of the call-audio bridge.
//
// Talks to the ESP32's /audio WebSocket:
//   - text  {"type":"audio_state","state":"connected","sample_rate":8000|16000,"codec":...}
//   - binary from device: phone audio, Int16 little-endian mono PCM at sample_rate
//   - binary to device:   microphone audio, same format
//   - text to device:     {"cmd":"connect_audio"} / {"cmd":"disconnect_audio"}
//
// Uses ScriptProcessorNode rather than AudioWorklet on purpose: AudioWorklet only exists in
// secure contexts (HTTPS), and the ESP32 serves plain HTTP.

const PLAYBACK_TARGET_MS = 80; // jitter buffer before playback (re)starts
const PLAYBACK_MAX_MS = 300; // beyond this we drop audio to keep latency bounded
const MIC_SEND_MS = 20; // microphone packet size
const PROCESSOR_FRAMES = 1024;

// Linear-interpolation resampler that keeps state across chunks so there are no clicks
// at chunk boundaries.
export class LinearResampler {
  constructor(inRate, outRate) {
    this.step = inRate / outRate;
    this.pos = 0;
    this.prev = 0;
  }

  process(input) {
    const n = input.length;
    if (n === 0) return new Float32Array(0);
    const out = new Float32Array(Math.ceil((n - this.pos) / this.step) + 1);
    let count = 0;
    let pos = this.pos;
    while (pos < n) {
      const i = Math.floor(pos);
      const f = pos - i;
      const a = i === 0 ? this.prev : input[i - 1];
      const b = input[i];
      out[count++] = a + (b - a) * f;
      pos += this.step;
    }
    this.pos = pos - n;
    this.prev = input[n - 1];
    return out.subarray(0, count);
  }
}

export function int16ToFloat32(int16) {
  const out = new Float32Array(int16.length);
  for (let i = 0; i < int16.length; i++) out[i] = int16[i] / 32768;
  return out;
}

export function float32ToInt16(float32) {
  const out = new Int16Array(float32.length);
  for (let i = 0; i < float32.length; i++) {
    out[i] = Math.max(-32768, Math.min(32767, Math.round(float32[i] * 32768)));
  }
  return out;
}

export function microphoneSupported() {
  return !!(typeof navigator !== 'undefined' && navigator.mediaDevices && navigator.mediaDevices.getUserMedia);
}

export class CallAudio {
  constructor({ host, onChange }) {
    this.host = host;
    this.onChange = onChange || (() => {});
    this.ws = null;
    this.ctx = null;
    this.stream = null;
    this.processor = null;
    this.micFilter = null;
    this.micMuted = false;
    this.state = {
      socket: 'closed', // closed | connecting | open
      callAudio: false, // SCO link to phone is up
      sampleRate: 8000,
      codec: 'CVSD',
      mic: 'off', // off | on | denied | unsupported
      error: '',
    };
    this._resetBuffers();
  }

  _set(patch) {
    this.state = { ...this.state, ...patch };
    this.onChange(this.state);
  }

  _resetBuffers() {
    this.playQueue = []; // Float32Array chunks at the AudioContext's rate
    this.playQueued = 0;
    this.playOffset = 0;
    this.playing = false;
    this.micPending = [];
    this.micPendingLen = 0;
    if (this.ctx) {
      this.playResampler = new LinearResampler(this.state.sampleRate, this.ctx.sampleRate);
      this.micResampler = new LinearResampler(this.ctx.sampleRate, this.state.sampleRate);
    }
  }

  async start() {
    const AudioCtx = window.AudioContext || window.webkitAudioContext;
    if (!AudioCtx) {
      this._set({ error: 'This browser does not support Web Audio.' });
      return;
    }
    this.ctx = new AudioCtx();
    await this.ctx.resume();

    this.processor = this.ctx.createScriptProcessor(PROCESSOR_FRAMES, 1, 1);
    this.processor.onaudioprocess = (e) => this._process(e);
    this.processor.connect(this.ctx.destination);
    this._resetBuffers();

    await this._startMic();
    this._openSocket();
  }

  async _startMic() {
    if (!microphoneSupported()) {
      this._set({ mic: 'unsupported' });
      return;
    }
    try {
      this.stream = await navigator.mediaDevices.getUserMedia({
        audio: { channelCount: 1, echoCancellation: true, noiseSuppression: true, autoGainControl: true },
      });
      const source = this.ctx.createMediaStreamSource(this.stream);
      // Low-pass before downsampling to 8/16 kHz so we don't alias.
      this.micFilter = this.ctx.createBiquadFilter();
      this.micFilter.type = 'lowpass';
      this.micFilter.frequency.value = this.state.sampleRate * 0.45;
      source.connect(this.micFilter);
      this.micFilter.connect(this.processor);
      this._set({ mic: 'on' });
    } catch (err) {
      this._set({ mic: 'denied', error: `Microphone unavailable: ${err.message}` });
    }
  }

  _openSocket() {
    this._set({ socket: 'connecting', error: '' });
    const ws = new WebSocket(`ws://${this.host}/audio`);
    ws.binaryType = 'arraybuffer';
    this.ws = ws;
    ws.onopen = () => this._set({ socket: 'open' });
    ws.onclose = () => {
      if (this.ws === ws) {
        this.ws = null;
        this._set({ socket: 'closed', callAudio: false });
      }
    };
    ws.onerror = () => this._set({ error: 'Audio connection to ESP32 failed.' });
    ws.onmessage = (e) => {
      if (typeof e.data === 'string') {
        this._handleText(e.data);
      } else {
        this._handleAudio(e.data);
      }
    };
  }

  _handleText(text) {
    let msg;
    try {
      msg = JSON.parse(text);
    } catch {
      return;
    }
    if (msg.type !== 'audio_state') return;
    const sampleRate = msg.sample_rate || 8000;
    this._set({ callAudio: msg.state === 'connected', sampleRate, codec: msg.codec || 'CVSD' });
    if (this.micFilter) this.micFilter.frequency.value = sampleRate * 0.45;
    this._resetBuffers();
  }

  _handleAudio(buffer) {
    if (!this.ctx || !this.state.callAudio) return;
    const samples = this.playResampler.process(int16ToFloat32(new Int16Array(buffer)));
    this.playQueue.push(samples);
    this.playQueued += samples.length;

    const max = (this.ctx.sampleRate * PLAYBACK_MAX_MS) / 1000;
    const target = (this.ctx.sampleRate * PLAYBACK_TARGET_MS) / 1000;
    while (this.playQueued > max && this.playQueue.length > 1) {
      const dropped = this.playQueue.shift();
      this.playQueued -= dropped.length - this.playOffset;
      this.playOffset = 0;
      if (this.playQueued <= target) break;
    }
  }

  _process(e) {
    const out = e.outputBuffer.getChannelData(0);
    this._fillPlayback(out);
    if (this.stream && !this.micMuted && this.state.callAudio && this.ws && this.ws.readyState === 1) {
      this._captureMic(e.inputBuffer.getChannelData(0));
    }
  }

  _fillPlayback(out) {
    const target = (this.ctx.sampleRate * PLAYBACK_TARGET_MS) / 1000;
    if (!this.playing && this.playQueued >= target) this.playing = true;
    let written = 0;
    while (this.playing && written < out.length && this.playQueue.length > 0) {
      const chunk = this.playQueue[0];
      const n = Math.min(chunk.length - this.playOffset, out.length - written);
      out.set(chunk.subarray(this.playOffset, this.playOffset + n), written);
      written += n;
      this.playOffset += n;
      this.playQueued -= n;
      if (this.playOffset >= chunk.length) {
        this.playQueue.shift();
        this.playOffset = 0;
      }
    }
    if (written < out.length) {
      out.fill(0, written);
      this.playing = false; // underrun: rebuild the jitter buffer
    }
  }

  _captureMic(input) {
    const resampled = this.micResampler.process(input);
    this.micPending.push(resampled);
    this.micPendingLen += resampled.length;
    const packet = (this.state.sampleRate * MIC_SEND_MS) / 1000;
    if (this.micPendingLen < packet) return;

    const joined = new Float32Array(this.micPendingLen);
    let off = 0;
    for (const c of this.micPending) {
      joined.set(c, off);
      off += c.length;
    }
    this.micPending = [];
    this.micPendingLen = 0;
    this.ws.send(float32ToInt16(joined).buffer);
  }

  setMicMuted(muted) {
    this.micMuted = muted;
  }

  sendCommand(cmd) {
    if (this.ws && this.ws.readyState === 1) this.ws.send(JSON.stringify({ cmd }));
  }

  stop() {
    if (this.ws) {
      const ws = this.ws;
      this.ws = null;
      ws.close();
    }
    if (this.stream) this.stream.getTracks().forEach((t) => t.stop());
    if (this.processor) this.processor.disconnect();
    if (this.ctx) this.ctx.close();
    this.stream = null;
    this.processor = null;
    this.micFilter = null;
    this.ctx = null;
    this._set({ socket: 'closed', callAudio: false, mic: 'off' });
  }
}
