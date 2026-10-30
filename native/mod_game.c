/* Engine adapter. Example mods in native/mods depend only on mod_api.h. */
#include <stdlib.h>
#include <stdio.h>
#include "global.h"
#include "main.h"
#include "mods.h"
#include "mod_host.h"
#include "mods/bundled.h"
#include "native.h"
#include "owned_identity_ledger.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "battle.h"
#include "data.h"
#include "event_data.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "item.h"
#include "item_icon.h"
#include "move.h"
#include "string_util.h"
#include "decompress.h"
#include "overworld.h"
#include "rtc.h"
#include "roamer.h"
#include "wild_encounter.h"
#include "constants/region_map_sections.h"
#include "constants/maps.h"
#include "constants/layouts.h"
#include "constants/form_change_types.h"
#include "constants/characters.h"
#include "constants/egg_ids.h"
#include "mod_content.inc"

struct ModConfig gModDraft;
static u16 sStarters[NUM_SPECIES];
static u32 sStarterCount;
static u64 sSpeciesKeys[NUM_SPECIES];
static bool32 sNormalizing, sHadBalls;
static const char *sExternalDirectory;
void ModsSetExternalBuiltinDirectory(const char *directory) { sExternalDirectory = directory; }
static const char sRunId[] = "rhh:host_run";
enum { RUN_BASE_SIZE = 32, RUN_MOVE_KEYS_OFFSET = RUN_BASE_SIZE,
       RUN_SCHEMA3_SIZE = RUN_BASE_SIZE + RHH_RUNTIME_MOVE_COUNT * 8,
       RUN_FEATURES_OFFSET = RUN_SCHEMA3_SIZE, RUN_SCHEMA4_SIZE = RUN_SCHEMA3_SIZE + 8,
       RUN_PENDING_EGG_OFFSET = RUN_SCHEMA4_SIZE, RUN_SCHEMA5_SIZE = RUN_SCHEMA4_SIZE + 16,
       RUN_EGG_ORDINAL_OFFSET = RUN_SCHEMA5_SIZE, RUN_STATE_SIZE = RUN_SCHEMA5_SIZE + 8 };
enum { RUN_FEATURE_NATURE = 1u, RUN_FEATURE_PENDING_EGG = 2u, RUN_FEATURE_EGG_ORDINAL = 4u };
_Static_assert(MOVE_RANDOM_TEACHABLE < RHH_RUNTIME_MOVE_FIRST, "custom move slots overlap engine IDs");
static u64 Read64(const u8 *p) { u64 n = 0; for (u32 i = 0; i < 8; ++i) n |= (u64)p[i] << (i * 8); return n; }
static void Write64(u8 *p, u64 n) { for (u32 i = 0; i < 8; ++i) p[i] = n >> (i * 8); }
static u8 *Run(void) { return RhhModsState(sRunId); }
static u64 RunValue(u32 offset) { const u8 *s = Run(); return s ? Read64(s + offset) : 0; }
int RhhModsMoveSlot(RhhContentKey key, int allocate, uint32_t *slot)
{
    u8 *state = Run();
    if (!state || !RhhModsHasActiveRun() || !key || !slot || (allocate != 0 && allocate != 1)
        || !RhhContentResolve(RHH_MOVE, key, NULL)) return -1;
    u32 freeSlot = RHH_RUNTIME_MOVE_COUNT;
    for (u32 i = 0; i < RHH_RUNTIME_MOVE_COUNT; ++i)
    {
        u64 current = Read64(state + RUN_MOVE_KEYS_OFFSET + i * 8);
        if (current == key) { *slot = RHH_RUNTIME_MOVE_FIRST + i; return 0; }
        if (!current && freeSlot == RHH_RUNTIME_MOVE_COUNT) freeSlot = i;
    }
    if (!allocate || freeSlot == RHH_RUNTIME_MOVE_COUNT || NativeSaveTransactionActive()) return -1;
    Write64(state + RUN_MOVE_KEYS_OFFSET + freeSlot * 8, key);
    *slot = RHH_RUNTIME_MOVE_FIRST + freeSlot; return 0;
}
RhhContentKey RhhModsMoveKey(uint32_t slot)
{
    const u8 *state = Run();
    if (!state || !RhhModsHasActiveRun() || slot < RHH_RUNTIME_MOVE_FIRST || slot >= 2048u) return 0;
    return Read64(state + RUN_MOVE_KEYS_OFFSET + (slot - RHH_RUNTIME_MOVE_FIRST) * 8);
}
static int Persist(uint64_t run, const uint8_t *data, uint32_t size) { return NativeSaveSetJournal(run, data, size); }
static int MigrateRun(uint32_t schema, const uint8_t *old, uint32_t oldSize, uint8_t *dst, uint32_t size)
{
    if (size != RUN_STATE_SIZE || !((schema == 1 && oldSize == 24) || (schema == 2 && oldSize == RUN_BASE_SIZE)
        || (schema == 3 && oldSize == RUN_SCHEMA3_SIZE) || (schema == 4 && oldSize == RUN_SCHEMA4_SIZE)
        || (schema == 5 && oldSize == RUN_SCHEMA5_SIZE))) return -1;
    memcpy(dst, old, oldSize); memset(dst + oldSize, 0, size - oldSize);
    return 0;
}
static int ValidateRun(const uint8_t *state, uint32_t size)
{
    if (!state || size != RUN_STATE_SIZE) return -1;
    u64 features = Read64(state + RUN_FEATURES_OFFSET);
    bool32 hasEggId = !!(Read64(state + RUN_PENDING_EGG_OFFSET) | Read64(state + RUN_PENDING_EGG_OFFSET + 8));
    if ((features & ~((u64)RUN_FEATURE_NATURE | RUN_FEATURE_PENDING_EGG | RUN_FEATURE_EGG_ORDINAL))
        || !!(features & RUN_FEATURE_PENDING_EGG) != hasEggId
        || !!(features & RUN_FEATURE_EGG_ORDINAL) != !!Read64(state + RUN_EGG_ORDINAL_OFFSET)) return -1;
    for (u32 i = 0; i < RHH_RUNTIME_MOVE_COUNT; ++i)
    {
        u64 key = Read64(state + RUN_MOVE_KEYS_OFFSET + i * 8);
        if (!key) continue;
        if (!RhhContentResolve(RHH_MOVE, key, NULL)) return -1;
        for (u32 j = 0; j < i; ++j)
            if (Read64(state + RUN_MOVE_KEYS_OFFSET + j * 8) == key) return -1;
    }
    return 0;
}
static const struct RhhModDefinition sRun = {
    .struct_size = sizeof(sRun), .abi_version = RHH_MOD_ABI_V1, .mod_version = 6, .state_schema = 6,
    .id = sRunId, .flags = RHH_MOD_REQUIRED, .state_size = RUN_STATE_SIZE, .migrate = MigrateRun, .validate = ValidateRun
};
static int ProjectRunSave(const char *id, const u8 *state, u32 stateSize, u32 *schema, u32 *size)
{
    if (strcmp(id, sRunId)) {
        int status = RhhModsWildQueueProjectSave(id, state, stateSize, schema, size);
        return status ? status : RhhModsEnvironmentProjectSave(id, state, stateSize, schema, size);
    }
    if (!state || stateSize != RUN_STATE_SIZE || !schema || !size) return -1;
    u64 features = Read64(state + RUN_FEATURES_OFFSET);
    if (features & RUN_FEATURE_EGG_ORDINAL) return 0;
    if (!(features & (RUN_FEATURE_NATURE | RUN_FEATURE_PENDING_EGG)))
    { *schema = 3; *size = RUN_SCHEMA3_SIZE; }
    else if (!(features & RUN_FEATURE_PENDING_EGG))
    { *schema = 4; *size = RUN_SCHEMA4_SIZE; }
    else
    { *schema = 5; *size = RUN_SCHEMA5_SIZE; }
    return 0;
}
int ModsDaycareOrdinalState(u64 *next)
{
    if (!next || !Run() || !RhhModsHasActiveRun()) return -1;
    *next = (RunValue(RUN_FEATURES_OFFSET) & RUN_FEATURE_EGG_ORDINAL) ? RunValue(RUN_EGG_ORDINAL_OFFSET) : 0;
    return 0;
}
int ModsEnableDaycareOrdinal(void)
{
    u8 *state = Run();
    if (!state || !RhhModsHasActiveRun() || gMain.inBattle || NativeSaveTransactionActive() || !RhhModsCommandsAllowed()) return -1;
    if (!(Read64(state + RUN_FEATURES_OFFSET) & RUN_FEATURE_EGG_ORDINAL)) {
        Write64(state + RUN_EGG_ORDINAL_OFFSET, 1);
        Write64(state + RUN_FEATURES_OFFSET, Read64(state + RUN_FEATURES_OFFSET) | RUN_FEATURE_EGG_ORDINAL);
    }
    return 0;
}
u64 ModsCommitDaycareOrdinal(void)
{
    u8 *state = Run(); u64 next;
    if (!state || ModsDaycareOrdinalState(&next) || !next || next == UINT64_MAX) return 0;
    if (NativeSaveTransactionActive()) NativeSaveInvalidateTransaction();
    Write64(state + RUN_EGG_ORDINAL_OFFSET, next + 1);
    return next;
}
bool32 ModsNatureFormatEnabled(void)
{ return !!(RunValue(RUN_FEATURES_OFFSET) & RUN_FEATURE_NATURE); }
int ModsEnableNatureFormat(void)
{
    u8 *state = Run();
    if (!state || !RhhModsHasActiveRun() || NativeSaveTransactionActive()) return -1;
    if (!ModsNatureFormatEnabled() && !ModsNatureFormatCanEnable()) return -1;
    Write64(state + RUN_FEATURES_OFFSET, Read64(state + RUN_FEATURES_OFFSET) | RUN_FEATURE_NATURE);
    return 0;
}
int ModsPendingEggId(u64 *high, u64 *low)
{
    u8 *state = Run();
    if (!state || !high || !low || !RhhModsHasActiveRun()
        || !(Read64(state + RUN_FEATURES_OFFSET) & RUN_FEATURE_PENDING_EGG)) return -1;
    *high = Read64(state + RUN_PENDING_EGG_OFFSET); *low = Read64(state + RUN_PENDING_EGG_OFFSET + 8);
    return 0;
}
void ModsClearPendingEggId(void)
{
    u8 *state = Run(); if (!state || !RhhModsHasActiveRun()) return;
    if (NativeSaveTransactionActive()) NativeSaveInvalidateTransaction();
    Write64(state + RUN_PENDING_EGG_OFFSET, 0); Write64(state + RUN_PENDING_EGG_OFFSET + 8, 0);
    Write64(state + RUN_FEATURES_OFFSET, Read64(state + RUN_FEATURES_OFFSET) & ~((u64)RUN_FEATURE_PENDING_EGG));
}
int ModsMintPendingEggId(void)
{
    u8 *state = Run(); struct NativeOwnedId id;
    if (!state || !RhhModsHasActiveRun()) return -1;
    for (u32 attempt = 0; attempt < 4; ++attempt)
    {
        if (NativeOwnedEntropy(&id, sizeof(id))) return -1;
        if (id.high || id.low)
        {
            if (NativeSaveTransactionActive()) NativeSaveInvalidateTransaction();
            Write64(state + RUN_PENDING_EGG_OFFSET, id.high); Write64(state + RUN_PENDING_EGG_OFFSET + 8, id.low);
            Write64(state + RUN_FEATURES_OFFSET, Read64(state + RUN_FEATURES_OFFSET) | RUN_FEATURE_PENDING_EGG);
            return 0;
        }
    }
    return -1;
}
static const struct RhhWorldStepSnapshot *sWorldStepContext;
static bool32 sAdvancingWorldStep;
static int WorldClockCurrent(struct RhhWorldClockSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out) || !Run() || !RhhModsHasActiveRun()) return -1;
    *out = (struct RhhWorldClockSnapshot){.struct_size = sizeof(*out), .run_id = RunValue(8), .steps = RunValue(24)};
    return 0;
}
static int WorldStepContext(struct RhhWorldStepSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWorldStepContext) return -1;
    *out = *sWorldStepContext; return 0;
}
static const struct RhhWorldClockService sWorldClock = {
    {sizeof(sWorldClock), RHH_SERVICE_VERSION(1, 0)}, WorldClockCurrent, WorldStepContext
};
static const struct RhhWorldTimeScope *sWorldTimeScope;
/* Sample the same RTC source and saved offset as the engine, without touching
 * gLocalTime, the current day-period globals, or game random state. */
