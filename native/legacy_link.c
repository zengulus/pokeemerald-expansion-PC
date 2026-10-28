// The native platform has no physical GameCube/GBA cartridge transport.
// Report a disconnected cable, and never execute downloaded ARM code.
#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "libgcnmultiboot.h"

void GameCubeMultiBoot_Init(struct GcmbStruct *state)
{
    memset(state, 0, sizeof(*state));
}
void GameCubeMultiBoot_Main(struct GcmbStruct *state) { state->gcmb_field_2 = 0; }
void GameCubeMultiBoot_HandleSerialInterrupt(struct GcmbStruct *state) { (void)state; }
void GameCubeMultiBoot_Quit(void) {}
void GameCubeMultiBoot_ExecuteProgram(struct GcmbStruct *state)
{
    (void)state;
    fputs("Cartridge multiboot is unavailable on the native platform.\n", stderr);
    abort();
}
int MultiBoot(struct MultiBootParam *params) { (void)params; return -1; }

struct BitUnPackArgs
{
    u16 length;
    u8 srcWidth;
    u8 dstWidth;
    u32 offset;
};

void BitUnPack(const void *input, void *output, const struct BitUnPackArgs *args)
{
    const u8 *src = input;
    u8 *dst = output;
    u32 word = 0;
    unsigned written = 0;
    unsigned mask = (1u << args->srcWidth) - 1;
    for (unsigned i = 0; i < args->length; ++i)
        for (unsigned bit = 0; bit < 8; bit += args->srcWidth)
        {
            u32 value = (src[i] >> bit) & mask;
            if (value || (args->offset & 0x80000000)) value += args->offset & 0x7fffffff;
            word |= value << written;
            written += args->dstWidth;
            if (written == 32)
            {
                memcpy(dst, &word, 4);
                dst += 4; written = 0; word = 0;
            }
        }
}
