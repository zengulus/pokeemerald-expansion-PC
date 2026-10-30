#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#ifdef _WIN32
#define NOMINMAX
#include <io.h>
#include <windows.h>
#undef TRANSPARENT
#else
#include <unistd.h>
#endif
#include "global.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "gba/flash_internal.h"
#include "native.h"
#include "mod_host.h"
#include "pc_save_format.h"

static char *sSavePath;
enum { ARCHIVE_HEADER = 16, ARCHIVE_RECORD = 68 };
struct ModSnapshot { u32 generation, size, archiveSize; u8 *data, *archive; };
static struct ModSnapshot sSnapshots[2], sPending;
struct GameSnapshot { u8 *data; size_t size; };
static struct GameSnapshot sGameSnapshots[2], sPendingGame;
static struct GameSnapshot sSpecial[3], sSpecialBackup[3];
static const struct NativeSaveArchiveCodec **sArchiveCodecs;
static u32 sArchiveCount;
static u8 *sJournal, *sPreviousJournal, *sFlashBackup;
static u32 sJournalSize, sPreviousJournalSize;
static u64 sJournalId, sPreviousJournalId, sRunSerial;
static bool sTransaction;
static bool sTransactionInvalid;
static int (*sCommitGuard)(void);
void NativeSaveSetCommitGuard(int (*guard)(void)) { sCommitGuard = guard; }
static bool sArchiveSaveExport;
int NativeSaveArchiveSaveExportActive(void) { return sArchiveSaveExport; }

static u32 GetWord(const u8 *p)
{
    return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}
static u64 GetLong(const u8 *p) { return GetWord(p) | (u64)GetWord(p + 4) << 32; }
static u32 ExtensionChecksum(const u8 *p, u32 size)
{
    u32 hash = 2166136261u;
    for (u32 i = 0; i < size; ++i) hash = (hash ^ p[i]) * 16777619u;
    return hash;
}
static void ClearModSave(void)
{
    for (unsigned i = 0; i < 2; ++i) { free(sSnapshots[i].data); free(sSnapshots[i].archive); }
    for (unsigned i = 0; i < 2; ++i) free(sGameSnapshots[i].data);
    memset(sGameSnapshots, 0, sizeof(sGameSnapshots));
    free(sPendingGame.data); memset(&sPendingGame, 0, sizeof(sPendingGame));
    for (unsigned i = 0; i < 3; ++i) { free(sSpecial[i].data); free(sSpecialBackup[i].data); }
    memset(sSpecial, 0, sizeof(sSpecial)); memset(sSpecialBackup, 0, sizeof(sSpecialBackup));
    memset(sSnapshots, 0, sizeof(sSnapshots));
    free(sPending.data); free(sPending.archive); memset(&sPending, 0, sizeof(sPending));
    free(sJournal); sJournal = NULL; sJournalSize = 0;
    free(sPreviousJournal); sPreviousJournal = NULL; sPreviousJournalSize = 0;
    sJournalId = sPreviousJournalId = sRunSerial = 0;
    free(sFlashBackup); sFlashBackup = NULL; sTransaction = sTransactionInvalid = false;
}

static void PutHeaderWord(u8 *dest, u32 value)
{
    for (unsigned i = 0; i < 4; ++i) dest[i] = value >> (i * 8);
}
static void PutLong(u8 *p, u64 n) { PutHeaderWord(p, n); PutHeaderWord(p + 4, n >> 32); }

static int ValidateGameSnapshot(const u8 *data, size_t size)
{
    if (!data || size < 40 || memcmp(data, "RHHGAME1", 8)) return -1;
    size_t at = 40;
    for (unsigned i = 0; i < 4; ++i)
    {
        u64 length = GetLong(data + 8 + i * 8);
        if (!length || length > size - at) return -1;
        at += (size_t)length;
    }
    return at == size ? 0 : -1;
}

int NativeSaveStageGame(const void *const regions[4], const size_t sizes[4], int retainStorage)
{
    if (!sTransaction || !regions || !sizes || sPendingGame.data) return -1;
    const void *storage = regions[2];
    if (retainStorage && sGameSnapshots[0].data)
    {
        const struct GameSnapshot *previous = &sGameSnapshots[0];
        if (ValidateGameSnapshot(previous->data, previous->size)) return -1;
        size_t pos = 40;
        for (unsigned i = 0; i <= 2; ++i)
        {
            u64 length = GetLong(previous->data + 8 + i * 8);
            if (length > previous->size - pos) return -1;
            if (i == 2)
            {
                if (length != sizes[2]) return -1;
                storage = previous->data + pos;
            }
            pos += (size_t)length;
        }
    }
    size_t total = 40;
    for (unsigned i = 0; i < 4; ++i)
    {
        if (!regions[i] || !sizes[i] || sizes[i] > SIZE_MAX - total) return -1;
        total += sizes[i];
    }
    u8 *data = malloc(total);
    if (!data) return -1;
    memcpy(data, "RHHGAME1", 8);
    size_t at = 40;
    for (unsigned i = 0; i < 4; ++i)
    {
        PutLong(data + 8 + i * 8, sizes[i]);
        memcpy(data + at, i == 2 ? storage : regions[i], sizes[i]); at += sizes[i];
    }
    sPendingGame = (struct GameSnapshot){data, total};
    return 0;
}

int NativeSaveLoadGame(void *const regions[4], const size_t sizes[4], uint32_t *generation)
{
    if (!regions || !sizes || !generation || !sGameSnapshots[0].data) return -1;
    const struct GameSnapshot *game = &sGameSnapshots[0];
    if (ValidateGameSnapshot(game->data, game->size)) return -1;
    size_t at = 40;
    for (unsigned i = 0; i < 4; ++i)
    {
        u64 length = GetLong(game->data + 8 + i * 8);
        if (!regions[i] || length != sizes[i] || length > game->size - at) return -1;
        at += (size_t)length;
    }
    if (at != game->size || NativeSaveRestoreMods(sSnapshots[0].generation)) return -1;
    at = 40;
    for (unsigned i = 0; i < 4; ++i)
    { memcpy(regions[i], game->data + at, sizes[i]); at += sizes[i]; }
    *generation = sSnapshots[0].generation;
    return 0;
}

int NativeSaveReadGameRegion(uint32_t region, size_t offset, void *out, size_t size)
{
    const struct GameSnapshot *game = &sGameSnapshots[0];
    if (region >= 4 || !out || ValidateGameSnapshot(game->data, game->size)) return -1;
    size_t at = 40;
    for (unsigned i = 0; i < 4; ++i)
    {
        u64 length = GetLong(game->data + 8 + i * 8);
        if (length > game->size - at) return -1;
        if (i == region)
        {
            if (offset > length || size > length - offset) return -1;
            memcpy(out, game->data + at + offset, size); return 0;
        }
        at += (size_t)length;
    }
    return -1;
}

int NativeSaveGameAvailable(void) { return !!sGameSnapshots[0].data; }

static int SpecialIndex(uint32_t key)
{
    if (key == SECTOR_ID_HOF_1) return 0;
    if (key == SECTOR_ID_TRAINER_HILL) return 1;
    if (key == SECTOR_ID_RECORDED_BATTLE) return 2;
    return -1;
}

int NativeSaveSpecialSize(uint32_t key, size_t *size)
{
    int index = SpecialIndex(key);
    if (index < 0 || !size) return -1;
    *size = sSpecial[index].size;
    return sSpecial[index].data ? 0 : 1;
}

