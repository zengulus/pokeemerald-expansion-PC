#!/usr/bin/env python3
"""Check content-description queries in an isolated native build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix='rhh-content-descriptions-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64',
                                     f'OBJ_DIR={build}', '-j8', 'gameplay-tests'], cwd=ROOT,
                                    stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        module = work / 'probe.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/content_description_probe.c', '-o', str(module)],
                       cwd=ROOT, check=True)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        result = subprocess.run([str(build / 'gameplay-tests'), 'content-descriptions', str(module)],
                                cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if (result.returncode or 'CONTENT_DESCRIPTION_BAD' in result.stdout
            or 'CONTENT_DESCRIPTION_EXTERNAL_OK' not in result.stdout
            or 'content descriptions: builtins, registered content and bounded copies passed' not in result.stdout):
            raise RuntimeError(f'content descriptions failed ({result.returncode}):\n{result.stdout}')
        ui = work / 'ui.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/ui_probe.c', '-o', str(ui)], cwd=ROOT, check=True)
        result = subprocess.run([str(build / 'gameplay-tests'), 'mod-ui', str(ui), '0'],
                                cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if (result.returncode or 'UI_PROBE_BAD' in result.stdout or 'mod ui: input, live redraw' not in result.stdout
            or any(marker not in result.stdout for marker in ('UI_PROBE_DRAW', 'UI_PROBE_EDIT', 'UI_PROBE_CLOSED', 'UI_PROBE_RESET'))):
            raise RuntimeError(f'content display UI regression failed ({result.returncode}):\n{result.stdout}')
    print('linux64: content descriptions passed; temporary files removed')


if __name__ == '__main__':
    main()
