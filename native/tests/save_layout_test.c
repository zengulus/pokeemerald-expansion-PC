// Exercise production save validation, including functions local to save.c.
#include <stdio.h>
#include <stdlib.h>
#include "../../src/save.c"
#include "recorded_battle.h"

// Measured against the authoritative ARM apcs-gnu build and both host targets.
_Static_assert(sizeof(struct SaveBlock2) == 3884, "RHH save block 2 ABI");
_Static_assert(sizeof(struct SaveBlock3) == 4, "RHH save block 3 ABI");
_Static_assert(sizeof(struct Pokemon) == 100, "RHH party pokemon ABI");
#if UINTPTR_MAX == UINT32_MAX
// The native container identifies the ABI; scripts are rebound on Continue.
_Static_assert(sizeof(struct SaveBlock1) == 15568, "RHH save block 1 ABI");
_Static_assert(offsetof(struct SaveBlock1, flags) == 4720, "RHH flags ABI");
_Static_assert(offsetof(struct SaveBlock1, vars) == 5020, "RHH variables ABI");
#else
_Static_assert(sizeof(struct SaveBlock1) == 15840, "native x64 save block 1 ABI");
_Static_assert(offsetof(struct SaveBlock1, flags) == 4976, "native x64 flags ABI");
_Static_assert(offsetof(struct SaveBlock1, vars) == 5276, "native x64 variables ABI");
#endif
_Static_assert(offsetof(struct SaveBlock2, encryptionKey) == 172, "RHH encryption ABI");
_Static_assert(sizeof(struct RecordedBattleSave) == 4092, "RHH recorded battle ABI");

struct SaveBlock3 gSaveblock3;
struct SaveBlock3 *gSaveBlock3Ptr = &gSaveblock3;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "save layout: %s\n", #condition); exit(1); } } while (0)

int main(void)
{
    struct SaveSectorLocation locations[NUM_SECTORS_PER_SLOT];
    u32 targets[NUM_SECTORS_PER_SLOT];
    memset(targets, 0x5a, sizeof(targets));
    for (unsigned i = 0; i < NUM_SECTORS_PER_SLOT; ++i)
        locations[i] = (struct SaveSectorLocation){&targets[i], sizeof(targets[i])};
    memset(gNativeFlash, 0xff, sizeof(gNativeFlash));
    gReadWriteSector = &gSaveDataBuffer;
    CHECK(GetSaveValidStatus(locations) == SAVE_STATUS_EMPTY);
    CopySaveSlotData(FULL_SAVE_SLOT, locations);
    for (unsigned i = 0; i < NUM_SECTORS_PER_SLOT; ++i) CHECK(targets[i] == 0x5a5a5a5a);

    // A forged signature must not allow an out-of-range id to index the layout.
    struct SaveSector *sector = (void *)gNativeFlash;
    sector->signature = SECTOR_SIGNATURE;
    sector->id = 0xffff;
    CHECK(GetSaveValidStatus(locations) == SAVE_STATUS_CORRUPT);
    CopySaveSlotData(FULL_SAVE_SLOT, locations);
    CHECK(targets[0] == 0x5a5a5a5a);

    // One valid slot, then a newer valid slot; verify the rotating-slot choice.
    for (unsigned slot = 0; slot < 2; ++slot)
    {
        for (unsigned i = 0; i < NUM_SECTORS_PER_SLOT; ++i)
        {
            sector = (void *)(gNativeFlash + (slot * NUM_SECTORS_PER_SLOT + i) * SECTOR_SIZE);
            memset(sector, 0, sizeof(*sector));
            sector->signature = SECTOR_SIGNATURE;
            sector->id = i;
            sector->counter = slot;
            u32 value = 1000 + slot * 100 + i;
            memcpy(sector->data, &value, sizeof(value));
            sector->checksum = CalculateChecksum(sector->data, sizeof(value));
        }
        CHECK(GetSaveValidStatus(locations) == SAVE_STATUS_OK);
        CHECK(gSaveCounter == slot);
        CopySaveSlotData(FULL_SAVE_SLOT, locations);
        for (unsigned i = 0; i < NUM_SECTORS_PER_SLOT; ++i) CHECK(targets[i] == 1000 + slot * 100 + i);
    }
    puts("RHH save sector validation and slot rotation passed");
    return 0;
}
