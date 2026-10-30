#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhAuthoredMapService *maps;
static const struct RhhSpeciesVisualService *visuals;
static const struct RhhObjectVisualService *objects;
static const struct RhhAuthoredEventStateService *eventState;
static const struct RhhModArchiveService *archive;
static RhhContentKey archiveOwner, archiveKey, archiveKey2;
static RhhContentKey route, route119, bulbasaur, cheri;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhAuthoredMap map = {.struct_size = sizeof(map)};
    struct RhhAuthoredMapTile tile = {.struct_size = sizeof(tile)};
    struct RhhAuthoredMapScanRequest scanRequest = {.struct_size = sizeof(scanRequest), .map = route,
        .width = 1, .height = 1, .limit = 1};
    struct RhhAuthoredMapScan scan = {.struct_size = sizeof(scan)};
    uint8_t rgba[8 * 8 * 4];
    struct RhhAuthoredTileImage batch[2];
    uint32_t count = 0;
    struct RhhAuthoredConnection connection = {.struct_size = sizeof(connection)};
    struct RhhAuthoredWarp warp = {.struct_size = sizeof(warp)};
    struct RhhAuthoredObject object = {.struct_size = sizeof(object)};
    struct RhhAuthoredCoordEvent coord = {.struct_size = sizeof(coord)};
    struct RhhAuthoredBackgroundEvent background = {.struct_size = sizeof(background)};
    struct RhhAuthoredObjectImage image = {.struct_size = sizeof(image)};
    struct RhhAuthoredAnimCommand anim = {.struct_size = sizeof(anim)};
    struct RhhObjectVisualResolution resolution = {.struct_size = sizeof(resolution)};
    struct RhhObjectVisual objectVisual = {.struct_size = sizeof(objectVisual)};
    struct RhhObjectVisualAnimCommand objectAnim = {.struct_size = sizeof(objectAnim)};
    struct RhhObjectVisualVariant oldMan = {.struct_size = sizeof(oldMan), .kind = RHH_OBJECT_VARIANT_MAUVILLE_OLD_MAN};
    struct RhhAuthoredEventFlag objectFlag = {.struct_size = sizeof(objectFlag)};
    struct RhhDynamicWarp dynamic = {.struct_size = sizeof(dynamic)};
    struct RhhBerryVisualRequest berryRequest = {.struct_size = sizeof(berryRequest), .stage = RHH_BERRY_FRUIT, .berry_item = cheri};
    struct RhhBerryVisual berry = {.struct_size = sizeof(berry)};
    struct RhhBerryVisualCommand berryAnim = {.struct_size = sizeof(berryAnim)};
    uint8_t actorImage[16 * 32 * 4];
    uint8_t objectImage[16 * 32 * 4];
    uint32_t facing = 0;
    struct RhhSpeciesVisualRequest request = {.struct_size = sizeof(request), .species = bulbasaur};
    struct RhhSpeciesVisual visual = {.struct_size = sizeof(visual)};
    struct RhhSpeciesVisualAnimCommand speciesAnim = {.struct_size = sizeof(speciesAnim)};
    uint8_t pokemonImage[32 * 32 * 4];
    uint8_t pokemonBatch[32 * 32 * 4 * 2];
    const uint32_t pokemonFrames[] = {0, 1};
    struct RhhModArchiveRecord archiveRecord = {.struct_size = sizeof(archiveRecord)};
    char archiveData[5] = {0};
    if (archive->find(archiveOwner, archiveKey, &archiveRecord)
        && archive->put(archiveOwner, archiveKey, 0, "hello", 5, &archiveRecord))
    { puts("AUTHORED_MAP_EXTERNAL_BAD"); return; }
    struct RhhModArchiveRecord second = {.struct_size = sizeof(second)};
    int hasSecond = !archive->find(archiveOwner, archiveKey2, &second);
    struct RhhModArchiveOperation archiveOps[] = {
        {.struct_size = sizeof(archiveOps[0]), .kind = RHH_MOD_ARCHIVE_PUT, .key = archiveKey,
         .expected_revision = archiveRecord.revision, .size = 5, .data = "hello"},
        {.struct_size = sizeof(archiveOps[0]), .kind = RHH_MOD_ARCHIVE_PUT, .key = archiveKey2,
         .expected_revision = hasSecond ? second.revision : 0, .size = 1, .data = "x"}
    };
    struct RhhModArchiveBatchResult archiveResult = {.struct_size = sizeof(archiveResult)};
    if (archive->batch(archiveOwner, archiveOps, 2, &archiveResult) || archiveResult.count != 2
        || archiveResult.operations[0].revision + 1 != archiveResult.operations[1].revision)
    { puts("AUTHORED_MAP_EXTERNAL_BAD"); return; }
    archiveRecord.revision = archiveResult.operations[0].revision;
    if (archiveRecord.size != sizeof(archiveData)
        || archive->copy(archiveOwner, archiveKey, archiveRecord.revision, archiveData, sizeof(archiveData))
        || memcmp(archiveData, "hello", sizeof(archiveData)))
    { puts("AUTHORED_MAP_EXTERNAL_BAD"); return; }
    if (maps->map(route, &map) || maps->tile(route, 0, 0, &tile)
        || maps->scan(&scanRequest, &scan) || scan.count != 1 || scan.next != 1 || scan.total != 1
        || scan.cells[0].status != RHH_AUTHORED_MAP_CELL_OK || memcmp(&scan.cells[0].tile, &tile, sizeof(tile))
        || maps->tile_rgba(route, tile.tile.tiles[0], rgba, sizeof(rgba))
        || maps->tiles_rgba(route, tile.tile.tiles, 2, batch, 2)
        || batch[0].status != RHH_AUTHORED_TILE_OK || memcmp(batch[0].rgba, rgba, sizeof(rgba))
        || maps->connection_count(route, &count) || count != 2 || maps->connection_at(route, 0, &connection)
        || connection.map != route || connection.direction != RHH_AUTHORED_CONNECTION_SOUTH || !connection.target
        || maps->warp_count(route, &count) || count != 1 || maps->warp_at(route, 0, &warp)
        || warp.map != route || !warp.target || !(warp.flags & RHH_AUTHORED_WARP_TARGET_AVAILABLE)
        || maps->object_count(route, &count) || !count || maps->object_at(route, 0, &object)
        || object.map != route || object.kind != RHH_AUTHORED_OBJECT_NORMAL || object.index
        || eventState->object_hide_flag(route, 0, &objectFlag) || objectFlag.map != route
        || objectFlag.flag_id != object.flag_id || objectFlag.run_id != 123
        || eventState->dynamic_warp(&dynamic) || dynamic.run_id != 123
        || maps->coord_count(route119, &count) || count < 3 || maps->coord_at(route119, 2, &coord)
        || coord.map != route119 || coord.kind != RHH_AUTHORED_COORD_WEATHER || !(coord.flags & RHH_AUTHORED_COORD_WEATHER_KNOWN)
        || maps->background_count(route, &count) || !count || maps->background_at(route, 0, &background)
        || background.map != route || background.kind != RHH_AUTHORED_BACKGROUND_SCRIPT
        || background.facing != RHH_AUTHORED_BACKGROUND_FACING_ANY || !(background.flags & RHH_AUTHORED_BACKGROUND_HAS_SCRIPT)
        || maps->object_image(route, 0, &image) || image.width != 16 || image.height != 32
        || maps->object_image_rgba(route, 0, actorImage, sizeof(actorImage))
        || maps->object_frame_count(route, 0, &count) || count < 2
        || maps->object_frame_rgba(route, 0, 1, actorImage, sizeof(actorImage))
        || objects->resolve(object.graphics_id, &resolution) || resolution.resolved_id != object.graphics_id || resolution.flags
        || objects->info(resolution.resolved_id, &objectVisual) || objectVisual.width != 16 || objectVisual.height != 32
        || objects->frame_rgba(resolution.resolved_id, 1, objectImage, sizeof(objectImage))
        || memcmp(actorImage, objectImage, sizeof(actorImage))
        || maps->object_face_animation(route, 0, RHH_WILD_ACTOR_EAST, &facing)
        || maps->object_animation_command(route, 0, facing, 0, &anim)
        || objects->animation_command(resolution.resolved_id, facing, 0, &objectAnim)
        || objectAnim.kind != anim.kind || objectAnim.image != anim.image || objectAnim.flags != anim.flags
        || objects->berry_info(&berryRequest, &berry) || berry.width != 16 || berry.height != 32
        || objects->berry_command(&berryRequest, 0, &berryAnim) || berryAnim.kind != RHH_AUTHORED_ANIM_FRAME
        || objects->berry_frame_rgba(&berryRequest, berryAnim.image, objectImage, sizeof(objectImage))
        || objects->variant_info(&oldMan, &objectVisual) || objectVisual.width != 16 || objectVisual.height != 32
        || objects->variant_face_animation(&oldMan, RHH_WILD_ACTOR_EAST, &facing)
        || objects->variant_animation_command(&oldMan, facing, 0, &objectAnim)
        || objectAnim.kind != RHH_AUTHORED_ANIM_FRAME
        || objects->variant_frame_rgba(&oldMan, objectAnim.image, objectImage, sizeof(objectImage))
        || anim.kind != RHH_AUTHORED_ANIM_FRAME || !(anim.flags & RHH_AUTHORED_ANIM_HFLIP)
        || visuals->info(&request, &visual) || visual.species != bulbasaur || visual.width != 32 || visual.height != 32
        || visuals->frame_rgba(&request, 0, pokemonImage, sizeof(pokemonImage))
        || visuals->frames_rgba(&request, pokemonFrames, 2, pokemonBatch, sizeof(pokemonImage), sizeof(pokemonBatch))
        || memcmp(pokemonImage, pokemonBatch, sizeof(pokemonImage))
        || visuals->face_animation(&request, RHH_WILD_ACTOR_EAST, &facing)
        || visuals->animation_command(&request, facing, 0, &speciesAnim)
        || speciesAnim.kind != RHH_AUTHORED_ANIM_FRAME || speciesAnim.image != 4
        || !(speciesAnim.flags & RHH_AUTHORED_ANIM_HFLIP)
        || map.map != route || tile.map != route || tile.x || tile.y || !map.width || !map.height)
        puts("AUTHORED_MAP_EXTERNAL_BAD");
    else puts("AUTHORED_MAP_EXTERNAL_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 11), RHH_SERVICE_REQUIRED, 0, "rhh:service/authored_maps"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/species_visuals"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/object_visuals"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/authored_event_state"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/mod_archive"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:authored_maps", .hook_count = 1, .hooks = hooks, .service_requirement_count = 5, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    maps = (const void *)host->get_service("rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 11));
    visuals = (const void *)host->get_service("rhh:service/species_visuals", RHH_SERVICE_VERSION(1, 2));
    objects = (const void *)host->get_service("rhh:service/object_visuals", RHH_SERVICE_VERSION(1, 2));
    eventState = (const void *)host->get_service("rhh:service/authored_event_state", RHH_SERVICE_VERSION(1, 1));
    archive = (const void *)host->get_service("rhh:service/mod_archive", RHH_SERVICE_VERSION(1, 1));
    archiveOwner = host->content_key("test:authored_maps"); archiveKey = host->content_key("test:authored_maps/archive");
    archiveKey2 = host->content_key("test:authored_maps/archive2");
    route = host->content_key("rhh:map/route103");
    route119 = host->content_key("rhh:map/route119");
    bulbasaur = host->content_key("rhh:species/bulbasaur");
    cheri = host->content_key("rhh:item/cheri_berry");
    return maps && maps->header.struct_size >= RHH_AUTHORED_MAP_SERVICE_V1_11_SIZE
        && visuals && visuals->header.struct_size >= RHH_SPECIES_VISUAL_SERVICE_V1_2_SIZE
        && objects && objects->header.struct_size >= RHH_OBJECT_VISUAL_SERVICE_V1_2_SIZE
        && eventState && eventState->header.struct_size >= RHH_AUTHORED_EVENT_STATE_SERVICE_V1_1_SIZE
        && archive && archive->header.struct_size >= RHH_MOD_ARCHIVE_SERVICE_V1_1_SIZE
        && route && route119 && bulbasaur && cheri && archiveOwner && archiveKey && archiveKey2 ? &definition : NULL;
}
