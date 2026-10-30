#!/usr/bin/env python3
"""Exercise battle lifecycle events and clean test builds on every exit."""
import argparse
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
    build_dir = ROOT / 'build/native' / target
    binary = build_dir / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    before = {path for path in build_dir.rglob('*') if path.is_file() or path.is_symlink()}
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-battle-lifecycle-') as directory:
            if windows:
                env['WINEPREFIX'] = str(Path(directory) / 'wine')
            module = Path(directory) / ('probe.dll' if windows else 'probe.so')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/battle_lifecycle_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            try:
                result = subprocess.run((['wine'] if windows else []) + [str(binary), 'battle-lifecycle', module_arg],
                                        cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            expected = {'BATTLE_LIFECYCLE_START': 5, 'BATTLE_LIFECYCLE_INITIAL': 14,
                        'BATTLE_LIFECYCLE_SWITCH': 1, 'BATTLE_LIFECYCLE_END': 4, 'BATTLE_LIFECYCLE_ABORT': 1,
                        'BATTLE_LIFECYCLE_OUTCOME_1': 1, 'BATTLE_LIFECYCLE_OUTCOME_2': 1,
                        'BATTLE_LIFECYCLE_OUTCOME_3': 1, 'BATTLE_LIFECYCLE_OUTCOME_5': 1,
                        'BATTLE_LIFECYCLE_OUTCOME_7': 1}
            if (result.returncode or 'BATTLE_LIFECYCLE_BAD' in result.stdout
                    or 'BATTLE_LIFECYCLE_NATURE' not in result.stdout
                    or 'BATTLE_LIFECYCLE_NATURE_PURE_BLOCKED' not in result.stdout
                    or 'battle lifecycle: trainer rosters' not in result.stdout
                    or any(result.stdout.count(marker) != count for marker, count in expected.items())):
                raise RuntimeError(f'{target} battle lifecycle failed ({result.returncode}):\n{result.stdout}')
        print(f'{target}: real switch opcodes, battle lifecycle and independent module passed')
    finally:
        for path in (path for path in build_dir.rglob('*') if path.is_file() or path.is_symlink()):
            if path not in before:
                path.unlink(missing_ok=True)
        for path in sorted((path for path in build_dir.rglob('*') if path.is_dir()),
                           key=lambda path: len(path.parts), reverse=True):
            try:
                path.rmdir()
            except OSError:
                pass


if __name__ == '__main__':
    main()
