# Native port progress

## Acceptance ledger

| Requirement | Current evidence |
| --- | --- |
| RHH source remains authoritative | Native target compiles the checkout's gameplay C and generated map/battle/script data |
| Native Windows executable and reproducible build | Final Windows x64 EXE/DLL match two independent clean exports; a portable ZIP also runs outside the repository |
| Startup and overworld | Windows and Linux enter the actual house overworld from a saved game |
| Map navigation, menus and NPC interactions | Real input covers houses/bedrooms, Littleroot, Route 101 and the lab, Mom/rival/Birch, Save, Bag, party and summary menus |
| Battles with expansion mechanics | Starter battle won in Linux and Windows; focused production-code modern type/ability/move checks pass on both |
| Audio | Finite, nonzero PCM; real PulseAudio and Windows WASAPI device runs accept all paced samples |
| Persistent save/reload | Fresh-save acceptance replay wins a battle, saves Torchic/story state, Continues, saves again and Continues again on Linux and Windows |
| Native regression tests | Production subsystem/gameplay checks pass on Windows x86/x64 and Linux x64; complete x64 replay and data checks also pass with AddressSanitizer |
| 64-bit support | Complete game builds on Windows/Linux x64; both pass opening-game, battle, Bag and repeated fresh-process Save/Continue replays at full-width addresses |
| External packs and independent engine updates | Architecture described; implementation deferred until playable slice |

## Checkpoint 0: investigation (2026-10-28)

Inspected upstream build, hardware headers, main loop, script ABI, flash access,
RHH compression and asset tools. Inspected both requested reference ports from
local source checkouts and their public repositories. Found host GCC, SDL2
2.30.0 (including 32-bit library), MinGW x86/x64, ARM cross tools, and Wine.
Windows SDL2 development libraries are not installed in the MinGW sysroot.
Upstream host asset tools built successfully.

## Checkpoint 1: native compilation and startup (2026-10-28)

Added independent `Makefile.native`, host-owned display/register/DMA memory,
SDL2 input/video/PCM/frame pacing, file-backed sector saving, and host RTC.
Adapted the reference software renderer and MP2K/CGB player with provenance in
`native/THIRD_PARTY.md`. Replaced ARM RNG and Smol code-copy calls with direct C.
Preserved the RHH main loop, gameplay C, maps and battle/script assets.

Build commands used: `make -f Makefile.native prepare`,
`make -f Makefile.native all -j8`, and
`make -f Makefile.native TARGET=windows32 all -j8`. Windows artifact currently:
`build/native/windows32/pokeemerald-expansion.exe` with its `SDL2.dll`.
This is an intermediate executable, not a completed playable delivery.

Actual Wine execution at 300 frames rendered the RHH intro (`/tmp/rhh-win300.bmp`)
and produced a PCM capture. Linux startup capture contained 420600 float
samples, all finite, 14020 nonzero (peak 0.06073). Test settings used
`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`, so these establish software output,
not physical audio playback. The PE imports SDL2 and Windows C/system libraries;
there is no emulator or ARM execution backend.

Memory/DMA and fresh-process byte-save tests passed on Linux x86/x64 and Wine
x86. `python3 native/tests/run.py --target linux64` also passed the RHH RNG
reference sequence and eight production-code Smol roundtrips (modes 1 and 2).
More codec modes, battle mechanics and real progress saves still need coverage.

Runtime investigation fixed a boot interrupt dependency and a physical RFU
adapter probe that spun forever waiting on a hardware timer. Native reports no
physical wireless adapter. A new-game test reached name entry, exposing a
null battle-state read in an out-of-battle animation and a naming-screen exit
callback use-after-free. The former was fixed; the latter is being verified.
An intermittent startup crash still needs revalidation outside the debugger.
Cartridge multiboot and physical link transports are unavailable; native never
executes their ARM payloads. No completed battle or overworld claim is made.

## Checkpoint 2: native map loading and real game saves (2026-10-28)

The standalone startup crash was an unchecked `0xffff` sector id on erased
flash. Save validation and copying now reject invalid ids before indexing the
layout. Added production-code tests for erased/corrupt sectors and rotating
save-slot selection. The native assertion handler logs and aborts instead of
waiting forever on a physical display timer.

