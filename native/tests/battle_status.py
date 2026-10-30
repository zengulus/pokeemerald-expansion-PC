#!/usr/bin/env python3
"""Test committed battle status events in an isolated, disposable build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with tempfile.TemporaryDirectory(prefix='rhh-battle-status-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64', f'OBJ_DIR={build}', '-j8', 'gameplay-tests'],
                                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        flags = ['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror', '-iquote', 'include']
        innate, probe, reaction, health = work / 'innate.so', work / 'probe.so', work / 'reaction.so', work / 'health.so'
        for source, output in [('native/tests/innate_status_probe.c', innate), ('native/tests/battle_status_probe.c', probe),
                               ('native/tests/status_condition_probe.c', reaction), ('native/tests/battle_health_probe.c', health)]:
            subprocess.run([*flags, source, '-o', str(output)], cwd=ROOT, check=True)
        binary = build / 'gameplay-tests'
        for command, marker in [(['battle-health-observation', str(health)], 'battle health observation: HP'),
                                (['bag-status'], 'bag status: active and off-field item cures'),
                                (['battle-status', str(innate), str(probe)], 'battle status: committed setter'),
                                (['status-condition', str(reaction), str(innate)], 'status conditions: proposals'),
                                (['innate-status', str(innate)], 'innate status: normal status rules')]:
            result = subprocess.run([str(binary), *command], cwd=ROOT, env=env, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, timeout=120)
            if (result.returncode or marker not in result.stdout or 'INNATE_STATUS_BAD' in result.stdout
                or 'BATTLE_STATUS_EXTERNAL_BAD' in result.stdout or 'BATTLE_ITEM_STATUS_EXTERNAL_BAD' in result.stdout
                or 'STATUS_CONDITION_PROBE_BAD' in result.stdout or 'BATTLE_HEALTH_PROBE_BAD' in result.stdout
                or (command[0] == 'battle-status' and (result.stdout.count('BATTLE_STATUS_EXTERNAL_OK') != 14
                    or result.stdout.count('BATTLE_ITEM_STATUS_EXTERNAL_OK') != 7))
                or (command[0] == 'status-condition' and result.stdout.count('STATUS_CONDITION_PROBE_OK') != 35)):
                raise RuntimeError(f'{command[0]} failed ({result.returncode}):\n{result.stdout}')
    print('linux64: battle status events, condition reactions and innate paths passed; temporary files removed')

if __name__ == '__main__':
    main()
