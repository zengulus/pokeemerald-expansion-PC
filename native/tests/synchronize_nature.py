#!/usr/bin/env python3
"""Test the real Synchronize consumer under three configurations in /tmp."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=('linux64', 'windows64'), default='linux64')
    windows = parser.parse_args().target == 'windows64'
    compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
    with tempfile.TemporaryDirectory(prefix='rhh-synchronize-nature-') as directory:
        work = Path(directory)
        overlay = work / 'include'
        overlay.mkdir()
        for entry in (ROOT / 'include').iterdir():
            if entry.name != 'config':
                (overlay / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
        config = overlay / 'config'
        config.mkdir()
        for entry in (ROOT / 'include/config').iterdir():
            if entry.name != 'overworld.h':
                (config / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
        original = (ROOT / 'include/config/overworld.h').read_text()
        old = '#define OW_SYNCHRONIZE_NATURE       GEN_LATEST'
        if original.count(old) != 1:
            raise RuntimeError('Could not prepare Synchronize configuration')
        env = dict(os.environ, WINEDEBUG='-all')
        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        try:
            for generation in (3, 8, 9):
                (config / 'overworld.h').write_text(original.replace(old, old.replace('GEN_LATEST', f'GEN_{generation}')))
                binary = work / ('synchronize.exe' if windows else 'synchronize')
                flags = ['-m64', '-std=gnu17', '-O2', '-ffunction-sections', '-fdata-sections',
                         '-DMODERN=1', '-DNATIVE', '-DTESTING=0', '-DEMERALD', '-DNDEBUG',
                         '-iquote', str(overlay), '-iquote', 'include', '-Inative/include']
                if windows:
                    flags += ['-flto', '-fwhole-program']
                subprocess.run([compiler, *flags, 'src/ow_abilities.c', 'native/tests/synchronize_nature_test.c',
                                '-Wl,--gc-sections', '-o', str(binary)], cwd=ROOT, check=True)
                subprocess.run((['wine'] if windows else []) + [str(binary)], cwd=ROOT, env=env, check=True, timeout=120)
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print('Synchronize configurations passed; temporary build removed')


if __name__ == '__main__':
    main()
