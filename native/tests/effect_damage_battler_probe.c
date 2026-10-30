#include "mod_api.h"
#include <stdio.h>

static const struct RhhMoveService *sMoves;
static RhhContentKey sSplash, sRain, sScratch;

static void Mode(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if ((event->kind == RHH_MOVE_EFFECT_MODE && (event->event_key == sSplash || event->event_key == sRain)))
        decision->value = RHH_MOVE_EFFECT_CUSTOM_STATUS;
    if (event->kind == RHH_MOVE_DAMAGE_EFFECT_MODE && event->event_key == sScratch)
        decision->value = RHH_MOVE_EFFECT_CUSTOM_DAMAGE;
}
static void Apply(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    if (event->event_key != sSplash && event->event_key != sRain && event->event_key != sScratch) return;
    const uint8_t *settings = state;
    struct RhhMoveEffectQueue before = {.struct_size = sizeof(before)};
    struct RhhMoveQueuedAction queued = {.struct_size = sizeof(queued)};
    if (sMoves->effect_queue(&before)) { puts("DAMAGE_BATTLER_BAD_QUEUE_VIEW"); return; }
    if (settings[3])
    {
        struct RhhMoveDamageEffectContext damage = {.struct_size = sizeof(damage)};
        int context = sMoves->effect_damage_context(&damage);
        if (!sMoves->effect_damage_from_hit(4, 1, 2, 0) || !sMoves->effect_damage_from_hit(settings[0], 0, 2, 0)
            || !sMoves->effect_damage_from_hit(settings[0], 1, 17, 0) || !sMoves->effect_damage_from_hit(settings[0], 1, 2, 8)
            || context || (!!(damage.flags & RHH_MOVE_DAMAGE_RECORDED)
                           == !!sMoves->effect_damage_from_hit(settings[0], 1, 2, settings[1]))) puts("DAMAGE_BATTLER_BAD_QUEUE");
        if (damage.flags & RHH_MOVE_DAMAGE_RECORDED) {
            uint32_t basis = damage.hp_lost + ((settings[1] & RHH_EFFECT_DAMAGE_FROM_HIT_INCLUDE_SUBSTITUTE) ? damage.substitute_lost : 0);
            uint32_t expected = basis ? (basis / 2 ? basis / 2 : 1) : 0;
            if (sMoves->effect_queue_at(before.action_count, &queued)
                || queued.kind != RHH_MOVE_QUEUED_DAMAGE_FROM_HIT || queued.recipient != settings[0]
                || queued.first != 1 || queued.second != 2 || queued.flags != settings[1]
                || queued.amount != expected || queued.move != event->event_key
                || queued.has_target != (event->event_key != sRain)) puts("DAMAGE_BATTLER_BAD_QUEUE_VIEW");
        }
        return;
    }
    if (!sMoves->effect_damage_battler(4, 1, 4, 0) || !sMoves->effect_damage_battler(settings[0], 0, 4, 0)
        || !sMoves->effect_damage_battler(settings[0], 1, 17, 0) || !sMoves->effect_damage_battler(settings[0], 1, 4, 4)
        || sMoves->effect_damage_battler(settings[0], 1, 4, settings[1])) puts("DAMAGE_BATTLER_BAD_QUEUE");
    if (sMoves->effect_queue_at(before.action_count, &queued) || queued.kind != RHH_MOVE_QUEUED_DAMAGE_BATTLER
        || queued.recipient != settings[0] || queued.first != 1 || queued.second != 4
        || queued.flags != settings[1] || queued.amount || queued.move != event->event_key
        || queued.has_target != (event->event_key != sRain))
        puts("DAMAGE_BATTLER_BAD_QUEUE_VIEW");
}
static void Preview(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)decision;
    if (event->event_key != sSplash && event->event_key != sRain && event->event_key != sScratch) return;
    const uint8_t *settings = state; uint32_t flags = settings[1] | (settings[2] ? 0 : RHH_AI_EFFECT_AUTO_SCORE);
    if (settings[3])
    {
        if (sMoves->ai_effect_offer_damage_from_hit(4, 1, 2, 100, 0) != RHH_AI_EFFECT_INVALID
            || sMoves->ai_effect_offer_damage_from_hit(settings[0], 1, 17, 100, 0) != RHH_AI_EFFECT_INVALID
            || sMoves->ai_effect_offer_damage_from_hit(settings[0], 1, 2, 0, 0) != RHH_AI_EFFECT_INVALID
            || sMoves->ai_effect_offer_damage_from_hit(settings[0], 1, 2, 100, 8) != RHH_AI_EFFECT_INVALID
            || sMoves->ai_effect_offer_damage_from_hit(settings[0], 1, 2, 100, flags) != RHH_AI_EFFECT_OK)
            puts("DAMAGE_BATTLER_BAD_PREVIEW");
        return;
    }
    struct RhhAiEffectAction invalid = {.struct_size = sizeof(invalid), .kind = RHH_AI_EFFECT_DAMAGE_BATTLER,
        .chance = 100, .first = 1, .second = 4};
    if (sMoves->ai_effect_offer(&invalid) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_damage_battler(4, 1, 4, 100, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_damage_battler(settings[0], 1, 17, 100, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_damage_battler(settings[0], 1, 4, 0, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_damage_battler(settings[0], 1, 4, 101, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_damage_battler(settings[0], 1, 4, 100, 4) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_damage_battler(settings[0], 1, 4, 100, flags) != RHH_AI_EFFECT_OK)
        puts("DAMAGE_BATTLER_BAD_PREVIEW");
}
static void Score(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    if (event->event_key != sSplash && event->event_key != sRain && event->event_key != sScratch) return;
    const uint8_t *settings = state; uint32_t count = 0, recipient = 999, unchanged = 999;
    struct RhhAiEffectAction action = {.struct_size = sizeof(action)};
    struct RhhAiEffectEstimate estimate = {.struct_size = sizeof(estimate)}, wrong = {.struct_size = sizeof(wrong), .median = 999};
    if (settings[3])
    {
        if (sMoves->ai_effect_count(&count) || count != 1 || sMoves->ai_effect_at(0, &action)
            || action.kind != RHH_AI_EFFECT_DAMAGE_FROM_HIT_BATTLER || action.first != 1 || action.second != 2
            || sMoves->ai_effect_damage_from_hit_at(0, &recipient) || recipient != settings[0]
            || !sMoves->ai_effect_damage_battler_at(0, &unchanged) || unchanged != 999
            || sMoves->ai_effect_estimate(0, recipient, &estimate)
            || !(estimate.flags & RHH_AI_ESTIMATE_DAMAGE_CACHE)
            || !sMoves->ai_effect_estimate(0, (recipient + 1) % 4, &wrong) || wrong.median != 999)
        { puts("DAMAGE_BATTLER_BAD_ESTIMATE"); return; }
        if (settings[2]) decision->value = 100 + (settings[2] == 1 ? estimate.minimum : settings[2] == 2 ? estimate.median : estimate.maximum);
        return;
    }
    if (sMoves->ai_effect_count(&count) || count != 1 || sMoves->ai_effect_at(0, &action)
        || action.kind != RHH_AI_EFFECT_DAMAGE_BATTLER || action.first != 1 || action.second != 4
        || sMoves->ai_effect_damage_battler_at(0, &recipient) || recipient != settings[0]
        || !sMoves->ai_effect_heal_battler_at(0, &unchanged) || unchanged != 999
        || sMoves->ai_effect_estimate(0, recipient, &estimate)
        || !sMoves->ai_effect_estimate(0, (recipient + 1) % 4, &wrong) || wrong.median != 999)
    { puts("DAMAGE_BATTLER_BAD_ESTIMATE"); return; }
    if (settings[2])
    {
        int value = settings[2] == 1 ? estimate.minimum : settings[2] == 2 ? estimate.median : estimate.maximum;
        decision->value = 100 + value;
    }
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_EFFECT_MODE, RHH_TRANSFORM, 0, Mode, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_DAMAGE_EFFECT_MODE, RHH_TRANSFORM, 0, Mode, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_EFFECT_APPLY, RHH_OBSERVE, 0, NULL, Apply},
    {sizeof(struct RhhModHook), RHH_MOVE_DAMAGE_EFFECT_APPLY, RHH_OBSERVE, 0, NULL, Apply},
    {sizeof(struct RhhModHook), RHH_AI_EFFECT_PREVIEW, RHH_CONSTRAINT, 0, Preview, NULL},
    {sizeof(struct RhhModHook), RHH_AI_MOVE_SCORE, RHH_TRANSFORM, 0, Score, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 50), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition),
    .abi_version = RHH_MOD_ABI_DAMAGE_EFFECTS, .id = "test:damage_battler", .hooks = sHooks, .hook_count = 6,
    .state_size = 4, .state_schema = 1, .service_requirements = sRequirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 50));
    sSplash = host->content_key("rhh:move/splash"); sRain = host->content_key("rhh:move/rain_dance");
    sScratch = host->content_key("rhh:move/scratch");
    return sMoves && sMoves->header.struct_size >= RHH_MOVE_SERVICE_V1_50_SIZE && sSplash && sRain && sScratch
        ? &sDefinition : NULL;
}
