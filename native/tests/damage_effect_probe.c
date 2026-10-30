#include "mod_api.h"
#include <string.h>

static const struct RhhMoveService *sMoves;
static RhhContentKey sEffect, sScratch;
static struct RhhModMove sMove;
static struct RhhModMoveEffectBinding sBindings[4];

static void Apply(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    if (event->event_key != sEffect) return;
    uint32_t *counts = state;
    struct RhhMoveEffectApplyContext context = {.struct_size = sizeof(context)};
    if (sMoves->effect_context(&context) || context.move != event->subject
        || context.attacker != event->args[0] || context.target != event->args[1])
    { counts[3]++; return; }
    struct RhhMoveDamageEffectContext damage = {.struct_size = sizeof(damage)};
    if (sMoves->effect_damage_context(&damage) || damage.move != context.move || damage.sequence != context.sequence
        || damage.attacker != context.attacker || damage.target != context.target)
    { counts[8]++; return; }
    struct RhhMoveDamageEffectContext small = {.struct_size = sizeof(small) - 1, .hp_lost = 999}, before = small;
    if (!sMoves->effect_damage_context(&small) || memcmp(&small, &before, sizeof(small))) counts[8]++;
    counts[5] = damage.flags; counts[6] = damage.hp_lost; counts[7] = damage.substitute_lost;
    counts[0]++; counts[1] |= 1u << context.target;
    if (context.flags & RHH_MOVE_EFFECT_FIRST_TARGET) counts[2]++;
    if (counts[9])
    {
        struct RhhMoveEffectQueue queue = {.struct_size = sizeof(queue)};
        if (sMoves->effect_queue(&queue)) { counts[8]++; return; }
        if (counts[9] >= 3)
        {
            if (!sMoves->effect_drain_user(0, 2, 0) || !sMoves->effect_drain_user(2, 1, 0)
                || !sMoves->effect_drain_user(1, 17, 0) || !sMoves->effect_drain_user(1, 2, 2)) counts[8]++;
            if (counts[9] == 5)
            {
                for (int i = 0; i < 8; ++i)
                    if (sMoves->effect_drain_user(1, 16, 0)) counts[8]++;
                if (!sMoves->effect_drain_user(1, 16, 0)) counts[8]++;
                return;
            }
            int queued = sMoves->effect_drain_user(1, 2,
                counts[9] == 4 ? RHH_EFFECT_DRAIN_INCLUDE_SUBSTITUTE : 0);
            if ((damage.flags & RHH_MOVE_DAMAGE_RECORDED) ? queued : !queued) counts[8]++;
            if (!queued) {
                uint32_t flags = counts[9] == 4 ? RHH_EFFECT_DRAIN_INCLUDE_SUBSTITUTE : 0;
                struct RhhMoveQueuedAction view = {.struct_size = sizeof(view)};
                if (sMoves->effect_queue_at(queue.action_count, &view) || view.kind != RHH_MOVE_QUEUED_DRAIN
                    || view.recipient != context.attacker || view.first != 1 || view.second != 2 || view.flags != flags
                    || view.amount != damage.hp_lost + (flags ? damage.substitute_lost : 0)) counts[8]++;
            }
            return;
        }
        if (!sMoves->effect_heal_user_from_damage(0, 2, 0)
            || !sMoves->effect_heal_user_from_damage(2, 1, 0)
            || !sMoves->effect_heal_user_from_damage(1, 17, 0)
            || !sMoves->effect_heal_user_from_damage(1, 2, 2)) counts[8]++;
        int queued = sMoves->effect_heal_user_from_damage(1, 2,
            counts[9] == 2 ? RHH_EFFECT_HEAL_INCLUDE_SUBSTITUTE : 0);
        if ((damage.flags & RHH_MOVE_DAMAGE_RECORDED) ? queued : !queued) counts[8]++;
        if (!queued) {
            uint32_t flags = counts[9] == 2 ? RHH_EFFECT_HEAL_INCLUDE_SUBSTITUTE : 0;
            uint32_t basis = damage.hp_lost + (flags ? damage.substitute_lost : 0);
            uint32_t expected = basis ? (basis / 2 ? basis / 2 : 1) : 0;
            struct RhhMoveQueuedAction view = {.struct_size = sizeof(view)};
            if (sMoves->effect_queue_at(queue.action_count, &view) || view.kind != RHH_MOVE_QUEUED_HEAL_FROM_HIT
                || view.recipient != context.attacker || view.first != 1 || view.second != 2
                || view.flags != flags || view.amount != expected) counts[8]++;
        }
        return;
    }
    sMoves->effect_stat(RHH_STAT_DEFENSE, -1);
    if (context.flags & RHH_MOVE_EFFECT_FIRST_TARGET) sMoves->effect_heal_user(1, 4);
}
static void Mode(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sScratch) decision->value = RHH_MOVE_EFFECT_CUSTOM_DAMAGE;
}
static void MoveApply(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct RhhMoveEffectApplyContext context = {.struct_size = sizeof(context)};
    uint32_t *counts = state;
    if (sMoves->effect_context(&context) || context.move != event->event_key) counts[3]++;
    else counts[4]++;
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_REGISTERED_MOVE_EFFECT, RHH_OBSERVE, 0, NULL, Apply},
    {sizeof(struct RhhModHook), RHH_MOVE_DAMAGE_EFFECT_MODE, RHH_TRANSFORM, 0, Mode, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_DAMAGE_EFFECT_APPLY, RHH_OBSERVE, 0, NULL, MoveApply}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 50), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModMoveEffect sEffects[] = {
    {sizeof(struct RhhModMoveEffect), RHH_MOVE_EFFECT_CUSTOM_DAMAGE, {0, 0}, "test:effect/impact", "Impact"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition),
    .abi_version = RHH_MOD_ABI_DAMAGE_EFFECTS, .id = "test:damage_effect", .state_size = 10 * sizeof(uint32_t),
    .hook_count = 3, .hooks = sHooks, .service_requirement_count = 1, .service_requirements = sRequirements,
    .move_count = 1, .moves = &sMove, .move_effect_count = 1, .move_effects = sEffects,
    .move_effect_binding_count = 4, .move_effect_bindings = sBindings};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 50));
    sEffect = host->content_key("test:effect/impact");
    sScratch = host->content_key("rhh:move/scratch");
    RhhContentKey move = host->content_key("test:move/impact");
    sMove = (struct RhhModMove){.struct_size = sizeof(sMove), .power = 60, .accuracy = 100, .pp = 10,
        .category = RHH_MOVE_PHYSICAL, .target = RHH_MOVE_TARGET_SELECTED,
        .template_move = host->content_key("rhh:move/tackle"), .type = host->content_key("rhh:type/normal"),
        .id = "test:move/impact", .name = "Impact", .description = "A test attack."};
    sBindings[0] = (struct RhhModMoveEffectBinding){sizeof(sBindings[0]), 0, move, sEffect};
    sBindings[1] = (struct RhhModMoveEffectBinding){sizeof(sBindings[1]), 0,
#ifdef DAMAGE_BAD_BINDING
        host->content_key("rhh:move/growl"), sEffect};
#else
        host->content_key("rhh:move/tackle"), sEffect};
#endif
    sBindings[2] = (struct RhhModMoveEffectBinding){sizeof(sBindings[2]), 0, host->content_key("rhh:move/surf"), sEffect};
    sBindings[3] = (struct RhhModMoveEffectBinding){sizeof(sBindings[3]), 0, host->content_key("rhh:move/flamethrower"), sEffect};
    return sMoves && sEffect && move ? &sDefinition : NULL;
}
