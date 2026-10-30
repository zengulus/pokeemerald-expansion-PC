#include "mod_api.h"

static void Legacy(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->ordinal == 1 || event->ordinal == 2) decision->value = RHH_BREED_IV_PARENT_0;
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BREED_INHERIT_IV, RHH_TRANSFORM, 0, Legacy, NULL}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_iv_legacy", .hooks = hooks, .hook_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{ return host ? &definition : NULL; }
