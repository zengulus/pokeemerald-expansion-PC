# Native mods implementation checkpoints

Implemented the requested API and eight default-off examples. Platform checks,
independent Windows reproduction and standalone archive verification passed.

## Public API and independent examples

`include/mod_api.h` defines the versioned C ABI, stable content identities,
read-only metadata, typed settings, deterministic RNG and hook composition,
portable private state, transactional migrations, and generic durable journals.
`MODDING.md` documents ownership, event contexts, save compatibility and traces.

Each requested option is an independent module in `native/mods/`, using only the
public API and standard C headers: Nuzlocke, wild randomizer, trainer randomizer,
starter selection, 600 BST, next-Gym/Champion level cap, followers and battle
music. Each has its own ID, descriptors and state. All eight build as standalone
DLLs/SOs and can replace the bundled copies with `--external-bundled-mods`.
The engine owns the original menus, save container, content metadata, Pokémon
objects and gameplay hooks. It owns run identity in `rhh:host_run`.

The original New Game menu opens the rules screen, defaulting every option off.
Pool mode gives stable area tables; Full mode advances only when a wild Pokémon
is actually created. Starter selection includes creatable non-legendary,
non-mythical species and permanent forms. Equalization totals exactly 600 while
preserving proportions within stat bounds and Shedinja's 1 HP. Caps use the
actual next Gym or Champion party. Followers use RHH's existing implementation.

Nuzlocke begins when Poké Balls are obtained. Each eligible first encounter per
named area consumes its attempt, including fleeing or failure. Owned Pokémon
remain dead through healing, revival, PC conversion and reload. Wipeout ends
the run. Versioned per-module journals preserve these facts across ordinary
reloads, including unsaved deaths. A persistence error locks the challenge.

## Persistence and migration verification

Native container v1 loads with all example settings off. Container v2 retains
atomic game/mod snapshots and corresponding generations, two run journals, and
a persisted run serial. Aborted writes, failed atomic commits, corrupt data and
missing required modules leave existing files untouched. Ordinary RHH game data
still requires the matching engine architecture/layout; mod schemas do not
migrate arbitrary upstream game saves.

Strict host tests cover registration-order independence, dependency cycles,
sticky constraints, stable content rebinding, deterministic RNG, transactional
state/configuration changes, New Game setting locks, runtime settings, durable
merging and persistence failure notifications. Native save tests cover legacy
upgrade, fresh-process reload, generation recovery, rollback and missing modules.

Real-engine tests cover all enabled species at 600 BST, actual randomized trainer
parties, all Gym/Champion caps, enemy-level independence, candy refusal, 32-draw
Full save/reload tapes, and sixteen actual wild creations. They exercise real
Nuzlocke healing/item/PC paths and RHH save/journal recovery. Example-specific
migration checks preserve the wild ordinal while adding its roster and expand
the Nuzlocke area/death ledgers; malformed old schemas are rejected.

## Music and original menus

The manifest locks twenty real Gen I–V battle MIDIs (wild, trainer, Gym,
Champion) and four genuine Pokémon SF2 banks. Gen I/II use Gameboy GM; Gen III
uses MezmerKaiser's Emerald GM; Gen IV uses DPPt Soundfont 2021 GM; Gen V uses
Xennon's BW2 bank. URLs, hashes and credits are in the manifest and
`native/THIRD_PARTY.md`. TinySoundFont/TinyMidiLoader are pinned at
`853a0a171759f1ddba0de1442133a75912bbeffa` with their MIT/Zlib notices.

All twenty tracks produced finite, nonzero audio on Linux64 and Windows x64/x86.
Real SFX-player advancement and fade-to-silence passed for each track. The MIDI
mix uses a separate delivery buffer so it does not feed back into MP2K reverb.
Original battle audio remains the default. The actual Options screen was
inspected; selecting Gen II, saving and fresh-process Continue retain it.

## Completed platform evidence

* Linux64 all-off input replay: `build/native/replays/linux64-iml9veps/`.
* Linux64 external examples with all options enabled, real starter battle,
  follower, repeated Save and built-in Continue:
  `build/native/replays/linux64-qy3x09pe/`.
* Windows64 external/built-in enabled replay, including Options and another
  fresh Continue: `build/native/replays/windows64-it3hah3d/`.
* Windows64 and Windows32 production subsystem/API/save suites passed.
* Linux64 strict API, real-engine migration/mechanics and external SO tests
  passed. The sanitizer subsystem, mechanics and all-twenty-music tests passed.
* The original ARM `modern` build passed after the integration.

The sanitizer identified fixed-length string reads past short rules-screen
labels and an existing short default-player-name string. Both now stop at their
actual terminators. The full sanitizer replay passed at
`build/native/replays/linux64-on4l5861/`, including the rules screen, naming,
battle, follower, Options, repeated Save and fresh Continue. Linux32 passed
at `build/native/replays/linux32-tacxgnd0/`; Windows32 passed at
`build/native/replays/windows32-exjuujg7/`. Both used external examples for
writes and built-in copies for Continue, including the music change.

A real Windows WASAPI device opened with Gen I MIDI/SoundFont music active
inside Birch's battle (`/tmp/rhh-wasapi-mods.json`). It generated 22,432,000
samples and queued 1,626,320 samples to the device. All twenty music renders
also passed AddressSanitizer. The native music and example sources have no
runtime network dependency.

Two independent clean Windows source exports produced identical executable,
SDL2 DLL and all eight example DLLs. The report is
`build/native/reproducible/windows64-ri9_g38j/result.json`; the executable's
SHA-256 is `5f9d6296068baddd14dd85e894eb732b48784a613444407ffb94f48b3e707703`.
`build/native/evidence/mods/verification.json` links the platform evidence and
archives the successful regression logs.

The Windows64 archive was extracted outside the repository. Every packaged
hash was verified, including all eight external DLLs and their source files.
The included public-API-only additional cap example compiled from those files.
The extracted executable loaded it alongside the eight external examples,
played the packaged Gen I battle MIDI/SoundFont, won Birch's battle, saved,
and preserved the party/configuration in a fresh process using the built-in
examples. Its level-50 constraint composed with the Gym cap by minimum (15).
The final archive is `build/native/dist/pokeemerald-expansion-windows64.zip`;
it includes the public header, all example sources, build instructions,
credits, music assets and reproducible build information.

These checks cover the opening gameplay replay and the targeted mechanics/API
regressions. Broad endgame and minigame coverage, physical GBA link replacement,
raw GBA save import and arbitrary upstream engine-save migration remain outside
this implementation's verified scope, as documented in `PORTING.md`.
