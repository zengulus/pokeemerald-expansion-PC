#include "common.h"
enum { HEADER = 32, AREA_LIMIT = 512, DEAD_LIMIT = 2048,
    AREAS = HEADER, DEAD = AREAS + AREA_LIMIT * 8, SIZE = DEAD + DEAD_LIMIT * 8,
    OLD_AREA_LIMIT = 32, OLD_DEAD_LIMIT = 128, OLD_DEAD = HEADER + OLD_AREA_LIMIT * 8,
    OLD_SIZE = OLD_DEAD + OLD_DEAD_LIMIT * 8 };
static int Contains(const uint8_t *s, unsigned offset, unsigned count, uint64_t key)
{
    for (unsigned i = 0; i < count; ++i) if (Read64(s + offset + i * 8) == key) return 1;
    return 0;
}
static int Validate(const uint8_t *s, uint32_t size)
{
    return size == SIZE && s[0] <= 1 && s[1] <= 1 && s[2] <= 1 && s[3] <= 1
        && Read16(s + 4) <= AREA_LIMIT && Read16(s + 6) <= DEAD_LIMIT ? 0 : -1;
}
static int Migrate(uint32_t schema, const uint8_t *old, uint32_t oldSize, uint8_t *s, uint32_t size)
{
    /* v1 had smaller ledgers. Identities already used stable keys, so expanding
     * them needs no engine enum, pointer or species-version assumptions. */
    if (schema != 1 || oldSize != OLD_SIZE || size != SIZE || old[0] > 1 || old[1] > 1 || old[2] > 1
        || Read16(old + 4) > OLD_AREA_LIMIT || Read16(old + 6) > OLD_DEAD_LIMIT) return -1;
    memset(s, 0, size); memcpy(s, old, HEADER); s[3] = 0;
    memcpy(s + AREAS, old + HEADER, Read16(old + 4) * 8);
    memcpy(s + DEAD, old + OLD_DEAD, Read16(old + 6) * 8);
    return Validate(s, size);
}
static int Merge(uint8_t *s, const uint8_t *journal, uint32_t size)
{
    if (Validate(s, size) || Validate(journal, size) || s[0] != journal[0]) return -1;
    s[1] |= journal[1]; s[2] |= journal[2];
    for (unsigned which = 0; which < 2; ++which)
    {
        unsigned countAt = which ? 6 : 4, offset = which ? DEAD : AREAS;
        unsigned limit = which ? DEAD_LIMIT : AREA_LIMIT;
        unsigned count = Read16(s + countAt), other = Read16(journal + countAt);
        for (unsigned i = 0; i < other; ++i)
        {
            uint64_t key = Read64(journal + offset + i * 8);
            if (!Contains(s, offset, count, key))
            { if (count == limit) s[2] = 1; else Write64(s + offset + count++ * 8, key); }
        }
        Write16(s + countAt, count);
    }
    return 0;
}
static void Reduce(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{
    const uint8_t *s = state;
    if (!s[0]) return;
    int dead = Contains(s, DEAD, Read16(s + 6), event->subject);
    if (event->kind == RHH_MON_IS_DEAD) { if (dead) out->value = 1; return; }
    if (event->kind == RHH_CAN_REVIVE && dead)
    { out->allowed = 0; strcpy(out->reason, "A fainted Nuzlocke Pokemon cannot be revived"); }
    else if (event->kind == RHH_CAN_CAPTURE && (s[2] || (s[1] && !s[3])))
    { out->allowed = 0; strcpy(out->reason, s[2] ? "Nuzlocke run ended" : "This encounter cannot be caught under Nuzlocke rules"); }
    else if (event->kind == RHH_CAN_CONTINUE && s[2])
    { out->allowed = 0; strcpy(out->reason, "Nuzlocke run ended"); }
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *out, void *state)
{
    (void)out; uint8_t *s = state;
    if (!s[0]) return;
    if (event->kind == RHH_INVENTORY_CHANGED && event->args[0]) s[1] = 1;
    if (event->kind == RHH_RUN_LOADED) { s[3] = 0; return; }
    if (event->kind == RHH_PERSISTENCE_FAILED) { s[2] = 1; return; }
    if (event->kind == RHH_ENCOUNTER_STARTED)
    {
        if (event->args[1]) s[1] = 1;
        s[3] = 0;
        if (!s[1] || s[2] || !event->args[0] || !event->event_key) return;
        unsigned count = Read16(s + 4);
        if (Contains(s, AREAS, count, event->event_key)) return;
        if (count == AREA_LIMIT) s[2] = 1;
        else { Write64(s + AREAS + count * 8, event->event_key); Write16(s + 4, count + 1); s[3] = 1; }
    }
    else if (event->kind == RHH_MON_FAINTED)
    {
        if (event->args[0]) s[1] = 1;
        if (!s[1]) return;
        unsigned count = Read16(s + 6);
        if (Contains(s, DEAD, count, event->subject)) return;
        if (count == DEAD_LIMIT) s[2] = 1;
        else { Write64(s + DEAD + count * 8, event->subject); Write16(s + 6, count + 1); }
    }
    else if (event->kind == RHH_WHITEOUT)
    { if (event->args[0]) s[1] = 1; if (s[1]) s[2] = 1; }
    else if (event->kind == RHH_MON_CAUGHT) s[3] = 0;
}
static const struct RhhModSetting sSettings[] = {BOOL_SETTING("Nuzlocke", "First encounter per named area, permanent fainting, and run ends on a wipeout.")};
static const struct RhhModHook sHooks[] = {
    REDUCER(RHH_CAN_CAPTURE, RHH_CONSTRAINT, 0, Reduce), REDUCER(RHH_CAN_REVIVE, RHH_CONSTRAINT, 0, Reduce),
    REDUCER(RHH_MON_IS_DEAD, RHH_CONSTRAINT, 0, Reduce), REDUCER(RHH_CAN_CONTINUE, RHH_CONSTRAINT, 0, Reduce),
    OBSERVER(RHH_ENCOUNTER_STARTED, Observe), OBSERVER(RHH_MON_FAINTED, Observe), OBSERVER(RHH_RUN_LOADED, Observe),
    OBSERVER(RHH_INVENTORY_CHANGED, Observe), OBSERVER(RHH_WHITEOUT, Observe),
    OBSERVER(RHH_PERSISTENCE_FAILED, Observe), OBSERVER(RHH_MON_CAUGHT, Observe)
};
static const struct RhhModDefinition sMod = {
    .struct_size = sizeof(sMod), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1, .state_schema = 2,
    .id = "rhh:nuzlocke", .flags = RHH_MOD_REQUIRED | RHH_MOD_DURABLE, .state_size = SIZE,
    .hook_count = sizeof(sHooks) / sizeof(*sHooks), .hooks = sHooks, .validate = Validate, .migrate = Migrate, .merge = Merge,
    .setting_count = 1, .settings = sSettings, .read_setting = BoolRead, .write_setting = BoolWrite
};
const struct RhhModDefinition *RhhBundledNuzlocke(const struct RhhModHostV1 *host) { return HostOK(host) ? &sMod : NULL; }
SHARED_ENTRY(RhhBundledNuzlocke)
