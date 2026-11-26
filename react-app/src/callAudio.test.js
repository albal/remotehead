import { LinearResampler, int16ToFloat32, float32ToInt16 } from './callAudio';

describe('callAudio helpers', () => {
  test('int16 <-> float32 round trip', () => {
    const input = new Int16Array([0, 16384, -16384, 32767, -32768]);
    const back = float32ToInt16(int16ToFloat32(input));
    expect(Array.from(back)).toEqual(Array.from(input));
  });

  test('float32ToInt16 clips out-of-range samples', () => {
    expect(Array.from(float32ToInt16(new Float32Array([2, -2])))).toEqual([32767, -32768]);
  });

  test('upsampling 8 kHz -> 48 kHz produces 6x samples across chunks', () => {
    const r = new LinearResampler(8000, 48000);
    let total = 0;
    for (let i = 0; i < 10; i++) total += r.process(new Float32Array(80)).length;
    expect(total).toBe(4800);
  });

  test('downsampling 48 kHz -> 16 kHz produces 1/3 samples across odd-sized chunks', () => {
    const r = new LinearResampler(48000, 16000);
    let total = 0;
    for (let i = 0; i < 30; i++) total += r.process(new Float32Array(1000 + (i % 7))).length;
    const inputTotal = Array.from({ length: 30 }, (_, i) => 1000 + (i % 7)).reduce((a, b) => a + b, 0);
    expect(Math.abs(total - inputTotal / 3)).toBeLessThanOrEqual(1);
  });

  test('resampler is continuous at chunk boundaries', () => {
    const r = new LinearResampler(8000, 16000);
    const ramp = Float32Array.from({ length: 16 }, (_, i) => i / 16);
    const out = [...r.process(ramp.subarray(0, 8)), ...r.process(ramp.subarray(8))];
    // The first two outputs sit in the resampler's one-sample startup delay.
    for (let i = 3; i < out.length; i++) {
      expect(out[i] - out[i - 1]).toBeCloseTo(1 / 32, 5);
    }
  });
});
