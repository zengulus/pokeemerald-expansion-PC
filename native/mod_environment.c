/* Host-owned, save-backed map weather and terrain transactions. */
#include "global.h"
#include "battle_shader.h"
#include "main.h"
#include "pokemon.h"
#include "random.h"
#include "move.h"
#include "battle.h"
#include "battle_setup.h"
#include "field_weather.h"
#include "constants/weather.h"
#include "data.h"
#include "mods.h"
#include "mod_host.h"
#include "native.h"
#include "constants/moves.h"

enum { ENV_MAPS = RHH_WORLD_ENVIRONMENT_MAP_CAPACITY, ENV_EFFECT_BYTES = 48, ENV_RECORD_BYTES = 104,
       ENV_LEGACY_BYTES = ENV_MAPS * ENV_RECORD_BYTES, ENV_TIME_BYTES = 24,
       ENV_TIME_RECORD_BYTES = ENV_TIME_BYTES * 2,
       ENV_STATE_BYTES = ENV_LEGACY_BYTES + ENV_MAPS * ENV_TIME_RECORD_BYTES, ENV_MAX_DURATION = 1000000 };
static const char sEnvironmentId[] = "rhh:world_environment";
static int sBusy;
static int sResolvingMove;
static const struct RhhWorldEnvironmentResult *sContext;
static const struct RhhWorldEnvironmentTiming *sTimingContext[2];
static const struct RhhBattleEnvironmentSeed *sBattleContext;
static struct { RhhContentKey map; u64 run, started; u32 weather, terrain; } sBattleReturn;

static u32 Read32(const u8 *p)
{ return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u64 Read64(const u8 *p)
{ u64 n = 0; for (u32 i = 0; i < 8; ++i) n |= (u64)p[i] << (i * 8); return n; }
static void Write32(u8 *p, u32 n)
{ for (u32 i = 0; i < 4; ++i) p[i] = n >> (i * 8); }
static void Write64(u8 *p, u64 n)
{ for (u32 i = 0; i < 8; ++i) p[i] = n >> (i * 8); }
static u8 *State(void) { return RhhModsState(sEnvironmentId); }
static u8 *Record(u8 *state, u32 index) { return state + index * ENV_RECORD_BYTES; }
static u8 *EffectBytes(u8 *record, u32 kind)
{ return record + 8 + (kind == RHH_WORLD_TERRAIN ? ENV_EFFECT_BYTES : 0); }
static u8 *TimeRecord(u8 *state, u32 index)
{ return state + ENV_LEGACY_BYTES + index * ENV_TIME_RECORD_BYTES; }
static u8 *TimeBytes(u8 *state, u32 index, u32 kind)
{ return TimeRecord(state, index) + (kind == RHH_WORLD_TERRAIN ? ENV_TIME_BYTES : 0); }
static struct RhhWorldEnvironmentEffect ReadEffect(const u8 *p)
{
    u32 bits = Read32(p + 4);
    s32 priority = bits <= INT32_MAX ? (s32)bits : (s32)((s64)bits - (INT64_C(1) << 32));
    return (struct RhhWorldEnvironmentEffect){.value = Read32(p), .priority = priority,
        .owner = Read64(p + 8), .move = Read64(p + 16), .actor_identity = Read64(p + 24),
        .started_step = Read64(p + 32), .expires_step = Read64(p + 40)};
}
static void WriteEffect(u8 *p, const struct RhhWorldEnvironmentEffect *effect)
{
    memset(p, 0, ENV_EFFECT_BYTES);
    if (!effect->value) return;
    Write32(p, effect->value); Write32(p + 4, (u32)effect->priority);
    Write64(p + 8, effect->owner); Write64(p + 16, effect->move);
    Write64(p + 24, effect->actor_identity); Write64(p + 32, effect->started_step);
    Write64(p + 40, effect->expires_step);
}
static u32 BuiltinMoveValue(u32 move, u32 *kind)
{
    switch (move)
    {
    case MOVE_RAIN_DANCE: *kind = RHH_WORLD_WEATHER; return RHH_WEATHER_RAIN;
    case MOVE_SUNNY_DAY: *kind = RHH_WORLD_WEATHER; return RHH_WEATHER_SUN;
    case MOVE_SANDSTORM: *kind = RHH_WORLD_WEATHER; return RHH_WEATHER_SAND;
    case MOVE_HAIL: *kind = RHH_WORLD_WEATHER; return RHH_WEATHER_HAIL;
    case MOVE_SNOWSCAPE: *kind = RHH_WORLD_WEATHER; return RHH_WEATHER_SNOW;
    case MOVE_GRASSY_TERRAIN: *kind = RHH_WORLD_TERRAIN; return RHH_TERRAIN_GRASSY;
    case MOVE_MISTY_TERRAIN: *kind = RHH_WORLD_TERRAIN; return RHH_TERRAIN_MISTY;
    case MOVE_ELECTRIC_TERRAIN: *kind = RHH_WORLD_TERRAIN; return RHH_TERRAIN_ELECTRIC;
    case MOVE_PSYCHIC_TERRAIN: *kind = RHH_WORLD_TERRAIN; return RHH_TERRAIN_PSYCHIC;
    default: return 0;
    }
}
static int ValidMoveEffect(u64 effect)
{
    u32 kind = effect >> 32, value = (u32)effect;
    return !effect || (kind == RHH_WORLD_WEATHER && value >= RHH_WEATHER_RAIN && value <= RHH_WEATHER_HEAVY_RAIN)
        || (kind == RHH_WORLD_TERRAIN && value >= RHH_TERRAIN_GRASSY && value <= RHH_TERRAIN_PSYCHIC);
}
static RhhContentKey SlotMoveKey(u32 move)
{
    if (!move) return 0;
    if (move < MOVES_COUNT_ALL)
        return GetMoveEffect(move) == EFFECT_PLACEHOLDER ? 0 : RhhContentKeyForId(RHH_MOVE, move);
    RhhContentKey key = RhhModsMoveKey(move);
    return ModsCustomMoveAvailable(RhhModsFindCustomMove(key)) ? key : 0;
}
static RhhContentKey ResolveMove(RhhContentKey key, u32 *builtin)
{
    u32 move;
    if (!key) return 0;
    if (!RhhContentResolve(RHH_MOVE, key, &move))
    {
        RhhContentKey canonical = move < MOVES_COUNT_ALL ? SlotMoveKey(move) : 0;
        if (canonical) *builtin = move;
        return canonical;
    }
    if (!ModsCustomMoveAvailable(RhhModsFindCustomMove(key))) return 0;
    *builtin = MOVE_NONE;
    return key;
}
static u64 MoveEffect(RhhContentKey key, u32 builtin)
{
    u32 kind = 0, value = BuiltinMoveValue(builtin, &kind);
    u64 original = RHH_WORLD_ENV_MOVE(kind, value);
    if (!key || !RhhModsHasActiveRun() || sResolvingMove || !RhhModsHasHook(RHH_WORLD_ENVIRONMENT_MOVE)) return original;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WORLD_ENVIRONMENT_MOVE,
        .run_seed = RhhModsRunSeed(), .event_key = key, .args = {kind, value}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = original};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sResolvingMove = 1; RhhModsDispatch(&event, &decision); sResolvingMove = 0;
    bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    return !impure && decision.allowed && ValidMoveEffect(decision.value) ? decision.value : original;
}
static RhhContentKey CanonicalMap(RhhContentKey key)
{
    u32 id;
    return !RhhContentResolve(RHH_MAP, key, &id) ? RhhContentKeyForId(RHH_MAP, id) : 0;
}
static RhhContentKey CurrentMap(void)
{
    return gSaveBlock1Ptr ? RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8)
                                                        | (u8)gSaveBlock1Ptr->location.mapNum) : 0;
}
static int Clock(u64 *step)
{
    const struct RhhWorldClockService *clock = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldClockSnapshot snapshot = {.struct_size = sizeof(snapshot)};
    if (!clock || clock->current(&snapshot)) return -1;
    *step = snapshot.steps; return 0;
}
struct EnvTime { u64 now, system; u32 source; bool32 available, systemAvailable; };
static struct EnvTime Time(void)
{
    struct EnvTime result = {0};
    result.system = RhhEcologySystemTime(); result.systemAvailable = result.system != 0;
    const struct RhhWorldTimeService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_time", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldTimeSnapshot sample = {.struct_size = sizeof(sample)};
    if (!service || service->sample(&sample) || (sample.flags & RHH_TIME_RTC_FALLBACK)
        || (sample.source != RHH_TIME_REAL && sample.source != RHH_TIME_FAKE)) return result;
    result.source = sample.source;
    result.now = ((u64)((s64)sample.local_day - INT32_MIN) * 24 + sample.hour) * 3600
        + sample.minute * 60 + sample.second;
    result.available = TRUE; return result;
}
static struct RhhWorldEnvironmentTiming ReadTiming(const u8 *p, const struct RhhWorldEnvironmentEffect *effect,
                                                     u64 step, const struct EnvTime *time)
{
    struct RhhWorldEnvironmentTiming result = {.struct_size = sizeof(result)};
    if (!effect->value) return result;
    if (Read32(p + 20) == 1)
    {
        result.clock = RHH_WORLD_ENV_CLOCK_PERMANENT; result.time_available = TRUE;
        result.started = effect->started_step; result.now = step;
    }
    else if (Read64(p))
    {
        result.clock = RHH_WORLD_ENV_CLOCK_TIME; result.source = Read32(p + 16);
        result.started = Read64(p); result.expires = Read64(p + 8);
        result.time_available = result.source == RHH_TIME_SYSTEM ? time->systemAvailable
            : time->available && time->source == result.source;
        if (result.time_available) result.now = result.source == RHH_TIME_SYSTEM ? time->system : time->now;
    }
    else
    {
        result.clock = RHH_WORLD_ENV_CLOCK_STEPS; result.time_available = TRUE;
        result.started = effect->started_step; result.expires = effect->expires_step; result.now = step;
    }
    return result;
}
static bool32 IsExpired(const struct RhhWorldEnvironmentTiming *timing)
{ return timing->clock && timing->clock != RHH_WORLD_ENV_CLOCK_PERMANENT
      && timing->time_available && timing->now >= timing->expires; }
