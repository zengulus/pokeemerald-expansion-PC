static void BeginSpecificDamage(const u8 *script)
{
    ModsMoveEffectReset(); gBattlerAttacker = 0; gBattlerTarget = 1;
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = script + 3;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction);
}

static void BeginDamageFromHit(u32 hp, u32 substitute, bool32 recorded)
{
    ModsMoveEffectReset(); gBattlerAttacker = 0; gBattlerTarget = 1;
    gCurrentMove = gChosenMove = MOVE_SCRATCH;
    if (recorded) ModsRecordMoveDamage(0, 1, MOVE_SCRATCH, hp, substitute);
    gBattleStruct->additionalEffectsCounter = 0;
    gBattlescriptCurrInstr = GetMoveBattleScript(MOVE_SCRATCH) + 9;
    gBattleScriptingCommandsTable[B_SCR_OP_SETADDITIONALEFFECTS]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_CALLNATIVE);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction);
}

static void FinishSpecificDamage(u32 battler, u32 expectedHp, bool32 fainted, bool32 berry)
{
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectDamageBattler
        && gBattleStruct->passiveHpUpdate[battler] > 0);
    gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[battler][0] == CONTROLLER_HEALTHBARUPDATE);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
    CHECK(gBattleControllerExecFlags && gBattleMons[battler].hp == expectedHp);
    BtlController_HandleSetMonData(battler);
    CHECK(!gBattleControllerExecFlags && GetMonData(GetBattlerMon(battler), MON_DATA_HP) == expectedHp);
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_TRYFAINTMON);
    gBattleScriptingCommandsTable[B_SCR_OP_TRYFAINTMON]();
    if (fainted)
    {
        CHECK(gBattlescriptCurrInstr == BattleScript_FaintBattler && (gHitMarker & HITMARKER_FAINTED(battler)));
        BattleScriptPop(); /* The engine's faint presentation resumes this action. */
    }
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_CALLNATIVE);
    u32 savedScripting = gBattleScripting.battler;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattleScripting.battler == battler && *gBattlescriptCurrInstr == B_SCR_OP_TRYACTIVATEITEM);
    gBattleScriptingCommandsTable[B_SCR_OP_TRYACTIVATEITEM]();
    if (berry)
    {
        CHECK(gBattlescriptCurrInstr == BattleScript_ItemHealHP_RemoveBerry
            && gBattleScripting.battler == battler && gBattleStruct->passiveHpUpdate[battler] < 0);
        BattleScriptPop(); /* Resume after the item presentation. */
        gBattleStruct->passiveHpUpdate[battler] = 0;
    }
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_CALLNATIVE);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattleScripting.battler == savedScripting);
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
    gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction && gBattlerTarget == 1);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
}

