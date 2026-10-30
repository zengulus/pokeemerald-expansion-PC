#!/usr/bin/env python3
"""Exercise species type composition through a separately built module."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux32', 'linux64', 'windows32', 'windows64'], default='linux64')
    args = parser.parse_args()
    windows = args.target.startswith('windows')
    bits = '64' if args.target.endswith('64') else '32'
    binary = ROOT / 'build/native' / args.target / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    build_dir = binary.parent
    existing = set(build_dir.rglob('*'))
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-species-types-') as directory:
            work = Path(directory)
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            module = work / ('types.dll' if windows else 'types.so')
            compiler = ('x86_64-w64-mingw32-gcc' if bits == '64' else 'i686-w64-mingw32-gcc') if windows else 'gcc'
            flags = [f'-m{bits}', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/species_types_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            try:
                command = (['wine'] if windows else []) + [str(binary), 'species-types', arg]
                result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, timeout=120)
                if (result.returncode or 'SPECIES_TYPES_BAD' in result.stdout
                        or 'species types: composition' not in result.stdout
                        or result.stdout.count('SPECIES_TYPES_ENTERED') != 2):
                    raise RuntimeError(f'{args.target} species types failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print(f'{args.target}: species type assignments, combat and switching passed')
    finally:
        for path in (p for p in build_dir.rglob('*') if p.is_file() or p.is_symlink()):
            if path not in existing:
                path.unlink(missing_ok=True)
        for path in sorted((p for p in build_dir.rglob('*') if p.is_dir() and p not in existing),
                           key=lambda p: len(p.parts), reverse=True):
            try:
                path.rmdir()
            except OSError:
                pass


if __name__ == '__main__':
    main()
