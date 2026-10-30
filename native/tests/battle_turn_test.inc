#include "battle_end_turn.h"
#include "battle_message.h"
#include "constants/battle_end_turn.h"

static bool32 TextEqualsAscii(const u8 *encoded, const char *ascii);
static void FinishTurnWithTwoExpirations(u32 turn, RhhContentKey ward, RhhContentKey mist)
{
    gBattleTurnCounter = turn;
    gBattleStruct->eventState.endTurn = ENDTURN_COUNT;
    gBattleStruct->eventState.endTurnBattler = gBattleStruct->eventState.battlerSide = 0;
    gBattlescriptCurrInstr = BattleScript_EndTurnEvents; gBattleControllerExecFlags = 0;
    CHECK(DoEndTurnEffects() && gBattlescriptCurrInstr == BattleScript_ModConditionMessages);
    for (u32 i = 0; i < 2; ++i)
    {
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_WAITMESSAGE && gBattleControllerExecFlags);
        u32 battler = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
        BufferStringBattle(STRINGID_MOD_EFFECT_MESSAGE, battler);
        bool32 mistFirst = mist < ward;
        CHECK(TextEqualsAscii(gDisplayedStringBattle, (i == 0) == mistFirst ? "Mist expired." : "Ward expired."));
        gBattleControllerExecFlags = 0; gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[B_SCR_OP_WAITMESSAGE]();
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
        gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModConditionMessagesEnd);
    gBattleScriptingCommandsTable[B_SCR_OP_RETURN]();
    CHECK(gBattlescriptCurrInstr == BattleScript_EndTurnEvents && !DoEndTurnEffects());
    CHECK(ModsConsumeEndTurnMessageResume() && !ModsConsumeEndTurnMessageResume());
}

static void FinishTestTurn(u32 turn)
{
    gBattleTurnCounter = turn;
    gBattleStruct->eventState.endTurn = ENDTURN_COUNT;
    gBattleStruct->eventState.endTurnBattler = 0;
    gBattleStruct->eventState.battlerSide = 0;
    CHECK(!DoEndTurnEffects());
}

static void PrepareTurnBattle(void)
{
    memset(&sBattle, 0, sizeof(sBattle)); gBattleStruct = &sBattle;
    gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE;
    gBattlersCount = 4; gAbsentBattlerFlags = 0; gBattleOutcome = 0; gBattleTurnCounter = 0;
    for (u32 side = 0; side < 2; ++side)
    {
        CreateMonWithIVs(&gParties[side][0], side ? SPECIES_CHARIZARD : SPECIES_TORCHIC,
                         20, 9700 + side, OTID_STRUCT_PRESET(54321 + side), 31);
        gParties[side][1] = gParties[side][0]; gPartiesCount[side] = 2;
    }
    for (u32 i = 0; i < 4; ++i)
    {
        gBattlerPositions[i] = i; gBattlerByTurnOrder[i] = i; gBattlerPartyIndexes[i] = i / 2;
        PokemonToBattleMon(&gParties[i & 1][i / 2], &gBattleMons[i]);
        gBattleStruct->battlerState[i].originalBattlerPartyId = PARTY_SIZE;
    }
}

static void CheckBattleTurn(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    struct BattleResources resources = {0}; gBattleResources = &resources;
    PrepareTurnBattle();
    const struct RhhBattleService *battle = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 2));
    CHECK(battle && battle->header.struct_size >= RHH_BATTLE_SERVICE_V1_2_SIZE);
    struct RhhBattleTurnContext outside = {.struct_size = sizeof(outside), .sequence = 999}, unchanged = outside;
    CHECK(battle->turn_context(&outside) && !memcmp(&outside, &unchanged, sizeof(outside)));
    ModsBattleBegin(); FinishTestTurn(1); FinishTestTurn(1); FinishTestTurn(2);
    gBattleOutcome = B_OUTCOME_WON; FinishTestTurn(3); gBattleOutcome = 0;
    gBattleTypeFlags |= BATTLE_TYPE_LINK; FinishTestTurn(4); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    gBattleTypeFlags |= BATTLE_TYPE_RECORDED; FinishTestTurn(5); gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED;
    FinishTestTurn(6);
    CHECK(battle->turn_context(&outside) && !memcmp(&outside, &unchanged, sizeof(outside)));
    ModsBattleReset(); FinishTestTurn(7); ModsBattleBegin(); FinishTestTurn(1);
    ModsBattleReset(); gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("battle turn: completion, exclusions, context and reset passed");
}

static u32 ConditionTurns(const struct RhhBattleService *battle, RhhContentKey key, u32 target)
{
    struct RhhBattleConditionState state = {.struct_size = sizeof(state)};
    CHECK(!battle->condition_state(key, target, &state));
    CHECK(state.key == key && state.target == target && state.battle_id);
    return state.remaining_turns;
}
