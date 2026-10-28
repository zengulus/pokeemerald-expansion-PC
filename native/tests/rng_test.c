#include <stdio.h>
#include "global.h"
#include "random.h"

int main(void)
{
    // SFC32 mathematical reference, seed 12345, stream 1, 16 warmup steps.
    static const u32 expected[] = {3261053875u, 3741302563u, 2548279504u,
        1761287231u, 1303069860u, 2982468226u, 1275143710u, 4123647846u,
        914045125u, 1868390967u};
    SeedRng(12345);
    for (unsigned i = 0; i < ARRAY_COUNT(expected); ++i)
    {
        u32 actual = Random32();
        if (actual != expected[i])
        {
            fprintf(stderr, "RNG step %u: %u != %u\n", i, actual, expected[i]);
            return 1;
        }
    }
    puts("RHH RNG reference sequence passed");
    return 0;
}
