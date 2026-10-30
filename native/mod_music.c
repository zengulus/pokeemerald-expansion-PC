/* The original MP2K players continue to own cries and sound effects. Only
 * battle BGM is replaced; timing follows generated samples, not wall time. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#define TSF_IMPLEMENTATION
#include "third_party/tinysoundfont/tsf.h"
#define TML_IMPLEMENTATION
#include "third_party/tinysoundfont/tml.h"
#include "global.h"
#include "main.h"
#include "battle.h"
#include "battle_setup.h"
#include "debug.h"
#include "mods.h"
#include "mod_host.h"
#include "mod_music.h"
#include "mod_ui.h"
#include "native.h"
#include "gba/m4a_internal.h"
#include "m4a.h"
#include "sound.h"
#include "random.h"
#include "constants/songs.h"
#include "third_party/gme/gme.h"
#undef byte

enum { SAMPLE_RATE = 42060, RENDER_BLOCK = 256, TRANSITION_FRAMES = 2524, LEVEL_RAMP_FRAMES = 420 };
static char *sRoot;
static tsf *sFont;
static Music_Emu *sGameBoy;
static tml_message *sSong, *sNext;
static uint64_t sSamples, sLoopStartSamples, sLoopSamples;
static tml_message *sLoopNext;
struct MidiLoop { unsigned start, end; int valid; };
static struct MidiLoop sLoop;
static uint32_t sAsset, sFontGeneration;
static RhhContentKey sTrackKey;
static float sVolume = 1;
static float sTrackGain = 1, sTail[TRANSITION_FRAMES * 2];
static float sGainBase = 1, sGainTarget = 1;
static uint32_t sProposedGain = RHH_MUSIC_GAIN_ONE, sAppliedLevel = RHH_MUSIC_GAIN_ONE, sGainRamp;
static unsigned sTailPosition = TRANSITION_FRAMES, sFadeIn;
static int sActive, sNativeRedirect;
static uint64_t sMusicEpoch;
static struct RhhMusicPlayback sPlayback;
static const struct RhhMusicGainContext *sGainContext;
static const struct RhhMusicLoopContext *sLoopContext;
enum { MUSIC_LIFECYCLE_CAPACITY = 64 };
struct MusicLifecycleQueued { struct RhhMusicLifecycle value; uint64_t runSeed; };
static struct MusicLifecycleQueued sLifecycleQueue[MUSIC_LIFECYCLE_CAPACITY];
static uint32_t sLifecycleHead, sLifecycleCount;
static uint64_t sLifecycleSequence, sLifecycleDropped;
static struct RhhMusicLifecycle sLifecycleCurrent;
static const struct RhhMusicLifecycle *sLifecycleContext;
static int sLifecycleActive, sLifecycleFlushing;
static void QueueLifecycle(struct RhhMusicLifecycle *value)
{
    if (sLifecycleSequence == UINT64_MAX)
    { if (sLifecycleDropped != UINT64_MAX) ++sLifecycleDropped; return; }
    value->sequence = ++sLifecycleSequence;
    if (!RhhModsHasHook(RHH_MUSIC_PLAYBACK_CHANGED)) return;
    if (sLifecycleCount == MUSIC_LIFECYCLE_CAPACITY)
    { if (sLifecycleDropped != UINT64_MAX) ++sLifecycleDropped; return; }
    uint32_t index = (sLifecycleHead + sLifecycleCount++) % MUSIC_LIFECYCLE_CAPACITY;
    sLifecycleQueue[index] = (struct MusicLifecycleQueued){*value, RhhModsRunSeed()};
}
static void StartLifecycle(uint32_t backend, uint32_t context, uint32_t flags, uint32_t asset,
                           RhhContentKey track, RhhContentKey requested, RhhContentKey original,
                           RhhContentKey map, RhhContentKey trainerA, RhhContentKey trainerB)
{
    sLifecycleCurrent = (struct RhhMusicLifecycle){.struct_size = sizeof(sLifecycleCurrent),
        .cause = RHH_MUSIC_STARTED, .backend = backend, .context = context, .flags = flags, .asset = asset,
        .track = track, .requested = requested, .original = original, .map = map, .trainers = {trainerA, trainerB}};
    sLifecycleActive = 1; QueueLifecycle(&sLifecycleCurrent);
}
static int MusicLifecycleState(struct RhhMusicLifecycleState *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhMusicLifecycleState result = {.struct_size = sizeof(result), .active = sLifecycleActive,
        .latest_sequence = sLifecycleSequence, .dropped = sLifecycleDropped};
    if (sLifecycleActive) result.current = sLifecycleCurrent;
    *out = result; return 0;
}
static int MusicLifecycleContext(struct RhhMusicLifecycle *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sLifecycleContext
        || RhhModsCurrentEvent() != RHH_MUSIC_PLAYBACK_CHANGED) return -1;
    *out = *sLifecycleContext; return 0;
}
void NativeMusicFlushLifecycle(void)
{
    if (sLifecycleFlushing || RhhModsCurrentEvent()) return;
    sLifecycleFlushing = 1;
    uint32_t count = sLifecycleCount;
    while (count-- && sLifecycleCount)
    {
        struct MusicLifecycleQueued item = sLifecycleQueue[sLifecycleHead];
        sLifecycleHead = (sLifecycleHead + 1) % MUSIC_LIFECYCLE_CAPACITY; --sLifecycleCount;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MUSIC_PLAYBACK_CHANGED,
            .run_seed = item.runSeed, .ordinal = item.value.sequence, .event_key = item.value.track,
            .subject = item.value.original, .args = {item.value.cause, item.value.backend, item.value.context, item.value.flags}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        sLifecycleContext = &item.value; RhhModsDispatch(&event, &decision); sLifecycleContext = NULL;
        gRngValue = rng; gRng2Value = rng2;
    }
    sLifecycleFlushing = 0;
}

static int sPreviewGuard;
static struct {
    RhhContentKey owner, key;
    tsf *font;
    tml_message *song, *next, *loopNext;
    uint64_t samples, loopStart, loopEnd, elapsed, limit;
    uint32_t duration, volume;
    int wasPaused, explicitLoop;
} sPreview;
static void FinishPreview(int restore);
extern bool8 gDisableMusic;
static struct {
    bool32 pending;
    struct RhhMusicBattleCueRequest request;
    uint64_t run, battle, sequence;
    RhhContentKey original;
    u32 song;
} sBattleCue;
static struct { bool32 armed; struct SongHeader *header; RhhContentKey track; } sBattleCueOverride;
static const struct RhhMusicBattleCueResult *sBattleCueResult;
static bool32 sBattleCueFlushing;
static void DiscardBattleCue(void)
{
    memset(&sBattleCue, 0, sizeof(sBattleCue));
    memset(&sBattleCueOverride, 0, sizeof(sBattleCueOverride));
}

static bool32 BattleCueEventAllowed(void)
{
    if (RhhModsCurrentStage() != RHH_OBSERVE) return FALSE;
    u32 event = RhhModsCurrentEvent();
    return event == RHH_BATTLE_STARTED || event == RHH_BATTLER_ENTERED || event == RHH_BATTLE_TURN_ENDED
        || event == RHH_MOVE_RESOLVED || event == RHH_INNATE_ENTRY || event == RHH_INNATE_TURN_END;
}
static bool32 BattleCueSnapshot(struct RhhBattleSnapshot *battle)
{
    const struct RhhBattleService *service = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 0));
    if (!service || service->current(battle) || battle->phase != RHH_BATTLE_PHASE_ACTIVE
        || (battle->flags & (RHH_BATTLE_LINK | RHH_BATTLE_RECORDED)) || gBattleOutcome) return FALSE;
    return TRUE;
}
static int QueueBattleCue(const struct RhhMusicBattleCueRequest *request)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->requester
        || !RhhModsRegisteredKey(request->requester)) return RHH_MUSIC_CUE_INVALID;
    if (request->track && !RhhModsFindMusic(request->track) && !NativeMusicImportedKnown(request->track)) {
        u32 asset;
        if (RhhContentResolve(RHH_MIDI, request->track, &asset) || asset < 1 || asset > 20) return RHH_MUSIC_CUE_INVALID;
    }
    if (sBattleCue.pending) return RHH_MUSIC_CUE_BUSY;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)}; u32 song;
    if (!BattleCueEventAllowed() || sBattleCueFlushing || !BattleCueSnapshot(&battle) || !sLifecycleActive
        || !sLifecycleCurrent.original || RhhContentResolve(RHH_MUSIC, sLifecycleCurrent.original, &song)
        || gMPlayTable[gSongTable[song].ms].info != &gMPlayInfo_BGM
        || sPreview.owner || gDisableMusic || gMPlayInfo_BGM.ident != ID_NUMBER
        || (gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE) || gMPlayInfo_BGM.fadeOI
        || NativeSaveTransactionActive()) return RHH_MUSIC_CUE_UNAVAILABLE;
    sBattleCue.pending = TRUE; sBattleCue.request = *request;
    sBattleCue.run = battle.run_id; sBattleCue.battle = battle.battle_id;
    sBattleCue.sequence = sLifecycleCurrent.sequence; sBattleCue.original = sLifecycleCurrent.original; sBattleCue.song = song;
    return RHH_MUSIC_CUE_QUEUED;
}
static int BattleCueResult(struct RhhMusicBattleCueResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBattleCueResult
        || RhhModsCurrentEvent() != RHH_MUSIC_BATTLE_CUE_FINISHED) return -1;
    *out = *sBattleCueResult; return 0;
}
void NativeMusicFlushBattleCue(void)
{
    if (!sBattleCue.pending || sBattleCueFlushing || RhhModsCurrentEvent()) return;
    typeof(sBattleCue) cue = sBattleCue; memset(&sBattleCue, 0, sizeof(sBattleCue));
    if (!RhhModsHasActiveRun() || RhhModsRunId() != cue.run || !RhhModsRegisteredKey(cue.request.requester)) return;
    sBattleCueFlushing = TRUE;
    struct RhhMusicBattleCueResult result = {.struct_size = sizeof(result), .outcome = RHH_MUSIC_CUE_STALE,
        .requester = cue.request.requester, .requested = cue.request.track, .original = cue.original,
        .run_id = cue.run, .battle_id = cue.battle, .before_sequence = cue.sequence};
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (BattleCueSnapshot(&battle) && battle.run_id == cue.run && battle.battle_id == cue.battle
        && sLifecycleActive && sLifecycleCurrent.sequence == cue.sequence && sLifecycleCurrent.original == cue.original
        && gMPlayTable[gSongTable[cue.song].ms].info == &gMPlayInfo_BGM
        && !sPreview.owner && !RhhUiActive() && !gDisableMusic && gMPlayInfo_BGM.ident == ID_NUMBER
        && !(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE) && !gMPlayInfo_BGM.fadeOI && !NativeSaveTransactionActive())
    {
        sBattleCueOverride = (typeof(sBattleCueOverride)){TRUE, gSongTable[cue.song].header, cue.request.track};
        PlayBGM(cue.song);
        sBattleCueOverride.armed = FALSE;
        if (sLifecycleActive && sLifecycleCurrent.sequence != cue.sequence && sLifecycleCurrent.original == cue.original)
        {
            result.actual = sLifecycleCurrent.track; result.after_sequence = sLifecycleCurrent.sequence;
            result.outcome = (sLifecycleCurrent.flags & RHH_MUSIC_FALLBACK) ? RHH_MUSIC_CUE_FALLBACK
                : result.actual == (cue.request.track ? cue.request.track : cue.original) ? RHH_MUSIC_CUE_PLAYED : RHH_MUSIC_CUE_OVERRIDDEN;
        }
    }
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MUSIC_BATTLE_CUE_FINISHED,
        .run_seed = RhhModsRunSeed(), .event_key = result.requester, .subject = result.requested, .args = {result.outcome}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBattleCueResult = &result; RhhModsDispatch(&event, &decision); sBattleCueResult = NULL;
    gRngValue = rng; gRng2Value = rng2; sBattleCueFlushing = FALSE;
}
static const char *const sRoles[] = {"wild", "trainer", "gym", "champion"};
/* Per-track trims measured from the bundled MIDI/SoundFont combinations.
 * The shared multiplier below matches their output to the native BGM mixer. */
