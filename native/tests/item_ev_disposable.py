#!/usr/bin/env python3
"""Exercise field EV transactions and their real party callbacks in a disposable build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with tempfile.TemporaryDirectory(prefix='rhh-item-ev-build-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64',
                                     f'OBJ_DIR={build}', '-j8', 'gameplay-tests'],
                                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        modules = {}
        for name in ('item_ev_probe', 'ev_limits_probe', 'item_ev_reduction_probe', 'battle_ev_probe', 'battle_ev_bank_probe'):
            modules[name] = work / f'{name}.so'
            subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-iquote', 'include', f'native/tests/{name}.c', '-o', str(modules[name])], cwd=ROOT, check=True)
        cases = [(['item-ev-yield', modules['item_ev_probe'], modules['ev_limits_probe']], 'item EV yield: applied'),
                 (['item-ev-reduction', modules['item_ev_reduction_probe']], 'item EV reductions: berries'),
                 (['item-ev-reduction-menu', modules['item_ev_reduction_probe']], 'item EV reduction: real party menu'),
                 (['ev-limits', modules['ev_limits_probe']], 'EV limits: editing'),
                 (['battle-ev-yield', modules['battle_ev_probe']], 'battle EV yield: caps'),
                 (['battle-ev-bank', modules['battle_ev_bank_probe']], 'battle EV bank: accepted'),
                 (['ev-purity'], 'EV purity: limits')]
        for arguments, marker in cases:
            result = subprocess.run([str(build / 'gameplay-tests'), *map(str, arguments)], cwd=ROOT, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
            if result.returncode or marker not in result.stdout or any(bad in result.stdout for bad in
                    ('ITEM_EV_BAD', 'EV_LIMIT_BAD', 'ITEM_REDUCTION_BAD', 'BATTLE_EV_BAD', 'BATTLE_BANK_BAD')):
                raise RuntimeError(f'{arguments[0]} failed ({result.returncode}):\n{result.stdout}')
    print('linux64: item EV transactions and party callbacks passed; temporary files removed')


if __name__ == '__main__':
    main()
