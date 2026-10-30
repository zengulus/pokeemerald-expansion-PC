#!/usr/bin/env python3
"""Fetch the selected public battle MIDI/SF2 files and record their provenance.

Normal builds are offline and use the checked-in assets/manifest. Re-fetching
checks the recorded SHA-256 values; --record is only for choosing new assets.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import io
from pathlib import Path
import struct
import subprocess
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / 'assets/music'
FONT_BASE = 'https://www.anapan.ca/Anapan/FM/Full%20GM%20Soundfonts/'
GM_MIRROR = 'https://datajake.braillescreen.net/SoundfontFiles/500_Soundfonts_Full_GM_Sets/'
TSF_COMMIT = '853a0a171759f1ddba0de1442133a75912bbeffa'
CATALOGUES = {
    'gb': 'https://www.vgmusic.com/music/console/nintendo/gameboy/',
    'gba': 'https://www.vgmusic.com/music/console/nintendo/gba/',
    'ds': 'https://www.vgmusic.com/music/console/nintendo/ds/',
}
TRACKS = [
    ('gb', ['PkmRB-Battle1.mid', 'PkmRB-Battle2.mid', 'PkmRB-Battle3.mid', 'PkmRB-Battle4.mid'], ['Joao *Johnnyz* Buaes'] * 4),
    ('gb', ['PkmGS-Battle1.mid', 'PkmGS-Battle3.mid', 'PkmGS-Battle4.mid', 'PkmGS-Battle6.mid'], ['Joao *Johnnyz* Buaes'] * 4),
    ('gba', ['PkmRS_Battle1.mid', 'PkmRS-Battle3.mid', 'PkmRS_Battle_5.mid', 'PkmRS-Battle10.mid'], ['Joao *Johnnyz* Buaes'] * 4),
    ('ds', ['Pokemon_Diamond_Pearl_Wild_Pokemon_Battle.mid', 'Trainer.mid', 'PMDPgymbattle.mid', 'pmdpp_champbattle.mid'], ['Electric Concerto', 'Brian Hayden', 'foxT.A.K.A', 'Joanna']),
    ('ds', ['PKMN_BW_wild_battle_midi.mid', 'TrainerWhite.mid', 'Gym_Leader.mid', 'irisfix.mid'], ['Neocrystal3D/Superpokemonray1234', 'Fortwaffles', 'Fortwaffles', 'Wobuffet3']),
]

def definitions():
    files = []
    for gen, (platform, tracks, authors) in enumerate(TRACKS, 1):
        for role, name, author in zip(['wild', 'trainer', 'gym', 'champion'], tracks, authors):
            files.append(dict(path=f'gen{gen}/{role}.mid', source=CATALOGUES[platform] + name,
                author=author, format='midi', id=f'rhh:midi/gen{gen}/{role}',
                source_catalogue=CATALOGUES[platform]))
    files.extend([
        dict(path='gameboy/gen1.gbs', source='https://fi.zophar.net/soundfiles/gameboy-gbs/pokemon-red/Pokemon%20Red%20%28EMU%29.zophar.zip',
             zip_entry='DMG-APAE-USA.gbs', zip_archive_small=True, format='gbs', id='rhh:gameboy/gen1',
             source_sha256='dfba721f7e8b0300e14f5e4d8135a2117c2fdf10052f6a643b6e21b51223e7c4'),
        dict(path='gameboy/gen2.gbs', source='https://fi.zophar.net/soundfiles/gameboy-gbs/pokemon-gold/Pokemon%20Gold%20%28EMU%29.zophar.zip',
             zip_entry='DMG-AAUJ-JPN.gbs', zip_archive_small=True, format='gbs', id='rhh:gameboy/gen2',
             source_sha256='2af120a66272675ae69c287d6608f6fb9bca106758b4b18a807bc277c0bb87e5'),
        dict(path='fonts/gen12.sf2', source=GM_MIRROR + 'Gameboy_GM_Soundfont_2.sf2', format='sf2', id='rhh:soundfont/gen12'),
        dict(path='fonts/gen3.sf2', source='https://archive.org/download/soundfonts-collection/The%20Ultimate%20SoundFont%20Pack.zip',
             zip_entry='Pokémon Emerald.sf2', archive_size=3226345874,
             format='sf2', id='rhh:soundfont/gen3', author='MezmerKaiser', title='Pokemon Emerald Soundfont (Updated Aug 13, 2021)'),
        dict(path='fonts/gen4.sf2', source='https://archive.org/download/soundfonts-collection/The%20Ultimate%20SoundFont%20Pack.zip',
             zip_entry='Pokémon Platinum.sf2', archive_size=3226345874,
             format='sf2', id='rhh:soundfont/gen4', title='Pokemon DPPt Soundfont 2021 (GM)'),
        dict(path='fonts/gen5.sf2', source='https://drive.google.com/uc?export=download&id=12f57qQTnlMnp5KZn2kyhWDYL3v_oyw4t',
             source_catalogue='https://www.polyphone.io/en/soundfonts/instrument-sets/1103-pokemon-black-and-white-2-soundfont-updated',
             author='Xennon', format='sf2', id='rhh:soundfont/gen5'),
    ])
    return files

class RemoteZip(io.RawIOBase):
    """Read only bounded byte ranges from a public ZIP, never its whole payload."""
    def __init__(self, url, size):
        self.url, self.size, self.position = url, size, 0
    def seekable(self): return True
    def tell(self): return self.position
    def seek(self, offset, whence=0):
        self.position = offset if whence == 0 else self.position + offset if whence == 1 else self.size + offset
        return self.position
    def read(self, size=-1):
        size = self.size - self.position if size < 0 else min(size, self.size - self.position)
        if not size: return b''
        if size < 0 or size > 16 * 1024 * 1024: raise ValueError('Archive range exceeds bounds')
        request = urllib.request.Request(self.url, headers={'Range': f'bytes={self.position}-{self.position + size - 1}'})
        with urllib.request.urlopen(request, timeout=60) as response:
            if response.status != 206: raise ValueError('Archive server ignored byte range')
            data = response.read(size + 1)
        if len(data) != size: raise ValueError('Archive server returned the wrong range length')
        self.position += size
        return data

def fetch_zip_member(entry):
    remote = RemoteZip(entry['source'], entry['archive_size'])
    with zipfile.ZipFile(remote) as archive:
        info = archive.getinfo(entry['zip_entry'])
        if info.compress_type != 9: return archive.read(info)
        # Python does not support Deflate64. Reconstruct a one-member ZIP from
        # the exact compressed bytes, then use Info-ZIP's Deflate64 decoder.
        remote.seek(info.header_offset)
        header = remote.read(30)
        name_size, extra_size = struct.unpack_from('<HH', header, 26)
        remote.seek(info.header_offset + 30 + name_size + extra_size)
        compressed = remote.read(info.compress_size)
        name = info.filename.encode('utf-8')
        local = struct.pack('<4s5H3I2H', b'PK\x03\x04', 21, 0x800, 9, 0, 0,
            info.CRC, info.compress_size, info.file_size, len(name), 0) + name + compressed
        central = struct.pack('<4s6H3I5H2I', b'PK\x01\x02', 21, 21, 0x800, 9, 0, 0,
            info.CRC, info.compress_size, info.file_size, len(name), 0, 0, 0, 0, 0, 0) + name
        end = struct.pack('<4s4H2IH', b'PK\x05\x06', 0, 0, 1, 1, len(central), len(local), 0)
        with tempfile.TemporaryDirectory(prefix='rhh-soundfont-') as directory:
            path = Path(directory) / 'font.zip'; path.write_bytes(local + central + end)
            return subprocess.run(['unzip', '-p', path], check=True, capture_output=True).stdout

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    manifest_path = ASSETS / 'manifest.json'
    old = json.loads(manifest_path.read_text()) if manifest_path.exists() else None
    if not old and not args.record:
        parser.error('No lock manifest; choosing assets requires --record')
    if args.record:
        files = definitions()
        # Derived DS exports require both the original sequence and its exact
        # instrument bank. Keep their recorded provenance when refreshing the
        # download catalog rather than replacing them with GM arrangements.
        derived = [entry.copy() for entry in old['files'] if entry.get('derived')] if old else []
        paths = {entry['path'] for entry in derived}
        files = [entry for entry in files if entry['path'] not in paths] + derived
    else:
        files = old['files']
    def fetch(entry):
        dest = ASSETS / entry['path']
        if dest.exists() and (args.record or hashlib.sha256(dest.read_bytes()).hexdigest() == entry['sha256']):
            data = dest.read_bytes()
        else:
            if entry.get('derived'):
                raise ValueError(f"{entry['path']} is a derived DS export; see THIRD_PARTY.md for regeneration")
            if entry.get('zip_archive_small'):
                request = urllib.request.Request(entry['source'], headers={'User-Agent': 'RHH-native-asset-fetch/1.0'})
                with urllib.request.urlopen(request, timeout=60) as response:
                    archive = response.read(2 * 1024 * 1024 + 1)
                if len(archive) > 2 * 1024 * 1024: raise ValueError('Music archive exceeds bounds')
                if entry.get('source_sha256') and hashlib.sha256(archive).hexdigest() != entry['source_sha256']:
                    raise ValueError(f"Archive hash changed: {entry['path']}")
                with zipfile.ZipFile(io.BytesIO(archive)) as zipped:
                    info = zipped.getinfo(entry['zip_entry'])
                    if info.file_size > 1024 * 1024: raise ValueError('GBS file exceeds bounds')
                    data = zipped.read(info)
            elif entry.get('zip_entry'):
                data = fetch_zip_member(entry)
            else:
                request = urllib.request.Request(entry['source'], headers={'User-Agent': 'RHH-native-asset-fetch/1.0'})
                data = urllib.request.urlopen(request, timeout=60).read()
        if entry['format'] == 'midi' and not data.startswith(b'MThd'):
            raise ValueError(f"Not a MIDI file: {entry['path']}")
        if entry['format'] == 'gbs' and (len(data) < 112 or data[:4] != b'GBS\1'):
            raise ValueError(f"Not a GBS file: {entry['path']}")
        if entry['format'] == 'sf2' and (data[:4] != b'RIFF' or data[8:12] != b'sfbk'):
            rejected = Path('/tmp/rhh-music-research') / (Path(entry['path']).name + '.response')
            rejected.write_bytes(data)
            raise ValueError(f"Not an SF2 file: {entry['path']} ({len(data)} bytes)")
        digest = hashlib.sha256(data).hexdigest()
        if not args.record and digest != entry['sha256']:
            raise ValueError(f"Asset hash changed: {entry['path']}")
        dest.parent.mkdir(parents=True, exist_ok=True); dest.write_bytes(data)
        entry.update(sha256=digest, size=len(data))
        print(entry['path'], len(data), digest, flush=True)
        return entry
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(fetch, files))
    packs = [dict(id=f'rhh:music/gen{g}', generation=g,
        font=f'fonts/gen{12 if g < 3 else g}.sf2',
        tracks={r:f'gen{g}/{r}.mid' for r in ['wild','trainer','gym','champion']}) for g in range(1,6)]
    if any(entry['path'] == 'fonts/gen4-native.sf2' for entry in results):
        packs[3]['font'] = 'fonts/gen4-native.sf2'
    if any(entry['path'] == 'fonts/gen5-wild.sf2' for entry in results):
        packs[4].pop('font')
        packs[4]['fonts'] = {role:f'fonts/gen5-{role}.sf2' for role in ['wild','trainer','gym','champion']}
    for generation, tracks in [(1, [23, 22, 21, 24]), (2, [40, 41, 45, 46])]:
        if any(entry['path'] == f'gameboy/gen{generation}.gbs' for entry in results):
            packs[generation - 1]['gameboy'] = {'file': f'gameboy/gen{generation}.gbs',
                'tracks': dict(zip(['wild', 'trainer', 'gym', 'champion'], tracks))}
    manifest = dict(version=1, files=results, packs=packs)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')

if __name__ == '__main__':
    main()