RHH encodes script-effect instrumentation using GBA ROM mirror addresses.
Native assembly now stores direct function addresses; generated metadata from
the authoritative `requests_effects` annotations preserves script analysis.
No ROM mirroring or fixed-address mapping is used. Native DMA request copies
complete synchronously, removing map-loader waits for asynchronous interrupts.
BG tilemap validation now accepts host allocations instead of imposing the
GBA IWRAM boundary. Regression tests cover host tilemaps and transfers beyond
the original 40 KiB vblank budget.

Verified real input replay through RHH intro, new game, naming, truck scene,
truck exit, moving-in NPC scripts, and the house Start/Save menu. The renderer
capture `/tmp/rhh-save-game.bmp` shows the actual house after saving.
`/tmp/rhh-game-progress.sav` is a 128 KiB file written by the existing RHH Save
flow. Fresh-process Linux and Wine Continue checks are being collected.

The expanded subsystem suite passes on Linux x64 and Wine x86, including the
production RHH RNG, save validation, graphics transfers and Smol asset modes
1, 2, 3, 5 and 6. Wine tests wait for their private server to exit before
cleaning its prefix. Full gameplay remains a 32-bit intermediate milestone;
no completed battle, broad regression coverage or reproducible clean-build
claim is established yet.

## Checkpoint 3: cross-platform Continue and story progression (2026-10-28)

Fresh Linux and Wine processes both loaded the same real RHH save, reporting
map 1,0, position 8,7, counter 1, intro state 4, name Aaaaaaa and 3000 money.
Normal movement then entered the bedroom, set the wall clock, completed Mom's
upstairs dialogue and the downstairs TV event, and saved again (counter 2,
intro state 7, wall-clock flag set). These checks exercise existing scripts,
map transitions and menus; battles are still pending.

A paced 180-frame Linux run opened SDL's PulseAudio device and generated and
queued 252360 float samples without queue rejection. This verifies the actual
audio backend, not a human listening assessment. The Windows audio backend
still needs the corresponding check. Native audio layout assertions now verify
that the RHH player and portable mixer share their expected structure layouts.

Build flags now normalize source paths and disable PE link timestamps; clean
independent build comparison is still pending.

## Checkpoint 4: playable Windows acceptance and reproducibility (2026-10-28)

`native/tests/replay.py` now drives genuine buttons from an empty save through
new game, naming, truck/house NPC scenes, clock/TV, rival visit, Route 101,
starter selection, a won battle and the lab. It saves, reloads in a fresh
process, saves again and reloads again. Both targets pass with the fixed
platform RTC (`2026-10-28T12:00:00`): Torchic, level 5, HP 20/20, rescue flag set,
clock flag set, intro state 7 and save counter progressing 1,2,3,4. Persistent
fields match across each fresh-process Continue. Latest acceptance artifacts:

- Linux: `build/native/replays/linux32-z5os9gb9/`
- Windows/Wine: `build/native/replays/windows32-4qa44sbm/`

The independent gameplay regression executable links the complete game C and
real data. It validates 1571 enabled species and 934 moves, physical/special
move categories, encrypted Pokémon/stat persistence, Fairy effectiveness,
Scrappy, Freeze-Dry, Flying Press, Thousand Arrows, Tera Shell, effect metadata,
script variables and native-address call/return. It passes on Linux x86 and
Windows x86. The subsystem suite also passes on Linux x64 and Windows x86,
including RTC BCD/leap dates and all generated Smol asset modes (1,2,3,5,6).

A rendering check exposed a transfer-order defect: immediate native queue
copies captured tilemaps before confirmation menus finished drawing. Queued
copies now run at VBlank or a completion poll, preserving drawing order while
allowing synchronous map-loader polls to progress. The clock confirmation menu
is visible (`/tmp/rhh-clock-menu.bmp`), and a regression changes a source after
enqueueing to verify the deferred copy observes it.

The ARM `apcs-gnu` ABI rounds small structs to four bytes. Explicit alignment
for affected persistent types restores save-block sizes and offsets on both
hosts. A compiler layout probe agrees on SaveBlock1=15568, SaveBlock2=3884,
SaveBlock3=4, Pokémon=100 and recorded battle=4092 bytes. Tests pin the default
configuration. Old development saves predating this correction are obsolete;
acceptance tests use fresh saves. Import from an independent GBA build remains
unverified.

