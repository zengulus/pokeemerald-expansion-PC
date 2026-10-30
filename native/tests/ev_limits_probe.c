#include "mod_api.h"
#include <stdio.h>

static const struct RhhEvLimitService *sLimits;
static void Transform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    unsigned mode = *(const unsigned char *)state;
    struct RhhEvLimits context = {.struct_size = sizeof(context)};
    if (sLimits->context(&context) || context.mon.identity != event->subject || context.source != event->args[0])
    { puts("EV_LIMIT_BAD"); return; }
    if (mode == 1 || mode == 4)
    { decision->value = 700; for (unsigned i = 0; i < RHH_STAT_COUNT; ++i) decision->stats[i] = 255; }
    if (mode == 2) { decision->value = 40; decision->stats[RHH_STAT_HP] = 8; }
    if (mode == 3) { decision->value = 1531; decision->stats[RHH_STAT_HP] = 256; }
    puts("EV_LIMIT_TRANSFORM");
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    if (*(const unsigned char *)state == 4) { decision->value = 300; decision->stats[RHH_STAT_HP] = 10; }
}
static void CannotIncrease(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    if (*(const unsigned char *)state == 4) { decision->value = 900; decision->stats[RHH_STAT_HP] = 250; }
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_EV_LIMITS, RHH_TRANSFORM, 0, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_EV_LIMITS, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_EV_LIMITS, RHH_CONSTRAINT, 1, CannotIncrease, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/ev_limits"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:ev_limits", .state_size = 1, .state_schema = 1,
    .hook_count = sizeof(sHooks) / sizeof(*sHooks), .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sLimits = (const void *)host->get_service("rhh:service/ev_limits", RHH_SERVICE_VERSION(1, 0));
    return sLimits ? &sDefinition : NULL;
}
