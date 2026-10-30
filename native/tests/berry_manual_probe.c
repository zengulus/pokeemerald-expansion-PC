#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhBerryPlotService *berries;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBerryManualWater value = {.struct_size = sizeof(value)};
    if (berries->manual_water_context(&value) || value.before.tree_id != value.after.tree_id
        || event->event_key != value.before.map || event->args[2] != value.changed
        || (!!memcmp(value.before.state_token, value.after.state_token, sizeof(value.before.state_token))) != !!value.changed)
    { puts("BERRY_MANUAL_EXTERNAL_BAD"); return; }
    puts(value.changed ? "BERRY_MANUAL_EXTERNAL_CHANGED" : "BERRY_MANUAL_EXTERNAL_REPEAT");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BERRY_PLOT_MANUALLY_WATERED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/berry_plot"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:berry_manual", .hook_count = 1, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    berries = (const void *)host->get_service("rhh:service/berry_plot", RHH_SERVICE_VERSION(1, 2));
    return berries && berries->header.struct_size >= RHH_BERRY_PLOT_SERVICE_V1_2_SIZE ? &definition : NULL;
}
