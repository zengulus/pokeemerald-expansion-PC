# Gameplay input replays

These are held-key events (`frame hexadecimal-mask`) consumed by the actual
SDL game. They do not create Pokémon, set story flags, teleport, or bypass
combat. Run `python3 native/tests/replay.py --target linux32` or `windows32`.
The runner uses a fixed diagnostic RTC to make the game RNG seed repeatable.

The phases start a new game, name the player, leave the truck, complete Mom's
moving-in dialogue and save; reload and set the clock, complete the TV event
and save; reload, visit the rival, enter Route 101, choose Torchic, win the
starter battle, nickname it, finish Birch's dialogue and save. Fresh processes
then Continue, Save again and Continue again, comparing persistent state.

Every run creates a new artifact directory containing the real save, reports,
traces, logs and BMP screenshots. The subsystem and gameplay regression
binaries are separate checks. Interactive controls are Z/X, Enter, Backspace,
arrow keys and A/S for the shoulder buttons.
