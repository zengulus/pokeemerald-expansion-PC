#!/usr/bin/env python3
"""Exercise sampled battle type changes in a disposable build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with tempfile.TemporaryDirectory(prefix='rhh-type-observation-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64',
                                     f'OBJ_DIR={build}', '-j8', 'gameplay-tests'],
                                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        module = work / 'observer.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/type_observation_probe.c', '-o', str(module)],
                       cwd=ROOT, check=True)
        result = subprocess.run([str(build / 'gameplay-tests'), 'type-observation', str(module)],
                                cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=120)
        if (result.returncode or 'TYPE_OBSERVATION_BAD' in result.stdout
            or 'type observation: raw, effective, simulation, switch, link and RNG passed' not in result.stdout):
            raise RuntimeError(f'type observation failed ({result.returncode}):\n{result.stdout}')
    print('linux64: battle type observations passed; temporary files removed')


if __name__ == '__main__':
    main()
