#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhOwnedMonService *owned;
static unsigned lateGateCalls;
static unsigned capEnabled;
static void Cap(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; if (capEnabled) decision->value = 6; }
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)decision; (void)state; if (event->args[0] == 77) capEnabled = 1; }
static void Gate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhExperienceAwardResult context = {.struct_size = sizeof(context)};
    if (owned->experience_award_context(&context) || context.ticket != event->ordinal
        || context.status != RHH_EXPERIENCE_AWARD_PENDING || context.experience.applied != event->args[1])
    { puts("EXPERIENCE_AWARD_BAD"); return; }
    struct RhhExperienceAwardRequest nested = {.struct_size = sizeof(nested), .amount = 2, .owner = context.owner,
        .expected.struct_size = sizeof(nested.expected)};
    struct RhhExperienceAwardResult rejected = {.struct_size = sizeof(rejected)};
    if (owned->experience(&context.experience.mon, &nested.expected)
        || owned->award_experience(&nested, &rejected) != RHH_EXPERIENCE_AWARD_UNAVAILABLE)
    { puts("EXPERIENCE_AWARD_BAD"); return; }
    if (context.experience.requested == 1)
    { decision->allowed = 0; strcpy(decision->reason, "test denial"); }
    if (context.experience.requested == 799 && ++lateGateCalls == 2)
    { decision->allowed = 0; strcpy(decision->reason, "late denial"); }
    puts("EXPERIENCE_AWARD_GATE");
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhExperienceAwardResult context = {.struct_size = sizeof(context)};
    if (owned->experience_award_context(&context) || context.ticket != event->ordinal
        || context.finished != (event->kind == RHH_EXPERIENCE_FLOW_FINISHED)
        || (event->kind == RHH_EXPERIENCE_AWARDED && context.status != RHH_EXPERIENCE_AWARD_OK))
    { puts("EXPERIENCE_AWARD_BAD"); return; }
    puts(event->kind == RHH_EXPERIENCE_AWARDED ? "EXPERIENCE_AWARD_APPLIED" : "EXPERIENCE_AWARD_FINISHED");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_AWARD_EXPERIENCE, RHH_CONSTRAINT, 0, Gate, NULL},
    {sizeof(struct RhhModHook), RHH_EXPERIENCE_AWARDED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_EXPERIENCE_FLOW_FINISHED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_LEVEL_CAP, RHH_TRANSFORM, 0, Cap, NULL},
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 29), RHH_SERVICE_REQUIRED, 0, "rhh:service/owned_mon"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:experience_award", .hooks = hooks, .hook_count = 5,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    owned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 29));
    return owned && owned->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_29_SIZE ? &definition : NULL;
}
