#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhBattleService *sBattle;
static const struct RhhMoveService *sMoves;
static RhhContentKey sTackle;
static uint64_t sBattleId, sSequence;
static int sObserving, sNested;

static void Nested(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (!sObserving) return;
    struct RhhBattleTurnContext context = {.struct_size = sizeof(context), .sequence = 999};
    if (!sBattle->turn_context(&context) || context.sequence != 999) puts("TURN_PROBE_BAD_NESTING");
    sNested = 1;
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleTurnContext context = {.struct_size = sizeof(context)};
    struct RhhBattleTurnContext small = {.struct_size = sizeof(small) - 1}, saved = small;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (!sBattle->turn_context(&small) || memcmp(&small, &saved, sizeof(small))
        || sBattle->turn_context(&context) || sBattle->current(&battle)
        || context.sequence != event->ordinal || context.battle.turn != event->args[0]
        || context.battle.flags != event->args[1] || context.battle.participant_count != event->args[2]
        || context.battle.battle_id != battle.battle_id || context.battle.run_id != battle.run_id
        || context.battle.participant_count != 4) { puts("TURN_PROBE_BAD_CONTEXT"); return; }
    if (context.battle.battle_id != sBattleId) { sBattleId = context.battle.battle_id; sSequence = 0; }
    if (context.sequence != sSequence++) puts("TURN_PROBE_BAD_SEQUENCE");
    struct RhhMoveInfo info = {.struct_size = sizeof(info)};
    sObserving = 1; sNested = 0;
    if (sMoves->info(sTackle, &info) || !sNested) puts("TURN_PROBE_BAD_NESTING");
    sObserving = 0;
    if (sBattle->turn_context(&context)) puts("TURN_PROBE_BAD_NESTING");
    printf("TURN_PROBE_EVENT %u %llu\n", context.battle.turn, (unsigned long long)context.sequence);
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_TURN_ENDED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TARGET, RHH_TRANSFORM, 0, Nested, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:battle_turn", .hook_count = 2, .hooks = sHooks,
    .service_requirement_count = 2, .service_requirements = sRequirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sBattle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 2));
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 0));
    sTackle = host->content_key("rhh:move/tackle");
    return sBattle && sMoves && sTackle ? &sDefinition : NULL;
}