int NativeSaveSpecialRead(uint32_t key, void *out, size_t size)
{
    int index = SpecialIndex(key);
    if (index < 0 || !out || !size) return -1;
    if (!sSpecial[index].data) return 1;
    if (sSpecial[index].size != size) return -1;
    memcpy(out, sSpecial[index].data, size); return 0;
}

int NativeSaveSpecialWrite(uint32_t key, const void *data, size_t size)
{
    int index = SpecialIndex(key);
    if (index < 0 || (!!data != !!size)) return -1;
    u8 *copy = size ? malloc(size) : NULL;
    if (size && !copy) return -1;
    if (size) memcpy(copy, data, size);
    struct GameSnapshot previous = sSpecial[index];
    sSpecial[index] = (struct GameSnapshot){copy, size};
    if (NativeSaveCommit())
    {
        sSpecial[index] = previous; free(copy); return -1;
    }
    free(previous.data); return 0;
}

int NativeSaveEraseGame(void)
{
    if (sTransaction || !sSavePath) return -1;
    u8 *flash = malloc(sizeof(gNativeFlash));
    if (!flash) return -1;
    memcpy(flash, gNativeFlash, sizeof(gNativeFlash));
    struct ModSnapshot snapshots[2] = {sSnapshots[0], sSnapshots[1]};
    struct GameSnapshot games[2] = {sGameSnapshots[0], sGameSnapshots[1]};
    struct GameSnapshot special[3] = {sSpecial[0], sSpecial[1], sSpecial[2]};
    u8 *journal = sJournal, *previousJournal = sPreviousJournal;
    u32 journalSize = sJournalSize, previousJournalSize = sPreviousJournalSize;
    u64 journalId = sJournalId, previousJournalId = sPreviousJournalId, runSerial = sRunSerial;
    memset(sSnapshots, 0, sizeof(sSnapshots)); memset(sGameSnapshots, 0, sizeof(sGameSnapshots));
    memset(sSpecial, 0, sizeof(sSpecial));
    sJournal = sPreviousJournal = NULL; sJournalSize = sPreviousJournalSize = 0;
    sJournalId = sPreviousJournalId = sRunSerial = 0;
    memset(gNativeFlash, 0xff, sizeof(gNativeFlash));
    if (NativeSaveCommit())
    {
        memcpy(gNativeFlash, flash, sizeof(gNativeFlash));
        memcpy(sSnapshots, snapshots, sizeof(snapshots)); memcpy(sGameSnapshots, games, sizeof(games));
        memcpy(sSpecial, special, sizeof(special));
        sJournal = journal; sPreviousJournal = previousJournal;
        sJournalSize = journalSize; sPreviousJournalSize = previousJournalSize;
        sJournalId = journalId; sPreviousJournalId = previousJournalId; sRunSerial = runSerial;
        free(flash); return -1;
    }
    for (unsigned i = 0; i < 2; ++i)
    { free(snapshots[i].data); free(snapshots[i].archive); free(games[i].data); }
    for (unsigned i = 0; i < 3; ++i) free(special[i].data);
    free(journal); free(previousJournal); free(flash);
    NativeSaveResetArchive();
    return 0;
}

struct ArchivePart { const u8 *data; u32 size; };
static int ArchiveCodecIndex(const char *id)
{
    for (u32 i = 0; i < sArchiveCount; ++i) if (!strcmp(id, sArchiveCodecs[i]->id)) return i;
    return -1;
}
/* Parse and validate every part against the corresponding mod snapshot. */
static int ReadArchives(const u8 *data, u32 size, struct ArchivePart *parts)
{
    memset(parts, 0, sArchiveCount * sizeof(*parts));
    if (!size) { if (data) return -1; }
    else if (!data || size < 8) return -1;
    else if (memcmp(data, "RHHARC01", 8))
    {
        int match = -1;
        for (u32 i = 0; i < sArchiveCount; ++i)
            if (sArchiveCodecs[i]->legacy_magic && !memcmp(data, sArchiveCodecs[i]->legacy_magic, 8)) match = i;
        if (match < 0) return -1;
        parts[match] = (struct ArchivePart){data, size};
    }
    else
    {
        if (size < ARCHIVE_HEADER || GetWord(data + 8) != size || !GetWord(data + 12)
            || GetWord(data + 12) > sArchiveCount) return -1;
        u32 at = ARCHIVE_HEADER; const char *previous = "";
        for (u32 n = 0; n < GetWord(data + 12); ++n)
        {
            if (size - at < ARCHIVE_RECORD || !memchr(data + at, 0, 64)) return -1;
            const char *id = (const char *)data + at; size_t idSize = strlen(id);
            if (!idSize || strcmp(previous, id) >= 0) return -1;
            for (u32 i = idSize + 1; i < 64; ++i) if (data[at + i]) return -1;
            int codec = ArchiveCodecIndex(id); u32 length = GetWord(data + at + 64);
            at += ARCHIVE_RECORD;
            if (codec < 0 || !length || length > size - at || parts[codec].data) return -1;
            parts[codec] = (struct ArchivePart){data + at, length};
            previous = id; at += length;
        }
        if (at != size) return -1;
    }
    for (u32 i = 0; i < sArchiveCount; ++i)
        if (sArchiveCodecs[i]->validate(parts[i].data, parts[i].size)) return -1;
    return 0;
}
static int ImportArchives(const u8 *data, u32 size)
{
    struct ArchivePart *parts = calloc(sArchiveCount ? sArchiveCount : 1, sizeof(*parts));
    if (!parts) return -1;
    if (ReadArchives(data, size, parts)) { free(parts); return -1; }
    for (u32 i = 0; i < sArchiveCount; ++i)
        if (sArchiveCodecs[i]->import_data(parts[i].data, parts[i].size)) { free(parts); return -1; }
    free(parts); return 0;
}
static int ExportArchives(u8 **data, u32 *size)
{
    u8 **buffers = calloc(sArchiveCount ? sArchiveCount : 1, sizeof(*buffers));
    u32 *lengths = calloc(sArchiveCount ? sArchiveCount : 1, sizeof(*lengths));
    u32 count = 0, total = ARCHIVE_HEADER;
    if (!buffers || !lengths) { free(buffers); free(lengths); return -1; }
    int result = -1; *data = NULL; *size = 0;
    for (u32 i = 0; i < sArchiveCount; ++i)
    {
        if (sArchiveCodecs[i]->export_data(&buffers[i], &lengths[i])
            || (!!buffers[i] != !!lengths[i]) || sArchiveCodecs[i]->validate(buffers[i], lengths[i])) goto cleanup;
        if (lengths[i])
        {
            if (total > UINT32_MAX - ARCHIVE_RECORD || lengths[i] > UINT32_MAX - ARCHIVE_RECORD - total) goto cleanup;
            total += ARCHIVE_RECORD + lengths[i]; ++count;
        }
    }
    if (!count) { result = 0; goto cleanup; }
    if (count == 1)
        for (u32 i = 0; i < sArchiveCount; ++i)
            if (lengths[i] && sArchiveCodecs[i]->legacy_magic && lengths[i] >= 8
                && !memcmp(buffers[i], sArchiveCodecs[i]->legacy_magic, 8))
            { *data = buffers[i]; *size = lengths[i]; buffers[i] = NULL; result = 0; goto cleanup; }
    u8 *output = calloc(total, 1); if (!output) goto cleanup;
    memcpy(output, "RHHARC01", 8); PutHeaderWord(output + 8, total); PutHeaderWord(output + 12, count);
    u32 at = ARCHIVE_HEADER; const char *previous = "";
    for (u32 n = 0; n < count; ++n)
    {
        int best = -1;
        for (u32 i = 0; i < sArchiveCount; ++i)
            if (lengths[i] && strcmp(sArchiveCodecs[i]->id, previous) > 0
                && (best < 0 || strcmp(sArchiveCodecs[i]->id, sArchiveCodecs[best]->id) < 0)) best = i;
        if (best < 0) { free(output); goto cleanup; }
        strcpy((char *)output + at, sArchiveCodecs[best]->id);
        PutHeaderWord(output + at + 64, lengths[best]); at += ARCHIVE_RECORD;
        memcpy(output + at, buffers[best], lengths[best]); at += lengths[best];
        previous = sArchiveCodecs[best]->id;
    }
    *data = output; *size = total; result = 0;
cleanup:
    for (u32 i = 0; i < sArchiveCount; ++i) free(buffers[i]);
    free(buffers); free(lengths);
    return result;
}