static void WriteTiming(u8 *p, const struct RhhWorldEnvironmentTiming *timing)
{
    memset(p, 0, ENV_TIME_BYTES);
    if (timing && timing->clock == RHH_WORLD_ENV_CLOCK_TIME)
    { Write64(p, timing->started); Write64(p + 8, timing->expires); Write32(p + 16, timing->source); }
    else if (timing && timing->clock == RHH_WORLD_ENV_CLOCK_PERMANENT) Write32(p + 20, 1);
}
static u32 FindRecord(u8 *state, RhhContentKey map, int *found)
{
    u32 i;
    for (i = 0; i < ENV_MAPS; ++i)
    {
        RhhContentKey current = Read64(Record(state, i));
        if (!current || current >= map) break;
    }
    *found = i < ENV_MAPS && Read64(Record(state, i)) == map;
    return i;
}
static void Snapshot(u8 *state, RhhContentKey map, u64 step, int raw,
                     struct RhhWorldEnvironmentSnapshot *out)
{
    *out = (struct RhhWorldEnvironmentSnapshot){.struct_size = sizeof(*out), .map = map, .step = step};
    int found; u32 i = FindRecord(state, map, &found);
    if (!found) return;
    out->weather = ReadEffect(EffectBytes(Record(state, i), RHH_WORLD_WEATHER));
    out->terrain = ReadEffect(EffectBytes(Record(state, i), RHH_WORLD_TERRAIN));
    if (!raw)
    {
        struct EnvTime time = {0};
        if (Read64(TimeBytes(state, i, RHH_WORLD_WEATHER)) || Read64(TimeBytes(state, i, RHH_WORLD_TERRAIN))) time = Time();
        struct RhhWorldEnvironmentTiming weather = ReadTiming(TimeBytes(state, i, RHH_WORLD_WEATHER), &out->weather, step, &time);
        struct RhhWorldEnvironmentTiming terrain = ReadTiming(TimeBytes(state, i, RHH_WORLD_TERRAIN), &out->terrain, step, &time);
        if (IsExpired(&weather)) memset(&out->weather, 0, sizeof(out->weather));
        if (IsExpired(&terrain)) memset(&out->terrain, 0, sizeof(out->terrain));
    }
}
static int Validate(const u8 *state, u32 size)
{
    if (size != ENV_STATE_BYTES) return -1;
    RhhContentKey previous = 0; int empty = 0;
    for (u32 i = 0; i < ENV_MAPS; ++i)
    {
        const u8 *record = state + i * ENV_RECORD_BYTES;
        RhhContentKey map = Read64(record);
        if (!map)
        {
            empty = 1;
            for (u32 j = 0; j < ENV_RECORD_BYTES; ++j) if (record[j]) return -1;
            for (u32 j = 0; j < ENV_TIME_RECORD_BYTES; ++j)
                if (state[ENV_LEGACY_BYTES + i * ENV_TIME_RECORD_BYTES + j]) return -1;
            continue;
        }
        if (empty || map <= previous || CanonicalMap(map) != map) return -1;
        previous = map;
        unsigned count = 0;
        for (u32 kind = RHH_WORLD_WEATHER; kind <= RHH_WORLD_TERRAIN; ++kind)
        {
            const u8 *p = record + 8 + (kind == RHH_WORLD_TERRAIN ? ENV_EFFECT_BYTES : 0);
            const u8 *t = state + ENV_LEGACY_BYTES + i * ENV_TIME_RECORD_BYTES
                + (kind == RHH_WORLD_TERRAIN ? ENV_TIME_BYTES : 0);
            struct RhhWorldEnvironmentEffect effect = ReadEffect(p);
            if (!effect.value)
            {
                for (u32 j = 0; j < ENV_EFFECT_BYTES; ++j) if (p[j]) return -1;
                for (u32 j = 0; j < ENV_TIME_BYTES; ++j) if (t[j]) return -1;
                continue;
            }
            u32 move;
            u64 started = Read64(t), expires = Read64(t + 8); u32 source = Read32(t + 16);
            u32 permanent = Read32(t + 20);
            if (permanent > 1 || (permanent && (started || expires || source || effect.move || effect.actor_identity
                                                 || effect.expires_step != UINT64_MAX))
                || (!permanent && started && (expires <= started || expires - started > ENV_MAX_DURATION
                    || (source != RHH_TIME_REAL && source != RHH_TIME_FAKE && source != RHH_TIME_SYSTEM) || effect.expires_step != UINT64_MAX))
                || (!permanent && !started && (expires || source || effect.expires_step <= effect.started_step
                                  || effect.expires_step - effect.started_step > ENV_MAX_DURATION))
                || !effect.owner || (!effect.move && effect.actor_identity)
                || !ValidMoveEffect(RHH_WORLD_ENV_MOVE(kind, effect.value))
                || (effect.move && !RhhContentResolve(RHH_MOVE, effect.move, &move)
                    && RhhContentKeyForId(RHH_MOVE, move) != effect.move)) return -1;
            ++count;
        }
        if (!count) return -1;
    }
    return 0;
}
static int Migrate(u32 schema, const u8 *old, u32 oldSize, u8 *dst, u32 size)
{
    if (size != ENV_STATE_BYTES || !((schema == 1 && oldSize == ENV_LEGACY_BYTES)
                                  || ((schema == 2 || schema == 3 || schema == 4) && oldSize == ENV_STATE_BYTES))) return -1;
    /* Older schemas never represented a permanent source, and schemas 1/2
     * never represented a move-less source. */
    for (u32 i = 0; i < ENV_MAPS && Read64(old + i * ENV_RECORD_BYTES); ++i)
        for (u32 kind = RHH_WORLD_WEATHER; kind <= RHH_WORLD_TERRAIN; ++kind)
        {
            const u8 *p = old + i * ENV_RECORD_BYTES + 8 + (kind == RHH_WORLD_TERRAIN ? ENV_EFFECT_BYTES : 0);
            if (schema < 3 && Read32(p) && !Read64(p + 16)) return -1;
            if (kind == RHH_WORLD_WEATHER && Read32(p) > RHH_WEATHER_FOG) return -1;
            if (schema >= 2 && schema < 4 && Read32(old + ENV_LEGACY_BYTES + i * ENV_TIME_RECORD_BYTES
                    + (kind == RHH_WORLD_TERRAIN ? ENV_TIME_BYTES : 0) + 20)) return -1;
            if (schema >= 2 && Read32(old + ENV_LEGACY_BYTES + i * ENV_TIME_RECORD_BYTES
                    + (kind == RHH_WORLD_TERRAIN ? ENV_TIME_BYTES : 0) + 16) == RHH_TIME_SYSTEM) return -1;
        }
    memcpy(dst, old, oldSize); memset(dst + oldSize, 0, size - oldSize); return 0;
}
int RhhModsEnvironmentProjectSave(const char *id, const u8 *state, u32 stateSize, u32 *schema, u32 *size)
{
    if (strcmp(id, sEnvironmentId)) return 0;
    if (!state || stateSize != ENV_STATE_BYTES || !schema || !size) return -1;
    for (u32 i = 0; i < ENV_MAPS && Read64(state + i * ENV_RECORD_BYTES); ++i)
        if (Read32(state + i * ENV_RECORD_BYTES + 8) > RHH_WEATHER_FOG) return 0;
    for (u32 i = 0; i < ENV_MAPS; ++i)
        for (u32 kind = RHH_WORLD_WEATHER; kind <= RHH_WORLD_TERRAIN; ++kind)
            if (Read32(state + ENV_LEGACY_BYTES + i * ENV_TIME_RECORD_BYTES
                + (kind == RHH_WORLD_TERRAIN ? ENV_TIME_BYTES : 0) + 16) == RHH_TIME_SYSTEM) return 0;
    for (u32 i = 0; i < ENV_MAPS; ++i)
        for (u32 kind = RHH_WORLD_WEATHER; kind <= RHH_WORLD_TERRAIN; ++kind)
            if (Read32(state + ENV_LEGACY_BYTES + i * ENV_TIME_RECORD_BYTES
                + (kind == RHH_WORLD_TERRAIN ? ENV_TIME_BYTES : 0) + 20))
            { *schema = 4; *size = ENV_STATE_BYTES; return 0; }
    for (u32 i = 0; i < ENV_MAPS && Read64(state + i * ENV_RECORD_BYTES); ++i)
        for (u32 kind = RHH_WORLD_WEATHER; kind <= RHH_WORLD_TERRAIN; ++kind)
        {
            const u8 *p = state + i * ENV_RECORD_BYTES + 8 + (kind == RHH_WORLD_TERRAIN ? ENV_EFFECT_BYTES : 0);
            if (Read32(p) && !Read64(p + 16)) { *schema = 3; *size = ENV_STATE_BYTES; return 0; }
        }
    for (u32 i = ENV_LEGACY_BYTES; i < ENV_STATE_BYTES; ++i)
        if (state[i]) { *schema = 2; *size = ENV_STATE_BYTES; return 0; }
    *schema = 1; *size = ENV_LEGACY_BYTES; return 0;
}
static const struct RhhModDefinition sEnvironment = {
    .struct_size = sizeof(sEnvironment), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 5, .state_schema = 5, .id = sEnvironmentId,
    .flags = RHH_MOD_REQUIRED, .state_size = ENV_STATE_BYTES, .migrate = Migrate, .validate = Validate
};
static int At(RhhContentKey map, struct RhhWorldEnvironmentSnapshot *out)
{
    u64 step; u8 *state = State();
    if (!out || out->struct_size < sizeof(*out) || !state || !(map = CanonicalMap(map)) || Clock(&step)) return -1;
    struct RhhWorldEnvironmentSnapshot result; Snapshot(state, map, step, 0, &result);
    *out = result; return 0;
}
static int Current(struct RhhWorldEnvironmentSnapshot *out) { return At(CurrentMap(), out); }
static void InspectRecord(u8 *state, u32 index, RhhContentKey map, u64 step, const struct EnvTime *time,
                          struct RhhWorldEnvironmentView *out)
{
    *out = (struct RhhWorldEnvironmentView){.struct_size = sizeof(*out),
        .environment.struct_size = sizeof(out->environment), .environment.map = map, .environment.step = step,
        .weather_timing.struct_size = sizeof(out->weather_timing), .terrain_timing.struct_size = sizeof(out->terrain_timing)};
    if (index >= ENV_MAPS) return;
    u8 *record = Record(state, index);
    out->environment.weather = ReadEffect(EffectBytes(record, RHH_WORLD_WEATHER));
    out->environment.terrain = ReadEffect(EffectBytes(record, RHH_WORLD_TERRAIN));
    out->weather_timing = ReadTiming(TimeBytes(state, index, RHH_WORLD_WEATHER), &out->environment.weather, step, time);
    out->terrain_timing = ReadTiming(TimeBytes(state, index, RHH_WORLD_TERRAIN), &out->environment.terrain, step, time);
    if (IsExpired(&out->weather_timing)) {
        memset(&out->environment.weather, 0, sizeof(out->environment.weather));
        out->weather_timing = (struct RhhWorldEnvironmentTiming){.struct_size = sizeof(out->weather_timing)};
    }
    if (IsExpired(&out->terrain_timing)) {
        memset(&out->environment.terrain, 0, sizeof(out->environment.terrain));
        out->terrain_timing = (struct RhhWorldEnvironmentTiming){.struct_size = sizeof(out->terrain_timing)};
    }
}
static int Inspect(RhhContentKey map, struct RhhWorldEnvironmentView *out)
{
    u8 *state = State(); u64 step;
    if (!out || out->struct_size < sizeof(*out) || !state || !(map = CanonicalMap(map)) || Clock(&step)) return -1;
    int found; u32 index = FindRecord(state, map, &found);
    struct EnvTime time = {0};
    if (found && (Read64(TimeBytes(state, index, RHH_WORLD_WEATHER)) || Read64(TimeBytes(state, index, RHH_WORLD_TERRAIN))))
        time = Time();
    struct RhhWorldEnvironmentView result;
    InspectRecord(state, found ? index : ENV_MAPS, map, step, &time, &result);
    *out = result; return 0;
}
static int List(struct RhhWorldEnvironmentView *out, u32 capacity, u32 *count)
{
    u8 *state = State(); u64 step;
    if (!count || (!out && capacity) || !state || Clock(&step)) return -1;
    struct EnvTime time = {0};
    for (u32 i = 0; i < ENV_MAPS && Read64(Record(state, i)); ++i)
        if (Read64(TimeBytes(state, i, RHH_WORLD_WEATHER)) || Read64(TimeBytes(state, i, RHH_WORLD_TERRAIN)))
        { time = Time(); break; }
    struct RhhWorldEnvironmentView views[ENV_MAPS]; u32 n = 0;
    for (u32 i = 0; i < ENV_MAPS && Read64(Record(state, i)); ++i)
    {
        struct RhhWorldEnvironmentView view;
        InspectRecord(state, i, Read64(Record(state, i)), step, &time, &view);
        if (view.environment.weather.value || view.environment.terrain.value) views[n++] = view;
    }
    if (out && capacity < n) return -1;
    if (out && n) memcpy(out, views, n * sizeof(*out));
    *count = n; return 0;
}
struct NativeBattleEnvironment ModsWorldEnvironment(void)
{
    struct NativeBattleEnvironment result = {.visibility = 1.f};
    struct RhhWorldEnvironmentSnapshot world = {.struct_size = sizeof(world)};
    if (Current(&world) || (!world.weather.value && !world.terrain.value)) return result;
    result.weather = world.weather.value; result.terrain = world.terrain.value; result.visibility = 1.f;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WORLD_ENVIRONMENT_PRESENTATION,
        .run_seed = RhhModsRunSeed(), .event_key = world.map, .ordinal = world.step,
        .args = {result.weather, result.terrain}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision);
    if (decision.allowed) result.shader = decision.value;
    return result;
}
static int Context(struct RhhWorldEnvironmentResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sContext) return -1;
    *out = *sContext; return 0;
}
static int Timing(RhhContentKey map, u32 kind, struct RhhWorldEnvironmentTiming *out)
{
    u8 *state = State(); u64 step;
    if (!out || out->struct_size < sizeof(*out) || !state || !(map = CanonicalMap(map))
        || (kind != RHH_WORLD_WEATHER && kind != RHH_WORLD_TERRAIN) || Clock(&step)) return -1;
    int found; u32 index = FindRecord(state, map, &found);
    if (!found) return -1;
    struct RhhWorldEnvironmentEffect effect = ReadEffect(EffectBytes(Record(state, index), kind));
    struct EnvTime time = {0};
    if (Read64(TimeBytes(state, index, kind))) time = Time();
    struct RhhWorldEnvironmentTiming result = ReadTiming(TimeBytes(state, index, kind), &effect, step, &time);
    if (!result.clock || IsExpired(&result)) return -1;
    *out = result; return 0;
}
static int ContextTiming(u32 kind, struct RhhWorldEnvironmentTiming *out)
{
    if (!out || out->struct_size < sizeof(*out) || kind < RHH_WORLD_WEATHER || kind > RHH_WORLD_TERRAIN
        || !sTimingContext[kind - RHH_WORLD_WEATHER]) return -1;
    *out = *sTimingContext[kind - RHH_WORLD_WEATHER]; return 0;
}
static int BattleContext(struct RhhBattleEnvironmentSeed *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBattleContext) return -1;
    *out = *sBattleContext; return 0;
}
static int Result(struct RhhWorldEnvironmentResult *out, int status, const char *reason)
{
    out->status = status;
    if (reason) snprintf(out->reason, sizeof(out->reason), "%s", reason);
    return status;
}
static int FieldMoveActor(const struct RhhWorldEnvironmentRequest *request, RhhContentKey move)
{
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    struct RhhOwnedMonSnapshot view = {.struct_size = sizeof(view)};
    if (!mons || request->party_slot >= PARTY_SIZE || mons->party_at(request->party_slot, &view)
        || view.identity != request->actor_identity || view.is_egg) return -1;
    struct Pokemon copy = gParties[B_TRAINER_PLAYER][request->party_slot];
    if (!copy.hp || ModsMonIsDead(&copy)) return -1;
    for (u32 i = 0; i < 4; ++i) if (SlotMoveKey(GetMonData(&copy, MON_DATA_MOVE1 + i)) == move) return 0;
    return -1;
}
static void Notify(struct RhhWorldEnvironmentResult *result, const struct RhhWorldEnvironmentTiming *weather,
                   const struct RhhWorldEnvironmentTiming *terrain)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WORLD_ENVIRONMENT_CHANGED,
        .run_seed = RhhModsRunSeed(), .event_key = result->after.map, .ordinal = result->after.step,
        .args = {result->kind, result->cause}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sContext = result; sTimingContext[0] = weather; sTimingContext[1] = terrain;
    RhhModsDispatch(&event, &decision); sContext = NULL; sTimingContext[0] = sTimingContext[1] = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static void Expire(u64 step);
