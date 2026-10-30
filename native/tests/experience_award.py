#!/usr/bin/env python3
"""Exercise experience awards through the real party menu; clean all test artifacts."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=('linux64', 'windows64'), default='linux64')
    args = parser.parse_args()
    windows = args.target == 'windows64'
    build = ROOT / 'build/native' / args.target
    before = set(build.rglob('*'))
    try:
        result = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'gameplay-tests'],
                                cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join(result.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-experience-award-') as directory:
            work = Path(directory)
            module = work / ('probe.dll' if windows else 'probe.so')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/experience_award_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            command = (['wine'] if windows else []) + [str(build / ('gameplay-tests.exe' if windows else 'gameplay-tests'))]
            try:
                for mode in range(9):
                    result = subprocess.run([*command, 'experience-award', module_arg, str(mode)], cwd=ROOT, env=env,
                                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
                    if (result.returncode or 'EXPERIENCE_AWARD_BAD' in result.stdout
                        or 'EXPERIENCE_AWARD_GATE' not in result.stdout or 'EXPERIENCE_AWARD_FINISHED' not in result.stdout
                        or ('EXPERIENCE_AWARD_APPLIED' in result.stdout) != (mode not in (2, 6, 8))
                        or 'experience award: validated menu flow completed' not in result.stdout):
                        raise RuntimeError(f'{args.target} mode {mode} failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    finally:
        for path in build.rglob('*'):
            if (path.is_file() or path.is_symlink()) and path not in before:
                path.unlink(missing_ok=True)
        for path in sorted((p for p in build.rglob('*') if p.is_dir() and p not in before),
                           key=lambda p: len(p.parts), reverse=True):
            try:
                path.rmdir()
            except OSError:
                pass
    print(f'{args.target}: experience award and party flows passed; test artifacts removed.')


if __name__ == '__main__':
    main()
