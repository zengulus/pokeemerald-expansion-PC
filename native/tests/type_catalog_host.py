#!/usr/bin/env python3
"""Run the host ABI and custom-type catalog tests in a temporary build."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux32', 'linux64', 'windows32', 'windows64'], default='linux64')
    args = parser.parse_args()
    windows = args.target.startswith('windows')
    bits = '64' if args.target.endswith('64') else '32'
    compiler = ('x86_64-w64-mingw32-gcc' if bits == '64' else 'i686-w64-mingw32-gcc') if windows else 'gcc'
    with tempfile.TemporaryDirectory(prefix='rhh-type-catalog-host-') as directory:
        work = Path(directory)
        binary = work / ('host-test.exe' if windows else 'host-test')
        subprocess.run([compiler, f'-m{bits}', '-std=gnu17', '-O2', '-g', '-ffunction-sections', '-fdata-sections',
                        '-iquote', 'include', '-Inative/include', 'native/mod_host.c', 'native/tests/mod_host_test.c',
                        '-Wl,--gc-sections', '-o', str(binary)], cwd=ROOT, check=True)
        env = dict(os.environ, WINEDEBUG='-all')
        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        try:
            subprocess.run((['wine'] if windows else []) + [str(binary)], cwd=ROOT, env=env, check=True, timeout=120)
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{args.target}: custom type catalog and host ABI passed')


if __name__ == '__main__':
    main()
