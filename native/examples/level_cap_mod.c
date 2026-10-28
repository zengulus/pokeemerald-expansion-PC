/* Build: gcc -shared -fPIC -iquote include native/examples/level_cap_mod.c -o cap.so
 * Run: ./build/native/linux64/pokeemerald-expansion --mod ./cap.so
 * This optional example caps party levels at 50, composing by minimum with
 * the built-in Gym cap. It has no mutable state and consumes no engine RNG. */
#include "mod_api.h"

static void Cap(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{
    (void)event; (void)state;
    if (out->value > 50) out->value = 50;
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_LEVEL_CAP, RHH_CONSTRAINT, 0, Cap, 0}
};
static const struct RhhModDefinition sDefinition = {
    sizeof(struct RhhModDefinition), RHH_MOD_ABI_V1, 1, 1, "example:cap50",
    RHH_MOD_REQUIRED, 0, 1, 0, sHooks, 0, 0
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    return host && host->abi_version == RHH_MOD_ABI_V1 && host->struct_size >= sizeof(*host)
        ? &sDefinition : 0;
}
