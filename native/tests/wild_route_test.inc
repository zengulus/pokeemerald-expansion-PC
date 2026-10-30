extern bool8 MovementAction_WalkNormalRight_Step0(struct ObjectEvent *, struct Sprite *);
extern bool8 MovementAction_WalkNormalLeft_Step0(struct ObjectEvent *, struct Sprite *);
extern bool8 MovementAction_WalkNormalUp_Step0(struct ObjectEvent *, struct Sprite *);
extern bool8 MovementAction_WalkNormalDown_Step0(struct ObjectEvent *, struct Sprite *);
static void CheckWildQueueMigration(const struct RhhWildBehaviorService *behavior, u64 handle)
{
    u8 *saved = NULL; u32 size = 0; CHECK(!RhhModsExport(&saved, &size));
    u32 at = 24; while (at + 80 <= size && strcmp((const char *)saved + at, "rhh:wild_queue_state"))
        at += 80 + ClockTestGet32(saved + at + 68);
    const u32 oldRecord = 40, v3Record = 40 + RHH_WILD_QUEUE_CAPACITY * 12, newRecord = v3Record + 48;
    u32 newLength = OBJECT_EVENTS_COUNT * newRecord, oldLength = OBJECT_EVENTS_COUNT * oldRecord;
    CHECK(at + 80 + newLength <= size && ClockTestGet32(saved + at + 64) == 4
        && ClockTestGet32(saved + at + 68) == newLength);
    struct RhhWildQueueRouteState guard = {.struct_size = sizeof(guard)};
    struct RhhWildQueueInterruption interruption = {.struct_size = sizeof(interruption)};
    u32 v3Length = OBJECT_EVENTS_COUNT * v3Record, v3Size = size - (newLength - v3Length);
    u8 *v3 = malloc(v3Size); CHECK(v3); memcpy(v3, saved, at + 80);
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot)
        memcpy(v3 + at + 80 + slot * v3Record, saved + at + 80 + slot * newRecord, v3Record);
    memcpy(v3 + at + 80 + v3Length, saved + at + 80 + newLength, size - at - 80 - newLength);
    ClockTestPut32(v3 + at + 64, 3); ClockTestPut32(v3 + at + 68, v3Length); ClockTestEnvelope(v3, v3Size);
    CHECK(!RhhModsImport(v3, v3Size) && !behavior->queue_interruption(handle, &interruption) && interruption.enabled);
    ClockTestPut32(v3 + at + 64, 2);
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot) memset(v3 + at + 80 + slot * v3Record + 34, 0, 3);
    ClockTestEnvelope(v3, v3Size); CHECK(!RhhModsImport(v3, v3Size));
    CHECK(!behavior->queue_route_state(handle, &guard) && guard.guarded
        && !behavior->queue_interruption(handle, &interruption) && !interruption.enabled);
    CHECK(!RhhModsImport(saved, size)); free(v3);
    u32 legacySize = size - (newLength - oldLength); u8 *legacy = malloc(legacySize); CHECK(legacy);
    memcpy(legacy, saved, at + 80);
    for (u32 slot = 0; slot < OBJECT_EVENTS_COUNT; ++slot) {
        memcpy(legacy + at + 80 + slot * oldRecord, saved + at + 80 + slot * newRecord, oldRecord);
        memset(legacy + at + 80 + slot * oldRecord + 33, 0, 4);
    }
    memcpy(legacy + at + 80 + oldLength, saved + at + 80 + newLength, size - at - 80 - newLength);
    ClockTestPut32(legacy + at + 64, 1); ClockTestPut32(legacy + at + 68, oldLength); ClockTestEnvelope(legacy, legacySize);
    legacy[at + 80 + 33] = 1; ClockTestEnvelope(legacy, legacySize);
    CHECK(RhhModsImport(legacy, legacySize));
    CHECK(!behavior->queue_route_state(handle, &guard) && guard.guarded);
    legacy[at + 80 + 33] = 0; ClockTestEnvelope(legacy, legacySize); CHECK(!RhhModsImport(legacy, legacySize));
    struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
    CHECK(!behavior->queue_at(handle, &queue) && queue.count == 4 && queue.revision == 1
        && !behavior->queue_route_state(handle, &guard) && !guard.guarded);
    CHECK(!RhhModsImport(saved, size) && !behavior->queue_route_state(handle, &guard) && guard.guarded);
    free(legacy); free(saved);
}
static void CheckWildQueueArrival(const struct RhhWildBehaviorService *behavior, const struct RhhWildActorService *actors,
                                  struct ObjectEvent *wild, const struct RhhWildRouteRequest *request, RhhContentKey owner)
{
    const struct RhhWildActorService *previousActorTest = sWildActorTest; sWildActorTest = actors;
    u8 *original = NULL; u32 originalSize = 0; CHECK(!RhhModsExport(&original, &originalSize));
    struct ObjectEvent initial = *wild; struct Sprite initialSprite = gSprites[wild->spriteId];
    struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
    struct RhhWildQueueRouteOptions options = {.struct_size = sizeof(options), .expected_revision = 1, .owner = owner,
        .flags = RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL | RHH_WILD_QUEUE_ROUTE_RETAIN_GOAL};
    struct RhhWildQueueRouteResult plan = {.struct_size = sizeof(plan)};
    CHECK(!behavior->queue_route(&options, request, &plan) && plan.change.after.revision == 2);
    struct RhhWildQueueCompletion completion = {.struct_size = sizeof(completion)};
    struct RhhWildQueueCompletion tooSmall = {.struct_size = 1, .on_arrival = 77}, unchanged = tooSmall;
    CHECK(!behavior->queue_completion(request->actor_handle, &completion) && completion.on_arrival && completion.revision == 2
        && behavior->queue_completion(request->actor_handle, &tooSmall) && !memcmp(&tooSmall, &unchanged, sizeof(tooSmall)));
    struct RhhWildQueueGoal goal = {.struct_size = sizeof(goal)};
    CHECK(!behavior->queue_goal(request->actor_handle, &goal) && (goal.flags & RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL));
    u8 *saved = NULL; u32 size = 0; CHECK(!RhhModsExport(&saved, &size));
    u32 at = 24; while (at + 80 <= size && strcmp((const char *)saved + at, "rhh:wild_queue_state"))
        at += 80 + ClockTestGet32(saved + at + 68);
    CHECK(at + 80 <= size && ClockTestGet32(saved + at + 64) == 5);
    ClockTestPut32(saved + at + 64, 4); ClockTestEnvelope(saved, size);
    CHECK(RhhModsImport(saved, size)); /* Old schema cannot carry the new flag. */
    ClockTestPut32(saved + at + 64, 5); ClockTestEnvelope(saved, size);
    CHECK(!RhhModsImport(saved, size)); free(saved);

