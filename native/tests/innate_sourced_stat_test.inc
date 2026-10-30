static void RunInnateSourcedEntry(void)
{
    CHECK(ModsInnateSwitchIn(0));
    gBattlescriptCurrInstr = BattleScript_ModInnateStat; gBattleControllerExecFlags = 0;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateSourcedStat && *gBattlescriptCurrInstr == B_SCR_OP_TRYSTATCHANGES);
    for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModInnateStat; ++step)
    {
        CHECK(step < 4096);
        if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
        u32 opcode = *gBattlescriptCurrInstr; CHECK(opcode != B_SCR_OP_END);
        if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[opcode]();
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateMessages);
}
static void CheckInnateSourcedStat(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct BattleSpriteInfo spriteInfo[MAX_BATTLERS_COUNT] = {0};
    struct BattleSpriteData spriteData = {.battlerData = spriteInfo};
    struct AiLogicData ai = {0}; struct AiPartyData parties = {0}; struct AiThinkingStruct thinking = {0};
    struct BattleHistory history = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources; gBattleSpritesDataPtr = &spriteData;
    gAiLogicData = &ai; gAiPartyData = &parties; gAiThinkingStruct = &thinking; gBattleHistory = &history;
    PrepareTurnBattle(); gBattleMons[2].level = 9; gBattleMons[3].hp = 0;
    for (u32 i = 0; i < MAX_BATTLERS_COUNT; ++i) gBattlerControllerEndFuncs[i] = EntryHealControllerComplete;
    const struct RhhAbilityService *abilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 18));
    CHECK(abilities && abilities->header.struct_size >= RHH_ABILITY_SERVICE_V1_18_SIZE
        && abilities->innate_entry_stat_from(1, RHH_STAT_ATTACK, -1)
        && abilities->innate_turn_stat_from(1, RHH_STAT_ATTACK, -1));
    gBattleMons[0].statStages[STAT_ATK] = gBattleMons[1].statStages[STAT_ATK] = DEFAULT_STAT_STAGE;
    gBattlerAttacker = 2; gBattleScripting.battler = 3;
    gBattleMons[1].ability = ABILITY_CLEAR_BODY; gBattleStruct->moldBreakerActive = TRUE;
    ModsBattleBegin(); RunInnateSourcedEntry();
    CHECK(gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE && gBattlerAttacker == 2
        && gBattleScripting.battler == 3 && gBattleStruct->moldBreakerActive);
    gBattleStruct->moldBreakerActive = FALSE;
    ModsBattleReset(); gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].volatiles.substitute = TRUE;
    ModsBattleBegin(); RunInnateSourcedEntry();
    CHECK(gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE - 1 && gBattlerAttacker == 2 && gBattleScripting.battler == 3);
    ModsBattleReset(); gBattleMons[1].volatiles.substitute = FALSE; gBattleMons[1].statStages[STAT_ATK] = DEFAULT_STAT_STAGE;
    gSideTimers[B_SIDE_OPPONENT].mistTimer = 1; ModsBattleBegin(); RunInnateSourcedEntry();
    CHECK(gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE);
    ModsBattleReset(); gSideTimers[B_SIDE_OPPONENT].mistTimer = 0; gBattleMons[1].ability = ABILITY_MIRROR_ARMOR;
    ModsBattleBegin(); RunInnateSourcedEntry();
    CHECK(gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE && gBattleMons[0].statStages[STAT_ATK] == DEFAULT_STAT_STAGE - 1);
    CHECK(gBattlerAttacker == 2 && gBattleScripting.battler == 3);
    ModsBattleReset(); gBattleMons[0].statStages[STAT_ATK] = DEFAULT_STAT_STAGE; gBattleMons[1].ability = ABILITY_DEFIANT;
    ModsBattleBegin(); RunInnateSourcedEntry();
    CHECK(gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE + 1 && gBattleScripting.battler == 3);
    ModsBattleReset(); gBattleMons[1].statStages[STAT_ATK] = DEFAULT_STAT_STAGE; gBattleMons[1].ability = ABILITY_CLEAR_BODY;
    gBattleStruct->moldBreakerActive = TRUE;
    ModsBattleBegin(); BeginInnateActionTurn(1); RunInnateTurnActions(0, 1, FALSE);
    CHECK(gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE && gBattleStruct->moldBreakerActive);
    gBattleStruct->moldBreakerActive = FALSE;
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].volatiles.substitute = TRUE;
    BeginInnateActionTurn(2); RunInnateTurnActions(0, 1, FALSE);
    CHECK(gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE - 1 && gBattlerAttacker == 2 && gBattleScripting.battler == 3);
    ModsBattleReset(); gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("innate sourced stat: protection, reflection and substitute passed");
}
