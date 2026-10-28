#include "global.h"
#include "native.h"
#include <assert.h>

#define DMA_SRC_MASK (3 << 7)
#define DMA_DEST_MASK (3 << 5)

ALIGNED(16) unsigned char gNativeRegisters[0x400];
ALIGNED(16) unsigned char gNativePalette[0x400];
ALIGNED(16) unsigned char gNativeVram[0x18000];
ALIGNED(16) unsigned char gNativeOam[0x400];
ALIGNED(16) unsigned char gNativeFlash[131072];
struct SoundInfo *SOUND_INFO_PTR;
unsigned short INTR_CHECK;
void *INTR_VECTOR;

// Keep host pointers out of packed register storage. Only control bits are
// reflected there so existing DmaStop calls continue to work.
static struct NativeDma
{
    const u8 *src;
    u8 *dst;
    u8 *initialDst;
    u32 count;
    u16 control;
} sDma[4];

void NativeRunDma(unsigned timing)
{
    for (unsigned ch = 0; ch < 4; ++ch)
    {
        struct NativeDma *dma = &sDma[ch];
        vu16 *reg = (vu16 *)(REG_BASE + 0xb0 + ch * 12 + 10);
        if (!(*reg & DMA_ENABLE))
            dma->control &= ~DMA_ENABLE;
        if (!(dma->control & DMA_ENABLE) || ((dma->control >> 12) & 3) != timing)
            continue;
        unsigned width = (dma->control & DMA_32BIT) ? 4 : 2;
        for (u32 i = 0; i < dma->count; ++i)
        {
            memcpy(dma->dst, dma->src, width);
            switch (dma->control & DMA_SRC_MASK)
            {
            case DMA_SRC_INC: dma->src += width; break;
            case DMA_SRC_DEC: dma->src -= width; break;
            }
            switch (dma->control & DMA_DEST_MASK)
            {
            case DMA_DEST_INC:
            case DMA_DEST_RELOAD: dma->dst += width; break;
            case DMA_DEST_DEC: dma->dst -= width; break;
            }
        }
        if ((dma->control & DMA_REPEAT) && timing != 0)
        {
            if ((dma->control & DMA_DEST_MASK) == DMA_DEST_RELOAD)
                dma->dst = dma->initialDst;
        }
        else
        {
            dma->control &= ~DMA_ENABLE;
            *reg &= ~DMA_ENABLE;
        }
    }
}

void DmaSet(int ch, const volatile void *src, volatile void *dst, u32 control)
{
    assert(ch >= 0 && ch < 4);
    struct NativeDma *dma = &sDma[ch];
    dma->src = (const u8 *)src;
    dma->dst = dma->initialDst = (u8 *)dst;
    dma->count = control & (ch == 3 ? 0xffff : 0x3fff);
    if (!dma->count)
        dma->count = ch == 3 ? 0x10000 : 0x4000;
    dma->control = control >> 16;
    *(vu32 *)(REG_BASE + 0xb0 + ch * 12 + 8) = control;
    NativeRunDma(0);
}

void CpuSet(const void *src, void *dst, u32 control)
{
    unsigned width = (control & CPU_SET_32BIT) ? 4 : 2;
    u8 value[4];
    const u8 *in = src;
    u8 *out = dst;
    u32 count = control & 0x1fffff;
    if (!count) return;
    memcpy(value, in, width);
    for (u32 i = 0; i < count; ++i, out += width)
    {
        if (control & CPU_SET_SRC_FIXED)
            memcpy(out, value, width);
        else
        {
            memcpy(out, in, width);
            in += width;
        }
    }
}

void CpuFastSet(const void *src, void *dst, u32 control)
{
    u32 count = ((control & 0x1fffff) + 7) & ~7u;
    CpuSet(src, dst, (control & CPU_SET_SRC_FIXED) | CPU_SET_32BIT | count);
}

static u32 ReadHeader(const u8 *src)
{
    return (u32)src[1] | (u32)src[2] << 8 | (u32)src[3] << 16;
}

void LZ77UnCompWram(const u32 *input, void *output)
{
    const u8 *src = (const u8 *)input;
    u8 *dst = output;
    u32 length = ReadHeader(src), written = 0;
    src += 4;
    while (written < length)
    {
        unsigned flags = *src++;
        for (unsigned bit = 0; bit < 8 && written < length; ++bit, flags <<= 1)
        {
            if (!(flags & 0x80)) dst[written++] = *src++;
            else
            {
                unsigned a = *src++, b = *src++;
                unsigned count = (a >> 4) + 3;
                unsigned back = ((a & 15) << 8 | b) + 1;
                assert(back <= written);
                while (count-- && written < length)
                {
                    dst[written] = dst[written - back];
                    ++written;
                }
            }
        }
    }
}

void LZ77UnCompVram(const u32 *src, void *dst) { LZ77UnCompWram(src, dst); }

void RLUnCompWram(const u32 *input, void *output)
{
    const u8 *src = (const u8 *)input;
    u8 *dst = output;
    u32 length = ReadHeader(src), written = 0;
    src += 4;
    while (written < length)
    {
        unsigned control = *src++;
        unsigned count = (control & 127) + ((control & 128) ? 3 : 1);
        u8 value = (control & 128) ? *src++ : 0;
        while (count-- && written < length)
            dst[written++] = (control & 128) ? value : *src++;
    }
}

void RLUnCompVram(const u32 *src, void *dst) { RLUnCompWram(src, dst); }
