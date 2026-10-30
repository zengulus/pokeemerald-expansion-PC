#ifndef RHH_MOD_UI_H
#define RHH_MOD_UI_H
#include "global.h"
struct SDL_Window;
int RhhUiActive(void);
int RhhUiPanelOwnedBy(u64 owner);
int RhhFileDialogInstallService(void);
void RhhFileDialogSetWindow(struct SDL_Window *window);
void RhhFileDialogCancelOwner(u64 owner);
void RhhFileDialogShutdown(void);
int RhhTextEntryInstallService(void);
void RhhTextEntryAdvance(void);
void RhhTextEntryCancelOwner(u64 owner);
void RhhUiTextReleaseBarrier(void);
void RhhUiAdvance(u16 keys);
void RhhUiAccessibilityFlush(void);
u16 RhhUiFilterKeys(u16 keys);
int RhhUiKeysBlocked(void);
int RhhUiOverlayActive(void);
void RhhUiOverlayReconcile(void);
void RhhUiOverlayAdvance(u16 keys);
int RhhUiOverlayRender(u16 *pixels);
int RhhUiRenderBattleConditions(u16 *pixels, u32 frame);
void RhhUiOverlayComposite(u16 *out, const u16 *scene, const u16 *overlay);
void RhhUiRender(u16 *pixels);
void RhhUiRestore(u16 *pixels);
int RhhUiPreparePartyMenu(u32 slot);
int RhhUiOpenPartyMenu(void);
int RhhUiPartyTakeRefresh(void);
#endif
