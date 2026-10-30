/* SDL-owned OpenGL 2.1 presenter. All GL entry points are loaded at runtime so
 * machines without OpenGL retain the ordinary SDL renderer and weather text. */
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "battle_shader.h"
#include "mod_host.h"

#define GL_FUNCTIONS(X) \
    X(void, GenTextures, (GLsizei n, GLuint *textures)) \
    X(void, DeleteTextures, (GLsizei n, const GLuint *textures)) \
    X(void, BindTexture, (GLenum target, GLuint texture)) \
    X(GLboolean, IsTexture, (GLuint texture)) \
    X(void, GetIntegerv, (GLenum pname, GLint *value)) \
    X(void, GetTexLevelParameteriv, (GLenum target, GLint level, GLenum pname, GLint *value)) \
    X(void, GetTexParameteriv, (GLenum target, GLenum pname, GLint *value)) \
    X(void, TexParameteri, (GLenum target, GLenum pname, GLint param)) \
    X(void, TexImage2D, (GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void *data)) \
    X(void, TexSubImage2D, (GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, const void *data)) \
    X(void, ActiveTexture, (GLenum texture)) \
    X(GLuint, CreateShader, (GLenum type)) \
    X(void, ShaderSource, (GLuint shader, GLsizei count, const GLchar *const *source, const GLint *length)) \
    X(void, CompileShader, (GLuint shader)) \
    X(void, GetShaderiv, (GLuint shader, GLenum pname, GLint *value)) \
    X(void, GetShaderInfoLog, (GLuint shader, GLsizei size, GLsizei *length, GLchar *log)) \
    X(void, DeleteShader, (GLuint shader)) \
    X(GLuint, CreateProgram, (void)) \
    X(void, AttachShader, (GLuint program, GLuint shader)) \
    X(void, LinkProgram, (GLuint program)) \
    X(void, GetProgramiv, (GLuint program, GLenum pname, GLint *value)) \
    X(void, GetProgramInfoLog, (GLuint program, GLsizei size, GLsizei *length, GLchar *log)) \
    X(void, DeleteProgram, (GLuint program)) \
    X(void, UseProgram, (GLuint program)) \
    X(GLint, GetUniformLocation, (GLuint program, const GLchar *name)) \
    X(void, Uniform1i, (GLint location, GLint value)) \
    X(void, Uniform1f, (GLint location, GLfloat value)) \
    X(void, Uniform2f, (GLint location, GLfloat x, GLfloat y)) \
    X(void, GenFramebuffers, (GLsizei n, GLuint *buffers)) \
    X(void, DeleteFramebuffers, (GLsizei n, const GLuint *buffers)) \
    X(void, BindFramebuffer, (GLenum target, GLuint buffer)) \
    X(void, FramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textureTarget, GLuint texture, GLint level)) \
    X(GLenum, CheckFramebufferStatus, (GLenum target)) \
    X(void, Viewport, (GLint x, GLint y, GLsizei w, GLsizei h)) \
    X(void, ClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a)) \
    X(void, Clear, (GLbitfield mask)) \
    X(void, Begin, (GLenum mode)) \
    X(void, End, (void)) \
    X(void, TexCoord2f, (GLfloat x, GLfloat y)) \
    X(void, Vertex2f, (GLfloat x, GLfloat y)) \
    X(void, PixelStorei, (GLenum name, GLint value)) \
    X(void, ReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void *data)) \
    X(const GLubyte *, GetString, (GLenum name))
#define DECLARE(ret, name, args) static ret (APIENTRY *p##name) args;
GL_FUNCTIONS(DECLARE)
#undef DECLARE
/* Optional: depth readback must not prevent ordinary rendering on old drivers. */
static void (APIENTRY *pGetTexImage)(GLenum, GLint, GLenum, GLenum, void *);
static void (APIENTRY *pBindBuffer)(GLenum, GLuint);
static GLenum (APIENTRY *pGetError)(void);

