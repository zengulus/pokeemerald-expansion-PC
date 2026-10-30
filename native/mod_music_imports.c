/* Persistent, content-addressed player MIDI/SF2 library. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#undef TRANSPARENT
#include <io.h>
#else
#include <unistd.h>
#include <sys/file.h>
#endif
#include "global.h"
#include "main.h"
#include "mod_api.h"
#include "mod_host.h"
#include "mod_music.h"
#include "music_sha256.h"
#include "native.h"

enum { MIDI_LIMIT = 4 * 1024 * 1024, FONT_LIMIT = 128 * 1024 * 1024,
       LIBRARY_LIMIT = 256 * 1024 * 1024, RECORD = 144, HEADER = 16, TRAILER = 32 };
static const u8 sMagic[8] = {'R','H','H','M','U','S','0','1'};
static char *sRoot;
static struct RhhImportedMusic sTracks[RHH_MUSIC_IMPORT_LIMIT];
static u32 sCount;
static bool32 sReady, sBusy;
static int sLoadStatus = RHH_MUSIC_IMPORT_UNAVAILABLE;
static const struct RhhMusicImportResult *sContext;
static u32 Read32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static void Write32(u8 *p, u32 n) { for (u32 i = 0; i < 4; ++i) p[i] = n >> (8 * i); }
#ifdef _WIN32
static wchar_t *Wide(const char *path)
{
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (!n) return NULL;
    wchar_t *out = malloc((size_t)n * sizeof(*out));
    if (!out) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, out, n)) { free(out); return NULL; }
    return out;
}
#endif
static FILE *Open(const char *path, int write)
{
#ifdef _WIN32
    wchar_t *wide = Wide(path); if (!wide) return NULL;
    FILE *file = _wfopen(wide, write ? L"wb" : L"rb"); free(wide); return file;
#else
    return fopen(path, write ? "wb" : "rb");
#endif
}
static int Replace(const char *from, const char *to)
{
#ifdef _WIN32
    wchar_t *a = Wide(from), *b = Wide(to);
    int ok = a && b && MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    free(a); free(b); return ok ? 0 : -1;
#else
    return rename(from, to);
#endif
}
static void Remove(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = Wide(path); if (wide) { _wremove(wide); free(wide); }
#else
    remove(path);
#endif
}
static char *Path(const char *name)
{
    if (!sRoot) return NULL;
    size_t n = strlen(sRoot) + strlen(name) + 2; char *path = malloc(n);
    if (path) snprintf(path, n, "%s/%s", sRoot, name);
    return path;
}
static void Hex(const u8 *hash, u32 count, char *out)
{
    static const char digits[] = "0123456789abcdef";
    for (u32 i = 0; i < count; ++i) { out[i*2] = digits[hash[i] >> 4]; out[i*2+1] = digits[hash[i] & 15]; }
    out[count*2] = 0;
}
static RhhContentKey TrackKey(const u8 midi[32], const u8 font[32])
{
    u8 pair[64], hash[32]; char id[52] = "user:music/";
    memcpy(pair, midi, 32); memcpy(pair + 32, font, 32); NativeMusicSha256(pair, sizeof(pair), hash);
    Hex(hash, 20, id + 11); return RhhContentKeyOf(id);
}
static char *BlobPath(const u8 hash[32], bool32 font)
{
    char name[96] = "rhh-music-"; Hex(hash, 32, name + 10);
    strcat(name, font ? ".sf2" : ".mid"); return Path(name);
}
static bool32 ValidLabel(const char *label)
{
    if (!label || !*label) return FALSE;
    u32 i = 0; while (label[i]) { if ((u8)label[i] < 32 || (u8)label[i] > 126 || ++i >= RHH_MUSIC_IMPORT_LABEL_SIZE) return FALSE; }
    return TRUE;
}
static int ReadAssetFile(const char *path, u32 limit, unsigned char **out, size_t *size)
{
    if (!path || !*path || strlen(path) > 4096) return RHH_MUSIC_IMPORT_INVALID;
    FILE *f = Open(path, 0); if (!f) return errno == ENOENT ? RHH_MUSIC_IMPORT_MISSING : RHH_MUSIC_IMPORT_IO;
    if (fseek(f, 0, SEEK_END)) { fclose(f); return RHH_MUSIC_IMPORT_IO; }
    long n = ftell(f);
    if (n <= 0 || (unsigned long)n > limit || fseek(f, 0, SEEK_SET)) { fclose(f); return RHH_MUSIC_IMPORT_INVALID; }
    u8 *data = malloc((size_t)n); if (!data) { fclose(f); return RHH_MUSIC_IMPORT_IO; }
    int ok = fread(data, 1, (size_t)n, f) == (size_t)n && !ferror(f);
    if (fclose(f)) ok = 0;
    if (!ok) { free(data); return RHH_MUSIC_IMPORT_IO; }
    *out = data; *size = (size_t)n; return RHH_MUSIC_IMPORT_OK;
}
static int AtomicWrite(const char *path, const u8 *data, size_t size)
{
    size_t n = strlen(path) + 40; char *tmp = malloc(n); if (!tmp) return -1;
#ifdef _WIN32
    snprintf(tmp, n, "%s.tmp.%lu", path, (unsigned long)GetCurrentProcessId());
#else
    snprintf(tmp, n, "%s.tmp.%lu", path, (unsigned long)getpid());
#endif
    FILE *f = Open(tmp, 1); if (!f) { free(tmp); return -1; }
    int ok = fwrite(data, 1, size, f) == size && !fflush(f);
#ifdef _WIN32
    if (ok && _commit(_fileno(f))) ok = 0;
#else
    if (ok && fsync(fileno(f))) ok = 0;
#endif
    if (fclose(f)) ok = 0;
    if (ok) ok = !Replace(tmp, path);
    if (!ok) Remove(tmp);
    free(tmp); return ok ? 0 : -1;
}
static bool32 TrackConflicts(RhhContentKey key)
{
    u32 ignored;
    return !key || RhhModsFindMusic(key) || !RhhContentResolve(RHH_MIDI, key, &ignored);
}
static bool32 ValidTrack(const struct RhhImportedMusic *t)
{
    return t->midi_size && t->midi_size <= MIDI_LIMIT && t->soundfont_size && t->soundfont_size <= FONT_LIMIT
        && ValidLabel(t->label) && ((!t->has_loop && !t->loop_start_ms && !t->loop_end_ms)
            || (t->has_loop == 1 && t->loop_start_ms < t->loop_end_ms))
        && t->key == TrackKey(t->midi_sha256, t->soundfont_sha256) && t->key;
}
static u64 LibraryBytes(const struct RhhImportedMusic *tracks, u32 count)
{
    u64 total = 0;
    for (u32 i = 0; i < count; ++i)
    {
        bool32 midiSeen = FALSE, fontSeen = FALSE;
        for (u32 j = 0; j < i; ++j)
        {
            if (!memcmp(tracks[i].midi_sha256, tracks[j].midi_sha256, 32)) midiSeen = TRUE;
            if (!memcmp(tracks[i].soundfont_sha256, tracks[j].soundfont_sha256, 32)) fontSeen = TRUE;
        }
        if (!midiSeen) total += tracks[i].midi_size;
        if (!fontSeen) total += tracks[i].soundfont_size;
    }
    return total;
}
static int WriteManifest(const struct RhhImportedMusic *tracks, u32 count)
{
    u8 data[HEADER + RECORD * RHH_MUSIC_IMPORT_LIMIT + TRAILER] = {0};
    memcpy(data, sMagic, 8); Write32(data + 8, count);
    for (u32 i = 0; i < count; ++i)
    {
        u8 *p = data + HEADER + i * RECORD;
        memcpy(p, tracks[i].label, strlen(tracks[i].label)); memcpy(p + 64, tracks[i].midi_sha256, 32);
        memcpy(p + 96, tracks[i].soundfont_sha256, 32); Write32(p + 128, tracks[i].midi_size);
        Write32(p + 132, tracks[i].soundfont_size); Write32(p + 136, tracks[i].loop_start_ms); Write32(p + 140, tracks[i].loop_end_ms);
    }
    size_t end = HEADER + count * RECORD; NativeMusicSha256(data, end, data + end);
    char *path = Path("rhh-music-library.v1"); if (!path) return -1;
    int status = AtomicWrite(path, data, end + TRAILER); free(path); return status;
}
static int LoadManifest(void)
{
    char *path = Path("rhh-music-library.v1"); if (!path) return -1;
    u8 *data = NULL; size_t size = 0; int status = ReadAssetFile(path, HEADER + RECORD * RHH_MUSIC_IMPORT_LIMIT + TRAILER, &data, &size);
    free(path); if (status == RHH_MUSIC_IMPORT_MISSING) return 0;
    if (status || size < HEADER + TRAILER || memcmp(data, sMagic, 8) || Read32(data + 12)) { free(data); return -1; }
    u32 count = Read32(data + 8); size_t end = HEADER + (size_t)count * RECORD;
    u8 digest[32];
    if (count > RHH_MUSIC_IMPORT_LIMIT || size != end + TRAILER) { free(data); return -1; }
    NativeMusicSha256(data, end, digest);
    if (memcmp(digest, data + end, 32)) { free(data); return -1; }
    struct RhhImportedMusic loaded[RHH_MUSIC_IMPORT_LIMIT] = {{0}};
    for (u32 i = 0; i < count; ++i)
    {
        const u8 *p = data + HEADER + i * RECORD; struct RhhImportedMusic *t = &loaded[i];
        t->struct_size = sizeof(*t); memcpy(t->label, p, 64); memcpy(t->midi_sha256, p + 64, 32);
        memcpy(t->soundfont_sha256, p + 96, 32); t->midi_size = Read32(p + 128); t->soundfont_size = Read32(p + 132);
        t->loop_start_ms = Read32(p + 136); t->loop_end_ms = Read32(p + 140); t->has_loop = !!t->loop_end_ms;
        t->key = TrackKey(t->midi_sha256, t->soundfont_sha256);
        if (!memchr(t->label, 0, sizeof(t->label)) || !ValidTrack(t) || TrackConflicts(t->key)) { free(data); return -1; }
        for (u32 j = 0; j < i; ++j)
            if (loaded[j].key == t->key
                || (!memcmp(loaded[j].midi_sha256, t->midi_sha256, 32) && loaded[j].midi_size != t->midi_size)
                || (!memcmp(loaded[j].soundfont_sha256, t->soundfont_sha256, 32) && loaded[j].soundfont_size != t->soundfont_size))
            { free(data); return -1; }
    }
    free(data);
    if (LibraryBytes(loaded, count) > LIBRARY_LIMIT) return -1;
    memcpy(sTracks, loaded, count * sizeof(*loaded)); sCount = count; return 0;
}
static FILE *LockLibrary(void)
{
    char *path = Path("rhh-music-library.lock"); if (!path) return NULL;
    FILE *f = Open(path, 1); free(path); if (!f) return NULL;
#ifdef _WIN32
    OVERLAPPED overlapped = {0};
    if (!LockFileEx((HANDLE)_get_osfhandle(_fileno(f)), LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                    0, 1, 0, &overlapped)) { fclose(f); return NULL; }
#else
    if (flock(fileno(f), LOCK_EX | LOCK_NB)) { fclose(f); return NULL; }
#endif
    return f;
}
static void UnlockLibrary(FILE *f)
{
#ifdef _WIN32
    OVERLAPPED overlapped = {0}; UnlockFileEx((HANDLE)_get_osfhandle(_fileno(f)), 0, 1, 0, &overlapped);
#else
    flock(fileno(f), LOCK_UN);
#endif
    fclose(f);
}
static bool32 RefreshManifest(void)
{
    sCount = 0; memset(sTracks, 0, sizeof(sTracks));
    sReady = !LoadManifest(); sLoadStatus = sReady ? RHH_MUSIC_IMPORT_OK : RHH_MUSIC_IMPORT_IO;
    return sReady;
}
void NativeMusicSetImportRoot(const char *path)
{
    free(sRoot); sRoot = NULL; sCount = 0; sReady = FALSE; sLoadStatus = RHH_MUSIC_IMPORT_UNAVAILABLE; memset(sTracks, 0, sizeof(sTracks));
    if (!path || !*path) return;
    sRoot = malloc(strlen(path) + 1); if (!sRoot) { sLoadStatus = RHH_MUSIC_IMPORT_IO; return; }
    strcpy(sRoot, path); RefreshManifest();
}
static int FindIndex(RhhContentKey key)
{ for (u32 i = 0; i < sCount; ++i) if (sTracks[i].key == key) return i; return -1; }
int NativeMusicImportedKnown(uint64_t key) { return sReady && FindIndex(key) >= 0; }
static int ReadBlob(const u8 hash[32], bool32 font, u32 size, u8 **out, size_t *length)
{
    char *path = BlobPath(hash, font); if (!path) return RHH_MUSIC_IMPORT_IO;
    int status = ReadAssetFile(path, font ? FONT_LIMIT : MIDI_LIMIT, out, length); free(path);
    if (status) return status;
    u8 actual[32]; NativeMusicSha256(*out, *length, actual);
    if (*length != size || memcmp(actual, hash, 32)) { free(*out); *out = NULL; return RHH_MUSIC_IMPORT_INVALID; }
    return 0;
}
int NativeMusicImportedRead(uint64_t key, unsigned char **midi, size_t *midiSize, unsigned char **font, size_t *fontSize)
{
    int index = sReady ? FindIndex(key) : -1; if (index < 0) return RHH_MUSIC_MISSING;
    const struct RhhImportedMusic *t = &sTracks[index];
    int status = ReadBlob(t->midi_sha256, FALSE, t->midi_size, midi, midiSize);
    if (!status) status = ReadBlob(t->soundfont_sha256, TRUE, t->soundfont_size, font, fontSize);
    if (status) { free(*midi); *midi = NULL; free(*font); *font = NULL;
        return status == RHH_MUSIC_IMPORT_MISSING ? RHH_MUSIC_MISSING : RHH_MUSIC_INVALID; }
    return RHH_MUSIC_READY;
}
static int EnsureBlob(const u8 hash[32], bool32 font, const u8 *data, u32 size, bool32 *created)
{
    char *path = BlobPath(hash, font); if (!path) return -1;
    u8 *existing = NULL; size_t length = 0; int status = ReadAssetFile(path, font ? FONT_LIMIT : MIDI_LIMIT, &existing, &length);
    *created = status == RHH_MUSIC_IMPORT_MISSING;
    if (!status)
    {
        u8 digest[32]; NativeMusicSha256(existing, length, digest); free(existing);
        if (length == size && !memcmp(digest, hash, 32)) { free(path); return 0; }
    }
    else if (!*created && status != RHH_MUSIC_IMPORT_INVALID) { free(path); return -1; }
    status = AtomicWrite(path, data, size); free(path); return status;
}
static bool32 Referenced(const struct RhhImportedMusic *tracks, u32 count, const u8 hash[32], bool32 font)
{
    for (u32 i = 0; i < count; ++i)
        if (!memcmp(font ? tracks[i].soundfont_sha256 : tracks[i].midi_sha256, hash, 32)) return TRUE;
    return FALSE;
}
static void RemoveBlob(const u8 hash[32], bool32 font)
{ char *path = BlobPath(hash, font); if (path) { Remove(path); free(path); } }
static uint32_t Count(void) { return sReady ? sCount : 0; }
static int Status(void) { return sLoadStatus; }
static int At(u32 index, struct RhhImportedMusic *out)
{ if (!sReady || index >= sCount || !out || out->struct_size < sizeof(*out)) return -1; *out = sTracks[index]; return 0; }
static int Find(RhhContentKey key, struct RhhImportedMusic *out)
{ int index = sReady ? FindIndex(key) : -1; return index < 0 ? -1 : At(index, out); }
static int Context(struct RhhMusicImportResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sContext
        || (event != RHH_MUSIC_IMPORTED && event != RHH_MUSIC_IMPORT_REMOVED)) return -1;
    *out = *sContext; return 0;
}
static bool32 Ready(RhhContentKey requester)
{ return sReady && !sBusy && requester && RhhModsRegisteredKey(requester)
    && RhhModsCommandsAllowed() && !gMain.inBattle && !NativeSaveTransactionActive(); }
static void Notify(u32 kind, const struct RhhMusicImportResult *result)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = result->track.key, .subject = result->requester};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sContext = result; RhhModsDispatch(&event, &decision); sContext = NULL;
}
static bool32 SameTrack(const struct RhhImportedMusic *a, const struct RhhImportedMusic *b)
{
    return a->key == b->key && a->midi_size == b->midi_size && a->soundfont_size == b->soundfont_size
        && a->has_loop == b->has_loop && a->loop_start_ms == b->loop_start_ms && a->loop_end_ms == b->loop_end_ms
        && !memcmp(a->midi_sha256, b->midi_sha256, 32) && !memcmp(a->soundfont_sha256, b->soundfont_sha256, 32)
        && !strcmp(a->label, b->label);
}
static int ImportData(RhhContentKey requester, const char *label, const u8 *midi, size_t midiSize,
                      const u8 *font, size_t fontSize, const struct RhhImportedMusic *expected,
                      struct RhhMusicImportResult *result)
{
    bool32 newMidi = FALSE, newFont = FALSE;
    FILE *lock = NULL;
    int status = RHH_MUSIC_IMPORT_INVALID;
    struct RhhImportedMusic *t = &result->track;
    if (NativeMusicInspectImported(midi, midiSize, font, fontSize, &t->loop_start_ms, &t->loop_end_ms) != RHH_MUSIC_READY)
    { status = RHH_MUSIC_IMPORT_INVALID; goto finish; }
    t->has_loop = !!t->loop_end_ms; t->midi_size = midiSize; t->soundfont_size = fontSize;
    NativeMusicSha256(midi, midiSize, t->midi_sha256); NativeMusicSha256(font, fontSize, t->soundfont_sha256);
    t->key = TrackKey(t->midi_sha256, t->soundfont_sha256); strcpy(t->label, label);
    if (expected && !SameTrack(t, expected)) { status = RHH_MUSIC_IMPORT_INVALID; goto finish; }
    lock = LockLibrary();
    if (!lock) { status = RHH_MUSIC_IMPORT_UNAVAILABLE; goto finish; }
    if (!RefreshManifest()) { status = RHH_MUSIC_IMPORT_IO; goto finish; }
    int index = FindIndex(t->key);
    if (index >= 0)
    {
        if (memcmp(t->midi_sha256, sTracks[index].midi_sha256, 32) || memcmp(t->soundfont_sha256, sTracks[index].soundfont_sha256, 32))
        { status = RHH_MUSIC_IMPORT_CONFLICT; goto finish; }
        /* Re-importing identical bytes also repairs missing or damaged blobs. */
        if (EnsureBlob(t->midi_sha256, FALSE, midi, midiSize, &newMidi)
            || EnsureBlob(t->soundfont_sha256, TRUE, font, fontSize, &newFont))
        { status = RHH_MUSIC_IMPORT_IO; goto finish; }
        result->track = sTracks[index]; status = RHH_MUSIC_IMPORT_OK; goto finish;
    }
    if (TrackConflicts(t->key)) { status = RHH_MUSIC_IMPORT_CONFLICT; goto finish; }
    if (sCount == RHH_MUSIC_IMPORT_LIMIT) { status = RHH_MUSIC_IMPORT_FULL; goto finish; }
    struct RhhImportedMusic staged[RHH_MUSIC_IMPORT_LIMIT]; memcpy(staged, sTracks, sizeof(staged)); staged[sCount] = *t;
    if (LibraryBytes(staged, sCount + 1) > LIBRARY_LIMIT) { status = RHH_MUSIC_IMPORT_FULL; goto finish; }
    if (EnsureBlob(t->midi_sha256, FALSE, midi, midiSize, &newMidi)
        || EnsureBlob(t->soundfont_sha256, TRUE, font, fontSize, &newFont) || WriteManifest(staged, sCount + 1))
    {
        if (newMidi && !Referenced(sTracks, sCount, t->midi_sha256, FALSE)) RemoveBlob(t->midi_sha256, FALSE);
        if (newFont && !Referenced(sTracks, sCount, t->soundfont_sha256, TRUE)) RemoveBlob(t->soundfont_sha256, TRUE);
        status = RHH_MUSIC_IMPORT_IO; goto finish;
    }
    sTracks[sCount++] = *t; status = result->status = RHH_MUSIC_IMPORT_OK; result->changed = 1;
    result->requester = requester; Notify(RHH_MUSIC_IMPORTED, result);
