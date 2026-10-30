static char DriveDrainScript(const u8 *resume)
{
    bool32 ooze = gBattlescriptCurrInstr == BattleScript_EffectAbsorbLiquidOoze;
    if (ooze)
    {
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_CALL);
        /* The ordinary ability popup is presentation-only; test its following branch. */
        gBattlescriptCurrInstr += 1 + sizeof(void *);
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_JUMPIFABILITY);
        gBattleScriptingCommandsTable[B_SCR_OP_JUMPIFABILITY]();
        if (*gBattlescriptCurrInstr == B_SCR_OP_RETURN)
        {
            gBattleScriptingCommandsTable[B_SCR_OP_RETURN]();
            CHECK(gBattlescriptCurrInstr == resume);
            return 'M';
        }
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
        gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    }
    CHECK(gBattlescriptCurrInstr == BattleScript_EffectAbsorb && *gBattlescriptCurrInstr == B_SCR_OP_HEALTHBARUPDATE);
    s32 change = gBattleStruct->passiveHpUpdate[0];
    u32 oldHp = gBattleMons[0].hp;
    u32 expected = change < 0 ? min(gBattleMons[0].maxHP, oldHp + (u32)-change)
                             : oldHp > (u32)change ? oldHp - (u32)change : 0;
    gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE](); CHECK(gBattleControllerExecFlags);
    gBattleControllerExecFlags = 0;
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_DATAHPUPDATE);
    gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE](); CHECK(gBattleControllerExecFlags);
    BtlController_HandleSetMonData(0);
    CHECK(!gBattleControllerExecFlags && gBattleMons[0].hp == expected);
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_PRINTFROMTABLE);
    gBattleScriptingCommandsTable[B_SCR_OP_PRINTFROMTABLE](); CHECK(gBattleControllerExecFlags);
    gBattleControllerExecFlags = 0; gPauseCounterBattle = 0;
    const u8 *wait = gBattlescriptCurrInstr; CHECK(*wait == B_SCR_OP_WAITMESSAGE);
    for (u32 frame = 0; frame <= B_WAIT_TIME_LONG && gBattlescriptCurrInstr == wait; ++frame)
        gBattleScriptingCommandsTable[B_SCR_OP_WAITMESSAGE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_TRYFAINTMON);
    gBattleScriptingCommandsTable[B_SCR_OP_TRYFAINTMON]();
    bool32 fainted = !gBattleMons[0].hp;
    if (fainted)
    {
        CHECK(gBattlescriptCurrInstr == BattleScript_FaintBattler && (gHitMarker & HITMARKER_FAINTED(0)));
        BattleScriptPop(); /* Simulate the engine completing the faint presentation. */
    }
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_RETURN);
    gBattleScriptingCommandsTable[B_SCR_OP_RETURN]();
    CHECK(gBattlescriptCurrInstr == resume);
    return fainted ? 'F' : ooze ? 'O' : 'D';
}

static u32 RunDamageEffectTail(u32 target, char *trace)
{
    const u8 *script = GetMoveBattleScript(gCurrentMove);
    CHECK(script == BattleScript_EffectHit && script[9] == B_SCR_OP_SETADDITIONALEFFECTS);
    gBattlerTarget = target; gBattleControllerExecFlags = 0;
    gBattlescriptCurrInstr = script + 9;
    gBattleScriptingCommandsTable[B_SCR_OP_SETADDITIONALEFFECTS]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_CALLNATIVE);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    if (gBattlescriptCurrInstr == BattleScript_MoveEnd)
    {
        trace[0] = 0; return 0;
    }
    u32 events = 0;
    for (u32 step = 0; ; ++step)
    {
        CHECK(step < 9 && gBattlescriptCurrInstr == BattleScript_ModEffectNextAction);
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        if (gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage) break;
        if (gBattlescriptCurrInstr == BattleScript_ModEffectNextAction) continue;
        if (gBattlescriptCurrInstr == BattleScript_MoveEffectStatChange)
        {
            trace[events++] = 'S'; DriveStatChangeScript();
        }
        else if (gBattlescriptCurrInstr == BattleScript_EffectAbsorb
                 || gBattlescriptCurrInstr == BattleScript_EffectAbsorbLiquidOoze)
        {
            trace[events++] = DriveDrainScript(BattleScript_ModEffectNextAction);
        }
        else
        {
            CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectHealUser);
            trace[events++] = 'U';
            u32 expected = min(gBattleMons[0].maxHP, gBattleMons[0].hp - gBattleStruct->passiveHpUpdate[0]);
            gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE](); CHECK(gBattleControllerExecFlags);
            gBattleControllerExecFlags = 0;
            gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE](); CHECK(gBattleControllerExecFlags);
            BtlController_HandleSetMonData(0);
            CHECK(!gBattleControllerExecFlags && gBattleMons[0].hp == expected);
            gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
        }
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_MoveEnd); trace[events] = 0; return events;
}

