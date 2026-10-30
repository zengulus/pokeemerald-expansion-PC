#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "pokemon_storage_system.h"
#include "native.h"
#include "owned_identity_ledger.h"

/* Stable logical layout, independent of C padding or native pointer width. */
enum { PARTY_BASE = 0, BOX_BASE = PARTY_BASE + PARTY_SIZE,
       DAYCARE_BASE = BOX_BASE + TOTAL_BOXES_COUNT * IN_BOX_COUNT,
       FUSION_BASE = DAYCARE_BASE + 3, CURSOR_BASE = FUSION_BASE + MAX_FUSION_STORAGE,
       GROUP_BASE = CURSOR_BASE + 1, TRADE_BASE = GROUP_BASE + IN_BOX_COUNT,
       LEDGER_SLOTS = TRADE_BASE + 1, HEADER_SIZE = 32, ID_SIZE = 16 };
static struct NativeOwnedId sIds[LEDGER_SLOTS];
static uint64_t sRun, sRevision;
static int (*sSaveGuard)(uint64_t run);
static void (*sResetGuard)(void);
void NativeOwnedLedgerSetSaveGuard(int (*guard)(uint64_t run)) { sSaveGuard = guard; }
void NativeOwnedLedgerSetResetGuard(void (*guard)(void)) { sResetGuard = guard; }
#ifdef RHH_OWNED_LEDGER_TEST
static int (*sEntropyOverride)(void *, uint32_t);
void NativeOwnedLedgerSetEntropyForTest(int (*source)(void *, uint32_t)) { sEntropyOverride = source; }
#endif

static int SameId(struct NativeOwnedId a, struct NativeOwnedId b) { return a.high == b.high && a.low == b.low; }
static int EmptyId(struct NativeOwnedId id) { return !id.high && !id.low; }
static uint32_t Read32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t Read64(const uint8_t *p) { return Read32(p) | (uint64_t)Read32(p + 4) << 32; }
static void Write32(uint8_t *p, uint32_t value) { for (unsigned i = 0; i < 4; ++i) p[i] = value >> (i * 8); }
static void Write64(uint8_t *p, uint64_t value) { Write32(p, value); Write32(p + 4, value >> 32); }

