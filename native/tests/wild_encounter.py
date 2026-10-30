#!/usr/bin/env python3
"""Exercise habitat-aware wild generation with an independently loaded mod."""
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
    compiler = ('x86_64-w64-mingw32-gcc' if bits == '64' else 'i686-w64-mingw32-gcc') if windows else 'gcc'
    binary = ROOT / 'build/native' / args.target / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    if not binary.exists():
        raise RuntimeError(f'Build {binary} first')
    with tempfile.TemporaryDirectory(prefix='rhh-wild-probe-') as directory:
        module = Path(directory) / ('wild-probe.dll' if windows else 'wild-probe.so')
        flags = [f'-m{bits}', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
        if not windows:
            flags.append('-fPIC')
        subprocess.run([compiler, *flags, 'native/tests/wild_encounter_probe.c', '-o', str(module)], cwd=ROOT, check=True)
        module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
        command = (['wine'] if windows else []) + [str(binary), 'wild-encounter-external', module_arg]
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
        result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode or 'WILD_PROBE_BAD' in result.stdout or any(
                marker not in result.stdout for marker in ('WILD_PROBE_WEIGHT', 'WILD_PROBE_PROPOSED', 'WILD_PROBE_GENERATED',
                    'WILD_PROBE_FISH_PROPOSED', 'WILD_PROBE_FISH_WEIGHT_1', 'WILD_PROBE_FISH_GENERATED_1',
                    'WILD_PROBE_FISH_WEIGHT_2', 'WILD_PROBE_FISH_GENERATED_2',
                    'WILD_PROBE_FISH_WEIGHT_3', 'WILD_PROBE_FISH_GENERATED_3')):
            raise RuntimeError(f'Wild encounter probe failed ({result.returncode}):\n{result.stdout}')
        print(f'{args.target}: external habitat-aware wild generation passed')

if __name__ == '__main__':
    main()
