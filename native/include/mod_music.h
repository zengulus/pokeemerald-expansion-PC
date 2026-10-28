#ifndef RHH_NATIVE_MOD_MUSIC_H
#define RHH_NATIVE_MOD_MUSIC_H
#include <stdint.h>
struct MusicPlayerInfo;
struct SongHeader;
void NativeMusicSetAssetRoot(const char *path);
void NativeMusicOnStart(struct MusicPlayerInfo *, struct SongHeader *);
void NativeMusicOnStop(struct MusicPlayerInfo *);
void NativeMusicOnVolume(struct MusicPlayerInfo *, unsigned volume);
int NativeMusicActive(void);
uint32_t NativeMusicAsset(void);
void NativeMusicMix(float *interleaved, unsigned frames);
void NativeMusicShutdown(void);
#endif
