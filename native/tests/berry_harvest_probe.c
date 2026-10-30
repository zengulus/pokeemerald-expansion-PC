#include "mod_api.h"
#include <stdio.h>

static const struct RhhBerryPlotService *berries;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBerryHarvest harvest = {.struct_size = sizeof(harvest)};
    if (berries->harvest_context(&harvest) || event->event_key != harvest.plot.plot.map
        || event->subject != harvest.berry_item || event->args[1] != harvest.berry_count
        || event->args[2] != harvest.mutation_count || harvest.plot.plot.stage != RHH_BERRY_FRUIT)
        puts("BERRY_HARVEST_EXTERNAL_BAD");
    else puts("BERRY_HARVEST_EXTERNAL_OK");
}
static void Yield(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhBerryYield value = {.struct_size = sizeof(value)};
    if (berries->yield_context(&value) || value.base != event->args[1]
        || value.plot.plot.map != event->event_key || value.plot.plot.berry_item != event->subject)
        puts("BERRY_YIELD_EXTERNAL_BAD");
    else puts("BERRY_YIELD_EXTERNAL_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BERRY_PLOT_HARVESTED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_BERRY_HARVEST_YIELD, RHH_TRANSFORM, 0, Yield, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 5), RHH_SERVICE_REQUIRED, 0, "rhh:service/berry_plot"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:berry_harvest", .hook_count = 2, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    berries = (const void *)host->get_service("rhh:service/berry_plot", RHH_SERVICE_VERSION(1, 5));
    return berries && berries->header.struct_size >= RHH_BERRY_PLOT_SERVICE_V1_5_SIZE ? &definition : NULL;
}
