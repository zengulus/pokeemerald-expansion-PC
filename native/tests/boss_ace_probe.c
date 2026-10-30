#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhBossAceService *aces;
static const struct RhhTrainerRosterService *rosters;
static RhhContentKey trainer, replacement;
static void Randomize(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == trainer && event->ordinal == 2) decision->value = replacement;
}
static void Permission(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhBossAceResult context = {.struct_size = sizeof(context)};
    struct RhhBossAceResult small = {.struct_size = sizeof(small) - 1, .status = 99}, old = small;
    if (aces->context(&context) || !aces->context(&small) || memcmp(&small, &old, sizeof(small))
        || context.is_preview != event->args[2] || context.before.trainer != event->event_key
        || context.before.revision != event->ordinal || !context.changed)
    { puts("BOSS_ACE_BAD_CONTEXT"); return; }
    struct RhhBossAceRequest nested = {.struct_size = sizeof(nested), .action = RHH_BOSS_ACE_SET,
        .trainer = context.after.trainer, .owner = event->subject, .expected_revision = context.before.revision,
        .trainer_difficulty = context.after.trainer_difficulty, .source_index = context.after.source_index,
        .roster.struct_size = sizeof(nested.roster)};
    struct RhhBossAceResult result = {.struct_size = sizeof(result)};
    if (event->args[0] == RHH_BOSS_ACE_SET
        && (rosters->get(nested.trainer, &nested.roster)
            || aces->apply(&nested, &result) != RHH_BOSS_ACE_UNAVAILABLE_COMMAND))
        puts("BOSS_ACE_BAD_RECURSION");
    if (event->args[0] == RHH_BOSS_ACE_SET && event->args[1] == 0)
    { decision->allowed = 0; snprintf(decision->reason, sizeof(decision->reason), "test denied"); puts("BOSS_ACE_DENIED"); }
    else puts(context.is_preview ? "BOSS_ACE_PREVIEW" : "BOSS_ACE_PERMISSION");
}
static void Changed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBossAceResult context = {.struct_size = sizeof(context)};
    struct RhhBossAceSelection live = {.struct_size = sizeof(live)};
    if (aces->context(&context) || context.is_preview || !context.changed || context.status != RHH_BOSS_ACE_OK
        || context.after.revision != event->ordinal || aces->get(context.after.trainer, context.after.trainer_difficulty, &live)
        || live.valid != context.after.valid || live.revision != context.after.revision
        || (live.valid && (live.owner != context.after.owner || live.source_index != context.after.source_index)))
        puts("BOSS_ACE_BAD_COMMIT");
    else puts("BOSS_ACE_CHANGED");
}
static void BattleStarted(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhBossAceMatch match = {.struct_size = sizeof(match)};
    if (aces->match(RHH_BATTLE_OWNER_OPPONENT_A, 2, &match) || !match.matched
        || match.selection.source_index != 2 || match.source.generated_species != replacement)
        puts("BOSS_ACE_BAD_BATTLE_MATCH");
    else puts("BOSS_ACE_BATTLE_MATCHED");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_TRAINER_SPECIES, RHH_TRANSFORM, 100, Randomize, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_CONFIGURE_BOSS_ACE, RHH_CONSTRAINT, 0, Permission, NULL},
    {sizeof(struct RhhModHook), RHH_BOSS_ACE_CONFIGURED, RHH_OBSERVE, 0, NULL, Changed},
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, BattleStarted}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/boss_ace"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/trainer_roster"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:boss_ace_probe", .hook_count = 4, .hooks = hooks, .service_requirement_count = 2, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service || !host->content_key) return NULL;
    aces = (const void *)host->get_service("rhh:service/boss_ace", RHH_SERVICE_VERSION(1, 0));
    rosters = (const void *)host->get_service("rhh:service/trainer_roster", RHH_SERVICE_VERSION(1, 0));
    trainer = host->content_key("rhh:trainer/roxanne_1"); replacement = host->content_key("rhh:species/mudkip");
    return aces && rosters && trainer && replacement ? &definition : NULL;
}
