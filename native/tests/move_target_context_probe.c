#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhMoveService *sMoves;
static const struct RhhBattleService *sBattle;
static RhhContentKey sWaterGun, sTackle;
static int sResolving, sNested;

static void Nested(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (!sResolving) return;
    struct RhhMoveTargetContext context = {.struct_size = sizeof(context), .battler = 999}, old = context;
    if (!sMoves->target_context(&context) || memcmp(&context, &old, sizeof(context))) puts("TARGET_CONTEXT_BAD_NESTED");
    sNested = 1;
}
static void Resolve(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhMoveTargetContext context = {.struct_size = sizeof(context)};
    struct RhhMoveTargetContext small = {.struct_size = sizeof(small) - 1, .battler = 999}, old = small;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (!sMoves->target_context(&small) || memcmp(&small, &old, sizeof(small))
        || sMoves->target_context(&context) || sBattle->current(&battle)
        || context.move != event->event_key || context.species != event->subject
        || context.battler != event->args[0] || context.phase != event->args[1]
        || context.target != event->args[2] || context.strike_count != event->args[3]
        || context.run_id != battle.run_id || context.battle_id != battle.battle_id
        || context.phase < RHH_MOVE_TARGET_SELECTION || context.phase > RHH_MOVE_TARGET_AI)
        puts("TARGET_CONTEXT_BAD_CONTEXT");
    struct RhhMoveInfo info = {.struct_size = sizeof(info)};
    sResolving = 1; sNested = 0;
    if (sMoves->info(sTackle, &info) || !sNested) puts("TARGET_CONTEXT_BAD_REENTRANCY");
    sResolving = 0;
    if (sMoves->target_context(&context)) puts("TARGET_CONTEXT_BAD_REENTRANCY");
    struct RhhMoveTargetPreview nestedPreview = {.struct_size = sizeof(nestedPreview), .target = 999}, savedPreview = nestedPreview;
    if (!sMoves->target_preview(context.battler, context.move, context.phase, &nestedPreview)
        || memcmp(&nestedPreview, &savedPreview, sizeof(nestedPreview))) puts("TARGET_CONTEXT_BAD_PREVIEW_NESTING");
    struct RhhMoveTargetFilter filter = {.struct_size = sizeof(filter), .allowed = 99}, savedFilter = filter;
    if (!sMoves->target_filter(context.battler, context.move, context.battler, &filter)
        || memcmp(&filter, &savedFilter, sizeof(filter))) puts("TARGET_CONTEXT_BAD_FILTER_NESTING");
    if (event->event_key != sWaterGun) return;
    if (context.phase != RHH_MOVE_TARGET_EXECUTION && !sMoves->target_lock()) puts("TARGET_CONTEXT_BAD_PHASE");
    if (context.phase == RHH_MOVE_TARGET_EXECUTION) (void)sMoves->target_lock();
    switch (((const uint8_t *)state)[0])
    {
    case 0: decision->value = context.battler == 1 ? RHH_MOVE_TARGET_BOTH : RHH_MOVE_TARGET_SELECTED; break;
    case 1: decision->value = RHH_MOVE_TARGET_NONE; break;
    case 2: decision->allowed = 0; decision->value = RHH_MOVE_TARGET_BOTH; break;
    case 3: decision->value = RHH_MOVE_TARGET_BOTH; break;
    case 4: decision->value = RHH_MOVE_TARGET_SMART; break;
    case 5: decision->value = RHH_MOVE_TARGET_DEPENDS; break;
    case 6: decision->value = context.weather == RHH_WEATHER_RAIN ? RHH_MOVE_TARGET_BOTH : RHH_MOVE_TARGET_SELECTED; break;
    case 7: decision->value = context.terrain == RHH_TERRAIN_PSYCHIC ? RHH_MOVE_TARGET_BOTH : RHH_MOVE_TARGET_SELECTED; break;
    }
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    if (event->event_key == sWaterGun && ((const uint8_t *)state)[0] == 3) decision->value = RHH_MOVE_TARGET_USER;
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_TARGET_RESOLVE, RHH_TRANSFORM, 0, Resolve, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_TARGET_RESOLVE, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TARGET, RHH_TRANSFORM, 0, Nested, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 43), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:target_context", .hook_count = 3, .hooks = sHooks, .state_size = 1, .state_schema = 1,
    .service_requirement_count = 2, .service_requirements = sRequirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 43));
    sBattle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 0));
    sWaterGun = host->content_key("rhh:move/water_gun"); sTackle = host->content_key("rhh:move/tackle");
    return sMoves && sMoves->header.struct_size >= RHH_MOVE_SERVICE_V1_43_SIZE && sBattle && sWaterGun && sTackle
        ? &sDefinition : NULL;
}
