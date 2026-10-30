#!/usr/bin/env python3
"""Exercise battle render pass composition in a disposable native build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix='rhh-battle-passes-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64',
                                     f'OBJ_DIR={build}', '-j8', 'gameplay-tests'], cwd=ROOT,
                                    stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        modules = []
        for index in range(2):
            module = work / f'probe{index}.so'
            subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-iquote', 'include', f'-DPASS_B={index}', 'native/tests/battle_pass_probe.c',
                            '-o', str(module)], cwd=ROOT, check=True)
            modules.append(str(module))
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        command = [str(build / 'gameplay-tests'), 'battle-passes', *modules, '0']
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if result.returncode or 'BATTLE_PASS_BAD' in result.stdout or 'battle passes: ordering, conflicts and purity passed' not in result.stdout:
            raise RuntimeError(f'battle pass collection failed ({result.returncode}):\n{result.stdout}')
        env['SDL_VIDEODRIVER'] = 'offscreen'
        command = [str(build / 'gameplay-tests'), 'battle-passes', *modules, '1']
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if result.returncode or 'BATTLE_PASS_BAD' in result.stdout or 'battle passes: GL ordering, uniforms, battler mask and protected UI passed' not in result.stdout:
            raise RuntimeError(f'battle pass GL failed ({result.returncode}):\n{result.stdout}')
    print('linux64: battle shader composition passed; temporary files removed')


if __name__ == '__main__':
    main()
