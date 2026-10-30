struct PhotoAlbumExpected { u64 run, first, second, actor; u32 generationA, generationB; };
static int RejectPhotoJournal(u64 run, const u8 *data, u32 size)
{ (void)run; (void)data; (void)size; return -1; }
static int SavePhotoJournal(u64 run, const u8 *data, u32 size)
{ return NativeSaveSetJournal(run, data, size); }
static void AlbumTestCapture(const struct RhhPhotoService *photo, u64 handle, u16 color, u32 frame)
{
    struct RhhPhotoRequest request = {.struct_size = sizeof(request), .owner = RhhContentKeyOf("test:photo_album_probe"),
        .actor_handle = handle, .max_distance = 4};
    struct RhhPhotoCapture capture = {.struct_size = sizeof(capture)};
    static u16 pixels[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
    for (u32 i = 0; i < ARRAY_COUNT(pixels); ++i) pixels[i] = color;
    CHECK(!photo->request(&request, &capture)); RhhModsPhotoPresent(pixels, FALSE, TRUE, frame);
}
static void AlbumTestMap(void)
{
    gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    ResetSpriteData(); gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
    gSprites[0].inUse = gSprites[1].inUse = TRUE;
    gObjectEvents[0].spriteId = 0; gObjectEvents[1].spriteId = 1;
    gMain.callback2 = CB2_Overworld; ScriptContext_Init(); UnlockPlayerFieldControls();
}
static void CheckPhotoAlbum(const char *path, const char *module, const char *expectedPath, u32 mode)
{
    InitModsForTestWithLibrary(module);
    if (mode == 2) { CHECK(NativeSaveOpen(path) != 0); NativeSaveClose(); RhhModsUnloadLibraries(); return; }
    CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhPhotoAlbumService *album = (const void *)host->get_service("rhh:service/photo_album", RHH_SERVICE_VERSION(1, 0));
    const struct RhhPhotoService *photo = (const void *)host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 0));
    const struct RhhPhotoScoreService *score = (const void *)host->get_service("rhh:service/photo_score", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWildActorService *actors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    CHECK(album && photo && actors && score && !host->get_service("rhh:service/photo_album", RHH_SERVICE_VERSION(1, 1))
        && host->get_service("rhh:service/photo_score", RHH_SERVICE_VERSION(1, 1))
        && !host->get_service("rhh:service/photo_score", RHH_SERVICE_VERSION(1, 2)));
    struct RhhPhotoScoreContext outside = {.struct_size = sizeof(outside), .habitat_available = 999};
    CHECK(score->context(&outside) && outside.habitat_available == 999);
    struct PhotoAlbumExpected expected = {0}; static u8 rgb[RHH_PHOTO_RGB_BYTES];
    if (!mode)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        memset(gObjectEvents, 0, sizeof(gObjectEvents)); AlbumTestMap();
        struct ObjectEvent *player = &gObjectEvents[0], *wild = &gObjectEvents[1];
        player->active = player->isPlayer = TRUE; player->mapGroup = 1; player->mapNum = 0;
        player->currentCoords.x = player->previousCoords.x = MAP_OFFSET; player->currentCoords.y = player->previousCoords.y = MAP_OFFSET;
        wild->active = TRUE; wild->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; wild->localId = LOCALID_OW_ENCOUNTER_END;
        wild->mapGroup = 1; wild->mapNum = 0; wild->trainerRange_berryTreeId = 5;
        wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 2; wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET;
        struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 19);
        wild->graphicsId = GetGraphicsIdForMon(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_IS_SHINY),
            GetGenderFromSpeciesAndPersonality(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_PERSONALITY)) == MON_FEMALE);
        ModsWildActorSpawned(wild, &mon);
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)}; CHECK(!actors->at(1, &actor));
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        struct RhhWildActorIndividual id = {.struct_size = sizeof(id)}; CHECK(!actors->individual(actor.handle, &id));
        AlbumTestCapture(photo, actor.handle, RGB(0, 0, 31), 1);
        AlbumTestCapture(photo, actor.handle, RGB(31, 0, 0), 2);
        CHECK(album->count() == 2);
        struct RhhPhotoAlbumEntry a = {.struct_size = sizeof(a)}, b = {.struct_size = sizeof(b)};
        CHECK(!album->at(0, &a) && !album->at(1, &b) && a.id.serial < b.id.serial
            && a.individual.serial == id.serial && b.individual.serial == id.serial);
        struct RhhPhotoScore scored = {.struct_size = sizeof(scored)}, unchanged = {.struct_size = sizeof(unchanged), .value = 999};
        CHECK(!score->find(&a.id, &scored) && scored.value == 777 && scored.base <= RHH_PHOTO_SCORE_MAX
            && (scored.flags & (RHH_PHOTO_SCORE_AVAILABLE | RHH_PHOTO_SCORE_ESTIMATED | RHH_PHOTO_SCORE_PARTIAL_CHECKS))
                == (RHH_PHOTO_SCORE_AVAILABLE | RHH_PHOTO_SCORE_ESTIMATED | RHH_PHOTO_SCORE_PARTIAL_CHECKS)
            && score->find(&b.id, &unchanged) && unchanged.value == 999);
        CHECK(!album->copy_rgb(&a.id, rgb, sizeof(rgb)) && !rgb[0] && !rgb[1] && rgb[2] == 255);
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); expected.generationA = gSaveCounter;
        CHECK(!album->remove(&a.id) && album->count() == 1 && album->find(&a.id, &a));
        CHECK(score->find(&a.id, &scored));
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); expected.generationB = gSaveCounter;
        expected.run = id.run_id; expected.actor = id.serial; expected.first = a.id.serial; expected.second = b.id.serial;
        CHECK(expected.generationA != expected.generationB);
        FILE *file = fopen(expectedPath, "wb"); CHECK(file && fwrite(&expected, sizeof(expected), 1, file) == 1 && !fclose(file));
        puts("album: generation snapshots and deletion passed");
    }
    else if (mode == 1)
    {
        FILE *file = fopen(expectedPath, "rb"); CHECK(file && fread(&expected, sizeof(expected), 1, file) == 1 && !fclose(file));
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && album->count() == 1);
        struct RhhPhotoAlbumEntry entry = {.struct_size = sizeof(entry)};
        CHECK(!album->at(0, &entry) && entry.id.run_id == expected.run && entry.id.serial == expected.second
            && !album->copy_rgb(&entry.id, rgb, sizeof(rgb)) && rgb[0] == 255 && !rgb[1] && !rgb[2]);
        struct RhhPhotoScore scored = {.struct_size = sizeof(scored), .value = 999};
        CHECK(score->find(&entry.id, &scored) && scored.value == 999);
        CHECK(!NativeSaveRestoreMods(expected.generationA) && album->count() == 2);
        CHECK(!album->at(0, &entry) && entry.id.serial == expected.first && !album->copy_rgb(&entry.id, rgb, sizeof(rgb))
            && !rgb[0] && !rgb[1] && rgb[2] == 255);
        CHECK(!score->find(&entry.id, &scored) && scored.value == 777);
        CHECK(!album->at(1, &entry) && entry.id.serial == expected.second);
        u32 size; const void *journal = NativeSaveGetJournal(expected.run, &size);
        CHECK(journal && !RhhModsMergeJournal(journal, size));
        AlbumTestMap();
        struct RhhWildActorIndividual id = {.struct_size = sizeof(id), .run_id = expected.run, .serial = expected.actor};
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
        CHECK(!actors->find_individual(&id, &actor));
        AlbumTestCapture(photo, actor.handle, RGB(0, 31, 0), 3);
        CHECK(album->count() == 3 && !album->at(2, &entry) && entry.id.serial > expected.second
            && !album->copy_rgb(&entry.id, rgb, sizeof(rgb)) && !rgb[0] && rgb[1] == 255 && !rgb[2]);
        CHECK(!score->find(&entry.id, &scored) && scored.value == scored.base && scored.value != 777);
        for (u32 i = 3; i < RHH_PHOTO_ALBUM_CAPACITY; ++i) AlbumTestCapture(photo, actor.handle, RGB(0, 31, 0), 100 + i);
        CHECK(album->count() == RHH_PHOTO_ALBUM_CAPACITY);
        AlbumTestCapture(photo, actor.handle, RGB(0, 31, 0), 20); CHECK(album->count() == RHH_PHOTO_ALBUM_CAPACITY);
        CHECK(!NativeSaveBegin() && album->remove(&entry.id) == RHH_ALBUM_UNAVAILABLE && NativeSaveEnd(999, FALSE));
        CHECK(album->count() == RHH_PHOTO_ALBUM_CAPACITY);
        u8 *sequence = RhhModsState("rhh:photo_sequence"), oldSequence[8]; CHECK(sequence);
        memcpy(oldSequence, sequence, sizeof(oldSequence)); memset(sequence, 0, sizeof(oldSequence));
        unsigned failures = sObservedPersistenceFailures; RhhModsSetPersistence(RejectPhotoJournal);
        AlbumTestCapture(photo, actor.handle, RGB(0, 31, 0), 21);
        CHECK(sObservedPersistenceFailures == failures + 1 && !memcmp(sequence, (u8[8]){0}, 8)
            && album->count() == RHH_PHOTO_ALBUM_CAPACITY);
        memcpy(sequence, oldSequence, sizeof(oldSequence));
        RhhModsSetPersistence(SavePhotoJournal);
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        puts("album: fresh-process restore, rollback-safe IDs and capacity passed");
    }
    else if (mode == 3)
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && album->count() == RHH_PHOTO_ALBUM_CAPACITY);
        struct RhhPhotoAlbumEntry first = {.struct_size = sizeof(first)}, last = {.struct_size = sizeof(last)};
        CHECK(!album->at(0, &first) && !album->at(RHH_PHOTO_ALBUM_CAPACITY - 1, &last)
            && first.id.serial < last.id.serial && !album->copy_rgb(&last.id, rgb, sizeof(rgb))
            && !rgb[0] && rgb[1] == 255 && !rgb[2]);
        struct RhhPhotoScore scored = {.struct_size = sizeof(scored)};
        CHECK(!score->find(&first.id, &scored) && scored.value == 777
            && !score->find(&last.id, &scored) && scored.value == 777);
        puts("album: full archive survived a fresh process");
    }
    else
    {
        FILE *file = fopen(expectedPath, "rb"); CHECK(file && fread(&expected, sizeof(expected), 1, file) == 1 && !fclose(file));
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
        if (mode == 4) CHECK(!NativeSaveRestoreMods(expected.generationA));
        CHECK(album->count() == 2);
        struct RhhPhotoAlbumEntry entry = {.struct_size = sizeof(entry)};
        struct RhhPhotoScore missing = {.struct_size = sizeof(missing), .value = 999};
        CHECK(!album->at(0, &entry) && entry.id.serial == expected.first
            && !album->copy_rgb(&entry.id, rgb, sizeof(rgb)) && !rgb[0] && !rgb[1] && rgb[2] == 255
            && score->find(&entry.id, &missing) && missing.value == 999);
        CHECK(!album->at(1, &entry) && entry.id.serial == expected.second
            && !album->copy_rgb(&entry.id, rgb, sizeof(rgb)) && rgb[0] == 255 && !rgb[1] && !rgb[2]
            && score->find(&entry.id, &missing) && missing.value == 999);
        if (mode == 4) { AlbumTestMap(); CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); }
        puts("album: legacy photos remain unscored");
    }
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    NativeSaveClose(); RhhModsUnloadLibraries();
}