static const float sTrackGains[20] = {
    3.03f, 3.11f, 3.27f, 2.86f, 2.88f, 2.85f, 2.95f, 3.51f,
    0.425f, 0.347f, 0.357f, 0.508f, 0.312f, 0.325f, 0.496f, 0.336f,
    0.478f, 0.439f, 0.407f, 0.397f
};
static const float sGameBoyGains[8] = {0.102f, 0.109f, 0.119f, 0.111f, 0.099f, 0.111f, 0.112f, 0.140f};
static const struct { u16 song; u8 role; } sBattleSongs[] = {
    {MUS_VS_WILD, 0}, {MUS_VS_REGI, 0}, {MUS_VS_KYOGRE_GROUDON, 0}, {MUS_VS_RAYQUAZA, 0},
    {MUS_VS_MEW, 0}, {MUS_C_VS_LEGEND_BEAST, 0}, {MUS_RG_VS_WILD, 0},
    {MUS_RG_VS_DEOXYS, 0}, {MUS_RG_VS_MEWTWO, 0}, {MUS_RG_VS_LEGEND, 0},
    {MUS_VS_TRAINER, 1}, {MUS_VS_RIVAL, 1}, {MUS_VS_AQUA_MAGMA, 1}, {MUS_RG_VS_TRAINER, 1},
    {MUS_VS_GYM_LEADER, 2}, {MUS_VS_ELITE_FOUR, 2}, {MUS_VS_FRONTIER_BRAIN, 2},
    {MUS_VS_AQUA_MAGMA_LEADER, 2}, {MUS_RG_VS_GYM_LEADER, 2},
    {MUS_VS_CHAMPION, 3}, {MUS_RG_VS_CHAMPION, 3}
};
void NativeMusicSetAssetRoot(const char *root)
{
    free(sRoot); sRoot = NULL;
    if (root) { sRoot = malloc(strlen(root) + 1); if (sRoot) strcpy(sRoot, root); }
}
static char *AssetPath(const char *relative)
{
    if (!sRoot) return NULL;
    char *path = malloc(strlen(sRoot) + strlen(relative) + 2);
    if (path) sprintf(path, "%s/%s", sRoot, relative);
    return path;
}
static void SetProgramFor(tsf *font, unsigned channel, unsigned program)
{
    if (tsf_channel_set_presetnumber(font, channel, program, channel == 9)) return;
    if (channel == 9 && tsf_channel_set_presetnumber(font, channel, 0, 1)) return;
    /* These game banks intentionally have fewer than 128 instruments. Choose
     * the nearest available voice in the GM family before a broader fallback. */
    for (unsigned pass = 0; pass < 2; ++pass)
        for (unsigned distance = 0; distance < 128; ++distance)
            for (unsigned side = 0; side < 2; ++side)
            {
                int candidate = (int)program + (side ? -(int)distance : (int)distance);
                if (candidate < 0 || candidate >= 128 || (!pass && (unsigned)candidate / 8 != program / 8)) continue;
                int index = tsf_get_presetindex(font, channel == 9 ? 128 : 0, candidate);
                if (index >= 0) { tsf_channel_set_presetindex(font, channel, index); return; }
            }
    tsf_channel_set_presetindex(font, channel, 0);
}
static void ResetChannels(void)
{
    tsf_reset(sFont);
    for (unsigned c = 0; c < 16; ++c)
    {
        SetProgramFor(sFont, c, 0);
    }
}
static void RenderSource(float *output, unsigned frames)
{
    if (!sGameBoy)
    {
        tsf_render_float(sFont, output, frames, 0);
        return;
    }
    short buffer[RENDER_BLOCK * 2];
    while (frames)
    {
        unsigned count = frames < RENDER_BLOCK ? frames : RENDER_BLOCK;
        if (gme_play(sGameBoy, count * 2, buffer)) memset(buffer, 0, count * 2 * sizeof(*buffer));
        for (unsigned i = 0; i < count * 2; ++i) *output++ = buffer[i] / 32768.0f;
        frames -= count;
    }
}
static void CaptureTail(void)
{
    if (!sActive || (!sGameBoy && !sFont) || (gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE)) return;
    RenderSource(sTail, TRANSITION_FRAMES);
    float gain = sVolume * sTrackGain * (gMPlayInfo_BGM.fadeOV >> FADE_VOL_SHIFT) / 64.0f;
    for (unsigned i = 0; i < TRANSITION_FRAMES; ++i)
    {
        float fade = gain * (TRANSITION_FRAMES - i) / TRANSITION_FRAMES;
        sTail[i * 2] *= fade; sTail[i * 2 + 1] *= fade;
        if (gSaveBlock2Ptr && gSaveBlock2Ptr->optionsSound == OPTIONS_SOUND_MONO)
            sTail[i * 2] = sTail[i * 2 + 1] = (sTail[i * 2] + sTail[i * 2 + 1]) * 0.5f;
    }
    sTailPosition = 0;
}
void NativeMusicOnStop(struct MusicPlayerInfo *player)
{
    if (player != &gMPlayInfo_BGM) return;
    if (sPreviewGuard) return;
    ++sMusicEpoch;
    if (sLifecycleActive)
    {
        struct RhhMusicLifecycle stopped = sLifecycleCurrent; stopped.cause = RHH_MUSIC_STOPPED;
        sLifecycleActive = 0; memset(&sLifecycleCurrent, 0, sizeof(sLifecycleCurrent));
        QueueLifecycle(&stopped);
    }
    if (sPreview.owner) FinishPreview(FALSE);
    CaptureTail();
    sActive = 0; sAsset = 0; sTrackKey = 0;
    memset(&sPlayback, 0, sizeof(sPlayback));
    if (sGameBoy) gme_delete(sGameBoy);
    sGameBoy = NULL;
    if (sFont) tsf_reset(sFont);
    if (sSong) tml_free(sSong);
    sSong = sNext = NULL;
}
void NativeMusicShutdown(void)
{
    if (sPreview.owner) FinishPreview(FALSE);
    NativeMusicOnStop(&gMPlayInfo_BGM);
    if (sFont) tsf_close(sFont);
    sFont = NULL; sFontGeneration = 0;
    sTailPosition = TRANSITION_FRAMES;
    NativeMusicSetAssetRoot(NULL);
    NativeMusicSetImportRoot(NULL);
    sLifecycleHead = sLifecycleCount = 0; sLifecycleSequence = sLifecycleDropped = 0;
    sLifecycleActive = sLifecycleFlushing = 0; sLifecycleContext = NULL;
    memset(&sLifecycleCurrent, 0, sizeof(sLifecycleCurrent));
    memset(&sBattleCue, 0, sizeof(sBattleCue)); memset(&sBattleCueOverride, 0, sizeof(sBattleCueOverride));
    sBattleCueResult = NULL; sBattleCueFlushing = FALSE;
}
int NativeMusicActive(void) { return sActive; }
int NativeMusicHasAudio(void) { return sPreview.owner || sActive || sTailPosition < TRANSITION_FRAMES; }
uint32_t NativeMusicAsset(void) { return sAsset; }
void NativeMusicLoopInfo(uint64_t *start, uint64_t *end, uint64_t *position)
{
    if (start) *start = sLoopStartSamples;
    if (end) *end = sLoopSamples;
    if (position) *position = sSamples;
}
static int MusicFind(RhhContentKey key, const struct RhhModMusic **out)
{
    if (!out) return -1;
    const struct RhhModMusic *track = RhhModsFindMusic(key);
    if (!track) return -1;
    *out = track; return 0;
}
static RhhContentKey MusicCurrent(void) { return sTrackKey; }
static int MusicGainContext(struct RhhMusicGainContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sGainContext || RhhModsCurrentEvent() != RHH_MUSIC_GAIN) return -1;
    *out = *sGainContext; return 0;
}
static void SyncMusicLevel(void)
{
    if (!sActive || sGainContext || sLoopContext || !sPlayback.track) return;
    u32 level = NativeMusicTrackLevel(sPlayback.track);
    if (level == sAppliedLevel) return;
    sAppliedLevel = level;
    u64 combined = (u64)sProposedGain * level / RHH_MUSIC_GAIN_ONE;
    if (combined > RHH_MUSIC_GAIN_MAX) combined = RHH_MUSIC_GAIN_MAX;
    sPlayback.gain = combined;
    sGainTarget = sGainBase * combined / (float)RHH_MUSIC_GAIN_ONE;
    sGainRamp = sGainTarget == sTrackGain ? 0 : LEVEL_RAMP_FRAMES;
}
static int MusicPlayback(struct RhhMusicPlayback *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sActive || sGainContext || sLoopContext || !sPlayback.track) return -1;
    SyncMusicLevel();
    struct RhhMusicPlayback snapshot = sPlayback;
    snapshot.flags = (gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE ? RHH_MUSIC_PLAYBACK_PAUSED : 0)
        | (sFadeIn ? RHH_MUSIC_PLAYBACK_FADING_IN : 0)
        | (sTailPosition < TRANSITION_FRAMES ? RHH_MUSIC_PLAYBACK_TRANSITION_TAIL : 0)
        | (sGainRamp ? RHH_MUSIC_PLAYBACK_LEVEL_RAMP : 0)
        | (!sGameBoy && sLoop.valid ? RHH_MUSIC_PLAYBACK_EXPLICIT_LOOP : 0);
    snapshot.position = sSamples;
    snapshot.loop_start = sGameBoy ? 0 : sLoopStartSamples;
    snapshot.loop_end = sGameBoy ? 0 : sLoopSamples;
    *out = snapshot; return 0;
}
static int ReadAsset(const char *relative, size_t max, unsigned char **out, size_t *length)
{
    char *path = AssetPath(relative);
    FILE *file = path ? fopen(path, "rb") : NULL;
    free(path);
    if (!file) return RHH_MUSIC_MISSING;
    if (fseek(file, 0, SEEK_END)) { fclose(file); return RHH_MUSIC_INVALID; }
    long size = ftell(file);
    if (size <= 0 || (unsigned long)size > max || fseek(file, 0, SEEK_SET))
    { fclose(file); return RHH_MUSIC_INVALID; }
    unsigned char *data = malloc((size_t)size);
    if (!data) { fclose(file); return RHH_MUSIC_INVALID; }
    int ok = fread(data, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    if (!ok) { free(data); return RHH_MUSIC_INVALID; }
    *out = data; *length = (size_t)size; return RHH_MUSIC_READY;
}
static uint32_t Be32(const unsigned char *p)
{ return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint32_t Le32(const unsigned char *p)
{ return (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0]; }
static int MidiVar(const unsigned char *p, size_t end, size_t *at, uint32_t *value)
{
    uint32_t n = 0;
    for (unsigned i = 0; i < 4 && *at < end; ++i)
    {
        unsigned byte = p[(*at)++]; n = (n << 7) | (byte & 127);
        if (!(byte & 128)) { *value = n; return 1; }
    }
    return 0;
}
static int ValidMidiContainer(const unsigned char *p, size_t n)
{
    if (n < 14 || memcmp(p, "MThd", 4) || Be32(p + 4) != 6 || p[8] || p[9] > 1
        || (p[12] & 0x80) || !(p[12] || p[13])) return 0;
    unsigned tracks = (unsigned)p[10] * 256 + p[11];
    if (!tracks || tracks > 64 || (p[9] == 0 && tracks != 1)) return 0;
    size_t pos = 14; uint32_t maxTicks = 0, maxTempo = 500000, events = 0;
    unsigned division = (unsigned)p[12] * 256 + p[13];
    for (unsigned i = 0; i < tracks; ++i)
    {
        if (n - pos < 8 || memcmp(p + pos, "MTrk", 4)) return 0;
        uint32_t size = Be32(p + pos + 4); pos += 8;
        if (size > n - pos) return 0;
        size_t end = pos + size; uint32_t ticks = 0; unsigned running = 0; int ended = 0;
        while (pos < end)
        {
            uint32_t delta, len;
            if (++events > 100000 || !MidiVar(p, end, &pos, &delta) || delta > 0xfffff
                || delta > UINT32_MAX - ticks || pos == end) return 0;
            ticks += delta;
            unsigned status = p[pos++];
            if (!(status & 128)) { if (!running) return 0; --pos; status = running; }
            else if (status < 0xf0) running = status;
            else running = 0;
            if (status == 0xff)
            {
                if (pos == end) return 0;
                unsigned type = p[pos++];
                if (!MidiVar(p, end, &pos, &len) || len > end - pos) return 0;
                if (type == 0x2f) { if (len || pos != end) return 0; ended = 1; }
                if (type == 0x51)
                {
                    if (len != 3) return 0;
                    uint32_t tempo = (uint32_t)p[pos] << 16 | (uint32_t)p[pos+1] << 8 | p[pos+2];
                    if (!tempo) return 0;
                    if (tempo > maxTempo) maxTempo = tempo;
                }
                pos += len;
            }
            else if (status == 0xf0 || status == 0xf7)
            {
                if (!MidiVar(p, end, &pos, &len) || len > end - pos) return 0;
                pos += len;
            }
            else if (status >= 0x80 && status < 0xf0)
            {
                unsigned bytes = (status & 0xe0) == 0xc0 ? 1 : 2;
                if (bytes > end - pos) return 0;
                while (bytes--) if (p[pos++] & 128) return 0;
            }
            else return 0;
        }
        if (!ended) return 0;
        if (ticks > maxTicks) maxTicks = ticks;
    }
    return pos == n && (double)maxTicks * maxTempo / (1000.0 * division) <= 86400000.0;
}

struct MidiTempo { uint32_t tick, tempo, order; };
static int CompareMidiTempo(const void *left, const void *right)
{
    const struct MidiTempo *a = left, *b = right;
    if (a->tick != b->tick) return a->tick < b->tick ? -1 : 1;
    return a->order < b->order ? -1 : a->order != b->order;
}
static int MidiMarker(const unsigned char *p, uint32_t len, const char *name)
{
    size_t i = 0;
    while (len && (p[len - 1] == ' ' || !p[len - 1])) --len;
    while (i < len && p[i] == ' ') ++i;
    for (; i < len && *name; ++i, ++name)
        if (tolower(p[i]) != *name) return 0;
    return i == len && !*name;
}
/* Read optional loop markers from a validated MIDI. TinyMidiLoader discards
 * text events; tempos are sorted in the same track order as its loader. */
static struct MidiLoop FindMidiLoop(const unsigned char *p, size_t n)
{
    struct MidiLoop result = {0};
    struct MidiTempo *tempos = malloc(2048 * sizeof(*tempos));
    if (!tempos) return result;
    unsigned count = 0, order = 0, tracks = (unsigned)p[10] * 256 + p[11];
    uint32_t start = 0, endTick = 0;
    int hasStart = 0, hasEnd = 0, overflow = 0;
    size_t pos = 14;
    for (unsigned i = 0; i < tracks; ++i)
    {
        size_t end = pos + 8 + Be32(p + pos + 4); pos += 8;
        uint32_t tick = 0; unsigned running = 0;
        while (pos < end)
        {
            uint32_t delta, len; MidiVar(p, end, &pos, &delta); tick += delta;
            unsigned status = p[pos++];
            if (!(status & 128)) { --pos; status = running; }
            else if (status < 0xf0) running = status;
            else running = 0;
            if (status == 0xff)
            {
                unsigned type = p[pos++]; MidiVar(p, end, &pos, &len);
                if (type == 0x51)
                {
                    if (count == 2048) overflow = 1;
                    else tempos[count++] = (struct MidiTempo){tick, (uint32_t)p[pos] << 16 | (uint32_t)p[pos + 1] << 8 | p[pos + 2], order++};
                }
                if (type == 0x01 || type == 0x06)
                {
                    if (MidiMarker(p + pos, len, "start") || MidiMarker(p + pos, len, "loopstart") || MidiMarker(p + pos, len, "loop_start"))
                    { if (!hasStart) start = tick; else if (start != tick) overflow = 1; hasStart = 1; }
                    if (MidiMarker(p + pos, len, "end") || MidiMarker(p + pos, len, "loopend") || MidiMarker(p + pos, len, "loop_end"))
                    { if (!hasEnd) endTick = tick; else if (endTick != tick) overflow = 1; hasEnd = 1; }
                }
                pos += len;
            }
            else if (status == 0xf0 || status == 0xf7) { MidiVar(p, end, &pos, &len); pos += len; }
            else pos += (status & 0xe0) == 0xc0 ? 1 : 2;
        }
    }
    if (!overflow && hasStart && hasEnd && start < endTick && pos == n)
    {
        qsort(tempos, count, sizeof(*tempos), CompareMidiTempo);
        unsigned division = (unsigned)p[12] * 256 + p[13], ms = 0, previous = 0, tempo = 500000, index = 0;
        uint32_t points[] = {start, endTick};
        unsigned *out[] = {&result.start, &result.end};
        for (unsigned i = 0; i < 2; ++i)
        {
            while (index < count && tempos[index].tick <= points[i])
            {
                ms += (unsigned)((double)(tempos[index].tick - previous) * tempo / (1000.0 * division));
                previous = tempos[index].tick; tempo = tempos[index++].tempo;
            }
            *out[i] = ms + (unsigned)((double)(points[i] - previous) * tempo / (1000.0 * division));
        }
        result.valid = result.start < result.end;
    }
    free(tempos);
    return result;
}
static int ValidRiffChunks(const unsigned char *p, size_t n, unsigned depth)
{
    if (depth > 4) return 0;
    size_t pos = 0;
    while (pos < n)
    {
        if (n - pos < 8) return 0;
        uint32_t size = Le32(p + pos + 4); pos += 8;
        if (size > n - pos) return 0;
        if (!memcmp(p + pos - 8, "LIST", 4)
            && (size < 4 || !ValidRiffChunks(p + pos + 4, size - 4, depth + 1))) return 0;
        pos += size;
        if (size & 1) { if (pos == n) return 0; ++pos; }
    }
    return pos == n;
}
static int ParsePair(const unsigned char *midi, size_t midiSize, const unsigned char *sf2, size_t sf2Size,
                     tml_message **song, tsf **font, struct MidiLoop *loop)
{
    if (!midi || !sf2 || midiSize > 4u * 1024u * 1024u || sf2Size > 128u * 1024u * 1024u
        || !ValidMidiContainer(midi, midiSize)) return RHH_MUSIC_INVALID;
    tml_message *parsedSong = tml_load_memory(midi, (int)midiSize);
    if (loop) *loop = FindMidiLoop(midi, midiSize);
    if (!parsedSong) return RHH_MUSIC_INVALID;
    if (sf2Size < 12 || memcmp(sf2, "RIFF", 4) || memcmp(sf2 + 8, "sfbk", 4)
        || Le32(sf2 + 4) != sf2Size - 8 || !ValidRiffChunks(sf2 + 12, sf2Size - 12, 0))
    { tml_free(parsedSong); return RHH_MUSIC_INVALID; }
    tsf *parsedFont = tsf_load_memory(sf2, (int)sf2Size);
    if (!parsedFont || !tsf_get_presetcount(parsedFont))
    { if (parsedFont) tsf_close(parsedFont); tml_free(parsedSong); return RHH_MUSIC_INVALID; }
    *song = parsedSong; *font = parsedFont; return RHH_MUSIC_READY;
}
int NativeMusicInspectImported(const unsigned char *midi, size_t midiSize, const unsigned char *sf2, size_t sf2Size,
                               uint32_t *loopStart, uint32_t *loopEnd)
{
    tml_message *song = NULL; tsf *font = NULL; struct MidiLoop loop = {0};
    int status = ParsePair(midi, midiSize, sf2, sf2Size, &song, &font, &loop);
    if (song) tml_free(song);
    if (font) tsf_close(font);
    if (status == RHH_MUSIC_READY && loopStart && loopEnd)
    { *loopStart = loop.valid ? loop.start : 0; *loopEnd = loop.valid ? loop.end : 0; }
    return status;
}
static int LoadCustomPair(const struct RhhModMusic *track, tml_message **song, tsf **font, struct MidiLoop *loop)
{
    unsigned char *midi = NULL, *sf2 = NULL; size_t midiSize = 0, sf2Size = 0;
    int status = ReadAsset(track->midi_path, 4u * 1024u * 1024u, &midi, &midiSize);
    if (status == RHH_MUSIC_READY) status = ReadAsset(track->soundfont_path, 128u * 1024u * 1024u, &sf2, &sf2Size);
    if (status == RHH_MUSIC_READY) status = ParsePair(midi, midiSize, sf2, sf2Size, song, font, loop);
    free(midi); free(sf2); return status;
}
static int LoadImportedPair(RhhContentKey key, tml_message **song, tsf **font, struct MidiLoop *loop)
{
    unsigned char *midi = NULL, *sf2 = NULL; size_t midiSize = 0, sf2Size = 0;
    int status = NativeMusicImportedRead(key, &midi, &midiSize, &sf2, &sf2Size);
    if (status == RHH_MUSIC_READY) status = ParsePair(midi, midiSize, sf2, sf2Size, song, font, loop);
    free(midi); free(sf2); return status;
}
static int MusicAvailability(RhhContentKey key)
{
    const struct RhhModMusic *track = RhhModsFindMusic(key);
    if (!track && !NativeMusicImportedKnown(key)) return RHH_MUSIC_MISSING;
    tml_message *song = NULL; tsf *font = NULL;
    int status = track ? LoadCustomPair(track, &song, &font, NULL) : LoadImportedPair(key, &song, &font, NULL);
    if (song) tml_free(song);
    if (font) tsf_close(font);
    return status;
}
static void FinishPreview(int restore)
{
    if (!sPreview.owner) return;
    int wasPaused = sPreview.wasPaused;
    tsf_close(sPreview.font); tml_free(sPreview.song); memset(&sPreview, 0, sizeof(sPreview));
    if (restore && !wasPaused) m4aMPlayContinue(&gMPlayInfo_BGM);
}
void NativeMusicClosePreview(uint64_t owner)
{ if (owner && sPreview.owner == owner) FinishPreview(TRUE); }
static int PreviewCurrent(struct RhhMusicPreviewSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    *out = (struct RhhMusicPreviewSnapshot){.struct_size = sizeof(*out), .active = !!sPreview.owner,
        .duration_ms = sPreview.duration, .elapsed_ms = sPreview.elapsed * 1000 / SAMPLE_RATE,
        .owner = sPreview.owner, .track = sPreview.key};
    return 0;
}
static unsigned SongEndMs(const tml_message *song)
{
    unsigned end = 0;
    for (const tml_message *m = song; m; m = m->next) if (m->time > end) end = m->time;
    return end;
}
static int MusicLoopContext(struct RhhMusicLoopContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sLoopContext || RhhModsCurrentEvent() != RHH_MUSIC_LOOP) return -1;
    *out = *sLoopContext; return 0;
}
/* Returns a changed, validated loop or the source loop. These rules do not
 * touch the synthesizer and cannot advance gameplay RNG. */
static struct MidiLoop ResolveMusicLoop(const tml_message *song, struct MidiLoop loop,
                                        struct RhhMusicLoopContext context)
{
    unsigned end = SongEndMs(song);
    if (!loop.valid || (uint64_t)loop.end > (uint64_t)end + 5000) loop = (struct MidiLoop){0};
    if (!RhhModsHasHook(RHH_MUSIC_LOOP) || sLoopContext) return loop;
    context.struct_size = sizeof(context); context.song_end_ms = end;
    context.flags = loop.valid ? RHH_MUSIC_LOOP_EMBEDDED : 0;
    context.start_ms = loop.valid ? loop.start : 0; context.end_ms = loop.valid ? loop.end : end;
    uint64_t original = RHH_MUSIC_LOOP_VALUE(context.start_ms, context.end_ms);
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MUSIC_LOOP,
        .run_seed = RhhModsRunSeed(), .event_key = context.track, .subject = context.original,
        .args = {context.phase, context.context, context.asset, context.flags}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = original};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sLoopContext = &context; RhhModsDispatch(&event, &decision); sLoopContext = NULL;
    int rngChanged = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    uint32_t start = decision.value, finish = decision.value >> 32;
    if (decision.allowed && !rngChanged && decision.value != original && start < finish
        && (uint64_t)finish <= (uint64_t)end + 5000)
        return (struct MidiLoop){start, finish, 1};
    return loop;
}
static int PreviewStartCore(const struct RhhMusicPreviewRequest *request, u32 gain)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->owner || !request->track
        || request->duration_ms < 500 || request->duration_ms > 30000 || gain > RHH_MUSIC_GAIN_MAX
        || !RhhModsRegisteredKey(request->owner)) return RHH_MUSIC_PREVIEW_INVALID;
    if (sPreview.owner) return RHH_MUSIC_PREVIEW_BUSY;
    if (!RhhModsCommandsAllowed() || !RhhModsHasActiveRun() || !RhhUiPanelOwnedBy(request->owner)
        || gMain.inBattle || NativeSaveTransactionActive() || gMPlayInfo_BGM.ident != ID_NUMBER)
        return RHH_MUSIC_PREVIEW_UNAVAILABLE;
    const struct RhhModMusic *track = RhhModsFindMusic(request->track);
    if (!track && !NativeMusicImportedKnown(request->track)) return RHH_MUSIC_PREVIEW_MISSING;
    tml_message *song = NULL; tsf *font = NULL; struct MidiLoop loop = {0};
    int status = track ? LoadCustomPair(track, &song, &font, &loop) : LoadImportedPair(request->track, &song, &font, &loop);
    if (status != RHH_MUSIC_READY)
        return status == RHH_MUSIC_MISSING ? RHH_MUSIC_PREVIEW_MISSING : RHH_MUSIC_PREVIEW_AUDIO_ERROR;
    tsf_set_output(font, TSF_STEREO_INTERLEAVED, SAMPLE_RATE, -12.0f);
    if (!tsf_set_max_voices(font, 256)) { tsf_close(font); tml_free(song); return RHH_MUSIC_PREVIEW_AUDIO_ERROR; }
    uint64_t epoch = sMusicEpoch, run = RhhModsRunId();
    loop = ResolveMusicLoop(song, loop, (struct RhhMusicLoopContext){.phase = RHH_MUSIC_LOOP_PREVIEW,
        .track = request->track, .owner = request->owner});
    if (sMusicEpoch != epoch || sPreview.owner || !RhhModsCommandsAllowed() || !RhhModsHasActiveRun()
        || RhhModsRunId() != run || !RhhUiPanelOwnedBy(request->owner) || gMain.inBattle
        || NativeSaveTransactionActive() || gMPlayInfo_BGM.ident != ID_NUMBER)
    { tsf_close(font); tml_free(song); return RHH_MUSIC_PREVIEW_UNAVAILABLE; }
    unsigned end = loop.valid ? loop.end : SongEndMs(song);
    sPreview.owner = request->owner; sPreview.key = request->track; sPreview.song = song; sPreview.next = song;
    sPreview.font = font; sPreview.loopNext = song; sPreview.loopStart = ((uint64_t)loop.start * SAMPLE_RATE + 999) / 1000;
    sPreview.loopEnd = ((uint64_t)end * SAMPLE_RATE + 999) / 1000 + !loop.valid;
    while (sPreview.loopNext && sPreview.loopNext->time < loop.start) sPreview.loopNext = sPreview.loopNext->next;
    sPreview.duration = request->duration_ms; sPreview.volume = gain; sPreview.explicitLoop = loop.valid;
    sPreview.limit = ((uint64_t)request->duration_ms * SAMPLE_RATE + 999) / 1000;
    sPreview.wasPaused = !!(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE);
    tsf_reset(font); for (unsigned channel = 0; channel < 16; ++channel) SetProgramFor(font, channel, 0);
    /* Silence only the BGM player; keep its native replacement synthesizer
     * and position intact while SFX and cries continue on their players. */
    sPreviewGuard = TRUE; m4aMPlayStop(&gMPlayInfo_BGM); sPreviewGuard = FALSE;
    return RHH_MUSIC_PREVIEW_OK;
}
static int PreviewStart(const struct RhhMusicPreviewRequest *request)
{
    if (!request || request->struct_size < sizeof(*request) || request->volume_percent > 100) return RHH_MUSIC_PREVIEW_INVALID;
    return PreviewStartCore(request, request->volume_percent * 10);
}
static int PreviewStartScaled(const struct RhhMusicScaledPreviewRequest *request)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || request->gain > RHH_MUSIC_GAIN_MAX)
        return RHH_MUSIC_PREVIEW_INVALID;
    struct RhhMusicPreviewRequest converted = {.struct_size = sizeof(converted), .duration_ms = request->duration_ms,
        .owner = request->owner, .track = request->track};
    return PreviewStartCore(&converted, request->gain);
}
static int PreviewStop(RhhContentKey owner)
{
    if (!owner || !RhhModsRegisteredKey(owner)) return RHH_MUSIC_PREVIEW_INVALID;
    if (!RhhModsCommandsAllowed()) return RHH_MUSIC_PREVIEW_UNAVAILABLE;
    if (!sPreview.owner || sPreview.owner != owner) return RHH_MUSIC_PREVIEW_UNAVAILABLE;
    FinishPreview(TRUE); return RHH_MUSIC_PREVIEW_OK;
}
static int PreviewTiming(struct RhhMusicPreviewTiming *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sPreview.owner) return -1;
    *out = (struct RhhMusicPreviewTiming){.struct_size = sizeof(*out), .sample_rate = SAMPLE_RATE,
        .flags = sPreview.explicitLoop ? RHH_MUSIC_PLAYBACK_EXPLICIT_LOOP : 0,
        .position = sPreview.samples, .loop_start = sPreview.loopStart, .loop_end = sPreview.loopEnd};
    return 0;
}
static int MusicInspectTrack(RhhContentKey key, struct RhhMusicTrackInfo *out);
static int MusicInspectSourceTiming(RhhContentKey key, struct RhhMusicSourceTiming *out);
static int MusicInspectInstruments(RhhContentKey key, struct RhhMusicInstrumentInspection *out);
static int MusicInspectNoteCoverage(RhhContentKey key, struct RhhMusicNoteCoverage *out);
static const struct RhhMusicPreviewService sPreviewService = {
    {sizeof(sPreviewService), RHH_SERVICE_VERSION(1, 2)}, PreviewStart, PreviewStop, PreviewCurrent, PreviewStartScaled, PreviewTiming
};
static const struct RhhMusicService sMusicService = {
    {sizeof(sMusicService), RHH_SERVICE_VERSION(1, 8)}, RhhModsMusicCount, RhhModsMusicAt,
    MusicFind, MusicAvailability, MusicCurrent, MusicGainContext, MusicPlayback,
    MusicLifecycleState, MusicLifecycleContext, MusicLoopContext, MusicInspectTrack, MusicInspectSourceTiming,
    QueueBattleCue, BattleCueResult, MusicInspectInstruments, MusicInspectNoteCoverage
};
int NativeMusicInstallService(void)
{ return RhhModsSetLifecycleCallback(DiscardBattleCue)
    || RhhModsAddService("rhh:service/music", &sMusicService.header)
    || RhhModsAddService("rhh:service/music_preview", &sPreviewService.header)
    || NativeMusicInstallAssignments() || NativeMusicInstallImports() ? -1 : 0; }
