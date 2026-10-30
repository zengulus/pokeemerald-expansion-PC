#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhAbilityService *abilities;
static const struct RhhOwnedMonService *owned;
static void Started(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhBattlerAbility result = {.struct_size = sizeof(result)};
    if (!abilities->battler(0, &result) && result.stored && result.battle_id) puts("ABILITY_OBSERVER_OK");
    else puts("ABILITY_BAD_OBSERVER");
}
static void Score(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhBattlerAbility result = {.struct_size = sizeof(result), .stored = 999}, before = result;
    if (abilities->battler(event->args[2], &result) && !memcmp(&result, &before, sizeof(result))) puts("ABILITY_AI_SCOPE_OK");
    else puts("ABILITY_BAD_AI_SCOPE");
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
    struct RhhOwnedAbility selected = {.struct_size = sizeof(selected), .selected_slot = 99}, original = selected;
    if (!owned->party_at(0, &mon) && abilities->owned(&mon, &selected)
        && !memcmp(&selected, &original, sizeof(selected))) puts("ABILITY_AI_OWNED_SCOPE_OK");
    else puts("ABILITY_BAD_AI_OWNED_SCOPE");
    struct RhhInnateResolution innates = {.struct_size = sizeof(innates), .count = 99}, saved = innates;
    if (abilities->battler_innates(event->args[2], &innates) && !memcmp(&innates, &saved, sizeof(saved)))
        puts("ABILITY_AI_INNATE_SCOPE_OK");
    else puts("ABILITY_BAD_AI_INNATE_SCOPE");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, Started},
    {sizeof(struct RhhModHook), RHH_AI_MOVE_SCORE, RHH_TRANSFORM, 0, Score, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:ability_probe", .hook_count = 2, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    abilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 0));
    owned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    return abilities && abilities->header.struct_size >= sizeof(*abilities) && owned ? &definition : NULL;
}
