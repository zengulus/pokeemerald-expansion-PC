#ifndef RHH_BATTLE_SHADER_H
#define RHH_BATTLE_SHADER_H
#include <stdint.h>
#include "mod_api.h"
typedef struct SDL_Window SDL_Window;
struct NativeBattleEnvironment {
    RhhContentKey shader;
    unsigned weather, terrain, suppressed, doubles, battle_scene;
    float visibility;
};
int NativeShaderInit(SDL_Window *window); /* zero on success */
void NativeShaderShutdown(void);
void NativeShaderPresent(const uint16_t *pixels, const struct NativeBattleEnvironment *, unsigned frame);
void NativeShaderPresentBattleOverlay(const uint16_t *pixels, const struct NativeBattleEnvironment *, unsigned frame,
                                      const uint16_t *overlay);
void NativeShaderPresentWorld(const uint16_t *pixels, const struct NativeBattleEnvironment *, unsigned frame);
void NativeShaderPresentField(const uint16_t *pixels, const struct NativeBattleEnvironment *, unsigned frame);
void NativeShaderPresentWorldOverlay(const uint16_t *pixels, const struct NativeBattleEnvironment *, unsigned frame,
                                     const uint16_t *overlay);
int NativeShaderCapture(uint8_t *rgb); /* 240*160*3 RGB bytes, zero on success */
/* True only when the last captured world frame preserves the input pixels. */
int NativeShaderPhotoVisibilityCompatible(const uint16_t *pixels, unsigned frame);
int NativeShaderPhotoMask(uint64_t ticket, const uint16_t *pixels, unsigned frame,
                          uint32_t *total, uint32_t *visible, uint8_t *mask, uint8_t *silhouette);
int NativeShaderProjectedMask(uint64_t ticket, const uint16_t *pixels, unsigned frame,
                              uint32_t *total, uint32_t *visible);
int NativeShaderGeometricMask(uint64_t ticket, const uint16_t *pixels, unsigned frame,
                              uint32_t *total, uint32_t *visible, uint8_t *mask, uint8_t *silhouette);
int NativeShaderPhotoFootprint(uint64_t ticket, const uint16_t *pixels, unsigned frame, struct RhhPhotoFootprint *out);
int NativeShaderGeometricFootprint(uint64_t ticket, const uint16_t *pixels, unsigned frame, struct RhhPhotoFootprint *out);
int NativeShaderPhotoDepth(uint64_t ticket, const uint16_t *pixels, unsigned frame, uint16_t *out);
int NativeShaderPhotoRenderInfo(uint64_t ticket, const uint16_t *pixels, unsigned frame, struct RhhPhotoRenderContext *out, RhhContentKey *variant);
int NativeShaderCapturePreview(uint8_t *rgb); /* test/display preview; includes overlay */
int NativeShaderReady(RhhContentKey shader);
const char *NativeShaderDriver(void);
extern uint16_t gNativeShaderUiPalettes;
extern uint8_t gNativeShaderSceneMask[240 * 160];
/* Topmost visible pixels attributed to battle sprites: 0 or battler + 1.
 * Borrowed from the last software frame, or NULL when unavailable. */
const uint8_t *NativeBattleBattlerMask(const uint16_t *pixels);
const struct RhhServiceHeader *ModsBattleBackdropService(void);
struct NativeBattleEnvironment ModsBattleEnvironment(void);
void ModsBattleShaderUpdateUiPalettes(void);
unsigned ModsCollectBattleRenderPasses(const struct NativeBattleEnvironment *env, unsigned frame,
                                       struct RhhBattleRenderPass *out);
struct NativeBattleEnvironment ModsWorldEnvironment(void);
void ModsSetBattleEnvironment(unsigned enabled);
unsigned ModsBattleEnvironmentEnabled(void);
int ModsShowWeatherRecap(void);
#endif
