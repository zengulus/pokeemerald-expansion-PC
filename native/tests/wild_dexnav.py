#!/usr/bin/env python3
"""Test real DexNav search handoff in a self-cleaning configuration build."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux64', 'windows64'], default='linux64')
    parser.add_argument('--inspect', action='store_true', help='Pause with the temporary type-display screenshot available')
    args = parser.parse_args()
    windows = args.target == 'windows64'
    with tempfile.TemporaryDirectory(prefix='rhh-dexnav-test-') as directory:
        work = Path(directory)
        overlay = work / 'include'
        overlay.mkdir()
        for entry in (ROOT / 'include').iterdir():
            if entry.name != 'config':
                (overlay / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
        config = overlay / 'config'
        config.mkdir()
        for entry in (ROOT / 'include/config').iterdir():
            if entry.name != 'dexnav.h':
                (config / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
        source = (ROOT / 'include/config/dexnav.h').read_text()
        replacements = {
            '#define DEXNAV_ENABLED                FALSE': '#define DEXNAV_ENABLED                TRUE',
            '#define DN_FLAG_SEARCHING             0': '#define DN_FLAG_SEARCHING             5',
            '#define DN_FLAG_DEXNAV_GET            0': '#define DN_FLAG_DEXNAV_GET            7',
            '#define DN_FLAG_DETECTOR_MODE         0': '#define DN_FLAG_DETECTOR_MODE         6',
            '#define DN_VAR_SPECIES                0': '#define DN_VAR_SPECIES                0x4001',
            '#define DN_VAR_STEP_COUNTER           0': '#define DN_VAR_STEP_COUNTER           0x4002',
        }
        for before, after in replacements.items():
            if source.count(before) != 1:
                raise RuntimeError('Could not prepare DexNav configuration: ' + before)
            source = source.replace(before, after)
        (config / 'dexnav.h').write_text(source)
        build = work / 'build'
        sdl = ('-Inative/deps/SDL2/x86_64-w64-mingw32/include' if windows
               else subprocess.check_output(['sdl2-config', '--cflags'], text=True).strip())
        flags = (f'-iquote {overlay} -iquote include -Inative/include -DMODERN=1 -DNATIVE -DPORTABLE '
                 '-DRENDERER_EASY_DRAW -DTESTING=0 -DEMERALD -DNDEBUG -DRHH_DEXNAV_TEST_FIXTURE '
                 f'-std=gnu17 {sdl} -I{build}')
        binary = build / ('gameplay-tests.exe' if windows else 'gameplay-tests')
        result = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={args.target}', f'OBJ_DIR={build}',
                                 f'CPPFLAGS={flags}', '-j8', str(binary)], cwd=ROOT,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join(result.stdout.splitlines()[-60:]))
        module = work / ('probe.dll' if windows else 'probe.so')
        compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
        compile_flags = ['-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
        if not windows:
            compile_flags.append('-fPIC')
        subprocess.run([compiler, *compile_flags, 'native/tests/wild_dexnav_probe.c', '-o', str(module)], cwd=ROOT, check=True)
        types_module = work / ('types.dll' if windows else 'types.so')
        subprocess.run([compiler, *compile_flags, 'native/tests/species_types_probe.c', '-o', str(types_module)], cwd=ROOT, check=True)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
        if windows:
            shutil.copyfile(ROOT / 'native/deps/SDL2/x86_64-w64-mingw32/bin/SDL2.dll', build / 'SDL2.dll')
            env['WINEPREFIX'] = str(work / 'wine')
        module_arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
        types_arg = 'Z:' + str(types_module).replace('/', '\\') if windows else str(types_module)
        screenshot = work / 'dexnav-types.bmp'
        screenshot_arg = 'Z:' + str(screenshot).replace('/', '\\') if windows else str(screenshot)
        try:
            result = subprocess.run((['wine'] if windows else []) + [str(binary), 'wild-dexnav', module_arg],
                                    cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            types_result = subprocess.run((['wine'] if windows else []) + [str(binary), 'dexnav-types', types_arg, screenshot_arg],
                                          cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        finally:
            if windows:
                subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        markers = ('DEXNAV_PROBE_CANCELLED', 'DEXNAV_PROBE_GENERATED',
                   'dexnav: search veto, rollback and generated battle candidate passed')
        if result.returncode or 'DEXNAV_PROBE_BAD' in result.stdout or any(m not in result.stdout for m in markers):
            raise RuntimeError(f'DexNav integration failed ({result.returncode}):\n{result.stdout}')
        if types_result.returncode or 'SPECIES_TYPES_BAD' in types_result.stdout or 'dexnav types: GUI' not in types_result.stdout:
            raise RuntimeError(f'DexNav types failed ({types_result.returncode}):\n{types_result.stdout}')
        print(f'{args.target}: real DexNav search, veto, retry and independent mod passed')
        print(f'{args.target}: DexNav three-type display and transitions passed; screenshot: {screenshot}', flush=True)
        if args.inspect:
            input('Press Enter to remove the temporary screenshot and build. ')


if __name__ == '__main__':
    main()