uint16_t gNativeShaderUiPalettes;
uint8_t gNativeShaderSceneMask[240 * 160];
static SDL_Window *sWindow;
static SDL_GLContext sContext;
static GLuint sTextures[8], sFramebuffer, sWorldFramebuffer, sProtectFramebuffer, sMaskFramebuffer;
static GLuint sPass, sWorldComposite, sOverlayComposite, sProtectComposite, sMaskComposite;
static struct { RhhContentKey key; GLuint program; } sPrograms[256];
static unsigned sProgramCount;
static uint8_t sRgb[240 * 160 * 3];
static uint8_t sOverlayRgba[240 * 160 * 4];
static uint8_t sWorldMask[240 * 160], sEffectiveMask[240 * 160];
static int sPreviewAvailable;
static const uint16_t *sPhotoCompatiblePixels;
static unsigned sPhotoCompatibleFrame;
static uint64_t sPhotoMaskTicket;
static uint64_t sPhotoDepthTicket;
static const uint16_t *sPhotoRenderPixels;
static struct RhhPhotoRenderContext sPhotoRender;
static RhhContentKey sPhotoRenderVariant;
static uint16_t sPhotoDepth[240 * 160];
static int sPhotoMaskExact;
static struct RhhPhotoFootprint sPhotoFootprint;
static uint8_t sPhotoMask[240 * 160], sPhotoSilhouette[240 * 160];
static const char sVertex[] = "#version 120\nvarying vec2 vUv; void main(){vUv=gl_MultiTexCoord0.xy;gl_Position=gl_Vertex;}";
static const char sPassFragment[] = "#version 120\nuniform sampler2D uScene; varying vec2 vUv; void main(){gl_FragColor=texture2D(uScene,vUv);}";
static const char sWorldCompositeFragment[] = "#version 120\nuniform sampler2D uScene,uMask,uWorld; varying vec2 vUv; void main(){vec4 scene=texture2D(uScene,vUv); vec4 world=texture2D(uWorld,vec2(vUv.x,1.-vUv.y)); gl_FragColor=mix(scene,world,step(0.25,texture2D(uMask,vUv).r));}";
static const char sOverlayCompositeFragment[] = "#version 120\nuniform sampler2D uScene,uOverlay; varying vec2 vUv; void main(){vec4 scene=texture2D(uScene,vec2(vUv.x,1.-vUv.y)); vec4 overlay=texture2D(uOverlay,vUv); gl_FragColor=mix(scene,overlay,overlay.a);}";
static const char sProtectCompositeFragment[] = "#version 120\nuniform sampler2D uScene,uMask,uWorld; varying vec2 vUv; void main(){vec2 f=vec2(vUv.x,1.-vUv.y); vec4 base=texture2D(uScene,f), effect=texture2D(uWorld,f); gl_FragColor=mix(base,effect,step(0.25,texture2D(uMask,vUv).r));}";
static const char sMaskCompositeFragment[] = "#version 120\nuniform sampler2D uOriginal,uProjected; varying vec2 vUv; void main(){float original=texture2D(uOriginal,vUv).r; float p=texture2D(uProjected,vec2(vUv.x,1.-vUv.y)).r; float m=original<.25 ? 0. : (p<.75 ? 128./255. : 1.); gl_FragColor=vec4(m,m,m,1.);}";

static GLuint Compile(GLenum type, const char *source)
{
    GLuint shader = pCreateShader(type);
    pShaderSource(shader, 1, &source, NULL); pCompileShader(shader);
    GLint okay = 0; pGetShaderiv(shader, GL_COMPILE_STATUS, &okay);
    if (!okay)
    {
        char log[1024]; pGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Battle shader compile: %s\n", log);
        pDeleteShader(shader); return 0;
    }
    return shader;
}

static GLuint Program(const char *fragment)
{
    GLuint vertex = Compile(GL_VERTEX_SHADER, sVertex), pixel = Compile(GL_FRAGMENT_SHADER, fragment);
    GLuint program = 0;
    if (vertex && pixel)
    {
        program = pCreateProgram(); pAttachShader(program, vertex); pAttachShader(program, pixel); pLinkProgram(program);
        GLint okay = 0; pGetProgramiv(program, GL_LINK_STATUS, &okay);
        if (!okay)
        {
            char log[1024]; pGetProgramInfoLog(program, sizeof(log), NULL, log);
            fprintf(stderr, "Battle shader link: %s\n", log); pDeleteProgram(program); program = 0;
        }
    }
    if (vertex) pDeleteShader(vertex);
    if (pixel) pDeleteShader(pixel);
    return program;
}

void NativeShaderShutdown(void)
{
    if (sContext)
    {
        for (unsigned i = 0; i < sProgramCount; ++i)
            if (sPrograms[i].program) pDeleteProgram(sPrograms[i].program);
        if (sPass) pDeleteProgram(sPass);
        if (sWorldComposite) pDeleteProgram(sWorldComposite);
        if (sOverlayComposite) pDeleteProgram(sOverlayComposite);
        if (sProtectComposite) pDeleteProgram(sProtectComposite);
        if (sMaskComposite) pDeleteProgram(sMaskComposite);
        if (sFramebuffer) pDeleteFramebuffers(1, &sFramebuffer);
        if (sWorldFramebuffer) pDeleteFramebuffers(1, &sWorldFramebuffer);
        if (sProtectFramebuffer) pDeleteFramebuffers(1, &sProtectFramebuffer);
        if (sMaskFramebuffer) pDeleteFramebuffers(1, &sMaskFramebuffer);
        if (sTextures[0]) pDeleteTextures(8, sTextures);
        SDL_GL_DeleteContext(sContext);
    }
    sContext = NULL; sWindow = NULL; sPass = sWorldComposite = sOverlayComposite = sFramebuffer = sWorldFramebuffer = sProgramCount = 0;
    sProtectComposite = sProtectFramebuffer = sMaskComposite = sMaskFramebuffer = 0;
    sPreviewAvailable = 0;
    sPhotoCompatiblePixels = NULL;
    sPhotoMaskTicket = 0;
    sPhotoDepthTicket = 0;
    sPhotoRenderPixels = NULL;
    memset(&sPhotoRender, 0, sizeof(sPhotoRender));
    sPhotoRenderVariant = 0;
    sPhotoMaskExact = 0;
    memset(sTextures, 0, sizeof(sTextures));
}

