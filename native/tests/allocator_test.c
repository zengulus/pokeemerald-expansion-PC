#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "malloc.h"
#include "dma3.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "allocator: %s\n", #c); exit(1); } } while (0)
static unsigned Count(void)
{
    const struct MemBlock *head = HeapHead();
    unsigned count = 0;
    for (const struct MemBlock *block = head->next; block != head; block = block->next)
    {
        CHECK(block->allocated && block->magic == MALLOC_SYSTEM_ID);
        CHECK(block->next->prev == block && block->prev->next == block);
        ++count;
    }
    return count;
}

int main(void)
{
    InitHeap(gHeap, HEAP_SIZE);
    char location[] = "native allocator test";
    const unsigned size = HEAP_SIZE * 8;
    u8 *large = AllocZeroed_(size, location);
    CHECK((uintptr_t)large % __alignof__(max_align_t) == 0);
    CHECK(Count() == 1 && HeapHead()->next->size == size);
    CHECK(MemBlockLocation(HeapHead()->next) == location);
    for (unsigned i = 0; i < size; ++i) CHECK(large[i] == 0);
    memset(large, 0x5a, size);
    void *blocks[50];
    for (unsigned i = 0; i < ARRAY_COUNT(blocks); ++i)
    {
        blocks[i] = Alloc_(i + 1, location);
        CHECK((uintptr_t)blocks[i] % __alignof__(max_align_t) == 0);
    }
    CHECK(Count() == 51);
    Free(blocks[20]);
    CHECK(Count() == 50);
    Free(large);
    CHECK(Count() == 49);
    Free(NULL);
    InitHeap(gHeap, HEAP_SIZE);
    CHECK(Count() == 0);
    u32 output[16] = {0};
    u32 *queued = Alloc(64);
    for (unsigned i = 0; i < 16; ++i) queued[i] = i * 7919;
    CHECK(RequestDma3Copy(queued, output, 64, 1) >= 0);
    Free(queued);
    CHECK(Count() == 0 && output[1] == 0);
    u32 *other = AllocZeroed(64);
    CHECK(other != queued);
    ProcessDma3Requests();
    for (unsigned i = 0; i < 16; ++i) CHECK(output[i] == i * 7919);
    Free(other);
    u32 original[16];
    for (unsigned i = 0; i < 16; ++i) original[i] = i + 1234;
    u32 *intermediate = Alloc(64);
    CHECK(RequestDma3Copy(original, intermediate, 64, 1) >= 0);
    CHECK(RequestDma3Copy(intermediate, output, 64, 1) >= 0);
    Free(intermediate);
    ProcessDma3Requests();
    CHECK(!memcmp(original, output, 64) && Count() == 0);
    puts("native allocator alignment, large blocks, reset and queued-transfer lifetimes passed");
    return 0;
}
