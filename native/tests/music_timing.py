#!/usr/bin/env python3
"""Exercise music source timing and imported-asset handling in a disposable build."""
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / 'native/assets/music'


def main():
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with tempfile.TemporaryDirectory(prefix='rhh-music-timing-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64',
                                     f'OBJ_DIR={build}', '-j8', 'gameplay-tests'],
                                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        module = work / 'import.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/music_import_probe.c', '-o', str(module)], cwd=ROOT, check=True)
        library = work / 'library'
        library.mkdir()

        def run(arguments, marker):
            result = subprocess.run([str(build / 'gameplay-tests'), *map(str, arguments)], cwd=ROOT, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=180)
            if result.returncode or marker not in result.stdout or 'MUSIC_IMPORT_BAD' in result.stdout:
                raise RuntimeError(f'{arguments[0]} failed ({result.returncode}):\n{result.stdout}')

        fixture = work / 'fixture'
        (fixture / 'gen1').mkdir(parents=True)
        (fixture / 'fonts').mkdir()
        shutil.copyfile(ASSETS / 'fonts/gen4.sf2', fixture / 'fonts/gen4.sf2')

        def write_midi(events):
            track = b''.join(b'\0' + bytes(event) for event in events) + b'\0\xff\x2f\0'
            (fixture / 'gen1/wild.mid').write_bytes(
                b'MThd' + struct.pack('>IHHH', 6, 0, 1, 480) + b'MTrk' + struct.pack('>I', len(track)) + track)

        write_midi([(0xc0, 0), (0x90, 60, 64), (0x90, 61, 64), (0xc0, 1), (0x90, 62, 64),
                    (0xb0, 0, 1), (0x90, 63, 64), (0xc0, 0), (0x90, 64, 64), (0xc9, 127), (0x99, 35, 64)])
        run([fixture, 'music-instruments', 0], 'music instruments: source presets')
        many = []
        for program in range(128):
            many.extend([(0xc0, program), (0x90, 60, 64)])
        many.extend([(0xc1, 0), (0x91, 60, 64), (0xc0, 0), (0x90, 60, 64)])
        write_midi(many)
        run([fixture, 'music-instruments', 1], 'music instruments: source presets')
        drums = [(0xc9, 0)] + [(0x99, key, 64) for key in range(128)] + [(0x99, 0, 64)]
        write_midi(drums)
        run([fixture, 'music-instruments', 2], 'music instruments: source presets')

        run([ASSETS, 'music'], 'custom music: battle and route selection')
        run([ASSETS, 'music-cue'], 'battle music cue: deferred selection')
        args = [work / 'test.sav', module, ASSETS, library]
        run(['music-import-save', *args], 'music imports: validation')
        midi_hash = hashlib.sha256((ASSETS / 'gen1/wild.mid').read_bytes()).hexdigest()
        copied = library / f'rhh-music-{midi_hash}.mid'
        data = bytearray(copied.read_bytes())
        data[-1] ^= 1
        copied.write_bytes(data)
        run(['music-import-load', *args], 'music imports: restart')
        run(['music-import-delete', *args], 'music imports: shared assets')
    print('linux64: music source timing, playback isolation and imported assets passed; temporary files removed')


if __name__ == '__main__':
    main()