static int SlotIndex(struct NativeOwnedLedgerSlot slot, uint32_t *index)
{
    if (!index) return -1;
    switch (slot.location)
    {
    case NATIVE_OWNED_PARTY:
        if (slot.box || slot.slot >= PARTY_SIZE) return -1;
        *index = PARTY_BASE + slot.slot; return 0;
    case NATIVE_OWNED_BOX:
        if (slot.box >= TOTAL_BOXES_COUNT || slot.slot >= IN_BOX_COUNT) return -1;
        *index = BOX_BASE + slot.box * IN_BOX_COUNT + slot.slot; return 0;
    case NATIVE_OWNED_DAYCARE:
        if (slot.box > 1 || (slot.box == 0 && slot.slot >= 2) || (slot.box == 1 && slot.slot)) return -1;
        *index = DAYCARE_BASE + (slot.box ? 2 : slot.slot); return 0;
    case NATIVE_OWNED_FUSION:
        if (slot.box || slot.slot >= MAX_FUSION_STORAGE) return -1;
        *index = FUSION_BASE + slot.slot; return 0;
    case NATIVE_OWNED_CURSOR:
        if (slot.box || slot.slot) return -1;
        *index = CURSOR_BASE; return 0;
    case NATIVE_OWNED_GROUP:
        if (slot.box || slot.slot >= IN_BOX_COUNT) return -1;
        *index = GROUP_BASE + slot.slot; return 0;
    case NATIVE_OWNED_TRADE:
        if (slot.box || slot.slot) return -1;
        *index = TRADE_BASE; return 0;
    default: return -1;
    }
}
static int Unique(const struct NativeOwnedId *ids)
{
    for (uint32_t i = 0; i < LEDGER_SLOTS; ++i)
        if (!EmptyId(ids[i]))
            for (uint32_t j = 0; j < i; ++j)
                if (SameId(ids[i], ids[j])) return 0;
    return 1;
}
static void LedgerReset(void)
{
    if (sResetGuard) { void (*guard)(void) = sResetGuard; sResetGuard = NULL; guard(); }
    memset(sIds, 0, sizeof(sIds)); sRun = sRevision = 0;
}
int NativeOwnedLedgerStart(uint64_t run)
{
    if (!run || NativeSaveTransactionActive()) return NATIVE_OWNED_LEDGER_INVALID;
    if (sRun != run) { LedgerReset(); sRun = run; }
    return NATIVE_OWNED_LEDGER_OK;
}
int NativeOwnedLedgerRevision(uint64_t run, uint64_t *revision)
{
    if (!run || run != sRun || !revision) return NATIVE_OWNED_LEDGER_INVALID;
    *revision = sRevision; return NATIVE_OWNED_LEDGER_OK;
}
int NativeOwnedLedgerRead(uint64_t run, struct NativeOwnedLedgerSlot slot, struct NativeOwnedId *out)
{
    uint32_t index;
    if (!run || run != sRun || !out || SlotIndex(slot, &index)) return NATIVE_OWNED_LEDGER_INVALID;
    *out = sIds[index]; return NATIVE_OWNED_LEDGER_OK;
}
int NativeOwnedLedgerMatches(uint64_t run, uint64_t revision, const struct NativeOwnedId *ids, uint32_t count)
{
    if (!run || run != sRun || revision != sRevision || count > LEDGER_SLOTS || (count && !ids)) return -1;
    uint32_t occupied = 0;
    for (uint32_t i = 0; i < LEDGER_SLOTS; ++i)
    {
        if (EmptyId(sIds[i])) continue;
        ++occupied;
        uint32_t j;
        for (j = 0; j < count; ++j) if (SameId(ids[j], sIds[i])) break;
        if (j == count) return -1;
    }
    if (occupied != count) return -1;
    for (uint32_t i = 0; i < count; ++i)
        for (uint32_t j = 0; j < i; ++j) if (SameId(ids[i], ids[j])) return -1;
    return 0;
}
int NativeOwnedLedgerApply(uint64_t run, uint64_t revision, const struct NativeOwnedLedgerEdit *edits, uint32_t count)
{
    if (!run || run != sRun || !edits || !count || count > LEDGER_SLOTS || NativeSaveTransactionActive())
        return NATIVE_OWNED_LEDGER_INVALID;
    if (revision != sRevision) return NATIVE_OWNED_LEDGER_STALE;
    if (sRevision == UINT64_MAX) return NATIVE_OWNED_LEDGER_INVALID;
    struct NativeOwnedId next[LEDGER_SLOTS]; uint8_t seen[LEDGER_SLOTS] = {0};
    memcpy(next, sIds, sizeof(next));
    int changed = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t index;
        if (SlotIndex(edits[i].slot, &index) || seen[index]) return NATIVE_OWNED_LEDGER_INVALID;
        seen[index] = 1;
        if (!SameId(sIds[index], edits[i].expected)) return NATIVE_OWNED_LEDGER_STALE;
        if (!SameId(sIds[index], edits[i].value)) changed = 1;
        next[index] = edits[i].value;
    }
    if (!changed) return NATIVE_OWNED_LEDGER_INVALID;
    if (!Unique(next)) return NATIVE_OWNED_LEDGER_CONFLICT;
    memcpy(sIds, next, sizeof(sIds)); ++sRevision; return NATIVE_OWNED_LEDGER_OK;
}
int NativeOwnedLedgerGenerate(uint64_t run, struct NativeOwnedId *out)
{
    if (!out || !run || run != sRun || NativeSaveTransactionActive()
        || sRevision == UINT64_MAX) return NATIVE_OWNED_LEDGER_INVALID;
    for (unsigned attempt = 0; attempt < 8; ++attempt)
    {
        struct NativeOwnedId id;
#ifdef RHH_OWNED_LEDGER_TEST
        if ((sEntropyOverride ? sEntropyOverride : NativeOwnedEntropy)(&id, sizeof(id))) return NATIVE_OWNED_LEDGER_ENTROPY;
#else
        if (NativeOwnedEntropy(&id, sizeof(id))) return NATIVE_OWNED_LEDGER_ENTROPY;
#endif
        if (EmptyId(id)) continue;
        uint32_t i;
        for (i = 0; i < LEDGER_SLOTS; ++i) if (SameId(sIds[i], id)) break;
        if (i != LEDGER_SLOTS) continue;
        *out = id; return NATIVE_OWNED_LEDGER_OK;
    }
    return NATIVE_OWNED_LEDGER_ENTROPY;
}
int NativeOwnedLedgerMintReplace(uint64_t run, uint64_t revision, struct NativeOwnedLedgerSlot slot,
                                struct NativeOwnedId expected, struct NativeOwnedId *out)
{
    uint32_t index; struct NativeOwnedId id;
    if (!out || !run || run != sRun || SlotIndex(slot, &index) || NativeSaveTransactionActive()
        || sRevision == UINT64_MAX) return NATIVE_OWNED_LEDGER_INVALID;
    if (revision != sRevision || !SameId(sIds[index], expected)) return NATIVE_OWNED_LEDGER_STALE;
    int result = NativeOwnedLedgerGenerate(run, &id);
    if (result) return result;
    struct NativeOwnedLedgerEdit edit = {.slot = slot, .expected = expected, .value = id};
    result = NativeOwnedLedgerApply(run, revision, &edit, 1);
    if (!result) *out = id;
    return result;
}
int NativeOwnedLedgerMint(uint64_t run, uint64_t revision, struct NativeOwnedLedgerSlot slot, struct NativeOwnedId *out)
{
    uint32_t index;
    if (!out || !run || run != sRun || SlotIndex(slot, &index) || NativeSaveTransactionActive()
        || sRevision == UINT64_MAX) return NATIVE_OWNED_LEDGER_INVALID;
    if (revision != sRevision) return NATIVE_OWNED_LEDGER_STALE;
    if (!EmptyId(sIds[index])) return NATIVE_OWNED_LEDGER_CONFLICT;
    return NativeOwnedLedgerMintReplace(run, revision, slot, (struct NativeOwnedId){0, 0}, out);
}
int NativeOwnedLedgerMintBatch(uint64_t run, uint64_t revision, const struct NativeOwnedLedgerSlot *slots,
                              uint32_t count, struct NativeOwnedId *out)
{
    if (!run || run != sRun || !slots || !count || count > LEDGER_SLOTS || NativeSaveTransactionActive()
        || sRevision == UINT64_MAX) return NATIVE_OWNED_LEDGER_INVALID;
    if (revision != sRevision) return NATIVE_OWNED_LEDGER_STALE;
    struct NativeOwnedLedgerEdit *edits = calloc(count, sizeof(*edits));
    if (!edits) return NATIVE_OWNED_LEDGER_INVALID;
    uint8_t seen[LEDGER_SLOTS] = {0}; int result = NATIVE_OWNED_LEDGER_INVALID;
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t index;
        if (SlotIndex(slots[i], &index) || seen[index]) goto done;
        if (!EmptyId(sIds[index])) { result = NATIVE_OWNED_LEDGER_CONFLICT; goto done; }
        seen[index] = 1; edits[i].slot = slots[i];
        unsigned attempt;
        for (attempt = 0; attempt < 8; ++attempt)
        {
#ifdef RHH_OWNED_LEDGER_TEST
            if ((sEntropyOverride ? sEntropyOverride : NativeOwnedEntropy)(&edits[i].value, sizeof(edits[i].value)))
#else
            if (NativeOwnedEntropy(&edits[i].value, sizeof(edits[i].value)))
#endif
            { result = NATIVE_OWNED_LEDGER_ENTROPY; goto done; }
            if (EmptyId(edits[i].value)) continue;
            uint32_t j;
            for (j = 0; j < LEDGER_SLOTS; ++j) if (SameId(sIds[j], edits[i].value)) break;
            if (j != LEDGER_SLOTS) continue;
            for (j = 0; j < i; ++j) if (SameId(edits[j].value, edits[i].value)) break;
            if (j == i) break;
        }
        if (attempt == 8) { result = NATIVE_OWNED_LEDGER_ENTROPY; goto done; }
    }
    result = NativeOwnedLedgerApply(run, revision, edits, count);
    if (!result && out) for (uint32_t i = 0; i < count; ++i) out[i] = edits[i].value;
