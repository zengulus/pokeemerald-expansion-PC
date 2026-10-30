#include "mod_api.h"
#include <stdio.h>

static const struct RhhMoveService *sMoves;
static RhhContentKey sSplash, sRain;

static void Mode(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sSplash || event->event_key == sRain) decision->value = RHH_MOVE_EFFECT_CUSTOM_STATUS;
}
static void Targets(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sSplash) decision->value = RHH_MOVE_TARGET_USER_AND_ALLY;
}
static void Apply(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (event->event_key != sSplash && event->event_key != sRain) return;
    if (!sMoves->effect_heal_battler(4, 1, 4) || !sMoves->effect_heal_battler(2, 0, 4)
        || !sMoves->effect_heal_battler(2, 2, 1) || !sMoves->effect_heal_battler(2, 1, 17)
        || sMoves->effect_heal_battler(2, 1, 4)) puts("HEAL_BATTLER_BAD_QUEUE");
}
static void Preview(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)decision;
    if (event->event_key != sSplash && event->event_key != sRain) return;
    uint32_t recipient = *(const uint8_t *)state;
    if (((const uint8_t *)state)[1])
    {
        struct RhhAiEffectAction user = {.struct_size = sizeof(user), .kind = RHH_AI_EFFECT_HEAL_USER,
            .chance = 100, .first = 1, .second = 4};
        for (uint32_t i = 0; i < RHH_AI_EFFECT_PREVIEW_LIMIT; ++i)
        {
            int status = i & 1 ? sMoves->ai_effect_offer_heal_battler(recipient, 1, 4, 100, 0)
                               : sMoves->ai_effect_offer(&user);
            if (status != RHH_AI_EFFECT_OK) puts("HEAL_BATTLER_BAD_CAPACITY");
        }
        if (sMoves->ai_effect_offer(&user) != RHH_AI_EFFECT_FULL
            || sMoves->ai_effect_offer_heal_battler(recipient, 1, 4, 100, 0) != RHH_AI_EFFECT_FULL)
            puts("HEAL_BATTLER_BAD_CAPACITY");
        return;
    }
    struct RhhAiEffectAction invalid = {.struct_size = sizeof(invalid), .kind = RHH_AI_EFFECT_HEAL_BATTLER,
        .chance = 100, .first = 1, .second = 4};
    if (sMoves->ai_effect_offer(&invalid) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_heal_battler(4, 1, 4, 100, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_heal_battler(recipient, 0, 4, 100, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_heal_battler(recipient, 1, 17, 100, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_heal_battler(recipient, 1, 4, 0, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_heal_battler(recipient, 1, 4, 101, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_heal_battler(recipient, 1, 4, 100, 4) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_heal_battler(recipient, 1, 4, 100, RHH_AI_EFFECT_AUTO_SCORE) != RHH_AI_EFFECT_OK)
        puts("HEAL_BATTLER_BAD_PREVIEW");
}
static void Score(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)decision;
    if (event->event_key != sSplash && event->event_key != sRain) return;
    uint32_t wanted = *(const uint8_t *)state, recipient = 999, unchanged = 999, count = 0;
    struct RhhAiEffectAction action = {.struct_size = sizeof(action)};
    struct RhhAiEffectEstimate estimate = {.struct_size = sizeof(estimate)}, wrong = {.struct_size = sizeof(wrong), .median = 999};
    if (((const uint8_t *)state)[1])
    {
        if (sMoves->ai_effect_count(&count) || count != RHH_AI_EFFECT_PREVIEW_LIMIT
            || !sMoves->ai_effect_heal_battler_at(0, &unchanged) || unchanged != 999
            || sMoves->ai_effect_heal_battler_at(1, &recipient) || recipient != wanted)
            puts("HEAL_BATTLER_BAD_CAPACITY");
        return;
    }
    if (sMoves->ai_effect_count(&count) || count != 1 || sMoves->ai_effect_at(0, &action)
        || action.kind != RHH_AI_EFFECT_HEAL_BATTLER || action.first != 1 || action.second != 4
        || sMoves->ai_effect_heal_battler_at(0, &recipient) || recipient != wanted
        || !sMoves->ai_effect_heal_battler_at(1, &unchanged) || unchanged != 999
        || sMoves->ai_effect_estimate(0, recipient, &estimate)
        || !sMoves->ai_effect_estimate(0, (recipient + 1) % 4, &wrong) || wrong.median != 999)
        puts("HEAL_BATTLER_BAD_ESTIMATE");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_EFFECT_MODE, RHH_TRANSFORM, 0, Mode, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TARGET, RHH_TRANSFORM, 0, Targets, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_EFFECT_APPLY, RHH_OBSERVE, 0, NULL, Apply},
    {sizeof(struct RhhModHook), RHH_AI_EFFECT_PREVIEW, RHH_CONSTRAINT, 0, Preview, NULL},
    {sizeof(struct RhhModHook), RHH_AI_MOVE_SCORE, RHH_TRANSFORM, 0, Score, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 27), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:heal_battler", .hooks = sHooks, .hook_count = 5,
    .state_size = 2, .state_schema = 1,
    .service_requirements = sRequirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 27));
    sSplash = host->content_key("rhh:move/splash");
    sRain = host->content_key("rhh:move/rain_dance");
    return sMoves && sMoves->header.struct_size >= RHH_MOVE_SERVICE_V1_27_SIZE && sSplash && sRain ? &sDefinition : NULL;
}
