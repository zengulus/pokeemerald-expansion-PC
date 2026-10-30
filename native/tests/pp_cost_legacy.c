/* An existing v1.23 mod subscribes only to committed deductions. */
#include "mod_api.h"
#include <stdio.h>
static const struct RhhMoveService *moves;
static void Rule(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhMovePPCostContext context = {.struct_size = sizeof(context)};
    if (moves->pp_cost_context(&context) || (context.flags & RHH_MOVE_PP_COST_AI)) puts("PP_COST_BAD_LEGACY_AI");
    ++decision->value;
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_PP_COST, RHH_TRANSFORM, 0, Rule, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 23), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:legacy_pp_cost", .hook_count = 1, .hooks = hooks,
    .service_requirement_count = 1, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    moves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 23));
    return moves ? &definition : NULL;
}
