/* Independent public-ABI module exercised by gameplay-tests. */
#include "mod_api.h"
#include <stdio.h>

static const struct RhhBreedingService *sBreeding;
static const struct RhhEggHatchService *sHatch;
static RhhContentKey sTorchic, sCharmander, sEevee, sShadowBall;
static int Parents(const struct RhhBreedingContext *context)
{
    return context->parents[0].mon.species == sTorchic
        && context->parents[1].mon.species == sCharmander;
}
static void Propose(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhBreedingContext context = {.struct_size = sizeof(context)};
    if (sBreeding->context(&context) || !Parents(&context)) return;
    if (context.preview && event->kind == RHH_BREED_COMPATIBILITY) puts("BREEDING_PROBE_PREVIEW_COMPATIBILITY");
    if (context.preview && event->kind == RHH_BREED_OFFSPRING) puts("BREEDING_PROBE_PREVIEW_OFFSPRING");
    if (event->kind == RHH_BREED_COMPATIBILITY && context.base_compatibility == RHH_DAYCARE_INCOMPATIBLE)
        decision->value = RHH_DAYCARE_MEDIUM;
    else if (event->kind == RHH_BREED_OFFSPRING && context.parent_slots[0] != context.parent_slots[1])
        decision->value = sEevee;
    else if (event->kind == RHH_BREED_INHERIT_IV && event->ordinal == RHH_STAT_HP)
        decision->value = RHH_BREED_IV_PARENT_0;
    else if (event->kind == RHH_BREED_INHERIT_MOVE && event->ordinal == 3)
        decision->value = sShadowBall;
}
static void Created(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBreedingContext context = {.struct_size = sizeof(context)};
    if (!sBreeding->context(&context) && Parents(&context) && context.egg_valid
        && context.offspring == sEevee && context.egg_identity == event->subject)
    {
        puts("BREEDING_PROBE_COMMITTED");
        for (unsigned i = 0; i < 4; ++i)
            if (context.offspring_moves[i] == sShadowBall && context.offspring_mon.ivs[RHH_STAT_HP] == 31)
                puts("BREEDING_PROBE_INHERITANCE");
    }
}
static void Hatched(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhEggHatchContext context = {.struct_size = sizeof(context)};
    if (!sHatch->context(&context) && context.egg.is_egg && !context.hatched.is_egg
        && context.egg.species == sEevee && context.egg.identity != context.hatched.identity
        && event->subject == context.egg.identity && event->ordinal == context.hatched.identity)
        puts("BREEDING_PROBE_HATCHED");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_BREED_COMPATIBILITY, RHH_TRANSFORM, 0, Propose, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_OFFSPRING, RHH_TRANSFORM, 0, Propose, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_INHERIT_IV, RHH_TRANSFORM, 0, Propose, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_INHERIT_MOVE, RHH_TRANSFORM, 0, Propose, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_EGG_CREATED, RHH_OBSERVE, 0, NULL, Created},
    {sizeof(struct RhhModHook), RHH_EGG_HATCHED, RHH_OBSERVE, 0, NULL, Hatched}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/breeding"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/egg_hatch"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_probe", .hook_count = 6, .hooks = sHooks,
    .service_requirement_count = 2, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service || !host->content_key) return NULL;
    sBreeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 2));
    sHatch = (const void *)host->get_service("rhh:service/egg_hatch", RHH_SERVICE_VERSION(1, 0));
    sTorchic = host->content_key("rhh:species/torchic");
    sCharmander = host->content_key("rhh:species/charmander");
    sEevee = host->content_key("rhh:species/eevee");
    sShadowBall = host->content_key("rhh:move/shadow_ball");
    return sBreeding && sBreeding->header.struct_size >= sizeof(*sBreeding)
        && sHatch && sHatch->header.struct_size >= sizeof(*sHatch) ? &sDefinition : NULL;
}