int NativeShaderInit(SDL_Window *window)
{
    sWindow = window; sContext = SDL_GL_CreateContext(window);
    if (!sContext) return -1;
#define LOAD(ret, name, args) do { \
    p##name = (ret (APIENTRY *) args)SDL_GL_GetProcAddress("gl" #name); \
    if (!p##name) p##name = (ret (APIENTRY *) args)SDL_GL_GetProcAddress("gl" #name "EXT"); \
    if (!p##name) { NativeShaderShutdown(); return -1; } \
} while (0);
    GL_FUNCTIONS(LOAD)
#undef LOAD
    pGetTexImage = (void (APIENTRY *)(GLenum, GLint, GLenum, GLenum, void *))SDL_GL_GetProcAddress("glGetTexImage");
    pBindBuffer = (void (APIENTRY *)(GLenum, GLuint))SDL_GL_GetProcAddress("glBindBuffer");
    pGetError = (GLenum (APIENTRY *)(void))SDL_GL_GetProcAddress("glGetError");
    SDL_GL_SetSwapInterval(0);
    sPass = Program(sPassFragment);
    sWorldComposite = Program(sWorldCompositeFragment);
    sOverlayComposite = Program(sOverlayCompositeFragment);
    sProtectComposite = Program(sProtectCompositeFragment);
    sMaskComposite = Program(sMaskCompositeFragment);
    if (!sPass || !sWorldComposite || !sOverlayComposite || !sProtectComposite || !sMaskComposite) { NativeShaderShutdown(); return -1; }
    pPixelStorei(GL_UNPACK_ALIGNMENT, 1); pPixelStorei(GL_PACK_ALIGNMENT, 1);
    pGenTextures(8, sTextures);
    for (unsigned i = 0; i < 8; ++i)
    {
        pBindTexture(GL_TEXTURE_2D, sTextures[i]);
        pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLenum format = i == 1 || i == 7 ? GL_LUMINANCE : (i == 4 || i == 6) ? GL_RGBA : GL_RGB;
        pTexImage2D(GL_TEXTURE_2D, 0, format, 240, 160, 0, format, GL_UNSIGNED_BYTE, NULL);
    }
    pGenFramebuffers(1, &sFramebuffer); pBindFramebuffer(GL_FRAMEBUFFER, sFramebuffer);
    pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sTextures[2], 0);
    if (pCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    { NativeShaderShutdown(); return -1; }
    pGenFramebuffers(1, &sProtectFramebuffer); pBindFramebuffer(GL_FRAMEBUFFER, sProtectFramebuffer);
    pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sTextures[5], 0);
    if (pCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    { NativeShaderShutdown(); return -1; }
    pGenFramebuffers(1, &sWorldFramebuffer); pBindFramebuffer(GL_FRAMEBUFFER, sWorldFramebuffer);
    pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sTextures[3], 0);
    if (pCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    { NativeShaderShutdown(); return -1; }
    pGenFramebuffers(1, &sMaskFramebuffer); pBindFramebuffer(GL_FRAMEBUFFER, sMaskFramebuffer);
    pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sTextures[6], 0);
    if (pCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    { NativeShaderShutdown(); return -1; }
    pBindFramebuffer(GL_FRAMEBUFFER, 0);
    return 0;
}

static GLuint FindProgram(RhhContentKey key)
{
    if (!sContext || !sPass || !key) return 0;
    for (unsigned i = 0; i < sProgramCount; ++i)
        if (sPrograms[i].key == key) return sPrograms[i].program;
    const struct RhhModShader *shader = RhhModsFindShader(key);
    if (!shader || sProgramCount == sizeof(sPrograms) / sizeof(*sPrograms)) return 0;
    GLuint program = Program(shader->fragment);
    sPrograms[sProgramCount].key = key; sPrograms[sProgramCount++].program = program;
    return program;
}
int NativeShaderReady(RhhContentKey shader) { return FindProgram(shader) != 0; }
const char *NativeShaderDriver(void) { return sContext && sPass ? (const char *)pGetString(GL_RENDERER) : "SDL"; }

static void Quad(int flip)
{
    pBegin(GL_QUADS);
    pTexCoord2f(0, flip ? 1 : 0); pVertex2f(-1, 1);
    pTexCoord2f(1, flip ? 1 : 0); pVertex2f(1, 1);
    pTexCoord2f(1, flip ? 0 : 1); pVertex2f(1, -1);
    pTexCoord2f(0, flip ? 0 : 1); pVertex2f(-1, -1);
    pEnd();
}
static int CompositeProjectedMask(GLuint texture)
{
    if (!texture || texture == sTextures[6] || !pIsTexture(texture)) return 0;
    pActiveTexture(GL_TEXTURE1); pBindTexture(GL_TEXTURE_2D, texture);
    GLint bound = 0, width = 0, height = 0, filter = 0, base = -1;
    pGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    if ((GLuint)bound != texture) return 0;
    pGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    pGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    pGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
    pGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &base);
    if (width != 240 || height != 160 || base != 0 || (filter != GL_NEAREST && filter != GL_LINEAR)) return 0;
    pBindFramebuffer(GL_FRAMEBUFFER, sMaskFramebuffer); pViewport(0, 0, 240, 160); pUseProgram(sMaskComposite);
    pActiveTexture(GL_TEXTURE0); pBindTexture(GL_TEXTURE_2D, sTextures[1]);
    pActiveTexture(GL_TEXTURE1); pBindTexture(GL_TEXTURE_2D, texture);
    pUniform1i(pGetUniformLocation(sMaskComposite, "uOriginal"), 0);
    pUniform1i(pGetUniformLocation(sMaskComposite, "uProjected"), 1);
    /* Texture 6 is kept in the same top-left sampling orientation as the
     * uploaded mask, although it is written through an OpenGL framebuffer. */
    Quad(1); return 1;
}
static int ValidWorldDepth(GLuint texture)
{
    if (!texture || !pIsTexture(texture)) return 0;
    pActiveTexture(GL_TEXTURE2); pBindTexture(GL_TEXTURE_2D, texture);
    GLint bound = 0, width = 0, height = 0, filter = 0, base = -1, format = 0, compare = 0;
    pGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    pGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    pGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    pGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
    pGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
    pGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &base);
    pGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, &compare);
    return (GLuint)bound == texture && width == 240 && height == 160 && base == 0 && compare == GL_NONE
        && (filter == GL_NEAREST || filter == GL_LINEAR)
        && (format == GL_DEPTH_COMPONENT || format == GL_DEPTH_COMPONENT16 || format == GL_DEPTH_COMPONENT24
            || format == GL_DEPTH_COMPONENT32 || format == GL_DEPTH_COMPONENT32F);
}
static int ReadPhotoDepth(GLuint texture)
{
    if (!pGetTexImage || !pBindBuffer || !pGetError) return -1;
    /* Bound pack buffers and nondefault pack layouts can otherwise turn a
     * CPU pointer into a buffer offset or overrun the fixed-size image. */
    unsigned errors = 0;
    while (pGetError() != GL_NO_ERROR && ++errors < 32) { }
    if (errors == 32) return -1;
    GLint active, binding, buffer, alignment, rowLength, skipRows, skipPixels, swap;
    pGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    pActiveTexture(GL_TEXTURE2); pGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    pGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &buffer);
    pGetIntegerv(GL_PACK_ALIGNMENT, &alignment); pGetIntegerv(GL_PACK_ROW_LENGTH, &rowLength);
    pGetIntegerv(GL_PACK_SKIP_ROWS, &skipRows); pGetIntegerv(GL_PACK_SKIP_PIXELS, &skipPixels);
    pGetIntegerv(GL_PACK_SWAP_BYTES, &swap);
    if (pGetError() != GL_NO_ERROR) { pActiveTexture(active); return -1; }
    pBindBuffer(GL_PIXEL_PACK_BUFFER, 0); pBindTexture(GL_TEXTURE_2D, texture);
    pPixelStorei(GL_PACK_ALIGNMENT, 2); pPixelStorei(GL_PACK_ROW_LENGTH, 0);
    pPixelStorei(GL_PACK_SKIP_ROWS, 0); pPixelStorei(GL_PACK_SKIP_PIXELS, 0); pPixelStorei(GL_PACK_SWAP_BYTES, 0);
    pGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, sPhotoDepth);
    int failed = pGetError() != GL_NO_ERROR;
    pPixelStorei(GL_PACK_ALIGNMENT, alignment); pPixelStorei(GL_PACK_ROW_LENGTH, rowLength);
    pPixelStorei(GL_PACK_SKIP_ROWS, skipRows); pPixelStorei(GL_PACK_SKIP_PIXELS, skipPixels); pPixelStorei(GL_PACK_SWAP_BYTES, swap);
    pBindBuffer(GL_PIXEL_PACK_BUFFER, buffer); pBindTexture(GL_TEXTURE_2D, binding); pActiveTexture(active);
    return failed ? -1 : 0;
}
static void WorldPass(GLuint program, const struct NativeBattleEnvironment *env, unsigned frame, GLuint maskTexture,
                      GLuint depthTexture, int battlerMaskAvailable)
{
    pBindFramebuffer(GL_FRAMEBUFFER, sWorldFramebuffer); pUseProgram(sPass);
    pActiveTexture(GL_TEXTURE0); pBindTexture(GL_TEXTURE_2D, sTextures[2]);
    pUniform1i(pGetUniformLocation(sPass, "uScene"), 0); Quad(0);
    pBindFramebuffer(GL_FRAMEBUFFER, sFramebuffer); pUseProgram(program);
    pActiveTexture(GL_TEXTURE0); pBindTexture(GL_TEXTURE_2D, sTextures[3]);
    pActiveTexture(GL_TEXTURE1); pBindTexture(GL_TEXTURE_2D, maskTexture);
    pActiveTexture(GL_TEXTURE2); pBindTexture(GL_TEXTURE_2D, depthTexture ? depthTexture : sTextures[0]);
    pActiveTexture(GL_TEXTURE3); pBindTexture(GL_TEXTURE_2D, sTextures[7]);
    pUniform1i(pGetUniformLocation(program, "uScene"), 0);
    pUniform1i(pGetUniformLocation(program, "uMask"), 1);
    pUniform1i(pGetUniformLocation(program, "uDepth"), 2);
    pUniform1i(pGetUniformLocation(program, "uDepthAvailable"), !!depthTexture);
    pUniform1i(pGetUniformLocation(program, "uBattlerMask"), 3);
    pUniform1i(pGetUniformLocation(program, "uBattlerMaskAvailable"), battlerMaskAvailable);
    pUniform1f(pGetUniformLocation(program, "uTime"), frame / 60.f);
    pUniform1f(pGetUniformLocation(program, "uWeather"), env->weather);
    pUniform1f(pGetUniformLocation(program, "uTerrain"), env->terrain);
    pUniform1f(pGetUniformLocation(program, "uSuppressed"), env->suppressed);
    pUniform1f(pGetUniformLocation(program, "uFade"), env->visibility);
    pUniform1f(pGetUniformLocation(program, "uDoubles"), env->doubles);
    pUniform2f(pGetUniformLocation(program, "uResolution"), 240, 160);
    Quad(0);
}

