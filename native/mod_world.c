/* Read-only overworld views for native presentation and gameplay modules. */
#include <stdlib.h>
#include "global.h"
#include "main.h"
#include "overworld.h"
#include "script.h"
#include "palette.h"
#include "fieldmap.h"
#include "battle.h"
#include "battle_controllers.h"
#include "event_data.h"
#include "event_scripts.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "sprite.h"
#include "pokemon.h"
#include "berry.h"
#include "decompress.h"
#include "item.h"
#include "item_menu.h"
#include "mods.h"
#include "wild_encounter_ow.h"
#include "wild_encounter.h"
#include "dexnav.h"
#include "safari_zone.h"
#include "metatile_behavior.h"
#include "field_weather.h"
#include "field_effect_helpers.h"
#include "field_effect.h"
#include "sound.h"
#include "constants/songs.h"
#include "constants/map_types.h"
#include "constants/metatile_behaviors.h"
#include "constants/event_objects.h"
#include "constants/event_bg.h"
#include "constants/weather.h"
#include "constants/vars.h"
#include "constants/berry.h"
#include "constants/flags.h"
#include "constants/field_effects.h"
#include "constants/trainer_types.h"
#include "constants/map_groups.h"
#include "constants/maps.h"
#include "constants/mauville_old_man.h"
#include "../src/data/map_group_count.h"
#include "mod_host.h"
#include "mod_ui.h"
#include "battle_shader.h"
#include "native.h"
#include "scene_ray.h"
#include "photo_visibility.h"
#include "display_capture.h"

static const u8 sSizes[3][4][2] = {
    {{8,8},{16,16},{32,32},{64,64}},
    {{16,8},{32,8},{32,16},{64,32}},
    {{8,16},{8,32},{16,32},{32,64}}
};
static const struct RhhHabitatSample *sHabitatContext;
static u32 NaturalWeather(u32 weather);
extern const struct MapHeader *const *const gMapGroups[];
_Static_assert(ARRAY_COUNT(MAP_GROUP_COUNT) > MAP_GROUPS_COUNT, "map group counts must cover every group");
static int Ready(void)
{
    return !gMain.inBattle && gSaveBlock1Ptr && gMapHeader.mapLayout
        && gMapHeader.mapLayout->width > 0 && gMapHeader.mapLayout->height > 0
        && gBackupMapLayout.map && gMapHeader.mapLayout->primaryTileset
        && gMapHeader.mapLayout->secondaryTileset
        && gMapHeader.mapLayout->primaryTileset->metatileAttributes
        && gMapHeader.mapLayout->secondaryTileset->metatileAttributes
        && gBackupMapLayout.width >= gMapHeader.mapLayout->width + MAP_OFFSET_W
        && gBackupMapLayout.height >= gMapHeader.mapLayout->height + MAP_OFFSET_H;
}
static int Current(struct RhhWorldSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out) || !Ready()) return -1;
    struct RhhWorldSnapshot view = {
        .struct_size = sizeof(view), .width = gMapHeader.mapLayout->width,
        .height = gMapHeader.mapLayout->height, .map_type = gMapHeader.mapType,
        .map = RhhContentKeyForId(RHH_MAP, (gSaveBlock1Ptr->location.mapGroup << 8)
            | (u8)gSaveBlock1Ptr->location.mapNum)
    };
    *out = view; return 0;
}
static int DisplayState(struct RhhWorldDisplayState *out)
{
    if (!out || out->struct_size < sizeof(*out) || !Ready()) return -1;
    u32 blend = REG_BLDCNT, alpha = REG_BLDALPHA, brightness = REG_BLDY, mosaic = REG_MOSAIC;
    struct RhhWorldDisplayState view = {.struct_size = sizeof(view), .display_control = REG_DISPCNT,
        .map = RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8)
                                   | (u8)gSaveBlock1Ptr->location.mapNum),
        .bg_control = {REG_BG0CNT, REG_BG1CNT, REG_BG2CNT, REG_BG3CNT},
        .blend_control = blend, .blend_alpha = alpha, .blend_y = brightness, .mosaic = mosaic,
        .win0_h = REG_WIN0H, .win0_v = REG_WIN0V, .win1_h = REG_WIN1H, .win1_v = REG_WIN1V,
        .win_in = REG_WININ, .win_out = REG_WINOUT,
        .blend_mode = (blend >> 6) & 3, .target1 = blend & 63, .target2 = (blend >> 8) & 63,
        .alpha_a = alpha & 31, .alpha_b = (alpha >> 8) & 31, .brightness = brightness & 31,
        .bg_mosaic_width = (mosaic & 15) + 1, .bg_mosaic_height = ((mosaic >> 4) & 15) + 1,
        .obj_mosaic_width = ((mosaic >> 8) & 15) + 1, .obj_mosaic_height = ((mosaic >> 12) & 15) + 1};
    *out = view; return 0;
}
static int DisplayFrame(struct RhhWorldDisplayFrame *out)
{
    if (!out || out->struct_size < sizeof(*out) || !Ready()) return -1;
    struct RhhWorldDisplayFrame result = {.struct_size = sizeof(result), .count = RHH_WORLD_DISPLAY_SCANLINES};
    u32 map;
    if (NativeDisplayFrameCopy(result.rows, RHH_WORLD_DISPLAY_SCANLINES, &map, &result.sequence, &result.run_id)
        || result.run_id != RhhModsRunId()
        || map != (((u8)gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum)) return -1;
    result.map = RhhContentKeyForId(RHH_MAP, map);
    if (!result.map) return -1;
    *out = result; return 0;
}
static int TileAtInternal(int32_t x, int32_t y, struct RhhWorldTile *out, bool32 window)
{
    if (!out || out->struct_size < sizeof(*out) || !Ready()) return -1;
    if (window)
    {
        if (x < -MAP_OFFSET || y < -MAP_OFFSET || x >= gBackupMapLayout.width - MAP_OFFSET
            || y >= gBackupMapLayout.height - MAP_OFFSET) return -1;
    }
    else if (x < 0 || y < 0 || x >= gMapHeader.mapLayout->width || y >= gMapHeader.mapLayout->height) return -1;
    u16 block = gBackupMapLayout.map[(y + MAP_OFFSET) * gBackupMapLayout.width + x + MAP_OFFSET];
    if (block == MAPGRID_UNDEFINED && (!gMapHeader.mapLayout->border
        || (gMapHeader.mapLayout->isFrlg && (!gMapHeader.mapLayout->borderWidth || !gMapHeader.mapLayout->borderHeight))))
        return -1;
    x += MAP_OFFSET; y += MAP_OFFSET;
    u32 metatile = MapGridGetMetatileIdAt(x, y);
    if (metatile >= NUM_METATILES_TOTAL) return -1;
    struct RhhWorldTile tile = {
        .struct_size = sizeof(tile), .metatile = metatile,
        .behavior = MapGridGetMetatileBehaviorAt(x, y), .elevation = MapGridGetElevationAt(x, y),
        .collision = MapGridGetCollisionAt(x, y), .layer = MapGridGetMetatileLayerTypeAt(x, y)
    };
    unsigned firstSecondary = GetNumMetatilesInPrimary(gMapHeader.mapLayout);
    const struct Tileset *tileset = tile.metatile < firstSecondary
        ? gMapHeader.mapLayout->primaryTileset : gMapHeader.mapLayout->secondaryTileset;
    if (!tileset->metatiles) return -1;
    unsigned offset = tile.metatile < firstSecondary ? tile.metatile : tile.metatile - firstSecondary;
    memcpy(tile.tiles, tileset->metatiles + offset * NUM_TILES_PER_METATILE, sizeof(tile.tiles));
    *out = tile; return 0;
}
static int TileAt(int32_t x, int32_t y, struct RhhWorldTile *out)
{ return TileAtInternal(x, y, out, FALSE); }
static const struct Sprite *ActorSprite(uint32_t slot)
{
    if (!Ready() || slot >= OBJECT_EVENTS_COUNT || !gObjectEvents[slot].active
        || gObjectEvents[slot].spriteId >= MAX_SPRITES) return NULL;
    const struct Sprite *sprite = &gSprites[gObjectEvents[slot].spriteId];
    return sprite->inUse && sprite->oam.shape < 3 ? sprite : NULL;
}
static int ActorAt(uint32_t slot, struct RhhWorldActor *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct Sprite *sprite = ActorSprite(slot);
    if (!sprite) return -1;
    const struct ObjectEvent *actor = &gObjectEvents[slot];
    struct RhhWorldActor view = {
        .struct_size = sizeof(view), .slot = slot, .local_id = actor->localId,
        .graphics_id = actor->graphicsId,
        .map = RhhContentKeyForId(RHH_MAP, (actor->mapGroup << 8) | actor->mapNum),
        .x = actor->currentCoords.x - MAP_OFFSET, .y = actor->currentCoords.y - MAP_OFFSET,
        .previous_x = actor->previousCoords.x - MAP_OFFSET, .previous_y = actor->previousCoords.y - MAP_OFFSET,
        .screen_x = sprite->x + sprite->x2 + (sprite->coordOffsetEnabled ? gSpriteCoordOffsetX : 0),
        .screen_y = sprite->y + sprite->y2 + (sprite->coordOffsetEnabled ? gSpriteCoordOffsetY : 0),
        .elevation = actor->currentElevation, .facing = actor->facingDirection,
        .movement = actor->movementType, .animation = sprite->animNum, .animation_frame = sprite->animCmdIndex,
        .flags = (actor->isPlayer ? RHH_ACTOR_PLAYER : 0) | ((actor->invisible || sprite->invisible || ModsWildActorHidden(actor)) ? RHH_ACTOR_HIDDEN : 0)
            | (actor->offScreen ? RHH_ACTOR_OFFSCREEN : 0) | (actor->inShortGrass ? RHH_ACTOR_IN_SHORT_GRASS : 0)
            | (actor->inShallowFlowingWater ? RHH_ACTOR_IN_SHALLOW_WATER : 0) | (actor->inSandPile ? RHH_ACTOR_IN_SAND_PILE : 0),
        .sprite_width = sSizes[sprite->oam.shape][sprite->oam.size][0],
        .sprite_height = sSizes[sprite->oam.shape][sprite->oam.size][1],
        .sprite_priority = sprite->oam.priority, .sprite_subpriority = sprite->subpriority
    };
    *out = view; return 0;
}
static int ActorVisibility(uint32_t slot, struct RhhWorldActorVisibility *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct Sprite *sprite = ActorSprite(slot);
    if (!sprite) return -1;
    const struct ObjectEvent *actor = &gObjectEvents[slot];
    struct RhhWorldActorVisibility result = {.struct_size = sizeof(result), .slot = slot,
        .flags = (actor->offScreen ? RHH_WORLD_ACTOR_CULLED : 0)
            | (actor->invisible ? RHH_WORLD_ACTOR_OBJECT_HIDDEN : 0)
            | (sprite->invisible ? RHH_WORLD_ACTOR_SPRITE_HIDDEN : 0)
            | (ModsWildActorHidden(actor) ? RHH_WORLD_ACTOR_CONCEALED : 0)
            | (IsOverworldLinkActive() ? RHH_WORLD_ACTOR_LINK_ACTIVE : 0)
            | (ObjectEventUsesStandardMovementCallback(actor, sprite) ? RHH_WORLD_ACTOR_ENGINE_MOVEMENT : 0)};
    *out = result; return 0;
}
static int OamRgba(const struct OamData *oam, uint8_t *rgba, uint32_t capacity)
{
    if (!oam || oam->shape >= 3 || !rgba) return -1;
    unsigned w = sSizes[oam->shape][oam->size][0], h = sSizes[oam->shape][oam->size][1];
    unsigned units = oam->bpp ? 2 : 1, stride = (REG_DISPCNT & 0x40) ? w / 8 : 16;
    unsigned last = oam->tileNum + (((h / 8 - 1) * stride + w / 8 - 1) * units) + units;
    if (capacity < w * h * 4 || last > 1024) return -1;
    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x)
        {
            unsigned tx = x, ty = y;
            if (!(oam->affineMode & 1))
            {
                if (oam->matrixNum & ST_OAM_HFLIP) tx = w - x - 1;
                if (oam->matrixNum & ST_OAM_VFLIP) ty = h - y - 1;
            }
            unsigned block = ((ty / 8) * stride + tx / 8) * units + oam->tileNum;
            unsigned index = oam->bpp ? gNativeVram[0x10000 + block * 32 + (ty % 8) * 8 + tx % 8]
                : gNativeVram[0x10000 + block * 32 + (ty % 8) * 4 + (tx % 8) / 2];
            if (!oam->bpp) { index = (index >> ((tx & 1) * 4)) & 15; index += oam->paletteNum * 16; }
            unsigned colorIndex = oam->bpp ? index : index & 15;
            u16 color; memcpy(&color, gNativePalette + 0x200 + index * 2, sizeof(color));
            unsigned dst = (y * w + x) * 4;
            for (unsigned c = 0; c < 3; ++c)
            { unsigned v = (color >> (c * 5)) & 31; rgba[dst+c] = (v << 3) | (v >> 2); }
            rgba[dst+3] = colorIndex ? 255 : 0;
        }
    return 0;
}
static int SpriteRgba(uint32_t slot, uint8_t *rgba, uint32_t capacity)
{
    const struct Sprite *sprite = ActorSprite(slot);
    return sprite ? OamRgba(&sprite->oam, rgba, capacity) : -1;
}

static int Camera(struct RhhWorldCamera *out)
{
    if (!out || out->struct_size < sizeof(*out) || !Ready()) return -1;
    s16 x, y;
    SetSpritePosToMapCoords(MAP_OFFSET, MAP_OFFSET, &x, &y);
    struct RhhWorldCamera view = {
        .struct_size = sizeof(view), .viewport_width = DISPLAY_WIDTH, .viewport_height = DISPLAY_HEIGHT,
        .tile_pixels = 16,
        .map = RhhContentKeyForId(RHH_MAP, (gSaveBlock1Ptr->location.mapGroup << 8)
            | (u8)gSaveBlock1Ptr->location.mapNum),
        .origin_x = x + gSpriteCoordOffsetX, .origin_y = y + gSpriteCoordOffsetY
    };
    *out = view; return 0;
}

static const struct SubspriteTable *SpriteParts(const struct Sprite *sprite)
{
    if (!sprite->subspriteTables || sprite->subspriteMode == SUBSPRITES_OFF) return NULL;
    const struct SubspriteTable *table = &sprite->subspriteTables[sprite->subspriteTableNum];
    return table->subsprites ? table : NULL;
}
static unsigned PartCount(const struct Sprite *sprite)
{
    const struct SubspriteTable *table = SpriteParts(sprite);
    return table ? table->subspriteCount : 1;
}
static int Billboard(uint32_t slot, struct RhhWorldBillboard *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct Sprite *sprite = ActorSprite(slot);
    if (!sprite) return -1;
    struct RhhWorldCamera camera = {.struct_size = sizeof(camera)};
    struct RhhWorldActor actor = {.struct_size = sizeof(actor)};
    if (Camera(&camera) || ActorAt(slot, &actor)) return -1;
    int xOffset = sprite->coordOffsetEnabled ? gSpriteCoordOffsetX : 0;
    int yOffset = sprite->coordOffsetEnabled ? gSpriteCoordOffsetY : 0;
    int anchorX = sprite->x + xOffset, anchorY = sprite->y - sprite->centerToCornerVecY + yOffset;
    struct RhhWorldBillboard view = {
        .struct_size = sizeof(view), .slot = slot, .part_count = PartCount(sprite), .flags = actor.flags,
        .anchor_x = anchorX, .anchor_y = anchorY,
        .world_anchor_x = anchorX - camera.origin_x, .world_anchor_y = anchorY - camera.origin_y,
        .visual_center_x = actor.screen_x, .visual_center_y = actor.screen_y,
        .offset_x = sprite->x2, .offset_y = sprite->y2,
        .elevation = actor.elevation, .facing = actor.facing, .subpriority = sprite->subpriority
    };
    *out = view; return 0;
}

/* Mirror AddSubspritesToOamBuffer's positions, flips, tile offsets and
 * priorities. Do not rely on the cached OAM x/y, which may be from a prior
 * frame or wrap around the hardware coordinate range. */
static int PartOam(const struct Sprite *sprite, uint32_t index, struct OamData *out, int *x, int *y)
{
    const struct SubspriteTable *table = SpriteParts(sprite);
    struct OamData oam = sprite->oam;
    int dx = sprite->centerToCornerVecX, dy = sprite->centerToCornerVecY;
    if (table)
    {
        if (index >= table->subspriteCount) return -1;
        const struct Subsprite *part = &table->subsprites[index];
        if (part->shape >= 3) return -1;
        oam.shape = part->shape; oam.size = part->size;
        dx = part->x; dy = part->y;
        if (oam.matrixNum & ST_OAM_HFLIP) dx = -dx - sSizes[oam.shape][oam.size][0];
        if (oam.matrixNum & ST_OAM_VFLIP) dy = -dy - sSizes[oam.shape][oam.size][1];
        oam.tileNum = (oam.tileNum + part->tileOffset) & 0x3ff;
        if (sprite->subspriteMode < SUBSPRITES_IGNORE_PRIORITY) oam.priority = part->priority;
    }
    else if (index != 0) return -1;
    if (oam.shape >= 3) return -1;
    *x = sprite->x + sprite->x2 + dx + (sprite->coordOffsetEnabled ? gSpriteCoordOffsetX : 0);
    *y = sprite->y + sprite->y2 + dy + (sprite->coordOffsetEnabled ? gSpriteCoordOffsetY : 0);
    *out = oam; return 0;
}
static int PartView(const struct Sprite *sprite, uint32_t index, struct RhhWorldSpritePart *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct OamData oam; int x, y;
    if (!sprite || PartOam(sprite, index, &oam, &x, &y)) return -1;
    struct RhhWorldSpritePart view = {
        .struct_size = sizeof(view), .index = index, .width = sSizes[oam.shape][oam.size][0],
        .height = sSizes[oam.shape][oam.size][1], .screen_x = x, .screen_y = y,
        .priority = oam.priority, .subpriority = sprite->subpriority,
        .flags = ((oam.affineMode & 1) ? RHH_WORLD_PART_AFFINE : 0)
            | (oam.objMode == ST_OAM_OBJ_BLEND ? RHH_WORLD_PART_SEMITRANSPARENT : 0)
            | (oam.objMode == ST_OAM_OBJ_WINDOW ? RHH_WORLD_PART_WINDOW : 0)
            | (oam.mosaic ? RHH_WORLD_PART_MOSAIC : 0)
            | (oam.affineMode == ST_OAM_AFFINE_ERASE ? RHH_WORLD_PART_DISABLED : 0)
    };
    *out = view; return 0;
}
static int SpritePart(uint32_t slot, uint32_t index, struct RhhWorldSpritePart *out)
{ return PartView(ActorSprite(slot), index, out); }
static int SpritePartRgba(uint32_t slot, uint32_t index, uint8_t *rgba, uint32_t capacity)
{
    const struct Sprite *sprite = ActorSprite(slot);
    struct OamData oam; int x, y;
    return sprite && !PartOam(sprite, index, &oam, &x, &y) ? OamRgba(&oam, rgba, capacity) : -1;
}
static const struct Sprite *EffectSprite(uint32_t slot, unsigned *templateIndex)
{
    if (!Ready() || slot >= MAX_SPRITES || !gSprites[slot].inUse || !gSprites[slot].template) return NULL;
    for (unsigned i = 0; i <= FLDEFFOBJ_SHINY_SPARKLE; ++i)
        if (gSprites[slot].template == gFieldEffectObjectTemplatePointers[i])
        { if (templateIndex) *templateIndex = i; return &gSprites[slot]; }
    return NULL;
}
static uint32_t EffectSlots(void) { return MAX_SPRITES; }
static int EffectAtInternal(uint32_t slot, struct RhhWorldEffectSprite *out, bool32 extended)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    unsigned index; const struct Sprite *sprite = EffectSprite(slot, &index);
    struct RhhWorldCamera camera = {.struct_size = sizeof(camera)};
    if (!sprite || sprite->oam.shape >= 3 || Camera(&camera)) return -1;
    int x = sprite->x + sprite->x2 + (sprite->coordOffsetEnabled ? gSpriteCoordOffsetX : 0);
    int y = sprite->y + sprite->y2 + (sprite->coordOffsetEnabled ? gSpriteCoordOffsetY : 0);
    int anchorY = y - sprite->centerToCornerVecY;
    unsigned kind = index == FLDEFFOBJ_TALL_GRASS ? RHH_WORLD_EFFECT_TALL_GRASS
        : index == FLDEFFOBJ_LONG_GRASS ? RHH_WORLD_EFFECT_LONG_GRASS
        : index == FLDEFFOBJ_SHORT_GRASS ? RHH_WORLD_EFFECT_SHORT_GRASS : RHH_WORLD_EFFECT_OTHER;
    if (extended && kind == RHH_WORLD_EFFECT_OTHER)
    {
        switch (index)
        {
        case FLDEFFOBJ_SAND_FOOTPRINTS: case FLDEFFOBJ_DEEP_SAND_FOOTPRINTS:
        case FLDEFFOBJ_BIKE_TIRE_TRACKS: case FLDEFFOBJ_TRACKS_SLITHER:
        case FLDEFFOBJ_TRACKS_SPOT: case FLDEFFOBJ_TRACKS_BUG:
            kind = RHH_WORLD_EFFECT_TRACKS; break;
        case FLDEFFOBJ_RIPPLE: kind = RHH_WORLD_EFFECT_RIPPLE; break;
        case FLDEFFOBJ_SPLASH: case FLDEFFOBJ_JUMP_SMALL_SPLASH: case FLDEFFOBJ_JUMP_BIG_SPLASH:
            kind = RHH_WORLD_EFFECT_SPLASH; break;
        case FLDEFFOBJ_JUMP_TALL_GRASS: kind = RHH_WORLD_EFFECT_TALL_GRASS; break;
        case FLDEFFOBJ_JUMP_LONG_GRASS: kind = RHH_WORLD_EFFECT_LONG_GRASS; break;
        }
    }
    struct RhhWorldEffectSprite result = {.struct_size = sizeof(result), .slot = slot, .kind = kind,
        .template_index = index, .part_count = PartCount(sprite),
        .flags = (sprite->invisible ? RHH_WORLD_EFFECT_HIDDEN : 0)
            | (sprite->coordOffsetEnabled ? RHH_WORLD_EFFECT_MAP_OFFSET : 0),
        .center_x = x, .center_y = y, .anchor_x = x, .anchor_y = anchorY,
        .world_anchor_x = x - camera.origin_x, .world_anchor_y = anchorY - camera.origin_y};
    *out = result; return 0;
}
static int EffectAt(uint32_t slot, struct RhhWorldEffectSprite *out)
{ return EffectAtInternal(slot, out, FALSE); }
static int EffectAtV13(uint32_t slot, struct RhhWorldEffectSprite *out)
{ return EffectAtInternal(slot, out, TRUE); }
static int EffectPart(uint32_t slot, uint32_t index, struct RhhWorldSpritePart *out)
{ return PartView(EffectSprite(slot, NULL), index, out); }
static int EffectPartRgba(uint32_t slot, uint32_t index, uint8_t *rgba, uint32_t capacity)
{
    const struct Sprite *sprite = EffectSprite(slot, NULL);
    struct OamData oam; int x, y;
    return sprite && !PartOam(sprite, index, &oam, &x, &y) ? OamRgba(&oam, rgba, capacity) : -1;
}
static int PartTransform(const struct Sprite *sprite, u32 index, struct RhhWorldSpriteTransform *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct OamData oam; int x, y;
    struct RhhWorldSpritePart part = {.struct_size = sizeof(part)};
    if (!sprite || PartOam(sprite, index, &oam, &x, &y) || PartView(sprite, index, &part)) return -1;
    bool32 affine = !!(oam.affineMode & ST_OAM_AFFINE_ON_MASK);
    bool32 doubled = affine && !!(oam.affineMode & ST_OAM_AFFINE_DOUBLE_MASK);
    struct RhhWorldSpriteTransform result = {.struct_size = sizeof(result), .index = index, .flags = part.flags,
        .source_width = part.width, .source_height = part.height,
        .draw_width = part.width * (doubled ? 2 : 1), .draw_height = part.height * (doubled ? 2 : 1),
        .matrix_index = affine ? oam.matrixNum : UINT32_MAX, .screen_x = x, .screen_y = y, .a = 256, .d = 256};
    if (affine)
    {
        if (oam.matrixNum >= OAM_MATRIX_COUNT) return -1;
        const struct OamMatrix *matrix = &gOamMatrices[oam.matrixNum];
        result.a = matrix->a; result.b = matrix->b; result.c = matrix->c; result.d = matrix->d;
    }
    *out = result; return 0;
}
static s32 MosaicCoordinate(s32 position, u32 size)
{
    s32 remainder = position % (s32)size;
    return position - (remainder < 0 ? remainder + (s32)size : remainder);
}
static int PartRasterRgba(const struct Sprite *sprite, u32 index, const struct RhhWorldSpriteRasterOptions *options,
                          u8 *rgba, u32 capacity)
{
    struct RhhWorldSpriteTransform transform = {.struct_size = sizeof(transform)};
    struct OamData oam; int x, y;
    if (!options || options->struct_size < sizeof(*options) || (options->flags & ~RHH_WORLD_RASTER_MOSAIC)
        || !options->mosaic_width || options->mosaic_width > 16 || !options->mosaic_height || options->mosaic_height > 16
        || options->reserved || !rgba || PartTransform(sprite, index, &transform) || PartOam(sprite, index, &oam, &x, &y)
        || capacity < transform.draw_width * transform.draw_height * 4) return -1;
    if (transform.flags & RHH_WORLD_PART_DISABLED)
    { memset(rgba, 0, transform.draw_width * transform.draw_height * 4); return 0; }
    u8 source[64 * 64 * 4];
    if (OamRgba(&oam, source, sizeof(source))) return -1;
    for (u32 dy = 0; dy < transform.draw_height; ++dy)
    for (u32 dx = 0; dx < transform.draw_width; ++dx)
    {
        s32 localX = (s32)dx - (s32)transform.draw_width / 2;
        s32 localY = (s32)dy - (s32)transform.draw_height / 2;
        if ((options->flags & RHH_WORLD_RASTER_MOSAIC) && (transform.flags & RHH_WORLD_PART_MOSAIC))
        {
            localX = MosaicCoordinate(transform.screen_x + (s32)dx, options->mosaic_width)
                - transform.screen_x - (s32)transform.draw_width / 2;
            localY = MosaicCoordinate(transform.screen_y + (s32)dy, options->mosaic_height)
                - transform.screen_y - (s32)transform.draw_height / 2;
        }
        s32 sx = ((transform.a * localX + transform.b * localY) >> 8) + (s32)transform.source_width / 2;
        s32 sy = ((transform.c * localX + transform.d * localY) >> 8) + (s32)transform.source_height / 2;
        u8 *dest = rgba + (dy * transform.draw_width + dx) * 4;
        if (sx < 0 || sy < 0 || (u32)sx >= transform.source_width || (u32)sy >= transform.source_height)
            memset(dest, 0, 4);
        else memcpy(dest, source + ((u32)sy * transform.source_width + (u32)sx) * 4, 4);
    }
    return 0;
}
static int PartTransformedRgba(const struct Sprite *sprite, u32 index, u8 *rgba, u32 capacity)
{
    const struct RhhWorldSpriteRasterOptions options = {.struct_size = sizeof(options), .mosaic_width = 1, .mosaic_height = 1};
    return PartRasterRgba(sprite, index, &options, rgba, capacity);
}
static int SpritePartTransform(u32 slot, u32 index, struct RhhWorldSpriteTransform *out)
{ return PartTransform(ActorSprite(slot), index, out); }
static int SpritePartTransformedRgba(u32 slot, u32 index, u8 *rgba, u32 capacity)
{ return PartTransformedRgba(ActorSprite(slot), index, rgba, capacity); }
static int EffectPartTransform(u32 slot, u32 index, struct RhhWorldSpriteTransform *out)
{ return PartTransform(EffectSprite(slot, NULL), index, out); }
static int EffectPartTransformedRgba(u32 slot, u32 index, u8 *rgba, u32 capacity)
{ return PartTransformedRgba(EffectSprite(slot, NULL), index, rgba, capacity); }
static int SpritePartRasterRgba(u32 slot, u32 index, const struct RhhWorldSpriteRasterOptions *options, u8 *rgba, u32 capacity)
{ return PartRasterRgba(ActorSprite(slot), index, options, rgba, capacity); }
static int EffectPartRasterRgba(u32 slot, u32 index, const struct RhhWorldSpriteRasterOptions *options, u8 *rgba, u32 capacity)
{ return PartRasterRgba(EffectSprite(slot, NULL), index, options, rgba, capacity); }
/* Mirror the field shadow's termination conditions without mutating the
 * object, advancing its callback, or requiring a live effect to exist. */
static int ActorShadow(uint32_t slot, struct RhhWorldActorShadow *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct Sprite *sprite = ActorSprite(slot);
    if (!sprite) return -1;
    const struct ObjectEvent *actor = &gObjectEvents[slot];
    const struct ObjectEventGraphicsInfo *graphics = GetObjectEventGraphicsInfo(actor->graphicsId);
    if (!graphics) return -1;
    unsigned size = graphics->shadowSize == SHADOW_SIZE_S ? RHH_WORLD_SHADOW_SMALL
        : graphics->shadowSize == SHADOW_SIZE_M ? RHH_WORLD_SHADOW_MEDIUM
        : graphics->shadowSize == SHADOW_SIZE_L ? RHH_WORLD_SHADOW_LARGE : RHH_WORLD_SHADOW_NONE;
    s32 coverY = 0;
    u32 suppression = ((actor->invisible || sprite->invisible || actor->offScreen || ModsWildActorHidden(actor)
                        || ModsWildActorPresentation(actor->spriteId, &coverY) == 1) ? RHH_WORLD_SHADOW_HIDDEN : 0)
        | (actor->noShadow ? RHH_WORLD_SHADOW_DISABLED : 0)
        | (size == RHH_WORLD_SHADOW_NONE ? RHH_WORLD_SHADOW_GRAPHICS_NONE : 0)
        | (gWeatherPtr->noShadows ? RHH_WORLD_SHADOW_WEATHER : 0)
        | ((MetatileBehavior_IsPuddle(actor->currentMetatileBehavior)
            || MetatileBehavior_IsSurfableWaterOrUnderwater(actor->currentMetatileBehavior)
            || MetatileBehavior_IsSurfableWaterOrUnderwater(actor->previousMetatileBehavior)) ? RHH_WORLD_SHADOW_WATER : 0)
        | (MetatileBehavior_IsPokeGrass(actor->currentMetatileBehavior) ? RHH_WORLD_SHADOW_GRASS : 0)
        | (actor->inHotSprings ? RHH_WORLD_SHADOW_HOT_SPRINGS : 0)
        | (actor->inSandPile ? RHH_WORLD_SHADOW_SAND_PILE : 0);
    struct RhhWorldActorShadow result = {.struct_size = sizeof(result), .slot = slot, .size = size,
        .suppressions = suppression, .effect_slot = UINT32_MAX};
    /* FldEff_Shadow searches backward and owns the highest matching slot. */
    for (int i = MAX_SPRITES - 1; i >= 0; --i)
    {
        const struct Sprite *effect = &gSprites[i];
        if (effect->inUse && effect->callback == UpdateShadowFieldEffect
            && effect->data[0] == actor->localId && effect->data[1] == actor->mapNum
            && effect->data[2] == actor->mapGroup)
        {
            result.effect_slot = i; result.effect_flags = RHH_WORLD_SHADOW_EFFECT_ACTIVE
                | (!effect->invisible ? RHH_WORLD_SHADOW_EFFECT_VISIBLE : 0);
            break;
        }
    }
    *out = result; return 0;
}
static uint32_t LocalLightSlots(void) { return MAX_SPRITES; }
static int LocalLightAt(uint32_t slot, struct RhhWorldLocalLight *out)
{
    if (!out || out->struct_size < sizeof(*out) || !Ready() || slot >= MAX_SPRITES) return -1;
    const struct Sprite *sprite = &gSprites[slot];
    if (!sprite->inUse || sprite->callback != UpdateLightSprite) return -1;
    struct RhhWorldCamera camera = {.struct_size = sizeof(camera)};
    if (Camera(&camera) || !camera.map) return -1;
    int x = sprite->x + sprite->x2 + (sprite->coordOffsetEnabled ? gSpriteCoordOffsetX : 0);
    int y = sprite->y + sprite->y2 + (sprite->coordOffsetEnabled ? gSpriteCoordOffsetY : 0);
    unsigned kind = sprite->data[5] == LIGHT_TYPE_BALL ? RHH_WORLD_LOCAL_LIGHT_LAMP
        : sprite->data[5] == LIGHT_TYPE_PKMN_CENTER_SIGN ? RHH_WORLD_LOCAL_LIGHT_POKEMON_CENTER
        : sprite->data[5] == LIGHT_TYPE_POKE_MART_SIGN ? RHH_WORLD_LOCAL_LIGHT_MART : RHH_WORLD_LOCAL_LIGHT_UNKNOWN;
    struct RhhWorldLocalLight result = {.struct_size = sizeof(result), .slot = slot, .kind = kind,
        .flags = (!sprite->invisible && sprite->oam.affineMode != ST_OAM_AFFINE_ERASE
                  && sprite->oam.objMode != ST_OAM_OBJ_WINDOW ? RHH_WORLD_LOCAL_LIGHT_VISIBLE : 0)
            | (sprite->coordOffsetEnabled ? RHH_WORLD_LOCAL_LIGHT_MAP_OFFSET : 0),
        .map = camera.map, .tile_x = sprite->data[6] - MAP_OFFSET, .tile_y = sprite->data[7] - MAP_OFFSET,
        .screen_x = x, .screen_y = y, .world_x = x - camera.origin_x, .world_y = y - camera.origin_y};
    *out = result; return 0;
}
static int TileRgba(uint16_t tile, uint8_t *rgba, uint32_t capacity)
{
    if (!Ready() || !rgba || capacity < 8 * 8 * 4) return -1;
    unsigned base = ((REG_BG2CNT >> 2) & 3) * 0x4000;
    unsigned offset = base + (tile & 0x3ff) * 32;
    if (offset + 32 > 0x10000) return -1;
    unsigned palette = tile >> 12;
    for (unsigned y = 0; y < 8; ++y)
        for (unsigned x = 0; x < 8; ++x)
        {
            unsigned tx = tile & (1 << 10) ? 7 - x : x;
            unsigned ty = tile & (1 << 11) ? 7 - y : y;
            unsigned bits = gNativeVram[offset + ty * 4 + tx / 2];
            unsigned index = (bits >> ((tx & 1) * 4)) & 15;
            u16 color; memcpy(&color, gNativePalette + (palette * 16 + index) * 2, sizeof(color));
            unsigned dst = (y * 8 + x) * 4;
            for (unsigned c = 0; c < 3; ++c)
            { unsigned v = (color >> (c * 5)) & 31; rgba[dst+c] = (v << 3) | (v >> 2); }
            rgba[dst+3] = index ? 255 : 0;
        }
    return 0;
}
static uint32_t ActorSlots(void) { return OBJECT_EVENTS_COUNT; }
struct WildActorEntry {
    u64 handle, runId, individual;
    u16 graphics;
    u8 localId, mapGroup, mapNum, hasMon, moving, moveAction, identified, needsSave;
    u8 cueSprite, cueAge, observed, presentationEligible;
    s16 cueX, cueY;
    struct Pokemon mon;
    struct RhhWildConcealment conceal;
    struct RhhWildStepResult step;
    bool8 concealMoved;
};
/* The native save container already validates the exact engine save layout.
 * Store the same fixed-width encrypted Pokémon representation used there. */
_Static_assert(sizeof(struct Pokemon) == 100, "wild actor save layout changed");
enum { WILD_ACTOR_RECORD_HEADER = 16, WILD_ACTOR_RECORD_SIZE = WILD_ACTOR_RECORD_HEADER + sizeof(struct Pokemon),
       WILD_ACTOR_STATE_SIZE = OBJECT_EVENTS_COUNT * WILD_ACTOR_RECORD_SIZE };
static const char sWildActorStateId[] = "rhh:wild_actor_state";
static u8 *WildActorState(void) { return RhhModsState(sWildActorStateId); }
static u8 *WildActorRecord(u8 *state, u32 slot) { return state + slot * WILD_ACTOR_RECORD_SIZE; }
static u16 WildActorRead16(const u8 *p) { return (u16)p[0] | (u16)p[1] << 8; }
static void WildActorWrite16(u8 *p, u16 n) { p[0] = n; p[1] = n >> 8; }
static u32 WildActorRead32(const u8 *p) { return (u32)WildActorRead16(p) | (u32)WildActorRead16(p + 2) << 16; }
static void WildActorWrite32(u8 *p, u32 n) { WildActorWrite16(p, n); WildActorWrite16(p + 2, n >> 16); }
static u64 WildActorRead64(const u8 *p)
{ u64 value = 0; for (u32 i = 0; i < 8; ++i) value |= (u64)p[i] << (i * 8); return value; }
static void WildActorWrite64(u8 *p, u64 value)
{ for (u32 i = 0; i < 8; ++i) p[i] = value >> (i * 8); }
static int ValidateWildActorState(const u8 *state, u32 size)
{
    if (size != WILD_ACTOR_STATE_SIZE) return -1;
    bool8 seen[OWE_SPAWNS_MAX] = {0};
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot)
    {
        const u8 *record = state + slot * WILD_ACTOR_RECORD_SIZE;
        if (!record[0])
        {
            for (u32 i = 0; i < WILD_ACTOR_RECORD_SIZE; ++i) if (record[i]) return -1;
            continue;
        }
        if ((record[0] != 1 && record[0] != 2) || record[7]) return -1;
        u64 individual = WildActorRead64(record + 8);
        if (record[0] == 1 && !IS_LOCALID_GENERATED_OWE(record[1])) return -1;
        if (record[0] == 2 && !individual) return -1;
        if (IS_LOCALID_GENERATED_OWE(record[1]))
        {
            u32 index = LOCALID_OW_ENCOUNTER_END - record[1];
            if (index >= OWE_SPAWNS_MAX || seen[index]) return -1;
            seen[index] = TRUE;
        }
        for (u32 previous = 0; individual && previous < slot; ++previous)
            if (WildActorRead64(state + previous * WILD_ACTOR_RECORD_SIZE + 8) == individual) return -1;
        u16 graphics = WildActorRead16(record + 4);
        if (!(graphics & OBJ_EVENT_MON) || (graphics & 0x8000)
            || !RhhContentKeyForId(RHH_MAP, ((u32)record[2] << 8) | record[3])) return -1;
        if (record[0] == 2)
        {
            for (u32 i = WILD_ACTOR_RECORD_HEADER; i < WILD_ACTOR_RECORD_SIZE; ++i)
                if (record[i]) return -1;
            continue;
        }
        struct Pokemon mon; memcpy(&mon, record + WILD_ACTOR_RECORD_HEADER, sizeof(mon));
        if (!GetMonData(&mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(&mon, MON_DATA_SANITY_IS_BAD_EGG)
            || GetMonData(&mon, MON_DATA_IS_EGG)) return -1;
        u32 species = GetMonData(&mon, MON_DATA_SPECIES), personality = GetMonData(&mon, MON_DATA_PERSONALITY);
        if (GetMonData(&mon, MON_DATA_SANITY_IS_BAD_EGG) || !species || species >= NUM_SPECIES
            || !RhhContentKeyForId(RHH_SPECIES, species) || mon.level != record[6]
            || mon.level < MIN_LEVEL || mon.level > MAX_LEVEL || !mon.hp || mon.hp > mon.maxHP) return -1;
        u32 visualSpecies = species == SPECIES_UNOWN ? GetUnownSpeciesId(personality) : species;
        if ((graphics & OBJ_EVENT_MON_SPECIES_MASK) != visualSpecies
            || !!(graphics & OBJ_EVENT_MON_SHINY) != !!GetMonData(&mon, MON_DATA_IS_SHINY)
            || !!(graphics & OBJ_EVENT_MON_FEMALE) != (GetGenderFromSpeciesAndPersonality(visualSpecies, personality) == MON_FEMALE)) return -1;
    }
    return 0;
}
static int MigrateWildActorState(u32 schema, const u8 *old, u32 oldSize, u8 *state, u32 size)
{
    if (schema != 1 || oldSize != WILD_ACTOR_STATE_SIZE || size != WILD_ACTOR_STATE_SIZE) return -1;
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot)
    {
        const u8 *record = old + slot * WILD_ACTOR_RECORD_SIZE;
        if (record[0] > 1 || WildActorRead64(record + 8)) return -1;
    }
    memcpy(state, old, size);
    return ValidateWildActorState(state, size);
}
static const struct RhhModDefinition sWildActorState = {
    .struct_size = sizeof(sWildActorState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 2, .state_schema = 2, .id = sWildActorStateId,
    .flags = RHH_MOD_REQUIRED, .state_size = WILD_ACTOR_STATE_SIZE,
    .validate = ValidateWildActorState, .migrate = MigrateWildActorState
};
static const char sWildActorSequenceId[] = "rhh:wild_actor_sequence";
static int MergeWildActorSequence(u8 *state, const u8 *other, u32 size)
{
    if (size != 8) return -1;
    if (WildActorRead64(other) > WildActorRead64(state)) memcpy(state, other, 8);
    return 0;
}
static const struct RhhModDefinition sWildActorSequence = {
    .struct_size = sizeof(sWildActorSequence), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sWildActorSequenceId,
    .flags = RHH_MOD_REQUIRED | RHH_MOD_DURABLE, .state_size = 8, .merge = MergeWildActorSequence
};
enum { WILD_CONCEAL_RECORD_SIZE = 40, WILD_CONCEAL_STATE_SIZE = OBJECT_EVENTS_COUNT * WILD_CONCEAL_RECORD_SIZE };
static const char sWildConcealStateId[] = "rhh:wild_conceal_state";
static u8 *WildConcealState(void) { return RhhModsState(sWildConcealStateId); }
static u8 *WildConcealRecord(u8 *state, u32 slot) { return state + slot * WILD_CONCEAL_RECORD_SIZE; }
static int ValidateWildConcealState(const u8 *state, u32 size)
{
    if (size != WILD_CONCEAL_STATE_SIZE) return -1;
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot)
    {
        const u8 *p = state + slot * WILD_CONCEAL_RECORD_SIZE;
        if (!p[0])
        {
            for (u32 i = 0; i < WILD_CONCEAL_RECORD_SIZE; ++i) if (p[i]) return -1;
            continue;
        }
        if (p[0] > RHH_WILD_CONCEAL_GROUND || (p[1] & ~(RHH_WILD_REVEAL_ON_MOVE | RHH_WILD_REVEAL_ON_APPROACH))
            || ((p[1] & RHH_WILD_REVEAL_ON_APPROACH) ? (!p[2] || p[2] > 16) : p[2] != 0)
            || p[7] || !WildActorRead64(p + 8) || !WildActorRead64(p + 24)
            || !(WildActorRead16(p + 32) & OBJ_EVENT_MON) || (WildActorRead16(p + 32) & 0x8000)
            || !RhhContentKeyForId(RHH_MAP, ((u32)p[4] << 8) | p[5])) return -1;
        for (u32 i = 34; i < WILD_CONCEAL_RECORD_SIZE; ++i) if (p[i]) return -1;
    }
    return 0;
}
static const struct RhhModDefinition sWildConcealState = {
    .struct_size = sizeof(sWildConcealState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sWildConcealStateId,
    .flags = RHH_MOD_REQUIRED, .state_size = WILD_CONCEAL_STATE_SIZE, .validate = ValidateWildConcealState
};
enum { WILD_TRAIT_HEADER = 16, WILD_TRAIT_ITEM_SIZE = 24,
       WILD_TRAIT_RECORD_SIZE = WILD_TRAIT_HEADER + RHH_WILD_TRAIT_OVERRIDE_CAPACITY * WILD_TRAIT_ITEM_SIZE,
       WILD_TRAIT_STATE_SIZE = OBJECT_EVENTS_COUNT * WILD_TRAIT_RECORD_SIZE };
static const char sWildTraitStateId[] = "rhh:wild_trait_state";
static u8 *WildTraitState(void) { return RhhModsState(sWildTraitStateId); }
static u8 *WildTraitRecord(u8 *state, u32 slot) { return state + slot * WILD_TRAIT_RECORD_SIZE; }
static int ValidateWildTraitState(const u8 *state, u32 size)
{
    if (size != WILD_TRAIT_STATE_SIZE) return -1;
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot)
    {
        const u8 *p = state + slot * WILD_TRAIT_RECORD_SIZE;
        u64 serial = WildActorRead64(p), revision = WildActorRead64(p + 8), previous = 0;
        if (!!serial != !!revision) return -1;
        for (u32 prior = 0; serial && prior < slot; ++prior)
            if (WildActorRead64(state + prior * WILD_TRAIT_RECORD_SIZE) == serial) return -1;
        for (u32 i = 0; i < RHH_WILD_TRAIT_OVERRIDE_CAPACITY; ++i)
        {
            const u8 *item = p + WILD_TRAIT_HEADER + i * WILD_TRAIT_ITEM_SIZE;
            u64 tag = WildActorRead64(item), owner = WildActorRead64(item + 8);
            if (!tag)
            {
                for (u32 j = 0; j < WILD_TRAIT_ITEM_SIZE; ++j) if (item[j]) return -1;
            }
            else
            {
                if (!serial || !owner || tag <= previous || (i && !previous)
                    || (item[16] != RHH_WILD_TRAIT_OFF && item[16] != RHH_WILD_TRAIT_ON)) return -1;
                for (u32 j = 17; j < WILD_TRAIT_ITEM_SIZE; ++j) if (item[j]) return -1;
            }
            previous = tag;
        }
    }
    return 0;
}
static const struct RhhModDefinition sWildTraitState = {
    .struct_size = sizeof(sWildTraitState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sWildTraitStateId,
    .flags = RHH_MOD_REQUIRED, .state_size = WILD_TRAIT_STATE_SIZE, .validate = ValidateWildTraitState
};
enum { WILD_PROFILE_HEADER = 16, WILD_PROFILE_ITEM_SIZE = 24,
       WILD_PROFILE_RECORD_SIZE = WILD_PROFILE_HEADER + RHH_WILD_PROFILE_CAPACITY * WILD_PROFILE_ITEM_SIZE,
       WILD_PROFILE_STATE_SIZE = OBJECT_EVENTS_COUNT * WILD_PROFILE_RECORD_SIZE };
static const char sWildProfileStateId[] = "rhh:wild_profile_state";
static u8 *WildProfileState(void) { return RhhModsState(sWildProfileStateId); }
static u8 *WildProfileRecord(u8 *state, u32 slot) { return state + slot * WILD_PROFILE_RECORD_SIZE; }
static int ValidateWildProfileState(const u8 *state, u32 size)
{
    if (size != WILD_PROFILE_STATE_SIZE) return -1;
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot)
    {
        const u8 *p = state + slot * WILD_PROFILE_RECORD_SIZE;
        u64 serial = WildActorRead64(p), revision = WildActorRead64(p + 8), lastOwner = 0, lastField = 0;
        if (!!serial != !!revision) return -1;
        for (u32 prior = 0; serial && prior < slot; ++prior)
            if (WildActorRead64(state + prior * WILD_PROFILE_RECORD_SIZE) == serial) return -1;
        bool32 empty = FALSE;
        for (u32 i = 0; i < RHH_WILD_PROFILE_CAPACITY; ++i)
        {
            const u8 *item = p + WILD_PROFILE_HEADER + i * WILD_PROFILE_ITEM_SIZE;
            u64 owner = WildActorRead64(item), field = WildActorRead64(item + 8);
            if (!owner)
            {
                empty = TRUE;
                for (u32 j = 0; j < WILD_PROFILE_ITEM_SIZE; ++j) if (item[j]) return -1;
            }
            else if (!serial || !field || empty || owner < lastOwner || (owner == lastOwner && field <= lastField))
                return -1;
            lastOwner = owner; lastField = field;
        }
    }
    return 0;
}
static const struct RhhModDefinition sWildProfileState = {
    .struct_size = sizeof(sWildProfileState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sWildProfileStateId,
    .flags = RHH_MOD_REQUIRED, .state_size = WILD_PROFILE_STATE_SIZE, .validate = ValidateWildProfileState
};
enum { WILD_ACTIVITY_RECORD_SIZE = 48, WILD_ACTIVITY_STATE_SIZE = OBJECT_EVENTS_COUNT * WILD_ACTIVITY_RECORD_SIZE };
static const char sWildActivityStateId[] = "rhh:wild_activity_state";
static u8 *WildActivityState(void) { return RhhModsState(sWildActivityStateId); }
static u8 *WildActivityRecord(u8 *state, u32 slot) { return state + slot * WILD_ACTIVITY_RECORD_SIZE; }
static int ValidateWildActivityState(const u8 *state, u32 size)
{
    if (size != WILD_ACTIVITY_STATE_SIZE) return -1;
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot)
    {
        const u8 *p = state + slot * WILD_ACTIVITY_RECORD_SIZE;
        u64 serial = WildActorRead64(p);
        if (!serial)
        {
            for (u32 i = 0; i < WILD_ACTIVITY_RECORD_SIZE; ++i) if (p[i]) return -1;
            continue;
        }
        if (!WildActorRead64(p + 8) || !WildActorRead64(p + 16)
            || WildActorRead64(p + 32) < WildActorRead64(p + 24) || p[40] > RHH_WILD_BEHAVIOR_ENGAGE) return -1;
        for (u32 i = 41; i < WILD_ACTIVITY_RECORD_SIZE; ++i) if (p[i]) return -1;
        for (u32 i = 0; i < slot; ++i)
            if (WildActorRead64(state + i * WILD_ACTIVITY_RECORD_SIZE) == serial) return -1;
    }
    return 0;
}
static const struct RhhModDefinition sWildActivityState = {
    .struct_size = sizeof(sWildActivityState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sWildActivityStateId,
    .flags = RHH_MOD_REQUIRED, .state_size = WILD_ACTIVITY_STATE_SIZE, .validate = ValidateWildActivityState
};
enum { WILD_POLICY_STATE_SIZE = 24 };
static const char sWildPolicyStateId[] = "rhh:wild_behavior_policy";
static int ValidateWildPolicyState(const u8 *state, u32 size)
{
    if (!state || size != WILD_POLICY_STATE_SIZE) return -1;
    u64 revision = WildActorRead64(state), requester = WildActorRead64(state + 8);
    u32 x = WildActorRead32(state + 16), y = WildActorRead32(state + 20);
    return (!revision && (requester || x || y)) || (revision && !requester)
        || x > RHH_WILD_BEHAVIOR_RADIUS_MAX || y > RHH_WILD_BEHAVIOR_RADIUS_MAX ? -1 : 0;
}
static const struct RhhModDefinition sWildPolicyState = {
    .struct_size = sizeof(sWildPolicyState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sWildPolicyStateId,
    .state_size = WILD_POLICY_STATE_SIZE, .validate = ValidateWildPolicyState
};
enum { WILD_QUEUE_V1_RECORD_SIZE = 40, WILD_QUEUE_GUARD_SIZE = 12,
       WILD_QUEUE_V3_RECORD_SIZE = WILD_QUEUE_V1_RECORD_SIZE + RHH_WILD_QUEUE_CAPACITY * WILD_QUEUE_GUARD_SIZE,
       WILD_QUEUE_GOAL_SIZE = 48, WILD_QUEUE_RECORD_SIZE = WILD_QUEUE_V3_RECORD_SIZE + WILD_QUEUE_GOAL_SIZE,
       WILD_QUEUE_STATE_SIZE = OBJECT_EVENTS_COUNT * WILD_QUEUE_RECORD_SIZE };
static const char sWildQueueStateId[] = "rhh:wild_queue_state";
static u8 *WildQueueState(void) { return RhhModsState(sWildQueueStateId); }
static u8 *WildQueueRecord(u8 *state, u32 slot) { return state + slot * WILD_QUEUE_RECORD_SIZE; }
static int ValidateWildQueueState(const u8 *state, u32 size)
{
    if (size != WILD_QUEUE_STATE_SIZE) return -1;
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot)
    {
        const u8 *p = state + slot * WILD_QUEUE_RECORD_SIZE; u64 serial = WildActorRead64(p);
        if (!serial) { for (u32 j = 0; j < WILD_QUEUE_RECORD_SIZE; ++j) if (p[j]) return -1; continue; }
        u32 count = p[24];
        if (!WildActorRead64(p + 8) || count > RHH_WILD_QUEUE_CAPACITY || (!!count != !!WildActorRead64(p + 16))) return -1;
        for (u32 j = 0; j < RHH_WILD_QUEUE_CAPACITY; ++j)
            if (j < count ? (p[25 + j] < RHH_WILD_BEHAVIOR_SOUTH || p[25 + j] > RHH_WILD_BEHAVIOR_ENGAGE)
                          : p[25 + j] != 0) return -1;
        if (p[33] > 1 || (p[33] && !count) || p[34] > 1 || (p[34] && !p[33])
            || (p[34] ? WildActorRead16(p + 35) > 2000 : WildActorRead16(p + 35) != 0)) return -1;
        if (p[37] > 1 || (p[37] && !p[33])) return -1;
        for (u32 j = 38; j < WILD_QUEUE_V1_RECORD_SIZE; ++j) if (p[j]) return -1;
        for (u32 i = 0; i < RHH_WILD_QUEUE_CAPACITY; ++i)
        {
            const u8 *guard = p + WILD_QUEUE_V1_RECORD_SIZE + i * WILD_QUEUE_GUARD_SIZE;
            if (!p[33] || i >= count)
            { for (u32 j = 0; j < WILD_QUEUE_GUARD_SIZE; ++j) if (guard[j]) return -1; continue; }
            for (u32 j = 0; j < 4; ++j) if (WildActorRead16(guard + 2 * j) > INT16_MAX - MAP_OFFSET) return -1;
            if (p[25 + i] > RHH_WILD_BEHAVIOR_EAST || guard[8] >= 16 || guard[10] >= 16
                || guard[9] > DIR_NORTHEAST || guard[11] > DIR_NORTHEAST) return -1;
            if (i) {
                const u8 *previous = guard - WILD_QUEUE_GUARD_SIZE;
                if (memcmp(previous + 4, guard, 4) || previous[10] != guard[8] || previous[11] != guard[9]) return -1;
            }
        }
        const u8 *goal = p + WILD_QUEUE_V3_RECORD_SIZE;
        if (!goal[0]) { for (u32 j = 0; j < WILD_QUEUE_GOAL_SIZE; ++j) if (goal[j]) return -1; }
        else
        {
            if (goal[0] > RHH_WILD_QUEUE_GOAL_HABITAT || !(goal[1] & RHH_WILD_QUEUE_ROUTE_RETAIN_GOAL)
                || (goal[1] & ~(RHH_WILD_QUEUE_ROUTE_RETAIN_GOAL | RHH_WILD_QUEUE_ROUTE_PREFIX | RHH_WILD_QUEUE_ROUTE_INTERRUPTIBLE | RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL))
                || (goal[1] & RHH_WILD_QUEUE_ROUTE_INTERRUPTIBLE ? WildActorRead16(goal + 2) > 2000 : WildActorRead16(goal + 2) != 0)
                || goal[4] > 1 || goal[5] || goal[6] || goal[7] || !WildActorRead64(goal + 8) || !WildActorRead64(goal + 16)
                || (count && (WildActorRead64(p + 16) != WildActorRead64(goal + 8)
                    || p[37] != !!(goal[1] & RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL)))) return -1;
            if (goal[0] == RHH_WILD_QUEUE_GOAL_POINT)
            {
                if (WildActorRead32(goal + 24) > INT16_MAX - MAP_OFFSET || WildActorRead32(goal + 28) > INT16_MAX - MAP_OFFSET
                    || WildActorRead32(goal + 32) || WildActorRead32(goal + 36) || WildActorRead32(goal + 40) || WildActorRead32(goal + 44)) return -1;
            }
            else if (WildActorRead32(goal + 24) || WildActorRead32(goal + 28)
                || WildActorRead32(goal + 32) > RHH_WILD_HABITAT_ROUTE_MAX_RADIUS
                || ((WildActorRead32(goal + 36) | WildActorRead32(goal + 40) | WildActorRead32(goal + 44)) & ~RHH_HABITAT_FLAGS_ALL)) return -1;
        }
        for (u32 j = 0; j < slot; ++j) if (WildActorRead64(state + j * WILD_QUEUE_RECORD_SIZE) == serial) return -1;
    }
    return 0;
}
static int MigrateWildQueueState(u32 schema, const u8 *old, u32 oldSize, u8 *state, u32 size)
{
    u32 oldRecord = schema == 1 ? WILD_QUEUE_V1_RECORD_SIZE : (schema == 2 || schema == 3) ? WILD_QUEUE_V3_RECORD_SIZE
        : schema == 4 ? WILD_QUEUE_RECORD_SIZE : 0;
    if (!oldRecord || oldSize != OBJECT_EVENTS_COUNT * oldRecord || size != WILD_QUEUE_STATE_SIZE) return -1;
    memset(state, 0, size);
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot)
    {
        const u8 *source = old + slot * oldRecord;
        if (schema < 3) for (u32 i = schema == 1 ? 33 : 34; i < WILD_QUEUE_V1_RECORD_SIZE; ++i) if (source[i]) return -1;
        if (schema >= 3 && (source[37] || source[38] || source[39])) return -1;
        if (schema == 4 && (source[WILD_QUEUE_V3_RECORD_SIZE + 1] & RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL)) return -1;
        memcpy(WildQueueRecord(state, slot), source, oldRecord);
    }
    return ValidateWildQueueState(state, size);
}
static const struct RhhModDefinition sWildQueueState = {
    .struct_size = sizeof(sWildQueueState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 5, .state_schema = 5, .id = sWildQueueStateId,
    .flags = RHH_MOD_REQUIRED, .state_size = WILD_QUEUE_STATE_SIZE,
    .validate = ValidateWildQueueState, .migrate = MigrateWildQueueState
};
/* Dormant actor storage is absent from ordinary saves until it has records.
 * Unlike an optional module record, an unknown named archive makes an older
 * host reject the save instead of silently dropping preserved individuals. */
enum { WILD_DORMANT_HEADER = 32, WILD_DORMANT_META = 40,
       WILD_DORMANT_MON = WILD_DORMANT_META,
       WILD_DORMANT_TRAIT = WILD_DORMANT_MON + sizeof(struct Pokemon),
       WILD_DORMANT_PROFILE = WILD_DORMANT_TRAIT + WILD_TRAIT_RECORD_SIZE,
       WILD_DORMANT_ACTIVITY = WILD_DORMANT_PROFILE + WILD_PROFILE_RECORD_SIZE,
       WILD_DORMANT_QUEUE = WILD_DORMANT_ACTIVITY + WILD_ACTIVITY_RECORD_SIZE,
       WILD_DORMANT_CONCEAL = WILD_DORMANT_QUEUE + WILD_QUEUE_RECORD_SIZE,
       WILD_DORMANT_V1_RECORD = WILD_DORMANT_CONCEAL + WILD_CONCEAL_RECORD_SIZE,
       WILD_DORMANT_CLOCK = WILD_DORMANT_V1_RECORD,
       WILD_DORMANT_RECORD = WILD_DORMANT_CLOCK + 8 };
static u8 (*sWildDormant)[WILD_DORMANT_RECORD];
static size_t sWildDormantCapacity;
static u32 sWildDormantCount;
static u64 sWildDormantRun;
static u64 sWildDormantRemoving;
static bool8 sWildDormantBusy;
static const struct RhhWildDormantRelocation *sWildDormantRelocation;
static const struct RhhWildDormantProfileResult *sWildDormantProfileContext;
static const struct RhhWildDormantProfileBatchResult *sWildDormantProfileBatchContext;
static const struct RhhWildDormantClockChange *sWildDormantClockContext;
static const struct RhhWildDormantWorkResult *sWildDormantWorkContext;
static void WildDormantReset(void)
{
    free(sWildDormant); sWildDormant = NULL; sWildDormantCapacity = 0;
    sWildDormantCount = 0; sWildDormantRun = 0;
    sWildDormantRemoving = 0; sWildDormantBusy = FALSE; sWildDormantRelocation = NULL; sWildDormantProfileContext = NULL;
    sWildDormantProfileBatchContext = NULL;
    sWildDormantClockContext = NULL;
    sWildDormantWorkContext = NULL;
}
static int WildDormantValidateInternal(const u8 *data, u32 size, u64 preparing)
{
    if (!size) return data ? -1 : 0;
    if (!data || size < WILD_DORMANT_HEADER
        || (memcmp(data, "RHWDOR01", 8) && memcmp(data, "RHWDOR02", 8))
        || WildActorRead32(data + 8) != size || !WildActorRead64(data + 16)
        || WildActorRead64(data + 24)) return -1;
    bool32 legacy = !memcmp(data, "RHWDOR01", 8);
    u32 stride = legacy ? WILD_DORMANT_V1_RECORD : WILD_DORMANT_RECORD;
    u32 count = WildActorRead32(data + 12);
    if (!count || count > (size - WILD_DORMANT_HEADER) / stride
        || size != WILD_DORMANT_HEADER + count * stride) return -1;
    const u8 *run = RhhModsState("rhh:host_run"), *sequence = RhhModsState(sWildActorSequenceId), *live = WildActorState();
    if (!run || !sequence || !live || WildActorRead64(run + 8) != WildActorRead64(data + 16)) return -1;
    u64 previous = 0;
    u8 scratch[OBJECT_EVENTS_COUNT * WILD_PROFILE_RECORD_SIZE];
    for (u32 i = 0; i < count; ++i)
    {
        const u8 *p = data + WILD_DORMANT_HEADER + i * stride;
        u64 serial = WildActorRead64(p), owner = WildActorRead64(p + 8), map = WildActorRead64(p + 16);
        u32 mapId;
        if (!serial || serial <= previous || serial > WildActorRead64(sequence) || !owner || !map
            || RhhContentResolve(RHH_MAP, map, &mapId)
            || WildActorRead32(p + 24) > INT16_MAX - MAP_OFFSET || WildActorRead32(p + 28) > INT16_MAX - MAP_OFFSET
            || p[32] < DIR_SOUTH || p[32] > DIR_NORTHEAST || p[33] >= 16 || p[34] > 1
            || (!legacy && WildActorRead64(p + WILD_DORMANT_CLOCK) > WildActorRead64(run + 24))) return -1;
        for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot)
            if (serial != preparing && WildActorRead64(live + slot * WILD_ACTOR_RECORD_SIZE + 8) == serial) return -1;
        for (u32 j = 35; j < WILD_DORMANT_META; ++j) if (p[j]) return -1;
        struct Pokemon mon; memcpy(&mon, p + WILD_DORMANT_MON, sizeof(mon));
        u32 species = GetMonData(&mon, MON_DATA_SPECIES);
        if (!GetMonData(&mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(&mon, MON_DATA_SANITY_IS_BAD_EGG)
            || GetMonData(&mon, MON_DATA_IS_EGG) || !species || species >= NUM_SPECIES
            || !RhhContentKeyForId(RHH_SPECIES, species) || mon.level < MIN_LEVEL || mon.level > MAX_LEVEL
            || !mon.hp || mon.hp > mon.maxHP) return -1;
        const u32 offsets[] = {WILD_DORMANT_TRAIT, WILD_DORMANT_PROFILE, WILD_DORMANT_ACTIVITY, WILD_DORMANT_QUEUE};
        const u32 lengths[] = {WILD_TRAIT_RECORD_SIZE, WILD_PROFILE_RECORD_SIZE, WILD_ACTIVITY_RECORD_SIZE, WILD_QUEUE_RECORD_SIZE};
        for (u32 j = 0; j < ARRAY_COUNT(offsets); ++j)
        {
            const u8 *record = p + offsets[j]; u64 recordSerial = WildActorRead64(record);
            if (recordSerial && recordSerial != serial) return -1;
            memset(scratch, 0, sizeof(scratch)); memcpy(scratch, record, lengths[j]);
            int bad = j == 0 ? ValidateWildTraitState(scratch, WILD_TRAIT_STATE_SIZE)
                : j == 1 ? ValidateWildProfileState(scratch, WILD_PROFILE_STATE_SIZE)
                : j == 2 ? ValidateWildActivityState(scratch, WILD_ACTIVITY_STATE_SIZE)
                : ValidateWildQueueState(scratch, WILD_QUEUE_STATE_SIZE);
            if (bad) return -1;
        }
        const u8 *conceal = p + WILD_DORMANT_CONCEAL;
        memset(scratch, 0, sizeof(scratch)); memcpy(scratch, conceal, WILD_CONCEAL_RECORD_SIZE);
        if (ValidateWildConcealState(scratch, WILD_CONCEAL_STATE_SIZE)
            || (conceal[0] && (WildActorRead64(conceal + 16) != serial
                || conceal[4] != (mapId >> 8) || conceal[5] != (mapId & 0xff)))) return -1;
        previous = serial;
    }
    return 0;
}
static int WildDormantValidate(const u8 *data, u32 size)
{ return WildDormantValidateInternal(data, size, 0); }
static int WildDormantExport(u8 **out, u32 *size)
{
    if (!out || !size) return -1;
    *out = NULL; *size = 0;
    if (!sWildDormantCount) return sWildDormantRun ? -1 : 0;
    if (sWildDormantCount > (UINT32_MAX - WILD_DORMANT_HEADER) / WILD_DORMANT_RECORD) return -1;
    u32 length = WILD_DORMANT_HEADER + sWildDormantCount * WILD_DORMANT_RECORD;
    u8 *data = calloc(length, 1); if (!data) return -1;
    memcpy(data, "RHWDOR02", 8); WildActorWrite32(data + 8, length); WildActorWrite32(data + 12, sWildDormantCount);
    WildActorWrite64(data + 16, sWildDormantRun); memcpy(data + WILD_DORMANT_HEADER, sWildDormant, length - WILD_DORMANT_HEADER);
    if (WildDormantValidate(data, length)) { free(data); return -1; }
    *out = data; *size = length; return 0;
}
static int WildDormantImport(const u8 *data, u32 size)
{
    if (WildDormantValidate(data, size)) return -1;
    if (!size) { WildDormantReset(); return 0; }
    u32 count = WildActorRead32(data + 12);
    u8 (*records)[WILD_DORMANT_RECORD] = calloc(count, WILD_DORMANT_RECORD);
    if (!records) return -1;
    WildDormantReset(); sWildDormant = records; sWildDormantCapacity = count;
    sWildDormantCount = count; sWildDormantRun = WildActorRead64(data + 16);
    bool32 legacy = !memcmp(data, "RHWDOR01", 8);
    u32 stride = legacy ? WILD_DORMANT_V1_RECORD : WILD_DORMANT_RECORD;
    const u8 *run = RhhModsState("rhh:host_run");
    for (u32 i = 0; i < sWildDormantCount; ++i)
    {
        memcpy(sWildDormant[i], data + WILD_DORMANT_HEADER + i * stride, stride);
        if (legacy) WildActorWrite64(sWildDormant[i] + WILD_DORMANT_CLOCK, WildActorRead64(run + 24));
    }
    return 0;
}
static const struct NativeSaveArchiveCodec sWildDormantCodec = {
    WildDormantExport, WildDormantValidate, WildDormantImport, WildDormantReset, "rhh:wild_dormant", NULL
};
const struct NativeSaveArchiveCodec *RhhModsWildDormantCodec(void) { return &sWildDormantCodec; }
int RhhModsWildQueueProjectSave(const char *id, const u8 *state, u32 stateSize, u32 *schema, u32 *size)
{
    if (strcmp(id, sWildQueueStateId)) return 0;
    if (!state || stateSize != WILD_QUEUE_STATE_SIZE || !schema || !size) return -1;
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot)
    {
        const u8 *p = state + slot * WILD_QUEUE_RECORD_SIZE;
        if (p[37] || (p[WILD_QUEUE_V3_RECORD_SIZE + 1] & RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL)) return 0;
    }
    *schema = 4; *size = WILD_QUEUE_STATE_SIZE; return 0;
}
static struct WildActorEntry sWildActors[OBJECT_EVENTS_COUNT];
static u64 sWildActorSerial;
static u64 sWildStepSerial;
static const struct RhhWildStepResult *sWildStepContext;
static const struct RhhWildActorEvent *sWildActorContext;
static const struct RhhWildActorIndividual *sWildActorIndividualContext;
static struct { u64 runId, next, end; bool8 failed, confirmed; } sWildActorLease;
struct WildBattleTraitEntry { RhhContentKey tag; bool32 valid; struct RhhWildTraitResult result; };
struct WildBattleTraits { u32 count; struct WildBattleTraitEntry entries[RHH_WILD_BATTLE_TRAIT_LIMIT]; };
struct WildBattleProfile { bool32 valid; u64 individual; u8 record[WILD_PROFILE_RECORD_SIZE]; };
static struct {
    struct RhhWildActorSnapshot actor;
    struct RhhWildActorIndividual individual;
    struct BoxPokemon opponent;
    struct RhhWildActorBattleCatch catchResult;
    struct WildBattleTraits traits;
    struct WildBattleProfile profile;
    u64 runId, battleId;
    u32 flags, opponentSlot;
    bool8 armed, transitioned, opponentValid, catchValid;
} sWildActorBattle;
static const struct RhhWildActorBattleResult *sWildActorBattleContext;
static const struct RhhWildActorBattleCatch *sWildActorBattleCatchContext;
static struct WildBattleTraits sWildBattleEndTraits;
static struct WildBattleProfile sWildBattleEndProfile;
static bool32 sWildBattleEndTraitsValid;
static struct { bool32 valid; u32 battler, opponentSlot; u64 runId, handle;
    struct RhhWildActorSnapshot actor; struct RhhWildActorIndividual individual;
    struct RhhOwnedMonSnapshot before; } sWildActorCatch;
static int WildActorSlot(const struct ObjectEvent *object)
{
    uintptr_t p = (uintptr_t)object, start = (uintptr_t)gObjectEvents;
    if (p < start || p >= start + sizeof(gObjectEvents) || (p - start) % sizeof(*object)) return -1;
    return (p - start) / sizeof(*object);
}
static bool32 WildActorMatches(u32 slot)
{
    const struct ObjectEvent *object = &gObjectEvents[slot];
    return sWildActors[slot].handle && RhhModsHasActiveRun() && sWildActors[slot].runId == RhhModsRunId()
        && object->active && object->trainerType == TRAINER_TYPE_OW_WILD_ENCOUNTER
        && object->localId == sWildActors[slot].localId && object->mapGroup == sWildActors[slot].mapGroup
        && object->mapNum == sWildActors[slot].mapNum && IS_OW_MON_OBJ(object);
}
/* Presentation belongs to the live actor, not a saved offscreen individual.
 * Its native field-effect pulse stays on the tile where it was created. */
static bool32 WildActorWasObserved(u32 slot);
static bool32 WildPresentationEligible(u32 slot)
{
    return WE_OW_ENCOUNTERS && sWildActors[slot].presentationEligible && WildActorMatches(slot);
}
static bool32 WildCueExists(u32 slot)
{
    const struct WildActorEntry *entry = &sWildActors[slot];
    if (!entry->cueSprite || entry->cueSprite > MAX_SPRITES) return FALSE;
    const struct Sprite *cue = &gSprites[entry->cueSprite - 1];
    return cue->inUse && cue->data[7] == (s16)(0x4200 | slot) && !cue->animEnded;
}
static bool32 WildCueVisible(u32 slot)
{
    return WildCueExists(slot) && sWildActors[slot].cueAge >= 2
        && !gSprites[sWildActors[slot].cueSprite - 1].invisible;
}
static void WildCueUpdate(u32 slot)
{
    struct WildActorEntry *entry = &sWildActors[slot];
    const struct ObjectEvent *object = &gObjectEvents[slot];
    if (!WildPresentationEligible(slot) || object->offScreen || object->invisible) return;
    if (WildCueExists(slot) && entry->cueX == object->currentCoords.x && entry->cueY == object->currentCoords.y) return;
    u32 behavior = MapGridGetMetatileBehaviorAt(object->currentCoords.x, object->currentCoords.y);
    bool32 water = MetatileBehavior_IsWaterWildEncounter(behavior);
    u32 anim = MetatileBehavior_IsLongGrass(behavior) ? OWE_SPAWN_ANIM_LONG_GRASS
        : (MetatileBehavior_IsPokeGrass(behavior) || MetatileBehavior_IsAshGrass(behavior))
            ? OWE_SPAWN_ANIM_GRASS : OWE_SPAWN_ANIM_CAVE;
    const struct SpriteTemplate *template = gFieldEffectObjectTemplatePointers[water ? FLDEFFOBJ_RIPPLE
        : gOverworldWildEncounterFieldEffectInfo[anim].visual];
    struct SpritePalette palette = water ? gSpritePalette_GeneralFieldEffect1 : GetOWESpawnDespawnAnimFldEffPalette(anim);
    bool32 freeSprite = FALSE;
    for (u32 i = 0; i < MAX_SPRITES; ++i) if (!gSprites[i].inUse) { freeSprite = TRUE; break; }
    if (!freeSprite || !template || !template->images || !CanAllocSpriteTiles(template->images[0].size / TILE_SIZE_4BPP)
        || (IndexOfSpritePaletteTag(palette.tag) == 0xff && !CountFreePaletteSlots())) { entry->cueSprite = 0; return; }
    s32 arguments[ARRAY_COUNT(gFieldEffectArguments)];
    memcpy(arguments, gFieldEffectArguments, sizeof(arguments));
    u8 used[MAX_SPRITES]; for (u32 i = 0; i < MAX_SPRITES; ++i) used[i] = gSprites[i].inUse;
    u32 sprite = MAX_SPRITES, effect = FLDEFF_OW_ENCOUNTER_SPAWN_ANIM;
    if (water)
    {
        s16 x = object->currentCoords.x, y = object->currentCoords.y;
        SetSpritePosToOffsetMapCoords(&x, &y, 8, 8);
        gFieldEffectArguments[0] = x; gFieldEffectArguments[1] = y;
        gFieldEffectArguments[2] = 151; gFieldEffectArguments[3] = 2;
        effect = FLDEFF_RIPPLE; FieldEffectStart(effect);
        for (u32 i = 0; i < MAX_SPRITES; ++i)
            if (!used[i] && gSprites[i].inUse) { sprite = i; break; }
    }
    else
    {
        gFieldEffectArguments[0] = object->currentCoords.x; gFieldEffectArguments[1] = object->currentCoords.y;
        gFieldEffectArguments[2] = anim; sprite = FieldEffectStart(effect);
    }
    memcpy(gFieldEffectArguments, arguments, sizeof(arguments));
    if (sprite >= MAX_SPRITES || used[sprite] || !gSprites[sprite].inUse) { entry->cueSprite = 0; return; }
    struct Sprite *cue = &gSprites[sprite];
    if (effect == FLDEFF_OW_ENCOUNTER_SPAWN_ANIM)
    { cue->data[0] = object->currentElevation; cue->data[1] = effect; }
    cue->data[7] = 0x4200 | slot;
    entry->cueSprite = sprite + 1; entry->cueAge = 0;
    entry->cueX = object->currentCoords.x; entry->cueY = object->currentCoords.y;
}
/* 0: ordinary sprite, 1: disturbance only, 2: reveal with its lower edge covered. */
u32 ModsWildActorPresentation(u32 spriteId, s32 *coverY)
{
    if (!coverY || spriteId >= MAX_SPRITES || gMain.inBattle || !WE_OW_ENCOUNTERS) return 0;
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
    {
        const struct ObjectEvent *object = &gObjectEvents[i];
        if (!WildPresentationEligible(i) || object->spriteId != spriteId || sWildActors[i].conceal.mode) continue;
        const struct Sprite *sprite = &gSprites[spriteId];
        if (!sprite->inUse) return 0;
        if (!sWildActors[i].observed) return WildCueExists(i) ? 1 : 0;
        const struct ObjectEventGraphicsInfo *graphics = GetObjectEventGraphicsInfo(object->graphicsId);
        *coverY = sprite->y + sprite->y2 + sprite->centerToCornerVecY + graphics->height - 8
            + (sprite->coordOffsetEnabled ? gSpriteCoordOffsetY : 0);
        return 2;
    }
    return 0;
}
bool32 ModsWildActorCueVisible(const struct ObjectEvent *object)
{
    int slot = WildActorSlot(object);
    return slot < 0 || !WildPresentationEligible(slot) || sWildActors[slot].conceal.mode
        || WildCueVisible(slot) || !WildCueExists(slot);
}
bool32 ModsWildActorHidden(const struct ObjectEvent *object)
{
    int slot = WildActorSlot(object);
    return slot >= 0 && WildActorMatches(slot) && sWildActors[slot].conceal.mode == RHH_WILD_CONCEAL_HIDDEN;
}
bool32 ModsWildActorGroundCover(const struct ObjectEvent *object)
{
    int slot = WildActorSlot(object);
    return slot >= 0 && WildActorMatches(slot) && sWildActors[slot].conceal.mode == RHH_WILD_CONCEAL_GROUND;
}
void ModsWildActorNoteStep(struct ObjectEvent *object)
{
    int slot = WildActorSlot(object);
    if (slot >= 0 && WildActorMatches(slot) && sWildActors[slot].conceal.mode
        && (sWildActors[slot].conceal.reveal_flags & RHH_WILD_REVEAL_ON_MOVE))
        sWildActors[slot].concealMoved = TRUE;
}
static bool32 WildConcealStore(u32 slot, const struct ObjectEvent *object, const struct RhhWildConcealment *policy)
{
    u8 *state = WildConcealState();
    if (!state || slot >= OBJECT_EVENTS_COUNT) return FALSE;
    u8 *p = WildConcealRecord(state, slot), old[WILD_CONCEAL_RECORD_SIZE];
    memcpy(old, p, sizeof(old)); memset(p, 0, WILD_CONCEAL_RECORD_SIZE);
    if (policy && policy->mode)
    {
        p[0] = policy->mode; p[1] = policy->reveal_flags; p[2] = policy->reveal_radius;
        p[3] = object->localId; p[4] = object->mapGroup; p[5] = object->mapNum;
        p[6] = object->trainerRange_berryTreeId & 0x7f;
        WildActorWrite64(p + 8, policy->owner); WildActorWrite64(p + 16, sWildActors[slot].individual);
        WildActorWrite64(p + 24, policy->revision); WildActorWrite16(p + 32, object->graphicsId);
    }
    if (ValidateWildConcealState(state, WILD_CONCEAL_STATE_SIZE))
    { memcpy(p, old, sizeof(old)); return FALSE; }
    return TRUE;
}
static bool32 WildActorStore(u32 slot, const struct ObjectEvent *object, const struct Pokemon *mon, u64 individual)
{
    u8 *state = WildActorState();
    if (!state || slot >= OBJECT_EVENTS_COUNT) return FALSE;
    u8 *record = WildActorRecord(state, slot); memset(record, 0, WILD_ACTOR_RECORD_SIZE);
    if (!RhhModsHasActiveRun() || (!mon && !individual)
        || (mon && !IS_LOCALID_GENERATED_OWE(object->localId))) return FALSE;
    record[0] = mon ? 1 : 2; record[1] = object->localId; record[2] = object->mapGroup; record[3] = object->mapNum;
    WildActorWrite16(record + 4, object->graphicsId); record[6] = object->trainerRange_berryTreeId & 0x7f;
    WildActorWrite64(record + 8, individual);
    if (mon) memcpy(record + WILD_ACTOR_RECORD_HEADER, mon, sizeof(*mon));
    if (ValidateWildActorState(state, WILD_ACTOR_STATE_SIZE))
    { memset(record, 0, WILD_ACTOR_RECORD_SIZE); return FALSE; }
    return TRUE;
}
static void WildActorSequenceFailure(void)
{
    if (sWildActorLease.failed) return;
    sWildActorLease.failed = TRUE;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_PERSISTENCE_FAILED,
        .run_seed = RhhModsRunSeed()};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision);
}
/* Reserve IDs in batches before exposing any of them. The durable journal
 * advances independently of ordinary save rollback. This is called outside
 * mod callbacks, so the journal cannot capture a half-mutated observer. */
static u64 WildActorAllocateIndividual(bool32 allowUncommitted)
{
    u8 *state = RhhModsState(sWildActorSequenceId);
    u64 runId = RhhModsRunId();
    if (!state || !RhhModsHasActiveRun() || NativeSaveTransactionActive()
        || (!RhhModsRunCommitted() && !allowUncommitted)) return 0;
    if (sWildActorLease.runId != runId)
    { memset(&sWildActorLease, 0, sizeof(sWildActorLease)); sWildActorLease.runId = runId; }
    if (sWildActorLease.end > WildActorRead64(state))
    { memset(&sWildActorLease, 0, sizeof(sWildActorLease)); sWildActorLease.runId = runId; }
    if (sWildActorLease.failed) return 0;
    if (!sWildActorLease.next || sWildActorLease.next > sWildActorLease.end)
    {
        u64 previous = WildActorRead64(state);
        if (previous > UINT64_MAX - 256) return 0;
        WildActorWrite64(state, previous + 256);
        if (RhhModsRunCommitted())
        {
            if (!RhhModsCanPersistDurably() || RhhModsFlushDurable())
            { WildActorWrite64(state, previous); WildActorSequenceFailure(); return 0; }
            sWildActorLease.confirmed = TRUE;
        }
        else sWildActorLease.confirmed = FALSE;
        sWildActorLease.next = previous + 1; sWildActorLease.end = previous + 256;
    }
    if (RhhModsRunCommitted() && !sWildActorLease.confirmed) return 0;
    return sWildActorLease.next++;
}
static int WildActorAt(u32 slot, struct RhhWildActorSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out) || slot >= OBJECT_EVENTS_COUNT
        || !RhhModsHasActiveRun() || !WildActorMatches(slot)) return -1;
    const struct ObjectEvent *object = &gObjectEvents[slot]; const struct WildActorEntry *entry = &sWildActors[slot];
    RhhContentKey species = RhhContentKeyForId(RHH_SPECIES, OW_SPECIES(object));
    RhhContentKey map = RhhContentKeyForId(RHH_MAP, ((u8)object->mapGroup << 8) | object->mapNum);
    if (!species || !map) return -1;
    struct RhhWildActorSnapshot result = {.struct_size = sizeof(result), .slot = slot, .local_id = object->localId,
        .flags = (IS_LOCALID_GENERATED_OWE(object->localId) ? RHH_WILD_ACTOR_GENERATED : 0)
            | (OW_SHINY(object) ? RHH_WILD_ACTOR_SHINY : 0) | (OW_FEMALE(object) ? RHH_WILD_ACTOR_FEMALE : 0)
            | ((object->invisible || entry->conceal.mode == RHH_WILD_CONCEAL_HIDDEN) ? RHH_WILD_ACTOR_HIDDEN : 0) | (object->offScreen ? RHH_WILD_ACTOR_OFFSCREEN : 0),
        .level = object->trainerRange_berryTreeId & 0x7f, .elevation = object->currentElevation,
        .facing = object->facingDirection, .handle = entry->handle, .map = map, .species = species,
        .x = object->currentCoords.x - MAP_OFFSET, .y = object->currentCoords.y - MAP_OFFSET};
    if (entry->hasMon && entry->graphics == object->graphicsId)
    {
        struct Pokemon mon = entry->mon;
        result.flags |= RHH_WILD_ACTOR_PRESERVED;
        result.identity = (u64)GetMonData(&mon, MON_DATA_OT_ID) << 32 | GetMonData(&mon, MON_DATA_PERSONALITY);
    }
    *out = result; return 0;
}
static int WildActorFind(u64 handle, struct RhhWildActorSnapshot *out)
{
    if (!handle || !out || out->struct_size < sizeof(*out)) return -1;
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
        if (sWildActors[i].handle == handle) return WildActorAt(i, out);
    return -1;
}
static int WildStepPending(u64 handle, struct RhhWildStepResult *out)
{
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    if (!out || out->struct_size < sizeof(*out) || WildActorFind(handle, &actor)
        || !sWildActors[actor.slot].moving || !sWildActors[actor.slot].step.ticket) return -1;
    *out = sWildActors[actor.slot].step; return 0;
}
static int WildStepContext(struct RhhWildStepResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildStepContext
        || RhhModsCurrentEvent() != RHH_WILD_ACTOR_STEP_FINISHED) return -1;
    *out = *sWildStepContext; return 0;
}
static void WildStepNotify(const struct RhhWildStepResult *result)
{
    if (!result || !result->ticket || !RhhModsHasActiveRun() || sWildStepContext
        || result->run_id != RhhModsRunId() || !result->before.handle) return;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WILD_ACTOR_STEP_FINISHED,
        .run_seed = RhhModsRunSeed(), .event_key = result->before.map, .subject = result->before.species,
        .ordinal = result->ticket, .args = {result->status, result->source, result->direction, result->after_valid}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sWildStepContext = result; RhhModsDispatch(&event, &decision); sWildStepContext = NULL;
}
static int WildActorIndividualAt(u32 slot, struct RhhWildActorIndividual *out)
{
    if (!out || out->struct_size < sizeof(*out) || slot >= OBJECT_EVENTS_COUNT || !WildActorMatches(slot)
        || !RhhModsRunCommitted() || !sWildActors[slot].identified || !sWildActors[slot].individual) return -1;
    *out = (struct RhhWildActorIndividual){.struct_size = sizeof(*out),
        .run_id = sWildActors[slot].runId, .serial = sWildActors[slot].individual};
    return 0;
}
static int WildActorIndividual(u64 handle, struct RhhWildActorIndividual *out)
{
    if (!handle || !out || out->struct_size < sizeof(*out)) return -1;
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
        if (sWildActors[i].handle == handle) return WildActorIndividualAt(i, out);
    return -1;
}
/* Prepare the exact archive record before the suspend transaction removes the actor. */
u32 RhhModsWildDormantRecordSize(void) { return WILD_DORMANT_RECORD; }
int RhhModsWildDormantPrepare(u64 handle, RhhContentKey owner, void *out, u32 capacity)
{
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id)};
    if (!out || capacity < WILD_DORMANT_RECORD || !owner || !RhhModsRegisteredKey(owner)
        || NativeSaveTransactionActive() || WildActorFind(handle, &actor) || WildActorIndividual(handle, &id)
        || !(actor.flags & RHH_WILD_ACTOR_GENERATED) || !(actor.flags & RHH_WILD_ACTOR_PRESERVED)
        || !IsOrdinaryGeneratedOverworldWildEncounter(&gObjectEvents[actor.slot])
        || sWildActors[actor.slot].moving || ObjectEventIsMovementOverridden(&gObjectEvents[actor.slot])
        || actor.x < 0 || actor.y < 0 || actor.x > INT16_MAX - MAP_OFFSET || actor.y > INT16_MAX - MAP_OFFSET
        || actor.facing < DIR_SOUTH || actor.facing > DIR_NORTHEAST) return -1;
    u8 buffer[WILD_DORMANT_HEADER + WILD_DORMANT_RECORD] = {0}, *p = buffer + WILD_DORMANT_HEADER;
    memcpy(buffer, "RHWDOR02", 8); WildActorWrite32(buffer + 8, sizeof(buffer)); WildActorWrite32(buffer + 12, 1);
    WildActorWrite64(buffer + 16, id.run_id); WildActorWrite64(p, id.serial); WildActorWrite64(p + 8, owner);
    WildActorWrite64(p + 16, actor.map); WildActorWrite32(p + 24, actor.x); WildActorWrite32(p + 28, actor.y);
    p[32] = actor.facing; p[33] = actor.elevation; p[34] = !!(gObjectEvents[actor.slot].trainerRange_berryTreeId & 0x80);
    const struct RhhWorldClockService *clock = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldClockSnapshot now = {.struct_size = sizeof(now)};
    if (!clock || clock->current(&now) || now.run_id != id.run_id) return -1;
    WildActorWrite64(p + WILD_DORMANT_CLOCK, now.steps);
    memcpy(p + WILD_DORMANT_MON, &sWildActors[actor.slot].mon, sizeof(struct Pokemon));
    u8 *states[] = {WildTraitState(), WildProfileState(), WildActivityState(), WildQueueState()};
    const u32 sourceLengths[] = {WILD_TRAIT_RECORD_SIZE, WILD_PROFILE_RECORD_SIZE, WILD_ACTIVITY_RECORD_SIZE, WILD_QUEUE_RECORD_SIZE};
    const u32 offsets[] = {WILD_DORMANT_TRAIT, WILD_DORMANT_PROFILE, WILD_DORMANT_ACTIVITY, WILD_DORMANT_QUEUE};
    for (u32 i = 0; i < ARRAY_COUNT(states); ++i)
    {
        if (!states[i]) return -1;
        const u8 *record = states[i] + actor.slot * sourceLengths[i];
        if (WildActorRead64(record) == id.serial) memcpy(p + offsets[i], record, sourceLengths[i]);
    }
    u8 *conceal = WildConcealState(); if (!conceal) return -1;
    const u8 *record = WildConcealRecord(conceal, actor.slot);
    if (sWildActors[actor.slot].conceal.mode) {
        if (!record[0] || WildActorRead64(record + 16) != id.serial) return -1;
        memcpy(p + WILD_DORMANT_CONCEAL, record, WILD_CONCEAL_RECORD_SIZE);
    }
    /* The source remains live during preparation, but must be removed before
     * the record can be accepted as an archive. */
    if (WildDormantValidateInternal(buffer, sizeof(buffer), id.serial)) return -1;
    memcpy(out, p, WILD_DORMANT_RECORD); return 0;
}
_Static_assert(OBJECT_EVENTS_COUNT <= RHH_WILD_NEIGHBOR_CAPACITY, "wild neighbor capacity must cover object slots");
static int CompareWildNeighbors(const void *left, const void *right)
{
    const struct RhhWildNeighbor *a = left, *b = right;
    if (a->distance != b->distance) return a->distance < b->distance ? -1 : 1;
    if (a->individual_available != b->individual_available) return a->individual_available ? -1 : 1;
    if (a->individual_available)
    {
        if (a->individual.run_id != b->individual.run_id) return a->individual.run_id < b->individual.run_id ? -1 : 1;
        if (a->individual.serial != b->individual.serial) return a->individual.serial < b->individual.serial ? -1 : 1;
    }
    return (a->actor.slot > b->actor.slot) - (a->actor.slot < b->actor.slot);
}
static int WildActorNeighbors(const struct RhhWildNeighborRequest *request, struct RhhWildNeighborList *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || request->radius > 64
        || (request->flags & ~(RHH_WILD_NEIGHBOR_SAME_SPECIES | RHH_WILD_NEIGHBOR_IDENTIFIED))
        || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhWildNeighborList result = {.struct_size = sizeof(result), .anchor.struct_size = sizeof(result.anchor)};
    if (WildActorFind(request->anchor_handle, &result.anchor)) return -1;
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
    {
        struct RhhWildNeighbor neighbor = {.actor.struct_size = sizeof(neighbor.actor),
            .individual.struct_size = sizeof(neighbor.individual)};
        if (WildActorAt(i, &neighbor.actor) || neighbor.actor.handle == result.anchor.handle
            || neighbor.actor.map != result.anchor.map
            || ((request->flags & RHH_WILD_NEIGHBOR_SAME_SPECIES) && neighbor.actor.species != result.anchor.species)) continue;
        int64_t dx = (int64_t)neighbor.actor.x - result.anchor.x, dy = (int64_t)neighbor.actor.y - result.anchor.y;
        u64 distance = (u64)(dx < 0 ? -dx : dx) + (u64)(dy < 0 ? -dy : dy);
        if (distance > request->radius) continue;
        neighbor.distance = (u32)distance;
        neighbor.individual_available = !WildActorIndividualAt(i, &neighbor.individual);
        if (!neighbor.individual_available) memset(&neighbor.individual, 0, sizeof(neighbor.individual));
        if ((request->flags & RHH_WILD_NEIGHBOR_IDENTIFIED) && !neighbor.individual_available) continue;
        result.neighbors[result.count++] = neighbor;
    }
    qsort(result.neighbors, result.count, sizeof(result.neighbors[0]), CompareWildNeighbors);
    *out = result;
    return 0;
}
struct WildRouteNode { s16 x, y; u16 length, parent; u8 elevation, first, facing, direction; };
static int HabitatSample(int32_t x, int32_t y, struct RhhHabitatSample *out);
typedef int (*WildRouteGoal)(s32 x, s32 y, void *context);
struct WildRoutePointGoal { s32 x, y; u32 stop_distance; };
static int WildRoutePointMatches(s32 x, s32 y, void *context)
{
    const struct WildRoutePointGoal *goal = context;
    s64 dx = (s64)x - goal->x, dy = (s64)y - goal->y;
    return (u64)(dx < 0 ? -dx : dx) + (u64)(dy < 0 ? -dy : dy) <= goal->stop_distance;
}
_Static_assert((int)DIR_SOUTH == (int)RHH_WILD_ACTOR_SOUTH && (int)DIR_NORTH == (int)RHH_WILD_ACTOR_NORTH
    && (int)DIR_WEST == (int)RHH_WILD_ACTOR_WEST && (int)DIR_EAST == (int)RHH_WILD_ACTOR_EAST
    && (int)DIR_SOUTHWEST == (int)RHH_WILD_ACTOR_SOUTHWEST && (int)DIR_SOUTHEAST == (int)RHH_WILD_ACTOR_SOUTHEAST
    && (int)DIR_NORTHWEST == (int)RHH_WILD_ACTOR_NORTHWEST && (int)DIR_NORTHEAST == (int)RHH_WILD_ACTOR_NORTHEAST,
    "wild route directions must match engine directions");
static void WildRouteTraceEmpty(struct RhhWildRouteTrace *trace)
{
    if (trace) *trace = (struct RhhWildRouteTrace){.struct_size = sizeof(*trace)};
}
static void WildRouteTraceFound(struct RhhWildRouteTrace *trace, const struct WildRouteNode *queue,
                                u32 parent, u32 length, const struct RhhWildRouteStep *last)
{
    if (!trace) return;
    struct RhhWildRouteTrace result = {.struct_size = sizeof(result), .count = length};
    result.steps[length - 1] = *last;
    for (u32 i = length - 1; i > 0; --i)
    {
        const struct WildRouteNode *node = &queue[parent];
        result.steps[i - 1] = (struct RhhWildRouteStep){.x = node->x, .y = node->y,
            .direction = node->direction, .elevation = node->elevation, .facing = node->facing};
        parent = node->parent;
    }
    *trace = result;
}
static int WildActorRouteFind(u64 handle, WildRouteGoal goal, void *goalContext, struct RhhWildRouteResult *out,
                              struct RhhWildRouteTrace *trace)
{
    if (!goal || !out || out->struct_size < sizeof(*out) || (trace && trace->struct_size < sizeof(*trace)) || !Ready()) return -1;
    struct RhhWildRouteResult result = {.struct_size = sizeof(result), .actor.struct_size = sizeof(result.actor)};
    if (WildActorFind(handle, &result.actor) || result.actor.x < 0 || result.actor.y < 0
        || result.actor.x > INT16_MAX - MAP_OFFSET || result.actor.y > INT16_MAX - MAP_OFFSET
        || result.actor.x >= gMapHeader.mapLayout->width || result.actor.y >= gMapHeader.mapLayout->height
        || result.actor.map != RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8)
            | (u8)gSaveBlock1Ptr->location.mapNum)) return -1;
    const struct ObjectEvent *actor = &gObjectEvents[result.actor.slot];
    if (actor->spriteId >= MAX_SPRITES || !gSprites[actor->spriteId].inUse
        || gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT || !gObjectEvents[gPlayerAvatar.objectEventId].active
        || sWildActors[result.actor.slot].moving || ObjectEventIsMovementOverridden((struct ObjectEvent *)actor)
        || actor->currentCoords.x != actor->previousCoords.x || actor->currentCoords.y != actor->previousCoords.y) return -1;
    int goalResult = goal(result.actor.x, result.actor.y, goalContext);
    if (goalResult < 0) return -1;
    if (goalResult)
    { result.status = RHH_WILD_ROUTE_ALREADY_THERE; result.visited = 1; WildRouteTraceEmpty(trace); *out = result; return 0; }
    enum { WIDTH = 2 * RHH_WILD_ROUTE_RADIUS + 1 };
    u16 seen[WIDTH][WIDTH][16] = {{{0}}};
    struct WildRouteNode queue[RHH_WILD_ROUTE_NODE_LIMIT];
    u32 head = 0, tail = 1; bool32 truncated = FALSE;
    queue[0] = (struct WildRouteNode){.x = result.actor.x, .y = result.actor.y,
        .elevation = actor->currentElevation, .facing = actor->facingDirection};
    seen[RHH_WILD_ROUTE_RADIUS][RHH_WILD_ROUTE_RADIUS][actor->currentElevation]
        = WE_OWE_UNRESTRICT_SIGHT ? 1u << actor->facingDirection : 1;
    static const u8 directions[] = {DIR_NORTH, DIR_WEST, DIR_EAST, DIR_SOUTH};
    while (head < tail)
    {
        struct WildRouteNode node = queue[head++];
        for (u32 i = 0; i < ARRAY_COUNT(directions); ++i)
        {
            u8 direction = directions[i];
            s32 x = node.x + gDirectionToVectors[direction].x, y = node.y + gDirectionToVectors[direction].y;
            s32 rx = x - result.actor.x, ry = y - result.actor.y;
            if (x < 0 || y < 0 || x >= gMapHeader.mapLayout->width || y >= gMapHeader.mapLayout->height
                || x > INT16_MAX - MAP_OFFSET || y > INT16_MAX - MAP_OFFSET
                || rx < -(s32)RHH_WILD_ROUTE_RADIUS || rx > (s32)RHH_WILD_ROUTE_RADIUS
                || ry < -(s32)RHH_WILD_ROUTE_RADIUS || ry > (s32)RHH_WILD_ROUTE_RADIUS) continue;
            u8 overwrite = DIR_NONE;
            u8 behavior = node.length ? MapGridGetMetatileBehaviorAt(node.x + MAP_OFFSET, node.y + MAP_OFFSET)
                                      : actor->currentMetatileBehavior;
            if (CheckRestrictedOWEMovementFromNoEffects(actor, node.x + MAP_OFFSET, node.y + MAP_OFFSET,
                                                        node.elevation, behavior, node.facing, direction, &overwrite)) continue;
            /* The collision check returns the actual direction used by the
             * walking animation on sideways stairs. The command itself still
             * takes the cardinal input that triggered that transition. */
            u8 movement = overwrite != DIR_NONE ? overwrite : direction;
            x = node.x + gDirectionToVectors[movement].x; y = node.y + gDirectionToVectors[movement].y;
            rx = x - result.actor.x; ry = y - result.actor.y;
            if (x < 0 || y < 0 || x >= gMapHeader.mapLayout->width || y >= gMapHeader.mapLayout->height
                || x > INT16_MAX - MAP_OFFSET || y > INT16_MAX - MAP_OFFSET
                || rx < -(s32)RHH_WILD_ROUTE_RADIUS || rx > (s32)RHH_WILD_ROUTE_RADIUS
                || ry < -(s32)RHH_WILD_ROUTE_RADIUS || ry > (s32)RHH_WILD_ROUTE_RADIUS) continue;
            u8 elevation = MapGridGetElevationAt(x + MAP_OFFSET, y + MAP_OFFSET);
            if (elevation == ELEVATION_MULTI_LEVEL
                || MapGridGetElevationAt(node.x + MAP_OFFSET, node.y + MAP_OFFSET) == ELEVATION_MULTI_LEVEL)
                elevation = node.elevation;
            u8 facing = actor->facingDirectionLocked ? node.facing : movement;
            u16 facingBit = WE_OWE_UNRESTRICT_SIGHT ? 1u << facing : 1;
            if (elevation >= 16 || (seen[rx + RHH_WILD_ROUTE_RADIUS][ry + RHH_WILD_ROUTE_RADIUS][elevation] & facingBit)) continue;
            u8 first = node.length ? node.first : direction;
            u32 length = node.length + 1;
            goalResult = goal(x, y, goalContext);
            if (goalResult < 0) return -1;
            if (goalResult)
            {
                result.status = RHH_WILD_ROUTE_FOUND; result.first_direction = first;
                result.length = length; result.visited = tail + (tail < RHH_WILD_ROUTE_NODE_LIMIT);
                struct RhhWildRouteStep last = {.x = x, .y = y, .direction = direction, .elevation = elevation, .facing = facing};
                WildRouteTraceFound(trace, queue, head - 1, length, &last);
                *out = result; return 0;
            }
            if (tail == RHH_WILD_ROUTE_NODE_LIMIT) { truncated = TRUE; continue; }
            seen[rx + RHH_WILD_ROUTE_RADIUS][ry + RHH_WILD_ROUTE_RADIUS][elevation] |= facingBit;
            queue[tail++] = (struct WildRouteNode){.x = x, .y = y, .length = length, .elevation = elevation,
                .first = first, .facing = facing, .parent = head - 1, .direction = direction};
        }
    }
    result.status = truncated ? RHH_WILD_ROUTE_LIMIT : RHH_WILD_ROUTE_NO_PATH;
    result.visited = tail; WildRouteTraceEmpty(trace); *out = result; return 0;
}
static int WildActorRouteImpl(const struct RhhWildRouteRequest *request, struct RhhWildRouteResult *out,
                              struct RhhWildRouteTrace *trace)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved[0] || request->reserved[1]
        || request->stop_distance > 1 || !out || out->struct_size < sizeof(*out) || !Ready()
        || request->target_x < 0 || request->target_y < 0
        || request->target_x >= gMapHeader.mapLayout->width || request->target_y >= gMapHeader.mapLayout->height
        || request->target_x > INT16_MAX - MAP_OFFSET || request->target_y > INT16_MAX - MAP_OFFSET) return -1;
    struct WildRoutePointGoal goal = {.x = request->target_x, .y = request->target_y, .stop_distance = request->stop_distance};
    return WildActorRouteFind(request->actor_handle, WildRoutePointMatches, &goal, out, trace);
}
static int WildActorRoute(const struct RhhWildRouteRequest *request, struct RhhWildRouteResult *out)
{ return WildActorRouteImpl(request, out, NULL); }
static int WildActorRouteTrace(const struct RhhWildRouteRequest *request, struct RhhWildRouteResult *out,
                               struct RhhWildRouteTrace *trace)
{ return trace ? WildActorRouteImpl(request, out, trace) : -1; }
enum { WILD_HABITAT_ROUTE_WIDTH = 2 * RHH_WILD_HABITAT_ROUTE_MAX_RADIUS + 1 };
struct WildHabitatRouteGoal {
    const struct RhhWildHabitatRouteRequest *request;
    s32 origin_x, origin_y;
    u8 checked[WILD_HABITAT_ROUTE_WIDTH][WILD_HABITAT_ROUTE_WIDTH];
    u32 sampled, denied;
    struct RhhHabitatSample target;
};
static int WildHabitatRouteMatches(s32 x, s32 y, void *context)
{
    struct WildHabitatRouteGoal *goal = context;
    const struct RhhWildHabitatRouteRequest *request = goal->request;
    static const s8 offsets[][2] = {{0, 0}, {0, -1}, {-1, 0}, {1, 0}, {0, 1}};
    u32 count = request->stop_distance ? ARRAY_COUNT(offsets) : 1;
    for (u32 i = 0; i < count; ++i)
    {
        s32 tx = x + offsets[i][0], ty = y + offsets[i][1];
        s32 dx = tx - goal->origin_x, dy = ty - goal->origin_y;
        if (tx < 0 || ty < 0 || tx >= gMapHeader.mapLayout->width || ty >= gMapHeader.mapLayout->height
            || (u32)(abs(dx) + abs(dy)) > request->radius) continue;
        u32 cx = dx + RHH_WILD_HABITAT_ROUTE_MAX_RADIUS, cy = dy + RHH_WILD_HABITAT_ROUTE_MAX_RADIUS;
        if (cx >= WILD_HABITAT_ROUTE_WIDTH || cy >= WILD_HABITAT_ROUTE_WIDTH || goal->checked[cx][cy]) continue;
        struct RhhHabitatSample sample = {.struct_size = sizeof(sample)};
        if (HabitatSample(tx, ty, &sample)) return -1;
        goal->checked[cx][cy] = 1; ++goal->sampled;
        if (!sample.permitted) { ++goal->denied; continue; }
        if ((sample.flags & request->all_flags) != request->all_flags
            || (request->any_flags && !(sample.flags & request->any_flags))
            || (sample.flags & request->exclude_flags)) continue;
        goal->target = sample;
        return 1;
    }
    return 0;
}
static int WildActorRouteHabitatImpl(const struct RhhWildHabitatRouteRequest *request, struct RhhWildHabitatRouteResult *out,
                                    struct RhhWildRouteTrace *trace)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || request->reserved2
        || request->stop_distance > 1 || request->radius > RHH_WILD_HABITAT_ROUTE_MAX_RADIUS
        || ((request->all_flags | request->any_flags | request->exclude_flags) & ~RHH_HABITAT_FLAGS_ALL)
        || !out || out->struct_size < sizeof(*out) || (trace && trace->struct_size < sizeof(*trace)) || sHabitatContext) return -1;
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    if (WildActorFind(request->actor_handle, &actor)) return -1;
    struct WildHabitatRouteGoal goal = {.request = request, .origin_x = actor.x, .origin_y = actor.y};
    struct RhhWildHabitatRouteResult result = {.struct_size = sizeof(result), .route.struct_size = sizeof(result.route)};
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    int status = WildActorRouteFind(request->actor_handle, WildHabitatRouteMatches, &goal, &result.route, trace);
    RhhModsWorldTimeScopeEnd(&timeScope);
    if (status) return -1;
    result.sampled = goal.sampled; result.denied = goal.denied; result.target = goal.target;
    *out = result; return 0;
}
static int WildActorRouteHabitat(const struct RhhWildHabitatRouteRequest *request, struct RhhWildHabitatRouteResult *out)
{ return WildActorRouteHabitatImpl(request, out, NULL); }
static int WildActorRouteHabitatTrace(const struct RhhWildHabitatRouteRequest *request, struct RhhWildHabitatRouteResult *out,
                                      struct RhhWildRouteTrace *trace)
{ return trace ? WildActorRouteHabitatImpl(request, out, trace) : -1; }
static int WildActorCommandReady(void);
static const struct RhhWildTraitEditResult *sWildTraitEditContext;
static bool8 sWildTraitEditBusy;
static int WildTraitOverrideAt(u64 handle, RhhContentKey tag, struct RhhWildTraitOverride *out)
{
    const struct RhhEcologyTagsService *tags = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/ecology_tags", RHH_SERVICE_VERSION(1, 0));
    struct RhhEcologyTag info = {.struct_size = sizeof(info)};
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id)};
    u8 *state = WildTraitState();
    if (!out || out->struct_size < sizeof(*out) || !state || !tags || tags->find(tag, &info)
        || WildActorFind(handle, &actor) || WildActorIndividual(handle, &id)) return -1;
    struct RhhWildTraitOverride result = {.struct_size = sizeof(result), .tag = tag};
    const u8 *p = WildTraitRecord(state, actor.slot);
    if (WildActorRead64(p) == id.serial)
    {
        result.revision = WildActorRead64(p + 8);
        for (u32 i = 0; i < RHH_WILD_TRAIT_OVERRIDE_CAPACITY; ++i)
        {
            const u8 *item = p + WILD_TRAIT_HEADER + i * WILD_TRAIT_ITEM_SIZE;
            if (WildActorRead64(item) == tag && RhhModsRegisteredKey(WildActorRead64(item + 8)))
            { result.owner = WildActorRead64(item + 8); result.value = item[16]; break; }
        }
    }
    *out = result; return 0;
}
static int WildTraitEditContext(struct RhhWildTraitEditResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildTraitEditContext
        || (event != RHH_CAN_EDIT_WILD_TRAIT && event != RHH_WILD_TRAIT_EDITED)) return -1;
    *out = *sWildTraitEditContext; return 0;
}
static int WildTraitEditFailure(struct RhhWildTraitEditResult *out, u32 status, const char *reason)
{
    out->status = status; if (reason) snprintf(out->reason, sizeof(out->reason), "%s", reason); return status;
}
static void WildTraitEditDispatch(u32 kind, struct RhhWildTraitEditResult *result, struct RhhModDecision *decision)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = result->after.tag, .subject = result->actor.species, .ordinal = result->individual.serial,
        .args = {result->before.value, result->after.value, result->is_preview, 0}};
    const struct RhhWildTraitEditResult *previous = sWildTraitEditContext;
    sWildTraitEditContext = result; RhhModsDispatch(&event, decision); sWildTraitEditContext = previous;
}
static int WildTraitEdit(const struct RhhWildTraitEditRequest *request, struct RhhWildTraitEditResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WILD_TRAIT_EDIT_INVALID;
    *out = (struct RhhWildTraitEditResult){.struct_size = sizeof(*out), .is_preview = preview,
        .actor.struct_size = sizeof(out->actor), .individual.struct_size = sizeof(out->individual),
        .before.struct_size = sizeof(out->before), .after.struct_size = sizeof(out->after)};
    if (!request || request->struct_size < sizeof(*request) || !request->actor_handle || !request->tag
        || !request->owner || !RhhModsRegisteredKey(request->owner) || request->value > RHH_WILD_TRAIT_ON)
        return WildTraitEditFailure(out, RHH_WILD_TRAIT_EDIT_INVALID, "invalid trait edit");
    if (sWildTraitEditBusy || !WildActorCommandReady() || NativeSaveTransactionActive())
        return WildTraitEditFailure(out, RHH_WILD_TRAIT_EDIT_UNAVAILABLE, "overworld commands unavailable");
    if (WildActorFind(request->actor_handle, &out->actor) || WildActorIndividual(request->actor_handle, &out->individual)
        || WildTraitOverrideAt(request->actor_handle, request->tag, &out->before))
        return WildTraitEditFailure(out, RHH_WILD_TRAIT_EDIT_INVALID, "unknown trait or unidentified actor");
    if (out->actor.map != RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum))
        return WildTraitEditFailure(out, RHH_WILD_TRAIT_EDIT_UNAVAILABLE, "actor is not on the current map");
    out->after = out->before;
    if (request->expected_revision != out->before.revision)
        return WildTraitEditFailure(out, RHH_WILD_TRAIT_EDIT_STALE, "trait revision changed");
    if (out->before.owner && out->before.owner != request->owner)
        return WildTraitEditFailure(out, RHH_WILD_TRAIT_EDIT_CONFLICT, "trait belongs to another module");
    if (out->before.value == request->value) return RHH_WILD_TRAIT_EDIT_OK;
    if (out->before.revision == UINT64_MAX)
        return WildTraitEditFailure(out, RHH_WILD_TRAIT_EDIT_UNAVAILABLE, "trait revision exhausted");
    u8 *state = WildTraitState(), *p = WildTraitRecord(state, out->actor.slot), staged[WILD_TRAIT_RECORD_SIZE];
    if (WildActorRead64(p) == out->individual.serial) memcpy(staged, p, sizeof(staged));
    else memset(staged, 0, sizeof(staged));
    u32 count = 0, index = RHH_WILD_TRAIT_OVERRIDE_CAPACITY;
    while (count < RHH_WILD_TRAIT_OVERRIDE_CAPACITY
        && WildActorRead64(staged + WILD_TRAIT_HEADER + count * WILD_TRAIT_ITEM_SIZE))
    {
        if (WildActorRead64(staged + WILD_TRAIT_HEADER + count * WILD_TRAIT_ITEM_SIZE) == request->tag) index = count;
        ++count;
    }
    if (index == RHH_WILD_TRAIT_OVERRIDE_CAPACITY && request->value && count == RHH_WILD_TRAIT_OVERRIDE_CAPACITY)
        return WildTraitEditFailure(out, RHH_WILD_TRAIT_EDIT_FULL, "actor trait capacity reached");
    if (index < count)
    {
        memmove(staged + WILD_TRAIT_HEADER + index * WILD_TRAIT_ITEM_SIZE,
                staged + WILD_TRAIT_HEADER + (index + 1) * WILD_TRAIT_ITEM_SIZE, (count - index - 1) * WILD_TRAIT_ITEM_SIZE);
        memset(staged + WILD_TRAIT_HEADER + (--count) * WILD_TRAIT_ITEM_SIZE, 0, WILD_TRAIT_ITEM_SIZE);
    }
    if (request->value)
    {
        index = 0;
        while (index < count && WildActorRead64(staged + WILD_TRAIT_HEADER + index * WILD_TRAIT_ITEM_SIZE) < request->tag) ++index;
        u8 *item = staged + WILD_TRAIT_HEADER + index * WILD_TRAIT_ITEM_SIZE;
        memmove(item + WILD_TRAIT_ITEM_SIZE, item, (count - index) * WILD_TRAIT_ITEM_SIZE);
        memset(item, 0, WILD_TRAIT_ITEM_SIZE); WildActorWrite64(item, request->tag);
        WildActorWrite64(item + 8, request->owner); item[16] = request->value;
    }
    out->after.value = request->value; out->after.owner = request->value ? request->owner : 0; ++out->after.revision;
    WildActorWrite64(staged, out->individual.serial); WildActorWrite64(staged + 8, out->after.revision);
    sWildTraitEditBusy = TRUE;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    WildTraitEditDispatch(RHH_CAN_EDIT_WILD_TRAIT, out, &decision);
    if (!decision.allowed)
    { sWildTraitEditBusy = FALSE; return WildTraitEditFailure(out, RHH_WILD_TRAIT_EDIT_DENIED, decision.reason); }
    if (!preview)
    {
        u8 old[WILD_TRAIT_RECORD_SIZE]; memcpy(old, p, sizeof(old)); memcpy(p, staged, sizeof(staged));
        if (ValidateWildTraitState(state, WILD_TRAIT_STATE_SIZE))
        { memcpy(p, old, sizeof(old)); sWildTraitEditBusy = FALSE; return WildTraitEditFailure(out, RHH_WILD_TRAIT_EDIT_UNAVAILABLE, "cannot store trait"); }
        decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
        WildTraitEditDispatch(RHH_WILD_TRAIT_EDITED, out, &decision);
    }
    sWildTraitEditBusy = FALSE; return RHH_WILD_TRAIT_EDIT_OK;
}
static int WildTraitEditPreview(const struct RhhWildTraitEditRequest *r, struct RhhWildTraitEditResult *o)
{ return WildTraitEdit(r, o, TRUE); }
static int WildTraitEditApply(const struct RhhWildTraitEditRequest *r, struct RhhWildTraitEditResult *o)
{ return WildTraitEdit(r, o, FALSE); }
static const struct RhhWildTraitEditsService sWildTraitEditsService = {
    {sizeof(sWildTraitEditsService), RHH_SERVICE_VERSION(1, 0)}, WildTraitOverrideAt, WildTraitEditPreview, WildTraitEditApply, WildTraitEditContext
};
static const struct RhhWildProfileResult *sWildProfileContext;
static const struct RhhWildProfileBatchResult *sWildProfileBatchContext;
static bool8 sWildProfileBusy;
static struct RhhWildProfileSchema sWildProfileSchemaRequest, sWildProfileSchemaOffers[RHH_WILD_PROFILE_SCHEMA_CAPACITY];
static u32 sWildProfileSchemaCount;
static bool8 sWildProfileSchemaBusy, sWildProfileSchemaInvalid;
static int WildProfileSchemaContext(struct RhhWildProfileSchema *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildProfileSchemaBusy
        || (RhhModsCurrentEvent() != RHH_WILD_PROFILE_SCHEMA
            && RhhModsCurrentEvent() != RHH_WILD_PROFILE_SCHEMA_COLLECT)) return -1;
    *out = sWildProfileSchemaRequest; return 0;
}
static int WildProfileOfferSchema(const struct RhhWildProfileSchema *schema)
{
    if (!sWildProfileSchemaBusy || (RhhModsCurrentEvent() != RHH_WILD_PROFILE_SCHEMA
        && RhhModsCurrentEvent() != RHH_WILD_PROFILE_SCHEMA_COLLECT)
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return -1;
    bool32 valid = schema && schema->struct_size >= sizeof(*schema) && !schema->reserved[0] && !schema->reserved[1]
        && schema->owner == sWildProfileSchemaRequest.owner && schema->field
        && (!sWildProfileSchemaRequest.field || schema->field == sWildProfileSchemaRequest.field)
        && (schema->kind == RHH_WILD_PROFILE_SIGNED || schema->kind == RHH_WILD_PROFILE_BOOLEAN)
        && schema->minimum <= schema->maximum && schema->default_value >= schema->minimum
        && schema->default_value <= schema->maximum
        && (schema->kind != RHH_WILD_PROFILE_BOOLEAN || (schema->minimum == 0 && schema->maximum == 1));
    size_t length = valid ? strnlen(schema->label, sizeof(schema->label)) : 0;
    if (!length || length == sizeof(schema->label)) valid = FALSE;
    for (size_t i = 0; valid && i < length; ++i)
        if ((unsigned char)schema->label[i] < 32 || (unsigned char)schema->label[i] > 126) valid = FALSE;
    if (!valid) { sWildProfileSchemaInvalid = TRUE; return -1; }
    struct RhhWildProfileSchema value = {.struct_size = sizeof(value), .kind = schema->kind,
        .owner = schema->owner, .field = schema->field, .minimum = schema->minimum,
        .maximum = schema->maximum, .default_value = schema->default_value};
    memcpy(value.label, schema->label, length);
    u32 index = 0;
    while (index < sWildProfileSchemaCount && sWildProfileSchemaOffers[index].field < value.field) ++index;
    if (index < sWildProfileSchemaCount && sWildProfileSchemaOffers[index].field == value.field)
    {
        if (memcmp(&value, &sWildProfileSchemaOffers[index], sizeof(value)))
        { sWildProfileSchemaInvalid = TRUE; return -1; }
        return 0;
    }
    if (sWildProfileSchemaCount == RHH_WILD_PROFILE_SCHEMA_CAPACITY)
    { sWildProfileSchemaInvalid = TRUE; return -1; }
    memmove(&sWildProfileSchemaOffers[index + 1], &sWildProfileSchemaOffers[index],
            (sWildProfileSchemaCount - index) * sizeof(value));
    sWildProfileSchemaOffers[index] = value; ++sWildProfileSchemaCount; return 0;
}
static int WildProfileSchemaCollect(RhhContentKey owner, RhhContentKey field, u32 kind)
{
    sWildProfileSchemaCount = 0;
    if (!RhhModsHasHook(kind)) return RHH_WILD_PROFILE_SCHEMA_FOUND;
    sWildProfileSchemaRequest = (struct RhhWildProfileSchema){.struct_size = sizeof(sWildProfileSchemaRequest),
        .owner = owner, .field = field};
    sWildProfileSchemaInvalid = FALSE; sWildProfileSchemaBusy = TRUE;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind,
        .run_seed = RhhModsRunSeed(), .event_key = field, .subject = owner};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision); sWildProfileSchemaBusy = FALSE;
    if (!decision.allowed || sWildProfileSchemaInvalid) return RHH_WILD_PROFILE_SCHEMA_CONFLICT;
    return RHH_WILD_PROFILE_SCHEMA_FOUND;
}
static int WildProfileSchema(RhhContentKey owner, RhhContentKey field, struct RhhWildProfileSchema *out)
{
    if (!out || out->struct_size < sizeof(*out) || !owner || !field || sWildProfileSchemaBusy)
        return RHH_WILD_PROFILE_SCHEMA_INVALID;
    int status = WildProfileSchemaCollect(owner, field, RHH_WILD_PROFILE_SCHEMA);
    if (status != RHH_WILD_PROFILE_SCHEMA_FOUND) return status;
    if (!sWildProfileSchemaCount) return RHH_WILD_PROFILE_SCHEMA_NOT_FOUND;
    *out = sWildProfileSchemaOffers[0]; return RHH_WILD_PROFILE_SCHEMA_FOUND;
}
static int WildProfileSchemaList(RhhContentKey owner, struct RhhWildProfileSchemaList *out)
{
    if (!out || out->struct_size < sizeof(*out) || !owner || sWildProfileSchemaBusy)
        return RHH_WILD_PROFILE_SCHEMA_INVALID;
    int status = WildProfileSchemaCollect(owner, 0, RHH_WILD_PROFILE_SCHEMA_COLLECT);
    if (status != RHH_WILD_PROFILE_SCHEMA_FOUND) return status;
    struct RhhWildProfileSchemaList result = {.struct_size = sizeof(result), .count = sWildProfileSchemaCount, .owner = owner};
    memcpy(result.fields, sWildProfileSchemaOffers, result.count * sizeof(result.fields[0]));
    for (u32 i = 0; i < result.count; ++i)
    {
        struct RhhWildProfileSchema single = {.struct_size = sizeof(single)};
        if (WildProfileSchema(owner, result.fields[i].field, &single) != RHH_WILD_PROFILE_SCHEMA_FOUND
            || memcmp(&single, &result.fields[i], sizeof(single))) return RHH_WILD_PROFILE_SCHEMA_CONFLICT;
    }
    *out = result; return RHH_WILD_PROFILE_SCHEMA_FOUND;
}
static int WildProfileValueInSchema(RhhContentKey owner, RhhContentKey field, int64_t value)
{
    struct RhhWildProfileSchema schema = {.struct_size = sizeof(schema)};
    int status = WildProfileSchema(owner, field, &schema);
    if (status == RHH_WILD_PROFILE_SCHEMA_NOT_FOUND) return RHH_WILD_PROFILE_OK;
    if (status != RHH_WILD_PROFILE_SCHEMA_FOUND) return RHH_WILD_PROFILE_UNAVAILABLE;
    return value < schema.minimum || value > schema.maximum ? RHH_WILD_PROFILE_INVALID : RHH_WILD_PROFILE_OK;
}
static void WildProfileRead(const u8 *p, u64 serial, RhhContentKey owner, RhhContentKey field,
                            struct RhhWildProfileValue *out)
{
    struct RhhWildProfileValue result = {.struct_size = sizeof(result), .owner = owner, .field = field,
        .owner_available = RhhModsRegisteredKey(owner)};
    if (WildActorRead64(p) == serial)
    {
        result.revision = WildActorRead64(p + 8);
        for (u32 i = 0; i < RHH_WILD_PROFILE_CAPACITY; ++i)
        {
            const u8 *item = p + WILD_PROFILE_HEADER + i * WILD_PROFILE_ITEM_SIZE;
            if (WildActorRead64(item) == owner && WildActorRead64(item + 8) == field)
            {
                u64 bits = WildActorRead64(item + 16);
                result.present = TRUE; memcpy(&result.value, &bits, sizeof(bits)); break;
            }
        }
    }
    *out = result;
}
static int WildProfileAt(u64 handle, RhhContentKey owner, RhhContentKey field, struct RhhWildProfileValue *out)
{
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id)};
    u8 *state = WildProfileState();
    if (!out || out->struct_size < sizeof(*out) || !owner || !field || !state
        || WildActorFind(handle, &actor) || WildActorIndividual(handle, &id)) return -1;
    WildProfileRead(WildProfileRecord(state, actor.slot), id.serial, owner, field, out); return 0;
}
static int WildProfileContext(struct RhhWildProfileResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildProfileContext
        || (event != RHH_CAN_EDIT_WILD_PROFILE && event != RHH_WILD_PROFILE_EDITED)) return -1;
    *out = *sWildProfileContext; return 0;
}
static int WildProfileFailure(struct RhhWildProfileResult *out, u32 status, const char *reason)
{ out->status = status; if (reason) snprintf(out->reason, sizeof(out->reason), "%s", reason); return status; }
static void WildProfileDispatch(u32 kind, struct RhhWildProfileResult *result, struct RhhModDecision *decision)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = result->after.field, .subject = result->actor.species, .ordinal = result->individual.serial,
        .args = {result->before.present, result->after.present, result->is_preview, 0}};
    const struct RhhWildProfileResult *previous = sWildProfileContext;
    sWildProfileContext = result; RhhModsDispatch(&event, decision); sWildProfileContext = previous;
}
static int WildProfileEdit(const struct RhhWildProfileRequest *request, struct RhhWildProfileResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WILD_PROFILE_INVALID;
    *out = (struct RhhWildProfileResult){.struct_size = sizeof(*out), .is_preview = preview,
        .actor.struct_size = sizeof(out->actor), .individual.struct_size = sizeof(out->individual),
        .before.struct_size = sizeof(out->before), .after.struct_size = sizeof(out->after)};
    if (!request || request->struct_size < sizeof(*request) || !request->actor_handle || !request->owner
        || !request->field || !RhhModsRegisteredKey(request->owner) || request->present > 1
        || (!request->present && request->value))
        return WildProfileFailure(out, RHH_WILD_PROFILE_INVALID, "invalid profile edit");
    if (sWildProfileBusy || !WildActorCommandReady() || NativeSaveTransactionActive())
        return WildProfileFailure(out, RHH_WILD_PROFILE_UNAVAILABLE, "overworld commands unavailable");
    if (WildActorFind(request->actor_handle, &out->actor) || WildActorIndividual(request->actor_handle, &out->individual)
        || WildProfileAt(request->actor_handle, request->owner, request->field, &out->before))
        return WildProfileFailure(out, RHH_WILD_PROFILE_INVALID, "actor identity unavailable");
    if (out->actor.map != RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum))
        return WildProfileFailure(out, RHH_WILD_PROFILE_UNAVAILABLE, "actor is not on the current map");
    out->after = out->before;
    if (request->expected_revision != out->before.revision)
        return WildProfileFailure(out, RHH_WILD_PROFILE_STALE, "profile revision changed");
    if (out->before.present == request->present && (!request->present || out->before.value == request->value)) return RHH_WILD_PROFILE_OK;
    if (request->present)
    {
        int schemaStatus = WildProfileValueInSchema(request->owner, request->field, request->value);
        if (schemaStatus != RHH_WILD_PROFILE_OK)
            return WildProfileFailure(out, schemaStatus, schemaStatus == RHH_WILD_PROFILE_INVALID
                ? "value outside profile schema" : "profile schema unavailable");
    }
    if (out->before.revision == UINT64_MAX)
        return WildProfileFailure(out, RHH_WILD_PROFILE_UNAVAILABLE, "profile revision exhausted");
    u8 *state = WildProfileState();
    if (!state) return WildProfileFailure(out, RHH_WILD_PROFILE_UNAVAILABLE, "profile state unavailable");
    u8 *p = WildProfileRecord(state, out->actor.slot), staged[WILD_PROFILE_RECORD_SIZE];
    if (WildActorRead64(p) == out->individual.serial) memcpy(staged, p, sizeof(staged));
    else memset(staged, 0, sizeof(staged));
    u32 count = 0, index = RHH_WILD_PROFILE_CAPACITY;
    while (count < RHH_WILD_PROFILE_CAPACITY && WildActorRead64(staged + WILD_PROFILE_HEADER + count * WILD_PROFILE_ITEM_SIZE))
    {
        u8 *item = staged + WILD_PROFILE_HEADER + count * WILD_PROFILE_ITEM_SIZE;
        if (WildActorRead64(item) == request->owner && WildActorRead64(item + 8) == request->field) index = count;
        ++count;
    }
    if (index == RHH_WILD_PROFILE_CAPACITY && request->present && count == RHH_WILD_PROFILE_CAPACITY)
        return WildProfileFailure(out, RHH_WILD_PROFILE_FULL, "actor profile capacity reached");
    if (index < count)
    {
        u8 *item = staged + WILD_PROFILE_HEADER + index * WILD_PROFILE_ITEM_SIZE;
        memmove(item, item + WILD_PROFILE_ITEM_SIZE, (count - index - 1) * WILD_PROFILE_ITEM_SIZE);
        memset(staged + WILD_PROFILE_HEADER + (--count) * WILD_PROFILE_ITEM_SIZE, 0, WILD_PROFILE_ITEM_SIZE);
    }
    if (request->present)
    {
        index = 0;
        while (index < count)
        {
            const u8 *item = staged + WILD_PROFILE_HEADER + index * WILD_PROFILE_ITEM_SIZE;
            u64 owner = WildActorRead64(item), field = WildActorRead64(item + 8);
            if (owner > request->owner || (owner == request->owner && field > request->field)) break;
            ++index;
        }
        u8 *item = staged + WILD_PROFILE_HEADER + index * WILD_PROFILE_ITEM_SIZE;
        memmove(item + WILD_PROFILE_ITEM_SIZE, item, (count - index) * WILD_PROFILE_ITEM_SIZE);
        WildActorWrite64(item, request->owner); WildActorWrite64(item + 8, request->field);
        WildActorWrite64(item + 16, (u64)request->value);
    }
    out->after.present = request->present; out->after.value = request->value; ++out->after.revision; out->changed = TRUE;
    WildActorWrite64(staged, out->individual.serial); WildActorWrite64(staged + 8, out->after.revision);
    sWildProfileBusy = TRUE;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    WildProfileDispatch(RHH_CAN_EDIT_WILD_PROFILE, out, &decision);
    if (!decision.allowed)
    { sWildProfileBusy = FALSE; return WildProfileFailure(out, RHH_WILD_PROFILE_DENIED, decision.reason); }
    if (!preview)
    {
        u8 old[WILD_PROFILE_RECORD_SIZE]; memcpy(old, p, sizeof(old)); memcpy(p, staged, sizeof(staged));
        if (ValidateWildProfileState(state, WILD_PROFILE_STATE_SIZE))
        { memcpy(p, old, sizeof(old)); sWildProfileBusy = FALSE; return WildProfileFailure(out, RHH_WILD_PROFILE_UNAVAILABLE, "cannot store profile"); }
        decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
        WildProfileDispatch(RHH_WILD_PROFILE_EDITED, out, &decision);
    }
    sWildProfileBusy = FALSE; return RHH_WILD_PROFILE_OK;
}
static int WildProfilePreview(const struct RhhWildProfileRequest *r, struct RhhWildProfileResult *o)
{ return WildProfileEdit(r, o, TRUE); }
static int WildProfileApply(const struct RhhWildProfileRequest *r, struct RhhWildProfileResult *o)
{ return WildProfileEdit(r, o, FALSE); }
static int WildProfileBatchContext(struct RhhWildProfileBatchResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildProfileBatchContext
        || (event != RHH_CAN_EDIT_WILD_PROFILE && event != RHH_WILD_PROFILE_EDITED)) return -1;
    *out = *sWildProfileBatchContext; return 0;
}
static int WildProfileBatchFailure(struct RhhWildProfileBatchResult *out, u32 status, const char *reason)
{ out->status = status; if (reason) snprintf(out->reason, sizeof(out->reason), "%s", reason); return status; }
static int WildProfileBatch(const struct RhhWildProfileBatchRequest *request, struct RhhWildProfileBatchResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WILD_PROFILE_INVALID;
    *out = (struct RhhWildProfileBatchResult){.struct_size = sizeof(*out), .is_preview = preview,
        .actor.struct_size = sizeof(out->actor), .individual.struct_size = sizeof(out->individual)};
    if (!request || request->struct_size < sizeof(*request) || !request->actor_handle || !request->owner
        || !RhhModsRegisteredKey(request->owner) || !request->count || request->count > RHH_WILD_PROFILE_CAPACITY)
        return WildProfileBatchFailure(out, RHH_WILD_PROFILE_INVALID, "invalid profile batch");
    u32 order[RHH_WILD_PROFILE_CAPACITY];
    for (u32 i = 0; i < request->count; ++i)
    {
        const struct RhhWildProfileBatchItem *item = &request->items[i];
        if (!item->field || item->reserved || item->present > 1 || (!item->present && item->value))
            return WildProfileBatchFailure(out, RHH_WILD_PROFILE_INVALID, "invalid profile field");
        u32 j = i;
        while (j && request->items[order[j - 1]].field > item->field) { order[j] = order[j - 1]; --j; }
        if (j && request->items[order[j - 1]].field == item->field)
            return WildProfileBatchFailure(out, RHH_WILD_PROFILE_INVALID, "duplicate profile field");
        order[j] = i;
    }
    if (sWildProfileBusy || !WildActorCommandReady() || NativeSaveTransactionActive())
        return WildProfileBatchFailure(out, RHH_WILD_PROFILE_UNAVAILABLE, "overworld commands unavailable");
    if (WildActorFind(request->actor_handle, &out->actor) || WildActorIndividual(request->actor_handle, &out->individual))
        return WildProfileBatchFailure(out, RHH_WILD_PROFILE_INVALID, "actor identity unavailable");
    if (out->actor.map != RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum))
        return WildProfileBatchFailure(out, RHH_WILD_PROFILE_UNAVAILABLE, "actor is not on the current map");
    u8 *state = WildProfileState();
    if (!state) return WildProfileBatchFailure(out, RHH_WILD_PROFILE_UNAVAILABLE, "profile state unavailable");
    u8 *p = WildProfileRecord(state, out->actor.slot), staged[WILD_PROFILE_RECORD_SIZE];
    u8 original[WILD_PROFILE_RECORD_SIZE]; memcpy(original, p, sizeof(original));
    if (WildActorRead64(p) == out->individual.serial) memcpy(staged, p, sizeof(staged));
    else memset(staged, 0, sizeof(staged));
    u32 count = 0;
    while (count < RHH_WILD_PROFILE_CAPACITY && WildActorRead64(staged + WILD_PROFILE_HEADER + count * WILD_PROFILE_ITEM_SIZE)) ++count;
    s32 finalCount = count;
    bool8 changed[RHH_WILD_PROFILE_CAPACITY] = {0};
    out->count = request->count;
    for (u32 i = 0; i < request->count; ++i)
    {
        const struct RhhWildProfileBatchItem *item = &request->items[order[i]];
        struct RhhWildProfileBatchChange *change = &out->changes[i];
        change->before.struct_size = sizeof(change->before);
        if (WildProfileAt(request->actor_handle, request->owner, item->field, &change->before))
            return WildProfileBatchFailure(out, RHH_WILD_PROFILE_INVALID, "actor identity unavailable");
        if (change->before.revision != request->expected_revision)
            return WildProfileBatchFailure(out, RHH_WILD_PROFILE_STALE, "profile revision changed");
        change->after = change->before; change->after.present = item->present; change->after.value = item->value;
        changed[i] = change->before.present != item->present || (item->present && change->before.value != item->value);
        if (changed[i])
        {
            if (item->present)
            {
                int schemaStatus = WildProfileValueInSchema(request->owner, item->field, item->value);
                if (schemaStatus != RHH_WILD_PROFILE_OK)
                    return WildProfileBatchFailure(out, schemaStatus, schemaStatus == RHH_WILD_PROFILE_INVALID
                        ? "value outside profile schema" : "profile schema unavailable");
            }
            ++out->changed_count; finalCount += (s32)item->present - (s32)change->before.present;
        }
    }
    if (!out->changed_count) return RHH_WILD_PROFILE_OK;
    if (request->expected_revision == UINT64_MAX)
        return WildProfileBatchFailure(out, RHH_WILD_PROFILE_UNAVAILABLE, "profile revision exhausted");
    if (finalCount < 0 || finalCount > (s32)RHH_WILD_PROFILE_CAPACITY)
        return WildProfileBatchFailure(out, RHH_WILD_PROFILE_FULL, "actor profile capacity reached");
    for (u32 i = 0; i < request->count; ++i)
    {
        struct RhhWildProfileBatchChange *change = &out->changes[i];
        change->after.revision = request->expected_revision + 1;
        if (!changed[i] || !change->before.present) continue;
        for (u32 j = 0; j < count; ++j)
        {
            u8 *item = staged + WILD_PROFILE_HEADER + j * WILD_PROFILE_ITEM_SIZE;
            if (WildActorRead64(item) != request->owner || WildActorRead64(item + 8) != change->before.field) continue;
            memmove(item, item + WILD_PROFILE_ITEM_SIZE, (count - j - 1) * WILD_PROFILE_ITEM_SIZE);
            memset(staged + WILD_PROFILE_HEADER + (--count) * WILD_PROFILE_ITEM_SIZE, 0, WILD_PROFILE_ITEM_SIZE);
            break;
        }
    }
    for (u32 i = 0; i < request->count; ++i)
    {
        struct RhhWildProfileBatchChange *change = &out->changes[i];
        if (!changed[i] || !change->after.present) continue;
        u32 j = 0;
        while (j < count)
        {
            const u8 *item = staged + WILD_PROFILE_HEADER + j * WILD_PROFILE_ITEM_SIZE;
            u64 owner = WildActorRead64(item), field = WildActorRead64(item + 8);
            if (owner > request->owner || (owner == request->owner && field > change->after.field)) break;
            ++j;
        }
        u8 *item = staged + WILD_PROFILE_HEADER + j * WILD_PROFILE_ITEM_SIZE;
        memmove(item + WILD_PROFILE_ITEM_SIZE, item, (count - j) * WILD_PROFILE_ITEM_SIZE);
        WildActorWrite64(item, request->owner); WildActorWrite64(item + 8, change->after.field);
        WildActorWrite64(item + 16, (u64)change->after.value); ++count;
    }
    WildActorWrite64(staged, out->individual.serial); WildActorWrite64(staged + 8, request->expected_revision + 1);
    sWildProfileBusy = TRUE; sWildProfileBatchContext = out;
    for (u32 i = 0; i < request->count; ++i)
    {
        if (!changed[i]) continue;
        struct RhhWildProfileResult item = {.struct_size = sizeof(item), .is_preview = preview, .changed = TRUE,
            .actor = out->actor, .individual = out->individual, .before = out->changes[i].before, .after = out->changes[i].after};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        WildProfileDispatch(RHH_CAN_EDIT_WILD_PROFILE, &item, &decision);
        if (!decision.allowed)
        {
            sWildProfileBatchContext = NULL; sWildProfileBusy = FALSE;
            return WildProfileBatchFailure(out, RHH_WILD_PROFILE_DENIED, decision.reason);
        }
    }
    struct RhhWildActorIndividual current = {.struct_size = sizeof(current)};
    if (!WildActorCommandReady() || NativeSaveTransactionActive() || memcmp(p, original, sizeof(original))
        || WildActorIndividual(request->actor_handle, &current) || current.run_id != out->individual.run_id
        || current.serial != out->individual.serial)
    {
        sWildProfileBatchContext = NULL; sWildProfileBusy = FALSE;
        return WildProfileBatchFailure(out, RHH_WILD_PROFILE_STALE, "actor or profile changed during validation");
    }
    if (!preview)
    {
        memcpy(p, staged, sizeof(staged));
        if (ValidateWildProfileState(state, WILD_PROFILE_STATE_SIZE))
        {
            memcpy(p, original, sizeof(original)); sWildProfileBatchContext = NULL; sWildProfileBusy = FALSE;
            return WildProfileBatchFailure(out, RHH_WILD_PROFILE_UNAVAILABLE, "cannot store profile");
        }
        for (u32 i = 0; i < request->count; ++i)
        {
            if (!changed[i]) continue;
            struct RhhWildProfileResult item = {.struct_size = sizeof(item), .changed = TRUE, .actor = out->actor,
                .individual = out->individual, .before = out->changes[i].before, .after = out->changes[i].after};
            struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
            WildProfileDispatch(RHH_WILD_PROFILE_EDITED, &item, &decision);
        }
    }
    sWildProfileBatchContext = NULL; sWildProfileBusy = FALSE; return RHH_WILD_PROFILE_OK;
}
static int WildProfileBatchPreview(const struct RhhWildProfileBatchRequest *r, struct RhhWildProfileBatchResult *o)
{ return WildProfileBatch(r, o, TRUE); }
static int WildProfileBatchApply(const struct RhhWildProfileBatchRequest *r, struct RhhWildProfileBatchResult *o)
{ return WildProfileBatch(r, o, FALSE); }
static int WildProfileBattle(RhhContentKey owner, RhhContentKey field, struct RhhWildProfileValue *out);
static const struct RhhWildProfileService sWildProfileService = {
    {sizeof(sWildProfileService), RHH_SERVICE_VERSION(1, 4)}, WildProfileAt, WildProfilePreview, WildProfileApply, WildProfileContext,
    WildProfileBatchPreview, WildProfileBatchApply, WildProfileBatchContext,
    WildProfileSchema, WildProfileSchemaContext, WildProfileOfferSchema, WildProfileSchemaList, WildProfileBattle
};
static const struct RhhWildTraitResult *sWildTraitContext;
static int WildTraitContext(struct RhhWildTraitResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildTraitContext
        || RhhModsCurrentEvent() != RHH_WILD_TRAIT_QUERY) return -1;
    *out = *sWildTraitContext; return 0;
}
static int WildTraitQuery(u64 handle, RhhContentKey tag, struct RhhWildTraitResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || sWildTraitContext) return -1;
    const struct RhhEcologyTagsService *tags = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/ecology_tags", RHH_SERVICE_VERSION(1, 0));
    struct RhhWildTraitResult result = {.struct_size = sizeof(result), .tag = tag, .permitted = 1,
        .actor.struct_size = sizeof(result.actor)};
    if (!tags || WildActorFind(handle, &result.actor) || tags->species_has(result.actor.species, tag, &result.base)) return -1;
    struct RhhWildActorIndividual individual = {.struct_size = sizeof(individual)};
    if (!WildActorIndividual(handle, &individual))
    { result.individual = individual; result.individual_available = 1; }
    result.value = result.base;
    struct RhhWildTraitOverride override = {.struct_size = sizeof(override)};
    if (result.individual_available && !WildTraitOverrideAt(handle, tag, &override) && override.value)
        result.value = override.value == RHH_WILD_TRAIT_ON;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WILD_TRAIT_QUERY,
        .run_seed = RhhModsRunSeed(), .event_key = tag, .subject = result.actor.species,
        .ordinal = result.individual_available ? result.individual.serial : handle,
        .args = {result.base, result.individual_available, override.value, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = result.value};
    u32 fallback = result.value;
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    sWildTraitContext = &result; RhhModsDispatch(&event, &decision); sWildTraitContext = NULL;
    RhhModsWorldTimeScopeEnd(&timeScope);
    bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
    struct RhhWildActorSnapshot current = {.struct_size = sizeof(current)};
    struct RhhWildActorIndividual currentIndividual = {.struct_size = sizeof(currentIndividual)};
    if (WildActorFind(handle, &current) || memcmp(&current, &result.actor, sizeof(current))
        || (!!WildActorIndividual(handle, &currentIndividual) == !!result.individual_available)
        || (result.individual_available && memcmp(&currentIndividual, &result.individual, sizeof(currentIndividual)))) return -1;
    if (result.individual_available) {
        struct RhhWildTraitOverride currentOverride = {.struct_size = sizeof(currentOverride)};
        if (WildTraitOverrideAt(handle, tag, &currentOverride) || memcmp(&currentOverride, &override, sizeof(override))) return -1;
    }
    result.permitted = !!decision.allowed;
    result.value = !decision.allowed ? 0 : !impure && decision.value <= 1 ? decision.value : fallback;
    *out = result; return 0;
}
static int WildTraitRequestBattle(RhhContentKey tag)
{
    if (RhhModsCurrentEvent() != RHH_WILD_ACTOR_ENGAGED || RhhModsCurrentStage() != RHH_OBSERVE
        || !RhhModsCurrentOwner() || !sWildActorContext || !sWildActorBattle.armed || sWildActorBattle.transitioned
        || sWildActorBattle.runId != RhhModsRunId() || sWildActorContext->actor.handle != sWildActorBattle.actor.handle) return -1;
    const struct RhhEcologyTagsService *tags = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/ecology_tags", RHH_SERVICE_VERSION(1, 0));
    struct RhhEcologyTag found = {.struct_size = sizeof(found)};
    if (!tags || tags->find(tag, &found)) return -1;
    struct WildBattleTraits *traits = &sWildActorBattle.traits;
    u32 index = 0;
    while (index < traits->count && traits->entries[index].tag < tag) ++index;
    if (index < traits->count && traits->entries[index].tag == tag) return 0;
    if (traits->count >= RHH_WILD_BATTLE_TRAIT_LIMIT) return -1;
    memmove(&traits->entries[index + 1], &traits->entries[index],
            (traits->count - index) * sizeof(traits->entries[0]));
    traits->entries[index] = (struct WildBattleTraitEntry){.tag = tag}; ++traits->count; return 0;
}
static int WildTraitBattle(RhhContentKey tag, struct RhhWildTraitResult *out);
static const struct RhhWildTraitsService sWildTraitsService = {
    {sizeof(sWildTraitsService), RHH_SERVICE_VERSION(1, 1)}, WildTraitQuery, WildTraitContext,
    WildTraitRequestBattle, WildTraitBattle
};
static int WildActorFindIndividual(const struct RhhWildActorIndividual *id, struct RhhWildActorSnapshot *out)
{
    if (!id || id->struct_size < sizeof(*id) || id->reserved || !id->run_id || !id->serial
        || !out || out->struct_size < sizeof(*out) || !RhhModsRunCommitted() || id->run_id != RhhModsRunId()) return -1;
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
        if (sWildActors[i].individual == id->serial && sWildActors[i].identified && WildActorMatches(i))
            return WildActorAt(i, out);
    return -1;
}
static int WildActorEventIndividual(struct RhhWildActorIndividual *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildActorIndividualContext
        || (event != RHH_WILD_ACTOR_SPAWNED && event != RHH_WILD_ACTOR_REMOVED
            && event != RHH_WILD_ACTOR_ENGAGED && event != RHH_WILD_ACTOR_IDENTIFIED
            && event != RHH_WILD_ACTOR_BATTLE_ENDED)) return -1;
    *out = *sWildActorIndividualContext; return 0;
}
static int WildActorContext(struct RhhWildActorEvent *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildActorContext
        || (event != RHH_WILD_ACTOR_SPAWNED && event != RHH_WILD_ACTOR_REMOVED
            && event != RHH_WILD_ACTOR_ENGAGED && event != RHH_WILD_ACTOR_IDENTIFIED)) return -1;
    *out = *sWildActorContext; return 0;
}
static int WildActorBattleResult(struct RhhWildActorBattleResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildActorBattleContext
        || RhhModsCurrentEvent() != RHH_WILD_ACTOR_BATTLE_ENDED) return -1;
    *out = *sWildActorBattleContext; return 0;
}
static int WildActorCommandReady(void)
{
    return RhhModsHasActiveRun() && RhhModsCommandsAllowed() && Ready()
        && gMain.callback2 == CB2_Overworld && !gPaletteFade.active && !gPaletteFade.y
        && !ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked() && !IsOverworldLinkActive();
}
static int WildActorCapacity(struct RhhWildActorCapacity *out)
{
    if (!out || out->struct_size < sizeof(*out) || !Ready()) return -1;
    struct RhhWildActorCapacity result = {.struct_size = sizeof(result),
        .automatic_enabled = !!WE_OW_ENCOUNTERS, .commands_allowed = !!WildActorCommandReady(),
        .next_generated_slot = ModsWildGeneratedFreeSlot(), .generated_count = GetNumberOfActiveOWEs(OWE_GENERATED),
        .generated_limit = OWE_SPAWNS_MAX, .manual_count = GetNumberOfActiveOWEs(OWE_MANUAL),
        .object_limit = OBJECT_EVENTS_COUNT, .sprite_limit = MAX_SPRITES,
        .map = RhhContentKeyForId(RHH_MAP, (gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum)};
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i) result.object_count += !!gObjectEvents[i].active;
    for (u32 i = 0; i < MAX_SPRITES; ++i) result.sprite_count += !!gSprites[i].inUse;
    *out = result; return 0;
}
static int WildActorSpawnPreflight(const struct RhhWildActorSpawnRequest *request,
                                   struct RhhWildActorSpawnPreflight *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved
        || !out || out->struct_size < sizeof(*out) || !Ready()) return -1;
    struct RhhWildActorSpawnPreflight result = {.struct_size = sizeof(result),
        .status = RHH_WILD_ACTOR_UNAVAILABLE, .generated_slot = UINT32_MAX};
    RhhContentKey map = RhhContentKeyForId(RHH_MAP, (gSaveBlock1Ptr->location.mapGroup << 8)
        | (u8)gSaveBlock1Ptr->location.mapNum);
    if (!WildActorCommandReady() || sWildActorSerial == UINT64_MAX) { *out = result; return 0; }
    if (!request->map || request->map != map || request->x < 0 || request->y < 0
        || request->x >= gMapHeader.mapLayout->width || request->y >= gMapHeader.mapLayout->height)
    { result.status = RHH_WILD_ACTOR_INVALID; *out = result; return 0; }
    u32 slot = UINT32_MAX; bool32 water = FALSE;
    result.status = PreflightModOverworldWildEncounter(request->x, request->y, &slot, &water);
    if (result.status == RHH_WILD_ACTOR_OK)
    { result.area = water ? RHH_WILD_WATER : RHH_WILD_LAND; result.generated_slot = slot; }
    *out = result; return 0;
}
static int WildActorSpeciesSpawnPreflight(const struct RhhWildActorSpeciesSpawnRequest *request,
                                          struct RhhWildActorSpeciesPreflight *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved[0] || request->reserved[1]
        || !out || out->struct_size < sizeof(*out) || !Ready()) return -1;
    struct RhhWildActorSpeciesPreflight result = {.struct_size = sizeof(result),
        .status = RHH_WILD_ACTOR_UNAVAILABLE, .generated_slot = UINT32_MAX};
    RhhContentKey map = RhhContentKeyForId(RHH_MAP, (gSaveBlock1Ptr->location.mapGroup << 8)
        | (u8)gSaveBlock1Ptr->location.mapNum);
    if (!WildActorCommandReady() || sWildActorSerial == UINT64_MAX) { *out = result; return 0; }
    u32 species;
    struct RhhContentInfo info = {.struct_size = sizeof(info)};
    if (!request->map || request->map != map || request->x < 0 || request->y < 0
        || request->x >= gMapHeader.mapLayout->width || request->y >= gMapHeader.mapLayout->height
        || !request->species || !request->level || request->level > MAX_LEVEL
        || RhhContentResolve(RHH_SPECIES, request->species, &species)
        || RhhContentInfoFor(RHH_SPECIES, request->species, &info) || !(info.flags & RHH_CONTENT_CREATABLE))
    { result.status = RHH_WILD_ACTOR_INVALID; *out = result; return 0; }
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    enum Ability lastAbility = gLastUsedAbility;
    u32 slot = UINT32_MAX; bool32 water = FALSE;
    result.status = PreflightModOverworldWildSpecies(request->x, request->y, species, request->level, &slot, &water);
    if (result.status == RHH_WILD_ACTOR_OK)
    {
        result.area = water ? RHH_WILD_WATER : RHH_WILD_LAND; result.generated_slot = slot;
        if (!IsWildLevelAllowedByRepel(request->level)) result.flags |= RHH_WILD_SPECIES_REPEL_BLOCKS;
        if (IsEncounterAbilityFilterPossible(request->level)) result.flags |= RHH_WILD_SPECIES_ABILITY_ROLL;
    }
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = lastAbility;
    if (!pure) return -1;
    *out = result; return 0;
}
static const struct RhhWildConcealResult *sWildConcealContext;
static bool8 sWildConcealBusy;
static u32 WildConcealCover(const struct ObjectEvent *object)
{
    if (!Ready() || (!object->isPlayer && (object->mapGroup != gSaveBlock1Ptr->location.mapGroup
        || object->mapNum != gSaveBlock1Ptr->location.mapNum))
        || object->currentCoords.x < MAP_OFFSET || object->currentCoords.y < MAP_OFFSET
        || object->currentCoords.x >= gMapHeader.mapLayout->width + MAP_OFFSET
        || object->currentCoords.y >= gMapHeader.mapLayout->height + MAP_OFFSET) return 0;
    u8 behavior = MapGridGetMetatileBehaviorAt(object->currentCoords.x, object->currentCoords.y);
    return (MetatileBehavior_IsTallGrass(behavior) ? RHH_WILD_COVER_TALL_GRASS : 0)
        | (MetatileBehavior_IsLongGrass(behavior) ? RHH_WILD_COVER_LONG_GRASS : 0)
        | (MetatileBehavior_IsShortGrass(behavior) ? RHH_WILD_COVER_SHORT_GRASS : 0)
        | (MetatileBehavior_IsSandOrDeepSand(behavior) ? RHH_WILD_COVER_SAND : 0)
        | (MetatileBehavior_IsShallowFlowingWater(behavior) ? RHH_WILD_COVER_SHALLOW_WATER : 0);
}
static int WildConcealAt(u64 handle, struct RhhWildConcealment *out)
{
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    if (!out || out->struct_size < sizeof(*out) || WildActorFind(handle, &actor)) return -1;
    *out = sWildActors[actor.slot].conceal;
    out->struct_size = sizeof(*out); out->cover = WildConcealCover(&gObjectEvents[actor.slot]);
    return 0;
}
static int WildConcealContext(struct RhhWildConcealResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildConcealContext
        || (event != RHH_CAN_WILD_CONCEALMENT && event != RHH_WILD_CONCEALMENT_CHANGED)) return -1;
    *out = *sWildConcealContext; return 0;
}
static int WildConcealFailure(struct RhhWildConcealResult *out, u32 status, const char *reason)
{
    out->status = status;
    if (reason) snprintf(out->reason, sizeof(out->reason), "%s", reason);
    return status;
}
static void WildConcealDispatch(u32 kind, struct RhhWildConcealResult *result, struct RhhModDecision *decision)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind,
        .run_seed = RhhModsRunSeed(), .event_key = result->actor.map, .subject = result->actor.species,
        .ordinal = result->actor.handle, .args = {result->cause, result->after.mode,
            result->after.reveal_flags, result->after.reveal_radius}};
    const struct RhhWildConcealResult *previous = sWildConcealContext;
    sWildConcealContext = result; RhhModsDispatch(&event, decision); sWildConcealContext = previous;
}
static int WildConcealChange(const struct RhhWildConcealRequest *request, struct RhhWildConcealResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WILD_CONCEAL_INVALID;
    *out = (struct RhhWildConcealResult){.struct_size = sizeof(*out), .is_preview = preview,
        .actor.struct_size = sizeof(out->actor), .individual.struct_size = sizeof(out->individual),
        .before.struct_size = sizeof(out->before), .after.struct_size = sizeof(out->after)};
    if (!request || request->struct_size < sizeof(*request) || !request->handle
        || !request->owner || !RhhModsRegisteredKey(request->owner)
        || request->mode > RHH_WILD_CONCEAL_GROUND
        || (request->reveal_flags & ~(RHH_WILD_REVEAL_ON_MOVE | RHH_WILD_REVEAL_ON_APPROACH))
        || (!request->mode && (request->reveal_flags || request->reveal_radius))
        || ((request->reveal_flags & RHH_WILD_REVEAL_ON_APPROACH)
            ? (!request->reveal_radius || request->reveal_radius > 16) : request->reveal_radius != 0))
        return WildConcealFailure(out, RHH_WILD_CONCEAL_INVALID, "invalid concealment request");
    if (sWildConcealBusy || !WildActorCommandReady())
        return WildConcealFailure(out, RHH_WILD_CONCEAL_UNAVAILABLE, "overworld commands unavailable");
    if (WildActorFind(request->handle, &out->actor))
        return WildConcealFailure(out, RHH_WILD_CONCEAL_INVALID, "actor no longer exists");
    u32 slot = out->actor.slot; struct WildActorEntry *entry = &sWildActors[slot];
    struct ObjectEvent *object = &gObjectEvents[slot];
    if (object->mapGroup != gSaveBlock1Ptr->location.mapGroup || object->mapNum != gSaveBlock1Ptr->location.mapNum
        || object->spriteId >= MAX_SPRITES || !gSprites[object->spriteId].inUse)
        return WildConcealFailure(out, RHH_WILD_CONCEAL_UNAVAILABLE, "actor is not on the current map");
    WildActorIndividualAt(slot, &out->individual);
    WildConcealAt(request->handle, &out->before); out->after = out->before;
    if (request->expected_revision != entry->conceal.revision)
        return WildConcealFailure(out, RHH_WILD_CONCEAL_STALE, "concealment revision changed");
    if (entry->conceal.mode && entry->conceal.owner != request->owner)
        return WildConcealFailure(out, RHH_WILD_CONCEAL_CONFLICT, "concealment belongs to another module");
    if (entry->conceal.mode == request->mode && entry->conceal.reveal_flags == request->reveal_flags
        && entry->conceal.reveal_radius == request->reveal_radius) return RHH_WILD_CONCEAL_OK;
    if (entry->conceal.revision == UINT64_MAX)
        return WildConcealFailure(out, RHH_WILD_CONCEAL_UNAVAILABLE, "concealment revision exhausted");
    out->cause = !entry->conceal.mode ? RHH_WILD_CONCEAL_SET
        : !request->mode ? RHH_WILD_CONCEAL_CLEARED : RHH_WILD_CONCEAL_CHANGED;
    out->after.mode = request->mode; out->after.reveal_flags = request->reveal_flags;
    out->after.reveal_radius = request->reveal_radius; out->after.owner = request->mode ? request->owner : 0;
    out->after.revision++;
    sWildConcealBusy = TRUE;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    WildConcealDispatch(RHH_CAN_WILD_CONCEALMENT, out, &decision);
    if (!decision.allowed)
    { sWildConcealBusy = FALSE; return WildConcealFailure(out, RHH_WILD_CONCEAL_DENIED, decision.reason); }
    if (!preview)
    {
        if (!WildConcealStore(slot, object, &out->after))
        { sWildConcealBusy = FALSE; return WildConcealFailure(out, RHH_WILD_CONCEAL_UNAVAILABLE, "cannot store concealment"); }
        entry->conceal = out->after; entry->conceal.cover = 0; entry->concealMoved = FALSE;
        if (out->before.mode == RHH_WILD_CONCEAL_GROUND || out->after.mode == RHH_WILD_CONCEAL_GROUND)
            object->triggerGroundEffectsOnMove = TRUE;
        WildActorAt(slot, &out->actor);
        decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
        WildConcealDispatch(RHH_WILD_CONCEALMENT_CHANGED, out, &decision);
    }
    sWildConcealBusy = FALSE; return RHH_WILD_CONCEAL_OK;
}
static int WildConcealPreview(const struct RhhWildConcealRequest *r, struct RhhWildConcealResult *o)
{ return WildConcealChange(r, o, TRUE); }
static int WildConcealApply(const struct RhhWildConcealRequest *r, struct RhhWildConcealResult *o)
{ return WildConcealChange(r, o, FALSE); }
static void WildConcealAutoReveal(u32 slot, u32 cause)
{
    struct WildActorEntry *entry = &sWildActors[slot]; struct ObjectEvent *object = &gObjectEvents[slot];
    if (!entry->conceal.mode || entry->conceal.revision == UINT64_MAX) return;
    struct RhhWildConcealResult result = {.struct_size = sizeof(result), .cause = cause,
        .actor.struct_size = sizeof(result.actor), .individual.struct_size = sizeof(result.individual),
        .before.struct_size = sizeof(result.before)};
    if (WildActorAt(slot, &result.actor) || WildConcealAt(entry->handle, &result.before)) return;
    WildActorIndividualAt(slot, &result.individual); result.after = result.before;
    result.after.mode = result.after.reveal_flags = result.after.reveal_radius = 0;
    result.after.owner = 0; result.after.revision++;
    if (!WildConcealStore(slot, object, &result.after)) return;
    entry->conceal = result.after; entry->conceal.cover = 0; entry->concealMoved = FALSE;
    if (result.before.mode == RHH_WILD_CONCEAL_GROUND) object->triggerGroundEffectsOnMove = TRUE;
    WildActorAt(slot, &result.actor);
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sWildConcealBusy = TRUE; WildConcealDispatch(RHH_WILD_CONCEALMENT_CHANGED, &result, &decision); sWildConcealBusy = FALSE;
}
static const struct RhhWildConcealService sWildConcealService = {
    {sizeof(sWildConcealService), RHH_SERVICE_VERSION(1, 0)}, WildConcealAt, WildConcealPreview, WildConcealApply, WildConcealContext
};
static int WildActorSpawn(const struct RhhWildActorSpawnRequest *request, struct RhhWildActorSnapshot *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved
        || !out || out->struct_size < sizeof(*out)) return RHH_WILD_ACTOR_INVALID;
    if (!WildActorCommandReady() || sWildActorSerial == UINT64_MAX) return RHH_WILD_ACTOR_UNAVAILABLE;
    RhhContentKey map = RhhContentKeyForId(RHH_MAP, (gSaveBlock1Ptr->location.mapGroup << 8)
        | (u8)gSaveBlock1Ptr->location.mapNum);
    if (!request->map || request->map != map || request->x < 0 || request->y < 0
        || request->x >= gMapHeader.mapLayout->width || request->y >= gMapHeader.mapLayout->height)
        return RHH_WILD_ACTOR_INVALID;
    u32 slot;
    int status = TrySpawnModOverworldWildEncounter(request->x, request->y, &slot);
    if (status != RHH_WILD_ACTOR_OK) return status;
    if (WildActorAt(slot, out))
    {
        RemoveObjectEvent(&gObjectEvents[slot]);
        return RHH_WILD_ACTOR_FAILED;
    }
    return RHH_WILD_ACTOR_OK;
}
static int WildActorSpawnSpecies(const struct RhhWildActorSpeciesSpawnRequest *request, struct RhhWildActorSnapshot *out)
{
    u32 species;
    struct RhhContentInfo info = {.struct_size = sizeof(info)};
    if (!request || request->struct_size < sizeof(*request) || request->reserved[0] || request->reserved[1]
        || !request->species || !request->level || request->level > MAX_LEVEL
        || RhhContentResolve(RHH_SPECIES, request->species, &species)
        || RhhContentInfoFor(RHH_SPECIES, request->species, &info)
        || !(info.flags & RHH_CONTENT_CREATABLE)
        || !out || out->struct_size < sizeof(*out)) return RHH_WILD_ACTOR_INVALID;
    if (!WildActorCommandReady() || sWildActorSerial == UINT64_MAX) return RHH_WILD_ACTOR_UNAVAILABLE;
    RhhContentKey map = RhhContentKeyForId(RHH_MAP, (gSaveBlock1Ptr->location.mapGroup << 8)
        | (u8)gSaveBlock1Ptr->location.mapNum);
    if (!request->map || request->map != map || request->x < 0 || request->y < 0
        || request->x >= gMapHeader.mapLayout->width || request->y >= gMapHeader.mapLayout->height)
        return RHH_WILD_ACTOR_INVALID;
    u32 slot;
    int status = TrySpawnModOverworldWildSpecies(request->x, request->y, species, request->level, &slot);
    if (status != RHH_WILD_ACTOR_OK) return status;
    if (WildActorAt(slot, out))
    {
        RemoveObjectEvent(&gObjectEvents[slot]);
        return RHH_WILD_ACTOR_FAILED;
    }
    return RHH_WILD_ACTOR_OK;
}
static int WildActorRemove(u64 handle)
{
    if (!handle) return RHH_WILD_ACTOR_INVALID;
    if (!WildActorCommandReady()) return RHH_WILD_ACTOR_UNAVAILABLE;
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    if (WildActorFind(handle, &actor)) return RHH_WILD_ACTOR_INVALID;
    struct ObjectEvent *object = &gObjectEvents[actor.slot];
    if (object->mapGroup != gSaveBlock1Ptr->location.mapGroup || object->mapNum != gSaveBlock1Ptr->location.mapNum
        || object->spriteId >= MAX_SPRITES || !gSprites[object->spriteId].inUse) return RHH_WILD_ACTOR_UNAVAILABLE;
    RemoveObjectEvent(object);
    return RHH_WILD_ACTOR_OK;
}
static int WildActorStep(u64 handle, u32 requestedDirection)
{
    enum Direction direction;
    switch (requestedDirection)
    {
    case RHH_WILD_ACTOR_SOUTH: direction = DIR_SOUTH; break;
    case RHH_WILD_ACTOR_NORTH: direction = DIR_NORTH; break;
    case RHH_WILD_ACTOR_WEST: direction = DIR_WEST; break;
    case RHH_WILD_ACTOR_EAST: direction = DIR_EAST; break;
    default: return RHH_WILD_ACTOR_INVALID;
    }
    if (!handle) return RHH_WILD_ACTOR_INVALID;
    if (!WildActorCommandReady() || sWildStepSerial == UINT64_MAX) return RHH_WILD_ACTOR_UNAVAILABLE;
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    if (WildActorFind(handle, &actor)) return RHH_WILD_ACTOR_INVALID;
    struct ObjectEvent *object = &gObjectEvents[actor.slot];
    if (object->mapGroup != gSaveBlock1Ptr->location.mapGroup || object->mapNum != gSaveBlock1Ptr->location.mapNum
        || object->spriteId >= MAX_SPRITES || !gSprites[object->spriteId].inUse
        || gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT || !gObjectEvents[gPlayerAvatar.objectEventId].active)
        return RHH_WILD_ACTOR_UNAVAILABLE;
    if (sWildActors[actor.slot].moving || ObjectEventIsMovementOverridden(object)
        || object->currentCoords.x != object->previousCoords.x || object->currentCoords.y != object->previousCoords.y)
        return RHH_WILD_ACTOR_BLOCKED;
    s32 x = actor.x + gDirectionToVectors[direction].x, y = actor.y + gDirectionToVectors[direction].y;
    if (x < 0 || y < 0 || x >= gMapHeader.mapLayout->width || y >= gMapHeader.mapLayout->height)
        return RHH_WILD_ACTOR_BLOCKED;
    u8 overwrite = DIR_NONE;
    if (CheckRestrictedOWEMovementNoEffects(object, direction, &overwrite)) return RHH_WILD_ACTOR_BLOCKED;
    if (overwrite != DIR_NONE)
    {
        x = actor.x + gDirectionToVectors[overwrite].x; y = actor.y + gDirectionToVectors[overwrite].y;
        if (x < 0 || y < 0 || x >= gMapHeader.mapLayout->width || y >= gMapHeader.mapLayout->height)
            return RHH_WILD_ACTOR_BLOCKED;
    }
    u32 elevation = MapGridGetElevationAt(x + MAP_OFFSET, y + MAP_OFFSET);
    if (elevation == ELEVATION_MULTI_LEVEL || MapGridGetElevationAt(actor.x + MAP_OFFSET, actor.y + MAP_OFFSET) == ELEVATION_MULTI_LEVEL)
        elevation = actor.elevation;
    u32 movement = overwrite != DIR_NONE ? overwrite : direction;
    struct RhhWildStepResult step = {.struct_size = sizeof(step), .status = RHH_WILD_STEP_PENDING,
        .source = RHH_WILD_STEP_DIRECT, .direction = requestedDirection, .run_id = RhhModsRunId(), .before = actor,
        .individual.struct_size = sizeof(step.individual),
        .expected = {x, y, elevation, object->facingDirectionLocked ? actor.facing : movement}};
    (void)WildActorIndividualAt(actor.slot, &step.individual);
    u8 oldOverwrite = object->directionOverwrite;
    object->directionOverwrite = overwrite;
    SetObjectEventDirection(object, direction);
    if (ObjectEventSetHeldMovement(object, GetWalkNormalMovementAction(direction)))
    { object->directionOverwrite = oldOverwrite; return RHH_WILD_ACTOR_BLOCKED; }
    sWildActors[actor.slot].moving = TRUE;
    sWildActors[actor.slot].moveAction = object->movementActionId;
    step.ticket = ++sWildStepSerial; sWildActors[actor.slot].step = step;
    return RHH_WILD_ACTOR_OK;
}
static int WildActorEngage(u64 handle)
{
    if (!handle) return RHH_WILD_ACTOR_INVALID;
    if (!WildActorCommandReady()) return RHH_WILD_ACTOR_UNAVAILABLE;
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    if (WildActorFind(handle, &actor)) return RHH_WILD_ACTOR_INVALID;
    struct ObjectEvent *object = &gObjectEvents[actor.slot];
    if (object->mapGroup != gSaveBlock1Ptr->location.mapGroup || object->mapNum != gSaveBlock1Ptr->location.mapNum
        || object->spriteId >= MAX_SPRITES || !gSprites[object->spriteId].inUse
        || gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT || !gObjectEvents[gPlayerAvatar.objectEventId].active)
        return RHH_WILD_ACTOR_UNAVAILABLE;
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];
    if (sWildActors[actor.slot].moving || object->currentCoords.x != object->previousCoords.x
        || object->currentCoords.y != object->previousCoords.y || player->currentCoords.x != player->previousCoords.x
        || player->currentCoords.y != player->previousCoords.y || !AreElevationsCompatible(object->currentElevation, player->currentElevation)
        || abs(object->currentCoords.x - player->currentCoords.x) + abs(object->currentCoords.y - player->currentCoords.y) != 1)
        return RHH_WILD_ACTOR_BLOCKED;
    TryTriggerOverworldWildEncounter(object, player);
    return ScriptContext_IsEnabled() ? RHH_WILD_ACTOR_OK : RHH_WILD_ACTOR_BLOCKED;
}
static const struct RhhWildBehaviorContext *sWildBehaviorContext;
static bool8 sWildBehaviorBusy;
static struct RhhWildBehaviorContext sWildBehaviorBatch[OBJECT_EVENTS_COUNT];
static struct RhhWildActivitySelection sWildSelectionBatch[OBJECT_EVENTS_COUNT];
static struct RhhWildQueue sWildQueueBatch[OBJECT_EVENTS_COUNT];
static bool8 sWildQueueChosen[OBJECT_EVENTS_COUNT];
static u32 sWildBehaviorCount;
static struct RhhWildActivity sWildActivities[RHH_WILD_ACTIVITY_CAPACITY];
static u32 sWildActivityCount;
static bool8 sWildActivityCollecting, sWildActivityInvalid;
static const struct RhhWildActivitySelection *sWildSelectionContext;
static const struct RhhWildActivityChange *sWildActivityChangeContext;
static const struct RhhWildQueueChange *sWildQueueChangeContext;
static const struct RhhWildQueueGoalChange *sWildQueueGoalContext;
static const struct RhhWildBehaviorPolicyChange *sWildPolicyContext;
enum { WILD_BEHAVIOR_RADIUS = 12, WILD_ACTIVITY_MAX_WEIGHT = 1000, WILD_ACTIVITY_MAX_INTERVAL = 65535,
       WILD_ACTIVITY_MIN_PRIORITY = -1000, WILD_ACTIVITY_MAX_PRIORITY = 1000 };
static int WildQueueAt(u64 handle, struct RhhWildQueue *out);
static bool32 WildBehaviorWithinRadius(s64 dx, s64 dy, u32 radiusX, u32 radiusY)
{ return dx >= -(s64)radiusX && dx <= radiusX && dy >= -(s64)radiusY && dy <= radiusY; }
static u64 WildPolicyRevision(void)
{
    const u8 *state = RhhModsState(sWildPolicyStateId);
    return state && RhhModsHasActiveRun() ? WildActorRead64(state) : 0;
}
static int WildBehaviorPolicy(struct RhhWildBehaviorPolicy *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    *out = (struct RhhWildBehaviorPolicy){.struct_size = sizeof(*out),
        .flags = RHH_WILD_POLICY_PLAYER_STEP | RHH_WILD_POLICY_CURRENT_MAP | RHH_WILD_POLICY_INCLUDES_OFFSCREEN
               | RHH_WILD_POLICY_REQUIRES_ID | RHH_WILD_POLICY_ONE_PENDING_BATCH,
        .radius_x = WILD_BEHAVIOR_RADIUS, .radius_y = WILD_BEHAVIOR_RADIUS,
        .actor_capacity = OBJECT_EVENTS_COUNT, .activity_capacity = RHH_WILD_ACTIVITY_CAPACITY,
        .max_weight = WILD_ACTIVITY_MAX_WEIGHT, .max_interval = WILD_ACTIVITY_MAX_INTERVAL,
        .min_priority = WILD_ACTIVITY_MIN_PRIORITY, .max_priority = WILD_ACTIVITY_MAX_PRIORITY};
    const u8 *state = RhhModsState(sWildPolicyStateId);
    if (state && RhhModsHasActiveRun() && WildActorRead64(state))
    { out->radius_x = WildActorRead32(state + 16); out->radius_y = WildActorRead32(state + 20); }
    return 0;
}
static int WildPolicyContext(struct RhhWildBehaviorPolicyChange *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildPolicyContext
        || (event != RHH_CAN_SET_WILD_BEHAVIOR_POLICY && event != RHH_WILD_BEHAVIOR_POLICY_CHANGED)) return -1;
    *out = *sWildPolicyContext; return 0;
}
static int WildPolicyFail(struct RhhWildBehaviorPolicyChange *result, u32 status, const char *reason)
{
    result->status = status; result->changed = FALSE; result->after = result->before;
    result->after_revision = result->before_revision; snprintf(result->reason, sizeof(result->reason), "%s", reason); return status;
}
static int WildPolicyChange(const struct RhhWildBehaviorPolicyRequest *request, struct RhhWildBehaviorPolicyChange *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WILD_POLICY_INVALID;
    struct RhhWildBehaviorPolicyChange result = {.struct_size = sizeof(result), .is_preview = preview,
        .before.struct_size = sizeof(result.before), .after.struct_size = sizeof(result.after)};
    u32 status = RHH_WILD_POLICY_INVALID;
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->requester
        || !RhhModsRegisteredKey(request->requester) || request->radius_x > RHH_WILD_BEHAVIOR_RADIUS_MAX
        || request->radius_y > RHH_WILD_BEHAVIOR_RADIUS_MAX)
    { WildPolicyFail(&result, status, "invalid behavior policy"); goto done; }
    result.requester = request->requester;
    if (sWildBehaviorBusy || sWildBehaviorCount || !WildActorCommandReady() || NativeSaveTransactionActive())
    { status = WildPolicyFail(&result, RHH_WILD_POLICY_UNAVAILABLE, "behavior policy unavailable"); goto done; }
    u8 *state = RhhModsState(sWildPolicyStateId);
    if (!state) { status = WildPolicyFail(&result, RHH_WILD_POLICY_UNAVAILABLE, "behavior policy state unavailable"); goto done; }
    result.before_revision = result.after_revision = WildActorRead64(state);
    WildBehaviorPolicy(&result.before); result.after = result.before;
    if (request->expected_revision != result.before_revision)
    { status = WildPolicyFail(&result, RHH_WILD_POLICY_STALE, "behavior policy revision changed"); goto done; }
    if (result.before.radius_x == request->radius_x && result.before.radius_y == request->radius_y)
    { status = RHH_WILD_POLICY_OK; goto done; }
    if (result.before_revision == UINT64_MAX)
    { status = WildPolicyFail(&result, RHH_WILD_POLICY_UNAVAILABLE, "behavior policy revision exhausted"); goto done; }
    u8 before[WILD_POLICY_STATE_SIZE], staged[WILD_POLICY_STATE_SIZE]; memcpy(before, state, sizeof(before));
    memset(staged, 0, sizeof(staged)); result.changed = TRUE; ++result.after_revision;
    result.after.radius_x = request->radius_x; result.after.radius_y = request->radius_y;
    WildActorWrite64(staged, result.after_revision); WildActorWrite64(staged + 8, request->requester);
    WildActorWrite32(staged + 16, request->radius_x); WildActorWrite32(staged + 20, request->radius_y);
    if (ValidateWildPolicyState(staged, sizeof(staged)))
    { status = WildPolicyFail(&result, RHH_WILD_POLICY_INVALID, "invalid behavior policy state"); goto done; }
    u64 run = RhhModsRunId(); rng_value_t rng = gRngValue, rng2 = gRng2Value; sWildBehaviorBusy = TRUE;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_SET_WILD_BEHAVIOR_POLICY,
        .run_seed = RhhModsRunSeed(), .subject = request->requester, .ordinal = result.before_revision,
        .args = {request->radius_x, request->radius_y, preview}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sWildPolicyContext = &result; RhhModsDispatch(&event, &decision); sWildPolicyContext = NULL;
    bool32 rngChanged = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed) status = WildPolicyFail(&result, RHH_WILD_POLICY_DENIED, decision.reason[0] ? decision.reason : "behavior policy denied");
    else if (rngChanged || !WildActorCommandReady() || NativeSaveTransactionActive() || sWildBehaviorCount
        || run != RhhModsRunId() || memcmp(before, state, sizeof(before)))
        status = WildPolicyFail(&result, RHH_WILD_POLICY_STALE, "behavior policy changed during validation");
    else
    {
        status = result.status = RHH_WILD_POLICY_OK;
        if (!preview)
        {
            memcpy(state, staged, sizeof(staged)); event.kind = RHH_WILD_BEHAVIOR_POLICY_CHANGED;
            event.ordinal = result.after_revision; decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
            sWildPolicyContext = &result; RhhModsDispatch(&event, &decision); sWildPolicyContext = NULL;
        }
    }
    sWildBehaviorBusy = FALSE;
done:
    result.status = status; *out = result; return status;
}
static int WildPolicyPreview(const struct RhhWildBehaviorPolicyRequest *request, struct RhhWildBehaviorPolicyChange *out)
{ return WildPolicyChange(request, out, TRUE); }
static int WildPolicyApply(const struct RhhWildBehaviorPolicyRequest *request, struct RhhWildBehaviorPolicyChange *out)
{ return WildPolicyChange(request, out, FALSE); }
static int WildBehaviorInspect(const struct RhhWildBehaviorInspectionRequest *request,
                               struct RhhWildBehaviorInspection *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->map
        || !out || out->struct_size < sizeof(*out)) return -1;
    u32 mapId;
    if (RhhContentResolve(RHH_MAP, request->map, &mapId) || RhhContentKeyForId(RHH_MAP, mapId) != request->map) return -1;
    struct RhhWildBehaviorInspection result = {.struct_size = sizeof(result),
        .actor.struct_size = sizeof(result.actor), .individual.struct_size = sizeof(result.individual)};
    if (WildActorFind(request->actor_handle, &result.actor)) return -1;
    result.individual_available = !WildActorIndividual(request->actor_handle, &result.individual);
    if (!result.individual_available) { result.gates |= RHH_WILD_GATE_NO_INDIVIDUAL; memset(&result.individual, 0, sizeof(result.individual)); }
    RhhContentKey currentMap = gSaveBlock1Ptr ? RhhContentKeyForId(RHH_MAP,
        ((u8)gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum) : 0;
    if (result.actor.map != request->map || request->map != currentMap) result.gates |= RHH_WILD_GATE_OTHER_MAP;
    result.dx = (s64)result.actor.x - request->x; result.dy = (s64)result.actor.y - request->y;
    struct RhhWildBehaviorPolicy policy = {.struct_size = sizeof(policy)};
    WildBehaviorPolicy(&policy);
    if (!WildBehaviorWithinRadius(result.dx, result.dy, policy.radius_x, policy.radius_y)) result.gates |= RHH_WILD_GATE_OUTSIDE_RADIUS;
    if (!WildActorCommandReady()) result.gates |= RHH_WILD_GATE_COMMANDS_UNAVAILABLE;
    if (NativeSaveTransactionActive()) result.gates |= RHH_WILD_GATE_SAVE_ACTIVE;
    if (sWildBehaviorCount) result.gates |= RHH_WILD_GATE_BATCH_PENDING;
    if (sWildBehaviorBusy) result.gates |= RHH_WILD_GATE_DISPATCHING;
    if (!RhhModsHasHook(RHH_WILD_BEHAVIOR_PROPOSE) && !RhhModsHasHook(RHH_WILD_ACTIVITY_COLLECT))
    {
        struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
        if (WildQueueAt(request->actor_handle, &queue) || !queue.count || !queue.owner_available || queue.revision == UINT64_MAX)
            result.gates |= RHH_WILD_GATE_NO_HOOKS;
    }
    *out = result; return 0;
}
static int WildBehaviorContext(struct RhhWildBehaviorContext *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildBehaviorContext
        || (event != RHH_WILD_BEHAVIOR_PROPOSE && event != RHH_WILD_BEHAVIOR_RESULT
            && event != RHH_WILD_ACTIVITY_COLLECT)) return -1;
    *out = *sWildBehaviorContext; return 0;
}
static int WildActivitySelected(struct RhhWildActivitySelection *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildSelectionContext
        || (event != RHH_WILD_BEHAVIOR_PROPOSE && event != RHH_WILD_BEHAVIOR_RESULT)) return -1;
    *out = *sWildSelectionContext; return 0;
}
static int WildActivityAt(u64 handle, struct RhhWildActivityState *out)
{
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildActorIndividual individual = {.struct_size = sizeof(individual)};
    u8 *state = WildActivityState();
    if (!out || out->struct_size < sizeof(*out) || !state || WildActorFind(handle, &actor)
        || WildActorIndividual(handle, &individual)) return -1;
    struct RhhWildActivityState result = {.struct_size = sizeof(result), .individual = individual};
    const u8 *p = WildActivityRecord(state, actor.slot);
    if (WildActorRead64(p) == individual.serial)
    {
        result.valid = 1; result.id = WildActorRead64(p + 8); result.owner = WildActorRead64(p + 16);
        result.since_step = WildActorRead64(p + 24); result.valid_until_step = WildActorRead64(p + 32);
        result.action = p[40];
    }
    *out = result; return 0;
}
static int WildActivityChangeContext(struct RhhWildActivityChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildActivityChangeContext
        || RhhModsCurrentEvent() != RHH_WILD_ACTIVITY_CHANGED) return -1;
    *out = *sWildActivityChangeContext; return 0;
}
static int WildQueueAt(u64 handle, struct RhhWildQueue *out)
{
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildActorIndividual individual = {.struct_size = sizeof(individual)};
    u8 *state = WildQueueState();
    if (!out || out->struct_size < sizeof(*out) || !state || WildActorFind(handle, &actor)
        || WildActorIndividual(handle, &individual)) return -1;
    struct RhhWildQueue result = {.struct_size = sizeof(result), .individual = individual};
    const u8 *p = WildQueueRecord(state, actor.slot);
    if (WildActorRead64(p) == individual.serial)
    {
        result.revision = WildActorRead64(p + 8); result.owner = WildActorRead64(p + 16);
        result.count = p[24]; memcpy(result.actions, p + 25, sizeof(result.actions));
        result.owner_available = result.owner && RhhModsRegisteredKey(result.owner);
    }
    *out = result; return 0;
}
static void WildQueueReadGoal(const u8 *p, const struct RhhWildQueue *queue, struct RhhWildQueueGoal *out)
{
    *out = (struct RhhWildQueueGoal){.struct_size = sizeof(*out), .revision = queue->revision, .individual = queue->individual};
    if (WildActorRead64(p) != queue->individual.serial) return;
    const u8 *goal = p + WILD_QUEUE_V3_RECORD_SIZE;
    if (!goal[0]) return;
    out->kind = goal[0]; out->flags = goal[1]; out->stop_distance = goal[4];
    if (goal[1] & RHH_WILD_QUEUE_ROUTE_INTERRUPTIBLE) out->interrupt_priority = (s32)WildActorRead16(goal + 2) - 1000;
    out->owner = WildActorRead64(goal + 8); out->map = WildActorRead64(goal + 16);
    out->owner_available = RhhModsRegisteredKey(out->owner);
    out->target_x = WildActorRead32(goal + 24); out->target_y = WildActorRead32(goal + 28);
    out->radius = WildActorRead32(goal + 32); out->all_flags = WildActorRead32(goal + 36);
    out->any_flags = WildActorRead32(goal + 40); out->exclude_flags = WildActorRead32(goal + 44);
}
static int WildQueueGoal(u64 handle, struct RhhWildQueueGoal *out)
{
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildQueue queue = {.struct_size = sizeof(queue)}; u8 *state = WildQueueState();
    if (!out || out->struct_size < sizeof(*out) || !state || WildActorFind(handle, &actor) || WildQueueAt(handle, &queue)) return -1;
    WildQueueReadGoal(WildQueueRecord(state, actor.slot), &queue, out); return 0;
}
static int WildQueueCompletion(u64 handle, struct RhhWildQueueCompletion *out)
{
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildQueue queue = {.struct_size = sizeof(queue)}; u8 *state = WildQueueState();
    if (!out || out->struct_size < sizeof(*out) || !state || WildActorFind(handle, &actor) || WildQueueAt(handle, &queue)) return -1;
    const u8 *p = WildQueueRecord(state, actor.slot);
    *out = (struct RhhWildQueueCompletion){.struct_size = sizeof(*out), .revision = queue.revision,
        .on_arrival = queue.count && WildActorRead64(p) == queue.individual.serial && p[37]};
    return 0;
}
static int WildQueueGoalContext(struct RhhWildQueueGoalChange *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildQueueGoalContext
        || (event != RHH_CAN_EDIT_WILD_QUEUE && event != RHH_WILD_QUEUE_CHANGED)) return -1;
    *out = *sWildQueueGoalContext; return 0;
}
static int WildQueueRouteState(u64 handle, struct RhhWildQueueRouteState *out)
{
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
    u8 *state = WildQueueState();
    if (!out || out->struct_size < sizeof(*out) || !state || WildActorFind(handle, &actor) || WildQueueAt(handle, &queue)) return -1;
    struct RhhWildQueueRouteState result = {.struct_size = sizeof(result), .revision = queue.revision};
    const u8 *p = WildQueueRecord(state, actor.slot);
    if (!queue.count || WildActorRead64(p) != queue.individual.serial || !p[33]) { *out = result; return 0; }
    const u8 *guard = p + WILD_QUEUE_V1_RECORD_SIZE;
    result.guarded = TRUE;
    result.origin = (struct RhhWildRouteWaypoint){WildActorRead16(guard), WildActorRead16(guard + 2), guard[8], guard[9]};
    result.landing = (struct RhhWildRouteWaypoint){WildActorRead16(guard + 4), WildActorRead16(guard + 6), guard[10], guard[11]};
    result.actual_origin = (struct RhhWildRouteWaypoint){actor.x, actor.y, actor.elevation, actor.facing};
    struct ObjectEvent *object = &gObjectEvents[actor.slot];
    if (!Ready() || actor.map != RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum)
        || actor.x < 0 || actor.y < 0 || actor.x >= gMapHeader.mapLayout->width || actor.y >= gMapHeader.mapLayout->height
        || object->spriteId >= MAX_SPRITES || !gSprites[object->spriteId].inUse
        || gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT || !gObjectEvents[gPlayerAvatar.objectEventId].active)
    { result.status = RHH_WILD_ACTOR_UNAVAILABLE; result.reason = RHH_WILD_ROUTE_GUARD_UNAVAILABLE; }
    else if (sWildActors[actor.slot].moving || ObjectEventIsMovementOverridden(object)
        || object->currentCoords.x != object->previousCoords.x || object->currentCoords.y != object->previousCoords.y)
    { result.status = RHH_WILD_ACTOR_BLOCKED; result.reason = RHH_WILD_ROUTE_GUARD_BUSY; }
    else if (result.actual_origin.x != result.origin.x || result.actual_origin.y != result.origin.y
        || result.actual_origin.elevation != result.origin.elevation || result.actual_origin.facing != result.origin.facing)
    { result.status = RHH_WILD_ACTOR_ROUTE_CHANGED; result.reason = RHH_WILD_ROUTE_GUARD_ORIGIN_CHANGED; }
    else
    {
        u8 direction = queue.actions[0], overwrite = DIR_NONE;
        if (direction < DIR_SOUTH || direction > DIR_EAST || CheckRestrictedOWEMovementNoEffects(object, direction, &overwrite))
        { result.status = RHH_WILD_ACTOR_BLOCKED; result.reason = RHH_WILD_ROUTE_GUARD_BLOCKED; }
        else
        {
            u8 movement = overwrite != DIR_NONE ? overwrite : direction;
            s32 x = actor.x + gDirectionToVectors[movement].x, y = actor.y + gDirectionToVectors[movement].y;
            if (x < 0 || y < 0 || x >= gMapHeader.mapLayout->width || y >= gMapHeader.mapLayout->height)
            { result.status = RHH_WILD_ACTOR_BLOCKED; result.reason = RHH_WILD_ROUTE_GUARD_BLOCKED; }
            else
            {
                u32 elevation = MapGridGetElevationAt(x + MAP_OFFSET, y + MAP_OFFSET);
                if (elevation == ELEVATION_MULTI_LEVEL || MapGridGetElevationAt(actor.x + MAP_OFFSET, actor.y + MAP_OFFSET) == ELEVATION_MULTI_LEVEL)
                    elevation = actor.elevation;
                result.actual_landing = (struct RhhWildRouteWaypoint){x, y, elevation,
                    object->facingDirectionLocked ? actor.facing : movement};
                if (result.actual_landing.x != result.landing.x || result.actual_landing.y != result.landing.y
                    || result.actual_landing.elevation != result.landing.elevation || result.actual_landing.facing != result.landing.facing)
                { result.status = RHH_WILD_ACTOR_ROUTE_CHANGED; result.reason = RHH_WILD_ROUTE_GUARD_LANDING_CHANGED; }
            }
        }
    }
    *out = result; return 0;
}
static int WildQueueInterruption(u64 handle, struct RhhWildQueueInterruption *out)
{
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildQueue queue = {.struct_size = sizeof(queue)}; u8 *state = WildQueueState();
    if (!out || out->struct_size < sizeof(*out) || !state || WildActorFind(handle, &actor) || WildQueueAt(handle, &queue)) return -1;
    struct RhhWildQueueInterruption result = {.struct_size = sizeof(result), .revision = queue.revision};
    const u8 *p = WildQueueRecord(state, actor.slot);
    if (queue.count && WildActorRead64(p) == queue.individual.serial && p[34])
    { result.enabled = TRUE; result.priority = (s32)WildActorRead16(p + 35) - 1000; }
    *out = result; return 0;
}
static int WildQueueContext(struct RhhWildQueueChange *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildQueueChangeContext
        || (event != RHH_CAN_EDIT_WILD_QUEUE && event != RHH_WILD_QUEUE_CHANGED)) return -1;
    *out = *sWildQueueChangeContext; return 0;
}
static void WildQueueDispatch(u32 kind, struct RhhWildQueueChange *change, struct RhhModDecision *decision)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = change->after.owner ? change->after.owner : change->before.owner,
        .subject = change->actor.species, .ordinal = change->before.individual.serial,
        .args = {change->cause, change->before.count, change->after.count, 0}};
    const struct RhhWildQueueChange *previous = sWildQueueChangeContext;
    sWildQueueChangeContext = change; RhhModsDispatch(&event, decision); sWildQueueChangeContext = previous;
}
static int WildQueueReplaceInternal(const struct RhhWildQueueRequest *request, struct RhhWildQueueChange *out,
                                    const struct RhhWildRouteResult *route, const struct RhhWildRouteTrace *trace,
                                    bool32 interruptible, s32 interruptPriority, bool32 onArrival, const u8 *goal)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WILD_QUEUE_INVALID;
    *out = (struct RhhWildQueueChange){.struct_size = sizeof(*out), .cause = RHH_WILD_QUEUE_REPLACED,
        .actor.struct_size = sizeof(out->actor), .before.struct_size = sizeof(out->before), .after.struct_size = sizeof(out->after)};
    if (!request || request->struct_size < sizeof(*request) || !request->actor_handle || !request->owner
        || !RhhModsRegisteredKey(request->owner) || request->count > RHH_WILD_QUEUE_CAPACITY)
        return out->status = RHH_WILD_QUEUE_INVALID;
    for (u32 i = 0; i < RHH_WILD_QUEUE_CAPACITY; ++i)
        if (i < request->count ? (request->actions[i] < RHH_WILD_BEHAVIOR_SOUTH || request->actions[i] > RHH_WILD_BEHAVIOR_ENGAGE)
                               : request->actions[i] != 0) return out->status = RHH_WILD_QUEUE_INVALID;
    if (sWildBehaviorBusy || sWildBehaviorCount || !WildActorCommandReady() || NativeSaveTransactionActive())
        return out->status = RHH_WILD_QUEUE_UNAVAILABLE;
    if (WildActorFind(request->actor_handle, &out->actor) || WildQueueAt(request->actor_handle, &out->before))
        return out->status = RHH_WILD_QUEUE_INVALID;
    out->after = out->before;
    if (out->actor.map != RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum))
        return out->status = RHH_WILD_QUEUE_UNAVAILABLE;
    if (request->expected_revision != out->before.revision) return out->status = RHH_WILD_QUEUE_STALE;
    u8 *state = WildQueueState(); if (!state) return out->status = RHH_WILD_QUEUE_UNAVAILABLE;
    u8 *p = WildQueueRecord(state, out->actor.slot), old[WILD_QUEUE_RECORD_SIZE]; memcpy(old, p, sizeof(old));
    struct RhhWildQueueGoalChange goals = {.struct_size = sizeof(goals)};
    WildQueueReadGoal(p, &out->before, &goals.before);
    if ((out->before.count && out->before.owner != request->owner)
        || (goals.before.kind && goals.before.owner != request->owner)) return out->status = RHH_WILD_QUEUE_CONFLICT;
    u8 staged[WILD_QUEUE_RECORD_SIZE] = {0}; WildActorWrite64(staged, out->before.individual.serial);
    WildActorWrite64(staged + 16, request->count ? request->owner : 0); staged[24] = request->count;
    memcpy(staged + 25, request->actions, sizeof(request->actions));
    if (goal) memcpy(staged + WILD_QUEUE_V3_RECORD_SIZE, goal, WILD_QUEUE_GOAL_SIZE);
    if (route || trace)
    {
        if (!route || !trace || !request->count || trace->count != request->count
            || route->actor.handle != out->actor.handle || route->actor.map != out->actor.map
            || route->actor.x != out->actor.x || route->actor.y != out->actor.y
            || route->actor.elevation != out->actor.elevation || route->actor.facing != out->actor.facing)
            return out->status = RHH_WILD_QUEUE_STALE;
        staged[33] = 1; staged[37] = !!onArrival;
        if (interruptible) { staged[34] = 1; WildActorWrite16(staged + 35, interruptPriority + 1000); }
        for (u32 i = 0; i < request->count; ++i)
        {
            s32 x = i ? trace->steps[i - 1].x : route->actor.x, y = i ? trace->steps[i - 1].y : route->actor.y;
            u32 elevation = i ? trace->steps[i - 1].elevation : route->actor.elevation;
            u32 facing = i ? trace->steps[i - 1].facing : route->actor.facing;
            const struct RhhWildRouteStep *step = &trace->steps[i]; u8 *guard = staged + WILD_QUEUE_V1_RECORD_SIZE + i * WILD_QUEUE_GUARD_SIZE;
            if (step->direction != request->actions[i] || x < 0 || y < 0 || step->x < 0 || step->y < 0
                || x > INT16_MAX - MAP_OFFSET || y > INT16_MAX - MAP_OFFSET || step->x > INT16_MAX - MAP_OFFSET
                || step->y > INT16_MAX - MAP_OFFSET || elevation >= 16 || step->elevation >= 16
                || facing > DIR_NORTHEAST || step->facing > DIR_NORTHEAST) return out->status = RHH_WILD_QUEUE_INVALID;
            WildActorWrite16(guard, x); WildActorWrite16(guard + 2, y);
            WildActorWrite16(guard + 4, step->x); WildActorWrite16(guard + 6, step->y);
            guard[8] = elevation; guard[9] = facing; guard[10] = step->elevation; guard[11] = step->facing;
        }
    }
    if ((!request->count && !out->before.count && !goals.before.kind && !goal) || (WildActorRead64(p) == out->before.individual.serial
        && !memcmp(p + 16, staged + 16, WILD_QUEUE_RECORD_SIZE - 16))) return RHH_WILD_QUEUE_OK;
    if (out->before.revision == UINT64_MAX) return out->status = RHH_WILD_QUEUE_UNAVAILABLE;
    out->changed = TRUE; out->after.count = request->count; out->after.owner = request->count ? request->owner : 0;
    out->after.owner_available = !!request->count; ++out->after.revision;
    memcpy(out->after.actions, request->actions, sizeof(out->after.actions));
    WildActorWrite64(staged + 8, out->after.revision);
    WildQueueReadGoal(staged, &out->after, &goals.after);
    sWildBehaviorBusy = TRUE;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    const struct RhhWildQueueGoalChange *previousGoal = sWildQueueGoalContext; sWildQueueGoalContext = &goals;
    WildQueueDispatch(RHH_CAN_EDIT_WILD_QUEUE, out, &decision);
    if (!decision.allowed) { sWildQueueGoalContext = previousGoal; sWildBehaviorBusy = FALSE; snprintf(out->reason, sizeof(out->reason), "%s", decision.reason);
        return out->status = RHH_WILD_QUEUE_DENIED; }
    memcpy(p, staged, sizeof(staged));
    if (ValidateWildQueueState(state, WILD_QUEUE_STATE_SIZE)) { memcpy(p, old, sizeof(old)); sWildQueueGoalContext = previousGoal; sWildBehaviorBusy = FALSE;
        return out->status = RHH_WILD_QUEUE_UNAVAILABLE; }
    decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
    WildQueueDispatch(RHH_WILD_QUEUE_CHANGED, out, &decision); sWildQueueGoalContext = previousGoal; sWildBehaviorBusy = FALSE; return RHH_WILD_QUEUE_OK;
}
static int WildQueueReplace(const struct RhhWildQueueRequest *request, struct RhhWildQueueChange *out)
{ return WildQueueReplaceInternal(request, out, NULL, NULL, FALSE, 0, FALSE, NULL); }
static int WildQueueRouteInternal(const struct RhhWildQueueRouteOptions *options, const void *request,
                                  struct RhhWildQueueRouteResult *out, bool32 habitat)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WILD_QUEUE_INVALID;
    *out = (struct RhhWildQueueRouteResult){.struct_size = sizeof(*out), .route.struct_size = sizeof(out->route),
        .habitat.struct_size = sizeof(out->habitat), .change.struct_size = sizeof(out->change)};
    if (!options || options->struct_size < RHH_WILD_QUEUE_ROUTE_OPTIONS_V1_0_SIZE
        || (options->struct_size > RHH_WILD_QUEUE_ROUTE_OPTIONS_V1_0_SIZE && options->struct_size < RHH_WILD_QUEUE_ROUTE_OPTIONS_V1_1_SIZE)
        || options->reserved || !options->owner
        || !RhhModsRegisteredKey(options->owner) || !request) return out->status = RHH_WILD_QUEUE_INVALID;
    bool32 interruptible = FALSE, prefix = FALSE, retain = FALSE, onArrival = FALSE; s32 priority = 0;
    if (options->struct_size >= RHH_WILD_QUEUE_ROUTE_OPTIONS_V1_1_SIZE)
    {
        if (options->flags & ~(RHH_WILD_QUEUE_ROUTE_INTERRUPTIBLE | RHH_WILD_QUEUE_ROUTE_PREFIX | RHH_WILD_QUEUE_ROUTE_RETAIN_GOAL | RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL)) return out->status = RHH_WILD_QUEUE_INVALID;
        interruptible = !!(options->flags & RHH_WILD_QUEUE_ROUTE_INTERRUPTIBLE); priority = options->interrupt_priority;
        prefix = !!(options->flags & RHH_WILD_QUEUE_ROUTE_PREFIX);
        retain = !!(options->flags & RHH_WILD_QUEUE_ROUTE_RETAIN_GOAL);
        onArrival = !!(options->flags & RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL);
        if (interruptible ? priority < WILD_ACTIVITY_MIN_PRIORITY || priority > WILD_ACTIVITY_MAX_PRIORITY : priority != 0)
            return out->status = RHH_WILD_QUEUE_INVALID;
    }
    if (habitat ? ((const struct RhhWildHabitatRouteRequest *)request)->struct_size < sizeof(struct RhhWildHabitatRouteRequest)
                : ((const struct RhhWildRouteRequest *)request)->struct_size < sizeof(struct RhhWildRouteRequest))
        return out->status = RHH_WILD_QUEUE_INVALID;
    if (sWildBehaviorBusy || sWildBehaviorCount || !WildActorCommandReady() || NativeSaveTransactionActive())
        return out->status = RHH_WILD_QUEUE_UNAVAILABLE;
    u64 handle = habitat ? ((const struct RhhWildHabitatRouteRequest *)request)->actor_handle
                         : ((const struct RhhWildRouteRequest *)request)->actor_handle;
    struct RhhWildQueue current = {.struct_size = sizeof(current)};
    if (WildQueueAt(handle, &current)) return out->status = RHH_WILD_QUEUE_INVALID;
    if (current.revision != options->expected_revision) return out->status = RHH_WILD_QUEUE_STALE;
    if (current.count && current.owner != options->owner) return out->status = RHH_WILD_QUEUE_CONFLICT;
    struct RhhWildQueueGoal currentGoal = {.struct_size = sizeof(currentGoal)};
    if (WildQueueGoal(handle, &currentGoal)) return out->status = RHH_WILD_QUEUE_INVALID;
    if (currentGoal.kind && currentGoal.owner != options->owner) return out->status = RHH_WILD_QUEUE_CONFLICT;
    struct RhhWildRouteTrace trace = {.struct_size = sizeof(trace)};
    if (habitat)
    {
        if (WildActorRouteHabitatTrace(request, &out->habitat, &trace)) return out->status = RHH_WILD_QUEUE_INVALID;
        out->habitat_available = TRUE; out->route = out->habitat.route;
    }
    else if (WildActorRouteTrace(request, &out->route, &trace)) return out->status = RHH_WILD_QUEUE_INVALID;
    if (out->route.status == RHH_WILD_ROUTE_ALREADY_THERE) return out->status = RHH_WILD_QUEUE_ALREADY_THERE;
    if (out->route.status == RHH_WILD_ROUTE_NO_PATH) return out->status = RHH_WILD_QUEUE_NO_PATH;
    if (out->route.status == RHH_WILD_ROUTE_LIMIT) return out->status = RHH_WILD_QUEUE_ROUTE_LIMIT;
    if (out->route.status != RHH_WILD_ROUTE_FOUND || !trace.count || trace.count != out->route.length)
        return out->status = RHH_WILD_QUEUE_UNAVAILABLE;
    if (trace.count > RHH_WILD_QUEUE_CAPACITY)
    {
        if (!prefix) return out->status = RHH_WILD_QUEUE_ROUTE_TOO_LONG;
        trace.count = RHH_WILD_QUEUE_CAPACITY;
    }
    struct RhhWildQueueRequest edit = {.struct_size = sizeof(edit), .count = trace.count, .actor_handle = handle,
        .expected_revision = options->expected_revision, .owner = options->owner};
    for (u32 i = 0; i < trace.count; ++i) edit.actions[i] = trace.steps[i].direction;
    u8 goal[WILD_QUEUE_GOAL_SIZE] = {0};
    if (retain)
    {
        goal[0] = habitat ? RHH_WILD_QUEUE_GOAL_HABITAT : RHH_WILD_QUEUE_GOAL_POINT; goal[1] = options->flags;
        if (interruptible) WildActorWrite16(goal + 2, priority + 1000);
        WildActorWrite64(goal + 8, options->owner); WildActorWrite64(goal + 16, out->route.actor.map);
        if (habitat)
        {
            const struct RhhWildHabitatRouteRequest *r = request;
            goal[4] = r->stop_distance; WildActorWrite32(goal + 32, r->radius);
            WildActorWrite32(goal + 36, r->all_flags); WildActorWrite32(goal + 40, r->any_flags); WildActorWrite32(goal + 44, r->exclude_flags);
        }
        else
        {
            const struct RhhWildRouteRequest *r = request;
            goal[4] = r->stop_distance; WildActorWrite32(goal + 24, r->target_x); WildActorWrite32(goal + 28, r->target_y);
        }
    }
    return out->status = WildQueueReplaceInternal(&edit, &out->change, &out->route, &trace, interruptible, priority, onArrival, retain ? goal : NULL);
}
static int WildQueueRoute(const struct RhhWildQueueRouteOptions *options, const struct RhhWildRouteRequest *request,
                          struct RhhWildQueueRouteResult *out)
{ return WildQueueRouteInternal(options, request, out, FALSE); }
static int WildQueueHabitatRoute(const struct RhhWildQueueRouteOptions *options, const struct RhhWildHabitatRouteRequest *request,
                                 struct RhhWildQueueRouteResult *out)
{ return WildQueueRouteInternal(options, request, out, TRUE); }
static int WildQueueReplan(u64 handle, u64 revision, RhhContentKey owner, struct RhhWildQueueRouteResult *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WILD_QUEUE_INVALID;
    *out = (struct RhhWildQueueRouteResult){.struct_size = sizeof(*out)};
    struct RhhWildQueueGoal goal = {.struct_size = sizeof(goal)};
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    if (!owner || !RhhModsRegisteredKey(owner) || WildQueueGoal(handle, &goal) || WildActorFind(handle, &actor) || !goal.kind)
        return out->status = RHH_WILD_QUEUE_INVALID;
    if (goal.revision != revision) return out->status = RHH_WILD_QUEUE_STALE;
    if (goal.owner != owner) return out->status = RHH_WILD_QUEUE_CONFLICT;
    if (goal.map != actor.map) return out->status = RHH_WILD_QUEUE_UNAVAILABLE;
    struct RhhWildQueueRouteOptions options = {.struct_size = sizeof(options), .expected_revision = revision,
        .owner = owner, .flags = goal.flags, .interrupt_priority = goal.interrupt_priority};
    if (goal.kind == RHH_WILD_QUEUE_GOAL_POINT)
    {
        struct RhhWildRouteRequest r = {.struct_size = sizeof(r), .actor_handle = handle, .stop_distance = goal.stop_distance,
            .target_x = goal.target_x, .target_y = goal.target_y};
        return WildQueueRoute(&options, &r, out);
    }
    struct RhhWildHabitatRouteRequest r = {.struct_size = sizeof(r), .actor_handle = handle, .stop_distance = goal.stop_distance,
        .radius = goal.radius, .all_flags = goal.all_flags, .any_flags = goal.any_flags, .exclude_flags = goal.exclude_flags};
    return WildQueueHabitatRoute(&options, &r, out);
}
static int CompareWildBehavior(const void *left, const void *right)
{
    const struct RhhWildBehaviorContext *a = left, *b = right;
    return (a->individual.serial > b->individual.serial) - (a->individual.serial < b->individual.serial);
}
static void WildBehaviorDispatch(u32 kind, struct RhhWildBehaviorContext *context, struct RhhModDecision *decision)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = context->step.map, .subject = context->actor.species, .ordinal = context->step.step,
        .args = {context->action, context->status, 0, 0}};
    const struct RhhWildBehaviorContext *previous = sWildBehaviorContext;
    sWildBehaviorContext = context; RhhModsDispatch(&event, decision); sWildBehaviorContext = previous;
}
static int WildActivityOffer(const struct RhhWildActivity *activity)
{
    if (!sWildActivityCollecting || RhhModsCurrentEvent() != RHH_WILD_ACTIVITY_COLLECT) return -1;
    if (!activity || activity->struct_size < sizeof(*activity) || activity->reserved || !activity->id
        || !RhhModsRegisteredKey(activity->owner) || activity->action > RHH_WILD_BEHAVIOR_ENGAGE
        || !activity->weight || activity->weight > WILD_ACTIVITY_MAX_WEIGHT || !activity->interval || activity->interval > WILD_ACTIVITY_MAX_INTERVAL
        || activity->priority < WILD_ACTIVITY_MIN_PRIORITY || activity->priority > WILD_ACTIVITY_MAX_PRIORITY || sWildActivityCount == RHH_WILD_ACTIVITY_CAPACITY)
    { sWildActivityInvalid = TRUE; return -1; }
    for (u32 i = 0; i < sWildActivityCount; ++i)
        if (sWildActivities[i].id == activity->id) { sWildActivityInvalid = TRUE; return -1; }
    sWildActivities[sWildActivityCount++] = *activity;
    return 0;
}
static int CompareWildActivities(const void *left, const void *right)
{
    const struct RhhWildActivity *a = left, *b = right;
    return (a->id > b->id) - (a->id < b->id);
}
static struct RhhWildActivitySelection WildActivitySelect(struct RhhWildBehaviorContext *context)
{
    struct RhhWildActivitySelection selected = {.struct_size = sizeof(selected)};
    if (!RhhModsHasHook(RHH_WILD_ACTIVITY_COLLECT)) return selected;
    sWildActivityCount = 0; sWildActivityInvalid = FALSE; sWildActivityCollecting = TRUE;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    WildBehaviorDispatch(RHH_WILD_ACTIVITY_COLLECT, context, &decision);
    sWildActivityCollecting = FALSE;
    if (!decision.allowed || sWildActivityInvalid || !sWildActivityCount) return selected;
    qsort(sWildActivities, sWildActivityCount, sizeof(sWildActivities[0]), CompareWildActivities);
    int32_t priority = sWildActivities[0].priority;
    for (u32 i = 1; i < sWildActivityCount; ++i)
        if (sWildActivities[i].priority > priority) priority = sWildActivities[i].priority;
    u32 total = 0, interval = UINT32_MAX;
    for (u32 i = 0; i < sWildActivityCount; ++i)
        if (sWildActivities[i].priority == priority)
        {
            total += sWildActivities[i].weight;
            if (sWildActivities[i].interval < interval) interval = sWildActivities[i].interval;
        }
    u64 identity = context->individual.serial ^ (context->individual.run_id * UINT64_C(0x9e3779b97f4a7c15));
    u64 choice = RhhModRandomAt(RhhModsRunSeed(), "rhh:wild_activity", identity,
                                context->step.step / interval) % total;
    for (u32 i = 0; i < sWildActivityCount; ++i)
        if (sWildActivities[i].priority == priority)
        {
            if (choice < sWildActivities[i].weight)
            {
                u64 remaining = interval - context->step.step % interval;
                selected.valid = 1; selected.interval = interval; selected.activity = sWildActivities[i];
                selected.until_step = UINT64_MAX - context->step.step < remaining
                    ? UINT64_MAX : context->step.step + remaining;
                return selected;
            }
            choice -= sWildActivities[i].weight;
        }
    return selected;
}
void RhhModsWildBehaviorOnStep(const struct RhhWorldStepSnapshot *step)
{
    if (!step || step->struct_size < sizeof(*step) || sWildBehaviorBusy || sWildBehaviorCount || !WildActorCommandReady()
        || NativeSaveTransactionActive() || step->run_id != RhhModsRunId()) return;
    struct RhhWildBehaviorContext *batch = sWildBehaviorBatch; u32 count = 0;
    struct RhhWildBehaviorPolicy policy = {.struct_size = sizeof(policy)};
    WildBehaviorPolicy(&policy);
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
    {
        struct RhhWildBehaviorContext item = {.struct_size = sizeof(item), .phase = RHH_WILD_BEHAVIOR_PROPOSAL,
            .step = *step, .actor.struct_size = sizeof(item.actor), .individual.struct_size = sizeof(item.individual)};
        if (WildActorAt(i, &item.actor) || WildActorIndividualAt(i, &item.individual) || item.actor.map != step->map) continue;
        int64_t dx = (int64_t)item.actor.x - step->x, dy = (int64_t)item.actor.y - step->y;
        if (!WildBehaviorWithinRadius(dx, dy, policy.radius_x, policy.radius_y)) continue;
        if (!RhhModsHasHook(RHH_WILD_BEHAVIOR_PROPOSE) && !RhhModsHasHook(RHH_WILD_ACTIVITY_COLLECT))
        {
            struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
            if (WildQueueAt(item.actor.handle, &queue) || !queue.count || !queue.owner_available || queue.revision == UINT64_MAX) continue;
        }
        batch[count++] = item;
    }
    qsort(batch, count, sizeof(batch[0]), CompareWildBehavior);
    sWildBehaviorBusy = TRUE;
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    for (u32 i = 0; i < count; ++i)
    {
        sWildQueueBatch[i] = (struct RhhWildQueue){.struct_size = sizeof(sWildQueueBatch[i])};
        if (WildQueueAt(batch[i].actor.handle, &sWildQueueBatch[i]) || !sWildQueueBatch[i].owner_available
            || sWildQueueBatch[i].revision == UINT64_MAX) sWildQueueBatch[i].count = 0;
        struct RhhWildQueueInterruption interruption = {.struct_size = sizeof(interruption)};
        if (sWildQueueBatch[i].count) (void)WildQueueInterruption(batch[i].actor.handle, &interruption);
        sWildSelectionBatch[i] = !sWildQueueBatch[i].count || interruption.enabled ? WildActivitySelect(&batch[i])
            : (struct RhhWildActivitySelection){.struct_size = sizeof(sWildSelectionBatch[i])};
        sWildQueueChosen[i] = sWildQueueBatch[i].count
            && !(interruption.enabled && sWildSelectionBatch[i].valid
                 && sWildSelectionBatch[i].activity.priority > interruption.priority);
        if (sWildQueueChosen[i]) sWildSelectionBatch[i] = (struct RhhWildActivitySelection){.struct_size = sizeof(sWildSelectionBatch[i])};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1,
            .value = sWildQueueChosen[i] ? sWildQueueBatch[i].actions[0]
                : sWildSelectionBatch[i].valid ? sWildSelectionBatch[i].activity.action : RHH_WILD_BEHAVIOR_NONE};
        sWildSelectionContext = &sWildSelectionBatch[i];
        WildBehaviorDispatch(RHH_WILD_BEHAVIOR_PROPOSE, &batch[i], &decision);
        sWildSelectionContext = NULL;
        if (decision.value <= RHH_WILD_BEHAVIOR_ENGAGE) batch[i].action = decision.value;
        else batch[i].status = RHH_WILD_ACTOR_INVALID;
        if (!decision.allowed) batch[i].status = RHH_WILD_ACTOR_DENIED;
    }
    RhhModsWorldTimeScopeEnd(&timeScope);
    sWildBehaviorCount = count;
    sWildBehaviorBusy = FALSE;
}
static void WildActivityCommit(const struct RhhWildBehaviorContext *context, const struct RhhWildActivitySelection *selected)
{
    if (!selected->valid && !context->action) return;
    struct RhhWildActivityChange change = {.struct_size = sizeof(change), .actor = context->actor,
        .before.struct_size = sizeof(change.before), .after.struct_size = sizeof(change.after)};
    u8 *state = WildActivityState();
    if (!state || WildActivityAt(context->actor.handle, &change.before)) return;
    u8 *p = WildActivityRecord(state, context->actor.slot), old[WILD_ACTIVITY_RECORD_SIZE];
    memcpy(old, p, sizeof(old)); memset(p, 0, WILD_ACTIVITY_RECORD_SIZE);
    if (selected->valid && selected->activity.action == context->action)
    {
        bool32 same = change.before.valid && change.before.id == selected->activity.id
            && change.before.owner == selected->activity.owner && change.before.action == context->action;
        WildActorWrite64(p, context->individual.serial); WildActorWrite64(p + 8, selected->activity.id);
        WildActorWrite64(p + 16, selected->activity.owner);
        WildActorWrite64(p + 24, same ? change.before.since_step : context->step.step);
        WildActorWrite64(p + 32, selected->until_step); p[40] = context->action;
    }
    if (ValidateWildActivityState(state, WILD_ACTIVITY_STATE_SIZE)
        || WildActivityAt(context->actor.handle, &change.after)) { memcpy(p, old, sizeof(old)); return; }
    if (change.before.valid == change.after.valid && change.before.id == change.after.id
        && change.before.owner == change.after.owner && change.before.action == change.after.action) return;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WILD_ACTIVITY_CHANGED,
        .run_seed = RhhModsRunSeed(), .event_key = change.after.valid ? change.after.id : change.before.id,
        .subject = context->actor.species, .ordinal = context->step.step,
        .args = {change.after.valid, change.after.action, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sWildActivityChangeContext = &change; RhhModsDispatch(&event, &decision); sWildActivityChangeContext = NULL;
}
static void WildQueueConsume(const struct RhhWildBehaviorContext *context, const struct RhhWildQueue *selected)
{
    if (!selected->count || context->action != selected->actions[0]) return;
    struct RhhWildQueueChange change = {.struct_size = sizeof(change), .cause = RHH_WILD_QUEUE_CONSUMED, .changed = TRUE,
        .actor = context->actor, .before.struct_size = sizeof(change.before), .after.struct_size = sizeof(change.after)};
    if (WildQueueAt(context->actor.handle, &change.before) || change.before.revision != selected->revision
        || change.before.owner != selected->owner || change.before.count != selected->count
        || change.before.individual.run_id != selected->individual.run_id
        || change.before.individual.serial != selected->individual.serial
        || memcmp(change.before.actions, selected->actions, sizeof(selected->actions))
        || change.before.revision == UINT64_MAX) return;
    u8 *state = WildQueueState(); if (!state) return;
    u8 *p = WildQueueRecord(state, context->actor.slot), old[WILD_QUEUE_RECORD_SIZE]; memcpy(old, p, sizeof(old));
    struct RhhWildQueueGoalChange goals = {.struct_size = sizeof(goals)};
    WildQueueReadGoal(p, &change.before, &goals.before);
    --p[24]; memmove(p + 25, p + 26, p[24]); p[25 + p[24]] = 0;
    if (p[33]) {
        u8 *guards = p + WILD_QUEUE_V1_RECORD_SIZE;
        memmove(guards, guards + WILD_QUEUE_GUARD_SIZE, p[24] * WILD_QUEUE_GUARD_SIZE);
        memset(guards + p[24] * WILD_QUEUE_GUARD_SIZE, 0, WILD_QUEUE_GUARD_SIZE);
        if (!p[24]) { p[33] = p[34] = p[37] = 0; WildActorWrite16(p + 35, 0); }
    }
    if (!p[24]) WildActorWrite64(p + 16, 0);
    WildActorWrite64(p + 8, change.before.revision + 1);
    if (ValidateWildQueueState(state, WILD_QUEUE_STATE_SIZE) || WildQueueAt(context->actor.handle, &change.after))
    { memcpy(p, old, sizeof(old)); return; }
    WildQueueReadGoal(p, &change.after, &goals.after);
    struct WildActorEntry *entry = &sWildActors[context->actor.slot];
    if (entry->moving && entry->step.ticket && entry->step.before.handle == context->actor.handle
        && entry->step.source == RHH_WILD_STEP_QUEUE && entry->step.queue_revision == selected->revision)
        entry->step.queue_consumed = TRUE;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    const struct RhhWildQueueGoalChange *previousGoal = sWildQueueGoalContext; sWildQueueGoalContext = &goals;
    WildQueueDispatch(RHH_WILD_QUEUE_CHANGED, &change, &decision);
    sWildQueueGoalContext = previousGoal;
}
static void WildBehaviorCommit(void)
{
    if (sWildBehaviorBusy || !sWildBehaviorCount) return;
    sWildBehaviorBusy = TRUE;
    struct RhhWildBehaviorContext *batch = sWildBehaviorBatch; u32 count = sWildBehaviorCount;
    sWildBehaviorCount = 0;
    for (u32 i = 0; i < count; ++i)
    {
        struct RhhWildBehaviorContext *item = &batch[i];
        if (!RhhModsHasActiveRun() || item->step.run_id != RhhModsRunId()) continue;
        if (!item->status)
        {
            struct RhhWildActorSnapshot current = {.struct_size = sizeof(current)};
            struct RhhWildActorIndividual individual = {.struct_size = sizeof(individual)};
            if (NativeSaveTransactionActive()) item->status = RHH_WILD_ACTOR_UNAVAILABLE;
            else if (WildActorFind(item->actor.handle, &current) || WildActorIndividual(item->actor.handle, &individual)
                || individual.run_id != item->individual.run_id || individual.serial != item->individual.serial
                || current.map != item->actor.map || current.x != item->actor.x || current.y != item->actor.y)
                item->status = RHH_WILD_ACTOR_INVALID;
            else if (!item->action) item->status = WildActorCommandReady() ? RHH_WILD_ACTOR_OK : RHH_WILD_ACTOR_UNAVAILABLE;
            else if (item->action == RHH_WILD_BEHAVIOR_ENGAGE) item->status = WildActorEngage(item->actor.handle);
            else {
                struct RhhWildQueueRouteState route = {.struct_size = sizeof(route)};
                if (sWildQueueChosen[i] && item->action == sWildQueueBatch[i].actions[0]
                    && (WildQueueRouteState(item->actor.handle, &route) || (route.guarded && route.status)))
                    item->status = route.status ? route.status : RHH_WILD_ACTOR_UNAVAILABLE;
                else item->status = WildActorStep(item->actor.handle, item->action);
                if (!item->status)
                {
                    struct WildActorEntry *entry = &sWildActors[item->actor.slot];
                    bool32 queued = sWildQueueChosen[i] && item->action == sWildQueueBatch[i].actions[0];
                    entry->step.source = queued ? RHH_WILD_STEP_QUEUE : RHH_WILD_STEP_BEHAVIOR;
                    if (queued) { entry->step.queue_owner = sWildQueueBatch[i].owner; entry->step.queue_revision = sWildQueueBatch[i].revision; }
                }
            }
        }
        if (!item->status) {
            if (sWildQueueChosen[i]) {
                u8 *state = WildQueueState();
                const u8 *p = state ? WildQueueRecord(state, item->actor.slot) : NULL;
                if (!p || !p[37] || item->action != sWildQueueBatch[i].actions[0])
                    WildQueueConsume(item, &sWildQueueBatch[i]);
            }
            else WildActivityCommit(item, &sWildSelectionBatch[i]);
        }
        if (!item->action) continue;
        item->phase = RHH_WILD_BEHAVIOR_REPORT;
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        sWildSelectionContext = &sWildSelectionBatch[i];
        WildBehaviorDispatch(RHH_WILD_BEHAVIOR_RESULT, item, &decision);
        sWildSelectionContext = NULL;
    }
    sWildBehaviorBusy = FALSE;
}
static const struct RhhWildBehaviorService sWildBehaviorService = {
    {sizeof(sWildBehaviorService), RHH_SERVICE_VERSION(1, 11)}, WildBehaviorContext, WildActivityOffer,
    WildActivitySelected, WildActivityAt, WildActivityChangeContext, WildBehaviorPolicy, WildBehaviorInspect,
    WildQueueAt, WildQueueReplace, WildQueueContext, WildQueueRoute, WildQueueHabitatRoute, WildQueueRouteState,
    WildQueueInterruption, WildQueueGoal, WildQueueReplan, WildQueueGoalContext,
    WildPolicyRevision, WildPolicyPreview, WildPolicyApply, WildPolicyContext, WildQueueCompletion
};
static int WildActorCatchOrigin(struct RhhWildCatchOrigin *out);
static int WildActorBattleCatch(struct RhhWildActorBattleCatch *out);
static int WildActorBattleView(struct RhhWildActorBattleView *out);
static const struct RhhWildActorService sWildActorService = {
    {sizeof(sWildActorService), RHH_SERVICE_VERSION(1, 18)}, ActorSlots, WildActorAt, WildActorFind, WildActorContext,
    WildActorSpawn, WildActorRemove, WildActorStep, WildActorEngage, WildActorBattleResult,
    WildActorIndividual, WildActorFindIndividual, WildActorEventIndividual, WildActorNeighbors, WildActorRoute, WildActorRouteHabitat,
    WildActorSpawnSpecies, WildActorRouteTrace, WildActorRouteHabitatTrace, WildStepPending, WildStepContext,
    WildActorCapacity, WildActorSpawnPreflight, WildActorSpeciesSpawnPreflight, WildActorCatchOrigin, WildActorBattleCatch,
    WildActorBattleView
};
static void WildActorNotify(u32 kind, u32 cause, const struct RhhWildActorSnapshot *actor,
                            const struct RhhWildActorIndividual *individual)
{
    if (!RhhModsHasActiveRun() || sWildActorContext) return;
    struct RhhWildActorEvent context = {.struct_size = sizeof(context), .cause = cause, .actor = *actor};
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = actor->map, .subject = actor->species, .ordinal = actor->handle, .args = {actor->slot, cause}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    sWildActorContext = &context; sWildActorIndividualContext = individual;
    RhhModsDispatch(&event, &decision);
    sWildActorIndividualContext = NULL; sWildActorContext = NULL;
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
}
void ModsWildActorEngaged(struct ObjectEvent *object)
{
    int slot = WildActorSlot(object);
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    ModsWildActorBattleReset();
    if (slot >= 0 && !WildActorAt(slot, &actor))
    {
        sWildActorBattle.actor = actor; sWildActorBattle.runId = RhhModsRunId(); sWildActorBattle.armed = TRUE;
        sWildActorBattle.individual.struct_size = sizeof(sWildActorBattle.individual);
        bool32 hasIndividual = !WildActorIndividualAt(slot, &sWildActorBattle.individual);
        WildActorNotify(RHH_WILD_ACTOR_ENGAGED, RHH_WILD_ACTOR_BATTLE, &actor,
                        hasIndividual ? &sWildActorBattle.individual : NULL);
    }
}
void ModsWildActorBattleReset(void)
{ memset(&sWildActorBattle, 0, sizeof(sWildActorBattle)); memset(&sWildActorCatch, 0, sizeof(sWildActorCatch)); }
void ModsWildActorBattleOpponentSlot(u32 slot)
{
    if (sWildActorBattle.armed && !sWildActorBattle.transitioned && sWildActorBattle.runId == RhhModsRunId() && slot < 2)
        sWildActorBattle.opponentSlot = slot;
}
void ModsWildActorBattleBind(u64 battleId)
{
    if (sWildActorBattle.armed && sWildActorBattle.transitioned && !sWildActorBattle.battleId
        && sWildActorBattle.runId == RhhModsRunId()) sWildActorBattle.battleId = battleId;
}
bool32 ModsWildActorBattleTracking(void)
{
    return sWildActorBattle.armed && sWildActorBattle.transitioned && sWildActorBattle.opponentValid
        && RhhModsHasActiveRun() && sWildActorBattle.runId == RhhModsRunId();
}
static bool32 SameWildCatchWitness(const struct BoxPokemon *a, const struct BoxPokemon *b)
{
    if (!a || !b || !a->hasSpecies || !b->hasSpecies || a->isEgg || b->isEgg || a->isBadEgg || b->isBadEgg
        || a->personality != b->personality || a->otId != b->otId || memcmp(a->otName, b->otName, sizeof(a->otName))) return FALSE;
    struct BoxPokemon first = *a, second = *b;
    u32 species = GetBoxMonData(&first, MON_DATA_SPECIES), other = GetBoxMonData(&second, MON_DATA_SPECIES);
    if (!species || species >= NUM_SPECIES || species != other
        || GetBoxMonData(&first, MON_DATA_EXP) != GetBoxMonData(&second, MON_DATA_EXP)) return FALSE;
    for (u32 i = 0; i < NUM_STATS; ++i)
        if (GetBoxMonData(&first, MON_DATA_HP_IV + i) != GetBoxMonData(&second, MON_DATA_HP_IV + i)
            || GetBoxMonData(&first, MON_DATA_HP_EV + i) != GetBoxMonData(&second, MON_DATA_HP_EV + i)) return FALSE;
    return !first.isBadEgg && !second.isBadEgg;
}
static int WildActorBattleView(struct RhhWildActorBattleView *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildActorBattle.armed || !sWildActorBattle.transitioned
        || !sWildActorBattle.opponentValid || !sWildActorBattle.battleId || !RhhModsHasActiveRun()
        || sWildActorBattle.runId != RhhModsRunId() || sWildActorBattle.opponentSlot >= 2
        || !SameWildCatchWitness(&gParties[B_TRAINER_OPPONENT_A][sWildActorBattle.opponentSlot].box,
                                 &sWildActorBattle.opponent)) return -1;
    const struct RhhBattleService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/battle", RHH_SERVICE_VERSION(1, 0));
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (!service || service->current(&battle) || battle.battle_id != sWildActorBattle.battleId
        || battle.run_id != sWildActorBattle.runId) return -1;
    struct RhhWildActorBattleView result = {.struct_size = sizeof(result), .battle_id = battle.battle_id,
        .opponent_party_slot = sWildActorBattle.opponentSlot, .actor = sWildActorBattle.actor};
    if (sWildActorBattle.individual.run_id == sWildActorBattle.runId && sWildActorBattle.individual.serial) {
        result.flags |= RHH_WILD_BATTLE_INDIVIDUAL_AVAILABLE; result.individual = sWildActorBattle.individual;
    }
    for (u32 i = 0; i < battle.participant_count; ++i) {
        const struct RhhBattleParticipant *participant = &battle.participants[i];
        if (participant->owner != B_TRAINER_OPPONENT_A || participant->party_slot != result.opponent_party_slot) continue;
        if (result.flags & RHH_WILD_BATTLE_PARTICIPANT_AVAILABLE) return -1;
        result.flags |= RHH_WILD_BATTLE_PARTICIPANT_AVAILABLE; result.participant = *participant;
    }
    *out = result; return 0;
}
static int WildTraitBattle(RhhContentKey tag, struct RhhWildTraitResult *out)
{
    if (!tag || !out || out->struct_size < sizeof(*out)) return -1;
    const struct WildBattleTraits *traits;
    if (RhhModsCurrentEvent() == RHH_WILD_ACTOR_BATTLE_ENDED && sWildActorBattleContext && sWildBattleEndTraitsValid)
        traits = &sWildBattleEndTraits;
    else {
        struct RhhWildActorBattleView view = {.struct_size = sizeof(view)};
        if (WildActorBattleView(&view)) return -1;
        traits = &sWildActorBattle.traits;
    }
    for (u32 i = 0; i < traits->count; ++i)
        if (traits->entries[i].tag == tag && traits->entries[i].valid)
        { *out = traits->entries[i].result; return 0; }
    return -1;
}
static int WildProfileBattle(RhhContentKey owner, RhhContentKey field, struct RhhWildProfileValue *out)
{
    if (!owner || !field || !out || out->struct_size < sizeof(*out)) return -1;
    const struct WildBattleProfile *profile;
    if (RhhModsCurrentEvent() == RHH_WILD_ACTOR_BATTLE_ENDED && sWildActorBattleContext && sWildBattleEndTraitsValid)
        profile = &sWildBattleEndProfile;
    else {
        struct RhhWildActorBattleView view = {.struct_size = sizeof(view)};
        if (WildActorBattleView(&view)) return -1;
        profile = &sWildActorBattle.profile;
    }
    if (!profile->valid) return -1;
    WildProfileRead(profile->record, profile->individual, owner, field, out); return 0;
}
void ModsWildActorBattleTransition(void)
{
    u32 slot = sWildActorBattle.actor.slot;
    if (!sWildActorBattle.armed || sWildActorBattle.runId != RhhModsRunId() || slot >= OBJECT_EVENTS_COUNT
        || !WildActorMatches(slot) || sWildActors[slot].handle != sWildActorBattle.actor.handle
        || gSpecialVar_LastTalked != sWildActorBattle.actor.local_id)
    { ModsWildActorBattleReset(); return; }
    sWildActorBattle.flags = (gBattleTypeFlags & BATTLE_TYPE_DOUBLE ? RHH_WILD_BATTLE_DOUBLE : 0)
        | (gBattleTypeFlags & BATTLE_TYPE_ROAMER ? RHH_WILD_BATTLE_ROAMER : 0)
        | (gBattleTypeFlags & BATTLE_TYPE_SAFARI ? RHH_WILD_BATTLE_SAFARI : 0);
    struct RhhWildActorIndividual individual = {.struct_size = sizeof(individual)};
    if (!WildActorIndividualAt(slot, &individual)) sWildActorBattle.individual = individual;
    /* The encounter preparation records slot 1 only when it actually makes
     * a second wild Pokémon; follower battles may use the double flag alone. */
    u32 opponentSlot = sWildActorBattle.opponentSlot;
    const struct BoxPokemon *opponent = &gParties[B_TRAINER_OPPONENT_A][opponentSlot].box;
    if (SameWildCatchWitness(opponent, opponent)
        && (!sWildActors[slot].hasMon || SameWildCatchWitness(opponent, &sWildActors[slot].mon.box))) {
        sWildActorBattle.opponent = *opponent; sWildActorBattle.opponentSlot = opponentSlot;
        sWildActorBattle.opponentValid = TRUE;
    }
    if (sWildActorBattle.traits.count) {
        struct RhhWorldTimeScope traitTime; RhhModsWorldTimeScopeBegin(&traitTime);
        for (u32 i = 0; i < sWildActorBattle.traits.count; ++i) {
            struct WildBattleTraitEntry *entry = &sWildActorBattle.traits.entries[i];
            entry->result.struct_size = sizeof(entry->result);
            entry->valid = !WildTraitQuery(sWildActorBattle.actor.handle, entry->tag, &entry->result);
        }
        RhhModsWorldTimeScopeEnd(&traitTime);
    }
    if (!sWildActorBattle.armed || sWildActorBattle.runId != RhhModsRunId() || !WildActorMatches(slot)
        || sWildActors[slot].handle != sWildActorBattle.actor.handle) { ModsWildActorBattleReset(); return; }
    u8 *profileState = WildProfileState();
    if (profileState && !WildActorIndividualAt(slot, &individual)) {
        sWildActorBattle.profile.valid = TRUE;
        sWildActorBattle.profile.individual = individual.serial;
        memcpy(sWildActorBattle.profile.record, WildProfileRecord(profileState, slot), WILD_PROFILE_RECORD_SIZE);
    }
    sWildActorBattle.transitioned = TRUE;
}
void ModsWildActorCatchCapture(const struct BoxPokemon *source, u32 battler, const struct RhhCatchPlacement *placement)
{
    memset(&sWildActorCatch, 0, sizeof(sWildActorCatch));
    if (!source || !placement || placement->struct_size < sizeof(*placement) || placement->cause != RHH_CATCH_ACQUIRED
        || !(placement->flags & RHH_CATCH_BEFORE_VALID) || !sWildActorBattle.armed || !sWildActorBattle.transitioned
        || !sWildActorBattle.opponentValid || sWildActorBattle.runId != RhhModsRunId() || !gMain.inBattle
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || !gBattleStruct
        || GetBattlerTrainer(battler) != B_TRAINER_OPPONENT_A
        || gBattlerPartyIndexes[battler] != sWildActorBattle.opponentSlot
        || source != &gParties[B_TRAINER_OPPONENT_A][sWildActorBattle.opponentSlot].box
        || !SameWildCatchWitness(source, &sWildActorBattle.opponent)) return;
    sWildActorCatch.valid = TRUE; sWildActorCatch.battler = battler;
    sWildActorCatch.opponentSlot = sWildActorBattle.opponentSlot; sWildActorCatch.runId = RhhModsRunId();
    sWildActorCatch.handle = sWildActorBattle.actor.handle; sWildActorCatch.actor = sWildActorBattle.actor;
    sWildActorCatch.individual = sWildActorBattle.individual; sWildActorCatch.before = placement->before;
}
static int CheckedWildCatchOrigin(const struct RhhCatchPlacement *placement, struct RhhWildCatchOrigin *out)
{
    if (!placement || !out || !sWildActorCatch.valid || !sWildActorBattle.armed || !sWildActorBattle.transitioned
        || !RhhModsHasActiveRun() || sWildActorCatch.runId != RhhModsRunId()
        || sWildActorBattle.runId != sWildActorCatch.runId || sWildActorBattle.actor.handle != sWildActorCatch.handle) return -1;
    if (placement->cause != RHH_CATCH_ACQUIRED
        || (placement->flags & (RHH_CATCH_BEFORE_VALID | RHH_CATCH_AFTER_VALID)) != (RHH_CATCH_BEFORE_VALID | RHH_CATCH_AFTER_VALID)
        || placement->run_id != sWildActorCatch.runId || placement->source_slot != sWildActorCatch.battler
        || !RhhMonSnapshotsEqual(&placement->before, &sWildActorCatch.before)
        || placement->after.species != placement->before.species
        || (u32)placement->after.identity != (u32)placement->before.identity
        || placement->after.level != placement->before.level || placement->after.nature != placement->before.nature
        || placement->after.stat_nature != placement->before.stat_nature || placement->after.is_egg
        || memcmp(placement->after.ivs, placement->before.ivs, sizeof(placement->after.ivs))
        || memcmp(placement->after.evs, placement->before.evs, sizeof(placement->after.evs))) return -1;
    struct RhhWildCatchOrigin result = {.struct_size = sizeof(result), .opponent_party_slot = sWildActorCatch.opponentSlot,
        .actor = sWildActorCatch.actor, .placement = *placement};
    if (sWildActorCatch.individual.run_id == sWildActorCatch.runId && sWildActorCatch.individual.serial) {
        result.flags = RHH_WILD_CATCH_INDIVIDUAL_AVAILABLE; result.individual = sWildActorCatch.individual;
    }
    *out = result; return 0;
}
static int WildActorCatchOrigin(struct RhhWildCatchOrigin *out)
{
    if (!out || out->struct_size < sizeof(*out) || RhhModsCurrentEvent() != RHH_OWNED_CATCH_PLACED) return -1;
    const struct RhhOwnedMonService *owned = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 10));
    struct RhhCatchPlacement placement = {.struct_size = sizeof(placement)};
    return !owned || owned->catch_placement_context(&placement) ? -1 : CheckedWildCatchOrigin(&placement, out);
}
void ModsWildActorCatchCommitted(const struct RhhCatchPlacement *placement, const struct BoxPokemon *destination)
{
    struct RhhWildCatchOrigin origin;
    if (CheckedWildCatchOrigin(placement, &origin)) return;
    const struct BoxPokemon *opponent = &gParties[B_TRAINER_OPPONENT_A][sWildActorBattle.opponentSlot].box;
    /* Capture rekeys the source in place. Follow it only if the final
     * destination and source still have the same checked witness. */
    if (SameWildCatchWitness(opponent, destination)) sWildActorBattle.opponent = *opponent;
    sWildActorBattle.catchResult = (struct RhhWildActorBattleCatch){.struct_size = sizeof(sWildActorBattle.catchResult),
        .opponent_party_slot = origin.opponent_party_slot, .destination_location = placement->destination_location,
        .destination_box = placement->destination_box, .destination_slot = placement->destination_slot, .after = placement->after};
    sWildActorBattle.catchValid = TRUE;
}
static int WildActorBattleCatch(struct RhhWildActorBattleCatch *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildActorBattleCatchContext
        || RhhModsCurrentEvent() != RHH_WILD_ACTOR_BATTLE_ENDED) return -1;
    *out = *sWildActorBattleCatchContext; return 0;
}
static u32 WildBattleOutcome(u32 outcome)
{
    switch (outcome & ~B_OUTCOME_LINK_BATTLE_RAN)
    {
    case B_OUTCOME_WON: return RHH_WILD_BATTLE_WON;
    case B_OUTCOME_LOST: return RHH_WILD_BATTLE_LOST;
    case B_OUTCOME_DREW: return RHH_WILD_BATTLE_DREW;
    case B_OUTCOME_RAN: return RHH_WILD_BATTLE_RAN;
    case B_OUTCOME_PLAYER_TELEPORTED: return RHH_WILD_BATTLE_PLAYER_TELEPORTED;
    case B_OUTCOME_MON_FLED: return RHH_WILD_BATTLE_MON_FLED;
    case B_OUTCOME_CAUGHT: return RHH_WILD_BATTLE_CAUGHT;
    case B_OUTCOME_NO_SAFARI_BALLS: return RHH_WILD_BATTLE_OUT_OF_BALLS;
    case B_OUTCOME_FORFEITED: return RHH_WILD_BATTLE_FORFEITED;
    case B_OUTCOME_MON_TELEPORTED: return RHH_WILD_BATTLE_MON_TELEPORTED;
    default: return RHH_WILD_BATTLE_UNKNOWN;
    }
}
void ModsWildActorBattleFinished(u32 outcome)
{
    if (!sWildActorBattle.armed || !sWildActorBattle.transitioned || !RhhModsHasActiveRun()
        || sWildActorBattle.runId != RhhModsRunId() || sWildActorBattleContext)
    { ModsWildActorBattleReset(); return; }
    struct RhhWildActorBattleResult context = {.struct_size = sizeof(context),
        .outcome = WildBattleOutcome(outcome), .flags = sWildActorBattle.flags, .actor = sWildActorBattle.actor};
    struct RhhWildActorIndividual individual = sWildActorBattle.individual;
    struct RhhWildActorBattleCatch caught = sWildActorBattle.catchResult;
    bool32 caughtValid = sWildActorBattle.catchValid;
    sWildBattleEndTraitsValid = sWildActorBattle.opponentValid
        && SameWildCatchWitness(&gParties[B_TRAINER_OPPONENT_A][sWildActorBattle.opponentSlot].box, &sWildActorBattle.opponent);
    if (sWildBattleEndTraitsValid) {
        sWildBattleEndTraits = sWildActorBattle.traits;
        sWildBattleEndProfile = sWildActorBattle.profile;
    }
    ModsWildActorBattleReset();
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WILD_ACTOR_BATTLE_ENDED,
        .run_seed = RhhModsRunSeed(), .event_key = context.actor.map, .subject = context.actor.species,
        .ordinal = context.actor.handle, .args = {context.outcome, context.flags}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    sWildActorBattleContext = &context;
    sWildActorBattleCatchContext = caughtValid ? &caught : NULL;
    sWildActorIndividualContext = individual.run_id && individual.serial ? &individual : NULL;
    RhhModsDispatch(&event, &decision);
    sWildActorIndividualContext = NULL; sWildActorBattleContext = NULL; sWildActorBattleCatchContext = NULL;
    sWildBattleEndTraitsValid = FALSE;
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
}
static void WildStepFinish(u32 slot, u32 status, bool32 sample)
{
    struct WildActorEntry *entry = &sWildActors[slot];
    struct RhhWildStepResult result = entry->step;
    if (!result.ticket) { entry->moving = FALSE; memset(&entry->step, 0, sizeof(entry->step)); return; }
    result.status = status; result.after.struct_size = sizeof(result.after);
    if (sample && !WildActorAt(slot, &result.after) && result.after.handle == result.before.handle)
    {
        result.after_valid = TRUE;
        if (status == RHH_WILD_STEP_ARRIVED
            && (result.after.x != result.expected.x || result.after.y != result.expected.y
                || result.after.elevation != result.expected.elevation || result.after.facing != result.expected.facing))
            result.status = RHH_WILD_STEP_DEVIATED;
    }
    else if (status == RHH_WILD_STEP_ARRIVED) result.status = RHH_WILD_STEP_LOST;
    if (result.status == RHH_WILD_STEP_ARRIVED && result.source == RHH_WILD_STEP_QUEUE && !result.queue_consumed)
    {
        u8 *state = WildQueueState();
        const u8 *p = state ? WildQueueRecord(state, slot) : NULL;
        struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
        if (p && p[37] && !WildQueueAt(result.before.handle, &queue) && queue.count
            && queue.revision == result.queue_revision && queue.owner == result.queue_owner
            && queue.individual.run_id == result.individual.run_id && queue.individual.serial == result.individual.serial
            && queue.actions[0] == result.direction)
        {
            struct RhhWildBehaviorContext context = {.actor = result.before, .action = result.direction};
            bool32 previousBusy = sWildBehaviorBusy; sWildBehaviorBusy = TRUE;
            WildQueueConsume(&context, &queue);
            sWildBehaviorBusy = previousBusy;
            result.queue_consumed = entry->step.queue_consumed;
        }
    }
    entry->moving = FALSE; memset(&entry->step, 0, sizeof(entry->step));
    WildStepNotify(&result);
}
void ModsWildActorsUpdate(void)
{
    WildBehaviorCommit();
    if (gMain.inBattle || gMain.callback2 != CB2_Overworld) return;
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
    {
        struct WildActorEntry *entry = &sWildActors[i];
        if (!WildActorMatches(i)) {
            if (entry->moving && !NativeSaveTransactionActive()) WildStepFinish(i, RHH_WILD_STEP_LOST, FALSE);
            continue;
        }
        struct ObjectEvent *object = &gObjectEvents[i];
        if (!entry->observed && entry->individual && WildActorWasObserved(i)) entry->observed = TRUE;
        if (!NativeSaveTransactionActive() && !entry->conceal.mode)
        {
            WildCueUpdate(i);
            if (WildCueExists(i) && entry->cueAge < 2) ++entry->cueAge;
        }
        if (entry->moving && !NativeSaveTransactionActive())
        {
            if (object->spriteId >= MAX_SPRITES || !gSprites[object->spriteId].inUse
                || !object->heldMovementActive || object->movementActionId != entry->moveAction)
                WildStepFinish(i, RHH_WILD_STEP_INTERRUPTED, TRUE);
            else if (object->heldMovementFinished)
            { ObjectEventClearHeldMovementIfFinished(object); WildStepFinish(i, RHH_WILD_STEP_ARRIVED, TRUE); }
        }
        if (entry->conceal.mode)
        {
            u32 cause = entry->concealMoved ? RHH_WILD_CONCEAL_MOVED : 0;
            if (!cause && (entry->conceal.reveal_flags & RHH_WILD_REVEAL_ON_APPROACH)
                && gPlayerAvatar.objectEventId < OBJECT_EVENTS_COUNT)
            {
                const struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];
                if (player->active && player->isPlayer && player->mapGroup == object->mapGroup
                    && player->mapNum == object->mapNum && AreElevationsCompatible(player->currentElevation, object->currentElevation)
                    && abs(player->currentCoords.x - object->currentCoords.x) + abs(player->currentCoords.y - object->currentCoords.y)
                       <= entry->conceal.reveal_radius) cause = RHH_WILD_CONCEAL_APPROACHED;
            }
            if (cause) WildConcealAutoReveal(i, cause);
        }
        if (!entry->individual && !entry->needsSave && RhhModsRunCommitted())
        {
            struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
            if (WildActorAt(i, &actor) || actor.level < MIN_LEVEL || actor.level > MAX_LEVEL) continue;
            u64 individual = WildActorAllocateIndividual(FALSE);
            if (!individual) continue;
            const struct Pokemon *mon = entry->hasMon && entry->graphics == object->graphicsId ? &entry->mon : NULL;
            if (!WildActorStore(i, object, mon, individual)) continue;
            entry->individual = individual; entry->identified = TRUE;
            WildConcealStore(i, object, &entry->conceal);
            struct RhhWildActorIndividual id = {.struct_size = sizeof(id), .run_id = entry->runId, .serial = individual};
            WildActorNotify(RHH_WILD_ACTOR_IDENTIFIED, RHH_WILD_ACTOR_ID_ASSIGNED, &actor, &id);
        }
    }
}
void ModsWildActorPrepareSaveObject(u32 slot, struct ObjectEvent *copy)
{
    if (!copy || slot >= OBJECT_EVENTS_COUNT || !WildActorMatches(slot) || !sWildActors[slot].moving
        || !copy->heldMovementActive || copy->movementActionId != sWildActors[slot].moveAction) return;
    /* The live animation may continue. A restored object has no sprite action
     * state, so save it settled on its current destination instead. */
    copy->previousCoords = copy->currentCoords;
    copy->previousElevation = copy->currentElevation;
    copy->heldMovementActive = FALSE; copy->heldMovementFinished = FALSE;
    copy->movementActionId = MOVEMENT_ACTION_NONE; copy->directionOverwrite = DIR_NONE;
}
static void WildActorTrack(struct ObjectEvent *object, const struct Pokemon *preserved, u64 individual, u32 cause,
                           const struct RhhWildConcealment *conceal, bool32 notify)
{
    int slot = WildActorSlot(object);
    if (slot < 0 || !object->active || object->trainerType != TRAINER_TYPE_OW_WILD_ENCOUNTER
        || !IS_OW_MON_OBJ(object) || sWildActorSerial == UINT64_MAX) return;
    struct WildActorEntry *entry = &sWildActors[slot]; memset(entry, 0, sizeof(*entry));
    entry->handle = ++sWildActorSerial; entry->runId = RhhModsRunId();
    entry->graphics = object->graphicsId; entry->localId = object->localId;
    entry->mapGroup = object->mapGroup; entry->mapNum = object->mapNum;
    entry->presentationEligible = WE_OW_ENCOUNTERS && gSaveBlock1Ptr && !GetSafariZoneFlag()
        && IsOrdinaryGeneratedOverworldWildEncounter(object) && GetCurrentMapWildMonHeaderId() != HEADER_NONE;
    entry->needsSave = cause == RHH_WILD_ACTOR_RESTORE && !individual;
    if (conceal) entry->conceal = *conceal;
    if (WildActorStore(slot, object, preserved, individual))
    {
        entry->individual = individual; entry->identified = individual && RhhModsRunCommitted();
        if (preserved) { entry->mon = *preserved; entry->hasMon = TRUE; }
    }
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id)};
    if (entry->individual && WildActorWasObserved(slot)) entry->observed = TRUE;
    /* Saved objects are restored before the map, weather and sprite systems.
     * The first overworld update creates their effects once those are ready. */
    if (cause != RHH_WILD_ACTOR_RESTORE && !entry->conceal.mode && !NativeSaveTransactionActive()) WildCueUpdate(slot);
    if (notify && !WildActorAt(slot, &actor))
        WildActorNotify(RHH_WILD_ACTOR_SPAWNED, cause, &actor, !WildActorIndividualAt(slot, &id) ? &id : NULL);
}
void ModsWildActorSpawned(struct ObjectEvent *object, const struct Pokemon *preserved)
{ WildActorTrack(object, preserved, 0, RHH_WILD_ACTOR_SPAWN, NULL, TRUE); }
bool32 ModsWildActorBeforeRemove(struct ObjectEvent *object, struct RhhWildActorSnapshot *out)
{
    int slot = WildActorSlot(object);
    return slot >= 0 && !WildActorAt(slot, out);
}
void ModsWildActorAfterRemove(struct ObjectEvent *object, const struct RhhWildActorSnapshot *before, u32 cause)
{
    int slot = WildActorSlot(object);
    if (slot < 0) return;
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id)};
    bool32 hasId = before && sWildActors[slot].handle == before->handle && sWildActors[slot].identified
        && sWildActors[slot].runId == RhhModsRunId() && RhhModsRunCommitted();
    struct RhhWildStepResult step = {0};
    if (hasId) { id.run_id = sWildActors[slot].runId; id.serial = sWildActors[slot].individual; }
    if (!before || sWildActors[slot].handle == before->handle)
    {
        if (sWildActors[slot].moving && sWildActors[slot].step.ticket)
        {
            step = sWildActors[slot].step;
            step.status = before ? RHH_WILD_STEP_REMOVED : RHH_WILD_STEP_LOST;
        }
        if (sWildActors[slot].runId && sWildActors[slot].runId == RhhModsRunId())
        {
            u8 *state = WildActorState();
            if (state) memset(WildActorRecord(state, slot), 0, WILD_ACTOR_RECORD_SIZE);
            WildConcealStore(slot, object, NULL);
            state = WildTraitState();
            if (state) memset(WildTraitRecord(state, slot), 0, WILD_TRAIT_RECORD_SIZE);
            state = WildProfileState();
            if (state) memset(WildProfileRecord(state, slot), 0, WILD_PROFILE_RECORD_SIZE);
            state = WildActivityState();
            if (state) memset(WildActivityRecord(state, slot), 0, WILD_ACTIVITY_RECORD_SIZE);
            state = WildQueueState();
            if (state) memset(WildQueueRecord(state, slot), 0, WILD_QUEUE_RECORD_SIZE);
        }
        memset(&sWildActors[slot], 0, sizeof(sWildActors[slot]));
    }
    if (step.ticket) WildStepNotify(&step);
    if (before) WildActorNotify(RHH_WILD_ACTOR_REMOVED,
        before->handle == sWildDormantRemoving ? RHH_WILD_ACTOR_SUSPENDED : cause, before, hasId ? &id : NULL);
}
static u32 WildDormantCount(void)
{
    return RhhModsRunCommitted() && (!sWildDormantCount || sWildDormantRun == RhhModsRunId())
        ? sWildDormantCount : 0;
}
static int WildDormantIndex(const struct RhhWildActorIndividual *id)
{
    if (!id || id->struct_size < sizeof(*id) || id->reserved || !id->serial || !id->run_id
        || !RhhModsRunCommitted() || id->run_id != RhhModsRunId()) return -1;
    for (u32 i = 0; i < WildDormantCount(); ++i)
        if (WildActorRead64(sWildDormant[i]) == id->serial) return i;
    return -1;
}
static int WildDormantAt(u32 index, struct RhhWildDormantActor *out)
{
    if (!out || out->struct_size < sizeof(*out) || index >= WildDormantCount()) return -1;
    const u8 *p = sWildDormant[index]; struct Pokemon mon;
    memcpy(&mon, p + WILD_DORMANT_MON, sizeof(mon));
    struct RhhWildDormantActor result = {.struct_size = sizeof(result), .level = mon.level,
        .facing = p[32], .elevation = p[33], .no_despawn = p[34],
        .owner_available = RhhModsRegisteredKey(WildActorRead64(p + 8)),
        .individual = {.struct_size = sizeof(struct RhhWildActorIndividual),
                       .run_id = sWildDormantRun, .serial = WildActorRead64(p)},
        .owner = WildActorRead64(p + 8), .map = WildActorRead64(p + 16),
        .species = RhhContentKeyForId(RHH_SPECIES, GetMonData(&mon, MON_DATA_SPECIES)),
        .x = WildActorRead32(p + 24), .y = WildActorRead32(p + 28)};
    if (!result.species) return -1;
    *out = result; return 0;
}
static int WildDormantFind(const struct RhhWildActorIndividual *id, struct RhhWildDormantActor *out)
{
    int index = WildDormantIndex(id);
    return index < 0 ? -1 : WildDormantAt(index, out);
}
static void WildDormantRemove(u32 index)
{
    memmove((u8 *)sWildDormant + index * WILD_DORMANT_RECORD,
            (u8 *)sWildDormant + (index + 1) * WILD_DORMANT_RECORD,
            (sWildDormantCount - index - 1) * WILD_DORMANT_RECORD);
    memset(sWildDormant[--sWildDormantCount], 0, WILD_DORMANT_RECORD);
    if (!sWildDormantCount) sWildDormantRun = 0;
}
static int WildDormantSuspend(u64 handle, RhhContentKey owner, struct RhhWildDormantActor *out)
{
    if (!handle || !owner || !RhhModsRegisteredKey(owner) || !out || out->struct_size < sizeof(*out))
        return RHH_WILD_ACTOR_INVALID;
    if (sWildDormantBusy || !WildActorCommandReady() || !RhhModsRunCommitted() || NativeSaveTransactionActive())
        return RHH_WILD_ACTOR_UNAVAILABLE;
    if (sWildDormantCount >= (UINT32_MAX - WILD_DORMANT_HEADER) / WILD_DORMANT_RECORD)
        return RHH_WILD_ACTOR_FULL;
    if (sWildDormantCount && sWildDormantRun != RhhModsRunId()) return RHH_WILD_ACTOR_UNAVAILABLE;
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id)};
    u8 record[WILD_DORMANT_RECORD];
    if (WildActorFind(handle, &actor) || WildActorIndividual(handle, &id)
        || RhhModsWildDormantPrepare(handle, owner, record, sizeof(record))) return RHH_WILD_ACTOR_INVALID;
    struct ObjectEvent *object = &gObjectEvents[actor.slot];
    if (object->mapGroup != gSaveBlock1Ptr->location.mapGroup || object->mapNum != gSaveBlock1Ptr->location.mapNum
        || object->spriteId >= MAX_SPRITES || !gSprites[object->spriteId].inUse) return RHH_WILD_ACTOR_UNAVAILABLE;
    s32 gridX = actor.x + MAP_OFFSET, gridY = actor.y + MAP_OFFSET;
    if (!AreCoordsInsidePlayerMap(gridX, gridY)) return RHH_WILD_ACTOR_BLOCKED;
    u32 tileElevation = MapGridGetElevationAt(gridX, gridY);
    if (tileElevation == ELEVATION_TRANSITION || tileElevation == ELEVATION_MULTI_LEVEL
        || tileElevation != actor.elevation || MapGridGetCollisionAt(gridX, gridY)) return RHH_WILD_ACTOR_BLOCKED;
    u32 index = 0;
    while (index < sWildDormantCount && WildActorRead64(sWildDormant[index]) < id.serial) ++index;
    if (index < sWildDormantCount && WildActorRead64(sWildDormant[index]) == id.serial) return RHH_WILD_ACTOR_INVALID;
    if (sWildDormantCount == sWildDormantCapacity)
    {
        size_t capacity = sWildDormantCapacity ? sWildDormantCapacity * 2 : 16;
        if (capacity > SIZE_MAX / WILD_DORMANT_RECORD) return RHH_WILD_ACTOR_FULL;
        void *records = realloc(sWildDormant, capacity * WILD_DORMANT_RECORD);
        if (!records) return RHH_WILD_ACTOR_FULL;
        sWildDormant = records; sWildDormantCapacity = capacity;
    }
    sWildDormantBusy = TRUE; sWildDormantRemoving = handle;
    memmove((u8 *)sWildDormant + (index + 1) * WILD_DORMANT_RECORD,
            (u8 *)sWildDormant + index * WILD_DORMANT_RECORD,
            (sWildDormantCount - index) * WILD_DORMANT_RECORD);
    memcpy(sWildDormant[index], record, sizeof(record)); ++sWildDormantCount; sWildDormantRun = id.run_id;
    struct RhhWildDormantActor result = {.struct_size = sizeof(result)};
    WildDormantAt(index, &result);
    RemoveObjectEvent(object);
    sWildDormantRemoving = 0; sWildDormantBusy = FALSE;
    *out = result; return RHH_WILD_ACTOR_OK;
}
static int WildDormantResume(const struct RhhWildActorIndividual *id, RhhContentKey owner, struct RhhWildActorSnapshot *out)
{
    if (!id || id->struct_size < sizeof(*id) || id->reserved || !owner || !RhhModsRegisteredKey(owner)
        || !out || out->struct_size < sizeof(*out)) return RHH_WILD_ACTOR_INVALID;
    if (sWildDormantBusy || !WildActorCommandReady() || NativeSaveTransactionActive() || sWildActorSerial == UINT64_MAX)
        return RHH_WILD_ACTOR_UNAVAILABLE;
    int index = WildDormantIndex(id);
    if (index < 0 || WildActorRead64(sWildDormant[index] + 8) != owner) return RHH_WILD_ACTOR_INVALID;
    const u8 *p = sWildDormant[index];
    RhhContentKey map = RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8)
        | (u8)gSaveBlock1Ptr->location.mapNum);
    if (WildActorRead64(p + 16) != map) return RHH_WILD_ACTOR_UNAVAILABLE;
    struct RhhWildActorSnapshot existing = {.struct_size = sizeof(existing)};
    if (!WildActorFindIndividual(id, &existing)) return RHH_WILD_ACTOR_INVALID;
    u8 *states[] = {WildActorState(), WildTraitState(), WildProfileState(), WildActivityState(), WildQueueState(), WildConcealState()};
    for (u32 i = 0; i < ARRAY_COUNT(states); ++i) if (!states[i]) return RHH_WILD_ACTOR_UNAVAILABLE;
    struct Pokemon mon; memcpy(&mon, p + WILD_DORMANT_MON, sizeof(mon));
    sWildDormantBusy = TRUE;
    u32 slot = UINT32_MAX;
    int status = TrySpawnModOverworldWildPreserved(WildActorRead32(p + 24), WildActorRead32(p + 28),
        &mon, p[32], p[33], p[34], &slot);
    if (status != RHH_WILD_ACTOR_OK) { sWildDormantBusy = FALSE; return status; }
    struct WildActorEntry previousEntry = sWildActors[slot];
    u8 oldActor[WILD_ACTOR_RECORD_SIZE], oldTrait[WILD_TRAIT_RECORD_SIZE], oldProfile[WILD_PROFILE_RECORD_SIZE],
       oldActivity[WILD_ACTIVITY_RECORD_SIZE], oldQueue[WILD_QUEUE_RECORD_SIZE], oldConceal[WILD_CONCEAL_RECORD_SIZE];
    u8 *old[] = {oldActor, oldTrait, oldProfile, oldActivity, oldQueue, oldConceal};
    const u32 lengths[] = {WILD_ACTOR_RECORD_SIZE, WILD_TRAIT_RECORD_SIZE, WILD_PROFILE_RECORD_SIZE,
                           WILD_ACTIVITY_RECORD_SIZE, WILD_QUEUE_RECORD_SIZE, WILD_CONCEAL_RECORD_SIZE};
    const u32 offsets[] = {0, WILD_DORMANT_TRAIT, WILD_DORMANT_PROFILE, WILD_DORMANT_ACTIVITY,
                           WILD_DORMANT_QUEUE, WILD_DORMANT_CONCEAL};
    for (u32 i = 0; i < ARRAY_COUNT(states); ++i) memcpy(old[i], states[i] + slot * lengths[i], lengths[i]);
    struct RhhWildConcealment conceal = {.struct_size = sizeof(conceal)};
    const u8 *savedConceal = p + WILD_DORMANT_CONCEAL;
    if (savedConceal[0] && RhhModsRegisteredKey(WildActorRead64(savedConceal + 8)))
        conceal = (struct RhhWildConcealment){.struct_size = sizeof(conceal), .mode = savedConceal[0],
            .reveal_flags = savedConceal[1], .reveal_radius = savedConceal[2],
            .owner = WildActorRead64(savedConceal + 8), .revision = WildActorRead64(savedConceal + 24)};
    WildActorTrack(&gObjectEvents[slot], &mon, id->serial, RHH_WILD_ACTOR_RESUMED, &conceal, FALSE);
    for (u32 i = 1; i < 5; ++i) memcpy(states[i] + slot * lengths[i], p + offsets[i], lengths[i]);
    if (WildActorRead64(states[3] + slot * lengths[3])
        && !RhhModsRegisteredKey(WildActorRead64(states[3] + slot * lengths[3] + 16)))
        memset(states[3] + slot * lengths[3], 0, lengths[3]);
    struct RhhWildActorSnapshot result = {.struct_size = sizeof(result)};
    struct RhhWildActorIndividual restored = {.struct_size = sizeof(restored)};
    bool32 valid = !WildActorAt(slot, &result) && !WildActorIndividualAt(slot, &restored)
        && restored.run_id == id->run_id && restored.serial == id->serial
        && (result.flags & RHH_WILD_ACTOR_PRESERVED) && WildConcealStore(slot, &gObjectEvents[slot], &conceal)
        && !ValidateWildTraitState(states[1], WILD_TRAIT_STATE_SIZE)
        && !ValidateWildProfileState(states[2], WILD_PROFILE_STATE_SIZE)
        && !ValidateWildActivityState(states[3], WILD_ACTIVITY_STATE_SIZE)
        && !ValidateWildQueueState(states[4], WILD_QUEUE_STATE_SIZE);
    if (!valid)
    {
        memset(&sWildActors[slot], 0, sizeof(sWildActors[slot]));
        RemoveObjectEvent(&gObjectEvents[slot]);
        sWildActors[slot] = previousEntry;
        for (u32 i = 0; i < ARRAY_COUNT(states); ++i) memcpy(states[i] + slot * lengths[i], old[i], lengths[i]);
        sWildDormantBusy = FALSE; return RHH_WILD_ACTOR_FAILED;
    }
    WildDormantRemove(index);
    WildActorNotify(RHH_WILD_ACTOR_SPAWNED, RHH_WILD_ACTOR_RESUMED, &result, &restored);
    sWildDormantBusy = FALSE; *out = result; return RHH_WILD_ACTOR_OK;
}
static int WildDormantDiscard(const struct RhhWildActorIndividual *id, RhhContentKey owner)
{
    if (!owner || !RhhModsRegisteredKey(owner)) return RHH_WILD_ACTOR_INVALID;
    if (sWildDormantBusy || !WildActorCommandReady() || NativeSaveTransactionActive()) return RHH_WILD_ACTOR_UNAVAILABLE;
    int index = WildDormantIndex(id);
    if (index < 0 || WildActorRead64(sWildDormant[index] + 8) != owner) return RHH_WILD_ACTOR_INVALID;
    WildDormantRemove(index); return RHH_WILD_ACTOR_OK;
}
static int WildDormantRelocationContext(struct RhhWildDormantRelocation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildDormantRelocation
        || RhhModsCurrentEvent() != RHH_WILD_DORMANT_RELOCATED) return -1;
    *out = *sWildDormantRelocation; return 0;
}
static int WildDormantRelocate(const struct RhhWildDormantRelocationRequest *request, struct RhhWildDormantRelocation *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved[0] || request->reserved[1]
        || request->expected.struct_size < sizeof(request->expected) || request->expected.reserved[0]
        || request->expected.reserved[1] || !request->expected.owner || !RhhModsRegisteredKey(request->expected.owner)
        || !request->map || request->x < 0 || request->y < 0 || request->x > INT16_MAX - MAP_OFFSET
        || request->y > INT16_MAX - MAP_OFFSET || request->facing < DIR_SOUTH || request->facing > DIR_NORTHEAST
        || !out || out->struct_size < sizeof(*out)) return RHH_WILD_ACTOR_INVALID;
    if (sWildDormantBusy || !WildActorCommandReady() || NativeSaveTransactionActive()) return RHH_WILD_ACTOR_UNAVAILABLE;
    int index = WildDormantIndex(&request->expected.individual);
    struct RhhWildDormantRelocation result = {.struct_size = sizeof(result),
        .before = {.struct_size = sizeof(result.before)}, .destination = {.struct_size = sizeof(result.destination)}};
    if (index < 0 || WildDormantAt(index, &result.before) || result.before.owner != request->expected.owner)
        return RHH_WILD_ACTOR_INVALID;
    const struct RhhWildDormantActor *expected = &request->expected, *current = &result.before;
    if (current->map != expected->map || current->x != expected->x || current->y != expected->y
        || current->facing != expected->facing || current->elevation != expected->elevation)
        return RHH_WILD_ACTOR_ROUTE_CHANGED;
    const struct RhhAuthoredMapService *maps = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 0));
    if (!maps || maps->tile(request->map, request->x, request->y, &result.destination)) return RHH_WILD_ACTOR_INVALID;
    u32 elevation = result.destination.tile.elevation, mapId;
    if (result.destination.tile.collision || elevation == ELEVATION_TRANSITION || elevation == ELEVATION_MULTI_LEVEL)
        return RHH_WILD_ACTOR_BLOCKED;
    if (RhhContentResolve(RHH_MAP, result.destination.map, &mapId)) return RHH_WILD_ACTOR_INVALID;
    result.changed = current->map != result.destination.map || current->x != request->x || current->y != request->y
        || current->facing != request->facing || current->elevation != elevation;
    result.after = result.before;
    if (result.changed)
    {
        sWildDormantBusy = TRUE;
        u8 *p = sWildDormant[index]; WildActorWrite64(p + 16, result.destination.map);
        WildActorWrite32(p + 24, request->x); WildActorWrite32(p + 28, request->y);
        p[32] = request->facing; p[33] = elevation;
        if (p[WILD_DORMANT_CONCEAL])
        { p[WILD_DORMANT_CONCEAL + 4] = mapId >> 8; p[WILD_DORMANT_CONCEAL + 5] = mapId; }
        result.after.map = result.destination.map; result.after.x = request->x; result.after.y = request->y;
        result.after.facing = request->facing; result.after.elevation = elevation;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WILD_DORMANT_RELOCATED,
            .run_seed = RhhModsRunSeed(), .event_key = result.after.map, .subject = result.after.species,
            .ordinal = result.after.individual.serial};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
        sWildDormantRelocation = &result; RhhModsDispatch(&event, &decision); sWildDormantRelocation = NULL;
        gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
        sWildDormantBusy = FALSE;
    }
    *out = result; return RHH_WILD_ACTOR_OK;
}
static int WildDormantStepPreview(const struct RhhWildActorIndividual *id, u32 direction,
                                  struct RhhWildDormantStepPreview *out)
{
    _Static_assert((int)DIR_SOUTH == (int)RHH_AUTHORED_CONNECTION_SOUTH
        && (int)DIR_NORTH == (int)RHH_AUTHORED_CONNECTION_NORTH
        && (int)DIR_WEST == (int)RHH_AUTHORED_CONNECTION_WEST
        && (int)DIR_EAST == (int)RHH_AUTHORED_CONNECTION_EAST, "authored connection directions differ");
    if (!id || !out || out->struct_size < sizeof(*out) || direction < DIR_SOUTH || direction > DIR_EAST) return -1;
    int index = WildDormantIndex(id);
    struct RhhWildDormantStepPreview result = {.struct_size = sizeof(result), .direction = direction,
        .status = RHH_WILD_DORMANT_STEP_UNAVAILABLE, .actor = {.struct_size = sizeof(result.actor)},
        .source = {.struct_size = sizeof(result.source)}, .destination = {.struct_size = sizeof(result.destination)},
        .connection = {.struct_size = sizeof(result.connection)}};
    const struct RhhAuthoredMapService *maps = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 3));
    struct RhhAuthoredMap map = {.struct_size = sizeof(map)};
    if (index < 0 || WildDormantAt(index, &result.actor) || !maps) return -1;
    if (maps->map(result.actor.map, &map) || maps->tile(result.actor.map, result.actor.x, result.actor.y, &result.source))
    { *out = result; return 0; }
    result.flags |= RHH_WILD_DORMANT_STEP_SOURCE_AVAILABLE;
    if (result.source.tile.collision || result.source.tile.elevation != result.actor.elevation)
        result.flags |= RHH_WILD_DORMANT_STEP_SOURCE_DIFFERS;
    s32 x = result.actor.x, y = result.actor.y;
    if (direction == DIR_SOUTH) ++y; else if (direction == DIR_NORTH) --y;
    else if (direction == DIR_WEST) --x; else ++x;
    RhhContentKey destinationMap = result.actor.map;
    if (x < 0 || y < 0 || x >= (s32)map.width || y >= (s32)map.height)
    {
        u32 count = 0, matches = 0; bool32 uncertain = FALSE;
        if (maps->connection_count(map.map, &count)) { *out = result; return 0; }
        for (u32 i = 0; i < count; ++i)
        {
            struct RhhAuthoredConnection c = {.struct_size = sizeof(c)};
            if (maps->connection_at(map.map, i, &c)) { uncertain = TRUE; continue; }
            if (c.direction != direction) continue;
            if (!(c.flags & RHH_AUTHORED_CONNECTION_GEOMETRY_AVAILABLE)) { uncertain = TRUE; continue; }
            s32 along = direction == DIR_NORTH || direction == DIR_SOUTH ? result.actor.x : result.actor.y;
            if (along < c.seam_start || along >= c.seam_end) continue;
            result.connection = c; ++matches;
        }
        if (uncertain)
        { result.connection = (struct RhhAuthoredConnection){.struct_size = sizeof(result.connection)}; *out = result; return 0; }
        if (!matches) { result.status = RHH_WILD_DORMANT_STEP_EDGE; *out = result; return 0; }
        if (matches > 1)
        {
            result.connection = (struct RhhAuthoredConnection){.struct_size = sizeof(result.connection)};
            result.status = RHH_WILD_DORMANT_STEP_AMBIGUOUS; *out = result; return 0;
        }
        destinationMap = result.connection.target; x -= result.connection.origin_x; y -= result.connection.origin_y;
        result.flags |= RHH_WILD_DORMANT_STEP_CROSSES_MAP;
    }
    if (maps->tile(destinationMap, x, y, &result.destination)) { *out = result; return 0; }
    result.flags |= RHH_WILD_DORMANT_STEP_DESTINATION_AVAILABLE;
    u32 elevation = result.destination.tile.elevation;
    if (result.destination.tile.collision) result.status = RHH_WILD_DORMANT_STEP_COLLISION;
    else if (elevation == ELEVATION_TRANSITION || elevation == ELEVATION_MULTI_LEVEL || elevation != result.actor.elevation)
        result.status = RHH_WILD_DORMANT_STEP_ELEVATION;
    else result.status = RHH_WILD_DORMANT_STEP_OPEN;
    *out = result; return 0;
}
static int WildDormantProfileAt(const struct RhhWildActorIndividual *id, RhhContentKey owner, RhhContentKey field,
                                struct RhhWildProfileValue *out)
{
    if (!out || out->struct_size < sizeof(*out) || !owner || !field) return -1;
    int index = WildDormantIndex(id);
    if (index < 0) return -1;
    WildProfileRead(sWildDormant[index] + WILD_DORMANT_PROFILE, id->serial, owner, field, out);
    return 0;
}
static int WildDormantProfileList(const struct RhhWildActorIndividual *id, RhhContentKey owner,
                                  struct RhhWildDormantProfileList *out)
{
    if (!out || out->struct_size < sizeof(*out) || !owner) return -1;
    int index = WildDormantIndex(id);
    if (index < 0) return -1;
    const u8 *p = sWildDormant[index] + WILD_DORMANT_PROFILE;
    struct RhhWildDormantProfileList result = {.struct_size = sizeof(result), .owner = owner,
        .owner_available = RhhModsRegisteredKey(owner), .individual = *id};
    result.individual.struct_size = sizeof(result.individual);
    if (WildActorRead64(p) == id->serial)
    {
        result.revision = WildActorRead64(p + 8);
        for (u32 i = 0; i < RHH_WILD_PROFILE_CAPACITY; ++i)
        {
            const u8 *item = p + WILD_PROFILE_HEADER + i * WILD_PROFILE_ITEM_SIZE;
            if (WildActorRead64(item) != owner) continue;
            RhhContentKey field = WildActorRead64(item + 8);
            WildProfileRead(p, id->serial, owner, field, &result.fields[result.count++]);
        }
    }
    *out = result; return 0;
}
static int WildDormantProfileContext(struct RhhWildDormantProfileResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildDormantProfileContext
        || (event != RHH_CAN_EDIT_DORMANT_WILD_PROFILE && event != RHH_DORMANT_WILD_PROFILE_EDITED)) return -1;
    *out = *sWildDormantProfileContext; return 0;
}
static void WildDormantProfileDispatch(u32 kind, struct RhhWildDormantProfileResult *result, struct RhhModDecision *decision)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = result->after.field, .subject = result->actor.species, .ordinal = result->actor.individual.serial,
        .args = {result->before.present, result->after.present, result->is_preview, 0}};
    sWildDormantProfileContext = result; RhhModsDispatch(&event, decision); sWildDormantProfileContext = NULL;
}
static int WildDormantProfileEdit(const struct RhhWildDormantProfileRequest *request,
                                  struct RhhWildDormantProfileResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WILD_PROFILE_INVALID;
    *out = (struct RhhWildDormantProfileResult){.struct_size = sizeof(*out), .is_preview = preview,
        .actor.struct_size = sizeof(out->actor), .before.struct_size = sizeof(out->before),
        .after.struct_size = sizeof(out->after)};
    if (!request || request->struct_size < sizeof(*request) || !request->owner || !request->field
        || !RhhModsRegisteredKey(request->owner) || request->present > 1 || (!request->present && request->value))
        return out->status = RHH_WILD_PROFILE_INVALID;
    if (sWildDormantBusy || sWildProfileBusy || !WildActorCommandReady() || NativeSaveTransactionActive())
        return out->status = RHH_WILD_PROFILE_UNAVAILABLE;
    int index = WildDormantIndex(&request->individual);
    if (index < 0 || WildDormantAt(index, &out->actor)) return out->status = RHH_WILD_PROFILE_INVALID;
    u8 *p = sWildDormant[index] + WILD_DORMANT_PROFILE;
    WildProfileRead(p, request->individual.serial, request->owner, request->field, &out->before);
    out->after = out->before;
    if (request->expected_revision != out->before.revision) return out->status = RHH_WILD_PROFILE_STALE;
    if (out->before.present == request->present && (!request->present || out->before.value == request->value))
        return RHH_WILD_PROFILE_OK;
    if (!RhhModsDormantProfileHooksCompatible())
    { snprintf(out->reason, sizeof(out->reason), "live profile hooks lack dormant counterparts"); return out->status = RHH_WILD_PROFILE_UNAVAILABLE; }
    if (out->before.revision == UINT64_MAX) return out->status = RHH_WILD_PROFILE_UNAVAILABLE;
    if (request->present)
    {
        rng_value_t schemaRng = gRngValue, schemaRng2 = gRng2Value; enum Ability schemaLast = gLastUsedAbility;
        int schema = WildProfileValueInSchema(request->owner, request->field, request->value);
        bool32 impure = memcmp(&schemaRng, &gRngValue, sizeof(schemaRng))
            || memcmp(&schemaRng2, &gRng2Value, sizeof(schemaRng2)) || schemaLast != gLastUsedAbility;
        gRngValue = schemaRng; gRng2Value = schemaRng2; gLastUsedAbility = schemaLast;
        if (impure) return out->status = RHH_WILD_PROFILE_UNAVAILABLE;
        if (schema != RHH_WILD_PROFILE_OK) return out->status = schema;
    }
    u8 original[WILD_PROFILE_RECORD_SIZE], staged[WILD_PROFILE_RECORD_SIZE];
    memcpy(original, p, sizeof(original));
    if (WildActorRead64(p) == request->individual.serial) memcpy(staged, p, sizeof(staged));
    else memset(staged, 0, sizeof(staged));
    u32 count = 0, found = RHH_WILD_PROFILE_CAPACITY;
    while (count < RHH_WILD_PROFILE_CAPACITY && WildActorRead64(staged + WILD_PROFILE_HEADER + count * WILD_PROFILE_ITEM_SIZE))
    {
        const u8 *item = staged + WILD_PROFILE_HEADER + count * WILD_PROFILE_ITEM_SIZE;
        if (WildActorRead64(item) == request->owner && WildActorRead64(item + 8) == request->field) found = count;
        ++count;
    }
    if (found == RHH_WILD_PROFILE_CAPACITY && request->present && count == RHH_WILD_PROFILE_CAPACITY)
        return out->status = RHH_WILD_PROFILE_FULL;
    if (found < count)
    {
        u8 *item = staged + WILD_PROFILE_HEADER + found * WILD_PROFILE_ITEM_SIZE;
        memmove(item, item + WILD_PROFILE_ITEM_SIZE, (count - found - 1) * WILD_PROFILE_ITEM_SIZE);
        memset(staged + WILD_PROFILE_HEADER + (--count) * WILD_PROFILE_ITEM_SIZE, 0, WILD_PROFILE_ITEM_SIZE);
    }
    if (request->present)
    {
        u32 insert = 0;
        while (insert < count)
        {
            const u8 *item = staged + WILD_PROFILE_HEADER + insert * WILD_PROFILE_ITEM_SIZE;
            u64 owner = WildActorRead64(item), field = WildActorRead64(item + 8);
            if (owner > request->owner || (owner == request->owner && field > request->field)) break;
            ++insert;
        }
        u8 *item = staged + WILD_PROFILE_HEADER + insert * WILD_PROFILE_ITEM_SIZE;
        memmove(item + WILD_PROFILE_ITEM_SIZE, item, (count - insert) * WILD_PROFILE_ITEM_SIZE);
        WildActorWrite64(item, request->owner); WildActorWrite64(item + 8, request->field);
        WildActorWrite64(item + 16, (u64)request->value);
    }
    out->after.present = request->present; out->after.value = request->value; ++out->after.revision; out->changed = TRUE;
    WildActorWrite64(staged, request->individual.serial); WildActorWrite64(staged + 8, out->after.revision);
    u8 scratch[WILD_PROFILE_STATE_SIZE] = {0}; memcpy(scratch, staged, sizeof(staged));
    if (ValidateWildProfileState(scratch, sizeof(scratch))) return out->status = RHH_WILD_PROFILE_UNAVAILABLE;
    sWildDormantBusy = sWildProfileBusy = TRUE;
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    WildDormantProfileDispatch(RHH_CAN_EDIT_DORMANT_WILD_PROFILE, out, &decision);
    bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))
        || last != gLastUsedAbility;
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
    if (impure || !decision.allowed)
    {
        snprintf(out->reason, sizeof(out->reason), "%s", impure ? "profile constraint changed gameplay state" : decision.reason);
        out->status = RHH_WILD_PROFILE_DENIED;
    }
    else if (!WildActorCommandReady() || NativeSaveTransactionActive() || WildDormantIndex(&request->individual) != index
        || memcmp(p, original, sizeof(original))) out->status = RHH_WILD_PROFILE_STALE;
    else if (!preview)
    {
        memcpy(p, staged, sizeof(staged));
        decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
        WildDormantProfileDispatch(RHH_DORMANT_WILD_PROFILE_EDITED, out, &decision);
        gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
    }
    sWildProfileBusy = sWildDormantBusy = FALSE;
    return out->status;
}
static int WildDormantProfilePreview(const struct RhhWildDormantProfileRequest *r, struct RhhWildDormantProfileResult *out)
{ return WildDormantProfileEdit(r, out, TRUE); }
static int WildDormantProfileApply(const struct RhhWildDormantProfileRequest *r, struct RhhWildDormantProfileResult *out)
{ return WildDormantProfileEdit(r, out, FALSE); }
static int WildDormantProfileBatchContext(struct RhhWildDormantProfileBatchResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildDormantProfileBatchContext
        || (event != RHH_CAN_EDIT_DORMANT_WILD_PROFILE && event != RHH_DORMANT_WILD_PROFILE_EDITED)) return -1;
    *out = *sWildDormantProfileBatchContext; return 0;
}
static int WildDormantProfileBatch(const struct RhhWildDormantProfileBatchRequest *request,
                                   struct RhhWildDormantProfileBatchResult *out, bool32 preview, bool32 deferCommit)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WILD_PROFILE_INVALID;
    *out = (struct RhhWildDormantProfileBatchResult){.struct_size = sizeof(*out), .is_preview = preview,
        .actor.struct_size = sizeof(out->actor)};
    if (!request || request->struct_size < sizeof(*request) || !request->owner || !RhhModsRegisteredKey(request->owner)
        || !request->count || request->count > RHH_WILD_PROFILE_CAPACITY) return out->status = RHH_WILD_PROFILE_INVALID;
    u32 order[RHH_WILD_PROFILE_CAPACITY];
    for (u32 i = 0; i < request->count; ++i)
    {
        const struct RhhWildProfileBatchItem *item = &request->items[i];
        if (!item->field || item->reserved || item->present > 1 || (!item->present && item->value))
            return out->status = RHH_WILD_PROFILE_INVALID;
        u32 j = i;
        while (j && request->items[order[j - 1]].field > item->field) { order[j] = order[j - 1]; --j; }
        if (j && request->items[order[j - 1]].field == item->field) return out->status = RHH_WILD_PROFILE_INVALID;
        order[j] = i;
    }
    if (sWildDormantBusy || sWildProfileBusy || !WildActorCommandReady() || NativeSaveTransactionActive())
        return out->status = RHH_WILD_PROFILE_UNAVAILABLE;
    int index = WildDormantIndex(&request->individual);
    if (index < 0 || WildDormantAt(index, &out->actor)) return out->status = RHH_WILD_PROFILE_INVALID;
    u8 *p = sWildDormant[index] + WILD_DORMANT_PROFILE;
    u8 original[WILD_PROFILE_RECORD_SIZE], staged[WILD_PROFILE_RECORD_SIZE];
    memcpy(original, p, sizeof(original));
    if (WildActorRead64(p) == request->individual.serial) memcpy(staged, p, sizeof(staged));
    else memset(staged, 0, sizeof(staged));
    u32 count = 0;
    while (count < RHH_WILD_PROFILE_CAPACITY && WildActorRead64(staged + WILD_PROFILE_HEADER + count * WILD_PROFILE_ITEM_SIZE)) ++count;
    s32 finalCount = count; bool8 changed[RHH_WILD_PROFILE_CAPACITY] = {0}; out->count = request->count;
    for (u32 i = 0; i < request->count; ++i)
    {
        const struct RhhWildProfileBatchItem *item = &request->items[order[i]];
        struct RhhWildProfileBatchChange *change = &out->changes[i];
        WildProfileRead(p, request->individual.serial, request->owner, item->field, &change->before);
        if (change->before.revision != request->expected_revision) return out->status = RHH_WILD_PROFILE_STALE;
        change->after = change->before; change->after.present = item->present; change->after.value = item->value;
        changed[i] = change->before.present != item->present || (item->present && change->before.value != item->value);
        if (!changed[i]) continue;
        if (item->present)
        {
            rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
            int status = WildProfileValueInSchema(request->owner, item->field, item->value);
            bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))
                || last != gLastUsedAbility;
            gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
            if (impure) return out->status = RHH_WILD_PROFILE_UNAVAILABLE;
            if (status != RHH_WILD_PROFILE_OK) return out->status = status;
        }
        ++out->changed_count; finalCount += (s32)item->present - (s32)change->before.present;
    }
    if (!out->changed_count) return RHH_WILD_PROFILE_OK;
    if (!RhhModsDormantProfileHooksCompatible())
    { snprintf(out->reason, sizeof(out->reason), "live profile hooks lack dormant counterparts"); return out->status = RHH_WILD_PROFILE_UNAVAILABLE; }
    if (request->expected_revision == UINT64_MAX) return out->status = RHH_WILD_PROFILE_UNAVAILABLE;
    if (finalCount < 0 || finalCount > (s32)RHH_WILD_PROFILE_CAPACITY) return out->status = RHH_WILD_PROFILE_FULL;
    for (u32 i = 0; i < request->count; ++i)
    {
        struct RhhWildProfileBatchChange *change = &out->changes[i];
        change->after.revision = request->expected_revision + 1;
        if (!changed[i] || !change->before.present) continue;
        for (u32 j = 0; j < count; ++j)
        {
            u8 *item = staged + WILD_PROFILE_HEADER + j * WILD_PROFILE_ITEM_SIZE;
            if (WildActorRead64(item) != request->owner || WildActorRead64(item + 8) != change->before.field) continue;
            memmove(item, item + WILD_PROFILE_ITEM_SIZE, (count - j - 1) * WILD_PROFILE_ITEM_SIZE);
            memset(staged + WILD_PROFILE_HEADER + (--count) * WILD_PROFILE_ITEM_SIZE, 0, WILD_PROFILE_ITEM_SIZE);
            break;
        }
    }
    for (u32 i = 0; i < request->count; ++i)
    {
        const struct RhhWildProfileBatchChange *change = &out->changes[i];
        if (!changed[i] || !change->after.present) continue;
        u32 j = 0;
        while (j < count)
        {
            const u8 *item = staged + WILD_PROFILE_HEADER + j * WILD_PROFILE_ITEM_SIZE;
            u64 owner = WildActorRead64(item), field = WildActorRead64(item + 8);
            if (owner > request->owner || (owner == request->owner && field > change->after.field)) break;
            ++j;
        }
        u8 *item = staged + WILD_PROFILE_HEADER + j * WILD_PROFILE_ITEM_SIZE;
        memmove(item + WILD_PROFILE_ITEM_SIZE, item, (count - j) * WILD_PROFILE_ITEM_SIZE);
        WildActorWrite64(item, request->owner); WildActorWrite64(item + 8, change->after.field);
        WildActorWrite64(item + 16, (u64)change->after.value); ++count;
    }
    WildActorWrite64(staged, request->individual.serial); WildActorWrite64(staged + 8, request->expected_revision + 1);
    u8 scratch[WILD_PROFILE_STATE_SIZE] = {0}; memcpy(scratch, staged, sizeof(staged));
    if (ValidateWildProfileState(scratch, sizeof(scratch))) return out->status = RHH_WILD_PROFILE_UNAVAILABLE;
    sWildDormantBusy = sWildProfileBusy = TRUE; sWildDormantProfileBatchContext = out;
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    for (u32 i = 0; i < request->count; ++i)
    {
        if (!changed[i]) continue;
        struct RhhWildDormantProfileResult item = {.struct_size = sizeof(item), .is_preview = preview, .changed = TRUE,
            .actor = out->actor, .before = out->changes[i].before, .after = out->changes[i].after};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        WildDormantProfileDispatch(RHH_CAN_EDIT_DORMANT_WILD_PROFILE, &item, &decision);
        bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))
            || last != gLastUsedAbility;
        gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
        if (impure || !decision.allowed)
        {
            snprintf(out->reason, sizeof(out->reason), "%s", impure ? "profile constraint changed gameplay state" : decision.reason);
            out->status = RHH_WILD_PROFILE_DENIED; goto done;
        }
    }
    if (!WildActorCommandReady() || NativeSaveTransactionActive() || WildDormantIndex(&request->individual) != index
        || memcmp(p, original, sizeof(original))) { out->status = RHH_WILD_PROFILE_STALE; goto done; }
    if (!preview && !deferCommit)
    {
        memcpy(p, staged, sizeof(staged));
        for (u32 i = 0; i < request->count; ++i)
        {
            if (!changed[i]) continue;
            struct RhhWildDormantProfileResult item = {.struct_size = sizeof(item), .changed = TRUE,
                .actor = out->actor, .before = out->changes[i].before, .after = out->changes[i].after};
            struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
            WildDormantProfileDispatch(RHH_DORMANT_WILD_PROFILE_EDITED, &item, &decision);
            gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
        }
    }
done:
    sWildDormantProfileBatchContext = NULL; sWildProfileBusy = sWildDormantBusy = FALSE;
    return out->status;
}
static int WildDormantProfileBatchPreview(const struct RhhWildDormantProfileBatchRequest *r, struct RhhWildDormantProfileBatchResult *out)
{ return WildDormantProfileBatch(r, out, TRUE, FALSE); }
static int WildDormantProfileBatchApply(const struct RhhWildDormantProfileBatchRequest *r, struct RhhWildDormantProfileBatchResult *out)
{ return WildDormantProfileBatch(r, out, FALSE, FALSE); }
static int WildDormantClock(const struct RhhWildActorIndividual *id, struct RhhWildDormantClock *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    int index = WildDormantIndex(id);
    const struct RhhWorldClockService *clock = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldClockSnapshot now = {.struct_size = sizeof(now)};
    struct RhhWildDormantClock result = {.struct_size = sizeof(result), .actor.struct_size = sizeof(result.actor)};
    if (index < 0 || !clock || clock->current(&now) || now.run_id != id->run_id
        || WildDormantAt(index, &result.actor)) return -1;
    result.current_step = now.steps; result.processed_step = WildActorRead64(sWildDormant[index] + WILD_DORMANT_CLOCK);
    if (result.processed_step > result.current_step) return -1;
    result.pending_steps = result.current_step - result.processed_step;
    *out = result; return 0;
}
static int WildDormantClockContext(struct RhhWildDormantClockChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildDormantClockContext
        || RhhModsCurrentEvent() != RHH_WILD_DORMANT_CLOCK_ADVANCED) return -1;
    *out = *sWildDormantClockContext; return 0;
}
static int WildDormantClockAdvance(const struct RhhWildDormantClockRequest *request, struct RhhWildDormantClockChange *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->owner
        || !RhhModsRegisteredKey(request->owner) || !out || out->struct_size < sizeof(*out))
        return RHH_WILD_DORMANT_CLOCK_INVALID;
    if (sWildDormantBusy || !WildActorCommandReady() || NativeSaveTransactionActive()) return RHH_WILD_DORMANT_CLOCK_UNAVAILABLE;
    int index = WildDormantIndex(&request->individual);
    struct RhhWildDormantClockChange result = {.struct_size = sizeof(result), .before.struct_size = sizeof(result.before)};
    if (index < 0 || WildDormantClock(&request->individual, &result.before) || result.before.actor.owner != request->owner)
        return RHH_WILD_DORMANT_CLOCK_INVALID;
    if (request->expected_step != result.before.processed_step) return RHH_WILD_DORMANT_CLOCK_STALE;
    if (request->through_step < request->expected_step || request->through_step > result.before.current_step)
        return RHH_WILD_DORMANT_CLOCK_INVALID;
    if (request->through_step - request->expected_step > RHH_WILD_DORMANT_MAX_STEP_ADVANCE)
        return RHH_WILD_DORMANT_CLOCK_LIMIT;
    result.after = result.before; result.changed = request->through_step != request->expected_step;
    if (result.changed)
    {
        sWildDormantBusy = TRUE;
        WildActorWrite64(sWildDormant[index] + WILD_DORMANT_CLOCK, request->through_step);
        result.after.processed_step = request->through_step;
        result.after.pending_steps = result.after.current_step - request->through_step;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WILD_DORMANT_CLOCK_ADVANCED,
            .run_seed = RhhModsRunSeed(), .event_key = result.after.actor.map, .subject = result.after.actor.species,
            .ordinal = result.after.actor.individual.serial};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
        sWildDormantClockContext = &result; RhhModsDispatch(&event, &decision); sWildDormantClockContext = NULL;
        gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last; sWildDormantBusy = FALSE;
    }
    *out = result; return RHH_WILD_DORMANT_CLOCK_OK;
}
static int WildDormantWorkContext(struct RhhWildDormantWorkResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildDormantWorkContext
        || (event != RHH_CAN_EDIT_DORMANT_WILD_PROFILE && event != RHH_DORMANT_WILD_PROFILE_EDITED
            && event != RHH_WILD_DORMANT_RELOCATED && event != RHH_WILD_DORMANT_CLOCK_ADVANCED)) return -1;
    *out = *sWildDormantWorkContext; return 0;
}
/* Apply a previously validated batch to a private copy of the dormant record. */
static bool32 WildDormantStageProfile(u8 *record, const struct RhhWildDormantProfileBatchResult *batch)
{
    if (!batch->changed_count) return TRUE;
    u8 *p = record + WILD_DORMANT_PROFILE;
    u32 count = 0;
    while (count < RHH_WILD_PROFILE_CAPACITY && WildActorRead64(p + WILD_PROFILE_HEADER + count * WILD_PROFILE_ITEM_SIZE)) ++count;
    for (u32 i = 0; i < batch->count; ++i)
    {
        const struct RhhWildProfileBatchChange *c = &batch->changes[i];
        bool32 changed = c->before.present != c->after.present || (c->after.present && c->before.value != c->after.value);
        if (!changed || !c->before.present) continue;
        u32 j = 0;
        while (j < count && (WildActorRead64(p + WILD_PROFILE_HEADER + j * WILD_PROFILE_ITEM_SIZE) != c->before.owner
            || WildActorRead64(p + WILD_PROFILE_HEADER + j * WILD_PROFILE_ITEM_SIZE + 8) != c->before.field)) ++j;
        if (j == count) return FALSE;
        u8 *item = p + WILD_PROFILE_HEADER + j * WILD_PROFILE_ITEM_SIZE;
        memmove(item, item + WILD_PROFILE_ITEM_SIZE, (count - j - 1) * WILD_PROFILE_ITEM_SIZE);
        memset(p + WILD_PROFILE_HEADER + (--count) * WILD_PROFILE_ITEM_SIZE, 0, WILD_PROFILE_ITEM_SIZE);
    }
    for (u32 i = 0; i < batch->count; ++i)
    {
        const struct RhhWildProfileBatchChange *c = &batch->changes[i];
        bool32 changed = c->before.present != c->after.present || (c->after.present && c->before.value != c->after.value);
        if (!changed || !c->after.present) continue;
        if (count >= RHH_WILD_PROFILE_CAPACITY) return FALSE;
        u32 j = 0;
        while (j < count)
        {
            const u8 *item = p + WILD_PROFILE_HEADER + j * WILD_PROFILE_ITEM_SIZE;
            u64 owner = WildActorRead64(item), field = WildActorRead64(item + 8);
            if (owner > c->after.owner || (owner == c->after.owner && field > c->after.field)) break;
            ++j;
        }
        u8 *item = p + WILD_PROFILE_HEADER + j * WILD_PROFILE_ITEM_SIZE;
        memmove(item + WILD_PROFILE_ITEM_SIZE, item, (count - j) * WILD_PROFILE_ITEM_SIZE);
        WildActorWrite64(item, c->after.owner); WildActorWrite64(item + 8, c->after.field);
        WildActorWrite64(item + 16, (u64)c->after.value); ++count;
    }
    WildActorWrite64(p, batch->actor.individual.serial); WildActorWrite64(p + 8, batch->changes[0].after.revision);
    u8 scratch[WILD_PROFILE_STATE_SIZE] = {0}; memcpy(scratch, p, WILD_PROFILE_RECORD_SIZE);
    return !ValidateWildProfileState(scratch, sizeof(scratch));
}
static int WildDormantWork(const struct RhhWildDormantWorkRequest *request, struct RhhWildDormantWorkResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WILD_DORMANT_WORK_INVALID;
    *out = (struct RhhWildDormantWorkResult){.struct_size = sizeof(*out), .is_preview = preview,
        .profile.struct_size = sizeof(out->profile), .relocation.struct_size = sizeof(out->relocation),
        .clock.struct_size = sizeof(out->clock)};
    if (!request || request->struct_size < sizeof(*request)
        || (request->flags & ~(RHH_WILD_DORMANT_WORK_RELOCATE | RHH_WILD_DORMANT_WORK_GUARD_PROFILE))
        || request->count > RHH_WILD_PROFILE_CAPACITY || request->expected.struct_size < sizeof(request->expected)
        || request->expected.reserved[0] || request->expected.reserved[1] || !request->expected.owner
        || !RhhModsRegisteredKey(request->expected.owner)
        || (!(request->flags & RHH_WILD_DORMANT_WORK_RELOCATE) && (request->map || request->x || request->y || request->facing))
        || ((request->flags & RHH_WILD_DORMANT_WORK_RELOCATE) && (!request->map || request->x < 0 || request->y < 0
            || request->x > INT16_MAX - MAP_OFFSET || request->y > INT16_MAX - MAP_OFFSET
            || request->facing < DIR_SOUTH || request->facing > DIR_NORTHEAST)))
        return out->status = RHH_WILD_DORMANT_WORK_INVALID;
    if (sWildDormantBusy || sWildProfileBusy || !WildActorCommandReady() || NativeSaveTransactionActive())
        return out->status = RHH_WILD_DORMANT_WORK_UNAVAILABLE;
    int index = WildDormantIndex(&request->expected.individual);
    out->clock.before.struct_size = sizeof(out->clock.before);
    if (index < 0 || WildDormantClock(&request->expected.individual, &out->clock.before)
        || out->clock.before.actor.owner != request->expected.owner) return out->status = RHH_WILD_DORMANT_WORK_INVALID;
    const struct RhhWildDormantActor *actor = &out->clock.before.actor, *expected = &request->expected;
    if (actor->map != expected->map || actor->x != expected->x || actor->y != expected->y
        || actor->facing != expected->facing || actor->elevation != expected->elevation
        || out->clock.before.processed_step != request->expected_step) return out->status = RHH_WILD_DORMANT_WORK_STALE;
    if (request->flags & RHH_WILD_DORMANT_WORK_GUARD_PROFILE)
    {
        const u8 *profile = sWildDormant[index] + WILD_DORMANT_PROFILE;
        u64 revision = WildActorRead64(profile) == expected->individual.serial ? WildActorRead64(profile + 8) : 0;
        if (revision != request->expected_profile_revision) return out->status = RHH_WILD_DORMANT_WORK_STALE;
    }
    if (request->through_step < request->expected_step || request->through_step > out->clock.before.current_step)
        return out->status = RHH_WILD_DORMANT_WORK_INVALID;
    if (request->through_step - request->expected_step > RHH_WILD_DORMANT_MAX_STEP_ADVANCE)
        return out->status = RHH_WILD_DORMANT_WORK_LIMIT;
    out->clock.after = out->clock.before; out->clock.after.processed_step = request->through_step;
    out->clock.after.pending_steps = out->clock.after.current_step - request->through_step;
    out->clock.changed = request->through_step != request->expected_step;
    out->relocation.before = out->relocation.after = *actor;
    u32 mapId = 0;
    if (request->flags & RHH_WILD_DORMANT_WORK_RELOCATE)
    {
        const struct RhhAuthoredMapService *maps = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 0));
        out->relocation.destination.struct_size = sizeof(out->relocation.destination);
        if (!maps || maps->tile(request->map, request->x, request->y, &out->relocation.destination)
            || RhhContentResolve(RHH_MAP, out->relocation.destination.map, &mapId)) return out->status = RHH_WILD_DORMANT_WORK_INVALID;
        u32 elevation = out->relocation.destination.tile.elevation;
        if (out->relocation.destination.tile.collision || elevation == ELEVATION_TRANSITION || elevation == ELEVATION_MULTI_LEVEL)
            return out->status = RHH_WILD_DORMANT_WORK_BLOCKED;
        out->relocation.after.map = out->relocation.destination.map; out->relocation.after.x = request->x;
        out->relocation.after.y = request->y; out->relocation.after.facing = request->facing; out->relocation.after.elevation = elevation;
        out->relocation.changed = actor->map != out->relocation.after.map || actor->x != request->x || actor->y != request->y
            || actor->facing != request->facing || actor->elevation != elevation;
    }
    out->clock.after.actor = out->relocation.after;
    out->profile.actor = *actor; out->profile.is_preview = preview;
    out->changed = out->clock.changed || out->relocation.changed || request->count;
    u8 original[WILD_DORMANT_RECORD], staged[WILD_DORMANT_RECORD];
    memcpy(original, sWildDormant[index], sizeof(original)); memcpy(staged, original, sizeof(staged));
    if (request->count)
    {
        struct RhhWildDormantProfileBatchRequest batch = {.struct_size = sizeof(batch), .count = request->count,
            .individual = request->expected.individual, .expected_revision = request->expected_profile_revision,
            .owner = request->expected.owner};
        memcpy(batch.items, request->items, request->count * sizeof(batch.items[0]));
        sWildDormantWorkContext = out;
        int status = WildDormantProfileBatch(&batch, &out->profile, preview, TRUE);
        sWildDormantWorkContext = NULL;
        if (status != RHH_WILD_PROFILE_OK)
        {
            snprintf(out->reason, sizeof(out->reason), "%s", out->profile.reason);
            return out->status = status == RHH_WILD_PROFILE_INVALID ? RHH_WILD_DORMANT_WORK_INVALID
                : status == RHH_WILD_PROFILE_STALE ? RHH_WILD_DORMANT_WORK_STALE
                : status == RHH_WILD_PROFILE_DENIED ? RHH_WILD_DORMANT_WORK_DENIED
                : status == RHH_WILD_PROFILE_FULL ? RHH_WILD_DORMANT_WORK_FULL : RHH_WILD_DORMANT_WORK_UNAVAILABLE;
        }
        if (!WildDormantStageProfile(staged, &out->profile)) return out->status = RHH_WILD_DORMANT_WORK_UNAVAILABLE;
    }
    if (out->relocation.changed)
    {
        WildActorWrite64(staged + 16, out->relocation.after.map); WildActorWrite32(staged + 24, request->x);
        WildActorWrite32(staged + 28, request->y); staged[32] = request->facing; staged[33] = out->relocation.after.elevation;
        if (staged[WILD_DORMANT_CONCEAL]) { staged[WILD_DORMANT_CONCEAL + 4] = mapId >> 8; staged[WILD_DORMANT_CONCEAL + 5] = mapId; }
    }
    WildActorWrite64(staged + WILD_DORMANT_CLOCK, request->through_step);
    out->changed = out->clock.changed || out->relocation.changed || out->profile.changed_count;
    struct RhhWildDormantClock current = {.struct_size = sizeof(current)};
    if (!WildActorCommandReady() || NativeSaveTransactionActive() || WildDormantIndex(&request->expected.individual) != index
        || memcmp(sWildDormant[index], original, sizeof(original)) || WildDormantClock(&request->expected.individual, &current)
        || current.current_step != out->clock.before.current_step) return out->status = RHH_WILD_DORMANT_WORK_STALE;
    if (!preview && out->changed)
    {
        sWildDormantBusy = sWildProfileBusy = TRUE; sWildDormantWorkContext = out;
        memcpy(sWildDormant[index], staged, sizeof(staged));
        rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
        struct RhhModEvent event = {.struct_size = sizeof(event), .run_seed = RhhModsRunSeed(),
            .event_key = out->relocation.after.map, .subject = actor->species, .ordinal = actor->individual.serial};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        if (out->relocation.changed)
        {
            event.kind = RHH_WILD_DORMANT_RELOCATED; sWildDormantRelocation = &out->relocation;
            RhhModsDispatch(&event, &decision); sWildDormantRelocation = NULL;
            gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
        }
        sWildDormantProfileBatchContext = &out->profile;
        for (u32 i = 0; i < out->profile.count; ++i)
        {
            const struct RhhWildProfileBatchChange *c = &out->profile.changes[i];
            if (c->before.present == c->after.present && (!c->after.present || c->before.value == c->after.value)) continue;
            struct RhhWildDormantProfileResult item = {.struct_size = sizeof(item), .changed = TRUE,
                .actor = out->relocation.after, .before = c->before, .after = c->after};
            decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
            WildDormantProfileDispatch(RHH_DORMANT_WILD_PROFILE_EDITED, &item, &decision);
            gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
        }
        sWildDormantProfileBatchContext = NULL;
        if (out->clock.changed)
        {
            event.kind = RHH_WILD_DORMANT_CLOCK_ADVANCED; decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
            sWildDormantClockContext = &out->clock; RhhModsDispatch(&event, &decision); sWildDormantClockContext = NULL;
            gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
        }
        sWildDormantWorkContext = NULL; sWildProfileBusy = sWildDormantBusy = FALSE;
    }
    return RHH_WILD_DORMANT_WORK_OK;
}
static int WildDormantWorkPreview(const struct RhhWildDormantWorkRequest *r, struct RhhWildDormantWorkResult *out)
{ return WildDormantWork(r, out, TRUE); }
static int WildDormantWorkApply(const struct RhhWildDormantWorkRequest *r, struct RhhWildDormantWorkResult *out)
{ return WildDormantWork(r, out, FALSE); }
static const struct RhhWildDormantService sWildDormantService = {
    {sizeof(sWildDormantService), RHH_SERVICE_VERSION(1, 8)}, WildDormantCount, WildDormantAt,
    WildDormantFind, WildDormantSuspend, WildDormantResume, WildDormantDiscard,
    WildDormantRelocate, WildDormantRelocationContext, WildDormantStepPreview,
    WildDormantProfileAt, WildDormantProfileList, WildDormantProfilePreview,
    WildDormantProfileApply, WildDormantProfileContext, WildDormantProfileBatchPreview,
    WildDormantProfileBatchApply, WildDormantProfileBatchContext, WildDormantClock,
    WildDormantClockAdvance, WildDormantClockContext, WildDormantWorkPreview, WildDormantWorkApply, WildDormantWorkContext
};
bool32 ModsWildActorCopyMon(struct ObjectEvent *object, struct Pokemon *out)
{
    int slot = WildActorSlot(object);
    if (!out || slot < 0 || !WildActorMatches(slot) || !sWildActors[slot].hasMon
        || sWildActors[slot].graphics != object->graphicsId) return FALSE;
    *out = sWildActors[slot].mon; return TRUE;
}
void ModsWildActorsBeforeSave(void)
{
    u8 *state = WildActorState();
    if (!state || !RhhModsHasActiveRun()) return;
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
    {
        u8 *traitState = WildTraitState();
        if (traitState && (!WildActorMatches(i) || !sWildActors[i].individual
            || WildActorRead64(WildTraitRecord(traitState, i)) != sWildActors[i].individual))
            memset(WildTraitRecord(traitState, i), 0, WILD_TRAIT_RECORD_SIZE);
        u8 *profileState = WildProfileState();
        if (profileState && (!WildActorMatches(i) || !sWildActors[i].individual
            || WildActorRead64(WildProfileRecord(profileState, i)) != sWildActors[i].individual))
            memset(WildProfileRecord(profileState, i), 0, WILD_PROFILE_RECORD_SIZE);
        u8 *activityState = WildActivityState();
        if (activityState && (!WildActorMatches(i) || !sWildActors[i].individual
            || WildActorRead64(WildActivityRecord(activityState, i)) != sWildActors[i].individual))
            memset(WildActivityRecord(activityState, i), 0, WILD_ACTIVITY_RECORD_SIZE);
        u8 *queueState = WildQueueState();
        if (queueState && (!WildActorMatches(i) || !sWildActors[i].individual
            || WildActorRead64(WildQueueRecord(queueState, i)) != sWildActors[i].individual))
            memset(WildQueueRecord(queueState, i), 0, WILD_QUEUE_RECORD_SIZE);
        if (WildActorMatches(i))
        {
            struct WildActorEntry *entry = &sWildActors[i]; struct ObjectEvent *object = &gObjectEvents[i];
            if (!entry->individual) entry->individual = WildActorAllocateIndividual(TRUE);
            const struct Pokemon *mon = entry->hasMon && entry->graphics == object->graphicsId ? &entry->mon : NULL;
            if (!WildActorStore(i, object, mon, entry->individual))
            {
                if (mon) entry->hasMon = FALSE;
                if (!WildActorStore(i, object, NULL, entry->individual) && !entry->identified) entry->individual = 0;
            }
            WildConcealStore(i, object, &entry->conceal);
        }
        else
        {
            memset(WildActorRecord(state, i), 0, WILD_ACTOR_RECORD_SIZE);
            WildConcealStore(i, &gObjectEvents[i], NULL);
        }
    }
}
void ModsWildActorsAfterSave(void)
{
    if (NativeSaveTransactionActive()) return;
    sWildActorLease.failed = FALSE; sWildActorLease.confirmed = TRUE;
    if (!RhhModsRunCommitted()) return;
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
    {
        struct WildActorEntry *entry = &sWildActors[i];
        if (!WildActorMatches(i) || !entry->individual || entry->identified) continue;
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
        if (WildActorAt(i, &actor)) continue;
        entry->identified = TRUE; entry->needsSave = FALSE;
        struct RhhWildActorIndividual id = {.struct_size = sizeof(id), .run_id = entry->runId, .serial = entry->individual};
        WildActorNotify(RHH_WILD_ACTOR_IDENTIFIED, RHH_WILD_ACTOR_ID_ASSIGNED, &actor, &id);
    }
}
void ModsWildActorsDiscardLive(void)
{
    memset(sWildActors, 0, sizeof(sWildActors));
    sWildBehaviorBusy = FALSE; sWildBehaviorCount = 0; sWildBehaviorContext = NULL;
    sWildActivityCollecting = sWildActivityInvalid = FALSE; sWildActivityCount = 0;
    sWildSelectionContext = NULL; sWildActivityChangeContext = NULL;
    memset(&sWildActorLease, 0, sizeof(sWildActorLease));
}
void ModsWildActorsAfterLoad(void)
{
    sWildBehaviorCount = 0;
    u8 *state = WildActorState();
    if (!state || !RhhModsHasActiveRun()) return;
    struct Pokemon mons[OBJECT_EVENTS_COUNT]; bool8 hasMon[OBJECT_EVENTS_COUNT] = {0};
    u64 individuals[OBJECT_EVENTS_COUNT] = {0};
    struct RhhWildConcealment conceal[OBJECT_EVENTS_COUNT] = {0};
    u8 *concealState = WildConcealState();
    const u8 *sequence = RhhModsState(sWildActorSequenceId);
    u64 highWater = sequence ? WildActorRead64(sequence) : 0;
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
    {
        const u8 *record = WildActorRecord(state, i); const struct ObjectEvent *object = &gObjectEvents[i];
        if (record[0] && object->active && object->trainerType == TRAINER_TYPE_OW_WILD_ENCOUNTER
            && object->localId == record[1] && object->mapGroup == record[2] && object->mapNum == record[3]
            && object->graphicsId == WildActorRead16(record + 4)
            && (object->trainerRange_berryTreeId & 0x7f) == record[6])
        {
            if (record[0] == 1)
            { memcpy(&mons[i], record + WILD_ACTOR_RECORD_HEADER, sizeof(mons[i])); hasMon[i] = TRUE; }
            u64 individual = WildActorRead64(record + 8);
            if (individual <= highWater) individuals[i] = individual;
        }
    }
    if (concealState)
        for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
        {
            const u8 *p = WildConcealRecord(concealState, i); const struct ObjectEvent *object = &gObjectEvents[i];
            if (p[0] && object->active && object->trainerType == TRAINER_TYPE_OW_WILD_ENCOUNTER
                && object->localId == p[3] && object->mapGroup == p[4] && object->mapNum == p[5]
                && (object->trainerRange_berryTreeId & 0x7f) == p[6] && object->graphicsId == WildActorRead16(p + 32)
                && (!WildActorRead64(p + 16) || WildActorRead64(p + 16) == individuals[i])
                && RhhModsRegisteredKey(WildActorRead64(p + 8)))
                conceal[i] = (struct RhhWildConcealment){.struct_size = sizeof(conceal[i]), .mode = p[0],
                    .reveal_flags = p[1], .reveal_radius = p[2], .owner = WildActorRead64(p + 8),
                    .revision = WildActorRead64(p + 24)};
        }
    if (concealState) memset(concealState, 0, WILD_CONCEAL_STATE_SIZE);
    u8 *traitState = WildTraitState();
    const struct RhhEcologyTagsService *tags = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/ecology_tags", RHH_SERVICE_VERSION(1, 0));
    if (traitState)
        for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
        {
            u8 *p = WildTraitRecord(traitState, i);
            if (!individuals[i] || WildActorRead64(p) != individuals[i])
            { memset(p, 0, WILD_TRAIT_RECORD_SIZE); continue; }
            u32 kept = 0; bool32 removed = FALSE;
            for (u32 j = 0; j < RHH_WILD_TRAIT_OVERRIDE_CAPACITY; ++j)
            {
                u8 *item = p + WILD_TRAIT_HEADER + j * WILD_TRAIT_ITEM_SIZE;
                RhhContentKey key = WildActorRead64(item);
                if (!key) break;
                struct RhhEcologyTag info = {.struct_size = sizeof(info)};
                if (!tags || tags->find(key, &info) || !RhhModsRegisteredKey(WildActorRead64(item + 8)))
                { removed = TRUE; continue; }
                if (kept != j) memmove(p + WILD_TRAIT_HEADER + kept * WILD_TRAIT_ITEM_SIZE, item, WILD_TRAIT_ITEM_SIZE);
                ++kept;
            }
            memset(p + WILD_TRAIT_HEADER + kept * WILD_TRAIT_ITEM_SIZE, 0,
                   (RHH_WILD_TRAIT_OVERRIDE_CAPACITY - kept) * WILD_TRAIT_ITEM_SIZE);
            if (removed && WildActorRead64(p + 8) != UINT64_MAX)
                WildActorWrite64(p + 8, WildActorRead64(p + 8) + 1);
        }
    u8 *profileState = WildProfileState();
    if (profileState)
        for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
        {
            u8 *p = WildProfileRecord(profileState, i);
            if (!individuals[i] || WildActorRead64(p) != individuals[i])
                memset(p, 0, WILD_PROFILE_RECORD_SIZE);
        }
    u8 *activityState = WildActivityState();
    if (activityState)
        for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
        {
            u8 *p = WildActivityRecord(activityState, i);
            if (!individuals[i] || WildActorRead64(p) != individuals[i]
                || (WildActorRead64(p) && !RhhModsRegisteredKey(WildActorRead64(p + 16))))
                memset(p, 0, WILD_ACTIVITY_RECORD_SIZE);
        }
    u8 *queueState = WildQueueState();
    if (queueState)
        for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
        {
            u8 *p = WildQueueRecord(queueState, i);
            if (!individuals[i] || WildActorRead64(p) != individuals[i]) memset(p, 0, WILD_QUEUE_RECORD_SIZE);
        }
    memset(&sWildActorLease, 0, sizeof(sWildActorLease));
    memset(state, 0, WILD_ACTOR_STATE_SIZE); memset(sWildActors, 0, sizeof(sWildActors));
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
        if (gObjectEvents[i].active && gObjectEvents[i].trainerType == TRAINER_TYPE_OW_WILD_ENCOUNTER
            && IS_OW_MON_OBJ(&gObjectEvents[i]))
        {
            WildActorTrack(&gObjectEvents[i], hasMon[i] ? &mons[i] : NULL, individuals[i], RHH_WILD_ACTOR_RESTORE, &conceal[i], TRUE);
            if (conceal[i].mode) WildConcealStore(i, &gObjectEvents[i], &conceal[i]);
        }
}
static u32 NaturalWeather(u32 weather)
{
    switch (weather)
    {
    case WEATHER_NONE: return RHH_NATURAL_WEATHER_NONE;
    case WEATHER_SUNNY_CLOUDS: return RHH_NATURAL_WEATHER_SUNNY_CLOUDS;
    case WEATHER_SUNNY: return RHH_NATURAL_WEATHER_SUNNY;
    case WEATHER_RAIN: return RHH_NATURAL_WEATHER_RAIN;
    case WEATHER_SNOW: return RHH_NATURAL_WEATHER_SNOW;
    case WEATHER_RAIN_THUNDERSTORM: return RHH_NATURAL_WEATHER_THUNDERSTORM;
    case WEATHER_FOG_HORIZONTAL: case WEATHER_FOG_DIAGONAL: case WEATHER_FOG: return RHH_NATURAL_WEATHER_FOG;
    case WEATHER_VOLCANIC_ASH: return RHH_NATURAL_WEATHER_ASH;
    case WEATHER_SANDSTORM: return RHH_NATURAL_WEATHER_SANDSTORM;
    case WEATHER_SHADE: return RHH_NATURAL_WEATHER_SHADE;
    case WEATHER_DROUGHT: return RHH_NATURAL_WEATHER_DROUGHT;
    case WEATHER_DOWNPOUR: return RHH_NATURAL_WEATHER_DOWNPOUR;
    case WEATHER_UNDERWATER: return RHH_NATURAL_WEATHER_UNDERWATER;
    case WEATHER_UNDERWATER_BUBBLES: return RHH_NATURAL_WEATHER_UNDERWATER_BUBBLES;
    case WEATHER_ABNORMAL: case WEATHER_ROUTE119_CYCLE: case WEATHER_ROUTE123_CYCLE:
    case WEATHER_DYNAMIC: return RHH_NATURAL_WEATHER_DYNAMIC;
    default: return RHH_NATURAL_WEATHER_UNKNOWN;
    }
}
static struct { u64 run, sequence; RhhContentKey map; u32 engine; bool32 valid; } sNaturalWeatherObservation;
static const struct RhhNaturalWeatherChange *sNaturalWeatherChange;
void RhhModsResetNaturalWeatherObservation(void)
{
    memset(&sNaturalWeatherObservation, 0, sizeof(sNaturalWeatherObservation));
    sNaturalWeatherChange = NULL;
}
static int NaturalWeatherChangeContext(struct RhhNaturalWeatherChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sNaturalWeatherChange
        || RhhModsCurrentEvent() != RHH_NATURAL_WEATHER_CHANGED) return -1;
    *out = *sNaturalWeatherChange; return 0;
}
void RhhModsObserveNaturalWeather(void)
{
    if (!RhhModsHasActiveRun() || !RhhModsHasHook(RHH_NATURAL_WEATHER_CHANGED)) {
        sNaturalWeatherObservation.valid = FALSE; return;
    }
    if (sNaturalWeatherChange || RhhModsCurrentEvent() || NativeSaveTransactionActive()) return;
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)};
    if (Current(&world) || !world.map) { sNaturalWeatherObservation.valid = FALSE; return; }
    u64 run = RhhModsRunId(); u32 engine = GetCurrentWeather();
    if (!sNaturalWeatherObservation.valid || sNaturalWeatherObservation.run != run || sNaturalWeatherObservation.map != world.map) {
        sNaturalWeatherObservation.run = run; sNaturalWeatherObservation.map = world.map;
        sNaturalWeatherObservation.engine = engine; sNaturalWeatherObservation.valid = TRUE; return;
    }
    u32 before = sNaturalWeatherObservation.engine;
    if (before == engine) return;
    sNaturalWeatherObservation.engine = engine;
    if (sNaturalWeatherObservation.sequence == UINT64_MAX) return;
    struct RhhNaturalWeatherChange context = {.struct_size = sizeof(context), .before_engine = before, .after_engine = engine,
        .before_natural = NaturalWeather(before), .after_natural = NaturalWeather(engine), .map = world.map,
        .run_id = run, .sequence = sNaturalWeatherObservation.sequence++};
    const struct RhhWorldClockService *clock = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldClockSnapshot now = {.struct_size = sizeof(now)};
    if (clock && !clock->current(&now) && now.run_id == run)
    { context.step_available = TRUE; context.step = now.steps; }
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_NATURAL_WEATHER_CHANGED,
        .run_seed = RhhModsRunSeed(), .event_key = world.map, .ordinal = context.sequence,
        .args = {context.before_natural, context.after_natural, before, engine}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sNaturalWeatherChange = &context; RhhModsDispatch(&event, &decision); sNaturalWeatherChange = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static int LightingInputs(struct RhhWorldLightingInputs *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsFinalized()) return -1;
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)};
    if (Current(&world) || !world.map) return -1;
    struct RhhWorldLightingInputs result = {.struct_size = sizeof(result), .map = world.map,
        .engine_weather = GetCurrentWeather()};
    result.natural_weather = NaturalWeather(result.engine_weather);
    switch (world.map_type)
    {
    case MAP_TYPE_TOWN: case MAP_TYPE_CITY: case MAP_TYPE_ROUTE: case MAP_TYPE_OCEAN_ROUTE:
        result.flags = RHH_WORLD_LIGHT_OUTDOORS; break;
    case MAP_TYPE_INDOOR: case MAP_TYPE_SECRET_BASE: result.flags = RHH_WORLD_LIGHT_INDOORS; break;
    case MAP_TYPE_UNDERGROUND: result.flags = RHH_WORLD_LIGHT_UNDERGROUND; break;
    case MAP_TYPE_UNDERWATER: result.flags = RHH_WORLD_LIGHT_UNDERWATER; break;
    }
    const struct RhhWorldTimeService *time = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_time", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWorldEnvironmentService *env = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    result.time.struct_size = sizeof(result.time);
    result.time_available = time && !time->sample(&result.time);
    if (!result.time_available) memset(&result.time, 0, sizeof(result.time));
    result.environment.struct_size = sizeof(result.environment);
    result.environment_available = env && !env->current(&result.environment);
    if (!result.environment_available) memset(&result.environment, 0, sizeof(result.environment));
    *out = result; return 0;
}
static int HabitatContext(struct RhhHabitatSample *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sHabitatContext) return -1;
    *out = *sHabitatContext; return 0;
}
static int HabitatBaseFlags(u32 behavior, u32 mapType, u32 *out)
{
    if (!out || behavior >= NUM_METATILE_BEHAVIORS) return -1;
    u32 flags = 0;
    if (MetatileBehavior_IsPokeGrass(behavior) || MetatileBehavior_IsShortGrass(behavior)
        || MetatileBehavior_IsAshGrass(behavior) || MetatileBehavior_IsLongGrassSouthEdge(behavior))
        flags |= RHH_HABITAT_GRASS;
    if (MetatileBehavior_IsTallGrass(behavior)) flags |= RHH_HABITAT_TALL_GRASS;
    if (MetatileBehavior_IsLongGrass(behavior) || MetatileBehavior_IsLongGrassSouthEdge(behavior)) flags |= RHH_HABITAT_LONG_GRASS;
    if (MetatileBehavior_IsSandOrDeepSand(behavior)) flags |= RHH_HABITAT_SAND;
    if (MetatileBehavior_IsSurfableWaterOrUnderwater(behavior)) flags |= RHH_HABITAT_WATER;
    if (MetatileBehavior_IsShallowFlowingWater(behavior) || MetatileBehavior_IsPuddle(behavior))
        flags |= RHH_HABITAT_WATER | RHH_HABITAT_SHALLOW_WATER;
    if (MetatileBehavior_IsIce(behavior)) flags |= RHH_HABITAT_ICE;
    if (MetatileBehavior_IsEncounterTile(behavior)) flags |= RHH_HABITAT_ENCOUNTER_TILE;
    if (mapType == MAP_TYPE_UNDERGROUND) flags |= RHH_HABITAT_UNDERGROUND;
    if (mapType == MAP_TYPE_UNDERWATER) flags |= RHH_HABITAT_UNDERWATER;
    if (mapType == MAP_TYPE_INDOOR || mapType == MAP_TYPE_SECRET_BASE) flags |= RHH_HABITAT_INDOORS;
    *out = flags; return 0;
}
static int AuthoredMapResolve(RhhContentKey key, const struct MapHeader **out, RhhContentKey *canonical)
{
    u32 id;
    if (!out || !RhhModsFinalized() || RhhContentResolve(RHH_MAP, key, &id) || id > UINT16_MAX) return -1;
    u32 group = id >> 8, number = id & 0xff;
    if (group >= MAP_GROUPS_COUNT || number >= MAP_GROUP_COUNT[group] || !gMapGroups[group]) return -1;
    const struct MapHeader *header = gMapGroups[group][number];
    if (!header || !header->mapLayout || header->mapLayout->width <= 0 || header->mapLayout->height <= 0
        || !header->mapLayout->map) return -1;
    RhhContentKey resolved = RhhContentKeyForId(RHH_MAP, id);
    if (!resolved) return -1;
    *out = header; if (canonical) *canonical = resolved; return 0;
}
static int AuthoredMap(RhhContentKey key, struct RhhAuthoredMap *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct MapHeader *header; RhhContentKey map;
    if (AuthoredMapResolve(key, &header, &map)) return -1;
    struct RhhAuthoredMap result = {.struct_size = sizeof(result), .map = map,
        .area = RhhContentKeyForId(RHH_AREA, header->regionMapSectionId),
        .width = header->mapLayout->width, .height = header->mapLayout->height,
        .map_type = header->mapType, .engine_weather = header->weather,
        .natural_weather = NaturalWeather(header->weather),
        .flags = (header->mapLayout->isFrlg ? RHH_AUTHORED_MAP_FRLG : 0)
            | (header->allowCycling ? RHH_AUTHORED_MAP_CYCLING : 0)
            | (header->allowEscaping ? RHH_AUTHORED_MAP_ESCAPE : 0)
            | (header->allowRunning ? RHH_AUTHORED_MAP_RUNNING : 0)
            | (header->cave ? RHH_AUTHORED_MAP_CAVE : 0)};
    *out = result; return 0;
}
static int AuthoredWildVariants(RhhContentKey key, u32 *out)
{
    const struct MapHeader *header; RhhContentKey map; u32 id, count = 0;
    if (!out || AuthoredMapResolve(key, &header, &map) || RhhContentResolve(RHH_MAP, map, &id)) return -1;
    for (u32 i = 0; gWildMonHeaders[i].mapGroup != MAP_GROUP(MAP_UNDEFINED); ++i)
        if (gWildMonHeaders[i].mapGroup == (id >> 8) && gWildMonHeaders[i].mapNum == (id & 0xff)) ++count;
    *out = count; return 0;
}
static int AuthoredWildTable(RhhContentKey key, u32 variant, u32 time, u32 area, struct RhhAuthoredWildTable *out)
{
    _Static_assert(TIME_MORNING == 0 && TIME_DAY == 1 && TIME_EVENING == 2 && TIME_NIGHT == 3,
                   "wild time mapping must match engine periods");
    _Static_assert(NUM_LAND_MONS_ENCOUNTER_SLOTS <= RHH_AUTHORED_WILD_SLOT_CAPACITY
        && NUM_WATER_MONS_ENCOUNTER_SLOTS <= RHH_AUTHORED_WILD_SLOT_CAPACITY
        && NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS <= RHH_AUTHORED_WILD_SLOT_CAPACITY
        && NUM_FISHING_MONS_ENCOUNTER_SLOTS <= RHH_AUTHORED_WILD_SLOT_CAPACITY
        && NUM_HIDDEN_MONS_ENCOUNTER_SLOTS <= RHH_AUTHORED_WILD_SLOT_CAPACITY, "wild slot capacity");
    const struct MapHeader *header; RhhContentKey map; u32 id;
    if (!out || out->struct_size < sizeof(*out) || time < RHH_WILD_TIME_MORNING || time > RHH_WILD_TIME_NIGHT
        || area < RHH_WILD_LAND || area > RHH_WILD_HIDDEN
        || AuthoredMapResolve(key, &header, &map) || RhhContentResolve(RHH_MAP, map, &id)) return -1;
    const struct WildPokemonHeader *selected = NULL;
    u32 found = 0;
    for (u32 i = 0; gWildMonHeaders[i].mapGroup != MAP_GROUP(MAP_UNDEFINED); ++i)
        if (gWildMonHeaders[i].mapGroup == (id >> 8) && gWildMonHeaders[i].mapNum == (id & 0xff))
            if (found++ == variant) { selected = &gWildMonHeaders[i]; break; }
    if (!selected) return -1;
    const struct WildEncounterTypes *types = &selected->encounterTypes[time - RHH_WILD_TIME_MORNING];
    const struct WildPokemonInfo *info = NULL; u32 count = 0;
    switch (area)
    {
    case RHH_WILD_LAND: info = types->landMonsInfo; count = NUM_LAND_MONS_ENCOUNTER_SLOTS; break;
    case RHH_WILD_WATER: info = types->waterMonsInfo; count = NUM_WATER_MONS_ENCOUNTER_SLOTS; break;
    case RHH_WILD_ROCKS: info = types->rockSmashMonsInfo; count = NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS; break;
    case RHH_WILD_FISHING: info = types->fishingMonsInfo; count = NUM_FISHING_MONS_ENCOUNTER_SLOTS; break;
    case RHH_WILD_HIDDEN: info = types->hiddenMonsInfo; count = NUM_HIDDEN_MONS_ENCOUNTER_SLOTS; break;
    }
    struct RhhAuthoredWildTable result = {.struct_size = sizeof(result), .map = map,
        .area = area, .time = time, .variant = variant};
    if (info)
    {
        if (!info->wildPokemon) return -1;
        result.present = TRUE; result.encounter_rate = info->encounterRate; result.slot_count = count;
        for (u32 i = 0; i < count; ++i)
        {
            const struct WildPokemon *slot = &info->wildPokemon[i];
            result.slots[i] = (struct RhhAuthoredWildSlot){.index = i, .min_level = slot->minLevel,
                .max_level = slot->maxLevel, .species_ordinal = slot->species,
                .species = RhhContentKeyForId(RHH_SPECIES, slot->species)};
        }
    }
    *out = result; return 0;
}
static int AuthoredWildBaseWeights(u32 area, u32 rod, struct RhhAuthoredWildWeights *out)
{
    _Static_assert(WILD_AREA_LAND == 0 && WILD_AREA_WATER == 1 && WILD_AREA_ROCKS == 2
        && WILD_AREA_FISHING == 3 && WILD_AREA_HIDDEN == 4, "wild area mapping must match public order");
    if (!out || out->struct_size < sizeof(*out) || !RhhModsFinalized()
        || area < RHH_WILD_LAND || area > RHH_WILD_HIDDEN) return -1;
    u16 weights[RHH_AUTHORED_WILD_SLOT_CAPACITY] = {0}; u32 first, count;
    if (!ModsWildBaseSlotWeights(area - RHH_WILD_LAND, rod, weights, &first, &count)
        || first > RHH_AUTHORED_WILD_SLOT_CAPACITY || count > RHH_AUTHORED_WILD_SLOT_CAPACITY - first) return -1;
    struct RhhAuthoredWildWeights result = {.struct_size = sizeof(result), .area = area, .rod = rod,
        .available = !!count, .first_slot = first, .slot_count = count};
    for (u32 i = 0; i < count; ++i)
    { result.weights[first + i] = weights[i]; result.total += weights[i]; }
    *out = result; return 0;
}
static int AuthoredWildSelection(RhhContentKey key, u32 time, u32 area, struct RhhAuthoredWildSelection *out)
{
    const struct MapHeader *header; RhhContentKey map; u32 id, count, variant; bool32 clamped;
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || !gSaveBlock1Ptr
        || NativeSaveTransactionActive() || time < RHH_WILD_TIME_MORNING || time > RHH_WILD_TIME_NIGHT
        || area < RHH_WILD_LAND || area > RHH_WILD_HIDDEN
        || AuthoredMapResolve(key, &header, &map) || RhhContentResolve(RHH_MAP, map, &id)
        || AuthoredWildVariants(map, &count)) return -1;
    struct RhhAuthoredWildSelection result = {.struct_size = sizeof(result), .map = map,
        .requested_time = time, .area = area, .variant_count = count, .run_id = RhhModsRunId()};
    u16 selected = ModsWildHeaderForMap(id >> 8, id & 0xff, &variant, &clamped);
    if (selected == HEADER_NONE) { if (count) return -1; }
    else
    {
        if (variant >= count || gWildMonHeaders[selected].mapGroup != (id >> 8)
            || gWildMonHeaders[selected].mapNum != (id & 0xff)) return -1;
        u32 flags = 0;
        enum TimeOfDay resolved = ModsWildResolveTime(&gWildMonHeaders[selected], area - RHH_WILD_LAND,
            time - RHH_WILD_TIME_MORNING, FALSE, &flags);
        if (resolved < TIME_MORNING || resolved > TIME_NIGHT) return -1;
        result.has_header = TRUE; result.variant = variant; result.selected_time = resolved + RHH_WILD_TIME_MORNING;
        result.flags = flags | (clamped ? RHH_AUTHORED_WILD_VARIANT_CLAMPED : 0);
        result.table.struct_size = sizeof(result.table);
        if (AuthoredWildTable(map, variant, result.selected_time, area, &result.table)) return -1;
    }
    if (!RhhModsHasActiveRun() || result.run_id != RhhModsRunId() || NativeSaveTransactionActive()) return -1;
    *out = result; return 0;
}
static int AuthoredHabitatSample(RhhContentKey key, s32 x, s32 y, struct RhhAuthoredHabitatSample *out);
static const struct RhhAuthoredWildWeightContext *sAuthoredWildWeightContext;
static int AuthoredWildPreviewContext(struct RhhAuthoredWildWeightContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAuthoredWildWeightContext
        || RhhModsCurrentEvent() != RHH_AUTHORED_WILD_WEIGHT) return -1;
    *out = *sAuthoredWildWeightContext; return 0;
}
static int AuthoredWildPreview(const struct RhhAuthoredWildPreviewRequest *request, struct RhhAuthoredWildPreview *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved[0] || request->reserved[1]
        || !out || out->struct_size < sizeof(*out) || sAuthoredWildWeightContext
        || !RhhModsHasActiveRun() || NativeSaveTransactionActive()) return -1;
    struct RhhAuthoredWildPreview result = {.struct_size = sizeof(result), .request = *request,
        .selection.struct_size = sizeof(result.selection), .habitat.struct_size = sizeof(result.habitat),
        .weights.struct_size = sizeof(result.weights)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    struct RhhWorldTimeScope scope; RhhModsWorldTimeScopeBegin(&scope);
    int status = -1;
    if (AuthoredWildSelection(request->map, request->time, request->area, &result.selection)
        || AuthoredWildBaseWeights(request->area, request->rod, &result.weights)
        || AuthoredHabitatSample(request->map, request->x, request->y, &result.habitat)) goto done;
    struct RhhAuthoredWildWeights baseline = result.weights;
    if (result.selection.has_header && result.selection.table.present && baseline.available) {
        if (baseline.first_slot > result.selection.table.slot_count
            || baseline.slot_count > result.selection.table.slot_count - baseline.first_slot) goto done;
        result.available = TRUE; result.total = 0;
        for (u32 i = baseline.first_slot; i < baseline.first_slot + baseline.slot_count; ++i) {
            struct RhhAuthoredWildWeightContext context = {.struct_size = sizeof(context), .slot = i,
                .base_weight = baseline.weights[i], .selected_time = result.selection.selected_time,
                .variant = result.selection.variant, .selection_flags = result.selection.flags, .request = *request,
                .candidate = result.selection.table.slots[i], .habitat = result.habitat};
            struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_AUTHORED_WILD_WEIGHT,
                .run_seed = RhhModsRunSeed(), .event_key = result.selection.map, .subject = context.candidate.species,
                .ordinal = request->ordinal, .args = {request->area, request->rod, i, request->time}};
            struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = context.base_weight};
            rng_value_t before = gRngValue, before2 = gRng2Value; enum Ability ability = gLastUsedAbility;
            sAuthoredWildWeightContext = &context; RhhModsDispatch(&event, &decision); sAuthoredWildWeightContext = NULL;
            bool32 used = memcmp(&before, &gRngValue, sizeof(before)) || memcmp(&before2, &gRng2Value, sizeof(before2));
            gRngValue = before; gRng2Value = before2; gLastUsedAbility = ability;
            result.weights.weights[i] = !decision.allowed ? 0 : !used && decision.value <= RHH_WILD_MAX_WEIGHT
                ? decision.value : context.base_weight;
            result.total += result.weights.weights[i];
        }
        if (!result.total) { result.fallback = TRUE; result.weights = baseline; result.total = baseline.total; }
        result.weights.total = result.total;
    }
    struct RhhAuthoredWildSelection current = {.struct_size = sizeof(current)};
    struct RhhAuthoredHabitatSample habitat = {.struct_size = sizeof(habitat)};
    if (AuthoredWildSelection(request->map, request->time, request->area, &current)
        || AuthoredHabitatSample(request->map, request->x, request->y, &habitat)
        || memcmp(&current, &result.selection, sizeof(current)) || memcmp(&habitat, &result.habitat, sizeof(habitat))) goto done;
    status = 0;
done:
    RhhModsWorldTimeScopeEnd(&scope); gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
    if (!status) *out = result;
    return status;
}
static const struct RhhAuthoredEncounterService sAuthoredEncounterService = {
    {sizeof(sAuthoredEncounterService), RHH_SERVICE_VERSION(1, 3)}, AuthoredWildVariants, AuthoredWildTable,
    AuthoredWildBaseWeights, AuthoredWildSelection, AuthoredWildPreview, AuthoredWildPreviewContext
};
int RhhModsAuthoredTileForLayout(const struct MapLayout *layout, u32 mapType, RhhContentKey map,
                                 s32 x, s32 y, struct RhhAuthoredMapTile *out)
{
    if (!out || out->struct_size < sizeof(*out) || !layout || !layout->map
        || layout->width <= 0 || layout->height <= 0 || x < 0 || y < 0 || x >= layout->width || y >= layout->height
        || (size_t)layout->width > SIZE_MAX / (size_t)layout->height) return -1;
    u16 block = layout->map[(size_t)y * layout->width + x];
    if (block == MAPGRID_UNDEFINED) return -1;
    u32 metatile = UNPACK_METATILE(block), primary = GetNumMetatilesInPrimary(layout);
    if (metatile >= NUM_METATILES_TOTAL) return -1;
    const struct Tileset *tileset = metatile < primary ? layout->primaryTileset : layout->secondaryTileset;
    if (!tileset || !tileset->metatiles || !tileset->metatileAttributes) return -1;
    u32 offset = metatile < primary ? metatile : metatile - primary;
    u32 attributes = layout->isFrlg ? ((const u32 *)tileset->metatileAttributes)[offset] : tileset->metatileAttributes[offset];
    struct RhhAuthoredMapTile result = {.struct_size = sizeof(result), .map = map, .x = x, .y = y,
        .tile = {.struct_size = sizeof(result.tile), .metatile = metatile,
            .behavior = ExtractMetatileAttribute(attributes, METATILE_ATTRIBUTE_BEHAVIOR, layout->isFrlg),
            .layer = ExtractMetatileAttribute(attributes, METATILE_ATTRIBUTE_LAYER_TYPE, layout->isFrlg),
            .collision = UNPACK_COLLISION(block), .elevation = UNPACK_ELEVATION(block)}};
    result.habitat_available = !HabitatBaseFlags(result.tile.behavior, mapType, &result.habitat_flags);
    memcpy(result.tile.tiles, tileset->metatiles + offset * NUM_TILES_PER_METATILE, sizeof(result.tile.tiles));
    *out = result; return 0;
}
static int AuthoredMapTile(RhhContentKey key, int32_t x, int32_t y, struct RhhAuthoredMapTile *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct MapHeader *header; RhhContentKey map;
    if (AuthoredMapResolve(key, &header, &map)) return -1;
    return RhhModsAuthoredTileForLayout(header->mapLayout, header->mapType, map, x, y, out);
}
int RhhModsAuthoredScanForLayout(const struct MapLayout *layout, u32 mapType, RhhContentKey map,
                                 const struct RhhAuthoredMapScanRequest *request, struct RhhAuthoredMapScan *out)
{
    if (!request || request->struct_size < sizeof(*request) || !out || out->struct_size < sizeof(*out)
        || !map || !layout || !layout->map || layout->width <= 0 || layout->height <= 0) return -1;
    struct RhhAuthoredMapScanRequest q = *request;
    if (q.reserved || !q.width || !q.height || !q.limit || q.limit > RHH_AUTHORED_MAP_SCAN_MAX) return -1;
    if (q.x >= (u32)layout->width || q.y >= (u32)layout->height
        || q.width > (u32)layout->width - q.x || q.height > (u32)layout->height - q.y
        || q.width > UINT32_MAX / q.height) return -1;
    u32 total = q.width * q.height;
    if (q.first > total) return -1;
    struct RhhAuthoredMapScan result = {.struct_size = sizeof(result), .total = total, .map = map};
    result.count = total - q.first < q.limit ? total - q.first : q.limit;
    result.next = q.first + result.count;
    for (u32 i = 0; i < result.count; ++i)
    {
        u32 index = q.first + i;
        struct RhhAuthoredMapCell *cell = &result.cells[i];
        cell->x = q.x + index % q.width; cell->y = q.y + index / q.width;
        cell->tile.struct_size = sizeof(cell->tile);
        if (RhhModsAuthoredTileForLayout(layout, mapType, map, cell->x, cell->y, &cell->tile))
        { memset(&cell->tile, 0, sizeof(cell->tile)); cell->status = RHH_AUTHORED_MAP_CELL_UNAVAILABLE; }
    }
    *out = result; return 0;
}
static int AuthoredMapScan(const struct RhhAuthoredMapScanRequest *request, struct RhhAuthoredMapScan *out)
{
    if (!request || request->struct_size < sizeof(*request) || !out || out->struct_size < sizeof(*out)) return -1;
    const struct MapHeader *header; RhhContentKey map;
    if (AuthoredMapResolve(request->map, &header, &map)) return -1;
    return RhhModsAuthoredScanForLayout(header->mapLayout, header->mapType, map, request, out);
}
struct AuthoredTileSource {
    const u8 *pixels;
    u8 *decoded;
    u32 length;
    bool8 inspected, compressed;
};
/* Zero means ready, one means unavailable, and -1 means allocation failure. */
static int AuthoredTileSourceAt(struct AuthoredTileSource *source, const struct Tileset *graphics, u32 offset)
{
    if (!source->inspected)
    {
        source->inspected = TRUE;
        if (!graphics || !graphics->tiles) return 1;
        source->pixels = (const u8 *)graphics->tiles; source->compressed = graphics->isCompressed;
        if (source->compressed)
        {
            union CompressionHeader headerBytes; memcpy(&headerBytes, graphics->tiles, sizeof(headerBytes));
            if (headerBytes.smol.mode > ENCODE_BOTH_DELTA_SYMS
                || (headerBytes.smol.mode == MODE_LZ77 && source->pixels[0] != 0x10)
                || (headerBytes.smol.mode != MODE_LZ77 && (!headerBytes.smol.loSize || !headerBytes.smol.symSize))) return 1;
            source->length = GetDecompressedDataSize(graphics->tiles);
            if (source->length > MAX_DECOMPRESSION_BUFFER_SIZE) source->length = 0;
        }
        else source->length = RhhModsRawTilesetSize(graphics);
    }
    if (!source->length || offset > source->length || source->length - offset < 32) return 1;
    if (source->compressed && !source->decoded)
    {
        source->decoded = calloc(1, source->length); if (!source->decoded) return -1;
        if (!TryDecompressDataWithHeaderWram(graphics->tiles, source->decoded)) return -1;
        source->pixels = source->decoded;
    }
    return 0;
}
static int AuthoredTileDecode(const struct MapLayout *layout, u16 tile, struct AuthoredTileSource sources[2], u8 rgba[256])
{
    u32 tileId = tile & 0x3ff, firstTile = GetNumTilesInPrimary(layout);
    u32 bank = tile >> 12, firstPalette = GetNumPalsInPrimary(layout);
    if (bank >= NUM_PALS_TOTAL) return 1;
    u32 sourceIndex = tileId < firstTile ? 0 : 1;
    const struct Tileset *graphics = sourceIndex ? layout->secondaryTileset : layout->primaryTileset;
    const struct Tileset *colors = bank < firstPalette ? layout->primaryTileset : layout->secondaryTileset;
    if (!colors || !colors->palettes) return 1;
    u32 paletteOffset = bank < firstPalette ? bank : bank - firstPalette;
    if (colors->isSecondary) paletteOffset += firstPalette;
    if (paletteOffset >= 16) return 1;
    u32 offset = (sourceIndex ? tileId - firstTile : tileId) * 32;
    int status = AuthoredTileSourceAt(&sources[sourceIndex], graphics, offset);
    if (status) return status;
    const u8 *pixels = sources[sourceIndex].pixels;
    for (u32 y = 0; y < 8; ++y) for (u32 x = 0; x < 8; ++x)
    {
        u32 tx = tile & (1u << 10) ? 7 - x : x, ty = tile & (1u << 11) ? 7 - y : y;
        u32 bits = pixels[offset + ty * 4 + tx / 2], index = (bits >> ((tx & 1) * 4)) & 15;
        u16 color = colors->palettes[paletteOffset][index]; u32 dst = (y * 8 + x) * 4;
        if (!colors->isSecondary && !index && bank == (bank < firstPalette ? 0 : firstPalette)) color = 0;
        for (u32 c = 0; c < 3; ++c) { u32 v = (color >> (c * 5)) & 31; rgba[dst + c] = (v << 3) | (v >> 2); }
        rgba[dst + 3] = index ? 255 : 0;
    }
    return 0;
}
static int AuthoredTileRgba(RhhContentKey key, u16 tile, u8 *rgba, u32 capacity)
{
    if (!rgba || capacity < 8 * 8 * 4) return -1;
    const struct MapHeader *header;
    if (AuthoredMapResolve(key, &header, NULL)) return -1;
    struct AuthoredTileSource sources[2] = {0}; u8 result[8 * 8 * 4];
    int status = AuthoredTileDecode(header->mapLayout, tile, sources, result);
    free(sources[0].decoded); free(sources[1].decoded);
    if (status) return -1;
    memcpy(rgba, result, sizeof(result)); return 0;
}
static int AuthoredTilesRgba(RhhContentKey key, const u16 *tiles, u32 count, struct RhhAuthoredTileImage *out, u32 capacity)
{
    if (count > RHH_AUTHORED_TILE_BATCH_MAX || capacity < count || (count && (!tiles || !out))) return -1;
    const struct MapHeader *header;
    if (AuthoredMapResolve(key, &header, NULL)) return -1;
    if (!count) return 0;
    struct RhhAuthoredTileImage *results = calloc(count, sizeof(*results));
    if (!results) return -1;
    struct AuthoredTileSource sources[2] = {0}; int failed = 0;
    for (u32 i = 0; i < count; ++i)
    {
        results[i].tile = tiles[i];
        int status = AuthoredTileDecode(header->mapLayout, tiles[i], sources, results[i].rgba);
        if (status < 0) { failed = 1; break; }
        results[i].status = status ? RHH_AUTHORED_TILE_UNAVAILABLE : RHH_AUTHORED_TILE_OK;
    }
    free(sources[0].decoded); free(sources[1].decoded);
    if (!failed) memcpy(out, results, count * sizeof(*results));
    free(results); return failed ? -1 : 0;
}
static int AuthoredConnections(const struct MapHeader *header, u32 *count)
{
    if (!header || !count) return -1;
    const struct MapConnections *connections = header->connections;
    if (!connections) { *count = 0; return 0; }
    if (connections->count < 0 || connections->count > 256
        || (connections->count && !connections->connections)) return -1;
    *count = connections->count; return 0;
}
static int AuthoredConnectionCount(RhhContentKey key, u32 *out)
{
    if (!out) return -1;
    const struct MapHeader *header; u32 count;
    if (AuthoredMapResolve(key, &header, NULL) || AuthoredConnections(header, &count)) return -1;
    *out = count; return 0;
}
int RhhModsAuthoredConnectionForHeader(const struct MapHeader *header, RhhContentKey map, u32 index,
                                       struct RhhAuthoredConnection *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    u32 count;
    if (!header || !header->mapLayout || header->mapLayout->width <= 0 || header->mapLayout->height <= 0
        || AuthoredConnections(header, &count) || index >= count) return -1;
    const struct MapConnection *connection = &header->connections->connections[index];
    struct RhhAuthoredConnection result = {.struct_size = sizeof(result), .index = index, .map = map,
        .engine_direction = connection->direction, .offset = connection->offset,
        .target = RhhContentKeyForId(RHH_MAP, (u32)connection->mapGroup << 8 | connection->mapNum)};
    switch (connection->direction)
    {
    case CONNECTION_SOUTH: result.direction = RHH_AUTHORED_CONNECTION_SOUTH; break;
    case CONNECTION_NORTH: result.direction = RHH_AUTHORED_CONNECTION_NORTH; break;
    case CONNECTION_WEST: result.direction = RHH_AUTHORED_CONNECTION_WEST; break;
    case CONNECTION_EAST: result.direction = RHH_AUTHORED_CONNECTION_EAST; break;
    case CONNECTION_DIVE: result.direction = RHH_AUTHORED_CONNECTION_DIVE; break;
    case CONNECTION_EMERGE: result.direction = RHH_AUTHORED_CONNECTION_EMERGE; break;
    default: result.direction = RHH_AUTHORED_CONNECTION_UNKNOWN; break;
    }
    const struct MapHeader *target;
    if (result.target && !AuthoredMapResolve(result.target, &target, NULL))
    {
        const struct MapLayout *sourceLayout = header->mapLayout, *targetLayout = target->mapLayout;
        result.flags |= RHH_AUTHORED_CONNECTION_TARGET_AVAILABLE;
        result.target_width = targetLayout->width; result.target_height = targetLayout->height;
        bool32 vertical = connection->direction == CONNECTION_SOUTH || connection->direction == CONNECTION_NORTH;
        bool32 horizontal = connection->direction == CONNECTION_WEST || connection->direction == CONNECTION_EAST;
        if (vertical || horizontal)
        {
            result.flags |= RHH_AUTHORED_CONNECTION_GEOMETRY_AVAILABLE;
            result.origin_x = vertical ? connection->offset : connection->direction == CONNECTION_WEST ? -targetLayout->width : sourceLayout->width;
            result.origin_y = horizontal ? connection->offset : connection->direction == CONNECTION_NORTH ? -targetLayout->height : sourceLayout->height;
            int64_t sourceExtent = vertical ? sourceLayout->width : sourceLayout->height;
            int64_t targetExtent = vertical ? targetLayout->width : targetLayout->height;
            int64_t start = connection->offset, end = start + targetExtent;
            if (start < 0) start = 0; else if (start > sourceExtent) start = sourceExtent;
            if (end < 0) end = 0; else if (end > sourceExtent) end = sourceExtent;
            if (end < start) end = start;
            result.seam_start = start; result.seam_end = end;
        }
    }
    *out = result; return 0;
}
static int AuthoredConnectionAt(RhhContentKey key, u32 index, struct RhhAuthoredConnection *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct MapHeader *header; RhhContentKey map;
    if (AuthoredMapResolve(key, &header, &map)) return -1;
    return RhhModsAuthoredConnectionForHeader(header, map, index, out);
}
static int AuthoredWarps(const struct MapHeader *header, u32 *count)
{
    if (!header || !count || !header->events || (header->events->warpCount && !header->events->warps)) return -1;
    *count = header->events->warpCount; return 0;
}
static int AuthoredWarpCount(RhhContentKey key, u32 *out)
{
    if (!out) return -1;
    const struct MapHeader *header; u32 count;
    if (AuthoredMapResolve(key, &header, NULL) || AuthoredWarps(header, &count)) return -1;
    *out = count; return 0;
}
int RhhModsAuthoredWarpForHeader(const struct MapHeader *header, RhhContentKey map, u32 index,
                                struct RhhAuthoredWarp *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    u32 count;
    if (AuthoredWarps(header, &count) || index >= count) return -1;
    const struct WarpEvent *warp = &header->events->warps[index];
    struct RhhAuthoredWarp result = {.struct_size = sizeof(result), .index = index, .map = map,
        .x = warp->x, .y = warp->y, .source_elevation = warp->elevation, .target_warp = warp->warpId};
    if (warp->mapNum == MAP_NUM(MAP_DYNAMIC)) result.flags |= RHH_AUTHORED_WARP_DYNAMIC;
    else
    {
        result.target = RhhContentKeyForId(RHH_MAP, (u32)warp->mapGroup << 8 | warp->mapNum);
        const struct MapHeader *target; u32 targetCount;
        if (result.target && !AuthoredMapResolve(result.target, &target, NULL))
        {
            result.flags |= RHH_AUTHORED_WARP_TARGET_AVAILABLE;
            /* WarpData stores the destination index as s8; negative values
             * use the engine's fallback rather than an indexed warp. */
            if (warp->warpId <= INT8_MAX && !AuthoredWarps(target, &targetCount) && warp->warpId < targetCount)
            {
                const struct WarpEvent *dest = &target->events->warps[warp->warpId];
                result.flags |= RHH_AUTHORED_WARP_TARGET_EVENT_AVAILABLE;
                result.target_x = dest->x; result.target_y = dest->y; result.target_elevation = dest->elevation;
                if (dest->x >= 0 && dest->y >= 0 && dest->x < target->mapLayout->width && dest->y < target->mapLayout->height)
                    result.flags |= RHH_AUTHORED_WARP_TARGET_IN_BOUNDS;
            }
        }
    }
    *out = result; return 0;
}
static int AuthoredWarpAt(RhhContentKey key, u32 index, struct RhhAuthoredWarp *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct MapHeader *header; RhhContentKey map;
    if (AuthoredMapResolve(key, &header, &map)) return -1;
    return RhhModsAuthoredWarpForHeader(header, map, index, out);
}
static int AuthoredObjects(const struct MapHeader *header, u32 *count)
{
    if (!header || !count || !header->events
        || (header->events->objectEventCount && !header->events->objectEvents)) return -1;
    *count = header->events->objectEventCount; return 0;
}
static int AuthoredObjectCount(RhhContentKey key, u32 *out)
{
    if (!out) return -1;
    const struct MapHeader *header; u32 count;
    if (AuthoredMapResolve(key, &header, NULL) || AuthoredObjects(header, &count)) return -1;
    *out = count; return 0;
}
int RhhModsAuthoredObjectForHeader(const struct MapHeader *header, RhhContentKey map, u32 index,
                                  struct RhhAuthoredObject *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    u32 count;
    if (AuthoredObjects(header, &count) || index >= count) return -1;
    const struct ObjectEventTemplate *object = &header->events->objectEvents[index];
    struct RhhAuthoredObject result = {.struct_size = sizeof(result), .index = index, .map = map,
        .engine_kind = object->kind, .kind = RHH_AUTHORED_OBJECT_UNKNOWN,
        .local_id = object->localId, .x = object->x, .y = object->y};
    if (object->kind == OBJ_KIND_NORMAL)
    {
        result.kind = RHH_AUTHORED_OBJECT_NORMAL;
        result.graphics_id = object->graphicsId; result.elevation = object->elevation;
        result.movement_type = object->movementType; result.movement_range_x = object->movementRangeX;
        result.movement_range_y = object->movementRangeY; result.trainer_type = object->trainerType;
        result.trainer_range_or_berry_id = object->trainerRange_berryTreeId; result.flag_id = object->flagId;
        if (object->graphicsId >= OBJ_EVENT_GFX_VARS && object->graphicsId <= OBJ_EVENT_GFX_VAR_F)
            result.flags |= RHH_AUTHORED_OBJECT_GRAPHICS_VARIABLE;
    }
    else if (object->kind == OBJ_KIND_CLONE)
    {
        result.kind = RHH_AUTHORED_OBJECT_CLONE; result.target_local_id = object->targetLocalId;
        if (object->targetMapGroup <= UINT8_MAX && object->targetMapNum <= UINT8_MAX)
            result.target = RhhContentKeyForId(RHH_MAP, (u32)object->targetMapGroup << 8 | object->targetMapNum);
        const struct MapHeader *target; u32 targetCount;
        if (result.target && !AuthoredMapResolve(result.target, &target, NULL))
        {
            result.flags |= RHH_AUTHORED_OBJECT_TARGET_AVAILABLE;
            if (!AuthoredObjects(target, &targetCount) && object->targetLocalId && object->targetLocalId <= targetCount)
                result.flags |= RHH_AUTHORED_OBJECT_TARGET_EVENT_AVAILABLE;
        }
    }
    *out = result; return 0;
}
static int AuthoredObjectAt(RhhContentKey key, u32 index, struct RhhAuthoredObject *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct MapHeader *header; RhhContentKey map;
    if (AuthoredMapResolve(key, &header, &map)) return -1;
    return RhhModsAuthoredObjectForHeader(header, map, index, out);
}
static int AuthoredCoords(const struct MapHeader *header, u32 *count)
{
    if (!header || !count || !header->events
        || (header->events->coordEventCount && !header->events->coordEvents)) return -1;
    *count = header->events->coordEventCount; return 0;
}
static int AuthoredCoordCount(RhhContentKey key, u32 *out)
{
    if (!out) return -1;
    const struct MapHeader *header; u32 count;
    if (AuthoredMapResolve(key, &header, NULL) || AuthoredCoords(header, &count)) return -1;
    *out = count; return 0;
}
static int AuthoredCoordWeather(u8 code, u32 *out)
{
    switch (code)
    {
    case COORD_EVENT_WEATHER_SUNNY_CLOUDS: *out = WEATHER_SUNNY_CLOUDS; break;
    case COORD_EVENT_WEATHER_SUNNY: *out = WEATHER_SUNNY; break;
    case COORD_EVENT_WEATHER_RAIN: *out = WEATHER_RAIN; break;
    case COORD_EVENT_WEATHER_SNOW: *out = WEATHER_SNOW; break;
    case COORD_EVENT_WEATHER_RAIN_THUNDERSTORM: *out = WEATHER_RAIN_THUNDERSTORM; break;
    case COORD_EVENT_WEATHER_FOG_HORIZONTAL: *out = WEATHER_FOG_HORIZONTAL; break;
    case COORD_EVENT_WEATHER_FOG_DIAGONAL: *out = WEATHER_FOG_DIAGONAL; break;
    case COORD_EVENT_WEATHER_VOLCANIC_ASH: *out = WEATHER_VOLCANIC_ASH; break;
    case COORD_EVENT_WEATHER_SANDSTORM: *out = WEATHER_SANDSTORM; break;
    case COORD_EVENT_WEATHER_SHADE: *out = WEATHER_SHADE; break;
    case COORD_EVENT_WEATHER_DROUGHT: *out = WEATHER_DROUGHT; break;
    case COORD_EVENT_WEATHER_ROUTE119_CYCLE: *out = WEATHER_ROUTE119_CYCLE; break;
    case COORD_EVENT_WEATHER_ROUTE123_CYCLE: *out = WEATHER_ROUTE123_CYCLE; break;
    default: return -1;
    }
    return 0;
}
int RhhModsAuthoredCoordForHeader(const struct MapHeader *header, RhhContentKey map, u32 index,
                                 struct RhhAuthoredCoordEvent *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    u32 count;
    if (AuthoredCoords(header, &count) || index >= count) return -1;
    const struct CoordEvent *event = &header->events->coordEvents[index];
    struct RhhAuthoredCoordEvent result = {.struct_size = sizeof(result), .index = index, .map = map,
        .x = event->x, .y = event->y, .elevation = event->elevation,
        .trigger = event->trigger, .condition = event->index};
    if (!event->script)
    {
        result.kind = RHH_AUTHORED_COORD_WEATHER; result.weather_code = (u8)event->trigger;
        if (!AuthoredCoordWeather(result.weather_code, &result.engine_weather))
        {
            result.flags |= RHH_AUTHORED_COORD_WEATHER_KNOWN;
            result.natural_weather = NaturalWeather(result.engine_weather);
        }
    }
    else result.kind = event->trigger == TRIGGER_RUN_IMMEDIATELY
        ? RHH_AUTHORED_COORD_SCRIPT_IMMEDIATE : RHH_AUTHORED_COORD_SCRIPT_CONDITIONAL;
    *out = result; return 0;
}
static int AuthoredCoordAt(RhhContentKey key, u32 index, struct RhhAuthoredCoordEvent *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct MapHeader *header; RhhContentKey map;
    if (AuthoredMapResolve(key, &header, &map)) return -1;
    return RhhModsAuthoredCoordForHeader(header, map, index, out);
}
static int AuthoredBackgrounds(const struct MapHeader *header, u32 *count)
{
    if (!header || !count || !header->events
        || (header->events->bgEventCount && !header->events->bgEvents)) return -1;
    *count = header->events->bgEventCount; return 0;
}
static int AuthoredBackgroundCount(RhhContentKey key, u32 *out)
{
    if (!out) return -1;
    const struct MapHeader *header; u32 count;
    if (AuthoredMapResolve(key, &header, NULL) || AuthoredBackgrounds(header, &count)) return -1;
    *out = count; return 0;
}
int RhhModsAuthoredBackgroundForHeader(const struct MapHeader *header, RhhContentKey map, u32 index,
                                      struct RhhAuthoredBackgroundEvent *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    u32 count;
    if (AuthoredBackgrounds(header, &count) || index >= count) return -1;
    const struct BgEvent *event = &header->events->bgEvents[index];
    struct RhhAuthoredBackgroundEvent result = {.struct_size = sizeof(result), .index = index,
        .map = map, .kind = RHH_AUTHORED_BACKGROUND_UNKNOWN, .engine_kind = event->kind,
        .x = event->x, .y = event->y, .elevation = event->elevation};
    if (event->kind <= BG_EVENT_PLAYER_FACING_WEST)
    {
        result.kind = RHH_AUTHORED_BACKGROUND_SCRIPT;
        switch (event->kind)
        {
        case BG_EVENT_PLAYER_FACING_ANY: result.facing = RHH_AUTHORED_BACKGROUND_FACING_ANY; break;
        case BG_EVENT_PLAYER_FACING_NORTH: result.facing = RHH_AUTHORED_BACKGROUND_FACING_NORTH; break;
        case BG_EVENT_PLAYER_FACING_SOUTH: result.facing = RHH_AUTHORED_BACKGROUND_FACING_SOUTH; break;
        case BG_EVENT_PLAYER_FACING_EAST: result.facing = RHH_AUTHORED_BACKGROUND_FACING_EAST; break;
        case BG_EVENT_PLAYER_FACING_WEST: result.facing = RHH_AUTHORED_BACKGROUND_FACING_WEST; break;
        }
        if (event->bgUnion.script) result.flags |= RHH_AUTHORED_BACKGROUND_HAS_SCRIPT;
    }
    else if (event->kind == 5 || event->kind == 6 || event->kind == BG_EVENT_HIDDEN_ITEM)
    {
        result.kind = RHH_AUTHORED_BACKGROUND_HIDDEN_ITEM;
        result.item_id = event->bgUnion.hiddenItem.item;
        result.hidden_item_id = event->bgUnion.hiddenItem.hiddenItemId;
        result.quantity = event->bgUnion.hiddenItem.quantity;
        if (event->bgUnion.hiddenItem.underfoot) result.flags |= RHH_AUTHORED_BACKGROUND_UNDERFOOT;
        result.item = RhhContentKeyForId(RHH_ITEM, result.item_id);
    }
    else if (event->kind == BG_EVENT_SECRET_BASE)
    {
        result.kind = RHH_AUTHORED_BACKGROUND_SECRET_BASE;
        result.secret_base_id = event->bgUnion.secretBaseId;
    }
    *out = result; return 0;
}
static int AuthoredBackgroundAt(RhhContentKey key, u32 index, struct RhhAuthoredBackgroundEvent *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct MapHeader *header; RhhContentKey map;
    if (AuthoredMapResolve(key, &header, &map)) return -1;
    return RhhModsAuthoredBackgroundForHeader(header, map, index, out);
}
static int AuthoredEventFlag(RhhContentKey map, u32 index, u32 flag, struct RhhAuthoredEventFlag *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || !gSaveBlock1Ptr) return -1;
    u32 scope;
    if (!flag) scope = RHH_AUTHORED_FLAG_NONE;
    else if (flag <= TEMP_FLAGS_END) scope = RHH_AUTHORED_FLAG_TEMPORARY;
    else if (flag < FLAGS_COUNT) scope = RHH_AUTHORED_FLAG_SAVED;
    else if (flag >= SPECIAL_FLAGS_START && flag <= SPECIAL_FLAGS_END) scope = RHH_AUTHORED_FLAG_SPECIAL;
    else return -1;
    struct RhhAuthoredEventFlag result = {.struct_size = sizeof(result), .index = index, .map = map,
        .run_id = RhhModsRunId(), .flag_id = flag, .scope = scope, .value = flag ? FlagGet(flag) : 0};
    *out = result; return 0;
}
static int AuthoredObjectHideFlag(RhhContentKey key, u32 index, struct RhhAuthoredEventFlag *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct MapHeader *header; RhhContentKey map; u32 count;
    if (AuthoredMapResolve(key, &header, &map) || AuthoredObjects(header, &count) || index >= count) return -1;
    const struct ObjectEventTemplate *object = &header->events->objectEvents[index];
    if (object->kind != OBJ_KIND_NORMAL && object->kind != OBJ_KIND_CLONE) return -1;
    return AuthoredEventFlag(map, index, object->flagId, out);
}
static int AuthoredHiddenItemFlag(RhhContentKey key, u32 index, struct RhhAuthoredEventFlag *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct MapHeader *header; RhhContentKey map; u32 count;
    if (AuthoredMapResolve(key, &header, &map) || AuthoredBackgrounds(header, &count) || index >= count) return -1;
    const struct BgEvent *event = &header->events->bgEvents[index];
    if (event->kind != 5 && event->kind != 6 && event->kind != BG_EVENT_HIDDEN_ITEM) return -1;
    u32 flag = FLAG_HIDDEN_ITEMS_START + event->bgUnion.hiddenItem.hiddenItemId;
    if (flag >= FLAGS_COUNT) return -1;
    return AuthoredEventFlag(map, index, flag, out);
}
static int AuthoredDynamicWarp(struct RhhDynamicWarp *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsFinalized()
        || !RhhModsHasActiveRun() || !gSaveBlock1Ptr) return -1;
    const struct WarpData *warp = &gSaveBlock1Ptr->dynamicWarp;
    struct RhhDynamicWarp result = {.struct_size = sizeof(result), .run_id = RhhModsRunId(),
        .map_group = warp->mapGroup, .map_number = warp->mapNum, .warp_id = warp->warpId,
        .saved_x = warp->x, .saved_y = warp->y};
    if (warp->mapGroup >= 0 && warp->mapNum >= 0)
    {
        RhhContentKey key = RhhContentKeyForId(RHH_MAP, (u32)warp->mapGroup << 8 | (u8)warp->mapNum);
        const struct MapHeader *header; u32 count;
        if (key && !AuthoredMapResolve(key, &header, &result.map) && !AuthoredWarps(header, &count))
        {
            result.flags |= RHH_DYNAMIC_WARP_MAP_AVAILABLE;
            if (warp->warpId >= 0 && (u32)warp->warpId < count)
            {
                const struct WarpEvent *event = &header->events->warps[warp->warpId];
                result.flags |= RHH_DYNAMIC_WARP_ARRIVAL_EVENT;
                result.arrival_x = event->x; result.arrival_y = event->y;
            }
            else if (warp->x >= 0 && warp->y >= 0)
            {
                result.flags |= RHH_DYNAMIC_WARP_ARRIVAL_COORDS;
                result.arrival_x = warp->x; result.arrival_y = warp->y;
            }
            else
            {
                result.flags |= RHH_DYNAMIC_WARP_ARRIVAL_CENTER;
                result.arrival_x = header->mapLayout->width / 2; result.arrival_y = header->mapLayout->height / 2;
            }
            if (result.arrival_x >= 0 && result.arrival_y >= 0 && result.arrival_x < header->mapLayout->width
                && result.arrival_y < header->mapLayout->height) result.flags |= RHH_DYNAMIC_WARP_ARRIVAL_IN_BOUNDS;
        }
        else result.map = 0;
    }
    *out = result; return 0;
}
static const struct RhhAuthoredEventStateService sAuthoredEventStateService = {
    {sizeof(sAuthoredEventStateService), RHH_SERVICE_VERSION(1, 1)}, AuthoredObjectHideFlag, AuthoredHiddenItemFlag,
    AuthoredDynamicWarp
};
static int ObjectImageSource(const struct ObjectEventGraphicsInfo *info, u32 frame, struct RhhAuthoredObjectImage *out,
                             const u8 **pixels, u16 palette[16], u32 *frames)
{
    if (!info || info->compressed || info->tileTag != TAG_NONE || !info->images || !info->images[0].data
        || info->width <= 0 || info->height <= 0 || info->width > 64 || info->height > 64
        || (info->width & 7) || (info->height & 7)
        || !RhhModsCopyObjectPalette(info->paletteTag, palette)) return -1;
    u32 frameCount = RhhModsAuthoredFrameCount(info->images);
    if (frame && (!frameCount || frame >= frameCount)) return -1;
    const struct SpriteFrameImage *image = info->images[0].relativeFrames ? &info->images[0] : &info->images[frame];
    if (!image->data || image->size < (u32)info->width * info->height / 2) return -1;
    *out = (struct RhhAuthoredObjectImage){.struct_size = sizeof(*out), .width = info->width, .height = info->height};
    *pixels = (const u8 *)image->data + (info->images[0].relativeFrames ? (size_t)frame * image->size : 0);
    if (frames) *frames = frameCount;
    return 0;
}
static int FixedObjectImageSource(u32 id, u32 frame, struct RhhAuthoredObjectImage *out,
                                  const u8 **pixels, u16 palette[16], u32 *frames,
                                  const struct ObjectEventGraphicsInfo **graphics)
{
    if (!RhhModsFinalized() || id >= NUM_OBJ_EVENT_GFX || (id & OBJ_EVENT_MON)
        || id == OBJ_EVENT_GFX_BARD || id == OBJ_EVENT_GFX_BERRY_TREE
        || id == OBJ_EVENT_GFX_BERRY_TREE_EARLY_STAGES || id == OBJ_EVENT_GFX_BERRY_TREE_LATE_STAGES) return -1;
    const struct ObjectEventGraphicsInfo *info = GetObjectEventGraphicsInfo(id);
    if (ObjectImageSource(info, frame, out, pixels, palette, frames)) return -1;
    if (graphics) *graphics = info;
    return 0;
}
static int AuthoredObjectImageSource(RhhContentKey key, u32 index, u32 frame, struct RhhAuthoredObjectImage *out,
                                     const u8 **pixels, u16 palette[16], u32 *frames,
                                     const struct ObjectEventGraphicsInfo **graphics)
{
    const struct MapHeader *header; RhhContentKey map; u32 count;
    if (AuthoredMapResolve(key, &header, &map) || AuthoredObjects(header, &count) || index >= count) return -1;
    const struct ObjectEventTemplate *object = &header->events->objectEvents[index];
    if (object->kind != OBJ_KIND_NORMAL
        || FixedObjectImageSource(object->graphicsId, frame, out, pixels, palette, frames, graphics)) return -1;
    out->index = index; out->map = map; return 0;
}
static int AuthoredObjectImage(RhhContentKey key, u32 index, struct RhhAuthoredObjectImage *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhAuthoredObjectImage result; const u8 *pixels; u16 palette[16];
    if (AuthoredObjectImageSource(key, index, 0, &result, &pixels, palette, NULL, NULL)) return -1;
    *out = result; return 0;
}
static void DecodeObjectImage(const struct RhhAuthoredObjectImage *image, const u8 *pixels, const u16 palette[16], u8 *rgba)
{
    for (u32 y = 0; y < image->height; ++y) for (u32 x = 0; x < image->width; ++x)
    {
        u32 tile = (y / 8) * (image->width / 8) + x / 8;
        u32 bits = pixels[tile * 32 + (y % 8) * 4 + (x % 8) / 2];
        u32 colorIndex = (bits >> ((x & 1) * 4)) & 15, dst = (y * image->width + x) * 4;
        u16 color = palette[colorIndex];
        for (u32 c = 0; c < 3; ++c) { u32 v = (color >> (c * 5)) & 31; rgba[dst + c] = (v << 3) | (v >> 2); }
        rgba[dst + 3] = colorIndex ? 255 : 0;
    }
}
static int AuthoredObjectFrameRgba(RhhContentKey key, u32 index, u32 frame, u8 *rgba, u32 capacity)
{
    if (!rgba) return -1;
    struct RhhAuthoredObjectImage image; const u8 *pixels; u16 palette[16];
    if (AuthoredObjectImageSource(key, index, frame, &image, &pixels, palette, NULL, NULL)
        || capacity < image.width * image.height * 4) return -1;
    DecodeObjectImage(&image, pixels, palette, rgba);
    return 0;
}
static int AuthoredObjectImageRgba(RhhContentKey key, u32 index, u8 *rgba, u32 capacity)
{
    return AuthoredObjectFrameRgba(key, index, 0, rgba, capacity);
}
static int AuthoredObjectFrameCount(RhhContentKey key, u32 index, u32 *out)
{
    if (!out) return -1;
    struct RhhAuthoredObjectImage image; const u8 *pixels; u16 palette[16]; u32 count;
    if (AuthoredObjectImageSource(key, index, 0, &image, &pixels, palette, &count, NULL) || !count) return -1;
    *out = count; return 0;
}
static int AuthoredObjectAnims(RhhContentKey key, u32 index, struct RhhAuthoredObjectImage *image,
                               const struct ObjectEventGraphicsInfo **graphics, u32 *frameCount, u32 *animCount)
{
    const u8 *pixels; u16 palette[16];
    if (AuthoredObjectImageSource(key, index, 0, image, &pixels, palette, frameCount, graphics)) return -1;
    *animCount = RhhModsAuthoredAnimCount((*graphics)->anims);
    return *animCount ? 0 : -1;
}
static int AuthoredObjectAnimationCount(RhhContentKey key, u32 index, u32 *out)
{
    if (!out) return -1;
    struct RhhAuthoredObjectImage image; const struct ObjectEventGraphicsInfo *info; u32 frames, count;
    if (AuthoredObjectAnims(key, index, &image, &info, &frames, &count)) return -1;
    *out = count; return 0;
}
static int AuthoredObjectAnimationScript(RhhContentKey key, u32 index, u32 animation,
                                         struct RhhAuthoredObjectImage *image, const union AnimCmd **commands,
                                         u32 *frames, u32 *count)
{
    const struct ObjectEventGraphicsInfo *info; u32 anims;
    if (AuthoredObjectAnims(key, index, image, &info, frames, &anims) || animation >= anims) return -1;
    const union AnimCmd *script = info->anims[animation];
    u32 size = RhhModsAuthoredAnimCommandCount(script);
    if (!size) return -1;
    *commands = script; *count = size; return 0;
}
static int AuthoredObjectAnimationCommandCount(RhhContentKey key, u32 index, u32 animation, u32 *out)
{
    if (!out) return -1;
    struct RhhAuthoredObjectImage image; const union AnimCmd *commands; u32 frames, count;
    if (AuthoredObjectAnimationScript(key, index, animation, &image, &commands, &frames, &count)) return -1;
    *out = count; return 0;
}
static void CopySourceAnimationCommand(const union AnimCmd *source, u32 frames, u32 count,
                                       struct RhhAuthoredAnimCommand *result)
{
    result->kind = RHH_AUTHORED_ANIM_UNKNOWN;
    if (source->type >= 0)
    {
        result->kind = RHH_AUTHORED_ANIM_FRAME; result->image = source->frame.imageValue;
        result->duration = source->frame.duration;
        if (source->frame.hFlip) result->flags |= RHH_AUTHORED_ANIM_HFLIP;
        if (source->frame.vFlip) result->flags |= RHH_AUTHORED_ANIM_VFLIP;
        if (frames && result->image < frames) result->flags |= RHH_AUTHORED_ANIM_FRAME_AVAILABLE;
    }
    else if (source->type == -1) result->kind = RHH_AUTHORED_ANIM_END;
    else if (source->type == -2)
    {
        result->kind = RHH_AUTHORED_ANIM_JUMP; result->argument = source->jump.target;
        if (result->argument < count) result->flags |= RHH_AUTHORED_ANIM_JUMP_IN_BOUNDS;
    }
    else if (source->type == -3) { result->kind = RHH_AUTHORED_ANIM_LOOP; result->argument = source->loop.count; }
}
static int AuthoredObjectAnimationCommand(RhhContentKey key, u32 index, u32 animation, u32 command,
                                          struct RhhAuthoredAnimCommand *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhAuthoredObjectImage image; const union AnimCmd *commands; u32 frames, count;
    if (AuthoredObjectAnimationScript(key, index, animation, &image, &commands, &frames, &count) || command >= count) return -1;
    struct RhhAuthoredAnimCommand result = {.struct_size = sizeof(result), .object_index = index,
        .animation = animation, .command = command, .map = image.map};
    CopySourceAnimationCommand(&commands[command], frames, count, &result);
    *out = result; return 0;
}
static int AuthoredObjectFaceAnimation(RhhContentKey key, u32 index, u32 direction, u32 *out)
{
    if (!out || direction < RHH_WILD_ACTOR_SOUTH || direction > RHH_WILD_ACTOR_NORTHEAST) return -1;
    struct RhhAuthoredObjectImage image; const struct ObjectEventGraphicsInfo *info; u32 frames, count;
    if (AuthoredObjectAnims(key, index, &image, &info, &frames, &count) || info->inanimate) return -1;
    u32 animation = GetFaceDirectionAnimNum(direction);
    if (animation >= count || !RhhModsAuthoredAnimCommandCount(info->anims[animation])) return -1;
    *out = animation; return 0;
}
static int ObjectVisualResolve(u32 id, struct RhhObjectVisualResolution *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsFinalized() || id > UINT16_MAX) return -1;
    struct RhhObjectVisualResolution result = {.struct_size = sizeof(result), .input_id = id,
        .resolved_id = id, .variable_slot = UINT32_MAX};
    if (id >= OBJ_EVENT_GFX_VARS && id <= OBJ_EVENT_GFX_VAR_F)
    {
        if (!gSaveBlock1Ptr) return -1;
        result.flags |= RHH_OBJECT_VISUAL_VARIABLE; result.variable_slot = id - OBJ_EVENT_GFX_VARS;
        result.resolved_id = VarGetObjectEventGraphicsId(result.variable_slot);
    }
    u32 resolved = result.resolved_id;
    if (resolved & OBJ_EVENT_MON)
    {
        u32 species = resolved & OBJ_EVENT_MON_SPECIES_MASK;
        result.flags |= RHH_OBJECT_VISUAL_POKEMON;
        if (resolved & OBJ_EVENT_MON_SHINY) result.flags |= RHH_OBJECT_VISUAL_SHINY;
        if (resolved & OBJ_EVENT_MON_FEMALE) result.flags |= RHH_OBJECT_VISUAL_FEMALE;
        if (species && species < NUM_SPECIES) result.species = RhhContentKeyForId(RHH_SPECIES, species);
    }
    else if (resolved >= NUM_OBJ_EVENT_GFX)
    {
        result.flags |= RHH_OBJECT_VISUAL_FALLBACK; result.resolved_id = OBJ_EVENT_GFX_NINJA_BOY;
    }
    else if (resolved == OBJ_EVENT_GFX_BARD || resolved == OBJ_EVENT_GFX_BERRY_TREE
             || resolved == OBJ_EVENT_GFX_BERRY_TREE_EARLY_STAGES || resolved == OBJ_EVENT_GFX_BERRY_TREE_LATE_STAGES)
        result.flags |= RHH_OBJECT_VISUAL_DYNAMIC;
    *out = result; return 0;
}
static int ObjectVisualInfo(u32 id, struct RhhObjectVisual *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhAuthoredObjectImage image; const u8 *pixels; const struct ObjectEventGraphicsInfo *info;
    u16 palette[16]; u32 frames;
    if (FixedObjectImageSource(id, 0, &image, &pixels, palette, &frames, &info)) return -1;
    struct RhhObjectVisual result = {.struct_size = sizeof(result), .graphics_id = id,
        .flags = info->inanimate ? RHH_OBJECT_VISUAL_INANIMATE : 0, .width = image.width, .height = image.height,
        .frame_count = frames, .animation_count = RhhModsAuthoredAnimCount(info->anims)};
    *out = result; return 0;
}
static int ObjectVisualFrameRgba(u32 id, u32 frame, u8 *rgba, u32 capacity)
{
    if (!rgba) return -1;
    struct RhhAuthoredObjectImage image; const u8 *pixels; u16 palette[16];
    if (FixedObjectImageSource(id, frame, &image, &pixels, palette, NULL, NULL)
        || capacity < image.width * image.height * 4) return -1;
    DecodeObjectImage(&image, pixels, palette, rgba); return 0;
}
static int ObjectVisualAnimation(u32 id, u32 animation, const union AnimCmd **commands, u32 *frames, u32 *count)
{
    struct RhhAuthoredObjectImage image; const u8 *pixels; const struct ObjectEventGraphicsInfo *info; u16 palette[16];
    if (FixedObjectImageSource(id, 0, &image, &pixels, palette, frames, &info)
        || animation >= RhhModsAuthoredAnimCount(info->anims)) return -1;
    const union AnimCmd *script = info->anims[animation];
    u32 size = RhhModsAuthoredAnimCommandCount(script);
    if (!size) return -1;
    *commands = script; *count = size; return 0;
}
static int ObjectVisualAnimationCount(u32 id, u32 animation, u32 *out)
{
    if (!out) return -1;
    const union AnimCmd *commands; u32 frames, count;
    if (ObjectVisualAnimation(id, animation, &commands, &frames, &count)) return -1;
    *out = count; return 0;
}
static int ObjectVisualAnimationCommand(u32 id, u32 animation, u32 command, struct RhhObjectVisualAnimCommand *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const union AnimCmd *commands; u32 frames, count;
    if (ObjectVisualAnimation(id, animation, &commands, &frames, &count) || command >= count) return -1;
    struct RhhAuthoredAnimCommand decoded = {0}; CopySourceAnimationCommand(&commands[command], frames, count, &decoded);
    struct RhhObjectVisualAnimCommand result = {.struct_size = sizeof(result), .graphics_id = id,
        .animation = animation, .command = command, .kind = decoded.kind, .flags = decoded.flags,
        .image = decoded.image, .duration = decoded.duration, .argument = decoded.argument};
    *out = result; return 0;
}
static int ObjectVisualFaceAnimation(u32 id, u32 direction, u32 *out)
{
    if (!out || direction < RHH_WILD_ACTOR_SOUTH || direction > RHH_WILD_ACTOR_NORTHEAST) return -1;
    struct RhhAuthoredObjectImage image; const u8 *pixels; const struct ObjectEventGraphicsInfo *info; u16 palette[16];
    u32 frames;
    if (FixedObjectImageSource(id, 0, &image, &pixels, palette, &frames, &info) || info->inanimate) return -1;
    u32 animation = GetFaceDirectionAnimNum(direction), count = RhhModsAuthoredAnimCount(info->anims);
    if (animation >= count || !RhhModsAuthoredAnimCommandCount(info->anims[animation])) return -1;
    *out = animation; return 0;
}
_Static_assert(RHH_BERRY_PLANTED == BERRY_STAGE_PLANTED && RHH_BERRY_SPROUTED == BERRY_STAGE_SPROUTED
    && RHH_BERRY_TALLER == BERRY_STAGE_TALLER && RHH_BERRY_FLOWERING == BERRY_STAGE_FLOWERING
    && RHH_BERRY_FRUIT == BERRY_STAGE_BERRIES && RHH_BERRY_TRUNK == BERRY_STAGE_TRUNK
    && RHH_BERRY_BUDDING == BERRY_STAGE_BUDDING, "public berry stages must match engine stages");
struct BerryVisualSource {
    struct RhhBerryVisual visual;
    const struct SpriteFrameImage *frames;
    const union AnimCmd *commands;
    u16 palette[16];
};
static int BerryVisualSource(const struct RhhBerryVisualRequest *request, struct BerryVisualSource *source)
{
    u32 item;
    if (!request || request->struct_size < sizeof(*request) || !source || !RhhModsFinalized()
        || request->stage < BERRY_STAGE_PLANTED || request->stage > BERRY_STAGE_BUDDING
        || RhhContentResolve(RHH_ITEM, request->berry_item, &item)
        || RhhContentKeyForId(RHH_ITEM, item) != request->berry_item) return -1;
    u32 berry = ItemIdToBerryType(item), id = RhhModsBerryObjectGraphicsId(request->stage);
    if (!berry || berry > NUM_BERRIES || id == UINT32_MAX) return -1;
    const struct Berry *definition = &gBerries[berry];
    if (!definition->berryTreePicTable || !definition->berryTreePaletteSlotTable) return -1;
    u32 frames = RhhModsBerryFrameCount(definition->berryTreePicTable);
    const struct ObjectEventGraphicsInfo *info = GetObjectEventGraphicsInfo(id);
    u32 animation = request->stage - 1;
    if (!frames || !info || !info->anims || animation >= RhhModsAuthoredAnimCount(info->anims)
        || info->width <= 0 || info->height <= 0 || info->width > 64 || info->height > 64
        || (info->width & 7) || (info->height & 7)
        || !RhhModsCopyBerryObjectPalette(definition->berryTreePaletteSlotTable[animation], source->palette)) return -1;
    const union AnimCmd *commands = info->anims[animation];
    u32 count = RhhModsAuthoredAnimCommandCount(commands);
    if (!count) return -1;
    source->visual = (struct RhhBerryVisual){.struct_size = sizeof(source->visual), .stage = request->stage,
        .berry_item = request->berry_item, .width = info->width, .height = info->height,
        .frame_count = frames, .command_count = count};
    source->frames = definition->berryTreePicTable; source->commands = commands; return 0;
}
static int BerryVisualInfo(const struct RhhBerryVisualRequest *request, struct RhhBerryVisual *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct BerryVisualSource source;
    if (BerryVisualSource(request, &source)) return -1;
    *out = source.visual; return 0;
}
static int BerryVisualFrameRgba(const struct RhhBerryVisualRequest *request, u32 frame, u8 *rgba, u32 capacity)
{
    if (!rgba) return -1;
    struct BerryVisualSource source;
    if (BerryVisualSource(request, &source) || frame >= source.visual.frame_count
        || capacity < source.visual.width * source.visual.height * 4) return -1;
    const struct SpriteFrameImage *image = &source.frames[frame];
    if (!image->data || image->relativeFrames || image->size != source.visual.width * source.visual.height / 2) return -1;
    struct RhhAuthoredObjectImage dimensions = {.width = source.visual.width, .height = source.visual.height};
    DecodeObjectImage(&dimensions, image->data, source.palette, rgba); return 0;
}
static int BerryVisualCommand(const struct RhhBerryVisualRequest *request, u32 command, struct RhhBerryVisualCommand *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct BerryVisualSource source;
    if (BerryVisualSource(request, &source) || command >= source.visual.command_count) return -1;
    struct RhhAuthoredAnimCommand decoded = {0};
    CopySourceAnimationCommand(&source.commands[command], source.visual.frame_count, source.visual.command_count, &decoded);
    struct RhhBerryVisualCommand result = {.struct_size = sizeof(result), .stage = request->stage,
        .berry_item = request->berry_item, .command = command, .kind = decoded.kind, .flags = decoded.flags,
        .image = decoded.image, .duration = decoded.duration, .argument = decoded.argument};
    *out = result; return 0;
}
extern const struct ObjectEventGraphicsInfo *const gMauvilleOldManGraphicsInfoPointers[];
static const struct ObjectEventGraphicsInfo *ObjectVariantGraphics(const struct RhhObjectVisualVariant *variant)
{
    if (!RhhModsFinalized() || !variant || variant->struct_size < sizeof(*variant) || variant->reserved
        || variant->kind != RHH_OBJECT_VARIANT_MAUVILLE_OLD_MAN || variant->variant > MAUVILLE_MAN_UNUSED2) return NULL;
    return gMauvilleOldManGraphicsInfoPointers[variant->variant];
}
static int ObjectVariantCurrent(u32 id, struct RhhObjectVisualVariant *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsFinalized() || !gSaveBlock1Ptr
        || id != OBJ_EVENT_GFX_BARD || gSaveBlock1Ptr->oldMan.common.id > MAUVILLE_MAN_UNUSED2) return -1;
    struct RhhObjectVisualVariant result = {.struct_size = sizeof(result),
        .kind = RHH_OBJECT_VARIANT_MAUVILLE_OLD_MAN, .variant = gSaveBlock1Ptr->oldMan.common.id};
    *out = result; return 0;
}
static int ObjectVariantInfo(const struct RhhObjectVisualVariant *variant, struct RhhObjectVisual *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct ObjectEventGraphicsInfo *info = ObjectVariantGraphics(variant);
    struct RhhAuthoredObjectImage image; const u8 *pixels; u16 palette[16]; u32 frames;
    if (ObjectImageSource(info, 0, &image, &pixels, palette, &frames)) return -1;
    struct RhhObjectVisual result = {.struct_size = sizeof(result), .graphics_id = OBJ_EVENT_GFX_BARD,
        .flags = info->inanimate ? RHH_OBJECT_VISUAL_INANIMATE : 0, .width = image.width, .height = image.height,
        .frame_count = frames, .animation_count = RhhModsAuthoredAnimCount(info->anims)};
    *out = result; return 0;
}
static int ObjectVariantFrame(const struct RhhObjectVisualVariant *variant, u32 frame, u8 *rgba, u32 capacity)
{
    if (!rgba) return -1;
    struct RhhAuthoredObjectImage image; const u8 *pixels; u16 palette[16];
    if (ObjectImageSource(ObjectVariantGraphics(variant), frame, &image, &pixels, palette, NULL)
        || capacity < image.width * image.height * 4) return -1;
    DecodeObjectImage(&image, pixels, palette, rgba); return 0;
}
static int ObjectVariantAnimation(const struct RhhObjectVisualVariant *variant, u32 animation,
                                  const union AnimCmd **commands, u32 *frames, u32 *count)
{
    const struct ObjectEventGraphicsInfo *info = ObjectVariantGraphics(variant);
    struct RhhAuthoredObjectImage image; const u8 *pixels; u16 palette[16];
    if (ObjectImageSource(info, 0, &image, &pixels, palette, frames)
        || animation >= RhhModsAuthoredAnimCount(info->anims)) return -1;
    const union AnimCmd *script = info->anims[animation]; u32 size = RhhModsAuthoredAnimCommandCount(script);
    if (!size) return -1;
    *commands = script; *count = size; return 0;
}
static int ObjectVariantAnimationCount(const struct RhhObjectVisualVariant *variant, u32 animation, u32 *out)
{
    if (!out) return -1;
    const union AnimCmd *commands; u32 frames, count;
    if (ObjectVariantAnimation(variant, animation, &commands, &frames, &count)) return -1;
    *out = count; return 0;
}
static int ObjectVariantAnimationCommand(const struct RhhObjectVisualVariant *variant, u32 animation, u32 command,
                                         struct RhhObjectVisualAnimCommand *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const union AnimCmd *commands; u32 frames, count;
    if (ObjectVariantAnimation(variant, animation, &commands, &frames, &count) || command >= count) return -1;
    struct RhhAuthoredAnimCommand decoded = {0}; CopySourceAnimationCommand(&commands[command], frames, count, &decoded);
    struct RhhObjectVisualAnimCommand result = {.struct_size = sizeof(result), .graphics_id = OBJ_EVENT_GFX_BARD,
        .animation = animation, .command = command, .kind = decoded.kind, .flags = decoded.flags,
        .image = decoded.image, .duration = decoded.duration, .argument = decoded.argument};
    *out = result; return 0;
}
static int ObjectVariantFaceAnimation(const struct RhhObjectVisualVariant *variant, u32 direction, u32 *out)
{
    if (!out || direction < RHH_WILD_ACTOR_SOUTH || direction > RHH_WILD_ACTOR_NORTHEAST) return -1;
    const struct ObjectEventGraphicsInfo *info = ObjectVariantGraphics(variant);
    struct RhhAuthoredObjectImage image; const u8 *pixels; u16 palette[16];
    if (ObjectImageSource(info, 0, &image, &pixels, palette, NULL) || info->inanimate) return -1;
    u32 animation = GetFaceDirectionAnimNum(direction);
    if (animation >= RhhModsAuthoredAnimCount(info->anims) || !RhhModsAuthoredAnimCommandCount(info->anims[animation])) return -1;
    *out = animation; return 0;
}
static const struct RhhObjectVisualService sObjectVisualService = {
    {sizeof(sObjectVisualService), RHH_SERVICE_VERSION(1, 2)}, ObjectVisualResolve, ObjectVisualInfo,
    ObjectVisualFrameRgba, ObjectVisualAnimationCount, ObjectVisualAnimationCommand, ObjectVisualFaceAnimation,
    BerryVisualInfo, BerryVisualFrameRgba, BerryVisualCommand, ObjectVariantCurrent, ObjectVariantInfo,
    ObjectVariantFrame, ObjectVariantAnimationCount, ObjectVariantAnimationCommand, ObjectVariantFaceAnimation
};
static const struct RhhAuthoredMapService sAuthoredMapService = {
    {sizeof(sAuthoredMapService), RHH_SERVICE_VERSION(1, 11)}, AuthoredMap, AuthoredMapTile, AuthoredTileRgba, AuthoredTilesRgba,
    AuthoredConnectionCount, AuthoredConnectionAt, AuthoredWarpCount, AuthoredWarpAt, AuthoredObjectCount, AuthoredObjectAt,
    AuthoredCoordCount, AuthoredCoordAt, AuthoredObjectImage, AuthoredObjectImageRgba, AuthoredObjectFrameCount, AuthoredObjectFrameRgba,
    AuthoredObjectAnimationCount, AuthoredObjectAnimationCommandCount, AuthoredObjectAnimationCommand, AuthoredObjectFaceAnimation,
    AuthoredBackgroundCount, AuthoredBackgroundAt, AuthoredMapScan
};
struct SpeciesVisualSource {
    struct RhhSpeciesVisual visual;
    const u32 *compressed;
    const struct ObjectEventGraphicsInfo *graphics;
    u32 decoded_size, frame_size;
    u16 palette[16];
};
static int SpeciesVisualSource(const struct RhhSpeciesVisualRequest *request, struct SpeciesVisualSource *source)
{
    u32 species;
    if (!request || request->struct_size < sizeof(*request)
        || (request->flags & ~(RHH_SPECIES_VISUAL_SHINY | RHH_SPECIES_VISUAL_FEMALE)) || !RhhModsFinalized()
        || RhhContentResolve(RHH_SPECIES, request->species, &species) || species == SPECIES_NONE || species >= NUM_SPECIES)
        return -1;
    bool32 shiny = !!(request->flags & RHH_SPECIES_VISUAL_SHINY), female = !!(request->flags & RHH_SPECIES_VISUAL_FEMALE);
    const struct ObjectEventGraphicsInfo *info = SpeciesToGraphicsInfo(species, shiny, female);
    if (!info || !info->compressed || info->tileTag != TAG_NONE || !info->images || !info->images[0].data
        || !info->images[0].relativeFrames || info->width <= 0 || info->height <= 0
        || info->width > 64 || info->height > 64 || (info->width & 7) || (info->height & 7)) return -1;
    u32 frameSize = (u32)info->width * info->height / 2;
    if (info->images[0].size != frameSize) return -1;
    const u32 *compressed = info->images[0].data;
    if (!IsCompressedData(compressed)) return -1;
    u32 size = GetDecompressedDataSize(compressed);
    if (!size || size > MAX_DECOMPRESSION_BUFFER_SIZE || size % frameSize) return -1;
    u32 flags = request->flags;
#if OW_POKEMON_OBJECT_EVENTS
    if (info == &gSpeciesInfo[SPECIES_NONE].overworldData) flags |= RHH_SPECIES_VISUAL_PLACEHOLDER;
#endif
    if (info->paletteTag != OBJ_EVENT_PAL_TAG_DYNAMIC)
    {
        if (!RhhModsCopyObjectPalette(info->paletteTag, source->palette)) return -1;
    }
    else
    {
        const u16 *palette = NULL;
#if OW_POKEMON_OBJECT_EVENTS == TRUE && OW_PKMN_OBJECTS_SHARE_PALETTES == FALSE
        /* Match the engine's follower palette selection, including its
         * existing fallback condition and female palette rules. */
        if ((shiny && gSpeciesInfo[species].overworldPalette)
            || (!shiny && gSpeciesInfo[species].overworldShinyPalette))
        {
#if P_GENDER_DIFFERENCES
            if (female && gSpeciesInfo[species].overworldPaletteFemale)
                palette = shiny ? gSpeciesInfo[species].overworldShinyPaletteFemale : gSpeciesInfo[species].overworldPaletteFemale;
            else
#endif
                palette = shiny ? gSpeciesInfo[species].overworldShinyPalette : gSpeciesInfo[species].overworldPalette;
        }
        else
#endif
            palette = GetMonSpritePalFromSpecies(species, shiny, female);
        if (!palette) return -1;
        memcpy(source->palette, palette, sizeof(source->palette));
    }
    source->visual = (struct RhhSpeciesVisual){.struct_size = sizeof(source->visual), .species = RhhContentKeyForId(RHH_SPECIES, species),
        .flags = flags, .width = info->width, .height = info->height, .frame_count = size / frameSize};
    source->compressed = compressed; source->graphics = info; source->decoded_size = size; source->frame_size = frameSize;
    return source->visual.species ? 0 : -1;
}
static int SpeciesVisualInfo(const struct RhhSpeciesVisualRequest *request, struct RhhSpeciesVisual *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct SpeciesVisualSource source;
    if (SpeciesVisualSource(request, &source)) return -1;
    *out = source.visual; return 0;
}
static void SpeciesVisualDrawFrame(const struct SpeciesVisualSource *source, const u8 *pixels, u8 *rgba)
{
    for (u32 y = 0; y < source->visual.height; ++y) for (u32 x = 0; x < source->visual.width; ++x)
    {
        u32 tile = (y / 8) * (source->visual.width / 8) + x / 8;
        u32 bits = pixels[tile * 32 + (y % 8) * 4 + (x % 8) / 2];
        u32 colorIndex = (bits >> ((x & 1) * 4)) & 15, dst = (y * source->visual.width + x) * 4;
        u16 color = source->palette[colorIndex];
        for (u32 c = 0; c < 3; ++c) { u32 v = (color >> (c * 5)) & 31; rgba[dst + c] = (v << 3) | (v >> 2); }
        rgba[dst + 3] = colorIndex ? 255 : 0;
    }
}
static int SpeciesVisualFrames(const struct RhhSpeciesVisualRequest *request, const u32 *frames, u32 count,
                               u8 *rgba, u32 stride, u32 capacity)
{
    if (count > RHH_SPECIES_VISUAL_BATCH_MAX || (count && (!frames || !rgba))) return -1;
    struct SpeciesVisualSource source;
    if (SpeciesVisualSource(request, &source)) return -1;
    if (!count) return 0;
    u32 imageSize = source.visual.width * source.visual.height * 4;
    if (stride < imageSize || (u64)(count - 1) * stride + imageSize > capacity) return -1;
    /* Keep frame indices stable even when the input aliases the output. */
    u32 selected[RHH_SPECIES_VISUAL_BATCH_MAX];
    memcpy(selected, frames, count * sizeof(*selected));
    for (u32 i = 0; i < count; ++i) if (selected[i] >= source.visual.frame_count) return -1;
    u8 *decoded = malloc(source.decoded_size);
    if (!decoded) return -1;
    if (!TryDecompressDataWithHeaderWram(source.compressed, decoded)) { free(decoded); return -1; }
    for (u32 i = 0; i < count; ++i)
        SpeciesVisualDrawFrame(&source, decoded + selected[i] * source.frame_size, rgba + (size_t)i * stride);
    free(decoded); return 0;
}
static int SpeciesVisualFrame(const struct RhhSpeciesVisualRequest *request, u32 frame, u8 *rgba, u32 capacity)
{
    return SpeciesVisualFrames(request, &frame, 1, rgba, 64u * 64u * 4u, capacity);
}
static int SpeciesVisualAnimationCount(const struct RhhSpeciesVisualRequest *request, u32 *out)
{
    if (!out) return -1;
    struct SpeciesVisualSource source;
    if (SpeciesVisualSource(request, &source)) return -1;
    u32 count = RhhModsAuthoredAnimCount(source.graphics->anims);
    if (!count) return -1;
    *out = count; return 0;
}
static int SpeciesVisualAnimationScript(const struct RhhSpeciesVisualRequest *request, u32 animation,
                                        struct SpeciesVisualSource *source, const union AnimCmd **commands, u32 *count)
{
    if (SpeciesVisualSource(request, source)) return -1;
    u32 animations = RhhModsAuthoredAnimCount(source->graphics->anims);
    if (!animations || animation >= animations) return -1;
    const union AnimCmd *script = source->graphics->anims[animation];
    u32 size = RhhModsAuthoredAnimCommandCount(script);
    if (!size) return -1;
    *commands = script; *count = size; return 0;
}
static int SpeciesVisualAnimationCommandCount(const struct RhhSpeciesVisualRequest *request, u32 animation, u32 *out)
{
    if (!out) return -1;
    struct SpeciesVisualSource source; const union AnimCmd *commands; u32 count;
    if (SpeciesVisualAnimationScript(request, animation, &source, &commands, &count)) return -1;
    *out = count; return 0;
}
static int SpeciesVisualAnimationCommand(const struct RhhSpeciesVisualRequest *request, u32 animation, u32 command,
                                         struct RhhSpeciesVisualAnimCommand *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct SpeciesVisualSource source; const union AnimCmd *commands; u32 count;
    if (SpeciesVisualAnimationScript(request, animation, &source, &commands, &count) || command >= count) return -1;
    struct RhhAuthoredAnimCommand decoded = {0};
    CopySourceAnimationCommand(&commands[command], source.visual.frame_count, count, &decoded);
    struct RhhSpeciesVisualAnimCommand result = {.struct_size = sizeof(result), .animation = animation, .command = command,
        .species = source.visual.species, .kind = decoded.kind, .flags = decoded.flags,
        .image = decoded.image, .duration = decoded.duration, .argument = decoded.argument};
    *out = result; return 0;
}
static int SpeciesVisualFaceAnimation(const struct RhhSpeciesVisualRequest *request, u32 direction, u32 *out)
{
    if (!out || direction < RHH_WILD_ACTOR_SOUTH || direction > RHH_WILD_ACTOR_NORTHEAST) return -1;
    struct SpeciesVisualSource source;
    if (SpeciesVisualSource(request, &source) || source.graphics->inanimate) return -1;
    u32 count = RhhModsAuthoredAnimCount(source.graphics->anims), animation = GetFaceDirectionAnimNum(direction);
    if (!count || animation >= count || !RhhModsAuthoredAnimCommandCount(source.graphics->anims[animation])) return -1;
    *out = animation; return 0;
}
static const struct RhhSpeciesVisualService sSpeciesVisualService = {
    {sizeof(sSpeciesVisualService), RHH_SERVICE_VERSION(1, 2)}, SpeciesVisualInfo, SpeciesVisualFrame,
    SpeciesVisualAnimationCount, SpeciesVisualAnimationCommandCount, SpeciesVisualAnimationCommand, SpeciesVisualFaceAnimation,
    SpeciesVisualFrames
};
static int HabitatSample(int32_t x, int32_t y, struct RhhHabitatSample *out)
{
    if (!out || out->struct_size < sizeof(*out) || sHabitatContext) return -1;
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)};
    struct RhhHabitatSample result = {.struct_size = sizeof(result), .x = x, .y = y, .permitted = 1};
    result.tile.struct_size = sizeof(result.tile);
    if (Current(&world) || !world.map || TileAt(x, y, &result.tile)) return -1;
    result.map = world.map; result.engine_weather = GetCurrentWeather();
    result.natural_weather = NaturalWeather(result.engine_weather);
    if (HabitatBaseFlags(result.tile.behavior, world.map_type, &result.base_flags)) return -1;
    const struct RhhWorldEnvironmentService *environment = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    result.environment.struct_size = sizeof(result.environment);
    result.environment_available = environment && !environment->current(&result.environment);
    if (!result.environment_available) memset(&result.environment, 0, sizeof(result.environment));
    result.flags = result.base_flags;
    if (RhhModsHasActiveRun())
    {
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_HABITAT_QUERY,
            .run_seed = RhhModsRunSeed(), .event_key = result.map,
            .ordinal = result.environment_available ? result.environment.step : 0, .args = {(u32)x, (u32)y}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = result.base_flags};
        rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
        struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
        sHabitatContext = &result; RhhModsDispatch(&event, &decision); sHabitatContext = NULL;
        RhhModsWorldTimeScopeEnd(&timeScope);
        bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
        gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
        result.permitted = pure && !!decision.allowed;
        result.flags = !result.permitted ? 0 : (decision.value & ~(u64)RHH_HABITAT_FLAGS_ALL) ? result.base_flags : decision.value;
    }
    *out = result; return 0;
}
static int HabitatSearch(const struct RhhHabitatSearchRequest *request, struct RhhHabitatSearchResult *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || request->reserved2
        || request->radius > RHH_HABITAT_SEARCH_MAX_RADIUS || (request->flags & ~RHH_HABITAT_SEARCH_INCLUDE_ORIGIN)
        || ((request->all_flags | request->any_flags | request->exclude_flags) & ~RHH_HABITAT_FLAGS_ALL)
        || !out || out->struct_size < sizeof(*out) || sHabitatContext) return -1;
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)};
    if (Current(&world) || !world.map || request->x < 0 || request->y < 0
        || (u32)request->x >= world.width || (u32)request->y >= world.height) return -1;
    struct RhhHabitatSearchResult result = {.struct_size = sizeof(result)};
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    for (u32 distance = (request->flags & RHH_HABITAT_SEARCH_INCLUDE_ORIGIN) ? 0 : 1;
         distance <= request->radius; ++distance)
    {
        for (s32 dy = -(s32)distance; dy <= (s32)distance; ++dy)
        {
            s32 xOffset = distance - (dy < 0 ? -dy : dy);
            for (s32 side = 0; side < (xOffset ? 2 : 1); ++side)
            {
                s64 x = (s64)request->x + (side ? xOffset : -xOffset), y = (s64)request->y + dy;
                if (x < 0 || y < 0 || x >= world.width || y >= world.height || x > INT32_MAX || y > INT32_MAX) continue;
                struct RhhHabitatSample sample = {.struct_size = sizeof(sample)};
                if (HabitatSample((s32)x, (s32)y, &sample)) { RhhModsWorldTimeScopeEnd(&timeScope); return -1; }
                ++result.scanned;
                if (!sample.permitted) { ++result.denied; continue; }
                if ((sample.flags & request->all_flags) != request->all_flags
                    || (request->any_flags && !(sample.flags & request->any_flags))
                    || (sample.flags & request->exclude_flags)) continue;
                result.found = TRUE; result.distance = distance; result.sample = sample;
                RhhModsWorldTimeScopeEnd(&timeScope); *out = result; return 0;
            }
        }
    }
    RhhModsWorldTimeScopeEnd(&timeScope); *out = result; return 0;
}
static int HabitatPlayer(struct RhhHabitatSample *out)
{
    if (!out || out->struct_size < sizeof(*out) || !Ready()
        || gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT) return -1;
    const struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];
    if (!player->active || !player->isPlayer
        || player->mapGroup != (u8)gSaveBlock1Ptr->location.mapGroup
        || player->mapNum != (u8)gSaveBlock1Ptr->location.mapNum) return -1;
    return HabitatSample(player->currentCoords.x - MAP_OFFSET, player->currentCoords.y - MAP_OFFSET, out);
}
static int HabitatNeighborhood(s32 x, s32 y, struct RhhHabitatNeighborhood *out)
{
    static const s8 offsets[RHH_HABITAT_NEIGHBOR_COUNT][2] = {
        {0, 0}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}
    };
    if (!out || out->struct_size < sizeof(*out) || sHabitatContext) return -1;
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)}, after = {.struct_size = sizeof(after)};
    if (Current(&world) || !world.map || x < 0 || y < 0 || (u32)x >= world.width || (u32)y >= world.height) return -1;
    struct RhhHabitatNeighborhood result = {.struct_size = sizeof(result)};
    u64 run = RhhModsRunId();
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    for (u32 i = 0; i < RHH_HABITAT_NEIGHBOR_COUNT; ++i)
    {
        s64 nx = (s64)x + offsets[i][0], ny = (s64)y + offsets[i][1];
        if (nx < 0 || ny < 0 || nx >= world.width || ny >= world.height || nx > INT32_MAX || ny > INT32_MAX) continue;
        struct RhhHabitatSample *sample = &result.samples[i]; sample->struct_size = sizeof(*sample);
        if (HabitatSample(nx, ny, sample)) { RhhModsWorldTimeScopeEnd(&timeScope); return -1; }
        result.present_mask |= 1u << i;
        if (sample->permitted) result.permitted_mask |= 1u << i;
        const struct RhhHabitatSample *center = &result.samples[RHH_HABITAT_CENTER];
        if (sample->map != world.map || sample->engine_weather != center->engine_weather
            || sample->environment_available != center->environment_available
            || (sample->environment_available && memcmp(&sample->environment, &center->environment, sizeof(sample->environment))))
        { RhhModsWorldTimeScopeEnd(&timeScope); return -1; }
    }
    const struct RhhHabitatSample *center = &result.samples[RHH_HABITAT_CENTER];
    const struct RhhWorldEnvironmentService *environment = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldEnvironmentSnapshot env = {.struct_size = sizeof(env)};
    bool32 envAvailable = environment && !environment->current(&env);
    if (Current(&after) || memcmp(&world, &after, sizeof(world)) || RhhModsRunId() != run
        || GetCurrentWeather() != center->engine_weather || envAvailable != center->environment_available
        || (envAvailable && memcmp(&env, &center->environment, sizeof(env))))
    { RhhModsWorldTimeScopeEnd(&timeScope); return -1; }
    for (u32 i = 0; i < RHH_HABITAT_NEIGHBOR_COUNT; ++i)
    {
        if (!(result.present_mask & (1u << i))) continue;
        struct RhhWorldTile tile = {.struct_size = sizeof(tile)};
        if (TileAt(result.samples[i].x, result.samples[i].y, &tile)
            || memcmp(&tile, &result.samples[i].tile, sizeof(tile)))
        { RhhModsWorldTimeScopeEnd(&timeScope); return -1; }
    }
    RhhModsWorldTimeScopeEnd(&timeScope); *out = result; return 0;
}
static int AuthoredHabitatSample(RhhContentKey key, s32 x, s32 y, struct RhhAuthoredHabitatSample *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhAuthoredHabitatSample result = {.struct_size = sizeof(result),
        .map.struct_size = sizeof(result.map), .sample.struct_size = sizeof(result.sample)};
    struct RhhAuthoredMapTile tile = {.struct_size = sizeof(tile)};
    if (AuthoredMap(key, &result.map) || AuthoredMapTile(key, x, y, &tile) || !tile.habitat_available) return -1;
    result.sample.map = result.map.map; result.sample.x = x; result.sample.y = y;
    result.sample.tile = tile.tile; result.sample.base_flags = result.sample.flags = tile.habitat_flags;
    result.sample.permitted = 1; result.sample.natural_weather = result.map.natural_weather;
    result.sample.engine_weather = result.map.engine_weather;
    const struct RhhWorldEnvironmentService *env = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldTimeScope scope; RhhModsWorldTimeScopeBegin(&scope);
    result.time_available = scope.available;
    if (result.time_available) result.time = scope.sample;
    result.sample.environment.struct_size = sizeof(result.sample.environment);
    result.sample.environment_available = env && !env->at(result.map.map, &result.sample.environment);
    if (!result.sample.environment_available) memset(&result.sample.environment, 0, sizeof(result.sample.environment));
    RhhModsWorldTimeScopeEnd(&scope); *out = result; return 0;
}
static const struct RhhHabitatService sHabitatService = {
    {sizeof(sHabitatService), RHH_SERVICE_VERSION(1, 7)}, HabitatSample, HabitatContext, HabitatSearch,
    RhhModsResolveHabitatWeather, HabitatPlayer, NaturalWeatherChangeContext, HabitatNeighborhood, AuthoredHabitatSample
};
static const struct RhhGroundEffectResult *sGroundEffectContext;
static int GroundEffectContext(struct RhhGroundEffectResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sGroundEffectContext
        || RhhModsCurrentEvent() != RHH_WORLD_GROUND_EFFECT) return -1;
    *out = *sGroundEffectContext; return 0;
}
static u32 GroundEffectMask(u32 phase)
{
    return phase == RHH_GROUND_BEGIN ? RHH_GROUND_SPLASH | RHH_GROUND_TRACKS | RHH_GROUND_DEEP_TRACKS
        : phase == RHH_GROUND_FINISH ? RHH_GROUND_SPLASH | RHH_GROUND_RIPPLE | RHH_GROUND_DUST : 0;
}
static int GroundEffectEvaluateInternal(const struct RhhGroundEffectRequest *request, struct RhhGroundEffectResult *out, bool32 preview)
{
    if (!request || request->struct_size < sizeof(*request) || request->slot >= OBJECT_EVENTS_COUNT
        || !GroundEffectMask(request->phase) || (request->base & ~GroundEffectMask(request->phase))
        || ((request->base & (RHH_GROUND_TRACKS | RHH_GROUND_DEEP_TRACKS)) == (RHH_GROUND_TRACKS | RHH_GROUND_DEEP_TRACKS))
        || !out || out->struct_size < sizeof(*out) || !RhhModsFinalized() || !RhhModsHasActiveRun()
        || !Ready() || sGroundEffectContext || sHabitatContext || NativeSaveTransactionActive()) return -1;
    const struct ObjectEvent *object = &gObjectEvents[request->slot];
    if (!object->active || (!object->isPlayer && (object->mapGroup != gSaveBlock1Ptr->location.mapGroup
        || object->mapNum != gSaveBlock1Ptr->location.mapNum))) return -1;
    struct RhhGroundEffectResult result = {.struct_size = sizeof(result), .phase = request->phase,
        .base = request->base, .permitted = TRUE, .is_preview = preview, .slot = request->slot};
    bool32 invisible = object->invisible || ModsWildActorHidden(object)
        || (object->spriteId < MAX_SPRITES && gSprites[object->spriteId].invisible);
    bool32 unlinked = object->spriteId >= MAX_SPRITES || !gSprites[object->spriteId].inUse
        || GetObjectEventIdByLocalIdAndMap(object->localId, object->mapNum, object->mapGroup) != request->slot;
    result.actor_flags = (object->isPlayer ? RHH_GROUND_ACTOR_PLAYER : 0)
        | (invisible ? RHH_GROUND_ACTOR_INVISIBLE : 0) | (object->offScreen ? RHH_GROUND_ACTOR_OFFSCREEN : 0)
        | (object->landingJump ? RHH_GROUND_ACTOR_LANDING_JUMP : 0)
        | (object->directionOverwrite ? RHH_GROUND_ACTOR_DIRECTION_OVERWRITE : 0)
        | (unlinked ? RHH_GROUND_ACTOR_UNLINKED : 0);
    struct RhhWildActorSnapshot wild = {.struct_size = sizeof(wild)};
    if (!WildActorAt(request->slot, &wild)) { result.actor_flags |= RHH_GROUND_ACTOR_WILD; result.wild_handle = wild.handle; }
    if (request->phase == RHH_GROUND_FINISH && object->landingJump) result.base &= ~RHH_GROUND_SPLASH;
    if (request->phase == RHH_GROUND_BEGIN && object->directionOverwrite) result.base &= ~(RHH_GROUND_TRACKS | RHH_GROUND_DEEP_TRACKS);
    result.value = result.base;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    result.current.struct_size = sizeof(result.current);
    if (!HabitatSample(object->currentCoords.x - MAP_OFFSET, object->currentCoords.y - MAP_OFFSET, &result.current))
        result.current_available = TRUE;
    else memset(&result.current, 0, sizeof(result.current));
    result.previous.struct_size = sizeof(result.previous);
    if (!HabitatSample(object->previousCoords.x - MAP_OFFSET, object->previousCoords.y - MAP_OFFSET, &result.previous))
        result.previous_available = TRUE;
    else memset(&result.previous, 0, sizeof(result.previous));
    if (RhhModsHasHook(RHH_WORLD_GROUND_EFFECT))
    {
        struct RhhWorldSnapshot world = {.struct_size = sizeof(world)};
        if (Current(&world)) { RhhModsWorldTimeScopeEnd(&timeScope); gRngValue = rng; gRng2Value = rng2; return -1; }
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WORLD_GROUND_EFFECT,
            .run_seed = RhhModsRunSeed(), .event_key = world.map, .subject = result.wild_handle ? wild.species : 0,
            .args = {request->slot, request->phase, result.base, preview}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = result.base};
        sGroundEffectContext = &result; RhhModsDispatch(&event, &decision); sGroundEffectContext = NULL;
        result.permitted = !!decision.allowed;
        if (decision.allowed && !(decision.value & ~(u64)GroundEffectMask(request->phase))
            && (decision.value & (RHH_GROUND_TRACKS | RHH_GROUND_DEEP_TRACKS)) != (RHH_GROUND_TRACKS | RHH_GROUND_DEEP_TRACKS))
            result.value = decision.value;
        else { result.value = result.base; result.permitted = FALSE; }
    }
    RhhModsWorldTimeScopeEnd(&timeScope);
    if (memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
    { result.value = result.base; result.permitted = FALSE; }
    gRngValue = rng; gRng2Value = rng2;
    if (request->phase == RHH_GROUND_FINISH && object->landingJump) result.value &= ~RHH_GROUND_SPLASH;
    if (request->phase == RHH_GROUND_BEGIN && object->directionOverwrite) result.value &= ~(RHH_GROUND_TRACKS | RHH_GROUND_DEEP_TRACKS);
    if (invisible || object->offScreen || unlinked) result.value &= result.base;
    *out = result; return 0;
}
static int GroundEffectEvaluate(const struct RhhGroundEffectRequest *request, struct RhhGroundEffectResult *out)
{ return GroundEffectEvaluateInternal(request, out, TRUE); }
u32 ModsGroundEffectFlags(const struct ObjectEvent *object, u32 phase, u32 flags)
{
    if (!object || !RhhModsHasHook(RHH_WORLD_GROUND_EFFECT) || RhhModsCurrentEvent()) return flags;
    uintptr_t address = (uintptr_t)object, first = (uintptr_t)gObjectEvents;
    if (address < first || address - first >= sizeof(gObjectEvents) || (address - first) % sizeof(*object)) return flags;
    u32 slot = (address - first) / sizeof(*object), base = 0, engineMask = 0;
    if (phase == RHH_GROUND_BEGIN) {
        engineMask = GROUND_EFFECT_FLAG_PUDDLE | GROUND_EFFECT_FLAG_SAND | GROUND_EFFECT_FLAG_DEEP_SAND;
        if (flags & GROUND_EFFECT_FLAG_SAND) base |= RHH_GROUND_TRACKS;
        if (flags & GROUND_EFFECT_FLAG_DEEP_SAND) base |= RHH_GROUND_DEEP_TRACKS;
    } else if (phase == RHH_GROUND_FINISH) {
        engineMask = GROUND_EFFECT_FLAG_PUDDLE | GROUND_EFFECT_FLAG_RIPPLES | GROUND_EFFECT_FLAG_LAND_ON_NORMAL_GROUND;
        if (flags & GROUND_EFFECT_FLAG_RIPPLES) base |= RHH_GROUND_RIPPLE;
        if (flags & GROUND_EFFECT_FLAG_LAND_ON_NORMAL_GROUND) base |= RHH_GROUND_DUST;
    } else return flags;
    if (flags & GROUND_EFFECT_FLAG_PUDDLE) base |= RHH_GROUND_SPLASH;
    struct RhhGroundEffectRequest request = {sizeof(request), slot, phase, base};
    struct RhhGroundEffectResult result = {.struct_size = sizeof(result)};
    if (GroundEffectEvaluateInternal(&request, &result, FALSE)) return flags;
    flags &= ~engineMask;
    if (result.value & RHH_GROUND_SPLASH) flags |= GROUND_EFFECT_FLAG_PUDDLE;
    if (result.value & RHH_GROUND_TRACKS) flags |= GROUND_EFFECT_FLAG_SAND;
    if (result.value & RHH_GROUND_DEEP_TRACKS) flags |= GROUND_EFFECT_FLAG_DEEP_SAND;
    if (result.value & RHH_GROUND_RIPPLE) flags |= GROUND_EFFECT_FLAG_RIPPLES;
    if (result.value & RHH_GROUND_DUST) flags |= GROUND_EFFECT_FLAG_LAND_ON_NORMAL_GROUND;
    return flags;
}
static const struct RhhGroundEffectService sGroundEffectService = {
    {sizeof(sGroundEffectService), RHH_SERVICE_VERSION(1, 0)}, GroundEffectEvaluate, GroundEffectContext
};
_Static_assert(sizeof(struct BerryTree) == 8, "berry snapshot token must cover the engine record");
_Static_assert(RHH_BERRY_EMPTY == BERRY_STAGE_NO_BERRY && RHH_BERRY_PLANTED == BERRY_STAGE_PLANTED
    && RHH_BERRY_SPROUTED == BERRY_STAGE_SPROUTED && RHH_BERRY_TALLER == BERRY_STAGE_TALLER
    && RHH_BERRY_FLOWERING == BERRY_STAGE_FLOWERING && RHH_BERRY_FRUIT == BERRY_STAGE_BERRIES
    && RHH_BERRY_TRUNK == BERRY_STAGE_TRUNK && RHH_BERRY_BUDDING == BERRY_STAGE_BUDDING,
    "public berry stages must match the engine");
static bool8 sBerryWaterBusy;
static const struct RhhBerryWaterResult *sBerryWaterContext;
static const struct RhhBerryManualWater *sBerryManualContext;
static const struct RhhBerryPlotInteraction *sBerryInteractionContext;
static const struct RhhBerryHarvest *sBerryHarvestContext;
static const struct RhhBerryYield *sBerryYieldContext;
static const struct RhhAuthoredBerryWaterResult *sAuthoredBerryWaterContext;
static struct BerryTree sBerryClockBefore[BERRY_TREES_COUNT];
static struct RhhBerryClockChange sBerryClockChanges[BERRY_TREES_COUNT];
static struct RhhBerryClockUpdate sBerryClockUpdate;
static const void *sBerryClockSave;
static bool8 sBerryClockActive, sBerryClockDispatch;
static int BerryClockContext(struct RhhBerryClockUpdate *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBerryClockDispatch
        || RhhModsCurrentEvent() != RHH_BERRY_CLOCK_UPDATED) return -1;
    *out = sBerryClockUpdate; return 0;
}
static int BerryClockChange(u32 index, struct RhhBerryClockChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBerryClockDispatch
        || RhhModsCurrentEvent() != RHH_BERRY_CLOCK_UPDATED || index >= sBerryClockUpdate.change_count) return -1;
    *out = sBerryClockChanges[index]; return 0;
}
static struct RhhBerryClockState BerryClockState(const struct BerryTree *tree)
{
    struct RhhBerryClockState state = {.berry = tree->berry, .stage = tree->stage, .watered = tree->watered,
        .moisture = tree->moistureLevel, .moisture_clock = tree->moistureClock,
        .minutes_to_stage = tree->minutesUntilNextStage, .yield = tree->berryYield,
        .regrowth = tree->regrowthCount, .growth_stopped = tree->stopGrowth, .weeds = tree->weeds,
        .pests = tree->pests, .mulch_ordinal = tree->mulch, .mutation_a = tree->mutationA, .mutation_b = tree->mutationB};
    if (tree->berry && tree->berry <= NUM_BERRIES)
        state.berry_item = RhhContentKeyForId(RHH_ITEM, BerryTypeToItemId(tree->berry));
    if (tree->mulch && tree->mulch <= ITEM_TO_MULCH(ITEM_AMAZE_MULCH))
        state.mulch_item = RhhContentKeyForId(RHH_ITEM, ITEM_GROWTH_MULCH + tree->mulch - 1);
    memcpy(state.state_token, tree, sizeof(state.state_token)); return state;
}
bool32 ModsBerryClockBegin(void)
{
    if (sBerryClockActive || sBerryClockDispatch || sBerryWaterBusy || !RhhModsHasActiveRun()
        || !RhhModsHasHook(RHH_BERRY_CLOCK_UPDATED) || RhhModsCurrentEvent() || NativeSaveTransactionActive()
        || gMain.inBattle || !gSaveBlock1Ptr) return FALSE;
    memcpy(sBerryClockBefore, gSaveBlock1Ptr->berryTrees, sizeof(sBerryClockBefore));
    sBerryClockUpdate = (struct RhhBerryClockUpdate){.struct_size = sizeof(sBerryClockUpdate), .run_id = RhhModsRunId()};
    sBerryClockSave = gSaveBlock1Ptr; sBerryClockActive = TRUE; return TRUE;
}
void ModsBerryClockFinish(s32 minutes)
{
    if (!sBerryClockActive) return;
    sBerryClockActive = FALSE;
    if (minutes <= 0 || !RhhModsHasActiveRun() || RhhModsRunId() != sBerryClockUpdate.run_id
        || gSaveBlock1Ptr != sBerryClockSave || RhhModsCurrentEvent() || NativeSaveTransactionActive()
        || gMain.inBattle || !RhhModsHasHook(RHH_BERRY_CLOCK_UPDATED)) return;
    sBerryClockUpdate.elapsed_minutes = minutes;
    for (u32 i = 0; i < BERRY_TREES_COUNT; ++i) {
        const struct BerryTree *after = &gSaveBlock1Ptr->berryTrees[i];
        if (!memcmp(&sBerryClockBefore[i], after, sizeof(*after))) continue;
        struct RhhBerryClockChange *change = &sBerryClockChanges[sBerryClockUpdate.change_count++];
        *change = (struct RhhBerryClockChange){.struct_size = sizeof(*change), .tree_id = i,
            .before = BerryClockState(&sBerryClockBefore[i]), .after = BerryClockState(after)};
    }
    if (!sBerryClockUpdate.change_count) return;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BERRY_CLOCK_UPDATED,
        .run_seed = RhhModsRunSeed(), .args = {minutes, sBerryClockUpdate.change_count}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    sBerryWaterBusy = TRUE; sBerryClockDispatch = TRUE; RhhModsDispatch(&event, &decision);
    sBerryClockDispatch = FALSE; sBerryWaterBusy = FALSE; gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
}
static bool32 BerryStageWaterable(const struct BerryTree *tree)
{
    if (!tree->berry || tree->berry > NUM_BERRIES || tree->stopGrowth) return FALSE;
    u32 bit;
    switch (tree->stage) {
    case BERRY_STAGE_PLANTED: bit = 1; break;
    case BERRY_STAGE_SPROUTED: bit = 2; break;
    case BERRY_STAGE_TALLER: case BERRY_STAGE_TRUNK: case BERRY_STAGE_BUDDING: bit = 4; break;
    case BERRY_STAGE_FLOWERING: bit = 8; break;
    default: return FALSE;
    }
    if (OW_BERRY_MOISTURE) return OW_BERRY_ALWAYS_WATERABLE ? tree->moistureLevel < 100 : tree->moistureLevel == 0;
    return !(tree->watered & bit);
}
static int BerryPlotTemplatesFor(u32 *indices, u32 *count, bool32 fieldBag)
{
    if (!count || !Ready() || !RhhModsFinalized() || !RhhModsHasActiveRun() || !gMapHeader.events
        || (gMapHeader.events->objectEventCount && !gMapHeader.events->objectEvents)
        || gMapHeader.events->objectEventCount > OBJECT_EVENT_TEMPLATES_COUNT
        || (gMain.callback2 != CB2_Overworld && !(fieldBag && gMain.callback2 == CB2_BagMenuRun
            && gBagMenu && gBagPosition.location == ITEMMENULOCATION_FIELD))) return -1;
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)};
    if (Current(&world) || !world.map) return -1;
    u32 found = 0;
    for (u32 i = 0; i < gMapHeader.events->objectEventCount; ++i)
    {
        const struct ObjectEventTemplate *t = &gSaveBlock1Ptr->objectEventTemplates[i];
        const struct ObjectEventTemplate *authored = &gMapHeader.events->objectEvents[i];
        /* Save-block script pointers can refer to another native process. */
        if (authored->kind == OBJ_KIND_CLONE || authored->script != BerryTreeScript) continue;
        if (t->localId != authored->localId || t->trainerRange_berryTreeId != authored->trainerRange_berryTreeId
            || t->graphicsId != OBJ_EVENT_GFX_BERRY_TREE || t->movementType != MOVEMENT_TYPE_BERRY_TREE_GROWTH) return -1;
        if (!t->localId || !t->trainerRange_berryTreeId || t->trainerRange_berryTreeId >= BERRY_TREES_COUNT
            || t->x < 0 || t->y < 0 || t->x >= gMapHeader.mapLayout->width || t->y >= gMapHeader.mapLayout->height) return -1;
        if (t->flagId && t->flagId >= FLAGS_COUNT && (t->flagId < SPECIAL_FLAGS_START || t->flagId > SPECIAL_FLAGS_END)) return -1;
        if (FlagGet(t->flagId)) continue;
        for (u32 j = 0; j < found; ++j) {
            const struct ObjectEventTemplate *other = &gSaveBlock1Ptr->objectEventTemplates[indices[j]];
            if (other->localId == t->localId || other->trainerRange_berryTreeId == t->trainerRange_berryTreeId) return -1;
        }
        indices[found++] = i;
    }
    *count = found; return 0;
}
static int BerryPlotTemplates(u32 *indices, u32 *count)
{ return BerryPlotTemplatesFor(indices, count, FALSE); }
static int BerryPlotCount(u32 *out)
{
    u32 indices[OBJECT_EVENT_TEMPLATES_COUNT], count;
    if (!out || BerryPlotTemplates(indices, &count)) return -1;
    *out = count; return 0;
}
static void BerryPlotSetState(struct RhhBerryPlot *plot, const struct BerryTree *tree)
{
    plot->berry = tree->berry; plot->stage = tree->stage; plot->watered = tree->watered;
    plot->moisture = tree->moistureLevel; plot->minutes_to_stage = tree->minutesUntilNextStage;
    plot->yield = tree->berryYield; plot->regrowth = tree->regrowthCount;
    plot->flags = (BerryStageWaterable(tree) ? RHH_BERRY_PLOT_WATERABLE : 0)
        | (OW_BERRY_MOISTURE ? RHH_BERRY_PLOT_MOISTURE : 0)
        | (OW_BERRY_MOISTURE && OW_BERRY_ALWAYS_WATERABLE ? RHH_BERRY_PLOT_ALWAYS_WATERABLE : 0)
        | (tree->stopGrowth ? RHH_BERRY_PLOT_GROWTH_STOPPED : 0);
    plot->berry_item = tree->berry && tree->berry <= NUM_BERRIES ? RhhContentKeyForId(RHH_ITEM, BerryTypeToItemId(tree->berry)) : 0;
    memcpy(plot->state_token, tree, sizeof(plot->state_token));
}
static int BerryPlotAtFor(u32 index, struct RhhBerryPlot *out, bool32 fieldBag)
{
    u32 indices[OBJECT_EVENT_TEMPLATES_COUNT], count;
    if (!out || out->struct_size < sizeof(*out) || BerryPlotTemplatesFor(indices, &count, fieldBag) || index >= count) return -1;
    const struct ObjectEventTemplate *t = &gSaveBlock1Ptr->objectEventTemplates[indices[index]];
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)}; if (Current(&world) || !world.map) return -1;
    struct RhhBerryPlot result = {.struct_size = sizeof(result), .index = index, .local_id = t->localId,
        .tree_id = t->trainerRange_berryTreeId, .map = world.map, .x = t->x, .y = t->y};
    BerryPlotSetState(&result, GetBerryTreeInfo(result.tree_id));
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    result.habitat.struct_size = sizeof(result.habitat);
    if (!HabitatSample(result.x, result.y, &result.habitat)) result.habitat_available = TRUE;
    else memset(&result.habitat, 0, sizeof(result.habitat));
    bool32 changed = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2; if (changed) return -1;
    *out = result; return 0;
}
static int BerryPlotAt(u32 index, struct RhhBerryPlot *out) { return BerryPlotAtFor(index, out, FALSE); }
static void BerryPlotCareFrom(const struct RhhBerryPlot *plot, struct RhhBerryPlotCare *out)
{
    struct BerryTree tree; memcpy(&tree, plot->state_token, sizeof(tree));
    struct RhhBerryPlotCare result = {.struct_size = sizeof(result), .weeds = tree.weeds,
        .pests = tree.pests, .mulch_ordinal = tree.mulch, .plot = *plot};
    if (tree.mulch && tree.mulch <= ITEM_TO_MULCH(ITEM_AMAZE_MULCH))
        result.mulch_item = RhhContentKeyForId(RHH_ITEM, ITEM_GROWTH_MULCH + tree.mulch - 1);
    *out = result;
}
static int BerryCareAt(u32 index, struct RhhBerryPlotCare *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhBerryPlot plot = {.struct_size = sizeof(plot)};
    if (BerryPlotAt(index, &plot)) return -1;
    BerryPlotCareFrom(&plot, out); return 0;
}
static bool32 BerryPlotMatches(const struct RhhBerryPlot *a, const struct RhhBerryPlot *b)
{
    return a->index == b->index && a->local_id == b->local_id && a->tree_id == b->tree_id
        && a->map == b->map && a->x == b->x && a->y == b->y && !memcmp(a->state_token, b->state_token, sizeof(a->state_token));
}
static bool32 BerryWaterEnvironmentMatches(const struct RhhBerryWaterRequest *request, const struct RhhBerryPlot *plot)
{
    if (!request->flags) return TRUE;
    if (!plot->habitat_available || !plot->habitat.permitted) return FALSE;
    if ((request->flags & (RHH_BERRY_WATER_REQUIRE_WEATHER | RHH_BERRY_WATER_REQUIRE_TERRAIN))
        && !plot->habitat.environment_available) return FALSE;
    const struct RhhWorldEnvironmentSnapshot *env = &plot->habitat.environment;
    return (!(request->flags & RHH_BERRY_WATER_REQUIRE_WEATHER) || env->weather.value == request->expected_weather)
        && (!(request->flags & RHH_BERRY_WATER_REQUIRE_TERRAIN) || env->terrain.value == request->expected_terrain)
        && (!(request->flags & RHH_BERRY_WATER_REQUIRE_NATURAL_WEATHER)
            || plot->habitat.natural_weather == request->expected.habitat.natural_weather)
        && (!(request->flags & RHH_BERRY_WATER_REQUIRE_OUTDOORS)
            || !(plot->habitat.flags & (RHH_HABITAT_INDOORS | RHH_HABITAT_UNDERGROUND | RHH_HABITAT_UNDERWATER)));
}
static int BerryWaterContext(struct RhhBerryWaterResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sBerryWaterContext
        || (event != RHH_CAN_WATER_BERRY_PLOT && event != RHH_BERRY_PLOT_WATERED)) return -1;
    *out = *sBerryWaterContext; return 0;
}
static int BerryManualWaterContext(struct RhhBerryManualWater *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBerryManualContext
        || RhhModsCurrentEvent() != RHH_BERRY_PLOT_MANUALLY_WATERED) return -1;
    *out = *sBerryManualContext; return 0;
}
static int BerryInteractionContext(struct RhhBerryPlotInteraction *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBerryInteractionContext
        || RhhModsCurrentEvent() != RHH_BERRY_PLOT_INTERACTED) return -1;
    *out = *sBerryInteractionContext; return 0;
}
static int BerryHarvestContext(struct RhhBerryHarvest *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBerryHarvestContext
        || RhhModsCurrentEvent() != RHH_BERRY_PLOT_HARVESTED) return -1;
    *out = *sBerryHarvestContext; return 0;
}
static int BerryObjectPlot(u32 objectSlot, struct RhhBerryPlot *out)
{
    if (objectSlot >= OBJECT_EVENTS_COUNT || !out || out->struct_size < sizeof(*out) || !gSaveBlock1Ptr) return -1;
    const struct ObjectEvent *object = &gObjectEvents[objectSlot];
    if (!object->active || object->mapGroup != gSaveBlock1Ptr->location.mapGroup
        || object->mapNum != gSaveBlock1Ptr->location.mapNum) return -1;
    u32 indices[OBJECT_EVENT_TEMPLATES_COUNT], count;
    if (BerryPlotTemplatesFor(indices, &count, TRUE)) return -1;
    for (u32 i = 0; i < count; ++i)
    {
        const struct ObjectEventTemplate *t = &gSaveBlock1Ptr->objectEventTemplates[indices[i]];
        if (t->localId == object->localId && t->trainerRange_berryTreeId == object->trainerRange_berryTreeId
            && object->currentCoords.x == t->x + MAP_OFFSET && object->currentCoords.y == t->y + MAP_OFFSET)
            return BerryPlotAtFor(i, out, TRUE);
    }
    return -1;
}
static int BerryYieldContext(struct RhhBerryYield *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBerryYieldContext
        || RhhModsCurrentEvent() != RHH_BERRY_HARVEST_YIELD) return -1;
    *out = *sBerryYieldContext; return 0;
}
static int BerryYieldEvaluate(const struct RhhBerryPlot *plot, struct RhhBerryYield *out)
{
    if (!plot || !out || out->struct_size < sizeof(*out) || sBerryYieldContext || sBerryWaterBusy
        || NativeSaveTransactionActive() || gMain.inBattle || plot->stage != RHH_BERRY_FRUIT
        || !plot->berry_item || !plot->yield || plot->yield > 255) return -1;
    struct RhhBerryYield result = {.struct_size = sizeof(result), .base = plot->yield,
        .value = plot->yield, .accepted = TRUE, .run_id = RhhModsRunId()};
    BerryPlotCareFrom(plot, &result.plot);
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    if (RhhModsHasHook(RHH_BERRY_HARVEST_YIELD))
    {
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BERRY_HARVEST_YIELD,
            .run_seed = RhhModsRunSeed(), .event_key = plot->map, .subject = plot->berry_item,
            .args = {plot->tree_id, plot->yield, plot->stage, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = plot->yield};
        sBerryWaterBusy = TRUE; sBerryYieldContext = &result; RhhModsDispatch(&event, &decision);
        sBerryYieldContext = NULL; sBerryWaterBusy = FALSE;
        if (decision.allowed && decision.value >= 1 && decision.value <= 255) result.value = decision.value;
        else result.accepted = FALSE;
    }
    if (memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
    { result.value = result.base; result.accepted = FALSE; }
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
    struct RhhBerryPlot current = {.struct_size = sizeof(current)};
    if (!RhhModsHasActiveRun() || result.run_id != RhhModsRunId() || BerryPlotAt(plot->index, &current)
        || !BerryPlotMatches(plot, &current) || plot->habitat_available != current.habitat_available
        || (plot->habitat_available && memcmp(&plot->habitat, &current.habitat, sizeof(plot->habitat)))) return -1;
    *out = result; return 0;
}
static int BerryYieldPreview(u32 index, struct RhhBerryYield *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhWorldTimeScope scope; RhhModsWorldTimeScopeBegin(&scope);
    struct RhhBerryPlot plot = {.struct_size = sizeof(plot)};
    int status = BerryPlotAt(index, &plot) ? -1 : BerryYieldEvaluate(&plot, out);
    RhhModsWorldTimeScopeEnd(&scope); return status;
}
u32 ModsBerryHarvestYield(u32 objectSlot, u32 treeId, u32 base)
{
    if (!RhhModsHasHook(RHH_BERRY_HARVEST_YIELD) || RhhModsCurrentEvent()) return base;
    struct RhhWorldTimeScope scope; RhhModsWorldTimeScopeBegin(&scope);
    struct RhhBerryPlot plot = {.struct_size = sizeof(plot)};
    struct RhhBerryYield result = {.struct_size = sizeof(result)};
    u32 value = base;
    if (!BerryObjectPlot(objectSlot, &plot) && plot.tree_id == treeId && plot.yield == base
        && !BerryYieldEvaluate(&plot, &result)) value = result.value;
    RhhModsWorldTimeScopeEnd(&scope); return value;
}
bool32 ModsBerryHarvestCapture(u32 objectSlot, struct RhhBerryHarvest *out)
{
    if (!out || !RhhModsHasActiveRun() || !RhhModsHasHook(RHH_BERRY_PLOT_HARVESTED)
        || sBerryWaterBusy || RhhModsCurrentEvent() || NativeSaveTransactionActive() || gMain.inBattle) return FALSE;
    struct RhhBerryPlot plot = {.struct_size = sizeof(plot)};
    if (BerryObjectPlot(objectSlot, &plot)) return FALSE;
    struct RhhBerryHarvest result = {.struct_size = sizeof(result), .object_slot = objectSlot, .run_id = RhhModsRunId()};
    BerryPlotCareFrom(&plot, &result.plot); *out = result; return TRUE;
}
void ModsBerryHarvestCommitted(struct RhhBerryHarvest *capture, u32 berryItem, u32 berryCount, u32 mutationItem, u32 mutationCount)
{
    if (!capture || capture->struct_size != sizeof(*capture) || (!berryCount && !mutationCount)
        || !RhhModsHasActiveRun() || capture->run_id != RhhModsRunId() || sBerryWaterBusy
        || RhhModsCurrentEvent() || NativeSaveTransactionActive() || gMain.inBattle
        || !RhhModsHasHook(RHH_BERRY_PLOT_HARVESTED)) return;
    struct RhhBerryPlot current = {.struct_size = sizeof(current)};
    const struct RhhBerryPlot *before = &capture->plot.plot;
    if (BerryObjectPlot(capture->object_slot, &current) || !BerryPlotMatches(before, &current)) return;
    capture->berry_item = berryCount ? RhhContentKeyForId(RHH_ITEM, berryItem) : 0;
    capture->mutation_item = mutationCount ? RhhContentKeyForId(RHH_ITEM, mutationItem) : 0;
    if ((berryCount && !capture->berry_item) || (mutationCount && !capture->mutation_item)) return;
    capture->berry_count = berryCount; capture->mutation_count = mutationCount;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BERRY_PLOT_HARVESTED,
        .run_seed = RhhModsRunSeed(), .event_key = before->map, .subject = capture->berry_item,
        .args = {before->tree_id, berryCount, mutationCount, capture->object_slot}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    sBerryWaterBusy = TRUE; sBerryHarvestContext = capture; RhhModsDispatch(&event, &decision);
    sBerryHarvestContext = NULL; sBerryWaterBusy = FALSE; gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
}
bool32 ModsBerryInteractionCapture(u32 objectSlot, u32 cause, struct RhhBerryPlotInteraction *out)
{
    if (!out || cause < RHH_BERRY_INTERACTION_PLANTED || cause > RHH_BERRY_INTERACTION_PESTS_CLEARED
        || sBerryWaterBusy || sBerryInteractionContext || RhhModsCurrentEvent() || NativeSaveTransactionActive()
        || gMain.inBattle || !RhhModsHasHook(RHH_BERRY_PLOT_INTERACTED)) return FALSE;
    struct RhhBerryPlot plot = {.struct_size = sizeof(plot)};
    if (BerryObjectPlot(objectSlot, &plot)) return FALSE;
    struct RhhBerryPlotInteraction result = {.struct_size = sizeof(result), .cause = cause,
        .object_slot = objectSlot, .run_id = RhhModsRunId()};
    BerryPlotCareFrom(&plot, &result.before); *out = result; return TRUE;
}
void ModsBerryInteractionCommitted(struct RhhBerryPlotInteraction *capture)
{
    if (!capture || capture->struct_size != sizeof(*capture) || !RhhModsHasActiveRun()
        || capture->run_id != RhhModsRunId() || sBerryWaterBusy || sBerryInteractionContext || RhhModsCurrentEvent()
        || NativeSaveTransactionActive() || gMain.inBattle || !RhhModsHasHook(RHH_BERRY_PLOT_INTERACTED)) return;
    struct RhhBerryPlot plot = {.struct_size = sizeof(plot)};
    const struct RhhBerryPlot *before = &capture->before.plot;
    if (BerryObjectPlot(capture->object_slot, &plot) || plot.index != before->index || plot.map != before->map
        || plot.tree_id != before->tree_id || plot.local_id != before->local_id
        || plot.x != before->x || plot.y != before->y) return;
    BerryPlotCareFrom(&plot, &capture->after);
    capture->changed = memcmp(before->state_token, plot.state_token, sizeof(plot.state_token)) != 0;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BERRY_PLOT_INTERACTED,
        .run_seed = RhhModsRunSeed(), .event_key = before->map,
        .subject = before->berry_item ? before->berry_item : plot.berry_item,
        .args = {capture->cause, before->tree_id, capture->changed, capture->object_slot}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    sBerryWaterBusy = TRUE; sBerryInteractionContext = capture; RhhModsDispatch(&event, &decision);
    sBerryInteractionContext = NULL; sBerryWaterBusy = FALSE; gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
}
bool32 ModsBerryManualCapture(u32 objectSlot, struct RhhBerryManualWater *out)
{
    if (!out || objectSlot >= OBJECT_EVENTS_COUNT || sBerryWaterBusy || RhhModsCurrentEvent()
        || NativeSaveTransactionActive() || gMain.inBattle || !RhhModsHasHook(RHH_BERRY_PLOT_MANUALLY_WATERED)) return FALSE;
    struct RhhBerryManualWater result = {.struct_size = sizeof(result), .object_slot = objectSlot,
        .run_id = RhhModsRunId(), .before.struct_size = sizeof(result.before)};
    if (BerryObjectPlot(objectSlot, &result.before)) return FALSE;
    *out = result; return TRUE;
}
void ModsBerryManualCommitted(struct RhhBerryManualWater *capture)
{
    if (!capture || capture->struct_size != sizeof(*capture) || !RhhModsHasActiveRun()
        || capture->run_id != RhhModsRunId() || sBerryWaterBusy || RhhModsCurrentEvent()
        || !gSaveBlock1Ptr || NativeSaveTransactionActive() || gMain.inBattle
        || capture->object_slot >= OBJECT_EVENTS_COUNT) return;
    struct ObjectEvent *object = &gObjectEvents[capture->object_slot];
    const struct RhhBerryPlot *before = &capture->before;
    if (!object->active || object->localId != before->local_id || object->trainerRange_berryTreeId != before->tree_id
        || object->mapGroup != gSaveBlock1Ptr->location.mapGroup || object->mapNum != gSaveBlock1Ptr->location.mapNum
        || object->currentCoords.x != before->x + MAP_OFFSET || object->currentCoords.y != before->y + MAP_OFFSET) return;
    capture->after.struct_size = sizeof(capture->after);
    if (BerryPlotAtFor(before->index, &capture->after, TRUE) || capture->after.map != before->map
        || capture->after.tree_id != before->tree_id || capture->after.local_id != before->local_id
        || capture->after.x != before->x || capture->after.y != before->y) return;
    capture->changed = memcmp(before->state_token, capture->after.state_token, sizeof(before->state_token)) != 0;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BERRY_PLOT_MANUALLY_WATERED,
        .run_seed = RhhModsRunSeed(), .event_key = before->map, .subject = before->berry_item,
        .args = {before->tree_id, before->stage, capture->changed, capture->object_slot}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBerryWaterBusy = TRUE; sBerryManualContext = capture; RhhModsDispatch(&event, &decision);
    sBerryManualContext = NULL; sBerryWaterBusy = FALSE; gRngValue = rng; gRng2Value = rng2;
}
static void BerryWaterDispatch(u32 kind, struct RhhBerryWaterResult *result, struct RhhModDecision *decision)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = result->before.map, .subject = result->before.berry_item,
        .args = {result->before.tree_id, result->before.stage, result->is_preview, 0}};
    const struct RhhBerryWaterResult *previous = sBerryWaterContext;
    sBerryWaterContext = result; RhhModsDispatch(&event, decision); sBerryWaterContext = previous;
}
static int BerryWater(const struct RhhBerryWaterRequest *request, struct RhhBerryWaterResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_BERRY_WATER_INVALID;
    *out = (struct RhhBerryWaterResult){.struct_size = sizeof(*out), .is_preview = preview,
        .before.struct_size = sizeof(out->before), .after.struct_size = sizeof(out->after)};
    if (!request || request->struct_size < sizeof(*request)
        || (request->flags & ~(RHH_BERRY_WATER_REQUIRE_WEATHER | RHH_BERRY_WATER_REQUIRE_TERRAIN
                              | RHH_BERRY_WATER_REQUIRE_NATURAL_WEATHER | RHH_BERRY_WATER_REQUIRE_OUTDOORS))
        || ((request->flags & RHH_BERRY_WATER_REQUIRE_WEATHER) ? request->expected_weather > RHH_WEATHER_HEAVY_RAIN : request->expected_weather != 0)
        || ((request->flags & RHH_BERRY_WATER_REQUIRE_TERRAIN) ? request->expected_terrain > RHH_TERRAIN_PSYCHIC : request->expected_terrain != 0)
        || ((request->flags & RHH_BERRY_WATER_REQUIRE_NATURAL_WEATHER)
            && (!request->expected.habitat_available || request->expected.habitat.struct_size < sizeof(request->expected.habitat)
                || request->expected.habitat.natural_weather > RHH_NATURAL_WEATHER_UNKNOWN))
        || !request->owner
        || !RhhModsRegisteredKey(request->owner) || request->expected.struct_size < sizeof(request->expected) || request->expected.reserved)
        return out->status = RHH_BERRY_WATER_INVALID;
    out->owner = request->owner;
    if (sBerryWaterBusy || !WildActorCommandReady() || NativeSaveTransactionActive()) return out->status = RHH_BERRY_WATER_UNAVAILABLE;
    if (BerryPlotAt(request->expected.index, &out->before)) return out->status = RHH_BERRY_WATER_UNAVAILABLE;
    out->after = out->before;
    if (!BerryPlotMatches(&out->before, &request->expected) || !BerryWaterEnvironmentMatches(request, &out->before))
        return out->status = RHH_BERRY_WATER_STALE;
    if (!(out->before.flags & RHH_BERRY_PLOT_WATERABLE) || !out->before.berry_item) return out->status = RHH_BERRY_WATER_NOT_NEEDED;
    struct BerryTree *tree = GetBerryTreeInfo(out->before.tree_id), staged = *tree;
    if (!WaterBerryTreeState(&staged)) return out->status = RHH_BERRY_WATER_UNAVAILABLE;
    BerryPlotSetState(&out->after, &staged); out->changed = memcmp(&staged, tree, sizeof(staged)) != 0;
    if (!out->changed) return out->status = RHH_BERRY_WATER_NOT_NEEDED;
    sBerryWaterBusy = TRUE; struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    BerryWaterDispatch(RHH_CAN_WATER_BERRY_PLOT, out, &decision);
    bool32 rngChanged = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed || rngChanged) {
        sBerryWaterBusy = FALSE; snprintf(out->reason, sizeof(out->reason), "%s", rngChanged ? "constraint used gameplay RNG" : decision.reason);
        return out->status = RHH_BERRY_WATER_DENIED;
    }
    if (!preview)
    {
        struct RhhBerryPlot current = {.struct_size = sizeof(current)};
        if (NativeSaveTransactionActive() || !WildActorCommandReady() || BerryPlotAt(request->expected.index, &current)
            || !BerryPlotMatches(&out->before, &current) || !BerryWaterEnvironmentMatches(request, &current))
        { sBerryWaterBusy = FALSE; return out->status = RHH_BERRY_WATER_STALE; }
        *tree = staged; decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
        BerryWaterDispatch(RHH_BERRY_PLOT_WATERED, out, &decision);
    }
    sBerryWaterBusy = FALSE; return RHH_BERRY_WATER_OK;
}
static int BerryWaterPreview(const struct RhhBerryWaterRequest *r, struct RhhBerryWaterResult *o) { return BerryWater(r, o, TRUE); }
static int BerryWaterApply(const struct RhhBerryWaterRequest *r, struct RhhBerryWaterResult *o) { return BerryWater(r, o, FALSE); }
static bool32 IsAuthoredBerryObject(const struct ObjectEventTemplate *object)
{
    return object && object->kind == OBJ_KIND_NORMAL && object->script == BerryTreeScript
        && object->graphicsId == OBJ_EVENT_GFX_BERRY_TREE && object->movementType == MOVEMENT_TYPE_BERRY_TREE_GROWTH
        && object->trainerRange_berryTreeId && object->trainerRange_berryTreeId < BERRY_TREES_COUNT;
}
static int BerryReferences(const struct RhhBerryReferenceRequest *request, struct RhhBerryReferencePage *out)
{
    if (!request || request->struct_size < sizeof(*request) || !out || out->struct_size < sizeof(*out)
        || !RhhModsFinalized() || request->tree_id >= BERRY_TREES_COUNT
        || !request->limit || request->limit > RHH_BERRY_REFERENCE_PAGE_MAX) return -1;
    const struct RhhBerryReferenceRequest query = *request;
    struct RhhBerryReferencePage result = {.struct_size = sizeof(result), .tree_id = query.tree_id, .first = query.first};
    for (u32 group = 0; group < MAP_GROUPS_COUNT; ++group) {
        if (!gMapGroups[group]) { result.unavailable_maps += MAP_GROUP_COUNT[group]; continue; }
        for (u32 number = 0; number < MAP_GROUP_COUNT[group]; ++number) {
            const struct MapHeader *header = gMapGroups[group][number];
            if (!header) { ++result.unavailable_maps; continue; }
            if (!header->events || !header->events->objectEventCount) continue;
            RhhContentKey key = RhhContentKeyForId(RHH_MAP, (group << 8) | number);
            const struct MapHeader *checked; u32 count;
            if (!key || AuthoredMapResolve(key, &checked, NULL) || checked != header || AuthoredObjects(header, &count))
            { ++result.unavailable_maps; continue; }
            for (u32 index = 0; index < count; ++index) {
                const struct ObjectEventTemplate *object = &header->events->objectEvents[index];
                if (!IsAuthoredBerryObject(object) || object->trainerRange_berryTreeId != query.tree_id) continue;
                if (result.total >= query.first && result.count < query.limit) {
                    result.references[result.count++] = (struct RhhBerryReference){.map = key, .object_index = index,
                        .local_id = object->localId, .elevation = object->elevation, .x = object->x, .y = object->y};
                }
                ++result.total;
            }
        }
    }
    if (query.first > result.total) return -1;
    result.next = query.first + result.count; *out = result; return 0;
}
static int AuthoredBerryPlotAt(RhhContentKey key, u32 index, struct RhhAuthoredBerryPlot *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || !gSaveBlock1Ptr) return -1;
    const struct MapHeader *header; RhhContentKey map; u32 count;
    if (AuthoredMapResolve(key, &header, &map) || AuthoredObjects(header, &count) || index >= count) return -1;
    const struct ObjectEventTemplate *object = &header->events->objectEvents[index];
    if (!IsAuthoredBerryObject(object)) return -1;
    const struct BerryTree *tree = GetBerryTreeInfo(object->trainerRange_berryTreeId);
    struct RhhAuthoredBerryPlot result = {.struct_size = sizeof(result), .object_index = index,
        .local_id = object->localId, .tree_id = object->trainerRange_berryTreeId, .elevation = object->elevation,
        .x = object->x, .y = object->y, .map = map, .run_id = RhhModsRunId(),
        .berry = tree->berry, .stage = tree->stage, .watered = tree->watered, .moisture = tree->moistureLevel,
        .minutes_to_stage = tree->minutesUntilNextStage, .yield = tree->berryYield, .regrowth = tree->regrowthCount,
        .growth_stopped = tree->stopGrowth, .weeds = tree->weeds, .pests = tree->pests, .mulch_ordinal = tree->mulch};
    if (tree->berry && tree->berry <= NUM_BERRIES)
        result.berry_item = RhhContentKeyForId(RHH_ITEM, BerryTypeToItemId(tree->berry));
    else if (tree->berry) result.flags |= RHH_AUTHORED_BERRY_UNKNOWN_ITEM;
    if (tree->mulch && tree->mulch <= ITEM_TO_MULCH(ITEM_AMAZE_MULCH))
        result.mulch_item = RhhContentKeyForId(RHH_ITEM, ITEM_GROWTH_MULCH + tree->mulch - 1);
    *out = result; return 0;
}
static int AuthoredBerrySnapshot(RhhContentKey map, u32 index, struct RhhAuthoredBerrySnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhAuthoredBerrySnapshot result = {.struct_size = sizeof(result),
        .plot.struct_size = sizeof(result.plot)};
    if (AuthoredBerryPlotAt(map, index, &result.plot)) return -1;
    const struct BerryTree *tree = GetBerryTreeInfo(result.plot.tree_id);
    result.flags = (BerryStageWaterable(tree) ? RHH_BERRY_PLOT_WATERABLE : 0)
        | (OW_BERRY_MOISTURE ? RHH_BERRY_PLOT_MOISTURE : 0)
        | (OW_BERRY_MOISTURE && OW_BERRY_ALWAYS_WATERABLE ? RHH_BERRY_PLOT_ALWAYS_WATERABLE : 0)
        | (tree->stopGrowth ? RHH_BERRY_PLOT_GROWTH_STOPPED : 0);
    memcpy(result.state_token, tree, sizeof(result.state_token));
    *out = result; return 0;
}
static bool32 AuthoredBerryMatches(const struct RhhAuthoredBerrySnapshot *a, const struct RhhAuthoredBerrySnapshot *b)
{
    return a->plot.map == b->plot.map && a->plot.run_id == b->plot.run_id
        && a->plot.object_index == b->plot.object_index && a->plot.local_id == b->plot.local_id
        && a->plot.tree_id == b->plot.tree_id && a->plot.x == b->plot.x && a->plot.y == b->plot.y
        && a->plot.elevation == b->plot.elevation && !memcmp(a->state_token, b->state_token, sizeof(a->state_token));
}
static bool32 AuthoredBerryEnvironmentMatches(const struct RhhAuthoredBerryWaterRequest *request)
{
    if (!request->flags) return TRUE;
    const struct RhhWorldEnvironmentService *env = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldEnvironmentSnapshot value = {.struct_size = sizeof(value)};
    return env && !env->at(request->expected.plot.map, &value)
        && (!(request->flags & RHH_BERRY_WATER_REQUIRE_WEATHER) || value.weather.value == request->expected_weather)
        && (!(request->flags & RHH_BERRY_WATER_REQUIRE_TERRAIN) || value.terrain.value == request->expected_terrain);
}
static int AuthoredBerryWaterContext(struct RhhAuthoredBerryWaterResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sAuthoredBerryWaterContext
        || (event != RHH_CAN_WATER_AUTHORED_BERRY && event != RHH_AUTHORED_BERRY_WATERED)) return -1;
    *out = *sAuthoredBerryWaterContext; return 0;
}
static void AuthoredBerryWaterDispatch(u32 kind, struct RhhAuthoredBerryWaterResult *result, struct RhhModDecision *decision)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = result->before.plot.map, .subject = result->before.plot.berry_item,
        .args = {result->before.plot.tree_id, result->before.plot.stage, result->is_preview, 0}};
    const struct RhhAuthoredBerryWaterResult *previous = sAuthoredBerryWaterContext;
    sAuthoredBerryWaterContext = result; RhhModsDispatch(&event, decision); sAuthoredBerryWaterContext = previous;
}
static int AuthoredBerryWater(const struct RhhAuthoredBerryWaterRequest *request, struct RhhAuthoredBerryWaterResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_BERRY_WATER_INVALID;
    *out = (struct RhhAuthoredBerryWaterResult){.struct_size = sizeof(*out), .is_preview = preview};
    if (!request || request->struct_size < sizeof(*request) || !request->owner || !RhhModsRegisteredKey(request->owner)
        || request->expected.struct_size < sizeof(request->expected) || request->expected.plot.struct_size < sizeof(request->expected.plot)
        || (request->flags & ~(RHH_BERRY_WATER_REQUIRE_WEATHER | RHH_BERRY_WATER_REQUIRE_TERRAIN))
        || ((request->flags & RHH_BERRY_WATER_REQUIRE_WEATHER) ? request->expected_weather > RHH_WEATHER_HEAVY_RAIN : request->expected_weather != 0)
        || ((request->flags & RHH_BERRY_WATER_REQUIRE_TERRAIN) ? request->expected_terrain > RHH_TERRAIN_PSYCHIC : request->expected_terrain != 0))
        return out->status = RHH_BERRY_WATER_INVALID;
    out->owner = request->owner;
    if (sBerryWaterBusy || !WildActorCommandReady() || NativeSaveTransactionActive()) return out->status = RHH_BERRY_WATER_UNAVAILABLE;
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)};
    out->before.struct_size = sizeof(out->before);
    if (Current(&world) || world.map == request->expected.plot.map
        || AuthoredBerrySnapshot(request->expected.plot.map, request->expected.plot.object_index, &out->before))
        return out->status = RHH_BERRY_WATER_UNAVAILABLE;
    out->after = out->before;
    if (!AuthoredBerryMatches(&out->before, &request->expected) || !AuthoredBerryEnvironmentMatches(request))
        return out->status = RHH_BERRY_WATER_STALE;
    if (!(out->before.flags & RHH_BERRY_PLOT_WATERABLE) || !out->before.plot.berry_item)
        return out->status = RHH_BERRY_WATER_NOT_NEEDED;
    struct BerryTree *tree = GetBerryTreeInfo(out->before.plot.tree_id), staged = *tree;
    if (!WaterBerryTreeState(&staged) || !memcmp(&staged, tree, sizeof(staged))) return out->status = RHH_BERRY_WATER_NOT_NEEDED;
    memcpy(out->after.state_token, &staged, sizeof(staged));
    out->after.plot.watered = staged.watered; out->after.plot.moisture = staged.moistureLevel;
    out->after.plot.yield = staged.berryYield;
    out->after.flags = (out->after.flags & ~RHH_BERRY_PLOT_WATERABLE) | (BerryStageWaterable(&staged) ? RHH_BERRY_PLOT_WATERABLE : 0);
    out->changed = TRUE; sBerryWaterBusy = TRUE;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    AuthoredBerryWaterDispatch(RHH_CAN_WATER_AUTHORED_BERRY, out, &decision);
    bool32 rngChanged = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed || rngChanged) {
        sBerryWaterBusy = FALSE; snprintf(out->reason, sizeof(out->reason), "%s", rngChanged ? "constraint used gameplay RNG" : decision.reason);
        return out->status = RHH_BERRY_WATER_DENIED;
    }
    struct RhhAuthoredBerrySnapshot current = {.struct_size = sizeof(current)};
    world.struct_size = sizeof(world);
    if (NativeSaveTransactionActive() || !WildActorCommandReady() || Current(&world) || world.map == request->expected.plot.map
        || AuthoredBerrySnapshot(request->expected.plot.map, request->expected.plot.object_index, &current)
        || !AuthoredBerryMatches(&out->before, &current) || !AuthoredBerryEnvironmentMatches(request))
    { sBerryWaterBusy = FALSE; return out->status = RHH_BERRY_WATER_STALE; }
    if (!preview) {
        *tree = staged; decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
        AuthoredBerryWaterDispatch(RHH_AUTHORED_BERRY_WATERED, out, &decision);
    }
    sBerryWaterBusy = FALSE; return RHH_BERRY_WATER_OK;
}
static int AuthoredBerryWaterPreview(const struct RhhAuthoredBerryWaterRequest *r, struct RhhAuthoredBerryWaterResult *o)
{ return AuthoredBerryWater(r, o, TRUE); }
static int AuthoredBerryWaterApply(const struct RhhAuthoredBerryWaterRequest *r, struct RhhAuthoredBerryWaterResult *o)
{ return AuthoredBerryWater(r, o, FALSE); }
static const struct RhhBerryPlotService sBerryPlotService = {
    {sizeof(sBerryPlotService), RHH_SERVICE_VERSION(1, 9)}, BerryPlotCount, BerryPlotAt, BerryWaterPreview,
    BerryWaterApply, BerryWaterContext, BerryManualWaterContext, BerryCareAt, BerryInteractionContext, BerryHarvestContext,
    BerryYieldPreview, BerryYieldContext, AuthoredBerryPlotAt, AuthoredBerrySnapshot, AuthoredBerryWaterPreview,
    AuthoredBerryWaterApply, AuthoredBerryWaterContext, BerryClockContext, BerryClockChange, BerryReferences
};
static int SceneTileAt(int32_t x, int32_t y, struct RhhSceneTile *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)};
    struct RhhWorldTile tile = {.struct_size = sizeof(tile)};
    if (Current(&world) || TileAt(x, y, &tile)) return -1;
    const struct RhhModSceneRule *rule = NULL;
    if (RhhModsSelectSceneRule(world.map, x, y, tile.metatile, tile.behavior, &rule)) return -1;
    struct RhhSceneTile result = {
        .struct_size = sizeof(result), .map = world.map, .x = x, .y = y, .tile = tile,
        .style = {RHH_SCENE_SOLID, RHH_SCENE_UPPER_FLAT, RHH_SCENE_RECEIVES_SHADOW, 0, 0}
    };
    if (rule) { result.rule = RhhContentKeyOf(rule->id); result.style = rule->style; }
    *out = result; return 0;
}
static int SceneWindow(struct RhhSceneWindow *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsFinalized() || !Ready()
        || gBackupMapLayout.width > INT16_MAX || gBackupMapLayout.height > INT16_MAX) return -1;
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)};
    if (Current(&world) || !world.map) return -1;
    struct RhhSceneWindow result = {.struct_size = sizeof(result), .map = world.map,
        .min_x = -MAP_OFFSET, .min_y = -MAP_OFFSET,
        .max_x = gBackupMapLayout.width - MAP_OFFSET - 1, .max_y = gBackupMapLayout.height - MAP_OFFSET - 1};
    *out = result; return 0;
}
static int SceneWindowTileAt(int32_t x, int32_t y, struct RhhSceneTile *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhSceneWindow window = {.struct_size = sizeof(window)};
    if (SceneWindow(&window) || x < window.min_x || x > window.max_x || y < window.min_y || y > window.max_y) return -1;
    if (x >= 0 && y >= 0 && x < gMapHeader.mapLayout->width && y < gMapHeader.mapLayout->height)
        return SceneTileAt(x, y, out);
    struct RhhWorldTile tile = {.struct_size = sizeof(tile)};
    if (TileAtInternal(x, y, &tile, TRUE)) return -1;
    struct RhhSceneTile result = {.struct_size = sizeof(result), .map = window.map, .x = x, .y = y, .tile = tile,
        .style = {RHH_SCENE_SOLID, RHH_SCENE_UPPER_FLAT, RHH_SCENE_RECEIVES_SHADOW, 0, 0}};
    *out = result; return 0;
}
/* Reconstruct the last connection writer only when its authored block still
 * matches the loaded grid and both maps interpret metatiles identically. */
static int SceneWindowResolve(int32_t x, int32_t y, struct RhhSceneWindowTile *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhSceneWindowTile result = {.struct_size = sizeof(result),
        .origin = RHH_SCENE_WINDOW_UNKNOWN, .scene.struct_size = sizeof(result.scene)};
    if (SceneWindowTileAt(x, y, &result.scene)) return -1;
    const struct MapLayout *current = gMapHeader.mapLayout;
    if (x >= 0 && y >= 0 && x < current->width && y < current->height)
    {
        result.origin = RHH_SCENE_WINDOW_CURRENT; result.source_map = result.scene.map;
        result.source_x = x; result.source_y = y; *out = result; return 0;
    }
    u16 block = gBackupMapLayout.map[(y + MAP_OFFSET) * gBackupMapLayout.width + x + MAP_OFFSET];
    if (block == MAPGRID_UNDEFINED)
    { result.origin = RHH_SCENE_WINDOW_BORDER; *out = result; return 0; }
    const struct MapConnections *connections = gMapHeader.connections;
    if (!connections || !connections->connections || connections->count < 0 || connections->count > 256)
    { *out = result; return 0; }
    const struct MapLayout *source = NULL;
    RhhContentKey sourceKey = 0;
    int32_t sourceX = 0, sourceY = 0;
    for (int i = 0; i < connections->count; ++i)
    {
        const struct MapConnection *c = &connections->connections[i];
        bool32 inStrip = (c->direction == CONNECTION_SOUTH && y >= current->height && y < current->height + MAP_OFFSET)
            || (c->direction == CONNECTION_NORTH && y < 0 && y >= -MAP_OFFSET)
            || (c->direction == CONNECTION_WEST && x < 0 && x >= -MAP_OFFSET)
            || (c->direction == CONNECTION_EAST && x >= current->width && x <= current->width + MAP_OFFSET);
        if (!inStrip) continue;
        RhhContentKey key = RhhContentKeyForId(RHH_MAP, ((u32)c->mapGroup << 8) | c->mapNum);
        if (!key) { source = NULL; sourceKey = 0; continue; }
        const struct MapHeader *header = GetMapHeaderFromConnection(c);
        const struct MapLayout *layout = header ? header->mapLayout : NULL;
        if (!layout || !layout->map || layout->width <= 0 || layout->height <= 0)
        { source = NULL; sourceKey = 0; continue; }
        int64_t sx, sy;
        if (c->direction == CONNECTION_SOUTH || c->direction == CONNECTION_NORTH)
        { sx = (int64_t)x - c->offset; sy = c->direction == CONNECTION_SOUTH ? y - current->height : (int64_t)layout->height + y; }
        else
        { sx = c->direction == CONNECTION_WEST ? (int64_t)layout->width + x : x - current->width; sy = (int64_t)y - c->offset; }
        if (sx < 0 || sy < 0 || sx >= layout->width || sy >= layout->height) continue;
        source = layout; sourceKey = key; sourceX = sx; sourceY = sy;
    }
    if (source && sourceKey && source->primaryTileset == current->primaryTileset
        && source->secondaryTileset == current->secondaryTileset && source->isFrlg == current->isFrlg
        && source->map[sourceY * source->width + sourceX] == block)
    {
        const struct RhhModSceneRule *rule = NULL;
        if (RhhModsSelectSceneRule(sourceKey, sourceX, sourceY, result.scene.tile.metatile,
                                   result.scene.tile.behavior, &rule)) return -1;
        result.origin = RHH_SCENE_WINDOW_CONNECTED; result.source_map = sourceKey;
        result.source_x = sourceX; result.source_y = sourceY;
        if (rule) { result.scene.rule = RhhContentKeyOf(rule->id); result.scene.style = rule->style; }
    }
    *out = result; return 0;
}
static int SceneResolvedTileAt(int32_t x, int32_t y, struct RhhSceneTile *out)
{
    struct RhhSceneWindowTile resolved = {.struct_size = sizeof(resolved)};
    if (SceneWindowResolve(x, y, &resolved)) return -1;
    *out = resolved.scene; return 0;
}
static int SceneWindowInfer(int32_t x, int32_t y, struct RhhSceneInference *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhSceneInference result = {.struct_size = sizeof(result), .tile.struct_size = sizeof(result.tile)};
    if (SceneWindowResolve(x, y, &result.tile)) return -1;
    struct RhhSceneTile *scene = &result.tile.scene;
    if (!scene->rule && (result.tile.origin == RHH_SCENE_WINDOW_CURRENT || result.tile.origin == RHH_SCENE_WINDOW_CONNECTED))
    {
        u32 behavior = scene->tile.behavior;
        if (behavior <= UINT8_MAX && (MetatileBehavior_IsSurfableWaterOrUnderwater(behavior)
            || MetatileBehavior_IsPuddle(behavior) || behavior == MB_SHALLOW_WATER))
        {
            scene->style.surface = RHH_SCENE_WATER; result.flags |= RHH_SCENE_INFERRED_WATER;
        }
        if (behavior <= UINT8_MAX && (MetatileBehavior_IsSignpost(behavior) || MetatileBehavior_IsPokemonCenterSign(behavior)
            || MetatileBehavior_IsPokeMartSign(behavior)))
        {
            u8 rgba[8 * 8 * 4];
            for (u32 i = 0; i < 8; ++i)
            {
                if (TileRgba(scene->tile.tiles[i], rgba, sizeof(rgba))) return -1;
                for (u32 pixel = 0; pixel < 64; ++pixel)
                    if (rgba[pixel * 4 + 3])
                    { if (i < 4) ++result.ground_pixels; else ++result.upper_pixels; }
            }
            result.flags |= RHH_SCENE_INFERENCE_SAMPLED;
            /* A transparent top layer over mostly opaque ground is the only
             * sign shape treated as a standing cutout. Ambiguous art stays flat. */
            if (result.ground_pixels >= 128 && result.upper_pixels >= 16 && result.upper_pixels <= 224)
            {
                scene->style.upper = RHH_SCENE_UPPER_BILLBOARD; scene->style.upper_height = 16;
                scene->style.flags |= RHH_SCENE_CASTS_SHADOW | RHH_SCENE_OCCLUDES;
                result.flags |= RHH_SCENE_INFERRED_SIGN;
            }
        }
    }
    *out = result; return 0;
}
static int SceneInferredTileAt(int32_t x, int32_t y, struct RhhSceneTile *out)
{
    struct RhhSceneInference inferred = {.struct_size = sizeof(inferred)};
    if (SceneWindowInfer(x, y, &inferred)) return -1;
    *out = inferred.tile.scene; return 0;
}
static void MeshVertex(struct RhhSceneQuad *quad, unsigned i, float x, float y, float z)
{ quad->vertex[i][0] = x; quad->vertex[i][1] = y; quad->vertex[i][2] = z; }
static void MeshHorizontal(struct RhhSceneMesh *mesh, u32 kind, u16 tile, float x, float y, float z)
{
    struct RhhSceneQuad *q = &mesh->quads[mesh->count++];
    q->kind = kind; q->tile = tile; q->flags = mesh->scene.style.flags; q->normal[2] = 1.f;
    MeshVertex(q, 0, x, y, z); MeshVertex(q, 1, x + 8, y, z);
    MeshVertex(q, 2, x + 8, y + 8, z); MeshVertex(q, 3, x, y + 8, z);
}
/* direction: north, south, west, east, or centered camera-facing billboard. */
static void MeshVertical(struct RhhSceneMesh *mesh, u32 kind, u16 tile, unsigned direction,
                         float x, float y, float start, float end, float top, float bottom)
{
    struct RhhSceneQuad *q = &mesh->quads[mesh->count++];
    q->kind = kind; q->tile = tile; q->flags = mesh->scene.style.flags;
    float x0, y0, x1, y1;
    if (direction < 2 || direction == 4)
    {
        x0 = x + start; x1 = x + end;
        y0 = y1 = y + (direction == 1 ? 16.f : direction == 4 ? 8.f : 0.f);
        if (direction < 2) q->normal[1] = direction ? 1.f : -1.f;
    }
    else
    {
        x0 = x1 = x + (direction == 3 ? 16.f : 0.f);
        y0 = y + start; y1 = y + end; q->normal[0] = direction == 3 ? 1.f : -1.f;
    }
    MeshVertex(q, 0, x0, y0, top); MeshVertex(q, 1, x1, y1, top);
    MeshVertex(q, 2, x1, y1, bottom); MeshVertex(q, 3, x0, y0, bottom);
}
static int BuildSceneMesh(const struct RhhSceneTile *scene, struct RhhSceneMesh *out)
{
    if (!scene || scene->struct_size < sizeof(*scene) || scene->tile.struct_size < sizeof(scene->tile)
        || !out || out->struct_size < sizeof(*out) || scene->reserved
        || scene->x < -1000000 || scene->x > 1000000 || scene->y < -1000000 || scene->y > 1000000)
        return -1;
    const struct RhhSceneStyle *style = &scene->style;
    if (style->surface > RHH_SCENE_WATER || style->upper > RHH_SCENE_UPPER_HIDDEN
        || (style->flags & ~(RHH_SCENE_RECEIVES_SHADOW | RHH_SCENE_CASTS_SHADOW | RHH_SCENE_OCCLUDES))
        || style->ground_height < -2048 || style->ground_height > 2048
        || style->upper_height < 0 || style->upper_height > 2048
        || ((style->upper == RHH_SCENE_UPPER_FLAT || style->upper == RHH_SCENE_UPPER_HIDDEN)
            ? style->upper_height != 0 : style->upper_height == 0)) return -1;
    struct RhhSceneMesh result = {.struct_size = sizeof(result), .scene = *scene};
    float x = scene->x * 16.f, y = scene->y * 16.f, ground = style->ground_height;
    for (unsigned i = 0; i < 4; ++i)
        MeshHorizontal(&result, RHH_SCENE_QUAD_GROUND, scene->tile.tiles[i], x + (i % 2) * 8, y + (i / 2) * 8, ground);
    if (style->upper != RHH_SCENE_UPPER_HIDDEN)
    {
        for (unsigned i = 0; i < 4; ++i)
        {
            if (style->upper == RHH_SCENE_UPPER_FLAT || style->upper == RHH_SCENE_UPPER_ROOF)
                MeshHorizontal(&result, RHH_SCENE_QUAD_UPPER, scene->tile.tiles[4 + i],
                    x + (i % 2) * 8, y + (i / 2) * 8,
                    ground + (style->upper == RHH_SCENE_UPPER_ROOF ? style->upper_height : 0));
            else
            {
                unsigned direction = style->upper == RHH_SCENE_UPPER_BILLBOARD ? 4 : style->upper - RHH_SCENE_UPPER_WALL_NORTH;
                float top = ground + style->upper_height * (i < 2 ? 1.f : .5f);
                float bottom = ground + style->upper_height * (i < 2 ? .5f : 0.f);
                MeshVertical(&result, direction == 4 ? RHH_SCENE_QUAD_BILLBOARD : RHH_SCENE_QUAD_UPPER,
                    scene->tile.tiles[4 + i], direction, x, y, (i % 2) * 8, (i % 2 + 1) * 8, top, bottom);
            }
        }
    }
    *out = result; return 0;
}
static int SceneMeshAtInternal(int32_t x, int32_t y, struct RhhSceneMesh *out, unsigned mode)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhSceneTile scene = {.struct_size = sizeof(scene)};
    struct RhhSceneMesh result = {.struct_size = sizeof(result)};
    struct RhhSceneWindow bounds = {.struct_size = sizeof(bounds)};
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)};
    if (Current(&world) || (mode && SceneWindow(&bounds))
        || (mode == 3 ? SceneInferredTileAt(x, y, &scene) : mode == 2 ? SceneResolvedTileAt(x, y, &scene)
            : mode ? SceneWindowTileAt(x, y, &scene) : SceneTileAt(x, y, &scene))
        || BuildSceneMesh(&scene, &result)) return -1;
    static const int dx[] = {0, 0, -1, 1}, dy[] = {-1, 1, 0, 0};
    static const u8 tileIndex[4][2] = {{0, 1}, {2, 3}, {0, 2}, {1, 3}};
    for (unsigned direction = 0; direction < 4; ++direction)
    {
        int nx = x + dx[direction], ny = y + dy[direction];
        if (mode ? (nx < bounds.min_x || nx > bounds.max_x || ny < bounds.min_y || ny > bounds.max_y)
                     : (nx < 0 || ny < 0 || (u32)nx >= world.width || (u32)ny >= world.height)) continue;
        struct RhhSceneTile neighbor = {.struct_size = sizeof(neighbor)};
        if (mode == 3 ? SceneInferredTileAt(nx, ny, &neighbor) : mode == 2 ? SceneResolvedTileAt(nx, ny, &neighbor)
            : mode ? SceneWindowTileAt(nx, ny, &neighbor) : SceneTileAt(nx, ny, &neighbor)) return -1;
        if (neighbor.style.ground_height >= scene.style.ground_height) continue;
        for (unsigned i = 0; i < 2; ++i)
            MeshVertical(&result, RHH_SCENE_QUAD_GROUND_EDGE, scene.tile.tiles[tileIndex[direction][i]],
                direction, x * 16.f, y * 16.f, i * 8.f, (i + 1) * 8.f,
                scene.style.ground_height, neighbor.style.ground_height);
    }
    if (mode == 3 && !scene.rule && scene.style.upper == RHH_SCENE_UPPER_BILLBOARD)
        for (u32 i = 0; i < result.count; ++i)
            if (result.quads[i].kind == RHH_SCENE_QUAD_GROUND || result.quads[i].kind == RHH_SCENE_QUAD_GROUND_EDGE)
                result.quads[i].flags &= ~RHH_SCENE_CASTS_SHADOW;
    *out = result; return 0;
}
static int SceneMeshAt(int32_t x, int32_t y, struct RhhSceneMesh *out)
{ return SceneMeshAtInternal(x, y, out, FALSE); }
static int SceneWindowMeshAt(int32_t x, int32_t y, struct RhhSceneMesh *out)
{ return SceneMeshAtInternal(x, y, out, 1); }
static int SceneWindowResolvedMeshAt(int32_t x, int32_t y, struct RhhSceneMesh *out)
{ return SceneMeshAtInternal(x, y, out, 2); }
static int SceneWindowInferredMeshAt(int32_t x, int32_t y, struct RhhSceneMesh *out)
{ return SceneMeshAtInternal(x, y, out, 3); }
static int SceneLightsAt(int32_t x, int32_t y, uint32_t period, struct RhhSceneLightBatch *out)
{
    if (!out || out->struct_size < sizeof(*out) || period > RHH_TIME_NIGHT) return -1;
    struct RhhSceneWindow bounds = {.struct_size = sizeof(bounds)};
    if (SceneWindow(&bounds) || x < bounds.min_x || x > bounds.max_x || y < bounds.min_y || y > bounds.max_y) return -1;
    struct RhhSceneLightBatch result = {.struct_size = sizeof(result)};
    const struct RhhModSceneLight *lights[RHH_SCENE_LIGHTS_PER_TILE]; uint32_t count = 0;
    bool32 current = x >= 0 && y >= 0 && x < gMapHeader.mapLayout->width && y < gMapHeader.mapLayout->height;
    /* Empty current-map tiles need no graphics decode or scene-rule search. */
    if (current)
    {
        if (RhhModsSceneLightsForTile(bounds.map, x, y, period, lights, &count)) return -1;
        if (!count) { *out = result; return 0; }
    }
    struct RhhSceneWindowTile tile = {.struct_size = sizeof(tile)};
    if (SceneWindowResolve(x, y, &tile)) return -1;
    if (tile.origin == RHH_SCENE_WINDOW_CURRENT || tile.origin == RHH_SCENE_WINDOW_CONNECTED)
    {
        if (!current && RhhModsSceneLightsForTile(tile.source_map, tile.source_x, tile.source_y, period, lights, &count)) return -1;
        for (unsigned i = 0; i < count; ++i)
        {
            const struct RhhModSceneLight *light = lights[i];
            result.lights[i] = (struct RhhSceneLight){.id = RhhContentKeyOf(light->id), .source_map = tile.source_map,
                .x = x * 16 + light->x % 16, .y = y * 16 + light->y % 16,
                .z = tile.scene.style.ground_height + light->height, .radius = light->radius,
                .rgb = light->rgb, .intensity = light->intensity};
        }
        result.count = count;
    }
    *out = result; return 0;
}
struct SceneTraceContext {
    struct RhhSceneWindow bounds;
    struct RhhSceneTraceResult result;
    float from[3], to[3], best;
    bool32 alpha, inferred;
};
static int SceneTraceVisit(int32_t x, int32_t y, void *user)
{
    struct SceneTraceContext *ctx = user;
    if (x < ctx->bounds.min_x || x > ctx->bounds.max_x || y < ctx->bounds.min_y || y > ctx->bounds.max_y) return 0;
    struct RhhSceneMesh mesh = {.struct_size = sizeof(mesh)};
    if (ctx->inferred ? SceneWindowInferredMeshAt(x, y, &mesh) : SceneWindowResolvedMeshAt(x, y, &mesh)) return -1;
    for (unsigned i = 0; i < mesh.count; ++i)
    {
        const struct RhhSceneQuad *quad = &mesh.quads[i]; float t;
        if (!(quad->flags & RHH_SCENE_CASTS_SHADOW) || !RhhSceneRayQuad(quad, ctx->from, ctx->to, UINT64_MAX, 0, &t)) continue;
        if (t > ctx->best + .0000001f) continue;
        if (ctx->alpha)
        {
            uint8_t rgba[8 * 8 * 4]; uint64_t bits = 0;
            if (TileRgba(quad->tile, rgba, sizeof(rgba))) return -1;
            for (unsigned pixel = 0; pixel < 64; ++pixel)
                if (rgba[pixel * 4 + 3] >= 128) bits |= UINT64_C(1) << pixel;
            if (!RhhSceneRayQuad(quad, ctx->from, ctx->to, bits, 1, &t)) continue;
        }
        if (ctx->result.status == RHH_SCENE_TRACE_BLOCKED && fabsf(t - ctx->best) <= .0000001f
            && (y > ctx->result.tile_y || (y == ctx->result.tile_y && (x > ctx->result.tile_x
                || (x == ctx->result.tile_x && i >= ctx->result.quad_index))))) continue;
        ctx->best = t; ctx->result.status = RHH_SCENE_TRACE_BLOCKED;
        ctx->result.tile_x = x; ctx->result.tile_y = y; ctx->result.quad_index = i;
        ctx->result.quad_kind = quad->kind; ctx->result.tile = quad->tile;
        ctx->result.fraction_millionths = (uint32_t)(t * 1000000.f + .5f);
    }
    return 0;
}
static int SceneTraceFinish(struct SceneTraceContext *ctx)
{
    ctx->result.map = ctx->bounds.map;
    return RhhSceneRayCells(ctx->from[0], ctx->from[1], ctx->to[0], ctx->to[1], SceneTraceVisit, ctx);
}
static int SceneTraceLightInternal(const struct RhhSceneTraceRequest *request, struct RhhSceneTraceResult *out, bool32 inferred)
{
    if (!request || request->struct_size < sizeof(*request) || (request->flags & ~RHH_SCENE_TRACE_ALPHA)
        || !out || out->struct_size < sizeof(*out)) return -1;
    struct SceneTraceContext ctx = {.bounds.struct_size = sizeof(ctx.bounds),
        .result = {.struct_size = sizeof(ctx.result), .fraction_millionths = 1000000}, .best = 1.f,
        .alpha = !!(request->flags & RHH_SCENE_TRACE_ALPHA), .inferred = inferred};
    if (SceneWindow(&ctx.bounds)) return -1;
    for (unsigned i = 0; i < 2; ++i)
    {
        int64_t lower = (int64_t)(i ? ctx.bounds.min_y : ctx.bounds.min_x) * 16;
        int64_t upper = ((int64_t)(i ? ctx.bounds.max_y : ctx.bounds.max_x) + 1) * 16;
        if (request->from[i] < lower || request->from[i] >= upper || request->to[i] < lower || request->to[i] >= upper
            || llabs((int64_t)request->to[i] - request->from[i]) > 512) return -1;
    }
    if (request->from[2] < -4096 || request->from[2] > 4096 || request->to[2] < -4096 || request->to[2] > 4096) return -1;
    for (unsigned i = 0; i < 3; ++i) { ctx.from[i] = request->from[i]; ctx.to[i] = request->to[i]; }
    if (SceneTraceFinish(&ctx)) return -1;
    *out = ctx.result; return 0;
}
static int SceneTraceLight(const struct RhhSceneTraceRequest *request, struct RhhSceneTraceResult *out)
{ return SceneTraceLightInternal(request, out, FALSE); }
static int SceneTraceInferredLight(const struct RhhSceneTraceRequest *request, struct RhhSceneTraceResult *out)
{ return SceneTraceLightInternal(request, out, TRUE); }
static int SceneTraceDirectionalLight(const struct RhhSceneDirectionalTraceRequest *request, struct RhhSceneDirectionalTraceResult *out)
{
    if (!request || request->struct_size < sizeof(*request) || !out || out->struct_size < sizeof(*out)
        || request->reserved[0] || request->reserved[1]
        || (request->flags & ~(RHH_SCENE_TRACE_ALPHA | RHH_SCENE_DIRECTIONAL_INFERRED))
        || !isfinite(request->max_distance) || request->max_distance <= 0 || request->max_distance > 512) return -1;
    struct SceneTraceContext ctx = {.bounds.struct_size = sizeof(ctx.bounds),
        .result = {.struct_size = sizeof(ctx.result), .fraction_millionths = 1000000}, .best = 1.f,
        .alpha = !!(request->flags & RHH_SCENE_TRACE_ALPHA), .inferred = !!(request->flags & RHH_SCENE_DIRECTIONAL_INFERRED)};
    if (SceneWindow(&ctx.bounds)) return -1;
    double direction[3], lengthSquared = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (!isfinite(request->from[i]) || !isfinite(request->direction[i]) || fabs(request->direction[i]) > 1048576) return -1;
        direction[i] = request->direction[i]; lengthSquared += direction[i] * direction[i];
    }
    if (lengthSquared < 1e-12) return -1;
    double length = sqrt(lengthSquared);
    for (unsigned i = 0; i < 3; ++i) direction[i] /= length;
    float lower[3] = {ctx.bounds.min_x * 16.f, ctx.bounds.min_y * 16.f, -4096.f};
    float upper[3] = {nextafterf((ctx.bounds.max_x + 1) * 16.f, -INFINITY),
                      nextafterf((ctx.bounds.max_y + 1) * 16.f, -INFINITY), 4096.f};
    double distance = request->max_distance; u32 coverage = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (request->from[i] < lower[i] || request->from[i] > upper[i]) return -1;
        double limit = direction[i] > 0 ? (upper[i] - (double)request->from[i]) / direction[i]
            : direction[i] < 0 ? (lower[i] - (double)request->from[i]) / direction[i] : INFINITY;
        u32 flag = i == 2 ? RHH_SCENE_DIRECTIONAL_HEIGHT_CLIPPED : RHH_SCENE_DIRECTIONAL_WINDOW_CLIPPED;
        if (limit < distance - 1e-9) { distance = fmax(0, limit); coverage = flag; }
        else if (limit < request->max_distance && fabs(limit - distance) <= 1e-9) coverage |= flag;
    }
    struct RhhSceneDirectionalTraceResult result = {.struct_size = sizeof(result), .coverage = coverage};
    double evaluatedSquared = 0;
    for (unsigned i = 0; i < 3; ++i) {
        ctx.from[i] = request->from[i];
        ctx.to[i] = fmaxf(lower[i], fminf(upper[i], request->from[i] + direction[i] * distance));
        result.end[i] = ctx.to[i];
        double delta = (double)ctx.to[i] - ctx.from[i]; evaluatedSquared += delta * delta;
    }
    result.distance = sqrt(evaluatedSquared);
    ctx.result.map = ctx.bounds.map;
    if (result.distance > 0 && SceneTraceFinish(&ctx)) return -1;
    result.hit = ctx.result; *out = result; return 0;
}
static int SceneTraceDirectionalShadow(const struct RhhSceneShadowTraceRequest *request, struct RhhSceneShadowTraceResult *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved[0] || request->reserved[1]
        || request->caster_count > RHH_SCENE_SHADOW_CASTER_LIMIT
        || (!!request->caster_count != !!request->casters) || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhSceneShadowTraceResult result = {.struct_size = sizeof(result), .caster_index = UINT32_MAX,
        .fraction_millionths = 1000000, .ray.struct_size = sizeof(result.ray),
        .billboard = {.struct_size = sizeof(result.billboard), .status = RHH_SCENE_TRACE_CLEAR,
            .fraction_millionths = 1000000, .pixel_x = UINT32_MAX, .pixel_y = UINT32_MAX}};
    if (SceneTraceDirectionalLight(&request->ray, &result.ray)) return -1;
    if (result.ray.hit.status == RHH_SCENE_TRACE_BLOCKED) {
        result.kind = RHH_SCENE_SHADOW_MESH;
        result.fraction_millionths = result.ray.hit.fraction_millionths;
    }
    for (u32 i = 0; i < request->caster_count; ++i)
    {
        const struct RhhSceneShadowCaster *caster = &request->casters[i];
        if (caster->struct_size < sizeof(*caster) || !caster->id || caster->reserved[0] || caster->reserved[1]) return -1;
        for (u32 j = 0; j < i; ++j) if (request->casters[j].id == caster->id) return -1;
        struct RhhSceneBillboardTraceRequest ray = {.struct_size = sizeof(ray), .flags = caster->flags,
            .width = caster->width, .height = caster->height, .stride = caster->stride,
            .rgba_size = caster->rgba_size, .rgba = caster->rgba};
        memcpy(ray.from, request->ray.from, sizeof(ray.from));
        memcpy(ray.to, result.ray.end, sizeof(ray.to));
        memcpy(ray.corners, caster->corners, sizeof(ray.corners));
        struct RhhSceneBillboardTraceResult hit = {.struct_size = sizeof(hit)};
        if (RhhSceneTraceBillboard(&ray, &hit)) return -1;
        if (hit.status != RHH_SCENE_TRACE_BLOCKED || hit.fraction_millionths > result.fraction_millionths
            || (hit.fraction_millionths == result.fraction_millionths
                && (result.kind != RHH_SCENE_SHADOW_BILLBOARD || caster->id >= result.caster_id))) continue;
        result.kind = RHH_SCENE_SHADOW_BILLBOARD; result.caster_index = i; result.caster_id = caster->id;
        result.fraction_millionths = hit.fraction_millionths; result.billboard = hit;
    }
    *out = result; return 0;
}
struct PerceptionEndpoint {
    u32 slot;
    const struct ObjectEvent *object;
    struct RhhWildActorSnapshot wild;
    struct RhhWildActorIndividual individual;
};
static int PerceptionEndpoint(const struct RhhPerceptionEndpoint *endpoint, struct PerceptionEndpoint *out)
{
    if (endpoint->reserved) return -1;
    *out = (struct PerceptionEndpoint){.wild.struct_size = sizeof(out->wild),
        .individual.struct_size = sizeof(out->individual)};
    if (endpoint->kind == RHH_PERCEPTION_PLAYER)
    {
        if (endpoint->wild_handle || gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT) return -1;
        out->slot = gPlayerAvatar.objectEventId; out->object = &gObjectEvents[out->slot];
        if (!out->object->active || !out->object->isPlayer) return -1;
    }
    else if (endpoint->kind == RHH_PERCEPTION_WILD)
    {
        if (WildActorFind(endpoint->wild_handle, &out->wild)) return -1;
        out->slot = out->wild.slot; out->object = &gObjectEvents[out->slot];
        WildActorIndividualAt(out->slot, &out->individual);
    }
    else return -1;
    const struct ObjectEvent *object = out->object;
    s32 x = object->currentCoords.x - MAP_OFFSET, y = object->currentCoords.y - MAP_OFFSET;
    if ((!object->isPlayer && (object->mapGroup != gSaveBlock1Ptr->location.mapGroup
            || object->mapNum != gSaveBlock1Ptr->location.mapNum))
        || x < 0 || y < 0 || x >= gMapHeader.mapLayout->width || y >= gMapHeader.mapLayout->height) return -1;
    return 0;
}
static bool32 PerceptionFaces(const struct ObjectEvent *viewer, s32 dx, s32 dy)
{
    if (!dx && !dy) return TRUE;
    switch (viewer->facingDirection)
    {
    case DIR_NORTH: return dy < 0 && abs(dx) <= -dy;
    case DIR_SOUTH: return dy > 0 && abs(dx) <= dy;
    case DIR_WEST: return dx < 0 && abs(dy) <= -dx;
    case DIR_EAST: return dx > 0 && abs(dy) <= dx;
    default: return FALSE;
    }
}
static int PerceptionTraceTile(s32 x, s32 y, const struct PerceptionEndpoint *viewer,
                               const struct PerceptionEndpoint *target, struct RhhPerceptionResult *result)
{
    if ((x == result->viewer_x && y == result->viewer_y) || (x == result->target_x && y == result->target_y)) return 0;
    u32 blockers = 0, actorSlot = UINT32_MAX; RhhContentKey rule = 0;
    if (result->checks & RHH_PERCEPTION_CHECK_SCENE)
    {
        struct RhhSceneTile scene = {.struct_size = sizeof(scene)};
        if (SceneTileAt(x, y, &scene)) return -1;
        if (!scene.rule) result->flags |= RHH_PERCEPTION_SCENE_UNANNOTATED;
        if (scene.style.flags & RHH_SCENE_OCCLUDES)
        { blockers |= RHH_PERCEPTION_BLOCK_SCENE; rule = scene.rule; }
    }
    if ((result->checks & RHH_PERCEPTION_CHECK_COLLISION)
        && MapGridGetCollisionAt(x + MAP_OFFSET, y + MAP_OFFSET)) blockers |= RHH_PERCEPTION_BLOCK_COLLISION;
    if (result->checks & RHH_PERCEPTION_CHECK_ACTORS)
        for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
        {
            const struct ObjectEvent *object = &gObjectEvents[i];
            if (i == viewer->slot || i == target->slot || !object->active || object->invisible
                || ModsWildActorHidden(object)
                || !AreElevationsCompatible(object->currentElevation, viewer->object->currentElevation)) continue;
            if ((object->currentCoords.x == x + MAP_OFFSET && object->currentCoords.y == y + MAP_OFFSET)
                || (object->previousCoords.x == x + MAP_OFFSET && object->previousCoords.y == y + MAP_OFFSET))
            { blockers |= RHH_PERCEPTION_BLOCK_ACTOR; actorSlot = i; break; }
        }
    if (blockers && !result->blocker_flags)
    { result->blocker_flags = blockers; result->blocker_x = x; result->blocker_y = y;
      result->blocker_actor_slot = actorSlot; result->blocker_scene_rule = rule; }
    return 0;
}
static int PerceptionQuery(const struct RhhPerceptionRequest *request, struct RhhPerceptionResult *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || request->max_distance > 64
        || (request->checks & ~(RHH_PERCEPTION_CHECK_SCENE | RHH_PERCEPTION_CHECK_COLLISION
            | RHH_PERCEPTION_CHECK_ACTORS | RHH_PERCEPTION_CHECK_FACING))
        || !out || out->struct_size < sizeof(*out) || !Ready() || !RhhModsHasActiveRun()) return -1;
    struct PerceptionEndpoint viewer, target;
    if (PerceptionEndpoint(&request->viewer, &viewer) || PerceptionEndpoint(&request->target, &target)
        || viewer.slot == target.slot) return -1;
    struct RhhPerceptionResult result = {.struct_size = sizeof(result), .checks = request->checks,
        .map = RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8)
            | (u8)gSaveBlock1Ptr->location.mapNum),
        .viewer_x = viewer.object->currentCoords.x - MAP_OFFSET, .viewer_y = viewer.object->currentCoords.y - MAP_OFFSET,
        .target_x = target.object->currentCoords.x - MAP_OFFSET, .target_y = target.object->currentCoords.y - MAP_OFFSET,
        .viewer_elevation = viewer.object->currentElevation, .target_elevation = target.object->currentElevation,
        .viewer_facing = viewer.object->facingDirection, .target_cover = WildConcealCover(target.object),
        .blocker_actor_slot = UINT32_MAX, .viewer_wild = viewer.wild, .target_wild = target.wild,
        .viewer_individual = viewer.individual, .target_individual = target.individual};
    if (!result.map) return -1;
    if (request->target.kind == RHH_PERCEPTION_WILD)
        result.target_conceal_mode = sWildActors[target.slot].conceal.mode;
    if (target.object->invisible || ModsWildActorHidden(target.object)) result.flags |= RHH_PERCEPTION_TARGET_HIDDEN;
    if (target.object->offScreen) result.flags |= RHH_PERCEPTION_TARGET_OFFSCREEN;
    const struct Sprite *sprite = ActorSprite(target.slot);
    if (!sprite) result.flags |= RHH_PERCEPTION_TARGET_NO_SPRITE;
    else if (sprite->invisible) result.flags |= RHH_PERCEPTION_TARGET_SPRITE_HIDDEN;
    s32 dx = result.target_x - result.viewer_x, dy = result.target_y - result.viewer_y;
    s32 ax = abs(dx), ay = abs(dy); result.distance = ax + ay;
    if (result.distance > request->max_distance) result.flags |= RHH_PERCEPTION_OUT_OF_RANGE;
    if (!AreElevationsCompatible(viewer.object->currentElevation, target.object->currentElevation))
        result.flags |= RHH_PERCEPTION_ELEVATION_MISMATCH;
    if ((request->checks & RHH_PERCEPTION_CHECK_FACING) && !PerceptionFaces(viewer.object, dx, dy))
        result.flags |= RHH_PERCEPTION_OUTSIDE_FACING;
    if (!(result.flags & RHH_PERCEPTION_OUT_OF_RANGE))
    {
        s32 x = result.viewer_x, y = result.viewer_y, sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
        s32 ix = 0, iy = 0;
        while (ix < ax || iy < ay)
        {
            s32 horizontal = (1 + 2 * ix) * ay, vertical = (1 + 2 * iy) * ax;
            if (horizontal == vertical)
            {
                /* At exact corners, test both adjacent tiles before crossing. */
                if (PerceptionTraceTile(x + sx, y, &viewer, &target, &result)
                    || PerceptionTraceTile(x, y + sy, &viewer, &target, &result)) return -1;
                x += sx; y += sy; ++ix; ++iy;
            }
            else if (horizontal < vertical) { x += sx; ++ix; }
            else { y += sy; ++iy; }
            if (PerceptionTraceTile(x, y, &viewer, &target, &result)) return -1;
        }
    }
    if (!(result.flags & (RHH_PERCEPTION_OUT_OF_RANGE | RHH_PERCEPTION_ELEVATION_MISMATCH
                          | RHH_PERCEPTION_OUTSIDE_FACING)) && !result.blocker_flags)
        result.flags |= RHH_PERCEPTION_LINE_CLEAR;
    if ((result.flags & RHH_PERCEPTION_LINE_CLEAR) && !(result.flags & RHH_PERCEPTION_TARGET_HIDDEN))
        result.flags |= RHH_PERCEPTION_PERCEIVABLE;
    *out = result; return 0;
}
static const struct RhhPerceptionService sPerceptionService = {
    {sizeof(sPerceptionService), RHH_SERVICE_VERSION(1, 0)}, PerceptionQuery
};

static struct {
    bool8 pending, geometric, quality;
    u64 nextTicket, step;
    u32 spriteId;
    struct RhhProjectedVisibilityResult result;
    struct RhhPerceptionResult sight;
    struct RhhWorldActor actor;
    struct RhhWorldCamera camera;
    struct RhhWorldBillboard billboard;
    struct RhhWorldActorVisibility visibility;
} sProjectedProbe;
static bool32 ProjectedProbeMatches(void)
{
    if (!RhhModsHasActiveRun() || sProjectedProbe.result.run_id != RhhModsRunId() || !Ready() || NativeSaveTransactionActive()
        || gMain.callback2 != CB2_Overworld || gPaletteFade.active || gPaletteFade.y
        || ScriptContext_IsEnabled() || ArePlayerFieldControlsLocked() || IsOverworldLinkActive()) return FALSE;
    struct RhhPerceptionRequest request = {.struct_size = sizeof(request), .max_distance = 64,
        .viewer = {RHH_PERCEPTION_PLAYER, 0, 0}, .target = {RHH_PERCEPTION_WILD, 0, sProjectedProbe.result.actor_handle}};
    struct RhhPerceptionResult sight = {.struct_size = sizeof(sight)};
    struct RhhWorldActor actor = {.struct_size = sizeof(actor)};
    struct RhhWorldCamera camera = {.struct_size = sizeof(camera)};
    struct RhhWorldBillboard billboard = {.struct_size = sizeof(billboard)};
    struct RhhWorldActorVisibility visibility = {.struct_size = sizeof(visibility)};
    const struct RhhWorldClockService *clock = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldClockSnapshot now = {.struct_size = sizeof(now)};
    if (PerceptionQuery(&request, &sight) || !(sight.flags & RHH_PERCEPTION_PERCEIVABLE)
        || sight.flags & RHH_PERCEPTION_TARGET_NO_SPRITE || sight.map != sProjectedProbe.sight.map
        || sight.target_wild.slot != sProjectedProbe.sight.target_wild.slot
        || sight.target_wild.species != sProjectedProbe.sight.target_wild.species
        || sight.viewer_x != sProjectedProbe.sight.viewer_x || sight.viewer_y != sProjectedProbe.sight.viewer_y
        || sight.target_x != sProjectedProbe.sight.target_x || sight.target_y != sProjectedProbe.sight.target_y
        || !clock || clock->current(&now) || now.run_id != sProjectedProbe.result.run_id || now.steps != sProjectedProbe.step
        || ActorAt(sight.target_wild.slot, &actor) || Camera(&camera) || Billboard(sight.target_wild.slot, &billboard)
        || ActorVisibility(sight.target_wild.slot, &visibility)
        || gObjectEvents[sight.target_wild.slot].spriteId != sProjectedProbe.spriteId) return FALSE;
    actor.animation = sProjectedProbe.actor.animation; actor.animation_frame = sProjectedProbe.actor.animation_frame;
    return !memcmp(&actor, &sProjectedProbe.actor, sizeof(actor)) && !memcmp(&camera, &sProjectedProbe.camera, sizeof(camera))
        && !memcmp(&billboard, &sProjectedProbe.billboard, sizeof(billboard))
        && !memcmp(&visibility, &sProjectedProbe.visibility, sizeof(visibility));
}
static int ProjectedProbeRequestInternal(const struct RhhProjectedVisibilityRequest *request,
                                         struct RhhProjectedVisibilityResult *out, bool32 geometric)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_PROJECTED_INVALID;
    struct RhhProjectedVisibilityResult result = {.struct_size = sizeof(result), .status = RHH_PROJECTED_INVALID};
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->owner
        || !RhhModsRegisteredKey(request->owner) || !request->actor_handle) { *out = result; return result.status; }
    struct RhhProjectedVisibilityRequest copy = *request; request = &copy;
    if (sProjectedProbe.pending || sProjectedProbe.nextTicket == UINT64_MAX || !WildActorCommandReady() || NativeSaveTransactionActive())
    { result.status = RHH_PROJECTED_UNAVAILABLE; *out = result; return result.status; }
    struct RhhPerceptionRequest sightRequest = {.struct_size = sizeof(sightRequest), .max_distance = 64,
        .viewer = {RHH_PERCEPTION_PLAYER, 0, 0}, .target = {RHH_PERCEPTION_WILD, 0, request->actor_handle}};
    struct RhhPerceptionResult sight = {.struct_size = sizeof(sight)};
    struct RhhWorldActor actor = {.struct_size = sizeof(actor)};
    struct RhhWorldCamera camera = {.struct_size = sizeof(camera)};
    struct RhhWorldBillboard billboard = {.struct_size = sizeof(billboard)};
    struct RhhWorldActorVisibility visibility = {.struct_size = sizeof(visibility)};
    const struct RhhWorldClockService *clock = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldClockSnapshot now = {.struct_size = sizeof(now)};
    if (PerceptionQuery(&sightRequest, &sight) || !(sight.flags & RHH_PERCEPTION_PERCEIVABLE)
        || sight.flags & RHH_PERCEPTION_TARGET_NO_SPRITE || !clock || clock->current(&now)
        || ActorAt(sight.target_wild.slot, &actor) || Camera(&camera) || Billboard(sight.target_wild.slot, &billboard)
        || ActorVisibility(sight.target_wild.slot, &visibility))
    { result.status = RHH_PROJECTED_UNAVAILABLE; *out = result; return result.status; }
    result.status = RHH_PROJECTED_PENDING; result.owner = request->owner; result.actor_handle = request->actor_handle;
    result.ticket = ++sProjectedProbe.nextTicket; result.run_id = now.run_id;
    sProjectedProbe.pending = TRUE; sProjectedProbe.geometric = geometric; sProjectedProbe.result = result; sProjectedProbe.step = now.steps;
    sProjectedProbe.spriteId = gObjectEvents[sight.target_wild.slot].spriteId;
    sProjectedProbe.sight = sight; sProjectedProbe.actor = actor; sProjectedProbe.camera = camera;
    sProjectedProbe.billboard = billboard; sProjectedProbe.visibility = visibility; *out = result;
    return RHH_PROJECTED_PENDING;
}
static int ProjectedProbeRequest(const struct RhhProjectedVisibilityRequest *request, struct RhhProjectedVisibilityResult *out)
{ return ProjectedProbeRequestInternal(request, out, FALSE); }
static int ProjectedProbeRequestGeometric(const struct RhhProjectedVisibilityRequest *request, struct RhhProjectedVisibilityResult *out)
{ return ProjectedProbeRequestInternal(request, out, TRUE); }
static int ProjectedProbeResult(u64 ticket, struct RhhProjectedVisibilityResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ticket || ticket != sProjectedProbe.result.ticket) return RHH_PROJECTED_INVALID;
    struct RhhProjectedVisibilityResult result = sProjectedProbe.result;
    if (!ProjectedProbeMatches()) result.status = RHH_PROJECTED_STALE;
    *out = result; return result.status;
}
static int ProjectedProbeCancel(RhhContentKey owner, u64 ticket)
{
    if (!RhhModsCommandsAllowed() || !owner || owner != sProjectedProbe.result.owner
        || !ticket || !sProjectedProbe.pending || ticket != sProjectedProbe.result.ticket) return -1;
    sProjectedProbe.pending = FALSE; sProjectedProbe.result.status = RHH_PROJECTED_UNAVAILABLE; return 0;
}
static int ProjectedProbeQuality(u64 ticket, u32 *quality)
{
    if (!quality || !ticket || ticket != sProjectedProbe.result.ticket || sProjectedProbe.pending
        || sProjectedProbe.result.status != RHH_PROJECTED_OK || !ProjectedProbeMatches()) return -1;
    *quality = sProjectedProbe.quality; return 0;
}
int RhhModsProjectedVisibilityTarget(uint32_t *slot, uint64_t *ticket)
{
    if (!slot || !ticket || !sProjectedProbe.pending) return -1;
    if (!ProjectedProbeMatches()) { sProjectedProbe.pending = FALSE; sProjectedProbe.result.status = RHH_PROJECTED_STALE; return -1; }
    *slot = sProjectedProbe.sight.target_wild.slot; *ticket = sProjectedProbe.result.ticket; return 0;
}
void RhhModsProjectedVisibilityFinish(const u16 *pixels, u32 frame, int shader, int world)
{
    if (!sProjectedProbe.pending) return;
    sProjectedProbe.pending = FALSE; sProjectedProbe.result.frame = frame;
    if (!ProjectedProbeMatches()) sProjectedProbe.result.status = RHH_PROJECTED_STALE;
    else if (!shader || !world) sProjectedProbe.result.status = RHH_PROJECTED_UNAVAILABLE;
    else if (!NativeShaderProjectedMask(sProjectedProbe.result.ticket, pixels, frame,
                 &sProjectedProbe.result.total_pixels, &sProjectedProbe.result.visible_pixels))
    { sProjectedProbe.quality = RHH_PHOTO_MASK_EXACT; sProjectedProbe.result.status = RHH_PROJECTED_OK; }
    else if (sProjectedProbe.geometric && !NativeShaderGeometricMask(sProjectedProbe.result.ticket, pixels, frame,
                 &sProjectedProbe.result.total_pixels, &sProjectedProbe.result.visible_pixels, NULL, NULL))
    { sProjectedProbe.quality = RHH_PHOTO_MASK_GEOMETRIC; sProjectedProbe.result.status = RHH_PROJECTED_OK; }
    else sProjectedProbe.result.status = RHH_PROJECTED_UNAVAILABLE;
}
static const struct RhhProjectedVisibilityService sProjectedVisibilityService = {
    {sizeof(sProjectedVisibilityService), RHH_SERVICE_VERSION(1, 1)}, ProjectedProbeRequest, ProjectedProbeResult,
    ProjectedProbeCancel, ProjectedProbeRequestGeometric, ProjectedProbeQuality
};

/* The journal is deliberately an ordinary save record, not a durable reward
 * journal. IDs belong to the saved actor lineage, not OT/personality pairs. */
enum { SCOUT_RECORDS = RHH_SCOUT_MAX_RECORDS, SCOUT_RECORD_SIZE = 48, SCOUT_STATE_SIZE = SCOUT_RECORDS * SCOUT_RECORD_SIZE };
static const char sScoutingStateId[] = "rhh:scouting_state";
static const struct RhhScoutingResult *sScoutingContext;
static bool8 sScoutingBusy;
static u64 sScoutingPhotoTicket;
static struct {
    bool8 valid, used, quality;
    u64 ticket, run, step;
    u32 spriteId;
    struct RhhPerceptionResult sight;
    struct RhhWorldActor actor;
    struct RhhWorldCamera camera;
    struct RhhWorldBillboard billboard;
    struct RhhWorldActorVisibility visibility;
} sScoutPhotoProof;
static int ScoutPhotoContext(u64 *ticket)
{
    u32 event = RhhModsCurrentEvent();
    if (!ticket || !sScoutingContext || !sScoutingPhotoTicket
        || (event != RHH_CAN_SCOUT && event != RHH_SCOUT_RECORDED)) return -1;
    *ticket = sScoutingPhotoTicket; return 0;
}
static int ScoutPhotoQuality(u32 *quality)
{
    u64 ticket;
    if (!quality || ScoutPhotoContext(&ticket) || !sScoutPhotoProof.valid || ticket != sScoutPhotoProof.ticket) return -1;
    *quality = sScoutPhotoProof.quality; return 0;
}
static bool32 ScoutPhotoMatches(u64 ticket, const struct RhhPerceptionResult *sight)
{
    const struct RhhWorldClockService *clock = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldClockSnapshot now = {.struct_size = sizeof(now)};
    struct RhhWorldCamera camera = {.struct_size = sizeof(camera)};
    struct RhhWorldBillboard billboard = {.struct_size = sizeof(billboard)};
    struct RhhWorldActorVisibility visibility = {.struct_size = sizeof(visibility)};
    struct RhhWorldActor actor = {.struct_size = sizeof(actor)};
    const struct RhhPerceptionResult *old = &sScoutPhotoProof.sight;
    if (!sScoutPhotoProof.valid || !sight || ActorAt(sight->target_wild.slot, &actor)) return FALSE;
    /* A photo is evidence of an earlier frame; idle animation may advance
     * without changing the subject or its position. */
    actor.animation = sScoutPhotoProof.actor.animation;
    actor.animation_frame = sScoutPhotoProof.actor.animation_frame;
    return ticket == sScoutPhotoProof.ticket && RhhModsRunId() == sScoutPhotoProof.run
        && sight && (sight->flags & RHH_PERCEPTION_PERCEIVABLE) && !(sight->flags & RHH_PERCEPTION_TARGET_NO_SPRITE)
        && sight->map == old->map && sight->target_wild.handle == old->target_wild.handle
        && sight->target_wild.slot == old->target_wild.slot && sight->target_wild.species == old->target_wild.species
        && sight->target_individual.run_id == old->target_individual.run_id
        && sight->target_individual.serial == old->target_individual.serial
        && sight->viewer_x == old->viewer_x && sight->viewer_y == old->viewer_y
        && sight->target_x == old->target_x && sight->target_y == old->target_y
        && clock && !clock->current(&now) && now.run_id == sScoutPhotoProof.run && now.steps == sScoutPhotoProof.step
        && !Camera(&camera) && !Billboard(sight->target_wild.slot, &billboard)
        && !ActorVisibility(sight->target_wild.slot, &visibility)
        && gObjectEvents[sight->target_wild.slot].spriteId == sScoutPhotoProof.spriteId
        && !memcmp(&actor, &sScoutPhotoProof.actor, sizeof(actor))
        && !memcmp(&camera, &sScoutPhotoProof.camera, sizeof(camera))
        && !memcmp(&billboard, &sScoutPhotoProof.billboard, sizeof(billboard))
        && !memcmp(&visibility, &sScoutPhotoProof.visibility, sizeof(visibility));
}
static u32 ScoutRead32(const u8 *p)
{ return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static void ScoutWrite32(u8 *p, u32 value)
{ for (u32 i = 0; i < 4; ++i) p[i] = value >> (i * 8); }
static int ValidateScoutingState(const u8 *state, u32 size)
{
    if (size != SCOUT_STATE_SIZE) return -1;
    bool32 empty = FALSE;
    u64 firstRun = 0;
    for (u32 i = 0; i < SCOUT_RECORDS; ++i)
    {
        const u8 *p = state + i * SCOUT_RECORD_SIZE;
        u64 run = WildActorRead64(p), serial = WildActorRead64(p + 8);
        if (!run)
        {
            empty = TRUE;
            for (u32 j = 0; j < SCOUT_RECORD_SIZE; ++j) if (p[j]) return -1;
            continue;
        }
        u32 species, map;
        if (!firstRun) firstRun = run;
        if (empty || run != firstRun || !serial || !ScoutRead32(p + 40) || (ScoutRead32(p + 44) & ~RHH_SCOUT_IDENTIFIED)
            || RhhContentResolve(RHH_SPECIES, WildActorRead64(p + 16), &species)
            || RhhContentKeyForId(RHH_SPECIES, species) != WildActorRead64(p + 16)
            || RhhContentResolve(RHH_MAP, WildActorRead64(p + 24), &map)
            || RhhContentKeyForId(RHH_MAP, map) != WildActorRead64(p + 24)) return -1;
        for (u32 j = 0; j < i; ++j)
        {
            const u8 *previous = state + j * SCOUT_RECORD_SIZE;
            if (WildActorRead64(previous) == run && WildActorRead64(previous + 8) == serial) return -1;
        }
    }
    return 0;
}
static const struct RhhModDefinition sScoutingState = {
    .struct_size = sizeof(sScoutingState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sScoutingStateId,
    .flags = RHH_MOD_REQUIRED, .state_size = SCOUT_STATE_SIZE, .validate = ValidateScoutingState
};
static u32 ScoutCount(void)
{
    const u8 *state = RhhModsState(sScoutingStateId);
    if (!state || !RhhModsHasActiveRun()) return 0;
    u32 count = 0;
    while (count < SCOUT_RECORDS && WildActorRead64(state + count * SCOUT_RECORD_SIZE) == RhhModsRunId()) ++count;
    return count;
}
static void ScoutReadRecord(const u8 *p, struct RhhScoutingRecord *out)
{
    *out = (struct RhhScoutingRecord){.struct_size = sizeof(*out), .flags = ScoutRead32(p + 44),
        .observations = ScoutRead32(p + 40), .individual = {.struct_size = sizeof(out->individual),
        .run_id = WildActorRead64(p), .serial = WildActorRead64(p + 8)},
        .species = (ScoutRead32(p + 44) & RHH_SCOUT_IDENTIFIED) ? WildActorRead64(p + 16) : 0,
        .first_map = WildActorRead64(p + 24), .first_step = WildActorRead64(p + 32)};
}
static int ScoutAt(u32 index, struct RhhScoutingRecord *out)
{
    const u8 *state = RhhModsState(sScoutingStateId);
    if (!out || out->struct_size < sizeof(*out) || !state || index >= ScoutCount()) return -1;
    ScoutReadRecord(state + index * SCOUT_RECORD_SIZE, out); return 0;
}
static int ScoutIndex(const struct RhhWildActorIndividual *id)
{
    const u8 *state = RhhModsState(sScoutingStateId);
    if (!state || !id || id->struct_size < sizeof(*id) || id->reserved || !id->serial
        || !RhhModsHasActiveRun() || id->run_id != RhhModsRunId()) return -1;
    for (u32 i = 0; i < ScoutCount(); ++i)
        if (WildActorRead64(state + i * SCOUT_RECORD_SIZE + 8) == id->serial) return i;
    return -1;
}
static bool32 WildActorWasObserved(u32 slot)
{
    if (slot >= OBJECT_EVENTS_COUNT || !sWildActors[slot].individual) return FALSE;
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id), .run_id = sWildActors[slot].runId,
        .serial = sWildActors[slot].individual};
    return ScoutIndex(&id) >= 0;
}
static int ScoutFind(const struct RhhWildActorIndividual *id, struct RhhScoutingRecord *out)
{
    int index = ScoutIndex(id);
    return index < 0 ? -1 : ScoutAt(index, out);
}
static int ScoutContext(struct RhhScoutingResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sScoutingContext
        || (event != RHH_CAN_SCOUT && event != RHH_SCOUT_RECORDED)) return -1;
    *out = *sScoutingContext; return 0;
}
static int ScoutFailure(struct RhhScoutingResult *out, u32 status, const char *reason)
{
    out->status = status;
    if (reason) snprintf(out->reason, sizeof(out->reason), "%s", reason);
    return status;
}
static void ScoutDispatch(u32 kind, const struct RhhScoutingResult *result, struct RhhModDecision *decision)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = result->sight.map, .subject = result->sight.target_wild.species,
        .ordinal = result->sight.target_wild.handle,
        .args = {result->action, result->is_preview, result->after.observations, result->after.flags}};
    sScoutingContext = result; RhhModsDispatch(&event, decision); sScoutingContext = NULL;
}
static int ScoutChange(const struct RhhScoutingRequest *request, struct RhhScoutingResult *out, bool32 preview, u64 photoTicket, bool32 allowGeometric)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_SCOUT_INVALID;
    *out = (struct RhhScoutingResult){.struct_size = sizeof(*out), .is_preview = preview,
        .before.struct_size = sizeof(out->before), .after.struct_size = sizeof(out->after),
        .sight.struct_size = sizeof(out->sight)};
    if (!request || request->struct_size < sizeof(*request) || !request->handle
        || (request->action != RHH_SCOUT_OBSERVE && request->action != RHH_SCOUT_IDENTIFY)
        || request->max_distance > 64 || (request->checks & ~(RHH_PERCEPTION_CHECK_SCENE
            | RHH_PERCEPTION_CHECK_COLLISION | RHH_PERCEPTION_CHECK_ACTORS | RHH_PERCEPTION_CHECK_FACING)))
        return ScoutFailure(out, RHH_SCOUT_INVALID, "invalid scouting request");
    out->action = request->action;
    if (sScoutingBusy || !WildActorCommandReady())
        return ScoutFailure(out, RHH_SCOUT_UNAVAILABLE, "overworld commands unavailable");
    struct RhhPerceptionRequest sight = {.struct_size = sizeof(sight), .checks = request->checks,
        .max_distance = request->max_distance, .viewer = {RHH_PERCEPTION_PLAYER, 0, 0},
        .target = {RHH_PERCEPTION_WILD, 0, request->handle}};
    if (PerceptionQuery(&sight, &out->sight)) return ScoutFailure(out, RHH_SCOUT_INVALID, "actor no longer available");
    if (photoTicket && !ScoutPhotoMatches(photoTicket, &out->sight))
        return ScoutFailure(out, RHH_SCOUT_STALE, "photo evidence no longer matches");
    if (photoTicket && !allowGeometric && sScoutPhotoProof.quality != RHH_PHOTO_MASK_EXACT)
        return ScoutFailure(out, RHH_SCOUT_EVIDENCE_QUALITY, "photo evidence is approximate");
    u32 evidenceBit = request->action == RHH_SCOUT_OBSERVE ? 1u : 2u;
    if (photoTicket && (sScoutPhotoProof.used & evidenceBit))
        return ScoutFailure(out, RHH_SCOUT_EVIDENCE_USED, "photo evidence already used");
    if (!(out->sight.flags & RHH_PERCEPTION_PERCEIVABLE)
        || (out->sight.target_wild.slot < OBJECT_EVENTS_COUNT
            && WildPresentationEligible(out->sight.target_wild.slot)
            && !sWildActors[out->sight.target_wild.slot].observed
            && !sWildActors[out->sight.target_wild.slot].conceal.mode
            && WildCueExists(out->sight.target_wild.slot) && !WildCueVisible(out->sight.target_wild.slot))
        || (!photoTicket && (out->sight.flags & (RHH_PERCEPTION_TARGET_OFFSCREEN | RHH_PERCEPTION_TARGET_NO_SPRITE
            | RHH_PERCEPTION_TARGET_SPRITE_HIDDEN))))
        return ScoutFailure(out, RHH_SCOUT_NOT_VISIBLE, "actor is not visible");
    const struct RhhWildActorIndividual *id = &out->sight.target_individual;
    if (!id->run_id || !id->serial) return ScoutFailure(out, RHH_SCOUT_UNAVAILABLE, "actor identity not yet saved");
    u8 *state = RhhModsState(sScoutingStateId); int index = ScoutIndex(id);
    if (!state || (ScoutCount() < SCOUT_RECORDS && WildActorRead64(state + ScoutCount() * SCOUT_RECORD_SIZE)))
        return ScoutFailure(out, RHH_SCOUT_UNAVAILABLE, "observation state unavailable");
    if (index < 0 && request->action == RHH_SCOUT_IDENTIFY)
        return ScoutFailure(out, RHH_SCOUT_NOT_OBSERVED, "actor has not been observed");
    if (index < 0 && ScoutCount() >= SCOUT_RECORDS) return ScoutFailure(out, RHH_SCOUT_FULL, "observation log is full");
    u8 record[SCOUT_RECORD_SIZE] = {0}, original[SCOUT_RECORD_SIZE];
    u32 originalCount = ScoutCount();
    if (index >= 0)
    {
        memcpy(record, state + index * SCOUT_RECORD_SIZE, sizeof(record)); ScoutReadRecord(record, &out->before);
        if (WildActorRead64(record + 16) != out->sight.target_wild.species)
            return ScoutFailure(out, RHH_SCOUT_UNAVAILABLE, "actor appearance changed");
    }
    else
    {
        const struct RhhWorldClockService *clock = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
        struct RhhWorldClockSnapshot now = {.struct_size = sizeof(now)};
        if (!clock || clock->current(&now)) return ScoutFailure(out, RHH_SCOUT_UNAVAILABLE, "world clock unavailable");
        index = ScoutCount(); WildActorWrite64(record, id->run_id); WildActorWrite64(record + 8, id->serial);
        WildActorWrite64(record + 16, out->sight.target_wild.species); WildActorWrite64(record + 24, out->sight.map);
        WildActorWrite64(record + 32, now.steps);
    }
    memcpy(original, state + index * SCOUT_RECORD_SIZE, sizeof(original));
    if (request->action == RHH_SCOUT_OBSERVE)
    {
        u32 count = ScoutRead32(record + 40);
        if (count == UINT32_MAX) return ScoutFailure(out, RHH_SCOUT_FULL, "observation count exhausted");
        ScoutWrite32(record + 40, count + 1);
    }
    else if (ScoutRead32(record + 44) & RHH_SCOUT_IDENTIFIED)
    { out->after = out->before; return RHH_SCOUT_OK; }
    else ScoutWrite32(record + 44, RHH_SCOUT_IDENTIFIED);
    ScoutReadRecord(record, &out->after); out->changed = TRUE;
    sScoutingBusy = TRUE; sScoutingPhotoTicket = photoTicket;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    ScoutDispatch(RHH_CAN_SCOUT, out, &decision);
    if (photoTicket && (memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))))
    { gRngValue = rng; gRng2Value = rng2; decision.allowed = 0; snprintf(decision.reason, sizeof(decision.reason), "constraint used gameplay RNG"); }
    if (!decision.allowed)
    { sScoutingPhotoTicket = 0; sScoutingBusy = FALSE; return ScoutFailure(out, RHH_SCOUT_DENIED, decision.reason); }
    if (photoTicket)
    {
        struct RhhPerceptionResult current = {.struct_size = sizeof(current)};
        if (!WildActorCommandReady() || RhhModsState(sScoutingStateId) != state || ScoutCount() != originalCount
            || memcmp(original, state + index * SCOUT_RECORD_SIZE, sizeof(original))
            || PerceptionQuery(&sight, &current) || !ScoutPhotoMatches(photoTicket, &current)
            || (sScoutPhotoProof.used & evidenceBit))
        { sScoutingPhotoTicket = 0; sScoutingBusy = FALSE; return ScoutFailure(out, RHH_SCOUT_STALE, "photo evidence changed"); }
    }
    if (!preview)
    {
        if (request->action == RHH_SCOUT_OBSERVE)
        {
            const struct RhhEcologyService *ecology = (const void *)RhhModsHostAPI()->get_service(
                "rhh:service/ecology", RHH_SERVICE_VERSION(1, 0));
            int64_t observations = 0;
            int status = ecology ? ecology->get(out->sight.map, RHH_ECOLOGY_OBSERVATIONS,
                                                 out->sight.target_wild.species, &observations) : -1;
            if (status < 0 || observations == INT64_MAX
                || ecology->set(out->sight.map, RHH_ECOLOGY_OBSERVATIONS,
                                out->sight.target_wild.species, observations + 1))
            {
                sScoutingPhotoTicket = 0; sScoutingBusy = FALSE;
                return ScoutFailure(out, RHH_SCOUT_UNAVAILABLE, "location observation could not be saved");
            }
        }
        memcpy(state + index * SCOUT_RECORD_SIZE, record, sizeof(record));
        if (request->action == RHH_SCOUT_OBSERVE && out->sight.target_wild.slot < OBJECT_EVENTS_COUNT
            && WildActorMatches(out->sight.target_wild.slot)
            && sWildActors[out->sight.target_wild.slot].handle == request->handle)
            sWildActors[out->sight.target_wild.slot].observed = TRUE;
        if (photoTicket) sScoutPhotoProof.used |= evidenceBit;
        decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
        ScoutDispatch(RHH_SCOUT_RECORDED, out, &decision);
    }
    sScoutingPhotoTicket = 0; sScoutingBusy = FALSE; return RHH_SCOUT_OK;
}
static int ScoutPreview(const struct RhhScoutingRequest *r, struct RhhScoutingResult *o)
{ return ScoutChange(r, o, TRUE, 0, FALSE); }
static int ScoutApply(const struct RhhScoutingRequest *r, struct RhhScoutingResult *o)
{ return ScoutChange(r, o, FALSE, 0, FALSE); }
/* R while facing a visible disturbance observes its existing actor. Before a
 * run has a durable identity, the reveal is transient and no log is written. */
bool32 ModsWildScoutFacing(void)
{
    if (!WE_OW_ENCOUNTERS || FlagGet(DN_FLAG_SEARCHING) || !WildActorCommandReady()
        || gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT) return FALSE;
    const struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];
    if (!player->active || !player->isPlayer) return FALSE;
    s16 x = player->currentCoords.x, y = player->currentCoords.y;
    switch (GetPlayerFacingDirection())
    {
    case DIR_NORTH: --y; break; case DIR_SOUTH: ++y; break;
    case DIR_WEST: --x; break; case DIR_EAST: ++x; break;
    default: return FALSE;
    }
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; ++i)
    {
        struct ObjectEvent *object = &gObjectEvents[i];
        if (!WildPresentationEligible(i) || sWildActors[i].conceal.mode || sWildActors[i].observed
            || object->currentCoords.x != x || object->currentCoords.y != y || !WildCueVisible(i)) continue;
        bool32 success = FALSE;
        if (RhhModsRunCommitted())
        {
            struct RhhScoutingRequest request = {.struct_size = sizeof(request), .action = RHH_SCOUT_OBSERVE,
                .checks = RHH_PERCEPTION_CHECK_FACING | RHH_PERCEPTION_CHECK_COLLISION,
                .max_distance = 1, .handle = sWildActors[i].handle};
            struct RhhScoutingResult result = {.struct_size = sizeof(result)};
            success = ScoutApply(&request, &result) == RHH_SCOUT_OK;
        }
        else
        {
            struct RhhPerceptionRequest request = {.struct_size = sizeof(request), .max_distance = 1,
                .checks = RHH_PERCEPTION_CHECK_FACING | RHH_PERCEPTION_CHECK_COLLISION,
                .viewer = {RHH_PERCEPTION_PLAYER, 0, 0}, .target = {RHH_PERCEPTION_WILD, 0, sWildActors[i].handle}};
            struct RhhPerceptionResult result = {.struct_size = sizeof(result)};
            success = !PerceptionQuery(&request, &result) && (result.flags & RHH_PERCEPTION_PERCEIVABLE)
                && !(result.flags & (RHH_PERCEPTION_TARGET_OFFSCREEN | RHH_PERCEPTION_TARGET_NO_SPRITE
                                     | RHH_PERCEPTION_TARGET_SPRITE_HIDDEN));
            if (success) sWildActors[i].observed = TRUE;
        }
        PlaySE(success ? SE_SELECT : SE_BOO);
        return TRUE;
    }
    return FALSE;
}
static int ScoutPhotoChange(const struct RhhScoutingPhotoRequest *request, struct RhhScoutingResult *out, bool32 preview, bool32 allowGeometric)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->ticket)
        return ScoutChange(NULL, out, preview, 0, FALSE);
    struct RhhScoutingPhotoRequest copy = *request;
    return ScoutChange(&copy.scout, out, preview, copy.ticket, allowGeometric);
}
static int ScoutPhotoPreview(const struct RhhScoutingPhotoRequest *r, struct RhhScoutingResult *o)
{ return ScoutPhotoChange(r, o, TRUE, FALSE); }
static int ScoutPhotoApply(const struct RhhScoutingPhotoRequest *r, struct RhhScoutingResult *o)
{ return ScoutPhotoChange(r, o, FALSE, FALSE); }
static int ScoutPhotoPreviewGeometric(const struct RhhScoutingPhotoRequest *r, struct RhhScoutingResult *o)
{ return ScoutPhotoChange(r, o, TRUE, TRUE); }
static int ScoutPhotoApplyGeometric(const struct RhhScoutingPhotoRequest *r, struct RhhScoutingResult *o)
{ return ScoutPhotoChange(r, o, FALSE, TRUE); }
static const struct RhhScoutingService sScoutingService = {
    {sizeof(sScoutingService), RHH_SERVICE_VERSION(1, 3)}, ScoutCount, ScoutAt, ScoutFind, ScoutPreview, ScoutApply, ScoutContext,
    ScoutPhotoPreview, ScoutPhotoApply, ScoutPhotoContext, ScoutPhotoQuality, ScoutPhotoPreviewGeometric, ScoutPhotoApplyGeometric
};

enum { PHOTO_SCORE_RECORD_SIZE = 48, PHOTO_SCORE_STATE_SIZE = RHH_PHOTO_ALBUM_CAPACITY * PHOTO_SCORE_RECORD_SIZE };
static const char sPhotoScoreStateId[] = "rhh:photo_score_state";
static u32 PhotoBaseScore(u32 distance, u32 cover)
{
    u32 penalty = distance > 10 ? 800 : distance * 80;
    return RHH_PHOTO_SCORE_MAX - penalty - (cover ? 100 : 0);
}
static int ValidatePhotoScoreState(const u8 *state, u32 size)
{
    if (size != PHOTO_SCORE_STATE_SIZE) return -1;
    bool32 empty = FALSE; u64 run = 0;
    for (u32 i = 0; i < RHH_PHOTO_ALBUM_CAPACITY; ++i)
    {
        const u8 *p = state + i * PHOTO_SCORE_RECORD_SIZE;
        u64 currentRun = WildActorRead64(p), serial = WildActorRead64(p + 8);
        if (!currentRun)
        {
            empty = TRUE;
            for (u32 j = 0; j < PHOTO_SCORE_RECORD_SIZE; ++j) if (p[j]) return -1;
            continue;
        }
        if (!run) run = currentRun;
        u32 base = ScoutRead32(p + 16), value = ScoutRead32(p + 20), flags = ScoutRead32(p + 24);
        u32 distance = ScoutRead32(p + 28), cover = ScoutRead32(p + 32), checks = ScoutRead32(p + 36);
        u32 allChecks = RHH_PERCEPTION_CHECK_SCENE | RHH_PERCEPTION_CHECK_COLLISION | RHH_PERCEPTION_CHECK_ACTORS;
        if (empty || currentRun != run || !serial || distance > 64 || value > RHH_PHOTO_SCORE_MAX
            || (cover & ~(RHH_WILD_COVER_TALL_GRASS | RHH_WILD_COVER_LONG_GRASS | RHH_WILD_COVER_SHORT_GRASS
                | RHH_WILD_COVER_SAND | RHH_WILD_COVER_SHALLOW_WATER))
            || (checks & ~(allChecks | RHH_PERCEPTION_CHECK_FACING))
            || (flags & ~(RHH_PHOTO_SCORE_AVAILABLE | RHH_PHOTO_SCORE_ESTIMATED | RHH_PHOTO_SCORE_GROUND_COVER
                | RHH_PHOTO_SCORE_UNANNOTATED | RHH_PHOTO_SCORE_PARTIAL_CHECKS))
            || (flags & (RHH_PHOTO_SCORE_AVAILABLE | RHH_PHOTO_SCORE_ESTIMATED))
                != (RHH_PHOTO_SCORE_AVAILABLE | RHH_PHOTO_SCORE_ESTIMATED)
            || !!(flags & RHH_PHOTO_SCORE_GROUND_COVER) != !!cover
            || !!(flags & RHH_PHOTO_SCORE_PARTIAL_CHECKS) != ((checks & allChecks) != allChecks)
            || base != PhotoBaseScore(distance, cover) || WildActorRead64(p + 40)) return -1;
        for (u32 j = 0; j < i; ++j)
            if (WildActorRead64(state + j * PHOTO_SCORE_RECORD_SIZE + 8) == serial) return -1;
    }
    return 0;
}
static const struct RhhModDefinition sPhotoScoreState = {
    .struct_size = sizeof(sPhotoScoreState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sPhotoScoreStateId,
    .flags = RHH_MOD_REQUIRED, .state_size = PHOTO_SCORE_STATE_SIZE, .validate = ValidatePhotoScoreState
};
static int PhotoScoreIndex(const struct RhhPhotoId *id)
{
    const u8 *state = RhhModsState(sPhotoScoreStateId);
    if (!state || !id || !id->run_id || !id->serial || id->run_id != RhhModsRunId()) return -1;
    for (u32 i = 0; i < RHH_PHOTO_ALBUM_CAPACITY; ++i)
    {
        const u8 *p = state + i * PHOTO_SCORE_RECORD_SIZE;
        if (!WildActorRead64(p)) break;
        if (WildActorRead64(p) == id->run_id && WildActorRead64(p + 8) == id->serial) return i;
    }
    return -1;
}
static void PhotoScoreRead(const u8 *p, struct RhhPhotoScore *out)
{
    *out = (struct RhhPhotoScore){.struct_size = sizeof(*out), .base = ScoutRead32(p + 16),
        .value = ScoutRead32(p + 20), .flags = ScoutRead32(p + 24), .distance = ScoutRead32(p + 28),
        .cover = ScoutRead32(p + 32), .checks = ScoutRead32(p + 36),
        .photo = {WildActorRead64(p), WildActorRead64(p + 8)}};
}
static int PhotoScoreInsert(const struct RhhPhotoScore *score, const struct RhhPhotoId *id)
{
    u8 *state = RhhModsState(sPhotoScoreStateId);
    if (!state || !score || !id || !id->run_id || !id->serial || PhotoScoreIndex(id) >= 0) return -1;
    u32 i = 0;
    while (i < RHH_PHOTO_ALBUM_CAPACITY && WildActorRead64(state + i * PHOTO_SCORE_RECORD_SIZE)) ++i;
    if (i == RHH_PHOTO_ALBUM_CAPACITY) return -1;
    u8 *p = state + i * PHOTO_SCORE_RECORD_SIZE;
    WildActorWrite64(p, id->run_id); WildActorWrite64(p + 8, id->serial);
    ScoutWrite32(p + 16, score->base); ScoutWrite32(p + 20, score->value); ScoutWrite32(p + 24, score->flags);
    ScoutWrite32(p + 28, score->distance); ScoutWrite32(p + 32, score->cover); ScoutWrite32(p + 36, score->checks);
    if (ValidatePhotoScoreState(state, PHOTO_SCORE_STATE_SIZE)) { memset(p, 0, PHOTO_SCORE_RECORD_SIZE); return -1; }
    return 0;
}
static void PhotoScoreRemove(const struct RhhPhotoId *id)
{
    u8 *state = RhhModsState(sPhotoScoreStateId); int index = PhotoScoreIndex(id);
    if (!state || index < 0) return;
    u32 after = (RHH_PHOTO_ALBUM_CAPACITY - (u32)index - 1) * PHOTO_SCORE_RECORD_SIZE;
    memmove(state + index * PHOTO_SCORE_RECORD_SIZE, state + (index + 1) * PHOTO_SCORE_RECORD_SIZE, after);
    memset(state + (RHH_PHOTO_ALBUM_CAPACITY - 1) * PHOTO_SCORE_RECORD_SIZE, 0, PHOTO_SCORE_RECORD_SIZE);
}

static struct {
    bool8 busy, stored, visibilityAvailable, geometricAvailable, footprintAvailable, presentedOnly, depthAvailable, renderAvailable;
    u8 targetSprite;
    u64 nextTicket, run, albumSerial;
    struct RhhPhotoRequest request;
    struct RhhPhotoCapture capture;
    struct RhhPhotoScore score;
    struct RhhPhotoRecent recent;
    u8 pixels[RHH_PHOTO_RGB_BYTES];
    u8 visibilityMask[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
    u8 silhouetteMask[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
    u16 depthPixels[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
    struct RhhPhotoVisibility visibility;
    struct RhhPhotoFootprint footprint;
    struct RhhPhotoDepth depth;
    struct RhhPhotoRenderContext render;
    RhhContentKey renderVariant;
} sPhoto;
static void RememberScoutPhoto(const struct RhhPhotoCapture *capture)
{
    memset(&sScoutPhotoProof, 0, sizeof(sScoutPhotoProof));
    if (!capture || capture->status != RHH_PHOTO_OK || (!sPhoto.visibilityAvailable && !sPhoto.geometricAvailable)
        || !sPhoto.visibility.visible_pixels) return;
    const struct RhhWorldClockService *clock = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldClockSnapshot now = {.struct_size = sizeof(now)};
    sScoutPhotoProof.camera.struct_size = sizeof(sScoutPhotoProof.camera);
    sScoutPhotoProof.actor.struct_size = sizeof(sScoutPhotoProof.actor);
    sScoutPhotoProof.billboard.struct_size = sizeof(sScoutPhotoProof.billboard);
    sScoutPhotoProof.visibility.struct_size = sizeof(sScoutPhotoProof.visibility);
    if (!clock || clock->current(&now) || now.run_id != RhhModsRunId()
        || ActorAt(capture->sight.target_wild.slot, &sScoutPhotoProof.actor)
        || Camera(&sScoutPhotoProof.camera) || Billboard(capture->sight.target_wild.slot, &sScoutPhotoProof.billboard)
        || ActorVisibility(capture->sight.target_wild.slot, &sScoutPhotoProof.visibility)) return;
    sScoutPhotoProof.spriteId = gObjectEvents[capture->sight.target_wild.slot].spriteId;
    sScoutPhotoProof.valid = TRUE; sScoutPhotoProof.ticket = capture->ticket;
    sScoutPhotoProof.quality = sPhoto.geometricAvailable ? RHH_PHOTO_MASK_GEOMETRIC : RHH_PHOTO_MASK_EXACT;
    sScoutPhotoProof.run = now.run_id; sScoutPhotoProof.step = now.steps; sScoutPhotoProof.sight = capture->sight;
}
int RhhModsPhotoTarget(uint32_t *slot, uint64_t *ticket)
{
    if (!slot || !ticket || !sPhoto.busy || !RhhModsHasActiveRun() || sPhoto.run != RhhModsRunId()) return -1;
    *slot = sPhoto.capture.sight.target_wild.slot; *ticket = sPhoto.capture.ticket; return 0;
}
static const char sPhotoSequenceId[] = "rhh:photo_sequence";
static const struct RhhModDefinition sPhotoSequence = {
    .struct_size = sizeof(sPhotoSequence), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sPhotoSequenceId,
    .flags = RHH_MOD_REQUIRED | RHH_MOD_DURABLE, .state_size = 8, .merge = MergeWildActorSequence
};
static struct { u64 run, next, end; } sPhotoLease;
static void PhotoSequenceFailure(void)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_PERSISTENCE_FAILED,
        .run_seed = RhhModsRunSeed()};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision);
}
static u64 PhotoAllocateSerial(void)
{
    u8 *state = RhhModsState(sPhotoSequenceId); u64 run = RhhModsRunId();
    if (!state || NativeSaveTransactionActive()) return 0;
    if (!RhhModsCanPersistDurably()) { PhotoSequenceFailure(); return 0; }
    if (sPhotoLease.run != run || sPhotoLease.end > WildActorRead64(state))
    { memset(&sPhotoLease, 0, sizeof(sPhotoLease)); sPhotoLease.run = run; }
    if (!sPhotoLease.next || sPhotoLease.next > sPhotoLease.end)
    {
        u64 old = WildActorRead64(state);
        if (old > UINT64_MAX - 64) return 0;
        WildActorWrite64(state, old + 64);
        if (RhhModsFlushDurable()) { WildActorWrite64(state, old); PhotoSequenceFailure(); return 0; }
        sPhotoLease.next = old + 1; sPhotoLease.end = old + 64;
    }
    return sPhotoLease.next++;
}
static const struct RhhPhotoCapture *sPhotoContext;
static const struct RhhPhotoScoreContext *sPhotoScoreContext;
static int PhotoContext(struct RhhPhotoCapture *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sPhotoContext
        || (event != RHH_CAN_PHOTO && event != RHH_PHOTO_SCORE
            && event != RHH_PHOTO_CAPTURED && event != RHH_PHOTO_FAILED)) return -1;
    *out = *sPhotoContext; return 0;
}
static int PhotoCopyRgb(u8 *out, u32 size)
{
    if (!out || size < RHH_PHOTO_RGB_BYTES || !sPhotoContext
        || (RhhModsCurrentEvent() != RHH_PHOTO_CAPTURED && RhhModsCurrentEvent() != RHH_PHOTO_SCORE)
        || sPhotoContext->status != RHH_PHOTO_OK) return -1;
    memcpy(out, sPhoto.pixels, RHH_PHOTO_RGB_BYTES); return 0;
}
static void PhotoDispatch(u32 kind, const struct RhhPhotoCapture *capture, struct RhhModDecision *decision)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = capture->sight.map, .subject = capture->sight.target_wild.species, .ordinal = capture->ticket,
        .args = {capture->status, capture->backend, capture->frame, 0}};
    sPhotoContext = capture; RhhModsDispatch(&event, decision); sPhotoContext = NULL;
}
static int PhotoSight(const struct RhhPhotoRequest *request, struct RhhPerceptionResult *out, bool32 presented)
{
    struct RhhPerceptionRequest sight = {.struct_size = sizeof(sight), .checks = request->checks,
        .max_distance = request->max_distance, .viewer = {RHH_PERCEPTION_PLAYER, 0, 0},
        .target = {RHH_PERCEPTION_WILD, 0, request->actor_handle}};
    if (PerceptionQuery(&sight, out)) return RHH_PHOTO_INVALID;
    if (!(out->flags & RHH_PERCEPTION_PERCEIVABLE) || (out->flags & RHH_PERCEPTION_TARGET_NO_SPRITE))
        return RHH_PHOTO_NOT_VISIBLE;
    if (out->flags & (RHH_PERCEPTION_TARGET_OFFSCREEN | RHH_PERCEPTION_TARGET_SPRITE_HIDDEN))
    {
        struct RhhWorldActorVisibility visibility = {.struct_size = sizeof(visibility)};
        if (!presented || ActorVisibility(out->target_wild.slot, &visibility)
            || !(visibility.flags & RHH_WORLD_ACTOR_CULLED)
            || ((visibility.flags & RHH_WORLD_ACTOR_SPRITE_HIDDEN)
                && !(visibility.flags & RHH_WORLD_ACTOR_ENGINE_MOVEMENT))
            || (visibility.flags & (RHH_WORLD_ACTOR_OBJECT_HIDDEN | RHH_WORLD_ACTOR_CONCEALED | RHH_WORLD_ACTOR_LINK_ACTIVE)))
            return RHH_PHOTO_NOT_VISIBLE;
    }
    if (!out->target_individual.run_id || !out->target_individual.serial) return RHH_PHOTO_UNAVAILABLE;
    return RHH_PHOTO_OK;
}
static int PhotoScoreContext(struct RhhPhotoScoreContext *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sPhotoScoreContext
        || (event != RHH_PHOTO_SCORE && event != RHH_PHOTO_CAPTURED)) return -1;
    *out = *sPhotoScoreContext; return 0;
}
static void PhotoEvaluate(struct RhhPhotoCapture *capture, struct RhhPhotoScoreContext *context)
{
    const u32 allChecks = RHH_PERCEPTION_CHECK_SCENE | RHH_PERCEPTION_CHECK_COLLISION | RHH_PERCEPTION_CHECK_ACTORS;
    u32 flags = RHH_PHOTO_SCORE_AVAILABLE | RHH_PHOTO_SCORE_ESTIMATED;
    if (capture->sight.target_cover) flags |= RHH_PHOTO_SCORE_GROUND_COVER;
    if (capture->sight.flags & RHH_PERCEPTION_SCENE_UNANNOTATED) flags |= RHH_PHOTO_SCORE_UNANNOTATED;
    if ((capture->sight.checks & allChecks) != allChecks) flags |= RHH_PHOTO_SCORE_PARTIAL_CHECKS;
    u32 base = PhotoBaseScore(capture->sight.distance, capture->sight.target_cover);
    *context = (struct RhhPhotoScoreContext){.struct_size = sizeof(*context), .capture = *capture,
        .score = {.struct_size = sizeof(context->score), .flags = flags, .base = base, .value = base,
                  .distance = capture->sight.distance, .cover = capture->sight.target_cover, .checks = capture->sight.checks},
        .habitat.struct_size = sizeof(context->habitat), .time.struct_size = sizeof(context->time)};
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    context->habitat_available = !HabitatSample(capture->sight.target_x, capture->sight.target_y, &context->habitat);
    const struct RhhWorldTimeService *time = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_time", RHH_SERVICE_VERSION(1, 0));
    context->time_available = time && !time->sample(&context->time);
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_PHOTO_SCORE, .run_seed = RhhModsRunSeed(),
        .event_key = capture->sight.map, .subject = capture->sight.target_wild.species, .ordinal = capture->ticket,
        .args = {capture->sight.distance, flags, capture->sight.target_cover, capture->sight.checks}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = base};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sPhotoContext = capture; sPhotoScoreContext = context;
    RhhModsDispatch(&event, &decision);
    sPhotoContext = NULL; sPhotoScoreContext = NULL;
    bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    RhhModsWorldTimeScopeEnd(&timeScope);
    if (!decision.allowed) context->score = (struct RhhPhotoScore){.struct_size = sizeof(context->score)};
    else if (!impure && decision.value <= RHH_PHOTO_SCORE_MAX) context->score.value = decision.value;
}
static bool32 PhotoModePresented(u32 mode)
{ return mode == RHH_PHOTO_REQUEST_PRESENTED || mode == RHH_PHOTO_REQUEST_PRESENTED_GEOMETRIC; }
static int PhotoRequestInternal(const struct RhhPhotoRequest *request, struct RhhPhotoCapture *out, u32 mode)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_PHOTO_INVALID;
    *out = (struct RhhPhotoCapture){.struct_size = sizeof(*out), .width = RHH_PHOTO_WIDTH,
        .height = RHH_PHOTO_HEIGHT, .stride = RHH_PHOTO_WIDTH * 3, .sight.struct_size = sizeof(out->sight)};
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->actor_handle
        || !request->owner || !RhhModsRegisteredKey(request->owner) || request->max_distance > 64
        || (request->checks & ~(RHH_PERCEPTION_CHECK_SCENE | RHH_PERCEPTION_CHECK_COLLISION
                                | RHH_PERCEPTION_CHECK_ACTORS | RHH_PERCEPTION_CHECK_FACING)))
        return out->status = RHH_PHOTO_INVALID;
    if (sPhoto.busy || sPhoto.nextTicket == UINT64_MAX || !WildActorCommandReady()
        || RhhUiActive() || RhhUiKeysBlocked()) return out->status = RHH_PHOTO_UNAVAILABLE;
    out->owner = request->owner; out->ticket = sPhoto.nextTicket + 1;
    out->status = PhotoSight(request, &out->sight, PhotoModePresented(mode));
    if (out->status != RHH_PHOTO_OK) return out->status;
    sPhoto.busy = TRUE; sPhoto.presentedOnly = mode;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    PhotoDispatch(RHH_CAN_PHOTO, out, &decision);
    bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed)
    {
        out->status = RHH_PHOTO_DENIED; memcpy(out->reason, decision.reason, sizeof(out->reason));
        sPhoto.busy = FALSE; return out->status;
    }
    if (impure)
    {
        out->status = RHH_PHOTO_UNAVAILABLE;
        snprintf(out->reason, sizeof(out->reason), "photo constraint consumed RNG");
        sPhoto.busy = FALSE; return out->status;
    }
    sPhoto.nextTicket = out->ticket; sPhoto.run = RhhModsRunId(); sPhoto.request = *request; sPhoto.capture = *out;
    sPhoto.targetSprite = gObjectEvents[out->sight.target_wild.slot].spriteId;
    sPhoto.visibilityAvailable = sPhoto.geometricAvailable = FALSE;
    sPhoto.footprintAvailable = sPhoto.depthAvailable = sPhoto.renderAvailable = FALSE;
    NativePhotoVisibilityRequest(sPhoto.targetSprite, out->ticket);
    return RHH_PHOTO_OK;
}
static int PhotoRequest(const struct RhhPhotoRequest *request, struct RhhPhotoCapture *out)
{ return PhotoRequestInternal(request, out, FALSE); }
static int PhotoRequestPresented(const struct RhhPhotoRequest *request, struct RhhPhotoCapture *out)
{ return PhotoRequestInternal(request, out, TRUE); }
static int PhotoRequestPresentedGeometric(const struct RhhPhotoRequest *request, struct RhhPhotoCapture *out)
{ return PhotoRequestInternal(request, out, 2); }
static int PhotoRequestGeometric(const struct RhhPhotoRequest *request, struct RhhPhotoCapture *out)
{ return PhotoRequestInternal(request, out, RHH_PHOTO_REQUEST_ORDINARY_GEOMETRIC); }
static int PhotoRequiresPresenter(uint32_t *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || !sPhotoContext || (event != RHH_CAN_PHOTO && event != RHH_PHOTO_SCORE
        && event != RHH_PHOTO_CAPTURED && event != RHH_PHOTO_FAILED)) return -1;
    *out = PhotoModePresented(sPhoto.presentedOnly); return 0;
}
static int PhotoRenderContext(struct RhhPhotoRenderContext *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sPhoto.renderAvailable
        || (event != RHH_PHOTO_SCORE && event != RHH_PHOTO_CAPTURED)) return -1;
    *out = sPhoto.render; return 0;
}
static int PhotoPresenterVariant(RhhContentKey *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || !sPhoto.renderAvailable || (event != RHH_PHOTO_SCORE && event != RHH_PHOTO_CAPTURED)) return -1;
    *out = sPhoto.renderVariant; return 0;
}
static int PhotoRequestMode(u32 *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || !sPhotoContext || (event != RHH_CAN_PHOTO && event != RHH_PHOTO_SCORE
        && event != RHH_PHOTO_CAPTURED && event != RHH_PHOTO_FAILED)) return -1;
    *out = sPhoto.presentedOnly; return 0;
}
static int PhotoVisibilityContext(struct RhhPhotoVisibility *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sPhoto.visibilityAvailable
        || (event != RHH_PHOTO_SCORE && event != RHH_PHOTO_CAPTURED)) return -1;
    *out = sPhoto.visibility;
    return 0;
}
static int PhotoGeometricContext(struct RhhPhotoVisibility *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sPhoto.geometricAvailable
        || (event != RHH_PHOTO_SCORE && event != RHH_PHOTO_CAPTURED)) return -1;
    *out = sPhoto.visibility; return 0;
}
static int PhotoDepthContext(struct RhhPhotoDepth *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sPhoto.depthAvailable
        || (event != RHH_PHOTO_SCORE && event != RHH_PHOTO_CAPTURED)) return -1;
    *out = sPhoto.depth; return 0;
}
static int PhotoDepthCopy(u16 *out, u32 size)
{
    struct RhhPhotoDepth context = {.struct_size = sizeof(context)};
    if (!out || size < RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT || PhotoDepthContext(&context)) return -1;
    memcpy(out, sPhoto.depthPixels, sizeof(sPhoto.depthPixels)); return 0;
}
static int PhotoGeometricCopyMask(u8 *out, u32 size)
{
    struct RhhPhotoVisibility context = {.struct_size = sizeof(context)};
    if (!out || size < sizeof(sPhoto.visibilityMask) || PhotoGeometricContext(&context)) return -1;
    memcpy(out, sPhoto.visibilityMask, sizeof(sPhoto.visibilityMask)); return 0;
}
static int PhotoGeometricCopySilhouette(u8 *out, u32 size)
{
    struct RhhPhotoVisibility context = {.struct_size = sizeof(context)};
    if (!out || size < sizeof(sPhoto.silhouetteMask) || PhotoGeometricContext(&context)) return -1;
    memcpy(out, sPhoto.silhouetteMask, sizeof(sPhoto.silhouetteMask)); return 0;
}
static int PhotoVisibilityCopyMask(u8 *out, u32 size)
{
    struct RhhPhotoVisibility context = {.struct_size = sizeof(context)};
    if (!out || size < sizeof(sPhoto.visibilityMask) || PhotoVisibilityContext(&context)) return -1;
    memcpy(out, sPhoto.visibilityMask, sizeof(sPhoto.visibilityMask));
    return 0;
}
static int PhotoExposure(struct RhhPhotoExposure *out, bool32 geometric)
{
    struct RhhPhotoVisibility visibility = {.struct_size = sizeof(visibility)};
    if (!out || out->struct_size < sizeof(*out)
        || (geometric ? PhotoGeometricContext(&visibility) : PhotoVisibilityContext(&visibility))) return -1;
    struct RhhPhotoExposure result = {.struct_size = sizeof(result), .ticket = visibility.ticket};
    u32 sum = 0;
    for (u32 i = 0; i < RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT; ++i)
    {
        if (!sPhoto.visibilityMask[i]) continue;
        const u8 *pixel = sPhoto.pixels + i * 3;
        u32 luma = (54u * pixel[0] + 183u * pixel[1] + 19u * pixel[2]) / 256u;
        if (!result.visible_pixels || luma < result.min_luma) result.min_luma = luma;
        if (luma > result.max_luma) result.max_luma = luma;
        ++result.visible_pixels;
        result.dark_pixels += luma <= 16;
        result.bright_pixels += luma >= 240;
        sum += luma;
    }
    if (result.visible_pixels) result.mean_luma = sum / result.visible_pixels;
    *out = result;
    return 0;
}
static int PhotoVisibilityExposure(struct RhhPhotoExposure *out) { return PhotoExposure(out, FALSE); }
static int PhotoGeometricExposure(struct RhhPhotoExposure *out) { return PhotoExposure(out, TRUE); }
static int PhotoVisibilityCopySilhouette(u8 *out, u32 size)
{
    struct RhhPhotoVisibility context = {.struct_size = sizeof(context)};
    if (!out || size < sizeof(sPhoto.silhouetteMask) || PhotoVisibilityContext(&context)) return -1;
    memcpy(out, sPhoto.silhouetteMask, sizeof(sPhoto.silhouetteMask));
    return 0;
}
static void PhotoBounds(const u8 *mask, struct RhhPhotoBounds *bounds)
{
    bool32 found = FALSE;
    for (u32 y = 0; y < RHH_PHOTO_HEIGHT; ++y)
        for (u32 x = 0; x < RHH_PHOTO_WIDTH; ++x)
            if (mask[y * RHH_PHOTO_WIDTH + x])
            {
                if (!found) { *bounds = (struct RhhPhotoBounds){x, y, x + 1, y + 1}; found = TRUE; }
                else
                {
                    if (x < bounds->left) bounds->left = x;
                    if (x + 1 > bounds->right) bounds->right = x + 1;
                    bounds->bottom = y + 1;
                }
            }
}
static int PhotoFraming(struct RhhPhotoFraming *out, bool32 geometric)
{
    struct RhhPhotoVisibility context = {.struct_size = sizeof(context)};
    if (!out || out->struct_size < sizeof(*out)
        || (geometric ? PhotoGeometricContext(&context) : PhotoVisibilityContext(&context))) return -1;
    struct RhhPhotoFraming result = {.struct_size = sizeof(result), .ticket = context.ticket};
    PhotoBounds(sPhoto.silhouetteMask, &result.silhouette);
    PhotoBounds(sPhoto.visibilityMask, &result.visible);
    *out = result; return 0;
}
static int PhotoVisibilityFraming(struct RhhPhotoFraming *out) { return PhotoFraming(out, FALSE); }
static int PhotoGeometricFraming(struct RhhPhotoFraming *out) { return PhotoFraming(out, TRUE); }
static int PhotoFootprint(struct RhhPhotoFootprint *out, bool32 geometric)
{
    struct RhhPhotoVisibility context = {.struct_size = sizeof(context)};
    if (!out || out->struct_size < sizeof(*out) || !sPhoto.footprintAvailable
        || (geometric ? PhotoGeometricContext(&context) : PhotoVisibilityContext(&context))) return -1;
    *out = sPhoto.footprint; return 0;
}
static int PhotoVisibilityFootprint(struct RhhPhotoFootprint *out) { return PhotoFootprint(out, FALSE); }
static int PhotoGeometricFootprint(struct RhhPhotoFootprint *out) { return PhotoFootprint(out, TRUE); }
static int PhotoComposition(struct RhhPhotoComposition *out, bool32 geometric)
{
    struct RhhPhotoVisibility context = {.struct_size = sizeof(context)};
    if (!out || out->struct_size < sizeof(*out)
        || (geometric ? PhotoGeometricContext(&context) : PhotoVisibilityContext(&context))) return -1;
    struct RhhPhotoComposition result = {.struct_size = sizeof(result), .ticket = context.ticket};
    for (u32 y = 0; y < RHH_PHOTO_HEIGHT; ++y)
        for (u32 x = 0; x < RHH_PHOTO_WIDTH; ++x)
        {
            u32 index = y * RHH_PHOTO_WIDTH + x, cell = (y * 3 / RHH_PHOTO_HEIGHT) * 3 + x * 3 / RHH_PHOTO_WIDTH;
            if (sPhoto.silhouetteMask[index])
            {
                ++result.silhouette.pixels; result.silhouette.x2_sum += 2 * x + 1;
                result.silhouette.y2_sum += 2 * y + 1; ++result.silhouette.cells[cell];
            }
            if (sPhoto.visibilityMask[index])
            {
                ++result.visible.pixels; result.visible.x2_sum += 2 * x + 1;
                result.visible.y2_sum += 2 * y + 1; ++result.visible.cells[cell];
            }
        }
    *out = result; return 0;
}
static int PhotoVisibilityComposition(struct RhhPhotoComposition *out) { return PhotoComposition(out, FALSE); }
static int PhotoGeometricComposition(struct RhhPhotoComposition *out) { return PhotoComposition(out, TRUE); }
static u32 PhotoLuma(u32 index)
{
    const u8 *pixel = sPhoto.pixels + index * 3;
    return (54u * pixel[0] + 183u * pixel[1] + 19u * pixel[2]) / 256u;
}
static int PhotoDetail(struct RhhPhotoDetail *out, bool32 geometric)
{
    struct RhhPhotoVisibility context = {.struct_size = sizeof(context)};
    if (!out || out->struct_size < sizeof(*out)
        || (geometric ? PhotoGeometricContext(&context) : PhotoVisibilityContext(&context))) return -1;
    struct RhhPhotoDetail result = {.struct_size = sizeof(result), .ticket = context.ticket};
    for (u32 y = 0; y < RHH_PHOTO_HEIGHT; ++y)
        for (u32 x = 0; x < RHH_PHOTO_WIDTH; ++x)
        {
            u32 index = y * RHH_PHOTO_WIDTH + x;
            if (!sPhoto.visibilityMask[index]) continue;
            u32 neighbors[2] = {index + 1, index + RHH_PHOTO_WIDTH};
            for (u32 direction = 0; direction < 2; ++direction)
            {
                if ((!direction && x + 1 >= RHH_PHOTO_WIDTH) || (direction && y + 1 >= RHH_PHOTO_HEIGHT)
                    || !sPhoto.visibilityMask[neighbors[direction]]) continue;
                u32 a = PhotoLuma(index), b = PhotoLuma(neighbors[direction]);
                u32 delta = a > b ? a - b : b - a;
                ++result.neighbor_pairs; result.luma_delta_sum += delta;
                result.high_contrast_pairs += delta >= 32;
                if (delta > result.max_luma_delta) result.max_luma_delta = delta;
            }
        }
    *out = result; return 0;
}
static int PhotoVisibilityDetail(struct RhhPhotoDetail *out) { return PhotoDetail(out, FALSE); }
static int PhotoGeometricDetail(struct RhhPhotoDetail *out) { return PhotoDetail(out, TRUE); }
static bool32 PhotoFootprintValid(const struct RhhPhotoFootprint *footprint, u64 ticket)
{
    if (!footprint || footprint->struct_size < sizeof(*footprint) || footprint->reserved || footprint->ticket != ticket
        || !footprint->total_pixels || footprint->inside_pixels != sPhoto.visibility.total_pixels
        || footprint->inside_pixels > footprint->total_pixels) return FALSE;
    s64 width = (s64)footprint->right - footprint->left, height = (s64)footprint->bottom - footprint->top;
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048
        || footprint->total_pixels > width * height) return FALSE;
    bool32 outside = footprint->left < 0 || footprint->top < 0
        || footprint->right > RHH_PHOTO_WIDTH || footprint->bottom > RHH_PHOTO_HEIGHT;
    if (outside != (footprint->total_pixels > footprint->inside_pixels)) return FALSE;
    for (u32 y = 0; y < RHH_PHOTO_HEIGHT; ++y)
        for (u32 x = 0; x < RHH_PHOTO_WIDTH; ++x)
            if (sPhoto.silhouetteMask[y * RHH_PHOTO_WIDTH + x]
                && ((s32)x < footprint->left || (s32)y < footprint->top
                    || (s32)x >= footprint->right || (s32)y >= footprint->bottom)) return FALSE;
    return TRUE;
}
static const struct RhhPhotoVisibilityService sPhotoVisibilityService = {
    {sizeof(sPhotoVisibilityService), RHH_SERVICE_VERSION(1, 8)}, PhotoVisibilityContext, PhotoVisibilityCopyMask,
    PhotoVisibilityExposure, PhotoVisibilityCopySilhouette, PhotoVisibilityFraming, PhotoVisibilityFootprint,
    PhotoVisibilityComposition, PhotoVisibilityDetail, PhotoGeometricContext, PhotoGeometricCopyMask, PhotoGeometricCopySilhouette,
    PhotoDepthContext, PhotoDepthCopy, PhotoGeometricExposure, PhotoGeometricFraming, PhotoGeometricFootprint,
    PhotoGeometricComposition, PhotoGeometricDetail
};
void RhhModsPhotoPresent(const u16 *pixels, int shader, int world, u32 frame)
{
    if (!sPhoto.busy) return;
    if (!RhhModsHasActiveRun() || sPhoto.run != RhhModsRunId())
    { sPhoto.busy = FALSE; NativePhotoVisibilityCancel(); return; }
    struct RhhPhotoCapture capture = sPhoto.capture;
    struct RhhPerceptionResult now = {.struct_size = sizeof(now)};
    int status = (!world || !WildActorCommandReady() || RhhUiActive() || RhhUiKeysBlocked())
        ? RHH_PHOTO_STALE : PhotoSight(&sPhoto.request, &now, PhotoModePresented(sPhoto.presentedOnly));
    if (status == RHH_PHOTO_OK && (now.map != capture.sight.map
        || now.target_individual.run_id != capture.sight.target_individual.run_id
        || now.target_individual.serial != capture.sight.target_individual.serial
        || now.target_wild.species != capture.sight.target_wild.species
        || now.target_wild.slot != capture.sight.target_wild.slot
        || gObjectEvents[now.target_wild.slot].spriteId != sPhoto.targetSprite
        || now.target_x != capture.sight.target_x || now.target_y != capture.sight.target_y
        || now.viewer_x != capture.sight.viewer_x || now.viewer_y != capture.sight.viewer_y)) status = RHH_PHOTO_STALE;
    if (status != RHH_PHOTO_OK) capture.status = RHH_PHOTO_STALE;
    else if (shader)
    {
        capture.backend = RHH_PHOTO_BACKEND_OPENGL;
        if (NativeShaderCapture(sPhoto.pixels)) capture.status = RHH_PHOTO_RENDER_FAILED;
        else
        {
            sPhoto.visibility = (struct RhhPhotoVisibility){.struct_size = sizeof(sPhoto.visibility), .ticket = capture.ticket};
            if (!NativeShaderPhotoMask(capture.ticket, pixels, frame, &sPhoto.visibility.total_pixels,
                                       &sPhoto.visibility.visible_pixels, sPhoto.visibilityMask, sPhoto.silhouetteMask))
                sPhoto.visibilityAvailable = TRUE;
            else if ((sPhoto.presentedOnly == RHH_PHOTO_REQUEST_PRESENTED_GEOMETRIC
                      || sPhoto.presentedOnly == RHH_PHOTO_REQUEST_ORDINARY_GEOMETRIC)
                     && !NativeShaderGeometricMask(capture.ticket, pixels, frame,
                        &sPhoto.visibility.total_pixels, &sPhoto.visibility.visible_pixels, sPhoto.visibilityMask, sPhoto.silhouetteMask))
                sPhoto.geometricAvailable = TRUE;
            else if (!PhotoModePresented(sPhoto.presentedOnly) && NativeShaderPhotoVisibilityCompatible(pixels, frame))
                sPhoto.visibilityAvailable = !NativePhotoVisibilityResult(capture.ticket, pixels,
                    &sPhoto.visibility.total_pixels, &sPhoto.visibility.visible_pixels, sPhoto.visibilityMask, sPhoto.silhouetteMask);
            if (PhotoModePresented(sPhoto.presentedOnly) && ((!sPhoto.visibilityAvailable && !sPhoto.geometricAvailable) || !sPhoto.visibility.visible_pixels))
                capture.status = RHH_PHOTO_NOT_VISIBLE;
        }
    }
    else if (PhotoModePresented(sPhoto.presentedOnly)) capture.status = RHH_PHOTO_NOT_VISIBLE;
    else if (!pixels) capture.status = RHH_PHOTO_RENDER_FAILED;
    else
    {
        capture.backend = RHH_PHOTO_BACKEND_SOFTWARE;
        for (u32 i = 0; i < RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT; ++i)
            for (u32 c = 0; c < 3; ++c)
            { u32 value = (pixels[i] >> (c * 5)) & 31; sPhoto.pixels[i * 3 + c] = (value << 3) | (value >> 2); }
        sPhoto.visibility = (struct RhhPhotoVisibility){.struct_size = sizeof(sPhoto.visibility), .ticket = capture.ticket};
        sPhoto.visibilityAvailable = !NativePhotoVisibilityResult(capture.ticket, pixels,
            &sPhoto.visibility.total_pixels, &sPhoto.visibility.visible_pixels, sPhoto.visibilityMask, sPhoto.silhouetteMask);
    }
    if (capture.status == RHH_PHOTO_OK)
    {
        sPhoto.render = (struct RhhPhotoRenderContext){.struct_size = sizeof(sPhoto.render),
            .backend = capture.backend, .frame = frame, .ticket = capture.ticket};
        sPhoto.renderVariant = 0;
        sPhoto.renderAvailable = !shader || !NativeShaderPhotoRenderInfo(capture.ticket, pixels, frame, &sPhoto.render, &sPhoto.renderVariant);
    }
    if (capture.status == RHH_PHOTO_OK && shader && (sPhoto.visibilityAvailable || sPhoto.geometricAvailable)
        && sPhoto.visibility.visible_pixels && !NativeShaderPhotoDepth(capture.ticket, pixels, frame, sPhoto.depthPixels))
    {
        sPhoto.depth = (struct RhhPhotoDepth){.struct_size = sizeof(sPhoto.depth), .ticket = capture.ticket,
            .mask_quality = sPhoto.geometricAvailable ? RHH_PHOTO_MASK_GEOMETRIC : RHH_PHOTO_MASK_EXACT,
            .visible_pixels = sPhoto.visibility.visible_pixels, .min_depth = 65535};
        for (u32 i = 0; i < RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT; ++i)
        {
            u32 depth = sPhoto.depthPixels[i];
            if (!sPhoto.visibilityMask[i]) { sPhoto.depthPixels[i] = 0; continue; }
            if (depth < sPhoto.depth.min_depth) sPhoto.depth.min_depth = depth;
            if (depth > sPhoto.depth.max_depth) sPhoto.depth.max_depth = depth;
            sPhoto.depth.depth_sum += depth;
        }
        sPhoto.depthAvailable = TRUE;
    }
    if (capture.status == RHH_PHOTO_OK && (sPhoto.visibilityAvailable || sPhoto.geometricAvailable))
    {
        struct RhhPhotoFootprint footprint = {.struct_size = sizeof(footprint)};
        int failed = sPhoto.geometricAvailable ? NativeShaderGeometricFootprint(capture.ticket, pixels, frame, &footprint)
            : shader && !NativeShaderPhotoVisibilityCompatible(pixels, frame)
            ? NativeShaderPhotoFootprint(capture.ticket, pixels, frame, &footprint)
            : NativePhotoVisibilityFootprint(capture.ticket, pixels, &footprint);
        if (!failed && PhotoFootprintValid(&footprint, capture.ticket))
        { sPhoto.footprint = footprint; sPhoto.footprintAvailable = TRUE; }
    }
    capture.frame = frame;
    struct RhhPhotoScoreContext scoreContext = {0};
    sPhoto.score = (struct RhhPhotoScore){.struct_size = sizeof(sPhoto.score)};
    if (capture.status == RHH_PHOTO_OK)
    {
        capture.sight = now; RememberScoutPhoto(&capture);
        PhotoEvaluate(&capture, &scoreContext); sPhoto.score = scoreContext.score;
    }
    sPhoto.stored = FALSE;
    sPhoto.albumSerial = capture.status == RHH_PHOTO_OK ? PhotoAllocateSerial() : 0;
    if (!RhhModsHasActiveRun() || sPhoto.run != RhhModsRunId())
    { sPhoto.busy = FALSE; NativePhotoVisibilityCancel(); return; }
    if (capture.status == RHH_PHOTO_OK)
    {
        sPhoto.recent = (struct RhhPhotoRecent){.struct_size = sizeof(sPhoto.recent), .ticket = capture.ticket,
            .photo = {RhhModsRunId(), sPhoto.albumSerial}, .individual = capture.sight.target_individual,
            .score = sPhoto.score};
        sPhoto.recent.score.photo = sPhoto.recent.photo;
    }
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    if (capture.status == RHH_PHOTO_OK) sPhotoScoreContext = &scoreContext;
    PhotoDispatch(capture.status == RHH_PHOTO_OK ? RHH_PHOTO_CAPTURED : RHH_PHOTO_FAILED, &capture, &decision);
    sPhotoScoreContext = NULL;
    sPhoto.busy = FALSE;
    sPhoto.visibilityAvailable = sPhoto.geometricAvailable = sPhoto.depthAvailable = sPhoto.renderAvailable = FALSE;
    NativePhotoVisibilityCancel();
}
static const struct RhhPhotoService sPhotoService = {
    {sizeof(sPhotoService), RHH_SERVICE_VERSION(1, 5)}, PhotoRequest, PhotoContext, PhotoCopyRgb,
    PhotoRequestPresented, PhotoRequiresPresenter, PhotoRequestPresentedGeometric, PhotoRequestMode, PhotoRenderContext, PhotoPresenterVariant,
    PhotoRequestGeometric
};

enum { PHOTO_ARCHIVE_HEADER = 16, PHOTO_ARCHIVE_META = 64,
       PHOTO_ARCHIVE_RECORD = PHOTO_ARCHIVE_META + RHH_PHOTO_RGB_BYTES };
static struct { struct RhhPhotoAlbumEntry entry; u8 pixels[RHH_PHOTO_RGB_BYTES]; } sAlbum[RHH_PHOTO_ALBUM_CAPACITY];
static u32 sAlbumCount;
static bool8 sAlbumBusy;
static const struct RhhPhotoAlbumEntry *sAlbumContext;
static void AlbumReset(void)
{
    memset(sAlbum, 0, sizeof(sAlbum)); sAlbumCount = 0; memset(&sPhoto.recent, 0, sizeof(sPhoto.recent));
    memset(&sScoutPhotoProof, 0, sizeof(sScoutPhotoProof));
    sProjectedProbe.pending = FALSE; sProjectedProbe.result.status = RHH_PROJECTED_STALE;
}
static int AlbumValidate(const u8 *data, u32 size)
{
    const u8 *scores = RhhModsState(sPhotoScoreStateId);
    if (!scores) return -1;
    if (!size) return data || WildActorRead64(scores) ? -1 : 0;
    if (!data || size < PHOTO_ARCHIVE_HEADER || memcmp(data, "RHHPH001", 8)
        || ScoutRead32(data + 8) != size || ScoutRead32(data + 12) > RHH_PHOTO_ALBUM_CAPACITY
        || size != PHOTO_ARCHIVE_HEADER + ScoutRead32(data + 12) * PHOTO_ARCHIVE_RECORD) return -1;
    const u8 *runState = RhhModsState("rhh:host_run");
    const u8 *sequence = RhhModsState(sPhotoSequenceId);
    if (!runState || !sequence || !WildActorRead64(runState + 8)) return -1;
    u64 run = 0;
    for (u32 i = 0; i < ScoutRead32(data + 12); ++i)
    {
        const u8 *p = data + PHOTO_ARCHIVE_HEADER + i * PHOTO_ARCHIVE_RECORD;
        u64 thisRun = WildActorRead64(p), serial = WildActorRead64(p + 8); u32 species, map;
        if (!run) run = thisRun;
        if (!thisRun || run != thisRun || thisRun != WildActorRead64(runState + 8)
            || !serial || serial > WildActorRead64(sequence) || !WildActorRead64(p + 16) || !WildActorRead64(p + 40)
            || RhhContentResolve(RHH_SPECIES, WildActorRead64(p + 24), &species)
            || RhhContentKeyForId(RHH_SPECIES, species) != WildActorRead64(p + 24)
            || RhhContentResolve(RHH_MAP, WildActorRead64(p + 32), &map)
            || RhhContentKeyForId(RHH_MAP, map) != WildActorRead64(p + 32)
            || (ScoutRead32(p + 60) != RHH_PHOTO_BACKEND_SOFTWARE && ScoutRead32(p + 60) != RHH_PHOTO_BACKEND_OPENGL)) return -1;
        for (u32 j = 0; j < i; ++j)
            if (WildActorRead64(data + PHOTO_ARCHIVE_HEADER + j * PHOTO_ARCHIVE_RECORD + 8) == serial) return -1;
    }
    for (u32 i = 0; i < RHH_PHOTO_ALBUM_CAPACITY; ++i)
    {
        const u8 *score = scores + i * PHOTO_SCORE_RECORD_SIZE; u64 scoreRun = WildActorRead64(score);
        if (!scoreRun) break;
        bool32 found = FALSE;
        for (u32 j = 0; j < ScoutRead32(data + 12); ++j)
        {
            const u8 *photo = data + PHOTO_ARCHIVE_HEADER + j * PHOTO_ARCHIVE_RECORD;
            if (WildActorRead64(photo) == scoreRun && WildActorRead64(photo + 8) == WildActorRead64(score + 8))
            { found = TRUE; break; }
        }
        if (!found) return -1;
    }
    return 0;
}
static int AlbumExport(u8 **data, u32 *size)
{
    if (!data || !size) return -1;
    *data = NULL; *size = 0;
    if (!sAlbumCount) return AlbumValidate(NULL, 0);
    u32 length = PHOTO_ARCHIVE_HEADER + sAlbumCount * PHOTO_ARCHIVE_RECORD;
    u8 *bytes = calloc(length, 1); if (!bytes) return -1;
    memcpy(bytes, "RHHPH001", 8); ScoutWrite32(bytes + 8, length); ScoutWrite32(bytes + 12, sAlbumCount);
    for (u32 i = 0; i < sAlbumCount; ++i)
    {
        u8 *p = bytes + PHOTO_ARCHIVE_HEADER + i * PHOTO_ARCHIVE_RECORD;
        const struct RhhPhotoAlbumEntry *e = &sAlbum[i].entry;
        WildActorWrite64(p, e->id.run_id); WildActorWrite64(p + 8, e->id.serial);
        WildActorWrite64(p + 16, e->individual.serial); WildActorWrite64(p + 24, e->species);
        WildActorWrite64(p + 32, e->map); WildActorWrite64(p + 40, e->owner); WildActorWrite64(p + 48, e->step);
        ScoutWrite32(p + 56, e->frame); ScoutWrite32(p + 60, e->backend);
        memcpy(p + PHOTO_ARCHIVE_META, sAlbum[i].pixels, RHH_PHOTO_RGB_BYTES);
    }
    if (AlbumValidate(bytes, length)) { free(bytes); return -1; }
    *data = bytes; *size = length; return 0;
}
static int AlbumImport(const u8 *data, u32 size)
{
    if (AlbumValidate(data, size)) return -1;
    AlbumReset(); if (!size) return 0;
    sAlbumCount = ScoutRead32(data + 12);
    for (u32 i = 0; i < sAlbumCount; ++i)
    {
        const u8 *p = data + PHOTO_ARCHIVE_HEADER + i * PHOTO_ARCHIVE_RECORD; u64 run = WildActorRead64(p);
        sAlbum[i].entry = (struct RhhPhotoAlbumEntry){.struct_size = sizeof(sAlbum[i].entry),
            .id = {run, WildActorRead64(p + 8)}, .individual = {.struct_size = sizeof(struct RhhWildActorIndividual),
                .run_id = run, .serial = WildActorRead64(p + 16)}, .species = WildActorRead64(p + 24),
            .map = WildActorRead64(p + 32), .owner = WildActorRead64(p + 40), .step = WildActorRead64(p + 48),
            .frame = ScoutRead32(p + 56), .backend = ScoutRead32(p + 60)};
        memcpy(sAlbum[i].pixels, p + PHOTO_ARCHIVE_META, RHH_PHOTO_RGB_BYTES);
    }
    return 0;
}
static const struct NativeSaveArchiveCodec sAlbumCodec = {
    AlbumExport, AlbumValidate, AlbumImport, AlbumReset, "rhh:photo_album", "RHHPH001"
};
static u32 AlbumCount(void)
{ return RhhModsHasActiveRun() && (!sAlbumCount || sAlbum[0].entry.id.run_id == RhhModsRunId()) ? sAlbumCount : 0; }
static int AlbumIndex(const struct RhhPhotoId *id)
{
    if (!id || !id->run_id || !id->serial || !RhhModsHasActiveRun() || id->run_id != RhhModsRunId()) return -1;
    for (u32 i = 0; i < AlbumCount(); ++i) if (sAlbum[i].entry.id.serial == id->serial) return i;
    return -1;
}
static int AlbumAt(u32 index, struct RhhPhotoAlbumEntry *out)
{
    if (!out || out->struct_size < sizeof(*out) || index >= AlbumCount()) return -1;
    *out = sAlbum[index].entry; return 0;
}
static int AlbumFind(const struct RhhPhotoId *id, struct RhhPhotoAlbumEntry *out)
{ int i = AlbumIndex(id); return i < 0 ? -1 : AlbumAt(i, out); }
static int PhotoScoreFind(const struct RhhPhotoId *id, struct RhhPhotoScore *out)
{
    int index = PhotoScoreIndex(id); const u8 *state = RhhModsState(sPhotoScoreStateId);
    if (!out || out->struct_size < sizeof(*out) || index < 0 || AlbumIndex(id) < 0 || !state) return -1;
    PhotoScoreRead(state + index * PHOTO_SCORE_RECORD_SIZE, out); return 0;
}
static int PhotoScoreRecent(struct RhhPhotoRecent *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun()
        || !sPhoto.recent.ticket || sPhoto.recent.photo.run_id != RhhModsRunId()) return -1;
    *out = sPhoto.recent; return 0;
}
static int AlbumCopy(const struct RhhPhotoId *id, u8 *out, u32 size)
{
    int i = AlbumIndex(id);
    if (i < 0 || !out || size < RHH_PHOTO_RGB_BYTES) return -1;
    memcpy(out, sAlbum[i].pixels, RHH_PHOTO_RGB_BYTES); return 0;
}
static int AlbumContext(struct RhhPhotoAlbumEntry *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sAlbumContext
        || (event != RHH_PHOTO_STORED && event != RHH_PHOTO_REMOVED)) return -1;
    *out = *sAlbumContext; return 0;
}
static void AlbumDispatch(u32 kind, const struct RhhPhotoAlbumEntry *entry)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = entry->map, .subject = entry->species, .ordinal = entry->id.serial};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sAlbumContext = entry; RhhModsDispatch(&event, &decision); sAlbumContext = NULL;
}
static int AlbumStore(u64 ticket, struct RhhPhotoAlbumEntry *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ticket) return RHH_ALBUM_INVALID;
    if (RhhModsCurrentEvent() != RHH_PHOTO_CAPTURED || !sPhotoContext || sPhotoContext->ticket != ticket
        || sAlbumBusy || !sPhoto.albumSerial || NativeSaveTransactionActive()) return RHH_ALBUM_UNAVAILABLE;
    if (sPhoto.stored) return RHH_ALBUM_DUPLICATE;
    if (sAlbumCount && sAlbum[0].entry.id.run_id != RhhModsRunId()) return RHH_ALBUM_UNAVAILABLE;
    if (sAlbumCount >= RHH_PHOTO_ALBUM_CAPACITY) return RHH_ALBUM_FULL;
    const struct RhhWorldClockService *clock = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldClockSnapshot now = {.struct_size = sizeof(now)};
    if (!clock || clock->current(&now)) return RHH_ALBUM_UNAVAILABLE;
    const struct RhhPhotoCapture *capture = sPhotoContext;
    struct RhhPhotoAlbumEntry entry = {.struct_size = sizeof(entry), .backend = capture->backend, .frame = capture->frame,
        .id = {RhhModsRunId(), sPhoto.albumSerial}, .individual = capture->sight.target_individual,
        .species = capture->sight.target_wild.species, .map = capture->sight.map, .owner = capture->owner, .step = now.steps};
    if ((sPhoto.score.flags & RHH_PHOTO_SCORE_AVAILABLE) && PhotoScoreInsert(&sPhoto.score, &entry.id))
        return RHH_ALBUM_UNAVAILABLE;
    sAlbum[sAlbumCount].entry = entry; memcpy(sAlbum[sAlbumCount].pixels, sPhoto.pixels, RHH_PHOTO_RGB_BYTES);
    ++sAlbumCount; sPhoto.stored = TRUE; *out = entry;
    sAlbumBusy = TRUE; AlbumDispatch(RHH_PHOTO_STORED, &entry); sAlbumBusy = FALSE;
    return RHH_ALBUM_OK;
}
static int AlbumRemove(const struct RhhPhotoId *id)
{
    int index = AlbumIndex(id);
    if (index < 0) return RHH_ALBUM_INVALID;
    if (sAlbumBusy || !RhhModsCommandsAllowed() || NativeSaveTransactionActive()) return RHH_ALBUM_UNAVAILABLE;
    struct RhhPhotoAlbumEntry removed = sAlbum[index].entry;
    PhotoScoreRemove(&removed.id);
    if ((u32)index + 1 < sAlbumCount)
        memmove(&sAlbum[index], &sAlbum[index + 1], (sAlbumCount - index - 1) * sizeof(sAlbum[0]));
    memset(&sAlbum[--sAlbumCount], 0, sizeof(sAlbum[0]));
    sAlbumBusy = TRUE; AlbumDispatch(RHH_PHOTO_REMOVED, &removed); sAlbumBusy = FALSE;
    return RHH_ALBUM_OK;
}
static const struct RhhPhotoAlbumService sAlbumService = {
    {sizeof(sAlbumService), RHH_SERVICE_VERSION(1, 0)}, AlbumCount, AlbumAt, AlbumFind, AlbumCopy, AlbumStore, AlbumRemove, AlbumContext
};
static const struct RhhPhotoScoreService sPhotoScoreService = {
    {sizeof(sPhotoScoreService), RHH_SERVICE_VERSION(1, 1)}, PhotoScoreContext, PhotoScoreFind, PhotoScoreRecent
};
static const struct RhhWorldSceneService sSceneService = {
    {sizeof(sSceneService), RHH_SERVICE_VERSION(1, 11)},
    SceneTileAt, SceneMeshAt, BuildSceneMesh, SceneWindow, SceneWindowTileAt, SceneWindowMeshAt,
    SceneWindowResolve, SceneWindowResolvedMeshAt, SceneLightsAt, RhhModsSceneLightCount, SceneTraceLight, RhhModsSceneShadowCasterCount,
    SceneWindowInfer, SceneWindowInferredMeshAt, SceneTraceInferredLight, RhhSceneTraceBillboard, SceneTraceDirectionalLight,
    SceneTraceDirectionalShadow
};
static const struct RhhOverworldService sService = {
    {sizeof(sService), RHH_SERVICE_VERSION(1, 11)}, ActorSlots, Current, TileAt, ActorAt, SpriteRgba, TileRgba,
    Camera, Billboard, SpritePart, SpritePartRgba, EffectSlots, EffectAt, EffectPart, EffectPartRgba, EffectAtV13, ActorShadow, LightingInputs, LocalLightSlots, LocalLightAt, ActorVisibility,
    SpritePartTransform, SpritePartTransformedRgba, EffectPartTransform, EffectPartTransformedRgba, DisplayState,
    SpritePartRasterRgba, EffectPartRasterRgba, DisplayFrame
};
int RhhModsInstallWorldService(void)
{
    RhhModsResetNaturalWeatherObservation();
    memset(sWildActors, 0, sizeof(sWildActors));
    sWildBehaviorBusy = FALSE; sWildBehaviorCount = 0; sWildBehaviorContext = NULL;
    sWildProfileBusy = sWildProfileSchemaBusy = sWildProfileSchemaInvalid = FALSE; sWildProfileSchemaCount = 0;
    sWildProfileContext = NULL; sWildProfileBatchContext = NULL;
    sWildActivityCollecting = sWildActivityInvalid = FALSE; sWildActivityCount = 0;
    sWildSelectionContext = NULL; sWildActivityChangeContext = NULL;
    sWildQueueChangeContext = NULL;
    sWildQueueGoalContext = NULL;
    sWildPolicyContext = NULL;
    sBerryWaterBusy = FALSE; sBerryWaterContext = NULL; sBerryManualContext = NULL;
    memset(&sWildActorLease, 0, sizeof(sWildActorLease));
    NativePhotoVisibilityCancel();
    memset(&sPhoto, 0, sizeof(sPhoto)); sPhotoContext = NULL; sPhotoScoreContext = NULL;
    memset(&sProjectedProbe, 0, sizeof(sProjectedProbe));
    memset(&sPhotoLease, 0, sizeof(sPhotoLease)); AlbumReset(); WildDormantReset(); sAlbumBusy = FALSE; sAlbumContext = NULL;
    ModsWildActorBattleReset();
    if (RhhModsRegister(&sWildActorState)) return -1;
    if (RhhModsRegister(&sWildActorSequence)) return -1;
    if (RhhModsRegister(&sWildConcealState)) return -1;
    if (RhhModsRegister(&sWildTraitState)) return -1;
    if (RhhModsRegister(&sWildProfileState)) return -1;
    if (RhhModsRegister(&sWildActivityState)) return -1;
    if (RhhModsRegister(&sWildPolicyState)) return -1;
    if (RhhModsRegister(&sWildQueueState)) return -1;
    if (RhhModsRegister(&sScoutingState)) return -1;
    if (RhhModsRegister(&sPhotoSequence)) return -1;
    if (RhhModsRegister(&sPhotoScoreState)) return -1;
    if (NativeSaveRegisterArchive(&sAlbumCodec)) return -1;
    if (NativeSaveRegisterArchive(&sWildDormantCodec)) return -1;
    if (RhhModsSetBeforeSave(ModsWildActorsBeforeSave)) return -1;
    if (RhhModsAddService("rhh:service/overworld", &sService.header)) return -1;
    if (RhhModsAddService("rhh:service/authored_maps", &sAuthoredMapService.header)) return -1;
    if (RhhModsAddService("rhh:service/authored_encounters", &sAuthoredEncounterService.header)) return -1;
    if (RhhModsAddService("rhh:service/authored_event_state", &sAuthoredEventStateService.header)) return -1;
    if (RhhModsAddService("rhh:service/object_visuals", &sObjectVisualService.header)) return -1;
    if (RhhModsAddService("rhh:service/species_visuals", &sSpeciesVisualService.header)) return -1;
    if (RhhModsAddService("rhh:service/world_scene", &sSceneService.header)) return -1;
    if (RhhModsInstallRenderService()) return -1;
    if (RhhModsAddService("rhh:service/habitat", &sHabitatService.header)) return -1;
    if (RhhModsAddService("rhh:service/ground_effect", &sGroundEffectService.header)) return -1;
    if (RhhModsAddService("rhh:service/berry_plot", &sBerryPlotService.header)) return -1;
    if (RhhModsInstallEcologyTagsService()) return -1;
    if (RhhModsInstallHabitatRegistryService()) return -1;
    if (RhhModsAddService("rhh:service/wild_traits", &sWildTraitsService.header)) return -1;
    if (RhhModsAddService("rhh:service/wild_trait_edits", &sWildTraitEditsService.header)) return -1;
    if (RhhModsAddService("rhh:service/wild_profile", &sWildProfileService.header)) return -1;
    if (RhhModsAddService("rhh:service/wild_behavior", &sWildBehaviorService.header)) return -1;
    if (RhhModsAddService("rhh:service/wild_actor", &sWildActorService.header)) return -1;
    if (RhhModsAddService("rhh:service/wild_dormant", &sWildDormantService.header)) return -1;
    if (RhhModsAddService("rhh:service/wild_concealment", &sWildConcealService.header)) return -1;
    if (RhhModsAddService("rhh:service/perception", &sPerceptionService.header)) return -1;
    if (RhhModsAddService("rhh:service/projected_visibility", &sProjectedVisibilityService.header)) return -1;
    if (RhhModsAddService("rhh:service/scouting", &sScoutingService.header)) return -1;
    if (RhhModsAddService("rhh:service/photo_capture", &sPhotoService.header)) return -1;
    if (RhhModsAddService("rhh:service/photo_visibility", &sPhotoVisibilityService.header)) return -1;
    if (RhhModsAddService("rhh:service/photo_album", &sAlbumService.header)) return -1;
    return RhhModsAddService("rhh:service/photo_score", &sPhotoScoreService.header);
}
