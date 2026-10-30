struct BattleEvTestState { u32 processed, denied, earned_speed, unique; };
struct BattleBankState { u32 mode, processed, accepted, banked, lastMode, lastApplied; };
static bool32 sBattleBankDeny;
static void BattleBankConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->kind == RHH_CAN_APPLY_BATTLE_EVS)
    {
        const struct RhhBattleEvYieldService *service = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/battle_ev_yield", RHH_SERVICE_VERSION(1, 1));
        struct RhhBattleEvPolicy policy = {.struct_size = sizeof(policy)};
        CHECK(service && !service->policy_context(&policy) && !policy.accepted && decision->value == policy.mode);
    }
    if (sBattleBankDeny && event->kind == RHH_CAN_APPLY_BATTLE_EVS)
    { decision->allowed = FALSE; strcpy(decision->reason, "independent denial"); }
    decision->value = RHH_BATTLE_EV_BANK;
}
static const struct RhhModHook sBattleBankHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_EV_PROPOSAL, RHH_CONSTRAINT, 0, BattleBankConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_APPLY_BATTLE_EVS, RHH_CONSTRAINT, 0, BattleBankConstraint, NULL}
};
static const struct RhhModDefinition sBattleBankRule = {.struct_size = sizeof(sBattleBankRule),
    .abi_version = RHH_MOD_ABI_V1, .id = "test:independent_battle_rule", .hooks = sBattleBankHooks, .hook_count = 2};
static void BankWithoutObserver(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->value = RHH_BATTLE_EV_BANK; }
static const struct RhhModHook sBankWithoutObserverHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_EV_PROPOSAL, RHH_TRANSFORM, 0, BankWithoutObserver, NULL}
};
static const struct RhhModDefinition sBankWithoutObserver = {.struct_size = sizeof(sBankWithoutObserver),
    .abi_version = RHH_MOD_ABI_V1, .id = "test:bank_without_observer",
    .hooks = sBankWithoutObserverHooks, .hook_count = 1};
