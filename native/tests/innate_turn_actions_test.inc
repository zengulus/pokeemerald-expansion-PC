static void RunInnateTurnActions(u32 expectedHeals, u32 expectedStats, bool32 message)
{
    u32 heals = 0, stats = 0, steps = 0;
    while (gBattlescriptCurrInstr == BattleScript_ModInnateTurnAction)
    {
        CHECK(++steps < 100);
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        if (*gBattlescriptCurrInstr == B_SCR_OP_HEALTHBARUPDATE)
        {
            u32 battler = gBattleScripting.battler;
            CHECK(battler < gBattlersCount && gBattleStruct->passiveHpUpdate[battler] < 0);
            u32 expected = min(gBattleMons[battler].maxHP, gBattleMons[battler].hp - gBattleStruct->passiveHpUpdate[battler]);
            gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE]();
            CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[battler][0] == CONTROLLER_HEALTHBARUPDATE);
            gBattleControllerExecFlags = 0;
            gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
            CHECK(gBattleControllerExecFlags && gBattleMons[battler].hp == expected
                && gBattleResources->bufferA[battler][0] == CONTROLLER_SETMONDATA);
            BtlController_HandleSetMonData(battler);
            CHECK(!gBattleControllerExecFlags && GetMonData(GetBattlerMon(battler), MON_DATA_HP) == expected);
            ++heals; gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
        }
        else if (gBattlescriptCurrInstr == BattleScript_ModInnateTurnStat || gBattlescriptCurrInstr == BattleScript_ModInnateTurnSourcedStat)
        {
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_TRYBATTLERSTATCHANGE || *gBattlescriptCurrInstr == B_SCR_OP_TRYSTATCHANGES);
            for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModInnateTurnAction; ++step)
            {
                CHECK(step < 4096);
                if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
                u32 opcode = *gBattlescriptCurrInstr; CHECK(opcode != B_SCR_OP_END);
                if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
                gBattleScriptingCommandsTable[opcode]();
            }
            ++stats;
        }
        else CHECK(gBattlescriptCurrInstr == BattleScript_ModConditionMessages);
    }
    CHECK(heals == expectedHeals && stats == expectedStats && gBattlescriptCurrInstr == BattleScript_ModConditionMessages);
    if (message)
    {
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_WAITMESSAGE && gBattleControllerExecFlags);
        BufferStringBattle(STRINGID_MOD_EFFECT_MESSAGE, GetBattlerAtPosition(B_POSITION_PLAYER_LEFT));
        CHECK(TextEqualsAscii(gDisplayedStringBattle, "Recovery complete."));
        gBattleControllerExecFlags = 0; gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[B_SCR_OP_WAITMESSAGE](); gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModConditionMessagesEnd);
    gBattleScriptingCommandsTable[B_SCR_OP_RETURN]();
    CHECK(gBattlescriptCurrInstr == BattleScript_EndTurnEvents && !DoEndTurnEffects());
    CHECK(ModsConsumeEndTurnMessageResume() && !ModsConsumeEndTurnMessageResume());
}
static void BeginInnateActionTurn(u32 turn)
{
    gBattleTurnCounter = turn; gBattleStruct->eventState.endTurn = ENDTURN_COUNT;
    gBattleStruct->eventState.endTurnBattler = gBattleStruct->eventState.battlerSide = 0;
    gBattlescriptCurrInstr = BattleScript_EndTurnEvents; gBattleControllerExecFlags = 0;
    CHECK(DoEndTurnEffects() && gBattlescriptCurrInstr == BattleScript_ModInnateTurnAction);
}
static void CheckInnateTurnActions(const char *module)
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
    PrepareTurnBattle();
    for (u32 i = 0; i < MAX_BATTLERS_COUNT; ++i)
    {
        gBattlerControllerEndFuncs[i] = EntryHealControllerComplete;
        for (u32 j = RHH_STAT_ATTACK; j < RHH_BATTLE_STAT_COUNT; ++j) gBattleMons[i].statStages[j] = DEFAULT_STAT_STAGE;
    }
    const struct RhhAbilityService *abilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 16));
    CHECK(abilities && abilities->header.struct_size >= RHH_ABILITY_SERVICE_V1_16_SIZE
        && abilities->innate_turn_heal(1, 8) && abilities->innate_turn_stat(RHH_STAT_ATTACK, 1)
        && abilities->innate_turn_heal_target(1, 1, 8) && abilities->innate_turn_stat_target(1, RHH_STAT_ATTACK, 1));
    for (u32 i = 0; i < 2; ++i) { u16 hp = 1; gBattleMons[i].hp = hp; SetMonData(GetBattlerMon(i), MON_DATA_HP, &hp); }
    gBattlerByTurnOrder[0] = 3; gBattlerByTurnOrder[1] = 1; gBattlerByTurnOrder[2] = 2; gBattlerByTurnOrder[3] = 0;
    gBattleMons[2].level = 9; gBattleMons[3].hp = 0;
    ModsBattleBegin(); u32 scripting = gBattleScripting.battler;
    BeginInnateActionTurn(1); RunInnateTurnActions(2, 2, TRUE);
    CHECK(gBattleScripting.battler == scripting);
    u16 hpAfter = gBattleMons[0].hp;
    gBattleMons[1].volatiles.gastroAcid = TRUE;
    BeginInnateActionTurn(2); gBattlerPartyIndexes[0] = 1;
    RunInnateTurnActions(0, 0, FALSE); gBattlerPartyIndexes[0] = 0;
    CHECK(gBattleMons[0].hp == hpAfter);
    BeginInnateActionTurn(3);
    gBattleMons[0].volatiles.healBlockTimer = 1; gBattleStruct->statChangeBattler = 1;
    RunInnateTurnActions(0, 0, FALSE);
    CHECK(gBattleMons[0].hp == hpAfter && gBattleStruct->statChangeBattler == 1);
    gBattleMons[0].volatiles.healBlockTimer = 0; gBattleStruct->statChangeBattler = 0;
    gBattleMons[0].hp = gBattleMons[0].maxHP;
    BeginInnateActionTurn(4); RunInnateTurnActions(0, 0, FALSE);
    gBattleMons[0].hp = 1;
    BeginInnateActionTurn(5); gBattleOutcome = B_OUTCOME_WON;
    RunInnateTurnActions(0, 0, FALSE); gBattleOutcome = 0;
    CHECK(gBattleMons[0].hp == 1);
    u16 hp = 1; gBattleMons[1].hp = hp; SetMonData(GetBattlerMon(1), MON_DATA_HP, &hp);
    BeginInnateActionTurn(6); RunInnateTurnActions(1, 1, FALSE);
    CHECK(gBattleMons[1].hp > 1 && gBattleMons[0].hp == 1 && gBattleMons[1].statStages[RHH_STAT_DEFENSE] == DEFAULT_STAT_STAGE + 1);
    hp = 1; gBattleMons[1].hp = hp; SetMonData(GetBattlerMon(1), MON_DATA_HP, &hp);
    BeginInnateActionTurn(7); gBattlerPartyIndexes[1] = 1;
    RunInnateTurnActions(0, 0, FALSE); gBattlerPartyIndexes[1] = 0;
    CHECK(gBattleMons[1].hp == 1);
    BeginInnateActionTurn(8); gBattlerPartyIndexes[0] = 1;
    RunInnateTurnActions(0, 0, FALSE); gBattlerPartyIndexes[0] = 0;
    CHECK(gBattleMons[1].hp == 1);
    BeginInnateActionTurn(9); gBattleMons[1].hp = 0;
    RunInnateTurnActions(0, 0, FALSE); CHECK(!gBattleMons[1].hp);
    CHECK(gBattleScripting.battler == scripting);
    ModsBattleReset(); gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("innate turn actions: healing, stats, staleness and capacity passed");
}
