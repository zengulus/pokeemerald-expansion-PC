#include "mod_api.h"
#include <stdio.h>
#include <string.h>

struct State { uint32_t processed, denied, earned_speed, unique; };
static const struct RhhBattleEvYieldService *sService;
static RhhContentKey sDenySpecies;
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhBattleEvYield yield = {.struct_size = sizeof(yield)};
    if (sService->context(&yield) || yield.mon.identity != event->subject
        || yield.defeated_species != event->event_key || yield.mon.slot != event->args[0])
    { puts("BATTLE_EV_BAD"); return; }
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i)
        if (yield.mon.evs[i] != yield.before[i] || yield.after[i] != yield.before[i]
            || (yield.proposed[i] >= yield.before[i]
                && yield.proposed[i] - yield.before[i] > yield.earned[i]))
        { puts("BATTLE_EV_BAD"); return; }
    if (yield.defeated_species == sDenySpecies)
    { decision->allowed = 0; snprintf(decision->reason, sizeof(decision->reason), "bank the yield"); }
    puts("BATTLE_EV_CONSTRAINT");
}
static void CannotUndo(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->allowed = 1; snprintf(decision->reason, sizeof(decision->reason), "override"); }
static void Processed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    struct State *s = state;
    struct RhhBattleEvYield yield = {.struct_size = sizeof(yield)};
    if (sService->context(&yield) || yield.mon.identity != event->subject || yield.applied != !!decision->allowed
        || (!yield.applied && strcmp(yield.reason, "bank the yield")))
    { puts("BATTLE_EV_BAD"); return; }
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i)
        if (yield.after[i] != (yield.applied ? yield.proposed[i] : yield.before[i]))
        { puts("BATTLE_EV_BAD"); return; }
    ++s->processed; s->denied += !yield.applied; s->earned_speed += yield.earned[RHH_STAT_SPEED];
    s->unique = yield.identity_unique;
    puts("BATTLE_EV_PROCESSED");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_APPLY_BATTLE_EVS, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_APPLY_BATTLE_EVS, RHH_CONSTRAINT, 1, CannotUndo, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_EVS_PROCESSED, RHH_OBSERVE, 0, NULL, Processed}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle_ev_yield"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:battle_evs", .state_size = sizeof(struct State), .state_schema = 1,
    .hook_count = sizeof(sHooks) / sizeof(*sHooks), .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sService = (const void *)host->get_service("rhh:service/battle_ev_yield", RHH_SERVICE_VERSION(1, 0));
    sDenySpecies = host->content_key("rhh:species/poochyena");
    return sService && sDenySpecies ? &sDefinition : NULL;
}
