#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhAuthoredEncounterService *service;
static void Inspect(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhAuthoredWildWeightContext context = {.struct_size = sizeof(context)};
    struct RhhAuthoredWildPreview nested = {.struct_size = sizeof(nested), .total = 999}, before = nested;
    if (service->preview_context(&context) || context.slot != event->args[2]
        || context.request.map != event->event_key || context.request.ordinal != event->ordinal
        || context.selected_time < RHH_WILD_TIME_MORNING || context.selected_time > RHH_WILD_TIME_NIGHT
        || context.candidate.species != event->subject || context.habitat.sample.map != event->event_key
        || !service->preview(&context.request, &nested) || memcmp(&nested, &before, sizeof(nested)))
        puts("AUTHORED_WILD_PREVIEW_BAD");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_AUTHORED_WILD_WEIGHT, RHH_CONSTRAINT, 1, Inspect, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 3), RHH_SERVICE_REQUIRED, 0, "rhh:service/authored_encounters"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:authored_wild_consumer", .hooks = hooks, .hook_count = 1,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    service = (const void *)host->get_service("rhh:service/authored_encounters", RHH_SERVICE_VERSION(1, 3));
    return service && service->header.struct_size >= RHH_AUTHORED_ENCOUNTER_SERVICE_V1_3_SIZE ? &definition : NULL;
}
