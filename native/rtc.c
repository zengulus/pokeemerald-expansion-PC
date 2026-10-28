#include <time.h>
#include <stdio.h>
#include "global.h"
#include "siirtc.h"

static struct SiiRtcInfo sFixedRtc;
static bool8 sUseFixedRtc;

// A diagnostic clock makes input replays independent of the wall-clock RNG seed.
int NativeRtcSetFixed(const char *timestamp)
{
    unsigned year, month, day, hour, minute, second;
    int consumed = 0;
    static const unsigned monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (sscanf(timestamp, "%u-%u-%uT%u:%u:%u%n", &year, &month, &day,
               &hour, &minute, &second, &consumed) != 6 || timestamp[consumed]
        || year < 2000 || year > 2099 || month < 1 || month > 12
        || hour > 23 || minute > 59 || second > 59 || day < 1
        || day > monthDays[month - 1] + (month == 2 && year % 4 == 0))
        return -1;
    unsigned days = 0;
    for (unsigned y = 2000; y < year; ++y) days += 365 + (y % 4 == 0);
    for (unsigned m = 1; m < month; ++m) days += monthDays[m - 1] + (m == 2 && year % 4 == 0);
    sFixedRtc = (struct SiiRtcInfo){0};
    sFixedRtc.year = (year % 100 / 10) * 16 + year % 10;
    sFixedRtc.month = (month / 10) * 16 + month % 10;
    sFixedRtc.day = (day / 10) * 16 + day % 10;
    sFixedRtc.hour = (hour / 10) * 16 + hour % 10;
    sFixedRtc.minute = (minute / 10) * 16 + minute % 10;
    sFixedRtc.second = (second / 10) * 16 + second % 10;
    sFixedRtc.dayOfWeek = (6 + days + day - 1) % 7;
    sFixedRtc.status = SIIRTCINFO_24HOUR;
    sUseFixedRtc = TRUE;
    return 0;
}

static u8 Bcd(unsigned value) { return (value / 10) * 16 + value % 10; }
void SiiRtcUnprotect(void) {}
void SiiRtcProtect(void) {}
u8 SiiRtcProbe(void) { return 0; }
bool8 SiiRtcReset(void) { return TRUE; }
bool8 SiiRtcGetStatus(struct SiiRtcInfo *rtc)
{
    rtc->status = SIIRTCINFO_24HOUR;
    return TRUE;
}
bool8 SiiRtcSetStatus(struct SiiRtcInfo *rtc) { (void)rtc; return TRUE; }
bool8 SiiRtcGetDateTime(struct SiiRtcInfo *rtc)
{
    if (sUseFixedRtc) { *rtc = sFixedRtc; return TRUE; }
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (!local) return FALSE;
    rtc->year = Bcd((local->tm_year + 1900) % 100);
    rtc->month = Bcd(local->tm_mon + 1);
    rtc->day = Bcd(local->tm_mday);
    rtc->dayOfWeek = local->tm_wday;
    rtc->hour = Bcd(local->tm_hour);
    rtc->minute = Bcd(local->tm_min);
    rtc->second = Bcd(local->tm_sec);
    rtc->status = SIIRTCINFO_24HOUR;
    return TRUE;
}
bool8 SiiRtcGetTime(struct SiiRtcInfo *rtc) { return SiiRtcGetDateTime(rtc); }
