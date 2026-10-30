/* A separate mod changes the selected ace's species and checks its origin. */
#include "mod_api.h"
#include <stdio.h>
static const struct RhhBattleService *battle;
static RhhContentKey trainer, roster, original, replacement;
static unsigned starts;
static void Transform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == trainer && event->ordinal == 1) decision->value = replacement;
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhTrainerPartySource ace = {.struct_size = sizeof(ace)}, lead = {.struct_size = sizeof(lead)};
    if (starts++) {
        if (battle->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_B, 0, &ace)
            || ace.trainer != roster || ace.roster_trainer != roster || ace.source_index != 0)
            puts("TRAINER_POOL_PROBE_BAD_MULTI");
        else puts("TRAINER_POOL_MULTI_OK");
        return;
    }
    if (battle->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 0, &lead)
        || battle->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &ace)
        || lead.source_index != 3 || lead.flags != RHH_TRAINER_SOURCE_LEAD
        || ace.source_index != 1 || ace.source_count != 4 || ace.flags != RHH_TRAINER_SOURCE_ACE
        || ace.trainer != trainer || ace.roster_trainer != roster
        || ace.source_species != original || ace.generated_species != replacement)
        puts("TRAINER_POOL_PROBE_BAD");
    else puts("TRAINER_POOL_PROBE_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_TRAINER_SPECIES, RHH_TRANSFORM, 100, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:trainer_pool",
    .hook_count = 2, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    battle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 1));
    trainer = host->content_key("rhh:trainer/roxanne_1");
    roster = host->content_key("rhh:trainer/brawly_1");
    original = host->content_key("rhh:species/zigzagoon");
    replacement = host->content_key("rhh:species/mudkip");
    return battle && battle->header.struct_size >= sizeof(*battle) ? &definition : NULL;
}
