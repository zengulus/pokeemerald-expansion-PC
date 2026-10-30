#include "mod_api.h"
#include <stdio.h>
#ifndef PASS_B
#define PASS_B 0
#endif
static const struct RhhModHostV1 *host;
static const struct RhhBattleRenderService *render;
static const struct RhhBattleBackdropService *backdrop;
#if PASS_B
#define MODULE_ID "test:battle_pass_b"
#define SHADER_ID "test:shader/battle_b"
#define PASS_ID "test:battle_pass/b"
#define PRIORITY 20
static const char fragment[] = "#version 120\nuniform sampler2D uScene,uBattlerMask; uniform bool uBattlerMaskAvailable; uniform float uTime; varying vec2 vUv;"
    "void main(){ if((uTime>.183 && uTime<.19)||(uTime>.199 && uTime<.205)){float n=floor(texture2D(uBattlerMask,vUv).r*255.+.5);"
    "gl_FragColor=!uBattlerMaskAvailable?vec4(1.,0.,1.,1.):n==1.?vec4(1.,0.,0.,1.):n==2.?vec4(0.,1.,0.,1.):vec4(0.,0.,1.,1.); return;}"
    "float r=texture2D(uScene,vUv).r; gl_FragColor=vec4(0.,r,0.,1.);}";
#else
#define MODULE_ID "test:battle_pass_a"
#define SHADER_ID "test:shader/battle_a"
#define PASS_ID "test:battle_pass/a"
#define PRIORITY 10
static const char fragment[] = "#version 120\nuniform float uWeather,uTerrain,uSuppressed,uDoubles,uFade; uniform bool uDepthAvailable; varying vec2 vUv;"
    "void main(){float r=(uWeather==6. && uTerrain==3. && uSuppressed==1. && uDoubles==1. && uFade==.5 && !uDepthAvailable)? .25:.5; gl_FragColor=vec4(r,0.,0.,1.);}";
#endif
static void Offer(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->ordinal == 13 || event->ordinal == 14) {
        struct RhhBattleBackdropInfo info = {.struct_size = sizeof(info)};
        const char *id = event->ordinal == 13 ? "rhh:battle_backdrop/grass" : "rhh:battle_backdrop/frontier";
        if (!backdrop || backdrop->current(&info) || info.key != host->content_key(id) || !info.has_artwork)
            puts("BATTLE_PASS_BAD_BACKDROP");
    }
    if (event->ordinal == 10) {
        if (event->args[0] || event->args[1] || event->args[2] || event->args[3]) puts("BATTLE_PASS_BAD_ARGS");
    } else if (event->args[0] != RHH_WEATHER_FOG || event->args[1] != RHH_TERRAIN_ELECTRIC
        || event->args[2] != 1 || event->args[3] != 1) puts("BATTLE_PASS_BAD_ARGS");
    if (event->ordinal == 12) return;
    if (event->ordinal == 8) decision->allowed = 0;
    struct RhhBattleRenderPass pass = {.struct_size = sizeof(pass), .priority = PRIORITY,
        .id = host->content_key(event->ordinal == 4 ? "test:battle_pass/conflict" : PASS_ID),
        .shader = host->content_key(SHADER_ID)};
    if (event->ordinal == 7) pass.priority = 10;
#if !PASS_B
    if (event->ordinal == 5) {
        for (unsigned i = 0; i < RHH_BATTLE_RENDER_PASS_CAPACITY + 1; ++i) {
            char id[48]; snprintf(id, sizeof(id), "test:battle_pass/overflow_%u", i);
            pass.id = host->content_key(id);
            int status = render->offer(&pass);
            if ((i < RHH_BATTLE_RENDER_PASS_CAPACITY && status) || (i == RHH_BATTLE_RENDER_PASS_CAPACITY && !status))
                puts("BATTLE_PASS_BAD_OVERFLOW");
        }
        return;
    }
    if (event->ordinal == 6) {
        pass.shader = host->content_key("test:shader/missing");
        if (!render->offer(&pass)) puts("BATTLE_PASS_BAD_INVALID");
        return;
    }
#endif
    int status = render->offer(&pass);
    if ((event->ordinal == 4 || event->ordinal == 5) ? (!PASS_B && status) : status)
        puts("BATTLE_PASS_BAD_OFFER");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_RENDER_PASSES, RHH_CONSTRAINT, 0, Offer, NULL}
};
static const struct RhhModShader shader = {sizeof(shader), 0, SHADER_ID, fragment};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle_render"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle_backdrop"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = MODULE_ID, .hook_count = 1, .hooks = hooks, .shader_count = 1, .shaders = &shader,
    .service_requirement_count = sizeof(requirements) / sizeof(requirements[0]), .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *api)
{
    if (!api || !api->get_service || !api->content_key) return NULL;
    host = api; render = (const void *)api->get_service("rhh:service/battle_render", RHH_SERVICE_VERSION(1, 0));
    backdrop = (const void *)api->get_service("rhh:service/battle_backdrop", RHH_SERVICE_VERSION(1, 1));
    uint8_t pixels[8 * 8 * 4];
    if (!backdrop || backdrop->image_rgba(api->content_key("rhh:battle_backdrop/grass"), 8, 8, 8, 8, pixels, sizeof(pixels))) return NULL;
    return render && backdrop && render->header.struct_size >= RHH_BATTLE_RENDER_SERVICE_V1_0_SIZE
        && backdrop->header.struct_size >= RHH_BATTLE_BACKDROP_SERVICE_V1_1_SIZE ? &definition : NULL;
}
