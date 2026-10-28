#!/usr/bin/env python3
"""Build native subsystem tests from production C and run in fresh processes."""
import argparse
import os
from pathlib import Path
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def run(*args, **kwargs):
    subprocess.run([str(x) for x in args], check=True, cwd=ROOT, **kwargs)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux32', 'linux64', 'windows32', 'windows64'], default='linux64')
    parser.add_argument('--sanitize', action='store_true', help='Enable AddressSanitizer (linux64)')
    args = parser.parse_args()
    if args.sanitize and args.target != 'linux64':
        parser.error('--sanitize requires --target linux64')
    windows = args.target.startswith('windows')
    bits = '-m64' if args.target.endswith('64') else '-m32'
    compiler = ('x86_64-w64-mingw32-gcc' if bits == '-m64' else 'i686-w64-mingw32-gcc') if windows else 'gcc'
    flags = [bits, '-std=gnu17', '-O2', '-g', '-fno-strict-aliasing', '-fwrapv',
             '-mno-ms-bitfields', '-ffunction-sections', '-fdata-sections',
             '-DMODERN=1', '-DNATIVE', '-DTESTING=0', '-DEMERALD', '-DNDEBUG',
             '-iquote', 'include', '-Inative/include']
    # PE and sanitizer metadata retain unused tables/functions. Internalize
    # this isolated program before linking to keep only exercised routines.
    if windows or args.sanitize:
        flags.extend(['-flto', '-fwhole-program'])
    if args.sanitize:
        flags.extend(['-fsanitize=address', '-fno-omit-frame-pointer'])
    with tempfile.TemporaryDirectory(prefix='rhh-native-tests-') as directory:
        work = Path(directory)
        def build(name, *sources):
            output = work / (name + ('.exe' if windows else ''))
            run(compiler, *flags, *sources, 'native/diagnostics.c', '-Wl,--gc-sections', '-o', output)
            return output
        def preprocess(source, name):
            prepared = work / (name + '.i')
            with prepared.open('w') as output:
                first = subprocess.Popen([compiler, *flags, '-E', source], stdout=subprocess.PIPE, cwd=ROOT)
                subprocess.run(['tools/preproc/preproc', '-ni', '-g', 'build/assets', source, 'charmap.txt'],
                               stdin=first.stdout, stdout=output, cwd=ROOT, check=True)
                first.stdout.close()
                if first.wait(): raise RuntimeError(f'{source} preprocessing failed')
            return prepared
        platform = build('platform', 'native/memory.c', 'native/save.c', 'native/mod_host.c', 'native/tests/platform_test.c')
        mods = build('mod-host', 'native/mod_host.c', 'native/tests/mod_host_test.c')
        mod_save = build('mod-save', 'native/memory.c', 'native/save.c', 'native/mod_host.c', 'native/tests/mod_save_test.c')
        rng = build('rng', 'src/random.c', 'native/tests/rng_test.c')
        rtc = build('rtc', 'native/rtc.c', 'native/tests/rtc_test.c')
        tasks = build('tasks', 'src/task.c', 'native/tests/task_test.c')
        operands = build('script-operands', 'src/script.c', 'native/tests/script_read_test.c')
        contest = build('contest-operands', 'native/tests/contest_script_test.c')
        allocator = build('allocator', 'native/allocator.c', 'native/memory.c',
                          'src/dma3_manager.c', 'native/tests/allocator_test.c')
        save_layout = build('save-layout', 'native/memory.c', 'native/save.c', 'native/mod_host.c', 'native/tests/save_layout_test.c')
        transfers = build('transfers', 'native/memory.c', 'src/dma3_manager.c', 'src/bg.c',
                          'src/gpu_regs.c', 'native/allocator.c', 'native/tests/graphics_transfer_test.c')
        # Compile the actual RHH implementation, with only asset-specific
        # unreachable functions removed by linker section collection.
        prepared = preprocess('src/decompress.c', 'decompress')
        decompressor = build('decompress', prepared, 'native/memory.c', 'native/tests/decompress_test.c')
        sprites = build('sprites', preprocess('src/battle_anim_mons.c', 'battle-anim-mons'),
                        'native/tests/sprite_callback_test.c')
        glyphs = build('glyphs', preprocess('native/tests/glyph_test.c', 'glyphs'))
        doors = build('doors', preprocess('native/tests/door_palette_test.c', 'doors'))
        env = os.environ.copy()
        runner = []
        if windows:
            prefix = work / 'wine'
            prefix.mkdir()
            env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all')
            runner = ['wine']
        def execute(exe, *arguments):
            run(*runner, exe, *arguments, env=env)
        save = work / 'progress.sav'
        def target_path(path):
            return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
        execute(platform, 'write', target_path(save))
        execute(platform, 'read', target_path(save))
        saved = save.read_bytes()
        assert len(saved) == 131072 + 32 and saved[:8] == b'RHHNATV\0'
        invalid = [saved[:24], saved[:-1], saved + b'\0', saved[32:]]
        for offset in [0, 8, 12, 16, 20, 24, 28]:
            changed = bytearray(saved)
            changed[offset] ^= 1
            invalid.append(changed)
        for i, contents in enumerate(invalid):
            bad = work / f'incompatible-{i}.sav'
            bad.write_bytes(contents)
            execute(platform, 'reject', target_path(bad))
            assert bad.read_bytes() == contents, 'Rejected save was modified'
        execute(rng)
        execute(mods)
        execute(mod_save, 'legacy', target_path(save))
        execute(mod_save, 'legacy-read', target_path(save))
        mod_path = work / 'mods.sav'
        execute(mod_save, 'write', target_path(mod_path))
        mod_bytes = mod_path.read_bytes()
        execute(mod_save, 'read', target_path(mod_path))
        assert mod_path.read_bytes() == mod_bytes, 'Aborted transaction changed the save'
        execute(mod_save, 'missing', target_path(mod_path))
        assert mod_path.read_bytes() == mod_bytes, 'Missing module changed the save'
        (work / 'mods.sav.tmp').mkdir()
        execute(mod_save, 'fail', target_path(mod_path))
        assert mod_path.read_bytes() == mod_bytes, 'Failed atomic commit changed the save'
        execute(rtc)
        execute(tasks)
        execute(sprites)
        execute(glyphs)
        execute(doors)
        execute(operands)
        execute(contest)
        execute(allocator)
        execute(save_layout)
        execute(transfers)
        generator = random.Random(0x524848)
        fixtures = [bytes(512), bytes(range(256)) * 4,
                    bytes(generator.randrange(16) for _ in range(1024)),
                    bytes(generator.randrange(256) for _ in range(1024))]
        modes = set()
        for i, data in enumerate(fixtures):
            raw = work / f'fixture-{i}.bin'
            raw.write_bytes(data)
            for suffix, options in [('smol', []), ('fastSmol', ['false', 'false', 'false'])]:
                packed = work / f'fixture-{i}.{suffix}'
                run('tools/compresSmol/compresSmol', '-w', raw, packed, *options,
                    stdout=subprocess.DEVNULL)
                modes.add(packed.read_bytes()[0] & 15)
                execute(decompressor, target_path(raw), target_path(packed))
        # Validate every mode present in the game's generated assets against its
        # uncompressed source, not just the compressor's choices on tiny inputs.
        assets = {}
        for packed in (ROOT / 'build/assets').rglob('*.smol'):
            mode = packed.read_bytes()[0] & 15
            raw = packed.with_suffix('')
            if mode not in assets and raw.exists():
                assets[mode] = (raw, packed)
        for mode, (raw, packed) in sorted(assets.items()):
            execute(decompressor, target_path(raw), target_path(packed))
            modes.add(mode)
        if windows:
            # Wine may still be flushing its private prefix after its last
            # process exits. Let it finish before removing the test directory.
            run('wineserver', '-w', env=env, timeout=30)
        print(f'{args.target}: platform, RTC, fresh-process save/reload, RHH RNG, '
              f'8 compression roundtrips and {len(assets)} game asset modes passed; modes={sorted(modes)}')

if __name__ == '__main__':
    main()
