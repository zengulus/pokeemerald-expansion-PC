#include "pokedex.h"
#include "constants/pokedex.h"
#include "constants/maps.h"

extern bool32 DexNavTestOpenGui(void);
extern bool32 DexNavTestSetupGuiStep(void);
extern void DexNavTestSelectGuiSpecies(enum Species species);
extern u8 DexNavTestGuiTypeSprite(u32 index);
extern void DexNavTestGuiSetType(u8 type);
extern u8 DexNavTestGuiTypeMatrix(void);
extern void DexNavTestGuiFrame(void);
extern void DexNavTestCloseGui(void);
extern void DrawFrame(u16 *pixels);
extern u32 gOamMatrixAllocBitmap;
static const struct RhhModType sDexNavCustomType = {
    sizeof(struct RhhModType), 0x86aee0, 0, 0, "test:type/crystal", "Crystal", NULL
};
static void DexNavCustomSpecies(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    const struct RhhTypeService *types = (const void *)RhhModsHostAPI()->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 6));
    if (event->event_key == RhhContentKeyOf("rhh:species/pidgey"))
    {
        struct RhhExtendedTypeList list = {.struct_size = sizeof(list), .count = 5,
            .types = {RhhContentKeyOf("rhh:type/fire"), RhhContentKeyOf("rhh:type/flying"),
                RhhContentKeyOf("rhh:type/grass"), RhhContentKeyOf("rhh:type/water"), RhhContentKeyOf("test:type/crystal")}};
        if (!types || types->offer_species_types_list(&list)) puts("SPECIES_TYPES_BAD");
        return;
    }
    if (event->event_key != RhhContentKeyOf("rhh:species/metapod")) return;
    struct RhhTypeList proposal = {.struct_size = sizeof(proposal), .count = 3,
        .types = {RhhContentKeyOf("rhh:type/fire"), RhhContentKeyOf("rhh:type/flying"), RhhContentKeyOf("test:type/crystal")}};
    if (!types || types->offer_species_types(&proposal)) puts("SPECIES_TYPES_BAD");
}
static const struct RhhModHook sDexNavCustomHook = {
    sizeof(struct RhhModHook), RHH_SPECIES_TYPES, RHH_TRANSFORM, 0, DexNavCustomSpecies, NULL
};
static const struct RhhModDefinition sDexNavCustomMod = {
    .struct_size = sizeof(sDexNavCustomMod), .abi_version = RHH_MOD_ABI_TYPES, .id = "test:zz_dexnav_custom_type",
    .hook_count = 1, .hooks = &sDexNavCustomHook,
    .type_count = 1, .types = &sDexNavCustomType
};

