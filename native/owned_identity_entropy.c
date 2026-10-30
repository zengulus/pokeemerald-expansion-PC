#include <stdint.h>
#include "owned_identity_ledger.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif
int NativeOwnedEntropy(void *out, uint32_t size)
{
    if (!out || !size) return -1;
    HMODULE library = LoadLibraryExA("advapi32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) return -1;
    typedef BOOLEAN (WINAPI *RandomBytes)(PVOID, ULONG);
    union { FARPROC address; RandomBytes call; } randomBytes;
    randomBytes.address = GetProcAddress(library, "SystemFunction036");
    int result = !randomBytes.address || !randomBytes.call(out, size);
    FreeLibrary(library); return result ? -1 : 0;
}
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
int NativeOwnedEntropy(void *out, uint32_t size)
{
    if (!out || !size) return -1;
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    uint8_t *bytes = out; uint32_t at = 0;
    while (at < size)
    {
        ssize_t n = read(fd, bytes + at, size - at);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { close(fd); return -1; }
        at += n;
    }
    return close(fd) ? -1 : 0;
}
#endif
