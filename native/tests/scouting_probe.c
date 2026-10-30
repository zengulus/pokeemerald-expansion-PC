#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhScoutingService *sScout;
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhScoutingResult context = {.struct_size = sizeof(context)};
    if (sScout->context(&context) || context.action != event->args[0] || !context.changed
        || context.after.individual.serial == 0) { puts("SCOUT_PROBE_BAD"); return; }
    if (context.action == RHH_SCOUT_IDENTIFY && context.before.observations < 2)
    { decision->allowed = 0; strcpy(decision->reason, "observe twice"); puts("SCOUT_PROBE_DENIED"); }
}
static void Committed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state; (void)decision;
    struct RhhScoutingResult context = {.struct_size = sizeof(context)}, nested = {.struct_size = sizeof(nested)};
    struct RhhScoutingRecord record = {.struct_size = sizeof(record)};
    if (sScout->context(&context) || context.is_preview || !context.changed || context.action != event->args[0]
        || sScout->find(&context.after.individual, &record) || record.flags != context.after.flags
        || record.observations != context.after.observations || record.species != context.after.species)
    { puts("SCOUT_PROBE_BAD"); return; }
    struct RhhScoutingRequest request = {.struct_size = sizeof(request), .action = RHH_SCOUT_OBSERVE,
        .handle = context.sight.target_wild.handle, .max_distance = 4};
    if (sScout->apply(&request, &nested) != RHH_SCOUT_UNAVAILABLE) { puts("SCOUT_PROBE_BAD"); return; }
    puts("SCOUT_PROBE_COMMITTED");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_SCOUT, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_SCOUT_RECORDED, RHH_OBSERVE, 0, NULL, Committed}
};
static const struct RhhServiceRequirement sReq[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/scouting"}
};
static const struct RhhModDefinition sDef = {.struct_size = sizeof(sDef), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:scouting_probe", .hook_count = 2, .hooks = sHooks, .service_requirement_count = 1, .service_requirements = sReq};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sScout = (const void *)host->get_service("rhh:service/scouting", RHH_SERVICE_VERSION(1, 0));
    return sScout && sScout->header.struct_size >= sizeof(*sScout) ? &sDef : NULL;
}
