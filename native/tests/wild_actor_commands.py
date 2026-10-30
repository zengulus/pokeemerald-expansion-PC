#!/usr/bin/env python3
"""Exercise actor commands with real OWE spawns in a self-cleaning build."""
import os
from pathlib import Path
import struct
import subprocess
import tempfile
from pc_save_test_helpers import extension_offset, replace_extension

ROOT = Path(__file__).resolve().parents[2]


def make_legacy_dormant_save(source, dest):
    """Re-encode the test save with the preceding dormant archive format."""
    data = bytearray(source.read_bytes())
    split = extension_offset(data)
    ext = data[split:]
    assert ext[:8] == b'RHHEX004'
    snapshot_sizes = [struct.unpack_from('<I', ext, 28 + 8 * i)[0] for i in range(2)]
    archive_sizes = [struct.unpack_from('<I', ext, 64 + 4 * i)[0] for i in range(2)]
    at = 80
    snapshots = []
    for size in snapshot_sizes:
        snapshots.append(ext[at:at + size]); at += size
    archives = []
    converted = 0
    for size in archive_sizes:
        archive = ext[at:at + size]; at += size
        if archive and archive[:8] == b'RHHARC01':
            count = struct.unpack_from('<I', archive, 12)[0]
            pos = 16; rewritten = bytearray(archive[:16])
            for _ in range(count):
                name = archive[pos:pos + 64]; length = struct.unpack_from('<I', archive, pos + 64)[0]
                pos += 68; payload = archive[pos:pos + length]; pos += length
                if name.split(b'\0', 1)[0] == b'rhh:wild_dormant':
                    assert payload[:8] == b'RHWDOR02'
                    records = struct.unpack_from('<I', payload, 12)[0]
                    stride = (len(payload) - 32) // records
                    assert records and 32 + records * stride == len(payload) and stride > 8
                    old = bytearray(payload[:32]); old[:8] = b'RHWDOR01'
                    for i in range(records): old.extend(payload[32 + i * stride:32 + (i + 1) * stride - 8])
                    struct.pack_into('<I', old, 8, len(old)); payload = old; converted += 1
                rewritten.extend(name); rewritten.extend(struct.pack('<I', len(payload))); rewritten.extend(payload)
            assert pos == len(archive)
            struct.pack_into('<I', rewritten, 8, len(rewritten)); archive = rewritten
        archives.append(archive)
    assert converted
    header = bytearray(ext[:80]); struct.pack_into('<II', header, 64, *(len(a) for a in archives))
    rebuilt = header + b''.join(snapshots) + b''.join(archives) + ext[at:]
    struct.pack_into('<I', rebuilt, 8, len(rebuilt))
    checksum = 2166136261
    for byte in rebuilt[16:]: checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    struct.pack_into('<I', rebuilt, 12, checksum)
    dest.write_bytes(replace_extension(data, rebuilt))


