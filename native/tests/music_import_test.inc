static const struct RhhModDefinition sMusicTransferTestMod = {
    .struct_size = sizeof(sMusicTransferTestMod), .abi_version = RHH_MOD_ABI_V1, .id = "test:music_transfer"
};
static void CheckMusicTransfer(const char *assetRoot, const char *sourceLibrary, const char *targetLibrary)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sMusicTransferTestMod) && !RhhModsFinalize());
    NativeMusicSetAssetRoot(assetRoot); NativeMusicSetImportRoot(sourceLibrary);
    const struct RhhMusicImportService *imports = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/music_imports", RHH_SERVICE_VERSION(1, 1));
    CHECK(imports && imports->header.struct_size >= RHH_MUSIC_IMPORT_SERVICE_V1_1_SIZE
        && RHH_MUSIC_IMPORT_SERVICE_V1_0_SIZE < RHH_MUSIC_IMPORT_SERVICE_V1_1_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/music_imports", RHH_SERVICE_VERSION(1, 2)));
    char midiPath[4096], fontPath[4096];
    CHECK(snprintf(midiPath, sizeof(midiPath), "%s/gen1/wild.mid", assetRoot) < sizeof(midiPath)
        && snprintf(fontPath, sizeof(fontPath), "%s/fonts/gen4.sf2", assetRoot) < sizeof(fontPath));
    RhhContentKey owner = RhhContentKeyOf("test:music_transfer");
    struct RhhMusicImportRequest request = {.struct_size = sizeof(request), .requester = owner,
        .midi_path = midiPath, .soundfont_path = fontPath, .label = "Portable wild theme"};
    struct RhhMusicImportResult result = {.struct_size = sizeof(result)};
    CHECK(!imports->import_pair(&request, &result) && result.changed);
    struct RhhImportedMusic track = result.track;
    u8 *midi = malloc(track.midi_size), *font = malloc(track.soundfont_size);
    CHECK(midi && font); memset(midi, 0xa5, track.midi_size); memset(font, 0x5a, track.soundfont_size);
    CHECK(imports->copy_pair(&track, midi, track.midi_size - 1, font, track.soundfont_size) == RHH_MUSIC_IMPORT_INVALID
        && midi[0] == 0xa5 && font[0] == 0x5a);
    CHECK(imports->copy_pair(&track, midi, track.midi_size, midi, track.soundfont_size) == RHH_MUSIC_IMPORT_INVALID
        && midi[0] == 0xa5);
    CHECK(!imports->copy_pair(&track, midi, track.midi_size, font, track.soundfont_size));
    u8 digest[32]; NativeMusicSha256(midi, track.midi_size, digest); CHECK(!memcmp(digest, track.midi_sha256, 32));
    NativeMusicSha256(font, track.soundfont_size, digest); CHECK(!memcmp(digest, track.soundfont_sha256, 32));
    NativeMusicSetImportRoot(targetLibrary); CHECK(imports->status() == RHH_MUSIC_IMPORT_OK && !imports->count());
    struct RhhMusicMemoryImportRequest transfer = {.struct_size = sizeof(transfer), .requester = owner,
        .expected = &track, .midi = midi, .midi_size = track.midi_size,
        .soundfont = font, .soundfont_size = track.soundfont_size};
    midi[track.midi_size - 1] ^= 1;
    CHECK(imports->import_memory(&transfer, &result) == RHH_MUSIC_IMPORT_INVALID && !imports->count());
    midi[track.midi_size - 1] ^= 1;
    struct RhhImportedMusic bad = track; bad.midi_sha256[0] ^= 1; transfer.expected = &bad;
    CHECK(imports->import_memory(&transfer, &result) == RHH_MUSIC_IMPORT_INVALID && !imports->count());
    transfer.expected = &track;
    CHECK(!imports->import_memory(&transfer, &result) && result.changed && result.track.key == track.key
        && !strcmp(result.track.label, track.label) && imports->count() == 1);
    CHECK(!imports->import_memory(&transfer, &result) && !result.changed && imports->count() == 1);
    memset(midi, 0, track.midi_size); memset(font, 0, track.soundfont_size);
    CHECK(!imports->copy_pair(&track, midi, track.midi_size, font, track.soundfont_size));
    NativeMusicSha256(midi, track.midi_size, digest); CHECK(!memcmp(digest, track.midi_sha256, 32));
    NativeMusicSha256(font, track.soundfont_size, digest); CHECK(!memcmp(digest, track.soundfont_sha256, 32));
    free(midi); free(font); NativeMusicSetImportRoot(NULL); NativeMusicShutdown(); RhhModsShutdown();
    puts("music transfer: verified pair, stable key and independent library passed");
}
static void CheckMusicImports(const char *save, const char *module, const char *assetRoot, const char *library, u32 mode)
{
    static const u8 abcHash[32] = {0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    static const u8 longHash[32] = {0x41,0xed,0xec,0xe4,0x2d,0x63,0xe8,0xd9,0xbf,0x51,0x5a,0x9b,0xa6,0x93,0x2e,0x1c,
        0x20,0xcb,0xc9,0xf5,0xa5,0xd1,0x34,0x64,0x5a,0xdb,0x5d,0xb1,0xb9,0x73,0x7e,0xa3};
    u8 hash[32], data[1000]; memset(data, 'a', sizeof(data)); NativeMusicSha256("abc", 3, hash); CHECK(!memcmp(hash, abcHash, 32));
    NativeMusicSha256(data, sizeof(data), hash); CHECK(!memcmp(hash, longHash, 32));
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(save)); gFlashMemoryPresent = TRUE;
    NativeMusicSetImportRoot(library); NativeMusicSetAssetRoot(assetRoot);
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhMusicImportService *imports = (const void *)host->get_service("rhh:service/music_imports", RHH_SERVICE_VERSION(1, 0));
    const struct RhhMusicService *music = (const void *)host->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 4));
    const struct RhhMusicAssignmentService *assign = (const void *)host->get_service("rhh:service/music_assignments", RHH_SERVICE_VERSION(1, 0));
    CHECK(imports && music && assign);
    char midi[4096], midi2[4096], font[4096], missing[4096];
    CHECK(snprintf(midi, sizeof(midi), "%s/gen1/wild.mid", assetRoot) < sizeof(midi)
        && snprintf(midi2, sizeof(midi2), "%s/gen1/trainer.mid", assetRoot) < sizeof(midi2)
        && snprintf(font, sizeof(font), "%s/fonts/gen4.sf2", assetRoot) < sizeof(font)
        && snprintf(missing, sizeof(missing), "%s/no-file.mid", library) < sizeof(missing));
    RhhContentKey owner = RhhContentKeyOf("test:music_import_probe");
    struct RhhMusicImportRequest request = {.struct_size = sizeof(request), .requester = owner,
        .midi_path = midi, .soundfont_path = font, .label = "Wild theme"};
    struct RhhMusicImportResult result = {.struct_size = sizeof(result)};
    struct RhhImportedMusic first = {.struct_size = sizeof(first)}, second = {.struct_size = sizeof(second)};
    if (mode == 0)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(99); Save_ResetSaveCounters();
        CHECK(imports->status() == RHH_MUSIC_IMPORT_OK && !imports->count() && imports->context(&result));
        struct RhhMusicImportResult small = {.struct_size = sizeof(small) - 1, .status = 777}, before = small;
        CHECK(imports->import_pair(&request, &small) == RHH_MUSIC_IMPORT_INVALID && !memcmp(&small, &before, sizeof(small)));
        request.midi_path = missing;
        CHECK(imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_MISSING && !imports->count());
        request.midi_path = font;
        CHECK(imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_INVALID && !imports->count());
        request.midi_path = midi; request.label = "";
        CHECK(imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_INVALID && !imports->count());
        request.label = "Wild theme"; gMain.inBattle = TRUE;
        CHECK(imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_UNAVAILABLE && !imports->count()); gMain.inBattle = FALSE;
        CHECK(!NativeSaveBegin());
        CHECK(imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_UNAVAILABLE && NativeSaveEnd(1, FALSE));
        CHECK(imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_OK && result.changed && imports->count() == 1);
        first = result.track; CHECK(first.key && !strcmp(first.label, "Wild theme") && first.midi_size && first.soundfont_size
            && music->availability(first.key) == RHH_MUSIC_READY && !imports->find(first.key, &first));
        struct RhhMusicTrackInfo info = {.struct_size = sizeof(info)};
        CHECK(!music->inspect_track(first.key, &info) && info.kind == RHH_MUSIC_TRACK_IMPORTED
            && info.backend == RHH_MUSIC_BACKEND_MIDI && info.availability == RHH_MUSIC_READY && info.track == first.key);
        struct RhhMusicSourceTiming timing = {.struct_size = sizeof(timing)};
        CHECK(!music->inspect_source_timing(first.key, &timing) && timing.kind == RHH_MUSIC_TRACK_IMPORTED
            && timing.availability == RHH_MUSIC_READY && (timing.flags & RHH_MUSIC_SOURCE_MIDI_TIMING)
            && timing.track == first.key && timing.last_event_ms && timing.loop_end_ms > timing.loop_start_ms);
        request.label = "Ignored duplicate name";
        CHECK(imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_OK && !result.changed
            && result.track.key == first.key && !strcmp(result.track.label, "Wild theme") && imports->count() == 1);
        request.midi_path = midi2; request.label = "Trainer theme";
        CHECK(imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_OK && result.changed && imports->count() == 2);
        second = result.track; CHECK(second.key != first.key && !memcmp(second.soundfont_sha256, first.soundfont_sha256, 32));
        struct RhhMusicAssignmentRequest assignment = {.struct_size = sizeof(assignment), .requester = owner,
            .action = RHH_MUSIC_ASSIGN_SET, .rule = {.struct_size = sizeof(assignment.rule), .context = RHH_MUSIC_OTHER, .track = first.key}};
        struct RhhMusicAssignmentResult assigned = {.struct_size = sizeof(assigned)};
        CHECK(assign->apply(&assignment, &assigned) == RHH_MUSIC_ASSIGN_OK && assigned.changed);
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        puts("music imports: validation, deduplication and saved assignment passed");
    }
    else if (mode == 1)
    {
        CHECK(imports->count() == 2 && !imports->at(0, &first) && !imports->at(1, &second));
        CHECK(music->availability(first.key) == RHH_MUSIC_INVALID); /* test corrupts the first copied MIDI */
        struct RhhMusicTrackInfo info = {.struct_size = sizeof(info)};
        CHECK(!music->inspect_track(first.key, &info) && info.kind == RHH_MUSIC_TRACK_IMPORTED && info.availability == RHH_MUSIC_INVALID);
        struct RhhMusicSourceTiming timing = {.struct_size = sizeof(timing)};
        CHECK(!music->inspect_source_timing(first.key, &timing) && timing.availability == RHH_MUSIC_INVALID && !timing.flags);
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
        cgb_audio_init(42060); m4aSoundInit(); sTestMusicSelection = 0;
        NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_ROUTE101].header);
        CHECK(!NativeMusicActive() && !music->current());
        CHECK(imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_OK && !result.changed
            && result.track.key == first.key && music->availability(first.key) == RHH_MUSIC_READY);
        NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_ROUTE101].header);
        CHECK(NativeMusicActive() && music->current() == first.key);
        float audio[4096] = {0}; NativeMusicMix(audio, 2048); double energy = 0;
        for (u32 i = 0; i < ARRAY_COUNT(audio); ++i) energy += (double)audio[i] * audio[i];
        CHECK(energy > 0.00001);
        puts("music imports: restart, corrupt-asset fallback and repair passed");
    }
    else if (mode == 2)
    {
        CHECK(imports->count() == 2 && !imports->at(0, &first) && !imports->at(1, &second)
            && LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
        CHECK(imports->remove(owner, first.key, &result) == RHH_MUSIC_IMPORT_OK && result.changed
            && imports->count() == 1 && music->availability(first.key) == RHH_MUSIC_MISSING
            && music->availability(second.key) == RHH_MUSIC_READY);
        struct RhhMusicTrackInfo info = {.struct_size = sizeof(info)};
        CHECK(!music->inspect_track(first.key, &info) && info.kind == RHH_MUSIC_TRACK_UNKNOWN && info.availability == RHH_MUSIC_MISSING);
        struct RhhMusicSourceTiming timing = {.struct_size = sizeof(timing)};
        CHECK(!music->inspect_source_timing(first.key, &timing) && timing.kind == RHH_MUSIC_TRACK_UNKNOWN
            && timing.availability == RHH_MUSIC_MISSING && !timing.flags);
        CHECK(imports->remove(owner, first.key, &result) == RHH_MUSIC_IMPORT_MISSING);
        CHECK(imports->remove(owner, second.key, &result) == RHH_MUSIC_IMPORT_OK && imports->count() == 0);
        struct RhhMusicSituation s = {.struct_size = sizeof(s), .context = RHH_MUSIC_OTHER};
        struct RhhMusicAssignment a = {.struct_size = sizeof(a)};
        CHECK(!assign->resolve(&s, &a) && a.track == first.key);
        cgb_audio_init(42060); m4aSoundInit(); NativeMusicOnStart(&gMPlayInfo_BGM, gSongTable[MUS_ROUTE101].header);
        CHECK(!NativeMusicActive() && !music->current());
        puts("music imports: shared assets and deleted-track fallback passed");
    }
    else if (mode == 3)
    {
        CHECK(imports->status() == RHH_MUSIC_IMPORT_IO && !imports->count()
            && imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_UNAVAILABLE);
        puts("music imports: corrupt manifest preserved and rejected");
    }
    else if (mode == 4)
    {
        CHECK(imports->status() == RHH_MUSIC_IMPORT_OK && imports->count() == 2
            && imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_UNAVAILABLE);
        puts("music imports: concurrent writer rejected");
    }
    else
    {
        char third[4096]; CHECK(snprintf(third, sizeof(third), "%s/gen1/gym.mid", assetRoot) < sizeof(third));
        request.midi_path = third; request.label = "Gym theme";
        CHECK(imports->status() == RHH_MUSIC_IMPORT_OK && imports->count() == 2
            && imports->import_pair(&request, &result) == RHH_MUSIC_IMPORT_IO && !result.changed
            && imports->count() == 2);
        puts("music imports: failed write left library unchanged");
    }
    NativeMusicShutdown(); NativeSaveClose(); RhhModsUnloadLibraries();
}