static u32 SaveLayoutSignature(void)
{
    // Detect layout/configuration changes before handing bytes to RHH's save
    // reader. Semantic format changes also require a version bump above.
    const u32 layout[] = {
        sizeof(struct SaveBlock1), sizeof(struct SaveBlock2), sizeof(struct SaveBlock3),
        sizeof(struct PokemonStorage), sizeof(struct Pokemon), sizeof(struct BoxPokemon),
        sizeof(struct ObjectEventTemplate), sizeof(struct EnigmaBerry),
        offsetof(struct SaveBlock1, objectEventTemplates),
        offsetof(struct SaveBlock1, flags), offsetof(struct SaveBlock1, vars),
        offsetof(struct SaveBlock1, enigmaBerry), offsetof(struct SaveBlock2, encryptionKey)
    };
    u32 hash = 2166136261u;
    for (unsigned i = 0; i < ARRAY_COUNT(layout); ++i)
        for (unsigned byte = 0; byte < 4; ++byte)
            hash = (hash ^ ((layout[i] >> (byte * 8)) & 255)) * 16777619u;
    return hash;
}

static int EncodeExtension(u8 **data, u32 *size)
{
    *data = NULL; *size = 0;
    if (!sSnapshots[0].data && !sJournalSize && !sPreviousJournalSize) return 0;
    bool archive = sSnapshots[0].archiveSize || sSnapshots[1].archiveSize;
    u64 total64 = (archive ? 80u : 64u) + (u64)sSnapshots[0].size + sSnapshots[1].size
        + sSnapshots[0].archiveSize + sSnapshots[1].archiveSize + sJournalSize + sPreviousJournalSize;
    if (total64 > UINT32_MAX) return -1;
    u32 total = total64;
    u8 *buffer = calloc(total, 1);
    if (!buffer) return -1;
    memcpy(buffer, archive ? "RHHEX004" : "RHHEX003", 8);
    PutHeaderWord(buffer + 8, total);
    PutHeaderWord(buffer + 16, sJournalSize);
    PutHeaderWord(buffer + 20, sPreviousJournalSize);
    PutLong(buffer + 40, sRunSerial);
    PutLong(buffer + 48, sJournalId);
    PutLong(buffer + 56, sPreviousJournalId);
    if (archive)
    {
        PutHeaderWord(buffer + 64, sSnapshots[0].archiveSize);
        PutHeaderWord(buffer + 68, sSnapshots[1].archiveSize);
    }
    u32 at = archive ? 80 : 64;
    for (unsigned i = 0; i < 2; ++i)
    {
        PutHeaderWord(buffer + 24 + i * 8, sSnapshots[i].generation);
        PutHeaderWord(buffer + 28 + i * 8, sSnapshots[i].size);
        if (sSnapshots[i].size) memcpy(buffer + at, sSnapshots[i].data, sSnapshots[i].size);
        at += sSnapshots[i].size;
    }
    for (unsigned i = 0; i < 2; ++i)
    {
        if (sSnapshots[i].archiveSize) memcpy(buffer + at, sSnapshots[i].archive, sSnapshots[i].archiveSize);
        at += sSnapshots[i].archiveSize;
    }
    if (sJournalSize) memcpy(buffer + at, sJournal, sJournalSize);
    at += sJournalSize;
    if (sPreviousJournalSize) memcpy(buffer + at, sPreviousJournal, sPreviousJournalSize);
    PutHeaderWord(buffer + 12, ExtensionChecksum(buffer + 16, total - 16));
    *data = buffer; *size = total;
    return 0;
}
static int DecodeExtension(const u8 *data, u32 size)
{
    bool oldFormat = size >= 40 && !memcmp(data, "RHHEX002", 8);
    bool hasArchive = size >= 80 && !memcmp(data, "RHHEX004", 8);
    if (size < (oldFormat ? 40 : hasArchive ? 80 : 64)
        || (!oldFormat && !hasArchive && memcmp(data, "RHHEX003", 8))
        || GetWord(data + 8) != size || (oldFormat && GetWord(data + 20))
        || (hasArchive && (GetWord(data + 72) || GetWord(data + 76)))
        || ExtensionChecksum(data + 16, size - 16) != GetWord(data + 12)) return -1;
    u32 at = oldFormat ? 40 : hasArchive ? 80 : 64;
    for (unsigned i = 0; i < 2; ++i)
    {
        u32 length = GetWord(data + 28 + i * 8);
        if (length > size - at) return -1;
        if (length)
        {
            /* Validate every retained generation, including required modules
             * and migrations, before accepting a writable save path. */
            if (RhhModsImport(data + at, length)) return -1;
            sSnapshots[i].data = malloc(length);
            if (!sSnapshots[i].data) return -1;
            memcpy(sSnapshots[i].data, data + at, length);
            sSnapshots[i].size = length;
            sSnapshots[i].generation = GetWord(data + 24 + i * 8);
        }
        at += length;
    }
    for (unsigned i = 0; i < 2; ++i)
    {
        u32 length = hasArchive ? GetWord(data + 64 + i * 4) : 0;
        if (length > size - at || (length && !sSnapshots[i].data)) return -1;
        if (sSnapshots[i].data)
        {
            if (RhhModsImport(sSnapshots[i].data, sSnapshots[i].size)) return -1;
            struct ArchivePart *parts = calloc(sArchiveCount ? sArchiveCount : 1, sizeof(*parts));
            if (!parts) return -1;
            int invalid = ReadArchives(length ? data + at : NULL, length, parts);
            free(parts);
            if (invalid)
            {
                fprintf(stderr, "Invalid or unavailable host archive in save generation %u\n", sSnapshots[i].generation);
                return -1;
            }
        }
        if (length)
        {
            sSnapshots[i].archive = malloc(length);
            if (!sSnapshots[i].archive) return -1;
            memcpy(sSnapshots[i].archive, data + at, length); sSnapshots[i].archiveSize = length;
        }
        at += length;
    }
    sJournalSize = GetWord(data + 16);
    sPreviousJournalSize = oldFormat ? 0 : GetWord(data + 20);
    if (sJournalSize > size - at || sPreviousJournalSize != size - at - sJournalSize) return -1;
    if (sJournalSize)
    {
        sJournal = malloc(sJournalSize);
        if (!sJournal) return -1;
        memcpy(sJournal, data + at, sJournalSize);
    }
    at += sJournalSize;
    if (sPreviousJournalSize)
    {
        sPreviousJournal = malloc(sPreviousJournalSize);
        if (!sPreviousJournal) return -1;
        memcpy(sPreviousJournal, data + at, sPreviousJournalSize);
    }
    sRunSerial = oldFormat ? 0 : GetLong(data + 40);
    sJournalId = oldFormat ? (sJournalSize >= 40 ? GetLong(sJournal + 32) : 0) : GetLong(data + 48);
    sPreviousJournalId = oldFormat ? 0 : GetLong(data + 56);
    if (sSnapshots[0].data && RhhModsImport(sSnapshots[0].data, sSnapshots[0].size)) return -1;
    if (ImportArchives(sSnapshots[0].archive, sSnapshots[0].archiveSize)) return -1;
    return 0;
}

