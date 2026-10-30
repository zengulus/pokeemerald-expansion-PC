#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "native.h"
#include "mod_host.h"
#include "owned_identity_ledger.h"
#include "agb_flash.h"
#include "gba/flash_internal.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%d: %s\n", __LINE__, #c); exit(1); } } while (0)
static const struct RhhModDefinition sMod = {.struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:owned_ledger", .state_schema = 1, .flags = RHH_MOD_REQUIRED, .state_size = 1};
static int sRejectImport;
static int EmptyExport(uint8_t **out, uint32_t *size) { *out = NULL; *size = 0; return 0; }
static int EmptyValidate(const uint8_t *data, uint32_t size) { return !!data || !!size; }
static int EmptyImport(const uint8_t *data, uint32_t size)
{
    if (EmptyValidate(data, size)) return -1;
    if (sRejectImport) { sRejectImport = 0; return -1; }
    return 0;
}
static void EmptyReset(void) {}
static const struct NativeSaveArchiveCodec sFailCodec = {
    EmptyExport, EmptyValidate, EmptyImport, EmptyReset, "test:fail_once", NULL
};
static const struct NativeOwnedId zero = {0, 0}, a = {0x111, 0xaaa}, b = {0x111, 0xbbb};
static int FailEntropy(void *out, uint32_t size) { (void)out; (void)size; return -1; }
static int CollidingEntropy(void *out, uint32_t size)
{ if (size != sizeof(a)) return -1; memcpy(out, &a, size); return 0; }
static const struct NativeOwnedLedgerSlot party0 = {NATIVE_OWNED_PARTY, 0, 0};
static const struct NativeOwnedLedgerSlot party1 = {NATIVE_OWNED_PARTY, 0, 1};
static const struct NativeOwnedLedgerSlot box7 = {NATIVE_OWNED_BOX, 0, 7};
static const struct NativeOwnedLedgerSlot cursor = {NATIVE_OWNED_CURSOR, 0, 0};
static void Expect(struct NativeOwnedLedgerSlot slot, struct NativeOwnedId expected)
{
    struct NativeOwnedId got = {0x77, 0x88};
    CHECK(!NativeOwnedLedgerRead(456, slot, &got));
    CHECK(got.high == expected.high && got.low == expected.low);
}
static void Save(uint32_t generation)
{ CHECK(!NativeSaveBegin() && !ProgramFlashByte(0, 5, generation) && !NativeSaveEnd(generation, 1)); }
static void CheckGen1(void)
{
    uint64_t revision = 0; CHECK(!NativeOwnedLedgerRevision(456, &revision) && revision == 3);
    Expect(party0, zero); Expect(party1, b); Expect(box7, a); Expect(cursor, zero);
}
static void CheckGen2(void)
{
    uint64_t revision = 0; CHECK(!NativeOwnedLedgerRevision(456, &revision) && revision == 4);
    Expect(party0, b); Expect(party1, a); Expect(box7, zero); Expect(cursor, zero);
}
int main(int argc, char **argv)
{
    CHECK(argc == 3);
    RhhModsInit(); CHECK(!RhhModsRegister(&sMod) && !RhhModsFinalize());
    CHECK(!NativeOwnedLedgerInstall() && !NativeSaveRegisterArchive(&sFailCodec));
    if (!strcmp(argv[1], "reject"))
    { CHECK(NativeSaveOpen(argv[2])); NativeSaveClose(); RhhModsShutdown(); return 0; }
    CHECK(!NativeSaveOpen(argv[2]));
    if (!strcmp(argv[1], "write"))
    {
        CHECK(!NativeOwnedLedgerStart(456));
        struct NativeOwnedLedgerSlot seedSlots[] = {party0, party1};
        struct NativeOwnedId seeded[] = {{77, 88}, {99, 111}}, savedSeeded[2];
        memcpy(savedSeeded, seeded, sizeof(seeded));
        NativeOwnedLedgerSetEntropyForTest(FailEntropy);
        CHECK(NativeOwnedLedgerMintBatch(456, 0, seedSlots, 2, seeded) == NATIVE_OWNED_LEDGER_ENTROPY
            && !memcmp(seeded, savedSeeded, sizeof(seeded)));
        NativeOwnedLedgerSetEntropyForTest(CollidingEntropy);
        CHECK(NativeOwnedLedgerMintBatch(456, 0, seedSlots, 2, seeded) == NATIVE_OWNED_LEDGER_ENTROPY
            && !memcmp(seeded, savedSeeded, sizeof(seeded)));
        NativeOwnedLedgerSetEntropyForTest(NULL); Expect(party0, zero); Expect(party1, zero);
        struct NativeOwnedLedgerEdit create[] = {{party0, zero, a}, {party1, zero, b}};
        CHECK(!NativeOwnedLedgerApply(456, 0, create, 2));
        struct NativeOwnedLedgerEdit duplicate[] = {{party0, a, b}};
        CHECK(NativeOwnedLedgerApply(456, 1, duplicate, 1) == NATIVE_OWNED_LEDGER_CONFLICT);
        struct NativeOwnedLedgerEdit duplicateSlots[] = {{party0, a, zero}, {party0, a, b}};
        CHECK(NativeOwnedLedgerApply(456, 1, duplicateSlots, 2) == NATIVE_OWNED_LEDGER_INVALID);
        struct NativeOwnedLedgerEdit wrong[] = {{party0, b, zero}};
        CHECK(NativeOwnedLedgerApply(456, 1, wrong, 1) == NATIVE_OWNED_LEDGER_STALE);
        struct NativeOwnedLedgerEdit partlyWrong[] = {{party0, a, zero}, {party1, a, zero}};
        CHECK(NativeOwnedLedgerApply(456, 1, partlyWrong, 2) == NATIVE_OWNED_LEDGER_STALE);
        CHECK(NativeOwnedLedgerApply(456, 0, create, 2) == NATIVE_OWNED_LEDGER_STALE);
        struct NativeOwnedLedgerEdit noop[] = {{party0, a, a}};
        CHECK(NativeOwnedLedgerApply(456, 1, noop, 1) == NATIVE_OWNED_LEDGER_INVALID);
        Expect(party0, a); Expect(party1, b);
        struct NativeOwnedLedgerEdit pick[] = {{party0, a, zero}, {cursor, zero, a}};
        CHECK(!NativeOwnedLedgerApply(456, 1, pick, 2));
        CHECK(NativeSaveBegin() && !NativeSaveTransactionActive()); Expect(cursor, a);
        struct NativeOwnedLedgerEdit place[] = {{cursor, a, zero}, {box7, zero, a}};
        CHECK(!NativeOwnedLedgerApply(456, 2, place, 2)); CheckGen1(); Save(101);
        struct NativeOwnedLedgerEdit cycle[] = {{party1, b, a}, {party0, zero, b}, {box7, a, zero}};
        CHECK(!NativeOwnedLedgerApply(456, 3, cycle, 3)); CheckGen2(); Save(102);
        CHECK(!NativeSaveBegin());
        CHECK(NativeOwnedLedgerApply(456, 4, pick, 2) == NATIVE_OWNED_LEDGER_INVALID
            && NativeOwnedLedgerStart(999) == NATIVE_OWNED_LEDGER_INVALID);
        CHECK(NativeSaveEnd(102, 0)); CheckGen2();
        CHECK(!NativeSaveBegin() && !ProgramFlashByte(0, 5, 123) && !NativeOwnedLedgerInvalidate(456));
        CHECK(NativeSaveEnd(103, 1) && gNativeFlash[5] == 102); /* A poisoned snapshot cannot commit. */
        Expect(party0, zero); Expect(party1, zero);
        CHECK(!NativeSaveRestoreMods(102)); CheckGen2();
        CHECK(!NativeSaveRestoreMods(101)); CheckGen1();
        CHECK(!NativeSaveRestoreMods(102)); CheckGen2();
        struct NativeOwnedLedgerSlot fresh = {NATIVE_OWNED_BOX, 0, 10};
        struct NativeOwnedId minted = {77, 88}, prior = minted, mintedAgain = {0, 0};
        CHECK(NativeOwnedLedgerMint(456, 3, fresh, &minted) == NATIVE_OWNED_LEDGER_STALE
            && !memcmp(&minted, &prior, sizeof(minted)));
        CHECK(NativeOwnedLedgerMint(456, 4, party0, &minted) == NATIVE_OWNED_LEDGER_CONFLICT
            && !memcmp(&minted, &prior, sizeof(minted)));
        NativeOwnedLedgerSetEntropyForTest(FailEntropy);
        CHECK(NativeOwnedLedgerMint(456, 4, fresh, &minted) == NATIVE_OWNED_LEDGER_ENTROPY
            && !memcmp(&minted, &prior, sizeof(minted)));
        NativeOwnedLedgerSetEntropyForTest(CollidingEntropy);
        CHECK(NativeOwnedLedgerMint(456, 4, fresh, &minted) == NATIVE_OWNED_LEDGER_ENTROPY
            && !memcmp(&minted, &prior, sizeof(minted)));
        NativeOwnedLedgerSetEntropyForTest(NULL); CheckGen2();
        CHECK(!NativeOwnedLedgerMint(456, 4, fresh, &minted) && (minted.high || minted.low));
        Expect(fresh, minted); CHECK(!NativeSaveRestoreMods(102)); CheckGen2();
        CHECK(!NativeOwnedLedgerMint(456, 4, fresh, &mintedAgain)
            && (minted.high != mintedAgain.high || minted.low != mintedAgain.low));
        CHECK(!NativeSaveRestoreMods(102)); CheckGen2();
        CHECK(NativeOwnedLedgerMintReplace(456, 4, party0, a, &minted) == NATIVE_OWNED_LEDGER_STALE);
        CHECK(!NativeOwnedLedgerMintReplace(456, 4, party0, b, &minted)
            && (minted.high != b.high || minted.low != b.low));
        Expect(party0, minted); CHECK(!NativeSaveRestoreMods(102)); CheckGen2();
        struct NativeOwnedLedgerSlot group = {NATIVE_OWNED_GROUP, 0, 29};
        struct NativeOwnedLedgerEdit groupHold[] = {{party0, b, zero}, {group, zero, b}};
        CHECK(!NativeOwnedLedgerApply(456, 4, groupHold, 2)); Expect(group, b);
        CHECK(NativeSaveBegin() && !NativeSaveTransactionActive());
        CHECK(!NativeSaveRestoreMods(102)); CheckGen2();
        struct NativeOwnedLedgerEdit hold[] = {{party0, b, zero}, {cursor, zero, b}};
        CHECK(!NativeOwnedLedgerApply(456, 4, hold, 2));
        sRejectImport = 1; CHECK(NativeSaveRestoreMods(101));
        uint64_t revision = 0; CHECK(!NativeOwnedLedgerRevision(456, &revision) && revision == 5);
        Expect(party0, zero); Expect(party1, a); Expect(cursor, b);
        CHECK(!NativeSaveRestoreMods(102)); CheckGen2();
        const struct NativeSaveArchiveCodec *codec = NativeOwnedLedgerCodec();
        uint8_t *encoded = NULL; uint32_t length = 0;
        CHECK(!codec->export_data(&encoded, &length) && encoded && length > 64 && !codec->validate(encoded, length));
        memcpy(encoded + 48, encoded + 32, 16); /* duplicate two occupied party IDs */
        CHECK(codec->validate(encoded, length) && codec->import_data(encoded, length)); free(encoded); CheckGen2();
        struct NativeOwnedId sentinel = {77, 88}, old = sentinel;
        CHECK(NativeOwnedLedgerRead(999, party0, &sentinel) && !memcmp(&sentinel, &old, sizeof(old)));
        struct NativeOwnedLedgerSlot invalid = {NATIVE_OWNED_DAYCARE, 1, 1};
        CHECK(NativeOwnedLedgerRead(456, invalid, &sentinel) && !memcmp(&sentinel, &old, sizeof(old)));
    }
    else if (!strcmp(argv[1], "read"))
    {
        CheckGen2(); CHECK(!NativeSaveRestoreMods(101)); CheckGen1();
        CHECK(!NativeSaveRestoreMods(102)); CheckGen2();
    }
    else CHECK(0);
    NativeSaveClose(); RhhModsShutdown(); puts("owned identity ledger: atomic mapping and generation restore passed"); return 0;
}
