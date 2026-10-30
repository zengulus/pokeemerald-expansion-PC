#ifndef RHH_PC_SAVE_FORMAT_H
#define RHH_PC_SAVE_FORMAT_H

#include <stddef.h>
#include <stdint.h>

/* A generation is an opaque, complete game and mod snapshot. The shared
 * payload is for state that deliberately survives ordinary save rollback. */
struct RhhPcSaveBlob {
    const uint8_t *data;
    size_t size;
};
struct RhhPcSaveGeneration {
    uint64_t generation;
    struct RhhPcSaveBlob payload;
};
struct RhhPcSaveImage {
    struct RhhPcSaveGeneration slots[2];
    struct RhhPcSaveBlob shared;
};
struct RhhPcSaveDecoded {
    struct RhhPcSaveImage image;
    /* A corrupt generation is unavailable; the other may still be valid. */
    unsigned valid_slots;
};

/* Encode allocates *out; the caller frees it. Decode borrows the input until
 * the caller is done with the views. Failure leaves output arguments alone. */
int RhhPcSaveEncode(const struct RhhPcSaveImage *image, uint8_t **out, size_t *size);
int RhhPcSaveDecode(const uint8_t *data, size_t size, struct RhhPcSaveDecoded *out);

#endif
