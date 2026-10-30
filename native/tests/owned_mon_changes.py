#!/usr/bin/env python3
"""Check owned-mon invalidation with an independent module in a disposable build."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux32', 'linux64', 'windows64'], default='linux64')
    target = parser.parse_args().target
    windows, bits = target.startswith('windows'), ('64' if target.endswith('64') else '32')
    with tempfile.TemporaryDirectory(prefix='rhh-owned-watch-') as directory:
        work = Path(directory)
        build = work / 'build'
        binary = build / ('owned-identity-tests.exe' if windows else 'owned-identity-tests')
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        try:
            targets = ['owned-identity-tests'] + ([str(build / 'SDL2.dll')] if windows else [])
            with (work / 'build.log').open('w') as log:
                result = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', f'OBJ_DIR={build}', '-j8', *targets],
                                        cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
            if result.returncode:
                raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
            module = work / ('probe.dll' if windows else 'probe.so')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = [f'-m{bits}', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/owned_mon_change_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            argument = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'owned-mon-changes', argument],
                                    cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            observed = [line for line in result.stdout.splitlines()
                        if line.startswith('OWNED_WATCH ') or line.startswith('OWNED_WATCH_RESET')]
            deltas = [line for line in result.stdout.splitlines() if line.startswith('OWNED_XP_DELTA ')]
            expected = ['OWNED_WATCH_RESET 456', 'OWNED_WATCH 6 260 1 0 0 5 10',
                        *(['OWNED_WATCH 6 1024 1 0 0 10 10'] * 4),
                        'OWNED_WATCH 6 48 1 0 0 10 10', 'OWNED_WATCH 6 512 1 0 0 10 10',
                        'OWNED_WATCH 6 512 1 0 0 10 10', 'OWNED_WATCH 6 16 1 0 0 10 10',
                        'OWNED_WATCH 6 256 1 0 0 10 10', 'OWNED_WATCH 6 256 1 0 0 10 10',
                        'OWNED_WATCH 6 260 1 0 0 10 11',
                        'OWNED_WATCH 2 0 1 0 0 11 0', 'OWNED_WATCH 4 0 2 0 0 0 11',
                        'OWNED_WATCH 6 16 2 0 0 11 11', 'OWNED_WATCH 6 256 2 0 0 11 11',
                        'OWNED_WATCH 6 1335 2 0 0 11 7',
                        'OWNED_WATCH 2 0 2 0 0 7 0', 'OWNED_WATCH 4 0 3 0 0 0 7',
                        'OWNED_WATCH 6 256 3 0 0 7 7',
                        'OWNED_WATCH 2 0 3 0 0 7 0', 'OWNED_WATCH 4 0 5 0 0 0 7',
                        'OWNED_WATCH 6 272 5 0 0 7 7', 'OWNED_WATCH 2 0 5 0 0 7 0',
                        'OWNED_WATCH_RESET 456', 'OWNED_WATCH_RESET 789']
            _, sep, remaining = result.stdout.partition('OWNED_WATCH_DEFER_BEGIN\n')
            deferred, end, after = remaining.partition('OWNED_WATCH_DEFER_END\n')
            if (result.returncode or 'OWNED_WATCH_BAD' in result.stdout or 'OWNED_WATCH_MODAL_COMMITTED' not in result.stdout
                or not sep or not end or 'OWNED_WATCH ' in deferred or 'OWNED_WATCH 6 272 5 0 0 7 7' not in after
                or observed != expected or result.stdout.count('OWNED_WATCH_TERA_VIEW') != 5
                or deltas != ['OWNED_XP_DELTA 1 1', 'OWNED_XP_DELTA 1 2', 'OWNED_XP_DELTA 2 1', 'OWNED_XP_DELTA 3 1']
                or 'owned watch: level, edits, storage, link and resets passed' not in result.stdout):
                raise RuntimeError(f'{target} owned-mon watch failed ({result.returncode}):\n{result.stdout}')
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f'{target}: owned-mon invalidation, Tera views and independent observer passed; temporary files removed')

if __name__ == '__main__':
    main()
