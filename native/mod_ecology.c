/* Shared, save-backed location ecology values. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mod_host.h"
#include "native.h"

enum { ECO_HEADER = 24, ECO_RECORD = 32 };
struct Entry { uint64_t map, key; int64_t value; uint32_t kind; };
static struct Entry *sEntries;
static size_t sCount, sCapacity;
static uint64_t sRun;

static uint32_t Read32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t Read64(const uint8_t *p) { return Read32(p) | (uint64_t)Read32(p + 4) << 32; }
static int64_t ReadSigned(const uint8_t *p)
{
    uint64_t value = Read64(p);
    return value <= INT64_MAX ? (int64_t)value : -1 - (int64_t)(UINT64_MAX - value);
}
static void Write32(uint8_t *p, uint32_t v)
{ for (unsigned i = 0; i < 4; ++i) p[i] = v >> (8 * i); }
static void Write64(uint8_t *p, uint64_t v) { Write32(p, v); Write32(p + 4, v >> 32); }
static int Compare(uint64_t map, uint32_t kind, uint64_t key, const struct Entry *e)
{
    if (map != e->map) return map < e->map ? -1 : 1;
    if (kind != e->kind) return kind < e->kind ? -1 : 1;
    return key == e->key ? 0 : key < e->key ? -1 : 1;
}
static size_t Find(uint64_t map, uint32_t kind, uint64_t key)
{
    size_t lo = 0, hi = sCount;
    while (lo < hi) { size_t mid = lo + (hi - lo) / 2;
        if (Compare(map, kind, key, &sEntries[mid]) > 0) lo = mid + 1; else hi = mid; }
    return lo;
}
static void Reset(void)
{ free(sEntries); sEntries = NULL; sCount = sCapacity = 0; sRun = 0; }
static int Validate(const uint8_t *data, uint32_t size)
{
    if (!size) return data ? -1 : 0;
    if (!data || size < ECO_HEADER || memcmp(data, "RHHECO01", 8)
        || Read32(data + 8) != size || !Read64(data + 16)) return -1;
    uint32_t count = Read32(data + 12);
    if (!count || count > (size - ECO_HEADER) / ECO_RECORD || size != ECO_HEADER + count * ECO_RECORD) return -1;
    const uint8_t *run = RhhModsState("rhh:host_run");
    if (!run || Read64(run + 8) != Read64(data + 16)) return -1;
    struct Entry previous = {0};
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint8_t *p = data + ECO_HEADER + i * ECO_RECORD;
        struct Entry e = {Read64(p), Read64(p + 8), ReadSigned(p + 16), Read32(p + 24)};
        if (!e.map || !e.key || (e.kind != RHH_ECOLOGY_CONDITION && e.kind != RHH_ECOLOGY_POPULATION
                                && e.kind != RHH_ECOLOGY_OBSERVATIONS)
            || (e.kind != RHH_ECOLOGY_CONDITION && e.value < 0) || Read32(p + 28)
            || (i && Compare(e.map, e.kind, e.key, &previous) <= 0)) return -1;
        previous = e;
    }
    return 0;
}
static int Export(uint8_t **out, uint32_t *size)
{
    if (!out || !size) return -1;
    *out = NULL; *size = 0;
    if (!sCount) return 0;
    if (sCount > (UINT32_MAX - ECO_HEADER) / ECO_RECORD) return -1;
    uint32_t length = ECO_HEADER + sCount * ECO_RECORD;
    uint8_t *data = calloc(length, 1); if (!data) return -1;
    memcpy(data, "RHHECO01", 8); Write32(data + 8, length); Write32(data + 12, sCount); Write64(data + 16, sRun);
    for (size_t i = 0; i < sCount; ++i)
    {
        uint8_t *p = data + ECO_HEADER + i * ECO_RECORD;
        Write64(p, sEntries[i].map); Write64(p + 8, sEntries[i].key);
        Write64(p + 16, (uint64_t)sEntries[i].value); Write32(p + 24, sEntries[i].kind);
    }
    if (Validate(data, length)) { free(data); return -1; }
    *out = data; *size = length; return 0;
}
static int Import(const uint8_t *data, uint32_t size)
{
    if (Validate(data, size)) return -1;
    if (!size) { Reset(); return 0; }
    uint32_t count = Read32(data + 12);
    struct Entry *entries = calloc(count, sizeof(*entries)); if (!entries) return -1;
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint8_t *p = data + ECO_HEADER + i * ECO_RECORD;
        entries[i] = (struct Entry){Read64(p), Read64(p + 8), ReadSigned(p + 16), Read32(p + 24)};
    }
    Reset(); sEntries = entries; sCount = sCapacity = count; sRun = Read64(data + 16); return 0;
}
static const struct NativeSaveArchiveCodec sCodec = {Export, Validate, Import, Reset, "rhh:ecology", NULL};
static int Ready(void) { return RhhModsHasActiveRun() && (!sRun || sRun == RhhModsRunId()); }
static int Canonical(uint64_t *map, uint32_t kind, uint64_t *key)
{
    uint32_t id;
    if (!map || !key || !*map || !*key || (kind != RHH_ECOLOGY_CONDITION && kind != RHH_ECOLOGY_POPULATION
                                           && kind != RHH_ECOLOGY_OBSERVATIONS)
        || RhhContentResolve(RHH_MAP, *map, &id) || !(*map = RhhContentKeyForId(RHH_MAP, id))) return -1;
    if (kind != RHH_ECOLOGY_CONDITION
        && (RhhContentResolve(RHH_SPECIES, *key, &id) || !(*key = RhhContentKeyForId(RHH_SPECIES, id)))) return -1;
    return 0;
}
static int Get(uint64_t map, uint32_t kind, uint64_t key, int64_t *out)
{
    if (!out || !Ready() || Canonical(&map, kind, &key)) return -1;
    size_t i = Find(map, kind, key);
    if (i == sCount || Compare(map, kind, key, &sEntries[i])) return 1;
    *out = sEntries[i].value; return 0;
}
static void PressureSettleMap(uint64_t map, uint64_t now);
static uint32_t PressureSetting(uint64_t map, const char *name, uint32_t fallback)
{
    uint64_t key = RhhContentKeyOf(name);
    size_t i = Find(map, RHH_ECOLOGY_CONDITION, key);
    if (i == sCount || Compare(map, RHH_ECOLOGY_CONDITION, key, &sEntries[i])
        || sEntries[i].value < 0 || sEntries[i].value > 10000) return fallback;
    return (uint32_t)sEntries[i].value;
}
static int IsRecoverySetting(uint32_t kind, uint64_t key)
{ return kind == RHH_ECOLOGY_CONDITION && key == RhhContentKeyOf("rhh:ecology/recovery_per_hour"); }
static int Writable(void)
{
    return Ready() && !NativeSaveTransactionActive()
        && (RhhModsCurrentEvent() ? RhhModsArchiveObserverAllowed() : RhhModsCommandsAllowed());
}
static int Set(uint64_t map, uint32_t kind, uint64_t key, int64_t value)
{
    if (!Writable() || Canonical(&map, kind, &key) || (kind != RHH_ECOLOGY_CONDITION && value < 0)) return -1;
    size_t i = Find(map, kind, key);
    if (i < sCount && !Compare(map, kind, key, &sEntries[i])) {
        if (IsRecoverySetting(kind, key) && sEntries[i].value != value) PressureSettleMap(map, RhhEcologySystemTime());
        sEntries[i].value = value; return 0;
    }
    if (sCount >= (UINT32_MAX - ECO_HEADER) / ECO_RECORD) return -1;
    if (sCount == sCapacity)
    {
        size_t capacity = sCapacity ? sCapacity * 2 : 16;
        if (capacity > SIZE_MAX / sizeof(*sEntries)) return -1;
        void *p = realloc(sEntries, capacity * sizeof(*sEntries)); if (!p) return -1;
        sEntries = p; sCapacity = capacity;
    }
    if (IsRecoverySetting(kind, key)) PressureSettleMap(map, RhhEcologySystemTime());
    memmove(sEntries + i + 1, sEntries + i, (sCount - i) * sizeof(*sEntries));
    sEntries[i] = (struct Entry){map, key, value, kind}; ++sCount; sRun = RhhModsRunId(); return 0;
}
static int Remove(uint64_t map, uint32_t kind, uint64_t key)
{
    if (!Writable() || Canonical(&map, kind, &key)) return -1;
    size_t i = Find(map, kind, key);
    if (i == sCount || Compare(map, kind, key, &sEntries[i])) return 1;
    if (IsRecoverySetting(kind, key)) PressureSettleMap(map, RhhEcologySystemTime());
    memmove(sEntries + i, sEntries + i + 1, (sCount - i - 1) * sizeof(*sEntries));
    if (!--sCount) sRun = 0;
    return 0;
}
static int At(uint64_t map, uint32_t index, struct RhhEcologyValue *out)
{
    uint32_t id;
    if (!out || out->struct_size < sizeof(*out) || !Ready() || !map
        || RhhContentResolve(RHH_MAP, map, &id) || !(map = RhhContentKeyForId(RHH_MAP, id))) return -1;
    size_t i = Find(map, 0, 0);
    if (index >= sCount - i || sEntries[i + index].map != map) return 1;
    const struct Entry *e = &sEntries[i + index];
    *out = (struct RhhEcologyValue){sizeof(*out), e->kind, e->map, e->key, e->value}; return 0;
}
static const struct RhhEcologyService sService = {
    {sizeof(sService), RHH_SERVICE_VERSION(1, 0)}, Get, Set, Remove, At
};
static const struct NativeSaveArchiveCodec sPressureCodec;
int RhhModsInstallEcologyService(void)
{ return NativeSaveRegisterArchive(&sCodec) || NativeSaveRegisterArchive(&sPressureCodec)
    || RhhModsAddService("rhh:service/ecology", &sService.header) ? -1 : 0; }

/* Encounter pressure is separate from the public population multiplier: mods
 * may keep using that service without changing their saved values. Deficit is
 * stored in basis-point seconds so fractional recovery survives every save. */
