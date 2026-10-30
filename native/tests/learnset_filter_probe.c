#include "mod_api.h"
#include <stdio.h>

static const struct RhhMoveService *sMoves;
static RhhContentKey sTorchic, sPoochyena, sScratch, sGrowl, sSurf, sProtect, sCounter, sAgility, sWaterGun, sEmber, sSurge;
static struct RhhModLearnset sEntries[3];

static void Order(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->subject != sTorchic) return;
    if (event->event_key == sSurge && event->args[2] != RHH_LEARNSET_ADDITION) puts("LEARNSET_FILTER_BAD");
    uint32_t count = 0;
    if (event->args[0] < RHH_LEARNSET_LEVEL || event->args[0] > RHH_LEARNSET_EGG
        || event->args[2] > RHH_LEARNSET_ADDITION || event->args[3]
        || sMoves->learnset_count(sTorchic, event->args[0], &count) || !count)
        puts("LEARNSET_FILTER_BAD");
    if (event->args[0] == RHH_LEARNSET_LEVEL && event->event_key == sSurf) decision->value = 0;
    if (event->args[0] == RHH_LEARNSET_LEVEL && event->event_key == sGrowl) decision->value = UINT32_MAX;
    if (event->args[0] == RHH_LEARNSET_LEVEL && event->event_key == sEmber) decision->value = UINT64_MAX;
    if (event->args[0] == RHH_LEARNSET_EGG && event->event_key == sAgility) decision->value = 0;
}
static void Filter(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->subject == sPoochyena && event->args[0] == RHH_LEARNSET_LEVEL)
    { decision->allowed = 0; return; }
    if (event->subject != sTorchic) return;
    if ((event->args[0] == RHH_LEARNSET_LEVEL && event->event_key == sScratch)
        || (event->args[0] == RHH_LEARNSET_TEACHABLE && event->event_key == sProtect)
        || (event->args[0] == RHH_LEARNSET_EGG && (event->event_key == sCounter || event->event_key == sWaterGun
                                                    || event->event_key == sSurge)))
        decision->allowed = 0;
    decision->value = 17; /* Constraints cannot reorder. */
}
static void Later(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->allowed = 1; }
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_LEARNSET_ENTRY, RHH_TRANSFORM, 0, Order, NULL},
    {sizeof(struct RhhModHook), RHH_LEARNSET_ENTRY, RHH_CONSTRAINT, 0, Filter, NULL},
    {sizeof(struct RhhModHook), RHH_LEARNSET_ENTRY, RHH_CONSTRAINT, 1, Later, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_LEARNSETS, .id = "test:learnset_filter",
    .hook_count = 3, .hooks = sHooks, .service_requirement_count = 1, .service_requirements = sRequirements,
    .learnset_count = 3, .learnsets = sEntries
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 2));
    sTorchic = host->content_key("rhh:species/torchic"); sPoochyena = host->content_key("rhh:species/poochyena");
    sScratch = host->content_key("rhh:move/scratch");
    sGrowl = host->content_key("rhh:move/growl"); sSurf = host->content_key("rhh:move/surf");
    sProtect = host->content_key("rhh:move/protect"); sCounter = host->content_key("rhh:move/counter");
    sAgility = host->content_key("rhh:move/agility"); sWaterGun = host->content_key("rhh:move/water_gun");
    sEmber = host->content_key("rhh:move/ember");
    sSurge = host->content_key("test:move/surge");
    sEntries[0] = (struct RhhModLearnset){sizeof(sEntries[0]), RHH_LEARNSET_LEVEL, 1, 0, sTorchic, sSurf};
    sEntries[1] = (struct RhhModLearnset){sizeof(sEntries[1]), RHH_LEARNSET_TEACHABLE, 0, 0, sTorchic, sWaterGun};
    sEntries[2] = (struct RhhModLearnset){sizeof(sEntries[2]), RHH_LEARNSET_EGG, 0, 0, sTorchic, sWaterGun};
    return sMoves && sTorchic && sPoochyena && sScratch && sGrowl && sSurf && sProtect && sCounter && sAgility && sWaterGun && sEmber
        ? &sDefinition : NULL;
}
