#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhBattleGroundingService *grounding;
static struct RhhModMoveEffectBinding binding;
static void Check(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)decision;
    const uint32_t *settings = state;
    struct RhhBattleGrounding view = {.struct_size = sizeof(view)};
    struct RhhBattleGrounding projected = {.struct_size = sizeof(projected)}, none = {.struct_size = sizeof(none)};
    struct RhhBattleGrounding wrong = {.struct_size = sizeof(wrong), .flags = 777}, before = wrong;
    if (grounding->ai(settings[0], &view) || view.battler != settings[0]
        || (view.flags & settings[1]) != settings[2] || view.terrain != settings[3]
        || !view.run_id || !view.battle_id || !grounding->live(settings[0], &wrong)
        || memcmp(&wrong, &before, sizeof(wrong)) || !grounding->ai(99, &wrong)
        || memcmp(&wrong, &before, sizeof(wrong))
        || grounding->preview_ai(settings[0], RHH_TERRAIN_PSYCHIC, &projected)
        || projected.terrain != RHH_TERRAIN_PSYCHIC || projected.engine_terrain == view.engine_terrain
        || projected.flags != (view.flags | RHH_BATTLE_GROUNDING_SCENARIO)
        || grounding->preview_ai(settings[0], RHH_TERRAIN_NONE, &none) || none.terrain != RHH_TERRAIN_NONE
        || none.flags != ((view.flags | RHH_BATTLE_GROUNDING_SCENARIO) & ~RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED)
        || !grounding->preview_ai(settings[0], 999, &wrong) || memcmp(&wrong, &before, sizeof(wrong))
        || !grounding->preview_live(settings[0], RHH_TERRAIN_PSYCHIC, &wrong) || memcmp(&wrong, &before, sizeof(wrong)))
        puts("GROUNDING_BAD");
    else puts("GROUNDING_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_AI_EFFECT_PREVIEW, RHH_CONSTRAINT, 0, Check, NULL},
    {sizeof(struct RhhModHook), RHH_AI_MOVE_SCORE, RHH_TRANSFORM, 0, Check, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle_grounding"}
};
static const struct RhhModMoveEffect effects[] = {
    {sizeof(struct RhhModMoveEffect), RHH_MOVE_EFFECT_CUSTOM_DAMAGE, {0, 0}, "test:effect/grounding", "Grounding"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_DAMAGE_EFFECTS,
    .id = "test:battle_grounding", .state_size = sizeof(uint32_t) * 4, .hook_count = 2, .hooks = hooks,
    .service_requirement_count = 1, .service_requirements = requirements,
    .move_effect_count = 1, .move_effects = effects, .move_effect_binding_count = 1, .move_effect_bindings = &binding};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    grounding = (const void *)host->get_service("rhh:service/battle_grounding", RHH_SERVICE_VERSION(1, 1));
    binding = (struct RhhModMoveEffectBinding){sizeof(binding), 0, host->content_key("rhh:move/scratch"),
        host->content_key("test:effect/grounding")};
    return grounding && grounding->header.struct_size >= RHH_BATTLE_GROUNDING_SERVICE_V1_1_SIZE
        && binding.move && binding.effect ? &definition : NULL;
}