static void Present(const uint16_t *pixels, const struct NativeBattleEnvironment *env, unsigned frame, int world,
                    int presenter, const uint16_t *overlay)
{
    sPreviewAvailable = 0;
    sPhotoRenderPixels = NULL;
    memset(&sPhotoRender, 0, sizeof(sPhotoRender));
    sPhotoRenderVariant = 0;
    sPhotoCompatiblePixels = NULL;
    sPhotoMaskTicket = 0;
    sPhotoDepthTicket = 0;
    sPhotoMaskExact = 0;
    for (unsigned i = 0; i < 240 * 160; ++i)
        for (unsigned c = 0; c < 3; ++c)
        {
            unsigned value = (pixels[i] >> (c * 5)) & 31;
            sRgb[i * 3 + c] = (value << 3) | (value >> 2);
        }
    pActiveTexture(GL_TEXTURE0); pBindTexture(GL_TEXTURE_2D, sTextures[0]);
    pTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 240, 160, GL_RGB, GL_UNSIGNED_BYTE, sRgb);
    pActiveTexture(GL_TEXTURE1); pBindTexture(GL_TEXTURE_2D, sTextures[1]);
    pTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 240, 160, GL_LUMINANCE, GL_UNSIGNED_BYTE, gNativeShaderSceneMask);
    const uint8_t *battlerMask = !world && env->battle_scene ? NativeBattleBattlerMask(pixels) : NULL;
    if (battlerMask)
    {
        pActiveTexture(GL_TEXTURE3); pBindTexture(GL_TEXTURE_2D, sTextures[7]);
        pTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 240, 160, GL_LUMINANCE, GL_UNSIGNED_BYTE, battlerMask);
    }
    int rendered = 0; GLuint maskTexture = sTextures[1], depthTexture = 0;
    uint32_t photoSlot = 0, photoValid = 0, photoQuality = RHH_PHOTO_MASK_EXACT; uint64_t photoTicket = 0;
    int wantsPhoto = !RhhModsPhotoTarget(&photoSlot, &photoTicket);
    int actualPhoto = wantsPhoto;
    int wantsProbe = world && !wantsPhoto && !RhhModsProjectedVisibilityTarget(&photoSlot, &photoTicket);
    wantsPhoto |= wantsProbe;
    RhhContentKey drawnPresenter = 0, presenterVariant = 0;
    if (presenter)
    {
        pBindFramebuffer(GL_FRAMEBUFFER, sWorldFramebuffer); pViewport(0, 0, 240, 160);
        memset(sWorldMask, 0, sizeof(sWorldMask));
        uint32_t maskValid = 0, maskTextureOutput = 0, depthTextureOutput = 0;
        memset(sPhotoMask, 255, sizeof(sPhotoMask)); memset(sPhotoSilhouette, 255, sizeof(sPhotoSilhouette));
        sPhotoFootprint = (struct RhhPhotoFootprint){.struct_size = sizeof(sPhotoFootprint), .ticket = photoTicket};
        struct RhhWorldPhotoTarget target = {.struct_size = sizeof(target), .actor_slot = photoSlot, .ticket = photoTicket,
            .visible = sPhotoMask, .silhouette = sPhotoSilhouette, .valid = &photoValid,
            .footprint = wantsProbe ? NULL : &sPhotoFootprint, .mask_quality = &photoQuality,
            .render_variant = actualPhoto ? &presenterVariant : NULL};
        struct RhhWorldFrame view = {.struct_size = sizeof(view), .width = 240, .height = 160, .frame = frame,
            .pixels = pixels, .mask = gNativeShaderSceneMask, .gl_proc = SDL_GL_GetProcAddress,
            .output_mask = sWorldMask, .output_mask_valid = &maskValid, .output_mask_texture = &maskTextureOutput,
            .photo_target = wantsPhoto ? &target : NULL, .output_depth_texture = &depthTextureOutput};
        rendered = RhhModsDrawWorldWithIdentity(&view, &drawnPresenter);
        if (rendered && ValidWorldDepth(depthTextureOutput)) depthTexture = depthTextureOutput;
        if (rendered && maskTextureOutput && CompositeProjectedMask(maskTextureOutput)) maskTexture = sTextures[6];
        if (rendered && maskTexture == sTextures[1] && maskValid == 1)
        {
            unsigned i;
            for (i = 0; i < 240 * 160; ++i)
                if (sWorldMask[i] != 128 && sWorldMask[i] != 255) break;
            if (i == 240 * 160)
            {
                for (i = 0; i < 240 * 160; ++i)
                    sEffectiveMask[i] = gNativeShaderSceneMask[i] < 64 ? 0 : sWorldMask[i];
                pActiveTexture(GL_TEXTURE1); pBindTexture(GL_TEXTURE_2D, sTextures[1]);
                pTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 240, 160, GL_LUMINANCE, GL_UNSIGNED_BYTE, sEffectiveMask);
            }
        }
    }
    GLuint program = rendered ? sWorldComposite : world ? 0 : FindProgram(env->shader);
    if (!program) program = sPass;
    pBindFramebuffer(GL_FRAMEBUFFER, sFramebuffer); pViewport(0, 0, 240, 160); pUseProgram(program);
    pActiveTexture(GL_TEXTURE0); pBindTexture(GL_TEXTURE_2D, sTextures[0]);
    pActiveTexture(GL_TEXTURE1); pBindTexture(GL_TEXTURE_2D, maskTexture);
    pActiveTexture(GL_TEXTURE2); pBindTexture(GL_TEXTURE_2D, rendered ? sTextures[3] : sTextures[0]);
    pUniform1i(pGetUniformLocation(program, "uScene"), 0);
    pUniform1i(pGetUniformLocation(program, "uMask"), 1);
    pUniform1i(pGetUniformLocation(program, "uWorld"), 2);
    pUniform1i(pGetUniformLocation(program, "uDepth"), 2);
    pUniform1i(pGetUniformLocation(program, "uDepthAvailable"), 0);
    pUniform1i(pGetUniformLocation(program, "uBattlerMask"), 3);
    pUniform1i(pGetUniformLocation(program, "uBattlerMaskAvailable"), !!battlerMask);
    pUniform1f(pGetUniformLocation(program, "uTime"), frame / 60.f);
    pUniform1f(pGetUniformLocation(program, "uWeather"), env->weather);
    pUniform1f(pGetUniformLocation(program, "uTerrain"), env->terrain);
    pUniform1f(pGetUniformLocation(program, "uSuppressed"), env->suppressed);
    pUniform1f(pGetUniformLocation(program, "uFade"), env->visibility);
    pUniform1f(pGetUniformLocation(program, "uDoubles"), env->doubles);
    pUniform2f(pGetUniformLocation(program, "uResolution"), 240, 160);
    Quad(0);
    GLuint worldEffect = world ? FindProgram(env->shader) : 0;
    struct RhhWorldRenderPass passes[RHH_WORLD_RENDER_PASS_CAPACITY];
    unsigned passCount = world ? RhhModsCollectWorldPasses(frame, env->weather, env->terrain, passes) : 0;
    struct RhhBattleRenderPass battlePasses[RHH_BATTLE_RENDER_PASS_CAPACITY];
    unsigned battlePassCount = !world ? ModsCollectBattleRenderPasses(env, frame, battlePasses) : 0;
    if (world && actualPhoto)
    {
        sPhotoRenderPixels = pixels;
        sPhotoRender = (struct RhhPhotoRenderContext){.struct_size = sizeof(sPhotoRender),
            .backend = RHH_PHOTO_BACKEND_OPENGL, .frame = frame, .ticket = photoTicket,
            .presenter = drawnPresenter, .environment_shader = worldEffect ? env->shader : 0,
            .flags = depthTexture ? RHH_PHOTO_RENDER_DEPTH_TEXTURE : 0};
        sPhotoRenderVariant = rendered ? presenterVariant : 0;
    }
    int photoPreserving = !worldEffect || RhhModsShaderPhotoPreservesPosition(env->shader);
    int geometryPreserving = !worldEffect || RhhModsShaderPhotoPreservesGeometry(env->shader);
    GLuint passPrograms[RHH_WORLD_RENDER_PASS_CAPACITY];
    GLuint battlePrograms[RHH_BATTLE_RENDER_PASS_CAPACITY];
    for (unsigned i = 0; i < passCount; ++i)
    {
        passPrograms[i] = FindProgram(passes[i].shader);
        if (!passPrograms[i]) continue;
        if (!RhhModsShaderPhotoPreservesPosition(passes[i].shader)) photoPreserving = 0;
        if (!RhhModsShaderPhotoPreservesGeometry(passes[i].shader)) geometryPreserving = 0;
    }
    for (unsigned i = 0; i < battlePassCount; ++i)
        battlePrograms[i] = FindProgram(battlePasses[i].shader);
    if (photoQuality == RHH_PHOTO_MASK_GEOMETRIC) photoPreserving = 0;
    else if (photoQuality != RHH_PHOTO_MASK_EXACT) photoPreserving = geometryPreserving = 0;
    if (world && !rendered && photoPreserving)
    { sPhotoCompatiblePixels = pixels; sPhotoCompatibleFrame = frame; }
    if (world && rendered && wantsPhoto && photoValid == 1 && geometryPreserving)
    {
        unsigned i;
        for (i = 0; i < 240 * 160; ++i)
            if (sPhotoMask[i] > 1 || sPhotoSilhouette[i] > 1 || sPhotoMask[i] > sPhotoSilhouette[i]) break;
        if (i == 240 * 160)
        {
            for (i = 0; i < 240 * 160; ++i)
                if (gNativeShaderSceneMask[i] < 64) sPhotoMask[i] = 0;
            sPhotoCompatiblePixels = pixels; sPhotoCompatibleFrame = frame; sPhotoMaskTicket = photoTicket;
            sPhotoMaskExact = photoPreserving;
            if (!wantsProbe && depthTexture && !ReadPhotoDepth(depthTexture)) sPhotoDepthTicket = photoTicket;
        }
    }
    if (wantsProbe) RhhModsProjectedVisibilityFinish(pixels, frame, 1, world);
    if (worldEffect || passCount || battlePassCount || (!world && env->battle_scene && program != sPass))
    {
        /* Keep the pre-effect scene for restoring protected interface pixels. */
        pBindFramebuffer(GL_FRAMEBUFFER, sProtectFramebuffer); pUseProgram(sPass);
        pActiveTexture(GL_TEXTURE0); pBindTexture(GL_TEXTURE_2D, world ? sTextures[2] : sTextures[0]);
        pUniform1i(pGetUniformLocation(sPass, "uScene"), 0); Quad(world ? 1 : 0);
        if (worldEffect) WorldPass(worldEffect, env, frame, maskTexture, depthTexture, 0);
        for (unsigned i = 0; i < passCount; ++i)
        {
            GLuint effect = passPrograms[i];
            if (effect)
            {
                WorldPass(effect, env, frame, maskTexture, depthTexture, 0);
                if (world && actualPhoto) sPhotoRender.passes[sPhotoRender.pass_count++] = passes[i];
            }
        }
        for (unsigned i = 0; i < battlePassCount; ++i)
            if (battlePrograms[i]) WorldPass(battlePrograms[i], env, frame, maskTexture, 0, !!battlerMask);
        /* Passes cannot paint over BG0 interface pixels, even when a module
         * neglects the mask. Overlay rendering still happens after this. */
        pBindFramebuffer(GL_FRAMEBUFFER, sWorldFramebuffer); pUseProgram(sProtectComposite);
        pActiveTexture(GL_TEXTURE0); pBindTexture(GL_TEXTURE_2D, sTextures[5]);
        pActiveTexture(GL_TEXTURE1); pBindTexture(GL_TEXTURE_2D, maskTexture);
        pActiveTexture(GL_TEXTURE2); pBindTexture(GL_TEXTURE_2D, sTextures[2]);
        pUniform1i(pGetUniformLocation(sProtectComposite, "uScene"), 0);
        pUniform1i(pGetUniformLocation(sProtectComposite, "uMask"), 1);
        pUniform1i(pGetUniformLocation(sProtectComposite, "uWorld"), 2);
        Quad(0);
        pBindFramebuffer(GL_FRAMEBUFFER, sFramebuffer); pUseProgram(sPass);
        pActiveTexture(GL_TEXTURE0); pBindTexture(GL_TEXTURE_2D, sTextures[3]);
        pUniform1i(pGetUniformLocation(sPass, "uScene"), 0); Quad(1);
    }
    GLuint displayTexture = sTextures[2];
    if (overlay)
    {
        for (unsigned i = 0; i < 240 * 160; ++i)
        {
            for (unsigned c = 0; c < 3; ++c)
            { unsigned value = (overlay[i] >> (c * 5)) & 31; sOverlayRgba[i * 4 + c] = (value << 3) | (value >> 2); }
            sOverlayRgba[i * 4 + 3] = (overlay[i] & 0x8000) ? 255 : 0;
        }
        pActiveTexture(GL_TEXTURE1); pBindTexture(GL_TEXTURE_2D, sTextures[4]);
        pTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 240, 160, GL_RGBA, GL_UNSIGNED_BYTE, sOverlayRgba);
        pBindFramebuffer(GL_FRAMEBUFFER, sWorldFramebuffer); pViewport(0, 0, 240, 160); pUseProgram(sOverlayComposite);
        pActiveTexture(GL_TEXTURE0); pBindTexture(GL_TEXTURE_2D, sTextures[2]);
        pUniform1i(pGetUniformLocation(sOverlayComposite, "uScene"), 0);
        pUniform1i(pGetUniformLocation(sOverlayComposite, "uOverlay"), 1);
        Quad(0); displayTexture = sTextures[3]; sPreviewAvailable = 1;
    }
    pBindFramebuffer(GL_FRAMEBUFFER, 0);
    int w, h; SDL_GL_GetDrawableSize(sWindow, &w, &h);
    pViewport(0, 0, w, h); pClearColor(0, 0, 0, 1); pClear(GL_COLOR_BUFFER_BIT);
    int vw = w, vh = w * 2 / 3;
    if (vh > h) { vh = h; vw = h * 3 / 2; }
    pViewport((w - vw) / 2, (h - vh) / 2, vw, vh);
    pUseProgram(sPass); pActiveTexture(GL_TEXTURE0); pBindTexture(GL_TEXTURE_2D, displayTexture);
    pUniform1i(pGetUniformLocation(sPass, "uScene"), 0); Quad(1);
    SDL_GL_SwapWindow(sWindow);
}
void NativeShaderPresent(const uint16_t *pixels, const struct NativeBattleEnvironment *env, unsigned frame)
{ Present(pixels, env, frame, 0, 0, NULL); }
void NativeShaderPresentBattleOverlay(const uint16_t *pixels, const struct NativeBattleEnvironment *env, unsigned frame,
                                      const uint16_t *overlay)
{ Present(pixels, env, frame, 0, 0, overlay); }
void NativeShaderPresentWorld(const uint16_t *pixels, const struct NativeBattleEnvironment *env, unsigned frame)
{ Present(pixels, env, frame, 1, 1, NULL); }
void NativeShaderPresentField(const uint16_t *pixels, const struct NativeBattleEnvironment *env, unsigned frame)
{ Present(pixels, env, frame, 1, 0, NULL); }
void NativeShaderPresentWorldOverlay(const uint16_t *pixels, const struct NativeBattleEnvironment *env, unsigned frame,
                                     const uint16_t *overlay)
{ Present(pixels, env, frame, 1, 1, overlay); }

