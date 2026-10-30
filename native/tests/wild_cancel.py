#!/usr/bin/env python3
"""Exercise encounter cancellation and clean all test builds afterwards."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
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
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-wild-cancel-probe-') as directory:
            module = Path(directory) / ('cancel.dll' if windows else 'cancel.so')
            if windows:
                env['WINEPREFIX'] = str(Path(directory) / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/wild_cancel_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            prefix = (['wine'] if windows else []) + [str(binary)]
            try:
                for mode, extra in [('wild-rules', []), ('wild-cancel', [module_arg])]:
                    result = subprocess.run([*prefix, mode, *extra], cwd=ROOT, env=env, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    markers = ('wild encounter: habitat-aware proposals',) if mode == 'wild-rules' else (
                        'WILD_CANCEL_FISH_OK', 'WILD_CANCEL_FEEBAS_OK', 'WILD_CANCEL_LAND_OK', 'WILD_CANCEL_WATER_OK',
                        'WILD_CANCEL_ROCKS_OK',
                        'WILD_CANCEL_RECOVERY_OK', 'wild cancellation: fishing')
                    if result.returncode or 'WILD_CANCEL_PROBE_BAD' in result.stdout or any(m not in result.stdout for m in markers):
                        raise RuntimeError(f'{args.target} {mode} failed ({result.returncode}):\n{result.stdout}')
                for regression in ('wild_encounter.py', 'wild_actor.py'):
                    subprocess.run([sys.executable, str(ROOT / 'native/tests' / regression), '--target', args.target],
                                   cwd=ROOT, env=env, check=True)
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print(f'{args.target}: cancellation, fishing task, sticky veto and legacy mod compatibility passed')
    finally:
        subprocess.run(['make', '-s', '-f', 'Makefile.native', f'TARGET={args.target}', 'clean-test-builds'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == '__main__':
    main()
