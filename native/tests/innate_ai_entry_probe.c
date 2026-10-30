#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhAbilityService *sAbilities;
static const struct RhhBattleGroundingService *sGrounding;
static RhhContentKey sSpecies, sRainDishSpecies, sInnate, sSecondInnate;
static unsigned sFieldStepCount;

static void CheckEntryGrounding(uint32_t battler, uint32_t terrain, int grounded, int semi)
{
    struct RhhBattleGrounding view = {.struct_size = sizeof(view)}, preview = {.struct_size = sizeof(preview)};
    struct RhhBattleGrounding wrong = {.struct_size = sizeof(wrong), .flags = 999}, old = wrong;
    const uint32_t expected = RHH_BATTLE_GROUNDING_AI | RHH_BATTLE_GROUNDING_AI_ENTRY | RHH_BATTLE_GROUNDING_SCENARIO;
    if (sGrounding->entry_ai(&view) || view.battler != battler || view.terrain != terrain
        || (view.flags & expected) != expected
        || !!(view.flags & RHH_BATTLE_GROUNDING_GROUNDED) != grounded
        || !!(view.flags & RHH_BATTLE_GROUNDING_SEMI_INVULNERABLE) != semi
        || !!(view.flags & RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED) != (grounded && !semi && terrain != RHH_TERRAIN_NONE)
        || sGrounding->preview_entry_ai(RHH_TERRAIN_PSYCHIC, &preview)
        || preview.terrain != RHH_TERRAIN_PSYCHIC
        || !!(preview.flags & RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED) != (grounded && !semi)
        || !sGrounding->preview_entry_ai(999, &wrong) || memcmp(&wrong, &old, sizeof(wrong))
        || !sGrounding->live(battler, &wrong) || memcmp(&wrong, &old, sizeof(wrong))
        || !sGrounding->ai(battler, &wrong) || memcmp(&wrong, &old, sizeof(wrong))) puts("AI_ENTRY_BAD_GROUNDING");
    else puts("AI_ENTRY_GROUNDING_OK");
}

