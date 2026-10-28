#include "common.h"
static int Write(void *state, const char *id, const struct RhhModValue *value)
{
    if (BoolWrite(state, id, value)) return -1;
    uint8_t *s = state; return s[0] && !Read16(s + 2) ? BuildPool(s) : 0;
}
static int Validate(const uint8_t *state, uint32_t size) { return ValidatePool(state, size, 1); }
static void Randomize(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{
    const uint8_t *s = state; unsigned count = Read16(s + 2);
    if (s[0] && count)
    {
        unsigned pick = sHost->random_at(event->run_seed, "rhh:trainer_randomizer", event->event_key, event->ordinal) % count;
        out->value = Read64(s + POOL_OFFSET + pick * 8);
    }
}
static const struct RhhModSetting sSettings[] = {BOOL_SETTING("Trainer randomization", "Stable randomized species for each trainer and party position.")};
static const struct RhhModHook sHooks[] = {REDUCER(RHH_TRAINER_SPECIES, RHH_TRANSFORM, 0, Randomize)};
static const struct RhhModDefinition sMod = {
    .struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1, .state_schema = 1,
    .id = "rhh:trainer_randomizer", .flags = RHH_MOD_REQUIRED, .state_size = POOL_STATE_SIZE,
    .hook_count = 1, .hooks = sHooks, .validate = Validate,
    .setting_count = 1, .settings = sSettings, .read_setting = BoolRead, .write_setting = Write
};
const struct RhhModDefinition *RhhBundledTrainerRandomizer(const struct RhhModHostV1 *host) { return HostOK(host) ? &sMod : NULL; }
SHARED_ENTRY(RhhBundledTrainerRandomizer)
