#include "mod_api.h"
#include <stdio.h>

static const struct RhhBreedingService *sBreeding;
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingIvJointRequest request = {.struct_size = sizeof(request)};
    struct RhhBreedingIvJointOdds odds = {.struct_size = sizeof(odds)};
    struct RhhBreedingNatureOdds nature = {.struct_size = sizeof(nature)};
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i) request.values[i] = 1u << 31;
    if (sBreeding->offer(&offer) || sBreeding->iv_joint_odds(&offer, &request, &odds)
        || odds.flags || odds.inherited_count != 3 || odds.numerator * 262144 != odds.denominator
        || sBreeding->nature_odds(&offer, &nature) || nature.denominator != 100
        || nature.source_denominator != 4 || nature.source_weights[0] || nature.source_weights[1]
        || nature.source_weights[2] != 4 || nature.weights[0] != 4)
        puts("BREED_JOINT_BAD");
    else puts("BREED_JOINT_EXTERNAL_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 9), RHH_SERVICE_REQUIRED, 0, "rhh:service/breeding"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_joint", .hooks = hooks, .hook_count = 1,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sBreeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 9));
    return sBreeding && sBreeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_9_SIZE ? &definition : NULL;
}
