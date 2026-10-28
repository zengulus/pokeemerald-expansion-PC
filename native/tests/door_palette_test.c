#include <stdio.h>
#include <stdlib.h>
#include "../../src/field_door.c"

int main(void)
{
    const u8 palettes[4] = {1, 3, 5, 7};
    u16 tiles[8];
    BuildDoorTiles(tiles, 100, palettes);
    for (unsigned i = 0; i < 8; ++i)
    {
        u16 expected = i < 4 ? (palettes[i] << 12) | (100 + i) : 7 << 12;
        if (tiles[i] != expected) return 1;
    }
    puts("door metatile palettes use exactly four source tiles");
    return 0;
}
