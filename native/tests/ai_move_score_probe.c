#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhMoveService *sMoves;
static const struct RhhBattleService *sBattle;
static RhhContentKey sScratch, sWaterGun, sGrowl, sCustom, sTackle, sExpanding;
static RhhContentKey sLiquidOoze;
static struct RhhModMove sMove;
static int sScoring, sNested;

static void Nested(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (!sScoring) return;
    struct RhhAiMoveScoreContext value = {.struct_size = sizeof(value), .score = 999}, old = value;
    if (!sMoves->ai_score_context(&value) || memcmp(&value, &old, sizeof(value))) puts("AI_SCORE_BAD_NESTED");
    struct RhhAiMoveEvaluation evaluation = {.struct_size = sizeof(evaluation), .damage_median = 999}, before = evaluation;
    if (!sMoves->ai_evaluation(&evaluation) || memcmp(&evaluation, &before, sizeof(evaluation))) puts("AI_SCORE_BAD_NESTED");
    sNested = 1;
}
static void Score(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhAiMoveScoreContext context = {.struct_size = sizeof(context)};
    struct RhhAiMoveScoreContext small = {.struct_size = sizeof(small) - 1}, old = small;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    struct RhhAiMoveEvaluation evaluation = {.struct_size = sizeof(evaluation)};
    struct RhhAiMoveEvaluation shortEval = {.struct_size = sizeof(shortEval) - 1}, oldEval = shortEval;
    if (!sMoves->ai_score_context(&small) || memcmp(&small, &old, sizeof(small))
        || sMoves->ai_score_context(&context) || sBattle->current(&battle)
        || context.battle_id != battle.battle_id || context.run_id != battle.run_id
        || context.move != event->event_key || context.attacker_species != event->subject
        || context.score != event->args[0] || context.attacker != event->args[1]
        || context.candidate != event->args[2] || context.move_slot != event->args[3]
        || !context.candidate_species || context.attacker != 1 || context.score != 100)
        puts("AI_SCORE_BAD_CONTEXT");
    if (!sMoves->ai_evaluation(&shortEval) || memcmp(&shortEval, &oldEval, sizeof(shortEval))
        || sMoves->ai_evaluation(&evaluation) || evaluation.run_id != context.run_id
        || evaluation.battle_id != context.battle_id || evaluation.move != context.move
        || evaluation.attacker != context.attacker || evaluation.candidate != context.candidate
        || evaluation.move_slot != context.move_slot) puts("AI_SCORE_BAD_EVALUATION");
    if (event->event_key == sScratch && (evaluation.damage_min != 17 || evaluation.damage_max != 22
        || evaluation.damage_random != 20 || evaluation.accuracy != 87
        || evaluation.type_effectiveness != RHH_TYPE_ONE || evaluation.attacker_hp_percent != 42
        || evaluation.candidate_hp_percent != 70)) puts("AI_SCORE_BAD_EVALUATION");
    struct RhhMoveInfo info = {.struct_size = sizeof(info)};
    sScoring = 1; sNested = 0;
    if (sMoves->info(sTackle, &info) || !sNested) puts("AI_SCORE_BAD_NESTED");
    sScoring = 0;
    if (sMoves->ai_score_context(&context)) puts("AI_SCORE_BAD_NESTED");
    if (sMoves->ai_evaluation(&evaluation)) puts("AI_SCORE_BAD_NESTED");
    if (event->event_key == sExpanding && context.move_target != RHH_MOVE_TARGET_BOTH)
        puts("AI_SCORE_BAD_KNOWN_TARGET");
    if (event->event_key == sScratch)
        decision->value = evaluation.candidate_ability == sLiquidOoze || (evaluation.flags & RHH_AI_EVAL_HEAL_BLOCKED)
            ? 70 : evaluation.flags & RHH_AI_EVAL_CANDIDATE_SUBSTITUTE ? 80
            : evaluation.flags & RHH_AI_EVAL_PARTNER_SIMULATION ? 75 : evaluation.damage_median >= 19 ? 201 : 80;
    else if (event->event_key == sWaterGun) decision->value = 256; /* Invalid: retain 100. */
    else if (event->event_key == sGrowl) decision->value = 201;
    else if (event->event_key == sCustom) decision->value = 220;
}
static void Deny(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sGrowl) { decision->allowed = 0; decision->value = 250; }
    else if (event->event_key == sTackle) decision->value = 250; /* Constraints cannot rewrite. */
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_AI_MOVE_SCORE, RHH_TRANSFORM, 0, Score, NULL},
    {sizeof(struct RhhModHook), RHH_AI_MOVE_SCORE, RHH_CONSTRAINT, 0, Deny, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TARGET, RHH_TRANSFORM, 0, Nested, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 20), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition),
    .abi_version = RHH_MOD_ABI_MOVES, .id = "test:ai_score", .hook_count = 3, .hooks = sHooks,
    .service_requirement_count = 2, .service_requirements = sRequirements, .move_count = 1, .moves = &sMove};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 20));
    sBattle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 0));
    sScratch = host->content_key("rhh:move/scratch"); sWaterGun = host->content_key("rhh:move/water_gun");
    sGrowl = host->content_key("rhh:move/growl"); sTackle = host->content_key("rhh:move/tackle");
    sExpanding = host->content_key("rhh:move/expanding_force");
    sLiquidOoze = host->content_key("rhh:ability/liquid_ooze");
    sCustom = host->content_key("test:move/ai_probe");
    sMove = (struct RhhModMove){.struct_size = sizeof(sMove), .power = 40, .accuracy = 100, .pp = 20,
        .category = RHH_MOVE_SPECIAL, .target = RHH_MOVE_TARGET_SELECTED,
        .template_move = sWaterGun, .type = host->content_key("rhh:type/water"),
        .id = "test:move/ai_probe", .name = "AI Probe", .description = "A test move."};
    return sMoves && sBattle && sScratch && sWaterGun && sGrowl && sTackle && sCustom && sExpanding && sLiquidOoze ? &sDefinition : NULL;
}
