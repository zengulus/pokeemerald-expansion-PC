#include "mod_ui.h"
#include "item_icon.h"
#include "decompress.h"
#include "pokemon_icon.h"
#include "palette.h"
static void AlbumUiTick(u16 held, u16 pressed)
{
    gMain.heldKeysRaw = held; gMain.newKeysRaw = pressed; gMain.newAndRepeatedKeys = pressed;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision);
}
static void CheckUiImage(const char *module)
{
    InitModsForTestWithLibrary(module); RhhModsSetRunContext(123, 456, 0);
    const struct RhhUiService *ui = (const void *)RhhModsHostAPI()->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 2));
    const struct RhhContentDisplayService *content = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/content_display", RHH_SERVICE_VERSION(1, 1));
    CHECK(ui && content && RHH_UI_CANVAS_V1_0_SIZE < RHH_UI_CANVAS_V1_2_SIZE
        && RHH_UI_CANVAS_V1_2_SIZE < RHH_UI_CANVAS_V1_5_SIZE
        && RHH_CONTENT_DISPLAY_SERVICE_V1_0_SIZE < RHH_CONTENT_DISPLAY_SERVICE_V1_1_SIZE
        && RHH_CONTENT_DISPLAY_SERVICE_V1_1_SIZE < RHH_CONTENT_DISPLAY_SERVICE_V1_2_SIZE);
    u8 icon[24 * 24 * 4], untouched[sizeof(icon)]; memset(icon, 0x5a, sizeof(icon)); memcpy(untouched, icon, sizeof(icon));
    RhhContentKey itemKey = RhhContentKeyForId(RHH_ITEM, ITEM_POKE_BALL);
    CHECK(content->item_icon_rgba(0, icon, sizeof(icon)) && !memcmp(icon, untouched, sizeof(icon))
        && content->item_icon_rgba(itemKey, icon, sizeof(icon) - 1) && !memcmp(icon, untouched, sizeof(icon))
        && content->item_icon_rgba(itemKey, NULL, sizeof(icon)));
    const u32 items[] = {ITEM_POKE_BALL, ITEM_TM_FOCUS_PUNCH};
    for (u32 i = 0; i < ARRAY_COUNT(items); ++i) {
        u32 raw[24 * 24 / 8], expanded[32 * 32 / 8] = {0};
        const u32 *source = GetItemIconPic(items[i]); const u16 *palette = GetItemIconPalette(items[i]);
        rng_value_t before = gRngValue, before2 = gRng2Value;
        CHECK(source && palette && GetDecompressedDataSize(source) == sizeof(raw)
            && TryDecompressDataWithHeaderWram(source, raw)
            && !content->item_icon_rgba(RhhContentKeyForId(RHH_ITEM, items[i]), icon, sizeof(icon))
            && !memcmp(&before, &gRngValue, sizeof(before)) && !memcmp(&before2, &gRng2Value, sizeof(before2)));
        CopyItemIconPicTo4x4Buffer(raw, expanded);
        for (u32 y = 0; y < 24; ++y) for (u32 x = 0; x < 24; ++x) {
            const u8 *rawBytes = (const u8 *)expanded;
            u32 offset = ((y / 8) * 4 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
            u32 colorIndex = (rawBytes[offset] >> ((x & 1) * 4)) & 15, dst = (y * 24 + x) * 4;
            for (u32 c = 0; c < 3; ++c) { u32 v = (palette[colorIndex] >> (5 * c)) & 31; CHECK(icon[dst + c] == ((v << 3) | (v >> 2))); }
            CHECK(icon[dst + 3] == (colorIndex ? 255 : 0));
        }
    }
    struct RhhPokemonIconRequest monRequest = {.struct_size = sizeof(monRequest),
        .species = RhhContentKeyForId(RHH_SPECIES, SPECIES_TORCHIC)};
    struct RhhPokemonIcon monInfo = {.struct_size = sizeof(monInfo)}, savedMonInfo = monInfo;
    u8 monPixels[32 * 32 * 4], untouchedMon[sizeof(monPixels)];
    memset(monPixels, 0x5a, sizeof(monPixels)); memcpy(untouchedMon, monPixels, sizeof(monPixels));
    CHECK(content->pokemon_icon_info(NULL, &monInfo) && !memcmp(&monInfo, &savedMonInfo, sizeof(monInfo))
        && content->pokemon_icon_rgba(&monRequest, 2, monPixels, sizeof(monPixels))
        && content->pokemon_icon_rgba(&monRequest, 0, monPixels, sizeof(monPixels) - 1)
        && !memcmp(monPixels, untouchedMon, sizeof(monPixels)));
    monRequest.flags = 128;
    CHECK(content->pokemon_icon_info(&monRequest, &monInfo) && !memcmp(&monInfo, &savedMonInfo, sizeof(monInfo)));
    const struct { u32 species, personality, egg; } cases[] = {
        {SPECIES_TORCHIC, 0, 0}, {SPECIES_PIKACHU, 0, 0}, {SPECIES_PIKACHU, 255, 0},
        {SPECIES_UNOWN, 0, 0}, {SPECIES_UNOWN, 1, 0}, {SPECIES_TORCHIC, 0, 1}, {SPECIES_MANAPHY, 0, 1}
    };
    ResetSpriteData(); FreeAllSpritePalettes(); LoadMonIconPalettes();
    RhhContentKey firstUnown = 0;
    for (u32 i = 0; i < ARRAY_COUNT(cases); ++i) {
        monRequest = (struct RhhPokemonIconRequest){.struct_size = sizeof(monRequest),
            .species = RhhContentKeyForId(RHH_SPECIES, cases[i].species), .personality = cases[i].personality,
            .flags = cases[i].egg ? RHH_POKEMON_ICON_EGG : 0};
        rng_value_t before = gRngValue, before2 = gRng2Value;
        u32 sprite = CreateMonIconIsEgg(cases[i].species, SpriteCB_MonIcon, 0, 0, 0, cases[i].personality, cases[i].egg);
        CHECK(sprite < MAX_SPRITES);
        const u8 *source = (const u8 *)gSprites[sprite].images;
        const u16 *palette = &gPlttBufferUnfaded[OBJ_PLTT_ID(gSprites[sprite].oam.paletteNum)];
        CHECK(!content->pokemon_icon_info(&monRequest, &monInfo) && monInfo.width == 32 && monInfo.height == 32
            && monInfo.species == monRequest.species && monInfo.resolved_species == RhhContentKeyForId(RHH_SPECIES,
                GetIconSpecies(cases[i].species, cases[i].personality))
            && monInfo.frame_count == RhhModsPokemonIconBytes(source) / (32u * 32u / 2u) && monInfo.frame_count >= 2
            && !!(monInfo.flags & RHH_POKEMON_ICON_EGG) == !!cases[i].egg);
#if P_GENDER_DIFFERENCES
        if (cases[i].species == SPECIES_PIKACHU && gSpeciesInfo[SPECIES_PIKACHU].iconSpriteFemale)
            CHECK(!!(monInfo.flags & RHH_POKEMON_ICON_FEMALE) == !!IsPersonalityFemale(SPECIES_PIKACHU, cases[i].personality));
#endif
        if (cases[i].species == SPECIES_UNOWN) {
            if (!firstUnown) firstUnown = monInfo.resolved_species;
            else CHECK(firstUnown != monInfo.resolved_species);
        }
        for (u32 frame = 0; frame < 2; ++frame) {
            CHECK(!content->pokemon_icon_rgba(&monRequest, frame, monPixels, sizeof(monPixels)));
            for (u32 y = 0; y < 32; ++y) for (u32 x = 0; x < 32; ++x) {
                u32 offset = frame * 512 + ((y / 8) * 4 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
                u32 colorIndex = (source[offset] >> ((x & 1) * 4)) & 15, dst = (y * 32 + x) * 4;
                for (u32 c = 0; c < 3; ++c) { u32 v = (palette[colorIndex] >> (5 * c)) & 31; CHECK(monPixels[dst + c] == ((v << 3) | (v >> 2))); }
                CHECK(monPixels[dst + 3] == (colorIndex ? 255 : 0));
            }
        }
        CHECK(!memcmp(&before, &gRngValue, sizeof(before)) && !memcmp(&before2, &gRng2Value, sizeof(before2)));
        FreeAndDestroyMonIconSprite(&gSprites[sprite]);
    }
    FreeMonIconPalettes(); ResetSpriteData();
    AlbumUiTick(0, 0); CHECK(ui->active());
    static u16 frame[DISPLAY_WIDTH * DISPLAY_HEIGHT + 2];
    for (u32 i = 0; i < ARRAY_COUNT(frame); ++i) frame[i] = 0x1234;
    RhhUiRender(frame + 1);
    CHECK(frame[0] == 0x1234 && frame[ARRAY_COUNT(frame) - 1] == 0x1234
        && frame[1] == 0x801f && frame[2] == 0x83e0
        && frame[1 + DISPLAY_WIDTH] == 0xfc00 && frame[2 + DISPLAY_WIDTH] == 0xffff
        && frame[1 + 10 * DISPLAY_WIDTH + 10] == 0x1234
        && frame[1 + 20 * DISPLAY_WIDTH + 20] == 0x1234 && frame[1 + 20 * DISPLAY_WIDTH + 22] == 0x1234
        && frame[1 + 22 * DISPLAY_WIDTH + 20] == 0xfc00 && frame[1 + 22 * DISPLAY_WIDTH + 22] == 0xffff);
    RhhUiAdvance(0); RhhUiAdvance(RHH_UI_B); CHECK(!ui->active());
    RhhModsUnloadLibraries(); puts("ui image: clipped scaling, bounds and callback lifetime passed");
}
