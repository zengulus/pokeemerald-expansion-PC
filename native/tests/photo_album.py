#!/usr/bin/env python3
"""Verify bounded photo archives and save generations, then clean test builds."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from pc_save_test_helpers import extension_offset, replace_extension, update_checksums

ROOT = Path(__file__).resolve().parents[2]


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def records(snapshot):
    assert snapshot[:8] == b'RHHMOD01'
    assert int.from_bytes(snapshot[8:12], 'little') == len(snapshot)
    pos = 24
    for _ in range(int.from_bytes(snapshot[12:16], 'little')):
        size = int.from_bytes(snapshot[pos + 68:pos + 72], 'little')
        end = pos + 80 + size
        assert end <= len(snapshot)
        yield pos, end, bytes(snapshot[pos:pos + 64]).split(b'\0', 1)[0]
        pos = end
    assert pos == len(snapshot)


def without_score_state(snapshot):
    entries = list(records(snapshot))
    assert sum(name == b'rhh:photo_score_state' for _, _, name in entries) == 1
    body = b''.join(snapshot[a:b] for a, b, name in entries if name != b'rhh:photo_score_state')
    result = bytearray(snapshot[:24] + body)
    result[8:12] = len(result).to_bytes(4, 'little')
    result[12:16] = (len(entries) - 1).to_bytes(4, 'little')
    result[16:20] = checksum(body).to_bytes(4, 'little')
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux64', 'windows64'], default='linux64')
    args = parser.parse_args()
    windows = args.target == 'windows64'
    build_dir = ROOT / 'build/native' / args.target
    binary = build_dir / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    before = set(build_dir.rglob('*'))
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-photo-album-') as directory:
            work = Path(directory)
            module = work / ('probe.dll' if windows else 'probe.so')
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/photo_album_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            def arg(path):
                return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
            def run(phase, path, markers):
                command = (['wine'] if windows else []) + [str(binary), 'photo-album-' + phase,
                                                           arg(path), arg(module), arg(work / 'expected.bin')]
                result = subprocess.run(command, cwd=ROOT, env=env, text=True,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if result.returncode or 'ALBUM_PROBE_BAD' in result.stdout or any(m not in result.stdout for m in markers):
                    raise RuntimeError(f'{args.target} album {phase} failed ({result.returncode}):\n{result.stdout}')
            try:
                save = work / 'album.sav'
                run('save', save, ['ALBUM_PROBE_STORED', 'ALBUM_PROBE_REMOVED', 'album: generation snapshots'])
                data = bytearray(save.read_bytes())
                assert len(data) < 3_000_000, 'bounded photo archive unexpectedly large'
                start = extension_offset(data)
                assert data[start:start + 8] == b'RHHEX004'
                size = int.from_bytes(data[start + 8:start + 12], 'little')
                word = lambda n: int.from_bytes(data[start + n:start + n + 4], 'little')
                archive = start + 80 + word(28) + word(36)
                assert data[archive:archive + 8] == b'RHHPH001' and word(64) and word(68)
                second_archive = archive + word(64)
                assert data[second_archive:second_archive + 8] == b'RHHPH001'
                first_snapshot = start + 80
                second_snapshot = first_snapshot + word(28)
                snapshots = [data[first_snapshot:second_snapshot], data[second_snapshot:archive]]
                old_snapshots = [without_score_state(snapshot) for snapshot in snapshots]
                legacy = bytearray(data[:first_snapshot] + b''.join(old_snapshots) + data[archive:])
                legacy[start + 28:start + 32] = len(old_snapshots[0]).to_bytes(4, 'little')
                legacy[start + 36:start + 40] = len(old_snapshots[1]).to_bytes(4, 'little')
                legacy_size = len(legacy) - start
                legacy[start + 8:start + 12] = legacy_size.to_bytes(4, 'little')
                legacy[start + 12:start + 16] = checksum(legacy[start + 16:]).to_bytes(4, 'little')
                legacy = replace_extension(data, legacy[start:])
                legacy_path = work / 'legacy.sav'; legacy_path.write_bytes(legacy)
                run('legacy', legacy_path, ['album: legacy photos remain unscored'])
                run('legacy-reload', legacy_path, ['album: legacy photos remain unscored'])

                # A valid score record pointing at the wrong photo must be rejected.
                damaged = bytearray(data)
                score_records = [(a, b) for a, b, name in records(snapshots[0])
                                 if name == b'rhh:photo_score_state']
                assert len(score_records) == 1
                score_pos = first_snapshot + score_records[0][0] + 80
                damaged[score_pos + 8] ^= 64
                damaged[first_snapshot + 16:first_snapshot + 20] = checksum(
                    damaged[first_snapshot + 24:second_snapshot]).to_bytes(4, 'little')
                damaged[start + 12:start + 16] = checksum(damaged[start + 16:start + size]).to_bytes(4, 'little')
                update_checksums(damaged)
                corrupt = work / 'corrupt.sav'; corrupt.write_bytes(damaged)
                run('reject', corrupt, [])
                assert corrupt.read_bytes() == damaged, 'rejected mismatched score was modified'
                for base in (archive, second_archive):
                    for offset in (base, base + 16, base + 24 + 7):
                        damaged = bytearray(data); damaged[offset] ^= 128
                        damaged[start + 12:start + 16] = checksum(damaged[start + 16:start + size]).to_bytes(4, 'little')
                        update_checksums(damaged)
                        corrupt = work / 'corrupt.sav'; corrupt.write_bytes(damaged)
                        run('reject', corrupt, [])
                        assert corrupt.read_bytes() == damaged, 'rejected archive was modified'
                run('load', save, ['ALBUM_PROBE_STORED', 'ALBUM_PROBE_FULL', 'ALBUM_PROBE_UNAVAILABLE',
                                   'album: fresh-process restore'])
                assert save.stat().st_size >= 99 * 240 * 160 * 3, 'full archive is missing image data'
                run('full-load', save, ['album: full archive survived'])
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print(f'{args.target}: bounded archive, corrupt save rejection and generation recovery passed')
    finally:
        for path in (p for p in build_dir.rglob('*') if p.is_file() or p.is_symlink()):
            if path not in before:
                path.unlink(missing_ok=True)
        for path in sorted((p for p in build_dir.rglob('*') if p.is_dir() and p not in before),
                           key=lambda p: len(p.parts), reverse=True):
            try:
                path.rmdir()
            except OSError:
                pass


if __name__ == '__main__':
    main()
