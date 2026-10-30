#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhBreedingService *breeding;
static struct RhhBreedingOffer expected;
static int inside;
static unsigned nestedCalls;
static void Learnset(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (!inside) return;
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer), .pending = 99}, original = offer;
    struct RhhBreedingPreview base = {.struct_size = sizeof(base)}, savedBase = base;
    struct RhhBreedingMovePreview preview = {.struct_size = sizeof(preview), .flags = 99}, saved = preview;
    if (!breeding->offer(&offer) || memcmp(&offer, &original, sizeof(offer))
        || !breeding->preview(&base) || memcmp(&base, &savedBase, sizeof(base))
        || !breeding->move_preview(&expected, &preview) || memcmp(&preview, &saved, sizeof(preview)))
        puts("BREED_MOVES_BAD_NESTED");
    ++nestedCalls;
}
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (event->args[0] != 73) return;
    expected = (struct RhhBreedingOffer){.struct_size = sizeof(expected)};
    if (breeding->offer(&expected)) { puts("BREED_MOVES_BAD_OFFER"); return; }
    struct RhhBreedingMovePreview preview = {.struct_size = sizeof(preview)};
    inside = 1; nestedCalls = 0; int status = breeding->move_preview(&expected, &preview); inside = 0;
    if (status || !preview.candidate_count || !nestedCalls
        || !!(preview.flags & RHH_BREED_MOVE_PREVIEW_HOOKS_ACTIVE) != !!event->args[1])
        puts("BREED_MOVES_BAD_PREVIEW");
    else puts("BREED_MOVES_EXTERNAL_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update},
    {sizeof(struct RhhModHook), RHH_LEARNSET_ENTRY, RHH_TRANSFORM, 0, Learnset, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 8), RHH_SERVICE_REQUIRED, 0, "rhh:service/breeding"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_moves", .hooks = hooks, .hook_count = 2, .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    breeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 8));
    return breeding && breeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_8_SIZE ? &definition : NULL;
}
