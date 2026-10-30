#include "mod_api.h"
#include <stdio.h>

static const struct RhhBattleService *sBattle;
static const struct RhhMoveService *sMoves;
static RhhContentKey sCondition;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision;
    uint8_t *seen = state;
    struct RhhMoveResolutionContext context = {.struct_size = sizeof(context)};
    if (*seen || sMoves->resolution_context(&context) || context.flags) return;
    if (sBattle->condition_set(sCondition, RHH_CONDITION_PLAYER_SIDE, 255))
        puts("REPLAY_CONDITION_BAD");
    else { *seen = 1; puts("REPLAY_CONDITION_SET"); }
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_RESOLVED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhModBattleCondition sConditions[] = {
    {sizeof(struct RhhModBattleCondition), RHH_CONDITION_SIDE, 255, 0,
     "test:condition/replay_ward", "Replay Ward"}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 4), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 8), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition),
    .abi_version = RHH_MOD_ABI_CONDITIONS, .id = "test:condition_replay", .state_size = 1,
    .hook_count = 1, .hooks = sHooks, .service_requirement_count = 2, .service_requirements = sRequirements,
    .battle_condition_count = 1, .battle_conditions = sConditions};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sBattle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 4));
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 8));
    sCondition = host->content_key("test:condition/replay_ward");
    return sBattle && sMoves && sCondition ? &sDefinition : NULL;
}
