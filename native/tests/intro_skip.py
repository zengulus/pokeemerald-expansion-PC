#!/usr/bin/env python3
"""Exercise early and late intro skips in fresh native processes."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
GAME = ROOT / 'build/native/linux64/pokeemerald-expansion'

with tempfile.TemporaryDirectory(prefix='rhh-intro-skip-') as folder:
    directory = Path(folder)
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    for frame in (150, 170, 190, 210, 240, 300, 350, 400, 450, 500, 550, 600, 650, 700, 750, 800):
        replay = directory / f'{frame}.input'
        replay.write_text(f'0 0\n{frame} 1\n{frame + 1} 0\n')
        subprocess.run([str(GAME), '--frames', '1100', '--save', str(directory / f'{frame}.sav'),
                        '--controls', str(directory / 'controls.cfg'), '--input', str(replay)],
                       cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                       check=True, timeout=30)
print('Early expansion and Pokémon Emerald intro skips passed')