static bool32 SameEffect(const struct RhhWorldEnvironmentEffect *a, const struct RhhWorldEnvironmentEffect *b)
{
    return a->value == b->value && a->priority == b->priority && a->owner == b->owner && a->move == b->move
        && a->actor_identity == b->actor_identity && a->started_step == b->started_step && a->expires_step == b->expires_step;
}
static bool32 SameSnapshot(const struct RhhWorldEnvironmentSnapshot *a, const struct RhhWorldEnvironmentSnapshot *b)
{
    return a->struct_size >= sizeof(*a) && b->struct_size >= sizeof(*b) && a->map == b->map && a->step == b->step
        && SameEffect(&a->weather, &b->weather) && SameEffect(&a->terrain, &b->terrain);
}
static bool32 SameRequest(const struct RhhWorldEnvironmentRequest *a, const struct RhhWorldEnvironmentRequest *b)
{
    return a->struct_size >= sizeof(*a) && a->kind == b->kind && a->party_slot == b->party_slot
        && a->reserved == b->reserved && a->priority == b->priority && a->reserved2 == b->reserved2
        && a->map == b->map && a->owner == b->owner && a->move == b->move
        && a->actor_identity == b->actor_identity && a->duration_steps == b->duration_steps;
}
static bool32 SameTiming(const struct RhhWorldEnvironmentTiming *a, const struct RhhWorldEnvironmentTiming *b, bool32 relative)
{
    if (a->struct_size < sizeof(*a) || b->struct_size < sizeof(*b) || a->clock != b->clock
        || a->source != b->source || a->time_available != b->time_available) return FALSE;
    if (a->clock == RHH_WORLD_ENV_CLOCK_TIME && a->time_available && b->now < a->now) return FALSE;
    if (relative && a->clock == RHH_WORLD_ENV_CLOCK_TIME)
        return a->expires >= a->started && b->expires >= b->started
            && a->expires - a->started == b->expires - b->started;
    return a->started == b->started && a->expires == b->expires;
}
static int Change(const struct RhhWorldEnvironmentRequest *request, struct RhhWorldEnvironmentResult *out,
                  int preview, int timed, u64 seconds, const struct RhhWorldEnvironmentQuote *expected,
                  struct RhhWorldEnvironmentQuote *quote, u32 sourceValue, int anyMap)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WORLD_ENV_INVALID;
    struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result), .is_preview = preview};
    int status = RHH_WORLD_ENV_INVALID; u8 *state = State(); u64 step; RhhContentKey map;
    bool32 realTimed = timed == 1 || timed == 3, permanent = timed == 2, systemTimed = timed == 3;
    struct RhhWorldTimeScope timeScope; bool32 scoped = FALSE;
    struct RhhWorldEnvironmentTiming timing = {.struct_size = sizeof(timing)},
        beforeTiming = {.struct_size = sizeof(beforeTiming)}, otherTiming = {.struct_size = sizeof(otherTiming)};
    if (!request || request->struct_size < sizeof(*request) || request->reserved || request->reserved2
        || (request->kind != RHH_WORLD_WEATHER && request->kind != RHH_WORLD_TERRAIN))
    { Result(&result, status, "invalid environment request"); goto done; }
    struct RhhWorldEnvironmentRequest originalRequest = *request;
    u64 originalRun = RhhModsRunId(); const void *originalSave = gSaveBlock1Ptr;
    if (!state || !gSaveBlock1Ptr || gMain.inBattle || sBusy || NativeSaveTransactionActive() || !RhhModsCommandsAllowed() || Clock(&step))
    { status = RHH_WORLD_ENV_UNAVAILABLE; Result(&result, status, "environment changes are unavailable"); goto done; }
    RhhModsWorldTimeScopeBegin(&timeScope); scoped = TRUE;
    if (!(map = CanonicalMap(request->map)) || (!anyMap && map != CurrentMap()) || !RhhModsRegisteredKey(request->owner))
    { Result(&result, status, "invalid map or module identity"); goto done; }
    result.kind = request->kind; Snapshot(state, map, step, 0, &result.before); result.after = result.before;
    struct RhhWorldEnvironmentEffect *before = request->kind == RHH_WORLD_WEATHER ? &result.before.weather : &result.before.terrain;
    struct RhhWorldEnvironmentEffect *after = request->kind == RHH_WORLD_WEATHER ? &result.after.weather : &result.after.terrain;
    int found; u32 index = FindRecord(state, map, &found);
    u32 otherKind = request->kind == RHH_WORLD_WEATHER ? RHH_WORLD_TERRAIN : RHH_WORLD_WEATHER;
    const struct RhhWorldEnvironmentEffect *other = request->kind == RHH_WORLD_WEATHER
        ? &result.before.terrain : &result.before.weather;
    struct EnvTime time = {0};
    if (realTimed || (found && (Read64(TimeBytes(state, index, request->kind))
                            || Read64(TimeBytes(state, index, otherKind))))) time = Time();
    u64 timerNow = systemTimed ? time.system : time.now;
    u32 timerSource = systemTimed ? RHH_TIME_SYSTEM : time.source;
    bool32 timerAvailable = systemTimed ? time.systemAvailable : time.available;
    if (found) timing = ReadTiming(TimeBytes(state, index, request->kind), before, step, &time);
    if (found) otherTiming = ReadTiming(TimeBytes(state, index, otherKind), other, step, &time);
    beforeTiming = timing;
    if (!request->move && !sourceValue)
    {
        if (timed || request->actor_identity || request->party_slot || request->duration_steps || !before->value)
        { Result(&result, status, "invalid clear request"); goto done; }
        if (before->owner != request->owner)
        { status = RHH_WORLD_ENV_CONFLICT; Result(&result, status, "only the owner can clear this effect"); goto done; }
        memset(after, 0, sizeof(*after)); result.cause = RHH_WORLD_ENV_CLEARED;
    }
    else
    {
        u32 move = 0; RhhContentKey key = 0; u64 effect = 0;
        if (realTimed && !timerAvailable)
        { status = RHH_WORLD_ENV_UNAVAILABLE; Result(&result, status, "world time unavailable"); goto done; }
        if ((realTimed && (request->duration_steps || !seconds || seconds > ENV_MAX_DURATION
                       || seconds > UINT64_MAX - timerNow))
            || (permanent && (!sourceValue || request->duration_steps || seconds))
            || (!timed && (!request->duration_steps || request->duration_steps > ENV_MAX_DURATION
                           || request->duration_steps > UINT64_MAX - step))
            || (sourceValue && (request->move || request->actor_identity || request->party_slot
                               || !ValidMoveEffect(RHH_WORLD_ENV_MOVE(request->kind, sourceValue))))
            || (!sourceValue && (!(key = ResolveMove(request->move, &move))
                || !(effect = MoveEffect(key, move)) || (u32)(effect >> 32) != request->kind)))
        { Result(&result, status, "invalid move or duration"); goto done; }
        if (!sourceValue && FieldMoveActor(request, key))
        { status = RHH_WORLD_ENV_STALE; Result(&result, status, "party Pokémon cannot use this move"); goto done; }
        if (before->value && before->owner != request->owner
            && (request->priority < before->priority || (request->priority == before->priority && request->owner > before->owner)))
        { status = RHH_WORLD_ENV_CONFLICT; Result(&result, status, "a higher-priority effect is active"); goto done; }
        if (!found && (index >= ENV_MAPS || Read64(Record(state, ENV_MAPS - 1))))
        { status = RHH_WORLD_ENV_FULL; Result(&result, status, "environment map capacity reached"); goto done; }
        *after = (struct RhhWorldEnvironmentEffect){.value = sourceValue ? sourceValue : (u32)effect, .priority = request->priority,
            .owner = request->owner, .move = key,
            .actor_identity = request->actor_identity, .started_step = step,
            .expires_step = (realTimed || permanent) ? UINT64_MAX : step + request->duration_steps};
        timing = (struct RhhWorldEnvironmentTiming){.struct_size = sizeof(timing),
            .clock = permanent ? RHH_WORLD_ENV_CLOCK_PERMANENT : realTimed ? RHH_WORLD_ENV_CLOCK_TIME : RHH_WORLD_ENV_CLOCK_STEPS,
            .source = realTimed ? timerSource : 0, .time_available = TRUE,
            .started = realTimed ? timerNow : step, .expires = permanent ? 0 : realTimed ? timerNow + seconds : step + request->duration_steps,
            .now = realTimed ? timerNow : step};
        result.cause = before->value ? RHH_WORLD_ENV_REPLACED : RHH_WORLD_ENV_SET;
    }
    struct RhhWorldEnvironmentTiming afterTiming = (request->move || sourceValue) ? timing
        : (struct RhhWorldEnvironmentTiming){.struct_size = sizeof(afterTiming)};
    if (expected && (expected->struct_size < sizeof(*expected) || expected->timed != (u32)timed
        || expected->run_id != RhhModsRunId() || expected->duration_seconds != seconds
        || !SameRequest(&expected->request, request) || expected->result.struct_size < sizeof(expected->result)
        || expected->result.status != RHH_WORLD_ENV_OK || expected->result.is_preview != 1
        || expected->result.kind != result.kind || expected->result.cause != result.cause
        || !SameSnapshot(&expected->result.before, &result.before) || !SameSnapshot(&expected->result.after, &result.after)
        || !SameTiming(&expected->before_timing, &beforeTiming, FALSE)
        || !SameTiming(&expected->other_before_timing, &otherTiming, FALSE)
        || !SameTiming(&expected->after_timing, &afterTiming, timed)))
    { status = RHH_WORLD_ENV_STALE; Result(&result, status, "environment preview changed"); goto done; }
    sBusy = 1;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_WORLD_ENVIRONMENT,
        .run_seed = RhhModsRunSeed(), .event_key = map, .subject = request->owner,
        .ordinal = step, .args = {request->kind, preview}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sContext = &result; sTimingContext[request->kind - RHH_WORLD_WEATHER] = &timing;
    RhhModsDispatch(&event, &decision); sContext = NULL; sTimingContext[0] = sTimingContext[1] = NULL;
    bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed)
    { status = RHH_WORLD_ENV_DENIED; Result(&result, status, decision.reason[0] ? decision.reason : "environment change denied"); goto unlock; }
    if (impure)
    { status = RHH_WORLD_ENV_UNAVAILABLE; Result(&result, status, "environment constraint consumed RNG"); goto unlock; }
    /* Constraints can query other services. Revalidate the reviewed target,
     * actor, clock and both effects before publishing a preview or commit. */
    RhhModsWorldTimeScopeEnd(&timeScope); RhhModsWorldTimeScopeBegin(&timeScope);
    u64 currentStep = 0; struct RhhWorldEnvironmentSnapshot current = {.struct_size = sizeof(current)};
    struct RhhWorldEnvironmentTiming currentBefore = {.struct_size = sizeof(currentBefore)},
        currentOther = {.struct_size = sizeof(currentOther)};
    bool32 stale = !RhhModsHasActiveRun() || RhhModsRunId() != originalRun || gSaveBlock1Ptr != originalSave
        || State() != state || gMain.inBattle || NativeSaveTransactionActive() || !RhhModsCommandsAllowed()
        || !SameRequest(&originalRequest, request) || (!anyMap && CurrentMap() != map)
        || Clock(&currentStep) || currentStep != step;
    if (!stale) {
        Snapshot(state, map, currentStep, 0, &current);
        stale = !SameSnapshot(&result.before, &current);
    }
    if (!stale) {
        int currentFound; u32 currentIndex = FindRecord(state, map, &currentFound);
        struct EnvTime currentTime = {0};
        if (realTimed || (currentFound && (Read64(TimeBytes(state, currentIndex, request->kind))
                                      || Read64(TimeBytes(state, currentIndex, otherKind))))) currentTime = Time();
        if (currentFound) {
            const struct RhhWorldEnvironmentEffect *selected = request->kind == RHH_WORLD_WEATHER ? &current.weather : &current.terrain;
            const struct RhhWorldEnvironmentEffect *otherEffect = request->kind == RHH_WORLD_WEATHER ? &current.terrain : &current.weather;
            currentBefore = ReadTiming(TimeBytes(state, currentIndex, request->kind), selected, currentStep, &currentTime);
            currentOther = ReadTiming(TimeBytes(state, currentIndex, otherKind), otherEffect, currentStep, &currentTime);
        }
        stale = !SameTiming(&beforeTiming, &currentBefore, FALSE) || !SameTiming(&otherTiming, &currentOther, FALSE)
            || (realTimed && (systemTimed
                ? (!currentTime.systemAvailable || currentTime.system < timerNow)
                : (!currentTime.available || currentTime.source != timerSource || currentTime.now != timerNow)))
            || (request->move && FieldMoveActor(request, after->move));
    }
    if (stale)
    { status = RHH_WORLD_ENV_STALE; Result(&result, status, "environment changed during validation"); goto unlock; }
    status = RHH_WORLD_ENV_OK; result.status = status;
    if (!preview)
    {
        /* Deliver expired effects before replacing their slots, using the
         * same clock sample that was validated above. */
        sBusy = 0; Expire(step); sBusy = 1;
        index = FindRecord(state, map, &found);
        if (!found)
        {
            memmove(Record(state, index + 1), Record(state, index), (ENV_MAPS - index - 1) * ENV_RECORD_BYTES);
            memmove(TimeRecord(state, index + 1), TimeRecord(state, index), (ENV_MAPS - index - 1) * ENV_TIME_RECORD_BYTES);
            memset(Record(state, index), 0, ENV_RECORD_BYTES); Write64(Record(state, index), map);
            memset(TimeRecord(state, index), 0, ENV_TIME_RECORD_BYTES);
        }
        WriteEffect(EffectBytes(Record(state, index), request->kind), after);
        WriteTiming(TimeBytes(state, index, request->kind), after->value ? &timing : NULL);
        if (!Read32(EffectBytes(Record(state, index), RHH_WORLD_WEATHER))
            && !Read32(EffectBytes(Record(state, index), RHH_WORLD_TERRAIN)))
        {
            memmove(Record(state, index), Record(state, index + 1), (ENV_MAPS - index - 1) * ENV_RECORD_BYTES);
            memmove(TimeRecord(state, index), TimeRecord(state, index + 1), (ENV_MAPS - index - 1) * ENV_TIME_RECORD_BYTES);
            memset(Record(state, ENV_MAPS - 1), 0, ENV_RECORD_BYTES);
            memset(TimeRecord(state, ENV_MAPS - 1), 0, ENV_TIME_RECORD_BYTES);
        }
        Notify(&result, request->kind == RHH_WORLD_WEATHER ? &timing : NULL,
               request->kind == RHH_WORLD_TERRAIN ? &timing : NULL);
    }
