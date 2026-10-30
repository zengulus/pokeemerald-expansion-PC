#include "mod_api.h"
#include <stdio.h>
#include <string.h>

struct WildServiceV16 {
    struct RhhServiceHeader header;
    int (*context)(struct RhhWildEncounterContext *);
    int (*weight_context)(struct RhhWildWeightContext *);
    int (*cancellation_context)(struct RhhWildEncounterCancellation *);
};
_Static_assert(sizeof(struct WildServiceV16) == RHH_WILD_ENCOUNTER_SERVICE_V1_6_SIZE, "wild service prefix changed");
static const struct RhhWildEncounterService *sWild;
static void Transform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildRateContext context = {.struct_size = sizeof(context)};
    struct RhhWildRateContext small = {.struct_size = sizeof(small) - 1, .base_rate = 999}, old = small;
    if (sWild->rate_context(&context) || !sWild->rate_context(&small) || memcmp(&small, &old, sizeof(small))
        || event->event_key != context.map || event->ordinal != context.step || event->args[0] != context.area
        || event->args[1] != context.source || event->args[2] != context.base_rate || event->args[3] != context.table_time
        || decision->value != context.base_rate
        || (context.table_rate == 7 ? context.habitat_available
            : (!context.habitat_available || !context.habitat.environment_available || context.habitat.map != context.map
               || context.habitat.x != context.x || context.habitat.y != context.y)))
    { puts("WILD_RATE_BAD"); return; }
    if (context.table_rate == 1) decision->value = 0;
    else if (context.table_rate == 2) decision->value = RHH_WILD_RATE_MAX;
    else if (context.table_rate == 3) decision->value = UINT64_MAX;
    else if (context.table_rate == 4) decision->value = 20;
    else if (context.table_rate == 5) decision->value = 600;
    else if (context.table_rate == 6)
    {
        if (context.habitat.environment.weather.value == RHH_WEATHER_RAIN) decision->value *= 2;
    }
    else if (context.table_rate == 7) decision->value = 201;
    else if (context.table_rate == 8)
    {
        if (context.habitat.environment.terrain.value == RHH_TERRAIN_GRASSY) decision->value /= 2;
    }
    else if (context.area == RHH_WILD_LAND && context.source == RHH_WILD_STANDARD
        && context.habitat.environment.weather.value == RHH_WEATHER_RAIN)
    {
        if (context.base_rate != context.table_rate * 8) puts("WILD_RATE_BAD"); /* Stench halves the engine threshold. */
        decision->value = 0; puts("WILD_RATE_ACTUAL");
    }
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhWildRateContext context = {.struct_size = sizeof(context)};
    if (sWild->rate_context(&context)) { puts("WILD_RATE_BAD"); return; }
    if (context.table_rate == 4) decision->allowed = 0;
    decision->value = 77; /* Constraint edits must not alter the rate. */
}
static void Later(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->allowed = 1; }
static void Bite(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    unsigned mode = *(const unsigned char *)state;
    struct RhhFishingBiteContext c = {.struct_size = sizeof(c)};
    struct RhhFishingBiteContext small = {.struct_size = 1, .base_percent = 999}, before = small;
    if (sWild->fishing_bite_context(&c) || !sWild->fishing_bite_context(&small) || memcmp(&small, &before, sizeof(small))
        || c.map != event->event_key || c.step != event->ordinal || c.rod != event->args[0]
        || c.phase != event->args[1] || c.base_percent != event->args[2] || c.sticky_ability != event->args[3]
        || decision->value != c.base_percent
        || (mode == 6 ? c.habitat_available : (!c.habitat_available || !c.habitat.environment_available
            || c.habitat.map != c.map || c.habitat.x != c.x || c.habitat.y != c.y)))
    { puts("WILD_RATE_BAD"); return; }
    switch (mode)
    {
    case 1: decision->value = 0; break;
    case 2: decision->value = 100; break;
    case 3: decision->value = 101; break;
    case 4: decision->value = 90; break;
    case 5: if (c.habitat.environment.weather.value == RHH_WEATHER_RAIN) decision->value = 100; break;
    case 6: decision->value = 50; break;
    }
}
static void BiteConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    if (*(const unsigned char *)state == 4) decision->allowed = 0;
    decision->value = 77;
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_ENCOUNTER_RATE, RHH_TRANSFORM, 0, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_ENCOUNTER_RATE, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_ENCOUNTER_RATE, RHH_CONSTRAINT, 1, Later, NULL},
    {sizeof(struct RhhModHook), RHH_FISHING_BITE_RATE, RHH_TRANSFORM, 0, Bite, NULL},
    {sizeof(struct RhhModHook), RHH_FISHING_BITE_RATE, RHH_CONSTRAINT, 0, BiteConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_FISHING_BITE_RATE, RHH_CONSTRAINT, 1, Later, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 11), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_encounter"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:wild_rate",
    .state_size = 1, .hook_count = 6, .hooks = sHooks, .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    const struct WildServiceV16 *old = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 6));
    sWild = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 11));
    return old && sWild && &old->header == &sWild->header && sWild->header.struct_size >= RHH_WILD_ENCOUNTER_SERVICE_V1_7_SIZE
        ? &sDefinition : NULL;
}
