#include "common.h"
static void Follow(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{ (void)event; if (*(const uint8_t *)state) out->value = 1; }
static const struct RhhModSetting sSettings[] = {BOOL_SETTING("Follower Pokemon", "Let the first healthy party Pokemon follow you.")};
static const struct RhhModHook sHooks[] = {REDUCER(RHH_FOLLOWER_ENABLED, RHH_TRANSFORM, 0, Follow)};
static const struct RhhModDefinition sMod = {
    .struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1, .state_schema = 1,
    .id = "rhh:follower", .flags = RHH_MOD_REQUIRED, .state_size = 1,
    .hook_count = 1, .hooks = sHooks, .validate = BoolValidate,
    .setting_count = 1, .settings = sSettings, .read_setting = BoolRead, .write_setting = BoolWrite
};
const struct RhhModDefinition *RhhBundledFollower(const struct RhhModHostV1 *host) { return HostOK(host) ? &sMod : NULL; }
SHARED_ENTRY(RhhBundledFollower)
