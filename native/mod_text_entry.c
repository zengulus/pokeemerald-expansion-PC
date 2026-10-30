/* Panel-owned native keyboard input for short, printable names. */
#include <string.h>
#include "global.h"
#include "main.h"
#include "mod_api.h"
#include "mod_host.h"
#include "mod_ui.h"
#include "native.h"
#include "native_text_input.h"

static struct RhhTextEntrySnapshot sEntry;
static void Clear(void)
{
    if (sEntry.owner) { NativeTextEnd(); RhhUiTextReleaseBarrier(); }
    memset(&sEntry, 0, sizeof(sEntry));
}
void RhhTextEntryCancelOwner(u64 owner) { if (owner && owner == sEntry.owner) Clear(); }
void RhhTextEntryAdvance(void)
{
    if (!sEntry.owner || sEntry.state != RHH_TEXT_ENTRY_EDITING) return;
    int value;
    while ((value = NativeTextPop()) >= 0)
    {
        size_t length = strlen(sEntry.text);
        if (value == NATIVE_TEXT_ACCEPT) { sEntry.state = RHH_TEXT_ENTRY_ACCEPTED; break; }
        if (value == NATIVE_TEXT_CANCEL) { sEntry.state = RHH_TEXT_ENTRY_CANCELLED; break; }
        if (value == NATIVE_TEXT_BACKSPACE) { if (length) sEntry.text[length - 1] = 0; }
        else if (value >= 32 && value <= 126 && length < sEntry.max_length)
        { sEntry.text[length] = value; sEntry.text[length + 1] = 0; }
    }
}
static int Begin(RhhContentKey owner, const char *initial, u32 maxLength)
{
    if (!owner || !RhhModsRegisteredKey(owner) || !initial || !maxLength || maxLength >= RHH_TEXT_ENTRY_CAPACITY) return -1;
    size_t length = strlen(initial); if (length > maxLength) return -1;
    for (size_t i = 0; i < length; ++i) if ((u8)initial[i] < 32 || (u8)initial[i] > 126) return -1;
    if (!RhhModsCommandsAllowed() || !RhhModsHasActiveRun() || !RhhUiPanelOwnedBy(owner)
        || gMain.inBattle || NativeSaveTransactionActive() || sEntry.owner || NativeTextBeginAscii()) return -1;
    sEntry = (struct RhhTextEntrySnapshot){.struct_size = sizeof(sEntry), .state = RHH_TEXT_ENTRY_EDITING,
        .max_length = maxLength, .owner = owner};
    memcpy(sEntry.text, initial, length + 1); return 0;
}
static int Poll(RhhContentKey owner, struct RhhTextEntrySnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out) || !owner || owner != sEntry.owner) return -1;
    *out = sEntry; return 0;
}
static int Take(RhhContentKey owner, struct RhhTextEntrySnapshot *out)
{
    if (!RhhModsCommandsAllowed() || !out || out->struct_size < sizeof(*out) || !owner
        || owner != sEntry.owner || sEntry.state == RHH_TEXT_ENTRY_EDITING) return -1;
    *out = sEntry; Clear(); return 0;
}
static const struct RhhTextEntryService sService = {{sizeof(sService), RHH_SERVICE_VERSION(1, 0)}, Begin, Poll, Take};
int RhhTextEntryInstallService(void) { return RhhModsAddService("rhh:service/text_entry", &sService.header); }
