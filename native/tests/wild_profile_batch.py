#!/usr/bin/env python3
"""Test atomic actor profiles with disposable builds and save files."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    build_dir = ROOT / 'build/native/linux64'
    existing = set(build_dir.rglob('*'))
    try:
        build = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', '-j8', 'gameplay-tests'],
                               cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if build.returncode:
            raise RuntimeError('\n'.join(build.stdout.splitlines()[-40:]))
        with tempfile.TemporaryDirectory(prefix='rhh-profile-batch-') as directory:
            work = Path(directory); module = work / 'probe.so'; save = work / 'profile.sav'
            subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-iquote', 'include', 'native/tests/wild_profile_batch_probe.c', '-o', str(module)],
                           cwd=ROOT, check=True)
            conflict = work / 'conflict.so'
            subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-iquote', 'include', 'native/tests/wild_profile_schema_conflict.c', '-o', str(conflict)],
                           cwd=ROOT, check=True)
            compatible = work / 'compatible.so'
            subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-iquote', 'include', '-DPROFILE_SCHEMA_IDENTICAL', 'native/tests/wild_profile_schema_conflict.c',
                            '-o', str(compatible)], cwd=ROOT, check=True)
            legacy = work / 'legacy.so'
            subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-iquote', 'include', '-DPROFILE_SCHEMA_LEGACY', 'native/tests/wild_profile_schema_conflict.c',
                            '-o', str(legacy)], cwd=ROOT, check=True)
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
            for mode, marker in (('save', 'wild profile batch: atomic constraints, capacity and save passed'),
                                 ('load', 'wild profile batch: fresh-process restore passed')):
                result = subprocess.run([str(build_dir / 'gameplay-tests'), f'wild-profile-batch-{mode}', str(save), str(module)],
                                        cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if (result.returncode or 'PROFILE_BATCH_BAD' in result.stdout or marker not in result.stdout
                    or (mode == 'save' and (result.stdout.count('PROFILE_BATCH_COMMITTED') != 11
                                            or 'PROFILE_BATCH_UPDATE_OK' not in result.stdout))):
                    raise RuntimeError(f'profile batch {mode} failed ({result.returncode}):\n{result.stdout}')
            for kind, other, marker in (('conflict', conflict, 'wild profile schema: independent conflicts fail closed'),
                                        ('compatible', compatible, 'wild profile schema: independent matching offers compose'),
                                        ('legacy', legacy, 'wild profile schema: legacy lookup remains available outside discovery')):
                for first, second in ((module, other), (other, module)):
                    result = subprocess.run([str(build_dir / 'gameplay-tests'), f'wild-profile-schema-{kind}', str(first), str(second)],
                                            cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    if result.returncode or 'PROFILE_BATCH_BAD' in result.stdout or marker not in result.stdout:
                        raise RuntimeError(f'profile schema {kind} failed ({result.returncode}):\n{result.stdout}')
    finally:
        for path in build_dir.rglob('*'):
            if (path.is_file() or path.is_symlink()) and path not in existing:
                path.unlink(missing_ok=True)
        for path in sorted((p for p in build_dir.rglob('*') if p.is_dir() and p not in existing),
                           key=lambda p: len(p.parts), reverse=True):
            try:
                path.rmdir()
            except OSError:
                pass
    print('Atomic wild profiles and external observer passed; test artifacts removed.')


if __name__ == '__main__':
    main()
