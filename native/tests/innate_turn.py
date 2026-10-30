#!/usr/bin/env python3
"""Exercise completed-turn innate effects and remove newly created build files."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=('linux64', 'linux32', 'windows64'), default='linux64')
    target = parser.parse_args().target
    windows = target == 'windows64'
    root = ROOT / 'build/native' / target
    before = set(root.rglob('*'))
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', WINEDEBUG='-all')
    try:
        result = subprocess.run(['make', '-f', 'Makefile.native', f'TARGET={target}', '-j8', 'gameplay-tests'],
                                cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if result.returncode:
            raise RuntimeError('\n'.join(result.stdout.splitlines()[-60:]))
        with tempfile.TemporaryDirectory(prefix='rhh-innate-turn-') as directory:
            module = Path(directory) / ('probe.dll' if windows else 'probe.so')
            actions = Path(directory) / ('actions.dll' if windows else 'actions.so')
            sourced = Path(directory) / ('sourced.dll' if windows else 'sourced.so')
            ai_entry = Path(directory) / ('ai-entry.dll' if windows else 'ai-entry.so')
            status = Path(directory) / ('status.dll' if windows else 'status.so')
            compiler = 'x86_64-w64-mingw32-gcc' if windows else 'gcc'
            flags = ['-m32' if target == 'linux32' else '-m64', '-shared', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
            if not windows: flags.append('-fPIC')
            subprocess.run([compiler, *flags, 'native/tests/innate_turn_probe.c', '-o', str(module)], cwd=ROOT, check=True)
            subprocess.run([compiler, *flags, 'native/tests/innate_turn_actions_probe.c', '-o', str(actions)], cwd=ROOT, check=True)
            subprocess.run([compiler, *flags, 'native/tests/innate_sourced_stat_probe.c', '-o', str(sourced)], cwd=ROOT, check=True)
            subprocess.run([compiler, *flags, 'native/tests/innate_ai_entry_probe.c', '-o', str(ai_entry)], cwd=ROOT, check=True)
            subprocess.run([compiler, *flags, 'native/tests/innate_status_probe.c', '-o', str(status)], cwd=ROOT, check=True)
            if windows: env['WINEPREFIX'] = str(Path(directory) / 'wine')
            arg = 'Z:' + str(module).replace('/', '\\') if windows else str(module)
            command = (['wine'] if windows else []) + [str(root / ('gameplay-tests.exe' if windows else 'gameplay-tests')), 'innate-turn', arg]
            try:
                run = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                expected = ('INNATE_TURN_TRIGGER 1', 'INNATE_TURN_TRIGGER 0', 'INNATE_TURN_COMPLETE 1',
                            'INNATE_TURN_COMPLETE 2', 'innate turn: ordering, suppression, conditions and messages passed')
                if (run.returncode or 'INNATE_TURN_BAD' in run.stdout or any(run.stdout.count(m) != 1 for m in expected)
                        or [run.stdout.index(m) for m in expected[:3]] != sorted(run.stdout.index(m) for m in expected[:3])):
                    raise RuntimeError(f'{target} failed ({run.returncode}):\n{run.stdout}')
                action_arg = 'Z:' + str(actions).replace('/', '\\') if windows else str(actions)
                action_command = (['wine'] if windows else []) + [str(root / ('gameplay-tests.exe' if windows else 'gameplay-tests')), 'innate-turn-actions', action_arg]
                run = subprocess.run(action_command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                markers = ('INNATE_ACTION_TRIGGER 1 1', 'INNATE_ACTION_TRIGGER 1 0', 'INNATE_ACTION_COMPLETE 1',
                           'INNATE_ACTION_TRIGGER 2 0', 'INNATE_ACTION_COMPLETE 2', 'INNATE_ACTION_TRIGGER 3 0',
                           'INNATE_ACTION_COMPLETE 3', 'INNATE_ACTION_TRIGGER 4 0', 'INNATE_ACTION_COMPLETE 4',
                           'INNATE_ACTION_TRIGGER 5 0', 'INNATE_ACTION_TARGET 6', 'INNATE_ACTION_COMPLETE 6',
                           'INNATE_ACTION_TARGET 7', 'INNATE_ACTION_COMPLETE 7', 'INNATE_ACTION_TARGET 8',
                           'INNATE_ACTION_COMPLETE 8', 'INNATE_ACTION_TARGET 9', 'INNATE_ACTION_COMPLETE 9',
                           'innate turn actions: healing, stats, staleness and capacity passed')
                if (run.returncode or 'INNATE_ACTION_BAD' in run.stdout or 'INNATE_ACTION_COMPLETE 5' in run.stdout
                        or any(run.stdout.count(m) != 1 for m in markers)
                        or [run.stdout.index(m) for m in markers] != sorted(run.stdout.index(m) for m in markers)):
                    raise RuntimeError(f'{target} actions failed ({run.returncode}):\n{run.stdout}')
                sourced_arg = 'Z:' + str(sourced).replace('/', '\\') if windows else str(sourced)
                sourced_command = (['wine'] if windows else []) + [str(root / ('gameplay-tests.exe' if windows else 'gameplay-tests')), 'innate-sourced-stat', sourced_arg]
                run = subprocess.run(sourced_command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if (run.returncode or 'SOURCED_STAT_BAD' in run.stdout
                        or 'innate sourced stat: protection, reflection and substitute passed' not in run.stdout):
                    raise RuntimeError(f'{target} sourced stats failed ({run.returncode}):\n{run.stdout}')
                status_arg = 'Z:' + str(status).replace('/', '\\') if windows else str(status)
                status_command = (['wine'] if windows else []) + [str(root / ('gameplay-tests.exe' if windows else 'gameplay-tests')), 'innate-status', status_arg]
                run = subprocess.run(status_command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if run.returncode or 'INNATE_STATUS_BAD' in run.stdout or 'innate status: normal status rules, source and recipient checks passed' not in run.stdout:
                    raise RuntimeError(f'{target} innate status failed ({run.returncode}):\n{run.stdout}')
                ai_arg = 'Z:' + str(ai_entry).replace('/', '\\') if windows else str(ai_entry)
                ai_command = (['wine'] if windows else []) + [str(root / ('gameplay-tests.exe' if windows else 'gameplay-tests')), 'innate-ai-entry', ai_arg]
                run = subprocess.run(ai_command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if (run.returncode or 'AI_ENTRY_BAD' in run.stdout or run.stdout.count('AI_ENTRY_PREVIEW 1 0') != 3
                        or run.stdout.count('AI_ENTRY_PREVIEW 1 1') != 3
                        or 'AI_ENTRY_SWITCH_SELECTED' not in run.stdout
                        or 'AI_ENTRY_HAZARD_ONCE' not in run.stdout
                        or 'AI_ENTRY_HEALING_WISH' not in run.stdout
                        or 'AI_ENTRY_OPPONENT_STAT_SWITCH_SELECTED' not in run.stdout
                        or 'AI_ENTRY_OPPONENT_STATUS_SWITCH_SELECTED' not in run.stdout
                        or 'AI_ENTRY_SYNCHRONIZE_SWITCH_SELECTED' not in run.stdout
                        or 'AI_ENTRY_STATUS_CANDIDATE 1' not in run.stdout
                        or 'AI_ENTRY_STATUS_CANDIDATE 2' not in run.stdout
                        or 'AI_TURN_HEAL_SWITCH_SELECTED' not in run.stdout
                        or 'AI_TURN_THRESHOLD_SWITCH_SELECTED' not in run.stdout
                        or 'AI_TURN_SASH_ONCE' not in run.stdout
                        or 'AI_ENTRY_OPPONENT_CANDIDATE 1' not in run.stdout
                        or 'AI_ENTRY_OPPONENT_CANDIDATE 2' not in run.stdout
                        or 'innate AI entry: scoped prediction, suppression and purity passed' not in run.stdout):
                    raise RuntimeError(f'{target} AI entry failed ({run.returncode}):\n{run.stdout}')
            finally:
                if windows:
                    subprocess.run(['wineserver', '-k'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f'{target}: completed-turn innate effects passed')
    finally:
        for path in sorted(root.rglob('*'), key=lambda p: len(p.parts), reverse=True):
            if (path.is_file() or path.is_symlink()) and path not in before: path.unlink()
            elif path.is_dir() and path not in before:
                try: path.rmdir()
                except OSError: pass


if __name__ == '__main__':
    main()
