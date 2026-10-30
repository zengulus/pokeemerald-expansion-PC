/* Exercise the public location service through the actual game save flow. */
static void CheckLocationEcology(const char *path, bool32 load)
{
    InitModsForTest(); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhEcologyService *eco = (const void *)host->get_service(
        "rhh:service/ecology", RHH_SERVICE_VERSION(1, 0));
    CHECK(eco && eco->header.struct_size >= RHH_ECOLOGY_SERVICE_V1_0_SIZE);
    RhhContentKey route = host->content_key("rhh:map/route103");
    RhhContentKey species = host->content_key("rhh:species/poochyena");
    RhhContentKey condition = host->content_key("test:ecology/food");
    if (!load)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        CHECK(!eco->set(route, RHH_ECOLOGY_POPULATION, species, 42));
        CHECK(!eco->set(route, RHH_ECOLOGY_CONDITION, condition, -17));
        CHECK(!eco->set(route, RHH_ECOLOGY_OBSERVATIONS, species, 3));
        gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE103);
        gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE103);
        gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE103)][MAP_NUM(MAP_ROUTE103)];
        struct WildPokemon table[2] = {{.species = SPECIES_POOCHYENA}, {.species = SPECIES_ZIGZAGOON}};
        const u16 base[2] = {10, 20}; u16 weights[2] = {0};
        CHECK(ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
                              0, 0, 0, 0, 2, base, weights) && weights[0] == 420 && weights[1] == 20);
        RhhContentKey other = host->content_key("rhh:species/zigzagoon");
        CHECK(!eco->set(route, RHH_ECOLOGY_POPULATION, species, 0));
        CHECK(ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
                              0, 0, 0, 0, 2, base, weights) && weights[0] == 0 && weights[1] == 20);
        CHECK(!eco->set(route, RHH_ECOLOGY_POPULATION, other, 0));
        CHECK(!ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
                               0, 0, 0, 0, 2, base, weights));
        CHECK(!eco->remove(route, RHH_ECOLOGY_POPULATION, other));
        CHECK(!eco->set(route, RHH_ECOLOGY_POPULATION, species, 42));
        gWeatherPtr->currWeather = WEATHER_NONE;
        RhhModsEnvironmentQueueBattle(RHH_WEATHER_SUN, RHH_TERRAIN_GRASSY);
        RhhModsEnvironmentOnFrame();
        const struct RhhWorldEnvironmentService *env = (const void *)host->get_service(
            "rhh:service/world_environment", RHH_SERVICE_VERSION(1, 6));
        struct RhhWorldEnvironmentTiming timing = {.struct_size = sizeof(timing)};
        CHECK(env && !env->timing(route, RHH_WORLD_WEATHER, &timing) && timing.source == RHH_TIME_SYSTEM);
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
    }
    else
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
        int64_t value;
        CHECK(!eco->get(route, RHH_ECOLOGY_POPULATION, species, &value) && value == 42);
        CHECK(!eco->get(route, RHH_ECOLOGY_CONDITION, condition, &value) && value == -17);
        CHECK(!eco->get(route, RHH_ECOLOGY_OBSERVATIONS, species, &value) && value == 3);
        const struct RhhWorldEnvironmentService *env = (const void *)host->get_service(
            "rhh:service/world_environment", RHH_SERVICE_VERSION(1, 6));
        struct RhhWorldEnvironmentView view = {.struct_size = sizeof(view)};
        CHECK(env && !env->inspect(route, &view) && view.environment.weather.value == RHH_WEATHER_SUN
            && view.environment.terrain.value == RHH_TERRAIN_GRASSY
            && view.weather_timing.source == RHH_TIME_SYSTEM && view.terrain_timing.source == RHH_TIME_SYSTEM);
    }
    NativeSaveClose(); RhhModsShutdown();
    puts(load ? "location ecology: game save reloaded" : "location ecology: game save written");
}