    wild->inanimate = TRUE;
    ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
    struct RhhWildStepResult pending = {.struct_size = sizeof(pending)};
    CHECK(wild->heldMovementActive && !actors->step_pending(request->actor_handle, &pending)
        && pending.source == RHH_WILD_STEP_QUEUE && !pending.queue_consumed
        && !behavior->queue_at(request->actor_handle, &queue) && queue.count == 4 && queue.revision == 2);
    u8 *inFlight = NULL; u32 inFlightSize = 0; CHECK(!RhhModsExport(&inFlight, &inFlightSize));
    SaveObjectEvents();
    CHECK(!gSaveBlock1Ptr->objectEvents[wild - gObjectEvents].heldMovementActive);
    ObjectEventClearHeldMovement(wild); ModsWildActorsUpdate();
    CHECK(!behavior->queue_at(request->actor_handle, &queue) && queue.count == 4 && queue.revision == 2
        && sLastWildStep.ticket == pending.ticket && sLastWildStep.status == RHH_WILD_STEP_INTERRUPTED
        && !sLastWildStep.queue_consumed);
    CHECK(!RhhModsImport(inFlight, inFlightSize) && actors->step_pending(request->actor_handle, &pending)
        && !behavior->queue_at(request->actor_handle, &queue) && queue.count == 4 && queue.revision == 2);
    free(inFlight);

    *wild = initial; gSprites[wild->spriteId] = initialSprite; wild->inanimate = TRUE;
    ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
    CHECK(wild->heldMovementActive && !actors->step_pending(request->actor_handle, &pending));
    wild->currentCoords.x += 2; wild->previousCoords = wild->currentCoords; wild->heldMovementFinished = TRUE;
    ModsWildActorsUpdate();
    CHECK(!behavior->queue_at(request->actor_handle, &queue) && queue.count == 4 && queue.revision == 2
        && sLastWildStep.ticket == pending.ticket && sLastWildStep.status == RHH_WILD_STEP_DEVIATED
        && !sLastWildStep.queue_consumed);

    *wild = initial; gSprites[wild->spriteId] = initialSprite; wild->inanimate = TRUE;
    ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
    CHECK(wild->heldMovementActive && !actors->step_pending(request->actor_handle, &pending));
    struct RhhWildQueueRequest replace = {.struct_size = sizeof(replace), .actor_handle = request->actor_handle,
        .expected_revision = 2, .owner = owner, .count = 1, .actions = {DIR_NORTH}};
    struct RhhWildQueueChange change = {.struct_size = sizeof(change)};
    CHECK(!behavior->queue_replace(&replace, &change) && change.after.revision == 3);
    CHECK(!behavior->queue_completion(request->actor_handle, &completion) && !completion.on_arrival && completion.revision == 3);
    MovementAction_WalkNormalUp_Step0(wild, &gSprites[wild->spriteId]); ObjectEventUpdateElevation(wild, &gSprites[wild->spriteId]);
    wild->previousCoords = wild->currentCoords; wild->heldMovementFinished = TRUE; ModsWildActorsUpdate();
    CHECK(!behavior->queue_at(request->actor_handle, &queue) && queue.count == 1 && queue.revision == 3
        && sLastWildStep.ticket == pending.ticket && sLastWildStep.status == RHH_WILD_STEP_ARRIVED
        && !sLastWildStep.queue_consumed);

