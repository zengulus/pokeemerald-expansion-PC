#include "mod_api.h"
#include <stdio.h>
struct ProbeState { uint32_t enabled, attempts, status, previous, remaining, changed, actions, nested, cured, scriptOrder, partyCured; };
static const struct RhhBattleService *battle;
static RhhContentKey condition;
static void React(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct ProbeState *probe = state;
    if (!probe->enabled) return;
    if (probe->actions == 27 && event->kind == RHH_BATTLER_STATUS_APPLIED && event->args[0] == 1) {
        ++probe->nested; return;
    }
    if (probe->actions == 2 && event->kind == RHH_BATTLER_STATUS_APPLIED && event->args[0] == 2) {
        struct RhhBattleStatusApplication status = {.struct_size = sizeof(status)};
        struct RhhBattleStatusAttribution attribution = {.struct_size = sizeof(attribution)};
        if (battle->status_application_context(&status) || battle->status_attribution(&attribution)
            || status.source_battler != 0 || status.target_battler != 2 || status.status != RHH_BATTLE_STATUS_POISON
            || status.trigger != RHH_BATTLE_STATUS_FROM_ABILITY || attribution.kind != RHH_STATUS_ATTRIBUTION_UNKNOWN
            || battle->status_reaction_animation(2, RHH_EFFECT_ANIM_PROTECT))
            puts("STATUS_CONDITION_PROBE_BAD");
        else ++probe->nested;
        return;
    }
    if (event->kind == RHH_BATTLER_STATUS_APPLIED) {
        struct RhhBattleStatusApplication context = {.struct_size = sizeof(context)};
        if (battle->status_application_context(&context) || context.target_battler != event->args[0])
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    } else {
        struct RhhBattleItemStatusChange context = {.struct_size = sizeof(context)};
        if (battle->item_status_context(&context) || context.battler != event->args[0])
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    }
    struct RhhBattleConditionSetResult result = {.struct_size = sizeof(result)};
    int status = battle->condition_set_result(condition, RHH_CONDITION_PLAYER_SIDE, 3, &result);
    if (status != (int)result.status || result.key != condition || result.target != RHH_CONDITION_PLAYER_SIDE)
    { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    if (probe->actions == 1) {
        if (!battle->status_reaction_heal(99, 1, 4) || !battle->status_reaction_heal(1, 0, 4)
            || !battle->status_reaction_stat(1, 0, 1) || !battle->status_reaction_stat(1, RHH_STAT_DEFENSE, 0)
            || !battle->status_reaction_animation(1, 0)
            || battle->status_reaction_animation(1, RHH_EFFECT_ANIM_PROTECT)
            || battle->status_reaction_heal(1, 1, 4)
            || battle->status_reaction_stat(1, RHH_STAT_DEFENSE, 1))
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    }
    if (probe->actions == 2) {
        if (!battle->status_reaction_stat_from(99, 1, RHH_STAT_ATTACK, -1)
            || !battle->status_reaction_stat_from(0, 1, RHH_STAT_ATTACK, 0)
            || !battle->status_reaction_status_from(0, 99, RHH_EFFECT_POISON)
            || !battle->status_reaction_status_from(0, 2, RHH_EFFECT_CONFUSION)
            || battle->status_reaction_stat_from(0, 1, RHH_STAT_ATTACK, -1)
            || battle->status_reaction_status_from(0, 2, RHH_EFFECT_POISON))
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    }
    if (probe->actions == 3 && battle->status_reaction_status_from(0, 2, RHH_EFFECT_POISON))
    { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    if (probe->actions == 4 || probe->actions == 5) {
        uint32_t flags = probe->actions == 5 ? RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD : 0;
        if (!battle->status_reaction_damage(99, 1, 4, 0) || !battle->status_reaction_damage(2, 0, 4, 0)
            || !battle->status_reaction_damage(2, 1, 17, 0) || !battle->status_reaction_damage(2, 1, 4, 2)
            || battle->status_reaction_damage(2, 1, 4, flags))
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    }
    if (probe->actions >= 6 && probe->actions <= 9) {
        uint32_t battler = probe->actions >= 8 ? 2 : 1;
        uint32_t mask = probe->actions == 6 ? RHH_BATTLE_CONDITION_BURN | RHH_BATTLE_CONDITION_POISON
            : probe->actions == 7 || probe->actions == 9 ? RHH_BATTLE_CONDITION_POISON
            : RHH_BATTLE_CONDITION_SLEEP | RHH_BATTLE_CONDITION_TOXIC | RHH_BATTLE_CONDITION_FREEZE;
        if (!battle->status_reaction_cure(99, mask) || !battle->status_reaction_cure(battler, 0)
            || !battle->status_reaction_cure(battler, RHH_BATTLE_CONDITIONS_ALL + 1)
            || battle->status_reaction_cure(battler, mask))
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    }
    ++probe->attempts; probe->status = result.status; probe->previous = result.previous_turns;
    probe->remaining = result.remaining_turns; probe->changed = result.changed;
    puts("STATUS_CONDITION_PROBE_OK");
}
static void Cured(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct ProbeState *probe = state;
    struct RhhBattleStatusCure cure = {.struct_size = sizeof(cure)}, small = {.struct_size = 1, .sequence = 77}, saved = small;
    struct RhhBattleStatusCureAttribution attribution = {.struct_size = sizeof(attribution)};
    int abilityCure = probe->actions >= 10 && probe->actions <= 12;
    int scriptAbility = probe->actions == 24 || probe->actions == 25;
    int switchCure = probe->actions == 33, uproar = probe->actions == 34;
    int scriptMove = (probe->actions >= 19 && probe->actions <= 23) || probe->actions == 26 || probe->actions == 27;
    int naturalCure = probe->actions == 13 || probe->actions == 15 || probe->actions == 16 || probe->actions == 18 || uproar;
    int moveCure = probe->actions == 14 || probe->actions == 17 || probe->actions == 35 || scriptMove;
    RhhContentKey move = 0;
    if (battle->status_cure_context(&cure) || !battle->status_cure_context(&small)
        || battle->status_cure_attribution(&attribution) || battle->status_cure_move(&move)
        || small.sequence != saved.sequence || cure.sequence != event->ordinal || cure.battler != event->args[0]
        || cure.removed != event->args[1] || cure.requested != event->args[2] || cure.origin_event != event->args[3]
        || cure.origin_event != (abilityCure || scriptAbility || switchCure || naturalCure || moveCure ? 0 : probe->actions == 9 ? RHH_BATTLER_ITEM_STATUS_CHANGED : RHH_BATTLER_STATUS_APPLIED)
        || cure.origin_sequence != 0 || !cure.participant_available
        || cure.participant.identity != event->subject || cure.before_raw == cure.after_raw
        || attribution.cause != (switchCure ? RHH_BATTLE_CURE_SWITCH_ABILITY : uproar ? RHH_BATTLE_CURE_UPROAR : scriptAbility ? RHH_BATTLE_CURE_SCRIPT_ABILITY : abilityCure ? (probe->actions == 10 ? RHH_BATTLE_CURE_IMMUNITY_ABILITY : RHH_BATTLE_CURE_END_TURN_ABILITY)
            : naturalCure ? RHH_BATTLE_CURE_NATURAL : moveCure ? RHH_BATTLE_CURE_MOVE : RHH_BATTLE_CURE_REACTION)
        || attribution.source_battler != (abilityCure || switchCure || probe->actions == 14 || probe->actions == 35 ? 1 : probe->actions == 26 ? RHH_BATTLE_NO_BATTLER
            : probe->actions == 17 || scriptMove || scriptAbility ? 0 : RHH_BATTLE_NO_BATTLER)
        || event->event_key != (abilityCure || scriptAbility || switchCure ? attribution.ability : move) || ((abilityCure || scriptAbility || switchCure) && !attribution.ability)
        || (moveCure && (!move || attribution.ability)) || (!moveCure && move))
    { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    if (probe->actions == 6) {
        if (cure.battler != 1 || cure.removed != RHH_BATTLE_CONDITION_BURN
            || battle->condition_set(condition, RHH_CONDITION_PLAYER_SIDE, 3)
            || battle->status_reaction_animation(1, RHH_EFFECT_ANIM_PROTECT))
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    } else if (probe->actions == 8) {
        if (cure.battler != 2 || cure.removed != (RHH_BATTLE_CONDITION_SLEEP | RHH_BATTLE_CONDITION_TOXIC | RHH_BATTLE_CONDITION_FREEZE))
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    } else if (probe->actions == 9) {
        if (cure.battler != 2 || cure.removed != RHH_BATTLE_CONDITION_POISON)
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    } else if (switchCure) {
        if (cure.battler != 1 || cure.requested != cure.removed
            || !battle->status_reaction_animation(1, RHH_EFFECT_ANIM_PROTECT))
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    } else if (scriptMove || scriptAbility) {
        if (cure.requested != cure.removed || (probe->actions == 21 && cure.battler != (probe->scriptOrder++ ? 2u : 0u))
            || (probe->actions != 21 && cure.battler != (probe->actions == 22 || probe->actions == 23 ? 0u
                : probe->actions == 20 || probe->actions == 24 || probe->actions == 25 || probe->actions == 26 || probe->actions == 27 ? 2u : 1u))
            || (probe->actions == 19 && battle->condition_set(condition, RHH_CONDITION_PLAYER_SIDE, 3))
            || (probe->actions == 27 ? battle->status_reaction_status_from(0, 1, RHH_EFFECT_PARALYSIS)
                                     : battle->status_reaction_animation(cure.battler, RHH_EFFECT_ANIM_PROTECT)))
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    } else if (abilityCure || naturalCure || moveCure) {
        if (cure.battler != 1 || cure.requested != cure.removed
            || battle->condition_set(condition, RHH_CONDITION_PLAYER_SIDE, 3)
            || battle->status_reaction_animation(1, RHH_EFFECT_ANIM_PROTECT))
        { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    } else { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    ++probe->cured;
}
static void PartyCured(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct ProbeState *probe = state;
    struct RhhBattlePartyStatusCure cure = {.struct_size = sizeof(cure)}, small = {.struct_size = 1, .sequence = 77};
    if ((probe->actions != 28 && probe->actions != 32) || battle->party_status_cure_context(&cure)
        || !battle->party_status_cure_context(&small) || small.sequence != 77
        || cure.owner != RHH_BATTLE_OWNER_PLAYER || cure.party_slot != (probe->actions == 28 ? 3 + probe->partyCured : 3)
        || cure.source_battler != (probe->actions == 32 ? RHH_BATTLE_NO_BATTLER : 0)
        || cure.before_raw == 0 || cure.after_raw != 0 || cure.removed != RHH_BATTLE_CONDITION_POISON
        || !cure.species || !cure.move || cure.move != event->event_key || cure.identity != event->subject
        || cure.sequence != event->ordinal || event->args[0] != cure.party_slot || event->args[1] != cure.removed
        || event->args[2] != cure.owner || event->args[3] != cure.source_battler
        || battle->condition_set(condition, RHH_CONDITION_PLAYER_SIDE, 3)
        || battle->status_reaction_animation(1, RHH_EFFECT_ANIM_PROTECT))
    { puts("STATUS_CONDITION_PROBE_BAD"); return; }
    ++probe->partyCured;
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLER_STATUS_APPLIED, RHH_OBSERVE, 0, NULL, React},
    {sizeof(struct RhhModHook), RHH_BATTLER_ITEM_STATUS_CHANGED, RHH_OBSERVE, 0, NULL, React},
    {sizeof(struct RhhModHook), RHH_BATTLER_STATUS_CURED, RHH_OBSERVE, 0, NULL, Cured},
    {sizeof(struct RhhModHook), RHH_BATTLE_PARTY_STATUS_CURED, RHH_OBSERVE, 0, NULL, PartyCured}
};
static const struct RhhModBattleCondition definitions[] = {
    {sizeof(struct RhhModBattleCondition), RHH_CONDITION_SIDE, 5, 0, "test:condition/status_ward", "Status Ward"}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 30), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_CONDITIONS,
    .id = "test:status_condition_probe", .state_size = sizeof(struct ProbeState), .hooks = hooks, .hook_count = 4,
    .battle_condition_count = 1, .battle_conditions = definitions,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    battle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 30));
    condition = host->content_key("test:condition/status_ward"); return battle && condition ? &definition : NULL;
}
