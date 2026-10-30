#!/usr/bin/env python3
"""Exercise complete owned-identity queries with a disposable test build."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from pc_save_test_helpers import extension_offset, replace_extension

ROOT = Path(__file__).resolve().parents[2]


def transform_witness_save(source, mode):
    """Change the witness part while preserving the outer save checksums."""
    data = bytearray(source)
    start = extension_offset(data)
    extension = bytearray(data[start:])
    if extension[:8] != b'RHHEX004':
        raise RuntimeError('expected archive-bearing native save')
    word = lambda n: int.from_bytes(extension[n:n + 4], 'little')
    snapshot_end = 80 + word(28) + word(36)
    offset, archives, changed = snapshot_end, [], False
    for generation, length in enumerate((word(64), word(68))):
        archive = extension[offset:offset + length]
        offset += length
        if not archive:
            archives.append(archive)
            continue
        if archive[:8] != b'RHHARC01':
            raise RuntimeError('expected named archive envelope')
        at, parts = 16, []
        for _ in range(int.from_bytes(archive[12:16], 'little')):
            name = bytes(archive[at:at + 64])
            size = int.from_bytes(archive[at + 64:at + 68], 'little')
            payload = bytearray(archive[at + 68:at + 68 + size]); at += 68 + size
            if name.split(b'\0', 1)[0] == b'rhh:owned_identity_witness':
                if payload[:8] not in (b'RHIWIT01', b'RHIWIT02'):
                    raise RuntimeError('expected witness archive')
                stride = 32 if payload[:8] == b'RHIWIT02' else 24
                if mode == 'version1':
                    count = int.from_bytes(payload[12:16], 'little')
                    payload = payload[:32] + b''.join(payload[32 + i * stride:56 + i * stride] for i in range(count))
                    payload[:8] = b'RHIWIT01'
                    payload[8:12] = len(payload).to_bytes(4, 'little')
                    changed = True
                if mode == 'legacy':
                    changed = True
                    continue
                if mode != 'version1' and generation == (1 if mode == 'previous-revision' else 0):
                    if mode in ('revision', 'previous-revision'):
                        revision = int.from_bytes(payload[24:32], 'little')
                        payload[24:32] = (revision + 1).to_bytes(8, 'little')
                    elif mode == 'duplicate':
                        if int.from_bytes(payload[12:16], 'little') < 2:
                            raise RuntimeError('need at least two witnesses')
                        payload[32 + stride:48 + stride] = payload[32:48]
                    elif mode == 'id':
                        payload[32] ^= 0x80
                    else:
                        raise RuntimeError('unknown witness transformation')
                    changed = True
            parts.append((name, payload))
        if at != len(archive) or not parts:
            raise RuntimeError('invalid archive envelope')
        rebuilt = bytearray(b'RHHARC01' + b'\0' * 4 + len(parts).to_bytes(4, 'little'))
        for name, payload in parts:
            rebuilt.extend(name); rebuilt.extend(len(payload).to_bytes(4, 'little')); rebuilt.extend(payload)
        rebuilt[8:12] = len(rebuilt).to_bytes(4, 'little')
        archives.append(rebuilt)
    if not changed:
        raise RuntimeError('no witness archive to transform')
    header = bytearray(extension[:80])
    for i, archive in enumerate(archives):
        header[64 + i * 4:68 + i * 4] = len(archive).to_bytes(4, 'little')
    rebuilt = header + extension[80:snapshot_end] + b''.join(archives) + extension[offset:]
    rebuilt[8:12] = len(rebuilt).to_bytes(4, 'little')
    checksum = 2166136261
    for byte in rebuilt[16:]:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    rebuilt[12:16] = checksum.to_bytes(4, 'little')
    return bytes(replace_extension(data, rebuilt))


def transform_history_save(source, legacy):
    """Rewrite history payloads while retaining valid outer save checksums."""
    data = bytearray(source)
    start = extension_offset(data)
    extension = bytearray(data[start:])
    if extension[:8] != b'RHHEX004':
        raise RuntimeError('expected archive-bearing native save')
    word = lambda n: int.from_bytes(extension[n:n + 4], 'little')
    snapshot_end = 80 + word(28) + word(36)
    offset = snapshot_end
    archives = []
    changed = False
    for archive_length in (word(64), word(68)):
        archive = extension[offset:offset + archive_length]
        offset += archive_length
        if not archive:
            archives.append(archive)
            continue
        if archive[:8] != b'RHHARC01':
            archives.append(archive)
            continue
        count = int.from_bytes(archive[12:16], 'little')
        at, parts = 16, []
        for _ in range(count):
            name = bytes(archive[at:at + 64])
            length = int.from_bytes(archive[at + 64:at + 68], 'little')
            payload = bytearray(archive[at + 68:at + 68 + length])
            at += 68 + length
            if name.split(b'\0', 1)[0] == b'rhh:link_trade_history':
                if payload[:8] != b'RHLTR002':
                    raise RuntimeError('expected version 2 link history')
                records = int.from_bytes(payload[12:16], 'little')
                if legacy:
                    old = bytearray(payload[:32])
                    old[:8] = b'RHLTR001'
                    for i in range(records):
                        old.extend(payload[64 + i * 296:64 + i * 296 + 264])
                    old[8:12] = len(old).to_bytes(4, 'little')
                    payload = old
                    changed = True
                elif not changed and records:
                    payload[64 + 264] ^= 1
                    changed = True
            parts.append((name, payload))
        if at != len(archive):
            raise RuntimeError('invalid archive envelope')
        rebuilt = bytearray(b'RHHARC01' + b'\0' * 4 + count.to_bytes(4, 'little'))
        for name, payload in parts:
            rebuilt.extend(name)
            rebuilt.extend(len(payload).to_bytes(4, 'little'))
            rebuilt.extend(payload)
        rebuilt[8:12] = len(rebuilt).to_bytes(4, 'little')
        archives.append(rebuilt)
    if not changed:
        raise RuntimeError('no link history to transform')
    header = bytearray(extension[:80])
    for i, archive in enumerate(archives):
        header[64 + 4 * i:68 + 4 * i] = len(archive).to_bytes(4, 'little')
    rebuilt = header + extension[80:snapshot_end] + b''.join(archives) + extension[offset:]
    rebuilt[8:12] = len(rebuilt).to_bytes(4, 'little')
    checksum = 2166136261
    for byte in rebuilt[16:]:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    rebuilt[12:16] = checksum.to_bytes(4, 'little')
    return bytes(replace_extension(data, rebuilt))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux32', 'linux64', 'windows32', 'windows64'], default='linux64')
    target = parser.parse_args().target
    windows, bits = target.startswith('windows'), ('64' if target.endswith('64') else '32')
    binary = ROOT / 'build/native' / target / ('owned-identity-tests.exe' if windows else 'owned-identity-tests')
    # The test target shares objects with the main build. Preserve existing
    # files and remove every new build artifact, even if dependencies change.
    build_dir = ROOT / 'build/native' / target
    existing = set(build_dir.rglob('*'))
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', '-j8', 'owned-identity-tests'],
                               cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-owned-identity-') as directory:
            module = Path(directory) / ('probe.dll' if windows else 'probe.so')
            compiler = ('x86_64-w64-mingw32-gcc' if bits == '64' else 'i686-w64-mingw32-gcc') if windows else 'gcc'
            flags = [f'-m{bits}', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/owned_identity_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            sky_module = Path(directory) / ('sky-probe.dll' if windows else 'sky-probe.so')
            subprocess.run([compiler, *flags, 'native/tests/owned_sky_probe.c', '-o', str(sky_module)], cwd=ROOT, check=True)
            follower_module = Path(directory) / ('follower-probe.dll' if windows else 'follower-probe.so')
            subprocess.run([compiler, *flags, 'native/tests/owned_follower_probe.c', '-o', str(follower_module)], cwd=ROOT, check=True)
            module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            sky_module_arg = 'Z:' + str(sky_module).replace('/', '\\') if windows else str(sky_module)
            follower_module_arg = 'Z:' + str(follower_module).replace('/', '\\') if windows else str(follower_module)
            save = Path(directory) / 'identity.sav'
            save_arg = 'Z:' + str(save).replace('/', '\\') if windows else str(save)
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
            if windows:
                env['WINEPREFIX'] = str(Path(directory) / 'wine')
            try:
                pc_save = Path(directory) / 'pc-ledger.sav'
                pc_arg = 'Z:' + str(pc_save).replace('/', '\\') if windows else str(pc_save)
                pc_result = subprocess.run((['wine'] if windows else []) + [str(binary), 'owned-pc-ledger', pc_arg],
                                           cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if pc_result.returncode or 'owned PC ledger: pickup, reused slots, shift, group, release and fail-closed passed' not in pc_result.stdout:
                    raise RuntimeError(f'{target} PC ledger failed ({pc_result.returncode}):\n{pc_result.stdout}')
                world_save = Path(directory) / 'world-ledger.sav'
                world_arg = 'Z:' + str(world_save).replace('/', '\\') if windows else str(world_save)
                world_result = subprocess.run((['wine'] if windows else []) + [str(binary), 'owned-world-ledger', world_arg],
                                              cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if world_result.returncode or 'owned world ledger: seed, reorder, daycare, fusion, gift and evolution passed' not in world_result.stdout:
                    raise RuntimeError(f'{target} world ledger failed ({world_result.returncode}):\n{world_result.stdout}')
                for kind in ('egg', 'catch', 'trade', 'consistency', 'sky', 'union', 'link-multi', 'frontier', 'rental', 'dome', 'pyramid', 'follower', 'follower-catch'):
                    extra_save = Path(directory) / f'{kind}-ledger.sav'
                    extra_arg = 'Z:' + str(extra_save).replace('/', '\\') if windows else str(extra_save)
                    extra = subprocess.run((['wine'] if windows else []) + [str(binary), f'owned-{kind}-ledger', extra_arg],
                                           cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    if extra.returncode or f'owned {kind} ledger:' not in extra.stdout:
                        raise RuntimeError(f'{target} {kind} ledger failed ({extra.returncode}):\n{extra.stdout}')
                    if kind == 'sky':
                        sky_save = Path(directory) / 'sky-probe.sav'
                        sky_arg = 'Z:' + str(sky_save).replace('/', '\\') if windows else str(sky_save)
                        sky = subprocess.run((['wine'] if windows else []) +
                            [str(binary), 'owned-sky-probe', sky_arg, sky_module_arg], cwd=ROOT, env=env,
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                        if sky.returncode or 'OWNED_SKY_PROBE_BAD' in sky.stdout or 'OWNED_SKY_PROBE_OK' not in sky.stdout:
                            raise RuntimeError(f'{target} sky ABI probe failed ({sky.returncode}):\n{sky.stdout}')
                    if kind == 'follower':
                        follower_save = Path(directory) / 'follower-probe.sav'
                        follower_arg = 'Z:' + str(follower_save).replace('/', '\\') if windows else str(follower_save)
                        follower = subprocess.run((['wine'] if windows else []) +
                            [str(binary), 'owned-follower-probe', follower_arg, follower_module_arg], cwd=ROOT, env=env,
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                        if follower.returncode or 'OWNED_FOLLOWER_PROBE_BAD' in follower.stdout or 'OWNED_FOLLOWER_PROBE_OK' not in follower.stdout:
                            raise RuntimeError(f'{target} follower ABI probe failed ({follower.returncode}):\n{follower.stdout}')
                    if kind == 'follower-catch':
                        catch_save = Path(directory) / 'follower-catch-probe.sav'
                        catch_arg = 'Z:' + str(catch_save).replace('/', '\\') if windows else str(catch_save)
                        catch = subprocess.run((['wine'] if windows else []) +
                            [str(binary), 'owned-follower-catch-probe', catch_arg, follower_module_arg], cwd=ROOT, env=env,
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                        if catch.returncode or 'OWNED_FOLLOWER_PROBE_BAD' in catch.stdout or catch.stdout.count('OWNED_FOLLOWER_CATCH_OK') != 6:
                            raise RuntimeError(f'{target} follower catch ABI probe failed ({catch.returncode}):\n{catch.stdout}')
                    if kind == 'consistency':
                        reload_result = subprocess.run((['wine'] if windows else []) +
                            [str(binary), 'owned-consistency-reload', extra_arg], cwd=ROOT, env=env,
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                        if reload_result.returncode or 'owned consistency reload: saved witnesses reject substituted slots' not in reload_result.stdout:
                            raise RuntimeError(f'{target} consistency reload failed ({reload_result.returncode}):\n{reload_result.stdout}')
                        for mode in ('legacy', 'version1', 'revision', 'duplicate', 'id', 'previous-revision'):
                            altered = transform_witness_save(extra_save.read_bytes(), mode)
                            altered_path = Path(directory) / f'witness-{mode}.sav'
                            altered_path.write_bytes(altered)
                            altered_arg = 'Z:' + str(altered_path).replace('/', '\\') if windows else str(altered_path)
                            test_mode = ('owned-consistency-legacy' if mode == 'legacy' else
                                         'owned-consistency-v1' if mode == 'version1' else
                                         'owned-consistency-rollback' if mode == 'previous-revision' else 'owned-consistency-reject')
                            check = subprocess.run((['wine'] if windows else []) + [str(binary), test_mode, altered_arg],
                                cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                            if check.returncode or (mode != 'version1' and altered_path.read_bytes() != altered):
                                raise RuntimeError(f'{target} witness {mode} failed ({check.returncode}):\n{check.stdout}')
                            if mode == 'version1':
                                if altered_path.read_bytes() == altered:
                                    raise RuntimeError(f'{target} witness v1 was not saved after migration')
                                migrated = subprocess.run((['wine'] if windows else []) +
                                    [str(binary), 'owned-consistency-reload', altered_arg], cwd=ROOT, env=env,
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                                if migrated.returncode or 'owned consistency reload: saved witnesses reject substituted slots' not in migrated.stdout:
                                    raise RuntimeError(f'{target} migrated witness reload failed ({migrated.returncode}):\n{migrated.stdout}')
                result = subprocess.run((['wine'] if windows else []) + [str(binary), 'owned-identity', save_arg, module_arg],
                                        cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if result.returncode or 'OWNED_IDENTITY_BAD' in result.stdout or any(marker not in result.stdout for marker in
                    ('OWNED_IDENTITY_PROBE_OK', 'OWNED_MOVE_OBSERVED',
                     'owned identity: hidden locations, corrupt collisions and safe queries passed')) \
                    or result.stdout.count('OWNED_MOVE_OBSERVED') != 6 \
                    or result.stdout.count('OWNED_PICKUP_OBSERVED') != 12 \
                    or result.stdout.count('OWNED_STORAGE_UNREADABLE_OBSERVED') != 8 \
                    or result.stdout.count('OWNED_GIFT_OBSERVED') != 6 \
                    or result.stdout.count('OWNED_DEBUG_CREATED_OBSERVED') != 2 \
                    or result.stdout.count('OWNED_MYSTERY_GIFT_OBSERVED') != 1 \
                    or result.stdout.count('OWNED_EVOLUTION_OBSERVED') != 6 \
                    or result.stdout.count('OWNED_PARTY_REORDER_OBSERVED') != 6 \
                    or result.stdout.count('OWNED_DAYCARE_TRANSFER_OBSERVED') != 5 \
                    or result.stdout.count('OWNED_FUSION_TRANSFER_OBSERVED') != 4 \
                    or result.stdout.count('OWNED_BATTLE_BASELINE_OBSERVED') != 1 \
                    or result.stdout.count('OWNED_CATCH_PLACEMENT_OBSERVED') != 4 \
                    or result.stdout.count('OWNED_INGAME_TRADE_OBSERVED') != 3 \
                    or result.stdout.count('OWNED_LINK_TRADE_SAVED_OBSERVED') != 2:
                    raise RuntimeError(f'{target} identity query failed ({result.returncode}):\n{result.stdout}')
                legacy = Path(directory) / 'legacy-history.sav'
                legacy.write_bytes(transform_history_save(save.read_bytes(), True))
                legacy_arg = 'Z:' + str(legacy).replace('/', '\\') if windows else str(legacy)
                migrated = subprocess.run((['wine'] if windows else []) + [str(binary), 'owned-history-read', legacy_arg, module_arg],
                                          cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if migrated.returncode or 'owned link history: legacy migration and stable checkpoints passed' not in migrated.stdout:
                    raise RuntimeError(f'{target} legacy history migration failed ({migrated.returncode}):\n{migrated.stdout}')
                corrupt = Path(directory) / 'corrupt-history.sav'
                corrupt_data = transform_history_save(save.read_bytes(), False)
                corrupt.write_bytes(corrupt_data)
                corrupt_arg = 'Z:' + str(corrupt).replace('/', '\\') if windows else str(corrupt)
                rejected = subprocess.run((['wine'] if windows else []) + [str(binary), 'owned-history-read', corrupt_arg, module_arg],
                                          cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if not rejected.returncode or corrupt.read_bytes() != corrupt_data:
                    raise RuntimeError(f'{target} corrupted history was accepted or modified:\n{rejected.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{target}: owned identity, transfers, catch and saved link trade checks passed')
    finally:
        for path in build_dir.rglob('*'):
            if (path.is_file() or path.is_symlink()) and path not in existing:
                path.unlink(missing_ok=True)
        for path in sorted((p for p in build_dir.rglob('*') if p.is_dir() and p not in existing),
                           key=lambda p: len(p.parts), reverse=True):
            try:
                path.rmdir()
            except OSError:
                pass


if __name__ == '__main__':
    main()
