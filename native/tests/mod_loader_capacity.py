#!/usr/bin/env python3
"""Load 40 separately built modules and clean up all temporary files."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux64', 'windows64'], default='linux64')
    windows = parser.parse_args().target == 'windows64'
    compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
    with tempfile.TemporaryDirectory(prefix='rhh-mod-loader-') as directory:
        work = Path(directory)
        flags = ['-m64', '-std=gnu17', '-O2', '-iquote', 'include', '-Inative/include']
        modules = []
        for index in range(40):
            module = work / (f'probe{index:03}' + ('.dll' if windows else '.so'))
            subprocess.run([compiler, *flags, '-shared', *([] if windows else ['-fPIC']),
                            f'-DTEST_INDEX={index}', 'native/tests/mod_capacity_probe.c', '-o', str(module)],
                           cwd=ROOT, check=True)
            modules.append(module)
        binary = work / ('loader.exe' if windows else 'loader')
        sdl = ROOT / 'native/deps/SDL2/x86_64-w64-mingw32'
        sdl_flags = [f'-I{sdl / "include"}', f'-L{sdl / "lib"}'] if windows else []
        subprocess.run([compiler, *flags, *sdl_flags, '-DSDL_MAIN_HANDLED', '-ffunction-sections', '-fdata-sections',
                        'native/mod_host.c', 'native/mod_loader.c', 'native/tests/mod_loader_capacity.c',
                        '-Wl,--gc-sections', '-lSDL2', '-o', str(binary)], cwd=ROOT, check=True)
        env = dict(os.environ, WINEDEBUG='-all')
        if windows:
            shutil.copyfile(sdl / 'bin/SDL2.dll', work / 'SDL2.dll')
            env['WINEPREFIX'] = str(work / 'wine')
        def arg(path):
            return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
        try:
            subprocess.run((['wine'] if windows else []) + [str(binary), *(arg(p) for p in modules)],
                           cwd=ROOT, env=env, check=True, timeout=120)
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == '__main__':
    main()
