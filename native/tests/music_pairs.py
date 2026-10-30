#!/usr/bin/env python3
"""Check the original DS sequence exports against their paired SoundFonts."""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from annotate_ds_music import SEQUENCES

def var(data, offset):
    value = 0
    while True:
        byte = data[offset]
        offset += 1
        value = (value << 7) | (byte & 127)
        if byte < 128:
            return value, offset

def check(generation, role, start, end):
    root = ROOT / 'assets/music'
    midi = (root / f'gen{generation}/{role}.mid').read_bytes()
    font = (root / ('fonts/gen4-native.sf2' if generation == 4 else f'fonts/gen5-{role}.sf2')).read_bytes()
    offset = font.index(b'phdr')
    length = struct.unpack_from('<I', font, offset + 4)[0]
    presets = {struct.unpack_from('<HH', font, offset + 8 + i + 20)
               for i in range(0, length - 38, 38)}
    position = 14
    events, markers = [], {}
    for track in range(int.from_bytes(midi[10:12], 'big')):
        assert midi[position:position + 4] == b'MTrk'
        length = int.from_bytes(midi[position + 4:position + 8], 'big')
        chunk = midi[position + 8:position + 8 + length]
        position += 8 + length
        at = tick = running = 0
        while at < len(chunk):
            delta, at = var(chunk, at)
            tick += delta
            status = chunk[at]
            if status & 128:
                at += 1
                running = status if status < 240 else 0
            else:
                status = running
            if status == 255:
                kind = chunk[at]
                size, at = var(chunk, at + 1)
                text = chunk[at:at + size]
                at += size
                if kind == 1 and text in (b'Start', b'End'):
                    markers[text] = tick
            elif status in (240, 247):
                size, at = var(chunk, at)
                at += size
            else:
                kind = status & 240
                size = 1 if kind in (192, 208) else 2
                events.append((tick, track, len(events), status, chunk[at:at + size]))
                at += size
    assert markers == {b'Start': start, b'End': end}
    programs = [0] * 16
    for _, _, _, status, data in sorted(events):
        channel, kind = status & 15, status & 240
        if kind == 192:
            programs[channel] = data[0]
        elif kind == 144 and data[1]:
            assert channel != 9, 'DS exports should not use General MIDI percussion mapping'
            assert (programs[channel], 0) in presets, (generation, role, channel, programs[channel])
    print(f'Gen {generation} {role}: native instruments and loop markers matched')

if __name__ == '__main__':
    for generation, entries in SEQUENCES.items():
        for role, _, start, end in entries:
            check(generation, role, start, end)
