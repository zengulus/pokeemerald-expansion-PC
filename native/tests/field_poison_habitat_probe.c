#include "mod_api.h"
#include <stdio.h>
static const struct RhhFieldHealingService *service;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhFieldPoisonContext poison = {.struct_size = sizeof(poison)};
    struct RhhFieldPoisonHabitatContext habitat = {.struct_size = sizeof(habitat)};
    if (service->poison_context(&poison) || service->poison_habitat_context(&habitat)
        || poison.applied != event->args[1] || habitat.available > 1
        || (habitat.available && (habitat.sample.struct_size != sizeof(habitat.sample) || !habitat.sample.map)))
        puts("FIELD_POISON_HABITAT_EXTERNAL_BAD");
    else puts("FIELD_POISON_HABITAT_EXTERNAL_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_FIELD_POISON_DAMAGED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 5), RHH_SERVICE_REQUIRED, 0, "rhh:service/field_healing"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:field_habitat_probe", .hooks = hooks, .hook_count = 1,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    service = (const void *)host->get_service("rhh:service/field_healing", RHH_SERVICE_VERSION(1, 5));
    return service && service->header.struct_size >= RHH_FIELD_HEALING_SERVICE_V1_5_SIZE ? &definition : NULL;
}
