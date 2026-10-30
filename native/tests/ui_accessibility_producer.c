#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhUiService *ui;
static const struct RhhUiAccessibilityService *access;
static const struct RhhModArchiveService *archive;
static RhhContentKey owner, otherOwner, archiveKey;
static unsigned page, overlay;
static int WriteArchive(RhhContentKey target)
{
    struct RhhModArchiveRecord record = {.struct_size = sizeof(record)};
    uint64_t expected = archive->find(target, archiveKey, &record) ? 0 : record.revision;
    return archive->put(target, archiveKey, expected, "x", 1, &record);
}
static void Draw(const struct RhhUiCanvas *canvas, void *user)
{
    (void)canvas; (void)user;
    if (!WriteArchive(owner)) puts("A11Y_PRODUCER_BAD_DRAW_ARCHIVE");
    struct RhhUiAccessibleContent content = {.struct_size = sizeof(content)};
    strcpy(content.title, "Pok\xc3\xa9mon stats");
    strcpy(content.body, "HP 31\nAttack 12");
    strcpy(content.focus, page ? "Attack" : "HP");
    struct RhhUiAccessibleContent bad = content;
    bad.body[0] = (char)0xc0; bad.body[1] = (char)0x80;
    if (!access->publish(owner, &bad) || !access->publish(owner + 1, &content)) puts("A11Y_PRODUCER_BAD");
    memset(bad.body, 'x', sizeof(bad.body));
    if (!access->publish(owner, &bad)) puts("A11Y_PRODUCER_BAD");
    if (access->publish(owner, &content)) puts("A11Y_PRODUCER_BAD");
    memset(&content, 0, sizeof(content)); /* The host must own its copy. */
}
static void Input(const struct RhhUiInput *input, void *user)
{
    (void)user;
    if (input->pressed & RHH_UI_A) {
        page ^= 1;
        if (WriteArchive(owner) || !WriteArchive(otherOwner)) puts("A11Y_PRODUCER_BAD_INPUT_ARCHIVE");
        else puts(overlay ? "A11Y_OVERLAY_ARCHIVE_OK" : "A11Y_MODAL_ARCHIVE_OK");
    }
    if (input->pressed & RHH_UI_B) {
        if (overlay) ui->overlay_close(owner); else ui->close(owner);
        if (!WriteArchive(owner)) puts("A11Y_PRODUCER_BAD_AFTER_CLOSE");
    }
}
static void Closed(uint32_t reason, void *user)
{ (void)reason; (void)user; if (!WriteArchive(owner)) puts("A11Y_PRODUCER_BAD_CLOSED_ARCHIVE"); }
static void SpoofDraw(const struct RhhUiCanvas *canvas, void *user) { (void)canvas; (void)user; }
static void SpoofInput(const struct RhhUiInput *input, void *user)
{
    (void)user;
    if (input->pressed & RHH_UI_A)
    {
        if (!WriteArchive(otherOwner) || !WriteArchive(owner)) puts("A11Y_PRODUCER_BAD_SPOOF");
        else puts("A11Y_SPOOF_ARCHIVE_BLOCKED");
    }
    if (input->pressed & RHH_UI_B) ui->close(otherOwner);
}
static int PartyOpen(const struct RhhOwnedMonSnapshot *mon, struct RhhUiPanel *panel, void *user)
{
    (void)mon;
    int spoof = user != NULL;
    *panel = (struct RhhUiPanel){.struct_size = sizeof(*panel), .owner = spoof ? otherOwner : owner,
        .input = spoof ? SpoofInput : Input, .draw = spoof ? SpoofDraw : Draw, .closed = spoof ? NULL : Closed};
    overlay = 0; return 0;
}
static void PartyCollect(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhUiPartyAction action = {.struct_size = sizeof(action), .owner = owner,
        .id = "test:a11y/a_owner", .label = "Owner", .open = PartyOpen};
    if (ui->offer_party_action(&action)) puts("A11Y_PRODUCER_BAD_PARTY");
    action.owner = otherOwner; action.id = "test:a11y/z_spoof"; action.label = "Spoof"; action.user = &page;
    if (ui->offer_party_action(&action)) puts("A11Y_PRODUCER_BAD_PARTY");
}
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    overlay = !!event->args[0];
    if (event->args[1]) {
        struct RhhUiPanel fake = {.struct_size = sizeof(fake), .owner = otherOwner, .draw = SpoofDraw, .input = SpoofInput};
        if (!ui->active() && ui->open(&fake)) puts("A11Y_PRODUCER_BAD_SPOOF_OPEN");
        return;
    }
    if (overlay) {
        struct RhhUiOverlay view = {.struct_size = sizeof(view), .owner = owner, .draw = Draw, .input = Input, .closed = Closed};
        if (!ui->overlay_active() && ui->overlay_open(&view)) puts("A11Y_PRODUCER_BAD");
        return;
    }
    struct RhhUiPanel panel = {.struct_size = sizeof(panel), .owner = owner, .draw = Draw, .input = Input, .closed = Closed};
    if (!ui->active() && ui->open(&panel)) puts("A11Y_PRODUCER_BAD");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update},
    {sizeof(struct RhhModHook), RHH_PARTY_MENU_ACTIONS, RHH_CONSTRAINT, 0, PartyCollect, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/ui_accessibility"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/mod_archive"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:a11y_producer", .hooks = hooks, .hook_count = 2, .service_requirements = requirements, .service_requirement_count = 2};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    ui = (const void *)host->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 3));
    access = (const void *)host->get_service("rhh:service/ui_accessibility", RHH_SERVICE_VERSION(1, 0));
    archive = (const void *)host->get_service("rhh:service/mod_archive", RHH_SERVICE_VERSION(1, 2));
    owner = host->content_key(definition.id); otherOwner = host->content_key("test:a11y_rng");
    archiveKey = host->content_key("test:a11y/archive"); return ui && access && archive && owner && otherOwner && archiveKey ? &definition : NULL;
}
