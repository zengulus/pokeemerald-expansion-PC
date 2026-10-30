#!/usr/bin/env python3
"""Test bounded wild routes in a disposable build and save directory."""
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
    with tempfile.TemporaryDirectory(prefix='rhh-wild-route-') as directory:
        work = Path(directory)
        build_dir = work / 'build'
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}',
                                f'OBJ_DIR={build_dir}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-50:]))
        module = work / ('probe.dll' if windows else 'probe.so')
        save = work / 'save.sav'
        compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
        flags = ['-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
        if not windows:
            flags.append('-fPIC')
        subprocess.run([compiler, *flags, 'native/tests/wild_route_probe.c', '-o', str(module)], cwd=ROOT, check=True)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')

        def arg(path):
            return 'Z:' + str(path).replace('/', '\\') if windows else str(path)

        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        try:
            for mode in ('wild-route', 'wild-route-load'):
                command = (['wine'] if windows else []) + [str(build_dir / ('gameplay-tests.exe' if windows else 'gameplay-tests')),
                                                           mode, arg(save), arg(module)]
                result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if mode == 'wild-route':
                    if (result.returncode or 'WILD_ROUTE_BAD' in result.stdout or 'WILD_ROUTE_ACTIVITY_OK' not in result.stdout
                        or 'WILD_ROUTE_TRACE_OK' not in result.stdout or 'WILD_ROUTE_QUEUE_EXTERNAL_OK' not in result.stdout
                        or result.stdout.count('WILD_ROUTE_GUARD_EXTERNAL_OK') != 2
                        or any(f'WILD_ROUTE_INTERRUPT_{i}_OK' not in result.stdout for i in (4, 5, 6))
                        or 'WILD_ROUTE_PREFIX_EXTERNAL_OK' not in result.stdout or 'WILD_ROUTE_COMMIT_OK' not in result.stdout
                        or 'wild route: bounded pure planning and real movement passed' not in result.stdout):
                        raise RuntimeError(f'{mode} failed ({result.returncode}):\n{result.stdout}')
                elif result.returncode or 'wild route: guarded queue restored after reload' not in result.stdout:
                    raise RuntimeError(f'{mode} failed ({result.returncode}):\n{result.stdout}')
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f'{args.target}: wild route planning and movement passed; temporary build and saves removed.')


if __name__ == '__main__':
    main()