enum { PRESSURE_RECORD = 32, PRESSURE_BASE = 10000, PRESSURE_DROP = 1000,
       PRESSURE_RECOVERY_PER_HOUR = 1000 };
struct Pressure { uint64_t map, species, deficit, updated; };
static struct Pressure *sPressure;
static size_t sPressureCount, sPressureCapacity;
static uint64_t sPressureRun, sLastClockSample;
static int sClockUnavailable;

static void PressureReset(void)
{ free(sPressure); sPressure = NULL; sPressureCount = sPressureCapacity = 0; sPressureRun = sLastClockSample = 0; sClockUnavailable = 0; }
static size_t PressureFind(uint64_t map, uint64_t species)
{
    size_t lo = 0, hi = sPressureCount;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (sPressure[mid].map < map || (sPressure[mid].map == map && sPressure[mid].species < species)) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}
static int PressureValidate(const uint8_t *data, uint32_t size)
{
    if (!size) return data ? -1 : 0;
    if (!data || size < ECO_HEADER || memcmp(data, "RHHECP01", 8) || Read32(data + 8) != size
        || !Read32(data + 12) || !Read64(data + 16)
        || Read32(data + 12) > (size - ECO_HEADER) / PRESSURE_RECORD
        || size != ECO_HEADER + Read32(data + 12) * PRESSURE_RECORD) return -1;
    const uint8_t *run = RhhModsState("rhh:host_run");
    if (!run || Read64(run + 8) != Read64(data + 16)) return -1;
    uint64_t lastMap = 0, lastSpecies = 0;
    for (uint32_t i = 0; i < Read32(data + 12); ++i) {
        const uint8_t *p = data + ECO_HEADER + i * PRESSURE_RECORD;
        uint64_t map = Read64(p), species = Read64(p + 8), deficit = Read64(p + 16);
        if (!map || !species || deficit > (uint64_t)PRESSURE_BASE * 3600
            || (i && (map < lastMap || (map == lastMap && species <= lastSpecies)))) return -1;
        lastMap = map; lastSpecies = species;
    }
    return 0;
}
static int PressureExport(uint8_t **out, uint32_t *size)
{
    if (!out || !size) return -1;
    *out = NULL; *size = 0;
    if (!sPressureCount) return 0;
    if (sPressureCount > (UINT32_MAX - ECO_HEADER) / PRESSURE_RECORD) return -1;
    uint32_t length = ECO_HEADER + sPressureCount * PRESSURE_RECORD;
    uint8_t *data = calloc(length, 1); if (!data) return -1;
    memcpy(data, "RHHECP01", 8); Write32(data + 8, length); Write32(data + 12, sPressureCount); Write64(data + 16, sPressureRun);
    for (size_t i = 0; i < sPressureCount; ++i) {
        uint8_t *p = data + ECO_HEADER + i * PRESSURE_RECORD;
        Write64(p, sPressure[i].map); Write64(p + 8, sPressure[i].species);
        Write64(p + 16, sPressure[i].deficit); Write64(p + 24, sPressure[i].updated);
    }
    if (PressureValidate(data, length)) { free(data); return -1; }
    *out = data; *size = length; return 0;
}
static int PressureImport(const uint8_t *data, uint32_t size)
{
    if (PressureValidate(data, size)) return -1;
    if (!size) { PressureReset(); return 0; }
    uint32_t count = Read32(data + 12);
    struct Pressure *entries = calloc(count, sizeof(*entries)); if (!entries) return -1;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *p = data + ECO_HEADER + i * PRESSURE_RECORD;
        entries[i] = (struct Pressure){Read64(p), Read64(p + 8), Read64(p + 16), Read64(p + 24)};
    }
    PressureReset(); sPressure = entries; sPressureCount = sPressureCapacity = count; sPressureRun = Read64(data + 16); return 0;
}
static const struct NativeSaveArchiveCodec sPressureCodec = {PressureExport, PressureValidate, PressureImport, PressureReset, "rhh:encounter_pressure", NULL};
static uint64_t PressureRemaining(const struct Pressure *entry, uint64_t now)
{
    if (!entry->updated || !now || now <= entry->updated) return entry->deficit;
    uint64_t elapsed = now - entry->updated;
    uint32_t rate = PressureSetting(entry->map, "rhh:ecology/recovery_per_hour", PRESSURE_RECOVERY_PER_HOUR);
    if (!rate) return entry->deficit;
    if (elapsed >= (entry->deficit + rate - 1) / rate) return 0;
    return entry->deficit - elapsed * rate;
}
static void PressureSettleMap(uint64_t map, uint64_t now)
{
    if (sPressureRun && sPressureRun != RhhModsRunId()) return;
    for (size_t i = 0; i < sPressureCount; ++i)
    {
        struct Pressure *entry = &sPressure[i];
        if (entry->map != map) continue;
        entry->deficit = PressureRemaining(entry, now);
        if (!now) entry->updated = 0;
        else if (now > entry->updated) entry->updated = now;
    }
}
uint32_t RhhEcologyFrequencyAt(uint64_t map, uint64_t species, uint64_t now)
{
    if (!Ready() || (sPressureRun && sPressureRun != RhhModsRunId())
        || Canonical(&map, RHH_ECOLOGY_POPULATION, &species)) return PRESSURE_BASE;
    size_t i = PressureFind(map, species);
    if (i == sPressureCount || sPressure[i].map != map || sPressure[i].species != species) return PRESSURE_BASE;
    uint64_t deficit = PressureRemaining(&sPressure[i], now);
    return PRESSURE_BASE - (deficit + 3599) / 3600;
}
int RhhEcologyDepleteAt(uint64_t map, uint64_t species, uint64_t now)
{
    if (!Writable() || (sPressureRun && sPressureRun != RhhModsRunId())
        || Canonical(&map, RHH_ECOLOGY_POPULATION, &species)) return -1;
    size_t i = PressureFind(map, species);
    if (i == sPressureCount || sPressure[i].map != map || sPressure[i].species != species) {
        if (sPressureCount >= (UINT32_MAX - ECO_HEADER) / PRESSURE_RECORD) return -1;
        if (sPressureCount == sPressureCapacity) {
            size_t capacity = sPressureCapacity ? sPressureCapacity * 2 : 16;
            if (capacity > SIZE_MAX / sizeof(*sPressure)) return -1;
            void *p = realloc(sPressure, capacity * sizeof(*sPressure)); if (!p) return -1;
            sPressure = p; sPressureCapacity = capacity;
        }
        memmove(sPressure + i + 1, sPressure + i, (sPressureCount - i) * sizeof(*sPressure));
        sPressure[i] = (struct Pressure){map, species, 0, 0}; ++sPressureCount; sPressureRun = RhhModsRunId();
    }
    struct Pressure *entry = &sPressure[i];
    entry->deficit = PressureRemaining(entry, now);
    uint64_t drop = (uint64_t)PressureSetting(map, "rhh:ecology/reduction", PRESSURE_DROP) * 3600;
    uint64_t maximum = (uint64_t)PRESSURE_BASE * 3600;
    entry->deficit = entry->deficit > maximum - drop ? maximum : entry->deficit + drop;
    if (now > entry->updated) entry->updated = now;
    return 0;
}

uint64_t RhhEcologySystemTime(void)
{
    time_t now = time(NULL);
    return now > 0 ? (uint64_t)now : 0;
}
void RhhEcologyResumeClock(uint64_t now)
{
    if (!now || !Writable() || (sPressureRun && sPressureRun != RhhModsRunId())) return;
    for (size_t i = 0; i < sPressureCount; ++i)
    {
        struct Pressure *entry = &sPressure[i];
        if (now > entry->updated)
        {
            entry->deficit = PressureRemaining(entry, now);
            entry->updated = now;
        }
    }
}
void RhhEcologyOnFrame(void)
{
    if (!sPressureCount || !Writable()) return;
    uint64_t now = RhhEcologySystemTime();
    if (!now) { sClockUnavailable = 1; return; }
    if (sClockUnavailable)
    {
        /* An interval without a valid sample cannot be reconstructed. */
        for (size_t i = 0; i < sPressureCount; ++i)
            if (now > sPressure[i].updated) sPressure[i].updated = now;
        sClockUnavailable = 0; sLastClockSample = now; return;
    }
    if (now != sLastClockSample)
    { RhhEcologyResumeClock(now); sLastClockSample = now; }
}
