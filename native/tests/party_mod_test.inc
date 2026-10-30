#include "party_menu.h"
#include "item_menu.h"
#include "mod_ui.h"
#include "field_move.h"
#include "battle_bg.h"
#include "constants/field_move.h"

static void PartyTestFrame(u16 keys)
{
    REG_KEYINPUT = keys ^ KEYS_MASK;
    AgbMainLoop();
    NativeRunDma(1); ProcessDma3Requests();
    if (gMain.vblankCallback) gMain.vblankCallback();
}
static void PartyTestPress(u16 keys) { PartyTestFrame(0); PartyTestFrame(keys); PartyTestFrame(0); }
static void PartyUiPress(u16 keys) { RhhUiAdvance(0); RhhUiAdvance(keys); }
static void PartyExperienceReturn(void) {}
static void CheckPartyItemReduction(const char *module)
{
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); cgb_audio_init(42060); AgbMain();
    ClearSav1(); ClearSav2(); ClearSav3(); SetSaveBlocksPointers(0);
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0); ClearBag();
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][0];
    CreateMonWithIVs(mon, SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1; u8 value = 25;
    SetMonData(mon, MON_DATA_HP_EV, &value); CalculateMonStats(mon);
    CHECK(AddBagItem(ITEM_POMEG_BERRY, 2)); CB2_PartyMenuFromStartMenu();
    unsigned frames = 0;
    while ((!FuncIsActiveTask(Task_HandleChooseMonInput) || gPaletteFade.active) && frames++ < 500) PartyTestFrame(0);
    CHECK(frames < 500 && gPartyMenu.slotId == 0 && gPartyMenu.menuType == PARTY_MENU_TYPE_FIELD);
    gSpecialVar_ItemId = ITEM_POMEG_BERRY; u8 task = FindTaskIdByFunc(Task_HandleChooseMonInput);
    CHECK(task != TASK_NONE); ItemUseCB_Medicine(task, Task_HandleChooseMonInput);
    struct ItemReductionState *state = RhhModsState("test:item_reduction");
    CHECK(state && state->processed == 1 && state->removed == 10 && GetMonData(mon, MON_DATA_HP_EV) == 15
        && CheckBagHasItem(ITEM_POMEG_BERRY, 1) && !CheckBagHasItem(ITEM_POMEG_BERRY, 2));
    state->deny = 1; struct Pokemon original = *mon;
    ItemUseCB_ReduceEV(task, Task_HandleChooseMonInput);
    CHECK(!memcmp(mon, &original, sizeof(original)) && CheckBagHasItem(ITEM_POMEG_BERRY, 1) && state->processed == 1);
    state->deny = 0; ItemUseCB_ReduceEV(task, Task_HandleChooseMonInput);
    CHECK(GetMonData(mon, MON_DATA_HP_EV) == 5 && !CheckBagHasItem(ITEM_POMEG_BERRY, 1)
        && state->processed == 2 && state->removed == 20);
    value = 0; SetMonData(mon, MON_DATA_HP_EV, &value); value = 100; SetMonData(mon, MON_DATA_FRIENDSHIP, &value);
    CHECK(AddBagItem(ITEM_POMEG_BERRY, 2)); ItemUseCB_ReduceEV(task, Task_HandleChooseMonInput);
    CHECK(GetMonData(mon, MON_DATA_FRIENDSHIP) > 100 && state->processed == 3 && state->removed == 20
        && CheckBagHasItem(ITEM_POMEG_BERRY, 1) && !CheckBagHasItem(ITEM_POMEG_BERRY, 2));
    value = MAX_FRIENDSHIP; SetMonData(mon, MON_DATA_FRIENDSHIP, &value); original = *mon;
    ItemUseCB_ReduceEV(task, Task_HandleChooseMonInput);
    CHECK(!memcmp(mon, &original, sizeof(original)) && state->processed == 3 && CheckBagHasItem(ITEM_POMEG_BERRY, 1));
    value = 7; SetMonData(mon, MON_DATA_HP_EV, &value); value = 8; SetMonData(mon, MON_DATA_ATK_EV, &value);
    CHECK(AddBagItem(ITEM_FRESH_START_MOCHI, 2)); gSpecialVar_ItemId = ITEM_FRESH_START_MOCHI;
    state->deny = 1; original = *mon; ItemUseCB_ResetEVs(task, Task_HandleChooseMonInput);
    CHECK(!memcmp(mon, &original, sizeof(original)) && CheckBagHasItem(ITEM_FRESH_START_MOCHI, 2) && state->processed == 3);
    state->deny = 0; ItemUseCB_ResetEVs(task, Task_HandleChooseMonInput);
    CHECK(!GetMonData(mon, MON_DATA_HP_EV) && !GetMonData(mon, MON_DATA_ATK_EV) && state->processed == 4
        && state->removed == 35 && CheckBagHasItem(ITEM_FRESH_START_MOCHI, 1) && !CheckBagHasItem(ITEM_FRESH_START_MOCHI, 2));
    RhhModsUnloadLibraries(); RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsFinalize()); RhhModsSetRunContext(123, 456, 0);
    value = 10; SetMonData(mon, MON_DATA_HP_EV, &value); value = 50; SetMonData(mon, MON_DATA_FRIENDSHIP, &value);
    gSpecialVar_ItemId = ITEM_POMEG_BERRY; ItemUseCB_ReduceEV(task, Task_HandleChooseMonInput);
    CHECK(!GetMonData(mon, MON_DATA_HP_EV) && !CheckBagHasItem(ITEM_POMEG_BERRY, 1));
    RhhModsUnloadLibraries(); SDL_Quit(); puts("item EV reduction: real party menu consumes one item");
}
static void CheckPartyExperienceLevels(bool32 fullMoves)
{
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); cgb_audio_init(42060); AgbMain();
    ClearSav1(); ClearSav2(); ClearSav3(); SetSaveBlocksPointers(0);
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsFinalize()); RhhModsSetRunContext(123, 456, 0);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    const u16 oldMoves[MAX_MON_MOVES] = {MOVE_SCRATCH, MOVE_GROWL, MOVE_TACKLE, MOVE_WATER_GUN};
    for (u32 i = 0; i < MAX_MON_MOVES; ++i)
        SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], fullMoves ? oldMoves[i] : MOVE_NONE, i);
    CHECK(AddBagItem(ITEM_EXP_CANDY_S, 1));
    CB2_PartyMenuFromStartMenu();
    unsigned frames = 0;
    while ((!FuncIsActiveTask(Task_HandleChooseMonInput) || gPaletteFade.active) && frames++ < 500) PartyTestFrame(0);
    CHECK(frames < 500 && gPartyMenu.slotId == 0);
    gPartyMenu.exitCallback = PartyExperienceReturn;
    gSpecialVar_ItemId = ITEM_EXP_CANDY_S;
    u8 task = FindTaskIdByFunc(Task_HandleChooseMonInput);
    CHECK(task != TASK_NONE);
    ItemUseCB_RareCandy(task, Task_HandleChooseMonInput);
    u32 level = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL);
    CHECK(level > 6 && level < 16 && !CheckBagHasItem(ITEM_EXP_CANDY_S, 1));
    const struct RhhOwnedMonService *owned = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 28));
    CHECK(owned);
    frames = 0;
    while (gMain.callback2 != PartyExperienceReturn && frames++ < 8000)
    {
        PartyTestFrame(frames % 12 == 0 ? A_BUTTON : 0);
        CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL) == level);
        if (!(frames % 12))
        {
            struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
            struct RhhOwnedExperience exp = {.struct_size = sizeof(exp)};
            struct RhhExperiencePreview preview = {.struct_size = sizeof(preview)};
            CHECK(!owned->party_at(0, &mon) && !owned->experience(&mon, &exp)
                && !owned->preview_experience(&exp, 0, &preview)
                && mon.level == level && preview.before_level == level && preview.after_level == level);
        }
    }
    CHECK(frames < 8000);
    u32 expected = 0;
    u32 lastExpected = MOVE_NONE;
    const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(SPECIES_TORCHIC);
    for (u32 i = 0; learnset[i].move != LEVEL_UP_MOVE_END; ++i)
        if (learnset[i].level > 5 && learnset[i].level <= level)
        {
            CHECK(expected < MAX_MON_MOVES);
            if (!fullMoves)
                CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + expected) == learnset[i].move);
            lastExpected = learnset[i].move;
            ++expected;
        }
    CHECK(expected >= 2);
    if (fullMoves)
    {
        bool32 learned = FALSE;
        for (u32 i = 0; i < MAX_MON_MOVES; ++i)
        {
            u32 move = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + i);
            CHECK(move != MOVE_NONE);
            if (move == lastExpected) learned = TRUE;
        }
        CHECK(learned);
    }
    SDL_Quit(); puts("party experience: level and learned moves stay consistent throughout the real menu flow");
}
static void ImpureBattlePass(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->ordinal == 9) { (void)Random(); (void)Random2(); }
}
static void CheckBattlePasses(const char *a, const char *b, bool32 gl)
{
    static const struct RhhModHook impureHooks[] = {
        {sizeof(struct RhhModHook), RHH_BATTLE_RENDER_PASSES, RHH_CONSTRAINT, 0, ImpureBattlePass, NULL}
    };
    static const struct RhhModDefinition impure = {.struct_size = sizeof(impure), .abi_version = RHH_MOD_ABI_SERVICES,
        .id = "test:battle_pass_rng", .hooks = impureHooks, .hook_count = 1};
    RhhModsInit(); CHECK(!ModsRegisterBuiltins());
    struct RhhModHook invalidHook = {sizeof(invalidHook), RHH_BATTLE_RENDER_PASSES, RHH_TRANSFORM, 0, ImpureBattlePass, NULL};
    struct RhhModDefinition invalid = {.struct_size = sizeof(invalid), .abi_version = RHH_MOD_ABI_SERVICES,
        .id = "test:invalid_battle_pass", .hooks = &invalidHook, .hook_count = 1};
    CHECK(RhhModsRegister(&invalid));
    CHECK(!RhhModsRegister(&impure)
        && !RhhModsLoadLibrary(b) && !RhhModsLoadLibrary(a) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhBattleRenderService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/battle_render", RHH_SERVICE_VERSION(1, 0));
    CHECK(service && service->header.struct_size >= RHH_BATTLE_RENDER_SERVICE_V1_0_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/battle_render", RHH_SERVICE_VERSION(1, 1)));
    struct RhhBattleRenderPass passes[RHH_BATTLE_RENDER_PASS_CAPACITY], outside = {.struct_size = sizeof(outside),
        .id = RhhContentKeyOf("test:battle_pass/a"), .shader = RhhContentKeyOf("test:shader/battle_a")};
    CHECK(service->offer(&outside));
    struct NativeBattleEnvironment env = {.battle_scene = 1, .weather = RHH_WEATHER_FOG,
        .terrain = RHH_TERRAIN_ELECTRIC, .suppressed = 1, .doubles = 1, .visibility = .5f};
    CHECK(ModsCollectBattleRenderPasses(&env, 3, passes) == 2
        && passes[0].id == RhhContentKeyOf("test:battle_pass/a") && passes[1].id == RhhContentKeyOf("test:battle_pass/b"));
    CHECK(!ModsCollectBattleRenderPasses(&env, 4, passes) && !ModsCollectBattleRenderPasses(&env, 5, passes));
    CHECK(ModsCollectBattleRenderPasses(&env, 6, passes) == 1 && passes[0].id == RhhContentKeyOf("test:battle_pass/b"));
    CHECK(ModsCollectBattleRenderPasses(&env, 7, passes) == 2 && passes[0].priority == passes[1].priority
        && passes[0].id < passes[1].id && !ModsCollectBattleRenderPasses(&env, 8, passes));
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(!ModsCollectBattleRenderPasses(&env, 9, passes) && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    struct NativeBattleEnvironment clear = {.battle_scene = 1, .visibility = 1.f};
    CHECK(ModsCollectBattleRenderPasses(&clear, 10, passes) == 2);
    RhhModsSetRunContext(0, 0, 0); CHECK(!ModsCollectBattleRenderPasses(&env, 3, passes));
    RhhModsSetRunContext(123, 456, 0);
    env.battle_scene = 0; CHECK(!ModsCollectBattleRenderPasses(&env, 3, passes)); env.battle_scene = 1;
    const struct RhhBattleBackdropService *backdrop = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/battle_backdrop", RHH_SERVICE_VERSION(1, 1));
    struct RhhBattleBackdropInfo info = {.struct_size = sizeof(info)}, original = info;
    CHECK(backdrop && backdrop->header.struct_size >= RHH_BATTLE_BACKDROP_SERVICE_V1_1_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/battle_backdrop", RHH_SERVICE_VERSION(1, 2))
        && backdrop->current(&info) && !memcmp(&info, &original, sizeof(info)));
    bool32 sawGrass = FALSE, sawUnavailable = FALSE;
    for (u32 i = 0; i < backdrop->count(); ++i)
    {
        CHECK(!backdrop->at(i, &info) && info.key == RhhContentKeyOf(info.id));
        if (info.key == RhhContentKeyOf("rhh:battle_backdrop/grass")) sawGrass = info.has_artwork;
        if (!info.has_artwork) sawUnavailable = TRUE;
    }
    original = info;
    struct RhhBattleBackdropInfo tinyBackdrop = {.struct_size = 1, .key = 55}, tinyOriginal = tinyBackdrop;
    CHECK(sawGrass && sawUnavailable && backdrop->at(backdrop->count(), &info)
        && !memcmp(&info, &original, sizeof(info)) && backdrop->at(0, &tinyBackdrop)
        && !memcmp(&tinyBackdrop, &tinyOriginal, sizeof(tinyBackdrop)));
    static u8 art[240 * 160 * 4];
    memset(art, 0x5a, sizeof(art));
    RhhContentKey grass = RhhContentKeyOf("rhh:battle_backdrop/grass");
    CHECK(backdrop->image_rgba(grass, 0, 0, 240, 160, art, sizeof(art) - 1)
        && art[0] == 0x5a && art[sizeof(art) - 1] == 0x5a
        && backdrop->image_rgba(grass, 510, 0, 3, 1, art, sizeof(art))
        && backdrop->image_rgba(RhhContentKeyOf("test:backdrop/missing"), 0, 0, 1, 1, art, sizeof(art))
        && art[0] == 0x5a && art[sizeof(art) - 1] == 0x5a);
    CHECK(!backdrop->image_rgba(grass, 0, 0, 240, 160, art, sizeof(art)));
    u8 crop[13 * 9 * 4], seamCrop[16 * 8 * 4], seamImage[512 * 8 * 4];
    CHECK(!backdrop->image_rgba(grass, 37, 27, 13, 9, crop, sizeof(crop)));
    for (u32 row = 0; row < 9; ++row)
        CHECK(!memcmp(crop + row * 13 * 4, art + ((row + 27) * 240 + 37) * 4, 13 * 4));
    CHECK(!backdrop->image_rgba(grass, 0, 0, 512, 8, seamImage, sizeof(seamImage))
        && !backdrop->image_rgba(grass, 252, 0, 16, 8, seamCrop, sizeof(seamCrop)));
    for (u32 row = 0; row < 8; ++row)
        CHECK(!memcmp(seamCrop + row * 16 * 4, seamImage + (row * 512 + 252) * 4, 16 * 4));
    SetSaveBlocksPointers(0); gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE101);
    gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE101);
    gBattleTypeFlags = 0; gBattleEnvironment = BATTLE_ENVIRONMENT_GRASS;
    gMain.inBattle = TRUE; gMain.callback2 = BattleMainCB2; gBattleStruct = &sBattle;
    ModsBattleBackdropReset();
    /* Compare every available catalog image with the real BG3 renderer. */
    static u16 backdropFrame[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    REG_DISPCNT = DISPCNT_BG3_ON; REG_BG3CNT = 3 | (2 << 2) | (26 << 8) | (1 << 14);
    REG_BG3HOFS = REG_BG3VOFS = 0; REG_BLDCNT = 0; REG_IME = 0;
    u32 imagesCompared = 0;
    for (u32 index = 0; index < backdrop->count(); ++index)
    {
        CHECK(!backdrop->at(index, &info));
        if (!info.has_artwork) continue;
        CHECK(!backdrop->image_rgba(info.key, 0, 0, 240, 160, art, sizeof(art)));
        gBattleEnvironment = index; DrawMainBattleBackground(); TransferPlttBuffer(); DrawFrame(backdropFrame);
        u32 compared = 0;
        for (u32 i = 0; i < ARRAY_COUNT(backdropFrame); ++i)
            if (art[4 * i + 3])
            {
                u16 pixel = backdropFrame[i];
                for (u32 c = 0; c < 3; ++c)
                { u8 value = (pixel >> (5 * c)) & 31; CHECK(art[4 * i + c] == ((value << 3) | (value >> 2))); }
                ++compared;
            }
        CHECK(compared > 1000); ++imagesCompared;
    }
    CHECK(imagesCompared >= 20);
    gBattleEnvironment = BATTLE_ENVIRONMENT_GRASS; DrawMainBattleBackground();
    rng = gRngValue; rng2 = gRng2Value;
    CHECK(!backdrop->current(&info) && info.key == RhhContentKeyOf("rhh:battle_backdrop/grass")
        && ModsCollectBattleRenderPasses(&env, 13, passes) == 2
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    /* Incremental loading is unavailable until all three parts agree. */
    CHECK(!LoadChosenBattleElement(3) && backdrop->current(&info));
    gBattleTypeFlags = BATTLE_TYPE_FRONTIER;
    CHECK(!LoadChosenBattleElement(4) && !LoadChosenBattleElement(5) && backdrop->current(&info));
    CHECK(!LoadChosenBattleElement(3) && !LoadChosenBattleElement(4) && backdrop->current(&info)
        && !LoadChosenBattleElement(5) && !backdrop->current(&info)
        && info.key == RhhContentKeyOf("rhh:battle_backdrop/frontier")
        && ModsCollectBattleRenderPasses(&env, 14, passes) == 2);
    gMain.inBattle = FALSE; CHECK(backdrop->current(&info)); ModsBattleBackdropReset(); gBattleTypeFlags = 0;
    if (gl) {
        CHECK(!SDL_Init(SDL_INIT_VIDEO)); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_Window *window = SDL_CreateWindow("Battle pass test", 0, 0, 480, 320, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        CHECK(window && !NativeShaderInit(window));
        static u16 pixels[DISPLAY_WIDTH * DISPLAY_HEIGHT], overlay[DISPLAY_WIDTH * DISPLAY_HEIGHT];
        static u8 captured[RHH_PHOTO_RGB_BYTES], preview[RHH_PHOTO_RGB_BYTES];
        for (u32 i = 0; i < ARRAY_COUNT(pixels); ++i) pixels[i] = RGB(0, 0, 31);
        pixels[0] = RGB(0, 31, 0); memset(gNativeShaderSceneMask, 255, sizeof(gNativeShaderSceneMask));
        gNativeShaderSceneMask[0] = gNativeShaderSceneMask[ARRAY_COUNT(pixels) - 1] = 0;
        NativeShaderPresent(pixels, &env, 3); CHECK(!NativeShaderCapture(captured));
        CHECK(!captured[0] && captured[1] == 255 && !captured[2]
            && !captured[3] && captured[4] >= 63 && captured[4] <= 65 && !captured[5]
            && !captured[RHH_PHOTO_RGB_BYTES - 3] && !captured[RHH_PHOTO_RGB_BYTES - 2]
            && captured[RHH_PHOTO_RGB_BYTES - 1] == 255);
        overlay[1] = RGB(31, 0, 0) | 0x8000;
        NativeShaderPresentBattleOverlay(pixels, &env, 3, overlay);
        CHECK(!NativeShaderCapturePreview(preview) && preview[3] == 255 && !preview[4] && !preview[5]);
        env.shader = RhhContentKeyOf("test:shader/battle_a");
        NativeShaderPresent(pixels, &env, 4); CHECK(!NativeShaderCapture(captured));
        CHECK(!captured[0] && captured[1] == 255 && !captured[2]
            && captured[3] >= 63 && captured[3] <= 65 && !captured[4] && !captured[5]);
        NativeShaderPresent(pixels, &env, 6); CHECK(!NativeShaderCapture(captured));
        CHECK(!captured[3] && captured[4] >= 63 && captured[4] <= 65 && !captured[5]);
        env.shader = 0; env.battle_scene = 0;
        NativeShaderPresent(pixels, &env, 3); CHECK(!NativeShaderCapture(captured));
        CHECK(!captured[3] && !captured[4] && captured[5] == 255);
        /* Two real OAM sprites overlap. The mask must follow the topmost
         * rendered pixels, rather than a bounding box or sprite position. */
        ResetSpriteData(); FreeAllSpritePalettes();
        for (u32 i = 0; i < 128; ++i) gMain.oamBuffer[i] = gDummyOamData;
        memset(gNativeVram + 0x10000, 0x11, 32);
        ((u16 *)gNativePalette)[0] = RGB(0, 0, 31);
        ((u16 *)gNativePalette)[0x101] = RGB(31, 31, 31);
        REG_DISPCNT = DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP; REG_BLDCNT = 0; REG_IME = 0;
        gMain.inBattle = TRUE; gMain.callback2 = BattleMainCB2; gBattleStruct = &sBattle; gBattlersCount = 2;
        u8 uiPalette = AllocSpritePalette(TAG_HEALTHBOX_PAL); CHECK(uiPalette < 16);
        ModsBattleShaderUpdateUiPalettes(); CHECK(gNativeShaderUiPalettes & (1u << uiPalette));
        gNativeShaderUiPalettes = 0;
        gBattlerSpriteIds[0] = 1; gBattlerSpriteIds[1] = 2;
        gSprites[1].inUse = TRUE; gSprites[1].x = 16; gSprites[1].y = 16; gSprites[1].oam.priority = 1;
        gSprites[2].inUse = TRUE; gSprites[2].x = 20; gSprites[2].y = 16; gSprites[2].oam.priority = 0;
        BuildOamBuffer(); LoadOam(); DrawFrame(pixels);
        const u8 *mask = NativeBattleBattlerMask(pixels);
        CHECK(mask && mask[16 * DISPLAY_WIDTH + 16] == 1 && mask[16 * DISPLAY_WIDTH + 19] == 1
            && mask[16 * DISPLAY_WIDTH + 20] == 2 && mask[16 * DISPLAY_WIDTH + 27] == 2
            && !mask[16 * DISPLAY_WIDTH + 28] && !NativeBattleBattlerMask(overlay));
        env.battle_scene = 1; NativeShaderPresent(pixels, &env, 11); CHECK(!NativeShaderCapture(captured));
        u32 left = (16 * DISPLAY_WIDTH + 16) * 3, overlap = (16 * DISPLAY_WIDTH + 20) * 3;
        CHECK(captured[left] == 255 && !captured[left + 1] && !captured[left + 2]
            && !captured[overlap] && captured[overlap + 1] == 255 && !captured[overlap + 2]);
        env.shader = RhhContentKeyOf("test:shader/battle_b");
        NativeShaderPresent(pixels, &env, 12); CHECK(!NativeShaderCapture(captured));
        CHECK(captured[left] == 255 && !captured[left + 1] && !captured[left + 2]
            && !captured[overlap] && captured[overlap + 1] == 255 && !captured[overlap + 2]);
        env.shader = 0;
        gNativeShaderUiPalettes = 1; DrawFrame(pixels); mask = NativeBattleBattlerMask(pixels);
        CHECK(mask && !mask[16 * DISPLAY_WIDTH + 16] && !mask[16 * DISPLAY_WIDTH + 20]);
        gNativeShaderUiPalettes = 0;
        REG_DISPCNT |= DISPCNT_WIN0_ON; REG_WIN0H = (16 << 8) | 20; REG_WIN0V = (16 << 8) | 24;
        REG_WININ = 0; REG_WINOUT = WINOUT_WIN01_OBJ;
        DrawFrame(pixels); mask = NativeBattleBattlerMask(pixels);
        CHECK(mask && !mask[16 * DISPLAY_WIDTH + 16] && mask[16 * DISPLAY_WIDTH + 20] == 2);
        REG_DISPCNT &= ~DISPCNT_WIN0_ON;
        for (u32 i = 0; i < 128; ++i)
            if (((struct OamData *)OAM)[i].priority == 1 && ((struct OamData *)OAM)[i].x == 16)
                ((struct OamData *)OAM)[i].x = 17;
        DrawFrame(pixels); mask = NativeBattleBattlerMask(pixels);
        CHECK(mask && !mask[16 * DISPLAY_WIDTH + 17] && mask[16 * DISPLAY_WIDTH + 20] == 2);
        gBattlerSpriteIds[1] = 1; BuildOamBuffer(); LoadOam(); DrawFrame(pixels);
        mask = NativeBattleBattlerMask(pixels);
        CHECK(mask && !mask[16 * DISPLAY_WIDTH + 16] && !mask[16 * DISPLAY_WIDTH + 20]);
        gBattlerSpriteIds[1] = 2; gSprites[2].inUse = FALSE; gSprites[1].y = 110;
        BuildOamBuffer(); LoadOam(); DrawFrame(pixels); mask = NativeBattleBattlerMask(pixels);
        CHECK(mask && mask[110 * DISPLAY_WIDTH + 16] == 1 && !mask[112 * DISPLAY_WIDTH + 16]);
        gMain.inBattle = FALSE; DrawFrame(pixels); CHECK(!NativeBattleBattlerMask(pixels));
        NativeShaderPresent(pixels, &env, 11); CHECK(!NativeShaderCapture(captured));
        CHECK(captured[left] == 255 && !captured[left + 1] && captured[left + 2] == 255);
        NativeShaderShutdown(); SDL_DestroyWindow(window); SDL_Quit();
        puts("battle passes: GL ordering, uniforms, battler mask and protected UI passed");
    }
    RhhModsUnloadLibraries(); puts("battle passes: ordering, conflicts and purity passed");
}
