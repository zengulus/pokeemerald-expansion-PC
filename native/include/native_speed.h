#ifndef RHH_NATIVE_SPEED_H
#define RHH_NATIVE_SPEED_H
/* Extra simulation steps for 1x, 1.5x, 2x and 3x; phase is per context. */
static inline unsigned NativeSpeedExtraSteps(unsigned setting, unsigned *phase)
{
    static const unsigned quarters[] = {0, 2, 4, 8};
    if (setting > 3) setting = 0;
    *phase += quarters[setting];
    unsigned extra = *phase / 4;
    *phase %= 4;
    return extra;
}
#endif
