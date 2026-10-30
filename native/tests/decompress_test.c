#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "decompress.h"
#include "decompress_error_handler.h"

// Use host allocation for this isolated codec test; game allocation and save
// layout are tested separately. The codec itself is the production RHH C.
void *Alloc_(u32 size, const char *location) { (void)location; return malloc(size); }
static int sFailUnchecked, sUncheckedCalls;
void *AllocUnchecked_(u32 size, const char *location) { (void)location; ++sUncheckedCalls; return sFailUnchecked ? NULL : malloc(size); }
void Free(void *pointer) { free(pointer); }

void DecompressionError(const u32 *src, enum CompressionError error)
{
    fprintf(stderr, "Decompression error %u at %p\n", error, (void *)src);
    exit(1);
}

int main(int argc, char **argv)
{
#ifdef NATIVE
    union CompressionHeader bad = {0}; bad.smol.mode = ENCODE_SYMS; bad.smol.loSize = 1; bad.smol.symSize = 1;
    u32 inputHeader[32] = {0}; memcpy(inputHeader, &bad, sizeof(bad));
    u8 untouched[16]; memset(untouched, 0x5a, sizeof(untouched));
    sFailUnchecked = 1;
    if (TryDecompressDataWithHeaderWram(inputHeader, untouched) || sUncheckedCalls != 1) return 1;
    for (unsigned i = 0; i < sizeof(untouched); ++i) if (untouched[i] != 0x5a) return 1;
    sFailUnchecked = 0;
#endif
    if (argc != 3) return 1;
    FILE *raw = fopen(argv[1], "rb"), *compressed = fopen(argv[2], "rb");
    if (!raw || !compressed) return 1;
    fseek(raw, 0, SEEK_END);
    size_t rawSize = ftell(raw);
    rewind(raw);
    fseek(compressed, 0, SEEK_END);
    size_t compressedSize = ftell(compressed);
    rewind(compressed);
    u8 *expected = malloc(rawSize), *input = malloc(compressedSize);
    u8 *result = malloc(rawSize + 32);
    if (!expected || !input || !result) return 1;
    memset(result, 0xcd, rawSize + 32);
    if (fread(expected, 1, rawSize, raw) != rawSize) return 1;
    if (fread(input, 1, compressedSize, compressed) != compressedSize) return 1;
    fclose(raw); fclose(compressed);
    DecompressDataWithHeaderWram((const u32 *)input, result);
    if (memcmp(expected, result, rawSize))
    {
        fprintf(stderr, "RHH compression roundtrip failed: %s\n", argv[2]);
        return 1;
    }
    for (unsigned i = 0; i < 32; ++i)
        if (result[rawSize + i] != 0xcd)
        {
            fprintf(stderr, "RHH decompression overrun at +%u\n", i);
            return 1;
        }
#ifdef NATIVE
    memset(result, 0xcd, rawSize + 32);
    if (!TryDecompressDataWithHeaderWram((const u32 *)input, result) || memcmp(expected, result, rawSize))
    {
        fprintf(stderr, "checked decompression failed: %s\n", argv[2]);
        return 1;
    }
    for (unsigned i = 0; i < 32; ++i)
        if (result[rawSize + i] != 0xcd) return 1;
#endif
    free(result); free(input); free(expected);
    return 0;
}