/* Independent game generations plus length-delimited special records. */
enum { PC_SHARED_HEADER = 40 };
static const u8 sEmptyGame[] = "RHHNOGM0";

static int EncodePcSave(u8 **out, size_t *outSize)
{
    u8 *extension = NULL; u32 extensionSize = 0;
    if (EncodeExtension(&extension, &extensionSize)) return -1;
    size_t sharedSize = PC_SHARED_HEADER + (size_t)extensionSize;
    for (unsigned i = 0; i < 3; ++i)
    {
        if (sSpecial[i].size > SIZE_MAX - sharedSize) { free(extension); return -1; }
        sharedSize += sSpecial[i].size;
    }
    u8 *shared = calloc(sharedSize, 1);
    if (!shared) { free(extension); return -1; }
    PutHeaderWord(shared, extensionSize);
    PutHeaderWord(shared + 4, sizeof(void *) * 8);
    PutHeaderWord(shared + 8, SaveLayoutSignature());
    size_t at = PC_SHARED_HEADER;
    for (unsigned i = 0; i < 3; ++i)
    {
        PutLong(shared + 16 + i * 8, sSpecial[i].size);
        if (sSpecial[i].size) memcpy(shared + at, sSpecial[i].data, sSpecial[i].size);
        at += sSpecial[i].size;
    }
    if (extensionSize) memcpy(shared + at, extension, extensionSize);
    struct RhhPcSaveImage image = {.shared = {shared, sharedSize}};
    for (unsigned i = 0; i < 2; ++i)
    {
        if (!sSnapshots[i].data) continue;
        image.slots[i].generation = sSnapshots[i].generation;
        image.slots[i].payload.data = sGameSnapshots[i].data ? sGameSnapshots[i].data : sEmptyGame;
        image.slots[i].payload.size = sGameSnapshots[i].data ? sGameSnapshots[i].size : sizeof(sEmptyGame) - 1;
    }
    int result = RhhPcSaveEncode(&image, out, outSize);
    free(shared); free(extension); return result;
}

static int DecodePcSave(const u8 *bytes, size_t size)
{
    struct RhhPcSaveDecoded decoded;
    if (RhhPcSaveDecode(bytes, size, &decoded)) return -1;
    const struct RhhPcSaveBlob shared = decoded.image.shared;
    if (shared.size < PC_SHARED_HEADER
        || GetWord(shared.data + 4) != sizeof(void *) * 8
        || GetWord(shared.data + 8) != SaveLayoutSignature() || GetWord(shared.data + 12)) return -1;
    size_t at = PC_SHARED_HEADER;
    for (unsigned i = 0; i < 3; ++i)
    {
        u64 length = GetLong(shared.data + 16 + i * 8);
        if (length > shared.size - at) return -1;
        if (length)
        {
            sSpecial[i].data = malloc((size_t)length);
            if (!sSpecial[i].data) return -1;
            sSpecial[i].size = (size_t)length;
            memcpy(sSpecial[i].data, shared.data + at, (size_t)length);
        }
        at += (size_t)length;
    }
    u32 extensionSize = GetWord(shared.data);
    if (extensionSize != shared.size - at
        || (extensionSize && DecodeExtension(shared.data + at, extensionSize))) return -1;
    unsigned usable = 0, preferred = 2;
    for (unsigned i = 0; i < 2; ++i)
    {
        if (!(decoded.valid_slots & (1u << i))) continue;
        const struct RhhPcSaveGeneration *slot = &decoded.image.slots[i];
        unsigned match;
        for (match = 0; match < 2; ++match)
            if (sSnapshots[match].data && slot->generation == sSnapshots[match].generation) break;
        if (match == 2) continue;
        if (slot->payload.size == sizeof(sEmptyGame) - 1
            && !memcmp(slot->payload.data, sEmptyGame, sizeof(sEmptyGame) - 1))
        { usable |= 1u << match; if (preferred == 2) preferred = match; continue; }
        if (ValidateGameSnapshot(slot->payload.data, slot->payload.size))
            continue;
        u8 *copy = malloc(slot->payload.size);
        if (!copy) return -1;
        memcpy(copy, slot->payload.data, slot->payload.size);
        sGameSnapshots[match] = (struct GameSnapshot){copy, slot->payload.size};
        usable |= 1u << match;
        if (preferred == 2) preferred = match;
    }
    if (preferred == 1)
    {
        /* Select the previous game and its matching mod state as one unit. */
        if (NativeSaveRestoreMods(sSnapshots[1].generation)) return -1;
    }
    else if (!usable && sSnapshots[0].data) return -1;
    return 0;
}
static const struct FlashType sFlashType = {
    .romSize = 131072, .sector = {.size = 4096, .shift = 12, .count = 32}
};
const struct FlashType *gFlash = &sFlashType;
const u16 *gFlashMaxTime;
u16 gFlashNumRemainingBytes;
u8 gFlashTimeoutFlag;
u8 (*PollFlashStatus)(u8 *);
u16 (*WaitForFlashWrite)(u8, u8 *, u8);

int NativeSaveOpen(const char *path)
{
    FILE *file;
    free(sSavePath);
    sSavePath = NULL;
    ClearModSave();
    NativeSaveResetArchive();
    RhhModsResetState();
    memset(gNativeFlash, 0xff, sizeof(gNativeFlash));
    file = fopen(path, "rb");
    if (file)
    {
        size_t capacity = 65536, size = 0;
        u8 *bytes = malloc(capacity);
        if (!bytes) { fclose(file); return -1; }
        bool failed = false;
        for (;;)
        {
            if (size == capacity)
            {
                if (capacity > SIZE_MAX / 2) { failed = true; break; }
                size_t next = capacity * 2;
                u8 *larger = realloc(bytes, next);
                if (!larger) { failed = true; break; }
                bytes = larger; capacity = next;
            }
            size_t count = fread(bytes + size, 1, capacity - size, file);
            size += count;
            if (!count) { failed = !!ferror(file); break; }
        }
        if (fclose(file)) failed = true;
        if (!failed && DecodePcSave(bytes, size)) failed = true;
        free(bytes);
        if (failed)
        {
            fprintf(stderr, "Invalid or incompatible PC save: %s\n", path);
            ClearModSave();
            NativeSaveResetArchive(); RhhModsResetState();
            return -1;
        }
    }
    else if (errno != ENOENT)
    {
        perror(path);
        return -1;
    }
    sSavePath = malloc(strlen(path) + 1);
    if (!sSavePath) return -1;
    strcpy(sSavePath, path);
    return 0;
}

