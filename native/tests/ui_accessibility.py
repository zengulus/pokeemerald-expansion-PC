#!/usr/bin/env python3
"""Exercise semantic modal text with independent producer and adapter modules."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=('linux64', 'windows64'), default='linux64')
    target = parser.parse_args().target
    windows = target == 'windows64'
    with tempfile.TemporaryDirectory(prefix='rhh-ui-accessibility-') as directory:
        work = Path(directory)
        build = work / 'build'
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
        if windows:
            env['WINEPREFIX'] = str(work / 'wine')
        try:
            binary = build / ('gameplay-tests.exe' if windows else 'gameplay-tests')
            targets = ['gameplay-tests'] + ([str(build / 'SDL2.dll')] if windows else [])
            with (work / 'build.log').open('w') as log:
                result = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', f'OBJ_DIR={build}',
                                         '-j8', *targets], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
            if result.returncode:
                raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            suffix = '.dll' if windows else '.so'
            modules = []
            for name in ('producer', 'adapter'):
                module = work / (name + suffix)
                subprocess.run([compiler, *flags, f'native/tests/ui_accessibility_{name}.c', '-o', str(module)], cwd=ROOT, check=True)
                modules.append('Z:' + str(module).replace('/', '\\') if windows else str(module))
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'ui-accessibility', *modules],
                                    cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            if (result.returncode or 'ui accessibility: copied text' not in result.stdout or 'A11Y_PRODUCER_BAD' in result.stdout
                or 'A11Y_ADAPTER_BAD' in result.stdout or result.stdout.count('A11Y_ADAPTER_CONTENT') != 3
                or result.stdout.count('A11Y_ADAPTER_CLOSED') != 1 or result.stdout.count('A11Y_OVERLAY_CONTENT') != 4
                or result.stdout.count('A11Y_OVERLAY_CLOSED') != 2
                or result.stdout.count('A11Y_MODAL_ARCHIVE_OK') != 2
                or result.stdout.count('A11Y_OVERLAY_ARCHIVE_OK') != 1
                or result.stdout.count('A11Y_SPOOF_ARCHIVE_BLOCKED') != 2):
                raise RuntimeError(f'{target} accessibility test failed ({result.returncode}):\n{result.stdout}')
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f'{target}: modal and overlay accessibility bridges passed; temporary files removed')

if __name__ == '__main__':
    main()
