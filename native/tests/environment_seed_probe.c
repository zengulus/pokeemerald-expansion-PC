/* Independent public-ABI module exercised by replay.py in a real battle. */
#include "mod_api.h"
#include <stdio.h>

static const struct RhhWorldEnvironmentService *sEnvironment;
static void Seed(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhBattleEnvironmentSeed context = {.struct_size = sizeof(context)};
    if (sEnvironment->battle_seed_context(&context) || context.kind != event->args[0]
        || context.initial_value != event->args[1] || context.environment.map != event->event_key
        || context.environment.step != event->ordinal || decision->value != context.initial_value
        || !(context.battle_flags & RHH_BATTLE_SEED_FIRST) || (context.battle_flags & RHH_BATTLE_SEED_TRAINER)
        || context.opponent_trainers[0] || context.opponent_trainers[1])
    { puts("BATTLE_SEED_PROBE_BAD"); return; }
    if (context.kind == RHH_WORLD_WEATHER)
    { decision->value = RHH_WEATHER_SUN; puts("BATTLE_SEED_PROBE_WEATHER"); }
    else if (context.kind == RHH_WORLD_TERRAIN)
    { decision->value = RHH_TERRAIN_GRASSY; puts("BATTLE_SEED_PROBE_TERRAIN"); }
    else puts("BATTLE_SEED_PROBE_BAD");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_ENVIRONMENT_SEED, RHH_TRANSFORM, 0, Seed, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/world_environment"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:battle_seed_probe", .hook_count = 1, .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sEnvironment = (const void *)host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 1));
    return sEnvironment && sEnvironment->header.struct_size >= sizeof(*sEnvironment) ? &sDefinition : NULL;
}
