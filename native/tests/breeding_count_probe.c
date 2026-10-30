#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhBreedingService *sBreeding;
static void Count(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhBreedingIvCountContext c = {.struct_size = sizeof(c)};
    struct RhhBreedingIvCountContext small = {.struct_size = 1, .base_count = 999}, before = small;
    struct RhhBreedingOffer nested = {.struct_size = sizeof(nested)};
    struct RhhBreedingPreview preview = {.struct_size = sizeof(preview)};
    if (sBreeding->iv_count_context(&c) || !sBreeding->iv_count_context(&small) || memcmp(&small, &before, sizeof(small))
        || c.base_count != event->args[0] || c.base_count != decision->value
        || c.parents[0].mon.species != event->event_key || c.parents[1].mon.species != event->subject
        || c.parents[0].mon.ivs[0] != 31 || c.parents[1].mon.ivs[0] != 10
        || !sBreeding->offer(&nested) || !sBreeding->preview(&preview))
    { puts("BREED_COUNT_BAD"); return; }
    unsigned mode = *(const unsigned char *)state;
    if (mode <= 6) decision->value = mode;
    else if (mode == 7) decision->value = 7;
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    if (*(const unsigned char *)state == 8) decision->allowed = 0;
    decision->value = 0;
}
static void Later(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->allowed = 1; }
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BREED_IV_COUNT, RHH_TRANSFORM, 0, Count, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_IV_COUNT, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_IV_COUNT, RHH_CONSTRAINT, 1, Later, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 7), RHH_SERVICE_REQUIRED, 0, "rhh:service/breeding"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_count", .state_size = 1, .hooks = hooks, .hook_count = 3,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sBreeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 7));
    return sBreeding && sBreeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_7_SIZE ? &definition : NULL;
}
