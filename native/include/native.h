#ifndef NATIVE_PLATFORM_H
#define NATIVE_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

void NativeRunDma(unsigned timing);
void NativeFrame(void);
void NativeReset(void);
int NativeSaveOpen(const char *path);
int NativeSaveCommit(void);
void NativeSaveClose(void);
int NativeSaveBegin(void);
int NativeSaveEnd(uint32_t generation, int success);
int NativeSaveRestoreMods(uint32_t generation);
uint64_t NativeSaveNewRunSerial(void);
int NativeSaveSetJournal(uint64_t runId, const void *data, uint32_t size);
const void *NativeSaveGetJournal(uint64_t runId, uint32_t *size);
int NativeRtcSetFixed(const char *timestamp);

#endif
