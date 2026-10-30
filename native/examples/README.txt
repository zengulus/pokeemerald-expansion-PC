The nine bundled examples use only the public C API and standard C headers.
They have independent IDs, settings and portable state. See MODDING.md for
ownership, hook contexts, composition, migrations and save compatibility.

In the extracted Windows distribution, compile an example from the folder
containing pokeemerald-expansion.exe (MinGW-w64 GCC required):

  x86_64-w64-mingw32-gcc -std=gnu17 -shared -static-libgcc -DRHH_MOD_SHARED -iquote examples/include examples/mods/nuzlocke.c -o example-mods/nuzlocke.dll

Use i686-w64-mingw32-gcc for the Windows x86 distribution. The compiler, DLLs
and executable must have matching architectures. Start all nine external
examples with:

  pokeemerald-expansion.exe --external-bundled-mods example-mods

The built-in copies are replaced in this mode, so module IDs do not collide.
Keep all nine DLLs in the directory. Their initial settings remain off; use
the original New Game screen to configure a run. Music is editable in Options.
Saved state works between the built-in and external versions of these sources.
When changing a saved layout, increment its schema and provide a migration.

level_cap_mod.c is a separate, complete stateless API example with its own ID:

  x86_64-w64-mingw32-gcc -std=gnu17 -shared -static-libgcc -iquote examples/include examples/level_cap_mod.c -o cap50.dll
  pokeemerald-expansion.exe --mod cap50.dll --mod-trace cap50.jsonl

It caps levels at 50 and composes with other caps by minimum. Native modules
are trusted code. The API does not sandbox them or add new engine table IDs.
