#!/usr/bin/env python3
"""Package the self-contained Windows binary and its runtime dependency."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['windows64', 'windows32'], default='windows64')
    parser.add_argument('--reproducible-report', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/native/dist')
    args = parser.parse_args()
    report = json.loads(args.reproducible_report.read_text())
    if report['target'] != args.target:
        raise RuntimeError('Reproduction report targets a different architecture')
    if 'rhhgme.dll' not in report['artifacts']:
        raise RuntimeError('Build verification must include the Game Boy music library')
    files = {}
    for name, expected in report['artifacts'].items():
        data = (ROOT / 'build/native' / args.target / name).read_bytes()
        if digest(data) != expected:
            raise RuntimeError(f'{name} differs from the verified clean builds')
        files[name] = data
    for name, source in {
        'README.txt': 'native/README_WINDOWS.txt',
        'PORTING.md': 'PORTING.md', 'PORTING_PROGRESS.md': 'PORTING_PROGRESS.md',
        'THIRD_PARTY.md': 'native/THIRD_PARTY.md',
        'MULTIPLATFORM_LICENSE.txt': 'native/MULTIPLATFORM_LICENSE',
        'SDL2_COPYING.txt': 'native/deps/SDL2/COPYING.txt',
        'MODDING.md': 'MODDING.md', 'MODS_PROGRESS.md': 'MODS_PROGRESS.md',
        'NEEDED_MOD_API_SERVICES.md': 'NEEDED_MOD_API_SERVICES.md',
        'include/mod_api.h': 'include/mod_api.h',
        'docs/tutorials/how_to_overworld_wild_encounters.md': 'docs/tutorials/how_to_overworld_wild_encounters.md',
        'docs/tutorials/how_to_new_move.md': 'docs/tutorials/how_to_new_move.md',
        'TINYSOUNDFONT_LICENSE.txt': 'native/third_party/tinysoundfont/LICENSE',
        'TINYMIDILOADER_LICENSE.txt': 'native/third_party/tinysoundfont/tml.h',
        'GAME_MUSIC_EMU_LICENSE.txt': 'native/third_party/gme/LICENSE',
        'examples/README.txt': 'native/examples/README.txt',
        'examples/generate_shader.py': 'native/generate_shader.py',
    }.items():
        files[name] = (ROOT / source).read_bytes()
    music = ROOT / 'build/native' / args.target / 'music'
    manifest = json.loads((music / 'manifest.json').read_text())
    for entry in manifest['files']:
        data = (music / entry['path']).read_bytes()
        if digest(data) != entry['sha256']: raise RuntimeError('Packaged music hash mismatch: ' + entry['path'])
        files['music/' + entry['path']] = data
    files['music/manifest.json'] = (music / 'manifest.json').read_bytes()
    files['examples/include/mod_api.h'] = (ROOT / 'include/mod_api.h').read_bytes()
    # Ship the standalone example alongside the bundled mod sources.
    files['examples/level_cap_mod.c'] = (ROOT / 'native/examples/level_cap_mod.c').read_bytes()
    for path in sorted((ROOT / 'native/mods').glob('*')):
        if path.suffix in {'.c', '.h', '.frag'}: files['examples/mods/' + path.name] = path.read_bytes()
    for path in sorted((ROOT / 'native/third_party/gme').glob('*')):
        if path.is_file(): files['game-music-emu-source/' + path.name] = path.read_bytes()
    for path in sorted((ROOT / 'build/native/evidence/battle-environment').glob('*')):
        if path.suffix in {'.png', '.html'}: files['previews/' + path.name] = path.read_bytes()
    label = b'x64' if args.target == 'windows64' else b'x86'
    files['README.txt'] = files['README.txt'].replace(b'Windows x64', b'Windows ' + label)
    files['BUILDINFO.json'] = (json.dumps(report, indent=2) + '\n').encode()
    files['SHA256SUMS.txt'] = ''.join(f'{digest(data)}  {name}\n'
                                    for name, data in sorted(files.items())).encode()
    args.output.mkdir(parents=True, exist_ok=True)
    stem = 'pokeemerald-expansion-' + args.target
    output = args.output / (stem + '.zip')
    with zipfile.ZipFile(output, 'w') as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(stem + '/' + name, (1980, 1, 1, 0, 0, 0))
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    print(f'{output}\nSHA-256: {digest(output.read_bytes())}')

if __name__ == '__main__':
    main()
