/* Independent consumer of the sampled time during ecology decisions. */
#include "mod_api.h"
#include <stdio.h>

static const struct RhhWorldTimeService *sTime;
static void Inspect(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhWorldTimeSnapshot now = {.struct_size = sizeof(now)};
    if (sTime->sample(&now) || now.hour != 21 || now.minute != 15 || now.second != 25
        || now.source != RHH_TIME_REAL || now.run_id != 456 || now.step != 789)
    { puts("WORLD_TIME_PROBE_BAD"); return; }
    if (event->kind == RHH_HABITAT_QUERY) puts("WORLD_TIME_HABITAT_OK");
    else if (event->kind == RHH_WILD_WEIGHT) puts("WORLD_TIME_WEIGHT_OK");
    else if (event->kind == RHH_WILD_CANDIDATE) puts("WORLD_TIME_CANDIDATE_OK");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_HABITAT_QUERY, RHH_CONSTRAINT, 100, Inspect, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_WEIGHT, RHH_CONSTRAINT, 100, Inspect, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_CANDIDATE, RHH_CONSTRAINT, 100, Inspect, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/world_time"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:world_time_probe",
    .hook_count = 3, .hooks = sHooks, .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sTime = (const void *)host->get_service("rhh:service/world_time", RHH_SERVICE_VERSION(1, 0));
    return sTime && sTime->header.struct_size >= sizeof(*sTime) ? &sDefinition : NULL;
}
