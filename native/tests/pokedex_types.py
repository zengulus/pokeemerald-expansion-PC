#!/usr/bin/env python3
"""Exercise HGSS Pokédex type icons in an isolated, self-cleaning build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix='rhh-pokedex-types-') as directory:
        work = Path(directory)
        overlay = work / 'include'
        overlay.mkdir()
        for entry in (ROOT / 'include').iterdir():
            if entry.name != 'config':
                (overlay / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
        config = overlay / 'config'
        config.mkdir()
        for entry in (ROOT / 'include/config').iterdir():
            if entry.name != 'pokedex_plus_hgss.h':
                (config / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
        source = (ROOT / 'include/config/pokedex_plus_hgss.h').read_text()
        before = '#define POKEDEX_PLUS_HGSS                   FALSE'
        if source.count(before) != 1:
            raise RuntimeError('Could not prepare the HGSS Pokédex configuration')
        (config / 'pokedex_plus_hgss.h').write_text(source.replace(before, before.replace('FALSE', 'TRUE')))
        build = work / 'build'
        sdl = subprocess.check_output(['sdl2-config', '--cflags'], text=True).strip()
        flags = (f'-iquote {overlay} -iquote include -Inative/include -DMODERN=1 -DNATIVE -DPORTABLE '
                 '-DRENDERER_EASY_DRAW -DTESTING=0 -DEMERALD -DNDEBUG -DRHH_POKEDEX_TYPE_TEST_FIXTURE '
                 f'-std=gnu17 {sdl} -I{build}')
        binary = build / 'gameplay-tests'
        result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', f'OBJ_DIR={build}',
                                 f'CPPFLAGS={flags}', '-j8', str(binary)], cwd=ROOT,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join(result.stdout.splitlines()[-60:]))
        module = work / 'types.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/species_types_probe.c', '-o', str(module)],
                       cwd=ROOT, check=True)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        result = subprocess.run([str(binary), 'pokedex-types', str(module)], cwd=ROOT, env=env,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode or 'SPECIES_TYPES_BAD' in result.stdout or 'pokedex types: icons' not in result.stdout:
            raise RuntimeError(f'Pokédex type integration failed ({result.returncode}):\n{result.stdout}')
        print('linux64: HGSS Pokédex type icons, transitions and exhaustion passed')


if __name__ == '__main__':
    main()
