#include "mod_api.h"
#include <stdio.h>

static const struct RhhAbilityService *sAbilities;
static RhhContentKey sTorchic, sAbility;
static void Offer(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->event_key != sTorchic) return;
    struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .unlock_level = 10, .ability = sAbility};
    if (sAbilities->offer_innate(&offer)) puts("INNATE_STATUS_BAD");
}
static void Turn(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision;
    struct RhhInnateTurnContext context = {.struct_size = sizeof(context)};
    if (sAbilities->innate_turn_context(&context) || context.battler != 0
        || !sAbilities->innate_turn_status_target(9, RHH_EFFECT_BURN)
        || !sAbilities->innate_turn_status_target(1, 0)
        || !sAbilities->innate_turn_status_target(1, RHH_EFFECT_CONFUSION)
        || sAbilities->innate_turn_status_target(1, *(const unsigned char *)state))
        puts("INNATE_STATUS_BAD");
}
static void Entry(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision;
    struct RhhInnateEntryContext context = {.struct_size = sizeof(context)};
    unsigned mode = *(const unsigned char *)state;
    if (sAbilities->innate_entry_context(&context) || context.battler != 0
        || !sAbilities->innate_entry_status_target(9, RHH_EFFECT_BURN)
        || !sAbilities->innate_entry_status_target(1, 0)
        || !sAbilities->innate_entry_status_target(1, RHH_EFFECT_CONFUSION))
        puts("INNATE_STATUS_BAD");
    if (mode & 128)
    {
        if (sAbilities->innate_entry_heal(1, 8) || sAbilities->innate_entry_stat(RHH_STAT_ATTACK, 1)) puts("INNATE_STATUS_BAD");
        for (unsigned i = 0; i < 8; ++i)
            if (sAbilities->innate_entry_status_target(1, mode & 127)) puts("INNATE_STATUS_BAD");
        if (!sAbilities->innate_entry_status_target(1, mode & 127)) puts("INNATE_STATUS_BAD");
    }
    else if (sAbilities->innate_entry_status_target(1, mode)) puts("INNATE_STATUS_BAD");
}
static const struct RhhModAbility abilities[] = {
    {sizeof(struct RhhModAbility), RHH_ABILITY_BREAKABLE, {0, 0}, "test:ability/turn_status", "Turn Status", "Applies a status."}
};
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_INNATE_PROPOSE, RHH_CONSTRAINT, 0, Offer, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_TURN_END, RHH_OBSERVE, 0, NULL, Turn},
    {sizeof(struct RhhModHook), RHH_INNATE_ENTRY, RHH_OBSERVE, 0, NULL, Entry}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 26), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_ABILITIES,
    .id = "test:innate_status", .state_size = 1, .hook_count = 3, .hooks = hooks, .ability_count = 1, .abilities = abilities,
    .service_requirement_count = 1, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sAbilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 26));
    sTorchic = host->content_key("rhh:species/torchic"); sAbility = host->content_key("test:ability/turn_status");
    return sAbilities && sTorchic && sAbility ? &definition : NULL;
}
