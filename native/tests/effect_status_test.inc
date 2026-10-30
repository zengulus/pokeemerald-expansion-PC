static void CheckStatusPresentation(u32 target)
{
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_STATUSANIMATION);
    gBattleScriptingCommandsTable[B_SCR_OP_STATUSANIMATION]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[target][0] == CONTROLLER_STATUSANIMATION);
    gBattleControllerExecFlags = 0;
    u32 opcode = *gBattlescriptCurrInstr;
    CHECK(opcode == B_SCR_OP_PRINTSTRING || opcode == B_SCR_OP_PRINTFROMTABLE);
    gBattleScriptingCommandsTable[opcode]();
    u8 *buffer = gBattleResources->bufferA[gBattlerAttacker];
    CHECK(gBattleControllerExecFlags && buffer[0] == CONTROLLER_PRINTSTRING);
    BufferStringBattle(buffer[2] | buffer[3] << 8, gBattlerAttacker);
    CHECK(gDisplayedStringBattle[0] != EOS);
    gBattleControllerExecFlags = 0; gPauseCounterBattle = 0;
    const u8 *wait = gBattlescriptCurrInstr;
    CHECK(*wait == B_SCR_OP_WAITMESSAGE);
    for (u32 frame = 0; frame <= B_WAIT_TIME_LONG && gBattlescriptCurrInstr == wait; ++frame)
        gBattleScriptingCommandsTable[B_SCR_OP_WAITMESSAGE]();
    CHECK(gBattlescriptCurrInstr != wait);
}
static u32 RunStatusEffectScriptForRecipient(const u8 *script, u32 target, u32 recipient, char *trace)
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
        if (gBattlescriptCurrInstr == BattleScript_MoveEffectToxic || gBattlescriptCurrInstr == BattleScript_MoveEffectSleep
            || gBattlescriptCurrInstr == BattleScript_MoveEffectPoison || gBattlescriptCurrInstr == BattleScript_MoveEffectBurn
            || gBattlescriptCurrInstr == BattleScript_MoveEffectFreeze || gBattlescriptCurrInstr == BattleScript_MoveEffectParalysis)
        {
            trace[events++] = gBattlescriptCurrInstr == BattleScript_MoveEffectToxic ? 'T'
                : gBattlescriptCurrInstr == BattleScript_MoveEffectSleep ? 'S'
                : gBattlescriptCurrInstr == BattleScript_MoveEffectPoison ? 'P'
                : gBattlescriptCurrInstr == BattleScript_MoveEffectBurn ? 'B'
                : gBattlescriptCurrInstr == BattleScript_MoveEffectFreeze ? 'F' : 'A';
            CHECK(gBattleMons[recipient].status1 & STATUS1_ANY);
            CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[recipient][0] == CONTROLLER_SETMONDATA
                && gBattleResources->bufferA[recipient][1] == REQUEST_STATUS_BATTLE);
            BtlController_HandleSetMonData(recipient);
            CHECK(!gBattleControllerExecFlags && GetMonData(GetBattlerMon(recipient), MON_DATA_STATUS) == gBattleMons[recipient].status1);
            CheckStatusPresentation(recipient);
            BattleScriptPop();
        }
        else if (gBattlescriptCurrInstr == BattleScript_MoveEffectConfusion)
        {
            trace[events++] = 'C';
            CHECK(gBattleMons[recipient].volatiles.confusionTimer && *gBattlescriptCurrInstr == B_SCR_OP_STATUSANIMATION
                && gBattlescriptCurrInstr[6] == TRUE);
            CheckStatusPresentation(recipient);
            BattleScriptPop();
        }
        else
        {
            trace[events++] = 'H';
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_HEALTHBARUPDATE && gBattleStruct->passiveHpUpdate[recipient] < 0);
            u32 oldHp = gBattleMons[recipient].hp;
            u32 expected = min(gBattleMons[recipient].maxHP, oldHp - gBattleStruct->passiveHpUpdate[recipient]);
            gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE]();
            CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[recipient][0] == CONTROLLER_HEALTHBARUPDATE);
            gBattleControllerExecFlags = 0;
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_DATAHPUPDATE);
            gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
            CHECK(gBattleControllerExecFlags && gBattleMons[recipient].hp == expected
                && gBattleResources->bufferA[recipient][0] == CONTROLLER_SETMONDATA
                && gBattleResources->bufferA[recipient][1] == REQUEST_HP_BATTLE);
            BtlController_HandleSetMonData(recipient);
            CHECK(!gBattleControllerExecFlags && GetMonData(GetBattlerMon(recipient), MON_DATA_HP) == expected);
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
            gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
        }
    }
    trace[events] = 0;
    return events;
}
static u32 RunStatusEffectScript(const u8 *script, u32 target, char *trace)
{ return RunStatusEffectScriptForRecipient(script, target, target, trace); }
static void CheckSpecificStatus(const char *probe)
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
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 32));
    u8 *settings = RhhModsState("test:status_battler"); CHECK(settings); settings[0] = 3; settings[1] = RHH_EFFECT_TOXIC;
    u32 unchanged = 999;
    CHECK(moves && moves->header.struct_size >= RHH_MOVE_SERVICE_V1_32_SIZE
        && moves->effect_status_battler(3, RHH_EFFECT_TOXIC)
        && moves->ai_effect_offer_status_battler(3, RHH_EFFECT_TOXIC, 100, 0) == RHH_AI_EFFECT_UNAVAILABLE
        && moves->ai_effect_status_battler_at(0, &unchanged) && unchanged == 999);
    gBattlerAttacker = 0; gCurrentMove = gChosenMove = MOVE_SPLASH;
    gBattleMons[0].moves[0] = MOVE_SPLASH; gBattleMons[0].pp[0] = 20;
    for (u32 i = 0; i < 4; ++i)
    {
        gBattleMons[i].ability = ABILITY_NONE; ai.abilities[i] = ABILITY_NONE;
        gBattleMons[i].types[0] = TYPE_NORMAL; gBattleMons[i].types[1] = gBattleMons[i].types[2] = TYPE_MYSTERY;
        gBattleMons[i].status1 = 0; gBattleMons[i].volatiles.confusionTimer = 0;
    }
    const u8 *script = GetMoveBattleScript(MOVE_SPLASH); CHECK(script == BattleScript_EffectModCustomStatus);
    char trace[9]; ModsMoveEffectReset();
    CHECK(RunStatusEffectScriptForRecipient(script, 1, 3, trace) == 1 && !strcmp(trace, "T")
        && (gBattleMons[3].status1 & STATUS1_TOXIC_POISON) && !gBattleMons[1].status1 && gBattlerTarget == 1);
    ModsMoveEffectReset(); gCurrentMove = gChosenMove = MOVE_RAIN_DANCE;
    settings[0] = 2; settings[1] = RHH_EFFECT_CONFUSION;
    CHECK(GetMoveBattleScript(MOVE_RAIN_DANCE) == script
        && RunStatusEffectScriptForRecipient(script, 1, 2, trace) == 1 && !strcmp(trace, "C")
        && gBattleMons[2].volatiles.confusionTimer && !gBattleMons[1].volatiles.confusionTimer && gBattlerTarget == 1);
    ModsMoveEffectReset(); settings[0] = 3; settings[1] = RHH_EFFECT_TOXIC;
    gBattleMons[3].status1 = 0; gBattleMons[3].types[0] = TYPE_STEEL;
    CHECK(!RunStatusEffectScriptForRecipient(script, 1, 3, trace) && !gBattleMons[3].status1);
    ModsMoveEffectReset(); gBattleMons[3].types[0] = TYPE_NORMAL; gBattleMons[3].ability = ABILITY_IMMUNITY;
    CHECK(!RunStatusEffectScriptForRecipient(script, 1, 3, trace) && !gBattleMons[3].status1);
    ModsMoveEffectReset(); gBattleMons[3].ability = ABILITY_NONE;
    gBattleMons[3].volatiles.substitute = TRUE; gBattleMons[3].volatiles.substituteHP = 10;
    CHECK(!RunStatusEffectScriptForRecipient(script, 1, 3, trace) && !gBattleMons[3].status1);
    gBattleMons[3].volatiles.substitute = FALSE; gBattleMons[3].volatiles.substituteHP = 0;
    BeginSpecificHeal(script); gBattleMons[3].hp = 0;
    u32 value = 999; s32 stages = 999;
    CHECK(ModsMoveEffectNextAction(&value, &stages) == MOD_EFFECT_DONE && gBattlerTarget == 1);
    gBattleMons[3].hp = gBattleMons[3].maxHP;
    BeginSpecificHeal(script); gBattlerPartyIndexes[3] = 0;
    CHECK(ModsMoveEffectNextAction(&value, &stages) == MOD_EFFECT_DONE && gBattlerTarget == 1);
    gBattlerPartyIndexes[3] = 1;
    /* The damaging hit's actual post-hit script can address a different battler. */
    gCurrentMove = gChosenMove = MOVE_SCRATCH; gBattlerAttacker = 0; gBattlerTarget = 1;
    ModsMoveEffectReset(); ModsRecordMoveDamage(0, 1, MOVE_SCRATCH, 5, 0);
    const u8 *hitScript = GetMoveBattleScript(MOVE_SCRATCH);
    CHECK(hitScript == BattleScript_EffectHit && hitScript[9] == B_SCR_OP_SETADDITIONALEFFECTS);
    gBattleStruct->additionalEffectsCounter = 0; gBattlescriptCurrInstr = hitScript + 9;
    gBattleScriptingCommandsTable[B_SCR_OP_SETADDITIONALEFFECTS]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_CALLNATIVE); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_MoveEffectToxic && gEffectBattler == 3
        && (gBattleMons[3].status1 & STATUS1_TOXIC_POISON) && !gBattleMons[1].status1 && gBattlerTarget == 1);
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[3][0] == CONTROLLER_SETMONDATA);
    BtlController_HandleSetMonData(3); CheckStatusPresentation(3); BattleScriptPop();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    gBattleMons[3].status1 = 0; gBattleMons[0].moves[1] = MOVE_RAIN_DANCE; gBattleMons[0].pp[1] = 10;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 110);
    ai.abilities[3] = ABILITY_IMMUNITY; CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 100);
    ai.abilities[3] = ABILITY_NONE; gBattleMons[3].types[0] = TYPE_STEEL;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 100);
    gBattleMons[3].types[0] = TYPE_NORMAL; settings[0] = 2; gBattleMons[2].volatiles.confusionTimer = 0;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 90);
    settings[1] = RHH_EFFECT_CONFUSION; ai.abilities[2] = ABILITY_OWN_TEMPO;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 100);
    /* Status scoring and its public query use the AI's Illusion knowledge,
     * including when the effect addresses a battler other than the candidate. */
    const struct RhhTypeService *types = (const void *)RhhModsHostAPI()->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 13));
    struct RhhExtendedTypeList outside = {.struct_size = sizeof(outside), .count = 123}, unchangedTypes = outside;
    RhhContentKey unchangedSpecies = 99;
    CHECK(types && types->header.struct_size >= RHH_TYPE_SERVICE_V1_13_SIZE
        && types->ai_battler_list(0, &outside) && !memcmp(&outside, &unchangedTypes, sizeof(outside))
        && types->ai_battler_species(0, &unchangedSpecies) && unchangedSpecies == 99);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_ZOROARK, 20, 1111, OTID_STRUCT_PRESET(5555), 31);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_MAGNEMITE, 20, 2222, OTID_STRUCT_PRESET(5555), 31);
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
    gBattleMons[1].moves[0] = MOVE_RAIN_DANCE; gBattleMons[1].pp[0] = 10;
    settings[0] = 0; settings[1] = RHH_EFFECT_TOXIC; settings[2] = 1; settings[3] = 1;
    gBattleStruct->illusion[0] = (struct Illusion){.state = ILLUSION_ON, .mon = &gParties[B_TRAINER_PLAYER][1]};
    struct BattlePokemon beforeIllusion = gBattleMons[0]; struct Illusion beforeCache = gBattleStruct->illusion[0];
    rng_value_t beforeRng = gRngValue, beforeRng2 = gRng2Value;
    CHECK(ModsAiMoveScore(1, 3, MOVE_RAIN_DANCE, 0, 100) == 100
        && !memcmp(&gBattleMons[0], &beforeIllusion, sizeof(beforeIllusion))
        && !memcmp(&gBattleStruct->illusion[0], &beforeCache, sizeof(beforeCache))
        && !memcmp(&gRngValue, &beforeRng, sizeof(beforeRng)) && !memcmp(&gRng2Value, &beforeRng2, sizeof(beforeRng2)));
    history.abilities[0] = ABILITY_ILLUSION; settings[2] = 2; settings[3] = 2;
    CHECK(ModsAiMoveScore(1, 3, MOVE_RAIN_DANCE, 0, 100) == 110);
    history.abilities[0] = ABILITY_NONE;
    history.usedMoves[0][0] = MOVE_FIERY_DANCE;
    CHECK(ModsAiMoveScore(1, 3, MOVE_RAIN_DANCE, 0, 100) == 110);
    history.usedMoves[0][0] = MOVE_NONE; settings[2] = 1; settings[3] = 1;
    gBattleMons[0].extraTypes[0] = TYPE_STEEL; gBattleMons[0].extraTypeCount = 1;
    CHECK(ModsAiMoveScore(1, 3, MOVE_RAIN_DANCE, 0, 100) == 100); /* Visible type change. */
    gBattleMons[0] = beforeIllusion; gBattleMons[2].hp = 0;
    gBattleStruct->illusion[0] = (struct Illusion){0}; beforeCache = gBattleStruct->illusion[0];
    CHECK(ModsAiMoveScore(1, 3, MOVE_RAIN_DANCE, 0, 100) == 100
        && !memcmp(&gBattleStruct->illusion[0], &beforeCache, sizeof(beforeCache)));
    gBattleStruct->illusion[0] = (struct Illusion){.state = ILLUSION_ON, .mon = &gParties[B_TRAINER_PLAYER][1]};
    u8 oldTera = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE), waterTera = TYPE_WATER;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE, &waterTera);
    SetActiveGimmick(0, GIMMICK_TERA); settings[2] = 0;
    CHECK(ModsAiMoveScore(1, 3, MOVE_RAIN_DANCE, 0, 100) == 110);
    SetActiveGimmick(0, GIMMICK_NONE); SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE, &oldTera);
    gBattlerBattleController[0] = BATTLE_CONTROLLER_OPPONENT; settings[3] = 2;
    CHECK(ModsAiMoveScore(1, 3, MOVE_RAIN_DANCE, 0, 100) == 110);
    gBattlerBattleController[0] = BATTLE_CONTROLLER_PLAYER;
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleSpritesDataPtr = NULL;
    gAiLogicData = NULL; gAiPartyData = NULL; gAiThinkingStruct = NULL; gAiBattleData = NULL; gBattleHistory = NULL;
    RhhModsUnloadLibraries(); puts("effect status: specified battler, presentation and AI passed");
}
