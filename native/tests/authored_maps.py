#!/usr/bin/env python3
"""Verify authored map snapshots against loaded engine data in a disposable build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix='rhh-authored-maps-') as directory:
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
                        '-iquote', 'include', 'native/tests/authored_maps_probe.c', '-o', str(module)], cwd=ROOT, check=True)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        result = subprocess.run([str(build / 'gameplay-tests'), 'authored-maps', str(module)], cwd=ROOT, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if (result.returncode or 'authored maps: static layouts match loaded engine tiles and remain isolated' not in result.stdout
            or 'AUTHORED_MAP_EXTERNAL_OK' not in result.stdout or 'AUTHORED_MAP_EXTERNAL_BAD' in result.stdout):
            raise RuntimeError(f'authored maps failed ({result.returncode}):\n{result.stdout}')
        # Exercise the production codec with a forced internal allocation failure.
        flags = ['-m64', '-std=gnu17', '-O2', '-g', '-fno-strict-aliasing', '-fwrapv', '-mno-ms-bitfields',
                 '-ffunction-sections', '-fdata-sections', '-DMODERN=1', '-DNATIVE', '-DTESTING=0',
                 '-DEMERALD', '-DNDEBUG', '-iquote', 'include', '-Inative/include']
        prepared = work / 'decompress.i'
        with prepared.open('w') as output:
            cpp = subprocess.Popen(['gcc', *flags, '-E', 'src/decompress.c'], cwd=ROOT, stdout=subprocess.PIPE)
            result = subprocess.run(['tools/preproc/preproc', '-ni', '-g', 'build/assets', 'src/decompress.c', 'charmap.txt'],
                                    cwd=ROOT, stdin=cpp.stdout, stdout=output)
            cpp.stdout.close()
            if cpp.wait() or result.returncode:
                raise RuntimeError('codec preprocessing failed')
        codec = work / 'codec-test'
        subprocess.run(['gcc', *flags, str(prepared), 'native/memory.c', 'native/tests/decompress_test.c',
                        'native/diagnostics.c', '-Wl,--gc-sections', '-o', str(codec)], cwd=ROOT, check=True)
        raw, packed = work / 'raw.bin', work / 'packed.bin'
        raw.write_bytes(b'\x42' * 4)
        packed.write_bytes(b'\x10\x04\x00\x00\x00' + b'\x42' * 4)
        subprocess.run([str(codec), str(raw), str(packed)], cwd=ROOT, check=True)
        for index, data in enumerate((bytes(1024), bytes(i % 16 for i in range(1024)),
                                      bytes((i * 73 + i // 7) % 256 for i in range(1024)))):
            raw.write_bytes(data)
            for suffix, options in [('smol', []), ('fastSmol', ['false', 'false', 'false'])]:
                packed = work / f'fixture-{index}.{suffix}'
                subprocess.run(['tools/compresSmol/compresSmol', '-w', str(raw), str(packed), *options],
                               cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
                subprocess.run([str(codec), str(raw), str(packed)], cwd=ROOT, check=True)
    print('linux64: authored map and loaded-map parity passed; temporary files removed')


if __name__ == '__main__':
    main()
