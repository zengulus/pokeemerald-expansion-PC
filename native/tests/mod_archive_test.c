#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "global.h"
#include "native.h"
#include "mod_host.h"
#include "agb_flash.h"
#include "gba/flash_internal.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%d: %s (%s)\n", __LINE__, #c, RhhModsError()); exit(1); } } while (0)
static const struct RhhModArchiveService *sArchive;
static RhhContentKey sOwnerA, sOwnerB, sKey1, sKey2;
static u8 sData[130000];
static int sMode;
static bool sNestedChecked;
static unsigned sCommittedEvents, sRejectedPreview;
static void UpdateA(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhModArchiveRecord record = {.struct_size = sizeof(record)}, saved = record;
    if (sMode == 1)
    {
        memset(sData, 'a', sizeof(sData));
        CHECK(sArchive->put(sOwnerB, sKey1, 0, sData, 1, &record) && !memcmp(&record, &saved, sizeof(record)));
        CHECK(!sArchive->put(sOwnerA, sKey1, 0, sData, sizeof(sData), &record));
        CHECK(record.owner == sOwnerA && record.key == sKey1 && record.run_id == 99 && record.size == sizeof(sData) && record.revision);
        saved = record;
        CHECK(sArchive->put(sOwnerA, sKey1, 0, sData, sizeof(sData), &record) && !memcmp(&record, &saved, sizeof(record)));
    }
    else if (sMode == 2)
    {
        CHECK(!sArchive->find(sOwnerA, sKey1, &record)); u64 oldRevision = record.revision;
        memset(sData, 'b', sizeof(sData));
        CHECK(!sArchive->put(sOwnerA, sKey1, oldRevision, sData, sizeof(sData), &record) && record.revision > oldRevision);
        saved = record;
        CHECK(sArchive->put(sOwnerA, sKey1, oldRevision, sData, sizeof(sData), &record) && !memcmp(&record, &saved, sizeof(record))
            && sArchive->remove(sOwnerA, sKey1, oldRevision));
        CHECK(!sArchive->put(sOwnerA, sKey2, 0, "two", 3, &record));
        u64 deleted = record.revision;
        CHECK(!sArchive->remove(sOwnerA, sKey2, deleted) && sArchive->remove(sOwnerA, sKey2, deleted));
        CHECK(!sArchive->put(sOwnerA, sKey2, 0, "two", 3, &record) && record.revision > deleted);
    }
    else if (sMode == 3)
        CHECK(sArchive->put(sOwnerA, sKey2, 0, "x", 1, &record) && !memcmp(&record, &saved, sizeof(record)));
    else if (sMode == 4)
    {
        const u32 bigSize = 600 * 1024;
        u8 *big = calloc(bigSize, 1); CHECK(big);
        CHECK(!sArchive->put(sOwnerA, 1, 0, big, bigSize, &record));
        struct RhhModArchiveOperation ops[] = {
            {.struct_size = sizeof(ops[0]), .kind = RHH_MOD_ARCHIVE_PUT, .key = 2,
             .size = bigSize, .data = big},
            {.struct_size = sizeof(ops[0]), .kind = RHH_MOD_ARCHIVE_REMOVE, .key = 1, .expected_revision = record.revision}
        };
        struct RhhModArchiveBatchResult batch = {.struct_size = sizeof(batch)};
        CHECK(!sArchive->batch(sOwnerA, ops, 2, &batch) && batch.count == 2
            && batch.operations[1].revision == batch.operations[0].revision + 1);
        CHECK(!sArchive->find(sOwnerA, 2, &record) && record.size == bigSize
            && !sArchive->remove(sOwnerA, 2, record.revision));
        free(big);
        for (u64 key = 1; key <= 300; ++key)
            CHECK(!sArchive->put(sOwnerA, key, 0, "x", 1, &record));
        CHECK(!sArchive->find(sOwnerA, 1, &record));
        struct RhhModArchiveOperation fullOps[] = {
            {.struct_size = sizeof(fullOps[0]), .kind = RHH_MOD_ARCHIVE_PUT, .key = 999, .size = 1, .data = "y"},
            {.struct_size = sizeof(fullOps[0]), .kind = RHH_MOD_ARCHIVE_REMOVE, .key = 1, .expected_revision = record.revision}
        };
        CHECK(!sArchive->batch(sOwnerA, fullOps, 2, &batch));
        CHECK(!sArchive->find(sOwnerA, 999, &record));
        fullOps[0].key = 1; fullOps[1].key = 999; fullOps[1].expected_revision = record.revision;
        CHECK(!sArchive->batch(sOwnerA, fullOps, 2, &batch));
        for (u64 key = 1; key <= 300; ++key)
        { CHECK(!sArchive->find(sOwnerA, key, &record) && !sArchive->remove(sOwnerA, key, record.revision)); }
    }
    else if (sMode == 5)
    {
        struct RhhModEvent nested = {.struct_size = sizeof(nested), .kind = RHH_UI_ACCESSIBILITY_CHANGED};
        struct RhhModDecision nestedDecision = {.struct_size = sizeof(nestedDecision), .allowed = 1};
        sNestedChecked = false; RhhModsDispatch(&nested, &nestedDecision);
        CHECK(sNestedChecked && RhhModsCurrentOwner() == sOwnerA
            && !sArchive->put(sOwnerA, sKey2, 0, "x", 1, &record)
            && !sArchive->remove(sOwnerA, sKey2, record.revision));
    }
    else if (sMode == 6)
    {
        struct RhhModArchiveRecord one = {.struct_size = sizeof(one)}, two = {.struct_size = sizeof(two)};
        CHECK(!sArchive->find(sOwnerA, sKey1, &one) && !sArchive->find(sOwnerA, sKey2, &two));
        struct RhhModArchiveOperation ops[] = {
            {.struct_size = sizeof(ops[0]), .kind = RHH_MOD_ARCHIVE_PUT, .key = sKey1,
             .expected_revision = one.revision, .size = sizeof(sData), .data = sData},
            {.struct_size = sizeof(ops[0]), .kind = RHH_MOD_ARCHIVE_REMOVE, .key = sKey2,
             .expected_revision = two.revision}
        };
        struct RhhModArchiveBatchResult batch = {.struct_size = sizeof(batch)}, before = batch;
        struct RhhModArchiveBatchResult tiny = {.struct_size = 1, .count = 99}, savedTiny = tiny;
        struct RhhModArchiveUsage usage = {.struct_size = sizeof(usage)}, beforeUsage;
        CHECK(!sArchive->usage(sOwnerA, &usage)); beforeUsage = usage;
        CHECK(sArchive->batch(sOwnerA, ops, 0, &batch) && !memcmp(&batch, &before, sizeof(batch))
            && sArchive->batch(sOwnerA, ops, RHH_MOD_ARCHIVE_BATCH_MAX + 1, &batch) && !memcmp(&batch, &before, sizeof(batch))
            && sArchive->batch(sOwnerA, ops, 2, &tiny) && !memcmp(&tiny, &savedTiny, sizeof(tiny)));
        CHECK(sArchive->batch(sOwnerB, ops, 2, &batch) && !memcmp(&batch, &before, sizeof(batch)));
        memset(sData, 'c', sizeof(sData));
        ops[1].expected_revision++;
        CHECK(sArchive->batch(sOwnerA, ops, 2, &batch) && !memcmp(&batch, &before, sizeof(batch))
            && !sArchive->usage(sOwnerA, &usage) && !memcmp(&usage, &beforeUsage, sizeof(usage)));
        u8 *original = malloc(sizeof(sData)); CHECK(original);
        CHECK(!sArchive->copy(sOwnerA, sKey1, one.revision, original, sizeof(sData)));
        for (u32 i = 0; i < sizeof(sData); ++i) CHECK(original[i] == 'b');
        free(original); memset(sData, 'b', sizeof(sData));
        ops[1].expected_revision--; ops[1].key = sKey1;
        CHECK(sArchive->batch(sOwnerA, ops, 2, &batch) && !memcmp(&batch, &before, sizeof(batch)));
        ops[1].key = sKey2; ops[1].data = "invalid";
        CHECK(sArchive->batch(sOwnerA, ops, 2, &batch) && !memcmp(&batch, &before, sizeof(batch)));
        ops[1].data = NULL;
        CHECK(!sArchive->batch(sOwnerA, ops, 2, &batch) && batch.owner == sOwnerA && batch.run_id == 99
            && batch.count == 2 && batch.operations[0].revision + 1 == batch.operations[1].revision
            && batch.revision == batch.operations[1].revision);
        CHECK(sArchive->find(sOwnerA, sKey2, &two) && !sArchive->find(sOwnerA, sKey1, &one)
            && one.revision == batch.operations[0].revision);
        CHECK(!sArchive->put(sOwnerA, sKey2, 0, "two", 3, &record));
        union { struct RhhModArchiveBatchResult result; struct RhhModArchiveOperation operation; } alias = {0};
        alias.result.struct_size = sizeof(alias.result); alias.operation.kind = RHH_MOD_ARCHIVE_PUT;
        alias.operation.key = 10001; alias.operation.size = 1; alias.operation.data = "x";
        CHECK(!sArchive->batch(sOwnerA, &alias.operation, 1, &alias.result)
            && alias.result.count == 1 && alias.result.operations[0].key == 10001);
        CHECK(!sArchive->find(sOwnerA, 10001, &record) && !sArchive->remove(sOwnerA, 10001, record.revision));
    }
    else if (sMode == 7)
    {
        struct RhhModEvent nested = {.struct_size = sizeof(nested), .kind = RHH_WORLD_STEP};
        struct RhhModDecision nestedDecision = {.struct_size = sizeof(nestedDecision), .allowed = 1};
        RhhModsDispatch(&nested, &nestedDecision); CHECK(RhhModsCurrentOwner() == sOwnerA);
    }
    else if (sMode == 9)
    {
        const u32 size = 3 * 1024 * 1024;
        u8 *large = malloc(size); CHECK(large); memset(large, 0x6a, size);
        CHECK(!sArchive->put(sOwnerA, 1000, 0, large, size, &record));
        free(large);
        for (u64 key = 2000; key < 2300; ++key)
            CHECK(!sArchive->put(sOwnerA, key, 0, "x", 1, &record));
    }
}
static void UpdateB(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (sMode == 5 && event->kind == RHH_UI_ACCESSIBILITY_CHANGED)
    {
        struct RhhModArchiveRecord record = {.struct_size = sizeof(record)};
        CHECK(RhhModsCurrentOwner() == sOwnerB && sArchive->put(sOwnerB, sKey2, 0, "x", 1, &record));
        sNestedChecked = true;
    }
    if (sMode == 1)
    {
        struct RhhModArchiveRecord record = {.struct_size = sizeof(record)}, saved = record;
        CHECK(sArchive->put(sOwnerA, sKey2, 0, "x", 1, &record) && !memcmp(&record, &saved, sizeof(record)));
        CHECK(!sArchive->put(sOwnerB, sKey1, 0, "other", 5, &record));
    }
}
static void Committed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    if (sMode != 7 && sMode != 8) return;
    struct RhhModArchiveRecord record = {.struct_size = sizeof(record)}, saved;
    CHECK(!sArchive->find(sOwnerA, sKey1, &record)); saved = record;
    CHECK(sArchive->put(sOwnerB, sKey2, 0, "x", 1, &record) && !memcmp(&record, &saved, sizeof(record)));
    if (sMode == 8) CHECK(sArchive->put(sOwnerA, sKey1, record.revision, sData, sizeof(sData), &record)
        && !memcmp(&record, &saved, sizeof(record)));
    else { CHECK(!sArchive->put(sOwnerA, sKey1, record.revision, sData, sizeof(sData), &record)); ++sCommittedEvents; }
}
static void Preview(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhModArchiveRecord record = {.struct_size = sizeof(record)}, saved = record;
    CHECK(sArchive->put(sOwnerA, 9999, 0, "x", 1, &record) && !memcmp(&record, &saved, sizeof(record)));
    ++sRejectedPreview;
}
static const struct RhhModHook sHookA[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, UpdateA},
    {sizeof(struct RhhModHook), RHH_WORLD_STEP, RHH_OBSERVE, 0, NULL, Committed},
    {sizeof(struct RhhModHook), RHH_MON_EDITED, RHH_OBSERVE, 0, NULL, Committed},
    {sizeof(struct RhhModHook), RHH_BREED_EGG_CREATED, RHH_OBSERVE, 0, NULL, Committed},
    {sizeof(struct RhhModHook), RHH_BATTLE_TURN_ENDED, RHH_OBSERVE, 0, NULL, Committed},
    {sizeof(struct RhhModHook), RHH_PHOTO_CAPTURED, RHH_OBSERVE, 0, NULL, Committed},
    {sizeof(struct RhhModHook), RHH_PHOTO_SCORE, RHH_TRANSFORM, 0, Preview, NULL}
};
static const struct RhhModHook sHookB[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, UpdateB},
    {sizeof(struct RhhModHook), RHH_UI_ACCESSIBILITY_CHANGED, RHH_OBSERVE, 0, NULL, UpdateB}
};
static const struct RhhModDefinition sA = {.struct_size = sizeof(sA), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:archive_a", .hook_count = sizeof(sHookA) / sizeof(sHookA[0]), .hooks = sHookA};
static const struct RhhModDefinition sB = {.struct_size = sizeof(sB), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:archive_b", .hook_count = 2, .hooks = sHookB};
static void Update(int mode)
{
    sMode = mode;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision); sMode = 0;
}
static void DispatchEvent(u32 kind, int mode)
{
    sMode = mode;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision); sMode = 0;
}
static void Setup(bool missing)
{
    RhhModsInit(); CHECK(!RhhModsInstallArchiveService());
    if (!missing) CHECK(!RhhModsRegister(&sA));
    CHECK(!RhhModsRegister(&sB) && !RhhModsFinalize());
    sArchive = (const void *)RhhModsHostAPI()->get_service("rhh:service/mod_archive", RHH_SERVICE_VERSION(1, 0));
    CHECK(sArchive && sArchive->header.struct_size >= RHH_MOD_ARCHIVE_SERVICE_V1_1_SIZE
        && RHH_MOD_ARCHIVE_SERVICE_V1_0_SIZE < RHH_MOD_ARCHIVE_SERVICE_V1_1_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/mod_archive", RHH_SERVICE_VERSION(1, 1)) == &sArchive->header
        && RhhModsHostAPI()->get_service("rhh:service/mod_archive", RHH_SERVICE_VERSION(1, 2)) == &sArchive->header
        && RhhModsHostAPI()->get_service("rhh:service/mod_archive", RHH_SERVICE_VERSION(1, 3)) == &sArchive->header
        && !RhhModsHostAPI()->get_service("rhh:service/mod_archive", RHH_SERVICE_VERSION(1, 4)));
    sOwnerA = RhhContentKeyOf(sA.id); sOwnerB = RhhContentKeyOf(sB.id);
    sKey1 = RhhContentKeyOf("test:archive/one"); sKey2 = RhhContentKeyOf("test:archive/two");
}
static void CheckA(char value, bool second)
{
    struct RhhModArchiveRecord record = {.struct_size = sizeof(record)};
    struct RhhModArchiveUsage usage = {.struct_size = sizeof(usage)};
    CHECK(!sArchive->usage(sOwnerA, &usage) && usage.record_count == (second ? 2u : 1u)
        && usage.owner_bytes == sizeof(sData) + (second ? 3u : 0u));
    CHECK(!sArchive->find(sOwnerA, sKey1, &record) && record.size == sizeof(sData));
    memset(sData, 0x5a, sizeof(sData));
    CHECK(sArchive->copy(sOwnerA, sKey1, record.revision, sData, sizeof(sData) - 1) && sData[0] == 0x5a
        && sArchive->copy(sOwnerA, sKey1, record.revision + 1, sData, sizeof(sData)) && sData[0] == 0x5a);
    CHECK(!sArchive->copy(sOwnerA, sKey1, record.revision, sData, sizeof(sData)));
    for (u32 i = 0; i < sizeof(sData); ++i) CHECK(sData[i] == value);
    CHECK((!sArchive->find(sOwnerA, sKey2, &record)) == second);
    struct RhhModArchiveRecord first = {.struct_size = sizeof(first)}, next = {.struct_size = sizeof(next)};
    CHECK(!sArchive->at(sOwnerA, 0, &first));
    if (second) CHECK(!sArchive->at(sOwnerA, 1, &next) && first.key < next.key);
    CHECK(sArchive->at(sOwnerA, second ? 2 : 1, &next));
}
static void Save(u32 generation)
{ CHECK(!NativeSaveBegin() && !ProgramFlashByte(0, 5, generation) && !NativeSaveEnd(generation, 1)); }
int main(int argc, char **argv)
{
    CHECK(argc == 3); bool missing = !strcmp(argv[1], "missing"); Setup(missing);
    if (!strcmp(argv[1], "reject")) { CHECK(NativeSaveOpen(argv[2])); NativeSaveClose(); RhhModsShutdown(); return 0; }
    CHECK(!NativeSaveOpen(argv[2]));
    struct RhhModArchiveUsage usage = {.struct_size = sizeof(usage)}, unchanged = usage;
    CHECK(sArchive->usage(sOwnerB, &usage) && !memcmp(&usage, &unchanged, sizeof(usage)));
    RhhModsSetRunContext(123, 99, 0);
    if (!strcmp(argv[1], "write"))
    {
        struct RhhModArchiveRecord record = {.struct_size = sizeof(record)}, saved = record;
        CHECK(sArchive->put(sOwnerA, sKey1, 0, "x", 1, &record) && !memcmp(&record, &saved, sizeof(record)));
        Update(1); CheckA('a', FALSE); Update(4); Update(5); CheckA('a', FALSE);
        sCommittedEvents = sRejectedPreview = 0; Update(7);
        DispatchEvent(RHH_MON_EDITED, 7); DispatchEvent(RHH_BREED_EGG_CREATED, 7);
        DispatchEvent(RHH_BATTLE_TURN_ENDED, 7); DispatchEvent(RHH_PHOTO_CAPTURED, 7);
        CHECK(sCommittedEvents == 5); DispatchEvent(RHH_PHOTO_SCORE, 7); CHECK(sRejectedPreview == 1);
        RhhModsSetDrawing(1); DispatchEvent(RHH_PHOTO_CAPTURED, 8); RhhModsSetDrawing(0);
        CheckA('a', FALSE);
        RhhModsSetRunContext(123, 100, 0); CHECK(sArchive->usage(sOwnerA, &usage));
        RhhModsSetRunContext(123, 99, 0); CheckA('a', FALSE); Save(1);
        CHECK(!NativeSaveBegin()); Update(3); DispatchEvent(RHH_PHOTO_CAPTURED, 8); CHECK(NativeSaveEnd(2, 0));
        Update(2); CheckA('b', TRUE); Update(6); CheckA('b', TRUE); Save(2);
        CHECK(!NativeSaveRestoreMods(1)); CheckA('a', FALSE);
        CHECK(!NativeSaveRestoreMods(2)); CheckA('b', TRUE);
    }
    else if (missing)
    {
        CHECK(sArchive->usage(sOwnerA, &usage));
        CHECK(!sArchive->usage(sOwnerB, &usage) && usage.record_count == 1);
        Save(3);
    }
    else if (!strcmp(argv[1], "read"))
    { CheckA('b', TRUE); CHECK(!NativeSaveRestoreMods(2)); CheckA('b', TRUE); }
    else if (!strcmp(argv[1], "grow")) { Update(9); Save(4); }
    else if (!strcmp(argv[1], "read-large"))
    {
        struct RhhModArchiveRecord record = {.struct_size = sizeof(record)};
        CHECK(!sArchive->find(sOwnerA, 1000, &record) && record.size == 3 * 1024 * 1024);
        u8 *large = malloc(record.size); CHECK(large);
        CHECK(!sArchive->copy(sOwnerA, 1000, record.revision, large, record.size)
            && large[0] == 0x6a && large[record.size - 1] == 0x6a); free(large);
        for (u64 key = 2000; key < 2300; ++key) CHECK(!sArchive->find(sOwnerA, key, &record));
    }
    else CHECK(0);
    NativeSaveClose(); RhhModsShutdown(); puts("mod archive: growing owner records and save generations passed"); return 0;
}
