#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <malloc.h>
#endif
#include "global.h"
#include "malloc.h"
#include "dma3.h"

// Some save relocation code uses this legacy region as scratch storage. Game
// allocations themselves use the host heap and have no GBA heap-size ceiling.
u8 gHeap[HEAP_SIZE] ALIGNED(__alignof__(max_align_t));
static struct MemBlock sHead;
static struct MemBlock sRetired;
_Static_assert(offsetof(struct MemBlock, data) == sizeof(struct MemBlock), "native allocation header");

static void EnsureHeap(void)
{
    if (!sHead.next)
    {
        sHead.next = sHead.prev = &sHead;
        sRetired.next = sRetired.prev = &sRetired;
    }
}

static void UnlinkBlock(struct MemBlock *block)
{
    block->prev->next = block->next;
    block->next->prev = block->prev;
}

static void ReleaseBlock(struct MemBlock *block)
{
    UnlinkBlock(block);
    block->magic = 0;
#ifdef _WIN32
    _aligned_free(block);
#else
    free(block);
#endif
}

void NativeCollectRetiredAllocations(void)
{
    EnsureHeap();
    struct MemBlock *next;
    for (struct MemBlock *block = sRetired.next; block != &sRetired; block = next)
    {
        next = block->next;
        if (!NativeDma3ReferencesBuffer(block->data, block->size ? block->size : 1))
            ReleaseBlock(block);
    }
}

void Free(void *pointer)
{
    if (!pointer) return;
    EnsureHeap();
    for (struct MemBlock *block = sHead.next; block != &sHead; block = block->next)
    {
        if (block->data != pointer) continue;
        assertf(block->magic == MALLOC_SYSTEM_ID && block->allocated, "Invalid native allocation");
        block->allocated = FALSE;
        // RHH queues graphics transfers before freeing their buffers. Keep
        // those bytes alive until ordered VBlank copies finish or are cleared.
        if (NativeDma3ReferencesBuffer(pointer, block->size ? block->size : 1))
        {
            UnlinkBlock(block);
            block->prev = sRetired.prev;
            block->next = &sRetired;
            sRetired.prev->next = block;
            sRetired.prev = block;
        }
        else
            ReleaseBlock(block);
        return;
    }
    fatalf("Free called with an unowned native pointer: %p", pointer);
}

void InitHeap(void *legacyRegion, u32 legacySize)
{
    (void)legacyRegion;
    (void)legacySize;
    EnsureHeap();
    ClearDma3Requests();
    while (sHead.next != &sHead)
        Free(sHead.next->data);
}

void *AllocUnchecked_(u32 size, const char *location)
{
    EnsureHeap();
    size_t bytes = size ? size : 1;
    if (bytes > SIZE_MAX - sizeof(struct MemBlock)) return NULL;
#ifdef _WIN32
    struct MemBlock *block = _aligned_malloc(sizeof(*block) + bytes, __alignof__(max_align_t));
#else
    struct MemBlock *block = malloc(sizeof(*block) + bytes);
#endif
    if (!block) return NULL;
    block->allocated = TRUE;
    block->magic = MALLOC_SYSTEM_ID;
    block->size = size;
    block->location = location;
    block->prev = sHead.prev;
    block->next = &sHead;
    sHead.prev->next = block;
    sHead.prev = block;
    return block->data;
}

void *Alloc_(u32 size, const char *location)
{
    void *pointer = AllocUnchecked_(size, location);
    if (!pointer) fatalf("Native allocation failed: %u bytes at %s", size, location ? location : "unknown");
    return pointer;
}

void *AllocZeroedUnchecked_(u32 size, const char *location)
{
    void *pointer = AllocUnchecked_(size, location);
    if (pointer) memset(pointer, 0, size);
    return pointer;
}

void *AllocZeroed_(u32 size, const char *location)
{
    void *pointer = AllocZeroedUnchecked_(size, location);
    if (!pointer) fatalf("Native allocation failed: %u bytes at %s", size, location ? location : "unknown");
    return pointer;
}

const struct MemBlock *HeapHead(void) { EnsureHeap(); return &sHead; }
const char *MemBlockLocation(const struct MemBlock *block) { return block->allocated ? block->location : NULL; }

void PrintHeap(void)
{
    EnsureHeap();
    for (const struct MemBlock *block = sHead.next; block != &sHead; block = block->next)
        fprintf(stderr, "native allocation %p: %zu bytes at %s\n", (const void *)block->data,
                block->size, block->location ? block->location : "unknown");
}
