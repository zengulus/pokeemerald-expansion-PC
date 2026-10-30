#include "mod_api.h"
static const struct RhhBreedingService *sBreeding;
static RhhContentKey sShadow;
static void Legacy(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhBreedingContext context = {.struct_size = sizeof(context)};
    if (!sBreeding->context(&context) && event->ordinal < 4 && context.offspring_moves[event->ordinal] == sShadow)
        decision->value = 0;
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BREED_INHERIT_MOVE, RHH_TRANSFORM, 0, Legacy, NULL}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_move_legacy", .hooks = hooks, .hook_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sBreeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 1));
    sShadow = host->content_key("rhh:move/shadow_ball");
    return sBreeding && sShadow ? &definition : NULL;
}
