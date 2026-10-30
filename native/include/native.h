#ifndef NATIVE_PLATFORM_H
#define NATIVE_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

void NativeRunDma(unsigned timing);
/* Internal pre-write observer for native emulated memory operations. */
void NativeMemorySetWriteObserver(void (*observer)(void *destination, size_t size));
void NativeFrame(void);
void NativeReset(void);
int NativeSaveOpen(const char *path);
int NativeSaveCommit(void);
void NativeSaveClose(void);
int NativeSaveBegin(void);
int NativeSaveTransactionActive(void);
/* A host invariant failed after the save snapshot was staged. The pending
 * generation must be rolled back even if the game reports success. */
void NativeSaveInvalidateTransaction(void);
/* Internal guard for host state that must still match its staged snapshot. */
void NativeSaveSetCommitGuard(int (*guard)(void));
/* Host-owned codecs have static lifetime and register before a save opens.
 * export_data returns an allocated buffer (or NULL/zero for no data); the save
 * layer frees it. validate must be read-only; NULL/zero represents a missing
 * part. import_data runs only after all parts validate. */
struct NativeSaveArchiveCodec {
    int (*export_data)(uint8_t **data, uint32_t *size);
    int (*validate)(const uint8_t *data, uint32_t size);
    int (*import_data)(const uint8_t *data, uint32_t size);
    void (*reset)(void);
    /* Stable host-owned ID. Optional eight-byte signature for a legacy raw
     * archive; signatures must be unique among registered codecs. */
    const char *id;
    const char *legacy_magic;
};
int NativeSaveRegisterArchive(const struct NativeSaveArchiveCodec *codec);
int RhhModsInstallArchiveService(void);
int NativeSaveArchiveSaveExportActive(void);
void NativeSaveResetArchive(void);
int NativeSaveEnd(uint32_t generation, int success);
int NativeSaveRestoreMods(uint32_t generation);
/* Stage or restore the complete native game state as four length-delimited
 * regions. Stage requires an active save transaction. */
int NativeSaveStageGame(const void *const regions[4], const size_t sizes[4], int retainStorage);
int NativeSaveLoadGame(void *const regions[4], const size_t sizes[4], uint32_t *generation);
int NativeSaveReadGameRegion(uint32_t region, size_t offset, void *out, size_t size);
int NativeSaveGameAvailable(void);
int NativeSaveEraseGame(void);
/* Special records are length-delimited. Read returns 1 when absent and -1
 * for invalid arguments or a size mismatch. */
int NativeSaveSpecialRead(uint32_t key, void *out, size_t size);
int NativeSaveSpecialSize(uint32_t key, size_t *size);
int NativeSaveSpecialWrite(uint32_t key, const void *data, size_t size);
uint64_t NativeSaveNewRunSerial(void);
int NativeSaveSetJournal(uint64_t runId, const void *data, uint32_t size);
const void *NativeSaveGetJournal(uint64_t runId, uint32_t *size);
int NativeRtcSetFixed(const char *timestamp);

#endif
