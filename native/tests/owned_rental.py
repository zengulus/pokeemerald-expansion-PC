#!/usr/bin/env python3
"""Exercise rental and related party projections with disposable builds and saves."""
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
    binary = build_dir / ('owned-identity-tests.exe' if windows else 'owned-identity-tests')
    before = set(build_dir.rglob('*'))
    try:
        result = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', '-j8', 'owned-identity-tests'],
                                cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if result.returncode:
            raise RuntimeError('\n'.join(result.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-owned-rental-') as directory:
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
            if windows:
                env['WINEPREFIX'] = str(Path(directory) / 'wine')
            try:
                for kind in ('rental', 'frontier', 'union', 'sky'):
                    save = Path(directory) / (kind + '.sav')
                    save_arg = 'Z:' + str(save).replace('/', '\\') if windows else str(save)
                    result = subprocess.run((['wine'] if windows else []) + [str(binary), f'owned-{kind}-ledger', save_arg],
                                            cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    if result.returncode or f'owned {kind} ledger:' not in result.stdout:
                        raise RuntimeError(f'{args.target} {kind} failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{args.target}: rental and existing party projection checks passed; test artifacts removed')
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