unlock:
    sBusy = 0;
done:
    if (scoped) RhhModsWorldTimeScopeEnd(&timeScope);
    if (quote) {
        quote->run_id = RhhModsRunId(); quote->timed = timed; quote->duration_seconds = seconds;
        if (request && request->struct_size >= sizeof(*request)) quote->request = *request;
        quote->result = result; quote->before_timing = beforeTiming;
        quote->other_before_timing = otherTiming;
        quote->after_timing = request && !request->move && !sourceValue
            ? (struct RhhWorldEnvironmentTiming){.struct_size = sizeof(quote->after_timing)} : timing;
    }
    *out = result; return status;
}
static int Preview(const struct RhhWorldEnvironmentRequest *request, struct RhhWorldEnvironmentResult *out)
{ return Change(request, out, 1, 0, 0, NULL, NULL, 0, 0); }
static int Apply(const struct RhhWorldEnvironmentRequest *request, struct RhhWorldEnvironmentResult *out)
{ return Change(request, out, 0, 0, 0, NULL, NULL, 0, 0); }
static int Timed(const struct RhhWorldEnvironmentTimedRequest *request, struct RhhWorldEnvironmentResult *out, int preview)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved)
        return Change(NULL, out, preview, 1, 0, NULL, NULL, 0, 0);
    return Change(&request->request, out, preview, 1, request->duration_seconds, NULL, NULL, 0, 0);
}
static int PreviewTimed(const struct RhhWorldEnvironmentTimedRequest *request, struct RhhWorldEnvironmentResult *out)
{ return Timed(request, out, 1); }
static int ApplyTimed(const struct RhhWorldEnvironmentTimedRequest *request, struct RhhWorldEnvironmentResult *out)
{ return Timed(request, out, 0); }
static int PreviewCheckedCommon(const struct RhhWorldEnvironmentRequest *request, int timed, u64 seconds,
                                struct RhhWorldEnvironmentQuote *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WORLD_ENV_INVALID;
    struct RhhWorldEnvironmentQuote quote = {.struct_size = sizeof(quote)};
    struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result)};
    int status = Change(request, &result, 1, timed, seconds, NULL, &quote, 0, 0);
    *out = quote; return status;
}
static int PreviewChecked(const struct RhhWorldEnvironmentRequest *request, struct RhhWorldEnvironmentQuote *out)
{ return PreviewCheckedCommon(request, 0, 0, out); }
static int ApplyChecked(const struct RhhWorldEnvironmentRequest *request, const struct RhhWorldEnvironmentQuote *expected,
                        struct RhhWorldEnvironmentResult *out)
{
    if (!expected) return Change(NULL, out, 0, 0, 0, NULL, NULL, 0, 0);
    return Change(request, out, 0, 0, 0, expected, NULL, 0, 0);
}
static int PreviewTimedChecked(const struct RhhWorldEnvironmentTimedRequest *request, struct RhhWorldEnvironmentQuote *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved)
        return PreviewCheckedCommon(NULL, 1, 0, out);
    return PreviewCheckedCommon(&request->request, 1, request->duration_seconds, out);
}
static int ApplyTimedChecked(const struct RhhWorldEnvironmentTimedRequest *request, const struct RhhWorldEnvironmentQuote *expected,
                             struct RhhWorldEnvironmentResult *out)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !expected)
        return Change(NULL, out, 0, 1, 0, NULL, NULL, 0, 0);
    return Change(&request->request, out, 0, 1, request->duration_seconds, expected, NULL, 0, 0);
}
static int SourceRequest(const struct RhhWorldEnvironmentSourceRequest *source,
                         struct RhhWorldEnvironmentRequest *request, int *timed, u64 *seconds)
{
    if (!source || source->struct_size < sizeof(*source) || source->reserved
        || (source->clock != RHH_WORLD_ENV_CLOCK_STEPS && source->clock != RHH_WORLD_ENV_CLOCK_TIME
            && source->clock != RHH_WORLD_ENV_CLOCK_PERMANENT)
        || (source->clock == RHH_WORLD_ENV_CLOCK_PERMANENT && (!source->value || source->duration))
        || (!source->value && (source->clock != RHH_WORLD_ENV_CLOCK_STEPS || source->duration))) return -1;
    *timed = source->clock == RHH_WORLD_ENV_CLOCK_PERMANENT ? 2 : source->clock == RHH_WORLD_ENV_CLOCK_TIME;
    *seconds = *timed == 1 ? source->duration : 0;
    *request = (struct RhhWorldEnvironmentRequest){.struct_size = sizeof(*request),
        .kind = source->kind, .priority = source->priority, .map = source->map, .owner = source->owner,
        .duration_steps = *timed ? 0 : source->duration};
    return 0;
}
static int SameSource(const struct RhhWorldEnvironmentSourceRequest *a, const struct RhhWorldEnvironmentSourceRequest *b)
{
    return a->struct_size >= sizeof(*a) && b->struct_size >= sizeof(*b)
        && a->kind == b->kind && a->value == b->value && a->clock == b->clock
        && a->priority == b->priority && a->reserved == b->reserved && a->map == b->map
        && a->owner == b->owner && a->duration == b->duration;
}
static int Source(const struct RhhWorldEnvironmentSourceRequest *source, struct RhhWorldEnvironmentResult *out, int preview,
                  const struct RhhWorldEnvironmentSourceQuote *expected, struct RhhWorldEnvironmentSourceQuote *quote, int anyMap)
{
    struct RhhWorldEnvironmentRequest request; int timed = 0; u64 seconds = 0;
    struct RhhWorldEnvironmentQuote transaction = {.struct_size = sizeof(transaction)};
    if (SourceRequest(source, &request, &timed, &seconds))
        return Change(NULL, out, preview, 0, 0, NULL, quote ? &quote->transaction : NULL, 0, anyMap);
    if (expected && (expected->struct_size < sizeof(*expected) || expected->reserved
                     || !SameSource(source, &expected->request)))
    {
        if (!out || out->struct_size < sizeof(*out)) return RHH_WORLD_ENV_INVALID;
        struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result), .is_preview = preview};
        Result(&result, RHH_WORLD_ENV_STALE, "environment source preview changed");
        *out = result; return RHH_WORLD_ENV_STALE;
    }
    int status = Change(&request, out, preview, timed, seconds,
                        expected ? &expected->transaction : NULL, quote ? &transaction : NULL, source->value, anyMap);
    if (quote) { quote->request = *source; quote->transaction = transaction; }
    return status;
}
static int PreviewSource(const struct RhhWorldEnvironmentSourceRequest *source, struct RhhWorldEnvironmentResult *out)
{ return Source(source, out, 1, NULL, NULL, 0); }
static int ApplySource(const struct RhhWorldEnvironmentSourceRequest *source, struct RhhWorldEnvironmentResult *out)
{ return Source(source, out, 0, NULL, NULL, 0); }
static int PreviewSourceCheckedCommon(const struct RhhWorldEnvironmentSourceRequest *source,
                                      struct RhhWorldEnvironmentSourceQuote *out, int anyMap)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_WORLD_ENV_INVALID;
    struct RhhWorldEnvironmentSourceQuote quote = {.struct_size = sizeof(quote),
        .transaction.struct_size = sizeof(quote.transaction)};
    struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result)};
    int status = Source(source, &result, 1, NULL, &quote, anyMap);
    *out = quote; return status;
}
static int PreviewSourceChecked(const struct RhhWorldEnvironmentSourceRequest *source, struct RhhWorldEnvironmentSourceQuote *out)
{ return PreviewSourceCheckedCommon(source, out, 0); }
static int ApplySourceChecked(const struct RhhWorldEnvironmentSourceRequest *source,
                              const struct RhhWorldEnvironmentSourceQuote *expected, struct RhhWorldEnvironmentResult *out)
{
    if (!expected) return Change(NULL, out, 0, 0, 0, NULL, NULL, 0, 0);
    return Source(source, out, 0, expected, NULL, 0);
}
static int PreviewSourceAt(const struct RhhWorldEnvironmentSourceRequest *source, struct RhhWorldEnvironmentResult *out)
{ return Source(source, out, 1, NULL, NULL, 1); }
static int ApplySourceAt(const struct RhhWorldEnvironmentSourceRequest *source, struct RhhWorldEnvironmentResult *out)
{ return Source(source, out, 0, NULL, NULL, 1); }
static int PreviewSourceAtChecked(const struct RhhWorldEnvironmentSourceRequest *source, struct RhhWorldEnvironmentSourceQuote *out)
{ return PreviewSourceCheckedCommon(source, out, 1); }
static int ApplySourceAtChecked(const struct RhhWorldEnvironmentSourceRequest *source,
                                const struct RhhWorldEnvironmentSourceQuote *expected, struct RhhWorldEnvironmentResult *out)
{
    if (!expected) return Change(NULL, out, 0, 0, 0, NULL, NULL, 0, 1);
    return Source(source, out, 0, expected, NULL, 1);
}
static int PartyMove(u32 partySlot, u32 moveSlot, struct RhhWorldEnvironmentMove *out)
{
    if (!out || out->struct_size < sizeof(*out) || partySlot >= PARTY_SIZE || moveSlot >= 4) return -1;
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
    if (!mons || mons->party_at(partySlot, &mon)) return -1;
    struct Pokemon copy = gParties[B_TRAINER_PLAYER][partySlot];
    u32 slot = GetMonData(&copy, MON_DATA_MOVE1 + moveSlot), builtin = 0;
    RhhContentKey key = SlotMoveKey(slot);
    if (!key || !ResolveMove(key, &builtin)) return -1;
    u64 effect = MoveEffect(key, builtin);
    if (!effect) return -1;
    struct RhhWorldEnvironmentMove result = {.struct_size = sizeof(result), .party_slot = partySlot,
        .move_slot = moveSlot, .usable = !mon.is_egg && copy.hp && !ModsMonIsDead(&copy),
        .kind = effect >> 32, .value = (u32)effect, .actor_identity = mon.identity, .move = key};
    *out = result; return 0;
}
static const struct RhhWorldEnvironmentService sService = {
    {sizeof(sService), RHH_SERVICE_VERSION(1, 10)}, At, Current, Preview, Apply, Context, BattleContext, PartyMove,
    Timing, ContextTiming, PreviewTimed, ApplyTimed, PreviewChecked, ApplyChecked, PreviewTimedChecked, ApplyTimedChecked,
    Inspect, List, PreviewSource, ApplySource, PreviewSourceChecked, ApplySourceChecked,
    PreviewSourceAt, ApplySourceAt, PreviewSourceAtChecked, ApplySourceAtChecked
};
int RhhModsInstallEnvironmentService(void)
{
    if (RhhModsRegister(&sEnvironment)) return -1;
    return RhhModsAddService("rhh:service/world_environment", &sService.header);
}
static void Expire(u64 step)
{
    u8 *state = State();
    if (!state || sBusy || !RhhModsHasActiveRun()) return;
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    struct EnvTime time = Time();
    sBusy = 1;
    for (u32 i = 0; i < ENV_MAPS && Read64(Record(state, i));)
    {
        u8 *record = Record(state, i); RhhContentKey map = Read64(record);
        struct RhhWorldEnvironmentEffect weather = ReadEffect(EffectBytes(record, RHH_WORLD_WEATHER));
        struct RhhWorldEnvironmentEffect terrain = ReadEffect(EffectBytes(record, RHH_WORLD_TERRAIN));
        struct RhhWorldEnvironmentTiming weatherTiming = ReadTiming(TimeBytes(state, i, RHH_WORLD_WEATHER), &weather, step, &time);
        struct RhhWorldEnvironmentTiming terrainTiming = ReadTiming(TimeBytes(state, i, RHH_WORLD_TERRAIN), &terrain, step, &time);
        int expiredWeather = IsExpired(&weatherTiming);
        int expiredTerrain = IsExpired(&terrainTiming);
        if (!expiredWeather && !expiredTerrain) { ++i; continue; }
        struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result), .status = RHH_WORLD_ENV_OK,
            .cause = RHH_WORLD_ENV_EXPIRED, .kind = expiredWeather && expiredTerrain ? 0
                : expiredWeather ? RHH_WORLD_WEATHER : RHH_WORLD_TERRAIN};
        Snapshot(state, map, step, 1, &result.before);
        if (expiredWeather) { memset(EffectBytes(record, RHH_WORLD_WEATHER), 0, ENV_EFFECT_BYTES);
                              memset(TimeBytes(state, i, RHH_WORLD_WEATHER), 0, ENV_TIME_BYTES); }
        if (expiredTerrain) { memset(EffectBytes(record, RHH_WORLD_TERRAIN), 0, ENV_EFFECT_BYTES);
                              memset(TimeBytes(state, i, RHH_WORLD_TERRAIN), 0, ENV_TIME_BYTES); }
        if (!Read32(EffectBytes(record, RHH_WORLD_WEATHER)) && !Read32(EffectBytes(record, RHH_WORLD_TERRAIN)))
        {
            memmove(record, Record(state, i + 1), (ENV_MAPS - i - 1) * ENV_RECORD_BYTES);
            memmove(TimeRecord(state, i), TimeRecord(state, i + 1), (ENV_MAPS - i - 1) * ENV_TIME_RECORD_BYTES);
            memset(Record(state, ENV_MAPS - 1), 0, ENV_RECORD_BYTES);
            memset(TimeRecord(state, ENV_MAPS - 1), 0, ENV_TIME_RECORD_BYTES);
        }
        else ++i;
        Snapshot(state, map, step, 0, &result.after);
        Notify(&result, expiredWeather ? &weatherTiming : NULL, expiredTerrain ? &terrainTiming : NULL);
    }
    sBusy = 0; RhhModsWorldTimeScopeEnd(&timeScope);
}
void RhhModsEnvironmentOnStep(u64 step) { Expire(step); }
static u32 NaturalBattleWeather(void)
{
    switch (GetCurrentWeather())
    {
    case WEATHER_RAIN: case WEATHER_RAIN_THUNDERSTORM: case WEATHER_DOWNPOUR: return RHH_WEATHER_RAIN;
    case WEATHER_DROUGHT: return RHH_WEATHER_SUN;
    case WEATHER_SANDSTORM: return RHH_WEATHER_SAND;
    case WEATHER_SNOW: return RHH_WEATHER_SNOW;
    case WEATHER_FOG_HORIZONTAL: case WEATHER_FOG_DIAGONAL: return RHH_WEATHER_FOG;
    default: return RHH_WEATHER_NONE;
    }
}
void RhhModsEnvironmentQueueBattle(u32 weather, u32 terrain)
{
    sBattleReturn = (typeof(sBattleReturn)){0};
    struct RhhWorldEnvironmentSnapshot current = {.struct_size = sizeof(current)};
    if (!RhhModsHasActiveRun() || Current(&current) || !current.map) return;
    u32 existingWeather = current.weather.value ? current.weather.value : NaturalBattleWeather();
    if (weather && weather <= RHH_WEATHER_HEAVY_RAIN && weather != existingWeather) sBattleReturn.weather = weather;
    if (terrain && terrain <= RHH_TERRAIN_PSYCHIC && terrain != current.terrain.value) sBattleReturn.terrain = terrain;
    if (sBattleReturn.weather || sBattleReturn.terrain)
    { sBattleReturn.map = current.map; sBattleReturn.run = RhhModsRunId(); sBattleReturn.started = RhhEcologySystemTime(); }
}
static void ApplyBattleReturn(void)
{
    if (!sBattleReturn.run) return;
    if (sBattleReturn.run != RhhModsRunId()) { sBattleReturn = (typeof(sBattleReturn)){0}; return; }
    if (gMain.inBattle || !RhhModsCommandsAllowed()) return;
    u64 duration = 1800; int64_t configured;
    const struct RhhEcologyService *ecology = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/ecology", RHH_SERVICE_VERSION(1, 0));
    if (ecology && !ecology->get(sBattleReturn.map, RHH_ECOLOGY_CONDITION,
        RhhContentKeyOf("rhh:ecology/battle_environment_seconds"), &configured)
        && configured > 0 && configured <= ENV_MAX_DURATION) duration = configured;
    u64 now = RhhEcologySystemTime();
    if (!now) return;
    if (!sBattleReturn.started) sBattleReturn.started = now;
    if (now < sBattleReturn.started) return;
    u64 elapsed = now - sBattleReturn.started;
    if (elapsed >= duration) { sBattleReturn = (typeof(sBattleReturn)){0}; return; }
    duration -= elapsed;
    for (u32 kind = RHH_WORLD_WEATHER; kind <= RHH_WORLD_TERRAIN; ++kind)
    {
        u32 *value = kind == RHH_WORLD_WEATHER ? &sBattleReturn.weather : &sBattleReturn.terrain;
        if (!*value) continue;
        struct RhhWorldEnvironmentRequest request = {.struct_size = sizeof(request), .kind = kind,
            .map = sBattleReturn.map, .owner = RhhContentKeyOf(sEnvironmentId)};
        struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result)};
        int status = Change(&request, &result, 0, 3, duration, NULL, NULL, *value, 1);
        if (status != RHH_WORLD_ENV_UNAVAILABLE && status != RHH_WORLD_ENV_STALE) *value = 0;
    }
    if (!sBattleReturn.weather && !sBattleReturn.terrain) sBattleReturn = (typeof(sBattleReturn)){0};
}
void RhhModsEnvironmentOnFrame(void)
{
    u8 *state = State(); u64 step;
    if (!state || sBusy || NativeSaveTransactionActive() || !RhhModsHasActiveRun()) return;
    ApplyBattleReturn();
    for (u32 i = 0; i < ENV_MAPS && Read64(Record(state, i)); ++i)
        if (Read64(TimeBytes(state, i, RHH_WORLD_WEATHER)) || Read64(TimeBytes(state, i, RHH_WORLD_TERRAIN)))
        { if (!Clock(&step)) Expire(step); return; }
}
static u32 BattleSeed(u32 kind)
{
    struct RhhWorldEnvironmentSnapshot view = {.struct_size = sizeof(view)};
    if (!gMain.inBattle || Current(&view)) return 0;
    const struct RhhWorldEnvironmentEffect *effect = kind == RHH_WORLD_WEATHER ? &view.weather : &view.terrain;
    u32 initial = effect->value;
    if (sBattleContext) return initial; /* A nested engine request cannot recursively dispatch. */
    struct RhhBattleEnvironmentSeed context = {.struct_size = sizeof(context), .kind = kind,
        .initial_value = initial, .environment = view};
    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
    {
        context.battle_flags |= RHH_BATTLE_SEED_TRAINER;
        u16 a = TRAINER_BATTLE_PARAM.opponentA, b = TRAINER_BATTLE_PARAM.opponentB;
        if (a && !IsSpecialTrainer(a)) context.opponent_trainers[0] = RhhContentKeyForId(RHH_TRAINER, a);
        if (b && b != UINT16_MAX && !IsSpecialTrainer(b)) context.opponent_trainers[1] = RhhContentKeyForId(RHH_TRAINER, b);
    }
    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) context.battle_flags |= RHH_BATTLE_SEED_DOUBLE;
    if (gBattleTypeFlags & BATTLE_TYPE_FIRST_BATTLE) context.battle_flags |= RHH_BATTLE_SEED_FIRST;
    if (gBattleTypeFlags & BATTLE_TYPE_ROAMER) context.battle_flags |= RHH_BATTLE_SEED_ROAMER;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_ENVIRONMENT_SEED,
        .run_seed = RhhModsRunSeed(), .event_key = view.map, .subject = effect->owner,
        .ordinal = view.step, .args = {kind, initial}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = initial};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBattleContext = &context; RhhModsDispatch(&event, &decision); sBattleContext = NULL;
    bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed) return 0;
    u32 maximum = kind == RHH_WORLD_WEATHER ? RHH_WEATHER_HEAVY_RAIN : RHH_TERRAIN_PSYCHIC;
    return !impure && decision.value <= maximum ? (u32)decision.value : initial;
}
u32 ModsWorldBattleWeather(void) { return BattleSeed(RHH_WORLD_WEATHER); }
u32 ModsWorldBattleTerrain(void)
{ return BattleSeed(RHH_WORLD_TERRAIN); }
