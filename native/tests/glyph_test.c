// Golden tile pixels and boundary guards for the production text blitter.
#include <stdio.h>
#include <stdlib.h>
#include "../../src/text.c"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "glyph: %s\n", #c); exit(1); } } while (0)

int main(void)
{
    u32 source[8];
    u32 guarded[18];
    for (unsigned i = 0; i < ARRAY_COUNT(source); ++i) source[i] = 0x11111111;
    memset(guarded, 0, sizeof(guarded));
    guarded[0] = guarded[9] = 0xa3a3a3a3;
    GLYPH_COPY((u8 *)&guarded[1], 32, 0, 0, source, 8, 8);
    for (unsigned i = 1; i <= 8; ++i) CHECK(guarded[i] == 0x11111111);
    CHECK(guarded[0] == 0xa3a3a3a3 && guarded[9] == 0xa3a3a3a3);

    memset(guarded + 1, 0, 8 * sizeof(u32));
    GLYPH_COPY((u8 *)&guarded[1], 32, 5, 0, source, 3, 8);
    for (unsigned i = 1; i <= 8; ++i) CHECK(guarded[i] == 0x11100000);
    CHECK(guarded[0] == 0xa3a3a3a3 && guarded[9] == 0xa3a3a3a3);

    memset(guarded, 0, sizeof(guarded));
    guarded[0] = guarded[17] = 0xa3a3a3a3;
    GLYPH_COPY((u8 *)&guarded[1], 64, 7, 0, source, 3, 8);
    for (unsigned i = 1; i <= 8; ++i)
    {
        CHECK(guarded[i] == 0x10000000);
        CHECK(guarded[i + 8] == 0x11);
    }
    CHECK(guarded[0] == 0xa3a3a3a3 && guarded[17] == 0xa3a3a3a3);
    GLYPH_COPY(NULL, 0, 0, 0, NULL, 0, 8);
    GLYPH_COPY(NULL, 0, 0, 0, NULL, 8, 0);
    puts("text glyph aligned, clipped, spanning and boundary checks passed");
    return 0;
}