static void CheckSpecificDamage(const char *probe)
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
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 47));
    u8 *settings = RhhModsState("test:damage_battler"); CHECK(settings); settings[0] = 2;
    u32 untouched = 999;
    CHECK(moves && moves->header.struct_size >= RHH_MOVE_SERVICE_V1_47_SIZE
        && RHH_MOVE_SERVICE_V1_46_SIZE < RHH_MOVE_SERVICE_V1_47_SIZE
        && moves->effect_damage_from_hit(2, 1, 2, 0)
        && moves->ai_effect_offer_damage_from_hit(2, 1, 2, 100, 0) == RHH_AI_EFFECT_UNAVAILABLE
        && moves->effect_damage_battler(2, 1, 4, 0)
        && moves->ai_effect_offer_damage_battler(2, 1, 4, 100, 0) == RHH_AI_EFFECT_UNAVAILABLE
        && moves->ai_effect_damage_battler_at(0, &untouched) && untouched == 999);
    gCurrentMove = gChosenMove = MOVE_SPLASH; gBattleMons[0].moves[0] = MOVE_SPLASH; gBattleMons[0].pp[0] = 20;
    const u8 *script = GetMoveBattleScript(MOVE_SPLASH); CHECK(script == BattleScript_EffectModCustomStatus);
    u32 maxHp = gBattleMons[2].maxHP, amount = max(1, GetNonDynamaxMaxHP(2) / 4);
    gBattleMons[2].hp = maxHp; gBattleMons[2].volatiles.substitute = TRUE; gBattleMons[2].volatiles.substituteHP = 9;
    gBattleScripting.battler = 3; BeginSpecificDamage(script); FinishSpecificDamage(2, maxHp - amount, FALSE, FALSE);
    CHECK(gBattleMons[2].volatiles.substituteHP == 9 && gBattleScripting.battler == 3);
    gBattleMons[2].volatiles.substitute = FALSE; gBattleMons[2].ability = ABILITY_MAGIC_GUARD;
    BeginSpecificDamage(script); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage && gBattleMons[2].hp == maxHp - amount);
    settings[1] = RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD;
    BeginSpecificDamage(script); FinishSpecificDamage(2, maxHp - 2 * amount, FALSE, FALSE);
    settings[1] = 0; gBattleMons[2].ability = ABILITY_NONE; gBattleMons[2].hp = 1;
    BeginSpecificDamage(script); FinishSpecificDamage(2, 0, TRUE, FALSE);
    gHitMarker &= ~HITMARKER_FAINTED(2); gBattleMons[2].hp = 1;
    BeginSpecificDamage(script); gAbsentBattlerFlags |= 1u << 2;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    gAbsentBattlerFlags &= ~(1u << 2);
    BeginSpecificDamage(script); gBattlerPartyIndexes[2] = 0;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    gBattlerPartyIndexes[2] = 1;
    BeginSpecificDamage(script);
    struct Pokemon *recipientMon = GetBattlerMon(2);
    u32 originalOt = GetMonData(recipientMon, MON_DATA_OT_ID), changedOt = originalOt ^ 1u;
    SetMonData(recipientMon, MON_DATA_OT_ID, &changedOt);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    SetMonData(recipientMon, MON_DATA_OT_ID, &originalOt);
    /* Field effects use the same action. */
    gBattleMons[2].hp = maxHp; gCurrentMove = gChosenMove = MOVE_RAIN_DANCE;
    const u8 *fieldScript = GetMoveBattleScript(MOVE_RAIN_DANCE); CHECK(fieldScript == BattleScript_EffectModCustomStatus);
    BeginSpecificDamage(fieldScript); FinishSpecificDamage(2, maxHp - amount, FALSE, FALSE);
    /* HP-threshold items see the actual recipient, not the prior scripting battler. */
    u16 berry = ITEM_ORAN_BERRY; gBattleMons[2].item = berry;
    SetMonData(GetBattlerMon(2), MON_DATA_HELD_ITEM, &berry);
    gBattleMons[2].hp = maxHp / 2 + amount; gBattleScripting.battler = 3;
    BeginSpecificDamage(fieldScript); FinishSpecificDamage(2, maxHp / 2, FALSE, TRUE);
    CHECK(gBattleScripting.battler == 3); berry = ITEM_NONE; gBattleMons[2].item = berry;
    SetMonData(GetBattlerMon(2), MON_DATA_HELD_ITEM, &berry);
    /* A damaging move's real post-hit script can queue the same action. */
    gCurrentMove = gChosenMove = MOVE_SCRATCH; gBattlerAttacker = 0; gBattlerTarget = 1;
    gBattleMons[2].hp = maxHp; ModsMoveEffectReset();
    ModsRecordMoveDamage(0, 1, MOVE_SCRATCH, 5, 0);
    const u8 *hitScript = GetMoveBattleScript(MOVE_SCRATCH);
    CHECK(hitScript == BattleScript_EffectHit && hitScript[9] == B_SCR_OP_SETADDITIONALEFFECTS);
    gBattleStruct->additionalEffectsCounter = 0; gBattlescriptCurrInstr = hitScript + 9;
    gBattleScriptingCommandsTable[B_SCR_OP_SETADDITIONALEFFECTS]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_CALLNATIVE);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction);
    FinishSpecificDamage(2, maxHp - amount, FALSE, FALSE);
    /* AI uses cached ability knowledge, rather than the hidden real ability. */
    settings[0] = 3; gBattleMons[3].hp = 20; gBattleMons[3].ability = ABILITY_MAGIC_GUARD;
    gBattleMons[0].moves[1] = MOVE_RAIN_DANCE; gBattleMons[0].pp[1] = 10;
    ai.abilities[3] = ABILITY_NONE;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 115);
    ai.abilities[3] = ABILITY_MAGIC_GUARD;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 100);
    settings[1] = RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 115);
    settings[0] = 2; settings[1] = 0;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 85);
    /* A hit on the chosen recipient changes the indirect-damage range. */
    settings[0] = 1; gBattleMons[1].hp = 20; gBattleMons[1].ability = ABILITY_NONE; ai.abilities[1] = ABILITY_NONE;
    gBattleMons[0].moves[0] = MOVE_SCRATCH;
    ai.simulatedDmg[0][1][0] = (struct SimulatedDamage){5, 10, 20, 10};
    u32 fraction = max(1, GetNonDynamaxMaxHP(1) / 4);
    settings[2] = 1; CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 100 - min(fraction, 15));
    settings[2] = 2; CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 100 - min(fraction, 10));
    settings[2] = 3; CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 100);
    ai.abilities[1] = ABILITY_MAGIC_GUARD; settings[2] = 2;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 100);
    settings[1] = RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 100 - min(fraction, 10));
    /* Hit-based damage uses the capped hit snapshot, with an independent
     * known-ability AI estimate and optional Substitute inclusion. */
    settings[3] = 1; settings[0] = 2; settings[1] = 0; settings[2] = 2;
    gBattleMons[2].hp = maxHp; gBattleMons[2].ability = ABILITY_NONE; ai.abilities[2] = ABILITY_NONE;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 95);
    settings[2] = 1; CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 90);
    settings[2] = 3; CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 98);
    ai.abilities[2] = ABILITY_MAGIC_GUARD; settings[2] = 2;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 100);
    settings[1] = RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 95);
    ai.abilities[2] = ABILITY_NONE; settings[1] = 0;
    gBattleMons[1].volatiles.substitute = TRUE;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 100);
    settings[1] = RHH_EFFECT_DAMAGE_FROM_HIT_INCLUDE_SUBSTITUTE;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SCRATCH, 0, 100) == 95);
    gBattleMons[1].volatiles.substitute = FALSE;
    settings[1] = 0; settings[2] = 0; gBattleMons[2].hp = maxHp;
    BeginDamageFromHit(10, 0, TRUE); FinishSpecificDamage(2, maxHp - 5, FALSE, FALSE);
    BeginDamageFromHit(0, 8, TRUE); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage && gBattleMons[2].hp == maxHp - 5);
    settings[1] = RHH_EFFECT_DAMAGE_FROM_HIT_INCLUDE_SUBSTITUTE;
    BeginDamageFromHit(0, 8, TRUE); FinishSpecificDamage(2, maxHp - 9, FALSE, FALSE);
    settings[1] = 0; gBattleMons[2].ability = ABILITY_MAGIC_GUARD;
    BeginDamageFromHit(10, 0, TRUE); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage && gBattleMons[2].hp == maxHp - 9);
    settings[1] = RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD;
    BeginDamageFromHit(10, 0, TRUE); FinishSpecificDamage(2, maxHp - 14, FALSE, FALSE);
    settings[1] = 0; gBattleMons[2].ability = ABILITY_NONE;
    BeginDamageFromHit(0, 0, FALSE); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    BeginDamageFromHit(0, 0, TRUE); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    BeginDamageFromHit(1, 0, TRUE); FinishSpecificDamage(2, maxHp - 15, FALSE, FALSE);
    BeginDamageFromHit(10, 0, TRUE); gBattlerPartyIndexes[2] = 0;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage && gBattleMons[2].hp == maxHp - 15);
    gBattlerPartyIndexes[2] = 1;
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleSpritesDataPtr = NULL;
    gAiLogicData = NULL; gAiPartyData = NULL; gAiThinkingStruct = NULL; gAiBattleData = NULL; gBattleHistory = NULL;
    RhhModsUnloadLibraries(); puts("effect damage: specified battler, faint and AI passed");
}
