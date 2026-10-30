#include "mod_api.h"
#include <stdio.h>
static const struct RhhUiService *ui;
static RhhContentKey owner;
static void Draw(const struct RhhUiCanvas *canvas, void *user) { (void)canvas; (void)user; }
static void Closed(uint32_t reason, void *user) { (void)user; printf("MUSIC_PREVIEW_PANEL_CLOSED %u\n", reason); }
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhUiPanel panel = {sizeof(panel), 0, owner, NULL, Draw, Closed, NULL};
    if (!ui->active() && ui->open(&panel)) puts("MUSIC_PREVIEW_BAD_PANEL");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/ui"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/music_preview"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:music_preview_probe", .hook_count = 1, .hooks = hooks,
    .service_requirement_count = 2, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    ui = (const void *)host->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 0));
    owner = host->content_key(definition.id); return ui && owner ? &definition : NULL;
}
