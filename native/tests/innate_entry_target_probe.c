#include "mod_api.h"
#include <stdio.h>

static const struct RhhAbilityService *sAbilities;
static RhhContentKey sTorchic, sAbility;
static void Offer(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->event_key != sTorchic) return;
    struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .unlock_level = 1, .ability = sAbility};
    if (sAbilities->offer_innate(&offer)) puts("ENTRY_TARGET_BAD");
}
static void Entry(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhInnateEntryContext context = {.struct_size = sizeof(context)};
    if (sAbilities->innate_entry_context(&context) || context.battler != 0 || context.ability != sAbility)
    { puts("ENTRY_TARGET_BAD"); return; }
    if (event->args[2] == 99)
    {
        for (unsigned i = 0; i < 8; ++i)
            if (sAbilities->innate_entry_heal_target(2, 1, 4) || sAbilities->innate_entry_stat_target(2, RHH_STAT_DEFENSE, 1))
                puts("ENTRY_TARGET_BAD");
        if (!sAbilities->innate_entry_heal(1, 4) || !sAbilities->innate_entry_stat(RHH_STAT_DEFENSE, 1)) puts("ENTRY_TARGET_BAD");
        puts("ENTRY_TARGET_CAPACITY"); return;
    }
    if (!sAbilities->innate_entry_heal_target(4, 1, 4) || !sAbilities->innate_entry_heal_target(3, 1, 4)
        || !sAbilities->innate_entry_heal_target(2, 0, 4) || !sAbilities->innate_entry_stat_target(2, RHH_STAT_HP, 1)
        || !sAbilities->innate_entry_stat_target(2, RHH_STAT_ATTACK, 0)
        || sAbilities->innate_entry_heal_target(2, 1, 4)
        || sAbilities->innate_entry_stat_target(2, RHH_STAT_DEFENSE, 1)) puts("ENTRY_TARGET_BAD");
    puts("ENTRY_TARGET_QUEUED");
}
static const struct RhhModAbility abilities[] = {
    {sizeof(struct RhhModAbility), RHH_ABILITY_BREAKABLE, {0, 0}, "test:ability/entry_target", "Entry Target", "A targeted entry effect."}
};
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_INNATE_PROPOSE, RHH_CONSTRAINT, 0, Offer, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_ENTRY, RHH_OBSERVE, 0, NULL, Entry}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 17), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_ABILITIES,
    .id = "test:innate_entry_target", .hook_count = 2, .hooks = hooks, .ability_count = 1, .abilities = abilities,
    .service_requirement_count = 1, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sAbilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 17));
    sTorchic = host->content_key("rhh:species/torchic"); sAbility = host->content_key("test:ability/entry_target");
    return sAbilities && sTorchic && sAbility ? &definition : NULL;
}
