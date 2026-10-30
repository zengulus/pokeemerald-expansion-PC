#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhMusicAssignmentService *service;
static const struct RhhMusicLevelService *levels;
static void Permission(const struct RhhModEvent *e, const void *state, struct RhhModDecision *d)
{
    (void)state;
    struct RhhMusicAssignmentResult r = {.struct_size = sizeof(r)};
    struct RhhMusicAssignmentResult small = {.struct_size = sizeof(small) - 1}, old = small;
    if (service->context(&r) || !service->context(&small) || memcmp(&small, &old, sizeof(small))
        || !r.changed || r.before_revision != e->ordinal || r.is_preview != e->args[1]
        || r.requester != e->subject) { puts("MUSIC_ASSIGN_BAD_CONTEXT"); return; }
    struct RhhMusicAssignmentRequest nested = {.struct_size = sizeof(nested), .action = RHH_MUSIC_ASSIGN_CLEAR,
        .requester = e->subject, .expected_revision = r.before_revision,
        .rule = {.struct_size = sizeof(nested.rule), .context = RHH_MUSIC_ANY}};
    struct RhhMusicAssignmentResult rejected = {.struct_size = sizeof(rejected)};
    if (service->apply(&nested, &rejected) != RHH_MUSIC_ASSIGN_UNAVAILABLE) puts("MUSIC_ASSIGN_BAD_RECURSION");
    if (e->args[2] == RHH_MUSIC_GYM) { d->allowed = 0; strcpy(d->reason, "test denied"); puts("MUSIC_ASSIGN_DENIED"); }
}
static void Changed(const struct RhhModEvent *e, const struct RhhModDecision *d, void *state)
{
    (void)d; (void)state;
    struct RhhMusicAssignmentResult r = {.struct_size = sizeof(r)};
    if (service->context(&r) || r.is_preview || !r.changed || r.status != RHH_MUSIC_ASSIGN_OK
        || r.after_revision != e->ordinal || service->revision() != e->ordinal) puts("MUSIC_ASSIGN_BAD_COMMIT");
    else puts("MUSIC_ASSIGN_CHANGED");
}
static void LevelPermission(const struct RhhModEvent *e, const void *state, struct RhhModDecision *d)
{
    (void)state;
    struct RhhMusicLevelResult r = {.struct_size = sizeof(r)};
    if (levels->context(&r) || r.track != e->event_key || r.requester != e->subject
        || r.before_revision != e->ordinal || r.after_gain != e->args[0] || r.is_preview != e->args[1])
    { puts("MUSIC_ASSIGN_BAD_LEVEL_CONTEXT"); return; }
    struct RhhMusicLevelRequest nested = {.struct_size = sizeof(nested), .requester = e->subject,
        .track = r.track, .gain = 500, .expected_revision = r.before_revision};
    struct RhhMusicLevelResult rejected = {.struct_size = sizeof(rejected)};
    if (levels->apply(&nested, &rejected) != RHH_MUSIC_LEVEL_UNAVAILABLE) puts("MUSIC_ASSIGN_BAD_LEVEL_RECURSION");
    if (e->args[0] == 2200) { d->allowed = 0; strcpy(d->reason, "level denied"); }
}
static void LevelChanged(const struct RhhModEvent *e, const struct RhhModDecision *d, void *state)
{
    (void)d; (void)state;
    struct RhhMusicLevelResult r = {.struct_size = sizeof(r)}; uint32_t gain = 0;
    if (levels->context(&r) || r.is_preview || !r.changed || r.after_revision != e->ordinal
        || levels->get(r.track, &gain) || gain != r.after_gain) puts("MUSIC_ASSIGN_BAD_LEVEL_COMMIT");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_SET_MUSIC_ASSIGNMENT, RHH_CONSTRAINT, 0, Permission, NULL},
    {sizeof(struct RhhModHook), RHH_MUSIC_ASSIGNMENT_CHANGED, RHH_OBSERVE, 0, NULL, Changed},
    {sizeof(struct RhhModHook), RHH_CAN_SET_MUSIC_LEVEL, RHH_CONSTRAINT, 0, LevelPermission, NULL},
    {sizeof(struct RhhModHook), RHH_MUSIC_LEVEL_CHANGED, RHH_OBSERVE, 0, NULL, LevelChanged}
};
static const struct RhhServiceRequirement req[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/music_assignments"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:music_assign_probe", .hooks = hooks, .hook_count = 4, .service_requirements = req, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    service = (const void *)host->get_service("rhh:service/music_assignments", RHH_SERVICE_VERSION(1, 0));
    levels = (const void *)host->get_service("rhh:service/music_levels", RHH_SERVICE_VERSION(1, 0));
    return service && service->header.struct_size >= sizeof(*service) && levels && levels->header.struct_size >= sizeof(*levels) ? &definition : NULL;
}
