#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhBreedingService *breeding;
static const struct RhhDaycareService *daycare;
static struct RhhDaycareEggId currentId;
static uint64_t currentRandom;
static void Permit(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhBreedingRequestResult context = {.struct_size = sizeof(context)};
    struct RhhBreedingRequestResult small = {.struct_size = sizeof(small) - 1, .status = 99}, before = small;
    if (breeding->request_context(&context) || !breeding->request_context(&small) || memcmp(&small, &before, sizeof(small))
        || context.offer.pending || context.offer.token != event->ordinal || context.offer.parent_count != 2)
    { puts("BREED_REQUEST_BAD_CONTEXT"); return; }
    struct RhhBreedingRequestResult nested = {.struct_size = sizeof(nested)};
    if (breeding->request_pending(&context.offer, &nested) != RHH_BREED_REQUEST_UNAVAILABLE)
        puts("BREED_REQUEST_BAD_RECURSION");
    if (context.offer.parents[0].mon.ivs[RHH_STAT_HP] == 7)
    {
        decision->allowed = 0; snprintf(decision->reason, sizeof(decision->reason), "test rule denied request");
        puts("BREED_REQUEST_DENIED");
    }
    else puts("BREED_REQUEST_ALLOWED");
}
static void Committed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBreedingRequestResult context = {.struct_size = sizeof(context)};
    struct RhhDaycareSnapshot snapshot = {.struct_size = sizeof(snapshot)};
    if (breeding->request_context(&context) || context.status != RHH_BREED_REQUEST_OK
        || context.offer.pending || context.offer.token != event->ordinal
        || daycare->current(&snapshot) || !snapshot.egg_pending)
        puts("BREED_REQUEST_BAD_COMMIT");
    else puts("BREED_REQUEST_COMMITTED");
}
static void PendingChanged(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    unsigned char *counts = state;
    unsigned expectedMask = counts[6] ? 1 : 3;
    struct RhhDaycarePendingChange context = {.struct_size = sizeof(context)};
    struct RhhDaycarePendingChange small = {.struct_size = sizeof(small) - 1, .cause = 99}, before = small;
    struct RhhDaycareSnapshot snapshot = {.struct_size = sizeof(snapshot)};
    struct RhhDaycareEggId id = {.struct_size = sizeof(id)}, live = {.struct_size = sizeof(live)};
    uint64_t random = 0;
    if (breeding->pending_context(&context) || !breeding->pending_context(&small) || memcmp(&small, &before, sizeof(small))
        || context.cause < RHH_DAYCARE_PENDING_WALK || context.cause > RHH_DAYCARE_PENDING_ACCEPTED
        || context.cause != event->args[0] || context.before != event->args[1] || context.after != event->args[2]
        || context.before == context.after || context.parent_mask != expectedMask || context.parent_mask != event->args[3]
        || !context.run_id || (!counts[6] && (daycare->current(&snapshot) || snapshot.egg_pending != context.after))
        || (counts[6] && !daycare->current(&snapshot)) || breeding->pending_change_id(&id)
        || breeding->pending_random_at(&id, "test:breeding_request", 77, &random)
        || id.run_id != context.run_id || !(id.high | id.low)
        || (context.after && (breeding->pending_id(&live) || memcmp(&id, &live, sizeof(id))))
        || (!context.after && (!breeding->pending_id(&live) || memcmp(&id, &currentId, sizeof(id)) || random != currentRandom))
        || (context.cause == RHH_DAYCARE_PENDING_ACCEPTED && counts[0] != counts[context.cause] + 1))
    { puts("BREED_REQUEST_BAD_PENDING"); return; }
    if (context.after) { currentId = id; currentRandom = random; }
    ++counts[context.cause];
}
static void EggCreated(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision;
    struct RhhBreedingContext context = {.struct_size = sizeof(context)};
    struct RhhDaycareEggId id = {.struct_size = sizeof(id)};
    unsigned char *counts = state;
    int idStatus = breeding->pending_change_id(&id);
    uint64_t random = 0;
    if (breeding->context(&context) || !context.egg_valid || !context.offspring_mon.is_egg
        || (counts[6] == 2 ? !idStatus : idStatus || memcmp(&id, &currentId, sizeof(id))
            || breeding->pending_random_at(&id, "test:breeding_request", 77, &random) || random != currentRandom))
        puts("BREED_REQUEST_BAD_EGG");
    else ++counts[0];
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_REQUEST_DAYCARE_EGG, RHH_CONSTRAINT, 0, Permit, NULL},
    {sizeof(struct RhhModHook), RHH_DAYCARE_EGG_REQUESTED, RHH_OBSERVE, 0, NULL, Committed},
    {sizeof(struct RhhModHook), RHH_DAYCARE_PENDING_CHANGED, RHH_OBSERVE, 0, NULL, PendingChanged},
    {sizeof(struct RhhModHook), RHH_BREED_EGG_CREATED, RHH_OBSERVE, 0, NULL, EggCreated}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 16), RHH_SERVICE_REQUIRED, 0, "rhh:service/breeding"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/daycare"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_request", .state_size = 7, .hook_count = 4, .hooks = hooks, .service_requirement_count = 2, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    breeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 16));
    daycare = (const void *)host->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 0));
    return breeding && breeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_16_SIZE && daycare ? &definition : NULL;
}
