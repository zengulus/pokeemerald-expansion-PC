#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhContentDisplayService *display;
static struct RhhModMove move;
static const struct RhhModAbility ability = {sizeof(ability), 0, {0, 0},
    "test:ability/description_probe", "Description", "Ability line one.\nAbility line two!"};
static RhhContentKey moveKey, abilityKey;
static void CheckDescriptions(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    char text[256];
    if (display->description(RHH_MOVE, moveKey, text, sizeof(text)) || strcmp(text, "Move line one.\nMove line two!"))
        puts("CONTENT_DESCRIPTION_BAD_CUSTOM_MOVE");
    if (display->description(RHH_ABILITY, abilityKey, text, sizeof(text)) || strcmp(text, "Ability line one.\nAbility line two!"))
        puts("CONTENT_DESCRIPTION_BAD_CUSTOM_ABILITY");
    if (display->description(RHH_MOVE, moveKey, text, strlen("Move line one.\nMove line two!") + 1)
        || strcmp(text, "Move line one.\nMove line two!")) puts("CONTENT_DESCRIPTION_BAD_CUSTOM_CAPACITY");
    strcpy(text, "unchanged");
    if (!display->description(RHH_MOVE, moveKey, text, 4) || strcmp(text, "unchanged")
        || !display->description(RHH_ABILITY, moveKey, text, sizeof(text)) || strcmp(text, "unchanged"))
        puts("CONTENT_DESCRIPTION_BAD_FAILURE");
    puts("CONTENT_DESCRIPTION_EXTERNAL_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, CheckDescriptions}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 3), RHH_SERVICE_REQUIRED, 0, "rhh:service/content_display"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_ABILITIES,
    .id = "test:content_descriptions", .hook_count = 1, .hooks = hooks, .move_count = 1, .moves = &move,
    .ability_count = 1, .abilities = &ability, .service_requirement_count = 1, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    display = (const void *)host->get_service("rhh:service/content_display", RHH_SERVICE_VERSION(1, 3));
    moveKey = host->content_key("test:move/description_probe");
    abilityKey = host->content_key("test:ability/description_probe");
    move = (struct RhhModMove){.struct_size = sizeof(move), .power = 50, .accuracy = 100, .pp = 20,
        .category = RHH_MOVE_PHYSICAL, .target = RHH_MOVE_TARGET_SELECTED,
        .template_move = host->content_key("rhh:move/tackle"), .type = host->content_key("rhh:type/normal"),
        .id = "test:move/description_probe", .name = "Probe", .description = "Move line one.\nMove line two!"};
    return display && display->header.struct_size >= RHH_CONTENT_DISPLAY_SERVICE_V1_3_SIZE
        && moveKey && abilityKey && move.template_move && move.type ? &definition : NULL;
}
