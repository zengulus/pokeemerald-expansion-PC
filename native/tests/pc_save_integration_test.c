#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "native.h"
#include "mod_host.h"
#include "save.h"
#include "hall_of_fame.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "PC save integration: %s (line %d)\n", #c, __LINE__); exit(1); } } while (0)
static const struct RhhModDefinition sMod = {
    .struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:pc_save", .state_size = 1, .state_schema = 1, .flags = RHH_MOD_REQUIRED
};
static u8 sFirst[16], sSecond[17], sStorage[256 * 1024 + 17], sFourth[4];
static void *const sRegions[4] = {sFirst, sSecond, sStorage, sFourth};
static const size_t sSizes[4] = {sizeof(sFirst), sizeof(sSecond), sizeof(sStorage), sizeof(sFourth)};

static void Fill(u8 value)
{
    for (unsigned i = 0; i < 4; ++i) memset(sRegions[i], value, sSizes[i]);
    *(u8 *)RhhModsState(sMod.id) = value;
}
static void Save(u32 generation, u8 value, bool32 retainStorage, bool32 success)
{
    Fill(value);
    CHECK(!NativeSaveBegin());
    CHECK(!NativeSaveStageGame((const void *const *)sRegions, sSizes, retainStorage));
    CHECK((NativeSaveEnd(generation, success) == 0) == success);
}
static void Load(u32 expected, u8 ordinary, u8 storage)
{
    Fill(0xff);
    u32 generation = 0;
    CHECK(NativeSaveGameAvailable());
    CHECK(!NativeSaveLoadGame(sRegions, sSizes, &generation) && generation == expected);
    CHECK(sFirst[0] == ordinary && sSecond[16] == ordinary && sFourth[3] == ordinary
        && sStorage[0] == storage && sStorage[sizeof(sStorage) - 1] == storage
        && *(u8 *)RhhModsState(sMod.id) == ordinary);
}
static void CheckSpecial(u8 expected)
{
    u8 special[8193];
    CHECK(!NativeSaveSpecialRead(SECTOR_ID_RECORDED_BATTLE, special, sizeof(special)));
    CHECK(special[0] == expected && special[sizeof(special) - 1] == expected);
}
static void CheckHall(void)
{
    size_t bytes = 0, count = 1000;
    CHECK(!NativeSaveSpecialSize(SECTOR_ID_HOF_1, &bytes) && bytes == count * sizeof(struct HallofFameTeam));
    struct HallofFameTeam *teams = malloc(bytes); CHECK(teams);
    CHECK(!NativeSaveSpecialRead(SECTOR_ID_HOF_1, teams, bytes));
    for (size_t i = 0; i < count; ++i) CHECK(teams[i].mon[0].tid == i + 1);
    free(teams);
}
int main(int argc, char **argv)
{
    CHECK(argc == 3);
    RhhModsInit(); CHECK(!RhhModsRegister(&sMod) && !RhhModsFinalize());
    if (!strcmp(argv[1], "reject"))
    { CHECK(NativeSaveOpen(argv[2])); NativeSaveClose(); RhhModsShutdown(); return 0; }
    CHECK(!NativeSaveOpen(argv[2]));
    if (!strcmp(argv[1], "write"))
    {
        CHECK(!NativeSaveGameAvailable());
        u8 special[8193]; memset(special, 7, sizeof(special));
        CHECK(!NativeSaveSpecialWrite(SECTOR_ID_RECORDED_BATTLE, special, sizeof(special)));
        size_t count = 1000, bytes = count * sizeof(struct HallofFameTeam);
        struct HallofFameTeam *teams = calloc(count, sizeof(*teams)); CHECK(teams);
        for (size_t i = 0; i < count; ++i) teams[i].mon[0].tid = i + 1;
        CHECK(!NativeSaveSpecialWrite(SECTOR_ID_HOF_1, teams, bytes)); free(teams);
        Save(1, 1, FALSE, TRUE); Save(2, 2, FALSE, TRUE); Save(3, 3, TRUE, TRUE);
        Load(3, 3, 2);
        Save(4, 4, FALSE, FALSE); Load(3, 3, 2);
        CHECK(!NativeSaveBegin()); memset(special, 8, sizeof(special));
        CHECK(!NativeSaveSpecialWrite(SECTOR_ID_RECORDED_BATTLE, special, sizeof(special)));
        CHECK(NativeSaveEnd(4, FALSE)); CheckSpecial(7);
    }
    else if (!strcmp(argv[1], "read")) { Load(3, 3, 2); CheckSpecial(7); CheckHall(); }
    else if (!strcmp(argv[1], "recover")) { Load(2, 2, 2); CheckSpecial(7); CheckHall(); }
    else if (!strcmp(argv[1], "fail"))
    {
        Load(3, 3, 2); Fill(4); CHECK(!NativeSaveBegin());
        CHECK(!NativeSaveStageGame((const void *const *)sRegions, sSizes, FALSE));
        CHECK(NativeSaveEnd(4, TRUE)); Load(3, 3, 2); CheckSpecial(7);
    }
    else if (!strcmp(argv[1], "erase-fail"))
    { CHECK(NativeSaveEraseGame()); Load(3, 3, 2); CheckSpecial(7); }
    else if (!strcmp(argv[1], "erase"))
    {
        CHECK(!NativeSaveEraseGame() && !NativeSaveGameAvailable());
        u8 empty[8193]; CHECK(NativeSaveSpecialRead(SECTOR_ID_RECORDED_BATTLE, empty, sizeof(empty)) == 1);
    }
    else if (!strcmp(argv[1], "empty")) CHECK(!NativeSaveGameAvailable());
    else CHECK(FALSE);
    NativeSaveClose(); RhhModsShutdown();
    return 0;
}
