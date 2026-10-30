#include "mod_api.h"
#include <stdio.h>
#include <string.h>

struct WildServiceV17 {
    struct RhhServiceHeader header;
    int (*context)(struct RhhWildEncounterContext *);
    int (*weight_context)(struct RhhWildWeightContext *);
    int (*cancellation_context)(struct RhhWildEncounterCancellation *);
    int (*rate_context)(struct RhhWildRateContext *);
};
_Static_assert(sizeof(struct WildServiceV17) == RHH_WILD_ENCOUNTER_SERVICE_V1_7_SIZE, "wild service v1.7 prefix changed");
static const struct RhhWildEncounterService *sWild;
static void Transform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildSpawnIntervalContext context = {.struct_size = sizeof(context)};
    struct RhhWildSpawnIntervalContext small = {.struct_size = sizeof(small) - 1, .base_frames = 999}, old = small;
    if (sWild->spawn_interval_context(&context) || !sWild->spawn_interval_context(&small)
        || memcmp(&small, &old, sizeof(small)) || event->event_key != context.map || event->ordinal != context.step
        || event->args[0] != context.area || event->args[1] != context.cause || event->args[2] != context.base_frames
        || event->args[3] != context.flags || decision->value != context.base_frames
        || context.capacity != 4 || context.active_count > context.capacity
        || (context.base_frames == 6 ? context.habitat_available
            : (!context.habitat_available || !context.habitat.environment_available
               || context.habitat.map != context.map || context.habitat.x != context.x || context.habitat.y != context.y))
        || (!context.base_frames && !(context.flags & RHH_WILD_SPAWN_LURE))
        || (context.base_frames == 240 && (context.flags != RHH_WILD_SPAWN_REPLACEMENT
                                           || context.active_count != context.capacity)))
    { puts("WILD_SPAWN_BAD"); return; }
    if (!context.base_frames) printf("WILD_SPAWN_LURE_%u\n", context.cause);
    if (context.base_frames == 240) puts("WILD_SPAWN_REPLACEMENT_OK");
    if (context.base_frames == 1) decision->value = 0;
    else if (context.base_frames == 2) decision->value = RHH_WILD_SPAWN_MAX_FRAMES;
    else if (context.base_frames == 3) decision->value = RHH_WILD_SPAWN_MAX_FRAMES + 1;
    else if (context.base_frames == 4) decision->value = 50;
    else if (context.base_frames == 5) decision->value = 6;
    else if (context.base_frames == 6) decision->value = 11;
    else if (context.habitat.environment.weather.value == RHH_WEATHER_RAIN) decision->value /= 2;
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhWildSpawnIntervalContext context = {.struct_size = sizeof(context)};
    if (sWild->spawn_interval_context(&context)) { puts("WILD_SPAWN_BAD"); return; }
    if (context.base_frames == 4) decision->allowed = 0;
    decision->value = 77;
}
static void Later(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->allowed = 1; }
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_SPAWN_INTERVAL, RHH_TRANSFORM, 0, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_SPAWN_INTERVAL, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_SPAWN_INTERVAL, RHH_CONSTRAINT, 1, Later, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 8), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_encounter"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:wild_spawn_interval", .hook_count = 3, .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    const struct WildServiceV17 *old = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 7));
    sWild = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 8));
    return old && sWild && &old->header == &sWild->header && sWild->header.struct_size >= RHH_WILD_ENCOUNTER_SERVICE_V1_8_SIZE
        ? &sDefinition : NULL;
}
