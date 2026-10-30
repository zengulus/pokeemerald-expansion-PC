/* Shared, save-backed music choices. No file paths are stored in the save. */
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "main.h"
#include "mod_api.h"
#include "mod_host.h"
#include "mod_music.h"
#include "native.h"
#include "random.h"

enum { HEADER = 8, RECORD = 40, STATE_SIZE = HEADER + RECORD * RHH_MUSIC_ASSIGNMENT_CAPACITY };
static const char sStateId[] = "rhh:music_assignments";
static bool32 sBusy;
static const struct RhhMusicAssignmentResult *sContext;
static u32 Read32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u64 Read64(const u8 *p) { u64 n = 0; for (u32 i = 0; i < 8; ++i) n |= (u64)p[i] << (8 * i); return n; }
static void Write32(u8 *p, u32 n) { for (u32 i = 0; i < 4; ++i) p[i] = n >> (8 * i); }
static void Write64(u8 *p, u64 n) { for (u32 i = 0; i < 8; ++i) p[i] = n >> (8 * i); }
static bool32 ValidContext(u32 c) { return c <= RHH_MUSIC_CHAMPION || c == RHH_MUSIC_ANY; }
static void ReadRule(const u8 *p, struct RhhMusicAssignment *out)
{
    *out = (struct RhhMusicAssignment){.struct_size = sizeof(*out), .context = Read32(p),
        .original = Read64(p + 8), .map = Read64(p + 16), .trainer = Read64(p + 24), .track = Read64(p + 32)};
}
static void WriteRule(u8 *p, const struct RhhMusicAssignment *rule)
{
    memset(p, 0, RECORD); Write32(p, rule->context); Write64(p + 8, rule->original);
    Write64(p + 16, rule->map); Write64(p + 24, rule->trainer); Write64(p + 32, rule->track);
}
static u32 Count(const u8 *state)
{
    u32 n = 0;
    while (n < RHH_MUSIC_ASSIGNMENT_CAPACITY && Read64(state + HEADER + n * RECORD + 32)) ++n;
    return n;
}
static int Validate(const u8 *state, u32 size)
{
    if (!state || size != STATE_SIZE) return -1;
    bool32 empty = FALSE;
    for (u32 i = 0; i < RHH_MUSIC_ASSIGNMENT_CAPACITY; ++i)
    {
        const u8 *p = state + HEADER + i * RECORD;
        if (!Read64(p + 32))
        {
            empty = TRUE;
            for (u32 j = 0; j < RECORD; ++j) if (p[j]) return -1;
            continue;
        }
        if (empty || !Read64(state) || !ValidContext(Read32(p)) || Read32(p + 4)) return -1;
        for (u32 j = 0; j < i; ++j)
            if (!memcmp(state + HEADER + j * RECORD, p, 32)) return -1;
    }
    return 0;
}
static const struct RhhModDefinition sState = {.struct_size = sizeof(sState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sStateId, .flags = RHH_MOD_REQUIRED,
    .state_size = STATE_SIZE, .validate = Validate};
static bool32 Canonical(u32 kind, RhhContentKey *key)
{
    u32 id;
    if (!*key) return TRUE;
    if (RhhContentResolve(kind, *key, &id)) return FALSE;
    *key = RhhContentKeyForId(kind, id);
    return *key != 0;
}
static bool32 KnownTrack(RhhContentKey key)
{
    u32 id;
    return key && (RhhModsFindMusic(key) || NativeMusicImportedKnown(key)
        || (!RhhContentResolve(RHH_MIDI, key, &id) && id >= 1 && id <= 20));
}
static uint64_t Revision(void)
{
    const u8 *state = RhhModsState(sStateId);
    return state && RhhModsHasActiveRun() ? Read64(state) : 0;
}
static uint32_t RuleCount(void)
{
    const u8 *state = RhhModsState(sStateId);
    return state && RhhModsHasActiveRun() ? Count(state) : 0;
}
static int RuleAt(u32 index, struct RhhMusicAssignment *out)
{
    const u8 *state = RhhModsState(sStateId);
    if (!state || !RhhModsHasActiveRun() || !out || out->struct_size < sizeof(*out) || index >= Count(state)) return -1;
    ReadRule(state + HEADER + index * RECORD, out); return 0;
}
static int Resolve(const struct RhhMusicSituation *situation, struct RhhMusicAssignment *out)
{
    const u8 *state = RhhModsState(sStateId);
    if (!state || !RhhModsHasActiveRun() || !situation || situation->struct_size < sizeof(*situation)
        || situation->context > RHH_MUSIC_CHAMPION || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhMusicSituation s = *situation;
    if (!Canonical(RHH_MUSIC, &s.original) || !Canonical(RHH_MAP, &s.map)
        || !Canonical(RHH_TRAINER, &s.trainers[0]) || !Canonical(RHH_TRAINER, &s.trainers[1])) return -1;
    struct RhhMusicAssignment best = {.struct_size = sizeof(best)};
    int bestRank = -1, bestTrainer = 2;
    for (u32 i = 0; i < Count(state); ++i)
    {
        struct RhhMusicAssignment r; ReadRule(state + HEADER + i * RECORD, &r);
        if ((r.context != RHH_MUSIC_ANY && r.context != s.context) || (r.original && r.original != s.original)
            || (r.map && r.map != s.map)) continue;
        int trainer = r.trainer ? (r.trainer == s.trainers[0] ? 0 : r.trainer == s.trainers[1] ? 1 : 2) : 0;
        if (trainer == 2) continue;
        int rank = (r.trainer ? 8 : 0) + (r.original ? 4 : 0) + (r.map ? 2 : 0) + (r.context != RHH_MUSIC_ANY);
        if (rank > bestRank || (rank == bestRank && trainer < bestTrainer))
        { best = r; bestRank = rank; bestTrainer = trainer; }
    }
    if (bestRank < 0) return -1;
    *out = best; return 0;
}
uint64_t NativeMusicAssignedTrack(uint32_t context, uint64_t original, uint64_t map, uint64_t trainerA, uint64_t trainerB)
{
    struct RhhMusicSituation s = {.struct_size = sizeof(s), .context = context, .original = original,
        .map = map, .trainers = {trainerA, trainerB}};
    struct RhhMusicAssignment rule = {.struct_size = sizeof(rule)};
    return !Resolve(&s, &rule) ? rule.track : 0;
}
static int Context(struct RhhMusicAssignmentResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sContext
        || (event != RHH_CAN_SET_MUSIC_ASSIGNMENT && event != RHH_MUSIC_ASSIGNMENT_CHANGED)) return -1;
    *out = *sContext; return 0;
}
static bool32 Ready(void)
{ return RhhModsHasActiveRun() && gSaveBlock1Ptr && gSaveBlock2Ptr && RhhModsCommandsAllowed()
    && !gMain.inBattle && !NativeSaveTransactionActive(); }
static int Fail(struct RhhMusicAssignmentResult *r, u32 status, const char *reason)
{ r->status = status; r->changed = 0; r->after_revision = r->before_revision; r->after = r->before;
  snprintf(r->reason, sizeof(r->reason), "%s", reason); return status; }
static int Change(const struct RhhMusicAssignmentRequest *request, struct RhhMusicAssignmentResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_MUSIC_ASSIGN_INVALID;
    struct RhhMusicAssignmentResult result = {.struct_size = sizeof(result), .is_preview = preview,
        .before.struct_size = sizeof(result.before), .after.struct_size = sizeof(result.after)};
    u32 status = RHH_MUSIC_ASSIGN_INVALID;
    if (!request || request->struct_size < sizeof(*request) || request->rule.struct_size < sizeof(request->rule)
        || !request->requester || !RhhModsRegisteredKey(request->requester) || !ValidContext(request->rule.context)
        || (request->action != RHH_MUSIC_ASSIGN_SET && request->action != RHH_MUSIC_ASSIGN_CLEAR)
        || (request->action == RHH_MUSIC_ASSIGN_CLEAR && request->rule.track)
        || (request->action == RHH_MUSIC_ASSIGN_SET && !KnownTrack(request->rule.track)))
    { Fail(&result, status, "invalid music assignment"); goto done; }
    result.requester = request->requester;
    if (sBusy || !Ready()) { status = Fail(&result, RHH_MUSIC_ASSIGN_UNAVAILABLE, "assignment unavailable"); goto done; }
    struct RhhMusicAssignment rule = request->rule;
    if (!Canonical(RHH_MUSIC, &rule.original) || !Canonical(RHH_MAP, &rule.map)
        || !Canonical(RHH_TRAINER, &rule.trainer))
    { status = Fail(&result, RHH_MUSIC_ASSIGN_INVALID, "unknown context key"); goto done; }
    u8 *state = RhhModsState(sStateId);
    if (!state) { status = Fail(&result, RHH_MUSIC_ASSIGN_UNAVAILABLE, "assignment state unavailable"); goto done; }
    result.before_revision = result.after_revision = Read64(state);
    if (request->expected_revision != result.before_revision)
    { status = Fail(&result, RHH_MUSIC_ASSIGN_STALE, "assignment revision changed"); goto done; }
    u32 count = Count(state), index = count; u8 record[RECORD]; WriteRule(record, &rule);
    for (u32 i = 0; i < count; ++i)
        if (!memcmp(state + HEADER + i * RECORD, record, 32)) { index = i; break; }
    if (index < count) ReadRule(state + HEADER + index * RECORD, &result.before);
    if ((request->action == RHH_MUSIC_ASSIGN_CLEAR && index == count)
        || (request->action == RHH_MUSIC_ASSIGN_SET && index < count && result.before.track == rule.track))
    { result.after = result.before; status = RHH_MUSIC_ASSIGN_OK; goto done; }
    if (index == count && count == RHH_MUSIC_ASSIGNMENT_CAPACITY)
    { status = Fail(&result, RHH_MUSIC_ASSIGN_FULL, "assignment capacity reached"); goto done; }
    if (result.before_revision == UINT64_MAX)
    { status = Fail(&result, RHH_MUSIC_ASSIGN_UNAVAILABLE, "assignment revision exhausted"); goto done; }
    u8 staged[STATE_SIZE], before[STATE_SIZE]; memcpy(before, state, sizeof(before)); memcpy(staged, state, sizeof(staged));
    if (request->action == RHH_MUSIC_ASSIGN_CLEAR)
    {
        memmove(staged + HEADER + index * RECORD, staged + HEADER + (index + 1) * RECORD, (count - index - 1) * RECORD);
        memset(staged + HEADER + (count - 1) * RECORD, 0, RECORD);
    }
    else { memcpy(staged + HEADER + index * RECORD, record, RECORD); result.after = rule; }
    result.changed = 1; result.after_revision++; Write64(staged, result.after_revision);
    if (Validate(staged, sizeof(staged))) { status = Fail(&result, RHH_MUSIC_ASSIGN_INVALID, "invalid assignment state"); goto done; }
    u64 run = RhhModsRunId(); rng_value_t rng = gRngValue, rng2 = gRng2Value; sBusy = TRUE;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_SET_MUSIC_ASSIGNMENT,
        .run_seed = RhhModsRunSeed(), .subject = request->requester, .ordinal = result.before_revision,
        .args = {request->action, preview, rule.context, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sContext = &result; RhhModsDispatch(&event, &decision); sContext = NULL;
    bool32 rngChanged = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed) status = Fail(&result, RHH_MUSIC_ASSIGN_DENIED, decision.reason[0] ? decision.reason : "assignment denied");
    else if (rngChanged || !Ready() || run != RhhModsRunId() || memcmp(before, state, sizeof(before))
        || (request->action == RHH_MUSIC_ASSIGN_SET && !KnownTrack(rule.track)))
        status = Fail(&result, RHH_MUSIC_ASSIGN_STALE, "assignment changed during validation");
    else
    {
        status = result.status = RHH_MUSIC_ASSIGN_OK;
        if (!preview)
        {
            memcpy(state, staged, sizeof(staged)); event.kind = RHH_MUSIC_ASSIGNMENT_CHANGED;
            event.ordinal = result.after_revision; decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
            rng = gRngValue; rng2 = gRng2Value;
            sContext = &result; RhhModsDispatch(&event, &decision); sContext = NULL;
            gRngValue = rng; gRng2Value = rng2;
        }
    }
    sBusy = FALSE;
done:
    result.status = status; *out = result; return status;
}
static int Preview(const struct RhhMusicAssignmentRequest *r, struct RhhMusicAssignmentResult *out) { return Change(r, out, TRUE); }
static int Apply(const struct RhhMusicAssignmentRequest *r, struct RhhMusicAssignmentResult *out) { return Change(r, out, FALSE); }
static const struct RhhMusicAssignmentService sService = {{sizeof(sService), RHH_SERVICE_VERSION(1, 0)},
    Revision, RuleCount, RuleAt, Resolve, Preview, Apply, Context};

enum { LEVEL_HEADER = 8, LEVEL_RECORD = 16, LEVEL_STATE_SIZE = LEVEL_HEADER + LEVEL_RECORD * RHH_MUSIC_LEVEL_CAPACITY };
static const char sLevelId[] = "rhh:music_levels";
static bool32 sLevelBusy;
static const struct RhhMusicLevelResult *sLevelContext;
static u32 LevelCount(const u8 *state)
{
    u32 count = 0;
    while (count < RHH_MUSIC_LEVEL_CAPACITY && Read64(state + LEVEL_HEADER + count * LEVEL_RECORD)) ++count;
    return count;
}
static int ValidateLevels(const u8 *state, u32 size)
{
    if (!state || size != LEVEL_STATE_SIZE) return -1;
    u64 previous = 0; bool32 empty = FALSE;
    for (u32 i = 0; i < RHH_MUSIC_LEVEL_CAPACITY; ++i)
    {
        const u8 *entry = state + LEVEL_HEADER + i * LEVEL_RECORD;
        u64 key = Read64(entry); u32 gain = Read32(entry + 8);
        if (!key)
        {
            empty = TRUE;
            if (gain || Read32(entry + 12)) return -1;
        }
        else
        {
            if (empty || !Read64(state) || key <= previous || gain > RHH_MUSIC_GAIN_MAX
                || gain == RHH_MUSIC_GAIN_ONE || Read32(entry + 12)) return -1;
            previous = key;
        }
    }
    return 0;
}
static const struct RhhModDefinition sLevelState = {.struct_size = sizeof(sLevelState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sLevelId, .state_size = LEVEL_STATE_SIZE, .validate = ValidateLevels};
static u32 LevelFind(const u8 *state, u64 key, bool32 *found)
{
    u32 count = LevelCount(state), index = 0;
    while (index < count && Read64(state + LEVEL_HEADER + index * LEVEL_RECORD) < key) ++index;
    *found = index < count && Read64(state + LEVEL_HEADER + index * LEVEL_RECORD) == key;
    return index;
}
uint32_t NativeMusicTrackLevel(uint64_t track)
{
    const u8 *state = RhhModsState(sLevelId);
    if (!track || !state || !RhhModsHasActiveRun()) return RHH_MUSIC_GAIN_ONE;
    bool32 found; u32 index = LevelFind(state, track, &found);
    return found ? Read32(state + LEVEL_HEADER + index * LEVEL_RECORD + 8) : RHH_MUSIC_GAIN_ONE;
}
static u64 LevelRevision(void)
{
    const u8 *state = RhhModsState(sLevelId);
    return state && RhhModsHasActiveRun() ? Read64(state) : 0;
}
static u32 LevelServiceCount(void)
{
    const u8 *state = RhhModsState(sLevelId);
    return state && RhhModsHasActiveRun() ? LevelCount(state) : 0;
}
static int LevelAt(u32 index, struct RhhMusicLevel *out)
{
    const u8 *state = RhhModsState(sLevelId);
    if (!out || out->struct_size < sizeof(*out) || !state || !RhhModsHasActiveRun() || index >= LevelCount(state)) return -1;
    const u8 *entry = state + LEVEL_HEADER + index * LEVEL_RECORD;
    *out = (struct RhhMusicLevel){.struct_size = sizeof(*out), .track = Read64(entry), .gain = Read32(entry + 8)}; return 0;
}
static int LevelGet(u64 track, u32 *gain)
{
    if (!track || !gain || !RhhModsHasActiveRun() || !RhhModsState(sLevelId)) return -1;
    *gain = NativeMusicTrackLevel(track); return 0;
}
static int LevelContext(struct RhhMusicLevelResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sLevelContext
        || (event != RHH_CAN_SET_MUSIC_LEVEL && event != RHH_MUSIC_LEVEL_CHANGED)) return -1;
    *out = *sLevelContext; return 0;
}
static int LevelFail(struct RhhMusicLevelResult *result, u32 status, const char *reason)
{
    result->status = status; result->changed = 0; result->after_gain = result->before_gain;
    result->after_revision = result->before_revision; snprintf(result->reason, sizeof(result->reason), "%s", reason); return status;
}
static bool32 LevelTrackSupported(u64 key)
{
    u32 id;
    return key && (RhhModsFindMusic(key) || NativeMusicImportedKnown(key)
        || (!RhhContentResolve(RHH_MIDI, key, &id) && id >= 1 && id <= 20 && (id < 9 || id > 12)));
}
static int LevelChange(const struct RhhMusicLevelRequest *request, struct RhhMusicLevelResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_MUSIC_LEVEL_INVALID;
    struct RhhMusicLevelResult result = {.struct_size = sizeof(result), .is_preview = preview};
    u32 status = RHH_MUSIC_LEVEL_INVALID;
    if (!request || request->struct_size < sizeof(*request) || !request->requester || !request->track
        || !RhhModsRegisteredKey(request->requester) || request->gain > RHH_MUSIC_GAIN_MAX
        || (request->gain != RHH_MUSIC_GAIN_ONE && !LevelTrackSupported(request->track)))
    { LevelFail(&result, status, "invalid track or level"); goto done; }
    result.requester = request->requester; result.track = request->track;
    if (sLevelBusy || !Ready()) { status = LevelFail(&result, RHH_MUSIC_LEVEL_UNAVAILABLE, "level unavailable"); goto done; }
    u8 *state = RhhModsState(sLevelId);
    if (!state) { status = LevelFail(&result, RHH_MUSIC_LEVEL_UNAVAILABLE, "level state unavailable"); goto done; }
    result.before_revision = result.after_revision = Read64(state);
    result.before_gain = result.after_gain = NativeMusicTrackLevel(request->track);
    if (request->expected_revision != result.before_revision)
    { status = LevelFail(&result, RHH_MUSIC_LEVEL_STALE, "level revision changed"); goto done; }
    if (request->gain == result.before_gain) { status = RHH_MUSIC_LEVEL_OK; goto done; }
    bool32 found; u32 count = LevelCount(state), index = LevelFind(state, request->track, &found);
    if (!found && count == RHH_MUSIC_LEVEL_CAPACITY)
    { status = LevelFail(&result, RHH_MUSIC_LEVEL_FULL, "level capacity reached"); goto done; }
    if (result.before_revision == UINT64_MAX)
    { status = LevelFail(&result, RHH_MUSIC_LEVEL_UNAVAILABLE, "level revision exhausted"); goto done; }
    u8 staged[LEVEL_STATE_SIZE], before[LEVEL_STATE_SIZE]; memcpy(before, state, sizeof(before)); memcpy(staged, state, sizeof(staged));
    u8 *entry = staged + LEVEL_HEADER + index * LEVEL_RECORD;
    if (request->gain == RHH_MUSIC_GAIN_ONE)
    {
        memmove(entry, entry + LEVEL_RECORD, (count - index - 1) * LEVEL_RECORD);
        memset(staged + LEVEL_HEADER + (count - 1) * LEVEL_RECORD, 0, LEVEL_RECORD);
    }
    else
    {
        if (!found) memmove(entry + LEVEL_RECORD, entry, (count - index) * LEVEL_RECORD);
        memset(entry, 0, LEVEL_RECORD); Write64(entry, request->track); Write32(entry + 8, request->gain);
    }
    result.changed = 1; result.after_gain = request->gain; result.after_revision++; Write64(staged, result.after_revision);
    if (ValidateLevels(staged, sizeof(staged))) { status = LevelFail(&result, RHH_MUSIC_LEVEL_INVALID, "invalid level state"); goto done; }
    u64 run = RhhModsRunId(); rng_value_t rng = gRngValue, rng2 = gRng2Value; sLevelBusy = TRUE;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_SET_MUSIC_LEVEL,
        .run_seed = RhhModsRunSeed(), .event_key = request->track, .subject = request->requester,
        .ordinal = result.before_revision, .args = {request->gain, preview}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sLevelContext = &result; RhhModsDispatch(&event, &decision); sLevelContext = NULL;
    bool32 rngChanged = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed) status = LevelFail(&result, RHH_MUSIC_LEVEL_DENIED, decision.reason[0] ? decision.reason : "level denied");
    else if (rngChanged || !Ready() || run != RhhModsRunId() || memcmp(before, state, sizeof(before))
        || (request->gain != RHH_MUSIC_GAIN_ONE && !LevelTrackSupported(request->track)))
        status = LevelFail(&result, RHH_MUSIC_LEVEL_STALE, "level changed during validation");
    else
    {
        status = result.status = RHH_MUSIC_LEVEL_OK;
        if (!preview)
        {
            memcpy(state, staged, sizeof(staged)); event.kind = RHH_MUSIC_LEVEL_CHANGED; event.ordinal = result.after_revision;
            decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
            rng = gRngValue; rng2 = gRng2Value;
            sLevelContext = &result; RhhModsDispatch(&event, &decision); sLevelContext = NULL;
            gRngValue = rng; gRng2Value = rng2;
        }
    }
    sLevelBusy = FALSE;
done:
    result.status = status; *out = result; return status;
}
static int LevelPreview(const struct RhhMusicLevelRequest *request, struct RhhMusicLevelResult *out) { return LevelChange(request, out, TRUE); }
static int LevelApply(const struct RhhMusicLevelRequest *request, struct RhhMusicLevelResult *out) { return LevelChange(request, out, FALSE); }
static const struct RhhMusicLevelService sLevelService = {{sizeof(sLevelService), RHH_SERVICE_VERSION(1, 0)},
    LevelRevision, LevelServiceCount, LevelAt, LevelGet, LevelPreview, LevelApply, LevelContext};
int NativeMusicInstallAssignments(void)
{
    sBusy = sLevelBusy = FALSE; sContext = NULL; sLevelContext = NULL;
    if (RhhModsRegister(&sState) || RhhModsRegister(&sLevelState)) return -1;
    return RhhModsAddService("rhh:service/music_assignments", &sService.header)
        || RhhModsAddService("rhh:service/music_levels", &sLevelService.header) ? -1 : 0;
}
