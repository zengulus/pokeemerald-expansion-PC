#include "mod_api.h"
#include <stdio.h>
#include <string.h>
/* Test an independently compiled v1.3 consumer against the extended service. */
struct BreedingServiceV13 {
    struct RhhServiceHeader header;
    int (*context)(struct RhhBreedingContext *out);
    int (*preview)(struct RhhBreedingPreview *out);
    int (*offer)(struct RhhBreedingOffer *out);
    int (*accept)(const struct RhhBreedingOffer *, struct RhhBreedingAcceptResult *out);
    int (*accept_context)(struct RhhBreedingAcceptResult *out);
};
_Static_assert(sizeof(struct BreedingServiceV13) == RHH_BREEDING_SERVICE_V1_3_SIZE, "v1.3 breeding prefix changed");
struct BreedingServiceV14 {
    struct BreedingServiceV13 v13;
    int (*iv_preview)(const struct RhhBreedingOffer *, struct RhhBreedingIvPreview *);
};
_Static_assert(sizeof(struct BreedingServiceV14) == RHH_BREEDING_SERVICE_V1_4_SIZE, "v1.4 breeding prefix changed");
struct BreedingServiceV15 {
    struct BreedingServiceV14 v14;
    int (*iv_odds)(const struct RhhBreedingOffer *, struct RhhBreedingIvOdds *);
};
_Static_assert(sizeof(struct BreedingServiceV15) == RHH_BREEDING_SERVICE_V1_5_SIZE, "v1.5 breeding prefix changed");
struct BreedingServiceV16 {
    struct BreedingServiceV15 v15;
    int (*iv_joint_odds)(const struct RhhBreedingOffer *, const struct RhhBreedingIvJointRequest *, struct RhhBreedingIvJointOdds *);
};
_Static_assert(sizeof(struct BreedingServiceV16) == RHH_BREEDING_SERVICE_V1_6_SIZE, "v1.6 breeding prefix changed");
static const struct BreedingServiceV13 *breeding;
static const struct RhhDaycareService *daycare;
static const struct RhhOwnedMonService *mons;
static RhhContentKey alternate;
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    if (event->args[0] == 77) ((unsigned char *)state)[0] ^= 1;
}
static void Offspring(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    if (((const unsigned char *)state)[0]) decision->value = alternate;
}
static void Allow(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhBreedingAcceptResult context = {.struct_size = sizeof(context)};
    struct RhhBreedingAcceptResult small = {.struct_size = sizeof(small) - 1, .party_slot = 999}, old = small;
    if (breeding->accept_context(&context) || !breeding->accept_context(&small) || memcmp(&small, &old, sizeof(small))
        || !context.offer.pending || context.offer.parent_count != 2 || context.offer.token != event->ordinal
        || context.offer.compatibility != event->args[0] || context.offer.party_count != event->args[1])
    { puts("BREED_ACCEPT_BAD_CONTEXT"); return; }
    struct RhhBreedingAcceptResult nested = {.struct_size = sizeof(nested)};
    if (breeding->accept(&context.offer, &nested) != RHH_BREED_ACCEPT_UNAVAILABLE) puts("BREED_ACCEPT_BAD_RECURSION");
    if (context.offer.parents[0].mon.ivs[RHH_STAT_HP] == 7)
    {
        decision->allowed = 0; snprintf(decision->reason, sizeof(decision->reason), "test rule denied egg");
        puts("BREED_ACCEPT_DENIED");
    }
    else puts("BREED_ACCEPT_ALLOWED");
}
static void Created(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBreedingContext context = {.struct_size = sizeof(context)};
    struct RhhDaycareSnapshot snapshot = {.struct_size = sizeof(snapshot)};
    struct RhhOwnedMonSnapshot egg = {.struct_size = sizeof(egg)};
    struct RhhBreedingAcceptResult outside = {.struct_size = sizeof(outside), .party_slot = 999}, old = outside;
    if (breeding->context(&context) || !context.egg_valid || !context.offspring_mon.is_egg
        || event->subject != context.egg_identity || daycare->current(&snapshot) || snapshot.egg_pending
        || mons->party_at(context.offspring_mon.slot, &egg) || !egg.is_egg || egg.identity != event->subject
        || !breeding->accept_context(&outside) || memcmp(&outside, &old, sizeof(outside)))
        puts("BREED_ACCEPT_BAD_COMMIT");
    else puts("BREED_ACCEPT_COMMITTED");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_ACCEPT_DAYCARE_EGG, RHH_CONSTRAINT, 0, Allow, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_EGG_CREATED, RHH_OBSERVE, 0, NULL, Created},
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update},
    {sizeof(struct RhhModHook), RHH_BREED_OFFSPRING, RHH_TRANSFORM, 100, Offspring, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 3), RHH_SERVICE_REQUIRED, 0, "rhh:service/breeding"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/daycare"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/owned_mon"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_accept", .state_size = 1, .hook_count = 4, .hooks = hooks,
    .service_requirement_count = 3, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    breeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 3));
    daycare = (const void *)host->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 0));
    mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    alternate = host->content_key("rhh:species/torchic");
    return breeding && breeding->header.struct_size >= sizeof(*breeding) && daycare && mons && alternate ? &definition : NULL;
}
