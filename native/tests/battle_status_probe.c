#include "mod_api.h"
#include <stdio.h>
static const struct RhhBattleService *battle;
struct ProbeState { uint64_t battle_id, next, item_next; };
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct ProbeState *probe = state;
    struct RhhBattleStatusApplication context = {.struct_size = sizeof(context)};
    struct RhhBattleStatusAttribution attribution = {.struct_size = sizeof(attribution)};
    if (battle->status_application_context(&context) || battle->status_attribution(&attribution)
        || (attribution.kind && (!attribution.ability || context.trigger != RHH_BATTLE_STATUS_FROM_ABILITY)))
    { puts("BATTLE_STATUS_EXTERNAL_BAD"); return; }
    if (probe->battle_id != context.battle_id) { probe->battle_id = context.battle_id; probe->next = probe->item_next = 0; }
    if (context.sequence != probe->next++ || event->ordinal != context.sequence
        || context.target_battler != event->args[0] || context.status != event->args[1]
        || context.trigger != event->args[2] || context.source_battler != event->args[3]
        || context.move != event->event_key || context.before_raw == context.after_raw
        || event->subject != context.target.identity)
        puts("BATTLE_STATUS_EXTERNAL_BAD");
    else puts("BATTLE_STATUS_EXTERNAL_OK");
}
static void ObserveItem(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct ProbeState *probe = state;
    struct RhhBattleItemStatusChange context = {.struct_size = sizeof(context)};
    if (battle->item_status_context(&context)) { puts("BATTLE_ITEM_STATUS_EXTERNAL_BAD"); return; }
    if (probe->battle_id != context.battle_id) { probe->battle_id = context.battle_id; probe->next = probe->item_next = 0; }
    if (context.sequence != probe->item_next++ || event->ordinal != context.sequence
        || context.battler != event->args[0] || context.added != event->args[1]
        || context.removed != event->args[2] || context.origin != event->args[3]
        || context.item != event->event_key || event->subject != context.participant.identity
        || !(context.added | context.removed))
        puts("BATTLE_ITEM_STATUS_EXTERNAL_BAD");
    else puts("BATTLE_ITEM_STATUS_EXTERNAL_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLER_STATUS_APPLIED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_BATTLER_ITEM_STATUS_CHANGED, RHH_OBSERVE, 0, NULL, ObserveItem}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 21), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:battle_status_probe", .state_size = sizeof(struct ProbeState), .hooks = hooks, .hook_count = 2,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    battle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 21));
    return battle && battle->header.struct_size >= RHH_BATTLE_SERVICE_V1_21_SIZE ? &definition : NULL;
}
