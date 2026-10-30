#!/usr/bin/env python3
"""Test the randomized Pyramid and visible actors in a self-cleaning build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix='rhh-random-pyramid-test-') as directory:
        work = Path(directory)
        overlay = work / 'include'
        overlay.mkdir()
        for entry in (ROOT / 'include').iterdir():
            if entry.name != 'config':
                (overlay / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
        config = overlay / 'config'
        config.mkdir()
        for entry in (ROOT / 'include/config').iterdir():
            if entry.name != 'overworld.h':
                (config / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
        source = (ROOT / 'include/config/overworld.h').read_text()
        before = '#define BATTLE_PYRAMID_RANDOM_ENCOUNTERS    FALSE'
        if source.count(before) != 1:
            raise RuntimeError('Could not prepare randomized Pyramid configuration')
        (config / 'overworld.h').write_text(source.replace(before, '#define BATTLE_PYRAMID_RANDOM_ENCOUNTERS    TRUE'))
        build = work / 'build'
        sdl = subprocess.check_output(['sdl2-config', '--cflags'], text=True).strip()
        flags = (f'-iquote {overlay} -iquote include -Inative/include -DMODERN=1 -DNATIVE -DPORTABLE '
                 '-DRENDERER_EASY_DRAW -DTESTING=0 -DEMERALD -DNDEBUG -DWE_OW_ENCOUNTERS=TRUE -std=gnu17 '
                 f'{sdl} -I{build}')
        binary = build / 'gameplay-tests'
        result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', f'OBJ_DIR={build}',
                                 f'CPPFLAGS={flags}', '-j8', str(binary)], cwd=ROOT, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join(result.stdout.splitlines()[-60:]))
        module = work / 'probe.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote',
                        'include', 'native/tests/wild_facility_probe.c', '-o', str(module)], cwd=ROOT, check=True)
        type_module = work / 'types.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote',
                        'include', 'native/tests/species_types_probe.c', '-o', str(type_module)], cwd=ROOT, check=True)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        for mode, marker in (('wild-facility-pyramid', 'facility wild: pyramid veto'),
                             ('wild-facility-pyramid-open', 'facility wild: pyramid veto'),
                             ('wild-facility-owe-pyramid', 'facility wild: overworld pyramid veto')):
            result = subprocess.run([str(binary), mode, str(module)], cwd=ROOT, env=env, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
            if result.returncode or 'FACILITY_PROBE_BAD' in result.stdout or any(
                m not in result.stdout for m in ('FACILITY_PROBE_CANCELLED', 'FACILITY_PROBE_GENERATED', marker)):
                raise RuntimeError(f'{mode} failed ({result.returncode}):\n{result.stdout}')
        result = subprocess.run([str(binary), 'species-types', str(type_module)], cwd=ROOT, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
        if (result.returncode or 'SPECIES_TYPES_BAD' in result.stdout
                or 'species types: composition' not in result.stdout
                or 'species types: randomized Pyramid requirement passed' not in result.stdout):
            raise RuntimeError(f'Pyramid species types failed ({result.returncode}):\n{result.stdout}')
        print('linux64: randomized Pyramid encounters and visible actors passed')


if __name__ == '__main__':
    main()
