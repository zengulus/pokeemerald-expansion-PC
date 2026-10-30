#!/usr/bin/env python3
"""Check world-time ABI and ecology composition, removing test builds afterward."""
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
        with tempfile.TemporaryDirectory(prefix='rhh-world-time-probe-') as directory:
            module = Path(directory) / ('probe.dll' if windows else 'probe.so')
            if windows:
                env['WINEPREFIX'] = str(Path(directory) / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/world_time_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            path = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            command = (['wine'] if windows else []) + [str(binary), 'world-time', path]
            try:
                result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            markers = ('WORLD_TIME_HABITAT_OK', 'WORLD_TIME_WEIGHT_OK', 'WORLD_TIME_CANDIDATE_OK',
                       'world time: sampled clock')
            if result.returncode or 'WORLD_TIME_PROBE_BAD' in result.stdout or any(m not in result.stdout for m in markers):
                raise RuntimeError(f'{args.target} world-time test failed ({result.returncode}):\n{result.stdout}')
            print(f'{args.target}: real RTC and independent ecology module passed')
    finally:
        subprocess.run(['make', '-s', '-f', 'Makefile.native', f'TARGET={args.target}', 'clean-test-builds'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == '__main__':
    main()
