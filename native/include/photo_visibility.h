#ifndef RHH_PHOTO_VISIBILITY_H
#define RHH_PHOTO_VISIBILITY_H

#include <stdint.h>
#include "mod_api.h"

struct OamData;
void NativePhotoOamBegin(void);
void NativePhotoOamAdd(unsigned first, unsigned end, unsigned sprite);
void NativePhotoOamLoaded(const struct OamData *oam);
void NativePhotoVisibilityRequest(unsigned sprite, uint64_t ticket);
void NativePhotoVisibilityCancel(void);
int NativePhotoVisibilityResult(uint64_t ticket, const uint16_t *pixels,
                                uint32_t *total, uint32_t *visible, uint8_t *mask, uint8_t *silhouette);
int NativePhotoVisibilityFootprint(uint64_t ticket, const uint16_t *pixels, struct RhhPhotoFootprint *out);

#endif
