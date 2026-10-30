#!/usr/bin/env python3
"""Generate bounds for the engine's static object-frame tables.

The generated code is compiled in the same translation unit as the source
arrays, so sizeof uses the actual included asset sizes for that build.
"""
import argparse
import re
from pathlib import Path


def generate(source):
    lines = source.splitlines()
    result = ["/* Generated object-frame bounds; do not edit. */"]
    index = 0
    while index < len(lines):
        line = lines[index]
        stripped = line.strip()
        if stripped.startswith("#define"):
            while line.rstrip().endswith("\\") and index + 1 < len(lines):
                index += 1
                line = lines[index]
        elif re.match(r"#\s*(if|ifdef|ifndef|elif|else|endif)\b", stripped):
            result.append(line)
        else:
            match = re.fullmatch(r"(?:static )?const struct SpriteFrameImage (\w+)\[\] = \{", stripped)
            if match:
                table = match.group(1)
                body = []
                index += 1
                while index < len(lines) and lines[index].strip() != "};":
                    body.append(lines[index])
                    index += 1
                if index >= len(lines):
                    raise ValueError(f"Unterminated table: {table}")
                text = "\n".join(body)
                ascending = re.findall(r"overworld_ascending_frames\(\s*(\w+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)", text)
                if ascending:
                    if len(ascending) != 1 or not text.lstrip().startswith("overworld_ascending_frames("):
                        raise ValueError(f"Unsupported mixed frame table: {table}")
                    asset, width, height = ascending[0]
                    count = f"sizeof({asset}) / ({width}u * {height}u * 32u)"
                else:
                    count = f"ARRAY_COUNT({table})"
                result.append(f"if (images == {table}) return {count};")
        index += 1
    return "\n".join(result) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--source", default="src/data/object_events/object_event_pic_tables.h")
    args = parser.parse_args()
    source = Path(args.source).read_text()
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(generate(source))


if __name__ == "__main__":
    main()
