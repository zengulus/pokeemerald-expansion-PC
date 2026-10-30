#!/usr/bin/env python3
"""Exercise scripted battle starts with an external mod; clean test builds."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux64', 'windows64'], default='linux64')
    args = parser.parse_args()
    windows = args.target == 'windows64'
    build_dir = ROOT / 'build/native' / args.target
    binary = build_dir / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    before = {path for path in build_dir.rglob('*') if path.is_file() or path.is_symlink()}
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-wild-scripted-') as directory:
            module = Path(directory) / ('probe.dll' if windows else 'probe.so')
            if windows:
                env['WINEPREFIX'] = str(Path(directory) / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/wild_scripted_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            command = (['wine'] if windows else []) + [str(binary), 'wild-scripted', module_arg]
            try:
                result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT)
                if (result.returncode or 'SCRIPTED_PROBE_BAD' in result.stdout
                        or result.stdout.count('SCRIPTED_BATTLE_SOURCE ') != 14
                        or result.stdout.count('SCRIPTED_FIRST_READY') != 1
                        or result.stdout.count('SCRIPTED_SPECIAL_READY ') != 5
                        or result.stdout.count('SCRIPTED_UNMARKED_READY') != 1
                        or result.stdout.count('TUTORIAL_PERMISSION 1') != 4
                        or result.stdout.count('TUTORIAL_PERMISSION 0') != 2
                        or result.stdout.splitlines().count('TUTORIAL_CANCELLED') != 4
                        or result.stdout.splitlines().count('TUTORIAL_CLEANUP') != 3
                        or result.stdout.count('SCRIPTED_READY slot=0') != 8
                        or result.stdout.count('SCRIPTED_READY slot=1') != 1
                        or result.stdout.count('SCRIPTED_CANDIDATE slot=0 level=7') != 3
                        or result.stdout.count('SCRIPTED_CANDIDATE slot=1 level=9') != 1
                        or result.stdout.count('SCRIPTED_CANDIDATE slot=0 level=10') != 1
                        or result.stdout.count('SCRIPTED_CANDIDATE slot=1 level=10') != 1
                        or result.stdout.count('SCRIPTED_CANCELLED slot=0 level=10') != 1
                        or result.stdout.count('SCRIPTED_CANCELLED slot=1 level=10') != 1
                        or result.stdout.count('SCRIPTED_CANCELLED slot=0 level=30') != 18
                        or result.stdout.count('EVENT_CANCELLED slot=0 level=10') != 1
                        or result.stdout.count('EVENT_CANCELLED slot=0 level=70') != 2
                        or result.stdout.count('EVENT_CANCELLED slot=0 level=50') != 2
                        or result.stdout.count('EVENT_CANCELLED slot=0 level=30') != 3
                        or result.stdout.count('SCRIPTED_CANCELLED slot=0 level=70') != 3
                        or result.stdout.count('SCRIPTED_CLEANUP slot=0') != 10
                        or result.stdout.count('EVENT_CLEANUP slot=0') != 7
                        or result.stdout.splitlines().count('EVENT_CANDIDATE level=7') != 1
                        or result.stdout.splitlines().count('EVENT_CANDIDATE level=9') != 1
                        or result.stdout.splitlines().count('EVENT_CANDIDATE level=10') != 1
                        or result.stdout.splitlines().count('EVENT_CANDIDATE level=11') != 1
                        or result.stdout.splitlines().count('EVENT_CANDIDATE level=70') != 3
                        or result.stdout.splitlines().count('EVENT_CANDIDATE level=50') != 2
                        or result.stdout.splitlines().count('EVENT_CANDIDATE level=30') != 3
                        or 'scripted wild: real single, double and legendary battle starts passed' not in result.stdout):
                    raise RuntimeError(f'{args.target} scripted battles failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print(f'{args.target}: scripted battle observations, corrupt data and external mod passed')
    finally:
        for path in (path for path in build_dir.rglob('*') if path.is_file() or path.is_symlink()):
            if path not in before:
                try:
                    path.unlink()
                except FileNotFoundError:
                    pass
        for path in sorted((path for path in build_dir.rglob('*') if path.is_dir()),
                           key=lambda path: len(path.parts), reverse=True):
            try:
                path.rmdir()
            except OSError:
                pass


if __name__ == '__main__':
    main()
