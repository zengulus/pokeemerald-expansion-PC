#!/usr/bin/env python3
"""Exercise genuine game input, battle, Save and fresh-process Continue."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / 'native/tests/replays'
PERSISTENT = ['map_group', 'map_num', 'x', 'y', 'save_counter', 'save_status',
              'name_hex', 'money', 'intro_state', 'wall_clock_set', 'rescued_birch', 'running_shoes', 'party']

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['linux32', 'linux64', 'windows32', 'windows64'], default='linux64')
    parser.add_argument('--output', type=Path, help='Keep replay artifacts in this directory')
    parser.add_argument('--wine-prefix', type=Path)
    parser.add_argument('--executable', type=Path, help='Alternate game binary, e.g. a sanitizer build')
    parser.add_argument('--timeout', type=int, default=180, help='Per-phase process timeout in seconds')
    parser.add_argument('--mods', action='store_true', help='Enable all bundled examples through the original New Game screen')
    parser.add_argument('--external-mods', action='store_true', help='Use external example libraries for writes and built-in examples for Continue')
    args = parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
        work = Path(tempfile.mkdtemp(prefix=f'{args.target}-', dir=args.output.resolve()))
        run(args, work)
    else:
        with tempfile.TemporaryDirectory(prefix=f'rhh-replay-{args.target}-') as temporary:
            run(args, Path(temporary))


def run(args, work):
    windows = args.target.startswith('windows')
    game = ROOT / f'build/native/{args.target}/pokeemerald-expansion{ ".exe" if windows else ""}'
    if args.executable:
        game = args.executable.resolve()
    require(game.exists(), f'Build {game} first')
    save = work / 'progress.sav'
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    if args.wine_prefix:
        args.wine_prefix.mkdir(parents=True, exist_ok=True)
        env['WINEPREFIX'] = str(args.wine_prefix.resolve())
    def path(p):
        return 'Z:' + str(p).replace('/', '\\') if windows else str(p)
    def phase(name, frames, fixture, module=None):
        report = work / f'{name}.json'
        trace = work / f'{name}.trace'
        command = (['wine'] if windows else []) + [str(game),
            '--fixed-rtc', '2026-10-28T12:00:00', '--frames', str(frames),
            '--save', path(save), '--input', path(FIXTURES / fixture),
            '--report', path(report), '--trace', path(trace),
            '--screenshot', path(work / f'{name}.bmp')]
        if args.mods: command += ['--mod-trace', path(work / f'{name}.jsonl')]
        if module: command += ['--mod', path(module)]
        if args.external_mods and name in ['new-game', 'clock', 'starter', 'save-again', 'options', 'environment-options', 'speed-options']:
            command += ['--external-bundled-mods', path(game.parent / 'example-mods')]
        log_path = work / f'{name}.log'
        try:
            with log_path.open('w') as log:
                subprocess.run(command, check=True, cwd=ROOT, env=env,
                               stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout)
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            print(f'{name}: last log output:\n{log_path.read_text(errors="replace")[-8192:]}',
                  file=sys.stderr, flush=True)
            raise
        data = json.loads(report.read_text())
        if save.exists():
            shutil.copyfile(save, work / f'{name}.sav')
        require(data['frames'] == frames, f'{name}: incomplete run')
        require(data['pointer_bits'] == (64 if args.target.endswith('64') else 32),
                f'{name}: wrong executable architecture')
        print(f'{name}: map={data["map_group"]},{data["map_num"]} '
              f'position={data["x"]},{data["y"]} save={data["save_counter"]} '
              f'party={data["party"]}', flush=True)
        return data, trace
    print(f'Artifacts: {work}' + ('' if args.output else ' (temporary)'), flush=True)
    initial, _ = phase('new-game', 16760 if args.mods else 15500,
                       'new-game-mods.input' if args.mods else 'new-game.input')
    require(initial['save_counter'] == 1 and initial['intro_state'] == 4,
            'New game did not reach the house and save')
    require(initial['running_shoes'] == 1, 'Running Shoes were not available at the start')
    require(save.stat().st_size > 80 and save.read_bytes()[:8] == b'RHHPCS01'
            and int.from_bytes(save.read_bytes()[8:12], 'little') == 2,
            'Unexpected native save container')
    clock, _ = phase('clock', 8000, 'clock.input')
    require(clock['save_counter'] == 2 and clock['wall_clock_set'] == 1
            and clock['intro_state'] == 7, 'Clock and TV story did not persist')
    compiler = ('x86_64-w64-mingw32-gcc' if args.target.endswith('64') else 'i686-w64-mingw32-gcc') if windows else 'gcc'
    run_flags = ['-m64' if args.target.endswith('64') else '-m32', '-shared', '-O2', '-iquote', 'include']
    if not windows: run_flags.append('-fPIC')
    seed_module = work / ('battle-seed-probe.dll' if windows else 'battle-seed-probe.so')
    subprocess.run([compiler, *run_flags, str(ROOT / 'native/tests/environment_seed_probe.c'), '-o', str(seed_module)],
                   check=True, cwd=ROOT, env=env, timeout=args.timeout)
    clock_save = save.read_bytes()
    seed_battle, _ = phase('battle-seed', 16000, 'starter.input', seed_module)
    seed_log = (work / 'battle-seed.log').read_text()
    require('BATTLE_SEED_PROBE_WEATHER' in seed_log and 'BATTLE_SEED_PROBE_TERRAIN' in seed_log
            and 'BATTLE_SEED_PROBE_BAD' not in seed_log and seed_battle['in_battle']
            and seed_battle['battle_environment']['weather'] == 2
            and seed_battle['battle_environment']['terrain'] == 1,
            'External battle-seed proposal did not reach real battle state')
    save.write_bytes(clock_save)
    battle, trace = phase('starter', 26500, 'starter.input')
    require('battle=1' in trace.read_text(), 'Replay never entered combat')
    require(battle['in_battle'] == 0 and battle['battle_outcome'] == 1,
            'Starter battle was not won')
    require(battle['save_counter'] == 3 and battle['rescued_birch'] == 1,
            'Battle story progress was not saved')
    require(len(battle['party']) == 1 and battle['party'][0]['species'] == (359 if args.mods else 255)
            and battle['party'][0]['level'] == 5, 'Starter party is incorrect')
    if args.mods:
        require(battle['mods']['flags'] == 31 and battle['mods']['wild_mode'] == 1
                and battle['mods']['music'] == 1 and battle['mods']['follower_present'],
                'Bundled example configuration or follower did not reach gameplay')
        records = [json.loads(line) for line in (work / 'starter.jsonl').read_text().splitlines()]
        ids = {record.get('mod') for record in records}
        require({'rhh:starter', 'rhh:equal_bst', 'rhh:battle_music', 'rhh:follower', 'rhh:level_cap'} <= ids,
                'Gameplay did not execute the independent example mods')
    loaded, _ = phase('continue', 1200, 'continue.input')
    require(all(loaded[key] == battle[key] for key in PERSISTENT),
            'Fresh-process Continue changed persistent gameplay state')
    require(loaded['world_steps'] == battle['world_steps'] and loaded['world_steps'] > 0,
            'World step clock did not survive Save and Continue')
    if args.mods:
        settings = ['flags', 'wild_mode', 'music', 'seed', 'starter_key', 'level_cap', 'run_failed']
        require(all(loaded['mods'][key] == battle['mods'][key] for key in settings),
                'Static/external example state did not survive Continue')
    scene_module = work / ('scene-probe.dll' if windows else 'scene-probe.so')
    subprocess.run([compiler, *run_flags, str(ROOT / 'native/tests/scene_probe.c'), '-o', str(scene_module)],
                   check=True, cwd=ROOT, env=env, timeout=args.timeout)
    scene, _ = phase('scene-snapshot', 1500, 'continue.input', scene_module)
    require('SCENE_PROBE_OK' in (work / 'scene-snapshot.log').read_text()
            and all(scene[key] == loaded[key] for key in PERSISTENT),
            'External scene module did not validate player anchors and sprite data on the real map')
    moving, _ = phase('scene-motion', 1338, 'options-movement.input', scene_module)
    require('SCENE_PROBE_MOVED' in (work / 'scene-motion.log').read_text()
            and 'SCENE_PROBE_BAD_MOTION' not in (work / 'scene-motion.log').read_text()
            and 'STEP_PROBE_OK' in (work / 'scene-motion.log').read_text()
            and 'STEP_PROBE_BAD' not in (work / 'scene-motion.log').read_text()
            and 'ENVIRONMENT_PROBE_OK' in (work / 'scene-motion.log').read_text()
            and 'ENVIRONMENT_PROBE_BAD' not in (work / 'scene-motion.log').read_text()
            and 'HABITAT_PROBE_OK' in (work / 'scene-motion.log').read_text()
            and 'HABITAT_PROBE_BAD' not in (work / 'scene-motion.log').read_text()
            and moving['world_steps'] > loaded['world_steps']
            and moving['save_counter'] == loaded['save_counter'],
            'External scene module did not track movement and committed world steps')
    written, _ = phase('save-again', 3000, 'save-party.input')
    require(written['save_counter'] == 4 and written['party'] == battle['party'],
            'Second Save did not preserve the party')
    reloaded, _ = phase('continue-again', 1200, 'continue.input')
    require(all(reloaded[key] == written[key] for key in PERSISTENT),
            'Second fresh-process Continue changed gameplay state')
    require(reloaded['world_steps'] == written['world_steps'], 'World step clock changed across reload')
    bag, trace = phase('bag', 1900, 'bag.input')
    nm = ('x86_64-w64-mingw32-nm' if args.target.endswith('64') else 'i686-w64-mingw32-nm') if windows else 'nm'
    symbols = subprocess.check_output([nm, '-n', str(game)], text=True)
    addresses = {line.split()[-1].lstrip('_'): int(line.split()[0], 16)
                 for line in symbols.splitlines() if len(line.split()) == 3}
    image_bias = int(bag['code_anchor'], 16) - addresses['AgbMain']
    address = addresses['CB2_BagMenuRun'] + image_bias
    callbacks = [int(line.split('cb=')[1].split()[0], 16)
                 for line in trace.read_text().splitlines() if 'cb=' in line]
    require(address in callbacks, 'Bag menu callback was not reached')
    require(all(bag[key] == written[key] for key in PERSISTENT),
            'Bag menu changed persistent gameplay state')
    summary, trace = phase('party-summary', 3000, 'party-summary.input')
    image_bias = int(summary['code_anchor'], 16) - addresses['AgbMain']
    callbacks = {int(line.split('cb=')[1].split()[0], 16) - image_bias
                 for line in trace.read_text().splitlines() if 'cb=' in line}
    require(addresses['CB2_UpdatePartyMenu'] in callbacks, 'Party menu was not reached')
    require(addresses['CB2_UpdateSummaryScreen'] in callbacks, 'Pokémon summary was not reached')
    require(all(summary[key] == written[key] for key in PERSISTENT),
            'Party/summary menus changed persistent gameplay state')
    if args.mods:
        options, _ = phase('options', 4000, 'options-music.input')
        require(options['save_counter'] == 5 and options['mods']['music'] == 2
                and options['party'] == written['party'],
                'Options did not change battle music and save the existing party')
        options_loaded, _ = phase('options-continue', 1200, 'continue.input')
        require(options_loaded['mods']['music'] == 2
                and all(options_loaded[key] == options[key] for key in PERSISTENT),
                'Options music choice did not survive fresh-process Continue')
        environment, _ = phase('environment-options', 4000, 'options-environment.input')
        require(environment['save_counter'] == 6
                and environment['battle_environment']['enabled']
                and environment['party'] == written['party'],
                'Options did not enable battle effects and save the existing party')
        environment_loaded, _ = phase('environment-continue', 1200, 'continue.input')
        require(environment_loaded['battle_environment']['enabled']
                and all(environment_loaded[key] == environment[key] for key in PERSISTENT),
                'Options battle effects choice did not survive fresh-process Continue')
        normal_movement, _ = phase('normal-movement', 1338, 'options-movement.input')
        speed, _ = phase('speed-options', 4000, 'options-speed.input')
        require(speed['save_counter'] == 7 and speed['speed'] == {'text': 3, 'overworld': 3, 'battle': 3}
                and speed['party'] == written['party'], 'Speed options did not save')
        speed_loaded, _ = phase('speed-continue', 1200, 'continue.input')
        require(speed_loaded['speed'] == speed['speed']
                and all(speed_loaded[key] == speed[key] for key in PERSISTENT),
                'Speed choices did not survive fresh-process Continue')
        fast_movement, _ = phase('fast-movement', 1338, 'options-movement.input')
        require(fast_movement['player']['x'] > normal_movement['player']['x'],
                'Faster overworld setting did not move the player farther')
        normal_steps = normal_movement['world_steps'] - environment_loaded['world_steps']
        fast_steps = fast_movement['world_steps'] - speed_loaded['world_steps']
        normal_distance = normal_movement['player']['x'] - environment_loaded['player']['x']
        fast_distance = fast_movement['player']['x'] - speed_loaded['player']['x']
        # The destination coordinate can already be the next tile while the
        # last movement has not yet reached its centre.
        require(normal_steps in (normal_distance, normal_distance - 1)
                and fast_steps in (fast_distance, fast_distance - 1)
                and fast_steps > normal_steps,
                'World step clock did not track completed movement at both speeds')
    print(f'{args.target}: real startup, maps, NPC scenes, won battle and repeated Save/Continue passed', flush=True)

if __name__ == '__main__':
    main()
