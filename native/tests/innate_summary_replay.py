#!/usr/bin/env python3
"""Exercise the real summary screen and remove temporary builds and screenshots."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from species_types_replay import bitmap

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inspect', action='store_true')
    args = parser.parse_args()
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    try:
        with tempfile.TemporaryDirectory(prefix='rhh-innate-summary-') as directory:
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
            module = work / 'innates.so'
            subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-iquote', 'include', 'native/tests/innate_summary_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            summary_input = (ROOT / 'native/tests/replays/party-summary.input').read_text().splitlines()
            prefix = '\n'.join(line for line in summary_input if int(line.split()[0]) < 2100) + '\n'
            for name, suffix in [('base', ''), ('open', '2200 4\n2201 0\n'),
                                 ('closed', '2200 4\n2201 0\n2300 4\n2301 0\n')]:
                (work / (name + '.input')).write_text(prefix + suffix)
            save = work / 'progress.sav'
            def run(phase, frames, fixture, mod=False, screenshot=False):
                report, trace = work / (phase + '.json'), work / (phase + '.trace')
                cmd = [str(game), '--fixed-rtc', '2026-10-28T12:00:00', '--frames', str(frames),
                       '--save', str(save), '--input', str(fixture), '--report', str(report), '--trace', str(trace)]
                if mod: cmd += ['--mod', str(module)]
                if screenshot: cmd += ['--screenshot', str(work / (phase + '.bmp'))]
                result = subprocess.run(cmd, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                        text=True, timeout=240)
                if result.returncode or 'SUMMARY_INNATE_BAD' in result.stdout:
                    raise RuntimeError(f'{phase} failed ({result.returncode}):\n{result.stdout[-5000:]}')
                return json.loads(report.read_text()), result.stdout, trace
            fixtures = ROOT / 'native/tests/replays'
            run('new-game', 15500, fixtures / 'new-game.input')
            run('clock', 8000, fixtures / 'clock.input')
            battle, _, _ = run('battle', 26500, fixtures / 'starter.input', mod=True)
            if battle['in_battle'] or battle['battle_outcome'] != 1 or not battle['rescued_birch']:
                raise RuntimeError(f'Could not reach the summary: {battle}')
            runs = [run(name, 2400, work / (name + '.input'), True, True) for name in ('base', 'open', 'closed')]
            symbols = subprocess.check_output(['nm', '-n', str(game)], text=True)
            addresses = {line.split()[-1]: int(line.split()[0], 16)
                         for line in symbols.splitlines() if len(line.split()) == 3}
            for name, (report, output, trace) in zip(('base', 'open', 'closed'), runs):
                bias = int(report['code_anchor'], 16) - addresses['AgbMain']
                callbacks = [int(line.split('cb=')[1].split()[0], 16) - bias
                             for line in trace.read_text().splitlines() if 'cb=' in line]
                if (not callbacks or callbacks[-1] != addresses['CB2_UpdateSummaryScreen']
                        or 'SUMMARY_INNATE_OFFERED' not in output or 'SUMMARY_PERSONAL_OFFERED' not in output):
                    raise RuntimeError(f'{name}: expected an innate proposal on the actual summary screen; '
                                       f'callback={callbacks[-1:]}, expected={addresses["CB2_UpdateSummaryScreen"]}, '
                                       f'output={output[-2000:]}')
            base, opened, closed = [bitmap(work / (name + '.bmp')) for name in ('base', 'open', 'closed')]
            changed = sum(base(x, y) != opened(x, y) for y in range(72, 160) for x in range(88, 232))
            restored = sum(base(x, y) != closed(x, y) for y in range(72, 160) for x in range(88, 232))
            if changed < 100 or restored:
                raise RuntimeError(f'Summary toggle did not redraw and restore its content: {changed} changed, {restored} unrestored')
            print(f'Actual summary toggle passed: {changed} pixels changed and restored. Screenshots: {work}', flush=True)
            if args.inspect: input('Press Enter to remove temporary build and screenshots. ')
    finally:
        subprocess.run(['make', '-s', '-f', 'Makefile.native', 'TARGET=linux64', 'clean-test-builds'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

if __name__ == '__main__':
    main()
