#!/usr/bin/env python3
"""Convert data-only GNU ARM assembler syntax to host GNU assembler syntax.

No machine instructions are translated. This is a build-time data converter.
Pointer directives are explicit; scalar words retain their original width.
"""
import re
import argparse
import sys

def convert(source, windows=False, pointer_size=4, song=False):
    lines = []
    globals = set()
    song_header = False
    first_header_pointer = False
    for line in source.splitlines():
        # ARM comment delimiter, excluding quoted filenames/strings.
        line = re.split(r'(?<!\\)@(?=(?:[^"\\]*"[^"\\]*")*[^"\\]*$)', line)[0]
        global_match = re.match(r'\s*\.global\s+(\w+)', line)
        if global_match:
            globals.add(global_match[1])
        label = re.match(r'\s*(\w+):', line)
        if label and song:
            song_header = label[1] in globals
            first_header_pointer = song_header
        # mid2agb's symbolic .word operands are sequence/header addresses.
        # Numeric extended-command values remain fixed-width scalars.
        song_pointer = song and pointer_size == 8 and re.match(r'\s*\.word\s+[A-Za-z_]', line)
        if song_pointer:
            if first_header_pointer:
                lines.append('.balign 8')
                first_header_pointer = False
            line = re.sub(r'\.word\b', '.quad', line)
        line = re.sub(r'\.4byte\b|\.word\b', '.long', line)
        line = re.sub(r'\.2byte\b|\.hword\b', '.short', line)
        # RHH tags effect-instrumented functions with a GBA ROM mirror offset.
        # Host pointers are direct; generate_script_metadata.py preserves the
        # effect annotation separately, without encoding it into addresses.
        line = re.sub(r'\s*\+\s*ROM_SIZE\b', '', line)
        match = re.match(r'(\s*)\.align\s+(\d+)(.*)', line)
        if match:
            alignment = 1 << int(match[2])
            if pointer_size == 8 and alignment >= 4:
                alignment = max(alignment, 8)
            line = f'{match[1]}.balign {alignment}{match[3]}'
        if re.match(r'\s*\.(?:type|size|syntax|thumb|thumb_func|arm)\b', line):
            continue
        if windows and re.match(r'\s*\.section\s+script_data', line):
            line = '.section script_data, "dw"'
        elif not windows and pointer_size == 8 and re.match(r'\s*\.section\s+\.rodata(?:\s|$)', line):
            line = '.section .data.rel.ro, "aw"'
        lines.append(line)
    return '\n'.join(lines) + '\n'

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--windows', action='store_true')
    parser.add_argument('--pointer-size', type=int, choices=[4, 8], default=4)
    parser.add_argument('--song', action='store_true')
    args = parser.parse_args()
    sys.stdout.write(convert(sys.stdin.read(), args.windows, args.pointer_size, args.song))
