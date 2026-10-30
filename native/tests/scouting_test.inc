static void CheckScouting(const char *path, const char *module, bool32 load)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhScoutingService *scout = (const void *)host->get_service("rhh:service/scouting", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWildActorService *actors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    CHECK(scout && actors && host->get_service("rhh:service/scouting", RHH_SERVICE_VERSION(1, 1)) == &scout->header
        && host->get_service("rhh:service/scouting", RHH_SERVICE_VERSION(1, 2)) == &scout->header
        && host->get_service("rhh:service/scouting", RHH_SERVICE_VERSION(1, 3)) == &scout->header
        && !host->get_service("rhh:service/scouting", RHH_SERVICE_VERSION(1, 4))
        && scout->header.struct_size >= RHH_SCOUTING_SERVICE_V1_3_SIZE);
    struct RhhScoutingResult result = {.struct_size = sizeof(result)};
    CHECK(scout->context(&result));
    if (load)
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && scout->count() == 1);
        struct RhhScoutingRecord record = {.struct_size = sizeof(record)}, found = {.struct_size = sizeof(found)};
        CHECK(!scout->at(0, &record) && record.observations == 2 && record.flags == RHH_SCOUT_IDENTIFIED
            && record.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_POOCHYENA)
            && record.individual.run_id && record.individual.serial && !scout->find(&record.individual, &found)
            && found.individual.serial == record.individual.serial);
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
        CHECK(!actors->find_individual(&record.individual, &actor) && actor.species == record.species);
        gSprites[gObjectEvents[actor.slot].spriteId].inUse = TRUE;
        s32 coverY = 0;
        CHECK(ModsWildActorPresentation(gObjectEvents[actor.slot].spriteId, &coverY) == 2);
        const struct RhhEcologyService *eco = (const void *)host->get_service("rhh:service/ecology", RHH_SERVICE_VERSION(1, 0));
        int64_t observations = 0;
        CHECK(eco && !eco->get(record.first_map, RHH_ECOLOGY_OBSERVATIONS, record.species, &observations)
            && observations == 2);
        puts("scouting: fresh-process save restored record and individual");
    }
    else
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE101)][MAP_NUM(MAP_ROUTE101)];
        gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE101); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE101);
        struct MapLayout layout = *gMapHeader.mapLayout; struct Tileset primary = *layout.primaryTileset;
        u16 attributes[1] = {MB_TALL_GRASS}; primary.metatileAttributes = attributes;
        layout.primaryTileset = &primary; gMapHeader.mapLayout = &layout;
        gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
        gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
        gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
        memset(gObjectEvents, 0, sizeof(gObjectEvents)); ResetTasks(); ResetSpriteData(); FreeAllSpritePalettes(); StartWeather(); gPlayerAvatar.objectEventId = 0;
        struct ObjectEvent *player = &gObjectEvents[0], *wild = &gObjectEvents[1];
        player->active = TRUE; player->isPlayer = TRUE; player->mapGroup = MAP_GROUP(MAP_ROUTE101); player->mapNum = MAP_NUM(MAP_ROUTE101); player->spriteId = 0;
        player->currentCoords.x = player->previousCoords.x = MAP_OFFSET; player->currentCoords.y = player->previousCoords.y = MAP_OFFSET;
        player->currentElevation = 3; player->facingDirection = DIR_EAST; gSprites[0].inUse = TRUE;
        wild->active = TRUE; wild->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; wild->localId = LOCALID_OW_ENCOUNTER_END;
        wild->mapGroup = MAP_GROUP(MAP_ROUTE101); wild->mapNum = MAP_NUM(MAP_ROUTE101);
        wild->warpArrowSpriteId = ROAMER_COUNT + 2; wild->spriteId = 1; wild->graphicsId = OBJ_EVENT_GFX_SPECIES(POOCHYENA);
        wild->trainerRange_berryTreeId = 5; wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 2;
        wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET; wild->currentElevation = 3; gSprites[1].inUse = TRUE;
        struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 19);
        wild->graphicsId = GetGraphicsIdForMon(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_IS_SHINY),
            GetGenderFromSpeciesAndPersonality(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_PERSONALITY)) == MON_FEMALE);
        ModsWildActorSpawned(wild, &mon);
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)}; CHECK(!actors->at(1, &actor));
        gMain.callback2 = CB2_Overworld; ScriptContext_Init(); UnlockPlayerFieldControls();
        ModsWildActorsUpdate(); ModsWildActorsUpdate();
        s32 coverY = 0; CHECK(ModsWildActorPresentation(wild->spriteId, &coverY) == 1);
        struct RhhScoutingRequest request = {.struct_size = sizeof(request), .action = RHH_SCOUT_OBSERVE,
            .handle = actor.handle, .checks = RHH_PERCEPTION_CHECK_COLLISION, .max_distance = 4};
        CHECK(scout->apply(&request, &result) == RHH_SCOUT_UNAVAILABLE && scout->count() == 0);
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        struct RhhWildActorIndividual id = {.struct_size = sizeof(id)}; CHECK(!actors->individual(actor.handle, &id));
        request.action = RHH_SCOUT_IDENTIFY;
        CHECK(scout->apply(&request, &result) == RHH_SCOUT_NOT_OBSERVED);
        request.action = RHH_SCOUT_OBSERVE;
        wild->offScreen = TRUE; CHECK(scout->apply(&request, &result) == RHH_SCOUT_NOT_VISIBLE); wild->offScreen = FALSE;
        wild->invisible = TRUE; CHECK(scout->apply(&request, &result) == RHH_SCOUT_NOT_VISIBLE); wild->invisible = FALSE;
        u32 tile = MAP_OFFSET * gBackupMapLayout.width + MAP_OFFSET + 1;
        gBackupMapLayout.map[tile] = PACK_COLLISION(1);
        CHECK(scout->apply(&request, &result) == RHH_SCOUT_NOT_VISIBLE); gBackupMapLayout.map[tile] = 0;
        rng_value_t rng = gRngValue; sTrackEncounterStarts = TRUE; sEncounterStartCount = 0;
        CHECK(!scout->preview(&request, &result) && result.changed && result.is_preview
            && result.after.observations == 1 && !result.after.species && !scout->count());
        player->currentCoords.x = player->previousCoords.x = MAP_OFFSET + 1;
        cgb_audio_init(42060); m4aSoundInit();
        CHECK(ModsWildScoutFacing() && scout->count() == 1);
        player->currentCoords.x = player->previousCoords.x = MAP_OFFSET;
        CHECK(ModsWildActorPresentation(wild->spriteId, &coverY) == 2);
        struct RhhScoutingRecord record = {.struct_size = sizeof(record)};
        CHECK(!scout->find(&id, &record) && record.observations == 1 && !record.species && !record.flags);
        request.action = RHH_SCOUT_IDENTIFY;
        CHECK(scout->apply(&request, &result) == RHH_SCOUT_DENIED && !strcmp(result.reason, "observe twice"));
        CHECK(!scout->find(&id, &record) && record.observations == 1 && !record.flags);
        request.action = RHH_SCOUT_OBSERVE; CHECK(!scout->apply(&request, &result));
        request.action = RHH_SCOUT_IDENTIFY; CHECK(!scout->preview(&request, &result) && result.is_preview
            && result.after.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_POOCHYENA));
        CHECK(!scout->find(&id, &record) && !record.flags && !record.species);
        CHECK(!scout->apply(&request, &result) && result.changed && result.after.flags == RHH_SCOUT_IDENTIFIED);
        CHECK(!scout->apply(&request, &result) && !result.changed);
        struct RhhScoutingRecord small = {.struct_size = 1, .observations = 999};
        CHECK(scout->at(0, &small) && small.observations == 999);
        struct RhhWildActorIndividual otherRun = id; ++otherRun.run_id;
        CHECK(scout->find(&otherRun, &record));
        u8 *state = RhhModsState("rhh:scouting_state"); CHECK(state);
        u8 original[48]; memcpy(original, state, sizeof(original));
        memcpy(state + 48, state, 48); u8 *exported = NULL; u32 exportedSize = 0;
        CHECK(RhhModsExport(&exported, &exportedSize));
        memset(state + 48, 0, 48); memset(state + 40, 0xff, 4);
        request.action = RHH_SCOUT_OBSERVE;
        CHECK(scout->apply(&request, &result) == RHH_SCOUT_FULL);
        memcpy(state, original, sizeof(original));
        for (u32 i = 0; i < RHH_SCOUT_MAX_RECORDS; ++i)
        {
            memcpy(state + 48 * i, original, 48);
            ClockTestPut64(state + 48 * i + 8, id.serial + i + 1);
        }
        CHECK(scout->count() == RHH_SCOUT_MAX_RECORDS && !RhhModsExport(&exported, &exportedSize));
        free(exported); exported = NULL;
        CHECK(scout->apply(&request, &result) == RHH_SCOUT_FULL);
        memset(state, 0, 48 * RHH_SCOUT_MAX_RECORDS); memcpy(state, original, sizeof(original));
        CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !sEncounterStartCount);
        sTrackEncounterStarts = FALSE;
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        ClearObjectEvent(wild); CHECK(!scout->find(&id, &record) && record.flags == RHH_SCOUT_IDENTIFIED);
        CHECK(scout->apply(&request, &result) == RHH_SCOUT_INVALID);
        free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
        puts("scouting: visibility, previews, policy, commits and no-op passed");
    }
    NativeSaveClose(); RhhModsUnloadLibraries();
}
