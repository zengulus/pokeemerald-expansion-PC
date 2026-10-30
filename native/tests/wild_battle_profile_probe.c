#include "mod_api.h"
#include <stdio.h>
#include <string.h>

struct BattleProfileProbeState { uint32_t engaged, started, ended, identified; };
static const struct RhhWildProfileService *sProfiles;
static const struct RhhWildActorService *sActors;
static RhhContentKey sOwner, sField, sMissing, sOtherOwner;

static int CheckProfile(int identified)
{
    struct RhhWildProfileValue value = {.struct_size = sizeof(value), .value = 123};
    struct RhhWildProfileValue unchanged = value;
    if (!identified)
        return !sProfiles->battle(sOwner, sField, &value) || memcmp(&value, &unchanged, sizeof(value));
    if (sProfiles->battle(sOwner, sField, &value) || !value.present || value.value != -17
        || value.owner != sOwner || value.field != sField || !value.owner_available || value.revision != 1)
        return 1;
    if (sProfiles->battle(sOwner, sMissing, &value) || value.present || value.value || value.revision != 1)
        return 1;
    if (sProfiles->battle(sOtherOwner, sField, &value) || value.present || value.owner_available || value.revision != 1)
        return 1;
    value = unchanged; value.struct_size = 1; unchanged = value;
    return !sProfiles->battle(sOwner, sField, &value) || memcmp(&value, &unchanged, sizeof(value));
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct BattleProfileProbeState *probe = state;
    if (event->kind == RHH_WILD_ACTOR_ENGAGED) {
        struct RhhWildActorEvent actor = {.struct_size = sizeof(actor)};
        struct RhhWildActorIndividual individual = {.struct_size = sizeof(individual)};
        if (sActors->context(&actor)) { puts("WILD_BATTLE_PROFILE_BAD"); return; }
        probe->identified = !sActors->individual(actor.actor.handle, &individual);
        ++probe->engaged;
    } else if (CheckProfile(probe->identified)) puts("WILD_BATTLE_PROFILE_BAD");
    else if (event->kind == RHH_BATTLE_STARTED) ++probe->started;
    else if (event->kind == RHH_WILD_ACTOR_BATTLE_ENDED) ++probe->ended;
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_ENGAGED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_BATTLE_ENDED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 4), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_profile"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 16), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_actor"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:battle_profile_probe",
    .state_size = sizeof(struct BattleProfileProbeState), .hooks = hooks, .hook_count = 3,
    .service_requirements = requirements, .service_requirement_count = 2
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sProfiles = (const void *)host->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 4));
    sActors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 16));
    sOwner = host->content_key("test:battle_profile_probe"); sField = host->content_key("test:profile/value");
    sMissing = host->content_key("test:profile/missing"); sOtherOwner = host->content_key("test:absent_owner");
    return sProfiles && sActors && sProfiles->header.struct_size >= RHH_WILD_PROFILE_SERVICE_V1_4_SIZE ? &definition : NULL;
}
