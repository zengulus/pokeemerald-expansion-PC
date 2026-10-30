/* Independent consumer of sampled battler type changes. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>

struct ProbeState { uint32_t count; struct RhhBattlerTypeObservation last; };
static const struct RhhTypeService *sTypes;
static const struct RhhAbilityService *sAbilities;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct ProbeState *probe = state;
    struct RhhBattlerTypeObservation sample = {.struct_size = sizeof(sample)};
    struct RhhBattlerTypeObservation tiny = {.struct_size = 1, .changed = 99}, saved = tiny;
    struct RhhExtendedTypeList current = {.struct_size = sizeof(current)};
    struct RhhBattlerAbility ability = {.struct_size = sizeof(ability)};
    struct RhhInnateResolution innates = {.struct_size = sizeof(innates)};
    unsigned bad = 0;
    if (sTypes->battler_observation_context(&sample)) bad |= 1;
    if (!sTypes->battler_observation_context(&tiny) || memcmp(&tiny, &saved, sizeof(tiny))
        || !sTypes->battler_observation_context(NULL)) bad |= 2;
    if (sample.sequence != event->ordinal || sample.changed != event->args[3]) bad |= 4;
    if (sample.after_participant.battler != event->args[0] || sample.after_participant.owner != event->args[1]
        || sample.after_participant.party_slot != event->args[2] || sample.after_participant.identity != event->subject
        || sample.after_participant.species != event->event_key) bad |= 8;
    if (sTypes->battler_list(event->args[0], &current)) bad |= 16;
    if (sAbilities->battler(event->args[0], &ability) || ability.battler != event->args[0]) bad |= 64;
    if (sAbilities->battler_innates(event->args[0], &innates) || innates.battler != event->args[0]) bad |= 128;
    if (current.count != sample.after_effective.count
        || memcmp(current.types, sample.after_effective.types, current.count * sizeof(current.types[0]))) bad |= 32;
    if (bad) printf("TYPE_OBSERVATION_BAD %u\n", bad);
    probe->last = sample;
    ++probe->count;
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLER_TYPES_OBSERVED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 15), RHH_SERVICE_REQUIRED, 0, "rhh:service/types"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_TYPES, .id = "test:type_observation",
    .state_size = sizeof(struct ProbeState), .hooks = hooks, .hook_count = 1,
    .service_requirements = requirements, .service_requirement_count = 2
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    sTypes = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 15));
    sAbilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 2));
    return sTypes && sAbilities && sTypes->header.struct_size >= RHH_TYPE_SERVICE_V1_15_SIZE ? &definition : NULL;
}
