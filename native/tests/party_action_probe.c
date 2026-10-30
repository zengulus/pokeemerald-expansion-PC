/* Independent v1.1 consumer checks party-action collection and read-only opening. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhUiService *ui;
static const struct RhhMonEditService *edits;
static RhhContentKey owner;
static int Change(const struct RhhOwnedMonSnapshot *mon)
{
    struct RhhMonEditRequest request = {.struct_size = sizeof(request), .fields = RHH_EDIT_IVS, .expected = *mon};
    struct RhhMonEditResult result = {.struct_size = sizeof(result)};
    memcpy(request.ivs, mon->ivs, sizeof(request.ivs));
    request.ivs[0] = mon->ivs[0] ? mon->ivs[0] - 1 : 1;
    return edits->commit(&request, &result);
}
static void Draw(const struct RhhUiCanvas *canvas, void *user)
{ (void)user; canvas->text(10, 10, "Party probe", 0, 1); }
static void Input(const struct RhhUiInput *keys, void *user)
{ (void)user; if (keys->pressed & RHH_UI_B) ui->close(owner); }
static int Open(const struct RhhOwnedMonSnapshot *mon, struct RhhUiPanel *panel, void *user)
{
    (void)user;
    if (Change(mon) != RHH_EDIT_UNAVAILABLE) puts("PARTY_PROBE_BAD_OPEN");
    else puts("PARTY_PROBE_OPEN_OK");
    *panel = (struct RhhUiPanel){sizeof(*panel), 0, owner, Input, Draw, NULL, NULL};
    return 0;
}
static void Collect(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
    if (ui->party_context(&mon) || mon.slot != event->args[0] || Change(&mon) != RHH_EDIT_UNAVAILABLE)
    { puts("PARTY_PROBE_BAD_CONTEXT"); return; }
    struct RhhUiPartyAction action = {sizeof(action), 0, owner, "test:party/probe", "Party probe", Open, NULL};
    if (ui->offer_party_action(&action) || !ui->offer_party_action(&action)) puts("PARTY_PROBE_BAD_OFFER");
    else puts("PARTY_PROBE_COLLECT_OK");
    action.id = "invalid id";
    if (!ui->offer_party_action(&action)) puts("PARTY_PROBE_BAD_ID");
    action.id = "unnamespaced";
    if (!ui->offer_party_action(&action)) puts("PARTY_PROBE_BAD_NAMESPACE");
    action.id = "test:party/long"; action.label = "A label too long to fit this menu";
    if (!ui->offer_party_action(&action)) puts("PARTY_PROBE_BAD_LABEL");
    action.id = "test:party/badowner"; action.label = "Invalid owner"; action.owner = 0;
    if (!ui->offer_party_action(&action)) puts("PARTY_PROBE_BAD_OWNER");
    action.owner = owner; action.label = "Additional action";
    for (unsigned i = 0; i < 5; ++i) {
        char id[32]; snprintf(id, sizeof(id), "test:party/extra%u", i); action.id = id;
        if (ui->offer_party_action(&action)) puts("PARTY_PROBE_BAD_EXTRA");
    }
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_PARTY_MENU_ACTIONS, RHH_CONSTRAINT, 0, Collect, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/ui"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/mon_edit"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:party_probe",
    .hook_count = 1, .hooks = hooks, .service_requirement_count = 2, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    ui = (const void *)host->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 1));
    edits = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 0));
    owner = host->content_key(definition.id);
    return ui && edits && ui->header.struct_size >= sizeof(*ui) ? &definition : NULL;
}
