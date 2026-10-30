static const struct RhhBattleFieldRulesService *sTestFieldRules;
#include "battle_end_turn.h"
#include "battle_set_effect.h"
#include "battle_scripts.h"
#include "battle_script_commands.h"
#include "constants/battle_end_turn.h"
#include "constants/battle_set_effect.h"
static u32 sTestFieldObservations, sTestFieldCommits;
static struct RhhBattleFieldRulesCommit sLastTestFieldCommit;
extern void BS_TrySetFairyLock(void);
static void ObserveTestFieldRules(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleFieldRulesObservation change = {.struct_size = sizeof(change)};
    struct RhhBattleFieldRulesObservation tiny = {.struct_size = 1, .active_changed = 99}, saved = tiny;
    CHECK(!RhhModsCommandsAllowed() && !sTestFieldRules->observation(&change)
        && sTestFieldRules->observation(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny))
        && change.sequence == sTestFieldObservations && event->ordinal == change.sequence);
    ++sTestFieldObservations; RhhModsObserveBattleEnvironment(); (void)Random(); (void)Random2();
}
static void ObserveTestFieldCommit(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleFieldRulesCommit commit = {.struct_size = sizeof(commit)};
    struct RhhBattleFieldRulesCommit tiny = {.struct_size = 1, .cause = 99}, saved = tiny;
    CHECK(!RhhModsCommandsAllowed() && !sTestFieldRules->commit(&commit)
        && sTestFieldRules->commit(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny))
        && commit.sequence == sTestFieldCommits && event->ordinal == commit.sequence
        && commit.active_changed == (commit.before.active ^ commit.after.active));
    sLastTestFieldCommit = commit; ++sTestFieldCommits; (void)Random(); (void)Random2();
}
static const struct RhhModHook sTestFieldHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_FIELD_RULES_OBSERVED, RHH_OBSERVE, 0, NULL, ObserveTestFieldRules},
    {sizeof(struct RhhModHook), RHH_BATTLE_FIELD_RULES_COMMITTED, RHH_OBSERVE, 0, NULL, ObserveTestFieldCommit}
};
static const struct RhhModDefinition sTestFieldMod = {.struct_size = sizeof(sTestFieldMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:battle_field_rng", .hook_count = ARRAY_COUNT(sTestFieldHooks), .hooks = sTestFieldHooks};
static void CheckBattleFieldRules(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sTestFieldMod) && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhBattleFieldRulesService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 0));
    CHECK(service && service->header.struct_size >= RHH_BATTLE_FIELD_RULES_SERVICE_V1_2_SIZE
        && RHH_BATTLE_FIELD_RULES_SERVICE_V1_0_SIZE < RHH_BATTLE_FIELD_RULES_SERVICE_V1_1_SIZE
        && RHH_BATTLE_FIELD_RULES_SERVICE_V1_1_SIZE < RHH_BATTLE_FIELD_RULES_SERVICE_V1_2_SIZE
        && RHH_BATTLE_FIELD_RULES_SERVICE_V1_3_SIZE < RHH_BATTLE_FIELD_RULES_SERVICE_V1_4_SIZE
        && RHH_BATTLE_FIELD_RULES_SERVICE_V1_4_SIZE < RHH_BATTLE_FIELD_RULES_SERVICE_V1_5_SIZE
        && RHH_BATTLE_FIELD_RULES_SERVICE_V1_5_SIZE < RHH_BATTLE_FIELD_RULES_SERVICE_V1_6_SIZE
        && RHH_BATTLE_FIELD_RULES_SERVICE_V1_6_SIZE < RHH_BATTLE_FIELD_RULES_SERVICE_V1_7_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 1)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 2)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 3)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 4)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 5)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 6)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 7)) == &service->header
        && !RhhModsHostAPI()->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 8)));
    sTestFieldRules = service; sTestFieldObservations = sTestFieldCommits = 0;
    struct RhhBattleFieldRulesCommit outsideCommit = {.struct_size = sizeof(outsideCommit), .cause = 99}, oldCommit = outsideCommit;
    CHECK(service->commit(&outsideCommit) && !memcmp(&outsideCommit, &oldCommit, sizeof(outsideCommit)));
    struct RhhBattleFieldRulesObservation outside = {.struct_size = sizeof(outside), .active_changed = 99}, oldOutside = outside;
    CHECK(service->observation(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    struct RhhBattleFieldRules result = {.struct_size = sizeof(result), .active = 999}, old = result;
    struct RhhBattleFieldRules small = {.struct_size = 1, .active = 999}, oldSmall = small;
    struct RhhBattleSports sports = {.struct_size = sizeof(sports), .effective = 999}, oldSports = sports;
    struct RhhBattleSports smallSports = {.struct_size = 1, .effective = 999}, oldSmallSports = smallSports;
    struct RhhBattleSportsObservation sportObservation = {.struct_size = sizeof(sportObservation), .changed = 999};
    struct RhhBattleSportsObservation oldSportObservation = sportObservation;
    CHECK(service->sports(&sports) && !memcmp(&sports, &oldSports, sizeof(sports)) && service->sports(NULL)
        && service->sports_observation(&sportObservation) && !memcmp(&sportObservation, &oldSportObservation, sizeof(sportObservation)));
    CHECK(service->current(&result) && !memcmp(&result, &old, sizeof(result)) && service->current(NULL));
    struct BattleScriptsStack stack = {0}; struct BattleCallbacksStack callbacks = {0};
    struct BattleResources resources = {.battleScriptsStack = &stack, .battleCallbackStack = &callbacks}; struct AiLogicData ai = {0};
    gBattleResources = &resources; gAiLogicData = &ai; PrepareTurnBattle();
    gFieldStatuses = STATUS_FIELD_GRAVITY | STATUS_FIELD_TRICK_ROOM;
    memset(&gFieldTimers, 0, sizeof(gFieldTimers)); gFieldTimers.gravityTimer = 3; gFieldTimers.trickRoomTimer = 2;
    gBattleTurnCounter = 7; rng_value_t rng = gRngValue, rng2 = gRng2Value;
    ModsBattleBegin();
    CHECK(service->current(&small) && !memcmp(&small, &oldSmall, sizeof(small))
        && !service->current(&result) && result.active == (RHH_BATTLE_FIELD_GRAVITY | RHH_BATTLE_FIELD_TRICK_ROOM)
        && result.gravity_turns == 3 && result.trick_room_turns == 2 && result.turn == 7
        && result.run_id == 456 && result.battle_id && !result.context_flags && !result.reserved);
    CHECK(service->sports(&smallSports) && !memcmp(&smallSports, &oldSmallSports, sizeof(smallSports))
        && !service->sports(&sports) && sports.mode == RHH_BATTLE_SPORT_FIELD_TIMER && sports.battler_count == gBattlersCount
        && !sports.field_active && !sports.effective && sports.run_id == result.run_id && sports.battle_id == result.battle_id);
    gBattleMons[0].volatiles.mudSport = TRUE; gBattleMons[1].volatiles.waterSport = TRUE;
    gFieldStatuses |= STATUS_FIELD_WATERSPORT; gFieldTimers.waterSportTimer = 4;
    CHECK(!service->sports(&sports) && sports.mud_battlers == 1 && sports.water_battlers == 2
        && sports.field_active == RHH_BATTLE_FIELD_WATER_SPORT && sports.effective == RHH_BATTLE_FIELD_WATER_SPORT
        && sports.water_turns == 4 && !sports.mud_turns);
    gBattleMons[0].volatiles.mudSport = FALSE; gBattleMons[1].volatiles.waterSport = FALSE;
    gFieldStatuses &= ~STATUS_FIELD_WATERSPORT; gFieldTimers.waterSportTimer = 0;
    RhhModsObserveBattleEnvironment(); CHECK(!sTestFieldObservations);
    gFieldStatuses |= STATUS_FIELD_MAGIC_ROOM; gFieldTimers.magicRoomTimer = 5;
    RhhModsObserveBattleEnvironment(); CHECK(sTestFieldObservations == 1);
    ai.aiCalcInProgress = TRUE; gFieldTimers.gravityTimer = 1; RhhModsObserveBattleEnvironment();
    CHECK(sTestFieldObservations == 1); gFieldTimers.gravityTimer = 3; ai.aiCalcInProgress = FALSE;
    gFieldStatuses &= ~STATUS_FIELD_MAGIC_ROOM; gFieldStatuses |= STATUS_FIELD_MAGIC_ROOM;
    RhhModsObserveBattleEnvironment(); CHECK(sTestFieldObservations == 1);
    gFieldTimers.gravityTimer = 2; RhhModsObserveBattleEnvironment(); CHECK(sTestFieldObservations == 2);
    CHECK(service->observation(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    gFieldStatuses = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers)); gBattlerAttacker = 0;
    static const u8 roomCommand[] = {B_SCR_OP_SETROOM};
    gCurrentMove = MOVE_TRICK_ROOM; gBattlescriptCurrInstr = roomCommand;
    gBattleScriptingCommandsTable[B_SCR_OP_SETROOM](); CHECK(sTestFieldCommits == 1 && (gFieldStatuses & STATUS_FIELD_TRICK_ROOM));
    gBattlescriptCurrInstr = roomCommand; gBattleScriptingCommandsTable[B_SCR_OP_SETROOM]();
    CHECK(sTestFieldCommits == 2 && !(gFieldStatuses & STATUS_FIELD_TRICK_ROOM));
    struct __attribute__((packed)) { u8 opcode; const u8 *fail; } gravityCommand = {B_SCR_OP_SETGRAVITY, roomCommand};
    gCurrentMove = MOVE_GRAVITY; gBattlescriptCurrInstr = (const u8 *)&gravityCommand;
    gBattleScriptingCommandsTable[B_SCR_OP_SETGRAVITY](); CHECK(sTestFieldCommits == 3 && gFieldTimers.gravityTimer == 5);
    gBattlescriptCurrInstr = (const u8 *)&gravityCommand; gBattleScriptingCommandsTable[B_SCR_OP_SETGRAVITY](); CHECK(sTestFieldCommits == 3);
    gFieldTimers.gravityTimer = 1; gBattleStruct->eventState.endTurn = ENDTURN_GRAVITY;
    gBattleStruct->eventState.endTurnBattler = gBattleStruct->eventState.battlerSide = 0;
    CHECK(DoEndTurnEffects() && sTestFieldCommits == 4 && !(gFieldStatuses & STATUS_FIELD_GRAVITY));
    struct BattleCalcValues calc = {.battlerAtk = 0, .battlerDef = 0, .move = MOVE_ION_DELUGE};
    struct SetEffect effect = {.script = roomCommand, .moveEffect = MOVE_EFFECT_ION_DELUGE, .effectBattler = 0, .primary = TRUE};
    SetMoveEffect(&calc, &effect); CHECK(sTestFieldCommits == 5 && (gFieldStatuses & STATUS_FIELD_ION_DELUGE));
    SetMoveEffect(&calc, &effect); CHECK(sTestFieldCommits == 5);
    calc.move = MOVE_GRAVITY; effect.moveEffect = MOVE_EFFECT_GRAVITY;
    SetMoveEffect(&calc, &effect); CHECK(sTestFieldCommits == 6 && (gFieldStatuses & STATUS_FIELD_GRAVITY));
    ai.aiCalcInProgress = TRUE; gFieldStatuses &= ~STATUS_FIELD_GRAVITY;
    SetMoveEffect(&calc, &effect); CHECK(sTestFieldCommits == 6); ai.aiCalcInProgress = FALSE;
    gFieldStatuses = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers));
    struct __attribute__((packed)) { u8 opcode; const u8 *fail; } sportCommand = {B_SCR_OP_SETTYPEBASEDHALVERS, roomCommand};
    gCurrentMove = MOVE_MUD_SPORT; gBattlerAttacker = 0; gBattlescriptCurrInstr = (const u8 *)&sportCommand;
    gBattleScriptingCommandsTable[B_SCR_OP_SETTYPEBASEDHALVERS]();
    CHECK(sTestFieldCommits == 7 && sLastTestFieldCommit.active_changed == RHH_BATTLE_FIELD_MUD_SPORT
        && sLastTestFieldCommit.after.mud_sport_turns == 5 && sLastTestFieldCommit.cause == RHH_BATTLE_FIELD_CAUSE_MOVE);
    gBattlescriptCurrInstr = (const u8 *)&sportCommand; gBattleScriptingCommandsTable[B_SCR_OP_SETTYPEBASEDHALVERS](); CHECK(sTestFieldCommits == 7);
    gCurrentMove = MOVE_WATER_SPORT; gBattlescriptCurrInstr = (const u8 *)&sportCommand;
    gBattleScriptingCommandsTable[B_SCR_OP_SETTYPEBASEDHALVERS]();
    CHECK(sTestFieldCommits == 8 && sLastTestFieldCommit.active_changed == RHH_BATTLE_FIELD_WATER_SPORT
        && sLastTestFieldCommit.after.water_sport_turns == 5);
    struct __attribute__((packed)) { u8 opcode; void (*func)(void); const u8 *fail; } fairyCommand = {0, BS_TrySetFairyLock, roomCommand};
    gCurrentMove = MOVE_FAIRY_LOCK; gBattlescriptCurrInstr = (const u8 *)&fairyCommand; BS_TrySetFairyLock();
    CHECK(sTestFieldCommits == 9 && sLastTestFieldCommit.active_changed == RHH_BATTLE_FIELD_FAIRY_LOCK
        && sLastTestFieldCommit.after.fairy_lock_turns == 2);
    gBattlescriptCurrInstr = (const u8 *)&fairyCommand; BS_TrySetFairyLock(); CHECK(sTestFieldCommits == 9);
    memset(&gStartingStatuses, 0, sizeof(gStartingStatuses));
    gStartingStatuses.trickRoom = TRUE; gStartingStatuses.magicRoomTemporary = TRUE; gStartingStatuses.wonderRoomTemporary = TRUE;
    const u32 starting[] = {RHH_BATTLE_FIELD_TRICK_ROOM, RHH_BATTLE_FIELD_MAGIC_ROOM, RHH_BATTLE_FIELD_WONDER_ROOM};
    for (u32 i = 0; i < ARRAY_COUNT(starting); ++i)
    {
        stack.size = callbacks.size = 0;
        CHECK(TryFieldEffects(FIELD_EFFECT_TRAINER_STATUSES) && sTestFieldCommits == 10 + i
            && sLastTestFieldCommit.active_changed == starting[i] && sLastTestFieldCommit.cause == RHH_BATTLE_FIELD_CAUSE_STARTING
            && !sLastTestFieldCommit.move && sLastTestFieldCommit.source_battler == RHH_BATTLE_NO_BATTLER);
    }
    CHECK(!sLastTestFieldCommit.after.trick_room_turns && sLastTestFieldCommit.after.magic_room_turns == 5
        && sLastTestFieldCommit.after.wonder_room_turns == 5);
    gFieldStatuses |= STATUS_FIELD_GRAVITY;
    gFieldTimers.trickRoomTimer = gFieldTimers.gravityTimer = gFieldTimers.waterSportTimer = 1;
    gFieldTimers.mudSportTimer = gFieldTimers.wonderRoomTimer = gFieldTimers.magicRoomTimer = 1;
    const u32 expirySteps[] = {ENDTURN_TRICK_ROOM, ENDTURN_GRAVITY, ENDTURN_WATER_SPORT,
        ENDTURN_MUD_SPORT, ENDTURN_WONDER_ROOM, ENDTURN_MAGIC_ROOM};
    const u32 expiryFlags[] = {RHH_BATTLE_FIELD_TRICK_ROOM, RHH_BATTLE_FIELD_GRAVITY, RHH_BATTLE_FIELD_WATER_SPORT,
        RHH_BATTLE_FIELD_MUD_SPORT, RHH_BATTLE_FIELD_WONDER_ROOM, RHH_BATTLE_FIELD_MAGIC_ROOM};
    for (u32 i = 0; i < ARRAY_COUNT(expirySteps); ++i)
    {
        stack.size = callbacks.size = 0; gBattleStruct->eventState.endTurn = expirySteps[i];
        gBattleStruct->eventState.endTurnBattler = gBattleStruct->eventState.battlerSide = 0;
        CHECK(DoEndTurnEffects() && sTestFieldCommits == 13 + i && sLastTestFieldCommit.active_changed == expiryFlags[i]
            && sLastTestFieldCommit.timer_changed == expiryFlags[i] && sLastTestFieldCommit.cause == RHH_BATTLE_FIELD_CAUSE_EXPIRED
            && !sLastTestFieldCommit.move && sLastTestFieldCommit.source_battler == RHH_BATTLE_NO_BATTLER);
    }
    CHECK(service->commit(&outsideCommit) && !memcmp(&outsideCommit, &oldCommit, sizeof(outsideCommit)));
    const u32 engine[] = {STATUS_FIELD_MAGIC_ROOM, STATUS_FIELD_TRICK_ROOM, STATUS_FIELD_WONDER_ROOM,
        STATUS_FIELD_MUDSPORT, STATUS_FIELD_WATERSPORT, STATUS_FIELD_GRAVITY, STATUS_FIELD_ION_DELUGE, STATUS_FIELD_FAIRY_LOCK};
    const u32 semantic[] = {RHH_BATTLE_FIELD_MAGIC_ROOM, RHH_BATTLE_FIELD_TRICK_ROOM, RHH_BATTLE_FIELD_WONDER_ROOM,
        RHH_BATTLE_FIELD_MUD_SPORT, RHH_BATTLE_FIELD_WATER_SPORT, RHH_BATTLE_FIELD_GRAVITY,
        RHH_BATTLE_FIELD_ION_DELUGE, RHH_BATTLE_FIELD_FAIRY_LOCK};
    for (u32 i = 0; i < ARRAY_COUNT(engine); ++i)
    {
        gFieldStatuses = engine[i];
        CHECK(!service->current(&result) && result.active == semantic[i]);
    }
    gFieldStatuses = UINT32_MAX;
    gFieldTimers = (struct FieldTimer){.magicRoomTimer = 1, .trickRoomTimer = 2, .wonderRoomTimer = 3,
        .mudSportTimer = 4, .waterSportTimer = 5, .gravityTimer = 6, .fairyLockTimer = 7};
    struct FieldTimer beforeTimers = gFieldTimers;
    ai.aiCalcInProgress = TRUE; ai.aiPredictionInProgress = TRUE; ai.switchInCalc = TRUE;
    CHECK(!service->current(&result) && result.active == 255 && result.context_flags == 7
        && result.magic_room_turns == 1 && result.trick_room_turns == 2 && result.wonder_room_turns == 3
        && result.mud_sport_turns == 4 && result.water_sport_turns == 5 && result.gravity_turns == 6
        && result.fairy_lock_turns == 7 && !memcmp(&beforeTimers, &gFieldTimers, sizeof(beforeTimers)));
    gFieldStatuses = 0; CHECK(!service->current(&result) && !result.active && result.gravity_turns == 6);
    old = result; gBattleTypeFlags |= BATTLE_TYPE_LINK; RhhModsObserveBattleEnvironment();
    CHECK(service->current(&result) && !memcmp(&result, &old, sizeof(result)));
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; gBattleTypeFlags |= BATTLE_TYPE_RECORDED;
    CHECK(service->current(&result) && !memcmp(&result, &old, sizeof(result)));
    gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED; gBattleOutcome = B_OUTCOME_WON;
    CHECK(service->current(&result) && !memcmp(&result, &old, sizeof(result)));
    gBattleOutcome = 0; ModsBattleReset();
    CHECK(service->current(&result) && !memcmp(&result, &old, sizeof(result))
        && sTestFieldObservations == 2 && sTestFieldCommits == 18
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gMain.inBattle = FALSE; gAiLogicData = NULL; gBattleResources = NULL; RhhModsUnloadLibraries();
    puts("battle field rules: flags, counters, contexts and exclusions passed");
}
static void BeginFieldEffect(const u8 *script)
{
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = script + 3;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction);
}
static void CheckFieldRuleEffect(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sTestFieldMod)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sTestFieldRules = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 6));
    CHECK(sTestFieldRules && sTestFieldRules->header.struct_size >= RHH_BATTLE_FIELD_RULES_SERVICE_V1_6_SIZE
        && sTestFieldRules->queue_move_effect(RHH_BATTLE_FIELD_GRAVITY)
        && sTestFieldRules->offer_ai_move_effect(RHH_BATTLE_FIELD_GRAVITY, 100) == RHH_AI_EFFECT_UNAVAILABLE
        && sTestFieldRules->queue_room_toggle(RHH_BATTLE_FIELD_TRICK_ROOM)
        && sTestFieldRules->offer_ai_room_toggle(RHH_BATTLE_FIELD_TRICK_ROOM, 100) == RHH_AI_EFFECT_UNAVAILABLE
        && sTestFieldRules->queue_field_move(RHH_BATTLE_FIELD_MUD_SPORT)
        && sTestFieldRules->offer_ai_field_move(RHH_BATTLE_FIELD_MUD_SPORT, 100) == RHH_AI_EFFECT_UNAVAILABLE);
    u8 *mode = RhhModsState("test:field_rule_effect"); CHECK(mode);
    struct BattleScriptsStack stack = {0}; struct BattleResources resources = {.battleScriptsStack = &stack};
    struct AiLogicData ai = {0}; gBattleResources = &resources; gAiLogicData = &ai;
    PrepareTurnBattle(); gFieldStatuses = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers));
    sTestFieldObservations = sTestFieldCommits = 0; ModsBattleBegin();
    gBattlerAttacker = gBattlerTarget = 0; gCurrentMove = gChosenMove = MOVE_RAIN_DANCE;
    const u8 *script = GetMoveBattleScript(MOVE_RAIN_DANCE); CHECK(script == BattleScript_EffectModCustomStatus);
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    BeginFieldEffect(script); CHECK(!gFieldStatuses && !sTestFieldCommits);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_MoveEffectGravity && (gFieldStatuses & STATUS_FIELD_GRAVITY)
        && gFieldTimers.gravityTimer == 5 && sTestFieldCommits == 1
        && sLastTestFieldCommit.move == RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE));
    BattleScriptPop(); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_MoveEffectIonDeluge && (gFieldStatuses & STATUS_FIELD_ION_DELUGE)
        && sTestFieldCommits == 2);
    BattleScriptPop(); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    ModsMoveEffectReset(); stack.size = 0; gFieldStatuses = STATUS_FIELD_GRAVITY;
    BeginFieldEffect(script); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_MoveEffectIonDeluge && sTestFieldCommits == 3);
    BattleScriptPop(); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    ModsMoveEffectReset(); stack.size = 0; gFieldStatuses = 0; BeginFieldEffect(script); gBattleMons[0].hp = 0;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage && !gFieldStatuses && sTestFieldCommits == 3);
    gBattleMons[0].hp = gBattleMons[0].maxHP;
    ModsMoveEffectReset(); stack.size = 0; BeginFieldEffect(script); gBattlerPartyIndexes[0] = 1;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage && !gFieldStatuses && sTestFieldCommits == 3);
    gBattlerPartyIndexes[0] = 0;
    ModsMoveEffectReset(); stack.size = 0; *mode = 1; BeginFieldEffect(script);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_MoveEffectGravity && sTestFieldCommits == 4);
    BattleScriptPop(); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage && sTestFieldCommits == 4);
    ModsMoveEffectReset(); stack.size = 0; *mode = 0; gFieldStatuses = 0;
    gBattlerAttacker = 0; gBattlerTarget = 1; gCurrentMove = gChosenMove = MOVE_SING;
    script = GetMoveBattleScript(MOVE_SING); CHECK(script == BattleScript_EffectModCustomStatus);
    BeginFieldEffect(script); gBattlerPartyIndexes[1] = 1;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage && !gFieldStatuses && sTestFieldCommits == 4
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gBattlerPartyIndexes[1] = 0; gCurrentMove = gChosenMove = MOVE_RAIN_DANCE; *mode = 0;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 105);
    gFieldStatuses = STATUS_FIELD_GRAVITY;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 95);
    gFieldStatuses = 0; *mode = 2;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 100);
    *mode = 3; CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 100
        && !gFieldStatuses && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    const u32 flags[] = {STATUS_FIELD_TRICK_ROOM, STATUS_FIELD_WONDER_ROOM, STATUS_FIELD_MAGIC_ROOM};
    const u32 rules[] = {RHH_BATTLE_FIELD_TRICK_ROOM, RHH_BATTLE_FIELD_WONDER_ROOM, RHH_BATTLE_FIELD_MAGIC_ROOM};
    const u8 *presentations[] = {BattleScript_ModEffectTrickRoom, BattleScript_ModEffectRoom, BattleScript_ModEffectRoom};
    u8 *timers[] = {&gFieldTimers.trickRoomTimer, &gFieldTimers.wonderRoomTimer, &gFieldTimers.magicRoomTimer};
    gBattlerAttacker = gBattlerTarget = 0; script = GetMoveBattleScript(MOVE_RAIN_DANCE);
    for (u32 cycle = 0; cycle < 2; ++cycle)
    {
        *mode = 4; ModsMoveEffectReset(); stack.size = 0; BeginFieldEffect(script);
        for (u32 i = 0; i < 3; ++i)
        {
            gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
            CHECK(gBattlescriptCurrInstr == presentations[i] && !!(gFieldStatuses & flags[i]) == !cycle
                && *timers[i] == (cycle ? 0 : 5) && gBattleCommunication[MULTISTRING_CHOOSER] == i * 2 + cycle
                && sTestFieldCommits == 5 + cycle * 3 + i && sLastTestFieldCommit.active_changed == rules[i]
                && sLastTestFieldCommit.cause == RHH_BATTLE_FIELD_CAUSE_MOVE
                && sLastTestFieldCommit.move == RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE));
            if (!cycle && !i)
            {
                /* Skip only the room message and run its real item-service loop. */
                gBattleMons[0].item = ITEM_ROOM_SERVICE;
                gBattleMons[0].statStages[STAT_SPEED] = DEFAULT_STAT_STAGE;
                gBattlescriptCurrInstr = BattleScript_ModEffectTrickRoom + 1 + sizeof(void *) + 3;
                CHECK(*gBattlescriptCurrInstr == B_SCR_OP_CALL);
                gBattleScriptingCommandsTable[*gBattlescriptCurrInstr]();
                for (u32 step = 0; step < 4; ++step)
                    gBattleScriptingCommandsTable[*gBattlescriptCurrInstr]();
                CHECK(gBattlescriptCurrInstr == BattleScript_ConsumableItemStatRaise && gLastUsedItem == ITEM_ROOM_SERVICE);
                gBattleMons[0].item = ITEM_NONE; stack.size = 0;
            }
            gBattlescriptCurrInstr = BattleScript_ModEffectNextAction;
        }
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    }
    *mode = 5; ModsMoveEffectReset(); BeginFieldEffect(script);
    for (u32 i = 0; i < 2; ++i)
    {
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectTrickRoom && !!(gFieldStatuses & STATUS_FIELD_TRICK_ROOM) == !i);
        gBattlescriptCurrInstr = BattleScript_ModEffectNextAction;
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    *mode = 6; ModsMoveEffectReset(); BeginFieldEffect(script);
    for (u32 i = 0; i < 8; ++i)
    {
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectTrickRoom && !!(gFieldStatuses & STATUS_FIELD_TRICK_ROOM) == !(i & 1));
        gBattlescriptCurrInstr = BattleScript_ModEffectNextAction;
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage && !gFieldStatuses);
    *mode = 4; ModsMoveEffectReset(); gBattlerTarget = 1; gCurrentMove = gChosenMove = MOVE_SING;
    script = GetMoveBattleScript(MOVE_SING); BeginFieldEffect(script); gBattlerPartyIndexes[1] = 1;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage && !gFieldStatuses && sTestFieldCommits == 20);
    gBattlerPartyIndexes[1] = 0; gCurrentMove = gChosenMove = MOVE_RAIN_DANCE;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 100);
    gFieldStatuses = STATUS_FIELD_TRICK_ROOM | STATUS_FIELD_MAGIC_ROOM;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 100);
    gFieldStatuses = 0; *mode = 6;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 100 && !gFieldStatuses
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    const u32 fieldFlags[] = {STATUS_FIELD_MUDSPORT, STATUS_FIELD_WATERSPORT, STATUS_FIELD_FAIRY_LOCK};
    const u32 fieldRules[] = {RHH_BATTLE_FIELD_MUD_SPORT, RHH_BATTLE_FIELD_WATER_SPORT, RHH_BATTLE_FIELD_FAIRY_LOCK};
    u8 *fieldTimers[] = {&gFieldTimers.mudSportTimer, &gFieldTimers.waterSportTimer, &gFieldTimers.fairyLockTimer};
    gBattlerAttacker = gBattlerTarget = 0; gCurrentMove = gChosenMove = MOVE_RAIN_DANCE;
    script = GetMoveBattleScript(MOVE_RAIN_DANCE); *mode = 7; ModsMoveEffectReset(); BeginFieldEffect(script);
    for (u32 i = 0; i < 3; ++i)
    {
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(gBattlescriptCurrInstr == (i == 2 ? BattleScript_ModEffectFairyLock : BattleScript_ModEffectSport)
            && (gFieldStatuses & fieldFlags[i]) && *fieldTimers[i] == (i == 2 ? 2 : 5)
            && sTestFieldCommits == 21 + i && sLastTestFieldCommit.active_changed == fieldRules[i]
            && sLastTestFieldCommit.cause == RHH_BATTLE_FIELD_CAUSE_MOVE
            && sLastTestFieldCommit.move == RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE));
        gBattlescriptCurrInstr = BattleScript_ModEffectNextAction;
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    ModsMoveEffectReset(); BeginFieldEffect(script);
    for (u32 i = 0; i < 3; ++i)
    {
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction && sTestFieldCommits == 23);
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 100);
    gFieldStatuses = 0; gBattleStruct->isSkyBattle = TRUE; *mode = 9;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 100);
    *mode = 7; ModsMoveEffectReset(); BeginFieldEffect(script);
    for (u32 i = 0; i < 2; ++i)
    {
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction && !gFieldStatuses && sTestFieldCommits == 23);
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectFairyLock && gFieldStatuses == STATUS_FIELD_FAIRY_LOCK
        && sTestFieldCommits == 24);
    gBattlescriptCurrInstr = BattleScript_ModEffectNextAction; gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    gBattleStruct->isSkyBattle = FALSE; gFieldStatuses = 0; *mode = 8; ModsMoveEffectReset(); BeginFieldEffect(script);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectFairyLock && sTestFieldCommits == 25);
    gBattlescriptCurrInstr = BattleScript_ModEffectNextAction;
    for (u32 i = 1; i < 8; ++i)
    {
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction && sTestFieldCommits == 25);
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    gFieldStatuses = 0; *mode = 7;
    CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 100);
    *mode = 8; CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 100);
    *mode = 7; ModsMoveEffectReset(); gBattlerTarget = 1; gCurrentMove = gChosenMove = MOVE_SING;
    script = GetMoveBattleScript(MOVE_SING); BeginFieldEffect(script); gBattlerPartyIndexes[1] = 1;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage && !gFieldStatuses && sTestFieldCommits == 25
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gBattlerPartyIndexes[1] = 0;
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gAiLogicData = NULL; RhhModsUnloadLibraries();
    puts("battle field effect: queued engine effects, validation and stale participants passed");
}
static void CheckFieldRuleLegacySport(const char *module)
{
    CHECK(B_SPORT_TURNS < GEN_6);
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sTestFieldMod)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sTestFieldRules = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 7));
    CHECK(sTestFieldRules);
    u8 *mode = RhhModsState("test:field_rule_effect"); CHECK(mode);
    struct BattleScriptsStack stack = {0}; struct BattleResources resources = {.battleScriptsStack = &stack};
    struct AiLogicData ai = {0}; gBattleResources = &resources; gAiLogicData = &ai;
    PrepareTurnBattle(); gFieldStatuses = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers));
    sTestFieldObservations = sTestFieldCommits = 0; ModsBattleBegin();
    gBattlerAttacker = gBattlerTarget = 0; gCurrentMove = gChosenMove = MOVE_RAIN_DANCE;
    const u8 *script = GetMoveBattleScript(MOVE_RAIN_DANCE);
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBattleSports sports = {.struct_size = sizeof(sports)};
    CHECK(!sTestFieldRules->sports(&sports) && sports.mode == RHH_BATTLE_SPORT_BATTLER_VOLATILE
        && !sports.effective && !sports.mud_battlers && !sports.water_battlers);
    RhhModsObserveBattleEnvironment();
    *mode = 7; CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 100);
    BeginFieldEffect(script);
    for (u32 i = 0; i < 3; ++i)
    {
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(gBattlescriptCurrInstr == (i == 2 ? BattleScript_ModEffectFairyLock : BattleScript_ModEffectSport));
        if (i == 0) CHECK(gBattleMons[0].volatiles.mudSport && !gFieldStatuses && !sTestFieldCommits);
        if (i == 1) CHECK(gBattleMons[0].volatiles.waterSport && !gFieldStatuses && !sTestFieldCommits);
        if (i == 2) CHECK(gFieldStatuses == STATUS_FIELD_FAIRY_LOCK && sTestFieldCommits == 1);
        gBattlescriptCurrInstr = BattleScript_ModEffectNextAction;
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
    CHECK(!sTestFieldRules->sports(&sports) && sports.mud_battlers == 1 && sports.water_battlers == 1
        && !sports.field_active && sports.effective == (RHH_BATTLE_FIELD_MUD_SPORT | RHH_BATTLE_FIELD_WATER_SPORT));
    RhhModsObserveBattleEnvironment();
    ai.aiCalcInProgress = TRUE; gBattleMons[0].volatiles.mudSport = FALSE;
    RhhModsObserveBattleEnvironment(); gBattleMons[0].volatiles.mudSport = TRUE; ai.aiCalcInProgress = FALSE;
    RhhModsObserveBattleEnvironment();
    *mode = 10; CHECK(ModsAiMoveScore(0, 1, MOVE_RAIN_DANCE, 0, 100) == 100);
    *mode = 7; ModsMoveEffectReset(); BeginFieldEffect(script);
    for (u32 i = 0; i < 3; ++i)
    {
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextAction && sTestFieldCommits == 1);
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gBattleMons[0].volatiles.mudSport = FALSE; gBattleMons[1].volatiles.mudSport = TRUE;
    gAbsentBattlerFlags |= 2;
    CHECK(!sTestFieldRules->sports(&sports) && sports.mud_battlers == 2 && sports.water_battlers == 1
        && sports.effective == (RHH_BATTLE_FIELD_MUD_SPORT | RHH_BATTLE_FIELD_WATER_SPORT));
    RhhModsObserveBattleEnvironment();
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gAbsentBattlerFlags &= ~2;
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gAiLogicData = NULL; RhhModsUnloadLibraries();
    puts("battle field effect: legacy per-battler sports and AI passed");
}
