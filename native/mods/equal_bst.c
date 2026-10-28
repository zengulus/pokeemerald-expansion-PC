#include "common.h"
static void Equalize(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{
    if (!*(const uint8_t *)state) return;
    unsigned weights[6], active = 63, remaining = 600;
    for (unsigned i = 0; i < 6; ++i) weights[i] = out->stats[i] ? out->stats[i] : 1;
    if (event->args[0]) { out->stats[0] = 1; active &= ~1u; --remaining; }
    for (;;)
    {
        unsigned sum = 0; int clipped = 0;
        for (unsigned i = 0; i < 6; ++i) if (active & (1u << i)) sum += weights[i];
        for (unsigned i = 0; i < 6; ++i)
            if ((active & (1u << i)) && (uint64_t)remaining * weights[i] > (uint64_t)255 * sum)
            { out->stats[i] = 255; active &= ~(1u << i); remaining -= 255; clipped = 1; break; }
        if (clipped) continue;
        unsigned used = 0, remainders[6] = {0};
        for (unsigned i = 0; i < 6; ++i)
            if (active & (1u << i))
            {
                unsigned product = remaining * weights[i];
                out->stats[i] = product / sum; remainders[i] = product % sum;
                if (!out->stats[i]) out->stats[i] = 1;
                used += out->stats[i];
            }
        while (used < remaining)
        {
            int best = -1;
            for (unsigned i = 0; i < 6; ++i)
                if ((active & (1u << i)) && out->stats[i] < 255
                    && (best < 0 || remainders[i] > remainders[best])) best = i;
            ++out->stats[best]; remainders[best] = 0; ++used;
        }
        while (used > remaining)
        {
            unsigned best = 0;
            for (unsigned i = 1; i < 6; ++i)
                if ((active & (1u << i)) && out->stats[i] > out->stats[best]) best = i;
            --out->stats[best]; --used;
        }
        break;
    }
}
static const struct RhhModSetting sSettings[] = {BOOL_SETTING("600 BST", "Preserve stat proportions, bounded to 1-255, with a total of 600.")};
static const struct RhhModHook sHooks[] = {REDUCER(RHH_BASE_STATS, RHH_FINALIZE, INT_MAX, Equalize)};
static const struct RhhModDefinition sMod = {
    .struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1, .state_schema = 1,
    .id = "rhh:equal_bst", .flags = RHH_MOD_REQUIRED, .state_size = 1,
    .hook_count = 1, .hooks = sHooks, .validate = BoolValidate,
    .setting_count = 1, .settings = sSettings, .read_setting = BoolRead, .write_setting = BoolWrite
};
const struct RhhModDefinition *RhhBundledEqualBST(const struct RhhModHostV1 *host) { return HostOK(host) ? &sMod : NULL; }
SHARED_ENTRY(RhhBundledEqualBST)
