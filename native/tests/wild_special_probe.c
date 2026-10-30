/* Independently built v1.3 consumer for protected special encounters. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhWildEncounterService *sWild;
static void Gate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context)};
    if (sWild->context(&context) || context.phase != RHH_WILD_PHASE_PROPOSAL
        || context.species != event->subject || context.species != decision->value
        || (context.source != RHH_WILD_ROAMER && context.source != RHH_WILD_OUTBREAK))
        puts("WILD_SPECIAL_PROBE_BAD");
}
static void Cancel(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state;
    struct RhhWildEncounterCancellation context = {.struct_size = sizeof(context)};
    if (sWild->cancellation_context(&context) || decision->allowed
        || context.candidate.phase != RHH_WILD_PHASE_CANCELLED || context.candidate.species != event->subject
        || strcmp(context.reason, "habitat excludes candidate"))
    { puts("WILD_SPECIAL_PROBE_BAD"); return; }
    puts(context.candidate.source == RHH_WILD_ROAMER ? "WILD_SPECIAL_ROAMER_DENIED" : "WILD_SPECIAL_OUTBREAK_DENIED");
}
static void Generated(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context)};
    if (sWild->context(&context) || context.phase != RHH_WILD_PHASE_CREATED
        || context.species != event->subject || !context.identity)
    { puts("WILD_SPECIAL_PROBE_BAD"); return; }
    puts(context.source == RHH_WILD_ROAMER ? "WILD_SPECIAL_ROAMER_CREATED" : "WILD_SPECIAL_OUTBREAK_CREATED");
}
/* A legacy v1.2 hook must not receive special candidates or cancellations. */
static void LegacyGate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context)};
    if (sWild->context(&context) || context.source >= RHH_WILD_ROAMER) puts("WILD_SPECIAL_PROBE_BAD");
}
static void LegacyCancel(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhWildEncounterCancellation context = {.struct_size = sizeof(context)};
    if (sWild->cancellation_context(&context) || context.candidate.source >= RHH_WILD_ROAMER)
        puts("WILD_SPECIAL_PROBE_BAD");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_SPECIAL_WILD, RHH_CONSTRAINT, 0, Gate, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_SPECIAL_CANCELLED, RHH_OBSERVE, 0, NULL, Cancel},
    {sizeof(struct RhhModHook), RHH_WILD_SPECIAL_GENERATED, RHH_OBSERVE, 0, NULL, Generated},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_WILD, RHH_CONSTRAINT, 0, LegacyGate, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_CANCELLED, RHH_OBSERVE, 0, NULL, LegacyCancel}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 3), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_encounter"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:wild_special_probe",
    .hook_count = 5, .hooks = sHooks, .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sWild = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 3));
    return sWild && sWild->header.struct_size >= RHH_WILD_ENCOUNTER_SERVICE_V1_3_SIZE ? &sDefinition : NULL;
}
