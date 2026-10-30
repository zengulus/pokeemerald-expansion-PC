#!/usr/bin/env python3
"""Exercise paid Pokémon edits and clean test builds on every exit."""
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
    build_dir = ROOT / 'build/native' / args.target
    binary = build_dir / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    before = set(build_dir.rglob('*'))
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-edit-cost-probe-') as directory:
            module = Path(directory) / ('probe.dll' if windows else 'probe.so')
            if windows:
                env['WINEPREFIX'] = str(Path(directory) / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/mon_edit_cost_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            legacy = Path(directory) / ('legacy.dll' if windows else 'legacy.so')
            subprocess.run([compiler, *flags, 'native/tests/mon_edit_v1_probe.c', '-o', str(legacy)], cwd=ROOT, check=True)
            recipe = Path(directory) / ('recipe.dll' if windows else 'recipe.so')
            subprocess.run([compiler, *flags, 'native/tests/mon_edit_recipe_probe.c', '-o', str(recipe)], cwd=ROOT, check=True)
            module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            legacy_arg = 'Z:' + str(legacy).replace('/', '\\') if windows else str(legacy)
            recipe_arg = 'Z:' + str(recipe).replace('/', '\\') if windows else str(recipe)
            prefix = (['wine'] if windows else []) + [str(binary)]
            try:
                for mode, marker in (('mon-edit-paid', 'paid edit: normal bag'),
                                     ('mon-edit-paid-pyramid', 'paid edit: Pyramid bag'),
                                     ('mon-edit-paid-secret-base', 'paid edit: secret-base bag')):
                    result = subprocess.run([*prefix, mode, module_arg], cwd=ROOT, env=env, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    if result.returncode or 'EDIT_COST_PROBE_BAD' in result.stdout or any(
                        m not in result.stdout for m in ('EDIT_COST_PROBE_CONSTRAINT', 'EDIT_COST_PROBE_COMMITTED',
                                                         'EDIT_COST_PROBE_UNPAID', marker)):
                        raise RuntimeError(f'{args.target} {mode} failed ({result.returncode}):\n{result.stdout}')
                result = subprocess.run([*prefix, 'mon-edit-legacy', legacy_arg], cwd=ROOT, env=env, text=True,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if result.returncode or 'EDIT_V1_PROBE_BAD' in result.stdout or any(m not in result.stdout for m in
                    ('EDIT_V1_PROBE_CONSTRAINT', 'EDIT_V1_PROBE_COMMITTED', 'mon edit service: atomic previews')):
                    raise RuntimeError(f'{args.target} legacy edit failed ({result.returncode}):\n{result.stdout}')
                for mode, marker in (('mon-edit-recipe', 'recipe edit: multiple pockets'),
                                     ('mon-edit-recipe-pyramid', 'recipe edit: Pyramid bag'),
                                     ('mon-edit-recipe-secret-base', 'recipe edit: secret-base')):
                    result = subprocess.run([*prefix, mode, recipe_arg], cwd=ROOT, env=env, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    if result.returncode or 'EDIT_RECIPE_PROBE_BAD' in result.stdout or any(m not in result.stdout for m in
                        ('EDIT_RECIPE_PROBE_CONSTRAINT', 'EDIT_RECIPE_PROBE_COMMITTED', 'EDIT_RECIPE_PROBE_UNPAID', marker)):
                        raise RuntimeError(f'{args.target} {mode} failed ({result.returncode}):\n{result.stdout}')
                save = Path(directory) / 'paid-edit.sav'
                save_arg = 'Z:' + str(save).replace('/', '\\') if windows else str(save)
                for mode, marker in (('mon-edit-paid-save', 'paid edit: saved Pokémon and payment'),
                                     ('mon-edit-paid-load', 'paid edit: fresh-process Pokémon and payment restored'),
                                     ('mon-edit-paid-load', 'paid edit: fresh-process Pokémon and payment restored')):
                    result = subprocess.run([*prefix, mode, save_arg, module_arg], cwd=ROOT, env=env, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    if result.returncode or 'EDIT_COST_PROBE_BAD' in result.stdout or marker not in result.stdout:
                        raise RuntimeError(f'{args.target} {mode} failed ({result.returncode}):\n{result.stdout}')
                recipe_save = Path(directory) / 'recipe-edit.sav'
                recipe_save_arg = 'Z:' + str(recipe_save).replace('/', '\\') if windows else str(recipe_save)
                for mode, marker in (('mon-edit-recipe-save', 'recipe edit: saved Pokémon and payment'),
                                     ('mon-edit-recipe-load', 'recipe edit: fresh-process Pokémon and payment restored'),
                                     ('mon-edit-recipe-load', 'recipe edit: fresh-process Pokémon and payment restored')):
                    result = subprocess.run([*prefix, mode, recipe_save_arg, recipe_arg], cwd=ROOT, env=env, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    if result.returncode or 'EDIT_RECIPE_PROBE_BAD' in result.stdout or marker not in result.stdout:
                        raise RuntimeError(f'{args.target} {mode} failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print(f'{args.target}: paid edits, recipes, both bags, constraints and v1.0/v1.1/v1.2 modules passed')
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
