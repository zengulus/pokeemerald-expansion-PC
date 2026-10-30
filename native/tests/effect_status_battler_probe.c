#include "mod_api.h"
#include <stdio.h>

static const struct RhhMoveService *sMoves;
static const struct RhhTypeService *sTypes;
static RhhContentKey sSplash, sRain, sScratch;
static RhhContentKey sSteel, sDark;
static RhhContentKey sMagnemite, sZoroark;
static void CheckKnownSpecies(const uint8_t *settings)
{
    if (!settings[3]) return;
    RhhContentKey known = 0, unchanged = 99;
    if (sTypes->ai_battler_species(settings[0], &known)
        || known != (settings[3] == 1 ? sMagnemite : sZoroark)
        || !sTypes->ai_battler_species(4, &unchanged) || unchanged != 99
        || !sTypes->ai_battler_species(settings[0], NULL)) puts("STATUS_BATTLER_BAD_KNOWN_SPECIES");
}
static void Mode(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->kind == RHH_MOVE_EFFECT_MODE && (event->event_key == sSplash || event->event_key == sRain))
        decision->value = RHH_MOVE_EFFECT_CUSTOM_STATUS;
    if (event->kind == RHH_MOVE_DAMAGE_EFFECT_MODE && event->event_key == sScratch)
        decision->value = RHH_MOVE_EFFECT_CUSTOM_DAMAGE;
}
static void Apply(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    if (event->event_key != sSplash && event->event_key != sRain && event->event_key != sScratch) return;
    const uint8_t *settings = state;
    if (!sMoves->effect_status_battler(4, RHH_EFFECT_TOXIC)
        || !sMoves->effect_status_battler(settings[0], 0)
        || !sMoves->effect_status_battler(settings[0], RHH_EFFECT_CONFUSION + 1)
        || sMoves->effect_status_battler(settings[0], settings[1])) puts("STATUS_BATTLER_BAD_QUEUE");
}
static void Preview(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)decision;
    if (event->event_key != sSplash && event->event_key != sRain && event->event_key != sScratch) return;
    const uint8_t *settings = state;
    CheckKnownSpecies(settings);
    struct RhhAiEffectAction invalid = {.struct_size = sizeof(invalid), .kind = RHH_AI_EFFECT_STATUS_BATTLER,
        .chance = 100, .first = RHH_EFFECT_TOXIC};
    if (sMoves->ai_effect_offer(&invalid) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_status_battler(4, RHH_EFFECT_TOXIC, 100, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_status_battler(settings[0], 0, 100, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_status_battler(settings[0], RHH_EFFECT_CONFUSION + 1, 100, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_status_battler(settings[0], RHH_EFFECT_TOXIC, 0, 0) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_status_battler(settings[0], RHH_EFFECT_TOXIC, 100, 4) != RHH_AI_EFFECT_INVALID
        || sMoves->ai_effect_offer_status_battler(settings[0], settings[1], 100, RHH_AI_EFFECT_AUTO_SCORE) != RHH_AI_EFFECT_OK)
        puts("STATUS_BATTLER_BAD_PREVIEW");
}
static void Score(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)decision;
    if (event->event_key != sSplash && event->event_key != sRain && event->event_key != sScratch) return;
    const uint8_t *settings = state; uint32_t count = 0, recipient = 999, unchanged = 999;
    CheckKnownSpecies(settings);
    struct RhhAiEffectAction action = {.struct_size = sizeof(action)};
    struct RhhAiEffectEstimate estimate = {.struct_size = sizeof(estimate)}, wrong = {.struct_size = sizeof(wrong), .median = 999};
    if (sMoves->ai_effect_count(&count) || count != 1 || sMoves->ai_effect_at(0, &action)
        || action.kind != RHH_AI_EFFECT_STATUS_BATTLER || action.first != settings[1]
        || sMoves->ai_effect_status_battler_at(0, &recipient) || recipient != settings[0]
        || !sMoves->ai_effect_stat_battler_at(0, &unchanged) || unchanged != 999
        || sMoves->ai_effect_estimate(0, recipient, &estimate)
        || !sMoves->ai_effect_estimate(0, (recipient + 1) % 4, &wrong) || wrong.median != 999)
        puts("STATUS_BATTLER_BAD_ESTIMATE");
    if (settings[2]) {
        struct RhhExtendedTypeList types = {.struct_size = sizeof(types)};
        RhhContentKey expected = settings[2] == 1 ? sSteel : sDark;
        int found = 0;
        if (sTypes->ai_battler_list(recipient, &types)) puts("STATUS_BATTLER_BAD_KNOWN_TYPES");
        for (uint32_t i = 0; i < types.count; ++i) if (types.types[i] == expected) found = 1;
        if (!found) puts("STATUS_BATTLER_BAD_KNOWN_TYPES");
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
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 32), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 13), RHH_SERVICE_REQUIRED, 0, "rhh:service/types"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition),
    .abi_version = RHH_MOD_ABI_DAMAGE_EFFECTS, .id = "test:status_battler", .hooks = sHooks, .hook_count = 6,
    .state_size = 4, .state_schema = 1, .service_requirements = sRequirements, .service_requirement_count = 2};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 32));
    sTypes = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 13));
    sSplash = host->content_key("rhh:move/splash"); sRain = host->content_key("rhh:move/rain_dance");
    sScratch = host->content_key("rhh:move/scratch");
    sSteel = host->content_key("rhh:type/steel"); sDark = host->content_key("rhh:type/dark");
    sMagnemite = host->content_key("rhh:species/magnemite"); sZoroark = host->content_key("rhh:species/zoroark");
    return sMoves && sMoves->header.struct_size >= RHH_MOVE_SERVICE_V1_32_SIZE
        && sTypes && sTypes->header.struct_size >= RHH_TYPE_SERVICE_V1_13_SIZE && sTypes->ai_battler_list && sTypes->ai_battler_species
        && sSplash && sRain && sScratch && sSteel && sDark && sMagnemite && sZoroark
        ? &sDefinition : NULL;
}
