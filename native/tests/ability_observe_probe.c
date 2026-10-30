#include <stdio.h>
#include "mod_api.h"

static const struct RhhAbilityService *sAbilities;
static RhhContentKey sLevitate, sMultitype;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    unsigned *count = state;
    struct RhhBattlerAbilityObservation context = {.struct_size = sizeof(context)};
    struct RhhBattlerAbility current = {.struct_size = sizeof(current)};
    struct RhhBattlerAbilityObservation small = {.struct_size = 1, .sequence = 99};
    if (sAbilities->ability_observation_context(&context) || !sAbilities->ability_observation_context(&small)
        || small.sequence != 99 || event->ordinal != *count || context.sequence != *count
        || context.participant.battler != event->args[0] || context.participant.owner != event->args[1]
        || context.participant.party_slot != event->args[2] || context.participant.identity != event->subject
        || context.after.stored != event->event_key || context.before.battle_id != context.after.battle_id
        || context.before.active != context.before.active_ignoring_mold_breaker
        || context.after.active != context.after.active_ignoring_mold_breaker
        || sAbilities->battler(context.participant.battler, &current)
        || current.stored != context.after.stored || current.active_ignoring_mold_breaker != context.after.active)
        puts("ABILITY_OBSERVATION_BAD");
    if (context.participant.battler != 0 || *count > 6
        || (*count == 0 && (context.before.active != sLevitate || context.after.active))
        || (*count == 1 && (context.before.stored != sLevitate || context.after.active != sMultitype))
        || (*count == 2 && (context.before.stored != sMultitype || context.after.active != sLevitate))
        || (*count == 3 && (context.before.active != sLevitate || context.after.active))
        || (*count == 4 && (context.before.active || context.after.active != sLevitate))
        || (*count == 5 && (context.before.active != sLevitate || context.after.active))
        || (*count == 6 && (context.before.active || context.after.active != sLevitate
                            || !(context.after.flags & RHH_BATTLER_ABILITY_OVERWRITTEN))))
        puts("ABILITY_OBSERVATION_BAD");
    ++*count;
    puts("ABILITY_OBSERVATION_OK");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLER_ABILITY_OBSERVED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 23), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:ability_observer", .state_size = sizeof(unsigned), .hook_count = 1, .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sAbilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 23));
    sLevitate = host->content_key("rhh:ability/levitate");
    sMultitype = host->content_key("rhh:ability/multitype");
    return sAbilities && sAbilities->header.struct_size >= RHH_ABILITY_SERVICE_V1_23_SIZE
        && sLevitate && sMultitype ? &sDefinition : NULL;
}
