#!/usr/bin/env python3
"""Exercise wild-actor notifications and battle identity through an external mod."""
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
    with tempfile.TemporaryDirectory(prefix='rhh-actor-probe-') as directory:
        module = Path(directory) / ('actor-probe.dll' if windows else 'actor-probe.so')
        flags = [f'-m{bits}', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
        if not windows:
            flags.append('-fPIC')
        subprocess.run([compiler, *flags, 'native/tests/wild_actor_probe.c', '-o', str(module)], cwd=ROOT, check=True)
        module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
        command = (['wine'] if windows else []) + [str(binary), 'wild-actor-external', module_arg]
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
        result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        markers = ('WILD_ACTOR_GENERATED_SPAWN', 'WILD_ACTOR_BATTLE_IDENTITY', 'WILD_ACTOR_BATTLE_CLEAR',
                   'WILD_ACTOR_MANUAL_SPAWN', 'WILD_ACTOR_MANUAL_REMOVE')
        if result.returncode or 'WILD_ACTOR_PROBE_BAD' in result.stdout or any(marker not in result.stdout for marker in markers):
            raise RuntimeError(f'Wild actor probe failed ({result.returncode}):\n{result.stdout}')
        print(f'{args.target}: external wild-actor lifecycle and battle identity passed')
        command = (['wine'] if windows else []) + [str(binary), 'wild-actor-id-failure']
        result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f'Identity persistence failure check failed ({result.returncode}):\n{result.stdout}')
        individual_module = Path(directory) / ('individual-probe.dll' if windows else 'individual-probe.so')
        subprocess.run([compiler, *flags, 'native/tests/wild_actor_individual_probe.c', '-o', str(individual_module)],
                       cwd=ROOT, check=True)
        save = Path(directory) / 'actor.sav'
        expected = Path(directory) / 'expected.bin'
        def path_arg(path):
            return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
        for phase in ('wild-actor-save', 'wild-actor-load', 'wild-actor-load'):
            command = (['wine'] if windows else []) + [str(binary), phase, path_arg(save), path_arg(expected),
                                                       path_arg(individual_module)]
            result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            markers = ('WILD_INDIVIDUAL_ASSIGNED',) if phase == 'wild-actor-save' else (
                'WILD_INDIVIDUAL_RESTORED', 'WILD_INDIVIDUAL_ASSIGNED', 'WILD_INDIVIDUAL_ENGAGED',
                'WILD_INDIVIDUAL_REMOVED', 'WILD_INDIVIDUAL_OUTCOME')
            if result.returncode or 'WILD_INDIVIDUAL_BAD' in result.stdout or any(m not in result.stdout for m in markers):
                raise RuntimeError(f'{phase} failed ({result.returncode}):\n{result.stdout}')
        print(f'{args.target}: fresh-process individual identity and rollback-safe allocation passed')

if __name__ == '__main__':
    main()
