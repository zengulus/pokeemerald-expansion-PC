#include "mod_api.h"
#include <stdint.h>
#ifdef _WIN32
#define GL_CALL __stdcall
#else
#define GL_CALL
#endif
static const struct RhhModHostV1 *host;
static unsigned textures[5];
static void (GL_CALL *deleteTextures)(int, const unsigned *);
static uint16_t samples[240 * 160];
static int Draw(const struct RhhWorldFrame *frame, const void *state)
{
    (void)state;
    void (GL_CALL *clearColor)(float, float, float, float) = frame->gl_proc("glClearColor");
    void (GL_CALL *clear)(unsigned) = frame->gl_proc("glClear");
    void (GL_CALL *gen)(int, unsigned *) = frame->gl_proc("glGenTextures");
    void (GL_CALL *bind)(unsigned, unsigned) = frame->gl_proc("glBindTexture");
    void (GL_CALL *param)(unsigned, unsigned, int) = frame->gl_proc("glTexParameteri");
    void (GL_CALL *image)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *) = frame->gl_proc("glTexImage2D");
    void (GL_CALL *enable)(unsigned) = frame->gl_proc("glEnable");
    void (GL_CALL *disable)(unsigned) = frame->gl_proc("glDisable");
    void (GL_CALL *scissor)(int, int, int, int) = frame->gl_proc("glScissor");
    deleteTextures = frame->gl_proc("glDeleteTextures");
    if (!clearColor || !clear || !gen || !bind || !param || !image || !deleteTextures || !enable || !disable || !scissor) return -1;
    if (frame->struct_size >= RHH_WORLD_FRAME_V1_4_SIZE && frame->output_depth_texture && frame->frame >= 12 && frame->frame <= 19)
    {
        if (!textures[0])
        {
            for (unsigned y = 0; y < 160; ++y) for (unsigned x = 0; x < 240; ++x)
                samples[y * 240 + x] = y < 80 ? 16384 : 49151;
            gen(5, textures);
            for (unsigned i = 0; i < 5; ++i)
            {
                if (i == 4) for (unsigned y = 0; y < 160; ++y) for (unsigned x = 0; x < 240; ++x)
                    samples[y * 240 + x] = x < 120 ? 30146 : 58982; /* .46 and .90 */
                bind(0x0de1, textures[i]); /* GL_TEXTURE_2D */
                param(0x0de1, 0x2801, 0x2600); param(0x0de1, 0x2800, 0x2600); /* nearest */
                if (i == 3) param(0x0de1, 0x884c, 0x884e); /* depth comparison */
                image(0x0de1, 0, i == 2 ? 0x8058 : 0x81a6, i == 1 ? 239 : 240, 160, 0,
                      i == 2 ? 0x1908 : 0x1902, i == 2 ? 0x1401 : 0x1403, i == 2 ? NULL : samples);
            }
        }
        if (frame->frame == 12) *frame->output_depth_texture = textures[0];
        if (frame->frame == 13) *frame->output_depth_texture = textures[1];
        if (frame->frame == 15) *frame->output_depth_texture = textures[2];
        if (frame->frame == 16) *frame->output_depth_texture = textures[3];
        if (frame->frame == 17 || frame->frame == 18) *frame->output_depth_texture = textures[4];
    }
    if (frame->frame >= 17 && frame->frame <= 19)
    {
        enable(0x0c11); /* GL_SCISSOR_TEST */
        for (int x = 0; x < 240; x += 2)
        {
            float value = (x / 2) % 2 ? 1.f : 0.f;
            scissor(x, 0, 2, 160); clearColor(value, value, value, 1); clear(0x4000);
        }
        disable(0x0c11);
    }
    else { clearColor(1, 0, 0, 1); clear(0x4000); }
    return 0;
}
static void Shutdown(void)
{
    if (deleteTextures) deleteTextures(5, textures);
    for (unsigned i = 0; i < 5; ++i) textures[i] = 0;
}
static void Select(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->value = host->content_key("test:world/depth_presenter"); }
static const struct RhhModHook hooks[] = {{sizeof(struct RhhModHook), RHH_WORLD_PRESENTATION, RHH_TRANSFORM, 1000, Select, NULL}};
static const struct RhhWorldPresenter presenter = {sizeof(presenter), 0, "test:world/depth_presenter", Draw, Shutdown};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:depth_presenter", .hooks = hooks, .hook_count = 1, .world_presenters = &presenter, .world_presenter_count = 1};
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *api)
{ if (!api || !api->content_key) return NULL; host = api; return &definition; }
