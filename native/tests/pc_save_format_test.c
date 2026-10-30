#include "pc_save_format.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "PC save format: %s (line %d)\n", #condition, __LINE__); exit(1); \
} } while (0)

int main(void)
{
    const uint8_t a[] = {1, 2, 3}, b[] = {4, 5}, shared[] = {6, 7, 8, 9};
    struct RhhPcSaveImage input = {
        .slots = {{42, {a, sizeof(a)}}, {41, {b, sizeof(b)}}},
        .shared = {shared, sizeof(shared)}
    };
    uint8_t *bytes = NULL; size_t size = 0;
    CHECK(!RhhPcSaveEncode(&input, &bytes, &size));
    struct RhhPcSaveDecoded decoded = {0};
    CHECK(!RhhPcSaveDecode(bytes, size, &decoded));
    CHECK(decoded.valid_slots == 3 && decoded.image.slots[0].generation == 42
        && decoded.image.slots[1].generation == 41
        && !memcmp(decoded.image.slots[0].payload.data, a, sizeof(a))
        && !memcmp(decoded.image.slots[1].payload.data, b, sizeof(b))
        && !memcmp(decoded.image.shared.data, shared, sizeof(shared)));
    /* A damaged newest generation leaves the older generation available. */
    bytes[80] ^= 1;
    CHECK(!RhhPcSaveDecode(bytes, size, &decoded) && decoded.valid_slots == 2
        && decoded.image.slots[1].generation == 41);
    bytes[80] ^= 1;
    struct RhhPcSaveDecoded before = decoded;
    bytes[size - 1] ^= 1;
    CHECK(RhhPcSaveDecode(bytes, size, &decoded) && !memcmp(&decoded, &before, sizeof(decoded)));
    bytes[size - 1] ^= 1;
    bytes[16] ^= 1;
    CHECK(RhhPcSaveDecode(bytes, size, &decoded));
    bytes[16] ^= 1;
    CHECK(RhhPcSaveDecode(bytes, size - 1, &decoded));
    for (size_t truncated = 0; truncated < size; ++truncated)
    {
        struct RhhPcSaveDecoded unchangedView = decoded;
        CHECK(RhhPcSaveDecode(bytes, truncated, &decoded));
        CHECK(!memcmp(&decoded, &unchangedView, sizeof(decoded)));
    }
    for (size_t offset = 0; offset < 80; ++offset)
    {
        bytes[offset] ^= 1;
        CHECK(RhhPcSaveDecode(bytes, size, &decoded));
        bytes[offset] ^= 1;
    }
    input.slots[1].generation = 42;
    uint8_t *unchanged = bytes; size_t oldSize = size;
    CHECK(RhhPcSaveEncode(&input, &unchanged, &oldSize) && unchanged == bytes && oldSize == size);
    free(bytes);

    /* Game generations are not limited by the old flash-sector capacity. */
    size_t largeSize = 256 * 1024 + 17;
    uint8_t *large = malloc(largeSize);
    CHECK(large);
    for (size_t i = 0; i < largeSize; ++i) large[i] = (uint8_t)(i * 31);
    input.slots[0] = (struct RhhPcSaveGeneration){43, {large, largeSize}};
    input.slots[1] = (struct RhhPcSaveGeneration){42, {a, sizeof(a)}};
    CHECK(!RhhPcSaveEncode(&input, &bytes, &size));
    CHECK(!RhhPcSaveDecode(bytes, size, &decoded) && decoded.valid_slots == 3
        && decoded.image.slots[0].payload.size == largeSize
        && !memcmp(decoded.image.slots[0].payload.data, large, largeSize));
    free(bytes); free(large);
    return 0;
}
