#!/usr/bin/env python3
"""Exercise module archives, rollback, missing owners and malformed saves."""
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
    with tempfile.TemporaryDirectory(prefix='rhh-mod-archive-') as directory:
        work = Path(directory)
        binary = work / 'archive-test'
        flags = ['-m64', '-std=gnu17', '-O2', '-fno-strict-aliasing', '-fwrapv', '-mno-ms-bitfields',
                 '-ffunction-sections', '-fdata-sections', '-DMODERN=1', '-DNATIVE', '-DTESTING=0',
                 '-DEMERALD', '-DNDEBUG', '-iquote', 'include', '-Inative/include']
        subprocess.run(['gcc', *flags, 'native/memory.c', 'native/save.c', 'native/pc_save_format.c', 'native/mod_host.c',
                        'native/tests/mod_archive_test.c', 'native/diagnostics.c', '-Wl,--gc-sections',
                        '-o', str(binary)], cwd=ROOT, check=True)
        def run(mode, path):
            subprocess.run([str(binary), mode, str(path)], cwd=ROOT, check=True, timeout=120,
                           env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'))
        save = work / 'records.sav'
        run('write', save)
        data = bytearray(save.read_bytes())
        extension = extension_offset(data)
        assert data[extension:extension + 8] == b'RHHEX004'
        archive = data.find(b'RHHMDA01', extension)
        assert archive >= 0
        corruptions = []
        for offset, value, width in [(12, 257, 4), (16, 0, 8), (32, 0, 8), (32 + 28, 1, 4),
                                     (32 + 24, 0xffffffff, 4), (32 + 16, 0xffffffffffffffff, 8)]:
            broken = bytearray(data)
            broken[archive + offset:archive + offset + width] = value.to_bytes(width, 'little')
            broken[extension + 12:extension + 16] = checksum(broken[extension + 16:]).to_bytes(4, 'little')
            update_checksums(broken)
            corruptions.append(broken)
        for index, broken in enumerate(corruptions):
            path = work / f'corrupt-{index}.sav'
            path.write_bytes(broken)
            run('reject', path)
            assert path.read_bytes() == broken, 'rejected save was modified'
        run('missing', save)
        run('read', save)
        run('grow', save)
        run('read-large', save)
    print('linux64: module archive, recovery and malformed-save checks passed; test artifacts removed')


if __name__ == '__main__':
    main()
