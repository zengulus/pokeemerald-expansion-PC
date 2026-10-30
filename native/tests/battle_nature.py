#!/usr/bin/env python3
"""Exercise battle nature provenance with a disposable production-code build."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=('linux64', 'windows64'), default='linux64')
    target = parser.parse_args().target
    windows = target == 'windows64'
    with tempfile.TemporaryDirectory(prefix='rhh-battle-nature-') as directory:
        work = Path(directory)
        binary = work / ('battle-nature.exe' if windows else 'battle-nature')
        compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
        flags = ['-m64', '-std=gnu17', '-O2', '-fno-strict-aliasing', '-fwrapv', '-mno-ms-bitfields',
                 '-ffunction-sections', '-fdata-sections', '-DMODERN=1', '-DNATIVE', '-DTESTING=0',
                 '-DEMERALD', '-DNDEBUG', '-iquote', 'include', '-Inative/include']
        if windows:
            flags += ['-flto', '-fwhole-program']
        subprocess.run([compiler, *flags, 'src/battle_util2.c', 'native/tests/battle_nature_test.c',
                        '-Wl,--gc-sections', '-o', str(binary)], cwd=ROOT, check=True)
        env = dict(os.environ, WINEDEBUG='-all')
        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        try:
            subprocess.run((['wine'] if windows else []) + [str(binary)], cwd=ROOT, env=env, check=True, timeout=120)
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f'{target}: battle nature provenance passed; temporary build removed')


if __name__ == '__main__':
    main()
