#!/usr/bin/env python3
"""Exercise scene mesh construction through a separately built module."""
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
    with tempfile.TemporaryDirectory(prefix='rhh-scene-mesh-') as directory:
        work = Path(directory)
        build_dir = work / 'build'
        binary = build_dir / ('gameplay-tests.exe' if windows else 'gameplay-tests')
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        try:
            targets = ['gameplay-tests'] + ([str(build_dir / 'SDL2.dll')] if windows else [])
            with (work / 'build.log').open('w') as log:
                build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}',
                                        f'OBJ_DIR={build_dir}', '-j8', *targets], cwd=ROOT,
                                       stdout=log, stderr=subprocess.STDOUT)
            if build.returncode:
                raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
            module = work / ('mesh.dll' if windows else 'mesh.so')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/scene_mesh_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            command = (['wine'] if windows else []) + [str(binary), 'scene-mesh', arg]
            result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if (result.returncode or 'SCENE_MESH_BAD' in result.stdout or 'SCENE_MESH_PUBLIC_OK' not in result.stdout
                    or 'scene mesh: actual Route 103 map tiles' not in result.stdout):
                raise RuntimeError(f'{args.target} scene mesh failed ({result.returncode}):\n{result.stdout}')
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f'{args.target}: scene mesh shapes, boundaries and public ABI passed; test artifacts removed')


if __name__ == '__main__':
    main()
