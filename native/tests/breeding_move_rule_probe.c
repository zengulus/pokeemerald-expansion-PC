#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhBreedingService *sBreeding;
static RhhContentKey sShadow, sUnknown, sCustom, sHydro, sToxic;
static void Rule(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhBreedingMoveRuleContext context = {.struct_size = sizeof(context)};
    struct RhhBreedingMoveRuleContext small = {.struct_size = 1, .slot = 99}, before = small;
    struct RhhBreedingContext staged = {.struct_size = sizeof(staged)};
    struct RhhBreedingOffer nested = {.struct_size = sizeof(nested)};
    struct RhhBreedingPreview preview = {.struct_size = sizeof(preview)};
    if (sBreeding->move_rule_context(&context) || !sBreeding->move_rule_context(&small) || memcmp(&small, &before, sizeof(small))
        || context.slot != event->ordinal || context.offspring != event->event_key || context.original_offspring != event->subject
        || context.compatibility != event->args[0] || context.parent_slots[0] != event->args[1]
        || context.parent_slots[1] != event->args[2] || !sBreeding->context(&staged)
        || !sBreeding->offer(&nested) || !sBreeding->preview(&preview))
    { puts("BREED_MOVE_RULE_BAD_CONTEXT"); return; }
    unsigned mode = *(const unsigned char *)state;
    if ((mode == 1 || mode == 6) && context.slot == 3) decision->value = sShadow;
    if (mode == 2 && context.slot == 3) decision->value = sUnknown;
    if (mode == 3 && context.slot == 3) decision->value = context.base_moves[0];
    if (mode == 4 || (mode == 5 && context.slot == 0)) decision->value = 0;
    if (mode == 7 && context.slot == 0) decision->value = sCustom;
    if (mode == 7 && context.slot > 0 && context.base_moves[context.slot] == sCustom) decision->value = 0;
    if (mode == 8 && context.slot == 3) decision->value = sHydro;
    if (mode == 9 && context.slot == 3) decision->value = sToxic;
}
static void Deny(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    if (*(const unsigned char *)state == 6 && event->ordinal == 3) decision->allowed = 0;
    decision->value = sUnknown;
}
static void Later(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->allowed = 1; }
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BREED_MOVE_RULE, RHH_TRANSFORM, 0, Rule, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_MOVE_RULE, RHH_CONSTRAINT, 0, Deny, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_MOVE_RULE, RHH_CONSTRAINT, 1, Later, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 12), RHH_SERVICE_REQUIRED, 0, "rhh:service/breeding"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_move_rule", .state_size = 1, .hooks = hooks, .hook_count = 3,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sBreeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 12));
    sShadow = host->content_key("rhh:move/shadow_ball"); sUnknown = host->content_key("test:move/not_registered");
    sCustom = host->content_key("test:move/surge");
    sHydro = host->content_key("rhh:move/hydro_pump"); sToxic = host->content_key("rhh:move/toxic");
    return sBreeding && sBreeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_12_SIZE
        && sShadow && sUnknown && sCustom && sHydro && sToxic ? &definition : NULL;
}
