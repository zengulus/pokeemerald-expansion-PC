/* Independent module used by replay.py to check scene views on a real map. */
#include "mod_api.h"
#include <stdio.h>

static const struct RhhOverworldService *sWorld;
static const struct RhhWorldSceneService *sScene;
static const struct RhhWorldClockService *sClock;
static const struct RhhWorldEnvironmentService *sEnvironment;
static const struct RhhHabitatService *sHabitat;
static RhhContentKey sRuleKey;
static int sReady, sDone;
static int32_t sStartX, sLastX, sLastY;
static uint64_t sLastStep;
static int sSawStep;
static unsigned sHabitatCalls;

static void HabitatRule(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhHabitatSample sample = {.struct_size = sizeof(sample)};
    if (sHabitat->context(&sample) || sample.map != event->event_key
        || sample.x != (int32_t)event->args[0] || sample.y != (int32_t)event->args[1]
        || !sample.environment_available || sample.environment.step != event->ordinal)
    { puts("HABITAT_PROBE_BAD"); return; }
    ++sHabitatCalls; decision->value |= RHH_HABITAT_GRASS;
}

static void ProbeStep(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWorldClockSnapshot clock = {.struct_size = sizeof(clock)};
    struct RhhWorldStepSnapshot step = {.struct_size = sizeof(step)};
    if (sClock->current(&clock) || sClock->step_context(&step) || step.step != event->ordinal
        || step.step != clock.steps || step.run_id != clock.run_id || step.map != event->event_key
        || (sSawStep && step.step != sLastStep + 1))
    { puts("STEP_PROBE_BAD"); return; }
    struct RhhWorldEnvironmentSnapshot environment = {.struct_size = sizeof(environment)};
    struct RhhWorldEnvironmentSnapshot byMap = {.struct_size = sizeof(byMap)};
    if (sEnvironment->current(&environment) || sEnvironment->at(step.map, &byMap)
        || environment.map != step.map || environment.step != step.step || byMap.step != step.step
        || environment.weather.value != byMap.weather.value || environment.terrain.value != byMap.terrain.value)
    { puts("ENVIRONMENT_PROBE_BAD"); return; }
    struct RhhHabitatSample habitat = {.struct_size = sizeof(habitat)};
    unsigned calls = sHabitatCalls;
    if (sHabitat->sample(step.x, step.y, &habitat) || sHabitatCalls != calls + 1
        || !habitat.permitted || !(habitat.flags & RHH_HABITAT_GRASS)
        || habitat.map != step.map || habitat.environment.step != step.step)
    { puts("HABITAT_PROBE_BAD"); return; }
    int found = 0;
    for (uint32_t slot = 0; slot < sWorld->actor_slots(); ++slot)
    {
        struct RhhWorldActor actor = {.struct_size = sizeof(actor)};
        if (!sWorld->actor_at(slot, &actor) && (actor.flags & RHH_ACTOR_PLAYER)
            && actor.x == step.x && actor.y == step.y && actor.map == step.map) { found = 1; break; }
    }
    if (!found) { puts("STEP_PROBE_BAD_POSITION"); return; }
    sLastStep = step.step;
    if (!sSawStep) { puts("STEP_PROBE_OK"); puts("ENVIRONMENT_PROBE_OK"); puts("HABITAT_PROBE_OK"); }
    sSawStep = 1;
}

