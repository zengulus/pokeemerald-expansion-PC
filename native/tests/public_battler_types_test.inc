static void CheckPublicBattlerSeen(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    struct BattleResources resources = {0}; struct AiLogicData ai = {0};
    gBattleResources = &resources; gAiLogicData = &ai;
    PrepareTurnBattle(); gBattlersCount = 2; gBattleTypeFlags &= ~BATTLE_TYPE_DOUBLE;
    memset(gSaveBlock1Ptr->dexSeen, 0, sizeof(gSaveBlock1Ptr->dexSeen)); ModsBattleBegin();
    const struct RhhTypeService *types = (const void *)RhhModsHostAPI()->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 14));
    struct RhhPublicBattlerTypes view = {.struct_size = sizeof(view)};
    CHECK(types && !types->public_battler_types(1, &view)
        && view.flags == RHH_PUBLIC_BATTLER_TYPES_UNKNOWN && !view.types.count);
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_CHARIZARD), FLAG_SET_SEEN);
    CHECK(!types->public_battler_types(1, &view) && !view.flags && view.types.count == 2
        && view.types.types[0] == RhhContentKeyForId(RHH_TYPE, TYPE_FIRE)
        && view.types.types[1] == RhhContentKeyForId(RHH_TYPE, TYPE_FLYING));
    struct RhhPublicBattlerTypes saved = view;
    ai.aiPredictionInProgress = TRUE;
    CHECK(types->public_battler_types(1, &view) && !memcmp(&view, &saved, sizeof(view)));
    ai.aiPredictionInProgress = FALSE;
    CHECK(types->public_battler_types(MAX_BATTLERS_COUNT, &view) && !memcmp(&view, &saved, sizeof(view)));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gAiLogicData = NULL;
    puts("public battler types: unseen and seen indicator policy passed");
}

