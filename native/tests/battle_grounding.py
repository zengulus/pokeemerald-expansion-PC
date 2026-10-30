#!/usr/bin/env python3
"""Check live and AI terrain grounding through an independent module."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix="rhh-battle-grounding-") as directory:
        work = Path(directory)
        build = work / "build"
        with (work / "build.log").open("w") as log:
            result = subprocess.run(["make", "-f", "Makefile.native", "TARGET=linux64",
                                     f"OBJ_DIR={build}", "-j8", "gameplay-tests"], cwd=ROOT,
                                    stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError("\n".join((work / "build.log").read_text(errors="replace").splitlines()[-60:]))
        module = work / "grounding.so"
        subprocess.run(["gcc", "-m64", "-shared", "-fPIC", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-iquote", "include", "native/tests/battle_grounding_probe.c", "-o", str(module)], cwd=ROOT, check=True)
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
        result = subprocess.run([str(build / "gameplay-tests"), "battle-grounding", str(module)], cwd=ROOT,
                                env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if (result.returncode or "battle grounding: live and AI terrain eligibility passed" not in result.stdout
            or result.stdout.count("GROUNDING_OK") != 14 or "GROUNDING_BAD" in result.stdout):
            raise RuntimeError(f"battle grounding failed ({result.returncode}):\n{result.stdout}")
        entry = work / "entry.so"
        subprocess.run(["gcc", "-m64", "-shared", "-fPIC", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-iquote", "include", "native/tests/innate_ai_entry_probe.c", "-o", str(entry)], cwd=ROOT, check=True)
        result = subprocess.run([str(build / "gameplay-tests"), "innate-ai-entry", str(entry)], cwd=ROOT,
                                env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if (result.returncode or "innate AI entry: scoped prediction, suppression and purity passed" not in result.stdout
            or result.stdout.count("AI_ENTRY_GROUNDING_OK") != 13 or "AI_ENTRY_BAD" in result.stdout):
            raise RuntimeError(f"switch-entry grounding failed ({result.returncode}):\n{result.stdout}")
    print("linux64: live and AI battle grounding passed; temporary files removed")


if __name__ == "__main__":
    main()
