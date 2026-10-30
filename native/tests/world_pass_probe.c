/* Compile twice with PASS_B=0/1 to test independent world-pass modules. */
#include "mod_api.h"
#include <stdio.h>
#ifndef PASS_B
#define PASS_B 0
#endif
static const struct RhhModHostV1 *sHost;
static const struct RhhWorldRenderService *sRender;
#if PASS_B
#define MODULE_ID "test:render_b"
#define SHADER_ID "test:shader/world_b"
#define PASS_ID "test:pass/b"
#define PRIORITY 20
static const char sFragment[] = "#version 120\nuniform sampler2D uScene; varying vec2 vUv; void main(){float r=texture2D(uScene,vUv).r; gl_FragColor=vec4(0.,r,0.,1.);}";
#else
#define MODULE_ID "test:render_a"
#define SHADER_ID "test:shader/world_a"
#define PASS_ID "test:pass/a"
#define PRIORITY 10
static const char sFragment[] = "#version 120\nuniform sampler2D uDepth; uniform bool uDepthAvailable; uniform float uTime; varying vec2 vUv;"
    "void main(){float r=.25; if(uTime>=.199 && uTime<.284) r=uDepthAvailable?texture2D(uDepth,vec2(vUv.x,1.-vUv.y)).r:.5;"
    "gl_FragColor=vec4(r,0.,0.,1.);}";
#endif
static void Offer(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->ordinal >= 17 && event->ordinal <= 19) return;
    struct RhhWorldRenderPass pass = {.struct_size = sizeof(pass), .priority = PRIORITY,
        .id = sHost->content_key(event->ordinal == 4 ? "test:pass/conflict" : PASS_ID),
        .shader = sHost->content_key(SHADER_ID)};
    if (event->ordinal == 7) pass.priority = 10;
#if !PASS_B
    if (event->ordinal == 5) {
        for (unsigned i = 0; i < RHH_WORLD_RENDER_PASS_CAPACITY + 1; ++i) {
            char name[32]; snprintf(name, sizeof(name), "test:pass/overflow_%u", i);
            pass.id = sHost->content_key(name);
            int result = sRender->offer(&pass);
            if ((i < RHH_WORLD_RENDER_PASS_CAPACITY && result) || (i == RHH_WORLD_RENDER_PASS_CAPACITY && !result))
                puts("WORLD_PASS_BAD");
        }
        return;
    }
    if (event->ordinal == 6) {
        pass.shader = sHost->content_key("test:shader/missing");
        if (!sRender->offer(&pass)) puts("WORLD_PASS_BAD");
        return;
    }
#endif
    int result = sRender->offer(&pass);
    if ((event->ordinal == 4 || event->ordinal == 5) ? (!PASS_B && result) : result)
        puts("WORLD_PASS_BAD");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_WORLD_RENDER_PASSES, RHH_CONSTRAINT, 0, Offer, NULL}
};
static const struct RhhModShader sShader = {sizeof(sShader), 0, SHADER_ID, sFragment};
static const struct RhhServiceRequirement sReq[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/world_render"}
};
static const struct RhhModDefinition sMod = {.struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = MODULE_ID, .hook_count = 1, .hooks = sHooks, .shader_count = 1, .shaders = &sShader,
    .service_requirement_count = 1, .service_requirements = sReq};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sHost = host; sRender = (const void *)host->get_service("rhh:service/world_render", RHH_SERVICE_VERSION(1, 0));
    return sRender && sRender->header.struct_size >= RHH_WORLD_RENDER_SERVICE_V1_0_SIZE ? &sMod : NULL;
}
