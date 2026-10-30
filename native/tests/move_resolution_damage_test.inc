static u32 sDamageHitCount;
static struct RhhMoveDamageApplied sDamageHits[64];
static void ObserveMoveDamageHit(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 45));
    struct RhhMoveDamageApplied hit = {.struct_size = sizeof(hit)};
    CHECK(moves && !moves->damage_applied_context(&hit) && sDamageHitCount < ARRAY_COUNT(sDamageHits));
    sDamageHits[sDamageHitCount++] = hit; Random(); Random2();
}
static const struct RhhModHook sMoveDamageHitHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_DAMAGE_APPLIED, RHH_OBSERVE, 0, NULL, ObserveMoveDamageHit}
};
static const struct RhhModDefinition sMoveDamageHitMod = {.struct_size = sizeof(sMoveDamageHitMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:damage_hit_observer",
    .hook_count = ARRAY_COUNT(sMoveDamageHitHooks), .hooks = sMoveDamageHitHooks};
static void RunResolutionScriptHp(s32 amount)
{
    static const u8 command[] = {B_SCR_OP_DATAHPUPDATE, BS_TARGET, ASSURANCE_IGNORE};
    gBattlerTarget = 1; gBattleStruct->passiveHpUpdate[1] = amount; gBattleControllerExecFlags = 0;
    gBattlescriptCurrInstr = command; gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
    CHECK(gBattlescriptCurrInstr == command + sizeof(command) && gBattleControllerExecFlags);
    BtlController_HandleSetMonData(1); CHECK(!gBattleControllerExecFlags);
}
static void BeginResolutionHealth(void)
{
    gBattlerAttacker = 0; gBattlerTarget = 1; gCurrentMove = gChosenMove = MOVE_TACKLE;
    ModsMoveTargetCaptureBegin(0, MOVE_TACKLE); ModsMoveTargetCaptureEnd(0, MOVE_TACKLE, TARGET_SELECTED);
}
static void RunResolutionAttackerZero(void)
{
    static const u8 command[] = {B_SCR_OP_SETATKHPTOZERO};
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = command;
    gBattleScriptingCommandsTable[B_SCR_OP_SETATKHPTOZERO]();
    CHECK(gBattlescriptCurrInstr == command + sizeof(command) && gBattleControllerExecFlags);
    BtlController_HandleSetMonData(gBattlerAttacker); CHECK(!gBattleControllerExecFlags);
}
static void CheckMoveResolutionDamage(const char *module)
{
    RhhModsInit(); sDamageHitCount = 0;
    CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sMoveDamageHitMod) && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 30));
    CHECK(moves && moves->header.struct_size >= RHH_MOVE_SERVICE_V1_30_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 29)) == &moves->header
        && RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 36)) == &moves->header
        && RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 37)) == &moves->header
        && RHH_MOVE_SERVICE_V1_36_SIZE < RHH_MOVE_SERVICE_V1_37_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 55)));
    struct RhhMoveResolutionDamage outside = {.struct_size = sizeof(outside), .hp_lost = 999}, old = outside;
    CHECK(moves->resolution_damage(0, &outside) && !memcmp(&outside, &old, sizeof(outside)));
    struct RhhMoveDamageApplied outsideHit = {.struct_size = sizeof(outsideHit), .hp_lost = 99}, savedHit = outsideHit;
    CHECK(moves->header.struct_size >= RHH_MOVE_SERVICE_V1_45_SIZE
        && RHH_MOVE_SERVICE_V1_44_SIZE < RHH_MOVE_SERVICE_V1_45_SIZE
        && moves->damage_applied_context(&outsideHit) && !memcmp(&outsideHit, &savedHit, sizeof(outsideHit)));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct BattleHistory history = {0}; struct AiPartyData parties = {0};
    struct BattleSpriteInfo spriteInfo[MAX_BATTLERS_COUNT] = {0}; struct BattleSpriteData spriteData = {.battlerData = spriteInfo};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources; gBattleSpritesDataPtr = &spriteData;
    gBattleHistory = &history; gAiPartyData = &parties; PrepareTurnBattle(); ModsBattleBegin();
    for (u32 i = 0; i < MAX_BATTLERS_COUNT; ++i) gBattlerControllerEndFuncs[i] = HealTestControllerComplete;

    gBattleMons[1].hp = 10;
    rng_value_t hitRng = gRngValue, hitRng2 = gRng2Value;
    RunRecordedMoveDamage(MOVE_TACKLE, 6, FALSE); RunRecordedMoveDamage(MOVE_TACKLE, 6, FALSE);
    CHECK(!gBattleMons[1].hp && sDamageHitCount == 2
        && sDamageHits[0].hp_lost == 6 && sDamageHits[0].target_hp == 4 && sDamageHits[0].sequence == 0
        && sDamageHits[1].hp_lost == 4 && !sDamageHits[1].target_hp && sDamageHits[1].sequence == 1
        && !memcmp(&hitRng, &gRngValue, sizeof(hitRng)) && !memcmp(&hitRng2, &gRng2Value, sizeof(hitRng2)));
    FinishTestMove(MOVE_TACKLE, 1);

    gBattleMons[1].hp = gBattleMons[1].maxHP;
    gBattleMons[1].volatiles.substitute = TRUE; gBattleMons[1].volatiles.substituteHP = 5;
    RunRecordedMoveDamage(MOVE_TACKLE, 7, TRUE); RunRecordedMoveDamage(MOVE_TACKLE, 3, FALSE);
    CHECK(sDamageHitCount == 4 && sDamageHits[2].substitute_lost == 5 && !sDamageHits[2].hp_lost
        && !sDamageHits[2].substitute_hp && sDamageHits[3].hp_lost == 3 && !sDamageHits[3].substitute_lost);
    FinishTestMove(MOVE_TACKLE, 1);

    RunRecordedMoveDamage(MOVE_TACKLE, 4, FALSE); gBattlerPartyIndexes[1] = 1;
    FinishTestMove(MOVE_TACKLE, 1); gBattlerPartyIndexes[1] = 0;

    ModsRecordMoveDamage(0, 1, MOVE_TACKLE, 4, 0); ModsRecordMoveDamage(0, 1, MOVE_SCRATCH, 6, 0);
    FinishTestMove(MOVE_TACKLE, 1);

    ModsRecordMoveDamage(0, 1, MOVE_TACKLE, 4, 0); gBattlerPartyIndexes[0] = 1;
    FinishTestMove(MOVE_TACKLE, 1); gBattlerPartyIndexes[0] = 0;

    ModsRecordMoveDamage(0, 1, MOVE_SURF, 5, 0); ModsRecordMoveDamage(0, 3, MOVE_SURF, 4, 2);
    FinishTestMove(MOVE_SURF, 3);

    ModsBattleReset(); ModsBattleBegin(); FinishTestMove(MOVE_GROWL, 1);
    const struct RhhMoveService *healthService = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 35));
    struct RhhMoveResolutionHealth outsideHealth = {.struct_size = sizeof(outsideHealth), .battler = 99}, savedHealth = outsideHealth;
    CHECK(healthService && healthService->header.struct_size >= RHH_MOVE_SERVICE_V1_35_SIZE
        && healthService->resolution_health(1, &outsideHealth) && !memcmp(&outsideHealth, &savedHealth, sizeof(outsideHealth)));
    gBattleMons[1].maxHP = 30; gBattleMons[1].hp = 10; BeginResolutionHealth();
    u32 hitsBeforeHealing = sDamageHitCount;
    RunRecordedMoveDamage(MOVE_TACKLE, 6, FALSE);
    RunResolutionScriptHp(-50); RunResolutionScriptHp(7); RunRecordedMoveDamage(MOVE_TACKLE, (u32)-5, FALSE);
    CHECK(gBattleMons[1].hp == 28 && sDamageHitCount == hitsBeforeHealing + 1); FinishTestMove(MOVE_TACKLE, 1);
    BeginResolutionHealth(); RunResolutionScriptHp(1); gBattlerPartyIndexes[0] = 1;
    FinishTestMove(MOVE_TACKLE, 1); gBattlerPartyIndexes[0] = 0;
    BeginResolutionHealth(); RunResolutionScriptHp(1); gBattlerPartyIndexes[1] = 1;
    FinishTestMove(MOVE_TACKLE, 1); gBattlerPartyIndexes[1] = 0;
    BeginResolutionHealth(); gCurrentMove = MOVE_SCRATCH; RunResolutionScriptHp(1);
    FinishTestMove(MOVE_TACKLE, 1);
    RunResolutionScriptHp(1); FinishTestMove(MOVE_TACKLE, 1);
    /* Exercise the real target-setup boundary, not only the test helper. */
    gBattleMons[1].hp = 10; gBattlerAttacker = 0; gBattlerTarget = 1;
    gCurrentMove = gChosenMove = MOVE_TACKLE; gCurrMovePos = 0;
    gBattleMons[0].moves[0] = MOVE_TACKLE; gBattleMons[0].pp[0] = 10;
    gBattleStruct->moveTarget[0] = 1; gBattleStruct->eventState.atkCanceler = CANCELER_SET_TARGETS;
    CHECK(DoAttackCanceler() == CANCELER_RESULT_RUN_SCRIPT_AND_INCREMENT);
    RunResolutionScriptHp(-4); CHECK(gBattleMons[1].hp == 14); FinishTestMove(MOVE_TACKLE, 1);
    gBattleMons[0].hp = 23; gBattlerAttacker = 0; gBattlerTarget = 1; gCurrentMove = gChosenMove = MOVE_MEMENTO;
    ModsMoveTargetCaptureBegin(0, MOVE_MEMENTO); ModsMoveTargetCaptureEnd(0, MOVE_MEMENTO, TARGET_SELECTED);
    RunResolutionAttackerZero(); RunResolutionAttackerZero(); CHECK(!gBattleMons[0].hp);
    FinishTestMove(MOVE_MEMENTO, 1);
    gBattleMons[0].hp = 9; RunResolutionAttackerZero(); FinishTestMove(MOVE_MEMENTO, 1);
    gBattleMons[0].hp = 15; gCurrentMove = gChosenMove = MOVE_MEMENTO;
    ModsMoveTargetCaptureBegin(0, MOVE_MEMENTO); ModsMoveTargetCaptureEnd(0, MOVE_MEMENTO, TARGET_SELECTED);
    gBattlerPartyIndexes[0] = 1; RunResolutionAttackerZero(); FinishTestMove(MOVE_MEMENTO, 1);
    gBattlerPartyIndexes[0] = 0;
    struct RhhMoveHealthComparison outsideComparison = {.struct_size = sizeof(outsideComparison), .start_hp = 999}, savedComparison = outsideComparison;
    CHECK(moves->resolution_health_comparison(1, &outsideComparison) && !memcmp(&outsideComparison, &savedComparison, sizeof(outsideComparison)));
    gBattleMons[1].maxHP = 30; gBattleMons[1].hp = 20; BeginResolutionHealth();
    gBattleMons[1].hp = 14; FinishTestMove(MOVE_TACKLE, 1); /* Unobserved net loss. */
    gBattleMons[1].hp = 20; BeginResolutionHealth(); RunRecordedMoveDamage(MOVE_TACKLE, 4, FALSE);
    gBattleMons[1].hp += 3; FinishTestMove(MOVE_TACKLE, 1); /* Unobserved gain after known damage. */
    gBattleMons[1].hp = 20; BeginResolutionHealth(); gAbsentBattlerFlags |= 1u << 1;
    FinishTestMove(MOVE_TACKLE, 1); gAbsentBattlerFlags &= ~(1u << 1);
    BeginResolutionHealth(); gBattlerPartyIndexes[1] = 1; FinishTestMove(MOVE_TACKLE, 1); gBattlerPartyIndexes[1] = 0;
    FinishTestMove(MOVE_TACKLE, 1); /* No target-setup window. */
    BeginResolutionHealth(); gBattlerPartyIndexes[0] = 1; FinishTestMove(MOVE_TACKLE, 1); gBattlerPartyIndexes[0] = 0;
    gBattleMons[1].hp = 20; BeginResolutionHealth(); gBattleMons[1].hp = 15; gBattleMons[1].hp = 20;
    FinishTestMove(MOVE_TACKLE, 1); /* Unobserved offsetting changes can match. */
    gAbsentBattlerFlags |= 1u << 1; BeginResolutionHealth(); gAbsentBattlerFlags &= ~(1u << 1);
    FinishTestMove(MOVE_TACKLE, 1); /* No initial endpoint. */
    BeginResolutionHealth(); FinishTestMove(MOVE_SCRATCH, 1); /* Changed move without a recorded HP update. */
    BeginResolutionHealth(); gAbsentBattlerFlags |= 1u; FinishTestMove(MOVE_TACKLE, 1); gAbsentBattlerFlags &= ~1u;
    gAbsentBattlerFlags |= 1u; BeginResolutionHealth(); gAbsentBattlerFlags &= ~1u; FinishTestMove(MOVE_TACKLE, 1);
    CHECK(moves->resolution_health_comparison(1, &outsideComparison) && !memcmp(&outsideComparison, &savedComparison, sizeof(outsideComparison)));
    CHECK(healthService->resolution_health(1, &outsideHealth) && !memcmp(&outsideHealth, &savedHealth, sizeof(outsideHealth)));
    CHECK(moves->resolution_damage(0, &outside) && !memcmp(&outside, &old, sizeof(outside)));
    CHECK(moves->damage_applied_context(&outsideHit) && !memcmp(&outsideHit, &savedHit, sizeof(outsideHit)));
    u32 beforeExcluded = sDamageHitCount;
    gBattleTypeFlags |= BATTLE_TYPE_LINK; ModsRecordMoveDamage(0, 1, MOVE_TACKLE, 4, 0);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; CHECK(sDamageHitCount == beforeExcluded);
    ModsBattleReset(); RhhModsUnloadLibraries();
    /* The hit event must work without subscribing to move resolution. */
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sMoveDamageHitMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0); PrepareTurnBattle(); ModsBattleBegin(); sDamageHitCount = 0;
    for (u32 i = 0; i < MAX_BATTLERS_COUNT; ++i) gBattlerControllerEndFuncs[i] = HealTestControllerComplete;
    gBattleMons[1].hp = 10; RunRecordedMoveDamage(MOVE_TACKLE, 3, FALSE);
    CHECK(sDamageHitCount == 1 && sDamageHits[0].hp_lost == 3 && sDamageHits[0].target_hp == 7
        && sDamageHits[0].sequence == 0);
    ModsBattleReset(); gMain.inBattle = FALSE; gAiPartyData = NULL; gBattleHistory = NULL;
    gBattleSpritesDataPtr = NULL; gBattleResources = NULL; RhhModsUnloadLibraries();
    puts("move resolution damage: multi-hit, substitute, ambiguity and reset passed");
}
