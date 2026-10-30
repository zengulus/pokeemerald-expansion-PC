#!/usr/bin/env python3
"""Exercise follower catches with a completely isolated enabled build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix='rhh-follower-enabled-') as directory:
        work = Path(directory)
        follower = (ROOT / 'include/config/follower_npc.h').read_text()
        battle = (ROOT / 'include/config/battle.h').read_text()
        for old, new in (
            ('FNPC_ENABLE_NPC_FOLLOWERS                 FALSE', 'FNPC_ENABLE_NPC_FOLLOWERS                 TRUE'),
            ('FNPC_FLAG_PARTNER_WILD_BATTLES            0', 'FNPC_FLAG_PARTNER_WILD_BATTLES            2'),
        ):
            if follower.count(old) != 1:
                raise RuntimeError(f'configuration setting changed: {old}')
            follower = follower.replace(old, new)
        old, new = 'B_MULTI_HALF_TEAMS              FALSE', 'B_MULTI_HALF_TEAMS              TRUE'
        if battle.count(old) != 1:
            raise RuntimeError(f'configuration setting changed: {old}')
        battle = battle.replace(old, new)
        (work / 'follower.h').write_text(follower)
        (work / 'battle.h').write_text(battle)
        obj = work / 'build'
        sdl = subprocess.check_output(['sdl2-config', '--cflags'], text=True).strip()
        flags = (f'-include {work}/follower.h -include {work}/battle.h '
                 f'-iquote include -Inative/include -DMODERN=1 -DNATIVE -DPORTABLE '
                 f'-DRENDERER_EASY_DRAW -DTESTING=0 -DEMERALD -DNDEBUG -std=gnu17 {sdl} -I{obj}')
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64',
                                     'NODEP=1', f'OBJ_DIR={obj}', f'CPPFLAGS={flags}',
                                     '-j8', 'owned-identity-tests'], cwd=ROOT, stdout=log,
                                    stderr=subprocess.STDOUT)
        if result.returncode:
            tail = (work / 'build.log').read_text(errors='replace').splitlines()[-40:]
            raise RuntimeError('enabled follower build failed:\n' + '\n'.join(tail))
        module = work / 'probe.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra',
                        '-Werror', '-iquote', 'include', 'native/tests/owned_follower_probe.c',
                        '-o', str(module)], cwd=ROOT, check=True)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        binary = obj / 'owned-identity-tests'
        for mode in ('ledger', 'probe'):
            command = [str(binary), f'owned-follower-catch-{mode}', str(work / f'{mode}.sav')]
            if mode == 'probe':
                command.append(str(module))
            result = subprocess.run(command, cwd=ROOT, env=env, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if result.returncode or 'owned follower-catch ledger:' not in result.stdout:
                raise RuntimeError(f'enabled follower catch {mode} failed:\n{result.stdout}')
            if mode == 'probe' and ('OWNED_FOLLOWER_PROBE_BAD' in result.stdout
                                    or result.stdout.count('OWNED_FOLLOWER_CATCH_OK') != 6):
                raise RuntimeError(f'enabled follower ABI probe failed:\n{result.stdout}')
    print('Enabled follower catch and ABI probe passed; temporary build removed.')


if __name__ == '__main__':
    main()
