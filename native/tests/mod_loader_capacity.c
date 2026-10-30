#include "mod_host.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s (%s)\n", __LINE__, #x, RhhModsError()); return 1; } } while (0)

int main(int argc, char **argv)
{
    CHECK(argc == 41);
    RhhModsInit();
    for (int i = 1; i < argc; ++i) CHECK(!RhhModsLoadLibrary(argv[i]));
    CHECK(!RhhModsFinalize());
    for (unsigned i = 0; i < 40; ++i)
    {
        char id[32]; snprintf(id, sizeof(id), "test:loader%03u", i);
        uint8_t *state = RhhModsState(id); CHECK(state); *state = i + 1;
    }
    uint8_t *data; uint32_t size;
    CHECK(!RhhModsExport(&data, &size)); RhhModsResetState(); CHECK(!RhhModsImport(data, size));
    for (unsigned i = 0; i < 40; ++i)
    {
        char id[32]; snprintf(id, sizeof(id), "test:loader%03u", i);
        uint8_t *state = RhhModsState(id); CHECK(state && *state == i + 1);
    }
    free(data); RhhModsUnloadLibraries();
    puts("40 independent libraries loaded and restored");
    return 0;
}
