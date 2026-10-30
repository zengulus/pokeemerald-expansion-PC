/* Live battle adapter, production compositor and actual compiled GLSL. Run
 * with xvfb-run, or SDL_VIDEODRIVER=dummy to exercise the no-GL fallback. */
#include "battle_shader.h"
#include "battle_main.h"
#include "battle_scripts.h"
#include "battle_interface.h"
#include "main.h"
#include "constants/rgb.h"
void DrawFrame(uint16_t *pixels);

static int sBrokenEnvironment;
static int sWorldSelected, sWorldDeclines, sWorldMaskMode;
static unsigned sWorldMaskTexture;
static void BrokenEnvironment(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{
    (void)event; (void)state;
    if (sBrokenEnvironment) out->value = RhhContentKeyOf("test:shader/broken");
}
static void SelectWorld(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{
    (void)event; (void)state;
    if (sWorldSelected) out->value = RhhContentKeyOf("test:presenter/world");
}
static int DrawWorld(const struct RhhWorldFrame *frame, const void *state)
{
    (void)state;
#ifdef _WIN32
#define RHH_GL_CALL __stdcall
#else
#define RHH_GL_CALL
#endif
    void (RHH_GL_CALL *clearColor)(float, float, float, float) = frame->gl_proc("glClearColor");
    void (RHH_GL_CALL *clear)(unsigned) = frame->gl_proc("glClear");
    CHECK(frame->width == 240 && frame->height == 160 && clearColor && clear);
    if (sWorldMaskMode && frame->struct_size >= RHH_WORLD_FRAME_V1_1_SIZE && frame->output_mask && frame->output_mask_valid)
    {
        memset(frame->output_mask, 255, 240 * 160); frame->output_mask[1] = 128;
        if (sWorldMaskMode == 2 || sWorldMaskMode == 5) frame->output_mask[100] = 0;
        *frame->output_mask_valid = sWorldMaskMode == 3 ? 2 : 1;
        if (sWorldMaskMode >= 4 && frame->struct_size >= RHH_WORLD_FRAME_V1_2_SIZE && frame->output_mask_texture)
            *frame->output_mask_texture = UINT32_MAX; /* Invalid GL name. */
        if (sWorldMaskMode >= 6 && frame->struct_size >= RHH_WORLD_FRAME_V1_2_SIZE && frame->output_mask_texture)
        {
            void (RHH_GL_CALL *gen)(int, unsigned *) = frame->gl_proc("glGenTextures");
            void (RHH_GL_CALL *bind)(unsigned, unsigned) = frame->gl_proc("glBindTexture");
            void (RHH_GL_CALL *param)(unsigned, unsigned, int) = frame->gl_proc("glTexParameteri");
            void (RHH_GL_CALL *upload)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *) = frame->gl_proc("glTexImage2D");
            CHECK(gen && bind && param && upload);
            if (!sWorldMaskTexture) gen(1, &sWorldMaskTexture);
            static u8 data[240 * 160 * 4]; memset(data, 255, sizeof(data));
            data[((159 * 240) + 1) * 4] = 128; data[2 * 4] = 128;
            bind(0x0de1, sWorldMaskTexture); /* GL_TEXTURE_2D */
            param(0x0de1, 0x2801, sWorldMaskMode == 8 ? 0x2702 : 0x2600); /* min filter */
            param(0x0de1, 0x2800, 0x2600);
            param(0x0de1, 0x813c, sWorldMaskMode == 9 ? 1 : 0); /* base mip level */
            upload(0x0de1, 0, 0x1908, sWorldMaskMode == 7 ? 1 : 240,
                   sWorldMaskMode == 7 ? 1 : 160, 0, 0x1908, 0x1401, data);
            frame->output_mask[1] = sWorldMaskMode == 6 ? 255 : 128;
            *frame->output_mask_texture = sWorldMaskTexture;
        }
    }
    clearColor(1.f, 0.f, 0.f, 1.f); clear(0x4000); /* GL_COLOR_BUFFER_BIT */
#undef RHH_GL_CALL
    return sWorldDeclines;
}
static void CaptureEnvironment(uint8_t *rgb, unsigned frame)
{
    static u16 pixels[240 * 160];
    struct NativeBattleEnvironment env = ModsBattleEnvironment();
    DrawFrame(pixels);
    NativeShaderPresent(pixels, &env, frame);
    CHECK(!NativeShaderCapture(rgb));
}
static void CheckBattleEnvironment(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins());
    const struct RhhModShader shaders[] = {
        {sizeof(struct RhhModShader), 0, "test:shader/broken", "invalid GLSL"},
        {sizeof(struct RhhModShader), 0, "test:shader/mask_probe",
            "#version 120\nuniform sampler2D uMask; varying vec2 vUv; void main(){"
            "gl_FragColor=texture2D(uMask,vUv).r<.75 ? vec4(1.,0.,0.,1.) : vec4(0.,1.,0.,1.);}"}
    };
    const struct RhhModHook hooks[] = {
        {sizeof(struct RhhModHook), RHH_BATTLE_ENVIRONMENT, RHH_TRANSFORM, 100, BrokenEnvironment, NULL},
        {sizeof(struct RhhModHook), RHH_WORLD_PRESENTATION, RHH_TRANSFORM, 100, SelectWorld, NULL}
    };
    const struct RhhWorldPresenter presenter = {sizeof(presenter), 0, "test:presenter/world", DrawWorld, NULL};
    const struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_V1,
        .id = "test:broken_shader", .mod_version = 1, .hook_count = ARRAY_COUNT(hooks), .hooks = hooks,
        .shader_count = ARRAY_COUNT(shaders), .shaders = shaders, .world_presenter_count = 1, .world_presenters = &presenter};
    CHECK(!RhhModsRegister(&mod)); CHECK(!RhhModsFinalize());
    CHECK(!ModsBattleEnvironmentEnabled()); ModsSetBattleEnvironment(1);
    gBattleStruct = &sBattle; gBattlersCount = 2;
    gMain.inBattle = TRUE; gMain.callback2 = BattleMainCB2;
    gBattlerPositions[0] = B_POSITION_PLAYER_LEFT; gBattlerPositions[1] = B_POSITION_OPPONENT_LEFT;
    memset(gBattleMons, 0, sizeof(gBattleMons));
    for (unsigned i = 0; i < 2; ++i) gBattleMons[i].hp = gBattleMons[i].maxHP = 100;
    ResetPaletteFade(); ResetSpriteData(); FreeAllSpritePalettes();
    const unsigned weather[] = {B_WEATHER_NONE, B_WEATHER_RAIN_NORMAL, B_WEATHER_SUN_NORMAL,
        B_WEATHER_SANDSTORM, B_WEATHER_HAIL, B_WEATHER_SNOW, B_WEATHER_FOG,
        B_WEATHER_STRONG_WINDS, B_WEATHER_SUN_PRIMAL, B_WEATHER_RAIN_PRIMAL, B_WEATHER_RAIN_DOWNPOUR};
    rng_value_t rng = gRngValue;
    for (unsigned i = 0; i < ARRAY_COUNT(weather); ++i)
    {
        gBattleWeather = weather[i];
        CHECK(ModsBattleEnvironment().weather == (i == 10 ? RHH_WEATHER_HEAVY_RAIN : i));
    }
    for (unsigned i = B_TERRAIN_NONE; i < B_TERRAIN_COUNT; ++i)
    {
        gFieldTimers.terrain = i; CHECK(ModsBattleEnvironment().terrain == i);
    }
    CHECK(!memcmp(&gRngValue, &rng, sizeof(rng)));
    gFieldTimers.terrain = B_TERRAIN_NONE; gBattleWeather = B_WEATHER_RAIN_NORMAL;
    RhhContentKey key = ModsBattleEnvironment().shader;
    CHECK(key && RhhModsFindShader(key)); CHECK(ModsShowWeatherRecap());
    gMain.callback2 = NULL; CHECK(!ModsBattleEnvironment().shader);
    gMain.callback2 = BattleMainCB2; gMain.inBattle = FALSE; CHECK(!ModsBattleEnvironment().shader);
    gMain.inBattle = TRUE;
    gBattleMons[0].ability = ABILITY_CLOUD_NINE;
    CHECK(ModsBattleEnvironment().suppressed);
    gBattleMons[0].ability = ABILITY_AIR_LOCK; CHECK(ModsBattleEnvironment().suppressed);
    gBattleMons[0].ability = ABILITY_NONE; CHECK(!ModsBattleEnvironment().suppressed);
    ModsSetBattleEnvironment(0); CHECK(!ModsBattleEnvironmentEnabled() && !ModsBattleEnvironment().shader);
    uint8_t *saved; uint32_t savedSize;
    CHECK(!RhhModsExport(&saved, &savedSize)); ModsSetBattleEnvironment(1);
    CHECK(!RhhModsImport(saved, savedSize)); CHECK(!ModsBattleEnvironmentEnabled()); free(saved);
    ModsSetBattleEnvironment(1);
    struct BattleScriptsStack stack = {0};
    struct BattleResources resources = {.battleScriptsStack = &stack};
    gBattleResources = &resources;
    gBattleStruct->weatherDuration = 3;
    CHECK(EndOrContinueWeather() && gBattleStruct->weatherDuration == 2);
    CHECK(gBattlescriptCurrInstr == BattleScript_WeatherContinues);
    stack.size = 0;
    CHECK(!SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_Window *window = SDL_CreateWindow("Battle environment test", 0, 0, 480, 320, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window || NativeShaderInit(window))
    {
        CHECK(!strcmp(SDL_GetCurrentVideoDriver(), "dummy"));
        CHECK(ModsShowWeatherRecap() && !NativeShaderReady(key));
        SDL_DestroyWindow(window); SDL_Quit();
        puts("battle environment: mappings, menus, suppression, settings, persistence and no-GL recaps passed");
        return;
    }
    printf("battle environment renderer: %s\n", NativeShaderDriver());
    {
        static u16 pixels[240 * 160]; static u8 captured[240 * 160 * 3];
        for (unsigned i = 0; i < ARRAY_COUNT(pixels); ++i) pixels[i] = RGB(0, 0, 31);
        memset(gNativeShaderSceneMask, 255, sizeof(gNativeShaderSceneMask));
        gNativeShaderSceneMask[0] = 0;
        struct NativeBattleEnvironment plain = {0}; sWorldSelected = 1;
        NativeShaderPresentWorld(pixels, &plain, 3); CHECK(!NativeShaderCapture(captured));
        CHECK(captured[0] == 0 && captured[1] == 0 && captured[2] == 255);
        CHECK(captured[3] == 255 && captured[4] == 0 && captured[5] == 0);
        sWorldDeclines = 1;
        NativeShaderPresentWorld(pixels, &plain, 4); CHECK(!NativeShaderCapture(captured));
        CHECK(captured[3] == 0 && captured[4] == 0 && captured[5] == 255);
        sWorldSelected = sWorldDeclines = 0;
        NativeShaderPresentWorld(pixels, &plain, 5); CHECK(!NativeShaderCapture(captured));
        CHECK(captured[3] == 0 && captured[4] == 0 && captured[5] == 255);
        plain.shader = RhhContentKeyOf("test:shader/mask_probe"); plain.visibility = 1.f; sWorldSelected = 1;
        sWorldMaskMode = 1;
        NativeShaderPresentWorld(pixels, &plain, 6); CHECK(!NativeShaderCapture(captured));
        CHECK(!captured[0] && !captured[1] && captured[2] == 255
            && captured[3] == 255 && !captured[4] && !captured[5]
            && !captured[6] && captured[7] == 255 && !captured[8]);
        sWorldMaskMode = 2; /* Invalid byte must discard the entire projection. */
        NativeShaderPresentWorld(pixels, &plain, 7); CHECK(!NativeShaderCapture(captured));
        CHECK(!captured[3] && captured[4] == 255 && !captured[5]);
        sWorldMaskMode = 3; /* Only the exact publication flag is accepted. */
        NativeShaderPresentWorld(pixels, &plain, 8); CHECK(!NativeShaderCapture(captured));
        CHECK(!captured[3] && captured[4] == 255 && !captured[5]);
        sWorldMaskMode = 4; /* An invalid GL name can fall back to a valid CPU mask. */
        NativeShaderPresentWorld(pixels, &plain, 10); CHECK(!NativeShaderCapture(captured));
        CHECK(captured[3] == 255 && !captured[4] && !captured[5]);
        sWorldMaskMode = 5; /* If both outputs are invalid, retain the original mask. */
        NativeShaderPresentWorld(pixels, &plain, 11); CHECK(!NativeShaderCapture(captured));
        CHECK(!captured[3] && captured[4] == 255 && !captured[5]);
        sWorldMaskMode = 6; /* GPU output takes precedence and has framebuffer orientation. */
        NativeShaderPresentWorld(pixels, &plain, 12); CHECK(!NativeShaderCapture(captured));
        unsigned lower = (159 * 240 + 2) * 3;
        CHECK(!captured[0] && !captured[1] && captured[2] == 255
            && captured[3] == 255 && !captured[4] && !captured[5]
            && !captured[6] && captured[7] == 255 && !captured[8]
            && captured[lower] == 255 && !captured[lower + 1] && !captured[lower + 2]);
        sWorldMaskMode = 7; /* Wrong texture size falls back to the CPU mask. */
        NativeShaderPresentWorld(pixels, &plain, 13); CHECK(!NativeShaderCapture(captured));
        CHECK(captured[3] == 255 && !captured[4] && !captured[5]);
        sWorldMaskMode = 8; /* A mipmapped filter without mipmaps is not accepted. */
        NativeShaderPresentWorld(pixels, &plain, 14); CHECK(!NativeShaderCapture(captured));
        CHECK(captured[3] == 255 && !captured[4] && !captured[5]
            && !captured[6] && captured[7] == 255 && !captured[8]);
        sWorldMaskMode = 9; /* The advertised mask must actually use level zero. */
        NativeShaderPresentWorld(pixels, &plain, 15); CHECK(!NativeShaderCapture(captured));
        CHECK(captured[3] == 255 && !captured[4] && !captured[5]
            && !captured[6] && captured[7] == 255 && !captured[8]);
        sWorldMaskMode = 1; sWorldDeclines = 1; /* Declined frames cannot publish masks. */
        NativeShaderPresentWorld(pixels, &plain, 9); CHECK(!NativeShaderCapture(captured));
        CHECK(!captured[3] && captured[4] == 255 && !captured[5]);
        sWorldMaskMode = sWorldSelected = sWorldDeclines = 0;
    }
    CHECK(NativeShaderReady(key)); CHECK(!ModsShowWeatherRecap());
    CHECK(EndOrContinueWeather() && gBattleStruct->weatherDuration == 1);
    CHECK(gBattlescriptCurrInstr == BattleScript_WeatherContinuesSilent);
    stack.size = 0;
    CHECK(EndOrContinueWeather() && !gBattleWeather && gBattleStruct->weatherDuration == 0);
    CHECK(gBattlescriptCurrInstr == BattleScript_WeatherFaded);
    CHECK(!ModsBattleEnvironment().shader);
    /* A simple tile scene with a BG0 text tile and a real UI sprite palette. */
    memset(gNativeVram, 0, sizeof(gNativeVram)); memset(gNativePalette, 0, sizeof(gNativePalette));
    memset(gNativeOam, 0, sizeof(gNativeOam));
    for (unsigned i = 0; i < 128; ++i) ((struct OamData *)gNativeOam)[i].affineMode = ST_OAM_AFFINE_ERASE;
    REG_DISPCNT = DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP;
    REG_BG0CNT = 30 << 8; REG_BG1CNT = (31 << 8) | 1;
    REG_BG0HOFS = REG_BG0VOFS = REG_BG1HOFS = REG_BG1VOFS = 0;
    REG_BLDCNT = 0;
    memset(gNativeVram + 32, 0x11, 32); memset(gNativeVram + 0x10000, 0x22, 32);
    for (unsigned i = 0; i < 1024; ++i) ((u16 *)BG_SCREEN_ADDR(31))[i] = 1;
    ((u16 *)BG_SCREEN_ADDR(30))[0] = 1;
    ((u16 *)gNativePalette)[1] = RGB(10, 14, 16);
    u16 pal[16] = {0}; pal[2] = RGB(28, 22, 10);
    struct SpritePalette uiPal = {pal, TAG_HEALTHBOX_PAL};
    unsigned palette = LoadSpritePalette(&uiPal);
    /* Transfer the palette just as the VBlank callback does. */
    memcpy(gNativePalette + 0x200 + palette * 32, pal, sizeof(pal));
    struct OamData *oam = (struct OamData *)gNativeOam;
    *oam = (struct OamData){.x = 10, .y = 10, .paletteNum = palette};
    unsigned monPalette = (palette + 1) % 16;
    memcpy(gNativePalette + 0x200 + monPalette * 32, pal, sizeof(pal));
    oam[1] = (struct OamData){.x = 176, .y = 68, .paletteNum = monPalette};
    static uint8_t base[240 * 160 * 3], current[sizeof(base)], other[sizeof(base)];
    CaptureEnvironment(base, 100);
    CHECK(gNativeShaderSceneMask[0] == 0 && gNativeShaderSceneMask[10 * 240 + 10] == 0);
    CHECK(gNativeShaderSceneMask[68 * 240 + 176] == 128);
    CHECK(gNativeShaderSceneMask[50 * 240 + 100] == 255 && gNativeShaderSceneMask[120 * 240] == 0);
    for (unsigned i = 1; i < ARRAY_COUNT(weather); ++i)
    {
        gBattleWeather = weather[i]; CaptureEnvironment(current, 100);
        CHECK(memcmp(base, current, sizeof(base)));
        for (unsigned pixel = 0; pixel < 240 * 160; ++pixel)
            if (!gNativeShaderSceneMask[pixel]) CHECK(!memcmp(base + pixel * 3, current + pixel * 3, 3));
    }
    gBattleWeather = B_WEATHER_RAIN_NORMAL; CaptureEnvironment(current, 100); CaptureEnvironment(other, 160);
    CHECK(memcmp(base + (68 * 240 + 176) * 3, current + (68 * 240 + 176) * 3, 3));
    CHECK(memcmp(current, other, sizeof(current))); /* Persistent animation advances. */
    for (unsigned i = B_TERRAIN_GRASSY; i < B_TERRAIN_COUNT; ++i)
    {
        gFieldTimers.terrain = i; CaptureEnvironment(current, 100);
        gBattleWeather = 0; CaptureEnvironment(other, 100);
        CHECK(memcmp(current, other, sizeof(current))); CHECK(memcmp(base, other, sizeof(base)));
        /* Terrain is behind the sprite, while the platform beneath it changes. */
        CHECK(!memcmp(base + (68 * 240 + 176) * 3, other + (68 * 240 + 176) * 3, 3));
        CHECK(memcmp(base + (72 * 240 + 165) * 3, other + (72 * 240 + 165) * 3, 3));
        /* Terrain spans the field, including outside the old platform patches. */
        for (unsigned x = 16; x <= 224; x += 104)
            CHECK(memcmp(base + (85 * 240 + x) * 3, other + (85 * 240 + x) * 3, 3));
        CHECK(!memcmp(base + (10 * 240 + 100) * 3, other + (10 * 240 + 100) * 3, 3));
        gBattleWeather = B_WEATHER_RAIN_NORMAL;
    }
    gBattleMons[0].ability = ABILITY_CLOUD_NINE; CaptureEnvironment(current, 100);
    gBattleWeather = 0; CaptureEnvironment(other, 100); CHECK(!memcmp(current, other, sizeof(current)));
    CHECK(memcmp(base, current, sizeof(base))); /* Suppression leaves terrain visible. */
    gBattleMons[0].ability = ABILITY_NONE; gFieldTimers.terrain = B_TERRAIN_NONE;
    CaptureEnvironment(current, 100); CHECK(!memcmp(base, current, sizeof(base)));
    gBattleWeather = B_WEATHER_RAIN_NORMAL; gBattleTypeFlags = BATTLE_TYPE_DOUBLE;
    gFieldTimers.terrain = B_TERRAIN_ELECTRIC; CaptureEnvironment(current, 100);
    CHECK(memcmp(base, current, sizeof(base)));
    gPaletteFade.y = 16; CaptureEnvironment(current, 100); CHECK(!memcmp(base, current, sizeof(base)));
    gPaletteFade.y = 0;
    ModsSetBattleEnvironment(0); CaptureEnvironment(current, 100);
    CHECK(!memcmp(base, current, sizeof(base)) && ModsShowWeatherRecap()); ModsSetBattleEnvironment(1);
    gMain.callback2 = NULL; CaptureEnvironment(current, 100); CHECK(!memcmp(base, current, sizeof(base)));
    gMain.callback2 = BattleMainCB2; gMain.inBattle = FALSE;
    CaptureEnvironment(current, 100); CHECK(!memcmp(base, current, sizeof(base))); gMain.inBattle = TRUE;
    sBrokenEnvironment = 1; CHECK(!NativeShaderReady(ModsBattleEnvironment().shader));
    CHECK(ModsShowWeatherRecap()); CaptureEnvironment(current, 100); CHECK(!memcmp(base, current, sizeof(base)));
    sBrokenEnvironment = 0; CHECK(!ModsShowWeatherRecap());
    CHECK(!memcmp(&gRngValue, &rng, sizeof(rng)));
    if (sWorldMaskTexture)
    {
#ifdef _WIN32
        void (__stdcall *deleteTextures)(int, const unsigned *) = SDL_GL_GetProcAddress("glDeleteTextures");
#else
        void (*deleteTextures)(int, const unsigned *) = SDL_GL_GetProcAddress("glDeleteTextures");
#endif
        CHECK(deleteTextures); deleteTextures(1, &sWorldMaskTexture); sWorldMaskTexture = 0;
    }
    NativeShaderShutdown(); CHECK(ModsShowWeatherRecap()); SDL_DestroyWindow(window); SDL_Quit();
    RhhModsUnloadLibraries();
    puts("battle environment: all weather/terrain GLSL, animation, combined effects, UI masks, expiry, suppression, menus and shader failure passed");
}
