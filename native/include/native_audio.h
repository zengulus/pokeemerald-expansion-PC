#ifndef NATIVE_AUDIO_H
#define NATIVE_AUDIO_H
#include "music_player.h"
#include "sound_mixer.h"
#include "cgb_audio.h"
void Platform_QueueAudio(float *samples, s32 bytes);
void RunMixerFrame(void);
void MP2KPlayerMain(void *player);
void MP2K_event_nxx(u8 clock, struct MP2KPlayerState *, struct MP2KTrack *);
void MP2K_event_lfos(struct MP2KPlayerState *, struct MP2KTrack *);
void MP2K_event_mod(struct MP2KPlayerState *, struct MP2KTrack *);
void MP2K_event_endtie(struct MP2KPlayerState *, struct MP2KTrack *);
void MP2KClearChain(struct MixerSource *);
#define NATIVE_MUSIC_EVENT(name) void MP2K_event_##name(struct MP2KPlayerState *, struct MP2KTrack *)
NATIVE_MUSIC_EVENT(fine);
NATIVE_MUSIC_EVENT(goto);
NATIVE_MUSIC_EVENT(patt);
NATIVE_MUSIC_EVENT(pend);
NATIVE_MUSIC_EVENT(rept);
NATIVE_MUSIC_EVENT(prio);
NATIVE_MUSIC_EVENT(tempo);
NATIVE_MUSIC_EVENT(keysh);
NATIVE_MUSIC_EVENT(voice);
NATIVE_MUSIC_EVENT(vol);
NATIVE_MUSIC_EVENT(pan);
NATIVE_MUSIC_EVENT(bend);
NATIVE_MUSIC_EVENT(bendr);
NATIVE_MUSIC_EVENT(lfodl);
NATIVE_MUSIC_EVENT(modt);
NATIVE_MUSIC_EVENT(tune);
NATIVE_MUSIC_EVENT(port);
#undef NATIVE_MUSIC_EVENT
#endif
