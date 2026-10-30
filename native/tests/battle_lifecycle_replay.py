#!/usr/bin/env python3
"""Observe a real story battle with a temporary game build; clean everything."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux64', 'windows64'], default='linux64')
    target = parser.parse_args().target
    windows = target == 'windows64'
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        with tempfile.TemporaryDirectory(prefix='rhh-battle-replay-') as directory:
            work = Path(directory)
            if windows:
                env['WINEPREFIX'] = str(work / 'wine')
            game = work / ('game.exe' if windows else 'game')
            build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', '-j8',
                                    f'GAME={game}', str(game)], cwd=ROOT, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if build.returncode:
                raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
            native_dir = ROOT / 'build/native' / target
            dependencies = ['SDL2.dll', 'rhhgme.dll'] if windows else ['librhhgme.so']
            for name in dependencies:
                source = native_dir / name
                if not source.is_file():
                    raise RuntimeError(f'Missing runtime dependency: {source}')
                (work / name).symlink_to(source)
            if (native_dir / 'music').is_dir():
                (work / 'music').symlink_to(native_dir / 'music', target_is_directory=True)
            module = work / ('probe.dll' if windows else 'probe.so')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows:
                flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/battle_lifecycle_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            def arg(path):
                return 'Z:' + str(path).replace('/', '\\') if windows else str(path)
            save = work / 'progress.sav'
            try:
                for phase, frames, fixture in [('new-game', 15500, 'new-game.input'),
                                                ('clock', 8000, 'clock.input'),
                                                ('battle', 26500, 'starter.input')]:
                    report = work / (phase + '.json')
                    command = (['wine'] if windows else []) + [str(game), '--fixed-rtc', '2026-10-28T12:00:00',
                        '--frames', str(frames), '--save', arg(save), '--input', arg(ROOT / 'native/tests/replays' / fixture),
                        '--report', arg(report)]
                    if phase == 'battle':
                        command += ['--mod', arg(module)]
                    result = subprocess.run(command, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                                            stderr=subprocess.STDOUT, text=True, timeout=240)
                    if result.returncode:
                        raise RuntimeError(f'{target} {phase} failed ({result.returncode}):\n{result.stdout[-5000:]}')
                    data = json.loads(report.read_text())
                    if phase == 'battle':
                        if (data['in_battle'] or data['battle_outcome'] != 1
                                or 'BATTLE_LIFECYCLE_BAD' in result.stdout
                                or result.stdout.count('BATTLE_LIFECYCLE_START') != 1
                                or result.stdout.count('BATTLE_LIFECYCLE_INITIAL') != 2
                                or result.stdout.count('BATTLE_LIFECYCLE_END') != 1
                                or result.stdout.count('BATTLE_LIFECYCLE_OUTCOME_1') != 1):
                            raise RuntimeError(f'{target} real battle lifecycle failed:\n{result.stdout[-5000:]}\n{data}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{target}: actual story battle emitted start, both initial entries and committed end')
    finally:
        subprocess.run(['make', '-s', '-f', 'Makefile.native', f'TARGET={target}', 'clean-test-builds'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == '__main__':
    main()
