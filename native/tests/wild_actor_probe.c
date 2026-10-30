/* Independent ABI observer for committed wild-actor lifetimes. */
#include "mod_api.h"
#include <stdio.h>

static const struct RhhWildActorService *sActors;
static uint64_t sGeneratedHandle;
static void ActorEvent(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildActorEvent context = {.struct_size = sizeof(context)};
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    if (sActors->context(&context) || context.actor.handle != event->ordinal
        || context.actor.map != event->event_key || context.actor.species != event->subject)
    { puts("WILD_ACTOR_PROBE_BAD"); return; }
    if (event->kind == RHH_WILD_ACTOR_SPAWNED)
    {
        if (sActors->find(context.actor.handle, &actor) || actor.slot != context.actor.slot)
        { puts("WILD_ACTOR_PROBE_BAD"); return; }
        if (context.actor.flags & RHH_WILD_ACTOR_GENERATED)
        {
            if (!(context.actor.flags & RHH_WILD_ACTOR_PRESERVED)) { puts("WILD_ACTOR_PROBE_BAD"); return; }
            sGeneratedHandle = context.actor.handle; puts("WILD_ACTOR_GENERATED_SPAWN");
        }
        else
        {
            if ((context.actor.flags & RHH_WILD_ACTOR_PRESERVED) || context.actor.handle == sGeneratedHandle)
            { puts("WILD_ACTOR_PROBE_BAD"); return; }
            puts("WILD_ACTOR_MANUAL_SPAWN");
        }
    }
    else if (event->kind == RHH_WILD_ACTOR_REMOVED)
    {
        if (!sActors->find(context.actor.handle, &actor)) { puts("WILD_ACTOR_PROBE_BAD"); return; }
        if (context.actor.handle == sGeneratedHandle && context.cause == RHH_WILD_ACTOR_CLEAR)
            puts("WILD_ACTOR_BATTLE_CLEAR");
        else if (!(context.actor.flags & RHH_WILD_ACTOR_GENERATED) && context.cause == RHH_WILD_ACTOR_REMOVE)
            puts("WILD_ACTOR_MANUAL_REMOVE");
        else puts("WILD_ACTOR_PROBE_BAD");
    }
    else puts("WILD_ACTOR_PROBE_BAD");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_SPAWNED, RHH_OBSERVE, 0, NULL, ActorEvent},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_REMOVED, RHH_OBSERVE, 0, NULL, ActorEvent}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_actor"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:wild_actor_probe", .hook_count = 2, .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sActors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 0));
    return sActors && sActors->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_0_SIZE ? &sDefinition : NULL;
}
