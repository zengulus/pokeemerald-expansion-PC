#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "global.h"

static _Noreturn void ReportFailure(const void *caller, const char *format, va_list args)
{
    fprintf(stderr, "Native engine assertion at %p: ", caller);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    fflush(stderr);
    abort();
}

void AssertfCrashScreen(const void *caller, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    ReportFailure(caller, format, args);
}

_Noreturn void FatalfCrashScreen(const void *caller, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    ReportFailure(caller, format, args);
}