int NativeSaveCommit(void)
{
    if (sTransaction) return 0;
    if (!sSavePath) return -1;
    u8 *data; size_t size;
    if (EncodePcSave(&data, &size)) return -1;
    char *temporary = malloc(strlen(sSavePath) + 5);
    if (!temporary) { free(data); return -1; }
    sprintf(temporary, "%s.tmp", sSavePath);
    FILE *file = fopen(temporary, "wb");
    int result = -1;
    if (file)
    {
        int ok = fwrite(data, 1, size, file) == size;
        if (fflush(file)) ok = 0;
#ifdef _WIN32
        if (_commit(_fileno(file))) ok = 0;
#else
        if (fsync(fileno(file))) ok = 0;
#endif
        if (fclose(file)) ok = 0;
        if (ok)
        {
#ifdef _WIN32
            result = MoveFileExA(temporary, sSavePath,
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
#else
            result = rename(temporary, sSavePath);
#endif
        }
    }
    if (result) perror("Native save commit");
    free(temporary);
    free(data);
    return result;
}

void NativeSaveClose(void)
{
    ClearModSave();
    NativeSaveResetArchive();
    free(sSavePath);
    sSavePath = NULL;
}

int NativeSaveTransactionActive(void) { return sTransaction; }
void NativeSaveInvalidateTransaction(void) { if (sTransaction) sTransactionInvalid = true; }
int NativeSaveRegisterArchive(const struct NativeSaveArchiveCodec *codec)
{
    if (!codec || !codec->export_data || !codec->validate || !codec->import_data || !codec->reset
        || !codec->id || !*codec->id || strlen(codec->id) >= 64
        || (codec->legacy_magic && (strlen(codec->legacy_magic) != 8 || !memcmp(codec->legacy_magic, "RHHARC01", 8)))) return -1;
    for (const char *p = codec->id; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')
            || *p == ':' || *p == '/' || *p == '-' || *p == '_' || *p == '.')) return -1;
    for (u32 i = 0; i < sArchiveCount; ++i)
    {
        if (sArchiveCodecs[i] == codec) return 0;
        if (!strcmp(sArchiveCodecs[i]->id, codec->id)
            || (codec->legacy_magic && sArchiveCodecs[i]->legacy_magic
                && !memcmp(codec->legacy_magic, sArchiveCodecs[i]->legacy_magic, 8))) return -1;
    }
    if (sSavePath || sTransaction || sArchiveCount == UINT32_MAX
        || (size_t)sArchiveCount + 1 > SIZE_MAX / sizeof(*sArchiveCodecs)) return -1;
    const struct NativeSaveArchiveCodec **codecs = realloc(sArchiveCodecs, ((size_t)sArchiveCount + 1) * sizeof(*codecs));
    if (!codecs) return -1;
    sArchiveCodecs = codecs;
    sArchiveCodecs[sArchiveCount++] = codec; return 0;
}

/* Records are kept in owner/key order so enumeration and save encoding are
 * deterministic. Unknown owners survive import until their modules return. */
struct ModArchiveEntry { u64 owner, key, revision; u32 size; u8 *data; };
static struct ModArchiveEntry *sModArchive;
static size_t sModArchiveCapacity;
static u32 sModArchiveCount, sModArchiveBytes;
static u64 sModArchiveRun, sModArchiveRevision;
enum { MOD_ARCHIVE_HEADER = 32, MOD_ARCHIVE_RECORD = 32 };
static void ModArchiveReset(void)
{
    for (u32 i = 0; i < sModArchiveCount; ++i) free(sModArchive[i].data);
    free(sModArchive); sModArchive = NULL; sModArchiveCapacity = 0;
    sModArchiveCount = sModArchiveBytes = 0; sModArchiveRun = sModArchiveRevision = 0;
}
static int ModArchiveValidate(const u8 *data, u32 size)
{
    if (!size) return data != NULL;
    if (!data || size < MOD_ARCHIVE_HEADER || memcmp(data, "RHHMDA01", 8) || GetWord(data + 8) != size
        || !GetLong(data + 16) || !GetLong(data + 24)) return -1;
    u32 count = GetWord(data + 12), at = MOD_ARCHIVE_HEADER;
    if (count > (size - MOD_ARCHIVE_HEADER) / MOD_ARCHIVE_RECORD) return -1;
    u64 lastOwner = 0, lastKey = 0, revision = GetLong(data + 24);
    for (u32 i = 0; i < count; ++i)
    {
        if (size - at < MOD_ARCHIVE_RECORD) return -1;
        u64 owner = GetLong(data + at), key = GetLong(data + at + 8), entryRevision = GetLong(data + at + 16);
        u32 length = GetWord(data + at + 24), reserved = GetWord(data + at + 28); at += MOD_ARCHIVE_RECORD;
        if (!owner || !key || !entryRevision || entryRevision > revision || reserved || !length
            || length > size - at
            || owner < lastOwner || (owner == lastOwner && key <= lastKey)) return -1;
        lastOwner = owner; lastKey = key; at += length;
    }
    return at == size ? 0 : -1;
}
static int ModArchiveExport(u8 **data, u32 *size)
{
    *data = NULL; *size = 0;
    if (!sModArchiveRun) return 0;
    u64 length64 = MOD_ARCHIVE_HEADER + (u64)sModArchiveCount * MOD_ARCHIVE_RECORD + sModArchiveBytes;
    if (length64 > UINT32_MAX) return -1;
    u32 length = length64;
    u8 *bytes = calloc(length, 1); if (!bytes) return -1;
    memcpy(bytes, "RHHMDA01", 8); PutHeaderWord(bytes + 8, length); PutHeaderWord(bytes + 12, sModArchiveCount);
    PutLong(bytes + 16, sModArchiveRun); PutLong(bytes + 24, sModArchiveRevision);
    u32 at = MOD_ARCHIVE_HEADER;
    for (u32 i = 0; i < sModArchiveCount; ++i)
    {
        const struct ModArchiveEntry *entry = &sModArchive[i];
        PutLong(bytes + at, entry->owner); PutLong(bytes + at + 8, entry->key); PutLong(bytes + at + 16, entry->revision);
        PutHeaderWord(bytes + at + 24, entry->size); at += MOD_ARCHIVE_RECORD;
        memcpy(bytes + at, entry->data, entry->size); at += entry->size;
    }
    if (ModArchiveValidate(bytes, length)) { free(bytes); return -1; }
    *data = bytes; *size = length; return 0;
}
static int ModArchiveImport(const u8 *data, u32 size)
{
    if (ModArchiveValidate(data, size)) return -1;
    if (!size) { ModArchiveReset(); return 0; }
    u32 count = GetWord(data + 12), at = MOD_ARCHIVE_HEADER, total = 0;
    struct ModArchiveEntry *entries = count ? calloc(count, sizeof(*entries)) : NULL;
    if (count && !entries) return -1;
    for (u32 i = 0; i < count; ++i)
    {
        struct ModArchiveEntry *entry = &entries[i];
        entry->owner = GetLong(data + at); entry->key = GetLong(data + at + 8);
        entry->revision = GetLong(data + at + 16); entry->size = GetWord(data + at + 24); at += MOD_ARCHIVE_RECORD;
        entry->data = malloc(entry->size);
        if (!entry->data) { for (u32 j = 0; j < i; ++j) free(entries[j].data); free(entries); return -1; }
        memcpy(entry->data, data + at, entry->size); at += entry->size; total += entry->size;
    }
    ModArchiveReset(); sModArchive = entries; sModArchiveCapacity = count;
    sModArchiveCount = count; sModArchiveBytes = total; sModArchiveRun = GetLong(data + 16);
    sModArchiveRevision = GetLong(data + 24); return 0;
}
static const struct NativeSaveArchiveCodec sModArchiveCodec = {
    ModArchiveExport, ModArchiveValidate, ModArchiveImport, ModArchiveReset, "rhh:mod_archive", NULL
};
static bool ModArchiveReady(u64 owner)
{ return RhhModsHasActiveRun() && RhhModsRegisteredKey(owner) && (!sModArchiveRun || sModArchiveRun == RhhModsRunId()); }
static bool ModArchiveWritable(u64 owner)
{ return ModArchiveReady(owner) && !sTransaction
    && ((RhhModsArchiveObserverAllowed() && RhhModsCurrentOwner() == owner)
        || (!RhhModsCurrentEvent() && RhhModsCommandsAllowed() && RhhModsUiInputOwner() == owner)); }
