#!/usr/bin/env python3
"""Exercise PC save snapshots, rollback and fresh-process recovery."""
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
    with tempfile.TemporaryDirectory(prefix='rhh-pc-save-') as directory:
        work = Path(directory)
        binary = work / ('test.exe' if windows else 'test')
        compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
        flags = ['-std=gnu17', '-O2', '-fno-strict-aliasing', '-fwrapv',
                        '-mno-ms-bitfields', '-ffunction-sections', '-fdata-sections',
                        '-DMODERN=1', '-DNATIVE', '-DTESTING=0', '-DEMERALD', '-DNDEBUG',
                        '-iquote', 'include', '-Inative/include']
        if windows:
            flags += ['-flto', '-fwhole-program']
        subprocess.run([compiler, *flags, 'native/memory.c',
                        'native/save.c', 'native/pc_save_format.c', 'native/mod_host.c',
                        'native/tests/pc_save_integration_test.c', 'native/diagnostics.c',
                        '-Wl,--gc-sections', '-o', str(binary)], cwd=ROOT, check=True)
        save = work / 'game.sav'
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', WINEDEBUG='-all')
        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        def run(mode, path=save):
            arg = 'Z:' + str(path).replace('/', '\\') if windows else str(path)
            subprocess.run((['wine'] if windows else []) + [str(binary), mode, arg],
                           cwd=ROOT, check=True, env=env)
        run('write'); run('read')
        unchanged = save.read_bytes()
        temp = work / 'game.sav.tmp'; temp.mkdir()
        run('fail')
        run('erase-fail')
        assert save.read_bytes() == unchanged
        temp.rmdir()
        data = bytearray(save.read_bytes())
        assert data[:8] == b'RHHPCS01'
        data[80 + 40] ^= 1  # Damage the latest game's payload, preserving its header.
        damaged = work / 'damaged.sav'; damaged.write_bytes(data)
        run('recover', damaged)
        old = work / 'old.sav'; old.write_bytes(b'RHHNATV\0' + bytes(131104))
        run('reject', old)
        run('erase'); run('empty')
        if windows:
            subprocess.run(['wineserver', '-w'], env=env, check=True, timeout=30)
    print(f'{target}: PC save large regions, fresh-process recovery, rollback and erase passed')


if __name__ == '__main__':
    main()
