#!/usr/bin/env python3
"""Check committed ability mutations and existing observations in a disposable build."""
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
    with tempfile.TemporaryDirectory(prefix='rhh-ability-mutations-') as directory:
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
            probe, observer = work / ('probe' + suffix), work / ('observer' + suffix)
            mutation_probe = work / ('mutation-probe' + suffix)
            gas_probe = work / ('gas-probe' + suffix)
            for source, output in [('native/tests/ability_probe.c', probe), ('native/tests/ability_observe_probe.c', observer),
                                   ('native/tests/ability_mutation_probe.c', mutation_probe), ('native/tests/ability_gas_probe.c', gas_probe)]:
                subprocess.run([compiler, *flags, source, '-o', str(output)], cwd=ROOT, check=True)
            def arg(path):
                return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
            for command, marker in [(['weather-presence'], 'weather presence: switch, entry and faint'),
                                    (['ability-mutations', arg(mutation_probe)], 'ability mutations: committed script changes'),
                                    (['ability-contacts', arg(mutation_probe)], 'ability contacts: committed changes'),
                                    (['ability-gas', arg(gas_probe)], 'ability gas: source transitions'),
                                    (['abilities', arg(probe), arg(observer)], 'abilities: species, owned and pure battle state passed')]:
                result = subprocess.run((['wine'] if windows else []) + [str(binary), *command], cwd=ROOT,
                                        env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                if result.returncode or marker not in result.stdout or 'ABILITY_BAD' in result.stdout or 'ABILITY_OBSERVATION_BAD' in result.stdout:
                    raise RuntimeError(f'{target} {command[0]} failed ({result.returncode}):\n{result.stdout}')
                if command[0] == 'abilities' and result.stdout.count('ABILITY_OBSERVATION_OK') != 7:
                    raise RuntimeError(f'{target} ability observation mismatch:\n{result.stdout}')
                if command[0] == 'ability-mutations' and ('ABILITY_MUTATION_EXTERNAL_BAD' in result.stdout
                    or result.stdout.count('ABILITY_MUTATION_EXTERNAL_OK') != 11):
                    raise RuntimeError(f'{target} external ability mutation mismatch:\n{result.stdout}')
                if command[0] == 'ability-contacts' and ('ABILITY_CONTACT_EXTERNAL_BAD' in result.stdout
                    or result.stdout.count('ABILITY_CONTACT_EXTERNAL_OK') != 6
                    or result.stdout.count('ABILITY_MUTATION_EXTERNAL_OK') != 1):
                    raise RuntimeError(f'{target} contact mutation mismatch:\n{result.stdout}')
                if command[0] == 'ability-gas' and ('ABILITY_GAS_EXTERNAL_BAD' in result.stdout
                    or result.stdout.count('ABILITY_GAS_EXTERNAL_OK') != 17):
                    raise RuntimeError(f'{target} gas transition mismatch:\n{result.stdout}')
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f'{target}: ability mutations and existing observations passed; temporary files removed')


if __name__ == '__main__':
    main()