finish:
    if (lock) UnlockLibrary(lock);
    return status;
}
static int Import(const struct RhhMusicImportRequest *request, struct RhhMusicImportResult *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_MUSIC_IMPORT_INVALID;
    struct RhhMusicImportResult result = {.struct_size = sizeof(result), .track.struct_size = sizeof(result.track)};
    u8 *midi = NULL, *font = NULL; size_t midiSize = 0, fontSize = 0;
    int status = RHH_MUSIC_IMPORT_INVALID;
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->requester
        || !ValidLabel(request->label) || !request->midi_path || !request->soundfont_path) goto done;
    result.requester = request->requester;
    if (!Ready(request->requester)) { status = RHH_MUSIC_IMPORT_UNAVAILABLE; goto done; }
    sBusy = TRUE;
    status = ReadAssetFile(request->midi_path, MIDI_LIMIT, &midi, &midiSize);
    if (!status) status = ReadAssetFile(request->soundfont_path, FONT_LIMIT, &font, &fontSize);
    if (!status) status = ImportData(request->requester, request->label, midi, midiSize, font, fontSize, NULL, &result);
    sBusy = FALSE;
done:
    free(midi); free(font); result.status = status; *out = result; return status;
}
static bool32 MemoryOverlaps(const void *a, size_t aSize, const void *b, size_t bSize)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return x <= y ? y - x < aSize : x - y < bSize;
}
static int CopyPair(const struct RhhImportedMusic *expected, void *midi, u32 midiCapacity, void *font, u32 fontCapacity)
{
    if (!expected || expected->struct_size < sizeof(*expected) || !ValidTrack(expected) || !midi || !font
        || midiCapacity < expected->midi_size || fontCapacity < expected->soundfont_size
        || MemoryOverlaps(midi, expected->midi_size, font, expected->soundfont_size)) return RHH_MUSIC_IMPORT_INVALID;
    struct RhhImportedMusic copy = *expected;
    if (!sReady || sBusy) return RHH_MUSIC_IMPORT_UNAVAILABLE;
    int index = FindIndex(copy.key);
    if (index < 0) return RHH_MUSIC_IMPORT_MISSING;
    if (!SameTrack(&copy, &sTracks[index])) return RHH_MUSIC_IMPORT_CONFLICT;
    u8 *midiData = NULL, *fontData = NULL; size_t midiSize = 0, fontSize = 0;
    int status = NativeMusicImportedRead(copy.key, &midiData, &midiSize, &fontData, &fontSize);
    if (status == RHH_MUSIC_READY) {
        memcpy(midi, midiData, midiSize); memcpy(font, fontData, fontSize); status = RHH_MUSIC_IMPORT_OK;
    } else status = status == RHH_MUSIC_MISSING ? RHH_MUSIC_IMPORT_MISSING : RHH_MUSIC_IMPORT_INVALID;
    free(midiData); free(fontData); return status;
}
static int ImportMemory(const struct RhhMusicMemoryImportRequest *request, struct RhhMusicImportResult *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_MUSIC_IMPORT_INVALID;
    struct RhhMusicImportResult result = {.struct_size = sizeof(result), .track.struct_size = sizeof(result.track)};
    int status = RHH_MUSIC_IMPORT_INVALID; u8 *midi = NULL, *font = NULL;
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->requester
        || !request->expected || request->expected->struct_size < sizeof(*request->expected) || !ValidTrack(request->expected)
        || !request->midi || !request->soundfont || request->midi_size != request->expected->midi_size
        || request->soundfont_size != request->expected->soundfont_size) goto done;
    struct RhhImportedMusic expected = *request->expected;
    result.requester = request->requester;
    if (!Ready(request->requester)) { status = RHH_MUSIC_IMPORT_UNAVAILABLE; goto done; }
    sBusy = TRUE;
    midi = malloc(request->midi_size); font = malloc(request->soundfont_size);
    if (!midi || !font) { status = RHH_MUSIC_IMPORT_IO; goto finish; }
    memcpy(midi, request->midi, request->midi_size); memcpy(font, request->soundfont, request->soundfont_size);
    status = ImportData(request->requester, expected.label, midi, request->midi_size, font,
                        request->soundfont_size, &expected, &result);