static void Probe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    if (sDone) return;
    struct RhhWorldCamera camera = {.struct_size = sizeof(camera)};
    if (sWorld->camera(&camera)) return;
    for (uint32_t slot = 0; slot < sWorld->actor_slots(); ++slot)
    {
        struct RhhWorldActor actor = {.struct_size = sizeof(actor)};
        struct RhhWorldBillboard billboard = {.struct_size = sizeof(billboard)};
        if (sWorld->actor_at(slot, &actor) || !(actor.flags & RHH_ACTOR_PLAYER)
            || sWorld->billboard(slot, &billboard)) continue;
        if (sReady)
        {
            int32_t dx = billboard.world_anchor_x - sLastX, dy = billboard.world_anchor_y - sLastY;
            if (dx < -4 || dx > 4 || dy < -4 || dy > 4)
            { puts("SCENE_PROBE_BAD_MOTION"); sDone = 1; return; }
            sLastX = billboard.world_anchor_x; sLastY = billboard.world_anchor_y;
            if (billboard.world_anchor_x >= sStartX + 16)
            { puts("SCENE_PROBE_MOVED"); sDone = 1; }
            return;
        }
        /* Only assert tile alignment when the player is stationary. */
        if (actor.x != actor.previous_x || actor.y != actor.previous_y || billboard.offset_x || billboard.offset_y) return;
        struct RhhSceneTile scene = {.struct_size = sizeof(scene)};
        struct RhhWorldTile tile = {.struct_size = sizeof(tile)};
        if (sScene->tile_at(actor.x, actor.y, &scene) || sWorld->tile_at(actor.x, actor.y, &tile)
            || scene.map != actor.map || scene.rule != sRuleKey || scene.tile.metatile != tile.metatile
            || scene.style.surface != RHH_SCENE_WATER || scene.style.upper != RHH_SCENE_UPPER_WALL_NORTH
            || scene.style.ground_height != 4 || scene.style.upper_height != 16)
        { puts("SCENE_PROBE_BAD_RULE"); sDone = 1; return; }
        if (camera.map != actor.map || billboard.world_anchor_x != actor.x * 16 + 8
            || billboard.world_anchor_y != actor.y * 16 + 16 || !billboard.part_count)
        {
            puts("SCENE_PROBE_BAD_ANCHOR"); sDone = 1; return;
        }
        for (uint32_t part = 0; part < billboard.part_count; ++part)
        {
            struct RhhWorldSpritePart view = {.struct_size = sizeof(view)};
            uint8_t pixels[64 * 64 * 4];
            if (sWorld->sprite_part(slot, part, &view) || view.width > 64 || view.height > 64
                || sWorld->sprite_part_rgba(slot, part, pixels, sizeof(pixels)))
            {
                puts("SCENE_PROBE_BAD_SPRITE"); sDone = 1; return;
            }
        }
        sStartX = sLastX = billboard.world_anchor_x; sLastY = billboard.world_anchor_y;
        puts("SCENE_PROBE_OK"); sReady = 1; return;
    }
}

static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Probe},
    {sizeof(struct RhhModHook), RHH_WORLD_STEP, RHH_OBSERVE, 0, NULL, ProbeStep},
    {sizeof(struct RhhModHook), RHH_HABITAT_QUERY, RHH_TRANSFORM, 0, HabitatRule, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/overworld"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/world_scene"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/world_clock"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/world_environment"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/habitat"}
};
static const struct RhhModSceneRule sRules[] = {{
    .struct_size = sizeof(struct RhhModSceneRule), .id = "test:scene/probe",
    .style = {RHH_SCENE_WATER, RHH_SCENE_UPPER_WALL_NORTH, RHH_SCENE_OCCLUDES, 4, 16}
}};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SCENE,
    .mod_version = 1, .state_schema = 1, .id = "test:scene_probe",
    .hook_count = 3, .hooks = sHooks,
    .service_requirement_count = 5, .service_requirements = sRequirements,
    .scene_rule_count = 1, .scene_rules = sRules
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sWorld = (const void *)host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 1));
    sScene = (const void *)host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 0));
    sClock = (const void *)host->get_service("rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
    sEnvironment = (const void *)host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    sHabitat = (const void *)host->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 0));
    sRuleKey = host->content_key(sRules[0].id);
    return sWorld && sWorld->header.struct_size >= RHH_OVERWORLD_SERVICE_V1_1_SIZE
        && sScene && sScene->header.struct_size >= RHH_WORLD_SCENE_SERVICE_V1_0_SIZE
        && sClock && sClock->header.struct_size >= sizeof(*sClock)
        && sEnvironment && sEnvironment->header.struct_size >= sizeof(*sEnvironment)
        && sHabitat && sHabitat->header.struct_size >= sizeof(*sHabitat) ? &sDefinition : NULL;
}
