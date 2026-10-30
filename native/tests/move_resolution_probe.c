#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhMoveService *sMoves;
static const struct RhhBattleService *sBattle;
static RhhContentKey sTackle, sScratch, sSurf, sSolar, sPound, sGrowl, sTidal;
static uint64_t sBattleId, sSequence;
static int sObserving, sSawNested;

static void NestedTarget(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (sObserving)
    {
        struct RhhMoveResolutionContext nested = {.struct_size = sizeof(nested), .attacker = 999};
        if (!sMoves->resolution_context(&nested) || nested.attacker != 999) puts("MOVE_RESOLUTION_BAD_NESTING");
        sSawNested = 1;
    }
}

static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhMoveResolutionContext context = {.struct_size = sizeof(context)};
    struct RhhMoveResolutionContext small = {.struct_size = sizeof(small) - 1}, unchanged = small;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (!sMoves->resolution_context(&small) || memcmp(&small, &unchanged, sizeof(small))
        || sMoves->resolution_context(&context) || sBattle->current(&battle)
        || context.battle_id != battle.battle_id || context.run_id != battle.run_id
        || context.move != event->event_key || context.sequence != event->ordinal
        || context.attacker != event->args[0] || context.last_target != event->args[1]
        || context.flags != event->args[2] || context.turn != event->args[3]
        || context.battler_count != 4) { puts("MOVE_RESOLUTION_BAD_CONTEXT"); return; }
    if (context.battle_id != sBattleId) { sBattleId = context.battle_id; sSequence = 0; }
    if (context.sequence != sSequence++) puts("MOVE_RESOLUTION_BAD_SEQUENCE");
    struct RhhMoveInfo info = {.struct_size = sizeof(info)};
    sObserving = 1; sSawNested = 0;
    if (sMoves->info(context.move, &info) || !sSawNested) puts("MOVE_RESOLUTION_BAD_NESTING");
    sObserving = 0;
    if (sMoves->resolution_context(&context)) puts("MOVE_RESOLUTION_BAD_NESTING");
    if (context.move == sTackle)
    {
        if (context.flags || context.attacker != 0 || context.last_target != 1 || context.chosen_move != sTackle
            || context.battlers[1].flags != RHH_MOVE_RESULT_SUPER_EFFECTIVE || context.battlers[1].last_damage != 12)
            puts("MOVE_RESOLUTION_BAD_TACKLE");
        puts("MOVE_RESOLUTION_SUCCESS");
    }
    else if (context.move == sScratch)
    {
        if (!(context.flags & RHH_MOVE_RESOLUTION_UNABLE) || context.chosen_move != sScratch)
            puts("MOVE_RESOLUTION_BAD_INTERRUPTION");
        puts("MOVE_RESOLUTION_UNABLE");
    }
    else if (context.move == sSurf)
    {
        if (context.flags || context.last_target != 3 || context.battlers[1].flags != RHH_MOVE_RESULT_MISSED
            || context.battlers[2].flags != (RHH_MOVE_RESULT_STAT_CHANGED | RHH_MOVE_RESULT_STAT_PREVENTED
                | RHH_MOVE_RESULT_SUPER_EFFECTIVE | RHH_MOVE_RESULT_NOT_VERY_EFFECTIVE
                | RHH_MOVE_RESULT_FAILED | RHH_MOVE_RESULT_NOT_PRESENT)
            || context.battlers[3].flags != (RHH_MOVE_RESULT_PROTECTED | RHH_MOVE_RESULT_UNAFFECTED)
            || context.battlers[3].last_damage != -8) puts("MOVE_RESOLUTION_BAD_SPREAD");
        puts("MOVE_RESOLUTION_SPREAD");
    }
    else if (context.move == sSolar)
    {
        if (context.flags != (RHH_MOVE_RESOLUTION_CHARGING | RHH_MOVE_RESOLUTION_ATTACKER_REPLACED))
            puts("MOVE_RESOLUTION_BAD_FLAGS");
        puts("MOVE_RESOLUTION_CHARGING");
    }
    else if (context.move == sPound)
    {
        if (context.chosen_move != sTackle) puts("MOVE_RESOLUTION_BAD_CHOSEN");
        puts("MOVE_RESOLUTION_AFTER_EXCLUDED");
    }
    else if (context.move == sGrowl) puts("MOVE_RESOLUTION_NEW_BATTLE");
    else if (context.move == sTidal)
    {
        if (context.chosen_move != sTidal || context.flags) puts("MOVE_RESOLUTION_BAD_REGISTERED");
        puts("MOVE_RESOLUTION_REGISTERED");
    }
    else puts("MOVE_RESOLUTION_BAD_MOVE");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_RESOLVED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TARGET, RHH_TRANSFORM, 0, NestedTarget, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 8), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:move_resolution",
    .hook_count = 2, .hooks = sHooks, .service_requirement_count = 2, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 8));
    sBattle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 0));
    sTackle = host->content_key("rhh:move/tackle"); sScratch = host->content_key("rhh:move/scratch");
    sSurf = host->content_key("rhh:move/surf"); sSolar = host->content_key("rhh:move/solarbeam");
    sPound = host->content_key("rhh:move/pound"); sGrowl = host->content_key("rhh:move/growl");
    sTidal = host->content_key("test:move/tidal_pulse");
    return sMoves && sBattle ? &sDefinition : NULL;
}
