#ifndef RHH_NATIVE_MOD_MUSIC_H
#define RHH_NATIVE_MOD_MUSIC_H
#include <stddef.h>
#include <stdint.h>
struct MusicPlayerInfo;
struct SongHeader;
void NativeMusicSetAssetRoot(const char *path);
void NativeMusicOnStart(struct MusicPlayerInfo *, struct SongHeader *);
void NativeMusicOnStop(struct MusicPlayerInfo *);
void NativeMusicOnVolume(struct MusicPlayerInfo *, unsigned volume);
int NativeMusicActive(void);
int NativeMusicHasAudio(void);
uint32_t NativeMusicAsset(void);
void NativeMusicLoopInfo(uint64_t *start, uint64_t *end, uint64_t *position);
void NativeMusicMix(float *interleaved, unsigned frames);
void NativeMusicShutdown(void);
void NativeMusicFlushLifecycle(void);
void NativeMusicFlushBattleCue(void);
void NativeMusicClosePreview(uint64_t owner);
int NativeMusicInstallAssignments(void);
uint32_t NativeMusicTrackLevel(uint64_t track);
uint64_t NativeMusicAssignedTrack(uint32_t context, uint64_t original, uint64_t map,
                                  uint64_t trainer_a, uint64_t trainer_b);
void NativeMusicSetImportRoot(const char *path);
int NativeMusicInstallImports(void);
int NativeMusicImportedKnown(uint64_t key);
int NativeMusicImportedRead(uint64_t key, unsigned char **midi, size_t *midi_size,
                            unsigned char **font, size_t *font_size);
int NativeMusicInspectImported(const unsigned char *midi, size_t midi_size,
                               const unsigned char *font, size_t font_size,
                               uint32_t *loop_start, uint32_t *loop_end);
#endif
