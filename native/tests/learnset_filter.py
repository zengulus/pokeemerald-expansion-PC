#!/usr/bin/env python3
"""Exercise learnset composition and clean all newly created test artifacts."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux32', 'linux64', 'windows64'], default='linux64')
    target = parser.parse_args().target
    windows, bits = target.startswith('windows'), ('64' if target.endswith('64') else '32')
    build_dir = ROOT / 'build/native' / target
    existing = {path for path in build_dir.rglob('*') if path.is_file() or path.is_symlink()}
    binary = build_dir / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-learnset-filter-') as directory:
            work = Path(directory)
            if windows: env['WINEPREFIX'] = str(work / 'wine')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = [f'-m{bits}', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows: flags.append('-fPIC')
            modules = []
            for name in ('learnset_filter_probe', 'learnset_probe'):
                module = work / (name + ('.dll' if windows else '.so'))
                subprocess.run([compiler, *flags, f'native/tests/{name}.c', '-o', str(module)], cwd=ROOT, check=True)
                modules.append(module)
            def arg(path):
                return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
            try:
                tests = [
                    ('learnset-filter', [arg(modules[0])], 'learnset filter: composition, ordering, consumers and link exclusions passed'),
                    ('learnset-filter-addon', [arg(modules[0]), arg(modules[1])], 'learnset filter: independent registered move composition passed'),
                    ('mod-learnsets-save', [arg(work / 'save.sav'), arg(modules[1])], 'mod learnsets: creation, level-up, breeding and keyed queries passed'),
                    ('mod-learnsets-load', [arg(work / 'save.sav'), arg(modules[1])], 'mod learnsets: save and slot remapping passed'),
                    ('mod-learnsets-missing', [arg(work / 'save.sav')], 'mod learnsets: missing module preserves learned key'),
                ]
                for mode, arguments, marker in tests:
                    result = subprocess.run((['wine'] if windows else []) + [str(binary), mode, *arguments],
                                            cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                    if result.returncode or 'LEARNSET_FILTER_BAD' in result.stdout or marker not in result.stdout:
                        raise RuntimeError(f'{target} {mode} failed ({result.returncode}):\n{result.stdout}')
            finally:
                if windows: subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{target}: learnset composition and existing learning/save paths passed')
    finally:
        try:
            subprocess.run(['make', '-s', '-f', 'Makefile.native', f'TARGET={target}', 'clean-test-builds'],
                           cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        finally:
            for path in build_dir.rglob('*'):
                if (path.is_file() or path.is_symlink()) and path not in existing:
                    path.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
