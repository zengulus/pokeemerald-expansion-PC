# Native support code provenance

`renderer.c`, `music_player.c`, `sound_mixer.c`, `cgb_audio.c` and the associated
audio headers were adapted from emilmichaiel/pokeemerald-multiplatform at
`db1cab3d2dc9f0e0a9f4a3d67983e5acf30a9a46`, itself derived from the native
port work of Kurausukun, NT_x86, Normmatt, camthesaxman and other contributors.
The renderer and portable MP2K/CGB work were also examined in
NTx86/pokeemerald-sdl2pc at `58b0783eb7b5335dc20e4d3b47226819da5cc32a`.

The fork's scoped MIT notice is retained in `MULTIPLATFORM_LICENSE`; as its
Scope section states, it applies only to original modifications through that
fork and does not relicense upstream or inherited third-party material. No new
license for upstream game code, inherited components or game assets is asserted
by this port. Existing source notices remain applicable.

SDL2 is an external dependency under the zlib license; its downloaded archive
includes its license and is excluded from source control. Its version and hash
are pinned by `setup_sdl2.sh`. The original asset tools retain their own notices.
No reference-port BIOS implementation, ROM image or emulator is imported.

## MIDI and SoundFont playback

The Gen I/II battle themes use the original Red and Gold GBS music data and
their Game Boy sound engine. Playback uses Game Music Emu 0.6.3, built as the
separate replaceable `librhhgme.so` / `rhhgme.dll` library. Its GBS-only
source, LGPL 2.1 license, standalone rebuild instructions and pinned upstream
archive hash are in `third_party/gme/`; Windows packages include those files.
The two original music archives, extracted GBS hashes and zero-based track
indices are recorded in `assets/music/manifest.json`. No automatic playback
limit or fade is applied; the game's own music code controls looping.
The track indices correspond to the one-based battle-theme listings for
[Red](https://www.zophar.net/music/gameboy-gbs/pokemon-red) and
[Gold](https://www.zophar.net/music/gameboy-gbs/pokemon-gold).

Unmodified TinySoundFont and TinyMidiLoader sources are pinned to
`schellingb/TinySoundFont` commit
`853a0a171759f1ddba0de1442133a75912bbeffa`. TinySoundFont uses the MIT license
in `third_party/tinysoundfont/LICENSE`; TinyMidiLoader's Zlib notice is retained
at the top of `third_party/tinysoundfont/tml.h`.

Every battle MIDI and SF2 file has its source URL, byte count and SHA-256 in
`assets/music/manifest.json`. DS MIDI exports have an extra marker track for
their original loop points; their note and controller tracks are unchanged.
Gen III uses the Emerald sequences and voicegroups already present in
this project. Gen IV/V playback uses sequences and their corresponding banks
exported from the Diamond and Black/White game music archives. Credits for the
retained MIDI files and banks:

* Gen I–III MIDIs: João “Johnnyz” Buaes, via VGMusic.
* Gen IV/V MIDI and SF2 pairs: extracted from the Diamond and Black/White 2SF
  music archives linked and hashed in the asset manifest, with VGMTrans 1.3
  at `fe806cf76a45d517c1b2cfbb8a3b624081671b25` (zlib license). The exporter
  converts the original sequences and their banks; it is not a DS emulator.
  Gen V uses Black/White's Champion theme.
* The Gameboy GM Soundfont by CynthiaCelestic / Rodolfo Ruiz-Velasco and the
  Gen I/II MIDI arrangements remain available for existing custom music.
* Pokémon Emerald Soundfont, updated August 13, 2021: ripped and assembled by
  **MezmerKaiser**, who requests credit in its embedded notice.
* The earlier DPPt GM-arranged and Black and White 2 (**Xennon**) banks remain
  available for existing custom music; the built-in styles now use the paired
  original banks.

To regenerate the DS pairs, extract the SDAT from each locked 2SF archive, open
it in VGMTrans 1.3, and export the collections named in
`annotate_ds_music.py` as MIDI and SF2. Run that script with the two export
directories to add the loop points measured from the original SSEQ backward
jumps. `copy_battle_music.py` verifies the resulting asset hashes offline.

The Emerald and Platinum banks were retrieved as individual members of a
public ZIP using byte ranges. The rest of that archive is neither included nor
required. `fetch_battle_music.py` records their exact archive member names.
`copy_battle_music.py` verifies the locked assets during offline builds.

These credits and the synthesizer licenses do not relicense the original game
compositions, arrangements, samples, or other upstream assets.
