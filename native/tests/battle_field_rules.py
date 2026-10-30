#!/usr/bin/env python3
"""Check battle field-rule snapshots and an independent native module."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix="rhh-battle-field-") as directory:
        work = Path(directory)
        build = work / "build"
        with (work / "build.log").open("w") as log:
            result = subprocess.run(["make", "-f", "Makefile.native", "TARGET=linux64",
                                     f"OBJ_DIR={build}", "-j8", "gameplay-tests"], cwd=ROOT,
                                    stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError("\n".join((work / "build.log").read_text(errors="replace").splitlines()[-60:]))
        module = work / "probe.so"
        subprocess.run(["gcc", "-m64", "-shared", "-fPIC", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-iquote", "include", "native/tests/battle_field_probe.c", "-o", str(module)], cwd=ROOT, check=True)
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
        result = subprocess.run([str(build / "gameplay-tests"), "battle-field-rules", str(module)], cwd=ROOT,
                                env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if (result.returncode or "battle field rules: flags, counters, contexts and exclusions passed" not in result.stdout
            or result.stdout.count("BATTLE_FIELD_EXTERNAL_OK") != 1 or "BATTLE_FIELD_EXTERNAL_BAD" in result.stdout
            or result.stdout.count("BATTLE_FIELD_CHANGE_OK") != 2 or "BATTLE_FIELD_CHANGE_BAD" in result.stdout
            or result.stdout.count("BATTLE_FIELD_COMMIT_OK") != 18 or "BATTLE_FIELD_COMMIT_BAD" in result.stdout):
            raise RuntimeError(f"battle field rules failed ({result.returncode}):\n{result.stdout}")
        effect_module = work / "effect-probe.so"
        subprocess.run(["gcc", "-m64", "-shared", "-fPIC", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-iquote", "include", "native/tests/field_rule_effect_probe.c", "-o", str(effect_module)], cwd=ROOT, check=True)
        effect = subprocess.run([str(build / "gameplay-tests"), "battle-field-effect", str(effect_module)], cwd=ROOT,
                                env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if (effect.returncode or "battle field effect: queued engine effects, validation and stale participants passed" not in effect.stdout
            or effect.stdout.count("FIELD_EFFECT_OFFERED") != 16 or "FIELD_EFFECT_BAD" in effect.stdout
            or effect.stdout.count("FIELD_AI_PREVIEW") != 11 or effect.stdout.count("FIELD_AI_SCORE") != 11
            or "FIELD_AI_BAD" in effect.stdout):
            raise RuntimeError(f"battle field effect failed ({effect.returncode}):\n{effect.stdout}")
        # Build a separate Gen V Sport configuration without changing repository settings.
        override = work / "legacy-sport.h"
        override.write_text('#include "config/battle.h"\n#undef B_SPORT_TURNS\n#define B_SPORT_TURNS GEN_5\n')
        legacy = work / "legacy-build"
        sdl_flags = subprocess.check_output(["sdl2-config", "--cflags"], text=True).strip()
        flags = (f"-iquote include -Inative/include -DMODERN=1 -DNATIVE -DPORTABLE -DRENDERER_EASY_DRAW "
                 f"-DTESTING=0 -DEMERALD -DNDEBUG -std=gnu17 {sdl_flags} -I{shlex.quote(str(legacy))} "
                 f"-include {shlex.quote(str(override))}")
        with (work / "legacy-build.log").open("w") as log:
            result = subprocess.run(["make", "-f", "Makefile.native", "TARGET=linux64", f"OBJ_DIR={legacy}",
                                     f"CPPFLAGS={flags}", "-j8", "gameplay-tests"], cwd=ROOT,
                                    stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError("\n".join((work / "legacy-build.log").read_text(errors="replace").splitlines()[-60:]))
        result = subprocess.run([str(legacy / "gameplay-tests"), "battle-field-legacy-sport", str(effect_module)],
                                cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if (result.returncode or "battle field effect: legacy per-battler sports and AI passed" not in result.stdout
            or result.stdout.count("FIELD_EFFECT_OFFERED") != 2 or "FIELD_EFFECT_BAD" in result.stdout
            or result.stdout.count("FIELD_AI_PREVIEW") != 2 or result.stdout.count("FIELD_AI_SCORE") != 2
            or "FIELD_AI_BAD" in result.stdout or result.stdout.count("SPORTS_EXTERNAL_OK") != 2
            or "SPORTS_EXTERNAL_BAD" in result.stdout):
            raise RuntimeError(f"legacy Sport effects failed ({result.returncode}):\n{result.stdout}")
    print("linux64: current and legacy battle field rules passed; temporary files removed")


if __name__ == "__main__":
    main()
