/* Save-backed, checked trainer-ace selections for native modules. */
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "main.h"
#include "mod_api.h"
#include "mod_host.h"
#include "native.h"

enum { ACE_HEADER = 8, ACE_RECORD = 64, ACE_STATE_SIZE = ACE_HEADER + RHH_BOSS_ACE_CAPACITY * ACE_RECORD };
static const char sStateId[] = "rhh:boss_ace_state";
static bool32 sBusy;
static const struct RhhBossAceResult *sContext;
static u32 Read32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u64 Read64(const u8 *p) { u64 n = 0; for (u32 i = 0; i < 8; ++i) n |= (u64)p[i] << (8 * i); return n; }
static void Write32(u8 *p, u32 n) { for (u32 i = 0; i < 4; ++i) p[i] = n >> (8 * i); }
static void Write64(u8 *p, u64 n) { for (u32 i = 0; i < 8; ++i) p[i] = n >> (8 * i); }
static u8 *Record(u8 *state, u32 i) { return state + ACE_HEADER + i * ACE_RECORD; }
static bool32 ValidDifficulty(u32 d) { return d >= RHH_TRAINER_DIFFICULTY_EASY && d <= RHH_TRAINER_DIFFICULTY_HARD; }
static int Validate(const u8 *state, u32 size)
{
    if (size != ACE_STATE_SIZE) return -1;
    u64 previous = 0; u32 previousDifficulty = 0; bool32 empty = FALSE;
    for (u32 i = 0; i < RHH_BOSS_ACE_CAPACITY; ++i)
    {
        const u8 *p = state + ACE_HEADER + i * ACE_RECORD;
        u64 trainer = Read64(p); u32 difficulty = Read32(p + 44);
        if (!trainer)
        {
            empty = TRUE;
            for (u32 j = 0; j < ACE_RECORD; ++j) if (p[j]) return -1;
            continue;
        }
        if (empty || !Read64(state) || !Read64(p + 8) || !Read64(p + 24) || !Read64(p + 32)
            || !ValidDifficulty(difficulty) || !ValidDifficulty(Read32(p + 48)) || Read32(p + 40) > 254
            || Read32(p + 52) > 255 || (Read32(p + 56) & ~(RHH_TRAINER_SOURCE_LEAD | RHH_TRAINER_SOURCE_ACE))
            || Read32(p + 60) || trainer < previous || (trainer == previous && difficulty <= previousDifficulty)) return -1;
        previous = trainer; previousDifficulty = difficulty;
    }
    return 0;
}
static const struct RhhModDefinition sState = {.struct_size = sizeof(sState), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sStateId, .flags = RHH_MOD_REQUIRED,
    .state_size = ACE_STATE_SIZE, .validate = Validate};