static int WorldTimeSample(struct RhhWorldTimeSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun()
        || !gSaveBlock2Ptr || (OW_USE_FAKE_RTC && !gSaveBlock3Ptr)) return -1;
    if (sWorldTimeScope)
    {
        if (!sWorldTimeScope->available) return -1;
        *out = sWorldTimeScope->sample; return 0;
    }
    struct SiiRtcInfo rtc = {0}; struct Time local = {0};
    RtcGetInfo(&rtc);
    u32 year = ConvertBcdToBinary(rtc.year), month = ConvertBcdToBinary(rtc.month);
    u32 day = ConvertBcdToBinary(rtc.day), hour = ConvertBcdToBinary(rtc.hour);
    u32 minute = ConvertBcdToBinary(rtc.minute), second = ConvertBcdToBinary(rtc.second);
    if (year > 99 || month < 1 || month > 12 || !day
        || day > (u32)sNumDaysInMonths[month - 1] + (month == 2 && IsLeapYear(year))
        || hour >= 24 || minute >= 60 || second >= 60) return -1;
    RtcCalcTimeDifference(&rtc, &local, &gSaveBlock2Ptr->localTimeOffset);
    if (local.hours < 0 || local.hours >= 24 || local.minutes < 0 || local.minutes >= 60
        || local.seconds < 0 || local.seconds >= 60) return -1;
    u32 override = GetTimeOfDayOverride();
    u32 effective = override ? override : local.hours;
    u32 period;
    switch (GetTimeOfDayForHour(effective))
    {
    case TIME_MORNING: period = RHH_TIME_MORNING; break;
    case TIME_DAY: period = RHH_TIME_DAY; break;
    case TIME_EVENING: period = RHH_TIME_EVENING; break;
    case TIME_NIGHT: period = RHH_TIME_NIGHT; break;
    default: return -1;
    }
    u32 status = RtcGetErrorStatus();
    *out = (struct RhhWorldTimeSnapshot){.struct_size = sizeof(*out),
        .source = OW_USE_FAKE_RTC ? RHH_TIME_FAKE : RHH_TIME_REAL, .period = period,
        .flags = (status & RTC_ERR_FLAG_MASK) ? RHH_TIME_RTC_FALLBACK : 0,
        .local_day = local.days, .hour = local.hours, .minute = local.minutes, .second = local.seconds,
        .effective_hour = effective, .override_hour = override, .rtc_status = status,
        .run_id = RunValue(8), .step = RunValue(24)};
    return 0;
}
void RhhModsWorldTimeScopeBegin(struct RhhWorldTimeScope *scope)
{
    scope->previous = sWorldTimeScope;
    scope->sample = (struct RhhWorldTimeSnapshot){.struct_size = sizeof(scope->sample)};
    scope->available = !WorldTimeSample(&scope->sample);
    sWorldTimeScope = scope;
}
void RhhModsWorldTimeScopeEnd(struct RhhWorldTimeScope *scope)
{
    if (sWorldTimeScope == scope) sWorldTimeScope = scope->previous;
}
static const struct RhhWorldTimeService sWorldTime = {
    {sizeof(sWorldTime), RHH_SERVICE_VERSION(1, 0)}, WorldTimeSample
};
static const struct RhhWildEncounterContext *sWildContext;
static const struct RhhWildWeightContext *sWildWeightContext;
static const struct RhhWildRateContext *sWildRateContext;
static const struct RhhWildSpawnIntervalContext *sWildSpawnIntervalContext;
static const struct RhhFishingBiteContext *sFishingBiteContext;
static const struct RhhWildEncounterCancellation *sWildCancellationContext;
static struct RhhScriptedEncounterSource sWildScriptedScope;
u64 ModsWildScriptedKey(const char *id)
{
    if (!id) return 0;
    u32 i = 0, colon = UINT32_MAX;
    for (; i < RHH_MOD_ID_MAX && id[i]; ++i)
    {
        char c = id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == ':' || c == '/' || c == '-' || c == '_' || c == '.')) return 0;
        if (c == ':') { if (colon != UINT32_MAX) return 0; colon = i; }
    }
    if (i == RHH_MOD_ID_MAX || colon == UINT32_MAX || !colon
        || i <= colon + 11 || strncmp(id + colon + 1, "encounter/", 10)) return 0;
    return RhhContentKeyOf(id);
}
struct RhhScriptedEncounterSource ModsWildScriptedScopeSwap(struct RhhScriptedEncounterSource source)
{
    struct RhhScriptedEncounterSource previous = sWildScriptedScope;
    sWildScriptedScope = source;
    return previous;
}
void ModsWildScriptedBattleSchedule(u32 task)
{
    ModsScriptedBattleSchedule(task, sWildScriptedScope);
}
void ModsWildFirstBattleSelect(void)
{
    ModsFirstBattleSelect(sWildScriptedScope);
}
static int WildScriptedSource(struct RhhScriptedEncounterSource *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildContext || !sWildScriptedScope.encounter
        || (event != RHH_WILD_SCRIPTED_CANDIDATE && event != RHH_WILD_EVENT_CANDIDATE
            && event != RHH_CAN_BEGIN_SCRIPTED_WILD && event != RHH_CAN_BEGIN_EVENT_WILD
            && event != RHH_WILD_SCRIPTED_CANCELLED && event != RHH_WILD_EVENT_CANCELLED
            && event != RHH_WILD_SCRIPTED_CLEANUP_FINISHED && event != RHH_WILD_EVENT_CLEANUP_FINISHED
            && event != RHH_WILD_SCRIPTED_READY && event != RHH_WILD_GHOST_READY
            && event != RHH_WILD_TUTORIAL_READY && event != RHH_WILD_FIRST_BATTLE_READY
            && event != RHH_CAN_BEGIN_TUTORIAL_WILD && event != RHH_WILD_TUTORIAL_CANCELLED
            && event != RHH_WILD_TUTORIAL_CLEANUP_FINISHED)) return -1;
    *out = sWildScriptedScope; return 0;
}
static int WildContext(struct RhhWildEncounterContext *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sWildContext
        || (event != RHH_WILD_CANDIDATE && event != RHH_CAN_GENERATE_WILD && event != RHH_CAN_GENERATE_SPECIAL_WILD
            && event != RHH_CAN_GENERATE_DEXNAV_WILD && event != RHH_WILD_GENERATED && event != RHH_WILD_SPECIAL_GENERATED
            && event != RHH_WILD_DEXNAV_GENERATED && event != RHH_WILD_CANCELLED
            && event != RHH_WILD_SPECIAL_CANCELLED && event != RHH_WILD_DEXNAV_CANCELLED
            && event != RHH_CAN_GENERATE_FACILITY_WILD && event != RHH_WILD_FACILITY_GENERATED
            && event != RHH_WILD_FACILITY_CANCELLED && event != RHH_WILD_SCRIPTED_READY
            && event != RHH_WILD_GHOST_READY && event != RHH_WILD_TUTORIAL_READY && event != RHH_WILD_FIRST_BATTLE_READY
            && event != RHH_WILD_SCRIPTED_CANDIDATE && event != RHH_CAN_GENERATE_MANUAL_FACILITY_WILD
            && event != RHH_WILD_MANUAL_FACILITY_GENERATED && event != RHH_WILD_MANUAL_FACILITY_CANCELLED
            && event != RHH_CAN_BEGIN_SCRIPTED_WILD && event != RHH_WILD_SCRIPTED_CANCELLED
            && event != RHH_WILD_EVENT_CANDIDATE && event != RHH_CAN_BEGIN_EVENT_WILD
            && event != RHH_WILD_EVENT_CANCELLED && event != RHH_WILD_SCRIPTED_CLEANUP_FINISHED
            && event != RHH_WILD_EVENT_CLEANUP_FINISHED && event != RHH_CAN_BEGIN_TUTORIAL_WILD
            && event != RHH_WILD_TUTORIAL_CANCELLED && event != RHH_WILD_TUTORIAL_CLEANUP_FINISHED)) return -1;
    *out = *sWildContext; return 0;
}
static int WildWeightContext(struct RhhWildWeightContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildWeightContext
        || RhhModsCurrentEvent() != RHH_WILD_WEIGHT) return -1;
    *out = *sWildWeightContext; return 0;
}
static int WildCancellationContext(struct RhhWildEncounterCancellation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildCancellationContext
        || (RhhModsCurrentEvent() != RHH_WILD_CANCELLED
            && RhhModsCurrentEvent() != RHH_WILD_SPECIAL_CANCELLED
            && RhhModsCurrentEvent() != RHH_WILD_DEXNAV_CANCELLED
            && RhhModsCurrentEvent() != RHH_WILD_FACILITY_CANCELLED
            && RhhModsCurrentEvent() != RHH_WILD_MANUAL_FACILITY_CANCELLED
            && RhhModsCurrentEvent() != RHH_WILD_SCRIPTED_CANCELLED
            && RhhModsCurrentEvent() != RHH_WILD_EVENT_CANCELLED
            && RhhModsCurrentEvent() != RHH_WILD_SCRIPTED_CLEANUP_FINISHED
            && RhhModsCurrentEvent() != RHH_WILD_EVENT_CLEANUP_FINISHED
            && RhhModsCurrentEvent() != RHH_WILD_TUTORIAL_CANCELLED
            && RhhModsCurrentEvent() != RHH_WILD_TUTORIAL_CLEANUP_FINISHED)) return -1;
    *out = *sWildCancellationContext; return 0;
}
static int WildRateContext(struct RhhWildRateContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildRateContext
        || RhhModsCurrentEvent() != RHH_WILD_ENCOUNTER_RATE) return -1;
    *out = *sWildRateContext; return 0;
}
static int WildSpawnIntervalContext(struct RhhWildSpawnIntervalContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWildSpawnIntervalContext
        || RhhModsCurrentEvent() != RHH_WILD_SPAWN_INTERVAL) return -1;
    *out = *sWildSpawnIntervalContext; return 0;
}
static int FishingBiteContext(struct RhhFishingBiteContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sFishingBiteContext
        || RhhModsCurrentEvent() != RHH_FISHING_BITE_RATE) return -1;
    *out = *sFishingBiteContext; return 0;
}
static const struct RhhWildEncounterService sWildService = {
    {sizeof(sWildService), RHH_SERVICE_VERSION(1, 20)}, WildContext, WildWeightContext, WildCancellationContext,
    WildRateContext, WildSpawnIntervalContext, FishingBiteContext, WildScriptedSource, ModsScriptedBattleSource
};
static struct RhhModEvent Event(u32 kind, u64 key, u64 subject, u64 ordinal)
{ return (struct RhhModEvent){sizeof(struct RhhModEvent), kind, RunValue(0), key, subject, ordinal, {0}}; }
static struct RhhModDecision Decision(u64 value)
{ return (struct RhhModDecision){.struct_size = sizeof(struct RhhModDecision), .allowed = 1, .value = value}; }
struct WildProposalGuard { rng_value_t rng, rng2; enum Ability lastAbility; };
static struct WildProposalGuard WildProposalBegin(void)
{ return (struct WildProposalGuard){gRngValue, gRng2Value, gLastUsedAbility}; }
static bool32 WildProposalEnd(const struct WildProposalGuard *guard)
{
    bool32 used = memcmp(&guard->rng, &gRngValue, sizeof(gRngValue))
        || memcmp(&guard->rng2, &gRng2Value, sizeof(gRng2Value));
    gRngValue = guard->rng; gRng2Value = guard->rng2; gLastUsedAbility = guard->lastAbility;
    return used;
}
static u64 Setting(const char *mod, const char *id)
{ struct RhhModValue v; return RhhModsGetSetting(mod, id, &v) ? 0 : v.value; }
static void Configure(const char *mod, const char *id, u32 type, u64 value)
{
    const struct RhhModValue v = {type, 0, value};
    if (RhhModsConfigure(mod, id, &v)) { fprintf(stderr, "Mod setting failed: %s/%s: %s\n", mod, id, RhhModsError()); abort(); }
}
struct ModConfig ModsGetConfig(void)
{
    u32 flags = 0;
    if (Setting("rhh:nuzlocke", "enabled")) flags |= MOD_NUZLOCKE;
    if (Setting("rhh:trainer_randomizer", "enabled")) flags |= MOD_TRAINERS;
    if (Setting("rhh:equal_bst", "enabled")) flags |= MOD_EQUALIZE;
    if (Setting("rhh:level_cap", "enabled")) flags |= MOD_LEVEL_CAP;
    if (Setting("rhh:follower", "enabled")) flags |= MOD_FOLLOWER;
    return (struct ModConfig){flags, Setting("rhh:wild_randomizer", "mode"), Setting("rhh:battle_music", "style"),
        Setting("rhh:starter", "species"), RunValue(0)};
}
bool32 ModsHasRule(u32 flag)
{
    if (flag == MOD_FOLLOWER)
    {
        struct RhhModEvent e = Event(RHH_FOLLOWER_ENABLED, 0, 0, 0);
        struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d); return d.allowed && d.value;
    }
    return (ModsGetConfig().flags & flag) != 0;
}
void ModsSetMusic(u32 music) { if (Run() && music <= 5) Configure("rhh:battle_music", "style", RHH_SETTING_CHOICE, music); }
static bool32 Eligible(u32 species)
{
    if (!species || species >= NUM_SPECIES || !gSpeciesInfo[species].baseHP) return FALSE;
    const struct SpeciesInfo *i = &gSpeciesInfo[species];
    if (i->isMegaEvolution || i->isPrimalReversion || i->isUltraBurst || i->isGigantamax || i->isTeraForm) return FALSE;
    if (i->formChangeTable)
        for (u32 n = 0; i->formChangeTable[n].method != FORM_CHANGE_TERMINATOR; ++n)
            if ((i->formChangeTable[n].method == FORM_CHANGE_END_BATTLE || i->formChangeTable[n].method == FORM_CHANGE_FAINT)
                && i->formChangeTable[n].targetSpecies != species) return FALSE;
    return TRUE;
}
bool32 ModsStarterEligible(u32 species)
{ return Eligible(species) && !gSpeciesInfo[species].isRestrictedLegendary && !gSpeciesInfo[species].isSubLegendary && !gSpeciesInfo[species].isMythical; }
u32 ModsStarterCount(void) { return sStarterCount; }
u32 ModsStarterAt(u32 index) { return index < sStarterCount ? sStarters[index] : SPECIES_NONE; }
static int CompareSpecies(const void *a, const void *b)
{ return strcmp(RhhContentName(RHH_SPECIES, *(const u16 *)a), RhhContentName(RHH_SPECIES, *(const u16 *)b)); }
static void ContentMetadata(u32 kind, u32 id, struct RhhContentInfo *out)
{
    if (kind != RHH_SPECIES) return;
    out->flags = 0;
    if (id >= NUM_SPECIES || !gSpeciesInfo[id].baseHP) return;
    const struct SpeciesInfo *i = &gSpeciesInfo[id];
    out->flags = RHH_CONTENT_ENABLED;
    if (Eligible(id)) out->flags |= RHH_CONTENT_CREATABLE;
    if (i->isRestrictedLegendary || i->isSubLegendary) out->flags |= RHH_CONTENT_LEGENDARY;
    if (i->isMythical) out->flags |= RHH_CONTENT_MYTHICAL;
    const u16 stats[] = {i->baseHP, i->baseAttack, i->baseDefense, i->baseSpeed, i->baseSpAttack, i->baseSpDefense};
    memcpy(out->base_stats, stats, sizeof(stats));
}
static const char *DisplayGlyph(u8 glyph, char one[2])
{
    one[1] = 0;
    if (glyph >= CHAR_A && glyph <= CHAR_Z) one[0] = 'A' + glyph - CHAR_A;
    else if (glyph >= CHAR_a && glyph <= CHAR_z) one[0] = 'a' + glyph - CHAR_a;
    else if (glyph >= CHAR_0 && glyph <= CHAR_9) one[0] = '0' + glyph - CHAR_0;
    else switch (glyph) {
    case CHAR_SPACE: one[0] = ' '; break; case CHAR_EXCL_MARK: one[0] = '!'; break;
    case CHAR_QUESTION_MARK: one[0] = '?'; break; case CHAR_PERIOD: one[0] = '.'; break;
    case CHAR_HYPHEN: one[0] = '-'; break; case CHAR_COLON: one[0] = ':'; break;
    case CHAR_COMMA: one[0] = ','; break; case CHAR_SEMICOLON: one[0] = ';'; break;
    case CHAR_SLASH: one[0] = '/'; break; case CHAR_PLUS: one[0] = '+'; break;
    case CHAR_AMPERSAND: one[0] = '&'; break; case CHAR_EQUALS: one[0] = '='; break;
    case CHAR_LEFT_PAREN: one[0] = '('; break; case CHAR_RIGHT_PAREN: one[0] = ')'; break;
    case CHAR_PERCENT: one[0] = '%'; break; case CHAR_LESS_THAN: one[0] = '<'; break;
    case CHAR_GREATER_THAN: one[0] = '>'; break; case CHAR_E_ACUTE: one[0] = 'E'; break;
    case CHAR_e_ACUTE: one[0] = 'e'; break;
    case CHAR_SGL_QUOTE_LEFT: case CHAR_SGL_QUOTE_RIGHT: one[0] = '\''; break;
    case CHAR_DBL_QUOTE_LEFT: case CHAR_DBL_QUOTE_RIGHT: one[0] = '"'; break;
    case CHAR_MALE: return "(M)"; case CHAR_FEMALE: return "(F)";
    default: one[0] = '?'; break;
    }
    return one;
}
static int ContentDisplayName(u32 kind, RhhContentKey key, char *out, u32 capacity)
{
    if (!RhhModsFinalized() || !out || !capacity) return -1;
    char result[128]; u32 length = 0;
    if (kind == RHH_SPECIES || kind == RHH_ITEM)
    {
        u32 id; const u8 *name; u32 limit;
        if (RhhContentResolve(kind, key, &id) || !id) return -1;
        if (kind == RHH_SPECIES) {
            if (id >= NUM_SPECIES || !gSpeciesInfo[id].baseHP) return -1;
            name = GetSpeciesName(id); limit = POKEMON_NAME_LENGTH + 1;
        } else {
            if (id >= ITEMS_COUNT || !gItemsInfo[id].name) return -1;
            name = GetItemName(id); limit = ITEM_NAME_LENGTH + 1;
        }
        u32 i = 0;
        while (i < limit && name[i] != EOS) {
            char one[2]; const char *glyph = DisplayGlyph(name[i++], one); size_t size = strlen(glyph);
            if (length + size >= sizeof(result)) return -1;
            memcpy(result + length, glyph, size); length += size;
        }
        if (i == limit || !length) return -1;
        result[length] = 0;
    }
    else if (kind == RHH_MOVE) {
        const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 8));
        if (!moves || moves->name(key, result, sizeof(result))) return -1;
    } else if (kind == RHH_ABILITY) {
        const struct RhhAbilityService *abilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 10));
        if (!abilities || abilities->name(key, result, sizeof(result))) return -1;
    } else if (kind == RHH_TYPE) {
        const struct RhhTypeCatalogService *types = (const void *)RhhModsHostAPI()->get_service("rhh:service/type_catalog", RHH_SERVICE_VERSION(1, 1));
        struct RhhTypeDisplayInfo info = {.struct_size = sizeof(info)};
        if (!types || types->describe(key, &info)) return -1;
        memcpy(result, info.label, sizeof(info.label));
    } else return -1;
    length = strlen(result);
    if (capacity <= length) return -1;
    memcpy(out, result, length + 1); return 0;
}
static int ContentDisplayDescription(u32 kind, RhhContentKey key, char *out, u32 capacity)
{
    if (!RhhModsFinalized() || !out || !capacity) return -1;
    const char *authored = NULL;
    const u8 *source = NULL;
    u32 id;
    if (kind == RHH_MOVE)
    {
        const struct RhhModMove *custom = RhhModsFindCustomMove(key);
        if (custom) { if (!ModsCustomMoveAvailable(custom)) return -1; authored = custom->description; }
        else {
            if (RhhContentResolve(kind, key, &id) || !id || id >= MOVES_COUNT_ALL
                || gMovesInfo[id].effect == EFFECT_PLACEHOLDER) return -1;
            source = gMovesInfo[id].description;
        }
    }
    else if (kind == RHH_ABILITY)
    {
        const struct RhhModAbility *custom = RhhModsCustomAbility(key);
        if (custom) authored = custom->description;
        else {
            if (RhhContentResolve(kind, key, &id) || !id || id >= ABILITIES_COUNT) return -1;
            source = gAbilitiesInfo[id].description;
        }
    }
    else if (kind == RHH_ITEM)
    {
        if (RhhContentResolve(kind, key, &id) || !id || id >= ITEMS_COUNT || !gItemsInfo[id].name) return -1;
        source = gItemsInfo[id].description;
    }
    else return -1;
    if (authored)
    {
        size_t length = strlen(authored);
        if (length >= capacity) return -1;
        memcpy(out, authored, length + 1); return 0;
    }
    if (!source) return -1;
    char result[RHH_CONTENT_DESCRIPTION_BYTES]; u32 input = 0, length = 0;
    while (input < sizeof(result) && source[input] != EOS)
    {
        u8 c = source[input++]; char one[2]; const char *glyph;
        if (c == EXT_CTRL_CODE_BEGIN)
        {
            if (input >= sizeof(result)) return -1;
            u32 size = GetExtCtrlCodeLength(source[input]);
            if (!size || size > sizeof(result) - input) return -1;
            input += size; continue;
        }
        if (c == PLACEHOLDER_BEGIN || c == CHAR_DYNAMIC || c == CHAR_KEYPAD_ICON || c == CHAR_EXTRA_SYMBOL)
        { if (input >= sizeof(result) || source[input] == EOS) return -1; ++input; glyph = "?"; }
        else if (c == CHAR_NEWLINE || c == CHAR_PROMPT_SCROLL || c == CHAR_PROMPT_CLEAR) glyph = "\n";
        else if (c == CHAR_NBSP || c == CHAR_BARD_WORD_DELIMIT || c == CHAR_SPACER) glyph = " ";
        else if (c == CHAR_ZWS) glyph = "";
        else if (c == CHAR_ELLIPSIS) glyph = "...";
        else if (c == CHAR_MULT_SIGN) glyph = "x";
        else if (c == CHAR_LV) glyph = "Lv";
        else glyph = DisplayGlyph(c, one);
        size_t size = strlen(glyph);
        if (size >= sizeof(result) - length) return -1;
        memcpy(result + length, glyph, size); length += size;
    }
    if (input == sizeof(result) || length >= capacity) return -1;
    result[length] = 0; memcpy(out, result, length + 1); return 0;
}
static int ContentItemIcon(RhhContentKey key, u8 *out, u32 capacity)
{
    u32 item;
    if (!RhhModsFinalized() || !out || capacity < 24u * 24u * 4u
        || RhhContentResolve(RHH_ITEM, key, &item) || !item || item >= ITEMS_COUNT || !gItemsInfo[item].name) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    const u32 *compressed = GetItemIconPic(item);
    const u16 *palette = GetItemIconPalette(item);
    bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (impure || !compressed || !palette || !IsCompressedData(compressed)
        || GetDecompressedDataSize(compressed) != 24u * 24u / 2u) return -1;
    u32 aligned[24u * 24u / 8u];
    if (!TryDecompressDataWithHeaderWram(compressed, aligned)) return -1;
    const u8 *pixels = (const u8 *)aligned;
    for (u32 y = 0; y < 24; ++y) for (u32 x = 0; x < 24; ++x) {
        u32 offset = ((y / 8) * 3 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
        u32 colorIndex = (pixels[offset] >> ((x & 1) * 4)) & 15, dst = (y * 24 + x) * 4;
        u16 color = palette[colorIndex];
        for (u32 c = 0; c < 3; ++c) { u32 v = (color >> (5 * c)) & 31; out[dst + c] = (v << 3) | (v >> 2); }
        out[dst + 3] = colorIndex ? 255 : 0;
    }
    return 0;
}
struct PokemonIconSource { struct RhhPokemonIcon info; const u8 *pixels; u16 palette[16]; };
static int PokemonIconSource(const struct RhhPokemonIconRequest *request, struct PokemonIconSource *out)
{
    u32 species;
    if (!request || request->struct_size < sizeof(*request) || !out || !RhhModsFinalized() || request->reserved
        || (request->flags & ~RHH_POKEMON_ICON_EGG) || RhhContentResolve(RHH_SPECIES, request->species, &species)
        || !species || species >= NUM_SPECIES || !gSpeciesInfo[species].baseHP) return -1;
    u32 resolved = GetIconSpecies(species, request->personality);
    if (resolved >= NUM_SPECIES || gSpeciesInfo[resolved].eggId >= EGG_ID_COUNT
        || gSpeciesInfo[species].eggId >= EGG_ID_COUNT) return -1;
    bool32 egg = !!(request->flags & RHH_POKEMON_ICON_EGG);
    const u8 *pixels = GetMonIconPtrIsEgg(species, request->personality, egg);
    u32 bytes = RhhModsPokemonIconBytes(pixels);
    if (!pixels || !bytes || bytes % (32u * 32u / 2u)) return -1;
    u32 palette = gSpeciesInfo[species].iconPalIndex, flags = request->flags;
    if (egg) {
        u32 eggId = gSpeciesInfo[species].eggId;
        palette = eggId == EGG_ID_NONE ? gSpeciesInfo[SPECIES_EGG].iconPalIndex : gEggDatas[eggId].eggIconPalIndex;
    }
    else {
#if P_GENDER_DIFFERENCES
        if (gSpeciesInfo[species].iconSpriteFemale && IsPersonalityFemale(species, request->personality))
            palette = gSpeciesInfo[species].iconPalIndexFemale;
        if (gSpeciesInfo[resolved].iconSpriteFemale && IsPersonalityFemale(resolved, request->personality))
            flags |= RHH_POKEMON_ICON_FEMALE;
#endif
        if (pixels == gSpeciesInfo[SPECIES_NONE].iconSprite) flags |= RHH_POKEMON_ICON_PLACEHOLDER;
    }
    if (!RhhModsCopyPokemonIconPalette(palette, out->palette)) return -1;
    out->info = (struct RhhPokemonIcon){.struct_size = sizeof(out->info), .flags = flags,
        .width = 32, .height = 32, .frame_count = bytes / (32u * 32u / 2u),
        .species = RhhContentKeyForId(RHH_SPECIES, species),
        .resolved_species = RhhContentKeyForId(RHH_SPECIES, resolved)};
    out->pixels = pixels;
    return out->info.species ? 0 : -1;
}
static int PokemonIconInfo(const struct RhhPokemonIconRequest *request, struct RhhPokemonIcon *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct PokemonIconSource source;
    if (PokemonIconSource(request, &source)) return -1;
    *out = source.info; return 0;
}
static int PokemonIconRgba(const struct RhhPokemonIconRequest *request, u32 frame, u8 *out, u32 capacity)
{
    if (!out || capacity < 32u * 32u * 4u) return -1;
    struct PokemonIconSource source;
    if (PokemonIconSource(request, &source) || frame >= source.info.frame_count) return -1;
    const u8 *pixels = source.pixels + (size_t)frame * (32u * 32u / 2u);
    for (u32 y = 0; y < 32; ++y) for (u32 x = 0; x < 32; ++x) {
        u32 offset = ((y / 8) * 4 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
        u32 colorIndex = (pixels[offset] >> ((x & 1) * 4)) & 15, dst = (y * 32 + x) * 4;
        u16 color = source.palette[colorIndex];
        for (u32 c = 0; c < 3; ++c) { u32 v = (color >> (5 * c)) & 31; out[dst + c] = (v << 3) | (v >> 2); }
        out[dst + 3] = colorIndex ? 255 : 0;
    }
    return 0;
}
static const struct RhhContentDisplayService sContentDisplayService = {
    {sizeof(sContentDisplayService), RHH_SERVICE_VERSION(1, 3)}, ContentDisplayName, ContentItemIcon,
    PokemonIconInfo, PokemonIconRgba, ContentDisplayDescription
};
/* Shared preference tags are declarative: mods may associate additional
 * species with them through the existing ecology-tags registry. */
static const struct RhhModEcologyTag sPreferenceTags[] = {
    {sizeof(struct RhhModEcologyTag), 0, "rhh:preference/rain", "Rain", "Favours rain."},
    {sizeof(struct RhhModEcologyTag), 0, "rhh:preference/sun", "Sun", "Favours sunshine."},
    {sizeof(struct RhhModEcologyTag), 0, "rhh:preference/sand", "Sand", "Favours sandstorms."},
    {sizeof(struct RhhModEcologyTag), 0, "rhh:preference/snow", "Snow", "Favours snow or hail."},
    {sizeof(struct RhhModEcologyTag), 0, "rhh:preference/fog", "Fog", "Favours fog."},
    {sizeof(struct RhhModEcologyTag), 0, "rhh:preference/grassy", "Grassy terrain", "Favours grassy terrain."},
    {sizeof(struct RhhModEcologyTag), 0, "rhh:preference/misty", "Misty terrain", "Favours misty terrain."},
    {sizeof(struct RhhModEcologyTag), 0, "rhh:preference/electric", "Electric terrain", "Favours electric terrain."},
    {sizeof(struct RhhModEcologyTag), 0, "rhh:preference/psychic", "Psychic terrain", "Favours psychic terrain."},
    {sizeof(struct RhhModEcologyTag), 0, "rhh:preference/berries", "Ripe berries", "Favours ripe berries."},
    {sizeof(struct RhhModEcologyTag), 0, "rhh:preference/wind", "Wind", "Favours strong winds."},
};
static const struct { u32 species, tag; } sDefaultPreferences[] = {
    {SPECIES_LOTAD, 0}, {SPECIES_PSYDUCK, 0}, {SPECIES_VULPIX, 1},
    {SPECIES_SANDSHREW, 2}, {SPECIES_SPHEAL, 3}, {SPECIES_LOTAD, 5},
    {SPECIES_CLEFAIRY, 6}, {SPECIES_ELECTRIKE, 7}, {SPECIES_ABRA, 8},
    {SPECIES_PIKACHU, 9}, {SPECIES_POOCHYENA, 9}, {SPECIES_ZIGZAGOON, 9}, {SPECIES_TAILLOW, 10}
};
static struct RhhModSpeciesTag sDefaultPreferenceBindings[ARRAY_COUNT(sDefaultPreferences)];
static struct RhhModDefinition sPreferenceDefinition = {
    .struct_size = sizeof(sPreferenceDefinition), .abi_version = RHH_MOD_ABI_ECOLOGY,
    .id = "rhh:encounter_preferences", .ecology_tag_count = ARRAY_COUNT(sPreferenceTags),
    .ecology_tags = sPreferenceTags, .species_tags = sDefaultPreferenceBindings
};
static int RegisterPreferences(void)
{
    sPreferenceDefinition.species_tag_count = 0;
    for (u32 i = 0; i < ARRAY_COUNT(sDefaultPreferences); ++i)
    {
        RhhContentKey species = RhhContentKeyForId(RHH_SPECIES, sDefaultPreferences[i].species);
        if (!species) continue;
        sDefaultPreferenceBindings[sPreferenceDefinition.species_tag_count++] = (struct RhhModSpeciesTag){
            sizeof(struct RhhModSpeciesTag), 0, species, RhhContentKeyOf(sPreferenceTags[sDefaultPreferences[i].tag].id)};
    }
    return RhhModsRegister(&sPreferenceDefinition);
}
int ModsRegisterBuiltins(void)
{
    if (RhhModsSetSaveProjection(ProjectRunSave)
        || RhhModsSetContent(sGameContent, ARRAY_COUNT(sGameContent)) || RhhModsRegister(&sRun)
        || RhhModsInstallMonService() || RhhModsInstallWorldService() || RhhModsInstallEcologyService() || RhhModsInstallPhotoRewardService()
        || RhhModsInstallUiService() || RhhModsInstallBattleService() || RhhModsInstallArchiveService()
        || RhhModsSetLifecycleCallback(ModsWildScriptCleanupDiscard)
        || RhhModsAddService("rhh:service/content_display", &sContentDisplayService.header)
        || RhhModsAddService("rhh:service/world_clock", &sWorldClock.header)
        || RhhModsAddService("rhh:service/world_time", &sWorldTime.header)
        || RhhModsAddService("rhh:service/wild_encounter", &sWildService.header)
        || RhhModsInstallEnvironmentService() || NativeMusicInstallService() || RegisterPreferences()) return -1;
    RhhModsSetContentMetadata(ContentMetadata); RhhModsSetPersistence(Persist);
    sStarterCount = 0;
    for (u32 i = 1; i < NUM_SPECIES; ++i)
    { sSpeciesKeys[i] = RhhContentKeyForId(RHH_SPECIES, i); if (ModsStarterEligible(i) && sSpeciesKeys[i]) sStarters[sStarterCount++] = i; }
    qsort(sStarters, sStarterCount, sizeof(*sStarters), CompareSpecies);
    const struct RhhModDefinition *(*const factories[])(const struct RhhModHostV1 *) = {
        RhhBundledNuzlocke, RhhBundledWildRandomizer, RhhBundledTrainerRandomizer, RhhBundledStarter,
        RhhBundledEqualBST, RhhBundledLevelCap, RhhBundledFollower, RhhBundledBattleMusic,
        RhhBundledBattleEnvironment
    };
    static const char *const names[] = {"nuzlocke", "wild_randomizer", "trainer_randomizer", "starter",
        "equal_bst", "level_cap", "follower", "battle_music", "battle_environment"};
    for (u32 i = 0; i < ARRAY_COUNT(factories); ++i)
    {
        if (!sExternalDirectory)
        { if (RhhModsRegister(factories[i](RhhModsHostAPI()))) return -1; }
        else
        {
#ifdef _WIN32
            const char *suffix = ".dll";
#else
            const char *suffix = ".so";
#endif
            char *path = malloc(strlen(sExternalDirectory) + strlen(names[i]) + strlen(suffix) + 2);
            if (!path) return -1;
            sprintf(path, "%s/%s%s", sExternalDirectory, names[i], suffix);
            int result = RhhModsLoadLibrary(path); free(path); if (result) return -1;
        }
    }
    return 0;
}
void ModsNewGame(u32 trainerId)
{
    RhhModsResetNaturalWeatherObservation();
    ModsBattleReset();
    ModsOwnedTradeReset();
    NativeSaveResetArchive();
    ModsWildActorBattleReset();
    ModsWildActorsDiscardLive();
    RhhModsResetState(); u8 *s = Run(); if (!s) return;
    u64 seed = gModDraft.seed ? gModDraft.seed : (UINT64_C(0x5248480000000000) | trainerId);
    Write64(s, seed); Write64(s + 8, RhhModRandomAt(seed, sRunId, trainerId, NativeSaveNewRunSerial()));
    RhhModsSetRunContext(seed, Read64(s + 8), 0); sHadBalls = FALSE;
    ModsPrepareLearnsets();
    Configure("rhh:nuzlocke", "enabled", RHH_SETTING_BOOL, !!(gModDraft.flags & MOD_NUZLOCKE));
    Configure("rhh:trainer_randomizer", "enabled", RHH_SETTING_BOOL, !!(gModDraft.flags & MOD_TRAINERS));
    Configure("rhh:equal_bst", "enabled", RHH_SETTING_BOOL, !!(gModDraft.flags & MOD_EQUALIZE));
    Configure("rhh:level_cap", "enabled", RHH_SETTING_BOOL, !!(gModDraft.flags & MOD_LEVEL_CAP));
    Configure("rhh:follower", "enabled", RHH_SETTING_BOOL, !!(gModDraft.flags & MOD_FOLLOWER));
    Configure("rhh:wild_randomizer", "mode", RHH_SETTING_CHOICE, gModDraft.wildMode);
    Configure("rhh:battle_music", "style", RHH_SETTING_CHOICE, gModDraft.music);
    Configure("rhh:starter", "species", RHH_SETTING_CONTENT, gModDraft.starter);
    struct RhhModEvent e = Event(RHH_RUN_STARTED, Read64(s + 8), 0, 0);
    struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d);
}
static u32 ResolveSpecies(u64 key, u32 fallback, bool32 starter)
{
    u32 species;
    return !RhhContentResolve(RHH_SPECIES, key, &species) && (starter ? ModsStarterEligible(species) : Eligible(species)) ? species : fallback;
}
static u64 MapKey(void)
{ return gSaveBlock1Ptr ? RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8)
                                                   | (u8)gSaveBlock1Ptr->location.mapNum) : 0; }
