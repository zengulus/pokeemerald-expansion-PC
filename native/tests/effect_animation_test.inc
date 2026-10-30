#include "constants/battle_anim.h"

static void StartAnimationEffect(const u8 *script)
{
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = script + 3;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction);
}
static void DriveEffectAnimation(u32 battler, u32 animation)
{
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectAnimation);
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_PLAYANIMATION_VAR);
    gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION_VAR]();
    if (!gBattleControllerExecFlags || gBattleResources->bufferA[battler][0] != CONTROLLER_BATTLEANIMATION
        || gBattleResources->bufferA[battler][1] != animation || *gBattlescriptCurrInstr != B_SCR_OP_WAITSTATE)
        fprintf(stderr, "animation diagnostic: battler=%u expected=%u flags=%u command=%u id=%u opcode=%u hit=%u\n",
            battler, animation, gBattleControllerExecFlags, gBattleResources->bufferA[battler][0],
            gBattleResources->bufferA[battler][1], *gBattlescriptCurrInstr, gHitMarker);
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[battler][0] == CONTROLLER_BATTLEANIMATION
        && gBattleResources->bufferA[battler][1] == animation && *gBattlescriptCurrInstr == B_SCR_OP_WAITSTATE);
    const u8 *waiting = gBattlescriptCurrInstr;
    gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE](); CHECK(gBattlescriptCurrInstr == waiting);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO); gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction);
}
static void DriveExpiryAnimation(u32 battler, u32 animation)
{
    CHECK(gBattlescriptCurrInstr == BattleScript_ModConditionMessages);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModConditionAnimation);
    gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION_VAR]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[battler][0] == CONTROLLER_BATTLEANIMATION
        && gBattleResources->bufferA[battler][1] == animation && *gBattlescriptCurrInstr == B_SCR_OP_WAITSTATE);
    const u8 *waiting = gBattlescriptCurrInstr;
    gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE](); CHECK(gBattlescriptCurrInstr == waiting);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO); gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
}
static void StartExpiryTurn(u32 turn)
{
    ModsMoveEffectReset(); gBattleTurnCounter = turn; gBattleStruct->eventState.endTurn = ENDTURN_COUNT;
    gBattleStruct->eventState.endTurnBattler = gBattleStruct->eventState.battlerSide = 0;
    gBattlescriptCurrInstr = BattleScript_EndTurnEvents; gBattleControllerExecFlags = 0;
    CHECK(DoEndTurnEffects() && gBattlescriptCurrInstr == BattleScript_ModConditionMessages);
}
static void DriveInnateConditionAnimation(u32 battler, u32 animation)
{
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateTurnAction);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateTurnAnimation && gBattleScripting.battler == battler);
    gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION_VAR]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[battler][0] == CONTROLLER_BATTLEANIMATION
        && gBattleResources->bufferA[battler][1] == animation && *gBattlescriptCurrInstr == B_SCR_OP_WAITSTATE);
    const u8 *waiting = gBattlescriptCurrInstr;
    gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE](); CHECK(gBattlescriptCurrInstr == waiting);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO); gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateTurnAction);
}
static void BeginConditionInnateTurn(void)
{
    gBattleTurnCounter = 1; gBattleStruct->eventState.endTurn = ENDTURN_COUNT;
    gBattleStruct->eventState.endTurnBattler = gBattleStruct->eventState.battlerSide = 0;
    gBattlescriptCurrInstr = BattleScript_EndTurnEvents; gBattleControllerExecFlags = 0;
    CHECK(DoEndTurnEffects() && gBattlescriptCurrInstr == BattleScript_ModInnateTurnAction);
}
static void DriveEntryConditionAnimation(u32 battler, u32 animation)
{
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateMessages);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateAnimation && gBattleScripting.battler == battler);
    gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION_VAR]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[battler][0] == CONTROLLER_BATTLEANIMATION
        && gBattleResources->bufferA[battler][1] == animation && *gBattlescriptCurrInstr == B_SCR_OP_WAITSTATE);
    const u8 *waiting = gBattlescriptCurrInstr;
    gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE](); CHECK(gBattlescriptCurrInstr == waiting);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO); gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
}
static void DriveResolutionCue(u32 battler, u32 animation)
{
    CHECK(gBattlescriptCurrInstr == BattleScript_ModResolutionCues);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_PLAYANIMATION_VAR);
    gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION_VAR]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[battler][0] == CONTROLLER_BATTLEANIMATION
        && gBattleResources->bufferA[battler][1] == animation && *gBattlescriptCurrInstr == B_SCR_OP_WAITSTATE);
    const u8 *waiting = gBattlescriptCurrInstr;
    gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE](); CHECK(gBattlescriptCurrInstr == waiting);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO); gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
}
static void FinishResolutionCues(const u8 *command)
{
    CHECK(gBattlescriptCurrInstr == BattleScript_ModResolutionCues);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModResolutionCuesEnd);
    gBattleScriptingCommandsTable[B_SCR_OP_RETURN](); CHECK(gBattlescriptCurrInstr == command);
    gBattleScriptingCommandsTable[B_SCR_OP_MOVEEND]();
    CHECK(gBattlescriptCurrInstr == command + 3);
}
