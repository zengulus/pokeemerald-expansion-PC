struct BattleHealthProbeState { u32 count; struct RhhBattlerHealthObservation last[4]; };
static u32 sHealthLocalCount;
static void HealthLocalObserver(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    ++sHealthLocalCount; (void)Random(); (void)Random2(); RhhModsObserveBattleHealth();
}
static const struct RhhModHook sHealthLocalHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLER_HEALTH_OBSERVED, RHH_OBSERVE, 0, NULL, HealthLocalObserver}
};
static const struct RhhModDefinition sHealthLocalMod = {
    .struct_size = sizeof(sHealthLocalMod), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:battle_health_rng",
    .hooks = sHealthLocalHooks, .hook_count = ARRAY_COUNT(sHealthLocalHooks)
};
static void CheckBattleHealthObservation(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sHealthLocalMod)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhBattleService *battle = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 44));
    struct RhhBattlerHealthObservation outside = {.struct_size = sizeof(outside), .sequence = 99}, saved = outside;
    CHECK(battle && battle->header.struct_size >= RHH_BATTLE_SERVICE_V1_44_SIZE
        && RHH_BATTLE_SERVICE_V1_43_SIZE < RHH_BATTLE_SERVICE_V1_44_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 46))
        && battle->health_observation_context(&outside) && !memcmp(&outside, &saved, sizeof(saved)));
    struct BattleResources resources = {0}; struct AiLogicData ai = {0};
    gBattleResources = &resources; gAiLogicData = &ai; PrepareTurnBattle();
    sHealthLocalCount = 0; ModsBattleBegin();
    struct BattleHealthProbeState *probe = RhhModsState("test:battle_health_probe"); CHECK(probe && !probe->count);
    u32 hp = gBattleMons[1].hp, max = gBattleMons[1].maxHP, hp2 = gBattleMons[2].hp;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    gBattleMons[1].hp -= 10; gBattleMons[1].maxHP += 20; gBattleMons[2].hp -= 1;
    RhhModsObserveBattleHealth();
    CHECK(probe->count == 2 && sHealthLocalCount == 2 && probe->last[1].sequence == 0
        && probe->last[1].changed == (RHH_BATTLER_HP_CHANGED | RHH_BATTLER_MAX_HP_CHANGED)
        && probe->last[1].before.hp == hp && probe->last[1].after.hp == hp - 10
        && probe->last[1].before.max_hp == max && probe->last[1].after.max_hp == max + 20
        && probe->last[2].sequence == 1 && probe->last[2].before.hp == hp2 && probe->last[2].after.hp == hp2 - 1
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    RhhModsObserveBattleHealth(); CHECK(probe->count == 2);
    ai.aiPredictionInProgress = TRUE; gBattleMons[1].hp = 1; RhhModsObserveBattleHealth();
    gBattleMons[1].hp = hp - 10; ai.aiPredictionInProgress = FALSE; RhhModsObserveBattleHealth(); CHECK(probe->count == 2);
    gBattleMons[1].hp = 0; RhhModsObserveBattleHealth();
    CHECK(probe->count == 3 && probe->last[1].after.fainted && probe->last[1].sequence == 2);
    ModsBattleSwitchPrepared(1); gBattleMons[1].hp = hp; gBattleMons[1].maxHP = max;
    RhhModsObserveBattleHealth(); ModsBattleSwitchEntered(1); RhhModsObserveBattleHealth(); CHECK(probe->count == 3);
    gBattleMons[1].hp -= 1; RhhModsObserveBattleHealth(); CHECK(probe->count == 4 && probe->last[1].sequence == 3);
    gBattleTypeFlags |= BATTLE_TYPE_LINK; gBattleMons[1].hp -= 1; RhhModsObserveBattleHealth();
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; RhhModsObserveBattleHealth(); CHECK(probe->count == 4);
    gBattleMons[1].hp -= 1; RhhModsObserveBattleHealth(); CHECK(probe->count == 5 && sHealthLocalCount == 5);
    gBattleMons[1].hp += 2; RhhModsObserveBattleHealth();
    CHECK(probe->count == 6 && sHealthLocalCount == 6 && probe->last[1].sequence == 5
        && probe->last[1].changed == RHH_BATTLER_HP_CHANGED
        && probe->last[1].after.hp == probe->last[1].before.hp + 2);
    CHECK(battle->health_observation_context(&outside) && !memcmp(&outside, &saved, sizeof(saved)));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gAiLogicData = NULL; RhhModsUnloadLibraries();
    puts("battle health observation: HP, max HP, switches, simulation and RNG passed");
}