static const struct RhhTrainerRosterService *Rosters(void)
{ return (const void *)RhhModsHostAPI()->get_service("rhh:service/trainer_roster", RHH_SERVICE_VERSION(1, 0)); }
static RhhContentKey CanonicalTrainer(RhhContentKey key)
{
    u32 id;
    if (!key) return 0;
    if (!RhhContentResolve(RHH_TRAINER, key, &id)) return RhhContentKeyForId(RHH_TRAINER, id);
    return key; /* A removed trainer can still be inspected and cleared. */
}
static u32 Count(const u8 *state)
{
    u32 count = 0;
    while (count < RHH_BOSS_ACE_CAPACITY && Read64(state + ACE_HEADER + count * ACE_RECORD)) ++count;
    return count;
}
static u32 Find(const u8 *state, RhhContentKey trainer, u32 difficulty, bool32 *found)
{
    u32 count = Count(state), i = 0;
    while (i < count)
    {
        const u8 *p = state + ACE_HEADER + i * ACE_RECORD;
        u64 key = Read64(p); u32 d = Read32(p + 44);
        if (key > trainer || (key == trainer && d >= difficulty)) break;
        ++i;
    }
    *found = i < count && Read64(state + ACE_HEADER + i * ACE_RECORD) == trainer
        && Read32(state + ACE_HEADER + i * ACE_RECORD + 44) == difficulty;
    return i;
}
static void Selection(const u8 *state, RhhContentKey trainer, u32 difficulty, struct RhhBossAceSelection *out)
{
    bool32 found; u32 index = Find(state, trainer, difficulty, &found);
    *out = (struct RhhBossAceSelection){.struct_size = sizeof(*out), .trainer = trainer,
        .trainer_difficulty = difficulty, .revision = Read64(state)};
    if (!found) return;
    const u8 *p = state + ACE_HEADER + index * ACE_RECORD;
    out->valid = 1; out->owner = Read64(p + 8); out->token = Read64(p + 16);
    out->roster_trainer = Read64(p + 24); out->species = Read64(p + 32);
    out->source_index = Read32(p + 40); out->roster_difficulty = Read32(p + 48);
    out->source_level = Read32(p + 52); out->source_flags = Read32(p + 56);
    if (!RhhModsRegisteredKey(out->owner)) out->flags |= RHH_BOSS_ACE_ORPHANED;
    const struct RhhTrainerRosterService *rosters = Rosters();
    struct RhhTrainerRoster current = {.struct_size = sizeof(current)};
    if (!rosters || rosters->get(trainer, &current)) out->flags |= RHH_BOSS_ACE_UNAVAILABLE;
    else if (current.trainer_difficulty != difficulty) out->flags |= RHH_BOSS_ACE_INACTIVE;
    else
    {
        struct RhhTrainerRosterEntry entry = {.struct_size = sizeof(entry)};
        if (current.token != out->token || current.roster_trainer != out->roster_trainer
            || current.roster_difficulty != out->roster_difficulty || rosters->entry(&current, out->source_index, &entry)
            || entry.species != out->species || entry.level != out->source_level || entry.flags != out->source_flags)
            out->flags |= RHH_BOSS_ACE_STALE;
    }
}
static uint32_t BossAceCount(void)
{
    const u8 *state = RhhModsState(sStateId);
    return RhhModsHasActiveRun() && state ? Count(state) : 0;
}
static int BossAceGet(RhhContentKey trainer, u32 difficulty, struct RhhBossAceSelection *out)
{
    const u8 *state = RhhModsState(sStateId);
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || !state
        || !(trainer = CanonicalTrainer(trainer)) || !ValidDifficulty(difficulty)) return -1;
    struct RhhBossAceSelection result; Selection(state, trainer, difficulty, &result); *out = result; return 0;
}
static int BossAceAt(u32 index, struct RhhBossAceSelection *out)
{
    u8 *state = RhhModsState(sStateId);
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || !state || index >= Count(state)) return -1;
    const u8 *p = Record(state, index); return BossAceGet(Read64(p), Read32(p + 44), out);
}
static int BossAceContext(struct RhhBossAceResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sContext
        || (event != RHH_CAN_CONFIGURE_BOSS_ACE && event != RHH_BOSS_ACE_CONFIGURED)) return -1;
    *out = *sContext; return 0;
}
static int Failure(struct RhhBossAceResult *result, u32 status, const char *reason)
{
    result->status = status; result->changed = 0;
    snprintf(result->reason, sizeof(result->reason), "%s", reason ? reason : ""); return status;
}
static bool32 Ready(void)
{ return RhhModsHasActiveRun() && gSaveBlock1Ptr && gSaveBlock2Ptr && RhhModsCommandsAllowed()
    && !gMain.inBattle && !NativeSaveTransactionActive(); }
