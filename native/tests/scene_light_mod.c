/* Independent module combining map lights and a state-controlled actor light. */
#include "mod_api.h"
#include <string.h>
static const struct RhhModHostV1 *sHost;
static const struct RhhWorldRenderService *sRender;
static const struct RhhWorldSceneService *sScene;
static const struct RhhOverworldService *sWorld;
static struct RhhModSceneLight lights[] = {
    {.struct_size = sizeof(struct RhhModSceneLight), .id = "test:light/oldale_fixture", .x = 8 * 16 + 8,
     .y = 8, .height = 12, .radius = 48, .rgb = 0xff20ff, .intensity = 900, .period_mask = 1u << (RHH_TIME_NIGHT - 1)},
    {.struct_size = sizeof(struct RhhModSceneLight), .id = "test:light/oldale_offscreen", .x = 18 * 16 + 8,
     .y = 8, .height = 12, .radius = 64, .rgb = 0x20ffff, .intensity = 900, .period_mask = 1u << (RHH_TIME_NIGHT - 1)}
};
static void OfferActorLight(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    unsigned mode = ((const unsigned char *)state)[0];
    if (!mode) return;
    struct RhhWorldLightContext context = {.struct_size = sizeof(context)};
    struct RhhWorldActor actor = {.struct_size = sizeof(actor)};
    struct RhhWorldBillboard billboard = {.struct_size = sizeof(billboard)};
    struct RhhSceneWindowTile tile = {.struct_size = sizeof(tile)};
    if (sRender->light_context(&context) || sWorld->actor_at(0, &actor)
        || (actor.flags & (RHH_ACTOR_HIDDEN | RHH_ACTOR_OFFSCREEN)) || sWorld->billboard(0, &billboard)
        || sScene->window_resolve(actor.x, actor.y, &tile)) return;
    struct RhhWorldDynamicLight light = {.struct_size = sizeof(light), .id = sHost->content_key("test:light/follow_actor"),
        .map = context.lighting.map, .x = billboard.world_anchor_x, .y = billboard.world_anchor_y,
        .z = tile.scene.style.ground_height + 16, .radius = 64, .rgb = 0x40b0ff, .intensity = 800};
    if (mode == 5) { light.x = 33 * 16 + 8; light.y = 13 * 16 - 8; light.z = 20; light.radius = 96; }
    if (sRender->offer_light(&light)) return;
    if (mode == 2) (void)sRender->offer_light(&light); /* duplicate */
    if (mode == 3) { light.id = sHost->content_key("test:light/invalid"); light.radius = 0; (void)sRender->offer_light(&light); }
    if (mode == 4) decision->allowed = 0;
}
static int Read(const void *state, const char *id, struct RhhModValue *out)
{
    if (strcmp(id, "mode")) return -1;
    *out = (struct RhhModValue){RHH_SETTING_CHOICE, 0, ((const unsigned char *)state)[0]}; return 0;
}
static int Write(void *state, const char *id, const struct RhhModValue *value)
{
    if (strcmp(id, "mode") || !value || value->type != RHH_SETTING_CHOICE || value->value > 5) return -1;
    ((unsigned char *)state)[0] = value->value; return 0;
}
static int Validate(const uint8_t *state, uint32_t size) { return size == 1 && state[0] <= 5 ? 0 : -1; }
static const char *const modes[] = {"Off", "Follow", "Duplicate", "Invalid", "Deny", "Wall"};
static const struct RhhModSetting settings[] = {
    {sizeof(struct RhhModSetting), RHH_SETTING_CHOICE, RHH_SETTING_RUNTIME, 0, "mode", "Test light", "Choose a test mode.", 0, 5, 6, 0, modes}
};
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_WORLD_LIGHTS, RHH_CONSTRAINT, 0, OfferActorLight, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 4), RHH_SERVICE_REQUIRED, 0, "rhh:service/world_scene"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 6), RHH_SERVICE_REQUIRED, 0, "rhh:service/overworld"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/world_render"}
};
static const struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_SCENE_LIGHTS,
    .id = "test:scene_light_module", .state_size = 1, .validate = Validate,
    .setting_count = 1, .settings = settings, .read_setting = Read, .write_setting = Write,
    .hook_count = 1, .hooks = hooks, .scene_light_count = 2, .scene_lights = lights,
    .service_requirement_count = 3, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->content_key || !host->get_service) return NULL;
    sHost = host;
    sScene = (const void *)host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 4));
    sWorld = (const void *)host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 6));
    sRender = (const void *)host->get_service("rhh:service/world_render", RHH_SERVICE_VERSION(1, 1));
    if (!sScene || sScene->header.struct_size < RHH_WORLD_SCENE_SERVICE_V1_4_SIZE
        || !sWorld || sWorld->header.struct_size < RHH_OVERWORLD_SERVICE_V1_6_SIZE
        || !sRender || sRender->header.struct_size < RHH_WORLD_RENDER_SERVICE_V1_1_SIZE) return NULL;
    lights[0].map = lights[1].map = host->content_key("rhh:map/oldale_town");
    return &mod;
}
