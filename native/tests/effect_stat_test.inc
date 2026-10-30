static u32 sStatAnimations, sStatMessages;
static void DriveStatChangeScript(void)
{
    for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModEffectNextAction; ++step)
    {
        CHECK(step < 4096);
        if (gBattleControllerExecFlags)
        {
            for (u32 b = 0; b < gBattlersCount; ++b)
                if (IsBattlerMarkedForControllerExec(b))
                {
                    if (gBattleResources->bufferA[b][0] == CONTROLLER_BATTLEANIMATION) ++sStatAnimations;
                    if (gBattleResources->bufferA[b][0] == CONTROLLER_PRINTSTRING) ++sStatMessages;
                }
            gBattleControllerExecFlags = 0;
        }
        u32 opcode = *gBattlescriptCurrInstr;
        CHECK(opcode != B_SCR_OP_END);
        if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[opcode]();
    }
    gBattleControllerExecFlags = 0;
}
static u32 RunStatEffectScript(const u8 *script, u32 target, char *trace)
{
    gBattlerTarget = target; gBattleControllerExecFlags = 0;
    gBattlescriptCurrInstr = script + 3;
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_CALLNATIVE);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    u32 events = 0, steps = 0;
    for (;;)
    {
        CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction && ++steps <= 9);
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        if (gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage) break;
        if (gBattlescriptCurrInstr == BattleScript_ModEffectNextAction) continue;
        CHECK(events < 8);
        if (gBattlescriptCurrInstr == BattleScript_MoveEffectStatChange)
        {
            trace[events++] = 'S';
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_TRYSTATCHANGES);
            DriveStatChangeScript();
        }
        else if (gBattlescriptCurrInstr == BattleScript_MoveEffectConfusion)
        {
            trace[events++] = 'C'; CHECK(gBattleMons[target].volatiles.confusionTimer);
            CheckStatusPresentation(target); BattleScriptPop();
        }
        else
        {
            u32 recipient = gBattlescriptCurrInstr == BattleScript_ModEffectHealUser ? gBattlerAttacker : target;
            trace[events++] = recipient == target ? 'H' : 'U';
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_HEALTHBARUPDATE && gBattleStruct->passiveHpUpdate[recipient] < 0);
            u32 expected = min(gBattleMons[recipient].maxHP, gBattleMons[recipient].hp - gBattleStruct->passiveHpUpdate[recipient]);
            gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE](); CHECK(gBattleControllerExecFlags);
            CHECK(gBattleResources->bufferA[recipient][0] == CONTROLLER_HEALTHBARUPDATE);
            gBattleControllerExecFlags = 0;
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_DATAHPUPDATE);
            gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE](); CHECK(gBattleControllerExecFlags);
            BtlController_HandleSetMonData(recipient);
            CHECK(!gBattleControllerExecFlags && gBattleMons[recipient].hp == expected
                && GetMonData(GetBattlerMon(recipient), MON_DATA_HP) == expected);
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
            gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
        }
    }
    trace[events] = 0; return events;
}
static void CheckSpecificStat(const char *probe)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(probe) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct BattleSpriteInfo spriteInfo[MAX_BATTLERS_COUNT] = {0};
    struct BattleSpriteData spriteData = {.battlerData = spriteInfo};
    struct AiLogicData ai = {0}; struct AiPartyData parties = {0};
    struct AiThinkingStruct thinking = {0}; struct AiBattleData decisions = {0}; struct BattleHistory history = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources; gBattleSpritesDataPtr = &spriteData;
    gAiLogicData = &ai; gAiPartyData = &parties; gAiThinkingStruct = &thinking; gAiBattleData = &decisions; gBattleHistory = &history;
    PrepareTurnBattle(); ModsBattleBegin();
    for (u32 i = 0; i < MAX_BATTLERS_COUNT; ++i) gBattlerControllerEndFuncs[i] = HealTestControllerComplete;
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 31));
    u8 *settings = RhhModsState("test:stat_battler"); CHECK(settings); settings[0] = 2; settings[1] = 7;
    u32 unchanged = 999;
    CHECK(moves && moves->header.struct_size >= RHH_MOVE_SERVICE_V1_31_SIZE
        && moves->effect_stat_battler(2, RHH_STAT_ATTACK, 1)
        && moves->ai_effect_offer_stat_battler(2, RHH_STAT_ATTACK, 1, 100, 0) == RHH_AI_EFFECT_UNAVAILABLE
        && moves->ai_effect_stat_battler_at(0, &unchanged) && unchanged == 999);
    gBattlerAttacker = 0; gCurrentMove = gChosenMove = MOVE_SPLASH;
    gBattleMons[0].moves[0] = MOVE_SPLASH; gBattleMons[0].pp[0] = 20;
    for (u32 i = 0; i < 4; ++i) { gBattleMons[i].ability = ABILITY_NONE; ai.abilities[i] = ABILITY_NONE; }
    const u8 *script = GetMoveBattleScript(MOVE_SPLASH); CHECK(script == BattleScript_EffectModCustomStatus);
    char trace[9]; ModsMoveEffectReset();
    CHECK(RunStatEffectScript(script, 1, trace) == 1 && !strcmp(trace, "S")
        && gBattleMons[2].statStages[STAT_ATK] == DEFAULT_STAT_STAGE + 1
        && gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE && gBattlerTarget == 1);
    ModsMoveEffectReset(); gCurrentMove = gChosenMove = MOVE_RAIN_DANCE;
    gBattleMons[2].ability = ABILITY_CONTRARY; gBattleMons[2].statStages[STAT_ATK] = DEFAULT_STAT_STAGE;
    CHECK(GetMoveBattleScript(MOVE_RAIN_DANCE) == script && RunStatEffectScript(script, 1, trace) == 1
        && gBattleMons[2].statStages[STAT_ATK] == DEFAULT_STAT_STAGE - 1 && gBattlerTarget == 1);
    ModsMoveEffectReset(); settings[0] = 3; settings[1] = 4;
    gBattleMons[3].ability = ABILITY_CLEAR_BODY; gBattleMons[3].statStages[STAT_ATK] = DEFAULT_STAT_STAGE;
    CHECK(RunStatEffectScript(script, 1, trace) == 1 && gBattleMons[3].statStages[STAT_ATK] == DEFAULT_STAT_STAGE);
    ModsMoveEffectReset(); settings[0] = 2; settings[1] = 7; gBattleMons[2].ability = ABILITY_NONE;
    gCurrentMove = gChosenMove = MOVE_SPLASH; BeginSpecificHeal(script); gBattleMons[2].hp = 0;
    u32 value = 999; s32 stages = 999;
    CHECK(ModsMoveEffectNextAction(&value, &stages) == MOD_EFFECT_DONE && gBattlerTarget == 1);
    gBattleMons[2].hp = gBattleMons[2].maxHP; gBattleMons[2].statStages[STAT_ATK] = DEFAULT_STAT_STAGE;
    BeginSpecificHeal(script); gBattlerPartyIndexes[2] = 0;
    CHECK(ModsMoveEffectNextAction(&value, &stages) == MOD_EFFECT_DONE && gBattlerTarget == 1);
    gBattlerPartyIndexes[2] = 1;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SPLASH, 0, 100) == 105);
    ai.abilities[2] = ABILITY_CONTRARY; CHECK(ModsAiMoveScore(0, 1, MOVE_SPLASH, 0, 100) == 95);
    ai.abilities[2] = ABILITY_NONE; settings[0] = 3;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SPLASH, 0, 100) == 95);
    settings[1] = 4; ai.abilities[3] = ABILITY_CLEAR_BODY;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SPLASH, 0, 100) == 100);
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleSpritesDataPtr = NULL;
    gAiLogicData = NULL; gAiPartyData = NULL; gAiThinkingStruct = NULL; gAiBattleData = NULL; gBattleHistory = NULL;
    RhhModsUnloadLibraries(); puts("effect stats: specified battler, engine rules and AI passed");
}
