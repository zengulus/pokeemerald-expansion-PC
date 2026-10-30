#!/usr/bin/env python3
"""Check the player-facing type API with indicators disabled by default."""
import os
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--seen', action='store_true', help='Test the seen-only indicator policy')
    args = parser.parse_args()
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with tempfile.TemporaryDirectory(prefix='rhh-public-types-') as directory:
        work = Path(directory)
        build = work / 'build'
        make_args = ['make', '-f', 'Makefile.native', 'TARGET=linux64', f'OBJ_DIR={build}', '-j8', 'gameplay-tests']
        if args.seen:
            overlay = work / 'include'
            overlay.mkdir()
            for entry in (ROOT / 'include').iterdir():
                if entry.name != 'config':
                    (overlay / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
            config = overlay / 'config'
            config.mkdir()
            for entry in (ROOT / 'include/config').iterdir():
                if entry.name != 'battle.h':
                    (config / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
            source = (ROOT / 'include/config/battle.h').read_text()
            old = '#define B_SHOW_TYPES        SHOW_TYPES_NEVER'
            if source.count(old) != 1:
                raise RuntimeError('Could not prepare the seen-only indicator configuration')
            (config / 'battle.h').write_text(source.replace(old, old.replace('SHOW_TYPES_NEVER', 'SHOW_TYPES_SEEN')))
            sdl = subprocess.check_output(['sdl2-config', '--cflags'], text=True).strip()
            flags = (f'-iquote {overlay} -iquote include -Inative/include -DMODERN=1 -DNATIVE -DPORTABLE '
                     f'-DRENDERER_EASY_DRAW -DTESTING=0 -DEMERALD -DNDEBUG -std=gnu17 {sdl} -I{build}')
            make_args.insert(-1, f'CPPFLAGS={flags}')
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(make_args,
                                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        if args.seen:
            command = ['public-battler-types-seen']
            marker = 'public battler types: unseen and seen indicator policy passed'
        else:
            module = work / 'types.so'
            subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-iquote', 'include', 'native/tests/type_chart_probe.c', '-o', str(module)],
                           cwd=ROOT, check=True)
            command = ['type-chart', str(module)]
            marker = 'type chart: stable keys'
        result = subprocess.run([str(build / 'gameplay-tests'), *command], cwd=ROOT,
                                env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
        if result.returncode or marker not in result.stdout or 'TYPE_PROBE_BAD' in result.stdout:
            raise RuntimeError(f'Type view failed ({result.returncode}):\n{result.stdout}')
        observation = work / 'public-observation.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/public_type_observation_probe.c', '-o', str(observation)],
                       cwd=ROOT, check=True)
        result = subprocess.run([str(build / 'gameplay-tests'), 'public-type-observation', str(observation),
                                 '1' if args.seen else '0'], cwd=ROOT, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
        if (result.returncode or 'PUBLIC_TYPE_OBSERVATION_BAD' in result.stdout
            or 'public type observation: visibility, illusion, switches and RNG passed' not in result.stdout):
            raise RuntimeError(f'Public type observation failed ({result.returncode}):\n{result.stdout}')
    print(f'linux64: {"seen-only" if args.seen else "disabled"} indicator type view passed; temporary files removed')


if __name__ == '__main__':
    main()
