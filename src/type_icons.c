#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_controllers.h"
#include "battle_gimmick.h"
#include "battle_util.h"
#include "decompress.h"
#include "graphics.h"
#include "pokedex.h"
#include "sprite.h"
#include "type_icons.h"
#ifdef NATIVE
#include "mods.h"
#endif

static void LoadTypeSpritesAndPalettes(void);
static void LoadTypeIconsPerBattler(enum BattlerId, u32);

static bool32 UseDoubleBattleCoords(u32);

#ifndef NATIVE
static enum Type GetMonPublicType(enum BattlerId, u32);
#endif
static bool32 ShouldHideUncaughtType(enum Species species);
static bool32 ShouldHideUnseenType(enum Species species);
#ifndef NATIVE
static enum Type GetMonDefensiveTeraType(struct Pokemon *, struct Pokemon *, enum BattlerId, u32, enum Species, enum Species);
#endif
static bool32 IsIllusionActiveAndTypeUnchanged(struct Pokemon *, enum Species, enum BattlerId);

static void CreateSpriteFromType(u32, bool32, enum Type[], u32, u32, enum BattlerId);
static bool32 ShouldSkipSecondType(enum Type[], u32);
static void SetTypeIconXY(s32*, s32*, u32, bool32, u32);

static void CreateSpriteAndSetTypeSpriteAttributes(enum Type, u32 x, u32 y, u32, enum BattlerId, bool32, u32, u32);
static bool32 ShouldFlipTypeIcon(bool32, u32, enum Type);

static void SpriteCB_TypeIcon(struct Sprite*);
static void DestroyTypeIcon(struct Sprite*);
static void FreeAllTypeIconResources(void);
static bool32 ShouldHideTypeIcon(enum BattlerId);
static s32 GetTypeIconHideMovement(bool32, u32);
static s32 GetTypeIconSlideMovement(bool32, u32, s32);
static s32 GetTypeIconBounceMovement(s32, u32);

const struct Coords16 sTypeIconPositions[][2] =
{
    [B_POSITION_PLAYER_LEFT] =
    {
        [FALSE] = {221, 86},
        [TRUE] = {144, 71},
    },
    [B_POSITION_OPPONENT_LEFT] =
    {
        [FALSE] = {20, 26},
        [TRUE] = {97, 14},
    },
    [B_POSITION_PLAYER_RIGHT] =
    {
        [TRUE] = {156, 96},
    },
    [B_POSITION_OPPONENT_RIGHT] =
    {
        [TRUE] = {85, 39},
    },
};

