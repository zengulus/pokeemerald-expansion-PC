#!/usr/bin/env python3
"""Test natural-weather notifications in an isolated, disposable build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with tempfile.TemporaryDirectory(prefix='rhh-natural-weather-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', f'OBJ_DIR={build}', '-j8', 'gameplay-tests'],
                                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        for command, marker in [('natural-weather', 'natural weather: coalesced changes, precedence, reset and RNG passed'),
                                ('world-environment-api', 'world environment: preview, conflicts')]:
            result = subprocess.run([str(build / 'gameplay-tests'), command], cwd=ROOT, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
            if result.returncode or marker not in result.stdout:
                raise RuntimeError(f'{command} failed ({result.returncode}):\n{result.stdout}')
    print('linux64: natural weather events and environment regression passed; temporary files removed')


if __name__ == '__main__':
    main()
