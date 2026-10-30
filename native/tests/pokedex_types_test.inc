#include "pokedex_common.h"
#include "pokedex_plus_hgss.h"

extern void RhhTestPokedexShowSpeciesTypes(enum Species species);
extern void RhhTestPokedexShowTypeIcon(u8 type);
extern struct PokedexView *sPokedexView;
static const struct RhhModType sPokedexCustomType = {
    sizeof(struct RhhModType), 0x86aee0, 0, 0, "test:type/crystal", "Crystal", NULL
};
static void PokedexCustomSpecies(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
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
static const struct RhhModHook sPokedexCustomHook = {
    sizeof(struct RhhModHook), RHH_SPECIES_TYPES, RHH_TRANSFORM, 0, PokedexCustomSpecies, NULL
};
static const struct RhhModDefinition sPokedexCustomMod = {
    .struct_size = sizeof(sPokedexCustomMod), .abi_version = RHH_MOD_ABI_TYPES, .id = "test:zz_pokedex_custom_type",
    .hook_count = 1, .hooks = &sPokedexCustomHook,
    .type_count = 1, .types = &sPokedexCustomType
};

static void CheckPokedexTypeIcons(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsLoadLibrary(module));
    CHECK(!RhhModsRegister(&sPokedexCustomMod) && !RhhModsFinalize());
    struct PokedexView view = {0}; sPokedexView = &view;
    ResetSpriteData(); FreeAllSpritePalettes();
    gMain.state = 3;
    CHECK(Task_TryLoadInfoScreen_HGSS(0) && gMain.state == 4);
    for (u32 i = 0; i < 3; ++i) CHECK(view.typeIconSpriteIds[i] < MAX_SPRITES);
    struct Sprite *first = &gSprites[view.typeIconSpriteIds[0]];
    struct Sprite *second = &gSprites[view.typeIconSpriteIds[1]];
    struct Sprite *third = &gSprites[view.typeIconSpriteIds[2]];
    u32 crystalSlot; CHECK(!RhhModsCustomTypeSlot(RhhContentKeyOf("test:type/crystal"), &crystalSlot));
    u16 originalTile = first->sheetTileStart;
    RhhTestPokedexShowTypeIcon(crystalSlot);
    CHECK(!first->invisible && first->sheetTileStart != originalTile && first->oam.paletteNum < 12
        && gPlttBufferUnfaded[OBJ_PLTT_ID(first->oam.paletteNum) + 2] == RGB(16, 21, 28));
    RhhTestPokedexShowSpeciesTypes(SPECIES_TORCHIC);
    CHECK(first->sheetTileStart == originalTile && first->animNum == TYPE_FIRE);
    RhhTestPokedexShowSpeciesTypes(SPECIES_TORCHIC);
    CHECK(!first->invisible && first->animNum == TYPE_FIRE && second->invisible && third->invisible);
    RhhModsSetRunContext(123, 456, 0);
    RhhTestPokedexShowSpeciesTypes(SPECIES_METAPOD);
    CHECK(first->animNum == TYPE_FIRE && second->animNum == TYPE_FLYING && !third->invisible
        && third->sheetTileStart != originalTile && third->oam.paletteNum < 12
        && gPlttBufferUnfaded[OBJ_PLTT_ID(third->oam.paletteNum) + 2] == RGB(16, 21, 28));
    RhhTestPokedexShowSpeciesTypes(SPECIES_PIDGEY);
    CHECK(first->animNum == TYPE_FIRE && second->animNum == TYPE_FLYING && third->animNum == TYPE_GRASS);
    for (u32 i = 0; i < 90; ++i) third->callback(third);
    CHECK(third->animNum == TYPE_WATER);
    for (u32 i = 0; i < 90; ++i) third->callback(third);
    CHECK(!third->invisible && third->sheetTileStart != originalTile
        && gPlttBufferUnfaded[OBJ_PLTT_ID(third->oam.paletteNum) + 2] == RGB(16, 21, 28));
    for (u32 i = 0; i < 90; ++i) third->callback(third);
    CHECK(third->sheetTileStart == originalTile && third->animNum == TYPE_GRASS);
    HandleCaughtMonPageTypeIcons_HGSS();
    for (u32 i = 0; i < 90; ++i) third->callback(third);
    CHECK(third->invisible);
    RhhTestPokedexShowSpeciesTypes(SPECIES_TORCHIC);
    CHECK(!first->invisible && first->animNum == TYPE_FIRE && first->x == 151 && first->y == 56);
    CHECK(!second->invisible && second->animNum == TYPE_GRASS && second->x == 184 && second->y == 56);
    CHECK(!third->invisible && third->animNum == TYPE_WATER && third->x == 217 && third->y == 56);
    RhhTestPokedexShowSpeciesTypes(SPECIES_GENGAR); /* Denied override retains original two types. */
    CHECK(first->animNum == TYPE_GHOST && second->animNum == TYPE_POISON && !second->invisible && third->invisible);
    RhhTestPokedexShowSpeciesTypes(SPECIES_PIKACHU);
    CHECK(first->animNum == TYPE_ELECTRIC && second->invisible && third->invisible);
    RhhTestPokedexShowSpeciesTypes(SPECIES_CHARIZARD);
    CHECK(!third->invisible && third->animNum == TYPE_GRASS);
    HandleCaughtMonPageTypeIcons_HGSS();
    CHECK(first->invisible && second->invisible && third->invisible);

    /* A full sprite pool must not index beyond gSprites. */
    ResetSpriteData(); FreeAllSpritePalettes();
    for (u32 i = 0; i < MAX_SPRITES - 2; ++i) gSprites[i].inUse = TRUE;
    gMain.state = 3;
    CHECK(Task_TryLoadInfoScreen_HGSS(0) && gMain.state == 4);
    CHECK(view.typeIconSpriteIds[2] >= MAX_SPRITES);
    RhhTestPokedexShowSpeciesTypes(SPECIES_TORCHIC);
    HandleCaughtMonPageTypeIcons_HGSS();
    ResetSpriteData(); FreeAllSpritePalettes(); sPokedexView = NULL; RhhModsUnloadLibraries();
    puts("pokedex types: icons, transitions and sprite exhaustion passed");
}
