#!/usr/bin/env python3
"""Check species types in a real battle and summary; remove every test artifact."""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def bitmap(path):
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bits = struct.unpack_from('<H', data, 28)[0]
    if width != 240 or abs(height) != 160 or bits not in (24, 32):
        raise RuntimeError(f'Unexpected screenshot format: {width}x{height}, {bits} bits')
    stride = ((width * bits + 31) // 32) * 4
    size = bits // 8
    def pixel(x, y):
        row = 159 - y if height > 0 else y
        start = offset + row * stride + x * size
        return data[start:start + 3]
    return pixel

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inspect', action='store_true', help='Pause with temporary screenshots available for inspection')
    args = parser.parse_args()
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    try:
        with tempfile.TemporaryDirectory(prefix='rhh-species-replay-') as directory:
            work = Path(directory)
            game = work / 'game'
            build = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', '-j8',
                                    f'GAME={game}', str(game)], cwd=ROOT, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if build.returncode:
                raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
            native = ROOT / 'build/native/linux64'
            (work / 'librhhgme.so').symlink_to(native / 'librhhgme.so')
            if (native / 'music').is_dir():
                (work / 'music').symlink_to(native / 'music', target_is_directory=True)
            module = work / 'types.so'
            subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-iquote', 'include', 'native/tests/species_types_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            save = work / 'progress.sav'
            def run(phase, frames, fixture, mod=False, screenshot=False):
                report, trace = work / (phase + '.json'), work / (phase + '.trace')
                cmd = [str(game), '--fixed-rtc', '2026-10-28T12:00:00', '--frames', str(frames),
                       '--save', str(save), '--input', str(ROOT / 'native/tests/replays' / fixture),
                       '--report', str(report), '--trace', str(trace)]
                if mod:
                    cmd += ['--mod', str(module)]
                if screenshot:
                    cmd += ['--screenshot', str(work / (phase + '.bmp'))]
                result = subprocess.run(cmd, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                        text=True, timeout=240)
                if result.returncode or 'SPECIES_TYPES_BAD' in result.stdout:
                    raise RuntimeError(f'{phase} failed ({result.returncode}):\n{result.stdout[-5000:]}')
                return json.loads(report.read_text()), result.stdout, trace
            run('new-game', 15500, 'new-game.input')
            run('clock', 8000, 'clock.input')
            battle, log, _ = run('battle', 26500, 'starter.input', mod=True)
            if battle['in_battle'] or battle['battle_outcome'] != 1 or not battle['rescued_birch'] or 'SPECIES_TYPES_ENTERED' not in log:
                raise RuntimeError(f'Actual battle did not use the species assignment: {battle}\n{log[-3000:]}')
            base, _, base_trace = run('summary-base', 2100, 'party-summary.input', screenshot=True)
            mod, _, mod_trace = run('summary-mod', 2100, 'party-summary.input', mod=True, screenshot=True)
            symbols = subprocess.check_output(['nm', '-n', str(game)], text=True)
            addresses = {line.split()[-1]: int(line.split()[0], 16)
                         for line in symbols.splitlines() if len(line.split()) == 3}
            for report, trace in ((base, base_trace), (mod, mod_trace)):
                bias = int(report['code_anchor'], 16) - addresses['AgbMain']
                callbacks = [int(line.split('cb=')[1].split()[0], 16) - bias
                             for line in trace.read_text().splitlines() if 'cb=' in line]
                if not callbacks or callbacks[-1] != addresses['CB2_UpdateSummaryScreen']:
                    raise RuntimeError('The screenshot was not taken on the Pokémon summary screen')
            a, b = bitmap(work / 'summary-base.bmp'), bitmap(work / 'summary-mod.bmp')
            changed = sum(a(x, y) != b(x, y) for y in range(48, 64) for x in range(112, 240))
            other = sum(a(x, y) != b(x, y) for y in range(160) for x in range(240)
                        if not (112 <= x < 240 and 48 <= y < 64))
            if changed < 100 or other:
                raise RuntimeError(f'Unexpected summary changes: {changed} type pixels, {other} elsewhere')
            print(f'Actual battle and summary passed; {changed} type pixels changed. Preview: {work / "summary-mod.bmp"}', flush=True)
            if args.inspect:
                input('Press Enter to remove the temporary preview and build. ')
    finally:
        subprocess.run(['make', '-s', '-f', 'Makefile.native', 'TARGET=linux64', 'clean-test-builds'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

if __name__ == '__main__':
    main()
