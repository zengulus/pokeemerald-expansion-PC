/* Rollback-safe item rewards for photographs. */
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "main.h"
#include "item.h"
#include "event_data.h"
#include "overworld.h"
#include "script.h"
#include "palette.h"
#include "battle_pyramid.h"
#include "constants/flags.h"
#include "mod_api.h"
#include "mod_host.h"
#include "mods.h"
#include "native.h"

enum { REWARD_HEADER = 8, REWARD_RECORD = 48,
       REWARD_LEDGER_SIZE = REWARD_HEADER + RHH_PHOTO_REWARD_CAPACITY * REWARD_RECORD,
       REWARD_UNSCORED = RHH_PHOTO_SCORE_MAX + 1,
       RETARGET_RECORD = 16, RETARGET_SIZE = REWARD_HEADER + RHH_PHOTO_EXPERIENCE_RETARGET_CAPACITY * RETARGET_RECORD,
       EXPERIENCE_APPLIED_SIZE = REWARD_HEADER + RHH_PHOTO_REWARD_CAPACITY * 8 };
static const char sLedgerId[] = "rhh:photo_reward_ledger";
static const char sAppliedId[] = "rhh:photo_reward_applied";
static const char sExperienceLedgerId[] = "rhh:photo_experience_ledger";
static const char sExperienceAppliedId[] = "rhh:photo_experience_applied";
static const char sExperienceRetargetId[] = "rhh:photo_experience_retarget";
static bool32 sBusy;
static const struct RhhPhotoRewardResult *sContext;
static bool32 sContextScoreAvailable;
static const struct RhhPhotoExperienceResult *sExperienceContext;
static const struct RhhPhotoExperienceRetargetResult *sRetargetContext;
static bool32 sExperienceRetryBlocked;
static struct {
    bool32 active, delivered;
    u64 ticket, run;
    u32 index;
    struct RhhPhotoExperienceResult result;
} sExperienceDelivery;

