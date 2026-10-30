static void RunInnateStatusTurn(u32 turn, u32 expectedStatus, u32 staleBattler)
{
    BeginInnateActionTurn(turn);
    u16 oldSlot = 0;
    if (staleBattler < gBattlersCount) { oldSlot = gBattlerPartyIndexes[staleBattler]; gBattlerPartyIndexes[staleBattler] ^= 1; }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(!!gBattleMons[1].status1 == !!expectedStatus);
    if (expectedStatus)
    {
        CHECK((gBattleMons[1].status1 & expectedStatus) && gBattleControllerExecFlags
            && gBattleResources->bufferA[1][0] == CONTROLLER_SETMONDATA);
        BtlController_HandleSetMonData(1);
        CHECK(!gBattleControllerExecFlags && GetMonData(GetBattlerMon(1), MON_DATA_STATUS) == gBattleMons[1].status1);
        for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModInnateTurnAction; ++step)
        {
            CHECK(step < 4096);
            if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
            u32 opcode = *gBattlescriptCurrInstr; CHECK(opcode != B_SCR_OP_END);
            if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
            gBattleScriptingCommandsTable[opcode]();
            AnimateSprites();
        }
    }
    if (staleBattler < gBattlersCount) gBattlerPartyIndexes[staleBattler] = oldSlot;
    RunInnateTurnActions(0, 0, FALSE);
}
static void RunInnateStatusEntry(u32 expectedStatus, u32 staleBattler)
{
    CHECK(ModsInnateSwitchIn(0));
    u16 oldSlot = 0;
    if (staleBattler < gBattlersCount) { oldSlot = gBattlerPartyIndexes[staleBattler]; gBattlerPartyIndexes[staleBattler] ^= 1; }
    gBattlescriptCurrInstr = BattleScript_ModInnateHeal; gBattleControllerExecFlags = 0;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateStat);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateMessages);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(!!gBattleMons[1].status1 == !!expectedStatus);
    if (expectedStatus)
    {
        CHECK((gBattleMons[1].status1 & expectedStatus) && gBattleControllerExecFlags
            && gBattleResources->bufferA[1][0] == CONTROLLER_SETMONDATA);
        BtlController_HandleSetMonData(1);
        CHECK(!gBattleControllerExecFlags && GetMonData(GetBattlerMon(1), MON_DATA_STATUS) == gBattleMons[1].status1);
        for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModInnateMessages; ++step)
        {
            CHECK(step < 4096);
            if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
            u32 opcode = *gBattlescriptCurrInstr; CHECK(opcode != B_SCR_OP_END);
            if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
            gBattleScriptingCommandsTable[opcode]();
        }
    }
    if (gBattlescriptCurrInstr == BattleScript_ModInnateMessages)
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateMessagesEnd);
    if (staleBattler < gBattlersCount) gBattlerPartyIndexes[staleBattler] = oldSlot;
}
static void RunMixedInnateStatusEntry(void)
{
    CHECK(ModsInnateSwitchIn(0));
    gBattlescriptCurrInstr = BattleScript_ModInnateHeal; gBattleControllerExecFlags = 0;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_HEALTHBARUPDATE && gBattleScripting.battler == 0
        && !gBattleMons[1].status1);
    gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE](); gBattleControllerExecFlags = 0;
    gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
    CHECK(gBattleControllerExecFlags && gBattleMons[0].hp > 1 && !gBattleMons[1].status1);
    BtlController_HandleSetMonData(0); CHECK(!gBattleControllerExecFlags);
    gBattleScriptingCommandsTable[B_SCR_OP_GOTO](); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateStat);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModInnateStat; ++step)
    {
        CHECK(step < 4096 && !gBattleMons[1].status1);
        if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
        u32 opcode = *gBattlescriptCurrInstr; CHECK(opcode != B_SCR_OP_END);
        if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[opcode]();
    }
    CHECK(gBattleMons[0].statStages[STAT_ATK] == DEFAULT_STAT_STAGE + 1);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateMessages);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK((gBattleMons[1].status1 & STATUS1_BURN) && gBattleControllerExecFlags);
    BtlController_HandleSetMonData(1); CHECK(!gBattleControllerExecFlags);
    for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModInnateMessages; ++step)
    {
        CHECK(step < 4096);
        if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
        u32 opcode = *gBattlescriptCurrInstr; CHECK(opcode != B_SCR_OP_END);
        if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[opcode]();
    }
    for (u32 i = 0; i < 7; ++i)
    { gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateMessages); }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateMessagesEnd);
}
static void CheckInnateStatus(const char *module)
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
    const struct RhhAbilityService *abilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 26));
    CHECK(abilities && abilities->header.struct_size >= RHH_ABILITY_SERVICE_V1_26_SIZE
        && abilities->innate_turn_status_target(1, RHH_EFFECT_BURN) && abilities->innate_entry_status_target(1, RHH_EFFECT_BURN));
    u8 *status = RhhModsState("test:innate_status"); CHECK(status);
    gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_WATER; gBattleMons[1].types[2] = TYPE_MYSTERY;
    gBattleMons[1].ability = ABILITY_NONE; gBattlerAttacker = 2; gBattleScripting.battler = 3;
    ModsBattleBegin();
    static const u32 effects[] = {0, STATUS1_SLEEP, STATUS1_POISON, STATUS1_BURN, STATUS1_FREEZE,
                                  STATUS1_PARALYSIS, STATUS1_TOXIC_POISON};
    for (u32 kind = RHH_EFFECT_SLEEP; kind <= RHH_EFFECT_TOXIC; ++kind)
    {
        *status = kind; gBattleMons[1].status1 = 0;
        RunInnateStatusTurn(kind, effects[kind], MAX_BATTLERS_COUNT);
        CHECK(gBattlerAttacker == 2 && gBattleScripting.battler == 3);
    }
    *status = RHH_EFFECT_BURN; gBattleMons[1].status1 = 0;
    gBattleMons[1].types[0] = TYPE_FIRE; RunInnateStatusTurn(7, 0, MAX_BATTLERS_COUNT);
    gBattleMons[1].types[0] = TYPE_WATER; gBattleMons[1].ability = ABILITY_WATER_VEIL;
    gBattleStruct->moldBreakerActive = TRUE; RunInnateStatusTurn(8, 0, MAX_BATTLERS_COUNT);
    CHECK(gBattleStruct->moldBreakerActive); gBattleStruct->moldBreakerActive = FALSE;
    gBattleMons[1].ability = ABILITY_NONE; gSideStatuses[B_SIDE_OPPONENT] |= SIDE_STATUS_SAFEGUARD;
    gBattleStruct->ignoreOrbFailure = TRUE; RunInnateStatusTurn(9, 0, MAX_BATTLERS_COUNT);
    CHECK(gBattleStruct->ignoreOrbFailure); gBattleStruct->ignoreOrbFailure = FALSE;
    gSideStatuses[B_SIDE_OPPONENT] &= ~SIDE_STATUS_SAFEGUARD;
    RunInnateStatusTurn(10, 0, 0); RunInnateStatusTurn(11, 0, 1);
    CHECK(gBattlerAttacker == 2 && gBattleScripting.battler == 3);
    *status = RHH_EFFECT_TOXIC; gBattleMons[1].ability = ABILITY_SYNCHRONIZE;
    RunInnateStatusTurn(12, STATUS1_TOXIC_POISON, MAX_BATTLERS_COUNT);
    CHECK(gBattleMons[0].status1 & (GetConfig(B_SYNCHRONIZE_TOXIC) < GEN_5 ? STATUS1_POISON : STATUS1_TOXIC_POISON));
    gBattleMons[0].status1 = gBattleMons[1].status1 = 0;
    gBattleMons[0].item = ITEM_PECHA_BERRY; gBattleMons[1].item = ITEM_LUM_BERRY;
    RunInnateStatusTurn(13, STATUS1_TOXIC_POISON, MAX_BATTLERS_COUNT);
    CHECK(!gBattleMons[0].status1 && !gBattleMons[1].status1
        && gBattleMons[0].item == ITEM_NONE && gBattleMons[1].item == ITEM_NONE);
    gBattleMons[1].ability = ABILITY_NONE;
    for (u32 kind = RHH_EFFECT_SLEEP; kind <= RHH_EFFECT_TOXIC; ++kind)
    {
        ModsBattleReset(); ModsBattleBegin(); *status = kind; gBattleMons[1].status1 = 0;
        RunInnateStatusEntry(effects[kind], MAX_BATTLERS_COUNT);
        CHECK(gBattlerAttacker == 2 && gBattleScripting.battler == 3);
    }
    *status = RHH_EFFECT_BURN; gBattleMons[1].status1 = 0;
    ModsBattleReset(); ModsBattleBegin(); gBattleMons[1].types[0] = TYPE_FIRE;
    RunInnateStatusEntry(0, MAX_BATTLERS_COUNT);
    gBattleMons[1].types[0] = TYPE_WATER; gBattleMons[1].ability = ABILITY_WATER_VEIL;
    ModsBattleReset(); ModsBattleBegin(); gBattleStruct->moldBreakerActive = TRUE;
    RunInnateStatusEntry(0, MAX_BATTLERS_COUNT); CHECK(gBattleStruct->moldBreakerActive);
    gBattleStruct->moldBreakerActive = FALSE; gBattleMons[1].ability = ABILITY_NONE;
    ModsBattleReset(); ModsBattleBegin(); gSideStatuses[B_SIDE_OPPONENT] |= SIDE_STATUS_SAFEGUARD;
    gBattleStruct->ignoreOrbFailure = TRUE; RunInnateStatusEntry(0, MAX_BATTLERS_COUNT);
    CHECK(gBattleStruct->ignoreOrbFailure); gBattleStruct->ignoreOrbFailure = FALSE;
    gSideStatuses[B_SIDE_OPPONENT] &= ~SIDE_STATUS_SAFEGUARD;
    ModsBattleReset(); ModsBattleBegin(); RunInnateStatusEntry(0, 0);
    ModsBattleReset(); ModsBattleBegin(); RunInnateStatusEntry(0, 1);
    CHECK(gBattlerAttacker == 2 && gBattleScripting.battler == 3);
    ModsBattleReset();
    *status = 128 | RHH_EFFECT_BURN; gBattleMons[1].status1 = 0;
    gBattleMons[0].hp = 1; gBattleMons[0].statStages[STAT_ATK] = DEFAULT_STAT_STAGE;
    u16 hp = 1; SetMonData(GetBattlerMon(0), MON_DATA_HP, &hp);
    ModsBattleBegin(); RunMixedInnateStatusEntry();
    CHECK(gBattlerAttacker == 2 && gBattleScripting.battler == 3);
    ModsBattleReset(); gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("innate status: normal status rules, source and recipient checks passed");
}