def main():
    with tempfile.TemporaryDirectory(prefix='rhh-wild-command-test-') as directory:
        work = Path(directory)
        build = work / 'build'
        sdl = subprocess.check_output(['sdl2-config', '--cflags'], text=True).strip()
        flags = ('-iquote include -Inative/include -DMODERN=1 -DNATIVE -DPORTABLE '
                 '-DRENDERER_EASY_DRAW -DTESTING=0 -DEMERALD -DNDEBUG -std=gnu17 '
                 f'-DWE_OW_ENCOUNTERS=TRUE {sdl} -I{build}')
        result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', f'OBJ_DIR={build}',
                                 f'CPPFLAGS={flags}', '-j8', str(build / 'gameplay-tests')], cwd=ROOT,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            print('\n'.join(line for line in result.stdout.splitlines() if 'error:' in line))
            print('\n'.join(result.stdout.splitlines()[-30:]))
            raise RuntimeError(f'Temporary build exited with {result.returncode}')
        probe = work / 'probe.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/wild_actor_command_probe.c', '-o', str(probe)],
                       cwd=ROOT, check=True)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        for module in (None, probe):
            command = [str(build / 'gameplay-tests'), 'wild-actor-auto']
            if module:
                command.append(str(module))
            result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            print(result.stdout, end='')
            if result.returncode:
                raise RuntimeError(f'Actor command test exited with {result.returncode}')
            if module and ('WILD_ACTOR_COMMAND_EXTERNAL_OK' not in result.stdout
                           or 'WILD_ACTOR_BATTLE_RESULT_EXTERNAL_OK' not in result.stdout
                           or 'WILD_STEP_REMOVED_EXTERNAL_OK' not in result.stdout
                           or 'WILD_STEP_QUEUE_EXTERNAL_OK' not in result.stdout
                           or 'WILD_ACTOR_COMMAND_BAD' in result.stdout
                           or any(f'WILD_BEHAVIOR_{i}_OK' not in result.stdout for i in (1, 2, 3, 5, 6, 7))
                           or any(f'WILD_ACTIVITY_CHANGE_{i}_OK' not in result.stdout for i in (1, 5))
                           or 'WILD_BEHAVIOR_4_OK' in result.stdout):
                raise RuntimeError('External mod command probe failed')
        for mode in ('wild-actor-species', 'wild-special-owe-outbreak', 'wild-special-owe-roamer'):
            result = subprocess.run([str(build / 'gameplay-tests'), mode], cwd=ROOT, env=env, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            print(result.stdout, end='')
            marker = 'wild actor: scripted engagement' if mode == 'wild-actor-species' else 'special wild: overworld'
            if result.returncode or marker not in result.stdout:
                raise RuntimeError(f'{mode} failed ({result.returncode})')
        facility_probe = work / 'facility-probe.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/wild_facility_probe.c', '-o', str(facility_probe)],
                       cwd=ROOT, check=True)
        for name in ('pike', 'pyramid'):
            mode = 'wild-facility-owe-' + name
            result = subprocess.run([str(build / 'gameplay-tests'), mode, str(facility_probe)], cwd=ROOT, env=env,
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            print(result.stdout, end='')
            if result.returncode or 'FACILITY_PROBE_BAD' in result.stdout or any(
                marker not in result.stdout for marker in ('FACILITY_PROBE_CANCELLED', 'FACILITY_PROBE_GENERATED',
                                                            f'facility wild: overworld {name} veto')):
                raise RuntimeError(f'{mode} failed ({result.returncode})')
        save, expected = work / 'actors.sav', work / 'expected.bin'
        for phase in ('wild-actor-save', 'wild-actor-load', 'wild-actor-load'):
            result = subprocess.run([str(build / 'gameplay-tests'), phase, str(save), str(expected)],
                                    cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            print(result.stdout, end='')
            if result.returncode:
                raise RuntimeError(f'{phase} exited with {result.returncode}')
        save, expected = work / 'dormant.sav', work / 'dormant-expected.bin'
        dormant_probe = work / 'dormant-probe.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/wild_dormant_probe.c', '-o', str(dormant_probe)],
                       cwd=ROOT, check=True)
        legacy_probe = work / 'dormant-legacy-probe.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/wild_dormant_legacy_probe.c', '-o', str(legacy_probe)],
                       cwd=ROOT, check=True)
        legacy_observer = work / 'dormant-legacy-observer.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-DLEGACY_OBSERVER_ONLY', '-iquote', 'include', 'native/tests/wild_dormant_legacy_probe.c',
                        '-o', str(legacy_observer)], cwd=ROOT, check=True)
        for phase in ('wild-dormant-save', 'wild-dormant-owner-missing', 'wild-dormant-legacy',
                      'wild-dormant-legacy-observer', 'wild-dormant-load', 'wild-dormant-verify'):
            if phase == 'wild-dormant-owner-missing':
                old_save = work / 'dormant-v1.sav'; make_legacy_dormant_save(save, old_save)
                legacy = subprocess.run([str(build / 'gameplay-tests'), 'wild-dormant-legacy-archive', str(old_save), str(expected)],
                                        cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                print(legacy.stdout, end='')
                if legacy.returncode or 'wild dormant clock: legacy archive starts at saved step' not in legacy.stdout:
                    raise RuntimeError(f'legacy dormant archive failed ({legacy.returncode})')
            mode = 'wild-dormant-legacy' if phase == 'wild-dormant-legacy-observer' else phase
            command = [str(build / 'gameplay-tests'), mode, str(save), str(expected)]
            if phase != 'wild-dormant-owner-missing': command.append(str(dormant_probe))
            if phase == 'wild-dormant-legacy': command.append(str(legacy_probe))
            if phase == 'wild-dormant-legacy-observer': command.append(str(legacy_observer))
            result = subprocess.run(command,
                                    cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            print(result.stdout, end='')
            marker = {'wild-dormant-save': 'DORMANT_PROBE_SUSPEND', 'wild-dormant-load': 'DORMANT_PROBE_RESUME'}.get(phase)
            if (result.returncode or 'DORMANT_PROBE_BAD' in result.stdout or (marker and marker not in result.stdout)
                or (phase == 'wild-dormant-save' and result.stdout.count('DORMANT_PROBE_RELOCATE') != 2)
                or (phase == 'wild-dormant-load' and result.stdout.count('DORMANT_PROBE_RELOCATE') != 1)
                or (phase == 'wild-dormant-save' and result.stdout.count('DORMANT_PROFILE_COMMITTED') != 20)
                or (phase == 'wild-dormant-save' and result.stdout.count('DORMANT_PROFILE_BATCH_COMMITTED') != 17)
                or (phase == 'wild-dormant-save' and result.stdout.count('DORMANT_CLOCK_COMMITTED') != 4)
                or (phase == 'wild-dormant-save' and result.stdout.count('DORMANT_WORK_ATOMIC') != 1)
                or (phase == 'wild-dormant-owner-missing' and 'wild dormant profile: missing owner fields retained' not in result.stdout)
                or (phase in ('wild-dormant-legacy', 'wild-dormant-legacy-observer')
                    and 'wild dormant profile: incompatible legacy rules prevent edits' not in result.stdout)):
                raise RuntimeError(f'{phase} exited with {result.returncode}')


if __name__ == '__main__':
    main()
