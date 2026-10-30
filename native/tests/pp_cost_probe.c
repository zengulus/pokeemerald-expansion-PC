/* A separately compiled consumer validates the scoped PP proposal ABI. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhMoveService *sMoves;
static RhhContentKey sTackle, sMetronome, sSurf;
static void Transform(const struct RhhModEvent *event, const void *data, struct RhhModDecision *decision)
{
    const unsigned mode = *(const unsigned char *)data;
    struct RhhMovePPCostContext context = {.struct_size = sizeof(context)};
    struct RhhMovePPCostContext shortOut = {.struct_size = sizeof(shortOut) - 1, .pp_before = 999};
    struct RhhMovePPCostContext saved = shortOut;
    if (!sMoves || sMoves->pp_cost_context(&context) || !sMoves->pp_cost_context(&shortOut)
        || memcmp(&shortOut, &saved, sizeof(saved)) || context.move != event->event_key
        || context.attacker_species != event->subject || context.base_cost != event->args[0]
        || context.attacker != event->args[1] || context.target != event->args[2]
        || context.move_slot != event->args[3] || !context.charged_move || !context.battle_id
        || decision->value != context.base_cost || !context.pp_before)
        puts("PP_COST_BAD_CONTEXT");
    if ((context.flags & RHH_MOVE_PP_COST_SUBMOVE) && (context.move != sTackle || context.charged_move != sMetronome))
        puts("PP_COST_BAD_SUBMOVE");
    if (context.move == sSurf && (!(context.flags & RHH_MOVE_PP_COST_SPREAD)
        || (!(context.flags & RHH_MOVE_PP_COST_AI) && context.base_cost != 3))) puts("PP_COST_BAD_SPREAD");
    if (context.flags & RHH_MOVE_PP_COST_AI)
    {
        struct RhhAiPPCostEstimate nested = {.struct_size = sizeof(nested), .cost = 99}, old = nested;
        if (!sMoves->ai_pp_cost_estimate(&nested) || memcmp(&nested, &old, sizeof(old))) puts("PP_COST_BAD_RECURSION");
    }
    if (mode == 7 && (context.flags & RHH_MOVE_PP_COST_PERMANENT)) puts("PP_COST_BAD_TEMPORARY");
    if (mode == 1) decision->value = context.base_cost + 1;
    if (mode == 2) decision->value = 0;
    if (mode == 3) decision->value = RHH_MOVE_PP_COST_MAX + 1;
    if (mode == 4) decision->value = RHH_MOVE_PP_COST_MAX;
    if (mode == 5 || mode == 6) decision->value = 3;
}
static void Constraint(const struct RhhModEvent *event, const void *data, struct RhhModDecision *decision)
{
    (void)event;
    if (*(const unsigned char *)data == 5) decision->allowed = 0;
    decision->value = 0; /* Constraints can deny but cannot rewrite a proposal. */
}
static void LaterConstraint(const struct RhhModEvent *event, const void *data, struct RhhModDecision *decision)
{ (void)event; (void)data; decision->allowed = 1; }
static void Deducted(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *data)
{
    (void)decision;
    unsigned mode = *(unsigned char *)data;
    struct RhhMovePPDeduction value = {.struct_size = sizeof(value)};
    struct RhhMovePPDeduction tiny = {.struct_size = 1, .cost = 999}, old = tiny;
    if (sMoves->pp_deduction_context(&value) || !sMoves->pp_deduction_context(&tiny) || memcmp(&tiny, &old, sizeof(tiny))) {
        puts("PP_COST_BAD_DEDUCTION_CONTEXT"); return;
    }
    unsigned expected = mode == 1 ? value.before.base_cost + 1 : mode == 2 ? 0 : mode == 4 ? 255
        : mode == 6 ? 3 : value.before.base_cost;
    unsigned paid = expected < value.before.pp_before ? expected : value.before.pp_before;
    if (value.run_id != 456 || !value.before.battle_id || value.cost != expected || value.paid != paid
        || value.pp_after != value.before.pp_before - paid || value.before.move != event->event_key
        || value.before.attacker_species != event->subject || value.before.attacker != event->args[0]
        || value.before.move_slot != event->args[1] || value.paid != event->args[2] || value.pp_after != event->args[3]
        || value.controller_queued != !!(value.before.flags & RHH_MOVE_PP_COST_PERMANENT)
        || (mode == 7 && value.controller_queued)
        || ((value.before.flags & RHH_MOVE_PP_COST_SUBMOVE)
            && (value.before.move != sTackle || value.before.charged_move != sMetronome))) puts("PP_COST_BAD_DEDUCTION");
    else puts("PP_COST_DEDUCTION_OK");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_PP_COST, RHH_TRANSFORM, 0, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_PP_COST, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_PP_COST, RHH_CONSTRAINT, 1, LaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_AI_MOVE_PP_COST, RHH_TRANSFORM, 0, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_AI_MOVE_PP_COST, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_AI_MOVE_PP_COST, RHH_CONSTRAINT, 1, LaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_PP_DEDUCTED, RHH_OBSERVE, 0, NULL, Deducted}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 44), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:pp_cost", .state_size = 1, .hook_count = 7, .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 44));
    sTackle = host->content_key("rhh:move/tackle"); sMetronome = host->content_key("rhh:move/metronome");
    sSurf = host->content_key("rhh:move/surf");
    return sMoves && sTackle && sMetronome && sSurf ? &sDefinition : NULL;
}
