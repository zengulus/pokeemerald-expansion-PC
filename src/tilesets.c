#include "global.h"
#include "tilesets.h"
#include "tileset_anims.h"

#include "data/tilesets/graphics.h"
#include "data/tilesets/metatiles.h"
#include "data/tilesets/headers.h"

#ifdef NATIVE
/* Only return sizes known at compile time. Unknown future raw assets fail
 * closed rather than reading past their source array. */
u32 RhhModsRawTilesetSize(const struct Tileset *tileset)
{
    if (!tileset || !tileset->tiles) return 0;
#define RAW_TILESET_SIZE(symbol) if (tileset->tiles == (symbol)) return sizeof(symbol)
    RAW_TILESET_SIZE(gTilesetTiles_SecretBase);
    RAW_TILESET_SIZE(gTilesetTiles_SecretBaseRedCave);
#if IS_FRLG
    RAW_TILESET_SIZE(gTilesetTiles_CableClub_Frlg);
#else
    RAW_TILESET_SIZE(gTilesetTiles_CableClub);
    RAW_TILESET_SIZE(gTilesetTiles_SecretBaseBrownCave);
    RAW_TILESET_SIZE(gTilesetTiles_SecretBaseTree);
    RAW_TILESET_SIZE(gTilesetTiles_SecretBaseShrub);
    RAW_TILESET_SIZE(gTilesetTiles_SecretBaseBlueCave);
    RAW_TILESET_SIZE(gTilesetTiles_SecretBaseYellowCave);
#endif
#undef RAW_TILESET_SIZE
    return 0;
}
#endif
