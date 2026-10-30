static u32 sPPDeductionCount;
static bool32 sPPCheckPartyPending;
static struct RhhMovePPDeduction sLastPPDeduction;
static void ObservePPDeduction(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 44));
    sLastPPDeduction = (struct RhhMovePPDeduction){.struct_size = sizeof(sLastPPDeduction)};
    CHECK(moves && !moves->pp_deduction_context(&sLastPPDeduction));
    if (sPPCheckPartyPending) CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_PP1) == 10);
    ++sPPDeductionCount; Random(); Random2();
}
static const struct RhhModHook sPPDeductionHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_PP_DEDUCTED, RHH_OBSERVE, 0, NULL, ObservePPDeduction}
};
static const struct RhhModDefinition sPPDeductionMod = {.struct_size = sizeof(sPPDeductionMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:pp_deduction_observer",
    .hook_count = ARRAY_COUNT(sPPDeductionHooks), .hooks = sPPDeductionHooks};
static void RunPPCostDeduction(u8 mode, u32 move, u32 slot, u32 pp, u32 expected)
{
    u8 *state = RhhModsState("test:pp_cost"); CHECK(state); *state = mode;
    gCurrentMove = gChosenMove = move; gCurrMovePos = slot; gChosenMovePos = slot;
    gBattleMons[0].moves[slot] = move; gBattleMons[0].pp[slot] = pp;
    gBattleStruct->submoveAnnouncement = SUBMOVE_NO_EFFECT;
    gBattleStruct->eventState.atkCanceler = CANCELER_PPDEDUCTION;
    gBattleControllerExecFlags = 0;
    CHECK(DoAttackCanceler() == CANCELER_RESULT_RUN_SCRIPT_AND_INCREMENT && gBattleMons[0].pp[slot] == expected);
    CHECK(gBattleResources->bufferA[0][0] == CONTROLLER_SETMONDATA
        && gBattleResources->bufferA[0][1] == REQUEST_PPMOVE1_BATTLE + slot
        && gBattleResources->bufferA[0][3] == expected);
}
static void CheckPPCostLegacy(const char *legacy, const char *aiProbe)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(legacy)
        && !RhhModsLoadLibrary(aiProbe) && !RhhModsFinalize()); RhhModsSetRunContext(123, 456, 0);
    struct BattleResources resources = {0}; struct AiLogicData ai = {0};
    gBattleResources = &resources; gAiLogicData = &ai; PrepareTurnBattle();
    gBattlersCount = 2; gBattleTypeFlags = BATTLE_TYPE_TRAINER; gBattlerAttacker = 0; gBattlerTarget = 1;
    gBattleMons[1].ability = ai.abilities[1] = ABILITY_PRESSURE; ModsBattleBegin();
    gBattleMons[0].moves[0] = MOVE_TACKLE; gBattleMons[0].pp[0] = 10;
    CHECK(ModsAiMoveScore(0, 1, MOVE_TACKLE, 0, 100) == 162); /* baseline + uncertainty flag */
    gCurrentMove = gChosenMove = MOVE_TACKLE; gCurrMovePos = 0;
    gBattleStruct->eventState.atkCanceler = CANCELER_PPDEDUCTION;
    CHECK(DoAttackCanceler() == CANCELER_RESULT_RUN_SCRIPT_AND_INCREMENT && gBattleMons[0].pp[0] == 7);
    ModsBattleReset(); gMain.inBattle = FALSE; gAiLogicData = NULL; RhhModsUnloadLibraries();
    puts("PP cost: v1.23 committed-only rule preserved");
}
