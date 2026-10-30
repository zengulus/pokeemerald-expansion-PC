#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "native.h"
#include "mod_host.h"
#include "agb_flash.h"
#include "gba/flash_internal.h"
#include "save.h"
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%d: %s\n", __LINE__, #c); exit(1); } } while (0)
static const struct RhhModDefinition sMod = {
    .struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1,
    .state_schema = 1, .id = "test:save", .flags = RHH_MOD_REQUIRED, .state_size = 1
};
static void Write(u32 generation, u8 value)
{
    *(u8 *)RhhModsState(sMod.id) = value;
    CHECK(NativeSaveBegin() == 0);
    CHECK(!NativeSaveSpecialWrite(SECTOR_ID_HOF_1, &value, sizeof(value)));
    CHECK(NativeSaveEnd(generation, 1) == 0);
}
int main(int argc, char **argv)
{
    CHECK(argc == 3); RhhModsInit();
    if (!strcmp(argv[1], "missing"))
    { CHECK(NativeSaveOpen(argv[2]) == -1); NativeSaveClose(); return 0; }
    CHECK(RhhModsRegister(&sMod) == 0 && RhhModsFinalize() == 0);
    CHECK(NativeSaveOpen(argv[2]) == 0);
    u8 *state = RhhModsState(sMod.id);
    if (!strcmp(argv[1], "write"))
    {
        Write(1, 11); Write(2, 22); Write(2, 23); /* same-generation link save */
        const u8 journal[] = {0xde, 0xad, 0xbe, 0xef};
        CHECK(NativeSaveNewRunSerial() == 1);
        CHECK(NativeSaveSetJournal(7, journal, sizeof(journal)) == 0);
        CHECK(NativeSaveSetJournal(8, journal, sizeof(journal)) == 0);
    }
    else if (!strcmp(argv[1], "legacy"))
    {
        CHECK(*state == 0 && NativeSaveRestoreMods(1) == 0);
        u8 marker; CHECK(!NativeSaveSpecialRead(SECTOR_ID_RECORDED_BATTLE, &marker, 1) && marker == 0x5a);
        CHECK(NativeSaveBegin() == 0 && NativeSaveEnd(1, 1) == 0);
    }
    else if (!strcmp(argv[1], "legacy-read"))
    { u8 marker; CHECK(*state == 0 && NativeSaveRestoreMods(1) == 0
        && !NativeSaveSpecialRead(SECTOR_ID_RECORDED_BATTLE, &marker, 1) && marker == 0x5a); }
    else
    {
        u8 special; CHECK(*state == 23 && !NativeSaveSpecialRead(SECTOR_ID_HOF_1, &special, 1) && special == 23);
        CHECK(NativeSaveNewRunSerial() == 2);
        u32 length; const u8 *journal = NativeSaveGetJournal(7, &length);
        CHECK(length == 4 && journal[0] == 0xde && journal[3] == 0xef);
        CHECK(NativeSaveGetJournal(8, &length) != NULL && length == 4);
        CHECK(NativeSaveRestoreMods(1) == 0 && *state == 11);
        CHECK(NativeSaveRestoreMods(2) == 0 && *state == 23);
        CHECK(NativeSaveRestoreMods(999) == -1 && *state == 23);
        *state = 33;
        CHECK(NativeSaveBegin() == 0);
        special = 33; CHECK(!NativeSaveSpecialWrite(SECTOR_ID_HOF_1, &special, 1));
        CHECK(NativeSaveEnd(3, !strcmp(argv[1], "fail")) == -1);
        CHECK(!NativeSaveSpecialRead(SECTOR_ID_HOF_1, &special, 1) && special == 23);
    }
    NativeSaveClose(); RhhModsShutdown();
    puts("native mod snapshots, transaction rollback and legacy migration passed");
    return 0;
}
