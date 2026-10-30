/* Asynchronous, panel-owned native file chooser for trusted mod UI. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#undef TRANSPARENT
#elif defined(RHH_HAVE_DBUS_PICKER)
#include <dbus/dbus.h>
#endif
#include <SDL2/SDL_syswm.h>
#ifdef _WIN32
#undef TRANSPARENT
#endif
#include "global.h"
#include "main.h"
#include "mod_api.h"
#include "mod_host.h"
#include "mod_ui.h"
#include "native.h"

static struct {
    SDL_Thread *thread;
    SDL_atomic_t done, cancel;
    RhhContentKey owner;
    u32 kind, state;
    int discard;
    char path[RHH_FILE_DIALOG_PATH_SIZE];
} sDialog;
static char sParent[64];
#ifdef _WIN32
static HWND sWindow;
#endif
void RhhFileDialogSetWindow(struct SDL_Window *window)
{
    sParent[0] = 0;
#ifdef _WIN32
    sWindow = NULL;
#endif
    if (!window) return;
    SDL_SysWMinfo info; SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return;
#if defined(SDL_VIDEO_DRIVER_X11) && !defined(_WIN32)
    if (info.subsystem == SDL_SYSWM_X11) snprintf(sParent, sizeof(sParent), "x11:%lx", (unsigned long)info.info.x11.window);
#endif
#ifdef _WIN32
    if (info.subsystem == SDL_SYSWM_WINDOWS) sWindow = info.info.win.window;
#endif
}

#if defined(RHH_HAVE_DBUS_PICKER) && !defined(_WIN32)
/* Load D-Bus at runtime so a missing desktop integration library does not
 * prevent the game from starting. All D-Bus work stays on the worker thread. */
#define DBUS_FUNCTIONS(X) \
    X(dbus_bus_get_private) X(dbus_bus_add_match) X(dbus_connection_set_exit_on_disconnect) \
    X(dbus_connection_send_with_reply_and_block) X(dbus_connection_read_write) X(dbus_connection_pop_message) \
    X(dbus_connection_send) X(dbus_connection_flush) X(dbus_connection_close) X(dbus_connection_unref) \
    X(dbus_message_new_method_call) X(dbus_message_unref) X(dbus_message_get_args) X(dbus_message_is_signal) \
    X(dbus_message_get_path) X(dbus_message_iter_init_append) X(dbus_message_iter_init) \
    X(dbus_message_iter_open_container) X(dbus_message_iter_close_container) X(dbus_message_iter_append_basic) \
    X(dbus_message_iter_get_arg_type) X(dbus_message_iter_get_basic) X(dbus_message_iter_next) X(dbus_message_iter_recurse)
