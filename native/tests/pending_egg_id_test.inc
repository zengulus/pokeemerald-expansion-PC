static int OldPendingFormatMigrate(u32 schema, const u8 *old, u32 oldSize, u8 *dst, u32 size)
{
    if (size != TEST_RUN_SCHEMA3_SIZE + 8 || !((schema == 1 && oldSize == 24)
        || (schema == 2 && oldSize == 32) || (schema == 3 && oldSize == TEST_RUN_SCHEMA3_SIZE))) return -1;
    memcpy(dst, old, oldSize); memset(dst + oldSize, 0, size - oldSize); return 0;
}
static const struct RhhModDefinition sOldPendingFormatRun = {
    .struct_size = sizeof(sOldPendingFormatRun), .abi_version = RHH_MOD_ABI_V1, .mod_version = 4,
    .state_schema = 4, .id = "rhh:host_run", .flags = RHH_MOD_REQUIRED,
    .state_size = TEST_RUN_SCHEMA3_SIZE + 8, .migrate = OldPendingFormatMigrate
};
static void CheckPendingEggIds(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsFinalize());
    CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 15));
    CHECK(breeding); FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct RhhDaycareEggId a = {.struct_size = sizeof(a)}, b = a, c = a, check = a;
    CHECK(breeding->pending_id(&check) && breeding->pending_change_id(&check));
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    TriggerPendingDaycareEgg(); CHECK(!breeding->pending_id(&a) && a.run_id == RhhModsRunId() && (a.high || a.low));
    struct RhhBreedingOffer first = {.struct_size = sizeof(first)}, second = {.struct_size = sizeof(second)};
    CHECK(!breeding->offer(&first));
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    u8 *snapshot = NULL; u32 length = 0, isolated5Length = 0;
    CHECK(!RhhModsExport(&snapshot, &length));
    u8 *isolated5 = NatureFormatRecord(snapshot, length, &isolated5Length); free(snapshot);
    CHECK(ClockTestGet32(isolated5 + 24 + 64) == 5 && ClockTestGet32(isolated5 + 24 + 68) == TEST_RUN_SCHEMA3_SIZE + 24);
    u8 *invalid = malloc(isolated5Length); CHECK(invalid); memcpy(invalid, isolated5, isolated5Length);
    memset(invalid + 24 + 80 + TEST_RUN_SCHEMA3_SIZE + 8, 0, 16); ClockTestEnvelope(invalid, isolated5Length);
    CHECK(RhhModsImport(invalid, isolated5Length) && !breeding->pending_id(&check) && !memcmp(&a, &check, sizeof(a)));
    free(invalid);
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); u32 generationA = gSaveCounter;
    RejectEggFromDayCare(); CHECK(breeding->pending_id(&check));
    CHECK(!ModsEnableNatureFormat()); snapshot = NULL;
    CHECK(!RhhModsExport(&snapshot, &length));
    u32 isolated4Length = 0; u8 *isolated4 = NatureFormatRecord(snapshot, length, &isolated4Length); free(snapshot);
    CHECK(ClockTestGet32(isolated4 + 24 + 64) == 4 && ClockTestGet32(isolated4 + 24 + 68) == TEST_RUN_SCHEMA3_SIZE + 8);
    TriggerPendingDaycareEgg(); CHECK(!breeding->pending_id(&b) && (a.high != b.high || a.low != b.low));
    CHECK(!breeding->offer(&second) && first.token != second.token);
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); u32 generationB = gSaveCounter;
    CHECK(generationA != generationB && !NativeSaveRestoreMods(generationA) && !breeding->pending_id(&check)
        && !memcmp(&a, &check, sizeof(a)) && !ModsNatureFormatEnabled());
    CHECK(!NativeSaveRestoreMods(generationB) && !breeding->pending_id(&check)
        && !memcmp(&b, &check, sizeof(b)) && ModsNatureFormatEnabled());
    NativeSaveClose(); CHECK(!NativeSaveOpen(path) && LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK
        && !breeding->pending_id(&check) && !memcmp(&b, &check, sizeof(b)));
    CHECK(!NativeSaveRestoreMods(generationA) && !breeding->pending_id(&check) && !memcmp(&a, &check, sizeof(a)));
    RejectEggFromDayCare(); TriggerPendingDaycareEgg();
    CHECK(!breeding->pending_id(&c) && (c.high != a.high || c.low != a.low) && (c.high != b.high || c.low != b.low));
    RejectEggFromDayCare(); FlagSet(FLAG_PENDING_DAYCARE_EGG);
    CHECK(breeding->pending_id(&check) && TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK
        && !breeding->pending_id(&check));
    c = check; NativeSaveClose();
    CHECK(!NativeSaveOpen(path) && LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK
        && !breeding->pending_id(&check) && !memcmp(&c, &check, sizeof(c)));
    NativeSaveClose();
    size_t idPathSize = strlen(path) + 4; char *idPath = malloc(idPathSize); CHECK(idPath);
    snprintf(idPath, idPathSize, "%s.id", path);
    FILE *idFile = fopen(idPath, "wb"); CHECK(idFile && fwrite(&c, sizeof(c), 1, idFile) == 1 && !fclose(idFile));
    free(idPath);
    /* A previous schema-4 host reads unmarked state and rejects the new ID. */
    RhhModsInit(); CHECK(!RhhModsRegister(&sOldPendingFormatRun) && !RhhModsFinalize());
    CHECK(!RhhModsImport(isolated4, isolated4Length));
    u8 *old = RhhModsState("rhh:host_run"), *saved = malloc(TEST_RUN_SCHEMA3_SIZE + 8); CHECK(old && saved);
    memcpy(saved, old, TEST_RUN_SCHEMA3_SIZE + 8);
    CHECK(RhhModsImport(isolated5, isolated5Length) && !memcmp(old, saved, TEST_RUN_SCHEMA3_SIZE + 8));
    free(saved); free(isolated4); free(isolated5); RhhModsShutdown();
    puts("pending egg identity: save, rollback, new branch and older reader passed");
}
static void CheckPendingEggIdReload(const char *path)
{
    size_t idPathSize = strlen(path) + 4; char *idPath = malloc(idPathSize); CHECK(idPath);
    snprintf(idPath, idPathSize, "%s.id", path);
    struct RhhDaycareEggId expected, actual = {.struct_size = sizeof(actual)};
    FILE *idFile = fopen(idPath, "rb"); CHECK(idFile && fread(&expected, sizeof(expected), 1, idFile) == 1 && !fclose(idFile));
    free(idPath);
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsFinalize());
    CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 15));
    CHECK(breeding && !breeding->pending_id(&actual) && !memcmp(&expected, &actual, sizeof(actual)));
    NativeSaveClose(); RhhModsShutdown(); puts("pending egg identity: fresh-process reload passed");
}
