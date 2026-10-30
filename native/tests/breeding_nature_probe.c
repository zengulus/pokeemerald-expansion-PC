#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhBreedingService *sBreeding;
static RhhContentKey sAdamant, sInvalid;
static void Transform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhBreedingNatureContext c = {.struct_size = sizeof(c)}, small = {.struct_size = 1, .generation = 999}, before = small;
    struct RhhBreedingOffer nested = {.struct_size = sizeof(nested)};
    struct RhhBreedingPreview preview = {.struct_size = sizeof(preview)};
    if (sBreeding->nature_context(&c) || !sBreeding->nature_context(&small) || memcmp(&small, &before, sizeof(small))
        || c.base_nature != event->event_key || c.base_nature != decision->value || c.generation != event->args[0]
        || !c.parents[0].mon.species || !c.parents[1].mon.species
        || !sBreeding->offer(&nested) || !sBreeding->preview(&preview))
    { puts("BREED_NATURE_BAD"); return; }
    unsigned mode = *(const unsigned char *)state;
    if (mode == 1 || mode == 5) decision->value = sAdamant;
    else if (mode == 2) decision->value = sInvalid;
    else if (mode == 3) decision->value = 0;
    else if (mode == 4 && !c.base_nature) decision->value = sAdamant;
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    if (*(const unsigned char *)state == 5) decision->allowed = 0;
    decision->value = sInvalid; /* Constraint stages cannot rewrite the transform. */
}
static void Later(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->allowed = 1; }
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision;
    if (*(unsigned char *)state != 1) return;
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingNatureOdds odds = {.struct_size = sizeof(odds)};
    if (sBreeding->offer(&offer) || sBreeding->nature_odds(&offer, &odds)
        || odds.weights[3] != odds.denominator) puts("BREED_NATURE_BAD");
    else puts("BREED_NATURE_NESTED_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BREED_NATURE, RHH_TRANSFORM, 0, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_NATURE, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_NATURE, RHH_CONSTRAINT, 1, Later, NULL},
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 10), RHH_SERVICE_REQUIRED, 0, "rhh:service/breeding"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_nature", .state_size = 1, .hooks = hooks, .hook_count = 4,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sBreeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 10));
    sAdamant = host->content_key("rhh:nature/adamant"); sInvalid = host->content_key("test:nature/missing");
    return sBreeding && sBreeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_10_SIZE && sAdamant && sInvalid ? &definition : NULL;
}
