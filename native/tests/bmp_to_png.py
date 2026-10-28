#!/usr/bin/env python3
"""Encode the backend's 24-bit BMP test capture as PNG using only Python."""
from pathlib import Path
import struct
import sys
import zlib

def convert(source, destination):
    data = Path(source).read_bytes()
    assert data[:2] == b'BM'
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    assert struct.unpack_from('<H', data, 28)[0] == 24
    assert width > 0 and height > 0
    stride = (width * 3 + 3) & ~3
    rows = []
    for y in reversed(range(height)):
        row = data[offset + y * stride:offset + y * stride + width * 3]
        rows.append(b'\0' + b''.join(row[x:x + 3][::-1] for x in range(0, len(row), 3)))
    def chunk(kind, contents):
        return struct.pack('>I', len(contents)) + kind + contents + struct.pack('>I', zlib.crc32(kind + contents))
    Path(destination).write_bytes(b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b''))

if __name__ == '__main__':
    convert(*sys.argv[1:])
