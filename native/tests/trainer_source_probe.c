/* Independent mod checks authored trainer source through species randomization. */
#include "mod_api.h"
#include <stdio.h>
static const struct RhhBattleService *battle;
static const struct RhhTrainerRosterService *rosters;
static RhhContentKey trainer, original, replacement;
static unsigned starts;
static void Transform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == trainer && event->ordinal == 2) decision->value = replacement;
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhTrainerPartySource source = {.struct_size = sizeof(source)};
    if (starts++) {
        if (!battle->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 2, &source)) puts("TRAINER_SOURCE_PROBE_BAD_STALE");
        else puts("TRAINER_SOURCE_PROBE_CLEARED");
        return;
    }
    if (battle->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 2, &source)
        || source.trainer != trainer || source.roster_trainer != trainer || source.source_difficulty != RHH_TRAINER_DIFFICULTY_NORMAL
        || source.source_index != 2 || source.source_count != 3
        || source.source_species != original || source.generated_species != replacement || source.source_level != 15)
        puts("TRAINER_SOURCE_PROBE_BAD");
    else {
        struct RhhTrainerRoster roster = {.struct_size = sizeof(roster)};
        struct RhhTrainerRosterEntry entry = {.struct_size = sizeof(entry)};
        uint32_t matches = 0;
        if (rosters->get(trainer, &roster) || rosters->entry(&roster, 2, &entry)
            || entry.species != original || rosters->match(&roster, 2, RHH_BATTLE_OWNER_OPPONENT_A, 2, &matches) || !matches)
            puts("TRAINER_SOURCE_PROBE_BAD_ROSTER");
        else { puts("TRAINER_SOURCE_PROBE_OK"); puts("TRAINER_ROSTER_PROBE_OK"); }
    }
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_TRAINER_SPECIES, RHH_TRANSFORM, 100, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/trainer_roster"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:trainer_source",
    .hook_count = 2, .hooks = hooks, .service_requirement_count = 2, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service || !host->content_key) return NULL;
    battle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 1));
    rosters = (const void *)host->get_service("rhh:service/trainer_roster", RHH_SERVICE_VERSION(1, 0));
    trainer = host->content_key("rhh:trainer/roxanne_1");
    original = host->content_key("rhh:species/nosepass");
    replacement = host->content_key("rhh:species/mudkip");
    return battle && battle->header.struct_size >= sizeof(*battle) && rosters ? &definition : NULL;
}
