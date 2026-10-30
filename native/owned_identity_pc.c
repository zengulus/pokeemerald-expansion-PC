#include <stdlib.h>
#include "global.h"
#include "pokemon_storage_system.h"
#include "daycare.h"
#include "trade.h"
#include "main.h"
#include "mods.h"
#include "mod_host.h"
#include "native.h"
#include "mod_api.h"
#include "owned_identity_ledger.h"

static uint64_t sRun;
static uint32_t sGroupBox;
static int sGroupBoxValid;
static uint64_t sHatchRun;
static int sHatchRekeyApplied, sHatchNameApplied;
static uint64_t sSaveRevision;
static uint32_t sHatchSlot;
static struct NativeOwnedId sHatchId;
static int sPcTradePending, sLinkTradeState, sIngamePartyPending;
static int sPartyLoadConsumed;
static uint32_t sPcTradeBox, sPcTradeSlot, sLinkTradeSlot;
static uint32_t sIngamePartySlot;
static int sSkyState;
static int sUnionState;
static int sRentalState;
static uint32_t sRentalSavedCount;
static int sRecordedState;
static const struct Pokemon *sRecordedBackup;
static struct Pokemon sRecordedOriginal[PARTY_SIZE];
static uint32_t sRecordedCount;
static uint8_t sUnionSlots[PARTY_SIZE];
static uint32_t sUnionCount, sUnionSavedCount;
static struct Pokemon sProjectionSaved[PARTY_SIZE];
static struct Pokemon sUnionExpected[PARTY_SIZE];
static struct Pokemon sFrontierResumeParty[PARTY_SIZE];
static uint8_t sFrontierResumeSlots[PARTY_SIZE];
static uint32_t sFrontierResumeCount;
static uint64_t sFrontierResumeRun, sFrontierResumeRevision;
static uint32_t sSkyMask, sSkyCount;
static struct NativeOwnedId sSkyIds[PARTY_SIZE];
static struct Pokemon sSkyOriginal[PARTY_SIZE];
static uint32_t sSkySavedCount;
static struct NativeOwnedId sLinkOutgoing, sLinkIncoming, sIngamePartyId;
static const struct NativeOwnedId sEmpty = {0, 0};
/* These are change detectors, not identities: different Pokémon can share
 * these values, and a tracked hatch can legitimately change OT details. */
enum { WITNESS_CAPACITY = PARTY_SIZE + TOTAL_BOXES_COUNT * IN_BOX_COUNT + 3
                          + MAX_FUSION_STORAGE + IN_BOX_COUNT + 2 };
struct OwnedWitness { struct NativeOwnedId id; uint32_t personality, otId; uint8_t otName[PLAYER_NAME_LENGTH]; };
static struct OwnedWitness sWitnesses[WITNESS_CAPACITY];
static uint64_t sWitnessRun, sWitnessRevision;
static int sWitnessValid, sWitnessNamesAvailable;

static int Empty(struct NativeOwnedId id) { return !id.high && !id.low; }
static int Same(struct NativeOwnedId a, struct NativeOwnedId b) { return a.high == b.high && a.low == b.low; }
static int Read(struct NativeOwnedLedgerSlot slot, struct NativeOwnedId *id)
{ return NativeOwnedLedgerRead(sRun, slot, id); }
static struct BoxPokemon *OwnedMon(struct NativeOwnedLedgerSlot slot)
{
    switch (slot.location)
    {
    case NATIVE_OWNED_PARTY:
        return slot.slot < PARTY_SIZE ? &gParties[B_TRAINER_PLAYER][slot.slot].box : NULL;
    case NATIVE_OWNED_BOX:
        return gPokemonStoragePtr && slot.box < TOTAL_BOXES_COUNT && slot.slot < IN_BOX_COUNT
            ? &gPokemonStoragePtr->boxes[slot.box][slot.slot] : NULL;
    case NATIVE_OWNED_DAYCARE:
        if (!gSaveBlock1Ptr) return NULL;
        if (!slot.box && slot.slot < DAYCARE_MON_COUNT) return &gSaveBlock1Ptr->daycare.mons[slot.slot].mon;
#if IS_FRLG
        if (slot.box == 1 && !slot.slot) return &gSaveBlock1Ptr->route5DayCareMon.mon;
#endif
        return NULL;
    case NATIVE_OWNED_FUSION:
        return gPokemonStoragePtr && slot.slot < MAX_FUSION_STORAGE ? &gPokemonStoragePtr->fusions[slot.slot].box : NULL;
    case NATIVE_OWNED_TRADE:
        return &gParties[B_TRAINER_OPPONENT_A][TRADEMON_FROM_PC].box;
    default: return NULL;
    }
}
static void ForgetWitness(struct NativeOwnedId id)
{
    if (Empty(id)) return;
    for (uint32_t i = 0; i < WITNESS_CAPACITY; ++i)
        if (Same(sWitnesses[i].id, id)) { sWitnesses[i].id = sEmpty; return; }
}
static int RememberWitness(struct NativeOwnedId id, const struct BoxPokemon *mon)
{
    if (Empty(id) || !mon || !mon->hasSpecies) return -1;
    uint32_t empty = WITNESS_CAPACITY;
    for (uint32_t i = 0; i < WITNESS_CAPACITY; ++i)
    {
        if (Same(sWitnesses[i].id, id))
        { sWitnesses[i].personality = mon->personality; sWitnesses[i].otId = mon->otId;
          memcpy(sWitnesses[i].otName, mon->otName, PLAYER_NAME_LENGTH); return 0; }
        if (empty == WITNESS_CAPACITY && Empty(sWitnesses[i].id)) empty = i;
    }
    if (empty == WITNESS_CAPACITY) return -1;
    sWitnesses[empty].id = id; sWitnesses[empty].personality = mon->personality;
    sWitnesses[empty].otId = mon->otId; memcpy(sWitnesses[empty].otName, mon->otName, PLAYER_NAME_LENGTH); return 0;
}
static int WitnessMatches(struct NativeOwnedId id, const struct BoxPokemon *mon)
{
    if (!mon || !mon->hasSpecies || Empty(id)) return 0;
    for (uint32_t i = 0; i < WITNESS_CAPACITY; ++i)
        if (Same(sWitnesses[i].id, id))
            return sWitnesses[i].personality == mon->personality && sWitnesses[i].otId == mon->otId
                && (!sWitnessNamesAvailable || !memcmp(sWitnesses[i].otName, mon->otName, PLAYER_NAME_LENGTH));
    return 0;
}
static int WitnessPersonalityMatches(struct NativeOwnedId id, const struct BoxPokemon *mon)
{
    if (!mon || !mon->hasSpecies || Empty(id)) return 0;
    for (uint32_t i = 0; i < WITNESS_CAPACITY; ++i)
        if (Same(sWitnesses[i].id, id)) return sWitnesses[i].personality == mon->personality;
    return 0;
}
static int WitnessOtMatches(struct NativeOwnedId id, const struct BoxPokemon *mon)
{
    for (uint32_t i = 0; i < WITNESS_CAPACITY; ++i)
        if (Same(sWitnesses[i].id, id)) return sWitnesses[i].otId == mon->otId;
    return 0;
}
static int WitnessNameMatches(struct NativeOwnedId id, const struct BoxPokemon *mon)
{
    for (uint32_t i = 0; i < WITNESS_CAPACITY; ++i)
        if (Same(sWitnesses[i].id, id)) return !memcmp(sWitnesses[i].otName, mon->otName, PLAYER_NAME_LENGTH);
    return 0;
}
static int WitnessMatchesSnapshot(struct NativeOwnedId id, const struct RhhOwnedMonSnapshot *mon)
{
    if (!mon || Empty(id)) return 0;
    for (uint32_t i = 0; i < WITNESS_CAPACITY; ++i)
        if (Same(sWitnesses[i].id, id))
            return (((uint64_t)sWitnesses[i].otId << 32) | sWitnesses[i].personality) == mon->identity;
    return 0;
}
static int GroupEmpty(void)
{
    for (uint32_t i = 0; i < IN_BOX_COUNT; ++i)
    {
        struct NativeOwnedId id;
        if (Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_GROUP, 0, i}, &id) || !Empty(id)) return 0;
    }
    return 1;
}
static int TemporaryEmpty(void)
{
    struct NativeOwnedId id;
    return !Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_CURSOR, 0, 0}, &id) && Empty(id) && GroupEmpty()
        && !Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_TRADE, 0, 0}, &id) && Empty(id);
}
static int MatchesOccupancy(struct NativeOwnedLedgerSlot slot, int occupied, int witnesses)
{
    struct NativeOwnedId id;
    return !Read(slot, &id) && (!!occupied == !Empty(id))
        && (!occupied || !witnesses || WitnessMatches(id, OwnedMon(slot)));
}
static int NonPartyOccupancyMatches(int witnesses)
{
    if (!gSaveBlock1Ptr || !gPokemonStoragePtr) return 0;
    for (uint32_t box = 0; box < TOTAL_BOXES_COUNT; ++box)
        for (uint32_t slot = 0; slot < IN_BOX_COUNT; ++slot)
            if (!MatchesOccupancy((struct NativeOwnedLedgerSlot){NATIVE_OWNED_BOX, box, slot},
                                  gPokemonStoragePtr->boxes[box][slot].hasSpecies, witnesses)) return 0;
    for (uint32_t i = 0; i < DAYCARE_MON_COUNT; ++i)
        if (!MatchesOccupancy((struct NativeOwnedLedgerSlot){NATIVE_OWNED_DAYCARE, 0, i},
                              gSaveBlock1Ptr->daycare.mons[i].mon.hasSpecies, witnesses)) return 0;
#if IS_FRLG
    if (!MatchesOccupancy((struct NativeOwnedLedgerSlot){NATIVE_OWNED_DAYCARE, 1, 0},
                          gSaveBlock1Ptr->route5DayCareMon.mon.hasSpecies, witnesses)) return 0;
#else
    if (!MatchesOccupancy((struct NativeOwnedLedgerSlot){NATIVE_OWNED_DAYCARE, 1, 0}, 0, witnesses)) return 0;
#endif
    for (uint32_t i = 0; i < MAX_FUSION_STORAGE; ++i)
        if (!MatchesOccupancy((struct NativeOwnedLedgerSlot){NATIVE_OWNED_FUSION, 0, i},
                              gPokemonStoragePtr->fusions[i].box.hasSpecies, witnesses)) return 0;
    return 1;
}
static int OccupancyMatches(int witnesses)
{
    if (!NonPartyOccupancyMatches(witnesses)) return 0;
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
        if (!MatchesOccupancy((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i},
                              gParties[B_TRAINER_PLAYER][i].box.hasSpecies, witnesses)) return 0;
    return 1;
}
static int UnionSavedPartyMatches(void)
{
    if (!gSaveBlock1Ptr) return 0;
    if ((sUnionState || sRentalState)
        && memcmp(gSaveBlock1Ptr->playerParty, sProjectionSaved, sizeof(sProjectionSaved))) return 0;
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
    {
        struct NativeOwnedId id; struct BoxPokemon *mon = &GetSavedPlayerPartyMon(i)->box;
        if (Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, &id)
            || (!!mon->hasSpecies != !Empty(id)) || (mon->hasSpecies && !WitnessMatches(id, mon))) return 0;
    }
    return 1;
}
static int RememberSlot(struct NativeOwnedLedgerSlot slot)
{
    struct NativeOwnedId id; struct BoxPokemon *mon = OwnedMon(slot);
    return Read(slot, &id) || (!Empty(id) && RememberWitness(id, mon));
}
static int RememberInitial(void)
{
    memset(sWitnesses, 0, sizeof(sWitnesses));
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
        if (RememberSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i})) return -1;
    for (uint32_t box = 0; box < TOTAL_BOXES_COUNT; ++box)
        for (uint32_t slot = 0; slot < IN_BOX_COUNT; ++slot)
            if (RememberSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_BOX, box, slot})) return -1;
    for (uint32_t i = 0; i < DAYCARE_MON_COUNT; ++i)
        if (RememberSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_DAYCARE, 0, i})) return -1;