void ModsWorldStep(s32 x, s32 y, s32 elevation, u32 behavior)
{
    u8 *run = Run();
    if (!run || !gSaveBlock1Ptr || gMain.inBattle || !RhhModsHasActiveRun() || sAdvancingWorldStep) return;
    u64 step = Read64(run + 24);
    if (step == UINT64_MAX) return;
    sAdvancingWorldStep = TRUE;
    Write64(run + 24, ++step);
    RhhModsEnvironmentOnStep(step);
    struct RhhWorldStepSnapshot context = {.struct_size = sizeof(context), .run_id = Read64(run + 8),
        .step = step, .map = MapKey(), .x = x, .y = y, .elevation = elevation, .behavior = behavior};
    struct RhhModEvent event = Event(RHH_WORLD_STEP, context.map, 0, step);
    struct RhhModDecision decision = Decision(0);
    sWorldStepContext = &context; RhhModsDispatch(&event, &decision); sWorldStepContext = NULL;
    RhhModsWildBehaviorOnStep(&context);
    sAdvancingWorldStep = FALSE;
}
u32 ModsWildSpecies(u32 species, u32 kind, u32 slot, u32 time)
{
    if (!Run() || species >= NUM_SPECIES) return species;
    struct RhhModEvent e = Event(RHH_WILD_SPECIES, MapKey(), sSpeciesKeys[species], RunValue(16));
    e.args[0] = kind | (time << 16); e.args[1] = slot;
    struct RhhModDecision d = Decision(sSpeciesKeys[species]); RhhModsDispatch(&e, &d);
    return ResolveSpecies(d.value, species, FALSE);
}
u32 ModsWildEncounterRate(u32 tableRate, u32 baseRate, u32 area, u32 source, u32 time,
                          s32 x, s32 y, bool32 ignoreAbility)
{
    if (!RhhModsHasActiveRun() || !RhhModsHasHook(RHH_WILD_ENCOUNTER_RATE) || sWildRateContext
        || baseRate > RHH_WILD_RATE_MAX || area > WILD_AREA_ROCKS
        || (source != RHH_WILD_STANDARD && source != RHH_WILD_PIKE && source != RHH_WILD_PYRAMID)) return baseRate;
    struct WildProposalGuard guard = WildProposalBegin();
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    struct RhhWildRateContext context = {.struct_size = sizeof(context), .area = area + RHH_WILD_LAND,
        .source = source, .table_time = time < TIMES_OF_DAY_COUNT ? time + RHH_WILD_TIME_MORNING : RHH_WILD_TIME_UNKNOWN,
        .table_rate = tableRate, .base_rate = baseRate, .ignore_ability = !!ignoreAbility,
        .x = x, .y = y, .map = MapKey(), .step = RunValue(24), .habitat.struct_size = sizeof(context.habitat)};
    const struct RhhHabitatService *habitat = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/habitat", RHH_SERVICE_VERSION(1, 0));
    context.habitat_available = habitat && !habitat->sample(x, y, &context.habitat);
    if (!context.habitat_available) memset(&context.habitat, 0, sizeof(context.habitat));
    struct RhhModEvent event = Event(RHH_WILD_ENCOUNTER_RATE, context.map, 0, context.step);
    event.args[0] = context.area; event.args[1] = source; event.args[2] = baseRate; event.args[3] = context.table_time;
    struct RhhModDecision decision = Decision(baseRate);
    sWildRateContext = &context; RhhModsDispatch(&event, &decision); sWildRateContext = NULL;
    RhhModsWorldTimeScopeEnd(&timeScope);
    bool32 used = WildProposalEnd(&guard);
    return !used && decision.allowed && decision.value <= RHH_WILD_RATE_MAX ? decision.value : baseRate;
}
u32 ModsFishingBiteRate(u32 rod, bool32 sticky, u32 phase, u32 basePercent)
{
    if (!RhhModsHasActiveRun() || !RhhModsHasHook(RHH_FISHING_BITE_RATE) || sFishingBiteContext
        || RhhModsCurrentEvent() || NativeSaveTransactionActive() || rod > 2 || basePercent > 100
        || (phase != RHH_FISHING_BITE_REGULAR && phase != RHH_FISHING_BITE_STICKY)) return basePercent;
    struct WildProposalGuard guard = WildProposalBegin();
    s16 x, y; GetXYCoordsOneStepInFrontOfPlayer(&x, &y);
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    struct RhhFishingBiteContext context = {.struct_size = sizeof(context), .phase = phase,
        .rod = RHH_WILD_ROD_OLD + rod, .base_percent = basePercent, .sticky_ability = !!sticky,
        .x = x - MAP_OFFSET, .y = y - MAP_OFFSET, .map = MapKey(), .step = RunValue(24),
        .habitat.struct_size = sizeof(context.habitat)};
    const struct RhhHabitatService *habitat = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/habitat", RHH_SERVICE_VERSION(1, 0));
    context.habitat_available = habitat && !habitat->sample(context.x, context.y, &context.habitat);
    if (!context.habitat_available) memset(&context.habitat, 0, sizeof(context.habitat));
    struct RhhModEvent event = Event(RHH_FISHING_BITE_RATE, context.map, 0, context.step);
    event.args[0] = context.rod; event.args[1] = phase; event.args[2] = basePercent; event.args[3] = !!sticky;
    struct RhhModDecision decision = Decision(basePercent);
    sFishingBiteContext = &context; RhhModsDispatch(&event, &decision); sFishingBiteContext = NULL;
    RhhModsWorldTimeScopeEnd(&timeScope);
    bool32 used = WildProposalEnd(&guard);
    return !used && decision.allowed && decision.value <= 100 ? decision.value : basePercent;
}
u32 ModsWildSpawnInterval(u32 baseFrames, u32 cause, u32 area, u32 activeCount, u32 capacity,
                          u32 flags, s32 x, s32 y)
{
    if (!RhhModsHasActiveRun() || !RhhModsHasHook(RHH_WILD_SPAWN_INTERVAL) || sWildSpawnIntervalContext
        || NativeSaveTransactionActive()
        || baseFrames > RHH_WILD_SPAWN_MAX_FRAMES || (area != WILD_AREA_LAND && area != WILD_AREA_WATER)
        || (cause != RHH_WILD_SPAWN_MINIMUM && cause != RHH_WILD_SPAWN_RECALCULATED) || !capacity
        || (flags & ~(RHH_WILD_SPAWN_LURE | RHH_WILD_SPAWN_REPLACEMENT))) return baseFrames;
    struct WildProposalGuard guard = WildProposalBegin();
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    struct RhhWildSpawnIntervalContext context = {.struct_size = sizeof(context), .cause = cause,
        .area = area + RHH_WILD_LAND, .base_frames = baseFrames, .active_count = activeCount,
        .capacity = capacity, .flags = flags, .x = x, .y = y, .map = MapKey(), .step = RunValue(24),
        .habitat.struct_size = sizeof(context.habitat)};
    const struct RhhHabitatService *habitat = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/habitat", RHH_SERVICE_VERSION(1, 0));
    context.habitat_available = habitat && !habitat->sample(x, y, &context.habitat);
    if (!context.habitat_available) memset(&context.habitat, 0, sizeof(context.habitat));
    struct RhhModEvent event = Event(RHH_WILD_SPAWN_INTERVAL, context.map, 0, context.step);
    event.args[0] = context.area; event.args[1] = cause; event.args[2] = baseFrames; event.args[3] = flags;
    struct RhhModDecision decision = Decision(baseFrames);
    sWildSpawnIntervalContext = &context; RhhModsDispatch(&event, &decision); sWildSpawnIntervalContext = NULL;
    RhhModsWorldTimeScopeEnd(&timeScope);
    bool32 used = WildProposalEnd(&guard);
    return !used && decision.allowed && decision.value <= RHH_WILD_SPAWN_MAX_FRAMES ? decision.value : baseFrames;
}
static RhhContentKey PreferenceWeatherTag(const struct RhhHabitatSample *sample)
{
    struct RhhHabitatWeather weather = {.struct_size = sizeof(weather)};
    if (RhhModsResolveHabitatWeather(sample, &weather)) return 0;
    switch (weather.kind)
    {
    case RHH_HABITAT_WEATHER_RAIN: case RHH_HABITAT_WEATHER_HEAVY_RAIN:
    case RHH_HABITAT_WEATHER_THUNDERSTORM: return RhhContentKeyOf("rhh:preference/rain");
    case RHH_HABITAT_WEATHER_SUN: case RHH_HABITAT_WEATHER_HARSH_SUN: return RhhContentKeyOf("rhh:preference/sun");
    case RHH_HABITAT_WEATHER_SAND: return RhhContentKeyOf("rhh:preference/sand");
    case RHH_HABITAT_WEATHER_SNOW: case RHH_HABITAT_WEATHER_HAIL: return RhhContentKeyOf("rhh:preference/snow");
    case RHH_HABITAT_WEATHER_FOG: return RhhContentKeyOf("rhh:preference/fog");
    case RHH_HABITAT_WEATHER_WIND: return RhhContentKeyOf("rhh:preference/wind");
    default: return 0;
    }
}
static RhhContentKey PreferenceTerrainTag(const struct RhhHabitatSample *sample)
{
    if (!sample->environment_available) return 0;
    switch (sample->environment.terrain.value)
    {
    case RHH_TERRAIN_GRASSY: return RhhContentKeyOf("rhh:preference/grassy");
    case RHH_TERRAIN_MISTY: return RhhContentKeyOf("rhh:preference/misty");
    case RHH_TERRAIN_ELECTRIC: return RhhContentKeyOf("rhh:preference/electric");
    case RHH_TERRAIN_PSYCHIC: return RhhContentKeyOf("rhh:preference/psychic");
    default: return 0;
    }
}
static bool32 PreferenceRipeBerries(void)
{
    const struct RhhBerryPlotService *plots = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/berry_plot", RHH_SERVICE_VERSION(1, 0));
    u32 count;
    if (!plots || plots->count(&count)) return FALSE;
    for (u32 i = 0; i < count; ++i)
    {
        struct RhhBerryPlot plot = {.struct_size = sizeof(plot)};
        if (!plots->at(i, &plot) && plot.stage == RHH_BERRY_FRUIT && plot.yield) return TRUE;
    }
    return FALSE;
}
bool32 ModsWildWeights(const struct WildPokemon *table, const struct WildPokemon *candidates,
                       u32 area, u32 source, u32 rod, u32 time, s32 x, s32 y,
                       u32 first, u32 count, const u16 *base, u16 *weights)
{
    if (!RhhModsHasActiveRun() || sWildWeightContext
        || !table || !candidates || !base || !weights || !count || count > 12
        || area > WILD_AREA_HIDDEN || source < RHH_WILD_STANDARD || source > RHH_WILD_FISHING_ROD
        || rod > RHH_WILD_ROD_SUPER || first > 12 - count) return FALSE;
    u64 ecologyNow = RhhEcologySystemTime();
    RhhEcologyResumeClock(ecologyNow);
    struct WildProposalGuard outer = WildProposalBegin();
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    struct RhhWildWeightContext context = {.struct_size = sizeof(context), .area = area + RHH_WILD_LAND,
        .source = source, .rod = rod, .table_time = time < TIMES_OF_DAY_COUNT ? time + RHH_WILD_TIME_MORNING : RHH_WILD_TIME_UNKNOWN,
        .x = x, .y = y, .map = MapKey(), .ordinal = RunValue(16), .first_slot = first, .slot_count = count};
    const struct RhhHabitatService *habitat = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/habitat", RHH_SERVICE_VERSION(1, 0));
    const struct RhhEcologyService *ecology = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/ecology", RHH_SERVICE_VERSION(1, 0));
    context.habitat.struct_size = sizeof(context.habitat);
    context.habitat_available = habitat && !habitat->sample(x, y, &context.habitat);
    if (!context.habitat_available) memset(&context.habitat, 0, sizeof(context.habitat));
    const struct RhhEcologyTagsService *tags = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/ecology_tags", RHH_SERVICE_VERSION(1, 0));
    RhhContentKey preferences[3] = {0};
    if (context.habitat_available)
    {
        preferences[0] = PreferenceWeatherTag(&context.habitat);
        preferences[1] = PreferenceTerrainTag(&context.habitat);
    }
    if (PreferenceRipeBerries()) preferences[2] = RhhContentKeyOf("rhh:preference/berries");
    int64_t configuredBonus; u32 preferenceBonus = 2500;
    if (ecology && !ecology->get(context.map, RHH_ECOLOGY_CONDITION,
                                 RhhContentKeyOf("rhh:ecology/preference_bonus"), &configuredBonus)
        && configuredBonus >= 0 && configuredBonus <= 10000) preferenceBonus = configuredBonus;
    bool32 changed = FALSE, adjusted = FALSE; u32 total = 0, frequencies[12], bonuses[12];
    for (u32 i = 0; i < count; ++i)
    {
        u32 slot = first + i, original = table[slot].species, candidate = candidates[slot].species;
        context.slot = slot;
        context.min_level = candidates[slot].minLevel; context.max_level = candidates[slot].maxLevel;
        context.table_species = original < NUM_SPECIES ? sSpeciesKeys[original] : 0;
        context.candidate_species = candidate < NUM_SPECIES ? sSpeciesKeys[candidate] : 0;
        int64_t population;
        u32 effective = base[i];
        if (ecology && context.candidate_species
            && !ecology->get(context.map, RHH_ECOLOGY_POPULATION, context.candidate_species, &population))
            effective = base[i] && (uint64_t)population > RHH_WILD_MAX_WEIGHT / base[i]
                ? RHH_WILD_MAX_WEIGHT : base[i] * (u32)population;
        frequencies[i] = context.candidate_species
            ? RhhEcologyFrequencyAt(context.map, context.candidate_species, ecologyNow) : 10000;
        bonuses[i] = 10000;
        if (tags && context.candidate_species && preferenceBonus)
            for (u32 p = 0; p < ARRAY_COUNT(preferences); ++p)
            {
                u32 has = 0;
                if (preferences[p] && !tags->species_has(context.candidate_species, preferences[p], &has) && has)
                    bonuses[i] += preferenceBonus;
            }
        adjusted |= frequencies[i] != 10000 || bonuses[i] != 10000;
        context.base_weight = effective;
        struct RhhModEvent event = Event(RHH_WILD_WEIGHT, context.map, context.candidate_species, context.ordinal);
        event.args[0] = context.area; event.args[1] = source; event.args[2] = slot; event.args[3] = rod;
        struct RhhModDecision decision = Decision(effective);
        struct WildProposalGuard guard = WildProposalBegin();
        sWildWeightContext = &context; RhhModsDispatch(&event, &decision); sWildWeightContext = NULL;
        bool32 used = WildProposalEnd(&guard);
        weights[i] = !decision.allowed ? 0 : !used && decision.value <= RHH_WILD_MAX_WEIGHT ? decision.value : effective;
        changed |= weights[i] != base[i]; total += weights[i];
    }
    if (adjusted && total)
    {
        /* Preserve precision without changing the mod weight proposal. The
         * shared divisor keeps every result and their sum within chooser bounds. */
        u64 raw[12], max = 0, sum = 0;
        for (u32 i = 0; i < count; ++i)
        {
            raw[i] = (u64)weights[i] * frequencies[i] * bonuses[i];
            if (raw[i] > max) max = raw[i];
            sum += raw[i];
        }
        u64 divisor = 100000000, scale = 100;
        if (max && scale > (u64)RHH_WILD_MAX_WEIGHT * divisor / max)
            scale = (u64)RHH_WILD_MAX_WEIGHT * divisor / max;
        if (sum && scale > (u64)(UINT16_MAX - count) * divisor / sum)
            scale = (u64)(UINT16_MAX - count) * divisor / sum;
        if (!scale)
        {
            scale = 1;
            if (divisor < (max + RHH_WILD_MAX_WEIGHT - 1) / RHH_WILD_MAX_WEIGHT)
                divisor = (max + RHH_WILD_MAX_WEIGHT - 1) / RHH_WILD_MAX_WEIGHT;
            if (divisor < (sum + UINT16_MAX - count - 1) / (UINT16_MAX - count))
                divisor = (sum + UINT16_MAX - count - 1) / (UINT16_MAX - count);
        }
        total = 0;
        for (u32 i = 0; i < count; ++i)
        {
            weights[i] = (raw[i] * scale + divisor / 2) / divisor;
            total += weights[i];
        }
        changed = TRUE;
    }
    RhhModsWorldTimeScopeEnd(&timeScope);
    WildProposalEnd(&outer);
    /* Do not cancel an encounter or alter its RNG if every slot was rejected. */
    return changed && total > 0 && total <= UINT16_MAX;
}
u32 ModsWildCandidate(u32 tableSpecies, u32 species, u32 kind, u32 source, u32 slot,
                      u32 level, u32 time, s32 x, s32 y, struct RhhWildEncounterContext *context)
{
    if (!context) return species;
    memset(context, 0, sizeof(*context));
    if (!RhhModsHasActiveRun() || sWildContext || species >= NUM_SPECIES || tableSpecies >= NUM_SPECIES
        || kind > WILD_AREA_HIDDEN || source < RHH_WILD_STANDARD || source > RHH_WILD_FEEBAS) return species;
    struct WildProposalGuard guard = WildProposalBegin();
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    *context = (struct RhhWildEncounterContext){.struct_size = sizeof(*context), .phase = RHH_WILD_PHASE_PROPOSAL,
        .area = kind + RHH_WILD_LAND, .source = source, .slot = slot, .level = level,
        .x = x, .y = y,
        .table_time = time < TIMES_OF_DAY_COUNT ? time + RHH_WILD_TIME_MORNING : RHH_WILD_TIME_UNKNOWN,
        .map = MapKey(), .table_species = sSpeciesKeys[tableSpecies], .initial_species = sSpeciesKeys[species],
        .species = sSpeciesKeys[species], .ordinal = RunValue(16)};
    const struct RhhHabitatService *habitat = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/habitat", RHH_SERVICE_VERSION(1, 0));
    context->habitat.struct_size = sizeof(context->habitat);
    context->habitat_available = habitat && !habitat->sample(x, y, &context->habitat);
    if (!context->habitat_available) memset(&context->habitat, 0, sizeof(context->habitat));
    struct RhhModEvent event = Event(RHH_WILD_CANDIDATE, context->map, context->initial_species, context->ordinal);
    event.args[0] = context->area; event.args[1] = source; event.args[2] = slot; event.args[3] = level;
    struct RhhModDecision decision = Decision(context->initial_species);
    sWildContext = context; RhhModsDispatch(&event, &decision); sWildContext = NULL;
    RhhModsWorldTimeScopeEnd(&timeScope);
    bool32 used = WildProposalEnd(&guard);
    u32 chosen = !used && decision.allowed ? ResolveSpecies(decision.value, species, FALSE) : species;
    context->species = sSpeciesKeys[chosen];
    return chosen;
}
static bool32 WildPermissionKind(struct RhhWildEncounterContext *context, struct RhhWildEncounterCancellation *cancel, u32 kind)
{
    if (!RhhModsHasHook(kind)) return TRUE;
    struct RhhModEvent event = Event(kind, context->map, context->species, context->ordinal);
    event.args[0] = context->area; event.args[1] = context->source;
    event.args[2] = context->slot; event.args[3] = context->level;
    struct RhhModDecision decision = Decision(context->species);
    sWildContext = context; RhhModsDispatch(&event, &decision); sWildContext = NULL;
    if (decision.allowed) return TRUE;
    *cancel = (struct RhhWildEncounterCancellation){.struct_size = sizeof(*cancel), .candidate = *context};
    cancel->candidate.phase = RHH_WILD_PHASE_CANCELLED;
    memcpy(cancel->reason, decision.reason, sizeof(cancel->reason));
    return FALSE;
}
static bool32 WildPermission(struct RhhWildEncounterContext *context, struct RhhWildEncounterCancellation *cancel)
{
    u32 kind = context->source == RHH_WILD_SCRIPTED ? RHH_CAN_BEGIN_SCRIPTED_WILD
        : context->source >= RHH_WILD_MANUAL_PIKE ? RHH_CAN_GENERATE_MANUAL_FACILITY_WILD
        : context->source >= RHH_WILD_PIKE ? RHH_CAN_GENERATE_FACILITY_WILD
        : context->source == RHH_WILD_DEXNAV ? RHH_CAN_GENERATE_DEXNAV_WILD
        : context->source >= RHH_WILD_ROAMER ? RHH_CAN_GENERATE_SPECIAL_WILD : RHH_CAN_GENERATE_WILD;
    return WildPermissionKind(context, cancel, kind);
}
void ModsWildCheckpointSave(struct RhhWildCheckpoint *checkpoint)
{
    memcpy(checkpoint->enemy, gParties[B_TRAINER_OPPONENT_A], sizeof(checkpoint->enemy));
    checkpoint->enemyCount = gPartiesCount[B_TRAINER_OPPONENT_A]; checkpoint->roamerIndex = gEncounteredRoamerIndex;
    checkpoint->rng = gRngValue; checkpoint->rng2 = gRng2Value;
}
void ModsWildCheckpointRestore(const struct RhhWildCheckpoint *checkpoint)
{
    memcpy(gParties[B_TRAINER_OPPONENT_A], checkpoint->enemy, sizeof(checkpoint->enemy));
    gPartiesCount[B_TRAINER_OPPONENT_A] = checkpoint->enemyCount; gEncounteredRoamerIndex = checkpoint->roamerIndex;
    gRngValue = checkpoint->rng; gRng2Value = checkpoint->rng2;
}
bool32 ModsWildSpecialPermission(u32 species, u32 kind, u32 source, u32 slot, u32 level,
                                 u32 time, s32 x, s32 y, u64 identity,
                                 struct RhhWildEncounterContext *context,
                                 struct RhhWildEncounterCancellation *cancel)
{
    if (!context || !cancel) return FALSE;
    memset(context, 0, sizeof(*context)); memset(cancel, 0, sizeof(*cancel));
    if (!RhhModsHasActiveRun() || sWildContext || kind > WILD_AREA_HIDDEN || species >= NUM_SPECIES
        || !sSpeciesKeys[species] || source < RHH_WILD_ROAMER || source == RHH_WILD_SCRIPTED
        || source > RHH_WILD_MANUAL_PYRAMID
        || (source >= RHH_WILD_MANUAL_PIKE
            ? (!RhhModsHasHook(RHH_CAN_GENERATE_MANUAL_FACILITY_WILD) && !RhhModsHasHook(RHH_WILD_MANUAL_FACILITY_GENERATED))
            : source >= RHH_WILD_PIKE
            ? (!RhhModsHasHook(RHH_CAN_GENERATE_FACILITY_WILD) && !RhhModsHasHook(RHH_WILD_FACILITY_GENERATED))
            : source == RHH_WILD_DEXNAV
            ? (!RhhModsHasHook(RHH_CAN_GENERATE_DEXNAV_WILD) && !RhhModsHasHook(RHH_WILD_DEXNAV_GENERATED))
            : (!RhhModsHasHook(RHH_CAN_GENERATE_SPECIAL_WILD) && !RhhModsHasHook(RHH_WILD_SPECIAL_GENERATED)))) return TRUE;
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    *context = (struct RhhWildEncounterContext){.struct_size = sizeof(*context), .phase = RHH_WILD_PHASE_PROPOSAL,
        .area = kind + RHH_WILD_LAND, .source = source, .slot = slot, .level = level,
        .table_time = time < TIMES_OF_DAY_COUNT ? time + RHH_WILD_TIME_MORNING : RHH_WILD_TIME_UNKNOWN,
        .x = x, .y = y, .map = MapKey(), .table_species = sSpeciesKeys[species],
        .initial_species = sSpeciesKeys[species], .species = sSpeciesKeys[species], .ordinal = RunValue(16), .identity = identity};
    const struct RhhHabitatService *habitat = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/habitat", RHH_SERVICE_VERSION(1, 0));
    context->habitat.struct_size = sizeof(context->habitat);
    context->habitat_available = habitat && !habitat->sample(x, y, &context->habitat);
    if (!context->habitat_available) memset(&context->habitat, 0, sizeof(context->habitat));
    bool32 allowed = WildPermission(context, cancel);
    RhhModsWorldTimeScopeEnd(&timeScope); return allowed;
}
void ModsWildSpecialCreated(struct RhhWildEncounterContext *context, struct Pokemon *mon)
{
    if (!context || context->phase != RHH_WILD_PHASE_PROPOSAL || !mon || !RhhModsHasActiveRun() || sWildContext) return;
    u32 species = GetMonData(mon, MON_DATA_SPECIES);
    if (!species || species >= NUM_SPECIES || !sSpeciesKeys[species] || sSpeciesKeys[species] != context->species) return;
    context->phase = RHH_WILD_PHASE_CREATED;
    context->identity = (u64)GetMonData(mon, MON_DATA_OT_ID) << 32 | GetMonData(mon, MON_DATA_PERSONALITY);
    u32 kind = context->source >= RHH_WILD_MANUAL_PIKE ? RHH_WILD_MANUAL_FACILITY_GENERATED
        : context->source >= RHH_WILD_PIKE ? RHH_WILD_FACILITY_GENERATED
        : context->source == RHH_WILD_DEXNAV ? RHH_WILD_DEXNAV_GENERATED : RHH_WILD_SPECIAL_GENERATED;
    struct RhhModEvent event = Event(kind, context->map, context->species, context->ordinal);
    event.args[0] = context->area; event.args[1] = context->source; event.args[2] = context->slot; event.args[3] = context->level;
    struct RhhModDecision decision = Decision(context->species);
    sWildContext = context; RhhModsDispatch(&event, &decision); sWildContext = NULL;
}
static u32 ScriptedCandidate(u32 species, u32 level, u32 slot, u32 kind)
{
    if (!RhhModsHasActiveRun() || sWildContext || !RhhModsHasHook(kind)
        || !species || species >= NUM_SPECIES || !sSpeciesKeys[species] || slot > 1) return species;
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context), .phase = RHH_WILD_PHASE_PROPOSAL,
        .source = RHH_WILD_SCRIPTED, .slot = slot, .level = level, .x = INT32_MIN, .y = INT32_MIN,
        .map = MapKey(), .initial_species = sSpeciesKeys[species], .species = sSpeciesKeys[species],
        .ordinal = RunValue(16)};
    struct RhhModEvent event = Event(kind, context.map, context.initial_species, context.ordinal);
    event.args[0] = context.area; event.args[1] = context.source; event.args[2] = slot; event.args[3] = level;
    struct RhhModDecision decision = Decision(context.initial_species);
    sWildContext = &context; RhhModsDispatch(&event, &decision); sWildContext = NULL;
    return decision.allowed ? ResolveSpecies(decision.value, species, FALSE) : species;
}
u32 ModsWildScriptedCandidate(u32 species, u32 level, u32 slot)
{ return ScriptedCandidate(species, level, slot, RHH_WILD_SCRIPTED_CANDIDATE); }
u32 ModsWildEventCandidate(u32 species, u32 level)
{ return ScriptedCandidate(species, level, 0, RHH_WILD_EVENT_CANDIDATE); }
static void WildScriptedReady(u32 count, u32 kind, u32 source)
{
    if (!RhhModsHasActiveRun() || sWildContext || !RhhModsHasHook(kind)) return;
    if (count > PARTY_SIZE) count = PARTY_SIZE;
    for (u32 slot = 0; slot < count; ++slot)
    {
        /* Decode a copy: corrupt encrypted data must never mutate the party. */
        struct Pokemon mon = gParties[B_TRAINER_OPPONENT_A][slot];
        if (!GetMonData(&mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(&mon, MON_DATA_SANITY_IS_BAD_EGG)) continue;
        u32 species = GetMonData(&mon, MON_DATA_SPECIES);
        if (!species || species >= NUM_SPECIES || !sSpeciesKeys[species]
            || GetMonData(&mon, MON_DATA_SANITY_IS_BAD_EGG)) continue;
        struct RhhWildEncounterContext context = {.struct_size = sizeof(context), .phase = RHH_WILD_PHASE_CREATED,
            .source = source, .slot = slot, .level = GetMonData(&mon, MON_DATA_LEVEL),
            .x = INT32_MIN, .y = INT32_MIN, .map = MapKey(),
            .initial_species = sSpeciesKeys[species], .species = sSpeciesKeys[species], .ordinal = RunValue(16),
            .identity = (u64)GetMonData(&mon, MON_DATA_OT_ID) << 32 | GetMonData(&mon, MON_DATA_PERSONALITY)};
        struct RhhModEvent event = Event(kind, context.map, context.species, context.ordinal);
        event.args[0] = context.area; event.args[1] = context.source; event.args[2] = slot; event.args[3] = context.level;
        struct RhhModDecision decision = Decision(context.species);
        sWildContext = &context; RhhModsDispatch(&event, &decision); sWildContext = NULL;
    }
}
void ModsWildScriptedReady(u32 count)
{ WildScriptedReady(count, RHH_WILD_SCRIPTED_READY, RHH_WILD_SCRIPTED); }
void ModsWildSpecialScriptedReady(bool32 tutorial)
{ WildScriptedReady(1, tutorial ? RHH_WILD_TUTORIAL_READY : RHH_WILD_GHOST_READY,
                    tutorial ? RHH_WILD_TUTORIAL : RHH_WILD_GHOST); }
void ModsWildFirstBattleReady(void)
{
    struct RhhScriptedEncounterSource source;
    if (!ModsFirstBattleInitializing(&source)) return;
    struct RhhScriptedEncounterSource previous = ModsWildScriptedScopeSwap(source);
    WildScriptedReady(1, RHH_WILD_FIRST_BATTLE_READY, RHH_WILD_FIRST_BATTLE);
    ModsWildScriptedScopeSwap(previous);
}
static bool32 PreparedWildPermission(struct Pokemon *prepared, u32 slot, u32 kind, u32 source,
                                     struct RhhWildEncounterCancellation *cancel)
{
    if (!prepared || !cancel || !RhhModsHasActiveRun() || sWildContext
        || !RhhModsHasHook(kind)) return TRUE;
    /* The permission hook sees the generated opponent but never decodes the
     * live party, since the decoder can mark corrupt data as a bad egg. */
    struct Pokemon mon = *prepared;
    if (!GetMonData(&mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(&mon, MON_DATA_SANITY_IS_BAD_EGG)) return TRUE;
    u32 species = GetMonData(&mon, MON_DATA_SPECIES);
    if (!species || species >= NUM_SPECIES || !sSpeciesKeys[species] || GetMonData(&mon, MON_DATA_SANITY_IS_BAD_EGG)) return TRUE;
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context), .phase = RHH_WILD_PHASE_PROPOSAL,
        .source = source, .slot = slot, .level = GetMonData(&mon, MON_DATA_LEVEL),
        .x = INT32_MIN, .y = INT32_MIN, .map = MapKey(), .initial_species = sSpeciesKeys[species],
        .species = sSpeciesKeys[species], .ordinal = RunValue(16),
        .identity = (u64)GetMonData(&mon, MON_DATA_OT_ID) << 32 | GetMonData(&mon, MON_DATA_PERSONALITY)};
    return WildPermissionKind(&context, cancel, kind);
}
bool32 ModsWildScriptedPermission(struct Pokemon *prepared, u32 slot, bool32 eventMon, struct RhhWildEncounterCancellation *cancel)
{ return PreparedWildPermission(prepared, slot, eventMon ? RHH_CAN_BEGIN_EVENT_WILD : RHH_CAN_BEGIN_SCRIPTED_WILD,
                                RHH_WILD_SCRIPTED, cancel); }
