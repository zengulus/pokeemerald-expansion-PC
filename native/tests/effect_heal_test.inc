#include "battle_dynamax.h"

static void HealTestControllerComplete(enum BattlerId battler)
{
    (void)battler;
    gBattleControllerExecFlags = 0;
}
static u32 sHealTestAmounts[8];
static u32 RunHealEffectScript(const u8 *script, u32 target)
{
    gBattlerTarget = target; gBattleControllerExecFlags = 0;
    gBattlescriptCurrInstr = script + 3;
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_CALLNATIVE);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    u32 applied = 0;
    for (;;)
    {
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_CALLNATIVE);
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        if (gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage) break;
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_HEALTHBARUPDATE
            && gBattleStruct->passiveHpUpdate[target] < 0);
        CHECK(applied < ARRAY_COUNT(sHealTestAmounts));
        sHealTestAmounts[applied] = -gBattleStruct->passiveHpUpdate[target];
        u32 oldHp = gBattleMons[target].hp;
        u32 expected = min(gBattleMons[target].maxHP, oldHp - gBattleStruct->passiveHpUpdate[target]);
        gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE]();
        CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[target][0] == CONTROLLER_HEALTHBARUPDATE);
        gBattleControllerExecFlags = 0;
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_DATAHPUPDATE);
        gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
        CHECK(gBattleControllerExecFlags && gBattleMons[target].hp == expected
            && gBattleResources->bufferA[target][0] == CONTROLLER_SETMONDATA
            && gBattleResources->bufferA[target][1] == REQUEST_HP_BATTLE);
        BtlController_HandleSetMonData(target);
        CHECK(!gBattleControllerExecFlags && GetMonData(GetBattlerMon(target), MON_DATA_HP) == expected);
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
        gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
        CHECK(++applied <= 8);
    }
    return applied;
}
static void BeginSpecificHeal(const u8 *script)
{
    ModsMoveEffectReset(); gBattlerAttacker = 0; gBattlerTarget = 1;
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = script + 3;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction);
}
static void CheckSpecificHealing(const char *probe)
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
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 27));
    u8 *recipient = RhhModsState("test:heal_battler"); CHECK(recipient); *recipient = 2;
    u32 untouched = 999;
    CHECK(moves && moves->header.struct_size >= RHH_MOVE_SERVICE_V1_27_SIZE && moves->effect_heal_battler(2, 1, 4)
        && moves->ai_effect_offer_heal_battler(2, 1, 4, 100, 0) == RHH_AI_EFFECT_UNAVAILABLE
        && moves->ai_effect_heal_battler_at(0, &untouched) && untouched == 999);
    gCurrentMove = gChosenMove = MOVE_SPLASH; gBattleMons[0].moves[0] = MOVE_SPLASH; gBattleMons[0].pp[0] = 20;
    CHECK(AI_GetBattlerMoveTargetType(0, MOVE_SPLASH) == TARGET_USER_AND_ALLY);
    const u8 *script = GetMoveBattleScript(MOVE_SPLASH); CHECK(script == BattleScript_EffectModCustomStatus);
    gBattleMons[2].hp = 1; u32 expected = min(gBattleMons[2].maxHP, 1 + max(1, GetNonDynamaxMaxHP(2) / 4));
    gBattleScripting.battler = 3; BeginSpecificHeal(script);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectHealBattler && gBattleScripting.battler == 3
        && gBattlerTarget == 1 && gBattleStruct->passiveHpUpdate[2] < 0);
    gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[2][0] == CONTROLLER_HEALTHBARUPDATE);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
    CHECK(gBattleControllerExecFlags && gBattleMons[2].hp == expected);
    BtlController_HandleSetMonData(2);
    CHECK(!gBattleControllerExecFlags && GetMonData(GetBattlerMon(2), MON_DATA_HP) == expected);
    gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction && gBattlerTarget == 1 && gBattleScripting.battler == 3);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    /* Blocked, full or fainted recipients are skipped when the queue executes. */
    gBattleMons[2].hp = 1; BeginSpecificHeal(script); gBattleMons[2].volatiles.healBlockTimer = 1;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    gBattleMons[2].volatiles.healBlockTimer = 0; BeginSpecificHeal(script); gBattleMons[0].volatiles.healBlockTimer = 1;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    gBattleMons[0].volatiles.healBlockTimer = 0; BeginSpecificHeal(script); gBattleMons[2].hp = 0;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    gBattleMons[2].hp = 1; BeginSpecificHeal(script); gAbsentBattlerFlags |= 1u << 2;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    gAbsentBattlerFlags &= ~(1u << 2); BeginSpecificHeal(script); gBattleMons[2].hp = gBattleMons[2].maxHP;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    /* Queued commands must not attach themselves to replacement occupants. */
    gBattleMons[2].hp = 1;
    u32 value = 999; s32 stages = 999;
    BeginSpecificHeal(script); gBattlerPartyIndexes[2] = 0;
    CHECK(ModsMoveEffectNextAction(&value, &stages) == MOD_EFFECT_DONE); gBattlerPartyIndexes[2] = 1;
    BeginSpecificHeal(script); gBattlerPartyIndexes[0] = 1;
    CHECK(ModsMoveEffectNextAction(&value, &stages) == MOD_EFFECT_DONE); gBattlerPartyIndexes[0] = 0;
    BeginSpecificHeal(script); gBattlerPartyIndexes[1] = 1;
    CHECK(ModsMoveEffectNextAction(&value, &stages) == MOD_EFFECT_DONE); gBattlerPartyIndexes[1] = 0;
    BeginSpecificHeal(script); gCurrentMove = MOVE_RAIN_DANCE;
    CHECK(ModsMoveEffectNextAction(&value, &stages) == MOD_EFFECT_DONE); gCurrentMove = MOVE_SPLASH;
    /* Field effects can also request a battler without rewriting the target. */
    gBattleMons[2].hp = 1; gCurrentMove = gChosenMove = MOVE_RAIN_DANCE;
    const u8 *fieldScript = GetMoveBattleScript(MOVE_RAIN_DANCE); CHECK(fieldScript == BattleScript_EffectModCustomStatus);
    BeginSpecificHeal(fieldScript); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectHealBattler && gBattlerTarget == 1);
    gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE](); CHECK(gBattleControllerExecFlags);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
    CHECK(gBattleControllerExecFlags && gBattleMons[2].hp == expected); BtlController_HandleSetMonData(2);
    /* A field move has no original target participant; ordinary form changes
     * also retain the recipient identity. */
    gBattleMons[2].hp = 1; BeginSpecificHeal(fieldScript);
    u32 oldSpecies = gBattleMons[2].species; gBattleMons[2].species = SPECIES_POOCHYENA;
    gAbsentBattlerFlags |= 1u << 1;
    CHECK(ModsMoveEffectNextAction(&value, &stages) == MOD_EFFECT_HEAL_BATTLER && ModsMoveEffectRecipient() == 2);
    gAbsentBattlerFlags &= ~(1u << 1); gBattleMons[2].species = oldSpecies;
    gCurrentMove = gChosenMove = MOVE_SPLASH; gBattleMons[2].hp = 1;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SPLASH, 0, 100) == 115);
    gBattleMons[2].volatiles.healBlockTimer = 1;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SPLASH, 0, 100) == 100);
    gBattleMons[2].volatiles.healBlockTimer = 0; gBattleMons[0].volatiles.healBlockTimer = 1;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SPLASH, 0, 100) == 100);
    gBattleMons[0].volatiles.healBlockTimer = 0;
    gBattleMons[0].moves[1] = MOVE_RAIN_DANCE; gBattleMons[0].pp[1] = 10;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 115);
    *recipient = 3; gBattleMons[3].hp = 1;
    CHECK(ModsAiMoveScore(0, 1, MOVE_SPLASH, 0, 100) == 85);
    gBattleMons[3].volatiles.healBlockTimer = 1;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 100);
    *recipient = 2; recipient[1] = 1;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 1, 100) == 100);
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleSpritesDataPtr = NULL;
    gAiLogicData = NULL; gAiPartyData = NULL; gAiThinkingStruct = NULL; gAiBattleData = NULL; gBattleHistory = NULL;
    RhhModsUnloadLibraries(); puts("effect healing: specified battler, controller and AI passed");
}
