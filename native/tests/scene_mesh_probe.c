/* Independent consumer of the public scene mesh ABI. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhWorldSceneService *sScene;
static const struct RhhWorldSceneService *sInferredTrace;
static const struct RhhWorldSceneService *sShadowTrace;
static const struct RhhOverworldService *sWorld;
static const struct RhhAuthoredEncounterService *sEncounters;
static const struct RhhEcologyTagsService *sEcology;
static RhhContentKey sZigzagoon;
static const struct RhhModEcologyTag tags[] = {
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/wary", "Wary", "Avoids nearby players."}
};
static struct RhhModSpeciesTag speciesTags[] = {{sizeof(struct RhhModSpeciesTag), 0, 0, 0}};
static struct RhhModSceneRule walls[] = {
    {.struct_size = sizeof(struct RhhModSceneRule), .id = "test:scene/oldale_trace_wall",
     .match = RHH_SCENE_MATCH_MAP | RHH_SCENE_MATCH_POSITION, .x = 3, .y = 0,
     .style = {RHH_SCENE_SOLID, RHH_SCENE_UPPER_WALL_NORTH, RHH_SCENE_CASTS_SHADOW | RHH_SCENE_OCCLUDES, 0, 32}},
    {.struct_size = sizeof(struct RhhModSceneRule), .id = "test:scene/oldale_sight_only",
     .match = RHH_SCENE_MATCH_MAP | RHH_SCENE_MATCH_POSITION, .x = 4, .y = 0,
     .style = {RHH_SCENE_SOLID, RHH_SCENE_UPPER_WALL_NORTH, RHH_SCENE_OCCLUDES, 0, 32}},
    {.struct_size = sizeof(struct RhhModSceneRule), .id = "test:scene/oldale_shadow_only",
     .match = RHH_SCENE_MATCH_MAP | RHH_SCENE_MATCH_POSITION, .x = 5, .y = 0,
     .style = {RHH_SCENE_SOLID, RHH_SCENE_UPPER_WALL_NORTH, RHH_SCENE_CASTS_SHADOW, 0, 32}}
};
static struct RhhModSceneLight lights[] = {
    {.struct_size = sizeof(struct RhhModSceneLight), .id = "test:light/route", .x = 24, .y = 21,
     .height = 12, .radius = 48, .rgb = 0xffaa55, .intensity = 600},
    {.struct_size = sizeof(struct RhhModSceneLight), .id = "test:light/oldale", .x = 37, .y = 3,
     .height = 12, .radius = 40, .rgb = 0x55aaff, .intensity = 500, .period_mask = 1u << (RHH_TIME_NIGHT - 1)}
};
static void Check(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    uint32_t has = 0, count = 0;
    struct RhhEcologyTag tag = {.struct_size = sizeof(tag)};
    struct RhhAuthoredWildTable authored = {.struct_size = sizeof(authored)};
    struct RhhAuthoredWildWeights weights = {.struct_size = sizeof(weights)};
    struct RhhAuthoredWildSelection small = {.struct_size = 1, .variant = 99}, savedSmall = small;
    if (!sInferredTrace || !sInferredTrace->trace_inferred_light
        || sEcology->species_has(sZigzagoon, speciesTags[0].tag, &has) || !has
        || sEcology->species_count(sZigzagoon, &count) || count != 1
        || sEcology->species_at(sZigzagoon, 0, &tag) || strcmp(tag.id, tags[0].id)
        || !sEncounters || sEncounters->variants(lights[0].map, &count) || !count
        || sEncounters->table(lights[0].map, 0, RHH_WILD_TIME_MORNING, RHH_WILD_LAND, &authored)
        || !authored.present || !authored.slot_count || !authored.slots[0].species
        || sEncounters->base_weights(RHH_WILD_FISHING, RHH_WILD_ROD_GOOD, &weights)
        || !weights.available || weights.first_slot != 2 || weights.total != 100 || weights.weights[2] != 60
        || !sEncounters->selection(lights[0].map, RHH_WILD_TIME_MORNING, RHH_WILD_LAND, &small)
        || memcmp(&small, &savedSmall, sizeof(small)))
    { puts("SCENE_MESH_BAD"); return; }
    struct RhhSceneShadowTraceRequest invalid = {.struct_size = sizeof(invalid), .caster_count = 1};
    struct RhhWorldSpriteTransform transform = {.struct_size = sizeof(transform), .a = 99}, savedTransform = transform;
    struct RhhWorldDisplayState display = {.struct_size = 1, .display_control = 99}, savedDisplay = display;
    struct RhhWorldDisplayFrame frame = {.struct_size = 1, .count = 99}, savedFrame = frame;
    struct RhhWorldSpriteRasterOptions raster = {.struct_size = sizeof(raster), .flags = RHH_WORLD_RASTER_MOSAIC,
        .mosaic_width = 2, .mosaic_height = 2};
    uint8_t unchanged[4] = {1, 2, 3, 4};
    if (!sWorld || !sWorld->sprite_part_transform(UINT32_MAX, 0, &transform)
        || memcmp(&transform, &savedTransform, sizeof(transform))
        || !sWorld->display_state(&display) || memcmp(&display, &savedDisplay, sizeof(display))
        || !sWorld->display_frame(&frame) || memcmp(&frame, &savedFrame, sizeof(frame))
        || !sWorld->sprite_part_raster_rgba(UINT32_MAX, 0, &raster, unchanged, sizeof(unchanged))
        || !sWorld->effect_part_transformed_rgba(UINT32_MAX, 0, unchanged, sizeof(unchanged))
        || unchanged[0] != 1 || unchanged[3] != 4)
    { puts("SCENE_MESH_BAD"); return; }
    struct RhhSceneBillboardTraceRequest wide = {.struct_size = sizeof(wide), .width = 128, .height = 128,
        .from = {0, -5, 4}, .to = {0, 5, 4},
        .corners = {{-4, 0, 8}, {4, 0, 8}, {4, 0, 0}, {-4, 0, 0}}};
    struct RhhSceneBillboardTraceResult wideHit = {.struct_size = sizeof(wideHit)};
    if (sShadowTrace->trace_billboard(&wide, &wideHit) || wideHit.status != RHH_SCENE_TRACE_BLOCKED
        || wideHit.pixel_x != 64 || wideHit.pixel_y != 64)
    { puts("SCENE_MESH_BAD"); return; }
    struct RhhSceneShadowTraceResult shadow = {.struct_size = sizeof(shadow), .caster_id = 99}, savedShadow = shadow;
    if (!sShadowTrace || !sShadowTrace->trace_directional_shadow
        || !sShadowTrace->trace_directional_shadow(&invalid, &shadow) || memcmp(&shadow, &savedShadow, sizeof(shadow)))
    { puts("SCENE_MESH_BAD"); return; }
    struct RhhSceneTile tile = {.struct_size = sizeof(tile), .x = 3, .y = 4,
        .tile.struct_size = sizeof(tile.tile), .style = {RHH_SCENE_WATER, RHH_SCENE_UPPER_BILLBOARD,
            RHH_SCENE_CASTS_SHADOW, -5, 20}};
    for (unsigned i = 0; i < 8; ++i) tile.tile.tiles[i] = i + 7;
    struct RhhSceneMesh mesh = {.struct_size = sizeof(mesh)};
    if (sScene->build_mesh(&tile, &mesh) || mesh.count != 8
        || mesh.quads[0].kind != RHH_SCENE_QUAD_GROUND || mesh.quads[0].tile != 7
        || mesh.quads[0].vertex[0][0] != 48 || mesh.quads[0].vertex[0][1] != 64
        || mesh.quads[0].vertex[0][2] != -5 || mesh.quads[4].kind != RHH_SCENE_QUAD_BILLBOARD
        || mesh.quads[4].vertex[0][1] != 72 || mesh.quads[4].vertex[0][2] != 15
        || mesh.quads[4].vertex[3][2] != 5 || mesh.quads[4].flags != RHH_SCENE_CASTS_SHADOW)
    { puts("SCENE_MESH_BAD"); return; }
    struct RhhSceneMesh previous = mesh; tile.style.upper = RHH_SCENE_UPPER_FLAT;
    if (!sScene->build_mesh(&tile, &mesh) || memcmp(&previous, &mesh, sizeof(mesh)))
    { puts("SCENE_MESH_BAD"); return; }
    puts("SCENE_MESH_PUBLIC_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_WORLD_RENDER_PASSES, RHH_CONSTRAINT, 0, Check, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 11), RHH_SERVICE_REQUIRED, 0, "rhh:service/world_scene"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/ecology_tags"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 11), RHH_SERVICE_REQUIRED, 0, "rhh:service/overworld"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/authored_encounters"}
};
static const struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_ECOLOGY,
    .id = "test:scene_mesh_probe", .hook_count = 1, .hooks = hooks,
    .service_requirement_count = 4, .service_requirements = requirements,
    .scene_light_count = 2, .scene_lights = lights, .scene_rule_count = 3, .scene_rules = walls,
    .ecology_tag_count = 1, .ecology_tags = tags, .species_tag_count = 1, .species_tags = speciesTags};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    lights[0].map = host->content_key("rhh:map/route103");
    lights[1].map = host->content_key("rhh:map/oldale_town");
    for (unsigned i = 0; i < 3; ++i) walls[i].map = lights[1].map;
    sZigzagoon = speciesTags[0].species = host->content_key("rhh:species/zigzagoon");
    speciesTags[0].tag = host->content_key(tags[0].id);
    sEcology = (const void *)host->get_service("rhh:service/ecology_tags", RHH_SERVICE_VERSION(1, 0));
    sScene = (const void *)host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 5));
    sInferredTrace = (const void *)host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 7));
    sShadowTrace = (const void *)host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 11));
    sWorld = (const void *)host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 11));
    sEncounters = (const void *)host->get_service("rhh:service/authored_encounters", RHH_SERVICE_VERSION(1, 2));
    return sEcology && sScene && sScene->header.struct_size >= RHH_WORLD_SCENE_SERVICE_V1_5_SIZE
        && sInferredTrace && sInferredTrace->header.struct_size >= RHH_WORLD_SCENE_SERVICE_V1_7_SIZE
        && sShadowTrace && sShadowTrace->header.struct_size >= RHH_WORLD_SCENE_SERVICE_V1_11_SIZE
        && sWorld && sWorld->header.struct_size >= RHH_OVERWORLD_SERVICE_V1_11_SIZE
        && sEncounters && sEncounters->header.struct_size >= RHH_AUTHORED_ENCOUNTER_SERVICE_V1_2_SIZE ? &mod : NULL;
}