static void CheckDexNavTypeIcons(const char *module, const char *screenshot)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsLoadLibrary(module));
    CHECK(!RhhModsRegister(&sDexNavCustomMod) && !RhhModsFinalize());
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE103)][MAP_NUM(MAP_ROUTE103)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE103);
    gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE103);
    InitGpuRegManager(); ClearDma3Requests(); SetDefaultFontsPointer();
    cgb_audio_init(42060); m4aSoundInit();
    u8 seenBefore[NUM_DEX_FLAG_BYTES]; memcpy(seenBefore, gSaveBlock1Ptr->dexSeen, sizeof(seenBefore));
    CHECK(!GetSetPokedexFlag(NATIONAL_DEX_NONE, FLAG_GET_SEEN));
    CHECK(!GetSetPokedexFlag(NATIONAL_DEX_NONE, FLAG_SET_SEEN));
    CHECK(!GetSetPokedexFlag(NATIONAL_DEX_COUNT + 1, FLAG_SET_SEEN));
    CHECK(!GetSetPokedexFlag((enum NationalDexOrder)-1, FLAG_SET_SEEN));
    CHECK(!memcmp(seenBefore, gSaveBlock1Ptr->dexSeen, sizeof(seenBefore)));
    CHECK(DexNavTestOpenGui());
    bool32 ready = FALSE;
    for (u32 i = 0; i < 200 && !ready; ++i)
    {
        ready = DexNavTestSetupGuiStep();
        ProcessDma3Requests();
    }
    CHECK(ready);
    for (u32 i = 0; i < 40; ++i) { DexNavTestGuiFrame(); ProcessDma3Requests(); }
    for (u32 i = 0; i < 3; ++i) CHECK(DexNavTestGuiTypeSprite(i) < MAX_SPRITES);
    struct Sprite *first = &gSprites[DexNavTestGuiTypeSprite(0)];
    struct Sprite *second = &gSprites[DexNavTestGuiTypeSprite(1)];
    struct Sprite *third = &gSprites[DexNavTestGuiTypeSprite(2)];
    u32 crystalSlot; CHECK(!RhhModsCustomTypeSlot(RhhContentKeyOf("test:type/crystal"), &crystalSlot));
    u16 originalTile = first->sheetTileStart;
    DexNavTestGuiSetType(crystalSlot);
    CHECK(!first->invisible && first->sheetTileStart != originalTile && first->oam.paletteNum < 12
        && gPlttBufferUnfaded[OBJ_PLTT_ID(first->oam.paletteNum) + 2] == RGB(16, 21, 28));
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_TORCHIC), FLAG_SET_SEEN);
    DexNavTestSelectGuiSpecies(SPECIES_TORCHIC);
    CHECK(first->sheetTileStart == originalTile && first->animNum == TYPE_FIRE
        && !first->invisible && second->invisible && third->invisible);
    RhhModsSetRunContext(123, 456, 0);
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_METAPOD), FLAG_SET_SEEN);
    DexNavTestSelectGuiSpecies(SPECIES_METAPOD);
    CHECK(first->animNum == TYPE_FIRE && second->animNum == TYPE_FLYING && !third->invisible
        && third->sheetTileStart != originalTile && third->oam.paletteNum < 12
        && gPlttBufferUnfaded[OBJ_PLTT_ID(third->oam.paletteNum) + 2] == RGB(16, 21, 28));
    DexNavTestSelectGuiSpecies(SPECIES_TORCHIC);
    CHECK(first->animNum == TYPE_FIRE && second->animNum == TYPE_GRASS && third->animNum == TYPE_WATER);
    CHECK(!first->invisible && !second->invisible && !third->invisible);
    CHECK(first->x == 180 && second->x == 204 && third->x == 228 && first->y == 77);
    u8 matrix = DexNavTestGuiTypeMatrix();
    CHECK(matrix < OAM_MATRIX_COUNT && (gOamMatrixAllocBitmap & (1u << matrix)));
    CHECK(first->oam.affineMode == ST_OAM_AFFINE_NORMAL && first->oam.matrixNum == matrix
        && second->oam.matrixNum == matrix && third->oam.matrixNum == matrix);
    CHECK(gOamMatrices[matrix].a == 0x180 && gOamMatrices[matrix].d == 0x100);
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_PIDGEY), FLAG_SET_SEEN);
    DexNavTestSelectGuiSpecies(SPECIES_PIDGEY);
    CHECK(first->animNum == TYPE_FIRE && second->animNum == TYPE_FLYING && third->animNum == TYPE_GRASS);
    for (u32 i = 0; i < 90; ++i) { DexNavTestGuiFrame(); ProcessDma3Requests(); }
    CHECK(third->animNum == TYPE_WATER);
    for (u32 i = 0; i < 90; ++i) { DexNavTestGuiFrame(); ProcessDma3Requests(); }
    CHECK(!third->invisible && third->sheetTileStart != originalTile
        && gPlttBufferUnfaded[OBJ_PLTT_ID(third->oam.paletteNum) + 2] == RGB(16, 21, 28));
    for (u32 i = 0; i < 90; ++i) { DexNavTestGuiFrame(); ProcessDma3Requests(); }
    CHECK(third->sheetTileStart == originalTile && third->animNum == TYPE_GRASS);
    DexNavTestSelectGuiSpecies(SPECIES_TORCHIC);
    for (u32 i = 0; i < 3; ++i) { DexNavTestGuiFrame(); ProcessDma3Requests(); }
    u16 pixels[DISPLAY_WIDTH * DISPLAY_HEIGHT]; DrawFrame(pixels);
    u32 nonblack = 0;
    for (u32 i = 0; i < ARRAY_COUNT(pixels); ++i) if (pixels[i] & 0x7FFF) ++nonblack;
    CHECK(nonblack > 1000);
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels, DISPLAY_WIDTH, DISPLAY_HEIGHT, 16,
        DISPLAY_WIDTH * 2, 31, 31 << 5, 31 << 10, 0);
    CHECK(surface && !SDL_SaveBMP(surface, screenshot)); SDL_FreeSurface(surface);
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_GENGAR), FLAG_SET_SEEN);
    DexNavTestSelectGuiSpecies(SPECIES_GENGAR); /* Denied override keeps the original two types. */
    CHECK(first->animNum == TYPE_GHOST && second->animNum == TYPE_POISON && third->invisible
        && first->oam.affineMode == ST_OAM_AFFINE_OFF && second->oam.affineMode == ST_OAM_AFFINE_OFF);
    DexNavTestSelectGuiSpecies(SPECIES_TORCHIC);
    CHECK(!third->invisible && third->animNum == TYPE_WATER && DexNavTestGuiTypeMatrix() == matrix);
    DexNavTestSelectGuiSpecies(SPECIES_PIKACHU); /* Unseen species stay hidden. */
    CHECK(first->animNum == TYPE_MYSTERY && second->invisible && third->invisible);
    DexNavTestGuiSetType(crystalSlot);
    u16 customTag = GetSpriteTileTagByTileStart(first->sheetTileStart);
    CHECK(customTag != TAG_NONE);
    DexNavTestCloseGui(); CHECK(!(gOamMatrixAllocBitmap & (1u << matrix)));
    CHECK(GetSpriteTileStartByTag(customTag) == TAG_NONE && IndexOfSpritePaletteTag(customTag) == 0xff);
    CHECK(DexNavTestOpenGui());
    ready = FALSE;
    for (u32 i = 0; i < 200 && !ready; ++i)
    {
        ready = DexNavTestSetupGuiStep();
        ProcessDma3Requests();
    }
    CHECK(ready);
    gOamMatrixAllocBitmap = 0xFFFFFFFFu;
    DexNavTestSelectGuiSpecies(SPECIES_TORCHIC);
    second = &gSprites[DexNavTestGuiTypeSprite(1)];
    third = &gSprites[DexNavTestGuiTypeSprite(2)];
    CHECK(DexNavTestGuiTypeMatrix() == 0xFF && second->animNum == TYPE_GRASS && third->invisible);
    for (u32 i = 0; i < 90; ++i) { DexNavTestGuiFrame(); ProcessDma3Requests(); }
    CHECK(second->animNum == TYPE_WATER);
    for (u32 i = 0; i < 90; ++i) { DexNavTestGuiFrame(); ProcessDma3Requests(); }
    CHECK(second->animNum == TYPE_GRASS);
    DexNavTestSelectGuiSpecies(SPECIES_PIDGEY);
    CHECK(second->animNum == TYPE_FLYING && third->invisible);
    for (u32 i = 0; i < 90; ++i) { DexNavTestGuiFrame(); ProcessDma3Requests(); }
    CHECK(second->animNum == TYPE_GRASS);
    for (u32 i = 0; i < 90; ++i) { DexNavTestGuiFrame(); ProcessDma3Requests(); }
    CHECK(second->animNum == TYPE_WATER);
    for (u32 i = 0; i < 90; ++i) { DexNavTestGuiFrame(); ProcessDma3Requests(); }
    CHECK(!second->invisible && second->sheetTileStart != originalTile);
    DexNavTestSelectGuiSpecies(SPECIES_PIKACHU);
    for (u32 i = 0; i < 90; ++i) { DexNavTestGuiFrame(); ProcessDma3Requests(); }
    CHECK(second->invisible);
    DexNavTestCloseGui(); gOamMatrixAllocBitmap = 0;
    RhhModsUnloadLibraries();
    puts("dexnav types: GUI, transitions, overflow and matrix cleanup passed");
}