static int BossAceChange(const struct RhhBossAceRequest *request, struct RhhBossAceResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_BOSS_ACE_INVALID;
    struct RhhBossAceResult result = {.struct_size = sizeof(result), .is_preview = preview,
        .before.struct_size = sizeof(result.before), .after.struct_size = sizeof(result.after)};
    u32 status = RHH_BOSS_ACE_INVALID;
    if (!request || request->struct_size < sizeof(*request) || !request->trainer || !request->owner
        || !RhhModsRegisteredKey(request->owner) || !ValidDifficulty(request->trainer_difficulty)
        || (request->action != RHH_BOSS_ACE_SET && request->action != RHH_BOSS_ACE_CLEAR)
        || (request->action == RHH_BOSS_ACE_CLEAR && request->source_index))
    { Failure(&result, status, "invalid boss ace request"); goto done; }
    if (sBusy || !Ready()) { status = Failure(&result, RHH_BOSS_ACE_UNAVAILABLE_COMMAND, "configuration unavailable"); goto done; }
    u8 *state = RhhModsState(sStateId); RhhContentKey trainer = CanonicalTrainer(request->trainer);
    if (!state || !trainer) { status = Failure(&result, RHH_BOSS_ACE_UNAVAILABLE_COMMAND, "configuration unavailable"); goto done; }
    Selection(state, trainer, request->trainer_difficulty, &result.before); result.after = result.before;
    if (request->expected_revision != result.before.revision)
    { status = Failure(&result, RHH_BOSS_ACE_STALE_REQUEST, "configuration revision changed"); goto done; }
    if (result.before.valid && result.before.owner != request->owner && !(result.before.flags & RHH_BOSS_ACE_ORPHANED))
    { status = Failure(&result, RHH_BOSS_ACE_CONFLICT, "selection belongs to another module"); goto done; }
    struct RhhTrainerRosterEntry entry = {.struct_size = sizeof(entry)};
    const struct RhhTrainerRosterService *rosters = Rosters();
    if (request->action == RHH_BOSS_ACE_SET && (!rosters || request->roster.struct_size < sizeof(request->roster)
        || request->roster.trainer != trainer || request->roster.trainer_difficulty != request->trainer_difficulty
        || !ValidDifficulty(request->roster.roster_difficulty) || rosters->entry(&request->roster, request->source_index, &entry)))
    { status = Failure(&result, RHH_BOSS_ACE_STALE_REQUEST, "trainer roster changed or is unavailable"); goto done; }
    bool32 found; u32 index = Find(state, trainer, request->trainer_difficulty, &found), count = Count(state);
    if (!found && request->action == RHH_BOSS_ACE_CLEAR) { status = RHH_BOSS_ACE_OK; goto done; }
    if (!found && count == RHH_BOSS_ACE_CAPACITY) { status = Failure(&result, RHH_BOSS_ACE_FULL, "selection capacity reached"); goto done; }
    u8 staged[ACE_STATE_SIZE]; memcpy(staged, state, sizeof(staged));
    u8 next[ACE_RECORD] = {0};
    if (request->action == RHH_BOSS_ACE_SET)
    {
        Write64(next, trainer); Write64(next + 8, request->owner); Write64(next + 16, request->roster.token);
        Write64(next + 24, request->roster.roster_trainer); Write64(next + 32, entry.species);
        Write32(next + 40, request->source_index); Write32(next + 44, request->trainer_difficulty);
        Write32(next + 48, request->roster.roster_difficulty); Write32(next + 52, entry.level); Write32(next + 56, entry.flags);
        if (found && !memcmp(Record(staged, index), next, sizeof(next))) { status = RHH_BOSS_ACE_OK; goto done; }
    }
    if (result.before.revision == UINT64_MAX)
    { status = Failure(&result, RHH_BOSS_ACE_UNAVAILABLE_COMMAND, "configuration revision exhausted"); goto done; }
    if (found && request->action == RHH_BOSS_ACE_CLEAR)
    {
        memmove(Record(staged, index), Record(staged, index + 1), (count - index - 1) * ACE_RECORD);
        memset(Record(staged, count - 1), 0, ACE_RECORD);
    }
    else
    {
        if (!found) memmove(Record(staged, index + 1), Record(staged, index), (count - index) * ACE_RECORD);
        memcpy(Record(staged, index), next, ACE_RECORD);
    }
    Write64(staged, result.before.revision + 1);
    if (Validate(staged, sizeof(staged))) { status = Failure(&result, RHH_BOSS_ACE_INVALID, "invalid selection"); goto done; }
    Selection(staged, trainer, request->trainer_difficulty, &result.after); result.changed = 1;
    u8 before[ACE_STATE_SIZE]; memcpy(before, state, sizeof(before)); u64 run = RhhModsRunId();
    sBusy = TRUE;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_CONFIGURE_BOSS_ACE,
        .run_seed = RhhModsRunSeed(), .event_key = trainer, .subject = request->owner,
        .ordinal = request->expected_revision, .args = {request->action, request->source_index, preview, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sContext = &result; RhhModsDispatch(&event, &decision); sContext = NULL;
    if (!decision.allowed) status = Failure(&result, RHH_BOSS_ACE_DENIED, decision.reason[0] ? decision.reason : "selection denied");
    else if (!Ready() || run != RhhModsRunId() || memcmp(before, state, sizeof(before))
        || (request->action == RHH_BOSS_ACE_SET && rosters->entry(&request->roster, request->source_index, &entry)))
        status = Failure(&result, RHH_BOSS_ACE_STALE_REQUEST, "configuration changed during validation");
    else
    {
        status = result.status = RHH_BOSS_ACE_OK;
        if (!preview)
        {
            memcpy(state, staged, sizeof(staged)); event.kind = RHH_BOSS_ACE_CONFIGURED;
            event.ordinal = result.after.revision; decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
            sContext = &result; RhhModsDispatch(&event, &decision); sContext = NULL;
        }
    }
    sBusy = FALSE;
done:
    result.status = status; *out = result; return status;
}
static int BossAcePreview(const struct RhhBossAceRequest *r, struct RhhBossAceResult *out) { return BossAceChange(r, out, TRUE); }
static int BossAceApply(const struct RhhBossAceRequest *r, struct RhhBossAceResult *out) { return BossAceChange(r, out, FALSE); }
static int BossAceMatch(u32 owner, u32 slot, struct RhhBossAceMatch *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct RhhBattleService *battle = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 1));
    const struct RhhTrainerRosterService *rosters = Rosters();
    struct RhhBossAceMatch result = {.struct_size = sizeof(result), .source.struct_size = sizeof(result.source),
        .selection.struct_size = sizeof(result.selection)};
    struct RhhTrainerRoster roster = {.struct_size = sizeof(roster)};
    if (!battle || !rosters || battle->trainer_party_source(owner, slot, &result.source)
        || rosters->get(result.source.trainer, &roster)
        || BossAceGet(result.source.trainer, roster.trainer_difficulty, &result.selection)) return -1;
    if (result.selection.valid && !result.selection.flags
        && rosters->match(&roster, result.selection.source_index, owner, slot, &result.matched)) return -1;
    *out = result; return 0;
}
static const struct RhhBossAceService sService = {{sizeof(sService), RHH_SERVICE_VERSION(1, 0)},
    BossAceCount, BossAceAt, BossAceGet, BossAcePreview, BossAceApply, BossAceContext, BossAceMatch};
int RhhModsInstallBossAceService(void)
{
    sBusy = FALSE; sContext = NULL;
    if (RhhModsRegister(&sState)) return -1;
    return RhhModsAddService("rhh:service/boss_ace", &sService.header);
}
