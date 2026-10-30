#include "battle_move_resolution.h"

static void FinishTestMove(u32 move, u32 target)
{
    gCurrentMove = gChosenMove = move; gBattlerAttacker = 0; gBattlerTarget = target;
    gBattleScripting.moveendState = MOVEEND_CLEAR_BITS;
    CHECK(DoMoveEnd(1, MOVEEND_CLEAR_BITS) == MOVEEND_RESULT_CONTINUE);
}
static void CheckMoveResolution(const char *module, const char *custom)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module));
    if (custom) CHECK(!RhhModsLoadLibrary(custom));
    CHECK(!RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    u32 customSlot = MOVE_NONE;
    if (custom) CHECK(!RhhModsMoveSlot(RhhContentKeyOf("test:move/tidal_pulse"), TRUE, &customSlot));
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 8));
    CHECK(moves);
    struct RhhMoveResolutionContext outside = {.struct_size = sizeof(outside), .attacker = 999}, unchanged = outside;
    CHECK(moves->resolution_context(&outside) && !memcmp(&outside, &unchanged, sizeof(outside)));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0}; struct AiLogicData ai = {0};
    struct AiPartyData parties = {0}; struct AiThinkingStruct thinking = {0}; struct BattleHistory history = {0};
    memset(&sBattle, 0, sizeof(sBattle)); gBattleStruct = &sBattle; gBattleResources = &resources;
    resources.battleScriptsStack = &scripts; gAiLogicData = &ai; gAiPartyData = &parties;
    gAiThinkingStruct = &thinking; gBattleHistory = &history;
    gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE;
    gBattlersCount = 4; gAbsentBattlerFlags = 0; gBattleTurnCounter = 3;
    for (u32 side = 0; side < 2; ++side)
    {
        CreateMonWithIVs(&gParties[side][0], side ? SPECIES_CHARIZARD : SPECIES_TORCHIC,
                         20, 9000 + side, OTID_STRUCT_PRESET(54321 + side), 31);
        gParties[side][1] = gParties[side][0]; gPartiesCount[side] = 2;
    }
    for (u32 i = 0; i < 4; ++i)
    {
        gBattlerPositions[i] = i; gBattlerPartyIndexes[i] = i / 2;
        PokemonToBattleMon(&gParties[i & 1][i / 2], &gBattleMons[i]);
        gBattleStruct->battlerState[i].originalBattlerPartyId = PARTY_SIZE;
    }
    ModsBattleBegin();
    struct AiCalcValues aiMove = {.move = MOVE_TACKLE};
    CHECK(AI_CalcDamage(&aiMove, 0, 1).median > 0);
    /* A bounced-move cleanup restores the original attacker first and must not notify yet. */
    gCurrentMove = gChosenMove = MOVE_TACKLE; gBattlerAttacker = 1; gBattlerTarget = 0;
    gBattleStruct->bouncedMoveIsUsed = TRUE; gBattleStruct->savedAttackerCount = gBattleStruct->savedTargetCount = 1;
    gBattleStruct->savedBattlerAttacker[0] = 0; gBattleStruct->savedBattlerTarget[0] = 1;
    gBattleScripting.moveendState = MOVEEND_CLEAR_BITS;
    CHECK(DoMoveEnd(1, MOVEEND_CLEAR_BITS) == MOVEEND_RESULT_RUN_SCRIPT && !gBattleStruct->bouncedMoveIsUsed);
    gBattleStruct->moveResultFlags[1] = MOVE_RESULT_SUPER_EFFECTIVE; gBattleStruct->moveDamage[1] = 12;
    rng_value_t rng = gRngValue; FinishTestMove(MOVE_TACKLE, 1);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !gBattleStruct->moveResultFlags[1] && !gBattleStruct->moveDamage[1]);
    CHECK(moves->resolution_context(&outside) && !memcmp(&outside, &unchanged, sizeof(outside)));

    /* The real attack canceler marks an unusable move; its cleanup carries that flag. */
    gCurrentMove = gChosenMove = MOVE_SCRATCH; gBattlerAttacker = 0; gBattlerTarget = 1;
    gBattleMons[0].volatiles.rechargeTimer = 1; gBattleStruct->eventState.atkCanceler = CANCELER_RECHARGE;
    CHECK(DoAttackCanceler() == CANCELER_RESULT_FAILURE && gBattleStruct->unableToUseMove);
    FinishTestMove(MOVE_SCRATCH, 1); CHECK(!gBattleStruct->unableToUseMove);

    /* Spread processing switches to the second target before the final notification. */
    gCurrentMove = gChosenMove = MOVE_SURF; gBattlerAttacker = 0; gBattlerTarget = 1;
    gBattleStruct->battlerState[0].targetsDone[0] = gBattleStruct->battlerState[0].targetsDone[2] = TRUE;
    gBattleScripting.moveendState = MOVEEND_NEXT_TARGET;
    CHECK(DoMoveEnd(1, MOVEEND_NEXT_TARGET) == MOVEEND_RESULT_BREAK && gBattlerTarget == 3);
    gBattleStruct->moveResultFlags[1] = MOVE_RESULT_MISSED;
    gBattleStruct->moveResultFlags[2] = MOVE_RESULT_STAT_CHANGED | MOVE_RESULT_STAT_CHANGE_PREVENTED
        | MOVE_RESULT_EXTREMELY_EFFECTIVE | MOVE_RESULT_MOSTLY_INEFFECTIVE | MOVE_RESULT_FAILED | MOVE_RESULT_NOT_PRESENT;
    gBattleStruct->moveResultFlags[3] = MOVE_RESULT_PROTECTED | MOVE_RESULT_DOESNT_AFFECT_FOE;
    gBattleStruct->moveDamage[3] = -8; FinishTestMove(MOVE_SURF, 3);
    gProtectStructs[0].chargingTurn = TRUE; gBattleStruct->battlerState[0].originalBattlerPartyId = 1;
    FinishTestMove(MOVE_SOLAR_BEAM, 1); gProtectStructs[0].chargingTurn = FALSE;

    /* These modes must neither dispatch nor consume a move sequence number. */
    gBattleTypeFlags |= BATTLE_TYPE_LINK; FinishTestMove(MOVE_POUND, 1);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; gBattleTypeFlags |= BATTLE_TYPE_RECORDED; FinishTestMove(MOVE_POUND, 1);
    gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED;
    gCurrentMove = MOVE_POUND; gChosenMove = MOVE_TACKLE; gBattlerAttacker = 0; gBattlerTarget = 1;
    gBattleScripting.moveendState = MOVEEND_CLEAR_BITS;
    CHECK(DoMoveEnd(1, MOVEEND_CLEAR_BITS) == MOVEEND_RESULT_CONTINUE);
    if (custom) FinishTestMove(customSlot, 1);
    ModsBattleReset(); FinishTestMove(MOVE_POUND, 1); /* No event after battle reset. */
    ModsBattleBegin(); FinishTestMove(MOVE_GROWL, 1);
    ModsBattleReset(); gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("move resolution: cleanup, interruption, spread and exclusions passed");
}

