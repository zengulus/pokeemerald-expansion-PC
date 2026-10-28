#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "dma3.h"
#include "bg.h"
#include "gpu_regs.h"

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "graphics transfer: %s\n", #expr); exit(1); } } while (0)

int main(void)
{
    static u32 source[13000], dest[13000];
    for (unsigned i = 0; i < ARRAY_COUNT(source); ++i) source[i] = i * 7919;
    ClearDma3Requests();
    // Copy more than the original 40 KiB per-vblank budget, outside vblank.
    REG_VCOUNT = 227;
    s16 request = RequestDma3Copy(source, dest, sizeof(source), 1);
    CHECK(request >= 0);
    CHECK(dest[1] == 0);
    // Menus may finish updating a live tilemap after enqueueing its copy.
    source[1] = 0x12345678;
    CHECK(CheckForSpaceForDma3Request(request) == 0);
    CHECK(!memcmp(source, dest, sizeof(source)));
    request = RequestDma3Fill(0x76543210, dest, sizeof(dest), 1);
    CHECK(request >= 0 && CheckForSpaceForDma3Request(-1) == 0);
    for (unsigned i = 0; i < ARRAY_COUNT(dest); ++i) CHECK(dest[i] == 0x76543210);
    // A valid host tilemap has no relationship to the GBA's IWRAM address range.
    struct BgTemplate bg = {.bg = 1, .mapBaseIndex = 29};
    InitGpuRegManager();
    InitBgsFromTemplates(0, &bg, 1);
    SetBgTilemapBuffer(1, source);
    CHECK(!IsTileMapOutsideWram(1));
    CopyBgTilemapBufferToVram(1);
    CHECK(CheckForSpaceForDma3Request(-1) == 0);
    CHECK(!memcmp(gNativeVram + 29 * BG_SCREEN_SIZE, source, BG_SCREEN_SIZE));
    puts("native graphics transfers complete without hardware interrupts");
    return 0;
}
