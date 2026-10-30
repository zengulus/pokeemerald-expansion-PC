#ifndef RHH_NATIVE_DISPLAY_CAPTURE_H
#define RHH_NATIVE_DISPLAY_CAPTURE_H
#include <stdint.h>
#include "mod_api.h"
/* Returns the latest complete sample and its engine map ID. */
int NativeDisplayFrameCopy(struct RhhWorldDisplayScanline *rows, uint32_t capacity,
                           uint32_t *map_id, uint64_t *sequence, uint64_t *run_id);
#endif
