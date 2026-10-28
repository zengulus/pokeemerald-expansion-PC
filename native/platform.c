#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "global.h"
#include "main.h"
#include "m4a.h"
#include "gba/m4a_internal.h"
#include "native.h"
#include "native_audio.h"
#include "load_save.h"
#include "script.h"
#include "save.h"
#include "pokemon.h"
#include "money.h"
#include "event_data.h"
#include "battle.h"
#include "mod_host.h"
#include "mods.h"
#include "mod_music.h"
#include "event_object_movement.h"

static SDL_Window *sWindow;
static SDL_Renderer *sRenderer;
static SDL_Texture *sTexture;
static SDL_AudioDeviceID sAudio;
static FILE *sModTrace;
static float *sMixedAudio;
static size_t sMixedAudioCapacity;
static u16 sPixels[DISPLAY_WIDTH * DISPLAY_HEIGHT];
static unsigned sFrame;
static unsigned sLimit;
static const char *sScreenshot;
static FILE *sAudioCapture;
static FILE *sInputReplay;
static FILE *sTrace;
static const char *sVideoState;
static const char *sReport;
static int sRealtime;
static u64 sAudioSamplesGenerated;
static u64 sAudioSamplesQueued;
static unsigned sNextInputFrame;
static unsigned sNextInputMask;
static u16 sReplayKeys;
void DrawFrame(u16 *pixels);
extern void CB2_InitCopyrightScreenAfterBootup(void);

// These symbols are metadata on GBA. They are ordinary objects/callbacks here.
const u8 RomHeaderGameCode[4] = {'B', 'P', 'E', 'E'};
const u8 RomHeaderSoftwareVersion = 0;
u32 IntrMain[1];
void gInitialMainCB2(void) { CB2_InitCopyrightScreenAfterBootup(); }

void Platform_QueueAudio(float *samples, s32 bytes)
{
    if (NativeMusicActive())
    {
        /* MP2K retains its output for reverb. Mix into a separate delivery
         * buffer so the replacement BGM never feeds back into its SFX mixer. */
        if ((size_t)bytes > sMixedAudioCapacity)
        {
            float *grown = realloc(sMixedAudio, bytes);
            if (!grown) { fprintf(stderr, "Audio delivery allocation failed\n"); abort(); }
            sMixedAudio = grown; sMixedAudioCapacity = bytes;
        }
        memcpy(sMixedAudio, samples, bytes); samples = sMixedAudio;
        NativeMusicMix(samples, bytes / (2 * sizeof(float)));
    }
    sAudioSamplesGenerated += bytes / sizeof(float);
    if (sAudioCapture) fwrite(samples, 1, bytes, sAudioCapture);
    if (sAudio && SDL_GetQueuedAudioSize(sAudio) < 42060 * 8)
        if (SDL_QueueAudio(sAudio, samples, bytes) == 0)
            sAudioSamplesQueued += bytes / sizeof(float);
}

static void Input(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_QUIT) sLimit = sFrame + 1;
    const u8 *keys = SDL_GetKeyboardState(NULL);
    u16 value = 0;
#define KEY(sdl, gba) if (keys[SDL_SCANCODE_##sdl]) value |= gba
    KEY(Z, A_BUTTON); KEY(X, B_BUTTON); KEY(RETURN, START_BUTTON);
    KEY(BACKSPACE, SELECT_BUTTON); KEY(A, L_BUTTON); KEY(S, R_BUTTON);
    KEY(UP, DPAD_UP); KEY(DOWN, DPAD_DOWN); KEY(LEFT, DPAD_LEFT); KEY(RIGHT, DPAD_RIGHT);
#undef KEY
    if (sInputReplay)
    {
        while (sFrame >= sNextInputFrame)
        {
            sReplayKeys = sNextInputMask;
            if (fscanf(sInputReplay, "%u %x", &sNextInputFrame, &sNextInputMask) != 2)
            {
                sNextInputFrame = UINT_MAX;
                break;
            }
        }
        value = sReplayKeys;
    }
    REG_KEYINPUT = value ^ KEYS_MASK;
}

