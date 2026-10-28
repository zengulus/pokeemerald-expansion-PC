# Native expansion port

The authoritative game is RHH pokeemerald-expansion at
`ae2144a897` (the starting checkout). Native code must compile and execute this
engine's C, including its battle mechanics, scripts and data. No GBA CPU, ROM
loader, interpreter for ARM instructions, or emulator is part of the target.

## Repository investigation

The upstream Makefile builds ARMv4T objects and links fixed GBA addresses with
`ld_script_modern.ld`. Host C/C++ tools generate maps, trainer tables, encoded
text, compressed graphics, samples and music sequences. Keep these tools and
formats initially. `tools/preproc` expands text and binary asset includes;
`scaninc` discovers asset prerequisites. The original battle test runner uses
mGBA and is not evidence of native execution.

Hardware dependencies include `gba/io_reg.h`, `gba/defines.h`, BIOS syscalls,
DMA, interrupt callbacks, timers, cartridge flash and RTC. Some routines copy
ARM instructions into writable memory and call them. Assembly data includes
32-bit pointers and packed structures; scripts and task/sprite fields also
store pointers as integers. RHH adds Smol compression and considerably more
battle code/data than the vanilla native ports.

## Reference investigation and provenance

* [NTx86/pokeemerald-sdl2pc](https://github.com/NTx86/pokeemerald-sdl2pc),
  inspected commit `58b0783eb7b5335dc20e4d3b47226819da5cc32a`: software tile
  rendering, host-backed registers, C DMA, a portable music mixer and explicit
  pointer-width script changes. Its current build describes 32/64-bit targets.
* [emilmichaiel/pokeemerald-multiplatform](https://github.com/emilmichaiel/pokeemerald-multiplatform),
  inspected commit `db1cab3d2dc9f0e0a9f4a3d67983e5acf30a9a46`: SDL2 platforms,
  audio repairs and file saving, but a 32-bit data ABI. Its MIT license expressly
  covers original fork modifications only, not upstream game code/assets or
  inherited third-party code. Retain this distinction in imported notices.

These are implementation references, not replacements for RHH. Do not import
their vanilla gameplay, mod overlay, experimental voxel renderer or launcher.
In particular the reference BIOS file identifies VBA-M ancestry; implement the
needed memory/decompression/math operations independently instead of importing
that file and its unrelated dependency/licensing surface.

## Native architecture

`Makefile.native` owns host compilation independently of the ARM target.
`native/` contains the platform boundary and regression harness. Hardware-facing
headers select host-owned storage under `NATIVE`; ordinary gameplay headers and
data remain authoritative. SDL2 supplies windows, input, PCM output and pacing.
A software tile renderer consumes game display state directly. This is an
intermediate rendering API, not a virtual GBA CPU or a GBA runtime dependency.

Both x86 and x64 compile the complete game. Pointer operands use explicit
`data_ptr` assembly directives and matching host-width C readers; scalar words
retain their original width. Map, event, music and voice descriptors have
matching pointer alignment. Tasks, sprites and list menus use typed runtime
references instead of packing addresses into signed halfwords. Native builds
reject direct pointer/integer casts of the wrong width. Linux x64 uses PIE and
Windows x64 uses ordinary high-address PE allocation; neither maps game memory
at GBA addresses. The native allocator uses the host heap with aligned blocks,
without imposing the original GBA heap ceiling. The old `gHeap` region remains
as scratch storage for upstream save relocation.

Allocations still referenced by queued graphics transfers stay alive until the
queue completes or is cleared. This preserves VBlank ordering without reading
freed host memory. Compressed graphics reserve and zero-fill the full transfer
capacity, even when the asset contains fewer tiles. Fixed-capacity palettes
include sixteen colors, with unused entries zero-filled; partial palettes that
use their own lengths retain those lengths. The tANS decoder bounds prefetches
to its declared bitstream. Text drawing uses defined shifts and touches only
the tiles containing glyph pixels.

Keep runtime saves in a configurable writable directory. Preserve RHH's save
validation and rotating sectors initially; the platform stores bytes durably
and reports failures. Longer term saves need versioned field serialization,
stable content identifiers and migrations independent of C structure layout.
Likewise external content packs need versioned manifests, validated assets,
stable namespaced registry IDs and explicit engine compatibility. Implement
these after the playable build, without removing expansion content to fit
original ROM or RAM limits.

## Verified build workflow

Host dependencies: GNU make, GCC/G++, Python 3, libpng development headers;
SDL2 development files for the target. MinGW builds Windows executables; Wine
can supply Windows runtime verification on Linux. Record exact commands and
results in `PORTING_PROGRESS.md`. Do not equate a PE file or a dummy SDL window
with a playable port.

The verified build host is Linux/WSL with the x86_64/i686 MinGW cross compilers. A native
Windows MSYS2 build has not been tested. ARM tools are not required by the native
build. The original GBA Makefile remains separate.

```sh
# Download and verify the pinned SDL2 2.30.0 MinGW development archive.
bash native/setup_sdl2.sh
make -f Makefile.native TARGET=windows64 prepare
make -f Makefile.native TARGET=windows64 all -j8
```

The output directory is `build/native/windows64/`. Keep
`pokeemerald-expansion.exe` and `SDL2.dll` together. Launch the executable
directly on Windows, or use Wine to test it on Linux. It does not load a game
ROM. Assets and the authoritative game C/data are linked into the executable.
The SDL archive URL and SHA-256 are pinned in `native/setup_sdl2.sh`.

For Linux x64, install the C runtime and SDL2 development library, then run
`make -f Makefile.native all -j8` (the default target is `linux64`). Explicit
`TARGET=linux32` and `TARGET=windows32` builds remain available; Linux x86 needs
32-bit C and SDL2 development libraries.

Controls: arrow keys move, Z is A, X is B, Enter is Start, Backspace is Select,
and A/S are L/R. The window resizes with the original 240×160 logical display.
The default save is `pokeemerald-expansion.sav` in the working directory;
`--save PATH` selects another writable location. The platform writes a temporary
file, flushes it and atomically replaces the save. RHH retains its checksums,
sector rotation and recovery logic. Saves use the versioned native container
described below; choose separate paths when testing different architectures.

```sh
make -f Makefile.native TARGET=windows64 gameplay-tests -j8
wine build/native/windows64/gameplay-tests.exe
python3 native/tests/run.py --target windows64
python3 native/tests/run.py --target linux64
python3 native/tests/replay.py --target windows64
python3 native/tests/reproducible.py --target windows64
# Package the matching verified EXE/DLL (use the result.json path printed above).
python3 native/package.py --target windows64 --reproducible-report build/native/reproducible/RUN/result.json
```

`replay.py` starts with an empty save, supplies real button events and checks
reports from new processes. It covers new game, maps, NPC scenes, menus, the
starter battle, Bag, party/summary menus and repeated saving/loading. It creates a fresh artifact
directory beneath `build/native/replays/` with saves, screenshots and logs.
`--fixed-rtc 2026-10-28T12:00:00` is a diagnostic option used by this replay;
ordinary gameplay reads the host clock. `--frames N` runs without pacing unless
`--realtime` is also supplied. `--report`, `--trace`, `--audio-capture`,
`--video-state` and `--screenshot` produce verification artifacts.

`reproducible.py` snapshots tracked and unignored source changes, makes two
independent clean exports, prepares all tools/data, builds each Windows target,
and compares executable/DLL hashes. It records toolchain versions and source,
dependency and artifact hashes in `result.json`. Supply `--sdl-archive PATH`
to use a previously downloaded archive offline. Reproducibility is scoped to
the recorded compiler/toolchain; different compiler releases are not expected
to emit identical machine code.

The Linux x64 address-checking workflow uses the same complete game and input
replay. It also checks all four normal/shiny and male/female palette variants
for every enabled species through the production palette loader:

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 python3 native/tests/run.py --target linux64 --sanitize
make -f Makefile.native TARGET=linux64 OBJ_DIR=build/native/linux64-asan \
  CFLAGS='-m64 -std=gnu17 -O1 -g -fPIE -fno-strict-aliasing -fwrapv -fno-toplevel-reorder -mno-ms-bitfields -Wno-address-of-packed-member -fsanitize=address -fno-omit-frame-pointer' \
  LDFLAGS='-m64 -pie -fsanitize=address -Wl,-z,noexecstack' all gameplay-tests -j8
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/native/linux64-asan/gameplay-tests
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 python3 native/tests/replay.py --target linux64 --executable build/native/linux64-asan/pokeemerald-expansion --timeout 600
```

Address and global bounds checks remain enabled. Leak detection is disabled for
the full-game replay because engine caches remain allocated at process exit.

## Persistent data and remaining modernization

ARM's `apcs-gnu` ABI rounds small aggregates to a four-byte boundary. Explicit
alignment now preserves that rule for affected saved types such as `Time`,
`Pokeblock` and `Mail`. With the default configuration, ARM, Linux x86 and
Windows x86 agree on SaveBlock1 (15568 bytes), SaveBlock2 (3884), SaveBlock3 (4),
party Pokémon (100), flag/variable offsets and recorded-battle layout. Tests
pin these values. On x64, SaveBlock1 is 15840 bytes (flag offset 4976, variables
5276), because saved object templates and Enigma Berry descriptors contain
pointers. SaveBlock2, SaveBlock3 and Pokémon retain the sizes above.

The file begins with a 32-byte little-endian native header: eight-byte magic
`RHHNATV\0`, format version, header length, payload length, pointer bits, layout
signature and a reserved zero word. The payload is RHH's 128 KiB sector image.
The header identifies the current structure/configuration layout before the
engine reads it. Truncated, oversized, unversioned, foreign-architecture and
incompatible-layout files are rejected without writing to them. RHH's sector
checksums and rotating-slot recovery remain authoritative; a whole-file payload
checksum would interfere with that recovery. Semantic format changes require
an explicit format-version bump, even if structure sizes do not change.

Continue rebinds saved map scripts from the current map descriptors, including
clone objects, and clears unused script slots. Fresh-process PIE and Windows
replays verify this with relocated runtime addresses. This container provides
ABI safety, rather than a portable interchange format: importing raw GBA saves,
cross-architecture migration and independent engine-version migration are not
implemented. Earlier raw development saves need a deliberate importer; the
acceptance replay always starts from a new save.

The next serialization step is a field-based schema with stable script/content
IDs, excluding runtime pointers entirely. Move save records out of fixed sector
budgets as that schema is introduced. External e-Reader Berry descriptions need
owned validated strings instead of their historical pointers. Generate native
C descriptors or validated serialized assets behind typed registries before
introducing pack loading. Keep the existing signed 16-bit gameplay counters and
fixed-width scalar operands distinct from runtime references. The renderer
still consumes the engine's tile/sprite formats; replacing that rendering API
and expanding content registries are separate from CPU execution.

For external packs, the proposed interface is a versioned manifest containing
pack namespace, content schema, dependencies and supported engine ABI range.
Content keys contain namespace, kind and a stable local ID. Separate species,
move, item, map, script and asset registries resolve those keys to typed `u32`
runtime handles; handle zero is invalid. Handles and array positions are never
persistent identity. Saves use a content-key dictionary, permitting registries
to be rebuilt independently of load order or engine address layout.

Built-in enum tables should first pass through a generator into these same
registries. Existing `u16` gameplay fields must be migrated deliberately before
accepting registries beyond their capacity; rejecting an oversized registry is
preferable to truncating a handle. Registry allocation uses the native heap,
without inheriting the ROM-size or GBA heap limits. Keep adapters at the old
table APIs while migrating callers, rather than introducing a second battle
engine or replacing authoritative RHH mechanics.

Asset records need explicit codec, dimensions, byte length and content hash.
Serialized script references identify a script/label or a versioned engine
export, never an external native address. Decode operands into typed runtime
references only after validating lengths, indices, codec outputs, dependencies
and command/export ABI versions. Build a complete candidate registry and
publish it only after validation; defer hot reload while battle/script/task
references are live. Start with loading packs at a session boundary.

Engine updates need explicit schema migrations and compatibility tests against
older saved content dictionaries. Missing packs/content should produce an
actionable compatibility result without interpreting addresses from older
executables. The current layout header is a guard until that importer exists.
This is the extension design, not an implemented pack loader; the playable
native build takes priority over introducing it prematurely.

Physical GBA wireless/link cables, GameCube multiboot and e-Reader downloads
have no native transport. They report unavailable/disconnected; native code
never executes an ARM payload. Network/link replacement, broad minigame/timer
coverage and external pack loading remain work. The accepted x64 opening-game
slice does not establish complete endgame, multiplayer or minigame coverage.

## Acceptance

Acceptance requires clean Windows builds, genuine startup, overworld movement,
menus, NPC scripts, complete battles including expansion mechanics, audible
PCM, and saves reloaded by a fresh process. Automated checks must distinguish
subsystem tests from full gameplay and real audio-device verification.
