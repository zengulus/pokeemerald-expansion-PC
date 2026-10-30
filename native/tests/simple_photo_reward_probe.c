#include "mod_api.h"
#include <stdint.h>

static const struct RhhPhotoAlbumService *sAlbum;
static const struct RhhSimplePhotoRewardService *sRewards;
static RhhContentKey sOwner, sRule, sItem;
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *memory)
{
    (void)event; (void)decision;
    uint8_t *state = memory;
    if (!state[0]) return;
    struct RhhPhotoAlbumEntry photo = {.struct_size = sizeof(photo)};
    if (sAlbum->at(0, &photo)) { state[1] = 255; return; }
    struct RhhPhotoRewardRequest request = {.struct_size = sizeof(request), .quantity = 2,
        .owner = sOwner, .rule = sRule, .item = sItem, .photo = photo.id};
    state[1] = sRewards->claim(&request);
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:simple_photo_reward",
    .state_size = 2, .state_schema = 1, .hook_count = 1, .hooks = sHooks
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sAlbum = (const void *)host->get_service("rhh:service/photo_album", RHH_SERVICE_VERSION(1, 0));
    sRewards = (const void *)host->get_service("rhh:service/photo_reward_simple", RHH_SERVICE_VERSION(1, 0));
    sOwner = host->content_key("test:simple_photo_reward");
    sRule = host->content_key("test:reward/species_at_location");
    sItem = host->content_key("rhh:item/potion");
    return sAlbum && sRewards ? &sDefinition : NULL;
}
