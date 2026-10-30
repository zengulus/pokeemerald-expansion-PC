#!/usr/bin/env python3
"""Exercise contest animation type resolution with a disposable test build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    build_dir = ROOT / 'build/native/linux64'
    existing = set(build_dir.rglob('*'))
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', '-j8', 'contest-type-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-40:]))
        with tempfile.TemporaryDirectory(prefix='rhh-contest-type-') as directory:
            module = Path(directory) / 'probe.so'
            subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-iquote', 'include', 'native/tests/contest_type_probe.c', '-o', str(module)],
                           cwd=ROOT, check=True)
            run = subprocess.run([str(build_dir / 'contest-type-tests'), str(module)], cwd=ROOT,
                                 env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'),
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            if run.returncode or 'contest Curse animation follows resolved and original type lists' not in run.stdout:
                raise RuntimeError(f'contest type test failed ({run.returncode}):\n{run.stdout}')
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
    print('Contest type resolution and external mod passed; test artifacts removed.')


if __name__ == '__main__':
    main()
