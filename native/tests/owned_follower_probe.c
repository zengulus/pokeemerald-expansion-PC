#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhOwnedMonService *sOwned;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (event->kind == RHH_PARTY_REORDERED) { puts("OWNED_FOLLOWER_PROBE_BAD unexpected reorder"); return; }
    if (event->kind == RHH_OWNED_CATCH_PLACED)
    {
        static unsigned count;
        static const uint32_t causes[] = {RHH_CATCH_ACQUIRED, RHH_CATCH_DISPLACED_PARTY, RHH_CATCH_ACQUIRED,
            RHH_CATCH_DISPLACED_PARTY, RHH_CATCH_ACQUIRED, RHH_CATCH_ACQUIRED};
        static const uint32_t destinations[] = {RHH_MON_BOX, RHH_MON_BOX, RHH_MON_PARTY, RHH_MON_BOX, RHH_MON_PARTY, RHH_MON_PARTY};
        static const uint32_t slots[] = {0, 1, 5, 2, 2, 4};
        struct RhhCatchPlacement placement = {.struct_size = sizeof(placement)};
        uint32_t field = 99;
        if (count >= sizeof(causes) / sizeof(causes[0]) || !sOwned || sOwned->catch_placement_context(&placement)
            || placement.cause != causes[count] || placement.destination_location != destinations[count]
            || placement.destination_slot != slots[count] || sOwned->party_view() != RHH_OWNED_PARTY_FOLLOWER_BATTLE
            || !sOwned->follower_party_field_slot(0, &field) || field != 99)
            puts("OWNED_FOLLOWER_PROBE_BAD catch");
        else { ++count; puts("OWNED_FOLLOWER_CATCH_OK"); }
        return;
    }
    if (event->kind == RHH_OWNED_MON_CHANGED)
    {
        if (sOwned && sOwned->party_view() == RHH_OWNED_PARTY_FOLLOWER_BATTLE)
            puts("OWNED_FOLLOWER_PROBE_BAD temporary party observation");
        return;
    }
    uint32_t first = 99, second = 99, third = 99, missing = 99;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon), .identity = 99}, before = mon;
    if (!sOwned || sOwned->party_view() != RHH_OWNED_PARTY_FOLLOWER_BATTLE
        || sOwned->follower_party_field_slot(0, &first) || first != 0
        || sOwned->follower_party_field_slot(event->args[0] ? 2 : 1, &second) || second != 2
        || sOwned->follower_party_field_slot(event->args[0] ? 3 : 2, &third) || third != 4
        || !sOwned->follower_party_field_slot(event->args[0] ? 1 : 3, &missing) || missing != 99
        || !sOwned->party_at(0, &mon) || memcmp(&mon, &before, sizeof(mon)))
        puts("OWNED_FOLLOWER_PROBE_BAD projection");
    else puts("OWNED_FOLLOWER_PROBE_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_PARTY_REORDERED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_OWNED_MON_CHANGED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_OWNED_CATCH_PLACED, RHH_OBSERVE, 0, NULL, Observe},
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 23), RHH_SERVICE_REQUIRED, 0, "rhh:service/owned_mon"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:owned_follower",
    .hook_count = 4, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sOwned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 23));
    return sOwned && sOwned->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_23_SIZE ? &definition : NULL;
}
