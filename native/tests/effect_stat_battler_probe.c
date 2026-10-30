#include "mod_api.h"
#include <stdio.h>

static const struct RhhMoveService *sMoves;
static RhhContentKey sSplash, sRain;

static void Mode(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sSplash || event->event_key == sRain) decision->value = RHH_MOVE_EFFECT_CUSTOM_STATUS;
}
static void Apply(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    if (event->event_key != sSplash && event->event_key != sRain) return;
    const uint8_t *settings = state; int stages = (int)settings[1] - 6;
    if (!sMoves->effect_stat_battler(4, RHH_STAT_ATTACK, 1)
        || !sMoves->effect_stat_battler(settings[0], RHH_STAT_HP, 1)
        || !sMoves->effect_stat_battler(settings[0], RHH_STAT_ATTACK, 0)
        || !sMoves->effect_stat_battler(settings[0], RHH_STAT_ATTACK, 7)
        || sMoves->effect_stat_battler(settings[0], RHH_STAT_ATTACK, stages)) puts("STAT_BATTLER_BAD_QUEUE");
}
static void Preview(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)decision;
    if (event->event_key != sSplash && event->event_key != sRain) return;
    const uint8_t *settings = state; int stages = (int)settings[1] - 6;
    struct RhhAiEffectAction invalid = {.struct_size = sizeof(invalid), .kind = RHH_AI_EFFECT_STAT_BATTLER,
        .chance = 100, .first = RHH_STAT_ATTACK, .stages = 1};
    if (sMoves->ai_effect_offer(&invalid) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_stat_battler(4, RHH_STAT_ATTACK, 1, 100, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_stat_battler(settings[0], RHH_STAT_HP, 1, 100, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_stat_battler(settings[0], RHH_STAT_ATTACK, 0, 100, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_stat_battler(settings[0], RHH_STAT_ATTACK, 1, 0, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_stat_battler(settings[0], RHH_STAT_ATTACK, 1, 100, 4) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_stat_battler(settings[0], RHH_STAT_ATTACK, stages, 100, RHH_AI_EFFECT_AUTO_SCORE) != RHH_AI_EFFECT_OK)
        puts("STAT_BATTLER_BAD_PREVIEW");
}
static void Score(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)decision;
    if (event->event_key != sSplash && event->event_key != sRain) return;
    const uint8_t *settings = state; uint32_t count = 0, recipient = 999, unchanged = 999;
    struct RhhAiEffectAction action = {.struct_size = sizeof(action)};
    struct RhhAiEffectEstimate estimate = {.struct_size = sizeof(estimate)}, wrong = {.struct_size = sizeof(wrong), .median = 999};
    if (sMoves->ai_effect_count(&count) || count != 1 || sMoves->ai_effect_at(0, &action)
        || action.kind != RHH_AI_EFFECT_STAT_BATTLER || action.first != RHH_STAT_ATTACK
        || action.stages != (int)settings[1] - 6 || sMoves->ai_effect_stat_battler_at(0, &recipient)
        || recipient != settings[0] || !sMoves->ai_effect_heal_battler_at(0, &unchanged) || unchanged != 999
        || sMoves->ai_effect_estimate(0, recipient, &estimate)
        || !sMoves->ai_effect_estimate(0, (recipient + 1) % 4, &wrong) || wrong.median != 999)
        puts("STAT_BATTLER_BAD_ESTIMATE");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_EFFECT_MODE, RHH_TRANSFORM, 0, Mode, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_EFFECT_APPLY, RHH_OBSERVE, 0, NULL, Apply},
    {sizeof(struct RhhModHook), RHH_AI_EFFECT_PREVIEW, RHH_CONSTRAINT, 0, Preview, NULL},
    {sizeof(struct RhhModHook), RHH_AI_MOVE_SCORE, RHH_TRANSFORM, 0, Score, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 31), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:stat_battler", .hooks = sHooks, .hook_count = 4,
    .state_size = 2, .state_schema = 1, .service_requirements = sRequirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 31));
    sSplash = host->content_key("rhh:move/splash"); sRain = host->content_key("rhh:move/rain_dance");
    return sMoves && sMoves->header.struct_size >= RHH_MOVE_SERVICE_V1_31_SIZE && sSplash && sRain ? &sDefinition : NULL;
}
