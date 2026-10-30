#include "link.h"
#include "recorded_battle.h"
#include "gba/flash_internal.h"
#include "util.h"
enum { TEST_RUN_SCHEMA3_SIZE = 32 + RHH_RUNTIME_MOVE_COUNT * 8 };
static u8 *NatureFormatRecord(const u8 *snapshot, u32 length, u32 *outLength)
{
    u32 at = 24;
    while (at + 80 <= length && strcmp((const char *)snapshot + at, "rhh:host_run"))
        at += 80 + ClockTestGet32(snapshot + at + 68);
    CHECK(at + 80 <= length);
    u32 payload = ClockTestGet32(snapshot + at + 68);
    CHECK(at + 80 + payload <= length);
    *outLength = 24 + 80 + payload;
    u8 *isolated = calloc(*outLength, 1); CHECK(isolated);
    memcpy(isolated, snapshot, 24); memcpy(isolated + 24, snapshot + at, 80 + payload);
    ClockTestPut32(isolated + 12, 1); ClockTestEnvelope(isolated, *outLength);
    return isolated;
}
static int OldNatureFormatMigrate(u32 schema, const u8 *old, u32 oldSize, u8 *dst, u32 size)
{
    if (size != TEST_RUN_SCHEMA3_SIZE || !((schema == 1 && oldSize == 24) || (schema == 2 && oldSize == 32))) return -1;
    memcpy(dst, old, oldSize); memset(dst + oldSize, 0, size - oldSize); return 0;
}
static const struct RhhModDefinition sOldNatureFormatRun = {
    .struct_size = sizeof(sOldNatureFormatRun), .abi_version = RHH_MOD_ABI_V1, .mod_version = 3,
    .state_schema = 3, .id = "rhh:host_run", .flags = RHH_MOD_REQUIRED,
    .state_size = TEST_RUN_SCHEMA3_SIZE, .migrate = OldNatureFormatMigrate
};
static int InvalidNatureProjection(const char *id, const u8 *state, u32 stateSize, u32 *schema, u32 *size)
{ (void)id; (void)state; *size = stateSize + 1; *schema = 99; return 0; }
static void CheckNatureFormatSave(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsFinalize());
    CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    CHECK(!ModsNatureFormatEnabled() && ModsEnableNatureFormat());
    gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
    CHECK(!ModsNatureFormatEnabled());
    u8 *legacy = NULL, *active = NULL; u32 legacyLength = 0, activeLength = 0;
    CHECK(!RhhModsExport(&legacy, &legacyLength));
    u32 isolatedLegacyLength; u8 *isolatedLegacy = NatureFormatRecord(legacy, legacyLength, &isolatedLegacyLength);
    CHECK(ClockTestGet32(isolatedLegacy + 24 + 64) == 3
        && ClockTestGet32(isolatedLegacy + 24 + 68) == TEST_RUN_SCHEMA3_SIZE);
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); u32 legacyGeneration = gSaveCounter;
    CHECK(!NativeSaveBegin() && ModsEnableNatureFormat() && !ModsNatureFormatEnabled());
    CHECK(NativeSaveEnd(legacyGeneration, FALSE) && !ModsNatureFormatEnabled());
    const struct RhhNatureService *natureService = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/nature", RHH_SERVICE_VERSION(1, 3));
    struct RhhNatureFormatCheck check = {.struct_size = sizeof(check)}, tiny = {.struct_size = 1, .status = 999}, oldTiny = tiny;
    CHECK(natureService && natureService->header.struct_size >= RHH_NATURE_SERVICE_V1_3_SIZE
        && natureService->format_preflight(&tiny) && !memcmp(&tiny, &oldTiny, sizeof(tiny))
        && !natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_READY);
    struct Pokemon mon;
    CreateMonWithIVs(&mon, SPECIES_TORCHIC, 10, 12345, OTID_STRUCT_PRESET(54321), 5);
    struct BoxPokemon modified = mon.box;
    CHECK(!NativeNatureStorageWrite(&modified, (GetNatureFromPersonality(mon.box.personality) + 1) % NUM_NATURES));
    struct BoxPokemon *slots[] = {&gParties[B_TRAINER_PLAYER][0].box, &gSaveBlock1Ptr->playerParty[1].box,
        &gPokemonStoragePtr->boxes[2][3], &gSaveBlock1Ptr->daycare.mons[0].mon, &gPokemonStoragePtr->fusions[0].box};
    const u32 locations[] = {RHH_MON_PARTY, RHH_NATURE_SAVED_PARTY, RHH_MON_BOX, RHH_MON_DAYCARE, RHH_MON_FUSION_STORAGE};
    const u32 areas[] = {0, 0, 2, 0, 0}, indices[] = {0, 1, 3, 0, 0};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    for (u32 i = 0; i < ARRAY_COUNT(slots); ++i)
    {
        struct BoxPokemon saved = *slots[i]; *slots[i] = modified;
        struct BoxPokemon before = *slots[i];
        CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_INCOMPATIBLE
            && check.location == locations[i] && check.box == areas[i] && check.slot == indices[i]
            && ModsEnableNatureFormat() && !ModsNatureFormatEnabled() && !memcmp(&before, slots[i], sizeof(before)));
        u8 highLegacyBits = 63; SetBoxMonData(slots[i], MON_DATA_NATIVE_NATURE_CODE, &highLegacyBits); before = *slots[i];
        CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_INCOMPATIBLE
            && check.location == locations[i] && check.box == areas[i] && check.slot == indices[i]
            && ModsEnableNatureFormat() && !ModsNatureFormatEnabled() && !memcmp(&before, slots[i], sizeof(before)));
        *slots[i] = mon.box; slots[i]->secure.raw[0] ^= 1; before = *slots[i];
        CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_UNREADABLE
            && check.location == locations[i] && check.box == areas[i] && check.slot == indices[i]
            && ModsEnableNatureFormat() && !ModsNatureFormatEnabled() && !memcmp(&before, slots[i], sizeof(before)));
        *slots[i] = saved;
    }
    struct RecordedBattleSave savedRecord, recorded = {0}, observed;
    int hadRecord = !NativeSaveSpecialRead(SECTOR_ID_RECORDED_BATTLE, &savedRecord, sizeof(savedRecord));
    recorded.battleFlags = BATTLE_TYPE_TRAINER;
    recorded.parties[2][3] = mon;
    recorded.checksum = CalcByteArraySum((const u8 *)&recorded, sizeof(recorded) - sizeof(recorded.checksum));
    CHECK(!NativeSaveSpecialWrite(SECTOR_ID_RECORDED_BATTLE, &recorded, sizeof(recorded)));
    CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_READY
        && !NativeSaveSpecialRead(SECTOR_ID_RECORDED_BATTLE, &observed, sizeof(observed))
        && !memcmp(&recorded, &observed, sizeof(recorded)));
    recorded.parties[2][3].box = modified;
    recorded.checksum = CalcByteArraySum((const u8 *)&recorded, sizeof(recorded) - sizeof(recorded.checksum));
    CHECK(!NativeSaveSpecialWrite(SECTOR_ID_RECORDED_BATTLE, &recorded, sizeof(recorded)));
    CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_INCOMPATIBLE
        && check.location == RHH_NATURE_RECORDED_BATTLE && check.box == 2 && check.slot == 3
        && ModsEnableNatureFormat() && !ModsNatureFormatEnabled());
    struct RecordedBattleSave versioned;
    CHECK(!NativeNatureRecordingEncode(&recorded, 1, &versioned));
    CHECK(!NativeSaveSpecialWrite(SECTOR_ID_RECORDED_BATTLE, &versioned, sizeof(versioned)));
    CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_READY
        && !NativeSaveSpecialRead(SECTOR_ID_RECORDED_BATTLE, &observed, sizeof(observed))
        && !memcmp(&versioned, &observed, sizeof(versioned)));
    recorded.parties[2][3].box = mon.box; recorded.parties[2][3].box.secure.raw[0] ^= 1;
    recorded.checksum = CalcByteArraySum((const u8 *)&recorded, sizeof(recorded) - sizeof(recorded.checksum));
    CHECK(!NativeSaveSpecialWrite(SECTOR_ID_RECORDED_BATTLE, &recorded, sizeof(recorded)));
    CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_UNREADABLE
        && check.location == RHH_NATURE_RECORDED_BATTLE && check.box == 2 && check.slot == 3);
    recorded.checksum ^= 1;
    CHECK(!NativeSaveSpecialWrite(SECTOR_ID_RECORDED_BATTLE, &recorded, sizeof(recorded)));
    CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_UNREADABLE
        && check.location == RHH_NATURE_RECORDED_BATTLE && check.box == UINT32_MAX && check.slot == UINT32_MAX);
    CHECK(!NativeSaveSpecialWrite(SECTOR_ID_RECORDED_BATTLE, hadRecord ? &savedRecord : NULL,
                                  hadRecord ? sizeof(savedRecord) : 0));
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gMain.inBattle = TRUE;
    CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_UNAVAILABLE
        && ModsEnableNatureFormat() && !ModsNatureFormatEnabled());
    gMain.inBattle = FALSE; gReceivedRemoteLinkPlayers = TRUE;
    CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_UNAVAILABLE
        && ModsEnableNatureFormat() && !ModsNatureFormatEnabled());
    gReceivedRemoteLinkPlayers = FALSE; gWirelessCommType = TRUE;
    CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_UNAVAILABLE
        && ModsEnableNatureFormat() && !ModsNatureFormatEnabled());
    gWirelessCommType = FALSE;
    CHECK(!ModsEnableNatureFormat() && ModsNatureFormatEnabled() && !ModsEnableNatureFormat());
    CHECK(!natureService->format_preflight(&check) && check.status == RHH_NATURE_FORMAT_ACTIVE);
    CHECK(!RhhModsExport(&active, &activeLength));
    u32 isolatedActiveLength; u8 *isolatedActive = NatureFormatRecord(active, activeLength, &isolatedActiveLength);
    CHECK(ClockTestGet32(isolatedActive + 24 + 64) == 4
        && ClockTestGet32(isolatedActive + 24 + 68) == TEST_RUN_SCHEMA3_SIZE + 8
        && isolatedActive[24 + 80 + TEST_RUN_SCHEMA3_SIZE] == 1);
    u8 *invalid = malloc(activeLength); CHECK(invalid); memcpy(invalid, active, activeLength);
    u32 at = 24;
    while (strcmp((const char *)invalid + at, "rhh:host_run")) at += 80 + ClockTestGet32(invalid + at + 68);
    invalid[at + 80 + TEST_RUN_SCHEMA3_SIZE] = 2; ClockTestEnvelope(invalid, activeLength);
    CHECK(RhhModsImport(invalid, activeLength) && ModsNatureFormatEnabled()); free(invalid);
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); u32 activeGeneration = gSaveCounter;
    CHECK(activeGeneration != legacyGeneration && !NativeSaveRestoreMods(legacyGeneration) && !ModsNatureFormatEnabled()
        && !NativeSaveRestoreMods(activeGeneration) && ModsNatureFormatEnabled());
    NativeSaveClose(); CHECK(!NativeSaveOpen(path) && LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && ModsNatureFormatEnabled());
    NativeSaveClose(); free(legacy); free(active);

    /* Simulate the previous host's exact required-record schema/migrator. */
    RhhModsInit(); CHECK(!RhhModsRegister(&sOldNatureFormatRun) && !RhhModsFinalize());
    CHECK(!RhhModsImport(isolatedLegacy, isolatedLegacyLength));
    u8 *oldState = RhhModsState("rhh:host_run"), *before = malloc(TEST_RUN_SCHEMA3_SIZE); CHECK(oldState && before);
    memcpy(before, oldState, TEST_RUN_SCHEMA3_SIZE);
    CHECK(RhhModsImport(isolatedActive, isolatedActiveLength) && !memcmp(oldState, before, TEST_RUN_SCHEMA3_SIZE));
    free(before); free(isolatedLegacy); free(isolatedActive);
    RhhModsInit(); CHECK(!RhhModsSetSaveProjection(InvalidNatureProjection)
        && !RhhModsRegister(&sOldNatureFormatRun) && !RhhModsFinalize());
    u8 *badExport = NULL; u32 badSize = 0;
    CHECK(RhhModsExport(&badExport, &badSize) && !badExport);
    RhhModsShutdown(); puts("nature format: conditional save version, rollback and old-reader rejection passed");
}
