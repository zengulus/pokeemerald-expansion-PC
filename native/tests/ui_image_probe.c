#include "mod_api.h"
#include <stdio.h>
static const struct RhhUiService *sUi;
static const struct RhhContentDisplayService *sContent;
static RhhContentKey sOwner;
static RhhContentKey sItem, sSpecies;
static const struct RhhUiCanvas *sCanvas;
static const uint8_t sPixels[16] = {255,0,0, 0,255,0, 42,42, 0,0,255, 255,255,255, 42,42};
static const struct RhhUiRgbImage sImage = {sizeof(sImage), 2, 2, 8, sizeof(sPixels), sPixels};
static const uint8_t sRgbaPixels[] = {255,0,0,0, 0,255,0,127, 0,0,255,128, 255,255,255,255};
static const struct RhhUiRgbaImage sRgba = {sizeof(sRgba), 2, 2, 8, sizeof(sRgbaPixels), 128, sRgbaPixels};
static void Draw(const struct RhhUiCanvas *canvas, void *user)
{
    (void)user; sCanvas = canvas;
    struct RhhUiRgbImage invalid = sImage;
    if (canvas->struct_size < sizeof(*canvas) || !canvas->image_rgb
        || canvas->image_rgb(-1, -1, 4, 4, &sImage)) { puts("UI_IMAGE_BAD"); return; }
    uint8_t icon[24 * 24 * 4];
    struct RhhUiRgbaImage item = {sizeof(item), 24, 24, 96, sizeof(icon), 1, icon};
    struct RhhUiRgbaImage invalidRgba = sRgba;
    if (!canvas->image_rgba || sContent->item_icon_rgba(sItem, icon, sizeof(icon))
        || canvas->image_rgba(20, 20, 4, 4, &sRgba) || canvas->image_rgba(40, 40, 24, 24, &item)) puts("UI_IMAGE_BAD");
    struct RhhPokemonIconRequest request = {.struct_size = sizeof(request), .species = sSpecies};
    struct RhhPokemonIcon info = {.struct_size = sizeof(info)};
    uint8_t monPixels[32 * 32 * 4];
    struct RhhUiRgbaImage mon = {sizeof(mon), 32, 32, 128, sizeof(monPixels), 1, monPixels};
    if (sContent->pokemon_icon_info(&request, &info) || info.width != 32 || info.height != 32 || info.frame_count < 2
        || sContent->pokemon_icon_rgba(&request, 1, monPixels, sizeof(monPixels))
        || canvas->image_rgba(75, 40, 32, 32, &mon)) puts("UI_IMAGE_BAD");
    invalidRgba.alpha_threshold = 0;
    if (!canvas->image_rgba(10, 10, 2, 2, &invalidRgba)) puts("UI_IMAGE_BAD");
    invalidRgba = sRgba; invalidRgba.size = sizeof(sRgbaPixels) - 1;
    if (!canvas->image_rgba(10, 10, 2, 2, &invalidRgba)
        || !canvas->image_rgba(10, 10, 2, 2, NULL)
        || canvas->image_rgba(INT32_MIN, INT32_MAX, 2, 2, &sRgba)) puts("UI_IMAGE_BAD");
    invalid.size = 13;
    if (!canvas->image_rgb(10, 10, 2, 2, &invalid)) puts("UI_IMAGE_BAD");
    invalid = sImage; invalid.stride = UINT32_MAX; invalid.height = 4096;
    if (!canvas->image_rgb(10, 10, 2, 2, &invalid)
        || !canvas->image_rgb(10, 10, 0, 2, &sImage)
        || !canvas->image_rgb(10, 10, 2, 40000, &sImage)
        || canvas->image_rgb(INT32_MIN, INT32_MAX, 2, 2, &sImage)) puts("UI_IMAGE_BAD");
    puts("UI_IMAGE_DRAW");
}
static void Input(const struct RhhUiInput *input, void *user)
{
    (void)user;
    if (input->pressed & RHH_UI_B)
    {
        if (!sCanvas || !sCanvas->image_rgb(0, 0, 2, 2, &sImage)
            || !sCanvas->image_rgba(0, 0, 2, 2, &sRgba) || sUi->close(sOwner)) puts("UI_IMAGE_BAD");
        else puts("UI_IMAGE_CLOSED");
    }
}
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    const struct RhhUiPanel panel = {sizeof(panel), 0, sOwner, Input, Draw, NULL, NULL};
    if (sUi->open(&panel)) puts("UI_IMAGE_BAD");
}
static const struct RhhModHook sHooks[] = {{sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update}};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 5), RHH_SERVICE_REQUIRED, 0, "rhh:service/ui"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/content_display"}
};
static const struct RhhModDefinition sMod = {.struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:ui_image", .hook_count = 1, .hooks = sHooks, .service_requirement_count = 2, .service_requirements = sRequirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sOwner = host->content_key(sMod.id);
    sUi = (const void *)host->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 5));
    sContent = (const void *)host->get_service("rhh:service/content_display", RHH_SERVICE_VERSION(1, 2));
    sItem = host->content_key("rhh:item/poke_ball");
    sSpecies = host->content_key("rhh:species/torchic");
    return sUi && sContent && sOwner && sItem && sSpecies ? &sMod : NULL;
}
