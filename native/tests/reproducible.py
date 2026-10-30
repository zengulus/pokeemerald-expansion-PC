#!/usr/bin/env python3
"""Build two independent source exports and compare the Windows artifacts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['windows32', 'windows64'], default='windows64')
    parser.add_argument('--sdl-archive', type=Path,
                        default=ROOT / 'native/deps/SDL2-devel-2.30.0-mingw.tar.gz')
    parser.add_argument('--output', type=Path, help='Keep build logs and report in this directory')
    parser.add_argument('-j', type=int, default=4)
    args = parser.parse_args()
    archive = args.sdl_archive.resolve()
    if not archive.is_file():
        raise RuntimeError('Run bash native/setup_sdl2.sh or supply --sdl-archive first')
    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
        work = Path(tempfile.mkdtemp(prefix=args.target + '-', dir=args.output.resolve()))
        run(args, archive, work)
    else:
        with tempfile.TemporaryDirectory(prefix=f'rhh-repro-report-{args.target}-') as temporary:
            run(args, archive, Path(temporary))


def run(args, archive, work):
    files = subprocess.check_output(['git', 'ls-files', '--cached', '--others',
                                     '--exclude-standard', '-z'], cwd=ROOT).decode().split('\0')
    files = sorted(name for name in set(files) if name and (ROOT / name).is_file())
    source_hash = hashlib.sha256()
    print(f'Build logs and report: {work}' + ('' if args.output else ' (temporary)'), flush=True)
    outputs = []
    env = dict(os.environ, SOURCE_DATE_EPOCH='0')
    # Independent checkouts can occupy gigabytes. Keep only their logs and
    # hash report, and remove the temporary trees on success or failure.
    with tempfile.TemporaryDirectory(prefix='rhh-repro-build-') as temporary:
        scratch = Path(temporary)
        snapshot = scratch / 'source'
        for name in files:
            destination = snapshot / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / name, destination)
            source_hash.update(name.encode() + b'\0')
            source_hash.update(destination.read_bytes())
        for name in ['first', 'second']:
            export = scratch / name
            for relative in files:
                dst = export / relative
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(snapshot / relative, dst)
            deps = export / 'native/deps'
            deps.mkdir(parents=True, exist_ok=True)
            shutil.copy2(archive, deps / 'SDL2-devel-2.30.0-mingw.tar.gz')
            commands = [['bash', 'native/setup_sdl2.sh'],
                        ['make', '-f', 'Makefile.native', 'prepare'],
                        ['make', '-f', 'Makefile.native', f'TARGET={args.target}', 'all', f'-j{args.j}']]
            log_path = work / f'{name}.log'
            try:
                with log_path.open('w') as log:
                    for command in commands:
                        subprocess.run(command, check=True, cwd=export, env=env,
                                       stdout=log, stderr=subprocess.STDOUT)
            except subprocess.CalledProcessError:
                print(f'{name}: last log output:\n{log_path.read_text(errors="replace")[-8192:]}',
                      file=sys.stderr, flush=True)
                raise
            built = export / 'build/native' / args.target
            artifacts = ['pokeemerald-expansion.exe', 'SDL2.dll', 'rhhgme.dll']
            artifacts += [str(path.relative_to(built)) for path in sorted((built / 'example-mods').glob('*.dll'))]
            outputs.append({file: digest(built / file) for file in artifacts})
            print(f'{name}: {outputs[-1]}', flush=True)
            shutil.rmtree(export)
    if outputs[0] != outputs[1]:
        raise RuntimeError('Independent clean Windows build artifacts differ')
    versions = {}
    cross_compiler = 'x86_64-w64-mingw32-gcc' if args.target == 'windows64' else 'i686-w64-mingw32-gcc'
    cross_cxx = cross_compiler[:-3] + 'g++'
    for program in ['gcc', 'g++', cross_compiler, cross_cxx, 'make', 'python3']:
        versions[program] = subprocess.check_output([program, '--version'], text=True).splitlines()[0]
    report = dict(target=args.target, source_sha256=source_hash.hexdigest(), sdl_archive_sha256=digest(archive),
                  artifacts=outputs[0], toolchain=versions, source_date_epoch=0)
    (work / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Two clean Windows builds are byte-for-byte identical', flush=True)

if __name__ == '__main__':
    main()