#define DECLARE(name) __typeof__(&name) name;
struct Dbus { void *library; DBUS_FUNCTIONS(DECLARE) };
#undef DECLARE
static int LoadDbus(struct Dbus *d)
{
    memset(d, 0, sizeof(*d)); d->library = SDL_LoadObject("libdbus-1.so.3"); if (!d->library) return -1;
#define LOAD(name) d->name = (__typeof__(d->name))SDL_LoadFunction(d->library, #name); if (!d->name) goto fail;
    DBUS_FUNCTIONS(LOAD)
#undef LOAD
    return 0;
fail:
    SDL_UnloadObject(d->library); memset(d, 0, sizeof(*d)); return -1;
}
static int HexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int UriPath(const char *uri, char out[RHH_FILE_DIALOG_PATH_SIZE])
{
    if (!uri || strncmp(uri, "file://", 7)) return -1;
    const char *p = uri + 7;
    if (*p != '/') { if (strncmp(p, "localhost/", 10)) return -1; p += 9; }
    size_t n = 0;
    while (*p)
    {
        unsigned char c = *p++;
        if (c == '?' || c == '#') return -1; /* these URI delimiters must be percent-encoded */
        if (c == '%')
        {
            if (!p[0] || !p[1]) return -1;
            int hi = HexValue(p[0]), lo = HexValue(p[1]); if (hi < 0 || lo < 0) return -1;
            c = (unsigned char)((hi << 4) | lo); p += 2;
        }
        if (!c || n + 1 >= RHH_FILE_DIALOG_PATH_SIZE) return -1;
        out[n++] = c;
    }
    if (!n || out[0] != '/') return -1;
    out[n] = 0; return 0;
}
static int AppendFilters(struct Dbus *d, DBusMessageIter *options, u32 kind)
{
    DBusMessageIter entry, variant, filters, filter, items, item;
    const char *key = "filters", *label = kind == RHH_FILE_DIALOG_MIDI ? "MIDI files" : "SoundFont files";
    const char *midiPatterns[] = {"*.mid", "*.midi", "*.MID", "*.MIDI"};
    const char *fontPatterns[] = {"*.sf2", "*.SF2"};
    dbus_uint32_t glob = 0;
    if (!d->dbus_message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, NULL, &entry)
        || !d->dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key)
        || !d->dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a(sa(us))", &variant)
        || !d->dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "(sa(us))", &filters)
        || !d->dbus_message_iter_open_container(&filters, DBUS_TYPE_STRUCT, NULL, &filter)
        || !d->dbus_message_iter_append_basic(&filter, DBUS_TYPE_STRING, &label)
        || !d->dbus_message_iter_open_container(&filter, DBUS_TYPE_ARRAY, "(us)", &items)) return -1;
    for (unsigned i = 0; i < (kind == RHH_FILE_DIALOG_MIDI ? 4 : 2); ++i)
    {
        const char *p = kind == RHH_FILE_DIALOG_MIDI ? midiPatterns[i] : fontPatterns[i];
        if (!d->dbus_message_iter_open_container(&items, DBUS_TYPE_STRUCT, NULL, &item)
            || !d->dbus_message_iter_append_basic(&item, DBUS_TYPE_UINT32, &glob)
            || !d->dbus_message_iter_append_basic(&item, DBUS_TYPE_STRING, &p)
            || !d->dbus_message_iter_close_container(&items, &item)) return -1;
    }
    return d->dbus_message_iter_close_container(&filter, &items)
        && d->dbus_message_iter_close_container(&filters, &filter)
        && d->dbus_message_iter_close_container(&variant, &filters)
        && d->dbus_message_iter_close_container(&entry, &variant)
        && d->dbus_message_iter_close_container(options, &entry) ? 0 : -1;
}
static int Response(struct Dbus *d, DBusMessage *message, char *path)
{
    DBusMessageIter it, dict;
    if (!d->dbus_message_iter_init(message, &it) || d->dbus_message_iter_get_arg_type(&it) != DBUS_TYPE_UINT32) return RHH_FILE_DIALOG_ERROR;
    dbus_uint32_t code = 2; d->dbus_message_iter_get_basic(&it, &code);
    if (code == 1) return RHH_FILE_DIALOG_CANCELLED;
    if (code || !d->dbus_message_iter_next(&it) || d->dbus_message_iter_get_arg_type(&it) != DBUS_TYPE_ARRAY) return RHH_FILE_DIALOG_ERROR;
    d->dbus_message_iter_recurse(&it, &dict);
    while (d->dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY)
    {
        DBusMessageIter pair, variant, values; const char *name = NULL, *uri = NULL;
        d->dbus_message_iter_recurse(&dict, &pair);
        if (d->dbus_message_iter_get_arg_type(&pair) != DBUS_TYPE_STRING) return RHH_FILE_DIALOG_ERROR;
        d->dbus_message_iter_get_basic(&pair, &name);
        if (!d->dbus_message_iter_next(&pair) || d->dbus_message_iter_get_arg_type(&pair) != DBUS_TYPE_VARIANT) return RHH_FILE_DIALOG_ERROR;
        if (!strcmp(name, "uris"))
        {
            d->dbus_message_iter_recurse(&pair, &variant);
            if (d->dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_ARRAY) return RHH_FILE_DIALOG_ERROR;
            d->dbus_message_iter_recurse(&variant, &values);
            if (d->dbus_message_iter_get_arg_type(&values) != DBUS_TYPE_STRING) return RHH_FILE_DIALOG_ERROR;
            d->dbus_message_iter_get_basic(&values, &uri);
            if (d->dbus_message_iter_next(&values) || UriPath(uri, path)) return RHH_FILE_DIALOG_ERROR;
            return RHH_FILE_DIALOG_SELECTED;
        }
        d->dbus_message_iter_next(&dict);
    }
    return RHH_FILE_DIALOG_ERROR;
}
static int RunDialog(u32 kind, char *path)
{
    if (!getenv("DBUS_SESSION_BUS_ADDRESS")) return RHH_FILE_DIALOG_ERROR;
    struct Dbus d; if (LoadDbus(&d)) return RHH_FILE_DIALOG_ERROR;
    DBusConnection *bus = d.dbus_bus_get_private(DBUS_BUS_SESSION, NULL);
    DBusMessage *call = NULL, *reply = NULL; int state = RHH_FILE_DIALOG_ERROR; char handle[512] = {0};
    if (!bus) goto cleanup;
    d.dbus_connection_set_exit_on_disconnect(bus, FALSE);
    d.dbus_bus_add_match(bus, "type='signal',sender='org.freedesktop.portal.Desktop',interface='org.freedesktop.portal.Request',member='Response'", NULL);
    d.dbus_connection_flush(bus);
    call = d.dbus_message_new_method_call("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                                         "org.freedesktop.portal.FileChooser", "OpenFile");
    if (!call) goto cleanup;
    DBusMessageIter root, options; const char *parent = sParent, *title = kind == RHH_FILE_DIALOG_MIDI ? "Choose a MIDI file" : "Choose a SoundFont file";
    d.dbus_message_iter_init_append(call, &root);
    if (!d.dbus_message_iter_append_basic(&root, DBUS_TYPE_STRING, &parent)
        || !d.dbus_message_iter_append_basic(&root, DBUS_TYPE_STRING, &title)
        || !d.dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY, "{sv}", &options)
        || AppendFilters(&d, &options, kind) || !d.dbus_message_iter_close_container(&root, &options)) goto cleanup;
    reply = d.dbus_connection_send_with_reply_and_block(bus, call, 3000, NULL);
    if (!reply) goto cleanup;
    const char *returned = NULL;
    if (!d.dbus_message_get_args(reply, NULL, DBUS_TYPE_OBJECT_PATH, &returned, DBUS_TYPE_INVALID)
        || !returned || strlen(returned) >= sizeof(handle)) goto cleanup;
    strcpy(handle, returned);
    while (!SDL_AtomicGet(&sDialog.cancel))
    {
        if (!d.dbus_connection_read_write(bus, 100)) goto cleanup;
        DBusMessage *message;
        while ((message = d.dbus_connection_pop_message(bus)))
        {
            const char *messagePath = d.dbus_message_get_path(message);
            int matched = messagePath && !strcmp(messagePath, handle)
                && d.dbus_message_is_signal(message, "org.freedesktop.portal.Request", "Response");
            if (matched) state = Response(&d, message, path);
            d.dbus_message_unref(message);
            if (matched) goto cleanup;
        }
    }
    state = RHH_FILE_DIALOG_CANCELLED;
    if (handle[0])
    {
        DBusMessage *close = d.dbus_message_new_method_call("org.freedesktop.portal.Desktop", handle,
                                                            "org.freedesktop.portal.Request", "Close");
        if (close) { d.dbus_connection_send(bus, close, NULL); d.dbus_connection_flush(bus); d.dbus_message_unref(close); }
    }
cleanup:
    if (reply) d.dbus_message_unref(reply);
    if (call) d.dbus_message_unref(call);
    if (bus) { d.dbus_connection_close(bus); d.dbus_connection_unref(bus); }
    SDL_UnloadObject(d.library);
    return state;
}
#elif defined(_WIN32)
static UINT_PTR CALLBACK DialogHook(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)wParam; (void)lParam;
    if (message == WM_INITDIALOG) SetTimer(window, 1, 100, NULL);
    else if (message == WM_TIMER && SDL_AtomicGet(&sDialog.cancel)) PostMessageW(GetParent(window), WM_COMMAND, IDCANCEL, 0);
    else if (message == WM_DESTROY) KillTimer(window, 1);
    return 0;
}
static int RunDialog(u32 kind, char *path)
{
    HMODULE library = LoadLibraryW(L"comdlg32.dll"); if (!library) return RHH_FILE_DIALOG_ERROR;
    BOOL (WINAPI *openFile)(LPOPENFILENAMEW) = (void *)GetProcAddress(library, "GetOpenFileNameW");
    DWORD (WINAPI *extendedError)(void) = (void *)GetProcAddress(library, "CommDlgExtendedError");
    if (!openFile || !extendedError) { FreeLibrary(library); return RHH_FILE_DIALOG_ERROR; }
    wchar_t *file = calloc(32768, sizeof(*file)); if (!file) { FreeLibrary(library); return RHH_FILE_DIALOG_ERROR; }
    OPENFILENAMEW dialog = {0}; dialog.lStructSize = sizeof(dialog); dialog.lpstrFile = file; dialog.nMaxFile = 32768;
    dialog.hwndOwner = sWindow;
    dialog.lpstrTitle = kind == RHH_FILE_DIALOG_MIDI ? L"Choose a MIDI file" : L"Choose a SoundFont file";
    dialog.lpstrFilter = kind == RHH_FILE_DIALOG_MIDI ? L"MIDI files\0*.mid;*.midi\0All files\0*.*\0\0"
        : L"SoundFont files\0*.sf2\0All files\0*.*\0\0";
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_ENABLEHOOK;
    dialog.lpfnHook = DialogHook;
    int result = RHH_FILE_DIALOG_CANCELLED;
    if (!SDL_AtomicGet(&sDialog.cancel))
    {
        if (openFile(&dialog))
            result = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, file, -1, path,
                                         RHH_FILE_DIALOG_PATH_SIZE, NULL, NULL) ? RHH_FILE_DIALOG_SELECTED : RHH_FILE_DIALOG_ERROR;
        else if (extendedError() && !SDL_AtomicGet(&sDialog.cancel)) result = RHH_FILE_DIALOG_ERROR;
    }
    free(file); FreeLibrary(library); return result;
}
#else
static int RunDialog(u32 kind, char *path) { (void)kind; (void)path; return RHH_FILE_DIALOG_ERROR; }
#endif
static int Worker(void *unused)
{
    (void)unused;
    u32 state = RunDialog(sDialog.kind, sDialog.path);
    if (SDL_AtomicGet(&sDialog.cancel)) state = RHH_FILE_DIALOG_CANCELLED;
    if (state != RHH_FILE_DIALOG_SELECTED) sDialog.path[0] = 0;
    sDialog.state = state; SDL_AtomicSet(&sDialog.done, 1); return 0;
}
static void Join(void)
{
    if (sDialog.thread && SDL_AtomicGet(&sDialog.done)) { SDL_WaitThread(sDialog.thread, NULL); sDialog.thread = NULL; }
}
static void Clear(void) { memset(&sDialog, 0, sizeof(sDialog)); }
void RhhFileDialogCancelOwner(u64 owner)
{
    if (owner && sDialog.owner == owner) { sDialog.discard = TRUE; SDL_AtomicSet(&sDialog.cancel, 1); }
}
void RhhFileDialogShutdown(void)
{
    if (sDialog.thread) { SDL_AtomicSet(&sDialog.cancel, 1); SDL_WaitThread(sDialog.thread, NULL); }
    Clear();
}
static int Begin(RhhContentKey owner, u32 kind)
{
    if (!owner || !RhhModsRegisteredKey(owner) || (kind != RHH_FILE_DIALOG_MIDI && kind != RHH_FILE_DIALOG_SOUNDFONT)) return RHH_FILE_DIALOG_INVALID;
    if (!RhhModsCommandsAllowed() || !RhhModsHasActiveRun() || !RhhUiPanelOwnedBy(owner)
        || gMain.inBattle || NativeSaveTransactionActive()) return RHH_FILE_DIALOG_UNAVAILABLE;
    Join(); if (sDialog.discard && !sDialog.thread) Clear();
    if (sDialog.owner) return RHH_FILE_DIALOG_BUSY;
    sDialog.owner = owner; sDialog.kind = kind; sDialog.state = RHH_FILE_DIALOG_PENDING;
    sDialog.thread = SDL_CreateThread(Worker, "rhh-file-dialog", NULL);
    if (!sDialog.thread) { Clear(); return RHH_FILE_DIALOG_UNAVAILABLE; }
    return RHH_FILE_DIALOG_OK;
}
static int Result(RhhContentKey owner, struct RhhFileDialogResult *out, int take)
{
    if (!out || out->struct_size < sizeof(*out) || !owner || owner != sDialog.owner || sDialog.discard) return RHH_FILE_DIALOG_INVALID;
    Join();
    if (!sDialog.thread && SDL_AtomicGet(&sDialog.cancel)) { sDialog.state = RHH_FILE_DIALOG_CANCELLED; sDialog.path[0] = 0; }
    struct RhhFileDialogResult result = {.struct_size = sizeof(result), .state = sDialog.thread ? RHH_FILE_DIALOG_PENDING : sDialog.state,
        .kind = sDialog.kind, .owner = owner};
    if (!sDialog.thread && sDialog.state == RHH_FILE_DIALOG_SELECTED) memcpy(result.path, sDialog.path, sizeof(result.path));
    *out = result; if (take && !sDialog.thread) Clear(); return RHH_FILE_DIALOG_OK;
}
static int Poll(RhhContentKey owner, struct RhhFileDialogResult *out) { return Result(owner, out, FALSE); }
static int Take(RhhContentKey owner, struct RhhFileDialogResult *out)
{ if (!RhhModsCommandsAllowed()) return RHH_FILE_DIALOG_UNAVAILABLE; return Result(owner, out, TRUE); }
static int Cancel(RhhContentKey owner)
{
    if (!RhhModsCommandsAllowed() || !owner || owner != sDialog.owner || sDialog.discard) return RHH_FILE_DIALOG_UNAVAILABLE;
    SDL_AtomicSet(&sDialog.cancel, 1); return RHH_FILE_DIALOG_OK;
}
static const struct RhhFileDialogService sService = {{sizeof(sService), RHH_SERVICE_VERSION(1, 0)}, Begin, Poll, Take, Cancel};
int RhhFileDialogInstallService(void) { return RhhModsAddService("rhh:service/file_dialog", &sService.header); }
