"""Independent tiny PNG fixtures using Python's zlib, never LodePNG."""
from pathlib import Path
import struct
import zlib

root = Path(__file__).resolve().parent

def chunk(name, payload):
    return struct.pack('>I', len(payload)) + name + payload + struct.pack('>I', zlib.crc32(name + payload))

def png(name, channels, depth=8, pixels=None, extras=b'', interlace=0):
    # Two horizontal pixels on each of two rows, including whitespace-like bytes.
    rgb = bytes([10, 32, 255, 0, 128, 13, 1, 2, 3, 250, 100, 50])
    data = pixels
    if data is None:
        data = rgb if channels == 2 else b''.join(rgb[i:i+3] + b'\xff' for i in range(0, 12, 3))
    stride = len(data) // 2
    rows = b'\0' + data[:stride] + b'\0' + data[stride:]
    if interlace:
        pixel_width = 3 if channels == 2 else 4
        rows = b''
        for x0, y0, dx, dy in [(0,0,8,8),(4,0,8,8),(0,4,4,8),(2,0,4,4),(0,2,2,4),(1,0,2,2),(0,1,1,2)]:
            if x0 >= 2 or y0 >= 2: continue
            for y in range(y0,2,dy):
                rows += b'\0' + b''.join(data[(y*2+x)*pixel_width:(y*2+x+1)*pixel_width] for x in range(x0,2,dx))
    header = struct.pack('>IIBBBBB', 2, 2, depth, channels, 0, 0, interlace)
    result = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + extras + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')
    (root / name).write_bytes(result)

png('rgb8.png', 2)
png('rgba8.png', 6)
png('transparent.png', 6, pixels=bytes([10, 32, 255, 0, 0, 128, 13, 255, 1, 2, 3, 255, 250, 100, 50, 255]))
png('gray8.png', 0, pixels=bytes([1, 2, 3, 4]))
png('rgb16.png', 2, depth=16, pixels=bytes(range(24)))
png('animated.png', 6, extras=chunk(b'acTL', struct.pack('>II', 1, 0)))
png('inflated-too-large.png', 2, pixels=b'\0' * (1024 * 1024))
png('oversized-dimensions.png', 2)
b = bytearray((root / 'oversized-dimensions.png').read_bytes())
b[16:20] = struct.pack('>I', 4097)
b[29:33] = struct.pack('>I', zlib.crc32(b[12:29]))
(root / 'oversized-dimensions.png').write_bytes(b)

png('rgb8-interlaced.png', 2, interlace=1)
png('rgba8-interlaced.png', 6, interlace=1)
png('transparent-key.png', 2, extras=chunk(b'tRNS', struct.pack('>HHH',10,32,255)))
png('ignored-ancillary.png', 2, extras=chunk(b'tEXt', b'note\0not pixel evidence'))
b=bytearray((root/'ignored-ancillary.png').read_bytes()); b[41]^=1
(root/'bad-ancillary-crc.png').write_bytes(b)
