#include "mod_api.h"
#include <stdio.h>

static const struct RhhBattleFieldRulesService *field;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhBattleFieldRules result = {.struct_size = sizeof(result)};
    if (field->current(&result) || result.active != (RHH_BATTLE_FIELD_GRAVITY | RHH_BATTLE_FIELD_TRICK_ROOM)
        || result.gravity_turns != 3 || result.trick_room_turns != 2 || result.turn != 7 || !result.run_id || !result.battle_id)
        puts("BATTLE_FIELD_EXTERNAL_BAD");
    else puts("BATTLE_FIELD_EXTERNAL_OK");
}
static void ObserveChange(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleFieldRulesObservation change = {.struct_size = sizeof(change)};
    if (field->observation(&change) || event->ordinal != change.sequence || event->subject != change.after.battle_id
        || event->args[0] != change.active_changed || event->args[1] != change.timer_changed
        || event->args[2] != change.after.active || event->args[3]
        || (change.sequence == 0 && (change.active_changed != RHH_BATTLE_FIELD_MAGIC_ROOM
            || change.timer_changed != RHH_BATTLE_FIELD_MAGIC_ROOM || change.after.magic_room_turns != 5))
        || (change.sequence == 1 && (change.active_changed || change.timer_changed != RHH_BATTLE_FIELD_GRAVITY
            || change.after.gravity_turns != 2)) || change.sequence > 1)
        puts("BATTLE_FIELD_CHANGE_BAD");
    else puts("BATTLE_FIELD_CHANGE_OK");
}
static void ObserveCommit(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleFieldRulesCommit commit = {.struct_size = sizeof(commit)};
    static const uint32_t expectedFlags[] = {RHH_BATTLE_FIELD_TRICK_ROOM, RHH_BATTLE_FIELD_TRICK_ROOM,
        RHH_BATTLE_FIELD_GRAVITY, RHH_BATTLE_FIELD_GRAVITY, RHH_BATTLE_FIELD_ION_DELUGE, RHH_BATTLE_FIELD_GRAVITY};
    if (field->commit(&commit) || event->ordinal != commit.sequence || event->subject != commit.after.battle_id || event->event_key != commit.move
        || event->args[0] != commit.cause || event->args[1] != commit.source_battler
        || event->args[2] != commit.active_changed || event->args[3] != commit.timer_changed
        || !commit.active_changed || (commit.active_changed & (commit.active_changed - 1))
        || (commit.sequence < sizeof(expectedFlags) / sizeof(expectedFlags[0])
            && (commit.active_changed != expectedFlags[commit.sequence]
                || (commit.sequence == 3 ? commit.cause != RHH_BATTLE_FIELD_CAUSE_EXPIRED || commit.move
                    || commit.source_battler != RHH_BATTLE_NO_BATTLER
                    : commit.cause != RHH_BATTLE_FIELD_CAUSE_MOVE || !commit.move || commit.source_battler != 0))))
        puts("BATTLE_FIELD_COMMIT_BAD");
    else puts("BATTLE_FIELD_COMMIT_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_BATTLE_FIELD_RULES_OBSERVED, RHH_OBSERVE, 0, NULL, ObserveChange},
    {sizeof(struct RhhModHook), RHH_BATTLE_FIELD_RULES_COMMITTED, RHH_OBSERVE, 0, NULL, ObserveCommit}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle_field_rules"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:battle_field", .hook_count = 3, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    field = (const void *)host->get_service("rhh:service/battle_field_rules", RHH_SERVICE_VERSION(1, 2));
    return field && field->header.struct_size >= RHH_BATTLE_FIELD_RULES_SERVICE_V1_2_SIZE ? &definition : NULL;
}
