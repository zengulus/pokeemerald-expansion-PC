#!/usr/bin/env python3
"""Exercise the internal identity ledger using disposable binaries and saves."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from pc_save_test_helpers import extension_offset, update_checksums

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux64', 'windows64'], default='linux64')
    target = parser.parse_args().target
    windows = target == 'windows64'
    compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
    with tempfile.TemporaryDirectory(prefix='rhh-owned-ledger-') as directory:
        work = Path(directory)
        binary = work / ('owned-ledger.exe' if windows else 'owned-ledger')
        flags = ['-m64', '-std=gnu17', '-O2', '-fno-strict-aliasing', '-fwrapv', '-mno-ms-bitfields',
                 '-ffunction-sections', '-fdata-sections', '-DMODERN=1', '-DNATIVE', '-DTESTING=0',
                 '-DEMERALD', '-DNDEBUG', '-DRHH_OWNED_LEDGER_TEST', '-iquote', 'include', '-Inative/include']
        if windows:
            flags += ['-flto', '-fwhole-program']
        subprocess.run([compiler, *flags, 'native/memory.c', 'native/save.c', 'native/pc_save_format.c', 'native/mod_host.c',
                        'native/owned_identity_ledger.c', 'native/owned_identity_entropy.c', 'native/tests/owned_identity_ledger_test.c',
                        'native/diagnostics.c', '-Wl,--gc-sections', '-o', str(binary)], cwd=ROOT, check=True)
        env = dict(os.environ, WINEDEBUG='-all')
        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        save = work / 'ledger.sav'
        save_arg = 'Z:' + str(save).replace('/', '\\') if windows else str(save)
        try:
            for mode in ('write', 'read'):
                subprocess.run((['wine'] if windows else []) + [str(binary), mode, save_arg],
                               cwd=ROOT, env=env, check=True, timeout=120)
            data = bytearray(save.read_bytes())
            start = extension_offset(data)
            assert data[start:start + 8] == b'RHHEX004'
            word = lambda n: int.from_bytes(data[start + n:start + n + 4], 'little')
            archive = start + 80 + word(28) + word(36)
            assert data[archive:archive + 8] == b'RHHARC01'
            record = archive + 16
            assert data[record:record + 25] == b'rhh:owned_identity_ledger'
            payload = record + 68
            assert data[payload:payload + 8] == b'RHIDLG01'
            corruptions = []
            duplicate = bytearray(data)
            duplicate[payload + 48:payload + 64] = duplicate[payload + 32:payload + 48]
            corruptions.append(duplicate)
            bad_revision = bytearray(data)
            bad_revision[payload + 24:payload + 32] = b'\0' * 8
            corruptions.append(bad_revision)
            bad_count = bytearray(data)
            bad_count[payload + 12:payload + 16] = (0xffffffff).to_bytes(4, 'little')
            corruptions.append(bad_count)
            for index, corrupted in enumerate(corruptions):
                checksum = 2166136261
                for byte in corrupted[start + 16:]:
                    checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
                corrupted[start + 12:start + 16] = checksum.to_bytes(4, 'little')
                update_checksums(corrupted)
                path = work / f'corrupt-{index}.sav'
                path.write_bytes(corrupted)
                path_arg = 'Z:' + str(path).replace('/', '\\') if windows else str(path)
                subprocess.run((['wine'] if windows else []) + [str(binary), 'reject', path_arg],
                               cwd=ROOT, env=env, check=True, timeout=120)
                assert path.read_bytes() == corrupted, 'rejected save was modified'
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f'{target}: owned identity ledger checks passed')


if __name__ == '__main__':
    main()
