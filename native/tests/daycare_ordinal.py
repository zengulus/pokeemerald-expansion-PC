#!/usr/bin/env python3
"""Test daycare ordinals and save compatibility in a disposable build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with tempfile.TemporaryDirectory(prefix='rhh-daycare-ordinal-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', f'OBJ_DIR={build}', '-j8', 'gameplay-tests'],
                                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        pending = str(work / 'pending.sav')
        ordinal = str(work / 'ordinal.sav')
        for args, marker in [(['daycare-ordinal', ordinal], 'daycare ordinal: commit, rollback, exhaustion and older reader passed'),
                             (['daycare-ordinal-reload', ordinal], 'daycare ordinal: fresh-process reload without observer passed'),
                             (['pending-egg-id', pending], 'pending egg identity: save, rollback, new branch and older reader passed'),
                             (['pending-egg-id-reload', pending], 'pending egg identity: fresh-process reload passed')]:
            result = subprocess.run([str(build / 'gameplay-tests'), *args], cwd=ROOT, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
            if result.returncode or marker not in result.stdout:
                raise RuntimeError(f'{args[0]} failed ({result.returncode}):\n{result.stdout}')
    print('linux64: daycare ordinals and pending-ID compatibility passed; temporary files removed')


if __name__ == '__main__':
    main()
