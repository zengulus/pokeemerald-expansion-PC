#include "common.h"
static void Cap(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{
    if (*(const uint8_t *)state && event->args[0] < out->value) out->value = event->args[0];
}
static const struct RhhModSetting sSettings[] = {BOOL_SETTING("Party level cap", "Use the next Gym Leader's ace, then the Champion's ace.")};
static const struct RhhModHook sHooks[] = {REDUCER(RHH_LEVEL_CAP, RHH_CONSTRAINT, 0, Cap)};
static const struct RhhModDefinition sMod = {
    .struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1, .state_schema = 1,
    .id = "rhh:level_cap", .flags = RHH_MOD_REQUIRED, .state_size = 1,
    .hook_count = 1, .hooks = sHooks, .validate = BoolValidate,
    .setting_count = 1, .settings = sSettings, .read_setting = BoolRead, .write_setting = BoolWrite
};
const struct RhhModDefinition *RhhBundledLevelCap(const struct RhhModHostV1 *host) { return HostOK(host) ? &sMod : NULL; }
SHARED_ENTRY(RhhBundledLevelCap)
