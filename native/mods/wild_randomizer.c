#include "common.h"
static const char *const sChoices[] = {"Off", "Stable area pools", "Every encounter"};
static int Read(const void *state, const char *id, struct RhhModValue *out) { return ReadByte(state, id, "mode", RHH_SETTING_CHOICE, out); }
static int Write(void *state, const char *id, const struct RhhModValue *value)
{
    uint8_t *s = state;
    if (strcmp(id, "mode") || value->type != RHH_SETTING_CHOICE || value->value > 2) return -1;
    s[0] = value->value;
    return s[0] && !Read16(s + 2) ? BuildPool(s) : 0;
}
static int Validate(const uint8_t *state, uint32_t size) { return ValidatePool(state, size, 2); }
static int Migrate(uint32_t schema, const uint8_t *old, uint32_t oldSize, uint8_t *state, uint32_t size)
{
    /* v1 stored the mode and draw ordinal; v2 freezes the canonical roster. */
    if (schema != 1 || oldSize != 16 || size != POOL_STATE_SIZE || old[0] > 2) return -1;
    memset(state, 0, size); memcpy(state, old, oldSize);
    return state[0] ? BuildPool(state) : 0;
}
static void Randomize(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{
    const uint8_t *s = state; unsigned count = Read16(s + 2);
    if (!s[0] || !count) return;
    uint64_t ordinal = s[0] == 2 ? Read64(s + 8) : ((uint64_t)event->args[0] << 32 | event->args[1]);
    unsigned pick = sHost->random_at(event->run_seed, "rhh:wild_randomizer", event->event_key, ordinal) % count;
    out->value = Read64(s + POOL_OFFSET + pick * 8);
}
static void Created(const struct RhhModEvent *event, const struct RhhModDecision *out, void *state)
{ (void)event; (void)out; uint8_t *s = state; if (s[0] == 2) Write64(s + 8, Read64(s + 8) + 1); }
static const struct RhhModSetting sSettings[] = {{sizeof(struct RhhModSetting), RHH_SETTING_CHOICE,
    RHH_SETTING_NEW_GAME, 0, "mode", "Wild randomization", "Stable area tables or a new draw for every encounter.", 0, 2, 3, 0, sChoices}};
static const struct RhhModHook sHooks[] = {REDUCER(RHH_WILD_SPECIES, RHH_TRANSFORM, 0, Randomize), OBSERVER(RHH_WILD_CREATED, Created)};
static const struct RhhModDefinition sMod = {
    .struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1, .state_schema = 2,
    .id = "rhh:wild_randomizer", .flags = RHH_MOD_REQUIRED, .state_size = POOL_STATE_SIZE,
    .hook_count = 2, .hooks = sHooks, .validate = Validate, .migrate = Migrate,
    .setting_count = 1, .settings = sSettings, .read_setting = Read, .write_setting = Write
};
const struct RhhModDefinition *RhhBundledWildRandomizer(const struct RhhModHostV1 *host) { return HostOK(host) ? &sMod : NULL; }
SHARED_ENTRY(RhhBundledWildRandomizer)
