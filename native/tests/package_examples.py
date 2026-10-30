#!/usr/bin/env python3
"""Check packaged mod examples and shader generation in a disposable tree."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True


def main():
    assert sorted(p.name for p in (ROOT / 'native/examples').iterdir()
                  if p.suffix in {'.c', '.h', '.frag'}) == ['level_cap_mod.c']
    with tempfile.TemporaryDirectory(prefix='rhh-package-examples-') as directory:
        work = Path(directory)
        for name in ('native', 'include', 'docs', 'PORTING.md', 'PORTING_PROGRESS.md',
                     'MODDING.md', 'MODS_PROGRESS.md', 'NEEDED_MOD_API_SERVICES.md'):
            source = ROOT / name
            (work / name).symlink_to(source, target_is_directory=source.is_dir())
        target = work / 'build/native/windows64'
        (target / 'music').mkdir(parents=True)
        library = b'test artifact\n'
        (target / 'rhhgme.dll').write_bytes(library)
        (target / 'music/manifest.json').write_text('{"files": []}\n')
        report = work / 'report.json'
        report.write_text(json.dumps({'target': 'windows64', 'artifacts': {
            'rhhgme.dll': hashlib.sha256(library).hexdigest()}}))
        spec = importlib.util.spec_from_file_location('package_examples_fixture', ROOT / 'native/package.py')
        package = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(package)
        package.ROOT = work
        sys.argv = ['package.py', '--target', 'windows64', '--reproducible-report', str(report),
                    '--output', str(work / 'dist')]
        package.main()
        archive = work / 'dist/pokeemerald-expansion-windows64.zip'
        with zipfile.ZipFile(archive) as z:
            stem = 'pokeemerald-expansion-windows64/'
            assert z.read(stem + 'NEEDED_MOD_API_SERVICES.md') == (ROOT / 'NEEDED_MOD_API_SERVICES.md').read_bytes()
            assert stem + 'HYPOTHETICAL_MODS.md' not in z.namelist()
            assert z.read(stem + 'examples/level_cap_mod.c') == (ROOT / 'native/examples/level_cap_mod.c').read_bytes()
            assert sorted(name for name in z.namelist()
                          if name.startswith(stem + 'examples/') and name.endswith('.c')
                          and '/mods/' not in name) == [stem + 'examples/level_cap_mod.c']
            z.extractall(work / 'extract')
        examples = work / 'extract/pokeemerald-expansion-windows64/examples'
        subprocess.run([sys.executable, str(examples / 'generate_shader.py')], check=True)
        assert not (examples / 'field_environment_shader.h').exists()
        assert (examples / 'mods/battle_environment_shader.h').read_bytes() == (ROOT / 'native/mods/battle_environment_shader.h').read_bytes()
        source = work / 'source/native'
        (source / 'mods').mkdir(parents=True)
        for relative in ('generate_shader.py', 'mods/battle_environment.frag'):
            (source / relative).write_bytes((ROOT / 'native' / relative).read_bytes())
        subprocess.run([sys.executable, str(source / 'generate_shader.py')], check=True)
        assert (source / 'mods/battle_environment_shader.h').read_bytes() == (ROOT / 'native/mods/battle_environment_shader.h').read_bytes()
        print('package: supported mod examples and packaged shader generator passed')


if __name__ == '__main__':
    main()
