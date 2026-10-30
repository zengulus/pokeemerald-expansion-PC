/* Independent consumer of copied battle lifecycle snapshots. */
#include "mod_api.h"
#include <stdio.h>
/* Compile a consumer of the original v1.0 prefix even with a newer header. */
struct LegacyBattleService {
    struct RhhServiceHeader header;
    int (*current)(struct RhhBattleSnapshot *out);
    int (*context)(struct RhhBattleEventContext *out);
};
_Static_assert(sizeof(struct LegacyBattleService) == RHH_BATTLE_SERVICE_V1_0_SIZE, "battle v1.0 prefix changed");
static const struct LegacyBattleService *battle;
static const struct RhhBattleService *battle11;
static uint64_t last_id, sequence;
static int active;
static void Pure(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhBattleNature nature = {.struct_size = sizeof(nature), .nature = 99};
    if (!battle11->nature(0, &nature) || nature.nature != 99) puts("BATTLE_LIFECYCLE_BAD_NATURE_PURITY");
    else puts("BATTLE_LIFECYCLE_NATURE_PURE_BLOCKED");
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleEventContext context = {.struct_size = sizeof(context)};
    struct RhhBattleSnapshot current = {.struct_size = sizeof(current)};
    if (battle->context(&context) || battle->current(&current) || context.event != event->kind
        || context.battle.battle_id != current.battle_id || context.battle.run_id != current.run_id
        || context.battle.participant_count != current.participant_count || context.sequence != event->ordinal
        || context.battler != event->args[0] || context.cause != event->args[1]
        || context.battle.outcome != event->args[2])
    { puts("BATTLE_LIFECYCLE_BAD_CONTEXT"); return; }
    if (event->kind == RHH_BATTLE_STARTED) {
        if (active || context.sequence || context.battle.battle_id <= last_id
            || context.battle.phase != RHH_BATTLE_PHASE_ACTIVE) puts("BATTLE_LIFECYCLE_BAD_START");
        active = 1; last_id = context.battle.battle_id; sequence = 1;
        puts("BATTLE_LIFECYCLE_START");
    } else {
        if (!active || context.battle.battle_id != last_id || context.sequence != sequence++)
            puts("BATTLE_LIFECYCLE_BAD_SEQUENCE");
        if (event->kind == RHH_BATTLER_ENTERED) {
            int found = 0;
            for (unsigned i = 0; i < current.participant_count; ++i)
                if (current.participants[i].battler == context.battler) {
                    found = 1;
                    if (current.participants[i].identity != event->subject) puts("BATTLE_LIFECYCLE_BAD_IDENTITY");
                }
            if (!found || (context.cause != RHH_BATTLE_ENTRY_INITIAL && context.cause != RHH_BATTLE_ENTRY_SWITCH))
                puts("BATTLE_LIFECYCLE_BAD_ENTRY");
            if (!(current.flags & (RHH_BATTLE_LINK | RHH_BATTLE_RECORDED))) {
                struct RhhBattleNature nature = {.struct_size = sizeof(nature)};
                if (battle11->nature(context.battler, &nature) || nature.battle_id != current.battle_id
                    || nature.battler != context.battler || !nature.nature_key)
                    puts("BATTLE_LIFECYCLE_BAD_NATURE");
                else puts("BATTLE_LIFECYCLE_NATURE");
            }
            puts(context.cause == RHH_BATTLE_ENTRY_INITIAL ? "BATTLE_LIFECYCLE_INITIAL" : "BATTLE_LIFECYCLE_SWITCH");
        } else {
            if (context.battle.phase != RHH_BATTLE_PHASE_ENDED || context.battler != RHH_BATTLE_NO_BATTLER)
                puts("BATTLE_LIFECYCLE_BAD_END");
            printf("BATTLE_LIFECYCLE_OUTCOME_%u\n", context.battle.outcome);
            active = 0; puts(context.cause == RHH_BATTLE_END_ABORTED ? "BATTLE_LIFECYCLE_ABORT" : "BATTLE_LIFECYCLE_END");
        }
    }
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_BATTLER_ENTERED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_BATTLE_ENDED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_TYPE_MATCHUP, RHH_TRANSFORM, 0, Pure, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:battle_lifecycle",
    .hook_count = 4, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    battle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 0));
    battle11 = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 11));
    return battle && battle->header.struct_size >= sizeof(*battle) && battle11
        && battle11->header.struct_size >= RHH_BATTLE_SERVICE_V1_11_SIZE ? &definition : NULL;
}
