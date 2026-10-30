#!/usr/bin/env python3
"""Check the perception ABI and geometry, removing test builds afterward."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux64', 'windows64'], default='linux64')
    args = parser.parse_args()
    windows = args.target == 'windows64'
    binary = ROOT / 'build/native' / args.target / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        result = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'gameplay-tests'],
                                cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if result.returncode:
            raise RuntimeError('\n'.join(result.stdout.splitlines()[-50:]))
        with tempfile.TemporaryDirectory(prefix='rhh-perception-probe-') as directory:
            module = Path(directory) / ('probe.dll' if windows else 'probe.so')
            if windows:
                env['WINEPREFIX'] = str(Path(directory) / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/perception_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            path = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            command = (['wine'] if windows else []) + [str(binary), 'perception', path]
            try:
                result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            markers = ('PERCEPTION_PROBE_OK', 'perception: distance, facing')
            if result.returncode or 'PERCEPTION_PROBE_BAD' in result.stdout or any(m not in result.stdout for m in markers):
                raise RuntimeError(f'{args.target} perception test failed ({result.returncode}):\n{result.stdout}')
            print(f'{args.target}: geometry and independent perception module passed')
    finally:
        subprocess.run(['make', '-s', '-f', 'Makefile.native', f'TARGET={args.target}', 'clean-test-builds'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == '__main__':
    main()
