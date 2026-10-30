#!/usr/bin/env python3
"""Generate bounded lookups for object animation tables and scripts."""
import argparse
import re
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    source = Path("src/data/object_events/object_event_anims.h").read_text()
    tables = re.findall(r"^(?:static )?const union AnimCmd \*const (\w+)\[\]\s*=", source, re.M)
    commands = re.findall(r"^(?:static )?const union AnimCmd (\w+)\[\]\s*=", source, re.M)
    if not tables or not commands:
        raise ValueError("Object animation declarations were not found")
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    lines = ["/* Generated from object_event_anims.h; do not edit. */",
             "u32 RhhModsAuthoredAnimCount(const union AnimCmd *const *anims)", "{", "    if (!anims) return 0;"]
    lines += [f"    if (anims == {name}) return ARRAY_COUNT({name});" for name in tables]
    lines += ["    return 0;", "}", "u32 RhhModsAuthoredAnimCommandCount(const union AnimCmd *commands)",
              "{", "    if (!commands) return 0;"]
    lines += [f"    if (commands == {name}) return ARRAY_COUNT({name});" for name in commands]
    lines += ["    return 0;", "}"]
    output.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
