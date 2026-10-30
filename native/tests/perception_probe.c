/* Independent native module consuming the perception and wild actor APIs. */
#include "mod_api.h"
#include <stdio.h>

static const struct RhhPerceptionService *sSight;
static const struct RhhWildActorService *sActors;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhPerceptionResult result = {.struct_size = sizeof(result)};
    if (sActors->at(1, &actor)) { puts("PERCEPTION_PROBE_BAD"); return; }
    struct RhhPerceptionRequest request = {.struct_size = sizeof(request), .max_distance = 4,
        .checks = RHH_PERCEPTION_CHECK_SCENE,
        .viewer = {RHH_PERCEPTION_PLAYER, 0, 0}, .target = {RHH_PERCEPTION_WILD, 0, actor.handle}};
    if (sSight->query(&request, &result) || result.target_wild.handle != actor.handle
        || result.target_conceal_mode != RHH_WILD_CONCEAL_GROUND
        || result.blocker_flags != RHH_PERCEPTION_BLOCK_SCENE || result.blocker_x != 1 || result.blocker_y != 0)
        puts("PERCEPTION_PROBE_BAD");
    else puts("PERCEPTION_PROBE_OK");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/perception"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_actor"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:perception_probe",
    .hook_count = 1, .hooks = sHooks, .service_requirement_count = 2, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sSight = (const void *)host->get_service("rhh:service/perception", RHH_SERVICE_VERSION(1, 0));
    sActors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 0));
    return sSight && sSight->header.struct_size >= sizeof(*sSight) && sActors ? &sDefinition : NULL;
}
