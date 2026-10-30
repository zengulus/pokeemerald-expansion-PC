#include <stdio.h>
#include "mod_api.h"
static const struct RhhAbilityService *sAbilities;
struct ProbeState { uint64_t battle, next; };
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct ProbeState *probe = state;
    struct RhhGasChange change = {.struct_size = sizeof(change)};
    if (sAbilities->gas_change_context(&change)) { puts("ABILITY_GAS_EXTERNAL_BAD"); return; }
    if (probe->battle != change.battle_id) { probe->battle = change.battle_id; probe->next = 0; }
    if (change.sequence != probe->next++ || event->ordinal != change.sequence
        || change.cause != event->args[0] || change.source_battler != event->args[1]
        || change.before.source_mask != event->args[2] || change.after.source_mask != event->args[3]
        || change.before.source_mask == change.after.source_mask || change.battle_id != change.before.battle_id
        || change.battle_id != change.after.battle_id || change.move != event->event_key)
        puts("ABILITY_GAS_EXTERNAL_BAD");
    else puts("ABILITY_GAS_EXTERNAL_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_NEUTRALIZING_GAS_CHANGED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 30), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:gas_probe", .state_size = sizeof(struct ProbeState), .hooks = hooks, .hook_count = 1,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sAbilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 30));
    return sAbilities && sAbilities->header.struct_size >= RHH_ABILITY_SERVICE_V1_30_SIZE ? &definition : NULL;
}
