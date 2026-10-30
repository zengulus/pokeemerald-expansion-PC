#!/usr/bin/env python3
"""Test camera capture on software and, on Linux, real OpenGL; clean up."""
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
    binary = ROOT / 'build/native' / args.target / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    build_dir = binary.parent
    existing = set(build_dir.rglob('*'))
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-photo-') as directory:
            module = Path(directory) / ('probe.dll' if windows else 'probe.so')
            if windows:
                env['WINEPREFIX'] = str(Path(directory) / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/photo_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            try:
                for use_gl in ((0,) if windows else (0, 1)):
                    command = (['wine'] if windows else (['xvfb-run', '-a'] if use_gl else []))
                    command += [str(binary), 'photo', module_arg, str(use_gl)]
                    run_env = dict(env)
                    if use_gl:
                        run_env.pop('SDL_VIDEODRIVER')
                    result = subprocess.run(command, cwd=ROOT, env=run_env, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    markers = ['PHOTO_PROBE_DENIED', 'PHOTO_PROBE_REQUESTED', 'PHOTO_PROBE_CAPTURED 1', 'PHOTO_PROBE_FAILED 5',
                               'PHOTO_PROBE_FAILED 6', 'PHOTO_PROBE_VISIBILITY', 'PHOTO_PROBE_FOOTPRINT', 'photo: software, invalidation',
                               'PHOTO_PROBE_COMPOSITION', 'PHOTO_PROBE_DETAIL',
                               *(f'PHOTO_PROBE_EXTRA_FOOTPRINT {frame}' for frame in range(34, 40))]
                    if use_gl:
                        markers.extend(('PHOTO_PROBE_CAPTURED 2', 'PHOTO_PROBE_GL_VISIBILITY',
                                        'PHOTO_PROBE_GL_EFFECT_UNAVAILABLE', 'PHOTO_PROBE_GL_PRESENTER_UNAVAILABLE',
                                        'PHOTO_PROBE_GL_PRESENTER_MASK', 'PHOTO_PROBE_GL_PRESENTER_EFFECT_UNAVAILABLE',
                                        'PHOTO_PROBE_GL_PRESENTER_INVALID', 'PHOTO_PROBE_GL_SAFE_EFFECT',
                                        'PHOTO_PROBE_GL_SAFE_EFFECT_PRESENTER', 'PHOTO_PROBE_GL_SAFE_PASS',
                                        'PHOTO_PROBE_GL_UNSAFE_PASS', 'PHOTO_PROBE_EXTRA_FOOTPRINT 40', 'PHOTO_PROBE_DEPTH'))
                    if result.returncode or 'PHOTO_PROBE_BAD' in result.stdout or any(m not in result.stdout for m in markers):
                        raise RuntimeError(f'{args.target} photo gl={use_gl} failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print(f'{args.target}: photo visibility, cancellation and renderer capture passed')
    finally:
        for path in build_dir.rglob('*'):
            if (path.is_file() or path.is_symlink()) and path not in existing:
                path.unlink(missing_ok=True)
        for path in sorted((p for p in build_dir.rglob('*') if p.is_dir() and p not in existing),
                           key=lambda p: len(p.parts), reverse=True):
            try:
                path.rmdir()
            except OSError:
                pass


if __name__ == '__main__':
    main()