static u32 ModArchiveIndex(u64 owner, u64 key)
{
    u32 i = 0;
    while (i < sModArchiveCount && (sModArchive[i].owner < owner || (sModArchive[i].owner == owner && sModArchive[i].key < key))) ++i;
    return i;
}
static int ModArchiveReserve(size_t count)
{
    if (count <= sModArchiveCapacity) return 0;
    if (count > SIZE_MAX / sizeof(*sModArchive)) return -1;
    size_t capacity = sModArchiveCapacity ? sModArchiveCapacity : 16;
    while (capacity < count)
    {
        if (capacity > SIZE_MAX / 2) { capacity = count; break; }
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*sModArchive)) capacity = count;
    struct ModArchiveEntry *entries = realloc(sModArchive, capacity * sizeof(*entries));
    if (!entries) return -1;
    sModArchive = entries; sModArchiveCapacity = capacity; return 0;
}
static u32 ModArchiveOwnerBytes(u64 owner)
{
    u32 total = 0;
    for (u32 i = 0; i < sModArchiveCount; ++i) if (sModArchive[i].owner == owner) total += sModArchive[i].size;
    return total;
}
static void ModArchiveRecordAt(u32 index, struct RhhModArchiveRecord *out)
{
    const struct ModArchiveEntry *entry = &sModArchive[index];
    *out = (struct RhhModArchiveRecord){.struct_size = sizeof(*out), .size = entry->size,
        .owner = entry->owner, .key = entry->key, .run_id = sModArchiveRun, .revision = entry->revision};
}
static int ModArchiveUsage(u64 owner, struct RhhModArchiveUsage *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ModArchiveReady(owner)) return -1;
    struct RhhModArchiveUsage result = {.struct_size = sizeof(result), .owner_bytes = ModArchiveOwnerBytes(owner),
        .total_bytes = sModArchiveBytes, .run_id = RhhModsRunId(), .revision = sModArchiveRevision};
    for (u32 i = 0; i < sModArchiveCount; ++i) result.record_count += sModArchive[i].owner == owner;
    *out = result; return 0;
}
static int ModArchiveAt(u64 owner, u32 index, struct RhhModArchiveRecord *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ModArchiveReady(owner)) return -1;
    for (u32 i = 0; i < sModArchiveCount; ++i)
        if (sModArchive[i].owner == owner && !index--) { ModArchiveRecordAt(i, out); return 0; }
    return -1;
}
static int ModArchiveFind(u64 owner, u64 key, struct RhhModArchiveRecord *out)
{
    if (!out || out->struct_size < sizeof(*out) || !key || !ModArchiveReady(owner)) return -1;
    u32 i = ModArchiveIndex(owner, key);
    if (i == sModArchiveCount || sModArchive[i].owner != owner || sModArchive[i].key != key) return -1;
    ModArchiveRecordAt(i, out); return 0;
}
static int ModArchiveCopy(u64 owner, u64 key, u64 revision, void *out, u32 capacity)
{
    if (!key || !revision || !out || !ModArchiveReady(owner)) return -1;
    u32 i = ModArchiveIndex(owner, key);
    if (i == sModArchiveCount || sModArchive[i].owner != owner || sModArchive[i].key != key
        || sModArchive[i].revision != revision || capacity < sModArchive[i].size) return -1;
    memcpy(out, sModArchive[i].data, sModArchive[i].size); return 0;
}
static int ModArchivePut(u64 owner, u64 key, u64 expected, const void *data, u32 size, struct RhhModArchiveRecord *out)
{
    if (!out || out->struct_size < sizeof(*out) || !key || !data || !size
        || !ModArchiveWritable(owner) || sModArchiveRevision == UINT64_MAX) return -1;
    u32 i = ModArchiveIndex(owner, key);
    bool exists = i < sModArchiveCount && sModArchive[i].owner == owner && sModArchive[i].key == key;
    u32 old = exists ? sModArchive[i].size : 0;
    if ((exists ? sModArchive[i].revision != expected : expected != 0)
        || (!exists && sModArchiveCount == UINT32_MAX)
        || size > UINT32_MAX - (sModArchiveBytes - old)
        || MOD_ARCHIVE_HEADER + (u64)(sModArchiveCount + !exists) * MOD_ARCHIVE_RECORD
             + sModArchiveBytes - old + size > UINT32_MAX) return -1;
    u8 *copy = malloc(size); if (!copy) return -1; memcpy(copy, data, size);
    if (!exists && ModArchiveReserve((size_t)sModArchiveCount + 1)) { free(copy); return -1; }
    if (exists) free(sModArchive[i].data);
    else { memmove(&sModArchive[i + 1], &sModArchive[i], (sModArchiveCount - i) * sizeof(sModArchive[0])); ++sModArchiveCount; }
    sModArchiveRun = RhhModsRunId(); ++sModArchiveRevision; sModArchiveBytes = sModArchiveBytes - old + size;
    sModArchive[i] = (struct ModArchiveEntry){owner, key, sModArchiveRevision, size, copy};
    ModArchiveRecordAt(i, out); return 0;
}
static int ModArchiveRemove(u64 owner, u64 key, u64 expected)
{
    if (!key || !expected || !ModArchiveWritable(owner) || sModArchiveRevision == UINT64_MAX) return -1;
    u32 i = ModArchiveIndex(owner, key);
    if (i == sModArchiveCount || sModArchive[i].owner != owner || sModArchive[i].key != key || sModArchive[i].revision != expected) return -1;
    sModArchiveBytes -= sModArchive[i].size; free(sModArchive[i].data);
    memmove(&sModArchive[i], &sModArchive[i + 1], (sModArchiveCount - i - 1) * sizeof(sModArchive[0]));
    memset(&sModArchive[--sModArchiveCount], 0, sizeof(sModArchive[0])); ++sModArchiveRevision; return 0;
}
static int ModArchiveBatch(u64 owner, const struct RhhModArchiveOperation *operations, u32 count,
                           struct RhhModArchiveBatchResult *out)
{
    if (!operations || !count || count > RHH_MOD_ARCHIVE_BATCH_MAX || !out || out->struct_size < sizeof(*out)
        || !ModArchiveWritable(owner) || sModArchiveRevision > UINT64_MAX - count) return -1;
    struct RhhModArchiveOperation ops[RHH_MOD_ARCHIVE_BATCH_MAX];
    u8 *staged[RHH_MOD_ARCHIVE_BATCH_MAX] = {0}; bool existed[RHH_MOD_ARCHIVE_BATCH_MAX];
    memcpy(ops, operations, count * sizeof(*ops));
    s64 totalBytes = sModArchiveBytes, finalCount = sModArchiveCount;
    for (u32 n = 0; n < count; ++n)
    {
        const struct RhhModArchiveOperation *op = &ops[n];
        if (op->struct_size < sizeof(*op) || op->reserved || !op->key
            || (op->kind != RHH_MOD_ARCHIVE_PUT && op->kind != RHH_MOD_ARCHIVE_REMOVE)) return -1;
        for (u32 j = 0; j < n; ++j) if (ops[j].key == op->key) return -1;
        u32 i = ModArchiveIndex(owner, op->key);
        existed[n] = i < sModArchiveCount && sModArchive[i].owner == owner && sModArchive[i].key == op->key;
        u32 oldSize = existed[n] ? sModArchive[i].size : 0;
        if (existed[n] ? sModArchive[i].revision != op->expected_revision : op->expected_revision != 0) return -1;
        if (op->kind == RHH_MOD_ARCHIVE_REMOVE)
        {
            if (!existed[n] || !op->expected_revision || op->data || op->size) return -1;
            --finalCount; totalBytes -= oldSize;
        }
        else
        {
            if (!op->data || !op->size) return -1;
            if (!existed[n]) ++finalCount;
            totalBytes += (s64)op->size - oldSize;
        }
    }
    if (finalCount < 0 || finalCount > UINT32_MAX || totalBytes < 0
        || MOD_ARCHIVE_HEADER + (u64)finalCount * MOD_ARCHIVE_RECORD + totalBytes > UINT32_MAX) return -1;
    for (u32 n = 0; n < count; ++n)
        if (ops[n].kind == RHH_MOD_ARCHIVE_PUT)
        {
            staged[n] = malloc(ops[n].size);
            if (!staged[n]) { for (u32 j = 0; j < n; ++j) free(staged[j]); return -1; }
            memcpy(staged[n], ops[n].data, ops[n].size);
        }
    if (ModArchiveReserve(finalCount))
    { for (u32 n = 0; n < count; ++n) free(staged[n]); return -1; }
    struct RhhModArchiveBatchResult result = {.struct_size = sizeof(result), .count = count, .owner = owner,
        .run_id = RhhModsRunId(), .revision = sModArchiveRevision + count};
    for (u32 n = 0; n < count; ++n)
        result.operations[n] = (struct RhhModArchiveOperationResult){ops[n].kind, ops[n].size,
            ops[n].key, sModArchiveRevision + n + 1};
    /* Remove first so a full archive can replace removed slots with new keys.
     * Revisions still reflect input order and no callback observes the steps. */
    for (u32 n = 0; n < count; ++n)
        if (ops[n].kind == RHH_MOD_ARCHIVE_REMOVE)
        {
            u32 i = ModArchiveIndex(owner, ops[n].key);
            free(sModArchive[i].data);
            if (i + 1 < sModArchiveCount)
                memmove(&sModArchive[i], &sModArchive[i + 1], (sModArchiveCount - i - 1) * sizeof(sModArchive[0]));
            memset(&sModArchive[--sModArchiveCount], 0, sizeof(sModArchive[0]));
        }
    for (u32 n = 0; n < count; ++n)
        if (ops[n].kind == RHH_MOD_ARCHIVE_PUT)
        {
            u32 i = ModArchiveIndex(owner, ops[n].key);
            if (existed[n]) free(sModArchive[i].data);
            else
            {
                if (i < sModArchiveCount)
                    memmove(&sModArchive[i + 1], &sModArchive[i], (sModArchiveCount - i) * sizeof(sModArchive[0]));
                ++sModArchiveCount;
            }
            sModArchive[i] = (struct ModArchiveEntry){owner, ops[n].key, result.operations[n].revision, ops[n].size, staged[n]};
        }
    sModArchiveRun = result.run_id; sModArchiveRevision = result.revision; sModArchiveBytes = totalBytes;
    *out = result; return 0;
}
static const struct RhhModArchiveService sModArchiveService = {
    {sizeof(sModArchiveService), RHH_SERVICE_VERSION(1, 3)}, ModArchiveUsage, ModArchiveAt, ModArchiveFind,
    ModArchiveCopy, ModArchivePut, ModArchiveRemove, ModArchiveBatch
};
int RhhModsInstallArchiveService(void)
{
    return NativeSaveRegisterArchive(&sModArchiveCodec) || RhhModsAddService("rhh:service/mod_archive", &sModArchiveService.header) ? -1 : 0;
}
void NativeSaveResetArchive(void)
{ for (u32 i = 0; i < sArchiveCount; ++i) sArchiveCodecs[i]->reset(); }

