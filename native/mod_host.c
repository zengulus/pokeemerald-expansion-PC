#include "mod_host.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>

enum { MAX_MODS = 32, MAX_HOOKS = 256, RECORD_HEADER = 80, FILE_HEADER = 24 };
struct Mod { const struct RhhModDefinition *def; uint8_t *state; };
struct Hook { unsigned mod, index, rank; };
struct TraceCache { int valid; struct RhhModEvent event; struct RhhModDecision before, after; };
static struct Mod sMods[MAX_MODS];
static struct Hook sHooks[MAX_HOOKS];
static struct TraceCache sLastTrace[MAX_HOOKS];
static unsigned sModCount, sHookCount;
static int sFinalized;
static FILE *sTrace;
static uint64_t sTraceSequence;
static char sError[192];
static const struct RhhContentBinding *sContent;
static size_t sContentCount;
struct ContentIndex { uint64_t key; const struct RhhContentBinding *binding; };
static struct ContentIndex *sContentIndex;
static void (*sMetadata)(uint32_t, uint32_t, struct RhhContentInfo *);
static int (*sPersist)(uint64_t, const uint8_t *, uint32_t);
static uint64_t sRunSeed, sRunId;
static int sCommitted, sSettingsLocked;

static int Fail(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(sError, sizeof(sError), format, args);
    va_end(args);
    if (sTrace) fprintf(sTrace, "{\"event\":\"error\",\"reason\":\"%s\"}\n", sError);
    return -1;
}
static int ValidName(const char *s)
{
    if (!s || !*s || strlen(s) >= RHH_MOD_ID_MAX) return 0;
    for (; *s; ++s)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9')
            || *s == ':' || *s == '/' || *s == '-' || *s == '_' || *s == '.')) return 0;
    return 1;
}
RhhContentKey RhhContentKeyOf(const char *name)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    if (!name) return 0;
    for (; *name; ++name) hash = (hash ^ (uint8_t)*name) * UINT64_C(1099511628211);
    return hash;
}
static uint64_t Mix(uint64_t x)
{
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}
uint64_t RhhModRandomAt(uint64_t seed, const char *id, uint64_t key, uint64_t ordinal)
{
    return Mix(seed ^ Mix(RhhContentKeyOf(id)) ^ Mix(key) ^ Mix(ordinal + UINT64_C(0x9e3779b97f4a7c15)));
}
static int CompareContent(const void *a, const void *b)
{
    uint64_t ka = ((const struct ContentIndex *)a)->key, kb = ((const struct ContentIndex *)b)->key;
    return (ka > kb) - (ka < kb);
}
int RhhModsSetContent(const struct RhhContentBinding *bindings, size_t count)
{
    struct ContentIndex *index = calloc(count ? count : 1, sizeof(*index));
    if (!index) return Fail("cannot allocate content registry");
    for (size_t i = 0; i < count; ++i)
    {
        if (!bindings[i].name || strlen(bindings[i].name) > 127)
        { free(index); return Fail("invalid content name"); }
        index[i] = (struct ContentIndex){RhhContentKeyOf(bindings[i].name), &bindings[i]};
        if (!index[i].key) { free(index); return Fail("zero content key is reserved"); }
    }
    qsort(index, count, sizeof(*index), CompareContent);
    for (size_t i = 1; i < count; ++i)
        if (index[i].key == index[i - 1].key)
        { free(index); return Fail("duplicate or colliding content identity"); }
    free(sContentIndex);
    sContentIndex = index;
    sContent = bindings;
    sContentCount = count;
    return 0;
}
int RhhContentResolve(uint32_t kind, RhhContentKey key, uint32_t *id)
{
    struct ContentIndex query = {.key = key};
    const struct ContentIndex *found = bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent);
    if (found && found->binding->kind == kind)
    { if (id) *id = found->binding->engine_id; return 0; }
    return -1;
}
const char *RhhContentName(uint32_t kind, uint32_t id)
{
    for (size_t i = 0; i < sContentCount; ++i)
        if (sContent[i].kind == kind && sContent[i].engine_id == id) return sContent[i].name;
    return NULL;
}
RhhContentKey RhhContentKeyForId(uint32_t kind, uint32_t id)
{
    const char *name = RhhContentName(kind, id);
    return name ? RhhContentKeyOf(name) : 0;
}
void RhhModsSetContentMetadata(void (*provider)(uint32_t, uint32_t, struct RhhContentInfo *)) { sMetadata = provider; }
uint32_t RhhContentCount(uint32_t kind)
{
    uint32_t n = 0;
    for (size_t i = 0; i < sContentCount; ++i) if (sContent[i].kind == kind) ++n;
    return n;
}
static int ContentInfo(const struct RhhContentBinding *binding, struct RhhContentInfo *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    memset(out, 0, sizeof(*out)); out->struct_size = sizeof(*out);
    out->kind = binding->kind; out->key = RhhContentKeyOf(binding->name);
    out->canonical_key = RhhContentKeyForId(binding->kind, binding->engine_id);
    out->flags = RHH_CONTENT_ENABLED;
    if (sMetadata) sMetadata(binding->kind, binding->engine_id, out);
    return 0;
}
int RhhContentAt(uint32_t kind, uint32_t index, struct RhhContentInfo *out)
{
    for (size_t i = 0; i < sContentCount; ++i)
        if (sContent[i].kind == kind && !index--) return ContentInfo(&sContent[i], out);
    return -1;
}
int RhhContentInfoFor(uint32_t kind, RhhContentKey key, struct RhhContentInfo *out)
{
    struct ContentIndex query = {.key = key};
    const struct ContentIndex *found = bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent);
    return found && found->binding->kind == kind ? ContentInfo(found->binding, out) : -1;
}
static const struct RhhModHostV1 sAPI = {
    sizeof(sAPI), RHH_MOD_ABI_V1, sizeof(void *) * 8, 0,
    RhhContentKeyOf, RhhContentResolve, RhhContentName, RhhModRandomAt,
    RhhContentCount, RhhContentAt, RhhContentInfoFor, RhhModsGetSetting
};
const struct RhhModHostV1 *RhhModsHostAPI(void) { return &sAPI; }
const char *RhhModsError(void) { return sError; }
void RhhModsSetTrace(FILE *file) { sTrace = file; sTraceSequence = 0; memset(sLastTrace, 0, sizeof(sLastTrace)); }
void RhhModsShutdown(void)
{
    for (unsigned i = 0; i < sModCount; ++i) free(sMods[i].state);
    memset(sMods, 0, sizeof(sMods));
    sModCount = sHookCount = sFinalized = 0;
    memset(sLastTrace, 0, sizeof(sLastTrace));
    sContent = NULL;
    sContentCount = 0;
    sMetadata = NULL; sPersist = NULL; sRunSeed = sRunId = 0; sCommitted = sSettingsLocked = 0;
    free(sContentIndex); sContentIndex = NULL;
}
void RhhModsInit(void) { RhhModsShutdown(); sError[0] = 0; }
static int FindMod(const char *id)
{
    if (!id) return -1;
    for (unsigned i = 0; i < sModCount; ++i)
        if (!strcmp(sMods[i].def->id, id)) return i;
    return -1;
}
int RhhModsGetSetting(const char *id, const char *setting, struct RhhModValue *out)
{
    int m = FindMod(id);
    if (m < 0 || !setting || !sMods[m].def->read_setting || !out) return -1;
    return sMods[m].def->read_setting(sMods[m].state, setting, out);
}
int RhhModsConfigure(const char *id, const char *setting, const struct RhhModValue *value)
{
    int m = FindMod(id);
    if (m < 0 || !setting || !value || value->reserved) return Fail("unknown module or invalid setting value");
    const struct RhhModDefinition *d = sMods[m].def;
    const struct RhhModSetting *found = NULL;
    for (unsigned i = 0; i < d->setting_count; ++i)
        if (!strcmp(setting, d->settings[i].id)) found = &d->settings[i];
    if (!found || !d->write_setting || found->type != value->type
        || (sSettingsLocked && found->scope == RHH_SETTING_NEW_GAME)) return Fail("setting is unavailable or locked");
    if (found->type != RHH_SETTING_CONTENT && (value->value < found->minimum || value->value > found->maximum))
        return Fail("setting value is out of range");
    uint8_t *staged = malloc(d->state_size ? d->state_size : 1);
    if (!staged) return Fail("cannot stage setting change");
    memcpy(staged, sMods[m].state, d->state_size);
    int result = d->write_setting(staged, setting, value);
    if (!result && d->validate) result = d->validate(staged, d->state_size);
    if (!result) memcpy(sMods[m].state, staged, d->state_size);
    free(staged);
    if (result) return Fail("setting rejected by %s", id);
    if (sTrace) fprintf(sTrace, "{\"event\":\"setting\",\"mod\":\"%s\",\"setting\":\"%s\",\"value\":%" PRIu64 "}\n", id, setting, value->value);
    return 0;
}
int RhhModsRegister(const struct RhhModDefinition *def)
{
    if (sFinalized) return Fail("registration is closed");
    if (!def || def->struct_size < sizeof(*def) || def->abi_version != RHH_MOD_ABI_V1)
        return Fail("unsupported mod ABI");
    if (!ValidName(def->id) || FindMod(def->id) >= 0) return Fail("invalid or duplicate mod id");
    if (def->flags & ~(RHH_MOD_REQUIRED | RHH_MOD_DURABLE)) return Fail("unsupported mod flags");
    if ((def->flags & RHH_MOD_DURABLE) && !def->merge) return Fail("durable module requires merge callback");
    if (def->setting_count > 16 || (def->setting_count && (!def->settings || !def->read_setting || !def->write_setting)))
        return Fail("invalid module settings");
    for (unsigned i = 0; i < def->setting_count; ++i)
    {
        const struct RhhModSetting *s = &def->settings[i];
        if (s->struct_size < sizeof(*s) || !ValidName(s->id) || !s->label || !s->description
            || s->type < RHH_SETTING_BOOL || s->type > RHH_SETTING_CONTENT
            || s->scope > RHH_SETTING_RUNTIME || s->choice_count > 32 || s->minimum > s->maximum
            || (s->choice_count && !s->choices)) return Fail("invalid setting descriptor");
        for (unsigned j = 0; j < i; ++j) if (!strcmp(s->id, def->settings[j].id)) return Fail("duplicate setting id");
    }
    if (sModCount == MAX_MODS || def->state_size > RHH_MOD_MAX_STATE
        || def->hook_count > MAX_HOOKS - sHookCount || def->dependency_count > MAX_MODS
        || (def->hook_count && !def->hooks) || (def->dependency_count && !def->dependencies))
        return Fail("mod exceeds host limits");
    for (unsigned i = 0; i < def->dependency_count; ++i)
        if (!ValidName(def->dependencies[i])) return Fail("invalid dependency id");
    for (unsigned i = 0; i < def->hook_count; ++i)
    {
        const struct RhhModHook *h = &def->hooks[i];
        if (h->struct_size < sizeof(*h) || !h->event || h->event >= RHH_MOD_EVENT_COUNT
            || h->stage > RHH_OBSERVE || (h->stage == RHH_OBSERVE ? !h->observe : !h->reduce))
            return Fail("invalid hook in %s", def->id);
        int notification = h->event == RHH_ENCOUNTER_STARTED || h->event == RHH_MON_FAINTED
            || h->event == RHH_RUN_STARTED || h->event == RHH_RUN_LOADED
            || h->event == RHH_INVENTORY_CHANGED || h->event == RHH_WHITEOUT
            || h->event == RHH_WILD_CREATED || h->event == RHH_PERSISTENCE_FAILED || h->event == RHH_MON_CAUGHT;
        if ((h->stage == RHH_OBSERVE) != notification)
            return Fail("mutable observers require committed notification events");
    }
    uint8_t *state = calloc(def->state_size ? def->state_size : 1, 1);
    if (!state) return Fail("cannot allocate mod state");
    if (def->validate && def->validate(state, def->state_size))
    { free(state); return Fail("zero default state is invalid for %s", def->id); }
    sMods[sModCount++] = (struct Mod){def, state};
    sHookCount += def->hook_count;
    return 0;
}
static int CompareHooks(const void *va, const void *vb)
{
    const struct Hook *a = va, *b = vb;
    const struct RhhModHook *ha = &sMods[a->mod].def->hooks[a->index];
    const struct RhhModHook *hb = &sMods[b->mod].def->hooks[b->index];
    if (ha->stage != hb->stage) return ha->stage < hb->stage ? -1 : 1;
    if (a->rank != b->rank) return a->rank < b->rank ? -1 : 1;
    if (ha->priority != hb->priority) return ha->priority < hb->priority ? -1 : 1;
    int id = strcmp(sMods[a->mod].def->id, sMods[b->mod].def->id);
    if (id) return id;
    return (a->index > b->index) - (a->index < b->index);
}
int RhhModsFinalize(void)
{
    unsigned done = 0, rank[MAX_MODS] = {0}, n = 0;
    while (n < sModCount)
    {
        int best = -1;
        for (unsigned i = 0; i < sModCount; ++i)
        {
            if (done & (1u << i)) continue;
            int ready = 1;
            for (unsigned d = 0; d < sMods[i].def->dependency_count; ++d)
            {
                int dep = FindMod(sMods[i].def->dependencies[d]);
                if (dep < 0) return Fail("missing dependency for %s", sMods[i].def->id);
                if (!(done & (1u << dep))) ready = 0;
            }
            if (ready && (best < 0 || strcmp(sMods[i].def->id, sMods[best].def->id) < 0)) best = i;
        }
        if (best < 0) return Fail("mod dependency cycle");
        /* Dependency depth, not registration order, controls execution. */
        for (unsigned d = 0; d < sMods[best].def->dependency_count; ++d)
        {
            unsigned depth = rank[FindMod(sMods[best].def->dependencies[d])] + 1;
            if (depth > rank[best]) rank[best] = depth;
        }
        done |= 1u << best;
        ++n;
    }
    n = 0;
    for (unsigned m = 0; m < sModCount; ++m)
        for (unsigned h = 0; h < sMods[m].def->hook_count; ++h)
            sHooks[n++] = (struct Hook){m, h, rank[m]};
    qsort(sHooks, sHookCount, sizeof(*sHooks), CompareHooks);
    if (sTrace)
        for (unsigned i = 0; i < sHookCount; ++i)
        {
            const struct RhhModDefinition *m = sMods[sHooks[i].mod].def;
            const struct RhhModHook *h = &m->hooks[sHooks[i].index];
            fprintf(sTrace, "{\"event\":\"hook_order\",\"index\":%u,\"mod\":\"%s\",\"hook\":%u,\"stage\":%u,\"dependency_depth\":%u,\"priority\":%d}\n",
                i, m->id, h->event, h->stage, sHooks[i].rank, h->priority);
        }
    sFinalized = 1;
    return 0;
}
void *RhhModsState(const char *id)
{
    int i = FindMod(id);
    return i < 0 ? NULL : sMods[i].state;
}
void RhhModsResetState(void)
{
    memset(sLastTrace, 0, sizeof(sLastTrace));
    sSettingsLocked = sCommitted = 0; sRunSeed = sRunId = 0;
    for (unsigned i = 0; i < sModCount; ++i) memset(sMods[i].state, 0, sMods[i].def->state_size);
}
static uint32_t Checksum(const uint8_t *p, uint32_t size)
{
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < size; ++i) hash = (hash ^ p[i]) * 16777619u;
    return hash;
}
static int SameDecision(const struct RhhModDecision *a, const struct RhhModDecision *b)
{
    return a->allowed == b->allowed && a->value == b->value
        && !memcmp(a->stats, b->stats, sizeof(a->stats)) && !strcmp(a->reason, b->reason);
}
void RhhModsDispatch(const struct RhhModEvent *event, struct RhhModDecision *decision)
{
    if (!sFinalized || event->struct_size < sizeof(*event) || decision->struct_size < sizeof(*decision)) return;
    if (event->kind == RHH_RUN_STARTED || event->kind == RHH_RUN_LOADED) sSettingsLocked = 1;
    int durableChanged = 0;
    if (sTrace && (event->kind == RHH_ENCOUNTER_STARTED || event->kind == RHH_MON_FAINTED
        || event->kind == RHH_RUN_STARTED || event->kind == RHH_RUN_LOADED))
    {
        fprintf(sTrace, "{\"seq\":%" PRIu64 ",\"event\":\"notification\",\"kind\":%u,\"seed\":\"%016" PRIx64 "\",\"key\":\"%016" PRIx64 "\",\"subject\":\"%016" PRIx64 "\",\"ordinal\":%" PRIu64 ",\"value\":%" PRIu64 "}\n",
            ++sTraceSequence, event->kind, event->run_seed, event->event_key, event->subject, event->ordinal, decision->value);
        fflush(sTrace);
    }
    for (unsigned i = 0; i < sHookCount; ++i)
    {
        struct Mod *m = &sMods[sHooks[i].mod];
        const struct RhhModHook *h = &m->def->hooks[sHooks[i].index];
        if (h->event != event->kind) continue;
        struct RhhModDecision before = *decision;
        uint32_t stateBefore = h->stage == RHH_OBSERVE && (sTrace || (m->def->flags & RHH_MOD_DURABLE))
            ? Checksum(m->state, m->def->state_size) : 0;
        if (h->stage == RHH_OBSERVE) h->observe(event, decision, m->state);
        else
        {
            h->reduce(event, m->state, decision);
            decision->allowed = before.allowed && decision->allowed;
            if (event->kind == RHH_LEVEL_CAP && decision->value > before.value) decision->value = before.value;
            if (event->kind == RHH_MON_IS_DEAD) decision->value = before.value || decision->value;
            decision->reason[sizeof(decision->reason) - 1] = 0;
            if (!before.allowed) memcpy(decision->reason, before.reason, sizeof(decision->reason));
        }
        if (h->stage == RHH_OBSERVE && (m->def->flags & RHH_MOD_DURABLE)
            && stateBefore != Checksum(m->state, m->def->state_size)) durableChanged = 1;
        if (sTrace)
        {
            struct TraceCache *cached = &sLastTrace[i];
            if (h->stage != RHH_OBSERVE)
            {
                if (SameDecision(&before, decision)) continue;
                if (cached->valid && !memcmp(event, &cached->event, sizeof(*event))
                    && SameDecision(&before, &cached->before) && SameDecision(decision, &cached->after)) continue;
                stateBefore = Checksum(m->state, m->def->state_size);
                cached->valid = 1; cached->event = *event; cached->before = before; cached->after = *decision;
            }
            /* Module reasons are escaped so a trace remains valid JSONL. */
            char reason[sizeof(decision->reason) * 2]; unsigned at = 0;
            for (unsigned j = 0; j < sizeof(decision->reason) && decision->reason[j]; ++j)
            {
                unsigned char c = decision->reason[j];
                if (c == '"' || c == '\\') reason[at++] = '\\';
                if (c >= 32) reason[at++] = c;
            }
            reason[at] = 0;
            fprintf(sTrace, "{\"seq\":%" PRIu64 ",\"event\":%u,\"mod\":\"%s\",\"version\":%u,\"stage\":%u,\"seed\":\"%016" PRIx64 "\",\"key\":\"%016" PRIx64 "\",\"subject\":\"%016" PRIx64 "\",\"ordinal\":%" PRIu64 ",\"args\":[%u,%u,%u,%u],\"before\":\"%016" PRIx64 "\",\"after\":\"%016" PRIx64 "\",\"allowed\":%s,\"reason\":\"%s\",\"state_before\":%u,\"state_after\":%u,\"stats_before\":[%u,%u,%u,%u,%u,%u],\"stats_after\":[%u,%u,%u,%u,%u,%u]}\n",
                ++sTraceSequence, event->kind, m->def->id, m->def->mod_version, h->stage,
                event->run_seed, event->event_key, event->subject, event->ordinal,
                event->args[0], event->args[1], event->args[2], event->args[3],
                before.value, decision->value, decision->allowed ? "true" : "false", reason,
                stateBefore, h->stage == RHH_OBSERVE ? Checksum(m->state, m->def->state_size) : stateBefore,
                before.stats[0], before.stats[1], before.stats[2], before.stats[3], before.stats[4], before.stats[5],
                decision->stats[0], decision->stats[1], decision->stats[2], decision->stats[3], decision->stats[4], decision->stats[5]);
            fflush(sTrace);
        }
    }
    if (durableChanged && sCommitted && sPersist && event->kind != RHH_PERSISTENCE_FAILED && RhhModsFlushDurable())
    {
        struct RhhModEvent failed = {.struct_size = sizeof(failed), .kind = RHH_PERSISTENCE_FAILED, .run_seed = sRunSeed};
        struct RhhModDecision out = {.struct_size = sizeof(out), .allowed = 1};
        RhhModsDispatch(&failed, &out);
    }
}
static void Put32(uint8_t *p, uint32_t n) { for (unsigned i = 0; i < 4; ++i) p[i] = n >> (8 * i); }
static uint32_t Get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int ExportRecords(uint8_t **data, uint32_t *size, uint32_t mask)
{
    uint32_t length = FILE_HEADER;
    unsigned count = 0;
    for (unsigned i = 0; i < sModCount; ++i)
    {
        const struct Mod *m = &sMods[i];
        if (mask && !(m->def->flags & mask)) continue;
        if (m->def->validate && m->def->validate(m->state, m->def->state_size))
            return Fail("invalid live state for %s", m->def->id);
        length += RECORD_HEADER + m->def->state_size;
        ++count;
    }
    uint8_t *buffer = calloc(length, 1);
    if (!buffer) return Fail("cannot allocate save records");
    memcpy(buffer, "RHHMOD01", 8);
    Put32(buffer + 8, length);
    Put32(buffer + 12, count);
    uint32_t at = FILE_HEADER;
    /* Stable save ordering, independent of library discovery/registration. */
    const char *previous = "";
    for (unsigned n = 0; n < count; ++n)
    {
        int best = -1;
        for (unsigned i = 0; i < sModCount; ++i)
            if ((!mask || (sMods[i].def->flags & mask)) && strcmp(sMods[i].def->id, previous) > 0
                && (best < 0 || strcmp(sMods[i].def->id, sMods[best].def->id) < 0)) best = i;
        const struct Mod *m = &sMods[best];
        strcpy((char *)buffer + at, m->def->id);
        Put32(buffer + at + 64, m->def->state_schema);
        Put32(buffer + at + 68, m->def->state_size);
        Put32(buffer + at + 72, m->def->flags);
        Put32(buffer + at + 76, m->def->mod_version);
        memcpy(buffer + at + RECORD_HEADER, m->state, m->def->state_size);
        at += RECORD_HEADER + m->def->state_size;
        previous = m->def->id;
    }
    Put32(buffer + 16, Checksum(buffer + FILE_HEADER, length - FILE_HEADER));
    *data = buffer; *size = length;
    return 0;
}
static int ImportRecords(const uint8_t *data, uint32_t size, int merge)
{
    uint8_t *staged[MAX_MODS] = {0};
    uint32_t seen = 0;
    int result = -1;
    if (size < FILE_HEADER || memcmp(data, "RHHMOD01", 8) || Get32(data + 8) != size
        || Get32(data + 12) > MAX_MODS || Get32(data + 20)
        || Checksum(data + FILE_HEADER, size - FILE_HEADER) != Get32(data + 16))
        return Fail("invalid mod save envelope");
    for (unsigned i = 0; i < sModCount; ++i)
    {
        if (!(staged[i] = calloc(sMods[i].def->state_size ? sMods[i].def->state_size : 1, 1)))
        { Fail("cannot stage mod migration"); goto cleanup; }
        if (merge) memcpy(staged[i], sMods[i].state, sMods[i].def->state_size);
    }
    uint32_t at = FILE_HEADER;
    for (uint32_t n = 0; n < Get32(data + 12); ++n)
    {
        if (size - at < RECORD_HEADER || !memchr(data + at, 0, 64) || !ValidName((const char *)data + at))
        { Fail("invalid mod save record"); goto cleanup; }
        uint32_t schema = Get32(data + at + 64), length = Get32(data + at + 68);
        uint32_t flags = Get32(data + at + 72);
        if (flags & ~(RHH_MOD_REQUIRED | RHH_MOD_DURABLE)) { Fail("unsupported saved mod flags"); goto cleanup; }
        if (length > RHH_MOD_MAX_STATE || length > size - at - RECORD_HEADER)
        { Fail("invalid mod state length"); goto cleanup; }
        int m = FindMod((const char *)data + at);
        if (m < 0 && (flags & RHH_MOD_REQUIRED))
        { Fail("save requires missing mod %s", data + at); goto cleanup; }
        if (m >= 0)
        {
            const struct RhhModDefinition *d = sMods[m].def;
            if (merge && (!(d->flags & RHH_MOD_DURABLE) || !d->merge))
            { Fail("journal contains non-durable module"); goto cleanup; }
            if (seen & (1u << m)) { Fail("duplicate saved mod"); goto cleanup; }
            seen |= 1u << m;
            uint8_t *incoming = calloc(d->state_size ? d->state_size : 1, 1);
            if (!incoming) { Fail("cannot stage incoming module"); goto cleanup; }
            int bad = 0;
            if (schema == d->state_schema && length == d->state_size) memcpy(incoming, data + at + RECORD_HEADER, length);
            else if (!d->migrate || d->migrate(schema, data + at + RECORD_HEADER, length, incoming, d->state_size)) bad = 1;
            if (!bad && d->validate) bad = d->validate(incoming, d->state_size);
            if (!bad)
            {
                if (merge) bad = d->merge(staged[m], incoming, d->state_size);
                else memcpy(staged[m], incoming, d->state_size);
            }
            free(incoming);
            if (bad) { Fail("state migration or merge failed for %s", d->id); goto cleanup; }
            if (d->validate && d->validate(staged[m], d->state_size))
            { Fail("state validation failed for %s", d->id); goto cleanup; }
            if (sTrace) fprintf(sTrace, "{\"event\":\"load_state\",\"mod\":\"%s\",\"from_schema\":%u,\"to_schema\":%u}\n", d->id, schema, d->state_schema);
        }
        at += RECORD_HEADER + length;
    }
    if (at != size) { Fail("trailing mod save bytes"); goto cleanup; }
    for (unsigned i = 0; i < sModCount; ++i)
        if (sMods[i].def->validate && sMods[i].def->validate(staged[i], sMods[i].def->state_size))
        { Fail("default or merged state is invalid for %s", sMods[i].def->id); goto cleanup; }
    for (unsigned i = 0; i < sModCount; ++i) memcpy(sMods[i].state, staged[i], sMods[i].def->state_size);
    if (!merge) sSettingsLocked = 1;
    result = 0;
cleanup:
    for (unsigned i = 0; i < sModCount; ++i) free(staged[i]);
    return result;
}
int RhhModsExport(uint8_t **data, uint32_t *size) { return ExportRecords(data, size, 0); }
int RhhModsImport(const uint8_t *data, uint32_t size) { return ImportRecords(data, size, 0); }
int RhhModsMergeJournal(const uint8_t *data, uint32_t size) { return ImportRecords(data, size, 1); }
void RhhModsSetPersistence(int (*persist)(uint64_t, const uint8_t *, uint32_t)) { sPersist = persist; }
void RhhModsSetRunContext(uint64_t seed, uint64_t runId, int committed)
{ sRunSeed = seed; sRunId = runId; sCommitted = committed; }
int RhhModsFlushDurable(void)
{
    if (!sCommitted || !sPersist) return 0;
    uint8_t *data; uint32_t size;
    if (ExportRecords(&data, &size, RHH_MOD_DURABLE)) return -1;
    int result = sPersist(sRunId, data, size); free(data);
    return result;
}
