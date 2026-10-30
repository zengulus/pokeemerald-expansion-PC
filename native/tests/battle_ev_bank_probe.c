#include "mod_api.h"
#include <stdio.h>

struct State { uint32_t mode, processed, accepted, banked, last_mode, last_applied; };
static const struct RhhBattleEvYieldService *service;
static void Proposal(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    const struct State *s = state;
    struct RhhBattleEvYield yield = {.struct_size = sizeof(yield)};
    struct RhhBattleEvPolicy policy = {.struct_size = sizeof(policy)};
    if (service->context(&yield) || service->policy_context(&policy) || policy.accepted
        || policy.mode != RHH_BATTLE_EV_APPLY || yield.mon.identity != event->subject)
        puts("BATTLE_BANK_BAD");
    if (s->mode == 1 || s->mode == 3) decision->value = RHH_BATTLE_EV_BANK;
    else if (s->mode == 2) decision->value = 999;
    if (s->mode == 3) decision->allowed = 0;
}
static void Processed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    struct State *s = state;
    struct RhhBattleEvYield yield = {.struct_size = sizeof(yield)};
    struct RhhBattleEvPolicy policy = {.struct_size = sizeof(policy)};
    if (service->context(&yield) || service->policy_context(&policy) || yield.mon.identity != event->subject
        || yield.applied != !!decision->allowed || (yield.applied && !policy.accepted))
    { puts("BATTLE_BANK_BAD"); return; }
    ++s->processed; s->accepted += policy.accepted; s->last_mode = policy.mode; s->last_applied = yield.applied;
    if (policy.accepted && policy.mode == RHH_BATTLE_EV_BANK)
        for (unsigned i = 0; i < RHH_STAT_COUNT; ++i) s->banked += yield.earned[i];
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_EV_PROPOSAL, RHH_TRANSFORM, 0, Proposal, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_EVS_PROCESSED, RHH_OBSERVE, 0, NULL, Processed}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle_ev_yield"}
};
static const struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:battle_bank", .state_size = sizeof(struct State), .state_schema = 1,
    .hooks = hooks, .hook_count = 2, .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    service = (const void *)host->get_service("rhh:service/battle_ev_yield", RHH_SERVICE_VERSION(1, 1));
    return service && service->header.struct_size >= RHH_BATTLE_EV_YIELD_SERVICE_V1_1_SIZE ? &mod : NULL;
}
