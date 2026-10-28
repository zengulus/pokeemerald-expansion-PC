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
#include "gba/flash_internal.h"
#include "native.h"
#include "mod_host.h"

static char *sSavePath;
enum { NATIVE_SAVE_HEADER_SIZE = 32, MAX_EXTENSION = 16 * 1024 * 1024, MAX_JOURNAL = 4 * 1024 * 1024 };
static const u8 sSaveMagic[8] = {'R', 'H', 'H', 'N', 'A', 'T', 'V', 0};
struct ModSnapshot { u32 generation, size; u8 *data; };
static struct ModSnapshot sSnapshots[2], sPending;
static u8 *sJournal, *sPreviousJournal, *sFlashBackup;
static u32 sJournalSize, sPreviousJournalSize;
static u64 sJournalId, sPreviousJournalId, sRunSerial;
static bool sTransaction;

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
    for (unsigned i = 0; i < 2; ++i) free(sSnapshots[i].data);
    memset(sSnapshots, 0, sizeof(sSnapshots));
    free(sPending.data); memset(&sPending, 0, sizeof(sPending));
    free(sJournal); sJournal = NULL; sJournalSize = 0;
    free(sPreviousJournal); sPreviousJournal = NULL; sPreviousJournalSize = 0;
    sJournalId = sPreviousJournalId = sRunSerial = 0;
    free(sFlashBackup); sFlashBackup = NULL; sTransaction = false;
}

static void PutHeaderWord(u8 *dest, u32 value)
{
    for (unsigned i = 0; i < 4; ++i) dest[i] = value >> (i * 8);
}
static void PutLong(u8 *p, u64 n) { PutHeaderWord(p, n); PutHeaderWord(p + 4, n >> 32); }

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

static void MakeSaveHeader(u8 header[NATIVE_SAVE_HEADER_SIZE], u32 extensionSize)
{
    memset(header, 0, NATIVE_SAVE_HEADER_SIZE);
    memcpy(header, sSaveMagic, sizeof(sSaveMagic));
    PutHeaderWord(header + 8, extensionSize ? 2 : 1);
    PutHeaderWord(header + 12, NATIVE_SAVE_HEADER_SIZE);
    PutHeaderWord(header + 16, sizeof(gNativeFlash));
    PutHeaderWord(header + 20, sizeof(void *) * 8);
    PutHeaderWord(header + 24, SaveLayoutSignature());
    PutHeaderWord(header + 28, extensionSize);
}

