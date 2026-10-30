#ifndef RHH_EXAMPLE_MOD_COMMON_H
#define RHH_EXAMPLE_MOD_COMMON_H
/* Shared example helpers. Only public mod API and standard C dependencies. */
#include "mod_api.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

static const struct RhhModHostV1 *sHost;
static inline uint32_t Read32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline uint64_t Read64(const uint8_t *p) { return Read32(p) | (uint64_t)Read32(p + 4) << 32; }
static inline void Write64(uint8_t *p, uint64_t n) { for (unsigned i = 0; i < 8; ++i) p[i] = n >> (i * 8); }
static inline unsigned Read16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static inline void Write16(uint8_t *p, unsigned n) { p[0] = n; p[1] = n >> 8; }
static inline int HostOK(const struct RhhModHostV1 *host)
{
    if (!host || host->abi_version != RHH_MOD_ABI_V1 || host->struct_size < RHH_MOD_HOST_V1_BASE_SIZE) return 0;
    sHost = host; return 1;
}
static inline int ReadByte(const void *state, const char *id, const char *key, unsigned type, struct RhhModValue *out)
{
    if (strcmp(id, key)) return -1;
    *out = (struct RhhModValue){type, 0, *(const uint8_t *)state}; return 0;
}
static inline int BoolRead(const void *state, const char *id, struct RhhModValue *out)
{ return ReadByte(state, id, "enabled", RHH_SETTING_BOOL, out); }
static inline int BoolWrite(void *state, const char *id, const struct RhhModValue *value)
{
    if (strcmp(id, "enabled") || value->type != RHH_SETTING_BOOL || value->value > 1) return -1;
    *(uint8_t *)state = value->value; return 0;
}
static inline int BoolValidate(const uint8_t *state, uint32_t size) { return size == 1 && state[0] <= 1 ? 0 : -1; }
#define BOOL_SETTING(label_, description_) {sizeof(struct RhhModSetting), RHH_SETTING_BOOL, RHH_SETTING_NEW_GAME, 0, "enabled", label_, description_, 0, 1, 0, 0, NULL}
#define REDUCER(event_, stage_, priority_, fn_) {sizeof(struct RhhModHook), event_, stage_, priority_, fn_, NULL}
#define OBSERVER(event_, fn_) {sizeof(struct RhhModHook), event_, RHH_OBSERVE, 0, NULL, fn_}
#ifdef RHH_MOD_SHARED
#ifdef _WIN32
#define MOD_EXPORT __declspec(dllexport)
#else
#define MOD_EXPORT __attribute__((visibility("default")))
#endif
#define SHARED_ENTRY(factory_) MOD_EXPORT const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host) { return factory_(host); }
#else
#define SHARED_ENTRY(factory_)
#endif

/* Frozen, canonical, creatable species pool. Aliases are not extra weight. */
enum { POOL_CAPACITY = 2048, POOL_OFFSET = 16, POOL_STATE_SIZE = POOL_OFFSET + POOL_CAPACITY * 8 };
struct PoolEntry { uint64_t key; const char *name; };
static inline int ComparePool(const void *a, const void *b)
{ return strcmp(((const struct PoolEntry *)a)->name, ((const struct PoolEntry *)b)->name); }
static inline int BuildPool(uint8_t *state)
{
    struct PoolEntry entries[POOL_CAPACITY]; unsigned count = 0;
    for (unsigned i = 0; i < sHost->content_count(RHH_SPECIES); ++i)
    {
        struct RhhContentInfo info = {.struct_size = sizeof(info)};
        if (sHost->content_at(RHH_SPECIES, i, &info) || info.key != info.canonical_key
            || !(info.flags & RHH_CONTENT_CREATABLE)) continue;
        if (count == POOL_CAPACITY) return -1;
        uint32_t id; if (sHost->resolve(RHH_SPECIES, info.key, &id)) return -1;
        entries[count++] = (struct PoolEntry){info.key, sHost->content_name(RHH_SPECIES, id)};
    }
    if (!count) return -1;
    qsort(entries, count, sizeof(*entries), ComparePool);
    Write16(state + 2, count);
    for (unsigned i = 0; i < count; ++i) Write64(state + POOL_OFFSET + i * 8, entries[i].key);
    return 0;
}
static inline int ValidatePool(const uint8_t *state, uint32_t size, unsigned maxMode)
{
    if (size != POOL_STATE_SIZE) return -1;
    unsigned count = Read16(state + 2);
    if (state[0] > maxMode || count > POOL_CAPACITY || (state[0] && !count)) return -1;
    for (unsigned i = 0; i < count; ++i)
    {
        struct RhhContentInfo info = {.struct_size = sizeof(info)};
        if (sHost->content_info(RHH_SPECIES, Read64(state + POOL_OFFSET + i * 8), &info)
            || !(info.flags & RHH_CONTENT_CREATABLE)) return -1;
    }
    return 0;
}
#endif
