#ifndef NATIVE_OWNED_IDENTITY_LEDGER_H
#define NATIVE_OWNED_IDENTITY_LEDGER_H

#include <stdint.h>
#include <stddef.h>

/* Internal storage substrate. Randomly minted IDs make reuse after rollback
 * overwhelmingly unlikely. The ledger does not prove a slot's provenance. */
struct NativeOwnedId { uint64_t high, low; };
enum NativeOwnedLedgerLocation {
    NATIVE_OWNED_PARTY = 1, NATIVE_OWNED_BOX, NATIVE_OWNED_DAYCARE,
    NATIVE_OWNED_FUSION, NATIVE_OWNED_CURSOR, NATIVE_OWNED_GROUP, NATIVE_OWNED_TRADE
};
struct NativeOwnedLedgerSlot { uint32_t location, box, slot; };
struct NativeOwnedLedgerEdit {
    struct NativeOwnedLedgerSlot slot;
    struct NativeOwnedId expected, value;
};
enum NativeOwnedLedgerResult {
    NATIVE_OWNED_LEDGER_OK = 0, NATIVE_OWNED_LEDGER_INVALID = -1,
    NATIVE_OWNED_LEDGER_STALE = -2, NATIVE_OWNED_LEDGER_CONFLICT = -3,
    NATIVE_OWNED_LEDGER_ENTROPY = -4
};

int NativeOwnedLedgerInstall(void);
int NativeOwnedLedgerStart(uint64_t run);
int NativeOwnedLedgerRevision(uint64_t run, uint64_t *revision);
int NativeOwnedLedgerRead(uint64_t run, struct NativeOwnedLedgerSlot slot, struct NativeOwnedId *out);
int NativeOwnedLedgerMatches(uint64_t run, uint64_t revision, const struct NativeOwnedId *ids, uint32_t count);
/* All expected values are checked against the original state, then values are
 * installed simultaneously. Duplicate slots and duplicate nonzero IDs fail. */
int NativeOwnedLedgerApply(uint64_t run, uint64_t revision, const struct NativeOwnedLedgerEdit *edits, uint32_t count);
int NativeOwnedLedgerMint(uint64_t run, uint64_t revision, struct NativeOwnedLedgerSlot slot, struct NativeOwnedId *out);
int NativeOwnedLedgerGenerate(uint64_t run, struct NativeOwnedId *out);
int NativeOwnedLedgerMintReplace(uint64_t run, uint64_t revision, struct NativeOwnedLedgerSlot slot,
                                struct NativeOwnedId expected, struct NativeOwnedId *out);
int NativeOwnedLedgerMintBatch(uint64_t run, uint64_t revision, const struct NativeOwnedLedgerSlot *slots,
                              uint32_t count, struct NativeOwnedId *out);
