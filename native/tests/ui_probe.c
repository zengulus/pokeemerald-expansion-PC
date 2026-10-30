/* Independently loaded consumer exercises modal input, drawing, and lifecycle. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhUiService *ui;
static const struct RhhOwnedMonService *mons;
static const struct RhhMonEditService *edit;
static const struct RhhContentDisplayService *display;
static RhhContentKey species;
static RhhContentKey owner;
static int draws;
static int Change(void)
{
    struct RhhMonEditRequest r = {.struct_size = sizeof(r), .fields = RHH_EDIT_IVS,
        .expected = {.struct_size = sizeof(r.expected)}};
    struct RhhMonEditResult out = {.struct_size = sizeof(out)};
    if (mons->party_at(0, &r.expected)) return -1;
    memcpy(r.ivs, r.expected.ivs, sizeof(r.ivs)); r.ivs[0] = 12;
    return edit->commit(&r, &out);
}
static void Draw(const struct RhhUiCanvas *c, void *user)
{
    (void)user; ++draws;
    if (Change() != RHH_EDIT_UNAVAILABLE || !ui->close(owner)) puts("UI_PROBE_BAD_DRAW");
    const struct RhhUiPoint p[] = {{-10, 10}, {20, 10}, {10, 30}};
    c->rect(-20, -20, 24, 24, 1); c->line(-30000, 30, 30000, 30, 2);
    c->polygon(p, 3, 3); c->text(20, 40, "Hello: 123", 4, 5);
    c->text(80, 70, "A", 4, 5); c->text(95, 70, "a", 4, 5);
    c->text(110, 70, ">", 4, 5); c->text(125, 70, " ", 4, 5);
    c->rect(2147483647, 2147483647, 100, 100, 6);
    puts("UI_PROBE_DRAW");
}
static void Input(const struct RhhUiInput *keys, void *user)
{
    (void)user;
    if (keys->pressed & RHH_UI_A) {
        if (Change() != RHH_EDIT_OK) puts("UI_PROBE_BAD_INPUT");
        else puts("UI_PROBE_EDIT");
    }
    if (keys->pressed & RHH_UI_B) ui->close(owner);
}
static void Closed(uint32_t why, void *user)
{
    (void)user;
    if (why == RHH_UI_RESET) puts("UI_PROBE_RESET");
    else if (why == RHH_UI_CLOSED) puts("UI_PROBE_CLOSED");
    else puts("UI_PROBE_BAD_CLOSE");
}
static void Update(const struct RhhModEvent *e, const struct RhhModDecision *d, void *state)
{
    (void)e; (void)d; (void)state;
    char name[32];
    if (display->name(RHH_SPECIES, species, name, sizeof(name)) || strcmp(name, "Torchic")) puts("UI_PROBE_BAD_DISPLAY");
    struct RhhUiInput input = {.struct_size = sizeof(input)};
    if (ui->input(&input)) { puts("UI_PROBE_BAD_SAMPLE"); return; }
    if ((input.held & RHH_UI_SELECT) && (input.pressed & RHH_UI_R)) {
        const struct RhhUiPanel panel = {sizeof(panel), 0, owner, Input, Draw, Closed, NULL};
        if (ui->open(&panel)) puts("UI_PROBE_BAD_OPEN");
    }
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/ui"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/mon_edit"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/owned_mon"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/content_display"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:ui_probe",
    .hook_count = 1, .hooks = hooks, .service_requirement_count = 4, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    ui = (const void *)host->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 0));
    mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    edit = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 0));
    display = (const void *)host->get_service("rhh:service/content_display", RHH_SERVICE_VERSION(1, 0));
    species = host->content_key("rhh:species/torchic");
    owner = host->content_key(definition.id);
    return ui && mons && edit && display && species ? &definition : NULL;
}
