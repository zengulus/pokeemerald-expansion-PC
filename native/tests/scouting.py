#!/usr/bin/env python3
"""Exercise scouting and fresh-process persistence, then remove test builds."""
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
        with tempfile.TemporaryDirectory(prefix='rhh-scouting-') as directory:
            work = Path(directory)
            module = work / ('probe.dll' if windows else 'probe.so')
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/scouting_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            def arg(path):
                return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
            try:
                for phase in ('save', 'load'):
                    command = (['wine'] if windows else []) + [str(binary), 'scouting-' + phase,
                                                              arg(work / 'test.sav'), arg(module)]
                    result = subprocess.run(command, cwd=ROOT, env=env, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    marker = ('scouting: visibility' if phase == 'save' else 'scouting: fresh-process save')
                    if (result.returncode or 'SCOUT_PROBE_BAD' in result.stdout or marker not in result.stdout
                            or (phase == 'save' and (result.stdout.count('SCOUT_PROBE_COMMITTED') != 3
                                                     or 'SCOUT_PROBE_DENIED' not in result.stdout))):
                        raise RuntimeError(f'{args.target} scouting {phase} failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print(f'{args.target}: scouting transactions, policy and fresh-process save passed')
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
