#!/usr/bin/env python3
"""Test ordinary-save photo rewards with independently loaded modules."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
binary = ROOT / 'build/native/linux64/gameplay-tests'
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
with tempfile.TemporaryDirectory(prefix='rhh-simple-photo-') as directory:
    work = Path(directory)
    modules = []
    for name in ('photo_album_probe', 'simple_photo_reward_probe'):
        output = work / (name + '.so')
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', f'native/tests/{name}.c', '-o', str(output)], cwd=ROOT, check=True)
        modules.append(output)
    save = work / 'save.sav'
    def run(path, phase):
        subprocess.run([str(binary), 'photo-reward', str(path), *(str(m) for m in modules), str(phase)],
                       cwd=ROOT, env=env, check=True)
    run(save, 10)
    run(save, 11)
    damaged = bytearray(save.read_bytes()); damaged[120] ^= 1
    older = work / 'older.sav'; older.write_bytes(damaged)
    run(older, 12)
