#!/usr/bin/env python3
"""Test facility encounter hooks and remove all test builds afterward."""
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
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-wild-facility-probe-') as directory:
            module = Path(directory) / ('probe.dll' if windows else 'probe.so')
            if windows:
                env['WINEPREFIX'] = str(Path(directory) / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/wild_facility_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            prefix = (['wine'] if windows else []) + [str(binary)]
            try:
                for name in ('pike', 'pyramid', 'pyramid-open'):
                    result = subprocess.run([*prefix, 'wild-facility-' + name, module_arg], cwd=ROOT, env=env, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    markers = ('FACILITY_PROBE_CANCELLED', 'FACILITY_PROBE_GENERATED',
                               f'facility wild: {name.split("-")[0]} veto')
                    if result.returncode or 'FACILITY_PROBE_BAD' in result.stdout or any(m not in result.stdout for m in markers):
                        raise RuntimeError(f'{args.target} {name} failed ({result.returncode}):\n{result.stdout}')
                for name in ('pike', 'pyramid'):
                    result = subprocess.run([*prefix, 'wild-facility-manual-' + name, module_arg], cwd=ROOT, env=env,
                                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    markers = ('FACILITY_PROBE_CANCELLED', 'FACILITY_PROBE_GENERATED', f'manual facility: {name} veto')
                    if result.returncode or 'FACILITY_PROBE_BAD' in result.stdout or any(m not in result.stdout for m in markers):
                        raise RuntimeError(f'{args.target} manual {name} failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print(f'{args.target}: facility encounters, rollback, battle notification and external mod passed')
    finally:
        subprocess.run(['make', '-s', '-f', 'Makefile.native', f'TARGET={args.target}', 'clean-test-builds'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == '__main__':
    main()