static void CheckBattleBank(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sBattleBankRule)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0); gMain.inBattle = TRUE; gBattleTypeFlags = 0;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1; struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][0];
    struct BattleBankState *state = RhhModsState("test:battle_bank"); CHECK(state);
    const struct RhhBattleEvYieldService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/battle_ev_yield", RHH_SERVICE_VERSION(1, 1));
    struct RhhBattleEvPolicy outside = {.struct_size = sizeof(outside), .mode = 999}, before = outside;
    CHECK(service && service->policy_context(&outside) && !memcmp(&outside, &before, sizeof(outside)));
    MonGainEVs(mon, SPECIES_ZIGZAGOON);
    CHECK(state->processed == 1 && state->accepted == 1 && !state->banked && state->lastApplied
        && state->lastMode == RHH_BATTLE_EV_APPLY && GetMonData(mon, MON_DATA_SPEED_EV) == 1);
    state->mode = 1; struct Pokemon original = *mon;
    MonGainEVs(mon, SPECIES_ZIGZAGOON);
    CHECK(state->processed == 2 && state->accepted == 2 && state->banked == 1 && !state->lastApplied
        && state->lastMode == RHH_BATTLE_EV_BANK && !memcmp(mon, &original, sizeof(*mon)));
    sBattleBankDeny = TRUE; MonGainEVs(mon, SPECIES_ZIGZAGOON); sBattleBankDeny = FALSE;
    CHECK(state->processed == 3 && state->accepted == 2 && state->banked == 1 && !memcmp(mon, &original, sizeof(*mon)));
    state->mode = 3; MonGainEVs(mon, SPECIES_ZIGZAGOON);
    CHECK(state->processed == 4 && state->accepted == 2 && state->banked == 1 && !memcmp(mon, &original, sizeof(*mon)));
    state->mode = 2; MonGainEVs(mon, SPECIES_ZIGZAGOON);
    CHECK(state->processed == 5 && state->accepted == 3 && state->banked == 1 && state->lastApplied
        && state->lastMode == RHH_BATTLE_EV_APPLY && GetMonData(mon, MON_DATA_SPEED_EV) == 2);
    u8 hp = MAX_PER_STAT_EVS, atk = MAX_PER_STAT_EVS, def = MAX_TOTAL_EVS - 2 * MAX_PER_STAT_EVS, zero = 0;
    SetMonData(mon, MON_DATA_HP_EV, &hp); SetMonData(mon, MON_DATA_ATK_EV, &atk);
    SetMonData(mon, MON_DATA_DEF_EV, &def); SetMonData(mon, MON_DATA_SPEED_EV, &zero);
    state->mode = 1; original = *mon; MonGainEVs(mon, SPECIES_ZIGZAGOON);
    CHECK(state->processed == 6 && state->accepted == 4 && state->banked == 2 && !state->lastApplied
        && !memcmp(mon, &original, sizeof(*mon)));
    CHECK(service->policy_context(&outside) && !memcmp(&outside, &before, sizeof(outside)));
    RhhModsUnloadLibraries(); RhhModsInit();
    CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sBankWithoutObserver) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    CreateMonWithIVs(mon, SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 31);
    MonGainEVs(mon, SPECIES_ZIGZAGOON); CHECK(GetMonData(mon, MON_DATA_SPEED_EV) == 1);
    gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("battle EV bank: accepted and denied yields are distinct");
}
static void CheckBattleEvYield(const char *module)
{
    InitModsForTest(); RhhModsSetRunContext(123, 456, 0);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][0];
    u16 item = ITEM_MACHO_BRACE; u8 pokerus = 0x11;
    SetMonData(mon, MON_DATA_HELD_ITEM, &item); SetMonData(mon, MON_DATA_POKERUS, &pokerus);
    struct Pokemon initial = *mon;
    gMain.inBattle = TRUE; gBattleTypeFlags = 0;
    rng_value_t rng = gRngValue;
    MonGainEVs(mon, SPECIES_ZIGZAGOON); struct Pokemon original = *mon;
    CHECK(GetMonData(mon, MON_DATA_SPEED_EV) == 4);
    RhhModsUnloadLibraries();

    InitModsForTestWithLibrary(module); RhhModsSetRunContext(123, 456, 0); *mon = initial;
    const struct RhhBattleEvYieldService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/battle_ev_yield", RHH_SERVICE_VERSION(1, 0));
    CHECK(service && service->header.struct_size >= sizeof(*service)
        && RhhModsHostAPI()->get_service("rhh:service/battle_ev_yield", RHH_SERVICE_VERSION(1, 1)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle_ev_yield", RHH_SERVICE_VERSION(1, 2)) == &service->header
        && !RhhModsHostAPI()->get_service("rhh:service/battle_ev_yield", RHH_SERVICE_VERSION(1, 3)));
    struct RhhBattleEvYield outside = {.struct_size = sizeof(outside), .applied = 99}, savedOutside = outside;
    CHECK(service->context(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside)));
    struct BattleEvTestState *state = RhhModsState("test:battle_evs"); CHECK(state);
    MonGainEVs(mon, SPECIES_ZIGZAGOON);
    CHECK(!memcmp(mon, &original, sizeof(*mon)) && state->processed == 1 && state->denied == 0
        && state->earned_speed == 4 && state->unique);
    struct Pokemon after = *mon;
    MonGainEVs(mon, SPECIES_POOCHYENA);
    CHECK(!memcmp(mon, &after, sizeof(*mon)) && state->processed == 2 && state->denied == 1);
    u8 hp = MAX_PER_STAT_EVS, atk = MAX_PER_STAT_EVS, def = MAX_TOTAL_EVS - 2 * MAX_PER_STAT_EVS;
    SetMonData(mon, MON_DATA_HP_EV, &hp); SetMonData(mon, MON_DATA_ATK_EV, &atk);
    SetMonData(mon, MON_DATA_DEF_EV, &def); u8 zero = 0; SetMonData(mon, MON_DATA_SPEED_EV, &zero);
    after = *mon; MonGainEVs(mon, SPECIES_ZIGZAGOON);
    CHECK(!memcmp(mon, &after, sizeof(*mon)) && state->processed == 3 && state->earned_speed == 8);
    gPokemonStoragePtr->boxes[0][0] = mon->box;
    MonGainEVs(mon, SPECIES_ZIGZAGOON); CHECK(state->processed == 4 && !state->unique && state->earned_speed == 12);
    memset(&gPokemonStoragePtr->boxes[0][0], 0, sizeof(gPokemonStoragePtr->boxes[0][0]));
    if (MAX_PER_STAT_EVS < 255)
    {
        *mon = initial; u8 overcap = 255; SetMonData(mon, MON_DATA_HP_EV, &overcap);
        after = *mon; gBattleTypeFlags = BATTLE_TYPE_LINK;
        MonGainEVs(mon, SPECIES_ZIGZAGOON); struct Pokemon legacy = *mon;
        gBattleTypeFlags = 0; *mon = after; MonGainEVs(mon, SPECIES_ZIGZAGOON);
        CHECK(!memcmp(mon, &legacy, sizeof(*mon)) && state->processed == 5);
    }
    u32 expectedProcessed = state->processed, expectedSpeed = state->earned_speed;
    uint8_t *save; uint32_t size; CHECK(!RhhModsExport(&save, &size));
    RhhModsResetState(); CHECK(!RhhModsImport(save, size)); free(save);
    CHECK(state->processed == expectedProcessed && state->denied == 1 && state->earned_speed == expectedSpeed);
    RhhModsSetRunContext(123, 456, 0); *mon = initial; gBattleTypeFlags = BATTLE_TYPE_LINK;
    MonGainEVs(mon, SPECIES_ZIGZAGOON);
    CHECK(!memcmp(mon, &original, sizeof(*mon)) && state->processed == expectedProcessed && !memcmp(&rng, &gRngValue, sizeof(rng)));
    gMain.inBattle = FALSE; gBattleTypeFlags = 0; RhhModsUnloadLibraries();
    puts("battle EV yield: caps, held items, denial, identity and state passed");
}
static void CheckBattleEvSave(const char *path, const char *module, bool32 load)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    if (!load)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_PETALBURG_CITY);
        gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_PETALBURG_CITY);
        gMapHeader = *gMapGroups[MAP_GROUP(MAP_PETALBURG_CITY)][MAP_NUM(MAP_PETALBURG_CITY)];
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 31);
        gPartiesCount[B_TRAINER_PLAYER] = 1;
        gMain.inBattle = TRUE; gBattleTypeFlags = 0;
        MonGainEVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_ZIGZAGOON);
        MonGainEVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_POOCHYENA);
        gMain.inBattle = FALSE;
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        puts("battle EV yield: saved allocation and policy state");
    }
    else
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && gPartiesCount[B_TRAINER_PLAYER] == 1);
        const struct BattleEvTestState *state = RhhModsState("test:battle_evs");
        CHECK(state && state->processed == 2 && state->denied == 1 && state->earned_speed == 1
            && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPEED_EV) == 1
            && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ATK_EV) == 0);
        puts("battle EV yield: fresh-process allocation and policy state restored");
    }
    NativeSaveClose(); RhhModsUnloadLibraries();
}
