#include "battle_main.h"
#include "battle_ai_util.h"
#include "battle_ai_main.h"
#include "battle_controllers.h"
#include "battle_gimmick.h"
#include "battle_end_turn.h"
#include "battle_script_commands.h"
#include "constants/battle_end_turn.h"
void BS_RemoveWeather(void);
void BS_RemoveWeatherTeraformZero(void);
void BS_RemoveTerrain(void);
void BS_RemoveTerrainMove(void);
void BS_RemoveTerrainTeraformZero(void);
void BS_TryToClearPrimalWeather(void);

static bool32 sImpureBattlerPriority;
static bool32 sImpureInnatePriority;
static bool32 sImpureSpeed, sImpureInnateSpeed;
static bool32 sImpureDamageStat, sImpureInnateDamageStat;
static const struct RhhMoveService *sCriticalMoves;
static const struct RhhAbilityService *sCriticalAbilities;
static u32 sCriticalMode, sCriticalCalls, sCriticalInnateCalls, sCriticalLastFlags;
static u32 sFinalMode, sFinalCalls, sFinalInnateCalls, sFinalLastFlags, sFinalLastKind;
static bool32 sCaptureBeat;
static u32 sBeatCaptureCount, sBeatStageMask;
static struct RhhBeatUpContributor sBeatCaptured[256];
static void BeatUpPhaseProbe(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (!sCaptureBeat) return;
    struct RhhBeatUpContributor result = {.struct_size = sizeof(result)};
    struct RhhBeatUpContributor tiny = {.struct_size = 1, .party_slot = 88}, saved = tiny;
    if (sCaptureBeat == 2)
    {
        result.hit_index = 77; struct RhhBeatUpContributor original = result;
        CHECK(sCriticalMoves->beat_up_contributor(&result) && !memcmp(&result, &original, sizeof(result)));
        ++sBeatCaptureCount; return;
    }
    CHECK(!sCriticalMoves->beat_up_contributor(&result)
        && sCriticalMoves->beat_up_contributor(&tiny) && !memcmp(&tiny, &saved, sizeof(saved))
        && sCriticalMoves->beat_up_contributor(NULL) && sBeatCaptureCount < ARRAY_COUNT(sBeatCaptured));
    sBeatCaptured[sBeatCaptureCount++] = result;
    if (event->kind == RHH_MOVE_POWER) sBeatStageMask |= 1;
    if (event->kind == RHH_INNATE_POWER) sBeatStageMask |= 2;
    if (event->kind == RHH_DAMAGE_STAT) sBeatStageMask |= 4;
    if (event->kind == RHH_INNATE_DAMAGE_STAT) sBeatStageMask |= 8;
    if (event->kind == RHH_DAMAGE_FINAL) sBeatStageMask |= 16;
    if (event->kind == RHH_INNATE_DAMAGE_FINAL) sBeatStageMask |= 32;
}
static void FinalDamageProbe(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sFinalMode) return;
    if (event->kind == RHH_DAMAGE_FINAL || event->kind == RHH_SELF_DAMAGE_FINAL) {
        struct RhhFinalDamageContext c = {.struct_size = sizeof(c)}, small = {.struct_size = 1, .engine_damage = 99};
        CHECK(!sCriticalMoves->final_damage_context(&c) && sCriticalMoves->final_damage_context(&small)
            && small.engine_damage == 99 && c.move == event->event_key && c.engine_damage == event->args[0]
            && c.flags == event->args[1] && c.attacker == event->args[2] && c.defender == event->args[3]
            && c.max_damage == RHH_FINAL_DAMAGE_MAX && c.effectiveness_q12);
        struct RhhInnateResolution list = {.struct_size = sizeof(list)};
        CHECK(!sCriticalAbilities->final_damage_innates(RHH_INNATE_POWER_ATTACKER, &list));
        ++sFinalCalls; sFinalLastFlags = c.flags; sFinalLastKind = event->kind;
        if (sFinalMode == 1 || sFinalMode == 7 || sFinalMode == 8) decision->value = c.engine_damage * 2u;
        if (sFinalMode == 2) decision->value = 0;
        if (sFinalMode == 3) decision->value = RHH_FINAL_DAMAGE_MAX + 1u;
        if (sFinalMode == 4) { decision->value = 7; (void)Random(); (void)Random2(); }
        if (sFinalMode == 9) decision->value = RHH_FINAL_DAMAGE_MAX;
    } else {
        struct RhhInnateFinalDamageContext c = {.struct_size = sizeof(c)}, small = {.struct_size = 1, .index = 99};
        CHECK(!sCriticalAbilities->innate_final_damage_context(&c) && sCriticalAbilities->innate_final_damage_context(&small)
            && small.index == 99 && c.ability == event->event_key && c.index == event->ordinal
            && c.current_damage == event->args[0] && c.role == event->args[1]
            && c.damage.move == event->subject && c.index < c.resolution.count && !c.resolution.entries[c.index].status);
        ++sFinalInnateCalls;
        if (sFinalMode == 5 || sFinalMode == 6) {
            decision->value = c.current_damage + 1;
            if (sFinalMode == 6 && c.index == 0) { (void)Random(); (void)Random2(); }
        }
    }
}
static void FinalDamageConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->kind != RHH_DAMAGE_FINAL && event->kind != RHH_SELF_DAMAGE_FINAL) return;
    if (sFinalMode == 7) decision->allowed = 0;
    if (sFinalMode == 8) decision->value = 0;
}
static void FinalDamageCannotUndo(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; if (sFinalMode == 7) decision->allowed = 1; }
static void CriticalStageProbe(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sCriticalMode) return;
    if (event->kind == RHH_MOVE_CRITICAL_STAGE) {
        struct RhhCriticalStageContext c = {.struct_size = sizeof(c)}, small = {.struct_size = 1, .base_value = 99};
        CHECK(!sCriticalMoves->critical_stage_context(&c) && sCriticalMoves->critical_stage_context(&small)
            && small.base_value == 99 && c.move == event->event_key && c.base_value == event->args[0]
            && c.generation == event->args[1] && c.attacker == event->args[2] && c.defender == event->args[3]
            && c.denominator && c.max_value == (c.generation == 1 ? 255 : 4));
        struct RhhInnateResolution list = {.struct_size = sizeof(list)};
        CHECK(!sCriticalAbilities->critical_stage_innates(RHH_INNATE_POWER_ATTACKER, &list));
        sCriticalLastFlags = c.flags;
        ++sCriticalCalls;
        if (sCriticalMode == 1) decision->value = c.base_value + 1;
        if (sCriticalMode == 2) decision->value = c.max_value + 1;
        if (sCriticalMode == 3) { decision->value = c.base_value + 1; (void)Random(); (void)Random2(); }
        if (sCriticalMode == 4) decision->allowed = 0;
    } else {
        struct RhhInnateCriticalStageContext c = {.struct_size = sizeof(c)}, small = {.struct_size = 1, .index = 99};
        CHECK(!sCriticalAbilities->innate_critical_stage_context(&c) && sCriticalAbilities->innate_critical_stage_context(&small)
            && small.index == 99 && c.ability == event->event_key && c.index == event->ordinal
            && c.current_value == event->args[0] && c.role == event->args[1]);
        ++sCriticalInnateCalls;
        if (sCriticalMode == 5 && c.current_value < c.critical.max_value) decision->value = c.current_value + 1;
        if (sCriticalMode == 6) { decision->value = c.current_value + 1; (void)Random(); (void)Random2(); }
    }
}
static u32 sBattlerPriorityDispatches;
static const struct RhhBattleService *sBattleEnvironmentService;
static struct RhhBattleEnvironmentObservation sLastEnvironmentObservation;
static u32 sEnvironmentEvents;
static bool32 sEnvironmentConsumeRng;
static const struct RhhAbilityService *sFieldActionAbilities;
static u32 sFieldActionMode, sFieldActionCallbacks;
static bool32 sDropTorrent;
static void RemoveEarlierFieldInnate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (!sDropTorrent || event->event_key != RhhContentKeyForId(RHH_SPECIES, SPECIES_CHARIZARD)) return;
    struct RhhInnateBattleContext context = {.struct_size = sizeof(context)};
    CHECK(!sFieldActionAbilities->battle_innate_context(&context));
    if (context.battler == 1) CHECK(sFieldActionAbilities->remove_battle_innate(RhhContentKeyForId(RHH_ABILITY, ABILITY_TORRENT)) == RHH_INNATE_OK);
}
static void InnateFieldAction(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    RhhContentKey selected = RhhContentKeyForId(RHH_ABILITY, sFieldActionMode == 3 ? ABILITY_LEVITATE : ABILITY_TORRENT);
    if (!sFieldActionMode || event->event_key != selected) return;
    if (event->kind == RHH_INNATE_ENTRY) {
        struct RhhInnateEntryContext context = {.struct_size = sizeof(context)};
        CHECK(!sFieldActionAbilities->innate_entry_context(&context));
        if (context.battler != 1) return;
        CHECK(sFieldActionAbilities->innate_entry_weather(RHH_WEATHER_NONE)
            && sFieldActionAbilities->innate_entry_weather(RHH_WEATHER_WIND)
            && sFieldActionAbilities->innate_entry_terrain(RHH_TERRAIN_NONE));
        CHECK(!sFieldActionAbilities->innate_entry_weather(RHH_WEATHER_RAIN)
            && !sFieldActionAbilities->innate_entry_terrain(RHH_TERRAIN_ELECTRIC));
    } else {
        struct RhhInnateTurnContext context = {.struct_size = sizeof(context)};
        CHECK(!sFieldActionAbilities->innate_turn_context(&context));
        if (context.battler != 1) return;
        CHECK(sFieldActionAbilities->innate_turn_weather(RHH_WEATHER_NONE)
            && sFieldActionAbilities->innate_turn_terrain(RHH_TERRAIN_NONE)
            && !sFieldActionAbilities->innate_turn_weather(RHH_WEATHER_SUN)
            && !sFieldActionAbilities->innate_turn_terrain(RHH_TERRAIN_GRASSY));
    }
    ++sFieldActionCallbacks;
}
static bool32 sTestSeed, sTestSeedDeny;
static u32 sTestSeedCalls;
static void ProposeTestBattleSeed(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sTestSeed) return;
    ++sTestSeedCalls;
    if (sTestSeedDeny) decision->allowed = 0;
    else decision->value = event->args[0] == RHH_WORLD_WEATHER ? RHH_WEATHER_RAIN : RHH_TERRAIN_GRASSY;
}
static struct RhhBattleEnvironmentCommit sLastEnvironmentCommit;
static u32 sEnvironmentCommits;
static struct RhhBattleEnvironmentTimerTick sLastEnvironmentTimerTick;
static u32 sEnvironmentTimerTicks;
static void ObserveBattleEnvironmentTimerTick(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleEnvironmentTimerTick tick = {.struct_size = sizeof(tick)};
    struct RhhBattleEnvironmentTimerTick small = {.struct_size = 1, .kind = 99}, saved = small;
    struct RhhBattleEnvironmentState nested = {.struct_size = sizeof(nested)};
    CHECK(sBattleEnvironmentService && !sBattleEnvironmentService->environment_timer_tick(&tick)
        && sBattleEnvironmentService->environment_timer_tick(&small) && !memcmp(&small, &saved, sizeof(small))
        && !ModsBattleEnvironmentTimerCapture(&nested) && tick.sequence == sEnvironmentTimerTicks
        && event->ordinal == tick.sequence && event->subject == tick.after.battle_id
        && event->args[0] == tick.kind && event->args[1] == (tick.kind == RHH_BATTLE_ENV_TIMER_WEATHER ? tick.before.weather_turns : tick.before.terrain_turns)
        && event->args[2] == (tick.kind == RHH_BATTLE_ENV_TIMER_WEATHER ? tick.after.weather_turns : tick.after.terrain_turns));
    sLastEnvironmentTimerTick = tick; ++sEnvironmentTimerTicks;
    ModsBattleEnvironmentTimerTicked(&tick.before, tick.kind);
    if (sEnvironmentConsumeRng) { (void)Random(); (void)Random2(); }
}
static void ObserveBattleEnvironmentCommit(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleEnvironmentCommit context = {.struct_size = sizeof(context)};
    struct RhhBattleEnvironmentCommit small = {.struct_size = 1, .cause = 99}, oldSmall = small;
    struct RhhBattleEnvironmentState now = {.struct_size = sizeof(now)};
    CHECK(sBattleEnvironmentService && !sBattleEnvironmentService->environment_commit(&context)
        && sBattleEnvironmentService->environment_commit(&small) && !memcmp(&small, &oldSmall, sizeof(small))
        && !sBattleEnvironmentService->environment(&now) && context.sequence == event->ordinal
        && context.sequence == sEnvironmentCommits && context.cause == event->args[0]
        && context.source_battler == event->args[1] && context.changed == event->args[2]
        && event->subject == context.after.battle_id && context.after.battle_id == now.battle_id
        && context.after.weather == now.weather && context.after.terrain == now.terrain
        && event->event_key == (context.move ? context.move : context.ability));
    sLastEnvironmentCommit = context;
    ++sEnvironmentCommits;
    if (sEnvironmentConsumeRng) { (void)Random(); (void)Random2(); }
}
static void ObserveBattleEnvironment(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleEnvironmentObservation observation = {.struct_size = sizeof(observation)};
    struct RhhBattleEnvironmentObservation small = {.struct_size = 1, .changed = 99}, savedSmall = small;
    struct RhhBattleEnvironmentState current = {.struct_size = sizeof(current)};
    CHECK(sBattleEnvironmentService && !sBattleEnvironmentService->environment_observation(&observation)
        && sBattleEnvironmentService->environment_observation(&small) && !memcmp(&small, &savedSmall, sizeof(small))
        && !sBattleEnvironmentService->environment(&current)
        && observation.changed == event->args[0] && observation.after.weather == event->args[1]
        && observation.after.terrain == event->args[2] && observation.after.weather_suppressed == event->args[3]
        && observation.sequence == event->ordinal && observation.sequence == sEnvironmentEvents
        && observation.after.run_id == current.run_id && observation.after.battle_id == current.battle_id
        && observation.after.weather == current.weather && observation.after.effective_weather == current.effective_weather);
    sLastEnvironmentObservation = observation;
    ++sEnvironmentEvents;
    RhhModsObserveBattleEnvironment(); /* No nested notification. */
    if (sEnvironmentConsumeRng) { (void)Random(); (void)Random2(); }
}
static void RunInnateEnvironmentPresentation(const u8 *resume)
{
    for (u32 steps = 0; gBattlescriptCurrInstr != resume; ++steps) {
        CHECK(steps < 10000 && gBattlescriptCurrInstr && *gBattlescriptCurrInstr != B_SCR_OP_END);
        if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
        u32 opcode = *gBattlescriptCurrInstr;
        if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[opcode]();
    }
    gBattleControllerExecFlags = 0;
}
static void EnvironmentTestFrame(void)
{
    gBattleWeather = B_WEATHER_RAIN_NORMAL; gBattleStruct->weatherDuration = 4;
    gMain.callback1 = NULL;
}
static void ImpurePriority(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->kind == RHH_DAMAGE_STAT) {
        if (sImpureDamageStat) { (void)Random(); (void)Random2(); decision->value = 99; }
    } else if (event->kind == RHH_INNATE_DAMAGE_STAT) {
        if (sImpureInnateDamageStat && event->ordinal == 0) { (void)Random(); (void)Random2(); decision->value = 99; }
    } else if (event->kind == RHH_BATTLER_SPEED) {
        if (sImpureSpeed) { (void)Random(); (void)Random2(); decision->value = 100; }
    } else if (event->kind == RHH_INNATE_SPEED) {
        if (sImpureInnateSpeed && event->ordinal == 0) { (void)Random(); (void)Random2(); decision->value = 100; }
    } else if (event->kind == RHH_INNATE_PRIORITY) {
        if (sImpureInnatePriority && event->ordinal == 0) { (void)Random(); (void)Random2(); decision->value = 14; }
    } else if (event->kind == RHH_MOVE_BATTLER_PRIORITY) {
        ++sBattlerPriorityDispatches;
        if (sImpureBattlerPriority) { (void)Random(); (void)Random2(); decision->value = 17; }
    } else if (event->event_key == RhhContentKeyOf("rhh:move/tackle")) {
        (void)Random(); (void)Random2(); decision->value = 14;
    }
}
static const struct RhhModHook sImpurePriorityHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_POWER, RHH_TRANSFORM, 0, BeatUpPhaseProbe, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_POWER, RHH_TRANSFORM, 0, BeatUpPhaseProbe, NULL},
    {sizeof(struct RhhModHook), RHH_DAMAGE_STAT, RHH_TRANSFORM, 0, BeatUpPhaseProbe, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_DAMAGE_STAT, RHH_TRANSFORM, 0, BeatUpPhaseProbe, NULL},
    {sizeof(struct RhhModHook), RHH_DAMAGE_FINAL, RHH_TRANSFORM, 0, BeatUpPhaseProbe, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_DAMAGE_FINAL, RHH_TRANSFORM, 0, BeatUpPhaseProbe, NULL},
    {sizeof(struct RhhModHook), RHH_DAMAGE_FINAL, RHH_TRANSFORM, 0, FinalDamageProbe, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_DAMAGE_FINAL, RHH_TRANSFORM, 0, FinalDamageProbe, NULL},
    {sizeof(struct RhhModHook), RHH_SELF_DAMAGE_FINAL, RHH_TRANSFORM, 0, FinalDamageProbe, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_SELF_DAMAGE_FINAL, RHH_TRANSFORM, 0, FinalDamageProbe, NULL},
    {sizeof(struct RhhModHook), RHH_SELF_DAMAGE_FINAL, RHH_CONSTRAINT, 0, FinalDamageConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_SELF_DAMAGE_FINAL, RHH_CONSTRAINT, 1, FinalDamageCannotUndo, NULL},
    {sizeof(struct RhhModHook), RHH_DAMAGE_FINAL, RHH_CONSTRAINT, 0, FinalDamageConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_DAMAGE_FINAL, RHH_CONSTRAINT, 1, FinalDamageCannotUndo, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_CRITICAL_STAGE, RHH_TRANSFORM, 0, CriticalStageProbe, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_CRITICAL_STAGE, RHH_TRANSFORM, 0, CriticalStageProbe, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_PRIORITY, RHH_TRANSFORM, 0, ImpurePriority, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BATTLER_PRIORITY, RHH_TRANSFORM, 0, ImpurePriority, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_PRIORITY, RHH_TRANSFORM, 0, ImpurePriority, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLER_SPEED, RHH_TRANSFORM, 0, ImpurePriority, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_SPEED, RHH_TRANSFORM, 0, ImpurePriority, NULL},
    {sizeof(struct RhhModHook), RHH_DAMAGE_STAT, RHH_TRANSFORM, 0, ImpurePriority, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_DAMAGE_STAT, RHH_TRANSFORM, 0, ImpurePriority, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_ENVIRONMENT_OBSERVED, RHH_OBSERVE, 0, NULL, ObserveBattleEnvironment},
    {sizeof(struct RhhModHook), RHH_BATTLE_ENVIRONMENT_COMMITTED, RHH_OBSERVE, 0, NULL, ObserveBattleEnvironmentCommit},
    {sizeof(struct RhhModHook), RHH_BATTLE_ENVIRONMENT_TIMER_TICKED, RHH_OBSERVE, 0, NULL, ObserveBattleEnvironmentTimerTick},
    {sizeof(struct RhhModHook), RHH_BATTLE_ENVIRONMENT_SEED, RHH_TRANSFORM, 0, ProposeTestBattleSeed, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_BATTLE_PERSONALIZE, RHH_CONSTRAINT, 0, RemoveEarlierFieldInnate, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_ENTRY, RHH_OBSERVE, 0, NULL, InnateFieldAction},
    {sizeof(struct RhhModHook), RHH_INNATE_TURN_END, RHH_OBSERVE, 0, NULL, InnateFieldAction},
};
static const struct RhhModDefinition sImpurePriorityMod = {.struct_size = sizeof(sImpurePriorityMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:impure_priority", .hooks = sImpurePriorityHooks,
    .hook_count = ARRAY_COUNT(sImpurePriorityHooks)};
static const struct RhhAbilityService *sOnlyPriorityAbilities;
static RhhContentKey sOnlyFieldAbility;
static const struct RhhModAbility sOnlyFieldDeclaration = {sizeof(struct RhhModAbility), RHH_ABILITY_BREAKABLE, {0, 0},
    "test:ability/field_probe", "Field Probe", "Test-only field action."};
static void OnlyPriorityOffer(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    RhhContentKey ability = event->event_key == RhhContentKeyForId(RHH_SPECIES, SPECIES_CHARIZARD)
        ? RhhContentKeyForId(RHH_ABILITY, ABILITY_TORRENT)
        : event->event_key == RhhContentKeyForId(RHH_SPECIES, SPECIES_TORCHIC) ? sOnlyFieldAbility : 0;
    if (!ability) return;
    struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .ability = ability, .unlock_level = 10};
    CHECK(sOnlyPriorityAbilities->offer_innate(&offer) == RHH_INNATE_OK);
}
static void OnlyPriorityTrigger(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhInnatePriorityContext context = {.struct_size = sizeof(context)};
    CHECK(!sOnlyPriorityAbilities->innate_priority_context(&context) && context.ability == event->event_key);
    decision->value = event->args[0] + 1;
}
static void OnlySpeedTrigger(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhInnateSpeedContext context = {.struct_size = sizeof(context)};
    CHECK(!sOnlyPriorityAbilities->innate_speed_context(&context) && context.ability == event->event_key);
    decision->value = event->args[0] + 1;
}
static void OnlyDamageStatTrigger(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhInnateDamageStatContext context = {.struct_size = sizeof(context)};
    CHECK(!sOnlyPriorityAbilities->innate_damage_stat_context(&context) && context.ability == event->event_key);
    if (context.role == RHH_INNATE_POWER_ATTACKER) decision->value = event->args[0] + 1;
}
static void OnlyFieldAction(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (event->event_key != sOnlyFieldAbility) return;
    struct RhhInnateEntryContext context = {.struct_size = sizeof(context)};
    CHECK(!sOnlyPriorityAbilities->innate_entry_context(&context));
    if (context.battler != 0) return;
    CHECK(!sOnlyPriorityAbilities->innate_entry_weather(RHH_WEATHER_RAIN)
        && !sOnlyPriorityAbilities->innate_entry_terrain(RHH_TERRAIN_ELECTRIC));
}
static const struct RhhModHook sOnlyPriorityHooks[] = {
    {sizeof(struct RhhModHook), RHH_INNATE_PROPOSE, RHH_CONSTRAINT, 0, OnlyPriorityOffer, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_PRIORITY, RHH_TRANSFORM, 0, OnlyPriorityTrigger, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_SPEED, RHH_TRANSFORM, 0, OnlySpeedTrigger, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_DAMAGE_STAT, RHH_TRANSFORM, 0, OnlyDamageStatTrigger, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_ENTRY, RHH_OBSERVE, 0, NULL, OnlyFieldAction},
    {sizeof(struct RhhModHook), RHH_BATTLE_ENVIRONMENT_COMMITTED, RHH_OBSERVE, 0, NULL, ObserveBattleEnvironmentCommit}
};
static const struct RhhModDefinition sOnlyPriorityMod = {.struct_size = sizeof(sOnlyPriorityMod),
    .abi_version = RHH_MOD_ABI_ABILITIES, .id = "test:only_innate_priority", .hooks = sOnlyPriorityHooks,
    .hook_count = ARRAY_COUNT(sOnlyPriorityHooks), .ability_count = 1, .abilities = &sOnlyFieldDeclaration};

