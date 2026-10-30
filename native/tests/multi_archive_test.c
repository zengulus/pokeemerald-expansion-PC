#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "native.h"
#include "mod_host.h"
#include "agb_flash.h"
#include "gba/flash_internal.h"
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%d: %s\n", __LINE__, #c); exit(1); } } while (0)

static u8 sPhoto, sIdentity;
static bool32 sRejectImport;
static const struct RhhModDefinition sMod = {.struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:archive_state", .state_schema = 1, .flags = RHH_MOD_REQUIRED, .state_size = 1};
static int ExportPart(u8 value, const char *magic, u32 prefix, u8 **data, u32 *size)
{
    *data = NULL; *size = 0; if (!value) return 0;
    *data = malloc(prefix + 1); if (!*data) return -1;
    memcpy(*data, magic, prefix); (*data)[prefix] = value; *size = prefix + 1; return 0;
}
static int PhotoExport(u8 **data, u32 *size) { return ExportPart(sPhoto, "TESTOLD1", 8, data, size); }
static int IdentityExport(u8 **data, u32 *size) { return ExportPart(sIdentity, "ID02", 4, data, size); }
static int PhotoValidate(const u8 *data, u32 size)
{ return !size ? !!data : !data || size != 9 || memcmp(data, "TESTOLD1", 8) || !data[8] || data[8] > 99; }
static int IdentityValidate(const u8 *data, u32 size)
{ return !size ? !!data : !data || size != 5 || memcmp(data, "ID02", 4) || !data[4] || data[4] > 99; }
static int PhotoImport(const u8 *data, u32 size)
{ if (PhotoValidate(data, size)) return -1; sPhoto = size ? data[8] : 0; return 0; }
static int IdentityImport(const u8 *data, u32 size)
{
    if (IdentityValidate(data, size)) return -1;
    if (sRejectImport) { sRejectImport = FALSE; return -1; }
    sIdentity = size ? data[4] : 0; return 0;
}
static void PhotoReset(void) { sPhoto = 0; }
static void IdentityReset(void) { sIdentity = 0; }
static const struct NativeSaveArchiveCodec sPhotoCodec = {
    PhotoExport, PhotoValidate, PhotoImport, PhotoReset, "test:photo", "TESTOLD1"
};
static const struct NativeSaveArchiveCodec sIdentityCodec = {
    IdentityExport, IdentityValidate, IdentityImport, IdentityReset, "test:identity", NULL
};
static void Register(bool32 reverse, bool32 identity)
{
    RhhModsInit(); CHECK(!RhhModsRegister(&sMod) && !RhhModsFinalize());
    if (reverse && identity) CHECK(!NativeSaveRegisterArchive(&sIdentityCodec));
    CHECK(!NativeSaveRegisterArchive(&sPhotoCodec));
    if (!reverse && identity) CHECK(!NativeSaveRegisterArchive(&sIdentityCodec));
    CHECK(!NativeSaveRegisterArchive(&sPhotoCodec));
    struct NativeSaveArchiveCodec duplicate = sIdentityCodec; duplicate.id = sPhotoCodec.id;
    CHECK(NativeSaveRegisterArchive(&duplicate)); duplicate.id = "test:other"; duplicate.legacy_magic = "TESTOLD1";
    CHECK(NativeSaveRegisterArchive(&duplicate));
}
static void Save(u32 generation, u8 photo, u8 identity)
{
    sPhoto = photo; sIdentity = identity; *(u8 *)RhhModsState(sMod.id) = photo;
    CHECK(!NativeSaveBegin() && !ProgramFlashByte(0, 5, photo) && !NativeSaveEnd(generation, 1));
}
int main(int argc, char **argv)
{
    CHECK(argc == 3);
    bool32 reverse = !strcmp(argv[1], "write-reverse");
    bool32 identity = strcmp(argv[1], "legacy-write") && strcmp(argv[1], "missing");
    Register(reverse, identity);
    if (!strcmp(argv[1], "fail-open"))
    {
        sRejectImport = TRUE;
        CHECK(NativeSaveOpen(argv[2]) && !sPhoto && !sIdentity
            && !*(u8 *)RhhModsState(sMod.id) && !RhhModsSettingsLocked());
        NativeSaveClose(); RhhModsShutdown(); return 0;
    }
    if (!strcmp(argv[1], "reject") || !strcmp(argv[1], "missing"))
    { CHECK(NativeSaveOpen(argv[2])); NativeSaveClose(); RhhModsShutdown(); return 0; }
    CHECK(!NativeSaveOpen(argv[2]));
    if (!strcmp(argv[1], "write") || reverse)
    {
        Save(1, 11, 0); Save(2, 22, 33);
        CHECK(!RhhModsSettingsLocked()); sRejectImport = TRUE;
        CHECK(NativeSaveRestoreMods(1) && sPhoto == 22 && sIdentity == 33
            && *(u8 *)RhhModsState(sMod.id) == 22 && !RhhModsSettingsLocked());
        CHECK(!NativeSaveRestoreMods(1) && sPhoto == 11 && sIdentity == 0 && *(u8 *)RhhModsState(sMod.id) == 11);
        sRejectImport = TRUE;
        CHECK(NativeSaveRestoreMods(2) && sPhoto == 11 && sIdentity == 0
            && *(u8 *)RhhModsState(sMod.id) == 11 && RhhModsSettingsLocked());
        CHECK(!NativeSaveRestoreMods(2) && sPhoto == 22 && sIdentity == 33 && *(u8 *)RhhModsState(sMod.id) == 22);
    }
    else if (!strcmp(argv[1], "legacy-write")) Save(1, 11, 0);
    else if (!strcmp(argv[1], "legacy-read"))
    {
        CHECK(sPhoto == 11 && sIdentity == 0 && !NativeSaveRestoreMods(1));
        Save(2, 22, 33);
    }
    else if (!strcmp(argv[1], "read"))
    {
        CHECK(sPhoto == 22 && sIdentity == 33 && *(u8 *)RhhModsState(sMod.id) == 22);
        CHECK(!NativeSaveRestoreMods(1) && sPhoto == 11 && !sIdentity && *(u8 *)RhhModsState(sMod.id) == 11);
        CHECK(!NativeSaveRestoreMods(2) && sPhoto == 22 && sIdentity == 33 && *(u8 *)RhhModsState(sMod.id) == 22);
    }
    else CHECK(0);
    NativeSaveClose(); RhhModsShutdown(); puts("multiple archives and legacy generations passed"); return 0;
}
