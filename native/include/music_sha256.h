#ifndef RHH_MUSIC_SHA256_H
#define RHH_MUSIC_SHA256_H
#include <stddef.h>
#include <stdint.h>
void NativeMusicSha256(const void *data, size_t size, uint8_t out[32]);
#endif