bool32 ModsWildTutorialPermission(struct Pokemon *prepared, struct RhhWildEncounterCancellation *cancel)
{ return PreparedWildPermission(prepared, 0, RHH_CAN_BEGIN_TUTORIAL_WILD, RHH_WILD_TUTORIAL, cancel); }
bool32 ModsWildResolve(u32 tableSpecies, u32 *species, u32 kind, u32 source, u32 slot,
                       u32 level, u32 time, s32 x, s32 y, struct RhhWildEncounterContext *context,
                       struct RhhWildEncounterCancellation *cancel)
{
    if (!species || !context || !cancel) return FALSE;
    memset(cancel, 0, sizeof(*cancel));
    /* These facilities replace the provisional table Pokémon after this
     * stage; do not claim the gate applies to the actual facility encounter. */
    if (!RhhModsHasHook(RHH_CAN_GENERATE_WILD)
        || gMapHeader.mapLayoutId == LAYOUT_BATTLE_FRONTIER_BATTLE_PIKE_ROOM_WILD_MONS
        || gMapHeader.mapLayoutId == LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR)
    { *species = ModsWildCandidate(tableSpecies, *species, kind, source, slot, level, time, x, y, context); return TRUE; }
    struct RhhWorldTimeScope timeScope; RhhModsWorldTimeScopeBegin(&timeScope);
    *species = ModsWildCandidate(tableSpecies, *species, kind, source, slot, level, time, x, y, context);
    if (!RhhModsHasActiveRun() || sWildContext || context->phase != RHH_WILD_PHASE_PROPOSAL)
    { RhhModsWorldTimeScopeEnd(&timeScope); return TRUE; }
    bool32 allowed = WildPermission(context, cancel);
    RhhModsWorldTimeScopeEnd(&timeScope);
    return allowed;
}
static void WildCancelledKind(const struct RhhWildEncounterCancellation *cancel, u32 kind)
{
    if (!cancel || cancel->struct_size < sizeof(*cancel) || !RhhModsHasActiveRun() || sWildContext
        || sWildCancellationContext || cancel->candidate.phase != RHH_WILD_PHASE_CANCELLED) return;
    const struct RhhWildEncounterContext *candidate = &cancel->candidate;
    struct RhhModEvent event = Event(kind, candidate->map, candidate->species, candidate->ordinal);
    event.args[0] = candidate->area; event.args[1] = candidate->source;
    event.args[2] = candidate->slot; event.args[3] = candidate->level;
    struct RhhModDecision decision = Decision(candidate->species); decision.allowed = FALSE;
    memcpy(decision.reason, cancel->reason, sizeof(decision.reason));
    sWildContext = candidate; sWildCancellationContext = cancel;
    RhhModsDispatch(&event, &decision);
    sWildCancellationContext = NULL; sWildContext = NULL;
}
void ModsWildCancelled(const struct RhhWildEncounterCancellation *cancel)
{
    if (!cancel || cancel->struct_size < sizeof(*cancel)) return;
    u32 source = cancel->candidate.source;
    u32 kind = source == RHH_WILD_SCRIPTED ? RHH_WILD_SCRIPTED_CANCELLED
        : source >= RHH_WILD_MANUAL_PIKE ? RHH_WILD_MANUAL_FACILITY_CANCELLED
        : source >= RHH_WILD_PIKE ? RHH_WILD_FACILITY_CANCELLED
        : source == RHH_WILD_DEXNAV ? RHH_WILD_DEXNAV_CANCELLED
        : source >= RHH_WILD_ROAMER ? RHH_WILD_SPECIAL_CANCELLED : RHH_WILD_CANCELLED;
    WildCancelledKind(cancel, kind);
}
void ModsWildEventCancelled(const struct RhhWildEncounterCancellation *cancel)
{ WildCancelledKind(cancel, RHH_WILD_EVENT_CANCELLED); }
void ModsWildTutorialCancelled(const struct RhhWildEncounterCancellation *cancel)
{ WildCancelledKind(cancel, RHH_WILD_TUTORIAL_CANCELLED); }
static struct {
    struct RhhWildEncounterCancellation cancel;
    struct RhhScriptedEncounterSource source;
    u64 run;
    u8 mapNum, mapGroup;
    bool8 valid, kind;
} sWildScriptCleanup;
void ModsWildScriptCleanupDiscard(void)
{
    sWildScriptCleanup.valid = FALSE;
}
void ModsWildScriptCleanupQueue(const struct RhhWildEncounterCancellation *cancel, u32 kind)
{
    if (sWildScriptCleanup.valid || !cancel || cancel->struct_size < sizeof(*cancel)
        || cancel->candidate.phase != RHH_WILD_PHASE_CANCELLED || !RhhModsHasActiveRun()
        || !gSaveBlock1Ptr || kind > 2) return;
    sWildScriptCleanup.cancel = *cancel; sWildScriptCleanup.run = RhhModsRunId();
    sWildScriptCleanup.source = sWildScriptedScope;
    sWildScriptCleanup.mapNum = gSaveBlock1Ptr->location.mapNum;
    sWildScriptCleanup.mapGroup = gSaveBlock1Ptr->location.mapGroup;
    sWildScriptCleanup.kind = kind; sWildScriptCleanup.valid = TRUE;
}
void ModsWildScriptCleanupFinish(void)
{
    if (!sWildScriptCleanup.valid) return;
    struct RhhWildEncounterCancellation cancel = sWildScriptCleanup.cancel;
    struct RhhScriptedEncounterSource source = sWildScriptCleanup.source;
    u64 run = sWildScriptCleanup.run;
    u8 mapNum = sWildScriptCleanup.mapNum, mapGroup = sWildScriptCleanup.mapGroup;
    u32 kind = sWildScriptCleanup.kind == 2 ? RHH_WILD_TUTORIAL_CLEANUP_FINISHED
        : sWildScriptCleanup.kind ? RHH_WILD_EVENT_CLEANUP_FINISHED : RHH_WILD_SCRIPTED_CLEANUP_FINISHED;
    sWildScriptCleanup.valid = FALSE;
    if (!RhhModsHasActiveRun() || run != RhhModsRunId() || !gSaveBlock1Ptr
        || mapNum != gSaveBlock1Ptr->location.mapNum || mapGroup != gSaveBlock1Ptr->location.mapGroup) return;
    struct RhhScriptedEncounterSource previous = ModsWildScriptedScopeSwap(source);
    WildCancelledKind(&cancel, kind);
    ModsWildScriptedScopeSwap(previous);
}
void ModsWildCandidateCreated(struct RhhWildEncounterContext *context, struct Pokemon *mon)
{
    if (!context || context->phase != RHH_WILD_PHASE_PROPOSAL || !mon || !RhhModsHasActiveRun() || sWildContext) return;
    u32 species = GetMonData(mon, MON_DATA_SPECIES);
    if (!species || species >= NUM_SPECIES || !sSpeciesKeys[species]) return;
    context->phase = RHH_WILD_PHASE_CREATED; context->species = sSpeciesKeys[species];
    context->level = GetMonData(mon, MON_DATA_LEVEL);
    context->identity = (u64)GetMonData(mon, MON_DATA_OT_ID) << 32 | GetMonData(mon, MON_DATA_PERSONALITY);
    struct RhhModEvent event = Event(RHH_WILD_GENERATED, context->map, context->species, context->ordinal);
    event.args[0] = context->area; event.args[1] = context->source; event.args[2] = context->slot; event.args[3] = context->level;
    struct RhhModDecision decision = Decision(context->species);
    sWildContext = context; RhhModsDispatch(&event, &decision); sWildContext = NULL;
}
void ModsWildCreated(u32 species, u32 kind, u32 slot, u32 time)
{
    u8 *s = Run(); if (!s) return;
    struct RhhModEvent e = Event(RHH_WILD_CREATED, MapKey(), sSpeciesKeys[species], Read64(s + 16));
    e.args[0] = kind | (time << 16); e.args[1] = slot;
    struct RhhModDecision d = Decision(sSpeciesKeys[species]); RhhModsDispatch(&e, &d);
    Write64(s + 16, Read64(s + 16) + 1);
}
u32 ModsTrainerSpecies(u32 species, u32 trainer, u32 slot)
{
    if (!Run() || species >= NUM_SPECIES) return species;
    struct RhhModEvent e = Event(RHH_TRAINER_SPECIES, RhhContentKeyForId(RHH_TRAINER, trainer), sSpeciesKeys[species], slot);
    struct RhhModDecision d = Decision(sSpeciesKeys[species]); RhhModsDispatch(&e, &d); return ResolveSpecies(d.value, species, FALSE);
}
u32 ModsStarterSpecies(u32 species, u32 slot)
{
    if (!Run()) return species;
    struct RhhModEvent e = Event(RHH_STARTER_SPECIES, RhhContentKeyOf("rhh:starter/birch"), sSpeciesKeys[species], slot);
    struct RhhModDecision d = Decision(sSpeciesKeys[species]); RhhModsDispatch(&e, &d); return ResolveSpecies(d.value, species, TRUE);
}
u32 ModsBaseStat(u32 species, u32 index, u32 original)
{
    if (!Run() || species >= NUM_SPECIES || index >= 6 || !gSpeciesInfo[species].baseHP) return original;
    const struct SpeciesInfo *i = &gSpeciesInfo[species];
    struct RhhModEvent e = Event(RHH_BASE_STATS, sSpeciesKeys[species], sSpeciesKeys[species], 0);
    e.args[0] = species == SPECIES_SHEDINJA || (P_BASE_HP_1_SHEDINJA_HANDLING && i->baseHP == 1);
    struct RhhModDecision d = Decision(0);
    const u16 stats[] = {i->baseHP, i->baseAttack, i->baseDefense, i->baseSpeed, i->baseSpAttack, i->baseSpDefense};
    memcpy(d.stats, stats, sizeof(stats)); RhhModsDispatch(&e, &d);
    return d.stats[index] >= 1 && d.stats[index] <= 255 ? d.stats[index] : original;
}
static u32 GymAceLevel(u32 *trainerId)
{
    static const u16 leaders[] = {TRAINER_ROXANNE_1, TRAINER_BRAWLY_1, TRAINER_WATTSON_1, TRAINER_FLANNERY_1,
        TRAINER_NORMAN_1, TRAINER_WINONA_1, TRAINER_TATE_AND_LIZA_1, TRAINER_JUAN_1, TRAINER_WALLACE};
    u32 next = 0; while (next < 8 && FlagGet(FLAG_BADGE01_GET + next)) ++next;
    *trainerId = leaders[next]; const struct Trainer *trainer = GetTrainerStructFromId(*trainerId);
    for (u32 i = 0; trainer->overrideTrainer && i < 8; ++i) trainer = GetTrainerStructFromId(trainer->overrideTrainer);
    u32 level = 1, count = trainer->poolSize ? trainer->poolSize : trainer->partySize;
    for (u32 i = 0; i < count; ++i) if (trainer->party[i].lvl > level) level = trainer->party[i].lvl;
    return level;
}
u32 ModsLevelCap(void)
{
    if (!Run()) return MAX_LEVEL;
    u32 trainer; u32 ace = GymAceLevel(&trainer);
    struct RhhModEvent e = Event(RHH_LEVEL_CAP, RhhContentKeyForId(RHH_TRAINER, trainer), 0, 0); e.args[0] = ace;
    struct RhhModDecision d = Decision(MAX_LEVEL); RhhModsDispatch(&e, &d);
    return d.value < 1 ? 1 : d.value > MAX_LEVEL ? MAX_LEVEL : d.value;
}
bool32 ModsHardCap(void) { return ModsLevelCap() < MAX_LEVEL; }
static bool32 InPlayerParty(const struct Pokemon *mon)
{
    uintptr_t at = (uintptr_t)mon, begin = (uintptr_t)gParties[B_TRAINER_PLAYER];
    return at >= begin && at < begin + sizeof(gParties[B_TRAINER_PLAYER]) && (at - begin) % sizeof(*mon) == 0;
}
u32 ModsClampExperience(struct Pokemon *mon, u32 exp)
{
    if (!InPlayerParty(mon) || !ModsHardCap()) return exp;
    u32 species = GetMonData(mon, MON_DATA_SPECIES); if (!species || species >= NUM_SPECIES) return exp;
    u32 limit = gExperienceTables[gSpeciesInfo[species].growthRate][ModsLevelCap()]; return exp > limit ? limit : exp;
}
static u64 MonKey(struct Pokemon *mon)
{ return (u64)GetMonData(mon, MON_DATA_OT_ID) << 32 | GetMonData(mon, MON_DATA_PERSONALITY); }
bool32 ModsMonIsDead(struct Pokemon *mon)
{
    if (!Run()) return FALSE;
    struct RhhModEvent e = Event(RHH_MON_IS_DEAD, 0, MonKey(mon), 0);
    struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d); return d.value != 0;
}
void ModsNormalizePartyMon(struct Pokemon *mon)
{
    if (sNormalizing || !Run() || !InPlayerParty(mon) || !GetMonData(mon, MON_DATA_SPECIES)) return;
    sNormalizing = TRUE;
    u32 exp = GetMonData(mon, MON_DATA_EXP), clipped = ModsClampExperience(mon, exp);
    if (clipped != exp) SetMonData(mon, MON_DATA_EXP, &clipped);
    if (ModsMonIsDead(mon)) { u16 hp = 0; SetMonData(mon, MON_DATA_HP, &hp); }
    sNormalizing = FALSE;
}
void ModsMonFainted(struct Pokemon *mon)
{
    if (!Run() || !InPlayerParty(mon) || !GetMonData(mon, MON_DATA_SPECIES)) return;
    struct RhhModEvent e = Event(RHH_MON_FAINTED, sSpeciesKeys[GetMonData(mon, MON_DATA_SPECIES)], MonKey(mon), 0);
    e.args[0] = HasAtLeastOnePokeBall(); struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d);
}
u32 ModsFilterHP(struct Pokemon *mon, u32 oldHP, u32 newHP)
{
    if (oldHP && !newHP) ModsMonFainted(mon);
    if (newHP && Run())
    {
        struct RhhModEvent e = Event(RHH_CAN_REVIVE, 0, MonKey(mon), 0);
        e.args[0] = ModsMonIsDead(mon); e.args[1] = oldHP; e.args[2] = newHP; e.args[3] = mon->maxHP;
        struct RhhModDecision d = Decision(newHP); RhhModsDispatch(&e, &d); if (!d.allowed) return 0;
    }
    return newHP;
}
static u64 AreaKey(void) { return RhhContentKeyForId(RHH_AREA, gMapHeader.regionMapSectionId); }
void ModsEncounterStarted(void)
{
    struct RhhModEvent e = Event(RHH_ENCOUNTER_STARTED, AreaKey(), 0, 0);
    e.args[0] = !FlagGet(WE_FLAG_NO_CATCHING); e.args[1] = HasAtLeastOnePokeBall();
    struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d);
}
bool32 ModsCanCapture(void)
{
    u32 species = GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES);
    struct RhhModEvent e = Event(RHH_CAN_CAPTURE, AreaKey(), species < NUM_SPECIES ? sSpeciesKeys[species] : 0, 0);
    struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d); return d.allowed;
}
void ModsCaughtMon(void)
{
    struct RhhModEvent e = Event(RHH_MON_CAUGHT, AreaKey(), 0, 0);
    struct RhhModDecision d = Decision(1); RhhModsDispatch(&e, &d);
}
void ModsWhiteout(void)
{
    struct RhhModEvent e = Event(RHH_WHITEOUT, 0, 0, 0); e.args[0] = HasAtLeastOnePokeBall();
    struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d);
}
bool32 ModsRunFailed(void)
{
    struct RhhModEvent e = Event(RHH_CAN_CONTINUE, 0, 0, 0);
    struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d); return !d.allowed;
}
void ModsAfterLoad(void)
{
    RhhModsResetNaturalWeatherObservation();
    ModsBattleReset();
    ModsWildActorBattleReset();
    ModsOwnedTradeReset();
    RhhModsResetOwnedMonWatch();
    if (!Run()) return;
    RhhModsSetRunContext(RunValue(0), RunValue(8), 1);
    u32 size; const u8 *journal = NativeSaveGetJournal(RunValue(8), &size);
    if (journal && RhhModsMergeJournal(journal, size)) { fprintf(stderr, "Required mod journal rejected: %s\n", RhhModsError()); abort(); }
    ModsBreedingReconcilePendingId();
    ModsPrepareLearnsets();
    ModsWildActorsDiscardLive();
    struct RhhModEvent e = Event(RHH_RUN_LOADED, RunValue(8), 0, 0);
    struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d);
    ModsWildActorsAfterLoad();
    for (u32 i = 0; i < PARTY_SIZE; ++i)
        if (GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_SPECIES))
        { ModsNormalizePartyMon(&gParties[B_TRAINER_PLAYER][i]); CalculateMonStats(&gParties[B_TRAINER_PLAYER][i]); }
    sHadBalls = HasAtLeastOnePokeBall();
}
void ModsAfterSave(void)
{
    if (!Run() || !RunValue(8)) return;
    RhhModsSetRunContext(RunValue(0), RunValue(8), 1);
    if (!RhhModsCanPersistDurably() || RhhModsFlushDurable())
    {
        struct RhhModEvent e = Event(RHH_PERSISTENCE_FAILED, 0, 0, 0);
        struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d);
    }
    else ModsWildActorsAfterSave();
}
void ModsSyncInventory(void)
{
    if (!Run()) return;
    bool32 balls = HasAtLeastOnePokeBall();
    if (balls != sHadBalls)
    {
        sHadBalls = balls; struct RhhModEvent e = Event(RHH_INVENTORY_CHANGED, RhhContentKeyOf("rhh:inventory/pokeballs"), 0, 0);
        e.args[0] = balls; struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d);
    }
}
void ModsEnforceParty(void)
{
    if (!Run()) return;
    ModsSyncInventory();
    for (u32 i = 0; i < PARTY_SIZE; ++i)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i]; if (!GetMonData(mon, MON_DATA_SPECIES)) continue;
        u32 exp = GetMonData(mon, MON_DATA_EXP);
        if (ModsClampExperience(mon, exp) != exp || GetMonData(mon, MON_DATA_LEVEL) > ModsLevelCap())
        { ModsNormalizePartyMon(mon); CalculateMonStats(mon); }
        if (ModsMonIsDead(mon) && GetMonData(mon, MON_DATA_HP)) { u16 hp = 0; SetMonData(mon, MON_DATA_HP, &hp); }
    }
}