struct PublicTypeProbeState { u32 count; struct RhhPublicBattlerTypeObservation last; };
static u32 sPublicTypeLocalCount;
static void PublicTypeLocal(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    ++sPublicTypeLocalCount; (void)Random(); (void)Random2(); RhhModsObserveBattleTypes();
}
static const struct RhhModHook sPublicTypeLocalHooks[] = {
    {sizeof(struct RhhModHook), RHH_PUBLIC_BATTLER_TYPES_OBSERVED, RHH_OBSERVE, 0, NULL, PublicTypeLocal}
};
static const struct RhhModDefinition sPublicTypeLocalMod = {
    .struct_size = sizeof(sPublicTypeLocalMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:public_type_rng", .hooks = sPublicTypeLocalHooks, .hook_count = ARRAY_COUNT(sPublicTypeLocalHooks)
};
static void CheckPublicTypeObservation(const char *module, bool32 seenPolicy)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sPublicTypeLocalMod)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    const struct RhhTypeService *types = (const void *)RhhModsHostAPI()->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 16));
    CHECK(types && types->header.struct_size >= RHH_TYPE_SERVICE_V1_16_SIZE
        && RHH_TYPE_SERVICE_V1_15_SIZE < RHH_TYPE_SERVICE_V1_16_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 17)));
    struct RhhPublicBattlerTypeObservation outside = {.struct_size = sizeof(outside), .sequence = 99}, saved = outside;
    CHECK(types->public_battler_observation_context(&outside) && !memcmp(&outside, &saved, sizeof(saved)));
    RhhModsSetRunContext(123, 456, 0);
    struct BattleResources resources = {0}; struct AiLogicData ai = {0};
    gBattleResources = &resources; gAiLogicData = &ai;
    PrepareTurnBattle(); gBattlersCount = 2; gBattleTypeFlags &= ~BATTLE_TYPE_DOUBLE;
    u8 savedSeen[sizeof(gSaveBlock1Ptr->dexSeen)]; memcpy(savedSeen, gSaveBlock1Ptr->dexSeen, sizeof(savedSeen));
    memset(gSaveBlock1Ptr->dexSeen, 0, sizeof(gSaveBlock1Ptr->dexSeen));
    sPublicTypeLocalCount = 0; ModsBattleBegin();
    struct PublicTypeProbeState *probe = RhhModsState("test:public_type_observation");
    CHECK(probe && !probe->count);
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_CHARIZARD), FLAG_SET_SEEN);
    RhhModsObserveBattleTypes();
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    if (seenPolicy)
    {
        CHECK(probe->count == 1 && sPublicTypeLocalCount == 1 && probe->last.sequence == 0
            && probe->last.before.flags == RHH_PUBLIC_BATTLER_TYPES_UNKNOWN && !probe->last.before.types.count
            && !probe->last.after.flags && probe->last.after.types.count == 2
            && probe->last.after.types.types[0] == RhhContentKeyForId(RHH_TYPE, TYPE_FIRE));
        struct Pokemon disguise;
        CreateMonWithIVs(&disguise, SPECIES_PIDGEY, 20, 9876, OTID_STRUCT_PRESET(54321), 31);
        gBattleStruct->illusion[1].state = ILLUSION_ON; gBattleStruct->illusion[1].mon = &disguise;
        RhhModsObserveBattleTypes();
        CHECK(probe->count == 2 && probe->last.after.types.types[0] == RhhContentKeyForId(RHH_TYPE, TYPE_NORMAL));
        gBattleStruct->illusion[1].state = ILLUSION_OFF; RhhModsObserveBattleTypes();
        CHECK(probe->count == 3 && probe->last.after.types.types[0] == RhhContentKeyForId(RHH_TYPE, TYPE_FIRE));
        SET_BATTLER_TYPE(1, TYPE_WATER); RhhModsObserveBattleTypes();
        CHECK(probe->count == 4 && probe->last.after.types.count == 1
            && probe->last.after.types.types[0] == RhhContentKeyForId(RHH_TYPE, TYPE_WATER));
        ai.aiPredictionInProgress = TRUE; SET_BATTLER_TYPE(1, TYPE_GRASS); RhhModsObserveBattleTypes();
        SET_BATTLER_TYPE(1, TYPE_WATER); ai.aiPredictionInProgress = FALSE; RhhModsObserveBattleTypes();
        CHECK(probe->count == 4);
        memset(gSaveBlock1Ptr->dexSeen, 0, sizeof(gSaveBlock1Ptr->dexSeen)); RhhModsObserveBattleTypes();
        CHECK(probe->count == 5 && probe->last.after.flags == RHH_PUBLIC_BATTLER_TYPES_UNKNOWN
            && !probe->last.after.types.count);
        ModsBattleSwitchPrepared(1); RhhModsObserveBattleTypes(); ModsBattleSwitchEntered(1);
        GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_CHARIZARD), FLAG_SET_SEEN);
        RhhModsObserveBattleTypes(); CHECK(probe->count == 5);
        SET_BATTLER_TYPE(1, TYPE_FIRE); RhhModsObserveBattleTypes(); CHECK(probe->count == 6);
        gBattleTypeFlags |= BATTLE_TYPE_LINK; SET_BATTLER_TYPE(1, TYPE_GRASS); RhhModsObserveBattleTypes();
        gBattleTypeFlags &= ~BATTLE_TYPE_LINK; RhhModsObserveBattleTypes(); CHECK(probe->count == 6);
        SET_BATTLER_TYPE(1, TYPE_WATER); RhhModsObserveBattleTypes();
        CHECK(probe->count == 7 && sPublicTypeLocalCount == 7 && probe->last.sequence == 6);
    }
    else
    {
        SET_BATTLER_TYPE(1, TYPE_WATER); RhhModsObserveBattleTypes();
        CHECK(!probe->count && !sPublicTypeLocalCount);
    }
    CHECK(types->public_battler_observation_context(&outside) && !memcmp(&outside, &saved, sizeof(saved)));
    memcpy(gSaveBlock1Ptr->dexSeen, savedSeen, sizeof(savedSeen));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gAiLogicData = NULL;
    RhhModsUnloadLibraries();
    puts("public type observation: visibility, illusion, switches and RNG passed");
}
