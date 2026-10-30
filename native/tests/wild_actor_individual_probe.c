/* Independently built v1.4 consumer for saved and newly identified actors. */
#include "mod_api.h"
#include <stdio.h>

static const struct RhhWildActorService *sActors;
static uint64_t sRestoredSerial;

static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildActorEvent context = {.struct_size = sizeof(context)};
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id)};
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    if (event->kind == RHH_WILD_ACTOR_BATTLE_ENDED)
    {
        if (!sRestoredSerial || sActors->event_individual(&id) || id.serial != sRestoredSerial
            || !sActors->find_individual(&id, &actor))
        { puts("WILD_INDIVIDUAL_BAD"); return; }
        puts("WILD_INDIVIDUAL_OUTCOME");
        return;
    }
    if (sActors->context(&context) || context.actor.handle != event->ordinal)
    { puts("WILD_INDIVIDUAL_BAD"); return; }
    if (event->kind == RHH_WILD_ACTOR_IDENTIFIED || context.cause == RHH_WILD_ACTOR_RESTORE)
    {
        if (sActors->event_individual(&id) || !id.run_id || !id.serial
            || sActors->find_individual(&id, &actor) || actor.handle != event->ordinal)
        { puts("WILD_INDIVIDUAL_BAD"); return; }
        if (context.cause == RHH_WILD_ACTOR_RESTORE)
        {
            if (context.actor.flags & RHH_WILD_ACTOR_PRESERVED) sRestoredSerial = id.serial;
            puts("WILD_INDIVIDUAL_RESTORED");
        }
        else puts("WILD_INDIVIDUAL_ASSIGNED");
    }
    else if (event->kind == RHH_WILD_ACTOR_ENGAGED && sRestoredSerial)
    {
        if (sActors->event_individual(&id) || id.serial != sRestoredSerial)
        { puts("WILD_INDIVIDUAL_BAD"); return; }
        puts("WILD_INDIVIDUAL_ENGAGED");
    }
    else if (event->kind == RHH_WILD_ACTOR_REMOVED && sRestoredSerial)
    {
        if (sActors->event_individual(&id) || id.serial != sRestoredSerial
            || !sActors->find_individual(&id, &actor))
        { puts("WILD_INDIVIDUAL_BAD"); return; }
        puts("WILD_INDIVIDUAL_REMOVED");
    }
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_SPAWNED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_IDENTIFIED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_ENGAGED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_REMOVED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_BATTLE_ENDED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 4), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_actor"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:wild_individual", .hook_count = 5, .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sActors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    return sActors && sActors->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_4_SIZE ? &sDefinition : NULL;
}
