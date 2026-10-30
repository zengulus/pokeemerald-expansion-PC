#!/usr/bin/env python3
"""Exercise per-individual traits through a separately compiled module."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux32', 'linux64', 'windows32', 'windows64'], default='linux64')
    args = parser.parse_args()
    windows = args.target.startswith('windows')
    bits = '64' if args.target.endswith('64') else '32'
    binary = ROOT / 'build/native' / args.target / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-wild-traits-') as directory:
            work = Path(directory)
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            module = work / ('traits.dll' if windows else 'traits.so')
            compiler = ('x86_64-w64-mingw32-gcc' if bits == '64' else 'i686-w64-mingw32-gcc') if windows else 'gcc'
            flags = [f'-m{bits}', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/wild_traits_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            def arg(path):
                return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
            try:
                for phase in ('wild-actor-save', 'wild-actor-load', 'wild-actor-load'):
                    cmd = (['wine'] if windows else []) + [str(binary), phase, arg(work / 'save.sav'),
                                                           arg(work / 'expected.bin'), arg(module)]
                    result = subprocess.run(cmd, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    required = ('WILD_TRAITS_SESSION', 'WILD_TRAITS_IDENTIFIED') if phase == 'wild-actor-save' else ('WILD_TRAITS_RESTORED', 'WILD_TRAITS_IDENTIFIED')
                    if result.returncode or 'WILD_TRAITS_BAD' in result.stdout or any(m not in result.stdout for m in required):
                        raise RuntimeError(f'{args.target} {phase} failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{args.target}: composable individual traits, identity and restore passed')
    finally:
        subprocess.run(['make', '-s', '-f', 'Makefile.native', f'TARGET={args.target}', 'clean-test-builds'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

if __name__ == '__main__':
    main()
