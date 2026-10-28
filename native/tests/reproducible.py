#!/usr/bin/env python3
"""Build two independent source exports and compare the Windows artifacts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['windows32', 'windows64'], default='windows64')
    parser.add_argument('--sdl-archive', type=Path,
                        default=ROOT / 'native/deps/SDL2-devel-2.30.0-mingw.tar.gz')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/native/reproducible')
    parser.add_argument('-j', type=int, default=4)
    args = parser.parse_args()
    archive = args.sdl_archive.resolve()
    if not archive.is_file():
        raise RuntimeError('Run bash native/setup_sdl2.sh or supply --sdl-archive first')
    args.output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=args.target + '-', dir=args.output.resolve()))
    files = subprocess.check_output(['git', 'ls-files', '--cached', '--others',
                                     '--exclude-standard', '-z'], cwd=ROOT).decode().split('\0')
    files = sorted(name for name in set(files) if name and (ROOT / name).is_file())
    source_hash = hashlib.sha256()
    snapshot = work / 'source'
    for name in files:
        destination = snapshot / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / name, destination)
        source_hash.update(name.encode() + b'\0')
        source_hash.update(destination.read_bytes())
    print(f'Artifacts and clean source exports: {work}', flush=True)
    outputs = []
    env = dict(os.environ, SOURCE_DATE_EPOCH='0')
    for name in ['first', 'second']:
        export = work / name
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
        with (work / f'{name}.log').open('w') as log:
            for command in commands:
                subprocess.run(command, check=True, cwd=export, env=env,
                               stdout=log, stderr=subprocess.STDOUT)
        built = export / 'build/native' / args.target
        artifacts = ['pokeemerald-expansion.exe', 'SDL2.dll']
        artifacts += [str(path.relative_to(built)) for path in sorted((built / 'example-mods').glob('*.dll'))]
        outputs.append({file: digest(built / file) for file in artifacts})
        print(f'{name}: {outputs[-1]}', flush=True)
    if outputs[0] != outputs[1]:
        raise RuntimeError('Independent clean Windows build artifacts differ')
    versions = {}
    cross_compiler = 'x86_64-w64-mingw32-gcc' if args.target == 'windows64' else 'i686-w64-mingw32-gcc'
    for program in ['gcc', 'g++', cross_compiler, 'make', 'python3']:
        versions[program] = subprocess.check_output([program, '--version'], text=True).splitlines()[0]
    report = dict(target=args.target, source_sha256=source_hash.hexdigest(), sdl_archive_sha256=digest(archive),
                  artifacts=outputs[0], toolchain=versions, source_date_epoch=0)
    (work / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Two clean Windows builds are byte-for-byte identical', flush=True)

if __name__ == '__main__':
    main()
