#!/usr/bin/env python3
"""Exercise archive writes at a real photo callback in a disposable build."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--gl', action='store_true', help='Also exercise OpenGL through SDL offscreen')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='rhh-photo-archive-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', f'OBJ_DIR={build}',
                                     '-j8', 'gameplay-tests'], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        module = work / 'photo-probe.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-DRHH_TEST_PHOTO_ARCHIVE', '-iquote', 'include', 'native/tests/photo_probe.c',
                        '-o', str(module)], cwd=ROOT, check=True)
        for use_gl in ((False, True) if args.gl else (False,)):
            env = dict(os.environ, SDL_AUDIODRIVER='dummy')
            env['SDL_VIDEODRIVER'] = 'offscreen' if use_gl else 'dummy'
            command = [str(build / 'gameplay-tests'), 'photo', str(module), '1' if use_gl else '0']
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, timeout=120)
            markers = ['PHOTO_PROBE_ARCHIVE_OK', 'PHOTO_PROBE_ARCHIVE_PREVIEW_BLOCKED',
                       'PHOTO_PROBE_ARCHIVE_FAILURE_BLOCKED', 'PHOTO_PROBE_CAPTURED 1',
                       'PHOTO_PROBE_FAILED 5', 'photo: software, invalidation']
            if use_gl:
                markers += ['PHOTO_PROBE_CAPTURED 2', 'PHOTO_PROBE_GL_SAFE_EFFECT_PRESENTER']
            if result.returncode or 'PHOTO_PROBE_BAD' in result.stdout or any(marker not in result.stdout for marker in markers):
                raise RuntimeError(f'photo archive integration gl={use_gl} failed ({result.returncode}):\n{result.stdout}')
    print('linux64: photo callback archive isolation passed' + (' with OpenGL' if args.gl else '') + '; test artifacts removed')


if __name__ == '__main__':
    main()
