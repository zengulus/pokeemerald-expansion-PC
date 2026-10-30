static const struct RhhHabitatService *sNaturalWeatherService;
static struct RhhNaturalWeatherChange sNaturalWeatherLast;
static u32 sNaturalWeatherCount;

static void ObserveNaturalWeather(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhNaturalWeatherChange change = {.struct_size = sizeof(change)}, small = {.struct_size = 1, .step = 99}, saved = small;
    CHECK(!sNaturalWeatherService->natural_weather_change(&change)
        && sNaturalWeatherService->natural_weather_change(&small) && !memcmp(&small, &saved, sizeof(small))
        && event->event_key == change.map && event->ordinal == change.sequence
        && event->args[0] == change.before_natural && event->args[1] == change.after_natural
        && event->args[2] == change.before_engine && event->args[3] == change.after_engine);
    sNaturalWeatherLast = change; ++sNaturalWeatherCount;
    RhhModsObserveNaturalWeather(); /* Reentrant sampling must not notify twice. */
    (void)Random(); (void)Random2();
}
static const struct RhhModHook sNaturalWeatherHook = {
    sizeof(struct RhhModHook), RHH_NATURAL_WEATHER_CHANGED, RHH_OBSERVE, 0, NULL, ObserveNaturalWeather
};
static const struct RhhModDefinition sNaturalWeatherMod = {.struct_size = sizeof(sNaturalWeatherMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:natural_weather", .hooks = &sNaturalWeatherHook, .hook_count = 1};

static void CheckNaturalWeather(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sNaturalWeatherMod) && !RhhModsFinalize());
    sNaturalWeatherService = (const void *)RhhModsHostAPI()->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 4));
    const struct RhhWorldEnvironmentService *environment = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    CHECK(sNaturalWeatherService && environment && mons
        && RHH_HABITAT_SERVICE_V1_3_SIZE < RHH_HABITAT_SERVICE_V1_4_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 3)) == &sNaturalWeatherService->header
        && !RhhModsHostAPI()->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 8)));
    struct RhhNaturalWeatherChange outside = {.struct_size = sizeof(outside), .step = 99}, saved = outside;
    CHECK(sNaturalWeatherService->natural_weather_change(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    gModDraft = (struct ModConfig){0}; ModsNewGame(7);
    gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    gMain.inBattle = FALSE; gWeatherPtr->currWeather = WEATHER_NONE; sNaturalWeatherCount = 0;
    RhhModsObserveNaturalWeather(); RhhModsObserveNaturalWeather(); CHECK(!sNaturalWeatherCount);
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    gWeatherPtr->currWeather = WEATHER_RAIN; RhhModsObserveNaturalWeather();
    CHECK(sNaturalWeatherCount == 1 && sNaturalWeatherLast.sequence == 0 && sNaturalWeatherLast.run_id == RhhModsRunId()
        && sNaturalWeatherLast.step_available && !sNaturalWeatherLast.step
        && sNaturalWeatherLast.before_natural == RHH_NATURAL_WEATHER_NONE
        && sNaturalWeatherLast.after_natural == RHH_NATURAL_WEATHER_RAIN
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    RhhModsObserveNaturalWeather(); CHECK(sNaturalWeatherCount == 1);
    gWeatherPtr->currWeather = WEATHER_FOG_HORIZONTAL; RhhModsObserveNaturalWeather();
    gWeatherPtr->currWeather = WEATHER_FOG_DIAGONAL; RhhModsObserveNaturalWeather();
    CHECK(sNaturalWeatherCount == 3 && sNaturalWeatherLast.sequence == 2
        && sNaturalWeatherLast.before_natural == RHH_NATURAL_WEATHER_FOG
        && sNaturalWeatherLast.after_natural == RHH_NATURAL_WEATHER_FOG
        && sNaturalWeatherLast.before_engine == WEATHER_FOG_HORIZONTAL && sNaturalWeatherLast.after_engine == WEATHER_FOG_DIAGONAL);

    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_RAIN_DANCE, 0); gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)}; CHECK(!mons->party_at(0, &mon));
    struct RhhWorldEnvironmentRequest request = {.struct_size = sizeof(request), .kind = RHH_WORLD_WEATHER,
        .map = sNaturalWeatherLast.map, .owner = RhhContentKeyOf("test:natural_weather"),
        .move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE), .actor_identity = mon.identity, .duration_steps = 10};
    struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result)};
    CHECK(!environment->apply(&request, &result)); RhhModsObserveNaturalWeather(); CHECK(sNaturalWeatherCount == 3);
    gWeatherPtr->currWeather = WEATHER_SUNNY; RhhModsObserveNaturalWeather();
    CHECK(sNaturalWeatherCount == 4 && sNaturalWeatherLast.after_natural == RHH_NATURAL_WEATHER_SUNNY);
    struct RhhHabitatSample sample = {.struct_size = sizeof(sample), .natural_weather = sNaturalWeatherLast.after_natural,
        .environment_available = TRUE};
    sample.environment.struct_size = sizeof(sample.environment); CHECK(!environment->current(&sample.environment));
    struct RhhHabitatWeather resolved = {.struct_size = sizeof(resolved)};
    CHECK(!sNaturalWeatherService->weather(&sample, &resolved) && resolved.source == RHH_HABITAT_WEATHER_SAVED
        && resolved.kind == RHH_HABITAT_WEATHER_RAIN);

    /* New maps and run/load resets establish a baseline instead of a false change. */
    gSaveBlock1Ptr->location.mapNum = 1; gWeatherPtr->currWeather = WEATHER_SANDSTORM;
    RhhModsObserveNaturalWeather(); CHECK(sNaturalWeatherCount == 4);
    gWeatherPtr->currWeather = WEATHER_RAIN; RhhModsObserveNaturalWeather(); CHECK(sNaturalWeatherCount == 5);
    u64 run = RhhModsRunId(); gWeatherPtr->currWeather = WEATHER_SUNNY;
    ModsAfterLoad(); CHECK(RhhModsRunId() == run);
    RhhModsObserveNaturalWeather(); CHECK(sNaturalWeatherCount == 5);
    gWeatherPtr->currWeather = WEATHER_RAIN; RhhModsObserveNaturalWeather();
    CHECK(sNaturalWeatherCount == 6 && sNaturalWeatherLast.sequence == 0);
    CHECK(sNaturalWeatherService->natural_weather_change(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; RhhModsSetRunContext(0, 0, 0);
    puts("natural weather: coalesced changes, precedence, reset and RNG passed");
}
