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

Unmodified TinySoundFont and TinyMidiLoader sources are pinned to
`schellingb/TinySoundFont` commit
`853a0a171759f1ddba0de1442133a75912bbeffa`. TinySoundFont uses the MIT license
in `third_party/tinysoundfont/LICENSE`; TinyMidiLoader's Zlib notice is retained
at the top of `third_party/tinysoundfont/tml.h`.

Every battle MIDI and SF2 file has its source URL, byte count and SHA-256 in
`assets/music/manifest.json`. The original files are retained without changes.
The arrangements are Pokémon battle music from Generations I–V, rather than
generic demonstration sequences. Credits:

* Gen I–III MIDIs: João “Johnnyz” Buaes, via VGMusic.
* Gen IV MIDIs: Electric Concerto (wild), Brian Hayden (trainer), foxT.A.K.A
  (Gym), and Joanna (Champion), via VGMusic.
* Gen V MIDIs: Neocrystal3D/Superpokemonray1234 (wild), Fortwaffles (trainer and
  Gym), and Wobuffet3 (Champion Iris), via VGMusic.
* Gameboy GM Soundfont: CynthiaCelestic / Rodolfo Ruiz-Velasco. Its embedded
  comment identifies Pokémon R/B/G/Y and G/S/C; both generation styles share
  this bank.
* Pokémon Emerald Soundfont, updated August 13, 2021: ripped and assembled by
  **MezmerKaiser**, who requests credit in its embedded notice.
* Pokémon DPPt Soundfont 2021: GM-arranged bank, preserved as “Pokémon Platinum”
  in the public archive linked in the manifest. Its embedded notice identifies
  the original Nintendo samples; an assembler name is not provided.
* Black and White 2 Soundfont: **Xennon**, with its original embedded notice and
  source-catalogue link retained.

The Emerald and Platinum banks were retrieved as individual members of a
public ZIP using byte ranges. The rest of that archive is neither included nor
required. `fetch_battle_music.py` records their exact archive member names.
`copy_battle_music.py` verifies the locked assets during offline builds.

These credits and the synthesizer licenses do not relicense the original game
compositions, arrangements, samples, or other upstream assets.
