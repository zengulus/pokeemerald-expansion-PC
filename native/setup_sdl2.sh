#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
version=2.30.0
archive="native/deps/SDL2-devel-${version}-mingw.tar.gz"
digest=d44bd8cd869219f43634a898d7735d0c92d4b2cca129f709dc8b98402b36d5f3
mkdir -p native/deps/SDL2
if [ ! -f "$archive" ]; then
    curl -fL "https://www.libsdl.org/release/SDL2-devel-${version}-mingw.tar.gz" -o "$archive"
fi
echo "$digest  $archive" | sha256sum --check
tar -xzf "$archive" --strip-components=1 -C native/deps/SDL2
