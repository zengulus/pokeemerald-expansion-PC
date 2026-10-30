#include "mod_api.h"
#include <stdio.h>
#include <string.h>

struct State { uint32_t mode, proposals, processed, banked, last_unique, last_mode, last_stat, last_earned; };
static const struct RhhItemEvYieldService *sService;
static void Transform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    const struct State *s = state;
    struct RhhItemEvYield c = {.struct_size = sizeof(c)}, small = {.struct_size = 1, .earned = 999}, old = small;
    if (sService->context(&c) || !sService->context(&small) || memcmp(&small, &old, sizeof(small))
        || c.item != event->event_key || c.mon.identity != event->subject || c.mon.slot != event->args[0]
        || c.stat != event->args[1] || c.earned != event->args[2] || !c.earned || c.stat >= RHH_STAT_COUNT)
    { puts("ITEM_EV_BAD"); return; }
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i)
        if (c.before[i] != c.mon.evs[i] || c.after[i] != c.before[i] || c.proposed[i] < c.before[i]
            || (i != c.stat && c.proposed[i] != c.before[i]) || (uint32_t)(c.proposed[i] - c.before[i]) > c.earned)
        { puts("ITEM_EV_BAD"); return; }
    if (s->mode == 1 || s->mode == 4) decision->value = RHH_ITEM_EV_BANK;
    else if (s->mode == 3) decision->value = 999;
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; const struct State *s = state;
    if (s->mode == 2 || s->mode == 4) decision->allowed = 0;
    decision->value = RHH_ITEM_EV_BANK;
}
static void Later(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->allowed = 1; }
static void Processed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    struct State *s = state; struct RhhItemEvYield c = {.struct_size = sizeof(c)};
    if (sService->context(&c) || c.item != event->event_key || c.mon.identity != event->subject
        || !decision->allowed || c.mode != decision->value || c.mode > RHH_ITEM_EV_BANK)
    { puts("ITEM_EV_BAD"); return; }
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i)
        if (c.after[i] != (c.mode == RHH_ITEM_EV_BANK ? c.before[i] : c.proposed[i]))
        { puts("ITEM_EV_BAD"); return; }
    ++s->processed; if (c.mode == RHH_ITEM_EV_BANK) s->banked += c.earned;
    s->last_unique = c.identity_unique; s->last_mode = c.mode; s->last_stat = c.stat; s->last_earned = c.earned;
    puts("ITEM_EV_PROCESSED");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_ITEM_EV_PROPOSAL, RHH_TRANSFORM, 0, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_ITEM_EV_PROPOSAL, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_ITEM_EV_PROPOSAL, RHH_CONSTRAINT, 1, Later, NULL},
    {sizeof(struct RhhModHook), RHH_ITEM_EV_PROCESSED, RHH_OBSERVE, 0, NULL, Processed}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/item_ev_yield"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:item_evs", .state_size = sizeof(struct State), .state_schema = 1,
    .hooks = hooks, .hook_count = 4, .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sService = (const void *)host->get_service("rhh:service/item_ev_yield", RHH_SERVICE_VERSION(1, 0));
    return sService && sService->header.struct_size >= sizeof(*sService) ? &definition : NULL;
}
