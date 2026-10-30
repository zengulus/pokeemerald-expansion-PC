#include "mod_api.h"
#include <stdio.h>
#include <string.h>

struct BattleHealthProbeState { uint32_t count; struct RhhBattlerHealthObservation last[4]; };
static const struct RhhBattleService *sBattle;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct BattleHealthProbeState *probe = state;
    struct RhhBattlerHealthObservation value = {.struct_size = sizeof(value)};
    struct RhhBattlerHealthObservation tiny = {.struct_size = 1, .changed = 99}, saved = tiny;
    if (sBattle->health_observation_context(&value) || !sBattle->health_observation_context(&tiny)
        || memcmp(&tiny, &saved, sizeof(tiny)) || value.sequence != event->ordinal
        || value.after.battler != event->args[0] || value.before.hp != event->args[1]
        || value.after.hp != event->args[2] || value.changed != event->args[3]
        || value.after.identity != event->subject || value.after.species != event->event_key
        || value.before.identity != value.after.identity || !value.run_id || !value.battle_id
        || !value.changed || value.after.battler >= 4)
        puts("BATTLE_HEALTH_PROBE_BAD");
    else { probe->last[value.after.battler] = value; ++probe->count; }
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLER_HEALTH_OBSERVED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 44), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:battle_health_probe",
    .state_size = sizeof(struct BattleHealthProbeState), .hooks = hooks, .hook_count = 1,
    .service_requirements = requirements, .service_requirement_count = 1
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    sBattle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 44));
    return sBattle && sBattle->header.struct_size >= RHH_BATTLE_SERVICE_V1_44_SIZE ? &definition : NULL;
}