static void RunRecordedMoveDamage(u32 move, u32 damage, bool32 breaksSubstitute)
{
    gBattlerAttacker = 0; gBattlerTarget = 1; gCurrentMove = gChosenMove = move;
    gBattleStruct->moveDamage[1] = damage; gBattleStruct->moveResultFlags[1] = 0;
    gBattleStruct->eventState.moveEndBattler = 0;
    gBattleStruct->eventState.atkCanceler = CANCELER_MOVE_DAMAGE_UPDATE;
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = BattleScript_EffectHit;
    enum CancelerResult result = DoAttackCanceler();
    if (breaksSubstitute)
    {
        CHECK(result == CANCELER_RESULT_RUN_SCRIPT && gBattlescriptCurrInstr == BattleScript_SubstituteFade);
        BattleScriptPop(); gBattleMons[1].volatiles.substitute = FALSE;
        result = DoAttackCanceler();
    }
    CHECK(result == CANCELER_RESULT_RUN_SCRIPT_AND_INCREMENT);
    if (gBattleControllerExecFlags) BtlController_HandleSetMonData(1);
    CHECK(!gBattleControllerExecFlags);
}
static void RunRecordedDamage(u32 damage, bool32 breaksSubstitute)
{ RunRecordedMoveDamage(MOVE_TACKLE, damage, breaksSubstitute); }

