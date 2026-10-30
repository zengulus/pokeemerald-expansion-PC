#!/usr/bin/env python3
"""Verify the condition ribbon in a real battle and remove every temporary build."""
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def bitmap_pixels(path):
    data = path.read_bytes()
    if data[:2] != b'BM':
        raise RuntimeError(f'{path} is not a bitmap')
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height, planes, bits = struct.unpack_from('<iiHH', data, 18)
    if width != 240 or abs(height) != 160 or planes != 1 or bits not in (16, 24, 32):
        raise RuntimeError(f'unexpected screenshot format: {width}x{height}, {bits} bits')
    stride = ((width * bits + 31) // 32) * 4
    size = bits // 8
    if len(data) < offset + stride * abs(height):
        raise RuntimeError('truncated screenshot')
    def pixel(x, y):
        row = height - 1 - y if height > 0 else y
        start = offset + row * stride + x * size
        return data[start:start + size]
    return pixel


def main():
    target = 'linux64'
    cache = [ROOT / 'build/native' / target / 'native' / f'{name}.{ext}'
             for name in ('mod_type_icons', 'mod_moves') for ext in ('o', 'd')]
    existed = {path: path.exists() for path in cache}
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    try:
        with tempfile.TemporaryDirectory(prefix='rhh-condition-replay-') as directory:
            work = Path(directory)
            game = work / 'game'
            build = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', '-j8',
                                    f'GAME={game}', str(game)], cwd=ROOT, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if build.returncode:
                raise RuntimeError('\n'.join(build.stdout.splitlines()[-60:]))
            native = ROOT / 'build/native' / target
            (work / 'librhhgme.so').symlink_to(native / 'librhhgme.so')
            if (native / 'music').is_dir():
                (work / 'music').symlink_to(native / 'music', target_is_directory=True)
            module = work / 'condition.so'
            subprocess.run(['gcc', '-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-fPIC',
                            '-iquote', 'include', 'native/tests/battle_condition_replay_probe.c', '-o', str(module)],
                           cwd=ROOT, check=True)
            save = work / 'progress.sav'
            def run(name, frames, fixture, use_module=False):
                report, screenshot = work / f'{name}.json', work / f'{name}.bmp'
                command = [str(game), '--fixed-rtc', '2026-10-28T12:00:00', '--frames', str(frames),
                           '--save', str(save), '--input', str(ROOT / 'native/tests/replays' / fixture),
                           '--report', str(report), '--screenshot', str(screenshot)]
                if use_module:
                    command += ['--mod', str(module)]
                result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, timeout=240)
                if result.returncode:
                    raise RuntimeError(f'{name} failed ({result.returncode}):\n{result.stdout[-5000:]}')
                return json.loads(report.read_text()), result.stdout, screenshot

            run('new-game', 15500, 'new-game.input')
            run('clock', 8000, 'clock.input')
            checkpoint = save.read_bytes()
            for frames in (16500, 17500, 18500, 19500, 21000, 23000):
                save.write_bytes(checkpoint)
                with_hud, log, hud_image = run('condition', frames, 'starter.input', True)
                if 'REPLAY_CONDITION_BAD' in log:
                    raise RuntimeError('condition could not be applied in the real battle')
                if with_hud['in_battle'] and 'REPLAY_CONDITION_SET' in log:
                    save.write_bytes(checkpoint)
                    plain, _, plain_image = run('plain', frames, 'starter.input')
                    if not plain['in_battle'] or plain['party'] != with_hud['party']:
                        raise RuntimeError('condition and reference runs reached different battle states')
                    hud, reference = bitmap_pixels(hud_image), bitmap_pixels(plain_image)
                    changed = sum(hud(x, y) != reference(x, y) for y in range(15) for x in range(240))
                    outside = sum(hud(x, y) != reference(x, y) for y in range(15, 160) for x in range(240))
                    if changed < 100 or outside:
                        raise RuntimeError(f'condition ribbon mismatch: {changed} top pixels, {outside} outside')
                    print(f'linux64: real battle condition ribbon captured at frame {frames}, outside scene unchanged')
                    break
            else:
                raise RuntimeError('replay did not capture an active condition during battle')
    finally:
        try:
            subprocess.run(['make', '-s', '-f', 'Makefile.native', f'TARGET={target}', 'clean-test-builds'],
                           cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        finally:
            for path in cache:
                if not existed[path]:
                    path.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
