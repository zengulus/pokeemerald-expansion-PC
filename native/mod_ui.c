/* A bounded, software-rendered modal surface for trusted native modules. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "main.h"
#include "battle_main.h"
#include "overworld.h"
#include "palette.h"
#include "random.h"
#include "script.h"
#include "mod_api.h"
#include "mod_host.h"
#include "mod_ui.h"
#include "mod_music.h"
#include "native_controls.h"
#include "fonts.h"
#include "constants/characters.h"
#include "constants/rgb.h"

#define W DISPLAY_WIDTH
#define H DISPLAY_HEIGHT
static struct RhhUiPanel sPanel;
static struct RhhUiOverlay sOverlay;
static RhhContentKey sPanelWriter, sOverlayWriter, sInputOwner;
static unsigned sInputKind;
static u16 sOverlayPrevious;
static unsigned sOverlayRepeat;
static u16 sBase[W * H], *sCanvas;
static u16 sPrevious;
static unsigned sRepeat;
static int sHaveBase, sClosing, sBlockKeys;
static int sTextBarrier;
static int sAccessibleDrawing, sAccessibleDirty;
static struct RhhUiAccessibleSnapshot sAccessible = {
    .struct_size = sizeof(sAccessible), .content = {.struct_size = sizeof(sAccessible.content)}
};
static struct RhhUiAccessibleSnapshot sOverlayAccessible = {
    .struct_size = sizeof(sOverlayAccessible), .content = {.struct_size = sizeof(sOverlayAccessible.content)}
};
enum { MAX_PARTY_ACTIONS = 32, MAX_PARTY_LABEL = 25 };
struct PartyAction {
    struct RhhUiPartyAction action;
    char id[RHH_MOD_ID_MAX], label[MAX_PARTY_LABEL];
    RhhContentKey verifiedOwner;
};
static struct PartyAction sPartyActions[MAX_PARTY_ACTIONS];
static struct RhhOwnedMonSnapshot sPartyMon;
static unsigned sPartyActionCount, sPartyCursor, sPartyStatus;
static int sCollectingParty, sPartyModal, sPartyRefresh;

static int Active(void) { return sPanel.owner != 0; }
static RhhContentKey InputOwner(void)
{
    if (!sInputOwner) return 0;
    if (sInputKind == 1 && Active() && sPanel.owner == sInputOwner && sPanelWriter == sInputOwner) return sInputOwner;
    if (sInputKind == 2 && RhhUiOverlayActive() && sOverlay.owner == sInputOwner && sOverlayWriter == sInputOwner) return sInputOwner;
    return 0;
}
static int AccessibleTextValid(const char *text, size_t capacity)
{
    const unsigned char *p = (const unsigned char *)text;
    const unsigned char *end = memchr(text, 0, capacity);
    if (!end) return 0;
    while (p < end)
    {
        u32 cp = *p++, need, minimum;
        if (cp < 0x80) { if (cp < 0x20 && cp != '\n' && cp != '\t') return 0; if (cp == 0x7f) return 0; continue; }
        if (cp >= 0xc2 && cp <= 0xdf) { need = 1; minimum = 0x80; cp &= 0x1f; }
        else if (cp >= 0xe0 && cp <= 0xef) { need = 2; minimum = 0x800; cp &= 0x0f; }
        else if (cp >= 0xf0 && cp <= 0xf4) { need = 3; minimum = 0x10000; cp &= 0x07; }
        else return 0;
        if ((size_t)(end - p) < need) return 0;
        while (need--) { if ((*p & 0xc0) != 0x80) return 0; cp = (cp << 6) | (*p++ & 0x3f); }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)
            || (cp >= 0x80 && cp <= 0x9f)) return 0;
    }
    return 1;
}
static int AccessiblePublish(RhhContentKey owner, const struct RhhUiAccessibleContent *content)
{
    struct RhhUiAccessibleSnapshot *snapshot = sAccessibleDrawing == 1 ? &sAccessible : &sOverlayAccessible;
    if (!owner || (sAccessibleDrawing == 1 ? (!Active() || sPanel.owner != owner)
        : sAccessibleDrawing == 2 ? (!RhhUiOverlayActive() || sOverlay.owner != owner) : 1) || !content
        || content->struct_size < sizeof(*content) || content->reserved || !content->title[0]
        || !AccessibleTextValid(content->title, sizeof(content->title))
        || !AccessibleTextValid(content->body, sizeof(content->body))
        || !AccessibleTextValid(content->focus, sizeof(content->focus))) return -1;
    struct RhhUiAccessibleContent copy = {.struct_size = sizeof(copy)};
    strcpy(copy.title, content->title); strcpy(copy.body, content->body); strcpy(copy.focus, content->focus);
    if (!snapshot->active || snapshot->owner != owner || memcmp(&snapshot->content, &copy, sizeof(copy)))
    {
        snapshot->active = 1; snapshot->owner = owner; snapshot->content = copy;
        ++snapshot->revision; sAccessibleDirty |= sAccessibleDrawing;
    }
    return 0;
}
static int AccessibleSnapshot(struct RhhUiAccessibleSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    *out = sAccessible; return 0;
}
static int OverlayAccessibleSnapshot(struct RhhUiAccessibleSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    *out = sOverlayAccessible; return 0;
}
static void ClearAccessible(int overlay)
{
    struct RhhUiAccessibleSnapshot *snapshot = overlay ? &sOverlayAccessible : &sAccessible;
    if (!snapshot->active) return;
    snapshot->active = 0; snapshot->owner = 0;
    memset(&snapshot->content, 0, sizeof(snapshot->content));
    snapshot->content.struct_size = sizeof(snapshot->content);
    ++snapshot->revision; sAccessibleDirty |= overlay ? 2 : 1;
}
void RhhUiAccessibilityFlush(void)
{
    if (!sAccessibleDirty || RhhModsCurrentEvent() || sAccessibleDrawing) return;
    for (int overlay = 0; overlay < 2; ++overlay)
    {
        int bit = overlay ? 2 : 1;
        if (!(sAccessibleDirty & bit)) continue;
        sAccessibleDirty &= ~bit;
        const struct RhhUiAccessibleSnapshot *snapshot = overlay ? &sOverlayAccessible : &sAccessible;
        struct RhhModEvent event = {.struct_size = sizeof(event),
            .kind = overlay ? RHH_UI_OVERLAY_ACCESSIBILITY_CHANGED : RHH_UI_ACCESSIBILITY_CHANGED,
            .run_seed = RhhModsRunSeed(), .subject = snapshot->owner, .ordinal = snapshot->revision,
            .args = {snapshot->active, 0, 0, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        RhhModsDispatch(&event, &decision);
        gRngValue = rng; gRng2Value = rng2;
    }
}
static const struct RhhUiAccessibilityService sAccessibilityService = {
    {sizeof(sAccessibilityService), RHH_SERVICE_VERSION(1, 1)}, AccessiblePublish, AccessibleSnapshot, OverlayAccessibleSnapshot
};
int RhhUiOverlayActive(void) { return sOverlay.owner != 0; }
int RhhUiActive(void) { return Active(); }
int RhhUiPanelOwnedBy(u64 owner) { return owner && sPanel.owner == owner; }
static int OverlayReady(void)
{
    return RhhModsHasActiveRun() && !gMain.inBattle && gMain.callback2 == CB2_Overworld
        && !gPaletteFade.active && !gPaletteFade.y && !ScriptContext_IsEnabled()
        && !ArePlayerFieldControlsLocked() && !IsOverworldLinkActive();
}
static void CloseOverlay(unsigned reason)
{
    if (!RhhUiOverlayActive()) return;
    struct RhhUiOverlay old = sOverlay; memset(&sOverlay, 0, sizeof(sOverlay));
    sOverlayWriter = 0;
    ClearAccessible(1);
    sBlockKeys = 1; sClosing = 1; RhhModsSetDrawing(1);
    if (old.closed) old.closed(reason, old.user);
    RhhModsSetDrawing(0); sClosing = 0;
}
static void ClosePanel(unsigned reason)
{
    if (!Active()) return;
    struct RhhUiPanel old = sPanel;
    memset(&sPanel, 0, sizeof(sPanel));
    sPanelWriter = 0;
    ClearAccessible(0);
    NativeMusicClosePreview(old.owner);
    RhhFileDialogCancelOwner(old.owner);
    RhhTextEntryCancelOwner(old.owner);
    if (sPartyModal) { sPartyRefresh = 1; sPartyModal = 0; }
    sClosing = 1;
    RhhModsSetDrawing(1);
    if (old.closed) old.closed(reason, old.user);
    RhhModsSetDrawing(0);
    sClosing = 0;
}
static void Reset(void)
{ CloseOverlay(RHH_UI_RESET); ClosePanel(RHH_UI_RESET); sAccessibleDirty = 0; sPartyActionCount = 0; sCollectingParty = sPartyRefresh = 0; }
void RhhUiTextReleaseBarrier(void) { sTextBarrier = 1; }
static int Close(RhhContentKey owner)
{
    if (!RhhModsCommandsAllowed() || !Active() || sPanel.owner != owner) return -1;
    ClosePanel(RHH_UI_CLOSED); return 0;
}
static int Open(const struct RhhUiPanel *panel)
{
    if (RhhModsCurrentEvent() != RHH_MOD_UPDATE || !RhhModsCommandsAllowed()
        || Active() || RhhUiOverlayActive() || sClosing || !panel || panel->struct_size < sizeof(*panel)
        || panel->reserved || !panel->draw || !RhhModsRegisteredKey(panel->owner)) return -1;
    sPanel = *panel; sPrevious = gMain.heldKeysRaw; sRepeat = 0; sHaveBase = 0; sBlockKeys = 1;
    sPanelWriter = panel->owner == RhhModsCurrentOwner() ? panel->owner : 0;
    return 0;
}
static int OverlayOpen(const struct RhhUiOverlay *overlay)
{
    if (RhhModsCurrentEvent() != RHH_MOD_UPDATE || !RhhModsCommandsAllowed() || !OverlayReady()
        || Active() || RhhUiOverlayActive() || sClosing || sBlockKeys || !overlay
        || overlay->struct_size < sizeof(*overlay) || overlay->reserved || !overlay->draw
        || !RhhModsRegisteredKey(overlay->owner)) return -1;
    sOverlay = *overlay; sOverlayPrevious = gMain.heldKeysRaw; sOverlayRepeat = 0;
    sOverlayWriter = overlay->owner == RhhModsCurrentOwner() ? overlay->owner : 0; return 0;
}
static int OverlayClose(RhhContentKey owner)
{
    if (!RhhModsCommandsAllowed() || !RhhUiOverlayActive() || sOverlay.owner != owner) return -1;
    CloseOverlay(RHH_UI_CLOSED); return 0;
}
static int Input(struct RhhUiInput *out)
{
    if (!out || out->struct_size < sizeof(*out) || RhhModsCurrentEvent() != RHH_MOD_UPDATE) return -1;
    *out = (struct RhhUiInput){sizeof(*out), gMain.heldKeysRaw, gMain.newKeysRaw, gMain.newAndRepeatedKeys};
    return 0;
}
static int KeyboardBinding(u32 button, struct RhhUiKeyboardBinding *out)
{
    unsigned action;
    switch (button)
    {
    case RHH_UI_A: action = CONTROL_A; break;
    case RHH_UI_B: action = CONTROL_B; break;
    case RHH_UI_START: action = CONTROL_START; break;
    case RHH_UI_SELECT: action = CONTROL_SELECT; break;
    case RHH_UI_L: action = CONTROL_L; break;
    case RHH_UI_R: action = CONTROL_R; break;
    case RHH_UI_UP: action = CONTROL_UP; break;
    case RHH_UI_DOWN: action = CONTROL_DOWN; break;
    case RHH_UI_LEFT: action = CONTROL_LEFT; break;
    case RHH_UI_RIGHT: action = CONTROL_RIGHT; break;
    default: return -1;
    }
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhUiKeyboardBinding result = {.struct_size = sizeof(result), .button = button};
    for (unsigned i = 0; i < 2; ++i)
    {
        const char *name = NativeControlsKeyName(action, i);
        size_t length = strlen(name);
        if (length >= sizeof(result.keys[i])) return -1;
        if (length) { memcpy(result.keys[i], name, length + 1); ++result.count; }
    }
    if (!result.count) return -1;
    *out = result; return 0;
}
static int PartyContext(struct RhhOwnedMonSnapshot *out)
{
    if (!sCollectingParty || RhhModsCurrentEvent() != RHH_PARTY_MENU_ACTIONS
        || !out || out->struct_size < sizeof(*out)) return -1;
    *out = sPartyMon; return 0;
}
static int ValidActionId(const char *id)
{
    if (!id || !*id) return 0;
    unsigned n = 0, colon = 0;
    for (; *id; ++id, ++n)
    {
        if (n >= RHH_MOD_ID_MAX - 1 || !((*id >= 'a' && *id <= 'z') || (*id >= '0' && *id <= '9')
            || *id == ':' || *id == '/' || *id == '-' || *id == '_' || *id == '.')) return 0;
        if (*id == ':' && (!n || !id[1] || colon++)) return 0;
    }
    return colon == 1;
}
static int OfferPartyAction(const struct RhhUiPartyAction *action)
{
    if (!sCollectingParty || RhhModsCurrentEvent() != RHH_PARTY_MENU_ACTIONS
        || !action || action->struct_size < sizeof(*action) || action->reserved || !action->open
        || !RhhModsRegisteredKey(action->owner) || !ValidActionId(action->id)
        || !action->label || !*action->label || strlen(action->label) >= MAX_PARTY_LABEL
        || sPartyActionCount >= MAX_PARTY_ACTIONS) return -1;
    for (const unsigned char *p = (const unsigned char *)action->label; *p; ++p)
        if (*p < 32 || *p > 126) return -1;
    for (unsigned i = 0; i < sPartyActionCount; ++i)
        if (RhhContentKeyOf(sPartyActions[i].id) == RhhContentKeyOf(action->id)) return -1;
    struct PartyAction *entry = &sPartyActions[sPartyActionCount++];
    entry->action = *action;
    entry->verifiedOwner = action->owner == RhhModsCurrentOwner() ? action->owner : 0;
    strcpy(entry->id, action->id); strcpy(entry->label, action->label);
    return 0;
}
static int ComparePartyActions(const void *a, const void *b)
{ return strcmp(((const struct PartyAction *)a)->id, ((const struct PartyAction *)b)->id); }
static const struct RhhUiService sService = {
    {sizeof(sService), RHH_SERVICE_VERSION(1, 5)}, Input, Open, Close, Active, PartyContext, OfferPartyAction,
    OverlayOpen, OverlayClose, RhhUiOverlayActive, KeyboardBinding
};
int RhhModsInstallUiService(void)
{
    if (RhhModsSetLifecycleCallback(Reset) || RhhModsSetUiInputOwnerProvider(InputOwner)) return -1;
    return RhhModsAddService("rhh:service/ui", &sService.header)
        || RhhModsAddService("rhh:service/ui_accessibility", &sAccessibilityService.header)
        || RhhFileDialogInstallService() || RhhTextEntryInstallService() ? -1 : 0;
}
int RhhUiPreparePartyMenu(u32 slot)
{
    if (Active() || RhhUiOverlayActive() || sCollectingParty) return 0;
    sPartyActionCount = 0;
    if (!RhhModsHasActiveRun()) return 0;
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    sPartyMon.struct_size = sizeof(sPartyMon);
    if (!mons || mons->party_at(slot, &sPartyMon)) return 0;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_PARTY_MENU_ACTIONS,
        .run_seed = RhhModsRunSeed(), .subject = sPartyMon.identity, .args = {slot, 0, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sCollectingParty = 1; RhhModsDispatch(&event, &decision); sCollectingParty = 0;
    qsort(sPartyActions, sPartyActionCount, sizeof(sPartyActions[0]), ComparePartyActions);
    for (unsigned i = 0; i < sPartyActionCount; ++i)
    { sPartyActions[i].action.id = sPartyActions[i].id; sPartyActions[i].action.label = sPartyActions[i].label; }
    return sPartyActionCount;
}
static void PartyChooserDraw(const struct RhhUiCanvas *canvas, void *user)
{
    (void)user;
    canvas->rect(16, 8, 208, 144, 0x2108); canvas->rect(18, 10, 204, 140, 0x7fff);
    canvas->text(25, 14, "Pokemon mod actions", 0x2108, 0x56b5);
    unsigned first = (sPartyCursor / 6) * 6;
    for (unsigned i = first; i < sPartyActionCount && i < first + 6; ++i) {
        canvas->text(25, 34 + (i - first) * 16, i == sPartyCursor ? ">" : " ", 0x2108, 0x56b5);
        canvas->text(39, 34 + (i - first) * 16, sPartyActions[i].label, 0x2108, 0x56b5);
    }
    canvas->text(25, 134, sPartyStatus == 1 ? "Pokemon changed - close" :
                 sPartyStatus == 2 ? "Action unavailable" : "A Open  B Back", 0x2108, 0x56b5);
}
static void PartyChooserInput(const struct RhhUiInput *input, void *user)
{
    (void)user;
    if (input->pressed & RHH_UI_B) { ClosePanel(RHH_UI_CLOSED); return; }
    if (input->repeated & RHH_UI_UP) { sPartyCursor = (sPartyCursor + sPartyActionCount - 1) % sPartyActionCount; sPartyStatus = 0; }
    if (input->repeated & RHH_UI_DOWN) { sPartyCursor = (sPartyCursor + 1) % sPartyActionCount; sPartyStatus = 0; }
    if (!(input->pressed & RHH_UI_A)) return;
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    struct RhhOwnedMonSnapshot now = {.struct_size = sizeof(now)};
    if (!mons || mons->party_at(sPartyMon.slot, &now) || !RhhMonSnapshotsEqual(&now, &sPartyMon))
    { sPartyStatus = 1; return; }
    struct PartyAction *entry = &sPartyActions[sPartyCursor];
    struct RhhUiPanel panel = {.struct_size = sizeof(panel)};
    RhhModsSetDrawing(1);
    int result = entry->action.open(&now, &panel, entry->action.user);
    RhhModsSetDrawing(0);
    if (result || panel.struct_size < sizeof(panel) || panel.reserved || !panel.draw
        || panel.owner != entry->action.owner || !RhhModsRegisteredKey(panel.owner))
    { sPartyStatus = 2; return; }
    sPanel = panel;
    sPanelWriter = entry->verifiedOwner;
}
int RhhUiOpenPartyMenu(void)
{
    if (!RhhModsHasActiveRun() || Active() || RhhUiOverlayActive() || !sPartyActionCount) return -1;
    sPartyCursor = sPartyStatus = 0; sPartyModal = 1; sPartyRefresh = 0; sHaveBase = 0; sBlockKeys = 1;
    sPrevious = gMain.heldKeysRaw; sRepeat = 0;
    sPanel = (struct RhhUiPanel){sizeof(sPanel), 0, RhhContentKeyOf("rhh:host_run"),
                                PartyChooserInput, PartyChooserDraw, NULL, NULL};
    sPanelWriter = 0;
    return 0;
}
int RhhUiPartyTakeRefresh(void) { int result = sPartyRefresh; sPartyRefresh = 0; return result; }
u16 RhhUiFilterKeys(u16 keys)
{
    if (RhhUiOverlayActive()) return 0;
    if (!sBlockKeys) return keys;
    if (!keys) sBlockKeys = 0;
    return 0;
}
int RhhUiKeysBlocked(void) { return sBlockKeys; }
void RhhUiOverlayReconcile(void)
{ if (RhhUiOverlayActive() && !OverlayReady()) CloseOverlay(RHH_UI_RESET); }
void RhhUiOverlayAdvance(u16 keys)
{
    if (!RhhUiOverlayActive()) return;
    keys &= KEYS_MASK; u16 pressed = keys & ~sOverlayPrevious;
    if (keys && keys == sOverlayPrevious) ++sOverlayRepeat; else sOverlayRepeat = 0;
    u16 repeated = pressed;
    if (sOverlayRepeat >= 20 && (sOverlayRepeat - 20) % 5 == 0) repeated |= keys;
    sOverlayPrevious = keys;
    struct RhhUiInput input = {sizeof(input), keys, pressed, repeated};
    if (sOverlay.input)
    {
        RhhContentKey previous = sInputOwner; unsigned kind = sInputKind;
        sInputOwner = sOverlayWriter; sInputKind = 2;
        sOverlay.input(&input, sOverlay.user);
        sInputOwner = previous; sInputKind = kind;
    }
    RhhUiAccessibilityFlush();
}
void RhhUiAdvance(u16 keys)
{
    if (!Active()) return;
    RhhTextEntryAdvance();
    if (sTextBarrier) { if (!keys) sTextBarrier = 0; keys = 0; }
    keys &= KEYS_MASK;
    u16 pressed = keys & ~sPrevious;
    if (keys && keys == sPrevious) ++sRepeat; else sRepeat = 0;
    u16 repeated = pressed;
    if (sRepeat >= 20 && (sRepeat - 20) % 5 == 0) repeated |= keys;
    sPrevious = keys;
    struct RhhUiInput input = {sizeof(input), keys, pressed, repeated};
    if (sPanel.input)
    {
        RhhContentKey previous = sInputOwner; unsigned kind = sInputKind;
        sInputOwner = sPanelWriter; sInputKind = 1;
        sPanel.input(&input, sPanel.user);
        sInputOwner = previous; sInputKind = kind;
    }
    RhhUiAccessibilityFlush();
}
static int Valid(int32_t n) { return n >= -32768 && n <= 32767; }
static void Pixel(int x, int y, u16 c)
{
    if ((unsigned)x < W && (unsigned)y < H) sCanvas[y * W + x] = c | 0x8000;
}
static void Rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    if (!sCanvas || w <= 0 || h <= 0) return;
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    if (right > W) right = W;
    if (bottom > H) bottom = H;
    for (int yy = top; yy < bottom; ++yy)
        for (int xx = left; xx < right; ++xx) Pixel(xx, yy, c);
}
static int OutCode(int x, int y)
{ return (x < 0 ? 1 : x >= W ? 2 : 0) | (y < 0 ? 4 : y >= H ? 8 : 0); }
static void Line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t c)
{
    if (!sCanvas || !Valid(x1) || !Valid(y1) || !Valid(x2) || !Valid(y2)) return;
    for (;;) {
        int a = OutCode(x1, y1), b = OutCode(x2, y2), o = a ? a : b;
        if (!(a | b)) break;
        if (a & b) return;
        int64_t x = x1, y = y1, dx = (int64_t)x2 - x1, dy = (int64_t)y2 - y1;
        if (o & 4) { x += dx * (0 - y1) / dy; y = 0; }
        else if (o & 8) { x += dx * (H - 1 - y1) / dy; y = H - 1; }
        else if (o & 1) { y += dy * (0 - x1) / dx; x = 0; }
        else { y += dy * (W - 1 - x1) / dx; x = W - 1; }
        if (o == a) { x1 = x; y1 = y; } else { x2 = x; y2 = y; }
    }
    int dx = x2 > x1 ? x2 - x1 : x1 - x2, sx = x1 < x2 ? 1 : -1;
    int dy = y2 > y1 ? y1 - y2 : y2 - y1, sy = y1 < y2 ? 1 : -1, err = dx + dy;
    for (;;) { Pixel(x1, y1, c); if (x1 == x2 && y1 == y2) break;
        int e = 2 * err; if (e >= dy) { err += dy; x1 += sx; } if (e <= dx) { err += dx; y1 += sy; } }
}
static void Polygon(const struct RhhUiPoint *p, uint32_t n, uint16_t c)
{
    if (!sCanvas || !p || n < 3 || n > 32) return;
    for (unsigned i = 0; i < n; ++i) if (!Valid(p[i].x) || !Valid(p[i].y)) return;
    for (int y = 0; y < H; ++y) {
        int xs[32], count = 0;
        for (unsigned i = 0, j = n - 1; i < n; j = i++) {
            int x1 = p[j].x, y1 = p[j].y, x2 = p[i].x, y2 = p[i].y;
            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y))
                xs[count++] = x1 + (int)((int64_t)(x2 - x1) * (y - y1) / (y2 - y1));
        }
        for (int i = 1; i < count; ++i) { int v = xs[i], j = i; while (j && xs[j-1] > v) { xs[j] = xs[j-1]; --j; } xs[j] = v; }
        for (int i = 0; i + 1 < count; i += 2) Rect(xs[i], y, xs[i+1] - xs[i] + 1, 1, c);
    }
}
static unsigned Glyph(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return CHAR_A + c - 'A';
    if (c >= 'a' && c <= 'z') return CHAR_a + c - 'a';
    if (c >= '0' && c <= '9') return CHAR_0 + c - '0';
    switch (c) { case ' ': return CHAR_SPACE; case '!': return CHAR_EXCL_MARK;
    case '.': return CHAR_PERIOD; case '-': return CHAR_HYPHEN; case ':': return CHAR_COLON;
    case ',': return CHAR_COMMA; case ';': return CHAR_SEMICOLON; case '&': return CHAR_AMPERSAND;
    case '"': return CHAR_DBL_QUOTE_RIGHT; case '/': return CHAR_SLASH; case '>': return CHAR_GREATER_THAN;
    case '<': return CHAR_LESS_THAN; case '+': return CHAR_PLUS; case '=': return CHAR_EQUALS;
    case '(': return CHAR_LEFT_PAREN; case ')': return CHAR_RIGHT_PAREN; case '%': return CHAR_PERCENT;
    case '\'': return CHAR_SGL_QUOTE_RIGHT; default: return CHAR_QUESTION_MARK; }
}
static void Text(int32_t x, int32_t y, const char *s, uint16_t color, uint16_t shadow)
{
    if (!sCanvas || !s || !Valid(x) || !Valid(y)) return;
    int start = x;
    for (unsigned i = 0; i < 512 && s[i]; ++i) {
        if (s[i] == '\n') { x = start; y += 16; if (y >= H) break; continue; }
        unsigned g = Glyph((unsigned char)s[i]), width = gFontNormalLatinGlyphWidths[g];
        const u16 *font = gFontNormalLatinGlyphs + 32 * g;
        for (unsigned yy = 0; yy < 15; ++yy) for (unsigned xx = 0; xx < width && xx < 16; ++xx) {
            u16 row = font[(yy / 8) * 16 + (xx / 8) * 8 + yy % 8];
            unsigned pos = xx % 8, shift = 14 - pos * 2;
            unsigned pixel = (row >> shift) & 3;
            if (pixel == 1 || pixel == 2) Pixel(x + xx, y + yy, pixel == 2 ? shadow : color);
        }
        x += width + 1; if (x >= W) break;
    }
}
static unsigned SmallTextWidth(const char *s)
{
    unsigned width = 0;
    for (unsigned i = 0; s[i] && i < 95; ++i)
        width += gFontSmallNarrowLatinGlyphWidths[Glyph((unsigned char)s[i])] + 1;
    return width;
}
static void SmallText(int x, int y, const char *s, u16 color, u16 shadow)
{
    for (unsigned i = 0; s[i] && i < 95; ++i)
    {
        unsigned glyph = Glyph((unsigned char)s[i]);
        unsigned width = gFontSmallNarrowLatinGlyphWidths[glyph];
        const u16 *font = gFontSmallNarrowLatinGlyphs + 32 * glyph;
        for (unsigned yy = 0; yy < 13; ++yy)
            for (unsigned xx = 0; xx < width && xx < 16; ++xx)
            {
                u16 row = font[(yy / 8) * 16 + (xx / 8) * 8 + yy % 8];
                unsigned pixel = (row >> (14 - (xx % 8) * 2)) & 3;
                if (pixel == 1 || pixel == 2) Pixel(x + xx, y + yy, pixel == 2 ? shadow : color);
            }
        x += width + 1;
    }
}
int RhhUiRenderBattleConditions(u16 *pixels, u32 frame)
{
    if (!pixels || sCanvas || Active() || !gMain.inBattle || gMain.callback2 != BattleMainCB2
        || (REG_DISPCNT & DISPCNT_FORCED_BLANK) || gPaletteFade.active || gPaletteFade.y
        || !RhhModsHasActiveRun()) return 0;
    const struct RhhBattleService *battle = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/battle", RHH_SERVICE_VERSION(1, 4));
    u32 count;
    if (!battle || battle->active_condition_count(&count) || !count) return 0;
    u32 index = (frame / 180u) % count;
    struct RhhActiveBattleCondition condition = {.struct_size = sizeof(condition)};
    if (battle->active_condition_at(index, &condition)) return 0;
    const char *side = condition.target == RHH_CONDITION_PLAYER_SIDE ? "Your side"
        : condition.target == RHH_CONDITION_OPPONENT_SIDE ? "Foe side" : "Field";
    char label[sizeof(condition.label)], text[96];
    memcpy(label, condition.label, sizeof(label)); label[sizeof(label) - 1] = 0;
    size_t length = strlen(label); int shortened = 0;
    for (;;)
    {
        if (count > 1)
            snprintf(text, sizeof(text), "%s: %s%s  %u turn%s  %u/%u", side, label, shortened ? "..." : "",
                     condition.remaining_turns, condition.remaining_turns == 1 ? "" : "s", index + 1, count);
        else
            snprintf(text, sizeof(text), "%s: %s%s  %u turn%s", side, label, shortened ? "..." : "",
                     condition.remaining_turns, condition.remaining_turns == 1 ? "" : "s");
        if (SmallTextWidth(text) <= W - 14 || !length) break;
        label[--length] = 0; shortened = 1;
    }
    unsigned width = SmallTextWidth(text) + 10;
    if (width > W - 4) width = W - 4;
    memset(pixels, 0, W * H * sizeof(*pixels)); sCanvas = pixels;
    Rect(2, 1, width, 14, RGB(5, 9, 12));
    Rect(3, 2, width - 2, 12, RGB(28, 30, 29));
    Rect(3, 2, 3, 12, condition.target == RHH_CONDITION_PLAYER_SIDE ? RGB(5, 21, 18)
        : condition.target == RHH_CONDITION_OPPONENT_SIDE ? RGB(26, 13, 7) : RGB(9, 16, 27));
    SmallText(8, 1, text, RGB(4, 7, 9), RGB(19, 22, 21));
    sCanvas = NULL; return 1;
}
static int ImageRgb(int32_t x, int32_t y, int32_t w, int32_t h, const struct RhhUiRgbImage *image)
{
    if (!sCanvas || !image || image->struct_size < sizeof(*image) || !image->pixels
        || !image->width || !image->height || image->width > 4096 || image->height > 4096
        || image->stride < image->width * 3u || w <= 0 || h <= 0 || w > 32767 || h > 32767
        || (u64)(image->height - 1) * image->stride + image->width * 3u > image->size) return -1;
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    if (right > W) right = W;
    if (bottom > H) bottom = H;
    for (int yy = top; yy < bottom; ++yy)
    {
        u32 sy = (u32)(((int64_t)yy - y) * image->height / h);
        for (int xx = left; xx < right; ++xx)
        {
            u32 sx = (u32)(((int64_t)xx - x) * image->width / w);
            const u8 *p = image->pixels + (size_t)sy * image->stride + sx * 3u;
            Pixel(xx, yy, (p[0] >> 3) | (p[1] >> 3) << 5 | (p[2] >> 3) << 10);
        }
    }
    return 0;
}
static int ImageRgba(int32_t x, int32_t y, int32_t w, int32_t h, const struct RhhUiRgbaImage *image)
{
    if (!sCanvas || !image || image->struct_size < sizeof(*image) || !image->pixels
        || !image->width || !image->height || image->width > 4096 || image->height > 4096
        || !image->alpha_threshold || image->alpha_threshold > 255 || image->stride < image->width * 4u
        || w <= 0 || h <= 0 || w > 32767 || h > 32767
        || (u64)(image->height - 1) * image->stride + image->width * 4u > image->size) return -1;
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    if (right > W) right = W;
    if (bottom > H) bottom = H;
    for (int yy = top; yy < bottom; ++yy) {
        u32 sy = (u32)(((int64_t)yy - y) * image->height / h);
        for (int xx = left; xx < right; ++xx) {
            u32 sx = (u32)(((int64_t)xx - x) * image->width / w);
            const u8 *p = image->pixels + (size_t)sy * image->stride + sx * 4u;
            if (p[3] >= image->alpha_threshold)
                Pixel(xx, yy, (p[0] >> 3) | (p[1] >> 3) << 5 | (p[2] >> 3) << 10);
        }
    }
    return 0;
}
static const struct RhhUiCanvas sCanvasApi = {sizeof(sCanvasApi), W, H, Rect, Line, Polygon, Text, ImageRgb, ImageRgba};
int RhhUiOverlayRender(u16 *pixels)
{
    if (!RhhUiOverlayActive() || !pixels || !OverlayReady()) return 0;
    memset(pixels, 0, W * H * sizeof(*pixels)); sCanvas = pixels;
    RhhModsSetDrawing(1); sAccessibleDrawing = 2; sOverlay.draw(&sCanvasApi, sOverlay.user); sAccessibleDrawing = 0; RhhModsSetDrawing(0);
    sCanvas = NULL; RhhUiAccessibilityFlush(); return 1;
}
void RhhUiOverlayComposite(u16 *out, const u16 *scene, const u16 *overlay)
{
    if (!out || !scene || !overlay) return;
    for (u32 i = 0; i < W * H; ++i) out[i] = (overlay[i] & 0x8000) ? overlay[i] : scene[i];
}
void RhhUiRender(u16 *pixels)
{
    if (!Active() || !pixels) return;
    if (!sHaveBase) { memcpy(sBase, pixels, sizeof(sBase)); sHaveBase = 1; }
    memcpy(pixels, sBase, sizeof(sBase)); sCanvas = pixels;
    RhhModsSetDrawing(1); sAccessibleDrawing = 1; sPanel.draw(&sCanvasApi, sPanel.user); sAccessibleDrawing = 0; RhhModsSetDrawing(0);
    sCanvas = NULL; RhhUiAccessibilityFlush();
}
void RhhUiRestore(u16 *pixels)
{
    if (sHaveBase && pixels) memcpy(pixels, sBase, sizeof(sBase));
    sHaveBase = 0;
}
