#!/usr/bin/env python3
"""Prepare matched VGMTrans 1.3 exports with the original SSEQ loop points.

Supply the two directories containing the named MIDI/SF2 collection exports.
The loop ticks were measured at each sequence's backward jump, on every track.
The extra MIDI track contains only standard text markers, leaving notes intact.
"""
import argparse
from pathlib import Path
import shutil
import struct

SEQUENCES = {
    4: [('wild', 'SEQ_BA_POKE', 1920, 10368),
        ('trainer', 'SEQ_BA_TRAIN', 1920, 14208),
        ('gym', 'SEQ_BA_GYM', 1920, 13248),
        ('champion', 'SEQ_BA_CHANP', 1152, 12672)],
    5: [('wild', 'SEQ_BGM_VS_NORAPOKE', 2112, 9792),
        ('trainer', 'SEQ_BGM_VS_TRAINER', 1920, 15360),
        ('gym', 'SEQ_BGM_VS_GYMLEADER', 2304, 12672),
        ('champion', 'SEQ_BGM_VS_CHAMP', 3840, 10752)],
}

def variable(number):
    parts = [number & 127]
    while number > 127:
        number >>= 7
        parts.append(128 | (number & 127))
    return bytes(reversed(parts))

def annotate(data, start, end):
    if data[:8] != b'MThd\0\0\0\x06' or data[8:10] != b'\0\x01':
        raise ValueError('Expected a type 1 MIDI export')
    count = int.from_bytes(data[10:12], 'big')
    track = (variable(start) + b'\xff\x01\x05Start'
             + variable(end - start) + b'\xff\x01\x03End'
             + b'\x00\xff\x2f\x00')
    return (data[:10] + struct.pack('>H', count + 1) + data[12:]
            + b'MTrk' + struct.pack('>I', len(track)) + track)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--diamond', type=Path, required=True)
    parser.add_argument('--black-white', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    for generation, source in ((4, args.diamond), (5, args.black_white)):
        for role, name, start, end in SEQUENCES[generation]:
            midi = args.output / f'gen{generation}/{role}.mid'
            midi.parent.mkdir(parents=True, exist_ok=True)
            midi.write_bytes(annotate((source / f'{name}.mid').read_bytes(), start, end))
            font = args.output / ('fonts/gen4-native.sf2' if generation == 4 else f'fonts/gen5-{role}.sf2')
            font.parent.mkdir(parents=True, exist_ok=True)
            if generation == 4 and font.exists():
                if font.read_bytes() != (source / f'{name}.sf2').read_bytes():
                    raise ValueError('Diamond tracks do not share the expected bank')
            else:
                shutil.copyfile(source / f'{name}.sf2', font)

if __name__ == '__main__':
    main()
