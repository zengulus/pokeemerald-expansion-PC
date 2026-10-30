#include "mod_api.h"
#include <stdio.h>

static const struct RhhAbilityService *sAbilities;
static RhhContentKey sTorchic, sAbility;
static void Offer(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->event_key != sTorchic) return;
    struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .unlock_level = 10, .ability = sAbility};
    if (sAbilities->offer_innate(&offer)) puts("SOURCED_STAT_BAD");
}
static void Entry(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhInnateEntryContext context = {.struct_size = sizeof(context)};
    if (sAbilities->innate_entry_context(&context) || context.battler != 0
        || !sAbilities->innate_entry_stat_from(9, RHH_STAT_ATTACK, -1)
        || !sAbilities->innate_entry_stat_from(1, RHH_STAT_HP, -1)
        || sAbilities->innate_entry_stat_from(1, RHH_STAT_ATTACK, -1)) puts("SOURCED_STAT_BAD");
    puts("SOURCED_STAT_ENTRY");
}
static void Turn(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhInnateTurnContext context = {.struct_size = sizeof(context)};
    if (sAbilities->innate_turn_context(&context) || context.battler != 0
        || !sAbilities->innate_turn_stat_from(9, RHH_STAT_ATTACK, -1)
        || !sAbilities->innate_turn_stat_from(1, RHH_STAT_ATTACK, 0)
        || sAbilities->innate_turn_stat_from(1, RHH_STAT_ATTACK, -1)) puts("SOURCED_STAT_BAD");
    puts("SOURCED_STAT_TURN");
}
static const struct RhhModAbility abilities[] = {
    {sizeof(struct RhhModAbility), RHH_ABILITY_BREAKABLE, {0, 0}, "test:ability/sourced_stat", "Sourced Stat", "A sourced stat change."}
};
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_INNATE_PROPOSE, RHH_CONSTRAINT, 0, Offer, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_ENTRY, RHH_OBSERVE, 0, NULL, Entry},
    {sizeof(struct RhhModHook), RHH_INNATE_TURN_END, RHH_OBSERVE, 0, NULL, Turn}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 18), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_ABILITIES,
    .id = "test:innate_sourced_stat", .hook_count = 3, .hooks = hooks, .ability_count = 1, .abilities = abilities,
    .service_requirement_count = 1, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sAbilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 18));
    sTorchic = host->content_key("rhh:species/torchic"); sAbility = host->content_key("test:ability/sourced_stat");
    return sAbilities && sTorchic && sAbility ? &definition : NULL;
}
