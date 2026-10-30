#!/usr/bin/env python3
"""Test move-resolution HP summaries in an isolated, disposable build."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=('linux64', 'windows64'), default='linux64')
    target = parser.parse_args().target
    windows = target == 'windows64'
    with tempfile.TemporaryDirectory(prefix='rhh-move-damage-') as directory:
        work = Path(directory)
        build = work / 'build'
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        try:
            binary = build / ('gameplay-tests.exe' if windows else 'gameplay-tests')
            targets = ['gameplay-tests'] + ([str(build / 'SDL2.dll')] if windows else [])
            with (work / 'build.log').open('w') as log:
                result = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}',
                                         f'OBJ_DIR={build}', '-j8', *targets], cwd=ROOT,
                                        stdout=log, stderr=subprocess.STDOUT)
            if result.returncode:
                raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
            module = work / ('observer.dll' if windows else 'observer.so')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/move_resolution_damage_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'move-resolution-damage', arg],
                                    cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            if (result.returncode or 'MOVE_DAMAGE_SUMMARY_BAD' in result.stdout
                or result.stdout.count('MOVE_DAMAGE_SUMMARY_OK') != 27
                or 'MOVE_DAMAGE_HIT_OK' not in result.stdout
                or 'move resolution damage: multi-hit, substitute, ambiguity and reset passed' not in result.stdout):
                raise RuntimeError(f'{target} move damage summary failed ({result.returncode}):\n{result.stdout}')
            accuracy_module = work / ('accuracy.dll' if windows else 'accuracy.so')
            subprocess.run([compiler, *flags, 'native/tests/move_accuracy_checked_probe.c', '-o', str(accuracy_module)], cwd=ROOT, check=True)
            accuracy_arg = 'Z:' + str(accuracy_module).replace('/', '\\') if windows else str(accuracy_module)
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'move-accuracy-checked', accuracy_arg],
                                    cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            if (result.returncode or 'ACCURACY_CHECK_BAD' in result.stdout or result.stdout.count('ACCURACY_CHECK_OK') != 4
                or 'accuracy check: actual checks, OHKO, skipped paths and RNG passed' not in result.stdout):
                raise RuntimeError(f'{target} accuracy check failed ({result.returncode}):\n{result.stdout}')
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f'{target}: move-resolution health and accuracy checks passed; test artifacts removed')


if __name__ == '__main__':
    main()
