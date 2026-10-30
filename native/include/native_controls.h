#ifndef RHH_NATIVE_CONTROLS_H
#define RHH_NATIVE_CONTROLS_H

#include <stdint.h>

enum NativeControl {
    CONTROL_A, CONTROL_B, CONTROL_START, CONTROL_SELECT, CONTROL_L, CONTROL_R,
    CONTROL_UP, CONTROL_DOWN, CONTROL_LEFT, CONTROL_RIGHT, CONTROL_COUNT
};
enum NativeControlCapture { CONTROL_WAITING, CONTROL_CHANGED, CONTROL_CANCELLED, CONTROL_SAVE_FAILED };

/* Paths are UTF-8 and remain caller-owned. NULL disables persistence. */
void NativeControlsInit(const char *path);
void NativeControlsShutdown(void);
uint16_t NativeControlsRead(const uint8_t *keys, int count);
void NativeControlsBeginCapture(unsigned action);
int NativeControlsCaptureResult(void);
void NativeControlsCancelCapture(void);
/* SDL scancode from a non-repeating key-down event. Returns true if consumed. */
int NativeControlsKeyDown(int scancode, int repeat);
int NativeControlsReset(void);
const char *NativeControlsName(unsigned action);
/* Individual key names are borrowed and empty when the key is absent. */
const char *NativeControlsKeyName(unsigned action, unsigned index);

#endif
