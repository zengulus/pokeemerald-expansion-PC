#include "mod_host.h"
#include <SDL2/SDL.h>
#include <string.h>

static void *sLibraries[32];
static unsigned sLibraryCount;

int RhhModsLoadLibrary(const char *path)
{
    if (sLibraryCount == 32) return -1;
    void *library = SDL_LoadObject(path);
    if (!library) { fprintf(stderr, "Mod load failed: %s\n", SDL_GetError()); return -1; }
    RhhModEntryV1 entry = (RhhModEntryV1)SDL_LoadFunction(library, "RHH_ModEntry_v1");
    const struct RhhModDefinition *definition = entry ? entry(RhhModsHostAPI()) : NULL;
    if (!definition || RhhModsRegister(definition))
    {
        fprintf(stderr, "Mod rejected: %s (%s)\n", path, RhhModsError());
        SDL_UnloadObject(library);
        return -1;
    }
    sLibraries[sLibraryCount++] = library;
    return 0;
}
void RhhModsUnloadLibraries(void)
{
    /* Definitions and callbacks belong to the libraries: destroy state first. */
    RhhModsShutdown();
    while (sLibraryCount) SDL_UnloadObject(sLibraries[--sLibraryCount]);
}
