#!/usr/bin/env python3
"""Test trainer-pool provenance with an external randomizer; remove test builds."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux32', 'linux64', 'windows32', 'windows64'], default='linux64')
    target = parser.parse_args().target
    windows = target.startswith('windows')
    bits = '64' if target.endswith('64') else '32'
    binary = ROOT / 'build/native' / target / ('trainer-source-tests.exe' if windows else 'trainer-source-tests')
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', '-j8', 'trainer-source-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-trainer-source-') as directory:
            if windows:
                env['WINEPREFIX'] = str(Path(directory) / 'wine')
            compiler = ('x86_64-w64-mingw32-gcc' if bits == '64' else 'i686-w64-mingw32-gcc') if windows else 'gcc'
            flags = [f'-m{bits}', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            try:
                for mode, probe, markers in [
                    ('trainer-source', 'trainer_source_probe.c', ('TRAINER_SOURCE_PROBE_OK', 'TRAINER_ROSTER_PROBE_OK', 'TRAINER_SOURCE_PROBE_CLEARED', 'trainer source: real generation')),
                    ('trainer-pool-source', 'trainer_pool_probe.c', ('TRAINER_POOL_PROBE_OK', 'TRAINER_POOL_MULTI_OK', 'trainer pool source: selected lead and ace')),
                ]:
                    module = Path(directory) / (mode + ('.dll' if windows else '.so'))
                    subprocess.run([compiler, *flags, 'native/tests/' + probe, '-o', str(module)], cwd=ROOT, check=True)
                    module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
                    result = subprocess.run((['wine'] if windows else []) + [str(binary), mode, module_arg],
                                            cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                    if (result.returncode or 'TRAINER_SOURCE_PROBE_BAD' in result.stdout or 'TRAINER_POOL_PROBE_BAD' in result.stdout
                            or any(marker not in result.stdout for marker in markers)):
                        raise RuntimeError(f'{target} {mode} failed ({result.returncode}):\n{result.stdout}')
                module = Path(directory) / ('boss-ace.dll' if windows else 'boss-ace.so')
                subprocess.run([compiler, *flags, 'native/tests/boss_ace_probe.c', '-o', str(module)], cwd=ROOT, check=True)
                save = Path(directory) / 'boss-ace.sav'
                def arg(path):
                    return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
                for phase in ('boss-ace-save', 'boss-ace-load'):
                    result = subprocess.run((['wine'] if windows else []) + [str(binary), phase, arg(save), arg(module)],
                                            cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                    marker = 'boss ace: preview, constraints' if phase.endswith('save') else 'boss ace: fresh-process restore'
                    if result.returncode or 'BOSS_ACE_BAD' in result.stdout or marker not in result.stdout:
                        raise RuntimeError(f'{target} {phase} failed ({result.returncode}):\n{result.stdout}')
                    if phase.endswith('save') and any(result.stdout.count(item) != count for item, count in
                            [('BOSS_ACE_PREVIEW', 1), ('BOSS_ACE_DENIED', 1), ('BOSS_ACE_CHANGED', 2),
                             ('BOSS_ACE_BATTLE_MATCHED', 1)]):
                        raise RuntimeError('Incorrect boss-ace notifications:\n' + result.stdout)
                    if phase.endswith('load') and result.stdout.count('BOSS_ACE_CHANGED') != 2:
                        raise RuntimeError('Incorrect orphan or removed-trainer notifications:\n' + result.stdout)
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{target}: trainer provenance, pool selection and saved boss-ace configuration passed')
    finally:
        subprocess.run(['make', '-s', '-f', 'Makefile.native', f'TARGET={target}', 'clean-test-builds'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == '__main__':
    main()
