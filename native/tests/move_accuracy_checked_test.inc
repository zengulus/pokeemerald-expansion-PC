static u32 sAccuracyCheckedCount, sAccuracyProposed;
static struct RhhMoveAccuracyCheck sAccuracyChecks[16];
static void AccuracyCheckedRule(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->value = sAccuracyProposed; }
static void AccuracyCheckedObserve(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 46));
    struct RhhMoveAccuracyCheck check = {.struct_size = sizeof(check)};
    CHECK(moves && !moves->accuracy_checked_context(&check) && sAccuracyCheckedCount < ARRAY_COUNT(sAccuracyChecks));
    sAccuracyChecks[sAccuracyCheckedCount++] = check; Random(); Random2();
    /* This isolated fixture has no animation controller; end the canceler
     * after a successful check instead of executing unrelated later phases. */
    if (!check.missed) gBattleStruct->eventState.atkCanceler = CANCELER_END;
}
static const struct RhhModHook sAccuracyCheckedHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_ACCURACY, RHH_TRANSFORM, 0, AccuracyCheckedRule, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_ACCURACY_CHECKED, RHH_OBSERVE, 0, NULL, AccuracyCheckedObserve}
};
static const struct RhhModDefinition sAccuracyCheckedMod = {.struct_size = sizeof(sAccuracyCheckedMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:accuracy_checked_host",
    .hook_count = ARRAY_COUNT(sAccuracyCheckedHooks), .hooks = sAccuracyCheckedHooks};
static enum CancelerResult RunAccuracyCheck(u32 move)
{
    gBattlerAttacker = 0; gBattlerTarget = 1; gCurrentMove = gChosenMove = move;
    gBattleStruct->eventState.atkCanceler = CANCELER_ACCURACY_CHECK;
    gBattleStruct->eventState.atkCancelerBattler = 0;
    gBattleStruct->battlerState[0].targetsDone[0] = TRUE;
    gBattleStruct->battlerState[0].targetsDone[1] = FALSE;
    gBattleStruct->moveResultFlags[0] = gBattleStruct->moveResultFlags[1] = 0;
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = BattleScript_EffectHit;
    return DoAttackCanceler();
}
static void CheckMoveAccuracyChecked(const char *module)
{
    RhhModsInit(); sAccuracyCheckedCount = 0;
    CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sAccuracyCheckedMod)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 46));
    struct RhhMoveAccuracyCheck outside = {.struct_size = sizeof(outside), .missed = 99}, saved = outside;
    CHECK(moves && RHH_MOVE_SERVICE_V1_45_SIZE < RHH_MOVE_SERVICE_V1_46_SIZE
        && moves->accuracy_checked_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct BattleHistory history = {0}; struct AiPartyData parties = {0};
    struct BattleSpriteInfo spriteInfo[MAX_BATTLERS_COUNT] = {0}; struct BattleSpriteData spriteData = {.battlerData = spriteInfo};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources; gBattleSpritesDataPtr = &spriteData;
    gBattleHistory = &history; gAiPartyData = &parties;
    PrepareTurnBattle(); gBattlersCount = 2; gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    gBattleMons[0].ability = gBattleMons[1].ability = ABILITY_NONE; ModsBattleBegin();
    sAccuracyProposed = 0;
    rng_value_t start = gRngValue, start2 = gRng2Value;
    struct BattleCalcValues cv = {.battlerAtk = 0, .battlerDef = 1, .move = MOVE_TACKLE, .moveEffect = GetMoveEffect(MOVE_TACKLE)};
    CHECK(DoesMoveMissTarget(&cv));
    rng_value_t expected = gRngValue, expected2 = gRng2Value; gRngValue = start; gRng2Value = start2;
    CHECK(RunAccuracyCheck(MOVE_TACKLE) == CANCELER_RESULT_RUN_SCRIPT && sAccuracyCheckedCount == 1
        && sAccuracyChecks[0].missed && sAccuracyChecks[0].sequence == 0
        && (sAccuracyChecks[0].target_result & RHH_MOVE_RESULT_MISSED)
        && !memcmp(&expected, &gRngValue, sizeof(expected)) && !memcmp(&expected2, &gRng2Value, sizeof(expected2)));
    sAccuracyProposed = 100;
    RunAccuracyCheck(MOVE_TACKLE);
    CHECK(sAccuracyCheckedCount == 2 && !sAccuracyChecks[1].missed && sAccuracyChecks[1].sequence == 1);
    sAccuracyProposed = 0; gBattleMons[0].ability = ABILITY_NO_GUARD;
    RunAccuracyCheck(MOVE_TACKLE);
    CHECK(sAccuracyCheckedCount == 3 && !sAccuracyChecks[2].missed);
    gBattleMons[0].ability = ABILITY_NONE; gBattleMons[1].ability = ABILITY_STURDY;
    CHECK(RunAccuracyCheck(MOVE_FISSURE) == CANCELER_RESULT_RUN_SCRIPT && sAccuracyCheckedCount == 4
        && sAccuracyChecks[3].missed && (sAccuracyChecks[3].flags & RHH_ACCURACY_CHECK_OHKO)
        && (sAccuracyChecks[3].flags & RHH_ACCURACY_CHECK_ENGINE_STURDY_FLAG));
    RunAccuracyCheck(MOVE_GROWL); CHECK(sAccuracyCheckedCount == 4);
    gBattleTypeFlags |= BATTLE_TYPE_LINK; RunAccuracyCheck(MOVE_FISSURE);
    CHECK(sAccuracyCheckedCount == 4); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    CHECK(moves->accuracy_checked_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gBattleSpritesDataPtr = NULL;
    gBattleHistory = NULL; gAiPartyData = NULL; RhhModsUnloadLibraries();
    puts("accuracy check: actual checks, OHKO, skipped paths and RNG passed");
}
