/* The original MP2K players continue to own cries and sound effects. Only
 * battle BGM is replaced; timing follows generated samples, not wall time. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define TSF_IMPLEMENTATION
#include "third_party/tinysoundfont/tsf.h"
#define TML_IMPLEMENTATION
#include "third_party/tinysoundfont/tml.h"
#include "global.h"
#include "mods.h"
#include "mod_host.h"
#include "mod_music.h"
#include "gba/m4a_internal.h"
#include "m4a.h"
#include "constants/songs.h"

enum { SAMPLE_RATE = 42060, RENDER_BLOCK = 256 };
static char *sRoot;
static tsf *sFont;
static tml_message *sSong, *sNext;
static uint64_t sSamples, sLoopSamples;
static uint32_t sAsset, sFontGeneration;
static float sVolume = 1;
static int sActive;
static const char *const sRoles[] = {"wild", "trainer", "gym", "champion"};
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
static void SetProgram(unsigned channel, unsigned program)
{
    if (tsf_channel_set_presetnumber(sFont, channel, program, channel == 9)) return;
    if (channel == 9 && tsf_channel_set_presetnumber(sFont, channel, 0, 1)) return;
    /* These game banks intentionally have fewer than 128 instruments. Choose
     * the nearest available voice in the GM family before a broader fallback. */
    for (unsigned pass = 0; pass < 2; ++pass)
        for (unsigned distance = 0; distance < 128; ++distance)
            for (unsigned side = 0; side < 2; ++side)
            {
                int candidate = (int)program + (side ? -(int)distance : (int)distance);
                if (candidate < 0 || candidate >= 128 || (!pass && (unsigned)candidate / 8 != program / 8)) continue;
                int index = tsf_get_presetindex(sFont, channel == 9 ? 128 : 0, candidate);
                if (index >= 0) { tsf_channel_set_presetindex(sFont, channel, index); return; }
            }
    tsf_channel_set_presetindex(sFont, channel, 0);
}
static void ResetChannels(void)
{
    tsf_reset(sFont);
    for (unsigned c = 0; c < 16; ++c)
    {
        SetProgram(c, 0);
    }
}
void NativeMusicOnStop(struct MusicPlayerInfo *player)
{
    if (player != &gMPlayInfo_BGM) return;
    sActive = 0; sAsset = 0;
    if (sFont) tsf_reset(sFont);
    if (sSong) tml_free(sSong);
    sSong = sNext = NULL;
}
void NativeMusicShutdown(void)
{
    NativeMusicOnStop(&gMPlayInfo_BGM);
    if (sFont) tsf_close(sFont);
    sFont = NULL; sFontGeneration = 0;
    NativeMusicSetAssetRoot(NULL);
}
int NativeMusicActive(void) { return sActive; }
uint32_t NativeMusicAsset(void) { return sAsset; }
void NativeMusicOnVolume(struct MusicPlayerInfo *player, unsigned volume)
{
    if (player == &gMPlayInfo_BGM) sVolume = volume / 256.0f;
}
static int LoadAsset(unsigned asset)
{
    unsigned generation = (asset - 1) / 4 + 1, role = (asset - 1) % 4;
    unsigned fontGeneration = generation < 3 ? 12 : generation;
    if (!sFont || sFontGeneration != fontGeneration)
    {
        char relative[64]; snprintf(relative, sizeof(relative), "fonts/gen%u.sf2", fontGeneration);
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
    char *path = AssetPath(relative);
    sSong = path ? tml_load_filename(path) : NULL;
    free(path);
    if (!sSong) { fprintf(stderr, "Battle MIDI unavailable: %s\n", relative); return -1; }
    unsigned end = 0;
    for (tml_message *m = sSong; m; m = m->next) if (m->time > end) end = m->time;
    sLoopSamples = ((uint64_t)end * SAMPLE_RATE + 999) / 1000 + 1;
    sSamples = 0; sNext = sSong; sAsset = asset; sVolume = 1; sActive = 1;
    ResetChannels();
    return 0;
}
void NativeMusicOnStart(struct MusicPlayerInfo *player, struct SongHeader *header)
{
    if (player != &gMPlayInfo_BGM) return;
    NativeMusicOnStop(player);
    for (unsigned i = 0; i < ARRAY_COUNT(sBattleSongs); ++i)
    {
        u16 song = sBattleSongs[i].song;
        if (gSongTable[song].header != header) continue;
        struct ModConfig config = ModsGetConfig();
        struct RhhModEvent e = {.struct_size = sizeof(e), .kind = RHH_BATTLE_MUSIC,
            .run_seed = config.seed, .event_key = RhhContentKeyForId(RHH_MUSIC, song), .args = {sBattleSongs[i].role}};
        struct RhhModDecision d = {.struct_size = sizeof(d), .allowed = 1};
        RhhModsDispatch(&e, &d);
        u32 asset;
        if (d.value && d.allowed && !RhhContentResolve(RHH_MIDI, d.value, &asset) && asset >= 1 && asset <= 20)
        {
            if (!LoadAsset(asset))
            {
                /* Native player skips the original BGM tracks while this
                 * synth is active, retaining the engine's fade/status logic. */
                player->status = 1;
                player->fadeOV = 64 << FADE_VOL_SHIFT;
            }
        }
        return;
    }
}
static void Message(const tml_message *m)
{
    unsigned channel = m->channel;
    switch (m->type)
    {
    case TML_NOTE_ON:
        if (m->velocity) tsf_channel_note_on(sFont, channel, (unsigned char)m->key, (unsigned char)m->velocity / 127.0f);
        else tsf_channel_note_off(sFont, channel, (unsigned char)m->key);
        break;
    case TML_NOTE_OFF: tsf_channel_note_off(sFont, channel, (unsigned char)m->key); break;
    case TML_PROGRAM_CHANGE:
        SetProgram(channel, (unsigned char)m->program);
        break;
    case TML_PITCH_BEND: tsf_channel_set_pitchwheel(sFont, channel, m->pitch_bend); break;
    case TML_CONTROL_CHANGE: tsf_channel_midi_control(sFont, channel, (unsigned char)m->control, (unsigned char)m->control_value); break;
    }
}
void NativeMusicMix(float *samples, unsigned frames)
{
    if (!sActive || !sFont || !sSong || (gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE)) return;
    float buffer[RENDER_BLOCK * 2];
    float gain = sVolume * (gMPlayInfo_BGM.fadeOV >> FADE_VOL_SHIFT) / 64.0f;
    unsigned at = 0;
    while (at < frames)
    {
        if (sSamples >= sLoopSamples) { sSamples = 0; sNext = sSong; ResetChannels(); }
        while (sNext && ((uint64_t)sNext->time * SAMPLE_RATE + 999) / 1000 <= sSamples)
        { Message(sNext); sNext = sNext->next; }
        unsigned count = frames - at;
        if (count > RENDER_BLOCK) count = RENDER_BLOCK;
        uint64_t until = sNext ? ((uint64_t)sNext->time * SAMPLE_RATE + 999) / 1000 : sLoopSamples;
        if (until > sSamples && until - sSamples < count) count = until - sSamples;
        if (!count) continue;
        tsf_render_float(sFont, buffer, count, 0);
        if (gSaveBlock2Ptr && gSaveBlock2Ptr->optionsSound == OPTIONS_SOUND_MONO)
            for (unsigned i = 0; i < count; ++i) buffer[i * 2] = buffer[i * 2 + 1] = (buffer[i * 2] + buffer[i * 2 + 1]) * 0.5f;
        for (unsigned i = 0; i < count * 2; ++i) samples[at * 2 + i] += buffer[i] * gain;
        sSamples += count; at += count;
    }
}