void NativeMusicOnVolume(struct MusicPlayerInfo *player, unsigned volume)
{
    if (player == &gMPlayInfo_BGM) sVolume = volume / 256.0f;
}
static void ConfigureSongLoop(void)
{
    unsigned end = SongEndMs(sSong);
    /* Exporters may put the final note-off a tick before the loop marker. */
    if (!sLoop.valid || (uint64_t)sLoop.end > (uint64_t)end + 5000) sLoop = (struct MidiLoop){0};
    sLoopStartSamples = ((uint64_t)sLoop.start * SAMPLE_RATE + 999) / 1000;
    if (sLoop.valid) end = sLoop.end;
    sLoopSamples = ((uint64_t)end * SAMPLE_RATE + 999) / 1000 + !sLoop.valid;
    sLoopNext = sSong;
    while (sLoopNext && sLoopNext->time < sLoop.start) sLoopNext = sLoopNext->next;
}
static void StartSong(RhhContentKey key, unsigned asset)
{
    ConfigureSongLoop();
    sSamples = 0; sNext = sSong; sAsset = asset; sTrackKey = key; sVolume = 1; sActive = 1;
    sTrackGain = asset >= 1 && asset <= 20 ? sTrackGains[asset - 1] * 0.48f : 1;
    sFadeIn = TRANSITION_FRAMES;
    ResetChannels();
}
static int LoadAsset(unsigned asset, RhhContentKey key)
{
    unsigned generation = (asset - 1) / 4 + 1, role = (asset - 1) % 4;
    unsigned fontGeneration = generation < 3 ? 12 : generation == 5 ? 51 + role : generation;
    if (!sFont || sFontGeneration != fontGeneration)
    {
        char relative[64];
        if (generation == 4) snprintf(relative, sizeof(relative), "fonts/gen4-native.sf2");
        else if (generation == 5) snprintf(relative, sizeof(relative), "fonts/gen5-%s.sf2", sRoles[role]);
        else snprintf(relative, sizeof(relative), "fonts/gen%u.sf2", fontGeneration);
        char *path = AssetPath(relative);
        tsf *font = path ? tsf_load_filename(path) : NULL;
        free(path);
        if (!font) { fprintf(stderr, "Battle SoundFont unavailable: gen%u\n", fontGeneration); return -1; }
        tsf_set_output(font, TSF_STEREO_INTERLEAVED, SAMPLE_RATE, -12.0f);
        if (!tsf_set_max_voices(font, 256)) { tsf_close(font); return -1; }
        if (sFont) tsf_close(sFont);
        sFont = font; sFontGeneration = fontGeneration;
    }
    char relative[64]; snprintf(relative, sizeof(relative), "gen%u/%s.mid", generation, sRoles[role]);
    unsigned char *midi = NULL; size_t midiSize = 0;
    sSong = NULL; sLoop = (struct MidiLoop){0};
    if (ReadAsset(relative, 4u * 1024u * 1024u, &midi, &midiSize) == RHH_MUSIC_READY && ValidMidiContainer(midi, midiSize))
    {
        sSong = tml_load_memory(midi, (int)midiSize);
        sLoop = FindMidiLoop(midi, midiSize);
    }
    free(midi);
    if (!sSong) { fprintf(stderr, "Battle MIDI unavailable: %s\n", relative); return -1; }
    StartSong(key, asset);
    return 0;
}
static int OpenGameBoy(unsigned asset, Music_Emu **out)
{
    static const unsigned tracks[2][4] = {{23, 22, 21, 24}, {40, 41, 45, 46}};
    unsigned generation = (asset - 1) / 4, role = (asset - 1) % 4;
    char relative[64]; snprintf(relative, sizeof(relative), "gameboy/gen%u.gbs", generation + 1);
    unsigned char *data = NULL; size_t size = 0;
    int status = ReadAsset(relative, 1024u * 1024u, &data, &size);
    if (status != RHH_MUSIC_READY) return status;
    if (size < 112 || memcmp(data, "GBS\1", 4) || data[4] <= tracks[generation][role])
    { free(data); return RHH_MUSIC_INVALID; }
    Music_Emu *player = NULL;
    const char *error = gme_open_data(data, (long)size, &player, SAMPLE_RATE);
    free(data);
    if (error || !player) { if (player) gme_delete(player); return RHH_MUSIC_INVALID; }
    /* The music code performs its own seamless loop; do not impose a playlist
     * time limit, silence skipping, automatic fade, or end-of-track detection. */
    gme_ignore_silence(player, 1);
    gme_set_autoload_playback_limit(player, 0);
    error = gme_start_track(player, tracks[generation][role]);
    if (error) { gme_delete(player); return RHH_MUSIC_INVALID; }
    *out = player; return RHH_MUSIC_READY;
}
static int LoadGameBoy(unsigned asset, RhhContentKey key)
{
    Music_Emu *player = NULL;
    if (OpenGameBoy(asset, &player) != RHH_MUSIC_READY) return -1;
    sGameBoy = player; sSong = sNext = sLoopNext = NULL;
    sLoop = (struct MidiLoop){0}; sSamples = sLoopStartSamples = sLoopSamples = 0;
    sAsset = asset; sTrackKey = key; sVolume = 1; sTrackGain = sGameBoyGains[asset - 1];
    sFadeIn = TRANSITION_FRAMES; sActive = 1;
    return 0;
}
static int InspectBundledMidi(unsigned asset, tml_message **songOut, struct MidiLoop *loopOut, tsf **fontOut)
{
    unsigned generation = (asset - 1) / 4 + 1, role = (asset - 1) % 4;
    char relative[64];
    if (generation == 4) snprintf(relative, sizeof(relative), "fonts/gen4-native.sf2");
    else snprintf(relative, sizeof(relative), "fonts/gen5-%s.sf2", sRoles[role]);
    char *path = AssetPath(relative);
    FILE *file = path ? fopen(path, "rb") : NULL;
    if (!file) { free(path); return RHH_MUSIC_MISSING; }
    fclose(file);
    tsf *font = tsf_load_filename(path); free(path);
    if (!font || !tsf_get_presetcount(font)) { if (font) tsf_close(font); return RHH_MUSIC_INVALID; }
    tsf_set_output(font, TSF_STEREO_INTERLEAVED, SAMPLE_RATE, -12.0f);
    if (!tsf_set_max_voices(font, 256)) { tsf_close(font); return RHH_MUSIC_INVALID; }
    snprintf(relative, sizeof(relative), "gen%u/%s.mid", generation, sRoles[role]);
    unsigned char *midi = NULL; size_t size = 0;
    int status = ReadAsset(relative, 4u * 1024u * 1024u, &midi, &size);
    if (status == RHH_MUSIC_READY)
    {
        tml_message *song = ValidMidiContainer(midi, size) ? tml_load_memory(midi, (int)size) : NULL;
        if (!song) status = RHH_MUSIC_INVALID;
        else {
            if (loopOut) *loopOut = FindMidiLoop(midi, size);
            if (songOut) *songOut = song;
            else tml_free(song);
        }
    }
    free(midi);
    if (status == RHH_MUSIC_READY && fontOut) *fontOut = font;
    else tsf_close(font);
    return status;
}
static int MusicInspectTrack(RhhContentKey key, struct RhhMusicTrackInfo *out)
{
    if (!key || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhMusicTrackInfo result = {.struct_size = sizeof(result), .track = key, .availability = RHH_MUSIC_MISSING};
    const struct RhhModMusic *declared = RhhModsFindMusic(key);
    if (declared || NativeMusicImportedKnown(key))
    {
        result.kind = declared ? RHH_MUSIC_TRACK_DECLARED : RHH_MUSIC_TRACK_IMPORTED;
        result.backend = RHH_MUSIC_BACKEND_MIDI; result.availability = MusicAvailability(key);
    }
    else
    {
        u32 asset;
        if (!RhhContentResolve(RHH_MIDI, key, &asset) && asset >= 1 && asset <= 20)
        {
            result.kind = RHH_MUSIC_TRACK_BUNDLED; result.asset = asset;
            if (asset <= 8)
            {
                Music_Emu *player = NULL; result.backend = RHH_MUSIC_BACKEND_GAMEBOY;
                result.availability = OpenGameBoy(asset, &player);
                if (player) gme_delete(player);
            }
            else if (asset <= 12) { result.backend = RHH_MUSIC_BACKEND_MP2K; result.availability = RHH_MUSIC_READY; }
            else { result.backend = RHH_MUSIC_BACKEND_MIDI; result.availability = InspectBundledMidi(asset, NULL, NULL, NULL); }
        }
    }
    *out = result; return 0;
}
static int MusicInspectSourceTiming(RhhContentKey key, struct RhhMusicSourceTiming *out)
{
    if (!key || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhMusicSourceTiming result = {.struct_size = sizeof(result), .track = key, .availability = RHH_MUSIC_MISSING};
    tml_message *song = NULL; tsf *font = NULL; struct MidiLoop loop = {0};
    const struct RhhModMusic *declared = RhhModsFindMusic(key);
    if (declared || NativeMusicImportedKnown(key))
    {
        result.kind = declared ? RHH_MUSIC_TRACK_DECLARED : RHH_MUSIC_TRACK_IMPORTED;
        result.backend = RHH_MUSIC_BACKEND_MIDI;
        result.availability = declared ? LoadCustomPair(declared, &song, &font, &loop)
                                       : LoadImportedPair(key, &song, &font, &loop);
    }
    else
    {
        u32 asset;
        if (!RhhContentResolve(RHH_MIDI, key, &asset) && asset >= 1 && asset <= 20)
        {
            result.kind = RHH_MUSIC_TRACK_BUNDLED; result.asset = asset;
            if (asset <= 8) {
                Music_Emu *player = NULL; result.backend = RHH_MUSIC_BACKEND_GAMEBOY;
                result.availability = OpenGameBoy(asset, &player);
                if (player) gme_delete(player);
            } else if (asset <= 12) {
                result.backend = RHH_MUSIC_BACKEND_MP2K; result.availability = RHH_MUSIC_READY;
            } else {
                result.backend = RHH_MUSIC_BACKEND_MIDI;
                result.availability = InspectBundledMidi(asset, &song, &loop, NULL);
            }
        }
    }
    if (result.availability == RHH_MUSIC_READY && song)
    {
        result.flags = RHH_MUSIC_SOURCE_MIDI_TIMING; result.last_event_ms = SongEndMs(song);
        if (loop.valid && (u64)loop.end <= (u64)result.last_event_ms + 5000)
        {
            result.flags |= RHH_MUSIC_SOURCE_EMBEDDED_LOOP;
            result.loop_start_ms = loop.start; result.loop_end_ms = loop.end;
        }
        else result.loop_end_ms = result.last_event_ms;
    }
    else if (result.availability == RHH_MUSIC_READY && (result.backend == RHH_MUSIC_BACKEND_GAMEBOY || result.backend == RHH_MUSIC_BACKEND_MP2K))
        result.flags = RHH_MUSIC_SOURCE_ENGINE_LOOP;
    if (song) tml_free(song);
    if (font) tsf_close(font);
    *out = result; return 0;
}
static int MusicInspectSourceNotes(RhhContentKey key, struct RhhMusicInstrumentInspection *instrumentOut,
                                   struct RhhMusicNoteCoverage *coverageOut)
{
    if (!key || (!!instrumentOut == !!coverageOut)
        || (instrumentOut && instrumentOut->struct_size < sizeof(*instrumentOut))
        || (coverageOut && coverageOut->struct_size < sizeof(*coverageOut))) return -1;
    struct RhhMusicInstrumentInspection result = {.struct_size = sizeof(result), .track = key,
        .availability = RHH_MUSIC_MISSING};
    struct RhhMusicNoteCoverage coverage = {.struct_size = sizeof(coverage), .track = key};
    tml_message *song = NULL; tsf *font = NULL;
    const struct RhhModMusic *declared = RhhModsFindMusic(key);
    if (declared || NativeMusicImportedKnown(key))
    {
        result.kind = declared ? RHH_MUSIC_TRACK_DECLARED : RHH_MUSIC_TRACK_IMPORTED;
        result.backend = RHH_MUSIC_BACKEND_MIDI;
        result.availability = declared ? LoadCustomPair(declared, &song, &font, NULL)
                                       : LoadImportedPair(key, &song, &font, NULL);
    }
    else
    {
        u32 asset;
        if (!RhhContentResolve(RHH_MIDI, key, &asset) && asset >= 1 && asset <= 20)
        {
            result.kind = RHH_MUSIC_TRACK_BUNDLED; result.asset = asset;
            if (asset <= 8)
            {
                Music_Emu *player = NULL; result.backend = RHH_MUSIC_BACKEND_GAMEBOY;
                result.availability = OpenGameBoy(asset, &player);
                if (player) gme_delete(player);
            }
            else if (asset <= 12)
            { result.backend = RHH_MUSIC_BACKEND_MP2K; result.availability = RHH_MUSIC_READY; }
            else
            {
                result.backend = RHH_MUSIC_BACKEND_MIDI;
                result.availability = InspectBundledMidi(asset, &song, NULL, &font);
            }
        }
    }
    if (result.availability == RHH_MUSIC_READY && song && font)
    {
        u32 requestedBank[16], requestedProgram[16] = {0};
        result.flags = RHH_MUSIC_INSTRUMENT_MIDI;
        coverage.flags = RHH_MUSIC_COVERAGE_MIDI;
        for (u32 channel = 0; channel < 16; ++channel)
        {
            if (!tsf_channel_set_presetindex(font, channel, 0)) { tml_free(song); tsf_close(font); return -1; }
            requestedBank[channel] = channel == 9 ? 128 : 0;
            SetProgramFor(font, channel, 0);
        }
        for (const tml_message *m = song; m; m = m->next)
        {
            u32 channel = m->channel;
            if (channel >= 16) continue;
            if (m->type == TML_CONTROL_CHANGE)
            {
                u32 control = (u8)m->control;
                if (control == TML_BANK_SELECT_MSB || control == TML_BANK_SELECT_LSB || control == TML_ALL_CTRL_OFF)
                    tsf_channel_midi_control(font, channel, control, (u8)m->control_value);
            }
            else if (m->type == TML_PROGRAM_CHANGE)
            {
                requestedProgram[channel] = (u8)m->program;
                requestedBank[channel] = tsf_channel_get_preset_bank(font, channel) | (channel == 9 ? 128 : 0);
                SetProgramFor(font, channel, requestedProgram[channel]);
            }
            else if (m->type == TML_NOTE_ON && (u8)m->velocity)
            {
                int index = tsf_channel_get_preset_index(font, channel);
                if (index < 0 || index >= tsf_get_presetcount(font)) { tml_free(song); tsf_close(font); return -1; }
                u32 bank = font->presets[index].bank, program = font->presets[index].preset;
                bool32 fallback = bank != requestedBank[channel] || program != requestedProgram[channel];
                if (coverageOut)
                {
                    const struct tsf_preset *preset = &font->presets[index];
                    u32 note = (u8)m->key, velocity = (u8)m->velocity;
                    short synthVelocity = (short)((velocity / 127.0f) * 127);
                    bool32 covered = FALSE;
                    for (int r = 0; r < preset->regionNum; ++r)
                    {
                        const struct tsf_region *region = &preset->regions[r];
                        if (note >= region->lokey && note <= region->hikey
                            && synthVelocity >= region->lovel && synthVelocity <= region->hivel)
                        { covered = TRUE; break; }
                    }
                    ++coverage.total_notes;
                    if (covered) ++coverage.covered_notes;
                    else
                    {
                        ++coverage.uncovered_notes;
                        u32 i;
                        for (i = 0; i < coverage.count; ++i)
                        {
                            const struct RhhMusicNoteGap *gap = &coverage.gaps[i];
                            if (gap->channel == channel && gap->key == note && gap->velocity == velocity
                                && gap->requested_bank == requestedBank[channel] && gap->requested_program == requestedProgram[channel]
                                && gap->selected_bank == bank && gap->selected_program == program) break;
                        }
                        if (i == coverage.count)
                        {
                            if (coverage.count == RHH_MUSIC_NOTE_GAP_LIMIT)
                            { coverage.flags |= RHH_MUSIC_COVERAGE_TRUNCATED; ++coverage.unlisted_uncovered_notes; continue; }
                            coverage.gaps[coverage.count++] = (struct RhhMusicNoteGap){.channel = channel, .key = note,
                                .velocity = velocity, .requested_bank = requestedBank[channel],
                                .requested_program = requestedProgram[channel], .selected_bank = bank,
                                .selected_program = program, .first_note_ms = m->time};
                        }
                        ++coverage.gaps[i].notes;
                    }
                    continue;
                }
                ++result.total_notes; result.fallback_notes += fallback;
                u32 i;
                for (i = 0; i < result.count; ++i)
                {
                    const struct RhhMusicInstrumentUse *use = &result.uses[i];
                    if (use->channel == channel && use->requested_bank == requestedBank[channel]
                        && use->requested_program == requestedProgram[channel]
                        && use->selected_bank == bank && use->selected_program == program) break;
                }
                if (i == result.count)
                {
                    if (result.count == RHH_MUSIC_INSTRUMENT_LIMIT)
                    { result.flags |= RHH_MUSIC_INSTRUMENT_TRUNCATED; ++result.unlisted_notes; continue; }
                    struct RhhMusicInstrumentUse *use = &result.uses[result.count++];
                    *use = (struct RhhMusicInstrumentUse){.channel = channel, .requested_bank = requestedBank[channel],
                        .requested_program = requestedProgram[channel], .selected_bank = bank, .selected_program = program,
                        .flags = fallback ? RHH_MUSIC_INSTRUMENT_FALLBACK : 0, .first_note_ms = m->time};
                    const char *name = tsf_get_presetname(font, index);
                    if (name) { memcpy(use->preset_name, name, 20); use->preset_name[20] = '\0'; }
                }
                ++result.uses[i].notes;
            }
        }
    }
    if (song) tml_free(song);
    if (font) tsf_close(font);
    if (instrumentOut) *instrumentOut = result;
    else
    {
        coverage.availability = result.availability; coverage.kind = result.kind;
        coverage.backend = result.backend; coverage.asset = result.asset;
        *coverageOut = coverage;
    }
    return 0;
}
static int MusicInspectInstruments(RhhContentKey key, struct RhhMusicInstrumentInspection *out)
{ return out ? MusicInspectSourceNotes(key, out, NULL) : -1; }
static int MusicInspectNoteCoverage(RhhContentKey key, struct RhhMusicNoteCoverage *out)
{ return out ? MusicInspectSourceNotes(key, NULL, out) : -1; }
static int LoadCustom(RhhContentKey key, const struct RhhModMusic *track)
{
    tml_message *song = NULL; tsf *font = NULL;
    struct MidiLoop loop = {0};
    int status = track ? LoadCustomPair(track, &song, &font, &loop) : LoadImportedPair(key, &song, &font, &loop);
    if (status != RHH_MUSIC_READY) return -1;
    tsf_set_output(font, TSF_STEREO_INTERLEAVED, SAMPLE_RATE, -12.0f);
    if (!tsf_set_max_voices(font, 256)) { tsf_close(font); tml_free(song); return -1; }
    if (sFont) tsf_close(sFont);
    sFont = font; sFontGeneration = 0; sSong = song; sLoop = loop;
    StartSong(key, 0);
    return 0;
}
/* Selection is presentation-only. Discard impure proposals while retaining
 * denials, and never let a music hook advance either gameplay RNG stream. */
static void DispatchMusicSelection(const struct RhhModEvent *event, struct RhhModDecision *decision)
{
    struct RhhModDecision before = *decision;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    RhhModsDispatch(event, decision);
    int changed = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (changed)
    {
        if (!decision->allowed) { before.allowed = 0; memcpy(before.reason, decision->reason, sizeof(before.reason)); }
        *decision = before;
    }
}
void NativeMusicOnStart(struct MusicPlayerInfo *player, struct SongHeader *header)
{
    if (player != &gMPlayInfo_BGM) return;
    if (sNativeRedirect) return;
    bool32 hasCue = sBattleCueOverride.armed && sBattleCueOverride.header == header;
    RhhContentKey cueTrack = hasCue ? sBattleCueOverride.track : 0;
    if (hasCue) sBattleCueOverride.armed = FALSE;
    NativeMusicOnStop(player);
    uint64_t startEpoch = sMusicEpoch;
    struct ModConfig config = ModsGetConfig();
    struct RhhModDecision d = {.struct_size = sizeof(d), .allowed = 1};
    RhhContentKey original = 0;
    unsigned context = RHH_MUSIC_OTHER;
    for (unsigned i = 0; i < ARRAY_COUNT(sBattleSongs); ++i)
    {
        u16 song = sBattleSongs[i].song;
        if (gSongTable[song].header != header) continue;
        original = RhhContentKeyForId(RHH_MUSIC, song);
        context = RHH_MUSIC_WILD + sBattleSongs[i].role;
        struct RhhModEvent e = {.struct_size = sizeof(e), .kind = RHH_BATTLE_MUSIC,
            .run_seed = config.seed, .event_key = original, .args = {sBattleSongs[i].role}};
        DispatchMusicSelection(&e, &d);
        if (sMusicEpoch != startEpoch) return;
        break;
    }
    if (!original)
        for (unsigned i = 0; i < RhhContentCount(RHH_MUSIC); ++i)
        {
            struct RhhContentInfo info = {.struct_size = sizeof(info)};
            uint32_t id;
            if (!RhhContentAt(RHH_MUSIC, i, &info) && !RhhContentResolve(RHH_MUSIC, info.key, &id)
                && gSongTable[id].header == header) { original = info.canonical_key; break; }
        }
    RhhContentKey map = gSaveBlock1Ptr ? RhhContentKeyForId(RHH_MAP,
        (gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum) : 0;
    RhhContentKey trainerA = 0, trainerB = 0;
    if (context >= RHH_MUSIC_TRAINER && context <= RHH_MUSIC_CHAMPION && !gIsDebugBattle
        && (gBattleTypeFlags & BATTLE_TYPE_TRAINER) && !(gBattleTypeFlags & (BATTLE_TYPE_LINK
            | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK | BATTLE_TYPE_FRONTIER
            | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_EREADER_TRAINER | BATTLE_TYPE_SECRET_BASE
            | BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_CATCH_TUTORIAL)))
    {
        trainerA = RhhContentKeyForId(RHH_TRAINER, TRAINER_BATTLE_PARAM.opponentA);
        if (gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS)
            trainerB = RhhContentKeyForId(RHH_TRAINER, TRAINER_BATTLE_PARAM.opponentB);
    }
    RhhContentKey assigned = NativeMusicAssignedTrack(context, original, map, trainerA, trainerB);
    if (assigned) d.value = assigned;
    if (hasCue && d.allowed) d.value = cueTrack;
    struct RhhModEvent e = {.struct_size = sizeof(e), .kind = RHH_MUSIC_SELECT,
        .run_seed = config.seed, .event_key = original, .subject = map, .args = {context}};
    DispatchMusicSelection(&e, &d);
    if (sMusicEpoch != startEpoch) return;
    if (!d.allowed || !d.value)
    {
        StartLifecycle(RHH_MUSIC_BACKEND_MP2K, context, 0, 0, original, 0, original, map, trainerA, trainerB);
        return;
    }
    const struct RhhModMusic *custom = RhhModsFindMusic(d.value);
    int imported = !custom && NativeMusicImportedKnown(d.value);
    uint32_t asset;
    int resolved = !custom && !imported && !RhhContentResolve(RHH_MIDI, d.value, &asset) && asset >= 1 && asset <= 20;
    if (resolved && asset >= 9 && asset <= 12)
    {
        static const u16 songs[] = {MUS_VS_WILD, MUS_VS_TRAINER, MUS_VS_GYM_LEADER, MUS_VS_CHAMPION};
        /* The original Emerald sequences and voicegroups are already part of
         * the game. Let MP2K play them with their native samples and loops. */
        sNativeRedirect = 1;
        MPlayStart(player, gSongTable[songs[asset - 9]].header);
        sNativeRedirect = 0;
        sAsset = asset; sTrackKey = d.value;
        StartLifecycle(RHH_MUSIC_BACKEND_MP2K, context, 0, asset, d.value, d.value, original, map, trainerA, trainerB);
        return;
    }
    int loaded = custom || imported ? !LoadCustom(d.value, custom)
        : resolved && !(asset <= 8 ? LoadGameBoy(asset, d.value) : LoadAsset(asset, d.value));
    if (loaded)
    {
        /* Retain the engine's fade/status logic and original SFX players. */
        player->status = 1;
        player->fadeOV = 64 << FADE_VOL_SHIFT;
        sPlayback = (struct RhhMusicPlayback){.struct_size = sizeof(sPlayback),
            .backend = sGameBoy ? RHH_MUSIC_BACKEND_GAMEBOY : RHH_MUSIC_BACKEND_MIDI,
            .context = context, .gain = RHH_MUSIC_GAIN_ONE, .sample_rate = SAMPLE_RATE, .asset = sAsset,
            .track = d.value, .original = original, .map = map, .trainers = {trainerA, trainerB}};
        uint64_t epoch = sMusicEpoch;
        if (!sGameBoy)
        {
            struct MidiLoop resolved = ResolveMusicLoop(sSong, sLoop, (struct RhhMusicLoopContext){.phase = RHH_MUSIC_LOOP_PLAYBACK,
                .context = context, .asset = sAsset, .track = d.value, .original = original, .map = map,
                .trainers = {trainerA, trainerB}});
            if (sMusicEpoch != epoch || !sActive) return;
            sLoop = resolved; ConfigureSongLoop();
        }
        u32 proposedGain = RHH_MUSIC_GAIN_ONE;
        if (!sGainContext && RhhModsHasHook(RHH_MUSIC_GAIN))
        {
            struct RhhMusicGainContext gainContext = {.struct_size = sizeof(gainContext), .backend = sPlayback.backend,
                .context = context, .asset = sAsset, .track = d.value, .original = original, .map = map,
                .trainers = {trainerA, trainerB}};
            struct RhhModEvent gainEvent = {.struct_size = sizeof(gainEvent), .kind = RHH_MUSIC_GAIN,
                .run_seed = config.seed, .event_key = d.value, .subject = original, .args = {context, sPlayback.backend, sAsset}};
            struct RhhModDecision gain = {.struct_size = sizeof(gain), .allowed = 1, .value = RHH_MUSIC_GAIN_ONE};
            rng_value_t rng = gRngValue, rng2 = gRng2Value;
            sGainContext = &gainContext; RhhModsDispatch(&gainEvent, &gain); sGainContext = NULL;
            int rngChanged = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
            gRngValue = rng; gRng2Value = rng2;
            if (gain.allowed && gain.value <= RHH_MUSIC_GAIN_MAX && !rngChanged) proposedGain = gain.value;
        }
        if (sActive && epoch == sMusicEpoch)
        {
            sGainBase = sTrackGain; sProposedGain = proposedGain; sAppliedLevel = NativeMusicTrackLevel(d.value);
            u64 combined = (u64)proposedGain * sAppliedLevel / RHH_MUSIC_GAIN_ONE;
            if (combined > RHH_MUSIC_GAIN_MAX) combined = RHH_MUSIC_GAIN_MAX;
            sPlayback.gain = combined; sGainTarget = sTrackGain = sGainBase * combined / (float)RHH_MUSIC_GAIN_ONE;
            sGainRamp = 0;
            StartLifecycle(sPlayback.backend, context, 0, sAsset, d.value, d.value, original, map, trainerA, trainerB);
        }
    }
    else if (sMusicEpoch == startEpoch)
        StartLifecycle(RHH_MUSIC_BACKEND_MP2K, context, RHH_MUSIC_FALLBACK, 0,
                       original, d.value, original, map, trainerA, trainerB);
}
static void MessageFor(tsf *font, const tml_message *m)
{
    unsigned channel = m->channel;
    switch (m->type)
    {
    case TML_NOTE_ON:
        if (m->velocity) tsf_channel_note_on(font, channel, (unsigned char)m->key, (unsigned char)m->velocity / 127.0f);
        else tsf_channel_note_off(font, channel, (unsigned char)m->key);
        break;
    case TML_NOTE_OFF: tsf_channel_note_off(font, channel, (unsigned char)m->key); break;
    case TML_PROGRAM_CHANGE:
        SetProgramFor(font, channel, (unsigned char)m->program);
        break;
    case TML_PITCH_BEND: tsf_channel_set_pitchwheel(font, channel, m->pitch_bend); break;
    case TML_CONTROL_CHANGE: tsf_channel_midi_control(font, channel, (unsigned char)m->control, (unsigned char)m->control_value); break;
    }
}
static void MixPreview(float *samples, unsigned frames)
{
    float buffer[RENDER_BLOCK * 2]; unsigned at = 0;
    while (at < frames && sPreview.owner)
    {
        if (sPreview.elapsed >= sPreview.limit) { FinishPreview(TRUE); break; }
        if (sPreview.samples >= sPreview.loopEnd)
        {
            for (tml_message *m = sPreview.song; m != sPreview.loopNext; m = m->next)
                if (m->type == TML_PROGRAM_CHANGE || m->type == TML_PITCH_BEND
                    || (m->type == TML_CONTROL_CHANGE && (unsigned char)m->control < 120)) MessageFor(sPreview.font, m);
            sPreview.samples = sPreview.loopStart; sPreview.next = sPreview.loopNext;
        }
        while (sPreview.next && ((uint64_t)sPreview.next->time * SAMPLE_RATE + 999) / 1000 <= sPreview.samples)
        { MessageFor(sPreview.font, sPreview.next); sPreview.next = sPreview.next->next; }
        unsigned count = frames - at;
        if (count > RENDER_BLOCK) count = RENDER_BLOCK;
        uint64_t until = sPreview.next ? ((uint64_t)sPreview.next->time * SAMPLE_RATE + 999) / 1000 : sPreview.loopEnd;
        if (until > sPreview.loopEnd) until = sPreview.loopEnd;
        if (until > sPreview.samples && until - sPreview.samples < count) count = until - sPreview.samples;
        if (sPreview.limit - sPreview.elapsed < count) count = sPreview.limit - sPreview.elapsed;
        if (!count) continue;
        tsf_render_float(sPreview.font, buffer, count, 0);
        float gain = sPreview.volume / (float)RHH_MUSIC_GAIN_ONE;
        for (unsigned i = 0; i < count; ++i)
        {
            float l = buffer[i*2], r = buffer[i*2+1];
            if (gSaveBlock2Ptr && gSaveBlock2Ptr->optionsSound == OPTIONS_SOUND_MONO) l = r = (l + r) * 0.5f;
            samples[(at+i)*2] += l * gain; samples[(at+i)*2+1] += r * gain;
        }
        sPreview.samples += count; sPreview.elapsed += count; at += count;
    }
    if (sPreview.owner && sPreview.elapsed >= sPreview.limit) FinishPreview(TRUE);
}
void NativeMusicMix(float *samples, unsigned frames)
{
    SyncMusicLevel();
    if (sPreview.owner) { MixPreview(samples, frames); return; }
    if (!sActive || (!sGameBoy && (!sFont || !sSong)) || (gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE))
    {
        for (unsigned i = 0; i < frames && sTailPosition < TRANSITION_FRAMES; ++i, ++sTailPosition)
        {
            samples[i * 2] += sTail[sTailPosition * 2];
            samples[i * 2 + 1] += sTail[sTailPosition * 2 + 1];
        }
        return;
    }
    float buffer[RENDER_BLOCK * 2];
    unsigned at = 0;
    while (at < frames)
    {
        if (!sGameBoy && sSamples >= sLoopSamples)
        {
            /* Keep release tails alive. Restore only the channel setup from
             * the intro; notes before the loop point must not play again. */
            for (tml_message *m = sSong; m != sLoopNext; m = m->next)
                if (m->type == TML_PROGRAM_CHANGE || m->type == TML_PITCH_BEND
                    || (m->type == TML_CONTROL_CHANGE && (unsigned char)m->control < 120)) MessageFor(sFont, m);
            sSamples = sLoopStartSamples; sNext = sLoopNext;
        }
        while (!sGameBoy && sNext && ((uint64_t)sNext->time * SAMPLE_RATE + 999) / 1000 <= sSamples)
        { MessageFor(sFont, sNext); sNext = sNext->next; }
        unsigned count = frames - at;
        if (count > RENDER_BLOCK) count = RENDER_BLOCK;
        if (!sGameBoy)
        {
            uint64_t until = sNext ? ((uint64_t)sNext->time * SAMPLE_RATE + 999) / 1000 : sLoopSamples;
            if (until > sLoopSamples) until = sLoopSamples;
            if (until > sSamples && until - sSamples < count) count = until - sSamples;
        }
        if (!count) continue;
        RenderSource(buffer, count);
        if (gSaveBlock2Ptr && gSaveBlock2Ptr->optionsSound == OPTIONS_SOUND_MONO)
            for (unsigned i = 0; i < count; ++i) buffer[i * 2] = buffer[i * 2 + 1] = (buffer[i * 2] + buffer[i * 2 + 1]) * 0.5f;
        for (unsigned i = 0; i < count; ++i)
        {
            if (sGainRamp)
            {
                sTrackGain += (sGainTarget - sTrackGain) / sGainRamp;
                if (!--sGainRamp) sTrackGain = sGainTarget;
            }
            float gain = sVolume * sTrackGain * (gMPlayInfo_BGM.fadeOV >> FADE_VOL_SHIFT) / 64.0f;
            float fade = sFadeIn ? (float)(TRANSITION_FRAMES - sFadeIn--) / TRANSITION_FRAMES : 1;
            samples[(at + i) * 2] += buffer[i * 2] * gain * fade;
            samples[(at + i) * 2 + 1] += buffer[i * 2 + 1] * gain * fade;
            if (sTailPosition < TRANSITION_FRAMES)
            {
                samples[(at + i) * 2] += sTail[sTailPosition * 2];
                samples[(at + i) * 2 + 1] += sTail[sTailPosition * 2 + 1];
                ++sTailPosition;
            }
        }
        sSamples += count; at += count;
    }
}