#if IS_FRLG
    if (RememberSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_DAYCARE, 1, 0})) return -1;
#endif
    for (uint32_t i = 0; i < MAX_FUSION_STORAGE; ++i)
        if (RememberSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_FUSION, 0, i})) return -1;
    return 0;
}
/* This archive accompanies the ledger, but is not itself an individual ID.
 * Its revision and complete ID set must match the same saved ledger. */
enum { WITNESS_HEADER_SIZE = 32, WITNESS_RECORD_V1_SIZE = 24, WITNESS_RECORD_V2_SIZE = 24 + PLAYER_NAME_LENGTH + 1 };
static uint32_t WitnessRead32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t WitnessRead64(const uint8_t *p) { return WitnessRead32(p) | (uint64_t)WitnessRead32(p + 4) << 32; }
static void WitnessWrite32(uint8_t *p, uint32_t value) { for (unsigned i = 0; i < 4; ++i) p[i] = value >> (8 * i); }
static void WitnessWrite64(uint8_t *p, uint64_t value) { WitnessWrite32(p, value); WitnessWrite32(p + 4, value >> 32); }
static void WitnessReset(void)
{
    memset(sWitnesses, 0, sizeof(sWitnesses)); sWitnessRun = sWitnessRevision = 0; sWitnessValid = sWitnessNamesAvailable = 0;
}
static int WitnessValidate(const uint8_t *data, uint32_t size)
{
    if (!size) return data ? -1 : 0;
    if (!data || size < WITNESS_HEADER_SIZE
        || (memcmp(data, "RHIWIT01", 8) && memcmp(data, "RHIWIT02", 8))
        || WitnessRead32(data + 8) != size || !WitnessRead64(data + 16) || !WitnessRead64(data + 24)) return -1;
    uint32_t count = WitnessRead32(data + 12);
    uint32_t stride = data[7] == '2' ? WITNESS_RECORD_V2_SIZE : WITNESS_RECORD_V1_SIZE;
    if (count > WITNESS_CAPACITY || size != WITNESS_HEADER_SIZE + count * stride) return -1;
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint8_t *record = data + WITNESS_HEADER_SIZE + i * stride;
        struct NativeOwnedId id = {WitnessRead64(record), WitnessRead64(record + 8)};
        if (Empty(id) || (stride == WITNESS_RECORD_V2_SIZE && record[stride - 1])) return -1;
        for (uint32_t j = 0; j < i; ++j)
        {
            const uint8_t *prior = data + WITNESS_HEADER_SIZE + j * stride;
            if (id.high == WitnessRead64(prior) && id.low == WitnessRead64(prior + 8)) return -1;
        }
    }
    return 0;
}
static int WitnessExport(uint8_t **out, uint32_t *size)
{
    if (!out || !size) return -1;
    *out = NULL; *size = 0;
    if (!sWitnessValid) return 0;
    uint64_t revision = sWitnessRevision;
    if (sRun && (sRun != sWitnessRun || NativeOwnedLedgerRevision(sRun, &revision))) return -1;
    struct NativeOwnedId ids[WITNESS_CAPACITY]; uint32_t count = 0;
    for (uint32_t i = 0; i < WITNESS_CAPACITY; ++i)
        if (!Empty(sWitnesses[i].id)) ids[count++] = sWitnesses[i].id;
    if (NativeOwnedLedgerMatches(sWitnessRun, revision, ids, count))
        return sRun ? -1 : 0; /* An inactive, superseded witness is not saved. */
    if (!revision) return 0; /* An empty, unmodified ledger has no archive yet. */
    uint32_t stride = sWitnessNamesAvailable ? WITNESS_RECORD_V2_SIZE : WITNESS_RECORD_V1_SIZE;
    uint32_t length = WITNESS_HEADER_SIZE + count * stride;
    uint8_t *data = calloc(length, 1); if (!data) return -1;
    memcpy(data, sWitnessNamesAvailable ? "RHIWIT02" : "RHIWIT01", 8); WitnessWrite32(data + 8, length); WitnessWrite32(data + 12, count);
    WitnessWrite64(data + 16, sWitnessRun); WitnessWrite64(data + 24, revision);
    uint32_t at = WITNESS_HEADER_SIZE;
    for (uint32_t i = 0; i < WITNESS_CAPACITY; ++i)
        if (!Empty(sWitnesses[i].id))
        {
            WitnessWrite64(data + at, sWitnesses[i].id.high); WitnessWrite64(data + at + 8, sWitnesses[i].id.low);
            WitnessWrite32(data + at + 16, sWitnesses[i].personality); WitnessWrite32(data + at + 20, sWitnesses[i].otId);
            if (sWitnessNamesAvailable) memcpy(data + at + 24, sWitnesses[i].otName, PLAYER_NAME_LENGTH);
            at += stride;
        }
    *out = data; *size = length; return 0;
}
static int WitnessImport(const uint8_t *data, uint32_t size)
{
    if (WitnessValidate(data, size)) return -1;
    if (!size) { WitnessReset(); return 0; }
    uint64_t run = WitnessRead64(data + 16), revision = WitnessRead64(data + 24);
    uint32_t count = WitnessRead32(data + 12); struct NativeOwnedId ids[WITNESS_CAPACITY];
    uint32_t stride = data[7] == '2' ? WITNESS_RECORD_V2_SIZE : WITNESS_RECORD_V1_SIZE;
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint8_t *record = data + WITNESS_HEADER_SIZE + i * stride;
        ids[i] = (struct NativeOwnedId){WitnessRead64(record), WitnessRead64(record + 8)};
    }
    if (NativeOwnedLedgerMatches(run, revision, ids, count)) return -1;
    WitnessReset();
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint8_t *record = data + WITNESS_HEADER_SIZE + i * stride;
        sWitnesses[i].id = ids[i]; sWitnesses[i].personality = WitnessRead32(record + 16);
        sWitnesses[i].otId = WitnessRead32(record + 20);
        if (stride == WITNESS_RECORD_V2_SIZE) memcpy(sWitnesses[i].otName, record + 24, PLAYER_NAME_LENGTH);
    }
    sWitnessRun = run; sWitnessRevision = revision; sWitnessValid = 1;
    sWitnessNamesAvailable = stride == WITNESS_RECORD_V2_SIZE; return 0;
}
static const struct NativeSaveArchiveCodec sWitnessCodec = {
    WitnessExport, WitnessValidate, WitnessImport, WitnessReset, "rhh:owned_identity_witness", NULL
};
int NativeOwnedTrackerInstall(void) { return NativeSaveRegisterArchive(&sWitnessCodec); }
static void Invalidate(void)
{
    if (sRun) (void)NativeOwnedLedgerInvalidate(sRun);
    NativeOwnedLedgerSetSaveGuard(NULL);
    NativeOwnedLedgerSetResetGuard(NULL);
    NativeSaveSetCommitGuard(NULL);
    WitnessReset();
    sRun = sHatchRun = 0; sHatchRekeyApplied = sHatchNameApplied = 0;
    sGroupBoxValid = sPcTradePending = sLinkTradeState = sIngamePartyPending = sSkyState = sUnionState = sRentalState = sRecordedState = 0;
    sPartyLoadConsumed = 0;
    sRentalSavedCount = sRecordedCount = 0; sRecordedBackup = NULL; memset(sRecordedOriginal, 0, sizeof(sRecordedOriginal));
    memset(sProjectionSaved, 0, sizeof(sProjectionSaved));
    memset(sUnionExpected, 0, sizeof(sUnionExpected)); memset(sFrontierResumeParty, 0, sizeof(sFrontierResumeParty));
    sFrontierResumeCount = 0; sFrontierResumeRun = sFrontierResumeRevision = 0;
    sSkySavedCount = 0; memset(sSkyOriginal, 0, sizeof(sSkyOriginal));
}
static int CheckSaveOccupancy(uint64_t run)
{
    if (!sRun || sRun != run || !RhhModsHasActiveRun() || RhhModsRunId() != run)
    { Invalidate(); return -1; }
    /* Battle views and an in-progress hatch are not stable save snapshots. */
    if (gMain.inBattle || sHatchRun || sSkyState || sUnionState || sRentalState || sRecordedState) return -1;
    if (!OccupancyMatches(1) || NativeOwnedLedgerRevision(run, &sSaveRevision)) { Invalidate(); return -1; }
    return 0;
}
static int CheckSaveCommit(void)
{
    uint64_t revision;
    if (!sRun || !RhhModsHasActiveRun() || RhhModsRunId() != sRun || gMain.inBattle || sHatchRun || sSkyState || sUnionState || sRentalState || sRecordedState
        || !TemporaryEmpty() || !OccupancyMatches(1) || NativeOwnedLedgerRevision(sRun, &revision)
        || revision != sSaveRevision) { Invalidate(); return -1; }
    return 0;
}
void NativeOwnedTrackerUnsupported(void) { if (sRun) Invalidate(); }
static int ExactArrayElement(const void *pointer, const void *base, size_t stride, size_t count)
{
    uintptr_t value = (uintptr_t)pointer, first = (uintptr_t)base;
    return value >= first && value - first < stride * count && (value - first) % stride == 0;
}
static int Overlaps(const void *pointer, size_t size, const void *base, size_t length)
{
    uintptr_t value = (uintptr_t)pointer, first = (uintptr_t)base;
    return size && length && (value >= first ? value - first < length : first - value < size);
}
void NativeOwnedTrackUnreportedCopy(const void *dest, const void *src, size_t size)
{
    if (!sRun || !dest || dest == src || !size) return;
    if (sRecordedState && Overlaps(dest, size, sRecordedBackup, sizeof(sRecordedOriginal))) { Invalidate(); return; }
    if ((sRentalState != 1 && !sRecordedState && Overlaps(dest, size, &gParties[B_TRAINER_PLAYER][0], sizeof(gParties[B_TRAINER_PLAYER])))
        || (gPokemonStoragePtr
            && (Overlaps(dest, size, &gPokemonStoragePtr->boxes, sizeof(gPokemonStoragePtr->boxes))
                || Overlaps(dest, size, &gPokemonStoragePtr->fusions, sizeof(gPokemonStoragePtr->fusions)))))
    { Invalidate(); return; }
    if (gSaveBlock1Ptr)
    {
        if ((sSkyState || sUnionState || sRentalState || sRecordedState)
            && Overlaps(dest, size, &gSaveBlock1Ptr->playerParty, sizeof(gSaveBlock1Ptr->playerParty)))
        { Invalidate(); return; }
        for (uint32_t i = 0; i < DAYCARE_MON_COUNT; ++i)
            if (Overlaps(dest, size, &gSaveBlock1Ptr->daycare.mons[i].mon, sizeof(struct BoxPokemon)))
            { Invalidate(); return; }
#if IS_FRLG
        if (Overlaps(dest, size, &gSaveBlock1Ptr->route5DayCareMon.mon, sizeof(struct BoxPokemon))) Invalidate();
#endif
    }
}
void NativeOwnedTrackLocalEcho(void *dest, const void *src, size_t size)
{
    if (dest == src || !size) return;
    if (sRun && size && memcmp(dest, src, size)) NativeOwnedTrackUnreportedCopy(dest, src, size);
    memcpy(dest, src, size);
}
void NativeOwnedTrackUnreportedCreation(const struct BoxPokemon *mon)
{
    if (!sRun || !mon) return;
    if (sRecordedState && ExactArrayElement(mon, sRecordedBackup, sizeof(struct Pokemon), PARTY_SIZE)) { Invalidate(); return; }
    if ((sRentalState != 1 && !sRecordedState && ExactArrayElement(mon, &gParties[B_TRAINER_PLAYER][0], sizeof(struct Pokemon), PARTY_SIZE))
        || (gPokemonStoragePtr
            && (ExactArrayElement(mon, &gPokemonStoragePtr->boxes[0][0], sizeof(struct BoxPokemon),
                                  TOTAL_BOXES_COUNT * IN_BOX_COUNT)
                || ExactArrayElement(mon, &gPokemonStoragePtr->fusions[0], sizeof(struct Pokemon), MAX_FUSION_STORAGE))))
    { Invalidate(); return; }
    if (gSaveBlock1Ptr)
    {
        if ((sSkyState || sUnionState || sRentalState || sRecordedState)
            && ExactArrayElement(mon, &gSaveBlock1Ptr->playerParty[0], sizeof(struct Pokemon), PARTY_SIZE))
        { Invalidate(); return; }
        for (uint32_t i = 0; i < DAYCARE_MON_COUNT; ++i)
            if (mon == &gSaveBlock1Ptr->daycare.mons[i].mon) { Invalidate(); return; }
#if IS_FRLG
        if (mon == &gSaveBlock1Ptr->route5DayCareMon.mon) Invalidate();
#endif
    }
}
void NativeOwnedTrackUnreportedIdentityChange(const struct BoxPokemon *mon, int hatchRekey, uint32_t newOt)
{
    if (!sRun || !mon) return;
    if (hatchRekey && sHatchRun == sRun && !sHatchRekeyApplied && sHatchSlot < PARTY_SIZE
        && gSaveBlock2Ptr && newOt == READ_OTID_FROM_SAVE
        && mon == &gParties[B_TRAINER_PLAYER][sHatchSlot].box && WitnessMatches(sHatchId, mon))
    { sHatchRekeyApplied = 1; return; }
    NativeOwnedTrackUnreportedCreation(mon);
}
void NativeOwnedTrackUnreportedTrainerNameChange(const struct BoxPokemon *mon, const uint8_t *name)
{
    if (!sRun || !mon) return;
    if (name && sHatchRun == sRun && !sHatchNameApplied && sHatchSlot < PARTY_SIZE
        && gSaveBlock2Ptr && !memcmp(name, gSaveBlock2Ptr->playerName, PLAYER_NAME_LENGTH)
        && mon == &gParties[B_TRAINER_PLAYER][sHatchSlot].box && mon->otId == READ_OTID_FROM_SAVE
        && WitnessPersonalityMatches(sHatchId, mon) && WitnessNameMatches(sHatchId, mon)
        && (sHatchRekeyApplied || WitnessOtMatches(sHatchId, mon)))
    { sHatchNameApplied = 1; return; }
    NativeOwnedTrackUnreportedCreation(mon);
}
void NativeOwnedTrackUnreportedPartyClear(void)
{
    if (!sRun) return;
    if (sRentalState == 1 || sRecordedState == 1) return;
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
    {
        struct NativeOwnedId id;
        if (Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, &id)
            || !Empty(id) || gParties[B_TRAINER_PLAYER][i].box.hasSpecies)
        { Invalidate(); return; }
    }
}
void NativeOwnedTrackUnreportedBoxClear(uint32_t box, uint32_t slot)
{
    if (!sRun) return;
    struct NativeOwnedId id;
    if (!gPokemonStoragePtr || box >= TOTAL_BOXES_COUNT || slot >= IN_BOX_COUNT
        || Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_BOX, box, slot}, &id)
        || !Empty(id) || gPokemonStoragePtr->boxes[box][slot].hasSpecies) Invalidate();
}
static void GuardMonClear(struct NativeOwnedLedgerSlot slot, const struct BoxPokemon *mon)
{
    struct NativeOwnedId id;
    if (Read(slot, &id) || !Empty(id) || mon->hasSpecies) Invalidate();
}
void NativeOwnedTrackUnreportedMonClear(const struct BoxPokemon *mon)
{
    if (!sRun || !mon) return;
    if (sRecordedState && ExactArrayElement(mon, sRecordedBackup, sizeof(struct Pokemon), PARTY_SIZE)) { Invalidate(); return; }
    if (sRentalState != 1 && !sRecordedState && ExactArrayElement(mon, &gParties[B_TRAINER_PLAYER][0], sizeof(struct Pokemon), PARTY_SIZE))
    {
        uint32_t slot = ((uintptr_t)mon - (uintptr_t)&gParties[B_TRAINER_PLAYER][0]) / sizeof(struct Pokemon);
        GuardMonClear((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, slot}, mon); return;
    }
    if (gPokemonStoragePtr && ExactArrayElement(mon, &gPokemonStoragePtr->boxes[0][0], sizeof(struct BoxPokemon),
                                                TOTAL_BOXES_COUNT * IN_BOX_COUNT))
    {
        uint32_t index = ((uintptr_t)mon - (uintptr_t)&gPokemonStoragePtr->boxes[0][0]) / sizeof(struct BoxPokemon);
        NativeOwnedTrackUnreportedBoxClear(index / IN_BOX_COUNT, index % IN_BOX_COUNT); return;
    }
    if (gPokemonStoragePtr && ExactArrayElement(mon, &gPokemonStoragePtr->fusions[0], sizeof(struct Pokemon), MAX_FUSION_STORAGE))
    {
        uint32_t slot = ((uintptr_t)mon - (uintptr_t)&gPokemonStoragePtr->fusions[0]) / sizeof(struct Pokemon);
        GuardMonClear((struct NativeOwnedLedgerSlot){NATIVE_OWNED_FUSION, 0, slot}, mon); return;
    }
    if (!gSaveBlock1Ptr) return;
    if ((sSkyState || sUnionState || sRentalState || sRecordedState)
        && ExactArrayElement(mon, &gSaveBlock1Ptr->playerParty[0], sizeof(struct Pokemon), PARTY_SIZE))
    { Invalidate(); return; }
    for (uint32_t i = 0; i < DAYCARE_MON_COUNT; ++i)
        if (mon == &gSaveBlock1Ptr->daycare.mons[i].mon)
        { GuardMonClear((struct NativeOwnedLedgerSlot){NATIVE_OWNED_DAYCARE, 0, i}, mon); return; }
#if IS_FRLG
    if (mon == &gSaveBlock1Ptr->route5DayCareMon.mon)
        GuardMonClear((struct NativeOwnedLedgerSlot){NATIVE_OWNED_DAYCARE, 1, 0}, mon);
#endif
}
void NativeOwnedTrackBeforePartyLoad(void)
{
    if (!sRun) return;
    /* These transitions validate their saved and projected parties before and
     * after the load. The follower's later repeated load must be an exact
     * saved echo, since its first full-party load has already completed. */
    if (sSkyState == 3 || sUnionState == 3 || sRentalState == 2)
    {
        if (sSkyState == 3 && (!gSaveBlock1Ptr || *GetSavedPlayerPartyCount() != sSkySavedCount
            || memcmp(gSaveBlock1Ptr->playerParty, sSkyOriginal, sizeof(sSkyOriginal))))
        { Invalidate(); return; }
        if ((sUnionState == 3 || sRentalState == 2)
            && (!gSaveBlock1Ptr || *GetSavedPlayerPartyCount() != (sUnionState == 3 ? sUnionSavedCount : sRentalSavedCount)
                || !UnionSavedPartyMatches())) { Invalidate(); return; }
        if (sPartyLoadConsumed) Invalidate();
        else sPartyLoadConsumed = 1;
        return;
    }
    if (sSkyState == 4 && gSaveBlock1Ptr
        && *GetSavedPlayerPartyCount() == gPartiesCount[B_TRAINER_PLAYER]
        && !memcmp(gSaveBlock1Ptr->playerParty, gParties[B_TRAINER_PLAYER], sizeof(gSaveBlock1Ptr->playerParty))) return;
    Invalidate();
}
void NativeOwnedTrackBeforePartySaveMon(uint32_t slot, const struct Pokemon *mon)
{
    if (!sRun || (!sSkyState && !sUnionState && !sRentalState && !sRecordedState)) return;
    if (!gSaveBlock1Ptr || !mon || slot >= PARTY_SIZE || NativeSaveTransactionActive()) { Invalidate(); return; }
    if (sSkyState == 1 || sSkyState == 4)
    {
        if (mon != &gParties[B_TRAINER_PLAYER][slot]) { Invalidate(); return; }
        if (sSkyState == 1)
        {
            struct NativeOwnedId id;
            if (Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, slot}, &id)
                || (!!mon->box.hasSpecies != !Empty(id)) || (!Empty(id) && !WitnessMatches(id, &mon->box))) Invalidate();
        }
        return;
    }
    if (sSkyState == 3 && !sPartyLoadConsumed && (sSkyMask & (1u << slot)))
    {
        uint32_t reduced = 0;
        for (uint32_t i = 0; i < slot; ++i) reduced += !!(sSkyMask & (1u << i));
        if (reduced < sSkyCount && mon == &gParties[B_TRAINER_PLAYER][reduced]
            && *GetSavedPlayerPartyCount() == sSkySavedCount
            && !memcmp(gSaveBlock1Ptr->playerParty, sSkyOriginal, sizeof(sSkyOriginal))
            && WitnessMatches(sSkyIds[slot], &mon->box) && WitnessMatches(sSkyIds[slot], &GetSavedPlayerPartyMon(slot)->box))
        { sSkyOriginal[slot] = *mon; return; }
    }
    if (sUnionState == 2)
    {
        for (uint32_t i = 0; i < sUnionCount; ++i)
            if (sUnionSlots[i] == slot && mon == &gParties[B_TRAINER_PLAYER][i])
            {
                struct NativeOwnedId id;
                if (!Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, slot}, &id)
                    && UnionSavedPartyMatches() && WitnessMatches(id, &mon->box)
                    && WitnessMatches(id, &GetSavedPlayerPartyMon(slot)->box))
                { sProjectionSaved[slot] = *mon; return; }
                break;
            }
    }
    Invalidate();
}
void NativeOwnedTrackerBattleViewUnsupported(void)
{ if (sRun && sRentalState != 1 && !sRecordedState && (!(sUnionState == 2 && gMain.inBattle) || ModsOwnedFrontierPartyActive())) Invalidate(); }
void NativeOwnedTrackCheckWitness(struct NativeOwnedLedgerSlot slot, const struct BoxPokemon *mon)
{
    if (!sRun) return;
    struct NativeOwnedId id;
    if (Read(slot, &id) || Empty(id) || !WitnessMatches(id, mon)) Invalidate();
}
void NativeOwnedTrackerUnreportedPartySwap(uint32_t first, uint32_t second)
{
    if (!sRun) return;
    if ((sUnionState == 2 && gMain.inBattle) || sRentalState == 1 || sRecordedState == 1) return;
    uint32_t field;
    if (NativeSaveTransactionActive() || !ModsOwnedBattlePartyViewActive()
        || ModsOwnedBattlePartyFieldSlot(first, &field) || ModsOwnedBattlePartyFieldSlot(second, &field)) Invalidate();
}
int NativeOwnedTrackerSeed(uint64_t run)
{
    uint64_t revision;
    if (!run || sRun || !RhhModsHasActiveRun() || RhhModsRunId() != run || !RhhModsCommandsAllowed()
        || gMain.inBattle || ModsOwnedStorageBusy() || ModsOwnedProjectedPartyActive() || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive()
        || NativeSaveTransactionActive() || !gSaveBlock1Ptr || !gPokemonStoragePtr
        || NativeOwnedLedgerRevision(run, &revision) || revision) return -1;
    struct NativeOwnedLedgerSlot slots[PARTY_SIZE + TOTAL_BOXES_COUNT * IN_BOX_COUNT + 3 + MAX_FUSION_STORAGE];
    uint32_t count = 0;
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
        if (gParties[B_TRAINER_PLAYER][i].box.hasSpecies)
            slots[count++] = (struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i};
    for (uint32_t box = 0; box < TOTAL_BOXES_COUNT; ++box)
        for (uint32_t slot = 0; slot < IN_BOX_COUNT; ++slot)
            if (gPokemonStoragePtr->boxes[box][slot].hasSpecies)
                slots[count++] = (struct NativeOwnedLedgerSlot){NATIVE_OWNED_BOX, box, slot};
    for (uint32_t i = 0; i < DAYCARE_MON_COUNT; ++i)
        if (gSaveBlock1Ptr->daycare.mons[i].mon.hasSpecies)
            slots[count++] = (struct NativeOwnedLedgerSlot){NATIVE_OWNED_DAYCARE, 0, i};
#if IS_FRLG
    if (gSaveBlock1Ptr->route5DayCareMon.mon.hasSpecies)
        slots[count++] = (struct NativeOwnedLedgerSlot){NATIVE_OWNED_DAYCARE, 1, 0};
#endif
    for (uint32_t i = 0; i < MAX_FUSION_STORAGE; ++i)
        if (gPokemonStoragePtr->fusions[i].box.hasSpecies)
            slots[count++] = (struct NativeOwnedLedgerSlot){NATIVE_OWNED_FUSION, 0, i};
    if (count && NativeOwnedLedgerMintBatch(run, revision, slots, count, NULL)) return -1;
    sRun = run;
    if (!TemporaryEmpty() || !OccupancyMatches(0) || RememberInitial()
        || NativeOwnedLedgerRevision(run, &sWitnessRevision))
    { (void)NativeOwnedLedgerInvalidate(run); WitnessReset(); sRun = 0; return -1; }
    sWitnessRun = run; sWitnessValid = sWitnessNamesAvailable = 1; sRun = 0; return 0;
}
int NativeOwnedTrackerStart(uint64_t run)
{
    uint64_t revision;
    if (!run || sRun || !RhhModsHasActiveRun() || RhhModsRunId() != run || gMain.inBattle
        || ModsOwnedStorageBusy() || ModsOwnedProjectedPartyActive() || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive()
        || NativeSaveTransactionActive() || NativeOwnedLedgerRevision(run, &revision)) return -1;
    sRun = run; sGroupBoxValid = 0;
    if (!sWitnessValid || sWitnessRun != run || sWitnessRevision != revision
        || !TemporaryEmpty() || !OccupancyMatches(1)) { sRun = 0; return -1; }
    /* Version 1 has no trainer names; capture them only after its original checks pass. */
    if (!sWitnessNamesAvailable)
    {
        if (RememberInitial()) { WitnessReset(); sRun = 0; return -1; }
        sWitnessNamesAvailable = 1;
    }
    NativeOwnedLedgerSetSaveGuard(CheckSaveOccupancy);
    NativeOwnedLedgerSetResetGuard(NativeOwnedTrackerReset);
    NativeSaveSetCommitGuard(CheckSaveCommit);
    return 0;
}
int NativeOwnedTrackerStop(uint64_t run)
{
    if (!run || sRun != run) return -1;
    if (sHatchRun || sPcTradePending || sLinkTradeState || sIngamePartyPending || sSkyState || sUnionState || sRentalState || sRecordedState
        || sFrontierResumeRun || !TemporaryEmpty()
        || !RhhModsHasActiveRun() || RhhModsRunId() != run || gMain.inBattle
        || NativeSaveTransactionActive() || !OccupancyMatches(1))
    { Invalidate(); return -1; }
    if (NativeOwnedLedgerRevision(run, &sWitnessRevision)) { Invalidate(); return -1; }
    NativeOwnedLedgerSetSaveGuard(NULL); NativeOwnedLedgerSetResetGuard(NULL); NativeSaveSetCommitGuard(NULL);
    sRun = 0; sGroupBoxValid = 0; return 0;
}
int NativeOwnedTrackerActive(uint64_t run) { return run && run == sRun; }
int NativeOwnedTrackerAudit(void)
{
    if (!sRun) return 1;
    if (!RhhModsHasActiveRun() || RhhModsRunId() != sRun) { Invalidate(); return -1; }
    if (!ModsOwnedBattlePartySummaryCheck()) return -1;
    if (sRecordedState == 1 && gSaveBlock1Ptr && gPokemonStoragePtr && !gMain.inBattle
        && !NativeSaveTransactionActive() && !ModsOwnedStorageBusy())
    {
        if (!sRecordedBackup || memcmp(sRecordedBackup, sRecordedOriginal, sizeof(sRecordedOriginal))
            || !TemporaryEmpty() || !NonPartyOccupancyMatches(1)) { Invalidate(); return -1; }
        return 0;
    }
    if (sRentalState == 1 && gSaveBlock1Ptr && gPokemonStoragePtr && !gMain.inBattle
        && !NativeSaveTransactionActive() && !ModsOwnedStorageBusy())
    {
        if (*GetSavedPlayerPartyCount() != sRentalSavedCount || !TemporaryEmpty()
            || !UnionSavedPartyMatches() || !NonPartyOccupancyMatches(1)) { Invalidate(); return -1; }
        return 0;
    }
    /* These paths have checked transitions or temporarily move Pokémon out of
     * their field slots. Auditing their intermediate layouts would be wrong. */
    if (!gSaveBlock1Ptr || !gPokemonStoragePtr || gMain.inBattle || sHatchRun || sSkyState || sUnionState || sRentalState || sRecordedState
        || sPcTradePending || sLinkTradeState || sIngamePartyPending || NativeSaveTransactionActive()
        || ModsOwnedStorageBusy() || ModsOwnedProjectedPartyActive() || ModsOwnedBattlePartyViewActive()
        || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive()) return 1;
    if (!TemporaryEmpty() || !OccupancyMatches(1)) { Invalidate(); return -1; }
    return 0;
}
void NativeOwnedTrackerReset(void) { if (sRun) Invalidate(); }
void NativeOwnedTrackRecordedBegin(const struct Pokemon *backup, uint32_t count)
{
    if (!sRun) return;
    if (sRecordedState || sRentalState || sUnionState || sSkyState || sHatchRun || sPcTradePending || sLinkTradeState || sIngamePartyPending
        || !backup || count > PARTY_SIZE || count != gPartiesCount[B_TRAINER_PLAYER]
        || memcmp(backup, gParties[B_TRAINER_PLAYER], sizeof(sRecordedOriginal))
        || !RhhModsHasActiveRun() || RhhModsRunId() != sRun || gMain.inBattle || NativeSaveTransactionActive()
        || ModsOwnedStorageBusy() || ModsOwnedProjectedPartyActive() || ModsOwnedBattlePartyViewActive()
        || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive() || !TemporaryEmpty() || !OccupancyMatches(1))
    { Invalidate(); return; }
    memcpy(sRecordedOriginal, backup, sizeof(sRecordedOriginal));
    sRecordedBackup = backup; sRecordedCount = count; sRecordedState = 1;
}
void NativeOwnedTrackRecordedBeforeRestore(const struct Pokemon *backup)
{
    if (!sRun) return;
    if (sRecordedState != 1 || backup != sRecordedBackup || !backup
        || memcmp(backup, sRecordedOriginal, sizeof(sRecordedOriginal)) || NativeSaveTransactionActive()
        || !RhhModsHasActiveRun() || RhhModsRunId() != sRun || !TemporaryEmpty() || !NonPartyOccupancyMatches(1))
    { Invalidate(); return; }
    sRecordedState = 2;
}
void NativeOwnedTrackRecordedRestored(void)
{
    if (!sRun) return;
    if (sRecordedState != 2 || gPartiesCount[B_TRAINER_PLAYER] != sRecordedCount
        || memcmp(gParties[B_TRAINER_PLAYER], sRecordedOriginal, sizeof(sRecordedOriginal)) || !OccupancyMatches(1))
    { Invalidate(); return; }
    sRecordedState = sRecordedCount = 0; sRecordedBackup = NULL; memset(sRecordedOriginal, 0, sizeof(sRecordedOriginal));
}
void NativeOwnedTrackRentalBegin(void)
{
    if (!sRun) return;
    if (sRentalState == 1)
    {
        if (!RhhModsHasActiveRun() || RhhModsRunId() != sRun || gMain.inBattle || NativeSaveTransactionActive()
            || !gSaveBlock1Ptr || *GetSavedPlayerPartyCount() != sRentalSavedCount || !TemporaryEmpty()
            || !UnionSavedPartyMatches() || !NonPartyOccupancyMatches(1)) Invalidate();
        return;
    }
    if (sRentalState || sRecordedState || sUnionState || sSkyState || sHatchRun || sPcTradePending || sLinkTradeState || sIngamePartyPending
        || !RhhModsHasActiveRun() || RhhModsRunId() != sRun || gMain.inBattle || NativeSaveTransactionActive()
        || ModsOwnedStorageBusy() || ModsOwnedProjectedPartyActive() || ModsOwnedBattlePartyViewActive()
        || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive() || !TemporaryEmpty() || !gSaveBlock1Ptr
        || gPartiesCount[B_TRAINER_PLAYER] > PARTY_SIZE
        || *GetSavedPlayerPartyCount() != gPartiesCount[B_TRAINER_PLAYER]
        || memcmp(gSaveBlock1Ptr->playerParty, gParties[B_TRAINER_PLAYER], sizeof(gSaveBlock1Ptr->playerParty))
        || !OccupancyMatches(1) || !UnionSavedPartyMatches()) { Invalidate(); return; }
    memcpy(sProjectionSaved, gSaveBlock1Ptr->playerParty, sizeof(sProjectionSaved));
    sRentalSavedCount = gPartiesCount[B_TRAINER_PLAYER]; sRentalState = 1;
}
void NativeOwnedTrackRentalBeforeRestore(void)
{
    if (!sRun) return;
    if (sRentalState != 1 || NativeSaveTransactionActive() || !RhhModsHasActiveRun() || RhhModsRunId() != sRun
        || !gSaveBlock1Ptr || *GetSavedPlayerPartyCount() != sRentalSavedCount || !TemporaryEmpty()
        || !UnionSavedPartyMatches() || !NonPartyOccupancyMatches(1)) { Invalidate(); return; }
    sRentalState = 2; sPartyLoadConsumed = 0;
}
void NativeOwnedTrackRentalRestored(void)
{
    if (!sRun) return;
    if (sRentalState != 2 || gPartiesCount[B_TRAINER_PLAYER] != sRentalSavedCount || !OccupancyMatches(1))
    { Invalidate(); return; }
    sRentalState = sRentalSavedCount = sPartyLoadConsumed = 0;
    memset(sProjectionSaved, 0, sizeof(sProjectionSaved));
}
void NativeOwnedTrackUnionBegin(const uint8_t *order, uint32_t count)
{
    if (!sRun) return;
    if (sUnionState || sSkyState || sRentalState || sRecordedState || sHatchRun || sPcTradePending || sLinkTradeState || sIngamePartyPending
        || !order || !count || count > PARTY_SIZE || !RhhModsHasActiveRun() || RhhModsRunId() != sRun
        || gMain.inBattle || NativeSaveTransactionActive() || ModsOwnedStorageBusy() || ModsOwnedBattlePartyViewActive()
        || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive() || !TemporaryEmpty()
        || gPartiesCount[B_TRAINER_PLAYER] > PARTY_SIZE || !gSaveBlock1Ptr
        || *GetSavedPlayerPartyCount() != gPartiesCount[B_TRAINER_PLAYER]
        || memcmp(gSaveBlock1Ptr->playerParty, gParties[B_TRAINER_PLAYER], sizeof(gSaveBlock1Ptr->playerParty))
        || !OccupancyMatches(1) || !UnionSavedPartyMatches()) { Invalidate(); return; }
    uint32_t seen = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t slot = order[i]; struct NativeOwnedId id;
        if (!slot || slot > PARTY_SIZE || (seen & (1u << (slot - 1)))
            || Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, slot - 1}, &id) || Empty(id))
        { Invalidate(); return; }
        seen |= 1u << (slot - 1); sUnionSlots[i] = slot - 1;
        sUnionExpected[i] = gParties[B_TRAINER_PLAYER][slot - 1];
    }
    memcpy(sProjectionSaved, gSaveBlock1Ptr->playerParty, sizeof(sProjectionSaved));
    sUnionSavedCount = gPartiesCount[B_TRAINER_PLAYER]; sUnionCount = count; sUnionState = 1;
}
void NativeOwnedTrackUnionReselect(const uint8_t *order, uint32_t count)
{
    if (!sRun) return;
    if (sUnionState != 2 || !order || !count || count > sUnionCount || !gSaveBlock1Ptr
        || gMain.inBattle || NativeSaveTransactionActive() || !RhhModsHasActiveRun() || RhhModsRunId() != sRun
        || gPartiesCount[B_TRAINER_PLAYER] != sUnionCount || *GetSavedPlayerPartyCount() != sUnionSavedCount
        || !TemporaryEmpty() || !UnionSavedPartyMatches() || !NonPartyOccupancyMatches(1))
    { Invalidate(); return; }
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
    {
        struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][i].box;
        if (i >= sUnionCount) { if (mon->hasSpecies) { Invalidate(); return; } }
        else
        {
            struct NativeOwnedId id;
            if (Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, sUnionSlots[i]}, &id)
                || !WitnessMatches(id, mon)) { Invalidate(); return; }
        }
    }
    uint8_t selected[PARTY_SIZE]; struct Pokemon expected[PARTY_SIZE]; uint32_t seen = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t slot = order[i];
        if (!slot || slot > sUnionCount || (seen & (1u << (slot - 1)))) { Invalidate(); return; }
        seen |= 1u << (slot - 1); selected[i] = sUnionSlots[slot - 1];
        expected[i] = gParties[B_TRAINER_PLAYER][slot - 1];
    }
    memcpy(sUnionSlots, selected, count); memcpy(sUnionExpected, expected, count * sizeof(expected[0]));
    sUnionCount = count; sUnionState = 1;
}
void NativeOwnedTrackUnionPrepared(void)
{
    if (!sRun) return;
    if (sUnionState != 1 || sFrontierResumeRun || !gSaveBlock1Ptr || *GetSavedPlayerPartyCount() != sUnionSavedCount
        || !UnionSavedPartyMatches() || !NonPartyOccupancyMatches(1)) { Invalidate(); return; }
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
    {
        struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][i].box;
        if (i >= sUnionCount) { if (mon->hasSpecies) { Invalidate(); return; } }
        else
        {
            struct NativeOwnedId id;
            if (Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, sUnionSlots[i]}, &id)
                || !WitnessMatches(id, mon)
                || memcmp(&gParties[B_TRAINER_PLAYER][i], &sUnionExpected[i], sizeof(struct Pokemon))) { Invalidate(); return; }
        }
    }
    sUnionState = 2;
}
void NativeOwnedTrackFrontierCapture(const u8 *selected, u32 count, const struct Pokemon *party)
{
    if (!sRun) return;
    uint64_t revision;
    if (sFrontierResumeRun || sUnionState != 2 || !RhhModsHasActiveRun() || RhhModsRunId() != sRun || gMain.inBattle
        || !ModsOwnedFrontierPartyReady() || !selected || !party
        || !count || count != sUnionCount || gPartiesCount[B_TRAINER_PLAYER] != count
        || memcmp(party, gParties[B_TRAINER_PLAYER], sizeof(sFrontierResumeParty))
        || NativeSaveTransactionActive() || !UnionSavedPartyMatches() || !NonPartyOccupancyMatches(1)
        || NativeOwnedLedgerRevision(sRun, &revision)) { Invalidate(); return; }
    for (u32 i = 0; i < count; ++i)
    {
        struct NativeOwnedId id;
        if (selected[i] != sUnionSlots[i] + 1
            || Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, sUnionSlots[i]}, &id)
            || !WitnessMatches(id, &party[i].box)) { Invalidate(); return; }
        sFrontierResumeSlots[i] = sUnionSlots[i];
    }
    for (u32 i = count; i < PARTY_SIZE; ++i) if (party[i].box.hasSpecies) { Invalidate(); return; }
    memcpy(sFrontierResumeParty, party, sizeof(sFrontierResumeParty));
    sFrontierResumeRun = sRun; sFrontierResumeRevision = revision; sFrontierResumeCount = count;
}
void NativeOwnedTrackFrontierResume(const u8 *selected, u32 count, const struct Pokemon *party)
{
    if (!sRun) return;
    uint64_t revision;
    if (sUnionState != 1 || sFrontierResumeRun != sRun || !RhhModsHasActiveRun() || RhhModsRunId() != sRun
        || gMain.inBattle || !selected || !party
        || count != sUnionCount || count != sFrontierResumeCount
        || memcmp(party, sFrontierResumeParty, sizeof(sFrontierResumeParty))
        || NativeSaveTransactionActive() || !UnionSavedPartyMatches() || !NonPartyOccupancyMatches(1)
        || NativeOwnedLedgerRevision(sRun, &revision) || revision != sFrontierResumeRevision)
    { Invalidate(); return; }
    for (u32 i = 0; i < count; ++i)
        if (selected[i] != sFrontierResumeSlots[i] + 1 || sUnionSlots[i] != sFrontierResumeSlots[i])
        { Invalidate(); return; }
    memcpy(sUnionExpected, sFrontierResumeParty, sizeof(sUnionExpected));
    memset(sFrontierResumeParty, 0, sizeof(sFrontierResumeParty));
    sFrontierResumeCount = 0; sFrontierResumeRun = sFrontierResumeRevision = 0;
}
void NativeOwnedTrackUnionBeforeRestore(void)
{
    if (!sRun) return;
    if (sUnionState != 2 || NativeSaveTransactionActive() || !RhhModsHasActiveRun() || RhhModsRunId() != sRun
        || !TemporaryEmpty() || !gSaveBlock1Ptr || *GetSavedPlayerPartyCount() != sUnionSavedCount
        || !UnionSavedPartyMatches() || !NonPartyOccupancyMatches(1)) { Invalidate(); return; }
    sUnionState = 3; sPartyLoadConsumed = 0;
}
void NativeOwnedTrackUnionRestored(void)
{
    if (!sRun) return;
    if (sUnionState != 3 || gPartiesCount[B_TRAINER_PLAYER] != sUnionSavedCount || !OccupancyMatches(1)) { Invalidate(); return; }
    sUnionState = sPartyLoadConsumed = 0; sUnionCount = sUnionSavedCount = 0;
    memset(sProjectionSaved, 0, sizeof(sProjectionSaved));
    memset(sUnionExpected, 0, sizeof(sUnionExpected));
}
void NativeOwnedTrackSkyBegin(uint32_t mask)
{
    if (!sRun) return;
    if (sSkyState || !mask || (mask & ~((1u << PARTY_SIZE) - 1)) || !RhhModsHasActiveRun()
        || RhhModsRunId() != sRun || gMain.inBattle || NativeSaveTransactionActive() || sHatchRun
        || gPartiesCount[B_TRAINER_PLAYER] > PARTY_SIZE
        || !TemporaryEmpty() || !OccupancyMatches(1)) { Invalidate(); return; }
    memcpy(sSkyOriginal, gParties[B_TRAINER_PLAYER], sizeof(sSkyOriginal));
    sSkySavedCount = gPartiesCount[B_TRAINER_PLAYER];
    sSkyCount = 0; sSkyMask = mask;
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
    {
        if (Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, &sSkyIds[i])
            || ((mask & (1u << i)) && Empty(sSkyIds[i]))) { Invalidate(); return; }
        if (mask & (1u << i)) ++sSkyCount;
    }
    sSkyState = 1;
}
static int SkyReducedMatches(void)
{
    uint32_t reduced = 0;
    for (uint32_t field = 0; field < PARTY_SIZE; ++field)
        if (sSkyMask & (1u << field))
        {
            if (reduced >= PARTY_SIZE || !WitnessMatches(sSkyIds[field], &gParties[B_TRAINER_PLAYER][reduced].box)) return 0;
            ++reduced;
        }
    if (reduced != sSkyCount) return 0;
    for (; reduced < PARTY_SIZE; ++reduced)
        if (gParties[B_TRAINER_PLAYER][reduced].box.hasSpecies) return 0;
    return 1;
}
static int SkyReducedMatchesSource(void)
{
    if (!SkyReducedMatches()) return 0;
    uint32_t reduced = 0;
    for (uint32_t field = 0; field < PARTY_SIZE; ++field)
        if (sSkyMask & (1u << field))
        {
            if (memcmp(&gParties[B_TRAINER_PLAYER][reduced], &sSkyOriginal[field], sizeof(struct Pokemon))) return 0;
            ++reduced;
        }
    return 1;
}
void NativeOwnedTrackSkyPrepared(void)
{
    if (!sRun) return;
    if (sSkyState != 1 || !gSaveBlock1Ptr || *GetSavedPlayerPartyCount() != sSkySavedCount
        || memcmp(GetSavedPlayerPartyMon(0), sSkyOriginal, sizeof(sSkyOriginal))
        || !SkyReducedMatchesSource()) { Invalidate(); return; }
    sSkyState = 2;
}
void NativeOwnedTrackSkyBeforeRestore(uint32_t mask)
{
    if (!sRun) return;
    if (sSkyState != 2 || mask != sSkyMask || NativeSaveTransactionActive() || !gSaveBlock1Ptr
        || *GetSavedPlayerPartyCount() != sSkySavedCount
        || memcmp(GetSavedPlayerPartyMon(0), sSkyOriginal, sizeof(sSkyOriginal)) || !SkyReducedMatches())
    { Invalidate(); return; }
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
    {
        struct BoxPokemon *saved = &GetSavedPlayerPartyMon(i)->box;
        if (!!saved->hasSpecies != !Empty(sSkyIds[i])
            || (saved->hasSpecies && !WitnessMatches(sSkyIds[i], saved))) { Invalidate(); return; }
    }
    sSkyState = 3; sPartyLoadConsumed = 0;
}
void NativeOwnedTrackSkyRestored(void)
{
    if (!sRun) return;
    if (sSkyState != 3 || !OccupancyMatches(1)) { Invalidate(); return; }
    sSkyState = sPartyLoadConsumed = 0; sSkyMask = sSkyCount = sSkySavedCount = 0;
    memset(sSkyOriginal, 0, sizeof(sSkyOriginal));
}
void NativeOwnedTrackFollowerCatchLoaded(void)
{
    if (!sRun) return;
    if ((sSkyState != 3 && sSkyState != 4) || !OccupancyMatches(1)) { Invalidate(); return; }
    sSkyState = 4; sPartyLoadConsumed = 0;
}
void NativeOwnedTrackFollowerCatchBeforeReduce(void)
{
    if (!sRun) return;
    if (sSkyState != 4 || !gSaveBlock1Ptr || gPartiesCount[B_TRAINER_PLAYER] > PARTY_SIZE
        || *GetSavedPlayerPartyCount() != gPartiesCount[B_TRAINER_PLAYER]
        || memcmp(GetSavedPlayerPartyMon(0), gParties[B_TRAINER_PLAYER], sizeof(sSkyOriginal))
        || !OccupancyMatches(1)) { Invalidate(); return; }
    memcpy(sSkyOriginal, gParties[B_TRAINER_PLAYER], sizeof(sSkyOriginal));
    sSkySavedCount = gPartiesCount[B_TRAINER_PLAYER];
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
        if (Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, &sSkyIds[i])
            || ((sSkyMask & (1u << i)) && Empty(sSkyIds[i]))) { Invalidate(); return; }
    sSkyState = 5;
}
void NativeOwnedTrackFollowerCatchReduced(void)
{
    if (!sRun) return;
    if (sSkyState != 5 || !SkyReducedMatchesSource()) { Invalidate(); return; }
    sSkyState = 2;
}
static int PcSlot(uint32_t location, uint32_t box, uint32_t slot, struct NativeOwnedLedgerSlot *out)
{
    if (location == RHH_MON_PARTY && !box && slot < PARTY_SIZE)
    { *out = (struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, slot}; return 0; }
    if (location == RHH_MON_BOX && box < TOTAL_BOXES_COUNT && slot < IN_BOX_COUNT)
    { *out = (struct NativeOwnedLedgerSlot){NATIVE_OWNED_BOX, box, slot}; return 0; }
    return -1;
}
static int OwnedSlot(uint32_t location, uint32_t box, uint32_t slot, struct NativeOwnedLedgerSlot *out)
{
    if (!PcSlot(location, box, slot, out)) return 0;
    if (location == RHH_MON_DAYCARE && box <= 1 && ((box == 0 && slot < 2) || (box == 1 && !slot)))
    { *out = (struct NativeOwnedLedgerSlot){NATIVE_OWNED_DAYCARE, box, slot}; return 0; }
    if (location == RHH_MON_FUSION_STORAGE && !box && slot < MAX_FUSION_STORAGE)
    { *out = (struct NativeOwnedLedgerSlot){NATIVE_OWNED_FUSION, 0, slot}; return 0; }
    return -1;
}
static int Commit(struct NativeOwnedLedgerEdit *edits, uint32_t count)
{
    uint64_t revision;
    return NativeOwnedLedgerRevision(sRun, &revision) || NativeOwnedLedgerApply(sRun, revision, edits, count);
}
void NativeOwnedPcTrackPickup(const struct RhhOwnedPickup *pickup)
{
    if (!pickup || !NativeOwnedTrackerActive(pickup->run_id)) return;
    struct NativeOwnedLedgerSlot source, held = {NATIVE_OWNED_CURSOR, 0, 0};
    struct NativeOwnedId id, existing;
    if (PcSlot(pickup->source_location, pickup->source_box, pickup->source_slot, &source)) { Invalidate(); return; }
    if (pickup->flags & RHH_OWNED_PICKUP_GROUP)
    {
        if (pickup->source_location != RHH_MON_BOX) { Invalidate(); return; }
        if (GroupEmpty()) { sGroupBox = pickup->source_box; sGroupBoxValid = 1; }
        if (!sGroupBoxValid || sGroupBox != pickup->source_box) { Invalidate(); return; }
        held = (struct NativeOwnedLedgerSlot){NATIVE_OWNED_GROUP, 0, pickup->source_slot};
    }
    if (Read(source, &id) || Empty(id) || Read(held, &existing) || !Empty(existing)
        || ((pickup->flags & RHH_OWNED_PICKUP_VALID) && !WitnessMatchesSnapshot(id, &pickup->mon)))
    { Invalidate(); return; }
    struct NativeOwnedLedgerEdit edits[] = {{source, id, sEmpty}, {held, sEmpty, id}};
    if (Commit(edits, 2)) Invalidate();
}
void NativeOwnedPcTrackMove(const struct RhhOwnedMove *move)
{
    if (!move || !NativeOwnedTrackerActive(move->run_id)) return;
    struct NativeOwnedLedgerSlot destination, held = {NATIVE_OWNED_CURSOR, 0, 0};
    struct NativeOwnedId id, displaced;
    if (PcSlot(move->destination_location, move->destination_box, move->destination_slot, &destination))
    { Invalidate(); return; }
    if (move->flags & RHH_OWNED_MOVE_GROUP)
    {
        if (move->source_location != RHH_MON_BOX || move->source_slot >= IN_BOX_COUNT || !sGroupBoxValid
            || move->source_box != sGroupBox || (move->flags & RHH_OWNED_MOVE_DISPLACED)) { Invalidate(); return; }
        held = (struct NativeOwnedLedgerSlot){NATIVE_OWNED_GROUP, 0, move->source_slot};
    }
    if (Read(held, &id) || Empty(id) || Read(destination, &displaced)
        || (!!(move->flags & RHH_OWNED_MOVE_DISPLACED) != !Empty(displaced))
        || (!(move->flags & RHH_OWNED_MOVE_BEFORE_UNREADABLE) && !WitnessMatchesSnapshot(id, &move->before))
        || (!(move->flags & RHH_OWNED_MOVE_AFTER_UNREADABLE) && !WitnessMatchesSnapshot(id, &move->after))
        || ((move->flags & RHH_OWNED_MOVE_DISPLACED) && !(move->flags & RHH_OWNED_MOVE_DISPLACED_UNREADABLE)
            && !WitnessMatchesSnapshot(displaced, &move->displaced))) { Invalidate(); return; }
    struct NativeOwnedLedgerEdit edits[] = {{held, id, displaced}, {destination, displaced, id}};
    if (Commit(edits, 2)) Invalidate();
}
void NativeOwnedPcTrackRelease(const struct RhhOwnedRelease *release)
{
    if (!release || !NativeOwnedTrackerActive(release->run_id)) return;
    struct NativeOwnedLedgerSlot source = {NATIVE_OWNED_CURSOR, 0, 0};
    struct NativeOwnedId id;
    if (!(release->flags & RHH_OWNED_RELEASE_MOVING)
        && PcSlot(release->source_location, release->source_box, release->source_slot, &source)) { Invalidate(); return; }
    if (Read(source, &id) || Empty(id)
        || (!(release->flags & RHH_OWNED_RELEASE_UNREADABLE) && !WitnessMatchesSnapshot(id, &release->mon)))
    { Invalidate(); return; }
    struct NativeOwnedLedgerEdit edit = {source, id, sEmpty};
    if (Commit(&edit, 1)) Invalidate();
    else ForgetWitness(id);
}
void NativeOwnedTrackPartyReorder(const struct RhhPartyReorder *batch)
{
    if (!batch || !NativeOwnedTrackerActive(batch->run_id)) return;
    if (!batch->count || batch->count > PARTY_SIZE || batch->reserved) { Invalidate(); return; }
    struct NativeOwnedId old[PARTY_SIZE], next[PARTY_SIZE]; uint8_t sources[PARTY_SIZE] = {0}, targets[PARTY_SIZE] = {0};
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
    {
        if (Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, &old[i])) { Invalidate(); return; }
        next[i] = old[i];
    }
    for (uint32_t i = 0; i < batch->count; ++i)
    {
        const struct RhhPartyRelocation *move = &batch->moves[i];
        uint32_t src = move->source_slot, dst = move->destination_slot;
        if (src >= PARTY_SIZE || dst >= PARTY_SIZE || src == dst || sources[src] || targets[dst]
            || (!!(move->flags & RHH_PARTY_BEFORE_OCCUPIED) != !Empty(old[src]))
            || (!!(move->flags & RHH_PARTY_AFTER_OCCUPIED) != !Empty(old[src]))
            || ((move->flags & RHH_PARTY_BEFORE_VALID) && !WitnessMatchesSnapshot(old[src], &move->before))
            || ((move->flags & RHH_PARTY_AFTER_VALID) && !WitnessMatchesSnapshot(old[src], &move->after)))
        { Invalidate(); return; }
        sources[src] = targets[dst] = 1; next[src] = sEmpty;
    }
    for (uint32_t i = 0; i < batch->count; ++i)
    {
        uint32_t src = batch->moves[i].source_slot, dst = batch->moves[i].destination_slot;
        if (!sources[dst] && !Empty(old[dst])) { Invalidate(); return; }
        next[dst] = old[src];
    }
    struct NativeOwnedLedgerEdit edits[PARTY_SIZE]; uint32_t count = 0;
    for (uint32_t i = 0; i < PARTY_SIZE; ++i)
        if (old[i].high != next[i].high || old[i].low != next[i].low)
            edits[count++] = (struct NativeOwnedLedgerEdit){{NATIVE_OWNED_PARTY, 0, i}, old[i], next[i]};
    if (count && Commit(edits, count)) Invalidate();
}
static void Transfer(uint32_t sourceLocation, uint32_t sourceBox, uint32_t sourceSlot,
                     uint32_t destinationLocation, uint32_t destinationBox, uint32_t destinationSlot)
{
    struct NativeOwnedLedgerSlot source, destination; struct NativeOwnedId id, existing;
    if (OwnedSlot(sourceLocation, sourceBox, sourceSlot, &source)
        || OwnedSlot(destinationLocation, destinationBox, destinationSlot, &destination)
        || Read(source, &id) || Empty(id) || Read(destination, &existing) || !Empty(existing))
    { Invalidate(); return; }
    struct NativeOwnedLedgerEdit edits[] = {{source, id, sEmpty}, {destination, sEmpty, id}};
    if (Commit(edits, 2)) Invalidate();
}
void NativeOwnedTrackDaycare(const struct RhhDaycareTransfer *transfer)
{
    if (!transfer || !NativeOwnedTrackerActive(transfer->run_id)) return;
    struct NativeOwnedLedgerSlot source; struct NativeOwnedId id;
    if (OwnedSlot(transfer->source_location, transfer->source_box, transfer->source_slot, &source)
        || Read(source, &id) || Empty(id)
        || ((transfer->flags & RHH_DAYCARE_TRANSFER_BEFORE_VALID) && !WitnessMatchesSnapshot(id, &transfer->before))
        || ((transfer->flags & RHH_DAYCARE_TRANSFER_AFTER_VALID) && !WitnessMatchesSnapshot(id, &transfer->after)))
    { Invalidate(); return; }
    Transfer(transfer->source_location, transfer->source_box, transfer->source_slot,
             transfer->destination_location, transfer->destination_box, transfer->destination_slot);
}
void NativeOwnedTrackFusion(const struct RhhFusionTransfer *transfer)
{
    if (!transfer || !NativeOwnedTrackerActive(transfer->run_id)) return;
    struct NativeOwnedId primary, id; struct NativeOwnedLedgerSlot source;
    if (Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, transfer->primary_slot}, &primary) || Empty(primary)
        || OwnedSlot(transfer->source_location, 0, transfer->source_slot, &source) || Read(source, &id) || Empty(id)
        || ((transfer->flags & RHH_FUSION_BEFORE_VALID) && !WitnessMatchesSnapshot(id, &transfer->before))
        || ((transfer->flags & RHH_FUSION_AFTER_VALID) && !WitnessMatchesSnapshot(id, &transfer->after))
        || ((transfer->flags & RHH_FUSION_PRIMARY_BEFORE_VALID) && !WitnessMatchesSnapshot(primary, &transfer->primary_before))
        || ((transfer->flags & RHH_FUSION_PRIMARY_AFTER_VALID) && !WitnessMatchesSnapshot(primary, &transfer->primary_after)))
    { Invalidate(); return; }
    Transfer(transfer->source_location, 0, transfer->source_slot,
             transfer->destination_location, transfer->destination_box, transfer->destination_slot);
}
static void NewIndividual(struct NativeOwnedLedgerSlot slot, int replaced)
{
    struct NativeOwnedId old, created; uint64_t revision;
    if (Read(slot, &old) || (!!replaced != !Empty(old)) || NativeOwnedLedgerRevision(sRun, &revision)
        || NativeOwnedLedgerMintReplace(sRun, revision, slot, old, &created)) { Invalidate(); return; }
    ForgetWitness(old);
    if (RememberWitness(created, OwnedMon(slot))) Invalidate();
}
void NativeOwnedTrackGift(const struct RhhOwnedGift *gift)
{
    if (!gift || !NativeOwnedTrackerActive(gift->run_id)) return;
    struct NativeOwnedLedgerSlot destination;
    if (OwnedSlot(gift->destination_location, gift->destination_box, gift->destination_slot, &destination))
    { Invalidate(); return; }
    if ((gift->flags & (RHH_GIFT_REPLACED | RHH_GIFT_REPLACED_VALID)) == (RHH_GIFT_REPLACED | RHH_GIFT_REPLACED_VALID))
    {
        struct NativeOwnedId old;
        if (Read(destination, &old) || !WitnessMatchesSnapshot(old, &gift->replaced)) { Invalidate(); return; }
    }
    NewIndividual(destination, gift->flags & RHH_GIFT_REPLACED);
}
static int WitnessEvolution(struct NativeOwnedId id, const struct RhhOwnedEvolution *evolution, int checkAfter)
{
    return (!(evolution->flags & RHH_EVOLUTION_BEFORE_VALID) || WitnessMatchesSnapshot(id, &evolution->before))
        && (!checkAfter || !(evolution->flags & RHH_EVOLUTION_AFTER_VALID) || WitnessMatchesSnapshot(id, &evolution->after));
}
void NativeOwnedTrackEvolution(const struct RhhOwnedEvolution *evolution)
{
    if (!evolution || !NativeOwnedTrackerActive(evolution->run_id)) return;
    if (evolution->flags & RHH_EVOLUTION_TRADE)
    {
        struct NativeOwnedId id;
        if (evolution->cause != RHH_EVOLUTION_CHANGED) { Invalidate(); return; }
        if ((evolution->flags & RHH_EVOLUTION_PC_PENDING) && sPcTradePending
            && evolution->destination_location == RHH_MON_BOX && evolution->destination_box == sPcTradeBox
            && evolution->destination_slot == sPcTradeSlot
            && !Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_TRADE, 0, 0}, &id) && !Empty(id)
            && WitnessEvolution(id, evolution, 1)) return;
        if ((evolution->flags & RHH_EVOLUTION_LINK_TRADE) && sLinkTradeState
            && evolution->source_location == RHH_MON_PARTY && evolution->source_slot == sLinkTradeSlot
            && evolution->destination_location == RHH_MON_PARTY && evolution->destination_slot == sLinkTradeSlot
            && !Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, sLinkTradeSlot}, &id)
            && id.high == sLinkIncoming.high && id.low == sLinkIncoming.low && WitnessEvolution(id, evolution, 1)) return;
        if (!(evolution->flags & (RHH_EVOLUTION_LINK_TRADE | RHH_EVOLUTION_PC_PENDING)) && sIngamePartyPending
            && evolution->source_location == RHH_MON_PARTY && evolution->source_slot == sIngamePartySlot
            && evolution->destination_location == RHH_MON_PARTY && evolution->destination_slot == sIngamePartySlot
            && !Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, sIngamePartySlot}, &id)
            && id.high == sIngamePartyId.high && id.low == sIngamePartyId.low && WitnessEvolution(id, evolution, 1)) return;
        Invalidate(); return;
    }
    struct NativeOwnedLedgerSlot source, destination; struct NativeOwnedId parent;
    if (OwnedSlot(evolution->source_location, 0, evolution->source_slot, &source)
        || OwnedSlot(evolution->destination_location, evolution->destination_box, evolution->destination_slot, &destination)
        || Read(source, &parent) || Empty(parent)
        || !WitnessEvolution(parent, evolution, evolution->cause != RHH_EVOLUTION_SPLIT)) { Invalidate(); return; }
    if (evolution->cause == RHH_EVOLUTION_SPLIT)
        NewIndividual(destination, evolution->flags & RHH_EVOLUTION_REPLACED);
    else if (evolution->cause != RHH_EVOLUTION_CHANGED || source.location != destination.location
             || source.box != destination.box || source.slot != destination.slot) Invalidate();
}
void NativeOwnedTrackTradePcPickup(uint32_t box, uint32_t slot)
{
    if (!sRun) return;
    struct NativeOwnedLedgerSlot source = {NATIVE_OWNED_BOX, box, slot}, held = {NATIVE_OWNED_TRADE, 0, 0};
    struct NativeOwnedId id, existing;
    if (sPcTradePending || sLinkTradeState || sIngamePartyPending || Read(source, &id) || Empty(id)
        || Read(held, &existing) || !Empty(existing) || !WitnessMatches(id, OwnedMon(held)))
    { Invalidate(); return; }
    struct NativeOwnedLedgerEdit edits[] = {{source, id, sEmpty}, {held, sEmpty, id}};
    if (Commit(edits, 2)) Invalidate();
}
void NativeOwnedTrackIngameTrade(const struct RhhIngameTrade *trade)
{
    if (!trade || !NativeOwnedTrackerActive(trade->run_id)) return;
    if (sPcTradePending || sLinkTradeState || sIngamePartyPending) { Invalidate(); return; }
    if (trade->source_location == RHH_MON_PARTY && trade->destination_location == RHH_MON_PARTY
        && trade->source_slot == trade->destination_slot && !trade->source_box && !trade->destination_box
        && trade->source_slot < PARTY_SIZE)
    {
        struct NativeOwnedLedgerSlot party = {NATIVE_OWNED_PARTY, 0, trade->source_slot};
        struct NativeOwnedId outgoing;
        if (Read(party, &outgoing) || !WitnessMatches(outgoing, &gParties[B_TRAINER_OPPONENT_A][0].box))
        { Invalidate(); return; }
        NewIndividual(party, 1);
        if (sRun && !Read(party, &sIngamePartyId))
        { sIngamePartyPending = 1; sIngamePartySlot = trade->source_slot; }
        return;
    }
    struct NativeOwnedId held, existing;
    struct NativeOwnedLedgerSlot temporary = {NATIVE_OWNED_TRADE, 0, 0};
    struct NativeOwnedLedgerSlot destination = {NATIVE_OWNED_BOX, trade->destination_box, trade->destination_slot};
    if (trade->source_location != RHH_MON_BOX || trade->destination_location != RHH_MON_BOX
        || Read(temporary, &held) || Empty(held) || !WitnessMatches(held, &gParties[B_TRAINER_OPPONENT_A][0].box)
        || Read(destination, &existing) || !Empty(existing))
    { Invalidate(); return; }
    NewIndividual(temporary, 1);
    if (sRun) { sPcTradePending = 1; sPcTradeBox = trade->destination_box; sPcTradeSlot = trade->destination_slot; }
}
int NativeOwnedTrackTradePcPreSync(uint32_t box, uint32_t slot)
{
    if (!sRun) return 1;
    struct NativeOwnedId held, existing;
    struct NativeOwnedLedgerSlot temporary = {NATIVE_OWNED_TRADE, 0, 0}, destination = {NATIVE_OWNED_BOX, box, slot};
    if (!sPcTradePending || box != sPcTradeBox || slot != sPcTradeSlot || Read(temporary, &held) || Empty(held)
        || Read(destination, &existing) || !Empty(existing)
        || !WitnessMatches(held, OwnedMon(temporary)) || !WitnessMatches(held, OwnedMon(destination)))
    { Invalidate(); return 0; }
    return 1;
}
void NativeOwnedTrackTradePcSync(uint32_t box, uint32_t slot)
{
    if (!sRun) return;
    struct NativeOwnedId held, existing;
    struct NativeOwnedLedgerSlot temporary = {NATIVE_OWNED_TRADE, 0, 0}, destination = {NATIVE_OWNED_BOX, box, slot};
    if (!NativeOwnedTrackTradePcPreSync(box, slot)) return;
    if (Read(temporary, &held) || Read(destination, &existing)) { Invalidate(); return; }
    struct NativeOwnedLedgerEdit edits[] = {{temporary, held, sEmpty}, {destination, sEmpty, held}};
    if (Commit(edits, 2)) { Invalidate(); return; }
    sPcTradePending = 0;
}
void NativeOwnedTrackLinkSwap(uint32_t slot, uint32_t partnerSlot)
{
    if (!sRun) return;
    struct NativeOwnedLedgerSlot party = {NATIVE_OWNED_PARTY, 0, slot}, temporary = {NATIVE_OWNED_TRADE, 0, 0};
    struct NativeOwnedId outgoing, empty, incoming;
    if (sPcTradePending || sLinkTradeState || sIngamePartyPending || partnerSlot >= PARTY_SIZE
        || Read(party, &outgoing) || Empty(outgoing)
        || !WitnessMatches(outgoing, &gParties[B_TRAINER_OPPONENT_A][partnerSlot].box)
        || Read(temporary, &empty) || !Empty(empty) || NativeOwnedLedgerGenerate(sRun, &incoming))
    { Invalidate(); return; }
    struct NativeOwnedLedgerEdit edits[] = {{party, outgoing, incoming}, {temporary, empty, outgoing}};
    if (Commit(edits, 2)) { Invalidate(); return; }
    if (RememberWitness(incoming, OwnedMon(party))) { Invalidate(); return; }
    sLinkTradeState = 1; sLinkTradeSlot = slot; sLinkOutgoing = outgoing; sLinkIncoming = incoming;
}
void NativeOwnedTrackLinkPrepareSave(uint32_t slot, int confirmed)
{
    if (!sRun || !sLinkTradeState) return;
    struct NativeOwnedId party, temporary;
    struct NativeOwnedLedgerSlot held = {NATIVE_OWNED_TRADE, 0, 0};
    if (!confirmed || sLinkTradeState != 1 || slot != sLinkTradeSlot
        || Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, slot}, &party)
        || party.high != sLinkIncoming.high || party.low != sLinkIncoming.low
        || !WitnessMatches(party, OwnedMon((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, slot}))
        || Read(held, &temporary) || temporary.high != sLinkOutgoing.high || temporary.low != sLinkOutgoing.low)
    { Invalidate(); return; }
    struct NativeOwnedLedgerEdit edit = {held, temporary, sEmpty};
    if (Commit(&edit, 1)) { Invalidate(); return; }
    ForgetWitness(temporary);
    sLinkTradeState = 2;
}
void NativeOwnedTrackLinkFinish(int saved)
{
    if (!sRun || !sLinkTradeState) return;
    if (!saved || sLinkTradeState != 2) { Invalidate(); return; }
    sLinkTradeState = 0;
}
void NativeOwnedTrackTradeEnd(void)
{
    if (!sRun) return;
    struct NativeOwnedId held;
    if (sLinkTradeState || sPcTradePending || Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_TRADE, 0, 0}, &held)
        || !Empty(held)) { Invalidate(); return; }
    sIngamePartyPending = 0;
}
void NativeOwnedTrackCatch(const struct RhhCatchPlacement *placement)
{
    if (!placement || !NativeOwnedTrackerActive(placement->run_id)) return;
    if (placement->cause == RHH_CATCH_DISPLACED_PARTY)
    {
        struct NativeOwnedLedgerSlot source; struct NativeOwnedId id;
        if (OwnedSlot(placement->source_location, 0, placement->source_slot, &source) || Read(source, &id) || Empty(id)
            || ((placement->flags & RHH_CATCH_BEFORE_VALID) && !WitnessMatchesSnapshot(id, &placement->before))
            || ((placement->flags & RHH_CATCH_AFTER_VALID) && !WitnessMatchesSnapshot(id, &placement->after)))
        { Invalidate(); return; }
        Transfer(placement->source_location, 0, placement->source_slot,
                 placement->destination_location, placement->destination_box, placement->destination_slot);
    }
    else if (placement->cause == RHH_CATCH_ACQUIRED)
    {
        struct NativeOwnedLedgerSlot destination;
        if (placement->source_location != RHH_MON_STAGED
            || OwnedSlot(placement->destination_location, placement->destination_box, placement->destination_slot, &destination))
        { Invalidate(); return; }
        NewIndividual(destination, 0);
    }
    else Invalidate();
}
void NativeOwnedTrackEggCreated(uint32_t partySlot)
{
    if (!sRun) return;
    if (!RhhModsHasActiveRun() || RhhModsRunId() != sRun || gMain.inBattle || ModsOwnedStorageBusy()
        || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive() || NativeSaveTransactionActive() || RhhModsCurrentEvent()
        || partySlot >= PARTY_SIZE || !gParties[B_TRAINER_PLAYER][partySlot].box.hasSpecies
        || !gParties[B_TRAINER_PLAYER][partySlot].box.isEgg) { Invalidate(); return; }
    NewIndividual((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, partySlot}, 0);
}
int NativeOwnedTrackHatchBefore(uint32_t partySlot)
{
    if (!sRun) return 0;
    if (!RhhModsHasActiveRun() || RhhModsRunId() != sRun || gMain.inBattle || ModsOwnedStorageBusy()
        || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive() || NativeSaveTransactionActive() || RhhModsCurrentEvent()
        || sHatchRun || partySlot >= PARTY_SIZE || !gParties[B_TRAINER_PLAYER][partySlot].box.hasSpecies
        || Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, partySlot}, &sHatchId) || Empty(sHatchId)
        || !WitnessMatches(sHatchId, &gParties[B_TRAINER_PLAYER][partySlot].box))
    { Invalidate(); return 0; }
    /* The script can be called again for a non-egg. Do not authorize an OT
     * change in that case; an actual identity write will fail closed. */
    if (!gParties[B_TRAINER_PLAYER][partySlot].box.isEgg) return 0;
    sHatchRun = sRun; sHatchSlot = partySlot; sHatchRekeyApplied = sHatchNameApplied = 0; return 1;
}
void NativeOwnedTrackHatchAfter(uint32_t partySlot)
{
    if (!sHatchRun || !NativeOwnedTrackerActive(sHatchRun)) return;
    struct NativeOwnedId after;
    if (partySlot != sHatchSlot || partySlot >= PARTY_SIZE || !gParties[B_TRAINER_PLAYER][partySlot].box.hasSpecies
        || gParties[B_TRAINER_PLAYER][partySlot].box.isEgg || !gSaveBlock2Ptr
        || gParties[B_TRAINER_PLAYER][partySlot].box.otId != READ_OTID_FROM_SAVE
        || !WitnessPersonalityMatches(sHatchId, &gParties[B_TRAINER_PLAYER][partySlot].box)
        || memcmp(gParties[B_TRAINER_PLAYER][partySlot].box.otName, gSaveBlock2Ptr->playerName, PLAYER_NAME_LENGTH)
        || (!sHatchRekeyApplied && !WitnessOtMatches(sHatchId, &gParties[B_TRAINER_PLAYER][partySlot].box))
        || (!sHatchNameApplied && !WitnessNameMatches(sHatchId, &gParties[B_TRAINER_PLAYER][partySlot].box))
        || Read((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, partySlot}, &after)
        || after.high != sHatchId.high || after.low != sHatchId.low) { Invalidate(); return; }
    if (RememberWitness(after, &gParties[B_TRAINER_PLAYER][partySlot].box)) { Invalidate(); return; }
    sHatchRun = 0; sHatchRekeyApplied = sHatchNameApplied = 0;
}
#ifdef RHH_OWNED_IDENTITY_TEST
/* The integration harness seeds deliberate raw slot mutations between moves. */
int NativeOwnedTrackerTestRemember(struct NativeOwnedLedgerSlot slot)
{ return sRun ? RememberSlot(slot) : -1; }
#endif
