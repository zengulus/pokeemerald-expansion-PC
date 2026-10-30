#include "battle_switch_in.h"

static u32 sEntryHealsApplied[MAX_BATTLERS_COUNT];
static bool32 sEntryBusyStatTest;
static void EntryHealControllerComplete(enum BattlerId battler)
{ (void)battler; gBattleControllerExecFlags = 0; }
static void RunInnateEntrySwitchEvents(u32 enteringMask)
{
    gBattleStruct->eventState.switchIn = SWITCH_IN_EVENTS_ORDER_BY_SPEED;
    gBattleStruct->eventState.battlerSwitchIn = 0; gBattleStruct->switchInBattlerCounter = 0;
    for (u32 i = 0; i < gBattlersCount; ++i) gBattleStruct->battlerState[i].switchIn = !!(enteringMask & (1u << i));
    gBattlescriptCurrInstr = BattleScript_FirstTurnSwitchInEvents;
    u32 steps = 0;
    while (gBattleStruct->eventState.switchIn < SWITCH_IN_EVENTS_COUNT)
    {
        bool32 effect = DoSwitchInEvents(); CHECK(++steps < 32);
        if (!effect) continue;
        CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateHeal);
        gBattleControllerExecFlags = 0;
        u32 savedScriptingBattler = gBattleScripting.battler;
        for (;;)
        {
            gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
            if (gBattlescriptCurrInstr == BattleScript_ModInnateStat) break;
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_HEALTHBARUPDATE);
            u32 battler = gBattleScripting.battler;
            CHECK(battler < gBattlersCount && gBattleStruct->passiveHpUpdate[battler] < 0);
            CHECK(-gBattleStruct->passiveHpUpdate[battler] == max(1, GetNonDynamaxMaxHP(battler)
                / (sEntryHealsApplied[battler] ? 16 : 8)));
            u32 expected = min(gBattleMons[battler].maxHP, gBattleMons[battler].hp - gBattleStruct->passiveHpUpdate[battler]);
            gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE]();
            CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[battler][0] == CONTROLLER_HEALTHBARUPDATE);
            gBattleControllerExecFlags = 0;
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_DATAHPUPDATE);
            gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
            CHECK(gBattleControllerExecFlags && gBattleMons[battler].hp == expected
                && gBattleResources->bufferA[battler][0] == CONTROLLER_SETMONDATA);
            BtlController_HandleSetMonData(battler);
            CHECK(!gBattleControllerExecFlags && GetMonData(GetBattlerMon(battler), MON_DATA_HP) == expected);
            ++sEntryHealsApplied[battler];
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
            gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
        }
        CHECK(gBattleScripting.battler == savedScriptingBattler);
        u32 statActions = 0;
        if (sEntryBusyStatTest) gSpecialStatuses[1].statStageAmount2 = 1;
        for (;;)
        {
            CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateStat);
            gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
            if (gBattlescriptCurrInstr == BattleScript_ModInnateMessages) break;
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_TRYBATTLERSTATCHANGE && ++statActions <= 8);
            for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModInnateStat; ++step)
            {
                CHECK(step < 4096);
                if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
                u32 opcode = *gBattlescriptCurrInstr;
                CHECK(opcode != B_SCR_OP_END);
                if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
                gBattleScriptingCommandsTable[opcode]();
            }
        }
        CHECK(statActions == (sEntryBusyStatTest ? 0 : 8) && gBattleScripting.battler == savedScriptingBattler);
        if (sEntryBusyStatTest)
        {
            CHECK(gSpecialStatuses[1].statStageAmount2 == 1);
            gSpecialStatuses[1].statStageAmount2 = 0;
            sEntryBusyStatTest = FALSE;
        }
        for (u32 i = 0; i < 8; ++i)
        {
            gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_WAITMESSAGE && gBattleControllerExecFlags);
            BufferStringBattle(STRINGID_MOD_EFFECT_MESSAGE, GetBattlerAtPosition(B_POSITION_PLAYER_LEFT));
            CHECK(TextEqualsAscii(gDisplayedStringBattle, i ? "Observer message." : "Entry Ward protects the team!"));
            gBattleControllerExecFlags = 0; gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
            gBattleScriptingCommandsTable[B_SCR_OP_WAITMESSAGE]();
            CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
            gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
        }
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateMessagesEnd);
        gBattleScriptingCommandsTable[B_SCR_OP_RETURN]();
        CHECK(gBattlescriptCurrInstr == BattleScript_FirstTurnSwitchInEvents);
    }
}
static void CheckInnateEntryTarget(const char *module)
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
    PrepareTurnBattle(); gBattleMons[3].hp = 0;
    for (u32 i = 0; i < MAX_BATTLERS_COUNT; ++i) gBattlerControllerEndFuncs[i] = EntryHealControllerComplete;
    const struct RhhAbilityService *abilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 17));
    CHECK(abilities && abilities->header.struct_size >= RHH_ABILITY_SERVICE_V1_17_SIZE
        && abilities->innate_entry_heal_target(2, 1, 4) && abilities->innate_entry_stat_target(2, RHH_STAT_DEFENSE, 1));
    u16 hp = 1; gBattleMons[2].hp = hp; SetMonData(GetBattlerMon(2), MON_DATA_HP, &hp);
    for (u32 i = RHH_STAT_ATTACK; i < RHH_BATTLE_STAT_COUNT; ++i) gBattleMons[2].statStages[i] = DEFAULT_STAT_STAGE;
    ModsBattleBegin(); CHECK(ModsInnateSwitchIn(0));
    u32 oldScripting = gBattleScripting.battler;
    gBattlescriptCurrInstr = BattleScript_ModInnateHeal; gBattleControllerExecFlags = 0;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_HEALTHBARUPDATE && gBattleScripting.battler == 2);
    gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE](); CHECK(gBattleControllerExecFlags);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
    CHECK(gBattleControllerExecFlags && gBattleMons[2].hp > 1); BtlController_HandleSetMonData(2);
    CHECK(!gBattleControllerExecFlags && GetMonData(GetBattlerMon(2), MON_DATA_HP) == gBattleMons[2].hp);
    gBattleScriptingCommandsTable[B_SCR_OP_GOTO](); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateStat);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_TRYBATTLERSTATCHANGE && gBattleScripting.battler == 2);
    for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModInnateStat; ++step)
    {
        CHECK(step < 4096);
        if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
        u32 opcode = *gBattlescriptCurrInstr; CHECK(opcode != B_SCR_OP_END);
        if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[opcode]();
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateMessages && gBattleScripting.battler == oldScripting
        && gBattleMons[2].statStages[STAT_DEF] == DEFAULT_STAT_STAGE + 1);
    ModsBattleReset(); ModsBattleBegin(); CHECK(ModsInnateSwitchIn(0));
    gBattlerPartyIndexes[2] = 0; CHECK(!ModsInnateNextHeal() && !ModsInnateNextStat()); gBattlerPartyIndexes[2] = 1;
    ModsBattleReset(); ModsBattleBegin(); CHECK(ModsInnateSwitchIn(0));
    gBattlerPartyIndexes[0] = 1; CHECK(!ModsInnateNextHeal() && !ModsInnateNextStat()); gBattlerPartyIndexes[0] = 0;
    ModsBattleReset(); ModsBattleBegin(); CHECK(ModsInnateSwitchIn(0));
    gBattleMons[2].hp = 0; CHECK(!ModsInnateNextHeal() && !ModsInnateNextStat()); gBattleMons[2].hp = 1;
    ModsBattleReset(); ModsBattleBegin(); CHECK(ModsInnateSwitchIn(0));
    gBattleMons[0].hp = 0; CHECK(!ModsInnateNextHeal() && !ModsInnateNextStat()); gBattleMons[0].hp = 1;
    ModsBattleReset(); gBattleTurnCounter = 99; ModsBattleBegin(); CHECK(ModsInnateSwitchIn(0));
    ModsBattleReset(); gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("innate entry target: execution and participant revalidation passed");
}
static void SendInnateEntryMon(struct BattleResources *resources, u32 slot)
{
    struct BattlePokemon next; PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][slot], &next);
    memcpy(&resources->bufferB[0][4], &next, BATTLE_POKEMON_TRANSFER_SIZE);
    gBattlerPartyIndexes[0] = slot; gBattleControllerExecFlags = 0; gCurrentMove = MOVE_TACKLE;
    const u8 update[] = {B_SCR_OP_SWITCHINDATAUPDATE, BS_BATTLER_0};
    gBattlescriptCurrInstr = update; gBattleScriptingCommandsTable[B_SCR_OP_SWITCHINDATAUPDATE]();
    const u8 animation[] = {B_SCR_OP_SWITCHINANIM, BS_BATTLER_0, 0, 0};
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = animation;
    gBattleScriptingCommandsTable[B_SCR_OP_SWITCHINANIM](); gBattleControllerExecFlags = 0;
}
static u32 EntryWardTurns(const struct RhhBattleService *battle, RhhContentKey key)
{
    struct RhhBattleConditionState state = {.struct_size = sizeof(state)};
    CHECK(!battle->condition_state(key, RHH_CONDITION_PLAYER_SIDE, &state));
    return state.remaining_turns;
}
