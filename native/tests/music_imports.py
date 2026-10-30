#!/usr/bin/env python3
"""Test persistent imported music with disposable files and build cleanup."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux32', 'linux64', 'windows32', 'windows64'], default='linux64')
    target = parser.parse_args().target
    windows = target.startswith('windows')
    bits = '64' if target.endswith('64') else '32'
    directory = ROOT / 'build/native' / target
    binary = directory / ('gameplay-tests.exe' if windows else 'gameplay-tests')
    common = [directory / 'native' / f'{name}.{ext}' for name in
              ('mod_file_dialog', 'mod_text_entry', 'mod_music_assignments', 'mod_music_imports', 'music_sha256', 'mod_boss_ace') for ext in ('o', 'd')]
    existed = {p: p.exists() for p in common}
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', '-j8', 'gameplay-tests'],
                               cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-music-imports-') as temp:
            work = Path(temp)
            library = work / 'música'
            library.mkdir()
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            module = work / ('import.dll' if windows else 'import.so')
            compiler = ('x86_64-w64-mingw32-gcc' if bits == '64' else 'i686-w64-mingw32-gcc') if windows else 'gcc'
            flags = [f'-m{bits}', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/music_import_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            def arg(path):
                return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
            def run(phase, marker):
                command = (['wine'] if windows else []) + [str(binary), phase, arg(work / 'test.sav'), arg(module),
                                                          arg(ROOT / 'native/assets/music'), arg(library)]
                result = subprocess.run(command, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                if result.returncode or 'MUSIC_IMPORT_BAD' in result.stdout or marker not in result.stdout:
                    raise RuntimeError(f'{target} {phase} failed ({result.returncode}):\n{result.stdout}')
                return result.stdout
            try:
                saved = run('music-import-save', 'music imports: validation')
                if saved.count('MUSIC_IMPORT_ADDED') != 2:
                    raise RuntimeError('Expected exactly two import notifications:\n' + saved)
                source = ROOT / 'native/assets/music/gen1/wild.mid'
                digest = hashlib.sha256(source.read_bytes()).hexdigest()
                copied = library / f'rhh-music-{digest}.mid'
                if copied.read_bytes() != source.read_bytes() or len(list(library.glob('*.mid'))) != 2 or len(list(library.glob('*.sf2'))) != 1:
                    raise RuntimeError('Assets were not copied and deduplicated correctly')
                if not windows:
                    import fcntl
                    with (library / 'rhh-music-library.lock').open('r+b') as lock:
                        fcntl.flock(lock, fcntl.LOCK_EX)
                        run('music-import-locked', 'music imports: concurrent writer rejected')
                manifest = library / 'rhh-music-library.v1'
                original_manifest = manifest.read_bytes()
                if not windows:
                    library.chmod(0o555)
                    try:
                        run('music-import-io', 'music imports: failed write left library unchanged')
                    finally:
                        library.chmod(0o755)
                    if manifest.read_bytes() != original_manifest or len(list(library.glob('*.mid'))) != 2:
                        raise RuntimeError('Failed import changed the existing library')
                bad = bytearray(original_manifest); bad[-1] ^= 1; manifest.write_bytes(bad)
                run('music-import-corrupt', 'music imports: corrupt manifest preserved')
                if manifest.read_bytes() != bad:
                    raise RuntimeError('The corrupt manifest was overwritten')
                manifest.write_bytes(original_manifest)
                bad_midi = bytearray(copied.read_bytes()); bad_midi[-1] ^= 1; copied.write_bytes(bad_midi)
                run('music-import-load', 'music imports: restart, corrupt-asset fallback and repair')
                if copied.read_bytes() != source.read_bytes():
                    raise RuntimeError('Re-import did not repair the copied MIDI')
                removed = run('music-import-delete', 'music imports: shared assets and deleted-track fallback')
                if removed.count('MUSIC_IMPORT_REMOVED') != 2 or list(library.glob('*.mid')) or list(library.glob('*.sf2')):
                    raise RuntimeError('Removing imports did not clean up unshared assets:\n' + removed)
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{target}: imported music, integrity, deduplication, playback and cleanup passed')
    finally:
        subprocess.run(['make', '-s', '-f', 'Makefile.native', f'TARGET={target}', 'clean-test-builds'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for path, present in existed.items():
            if not present:
                path.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
