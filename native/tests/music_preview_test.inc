#include "native_text_input.h"
static void CheckMusicPreview(const char *module, const char *root, const char *library)
{
    InitModsForTestWithLibrary(module); gModDraft = (struct ModConfig){0}; ModsNewGame(99);
    NativeMusicSetAssetRoot(root); NativeMusicSetImportRoot(library); cgb_audio_init(42060); m4aSoundInit();
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhMusicPreviewService *preview = (const void *)host->get_service("rhh:service/music_preview", RHH_SERVICE_VERSION(1, 0));
    const struct RhhMusicService *music = (const void *)host->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 0));
    const struct RhhMusicImportService *imports = (const void *)host->get_service("rhh:service/music_imports", RHH_SERVICE_VERSION(1, 0));
    const struct RhhUiService *ui = (const void *)host->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 0));
    CHECK(preview && music && imports && ui);
    RhhContentKey owner = RhhContentKeyOf("test:music_preview_probe"), other = RhhContentKeyOf("test:music_catalog");
    char midi[4096], font[4096];
    CHECK(snprintf(midi, sizeof(midi), "%s/gen1/wild.mid", root) < sizeof(midi)
        && snprintf(font, sizeof(font), "%s/fonts/gen4.sf2", root) < sizeof(font));
    struct RhhMusicImportRequest import = {.struct_size = sizeof(import), .requester = owner,
        .midi_path = midi, .soundfont_path = font, .label = "Preview track"};
    struct RhhMusicImportResult imported = {.struct_size = sizeof(imported)};
    CHECK(imports->import_pair(&import, &imported) == RHH_MUSIC_IMPORT_OK && imported.track.key);
    struct RhhMusicPreviewRequest request = {.struct_size = sizeof(request), .duration_ms = 1000,
        .volume_percent = 100, .owner = owner, .track = imported.track.key};
    struct RhhMusicPreviewSnapshot snapshot = {.struct_size = sizeof(snapshot)}, small = {.struct_size = sizeof(small) - 1, .active = 999}, old = small;
    CHECK(preview->current(&small) && !memcmp(&small, &old, sizeof(small))
        && preview->start(&request) == RHH_MUSIC_PREVIEW_UNAVAILABLE);
    sTestMusicSelection = 0; PlayBGM(MUS_ROUTE101);
    NativeMusicFlushLifecycle(); sTestMusicLifecycleCount = 0;
    struct RhhMusicLifecycleState beforePreview = {.struct_size = sizeof(beforePreview)};
    CHECK(!music->lifecycle(&beforePreview) && beforePreview.active);
    struct SongHeader *original = gMPlayInfo_BGM.songHeader; CHECK(original && !(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE));
    struct RhhModEvent tick = {.struct_size = sizeof(tick), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&tick, &decision); CHECK(ui->active());
    request.owner = other; CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_UNAVAILABLE); request.owner = owner;
    request.duration_ms = 499; CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_INVALID); request.duration_ms = 1000;
    request.volume_percent = 101; CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_INVALID); request.volume_percent = 100;
    request.track = RhhContentKeyOf("test:unknown_track");
    CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_MISSING && !(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE));
    request.track = RhhContentKeyOf("test:music/invalid_font");
    CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_AUDIO_ERROR && !(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE));
    request.track = imported.track.key;
    /* A BGM change inside a proposal invalidates the pending preview. */
    sTestLoopMode = 6;
    CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_UNAVAILABLE
        && !preview->current(&snapshot) && !snapshot.active);
    sTestLoopMode = 0; PlayBGM(MUS_ROUTE101); original = gMPlayInfo_BGM.songHeader;
    NativeMusicFlushLifecycle(); sTestMusicLifecycleCount = 0;
    CHECK(!music->lifecycle(&beforePreview) && beforePreview.active);
    struct RhhMusicPreviewTiming timing = {.struct_size = sizeof(timing), .position = 999};
    CHECK(preview->timing(&timing) && timing.position == 999);
    sTestLoopMode = 1;
    CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_OK && NativeMusicHasAudio()
        && (gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE) && gMPlayInfo_BGM.songHeader == original);
    CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_BUSY && preview->stop(other) == RHH_MUSIC_PREVIEW_UNAVAILABLE);
    struct RhhMusicPreviewTiming tinyTiming = {.struct_size = 1, .position = 999};
    CHECK(preview->timing(&tinyTiming) && tinyTiming.position == 999);
    CHECK(!preview->timing(&timing) && timing.sample_rate == 42060 && timing.loop_start == 4206
        && timing.loop_end == 16824 && sTestLoopContext.phase == RHH_MUSIC_LOOP_PREVIEW
        && sTestLoopContext.track == imported.track.key && sTestLoopContext.owner == owner);
    PlaySE(SE_SELECT); struct MusicPlayerInfo *fx = gMPlayTable[gSongTable[SE_SELECT].ms].info; u32 clock = fx->clock;
    for (u32 i = 0; i < 4; ++i) m4aSoundMain(); CHECK(fx->clock > clock);
    float output[4096] = {0}; NativeMusicMix(output, 2048); double energy = 0;
    for (u32 i = 0; i < ARRAY_COUNT(output); ++i) energy += (double)output[i] * output[i];
    CHECK(energy > 0.00001 && !preview->current(&snapshot) && snapshot.active
        && snapshot.owner == owner && snapshot.track == imported.track.key && snapshot.elapsed_ms > 0);
    static float previewLoopAudio[19000 * 2]; NativeMusicMix(previewLoopAudio, 19000);
    CHECK(!preview->timing(&timing) && timing.position == 4206 + 2048 + 19000 - 16824);
    sTestLoopMode = 0;
    CHECK(preview->stop(owner) == RHH_MUSIC_PREVIEW_OK && !(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE)
        && gMPlayInfo_BGM.songHeader == original && !preview->current(&snapshot) && !snapshot.active);
    NativeMusicFlushLifecycle();
    struct RhhMusicLifecycleState afterPreview = {.struct_size = sizeof(afterPreview)};
    CHECK(!music->lifecycle(&afterPreview) && afterPreview.active && !sTestMusicLifecycleCount
        && afterPreview.latest_sequence == beforePreview.latest_sequence
        && afterPreview.current.track == beforePreview.current.track);
    request.volume_percent = 0; CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_OK);
    for (u32 i = 0; i < ARRAY_COUNT(output); ++i) output[i] = 0.25f;
    NativeMusicMix(output, 2048);
    for (u32 i = 0; i < ARRAY_COUNT(output); ++i) CHECK(output[i] == 0.25f);
    CHECK(preview->stop(owner) == RHH_MUSIC_PREVIEW_OK);
    struct RhhMusicScaledPreviewRequest scaled = {.struct_size = sizeof(scaled), .duration_ms = 1000,
        .gain = RHH_MUSIC_GAIN_MAX + 1, .owner = owner, .track = imported.track.key};
    CHECK(host->get_service("rhh:service/music_preview", RHH_SERVICE_VERSION(1, 1))
        && preview->start_scaled(&scaled) == RHH_MUSIC_PREVIEW_INVALID);
    static float quietPreview[4096], normalPreview[4096], loudPreview[4096];
    scaled.gain = 500; CHECK(preview->start_scaled(&scaled) == RHH_MUSIC_PREVIEW_OK);
    NativeMusicMix(quietPreview, 2048); CHECK(preview->stop(owner) == RHH_MUSIC_PREVIEW_OK);
    scaled.gain = 1000; CHECK(preview->start_scaled(&scaled) == RHH_MUSIC_PREVIEW_OK);
    NativeMusicMix(normalPreview, 2048); CHECK(preview->stop(owner) == RHH_MUSIC_PREVIEW_OK);
    scaled.gain = 1500; CHECK(preview->start_scaled(&scaled) == RHH_MUSIC_PREVIEW_OK);
    NativeMusicMix(loudPreview, 2048); CHECK(preview->stop(owner) == RHH_MUSIC_PREVIEW_OK);
    double quietEnergy = 0, normalEnergy = 0, loudEnergy = 0;
    for (u32 i = 0; i < ARRAY_COUNT(quietPreview); ++i)
    { quietEnergy += quietPreview[i] * quietPreview[i]; normalEnergy += normalPreview[i] * normalPreview[i]; loudEnergy += loudPreview[i] * loudPreview[i]; }
    CHECK(normalEnergy > 0 && fabs(quietEnergy / normalEnergy - .25) < .01 && fabs(loudEnergy / normalEnergy - 2.25) < .01);
    request.volume_percent = 100; u32 oldSound = gSaveBlock2Ptr->optionsSound; gSaveBlock2Ptr->optionsSound = OPTIONS_SOUND_MONO;
    CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_OK); memset(output, 0, sizeof(output)); NativeMusicMix(output, 2048);
    for (u32 i = 0; i < 2048; ++i) CHECK(output[i * 2] == output[i * 2 + 1]);
    CHECK(preview->stop(owner) == RHH_MUSIC_PREVIEW_OK); gSaveBlock2Ptr->optionsSound = oldSound;
    request.duration_ms = 500; CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_OK);
    for (u32 i = 0; i < 24; ++i) { float block[2048] = {0}; NativeMusicMix(block, 1024); }
    CHECK(!preview->current(&snapshot) && !snapshot.active && !(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE));
    /* A native replacement track retains its position while preview plays. */
    sTestMusicSelection = RhhContentKeyOf("test:music/custom"); PlayBGM(MUS_VS_TRAINER);
    CHECK(NativeMusicActive() && music->current() == sTestMusicSelection);
    float block[4096] = {0}; NativeMusicMix(block, 2048); u64 position; NativeMusicLoopInfo(NULL, NULL, &position);
    struct RhhMusicPlayback playback = {.struct_size = sizeof(playback)};
    CHECK(!music->playback(&playback) && playback.track == sTestMusicSelection && playback.position == position
        && !(playback.flags & RHH_MUSIC_PLAYBACK_PAUSED));
    CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_OK); memset(block, 0, sizeof(block)); NativeMusicMix(block, 2048);
    CHECK(!music->playback(&playback) && playback.position == position && (playback.flags & RHH_MUSIC_PLAYBACK_PAUSED));
    const struct RhhMusicLevelService *levels = (const void *)host->get_service("rhh:service/music_levels", RHH_SERVICE_VERSION(1, 0));
    CHECK(levels);
    struct RhhMusicLevelRequest levelRequest = {.struct_size = sizeof(levelRequest), .requester = owner,
        .track = sTestMusicSelection, .gain = 500, .expected_revision = levels->revision()};
    struct RhhMusicLevelResult levelResult = {.struct_size = sizeof(levelResult)};
    CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_OK && !music->playback(&playback)
        && playback.gain == 500 && (playback.flags & RHH_MUSIC_PLAYBACK_PAUSED) && (playback.flags & RHH_MUSIC_PLAYBACK_LEVEL_RAMP));
    NativeMusicMix(block, 2048);
    CHECK(!music->playback(&playback) && playback.position == position && (playback.flags & RHH_MUSIC_PLAYBACK_LEVEL_RAMP));
    u64 during; NativeMusicLoopInfo(NULL, NULL, &during); CHECK(position == during && music->current() == sTestMusicSelection);
    CHECK(preview->stop(owner) == RHH_MUSIC_PREVIEW_OK); NativeMusicMix(block, 2048);
    NativeMusicLoopInfo(NULL, NULL, &during); CHECK(during == position + 2048);
    CHECK(!music->playback(&playback) && playback.position == during && playback.gain == 500
        && !(playback.flags & (RHH_MUSIC_PLAYBACK_PAUSED | RHH_MUSIC_PLAYBACK_LEVEL_RAMP)));
    levelRequest.gain = RHH_MUSIC_GAIN_ONE; levelRequest.expected_revision = levels->revision();
    CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_OK);
    sTestMusicSelection = 0; ModsSetMusic(1); PlayBGM(MUS_VS_WILD); CHECK(NativeMusicActive() && NativeMusicAsset() == 1);
    NativeMusicMix(block, 2048); NativeMusicLoopInfo(NULL, NULL, &position);
    CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_OK); NativeMusicMix(block, 2048);
    NativeMusicLoopInfo(NULL, NULL, &during); CHECK(position == during);
    CHECK(preview->stop(owner) == RHH_MUSIC_PREVIEW_OK); NativeMusicMix(block, 2048);
    NativeMusicLoopInfo(NULL, NULL, &during); CHECK(during == position + 2048); ModsSetMusic(0);
    CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_OK && !ui->close(owner)
        && !preview->current(&snapshot) && !snapshot.active && !(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE));
    RhhModsDispatch(&tick, &decision); CHECK(ui->active());
    CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_OK); sTestMusicSelection = 0; PlayBGM(MUS_ROUTE110);
    CHECK(!preview->current(&snapshot) && !snapshot.active && !NativeMusicActive());
    m4aMPlayStop(&gMPlayInfo_BGM); CHECK(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE);
    CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_OK && preview->stop(owner) == RHH_MUSIC_PREVIEW_OK
        && (gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE));
    m4aMPlayContinue(&gMPlayInfo_BGM); CHECK(preview->start(&request) == RHH_MUSIC_PREVIEW_OK);
    RhhModsSetRunContext(123, 457, 0); CHECK(!ui->active() && !preview->current(&snapshot) && !snapshot.active
        && !(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE));
    NativeMusicShutdown(); RhhModsUnloadLibraries();
    puts("music preview: modal ownership, audio, SFX and restoration passed");
}

