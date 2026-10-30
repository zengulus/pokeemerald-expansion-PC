#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "native_controls.h"

/* The values are the stable GBA register bits, kept independent of engine headers. */
static const uint16_t sButtons[CONTROL_COUNT] = {1, 2, 8, 4, 512, 256, 64, 128, 32, 16};
struct Binding { int first, second; };
static const struct Binding sDefaults[CONTROL_COUNT] = {
    {SDL_SCANCODE_Z, 0}, {SDL_SCANCODE_X, 0}, {SDL_SCANCODE_RETURN, 0},
    {SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT}, {SDL_SCANCODE_A, 0}, {SDL_SCANCODE_S, 0},
    {SDL_SCANCODE_UP, 0}, {SDL_SCANCODE_DOWN, 0}, {SDL_SCANCODE_LEFT, 0}, {SDL_SCANCODE_RIGHT, 0}
};
static const char *const sIds[CONTROL_COUNT] = {"a", "b", "start", "select", "l", "r", "up", "down", "left", "right"};
static struct Binding sBindings[CONTROL_COUNT];
static char *sPath;
static int sCapture = -1, sResult, sBlocked, sReleaseBarrier;

#ifdef _WIN32
static wchar_t *WidePath(const char *path)
{
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (!length) return NULL;
    wchar_t *wide = malloc((size_t)length * sizeof(*wide));
    if (!wide) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, length))
    { free(wide); return NULL; }
    return wide;
}
#endif
static FILE *OpenPath(const char *path, int write)
{
#ifdef _WIN32
    wchar_t *wide = WidePath(path);
    if (!wide) return NULL;
    FILE *file = _wfopen(wide, write ? L"wb" : L"rb");
    free(wide); return file;
#else
    return fopen(path, write ? "wb" : "rb");
#endif
}
static int ReplacePath(const char *from, const char *to)
{
#ifdef _WIN32
    wchar_t *source = WidePath(from), *target = WidePath(to);
    int ok = source && target && MoveFileExW(source, target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    free(source); free(target); return ok ? 0 : -1;
#else
    return rename(from, to);
#endif
}
static void RemovePath(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = WidePath(path);
    if (wide) { _wremove(wide); free(wide); }
#else
    remove(path);
#endif
}

static int Valid(const struct Binding *bindings)
{
    for (unsigned i = 0; i < CONTROL_COUNT; ++i)
    {
        int keys[] = {bindings[i].first, bindings[i].second};
        if (!keys[0]) return 0;
        for (unsigned k = 0; k < 2; ++k)
        {
            int code = keys[k];
            if (!code && k) continue;
            if (code <= 0 || code >= SDL_NUM_SCANCODES || code == SDL_SCANCODE_ESCAPE
                || !SDL_GetScancodeName((SDL_Scancode)code)[0]) return 0;
            for (unsigned j = 0; j <= i; ++j)
                for (unsigned l = 0; l < 2; ++l)
                    if ((j != i || l < k) && code == (l ? bindings[j].second : bindings[j].first)) return 0;
        }
    }
    return 1;
}
static int Save(const struct Binding *bindings)
{
    if (!sPath) return -1;
    size_t n = strlen(sPath);
    char *tmp = malloc(n + 5);
    if (!tmp) return -1;
    memcpy(tmp, sPath, n); memcpy(tmp + n, ".tmp", 5);
    FILE *file = OpenPath(tmp, 1);
    if (!file) { free(tmp); return -1; }
    int ok = fprintf(file, "RHH-CONTROLS 1\n") > 0;
    for (unsigned i = 0; i < CONTROL_COUNT; ++i)
        if (fprintf(file, "%s %d %d\n", sIds[i], bindings[i].first, bindings[i].second) < 0) ok = 0;
    if (fflush(file)) ok = 0;
    if (fclose(file)) ok = 0;
    if (ok) ok = ReplacePath(tmp, sPath) == 0;
    if (!ok) RemovePath(tmp);
    free(tmp);
    return ok ? 0 : -1;
}
void NativeControlsInit(const char *path)
{
    NativeControlsShutdown();
    memcpy(sBindings, sDefaults, sizeof(sBindings));
    if (!path) return;
    sPath = malloc(strlen(path) + 1);
    if (!sPath) return;
    strcpy(sPath, path);
    FILE *file = OpenPath(path, 0);
    if (!file) return;
    char line[128]; struct Binding loaded[CONTROL_COUNT];
    int ok = fgets(line, sizeof(line), file) && !strcmp(line, "RHH-CONTROLS 1\n");
    for (unsigned i = 0; ok && i < CONTROL_COUNT; ++i)
    {
        char id[32], extra;
        ok = fgets(line, sizeof(line), file) && sscanf(line, "%31s %d %d %c", id,
            &loaded[i].first, &loaded[i].second, &extra) == 3 && !strcmp(id, sIds[i]);
    }
    if (ok && fgets(line, sizeof(line), file)) ok = 0;
    if (ferror(file)) ok = 0;
    fclose(file);
    if (ok && Valid(loaded)) memcpy(sBindings, loaded, sizeof(sBindings));
}
void NativeControlsShutdown(void)
{
    free(sPath); sPath = NULL; sCapture = -1; sResult = CONTROL_WAITING; sBlocked = sReleaseBarrier = 0;
}
uint16_t NativeControlsRead(const uint8_t *keys, int count)
{
    uint16_t result = 0;
    if (!keys || count <= 0) return 0;
    if (sBlocked && (sBlocked >= count || !keys[sBlocked])) sBlocked = 0;
    if (sCapture >= 0) return 0;
    if (sReleaseBarrier)
    {
        for (unsigned i = 0; i < CONTROL_COUNT; ++i)
        {
            int a = sBindings[i].first, b = sBindings[i].second;
            if ((a && a < count && keys[a]) || (b && b < count && keys[b])) return 0;
        }
        sReleaseBarrier = 0;
    }
    for (unsigned i = 0; i < CONTROL_COUNT; ++i)
    {
        int a = sBindings[i].first, b = sBindings[i].second;
        if ((a && a < count && a != sBlocked && keys[a]) || (b && b < count && b != sBlocked && keys[b]))
            result |= sButtons[i];
    }
    return result;
}
void NativeControlsBeginCapture(unsigned action)
{ if (action < CONTROL_COUNT) { sCapture = action; sResult = CONTROL_WAITING; } }
void NativeControlsCancelCapture(void)
{ sCapture = -1; sResult = CONTROL_CANCELLED; sReleaseBarrier = 1; }
int NativeControlsCaptureResult(void)
{ int result = sResult; sResult = CONTROL_WAITING; return result; }
int NativeControlsKeyDown(int scancode, int repeat)
{
    if (sCapture < 0 || repeat) return sCapture >= 0;
    if (scancode == SDL_SCANCODE_ESCAPE) { NativeControlsCancelCapture(); return 1; }
    if (scancode <= 0 || scancode >= SDL_NUM_SCANCODES || !SDL_GetScancodeName((SDL_Scancode)scancode)[0]) return 1;
    struct Binding next[CONTROL_COUNT]; memcpy(next, sBindings, sizeof(next));
    int old = next[sCapture].first;
    for (unsigned i = 0; i < CONTROL_COUNT; ++i)
    {
        if ((int)i == sCapture) continue;
        if (next[i].first == scancode) next[i].first = old;
        else if (next[i].second == scancode) next[i].second = 0;
    }
    next[sCapture] = (struct Binding){scancode, 0};
    if (!Valid(next) || Save(next)) sResult = CONTROL_SAVE_FAILED;
    else { memcpy(sBindings, next, sizeof(next)); sResult = CONTROL_CHANGED; }
    sBlocked = scancode; sCapture = -1; sReleaseBarrier = 1;
    return 1;
}
int NativeControlsReset(void)
{
    if (Save(sDefaults)) return -1;
    memcpy(sBindings, sDefaults, sizeof(sBindings));
    sReleaseBarrier = 1;
    return 0;
}
static const char *KeyName(int code)
{
    switch (code)
    {
    case SDL_SCANCODE_RETURN: return "Enter";
    case SDL_SCANCODE_KP_ENTER: return "Keypad Enter";
    case SDL_SCANCODE_LEFTBRACKET: return "Left bracket";
    case SDL_SCANCODE_RIGHTBRACKET: return "Right bracket";
    case SDL_SCANCODE_BACKSLASH: return "Backslash";
    case SDL_SCANCODE_APOSTROPHE: return "Apostrophe";
    case SDL_SCANCODE_GRAVE: return "Grave";
    case SDL_SCANCODE_SEMICOLON: return "Semicolon";
    case SDL_SCANCODE_EQUALS: return "Equals";
    case SDL_SCANCODE_COMMA: return "Comma";
    case SDL_SCANCODE_PERIOD: return "Period";
    case SDL_SCANCODE_MINUS: return "Minus";
    case SDL_SCANCODE_SLASH: return "Slash";
    default: break;
    }
    return SDL_GetScancodeName((SDL_Scancode)code);
}
const char *NativeControlsKeyName(unsigned action, unsigned index)
{
    if (action >= CONTROL_COUNT || index > 1) return "";
    int code = index ? sBindings[action].second : sBindings[action].first;
    return code ? KeyName(code) : "";
}
const char *NativeControlsName(unsigned action)
{
    if (action >= CONTROL_COUNT) return "";
    if (sBindings[action].first == SDL_SCANCODE_LSHIFT && sBindings[action].second == SDL_SCANCODE_RSHIFT)
        return "Shift";
    return NativeControlsKeyName(action, 0);
}
