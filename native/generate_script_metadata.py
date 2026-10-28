#!/usr/bin/env python3
"""Retain RHH effect instrumentation as metadata instead of ROM address tags."""
import argparse
from pathlib import Path
import re
import subprocess

def annotated_functions():
    functions = set()
    paths = [Path('data/script_cmd_table.inc'), Path('data/specials.inc')]
    paths.extend(Path('asm/macros').rglob('*.inc'))
    paths.extend(Path('data').rglob('*.inc'))
    for path in paths:
        for line in path.read_text().splitlines():
            line = line.split('@', 1)[0].strip()
            if not re.search(r'\brequests_effects\s*=\s*(?:1|TRUE)\b', line):
                continue
            match = re.match(r'script_cmd_table_entry\s+\w+\s+(\w+)\b', line)
            if not match:
                match = re.match(r'(?:def_special|callnative|gotonative)\s+(\w+)\b', line)
            if match:
                functions.add(match[1])
    return functions

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--nm', required=True)
    parser.add_argument('--windows', action='store_true')
    parser.add_argument('--output', required=True)
    parser.add_argument('objects', nargs='+')
    args = parser.parse_args()
    exported = set()
    output = subprocess.check_output([args.nm, '--defined-only', *args.objects], text=True)
    for line in output.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[1] in ('T', 'W'):
            name = fields[2]
            exported.add(name[1:] if args.windows and name.startswith('_') else name)
    functions = sorted(annotated_functions() & exported)
    lines = ['/* Generated from upstream requests_effects annotations. */', '#include <stdint.h>', '#include <stddef.h>']
    lines.extend(f'extern void {name}(void);' for name in functions)
    lines.append('static const uintptr_t sInstrumented[] = {')
    lines.extend(f'    (uintptr_t){name},' for name in functions)
    lines.extend(['};', 'int NativeScriptIsInstrumented(uintptr_t function)', '{',
                  '    for (size_t i = 0; i < sizeof(sInstrumented) / sizeof(*sInstrumented); ++i)',
                  '        if (sInstrumented[i] == function) return 1;', '    return 0;', '}'])
    Path(args.output).write_text('\n'.join(lines) + '\n')

if __name__ == '__main__':
    main()
