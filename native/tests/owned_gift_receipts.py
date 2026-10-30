#!/usr/bin/env python3
"""Check owned transition tracking and failure paths in a disposable build."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with tempfile.TemporaryDirectory(prefix='rhh-owned-gifts-') as directory:
        work = Path(directory)
        build = work / 'build'
        with (work / 'build.log').open('w') as log:
            result = subprocess.run(['make', '-f', 'Makefile.native', 'TARGET=linux64',
                                     f'OBJ_DIR={build}', '-j8', 'owned-identity-tests'],
                                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError('\n'.join((work / 'build.log').read_text(errors='replace').splitlines()[-60:]))
        for mode, marker in [('owned-world-ledger', 'owned world ledger: seed, reorder, daycare, fusion, gift and evolution passed'),
                             ('owned-consistency-ledger', 'owned consistency ledger: activation and save occupancy checks passed'),
                             ('owned-identity-write-audit', 'owned identity write audit: transient identity and occupancy changes fail closed'),
                             ('owned-trade-ledger', 'owned trade ledger: PC staging, link commit and rollback passed'),
                             ('owned-catch-ledger', 'owned catch ledger: displacement, acquisition and unsafe battle invalidation passed'),
                             ('owned-follower-catch-ledger', 'owned follower-catch ledger: full party and selected party reconciliation passed'),
                             ('owned-pc-ledger', 'owned PC ledger: pickup, reused slots, shift, group, release and fail-closed passed')]:
            result = subprocess.run([str(build / 'owned-identity-tests'), mode, str(work / f'{mode}.sav')],
                                    cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, timeout=120)
            if result.returncode or marker not in result.stdout:
                raise RuntimeError(f'{mode} failed ({result.returncode}):\n{result.stdout}')
        probe = work / 'identity-probe.so'
        subprocess.run(['gcc', '-m64', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-iquote', 'include', 'native/tests/owned_identity_probe.c', '-o', str(probe)],
                       cwd=ROOT, check=True)
        result = subprocess.run([str(build / 'owned-identity-tests'), 'owned-identity',
                                 str(work / 'identity.sav'), str(probe)], cwd=ROOT, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if (result.returncode or 'OWNED_IDENTITY_BAD' in result.stdout
                or 'owned identity: hidden locations, corrupt collisions and safe queries passed' not in result.stdout
                or result.stdout.count('OWNED_GIFT_OBSERVED') != 6
                or result.stdout.count('OWNED_DEBUG_CREATED_OBSERVED') != 2
                or result.stdout.count('OWNED_MYSTERY_GIFT_OBSERVED') != 1
                or result.stdout.count('OWNED_INGAME_TRADE_OBSERVED') != 3):
            raise RuntimeError(f'owned receipt observer failed ({result.returncode}):\n{result.stdout}')
    print('linux64: owned transition receipts and storage ledger passed; temporary files removed')


if __name__ == '__main__':
    main()
