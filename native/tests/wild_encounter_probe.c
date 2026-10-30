/* Independent ABI module used by wild_encounter.py. */
#include "mod_api.h"
#include <stdio.h>

/* Compile against the exact v1.1 prefix to exercise an older module ABI. */
struct WildEncounterServiceV11 {
    struct RhhServiceHeader header;
    int (*context)(struct RhhWildEncounterContext *out);
    int (*weight_context)(struct RhhWildWeightContext *out);
};
_Static_assert(sizeof(struct WildEncounterServiceV11) == RHH_WILD_ENCOUNTER_SERVICE_V1_1_SIZE, "v1.1 prefix changed");
static const struct WildEncounterServiceV11 *sWild;
static RhhContentKey sPoochyena, sZigzagoon;
#ifdef RHH_TEST_HABITAT_REGISTRY
static const struct RhhHabitatRegistryService *sHabitats;
static RhhContentKey sRainHabitat;
static const struct RhhModHabitat sHabitat = {.struct_size = sizeof(sHabitat),
    .id = "test:habitat/rain", .label = "Rain", .description = "An active rain effect.",
    .weather_mask = 1u << RHH_WEATHER_RAIN};
#endif
static unsigned sFishingTarget, sFishingRod;
static void Weight(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildWeightContext context = {.struct_size = sizeof(context)};
    if (sWild->weight_context(&context) || context.map != event->event_key
        || context.slot != event->args[2] || context.x != 2 || context.y != 1
        || !context.habitat_available || context.habitat.environment.weather.value != RHH_WEATHER_RAIN
        || (context.source != RHH_WILD_OVERWORLD_ACTOR && context.source != RHH_WILD_FISHING_ROD))
    { puts("WILD_PROBE_BAD"); return; }
#ifdef RHH_TEST_HABITAT_REGISTRY
    uint32_t matches = 0;
    struct RhhHabitatSample altered = context.habitat;
    if (sHabitats->match(sRainHabitat, &context.habitat, 0, &matches) || !matches)
    { puts("WILD_PROBE_BAD_HABITAT"); return; }
    struct RhhHabitatEvaluation evaluation = {.struct_size = sizeof(evaluation)};
    if (sHabitats->evaluate(sRainHabitat, &context.habitat, 0, &evaluation)
        || evaluation.status != RHH_HABITAT_EVALUATION_MATCH || evaluation.mismatch || evaluation.unavailable)
    { puts("WILD_PROBE_BAD_HABITAT"); return; }
    altered.environment.weather.value = RHH_WEATHER_SUN;
    if (sHabitats->match(sRainHabitat, &altered, 0, &matches) || matches)
    { puts("WILD_PROBE_BAD_HABITAT"); return; }
    altered.environment_available = 0;
    if (sHabitats->evaluate(sRainHabitat, &altered, 0, &evaluation)
        || evaluation.status != RHH_HABITAT_EVALUATION_INDETERMINATE
        || evaluation.unavailable != RHH_HABITAT_REASON_SAVED_WEATHER)
    { puts("WILD_PROBE_BAD_HABITAT"); return; }
    puts("WILD_PROBE_HABITAT");
#endif
    unsigned target = 3;
    if (context.source == RHH_WILD_FISHING_ROD)
    {
        if (context.rod == RHH_WILD_ROD_OLD) target = 1;
        else if (context.rod == RHH_WILD_ROD_GOOD) target = 4;
        else if (context.rod == RHH_WILD_ROD_SUPER) target = 9;
        else { puts("WILD_PROBE_BAD"); return; }
        sFishingTarget = target; sFishingRod = context.rod;
    }
    decision->value = context.slot == target ? 100 : 0;
    if (context.slot == target)
    {
        if (context.source == RHH_WILD_FISHING_ROD) printf("WILD_PROBE_FISH_WEIGHT_%u\n", context.rod);
        else puts("WILD_PROBE_WEIGHT");
    }
}
static void Candidate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context)};
    if (sWild->context(&context) || context.phase != RHH_WILD_PHASE_PROPOSAL
        || (context.source != RHH_WILD_OVERWORLD_ACTOR && context.source != RHH_WILD_FISHING_ROD)
        || (context.source == RHH_WILD_OVERWORLD_ACTOR && (context.table_species != sPoochyena || context.initial_species != sPoochyena))
        || context.map != event->event_key
        || context.x != 2 || context.y != 1
        || context.slot != (context.source == RHH_WILD_FISHING_ROD ? sFishingTarget : 3u)
        || !context.habitat_available || context.habitat.x != 2 || context.habitat.y != 1
        || context.habitat.environment.weather.value != RHH_WEATHER_RAIN)
    { puts("WILD_PROBE_BAD"); return; }
    decision->value = sZigzagoon;
    puts(context.source == RHH_WILD_FISHING_ROD ? "WILD_PROBE_FISH_PROPOSED" : "WILD_PROBE_PROPOSED");
}
static void Generated(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context)};
    if (sWild->context(&context) || context.phase != RHH_WILD_PHASE_CREATED
        || context.species != sZigzagoon || event->subject != sZigzagoon
        || context.habitat.x != 2 || context.habitat.y != 1)
    { puts("WILD_PROBE_BAD"); return; }
    if (context.source == RHH_WILD_FISHING_ROD) printf("WILD_PROBE_FISH_GENERATED_%u\n", sFishingRod);
    else puts("WILD_PROBE_GENERATED");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_CANDIDATE, RHH_TRANSFORM, 0, Candidate, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_GENERATED, RHH_OBSERVE, 0, NULL, Generated},
    {sizeof(struct RhhModHook), RHH_WILD_WEIGHT, RHH_TRANSFORM, 0, Weight, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_encounter"}
#ifdef RHH_TEST_HABITAT_REGISTRY
    , {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/habitat_registry"}
#endif
};
static const struct RhhModDefinition sDefinition = {
#ifdef RHH_TEST_HABITAT_REGISTRY
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_HABITATS,
    .habitat_count = 1, .habitats = &sHabitat,
#else
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
#endif
    .id = "test:wild_probe", .hook_count = 3, .hooks = sHooks,
    .service_requirement_count = sizeof(sRequirements) / sizeof(sRequirements[0]), .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service || !host->content_key) return NULL;
    sWild = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 1));
#ifdef RHH_TEST_HABITAT_REGISTRY
    sHabitats = (const void *)host->get_service("rhh:service/habitat_registry", RHH_SERVICE_VERSION(1, 2));
    sRainHabitat = host->content_key(sHabitat.id);
    if (!sHabitats || sHabitats->header.struct_size < RHH_HABITAT_REGISTRY_SERVICE_V1_2_SIZE || !sRainHabitat) return NULL;
#endif
    sPoochyena = host->content_key("rhh:species/poochyena");
    sZigzagoon = host->content_key("rhh:species/zigzagoon");
    return sWild && sWild->header.struct_size >= sizeof(*sWild) ? &sDefinition : NULL;
}
