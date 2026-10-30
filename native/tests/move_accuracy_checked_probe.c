#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhMoveService *sMoves;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhMoveAccuracyCheck check = {.struct_size = sizeof(check)};
    struct RhhMoveAccuracyCheck small = {.struct_size = 1, .missed = 99}, saved = small;
    if (sMoves->accuracy_checked_context(&check) || !sMoves->accuracy_checked_context(&small)
        || memcmp(&small, &saved, sizeof(small)) || check.run_id != 456 || !check.battle_id
        || check.sequence != event->ordinal || check.move != event->event_key || check.target_species != event->subject
        || check.attacker != event->args[0] || check.target != event->args[1]
        || check.missed != event->args[2] || check.flags != event->args[3]) puts("ACCURACY_CHECK_BAD");
    else puts("ACCURACY_CHECK_OK");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_ACCURACY_CHECKED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 46), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:accuracy_checked", .hook_count = 1, .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 46));
    return sMoves && sMoves->header.struct_size >= RHH_MOVE_SERVICE_V1_46_SIZE ? &sDefinition : NULL;
}