    *wild = initial; gSprites[wild->spriteId] = initialSprite; wild->inanimate = TRUE;
    options.expected_revision = 3; CHECK(!behavior->queue_route(&options, request, &plan) && plan.change.after.revision == 4);
    ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
    CHECK(wild->heldMovementActive && !actors->step_pending(request->actor_handle, &pending)
        && !behavior->queue_at(request->actor_handle, &queue) && queue.count == 4 && queue.revision == 4);
    MovementAction_WalkNormalUp_Step0(wild, &gSprites[wild->spriteId]); ObjectEventUpdateElevation(wild, &gSprites[wild->spriteId]);
    wild->previousCoords = wild->currentCoords; wild->heldMovementFinished = TRUE; ModsWildActorsUpdate();
    CHECK(!behavior->queue_at(request->actor_handle, &queue) && queue.count == 3 && queue.revision == 5
        && sLastWildStep.ticket == pending.ticket && sLastWildStep.status == RHH_WILD_STEP_ARRIVED
        && sLastWildStep.queue_consumed && sLastWildStep.queue_revision == 4);
    CHECK(!behavior->queue_replan(request->actor_handle, 5, owner, &plan)
        && !behavior->queue_goal(request->actor_handle, &goal) && (goal.flags & RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL));
    *wild = initial; gSprites[wild->spriteId] = initialSprite;
    CHECK(!behavior->queue_at(request->actor_handle, &queue));
    options.expected_revision = queue.revision; options.flags = RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL;
    struct RhhWildRouteRequest last = *request; last.target_x = 3; last.target_y = 2; last.stop_distance = 0;
    CHECK(!behavior->queue_route(&options, &last, &plan) && plan.change.after.count == 1);
    u64 revision = plan.change.after.revision; wild->inanimate = TRUE;
    CHECK(!behavior->queue_completion(request->actor_handle, &completion) && completion.on_arrival && completion.revision == revision);
    ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
    CHECK(wild->heldMovementActive && !actors->step_pending(request->actor_handle, &pending));
    wild->active = FALSE; ModsWildActorsUpdate();
    CHECK(sLastWildStep.ticket == pending.ticket && sLastWildStep.status == RHH_WILD_STEP_LOST && !sLastWildStep.queue_consumed);
    *wild = initial; gSprites[wild->spriteId] = initialSprite; wild->inanimate = TRUE;
    CHECK(!behavior->queue_at(request->actor_handle, &queue) && queue.count == 1 && queue.revision == revision);
    ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
    CHECK(wild->heldMovementActive && !actors->step_pending(request->actor_handle, &pending));
    MovementAction_WalkNormalUp_Step0(wild, &gSprites[wild->spriteId]); ObjectEventUpdateElevation(wild, &gSprites[wild->spriteId]);
    wild->previousCoords = wild->currentCoords; wild->heldMovementFinished = TRUE;
    CHECK(!NativeSaveBegin()); ModsWildActorsUpdate();
    CHECK(!behavior->queue_at(request->actor_handle, &queue) && queue.count == 1 && queue.revision == revision
        && !actors->step_pending(request->actor_handle, &pending));
    CHECK(NativeSaveEnd(1, FALSE)); ModsWildActorsUpdate();
    CHECK(!behavior->queue_at(request->actor_handle, &queue) && !queue.count && queue.revision == revision + 1
        && sLastWildStep.ticket == pending.ticket && sLastWildStep.queue_consumed);
    CHECK(!behavior->queue_completion(request->actor_handle, &completion) && !completion.on_arrival && completion.revision == revision + 1);
    saved = NULL; size = 0; CHECK(!RhhModsExport(&saved, &size));
    at = 24; while (at + 80 <= size && strcmp((const char *)saved + at, "rhh:wild_queue_state"))
        at += 80 + ClockTestGet32(saved + at + 68);
    CHECK(at + 80 <= size && ClockTestGet32(saved + at + 64) == 4); free(saved);
    *wild = initial; gSprites[wild->spriteId] = initialSprite;
    CHECK(!RhhModsImport(original, originalSize)); free(original); sWildActorTest = previousActorTest;
}
static void CheckWildRoute(const char *path, const char *module)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    const struct RhhWildActorService *actors = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 6));
    CHECK(actors && actors->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_6_SIZE
        && RHH_WILD_ACTOR_SERVICE_V1_5_SIZE < RHH_WILD_ACTOR_SERVICE_V1_6_SIZE
        && RHH_WILD_ACTOR_SERVICE_V1_6_SIZE < RHH_WILD_ACTOR_SERVICE_V1_7_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 8))
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 9))
        && RHH_WILD_ACTOR_SERVICE_V1_8_SIZE < RHH_WILD_ACTOR_SERVICE_V1_9_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 12))
        && !RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 19)));
    gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
    gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    struct MapLayout layout = *gMapHeader.mapLayout; struct Tileset primary = *layout.primaryTileset;
    u16 attributes[4] = {MB_NORMAL, MB_TALL_GRASS, MB_SIDEWAYS_STAIRS_LEFT_SIDE, MB_SIDEWAYS_STAIRS_RIGHT_SIDE};
    primary.metatileAttributes = attributes;
    layout.primaryTileset = &primary; CHECK(layout.width > 8 && layout.height > 8 && !layout.isFrlg); gMapHeader.mapLayout = &layout;
    gBackupMapLayout.width = layout.width + MAP_OFFSET_W; gBackupMapLayout.height = layout.height + MAP_OFFSET_H;
    size_t count = gBackupMapLayout.width * gBackupMapLayout.height;
    gBackupMapLayout.map = calloc(count, sizeof(u16)); CHECK(gBackupMapLayout.map);
    for (size_t i = 0; i < count; ++i) gBackupMapLayout.map[i] = PACK_ELEVATION(3);
    u32 center = (MAP_OFFSET + 3) * gBackupMapLayout.width + MAP_OFFSET + 3;
    gBackupMapLayout.map[center + 1] |= PACK_COLLISION(1);
    memset(gObjectEvents, 0, sizeof(gObjectEvents)); ResetSpriteData(); gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
    struct ObjectEvent *player = &gObjectEvents[0], *wild = &gObjectEvents[1], *obstacle = &gObjectEvents[2];
    player->active = player->isPlayer = TRUE; player->mapGroup = 1; player->mapNum = 0; player->spriteId = 0; player->currentElevation = 3;
    player->currentCoords.x = player->previousCoords.x = MAP_OFFSET; player->currentCoords.y = player->previousCoords.y = MAP_OFFSET;
    wild->active = TRUE; wild->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; wild->localId = LOCALID_OW_ENCOUNTER_END;
    wild->mapGroup = 1; wild->mapNum = 0; wild->spriteId = 1; wild->graphicsId = OBJ_EVENT_GFX_SPECIES(POOCHYENA);
    wild->trainerRange_berryTreeId = 5; wild->warpArrowSpriteId = ROAMER_COUNT + 2; wild->currentElevation = 3;
    wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 3; wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET + 3;
    wild->currentMetatileBehavior = MB_NORMAL; gSprites[0].inUse = gSprites[1].inUse = TRUE;
    ModsWildActorSpawned(wild, NULL); CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
    gMain.callback2 = CB2_Overworld; ScriptContext_Init(); UnlockPlayerFieldControls();
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)}; CHECK(!actors->at(1, &actor));
    struct RhhWildRouteRequest request = {.struct_size = sizeof(request), .actor_handle = actor.handle,
        .target_x = 6, .target_y = 3, .stop_distance = 1};
    struct RhhWildRouteResult route = {.struct_size = sizeof(route)}, unchanged;
    struct RhhWildRouteTrace trace = {.struct_size = sizeof(trace)}, savedTrace;
    struct ObjectEvent before[OBJECT_EVENTS_COUNT]; memcpy(before, gObjectEvents, sizeof(before));
    struct Sprite savedSprites[2] = {gSprites[0], gSprites[1]}; rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(!actors->route(&request, &route) && route.status == RHH_WILD_ROUTE_FOUND && route.first_direction == RHH_WILD_ACTOR_NORTH
        && route.length == 4 && route.visited <= RHH_WILD_ROUTE_NODE_LIMIT);
    struct RhhWildRouteResult shortRoute = route;
    CHECK(!actors->route_trace(&request, &route, &trace) && !memcmp(&route, &shortRoute, sizeof(route)) && trace.count == 4);
    static const struct RhhWildRouteStep expected[] = {
        {3, 2, DIR_NORTH, 3, DIR_NORTH, 0}, {4, 2, DIR_EAST, 3, DIR_EAST, 0},
        {5, 2, DIR_EAST, 3, DIR_EAST, 0}, {6, 2, DIR_EAST, 3, DIR_EAST, 0},
    };
    CHECK(!memcmp(trace.steps, expected, sizeof(expected)));
    savedTrace = trace; unchanged = route; trace.struct_size = 1; savedTrace.struct_size = 1;
    CHECK(actors->route_trace(&request, &route, &trace) && !memcmp(&route, &unchanged, sizeof(route))
        && !memcmp(&trace, &savedTrace, sizeof(trace)) && actors->route_trace(&request, &route, NULL));
    trace.struct_size = sizeof(trace);
    CHECK(!memcmp(before, gObjectEvents, sizeof(before)) && !memcmp(savedSprites, gSprites, sizeof(savedSprites))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)) && !ScriptContext_IsEnabled());
    const struct RhhWildBehaviorService *behavior = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 9));
    CHECK(behavior && behavior->header.struct_size >= RHH_WILD_BEHAVIOR_SERVICE_V1_9_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 7))
        && RhhModsHostAPI()->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 9))
        && RhhModsHostAPI()->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 10))
        && RhhModsHostAPI()->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 11))
        && RHH_WILD_BEHAVIOR_SERVICE_V1_10_SIZE < RHH_WILD_BEHAVIOR_SERVICE_V1_11_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 12)));
    struct RhhWildQueueRouteOptions options = {.struct_size = sizeof(options), .owner = RhhContentKeyOf("test:wild_route"),
        .flags = RHH_WILD_QUEUE_ROUTE_INTERRUPTIBLE | RHH_WILD_QUEUE_ROUTE_RETAIN_GOAL, .interrupt_priority = 1};
    struct RhhWildQueueRouteResult plan = {.struct_size = sizeof(plan)};
    struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
    CHECK(!behavior->queue_route(&options, &request, &plan) && plan.route.length == 4
        && plan.change.changed && plan.change.after.revision == 1 && plan.change.after.count == 4
        && plan.change.after.actions[0] == DIR_NORTH && plan.change.after.actions[1] == DIR_EAST);
    struct RhhWildQueueGoal goal = {.struct_size = sizeof(goal)};
    CHECK(!behavior->queue_goal(actor.handle, &goal) && goal.kind == RHH_WILD_QUEUE_GOAL_POINT
        && goal.target_x == 6 && goal.target_y == 3 && goal.stop_distance == 1
        && goal.owner == options.owner && goal.owner_available && goal.revision == 1);
    struct RhhModEvent denyGoal = {.struct_size = sizeof(denyGoal), .kind = RHH_MOD_UPDATE, .args = {8}};
    struct RhhModDecision denyDecision = {.struct_size = sizeof(denyDecision), .allowed = 1};
    RhhModsDispatch(&denyGoal, &denyDecision);
    struct RhhWildRouteRequest deniedRoute = request; deniedRoute.target_y = 2;
    options.expected_revision = 1;
    CHECK(behavior->queue_route(&options, &deniedRoute, &plan) == RHH_WILD_QUEUE_DENIED
        && !behavior->queue_goal(actor.handle, &goal) && goal.target_y == 3 && goal.revision == 1);
    denyGoal.args[0] = 0; RhhModsDispatch(&denyGoal, &denyDecision); options.expected_revision = 0;
    struct RhhWildQueueRouteState guard = {.struct_size = sizeof(guard)};
    CHECK(!behavior->queue_route_state(actor.handle, &guard) && guard.guarded && !guard.status
        && guard.origin.x == 3 && guard.origin.y == 3 && guard.landing.x == 3 && guard.landing.y == 2);
    struct RhhWildQueueInterruption interruption = {.struct_size = sizeof(interruption)};
    CHECK(!behavior->queue_interruption(actor.handle, &interruption) && interruption.enabled && interruption.priority == 1
        && interruption.revision == 1);
    CheckWildQueueMigration(behavior, actor.handle);
    CheckWildQueueArrival(behavior, actors, wild, &request, options.owner);
    struct RhhModEvent interruptEvent = {.struct_size = sizeof(interruptEvent), .kind = RHH_MOD_UPDATE, .args = {4}};
    struct RhhModDecision interruptDecision = {.struct_size = sizeof(interruptDecision), .allowed = 1};
    RhhModsDispatch(&interruptEvent, &interruptDecision);
    ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
    CHECK(!wild->heldMovementActive && !behavior->queue_at(actor.handle, &queue) && queue.count == 4 && queue.revision == 1);
    struct ObjectEvent initialInterrupted = *wild; struct Sprite spriteInterrupted = gSprites[1]; wild->inanimate = TRUE;
    interruptEvent.args[0] = 5; RhhModsDispatch(&interruptEvent, &interruptDecision);
    ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
    CHECK(wild->heldMovementActive && !behavior->queue_at(actor.handle, &queue) && queue.count == 4 && queue.revision == 1);
    MovementAction_WalkNormalUp_Step0(wild, &gSprites[1]); ObjectEventClearHeldMovement(wild);
    wild->previousCoords = wild->currentCoords; ModsWildActorsUpdate();
    CHECK(!behavior->queue_route_state(actor.handle, &guard) && guard.reason == RHH_WILD_ROUTE_GUARD_ORIGIN_CHANGED);
    *wild = initialInterrupted; gSprites[1] = spriteInterrupted; ModsWildActorsUpdate();
    CHECK(!behavior->queue_route_state(actor.handle, &guard) && !guard.status);
    wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET - 1;
    CHECK(!behavior->queue_route_state(actor.handle, &guard) && guard.status == RHH_WILD_ACTOR_UNAVAILABLE
        && guard.reason == RHH_WILD_ROUTE_GUARD_UNAVAILABLE);
    wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 3;
    struct RhhModEvent guardEvent = {.struct_size = sizeof(guardEvent), .kind = RHH_MOD_UPDATE, .args = {3}};
    struct RhhModDecision guardDecision = {.struct_size = sizeof(guardDecision), .allowed = 1}; RhhModsDispatch(&guardEvent, &guardDecision);
    wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 2;
    CHECK(!behavior->queue_route_state(actor.handle, &guard) && guard.status == RHH_WILD_ACTOR_ROUTE_CHANGED
        && guard.reason == RHH_WILD_ROUTE_GUARD_ORIGIN_CHANGED);
    ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
    CHECK(!wild->heldMovementActive && !behavior->queue_at(actor.handle, &queue) && queue.count == 4 && queue.revision == 1);
    wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 3;
    wild->facingDirectionLocked = TRUE;
    CHECK(!behavior->queue_route_state(actor.handle, &guard) && guard.status == RHH_WILD_ACTOR_ROUTE_CHANGED
        && guard.reason == RHH_WILD_ROUTE_GUARD_LANDING_CHANGED);
    ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
    CHECK(!wild->heldMovementActive && !behavior->queue_at(actor.handle, &queue) && queue.count == 4 && queue.revision == 1);
    wild->facingDirectionLocked = FALSE;
    CHECK(!behavior->queue_route_state(actor.handle, &guard) && !guard.status);
    guardEvent.args[0] = 0; RhhModsDispatch(&guardEvent, &guardDecision);
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
    CHECK(behavior->queue_route(&options, &request, &plan) == RHH_WILD_QUEUE_STALE);
    options.expected_revision = 1;
    options.owner = RhhContentKeyOf("test:mon_edit_rules");
    CHECK(behavior->queue_route(&options, &request, &plan) == RHH_WILD_QUEUE_CONFLICT);
    options.owner = RhhContentKeyOf("test:wild_route"); options.reserved = 1;
    CHECK(behavior->queue_route(&options, &request, &plan) == RHH_WILD_QUEUE_INVALID); options.reserved = 0;
    options.flags = 16; CHECK(behavior->queue_route(&options, &request, &plan) == RHH_WILD_QUEUE_INVALID);
    options.flags = RHH_WILD_QUEUE_ROUTE_INTERRUPTIBLE; options.interrupt_priority = 1001;
    CHECK(behavior->queue_route(&options, &request, &plan) == RHH_WILD_QUEUE_INVALID); options.interrupt_priority = 1;
    struct RhhWildQueueRouteResult tooSmall = {.struct_size = 1}, unchangedPlan = tooSmall;
    CHECK(behavior->queue_route(&options, &request, &tooSmall) == RHH_WILD_QUEUE_INVALID
        && !memcmp(&tooSmall, &unchangedPlan, sizeof(tooSmall)));
    struct RhhWildRouteRequest failed = request; failed.target_x = 3; failed.target_y = 3; failed.stop_distance = 0;
    CHECK(behavior->queue_route(&options, &failed, &plan) == RHH_WILD_QUEUE_ALREADY_THERE);
    for (int x = 3; x <= 7; ++x)
        gBackupMapLayout.map[(MAP_OFFSET + 4) * gBackupMapLayout.width + MAP_OFFSET + x] |= PACK_COLLISION(1);
    failed.target_x = 7; failed.target_y = 7;
    CHECK(behavior->queue_route(&options, &failed, &plan) == RHH_WILD_QUEUE_ROUTE_TOO_LONG
        && plan.route.length > RHH_WILD_QUEUE_CAPACITY);
    /* An independent module can opt into a guarded prefix. Restore the
     * original mod save state so the remaining short-route tests retain their
     * established revision and load fixture. */
    u8 *prefixSaved = NULL; u32 prefixSavedSize = 0;
    CHECK(!RhhModsExport(&prefixSaved, &prefixSavedSize));
    interruptEvent.args[0] = 7; RhhModsDispatch(&interruptEvent, &interruptDecision);
    CHECK(!behavior->queue_at(actor.handle, &queue) && queue.count == RHH_WILD_QUEUE_CAPACITY && queue.revision == 2
        && !behavior->queue_route_state(actor.handle, &guard) && guard.guarded && !guard.status);
    /* Force a long detour to a single reachable habitat match. */
    u32 habitatWall = (MAP_OFFSET + 4) * gBackupMapLayout.width + MAP_OFFSET + 2;
    u32 habitatTile = (MAP_OFFSET + 5) * gBackupMapLayout.width + MAP_OFFSET + 6;
    gBackupMapLayout.map[habitatWall] |= PACK_COLLISION(1);
    gBackupMapLayout.map[habitatTile] |= 1;
    struct RhhWildHabitatRouteRequest longHabitat = {.struct_size = sizeof(longHabitat), .actor_handle = actor.handle,
        .radius = 5, .all_flags = RHH_HABITAT_GRASS};
    struct RhhWildHabitatRouteResult habitatPreview = {.struct_size = sizeof(habitatPreview)};
    struct RhhWildRouteTrace habitatTrace = {.struct_size = sizeof(habitatTrace)};
    CHECK(!actors->route_habitat_trace(&longHabitat, &habitatPreview, &habitatTrace)
        && habitatPreview.route.status == RHH_WILD_ROUTE_FOUND && habitatTrace.count > RHH_WILD_QUEUE_CAPACITY);
    options.expected_revision = 2; options.flags |= RHH_WILD_QUEUE_ROUTE_PREFIX | RHH_WILD_QUEUE_ROUTE_RETAIN_GOAL;
    CHECK(!behavior->queue_habitat_route(&options, &longHabitat, &plan) && plan.habitat_available
        && plan.route.length == habitatTrace.count && plan.change.after.count == RHH_WILD_QUEUE_CAPACITY
        && plan.change.after.revision == 3 && plan.habitat.target.x == 6 && plan.habitat.target.y == 5);
    for (u32 i = 0; i < RHH_WILD_QUEUE_CAPACITY; ++i)
        CHECK(plan.change.after.actions[i] == habitatTrace.steps[i].direction);
    CHECK(!behavior->queue_route_state(actor.handle, &guard) && guard.guarded && !guard.status);
    CHECK(!behavior->queue_goal(actor.handle, &goal) && goal.kind == RHH_WILD_QUEUE_GOAL_HABITAT
        && goal.radius == 5 && goal.all_flags == RHH_HABITAT_GRASS && goal.revision == 3);
    u8 *goalSaved = NULL; u32 goalSavedSize = 0; CHECK(!RhhModsExport(&goalSaved, &goalSavedSize));
    CHECK(!behavior->queue_replan(actor.handle, 3, options.owner, &plan) && !plan.change.changed
        && plan.change.after.revision == 3);
    CHECK(!RhhModsImport(goalSaved, goalSavedSize) && !behavior->queue_goal(actor.handle, &goal)
        && goal.kind == RHH_WILD_QUEUE_GOAL_HABITAT && goal.radius == 5 && goal.revision == 3);
    free(goalSaved);
    struct ObjectEvent beforePrefixWalk = *wild; struct Sprite beforePrefixSprite = gSprites[1];
    wild->inanimate = TRUE;
    for (u32 i = 0; i < RHH_WILD_QUEUE_CAPACITY; ++i)
    {
        const struct RhhWildRouteStep *step = &habitatTrace.steps[i];
        ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
        CHECK(wild->heldMovementActive && !behavior->queue_at(actor.handle, &queue)
            && queue.count == RHH_WILD_QUEUE_CAPACITY - i - 1 && queue.revision == 4 + i);
        if (step->direction == DIR_NORTH) MovementAction_WalkNormalUp_Step0(wild, &gSprites[1]);
        else if (step->direction == DIR_SOUTH) MovementAction_WalkNormalDown_Step0(wild, &gSprites[1]);
        else if (step->direction == DIR_WEST) MovementAction_WalkNormalLeft_Step0(wild, &gSprites[1]);
        else { CHECK(step->direction == DIR_EAST); MovementAction_WalkNormalRight_Step0(wild, &gSprites[1]); }
        ObjectEventUpdateElevation(wild, &gSprites[1]); ObjectEventClearHeldMovement(wild);
        wild->previousCoords = wild->currentCoords; ModsWildActorsUpdate();
    }
    CHECK(!behavior->queue_goal(actor.handle, &goal) && goal.kind == RHH_WILD_QUEUE_GOAL_HABITAT && goal.revision == 11);
    CHECK(!behavior->queue_replan(actor.handle, 11, options.owner, &plan) && plan.change.after.count
        && plan.change.after.revision == 12 && plan.habitat_available);
    *wild = beforePrefixWalk; gSprites[1] = beforePrefixSprite; ModsWildActorsUpdate();
    gBackupMapLayout.map[habitatWall] &= ~PACK_COLLISION(1); gBackupMapLayout.map[habitatTile] &= ~1;
    CHECK(!RhhModsImport(prefixSaved, prefixSavedSize)); free(prefixSaved);
    options.expected_revision = 1; options.flags = RHH_WILD_QUEUE_ROUTE_INTERRUPTIBLE;
    interruptEvent.args[0] = 0; RhhModsDispatch(&interruptEvent, &interruptDecision);
    for (int x = 3; x <= 7; ++x)
        gBackupMapLayout.map[(MAP_OFFSET + 4) * gBackupMapLayout.width + MAP_OFFSET + x] &= ~PACK_COLLISION(1);
    gBackupMapLayout.map[center - gBackupMapLayout.width] |= PACK_COLLISION(1);
    gBackupMapLayout.map[center + gBackupMapLayout.width] |= PACK_COLLISION(1);
    gBackupMapLayout.map[center - 1] |= PACK_COLLISION(1);
    CHECK(behavior->queue_route(&options, &request, &plan) == RHH_WILD_QUEUE_NO_PATH);
    gBackupMapLayout.map[center - gBackupMapLayout.width] &= ~PACK_COLLISION(1);
    gBackupMapLayout.map[center + gBackupMapLayout.width] &= ~PACK_COLLISION(1);
    gBackupMapLayout.map[center - 1] &= ~PACK_COLLISION(1);
    CHECK(!behavior->queue_at(actor.handle, &queue) && queue.count == 4 && queue.revision == 1);
    /* Replay the reported inputs through the real walking actions and compare
     * each landing and facing; finish each held move before the next input. */
    struct ObjectEvent initialWild = *wild; struct Sprite initialSprite = gSprites[1];
    wild->inanimate = TRUE;
    interruptEvent.args[0] = 6; RhhModsDispatch(&interruptEvent, &interruptDecision);
    for (u32 i = 0; i < trace.count; ++i)
    {
        const struct RhhWildRouteStep *step = &trace.steps[i];
        if (i == 1)
        {
            u32 blocked = (step->y + MAP_OFFSET) * gBackupMapLayout.width + step->x + MAP_OFFSET;
            gBackupMapLayout.map[blocked] |= PACK_COLLISION(1);
            CHECK(!behavior->queue_route_state(actor.handle, &guard) && guard.status == RHH_WILD_ACTOR_BLOCKED
                && guard.reason == RHH_WILD_ROUTE_GUARD_BLOCKED);
            ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
            CHECK(!wild->heldMovementActive && !behavior->queue_at(actor.handle, &queue)
                && queue.count == 3 && queue.revision == 2);
            gBackupMapLayout.map[blocked] &= ~PACK_COLLISION(1);
        }
        ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate();
        CHECK(wild->heldMovementActive && !behavior->queue_at(actor.handle, &queue)
            && queue.count == trace.count - i - 1 && queue.revision == 2 + i);
        if (step->direction == DIR_NORTH) MovementAction_WalkNormalUp_Step0(wild, &gSprites[1]);
        else { CHECK(step->direction == DIR_EAST); MovementAction_WalkNormalRight_Step0(wild, &gSprites[1]); }
        ObjectEventUpdateElevation(wild, &gSprites[1]);
        CHECK(wild->currentCoords.x == MAP_OFFSET + step->x && wild->currentCoords.y == MAP_OFFSET + step->y
            && wild->currentElevation == step->elevation && wild->facingDirection == step->facing);
        ObjectEventClearHeldMovement(wild);
        wild->previousCoords = wild->currentCoords; ModsWildActorsUpdate();
        if (!i) { interruptEvent.args[0] = 0; RhhModsDispatch(&interruptEvent, &interruptDecision); }
    }
    CHECK(!behavior->queue_goal(actor.handle, &goal) && goal.kind == RHH_WILD_QUEUE_GOAL_POINT
        && goal.revision == 5 && goal.target_x == 6 && goal.target_y == 3);
    CHECK(behavior->queue_replan(actor.handle, 4, options.owner, &plan) == RHH_WILD_QUEUE_STALE);
    RhhContentKey otherOwner = RhhContentKeyOf("test:mon_edit_rules");
    CHECK(behavior->queue_replan(actor.handle, 5, otherOwner, &plan) == RHH_WILD_QUEUE_CONFLICT);
    struct RhhWildQueueRequest deniedEdit = {.struct_size = sizeof(deniedEdit), .actor_handle = actor.handle,
        .expected_revision = 5, .owner = otherOwner, .count = 1, .actions = {DIR_NORTH}};
    struct RhhWildQueueChange deniedChange = {.struct_size = sizeof(deniedChange)};
    CHECK(behavior->queue_replace(&deniedEdit, &deniedChange) == RHH_WILD_QUEUE_CONFLICT);
    CHECK(behavior->queue_replan(actor.handle, 5, options.owner, &plan) == RHH_WILD_QUEUE_ALREADY_THERE);
    u8 *arrivedSave = NULL; u32 arrivedSize = 0; CHECK(!RhhModsExport(&arrivedSave, &arrivedSize));
    struct RhhWildQueueRequest clearGoal = {.struct_size = sizeof(clearGoal), .actor_handle = actor.handle,
        .expected_revision = 5, .owner = options.owner};
    CHECK(!behavior->queue_replace(&clearGoal, &deniedChange) && deniedChange.changed && deniedChange.after.revision == 6
        && !behavior->queue_goal(actor.handle, &goal) && !goal.kind && goal.revision == 6);
    CHECK(!RhhModsImport(arrivedSave, arrivedSize) && !behavior->queue_goal(actor.handle, &goal)
        && goal.kind == RHH_WILD_QUEUE_GOAL_POINT && goal.revision == 5);
    free(arrivedSave);
    *wild = initialWild; gSprites[1] = initialSprite;
    request.target_x = 3; request.target_y = 3; request.stop_distance = 0;
    CHECK(!actors->route(&request, &route) && route.status == RHH_WILD_ROUTE_ALREADY_THERE && !route.first_direction && !route.length);
    CHECK(!actors->route_trace(&request, &route, &trace) && route.status == RHH_WILD_ROUTE_ALREADY_THERE && !trace.count);
    request.target_y = 4; CHECK(!actors->route(&request, &route) && route.status == RHH_WILD_ROUTE_FOUND
        && route.first_direction == RHH_WILD_ACTOR_SOUTH && route.length == 1);
    request.target_x = 6; request.target_y = 3; request.stop_distance = 1;
    obstacle->active = TRUE; obstacle->mapGroup = 1; obstacle->mapNum = 0; obstacle->currentElevation = 3;
    obstacle->currentCoords.x = obstacle->previousCoords.x = MAP_OFFSET + 3;
    obstacle->currentCoords.y = obstacle->previousCoords.y = MAP_OFFSET + 2;
    CHECK(!actors->route(&request, &route) && route.status == RHH_WILD_ROUTE_FOUND
        && route.first_direction == RHH_WILD_ACTOR_SOUTH && route.length == 4 && obstacle->active);
    obstacle->active = FALSE;
    gBackupMapLayout.map[center - gBackupMapLayout.width] |= PACK_COLLISION(1);
    gBackupMapLayout.map[center + gBackupMapLayout.width] |= PACK_COLLISION(1);
    gBackupMapLayout.map[center - 1] |= PACK_COLLISION(1);
    CHECK(!actors->route(&request, &route) && route.status == RHH_WILD_ROUTE_NO_PATH && route.visited == 1);
    CHECK(!actors->route_trace(&request, &route, &trace) && route.status == RHH_WILD_ROUTE_NO_PATH && !trace.count);
    gBackupMapLayout.map[center - gBackupMapLayout.width] &= ~PACK_COLLISION(1);
    gBackupMapLayout.map[center + gBackupMapLayout.width] &= ~PACK_COLLISION(1);
    gBackupMapLayout.map[center - 1] &= ~PACK_COLLISION(1);
