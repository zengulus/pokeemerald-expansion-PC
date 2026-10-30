/* Independent AI scoring consumer of the PP estimate. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhMoveService *sMoves;
static void Score(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhAiPPCostEstimate result = {.struct_size = sizeof(result)};
    struct RhhAiPPCostEstimate small = {.struct_size = sizeof(small) - 1, .cost = 99}, saved = small;
    if (sMoves->ai_pp_cost_estimate(&result) || !sMoves->ai_pp_cost_estimate(&small)
        || memcmp(&small, &saved, sizeof(saved)) || result.paid > result.cost
        || result.paid > result.pp_before || result.pp_after != result.pp_before - result.paid
        || !!(result.flags & RHH_AI_PP_DEDUCTION_SKIPPED) != (result.base_cost == 0))
    { puts("PP_COST_BAD_AI"); return; }
    decision->value = result.cost + ((result.flags & RHH_AI_PP_UNKNOWN_ABILITY) ? 16 : 0)
        + ((result.flags & RHH_AI_PP_ASSUMED_TARGET) ? 32 : 0)
        + ((result.flags & RHH_AI_PP_CALLED_MOVE) ? 64 : 0)
        + ((result.flags & RHH_AI_PP_COMMIT_RULES_MAY_DIFFER) ? 128 : 0);
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_AI_MOVE_SCORE, RHH_TRANSFORM, 0, Score, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 24), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:pp_cost_ai", .hook_count = 1, .hooks = hooks,
    .service_requirement_count = 1, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 24));
    return sMoves ? &definition : NULL;
}
