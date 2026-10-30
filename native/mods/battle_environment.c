#include "common.h"
static const char sFragment[] =
#include "battle_environment_shader.h"
;
/* Zero-filled state preserves the existing default-off mod preference. */
static int Read(const void *state, const char *id, struct RhhModValue *out)
{
    if (strcmp(id, "enabled")) return -1;
    *out = (struct RhhModValue){RHH_SETTING_BOOL, 0, *(const uint8_t *)state}; return 0;
}
static int Write(void *state, const char *id, const struct RhhModValue *value)
{
    if (strcmp(id, "enabled") || value->type != RHH_SETTING_BOOL || value->value > 1) return -1;
    *(uint8_t *)state = value->value; return 0;
}
static void Environment(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{
    if (*(const uint8_t *)state && event->args[3])
        out->value = sHost->content_key("rhh:shader/battle_environment");
}
static void Notice(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{
    if (*(const uint8_t *)state && event->args[0] == RHH_NOTICE_WEATHER_RECAP && event->args[1])
    { out->allowed = 0; strcpy(out->reason, "Persistent shader displays the current weather"); }
}
static const struct RhhModSetting sSettings[] = {{sizeof(struct RhhModSetting), RHH_SETTING_BOOL,
    RHH_SETTING_RUNTIME, 0, "enabled", "Battle environment", "Persistent weather and terrain shaders replace weather recaps.", 0, 1, 0, 0, NULL}};
static const struct RhhModHook sHooks[] = {
    REDUCER(RHH_BATTLE_ENVIRONMENT, RHH_TRANSFORM, 0, Environment),
    REDUCER(RHH_BATTLE_ENVIRONMENT_NOTICE, RHH_CONSTRAINT, 0, Notice)
};
static const struct RhhModShader sShaders[] = {{sizeof(struct RhhModShader), 0, "rhh:shader/battle_environment", sFragment}};
static const struct RhhModDefinition sMod = {
    .struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1, .state_schema = 1,
    .id = "rhh:battle_environment", .flags = RHH_MOD_REQUIRED, .state_size = 1,
    .hook_count = 2, .hooks = sHooks, .validate = BoolValidate,
    .setting_count = 1, .settings = sSettings, .read_setting = Read, .write_setting = Write,
    .shader_count = 1, .shaders = sShaders
};
const struct RhhModDefinition *RhhBundledBattleEnvironment(const struct RhhModHostV1 *host) { return HostOK(host) ? &sMod : NULL; }
SHARED_ENTRY(RhhBundledBattleEnvironment)