void NativeFrame(void)
{
    Input();
    REG_VCOUNT = 160;
    REG_DISPSTAT |= DISPSTAT_VBLANK;
    AgbMainLoop();
    ModsEnforceParty();
    NativeRunDma(1);
    if (REG_IME && (REG_IE & INTR_FLAG_VBLANK)) gIntrTable[4]();
    REG_DISPSTAT &= ~DISPSTAT_VBLANK;
    DrawFrame(sPixels);
    SDL_UpdateTexture(sTexture, NULL, sPixels, DISPLAY_WIDTH * 2);
    SDL_RenderClear(sRenderer);
    SDL_RenderCopy(sRenderer, sTexture, NULL, NULL);
    SDL_RenderPresent(sRenderer);
    ++sFrame;
    if (sTrace && (sFrame % 30 == 0))
    {
        fprintf(sTrace, "%u cb=%p state=%u keys=%x map=%d,%d pos=%d,%d battle=%u script=%u\n",
            sFrame, (void *)gMain.callback2, gMain.state, gMain.heldKeys,
            gSaveBlock1Ptr ? gSaveBlock1Ptr->location.mapGroup : -1,
            gSaveBlock1Ptr ? gSaveBlock1Ptr->location.mapNum : -1,
            gSaveBlock1Ptr ? gSaveBlock1Ptr->pos.x : -1,
            gSaveBlock1Ptr ? gSaveBlock1Ptr->pos.y : -1,
            gMain.inBattle, ScriptContext_IsEnabled());
        fflush(sTrace);
    }
}

void VBlankIntrWait(void) { NativeFrame(); }
void ReInitializeEWRAM(void) { AgbMain(); }
void SoftReset(u32 flags) { (void)flags; AgbMain(); }
void RegisterRamReset(u32 flags)
{
    if (flags & RESET_VRAM) memset(gNativeVram, 0, sizeof(gNativeVram));
    if (flags & RESET_PALETTE) memset(gNativePalette, 0, sizeof(gNativePalette));
    if (flags & RESET_OAM) memset(gNativeOam, 0, sizeof(gNativeOam));
}

u16 Sqrt(u32 value) { return (u16)sqrt((double)value); }
u16 ArcTan2(s16 x, s16 y) { return (u16)(atan2((double)y, (double)x) * (65536.0 / (2 * M_PI))); }
s32 Div(s32 num, s32 denominator) { return denominator ? num / denominator : 0; }

void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dst, s32 count)
{
    while (count--)
    {
        double angle = (src->alpha >> 8) * (2 * M_PI / 256.0);
        s32 c = (s16)(cos(angle) * 16384), s = (s16)(sin(angle) * 16384);
        dst->pa = (c * src->sx) >> 14;
        dst->pb = -(s * src->sx) >> 14;
        dst->pc = (s * src->sy) >> 14;
        dst->pd = (c * src->sy) >> 14;
        dst->dx = src->texX - dst->pa * src->scrX - dst->pb * src->scrY;
        dst->dy = src->texY - dst->pc * src->scrX - dst->pd * src->scrY;
        ++src; ++dst;
    }
}

void ObjAffineSet(struct ObjAffineSrcData *src, void *output, s32 count, s32 stride)
{
    u8 *dst = output;
    while (count--)
    {
        double angle = (src->rotation >> 8) * (2 * M_PI / 256.0);
        s32 c = (s16)(cos(angle) * 16384), s = (s16)(sin(angle) * 16384);
        s16 matrix[4] = {(c * src->xScale) >> 14, -(s * src->xScale) >> 14,
                        (s * src->yScale) >> 14, (c * src->yScale) >> 14};
        for (unsigned i = 0; i < 4; ++i, dst += stride) memcpy(dst, &matrix[i], 2);
        ++src;
    }
}