static void Offer(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)decision;
    if (event->event_key != sSpecies && event->event_key != sRainDishSpecies) return;
    struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .unlock_level = 10, .ability = sInnate};
    if (sAbilities->offer_innate(&offer)) puts("AI_ENTRY_BAD");
    if (*(const unsigned char *)state == 36 || *(const unsigned char *)state == 37)
    {
        offer.ability = sSecondInnate;
        if (sAbilities->offer_innate(&offer)) puts("AI_ENTRY_BAD");
    }
}
static void Preview(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhAiInnateEntryContext c = {.struct_size = sizeof(c)}, tiny = {.struct_size = 1, .battler = 999}, before = tiny;
    struct RhhBattlerAbility actual = {.struct_size = sizeof(actual)};
    if (sAbilities->ai_innate_entry_context(&c) || !sAbilities->ai_innate_entry_context(&tiny)
        || memcmp(&tiny, &before, sizeof(tiny)) || c.ability != sInnate || c.ability != event->event_key
        || c.battler != event->args[0] || c.party_slot != event->args[1] || c.hp != event->args[2]
        || c.max_hp != event->args[3] || c.index != event->ordinal || c.offered || c.capacity != RHH_AI_INNATE_ENTRY_HEAL_LIMIT
        || c.index >= c.resolution.count || c.resolution.entries[c.index].status
        || !(c.resolution.flags & RHH_INNATE_AI_KNOWLEDGE) || !sAbilities->battler(c.battler, &actual)
        || !sAbilities->offer_ai_innate_entry_heal(0, 1) || !sAbilities->offer_ai_innate_entry_heal(2, 1))
    { puts("AI_ENTRY_BAD"); return; }
    uint32_t enabled = *(const unsigned char *)state;
    if (enabled == 3)
    {
        for (unsigned i = 0; i < RHH_AI_INNATE_ENTRY_HEAL_LIMIT; ++i)
            if (sAbilities->offer_ai_innate_entry_heal(1, 2)) puts("AI_ENTRY_BAD");
        if (!sAbilities->offer_ai_innate_entry_heal(1, 2)) puts("AI_ENTRY_BAD");
        struct RhhAiInnateEntryContext full = {.struct_size = sizeof(full)};
        if (sAbilities->ai_innate_entry_context(&full) || full.offered != RHH_AI_INNATE_ENTRY_HEAL_LIMIT || full.capacity)
            puts("AI_ENTRY_BAD");
    }
    else if (enabled == 4)
    {
        uint32_t hazard = c.max_hp / 8;
        if (!hazard) hazard = 1;
        if (c.hp != 20 - hazard) puts("AI_ENTRY_BAD");
        else puts("AI_ENTRY_HAZARD_ONCE");
    }
    else if (enabled == 5)
    {
        uint32_t hazard = c.max_hp / 8;
        if (!hazard) hazard = 1;
        if (c.hp != c.max_hp - hazard) puts("AI_ENTRY_BAD");
        else puts("AI_ENTRY_HEALING_WISH");
    }
    else if (enabled && enabled < 6 && sAbilities->offer_ai_innate_entry_heal(1, 2)) puts("AI_ENTRY_BAD");
    if (enabled == 2) decision->allowed = 0;
    if (enabled < 6) printf("AI_ENTRY_PREVIEW %u %u\n", c.party_slot, enabled);
}
static void PreviewStat(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhAiInnateStatContext c = {.struct_size = sizeof(c)};
    if (sAbilities->ai_innate_entry_stat_context(&c) || c.entry.ability != event->event_key
        || c.entry.party_slot != event->args[1] || c.entry.hp != event->args[2]
        || c.entry.offered || c.entry.capacity != RHH_AI_INNATE_ENTRY_STAT_LIMIT
        || !sAbilities->offer_ai_innate_entry_stat(RHH_STAT_HP, 1)
        || !sAbilities->offer_ai_innate_entry_stat(RHH_STAT_ATTACK, 0)
        || !sAbilities->offer_ai_innate_entry_stat(RHH_STAT_ATTACK, 7))
    { puts("AI_ENTRY_BAD"); return; }
    unsigned mode = *(const unsigned char *)state;
    if ((mode == 6 || mode == 7) && sAbilities->offer_ai_innate_entry_stat(RHH_STAT_ATTACK, 2)) puts("AI_ENTRY_BAD");
    if (mode == 7) decision->allowed = 0;
    if (mode == 9 || mode == 10 || mode == 11)
    {
        struct RhhAiInnateOpponentStatContext target = {.struct_size = sizeof(target)}, tiny = {.struct_size = 1, .battler = 999}, saved = tiny;
        if (sAbilities->ai_innate_entry_opponent_context(&target) || target.battler != 0
            || !sAbilities->ai_innate_entry_opponent_context(&tiny) || memcmp(&tiny, &saved, sizeof(tiny))
            || !sAbilities->offer_ai_innate_entry_opponent_stat(RHH_STAT_HP, -1)
            || !sAbilities->offer_ai_innate_entry_opponent_stat(RHH_STAT_ATTACK, 0)
            || sAbilities->offer_ai_innate_entry_opponent_stat(RHH_STAT_ATTACK, -1)) puts("AI_ENTRY_BAD");
        if (mode == 10) decision->allowed = 0;
        if (mode == 11)
        {
            if (target.stages[RHH_STAT_ATTACK] != 0) puts("AI_ENTRY_BAD");
            printf("AI_ENTRY_OPPONENT_CANDIDATE %u\n", c.entry.party_slot);
        }
    }
    if (mode == 12)
    {
        for (unsigned i = 0; i < RHH_AI_INNATE_ENTRY_STAT_LIMIT; ++i)
            if (sAbilities->offer_ai_innate_entry_opponent_stat(RHH_STAT_ATTACK, -1)) puts("AI_ENTRY_BAD");
        if (!sAbilities->offer_ai_innate_entry_stat(RHH_STAT_DEFENSE, 1)
            || !sAbilities->offer_ai_innate_entry_opponent_stat(RHH_STAT_DEFENSE, 1)) puts("AI_ENTRY_BAD");
    }
    if ((mode >= 20 && mode <= 23) || mode == 25 || mode == 26 || mode == 27)
    {
        struct RhhAiInnateOpponentStatusContext target = {.struct_size = sizeof(target)}, tiny = {.struct_size = 1, .battler = 999}, saved = tiny;
        unsigned status = mode == 22 ? RHH_EFFECT_PARALYSIS : mode == 25 ? RHH_EFFECT_TOXIC
            : mode == 26 ? RHH_EFFECT_SLEEP : RHH_EFFECT_BURN;
        if (sAbilities->ai_innate_entry_opponent_status_context(&target) || target.battler != 0
            || !sAbilities->ai_innate_entry_opponent_status_context(&tiny) || memcmp(&tiny, &saved, sizeof(tiny))
            || !sAbilities->offer_ai_innate_entry_opponent_status(0)
            || !sAbilities->offer_ai_innate_entry_opponent_status(RHH_EFFECT_CONFUSION)
            || sAbilities->offer_ai_innate_entry_opponent_status(status))
            puts("AI_ENTRY_BAD");
        if (mode == 21) decision->allowed = 0;
        if (mode == 23 && sAbilities->offer_ai_innate_entry_opponent_stat(RHH_STAT_ATTACK, -1)) puts("AI_ENTRY_BAD");
        if (mode == 27 && sAbilities->offer_ai_innate_entry_opponent_status(RHH_EFFECT_BURN)) puts("AI_ENTRY_BAD");
    }
    if (mode == 24)
    {
        struct RhhAiInnateOpponentStatusContext target = {.struct_size = sizeof(target)};
        if (sAbilities->ai_innate_entry_opponent_status_context(&target) || target.status
            || (c.entry.party_slot == 1 && sAbilities->offer_ai_innate_entry_opponent_status(RHH_EFFECT_BURN)))
            puts("AI_ENTRY_BAD");
        printf("AI_ENTRY_STATUS_CANDIDATE %u\n", c.entry.party_slot);
    }
}
static void PreviewTurnHeal(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhAiInnateEntryContext c = {.struct_size = sizeof(c)}, tiny = {.struct_size = 1, .hp = 999}, before = tiny;
    if (sAbilities->ai_innate_turn_heal_context(&c) || !sAbilities->ai_innate_turn_heal_context(&tiny)
        || memcmp(&tiny, &before, sizeof(tiny)) || c.ability != event->event_key
        || c.hp != event->args[2] || c.max_hp != event->args[3]
        || c.offered || c.capacity != RHH_AI_INNATE_ENTRY_HEAL_LIMIT
        || !sAbilities->ai_innate_entry_context(&tiny) || !sAbilities->offer_ai_innate_entry_heal(1, 2)
        || !sAbilities->offer_ai_innate_turn_heal(0, 1) || !sAbilities->offer_ai_innate_turn_heal(2, 1))
    { puts("AI_ENTRY_BAD"); return; }
    unsigned mode = *(const unsigned char *)state;
    if ((mode == 13 || mode == 14) && sAbilities->offer_ai_innate_turn_heal(1, 4)) puts("AI_ENTRY_BAD");
    if (mode == 19 && sAbilities->offer_ai_innate_turn_heal(1, 1)) puts("AI_ENTRY_BAD");
    if (mode == 14) decision->allowed = 0;
    if (mode == 15)
    {
        for (unsigned i = 0; i < RHH_AI_INNATE_ENTRY_HEAL_LIMIT; ++i)
            if (sAbilities->offer_ai_innate_turn_heal(1, 2)) puts("AI_ENTRY_BAD");
        if (!sAbilities->offer_ai_innate_turn_heal(1, 2)) puts("AI_ENTRY_BAD");
    }
}
static void PreviewTurnStep(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhAiInnateEntryContext c = {.struct_size = sizeof(c)}, tiny = {.struct_size = 1, .hp = 999}, before = tiny;
    if (sAbilities->ai_innate_turn_step_context(&c) || !sAbilities->ai_innate_turn_step_context(&tiny)
        || memcmp(&tiny, &before, sizeof(tiny)) || c.ability != event->event_key
        || c.hp != event->args[2] || c.max_hp != event->args[3] || !c.hp || c.hp > c.max_hp
        || c.offered || c.capacity != RHH_AI_INNATE_ENTRY_HEAL_LIMIT
        || !sAbilities->ai_innate_turn_heal_context(&tiny) || !sAbilities->offer_ai_innate_turn_heal(1, 2)
        || !sAbilities->offer_ai_innate_turn_step_heal(0, 1) || !sAbilities->offer_ai_innate_turn_step_heal(2, 1))
    { puts("AI_ENTRY_BAD"); return; }
    unsigned mode = *(const unsigned char *)state;
    if (mode == 35 && ++sFieldStepCount == 6) puts("AI_FIELD_FINITE_HEAL_EXPIRED");
    if ((mode == 16 || mode == 17) && c.hp <= c.max_hp / 2
        && sAbilities->offer_ai_innate_turn_step_heal(1, 4)) puts("AI_ENTRY_BAD");
    if (mode == 17) decision->allowed = 0;
    if (mode == 18)
    {
        for (unsigned i = 0; i < RHH_AI_INNATE_ENTRY_HEAL_LIMIT; ++i)
            if (sAbilities->offer_ai_innate_turn_step_heal(1, 2)) puts("AI_ENTRY_BAD");
        if (!sAbilities->offer_ai_innate_turn_step_heal(1, 2)) puts("AI_ENTRY_BAD");
    }
}
static void PreviewField(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    unsigned mode = *(const unsigned char *)state;
    if (mode < 28) return;
    if (mode == 35) sFieldStepCount = 0;
    struct RhhAiInnateFieldContext c = {.struct_size = sizeof(c)}, tiny = {.struct_size = 1, .weather = 999}, saved = tiny;
    struct RhhBattlerAbility hidden = {.struct_size = sizeof(hidden)};
    if (sAbilities->ai_innate_entry_field_context(&c) || !sAbilities->ai_innate_entry_field_context(&tiny)
        || memcmp(&tiny, &saved, sizeof(tiny)) || c.entry.ability != event->event_key
        || c.entry.battler != event->args[0] || c.entry.party_slot != event->args[1]
        || c.entry.hp != event->args[2] || c.entry.index != event->ordinal
        || !(c.entry.resolution.flags & RHH_INNATE_AI_KNOWLEDGE)
        || !sAbilities->battler(c.entry.battler, &hidden)
        || !sAbilities->offer_ai_innate_entry_weather(RHH_WEATHER_NONE)
        || !sAbilities->offer_ai_innate_entry_weather(RHH_WEATHER_WIND)
        || !sAbilities->offer_ai_innate_entry_terrain(RHH_TERRAIN_NONE)) { puts("AI_ENTRY_BAD"); return; }
    if (mode == 36 || mode == 37)
    {
        struct RhhAiInnateFieldContext projected = {.struct_size = sizeof(projected)};
        if (!sAbilities->ai_innate_entry_field_projected(&tiny) || memcmp(&tiny, &saved, sizeof(tiny))
            || sAbilities->ai_innate_entry_field_projected(&projected)) { puts("AI_ENTRY_BAD"); return; }
        CheckEntryGrounding(c.entry.battler, projected.terrain, 1, 0);
        if (event->event_key == sInnate)
        {
            if (projected.weather != RHH_WEATHER_SUN || projected.terrain != RHH_TERRAIN_NONE
                || sAbilities->offer_ai_innate_entry_weather(RHH_WEATHER_RAIN)
                || sAbilities->offer_ai_innate_entry_terrain(RHH_TERRAIN_ELECTRIC)
                || sAbilities->ai_innate_entry_field_projected(&projected)
                || projected.weather != RHH_WEATHER_RAIN || projected.terrain != RHH_TERRAIN_ELECTRIC
                || projected.entry.offered != 2) puts("AI_ENTRY_BAD");
            CheckEntryGrounding(c.entry.battler, projected.terrain, 1, 0);
            if (mode == 37) decision->allowed = 0;
        }
        else if (event->event_key == sSecondInnate)
        {
            if (c.weather != RHH_WEATHER_SUN || c.terrain != RHH_TERRAIN_NONE
                || projected.weather != (mode == 36 ? RHH_WEATHER_RAIN : RHH_WEATHER_SUN)
                || projected.terrain != (mode == 36 ? RHH_TERRAIN_ELECTRIC : RHH_TERRAIN_NONE)
                || projected.entry.offered != (mode == 36 ? 2 : 0)
                || projected.entry.capacity != RHH_AI_INNATE_ENTRY_FIELD_LIMIT - projected.entry.offered
                || sAbilities->offer_ai_innate_entry_terrain(RHH_TERRAIN_GRASSY)
                || sAbilities->ai_innate_entry_field_projected(&projected)
                || projected.terrain != RHH_TERRAIN_GRASSY || projected.terrain_turns != 5) puts("AI_ENTRY_BAD");
            CheckEntryGrounding(c.entry.battler, projected.terrain, 1, 0);
        }
        else puts("AI_ENTRY_BAD");
        return;
    }
    if (mode >= 39 && mode <= 43)
    {
        if (sAbilities->offer_ai_innate_entry_terrain(RHH_TERRAIN_ELECTRIC)) puts("AI_ENTRY_BAD");
        CheckEntryGrounding(c.entry.battler, RHH_TERRAIN_ELECTRIC, mode == 40 || mode == 41, mode == 41);
        return;
    }
    if (mode == 38) {
        struct RhhAiInnateFieldContext projected = {.struct_size = sizeof(projected)};
        if (sAbilities->offer_ai_innate_entry_weather(RHH_WEATHER_RAIN)
            || sAbilities->offer_ai_innate_entry_weather(RHH_WEATHER_SUN)
            || sAbilities->ai_innate_entry_field_projected(&projected)
            || projected.weather != RHH_WEATHER_SUN || projected.weather_turns != 5)
            puts("AI_ENTRY_BAD");
    } else if (mode == 30) {
        for (unsigned i = 0; i < RHH_AI_INNATE_ENTRY_FIELD_LIMIT; ++i)
            if (sAbilities->offer_ai_innate_entry_weather(i & 1 ? RHH_WEATHER_SUN : RHH_WEATHER_RAIN)) puts("AI_ENTRY_BAD");
        if (!sAbilities->offer_ai_innate_entry_terrain(RHH_TERRAIN_GRASSY)) puts("AI_ENTRY_BAD");
    } else {
        if (sAbilities->offer_ai_innate_entry_weather(RHH_WEATHER_RAIN)
            || sAbilities->offer_ai_innate_entry_terrain(RHH_TERRAIN_ELECTRIC)) puts("AI_ENTRY_BAD");
    }
    if (mode == 29) decision->allowed = 0;
    if (mode == 31 && !(c.flags & RHH_AI_INNATE_FIELD_WEATHER_BLOCKED)) puts("AI_ENTRY_BAD");
    if (mode == 32 && !(c.flags & RHH_AI_INNATE_FIELD_TERRAIN_BLOCKED)) puts("AI_ENTRY_BAD");
    if (mode == 33 && !(c.flags & RHH_AI_INNATE_FIELD_WEATHER_SUPPRESSED)) puts("AI_ENTRY_BAD");
    if (mode == 34 && c.weather != RHH_WEATHER_SUN) puts("AI_ENTRY_BAD");
    if (mode >= 31 && mode <= 33)
    {
        struct RhhAiInnateFieldContext projected = {.struct_size = sizeof(projected)};
        if (sAbilities->ai_innate_entry_field_projected(&projected)
            || (mode == 31 && (!(projected.flags & RHH_AI_INNATE_FIELD_WEATHER_BLOCKED) || projected.weather != c.weather))
            || (mode == 32 && (!(projected.flags & RHH_AI_INNATE_FIELD_TERRAIN_BLOCKED) || projected.terrain != c.terrain))
            || (mode == 33 && (!(projected.flags & RHH_AI_INNATE_FIELD_WEATHER_SUPPRESSED) || projected.weather != RHH_WEATHER_RAIN)))
            puts("AI_ENTRY_BAD");
    }
}
static const struct RhhModAbility abilities[] = {
    {sizeof(struct RhhModAbility), RHH_ABILITY_BREAKABLE, {0, 0}, "test:ability/ai_entry", "AI Entry", "Predictable entry healing."},
    {sizeof(struct RhhModAbility), RHH_ABILITY_BREAKABLE, {0, 0}, "test:ability/ai_entry_second", "AI Entry 2", "Second entry forecast."}
};
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_INNATE_PROPOSE, RHH_CONSTRAINT, 0, Offer, NULL},
    {sizeof(struct RhhModHook), RHH_AI_INNATE_ENTRY_PREVIEW, RHH_CONSTRAINT, 0, Preview, NULL},
    {sizeof(struct RhhModHook), RHH_AI_INNATE_ENTRY_STAT_PREVIEW, RHH_CONSTRAINT, 0, PreviewStat, NULL},
    {sizeof(struct RhhModHook), RHH_AI_INNATE_TURN_HEAL_PREVIEW, RHH_CONSTRAINT, 0, PreviewTurnHeal, NULL},
    {sizeof(struct RhhModHook), RHH_AI_INNATE_TURN_STEP_PREVIEW, RHH_CONSTRAINT, 0, PreviewTurnStep, NULL},
    {sizeof(struct RhhModHook), RHH_AI_INNATE_ENTRY_FIELD_PREVIEW, RHH_CONSTRAINT, 0, PreviewField, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 36), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle_grounding"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_ABILITIES,
    .id = "test:ai_entry", .state_size = 1, .hooks = hooks, .hook_count = 6,
    .abilities = abilities, .ability_count = 2, .service_requirements = requirements, .service_requirement_count = 2};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sAbilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 36));
    sGrounding = (const void *)host->get_service("rhh:service/battle_grounding", RHH_SERVICE_VERSION(1, 2));
    sSpecies = host->content_key("rhh:species/torchic"); sRainDishSpecies = host->content_key("rhh:species/lotad");
    sInnate = host->content_key("test:ability/ai_entry");
    sSecondInnate = host->content_key("test:ability/ai_entry_second");
    return sAbilities && sGrounding && sGrounding->header.struct_size >= RHH_BATTLE_GROUNDING_SERVICE_V1_2_SIZE
        && sSpecies && sRainDishSpecies && sInnate && sSecondInnate ? &definition : NULL;
}
