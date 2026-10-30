#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "native_controls.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static uint8_t keys[SDL_NUM_SCANCODES];
static uint16_t Read(void) { return NativeControlsRead(keys, SDL_NUM_SCANCODES); }
int main(int argc, char **argv)
{
    CHECK(argc == 2);
    NativeControlsInit(argv[1]);
    CHECK(!strcmp(NativeControlsName(CONTROL_SELECT), "Shift"));
    keys[SDL_SCANCODE_Z] = keys[SDL_SCANCODE_X] = keys[SDL_SCANCODE_RETURN] = 1;
    keys[SDL_SCANCODE_LSHIFT] = keys[SDL_SCANCODE_UP] = keys[SDL_SCANCODE_A] = 1;
    CHECK(Read() == (1 | 2 | 4 | 8 | 64 | 512));
    memset(keys, 0, sizeof(keys)); keys[SDL_SCANCODE_RSHIFT] = 1; CHECK(Read() == 4);
    memset(keys, 0, sizeof(keys));
    NativeControlsBeginCapture(CONTROL_A);
    CHECK(NativeControlsKeyDown(SDL_SCANCODE_X, 1));
    CHECK(NativeControlsCaptureResult() == CONTROL_WAITING);
    CHECK(NativeControlsKeyDown(SDL_SCANCODE_X, 0));
    CHECK(NativeControlsCaptureResult() == CONTROL_CHANGED);
    keys[SDL_SCANCODE_X] = 1; CHECK(Read() == 0); /* Captured key waits for release. */
    keys[SDL_SCANCODE_X] = 0; CHECK(Read() == 0);
    keys[SDL_SCANCODE_X] = 1; CHECK(Read() == 1);
    keys[SDL_SCANCODE_X] = 0; keys[SDL_SCANCODE_Z] = 1; CHECK(Read() == 2); /* Swap. */
    memset(keys, 0, sizeof(keys));
    NativeControlsShutdown(); NativeControlsInit(argv[1]);
    keys[SDL_SCANCODE_X] = 1; CHECK(Read() == 1); keys[SDL_SCANCODE_X] = 0;
    NativeControlsBeginCapture(CONTROL_B);
    CHECK(NativeControlsKeyDown(SDL_SCANCODE_ESCAPE, 0));
    CHECK(NativeControlsCaptureResult() == CONTROL_CANCELLED);
    CHECK(Read() == 0);
    keys[SDL_SCANCODE_Z] = 1; CHECK(Read() == 2); keys[SDL_SCANCODE_Z] = 0;
    NativeControlsBeginCapture(CONTROL_L);
    CHECK(NativeControlsKeyDown(SDL_SCANCODE_RSHIFT, 0));
    CHECK(NativeControlsCaptureResult() == CONTROL_CHANGED);
    keys[SDL_SCANCODE_RSHIFT] = 1; CHECK(Read() == 0); keys[SDL_SCANCODE_RSHIFT] = 0; CHECK(Read() == 0);
    keys[SDL_SCANCODE_RSHIFT] = 1; CHECK(Read() == 512); keys[SDL_SCANCODE_RSHIFT] = 0;
    keys[SDL_SCANCODE_LSHIFT] = 1; CHECK(Read() == 4); keys[SDL_SCANCODE_LSHIFT] = 0;
    CHECK(!NativeControlsReset());
    NativeControlsShutdown(); NativeControlsInit(argv[1]);
    keys[SDL_SCANCODE_RSHIFT] = 1; CHECK(Read() == 4); keys[SDL_SCANCODE_RSHIFT] = 0;
    NativeControlsShutdown();
    FILE *file = fopen(argv[1], "wb"); CHECK(file);
    CHECK(fputs("RHH-CONTROLS 1\na 29 0\n", file) >= 0 && !fclose(file));
    NativeControlsInit(argv[1]);
    keys[SDL_SCANCODE_Z] = 1; CHECK(Read() == 1); keys[SDL_SCANCODE_Z] = 0;
    char unicodePath[1024];
    CHECK(snprintf(unicodePath, sizeof(unicodePath), "%s.\xc3\xa9", argv[1]) < (int)sizeof(unicodePath));
    NativeControlsShutdown(); NativeControlsInit(unicodePath);
    NativeControlsBeginCapture(CONTROL_A); CHECK(NativeControlsKeyDown(SDL_SCANCODE_Q, 0));
    CHECK(NativeControlsCaptureResult() == CONTROL_CHANGED);
    NativeControlsShutdown(); NativeControlsInit(unicodePath);
    CHECK(!strcmp(NativeControlsName(CONTROL_A), "Q"));
    NativeControlsShutdown(); NativeControlsInit("/missing-rhh-test-dir/controls.cfg");
    NativeControlsBeginCapture(CONTROL_A); CHECK(NativeControlsKeyDown(SDL_SCANCODE_X, 0));
    CHECK(NativeControlsCaptureResult() == CONTROL_SAVE_FAILED);
    CHECK(Read() == 0);
    keys[SDL_SCANCODE_Z] = 1; CHECK(Read() == 1);
    NativeControlsShutdown();
    puts("keyboard defaults, rebinding, swaps, persistence, reset and failure recovery passed");
    return 0;
}
