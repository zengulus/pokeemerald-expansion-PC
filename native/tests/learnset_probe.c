#include "mod_api.h"
static struct RhhModMove sMove;
static struct RhhModLearnset sEntries[6];
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_LEARNSETS, .id = "test:learnsets",
    .move_count = 1, .moves = &sMove, .learnset_count = 6, .learnsets = sEntries
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->content_key) return NULL;
    RhhContentKey torchic = host->content_key("rhh:species/torchic");
    RhhContentKey surge = host->content_key("test:move/surge"), surf = host->content_key("rhh:move/surf");
    sMove = (struct RhhModMove){.struct_size = sizeof(sMove), .power = 60, .accuracy = 100, .pp = 20,
        .category = RHH_MOVE_SPECIAL, .target = RHH_MOVE_TARGET_SELECTED,
        .template_move = host->content_key("rhh:move/water_gun"), .type = host->content_key("rhh:type/water"),
        .id = "test:move/surge", .name = "Surge", .description = "A surge of water."};
    sEntries[0] = (struct RhhModLearnset){sizeof(sEntries[0]), RHH_LEARNSET_LEVEL, 50, 0, torchic, surge};
    sEntries[1] = (struct RhhModLearnset){sizeof(sEntries[1]), RHH_LEARNSET_LEVEL, 50, 0, torchic, surf};
    sEntries[2] = sEntries[0]; /* Duplicate declarations must not duplicate the engine entry. */
    sEntries[3] = (struct RhhModLearnset){sizeof(sEntries[3]), RHH_LEARNSET_TEACHABLE, 0, 0, torchic, surge};
    sEntries[4] = (struct RhhModLearnset){sizeof(sEntries[4]), RHH_LEARNSET_EGG, 0, 0, torchic, surge};
    sEntries[5] = (struct RhhModLearnset){sizeof(sEntries[5]), RHH_LEARNSET_LEVEL, 0, 0, torchic, surf};
    return &sDefinition;
}