static void DiscardSpecialBackup(void)
{
    for (unsigned i = 0; i < 3; ++i) free(sSpecialBackup[i].data);
    memset(sSpecialBackup, 0, sizeof(sSpecialBackup));
}
static void RestoreSpecialBackup(void)
{
    for (unsigned i = 0; i < 3; ++i) free(sSpecial[i].data);
    memcpy(sSpecial, sSpecialBackup, sizeof(sSpecial));
    memset(sSpecialBackup, 0, sizeof(sSpecialBackup));
}

int NativeSaveBegin(void)
{
    if (sTransaction)
    {
        memcpy(gNativeFlash, sFlashBackup, sizeof(gNativeFlash));
        free(sFlashBackup); sFlashBackup = NULL;
        free(sPending.data); free(sPending.archive); memset(&sPending, 0, sizeof(sPending));
        free(sPendingGame.data); memset(&sPendingGame, 0, sizeof(sPendingGame));
        RestoreSpecialBackup();
        sTransaction = false;
    }
    sTransactionInvalid = false;
    RhhModsPrepareSave();
    sFlashBackup = malloc(sizeof(gNativeFlash));
    int exportFailed = !sFlashBackup || RhhModsExport(&sPending.data, &sPending.size);
    for (unsigned i = 0; i < 3 && !exportFailed; ++i)
    {
        if (!sSpecial[i].size) continue;
        sSpecialBackup[i].data = malloc(sSpecial[i].size);
        if (!sSpecialBackup[i].data) { exportFailed = true; break; }
        sSpecialBackup[i].size = sSpecial[i].size;
        memcpy(sSpecialBackup[i].data, sSpecial[i].data, sSpecial[i].size);
    }
    if (!exportFailed)
    {
        sArchiveSaveExport = true;
        exportFailed = ExportArchives(&sPending.archive, &sPending.archiveSize);
        sArchiveSaveExport = false;
    }
    if (exportFailed)
    { free(sFlashBackup); sFlashBackup = NULL; free(sPending.data); free(sPending.archive); DiscardSpecialBackup();
      memset(&sPending, 0, sizeof(sPending)); return -1; }
    memcpy(sFlashBackup, gNativeFlash, sizeof(gNativeFlash));
    sTransaction = true;
    return 0;
}
int NativeSaveEnd(uint32_t generation, int success)
{
    if (!sTransaction) return -1;
    struct ModSnapshot old[2] = {sSnapshots[0], sSnapshots[1]};
    struct GameSnapshot oldGame[2] = {sGameSnapshots[0], sGameSnapshots[1]};
    if (success && sCommitGuard && sCommitGuard()) success = false;
    success = success && !sTransactionInvalid;
    sTransaction = sTransactionInvalid = false;
    int result = -1;
    if (success)
    {
        sPending.generation = generation;
        sSnapshots[0] = sPending;
        sSnapshots[1] = old[0].data && old[0].generation == generation ? old[1] : old[0];
        sGameSnapshots[0] = sPendingGame;
        sGameSnapshots[1] = old[0].data && old[0].generation == generation ? oldGame[1] : oldGame[0];
        result = NativeSaveCommit();
    }
    if (result)
    {
        memcpy(gNativeFlash, sFlashBackup, sizeof(gNativeFlash));
        sSnapshots[0] = old[0]; sSnapshots[1] = old[1];
        sGameSnapshots[0] = oldGame[0]; sGameSnapshots[1] = oldGame[1];
        RestoreSpecialBackup();
        free(sPending.data); free(sPending.archive);
        free(sPendingGame.data);
    }
    else
    {
        struct ModSnapshot discarded = old[0].data && old[0].generation == generation ? old[0] : old[1];
        free(discarded.data); free(discarded.archive);
        free(old[0].data && old[0].generation == generation ? oldGame[0].data : oldGame[1].data);
        DiscardSpecialBackup();
    }
    memset(&sPending, 0, sizeof(sPending));
    memset(&sPendingGame, 0, sizeof(sPendingGame));
    free(sFlashBackup); sFlashBackup = NULL;
    return result;
}
int NativeSaveRestoreMods(uint32_t generation)
{
    if (!sSnapshots[0].data) { RhhModsResetState(); NativeSaveResetArchive(); return 0; } /* v1 migration */
    for (unsigned i = 0; i < 2; ++i)
        if (sSnapshots[i].data && sSnapshots[i].generation == generation)
        {
            u8 *previousState = NULL, *previousArchive = NULL; u32 stateSize = 0, archiveSize = 0;
            int previousLock = RhhModsSettingsLocked();
            if (RhhModsExport(&previousState, &stateSize) || ExportArchives(&previousArchive, &archiveSize))
            { free(previousState); free(previousArchive); return -1; }
            int result = RhhModsImport(sSnapshots[i].data, sSnapshots[i].size);
            if (!result) result = ImportArchives(sSnapshots[i].archive, sSnapshots[i].archiveSize);
            if (result)
            {
                if (RhhModsImport(previousState, stateSize) || ImportArchives(previousArchive, archiveSize))
                    fprintf(stderr, "Could not restore prior mod archives after a failed load\n");
                RhhModsRestoreSettingsLock(previousLock);
            }
            free(previousState); free(previousArchive);
            if (!result && i)
            {
                struct ModSnapshot swap = sSnapshots[0]; sSnapshots[0] = sSnapshots[1]; sSnapshots[1] = swap;
                struct GameSnapshot game = sGameSnapshots[0]; sGameSnapshots[0] = sGameSnapshots[1]; sGameSnapshots[1] = game;
            }
            return result;
        }
    fprintf(stderr, "No mod snapshot for recovered save generation %u\n", generation);
    return -1;
}
uint64_t NativeSaveNewRunSerial(void) { return ++sRunSerial; }
int NativeSaveSetJournal(uint64_t runId, const void *data, uint32_t size)
{
    u8 *copy = size ? malloc(size) : NULL;
    if (size && !copy) return -1;
    if (size) memcpy(copy, data, size);
    u8 *old = sJournal; u32 oldSize = sJournalSize;
    u8 *oldPrevious = sPreviousJournal; u32 oldPreviousSize = sPreviousJournalSize;
    u64 oldId = sJournalId, oldPreviousId = sPreviousJournalId;
    bool shift = old && oldId != runId;
    if (shift) { sPreviousJournal = old; sPreviousJournalSize = oldSize; sPreviousJournalId = oldId; }
    sJournal = copy; sJournalSize = size;
    sJournalId = runId;
    /* A journal cannot publish an in-progress game save. It will be included
     * in the transaction's atomic commit if one is currently running. */
    int result = NativeSaveCommit();
    if (result)
    {
        sJournal = old; sJournalSize = oldSize; sJournalId = oldId;
        sPreviousJournal = oldPrevious; sPreviousJournalSize = oldPreviousSize; sPreviousJournalId = oldPreviousId;
        free(copy);
    }
    else free(shift ? oldPrevious : old);
    return result;
}
const void *NativeSaveGetJournal(uint64_t runId, uint32_t *size)
{
    if (sJournal && sJournalId == runId) { *size = sJournalSize; return sJournal; }
    if (sPreviousJournal && sPreviousJournalId == runId) { *size = sPreviousJournalSize; return sPreviousJournal; }
    *size = 0; return NULL;
}