static void CheckDamageEffect(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 19));
    RhhContentKey key = RhhContentKeyOf("test:move/impact"), effect = RhhContentKeyOf("test:effect/impact"), bound = 0;
    struct RhhMoveEffectInfo info = {.struct_size = sizeof(info)}; u32 slot, mode;
    CHECK(moves && !moves->registered_effect_info(effect, &info) && info.mode == RHH_MOVE_EFFECT_CUSTOM_DAMAGE
        && !moves->registered_effect_for_move(key, &bound) && bound == effect
        && !moves->effect_mode(key, &mode) && mode == RHH_MOVE_EFFECT_CUSTOM_DAMAGE
        && !RhhModsMoveSlot(key, TRUE, &slot) && GetMoveEffect(slot) == EFFECT_HIT);
    struct RhhMoveDamageEffectContext outside = {.struct_size = sizeof(outside), .hp_lost = 999}, unchanged = outside;
    CHECK(moves->effect_damage_context(&outside) && !memcmp(&outside, &unchanged, sizeof(outside))
        && moves->effect_heal_user_from_damage(1, 2, 0) && moves->effect_drain_user(1, 2, 0));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0}; struct BattleHistory history = {0};
    struct AiPartyData parties = {0};
    struct BattleSpriteInfo spriteInfo[MAX_BATTLERS_COUNT] = {0}; struct BattleSpriteData spriteData = {.battlerData = spriteInfo};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources; gBattleSpritesDataPtr = &spriteData; gBattleHistory = &history; gAiPartyData = &parties;
    PrepareTurnBattle(); ModsBattleBegin();
    for (u32 i = 0; i < MAX_BATTLERS_COUNT; ++i) gBattlerControllerEndFuncs[i] = HealTestControllerComplete;
    gBattlerAttacker = 0; gCurrentMove = gChosenMove = MOVE_TACKLE; gBattleMons[0].hp = 1;
    uint32_t *counts = RhhModsState("test:damage_effect"); CHECK(counts);
    char trace[9];
    CHECK(RunDamageEffectTail(1, trace) == 2 && !strcmp(trace, "SU") && counts[0] == 1 && counts[1] == 2
        && counts[2] == 1 && !counts[3] && gBattleMons[1].statStages[STAT_DEF] == DEFAULT_STAT_STAGE - 1
        && gBattleMons[0].hp == 1 + max(1, GetNonDynamaxMaxHP(0) / 4));
    CHECK(!RunDamageEffectTail(1, trace) && counts[0] == 1); /* No duplicate target callback. */
    ModsMoveEffectReset(); gBattleStruct->moveResultFlags[1] = MOVE_RESULT_MISSED;
    CHECK(!RunDamageEffectTail(1, trace) && counts[0] == 1); gBattleStruct->moveResultFlags[1] = 0;
    ModsMoveEffectReset(); gBattleStruct->moveResultFlags[1] = MOVE_RESULT_PROTECTED;
    CHECK(!RunDamageEffectTail(1, trace) && counts[0] == 1); gBattleStruct->moveResultFlags[1] = 0;
    ModsMoveEffectReset(); gBattleMons[0].hp = 1; gBattleMons[1].hp = 0;
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "U") && counts[0] == 2);
    gBattleMons[1].hp = 1; ModsMoveEffectReset(); gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(!moves->effect_mode(key, &mode) && mode == RHH_MOVE_EFFECT_INHERITED);
    CHECK(!RunDamageEffectTail(1, trace) && counts[0] == 2); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    ModsMoveEffectReset(); gCurrentMove = gChosenMove = slot; gBattleMons[0].hp = 1;
    CHECK(RunDamageEffectTail(1, trace) == 2 && !strcmp(trace, "SU") && counts[0] == 3);
    ModsMoveEffectReset(); gCurrentMove = gChosenMove = MOVE_SURF; gBattleMons[0].hp = 1;
    CHECK(RunDamageEffectTail(1, trace) == 2 && !strcmp(trace, "SU"));
    CHECK(RunDamageEffectTail(3, trace) == 1 && !strcmp(trace, "S") && counts[0] == 5
        && (counts[1] & (1u << 3)) && counts[2] == 4 && !counts[3] && counts[4] == 5);
    ModsMoveEffectReset(); gCurrentMove = gChosenMove = MOVE_SCRATCH;
    CHECK(!moves->effect_mode(RhhContentKeyOf("rhh:move/scratch"), &mode) && mode == RHH_MOVE_EFFECT_CUSTOM_DAMAGE
        && !RunDamageEffectTail(1, trace) && counts[0] == 5 && counts[4] == 6);
    CHECK(GetMoveAdditionalEffectCount(MOVE_FLAMETHROWER) == gMovesInfo[MOVE_FLAMETHROWER].numAdditionalEffects
        && GetMoveAdditionalEffectCount(MOVE_FLAMETHROWER) > 0);
    /* Drive Flamethrower's real secondary effect, then resume its hit tail. */
    ModsMoveEffectReset(); gCurrentMove = gChosenMove = MOVE_FLAMETHROWER; gBattlerTarget = 1;
    gBattleMons[0].hp = 1; gBattleMons[1].hp = gBattleMons[1].maxHP; gBattleMons[1].status1 = 0;
    gBattleMons[1].ability = ABILITY_NONE; SET_BATTLER_TYPE(1, TYPE_NORMAL);
    const u8 *secondary = BattleScript_EffectHit + 9; bool32 burned = FALSE;
    for (u32 attempt = 0; attempt < 2048 && !burned; ++attempt)
    {
        gBattleStruct->additionalEffectsCounter = 0; gBattlescriptCurrInstr = secondary;
        gBattleScriptingCommandsTable[B_SCR_OP_SETADDITIONALEFFECTS]();
        burned = gBattlescriptCurrInstr == BattleScript_MoveEffectBurn;
    }
    CHECK(burned && (gBattleMons[1].status1 & STATUS1_BURN) && counts[0] == 5 && counts[4] == 6);
    BtlController_HandleSetMonData(1); CheckStatusPresentation(1); BattleScriptPop();
    CHECK(gBattlescriptCurrInstr == secondary && gBattleStruct->additionalEffectsCounter == 1);
    CHECK(RunDamageEffectTail(1, trace) == 2 && !strcmp(trace, "SU") && counts[0] == 6 && counts[4] == 7);
    /* Real damage is capped by current HP and drives the user-healing action. */
    ModsMoveEffectReset(); counts[9] = 1; gBattleMons[0].hp = 1; gBattleMons[1].hp = 20;
    gBattleMons[1].status1 = 0; RunRecordedDamage(30, FALSE);
    CHECK(!gBattleMons[1].hp && RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "U")
        && counts[5] == RHH_MOVE_DAMAGE_RECORDED && counts[6] == 20 && !counts[7]
        && !counts[8] && gBattleMons[0].hp == 11);
    /* A substitute hit can be excluded or explicitly included. */
    ModsMoveEffectReset(); gBattleMons[0].hp = 1; gBattleMons[1].hp = 20;
    gBattleMons[1].volatiles.substitute = TRUE; gBattleMons[1].volatiles.substituteHP = 10;
    RunRecordedDamage(6, FALSE);
    CHECK(!RunDamageEffectTail(1, trace) && counts[5] == RHH_MOVE_DAMAGE_RECORDED
        && !counts[6] && counts[7] == 6 && !counts[8] && gBattleMons[0].hp == 1
        && gBattleMons[1].hp == 20 && gBattleMons[1].volatiles.substituteHP == 4);
    ModsMoveEffectReset(); counts[9] = 2; gBattleMons[0].hp = 1;
    RunRecordedDamage(30, TRUE);
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "U") && !counts[6] && counts[7] == 4
        && !counts[8] && gBattleMons[0].hp == 3 && gBattleMons[1].hp == 20);
    ModsMoveEffectReset(); counts[9] = 1; gBattleMons[0].hp = 1;
    RunRecordedDamage(0, FALSE);
    CHECK(!RunDamageEffectTail(1, trace) && counts[5] == RHH_MOVE_DAMAGE_RECORDED
        && !counts[6] && !counts[7] && !counts[8] && gBattleMons[0].hp == 1);
    ModsMoveEffectReset(); gBattleMons[0].hp = 1; gBattleMons[1].hp = 20;
    RunRecordedDamage(1, FALSE);
    gBattleStruct->moveDamage[1] = 999; /* Later engine changes must not rewrite the snapshot. */
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "U")
        && counts[6] == 1 && gBattleMons[0].hp == 2 && !counts[8]);
    ModsMoveEffectReset(); gBattleMons[0].hp = 1; gBattleMons[1].hp = 20;
    gBattleMons[0].volatiles.healBlockTimer = 1; RunRecordedDamage(8, FALSE);
    CHECK(!RunDamageEffectTail(1, trace) && gBattleMons[0].hp == 1 && counts[6] == 8 && !counts[8]);
    gBattleMons[0].volatiles.healBlockTimer = 0;
    ModsMoveEffectReset(); gBattleMons[0].hp = gBattleMons[0].maxHP; gBattleMons[1].hp = 20;
    RunRecordedDamage(8, FALSE);
    CHECK(!RunDamageEffectTail(1, trace) && gBattleMons[0].hp == gBattleMons[0].maxHP && !counts[8]);
    /* No HP update is distinguishable from a recorded zero and rejects healing. */
    ModsMoveEffectReset(); gBattleMons[0].hp = 1; gBattleMons[1].hp = 20;
    CHECK(!RunDamageEffectTail(1, trace) && !counts[5] && !counts[6] && !counts[7]
        && !counts[8] && gBattleMons[0].hp == 1);
    /* Drain uses the engine's absorb scripts and its held-item and ability rules. */
    ModsMoveEffectReset(); counts[9] = 3; gBattleMons[0].hp = 10; gBattleMons[1].hp = 30;
    RunRecordedDamage(20, FALSE);
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "D")
        && gBattleMons[0].hp == 20 && !counts[8]);
    ModsMoveEffectReset(); gBattleMons[0].hp = 10; gBattleMons[0].item = ITEM_BIG_ROOT; gBattleMons[1].hp = 30;
    RunRecordedDamage(20, FALSE);
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "D") && gBattleMons[0].hp == 23 && !counts[8]);
    ModsMoveEffectReset(); gBattleMons[0].hp = 20; gBattleMons[1].hp = 10; gBattleMons[1].ability = ABILITY_LIQUID_OOZE;
    RunRecordedDamage(20, FALSE);
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "O")
        && gBattleMons[0].hp == 14 && !gBattleMons[1].hp && !counts[8]);
    ModsMoveEffectReset(); gBattleMons[0].item = ITEM_NONE; gBattleMons[0].hp = gBattleMons[0].maxHP;
    gBattleMons[1].hp = 30; RunRecordedDamage(20, FALSE);
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "O")
        && gBattleMons[0].hp == gBattleMons[0].maxHP - 10 && !counts[8]);
    ModsMoveEffectReset(); gBattleMons[0].hp = 20; gBattleMons[0].ability = ABILITY_MAGIC_GUARD;
    gBattleMons[1].hp = 30; RunRecordedDamage(20, FALSE);
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "M") && gBattleMons[0].hp == 20 && !counts[8]);
    ModsMoveEffectReset(); gBattleMons[0].ability = ABILITY_NONE; gBattleMons[0].item = ITEM_NONE;
    gBattleMons[0].hp = 10; gBattleMons[1].hp = 30; gBattleMons[1].volatiles.gastroAcid = TRUE;
    RunRecordedDamage(20, FALSE);
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "D") && gBattleMons[0].hp == 20 && !counts[8]);
    ModsMoveEffectReset(); gBattleMons[1].volatiles.gastroAcid = FALSE; gBattleMons[0].hp = 10;
    gBattleMons[0].volatiles.healBlockTimer = 1; gBattleMons[1].hp = 30; RunRecordedDamage(20, FALSE);
    CHECK(!RunDamageEffectTail(1, trace) && gBattleMons[0].hp == 10 && !counts[8]);
    ModsMoveEffectReset(); gBattleMons[0].volatiles.healBlockTimer = 0; gBattleMons[0].hp = 10;
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].hp = 30;
    gBattleMons[1].volatiles.substitute = TRUE; gBattleMons[1].volatiles.substituteHP = 10;
    RunRecordedDamage(6, FALSE);
    CHECK(!RunDamageEffectTail(1, trace) && gBattleMons[0].hp == 10 && !counts[8]);
    ModsMoveEffectReset(); counts[9] = 4; gBattleMons[0].hp = 10;
    RunRecordedDamage(30, TRUE);
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "D") && gBattleMons[0].hp == 12 && !counts[8]);
    ModsMoveEffectReset(); counts[9] = 3; gBattleMons[0].hp = 10; gBattleMons[1].hp = 30;
    RunRecordedDamage(1, FALSE);
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "D") && gBattleMons[0].hp == 11 && !counts[8]);
    ModsMoveEffectReset(); gBattleMons[0].hp = 10; gBattleMons[1].hp = 30; RunRecordedDamage(0, FALSE);
    CHECK(!RunDamageEffectTail(1, trace) && gBattleMons[0].hp == 10 && !counts[8]);
    /* The shared handler still executes a built-in Absorb move normally. */
    ModsMoveEffectReset(); gBattleMons[0].hp = 10; gBattleMons[1].hp = 30;
    RunRecordedDamage(20, FALSE); gCurrentMove = gChosenMove = MOVE_ABSORB;
    const u8 *absorbTail = BattleScript_EffectHit + 9;
    gBattlescriptCurrInstr = absorbTail; gBattleStruct->additionalEffectsCounter = 0;
    gBattleScriptingCommandsTable[B_SCR_OP_SETADDITIONALEFFECTS]();
    CHECK(gBattlescriptCurrInstr == BattleScript_EffectAbsorb && DriveDrainScript(absorbTail) == 'D'
        && gBattleMons[0].hp == 20);
    ModsMoveEffectReset(); counts[9] = 3; gCurrentMove = gChosenMove = MOVE_TACKLE;
    gBattleMons[0].hp = 10; gBattleMons[1].hp = 20;
    CHECK(!RunDamageEffectTail(1, trace) && !counts[5] && !counts[8] && gBattleMons[0].hp == 10);
    ModsMoveEffectReset(); counts[9] = 5; gBattleMons[0].hp = 10; gBattleMons[1].hp = 30;
    RunRecordedDamage(8, FALSE);
    CHECK(RunDamageEffectTail(1, trace) == 8 && !strcmp(trace, "DDDDDDDD") && gBattleMons[0].hp == 18 && !counts[8]);
    /* A lethal Liquid Ooze result enters the ordinary faint script. */
    gBattlerPositions[0] = B_POSITION_OPPONENT_LEFT; gBattlerPositions[1] = B_POSITION_PLAYER_LEFT;
    gBattlerPositions[2] = B_POSITION_OPPONENT_RIGHT; gBattlerPositions[3] = B_POSITION_PLAYER_RIGHT;
    ModsMoveEffectReset(); counts[9] = 3; gBattleMons[0].hp = 3; gBattleMons[1].hp = 30;
    gBattleMons[1].ability = ABILITY_LIQUID_OOZE; gHitMarker = 0; RunRecordedDamage(20, FALSE);
    CHECK(RunDamageEffectTail(1, trace) == 1 && !strcmp(trace, "F") && !gBattleMons[0].hp && !counts[8]);
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleSpritesDataPtr = NULL; gBattleHistory = NULL; gAiPartyData = NULL; RhhModsUnloadLibraries();
    puts("damage effect: registered moves, hit outcomes, knockout and spread passed");
}

static void CheckDamageEffectInvalid(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module));
    CHECK(RhhModsFinalize() && strstr(RhhModsError(), "incompatible"));
    RhhModsUnloadLibraries(); puts("damage effect: incompatible binding rejected");
}