static int EncodeExtension(u8 **data, u32 *size)
{
    *data = NULL; *size = 0;
    if (!sSnapshots[0].data && !sJournalSize && !sPreviousJournalSize) return 0;
    u32 total = 64 + sSnapshots[0].size + sSnapshots[1].size + sJournalSize + sPreviousJournalSize;
    u8 *buffer = calloc(total, 1);
    if (!buffer) return -1;
    memcpy(buffer, "RHHEX003", 8);
    PutHeaderWord(buffer + 8, total);
    PutHeaderWord(buffer + 16, sJournalSize);
    PutHeaderWord(buffer + 20, sPreviousJournalSize);
    PutLong(buffer + 40, sRunSerial);
    PutLong(buffer + 48, sJournalId);
    PutLong(buffer + 56, sPreviousJournalId);
    u32 at = 64;
    for (unsigned i = 0; i < 2; ++i)
    {
        PutHeaderWord(buffer + 24 + i * 8, sSnapshots[i].generation);
        PutHeaderWord(buffer + 28 + i * 8, sSnapshots[i].size);
        if (sSnapshots[i].size) memcpy(buffer + at, sSnapshots[i].data, sSnapshots[i].size);
        at += sSnapshots[i].size;
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
    if (size < (oldFormat ? 40 : 64) || (!oldFormat && memcmp(data, "RHHEX003", 8))
        || GetWord(data + 8) != size || (oldFormat && GetWord(data + 20))
        || ExtensionChecksum(data + 16, size - 16) != GetWord(data + 12)) return -1;
    u32 at = oldFormat ? 40 : 64;
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
    sJournalSize = GetWord(data + 16);
    sPreviousJournalSize = oldFormat ? 0 : GetWord(data + 20);
    if (sJournalSize > size - at || sPreviousJournalSize != size - at - sJournalSize
        || sJournalSize > MAX_JOURNAL || sPreviousJournalSize > MAX_JOURNAL) return -1;
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
    RhhModsResetState();
    memset(gNativeFlash, 0xff, sizeof(gNativeFlash));
    file = fopen(path, "rb");
    if (file)
    {
        u8 expected[NATIVE_SAVE_HEADER_SIZE], actual[NATIVE_SAVE_HEADER_SIZE];
        MakeSaveHeader(expected, 0);
        if (fread(actual, 1, sizeof(actual), file) != sizeof(actual))
        { fclose(file); return -1; }
        u32 version = GetWord(actual + 8), extensionSize = GetWord(actual + 28);
        PutHeaderWord(expected + 8, version);
        PutHeaderWord(expected + 28, extensionSize);
        if ((version != 1 && version != 2) || (version == 1 && extensionSize)
            || (version == 2 && extensionSize < 40) || extensionSize > MAX_EXTENSION
            || memcmp(actual, expected, sizeof(actual)))
        {
            fclose(file);
            fprintf(stderr, "Incompatible native save header (version, architecture or layout): %s\n", path);
            return -1;
        }
        size_t count = fread(gNativeFlash, 1, sizeof(gNativeFlash), file);
        u8 *extension = extensionSize ? malloc(extensionSize) : NULL;
        bool extensionFailed = extensionSize && (!extension
            || fread(extension, 1, extensionSize, file) != extensionSize
            || DecodeExtension(extension, extensionSize));
        free(extension);
        int extra = fgetc(file);
        int failed = ferror(file);
        fclose(file);
        if (count != sizeof(gNativeFlash) || extra != EOF || failed || extensionFailed)
        {
            fprintf(stderr, "Invalid native save size: %s\n", path);
            if (extensionFailed) fprintf(stderr, "Mod state rejected: %s\n", RhhModsError());
            ClearModSave();
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
    u8 *extension; u32 extensionSize;
    if (EncodeExtension(&extension, &extensionSize)) return -1;
    char *temporary = malloc(strlen(sSavePath) + 5);
    if (!temporary) { free(extension); return -1; }
    sprintf(temporary, "%s.tmp", sSavePath);
    FILE *file = fopen(temporary, "wb");
    int result = -1;
    if (file)
    {
        u8 header[NATIVE_SAVE_HEADER_SIZE];
        MakeSaveHeader(header, extensionSize);
        int ok = fwrite(header, 1, sizeof(header), file) == sizeof(header);
        if (fwrite(gNativeFlash, 1, sizeof(gNativeFlash), file) != sizeof(gNativeFlash)) ok = 0;
        if (extensionSize && fwrite(extension, 1, extensionSize, file) != extensionSize) ok = 0;
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
    free(extension);
    return result;
}

void NativeSaveClose(void)
{
    ClearModSave();
    free(sSavePath);
    sSavePath = NULL;
}

int NativeSaveBegin(void)
{
    if (sTransaction)
    {
        memcpy(gNativeFlash, sFlashBackup, sizeof(gNativeFlash));
        free(sFlashBackup); sFlashBackup = NULL;
        free(sPending.data); memset(&sPending, 0, sizeof(sPending));
        sTransaction = false;
    }
    sFlashBackup = malloc(sizeof(gNativeFlash));
    if (!sFlashBackup || RhhModsExport(&sPending.data, &sPending.size))
    { free(sFlashBackup); sFlashBackup = NULL; return -1; }
    memcpy(sFlashBackup, gNativeFlash, sizeof(gNativeFlash));
    sTransaction = true;
    return 0;
}
int NativeSaveEnd(uint32_t generation, int success)
{
    if (!sTransaction) return -1;
    struct ModSnapshot old[2] = {sSnapshots[0], sSnapshots[1]};
    sTransaction = false;
    int result = -1;
    if (success)
    {
        sPending.generation = generation;
        sSnapshots[0] = sPending;
        sSnapshots[1] = old[0].data && old[0].generation == generation ? old[1] : old[0];
        result = NativeSaveCommit();
    }
    if (result)
    {
        memcpy(gNativeFlash, sFlashBackup, sizeof(gNativeFlash));
        sSnapshots[0] = old[0]; sSnapshots[1] = old[1];
        free(sPending.data);
    }
    else free(old[0].data && old[0].generation == generation ? old[0].data : old[1].data);
    memset(&sPending, 0, sizeof(sPending));
    free(sFlashBackup); sFlashBackup = NULL;
    return result;
}
int NativeSaveRestoreMods(uint32_t generation)
{
    if (!sSnapshots[0].data) { RhhModsResetState(); return 0; } /* v1 migration: all defaults off */
    for (unsigned i = 0; i < 2; ++i)
        if (sSnapshots[i].data && sSnapshots[i].generation == generation)
        {
            int result = RhhModsImport(sSnapshots[i].data, sSnapshots[i].size);
            if (!result && i)
            { struct ModSnapshot swap = sSnapshots[0]; sSnapshots[0] = sSnapshots[1]; sSnapshots[1] = swap; }
            return result;
        }
    fprintf(stderr, "No mod snapshot for recovered save generation %u\n", generation);
    return -1;
}
uint64_t NativeSaveNewRunSerial(void) { return ++sRunSerial; }
int NativeSaveSetJournal(uint64_t runId, const void *data, uint32_t size)
{
    if (size > MAX_JOURNAL) return -1;
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
