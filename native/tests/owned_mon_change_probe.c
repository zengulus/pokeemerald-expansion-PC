#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhOwnedMonService *mons;
static const struct RhhMonEditService *edit;
static const struct RhhUiService *ui;
static RhhContentKey owner;
static void Draw(const struct RhhUiCanvas *canvas, void *user) { (void)canvas; (void)user; }
static void ModalInput(const struct RhhUiInput *input, void *user)
{
    (void)input; (void)user;
    struct RhhMonEditRequest request = {.struct_size = sizeof(request), .fields = RHH_EDIT_IVS,
        .expected.struct_size = sizeof(request.expected)};
    struct RhhMonEditResult result = {.struct_size = sizeof(result)};
    if (mons->party_at(0, &request.expected)) { puts("OWNED_WATCH_BAD_MODAL"); return; }
    memcpy(request.ivs, request.expected.ivs, sizeof(request.ivs)); request.ivs[1] = 20;
    if (edit->commit(&request, &result) != RHH_EDIT_OK || ui->close(owner)) puts("OWNED_WATCH_BAD_MODAL");
    else puts("OWNED_WATCH_MODAL_COMMITTED");
}
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (event->args[0] != 77) return;
    struct RhhUiPanel panel = {sizeof(panel), 0, owner, ModalInput, Draw, NULL, NULL};
    if (ui->open(&panel)) puts("OWNED_WATCH_BAD_MODAL_OPEN");
}
static void Changed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhOwnedMonChange change = {.struct_size = sizeof(change)};
    struct RhhOwnedMonChange small = {.struct_size = sizeof(small) - 1, .flags = 999}, saved = small;
    struct RhhOwnedExperienceChange xp = {.struct_size = sizeof(xp), .before_exp = 99};
    struct RhhOwnedExperienceChange smallXp = {.struct_size = sizeof(smallXp) - 1, .before_exp = 99}, savedXp = smallXp;
    if (mons->change_context(&change) || !mons->change_context(&small) || memcmp(&small, &saved, sizeof(small))
        || change.sequence != event->ordinal || change.flags != event->args[0]
        || change.location != event->args[1] || change.box != event->args[2] || change.slot != event->args[3])
    { puts("OWNED_WATCH_BAD_CONTEXT"); return; }
    if (change.flags == RHH_OWNED_CHANGE_RESET)
    {
        if (event->event_key || event->subject || change.fields || change.location
            || !mons->experience_change_context(&xp) || xp.before_exp != 99) puts("OWNED_WATCH_BAD_RESET");
        printf("OWNED_WATCH_RESET %llu\n", (unsigned long long)change.run_id);
        return;
    }
    if (mons->experience_change_context(&xp) || !mons->experience_change_context(&smallXp)
        || memcmp(&smallXp, &savedXp, sizeof(smallXp)) || xp.flags != change.flags
        || xp.location != change.location || xp.box != change.box || xp.slot != change.slot
        || xp.run_id != change.run_id || xp.sequence != change.sequence
        || (!(xp.flags & RHH_OWNED_CHANGE_BEFORE) && xp.before_exp)
        || (!(xp.flags & RHH_OWNED_CHANGE_AFTER) && xp.after_exp)
        || (xp.flags == (RHH_OWNED_CHANGE_BEFORE | RHH_OWNED_CHANGE_AFTER)
            && !!(change.fields & RHH_OWNED_CHANGED_EXPERIENCE) != (xp.before_exp != xp.after_exp)))
        puts("OWNED_WATCH_BAD_XP_CONTEXT");
    const struct RhhOwnedMonSnapshot *subject = change.flags & RHH_OWNED_CHANGE_AFTER ? &change.after : &change.before;
    if (event->event_key != subject->species || event->subject != subject->identity
        || !subject->identity || subject->location != change.location || subject->slot != change.slot
        || subject->box != change.box) puts("OWNED_WATCH_BAD_SUBJECT");
    if ((change.flags & RHH_OWNED_CHANGE_AFTER) && (change.fields & RHH_OWNED_CHANGED_EXPERIENCE))
    {
        struct RhhOwnedExperience view = {.struct_size = sizeof(view)};
        if (mons->experience(subject, &view) || view.mon.identity != subject->identity || view.experience != xp.after_exp
            || view.experience < view.level_start_exp || view.experience > view.maximum_exp)
            puts("OWNED_WATCH_BAD_EXPERIENCE");
    }
    if ((change.flags & RHH_OWNED_CHANGE_AFTER) && (change.fields & RHH_OWNED_CHANGED_TERA))
    {
        struct RhhOwnedTeraView view = {.struct_size = sizeof(view)};
        if (mons->tera_view(subject, &view) || view.mon.identity != subject->identity
            || view.source < RHH_OWNED_TERA_PERSONALITY || view.source > RHH_OWNED_TERA_FORCED)
            puts("OWNED_WATCH_BAD_TERA_VIEW");
        else puts("OWNED_WATCH_TERA_VIEW");
    }
    if (change.fields == RHH_OWNED_CHANGED_EXPERIENCE)
        printf("OWNED_XP_DELTA %u %u\n", change.location, xp.after_exp - xp.before_exp);
    printf("OWNED_WATCH %u %u %u %u %u %u %u\n", change.flags, change.fields, change.location,
           change.box, change.slot, change.before.level, change.after.level);
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_OWNED_MON_CHANGED, RHH_OBSERVE, 0, NULL, Changed},
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 41), RHH_SERVICE_REQUIRED, 0, "rhh:service/owned_mon"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/mon_edit"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/ui"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:owned_mon_changes", .hook_count = 2, .hooks = hooks,
    .service_requirement_count = 3, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 41));
    edit = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 0));
    ui = (const void *)host->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 0));
    owner = host->content_key(definition.id);
    return mons && edit && ui && owner ? &definition : NULL;
}