static u16 WriteByte(u16 sector, u32 offset, u8 value)
{
    if (sector >= 32 || offset >= 4096) return 1;
    gNativeFlash[sector * 4096 + offset] = value;
    return NativeSaveCommit() ? 1 : 0;
}

static u16 WriteSector(u16 sector, u8 *src)
{
    if (sector >= 32) return 1;
    memcpy(gNativeFlash + sector * 4096, src, 4096);
    return NativeSaveCommit() ? 1 : 0;
}

static u16 EraseSector(u16 sector)
{
    if (sector >= 32) return 1;
    memset(gNativeFlash + sector * 4096, 0xff, 4096);
    return NativeSaveCommit() ? 1 : 0;
}

static u16 EraseChip(void)
{
    memset(gNativeFlash, 0xff, sizeof(gNativeFlash));
    return NativeSaveCommit() ? 1 : 0;
}

u16 (*ProgramFlashByte)(u16, u32, u8) = WriteByte;
u16 (*ProgramFlashSector)(u16, u8 *) = WriteSector;
u16 (*EraseFlashSector)(u16) = EraseSector;
u16 (*EraseFlashChip)(void) = EraseChip;

u16 IdentifyFlash(void) { return 0; }
u16 SetFlashTimerIntr(u8 timer, void (**callback)(void)) { return timer >= 4; }
void SwitchFlashBank(u8 bank) { (void)bank; }

void ReadFlash(u16 sector, u32 offset, u8 *dest, u32 size)
{
    size_t start = (size_t)sector * 4096 + offset;
    if (start > sizeof(gNativeFlash) || size > sizeof(gNativeFlash) - start)
    {
        fprintf(stderr, "Invalid flash read: %u/%u/%u\n", sector, offset, size);
        abort();
    }
    memcpy(dest, gNativeFlash + start, size);
}

u32 ProgramFlashSectorAndVerify(u16 sector, u8 *src)
{
    if (WriteSector(sector, src)) return 1;
    return memcmp(gNativeFlash + sector * 4096, src, 4096) != 0;
}

u32 VerifyFlashSectorNBytes(u16 sector, u8 *src, u32 size)
{
    if (sector >= 32 || size > 4096) return 1;
    return memcmp(gNativeFlash + sector * 4096, src, size) != 0;
}

u32 VerifyFlashSector(u16 sector, u8 *src)
{
    return VerifyFlashSectorNBytes(sector, src, 4096);
}