const union AnimCmd sSpriteAnim_TypeIcon_Normal[] =
{
    ANIMCMD_FRAME(TYPE_ICON_1_FRAME(TYPE_NORMAL), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Fighting[] =
{
    ANIMCMD_FRAME(TYPE_ICON_1_FRAME(TYPE_FIGHTING), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Flying[] =
{
    ANIMCMD_FRAME(TYPE_ICON_1_FRAME(TYPE_FLYING), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Poison[] =
{
    ANIMCMD_FRAME(TYPE_ICON_1_FRAME(TYPE_POISON), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Ground[] =
{
    ANIMCMD_FRAME(TYPE_ICON_1_FRAME(TYPE_GROUND), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Rock[] =
{
    ANIMCMD_FRAME(TYPE_ICON_1_FRAME(TYPE_ROCK), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Bug[] =
{
    ANIMCMD_FRAME(TYPE_ICON_1_FRAME(TYPE_BUG), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Ghost[] =
{
    ANIMCMD_FRAME(TYPE_ICON_1_FRAME(TYPE_GHOST), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Steel[] =
{
    ANIMCMD_FRAME(TYPE_ICON_1_FRAME(TYPE_STEEL), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Mystery[] =
{
    ANIMCMD_FRAME(TYPE_ICON_1_FRAME(TYPE_MYSTERY), 0),
    ANIMCMD_END
};

const union AnimCmd sSpriteAnim_TypeIcon_Fire[] =
{
    ANIMCMD_FRAME(TYPE_ICON_2_FRAME(TYPE_FIRE), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Water[] =
{
    ANIMCMD_FRAME(TYPE_ICON_2_FRAME(TYPE_WATER), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Grass[] =
{
    ANIMCMD_FRAME(TYPE_ICON_2_FRAME(TYPE_GRASS), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Electric[] =
{
    ANIMCMD_FRAME(TYPE_ICON_2_FRAME(TYPE_ELECTRIC), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Psychic[] =
{
    ANIMCMD_FRAME(TYPE_ICON_2_FRAME(TYPE_PSYCHIC), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Ice[] =
{
    ANIMCMD_FRAME(TYPE_ICON_2_FRAME(TYPE_ICE), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Dragon[] =
{
    ANIMCMD_FRAME(TYPE_ICON_2_FRAME(TYPE_DRAGON), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Dark[] =
{
    ANIMCMD_FRAME(TYPE_ICON_2_FRAME(TYPE_DARK), 0),
    ANIMCMD_END
};
const union AnimCmd sSpriteAnim_TypeIcon_Fairy[] =
{
    ANIMCMD_FRAME(TYPE_ICON_2_FRAME(TYPE_FAIRY), 0),
    ANIMCMD_END
};

const union AnimCmd *const sSpriteAnimTable_TypeIcons[] =
{
    [TYPE_NONE] =       sSpriteAnim_TypeIcon_Mystery,
    [TYPE_NORMAL] =     sSpriteAnim_TypeIcon_Normal,
    [TYPE_FIGHTING] =   sSpriteAnim_TypeIcon_Fighting,
    [TYPE_FLYING] =     sSpriteAnim_TypeIcon_Flying,
    [TYPE_POISON] =     sSpriteAnim_TypeIcon_Poison,
    [TYPE_GROUND] =     sSpriteAnim_TypeIcon_Ground,
    [TYPE_ROCK] =       sSpriteAnim_TypeIcon_Rock,
    [TYPE_BUG] =        sSpriteAnim_TypeIcon_Bug,
    [TYPE_GHOST] =      sSpriteAnim_TypeIcon_Ghost,
    [TYPE_STEEL] =      sSpriteAnim_TypeIcon_Steel,
    [TYPE_MYSTERY] =    sSpriteAnim_TypeIcon_Mystery,
    [TYPE_FIRE] =       sSpriteAnim_TypeIcon_Fire,
    [TYPE_WATER] =      sSpriteAnim_TypeIcon_Water,
    [TYPE_GRASS] =      sSpriteAnim_TypeIcon_Grass,
    [TYPE_ELECTRIC] =   sSpriteAnim_TypeIcon_Electric,
    [TYPE_PSYCHIC] =    sSpriteAnim_TypeIcon_Psychic,
    [TYPE_ICE] =        sSpriteAnim_TypeIcon_Ice,
    [TYPE_DRAGON] =     sSpriteAnim_TypeIcon_Dragon,
    [TYPE_DARK] =       sSpriteAnim_TypeIcon_Dark,
    [TYPE_FAIRY] =      sSpriteAnim_TypeIcon_Fairy,
    [TYPE_STELLAR] =    sSpriteAnim_TypeIcon_Mystery,
};

const struct SpritePalette sTypeIconPal1 =
{
    .data = gBattleIcons_Pal1,
    .tag = TYPE_ICON_TAG
};

const struct SpritePalette sTypeIconPal2 =
{
    .data = gBattleIcons_Pal2,
    .tag = TYPE_ICON_TAG_2
};

const struct OamData sOamData_TypeIcons =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(8x16),
    .size = SPRITE_SIZE(8x16),
    .priority = 1,
};

const struct CompressedSpriteSheet sSpriteSheet_TypeIcons2 =
{
    .data = gBattleIcons_Gfx2,
    .size = (8*16) * 9,
    .tag = TYPE_ICON_TAG_2,
};

const struct CompressedSpriteSheet sSpriteSheet_TypeIcons1 =
{
    .data = gBattleIcons_Gfx1,
    .size = (8*16) * 10,
    .tag = TYPE_ICON_TAG,
};

const struct SpriteTemplate sSpriteTemplate_TypeIcons1 =
{
    .tileTag = TYPE_ICON_TAG,
    .paletteTag = TYPE_ICON_TAG,
    .oam = &sOamData_TypeIcons,
    .anims = sSpriteAnimTable_TypeIcons,
    .callback = SpriteCB_TypeIcon
};

const struct SpriteTemplate sSpriteTemplate_TypeIcons2 =
{
    .tileTag = TYPE_ICON_TAG_2,
    .paletteTag = TYPE_ICON_TAG_2,
    .oam = &sOamData_TypeIcons,
    .anims = sSpriteAnimTable_TypeIcons,
    .callback = SpriteCB_TypeIcon
};

void LoadTypeIcons(enum BattlerId battler)
{
    u32 position;

    struct Pokemon* mon = GetBattlerMon(battler);
    enum Species species = GetMonData(mon, MON_DATA_SPECIES);

    if (B_SHOW_TYPES == SHOW_TYPES_NEVER
        || (B_SHOW_TYPES == SHOW_TYPES_SEEN && !GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_GET_SEEN)))
        return;

    LoadTypeSpritesAndPalettes();

    for (position = 0; position < gBattlersCount; ++position)
        LoadTypeIconsPerBattler(battler, position);
}

static void LoadTypeSpritesAndPalettes(void)
{
    if (IndexOfSpritePaletteTag(TYPE_ICON_TAG) != UCHAR_MAX)
        return;

    LoadCompressedSpriteSheet(&sSpriteSheet_TypeIcons1);
    LoadCompressedSpriteSheet(&sSpriteSheet_TypeIcons2);
    LoadSpritePalette(&sTypeIconPal1);
    LoadSpritePalette(&sTypeIconPal2);
}

static void LoadTypeIconsPerBattler(enum BattlerId battler, u32 position)
{
    u32 typeNum;
#ifdef NATIVE
    enum Type types[3];
#else
    enum Type types[2];
#endif
    enum BattlerId battlerId = GetBattlerAtPosition(position);
    bool32 useDoubleBattleCoords = UseDoubleBattleCoords(battlerId);

    if (!IsBattlerAlive(battlerId))
        return;

#ifdef NATIVE
    u32 typeCount = GetBattlerPublicTypeIcons(battlerId, types);
#else
    u32 typeCount = 2;
    for (typeNum = 0; typeNum < typeCount; ++typeNum)
        types[typeNum] = GetMonPublicType(battlerId, typeNum);
#endif

    for (typeNum = 0; typeNum < typeCount; ++typeNum)
        CreateSpriteFromType(position, useDoubleBattleCoords, types, typeNum, typeCount, battler);
}

#ifdef NATIVE
u32 GetBattlerPublicTypeIcons(enum BattlerId battler, enum Type types[3])
{
    struct BattleTypeList list;
    GetBattlerPublicTypeList(battler, &list);
    u32 count = min(list.count, 3);
    for (u32 i = 0; i < count; ++i) types[i] = list.types[i];
    return count;
}

void GetBattlerPublicTypeList(enum BattlerId battler, struct BattleTypeList *list)
{
    struct Pokemon *mon = GetBattlerMon(battler);
    enum Species species = GetMonData(mon, MON_DATA_SPECIES);
    struct Pokemon *illusion = GetIllusionMonPtr(battler);
    list->count = 0;
    if (!ShouldHideUncaughtType(species) && !ShouldHideUnseenType(species))
    {
        bool32 tera = GetActiveGimmick(battler) == GIMMICK_TERA;
        if (tera && GetBattlerTeraType(battler) != TYPE_STELLAR)
            BattleTypeListAdd(list, GetBattlerTeraType(battler));
        else if (IsIllusionActiveAndTypeUnchanged(illusion, species, battler))
        {
            GetSpeciesTypeList(GetMonData(illusion, MON_DATA_SPECIES), list);
            if (!tera && gBattleMons[battler].volatiles.roostActive) ApplyRoostToTypeList(list);
        }
        else
            GetBattlerTypeList(battler, FALSE, list);
    }
    if (!list->count) { list->types[0] = TYPE_MYSTERY; list->count = 1; }
}
#endif

static bool32 UseDoubleBattleCoords(u32 position)
{
    if (!IsDoubleBattle())
        return FALSE;

    if ((position == B_POSITION_PLAYER_LEFT) && (gBattleMons[B_POSITION_PLAYER_RIGHT].species == SPECIES_NONE))
        return FALSE;

    if ((position == B_POSITION_OPPONENT_LEFT) && (gBattleMons[B_POSITION_OPPONENT_RIGHT].species == SPECIES_NONE))
        return FALSE;

    return TRUE;
}

#ifndef NATIVE
static enum Type GetMonPublicType(enum BattlerId battlerId, u32 typeNum)
{
    struct Pokemon *mon = GetBattlerMon(battlerId);
    enum Species monSpecies = GetMonData(mon,MON_DATA_SPECIES,NULL);
    struct Pokemon *monIllusion;
    enum Species illusionSpecies;

    if (ShouldHideUncaughtType(monSpecies) || ShouldHideUnseenType(monSpecies))
        return TYPE_MYSTERY;

    monIllusion = GetIllusionMonPtr(battlerId);
    illusionSpecies = monIllusion ? GetMonData(monIllusion,MON_DATA_SPECIES,NULL) : SPECIES_NONE;

    if (GetActiveGimmick(battlerId) == GIMMICK_TERA)
        return GetMonDefensiveTeraType(mon,monIllusion,battlerId,typeNum,illusionSpecies,monSpecies);

    if (IsIllusionActiveAndTypeUnchanged(monIllusion,monSpecies, battlerId))
        return GetSpeciesType(illusionSpecies, typeNum);

    return gBattleMons[battlerId].types[typeNum];
}
#endif

static bool32 ShouldHideUncaughtType(enum Species species)
{
    if (B_SHOW_TYPES != SHOW_TYPES_CAUGHT)
        return FALSE;

    if (GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_GET_CAUGHT))
        return FALSE;

    return TRUE;
}

static bool32 ShouldHideUnseenType(enum Species species)
{
    if (B_SHOW_TYPES != SHOW_TYPES_SEEN)
        return FALSE;

    if (GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_GET_SEEN))
        return FALSE;

    return TRUE;
}

#ifndef NATIVE
static enum Type GetMonDefensiveTeraType(struct Pokemon *mon, struct Pokemon *monIllusion, enum BattlerId battlerId, u32 typeNum, enum Species illusionSpecies, enum Species monSpecies)
{
    enum Type teraType = GetBattlerTeraType(battlerId);
    enum Species targetSpecies;

    if (teraType != TYPE_STELLAR)
        return teraType;

    targetSpecies = (monIllusion != NULL) ? illusionSpecies : monSpecies;

    return GetSpeciesType(targetSpecies, typeNum);
}
#endif

static bool32 IsIllusionActiveAndTypeUnchanged(struct Pokemon *monIllusion, enum Species monSpecies, enum BattlerId battlerId)
{
    if (monIllusion == NULL)
        return FALSE;

#ifdef NATIVE
    struct BattleTypeList speciesTypes, active;
    GetSpeciesTypeList(monSpecies, &speciesTypes);
    GetBattlerRawTypeList(battlerId, &active);
    return BattleTypeListsEqual(&speciesTypes, &active);
#else
    u32 typeNum;
    for (typeNum = 0; typeNum < 2; typeNum++)
        if (GetSpeciesType(monSpecies, typeNum) != gBattleMons[battlerId].types[typeNum])
        return FALSE;
#endif

    return TRUE;
}

static void CreateSpriteFromType(u32 position, bool32 useDoubleBattleCoords, enum Type types[], u32 typeNum, u32 typeCount, enum BattlerId battler)
{
    s32 x = 0, y = 0;

    if (ShouldSkipSecondType(types, typeNum))
        return;

    SetTypeIconXY(&x, &y, position, useDoubleBattleCoords, typeNum);

    CreateSpriteAndSetTypeSpriteAttributes(types[typeNum], x, y, position, battler, useDoubleBattleCoords, typeNum, typeCount);
}

static bool32 ShouldSkipSecondType(enum Type types[], u32 typeNum)
{
    if (!typeNum)
        return FALSE;

    if (types[0] != types[1])
        return FALSE;

    return TRUE;
}

static void SetTypeIconXY(s32* x, s32* y, u32 position, bool32 useDoubleBattleCoords, u32 typeNum)
{
    *x = sTypeIconPositions[position][useDoubleBattleCoords].x;
    *y = sTypeIconPositions[position][useDoubleBattleCoords].y + (11 * typeNum);
}

static void CreateSpriteAndSetTypeSpriteAttributes(enum Type type, u32 x, u32 y, u32 position, enum BattlerId battler, bool32 useDoubleBattleCoords, u32 slot, u32 slots)
{
    struct Sprite* sprite;
#ifdef NATIVE
    u32 originalType = type;
    if (type >= NUMBER_OF_MON_TYPES) type = TYPE_MYSTERY;
#endif
    const struct SpriteTemplate* spriteTemplate = gTypesInfo[type].useSecondTypeIconPalette ? &sSpriteTemplate_TypeIcons2 : &sSpriteTemplate_TypeIcons1;
    u32 spriteId = CreateSpriteAtEndUnchecked(spriteTemplate, x, y, UCHAR_MAX);

    if (spriteId == MAX_SPRITES)
        return;

    sprite = &gSprites[spriteId];
    sprite->tMonPosition = position;
    sprite->tBattlerId = battler;
    sprite->tVerticalPosition = y;

    sprite->hFlip = ShouldFlipTypeIcon(useDoubleBattleCoords, position, type);

    StartSpriteAnim(sprite, type);
#ifdef NATIVE
    sprite->tTypeIconSlot = slot;
    sprite->tTypeIconTimer = 0;
    sprite->tTypeIconPage = slots;
    sprite->tTypeIconType = originalType;
    if (originalType >= NUMBER_OF_MON_TYPES && ModsSetCustomTypeIcon(sprite, originalType, TRUE))
        sprite->hFlip = FALSE;
#else
    (void)slot; (void)slots;
#endif
}

static bool32 ShouldFlipTypeIcon(bool32 useDoubleBattleCoords, u32 position, enum Type typeId)
{
    enum BattleSide side = (useDoubleBattleCoords) ? B_SIDE_OPPONENT : B_SIDE_PLAYER;

    if (GetBattlerSide(GetBattlerAtPosition(position)) != side)
        return FALSE;

    return !gTypesInfo[typeId].isSpecialCaseType;
}

#ifdef NATIVE
static void UpdateTypeIconPage(struct Sprite *sprite, bool32 useDoubleBattleCoords)
{
    bool32 advance = ++sprite->tTypeIconTimer >= 90;
    if (!advance && sprite->tTypeIconTimer % 15) return;
    if (advance) sprite->tTypeIconTimer = 0;
    struct BattleTypeList types;
    enum BattlerId shownBattler = GetBattlerAtPosition(sprite->tMonPosition);
    GetBattlerPublicTypeList(shownBattler, &types);
    u32 slots = sprite->tTypeIconPage & 3;
    u32 pages = (types.count + slots - 1) / slots;
    u32 page = ((u32)sprite->tTypeIconPage >> 2) % pages;
    if (advance) page = (page + 1) % pages;
    sprite->tTypeIconPage = slots | (page << 2);
    u32 index = page * slots + sprite->tTypeIconSlot;
    if (index >= types.count)
    {
        sprite->invisible = TRUE;
        ModsClearCustomTypeIcon(sprite);
        sprite->tTypeIconType = -1;
        return;
    }
    sprite->invisible = FALSE;
    u32 type = types.types[index];
    if (sprite->tTypeIconType == type) return;
    ModsClearCustomTypeIcon(sprite);
    u32 displayType = type < NUMBER_OF_MON_TYPES ? type : TYPE_MYSTERY;
    const struct SpriteTemplate *template = gTypesInfo[displayType].useSecondTypeIconPalette
        ? &sSpriteTemplate_TypeIcons2 : &sSpriteTemplate_TypeIcons1;
    sprite->template = template;
    sprite->anims = template->anims;
    sprite->sheetTileStart = GetSpriteTileStartByTag(template->tileTag);
    sprite->oam.tileNum = sprite->sheetTileStart;
    u32 palette = IndexOfSpritePaletteTag(template->paletteTag);
    sprite->oam.paletteNum = palette < 16 ? palette : 15;
    sprite->hFlip = ShouldFlipTypeIcon(useDoubleBattleCoords, sprite->tMonPosition, displayType);
    StartSpriteAnim(sprite, displayType);
    if (type >= NUMBER_OF_MON_TYPES && ModsSetCustomTypeIcon(sprite, type, TRUE)) sprite->hFlip = FALSE;
    sprite->tTypeIconType = type;
}

#ifdef RHH_BATTLE_TYPE_ICON_TEST
void RhhTestAdvanceBattleTypeIcon(struct Sprite *sprite)
{
    UpdateTypeIconPage(sprite, UseDoubleBattleCoords(GetBattlerAtPosition(sprite->tMonPosition)));
}
#endif
#endif

static void SpriteCB_TypeIcon(struct Sprite *sprite)
{
    u32 position = sprite->tMonPosition;
    enum BattlerId battlerId = sprite->tBattlerId;
    bool32 useDoubleBattleCoords = UseDoubleBattleCoords(GetBattlerAtPosition(position));

    if (sprite->tHideIconTimer == NUM_FRAMES_HIDE_TYPE_ICON)
    {
        DestroyTypeIcon(sprite);
        return;
    }

    if (ShouldHideTypeIcon(battlerId))
    {
        sprite->x += GetTypeIconHideMovement(useDoubleBattleCoords, position);
        ++sprite->tHideIconTimer;
        return;
    }

#ifdef NATIVE
    UpdateTypeIconPage(sprite, useDoubleBattleCoords);
#endif
    sprite->x += GetTypeIconSlideMovement(useDoubleBattleCoords,position, sprite->x);
    sprite->y = GetTypeIconBounceMovement(sprite->tVerticalPosition,position);
}

static const u32 typeIconTags[] =
{
    TYPE_ICON_TAG,
    TYPE_ICON_TAG_2
};

static void DestroyTypeIcon(struct Sprite* sprite)
{
    u32 spriteId, tag;

#ifdef NATIVE
    DestroySprite(sprite);
    ModsReclaimCustomTypeIcons();
#else
    DestroySpriteAndFreeResources(sprite);
#endif

    for (spriteId = 0; spriteId < MAX_SPRITES; ++spriteId)
    {
        if (!gSprites[spriteId].inUse)
            continue;

        for (tag = 0; tag < 2; tag++)
        {
            if (gSprites[spriteId].template->paletteTag == typeIconTags[tag])
                return;

            if (gSprites[spriteId].template->tileTag == typeIconTags[tag])
                return;
        }
    }

    FreeAllTypeIconResources();
}

static void FreeAllTypeIconResources(void)
{
    u32 tag;

    for (tag = 0; tag < 2; tag++)
    {
        FreeSpriteTilesByTag(typeIconTags[tag]);
        FreeSpritePaletteByTag(typeIconTags[tag]);
    }
}

static void (*const sShowTypesControllerFuncs[])(enum BattlerId battler) =
{
    PlayerHandleChooseMove,
    HandleChooseMoveAfterDma3,
    HandleInputChooseTarget,
    HandleInputShowTargets,
    HandleInputShowEntireFieldTargets,
    HandleMoveSwitching,
    HandleInputChooseMove,
};


static bool32 ShouldHideTypeIcon(enum BattlerId battlerId)
{
    u32 funcIndex;

    for (funcIndex = 0; funcIndex < ARRAY_COUNT(sShowTypesControllerFuncs); funcIndex++)
        if (gBattlerControllerFuncs[battlerId] == sShowTypesControllerFuncs[funcIndex])
            return FALSE;

    return TRUE;
}

static s32 GetTypeIconHideMovement(bool32 useDoubleBattleCoords, u32 position)
{
    if (useDoubleBattleCoords)
    {
        if (position == B_POSITION_PLAYER_LEFT || position == B_POSITION_PLAYER_RIGHT)
            return 1;
        else
            return -1;
    }

    if (position == B_POSITION_PLAYER_LEFT)
        return -1;
    else
        return 1;
}

static s32 GetTypeIconSlideMovement(bool32 useDoubleBattleCoords, u32 position, s32 xPos)
{
    if (useDoubleBattleCoords)
    {
        switch (position)
        {
        case B_POSITION_PLAYER_LEFT:
        case B_POSITION_PLAYER_RIGHT:
            if (xPos > sTypeIconPositions[position][useDoubleBattleCoords].x - 10)
                return -1;
            break;
        default:
        case B_POSITION_OPPONENT_LEFT:
        case B_POSITION_OPPONENT_RIGHT:
            if (xPos < sTypeIconPositions[position][useDoubleBattleCoords].x + 10)
                return 1;
            break;
        }
        return 0;
    }

    if (position == B_POSITION_PLAYER_LEFT)
    {
        if (xPos < sTypeIconPositions[position][useDoubleBattleCoords].x + 10)
            return 1;
    }
    else
    {
        if (xPos > sTypeIconPositions[position][useDoubleBattleCoords].x - 10)
            return -1;
    }
    return 0;
}

static s32 GetTypeIconBounceMovement(s32 originalY, u32 position)
{
    struct Sprite *healthbox = &gSprites[gHealthboxSpriteIds[GetBattlerAtPosition(position)]];
    return originalY + healthbox->y2;
}