static u32 Read32(const u8 *p)
{ return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u64 Read64(const u8 *p)
{ u64 n = 0; for (u32 i = 0; i < 8; ++i) n |= (u64)p[i] << (i * 8); return n; }
static void Write32(u8 *p, u32 n) { for (u32 i = 0; i < 4; ++i) p[i] = n >> (i * 8); }
static void Write64(u8 *p, u64 n) { for (u32 i = 0; i < 8; ++i) p[i] = n >> (i * 8); }
static u8 *Record(u8 *state, u32 i) { return state + REWARD_HEADER + i * REWARD_RECORD; }
static const u8 *ConstRecord(const u8 *state, u32 i) { return state + REWARD_HEADER + i * REWARD_RECORD; }

static int ValidateLedger(const u8 *state, u32 size)
{
    if (size != REWARD_LEDGER_SIZE || Read32(state) > RHH_PHOTO_REWARD_CAPACITY || Read32(state + 4)) return -1;
    u32 count = Read32(state);
    for (u32 i = 0; i < RHH_PHOTO_REWARD_CAPACITY; ++i)
    {
        const u8 *p = ConstRecord(state, i);
        if (i >= count)
        { for (u32 j = 0; j < REWARD_RECORD; ++j) if (p[j]) return -1; continue; }
        if (!Read64(p) || !Read64(p + 8) || !Read64(p + 16) || !Read64(p + 24) || !Read64(p + 32)
            || !Read32(p + 40) || Read32(p + 40) > 999 || Read32(p + 44) > REWARD_UNSCORED) return -1;
        for (u32 j = 0; j < i; ++j)
        {
            const u8 *q = ConstRecord(state, j);
            if (Read64(p) == Read64(q) && Read64(p + 16) == Read64(q + 16)
                && Read64(p + 24) == Read64(q + 24)) return -1;
        }
    }
    return 0;
}
static int MergeLedger(u8 *state, const u8 *journal, u32 size)
{
    if (ValidateLedger(state, size) || ValidateLedger(journal, size)) return -1;
    u32 a = Read32(state), b = Read32(journal), common = a < b ? a : b;
    if (memcmp(state + REWARD_HEADER, journal + REWARD_HEADER, common * REWARD_RECORD)) return -1;
    if (b > a) memcpy(state, journal, size);
    return 0;
}
static int ValidateApplied(const u8 *state, u32 size)
{ return size != 8 || Read32(state) > RHH_PHOTO_REWARD_CAPACITY || Read32(state + 4) ? -1 : 0; }
static int ValidateExperienceApplied(const u8 *state, u32 size)
{
    if (size != EXPERIENCE_APPLIED_SIZE || Read32(state) > RHH_PHOTO_REWARD_CAPACITY || Read32(state + 4)) return -1;
    for (u32 i = Read32(state); i < RHH_PHOTO_REWARD_CAPACITY; ++i)
        if (Read64(state + REWARD_HEADER + i * 8)) return -1;
    return 0;
}
static int MigrateExperienceApplied(u32 schema, const u8 *old, u32 oldSize, u8 *state, u32 size)
{
    if (schema != 1 || oldSize != 8 || size != EXPERIENCE_APPLIED_SIZE || ValidateApplied(old, oldSize)) return -1;
    memset(state, 0, size); memcpy(state, old, oldSize); return 0;
}
static int ValidateExperienceLedger(const u8 *state, u32 size)
{
    if (size != REWARD_LEDGER_SIZE || Read32(state) > RHH_PHOTO_REWARD_CAPACITY || Read32(state + 4)) return -1;
    u32 count = Read32(state);
    for (u32 i = 0; i < RHH_PHOTO_REWARD_CAPACITY; ++i)
    {
        const u8 *p = ConstRecord(state, i);
        if (i >= count)
        { for (u32 j = 0; j < REWARD_RECORD; ++j) if (p[j]) return -1; continue; }
        if (!Read64(p) || !Read64(p + 8) || !Read64(p + 16) || !Read64(p + 24) || !Read64(p + 32)
            || !Read32(p + 40) || Read32(p + 44) > REWARD_UNSCORED) return -1;
        for (u32 j = 0; j < i; ++j)
        {
            const u8 *q = ConstRecord(state, j);
            if (Read64(p) == Read64(q) && Read64(p + 16) == Read64(q + 16)
                && Read64(p + 24) == Read64(q + 24)) return -1;
        }
    }
    return 0;
}
static int MergeExperienceLedger(u8 *state, const u8 *journal, u32 size)
{
    if (ValidateExperienceLedger(state, size) || ValidateExperienceLedger(journal, size)) return -1;
    u32 a = Read32(state), b = Read32(journal), common = a < b ? a : b;
    if (memcmp(state + REWARD_HEADER, journal + REWARD_HEADER, common * REWARD_RECORD)) return -1;
    if (b > a) memcpy(state, journal, size);
    return 0;
}
static int ValidateRetargetLedger(const u8 *state, u32 size)
{
    if (size != RETARGET_SIZE || Read32(state) > RHH_PHOTO_EXPERIENCE_RETARGET_CAPACITY || Read32(state + 4)) return -1;
    u32 count = Read32(state);
    for (u32 i = 0; i < RHH_PHOTO_EXPERIENCE_RETARGET_CAPACITY; ++i)
    {
        const u8 *p = state + REWARD_HEADER + i * RETARGET_RECORD;
        if (i >= count)
        { for (u32 j = 0; j < RETARGET_RECORD; ++j) if (p[j]) return -1; continue; }
        if (!Read32(p) || Read32(p) > RHH_PHOTO_REWARD_CAPACITY || Read32(p + 4) || !Read64(p + 8)) return -1;
    }
    return 0;
}
static int MergeRetargetLedger(u8 *state, const u8 *journal, u32 size)
{
    if (ValidateRetargetLedger(state, size) || ValidateRetargetLedger(journal, size)) return -1;
    u32 a = Read32(state), b = Read32(journal), common = a < b ? a : b;
    if (memcmp(state + REWARD_HEADER, journal + REWARD_HEADER, common * RETARGET_RECORD)) return -1;
    if (b > a) memcpy(state, journal, size);
    return 0;
}
static int ExperienceRecipientIdentity(const u8 *ledger, u32 index, u64 *out)
{
    const u8 *changes = RhhModsState(sExperienceRetargetId);
    if (!ledger || !changes || index >= Read32(ledger) || ValidateRetargetLedger(changes, RETARGET_SIZE)) return -1;
    u64 identity = Read64(ConstRecord(ledger, index) + 32);
    for (u32 i = 0; i < Read32(changes); ++i)
    {
        const u8 *p = changes + REWARD_HEADER + i * RETARGET_RECORD;
        u32 sequence = Read32(p);
        if (sequence > Read32(ledger)) return -1;
        if (sequence == index + 1) identity = Read64(p + 8);
    }
    *out = identity; return 0;
}
static void ResetExperienceDelivery(void)
{ memset(&sExperienceDelivery, 0, sizeof(sExperienceDelivery)); sExperienceRetryBlocked = FALSE; }
static bool32 ExperiencePending(void)
{
    const u8 *ledger = RhhModsState(sExperienceLedgerId), *applied = RhhModsState(sExperienceAppliedId);
    return ledger && applied && Read32(applied) < Read32(ledger);
}
static bool32 HasReward(const u8 *ledger, u64 individual, RhhContentKey owner, RhhContentKey rule)
{
    if (!ledger) return FALSE;
    for (u32 i = 0; i < Read32(ledger); ++i)
    {
        const u8 *p = ConstRecord(ledger, i);
        if (Read64(p) == individual && Read64(p + 16) == owner && Read64(p + 24) == rule) return TRUE;
    }
    return FALSE;
}

static bool32 Ready(void)
{
    return RhhModsHasActiveRun() && RhhModsCanPersistDurably() && RhhModsCommandsAllowed()
        && gSaveBlock1Ptr && gSaveBlock2Ptr && !NativeSaveTransactionActive()
        && !gMain.inBattle && gMain.callback2 == CB2_Overworld && !gPaletteFade.active && !gPaletteFade.y
        && !ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked() && !IsOverworldLinkActive()
        && CurrentBattlePyramidLocation() == PYRAMID_LOCATION_NONE && !FlagGet(FLAG_STORING_ITEMS_IN_PYRAMID_BAG);
}
static bool32 BagReady(u32 item)
{
    u32 pocket = GetItemPocket(item);
    if (pocket >= POCKETS_COUNT || !gBagPockets[pocket].itemSlots || !gBagPockets[pocket].capacity) return FALSE;
    switch (pocket)
    {
    case POCKET_ITEMS: return gBagPockets[pocket].itemSlots == gSaveBlock1Ptr->bag.items;
    case POCKET_KEY_ITEMS: return gBagPockets[pocket].itemSlots == gSaveBlock1Ptr->bag.keyItems;
    case POCKET_POKE_BALLS: return gBagPockets[pocket].itemSlots == gSaveBlock1Ptr->bag.pokeBalls;
    case POCKET_TM_HM: return gBagPockets[pocket].itemSlots == gSaveBlock1Ptr->bag.TMsHMs;
    case POCKET_BERRIES: return gBagPockets[pocket].itemSlots == gSaveBlock1Ptr->bag.berries;
    default: return FALSE;
    }
}
static u32 Count(void)
{
    const u8 *ledger = RhhModsState(sLedgerId);
    return RhhModsHasActiveRun() && ledger ? Read32(ledger) : 0;
}
static int ReceiptAt(u32 index, struct RhhPhotoRewardReceipt *out)
{
    const u8 *ledger = RhhModsState(sLedgerId), *applied = RhhModsState(sAppliedId);
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || !ledger || !applied
        || index >= Read32(ledger) || Read32(applied) > Read32(ledger)) return -1;
    const u8 *p = ConstRecord(ledger, index); u64 run = RhhModsRunId();
    *out = (struct RhhPhotoRewardReceipt){.struct_size = sizeof(*out), .quantity = Read32(p + 40),
        .score = Read32(p + 44) == REWARD_UNSCORED ? 0 : Read32(p + 44),
        .delivered = index < Read32(applied), .sequence = index + 1,
        .photo = {run, Read64(p + 8)},
        .individual = {.struct_size = sizeof(struct RhhWildActorIndividual), .run_id = run, .serial = Read64(p)},
        .owner = Read64(p + 16), .rule = Read64(p + 24), .item = Read64(p + 32)};
    return 0;
}
static int ScoreAvailable(u32 index, u32 *out)
{
    const u8 *ledger = RhhModsState(sLedgerId);
    if (!out || !RhhModsHasActiveRun() || !ledger || index >= Read32(ledger)) return -1;
    *out = Read32(ConstRecord(ledger, index) + 44) != REWARD_UNSCORED;
    return 0;
}
static int Context(struct RhhPhotoRewardResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sContext
        || (event != RHH_CAN_CLAIM_PHOTO_REWARD && event != RHH_PHOTO_REWARD_CLAIMED
            && event != RHH_PHOTO_REWARD_DELIVERED)) return -1;
    *out = *sContext; return 0;
}
static int ContextScoreAvailable(u32 *out)
{
    struct RhhPhotoRewardResult check = {.struct_size = sizeof(check)};
    if (!out || Context(&check)) return -1;
    *out = sContextScoreAvailable; return 0;
}
static u32 Failure(struct RhhPhotoRewardResult *result, u32 status, const char *reason)
{
    result->status = status; result->changed = 0;
    snprintf(result->reason, sizeof(result->reason), "%s", reason); return status;
}
static void Notify(u32 kind, struct RhhPhotoRewardResult *result, bool32 scoreAvailable)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = result->receipt.rule, .subject = result->receipt.individual.serial,
        .ordinal = result->receipt.sequence, .args = {result->receipt.quantity, result->receipt.score, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sContext = result; sContextScoreAvailable = scoreAvailable;
    RhhModsDispatch(&event, &decision); sContext = NULL;
}
static u32 DeliverNext(struct RhhPhotoRewardResult *result)
{
    u8 *ledger = RhhModsState(sLedgerId), *applied = RhhModsState(sAppliedId);
    if (!Ready() || !ledger || !applied || Read32(applied) > Read32(ledger))
        return Failure(result, RHH_PHOTO_REWARD_UNAVAILABLE, "reward delivery unavailable");
    u32 index = Read32(applied);
    if (index == Read32(ledger)) return Failure(result, RHH_PHOTO_REWARD_NOT_ELIGIBLE, "no pending reward");
    result->receipt.struct_size = sizeof(result->receipt);
    if (ReceiptAt(index, &result->receipt)) return Failure(result, RHH_PHOTO_REWARD_UNAVAILABLE, "reward ledger unavailable");
    u32 item;
    if (RhhContentResolve(RHH_ITEM, result->receipt.item, &item) || !item
        || RhhContentKeyForId(RHH_ITEM, item) != result->receipt.item || !BagReady(item))
        return Failure(result, RHH_PHOTO_REWARD_UNAVAILABLE, "reward item unavailable");
    if (!CheckBagHasSpace(item, result->receipt.quantity) || !AddBagItem(item, result->receipt.quantity))
        return Failure(result, RHH_PHOTO_REWARD_BAG_FULL, "not enough bag space");
    Write32(applied, index + 1); result->receipt.delivered = 1;
    result->status = RHH_PHOTO_REWARD_OK; result->changed = 1; result->reason[0] = 0;
    ModsSyncInventory(); Notify(RHH_PHOTO_REWARD_DELIVERED, result,
        Read32(ConstRecord(ledger, index) + 44) != REWARD_UNSCORED); return result->status;
}
static int Deliver(struct RhhPhotoRewardResult *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_PHOTO_REWARD_INVALID;
    struct RhhPhotoRewardResult result = {.struct_size = sizeof(result)};
    u32 status;
    if (sBusy) status = Failure(&result, RHH_PHOTO_REWARD_UNAVAILABLE, "reward transaction busy");
    else { sBusy = TRUE; status = DeliverNext(&result); sBusy = FALSE; }
    *out = result; return status;
}
static int PhotoForReward(const struct RhhPhotoId *id, struct RhhWildActorIndividual *individual,
                          struct RhhPhotoScore *score)
{
    const struct RhhPhotoAlbumService *album = (const void *)RhhModsHostAPI()->get_service("rhh:service/photo_album", RHH_SERVICE_VERSION(1, 0));
    const struct RhhPhotoScoreService *scores = (const void *)RhhModsHostAPI()->get_service("rhh:service/photo_score", RHH_SERVICE_VERSION(1, 1));
    struct RhhPhotoAlbumEntry entry = {.struct_size = sizeof(entry)};
    if (!album || !scores) return -1;
    if (!album->find(id, &entry))
    {
        *individual = entry.individual;
        if (scores->find(id, score))
            *score = (struct RhhPhotoScore){.struct_size = sizeof(*score), .photo = *id};
        return 0;
    }
    struct RhhPhotoRecent recent = {.struct_size = sizeof(recent)};
    if (scores->recent(&recent) || !id->serial || recent.photo.run_id != id->run_id
        || recent.photo.serial != id->serial) return -1;
    *individual = recent.individual; *score = recent.score; return 0;
}
static int Change(const struct RhhPhotoRewardRequest *request, struct RhhPhotoRewardResult *out,
                  bool32 preview, bool32 allowUnscored)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_PHOTO_REWARD_INVALID;
    struct RhhPhotoRewardResult result = {.struct_size = sizeof(result), .is_preview = preview};
    u32 status = RHH_PHOTO_REWARD_INVALID, item = 0;
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->owner
        || !RhhModsRegisteredKey(request->owner) || !request->rule || !request->item
        || !request->quantity || request->quantity > 999 || request->minimum_score > RHH_PHOTO_SCORE_MAX
        || !request->photo.run_id || !request->photo.serial
        || RhhContentResolve(RHH_ITEM, request->item, &item) || !item
        || RhhContentKeyForId(RHH_ITEM, item) != request->item)
    { Failure(&result, status, "invalid photo reward request"); goto done; }
    if (sBusy || !Ready()) { status = Failure(&result, RHH_PHOTO_REWARD_UNAVAILABLE, "reward transaction unavailable"); goto done; }
    struct RhhWildActorIndividual individual = {.struct_size = sizeof(individual)};
    struct RhhPhotoScore score = {.struct_size = sizeof(score)};
    u8 *ledger = RhhModsState(sLedgerId), *applied = RhhModsState(sAppliedId);
    const u8 *xpLedger = RhhModsState(sExperienceLedgerId), *xpApplied = RhhModsState(sExperienceAppliedId);
    if (!ledger || !applied || !xpLedger || !xpApplied || Read32(applied) > Read32(ledger)
        || Read32(xpApplied) > Read32(xpLedger)
        || request->photo.run_id != RhhModsRunId() || PhotoForReward(&request->photo, &individual, &score)
        || !individual.serial || individual.run_id != RhhModsRunId()
        || (!(score.flags & RHH_PHOTO_SCORE_AVAILABLE) && (!allowUnscored || request->minimum_score)))
    { status = Failure(&result, RHH_PHOTO_REWARD_NOT_ELIGIBLE, "eligible photo unavailable"); goto done; }
    if (score.value < request->minimum_score)
    { status = Failure(&result, RHH_PHOTO_REWARD_NOT_ELIGIBLE, "photo score is too low"); goto done; }
    u32 count = Read32(ledger);
    for (u32 i = 0; i < count; ++i)
    {
        const u8 *p = ConstRecord(ledger, i);
        if (Read64(p) == individual.serial && Read64(p + 16) == request->owner && Read64(p + 24) == request->rule)
        {
            result.receipt.struct_size = sizeof(result.receipt); ReceiptAt(i, &result.receipt);
            status = Failure(&result, RHH_PHOTO_REWARD_ALREADY, "individual already rewarded for this rule"); goto done;
        }
    }
    if (HasReward(xpLedger, individual.serial, request->owner, request->rule))
    { status = Failure(&result, RHH_PHOTO_REWARD_ALREADY, "individual already rewarded for this rule"); goto done; }
    if (ExperiencePending()) { status = Failure(&result, RHH_PHOTO_REWARD_PENDING, "deliver the pending experience reward first"); goto done; }
    if (Read32(applied) != count) { status = Failure(&result, RHH_PHOTO_REWARD_PENDING, "deliver the pending reward first"); goto done; }
    if (count == RHH_PHOTO_REWARD_CAPACITY) { status = Failure(&result, RHH_PHOTO_REWARD_FULL, "reward ledger full"); goto done; }
    if (!BagReady(item)) { status = Failure(&result, RHH_PHOTO_REWARD_UNAVAILABLE, "normal bag unavailable"); goto done; }
    if (!CheckBagHasSpace(item, request->quantity)) { status = Failure(&result, RHH_PHOTO_REWARD_BAG_FULL, "not enough bag space"); goto done; }
    result.receipt = (struct RhhPhotoRewardReceipt){.struct_size = sizeof(result.receipt), .quantity = request->quantity,
        .score = score.value, .sequence = count + 1, .photo = request->photo, .individual = individual,
        .owner = request->owner, .rule = request->rule, .item = request->item};
    result.status = RHH_PHOTO_REWARD_OK; result.changed = 1;
    struct Bag beforeBag = gSaveBlock1Ptr->bag;
    u64 run = RhhModsRunId(); u32 beforeApplied = Read32(applied);
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_CLAIM_PHOTO_REWARD,
        .run_seed = RhhModsRunSeed(), .event_key = request->rule, .subject = individual.serial,
        .ordinal = count + 1, .args = {request->quantity, score.value, preview, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBusy = TRUE; sContext = &result; sContextScoreAvailable = !!(score.flags & RHH_PHOTO_SCORE_AVAILABLE);
    RhhModsDispatch(&event, &decision); sContext = NULL;
    bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    struct RhhWildActorIndividual checkIndividual = {.struct_size = sizeof(checkIndividual)};
    struct RhhPhotoScore checkScore = {.struct_size = sizeof(checkScore)};
    if (!decision.allowed) status = Failure(&result, RHH_PHOTO_REWARD_DENIED, decision.reason[0] ? decision.reason : "reward denied");
    else if (impure) status = Failure(&result, RHH_PHOTO_REWARD_UNAVAILABLE, "reward constraint consumed RNG");
    else if (!Ready() || run != RhhModsRunId() || Read32(ledger) != count || Read32(applied) != beforeApplied
        || memcmp(&beforeBag, &gSaveBlock1Ptr->bag, sizeof(beforeBag))
        || PhotoForReward(&request->photo, &checkIndividual, &checkScore)
        || checkIndividual.serial != individual.serial || checkIndividual.run_id != individual.run_id
        || checkScore.value != score.value || checkScore.flags != score.flags)
        status = Failure(&result, RHH_PHOTO_REWARD_UNAVAILABLE, "reward inputs changed during validation");
    else if (preview) status = RHH_PHOTO_REWARD_OK;
    else
    {
        u8 *p = Record(ledger, count);
        Write64(p, individual.serial); Write64(p + 8, request->photo.serial);
        Write64(p + 16, request->owner); Write64(p + 24, request->rule); Write64(p + 32, request->item);
        Write32(p + 40, request->quantity);
        Write32(p + 44, score.flags & RHH_PHOTO_SCORE_AVAILABLE ? score.value : REWARD_UNSCORED);
        Write32(ledger, count + 1);
        if (RhhModsFlushDurable())
        {
            memset(p, 0, REWARD_RECORD); Write32(ledger, count);
            status = Failure(&result, RHH_PHOTO_REWARD_PERSISTENCE_FAILED, "could not persist reward");
        }
        else
        {
            Notify(RHH_PHOTO_REWARD_CLAIMED, &result, !!(score.flags & RHH_PHOTO_SCORE_AVAILABLE));
            status = DeliverNext(&result);
            if (status != RHH_PHOTO_REWARD_OK)
            { result.status = status = RHH_PHOTO_REWARD_PENDING; result.changed = 1; }
        }
    }
    sBusy = FALSE;
done:
    result.status = status; *out = result; return status;
}
static int Preview(const struct RhhPhotoRewardRequest *r, struct RhhPhotoRewardResult *out) { return Change(r, out, TRUE, FALSE); }
static int Claim(const struct RhhPhotoRewardRequest *r, struct RhhPhotoRewardResult *out) { return Change(r, out, FALSE, FALSE); }
static int PreviewAllowUnscored(const struct RhhPhotoRewardRequest *r, struct RhhPhotoRewardResult *out) { return Change(r, out, TRUE, TRUE); }
static int ClaimAllowUnscored(const struct RhhPhotoRewardRequest *r, struct RhhPhotoRewardResult *out) { return Change(r, out, FALSE, TRUE); }
static u32 ExperienceCount(void)
{
    const u8 *ledger = RhhModsState(sExperienceLedgerId);
    return RhhModsHasActiveRun() && ledger ? Read32(ledger) : 0;
}
static int ExperienceAt(u32 index, struct RhhPhotoExperienceReceipt *out)
{
    const u8 *ledger = RhhModsState(sExperienceLedgerId), *applied = RhhModsState(sExperienceAppliedId);
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || !ledger || !applied
        || index >= Read32(ledger) || Read32(applied) > Read32(ledger)) return -1;
    const u8 *p = ConstRecord(ledger, index); u64 run = RhhModsRunId(), recipient; u32 score = Read32(p + 44);
    if (ExperienceRecipientIdentity(ledger, index, &recipient) || ValidateExperienceApplied(applied, EXPERIENCE_APPLIED_SIZE)) return -1;
    if (index < Read32(applied))
    {
        recipient = Read64(applied + REWARD_HEADER + index * 8);
        if (!recipient) recipient = Read64(p + 32); /* A pre-retarget save delivered to the original recipient. */
    }
    *out = (struct RhhPhotoExperienceReceipt){.struct_size = sizeof(*out), .amount = Read32(p + 40),
        .score = score == REWARD_UNSCORED ? 0 : score, .score_available = score != REWARD_UNSCORED,
        .delivered = index < Read32(applied), .sequence = index + 1, .recipient_identity = recipient,
        .photo = {run, Read64(p + 8)},
        .individual = {.struct_size = sizeof(struct RhhWildActorIndividual), .run_id = run, .serial = Read64(p)},
        .owner = Read64(p + 16), .rule = Read64(p + 24)};
    return 0;
}
static int ExperienceContext(struct RhhPhotoExperienceResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sExperienceContext
        || (event != RHH_CAN_CLAIM_PHOTO_EXPERIENCE && event != RHH_PHOTO_EXPERIENCE_CLAIMED
            && event != RHH_PHOTO_EXPERIENCE_DELIVERED)) return -1;
    *out = *sExperienceContext; return 0;
}
static u32 ExperienceFailure(struct RhhPhotoExperienceResult *result, u32 status, const char *reason)
{
    result->status = status; result->changed = 0;
    snprintf(result->reason, sizeof(result->reason), "%s", reason); return status;
}
static void ExperienceNotify(u32 kind, struct RhhPhotoExperienceResult *result)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = result->receipt.rule, .subject = result->receipt.individual.serial,
        .ordinal = result->receipt.sequence, .args = {result->receipt.amount, result->receipt.score, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sExperienceContext = result; RhhModsDispatch(&event, &decision); sExperienceContext = NULL;
}
static const struct RhhOwnedMonService *ExperienceOwned(void)
{ return (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 29)); }
static int ExperienceRecipient(u64 identity, struct RhhOwnedExperience *out)
{
    const struct RhhOwnedMonService *owned = ExperienceOwned();
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    if (!owned || !identity || owned->identity_query(identity, &query) || !query.unique_valid
        || query.unique.location != RHH_MON_PARTY || query.unique.is_egg
        || owned->experience(&query.unique, out)) return -1;
    return 0;
}
static u32 DeliverExperienceNext(struct RhhPhotoExperienceResult *result)
{
    u8 *ledger = RhhModsState(sExperienceLedgerId), *applied = RhhModsState(sExperienceAppliedId);
    const u8 *itemLedger = RhhModsState(sLedgerId), *itemApplied = RhhModsState(sAppliedId);
    if (!Ready() || !ledger || !applied || Read32(applied) > Read32(ledger) || sExperienceDelivery.active)
        return ExperienceFailure(result, RHH_PHOTO_REWARD_UNAVAILABLE, "experience delivery unavailable");
    if (!itemLedger || !itemApplied || Read32(itemApplied) != Read32(itemLedger))
        return ExperienceFailure(result, RHH_PHOTO_REWARD_PENDING, "deliver the pending item reward first");
    u32 index = Read32(applied);
    if (index == Read32(ledger)) return ExperienceFailure(result, RHH_PHOTO_REWARD_NOT_ELIGIBLE, "no pending experience reward");
    result->receipt.struct_size = sizeof(result->receipt);
    if (ExperienceAt(index, &result->receipt)) return ExperienceFailure(result, RHH_PHOTO_REWARD_UNAVAILABLE, "experience ledger unavailable");
    struct RhhOwnedExperience recipient = {.struct_size = sizeof(recipient)};
    const struct RhhOwnedMonService *owned = ExperienceOwned();
    if (ExperienceRecipient(result->receipt.recipient_identity, &recipient))
        return ExperienceFailure(result, RHH_PHOTO_REWARD_UNAVAILABLE, "recipient unavailable or ambiguous");
    struct RhhExperienceAwardRequest request = {.struct_size = sizeof(request), .amount = result->receipt.amount,
        .owner = RhhContentKeyOf(sExperienceLedgerId), .expected = recipient};
    struct RhhExperienceAwardResult award = {.struct_size = sizeof(award)};
    if (owned->award_experience(&request, &award) != RHH_EXPERIENCE_AWARD_PENDING)
        return ExperienceFailure(result, RHH_PHOTO_REWARD_UNAVAILABLE, "experience award unavailable");
    result->experience = award.experience; result->status = RHH_PHOTO_REWARD_PENDING; result->changed = 0;
    sExperienceDelivery.active = TRUE; sExperienceDelivery.delivered = FALSE;
    sExperienceDelivery.ticket = award.ticket; sExperienceDelivery.run = RhhModsRunId();
    sExperienceDelivery.index = index; sExperienceDelivery.result = *result;
    return result->status;
}
static int DeliverExperience(struct RhhPhotoExperienceResult *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_PHOTO_REWARD_INVALID;
    struct RhhPhotoExperienceResult result = {.struct_size = sizeof(result)};
    u32 status;
    if (sBusy) status = ExperienceFailure(&result, RHH_PHOTO_REWARD_UNAVAILABLE, "reward transaction busy");
    else { sBusy = TRUE; sExperienceRetryBlocked = FALSE; status = DeliverExperienceNext(&result); sBusy = FALSE; }
    *out = result; return status;
}
int RhhModsPhotoExperienceCommit(u64 ticket, u64 recipient)
{
    if (!sExperienceDelivery.active || sExperienceDelivery.ticket != ticket) return 0;
    u8 *ledger = RhhModsState(sExperienceLedgerId), *applied = RhhModsState(sExperienceAppliedId);
    u32 index = sExperienceDelivery.index;
    const struct RhhOwnedMonService *owned = ExperienceOwned();
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    u64 currentRecipient;
    if (!ledger || !applied || sExperienceDelivery.delivered || sExperienceDelivery.run != RhhModsRunId()
        || index >= Read32(ledger) || Read32(applied) != index
        || ExperienceRecipientIdentity(ledger, index, &currentRecipient) || currentRecipient != recipient || !owned
        || owned->identity_query(recipient, &query) || !query.unique_valid || query.unique.location != RHH_MON_PARTY
        || query.unique.slot != sExperienceDelivery.result.experience.mon.slot) return -1;
    Write64(applied + REWARD_HEADER + index * 8, recipient);
    Write32(applied, index + 1);
    sExperienceDelivery.delivered = TRUE;
    sExperienceDelivery.result.receipt.delivered = 1;
    sExperienceDelivery.result.status = RHH_PHOTO_REWARD_OK; sExperienceDelivery.result.changed = 1;
    return 0;
}
void RhhModsPhotoExperienceNotify(u64 ticket)
{
    if (sExperienceDelivery.active && sExperienceDelivery.delivered && sExperienceDelivery.ticket == ticket)
        ExperienceNotify(RHH_PHOTO_EXPERIENCE_DELIVERED, &sExperienceDelivery.result);
}
void RhhModsPhotoExperienceFinish(u64 ticket)
{
    if (sExperienceDelivery.active && sExperienceDelivery.ticket == ticket)
    {
        bool32 retryBlocked = !sExperienceDelivery.delivered;
        ResetExperienceDelivery(); sExperienceRetryBlocked = retryBlocked;
    }
}
static int ChangeExperience(const struct RhhPhotoExperienceRequest *request, struct RhhPhotoExperienceResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_PHOTO_REWARD_INVALID;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhPhotoExperienceResult result = {.struct_size = sizeof(result), .is_preview = preview,
        .experience.struct_size = sizeof(result.experience)};
    u32 status = RHH_PHOTO_REWARD_INVALID;
    if (!request || request->struct_size < sizeof(*request) || !request->owner || !RhhModsRegisteredKey(request->owner)
        || !request->rule || !request->amount || request->minimum_score > RHH_PHOTO_SCORE_MAX
        || request->recipient.struct_size < sizeof(request->recipient)
        || request->recipient.mon.struct_size < sizeof(request->recipient.mon)
        || request->recipient.reserved[0] || request->recipient.reserved[1]
        || (request->flags & ~RHH_PHOTO_EXPERIENCE_ALLOW_UNSCORED) || !request->photo.run_id || !request->photo.serial)
    { ExperienceFailure(&result, status, "invalid photo experience request"); goto done; }
    struct RhhPhotoExperienceRequest copy = *request; request = &copy;
    if (sBusy || !Ready()) { status = ExperienceFailure(&result, RHH_PHOTO_REWARD_UNAVAILABLE, "reward transaction unavailable"); goto done; }
    struct RhhWildActorIndividual individual = {.struct_size = sizeof(individual)};
    struct RhhPhotoScore score = {.struct_size = sizeof(score)};
    u8 *ledger = RhhModsState(sExperienceLedgerId), *applied = RhhModsState(sExperienceAppliedId);
    const u8 *itemLedger = RhhModsState(sLedgerId), *itemApplied = RhhModsState(sAppliedId);
    if (!ledger || !applied || !itemLedger || !itemApplied || Read32(applied) > Read32(ledger)
        || Read32(itemApplied) > Read32(itemLedger) || request->photo.run_id != RhhModsRunId()
        || PhotoForReward(&request->photo, &individual, &score) || !individual.serial || individual.run_id != RhhModsRunId()
        || (!(score.flags & RHH_PHOTO_SCORE_AVAILABLE)
            && (!(request->flags & RHH_PHOTO_EXPERIENCE_ALLOW_UNSCORED) || request->minimum_score)))
    { status = ExperienceFailure(&result, RHH_PHOTO_REWARD_NOT_ELIGIBLE, "eligible photo unavailable"); goto done; }
    if (score.value < request->minimum_score)
    { status = ExperienceFailure(&result, RHH_PHOTO_REWARD_NOT_ELIGIBLE, "photo score is too low"); goto done; }
    u32 count = Read32(ledger);
    if (HasReward(ledger, individual.serial, request->owner, request->rule)
        || HasReward(itemLedger, individual.serial, request->owner, request->rule))
    { status = ExperienceFailure(&result, RHH_PHOTO_REWARD_ALREADY, "individual already rewarded for this rule"); goto done; }
    if (Read32(applied) != count || Read32(itemApplied) != Read32(itemLedger))
    { status = ExperienceFailure(&result, RHH_PHOTO_REWARD_PENDING, "deliver the pending reward first"); goto done; }
    if (count == RHH_PHOTO_REWARD_CAPACITY)
    { status = ExperienceFailure(&result, RHH_PHOTO_REWARD_FULL, "experience ledger full"); goto done; }
    const struct RhhOwnedMonService *owned = ExperienceOwned();
    struct RhhOwnedExperience recipient = {.struct_size = sizeof(recipient)};
    if (!owned || ExperienceRecipient(request->recipient.mon.identity, &recipient)
        || !RhhMonSnapshotsEqual(&request->recipient.mon, &recipient.mon)
        || recipient.experience != request->recipient.experience
        || !GetMonData(&gParties[B_TRAINER_PLAYER][recipient.mon.slot], MON_DATA_HP)
        || owned->preview_experience(&recipient, request->amount, &result.experience) || !result.experience.applied
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
    { status = ExperienceFailure(&result, RHH_PHOTO_REWARD_UNAVAILABLE, "recipient or award unavailable"); goto done; }
    result.receipt = (struct RhhPhotoExperienceReceipt){.struct_size = sizeof(result.receipt), .amount = request->amount,
        .score = score.value, .score_available = !!(score.flags & RHH_PHOTO_SCORE_AVAILABLE), .sequence = count + 1,
        .recipient_identity = recipient.mon.identity, .photo = request->photo, .individual = individual,
        .owner = request->owner, .rule = request->rule};
    result.status = RHH_PHOTO_REWARD_OK; result.changed = 1;
    u64 run = RhhModsRunId();
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_CLAIM_PHOTO_EXPERIENCE,
        .run_seed = RhhModsRunSeed(), .event_key = request->rule, .subject = individual.serial,
        .ordinal = count + 1, .args = {request->amount, score.value, preview, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sBusy = TRUE; sExperienceContext = &result; RhhModsDispatch(&event, &decision); sExperienceContext = NULL;
    struct RhhWildActorIndividual checkIndividual = {.struct_size = sizeof(checkIndividual)};
    struct RhhPhotoScore checkScore = {.struct_size = sizeof(checkScore)};
    struct RhhOwnedExperience checkRecipient = {.struct_size = sizeof(checkRecipient)};
    struct RhhExperiencePreview checkPreview = {.struct_size = sizeof(checkPreview)};
    if (!decision.allowed) status = ExperienceFailure(&result, RHH_PHOTO_REWARD_DENIED, decision.reason[0] ? decision.reason : "reward denied");
    else if (!Ready() || run != RhhModsRunId() || Read32(ledger) != count || Read32(applied) != count
        || Read32(itemApplied) != Read32(itemLedger) || PhotoForReward(&request->photo, &checkIndividual, &checkScore)
        || checkIndividual.serial != individual.serial || checkIndividual.run_id != individual.run_id
        || checkScore.value != score.value || checkScore.flags != score.flags
        || ExperienceRecipient(recipient.mon.identity, &checkRecipient)
        || !RhhMonSnapshotsEqual(&recipient.mon, &checkRecipient.mon) || recipient.experience != checkRecipient.experience
        || !GetMonData(&gParties[B_TRAINER_PLAYER][checkRecipient.mon.slot], MON_DATA_HP)
        || owned->preview_experience(&checkRecipient, request->amount, &checkPreview)
        || memcmp(&checkPreview, &result.experience, sizeof(checkPreview))
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
        status = ExperienceFailure(&result, RHH_PHOTO_REWARD_UNAVAILABLE, "reward inputs changed during validation");
    else if (preview) status = RHH_PHOTO_REWARD_OK;
    else
    {
        u8 *p = Record(ledger, count);
        Write64(p, individual.serial); Write64(p + 8, request->photo.serial); Write64(p + 16, request->owner);
        Write64(p + 24, request->rule); Write64(p + 32, recipient.mon.identity); Write32(p + 40, request->amount);
        Write32(p + 44, score.flags & RHH_PHOTO_SCORE_AVAILABLE ? score.value : REWARD_UNSCORED);
        Write32(ledger, count + 1);
        if (RhhModsFlushDurable())
        {
            memset(p, 0, REWARD_RECORD); Write32(ledger, count);
            status = ExperienceFailure(&result, RHH_PHOTO_REWARD_PERSISTENCE_FAILED, "could not persist reward");
        }
        else
        {
            ExperienceNotify(RHH_PHOTO_EXPERIENCE_CLAIMED, &result);
            status = DeliverExperienceNext(&result);
            if (status != RHH_PHOTO_REWARD_OK)
            { result.status = status = RHH_PHOTO_REWARD_PENDING; result.changed = 1; }
        }
    }
    sBusy = FALSE;
done:
    if (status != RHH_PHOTO_REWARD_OK && status != RHH_PHOTO_REWARD_PENDING)
    { gRngValue = rng; gRng2Value = rng2; }
    result.status = status; *out = result; return status;
}
static int PreviewExperience(const struct RhhPhotoExperienceRequest *r, struct RhhPhotoExperienceResult *out)
{ return ChangeExperience(r, out, TRUE); }
static int ClaimExperience(const struct RhhPhotoExperienceRequest *r, struct RhhPhotoExperienceResult *out)
{ return ChangeExperience(r, out, FALSE); }
static bool32 SameExperienceReceipt(const struct RhhPhotoExperienceReceipt *a, const struct RhhPhotoExperienceReceipt *b)
{
    return a->struct_size >= sizeof(*a) && a->individual.struct_size >= sizeof(a->individual)
        && !a->reserved && !a->individual.reserved
        && a->amount == b->amount && a->score == b->score && a->score_available == b->score_available
        && a->delivered == b->delivered && a->sequence == b->sequence && a->recipient_identity == b->recipient_identity
        && a->photo.run_id == b->photo.run_id && a->photo.serial == b->photo.serial
        && a->individual.run_id == b->individual.run_id && a->individual.serial == b->individual.serial
        && a->owner == b->owner && a->rule == b->rule;
}
static int RetargetContext(struct RhhPhotoExperienceRetargetResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sRetargetContext
        || (event != RHH_CAN_RETARGET_PHOTO_EXPERIENCE && event != RHH_PHOTO_EXPERIENCE_RETARGETED)) return -1;
    *out = *sRetargetContext; return 0;
}
static int RetargetExperience(const struct RhhPhotoExperienceRetargetRequest *request,
                              struct RhhPhotoExperienceRetargetResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_PHOTO_REWARD_INVALID;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhPhotoExperienceRetargetResult result = {.struct_size = sizeof(result), .is_preview = preview,
        .before.struct_size = sizeof(result.before), .experience.struct_size = sizeof(result.experience)};
    u32 status = RHH_PHOTO_REWARD_INVALID;
    if (!request || request->struct_size < sizeof(*request) || request->reserved
        || request->expected.struct_size < sizeof(request->expected)
        || request->expected.individual.struct_size < sizeof(request->expected.individual)
        || request->expected.reserved || request->expected.individual.reserved
        || !request->expected.sequence || request->expected.sequence > RHH_PHOTO_REWARD_CAPACITY
        || request->recipient.struct_size < sizeof(request->recipient)
        || request->recipient.mon.struct_size < sizeof(request->recipient.mon)
        || request->recipient.reserved[0] || request->recipient.reserved[1]) goto done;
    struct RhhPhotoExperienceRetargetRequest copy = *request; request = &copy;
    if (sBusy || sExperienceDelivery.active || !Ready()) { status = RHH_PHOTO_REWARD_UNAVAILABLE; goto done; }
    u8 *ledger = RhhModsState(sExperienceLedgerId), *applied = RhhModsState(sExperienceAppliedId);
    u8 *changes = RhhModsState(sExperienceRetargetId);
    u32 index = request->expected.sequence - 1;
    if (!ledger || !applied || !changes || ExperienceAt(index, &result.before)
        || !SameExperienceReceipt(&request->expected, &result.before))
    { status = RHH_PHOTO_REWARD_STALE; goto done; }
    if (result.before.delivered || Read32(applied) != index)
    { status = RHH_PHOTO_REWARD_NOT_ELIGIBLE; goto done; }
    u32 count = Read32(changes);
    bool32 changed = request->recipient.mon.identity != result.before.recipient_identity;
    if (changed && count == RHH_PHOTO_EXPERIENCE_RETARGET_CAPACITY) { status = RHH_PHOTO_REWARD_FULL; goto done; }
    const struct RhhOwnedMonService *owned = ExperienceOwned();
    struct RhhOwnedExperience recipient = {.struct_size = sizeof(recipient)};
    if (!owned || ExperienceRecipient(request->recipient.mon.identity, &recipient)
        || !RhhMonSnapshotsEqual(&request->recipient.mon, &recipient.mon)
        || request->recipient.experience != recipient.experience
        || !GetMonData(&gParties[B_TRAINER_PLAYER][recipient.mon.slot], MON_DATA_HP)
        || owned->preview_experience(&recipient, result.before.amount, &result.experience) || !result.experience.applied
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
    { status = RHH_PHOTO_REWARD_UNAVAILABLE; goto done; }
    result.after = result.before; result.after.recipient_identity = recipient.mon.identity;
    result.status = RHH_PHOTO_REWARD_OK; result.changed = changed;
    u64 run = RhhModsRunId();
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_RETARGET_PHOTO_EXPERIENCE,
        .run_seed = RhhModsRunSeed(), .event_key = result.before.rule, .subject = result.before.individual.serial,
        .ordinal = result.before.sequence, .args = {preview, changed, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sBusy = TRUE; sRetargetContext = &result; RhhModsDispatch(&event, &decision); sRetargetContext = NULL;
    struct RhhPhotoExperienceReceipt current = {.struct_size = sizeof(current)};
    struct RhhOwnedExperience checkRecipient = {.struct_size = sizeof(checkRecipient)};
    struct RhhExperiencePreview checkPreview = {.struct_size = sizeof(checkPreview)};
    if (!decision.allowed)
    { status = RHH_PHOTO_REWARD_DENIED; snprintf(result.reason, sizeof(result.reason), "%s", decision.reason); }
    else if (!Ready() || sExperienceDelivery.active || run != RhhModsRunId() || Read32(changes) != count
        || ExperienceAt(index, &current) || !SameExperienceReceipt(&result.before, &current)
        || Read32(applied) != index || ExperienceRecipient(recipient.mon.identity, &checkRecipient)
        || !RhhMonSnapshotsEqual(&recipient.mon, &checkRecipient.mon) || recipient.experience != checkRecipient.experience
        || !GetMonData(&gParties[B_TRAINER_PLAYER][checkRecipient.mon.slot], MON_DATA_HP)
        || owned->preview_experience(&checkRecipient, result.before.amount, &checkPreview)
        || memcmp(&checkPreview, &result.experience, sizeof(checkPreview))
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
        status = RHH_PHOTO_REWARD_STALE;
    else if (preview) status = RHH_PHOTO_REWARD_OK;
    else if (!changed) { sExperienceRetryBlocked = FALSE; status = RHH_PHOTO_REWARD_OK; }
    else
    {
        u8 *p = changes + REWARD_HEADER + count * RETARGET_RECORD;
        Write32(p, index + 1); Write64(p + 8, recipient.mon.identity); Write32(changes, count + 1);
        if (RhhModsFlushDurable())
        { memset(p, 0, RETARGET_RECORD); Write32(changes, count); status = RHH_PHOTO_REWARD_PERSISTENCE_FAILED; }
        else
        {
            sExperienceRetryBlocked = FALSE; status = RHH_PHOTO_REWARD_OK;
            event.kind = RHH_PHOTO_EXPERIENCE_RETARGETED;
            decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
            sRetargetContext = &result; RhhModsDispatch(&event, &decision); sRetargetContext = NULL;
        }
    }
    sBusy = FALSE;
done:
    if (status != RHH_PHOTO_REWARD_OK) { result.changed = 0; gRngValue = rng; gRng2Value = rng2; }
    result.status = status; *out = result; return status;
}
static int PreviewExperienceRetarget(const struct RhhPhotoExperienceRetargetRequest *r, struct RhhPhotoExperienceRetargetResult *out)
{ return RetargetExperience(r, out, TRUE); }
static int CommitExperienceRetarget(const struct RhhPhotoExperienceRetargetRequest *r, struct RhhPhotoExperienceRetargetResult *out)
{ return RetargetExperience(r, out, FALSE); }
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    const u8 *ledger = RhhModsState(sLedgerId), *applied = RhhModsState(sAppliedId);
    if (!sBusy && ledger && applied && Read32(applied) < Read32(ledger) && Ready())
    { struct RhhPhotoRewardResult result = {.struct_size = sizeof(result)}; Deliver(&result); }
    else if (!sBusy && !sExperienceDelivery.active && !sExperienceRetryBlocked && ExperiencePending() && Ready())
    { struct RhhPhotoExperienceResult result = {.struct_size = sizeof(result)}; DeliverExperience(&result); }
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update}
};
static const struct RhhModDefinition sLedger = {.struct_size = sizeof(sLedger), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sLedgerId, .flags = RHH_MOD_REQUIRED | RHH_MOD_DURABLE,
    .state_size = REWARD_LEDGER_SIZE, .validate = ValidateLedger, .merge = MergeLedger,
    .hook_count = ARRAY_COUNT(sHooks), .hooks = sHooks};
static const struct RhhModDefinition sApplied = {.struct_size = sizeof(sApplied), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sAppliedId, .flags = RHH_MOD_REQUIRED,
    .state_size = 8, .validate = ValidateApplied};
static const struct RhhModDefinition sExperienceLedger = {.struct_size = sizeof(sExperienceLedger), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sExperienceLedgerId, .flags = RHH_MOD_REQUIRED | RHH_MOD_DURABLE,
    .state_size = REWARD_LEDGER_SIZE, .validate = ValidateExperienceLedger, .merge = MergeExperienceLedger};
static const struct RhhModDefinition sExperienceApplied = {.struct_size = sizeof(sExperienceApplied), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 2, .state_schema = 2, .id = sExperienceAppliedId, .flags = RHH_MOD_REQUIRED,
    .state_size = EXPERIENCE_APPLIED_SIZE, .validate = ValidateExperienceApplied, .migrate = MigrateExperienceApplied};
static const struct RhhModDefinition sExperienceRetarget = {.struct_size = sizeof(sExperienceRetarget), .abi_version = RHH_MOD_ABI_V1,
    .mod_version = 1, .state_schema = 1, .id = sExperienceRetargetId, .flags = RHH_MOD_REQUIRED | RHH_MOD_DURABLE,
    .state_size = RETARGET_SIZE, .validate = ValidateRetargetLedger, .merge = MergeRetargetLedger};

/* Ordinary-save claims use the owner's archive; no durable journal or actor
 * identity is needed. The record payload detects an unlikely key collision. */
static int SimpleClaim(const struct RhhPhotoRewardRequest *request)
{
    u32 item;
    if (!request || request->struct_size < sizeof(*request) || request->reserved
        || !request->owner || !request->rule || !request->item || !request->quantity
        || request->quantity > 999 || request->minimum_score > RHH_PHOTO_SCORE_MAX
        || !request->photo.run_id || !request->photo.serial
        || RhhContentResolve(RHH_ITEM, request->item, &item) || !item
        || RhhContentKeyForId(RHH_ITEM, item) != request->item) return RHH_PHOTO_REWARD_INVALID;
    if (sBusy || !RhhModsHasActiveRun() || !RhhModsCommandsAllowed()
        || RhhModsCurrentEvent() != RHH_MOD_UPDATE || RhhModsCurrentOwner() != request->owner
        || !RhhModsRegisteredKey(request->owner) || NativeSaveTransactionActive()
        || !gSaveBlock1Ptr || !gSaveBlock2Ptr || gMain.inBattle || gMain.callback2 != CB2_Overworld
        || gPaletteFade.active || gPaletteFade.y || ScriptContext_IsEnabled() || ArePlayerFieldControlsLocked()
        || IsOverworldLinkActive() || CurrentBattlePyramidLocation() != PYRAMID_LOCATION_NONE
        || FlagGet(FLAG_STORING_ITEMS_IN_PYRAMID_BAG)) return RHH_PHOTO_REWARD_UNAVAILABLE;
    const struct RhhPhotoAlbumService *album = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/photo_album", RHH_SERVICE_VERSION(1, 0));
    const struct RhhPhotoScoreService *scores = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/photo_score", RHH_SERVICE_VERSION(1, 0));
    const struct RhhModArchiveService *archive = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/mod_archive", RHH_SERVICE_VERSION(1, 0));
    struct RhhPhotoAlbumEntry photo = {.struct_size = sizeof(photo)};
    struct RhhPhotoScore score = {.struct_size = sizeof(score)};
    struct RhhModArchiveUsage usage = {.struct_size = sizeof(usage)};
    if (!album || !archive || request->photo.run_id != RhhModsRunId()
        || album->find(&request->photo, &photo) || !photo.map || !photo.species
        || archive->usage(request->owner, &usage)) return RHH_PHOTO_REWARD_NOT_ELIGIBLE;
    if (request->minimum_score && (!scores || scores->find(&request->photo, &score)
        || score.value < request->minimum_score)) return RHH_PHOTO_REWARD_NOT_ELIGIBLE;
    u8 record[32] = {0};
    memcpy(record, "RHPSIMP1", 8); Write64(record + 8, photo.map);
    Write64(record + 16, photo.species); Write64(record + 24, request->rule);
    u64 key = UINT64_C(14695981039346656037);
    for (u32 i = 0; i < sizeof(record); ++i) key = (key ^ record[i]) * UINT64_C(1099511628211);
    if (!key) key = 1;
    struct RhhModArchiveRecord existing = {.struct_size = sizeof(existing)};
    if (!archive->find(request->owner, key, &existing))
    {
        u8 saved[sizeof(record)];
        if (existing.size != sizeof(saved) || archive->copy(request->owner, key, existing.revision, saved, sizeof(saved))
            || memcmp(saved, record, sizeof(saved))) return RHH_PHOTO_REWARD_INVALID;
        return RHH_PHOTO_REWARD_ALREADY;
    }
    if (!BagReady(item)) return RHH_PHOTO_REWARD_UNAVAILABLE;
    if (!CheckBagHasSpace(item, request->quantity)) return RHH_PHOTO_REWARD_BAG_FULL;
    struct Bag before = gSaveBlock1Ptr->bag;
    if (!AddBagItem(item, request->quantity)) return RHH_PHOTO_REWARD_BAG_FULL;
    struct RhhModArchiveRecord created = {.struct_size = sizeof(created)};
    if (archive->put(request->owner, key, 0, record, sizeof(record), &created))
    { gSaveBlock1Ptr->bag = before; return RHH_PHOTO_REWARD_UNAVAILABLE; }
    return RHH_PHOTO_REWARD_OK;
}
static const struct RhhSimplePhotoRewardService sSimpleService = {
    {sizeof(sSimpleService), RHH_SERVICE_VERSION(1, 0)}, SimpleClaim
};
static const struct RhhPhotoRewardService sService = {
    {sizeof(sService), RHH_SERVICE_VERSION(1, 3)}, Count, ReceiptAt, Preview, Claim, Deliver, Context,
    PreviewAllowUnscored, ClaimAllowUnscored, ScoreAvailable, ContextScoreAvailable,
    ExperienceCount, ExperienceAt, PreviewExperience, ClaimExperience, DeliverExperience, ExperienceContext,
    PreviewExperienceRetarget, CommitExperienceRetarget, RetargetContext
};
int RhhModsInstallPhotoRewardService(void)
{
    sBusy = FALSE; sContext = NULL;
    ResetExperienceDelivery(); sExperienceContext = NULL; sRetargetContext = NULL;
    return RhhModsSetLifecycleCallback(ResetExperienceDelivery)
        || RhhModsRegister(&sLedger) || RhhModsRegister(&sApplied)
        || RhhModsRegister(&sExperienceLedger) || RhhModsRegister(&sExperienceApplied) || RhhModsRegister(&sExperienceRetarget)
        || RhhModsAddService("rhh:service/photo_reward", &sService.header)
        || RhhModsAddService("rhh:service/photo_reward_simple", &sSimpleService.header) ? -1 : 0;
}
