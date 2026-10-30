static const struct RhhWorldEnvironmentService *sTimedEnvironment;
static struct RhhWorldEnvironmentTiming sTimedContext;
static struct RhhWorldEnvironmentResult sTimedResult;
static unsigned sTimedNotifications, sTimedConstraints;
static bool32 sTimedDeny, sTimedImpure, sTimedMoveImpure, sTimedObserverImpure;
static u32 sTimedMutation;
static void TimedEnvironmentConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    sTimedContext.struct_size = sizeof(sTimedContext);
    CHECK(!sTimedEnvironment->context_timing(event->args[0], &sTimedContext));
    ++sTimedConstraints;
    if (sTimedMutation == 1) ModsWorldStep(0, 0, 0, 0);
    if (sTimedMutation == 2) ++gSaveBlock1Ptr->location.mapNum;
    if (sTimedMutation == 3) SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_TACKLE, 0);
    if (sTimedMutation == 4) CHECK(!NativeRtcSetFixed("2024-06-01T12:00:01"));
    if (sTimedMutation == 5) gMain.inBattle = TRUE;
    if (sTimedMutation == 6) ((u8 *)RhhModsState("rhh:world_environment"))[12] ^= 1;
    if (sTimedImpure) { (void)Random32(); (void)Random2_32(); }
    if (sTimedDeny) decision->allowed = 0;
}
static void TimedEnvironmentMove(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sTimedMoveImpure) {
        (void)Random32(); (void)Random2_32();
        decision->value = RHH_WORLD_ENV_MOVE(RHH_WORLD_WEATHER, RHH_WEATHER_FOG);
    }
}
static void TimedEnvironmentChanged(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    sTimedResult.struct_size = sizeof(sTimedResult);
    CHECK(!sTimedEnvironment->context(&sTimedResult));
    sTimedContext.struct_size = sizeof(sTimedContext);
    if (sTimedResult.kind) CHECK(!sTimedEnvironment->context_timing(sTimedResult.kind, &sTimedContext));
    else
    {
        CHECK(!sTimedEnvironment->context_timing(RHH_WORLD_WEATHER, &sTimedContext)
            && sTimedContext.clock == RHH_WORLD_ENV_CLOCK_TIME);
        CHECK(!sTimedEnvironment->context_timing(RHH_WORLD_TERRAIN, &sTimedContext)
            && sTimedContext.clock == RHH_WORLD_ENV_CLOCK_STEPS);
    }
    ++sTimedNotifications;
    if (sTimedObserverImpure) { (void)Random32(); (void)Random2_32(); }
}
static const struct RhhModHook sTimedHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_WORLD_ENVIRONMENT, RHH_CONSTRAINT, 0, TimedEnvironmentConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WORLD_ENVIRONMENT_CHANGED, RHH_OBSERVE, 0, NULL, TimedEnvironmentChanged},
    {sizeof(struct RhhModHook), RHH_WORLD_ENVIRONMENT_MOVE, RHH_TRANSFORM, 0, TimedEnvironmentMove, NULL}
};
static const struct RhhModDefinition sTimedMod = {
    .struct_size = sizeof(sTimedMod), .abi_version = RHH_MOD_ABI_V1, .id = "test:timed_environment",
    .hook_count = 3, .hooks = sTimedHooks
};
static const struct RhhModDefinition sSourceOtherMod = {
    .struct_size = sizeof(sSourceOtherMod), .abi_version = RHH_MOD_ABI_V1, .id = "test:environment_other"
};
static u32 TimedRead32(const u8 *p)
{ return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u32 TimedExportSchema(const u8 *data, u32 size)
{
    for (u32 at = 24; at + 80 <= size;)
    {
        u32 length = TimedRead32(data + at + 68);
        CHECK(length <= size - at - 80);
        if (!strcmp((const char *)data + at, "rhh:world_environment")) return TimedRead32(data + at + 64);
        at += 80 + length;
    }
    CHECK(FALSE); return 0;
}
static void CheckTimedEnvironment(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sTimedMod)
        && !RhhModsRegister(&sSourceOtherMod) && !RhhModsFinalize());
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); ClockTestPut64(run + 24, 0);
    RhhModsSetRunContext(123, 456, 0);
    sTimedEnvironment = (const void *)RhhModsHostAPI()->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 6));
    CHECK(sTimedEnvironment && sTimedEnvironment->header.struct_size >= RHH_WORLD_ENVIRONMENT_SERVICE_V1_6_SIZE
        && RHH_WORLD_ENVIRONMENT_SERVICE_V1_5_SIZE < RHH_WORLD_ENVIRONMENT_SERVICE_V1_6_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 7)) == &sTimedEnvironment->header
        && RHH_WORLD_ENVIRONMENT_SERVICE_V1_6_SIZE < RHH_WORLD_ENVIRONMENT_SERVICE_V1_7_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 8)) == &sTimedEnvironment->header
        && RHH_WORLD_ENVIRONMENT_SERVICE_V1_7_SIZE < RHH_WORLD_ENVIRONMENT_SERVICE_V1_8_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 9)) == &sTimedEnvironment->header
        && RHH_WORLD_ENVIRONMENT_SERVICE_V1_8_SIZE == RHH_WORLD_ENVIRONMENT_SERVICE_V1_9_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 10)) == &sTimedEnvironment->header
        && sTimedEnvironment->header.struct_size >= RHH_WORLD_ENVIRONMENT_SERVICE_V1_10_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 11)));
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    CHECK(mons); memset(&gSaveBlock2Ptr->localTimeOffset, 0, sizeof(gSaveBlock2Ptr->localTimeOffset));
    CHECK(!NativeRtcSetFixed("2024-06-01T12:00:00"));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_RAIN_DANCE, 0);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_GRASSY_TERRAIN, 1);
    gPartiesCount[B_TRAINER_PLAYER] = 1; gMain.inBattle = FALSE;
    gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)}; CHECK(!mons->party_at(0, &mon));
    struct RhhWorldEnvironmentTimedRequest request = {.struct_size = sizeof(request), .duration_seconds = 10,
        .request = {.struct_size = sizeof(request.request), .kind = RHH_WORLD_WEATHER,
            .map = RhhContentKeyForId(RHH_MAP, 1 << 8), .owner = RhhContentKeyOf("test:timed_environment"),
            .move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE), .actor_identity = mon.identity}};
    struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result)};
    struct RhhWorldEnvironmentSnapshot view = {.struct_size = sizeof(view)};
    struct RhhWorldEnvironmentTiming timing = {.struct_size = sizeof(timing), .clock = 99}, unchanged = timing;
    struct RhhWorldEnvironmentView detailed = {.struct_size = sizeof(detailed)};
    struct RhhWorldEnvironmentView small = {.struct_size = 1, .reserved = 99}, savedSmall = small;
    u32 count = 99;
    CHECK(!sTimedEnvironment->inspect(request.request.map, &detailed) && detailed.environment.map == request.request.map
        && !detailed.environment.weather.value && !detailed.weather_timing.clock && !detailed.terrain_timing.clock
        && !sTimedEnvironment->list(NULL, 0, &count) && !count
        && sTimedEnvironment->inspect(request.request.map, &small) && !memcmp(&small, &savedSmall, sizeof(small)));
    struct RhhWorldEnvironmentView savedDetailed = detailed;
    CHECK(sTimedEnvironment->inspect(RhhContentKeyOf("test:unknown_map"), &detailed)
        && !memcmp(&detailed, &savedDetailed, sizeof(detailed)));
    CHECK(sTimedEnvironment->timing(request.request.map, RHH_WORLD_WEATHER, &timing)
        && !memcmp(&timing, &unchanged, sizeof(timing)) && sTimedEnvironment->context_timing(RHH_WORLD_WEATHER, &timing));
    u8 *before = NULL, *after = NULL; u32 beforeSize, afterSize;
    CHECK(!RhhModsExport(&before, &beforeSize) && TimedExportSchema(before, beforeSize) == 1);
    rng_value_t rng = gRngValue, rng2 = gRng2Value; struct Pokemon party = gParties[B_TRAINER_PLAYER][0];
    sTimedConstraints = sTimedNotifications = 0; sTimedDeny = sTimedImpure = sTimedMoveImpure = FALSE;
    for (u32 mutation = 1; mutation <= 5; ++mutation) {
        sTimedMutation = mutation;
        CHECK(sTimedEnvironment->apply_timed(&request, &result) == RHH_WORLD_ENV_STALE
            && !sTimedNotifications && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        sTimedMutation = 0; gMain.inBattle = FALSE; gSaveBlock1Ptr->location.mapNum = 0;
        gParties[B_TRAINER_PLAYER][0] = party; ClockTestPut64(run + 24, 0);
        CHECK(!NativeRtcSetFixed("2024-06-01T12:00:00") && !sTimedEnvironment->at(request.request.map, &view)
            && !view.weather.value && !view.terrain.value);
    }
    struct RhhWorldEnvironmentMove knownMove = {.struct_size = sizeof(knownMove)};
    sTimedMoveImpure = TRUE;
    CHECK(!sTimedEnvironment->party_move(0, 0, &knownMove) && knownMove.value == RHH_WEATHER_RAIN
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sTimedMoveImpure = FALSE; sTimedImpure = TRUE;
    CHECK(sTimedEnvironment->preview_timed(&request, &result) == RHH_WORLD_ENV_UNAVAILABLE
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)) && !sTimedNotifications);
    sTimedDeny = TRUE;
    CHECK(sTimedEnvironment->apply_timed(&request, &result) == RHH_WORLD_ENV_DENIED
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)) && !sTimedNotifications);
    sTimedDeny = sTimedImpure = FALSE;
    CHECK(!sTimedEnvironment->preview_timed(&request, &result) && result.is_preview
        && result.after.weather.value == RHH_WEATHER_RAIN && result.after.weather.expires_step == UINT64_MAX
        && sTimedContext.clock == RHH_WORLD_ENV_CLOCK_TIME && sTimedContext.source == RHH_TIME_REAL
        && sTimedContext.time_available && sTimedContext.expires - sTimedContext.started == 10 && !sTimedNotifications);
    struct RhhWorldEnvironmentQuote quote = {.struct_size = sizeof(quote)};
    CHECK(!sTimedEnvironment->preview_timed_checked(&request, &quote) && quote.timed
        && quote.after_timing.clock == RHH_WORLD_ENV_CLOCK_TIME && quote.after_timing.expires - quote.after_timing.started == 10);
    CHECK(!NativeRtcSetFixed("2024-06-01T11:59:59")
        && sTimedEnvironment->apply_timed_checked(&request, &quote, &result) == RHH_WORLD_ENV_STALE);
    CHECK(!NativeRtcSetFixed("2024-06-01T12:00:01")
        && !sTimedEnvironment->apply_timed_checked(&request, &quote, &result)
        && !sTimedEnvironment->timing(request.request.map, RHH_WORLD_WEATHER, &timing)
        && timing.started == quote.after_timing.started + 1 && timing.expires - timing.started == 10);
    CHECK(!sTimedEnvironment->preview_timed_checked(&request, &quote));
    CHECK(!NativeRtcSetFixed("2024-06-01T12:00:02") && !sTimedEnvironment->apply_timed(&request, &result));
    CHECK(sTimedEnvironment->apply_timed_checked(&request, &quote, &result) == RHH_WORLD_ENV_STALE);
    struct RhhWorldEnvironmentTimedRequest otherTest = request;
    otherTest.request.kind = RHH_WORLD_TERRAIN; otherTest.request.move = RhhContentKeyForId(RHH_MOVE, MOVE_GRASSY_TERRAIN);
    CHECK(!sTimedEnvironment->apply_timed(&otherTest, &result)
        && !sTimedEnvironment->preview_timed_checked(&request, &quote)
        && quote.other_before_timing.clock == RHH_WORLD_ENV_CLOCK_TIME);
    CHECK(!NativeRtcSetFixed("2024-06-01T12:00:03") && !sTimedEnvironment->apply_timed(&otherTest, &result));
    CHECK(sTimedEnvironment->apply_timed_checked(&request, &quote, &result) == RHH_WORLD_ENV_STALE);
    struct RhhWorldEnvironmentRequest clearTest = request.request;
    clearTest.move = 0; clearTest.actor_identity = 0; clearTest.duration_steps = 0;
    CHECK(!sTimedEnvironment->apply(&clearTest, &result));
    clearTest.kind = RHH_WORLD_TERRAIN;
    CHECK(!sTimedEnvironment->apply(&clearTest, &result) && !NativeRtcSetFixed("2024-06-01T12:00:00"));
    sTimedConstraints = sTimedNotifications = 0;
    CHECK(!RhhModsExport(&after, &afterSize) && beforeSize == afterSize && !memcmp(before, after, beforeSize));
    free(after); free(before);
    request.duration_seconds = 0;
    CHECK(sTimedEnvironment->apply_timed(&request, &result) == RHH_WORLD_ENV_INVALID);
    request.duration_seconds = 10; request.request.duration_steps = 1;
    CHECK(sTimedEnvironment->apply_timed(&request, &result) == RHH_WORLD_ENV_INVALID); request.request.duration_steps = 0;
    sTimedDeny = TRUE; CHECK(sTimedEnvironment->apply_timed(&request, &result) == RHH_WORLD_ENV_DENIED);
    sTimedDeny = FALSE; sTimedObserverImpure = TRUE;
    rng = gRngValue; rng2 = gRng2Value;
    CHECK(!sTimedEnvironment->apply_timed(&request, &result) && sTimedNotifications == 1
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sTimedObserverImpure = FALSE;
    u8 *rawEnvironment = RhhModsState("rhh:world_environment"); CHECK(rawEnvironment);
    u8 savedPriorityByte = rawEnvironment[12]; sTimedMutation = 6;
    CHECK(sTimedEnvironment->preview_timed(&request, &result) == RHH_WORLD_ENV_STALE
        && result.is_preview && sTimedNotifications == 1);
    rawEnvironment[12] = savedPriorityByte; sTimedMutation = 0;
    CHECK(!sTimedEnvironment->timing(request.request.map, RHH_WORLD_WEATHER, &timing)
        && timing.clock == RHH_WORLD_ENV_CLOCK_TIME && timing.time_available && timing.expires - timing.started == 10);
    CHECK(!RhhModsExport(&before, &beforeSize) && TimedExportSchema(before, beforeSize) == 2);
    CHECK(!NativeRtcSetFixed("2024-06-01T12:00:09") && !sTimedEnvironment->current(&view)
        && view.weather.value == RHH_WEATHER_RAIN);
    CHECK(!sTimedEnvironment->inspect(request.request.map, &detailed) && detailed.environment.weather.value == RHH_WEATHER_RAIN
        && detailed.weather_timing.clock == RHH_WORLD_ENV_CLOCK_TIME && detailed.weather_timing.time_available
        && detailed.weather_timing.expires - detailed.weather_timing.now == 1 && !detailed.terrain_timing.clock);
    RhhModsEnvironmentOnFrame(); CHECK(sTimedNotifications == 1);
    /* Simulate loading a timer made with the other clock source. */
    u8 *environment = RhhModsState("rhh:world_environment"); CHECK(environment);
    const u32 sourceOffset = 128 * 104 + 16;
    CHECK(environment[sourceOffset] == RHH_TIME_REAL); environment[sourceOffset] = RHH_TIME_FAKE;
    CHECK(!NativeRtcSetFixed("2024-06-01T12:00:11") && !sTimedEnvironment->current(&view)
        && view.weather.value == RHH_WEATHER_RAIN && !sTimedEnvironment->timing(request.request.map, RHH_WORLD_WEATHER, &timing)
        && !timing.time_available && timing.source == RHH_TIME_FAKE);
    CHECK(!sTimedEnvironment->inspect(request.request.map, &detailed) && detailed.environment.weather.value == RHH_WEATHER_RAIN
        && !detailed.weather_timing.time_available && detailed.weather_timing.source == RHH_TIME_FAKE
        && !sTimedEnvironment->list(NULL, 0, &count) && count == 1);
    RhhModsEnvironmentOnFrame(); CHECK(sTimedNotifications == 1); environment[sourceOffset] = RHH_TIME_REAL;
    CHECK(!NativeRtcSetFixed("2024-06-01T11:59:50") && !sTimedEnvironment->timing(request.request.map, RHH_WORLD_WEATHER, &timing)
        && timing.time_available && timing.now < timing.started && timing.expires - timing.started == 10);
    gSaveBlock2Ptr->localTimeOffset.hours = 100;
    CHECK(!NativeRtcSetFixed("2024-06-01T12:00:11") && !sTimedEnvironment->current(&view)
        && view.weather.value == RHH_WEATHER_RAIN && !sTimedEnvironment->timing(request.request.map, RHH_WORLD_WEATHER, &timing)
        && !timing.time_available);
    RhhModsEnvironmentOnFrame(); CHECK(sTimedNotifications == 1);
    gSaveBlock2Ptr->localTimeOffset.hours = 0;
    CHECK(!sTimedEnvironment->current(&view) && !view.weather.value);
    u8 *expiredBefore = NULL, *expiredAfter = NULL; u32 expiredBeforeSize = 0, expiredAfterSize = 0;
    CHECK(!RhhModsExport(&expiredBefore, &expiredBeforeSize));
    CHECK(!sTimedEnvironment->inspect(request.request.map, &detailed) && !detailed.environment.weather.value
        && !detailed.weather_timing.clock && !sTimedEnvironment->list(NULL, 0, &count) && !count
        && sTimedNotifications == 1); /* Queries do not commit expiry. */
    CHECK(!RhhModsExport(&expiredAfter, &expiredAfterSize) && expiredBeforeSize == expiredAfterSize
        && !memcmp(expiredBefore, expiredAfter, expiredBeforeSize));
    free(expiredBefore); free(expiredAfter);
    RhhModsEnvironmentOnFrame(); CHECK(sTimedNotifications == 2 && sTimedResult.cause == RHH_WORLD_ENV_EXPIRED
        && sTimedResult.kind == RHH_WORLD_WEATHER && sTimedContext.clock == RHH_WORLD_ENV_CLOCK_TIME);
    RhhModsEnvironmentOnFrame(); CHECK(sTimedNotifications == 2);
    CHECK(!NativeRtcSetFixed("2024-06-01T12:00:05") && !RhhModsImport(before, beforeSize)
        && !sTimedEnvironment->current(&view) && view.weather.value == RHH_WEATHER_RAIN);
    free(before);
    struct RhhWorldEnvironmentRequest step = request.request; step.kind = RHH_WORLD_TERRAIN;
    step.move = RhhContentKeyForId(RHH_MOVE, MOVE_GRASSY_TERRAIN); step.duration_steps = 2;
    CHECK(!sTimedEnvironment->apply(&step, &result) && !sTimedEnvironment->timing(step.map, RHH_WORLD_TERRAIN, &timing)
        && timing.clock == RHH_WORLD_ENV_CLOCK_STEPS && timing.expires - timing.started == 2);
    CHECK(!sTimedEnvironment->inspect(step.map, &detailed) && detailed.environment.weather.value == RHH_WEATHER_RAIN
        && detailed.environment.terrain.value == RHH_TERRAIN_GRASSY && detailed.weather_timing.clock == RHH_WORLD_ENV_CLOCK_TIME
        && detailed.terrain_timing.clock == RHH_WORLD_ENV_CLOCK_STEPS && detailed.terrain_timing.now == detailed.environment.step);
    ModsWorldStep(0, 0, 0, 0); CHECK(!sTimedEnvironment->current(&view) && view.weather.value && view.terrain.value);
    CHECK(!NativeRtcSetFixed("2024-06-01T12:00:10"));
    ModsWorldStep(0, 0, 0, 0);
    CHECK(sTimedResult.cause == RHH_WORLD_ENV_EXPIRED && !sTimedResult.kind
        && !sTimedEnvironment->current(&view) && !view.weather.value && !view.terrain.value);
    CHECK(!sTimedEnvironment->apply(&step, &result));
    CHECK(!RhhModsExport(&before, &beforeSize) && TimedExportSchema(before, beforeSize) == 1
        && !RhhModsImport(before, beforeSize) && !sTimedEnvironment->timing(step.map, RHH_WORLD_TERRAIN, &timing)
        && timing.clock == RHH_WORLD_ENV_CLOCK_STEPS && timing.expires - timing.started == 2); free(before);
    CHECK(!sTimedEnvironment->apply_timed(&request, &result));
    CHECK(!NativeRtcSetFixed("2024-06-01T12:00:21"));
    unsigned notifications = sTimedNotifications;
    CHECK(!sTimedEnvironment->apply_timed(&request, &result) && sTimedNotifications == notifications + 2
        && result.cause == RHH_WORLD_ENV_SET && !result.before.weather.value
        && !sTimedEnvironment->current(&view) && view.weather.value && view.terrain.value);
    RhhContentKey firstMap = request.request.map;
    gSaveBlock1Ptr->location.mapNum = 1; request.request.map = RhhContentKeyForId(RHH_MAP, (1 << 8) | 1);
    CHECK(!sTimedEnvironment->apply_timed(&request, &result)
        && !sTimedEnvironment->timing(firstMap, RHH_WORLD_WEATHER, &timing)
        && timing.clock == RHH_WORLD_ENV_CLOCK_TIME && timing.expires - timing.started == 10
        && !sTimedEnvironment->timing(request.request.map, RHH_WORLD_WEATHER, &timing)
        && timing.clock == RHH_WORLD_ENV_CLOCK_TIME);
    struct RhhWorldEnvironmentView maps[2], untouched[2]; memset(maps, 0x5a, sizeof(maps)); memcpy(untouched, maps, sizeof(maps));
    count = 77;
    CHECK(sTimedEnvironment->list(maps, 1, &count) && count == 77 && !memcmp(maps, untouched, sizeof(maps))
        && sTimedEnvironment->list(NULL, 1, &count) && count == 77
        && !sTimedEnvironment->list(NULL, 0, &count) && count == 2
        && !sTimedEnvironment->list(maps, ARRAY_COUNT(maps), &count) && count == 2
        && maps[0].environment.map < maps[1].environment.map
        && maps[0].environment.step == maps[1].environment.step
        && maps[0].weather_timing.now == maps[1].weather_timing.now
        && maps[0].weather_timing.clock == RHH_WORLD_ENV_CLOCK_TIME && maps[1].weather_timing.clock == RHH_WORLD_ENV_CLOCK_TIME);
    gSaveBlock1Ptr->location.mapNum = 0; request.request.map = firstMap;
    struct RhhWorldEnvironmentRequest clear = request.request;
    clear.move = 0; clear.actor_identity = 0; clear.duration_steps = 0;
    RhhContentKey secondMap = RhhContentKeyForId(RHH_MAP, (1 << 8) | 1);
    if (secondMap < firstMap)
    {
        gSaveBlock1Ptr->location.mapNum = 1; clear.map = secondMap;
        CHECK(!sTimedEnvironment->apply(&clear, &result)
            && !sTimedEnvironment->timing(firstMap, RHH_WORLD_WEATHER, &timing)
            && timing.clock == RHH_WORLD_ENV_CLOCK_TIME && timing.expires - timing.started == 10);
        gSaveBlock1Ptr->location.mapNum = 0; clear.map = firstMap;
        CHECK(!sTimedEnvironment->apply(&clear, &result));
        clear.kind = RHH_WORLD_TERRAIN; CHECK(!sTimedEnvironment->apply(&clear, &result));
    }
    else
    {
        CHECK(!sTimedEnvironment->apply(&clear, &result));
        clear.kind = RHH_WORLD_TERRAIN; CHECK(!sTimedEnvironment->apply(&clear, &result));
        CHECK(!sTimedEnvironment->timing(secondMap, RHH_WORLD_WEATHER, &timing)
            && timing.clock == RHH_WORLD_ENV_CLOCK_TIME && timing.expires - timing.started == 10);
        gSaveBlock1Ptr->location.mapNum = 1; clear.map = secondMap; clear.kind = RHH_WORLD_WEATHER;
        CHECK(!sTimedEnvironment->apply(&clear, &result));
    }
    CHECK(!RhhModsExport(&before, &beforeSize) && TimedExportSchema(before, beforeSize) == 1); free(before);
    CHECK(!memcmp(&party, &gParties[B_TRAINER_PLAYER][0], sizeof(party))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    /* Module-owned sources share transactions without requiring a party move. */
    gSaveBlock1Ptr->location.mapNum = 0;
    struct RhhWorldEnvironmentSourceRequest source = {.struct_size = sizeof(source), .kind = RHH_WORLD_WEATHER,
        .value = RHH_WEATHER_FOG, .clock = RHH_WORLD_ENV_CLOCK_STEPS, .priority = 5,
        .map = RhhContentKeyForId(RHH_MAP, 1 << 8), .owner = RhhContentKeyOf("test:timed_environment"), .duration = 2};
    struct RhhWorldEnvironmentSourceQuote sourceQuote = {.struct_size = sizeof(sourceQuote)};
    notifications = sTimedNotifications;
    CHECK(!sTimedEnvironment->preview_source_checked(&source, &sourceQuote)
        && sourceQuote.transaction.result.after.weather.value == RHH_WEATHER_FOG
        && !sourceQuote.transaction.result.after.weather.move && !sourceQuote.transaction.result.after.weather.actor_identity
        && !sTimedEnvironment->current(&view) && !view.weather.value && notifications == sTimedNotifications);
    struct RhhWorldEnvironmentSourceRequest changed = source; changed.value = RHH_WEATHER_RAIN;
    CHECK(sTimedEnvironment->apply_source_checked(&changed, &sourceQuote, &result) == RHH_WORLD_ENV_STALE
        && !sTimedEnvironment->current(&view) && !view.weather.value);
    sTimedImpure = TRUE;
    CHECK(sTimedEnvironment->apply_source(&source, &result) == RHH_WORLD_ENV_UNAVAILABLE
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sTimedImpure = FALSE; sTimedDeny = TRUE;
    CHECK(sTimedEnvironment->apply_source(&source, &result) == RHH_WORLD_ENV_DENIED);
    sTimedDeny = FALSE;
    CHECK(!sTimedEnvironment->apply_source_checked(&source, &sourceQuote, &result)
        && result.cause == RHH_WORLD_ENV_SET && sTimedNotifications == notifications + 1);
    CHECK(!RhhModsExport(&before, &beforeSize) && TimedExportSchema(before, beforeSize) == 3
        && !RhhModsImport(before, beforeSize) && !sTimedEnvironment->current(&view)
        && view.weather.value == RHH_WEATHER_FOG && !view.weather.move); free(before);
    changed = source; changed.owner = RhhContentKeyOf("test:environment_other"); changed.priority = 4;
    CHECK(sTimedEnvironment->apply_source(&changed, &result) == RHH_WORLD_ENV_CONFLICT);
    changed.value = 0; changed.duration = 0;
    CHECK(sTimedEnvironment->apply_source(&changed, &result) == RHH_WORLD_ENV_CONFLICT);
    changed.value = RHH_WEATHER_RAIN; changed.duration = 2; changed.priority = 6;
    CHECK(!sTimedEnvironment->apply_source(&changed, &result) && result.cause == RHH_WORLD_ENV_REPLACED
        && result.after.weather.owner == changed.owner);
    gMain.inBattle = TRUE;
    CHECK(ModsWorldBattleWeather() == RHH_WEATHER_RAIN);
    gMain.inBattle = FALSE;
    changed.value = 0; changed.duration = 0;
    CHECK(!sTimedEnvironment->apply_source(&changed, &result) && result.cause == RHH_WORLD_ENV_CLEARED
        && !sTimedEnvironment->current(&view) && !view.weather.value);
    CHECK(!sTimedEnvironment->apply_source(&source, &result));
    changed = source; changed.value = 999;
    CHECK(sTimedEnvironment->apply_source(&changed, &result) == RHH_WORLD_ENV_INVALID);
    changed = source; changed.clock = 99;
    CHECK(sTimedEnvironment->apply_source(&changed, &result) == RHH_WORLD_ENV_INVALID);
    changed = source; changed.owner = RhhContentKeyOf("test:not_registered");
    CHECK(sTimedEnvironment->apply_source(&changed, &result) == RHH_WORLD_ENV_INVALID);
    ModsWorldStep(0, 0, 0, 0); ModsWorldStep(0, 0, 0, 0);
    CHECK(!sTimedEnvironment->current(&view) && !view.weather.value && sTimedResult.cause == RHH_WORLD_ENV_EXPIRED);
    CHECK(!NativeRtcSetFixed("2024-06-01T12:01:00"));
    source.kind = RHH_WORLD_TERRAIN; source.value = RHH_TERRAIN_ELECTRIC;
    source.clock = RHH_WORLD_ENV_CLOCK_TIME; source.duration = 3;
    CHECK(!sTimedEnvironment->apply_source(&source, &result)
        && !sTimedEnvironment->timing(source.map, source.kind, &timing)
        && timing.clock == RHH_WORLD_ENV_CLOCK_TIME && timing.expires - timing.started == 3);
    CHECK(!NativeRtcSetFixed("2024-06-01T12:01:03")); RhhModsEnvironmentOnFrame();
    CHECK(!sTimedEnvironment->current(&view) && !view.terrain.value && sTimedResult.cause == RHH_WORLD_ENV_EXPIRED);
    /* Remote transactions use the target map's state, never the loaded map. */
    source.kind = RHH_WORLD_WEATHER; source.value = RHH_WEATHER_SAND;
    source.clock = RHH_WORLD_ENV_CLOCK_STEPS; source.duration = 2;
    source.map = RhhContentKeyForId(RHH_MAP, (1 << 8) | 1);
    CHECK(source.map && source.map != RhhContentKeyForId(RHH_MAP, 1 << 8));
    gMain.inBattle = TRUE;
    CHECK(sTimedEnvironment->apply_source_at(&source, &result) == RHH_WORLD_ENV_UNAVAILABLE);
    gMain.inBattle = FALSE;
    notifications = sTimedNotifications;
    CHECK(sTimedEnvironment->preview_source(&source, &result) == RHH_WORLD_ENV_INVALID
        && sTimedEnvironment->apply_source(&source, &result) == RHH_WORLD_ENV_INVALID
        && !sTimedEnvironment->preview_source_at_checked(&source, &sourceQuote)
        && sourceQuote.transaction.result.before.map == source.map
        && !sTimedEnvironment->at(source.map, &view) && !view.weather.value
        && notifications == sTimedNotifications);
    sTimedDeny = TRUE;
    CHECK(sTimedEnvironment->apply_source_at(&source, &result) == RHH_WORLD_ENV_DENIED);
    sTimedDeny = FALSE;
    CHECK(!sTimedEnvironment->apply_source_at_checked(&source, &sourceQuote, &result)
        && result.after.map == source.map && result.after.weather.value == RHH_WEATHER_SAND
        && sTimedResult.after.map == source.map && sTimedNotifications == notifications + 1
        && gSaveBlock1Ptr->location.mapGroup == 1 && gSaveBlock1Ptr->location.mapNum == 0);
    CHECK(!sTimedEnvironment->current(&view) && !view.weather.value
        && !sTimedEnvironment->at(source.map, &view) && view.weather.value == RHH_WEATHER_SAND);
    CHECK(!RhhModsExport(&before, &beforeSize) && TimedExportSchema(before, beforeSize) == 3
        && !RhhModsImport(before, beforeSize) && !sTimedEnvironment->at(source.map, &view)
        && view.weather.value == RHH_WEATHER_SAND); free(before);
    CHECK(!sTimedEnvironment->preview_source_at_checked(&source, &sourceQuote));
    changed = source; changed.value = RHH_WEATHER_SUN;
    CHECK(!sTimedEnvironment->apply_source_at(&changed, &result)
        && sTimedEnvironment->apply_source_at_checked(&source, &sourceQuote, &result) == RHH_WORLD_ENV_STALE);
    changed.map = RhhContentKeyOf("test:unknown_map");
    CHECK(sTimedEnvironment->apply_source_at(&changed, &result) == RHH_WORLD_ENV_INVALID);
    ModsWorldStep(0, 0, 0, 0); ModsWorldStep(0, 0, 0, 0);
    CHECK(!sTimedEnvironment->at(source.map, &view) && !view.weather.value
        && sTimedResult.cause == RHH_WORLD_ENV_EXPIRED && sTimedResult.before.map == source.map);
    source.clock = RHH_WORLD_ENV_CLOCK_PERMANENT; source.duration = 0;
    changed = source; changed.duration = 1;
    CHECK(sTimedEnvironment->apply_source_at(&changed, &result) == RHH_WORLD_ENV_INVALID);
    changed = source; changed.value = 0;
    CHECK(sTimedEnvironment->apply_source_at(&changed, &result) == RHH_WORLD_ENV_INVALID);
    CHECK(!sTimedEnvironment->preview_source_at_checked(&source, &sourceQuote)
        && sourceQuote.transaction.timed == 2 && sourceQuote.transaction.after_timing.clock == RHH_WORLD_ENV_CLOCK_PERMANENT
        && !sourceQuote.transaction.after_timing.expires
        && !sTimedEnvironment->apply_source_at_checked(&source, &sourceQuote, &result)
        && result.after.weather.expires_step == UINT64_MAX);
    CHECK(!sTimedEnvironment->timing(source.map, source.kind, &timing)
        && timing.clock == RHH_WORLD_ENV_CLOCK_PERMANENT && timing.time_available && !timing.expires);
    CHECK(!RhhModsExport(&before, &beforeSize) && TimedExportSchema(before, beforeSize) == 4
        && !RhhModsImport(before, beforeSize)); free(before);
    notifications = sTimedNotifications;
    ModsWorldStep(0, 0, 0, 0); ModsWorldStep(0, 0, 0, 0);
    CHECK(!NativeRtcSetFixed("2025-06-01T12:00:00")); RhhModsEnvironmentOnFrame();
    CHECK(!sTimedEnvironment->at(source.map, &view) && view.weather.value == RHH_WEATHER_SAND
        && sTimedNotifications == notifications);
    changed = source; changed.clock = RHH_WORLD_ENV_CLOCK_STEPS; changed.value = 0;
    changed.owner = RhhContentKeyOf("test:environment_other");
    CHECK(sTimedEnvironment->apply_source_at(&changed, &result) == RHH_WORLD_ENV_CONFLICT);
    changed.owner = source.owner;
    CHECK(!sTimedEnvironment->apply_source_at(&changed, &result) && result.cause == RHH_WORLD_ENV_CLEARED
        && !sTimedEnvironment->at(source.map, &view) && !view.weather.value);
    CHECK(!sTimedEnvironment->apply_source_at(&source, &result));
    source.clock = RHH_WORLD_ENV_CLOCK_STEPS; source.duration = 2;
    CHECK(!sTimedEnvironment->apply_source_at(&source, &result) && result.cause == RHH_WORLD_ENV_REPLACED
        && !RhhModsExport(&before, &beforeSize) && TimedExportSchema(before, beforeSize) == 3); free(before);
    ModsWorldStep(0, 0, 0, 0); ModsWorldStep(0, 0, 0, 0);
    CHECK(!sTimedEnvironment->at(source.map, &view) && !view.weather.value);
    CHECK(!RhhModsExport(&before, &beforeSize) && TimedExportSchema(before, beforeSize) == 1); free(before);
    RhhModsUnloadLibraries(); puts("timed environment: clocks, expiry, callbacks, migration and rollback passed");
}
