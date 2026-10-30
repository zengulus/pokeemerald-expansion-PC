#include "constants/maps.h"
#include "constants/weather.h"
#include "constants/vars.h"
#include "constants/event_bg.h"
#include "constants/mauville_old_man.h"
#include "berry.h"
#include "item.h"
extern const u32 gObjectEventPic_Man3[];
extern const u16 gObjectEventPal_Npc2[];
extern const u16 gObjectEventPal_Npc1[], gObjectEventPal_Npc3[], gObjectEventPal_Npc4[];
extern const u32 gObjectEventPic_Bulbasaur[];
extern const u16 gOverworldPalette_Bulbasaur[], gShinyOverworldPalette_Bulbasaur[];
static void CheckAuthoredMaps(void)
{
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhAuthoredMapService *maps = (const void *)host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 0));
    const struct RhhOverworldService *world = (const void *)host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 0));
    const struct RhhHabitatService *habitat = (const void *)host->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 0));
    CHECK(maps && world && habitat && maps->header.struct_size >= RHH_AUTHORED_MAP_SERVICE_V1_11_SIZE
        && RHH_AUTHORED_MAP_SERVICE_V1_0_SIZE < RHH_AUTHORED_MAP_SERVICE_V1_1_SIZE
        && RHH_AUTHORED_MAP_SERVICE_V1_1_SIZE < RHH_AUTHORED_MAP_SERVICE_V1_2_SIZE
        && RHH_AUTHORED_MAP_SERVICE_V1_2_SIZE < RHH_AUTHORED_MAP_SERVICE_V1_3_SIZE
        && RHH_AUTHORED_MAP_SERVICE_V1_3_SIZE < RHH_AUTHORED_MAP_SERVICE_V1_4_SIZE
        && RHH_AUTHORED_MAP_SERVICE_V1_4_SIZE < RHH_AUTHORED_MAP_SERVICE_V1_5_SIZE
        && RHH_AUTHORED_MAP_SERVICE_V1_5_SIZE < RHH_AUTHORED_MAP_SERVICE_V1_6_SIZE
        && RHH_AUTHORED_MAP_SERVICE_V1_6_SIZE < RHH_AUTHORED_MAP_SERVICE_V1_7_SIZE
        && RHH_AUTHORED_MAP_SERVICE_V1_7_SIZE < RHH_AUTHORED_MAP_SERVICE_V1_8_SIZE
        && RHH_AUTHORED_MAP_SERVICE_V1_8_SIZE < RHH_AUTHORED_MAP_SERVICE_V1_9_SIZE
        && RHH_AUTHORED_MAP_SERVICE_V1_9_SIZE < RHH_AUTHORED_MAP_SERVICE_V1_10_SIZE
        && RHH_AUTHORED_MAP_SERVICE_V1_10_SIZE < RHH_AUTHORED_MAP_SERVICE_V1_11_SIZE
        && host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 1)) == &maps->header
        && host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 2)) == &maps->header
        && host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 3)) == &maps->header
        && host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 4)) == &maps->header
        && host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 5)) == &maps->header
        && host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 6)) == &maps->header
        && host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 7)) == &maps->header
        && host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 8)) == &maps->header
        && host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 9)) == &maps->header
        && host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 10)) == &maps->header
        && host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 11)) == &maps->header
        && !host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 12)));
    struct RhhAuthoredMap info = {.struct_size = sizeof(info)}, tiny = {.struct_size = 1, .width = 99}, oldTiny = tiny;
    RhhContentKey route = RhhContentKeyForId(RHH_MAP, MAP_ROUTE103), pallet = RhhContentKeyForId(RHH_MAP, MAP_PALLET_TOWN);
    CHECK(route && pallet && maps->map(route, &tiny) && !memcmp(&tiny, &oldTiny, sizeof(tiny)));
    gMain.inBattle = TRUE; rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct MapHeader unchangedHeader = gMapHeader; struct BackupMapLayout unchangedGrid = gBackupMapLayout;
    const struct RhhSpeciesVisualService *visuals = (const void *)host->get_service("rhh:service/species_visuals", RHH_SERVICE_VERSION(1, 0));
    CHECK(visuals && visuals->header.struct_size >= RHH_SPECIES_VISUAL_SERVICE_V1_2_SIZE
        && RHH_SPECIES_VISUAL_SERVICE_V1_0_SIZE < RHH_SPECIES_VISUAL_SERVICE_V1_1_SIZE
        && RHH_SPECIES_VISUAL_SERVICE_V1_1_SIZE < RHH_SPECIES_VISUAL_SERVICE_V1_2_SIZE
        && host->get_service("rhh:service/species_visuals", RHH_SERVICE_VERSION(1, 1)) == &visuals->header
        && host->get_service("rhh:service/species_visuals", RHH_SERVICE_VERSION(1, 2)) == &visuals->header
        && !host->get_service("rhh:service/species_visuals", RHH_SERVICE_VERSION(1, 3)));
    RhhContentKey bulbasaur = RhhContentKeyForId(RHH_SPECIES, SPECIES_BULBASAUR);
    struct RhhSpeciesVisualRequest visualRequest = {.struct_size = sizeof(visualRequest), .species = bulbasaur};
    struct RhhSpeciesVisual visual = {.struct_size = sizeof(visual), .width = 99}, oldVisual = visual;
    struct RhhSpeciesVisual smallVisual = {.struct_size = 1, .width = 99}, oldSmallVisual = smallVisual;
    u8 pokemonPixels[32 * 32 * 4], savedPokemonPixels[sizeof(pokemonPixels)];
    memset(pokemonPixels, 0x5a, sizeof(pokemonPixels)); memcpy(savedPokemonPixels, pokemonPixels, sizeof(pokemonPixels));
    CHECK(bulbasaur && visuals->info(NULL, &visual) && !memcmp(&visual, &oldVisual, sizeof(visual))
        && visuals->info(&visualRequest, &smallVisual) && !memcmp(&smallVisual, &oldSmallVisual, sizeof(smallVisual))
        && visuals->info(&visualRequest, NULL) && visuals->frame_rgba(&visualRequest, 0, NULL, sizeof(pokemonPixels))
        && visuals->frame_rgba(&visualRequest, 0, pokemonPixels, sizeof(pokemonPixels) - 1)
        && !memcmp(pokemonPixels, savedPokemonPixels, sizeof(pokemonPixels)));
    CHECK(!visuals->info(&visualRequest, &visual) && visual.species == bulbasaur && !visual.flags
        && visual.width == 32 && visual.height == 32 && visual.frame_count > 1);
    enum { IMAGE_BYTES = 32 * 32 * 4, FRAME_STRIDE = IMAGE_BYTES + 8 };
    u32 frameIndices[] = {1, 0, 1};
    u8 framesRgba[FRAME_STRIDE * 3], beforeFrames[sizeof(framesRgba)], expectedFrame[IMAGE_BYTES];
    memset(framesRgba, 0x5a, sizeof(framesRgba)); memcpy(beforeFrames, framesRgba, sizeof(framesRgba));
    u32 invalidFrames[] = {0, visual.frame_count};
    CHECK(!visuals->frames_rgba(&visualRequest, NULL, 0, NULL, 0, 0)
        && visuals->frames_rgba(NULL, NULL, 0, NULL, 0, 0)
        && visuals->frames_rgba(&visualRequest, NULL, 1, framesRgba, FRAME_STRIDE, sizeof(framesRgba))
        && visuals->frames_rgba(&visualRequest, frameIndices, 1, NULL, FRAME_STRIDE, sizeof(framesRgba))
        && visuals->frames_rgba(&visualRequest, frameIndices, RHH_SPECIES_VISUAL_BATCH_MAX + 1, framesRgba, FRAME_STRIDE, sizeof(framesRgba))
        && visuals->frames_rgba(&visualRequest, frameIndices, 3, framesRgba, IMAGE_BYTES - 1, sizeof(framesRgba))
        && visuals->frames_rgba(&visualRequest, frameIndices, 3, framesRgba, UINT32_MAX, sizeof(framesRgba))
        && visuals->frames_rgba(&visualRequest, frameIndices, 3, framesRgba, FRAME_STRIDE, FRAME_STRIDE * 2 + IMAGE_BYTES - 1)
        && visuals->frames_rgba(&visualRequest, invalidFrames, 2, framesRgba, FRAME_STRIDE, sizeof(framesRgba))
        && !memcmp(framesRgba, beforeFrames, sizeof(framesRgba)));
    CHECK(!visuals->frames_rgba(&visualRequest, frameIndices, 3, framesRgba, FRAME_STRIDE, sizeof(framesRgba)));
    for (u32 i = 0; i < ARRAY_COUNT(frameIndices); ++i)
    {
        CHECK(!visuals->frame_rgba(&visualRequest, frameIndices[i], expectedFrame, sizeof(expectedFrame))
            && !memcmp(framesRgba + i * FRAME_STRIDE, expectedFrame, IMAGE_BYTES));
        for (u32 j = IMAGE_BYTES; j < FRAME_STRIDE; ++j) CHECK(framesRgba[i * FRAME_STRIDE + j] == 0x5a);
    }
    union { u32 words[IMAGE_BYTES * 3 / sizeof(u32)]; u8 bytes[IMAGE_BYTES * 3]; } alias;
    alias.words[0] = 1; alias.words[1] = 0; alias.words[2] = 1;
    CHECK(!visuals->frames_rgba(&visualRequest, alias.words, 3, alias.bytes, IMAGE_BYTES, sizeof(alias.bytes))
        && !memcmp(alias.bytes, framesRgba, IMAGE_BYTES)
        && !memcmp(alias.bytes + IMAGE_BYTES, framesRgba + FRAME_STRIDE, IMAGE_BYTES)
        && !memcmp(alias.bytes + 2 * IMAGE_BYTES, framesRgba + 2 * FRAME_STRIDE, IMAGE_BYTES));
    u32 maxFrames[RHH_SPECIES_VISUAL_BATCH_MAX] = {0};
    u8 *maxPixels = malloc(RHH_SPECIES_VISUAL_BATCH_MAX * IMAGE_BYTES); CHECK(maxPixels);
    CHECK(!visuals->frames_rgba(&visualRequest, maxFrames, RHH_SPECIES_VISUAL_BATCH_MAX, maxPixels,
                                IMAGE_BYTES, RHH_SPECIES_VISUAL_BATCH_MAX * IMAGE_BYTES)
        && !memcmp(maxPixels, framesRgba + FRAME_STRIDE, IMAGE_BYTES)
        && !memcmp(maxPixels + (RHH_SPECIES_VISUAL_BATCH_MAX - 1) * IMAGE_BYTES, framesRgba + FRAME_STRIDE, IMAGE_BYTES));
    free(maxPixels);
    u32 speciesAnimations = 99, speciesEast = 99, speciesWest = 99, speciesCommands = 99;
    struct RhhSpeciesVisualAnimCommand speciesAnim = {.struct_size = sizeof(speciesAnim), .command = 99}, oldSpeciesAnim = speciesAnim;
    struct RhhSpeciesVisualAnimCommand tinySpeciesAnim = {.struct_size = 1, .command = 99}, oldTinySpeciesAnim = tinySpeciesAnim;
    CHECK(visuals->animation_count(NULL, &speciesAnimations) && speciesAnimations == 99
        && visuals->animation_count(&visualRequest, NULL)
        && !visuals->animation_count(&visualRequest, &speciesAnimations) && speciesAnimations >= 8
        && visuals->face_animation(&visualRequest, 0, &speciesEast) && speciesEast == 99
        && visuals->face_animation(&visualRequest, UINT32_MAX, &speciesEast) && speciesEast == 99
        && !visuals->face_animation(&visualRequest, RHH_WILD_ACTOR_EAST, &speciesEast)
        && !visuals->face_animation(&visualRequest, RHH_WILD_ACTOR_WEST, &speciesWest) && speciesEast != speciesWest
        && visuals->animation_command_count(&visualRequest, speciesAnimations, &speciesCommands) && speciesCommands == 99
        && visuals->animation_command_count(&visualRequest, speciesEast, NULL)
        && !visuals->animation_command_count(&visualRequest, speciesEast, &speciesCommands) && speciesCommands == 2
        && visuals->animation_command(&visualRequest, speciesEast, speciesCommands, &speciesAnim)
        && !memcmp(&speciesAnim, &oldSpeciesAnim, sizeof(speciesAnim))
        && visuals->animation_command(&visualRequest, speciesEast, 0, &tinySpeciesAnim)
        && !memcmp(&tinySpeciesAnim, &oldTinySpeciesAnim, sizeof(tinySpeciesAnim)));
    CHECK(!visuals->animation_command(&visualRequest, speciesEast, 0, &speciesAnim)
        && speciesAnim.species == bulbasaur && speciesAnim.animation == speciesEast && !speciesAnim.command
        && speciesAnim.kind == RHH_AUTHORED_ANIM_FRAME && speciesAnim.image == 4 && speciesAnim.duration == 16
        && speciesAnim.flags == (RHH_AUTHORED_ANIM_HFLIP | RHH_AUTHORED_ANIM_FRAME_AVAILABLE));
    CHECK(!visuals->animation_command(&visualRequest, speciesWest, 0, &speciesAnim) && speciesAnim.image == 4
        && speciesAnim.flags == RHH_AUTHORED_ANIM_FRAME_AVAILABLE
        && !visuals->animation_command(&visualRequest, speciesEast, 1, &speciesAnim)
        && speciesAnim.kind == RHH_AUTHORED_ANIM_JUMP && !speciesAnim.argument
        && speciesAnim.flags == RHH_AUTHORED_ANIM_JUMP_IN_BOUNDS);
    u32 speciesSouthwest = 99;
    CHECK(!visuals->face_animation(&visualRequest, RHH_WILD_ACTOR_SOUTHWEST, &speciesSouthwest) && speciesSouthwest == speciesWest);
    CHECK(visuals->frame_rgba(&visualRequest, visual.frame_count, pokemonPixels, sizeof(pokemonPixels))
        && !memcmp(pokemonPixels, savedPokemonPixels, sizeof(pokemonPixels)));
    u32 decodedSize = GetDecompressedDataSize(gObjectEventPic_Bulbasaur);
    u8 *decodedPokemon = malloc(decodedSize); CHECK(decodedPokemon && decodedSize >= 2 * 32 * 32 / 2
        && TryDecompressDataWithHeaderWram(gObjectEventPic_Bulbasaur, decodedPokemon));
    for (u32 variant = 0; variant < 2; ++variant)
    {
        visualRequest.flags = variant ? RHH_SPECIES_VISUAL_SHINY : 0;
        CHECK(!visuals->info(&visualRequest, &visual) && visual.flags == visualRequest.flags
            && !visuals->frame_rgba(&visualRequest, 1, pokemonPixels, sizeof(pokemonPixels)));
        const u16 *palette = variant ? gShinyOverworldPalette_Bulbasaur : gOverworldPalette_Bulbasaur;
        const u8 *pixels = decodedPokemon + 32 * 32 / 2;
        for (u32 y = 0; y < 32; ++y) for (u32 x = 0; x < 32; ++x)
        {
            u32 offset = ((y / 8) * 4 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
            u32 colorIndex = (pixels[offset] >> ((x & 1) * 4)) & 15, dst = (y * 32 + x) * 4;
            u16 color = palette[colorIndex];
            for (u32 c = 0; c < 3; ++c) { u32 v = (color >> (c * 5)) & 31; CHECK(pokemonPixels[dst + c] == ((v << 3) | (v >> 2))); }
            CHECK(pokemonPixels[dst + 3] == (colorIndex ? 255 : 0));
        }
    }
    free(decodedPokemon);
    visualRequest.flags = RHH_SPECIES_VISUAL_FEMALE;
    CHECK(!visuals->info(&visualRequest, &visual) && visual.flags == RHH_SPECIES_VISUAL_FEMALE);
    visualRequest.flags = RHH_SPECIES_VISUAL_PLACEHOLDER; oldVisual = visual;
    CHECK(visuals->info(&visualRequest, &visual) && !memcmp(&visual, &oldVisual, sizeof(visual)));
    visualRequest.flags = 0; visualRequest.species = RhhContentKeyOf("test:invalid-species");
    CHECK(visuals->info(&visualRequest, &visual) && !memcmp(&visual, &oldVisual, sizeof(visual)));
    CHECK(!maps->map(route, &info) && info.map == route && info.width && info.height
        && !memcmp(&unchangedHeader, &gMapHeader, sizeof(gMapHeader))
        && !memcmp(&unchangedGrid, &gBackupMapLayout, sizeof(gBackupMapLayout)));
    {
        struct RhhAuthoredMapScanRequest request = {.struct_size = sizeof(request), .map = route,
            .width = info.width, .height = info.height, .limit = 37};
        struct RhhAuthoredMapScan page = {.struct_size = sizeof(page)}, before = page;
        struct RhhAuthoredMapScan tinyPage = {.struct_size = 1, .count = 99}, savedTinyPage = tinyPage;
        u32 total = info.width * info.height;
        CHECK(maps->scan(NULL, &page) && maps->scan(&request, NULL)
            && maps->scan(&request, &tinyPage) && !memcmp(&tinyPage, &savedTinyPage, sizeof(tinyPage)));
        request.struct_size = 1; CHECK(maps->scan(&request, &page) && !memcmp(&page, &before, sizeof(page)));
        request.struct_size = sizeof(request); request.map = RhhContentKeyOf("test:unknown_map");
        CHECK(maps->scan(&request, &page) && !memcmp(&page, &before, sizeof(page))); request.map = route;
        request.width = 0; CHECK(maps->scan(&request, &page) && !memcmp(&page, &before, sizeof(page))); request.width = info.width;
        request.height = 0; CHECK(maps->scan(&request, &page) && !memcmp(&page, &before, sizeof(page))); request.height = info.height;
        request.limit = 0; CHECK(maps->scan(&request, &page) && !memcmp(&page, &before, sizeof(page)));
        request.limit = RHH_AUTHORED_MAP_SCAN_MAX + 1; CHECK(maps->scan(&request, &page) && !memcmp(&page, &before, sizeof(page)));
        request.limit = 37; request.first = total + 1; CHECK(maps->scan(&request, &page) && !memcmp(&page, &before, sizeof(page)));
        request.first = 0; request.x = UINT32_MAX; CHECK(maps->scan(&request, &page) && !memcmp(&page, &before, sizeof(page)));
        request.x = 0; request.width = UINT32_MAX; CHECK(maps->scan(&request, &page) && !memcmp(&page, &before, sizeof(page)));
        request.width = info.width; request.reserved = 1; CHECK(maps->scan(&request, &page) && !memcmp(&page, &before, sizeof(page)));
        request.reserved = 0;
        request.limit = RHH_AUTHORED_MAP_SCAN_MAX;
        CHECK(!maps->scan(&request, &page) && page.count == (total < request.limit ? total : request.limit));
        request.limit = 37;
        while (request.first < total)
        {
            u32 first = request.first;
            CHECK(!maps->scan(&request, &page) && page.map == route && page.total == total
                && page.count && page.count <= request.limit && page.next == first + page.count);
            for (u32 i = 0; i < page.count; ++i)
            {
                u32 index = first + i; const struct RhhAuthoredMapCell *cell = &page.cells[i];
                struct RhhAuthoredMapTile single = {.struct_size = sizeof(single)};
                CHECK(cell->x == index % info.width && cell->y == index / info.width);
                int status = maps->tile(route, cell->x, cell->y, &single);
                if (status) { struct RhhAuthoredMapTile zero = {0}; CHECK(cell->status == RHH_AUTHORED_MAP_CELL_UNAVAILABLE && !memcmp(&cell->tile, &zero, sizeof(zero))); }
                else CHECK(cell->status == RHH_AUTHORED_MAP_CELL_OK && !memcmp(&cell->tile, &single, sizeof(single)));
            }
            request.first = page.next;
        }
        CHECK(!maps->scan(&request, &page) && !page.count && page.next == total && page.total == total);
        request.x = 1; request.y = 1; request.width = 2; request.height = 2; request.first = 1; request.limit = 2;
        CHECK(!maps->scan(&request, &page) && page.count == 2 && page.total == 4 && page.next == 3
            && page.cells[0].x == 2 && page.cells[0].y == 1 && page.cells[1].x == 1 && page.cells[1].y == 2);
        CHECK(!memcmp(&unchangedHeader, &gMapHeader, sizeof(gMapHeader))
            && !memcmp(&unchangedGrid, &gBackupMapLayout, sizeof(gBackupMapLayout))
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    }
    const struct RhhAuthoredEventStateService *eventState = (const void *)host->get_service(
        "rhh:service/authored_event_state", RHH_SERVICE_VERSION(1, 0));
    struct RhhAuthoredEventFlag eventFlag = {.struct_size = sizeof(eventFlag)}, unchangedFlag = eventFlag;
    struct RhhDynamicWarp dynamic = {.struct_size = sizeof(dynamic), .saved_x = 99}, unchangedDynamic = dynamic;
    CHECK(eventState && eventState->header.struct_size >= RHH_AUTHORED_EVENT_STATE_SERVICE_V1_1_SIZE
        && RHH_AUTHORED_EVENT_STATE_SERVICE_V1_0_SIZE < RHH_AUTHORED_EVENT_STATE_SERVICE_V1_1_SIZE
        && host->get_service("rhh:service/authored_event_state", RHH_SERVICE_VERSION(1, 1)) == &eventState->header
        && !host->get_service("rhh:service/authored_event_state", RHH_SERVICE_VERSION(1, 2))
        && eventState->dynamic_warp(&dynamic) && !memcmp(&dynamic, &unchangedDynamic, sizeof(dynamic))
        && eventState->object_hide_flag(route, 0, &eventFlag) && !memcmp(&eventFlag, &unchangedFlag, sizeof(eventFlag)));
    RhhModsSetRunContext(7, 123, 0);
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1}; RhhModsDispatch(&event, &decision);
    {
    struct WarpData previous = gSaveBlock1Ptr->dynamicWarp;
    const struct MapHeader *townHeader = Overworld_GetMapHeaderByGroupAndId(MAP_LITTLEROOT_TOWN >> 8, MAP_LITTLEROOT_TOWN & 0xff);
    const struct WarpEvent *arrival = &townHeader->events->warps[0];
    gSaveBlock1Ptr->dynamicWarp = (struct WarpData){.mapGroup = MAP_LITTLEROOT_TOWN >> 8,
        .mapNum = MAP_LITTLEROOT_TOWN & 0xff, .warpId = 0, .x = 7, .y = 8};
    CHECK(!eventState->dynamic_warp(&dynamic) && dynamic.run_id == 123
        && dynamic.map == RhhContentKeyForId(RHH_MAP, MAP_LITTLEROOT_TOWN)
        && dynamic.flags == (RHH_DYNAMIC_WARP_MAP_AVAILABLE | RHH_DYNAMIC_WARP_ARRIVAL_EVENT | RHH_DYNAMIC_WARP_ARRIVAL_IN_BOUNDS)
        && dynamic.saved_x == 7 && dynamic.saved_y == 8 && dynamic.arrival_x == arrival->x && dynamic.arrival_y == arrival->y);
    gSaveBlock1Ptr->dynamicWarp.warpId = -1;
    CHECK(!eventState->dynamic_warp(&dynamic) && dynamic.flags == (RHH_DYNAMIC_WARP_MAP_AVAILABLE
        | RHH_DYNAMIC_WARP_ARRIVAL_COORDS | RHH_DYNAMIC_WARP_ARRIVAL_IN_BOUNDS)
        && dynamic.arrival_x == 7 && dynamic.arrival_y == 8);
    gSaveBlock1Ptr->dynamicWarp.x = 300;
    CHECK(!eventState->dynamic_warp(&dynamic) && !(dynamic.flags & RHH_DYNAMIC_WARP_ARRIVAL_IN_BOUNDS)
        && dynamic.arrival_x == 300);
    gSaveBlock1Ptr->dynamicWarp.x = -1;
    CHECK(!eventState->dynamic_warp(&dynamic) && dynamic.flags == (RHH_DYNAMIC_WARP_MAP_AVAILABLE
        | RHH_DYNAMIC_WARP_ARRIVAL_CENTER | RHH_DYNAMIC_WARP_ARRIVAL_IN_BOUNDS)
        && dynamic.arrival_x == townHeader->mapLayout->width / 2 && dynamic.arrival_y == townHeader->mapLayout->height / 2);
    gSaveBlock1Ptr->dynamicWarp.mapGroup = -1;
    CHECK(!eventState->dynamic_warp(&dynamic) && !dynamic.flags && !dynamic.map && dynamic.map_group == -1);
    unchangedDynamic = dynamic;
    struct RhhDynamicWarp smallDynamic = {.struct_size = 1, .arrival_x = 55}, oldSmallDynamic = smallDynamic;
    CHECK(eventState->dynamic_warp(NULL) && eventState->dynamic_warp(&smallDynamic)
        && !memcmp(&smallDynamic, &oldSmallDynamic, sizeof(smallDynamic)));
    gSaveBlock1Ptr->dynamicWarp = previous;
    }
    struct RhhAuthoredMap savedInfo = info;
    CHECK(maps->map(RhhContentKeyOf("test:missing-map"), &info) && !memcmp(&info, &savedInfo, sizeof(info)));
    u32 connectionCount = 99;
    struct RhhAuthoredConnection connection = {.struct_size = sizeof(connection), .index = 99}, oldConnection = connection;
    struct RhhAuthoredConnection smallConnection = {.struct_size = 1, .index = 99}, oldSmall = smallConnection;
    CHECK(maps->connection_count(0, &connectionCount) && connectionCount == 99
        && maps->connection_count(route, NULL) && !maps->connection_count(route, &connectionCount) && connectionCount == 2
        && maps->connection_at(route, connectionCount, &connection) && !memcmp(&connection, &oldConnection, sizeof(connection))
        && maps->connection_at(route, 0, &smallConnection) && !memcmp(&smallConnection, &oldSmall, sizeof(smallConnection)));
    const struct MapHeader *routeHeader = Overworld_GetMapHeaderByGroupAndId(MAP_ROUTE103 >> 8, MAP_ROUTE103 & 0xff);
    {
    u32 noFlag = UINT32_MAX, savedFlag = UINT32_MAX, temporaryFlag = UINT32_MAX;
    for (u32 i = 0; i < routeHeader->events->objectEventCount; ++i) {
        const struct ObjectEventTemplate *entry = &routeHeader->events->objectEvents[i];
        if (entry->kind != OBJ_KIND_NORMAL) continue;
        if (!entry->flagId) noFlag = i;
        if (entry->flagId == FLAG_HIDE_ROUTE_103_RIVAL) savedFlag = i;
        if (entry->flagId == FLAG_TEMP_12) temporaryFlag = i;
    }
    CHECK(noFlag != UINT32_MAX && savedFlag != UINT32_MAX && temporaryFlag != UINT32_MAX);
    CHECK(!eventState->object_hide_flag(route, noFlag, &eventFlag) && eventFlag.scope == RHH_AUTHORED_FLAG_NONE
        && !eventFlag.value && !eventFlag.flag_id && eventFlag.run_id == 123);
    const u32 testFlags[] = {FLAG_HIDE_ROUTE_103_RIVAL, FLAG_TEMP_12};
    const u32 testIndices[] = {savedFlag, temporaryFlag};
    const u32 testScopes[] = {RHH_AUTHORED_FLAG_SAVED, RHH_AUTHORED_FLAG_TEMPORARY};
    for (u32 i = 0; i < ARRAY_COUNT(testFlags); ++i) {
        bool32 old = FlagGet(testFlags[i]); FlagClear(testFlags[i]);
        CHECK(!eventState->object_hide_flag(route, testIndices[i], &eventFlag) && eventFlag.scope == testScopes[i]
            && eventFlag.flag_id == testFlags[i] && !eventFlag.value);
        FlagSet(testFlags[i]);
        CHECK(!eventState->object_hide_flag(route, testIndices[i], &eventFlag) && eventFlag.value == 1);
        if (!old) FlagClear(testFlags[i]);
    }
    RhhContentKey hiddenMap = RhhContentKeyForId(RHH_MAP, MAP_ROUTE109);
    const struct MapHeader *hiddenHeader = Overworld_GetMapHeaderByGroupAndId(MAP_ROUTE109 >> 8, MAP_ROUTE109 & 0xff);
    u32 hiddenIndex = UINT32_MAX, nonHiddenIndex = UINT32_MAX;
    for (u32 i = 0; i < hiddenHeader->events->bgEventCount; ++i) {
        if (hiddenHeader->events->bgEvents[i].kind == BG_EVENT_HIDDEN_ITEM) hiddenIndex = i;
        else nonHiddenIndex = i;
    }
    CHECK(hiddenIndex != UINT32_MAX && nonHiddenIndex != UINT32_MAX);
    u32 hiddenFlag = FLAG_HIDDEN_ITEMS_START + hiddenHeader->events->bgEvents[hiddenIndex].bgUnion.hiddenItem.hiddenItemId;
    bool32 oldHidden = FlagGet(hiddenFlag); FlagClear(hiddenFlag);
    CHECK(!eventState->hidden_item_flag(hiddenMap, hiddenIndex, &eventFlag) && eventFlag.map == hiddenMap
        && eventFlag.flag_id == hiddenFlag && eventFlag.scope == RHH_AUTHORED_FLAG_SAVED && !eventFlag.value);
    FlagSet(hiddenFlag);
    CHECK(!eventState->hidden_item_flag(hiddenMap, hiddenIndex, &eventFlag) && eventFlag.value == 1);
    if (!oldHidden) FlagClear(hiddenFlag);
    unchangedFlag = eventFlag;
    CHECK(eventState->hidden_item_flag(hiddenMap, nonHiddenIndex, &eventFlag)
        && eventState->hidden_item_flag(hiddenMap, hiddenHeader->events->bgEventCount, &eventFlag)
        && eventState->object_hide_flag(0, noFlag, &eventFlag) && !memcmp(&eventFlag, &unchangedFlag, sizeof(eventFlag)));
    }
    u32 objectCount = 99;
    struct RhhAuthoredObject object = {.struct_size = sizeof(object), .index = 99}, oldObject = object;
    struct RhhAuthoredObject smallObject = {.struct_size = 1, .index = 99}, oldSmallObject = smallObject;
    CHECK(maps->object_count(0, &objectCount) && objectCount == 99 && maps->object_count(route, NULL)
        && !maps->object_count(route, &objectCount) && objectCount == routeHeader->events->objectEventCount && objectCount > 1
        && maps->object_at(route, objectCount, &object) && !memcmp(&object, &oldObject, sizeof(object))
        && maps->object_at(route, 0, &smallObject) && !memcmp(&smallObject, &oldSmallObject, sizeof(smallObject)));
    for (u32 i = 0; i < objectCount; ++i)
    {
        const struct ObjectEventTemplate *source = &routeHeader->events->objectEvents[i];
        CHECK(!maps->object_at(route, i, &object) && object.map == route && object.index == i
            && object.kind == RHH_AUTHORED_OBJECT_NORMAL && object.engine_kind == source->kind
            && object.local_id == source->localId && object.x == source->x && object.y == source->y
            && object.graphics_id == source->graphicsId && object.elevation == source->elevation
            && object.movement_type == source->movementType && object.movement_range_x == source->movementRangeX
            && object.movement_range_y == source->movementRangeY && object.trainer_type == source->trainerType
            && object.trainer_range_or_berry_id == source->trainerRange_berryTreeId && object.flag_id == source->flagId
            && !!(object.flags & RHH_AUTHORED_OBJECT_GRAPHICS_VARIABLE)
                == (source->graphicsId >= OBJ_EVENT_GFX_VARS && source->graphicsId <= OBJ_EVENT_GFX_VAR_F));
    }
    struct RhhAuthoredObjectImage sourceImage = {.struct_size = sizeof(sourceImage)};
    struct RhhAuthoredObjectImage oldSourceImage = {.struct_size = sizeof(sourceImage), .index = 99};
    struct RhhAuthoredObjectImage smallImage = {.struct_size = 1, .index = 99}, oldSmallImage = smallImage;
    u8 actorPixels[16 * 32 * 4], unchangedPixels[sizeof(actorPixels)];
    memset(actorPixels, 0xa5, sizeof(actorPixels)); memcpy(unchangedPixels, actorPixels, sizeof(actorPixels));
    CHECK(maps->object_image(route, 0, &smallImage) && !memcmp(&smallImage, &oldSmallImage, sizeof(smallImage))
        && maps->object_image(route, 1, &oldSourceImage) && oldSourceImage.index == 99
        && maps->object_image_rgba(route, 1, actorPixels, sizeof(actorPixels))
        && maps->object_image_rgba(route, 0, actorPixels, sizeof(actorPixels) - 1)
        && maps->object_image_rgba(route, objectCount, actorPixels, sizeof(actorPixels))
        && maps->object_image_rgba(route, 0, NULL, sizeof(actorPixels))
        && !memcmp(actorPixels, unchangedPixels, sizeof(actorPixels)));
    CHECK(!maps->object_image(route, 0, &sourceImage) && sourceImage.map == route && !sourceImage.index
        && sourceImage.width == 16 && sourceImage.height == 32
        && !maps->object_image_rgba(route, 0, actorPixels, sizeof(actorPixels)));
    const u8 *rawActor = (const u8 *)gObjectEventPic_Man3;
    u32 opaquePixels = 0, transparentPixels = 0;
    for (u32 y = 0; y < sourceImage.height; ++y) for (u32 x = 0; x < sourceImage.width; ++x)
    {
        u32 offset = ((y / 8) * 2 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
        u32 colorIndex = (rawActor[offset] >> ((x & 1) * 4)) & 15, dst = (y * sourceImage.width + x) * 4;
        u16 color = gObjectEventPal_Npc2[colorIndex];
        for (u32 c = 0; c < 3; ++c) { u32 v = (color >> (c * 5)) & 31; CHECK(actorPixels[dst + c] == ((v << 3) | (v >> 2))); }
        CHECK(actorPixels[dst + 3] == (colorIndex ? 255 : 0));
        if (colorIndex) ++opaquePixels; else ++transparentPixels;
    }
    CHECK(opaquePixels && transparentPixels);
    const struct RhhObjectVisualService *objectVisuals = (const void *)host->get_service("rhh:service/object_visuals", RHH_SERVICE_VERSION(1, 0));
    CHECK(objectVisuals && objectVisuals->header.struct_size >= RHH_OBJECT_VISUAL_SERVICE_V1_2_SIZE
        && RHH_OBJECT_VISUAL_SERVICE_V1_0_SIZE < RHH_OBJECT_VISUAL_SERVICE_V1_1_SIZE
        && RHH_OBJECT_VISUAL_SERVICE_V1_1_SIZE < RHH_OBJECT_VISUAL_SERVICE_V1_2_SIZE
        && host->get_service("rhh:service/object_visuals", RHH_SERVICE_VERSION(1, 1)) == &objectVisuals->header
        && host->get_service("rhh:service/object_visuals", RHH_SERVICE_VERSION(1, 2)) == &objectVisuals->header
        && !host->get_service("rhh:service/object_visuals", RHH_SERVICE_VERSION(1, 3)));
    struct RhhObjectVisualResolution resolved = {.struct_size = sizeof(resolved), .resolved_id = 999}, oldResolved = resolved;
    struct RhhObjectVisualResolution smallResolved = {.struct_size = 1, .resolved_id = 999}, oldSmallResolved = smallResolved;
    struct RhhObjectVisual sourceVisual = {.struct_size = sizeof(sourceVisual), .width = 999};
    struct RhhObjectVisual tinyVisual = {.struct_size = 1, .width = 999}, oldTinyVisual = tinyVisual;
    u32 graphic = routeHeader->events->objectEvents[0].graphicsId;
    CHECK(objectVisuals->resolve(UINT32_MAX, &resolved) && !memcmp(&resolved, &oldResolved, sizeof(resolved))
        && objectVisuals->resolve(graphic, &smallResolved) && !memcmp(&smallResolved, &oldSmallResolved, sizeof(smallResolved))
        && objectVisuals->info(graphic, &tinyVisual) && !memcmp(&tinyVisual, &oldTinyVisual, sizeof(tinyVisual))
        && !objectVisuals->resolve(graphic, &resolved) && resolved.input_id == graphic && resolved.resolved_id == graphic
        && !resolved.flags && resolved.variable_slot == UINT32_MAX
        && !objectVisuals->info(graphic, &sourceVisual) && sourceVisual.graphics_id == graphic
        && sourceVisual.width == sourceImage.width && sourceVisual.height == sourceImage.height
        && sourceVisual.frame_count > 1 && sourceVisual.animation_count >= 8);
    u8 graphicPixels[sizeof(actorPixels)]; memset(graphicPixels, 0x5a, sizeof(graphicPixels));
    u8 oldGraphicPixels[sizeof(graphicPixels)]; memcpy(oldGraphicPixels, graphicPixels, sizeof(graphicPixels));
    CHECK(objectVisuals->frame_rgba(graphic, sourceVisual.frame_count, graphicPixels, sizeof(graphicPixels))
        && objectVisuals->frame_rgba(graphic, 0, graphicPixels, sizeof(graphicPixels) - 1)
        && objectVisuals->frame_rgba(graphic, 0, NULL, sizeof(graphicPixels))
        && !memcmp(graphicPixels, oldGraphicPixels, sizeof(graphicPixels))
        && !objectVisuals->frame_rgba(graphic, 0, graphicPixels, sizeof(graphicPixels))
        && !memcmp(graphicPixels, actorPixels, sizeof(actorPixels)));
    u32 objectFacing = 99, objectCommands = 99;
    struct RhhObjectVisualAnimCommand objectAnim = {.struct_size = sizeof(objectAnim), .kind = 99}, oldObjectAnim = objectAnim;
    CHECK(objectVisuals->face_animation(graphic, 0, &objectFacing) && objectFacing == 99
        && !objectVisuals->face_animation(graphic, RHH_WILD_ACTOR_EAST, &objectFacing)
        && !objectVisuals->animation_command_count(graphic, objectFacing, &objectCommands) && objectCommands == 2
        && objectVisuals->animation_command(graphic, objectFacing, objectCommands, &objectAnim)
        && !memcmp(&objectAnim, &oldObjectAnim, sizeof(objectAnim))
        && !objectVisuals->animation_command(graphic, objectFacing, 0, &objectAnim)
        && objectAnim.kind == RHH_AUTHORED_ANIM_FRAME && (objectAnim.flags & RHH_AUTHORED_ANIM_HFLIP));
    u16 oldVar = VarGet(VAR_OBJ_GFX_ID_0);
    VarSet(VAR_OBJ_GFX_ID_0, graphic);
    CHECK(!objectVisuals->resolve(OBJ_EVENT_GFX_VAR_0, &resolved) && resolved.flags == RHH_OBJECT_VISUAL_VARIABLE
        && !resolved.variable_slot && resolved.resolved_id == graphic);
    u32 capturedGraphic = resolved.resolved_id;
    VarSet(VAR_OBJ_GFX_ID_0, NUM_OBJ_EVENT_GFX);
    CHECK(!objectVisuals->resolve(OBJ_EVENT_GFX_VAR_0, &resolved)
        && resolved.flags == (RHH_OBJECT_VISUAL_VARIABLE | RHH_OBJECT_VISUAL_FALLBACK)
        && resolved.resolved_id == OBJ_EVENT_GFX_NINJA_BOY
        && !objectVisuals->frame_rgba(capturedGraphic, 0, graphicPixels, sizeof(graphicPixels))
        && !memcmp(graphicPixels, actorPixels, sizeof(actorPixels)));
    VarSet(VAR_OBJ_GFX_ID_0, OBJ_EVENT_MON | OBJ_EVENT_MON_SHINY | OBJ_EVENT_MON_FEMALE | SPECIES_BULBASAUR);
    struct RhhObjectVisual savedVisual = sourceVisual;
    CHECK(!objectVisuals->resolve(OBJ_EVENT_GFX_VAR_0, &resolved)
        && resolved.flags == (RHH_OBJECT_VISUAL_VARIABLE | RHH_OBJECT_VISUAL_POKEMON | RHH_OBJECT_VISUAL_SHINY | RHH_OBJECT_VISUAL_FEMALE)
        && resolved.species == bulbasaur && objectVisuals->info(resolved.resolved_id, &sourceVisual)
        && !memcmp(&sourceVisual, &savedVisual, sizeof(sourceVisual)));
    CHECK(!objectVisuals->resolve(OBJ_EVENT_GFX_BARD, &resolved) && resolved.flags == RHH_OBJECT_VISUAL_DYNAMIC
        && objectVisuals->info(resolved.resolved_id, &sourceVisual) && !memcmp(&sourceVisual, &savedVisual, sizeof(sourceVisual)));
    u8 savedOldManId = gSaveBlock1Ptr->oldMan.common.id;
    struct RhhObjectVisualVariant variant = {.struct_size = sizeof(variant)}, savedVariant;
    struct RhhObjectVisual variantInfo = {.struct_size = sizeof(variantInfo)};
    u8 variantPixels[16 * 32 * 4], retainedVariantPixels[sizeof(variantPixels)];
    for (u32 id = MAUVILLE_MAN_BARD; id <= MAUVILLE_MAN_UNUSED2; ++id) {
        gSaveBlock1Ptr->oldMan.common.id = id;
        const struct ObjectEventGraphicsInfo *engine = GetObjectEventGraphicsInfo(OBJ_EVENT_GFX_BARD);
        CHECK(!objectVisuals->variant_current(OBJ_EVENT_GFX_BARD, &variant)
            && variant.kind == RHH_OBJECT_VARIANT_MAUVILLE_OLD_MAN && variant.variant == id
            && !objectVisuals->variant_info(&variant, &variantInfo)
            && variantInfo.width == engine->width && variantInfo.height == engine->height
            && variantInfo.frame_count == RhhModsAuthoredFrameCount(engine->images)
            && variantInfo.animation_count == RhhModsAuthoredAnimCount(engine->anims)
            && !objectVisuals->variant_frame_rgba(&variant, 0, variantPixels, sizeof(variantPixels)));
        u16 palette[16]; CHECK(RhhModsCopyObjectPalette(engine->paletteTag, palette));
        const u8 *raw = engine->images[0].data;
        for (u32 y = 0; y < variantInfo.height; ++y) for (u32 x = 0; x < variantInfo.width; ++x) {
            u32 offset = ((y / 8) * (variantInfo.width / 8) + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
            u32 colorIndex = (raw[offset] >> ((x & 1) * 4)) & 15, dst = (y * variantInfo.width + x) * 4;
            u16 color = palette[colorIndex];
            for (u32 c = 0; c < 3; ++c) { u32 v = (color >> (c * 5)) & 31; CHECK(variantPixels[dst + c] == ((v << 3) | (v >> 2))); }
            CHECK(variantPixels[dst + 3] == (colorIndex ? 255 : 0));
        }
        u32 facing = 99, commands = 99;
        struct RhhObjectVisualAnimCommand anim = {.struct_size = sizeof(anim)};
        CHECK(!objectVisuals->variant_face_animation(&variant, RHH_WILD_ACTOR_EAST, &facing)
            && facing == GetFaceDirectionAnimNum(RHH_WILD_ACTOR_EAST)
            && !objectVisuals->variant_animation_command_count(&variant, facing, &commands) && commands
            && !objectVisuals->variant_animation_command(&variant, facing, 0, &anim)
            && anim.kind == RHH_AUTHORED_ANIM_FRAME && anim.image == engine->anims[facing][0].frame.imageValue
            && anim.duration == engine->anims[facing][0].frame.duration);
        if (id == MAUVILLE_MAN_BARD) { savedVariant = variant; memcpy(retainedVariantPixels, variantPixels, sizeof(variantPixels)); }
    }
    CHECK(!objectVisuals->variant_frame_rgba(&savedVariant, 0, variantPixels, sizeof(variantPixels))
        && !memcmp(variantPixels, retainedVariantPixels, sizeof(variantPixels)));
    variant = savedVariant; variant.variant = MAUVILLE_MAN_UNUSED2 + 1;
    struct RhhObjectVisual unchangedVariantInfo = variantInfo;
    CHECK(objectVisuals->variant_info(&variant, &variantInfo) && !memcmp(&variantInfo, &unchangedVariantInfo, sizeof(variantInfo))
        && objectVisuals->variant_frame_rgba(&savedVariant, variantInfo.frame_count, variantPixels, sizeof(variantPixels))
        && objectVisuals->variant_frame_rgba(&savedVariant, 0, variantPixels, sizeof(variantPixels) - 1)
        && !memcmp(variantPixels, retainedVariantPixels, sizeof(variantPixels)));
    gSaveBlock1Ptr->oldMan.common.id = MAUVILLE_MAN_UNUSED2 + 1;
    variant = savedVariant;
    CHECK(objectVisuals->variant_current(OBJ_EVENT_GFX_BARD, &variant) && !memcmp(&variant, &savedVariant, sizeof(variant))
        && objectVisuals->variant_current(graphic, &variant) && !memcmp(&variant, &savedVariant, sizeof(variant)));
    gSaveBlock1Ptr->oldMan.common.id = savedOldManId;
    RhhContentKey variableRoute = RhhContentKeyForId(RHH_MAP, MAP_ROUTE119); u32 variableObjects = 0, variableIndex;
    CHECK(variableRoute && !maps->object_count(variableRoute, &variableObjects));
    for (variableIndex = 0; variableIndex < variableObjects; ++variableIndex)
    {
        object = (struct RhhAuthoredObject){.struct_size = sizeof(object)};
        CHECK(!maps->object_at(variableRoute, variableIndex, &object));
        if (object.kind == RHH_AUTHORED_OBJECT_NORMAL && object.graphics_id == OBJ_EVENT_GFX_VAR_0) break;
    }
    CHECK(variableIndex < variableObjects && (object.flags & RHH_AUTHORED_OBJECT_GRAPHICS_VARIABLE));
    VarSet(VAR_OBJ_GFX_ID_0, graphic);
    CHECK(!objectVisuals->resolve(object.graphics_id, &resolved) && resolved.resolved_id == graphic
        && !objectVisuals->frame_rgba(resolved.resolved_id, 0, graphicPixels, sizeof(graphicPixels))
        && !memcmp(graphicPixels, actorPixels, sizeof(actorPixels)));
    struct SaveBlock1 *savedBlock = gSaveBlock1Ptr; gSaveBlock1Ptr = NULL; oldResolved = resolved;
    CHECK(objectVisuals->resolve(OBJ_EVENT_GFX_VAR_0, &resolved) && !memcmp(&resolved, &oldResolved, sizeof(resolved))
        && !objectVisuals->resolve(graphic, &resolved) && resolved.resolved_id == graphic);
    gSaveBlock1Ptr = savedBlock;
    VarSet(VAR_OBJ_GFX_ID_0, oldVar);
    struct RhhBerryVisualRequest berryRequest = {.struct_size = sizeof(berryRequest), .stage = RHH_BERRY_PLANTED,
        .berry_item = RhhContentKeyForId(RHH_ITEM, ITEM_CHERI_BERRY)};
    struct RhhBerryVisual berryVisual = {.struct_size = sizeof(berryVisual), .width = 99}, oldBerryVisual = berryVisual;
    struct RhhBerryVisual tinyBerryVisual = {.struct_size = 1, .width = 99}, oldTinyBerryVisual = tinyBerryVisual;
    struct RhhBerryVisualCommand berryCommand = {.struct_size = sizeof(berryCommand), .kind = 99}, oldBerryCommand = berryCommand;
    CHECK(berryRequest.berry_item && objectVisuals->berry_info(NULL, &berryVisual)
        && !memcmp(&berryVisual, &oldBerryVisual, sizeof(berryVisual))
        && objectVisuals->berry_info(&berryRequest, &tinyBerryVisual) && !memcmp(&tinyBerryVisual, &oldTinyBerryVisual, sizeof(tinyBerryVisual))
        && objectVisuals->berry_command(&berryRequest, UINT32_MAX, &berryCommand) && !memcmp(&berryCommand, &oldBerryCommand, sizeof(berryCommand)));
    berryRequest.stage = RHH_BERRY_EMPTY;
    CHECK(objectVisuals->berry_info(&berryRequest, &berryVisual) && !memcmp(&berryVisual, &oldBerryVisual, sizeof(berryVisual)));
    berryRequest.stage = RHH_BERRY_BUDDING + 1;
    CHECK(objectVisuals->berry_info(&berryRequest, &berryVisual) && !memcmp(&berryVisual, &oldBerryVisual, sizeof(berryVisual)));
    berryRequest.stage = RHH_BERRY_PLANTED; berryRequest.berry_item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION);
    CHECK(objectVisuals->berry_info(&berryRequest, &berryVisual) && !memcmp(&berryVisual, &oldBerryVisual, sizeof(berryVisual)));
    const u16 *berryPalettes[] = {gObjectEventPal_Npc1, gObjectEventPal_Npc2, gObjectEventPal_Npc3, gObjectEventPal_Npc4};
    u8 berryPixels[16 * 32 * 4], savedBerryPixels[sizeof(berryPixels)];
    u32 berryCount = 0, berryCommands = 0;
    for (u32 berry = 1; berry <= NUM_BERRIES; ++berry)
    {
        berryRequest.berry_item = RhhContentKeyForId(RHH_ITEM, BerryTypeToItemId(berry));
        CHECK(berryRequest.berry_item);
        for (u32 stage = RHH_BERRY_PLANTED; stage <= RHH_BERRY_BUDDING; ++stage)
        {
            berryRequest.stage = stage;
            CHECK(!objectVisuals->berry_info(&berryRequest, &berryVisual) && berryVisual.berry_item == berryRequest.berry_item
                && berryVisual.stage == stage && berryVisual.width == 16 && berryVisual.height == (stage <= RHH_BERRY_SPROUTED ? 16 : 32)
                && berryVisual.frame_count == 9 && berryVisual.command_count >= 2);
            for (u32 command = 0; command < berryVisual.command_count; ++command)
            {
                CHECK(!objectVisuals->berry_command(&berryRequest, command, &berryCommand)
                    && berryCommand.command == command && berryCommand.stage == stage && berryCommand.berry_item == berryRequest.berry_item);
                if (berryCommand.kind != RHH_AUTHORED_ANIM_FRAME) continue;
                CHECK(berryCommand.flags & RHH_AUTHORED_ANIM_FRAME_AVAILABLE);
                memset(berryPixels, 0xa5, sizeof(berryPixels)); memcpy(savedBerryPixels, berryPixels, sizeof(berryPixels));
                u32 bytes = berryVisual.width * berryVisual.height * 4;
                CHECK(objectVisuals->berry_frame_rgba(&berryRequest, berryCommand.image, berryPixels, bytes - 1)
                    && !memcmp(berryPixels, savedBerryPixels, sizeof(berryPixels))
                    && !objectVisuals->berry_frame_rgba(&berryRequest, berryCommand.image, berryPixels, sizeof(berryPixels)));
                const u8 *raw = gBerries[berry].berryTreePicTable[berryCommand.image].data;
                u32 slot = gBerries[berry].berryTreePaletteSlotTable[stage - 1]; CHECK(slot >= 2 && slot <= 5);
                const u16 *palette = berryPalettes[slot - 2];
                for (u32 y = 0; y < berryVisual.height; ++y) for (u32 x = 0; x < berryVisual.width; ++x)
                {
                    u32 offset = ((y / 8) * 2 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
                    u32 colorIndex = (raw[offset] >> ((x & 1) * 4)) & 15, dst = (y * 16 + x) * 4;
                    u16 color = palette[colorIndex];
                    for (u32 c = 0; c < 3; ++c) { u32 v = (color >> (c * 5)) & 31; CHECK(berryPixels[dst + c] == ((v << 3) | (v >> 2))); }
                    CHECK(berryPixels[dst + 3] == (colorIndex ? 255 : 0));
                }
                ++berryCommands;
            }
        }
        ++berryCount;
    }
    CHECK(berryCount == NUM_BERRIES && berryCommands > berryCount * 7);
    u32 frameCount = 99;
    CHECK(maps->object_frame_count(0, 0, &frameCount) && frameCount == 99
        && maps->object_frame_count(route, 1, &frameCount) && frameCount == 99
        && maps->object_frame_count(route, 0, NULL)
        && !maps->object_frame_count(route, 0, &frameCount) && frameCount > 1);
    memcpy(unchangedPixels, actorPixels, sizeof(actorPixels));
    CHECK(maps->object_frame_rgba(route, 0, frameCount, actorPixels, sizeof(actorPixels))
        && maps->object_frame_rgba(route, 0, UINT32_MAX, actorPixels, sizeof(actorPixels))
        && maps->object_frame_rgba(route, 0, 1, actorPixels, sizeof(actorPixels) - 1)
        && !memcmp(actorPixels, unchangedPixels, sizeof(actorPixels))
        && !maps->object_frame_rgba(route, 0, 0, actorPixels, sizeof(actorPixels))
        && !memcmp(actorPixels, unchangedPixels, sizeof(actorPixels))
        && !maps->object_frame_rgba(route, 0, 1, actorPixels, sizeof(actorPixels)));
    rawActor += 16 * 32 / 2;
    for (u32 y = 0; y < 32; ++y) for (u32 x = 0; x < 16; ++x)
    {
        u32 offset = ((y / 8) * 2 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
        u32 colorIndex = (rawActor[offset] >> ((x & 1) * 4)) & 15, dst = (y * 16 + x) * 4;
        u16 color = gObjectEventPal_Npc2[colorIndex];
        for (u32 c = 0; c < 3; ++c) { u32 v = (color >> (c * 5)) & 31; CHECK(actorPixels[dst + c] == ((v << 3) | (v >> 2))); }
        CHECK(actorPixels[dst + 3] == (colorIndex ? 255 : 0));
    }
    RhhContentKey gym = RhhContentKeyForId(RHH_MAP, MAP_FORTREE_CITY_GYM);
    const struct MapHeader *gymHeader = Overworld_GetMapHeaderByGroupAndId(MAP_FORTREE_CITY_GYM >> 8, MAP_FORTREE_CITY_GYM & 0xff);
    u32 winona = 0;
    while (winona < gymHeader->events->objectEventCount && gymHeader->events->objectEvents[winona].graphicsId != OBJ_EVENT_GFX_WINONA) ++winona;
    CHECK(winona < gymHeader->events->objectEventCount && !maps->object_frame_count(gym, winona, &frameCount) && frameCount == 9
        && !maps->object_frame_rgba(gym, winona, 0, actorPixels, sizeof(actorPixels)));
    memcpy(unchangedPixels, actorPixels, sizeof(actorPixels));
    CHECK(!maps->object_frame_rgba(gym, winona, 3, actorPixels, sizeof(actorPixels))
        && !memcmp(actorPixels, unchangedPixels, sizeof(actorPixels))
        && maps->object_frame_rgba(gym, winona, frameCount, actorPixels, sizeof(actorPixels))
        && !memcmp(actorPixels, unchangedPixels, sizeof(actorPixels)));
    u32 animations = 99, east = 99, west = 99, commands = 99;
    struct RhhAuthoredAnimCommand anim = {.struct_size = sizeof(anim), .command = 99}, oldAnim = anim;
    struct RhhAuthoredAnimCommand smallAnim = {.struct_size = 1, .command = 99}, oldSmallAnim = smallAnim;
    CHECK(maps->object_animation_count(0, 0, &animations) && animations == 99
        && maps->object_animation_count(route, 1, &animations) && animations == 99
        && maps->object_animation_count(route, 0, NULL)
        && !maps->object_animation_count(route, 0, &animations) && animations >= 8
        && maps->object_face_animation(route, 0, 0, &east) && east == 99
        && maps->object_face_animation(route, 0, UINT32_MAX, &east) && east == 99
        && !maps->object_face_animation(route, 0, RHH_WILD_ACTOR_EAST, &east)
        && !maps->object_face_animation(route, 0, RHH_WILD_ACTOR_WEST, &west) && east != west
        && maps->object_animation_command_count(route, 0, animations, &commands) && commands == 99
        && maps->object_animation_command_count(route, 0, east, NULL)
        && !maps->object_animation_command_count(route, 0, east, &commands) && commands == 2
        && maps->object_animation_command(route, 0, east, commands, &anim) && !memcmp(&anim, &oldAnim, sizeof(anim))
        && maps->object_animation_command(route, 0, east, 0, &smallAnim) && !memcmp(&smallAnim, &oldSmallAnim, sizeof(smallAnim)));
    CHECK(!maps->object_animation_command(route, 0, east, 0, &anim) && anim.map == route
        && anim.object_index == 0 && anim.animation == east && !anim.command && anim.kind == RHH_AUTHORED_ANIM_FRAME
        && anim.image == 2 && anim.duration == 16 && anim.flags == (RHH_AUTHORED_ANIM_HFLIP | RHH_AUTHORED_ANIM_FRAME_AVAILABLE));
    CHECK(!maps->object_animation_command(route, 0, west, 0, &anim) && anim.image == 2
        && anim.flags == RHH_AUTHORED_ANIM_FRAME_AVAILABLE);
    CHECK(!maps->object_animation_command(route, 0, east, 1, &anim) && anim.kind == RHH_AUTHORED_ANIM_JUMP
        && anim.argument == 0 && anim.flags == RHH_AUTHORED_ANIM_JUMP_IN_BOUNDS);
    u32 southwest = 99;
    CHECK(!maps->object_face_animation(route, 0, RHH_WILD_ACTOR_SOUTHWEST, &southwest) && southwest == west);
    CHECK(routeHeader->connections && routeHeader->connections->count == connectionCount);
    gMapHeader = *routeHeader;
    for (u32 i = 0; i < connectionCount; ++i)
    {
        const struct MapConnection *engine = &routeHeader->connections->connections[i];
        CHECK(!maps->connection_at(route, i, &connection) && connection.index == i && connection.map == route
            && connection.engine_direction == engine->direction && connection.direction == engine->direction
            && connection.offset == engine->offset && connection.target
            && (connection.flags & RHH_AUTHORED_CONNECTION_TARGET_AVAILABLE)
            && (connection.flags & RHH_AUTHORED_CONNECTION_GEOMETRY_AVAILABLE)
            && connection.seam_start >= 0 && connection.seam_end > connection.seam_start);
        struct RhhAuthoredMap target = {.struct_size = sizeof(target)};
        CHECK(!maps->map(connection.target, &target) && target.width == connection.target_width && target.height == connection.target_height);
        for (u32 edge = 0; edge < 2; ++edge)
        {
            s32 along = edge ? connection.seam_end - 1 : connection.seam_start;
            s32 x = engine->direction == CONNECTION_WEST ? -1 : engine->direction == CONNECTION_EAST ? savedInfo.width : along;
            s32 y = engine->direction == CONNECTION_NORTH ? -1 : engine->direction == CONNECTION_SOUTH ? savedInfo.height : along;
            s32 tx = x - connection.origin_x, ty = y - connection.origin_y;
            CHECK(tx >= 0 && ty >= 0 && tx < target.width && ty < target.height
                && GetMapConnectionAtPos(x + MAP_OFFSET, y + MAP_OFFSET) == engine);
        }
    }
    gMapHeader = unchangedHeader;
    struct MapConnection edgeCases[] = {
        {.direction = CONNECTION_EAST, .offset = INT32_MAX, .mapGroup = MAP_OLDALE_TOWN >> 8, .mapNum = MAP_OLDALE_TOWN},
        {.direction = CONNECTION_WEST, .offset = INT32_MIN, .mapGroup = MAP_OLDALE_TOWN >> 8, .mapNum = MAP_OLDALE_TOWN},
        {.direction = CONNECTION_DIVE, .mapGroup = MAP_PALLET_TOWN >> 8, .mapNum = MAP_PALLET_TOWN},
        {.direction = 255, .mapGroup = MAP_OLDALE_TOWN >> 8, .mapNum = MAP_OLDALE_TOWN},
        {.direction = CONNECTION_NORTH, .mapGroup = 255, .mapNum = 255}
    };
    struct MapConnections edgeList = {.count = ARRAY_COUNT(edgeCases), .connections = edgeCases};
    struct MapHeader synthetic = *routeHeader; synthetic.connections = &edgeList;
    CHECK(!RhhModsAuthoredConnectionForHeader(&synthetic, route, 0, &connection)
        && (connection.flags & RHH_AUTHORED_CONNECTION_GEOMETRY_AVAILABLE)
        && connection.seam_start == routeHeader->mapLayout->height && connection.seam_end == connection.seam_start);
    CHECK(!RhhModsAuthoredConnectionForHeader(&synthetic, route, 1, &connection)
        && (connection.flags & RHH_AUTHORED_CONNECTION_GEOMETRY_AVAILABLE) && !connection.seam_start && !connection.seam_end);
    CHECK(!RhhModsAuthoredConnectionForHeader(&synthetic, route, 2, &connection) && connection.target == pallet);
    if (!gMapGroups[MAP_PALLET_TOWN >> 8]) CHECK(!connection.flags && !connection.target_width && !connection.target_height);
    CHECK(!RhhModsAuthoredConnectionForHeader(&synthetic, route, 3, &connection)
        && connection.direction == RHH_AUTHORED_CONNECTION_UNKNOWN && connection.engine_direction == 255
        && (connection.flags & RHH_AUTHORED_CONNECTION_TARGET_AVAILABLE)
        && !(connection.flags & RHH_AUTHORED_CONNECTION_GEOMETRY_AVAILABLE));
    CHECK(!RhhModsAuthoredConnectionForHeader(&synthetic, route, 4, &connection) && !connection.target && !connection.flags);
    edgeList.count = 257; connection = oldConnection;
    CHECK(RhhModsAuthoredConnectionForHeader(&synthetic, route, 0, &connection) && !memcmp(&connection, &oldConnection, sizeof(connection)));
    edgeList.count = 1; edgeList.connections = NULL;
    CHECK(RhhModsAuthoredConnectionForHeader(&synthetic, route, 0, &connection) && !memcmp(&connection, &oldConnection, sizeof(connection)));
    RhhContentKey base = RhhContentKeyForId(RHH_MAP, MAP_SECRET_BASE_RED_CAVE1);
    CHECK(!maps->connection_count(base, &connectionCount) && connectionCount == 0);
    const u32 waterMaps[] = {MAP_ROUTE124, MAP_UNDERWATER_ROUTE124};
    u32 transitions = 0;
    for (u32 i = 0; i < ARRAY_COUNT(waterMaps); ++i)
    {
        RhhContentKey key = RhhContentKeyForId(RHH_MAP, waterMaps[i]);
        CHECK(!maps->connection_count(key, &connectionCount));
        for (u32 j = 0; j < connectionCount; ++j)
        {
            CHECK(!maps->connection_at(key, j, &connection));
            if (connection.direction != RHH_AUTHORED_CONNECTION_DIVE && connection.direction != RHH_AUTHORED_CONNECTION_EMERGE) continue;
            CHECK((connection.flags & RHH_AUTHORED_CONNECTION_TARGET_AVAILABLE)
                && !(connection.flags & RHH_AUTHORED_CONNECTION_GEOMETRY_AVAILABLE)
                && connection.target_width && connection.target_height && !connection.origin_x && !connection.origin_y
                && !connection.seam_start && !connection.seam_end); ++transitions;
        }
    }
    CHECK(transitions == 2);
    if (!gMapGroups[MAP_PALLET_TOWN >> 8])
    {
        connectionCount = 99;
        CHECK(maps->connection_count(pallet, &connectionCount) && connectionCount == 99);
    }
    u32 warpCount = 99;
    struct RhhAuthoredWarp warp = {.struct_size = sizeof(warp), .index = 99}, oldWarp = warp;
    struct RhhAuthoredWarp smallWarp = {.struct_size = 1, .index = 99}, oldSmallWarp = smallWarp;
    RhhContentKey town = RhhContentKeyForId(RHH_MAP, MAP_LITTLEROOT_TOWN);
    CHECK(maps->warp_count(0, &warpCount) && warpCount == 99 && maps->warp_count(town, NULL)
        && !maps->warp_count(town, &warpCount) && warpCount == 3
        && maps->warp_at(town, warpCount, &warp) && !memcmp(&warp, &oldWarp, sizeof(warp))
        && maps->warp_at(town, 0, &smallWarp) && !memcmp(&smallWarp, &oldSmallWarp, sizeof(smallWarp)));
    const struct MapHeader *townHeader = Overworld_GetMapHeaderByGroupAndId(MAP_LITTLEROOT_TOWN >> 8, MAP_LITTLEROOT_TOWN & 0xff);
    for (u32 i = 0; i < warpCount; ++i)
    {
        const struct WarpEvent *engine = &townHeader->events->warps[i];
        const struct MapHeader *destination = Overworld_GetMapHeaderByGroupAndId(engine->mapGroup, engine->mapNum);
        const struct WarpEvent *arrival = &destination->events->warps[engine->warpId];
        CHECK(!maps->warp_at(town, i, &warp) && warp.map == town && warp.index == i
            && warp.x == engine->x && warp.y == engine->y && warp.source_elevation == engine->elevation
            && warp.target == RhhContentKeyForId(RHH_MAP, (u32)engine->mapGroup << 8 | engine->mapNum)
            && warp.target_warp == engine->warpId && (warp.flags & RHH_AUTHORED_WARP_TARGET_AVAILABLE)
            && (warp.flags & RHH_AUTHORED_WARP_TARGET_EVENT_AVAILABLE)
            && (warp.flags & RHH_AUTHORED_WARP_TARGET_IN_BOUNDS)
            && warp.target_x == arrival->x && warp.target_y == arrival->y && warp.target_elevation == arrival->elevation);
    }
    RhhContentKey route101 = RhhContentKeyForId(RHH_MAP, MAP_ROUTE101);
    CHECK(!maps->warp_count(route101, &warpCount) && !warpCount);
    CHECK(!maps->warp_count(base, &warpCount) && warpCount == 1 && !maps->warp_at(base, 0, &warp)
        && warp.flags == RHH_AUTHORED_WARP_DYNAMIC && warp.target_warp == WARP_ID_SECRET_BASE && !warp.target);
    struct WarpEvent warpCases[] = {
        {.x = -1, .y = 4, .elevation = 7, .warpId = 255, .mapGroup = MAP_LITTLEROOT_TOWN >> 8, .mapNum = MAP_LITTLEROOT_TOWN & 0xff},
        {.warpId = WARP_ID_DYNAMIC, .mapGroup = 0, .mapNum = MAP_NUM(MAP_DYNAMIC)},
        {.warpId = 0, .mapGroup = 255, .mapNum = 255},
        {.warpId = 0, .mapGroup = MAP_PALLET_TOWN >> 8, .mapNum = MAP_PALLET_TOWN & 0xff},
        {.warpId = 127, .mapGroup = MAP_LITTLEROOT_TOWN >> 8, .mapNum = MAP_LITTLEROOT_TOWN & 0xff}
    };
    struct MapEvents warpEvents = {.warpCount = ARRAY_COUNT(warpCases), .warps = warpCases};
    synthetic = *routeHeader; synthetic.events = &warpEvents;
    CHECK(!RhhModsAuthoredWarpForHeader(&synthetic, route, 0, &warp) && warp.x == -1 && warp.y == 4
        && warp.source_elevation == 7 && warp.target_warp == 255
        && (warp.flags & RHH_AUTHORED_WARP_TARGET_AVAILABLE) && !(warp.flags & RHH_AUTHORED_WARP_TARGET_EVENT_AVAILABLE));
    CHECK(!RhhModsAuthoredWarpForHeader(&synthetic, route, 1, &warp) && warp.flags == RHH_AUTHORED_WARP_DYNAMIC && !warp.target);
    CHECK(!RhhModsAuthoredWarpForHeader(&synthetic, route, 2, &warp) && !warp.flags && !warp.target);
    CHECK(!RhhModsAuthoredWarpForHeader(&synthetic, route, 3, &warp) && warp.target == pallet);
    if (!gMapGroups[MAP_PALLET_TOWN >> 8]) CHECK(!warp.flags && !warp.target_x && !warp.target_y);
    CHECK(!RhhModsAuthoredWarpForHeader(&synthetic, route, 4, &warp) && (warp.flags & RHH_AUTHORED_WARP_TARGET_AVAILABLE)
        && !(warp.flags & RHH_AUTHORED_WARP_TARGET_EVENT_AVAILABLE));
    warpEvents.warpCount = 1; warpEvents.warps = NULL; warp = oldWarp;
    CHECK(RhhModsAuthoredWarpForHeader(&synthetic, route, 0, &warp) && !memcmp(&warp, &oldWarp, sizeof(warp)));
    synthetic.events = NULL;
    CHECK(RhhModsAuthoredWarpForHeader(&synthetic, route, 0, &warp) && !memcmp(&warp, &oldWarp, sizeof(warp)));
    struct ObjectEventTemplate objects[] = {
        {.kind = OBJ_KIND_CLONE, .localId = 7, .x = -2, .y = 4, .graphicsId = 123,
         .targetLocalId = 1, .targetMapGroup = MAP_ROUTE103 >> 8, .targetMapNum = MAP_ROUTE103 & 0xff},
        {.kind = OBJ_KIND_CLONE, .targetLocalId = 0, .targetMapGroup = MAP_PALLET_TOWN >> 8, .targetMapNum = MAP_PALLET_TOWN & 0xff},
        {.kind = OBJ_KIND_CLONE, .targetLocalId = 255, .targetMapGroup = MAP_ROUTE103 >> 8, .targetMapNum = MAP_ROUTE103 & 0xff},
        {.kind = OBJ_KIND_CLONE, .targetLocalId = 1, .targetMapGroup = 256, .targetMapNum = 0},
        {.kind = 42, .localId = 5, .graphicsId = 999}
    };
    struct MapEvents objectEvents = {.objectEventCount = ARRAY_COUNT(objects), .objectEvents = objects};
    synthetic = *routeHeader; synthetic.events = &objectEvents;
    CHECK(!RhhModsAuthoredObjectForHeader(&synthetic, route, 0, &object) && object.kind == RHH_AUTHORED_OBJECT_CLONE
        && object.local_id == 7 && object.x == -2 && object.y == 4 && object.target == route && object.target_local_id == 1
        && object.flags == (RHH_AUTHORED_OBJECT_TARGET_AVAILABLE | RHH_AUTHORED_OBJECT_TARGET_EVENT_AVAILABLE)
        && !object.graphics_id && !object.elevation && !object.movement_type && !object.trainer_type && !object.flag_id);
    CHECK(!RhhModsAuthoredObjectForHeader(&synthetic, route, 1, &object) && object.target == pallet
        && !(object.flags & RHH_AUTHORED_OBJECT_TARGET_EVENT_AVAILABLE));
    if (!gMapGroups[MAP_PALLET_TOWN >> 8]) CHECK(!object.flags);
    CHECK(!RhhModsAuthoredObjectForHeader(&synthetic, route, 2, &object)
        && (object.flags & RHH_AUTHORED_OBJECT_TARGET_AVAILABLE) && !(object.flags & RHH_AUTHORED_OBJECT_TARGET_EVENT_AVAILABLE));
    CHECK(!RhhModsAuthoredObjectForHeader(&synthetic, route, 3, &object) && !object.target && !object.flags);
    CHECK(!RhhModsAuthoredObjectForHeader(&synthetic, route, 4, &object) && object.kind == RHH_AUTHORED_OBJECT_UNKNOWN
        && object.engine_kind == 42 && object.local_id == 5 && !object.graphics_id);
    object = oldObject; objectEvents.objectEvents = NULL;
    CHECK(RhhModsAuthoredObjectForHeader(&synthetic, route, 0, &object) && !memcmp(&object, &oldObject, sizeof(object)));
    RhhContentKey route119 = RhhContentKeyForId(RHH_MAP, MAP_ROUTE119);
    const struct MapHeader *weatherHeader = Overworld_GetMapHeaderByGroupAndId(MAP_ROUTE119 >> 8, MAP_ROUTE119 & 0xff);
    u32 coordCount = 99, weatherCount = 0, scriptCount = 0;
    struct RhhAuthoredCoordEvent coord = {.struct_size = sizeof(coord), .index = 99}, oldCoord = coord;
    struct RhhAuthoredCoordEvent smallCoord = {.struct_size = 1, .index = 99}, oldSmallCoord = smallCoord;
    CHECK(maps->coord_count(0, &coordCount) && coordCount == 99 && maps->coord_count(route119, NULL)
        && !maps->coord_count(route119, &coordCount) && coordCount == weatherHeader->events->coordEventCount && coordCount
        && maps->coord_at(route119, coordCount, &coord) && !memcmp(&coord, &oldCoord, sizeof(coord))
        && maps->coord_at(route119, 0, &smallCoord) && !memcmp(&smallCoord, &oldSmallCoord, sizeof(smallCoord)));
    for (u32 i = 0; i < coordCount; ++i)
    {
        const struct CoordEvent *source = &weatherHeader->events->coordEvents[i];
        CHECK(!maps->coord_at(route119, i, &coord) && coord.index == i && coord.map == route119
            && coord.x == source->x && coord.y == source->y && coord.elevation == source->elevation
            && coord.trigger == source->trigger && coord.condition == source->index);
        if (source->script) { CHECK(coord.kind == RHH_AUTHORED_COORD_SCRIPT_CONDITIONAL && !coord.flags); ++scriptCount; }
        else {
            CHECK(coord.kind == RHH_AUTHORED_COORD_WEATHER && (coord.flags & RHH_AUTHORED_COORD_WEATHER_KNOWN));
            if (source->trigger == COORD_EVENT_WEATHER_ROUTE119_CYCLE)
                CHECK(coord.engine_weather == WEATHER_ROUTE119_CYCLE && coord.natural_weather == RHH_NATURAL_WEATHER_DYNAMIC);
            else if (source->trigger == COORD_EVENT_WEATHER_SUNNY)
                CHECK(coord.engine_weather == WEATHER_SUNNY && coord.natural_weather == RHH_NATURAL_WEATHER_SUNNY);
            ++weatherCount;
        }
    }
    CHECK(weatherCount && scriptCount && !maps->coord_count(route, &coordCount) && !coordCount);
    static const u8 scriptByte = 0;
    struct CoordEvent coords[] = {
        {.x = -2, .y = 3, .elevation = 7, .trigger = COORD_EVENT_WEATHER_FOG_DIAGONAL},
        {.trigger = 256 + COORD_EVENT_WEATHER_RAIN},
        {.trigger = 255},
        {.trigger = TRIGGER_RUN_IMMEDIATELY, .index = 5, .script = &scriptByte},
        {.trigger = 123, .index = 42, .script = &scriptByte}
    };
    struct MapEvents coordEvents = {.coordEventCount = ARRAY_COUNT(coords), .coordEvents = coords};
    synthetic = *routeHeader; synthetic.events = &coordEvents;
    CHECK(!RhhModsAuthoredCoordForHeader(&synthetic, route, 0, &coord) && coord.x == -2 && coord.y == 3
        && coord.elevation == 7 && coord.kind == RHH_AUTHORED_COORD_WEATHER
        && coord.weather_code == COORD_EVENT_WEATHER_FOG_DIAGONAL && coord.flags == RHH_AUTHORED_COORD_WEATHER_KNOWN
        && coord.engine_weather == WEATHER_FOG_DIAGONAL && coord.natural_weather == RHH_NATURAL_WEATHER_FOG);
    CHECK(!RhhModsAuthoredCoordForHeader(&synthetic, route, 1, &coord) && coord.trigger == 259
        && coord.weather_code == COORD_EVENT_WEATHER_RAIN && coord.natural_weather == RHH_NATURAL_WEATHER_RAIN);
    CHECK(!RhhModsAuthoredCoordForHeader(&synthetic, route, 2, &coord) && !coord.flags && coord.weather_code == 255);
    CHECK(!RhhModsAuthoredCoordForHeader(&synthetic, route, 3, &coord) && coord.kind == RHH_AUTHORED_COORD_SCRIPT_IMMEDIATE
        && coord.condition == 5 && !coord.flags && !coord.weather_code);
    CHECK(!RhhModsAuthoredCoordForHeader(&synthetic, route, 4, &coord) && coord.kind == RHH_AUTHORED_COORD_SCRIPT_CONDITIONAL
        && coord.trigger == 123 && coord.condition == 42 && !coord.flags);
    coord = oldCoord; coordEvents.coordEvents = NULL;
    CHECK(RhhModsAuthoredCoordForHeader(&synthetic, route, 0, &coord) && !memcmp(&coord, &oldCoord, sizeof(coord)));
    synthetic.events = NULL;
    CHECK(RhhModsAuthoredCoordForHeader(&synthetic, route, 0, &coord) && !memcmp(&coord, &oldCoord, sizeof(coord)));
    synthetic.events = NULL;
    CHECK(RhhModsAuthoredObjectForHeader(&synthetic, route, 0, &object) && !memcmp(&object, &oldObject, sizeof(object)));
    u32 bgCount = 99;
    struct RhhAuthoredBackgroundEvent bg = {.struct_size = sizeof(bg), .index = 99}, oldBg = bg;
    struct RhhAuthoredBackgroundEvent smallBg = {.struct_size = 1, .index = 99}, oldSmallBg = smallBg;
    CHECK(maps->background_count(0, &bgCount) && bgCount == 99 && maps->background_count(route, NULL)
        && !maps->background_count(route, &bgCount) && bgCount == routeHeader->events->bgEventCount && bgCount
        && maps->background_at(route, bgCount, &bg) && !memcmp(&bg, &oldBg, sizeof(bg))
        && maps->background_at(route, 0, &smallBg) && !memcmp(&smallBg, &oldSmallBg, sizeof(smallBg)));
    for (u32 i = 0; i < bgCount; ++i)
    {
        const struct BgEvent *source = &routeHeader->events->bgEvents[i];
        CHECK(!maps->background_at(route, i, &bg) && bg.map == route && bg.index == i
            && bg.x == source->x && bg.y == source->y && bg.elevation == source->elevation
            && bg.engine_kind == source->kind);
    }
    struct BgEvent backgrounds[] = {
        {.x = 12, .y = 7, .elevation = 3, .kind = BG_EVENT_PLAYER_FACING_NORTH, .bgUnion.script = &scriptByte},
        {.kind = BG_EVENT_PLAYER_FACING_ANY},
        {.kind = 5, .bgUnion.hiddenItem = {.item = ITEM_POTION, .hiddenItemId = 41, .quantity = 3, .underfoot = 1}},
        {.kind = 6, .bgUnion.hiddenItem = {.item = ITEM_POTION, .hiddenItemId = 42, .quantity = 1}},
        {.kind = BG_EVENT_HIDDEN_ITEM, .bgUnion.hiddenItem = {.item = ITEM_POTION, .hiddenItemId = 43, .quantity = 2}},
        {.kind = BG_EVENT_SECRET_BASE, .bgUnion.secretBaseId = 1234},
        {.kind = 255, .bgUnion.script = &scriptByte}
    };
    struct MapEvents backgroundEvents = {.bgEventCount = ARRAY_COUNT(backgrounds), .bgEvents = backgrounds};
    synthetic = *routeHeader; synthetic.events = &backgroundEvents;
    CHECK(!RhhModsAuthoredBackgroundForHeader(&synthetic, route, 0, &bg) && bg.kind == RHH_AUTHORED_BACKGROUND_SCRIPT
        && bg.flags == RHH_AUTHORED_BACKGROUND_HAS_SCRIPT && bg.engine_kind == BG_EVENT_PLAYER_FACING_NORTH
        && bg.facing == RHH_AUTHORED_BACKGROUND_FACING_NORTH && bg.x == 12 && bg.y == 7 && bg.elevation == 3
        && !bg.item && !bg.secret_base_id);
    CHECK(!RhhModsAuthoredBackgroundForHeader(&synthetic, route, 1, &bg) && bg.kind == RHH_AUTHORED_BACKGROUND_SCRIPT
        && bg.facing == RHH_AUTHORED_BACKGROUND_FACING_ANY && !bg.flags);
    const u32 facingKinds[] = {RHH_AUTHORED_BACKGROUND_FACING_ANY, RHH_AUTHORED_BACKGROUND_FACING_NORTH,
        RHH_AUTHORED_BACKGROUND_FACING_SOUTH, RHH_AUTHORED_BACKGROUND_FACING_EAST, RHH_AUTHORED_BACKGROUND_FACING_WEST};
    for (u32 i = 0; i < ARRAY_COUNT(facingKinds); ++i)
    {
        backgrounds[1].kind = i;
        CHECK(!RhhModsAuthoredBackgroundForHeader(&synthetic, route, 1, &bg)
            && bg.kind == RHH_AUTHORED_BACKGROUND_SCRIPT && bg.facing == facingKinds[i]);
    }
    for (u32 i = 2; i <= 4; ++i)
        CHECK(!RhhModsAuthoredBackgroundForHeader(&synthetic, route, i, &bg) && bg.kind == RHH_AUTHORED_BACKGROUND_HIDDEN_ITEM
            && bg.item_id == ITEM_POTION && bg.item == RhhContentKeyForId(RHH_ITEM, ITEM_POTION)
            && bg.hidden_item_id == 39 + i && bg.quantity == backgrounds[i].bgUnion.hiddenItem.quantity
            && !!(bg.flags & RHH_AUTHORED_BACKGROUND_UNDERFOOT) == (i == 2) && !bg.secret_base_id && !bg.facing);
    CHECK(!RhhModsAuthoredBackgroundForHeader(&synthetic, route, 5, &bg) && bg.kind == RHH_AUTHORED_BACKGROUND_SECRET_BASE
        && bg.secret_base_id == 1234 && !bg.item && !bg.flags);
    CHECK(!RhhModsAuthoredBackgroundForHeader(&synthetic, route, 6, &bg) && bg.kind == RHH_AUTHORED_BACKGROUND_UNKNOWN
        && bg.engine_kind == 255 && !bg.flags && !bg.item && !bg.secret_base_id);
    bg = oldBg; backgroundEvents.bgEvents = NULL;
    CHECK(RhhModsAuthoredBackgroundForHeader(&synthetic, route, 0, &bg) && !memcmp(&bg, &oldBg, sizeof(bg)));
    synthetic.events = NULL;
    CHECK(RhhModsAuthoredBackgroundForHeader(&synthetic, route, 0, &bg) && !memcmp(&bg, &oldBg, sizeof(bg)));
    RhhContentKey route121 = RhhContentKeyForId(RHH_MAP, MAP_ROUTE121);
    const struct MapHeader *hiddenHeader = Overworld_GetMapHeaderByGroupAndId(MAP_ROUTE121 >> 8, MAP_ROUTE121 & 0xff);
    u32 hiddenCount = 0;
    CHECK(route121 && !maps->background_count(route121, &bgCount) && bgCount == hiddenHeader->events->bgEventCount);
    for (u32 i = 0; i < bgCount; ++i)
    {
        const struct BgEvent *source = &hiddenHeader->events->bgEvents[i];
        if (source->kind != 5 && source->kind != 6 && source->kind != BG_EVENT_HIDDEN_ITEM) continue;
        CHECK(!maps->background_at(route121, i, &bg) && bg.kind == RHH_AUTHORED_BACKGROUND_HIDDEN_ITEM
            && bg.x == source->x && bg.y == source->y && bg.item_id == source->bgUnion.hiddenItem.item
            && bg.hidden_item_id == source->bgUnion.hiddenItem.hiddenItemId
            && bg.quantity == source->bgUnion.hiddenItem.quantity
            && bg.item == RhhContentKeyForId(RHH_ITEM, source->bgUnion.hiddenItem.item));
        ++hiddenCount;
    }
    CHECK(hiddenCount);
    struct RhhAuthoredMapTile tile = {.struct_size = sizeof(tile), .habitat_flags = 999}, savedTile = tile;
    u8 rgba[8 * 8 * 4], savedRgba[sizeof(rgba)]; memset(rgba, 0x5a, sizeof(rgba)); memcpy(savedRgba, rgba, sizeof(rgba));
    CHECK(maps->tile_rgba(route, 0, rgba, sizeof(rgba) - 1)
        && maps->tile_rgba(route, 13u << 12, rgba, sizeof(rgba))
        && maps->tile_rgba(0, 0, rgba, sizeof(rgba)) && maps->tile_rgba(route, 0, NULL, sizeof(rgba))
        && !memcmp(rgba, savedRgba, sizeof(rgba)));
    CHECK(!maps->tile(route, 0, 0, &tile));
    struct RhhAuthoredHabitatSample baseline = {.struct_size = sizeof(baseline)}, baselineTiny = {.struct_size = 1}, baselineOld = baselineTiny;
    struct MapHeader beforeBaselineHeader = gMapHeader; struct BackupMapLayout beforeBaselineGrid = gBackupMapLayout;
    rng_value_t baselineRng = gRngValue, baselineRng2 = gRng2Value;
    CHECK(host->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 6)) == &habitat->header
        && RHH_HABITAT_SERVICE_V1_5_SIZE < RHH_HABITAT_SERVICE_V1_6_SIZE
        && !host->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 8))
        && habitat->authored_sample(route, 0, 0, &baselineTiny) && !memcmp(&baselineTiny, &baselineOld, sizeof(baselineTiny))
        && !habitat->authored_sample(route, 0, 0, &baseline) && baseline.map.map == route
        && baseline.sample.map == route && baseline.sample.x == 0 && baseline.sample.y == 0
        && baseline.sample.permitted && baseline.sample.base_flags == tile.habitat_flags
        && baseline.sample.flags == tile.habitat_flags && !memcmp(&baseline.sample.tile, &tile.tile, sizeof(tile.tile))
        && baseline.sample.natural_weather == baseline.map.natural_weather
        && !memcmp(&gMapHeader, &beforeBaselineHeader, sizeof(gMapHeader))
        && !memcmp(&gBackupMapLayout, &beforeBaselineGrid, sizeof(gBackupMapLayout))
        && !memcmp(&baselineRng, &gRngValue, sizeof(baselineRng)) && !memcmp(&baselineRng2, &gRng2Value, sizeof(baselineRng2)));
    struct RhhAuthoredHabitatSample savedBaseline = baseline;
    CHECK(habitat->authored_sample(route, -1, 0, &baseline) && !memcmp(&baseline, &savedBaseline, sizeof(baseline))
        && habitat->authored_sample(0, 0, 0, &baseline) && !memcmp(&baseline, &savedBaseline, sizeof(baseline)));
    u16 entries[] = {tile.tile.tiles[0], tile.tile.tiles[0] ^ (1u << 10), 13u << 12, tile.tile.tiles[0]};
    struct RhhAuthoredTileImage batch[ARRAY_COUNT(entries)], savedBatch[ARRAY_COUNT(entries)];
    memset(batch, 0x5a, sizeof(batch)); memcpy(savedBatch, batch, sizeof(batch));
    CHECK(maps->tiles_rgba(route, entries, ARRAY_COUNT(entries), batch, ARRAY_COUNT(entries) - 1)
        && maps->tiles_rgba(route, NULL, 1, batch, ARRAY_COUNT(entries))
        && maps->tiles_rgba(route, entries, 1, NULL, 1)
        && maps->tiles_rgba(route, entries, RHH_AUTHORED_TILE_BATCH_MAX + 1, batch, RHH_AUTHORED_TILE_BATCH_MAX + 1)
        && maps->tiles_rgba(0, entries, ARRAY_COUNT(entries), batch, ARRAY_COUNT(entries))
        && !memcmp(batch, savedBatch, sizeof(batch))
        && !maps->tiles_rgba(route, NULL, 0, NULL, 0) && maps->tiles_rgba(0, NULL, 0, NULL, 0));
    CHECK(!maps->tiles_rgba(route, entries, ARRAY_COUNT(entries), batch, ARRAY_COUNT(entries)));
    for (u32 i = 0; i < ARRAY_COUNT(entries); ++i)
    {
        CHECK(batch[i].tile == entries[i] && !batch[i].reserved);
        if (i == 2)
        {
            u8 zero[sizeof(batch[i].rgba)] = {0};
            CHECK(batch[i].status == RHH_AUTHORED_TILE_UNAVAILABLE && !memcmp(batch[i].rgba, zero, sizeof(zero)));
        }
        else CHECK(batch[i].status == RHH_AUTHORED_TILE_OK && !maps->tile_rgba(route, entries[i], rgba, sizeof(rgba))
                   && !memcmp(batch[i].rgba, rgba, sizeof(rgba)));
    }
    u16 many[RHH_AUTHORED_TILE_BATCH_MAX];
    struct RhhAuthoredTileImage *manyResults = calloc(ARRAY_COUNT(many), sizeof(*manyResults)); CHECK(manyResults);
    for (u32 i = 0; i < ARRAY_COUNT(many); ++i) many[i] = entries[i % ARRAY_COUNT(entries)];
    CHECK(!maps->tiles_rgba(route, many, ARRAY_COUNT(many), manyResults, ARRAY_COUNT(many)));
    for (u32 i = 0; i < ARRAY_COUNT(many); ++i)
        CHECK(!memcmp(&manyResults[i], &batch[i % ARRAY_COUNT(entries)], sizeof(batch[0])));
    free(manyResults);
    tile = savedTile;
    CHECK(maps->tile(route, -1, 0, &tile) && maps->tile(route, savedInfo.width, 0, &tile)
        && maps->tile(route, 0, savedInfo.height, &tile) && maps->tile(0, 0, 0, &tile)
        && !memcmp(&tile, &savedTile, sizeof(tile)) && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gMain.inBattle = FALSE; ResetTasks(); StartWeather();
    struct WarpData location = gSaveBlock1Ptr->location;
    const u32 ids[] = {MAP_ROUTE103, MAP_SECRET_BASE_RED_CAVE1, MAP_PALLET_TOWN};
    u32 checked = 0, styles = 0, rgbaChecked = 0, rgbaFormats = 0;
    for (u32 m = 0; m < ARRAY_COUNT(ids); ++m)
    {
        u32 id = ids[m]; RhhContentKey key = RhhContentKeyForId(RHH_MAP, id);
        if (!gMapGroups[id >> 8])
        {
            struct RhhAuthoredMap absent = {.struct_size = sizeof(absent), .width = 99}, old = absent;
            CHECK(maps->map(key, &absent) && !memcmp(&absent, &old, sizeof(absent)));
            continue;
        }
        const struct MapHeader *header = Overworld_GetMapHeaderByGroupAndId(id >> 8, id & 0xff);
        CHECK(header && header->mapLayout && !maps->map(key, &info)
            && info.width == header->mapLayout->width && info.height == header->mapLayout->height
            && info.map_type == header->mapType && info.engine_weather == header->weather
            && !!(info.flags & RHH_AUTHORED_MAP_FRLG) == !!header->mapLayout->isFrlg);
        styles |= header->mapLayout->isFrlg ? 2u : 1u;
        gMapHeader = *header; gSaveBlock1Ptr->location.mapGroup = id >> 8; gSaveBlock1Ptr->location.mapNum = id;
        gBackupMapLayout.width = header->mapLayout->width + MAP_OFFSET_W;
        gBackupMapLayout.height = header->mapLayout->height + MAP_OFFSET_H;
        gBackupMapLayout.map = calloc((size_t)gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
        for (u32 y = 0; y < info.height; ++y)
            memcpy(&gBackupMapLayout.map[(y + MAP_OFFSET) * gBackupMapLayout.width + MAP_OFFSET],
                   &header->mapLayout->map[y * info.width], info.width * sizeof(u16));
        const struct Tileset *graphics[] = {header->mapLayout->primaryTileset, header->mapLayout->secondaryTileset};
        u32 firstTile = GetNumTilesInPrimary(header->mapLayout);
        u32 mapPixels = 0, mapSources = 0;
        memset(gNativeVram, 0, 0x10000); REG_BG2CNT = 0;
        for (u32 i = 0; i < 2; ++i)
        {
            CHECK(graphics[i] && graphics[i]->tiles);
            u32 length = graphics[i]->isCompressed ? GetDecompressedDataSize(graphics[i]->tiles) : RhhModsRawTilesetSize(graphics[i]);
            u32 limit = (i ? NUM_TILES_TOTAL - firstTile : firstTile) * 32;
            CHECK(length && length <= MAX_DECOMPRESSION_BUFFER_SIZE);
            if (graphics[i]->isCompressed)
            {
                u8 *unpacked = calloc(1, length); CHECK(unpacked); DecompressDataWithHeaderWram(graphics[i]->tiles, unpacked);
                memcpy(gNativeVram + (i ? firstTile * 32 : 0), unpacked, min(length, limit)); free(unpacked);
            }
            else memcpy(gNativeVram + (i ? firstTile * 32 : 0), graphics[i]->tiles, min(length, limit));
        }
        LoadMapTilesetPalettes(header->mapLayout); memcpy(gNativePalette, gPlttBufferFaded, 512);
        if (id == MAP_SECRET_BASE_RED_CAVE1)
        {
            u32 rawSize = RhhModsRawTilesetSize(graphics[1]);
            CHECK(!graphics[1]->isCompressed && rawSize == 83 * 32);
            memset(rgba, 0x5a, sizeof(rgba)); memcpy(savedRgba, rgba, sizeof(rgba));
            u16 missing = (GetNumPalsInPrimary(header->mapLayout) << 12) | (firstTile + rawSize / 32);
            CHECK(maps->tile_rgba(key, missing, rgba, sizeof(rgba)) && !memcmp(rgba, savedRgba, sizeof(rgba)));
            u16 rawEntries[] = {missing, (GetNumPalsInPrimary(header->mapLayout) << 12) | firstTile};
            struct RhhAuthoredTileImage rawBatch[ARRAY_COUNT(rawEntries)];
            CHECK(!maps->tiles_rgba(key, rawEntries, ARRAY_COUNT(rawEntries), rawBatch, ARRAY_COUNT(rawBatch))
                && rawBatch[0].status == RHH_AUTHORED_TILE_UNAVAILABLE && rawBatch[1].status == RHH_AUTHORED_TILE_OK
                && !maps->tile_rgba(key, rawEntries[1], rgba, sizeof(rgba))
                && !memcmp(rawBatch[1].rgba, rgba, sizeof(rgba)));
        }
        for (u32 y = 0; y < info.height; y += 2) for (u32 x = 0; x < info.width; x += 2)
        {
            u16 raw = header->mapLayout->map[y * info.width + x];
            struct RhhWorldTile live = {.struct_size = sizeof(live)};
            struct RhhHabitatSample sample = {.struct_size = sizeof(sample)};
            tile = (struct RhhAuthoredMapTile){.struct_size = sizeof(tile)};
            if (raw == MAPGRID_UNDEFINED) { CHECK(maps->tile(key, x, y, &tile)); continue; }
            CHECK(!world->tile_at(x, y, &live));
            CHECK(!maps->tile(key, x, y, &tile) && tile.map == key && tile.x == x && tile.y == y
                && !memcmp(&tile.tile, &live, sizeof(live)));
            if (live.behavior >= NUM_METATILE_BEHAVIORS) CHECK(!tile.habitat_available && !tile.habitat_flags);
            else CHECK(tile.habitat_available && !habitat->sample(x, y, &sample) && tile.habitat_flags == sample.base_flags);
            if (mapPixels < 96 || mapSources != 3)
            {
                struct RhhAuthoredTileImage images[NUM_TILES_PER_METATILE];
                CHECK(!maps->tiles_rgba(key, tile.tile.tiles, NUM_TILES_PER_METATILE, images, ARRAY_COUNT(images)));
                for (u32 i = 0; i < ARRAY_COUNT(tile.tile.tiles); ++i)
                {
                    u16 entry = tile.tile.tiles[i]; if ((entry >> 12) >= NUM_PALS_TOTAL) continue;
                    u32 source = (entry & 0x3ff) < firstTile ? 0 : 1;
                    if (mapPixels >= 96 && (mapSources & (1u << source))) continue;
                    u8 actual[sizeof(rgba)], liveRgba[sizeof(rgba)];
                    if (maps->tile_rgba(key, entry, actual, sizeof(actual)))
                    { CHECK(images[i].status == RHH_AUTHORED_TILE_UNAVAILABLE); continue; }
                    CHECK(images[i].status == RHH_AUTHORED_TILE_OK && images[i].tile == entry
                        && !memcmp(images[i].rgba, actual, sizeof(actual)));
                    for (u32 flip = 0; flip < 4; ++flip)
                    {
                        u16 flipped = entry ^ (flip << 10);
                        CHECK(!maps->tile_rgba(key, flipped, actual, sizeof(actual))
                            && !world->tile_rgba(flipped, liveRgba, sizeof(liveRgba))
                            && !memcmp(actual, liveRgba, sizeof(actual))); ++rgbaChecked; ++mapPixels;
                    }
                    mapSources |= 1u << source; rgbaFormats |= graphics[source]->isCompressed ? 1u : 2u;
                    if (mapPixels >= 96 && mapSources == 3) break;
                }
            }
            ++checked;
        }
        CHECK(info.width && info.height);
        CHECK(mapPixels >= 96);
        tile = (struct RhhAuthoredMapTile){.struct_size = sizeof(tile)};
        CHECK(!maps->tile(key, 0, 0, &tile)); savedTile = tile;
        gBackupMapLayout.map[MAP_OFFSET * gBackupMapLayout.width + MAP_OFFSET] ^= MAPGRID_COLLISION_MASK;
        CHECK(!maps->tile(key, 0, 0, &tile) && !memcmp(&tile, &savedTile, sizeof(tile)));
        free(gBackupMapLayout.map); gBackupMapLayout = unchangedGrid;
    }
    CHECK(checked > 100 && (styles & 1) && rgbaChecked >= 192 && rgbaFormats == 3);
    /* The native data set omits FRLG map groups; exercise its layout format
     * against the real engine decoder with an isolated synthetic tileset. */
    u16 blocks[] = {PACK_METATILE(0) | PACK_COLLISION(2) | PACK_ELEVATION(3),
                    PACK_METATILE(NUM_METATILES_IN_PRIMARY_FRLG) | PACK_COLLISION(1) | PACK_ELEVATION(7)};
    u32 primaryAttrs[] = {MB_TALL_GRASS | (2u << METATILE_ATTR_LAYER_SHIFT_FRLG)};
    u32 secondaryAttrs[] = {MB_PUDDLE | (1u << METATILE_ATTR_LAYER_SHIFT_FRLG)};
    u16 primaryTiles[NUM_TILES_PER_METATILE] = {1, 2, 3, 4, 5, 6, 7, 8};
    u16 secondaryTiles[NUM_TILES_PER_METATILE] = {8, 7, 6, 5, 4, 3, 2, 1};
    struct Tileset primary = {.metatiles = primaryTiles, .metatileAttributes = (const u16 *)primaryAttrs};
    struct Tileset secondary = {.metatiles = secondaryTiles, .metatileAttributes = (const u16 *)secondaryAttrs};
    struct MapLayout fixture = {.width = 2, .height = 1, .map = blocks, .primaryTileset = &primary,
        .secondaryTileset = &secondary, .isFrlg = TRUE};
    {
        u16 original = blocks[1]; blocks[1] = MAPGRID_UNDEFINED;
        struct RhhAuthoredMapScanRequest request = {.struct_size = sizeof(request), .map = route,
            .width = 2, .height = 1, .limit = 2};
        struct RhhAuthoredMapScan page = {.struct_size = sizeof(page)};
        struct RhhAuthoredMapTile zero = {0};
        CHECK(!RhhModsAuthoredScanForLayout(&fixture, MAP_TYPE_ROUTE, route, &request, &page)
            && page.count == 2 && page.cells[0].status == RHH_AUTHORED_MAP_CELL_OK
            && page.cells[1].status == RHH_AUTHORED_MAP_CELL_UNAVAILABLE
            && page.cells[1].x == 1 && page.cells[1].y == 0 && !memcmp(&page.cells[1].tile, &zero, sizeof(zero)));
        blocks[1] = original;
    }
    gMapHeader.mapLayout = &fixture; gMapHeader.mapType = MAP_TYPE_ROUTE;
    gSaveBlock1Ptr->location.mapGroup = MAP_ROUTE103 >> 8; gSaveBlock1Ptr->location.mapNum = MAP_ROUTE103;
    gBackupMapLayout.width = fixture.width + MAP_OFFSET_W; gBackupMapLayout.height = fixture.height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc((size_t)gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    memcpy(&gBackupMapLayout.map[MAP_OFFSET * gBackupMapLayout.width + MAP_OFFSET], blocks, sizeof(blocks));
    for (u32 x = 0; x < ARRAY_COUNT(blocks); ++x)
    {
        struct RhhWorldTile live = {.struct_size = sizeof(live)};
        struct RhhHabitatSample sample = {.struct_size = sizeof(sample)};
        tile = (struct RhhAuthoredMapTile){.struct_size = sizeof(tile)};
        CHECK(!RhhModsAuthoredTileForLayout(&fixture, MAP_TYPE_ROUTE, route, x, 0, &tile)
            && !world->tile_at(x, 0, &live) && !memcmp(&tile.tile, &live, sizeof(live))
            && tile.habitat_available && !habitat->sample(x, 0, &sample) && tile.habitat_flags == sample.base_flags);
    }
    free(gBackupMapLayout.map); gBackupMapLayout = unchangedGrid;
    gMapHeader = unchangedHeader; gSaveBlock1Ptr->location = location; gMain.inBattle = FALSE;
    RhhModsUnloadLibraries(); puts("authored maps: static layouts match loaded engine tiles and remain isolated");
}
