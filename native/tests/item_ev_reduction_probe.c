#include "mod_api.h"
#include <stdio.h>
#include <string.h>
struct State { uint32_t deny, proposed, processed, removed, mask, unique; };
static const struct RhhItemEvYieldService *service;
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    const struct State *s = state;
    struct RhhItemEvReduction c = {.struct_size = sizeof(c)}, small = {.struct_size = 1, .stat_mask = 999}, old = small;
    if (service->reduction_context(&c) || !service->reduction_context(&small) || memcmp(&small, &old, sizeof(small))
        || c.item != event->event_key || c.mon.identity != event->subject || c.stat_mask != event->args[1]
        || c.mon.slot != event->args[0] || c.change > 0 || !c.stat_mask)
        puts("ITEM_REDUCTION_BAD");
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i)
        if (c.before[i] != c.mon.evs[i] || c.after[i] != c.before[i] || c.proposed[i] > c.before[i]
            || (!(c.stat_mask & (1u << i)) && c.proposed[i] != c.before[i])) puts("ITEM_REDUCTION_BAD");
    if (s->deny) { decision->allowed = 0; strcpy(decision->reason, "no EV reductions"); }
}
static void Processed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    struct State *s = state; struct RhhItemEvReduction c = {.struct_size = sizeof(c)};
    if (service->reduction_context(&c) || c.mon.identity != event->subject || !decision->allowed)
    { puts("ITEM_REDUCTION_BAD"); return; }
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i)
    {
        if (c.after[i] != c.proposed[i]) puts("ITEM_REDUCTION_BAD");
        s->removed += c.before[i] - c.after[i];
    }
    ++s->processed; s->mask = c.stat_mask; s->unique = c.identity_unique;
    puts("ITEM_REDUCTION_PROCESSED");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_REDUCE_ITEM_EVS, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_ITEM_EV_REDUCTION_PROCESSED, RHH_OBSERVE, 0, NULL, Processed}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/item_ev_yield"}
};
static const struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:item_reduction", .state_size = sizeof(struct State), .state_schema = 1, .hooks = hooks, .hook_count = 2,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    service = (const void *)host->get_service("rhh:service/item_ev_yield", RHH_SERVICE_VERSION(1, 1));
    return service && service->header.struct_size >= RHH_ITEM_EV_YIELD_SERVICE_V1_1_SIZE ? &mod : NULL;
}
