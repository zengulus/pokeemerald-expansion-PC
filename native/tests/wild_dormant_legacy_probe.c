/* A legacy live-profile constraint without a dormant counterpart. */
#include "mod_api.h"
#ifdef LEGACY_OBSERVER_ONLY
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)event; (void)decision; (void)state; }
#else
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; (void)decision; }
#endif
static const struct RhhModHook sHooks[] = {
#ifdef LEGACY_OBSERVER_ONLY
    {sizeof(struct RhhModHook), RHH_WILD_PROFILE_EDITED, RHH_OBSERVE, 0, NULL, Observe}
#else
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_WILD_PROFILE, RHH_CONSTRAINT, 0, Constraint, NULL}
#endif
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:dormant_legacy", .hooks = sHooks, .hook_count = 1
};
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{ return host && host->abi_version >= RHH_MOD_ABI_V1 ? &sDefinition : NULL; }