Two clean source exports, each with freshly prepared host tools/assets, build
identical Windows artifacts. Reproduction report and full logs:
`build/native/reproducible/windows32-p7f66con/result.json`.

- EXE SHA-256: `b3f84fe10c135212aa47e4c1eb6e0bf6f6a565a6f03d81377b41b3597928fef0`
- SDL2 DLL SHA-256: `3078a42c9b672a970a30a9bf070617a2dd676a94798ec6d5bd221caadc28a397`
- Compiler: i686 MinGW GCC 13-win32; host GCC/G++ 13.3.0; make 4.3; Python 3.12.3.

The preserved checkpoint EXE in `build/native/checkpoints/playable32/` has the
above hash. Later modernization changes produce a different executable.
A real SDL window (no dummy video
backend) ran under Wine with WASAPI audio: 180 paced frames generated and
queued all 252360 float samples. Earlier real PulseAudio verification reports
the same complete queue count. These are device/output checks, not human
listening assessments.

This establishes the playable 32-bit milestone. Complete 64-bit gameplay,
pointer-free save serialization, broad battle/minigame coverage, replacement
link transports and content-pack loading remain. The native architecture and
concrete migration steps are recorded in `PORTING.md`; the overall goal is not
marked complete at this intermediate milestone.

## Checkpoint 5: genuine x64 gameplay and ABI-safe saves (2026-10-28)

The complete Linux x64 PIE and Windows x64 PE execute at ordinary high host
addresses. The full input replay passes on both, covering the opening story,
NPCs, map navigation, the won Torchic battle, Bag and repeated fresh-process
Save/Continue. Evidence from the first complete x64 replays:

- Linux: `build/native/replays/linux64-mdo703xr/`
- Windows/Wine: `build/native/replays/windows64-lulz3_bp/`

These runs saved Torchic level 5, HP 20/20, money 3000, both story flags and
save counters 1 through 4. `code_anchor` and callback traces distinguish real
wide addresses from a low-address workaround. The replay resolves callback
symbols relative to a runtime code anchor, supporting PIE/ASLR.

Assembly now marks pointer operands explicitly, preserving scalar widths.
Map/event descriptors, music tracks, voice unions, script call/return and
conditional branch lengths match the host ABI. Typed task/sprite references,
typed list-menu storage and full-width window attributes replace pointer
packing. The native allocator supports aligned allocations larger than the
original GBA heap. Field-effect, field-move, door and battle animation paths
use host pointers. Wrong-width pointer casts are build errors.

The x64 gameplay regression executable passes data checks (1571 species, 934
moves), 518 map layouts plus a clone fixture, Pokémon persistence, modern
battle mechanics and script effects/call/return. The Windows x64 subsystem
suite passes, including new contest branch/call/return tests and 11 invalid
save-container rejection cases. Broad endgame/minigame coverage is still
unverified; these checks do not imply every expansion feature was played.

Native saves now have a 32-byte version/layout/architecture header followed by
the existing RHH sector payload. Incompatible files are rejected before engine
loading and left unchanged. This prevents silent cross-ABI corruption while
preserving sector recovery. It does not implement raw GBA import or a portable
save schema. The pointer-free schema, content IDs, migrations and pack registry
design remain documented in `PORTING.md`.

Final rebuild/replay, Windows x64 real-device audio and independent clean x64
build comparison are being collected after the shared pointer cleanup.

## Checkpoint 6: native memory lifetimes and strict address checks (2026-10-28)

The complete Linux x64 AddressSanitizer replay passes new game, clock/TV,
starter battle, repeated Save/Continue, Bag, party and summary screens, including
closing the summary and returning to the overworld. Current evidence:
`build/native/replays/linux64-y0os7jgq/`. Bounds checking includes globals and
exact-size compressed inputs; only process-exit leak detection is disabled.
The full-game regression also passes under AddressSanitizer, loading all four
palette variants for each of the 1571 enabled species.

This validation exposed and fixed several inherited hardware assumptions:

- Text glyphs relied on shifts by 32 having ARM behavior; x86 could write into
  the next tile or an allocation header. Defined shifts and bounded writes now
  preserve the intended pixels. Golden pixel/guard tests cover the edges.
