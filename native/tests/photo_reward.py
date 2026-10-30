#!/usr/bin/env python3
"""Exercise durable photo rewards across fresh processes and save rollback."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from pc_save_test_helpers import replace_game_generations

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux64', 'windows64'], default='linux64')
    target = parser.parse_args().target
    windows = target == 'windows64'
    build_temp = tempfile.TemporaryDirectory(prefix='rhh-photo-build-')
    build_dir = Path(build_temp.name)
    binary = build_dir / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        targets = ['gameplay-tests'] + ([str(build_dir / 'SDL2.dll')] if windows else [])
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', f'OBJ_DIR={build_dir}', '-j8', *targets],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-photo-reward-') as directory:
            work = Path(directory)
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            modules = []
            for name in ('photo_album_probe', 'photo_reward_probe'):
                module = work / (name + ('.dll' if windows else '.so'))
                defines = ['-DPHOTO_REWARD_SKIP_FRAME4'] if name == 'photo_album_probe' else []
                subprocess.run([compiler, *flags, *defines, f'native/tests/{name}.c', '-o', str(module)], cwd=ROOT, check=True)
                modules.append(module)
            photo_probe = work / ('photo_probe.dll' if windows else 'photo_probe.so')
            subprocess.run([compiler, *flags, 'native/tests/photo_probe.c', '-o', str(photo_probe)], cwd=ROOT, check=True)

            def arg(path):
                return 'Z:' + str(path).replace('/', '\\') if windows else str(path)

            def run(save, phase, marker, delivered):
                command = (['wine'] if windows else []) + [str(binary), 'photo-reward', arg(save),
                            *(arg(module) for module in modules), str(phase)]
                result = subprocess.run(command, cwd=ROOT, env=env, text=True,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if (result.returncode or 'PHOTO_REWARD_BAD' in result.stdout or 'ALBUM_PROBE_BAD' in result.stdout
                    or marker not in result.stdout or ('PHOTO_REWARD_DELIVERED' in result.stdout) != delivered):
                    raise RuntimeError(f'{target} phase {phase} failed ({result.returncode}):\n{result.stdout}')

            save = work / 'reward.sav'
            try:
                for command, marker, bad in [(['world-environment-api'], 'world environment: preview, conflicts', None),
                                             (['photo', arg(photo_probe), '0'], 'photo: software, invalidation', 'PHOTO_PROBE_BAD')]:
                    result = subprocess.run((['wine'] if windows else []) + [str(binary), *command], cwd=ROOT,
                                            env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    if result.returncode or marker not in result.stdout or (bad and bad in result.stdout):
                        raise RuntimeError(f'{target} {command[0]} failed ({result.returncode}):\n{result.stdout}')
                run(save, 0, 'photo reward: validated, persisted and delivered once', True)
                initial = save.read_bytes()
                run(save, 1, 'photo reward: pending delivery replayed and consumption saved', True)
                run(save, 2, 'photo reward: saved consumption did not trigger another grant', False)
                rolled_back = replace_game_generations(save.read_bytes(), initial)
                rollback = work / 'rollback.sav'
                rollback.write_bytes(rolled_back)
                run(rollback, 3, 'photo reward: older save received one replayed reward', True)
                unarchived = work / 'unarchived.sav'
                run(unarchived, 4, 'photo reward: unarchived capture earned a durable reward', True)
                run(unarchived, 1, 'photo reward: pending delivery replayed and consumption saved', True)
                run(unarchived, 2, 'photo reward: saved consumption did not trigger another grant', False)
                for phase in (5, 6):
                    unscored = work / f'unscored-{phase}.sav'
                    run(unscored, phase, 'photo reward: unscored capture earned a durable reward', True)
                    run(unscored, 7, 'photo reward: unscored reward replayed and consumption saved', True)
                    run(unscored, 8, 'photo reward: unscored consumption did not trigger another grant', False)
                run(work / 'scored-zero.sav', 9, 'photo reward: scored zero remains distinguishable', True)
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{target}: photo rewards survive rollback without duplicate grants')
    finally:
        build_temp.cleanup()


if __name__ == '__main__':
    main()
