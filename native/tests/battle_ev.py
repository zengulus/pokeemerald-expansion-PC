#!/usr/bin/env python3
"""Exercise battle EV policies in the native engine and clean test builds."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux64', 'windows64'], default='linux64')
    target = parser.parse_args().target
    windows = target == 'windows64'
    build_dir = ROOT / 'build/native' / target
    binary = build_dir / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    before = set(build_dir.rglob('*'))
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-battle-ev-') as directory:
            work = Path(directory)
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            module = work / ('probe.dll' if windows else 'probe.so')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/battle_ev_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            bank_module = work / ('bank.dll' if windows else 'bank.so')
            subprocess.run([compiler, *flags, 'native/tests/battle_ev_bank_probe.c', '-o', str(bank_module)], cwd=ROOT, check=True)
            bank_arg = 'Z:' + str(bank_module).replace('/', '\\') if windows else str(bank_module)
            try:
                save = work / 'battle-ev.sav'
                save_arg = 'Z:' + str(save).replace('/', '\\') if windows else str(save)
                modes = [('battle-ev-yield', [module_arg], 'battle EV yield: caps, held items, denial, identity and state passed'),
                         ('battle-ev-bank', [bank_arg], 'battle EV bank: accepted and denied yields are distinct'),
                         ('battle-ev-save', [save_arg, module_arg], 'battle EV yield: saved allocation and policy state'),
                         ('battle-ev-load', [save_arg, module_arg], 'battle EV yield: fresh-process allocation and policy state restored')]
                for mode, args, marker in modes:
                    result = subprocess.run((['wine'] if windows else []) + [str(binary), mode, *args],
                                            cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    if (result.returncode or 'BATTLE_EV_BAD' in result.stdout or 'BATTLE_BANK_BAD' in result.stdout or marker not in result.stdout
                        or (mode in ('battle-ev-yield', 'battle-ev-save') and ('BATTLE_EV_CONSTRAINT' not in result.stdout
                                                         or 'BATTLE_EV_PROCESSED' not in result.stdout))):
                        raise RuntimeError(f'{target} {mode} failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{target}: battle EV yield policies passed')
    finally:
        for path in (p for p in build_dir.rglob('*') if p.is_file() or p.is_symlink()):
            if path not in before:
                path.unlink(missing_ok=True)
        for path in sorted((p for p in build_dir.rglob('*') if p.is_dir() and p not in before),
                           key=lambda p: len(p.parts), reverse=True):
            try:
                path.rmdir()
            except OSError:
                pass


if __name__ == '__main__':
    main()
