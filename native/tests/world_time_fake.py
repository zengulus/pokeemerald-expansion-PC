#!/usr/bin/env python3
"""Exercise the fake-RTC build in a self-cleaning temporary directory."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix='rhh-fake-clock-test-') as directory:
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
        before = '#define OW_USE_FAKE_RTC                 FALSE'
        if source.count(before) != 1:
            raise RuntimeError('Could not prepare fake RTC configuration')
        (config / 'overworld.h').write_text(source.replace(before, '#define OW_USE_FAKE_RTC                 TRUE'))
        build = work / 'build'
        sdl = subprocess.check_output(['sdl2-config', '--cflags'], text=True).strip()
        flags = (f'-iquote {overlay} -iquote include -Inative/include -DMODERN=1 -DNATIVE -DPORTABLE '
                 '-DRENDERER_EASY_DRAW -DTESTING=0 -DEMERALD -DNDEBUG -std=gnu17 '
                 f'{sdl} -I{build}')
        result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', f'OBJ_DIR={build}',
                                 f'CPPFLAGS={flags}', '-j8', str(build / 'gameplay-tests')], cwd=ROOT,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join(result.stdout.splitlines()[-50:]))
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        result = subprocess.run([str(build / 'gameplay-tests'), 'world-time-fake'], cwd=ROOT, env=env,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode or 'fake RTC progression' not in result.stdout:
            raise RuntimeError(f'Fake clock test failed ({result.returncode}):\n{result.stdout}')
        print('linux64: fake RTC, invalid-date rejection and pure queries passed')


if __name__ == '__main__':
    main()
