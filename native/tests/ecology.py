#!/usr/bin/env python3
"""Verify location ecology values across real save generations and processes."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
from pc_save_test_helpers import extension_offset, update_checksums

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--target', choices=('linux64', 'windows64'), default='linux64')
windows = parser.parse_args().target == 'windows64'
with tempfile.TemporaryDirectory(prefix='rhh-ecology-') as directory:
    work = Path(directory)
    binary = work / ('ecology-test.exe' if windows else 'ecology-test')
    compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
    flags = ['-std=gnu17', '-O2', '-fno-strict-aliasing', '-fwrapv', '-mno-ms-bitfields',
             '-ffunction-sections', '-fdata-sections', '-DMODERN=1', '-DNATIVE', '-DTESTING=0',
             '-DEMERALD', '-DNDEBUG', '-iquote', 'include', '-Inative/include']
    if windows:
        flags += ['-flto', '-fwhole-program']
    subprocess.run([compiler, *flags, 'native/memory.c', 'native/save.c', 'native/pc_save_format.c',
                    'native/mod_host.c', 'native/mod_ecology.c', 'native/diagnostics.c',
                    'native/tests/ecology_test.c', '-Wl,--gc-sections', '-o', str(binary)], cwd=ROOT, check=True)
    env = dict(os.environ, WINEDEBUG='-all')
    if windows:
        env['WINEPREFIX'] = str(work / 'wine')
    def run(mode, file_path):
        arg = 'Z:' + str(file_path).replace('/', '\\') if windows else str(file_path)
        subprocess.run((['wine'] if windows else []) + [str(binary), mode, arg], cwd=ROOT, env=env, check=True)
    save = work / 'game.sav'
    run('write', save)
    data = bytearray(save.read_bytes())
    extension = extension_offset(data)
    archive = data.find(b'RHHECO01', extension)
    assert archive >= 0
    data[archive + 24 + 28] = 1  # Reserved record bytes must stay zero.
    end = extension + int.from_bytes(data[extension + 8:extension + 12], 'little')
    checksum = 2166136261
    for byte in data[extension + 16:end]:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    data[extension + 12:extension + 16] = checksum.to_bytes(4, 'little')
    update_checksums(data)
    corrupt = work / 'corrupt.sav'; corrupt.write_bytes(data)
    run('reject', corrupt)
    assert corrupt.read_bytes() == data
    run('read', save)
    run('removed', save)
    if windows:
        subprocess.run(['wineserver', '-w'], env=env, check=True, timeout=30)
