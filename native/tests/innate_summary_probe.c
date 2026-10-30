#include "mod_api.h"
#include <stdio.h>
static const struct RhhAbilityService *abilities;
static RhhContentKey torrent, levitate, custom;
static void Propose(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .ability = torrent, .unlock_level = 1};
    if (abilities->offer_innate(&offer) != RHH_INNATE_OK) { puts("SUMMARY_INNATE_BAD"); return; }
    offer.ability = levitate; offer.unlock_level = 30;
    if (abilities->offer_innate(&offer) != RHH_INNATE_OK) { puts("SUMMARY_INNATE_BAD"); return; }
    puts("SUMMARY_INNATE_OFFERED");
}
static void Personalize(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhInnatePersonalContext context = {.struct_size = sizeof(context)};
    struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .ability = custom, .unlock_level = 10};
    if (abilities->personal_context(&context) || abilities->offer_innate(&offer) != RHH_INNATE_OK)
        puts("SUMMARY_INNATE_BAD");
    else puts("SUMMARY_PERSONAL_OFFERED");
}
static const struct RhhModAbility definitions[] = {
    {sizeof(struct RhhModAbility), 0, {0, 0}, "test:ability/summary_focus", "Summary Focus Extended Ability", "A test ability."}
};
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_INNATE_PROPOSE, RHH_CONSTRAINT, 0, Propose, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_PERSONALIZE, RHH_CONSTRAINT, 0, Personalize, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 11), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_ABILITIES,
    .id = "test:innate_summary", .hooks = hooks, .hook_count = 2, .abilities = definitions, .ability_count = 1,
    .service_requirements = requirements, .service_requirement_count = 1};
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    abilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 11));
    torrent = host->content_key("rhh:ability/torrent"); levitate = host->content_key("rhh:ability/levitate");
    custom = host->content_key("test:ability/summary_focus");
    return abilities && torrent && levitate && custom ? &definition : NULL;
}
