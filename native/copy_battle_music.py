#!/usr/bin/env python3
"""Verify and copy the locked battle music assets; entirely offline."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parent / 'assets/music'
    manifest = json.loads((source / 'manifest.json').read_text())
    if manifest['version'] != 1: raise ValueError('Unsupported music manifest')
    for entry in manifest['files']:
        path = source / entry['path']
        if path.stat().st_size != entry['size'] or hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError(f'Music asset failed verification: {path}')
        destination = args.destination / entry['path']
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, destination)
    shutil.copyfile(source / 'manifest.json', args.destination / 'manifest.json')
    print(f"Verified {len(manifest['files'])} music assets: {args.destination}")

if __name__ == '__main__':
    main()