finish:
    sBusy = FALSE;
done:
    free(midi); free(font); result.status = status; *out = result; return status;
}
static int Delete(RhhContentKey requester, RhhContentKey key, struct RhhMusicImportResult *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_MUSIC_IMPORT_INVALID;
    struct RhhMusicImportResult result = {.struct_size = sizeof(result), .requester = requester,
        .track.struct_size = sizeof(result.track)};
    int status = RHH_MUSIC_IMPORT_UNAVAILABLE;
    if (!Ready(requester)) goto done;
    FILE *lock = LockLibrary(); if (!lock) goto done;
    if (!RefreshManifest()) { status = RHH_MUSIC_IMPORT_IO; UnlockLibrary(lock); goto done; }
    int index = FindIndex(key);
    if (index < 0) { status = RHH_MUSIC_IMPORT_MISSING; UnlockLibrary(lock); goto done; }
    sBusy = TRUE; result.track = sTracks[index];
    struct RhhImportedMusic staged[RHH_MUSIC_IMPORT_LIMIT]; memcpy(staged, sTracks, sizeof(staged));
    memmove(&staged[index], &staged[index + 1], (sCount - index - 1) * sizeof(*staged));
    if (WriteManifest(staged, sCount - 1)) { status = RHH_MUSIC_IMPORT_IO; sBusy = FALSE; UnlockLibrary(lock); goto done; }
    --sCount; memcpy(sTracks, staged, sCount * sizeof(*staged)); memset(&sTracks[sCount], 0, sizeof(*sTracks));
    if (!Referenced(sTracks, sCount, result.track.midi_sha256, FALSE)) RemoveBlob(result.track.midi_sha256, FALSE);
    if (!Referenced(sTracks, sCount, result.track.soundfont_sha256, TRUE)) RemoveBlob(result.track.soundfont_sha256, TRUE);
    status = result.status = RHH_MUSIC_IMPORT_OK; result.changed = 1; Notify(RHH_MUSIC_IMPORT_REMOVED, &result);
    sBusy = FALSE; UnlockLibrary(lock);
done:
    result.status = status; *out = result; return status;
}
static const struct RhhMusicImportService sService = {{sizeof(sService), RHH_SERVICE_VERSION(1, 1)},
    Status, Count, At, Find, Import, Delete, Context, CopyPair, ImportMemory};
int NativeMusicInstallImports(void)
{
    sBusy = FALSE; sContext = NULL;
    return RhhModsAddService("rhh:service/music_imports", &sService.header);
}
