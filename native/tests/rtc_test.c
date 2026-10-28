#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "native.h"
#include "siirtc.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "RTC: %s\n", #c); exit(1); } } while (0)

int main(void)
{
    struct SiiRtcInfo rtc;
    CHECK(NativeRtcSetFixed("2024-02-29T23:59:59") == 0);
    CHECK(SiiRtcGetDateTime(&rtc));
    CHECK(rtc.year == 0x24 && rtc.month == 2 && rtc.day == 0x29);
    CHECK(rtc.hour == 0x23 && rtc.minute == 0x59 && rtc.second == 0x59);
    CHECK(rtc.dayOfWeek == 4 && rtc.status == SIIRTCINFO_24HOUR);
    CHECK(NativeRtcSetFixed("2023-02-29T12:00:00") == -1);
    CHECK(NativeRtcSetFixed("2024-13-01T12:00:00") == -1);
    CHECK(NativeRtcSetFixed("2024-02-29T24:00:00") == -1);
    CHECK(NativeRtcSetFixed("2024-02-29T12:00:00garbage") == -1);
    CHECK(NativeRtcSetFixed("2000-01-01T00:00:00") == 0);
    CHECK(SiiRtcGetDateTime(&rtc) && rtc.dayOfWeek == 6);
    CHECK(NativeRtcSetFixed("2026-10-28T12:00:00") == 0);
    CHECK(SiiRtcGetDateTime(&rtc) && rtc.dayOfWeek == 3);
    puts("RTC BCD dates, leap-year validation and reproducible clock passed");
    return 0;
}
