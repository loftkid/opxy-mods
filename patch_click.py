# patches the metronome click constants (8d504ffc, see metronome-dig-notes.md) in a built open-xy os.ldr.
# edits use the sdk profile's "table" form: 8-byte `da0x hi lo 0000` words, checked against stock bytes first.
#   python3 -I patch_click.py <variant> <build>/candidate <out>/candidate
import json, shutil, struct, sys
from functools import reduce
from operator import xor
from pathlib import Path

fs = 44100
# file offset of each constant's 8-byte instruction, with its stock bytes (os 1.2.1, sha 60358afd...)
sites = {
    'accent_hz': (0x13c32e, '01da1a094a240000'),
    'beat_hz':   (0x13c336, '00da6f06b7a80000'),
    'decay_ms':  (0x13c386, '01da2500e5260000'),
    'lowpass':   (0x13c2b4, '04dacc6cbfcc0000'),
}

def value(key, x):
    if key.endswith('_hz'):
        return round(x / fs * 2**32)  # phase increment
    if key == 'decay_ms':
        return round(2**31 / (x / 1000 * fs))  # q31 step per sample, linear to zero
    return round(x * 2**31)  # q31 coefficient

def word(stock, v):
    op = struct.unpack_from('<H', stock)[0]
    return struct.pack('<4H', op, v >> 16, v & 0xffff, 0)

def check_ldr(data):
    off = 0
    while off < len(data):
        raw = data[off:off + 16]
        code, _t, count, _a = struct.unpack('<IIII', raw)
        assert code >> 24 == 0xad and reduce(xor, raw) == 0, f'bad ldr header at {off:#x}'
        off += 16 + (0 if code & 0x100 else count)
        if code & 0x8000:
            assert off == len(data), 'trailing bytes after final'
            return
    raise ValueError('missing final record')

def main(name, src, dst):
    v = json.load(open(Path(__file__).with_name('variants.json')))[name]
    src, dst = Path(src), Path(dst)
    assert not dst.exists(), f'{dst} exists'
    data = bytearray((src / 'os.ldr').read_bytes())
    for key, (off, stock) in sites.items():
        stock = bytes.fromhex(stock)
        assert data[off:off + 8] == stock, f'{key}: bytes at {off:#x} are not stock 1.2.1'
        new = word(stock, value(key, v[key]))
        data[off:off + 8] = new
        print(f'{key:9} {off:#08x} {stock.hex()} -> {new.hex()}')
    check_ldr(bytes(data))
    shutil.copytree(src, dst)
    (dst / 'os.ldr').write_bytes(data)
    changed = sum(a != b for a, b in zip(data, (src / 'os.ldr').read_bytes()))
    print(f'{changed} bytes changed; wrote {dst / "os.ldr"}')

if __name__ == '__main__':
    main(*sys.argv[1:4])
