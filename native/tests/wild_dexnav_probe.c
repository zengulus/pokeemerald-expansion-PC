/* Independent v1.4 consumer; legacy v1.3 events must not receive DexNav. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhWildEncounterService *sWild;
static void Gate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    if (sWild->context(&c) || c.source != RHH_WILD_DEXNAV || c.phase != RHH_WILD_PHASE_PROPOSAL
        || c.species != event->subject || c.species != decision->value || c.identity || c.slot != UINT32_MAX)
        puts("DEXNAV_PROBE_BAD");
}
static void Notice(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    if (sWild->context(&c) || c.source != RHH_WILD_DEXNAV || c.species != event->subject)
    { puts("DEXNAV_PROBE_BAD"); return; }
    if (event->kind == RHH_WILD_DEXNAV_CANCELLED)
    {
        struct RhhWildEncounterCancellation cancellation = {.struct_size = sizeof(cancellation)};
        if (decision->allowed || c.phase != RHH_WILD_PHASE_CANCELLED || sWild->cancellation_context(&cancellation)
            || strcmp(cancellation.reason, "habitat excludes candidate")) puts("DEXNAV_PROBE_BAD");
        else puts("DEXNAV_PROBE_CANCELLED");
    }
    else if (c.phase != RHH_WILD_PHASE_CREATED || !c.identity) puts("DEXNAV_PROBE_BAD");
    else puts("DEXNAV_PROBE_GENERATED");
}
static void LegacyGate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; (void)decision; puts("DEXNAV_PROBE_BAD"); }
static void LegacyNotice(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)event; (void)decision; (void)state; puts("DEXNAV_PROBE_BAD"); }
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_DEXNAV_WILD, RHH_CONSTRAINT, 0, Gate, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_DEXNAV_CANCELLED, RHH_OBSERVE, 0, NULL, Notice},
    {sizeof(struct RhhModHook), RHH_WILD_DEXNAV_GENERATED, RHH_OBSERVE, 0, NULL, Notice},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_SPECIAL_WILD, RHH_CONSTRAINT, 0, LegacyGate, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_SPECIAL_CANCELLED, RHH_OBSERVE, 0, NULL, LegacyNotice},
    {sizeof(struct RhhModHook), RHH_WILD_SPECIAL_GENERATED, RHH_OBSERVE, 0, NULL, LegacyNotice},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_WILD, RHH_CONSTRAINT, 0, LegacyGate, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_CANCELLED, RHH_OBSERVE, 0, NULL, LegacyNotice}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 4), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_encounter"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:wild_dexnav_probe",
    .hook_count = sizeof(sHooks) / sizeof(*sHooks), .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sWild = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 4));
    return sWild && sWild->header.struct_size >= RHH_WILD_ENCOUNTER_SERVICE_V1_4_SIZE ? &sDefinition : NULL;
}
