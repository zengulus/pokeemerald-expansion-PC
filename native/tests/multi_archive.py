#!/usr/bin/env python3
"""Exercise multiple generation archives and legacy recovery in a temporary tree."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from pc_save_test_helpers import extension_offset, update_checksums

ROOT = Path(__file__).resolve().parents[2]


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux64', 'windows64'], default='linux64')
    windows = parser.parse_args().target == 'windows64'
    compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
    with tempfile.TemporaryDirectory(prefix='rhh-multi-archive-') as directory:
        work = Path(directory); binary = work / ('archive.exe' if windows else 'archive')
        flags = ['-m64', '-std=gnu17', '-O2', '-fno-strict-aliasing', '-fwrapv', '-mno-ms-bitfields',
                 '-ffunction-sections', '-fdata-sections', '-DMODERN=1', '-DNATIVE', '-DTESTING=0',
                 '-DEMERALD', '-DNDEBUG', '-iquote', 'include', '-Inative/include']
        if windows:
            flags += ['-flto', '-fwhole-program']
        subprocess.run([compiler, *flags, 'native/memory.c', 'native/save.c', 'native/pc_save_format.c', 'native/mod_host.c',
                        'native/tests/multi_archive_test.c', 'native/diagnostics.c', '-Wl,--gc-sections',
                        '-o', str(binary)], cwd=ROOT, check=True)
        original = work / ('mod-save.exe' if windows else 'mod-save')
        subprocess.run([compiler, *flags, 'native/memory.c', 'native/save.c', 'native/pc_save_format.c', 'native/mod_host.c',
                        'native/tests/mod_save_test.c', 'native/diagnostics.c', '-Wl,--gc-sections',
                        '-o', str(original)], cwd=ROOT, check=True)
        env = dict(os.environ, WINEDEBUG='-all')
        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        def arg(path):
            return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
        def run(mode, path):
            subprocess.run((['wine'] if windows else []) + [str(binary), mode, arg(path)],
                           cwd=ROOT, env=env, check=True, timeout=120)
        try:
            normal = work / 'normal.sav'; reverse = work / 'reverse.sav'; legacy = work / 'legacy.sav'
            run('write', normal); run('write-reverse', reverse)
            assert normal.read_bytes() == reverse.read_bytes(), 'archive order depends on registration order'
            run('read', normal); run('missing', normal); run('fail-open', normal)
            run('legacy-write', legacy); run('legacy-read', legacy); run('read', legacy)
            data = bytearray(normal.read_bytes()); start = extension_offset(data)
            assert data[start:start + 8] == b'RHHEX004'
            word = lambda n: int.from_bytes(data[start + n:start + n + 4], 'little')
            archive = start + 80 + word(28) + word(36)
            assert data[archive:archive + 8] == b'RHHARC01'
            old_archive = archive + word(64)
            assert data[old_archive:old_archive + 8] == b'TESTOLD1'
            first = archive + 16; first_len = int.from_bytes(data[first + 64:first + 68], 'little')
            second = first + 68 + first_len
            assert data[first:first + 13] == b'test:identity' and data[second:second + 10] == b'test:photo'
            corruptions = []
            bad = bytearray(data); bad[first] = ord('u'); corruptions.append(bad)
            bad = bytearray(data); bad[second:second + 64] = data[first:first + 64]; corruptions.append(bad)
            bad = bytearray(data); bad[first + 64:first + 68] = (0xffffffff).to_bytes(4, 'little'); corruptions.append(bad)
            bad = bytearray(data); bad[old_archive + 8] = 100; corruptions.append(bad)
            for index, bad in enumerate(corruptions):
                bad[start + 12:start + 16] = checksum(bad[start + 16:]).to_bytes(4, 'little')
                update_checksums(bad)
                path = work / f'corrupt{index}.sav'; path.write_bytes(bad)
                run('reject', path); assert path.read_bytes() == bad, 'rejected save was modified'
            plain = work / 'plain.sav'
            for mode in ('write', 'read', 'missing'):
                subprocess.run((['wine'] if windows else []) + [str(original), mode, arg(plain)],
                               cwd=ROOT, env=env, check=True, timeout=120)
            plain_bytes = plain.read_bytes(); (work / 'plain.sav.tmp').mkdir()
            subprocess.run((['wine'] if windows else []) + [str(original), 'fail', arg(plain)],
                           cwd=ROOT, env=env, check=True, timeout=120)
            assert plain.read_bytes() == plain_bytes, 'failed save transaction changed the file'
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(('windows64' if windows else 'linux64') + ': multiple archives and legacy recovery passed')


if __name__ == '__main__':
    main()
