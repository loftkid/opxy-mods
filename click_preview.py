# renders the op-xy metronome click (model of 8d504ffc, see metronome-dig-notes.md) for stock and soft variants
import math, struct, wave, json, sys

fs = 44100
variants = json.load(open('variants.json'))

def render(v, level=0.8, bpm=120, bars=2):
    taper = 0.2 * level + 0.8 * level * level
    a0 = taper * v['gain'] * 0.5
    decay = 1 / (v['decay_ms'] / 1000 * fs)  # stock multiplier 0.0011338 = 20 ms
    beat = int(fs * 60 / bpm)
    out = [0.0] * (beat * 4 * bars + fs // 4)
    phase, y = 0, 0.0
    for b in range(4 * bars):
        inc = int(v['accent_hz' if b % 4 == 0 else 'beat_hz'] / fs * 2**32)
        a, d = a0, a0 * decay
        for i in range(b * beat, (b + 1) * beat):
            s = a if phase & 0x80000000 else -a
            y = v['lowpass'] * y + s
            a = max(a - d, 0.0)
            phase = (phase + inc) & 0xffffffff
            out[i] = y
    return out

for name, v in variants.items():
    out = render(v)
    peak = max(abs(x) for x in out)
    with wave.open(f'previews/{name}.wav', 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(fs)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1, min(1, x)) * 32767)) for x in out))
    print(f'{name:8} peak {peak:.2f} (>1 clips at this level)')