- Queued transfers could outlive host allocations. Referenced allocations now
  retire until ordered transfers complete or are cleared. Tests cover a freed
  source and a freed intermediate destination/source without flushing early.
- Fixed-size tileset/sprite uploads could exceed decoded asset lengths. The
  transfer allocation now includes zero-filled capacity; production tests check
  short assets through both background and sprite upload paths.
- tANS prefetched beyond its final encoded word. Native readers now supply
  logical zero padding within an explicit bitstream bound, without borrowing
  bytes from neighboring objects. Codec tests no longer pad their inputs.
- Short sprite palettes relied on neighboring data for unused colors. Fixed
  banks now have sixteen entries; three partial background palettes that use
  their own lengths retain those lengths.
- Birch's text callback read before the start of its string, and blank door
  layers read beyond their four source palettes. Both reads are bounded now.

The subsystem suite passes with AddressSanitizer, including glyph/door bounds,
queue lifetimes, save validation, RTC, script operands, RNG and codec modes.
The original ARM modern target also still builds, preserving the separate
upstream build; it is not used for native runtime verification. Final Windows
replays, audio, clean reproduction and standalone packaging are being refreshed
after these corrections.

## Checkpoint 7: delivered Windows x64 acceptance (2026-10-28)

The final native Windows x64 executable directly runs the complete expansion C
engine and linked assets. The package is
`build/native/dist/pokeemerald-expansion-windows64.zip`; extract it and keep
`pokeemerald-expansion.exe` beside `SDL2.dll`. Source, build configuration,
tests and setup instructions remain in this worktree. The package includes
controls, build/provenance documentation, runtime licenses and build hashes.

Two independent clean source exports produce the same final files, and those
hashes match the working executable used for runtime verification:

- EXE: `0d53aefc66e289d96bddf00a8d9b1bd144b0546951aa4281e5a6f04616111620`
- SDL2: `df6be1cd49d741d1a3963eca3f32dea9aa78c39de6e876aabfe74bd2ae528872`
- Reproduction: `build/native/reproducible/windows64-ylzx26bp/result.json`
- Evidence index: `build/native/evidence/windows64/verification.json`

The source snapshot matches the delivered executable sources; subsequent
changes to this ledger only record the collected evidence. Preparation and
native builds also passed with ARM tools replaced by failing sentinels, with
no sentinel invoked. The PE is AMD64, with ordinary image base `0x140000000`,
and imports only SDL2 and Windows system/CRT libraries. No ROM or emulator is
loaded or packaged.

Final input replays all pass through startup/naming, overworld traversal, NPC
scenes, the won starter battle, Save, fresh-process Continue, another Save and
Continue, Bag, party and summary menus, then return to the overworld:

- Windows x64: `build/native/replays/windows64-ulj7it0a/`
- Windows x86: `build/native/replays/windows32-a6xlg4pt/`
- Linux x64: `build/native/replays/linux64-ivlkr0nk/`
- Linux x64 with AddressSanitizer: `build/native/replays/linux64-y0os7jgq/`

All preserve Torchic level 5, HP 20/20, money 3000, name/story flags and save
counters 1 through 4 as expected. Both Windows subsystem suites and gameplay
regressions pass. The latter validate 1571 enabled species, all four palette
variants per species, 934 moves, 518 maps plus a clone fixture, encrypted Pokémon
persistence, modern type/ability/move rules and script effects/call/return.

A real SDL window and WASAPI device ran the final x64 EXE for 300 paced frames.
All 420600 generated float samples were queued, all were finite, 49070 were
nonzero and peak magnitude was 0.06072852. This is a device/output check rather
than a human listening assessment. Final battle and summary captures were
visually inspected; they are in the evidence directory.

The extracted ZIP ran from an isolated `/tmp` directory containing the packaged
runtime, a copied native save and diagnostic button files. It loaded counter 4,
saved counter 5 and reloaded counter 5 in a fresh process, preserving party and
story fields. It required no repository assets or GBA ROM. Standalone results
and reports are retained in the evidence directory.

The requested native playable result and extension design are established.
Remaining work is explicit: broad endgame/minigame testing, replacement link
transports, raw GBA save import, architecture/engine-version save migrations,
pointer-free serialization and the designed external-pack loader. These are
not claims of implemented or verified functionality.
