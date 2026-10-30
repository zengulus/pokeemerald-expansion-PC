#!/usr/bin/env python3
"""Exercise item EV policies and remove all newly created build files."""
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
    before = set(build_dir.rglob('*'))
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-item-ev-') as directory:
            work = Path(directory)
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            modules = []
            for name in ('item_ev_probe', 'ev_limits_probe'):
                module = work / (name + ('.dll' if windows else '.so'))
                subprocess.run([compiler, *flags, f'native/tests/{name}.c', '-o', str(module)], cwd=ROOT, check=True)
                modules.append('Z:' + str(module).replace('/', '\\') if windows else str(module))
            reduction = work / ('reduction.dll' if windows else 'reduction.so')
            subprocess.run([compiler, *flags, 'native/tests/item_ev_reduction_probe.c', '-o', str(reduction)], cwd=ROOT, check=True)
            reduction_arg = 'Z:' + str(reduction).replace('/', '\\') if windows else str(reduction)
            try:
                result = subprocess.run((['wine'] if windows else []) + [str(binary), 'item-ev-yield', *modules],
                                        cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if (result.returncode or 'ITEM_EV_BAD' in result.stdout or 'EV_LIMIT_BAD' in result.stdout
                    or 'item EV yield: applied, banked, denied, caps and compatibility passed' not in result.stdout
                    or 'ITEM_EV_PROCESSED' not in result.stdout):
                    raise RuntimeError(f'{target} item EV test failed ({result.returncode}):\n{result.stdout}')
                result = subprocess.run((['wine'] if windows else []) + [str(binary), 'item-ev-reduction', reduction_arg],
                                        cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if (result.returncode or 'ITEM_REDUCTION_BAD' in result.stdout or 'ITEM_REDUCTION_PROCESSED' not in result.stdout
                    or 'item EV reductions: berries, reset, friendship and denial passed' not in result.stdout):
                    raise RuntimeError(f'{target} item EV reduction failed ({result.returncode}):\n{result.stdout}')
                result = subprocess.run((['wine'] if windows else []) + [str(binary), 'item-ev-reduction-menu', reduction_arg],
                                        cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if (result.returncode or 'ITEM_REDUCTION_BAD' in result.stdout
                    or 'item EV reduction: real party menu consumes one item' not in result.stdout):
                    raise RuntimeError(f'{target} item EV menu failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{target}: item EV policies passed')
    finally:
        for path in (p for p in build_dir.rglob('*') if p.is_file() or p.is_symlink()):
            if path not in before:
                path.unlink(missing_ok=True)
        for path in sorted((p for p in build_dir.rglob('*') if p.is_dir() and p not in before),
                           key=lambda p: len(p.parts), reverse=True):
            try:
                path.rmdir()
            except OSError:
                pass


if __name__ == '__main__':
    main()
