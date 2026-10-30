#!/usr/bin/env python3
"""Embed the example's readable GLSL without an external runtime asset."""
import json
from pathlib import Path
root = Path(__file__).resolve().parent
for relative in ('mods/battle_environment.frag',):
    source = root / relative
    source.with_name(source.stem + '_shader.h').write_text(
        f'/* Generated from {source.name} by native/generate_shader.py. */\n'
        + ''.join(json.dumps(line + '\n') + '\n' for line in source.read_text().splitlines()))
