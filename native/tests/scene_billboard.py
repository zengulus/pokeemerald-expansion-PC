#!/usr/bin/env python3
"""Test the billboard trace and its installed service in a disposable build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with tempfile.TemporaryDirectory(prefix='rhh-scene-billboard-') as directory:
        work = Path(directory)
        standalone = work / 'billboard-test'
        subprocess.run(['gcc', '-std=gnu17', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', '-Inative/include', 'native/scene_billboard_trace.c',
                        'native/tests/scene_billboard_test.c', '-lm', '-o', str(standalone)], cwd=ROOT, check=True)
        subprocess.run([str(standalone)], cwd=ROOT, env=env, check=True)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64',
                                     f'OBJ_DIR={build}', '-j8', 'gameplay-tests'],
                                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        result = subprocess.run([str(build / 'gameplay-tests'), 'wild-rules'], cwd=ROOT, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if result.returncode or 'overworld service: map, camera' not in result.stdout:
            raise RuntimeError(f'world scene service failed ({result.returncode}):\n{result.stdout}')
        module = work / 'scene-mesh.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/scene_mesh_probe.c', '-o', str(module)], cwd=ROOT, check=True)
        result = subprocess.run([str(build / 'gameplay-tests'), 'scene-mesh', str(module)], cwd=ROOT, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if (result.returncode or 'SCENE_MESH_BAD' in result.stdout or 'SCENE_MESH_PUBLIC_OK' not in result.stdout
                or 'scene mesh: actual Route 103 map tiles' not in result.stdout):
            raise RuntimeError(f'public scene module failed ({result.returncode}):\n{result.stdout}')
    print('linux64: billboard trace and world scene service passed; temporary files removed')


if __name__ == '__main__':
    main()
