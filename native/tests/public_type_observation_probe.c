/* Independent consumer of the player-facing sampled type view. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>

struct PublicTypeProbeState { uint32_t count; struct RhhPublicBattlerTypeObservation last; };
static const struct RhhTypeService *sTypes;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct PublicTypeProbeState *probe = state;
    struct RhhPublicBattlerTypeObservation value = {.struct_size = sizeof(value)};
    struct RhhPublicBattlerTypeObservation tiny = {.struct_size = 1, .reserved = 77}, saved = tiny;
    struct RhhPublicBattlerTypes current = {.struct_size = sizeof(current)};
    if (sTypes->public_battler_observation_context(&value)
        || !sTypes->public_battler_observation_context(&tiny) || memcmp(&tiny, &saved, sizeof(tiny))
        || !sTypes->public_battler_observation_context(NULL)
        || event->event_key || event->subject || event->args[3]
        || value.sequence != event->ordinal || value.after.battler != event->args[0]
        || value.before.flags != event->args[1] || value.after.flags != event->args[2]
        || sTypes->public_battler_types(value.after.battler, &current)
        || current.flags != value.after.flags || current.types.count != value.after.types.count
        || memcmp(current.types.types, value.after.types.types, current.types.count * sizeof(current.types.types[0])))
        puts("PUBLIC_TYPE_OBSERVATION_BAD");
    probe->last = value; ++probe->count;
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_PUBLIC_BATTLER_TYPES_OBSERVED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 16), RHH_SERVICE_REQUIRED, 0, "rhh:service/types"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_TYPES, .id = "test:public_type_observation",
    .state_size = sizeof(struct PublicTypeProbeState), .hooks = hooks, .hook_count = 1,
    .service_requirements = requirements, .service_requirement_count = 1
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    sTypes = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 16));
    return sTypes && sTypes->header.struct_size >= RHH_TYPE_SERVICE_V1_16_SIZE ? &definition : NULL;
}