static int WriteReport(const char *path)
{
    FILE *file = fopen(path, "w");
    if (!file) return -1;
    struct ModConfig mods = ModsGetConfig();
    fprintf(file, "{\"mods\":{\"flags\":%u,\"wild_mode\":%u,\"music\":%u,\"seed\":\"%016llx\",\"starter_key\":\"%016llx\",\"level_cap\":%u,\"follower_present\":%s,\"run_failed\":%s,\"music_asset\":%u},",
        mods.flags, mods.wildMode, mods.music, (unsigned long long)mods.seed,
        (unsigned long long)mods.starter, ModsLevelCap(), GetFollowerObject() ? "true" : "false",
        ModsRunFailed() ? "true" : "false", NativeMusicAsset());
    fprintf(file, "\"code_anchor\":\"%p\",", (void *)AgbMain);
    fprintf(file, "\"pointer_bits\":%u,\"frames\":%u,\"map_group\":%d,\"map_num\":%d,\"x\":%d,\"y\":%d,",
        (unsigned)(sizeof(void *) * CHAR_BIT), sFrame,
        gSaveBlock1Ptr ? gSaveBlock1Ptr->location.mapGroup : -1,
        gSaveBlock1Ptr ? gSaveBlock1Ptr->location.mapNum : -1,
        gSaveBlock1Ptr ? gSaveBlock1Ptr->pos.x : -1,
        gSaveBlock1Ptr ? gSaveBlock1Ptr->pos.y : -1);
    fprintf(file, "\"save_counter\":%u,\"save_status\":%u,\"in_battle\":%u,\"battle_outcome\":%u,\"name_hex\":\"",
        gSaveCounter, gSaveFileStatus, gMain.inBattle, gBattleOutcome);
    if (gSaveBlock2Ptr)
        for (unsigned i = 0; i < sizeof(gSaveBlock2Ptr->playerName); ++i)
            fprintf(file, "%02x", gSaveBlock2Ptr->playerName[i]);
    fprintf(file, "\",\"money\":%u,\"intro_state\":%u,\"wall_clock_set\":%u,\"rescued_birch\":%u,\"party\":[",
        gSaveBlock1Ptr ? GetMoney(&gSaveBlock1Ptr->money) : 0,
        gSaveBlock1Ptr ? VarGet(VAR_LITTLEROOT_INTRO_STATE) : 0,
        gSaveBlock1Ptr ? FlagGet(FLAG_SET_WALL_CLOCK) : 0,
        gSaveBlock1Ptr ? FlagGet(FLAG_RESCUED_BIRCH) : 0);
    for (unsigned i = 0; i < gPlayerPartyCount; ++i)
        fprintf(file, "%s{\"species\":%u,\"level\":%u,\"hp\":%u,\"max_hp\":%u}",
            i ? "," : "", GetMonData(&gPlayerParty[i], MON_DATA_SPECIES),
            GetMonData(&gPlayerParty[i], MON_DATA_LEVEL), GetMonData(&gPlayerParty[i], MON_DATA_HP),
            GetMonData(&gPlayerParty[i], MON_DATA_MAX_HP));
    fprintf(file, "],\"audio_device_open\":%s,\"audio_driver\":\"%s\",\"audio_samples_generated\":%llu,\"audio_samples_queued\":%llu}\n",
        sAudio ? "true" : "false", SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "none",
        (unsigned long long)sAudioSamplesGenerated, (unsigned long long)sAudioSamplesQueued);
    int failed = ferror(file);
    if (fclose(file)) failed = 1;
    return failed ? -1 : 0;
}

