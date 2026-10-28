#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "native.h"
#include "agb_flash.h"
#include "gba/flash_internal.h"

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)

static void TestMemory(void)
{
    u32 src[16], dst[16];
    for (unsigned i = 0; i < 16; ++i) src[i] = 0x12340000 + i;
    CpuSet(src, dst, CPU_SET_32BIT | 16);
    CHECK(!memcmp(src, dst, sizeof(src)));
    CpuSet(src, dst, CPU_SET_32BIT | CPU_SET_SRC_FIXED | 16);
    for (unsigned i = 0; i < 16; ++i) CHECK(dst[i] == src[0]);
    CpuFastSet(src, dst, 16);
    CHECK(!memcmp(src, dst, sizeof(src)));
    DmaSet(3, src, dst, ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC) << 16) | 16);
    CHECK(!memcmp(src, dst, sizeof(src)));
    memset(dst, 0, sizeof(dst));
    DmaSet(0, src, dst, ((DMA_ENABLE | DMA_32BIT | DMA_START_HBLANK | DMA_REPEAT | DMA_DEST_RELOAD) << 16) | 2);
    CHECK(dst[0] == 0);
    NativeRunDma(2);
    CHECK(dst[0] == src[0] && dst[1] == src[1]);
    NativeRunDma(2);
    CHECK(dst[0] == src[2] && dst[1] == src[3]);
    DmaStop(0);
    NativeRunDma(2);
    CHECK(dst[0] == src[2]);
}

static void TestCompression(void)
{
    // Literal ABC followed by an overlapping back-reference, clipped at size.
    const u8 lz[] = {0x10, 9, 0, 0, 0x10, 'A', 'B', 'C', 0x30, 0x02};
    u8 dst[16];
    memset(dst, 0xcd, sizeof(dst));
    LZ77UnCompWram((const u32 *)lz, dst);
    CHECK(!memcmp(dst, "ABCABCABC", 9));
    CHECK(dst[9] == 0xcd);
    const u8 rl[] = {0x30, 7, 0, 0, 1, 'a', 'b', 0x82, 'c'};
    RLUnCompWram((const u32 *)rl, dst);
    CHECK(!memcmp(dst, "abccccc", 7));
}

int main(int argc, char **argv)
{
    CHECK(argc == 3);
    if (!strcmp(argv[1], "reject"))
    {
        CHECK(NativeSaveOpen(argv[2]) == -1);
        NativeSaveClose();
        return 0;
    }
    TestMemory();
    TestCompression();
    CHECK(NativeSaveOpen(argv[2]) == 0);
    if (!strcmp(argv[1], "write"))
    {
        for (unsigned i = 0; i < sizeof(gNativeFlash); ++i)
            CHECK(gNativeFlash[i] == 0xff);
        u8 sector[4096];
        for (unsigned i = 0; i < sizeof(sector); ++i) sector[i] = i * 31;
        CHECK(ProgramFlashSectorAndVerify(17, sector) == 0);
        CHECK(ProgramFlashByte(31, 4095, 0x5a) == 0);
        CHECK(ProgramFlashByte(32, 0, 0) != 0);
    }
    else
    {
        for (unsigned i = 0; i < 4096; ++i)
            CHECK(gNativeFlash[17 * 4096 + i] == (u8)(i * 31));
        CHECK(gNativeFlash[131071] == 0x5a);
        CHECK(gNativeFlash[0] == 0xff);
    }
    NativeSaveClose();
    puts("native platform checks passed");
    return 0;
}
