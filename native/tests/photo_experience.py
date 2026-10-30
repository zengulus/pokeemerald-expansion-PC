#!/usr/bin/env python3
"""Exercise durable photo experience through save rollback and remove test artifacts."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from pc_save_test_helpers import extension_offset, replace_extension, replace_game_generations

ROOT = Path(__file__).resolve().parents[2]


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def downgrade_applied_snapshots(source):
    """Build a v1.2-style saved state while retaining the newer journal."""
    data = bytearray(source)
    start = extension_offset(data)
    extension = bytearray(data[start:])
    if extension[:8] != b'RHHEX004':
        raise RuntimeError('expected archive-bearing native save')
    word = lambda at: int.from_bytes(extension[at:at + 4], 'little')
    lengths = (word(28), word(36)); offset = 80; snapshots = []; changed = False
    for length in lengths:
        snapshot = bytearray(extension[offset:offset + length]); offset += length
        if not snapshot:
            snapshots.append(snapshot)
            continue
        if snapshot[:8] != b'RHHMOD01':
            raise RuntimeError('expected mod snapshot')
        records = []; at = 24
        for _ in range(int.from_bytes(snapshot[12:16], 'little')):
            header = bytearray(snapshot[at:at + 80]); length = int.from_bytes(header[68:72], 'little')
            payload = bytearray(snapshot[at + 80:at + 80 + length]); at += 80 + length
            name = bytes(header[:64]).split(b'\0', 1)[0]
            if name == b'rhh:photo_experience_retarget':
                changed = True
                continue
            if name == b'rhh:photo_experience_applied':
                if len(payload) != 8 + 128 * 8:
                    raise RuntimeError('unexpected experience applied state')
                payload = payload[:8]; header[64:68] = (1).to_bytes(4, 'little')
                header[68:72] = (8).to_bytes(4, 'little'); header[76:80] = (1).to_bytes(4, 'little')
                changed = True
            records.append(header + payload)
        if at != len(snapshot):
            raise RuntimeError('invalid mod snapshot')
        rebuilt = bytearray(snapshot[:24]) + b''.join(records)
        rebuilt[8:12] = len(rebuilt).to_bytes(4, 'little')
        rebuilt[12:16] = len(records).to_bytes(4, 'little')
        rebuilt[16:20] = checksum(rebuilt[24:]).to_bytes(4, 'little')
        snapshots.append(rebuilt)
    if not changed:
        raise RuntimeError('no experience state to downgrade')
    header = bytearray(extension[:80])
    header[28:32] = len(snapshots[0]).to_bytes(4, 'little')
    header[36:40] = len(snapshots[1]).to_bytes(4, 'little')
    rebuilt = header + snapshots[0] + snapshots[1] + extension[offset:]
    rebuilt[8:12] = len(rebuilt).to_bytes(4, 'little')
    rebuilt[12:16] = checksum(rebuilt[16:]).to_bytes(4, 'little')
    return bytes(replace_extension(data, rebuilt))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=('linux64', 'windows64'), default='linux64')
    args = parser.parse_args()
    windows = args.target == 'windows64'
    build = ROOT / 'build/native' / args.target
    before = set(build.rglob('*'))
    try:
        result = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'gameplay-tests'],
                                cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join(result.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-photo-experience-') as directory:
            work = Path(directory)
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            modules = []
            for name in ('photo_album_probe', 'photo_experience_probe'):
                module = work / (name + ('.dll' if windows else '.so'))
                subprocess.run([compiler, *flags, f'native/tests/{name}.c', '-o', str(module)], cwd=ROOT, check=True)
                modules.append(module)
            def arg(path):
                return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
            command = (['wine'] if windows else []) + [str(build / ('gameplay-tests.exe' if windows else 'gameplay-tests'))]
            def run(save, phase, marker, delivered):
                result = subprocess.run([*command, 'photo-experience', arg(save), *(arg(p) for p in modules), str(phase)],
                                        cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
                if (result.returncode or 'PHOTO_EXPERIENCE_BAD' in result.stdout or 'ALBUM_PROBE_BAD' in result.stdout
                    or marker not in result.stdout or ('PHOTO_EXPERIENCE_DELIVERED' in result.stdout) != delivered):
                    raise RuntimeError(f'{args.target} phase {phase} failed ({result.returncode}):\n{result.stdout}')
                if ('PHOTO_EXPERIENCE_ATOMIC' in result.stdout) != delivered:
                    raise RuntimeError(f'{args.target} phase {phase} did not atomically mark delivery:\n{result.stdout}')
                if phase == 10 and (result.stdout.count('PHOTO_EXPERIENCE_RETARGETED') != 1
                                    or 'PHOTO_EXPERIENCE_RETARGET_GATE' not in result.stdout):
                    raise RuntimeError(f'{args.target} retarget notifications failed:\n{result.stdout}')
            try:
                save = work / 'reward.sav'
                run(save, 0, 'photo experience: durable claim and real level-up passed', True)
                initial = save.read_bytes()
                run(save, 1, 'photo experience: pending delivery replayed and saved', True)
                run(save, 2, 'photo experience: saved delivery did not repeat', False)
                rollback_data = replace_game_generations(save.read_bytes(), initial)
                rollback = work / 'rollback.sav'; rollback.write_bytes(rollback_data)
                run(rollback, 3, 'photo experience: older save received one replayed award', True)
                run(work / 'item.sav', 4, 'photo experience: item claim prevents duplicate experience', False)
                unscored = work / 'unscored.sav'
                run(unscored, 5, 'photo experience: unscored claim and real level-up passed', True)
                run(unscored, 6, 'photo experience: pending delivery replayed and saved', True)
                run(unscored, 7, 'photo experience: saved delivery did not repeat', False)
                run(work / 'collision.sav', 8, 'photo experience: ambiguous recipient rolled back and retried', True)
                run(work / 'late-denial.sav', 9, 'photo experience: denied handoff waits for explicit retry', True)
                retarget = work / 'retarget.sav'
                run(retarget, 10, 'photo experience: retargeted receipt and delivered to new recipient', True)
                initial_retarget = retarget.read_bytes()
                run(retarget, 11, 'photo experience: pending delivery replayed and saved', True)
                run(retarget, 12, 'photo experience: saved delivery did not repeat', False)
                old_retarget = replace_game_generations(retarget.read_bytes(), initial_retarget)
                retarget_rollback = work / 'retarget-rollback.sav'; retarget_rollback.write_bytes(old_retarget)
                run(retarget_rollback, 13, 'photo experience: older save received one replayed award', True)
                run(work / 'retarget-capacity.sav', 14, 'photo experience: retarget history capacity enforced', True)
                history = work / 'history.sav'
                run(history, 15, 'photo experience: durable claim and real level-up passed', True)
                before_delivery = history.read_bytes()
                run(history, 1, 'photo experience: pending delivery replayed and saved', True)
                delivered_save = history.read_bytes()
                branch_data = replace_game_generations(delivered_save, before_delivery)
                branch = work / 'history-branch.sav'; branch.write_bytes(branch_data)
                run(branch, 16, 'photo experience: older save retargeted without delivery', False)
                delivered_data = replace_game_generations(branch.read_bytes(), delivered_save)
                delivered_history = work / 'history-delivered.sav'; delivered_history.write_bytes(delivered_data)
                run(delivered_history, 17, 'photo experience: saved delivery did not repeat', False)
                legacy_history = work / 'history-legacy.sav'
                legacy_history.write_bytes(downgrade_applied_snapshots(delivered_data))
                run(legacy_history, 17, 'photo experience: saved delivery did not repeat', False)
                run(branch, 13, 'photo experience: older save received one replayed award', True)
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    finally:
        for path in build.rglob('*'):
            if (path.is_file() or path.is_symlink()) and path not in before:
                path.unlink(missing_ok=True)
        for path in sorted((p for p in build.rglob('*') if p.is_dir() and p not in before),
                           key=lambda p: len(p.parts), reverse=True):
            try:
                path.rmdir()
            except OSError:
                pass
    print(f'{args.target}: photo experience survives rollback without duplicate awards; test artifacts removed.')


if __name__ == '__main__':
    main()
