#include "fishing.h"

static void CheckWildRate(const char *module)
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
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_GRIMER, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    u32 abilityNumber = 0; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ABILITY_NUM, &abilityNumber);
    CHECK(GetMonAbility(&gParties[B_TRAINER_PLAYER][0]) == ABILITY_STENCH);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_RAIN_DANCE, 0);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_GRASSY_TERRAIN, 1); gPartiesCount[B_TRAINER_PLAYER] = 1;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWorldEnvironmentService *env = (const void *)host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWildEncounterService *wild = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 11));
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)}; CHECK(mons && env && wild && !mons->party_at(0, &mon));
    struct RhhWorldEnvironmentRequest request = {.struct_size = sizeof(request), .kind = RHH_WORLD_WEATHER,
        .map = RhhContentKeyForId(RHH_MAP, (MAP_GROUP(MAP_ROUTE103) << 8) | MAP_NUM(MAP_ROUTE103)),
        .owner = RhhContentKeyOf("test:wild_rate"), .move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE),
        .actor_identity = mon.identity, .duration_steps = 20};
    struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result)}; CHECK(!env->apply(&request, &result));
    request.kind = RHH_WORLD_TERRAIN; request.move = RhhContentKeyForId(RHH_MOVE, MOVE_GRASSY_TERRAIN);
    request.duration_steps = 3; CHECK(!env->apply(&request, &result));
    struct RhhWildRateContext outside = {.struct_size = sizeof(outside), .base_rate = 99}, oldOutside = outside;
    CHECK(wild->rate_context(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    struct RhhFishingBiteContext biteOutside = {.struct_size = sizeof(biteOutside), .base_percent = 99}, oldBiteOutside = biteOutside;
    CHECK(wild->fishing_bite_context(&biteOutside) && !memcmp(&biteOutside, &oldBiteOutside, sizeof(biteOutside))
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 10)) == &wild->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 12)) == &wild->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 13)) == &wild->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 14)) == &wild->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 15)) == &wild->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 16)) == &wild->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 17)) == &wild->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 18)) == &wild->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 19)) == &wild->header
        && !host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 21)));
    u8 *biteMode = RhhModsState("test:wild_rate"); CHECK(biteMode);
    gObjectEvents[0].facingDirection = DIR_SOUTH;
    rng_value_t biteRng = gRngValue, biteRng2 = gRng2Value;
    *biteMode = 0; CHECK(ModsFishingBiteRate(OLD_ROD, FALSE, RHH_FISHING_BITE_REGULAR, 25) == 25);
    *biteMode = 1; CHECK(ModsFishingBiteRate(GOOD_ROD, TRUE, RHH_FISHING_BITE_STICKY, 85) == 0);
    *biteMode = 2; CHECK(ModsFishingBiteRate(SUPER_ROD, FALSE, RHH_FISHING_BITE_REGULAR, 75) == 100);
    *biteMode = 3; CHECK(ModsFishingBiteRate(OLD_ROD, FALSE, RHH_FISHING_BITE_REGULAR, 25) == 25);
    *biteMode = 4; CHECK(ModsFishingBiteRate(OLD_ROD, FALSE, RHH_FISHING_BITE_REGULAR, 25) == 25);
    *biteMode = 5; CHECK(ModsFishingBiteRate(OLD_ROD, FALSE, RHH_FISHING_BITE_REGULAR, 25) == 100);
    *biteMode = 6; gObjectEvents[0].currentCoords.y = -10;
    CHECK(ModsFishingBiteRate(OLD_ROD, FALSE, RHH_FISHING_BITE_REGULAR, 25) == 50);
    gObjectEvents[0].currentCoords.y = MAP_OFFSET + 1;
    CHECK(!memcmp(&biteRng, &gRngValue, sizeof(biteRng)) && !memcmp(&biteRng2, &gRng2Value, sizeof(biteRng2)));
    u32 expectedRoll = RandomUniform(RNG_FISHING_BITE, 1, 100); rng_value_t biteAfterRoll = gRngValue; gRngValue = biteRng;
    *biteMode = 0; CHECK(FishingRollForBite(OLD_ROD, FALSE, FALSE) == (expectedRoll <= 25)
        && !memcmp(&biteAfterRoll, &gRngValue, sizeof(biteAfterRoll)));
    gRngValue = biteRng;
    *biteMode = 1; CHECK(!FishingRollForBite(OLD_ROD, FALSE, FALSE) && !memcmp(&biteAfterRoll, &gRngValue, sizeof(biteAfterRoll)));
    gRngValue = biteRng; *biteMode = 2;
    CHECK(FishingRollForBite(OLD_ROD, FALSE, FALSE) && !memcmp(&biteAfterRoll, &gRngValue, sizeof(biteAfterRoll)));
    gRngValue = biteRng;
    const u16 stickyWeights[] = {15, 85};
    bool32 expectedSticky = RandomWeightedArray(RNG_FISHING_GEN3_STICKY, 100, ARRAY_COUNT(stickyWeights), stickyWeights);
    rng_value_t afterSticky = gRngValue;
    u32 legacyRoll = RandomUniform(RNG_FISHING_BITE, 1, 100); rng_value_t afterBoth = gRngValue;
    rng_value_t legacyExpectedRng = expectedSticky ? afterSticky : afterBoth;
    gRngValue = biteRng; *biteMode = 0;
    CHECK(FishingRollForBite(OLD_ROD, TRUE, TRUE) == (expectedSticky || legacyRoll <= 25)
        && !memcmp(&legacyExpectedRng, &gRngValue, sizeof(legacyExpectedRng)));
    gRngValue = biteRng; *biteMode = 1;
    CHECK(!FishingRollForBite(OLD_ROD, TRUE, TRUE) && !memcmp(&afterBoth, &gRngValue, sizeof(afterBoth)));
    gRngValue = biteRng; *biteMode = 2;
    CHECK(FishingRollForBite(OLD_ROD, TRUE, TRUE) && !memcmp(&afterSticky, &gRngValue, sizeof(afterSticky)));
    gRngValue = biteRng; *biteMode = 0;
    rng_value_t before = gRngValue;
    CHECK(ModsWildEncounterRate(1, 400, WILD_AREA_LAND, RHH_WILD_STANDARD, TIME_OF_DAY_DEFAULT, 1, 1, FALSE) == 0);
    CHECK(ModsWildEncounterRate(2, 400, WILD_AREA_WATER, RHH_WILD_STANDARD, TIME_OF_DAY_DEFAULT, 1, 1, FALSE) == RHH_WILD_RATE_MAX);
    CHECK(ModsWildEncounterRate(3, 400, WILD_AREA_LAND, RHH_WILD_PIKE, TIME_OF_DAY_DEFAULT, 1, 1, FALSE) == 400);
    CHECK(ModsWildEncounterRate(4, 400, WILD_AREA_LAND, RHH_WILD_PYRAMID, TIME_OF_DAY_DEFAULT, 1, 1, FALSE) == 400);
    CHECK(ModsWildEncounterRate(5, 400, WILD_AREA_ROCKS, RHH_WILD_STANDARD, TIME_OF_DAY_DEFAULT, 1, 1, TRUE) == 600);
    CHECK(ModsWildEncounterRate(6, 400, WILD_AREA_LAND, RHH_WILD_STANDARD, TIME_OF_DAY_DEFAULT, 1, 1, FALSE) == 800);
    CHECK(ModsWildEncounterRate(7, 400, WILD_AREA_LAND, RHH_WILD_STANDARD, TIME_OF_DAY_DEFAULT, -1, -1, FALSE) == 201);
    CHECK(ModsWildEncounterRate(8, 400, WILD_AREA_LAND, RHH_WILD_STANDARD, TIME_OF_DAY_DEFAULT, 1, 1, FALSE) == 200);
    CHECK(!memcmp(&before, &gRngValue, sizeof(before)));
    Random(); rng_value_t expected = gRngValue; gRngValue = before;
    CHECK(!StandardWildEncounter(MB_TALL_GRASS, MB_TALL_GRASS));
    CHECK(!memcmp(&expected, &gRngValue, sizeof(expected)));
    for (u32 i = 0; i < 3; ++i) ModsWorldStep(1, 1, 0, MB_TALL_GRASS);
    CHECK(ModsWildEncounterRate(8, 400, WILD_AREA_LAND, RHH_WILD_STANDARD, TIME_OF_DAY_DEFAULT, 1, 1, FALSE) == 400);
    for (u32 i = 3; i < 20; ++i) ModsWorldStep(1, 1, 0, MB_TALL_GRASS);
    CHECK(ModsWildEncounterRate(6, 400, WILD_AREA_LAND, RHH_WILD_STANDARD, TIME_OF_DAY_DEFAULT, 1, 1, FALSE) == 400);
    *biteMode = 5; CHECK(ModsFishingBiteRate(OLD_ROD, FALSE, RHH_FISHING_BITE_REGULAR, 25) == 25);
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; RhhModsUnloadLibraries();
    puts("wild rate: environment, constraints, engine roll and RNG passed");
}