static void CheckMovePriority(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sImpurePriorityMod)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 41));
    CHECK(moves && moves->header.struct_size >= RHH_MOVE_SERVICE_V1_40_SIZE
        && RHH_MOVE_SERVICE_V1_38_SIZE < RHH_MOVE_SERVICE_V1_39_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 39)) == &moves->header
        && !RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 55)));
    struct RhhMoveBattlerPriority outside = {.struct_size = sizeof(outside), .hp = 77}, saved = outside;
    CHECK(moves->battler_priority_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    const struct RhhAbilityService *abilities = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/abilities", RHH_SERVICE_VERSION(1, 34));
    sCriticalMoves = moves; sCriticalAbilities = abilities; sCriticalMode = sFinalMode = 0;
    struct RhhCriticalStageContext outsideCritical = {.struct_size = sizeof(outsideCritical), .base_value = 99}, savedCritical = outsideCritical;
    struct RhhInnateCriticalStageContext outsideInnateCritical = {.struct_size = sizeof(outsideInnateCritical), .index = 99}, savedInnateCritical = outsideInnateCritical;
    struct RhhFinalDamageContext outsideFinal = {.struct_size = sizeof(outsideFinal), .engine_damage = 99}, savedFinal = outsideFinal;
    struct RhhInnateFinalDamageContext outsideInnateFinal = {.struct_size = sizeof(outsideInnateFinal), .index = 99}, savedInnateFinal = outsideInnateFinal;
    CHECK(RHH_MOVE_SERVICE_V1_47_SIZE < RHH_MOVE_SERVICE_V1_48_SIZE
        && RHH_ABILITY_SERVICE_V1_36_SIZE < RHH_ABILITY_SERVICE_V1_37_SIZE
        && RHH_MOVE_SERVICE_V1_50_SIZE < RHH_MOVE_SERVICE_V1_51_SIZE
        && RHH_ABILITY_SERVICE_V1_37_SIZE < RHH_ABILITY_SERVICE_V1_38_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 52)) == &moves->header
        && RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 39)) == &abilities->header
        && RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 53)) == &moves->header
        && RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 54)) == &moves->header
        && RHH_MOVE_SERVICE_V1_53_SIZE < RHH_MOVE_SERVICE_V1_54_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 40)) == &abilities->header
        && moves->final_damage_context(&outsideFinal) && !memcmp(&outsideFinal, &savedFinal, sizeof(savedFinal))
        && abilities->innate_final_damage_context(&outsideInnateFinal) && !memcmp(&outsideInnateFinal, &savedInnateFinal, sizeof(savedInnateFinal))
        && moves->critical_stage_context(&outsideCritical) && !memcmp(&outsideCritical, &savedCritical, sizeof(savedCritical))
        && abilities->innate_critical_stage_context(&outsideInnateCritical) && !memcmp(&outsideInnateCritical, &savedInnateCritical, sizeof(savedInnateCritical)));
    struct RhhBeatUpContributor outsideBeat = {.struct_size = sizeof(outsideBeat), .hit_index = 99}, savedOutsideBeat = outsideBeat;
    CHECK(moves->beat_up_contributor(&outsideBeat) && !memcmp(&outsideBeat, &savedOutsideBeat, sizeof(outsideBeat)));
    sFieldActionAbilities = abilities; sFieldActionMode = sFieldActionCallbacks = 0;
    struct RhhInnateResolution outsideList = {.struct_size = sizeof(outsideList), .count = 99}, savedList = outsideList;
    struct RhhInnatePriorityContext outsideInnate = {.struct_size = sizeof(outsideInnate), .index = 99}, savedInnate = outsideInnate;
    CHECK(abilities && RHH_ABILITY_SERVICE_V1_30_SIZE < RHH_ABILITY_SERVICE_V1_31_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 30)) == &abilities->header
        && !RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 41))
        && abilities->priority_innates(&outsideList) && !memcmp(&outsideList, &savedList, sizeof(outsideList))
        && abilities->innate_priority_context(&outsideInnate) && !memcmp(&outsideInnate, &savedInnate, sizeof(outsideInnate)));
    const struct RhhBattleService *battle = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 45));
    sBattleEnvironmentService = battle; sEnvironmentEvents = sEnvironmentCommits = sEnvironmentTimerTicks = 0; sEnvironmentConsumeRng = FALSE;
    struct RhhBattleSpeedContext outsideSpeed = {.struct_size = sizeof(outsideSpeed), .hp = 99}, savedSpeed = outsideSpeed;
    struct RhhInnateSpeedContext outsideInnateSpeed = {.struct_size = sizeof(outsideInnateSpeed), .index = 99}, savedInnateSpeed = outsideInnateSpeed;
    CHECK(battle && RHH_BATTLE_SERVICE_V1_30_SIZE < RHH_BATTLE_SERVICE_V1_31_SIZE
        && RHH_ABILITY_SERVICE_V1_31_SIZE < RHH_ABILITY_SERVICE_V1_32_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 46))
        && battle->speed_context(&outsideSpeed) && !memcmp(&outsideSpeed, &savedSpeed, sizeof(outsideSpeed))
        && abilities->speed_innates(&outsideList) && !memcmp(&outsideList, &savedList, sizeof(outsideList))
        && abilities->innate_speed_context(&outsideInnateSpeed) && !memcmp(&outsideInnateSpeed, &savedInnateSpeed, sizeof(outsideInnateSpeed)));
    struct RhhBattleEnvironmentState outsideEnvironment = {.struct_size = sizeof(outsideEnvironment), .weather = 99}, savedEnvironment = outsideEnvironment;
    struct RhhBattleEnvironmentObservation outsideObservation = {.struct_size = sizeof(outsideObservation), .changed = 99}, savedObservation = outsideObservation;
    CHECK(RHH_BATTLE_SERVICE_V1_31_SIZE < RHH_BATTLE_SERVICE_V1_32_SIZE
        && battle->environment(&outsideEnvironment) && !memcmp(&outsideEnvironment, &savedEnvironment, sizeof(outsideEnvironment))
        && battle->environment_observation(&outsideObservation) && !memcmp(&outsideObservation, &savedObservation, sizeof(outsideObservation)));
    struct RhhBattleEnvironmentCommit outsideCommit = {.struct_size = sizeof(outsideCommit), .changed = 99}, savedCommit = outsideCommit;
    struct RhhBattleEnvironmentTimerTick outsideTick = {.struct_size = sizeof(outsideTick), .kind = 99}, savedTick = outsideTick;
    CHECK(RHH_BATTLE_SERVICE_V1_32_SIZE < RHH_BATTLE_SERVICE_V1_33_SIZE
        && RHH_BATTLE_SERVICE_V1_34_SIZE == RHH_BATTLE_SERVICE_V1_33_SIZE
        && RHH_BATTLE_SERVICE_V1_35_SIZE == RHH_BATTLE_SERVICE_V1_34_SIZE
        && RHH_BATTLE_SERVICE_V1_35_SIZE < RHH_BATTLE_SERVICE_V1_36_SIZE
        && RHH_ABILITY_SERVICE_V1_33_SIZE < RHH_ABILITY_SERVICE_V1_34_SIZE
        && abilities->innate_entry_weather(RHH_WEATHER_RAIN) && abilities->innate_entry_terrain(RHH_TERRAIN_GRASSY)
        && abilities->innate_turn_weather(RHH_WEATHER_RAIN) && abilities->innate_turn_terrain(RHH_TERRAIN_GRASSY)
        && battle->environment_commit(&outsideCommit) && !memcmp(&outsideCommit, &savedCommit, sizeof(outsideCommit))
        && battle->environment_timer_tick(&outsideTick) && !memcmp(&outsideTick, &savedTick, sizeof(outsideTick)));
    struct RhhDamageStatContext outsideStat = {.struct_size = sizeof(outsideStat), .power = 99}, savedStat = outsideStat;
    struct RhhInnateDamageStatContext outsideInnateStat = {.struct_size = sizeof(outsideInnateStat), .index = 99}, savedInnateStat = outsideInnateStat;
    CHECK(RHH_MOVE_SERVICE_V1_40_SIZE < RHH_MOVE_SERVICE_V1_41_SIZE
        && RHH_ABILITY_SERVICE_V1_32_SIZE < RHH_ABILITY_SERVICE_V1_33_SIZE
        && moves->damage_stat_context(&outsideStat) && !memcmp(&outsideStat, &savedStat, sizeof(outsideStat))
        && abilities->damage_stat_innates(RHH_INNATE_POWER_ATTACKER, &outsideList) && !memcmp(&outsideList, &savedList, sizeof(outsideList))
        && abilities->innate_damage_stat_context(&outsideInnateStat) && !memcmp(&outsideInnateStat, &savedInnateStat, sizeof(outsideInnateStat)));
    CHECK(GetMovePriority(MOVE_WATER_GUN) == 0);
    RhhModsSetRunContext(123, 456, 0);
    CHECK(GetMovePriority(MOVE_WATER_GUN) == 3 && GetMovePriority(MOVE_SCRATCH) == 0
        && GetMovePriority(MOVE_POUND) == 0 && GetMovePriority(MOVE_GROWL) == -7
        && GetMovePriority(MOVE_STRUGGLE) == 0);
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(GetMovePriority(MOVE_TACKLE) == 0 && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    struct RhhMoveInfo info = {.struct_size = sizeof(info)};
    CHECK(!moves->info(RhhContentKeyOf("rhh:move/water_gun"), &info) && info.priority == 3);
    RhhContentKey custom = RhhContentKeyOf("test:move/priority_probe");
    CHECK(!moves->info(custom, &info) && info.priority == 1);
    u32 slot; CHECK(!RhhModsMoveSlot(custom, 1, &slot) && GetMovePriority(slot) == 1);

    struct BattleScriptsStack environmentScripts = {0}; struct BattleCallbacksStack environmentCallbacks = {0};
    struct BattleResources resources = {.battleScriptsStack = &environmentScripts, .battleCallbackStack = &environmentCallbacks}; struct AiLogicData ai = {0};
    struct AiPartyData parties = {0}; struct BattleHistory history = {0}; struct AiThinkingStruct thinking = {0};
    gBattleResources = &resources; gAiLogicData = &ai; gAiPartyData = &parties; gAiThinkingStruct = &thinking; gBattleHistory = &history;
    PrepareTurnBattle(); gBattleWeather = 0; gFieldTimers.terrain = 0; gFieldTimers.terrainTimer = 0;
    ModsBattleBegin(); CHECK(!sEnvironmentEvents);
    struct RhhBattleEnvironmentState field = {.struct_size = sizeof(field)}, tinyField = {.struct_size = 1, .weather = 99}, savedTinyField = tinyField;
    CHECK(!battle->environment(&field) && field.weather == RHH_WEATHER_NONE && field.terrain == RHH_TERRAIN_NONE
        && battle->environment(&tinyField) && !memcmp(&tinyField, &savedTinyField, sizeof(tinyField)));
    gBattleWeather = B_WEATHER_RAIN_NORMAL; gBattleStruct->weatherDuration = 5;
    gFieldTimers.terrain = B_TERRAIN_GRASSY; gFieldTimers.terrainTimer = 3;
    sEnvironmentConsumeRng = TRUE; rng = gRngValue; rng2 = gRng2Value;
    RhhModsObserveBattleEnvironment(); sEnvironmentConsumeRng = FALSE;
    CHECK(sEnvironmentEvents == 1 && sLastEnvironmentObservation.changed == (RHH_BATTLE_ENV_WEATHER_CHANGED
        | RHH_BATTLE_ENV_TERRAIN_CHANGED | RHH_BATTLE_ENV_WEATHER_TIMER_CHANGED
        | RHH_BATTLE_ENV_TERRAIN_TIMER_CHANGED | RHH_BATTLE_ENV_RAW_WEATHER_CHANGED)
        && sLastEnvironmentObservation.before.weather == RHH_WEATHER_NONE
        && sLastEnvironmentObservation.after.weather == RHH_WEATHER_RAIN
        && sLastEnvironmentObservation.after.effective_weather == RHH_WEATHER_RAIN
        && sLastEnvironmentObservation.after.terrain == RHH_TERRAIN_GRASSY
        && sLastEnvironmentObservation.after.weather_turns == 5 && sLastEnvironmentObservation.after.terrain_turns == 3
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    RhhModsObserveBattleEnvironment(); CHECK(sEnvironmentEvents == 1);
    gBattleMons[2].ability = ABILITY_CLOUD_NINE; RhhModsObserveBattleEnvironment();
    CHECK(sEnvironmentEvents == 2 && sLastEnvironmentObservation.changed == RHH_BATTLE_ENV_SUPPRESSION_CHANGED
        && sLastEnvironmentObservation.after.weather == RHH_WEATHER_RAIN
        && sLastEnvironmentObservation.after.weather_suppressed && sLastEnvironmentObservation.after.effective_weather == RHH_WEATHER_NONE);
    gBattleStruct->weatherDuration = 4; gFieldTimers.terrainTimer = 2; RhhModsObserveBattleEnvironment();
    CHECK(sEnvironmentEvents == 3 && sLastEnvironmentObservation.changed == (RHH_BATTLE_ENV_WEATHER_TIMER_CHANGED | RHH_BATTLE_ENV_TERRAIN_TIMER_CHANGED));
    gBattleWeather = B_WEATHER_SUN_NORMAL; RhhModsObserveBattleEnvironment();
    CHECK(sEnvironmentEvents == 4 && sLastEnvironmentObservation.changed == (RHH_BATTLE_ENV_WEATHER_CHANGED | RHH_BATTLE_ENV_RAW_WEATHER_CHANGED)
        && sLastEnvironmentObservation.after.weather == RHH_WEATHER_SUN && !sLastEnvironmentObservation.after.effective_weather);
    gBattleMons[2].ability = ABILITY_BLAZE; gBattleWeather = 0; gBattleStruct->weatherDuration = 0;
    gFieldTimers.terrain = 0; gFieldTimers.terrainTimer = 0; RhhModsObserveBattleEnvironment();
    CHECK(sEnvironmentEvents == 5 && sLastEnvironmentObservation.after.weather == RHH_WEATHER_NONE
        && !sLastEnvironmentObservation.after.weather_suppressed && sLastEnvironmentObservation.after.terrain == RHH_TERRAIN_NONE);
    gBattleTypeFlags |= BATTLE_TYPE_LINK; field = savedEnvironment;
    CHECK(battle->environment(&field) && !memcmp(&field, &savedEnvironment, sizeof(field)));
    RhhModsObserveBattleEnvironment(); CHECK(sEnvironmentEvents == 5);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; RhhModsObserveBattleEnvironment(); CHECK(sEnvironmentEvents == 5);
    gBattleWeather = B_WEATHER_RAIN_NORMAL; gBattleWeather = 0; RhhModsObserveBattleEnvironment(); CHECK(sEnvironmentEvents == 5);
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); cgb_audio_init(42060); m4aSoundInit();
    gMain.callback1 = EnvironmentTestFrame; NativeFrame(); SDL_Quit();
    CHECK(!gMain.callback1 && sEnvironmentEvents == 6 && sLastEnvironmentObservation.after.weather == RHH_WEATHER_RAIN);
    gBattleWeather = B_WEATHER_RAIN_PRIMAL; RhhModsObserveBattleEnvironment();
    CHECK(sEnvironmentEvents == 7 && sLastEnvironmentObservation.after.weather == RHH_WEATHER_HEAVY_RAIN);
    gBattleWeather = B_WEATHER_RAIN_DOWNPOUR; RhhModsObserveBattleEnvironment();
    CHECK(sEnvironmentEvents == 8 && sLastEnvironmentObservation.changed == RHH_BATTLE_ENV_RAW_WEATHER_CHANGED);
    gBattleWeather = 0; gBattleStruct->weatherDuration = 0; RhhModsObserveBattleEnvironment(); CHECK(sEnvironmentEvents == 9);
    CHECK(battle->environment_observation(&outsideObservation) && !memcmp(&outsideObservation, &savedObservation, sizeof(outsideObservation)));
    sEnvironmentConsumeRng = TRUE; rng = gRngValue; rng2 = gRng2Value;
    CHECK(TryChangeBattleWeatherFromMove(1, BATTLE_WEATHER_RAIN, MOVE_RAIN_DANCE) == WEATHER_FAILURE_SUCCESS);
    sEnvironmentConsumeRng = FALSE;
    CHECK(sEnvironmentCommits == 1 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_MOVE
        && sLastEnvironmentCommit.source_battler == 1 && sLastEnvironmentCommit.move == RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE)
        && !sLastEnvironmentCommit.ability && sLastEnvironmentCommit.before.weather == RHH_WEATHER_NONE
        && sLastEnvironmentCommit.after.weather == RHH_WEATHER_RAIN && sLastEnvironmentCommit.after.weather_turns == 5
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    CHECK(TryChangeBattleWeatherFromMove(1, BATTLE_WEATHER_RAIN, MOVE_RAIN_DANCE) == WEATHER_FAILURE_SAME_WEATHER && sEnvironmentCommits == 1);
    CHECK(TryChangeBattleWeather(1, BATTLE_WEATHER_SUN, ABILITY_DROUGHT) == WEATHER_FAILURE_SUCCESS
        && sEnvironmentCommits == 2 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_ABILITY
        && sLastEnvironmentCommit.ability == RhhContentKeyForId(RHH_ABILITY, ABILITY_DROUGHT)
        && sLastEnvironmentCommit.before.weather == RHH_WEATHER_RAIN && sLastEnvironmentCommit.after.weather == RHH_WEATHER_SUN);
    CHECK(TryChangeBattleTerrainFromAbility(1, B_TERRAIN_GRASSY, ABILITY_GRASSY_SURGE)
        && sEnvironmentCommits == 3 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_ABILITY
        && sLastEnvironmentCommit.ability == RhhContentKeyForId(RHH_ABILITY, ABILITY_GRASSY_SURGE)
        && sLastEnvironmentCommit.after.terrain == RHH_TERRAIN_GRASSY);
    CHECK(!TryChangeBattleTerrainFromAbility(1, B_TERRAIN_GRASSY, ABILITY_GRASSY_SURGE) && sEnvironmentCommits == 3);
    CHECK(TryChangeBattleTerrainFromMove(1, B_TERRAIN_ELECTRIC, MOVE_ELECTRIC_TERRAIN)
        && sEnvironmentCommits == 4 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_MOVE
        && sLastEnvironmentCommit.move == RhhContentKeyForId(RHH_MOVE, MOVE_ELECTRIC_TERRAIN)
        && sLastEnvironmentCommit.before.terrain == RHH_TERRAIN_GRASSY && sLastEnvironmentCommit.after.terrain == RHH_TERRAIN_ELECTRIC);
    ai.aiCalcInProgress = TRUE;
    CHECK(TryChangeBattleTerrainFromMove(1, B_TERRAIN_MISTY, MOVE_MISTY_TERRAIN) && sEnvironmentCommits == 4);
    ai.aiCalcInProgress = FALSE; gFieldTimers.terrain = B_TERRAIN_ELECTRIC;
    gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(TryChangeBattleTerrainFromMove(1, B_TERRAIN_GRASSY, MOVE_GRASSY_TERRAIN) && sEnvironmentCommits == 4);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; gFieldTimers.terrain = B_TERRAIN_ELECTRIC;
    gBattleStruct->weatherDuration = 2; environmentScripts.size = 0;
    sEnvironmentConsumeRng = TRUE; rng = gRngValue; rng2 = gRng2Value;
    CHECK(EndOrContinueWeather() && sEnvironmentTimerTicks == 1 && sEnvironmentCommits == 4
        && sLastEnvironmentTimerTick.kind == RHH_BATTLE_ENV_TIMER_WEATHER
        && sLastEnvironmentTimerTick.before.weather_turns == 2 && sLastEnvironmentTimerTick.after.weather_turns == 1
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sEnvironmentConsumeRng = FALSE;
    gBattleStruct->weatherDuration = 2; ai.aiCalcInProgress = TRUE; environmentScripts.size = 0;
    CHECK(EndOrContinueWeather() && sEnvironmentTimerTicks == 1);
    ai.aiCalcInProgress = FALSE; gBattleStruct->weatherDuration = 2; gBattleTypeFlags |= BATTLE_TYPE_LINK; environmentScripts.size = 0;
    CHECK(EndOrContinueWeather() && sEnvironmentTimerTicks == 1);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    gFieldTimers.terrainTimer = 2; gBattleStruct->eventState.endTurn = ENDTURN_TERRAIN; environmentScripts.size = 0;
    (void)DoEndTurnEffects();
    CHECK(sEnvironmentTimerTicks == 2 && sLastEnvironmentTimerTick.kind == RHH_BATTLE_ENV_TIMER_TERRAIN
        && sLastEnvironmentTimerTick.before.terrain_turns == 2 && sLastEnvironmentTimerTick.after.terrain_turns == 1
        && sEnvironmentCommits == 4);
    gBattleStruct->weatherDuration = 1; environmentScripts.size = 0;
    CHECK(EndOrContinueWeather() && sEnvironmentCommits == 5 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_EXPIRED
        && sLastEnvironmentCommit.source_battler == RHH_BATTLE_NO_BATTLER && sLastEnvironmentCommit.before.weather_turns == 1
        && sLastEnvironmentCommit.after.weather == RHH_WEATHER_NONE && sEnvironmentTimerTicks == 2);
    gFieldTimers.terrainTimer = 1; gBattleStruct->eventState.endTurn = ENDTURN_TERRAIN; environmentScripts.size = 0;
    CHECK(DoEndTurnEffects() && sEnvironmentCommits == 6 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_EXPIRED
        && sLastEnvironmentCommit.before.terrain == RHH_TERRAIN_ELECTRIC && sLastEnvironmentCommit.after.terrain == RHH_TERRAIN_NONE
        && sEnvironmentTimerTicks == 2);
    gBattleWeather = B_WEATHER_RAIN_PRIMAL; gBattleStruct->weatherDuration = 0; environmentScripts.size = 0;
    CHECK(EndOrContinueWeather() && !gBattleStruct->weatherDuration && sEnvironmentTimerTicks == 2 && sEnvironmentCommits == 6);
    gBattleWeather = B_WEATHER_NONE;
    gBattleStruct->eventState.endTurn = 0; environmentScripts.size = 0;
    CHECK(TryChangeBattleTerrainFromMove(1, B_TERRAIN_GRASSY, MOVE_GRASSY_TERRAIN) && sEnvironmentCommits == 7);
    gCurrentMove = MOVE_DEFOG;
    CHECK(TryDefogClear(1, TRUE) && sEnvironmentCommits == 8 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_CLEARED
        && sLastEnvironmentCommit.source_battler == 1 && sLastEnvironmentCommit.move == RhhContentKeyForId(RHH_MOVE, MOVE_DEFOG)
        && !sLastEnvironmentCommit.ability && sLastEnvironmentCommit.before.terrain == RHH_TERRAIN_GRASSY
        && sLastEnvironmentCommit.after.terrain == RHH_TERRAIN_NONE);
    environmentScripts.size = 0; gFieldTimers.terrainTimer = 0;
    gBattleWeather = B_WEATHER_RAIN_NORMAL; gBattleStruct->weatherDuration = 5; gBattlescriptCurrInstr = BattleScript_EffectHit; gBattlerAttacker = 1;
    BS_RemoveWeatherTeraformZero(); CHECK(sEnvironmentCommits == 9 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_CLEARED
        && sLastEnvironmentCommit.source_battler == 1 && !sLastEnvironmentCommit.move
        && sLastEnvironmentCommit.ability == RhhContentKeyForId(RHH_ABILITY, ABILITY_TERAFORM_ZERO)
        && sLastEnvironmentCommit.after.weather == RHH_WEATHER_NONE);
    BS_RemoveWeatherTeraformZero(); CHECK(sEnvironmentCommits == 9);
    gFieldTimers.terrain = B_TERRAIN_MISTY; gBattlescriptCurrInstr = BattleScript_EffectHit; gCurrentMove = MOVE_ICE_SPINNER;
    BS_RemoveTerrainMove(); CHECK(sEnvironmentCommits == 10 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_CLEARED
        && sLastEnvironmentCommit.source_battler == 1 && sLastEnvironmentCommit.move == RhhContentKeyForId(RHH_MOVE, MOVE_ICE_SPINNER)
        && !sLastEnvironmentCommit.ability && sLastEnvironmentCommit.after.terrain == RHH_TERRAIN_NONE);
    BS_RemoveTerrainMove(); CHECK(sEnvironmentCommits == 10);
    gBattleWeather = B_WEATHER_FOG; environmentScripts.size = 0; gCurrentMove = MOVE_DEFOG;
    CHECK(TryDefogClear(1, FALSE) && sEnvironmentCommits == 10 && (gBattleWeather & B_WEATHER_FOG));
    CHECK(TryDefogClear(1, TRUE) && sEnvironmentCommits == 11 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_CLEARED
        && sLastEnvironmentCommit.source_battler == 1 && sLastEnvironmentCommit.move == RhhContentKeyForId(RHH_MOVE, MOVE_DEFOG)
        && !sLastEnvironmentCommit.ability && sLastEnvironmentCommit.after.weather == RHH_WEATHER_NONE);
    gBattleWeather = B_WEATHER_RAIN_PRIMAL; gBattleMons[1].ability = ABILITY_PRIMORDIAL_SEA;
    gBattlescriptCurrInstr = BattleScript_EffectHit; BS_TryToClearPrimalWeather(); CHECK(sEnvironmentCommits == 11 && gBattleWeather);
    gBattleMons[1].ability = ABILITY_NONE; gBattlerAttacker = 1; gCurrentMove = MOVE_TACKLE;
    gBattlescriptCurrInstr = BattleScript_EffectHit; BS_TryToClearPrimalWeather();
    CHECK(sEnvironmentCommits == 12 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_PRIMAL_ENDED
        && sLastEnvironmentCommit.before.weather == RHH_WEATHER_HEAVY_RAIN && sLastEnvironmentCommit.after.weather == RHH_WEATHER_NONE);
    gBattleControllerExecFlags = 0; environmentScripts.size = environmentCallbacks.size = 0;
    u8 oldWeather = gWeatherPtr->currWeather, oldGroup = gSaveBlock1Ptr->location.mapGroup, oldMap = gSaveBlock1Ptr->location.mapNum;
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE101); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE101);
    memset(&gStartingStatuses, 0, sizeof(gStartingStatuses)); gWeatherPtr->currWeather = WEATHER_RAIN;
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && sEnvironmentCommits == 13
        && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_NATURAL_WEATHER
        && sLastEnvironmentCommit.source_battler == RHH_BATTLE_NO_BATTLER && !sLastEnvironmentCommit.move && !sLastEnvironmentCommit.ability
        && sLastEnvironmentCommit.after.weather == RHH_WEATHER_RAIN);
    CHECK(!TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && sEnvironmentCommits == 13);
    environmentScripts.size = environmentCallbacks.size = 0; gWeatherPtr->currWeather = WEATHER_RAIN_THUNDERSTORM;
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_TERRAIN) && sEnvironmentCommits == 14
        && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_NATURAL_WEATHER
        && sLastEnvironmentCommit.after.terrain == RHH_TERRAIN_ELECTRIC);
    environmentScripts.size = environmentCallbacks.size = 0; gWeatherPtr->currWeather = WEATHER_NONE;
    gBattleWeather = 0; gFieldTimers.terrain = B_TERRAIN_NONE;
    gStartingStatuses.weatherSunTemporary = TRUE; gStartingStatuses.mistyTerrainTemporary = TRUE;
    sTestSeed = TRUE; sTestSeedDeny = FALSE; sTestSeedCalls = 0;
    CHECK(!TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && !TryFieldEffects(FIELD_EFFECT_OVERWORLD_TERRAIN)
        && !sTestSeedCalls && sEnvironmentCommits == 14);
    CHECK(TryFieldEffects(FIELD_EFFECT_TRAINER_STATUSES) && sEnvironmentCommits == 15
        && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_STARTING_STATUS
        && sLastEnvironmentCommit.after.terrain == RHH_TERRAIN_MISTY && sLastEnvironmentCommit.after.terrain_turns == 5);
    CHECK(TryFieldEffects(FIELD_EFFECT_TRAINER_STATUSES) && sEnvironmentCommits == 16
        && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_STARTING_STATUS
        && sLastEnvironmentCommit.after.weather == RHH_WEATHER_SUN && sLastEnvironmentCommit.after.weather_turns == 5);
    environmentScripts.size = environmentCallbacks.size = 0; gBattleWeather = 0;
    gFieldTimers.terrain = B_TERRAIN_NONE; gFieldTimers.terrainTimer = 0;
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && sEnvironmentCommits == 17
        && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_WORLD_SEED
        && sLastEnvironmentCommit.after.weather == RHH_WEATHER_RAIN && !sLastEnvironmentCommit.after.weather_turns);
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_TERRAIN) && sEnvironmentCommits == 18
        && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_WORLD_SEED
        && sLastEnvironmentCommit.after.terrain == RHH_TERRAIN_GRASSY && !sLastEnvironmentCommit.after.terrain_turns
        && sTestSeedCalls == 2);
    environmentScripts.size = environmentCallbacks.size = 0; gBattleWeather = 0; gFieldTimers.terrain = B_TERRAIN_NONE;
    sTestSeedDeny = TRUE;
    CHECK(!TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && !TryFieldEffects(FIELD_EFFECT_OVERWORLD_TERRAIN)
        && sEnvironmentCommits == 18 && sTestSeedCalls == 4);
    sTestSeed = sTestSeedDeny = FALSE; gWeatherPtr->currWeather = oldWeather;
    gSaveBlock1Ptr->location.mapGroup = oldGroup; gSaveBlock1Ptr->location.mapNum = oldMap;
    gBattleWeather = 0; gBattleStruct->weatherDuration = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers));
    environmentScripts.size = environmentCallbacks.size = 0;
    CHECK(battle->environment_commit(&outsideCommit) && !memcmp(&outsideCommit, &savedCommit, sizeof(outsideCommit)));
    gBattleStruct->overworldWeatherPresent = FALSE; gBattleMons[1].ability = ABILITY_NONE;
    sFieldActionMode = 1; u32 savedScriptingBattler = gBattleScripting.battler;
    CHECK(ModsInnateSwitchIn(1) && sFieldActionCallbacks == 1);
    CHECK(ModsInnateNextEnvironment() == MOD_INNATE_ENV_WEATHER && sEnvironmentCommits == 19
        && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_INNATE
        && sLastEnvironmentCommit.source_battler == 1 && sLastEnvironmentCommit.ability == RhhContentKeyForId(RHH_ABILITY, ABILITY_TORRENT)
        && gBattleWeather == B_WEATHER_RAIN_NORMAL && gBattleStruct->weatherDuration == 5);
    CHECK(ModsInnateNextEnvironment() == MOD_INNATE_ENV_TERRAIN && sEnvironmentCommits == 20
        && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_INNATE && gFieldTimers.terrain == B_TERRAIN_ELECTRIC
        && gFieldTimers.terrainTimer == 5);
    CHECK(ModsInnateNextEnvironment() == MOD_INNATE_ENV_DONE && gBattleScripting.battler == savedScriptingBattler);
    ModsBattleSwitchPrepared(1); ModsBattleSwitchEntered(1); CHECK(ModsInnateSwitchIn(1));
    gBattleMons[1].volatiles.gastroAcid = TRUE;
    CHECK(ModsInnateNextEnvironment() == MOD_INNATE_ENV_DONE && sEnvironmentCommits == 20);
    gBattleMons[1].volatiles.gastroAcid = FALSE;
    sFieldActionMode = 2; gBattleTurnCounter = 1;
    CHECK(ModsBattleTurnEnded() && sFieldActionCallbacks == 3);
    u32 status = 0, source = 0;
    CHECK(ModsInnateTurnNextAction(&status, &source) == MOD_INNATE_TURN_WEATHER && sEnvironmentCommits == 21
        && sLastEnvironmentCommit.ability == RhhContentKeyForId(RHH_ABILITY, ABILITY_TORRENT)
        && gBattleWeather == B_WEATHER_SUN_NORMAL);
    CHECK(ModsInnateTurnNextAction(&status, &source) == MOD_INNATE_TURN_TERRAIN && sEnvironmentCommits == 22
        && gFieldTimers.terrain == B_TERRAIN_GRASSY);
    CHECK(ModsInnateTurnNextAction(&status, &source) == MOD_INNATE_TURN_DONE && gBattleScripting.battler == savedScriptingBattler);
    ModsFinishInnateTurnActions(); sFieldActionMode = 1;
    gBattleWeather = 0; gBattleStruct->weatherDuration = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers));
    ModsBattleSwitchPrepared(1); ModsBattleSwitchEntered(1); CHECK(ModsInnateSwitchIn(1));
    environmentScripts.size = 0; gBattlescriptCurrInstr = BattleScript_ModInnateMessages;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateEnvironmentWeather && gBattleWeather == B_WEATHER_RAIN_NORMAL);
    RunInnateEnvironmentPresentation(BattleScript_ModInnateMessages);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateEnvironmentTerrain && gFieldTimers.terrain == B_TERRAIN_ELECTRIC);
    RunInnateEnvironmentPresentation(BattleScript_ModInnateMessages);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateMessagesEnd && !environmentScripts.size
        && gBattleScripting.battler == savedScriptingBattler);
    sFieldActionMode = 2; gBattleTurnCounter = 2; CHECK(ModsBattleTurnEnded());
    gBattlescriptCurrInstr = BattleScript_ModInnateTurnAction; gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateEnvironmentWeather && gBattleWeather == B_WEATHER_SUN_NORMAL);
    RunInnateEnvironmentPresentation(BattleScript_ModInnateTurnAction);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModInnateEnvironmentTerrain && gFieldTimers.terrain == B_TERRAIN_GRASSY);
    RunInnateEnvironmentPresentation(BattleScript_ModInnateTurnAction);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModConditionMessages && !environmentScripts.size
        && gBattleScripting.battler == savedScriptingBattler);
    sFieldActionMode = 1; gBattleWeather = 0; gBattleStruct->overworldWeatherPresent = TRUE;
    gFieldTimers.terrain = B_TERRAIN_NONE; gBattleStruct->isSkyBattle = TRUE;
    ModsBattleSwitchPrepared(1); ModsBattleSwitchEntered(1); CHECK(ModsInnateSwitchIn(1));
    CHECK(ModsInnateNextEnvironment() == MOD_INNATE_ENV_DONE && !gBattleWeather && !gFieldTimers.terrain);
    gBattleStruct->overworldWeatherPresent = FALSE; gBattleStruct->isSkyBattle = FALSE;
    sFieldActionMode = 3; sDropTorrent = FALSE;
    ModsBattleSwitchPrepared(1); ModsBattleSwitchEntered(1); CHECK(ModsInnateSwitchIn(1));
    sDropTorrent = TRUE; u32 beforeReorderedCommits = sEnvironmentCommits;
    CHECK(ModsInnateNextEnvironment() == MOD_INNATE_ENV_WEATHER && sEnvironmentCommits == beforeReorderedCommits + 1
        && sLastEnvironmentCommit.ability == RhhContentKeyForId(RHH_ABILITY, ABILITY_LEVITATE));
    CHECK(ModsInnateNextEnvironment() == MOD_INNATE_ENV_TERRAIN && sEnvironmentCommits == beforeReorderedCommits + 2
        && sLastEnvironmentCommit.ability == RhhContentKeyForId(RHH_ABILITY, ABILITY_LEVITATE));
    CHECK(ModsInnateNextEnvironment() == MOD_INNATE_ENV_DONE); sDropTorrent = FALSE;
    u32 beforeTeraformClear = sEnvironmentCommits;
    gFieldTimers.terrain = B_TERRAIN_GRASSY; gBattlerAttacker = 1; gBattlescriptCurrInstr = BattleScript_EffectHit;
    BS_RemoveTerrainTeraformZero();
    CHECK(sEnvironmentCommits == beforeTeraformClear + 1 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_CLEARED
        && sLastEnvironmentCommit.source_battler == 1 && !sLastEnvironmentCommit.move
        && sLastEnvironmentCommit.ability == RhhContentKeyForId(RHH_ABILITY, ABILITY_TERAFORM_ZERO)
        && sLastEnvironmentCommit.after.terrain == RHH_TERRAIN_NONE);
    BS_RemoveTerrainTeraformZero(); CHECK(sEnvironmentCommits == beforeTeraformClear + 1);
    gFieldTimers.terrain = B_TERRAIN_MISTY; gBattlescriptCurrInstr = BattleScript_EffectHit;
    BS_RemoveTerrain();
    CHECK(sEnvironmentCommits == beforeTeraformClear + 2 && sLastEnvironmentCommit.source_battler == RHH_BATTLE_NO_BATTLER
        && !sLastEnvironmentCommit.move && !sLastEnvironmentCommit.ability);
    gBattleWeather = B_WEATHER_RAIN_NORMAL; gBattlescriptCurrInstr = BattleScript_EffectHit;
    BS_RemoveWeather();
    CHECK(sEnvironmentCommits == beforeTeraformClear + 3 && sLastEnvironmentCommit.source_battler == RHH_BATTLE_NO_BATTLER
        && !sLastEnvironmentCommit.move && !sLastEnvironmentCommit.ability);
    sFieldActionMode = 0; gBattleTurnCounter = 0;
    gBattleWeather = 0; gBattleStruct->weatherDuration = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers));
    RhhModsObserveBattleEnvironment();
    gBattleMons[0].moves[0] = MOVE_TACKLE; gBattleMons[1].moves[0] = MOVE_WATER_GUN;
    gBattleMons[0].speed = 200; gBattleMons[1].speed = 10;
    gChosenActionByBattler[0] = gChosenActionByBattler[1] = B_ACTION_USE_MOVE;
    gBattleStruct->chosenMovePositions[0] = gBattleStruct->chosenMovePositions[1] = 0;
    struct BattleCalcValues cv = {.battlerAtk = 1, .battlerDef = 0};
    CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 3
        && GetWhichBattlerFasterOrTies(&cv, FALSE) == 1);
    ai.speedStats[0] = 200; ai.speedStats[1] = 10;
    CHECK(AI_WhoStrikesFirst(1, 0, MOVE_WATER_GUN, MOVE_TACKLE, CONSIDER_PRIORITY) == AI_IS_FASTER);
    CHECK(GetBattleMovePriority(0, ABILITY_PRANKSTER, MOVE_GROWL) == -6);
    u32 *mode = RhhModsState("test:move_priority"); CHECK(mode); *mode = 1;
    gBattleMons[1].hp = gBattleMons[1].maxHP;
    CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 4);
    gBattleMons[1].hp = gBattleMons[1].maxHP / 2;
    CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == -2
        && GetWhichBattlerFasterOrTies(&cv, FALSE) == -1
        && AI_WhoStrikesFirst(1, 0, MOVE_WATER_GUN, MOVE_TACKLE, CONSIDER_PRIORITY) == AI_IS_SLOWER);
    *mode = 8; CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 3);
    *mode = 9; CHECK(GetAiBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 3
        && GetAiChosenMovePriority(1, ABILITY_NONE) == 3
        && AI_WhoStrikesFirst(1, 0, MOVE_WATER_GUN, MOVE_TACKLE, CONSIDER_PRIORITY) == AI_IS_FASTER);
    ai.aiCalcInProgress = TRUE; CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 3);
    ai.aiCalcInProgress = FALSE;
    *mode = 2; gBattleMons[1].hp = gBattleMons[1].maxHP; gBattleMons[1].ability = ABILITY_GALE_WINGS;
    CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_GUST) == 0
        && GetBattleMovePriority(1, ABILITY_GALE_WINGS, MOVE_GUST) == 2);
    *mode = 3; gProtectStructs[0].pranksterElevated = FALSE;
    CHECK(GetBattleMovePriority(0, ABILITY_PRANKSTER, MOVE_GROWL) == -5 && gProtectStructs[0].pranksterElevated);
    *mode = 4; CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 3);
    *mode = 5; CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 3);
    *mode = 6; sImpureBattlerPriority = TRUE; rng = gRngValue; rng2 = gRng2Value;
    CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_SCRATCH) == 0
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sImpureBattlerPriority = FALSE;
    *mode = 1; u32 dispatches = sBattlerPriorityDispatches;
    gProtectStructs[0].quash = TRUE;
    CHECK(GetBattleMovePriority(0, ABILITY_NONE, MOVE_WATER_GUN) == -8
        && GetBattleMovePriority(0, ABILITY_NONE, MOVE_STRUGGLE) == -8 && sBattlerPriorityDispatches == dispatches);
    gProtectStructs[0].quash = FALSE;
    CHECK(GetBattleMovePriority(0, ABILITY_NONE, MOVE_STRUGGLE) == 0 && sBattlerPriorityDispatches == dispatches);
    *mode = 7; SetActiveGimmick(0, GIMMICK_DYNAMAX);
    CHECK(GetBattleMovePriority(0, ABILITY_NONE, MOVE_GROWL) == GetMovePriority(MOVE_MAX_GUARD) - 1);
    *mode = 11; CHECK(GetAiBattleMovePriority(0, ABILITY_NONE, MOVE_GROWL) == GetMovePriority(MOVE_MAX_GUARD) - 1);
    SetActiveGimmick(0, GIMMICK_NONE);
    *mode = 12; gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].level = 20;
    CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 6
        && GetAiBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 6);
    gBattleMons[1].level = 9; CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 3);
    gBattleMons[1].level = 50; CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 9);
    gBattleMons[1].level = 20; gBattleMons[1].ability = ABILITY_TORRENT;
    CHECK(GetBattleMovePriority(1, ABILITY_TORRENT, MOVE_WATER_GUN) == 5);
    ai.abilities[1] = ABILITY_TORRENT;
    CHECK(GetAiBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 6);
    ai.abilities[1] = ABILITY_NONE;
    CHECK(GetAiBattleMovePriority(1, ABILITY_TORRENT, MOVE_WATER_GUN) == 5);
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].volatiles.gastroAcid = TRUE;
    CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 3);
    gBattleMons[1].volatiles.gastroAcid = FALSE; gBattleMons[2].volatiles.neutralizingGas = TRUE;
    ai.holdEffects[1] = HOLD_EFFECT_NONE;
    CHECK(GetAiBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 3);
    ai.holdEffects[1] = HOLD_EFFECT_ABILITY_SHIELD;
    CHECK(GetAiBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 6);
    ai.holdEffects[1] = HOLD_EFFECT_NONE; gBattleMons[2].volatiles.neutralizingGas = FALSE;
    *mode = 13; CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 5);
    *mode = 14; CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 5);
    *mode = 15; sImpureInnatePriority = TRUE; rng = gRngValue; rng2 = gRng2Value;
    CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 5
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sImpureInnatePriority = FALSE;
    *mode = 16; gBattleMons[1].level = 20; gBattleMons[1].ability = ABILITY_NONE;
    CHECK(GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 23
        && GetAiBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 23);
    ai.aiCalcInProgress = TRUE; CHECK(GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 23);
    ai.aiCalcInProgress = FALSE;
    gBattleMons[1].level = 9; CHECK(GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 20);
    gBattleMons[1].level = 50; CHECK(GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 26);
    gBattleMons[1].level = 20; ai.abilities[1] = ABILITY_TORRENT;
    CHECK(GetAiBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 23
        && GetAiBattlerTotalSpeedStat(1, ABILITY_TORRENT, HOLD_EFFECT_NONE) == 22);
    ai.abilities[1] = ABILITY_NONE;
    gBattleMons[1].volatiles.gastroAcid = TRUE;
    CHECK(GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 20);
    gBattleMons[1].volatiles.gastroAcid = FALSE; gBattleMons[2].volatiles.neutralizingGas = TRUE;
    CHECK(GetAiBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 20
        && GetAiBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_ABILITY_SHIELD) == 23);
    gBattleMons[2].volatiles.neutralizingGas = FALSE;
    *mode = 17; CHECK(GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 10);
    *mode = 18; CHECK(GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 10);
    *mode = 19; sImpureSpeed = TRUE; rng = gRngValue; rng2 = gRng2Value;
    CHECK(GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 10
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sImpureSpeed = FALSE;
    *mode = 20; CHECK(GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 12);
    *mode = 21; sImpureInnateSpeed = TRUE; rng = gRngValue; rng2 = gRng2Value;
    CHECK(GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 12
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sImpureInnateSpeed = FALSE;
    gBattleMons[0].speed = 22;
    *mode = 22; CHECK(GetAiWhichBattlerFasterOrTies(&cv, TRUE) == 1);
    *mode = 23; CHECK(GetWhichBattlerFasterOrTies(&cv, TRUE) == 1);
    gBattleMons[0].speed = 200;
    struct DamageContext damage = {.battlerAtk = 1, .battlerDef = 0, .move = MOVE_TACKLE,
        .chosenMove = MOVE_TACKLE, .baseMove = MOVE_TACKLE, .moveType = TYPE_NORMAL, .fixedBasePower = 50};
    CHECK(!AI_ShouldAssumeCritical(CRITICAL_HIT_BLOCKED, GEN_1, TRUE)
        && AI_ShouldAssumeCritical(CRITICAL_HIT_ALWAYS, GEN_1, FALSE)
        && !AI_ShouldAssumeCritical(255, GEN_1, FALSE)
        && AI_ShouldAssumeCritical(255, GEN_1, TRUE)
        && !AI_ShouldAssumeCritical(256, GEN_1, TRUE)
        && !AI_ShouldAssumeCritical(4, GEN_2, FALSE)
        && !AI_ShouldAssumeCritical(4, GEN_3, FALSE)
        && AI_ShouldAssumeCritical(3, GEN_6, FALSE)
        && !AI_ShouldAssumeCritical(5, GEN_7, TRUE));
    sCriticalMode = 1; CHECK(ModsMoveCriticalStage(&damage, 0, GEN_1) == 1
        && ModsMoveCriticalStage(&damage, 0, GEN_2) == 1 && ModsMoveCriticalStage(&damage, 0, GEN_7) == 1);
    sCriticalMode = 2; CHECK(ModsMoveCriticalStage(&damage, 0, GEN_7) == 0);
    sCriticalMode = 3; rng = gRngValue; rng2 = gRng2Value;
    CHECK(ModsMoveCriticalStage(&damage, 0, GEN_7) == 0 && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sCriticalMode = 4; CHECK(ModsMoveCriticalStage(&damage, 0, GEN_7) == 0);
    sCriticalMode = 5; sCriticalInnateCalls = 0;
    CHECK(ModsMoveCriticalStage(&damage, 0, GEN_7) == 2 && sCriticalInnateCalls == 2);
    sCriticalMode = 6; rng = gRngValue; rng2 = gRng2Value;
    CHECK(ModsMoveCriticalStage(&damage, 0, GEN_7) == 0 && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sCriticalMode = 1; u32 criticalCalls = sCriticalCalls;
    CHECK(ModsMoveCriticalStage(&damage, CRITICAL_HIT_BLOCKED, GEN_7) == CRITICAL_HIT_BLOCKED
        && ModsMoveCriticalStage(&damage, CRITICAL_HIT_ALWAYS, GEN_7) == CRITICAL_HIT_ALWAYS
        && sCriticalCalls == criticalCalls);
    damage.aiCalc = TRUE; CHECK(ModsMoveCriticalStage(&damage, 0, GEN_7) == 1 && sCriticalLastFlags == RHH_CRITICAL_AI);
    damage.aiCalc = FALSE; sCriticalMode = 0;
    s32 normalCritical = CalcCritChanceStage(&damage), gen1Critical = CalcCritChanceStageGen1(&damage);
    sCriticalMode = 1;
    CHECK(CalcCritChanceStage(&damage) == normalCritical + 1 && CalcCritChanceStageGen1(&damage) == gen1Critical + 1);
    criticalCalls = sCriticalCalls;
    gSideStatuses[GetBattlerSide(damage.battlerDef)] |= SIDE_STATUS_LUCKY_CHANT;
    CHECK(CalcCritChanceStage(&damage) == CRITICAL_HIT_BLOCKED
        && CalcCritChanceStageGen1(&damage) == CRITICAL_HIT_BLOCKED && sCriticalCalls == criticalCalls);
    gSideStatuses[GetBattlerSide(damage.battlerDef)] &= ~SIDE_STATUS_LUCKY_CHANT;
    gBattleMons[damage.battlerAtk].volatiles.laserFocusTimer = 1;
    CHECK(CalcCritChanceStage(&damage) == CRITICAL_HIT_ALWAYS
        && CalcCritChanceStageGen1(&damage) == CRITICAL_HIT_ALWAYS && sCriticalCalls == criticalCalls);
    gBattleMons[damage.battlerAtk].volatiles.laserFocusTimer = 0; sCriticalMode = 0;
    damage.typeEffectivenessModifier = UQ_4_12(1);
    sFinalMode = 1; CHECK(ModsFinalDamage(&damage, 100) == 200);
    sFinalMode = 2; CHECK(ModsFinalDamage(&damage, 100) == 100);
    sFinalMode = 3; CHECK(ModsFinalDamage(&damage, 100) == 100);
    sFinalMode = 4; rng = gRngValue; rng2 = gRng2Value;
    CHECK(ModsFinalDamage(&damage, 100) == 100 && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sFinalMode = 7; CHECK(ModsFinalDamage(&damage, 100) == 100);
    sFinalMode = 8; CHECK(ModsFinalDamage(&damage, 100) == 200);
    sFinalMode = 5; sFinalInnateCalls = 0;
    CHECK(ModsFinalDamage(&damage, 100) == 102 && sFinalInnateCalls == 2);
    damage.abilities[1] = ABILITY_TORRENT; CHECK(ModsFinalDamage(&damage, 100) == 101);
    damage.abilities[1] = ABILITY_NONE; gBattleMons[1].volatiles.gastroAcid = TRUE;
    CHECK(ModsFinalDamage(&damage, 100) == 100); gBattleMons[1].volatiles.gastroAcid = FALSE;
    sFinalMode = 6; rng = gRngValue; rng2 = gRng2Value;
    CHECK(ModsFinalDamage(&damage, 100) == 101 && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    damage.aiCalc = TRUE; damage.isCrit = TRUE; sFinalMode = 1;
    CHECK(ModsFinalDamage(&damage, 100) == 200
        && sFinalLastFlags == (RHH_FINAL_DAMAGE_AI | RHH_FINAL_DAMAGE_CRITICAL));
    damage.aiCalc = damage.isCrit = FALSE;
    damage.battlerDef = damage.battlerAtk; damage.isSelfInflicted = TRUE; sFinalMode = 5; sFinalInnateCalls = 0;
    CHECK(ModsFinalDamage(&damage, 100) == 102 && sFinalInnateCalls == 2
        && sFinalLastFlags == RHH_FINAL_DAMAGE_SELF_INFLICTED);
    damage.battlerDef = 0; damage.isSelfInflicted = FALSE;
    sFinalMode = 1; u32 finalCalls = sFinalCalls;
    CHECK(!ModsFinalDamage(&damage, 0)); damage.typeEffectivenessModifier = 0;
    CHECK(ModsFinalDamage(&damage, 100) == 100 && sFinalCalls == finalCalls);
    damage.typeEffectivenessModifier = UQ_4_12(1);
    gBattleTypeFlags |= BATTLE_TYPE_LINK; CHECK(ModsFinalDamage(&damage, 100) == 100 && sFinalCalls == finalCalls);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    sFinalMode = 0; s32 finalBase = ApplyModifiersAfterDmgRoll(&damage, 100);
    sFinalMode = 1; CHECK(ApplyModifiersAfterDmgRoll(&damage, 100) == finalBase * 2);
    *mode = 0; damage.randomFactor = TRUE; rng = gRngValue; rng2 = gRng2Value; sFinalMode = 0;
    s32 combatBase = CalculateMoveDamage(&damage);
    gRngValue = rng; gRng2Value = rng2; sFinalMode = 1;
    CHECK(combatBase > 0 && CalculateMoveDamage(&damage) == combatBase * 2);
    damage.randomFactor = FALSE; finalCalls = sFinalCalls; damage.move = MOVE_SEISMIC_TOSS; damage.moveType = TYPE_FIGHTING;
    CHECK(CalculateMoveDamage(&damage) > 0 && sFinalCalls == finalCalls);
    damage.move = MOVE_TACKLE; damage.moveType = TYPE_NORMAL; damage.randomFactor = TRUE;
    struct BattlePokemon finalTargetBefore = gBattleMons[0]; SET_BATTLER_TYPE(0, TYPE_GHOST);
    CHECK(!CalculateMoveDamage(&damage) && sFinalCalls == finalCalls);
    gBattleMons[0] = finalTargetBefore;
    struct AiCalcValues finalAi = {.move = MOVE_TACKLE};
    rng = gRngValue; rng2 = gRng2Value; sFinalMode = 0;
    struct SimulatedDamage ordinaryAi = AI_CalcDamage(&finalAi, 1, 0);
    gRngValue = rng; gRng2Value = rng2; sFinalMode = 1;
    struct SimulatedDamage modifiedAi = AI_CalcDamage(&finalAi, 1, 0);
    CHECK(ordinaryAi.median > 0 && modifiedAi.minimum == ordinaryAi.minimum * 2
        && modifiedAi.median == ordinaryAi.median * 2 && modifiedAi.maximum == ordinaryAi.maximum * 2
        && (sFinalLastFlags & RHH_FINAL_DAMAGE_AI));
    enum BattleTrainer beatTrainer = GetBattlerTrainer(1);
    struct AiPartyData savedPartyKnowledge = parties;
    parties.count[beatTrainer] = 3;
    parties.mons[beatTrainer][0].species = SPECIES_CHARIZARD;
    parties.mons[beatTrainer][0].wasSentInBattle = TRUE;
    parties.mons[beatTrainer][1].species = SPECIES_SLAKING;
    parties.mons[beatTrainer][1].wasSentInBattle = TRUE;
    parties.mons[beatTrainer][2].species = SPECIES_NONE;
    for (u32 i = 0; i < PARTY_SIZE; ++i) gBattleStruct->beatUpSpecies[i] = SPECIES_BULBASAUR + i;
    enum Species beatSaved[PARTY_SIZE]; memcpy(beatSaved, gBattleStruct->beatUpSpecies, sizeof(beatSaved));
    gBattleStruct->beatUpSlot = 4;
    struct AiCalcValues beatAi = {.move = MOVE_BEAT_UP};
    rng = gRngValue; rng2 = gRng2Value; sFinalMode = 0;
    struct SimulatedDamage beatBase = AI_CalcDamage(&beatAi, 1, 0);
    CHECK(beatBase.minimum > 0 && beatBase.minimum <= beatBase.median && beatBase.median <= beatBase.maximum
        && gBattleStruct->beatUpSlot == 4 && !memcmp(beatSaved, gBattleStruct->beatUpSpecies, sizeof(beatSaved)));
    parties.mons[beatTrainer][1].species = SPECIES_MAGIKARP;
    gRngValue = rng; gRng2Value = rng2;
    struct SimulatedDamage beatWeaker = AI_CalcDamage(&beatAi, 1, 0);
    CHECK(beatWeaker.median < beatBase.median);
    parties.mons[beatTrainer][1].species = SPECIES_SLAKING;
    gRngValue = rng; gRng2Value = rng2; sFinalMode = 1; finalCalls = sFinalCalls;
    *mode = 42; sCaptureBeat = TRUE; sBeatCaptureCount = sBeatStageMask = 0;
    struct SimulatedDamage beatModified = AI_CalcDamage(&beatAi, 1, 0);
    sCaptureBeat = FALSE; *mode = 0;
    CHECK(beatModified.minimum == beatBase.minimum * 2 && beatModified.median == beatBase.median * 2
        && beatModified.maximum == beatBase.maximum * 2 && beatModified.random == beatBase.random * 2
        && sFinalCalls == finalCalls + 12 && (sFinalLastFlags & RHH_FINAL_DAMAGE_AI) && gBattleStruct->beatUpSlot == 4
        && !memcmp(beatSaved, gBattleStruct->beatUpSpecies, sizeof(beatSaved)));
    CHECK(sBeatCaptureCount > 12 && sBeatStageMask == 63);
    for (u32 i = 0; i < sBeatCaptureCount; ++i)
    {
        const struct RhhBeatUpContributor *c = &sBeatCaptured[i];
        CHECK(c->attacker == 1 && c->defender == 0 && c->hit_index < 3
            && (c->flags & RHH_BEAT_UP_CONTRIBUTOR_AI));
        if (c->hit_index == 0)
            CHECK(c->species == RhhContentKeyForId(RHH_SPECIES, SPECIES_CHARIZARD) && c->party_slot == 0
                && (c->flags & RHH_BEAT_UP_CONTRIBUTOR_APPARENT));
        else if (c->hit_index == 1)
            CHECK(c->species == RhhContentKeyForId(RHH_SPECIES, SPECIES_SLAKING) && c->party_slot == 1
                && !(c->flags & RHH_BEAT_UP_CONTRIBUTOR_ESTIMATED));
        else
            CHECK(c->species == RhhContentKeyForId(RHH_SPECIES, SPECIES_CHARIZARD)
                && c->party_slot == RHH_BEAT_UP_CONTRIBUTOR_NO_SLOT
                && (c->flags & RHH_BEAT_UP_CONTRIBUTOR_ESTIMATED));
    }
    struct Pokemon hiddenBefore = gParties[beatTrainer][2];
    CreateMonWithIVs(&gParties[beatTrainer][2], SPECIES_MAGIKARP, 20, 17, OTID_STRUCT_PRESET(23), 31);
    sFinalMode = 0; rng = gRngValue; rng2 = gRng2Value;
    struct SimulatedDamage hiddenFirst = AI_CalcDamage(&beatAi, 1, 0);
    CreateMonWithIVs(&gParties[beatTrainer][2], SPECIES_SLAKING, 20, 17, OTID_STRUCT_PRESET(23), 31);
    gRngValue = rng; gRng2Value = rng2;
    struct SimulatedDamage hiddenSecond = AI_CalcDamage(&beatAi, 1, 0);
    CHECK(!memcmp(&hiddenFirst, &hiddenSecond, sizeof(hiddenFirst)));
    gParties[beatTrainer][2] = hiddenBefore; sFinalMode = 9;
    struct SimulatedDamage beatCapped = AI_CalcDamage(&beatAi, 1, 0);
    CHECK(beatCapped.minimum == 65535 && beatCapped.median == 65535
        && beatCapped.maximum == 65535 && beatCapped.random == 65535);
    sFinalMode = 1;
    parties.mons[beatTrainer][1].isFainted = TRUE;
    finalCalls = sFinalCalls; (void)AI_CalcDamage(&beatAi, 1, 0); CHECK(sFinalCalls == finalCalls + 8);
    parties.mons[beatTrainer][1].isFainted = FALSE;
    parties.mons[beatTrainer][1].status = STATUS1_BURN;
    finalCalls = sFinalCalls; (void)AI_CalcDamage(&beatAi, 1, 0); CHECK(sFinalCalls == finalCalls + 8);
    struct DamageContext beatHit = {.battlerAtk = 1, .battlerDef = 0, .move = MOVE_BEAT_UP,
        .chosenMove = MOVE_BEAT_UP, .baseMove = MOVE_BEAT_UP, .moveType = TYPE_DARK, .randomFactor = TRUE};
    gBattleStruct->beatUpSpecies[0] = SPECIES_SLAKING; gBattleStruct->beatUpSlot = 0;
    rng = gRngValue; rng2 = gRng2Value; sFinalMode = 0;
    s32 beatHitBase = CalculateMoveDamage(&beatHit);
    gBattleStruct->beatUpSlot = 0; gRngValue = rng; gRng2Value = rng2; sFinalMode = 1;
    CHECK(beatHitBase > 0 && CalculateMoveDamage(&beatHit) == beatHitBase * 2
        && !(sFinalLastFlags & RHH_FINAL_DAMAGE_AI));
    memcpy(gBattleStruct->beatUpSpecies, beatSaved, sizeof(beatSaved)); gBattleStruct->beatUpSlot = 4;
    struct Pokemon savedBeatParty[PARTY_SIZE]; memcpy(savedBeatParty, gParties[beatTrainer], sizeof(savedBeatParty));
    u8 beatControllers[2] = {gBattlerBattleController[0], gBattlerBattleController[1]};
    memset(gParties[beatTrainer], 0, sizeof(savedBeatParty));
    CreateMonWithIVs(&gParties[beatTrainer][0], SPECIES_CHARIZARD, 20, 17, OTID_STRUCT_PRESET(23), 31);
    CreateMonWithIVs(&gParties[beatTrainer][1], SPECIES_MAGIKARP, 20, 18, OTID_STRUCT_PRESET(23), 31);
    gBattlerBattleController[0] = BATTLE_CONTROLLER_PLAYER; gBattlerBattleController[1] = BATTLE_CONTROLLER_OPPONENT;
    sFinalMode = 0; rng = gRngValue; rng2 = gRng2Value;
    struct SimulatedDamage ownWeaker = AI_CalcDamage(&beatAi, 1, 0);
    CreateMonWithIVs(&gParties[beatTrainer][1], SPECIES_SLAKING, 20, 18, OTID_STRUCT_PRESET(23), 31);
    gRngValue = rng; gRng2Value = rng2;
    sCaptureBeat = TRUE; sBeatCaptureCount = sBeatStageMask = 0;
    struct SimulatedDamage ownStronger = AI_CalcDamage(&beatAi, 1, 0);
    sCaptureBeat = FALSE;
    CHECK(ownStronger.median > ownWeaker.median);
    CHECK(sBeatCaptureCount && sBeatStageMask == 63);
    for (u32 i = 0; i < sBeatCaptureCount; ++i)
        CHECK((sBeatCaptured[i].flags & (RHH_BEAT_UP_CONTRIBUTOR_AI | RHH_BEAT_UP_CONTRIBUTOR_OWN_PARTY
            | RHH_BEAT_UP_CONTRIBUTOR_SLOT_KNOWN)) == (RHH_BEAT_UP_CONTRIBUTOR_AI
            | RHH_BEAT_UP_CONTRIBUTOR_OWN_PARTY | RHH_BEAT_UP_CONTRIBUTOR_SLOT_KNOWN)
            && sBeatCaptured[i].party_slot == sBeatCaptured[i].hit_index);
    u32 beatStatus = STATUS1_BURN; SetMonData(&gParties[beatTrainer][1], MON_DATA_STATUS, &beatStatus);
    sFinalMode = 1; finalCalls = sFinalCalls; (void)AI_CalcDamage(&beatAi, 1, 0);
    CHECK(sFinalCalls == finalCalls + 4);
    memcpy(gParties[beatTrainer], savedBeatParty, sizeof(savedBeatParty));
    gBattlerBattleController[0] = beatControllers[0]; gBattlerBattleController[1] = beatControllers[1];
    parties = savedPartyKnowledge;
    struct DamageContext selfDamage = {.battlerAtk = 1, .battlerDef = 1,
        .move = MOVE_NONE, .chosenMove = MOVE_NONE, .baseMove = MOVE_NONE,
        .moveType = TYPE_MYSTERY, .fixedBasePower = 40, .isSelfInflicted = TRUE, .updateFlags = TRUE};
    sFinalMode = 0; s32 selfBase = CalculateMoveDamage(&selfDamage);
    finalCalls = sFinalCalls; sFinalMode = 1;
    CHECK(selfBase > 0 && CalculateMoveDamage(&selfDamage) == selfBase * 2
        && sFinalCalls == finalCalls + 1 && sFinalLastFlags == RHH_FINAL_DAMAGE_SELF_INFLICTED
        && sFinalLastKind == RHH_SELF_DAMAGE_FINAL);
    sFinalMode = 4; rng = gRngValue; rng2 = gRng2Value;
    CHECK(CalculateMoveDamage(&selfDamage) == selfBase
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sFinalMode = 7; CHECK(CalculateMoveDamage(&selfDamage) == selfBase);
    sFinalMode = 8; CHECK(CalculateMoveDamage(&selfDamage) == selfBase * 2);
    sFinalMode = 5; sFinalInnateCalls = 0;
    CHECK(CalculateMoveDamage(&selfDamage) == selfBase + 2 && sFinalInnateCalls == 2);
    sFinalMode = 0; *mode = 40;
    CHECK(CalculateMoveDamage(&selfDamage) == selfBase);
    *mode = 41; CHECK(CalculateMoveDamage(&selfDamage) == selfBase + 13);
    sFinalMode = 0; *mode = 40; damage.aiCalc = TRUE; damage.typeEffectivenessModifier = UQ_4_12(1);
    CHECK(ModsFinalDamage(&damage, 100) == 113);
    damage.aiCalc = FALSE; *mode = 0;
    u32 attack = 100, defense = 100;
    *mode = 30; ModsDamageStats(&damage, 50, &attack, &defense); CHECK(attack == 200 && defense == 50);
    damage.aiCalc = TRUE; attack = defense = 100;
    ModsDamageStats(&damage, 50, &attack, &defense); CHECK(attack == 200 && defense == 50);
    damage.aiCalc = FALSE;
    *mode = 31; attack = defense = 100;
    ModsDamageStats(&damage, 50, &attack, &defense); CHECK(attack == 103 && defense == 103);
    damage.abilities[1] = ABILITY_TORRENT; attack = defense = 100;
    ModsDamageStats(&damage, 50, &attack, &defense); CHECK(attack == 102 && defense == 102);
    damage.abilities[1] = ABILITY_NONE;
    gBattleMons[1].volatiles.gastroAcid = TRUE; attack = defense = 100;
    ModsDamageStats(&damage, 50, &attack, &defense); CHECK(attack == 100 && defense == 100);
    gBattleMons[1].volatiles.gastroAcid = FALSE;
    *mode = 32; attack = defense = 100;
    ModsDamageStats(&damage, 50, &attack, &defense); CHECK(attack == 100 && defense == 100);
    *mode = 33; sImpureDamageStat = TRUE; rng = gRngValue; rng2 = gRng2Value; attack = defense = 100;
    ModsDamageStats(&damage, 50, &attack, &defense); CHECK(attack == 100 && defense == 100
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sImpureDamageStat = FALSE;
    *mode = 34; attack = defense = 100;
    ModsDamageStats(&damage, 50, &attack, &defense); CHECK(attack == 102 && defense == 102);
    *mode = 35; sImpureInnateDamageStat = TRUE; rng = gRngValue; rng2 = gRng2Value; attack = defense = 100;
    ModsDamageStats(&damage, 50, &attack, &defense); CHECK(attack == 102 && defense == 102
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sImpureInnateDamageStat = FALSE;
    *mode = 36; gFieldStatuses |= STATUS_FIELD_WONDER_ROOM;
    damage.move = MOVE_FOUL_PLAY; ModsDamageStats(&damage, 50, &attack, &defense);
    damage.move = MOVE_BODY_PRESS; ModsDamageStats(&damage, 50, &attack, &defense);
    damage.move = MOVE_PSYSHOCK; ModsDamageStats(&damage, 50, &attack, &defense);
    gFieldStatuses &= ~STATUS_FIELD_WONDER_ROOM; damage.move = MOVE_TACKLE;
    *mode = 37; attack = defense = 100; ModsDamageStats(&damage, 50, &attack, &defense);
    CHECK(attack == 100 && defense == 110);
    damage.battlerAtk = 0; damage.battlerDef = 1; damage.aiCalc = TRUE;
    damage.abilities[0] = ABILITY_MOLD_BREAKER; damage.holdEffects[1] = HOLD_EFFECT_NONE;
    *mode = 38; attack = defense = 100; ModsDamageStats(&damage, 50, &attack, &defense);
    damage.holdEffects[1] = HOLD_EFFECT_ABILITY_SHIELD;
    *mode = 39; ModsDamageStats(&damage, 50, &attack, &defense);
    damage.battlerAtk = 1; damage.battlerDef = 0; damage.aiCalc = FALSE;
    damage.abilities[0] = ABILITY_NONE; damage.holdEffects[1] = HOLD_EFFECT_NONE;
    *mode = 0; s32 baseDamage = CalculateMoveDamageVars(&damage);
    *mode = 30; CHECK(CalculateMoveDamageVars(&damage) > baseDamage);
    damage.aiCalc = TRUE; *mode = 0; baseDamage = CalculateMoveDamageVars(&damage);
    *mode = 30; CHECK(CalculateMoveDamageVars(&damage) > baseDamage);
    damage.aiCalc = FALSE;
    *mode = 30; gBattleTypeFlags |= BATTLE_TYPE_LINK; attack = defense = 100;
    ModsDamageStats(&damage, 50, &attack, &defense); CHECK(attack == 100 && defense == 100);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    *mode = 1; dispatches = sBattlerPriorityDispatches;
    gBattleTypeFlags |= BATTLE_TYPE_LINK; CHECK(GetMovePriority(MOVE_WATER_GUN) == 0
        && !moves->info(RhhContentKeyOf("rhh:move/water_gun"), &info) && info.priority == 0
        && GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 0 && sBattlerPriorityDispatches == dispatches);
    gBattleTypeFlags = BATTLE_TYPE_RECORDED; CHECK(GetMovePriority(MOVE_WATER_GUN) == 0
        && GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 0 && sBattlerPriorityDispatches == dispatches);
    CHECK(moves->battler_priority_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    PrepareTurnBattle(); gFieldStatuses = 0; sFinalMode = 0; *mode = 0;
    u32 skippedStatus = STATUS1_BURN;
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][1], MON_DATA_STATUS, &skippedStatus);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][2], SPECIES_SLAKING, 20, 19, OTID_STRUCT_PRESET(23), 31);
    gCurrentMove = gChosenMove = MOVE_BEAT_UP; gBattlerAttacker = 1; gBattlerTarget = 0;
    gBattleStruct->eventState.atkCanceler = CANCELER_MULTIHIT_MOVES;
    gBattleStruct->moveResultFlags[0] = 0; gSpecialStatuses[0].storedTypeEffectiveness = UQ_4_12(1);
    (void)DoAttackCanceler();
    CHECK(gBattleStruct->beatUpContributorValid && gBattleStruct->beatUpContributorCount == 2
        && gBattleStruct->beatUpContributorBattler == 1 && gBattleStruct->beatUpPartySlots[0] == 0
        && gBattleStruct->beatUpPartySlots[1] == 2);
    struct DamageContext realBeat = {.battlerAtk = 1, .battlerDef = 0, .move = MOVE_BEAT_UP,
        .chosenMove = MOVE_BEAT_UP, .baseMove = MOVE_BEAT_UP, .moveType = TYPE_DARK,
        .typeEffectivenessModifier = UQ_4_12(1), .randomFactor = TRUE, .updateFlags = TRUE};
    gBattleStruct->beatUpSlot = 0; sCaptureBeat = TRUE; sBeatCaptureCount = sBeatStageMask = 0;
    CHECK(CalculateMoveDamageVars(&realBeat) > 0 && CalculateMoveDamageVars(&realBeat) > 0);
    sCaptureBeat = FALSE;
    CHECK(sBeatCaptureCount && sBeatStageMask == 63);
    for (u32 i = 0; i < sBeatCaptureCount; ++i)
    {
        const struct RhhBeatUpContributor *c = &sBeatCaptured[i];
        CHECK(!(c->flags & RHH_BEAT_UP_CONTRIBUTOR_AI) && (c->flags & RHH_BEAT_UP_CONTRIBUTOR_SLOT_KNOWN));
        if (c->hit_index == 0)
            CHECK(c->party_slot == 0 && c->species == RhhContentKeyForId(RHH_SPECIES, SPECIES_CHARIZARD));
        else
            CHECK(c->hit_index == 1 && c->party_slot == 2
                && c->species == RhhContentKeyForId(RHH_SPECIES, SPECIES_SLAKING));
    }
    gBattleScripting.moveendState = MOVEEND_CLEAR_BITS;
    (void)DoMoveEnd(0, MOVEEND_CLEAR_BITS);
    CHECK(!gBattleStruct->beatUpContributorValid);
    gBattleStruct->beatUpSlot = 0; sCaptureBeat = 2; sBeatCaptureCount = 0;
    CHECK(CalculateMoveDamageVars(&realBeat) > 0 && sBeatCaptureCount);
    realBeat.move = realBeat.chosenMove = realBeat.baseMove = MOVE_TACKLE;
    realBeat.moveType = TYPE_NORMAL; sCaptureBeat = 2; sBeatCaptureCount = 0;
    CHECK(CalculateMoveDamageVars(&realBeat) > 0 && sBeatCaptureCount);
    sCaptureBeat = FALSE;
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gAiLogicData = NULL;
    gAiPartyData = NULL; gBattleHistory = NULL;
    RhhModsUnloadLibraries();
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sOnlyPriorityMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sOnlyPriorityAbilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 34));
    sBattleEnvironmentService = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 35));
    sOnlyFieldAbility = RhhContentKeyOf("test:ability/field_probe"); sEnvironmentCommits = 0;
    CHECK(sOnlyPriorityAbilities && sBattleEnvironmentService && sOnlyFieldAbility && !RhhModsHasHook(RHH_MOVE_BATTLER_PRIORITY) && RhhModsHasHook(RHH_INNATE_PRIORITY));
    gBattleResources = &resources; gAiLogicData = &ai; gAiPartyData = &parties; gBattleHistory = &history;
    PrepareTurnBattle(); ModsBattleBegin(); gBattleMons[1].ability = ABILITY_NONE; ai.abilities[1] = ABILITY_NONE;
    CHECK(GetBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 1
        && GetAiBattleMovePriority(1, ABILITY_NONE, MOVE_WATER_GUN) == 1);
    gBattleMons[1].speed = 10;
    CHECK(!RhhModsHasHook(RHH_BATTLER_SPEED) && RhhModsHasHook(RHH_INNATE_SPEED)
        && GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 11
        && GetAiBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 11);
    damage = (struct DamageContext){.battlerAtk = 1, .battlerDef = 0, .move = MOVE_TACKLE,
        .chosenMove = MOVE_TACKLE, .baseMove = MOVE_TACKLE, .moveType = TYPE_NORMAL};
    attack = defense = 100;
    CHECK(!RhhModsHasHook(RHH_DAMAGE_STAT) && RhhModsHasHook(RHH_INNATE_DAMAGE_STAT));
    ModsDamageStats(&damage, 50, &attack, &defense); CHECK(attack == 101 && defense == 101);
    gBattleWeather = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers)); environmentScripts.size = 0;
    CHECK(ModsInnateSwitchIn(0) && ModsInnateNextEnvironment() == MOD_INNATE_ENV_WEATHER
        && sEnvironmentCommits == 1 && sLastEnvironmentCommit.ability == sOnlyFieldAbility
        && sLastEnvironmentCommit.source_battler == 0 && sLastEnvironmentCommit.cause == RHH_BATTLE_ENV_CAUSE_INNATE);
    CHECK(ModsInnateNextEnvironment() == MOD_INNATE_ENV_TERRAIN && sEnvironmentCommits == 2
        && sLastEnvironmentCommit.ability == sOnlyFieldAbility && gFieldTimers.terrain == B_TERRAIN_ELECTRIC);
    CHECK(ModsInnateNextEnvironment() == MOD_INNATE_ENV_DONE);
    gBattleTypeFlags = BATTLE_TYPE_LINK;
    CHECK(GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE) == 10);
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gAiLogicData = NULL;
    gAiPartyData = NULL; gAiThinkingStruct = NULL; gBattleHistory = NULL; RhhModsShutdown();
    puts("move priority: composition, metadata, battle, AI and purity passed");
}
