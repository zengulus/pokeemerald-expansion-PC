#!/usr/bin/env python3
"""Verify a denied Wally tutorial survives a fresh-process save/load."""
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
    before = {p for p in build_dir.rglob('*') if p.is_file() or p.is_symlink()}
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-tutorial-retry-') as directory:
            work = Path(directory)
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            module = work / ('probe.dll' if windows else 'probe.so')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/wild_scripted_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            def arg(path):
                return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
            save = work / 'retry.sav'
            try:
                for mode, markers in [('tutorial-retry-save', ('TUTORIAL_CANCELLED', 'TUTORIAL_CLEANUP',
                                                               'tutorial retry: denied scene and restored party saved')),
                                      ('tutorial-retry-load', ('TUTORIAL_PERMISSION 0', 'SCRIPTED_SPECIAL_READY',
                                                               'tutorial retry: fresh-process reload and allowed retry passed'))]:
                    result = subprocess.run((['wine'] if windows else []) + [str(binary), mode, arg(save), arg(module)],
                                            cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    if result.returncode or 'SCRIPTED_PROBE_BAD' in result.stdout or any(m not in result.stdout for m in markers):
                        raise RuntimeError(f'{target} {mode} failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{target}: denied tutorial saved, reloaded, and retried')
    finally:
        for p in (p for p in build_dir.rglob('*') if p.is_file() or p.is_symlink()):
            if p not in before:
                p.unlink(missing_ok=True)
        for p in sorted((p for p in build_dir.rglob('*') if p.is_dir()), key=lambda p: len(p.parts), reverse=True):
            try:
                p.rmdir()
            except OSError:
                pass


if __name__ == '__main__':
    main()