done:
    free(edits); return result;
}
int NativeOwnedLedgerInvalidate(uint64_t run)
{
    if (!run || run != sRun)
        return NATIVE_OWNED_LEDGER_INVALID;
    if (NativeSaveTransactionActive()) NativeSaveInvalidateTransaction();
    if (sRevision == UINT64_MAX) { LedgerReset(); return NATIVE_OWNED_LEDGER_OK; }
    memset(sIds, 0, sizeof(sIds)); ++sRevision; return NATIVE_OWNED_LEDGER_OK;
}

static int LedgerValidate(const uint8_t *data, uint32_t size)
{
    if (!size) return data ? -1 : 0;
    if (!data || size != HEADER_SIZE + LEDGER_SLOTS * ID_SIZE || memcmp(data, "RHIDLG01", 8)
        || Read32(data + 8) != size || Read32(data + 12) != LEDGER_SLOTS
        || !Read64(data + 16) || !Read64(data + 24)) return -1;
    struct NativeOwnedId ids[LEDGER_SLOTS];
    for (uint32_t i = 0; i < LEDGER_SLOTS; ++i)
        ids[i] = (struct NativeOwnedId){Read64(data + HEADER_SIZE + i * ID_SIZE),
                                        Read64(data + HEADER_SIZE + i * ID_SIZE + 8)};
    return Unique(ids) ? 0 : -1;
}
static int LedgerExport(uint8_t **out, uint32_t *size)
{
    if (!out || !size) return -1;
    *out = NULL; *size = 0;
    if (NativeSaveArchiveSaveExportActive())
    {
        for (uint32_t i = CURSOR_BASE; i < LEDGER_SLOTS; ++i)
            if (!EmptyId(sIds[i])) return -1;
        if (sSaveGuard && sSaveGuard(sRun)) return -1;
    }
    if (!sRun || !sRevision) return 0;
    uint32_t length = HEADER_SIZE + LEDGER_SLOTS * ID_SIZE;
    uint8_t *data = calloc(length, 1); if (!data) return -1;
    memcpy(data, "RHIDLG01", 8); Write32(data + 8, length); Write32(data + 12, LEDGER_SLOTS);
    Write64(data + 16, sRun); Write64(data + 24, sRevision);
    for (uint32_t i = 0; i < LEDGER_SLOTS; ++i)
    { Write64(data + HEADER_SIZE + i * ID_SIZE, sIds[i].high); Write64(data + HEADER_SIZE + i * ID_SIZE + 8, sIds[i].low); }
    *out = data; *size = length; return 0;
}
static int LedgerImport(const uint8_t *data, uint32_t size)
{
    if (LedgerValidate(data, size)) return -1;
    LedgerReset(); if (!size) return 0;
    sRun = Read64(data + 16); sRevision = Read64(data + 24);
    for (uint32_t i = 0; i < LEDGER_SLOTS; ++i)
        sIds[i] = (struct NativeOwnedId){Read64(data + HEADER_SIZE + i * ID_SIZE),
                                        Read64(data + HEADER_SIZE + i * ID_SIZE + 8)};
    return 0;
}
static const struct NativeSaveArchiveCodec sCodec = {
    LedgerExport, LedgerValidate, LedgerImport, LedgerReset, "rhh:owned_identity_ledger", NULL
};
const struct NativeSaveArchiveCodec *NativeOwnedLedgerCodec(void) { return &sCodec; }
int NativeOwnedLedgerInstall(void) { return NativeSaveRegisterArchive(&sCodec); }
