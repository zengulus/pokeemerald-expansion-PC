#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "sprite.h"
#include "battle_anim.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "sprite callback: %s\n", #c); exit(1); } } while (0)
static void Next(struct Sprite *sprite) { ++sprite->data[0]; }

int main(void)
{
    struct Sprite sprite = {0};
    sprite.data[0] = 10;
    StoreSpriteCallbackInData6(&sprite, Next);
    struct Sprite copy = sprite;
    SetCallbackToStoredInData6(&copy);
    CHECK(copy.callback == Next);
    copy.callback(&copy);
    CHECK(copy.data[0] == 11 && sprite.data[0] == 10);
    static const u32 pixels[] = {0x12345678, 0xabcdef00};
    SetSpritePointerArg(&sprite, 3, pixels);
    copy = sprite;
    CHECK(GetSpritePointerArg(&copy, 3) == pixels);
    CHECK(((const u32 *)GetSpritePointerArg(&copy, 3))[1] == 0xabcdef00);
#if UINTPTR_MAX > UINT32_MAX
    CHECK((uintptr_t)Next > UINT32_MAX);
#endif
    puts("sprite full-width callback and copy checks passed");
    return 0;
}
