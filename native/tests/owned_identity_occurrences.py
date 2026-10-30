#!/usr/bin/env python3
"""Check owned-identity collision locations with a disposable build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with tempfile.TemporaryDirectory(prefix='rhh-owned-occurrences-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64',
                                     f'OBJ_DIR={build}', '-j8', 'owned-identity-tests'], cwd=ROOT,
                                    stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        module = work / 'probe.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/owned_identity_probe.c', '-o', str(module)], cwd=ROOT, check=True)
        result = subprocess.run([str(build / 'owned-identity-tests'), 'owned-identity', str(work / 'save.sav'), str(module)],
                                cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=180)
        if (result.returncode or 'OWNED_IDENTITY_PROBE_OK' not in result.stdout or 'OWNED_IDENTITY_BAD' in result.stdout
                or 'owned identity: hidden locations, corrupt collisions and safe queries passed' not in result.stdout):
            raise RuntimeError(f'owned identity occurrence test failed ({result.returncode}):\n{result.stdout}')
    print('linux64: owned identity collision locations passed; temporary files removed')


if __name__ == '__main__':
    main()