int NativeOwnedLedgerInvalidate(uint64_t run);
/* Internal tracker guard, invoked only for an actual save export. */
void NativeOwnedLedgerSetSaveGuard(int (*guard)(uint64_t run));
void NativeOwnedLedgerSetResetGuard(void (*guard)(void));
int NativeOwnedEntropy(void *out, uint32_t size);
#ifdef RHH_OWNED_LEDGER_TEST
void NativeOwnedLedgerSetEntropyForTest(int (*source)(void *, uint32_t));
#endif
/* Experimental tracker. Keep its ledger private until all transitions are wired. */
int NativeOwnedTrackerSeed(uint64_t run);
int NativeOwnedTrackerInstall(void);
int NativeOwnedTrackerStart(uint64_t run);
int NativeOwnedTrackerStop(uint64_t run);
int NativeOwnedTrackerActive(uint64_t run);
/* 0: checked, 1: inactive or temporarily unsafe, -1: invalidated. */
int NativeOwnedTrackerAudit(void);
void NativeOwnedTrackerReset(void);
void NativeOwnedTrackerUnsupported(void);
struct BoxPokemon;
struct Pokemon;
/* Direct creation into owned storage has no transfer/creation receipt. */
void NativeOwnedTrackUnreportedCreation(const struct BoxPokemon *mon);
/* The hatch rekey is the only currently verified identity-field transition. */
void NativeOwnedTrackUnreportedIdentityChange(const struct BoxPokemon *mon, int hatchRekey, uint32_t newOt);
void NativeOwnedTrackUnreportedTrainerNameChange(const struct BoxPokemon *mon, const uint8_t *name);
void NativeOwnedTrackUnreportedPartyClear(void);
void NativeOwnedTrackUnreportedBoxClear(uint32_t box, uint32_t slot);
void NativeOwnedTrackUnreportedMonClear(const struct BoxPokemon *mon);
void NativeOwnedTrackBeforePartyLoad(void);
void NativeOwnedTrackBeforePartySaveMon(uint32_t slot, const struct Pokemon *mon);
void NativeOwnedTrackUnreportedCopy(const void *dest, const void *src, size_t size);
/* A link engine self-echo may retain bindings only if every byte is unchanged. */
void NativeOwnedTrackLocalEcho(void *dest, const void *src, size_t size);
/* Check the unencrypted change detector even when a full snapshot is invalid. */
void NativeOwnedTrackCheckWitness(struct NativeOwnedLedgerSlot slot, const struct BoxPokemon *mon);
struct RhhOwnedPickup;
struct RhhOwnedMove;
struct RhhOwnedRelease;
void NativeOwnedPcTrackPickup(const struct RhhOwnedPickup *pickup);
void NativeOwnedPcTrackMove(const struct RhhOwnedMove *move);
void NativeOwnedPcTrackRelease(const struct RhhOwnedRelease *release);
struct RhhPartyReorder;
struct RhhDaycareTransfer;
struct RhhFusionTransfer;
struct RhhOwnedGift;
struct RhhOwnedEvolution;
struct RhhCatchPlacement;
struct RhhIngameTrade;
void NativeOwnedTrackPartyReorder(const struct RhhPartyReorder *batch);
void NativeOwnedTrackDaycare(const struct RhhDaycareTransfer *transfer);
void NativeOwnedTrackFusion(const struct RhhFusionTransfer *transfer);
void NativeOwnedTrackGift(const struct RhhOwnedGift *gift);
void NativeOwnedTrackEvolution(const struct RhhOwnedEvolution *evolution);
void NativeOwnedTrackCatch(const struct RhhCatchPlacement *placement);
void NativeOwnedTrackEggCreated(uint32_t partySlot);
int NativeOwnedTrackHatchBefore(uint32_t partySlot);
void NativeOwnedTrackHatchAfter(uint32_t partySlot);
void NativeOwnedTrackTradePcPickup(uint32_t box, uint32_t slot);
void NativeOwnedTrackIngameTrade(const struct RhhIngameTrade *trade);
int NativeOwnedTrackTradePcPreSync(uint32_t box, uint32_t slot);
void NativeOwnedTrackTradePcSync(uint32_t box, uint32_t slot);
void NativeOwnedTrackLinkSwap(uint32_t slot, uint32_t partnerSlot);
void NativeOwnedTrackLinkPrepareSave(uint32_t slot, int confirmed);
void NativeOwnedTrackLinkFinish(int saved);
void NativeOwnedTrackTradeEnd(void);
void NativeOwnedTrackSkyBegin(uint32_t mask);
void NativeOwnedTrackSkyPrepared(void);
void NativeOwnedTrackSkyBeforeRestore(uint32_t mask);
void NativeOwnedTrackSkyRestored(void);
void NativeOwnedTrackFollowerCatchLoaded(void);
void NativeOwnedTrackFollowerCatchBeforeReduce(void);
void NativeOwnedTrackFollowerCatchReduced(void);
void NativeOwnedTrackUnionBegin(const uint8_t *order, uint32_t count);
void NativeOwnedTrackUnionReselect(const uint8_t *order, uint32_t count);
void NativeOwnedTrackUnionPrepared(void);
void NativeOwnedTrackUnionBeforeRestore(void);
void NativeOwnedTrackUnionRestored(void);
void NativeOwnedTrackRentalBegin(void);
void NativeOwnedTrackRentalBeforeRestore(void);
void NativeOwnedTrackRentalRestored(void);
void NativeOwnedTrackRecordedBegin(const struct Pokemon *backup, uint32_t count);
void NativeOwnedTrackRecordedBeforeRestore(const struct Pokemon *backup);
void NativeOwnedTrackRecordedRestored(void);
struct NativeSaveArchiveCodec;
const struct NativeSaveArchiveCodec *NativeOwnedLedgerCodec(void);

#endif
