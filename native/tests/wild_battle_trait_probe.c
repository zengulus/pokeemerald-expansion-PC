#include "mod_api.h"
#include <stdio.h>
#include <string.h>

struct BattleTraitProbeState { uint32_t engaged, started, ended; uint64_t handle; };
static const struct RhhWildTraitsService *sTraits;
static const struct RhhWildActorService *sActors;
static RhhContentKey sKnown, sImpure, sDenied;
static int CheckTraits(uint64_t handle)
{
    struct RhhWildTraitResult known = {.struct_size = sizeof(known)}, impure = {.struct_size = sizeof(impure)},
        denied = {.struct_size = sizeof(denied)}, tiny = {.struct_size = 1, .value = 99}, saved = tiny;
    return sTraits->battle(sKnown, &known) || sTraits->battle(sImpure, &impure) || sTraits->battle(sDenied, &denied)
        || known.actor.handle != handle || !known.permitted || known.base != 1 || known.value != 0
        || impure.actor.handle != handle || !impure.permitted || impure.base != 0 || impure.value != 0
        || denied.actor.handle != handle || denied.permitted || denied.value != 0
        || !sTraits->battle(sKnown, &tiny) || memcmp(&tiny, &saved, sizeof(tiny));
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct BattleTraitProbeState *probe = state;
    if (event->kind == RHH_WILD_ACTOR_ENGAGED) {
        struct RhhWildActorEvent context = {.struct_size = sizeof(context)};
        if (sActors->context(&context) || sTraits->request_battle(sKnown) || sTraits->request_battle(sImpure)
            || sTraits->request_battle(sDenied) || sTraits->request_battle(sKnown) || !sTraits->request_battle(0))
        { puts("WILD_BATTLE_TRAITS_BAD"); return; }
        probe->handle = context.actor.handle; ++probe->engaged;
    } else if (CheckTraits(probe->handle)) puts("WILD_BATTLE_TRAITS_BAD");
    else if (event->kind == RHH_BATTLE_STARTED) ++probe->started;
    else if (event->kind == RHH_WILD_ACTOR_BATTLE_ENDED) ++probe->ended;
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_ENGAGED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_BATTLE_ENDED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_traits"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 18), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_actor"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:battle_trait_probe",
    .state_size = sizeof(struct BattleTraitProbeState), .hooks = hooks, .hook_count = 3,
    .service_requirements = requirements, .service_requirement_count = 2
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sTraits = (const void *)host->get_service("rhh:service/wild_traits", RHH_SERVICE_VERSION(1, 1));
    sActors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 18));
    sKnown = host->content_key("test:trait/known"); sImpure = host->content_key("test:trait/impure");
    sDenied = host->content_key("test:trait/denied");
    return sTraits && sActors && sTraits->header.struct_size >= RHH_WILD_TRAITS_SERVICE_V1_1_SIZE ? &definition : NULL;
}
