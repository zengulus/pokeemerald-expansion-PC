static const struct RhhBreedingService *sOrdinalBreeding;
static u64 sExpectedDaycareOrdinal;
static u32 sDaycareOrdinalEvents;

static void ObserveDaycareOrdinal(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhDaycareEggOrdinal ordinal = {.struct_size = sizeof(ordinal), .ordinal = 99}, old = ordinal;
    struct RhhDaycareOrdinalState current = {.struct_size = sizeof(current)}, unchanged = current;
    CHECK(!sOrdinalBreeding->ordinal_state(&current));
    if (sExpectedDaycareOrdinal) {
        CHECK(!sOrdinalBreeding->egg_ordinal_context(&ordinal) && ordinal.run_id == RhhModsRunId()
            && ordinal.ordinal == sExpectedDaycareOrdinal && ordinal.party_slot < PARTY_SIZE
            && current.next == ordinal.ordinal + 1 && current.enabled);
    } else CHECK(sOrdinalBreeding->egg_ordinal_context(&ordinal) && !memcmp(&ordinal, &old, sizeof(ordinal)));
    CHECK(sOrdinalBreeding->enable_ordinal(RhhModsRunId(), &unchanged) && unchanged.next == 0);
    ++sDaycareOrdinalEvents;
}
static const struct RhhModHook sDaycareOrdinalHook = {
    sizeof(struct RhhModHook), RHH_BREED_EGG_CREATED, RHH_OBSERVE, 0, NULL, ObserveDaycareOrdinal
};
static const struct RhhModDefinition sDaycareOrdinalMod = {.struct_size = sizeof(sDaycareOrdinalMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:daycare_ordinal", .hooks = &sDaycareOrdinalHook, .hook_count = 1};

static int OldOrdinalFormatMigrate(u32 schema, const u8 *old, u32 oldSize, u8 *dst, u32 size)
{
    if (size != TEST_RUN_SCHEMA3_SIZE + 24 || !((schema == 3 && oldSize == TEST_RUN_SCHEMA3_SIZE)
        || (schema == 4 && oldSize == TEST_RUN_SCHEMA3_SIZE + 8))) return -1;
    memcpy(dst, old, oldSize); memset(dst + oldSize, 0, size - oldSize); return 0;
}
static const struct RhhModDefinition sOldOrdinalFormatRun = {
    .struct_size = sizeof(sOldOrdinalFormatRun), .abi_version = RHH_MOD_ABI_V1, .mod_version = 5,
    .state_schema = 5, .id = "rhh:host_run", .flags = RHH_MOD_REQUIRED,
    .state_size = TEST_RUN_SCHEMA3_SIZE + 24, .migrate = OldOrdinalFormatMigrate
};

static void CheckDaycareOrdinal(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sDaycareOrdinalMod)
        && !RhhModsFinalize() && !NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
    sOrdinalBreeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 25));
    CHECK(sOrdinalBreeding && sOrdinalBreeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_25_SIZE
        && RHH_BREEDING_SERVICE_V1_24_SIZE < RHH_BREEDING_SERVICE_V1_25_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 24)) == &sOrdinalBreeding->header
        && !RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 29)));
    struct RhhDaycareOrdinalState current = {.struct_size = sizeof(current)}, tiny = {.struct_size = 1, .next = 99}, saved = tiny;
    struct RhhDaycareEggOrdinal outside = {.struct_size = sizeof(outside), .ordinal = 99}, outsideSaved = outside;
    CHECK(!sOrdinalBreeding->ordinal_state(&current) && !current.enabled && !current.next
        && sOrdinalBreeding->ordinal_state(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny))
        && sOrdinalBreeding->egg_ordinal_context(&outside) && !memcmp(&outside, &outsideSaved, sizeof(outside)));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12321), 23);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    gMain.inBattle = FALSE; gMain.callback2 = CB2_Overworld; memset(&gPaletteFade, 0, sizeof(gPaletteFade));
    ScriptContext_Init(); UnlockPlayerFieldControls(); FlagClear(FLAG_PENDING_DAYCARE_EGG);
    sDaycareOrdinalEvents = 0; sExpectedDaycareOrdinal = 0;
    GiveEggFromDaycare(); CHECK(sDaycareOrdinalEvents == 1 && gPartiesCount[B_TRAINER_PLAYER] == 1);
    CHECK(!sOrdinalBreeding->ordinal_state(&current) && !current.enabled && !current.next);

    u8 *snapshot = NULL; u32 length = 0, oldLength = 0;
    CHECK(!RhhModsExport(&snapshot, &length));
    u8 *oldFormat = NatureFormatRecord(snapshot, length, &oldLength); free(snapshot);
    CHECK(ClockTestGet32(oldFormat + 24 + 64) == 3);
    current.next = 99; struct RhhDaycareOrdinalState unchanged = current;
    CHECK(sOrdinalBreeding->enable_ordinal(RhhModsRunId() + 1, &current) && !memcmp(&current, &unchanged, sizeof(current)));
    gMain.inBattle = TRUE; CHECK(sOrdinalBreeding->enable_ordinal(RhhModsRunId(), &current)); gMain.inBattle = FALSE;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(!sOrdinalBreeding->enable_ordinal(RhhModsRunId(), &current) && current.enabled && current.next == 1
        && !sOrdinalBreeding->enable_ordinal(RhhModsRunId(), &current) && current.next == 1
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    TriggerPendingDaycareEgg(); sExpectedDaycareOrdinal = 1;
    GiveEggFromDaycare(); CHECK(sDaycareOrdinalEvents == 2 && !sOrdinalBreeding->ordinal_state(&current) && current.next == 2);
    sExpectedDaycareOrdinal = 2; GiveEggFromDaycare();
    CHECK(sDaycareOrdinalEvents == 3 && !sOrdinalBreeding->ordinal_state(&current) && current.next == 3);
    for (u32 i = gPartiesCount[B_TRAINER_PLAYER]; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = parent;
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE; GiveEggFromDaycare();
    CHECK(sDaycareOrdinalEvents == 3 && !sOrdinalBreeding->ordinal_state(&current) && current.next == 3);
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); u32 generation = gSaveCounter;
    ZeroMonData(&gParties[B_TRAINER_PLAYER][PARTY_SIZE - 1]); CalculatePlayerPartyCount();
    sExpectedDaycareOrdinal = 3; GiveEggFromDaycare();
    CHECK(sDaycareOrdinalEvents == 4 && !sOrdinalBreeding->ordinal_state(&current) && current.next == 4);
    CHECK(!NativeSaveRestoreMods(generation) && !sOrdinalBreeding->ordinal_state(&current) && current.next == 3);
    ZeroMonData(&gParties[B_TRAINER_PLAYER][PARTY_SIZE - 1]); CalculatePlayerPartyCount();
    GiveEggFromDaycare(); CHECK(sDaycareOrdinalEvents == 5 && !sOrdinalBreeding->ordinal_state(&current) && current.next == 4);

    snapshot = NULL; CHECK(!RhhModsExport(&snapshot, &length));
    u32 enabledLength = 0; u8 *enabledFormat = NatureFormatRecord(snapshot, length, &enabledLength); free(snapshot);
    CHECK(ClockTestGet32(enabledFormat + 24 + 64) == 6 && ClockTestGet32(enabledFormat + 24 + 68) == TEST_RUN_SCHEMA3_SIZE + 32);
    u8 *runState = RhhModsState("rhh:host_run"); CHECK(runState);
    ClockTestPut64(runState + TEST_RUN_SCHEMA3_SIZE + 24, UINT64_MAX);
    CHECK(!sOrdinalBreeding->ordinal_state(&current) && current.enabled && current.exhausted && current.next == UINT64_MAX);
    ZeroMonData(&gParties[B_TRAINER_PLAYER][PARTY_SIZE - 1]); CalculatePlayerPartyCount();
    sExpectedDaycareOrdinal = 0; GiveEggFromDaycare();
    CHECK(sDaycareOrdinalEvents == 6 && !sOrdinalBreeding->ordinal_state(&current) && current.exhausted);
    NativeSaveClose();
    RhhModsInit(); CHECK(!RhhModsRegister(&sOldOrdinalFormatRun) && !RhhModsFinalize());
    CHECK(!RhhModsImport(oldFormat, oldLength));
    u8 *oldRun = RhhModsState("rhh:host_run"), *oldCopy = malloc(TEST_RUN_SCHEMA3_SIZE + 24); CHECK(oldRun && oldCopy);
    memcpy(oldCopy, oldRun, TEST_RUN_SCHEMA3_SIZE + 24);
    CHECK(RhhModsImport(enabledFormat, enabledLength) && !memcmp(oldRun, oldCopy, TEST_RUN_SCHEMA3_SIZE + 24));
    free(oldCopy); free(oldFormat); free(enabledFormat); RhhModsShutdown();
    puts("daycare ordinal: commit, rollback, exhaustion and older reader passed");
}

static void CheckDaycareOrdinalReload(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsFinalize() && !NativeSaveOpen(path));
    gFlashMemoryPresent = TRUE; CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/breeding", RHH_SERVICE_VERSION(1, 25));
    struct RhhDaycareOrdinalState state = {.struct_size = sizeof(state)};
    CHECK(breeding && !breeding->ordinal_state(&state) && state.enabled && state.next == 3);
    /* The original observer is absent; actual placements must still count. */
    ZeroMonData(&gParties[B_TRAINER_PLAYER][PARTY_SIZE - 1]); CalculatePlayerPartyCount();
    GiveEggFromDaycare(); CHECK(!breeding->ordinal_state(&state) && state.next == 4);
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); NativeSaveClose();
    CHECK(!NativeSaveOpen(path) && LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK
        && !breeding->ordinal_state(&state) && state.enabled && state.next == 4);
    NativeSaveClose(); RhhModsShutdown(); puts("daycare ordinal: fresh-process reload without observer passed");
}
