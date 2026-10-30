#include "mod_api.h"
#include <stdio.h>

static const struct RhhBattleFieldRulesService *field;
static const struct RhhMoveService *moves;
static RhhContentKey effectKey;
static struct RhhModMoveEffectBinding bindings[2];
static void ObserveSports(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleSportsObservation observation = {.struct_size = sizeof(observation)};
    if (field->sports_observation(&observation) || event->subject != observation.after.battle_id
        || event->ordinal != observation.sequence || event->args[0] != observation.changed
        || event->args[1] != observation.after.mud_battlers || event->args[2] != observation.after.water_battlers
        || event->args[3] != observation.after.effective || !observation.changed
        || (observation.sequence == 0 && (observation.changed != (RHH_BATTLE_SPORTS_CHANGED_MUD_BATTLERS
            | RHH_BATTLE_SPORTS_CHANGED_WATER_BATTLERS | RHH_BATTLE_SPORTS_CHANGED_EFFECTIVE)
            || observation.before.mud_battlers || observation.before.water_battlers
            || observation.after.mud_battlers != 1 || observation.after.water_battlers != 1))
        || (observation.sequence == 1 && (observation.changed != RHH_BATTLE_SPORTS_CHANGED_MUD_BATTLERS
            || observation.before.mud_battlers != 1 || observation.after.mud_battlers != 2))
        || observation.sequence > 1)
        puts("SPORTS_EXTERNAL_BAD");
    else puts("SPORTS_EXTERNAL_OK");
}
static void Apply(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    if (event->event_key != effectKey) return;
    if (!field->queue_move_effect(0) || !field->queue_move_effect(RHH_BATTLE_FIELD_MAGIC_ROOM)
        || !field->queue_move_effect(RHH_BATTLE_FIELD_GRAVITY | RHH_BATTLE_FIELD_ION_DELUGE))
        puts("FIELD_EFFECT_BAD_INVALID");
    if (!field->queue_room_toggle(0) || !field->queue_room_toggle(RHH_BATTLE_FIELD_GRAVITY)
        || !field->queue_room_toggle(RHH_BATTLE_FIELD_TRICK_ROOM | RHH_BATTLE_FIELD_MAGIC_ROOM))
        puts("FIELD_EFFECT_BAD_INVALID");
    if (!field->queue_field_move(0) || !field->queue_field_move(RHH_BATTLE_FIELD_GRAVITY)
        || !field->queue_field_move(RHH_BATTLE_FIELD_MUD_SPORT | RHH_BATTLE_FIELD_FAIRY_LOCK))
        puts("FIELD_EFFECT_BAD_INVALID");
    unsigned mode = *(unsigned char *)state;
    if (mode >= 7)
    {
        const uint32_t rules[] = {RHH_BATTLE_FIELD_MUD_SPORT, RHH_BATTLE_FIELD_WATER_SPORT, RHH_BATTLE_FIELD_FAIRY_LOCK};
        unsigned count = mode == 8 ? 8 : 3;
        for (unsigned i = 0; i < count; ++i)
            if (field->queue_field_move(rules[mode == 8 ? 2 : i])) puts("FIELD_EFFECT_BAD_FIELD_MOVE");
        if (mode == 8 && !field->queue_field_move(rules[0])) puts("FIELD_EFFECT_BAD_CAPACITY");
        puts("FIELD_EFFECT_OFFERED"); return;
    }
    if (mode >= 4)
    {
        const uint32_t rooms[] = {RHH_BATTLE_FIELD_TRICK_ROOM, RHH_BATTLE_FIELD_WONDER_ROOM, RHH_BATTLE_FIELD_MAGIC_ROOM};
        unsigned count = mode == 6 ? 8 : mode == 5 ? 2 : 3;
        for (unsigned i = 0; i < count; ++i)
            if (field->queue_room_toggle(rooms[mode == 5 || mode == 6 ? 0 : i])) puts("FIELD_EFFECT_BAD_ROOM");
        if (mode == 6 && !field->queue_room_toggle(RHH_BATTLE_FIELD_MAGIC_ROOM)) puts("FIELD_EFFECT_BAD_CAPACITY");
        puts("FIELD_EFFECT_OFFERED"); return;
    }
    unsigned count = mode == 1 ? 8 : 2;
    for (unsigned i = 0; i < count; ++i)
    {
        uint32_t rule = count == 8 || !i ? RHH_BATTLE_FIELD_GRAVITY : RHH_BATTLE_FIELD_ION_DELUGE;
        if (field->queue_move_effect(rule)) puts("FIELD_EFFECT_BAD_QUEUE");
    }
    if (count == 8 && !field->queue_move_effect(RHH_BATTLE_FIELD_ION_DELUGE)) puts("FIELD_EFFECT_BAD_CAPACITY");
    puts("FIELD_EFFECT_OFFERED");
}
static void Preview(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    if (event->subject != effectKey) return;
    unsigned mode = *(const unsigned char *)state;
    if (field->offer_ai_move_effect(0, 100) != RHH_AI_EFFECT_INVALID
        || field->offer_ai_move_effect(RHH_BATTLE_FIELD_MAGIC_ROOM, 100) != RHH_AI_EFFECT_INVALID
        || field->offer_ai_move_effect(RHH_BATTLE_FIELD_GRAVITY, 0) != RHH_AI_EFFECT_INVALID
        || field->offer_ai_move_effect(RHH_BATTLE_FIELD_GRAVITY, 101) != RHH_AI_EFFECT_INVALID)
        puts("FIELD_AI_BAD_INVALID");
    if (field->offer_ai_room_toggle(0, 100) != RHH_AI_EFFECT_INVALID
        || field->offer_ai_room_toggle(RHH_BATTLE_FIELD_GRAVITY, 100) != RHH_AI_EFFECT_INVALID
        || field->offer_ai_room_toggle(RHH_BATTLE_FIELD_TRICK_ROOM | RHH_BATTLE_FIELD_MAGIC_ROOM, 100) != RHH_AI_EFFECT_INVALID
        || field->offer_ai_room_toggle(RHH_BATTLE_FIELD_TRICK_ROOM, 0) != RHH_AI_EFFECT_INVALID
        || field->offer_ai_room_toggle(RHH_BATTLE_FIELD_TRICK_ROOM, 101) != RHH_AI_EFFECT_INVALID)
        puts("FIELD_AI_BAD_INVALID");
    if (field->offer_ai_field_move(0, 100) != RHH_AI_EFFECT_INVALID
        || field->offer_ai_field_move(RHH_BATTLE_FIELD_TRICK_ROOM, 100) != RHH_AI_EFFECT_INVALID
        || field->offer_ai_field_move(RHH_BATTLE_FIELD_MUD_SPORT | RHH_BATTLE_FIELD_FAIRY_LOCK, 100) != RHH_AI_EFFECT_INVALID
        || field->offer_ai_field_move(RHH_BATTLE_FIELD_MUD_SPORT, 0) != RHH_AI_EFFECT_INVALID
        || field->offer_ai_field_move(RHH_BATTLE_FIELD_MUD_SPORT, 101) != RHH_AI_EFFECT_INVALID)
        puts("FIELD_AI_BAD_INVALID");
    if (mode >= 7)
    {
        const uint32_t rules[] = {RHH_BATTLE_FIELD_MUD_SPORT, RHH_BATTLE_FIELD_WATER_SPORT, RHH_BATTLE_FIELD_FAIRY_LOCK};
        unsigned count = mode == 8 ? RHH_AI_EFFECT_PREVIEW_LIMIT : 3;
        for (unsigned i = 0; i < count; ++i)
            if (field->offer_ai_field_move(rules[mode == 8 ? 2 : i], 60) != RHH_AI_EFFECT_OK) puts("FIELD_AI_BAD_FIELD_MOVE");
        if (mode == 8 && field->offer_ai_field_move(rules[0], 100) != RHH_AI_EFFECT_FULL) puts("FIELD_AI_BAD_CAPACITY");
        puts("FIELD_AI_PREVIEW"); return;
    }
    if (mode >= 4)
    {
        const uint32_t rooms[] = {RHH_BATTLE_FIELD_TRICK_ROOM, RHH_BATTLE_FIELD_WONDER_ROOM, RHH_BATTLE_FIELD_MAGIC_ROOM};
        unsigned count = mode == 6 ? RHH_AI_EFFECT_PREVIEW_LIMIT : 3;
        for (unsigned i = 0; i < count; ++i)
            if (field->offer_ai_room_toggle(rooms[mode == 6 ? 0 : i], 75) != RHH_AI_EFFECT_OK) puts("FIELD_AI_BAD_ROOM");
        if (mode == 6 && field->offer_ai_room_toggle(rooms[2], 100) != RHH_AI_EFFECT_FULL) puts("FIELD_AI_BAD_CAPACITY");
        puts("FIELD_AI_PREVIEW"); return;
    }
    if (mode == 3)
    {
        for (unsigned i = 0; i < RHH_AI_EFFECT_PREVIEW_LIMIT; ++i)
            if (field->offer_ai_move_effect(RHH_BATTLE_FIELD_GRAVITY, 100) != RHH_AI_EFFECT_OK) puts("FIELD_AI_BAD_CAPACITY");
        if (field->offer_ai_move_effect(RHH_BATTLE_FIELD_ION_DELUGE, 100) != RHH_AI_EFFECT_FULL) puts("FIELD_AI_BAD_CAPACITY");
    }
    else if (field->offer_ai_move_effect(RHH_BATTLE_FIELD_GRAVITY, 75) != RHH_AI_EFFECT_OK
             || field->offer_ai_move_effect(RHH_BATTLE_FIELD_ION_DELUGE, 40) != RHH_AI_EFFECT_OK)
        puts("FIELD_AI_BAD_OFFER");
    if (mode == 2) decision->allowed = 0;
    puts("FIELD_AI_PREVIEW");
}
static void Score(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    unsigned mode = *(const unsigned char *)state;
    uint32_t count = 99;
    if (moves->ai_effect_count(&count)) { puts("FIELD_AI_BAD_COUNT"); return; }
    if (mode == 2 || mode == 3 || mode == 6 || mode == 8)
    {
        if (count != (mode == 2 ? 0u : RHH_AI_EFFECT_PREVIEW_LIMIT)) puts("FIELD_AI_BAD_COUNT");
        puts("FIELD_AI_SCORE"); return;
    }
    if (mode == 7 || mode == 9 || mode == 10)
    {
        const uint32_t rules[] = {RHH_BATTLE_FIELD_MUD_SPORT, RHH_BATTLE_FIELD_WATER_SPORT, RHH_BATTLE_FIELD_FAIRY_LOCK};
        struct RhhBattleFieldRules current = {.struct_size = sizeof(current)};
        struct RhhBattleSports sports = {.struct_size = sizeof(sports)};
        if (count != 3 || field->current(&current) || field->sports(&sports)) puts("FIELD_AI_BAD_FIELD_COUNT");
        for (unsigned i = 0; i < 3; ++i)
        {
            struct RhhAiEffectAction action = {.struct_size = sizeof(action)};
            struct RhhAiEffectEstimate estimate = {.struct_size = sizeof(estimate)};
            unsigned blocked = !!(current.active & rules[i]) || (mode == 9 && i < 2)
                || (mode == 10 && i < 2 && !!((i ? sports.water_battlers : sports.mud_battlers) & 1));
            if (moves->ai_effect_at(i, &action) || action.kind != RHH_AI_EFFECT_FIELD_RULE
                || action.first != rules[i] || action.chance != 60 || action.flags
                || moves->ai_effect_estimate(i, RHH_AI_EFFECT_NO_BATTLER, &estimate)
                || estimate.minimum != (blocked ? 0 : 1) || estimate.median != estimate.minimum
                || estimate.maximum != estimate.minimum || !!(estimate.flags & RHH_AI_ESTIMATE_BLOCKED) != blocked)
                puts("FIELD_AI_BAD_FIELD_ESTIMATE");
        }
        puts("FIELD_AI_SCORE"); return;
    }
    if (mode == 4 || mode == 5)
    {
        const uint32_t rooms[] = {RHH_BATTLE_FIELD_TRICK_ROOM, RHH_BATTLE_FIELD_WONDER_ROOM, RHH_BATTLE_FIELD_MAGIC_ROOM};
        struct RhhBattleFieldRules current = {.struct_size = sizeof(current)};
        if (count != 3 || field->current(&current)) puts("FIELD_AI_BAD_ROOM_COUNT");
        for (unsigned i = 0; i < 3; ++i)
        {
            struct RhhAiEffectAction action = {.struct_size = sizeof(action)};
            struct RhhAiEffectEstimate estimate = {.struct_size = sizeof(estimate)};
            if (moves->ai_effect_at(i, &action) || action.kind != RHH_AI_EFFECT_ROOM_TOGGLE
                || action.first != rooms[i] || action.chance != 75 || action.flags
                || moves->ai_effect_estimate(i, RHH_AI_EFFECT_NO_BATTLER, &estimate)
                || estimate.minimum != ((current.active & rooms[i]) ? -1 : 1)
                || estimate.median != estimate.minimum || estimate.maximum != estimate.minimum || estimate.flags)
                puts("FIELD_AI_BAD_ROOM_ESTIMATE");
        }
        puts("FIELD_AI_SCORE"); return;
    }
    struct RhhAiEffectAction action = {.struct_size = sizeof(action)};
    struct RhhAiEffectEstimate estimate = {.struct_size = sizeof(estimate)}, wrong = {.struct_size = sizeof(wrong), .median = 99};
    struct RhhBattleFieldRules current = {.struct_size = sizeof(current)};
    if (count != 2 || moves->ai_effect_at(0, &action) || action.kind != RHH_AI_EFFECT_FIELD_RULE
        || action.first != RHH_BATTLE_FIELD_GRAVITY || action.chance != 75 || action.flags
        || field->current(&current) || moves->ai_effect_estimate(0, RHH_AI_EFFECT_NO_BATTLER, &estimate)
        || !moves->ai_effect_estimate(0, 0, &wrong) || wrong.median != 99
        || estimate.kind != RHH_AI_EFFECT_FIELD_RULE || estimate.chance != 75
        || estimate.median != ((current.active & RHH_BATTLE_FIELD_GRAVITY) ? 0 : 1)
        || !!(estimate.flags & RHH_AI_ESTIMATE_BLOCKED) != !!(current.active & RHH_BATTLE_FIELD_GRAVITY))
        puts("FIELD_AI_BAD_ESTIMATE");
    if (moves->ai_effect_at(1, &action) || action.kind != RHH_AI_EFFECT_FIELD_RULE
        || action.first != RHH_BATTLE_FIELD_ION_DELUGE || action.chance != 40)
        puts("FIELD_AI_BAD_ORDER");
    decision->value = (current.active & RHH_BATTLE_FIELD_GRAVITY) ? decision->value - 5 : decision->value + 5;
    puts("FIELD_AI_SCORE");
}
static const struct RhhModMoveEffect effects[] = {
    {sizeof(struct RhhModMoveEffect), RHH_MOVE_EFFECT_CUSTOM_STATUS, {0, 0}, "test:effect/field_rule", "Field Rule"}
};
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_REGISTERED_MOVE_EFFECT, RHH_OBSERVE, 0, NULL, Apply},
    {sizeof(struct RhhModHook), RHH_AI_EFFECT_PREVIEW, RHH_CONSTRAINT, 0, Preview, NULL},
    {sizeof(struct RhhModHook), RHH_AI_MOVE_SCORE, RHH_TRANSFORM, 0, Score, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_SPORTS_OBSERVED, RHH_OBSERVE, 0, NULL, ObserveSports}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 7), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle_field_rules"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 21), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_MOVE_EFFECTS,
    .id = "test:field_rule_effect", .state_size = 1, .hook_count = 4, .hooks = hooks,
    .service_requirement_count = 2, .service_requirements = requirements,
    .move_effect_count = 1, .move_effects = effects, .move_effect_binding_count = 2, .move_effect_bindings = bindings};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    field = (const void *)host->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 7));
    moves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 21));
    effectKey = host->content_key("test:effect/field_rule");
    bindings[0] = (struct RhhModMoveEffectBinding){sizeof(bindings[0]), 0, host->content_key("rhh:move/rain_dance"), effectKey};
    bindings[1] = (struct RhhModMoveEffectBinding){sizeof(bindings[1]), 0, host->content_key("rhh:move/sing"), effectKey};
    return field && field->header.struct_size >= RHH_BATTLE_FIELD_RULES_SERVICE_V1_7_SIZE && moves
        && bindings[0].move && bindings[1].move && effectKey ? &definition : NULL;
}