int NativeShaderCapture(uint8_t *rgb)
{
    if (!sContext || !sPass) return -1;
    pBindFramebuffer(GL_FRAMEBUFFER, sFramebuffer);
    pReadPixels(0, 0, 240, 160, GL_RGB, GL_UNSIGNED_BYTE, sRgb);
    pBindFramebuffer(GL_FRAMEBUFFER, 0);
    for (unsigned y = 0; y < 160; ++y) memcpy(rgb + y * 720, sRgb + (159 - y) * 720, 720);
    return 0;
}
int NativeShaderPhotoVisibilityCompatible(const uint16_t *pixels, unsigned frame)
{ return sContext && sPass && !sPhotoMaskTicket && pixels && pixels == sPhotoCompatiblePixels && frame == sPhotoCompatibleFrame; }
int NativeShaderProjectedMask(uint64_t ticket, const uint16_t *pixels, unsigned frame, uint32_t *total, uint32_t *visible)
{
    if (!sPhotoMaskExact) return -1;
    return NativeShaderGeometricMask(ticket, pixels, frame, total, visible, NULL, NULL);
}
int NativeShaderGeometricMask(uint64_t ticket, const uint16_t *pixels, unsigned frame,
                              uint32_t *total, uint32_t *visible, uint8_t *mask, uint8_t *silhouette)
{
    if (!sContext || !sPass || !ticket || ticket != sPhotoMaskTicket || !pixels || pixels != sPhotoCompatiblePixels
        || (!!mask != !!silhouette)
        || frame != sPhotoCompatibleFrame || !total || !visible) return -1;
    uint32_t count = 0, shown = 0;
    for (unsigned i = 0; i < 240 * 160; ++i) { count += sPhotoSilhouette[i]; shown += sPhotoMask[i]; }
    *total = count; *visible = shown;
    if (mask) { memcpy(mask, sPhotoMask, sizeof(sPhotoMask)); memcpy(silhouette, sPhotoSilhouette, sizeof(sPhotoSilhouette)); }
    return 0;
}
int NativeShaderPhotoMask(uint64_t ticket, const uint16_t *pixels, unsigned frame,
                          uint32_t *total, uint32_t *visible, uint8_t *mask, uint8_t *silhouette)
{
    uint32_t count, shown;
    if (!total || !visible || !mask || !silhouette
        || NativeShaderProjectedMask(ticket, pixels, frame, &count, &shown) || !count) return -1;
    *total = count; *visible = shown;
    memcpy(mask, sPhotoMask, sizeof(sPhotoMask)); memcpy(silhouette, sPhotoSilhouette, sizeof(sPhotoSilhouette));
    return 0;
}
int NativeShaderGeometricFootprint(uint64_t ticket, const uint16_t *pixels, unsigned frame, struct RhhPhotoFootprint *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sContext || !sPass || !ticket || ticket != sPhotoMaskTicket
        || !pixels || pixels != sPhotoCompatiblePixels || frame != sPhotoCompatibleFrame || !sPhotoFootprint.total_pixels) return -1;
    *out = sPhotoFootprint; return 0;
}
int NativeShaderPhotoFootprint(uint64_t ticket, const uint16_t *pixels, unsigned frame, struct RhhPhotoFootprint *out)
{ return !sPhotoMaskExact ? -1 : NativeShaderGeometricFootprint(ticket, pixels, frame, out); }
int NativeShaderPhotoDepth(uint64_t ticket, const uint16_t *pixels, unsigned frame, uint16_t *out)
{
    if (!out || !sContext || !sPass || !ticket || ticket != sPhotoMaskTicket || ticket != sPhotoDepthTicket
        || !pixels || pixels != sPhotoCompatiblePixels || frame != sPhotoCompatibleFrame) return -1;
    for (unsigned y = 0; y < 160; ++y)
        memcpy(out + y * 240, sPhotoDepth + (159 - y) * 240, 240 * sizeof(*out));
    return 0;
}
int NativeShaderPhotoRenderInfo(uint64_t ticket, const uint16_t *pixels, unsigned frame, struct RhhPhotoRenderContext *out, RhhContentKey *variant)
{
    if (!out || out->struct_size < sizeof(*out) || !sContext || !sPass || !ticket
        || ticket != sPhotoRender.ticket || !pixels || pixels != sPhotoRenderPixels || frame != sPhotoRender.frame) return -1;
    *out = sPhotoRender; if (variant) *variant = sPhotoRenderVariant; return 0;
}
int NativeShaderCapturePreview(uint8_t *rgb)
{
    if (!rgb || !sContext || !sPass || !sPreviewAvailable) return -1;
    pBindFramebuffer(GL_FRAMEBUFFER, sWorldFramebuffer);
    pReadPixels(0, 0, 240, 160, GL_RGB, GL_UNSIGNED_BYTE, sRgb);
    pBindFramebuffer(GL_FRAMEBUFFER, 0);
    for (unsigned y = 0; y < 160; ++y) memcpy(rgb + y * 720, sRgb + (159 - y) * 720, 720);
    return 0;
}