#if WE_OWE_RESTRICT_METATILE
    gBackupMapLayout.map[center] |= 1; wild->currentMetatileBehavior = MB_TALL_GRASS;
    request.target_x = 3; request.target_y = 4; request.stop_distance = 0;
    CHECK(!actors->route(&request, &route) && route.status == RHH_WILD_ROUTE_NO_PATH && route.visited == 1);
    gBackupMapLayout.map[center] &= ~1; wild->currentMetatileBehavior = MB_NORMAL;
#endif
    /* Exercise all four horizontal stair transitions against the actual
     * movement action, and ensure a diagonal landing occupied by an actor
     * is rejected without changing state. */
    static const struct { u8 start, next, direction; s8 dx, dy; } stairs[] = {
        {0, 2, DIR_EAST, 1, -1}, {2, 0, DIR_WEST, -1, 1},
        {3, 0, DIR_EAST, 1, 1}, {0, 3, DIR_WEST, -1, -1},
    };
    for (u32 i = 0; i < ARRAY_COUNT(stairs); ++i)
    {
        const u32 neighbor = stairs[i].direction == DIR_EAST ? center + 1 : center - 1;
        gBackupMapLayout.map[center] = PACK_ELEVATION(3) | stairs[i].start;
        gBackupMapLayout.map[center - gBackupMapLayout.width] = PACK_ELEVATION(3) | PACK_COLLISION(1);
        gBackupMapLayout.map[center + gBackupMapLayout.width] = PACK_ELEVATION(3) | PACK_COLLISION(1);
        gBackupMapLayout.map[center - 1] = gBackupMapLayout.map[center + 1] = PACK_ELEVATION(3) | PACK_COLLISION(1);
        gBackupMapLayout.map[neighbor] = PACK_ELEVATION(3) | stairs[i].next;
        wild->currentMetatileBehavior = attributes[stairs[i].start]; wild->facingDirection = DIR_SOUTH;
        request.target_x = 3 + stairs[i].dx; request.target_y = 3 + stairs[i].dy; request.stop_distance = 0;
        memcpy(before, gObjectEvents, sizeof(before)); struct Sprite spriteBefore = gSprites[1];
        CHECK(!actors->route(&request, &route) && route.status == RHH_WILD_ROUTE_FOUND
            && route.length == 1 && route.first_direction == stairs[i].direction);
        CHECK(!memcmp(before, gObjectEvents, sizeof(before)) && !memcmp(&spriteBefore, &gSprites[1], sizeof(spriteBefore))
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        u32 landing = center + stairs[i].dy * (s32)gBackupMapLayout.width + stairs[i].dx;
        if (i == 0)
        {
            gBackupMapLayout.map[landing] = PACK_ELEVATION(3) | 2;
            gBackupMapLayout.map[landing + 1] = PACK_ELEVATION(3) | 2;
            gBackupMapLayout.map[landing - gBackupMapLayout.width] = PACK_ELEVATION(3) | PACK_COLLISION(1);
            request.target_x = 5; request.target_y = 1;
            CHECK(!actors->route(&request, &route) && route.status == RHH_WILD_ROUTE_FOUND
                && route.first_direction == DIR_EAST && route.length == 2);
            CHECK(!actors->route_trace(&request, &route, &trace) && trace.count == 2
                && trace.steps[0].x == 4 && trace.steps[0].y == 2 && trace.steps[0].direction == DIR_EAST
                && trace.steps[0].facing == DIR_NORTHEAST && trace.steps[1].x == 5 && trace.steps[1].y == 1
                && trace.steps[1].direction == DIR_EAST && trace.steps[1].facing == DIR_NORTHEAST);
            request.target_x = 4; request.target_y = 2;
            gBackupMapLayout.map[landing + 1] = gBackupMapLayout.map[landing - gBackupMapLayout.width] = PACK_ELEVATION(3);
            gBackupMapLayout.map[landing] = PACK_ELEVATION(3) | 1;
            struct RhhWildHabitatRouteRequest habitat = {.struct_size = sizeof(habitat), .actor_handle = actor.handle,
                .radius = 2, .all_flags = RHH_HABITAT_GRASS};
            struct RhhWildHabitatRouteResult result = {.struct_size = sizeof(result)};
            CHECK(!actors->route_habitat(&habitat, &result) && result.route.status == RHH_WILD_ROUTE_FOUND
                && result.route.first_direction == DIR_EAST && result.route.length == 1
                && result.target.x == request.target_x && result.target.y == request.target_y);
            struct RhhWildHabitatRouteResult original = result;
            CHECK(!actors->route_habitat_trace(&habitat, &result, &trace) && !memcmp(&result, &original, sizeof(result))
                && trace.count == 1 && trace.steps[0].direction == DIR_EAST && trace.steps[0].facing == DIR_NORTHEAST
                && trace.steps[0].x == result.target.x && trace.steps[0].y == result.target.y);
            options.expected_revision = 5;
            CHECK(!behavior->queue_habitat_route(&options, &habitat, &plan) && plan.habitat_available
                && plan.habitat.target.x == 4 && plan.habitat.target.y == 2 && plan.change.after.count == 1
                && plan.change.after.actions[0] == DIR_EAST && plan.change.after.revision == 6);
            CHECK(!behavior->queue_route_state(actor.handle, &guard) && guard.guarded && !guard.status
                && guard.landing.x == 4 && guard.landing.y == 2 && guard.landing.facing == DIR_NORTHEAST);
            struct RhhWildQueueRequest clear = {.struct_size = sizeof(clear), .actor_handle = actor.handle,
                .expected_revision = 6, .owner = options.owner, .count = 1, .actions = {DIR_EAST}};
            struct RhhWildQueueChange cleared = {.struct_size = sizeof(cleared)};
            CHECK(!behavior->queue_replace(&clear, &cleared) && cleared.changed && cleared.after.revision == 7
                && !behavior->queue_route_state(actor.handle, &guard) && !guard.guarded);
            options.expected_revision = 7;
            CHECK(!behavior->queue_habitat_route(&options, &habitat, &plan) && plan.change.after.revision == 8
                && !behavior->queue_route_state(actor.handle, &guard) && guard.guarded);
            clear.expected_revision = 8; clear.count = 0; memset(clear.actions, 0, sizeof(clear.actions));
            CHECK(!behavior->queue_replace(&clear, &cleared) && !cleared.after.count && cleared.after.revision == 9);
            /* A consumer compiled before the appended fields supplies only the
             * original prefix; bytes after it must not affect the policy. */
            options.struct_size = RHH_WILD_QUEUE_ROUTE_OPTIONS_V1_0_SIZE; options.expected_revision = 9;
            options.flags = UINT32_MAX; options.interrupt_priority = INT32_MAX;
            CHECK(!behavior->queue_habitat_route(&options, &habitat, &plan) && plan.change.after.revision == 10
                && !behavior->queue_interruption(actor.handle, &interruption) && !interruption.enabled);
            clear.expected_revision = 10;
            CHECK(!behavior->queue_replace(&clear, &cleared) && !cleared.after.count && cleared.after.revision == 11);
            options.struct_size = sizeof(options); options.flags = RHH_WILD_QUEUE_ROUTE_INTERRUPTIBLE; options.interrupt_priority = 1;
            savedTrace = trace; original = result; habitat.reserved = 1;
            CHECK(actors->route_habitat_trace(&habitat, &result, &trace)
                && !memcmp(&result, &original, sizeof(result)) && !memcmp(&trace, &savedTrace, sizeof(trace)));
        }
        obstacle->active = TRUE; obstacle->mapGroup = 1; obstacle->mapNum = 0; obstacle->currentElevation = 3;
        obstacle->currentCoords.x = obstacle->previousCoords.x = MAP_OFFSET + request.target_x;
        obstacle->currentCoords.y = obstacle->previousCoords.y = MAP_OFFSET + request.target_y;
        CHECK(!actors->route(&request, &route) && route.status == RHH_WILD_ROUTE_NO_PATH
            && actors->step(actor.handle, stairs[i].direction) == RHH_WILD_ACTOR_BLOCKED);
        obstacle->active = FALSE; wild->inanimate = TRUE;
        CHECK(actors->step(actor.handle, stairs[i].direction) == RHH_WILD_ACTOR_OK && wild->heldMovementActive);
        if (stairs[i].direction == DIR_EAST) MovementAction_WalkNormalRight_Step0(wild, &gSprites[1]);
        else MovementAction_WalkNormalLeft_Step0(wild, &gSprites[1]);
        CHECK(wild->currentCoords.x == MAP_OFFSET + request.target_x && wild->currentCoords.y == MAP_OFFSET + request.target_y
            && wild->movementDirection == wild->directionOverwrite);
        ObjectEventClearHeldMovement(wild);
        wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 3;
        wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET + 3;
        wild->inanimate = FALSE; wild->directionOverwrite = DIR_NONE; ModsWildActorsUpdate();
        gBackupMapLayout.map[landing] = PACK_ELEVATION(3);
    }
    gBackupMapLayout.map[center] = gBackupMapLayout.map[center - gBackupMapLayout.width]
        = gBackupMapLayout.map[center + gBackupMapLayout.width] = gBackupMapLayout.map[center - 1] = PACK_ELEVATION(3);
    gBackupMapLayout.map[center + 1] = PACK_ELEVATION(3) | PACK_COLLISION(1);
    wild->currentMetatileBehavior = MB_NORMAL; wild->facingDirection = DIR_SOUTH;
    /* A cardinal input at the map edge may resolve outside the map. */
    u32 top = MAP_OFFSET * gBackupMapLayout.width + MAP_OFFSET + 3;
    wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET;
    gBackupMapLayout.map[top + 1] = PACK_ELEVATION(3) | 2;
    gBackupMapLayout.map[top - 1] = gBackupMapLayout.map[top + gBackupMapLayout.width] = PACK_ELEVATION(3) | PACK_COLLISION(1);
    request.target_x = 4; request.target_y = 0; request.stop_distance = 0;
    CHECK(!actors->route(&request, &route) && route.status == RHH_WILD_ROUTE_NO_PATH
        && actors->step(actor.handle, DIR_EAST) == RHH_WILD_ACTOR_BLOCKED);
    gBackupMapLayout.map[top + 1] = gBackupMapLayout.map[top - 1] = gBackupMapLayout.map[top + gBackupMapLayout.width] = PACK_ELEVATION(3);
    wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET + 3;
    request.target_x = 6; request.target_y = 3; request.stop_distance = 1; unchanged = route;
    request.stop_distance = 2; CHECK(actors->route(&request, &route) && !memcmp(&route, &unchanged, sizeof(route)));
    request.stop_distance = 1; request.target_x = -1; CHECK(actors->route(&request, &route) && !memcmp(&route, &unchanged, sizeof(route)));
    request.target_x = 6; request.reserved[0] = 1; CHECK(actors->route(&request, &route) && !memcmp(&route, &unchanged, sizeof(route)));
    request.reserved[0] = 0; request.actor_handle ^= UINT64_C(0x12340000);
    CHECK(actors->route(&request, &route) && !memcmp(&route, &unchanged, sizeof(route))); request.actor_handle = actor.handle;
    wild->heldMovementActive = TRUE; CHECK(actors->route(&request, &route) && !memcmp(&route, &unchanged, sizeof(route)));
    wild->heldMovementActive = FALSE;
    CHECK(!actors->route(&request, &route) && route.status == RHH_WILD_ROUTE_FOUND
        && route.first_direction == RHH_WILD_ACTOR_NORTH);
    gBackupMapLayout.map[center - gBackupMapLayout.width] |= PACK_COLLISION(1);
    CHECK(actors->step(actor.handle, route.first_direction) == RHH_WILD_ACTOR_BLOCKED && !wild->heldMovementActive);
    gBackupMapLayout.map[center - gBackupMapLayout.width] &= ~PACK_COLLISION(1);
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE, .args = {2}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1}; RhhModsDispatch(&event, &decision);
    event.args[0] = 1; RhhModsDispatch(&event, &decision);
    ModsWorldStep(0, 0, 3, MB_NORMAL); ModsWildActorsUpdate(); CHECK(wild->heldMovementActive);
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; NativeSaveClose(); RhhModsUnloadLibraries();
    puts("wild route: bounded pure planning and real movement passed");
}
static void CheckWildRouteLoad(const char *path, const char *module)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
    const struct RhhWildActorService *actors = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 9));
    const struct RhhWildBehaviorService *behavior = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 9));
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
    struct RhhWildQueueRouteState guard = {.struct_size = sizeof(guard)};
    struct RhhWildQueueInterruption interruption = {.struct_size = sizeof(interruption)};
    struct RhhWildQueueGoal goal = {.struct_size = sizeof(goal)};
    CHECK(actors && behavior && !actors->at(1, &actor) && !behavior->queue_at(actor.handle, &queue)
        && queue.count == 4 && queue.revision == 1 && !behavior->queue_route_state(actor.handle, &guard)
        && guard.guarded && guard.revision == 1 && guard.origin.x == 3 && guard.origin.y == 3
        && guard.landing.x == 3 && guard.landing.y == 2
        && !behavior->queue_interruption(actor.handle, &interruption) && interruption.enabled && interruption.priority == 1
        && !behavior->queue_goal(actor.handle, &goal) && goal.kind == RHH_WILD_QUEUE_GOAL_POINT
        && goal.target_x == 6 && goal.target_y == 3 && goal.revision == 1);
    NativeSaveClose(); RhhModsUnloadLibraries(); puts("wild route: guarded queue restored after reload");
}
