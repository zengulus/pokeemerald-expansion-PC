/* Independently built consumer of candidate permission and cancellation. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhWildEncounterService *sWild;
static RhhContentKey sZigzagoon;
static void Candidate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; decision->value = sZigzagoon;
}
static void Gate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context)};
    if (sWild->context(&context) || context.phase != RHH_WILD_PHASE_PROPOSAL
        || context.species != sZigzagoon || context.species != event->subject || decision->value != context.species)
        puts("WILD_CANCEL_PROBE_BAD");
}
static void Cancelled(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state;
    struct RhhWildEncounterCancellation result = {.struct_size = sizeof(result)};
    if (sWild->cancellation_context(&result) || decision->allowed
        || result.candidate.phase != RHH_WILD_PHASE_CANCELLED || result.candidate.species != sZigzagoon
        || result.candidate.species != event->subject || strcmp(result.reason, "habitat excludes candidate"))
    { puts("WILD_CANCEL_PROBE_BAD"); return; }
    if (result.candidate.source == RHH_WILD_FEEBAS) puts("WILD_CANCEL_FEEBAS_OK");
    else if (result.candidate.source == RHH_WILD_FISHING_ROD) puts("WILD_CANCEL_FISH_OK");
    else if (result.candidate.area == RHH_WILD_LAND) puts("WILD_CANCEL_LAND_OK");
    else if (result.candidate.area == RHH_WILD_WATER) puts("WILD_CANCEL_WATER_OK");
    else if (result.candidate.area == RHH_WILD_ROCKS) puts("WILD_CANCEL_ROCKS_OK");
    else puts("WILD_CANCEL_PROBE_BAD");
}
static void Generated(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (event->subject == sZigzagoon) puts("WILD_CANCEL_RECOVERY_OK");
    else puts("WILD_CANCEL_PROBE_BAD");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_CANDIDATE, RHH_TRANSFORM, 0, Candidate, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_WILD, RHH_CONSTRAINT, 0, Gate, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_CANCELLED, RHH_OBSERVE, 0, NULL, Cancelled},
    {sizeof(struct RhhModHook), RHH_WILD_GENERATED, RHH_OBSERVE, 0, NULL, Generated}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_encounter"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:wild_cancel_probe",
    .hook_count = 4, .hooks = sHooks, .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service || !host->content_key) return NULL;
    sWild = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 2));
    sZigzagoon = host->content_key("rhh:species/zigzagoon");
    return sWild && sWild->header.struct_size >= RHH_WILD_ENCOUNTER_SERVICE_V1_2_SIZE && sZigzagoon ? &sDefinition : NULL;
}
