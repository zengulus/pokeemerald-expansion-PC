#include "pc_save_format.h"
#include <stdlib.h>
#include <string.h>

enum { HEADER_SIZE = 80, SLOT_SIZE = 24 };
static const uint8_t sMagic[8] = {'R','H','H','P','C','S','0','1'};

static uint32_t Read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint64_t Read64(const uint8_t *p) { return Read32(p) | (uint64_t)Read32(p + 4) << 32; }
static void Write32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (8 * i));
}
static void Write64(uint8_t *p, uint64_t value)
{
    Write32(p, (uint32_t)value); Write32(p + 4, (uint32_t)(value >> 32));
}
static uint32_t Checksum(const uint8_t *bytes, size_t size)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i)
    {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
static int ValidBlob(struct RhhPcSaveBlob blob)
{ return (!!blob.data == !!blob.size); }

int RhhPcSaveEncode(const struct RhhPcSaveImage *image, uint8_t **out, size_t *size)
{
    if (!image || !out || !size || !ValidBlob(image->shared)) return -1;
    size_t total = HEADER_SIZE;
    for (unsigned i = 0; i < 2; ++i)
    {
        const struct RhhPcSaveGeneration *slot = &image->slots[i];
        if (!ValidBlob(slot->payload) || (!!slot->generation != !!slot->payload.size)
            || slot->payload.size > SIZE_MAX - total) return -1;
        total += slot->payload.size;
    }
    if (image->slots[0].generation && image->slots[0].generation == image->slots[1].generation)
        return -1;
    if (image->shared.size > SIZE_MAX - total) return -1;
    total += image->shared.size;
    uint8_t *bytes = calloc(total, 1);
    if (!bytes) return -1;
    memcpy(bytes, sMagic, sizeof(sMagic));
    Write32(bytes + 8, 1); Write32(bytes + 12, HEADER_SIZE);
    size_t at = HEADER_SIZE;
    for (unsigned i = 0; i < 2; ++i)
    {
        const struct RhhPcSaveGeneration *slot = &image->slots[i];
        uint8_t *meta = bytes + 16 + SLOT_SIZE * i;
        Write64(meta, slot->generation); Write64(meta + 8, slot->payload.size);
        if (slot->payload.size)
        {
            Write32(meta + 16, Checksum(slot->payload.data, slot->payload.size));
            memcpy(bytes + at, slot->payload.data, slot->payload.size);
            at += slot->payload.size;
        }
    }
    Write64(bytes + 64, image->shared.size);
    if (image->shared.size)
    {
        Write32(bytes + 72, Checksum(image->shared.data, image->shared.size));
        memcpy(bytes + at, image->shared.data, image->shared.size);
    }
    Write32(bytes + 76, Checksum(bytes, 76));
    *out = bytes; *size = total; return 0;
}

int RhhPcSaveDecode(const uint8_t *data, size_t size, struct RhhPcSaveDecoded *out)
{
    if (!data || !out || size < HEADER_SIZE || memcmp(data, sMagic, sizeof(sMagic))
        || Read32(data + 8) != 1 || Read32(data + 12) != HEADER_SIZE
        || Read32(data + 76) != Checksum(data, 76)) return -1;
    struct RhhPcSaveDecoded decoded = {0};
    size_t at = HEADER_SIZE;
    for (unsigned i = 0; i < 2; ++i)
    {
        const uint8_t *meta = data + 16 + SLOT_SIZE * i;
        uint64_t generation = Read64(meta), length = Read64(meta + 8);
        uint32_t expected = Read32(meta + 16);
        if (Read32(meta + 20) || length > size - at || (!!generation != !!length)
            || (!length && expected)) return -1;
        if (length && Checksum(data + at, (size_t)length) == expected)
        {
            decoded.image.slots[i].generation = generation;
            decoded.image.slots[i].payload = (struct RhhPcSaveBlob){data + at, (size_t)length};
            decoded.valid_slots |= 1u << i;
        }
        at += (size_t)length;
    }
    if (decoded.image.slots[0].generation && decoded.image.slots[0].generation == decoded.image.slots[1].generation)
        return -1;
    uint64_t shared = Read64(data + 64); uint32_t checksum = Read32(data + 72);
    if (shared != size - at || (!shared && checksum)
        || (shared && Checksum(data + at, (size_t)shared) != checksum)) return -1;
    if (shared) decoded.image.shared = (struct RhhPcSaveBlob){data + at, (size_t)shared};
    *out = decoded; return 0;
}
