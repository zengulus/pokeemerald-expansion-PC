#!/usr/bin/env python3
"""Generate bounds for compiled Pokémon menu-icon assets."""
import argparse
import re
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    source = Path('src/data/graphics/pokemon.h').read_text().splitlines()
    lines = ['/* Generated Pokémon icon bounds; do not edit. */']
    for line in source:
        stripped = line.strip()
        if re.match(r'#\s*(if|ifdef|ifndef|elif|else|endif)\b', stripped):
            lines.append(line)
            continue
        match = re.fullmatch(r'const u8 (gMon(?:Egg)?Icon_\w+)\[\] = INCGFX_U8\(.*\);', stripped)
        if match:
            name = match.group(1)
            lines.append(f'if (pixels == {name}) return sizeof({name});')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