int main(int argc, char **argv)
{
    const char *save = "pokeemerald-expansion.sav";
    RhhModsInit();
    for (int i = 1; i + 1 < argc; ++i)
        if (!strcmp(argv[i], "--external-bundled-mods")) ModsSetExternalBuiltinDirectory(argv[i + 1]);
    if (ModsRegisterBuiltins())
    { fprintf(stderr, "Mod initialization failed: %s\n", RhhModsError()); return 1; }
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--external-bundled-mods") && i + 1 < argc) ++i;
        else if (!strcmp(argv[i], "--mod") && i + 1 < argc)
        { if (RhhModsLoadLibrary(argv[++i])) return 1; }
        else if (!strcmp(argv[i], "--mod-trace") && i + 1 < argc)
        {
            sModTrace = fopen(argv[++i], "w");
            if (!sModTrace) return 1;
            RhhModsSetTrace(sModTrace);
        }
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) sLimit = strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) sScreenshot = argv[++i];
        else if (!strcmp(argv[i], "--audio-capture") && i + 1 < argc) sAudioCapture = fopen(argv[++i], "wb");
        else if (!strcmp(argv[i], "--trace") && i + 1 < argc) sTrace = fopen(argv[++i], "w");
        else if (!strcmp(argv[i], "--video-state") && i + 1 < argc) sVideoState = argv[++i];
        else if (!strcmp(argv[i], "--report") && i + 1 < argc) sReport = argv[++i];
        else if (!strcmp(argv[i], "--realtime")) sRealtime = 1;
        else if (!strcmp(argv[i], "--fixed-rtc") && i + 1 < argc)
        {
            if (NativeRtcSetFixed(argv[++i]))
            {
                fprintf(stderr, "Invalid fixed RTC: %s\n", argv[i]);
                return 1;
            }
        }
        else if (!strcmp(argv[i], "--input") && i + 1 < argc)
        {
            sInputReplay = fopen(argv[++i], "r");
            if (!sInputReplay || fscanf(sInputReplay, "%u %x", &sNextInputFrame, &sNextInputMask) != 2)
                return 1;
        }
        else { fprintf(stderr, "Unknown argument: %s\n", argv[i]); return 1; }
    }
    if (RhhModsFinalize()) { fprintf(stderr, "Mod ordering failed: %s\n", RhhModsError()); return 1; }
    if (NativeSaveOpen(save)) return 1;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER))
        goto failed;
    char *base = SDL_GetBasePath();
    if (base)
    {
        char *music = malloc(strlen(base) + sizeof("music"));
        if (music) { sprintf(music, "%smusic", base); NativeMusicSetAssetRoot(music); free(music); }
        SDL_free(base);
    }
    sWindow = SDL_CreateWindow("Pokémon Emerald Expansion", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, 720, 480, SDL_WINDOW_RESIZABLE);
    if (!sWindow) goto failed;
    sRenderer = SDL_CreateRenderer(sWindow, -1, 0);
    if (!sRenderer) goto failed;
    SDL_RenderSetLogicalSize(sRenderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    sTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ABGR1555,
        SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (!sTexture) goto failed;
    SDL_AudioSpec spec = {.freq = 42060, .format = AUDIO_F32SYS, .channels = 2, .samples = 1024};
    sAudio = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if (!sAudio) fprintf(stderr, "Audio device unavailable: %s\n", SDL_GetError());
    else SDL_PauseAudioDevice(sAudio, 0);
    cgb_audio_init(42060);
    REG_KEYINPUT = KEYS_MASK;
    AgbMain();
    Uint64 frequency = SDL_GetPerformanceFrequency();
    double nextFrame = SDL_GetPerformanceCounter();
    while (!sLimit || sFrame < sLimit)
    {
        NativeFrame();
        if (!sLimit || sRealtime)
        {
            nextFrame += (double)frequency / 60;
            double now = SDL_GetPerformanceCounter();
            if (now - nextFrame > (double)frequency / 15)
                nextFrame = now;
            while ((now = SDL_GetPerformanceCounter()) < nextFrame)
                SDL_Delay((Uint32)((nextFrame - now) * 1000 / frequency));
        }
    }
    if (sScreenshot)
    {
        SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(sPixels, DISPLAY_WIDTH,
            DISPLAY_HEIGHT, 16, DISPLAY_WIDTH * 2, 31, 31 << 5, 31 << 10, 0);
        if (!surface || SDL_SaveBMP(surface, sScreenshot)) goto failed;
        SDL_FreeSurface(surface);
    }
    if (sVideoState)
    {
        FILE *file = fopen(sVideoState, "wb");
        if (!file) return 1;
        fwrite(gNativeRegisters, 1, sizeof(gNativeRegisters), file);
        fwrite(gNativePalette, 1, sizeof(gNativePalette), file);
        fwrite(gNativeVram, 1, sizeof(gNativeVram), file);
        fwrite(gNativeOam, 1, sizeof(gNativeOam), file);
        fclose(file);
    }
    printf("native frames=%u callback=%p\n", sFrame, (void *)gMain.callback2);
    if (sReport && WriteReport(sReport))
    {
        perror("Native report");
        return 1;
    }
    if (sAudioCapture) fclose(sAudioCapture);
    if (sTrace) fclose(sTrace);
    if (sInputReplay) fclose(sInputReplay);
    SDL_CloseAudioDevice(sAudio);
    SDL_DestroyTexture(sTexture);
    SDL_DestroyRenderer(sRenderer);
    SDL_DestroyWindow(sWindow);
    SDL_Quit();
    NativeSaveClose();
    NativeMusicShutdown();
    free(sMixedAudio); sMixedAudio = NULL; sMixedAudioCapacity = 0;
    RhhModsUnloadLibraries();
    RhhModsSetTrace(NULL);
    if (sModTrace) fclose(sModTrace);
    return 0;
failed:
    fprintf(stderr, "SDL: %s\n", SDL_GetError());
    return 1;
}
