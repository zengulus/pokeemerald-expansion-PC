#!/usr/bin/env python3
"""Check live identity audits and tracked transitions without retaining test builds."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux64', 'windows64'], default='linux64')
    target = parser.parse_args().target
    windows = target == 'windows64'
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    with tempfile.TemporaryDirectory(prefix='rhh-owned-audit-') as directory:
        work = Path(directory)
        build_dir = work / 'build'
        binary = build_dir / ('owned-identity-tests.exe' if windows else 'owned-identity-tests')
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', f'OBJ_DIR={build_dir}', '-j8', 'owned-identity-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        modes = [('owned-live-audit', 'owned live audit: stable views, unsupported changes and UI ordering passed'),
                 ('nature-storage', 'nature storage: staged codec, legacy defaults, checksum and serialized roundtrip passed'),
                 ('owned-live-audit-capacity', 'owned live audit: full storage 120 checks'),
                 ('owned-creation-audit', 'owned creation audit: unreported identical and empty-slot creations fail closed'),
                 ('owned-copy-audit', 'owned copy audit: unreported identical, partial and empty-slot copies fail closed'),
                 ('owned-identity-write-audit', 'owned identity write audit: transient identity and occupancy changes fail closed'),
                 ('owned-trade-copy-guard', 'owned trade copy: changed staged or provisional Pokemon invalidates tracking'),
                 ('owned-consistency-ledger', 'owned consistency ledger: activation and save occupancy checks passed'),
                 ('owned-pc-ledger', 'owned PC ledger: pickup, reused slots, shift, group, release and fail-closed passed'),
                 ('owned-world-ledger', 'owned world ledger: seed, reorder, daycare, fusion, gift and evolution passed'),
                 ('owned-follower-ledger', 'owned follower ledger: selection, mapping and restoration passed')]
        modes.extend((f'owned-{kind}-ledger', f'owned {kind} ledger:') for kind in
                     ('egg', 'catch', 'trade', 'sky', 'union', 'link-multi', 'frontier', 'rental', 'dome', 'pyramid', 'follower-catch'))
        result = subprocess.run((['wine'] if windows else []) + [str(binary), 'wild-proposal-purity'],
                                cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode or 'wild proposals: pure rules compose, RNG fallback and habitat isolation passed' not in result.stdout:
            raise RuntimeError(f'{target} wild proposal purity failed ({result.returncode}):\n{result.stdout}')
        source_library, target_library = work / 'music-source', work / 'music-target'
        source_library.mkdir(); target_library.mkdir()
        def test_arg(path):
            return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
        result = subprocess.run((['wine'] if windows else []) + [str(binary), 'music-transfer',
                                test_arg(ROOT / 'native/assets/music'), test_arg(source_library), test_arg(target_library)],
                                cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode or 'music transfer: verified pair, stable key and independent library passed' not in result.stdout:
            raise RuntimeError(f'{target} music transfer failed ({result.returncode}):\n{result.stdout}')
        try:
            module = work / ('wild-catch-origin.dll' if windows else 'wild-catch-origin.so')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows: flags.append('-fPIC')
            authored_module = work / ('authored-wild.dll' if windows else 'authored-wild.so')
            subprocess.run([compiler, *flags, 'native/tests/authored_wild_preview_probe.c', '-o', str(authored_module)], cwd=ROOT, check=True)
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'authored-wild-preview', test_arg(authored_module)],
                                    cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if (result.returncode or 'AUTHORED_WILD_PREVIEW_BAD' in result.stdout
                or 'authored wild preview: unloaded habitat, composition, fallback and RNG passed' not in result.stdout):
                raise RuntimeError(f'{target} authored wild preview failed ({result.returncode}):\n{result.stdout}')
            music_module = work / ('music-import.dll' if windows else 'music-import.so')
            subprocess.run([compiler, *flags, 'native/tests/music_import_probe.c', '-o', str(music_module)], cwd=ROOT, check=True)
            music_library = work / 'music-regression'; music_library.mkdir()
            music_save = work / 'music-regression.sav'
            for phase, marker in [('music-import-save', 'music imports: validation'),
                                  ('music-import-load', 'music imports: restart, corrupt-asset fallback and repair'),
                                  ('music-import-delete', 'music imports: shared assets and deleted-track fallback')]:
                if phase == 'music-import-load':
                    source = ROOT / 'native/assets/music/gen1/wild.mid'
                    blob = music_library / ('rhh-music-' + hashlib.sha256(source.read_bytes()).hexdigest() + '.mid')
                    damaged = bytearray(blob.read_bytes()); damaged[-1] ^= 1; blob.write_bytes(damaged)
                result = subprocess.run((['wine'] if windows else []) + [str(binary), phase, test_arg(music_save),
                                        test_arg(music_module), test_arg(ROOT / 'native/assets/music'), test_arg(music_library)],
                                        cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if result.returncode or 'MUSIC_IMPORT_BAD' in result.stdout or marker not in result.stdout:
                    raise RuntimeError(f'{target} {phase} failed ({result.returncode}):\n{result.stdout}')
            subprocess.run([compiler, *flags, 'native/tests/wild_catch_origin_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'wild-catch-origin', module_arg],
                                    cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if (result.returncode or 'wild catch origin: single, double, identical opponents' not in result.stdout
                or 'WILD_CATCH_ORIGIN_BAD' in result.stdout):
                raise RuntimeError(f'{target} wild catch origin failed ({result.returncode}):\n{result.stdout}')
            subprocess.run([compiler, *flags, '-DBATTLE_ONLY', 'native/tests/wild_catch_origin_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'wild-battle-catch-only', module_arg],
                                    cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if (result.returncode or 'wild battle catch: independent observer and double opponents passed' not in result.stdout
                or 'WILD_CATCH_ORIGIN_BAD' in result.stdout):
                raise RuntimeError(f'{target} wild battle catch failed ({result.returncode}):\n{result.stdout}')
            subprocess.run([compiler, *flags, '-DVIEW_ONLY', 'native/tests/wild_catch_origin_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'wild-battle-view-only', module_arg],
                                    cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if (result.returncode or 'wild battle actor: capture rekey without catch observers passed' not in result.stdout
                or 'WILD_CATCH_ORIGIN_BAD' in result.stdout):
                raise RuntimeError(f'{target} wild battle view failed ({result.returncode}):\n{result.stdout}')
            subprocess.run([compiler, *flags, 'native/tests/wild_battle_trait_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'wild-battle-traits', module_arg],
                                    cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if (result.returncode or 'wild battle traits: independent rules, capture, RNG and battle lifecycle passed' not in result.stdout
                or 'WILD_BATTLE_TRAITS_BAD' in result.stdout):
                raise RuntimeError(f'{target} wild battle traits failed ({result.returncode}):\n{result.stdout}')
            subprocess.run([compiler, *flags, 'native/tests/wild_battle_profile_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'wild-battle-profile', module_arg],
                                    cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if (result.returncode or 'wild battle profile: captured fields, identity, witness and battle lifecycle passed' not in result.stdout
                or 'WILD_BATTLE_PROFILE_BAD' in result.stdout):
                raise RuntimeError(f'{target} wild battle profile failed ({result.returncode}):\n{result.stdout}')
            subprocess.run([compiler, *flags, 'native/tests/wild_traits_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            def arg(path):
                return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
            for phase in ('wild-actor-save', 'wild-actor-load', 'wild-actor-load'):
                result = subprocess.run((['wine'] if windows else []) + [str(binary), phase, arg(work / 'wild.sav'),
                                        arg(work / 'wild-expected.bin'), module_arg], cwd=ROOT, env=env,
                                        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                required = ('WILD_TRAITS_SESSION', 'WILD_TRAITS_IDENTIFIED') if phase == 'wild-actor-save' else ('WILD_TRAITS_RESTORED', 'WILD_TRAITS_IDENTIFIED')
                if result.returncode or 'WILD_TRAITS_BAD' in result.stdout or any(m not in result.stdout for m in required):
                    raise RuntimeError(f'{target} {phase} traits failed ({result.returncode}):\n{result.stdout}')
            subprocess.run([compiler, *flags, 'native/tests/owned_identity_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'owned-identity',
                                    arg(work / 'owned-observer.sav'), module_arg], cwd=ROOT, env=env,
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if (result.returncode or 'OWNED_IDENTITY_BAD' in result.stdout
                or 'OWNED_IDENTITY_PROBE_OK' not in result.stdout
                or result.stdout.count('OWNED_EVOLUTION_OBSERVED') != 6
                or result.stdout.count('OWNED_LINK_TRADE_SAVED_OBSERVED') != 2
                or 'owned identity: hidden locations, corrupt collisions and safe queries passed' not in result.stdout):
                raise RuntimeError(f'{target} owned identity observer failed ({result.returncode}):\n{result.stdout}')
            for mode, marker in modes:
                save = work / (mode + '.sav')
                save_arg = 'Z:' + str(save).replace('/', '\\') if windows else str(save)
                result = subprocess.run((['wine'] if windows else []) + [str(binary), mode, save_arg],
                                        cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if result.returncode or marker not in result.stdout:
                    raise RuntimeError(f'{target} {mode} failed ({result.returncode}):\n{result.stdout}')
                if mode == 'owned-live-audit-capacity':
                    print(f'{target}: {result.stdout.strip()}')
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{target}: owned identity live audit passed')


if __name__ == '__main__':
    main()
