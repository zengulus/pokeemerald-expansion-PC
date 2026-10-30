#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhOwnedMonService *sOwned;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (event->kind == RHH_PARTY_REORDERED) { puts("OWNED_SKY_PROBE_BAD unexpected reorder"); return; }
    uint32_t first = 99, second = 99, missing = 99;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon), .identity = 99}, before = mon;
    if (!sOwned || sOwned->party_view() != RHH_OWNED_PARTY_SKY_BATTLE
        || sOwned->sky_party_field_slot(0, &first) || first != 1
        || sOwned->sky_party_field_slot(1, &second) || second != 3
        || !sOwned->sky_party_field_slot(2, &missing) || missing != 99
        || !sOwned->party_at(0, &mon) || memcmp(&mon, &before, sizeof(mon)))
        puts("OWNED_SKY_PROBE_BAD projection");
    else puts("OWNED_SKY_PROBE_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_PARTY_REORDERED, RHH_OBSERVE, 0, NULL, Observe},
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 20), RHH_SERVICE_REQUIRED, 0, "rhh:service/owned_mon"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:owned_sky",
    .hook_count = 2, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sOwned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 20));
    return sOwned && sOwned->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_20_SIZE ? &definition : NULL;
}
