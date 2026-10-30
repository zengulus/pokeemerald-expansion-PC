static void CheckMusicAssignments(const char *path, const char *module, const char *root, bool32 load)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    const struct RhhMusicAssignmentService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/music_assignments", RHH_SERVICE_VERSION(1, 0));
    const struct RhhMusicLevelService *levels = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/music_levels", RHH_SERVICE_VERSION(1, 0));
    CHECK(service && service->header.struct_size >= sizeof(*service) && levels && levels->header.struct_size >= sizeof(*levels));
    RhhContentKey owner = RhhContentKeyOf("test:music_assign_probe");
    RhhContentKey custom = RhhContentKeyOf("test:music/custom"), missing = RhhContentKeyOf("test:music/missing");
    RhhContentKey builtIn = RhhContentKeyForId(RHH_MIDI, 9), map = RhhContentKeyForId(RHH_MAP, (1 << 8));
    RhhContentKey original = RhhContentKeyForId(RHH_MUSIC, MUS_VS_TRAINER);
    RhhContentKey roxanne = RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1);
    RhhContentKey other = RhhContentKeyForId(RHH_TRAINER, TRAINER_BRAWLY_1);
    CHECK(owner && custom && missing && builtIn && map && original && roxanne && other);
    struct RhhMusicAssignment rule = {.struct_size = sizeof(rule)};
    struct RhhMusicSituation situation = {.struct_size = sizeof(situation), .context = RHH_MUSIC_TRAINER,
        .map = map, .original = original, .trainers = {roxanne, other}};
    struct RhhMusicAssignmentResult result = {.struct_size = sizeof(result)};
    struct RhhMusicLevelRequest levelRequest = {.struct_size = sizeof(levelRequest), .requester = owner, .track = custom, .gain = 500};
    struct RhhMusicLevelResult levelResult = {.struct_size = sizeof(levelResult)};
    u32 level = 999;
    if (!load)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(99); Save_ResetSaveCounters();
        gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
        CHECK(!service->count() && !service->revision() && service->resolve(&situation, &rule));
        CHECK(!levels->revision() && !levels->count() && !levels->get(custom, &level) && level == RHH_MUSIC_GAIN_ONE);
        CHECK(levels->preview(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_OK && levelResult.changed
            && levelResult.is_preview && levelResult.before_gain == 1000 && levelResult.after_gain == 500
            && levelResult.after_revision == 1 && !levels->count());
        levelRequest.gain = 2200;
        CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_DENIED && !strcmp(levelResult.reason, "level denied")
            && !levels->revision());
        rng_value_t levelRng = gRngValue, levelRng2 = gRng2Value;
        levelRequest.gain = 2100;
        CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_STALE && !levels->revision()
            && !memcmp(&levelRng, &gRngValue, sizeof(levelRng)) && !memcmp(&levelRng2, &gRng2Value, sizeof(levelRng2)));
        levelRequest.gain = 500; sTestMusicObserveRng = 1;
        CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_OK && levelResult.changed && levels->revision() == 1
            && !memcmp(&levelRng, &gRngValue, sizeof(levelRng)) && !memcmp(&levelRng2, &gRng2Value, sizeof(levelRng2)));
        sTestMusicObserveRng = 0;
        CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_STALE);
        levelRequest.expected_revision = 1;
        CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_OK && !levelResult.changed);
        levelRequest.track = RhhContentKeyForId(RHH_MIDI, 1); levelRequest.gain = 1500;
        CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_OK && levels->revision() == 2 && levels->count() == 2);
        struct RhhMusicLevel first = {.struct_size = sizeof(first)}, second = {.struct_size = sizeof(second)};
        CHECK(!levels->at(0, &first) && !levels->at(1, &second) && first.track < second.track && levels->at(2, &second));
        struct RhhMusicLevelResult smallLevel = {.struct_size = 1, .status = 999}, savedLevel = smallLevel;
        CHECK(levels->apply(&levelRequest, &smallLevel) == RHH_MUSIC_LEVEL_INVALID && !memcmp(&smallLevel, &savedLevel, sizeof(smallLevel)));
        levelRequest.expected_revision = 2; levelRequest.track = RhhContentKeyOf("test:unknown_music"); levelRequest.gain = 900;
        CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_INVALID && levels->revision() == 2);
        levelRequest.track = builtIn;
        CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_INVALID && levels->revision() == 2); /* MP2K is not a replacement synthesizer. */
        struct RhhMusicAssignmentRequest request = {.struct_size = sizeof(request), .action = RHH_MUSIC_ASSIGN_SET,
            .requester = owner, .rule = {.struct_size = sizeof(request.rule), .context = RHH_MUSIC_ANY, .track = custom}};
        NativeMusicSetAssetRoot(root); cgb_audio_init(42060); m4aSoundInit();
        sTestMusicSelection = builtIn; sTestMusicRngMode = 3;
        rng_value_t battleRng = gRngValue, battleRng2 = gRng2Value;
        NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_VS_TRAINER].header);
        CHECK(!NativeMusicActive() && !NativeMusicAsset() && !memcmp(&battleRng, &gRngValue, sizeof(battleRng))
            && !memcmp(&battleRng2, &gRng2Value, sizeof(battleRng2)));
        sTestMusicSelection = 0; sTestMusicRngMode = 0; NativeMusicShutdown();
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        sTestAssignmentRngMode = 1;
        CHECK(service->preview(&request, &result) == RHH_MUSIC_ASSIGN_STALE && !result.changed && !service->revision()
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_STALE && !service->count()
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        sTestAssignmentRngMode = 2;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_DENIED && !strcmp(result.reason, "impure denial")
            && !service->revision() && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        sTestAssignmentRngMode = 0;
        CHECK(service->preview(&request, &result) == RHH_MUSIC_ASSIGN_OK && result.changed && result.is_preview
            && result.after_revision == 1 && !service->count() && !memcmp(&rng, &gRngValue, sizeof(rng)));
        sTestMusicObserveRng = 1;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_OK && result.changed && service->revision() == 1
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        sTestMusicObserveRng = 0;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_STALE && service->count() == 1);
        request.expected_revision = 1;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_OK && !result.changed && service->revision() == 1);
        request.rule.context = RHH_MUSIC_GYM; request.rule.track = builtIn;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_DENIED && !strcmp(result.reason, "test denied")
            && service->revision() == 1);
        request.rule.context = RHH_MUSIC_TRAINER;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_OK && result.after_revision == 2);
        request.expected_revision = 2; request.rule.context = RHH_MUSIC_ANY; request.rule.map = map; request.rule.track = missing;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_OK && service->resolve(&situation, &rule) == 0 && rule.track == missing);
        request.expected_revision = 3; request.rule.map = 0; request.rule.trainer = other; request.rule.track = builtIn;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_OK && service->resolve(&situation, &rule) == 0 && rule.track == builtIn);
        request.expected_revision = 4; request.rule.trainer = roxanne; request.rule.track = custom;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_OK && service->resolve(&situation, &rule) == 0 && rule.track == custom);
        situation.trainers[0] = other; situation.trainers[1] = roxanne;
        CHECK(!service->resolve(&situation, &rule) && rule.track == builtIn);
        situation.trainers[0] = roxanne; situation.trainers[1] = other;
        request.expected_revision = 5; request.rule.trainer = 0; request.rule.original = original; request.rule.track = builtIn;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_OK && service->revision() == 6
            && !service->resolve(&situation, &rule) && rule.track == custom);
        situation.trainers[0] = situation.trainers[1] = 0;
        CHECK(!service->resolve(&situation, &rule) && rule.track == builtIn);
        situation.trainers[0] = roxanne; situation.trainers[1] = other;
        struct RhhMusicAssignmentResult small = {.struct_size = sizeof(small) - 1, .status = 999}, savedSmall = small;
        CHECK(service->apply(&request, &small) == RHH_MUSIC_ASSIGN_INVALID && !memcmp(&small, &savedSmall, sizeof(small)));
        request.expected_revision = 6; request.rule.trainer = 0; request.rule.original = RhhContentKeyOf("test:invalid_song");
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_INVALID && service->revision() == 6);
        request.rule.original = original; request.rule.track = RhhContentKeyOf("test:missing_track");
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_INVALID);
        CHECK(!NativeSaveBegin()); request.rule.original = 0; request.rule.track = custom;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_UNAVAILABLE && NativeSaveEnd(1, FALSE));
        NativeMusicSetAssetRoot(root); cgb_audio_init(42060); m4aSoundInit(); sTestMusicSelection = 0;
        gBattleTypeFlags = BATTLE_TYPE_TRAINER; TRAINER_BATTLE_PARAM.opponentA = TRAINER_ROXANNE_1;
        /* A music transform cannot advance gameplay RNG or select an impure
         * track; a denial remains effective even if that hook used RNG. */
        sTestMusicSelection = builtIn; sTestMusicRngMode = 1; rng = gRngValue; rng2 = gRng2Value;
        NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_VS_TRAINER].header);
        CHECK(NativeMusicActive() && !NativeMusicAsset() && !memcmp(&rng, &gRngValue, sizeof(rng))
            && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        sTestMusicRngMode = 2;
        NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_VS_TRAINER].header);
        CHECK(!NativeMusicActive() && !NativeMusicAsset() && !memcmp(&rng, &gRngValue, sizeof(rng))
            && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        sTestMusicRngMode = 4;
        NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_VS_TRAINER].header);
        CHECK(!NativeMusicActive() && !NativeMusicAsset() && !memcmp(&rng, &gRngValue, sizeof(rng))
            && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        sTestMusicObserveRng = 1; NativeMusicFlushLifecycle();
        CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        sTestMusicObserveRng = 0;
        sTestMusicRngMode = 0; sTestMusicSelection = 0;
        NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_VS_TRAINER].header);
        CHECK(NativeMusicActive() && NativeMusicAsset() == 0 && NativeMusicAssignedTrack(RHH_MUSIC_TRAINER, original, map, roxanne, 0) == custom);
        CHECK(((const struct RhhMusicService *)RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 0)))->current() == custom);
        struct RhhMusicPlayback playback = {.struct_size = sizeof(playback)};
        CHECK(!((const struct RhhMusicService *)RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 1)))->playback(&playback)
            && playback.track == custom && playback.gain == 500);
        sTestGainMode = 1; sTestGainValue = 500;
        NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_VS_TRAINER].header);
        CHECK(!((const struct RhhMusicService *)RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 1)))->playback(&playback)
            && playback.track == custom && playback.gain == 250);
        sTestGainMode = 0; sTestGainValue = RHH_MUSIC_GAIN_ONE;
        sTestMusicSelection = builtIn; NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_VS_TRAINER].header);
        CHECK(!NativeMusicActive() && NativeMusicAsset() == 9);
        const struct RhhMusicService *music = (const void *)RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 2));
        struct RhhMusicLifecycleState lifecycle = {.struct_size = sizeof(lifecycle)};
        CHECK(music && !music->lifecycle(&lifecycle) && lifecycle.active
            && lifecycle.current.backend == RHH_MUSIC_BACKEND_MP2K && lifecycle.current.asset == 9
            && lifecycle.current.track == builtIn && !lifecycle.current.flags);
        sTestMusicSelection = 0;
        gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_FRONTIER;
        NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_VS_TRAINER].header);
        CHECK(!NativeMusicActive() && NativeMusicAsset() == 9); /* facility trainer IDs are not authored bosses */
        NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_VS_WILD].header);
        CHECK(!NativeMusicActive() && !NativeMusicAsset()); /* missing map-specific pair falls back */
        CHECK(!music->lifecycle(&lifecycle) && lifecycle.active
            && lifecycle.current.backend == RHH_MUSIC_BACKEND_MP2K && (lifecycle.current.flags & RHH_MUSIC_FALLBACK)
            && lifecycle.current.original == RhhContentKeyForId(RHH_MUSIC, MUS_VS_WILD));
        NativeMusicShutdown(); gBattleTypeFlags = 0;
        u8 *snapshot = NULL; u32 size = 0; CHECK(!RhhModsExport(&snapshot, &size));
        u8 *bad = malloc(size); CHECK(bad); memcpy(bad, snapshot, size); u32 offset = 24;
        u32 levelOffset = 24;
        while (levelOffset + 80 <= size && strcmp((const char *)snapshot + levelOffset, "rhh:music_levels"))
            levelOffset += 80 + ClockTestGet32(snapshot + levelOffset + 68);
        CHECK(levelOffset + 80 + 8 + 16 <= size);
        ClockTestPut32(bad + levelOffset + 80 + 8 + 8, RHH_MUSIC_GAIN_MAX + 1); ClockTestEnvelope(bad, size);
        CHECK(RhhModsImport(bad, size) && levels->revision() == 2 && !levels->get(custom, &level) && level == 500);
        memcpy(bad, snapshot, size);
        u32 removed = 80 + ClockTestGet32(bad + levelOffset + 68);
        memmove(bad + levelOffset, bad + levelOffset + removed, size - levelOffset - removed);
        ClockTestPut32(bad + 12, ClockTestGet32(bad + 12) - 1); ClockTestEnvelope(bad, size - removed);
        CHECK(!RhhModsImport(bad, size - removed) && !levels->revision() && !levels->count()
            && !levels->get(custom, &level) && level == RHH_MUSIC_GAIN_ONE);
        CHECK(!RhhModsImport(snapshot, size) && levels->revision() == 2 && levels->count() == 2);
        memcpy(bad, snapshot, size);
        while (offset + 80 <= size && strcmp((const char *)bad + offset, "rhh:music_assignments"))
            offset += 80 + ClockTestGet32(bad + offset + 68);
        CHECK(offset + 80 + 8 + 80 <= size);
        memcpy(bad + offset + 80 + 8 + 40, bad + offset + 80 + 8, 40);
        ClockTestEnvelope(bad, size);
        CHECK(RhhModsImport(bad, size) && service->revision() == 6 && service->count() == 6);
        free(snapshot); free(bad);
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        levelRequest.track = custom; levelRequest.gain = RHH_MUSIC_GAIN_ONE; levelRequest.expected_revision = 2;
        CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_OK && levels->revision() == 3 && levels->count() == 1);
        request.rule.context = RHH_MUSIC_ANY; request.rule.map = 0; request.rule.trainer = roxanne;
        request.rule.track = 0; request.action = RHH_MUSIC_ASSIGN_CLEAR;
        CHECK(service->apply(&request, &result) == RHH_MUSIC_ASSIGN_OK && service->revision() == 7);
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && service->revision() == 6
            && !service->resolve(&situation, &rule) && rule.track == custom
            && levels->revision() == 2 && !levels->get(custom, &level) && level == 500);
        puts("music assignments: constraints, precedence, playback and rollback passed");
    }
    else
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && service->revision() == 6 && service->count() == 6
            && !service->resolve(&situation, &rule) && rule.track == custom
            && levels->revision() == 2 && levels->count() == 2 && !levels->get(custom, &level) && level == 500);
        CHECK(service->at(6, &rule) && service->context(&result));
        puts("music assignments: fresh-process restore passed");
    }
    NativeSaveClose(); RhhModsUnloadLibraries();
}
