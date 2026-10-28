#include "common.h"
static int Eligible(uint64_t key)
{
    if (!key) return 1;
    struct RhhContentInfo info = {.struct_size = sizeof(info)};
    return !sHost->content_info(RHH_SPECIES, key, &info) && (info.flags & RHH_CONTENT_CREATABLE)
        && !(info.flags & (RHH_CONTENT_LEGENDARY | RHH_CONTENT_MYTHICAL));
}
static int Read(const void *state, const char *id, struct RhhModValue *out)
{
    if (strcmp(id, "species")) return -1;
    *out = (struct RhhModValue){RHH_SETTING_CONTENT, 0, Read64(state)}; return 0;
}
static int Write(void *state, const char *id, const struct RhhModValue *value)
{
    if (strcmp(id, "species") || value->type != RHH_SETTING_CONTENT || !Eligible(value->value)) return -1;
    Write64(state, value->value); return 0;
}
static int Validate(const uint8_t *state, uint32_t size) { return size == 8 && Eligible(Read64(state)) ? 0 : -1; }
static void Starter(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{ if (event->ordinal == 1 && Read64(state)) out->value = Read64(state); }
static const struct RhhModSetting sSettings[] = {{sizeof(struct RhhModSetting), RHH_SETTING_CONTENT,
    RHH_SETTING_NEW_GAME, RHH_SPECIES, "species", "Starter Pokemon", "Choose any creatable non-legendary, non-mythical species; zero keeps original starters.", 0, UINT64_MAX, 0, 0, NULL}};
static const struct RhhModHook sHooks[] = {REDUCER(RHH_STARTER_SPECIES, RHH_TRANSFORM, 0, Starter)};
static const struct RhhModDefinition sMod = {
    .struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1, .state_schema = 1,
    .id = "rhh:starter", .flags = RHH_MOD_REQUIRED, .state_size = 8,
    .hook_count = 1, .hooks = sHooks, .validate = Validate,
    .setting_count = 1, .settings = sSettings, .read_setting = Read, .write_setting = Write
};
const struct RhhModDefinition *RhhBundledStarter(const struct RhhModHostV1 *host) { return HostOK(host) ? &sMod : NULL; }
SHARED_ENTRY(RhhBundledStarter)
