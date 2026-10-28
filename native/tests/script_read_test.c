#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "script.h"
#include "constants/script_commands.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "script operand: %s\n", #c); exit(1); } } while (0)
static void NativeFunction(void) {}

int main(void)
{
    unsigned object = 123;
    uintptr_t pointer = (uintptr_t)&object;
    u8 bytes[1 + sizeof(pointer) + 6] = {0};
    memcpy(bytes + 1, &pointer, sizeof(pointer));
    u8 *scalar = bytes + 1 + sizeof(pointer);
    scalar[0] = 0x98; scalar[1] = 0xba; scalar[2] = 0xdc; scalar[3] = 0xfe;
    scalar[4] = 0xcd; scalar[5] = 0xab;
    struct ScriptContext ctx = {0};
    ctx.scriptPtr = bytes + 1; // Deliberately unaligned.
    CHECK(ScriptPeekPointer(&ctx) == pointer && ctx.scriptPtr == bytes + 1);
    CHECK(ScriptReadPointer(&ctx) == pointer);
    CHECK(ctx.scriptPtr == scalar);
    CHECK(ScriptPeekWord(&ctx) == 0xfedcba98);
    CHECK(ScriptReadWord(&ctx) == 0xfedcba98);
    CHECK(ScriptReadHalfword(&ctx) == 0xabcd);
    CHECK(ctx.scriptPtr == bytes + sizeof(bytes));
    CHECK((uintptr_t)T1_READ_PTR(bytes + 1) == pointer);
    CHECK((uintptr_t)T2_READ_PTR(bytes + 1) == pointer);
    CHECK(sizeof(ctx.data[0]) == sizeof(uintptr_t));
    u8 call[1 + sizeof(void *)] = {SCR_OP_CALLNATIVE};
    uintptr_t function = (uintptr_t)NativeFunction;
    memcpy(call + 1, &function, sizeof(function));
    CHECK(Script_MatchesCallNative(call, NativeFunction, FALSE));
    CHECK(Script_MatchesCallNative(call, NativeFunction, TRUE));
    CHECK(!Script_MatchesCallNative(call, main, FALSE));
    puts("script unaligned full-width pointers and fixed-width scalars passed");
    return 0;
}
