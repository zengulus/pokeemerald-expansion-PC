#!/usr/bin/env python3
"""Check daycare timing API against engine steps in a disposable build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix='rhh-daycare-timing-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64',
                                     f'OBJ_DIR={build}', '-j8', 'gameplay-tests'], cwd=ROOT,
                                    stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        for mode, marker in [('daycare-timing', 'daycare timing: egg roll preview, charm and hatch cycle passed'),
                             ('egg-cycle-policy', 'egg cycle policy: preview, live, fallback and RNG passed'),
                             ('egg-cycle-observer', 'egg cycle observer: unmodified engine updates passed'),
                             ('egg-hatch-policy', 'egg hatch policy: preview, party order, denial and RNG passed')]:
            result = subprocess.run([str(build / 'gameplay-tests'), mode], cwd=ROOT, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
            if result.returncode or marker not in result.stdout:
                raise RuntimeError(f'{mode} failed ({result.returncode}):\n{result.stdout}')
    print('linux64: daycare timing API passed; temporary files removed')


if __name__ == '__main__':
    main()
