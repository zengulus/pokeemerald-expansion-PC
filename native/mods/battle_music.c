#include "common.h"
static const char *const sChoices[] = {"Original", "Gen I", "Gen II", "Gen III", "Gen IV", "Gen V"};
static const char *const sRoles[] = {"wild", "trainer", "gym", "champion"};
static int Read(const void *state, const char *id, struct RhhModValue *out) { return ReadByte(state, id, "style", RHH_SETTING_CHOICE, out); }
static int Write(void *state, const char *id, const struct RhhModValue *value)
{
    if (strcmp(id, "style") || value->type != RHH_SETTING_CHOICE || value->value > 5) return -1;
    *(uint8_t *)state = value->value; return 0;
}
static int Validate(const uint8_t *state, uint32_t size) { return size == 1 && state[0] <= 5 ? 0 : -1; }
static void Music(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{
    unsigned style = *(const uint8_t *)state;
    if (style && event->args[0] < 4)
    {
        char key[64]; snprintf(key, sizeof(key), "rhh:midi/gen%u/%s", style, sRoles[event->args[0]]);
        out->value = sHost->content_key(key);
    }
}
static const struct RhhModSetting sSettings[] = {{sizeof(struct RhhModSetting), RHH_SETTING_CHOICE,
    RHH_SETTING_RUNTIME, 0, "style", "Battle music", "MIDI and SoundFont music for Generations I-V.", 0, 5, 6, 0, sChoices}};
static const struct RhhModHook sHooks[] = {REDUCER(RHH_BATTLE_MUSIC, RHH_TRANSFORM, 0, Music)};
static const struct RhhModDefinition sMod = {
    .struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1, .state_schema = 1,
    .id = "rhh:battle_music", .flags = RHH_MOD_REQUIRED, .state_size = 1,
    .hook_count = 1, .hooks = sHooks, .validate = Validate,
    .setting_count = 1, .settings = sSettings, .read_setting = Read, .write_setting = Write
};
const struct RhhModDefinition *RhhBundledBattleMusic(const struct RhhModHostV1 *host) { return HostOK(host) ? &sMod : NULL; }
SHARED_ENTRY(RhhBundledBattleMusic)
