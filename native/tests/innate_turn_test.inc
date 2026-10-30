static void CheckInnateTurn(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources;
    PrepareTurnBattle();
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhAbilityService *abilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 12));
    const struct RhhBattleService *battle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 9));
    CHECK(abilities && battle && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 11)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 13)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 14)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 15)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 16)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 17)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 18)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 19)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 20)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 21)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 22)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 25)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 26)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 27)) == &abilities->header
        && !host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 41)));
    struct RhhInnateTurnContext outside = {.struct_size = sizeof(outside), .battler = 999}, saved = outside;
    CHECK(abilities->innate_turn_context(&outside) && !memcmp(&outside, &saved, sizeof(outside))
        && abilities->innate_turn_message("outside"));
    RhhContentKey ward = RhhContentKeyOf("test:condition/turn_ward");
    gBattlerByTurnOrder[0] = 3; gBattlerByTurnOrder[1] = 1; gBattlerByTurnOrder[2] = 2; gBattlerByTurnOrder[3] = 0;
    gBattleMons[2].level = 9; gBattleMons[3].hp = 0;
    ModsBattleBegin();
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    gBattleTurnCounter = 1; gBattleStruct->eventState.endTurn = ENDTURN_COUNT;
    gBattleStruct->eventState.endTurnBattler = gBattleStruct->eventState.battlerSide = 0;
    gBattlescriptCurrInstr = BattleScript_EndTurnEvents; gBattleControllerExecFlags = 0;
    CHECK(DoEndTurnEffects() && gBattlescriptCurrInstr == BattleScript_ModConditionMessages
        && ConditionTurns(battle, ward, RHH_CONDITION_PLAYER_SIDE) == 2
        && ConditionTurns(battle, ward, RHH_CONDITION_OPPONENT_SIDE) == 2);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_WAITMESSAGE && gBattleControllerExecFlags);
    BufferStringBattle(STRINGID_MOD_EFFECT_MESSAGE, GetBattlerAtPosition(B_POSITION_PLAYER_LEFT));
    CHECK(TextEqualsAscii(gDisplayedStringBattle, "Turn ward is active."));
    gBattleControllerExecFlags = 0; gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
    gBattleScriptingCommandsTable[B_SCR_OP_WAITMESSAGE](); gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModConditionMessagesEnd);
    gBattleScriptingCommandsTable[B_SCR_OP_RETURN]();
    CHECK(gBattlescriptCurrInstr == BattleScript_EndTurnEvents && !DoEndTurnEffects());
    CHECK(ModsConsumeEndTurnMessageResume() && !ModsConsumeEndTurnMessageResume());
    FinishTestTurn(1); /* No duplicate notifications. */
    gBattleMons[0].volatiles.gastroAcid = TRUE; gBattleMons[1].volatiles.gastroAcid = TRUE;
    FinishTestTurn(2);
    CHECK(ConditionTurns(battle, ward, RHH_CONDITION_PLAYER_SIDE) == 1
        && ConditionTurns(battle, ward, RHH_CONDITION_OPPONENT_SIDE) == 1);
    gBattleOutcome = B_OUTCOME_WON; FinishTestTurn(3); gBattleOutcome = 0;
    gBattleTypeFlags |= BATTLE_TYPE_LINK; FinishTestTurn(4); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    gBattleTypeFlags |= BATTLE_TYPE_RECORDED; FinishTestTurn(5); gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED;
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && abilities->innate_turn_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    ModsBattleReset(); gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("innate turn: ordering, suppression, conditions and messages passed");
}
