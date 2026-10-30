#include "field_effect.h"
#include "constants/field_effects.h"
static const struct RhhGroundEffectService *sGroundTest;
static u32 sGroundMode, sGroundLive, sGroundPreview, sGroundNested;
static void GroundTestTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhGroundEffectResult context = {.struct_size = sizeof(context)};
    CHECK(sGroundTest && !sGroundTest->context(&context) && context.slot == 0 && context.current_available);
    if (context.is_preview) ++sGroundPreview; else ++sGroundLive;
    struct RhhGroundEffectRequest nested = {.struct_size = sizeof(nested), .slot = 0, .phase = context.phase, .base = context.base};
    struct RhhGroundEffectResult unchanged = {.struct_size = sizeof(unchanged), .value = 999}, copy = unchanged;
    CHECK(sGroundTest->evaluate(&nested, &unchanged) && !memcmp(&unchanged, &copy, sizeof(copy))); ++sGroundNested;
    if (sGroundMode == 1) decision->value |= context.phase == RHH_GROUND_BEGIN
        ? RHH_GROUND_SPLASH | RHH_GROUND_TRACKS : RHH_GROUND_SPLASH | RHH_GROUND_RIPPLE;
    if (sGroundMode == 2) decision->value = UINT64_C(1) << 40;
    if (sGroundMode == 5) { (void)Random32(); (void)Random2_32(); decision->value |= RHH_GROUND_RIPPLE; }
}
static void GroundTestConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sGroundMode == 3) decision->allowed = 0;
    if (sGroundMode == 4) decision->value = 0;
}
static const struct RhhModHook sGroundTestHooks[] = {
    {sizeof(struct RhhModHook), RHH_WORLD_GROUND_EFFECT, RHH_TRANSFORM, 0, GroundTestTransform, NULL},
    {sizeof(struct RhhModHook), RHH_WORLD_GROUND_EFFECT, RHH_CONSTRAINT, 0, GroundTestConstraint, NULL}
};
static const struct RhhModDefinition sGroundTestMod = {.struct_size = sizeof(sGroundTestMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:ground_effect", .hooks = sGroundTestHooks, .hook_count = ARRAY_COUNT(sGroundTestHooks)};
static bool8 GroundTestNoMovement(struct ObjectEvent *object, struct Sprite *sprite)
{ (void)object; (void)sprite; return FALSE; }
