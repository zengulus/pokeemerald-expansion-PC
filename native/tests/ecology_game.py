#!/usr/bin/env python3
"""Exercise the public ecology service through a real game save and reload."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', '-j8', 'gameplay-tests'],
               cwd=ROOT, stdout=subprocess.DEVNULL, check=True)
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
with tempfile.TemporaryDirectory(prefix='rhh-ecology-game-') as directory:
    save = Path(directory) / 'game.sav'
    for phase in ('write', 'read'):
        subprocess.run([str(ROOT / 'build/native/linux64/gameplay-tests'), 'location-ecology', str(save), phase],
                       cwd=ROOT, env=env, check=True)
