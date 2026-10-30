Pokemon Emerald Expansion - native Windows x64

Extract this entire folder, then run pokeemerald-expansion.exe. Keep SDL2.dll
and rhhgme.dll beside it. Game code and assets are included; no game ROM is loaded.
Keep the music folder beside it as well for the optional battle music styles.

Controls
  Arrow keys: move
  Z: A / confirm       X: B / cancel
  Enter: Start         Left or Right Shift: Select
  A / S: L / R

Choose CONTROLS in the game's Options menu to change any keyboard binding.
Select an action, press A, then press the desired key; Escape cancels the
capture. If a key is already assigned, its existing action takes the old key.
RESET DEFAULTS restores the bindings above. Changes are saved immediately in
the user's RHH/PokeemeraldExpansion preferences folder, independently of game
saves. To use a specific settings file, launch with:
  pokeemerald-expansion.exe --controls "C:\path\controls.cfg"

When naming your character, Pokémon or PC boxes, type directly on the keyboard.
Enter confirms and Backspace deletes. Running Shoes are available from the
start; hold the B button to run where the game permits it.

Save from the game's Start menu. Progress is stored in
pokeemerald-expansion.sav in the working directory. To choose another location,
launch from a terminal with: pokeemerald-expansion.exe --save "C:\path\game.sav"

This build uses a versioned native save format. Raw GBA saves, saves from other
architectures/layouts and ordinary game-data migrations between engine revisions
are not supported. Native container v1 saves upgrade to default-off mod settings.
Mod state has its own versioned schemas and migrations. Keep the corresponding
executable with an existing save; see MODDING.md for compatibility details.

New Game includes default-off Nuzlocke, wild pool/full randomization, trainer
randomization, a non-legendary starter picker, 600 BST, next-Gym/Champion level
caps, followers, and Generation I-V battle music. Music is also in Options.
These options and the new battle-environment renderer are nine independent
example mods using the public C API. Enable BATTLE FX in Options for persistent
weather and terrain shaders. It defaults off. The original messages remain
when the renderer cannot use a shader.

To run their external DLL builds instead of the built-in copies:
  pokeemerald-expansion.exe --external-bundled-mods example-mods
Their source and public API header are included under examples. Both versions
use the same module IDs and portable state, so saves work between them.
Use --mod path.dll for an additional trusted native module and
--mod-trace run.jsonl for execution traces. See MODDING.md.

Opening-game play, maps, NPCs, battle, menus, audio and repeated saving/loading
were verified on Windows x64 through Wine. Broad endgame/minigame coverage and
replacement networking for physical GBA link features remain unverified or
unimplemented. See PORTING.md and PORTING_PROGRESS.md for evidence and scope.

BUILDINFO.json identifies the reproducible build when supplied. SHA256SUMS.txt
lists packaged file hashes. THIRD_PARTY.md and the included licenses describe
the native-port changes and SDL2; these do not relicense upstream game assets.
