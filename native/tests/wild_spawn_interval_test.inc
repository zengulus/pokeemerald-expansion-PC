#ifdef RHH_WILD_SPAWN_TEST
#include "wild_encounter_ow.h"
extern u32 RhhTestWildSpawnCountdown(void);
extern void RhhTestWildSpawnRecalculate(void);
static void CheckWildSpawnInterval(const char *module)
{
    InitModsForTestWithLibrary(module);
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE103)][MAP_NUM(MAP_ROUTE103)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE103);
    gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE103);
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 0);
    memset(gObjectEvents, 0, sizeof(gObjectEvents)); gPlayerAvatar.objectEventId = 0;
    gObjectEvents[0].active = TRUE; gObjectEvents[0].isPlayer = TRUE;
    gObjectEvents[0].currentCoords.x = gObjectEvents[0].previousCoords.x = MAP_OFFSET + 1;
    gObjectEvents[0].currentCoords.y = gObjectEvents[0].previousCoords.y = MAP_OFFSET + 1;
    gMain.callback2 = CB2_Overworld; ScriptContext_Init(); UnlockPlayerFieldControls();
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_RAIN_DANCE, 0); gPartiesCount[B_TRAINER_PLAYER] = 1;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWorldEnvironmentService *env = (const void *)host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWildEncounterService *wild = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 8));
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)}; CHECK(mons && env && wild && !mons->party_at(0, &mon));
    struct RhhWildSpawnIntervalContext outside = {.struct_size = sizeof(outside), .base_frames = 99}, savedOutside = outside;
    CHECK(wild->spawn_interval_context(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside)));
    rng_value_t before = gRngValue;
    CHECK(ModsWildSpawnInterval(1, RHH_WILD_SPAWN_MINIMUM, WILD_AREA_LAND, 0, 4, 0, 1, 1) == 0);
    CHECK(ModsWildSpawnInterval(2, RHH_WILD_SPAWN_RECALCULATED, WILD_AREA_WATER, 1, 4, 0, 1, 1) == 254);
    CHECK(ModsWildSpawnInterval(3, RHH_WILD_SPAWN_MINIMUM, WILD_AREA_LAND, 0, 4, 0, 1, 1) == 3);
    CHECK(ModsWildSpawnInterval(4, RHH_WILD_SPAWN_MINIMUM, WILD_AREA_LAND, 0, 4, 0, 1, 1) == 4);
    CHECK(ModsWildSpawnInterval(5, RHH_WILD_SPAWN_MINIMUM, WILD_AREA_LAND, 0, 4, 0, 1, 1) == 6);
    CHECK(ModsWildSpawnInterval(6, RHH_WILD_SPAWN_MINIMUM, WILD_AREA_LAND, 0, 4, 0, -1, -1) == 11);
    CHECK(ModsWildSpawnInterval(255, RHH_WILD_SPAWN_MINIMUM, WILD_AREA_LAND, 0, 4, 0, 1, 1) == 255);
    CHECK(!memcmp(&before, &gRngValue, sizeof(before)));
    SetMinimumOWESpawnTimer(); CHECK(RhhTestWildSpawnCountdown() == 30);
    struct RhhWorldEnvironmentRequest request = {.struct_size = sizeof(request), .kind = RHH_WORLD_WEATHER,
        .map = RhhContentKeyForId(RHH_MAP, (MAP_GROUP(MAP_ROUTE103) << 8) | MAP_NUM(MAP_ROUTE103)),
        .owner = RhhContentKeyOf("test:wild_spawn_interval"), .move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE),
        .actor_identity = mon.identity, .duration_steps = 3};
    struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result)}; CHECK(!env->apply(&request, &result));
    SetMinimumOWESpawnTimer(); CHECK(RhhTestWildSpawnCountdown() == 15);
    UpdateOverworldWildEncounter(); CHECK(RhhTestWildSpawnCountdown() == 14);
    struct ObjectEvent *actor = &gObjectEvents[1]; actor->active = TRUE; actor->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER;
    actor->localId = LOCALID_OW_ENCOUNTER_END;
    RhhTestWildSpawnRecalculate(); CHECK(RhhTestWildSpawnCountdown() == 30);
    VarSet(VAR_REPEL_STEP_COUNT, REPEL_LURE_MASK | 10);
    SetMinimumOWESpawnTimer(); CHECK(RhhTestWildSpawnCountdown() == 0);
    RhhTestWildSpawnRecalculate(); CHECK(RhhTestWildSpawnCountdown() == 0);
    for (u32 i = 2; i <= 4; ++i)
    {
        gObjectEvents[i].active = TRUE; gObjectEvents[i].trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER;
        gObjectEvents[i].localId = LOCALID_OW_ENCOUNTER_END - (i - 1);
    }
    RhhTestWildSpawnRecalculate(); CHECK(RhhTestWildSpawnCountdown() == 120);
    VarSet(VAR_REPEL_STEP_COUNT, 0);
    for (u32 i = 1; i <= 4; ++i) gObjectEvents[i].active = FALSE;
    CHECK(!NativeSaveBegin() && NativeSaveTransactionActive());
    SetMinimumOWESpawnTimer(); CHECK(RhhTestWildSpawnCountdown() == 30);
    CHECK(NativeSaveEnd(1, FALSE) && !NativeSaveTransactionActive());
    for (u32 i = 0; i < 3; ++i) ModsWorldStep(1, 1, 0, MB_TALL_GRASS);
    SetMinimumOWESpawnTimer(); CHECK(RhhTestWildSpawnCountdown() == 30);
    CHECK(!memcmp(&before, &gRngValue, sizeof(before)));
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; RhhModsUnloadLibraries();
    puts("wild spawn interval: environment, countdown, lure, constraints and RNG passed");
}
static void WildSpawnExampleGrass(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->value |= RHH_HABITAT_GRASS; }
static const struct RhhModHook sWildSpawnExampleHooks[] = {
    {sizeof(struct RhhModHook), RHH_HABITAT_QUERY, RHH_TRANSFORM, 0, WildSpawnExampleGrass, NULL}
};
static const struct RhhModDefinition sWildSpawnExampleGrass = {
    .struct_size = sizeof(sWildSpawnExampleGrass), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:wild_spawn_grass", .hook_count = 1, .hooks = sWildSpawnExampleHooks
};
#endif
