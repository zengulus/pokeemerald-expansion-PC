/* Copied views and validated edits of owned Pokémon for native modules. */
#include <stdlib.h>
#include <stdio.h>
#include "global.h"
#include "main.h"
#include "link.h"
#include "pokemon.h"
#include "move.h"
#include "mods.h"
#include "pokemon_storage_system.h"
#include "daycare.h"
#include "event_data.h"
#include "mod_host.h"
#include "native.h"
#include "nature_storage.h"
#include "owned_identity_ledger.h"
#include "music_sha256.h"
#include "item.h"
#include "money.h"
#include "battle_pyramid.h"
#include "battle.h"
#include "overworld.h"
#include "script.h"
#include "palette.h"
#include "caps.h"
#include "party_menu.h"
#include "mod_ui.h"
#include "secret_base.h"
#include "trade.h"
#include "pokeblock.h"
#include "pokeball.h"
#include "recorded_battle.h"
#include "save.h"
#include "gba/flash_internal.h"
#include "constants/party_menu.h"
#include "constants/items.h"
#include "constants/characters.h"
#include "constants/moves.h"
#include "constants/tv.h"

static bool32 sSkyPartyActive, sSkyPartyPreparing, sSkyPartyValid;
static bool32 sUnionPartyActive, sUnionPartyValid, sUnionPartyPreparing;
static bool32 sRentalPartyActive;
static bool32 sRecordedPartyActive;
enum { LINK_PROJECTION_UNION = 1, LINK_PROJECTION_MULTI = 2, LINK_PROJECTION_FRONTIER = 3 };
static u8 sLinkProjectionKind;
static u8 sUnionPartyCount, sUnionSavedPartyCount, sUnionPartySlots[PARTY_SIZE];
static u64 sUnionPartyIdentity[PARTY_SIZE];
enum { PROJECTION_SKY = 1, PROJECTION_FOLLOWER = 2 };
static u8 sPartyProjectionKind;
static bool32 sFollowerCatchFull, sFollowerCatchLoaded;
static u8 sSkyPartySlots[PARTY_SIZE], sSkyPartyCount;
static u32 sSkyPartyMask;
static u32 sSkyPartyPersonality[PARTY_SIZE], sSkyPartyOt[PARTY_SIZE];
static u8 sSkyPartyOccupied[PARTY_SIZE];
bool32 ModsOwnedProjectedPartyActive(void) { return sSkyPartyActive || sUnionPartyActive || sRentalPartyActive || sRecordedPartyActive; }
bool32 ModsOwnedRentalPartyActive(void) { return sRentalPartyActive; }
void ModsOwnedRentalPartyBegin(void)
{
    if (sSkyPartyActive || sUnionPartyActive || sRecordedPartyActive) NativeOwnedTrackerUnsupported();
    NativeOwnedTrackRentalBegin();
    sRentalPartyActive = TRUE;
}
void ModsOwnedRentalPartyBeforeRestore(void)
{ if (sRentalPartyActive) NativeOwnedTrackRentalBeforeRestore(); }
void ModsOwnedRentalPartyAfterRestore(void)
{
    if (!sRentalPartyActive) return;
    NativeOwnedTrackRentalRestored(); sRentalPartyActive = FALSE;
}
void ModsOwnedRecordedPartyBegin(const struct Pokemon *backup, u32 count)
{
    if (sSkyPartyActive || sUnionPartyActive || sRentalPartyActive || sRecordedPartyActive) NativeOwnedTrackerUnsupported();
    NativeOwnedTrackRecordedBegin(backup, count); sRecordedPartyActive = TRUE;
}
void ModsOwnedRecordedPartyBeforeRestore(const struct Pokemon *backup)
{ if (sRecordedPartyActive) NativeOwnedTrackRecordedBeforeRestore(backup); }
void ModsOwnedRecordedPartyAfterRestore(void)
{
    if (!sRecordedPartyActive) return;
    NativeOwnedTrackRecordedRestored(); sRecordedPartyActive = FALSE;
}
void ModsOwnedBeforePartyReduction(void)
{
    if ((sUnionPartyActive && sUnionPartyPreparing)
        || (ModsOwnedFollowerPartyActive() && (sSkyPartyPreparing || (sFollowerCatchFull && sFollowerCatchLoaded))))
        return;
    ModsOwnedProjectedPartyInvalidate();
    NativeOwnedTrackerUnsupported();
}
void ModsOwnedProjectedPartyInvalidate(void)
{ if (sSkyPartyActive) sSkyPartyValid = FALSE; if (sUnionPartyActive) sUnionPartyValid = FALSE; }
bool32 ModsOwnedSkyPartyActive(void) { return sSkyPartyActive && sPartyProjectionKind == PROJECTION_SKY; }
bool32 ModsOwnedFollowerPartyActive(void) { return sSkyPartyActive && sPartyProjectionKind == PROJECTION_FOLLOWER; }
bool32 ModsOwnedFollowerCatchFullActive(void) { return ModsOwnedFollowerPartyActive() && sFollowerCatchFull; }
bool32 ModsOwnedSkyPartyPreparing(void) { return ModsOwnedSkyPartyActive() && sSkyPartyPreparing; }
static bool32 SkyProjectionMatches(void);
bool32 ModsOwnedFollowerPartyReady(void)
{ return ModsOwnedFollowerPartyActive() && sSkyPartyValid && !sSkyPartyPreparing && SkyProjectionMatches(); }
static void BeginPartyProjection(u32 mask, u8 kind, bool32 validOrder)
{
    bool32 nested = sSkyPartyActive;
    sSkyPartyActive = sSkyPartyPreparing = TRUE; sPartyProjectionKind = kind; sSkyPartyMask = mask; sSkyPartyCount = 0;
    sSkyPartyValid = !nested && !sUnionPartyActive && !sRentalPartyActive && !sRecordedPartyActive && validOrder && mask && !(mask & ~((1u << PARTY_SIZE) - 1)) && !gMain.inBattle
        && !NativeSaveTransactionActive() && !ModsOwnedStorageBusy() && !ModsOwnedBattlePartyViewActive()
        && !ModsOwnedTradePcHeld() && !ModsOwnedLinkTradeActive() && gSaveBlock1Ptr && gPokemonStoragePtr;
    for (u32 slot = 0; slot < PARTY_SIZE; ++slot)
    {
        struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][slot].box;
        sSkyPartyOccupied[slot] = mon->hasSpecies;
        sSkyPartyPersonality[slot] = mon->personality; sSkyPartyOt[slot] = mon->otId;
        if (mask & (1u << slot))
        {
            if (!mon->hasSpecies) sSkyPartyValid = FALSE;
            sSkyPartySlots[sSkyPartyCount++] = slot;
        }
    }
    if (!sSkyPartyValid) NativeOwnedTrackerUnsupported();
    else NativeOwnedTrackSkyBegin(mask);
}
void ModsOwnedSkyPartyBegin(u32 mask) { BeginPartyProjection(mask, PROJECTION_SKY, TRUE); }
void ModsOwnedFollowerPartyBegin(const u8 *order, u32 count)
{
    u32 mask = 0, previous = 0; bool32 valid = order && count <= PARTY_SIZE, ended = FALSE;
    for (u32 i = 0; valid && i < count; ++i)
    {
        u32 value = order[i];
        if (!value) { ended = TRUE; continue; }
        if (ended || value > PARTY_SIZE || value <= previous) { valid = FALSE; break; }
        previous = value; mask |= 1u << (value - 1);
    }
    BeginPartyProjection(mask, PROJECTION_FOLLOWER, valid);
}
static bool32 SkyProjectionMatches(void)
{
    for (u32 reduced = 0; reduced < PARTY_SIZE; ++reduced)
    {
        struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][reduced].box;
        if (reduced >= sSkyPartyCount) { if (mon->hasSpecies) return FALSE; continue; }
        u32 field = sSkyPartySlots[reduced];
        if (!mon->hasSpecies || mon->personality != sSkyPartyPersonality[field] || mon->otId != sSkyPartyOt[field]) return FALSE;
    }
    return TRUE;
}
void ModsOwnedSkyPartyPrepared(void)
{
    if (!sSkyPartyActive || !sSkyPartyValid || !SkyProjectionMatches())
    { sSkyPartyValid = FALSE; NativeOwnedTrackerUnsupported(); }
    NativeOwnedTrackSkyPrepared(); sSkyPartyPreparing = FALSE;
}
void ModsOwnedSkyPartyBeforeRestore(u32 mask)
{
    if (!sSkyPartyActive || !sSkyPartyValid || mask != sSkyPartyMask || !SkyProjectionMatches() || !gSaveBlock1Ptr)
    { sSkyPartyValid = FALSE; NativeOwnedTrackerUnsupported(); }
    else
        for (u32 i = 0; i < PARTY_SIZE; ++i)
        {
            struct BoxPokemon *saved = &GetSavedPlayerPartyMon(i)->box;
            if (!!saved->hasSpecies != !!sSkyPartyOccupied[i] || (saved->hasSpecies
                && (saved->personality != sSkyPartyPersonality[i] || saved->otId != sSkyPartyOt[i])))
            { sSkyPartyValid = FALSE; NativeOwnedTrackerUnsupported(); break; }
        }
    NativeOwnedTrackSkyBeforeRestore(mask);
}
void ModsOwnedSkyPartyAfterRestore(void)
{
    NativeOwnedTrackSkyRestored();
    sSkyPartyActive = sSkyPartyPreparing = sSkyPartyValid = sFollowerCatchFull = sFollowerCatchLoaded = FALSE;
    sSkyPartyCount = sPartyProjectionKind = 0;
}
void ModsOwnedFollowerPartyPrepared(void) { ModsOwnedSkyPartyPrepared(); }
void ModsOwnedFollowerPartyBeforeRestore(const u16 *order, u32 count)
{
    bool32 valid = ModsOwnedFollowerPartyActive() && order && count <= PARTY_SIZE;
    for (u32 i = 0; valid && i < count; ++i)
        if (order[i] != (i < sSkyPartyCount ? (u32)sSkyPartySlots[i] + 1 : 0)) valid = FALSE;
    if (count < sSkyPartyCount) valid = FALSE;
    ModsOwnedSkyPartyBeforeRestore(valid ? sSkyPartyMask : UINT32_MAX);
}
void ModsOwnedFollowerPartyAfterRestore(void) { ModsOwnedSkyPartyAfterRestore(); }
static bool32 UnionProjectionMatches(void)
{
    if (!gSaveBlock1Ptr || *GetSavedPlayerPartyCount() != sUnionSavedPartyCount) return FALSE;
    for (u32 i = 0; i < PARTY_SIZE; ++i)
    {
        struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][i].box;
        if (i >= sUnionPartyCount) { if (mon->hasSpecies) return FALSE; continue; }
        struct BoxPokemon *saved = &GetSavedPlayerPartyMon(sUnionPartySlots[i])->box;
        if (!mon->hasSpecies || !saved->hasSpecies
            || ((u64)mon->otId << 32 | mon->personality) != sUnionPartyIdentity[i]
            || ((u64)saved->otId << 32 | saved->personality) != sUnionPartyIdentity[i]) return FALSE;
    }
    return TRUE;
}
bool32 ModsOwnedUnionPartyReady(void)
{ return sUnionPartyActive && sLinkProjectionKind == LINK_PROJECTION_UNION
    && sUnionPartyValid && !sUnionPartyPreparing && UnionProjectionMatches(); }
bool32 ModsOwnedLinkMultiPartyReady(void)
{ return sUnionPartyActive && sLinkProjectionKind == LINK_PROJECTION_MULTI
    && sUnionPartyValid && !sUnionPartyPreparing && UnionProjectionMatches(); }
bool32 ModsOwnedFrontierPartyActive(void)
{ return sUnionPartyActive && sLinkProjectionKind == LINK_PROJECTION_FRONTIER; }
bool32 ModsOwnedFrontierPartyReady(void)
{ return ModsOwnedFrontierPartyActive() && sUnionPartyValid && !sUnionPartyPreparing
    && gPartiesCount[B_TRAINER_PLAYER] == sUnionPartyCount && UnionProjectionMatches(); }
bool32 ModsOwnedFrontierMultiPartyReady(void)
{ return ModsOwnedFrontierPartyReady() && sUnionPartyCount <= MULTI_PARTY_SIZE; }
u32 ModsOwnedFrontierPartySelection(u8 *order, u32 capacity)
{
    if (!order || !ModsOwnedFrontierPartyReady() || capacity < sUnionPartyCount) return 0;
    for (u32 i = 0; i < sUnionPartyCount; ++i) order[i] = sUnionPartySlots[i] + 1;
    return sUnionPartyCount;
}
void ModsOwnedFrontierBeforeSaveParty(void)
{
    if (sRentalPartyActive || sRecordedPartyActive) NativeOwnedTrackerUnsupported();
    if (!ModsOwnedFrontierPartyActive()) return;
    sUnionPartyValid = FALSE;
    NativeOwnedTrackerUnsupported();
}
void ModsOwnedFrontierBeforeSavePartyMon(u32 index, struct Pokemon *mon)
{
    if (sRentalPartyActive || sRecordedPartyActive) NativeOwnedTrackerUnsupported();
    if (!ModsOwnedFrontierPartyActive()) return;
    if (ModsOwnedFrontierPartyReady() && !gMain.inBattle)
        for (u32 i = 0; i < sUnionPartyCount; ++i)
            if (index == sUnionPartySlots[i] && mon == &gParties[B_TRAINER_PLAYER][i]) return;
    ModsOwnedFrontierBeforeSaveParty();
}
static void BeginLinkProjection(const u8 *order, u32 count, u8 kind)
{
    if (sUnionPartyActive || sSkyPartyActive) NativeOwnedTrackerUnsupported();
    sUnionPartyValid = !sUnionPartyActive && !sSkyPartyActive && !sRentalPartyActive && !sRecordedPartyActive && order && count && count <= PARTY_SIZE
        && (kind != LINK_PROJECTION_MULTI || count <= MULTI_PARTY_SIZE)
        && !gMain.inBattle && !NativeSaveTransactionActive() && !ModsOwnedStorageBusy()
        && !ModsOwnedBattlePartyViewActive() && !ModsOwnedTradePcHeld() && !ModsOwnedLinkTradeActive()
        && gSaveBlock1Ptr && gPokemonStoragePtr && gPartiesCount[B_TRAINER_PLAYER] <= PARTY_SIZE
        && *GetSavedPlayerPartyCount() == gPartiesCount[B_TRAINER_PLAYER];
    sUnionPartyActive = sUnionPartyPreparing = TRUE; sUnionPartyCount = 0; sLinkProjectionKind = kind;
    sUnionSavedPartyCount = sUnionPartyValid ? gPartiesCount[B_TRAINER_PLAYER] : 0;
    u32 seen = 0;
    for (u32 i = 0; sUnionPartyValid && i < count; ++i)
    {
        u32 slot = order[i];
        if (!slot || slot > PARTY_SIZE || (seen & (1u << (slot - 1)))) { sUnionPartyValid = FALSE; break; }
        --slot;
        if (slot >= sUnionSavedPartyCount) { sUnionPartyValid = FALSE; break; }
        struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][slot].box;
        struct BoxPokemon *saved = &GetSavedPlayerPartyMon(slot)->box;
        if (!mon->hasSpecies || !saved->hasSpecies || mon->personality != saved->personality || mon->otId != saved->otId)
        { sUnionPartyValid = FALSE; break; }
        seen |= 1u << slot; sUnionPartySlots[i] = slot;
        sUnionPartyIdentity[i] = (u64)mon->otId << 32 | mon->personality; ++sUnionPartyCount;
    }
    if (!sUnionPartyValid) NativeOwnedTrackerUnsupported();
    NativeOwnedTrackUnionBegin(order, count);
}
void ModsOwnedUnionPartyBegin(const u8 *order, u32 count) { BeginLinkProjection(order, count, LINK_PROJECTION_UNION); }
void ModsOwnedLinkMultiPartyBegin(const u8 *order, u32 count) { BeginLinkProjection(order, count, LINK_PROJECTION_MULTI); }
void ModsOwnedFrontierPartyBegin(const u8 *order, u32 count) { BeginLinkProjection(order, count, LINK_PROJECTION_FRONTIER); }
void ModsOwnedFrontierPartyReselect(const u8 *order, u32 count, u32 capacity)
{
    if (!ModsOwnedFrontierPartyActive()) return;
    bool32 valid = ModsOwnedFrontierPartyReady() && order && count && count <= sUnionPartyCount && count <= capacity;
    u8 slots[PARTY_SIZE]; u64 identities[PARTY_SIZE]; u32 seen = 0;
    for (u32 i = 0; valid && i < count; ++i)
    {
        u32 slot = order[i];
        if (!slot || slot > sUnionPartyCount || (seen & (1u << (slot - 1)))) { valid = FALSE; break; }
        seen |= 1u << (slot - 1); slots[i] = sUnionPartySlots[slot - 1]; identities[i] = sUnionPartyIdentity[slot - 1];
    }
    for (u32 i = count; valid && i < capacity; ++i) if (order[i]) valid = FALSE;
    if (!valid) { sUnionPartyValid = FALSE; NativeOwnedTrackerUnsupported(); }
    else
    {
        NativeOwnedTrackUnionReselect(order, count);
        memcpy(sUnionPartySlots, slots, count); memcpy(sUnionPartyIdentity, identities, count * sizeof(identities[0]));
        sUnionPartyCount = count;
    }
    sUnionPartyPreparing = TRUE;
}
bool32 ModsOwnedFrontierPartyBeforeSwap(u32 first, u32 second)
{
    if (!ModsOwnedFrontierPartyActive() || gMain.inBattle) return FALSE;
    if (!ModsOwnedFrontierPartyReady() || first >= sUnionPartyCount || second >= sUnionPartyCount || first == second)
    {
        sUnionPartyValid = FALSE; sUnionPartyPreparing = TRUE; NativeOwnedTrackerUnsupported();
        return TRUE;
    }
    u8 order[PARTY_SIZE];
    for (u32 i = 0; i < sUnionPartyCount; ++i) order[i] = i + 1;
    u8 temp = order[first]; order[first] = order[second]; order[second] = temp;
    ModsOwnedFrontierPartyReselect(order, sUnionPartyCount, sUnionPartyCount);
    return TRUE;
}
void ModsOwnedUnionPartyPrepared(void)
{
    if (!sUnionPartyActive) return;
    if (!sUnionPartyValid || !UnionProjectionMatches()) { sUnionPartyValid = FALSE; NativeOwnedTrackerUnsupported(); }
    NativeOwnedTrackUnionPrepared(); sUnionPartyPreparing = FALSE;
}
void ModsOwnedUnionPartyBeforeRestore(void)
{
    if (!sUnionPartyActive) return;
    if (ModsOwnedFrontierPartyActive() && !sUnionPartyValid) NativeOwnedTrackerUnsupported();
    NativeOwnedTrackUnionBeforeRestore();
}
void ModsOwnedUnionPartyAfterRestore(void)
{
    if (!sUnionPartyActive) return;
    NativeOwnedTrackUnionRestored(); sUnionPartyActive = sUnionPartyValid = sUnionPartyPreparing = FALSE;
    sUnionPartyCount = sUnionSavedPartyCount = sLinkProjectionKind = 0;
}
void ModsOwnedFollowerCatchBeforeLoad(void)
{
    if (!ModsOwnedFollowerPartyActive() || sFollowerCatchFull) return;
    ModsOwnedFollowerPartyBeforeRestore(gSaveBlock2Ptr ? gSaveBlock2Ptr->frontier.selectedPartyMons : NULL,
                                        MAX_FRONTIER_PARTY_SIZE);
    sFollowerCatchFull = TRUE; sFollowerCatchLoaded = FALSE;
}
void ModsOwnedFollowerCatchLoaded(void)
{
    if (!ModsOwnedFollowerCatchFullActive()) return;
    if (!sFollowerCatchLoaded && sSkyPartyValid)
    {
        for (u32 i = 0; i < PARTY_SIZE; ++i)
        {
            struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][i].box;
            if (!!mon->hasSpecies != !!sSkyPartyOccupied[i] || (mon->hasSpecies
                && (mon->personality != sSkyPartyPersonality[i] || mon->otId != sSkyPartyOt[i])))
            { sSkyPartyValid = FALSE; NativeOwnedTrackerUnsupported(); break; }
        }
    }
    sFollowerCatchLoaded = TRUE;
    NativeOwnedTrackFollowerCatchLoaded();
}
void ModsOwnedFollowerCatchBeforeReduce(void)
{
    if (!ModsOwnedFollowerCatchFullActive()) return;
    bool32 valid = sSkyPartyValid && gSaveBlock2Ptr && gSaveBlock1Ptr;
    for (u32 i = 0; valid && i < MAX_FRONTIER_PARTY_SIZE; ++i)
        if (gSaveBlock2Ptr->frontier.selectedPartyMons[i] != (i < sSkyPartyCount ? (u32)sSkyPartySlots[i] + 1 : 0)) valid = FALSE;
    for (u32 i = 0; valid && i < PARTY_SIZE; ++i)
    {
        struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][i].box;
        struct BoxPokemon *saved = &GetSavedPlayerPartyMon(i)->box;
        if (memcmp(mon, saved, sizeof(*mon)) || ((sSkyPartyMask & (1u << i)) && !mon->hasSpecies)) valid = FALSE;
    }
    if (!valid) { sSkyPartyValid = FALSE; NativeOwnedTrackerUnsupported(); }
    else for (u32 i = 0; i < PARTY_SIZE; ++i)
    {
        struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][i].box;
        sSkyPartyOccupied[i] = mon->hasSpecies;
        sSkyPartyPersonality[i] = mon->personality; sSkyPartyOt[i] = mon->otId;
    }
    NativeOwnedTrackFollowerCatchBeforeReduce();
}
void ModsOwnedFollowerCatchReduced(void)
{
    if (!ModsOwnedFollowerCatchFullActive()) return;
    if (!sSkyPartyValid || !SkyProjectionMatches()) { sSkyPartyValid = FALSE; NativeOwnedTrackerUnsupported(); }
    NativeOwnedTrackFollowerCatchReduced();
    sFollowerCatchFull = sFollowerCatchLoaded = FALSE;
}

static int Snapshot(struct BoxPokemon *mon, unsigned location, unsigned box, unsigned slot,
                    struct RhhOwnedMonSnapshot *out)
{
    if (!mon || !out || out->struct_size < sizeof(*out)
        || !GetBoxMonData(mon, MON_DATA_SANITY_HAS_SPECIES)
        || GetBoxMonData(mon, MON_DATA_SANITY_IS_BAD_EGG)) return -1;
    /* The engine's decoder can mark corrupt data as a bad egg. Query a copy so
     * even malformed input cannot turn a read-only service into a mutation. */
    struct BoxPokemon copy = *mon;
    mon = &copy;
    u32 species = GetBoxMonData(mon, MON_DATA_SPECIES);
    if (GetBoxMonData(mon, MON_DATA_SANITY_IS_BAD_EGG) || !species || species >= NUM_SPECIES) return -1;
    RhhContentKey key = RhhContentKeyForId(RHH_SPECIES, species);
    if (!key) return -1;
    u32 personality = GetBoxMonData(mon, MON_DATA_PERSONALITY);
    struct RhhOwnedMonSnapshot result = {
        .struct_size = sizeof(result), .location = location, .box = box, .slot = slot,
        .identity = (u64)GetBoxMonData(mon, MON_DATA_OT_ID) << 32 | personality,
        .species = key, .level = GetLevelFromBoxMonExp(mon),
        .nature = GetBoxMonNature(mon),
        .stat_nature = GetBoxMonData(mon, MON_DATA_HIDDEN_NATURE),
        .is_egg = GetBoxMonData(mon, MON_DATA_IS_EGG),
        .max_iv = MAX_PER_STAT_IVS, .max_stat_ev = MAX_PER_STAT_EVS, .max_total_ev = MAX_TOTAL_EVS
    };
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i)
    {
        result.ivs[i] = GetBoxMonData(mon, MON_DATA_HP_IV + i);
        result.evs[i] = GetBoxMonData(mon, MON_DATA_HP_EV + i);
    }
    *out = result;
    return 0;
}
static int PartyAt(uint32_t slot, struct RhhOwnedMonSnapshot *out)
{
    if (ModsOwnedBattlePartyViewActive() || ModsOwnedProjectedPartyActive() || ModsOwnedLinkTradeActive()
        || slot >= PARTY_SIZE || slot >= gPartiesCount[B_TRAINER_PLAYER]) return -1;
    return Snapshot(&gParties[B_TRAINER_PLAYER][slot].box, RHH_MON_PARTY, 0, slot, out);
}
static int BoxAt(uint32_t box, uint32_t slot, struct RhhOwnedMonSnapshot *out)
{
    if (!gPokemonStoragePtr || box >= TOTAL_BOXES_COUNT || slot >= IN_BOX_COUNT) return -1;
    return Snapshot(&gPokemonStoragePtr->boxes[box][slot], RHH_MON_BOX, box, slot, out);
}
static int Find(uint64_t identity, struct RhhOwnedMonSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out) || ModsOwnedBattlePartyViewActive() || ModsOwnedProjectedPartyActive()
        || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive()) return -1;
    struct RhhOwnedMonSnapshot match, candidate;
    unsigned found = 0;
    for (unsigned i = 0; i < PARTY_SIZE; ++i)
    {
        candidate.struct_size = sizeof(candidate);
        if (!PartyAt(i, &candidate) && candidate.identity == identity)
        { match = candidate; ++found; }
    }
    if (gPokemonStoragePtr)
        for (unsigned box = 0; box < TOTAL_BOXES_COUNT; ++box)
            for (unsigned slot = 0; slot < IN_BOX_COUNT; ++slot)
            {
                candidate.struct_size = sizeof(candidate);
                if (!BoxAt(box, slot, &candidate) && candidate.identity == identity)
                { match = candidate; ++found; }
            }
    /* OT/personality is not guaranteed unique, so never choose an arbitrary clone. */
    if (found != 1) return -1;
    *out = match;
    return 0;
}
static void IdentityAdd(const struct BoxPokemon *mon, u32 location, u32 box, u32 slot,
                        u64 identity, struct RhhOwnedIdentityQuery *result, u32 *locationCount,
                        struct RhhOwnedIdentityOccurrences *occurrences)
{
    if (!mon->hasSpecies || ((u64)mon->otId << 32 | mon->personality) != identity) return;
    ++result->matches; ++*locationCount;
    struct RhhOwnedMonSnapshot candidate = {.struct_size = sizeof(candidate)};
    bool32 unreadable = !!Snapshot((struct BoxPokemon *)mon, location, box, slot, &candidate);
    if (unreadable) result->flags |= RHH_OWNED_IDENTITY_CORRUPT;
    else if (result->matches == 1) result->unique = candidate;
    if (occurrences)
    {
        if (occurrences->count == RHH_OWNED_IDENTITY_OCCURRENCE_LIMIT)
            occurrences->flags |= RHH_OWNED_OCCURRENCES_TRUNCATED;
        else
            occurrences->entries[occurrences->count++] = (struct RhhOwnedIdentityOccurrence){
                .location = location, .box = box, .slot = slot,
                .flags = unreadable ? RHH_OWNED_OCCURRENCE_UNREADABLE : 0,
                .species = unreadable ? 0 : candidate.species};
    }
}
static const struct RhhPartyReorder *sPartyReorderContext;
static const struct RhhDaycareTransfer *sDaycareTransferContext;
static const struct RhhFusionTransfer *sFusionTransferContext;
static const struct RhhCatchPlacement *sCatchPlacementContext;
static const struct RhhOwnedGift *sOwnedGiftContext;
static const struct RhhOwnedEvolution *sOwnedEvolutionContext;
static const struct RhhIngameTrade *sIngameTradeContext;
static const struct RhhLinkTrade *sLinkTradeContext;
static const struct RhhSavedLinkTrade *sLinkTradeReceiptContext;
static bool32 OwnedStorageAvailable(void)
{
    return !(!gSaveBlock1Ptr || !gPokemonStoragePtr
        || NativeSaveTransactionActive() || ModsOwnedStorageBusy() || ModsOwnedBattlePartyViewActive() || ModsOwnedProjectedPartyActive()
        || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive() || sPartyReorderContext || sDaycareTransferContext
        || sFusionTransferContext || sCatchPlacementContext || sOwnedGiftContext || sOwnedEvolutionContext
        || sIngameTradeContext || sLinkTradeContext
        || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK
            | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_EREADER_TRAINER
            | BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_CATCH_TUTORIAL))));
}
static bool32 CheckLegacyNatureSlot(const struct BoxPokemon *box, u32 location, u32 area, u32 slot,
                                    struct RhhNatureFormatCheck *result)
{
    if (!box->hasSpecies && !box->isBadEgg) return TRUE;
    struct BoxPokemon copy = *box;
    u32 species = GetBoxMonData(&copy, MON_DATA_SPECIES);
    u32 code = GetBoxMonData(&copy, MON_DATA_NATIVE_NATURE_CODE);
    if (copy.isBadEgg || !species || species >= NUM_SPECIES) result->status = RHH_NATURE_FORMAT_UNREADABLE;
    else if (code) result->status = RHH_NATURE_FORMAT_INCOMPATIBLE;
    else return TRUE;
    result->location = location; result->box = area; result->slot = slot; return FALSE;
}
static bool32 CheckLegacyRecordedBattle(struct RhhNatureFormatCheck *result)
{
    struct RecordedBattleSave record;
    int read = NativeSaveSpecialRead(SECTOR_ID_RECORDED_BATTLE, &record, sizeof(record));
    if (read == 1) return TRUE;
    if (read)
    {
        result->status = RHH_NATURE_FORMAT_UNREADABLE; result->location = RHH_NATURE_RECORDED_BATTLE;
        result->box = result->slot = UINT32_MAX; return FALSE;
    }
    unsigned natureFormat;
    if (NativeNatureRecordingDecode(&record, &record, &natureFormat))
    {
        result->status = RHH_NATURE_FORMAT_UNREADABLE; result->location = RHH_NATURE_RECORDED_BATTLE;
        result->box = result->slot = UINT32_MAX; return FALSE;
    }
    if (natureFormat) return TRUE; /* The versioned decoder validated all occupied slots. */
    for (u32 trainer = 0; trainer < MAX_BATTLE_TRAINERS; ++trainer)
        for (u32 slot = 0; slot < PARTY_SIZE; ++slot)
            if (!CheckLegacyNatureSlot(&record.parties[trainer][slot].box, RHH_NATURE_RECORDED_BATTLE,
                                       trainer, slot, result)) return FALSE;
    return TRUE;
}
static int NatureFormatPreflight(struct RhhNatureFormatCheck *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhNatureFormatCheck result = {.struct_size = sizeof(result), .status = RHH_NATURE_FORMAT_UNAVAILABLE};
    if (!RhhModsHasActiveRun() || !OwnedStorageAvailable() || gMain.inBattle
        || NativeLinkSessionOpen() || gReceivedRemoteLinkPlayers || IsLinkConnectionEstablished()) goto done;
    if (ModsNatureFormatEnabled()) { result.status = RHH_NATURE_FORMAT_ACTIVE; goto done; }
    result.status = RHH_NATURE_FORMAT_READY;
    for (u32 i = 0; i < PARTY_SIZE; ++i)
        if (!CheckLegacyNatureSlot(&gParties[B_TRAINER_PLAYER][i].box, RHH_MON_PARTY, 0, i, &result)) goto done;
    for (u32 i = 0; i < PARTY_SIZE; ++i)
        if (!CheckLegacyNatureSlot(&gSaveBlock1Ptr->playerParty[i].box, RHH_NATURE_SAVED_PARTY, 0, i, &result)) goto done;
    for (u32 b = 0; b < TOTAL_BOXES_COUNT; ++b)
        for (u32 i = 0; i < IN_BOX_COUNT; ++i)
            if (!CheckLegacyNatureSlot(&gPokemonStoragePtr->boxes[b][i], RHH_MON_BOX, b, i, &result)) goto done;
    for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
        if (!CheckLegacyNatureSlot(&gSaveBlock1Ptr->daycare.mons[i].mon, RHH_MON_DAYCARE, 0, i, &result)) goto done;
#if IS_FRLG
    if (!CheckLegacyNatureSlot(&gSaveBlock1Ptr->route5DayCareMon.mon, RHH_MON_DAYCARE, 1, 0, &result)) goto done;
#endif
    for (u32 i = 0; i < MAX_FUSION_STORAGE; ++i)
        if (!CheckLegacyNatureSlot(&gPokemonStoragePtr->fusions[i].box, RHH_MON_FUSION_STORAGE, 0, i, &result)) goto done;
    if (!CheckLegacyRecordedBattle(&result)) goto done;
done:
    *out = result; return 0;
}
bool32 ModsNatureFormatCanEnable(void)
{
    struct RhhNatureFormatCheck result = {.struct_size = sizeof(result)};
    return !NatureFormatPreflight(&result) && result.status == RHH_NATURE_FORMAT_READY;
}
static uint32_t OwnedDaycareCount(void) { return IS_FRLG ? 2 : 1; }
static uint32_t OwnedDaycareSlots(uint32_t daycare)
{ return daycare == 0 ? DAYCARE_MON_COUNT : (IS_FRLG && daycare == 1 ? 1 : 0); }
static uint32_t OwnedFusionSlots(void) { return MAX_FUSION_STORAGE; }
static int OwnedDaycareAt(uint32_t daycare, uint32_t slot, struct RhhOwnedMonSnapshot *out)
{
    if (!OwnedStorageAvailable() || slot >= OwnedDaycareSlots(daycare)) return -1;
    struct BoxPokemon *mon = &gSaveBlock1Ptr->daycare.mons[slot].mon;
#if IS_FRLG
    if (daycare == 1) mon = &gSaveBlock1Ptr->route5DayCareMon.mon;
#endif
    return Snapshot(mon, RHH_MON_DAYCARE, daycare, slot, out);
}
static int OwnedFusionAt(uint32_t slot, struct RhhOwnedMonSnapshot *out)
{
    if (!OwnedStorageAvailable() || slot >= MAX_FUSION_STORAGE) return -1;
    return Snapshot(&gPokemonStoragePtr->fusions[slot].box, RHH_MON_FUSION_STORAGE, 0, slot, out);
}
static int OwnedExperience(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedExperience *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || !out || out->struct_size < sizeof(*out)
        || !OwnedStorageAvailable()) return -1;
    struct RhhOwnedMonSnapshot current = {.struct_size = sizeof(current)};
    const struct BoxPokemon *source = NULL;
    switch (expected->location)
    {
    case RHH_MON_PARTY:
        if (!PartyAt(expected->slot, &current)) source = &gParties[B_TRAINER_PLAYER][expected->slot].box;
        break;
    case RHH_MON_BOX:
        if (!BoxAt(expected->box, expected->slot, &current)) source = &gPokemonStoragePtr->boxes[expected->box][expected->slot];
        break;
    case RHH_MON_DAYCARE:
        if (!OwnedDaycareAt(expected->box, expected->slot, &current))
        {
            source = &gSaveBlock1Ptr->daycare.mons[expected->slot].mon;
#if IS_FRLG
            if (expected->box == 1) source = &gSaveBlock1Ptr->route5DayCareMon.mon;
#endif
        }
        break;
    case RHH_MON_FUSION_STORAGE:
        if (!OwnedFusionAt(expected->slot, &current)) source = &gPokemonStoragePtr->fusions[expected->slot].box;
        break;
    }
    if (!source || current.is_egg || !RhhMonSnapshotsEqual(expected, &current)
        || current.level < 1 || current.level > MAX_LEVEL) return -1;
    struct BoxPokemon copy = *source;
    u32 species = GetBoxMonData(&copy, MON_DATA_SPECIES);
    u32 growth = gSpeciesInfo[species].growthRate;
    if (growth > GROWTH_SLOW) return -1;
    u32 exp = GetBoxMonData(&copy, MON_DATA_EXP);
    u32 next = gExperienceTables[growth][current.level < MAX_LEVEL ? current.level + 1 : MAX_LEVEL];
    struct RhhOwnedExperience result = {.struct_size = sizeof(result), .experience = exp,
        .level_start_exp = gExperienceTables[growth][current.level], .next_level_exp = next,
        .to_next_level = current.level < MAX_LEVEL && exp < next ? next - exp : 0,
        .maximum_exp = gExperienceTables[growth][MAX_LEVEL], .mon = current};
    *out = result; return 0;
}
static bool32 sOwnedStatsBusy;
static int OwnedStats(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedStats *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || !out || out->struct_size < sizeof(*out)
        || sOwnedStatsBusy || gMain.inBattle || !OwnedStorageAvailable()) return -1;
    struct RhhOwnedMonSnapshot current = {.struct_size = sizeof(current)};
    const struct Pokemon *party = NULL;
    const struct BoxPokemon *box = NULL;
    switch (expected->location)
    {
    case RHH_MON_PARTY:
        if (!PartyAt(expected->slot, &current)) party = &gParties[B_TRAINER_PLAYER][expected->slot];
        break;
    case RHH_MON_BOX:
        if (!BoxAt(expected->box, expected->slot, &current)) box = &gPokemonStoragePtr->boxes[expected->box][expected->slot];
        break;
    case RHH_MON_DAYCARE:
        if (!OwnedDaycareAt(expected->box, expected->slot, &current))
        {
            box = &gSaveBlock1Ptr->daycare.mons[expected->slot].mon;
#if IS_FRLG
            if (expected->box == 1) box = &gSaveBlock1Ptr->route5DayCareMon.mon;
#endif
        }
        break;
    case RHH_MON_FUSION_STORAGE:
        if (!OwnedFusionAt(expected->slot, &current)) party = &gPokemonStoragePtr->fusions[expected->slot];
        break;
    }
    if ((!party && !box) || current.is_egg || !RhhMonSnapshotsEqual(expected, &current)
        || current.level < 1 || current.level > MAX_LEVEL) return -1;
    struct Pokemon staged = {0};
    struct RhhOwnedStats result = {.struct_size = sizeof(result), .mon = current};
    if (party) staged = *party;
    else
    {
        struct BoxPokemon original = *box;
        staged.box = original;
        u32 lost = GetBoxMonData(&staged.box, MON_DATA_HP_LOST);
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        u64 run = RhhModsRunId();
        const void *save = gSaveBlock1Ptr, *storage = gPokemonStoragePtr;
        sOwnedStatsBusy = TRUE;
        CalculateMonStatsPreview(&staged);
        sOwnedStatsBusy = FALSE;
        bool32 valid = run == RhhModsRunId() && save == gSaveBlock1Ptr && storage == gPokemonStoragePtr
            && !gMain.inBattle && OwnedStorageAvailable()
            && !memcmp(&original, box, sizeof(original))
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
        if (!valid) { gRngValue = rng; gRng2Value = rng2; return -1; }
        result.flags = RHH_OWNED_STATS_RECONSTRUCTED;
        if (lost > staged.maxHP) { lost = staged.maxHP; result.flags |= RHH_OWNED_STATS_HP_LOSS_CLAMPED; }
        staged.hp = staged.maxHP - lost;
    }
    if (!staged.maxHP || staged.hp > staged.maxHP || !staged.level || staged.level > MAX_LEVEL) return -1;
    result.level = staged.level; result.current_hp = staged.hp;
    result.stats[RHH_STAT_HP] = staged.maxHP; result.stats[RHH_STAT_ATTACK] = staged.attack;
    result.stats[RHH_STAT_DEFENSE] = staged.defense; result.stats[RHH_STAT_SPEED] = staged.speed;
    result.stats[RHH_STAT_SP_ATTACK] = staged.spAttack; result.stats[RHH_STAT_SP_DEFENSE] = staged.spDefense;
    *out = result; return 0;
}
static u32 HyperTrainingMask(struct BoxPokemon *mon)
{
    u32 mask = 0;
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i)
        if (GetBoxMonData(mon, MON_DATA_HYPER_TRAINED_HP + i)) mask |= 1u << i;
    return mask;
}
static int OwnedIvView(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedIvView *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || !out || out->struct_size < sizeof(*out)
        || gMain.inBattle || !OwnedStorageAvailable()) return -1;
    struct RhhOwnedMonSnapshot current = {.struct_size = sizeof(current)};
    const struct BoxPokemon *source = NULL;
    switch (expected->location)
    {
    case RHH_MON_PARTY:
        if (!PartyAt(expected->slot, &current)) source = &gParties[B_TRAINER_PLAYER][expected->slot].box;
        break;
    case RHH_MON_BOX:
        if (!BoxAt(expected->box, expected->slot, &current)) source = &gPokemonStoragePtr->boxes[expected->box][expected->slot];
        break;
    case RHH_MON_DAYCARE:
        if (!OwnedDaycareAt(expected->box, expected->slot, &current))
        {
            source = &gSaveBlock1Ptr->daycare.mons[expected->slot].mon;
#if IS_FRLG
            if (expected->box == 1) source = &gSaveBlock1Ptr->route5DayCareMon.mon;
#endif
        }
        break;
    case RHH_MON_FUSION_STORAGE:
        if (!OwnedFusionAt(expected->slot, &current)) source = &gPokemonStoragePtr->fusions[expected->slot].box;
        break;
    }
    if (!source || current.is_egg || !RhhMonSnapshotsEqual(expected, &current)) return -1;
    struct BoxPokemon copy = *source;
    struct RhhOwnedIvView result = {.struct_size = sizeof(result), .mon = current};
    result.hyper_trained = HyperTrainingMask(&copy);
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i)
        result.effective_ivs[i] = (result.hyper_trained & (1u << i)) ? current.max_iv : current.ivs[i];
    *out = result; return 0;
}
static RhhContentKey OwnedTeraTypeKey(u32 type)
{
    if (type == TYPE_NONE || type == TYPE_MYSTERY || type >= NUMBER_OF_MON_TYPES) return 0;
    return RhhContentKeyForId(RHH_TYPE, type);
}
static int MakeOwnedTeraView(const struct BoxPokemon *source, const struct RhhOwnedMonSnapshot *mon,
                             struct RhhOwnedTeraView *out)
{
    struct BoxPokemon copy = *source;
    struct RhhOwnedTeraView result = {.struct_size = sizeof(result), .mon = *mon};
    if (!GetBoxMonStoredTeraType(&copy, &result.stored_ordinal)) return -1;
    u32 species = GetBoxMonData(&copy, MON_DATA_SPECIES);
    if (!species || species >= NUM_SPECIES) return -1;
    result.resolved_ordinal = GetBoxMonData(&copy, MON_DATA_TERA_TYPE);
    result.source = gSpeciesInfo[species].forceTeraType ? RHH_OWNED_TERA_FORCED
        : result.stored_ordinal == TYPE_NONE ? RHH_OWNED_TERA_PERSONALITY : RHH_OWNED_TERA_STORED;
    result.stored_type = OwnedTeraTypeKey(result.stored_ordinal);
    result.resolved_type = OwnedTeraTypeKey(result.resolved_ordinal);
    if (result.stored_ordinal != TYPE_NONE && !result.stored_type) result.flags |= RHH_OWNED_TERA_UNSUPPORTED_STORED;
    if (!result.resolved_type) result.flags |= RHH_OWNED_TERA_UNSUPPORTED_RESOLVED;
    *out = result; return 0;
}
static int OwnedTeraView(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedTeraView *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || !out || out->struct_size < sizeof(*out)
        || gMain.inBattle || !OwnedStorageAvailable()) return -1;
    struct RhhOwnedMonSnapshot current = {.struct_size = sizeof(current)};
    const struct BoxPokemon *source = NULL;
    switch (expected->location)
    {
    case RHH_MON_PARTY:
        if (!PartyAt(expected->slot, &current)) source = &gParties[B_TRAINER_PLAYER][expected->slot].box;
        break;
    case RHH_MON_BOX:
        if (!BoxAt(expected->box, expected->slot, &current)) source = &gPokemonStoragePtr->boxes[expected->box][expected->slot];
        break;
    case RHH_MON_DAYCARE:
        if (!OwnedDaycareAt(expected->box, expected->slot, &current))
        {
            source = &gSaveBlock1Ptr->daycare.mons[expected->slot].mon;
#if IS_FRLG
            if (expected->box == 1) source = &gSaveBlock1Ptr->route5DayCareMon.mon;
#endif
        }
        break;
    case RHH_MON_FUSION_STORAGE:
        if (!OwnedFusionAt(expected->slot, &current)) source = &gPokemonStoragePtr->fusions[expected->slot].box;
        break;
    }
    if (!source || current.is_egg || !RhhMonSnapshotsEqual(expected, &current)) return -1;
    return MakeOwnedTeraView(source, &current, out);
}
static int PreviewOwnedExperience(const struct RhhOwnedExperience *expected, u32 amount,
                                  struct RhhExperiencePreview *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhOwnedExperience current = {.struct_size = sizeof(current)};
    if (OwnedExperience(&expected->mon, &current) || current.experience != expected->experience
        || current.experience < current.level_start_exp
        || (current.mon.level < MAX_LEVEL && current.experience >= current.next_level_exp)) return -1;
    if (current.mon.location == RHH_MON_PARTY)
    {
        struct Pokemon copy = gParties[B_TRAINER_PLAYER][current.mon.slot];
        if (GetMonData(&copy, MON_DATA_LEVEL) != current.mon.level) return -1;
    }
    u32 species;
    if (RhhContentResolve(RHH_SPECIES, current.mon.species, &species) || !species || species >= NUM_SPECIES) return -1;
    u32 growth = gSpeciesInfo[species].growthRate;
    if (growth > GROWTH_SLOW) return -1;
    u32 modCap = ModsLevelCap();
    bool32 hard = B_EXP_CAP_TYPE == EXP_CAP_HARD || modCap < MAX_LEVEL;
    u32 cap = modCap < MAX_LEVEL ? modCap : hard ? GetCurrentLevelCap() : MAX_LEVEL;
    if (cap > MAX_LEVEL) cap = MAX_LEVEL;
    u32 limit = gExperienceTables[growth][cap];
    u32 applied = amount;
    if (current.mon.level >= cap || current.experience >= limit) applied = 0;
    else if ((u64)current.experience + applied > limit) applied = limit - current.experience;
    u32 after = current.experience + applied, level = 1;
    while (level <= MAX_LEVEL && gExperienceTables[growth][level] <= after) ++level;
    struct RhhExperiencePreview result = {.struct_size = sizeof(result),
        .flags = (hard ? RHH_EXPERIENCE_HARD_CAP : 0)
               | (current.mon.level == MAX_LEVEL ? RHH_EXPERIENCE_MAX_LEVEL : 0),
        .requested = amount, .applied = applied, .before_exp = current.experience, .after_exp = after,
        .before_level = current.mon.level, .after_level = level - 1, .level_cap = cap, .cap_exp = limit, .mon = current.mon};
    *out = result; return 0;
}
static bool32 sExperienceMovesBusy;
static int PreviewOwnedExperienceMoves(const struct RhhOwnedExperience *expected, u32 amount, u32 start,
                                       struct RhhExperienceMovePage *out)
{
    if (!out || out->struct_size < sizeof(*out) || sExperienceMovesBusy
        || ModsLearnsetResolutionActive()) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    u64 run = RhhModsRunId();
    struct RhhExperienceMovePage result = {.struct_size = sizeof(result), .start = start,
        .experience.struct_size = sizeof(result.experience)};
    struct RhhOwnedExperience current = {.struct_size = sizeof(current)};
    u32 species;
    bool32 valid = TRUE;
    sExperienceMovesBusy = TRUE;
    if (PreviewOwnedExperience(expected, amount, &result.experience)
        || RhhContentResolve(RHH_SPECIES, result.experience.mon.species, &species)
        || !species || species >= NUM_SPECIES) valid = FALSE;
    if (valid && result.experience.after_level > result.experience.before_level)
    {
        const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(species);
        for (u32 i = 0; learnset[i].move != LEVEL_UP_MOVE_END; ++i)
        {
            u32 level = learnset[i].level, move = learnset[i].move;
            if (level <= result.experience.before_level || level > result.experience.after_level) continue;
            if (result.total >= start && result.count < RHH_EXPERIENCE_MOVE_PAGE_SIZE)
            {
                RhhContentKey key = move < MOVES_COUNT_ALL ? RhhContentKeyForId(RHH_MOVE, move) : RhhModsMoveKey(move);
                if (!key) { valid = FALSE; break; }
                result.candidates[result.count++] = (struct RhhExperienceMoveCandidate){.level = level, .move = key};
            }
            ++result.total;
        }
    }
    valid = valid && run == RhhModsRunId() && !OwnedExperience(&expected->mon, &current)
        && current.experience == expected->experience && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    sExperienceMovesBusy = FALSE;
    if (!valid) { gRngValue = rng; gRng2Value = rng2; return -1; }
    *out = result; return 0;
}
static struct {
    bool32 active, inContext;
    u64 nextTicket, run;
    struct RhhExperienceAwardRequest request;
    struct RhhExperienceAwardResult result;
} sExperienceAward;
static void ResetExperienceAward(void)
{
    u64 nextTicket = sExperienceAward.nextTicket;
    memset(&sExperienceAward, 0, sizeof(sExperienceAward));
    sExperienceAward.nextTicket = nextTicket;
}
static void ExperienceAwardDispatch(u32 kind, struct RhhModDecision *decision)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = sExperienceAward.result.owner, .subject = sExperienceAward.result.experience.mon.identity,
        .ordinal = sExperienceAward.result.ticket,
        .args = {sExperienceAward.result.experience.mon.slot, sExperienceAward.result.experience.applied,
                 sExperienceAward.result.status, sExperienceAward.result.finished}};
    sExperienceAward.inContext = TRUE;
    RhhModsDispatch(&event, decision);
    sExperienceAward.inContext = FALSE;
}
static int ExperienceAwardContext(struct RhhExperienceAwardResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sExperienceAward.inContext
        || (event != RHH_CAN_AWARD_EXPERIENCE && event != RHH_EXPERIENCE_AWARDED
            && event != RHH_EXPERIENCE_FLOW_FINISHED)) return -1;
    *out = sExperienceAward.result; return 0;
}
static int ExperienceAwardStatus(u64 ticket, struct RhhExperienceAwardResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ticket || ticket != sExperienceAward.result.ticket
        || sExperienceAward.run != RhhModsRunId()) return -1;
    *out = sExperienceAward.result; return 0;
}
static bool32 ExperienceAwardReady(void)
{
    return RhhModsHasActiveRun() && RhhModsCommandsAllowed() && OwnedStorageAvailable() && gSaveBlock2Ptr
        && !gMain.inBattle && gMain.callback2 == CB2_Overworld && !gPaletteFade.active && !gPaletteFade.y
        && !ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked() && !IsOverworldLinkActive()
        && !RhhUiActive() && !RhhUiOverlayActive() && !RhhUiKeysBlocked();
}
static int AwardExperience(const struct RhhExperienceAwardRequest *request, struct RhhExperienceAwardResult *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_EXPERIENCE_AWARD_INVALID;
    struct RhhExperienceAwardResult result = {.struct_size = sizeof(result), .status = RHH_EXPERIENCE_AWARD_INVALID,
        .experience.struct_size = sizeof(result.experience)};
    if (!request || request->struct_size < sizeof(*request) || !request->owner
        || !RhhModsRegisteredKey(request->owner) || !request->amount || request->expected.mon.location != RHH_MON_PARTY)
    { *out = result; return result.status; }
    struct RhhExperienceAwardRequest copy = *request;
    request = &copy;
    if (sExperienceAward.active || sExperienceAward.nextTicket == UINT64_MAX || !ExperienceAwardReady())
    { result.status = RHH_EXPERIENCE_AWARD_UNAVAILABLE; *out = result; return result.status; }
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    if (PreviewOwnedExperience(&request->expected, request->amount, &result.experience)
        || !GetMonData(&gParties[B_TRAINER_PLAYER][request->expected.mon.slot], MON_DATA_HP))
        result.status = RHH_EXPERIENCE_AWARD_STALE;
    else if (!result.experience.applied) result.status = RHH_EXPERIENCE_AWARD_NO_GAIN;
    else if (memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
        result.status = RHH_EXPERIENCE_AWARD_UNAVAILABLE;
    else
    {
        struct RhhExperienceAwardResult previous = sExperienceAward.result;
        u64 previousRun = sExperienceAward.run;
        result.owner = request->owner; result.ticket = sExperienceAward.nextTicket + 1;
        result.status = RHH_EXPERIENCE_AWARD_PENDING;
        sExperienceAward.active = TRUE; sExperienceAward.request = *request; sExperienceAward.result = result;
        sExperienceAward.run = RhhModsRunId();
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        ExperienceAwardDispatch(RHH_CAN_AWARD_EXPERIENCE, &decision);
        struct RhhExperiencePreview again = {.struct_size = sizeof(again)};
        if (!decision.allowed)
        {
            result.status = RHH_EXPERIENCE_AWARD_DENIED;
            snprintf(result.reason, sizeof(result.reason), "%s", decision.reason);
        }
        else if (!ExperienceAwardReady() || RhhModsRunId() != sExperienceAward.run
            || PreviewOwnedExperience(&request->expected, request->amount, &again)
            || memcmp(&again, &result.experience, sizeof(again))
            || !GetMonData(&gParties[B_TRAINER_PLAYER][request->expected.mon.slot], MON_DATA_HP)
            || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
            result.status = RHH_EXPERIENCE_AWARD_STALE;
        else if (!StartPartyExperienceFlow(request->expected.mon.slot)) result.status = RHH_EXPERIENCE_AWARD_UNAVAILABLE;
        if (result.status != RHH_EXPERIENCE_AWARD_PENDING)
        {
            sExperienceAward.active = FALSE; sExperienceAward.result = previous; sExperienceAward.run = previousRun;
            result.ticket = 0;
        }
        else sExperienceAward.nextTicket = result.ticket;
    }
    if (result.status != RHH_EXPERIENCE_AWARD_PENDING) { gRngValue = rng; gRng2Value = rng2; }
    *out = result; return result.status;
}
bool32 ModsExperienceFlowApply(u32 slot, u32 *beforeLevel, u32 *afterLevel, u32 *applied)
{
    if (!sExperienceAward.active || sExperienceAward.result.status != RHH_EXPERIENCE_AWARD_PENDING
        || slot != sExperienceAward.request.expected.mon.slot || sExperienceAward.run != RhhModsRunId()) return FALSE;
    struct RhhExperiencePreview current = {.struct_size = sizeof(current)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    ExperienceAwardDispatch(RHH_CAN_AWARD_EXPERIENCE, &decision);
    if (!decision.allowed)
    {
        gRngValue = rng; gRng2Value = rng2; sExperienceAward.result.status = RHH_EXPERIENCE_AWARD_DENIED;
        snprintf(sExperienceAward.result.reason, sizeof(sExperienceAward.result.reason), "%s", decision.reason);
        return FALSE;
    }
    if (PreviewOwnedExperience(&sExperienceAward.request.expected, sExperienceAward.request.amount, &current)
        || sExperienceAward.run != RhhModsRunId()
        || memcmp(&current, &sExperienceAward.result.experience, sizeof(current))
        || !GetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_HP)
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
    { gRngValue = rng; gRng2Value = rng2; sExperienceAward.result.status = RHH_EXPERIENCE_AWARD_STALE; return FALSE; }
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][slot], saved = *mon;
    u8 oldLevelUpHp = gBattleScripting.levelUpHP;
    u32 points = current.after_exp;
    SetMonData(mon, MON_DATA_EXP, &points); CalculateMonStats(mon);
    if (GetMonData(mon, MON_DATA_EXP) != current.after_exp || GetMonData(mon, MON_DATA_LEVEL) != current.after_level
        || sExperienceAward.run != RhhModsRunId() || memcmp(&rng, &gRngValue, sizeof(rng))
        || memcmp(&rng2, &gRng2Value, sizeof(rng2))
        || RhhModsPhotoExperienceCommit(sExperienceAward.result.ticket, current.mon.identity))
    {
        *mon = saved; gBattleScripting.levelUpHP = oldLevelUpHp; gRngValue = rng; gRng2Value = rng2;
        sExperienceAward.result.status = RHH_EXPERIENCE_AWARD_STALE; return FALSE;
    }
    *beforeLevel = current.before_level; *afterLevel = current.after_level; *applied = current.applied;
    sExperienceAward.result.status = RHH_EXPERIENCE_AWARD_OK;
    decision = (struct RhhModDecision){.struct_size = sizeof(decision), .allowed = 1};
    ExperienceAwardDispatch(RHH_EXPERIENCE_AWARDED, &decision);
    RhhModsPhotoExperienceNotify(sExperienceAward.result.ticket);
    return TRUE;
}
void ModsExperienceFlowFinish(void)
{
    if (!sExperienceAward.active) return;
    if (sExperienceAward.result.status == RHH_EXPERIENCE_AWARD_PENDING)
        sExperienceAward.result.status = RHH_EXPERIENCE_AWARD_UNAVAILABLE;
    sExperienceAward.result.finished = TRUE;
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    ExperienceAwardDispatch(RHH_EXPERIENCE_FLOW_FINISHED, &decision);
    RhhModsPhotoExperienceFinish(sExperienceAward.result.ticket);
    sExperienceAward.active = FALSE;
}
static void IdentityScan(u64 identity, struct RhhOwnedIdentityQuery *result,
                         struct RhhOwnedIdentityOccurrences *occurrences)
{
    for (u32 i = 0; i < PARTY_SIZE && i < gPartiesCount[B_TRAINER_PLAYER]; ++i)
        IdentityAdd(&gParties[B_TRAINER_PLAYER][i].box, RHH_MON_PARTY, 0, i, identity, result, &result->party_matches, occurrences);
    for (u32 box = 0; box < TOTAL_BOXES_COUNT; ++box)
        for (u32 slot = 0; slot < IN_BOX_COUNT; ++slot)
            IdentityAdd(&gPokemonStoragePtr->boxes[box][slot], RHH_MON_BOX, box, slot, identity, result, &result->box_matches, occurrences);
    for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
        IdentityAdd(&gSaveBlock1Ptr->daycare.mons[i].mon, RHH_MON_DAYCARE, 0, i, identity, result, &result->daycare_matches, occurrences);
#if IS_FRLG
    IdentityAdd(&gSaveBlock1Ptr->route5DayCareMon.mon, RHH_MON_DAYCARE, 1, 0, identity, result, &result->daycare_matches, occurrences);
#endif
    for (u32 i = 0; i < MAX_FUSION_STORAGE; ++i)
        IdentityAdd(&gPokemonStoragePtr->fusions[i].box, RHH_MON_FUSION_STORAGE, 0, i, identity, result, &result->fusion_matches, occurrences);
}
static int IdentityQuery(u64 identity, struct RhhOwnedIdentityQuery *out)
{
    if (!out || out->struct_size < sizeof(*out) || !OwnedStorageAvailable()) return -1;
    struct RhhOwnedIdentityQuery result = {.struct_size = sizeof(result), .unique.struct_size = sizeof(result.unique)};
    IdentityScan(identity, &result, NULL);
    result.unique_valid = result.matches == 1 && !result.flags;
    if (!result.unique_valid) result.unique = (struct RhhOwnedMonSnapshot){.struct_size = sizeof(result.unique)};
    *out = result; return 0;
}
static int IdentityOccurrences(u64 identity, struct RhhOwnedIdentityOccurrences *out)
{
    if (!out || out->struct_size < sizeof(*out) || !OwnedStorageAvailable()) return -1;
    struct RhhOwnedIdentityQuery summary = {.struct_size = sizeof(summary), .unique.struct_size = sizeof(summary.unique)};
    struct RhhOwnedIdentityOccurrences result = {.struct_size = sizeof(result), .identity = identity};
    IdentityScan(identity, &summary, &result);
    result.matches = summary.matches;
    *out = result; return 0;
}
static bool32 UniqueOwnedIdentity(u64 identity)
{
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    return !IdentityQuery(identity, &query) && query.unique_valid;
}
static bool32 sFieldHealBusy;
static const struct RhhFieldHealResult *sFieldHealContext;
static const struct RhhFieldHealEnvironmentRequest *sFieldHealEnvironmentContext;
static const struct RhhFieldHabitatGuard *sFieldHabitatContext;
static bool32 FieldHabitatValid(const struct RhhFieldHabitatGuard *guard)
{
    return guard && guard->struct_size >= sizeof(*guard) && !guard->reserved && guard->run_id
        && guard->expected.struct_size >= sizeof(guard->expected) && !guard->expected.reserved
        && guard->expected.map && guard->expected.permitted == 1 && guard->expected.environment_available <= 1
        && guard->expected.tile.struct_size >= sizeof(guard->expected.tile);
}
static int FieldHabitatContext(struct RhhFieldHabitatGuard *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sFieldHabitatContext
        || (event != RHH_CAN_FIELD_HEAL && event != RHH_FIELD_HEALED
            && event != RHH_CAN_CURE_FIELD_STATUS && event != RHH_FIELD_STATUS_CURED)) return -1;
    *out = *sFieldHabitatContext; return 0;
}
static bool32 FieldHabitatMatches(const struct RhhFieldHabitatGuard *guard)
{
    if (!guard) return TRUE;
    const struct RhhHabitatService *habitat = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/habitat", RHH_SERVICE_VERSION(1, 3));
    struct RhhHabitatSample current = {.struct_size = sizeof(current)};
    const struct RhhHabitatSample *expected = &guard->expected;
    if (!FieldHabitatValid(guard) || guard->run_id != RhhModsRunId()
        || !habitat || habitat->player(&current) || !current.permitted) return FALSE;
    return current.map == expected->map && current.x == expected->x && current.y == expected->y
        && current.base_flags == expected->base_flags && current.flags == expected->flags
        && current.natural_weather == expected->natural_weather && current.engine_weather == expected->engine_weather
        && current.environment_available == expected->environment_available
        && !memcmp(&current.tile, &expected->tile, sizeof(current.tile))
        && !memcmp(&current.environment, &expected->environment, sizeof(current.environment));
}
static int FieldHealthAt(uint32_t slot, struct RhhFieldHealth *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun()
        || gMain.inBattle || !OwnedStorageAvailable()) return -1;
    struct RhhFieldHealth health = {.struct_size = sizeof(health), .mon.struct_size = sizeof(health.mon)};
    if (PartyAt(slot, &health.mon)) return -1;
    struct Pokemon mon = gParties[B_TRAINER_PLAYER][slot];
    health.hp = mon.hp; health.max_hp = mon.maxHP;
    if (!health.max_hp || health.hp > health.max_hp) return -1;
    *out = health; return 0;
}
static int FieldHealContext(struct RhhFieldHealResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sFieldHealContext
        || (event != RHH_CAN_FIELD_HEAL && event != RHH_FIELD_HEALED)) return -1;
    *out = *sFieldHealContext; return 0;
}
static int FieldHealEnvironmentContext(struct RhhFieldHealEnvironmentRequest *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sFieldHealEnvironmentContext
        || (event != RHH_CAN_FIELD_HEAL && event != RHH_FIELD_HEALED)) return -1;
    *out = *sFieldHealEnvironmentContext; return 0;
}
static bool32 FieldHealEnvironmentMatches(const struct RhhFieldHealEnvironmentRequest *expected)
{
    if (!expected) return TRUE;
    const struct RhhWorldEnvironmentService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    struct RhhWorldEnvironmentSnapshot current = {.struct_size = sizeof(current)};
    if (!service || expected->expected_run_id != RhhModsRunId()
        || service->current(&current) || current.map != expected->expected_map) return FALSE;
    return (!(expected->flags & RHH_FIELD_HEAL_REQUIRE_WEATHER) || current.weather.value == expected->expected_weather)
        && (!(expected->flags & RHH_FIELD_HEAL_REQUIRE_TERRAIN) || current.terrain.value == expected->expected_terrain)
        && (!(expected->flags & RHH_FIELD_HEAL_REQUIRE_STEP) || current.step == expected->expected_step);
}
static int FieldHeal(const struct RhhFieldHealRequest *request, const struct RhhFieldHealEnvironmentRequest *environment,
                     const struct RhhFieldHabitatGuard *habitat, struct RhhFieldHealResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_FIELD_HEAL_INVALID;
    struct RhhFieldHealResult result = {.struct_size = sizeof(result), .status = RHH_FIELD_HEAL_INVALID,
        .is_preview = preview};
    if (!request || request->struct_size < sizeof(*request) || request->expected.struct_size < sizeof(request->expected)
        || request->expected.mon.struct_size < sizeof(request->expected.mon) || request->expected.mon.location != RHH_MON_PARTY
        || request->reserved[0] || request->reserved[1] || request->expected.reserved || !request->amount
        || !request->owner || !RhhModsRegisteredKey(request->owner)) goto done;
    if (environment && (environment->struct_size < sizeof(*environment) || !environment->expected_map || !environment->expected_run_id
        || (environment->flags & ~(RHH_FIELD_HEAL_REQUIRE_WEATHER | RHH_FIELD_HEAL_REQUIRE_TERRAIN | RHH_FIELD_HEAL_REQUIRE_STEP))
        || ((environment->flags & RHH_FIELD_HEAL_REQUIRE_WEATHER) ? environment->expected_weather > RHH_WEATHER_HEAVY_RAIN : environment->expected_weather != 0)
        || ((environment->flags & RHH_FIELD_HEAL_REQUIRE_TERRAIN) ? environment->expected_terrain > RHH_TERRAIN_PSYCHIC : environment->expected_terrain != 0)
        || (!(environment->flags & RHH_FIELD_HEAL_REQUIRE_STEP) && environment->expected_step))) goto done;
    struct RhhFieldHealRequest copy = *request; request = &copy; result.owner = request->owner;
    if (sFieldHealBusy || !ExperienceAwardReady()) { result.status = RHH_FIELD_HEAL_UNAVAILABLE; goto done; }
    sFieldHealBusy = TRUE;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    bool32 committed = FALSE;
    u64 run = RhhModsRunId();
    struct RhhFieldHealth current = {.struct_size = sizeof(current)};
    if (!FieldHabitatMatches(habitat) || FieldHealthAt(request->expected.mon.slot, &current)
        || !RhhMonSnapshotsEqual(&current.mon, &request->expected.mon)
        || current.hp != request->expected.hp || current.max_hp != request->expected.max_hp
        || !UniqueOwnedIdentity(current.mon.identity) || !FieldHealEnvironmentMatches(environment))
    { result.status = RHH_FIELD_HEAL_STALE; goto unlock; }
    result.before = result.after = current;
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][current.mon.slot], original = *mon, staged = *mon;
    if (current.mon.is_egg || !current.hp || ModsMonIsDead(&staged))
    { result.status = RHH_FIELD_HEAL_DENIED; goto unlock; }
    if (current.hp == current.max_hp) { result.status = RHH_FIELD_HEAL_NOT_NEEDED; goto unlock; }
    u32 amount = request->amount < current.max_hp - current.hp ? request->amount : current.max_hp - current.hp;
    u16 target = current.hp + amount;
    SetMonData(&staged, MON_DATA_HP, &target);
    if (staged.hp != target) { result.status = RHH_FIELD_HEAL_DENIED; goto unlock; }
    result.applied = amount; result.after.hp = target; result.status = RHH_FIELD_HEAL_OK;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_FIELD_HEAL,
        .run_seed = RhhModsRunSeed(), .event_key = request->owner, .subject = current.mon.identity,
        .args = {current.mon.slot, request->amount, current.hp, preview}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = amount};
    sFieldHealContext = &result; sFieldHealEnvironmentContext = environment; sFieldHabitatContext = habitat;
    RhhModsDispatch(&event, &decision); sFieldHealContext = NULL; sFieldHealEnvironmentContext = NULL; sFieldHabitatContext = NULL;
    if (!decision.allowed)
    { result.status = RHH_FIELD_HEAL_DENIED; memcpy(result.reason, decision.reason, sizeof(result.reason)); goto unlock; }
    if (!FieldHabitatMatches(habitat) || run != RhhModsRunId() || !ExperienceAwardReady() || memcmp(mon, &original, sizeof(original))
        || !UniqueOwnedIdentity(current.mon.identity) || !FieldHealEnvironmentMatches(environment))
    { result.status = RHH_FIELD_HEAL_STALE; goto unlock; }
    if (memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
    { result.status = RHH_FIELD_HEAL_DENIED; goto unlock; }
    if (!preview)
    {
        *mon = staged;
        committed = TRUE;
        event.kind = RHH_FIELD_HEALED;
        sFieldHealContext = &result; sFieldHealEnvironmentContext = environment; sFieldHabitatContext = habitat;
        RhhModsDispatch(&event, &decision); sFieldHealContext = NULL; sFieldHealEnvironmentContext = NULL; sFieldHabitatContext = NULL;
        gRngValue = rng; gRng2Value = rng2;
    }
unlock:
    if (!committed && (memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))))
    { gRngValue = rng; gRng2Value = rng2; result.status = RHH_FIELD_HEAL_DENIED; }
    sFieldHealBusy = FALSE;
done:
    *out = result; return result.status;
}
static int PreviewFieldHeal(const struct RhhFieldHealRequest *request, struct RhhFieldHealResult *out)
{ return FieldHeal(request, NULL, NULL, out, TRUE); }
static int CommitFieldHeal(const struct RhhFieldHealRequest *request, struct RhhFieldHealResult *out)
{ return FieldHeal(request, NULL, NULL, out, FALSE); }
static int FieldHealWithEnvironment(const struct RhhFieldHealEnvironmentRequest *request, struct RhhFieldHealResult *out, bool32 preview)
{
    if (!request || request->struct_size < sizeof(*request)) return FieldHeal(NULL, NULL, NULL, out, preview);
    struct RhhFieldHealEnvironmentRequest copy = *request;
    return FieldHeal(&copy.heal, &copy, NULL, out, preview);
}
static int PreviewFieldHealEnvironment(const struct RhhFieldHealEnvironmentRequest *request, struct RhhFieldHealResult *out)
{ return FieldHealWithEnvironment(request, out, TRUE); }
static int CommitFieldHealEnvironment(const struct RhhFieldHealEnvironmentRequest *request, struct RhhFieldHealResult *out)
{ return FieldHealWithEnvironment(request, out, FALSE); }
static const struct RhhFieldStatusResult *sFieldStatusContext;
static u32 FieldConditionFlags(u32 raw)
{
    return (raw & STATUS1_SLEEP ? RHH_FIELD_SLEEP : 0) | (raw & STATUS1_POISON ? RHH_FIELD_POISON : 0)
        | (raw & STATUS1_TOXIC_POISON ? RHH_FIELD_TOXIC : 0) | (raw & STATUS1_BURN ? RHH_FIELD_BURN : 0)
        | (raw & STATUS1_FREEZE ? RHH_FIELD_FREEZE : 0) | (raw & STATUS1_PARALYSIS ? RHH_FIELD_PARALYSIS : 0)
        | (raw & STATUS1_FROSTBITE ? RHH_FIELD_FROSTBITE : 0);
}
static int FieldConditionAt(u32 slot, struct RhhFieldCondition *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhFieldCondition result = {.struct_size = sizeof(result), .health.struct_size = sizeof(result.health)};
    if (FieldHealthAt(slot, &result.health)) return -1;
    struct Pokemon mon = gParties[B_TRAINER_PLAYER][slot];
    result.raw_status = GetMonData(&mon, MON_DATA_STATUS); result.conditions = FieldConditionFlags(result.raw_status);
    *out = result; return 0;
}
static int FieldStatusContext(struct RhhFieldStatusResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sFieldStatusContext
        || (event != RHH_CAN_CURE_FIELD_STATUS && event != RHH_FIELD_STATUS_CURED)) return -1;
    *out = *sFieldStatusContext; return 0;
}
static bool32 FieldStatusEnvironmentMatches(const struct RhhFieldStatusRequest *request)
{
    if (!request->expected_map && !request->expected_run_id) return TRUE;
    const struct RhhFieldHealEnvironmentRequest environment = {.struct_size = sizeof(environment), .flags = request->flags,
        .expected_map = request->expected_map, .expected_run_id = request->expected_run_id,
        .expected_step = request->expected_step, .expected_weather = request->expected_weather, .expected_terrain = request->expected_terrain};
    return FieldHealEnvironmentMatches(&environment);
}
static int FieldStatusCure(const struct RhhFieldStatusRequest *request, const struct RhhFieldHabitatGuard *habitat, struct RhhFieldStatusResult *out, bool32 preview)
{
    enum { ALL = RHH_FIELD_SLEEP | RHH_FIELD_POISON | RHH_FIELD_TOXIC | RHH_FIELD_BURN
               | RHH_FIELD_FREEZE | RHH_FIELD_PARALYSIS | RHH_FIELD_FROSTBITE };
    if (!out || out->struct_size < sizeof(*out)) return RHH_FIELD_HEAL_INVALID;
    struct RhhFieldStatusResult result = {.struct_size = sizeof(result), .status = RHH_FIELD_HEAL_INVALID, .is_preview = preview};
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !request->cure || (request->cure & ~ALL)
        || !request->owner || !RhhModsRegisteredKey(request->owner) || request->expected.struct_size < sizeof(request->expected)
        || request->expected.reserved || request->expected.health.struct_size < sizeof(request->expected.health)
        || request->expected.health.reserved || request->expected.health.mon.struct_size < sizeof(request->expected.health.mon)
        || request->expected.health.mon.location != RHH_MON_PARTY
        || (request->flags & ~(RHH_FIELD_HEAL_REQUIRE_WEATHER | RHH_FIELD_HEAL_REQUIRE_TERRAIN | RHH_FIELD_HEAL_REQUIRE_STEP))
        || (!!request->expected_map != !!request->expected_run_id) || (request->flags && !request->expected_map)
        || ((request->flags & RHH_FIELD_HEAL_REQUIRE_WEATHER) ? request->expected_weather > RHH_WEATHER_HEAVY_RAIN : request->expected_weather != 0)
        || ((request->flags & RHH_FIELD_HEAL_REQUIRE_TERRAIN) ? request->expected_terrain > RHH_TERRAIN_PSYCHIC : request->expected_terrain != 0)
        || (!(request->flags & RHH_FIELD_HEAL_REQUIRE_STEP) && request->expected_step)) goto done;
    struct RhhFieldStatusRequest copy = *request; request = &copy; result.request = copy;
    if (sFieldHealBusy || !ExperienceAwardReady()) { result.status = RHH_FIELD_HEAL_UNAVAILABLE; goto done; }
    sFieldHealBusy = TRUE;
    rng_value_t rng = gRngValue, rng2 = gRng2Value; bool32 committed = FALSE; u64 run = RhhModsRunId();
    struct RhhFieldCondition current = {.struct_size = sizeof(current)};
    if (!FieldHabitatMatches(habitat) || FieldConditionAt(request->expected.health.mon.slot, &current)
        || !RhhMonSnapshotsEqual(&current.health.mon, &request->expected.health.mon)
        || current.health.hp != request->expected.health.hp || current.health.max_hp != request->expected.health.max_hp
        || current.raw_status != request->expected.raw_status || current.conditions != request->expected.conditions
        || !UniqueOwnedIdentity(current.health.mon.identity) || !FieldStatusEnvironmentMatches(request))
    { result.status = RHH_FIELD_HEAL_STALE; goto unlock; }
    result.before = result.after = current;
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][current.health.mon.slot], original = *mon, staged = *mon;
    if (current.health.mon.is_egg || !current.health.hp || ModsMonIsDead(&staged))
    { result.status = RHH_FIELD_HEAL_DENIED; goto unlock; }
    result.cured = current.conditions & request->cure;
    if (!result.cured) { result.status = RHH_FIELD_HEAL_NOT_NEEDED; goto unlock; }
    u32 remove = (result.cured & RHH_FIELD_SLEEP ? STATUS1_SLEEP : 0)
        | (result.cured & RHH_FIELD_POISON ? STATUS1_POISON : 0)
        | (result.cured & RHH_FIELD_TOXIC ? STATUS1_TOXIC_POISON | STATUS1_TOXIC_COUNTER : 0)
        | (result.cured & RHH_FIELD_BURN ? STATUS1_BURN : 0) | (result.cured & RHH_FIELD_FREEZE ? STATUS1_FREEZE : 0)
        | (result.cured & RHH_FIELD_PARALYSIS ? STATUS1_PARALYSIS : 0) | (result.cured & RHH_FIELD_FROSTBITE ? STATUS1_FROSTBITE : 0);
    u32 newStatus = current.raw_status & ~remove; SetMonData(&staged, MON_DATA_STATUS, &newStatus);
    if (GetMonData(&staged, MON_DATA_STATUS) != newStatus) { result.status = RHH_FIELD_HEAL_DENIED; goto unlock; }
    result.after.raw_status = newStatus; result.after.conditions = FieldConditionFlags(newStatus); result.status = RHH_FIELD_HEAL_OK;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_CURE_FIELD_STATUS,
        .run_seed = RhhModsRunSeed(), .event_key = request->owner, .subject = current.health.mon.identity,
        .args = {current.health.mon.slot, request->cure, result.cured, preview}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = result.cured};
    sFieldStatusContext = &result; sFieldHabitatContext = habitat; RhhModsDispatch(&event, &decision); sFieldStatusContext = NULL; sFieldHabitatContext = NULL;
    if (!decision.allowed) { result.status = RHH_FIELD_HEAL_DENIED; memcpy(result.reason, decision.reason, sizeof(result.reason)); goto unlock; }
    if (!FieldHabitatMatches(habitat) || run != RhhModsRunId() || !ExperienceAwardReady() || memcmp(mon, &original, sizeof(original))
        || !UniqueOwnedIdentity(current.health.mon.identity) || !FieldStatusEnvironmentMatches(request))
    { result.status = RHH_FIELD_HEAL_STALE; goto unlock; }
    if (memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
    { result.status = RHH_FIELD_HEAL_DENIED; goto unlock; }
    if (!preview)
    {
        *mon = staged; committed = TRUE; event.kind = RHH_FIELD_STATUS_CURED;
        sFieldStatusContext = &result; sFieldHabitatContext = habitat; RhhModsDispatch(&event, &decision); sFieldStatusContext = NULL; sFieldHabitatContext = NULL;
        gRngValue = rng; gRng2Value = rng2;
    }
unlock:
    if (!committed && (memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))))
    { gRngValue = rng; gRng2Value = rng2; result.status = RHH_FIELD_HEAL_DENIED; }
    sFieldHealBusy = FALSE;
done:
    *out = result; return result.status;
}
static int PreviewFieldStatus(const struct RhhFieldStatusRequest *request, struct RhhFieldStatusResult *out)
{ return FieldStatusCure(request, NULL, out, TRUE); }
static int CommitFieldStatus(const struct RhhFieldStatusRequest *request, struct RhhFieldStatusResult *out)
{ return FieldStatusCure(request, NULL, out, FALSE); }
static int FieldHabitatHeal(const struct RhhFieldHabitatHealRequest *request, struct RhhFieldHealResult *out, bool32 preview)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !FieldHabitatValid(&request->habitat))
        return FieldHeal(NULL, NULL, NULL, out, preview);
    struct RhhFieldHabitatHealRequest copy = *request;
    return FieldHeal(&copy.heal, NULL, &copy.habitat, out, preview);
}
static int FieldHabitatCure(const struct RhhFieldHabitatCureRequest *request, struct RhhFieldStatusResult *out, bool32 preview)
{
    if (!request || request->struct_size < sizeof(*request) || request->reserved || !FieldHabitatValid(&request->habitat))
        return FieldStatusCure(NULL, NULL, out, preview);
    struct RhhFieldHabitatCureRequest copy = *request;
    return FieldStatusCure(&copy.cure, &copy.habitat, out, preview);
}
static int PreviewFieldHabitatHeal(const struct RhhFieldHabitatHealRequest *r, struct RhhFieldHealResult *o)
{ return FieldHabitatHeal(r, o, TRUE); }
static int CommitFieldHabitatHeal(const struct RhhFieldHabitatHealRequest *r, struct RhhFieldHealResult *o)
{ return FieldHabitatHeal(r, o, FALSE); }
static int PreviewFieldHabitatCure(const struct RhhFieldHabitatCureRequest *r, struct RhhFieldStatusResult *o)
{ return FieldHabitatCure(r, o, TRUE); }
static int CommitFieldHabitatCure(const struct RhhFieldHabitatCureRequest *r, struct RhhFieldStatusResult *o)
{ return FieldHabitatCure(r, o, FALSE); }
static const struct RhhFieldPoisonContext *sFieldPoisonContext;
static struct ModsFieldPoisonCapture *sFieldPoisonCapture;
static int FieldPoisonContext(struct RhhFieldPoisonContext *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sFieldPoisonContext
        || (event != RHH_FIELD_POISON_DAMAGE && event != RHH_FIELD_POISON_DAMAGED)) return -1;
    *out = *sFieldPoisonContext; return 0;
}
static void SampleFieldPoisonHabitat(struct RhhFieldPoisonHabitatContext *out)
{
    *out = (struct RhhFieldPoisonHabitatContext){.struct_size = sizeof(*out), .sample.struct_size = sizeof(out->sample)};
    const struct RhhHabitatService *habitat = (const void *)RhhModsHostAPI()->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 3));
    out->available = habitat && !habitat->player(&out->sample);
    if (!out->available) memset(&out->sample, 0, sizeof(out->sample));
}
static int FieldPoisonHabitatContext(struct RhhFieldPoisonHabitatContext *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sFieldPoisonCapture
        || (event != RHH_FIELD_POISON_DAMAGE && event != RHH_FIELD_POISON_DAMAGED)) return -1;
    if (!sFieldPoisonCapture->habitatQueried)
    {
        sFieldPoisonCapture->habitatQueried = TRUE;
        SampleFieldPoisonHabitat(&sFieldPoisonCapture->habitat);
    }
    *out = sFieldPoisonCapture->habitat; return 0;
}
static bool32 FieldPoisonHabitatMatches(const struct RhhFieldPoisonHabitatContext *expected)
{
    struct RhhFieldPoisonHabitatContext current;
    SampleFieldPoisonHabitat(&current);
    if (current.available != expected->available) return FALSE;
    if (!current.available) return TRUE;
    const struct RhhHabitatSample *a = &current.sample, *b = &expected->sample;
    return a->map == b->map && a->x == b->x && a->y == b->y && a->permitted == b->permitted
        && a->base_flags == b->base_flags && a->flags == b->flags
        && a->natural_weather == b->natural_weather && a->engine_weather == b->engine_weather
        && a->environment_available == b->environment_available
        && !memcmp(&a->tile, &b->tile, sizeof(a->tile)) && !memcmp(&a->environment, &b->environment, sizeof(a->environment));
}
int ModsFieldPoisonPrepare(u32 slot, u32 hp, u32 minimum, struct ModsFieldPoisonCapture *out)
{
    if (!out || !RhhModsHasActiveRun() || sFieldPoisonContext || gMain.inBattle
        || (!RhhModsHasHook(RHH_FIELD_POISON_DAMAGE) && !RhhModsHasHook(RHH_FIELD_POISON_DAMAGED))) return FALSE;
    struct RhhFieldPoisonContext result = {.struct_size = sizeof(result), .run_id = RhhModsRunId(),
        .before.struct_size = sizeof(result.before), .environment.struct_size = sizeof(result.environment)};
    if (FieldConditionAt(slot, &result.before) || result.before.health.hp != hp
        || !(result.before.conditions & (RHH_FIELD_POISON | RHH_FIELD_TOXIC))) return FALSE;
    const struct RhhWorldEnvironmentService *environment = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    result.environment_available = environment && !environment->current(&result.environment);
    result.max_damage = hp > minimum ? hp - minimum : 0;
    result.base_damage = result.max_damage ? 1 : 0; result.selected_damage = result.base_damage;
    struct ModsFieldPoisonCapture capture = {.context = result};
    if (RhhModsHasHook(RHH_FIELD_POISON_DAMAGE))
    {
        struct Pokemon original = gParties[B_TRAINER_PLAYER][slot];
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_FIELD_POISON_DAMAGE,
            .run_seed = RhhModsRunSeed(), .event_key = result.environment_available ? result.environment.map : 0,
            .subject = result.before.health.mon.identity, .ordinal = result.environment_available ? result.environment.step : 0,
            .args = {slot, result.base_damage, result.max_damage, result.before.conditions}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = result.base_damage};
        sFieldPoisonContext = &result; sFieldPoisonCapture = &capture;
        RhhModsDispatch(&event, &decision); sFieldPoisonContext = NULL; sFieldPoisonCapture = NULL;
        bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
        bool32 valid = result.run_id == RhhModsRunId() && !memcmp(&original, &gParties[B_TRAINER_PLAYER][slot], sizeof(original));
        gRngValue = rng; gRng2Value = rng2;
        if (valid && result.environment_available)
        {
            struct RhhWorldEnvironmentSnapshot after = {.struct_size = sizeof(after)};
            valid = environment && !environment->current(&after) && !memcmp(&after, &result.environment, sizeof(after));
        }
        if (valid && capture.habitatQueried) valid = FieldPoisonHabitatMatches(&capture.habitat);
        if (valid) valid = result.run_id == RhhModsRunId() && !memcmp(&original, &gParties[B_TRAINER_PLAYER][slot], sizeof(original));
        if (!valid) return -1;
        if (pure && !decision.allowed) { result.selected_damage = 0; result.overridden = TRUE; }
        else if (pure && decision.value <= result.max_damage && decision.value != result.base_damage)
        { result.selected_damage = decision.value; result.overridden = TRUE; }
    }
    capture.context = result; *out = capture; return TRUE;
}
void ModsFieldPoisonCommitted(struct ModsFieldPoisonCapture *capture, u32 afterHp, bool32 reachedThreshold)
{
    struct RhhFieldPoisonContext *context = capture ? &capture->context : NULL;
    if (!context || !RhhModsHasHook(RHH_FIELD_POISON_DAMAGED) || sFieldPoisonContext
        || context->run_id != RhhModsRunId() || afterHp >= context->before.health.hp) return;
    context->after_hp = afterHp; context->applied = context->before.health.hp - afterHp;
    context->reached_threshold = !!reachedThreshold;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_FIELD_POISON_DAMAGED,
        .run_seed = RhhModsRunSeed(), .event_key = context->environment_available ? context->environment.map : 0,
        .subject = context->before.health.mon.identity, .ordinal = context->environment_available ? context->environment.step : 0,
        .args = {context->before.health.mon.slot, context->applied, afterHp, context->reached_threshold}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sFieldPoisonContext = context; sFieldPoisonCapture = capture;
    RhhModsDispatch(&event, &decision); sFieldPoisonContext = NULL; sFieldPoisonCapture = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static const struct RhhFieldHealingService sFieldHealingService = {
    {sizeof(sFieldHealingService), RHH_SERVICE_VERSION(1, 5)}, FieldHealthAt, PreviewFieldHeal, CommitFieldHeal, FieldHealContext,
    PreviewFieldHealEnvironment, CommitFieldHealEnvironment, FieldHealEnvironmentContext,
    FieldConditionAt, PreviewFieldStatus, CommitFieldStatus, FieldStatusContext, FieldPoisonContext,
    PreviewFieldHabitatHeal, CommitFieldHabitatHeal, PreviewFieldHabitatCure, CommitFieldHabitatCure, FieldHabitatContext,
    FieldPoisonHabitatContext
};
static bool32 sEvLimitBusy;
static const struct RhhEvLimits *sEvLimitContext;
/* EV previews and proposals are deterministic. An impure dispatch cannot
 * change a proposal, but an explicit denial remains sticky. */
static void DispatchEvProposal(const struct RhhModEvent *event, struct RhhModDecision *decision)
{
    struct RhhModDecision before = *decision;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    RhhModsDispatch(event, decision);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!pure)
    {
        if (!decision->allowed) { before.allowed = 0; memcpy(before.reason, decision->reason, sizeof(before.reason)); }
        *decision = before;
    }
}
static void DispatchEvObserver(const struct RhhModEvent *event, struct RhhModDecision *decision)
{
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    RhhModsDispatch(event, decision);
    gRngValue = rng; gRng2Value = rng2;
}
static int EvLimitContext(struct RhhEvLimits *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sEvLimitContext || RhhModsCurrentEvent() != RHH_EV_LIMITS) return -1;
    *out = *sEvLimitContext; return 0;
}
static int CalculateEvLimits(const struct RhhOwnedMonSnapshot *mon, u32 source, RhhContentKey item,
                             u32 defaultTotal, struct RhhEvLimits *out)
{
    if (sEvLimitBusy) return -1;
    struct RhhEvLimits result = {.struct_size = sizeof(result), .source = source, .total = defaultTotal,
        .item = item, .mon = *mon};
    for (u32 i = 0; i < NUM_STATS; ++i) result.per_stat[i] = MAX_PER_STAT_EVS;
    if (RhhModsHasHook(RHH_EV_LIMITS))
    {
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_EV_LIMITS,
            .run_seed = RhhModsRunSeed(), .event_key = mon->species, .subject = mon->identity, .args = {source, 0, 0, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = defaultTotal};
        memcpy(decision.stats, result.per_stat, sizeof(decision.stats));
        sEvLimitBusy = TRUE; sEvLimitContext = &result; DispatchEvProposal(&event, &decision); sEvLimitContext = NULL; sEvLimitBusy = FALSE;
        bool32 valid = decision.allowed && decision.value <= NUM_STATS * 255;
        for (u32 i = 0; i < NUM_STATS; ++i) if (decision.stats[i] > 255) valid = FALSE;
        if (valid) { result.total = decision.value; memcpy(result.per_stat, decision.stats, sizeof(result.per_stat)); }
    }
    *out = result; return 0;
}
void ModsEvLimits(struct Pokemon *mon, u32 source, u32 item, u32 defaultTotal, u16 *perStat, u32 *total)
{
    for (u32 i = 0; i < NUM_STATS; ++i) perStat[i] = MAX_PER_STAT_EVS;
    *total = defaultTotal;
    uintptr_t p = (uintptr_t)mon, begin = (uintptr_t)gParties[B_TRAINER_PLAYER];
    if (sEvLimitBusy || !RhhModsHasActiveRun() || !RhhModsHasHook(RHH_EV_LIMITS)
        || p < begin || p >= begin + sizeof(gParties[B_TRAINER_PLAYER]) || (p - begin) % sizeof(*mon)
        || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK
            | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_EREADER_TRAINER
            | BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_CATCH_TUTORIAL)))) return;
    struct RhhOwnedMonSnapshot snapshot = {.struct_size = sizeof(snapshot)};
    struct RhhEvLimits limits;
    if (PartyAt((p - begin) / sizeof(*mon), &snapshot) || snapshot.is_egg
        || CalculateEvLimits(&snapshot, source, item ? RhhContentKeyForId(RHH_ITEM, item) : 0, defaultTotal, &limits)) return;
    memcpy(perStat, limits.per_stat, sizeof(limits.per_stat)); *total = limits.total;
}
static int QueryEvLimits(const struct RhhOwnedMonSnapshot *expected, struct RhhEvLimits *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || !out || out->struct_size < sizeof(*out)
        || sEvLimitBusy || !RhhModsHasActiveRun() || !RhhModsCommandsAllowed() || gMain.inBattle || !OwnedStorageAvailable()) return -1;
    struct RhhOwnedMonSnapshot current = {.struct_size = sizeof(current)};
    int status = expected->location == RHH_MON_PARTY ? PartyAt(expected->slot, &current)
        : expected->location == RHH_MON_BOX ? BoxAt(expected->box, expected->slot, &current) : -1;
    if (status || current.is_egg || !RhhMonSnapshotsEqual(expected, &current) || !UniqueOwnedIdentity(current.identity)) return -1;
    return CalculateEvLimits(&current, RHH_EV_LIMIT_EDIT, 0, MAX_TOTAL_EVS, out);
}
static const struct RhhEvLimitService sEvLimitService = {
    {sizeof(sEvLimitService), RHH_SERVICE_VERSION(1, 1)}, QueryEvLimits, EvLimitContext
};
static bool32 sBattleEvBusy;
static const struct RhhBattleEvYield *sBattleEvContext;
static const struct RhhBattleEvPolicy *sBattleEvPolicy;
bool32 ModsBattleEvYieldEnabled(struct Pokemon *mon)
{
    uintptr_t p = (uintptr_t)mon, begin = (uintptr_t)gParties[B_TRAINER_PLAYER];
    return !sBattleEvBusy && RhhModsHasActiveRun() && gMain.inBattle
        && !(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK
            | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_EREADER_TRAINER
            | BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_CATCH_TUTORIAL))
        && p >= begin && p < begin + sizeof(gParties[B_TRAINER_PLAYER]) && (p - begin) % sizeof(*mon) == 0
        && (RhhModsHasHook(RHH_CAN_APPLY_BATTLE_EVS) || RhhModsHasHook(RHH_BATTLE_EVS_PROCESSED)
            || RhhModsHasHook(RHH_BATTLE_EV_PROPOSAL));
}
static int BattleEvContext(struct RhhBattleEvYield *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sBattleEvContext
        || (event != RHH_BATTLE_EV_PROPOSAL && event != RHH_CAN_APPLY_BATTLE_EVS
            && event != RHH_BATTLE_EVS_PROCESSED)) return -1;
    *out = *sBattleEvContext; return 0;
}
static int BattleEvPolicyContext(struct RhhBattleEvPolicy *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sBattleEvPolicy
        || (event != RHH_BATTLE_EV_PROPOSAL && event != RHH_CAN_APPLY_BATTLE_EVS
            && event != RHH_BATTLE_EVS_PROCESSED)) return -1;
    *out = *sBattleEvPolicy; return 0;
}
void ModsBattleEvYield(struct Pokemon *mon, u32 defeatedSpecies, const u8 *before,
                       const u8 *proposed, const u16 *earned)
{
    unsigned slot = mon - gParties[B_TRAINER_PLAYER];
    struct RhhBattleEvYield result = {.struct_size = sizeof(result), .mon.struct_size = sizeof(result.mon),
        .defeated_species = RhhContentKeyForId(RHH_SPECIES, defeatedSpecies)};
    if (PartyAt(slot, &result.mon) || !result.defeated_species)
    {
        for (u32 i = 0; i < NUM_STATS; ++i) if (proposed[i] != before[i])
            SetMonData(mon, MON_DATA_HP_EV + i, &proposed[i]);
        return;
    }
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    result.identity_unique = !IdentityQuery(result.mon.identity, &query) && query.unique_valid;
    for (u32 i = 0; i < NUM_STATS; ++i)
    { result.before[i] = result.after[i] = before[i]; result.proposed[i] = proposed[i]; result.earned[i] = earned[i]; }
    struct Pokemon original = *mon; u64 run = RhhModsRunId();
    struct RhhBattleEvPolicy policy = {.struct_size = sizeof(policy), .mode = RHH_BATTLE_EV_APPLY};
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_EV_PROPOSAL,
        .run_seed = RhhModsRunSeed(), .event_key = result.defeated_species, .subject = result.mon.identity, .args = {slot, 0, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = RHH_BATTLE_EV_APPLY};
    sBattleEvBusy = TRUE; sBattleEvContext = &result; sBattleEvPolicy = &policy;
    DispatchEvProposal(&event, &decision);
    if (decision.value == RHH_BATTLE_EV_BANK && RhhModsHasHook(RHH_BATTLE_EVS_PROCESSED))
        policy.mode = RHH_BATTLE_EV_BANK;
    decision.value = policy.mode;
    event.kind = RHH_CAN_APPLY_BATTLE_EVS;
    DispatchEvProposal(&event, &decision); sBattleEvContext = NULL; sBattleEvPolicy = NULL;
    if (memcmp(mon, &original, sizeof(original)) || run != RhhModsRunId() || !gMain.inBattle
        || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK
            | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_EREADER_TRAINER
            | BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_CATCH_TUTORIAL)))
    { sBattleEvBusy = FALSE; return; }
    policy.accepted = !!decision.allowed;
    if (decision.allowed && policy.mode == RHH_BATTLE_EV_APPLY)
    {
        for (u32 i = 0; i < NUM_STATS; ++i) if (proposed[i] != before[i])
            SetMonData(mon, MON_DATA_HP_EV + i, &proposed[i]);
        for (u32 i = 0; i < NUM_STATS; ++i) result.after[i] = GetMonData(mon, MON_DATA_HP_EV + i);
        result.applied = TRUE;
    }
    else if (!decision.allowed) snprintf(result.reason, sizeof(result.reason), "%s", decision.reason);
    else
    {
        /* Preserve the v1.0 observer contract: allowed means EVs were applied. */
        decision.allowed = 0;
        snprintf(result.reason, sizeof(result.reason), "yield set aside");
    }
    event.kind = RHH_BATTLE_EVS_PROCESSED;
    sBattleEvContext = &result; sBattleEvPolicy = &policy; DispatchEvObserver(&event, &decision);
    sBattleEvContext = NULL; sBattleEvPolicy = NULL; sBattleEvBusy = FALSE;
}
static const struct RhhBattleEvYieldService sBattleEvService = {
    {sizeof(sBattleEvService), RHH_SERVICE_VERSION(1, 2)}, BattleEvContext, BattleEvPolicyContext
};
static bool32 sItemEvBusy;
static const struct RhhItemEvYield *sItemEvContext;
static const struct RhhItemEvReduction *sItemEvReduction;
static int PreviewItemEv(const struct RhhOwnedMonSnapshot *expected, RhhContentKey item, struct RhhItemEvPreview *out);
static int ItemEvContext(struct RhhItemEvYield *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sItemEvContext
        || (event != RHH_ITEM_EV_PROPOSAL && event != RHH_ITEM_EV_PROCESSED)) return -1;
    *out = *sItemEvContext; return 0;
}
static int ItemEvReductionContext(struct RhhItemEvReduction *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sItemEvReduction
        || (event != RHH_CAN_REDUCE_ITEM_EVS && event != RHH_ITEM_EV_REDUCTION_PROCESSED)) return -1;
    *out = *sItemEvReduction; return 0;
}
static const struct RhhItemEvYieldService sItemEvService = {
    {sizeof(sItemEvService), RHH_SERVICE_VERSION(1, 3)}, ItemEvContext, ItemEvReductionContext, PreviewItemEv
};
static bool32 EvReductionChangesFriendship(struct Pokemon *mon, const u8 *effect)
{
    if (ShouldSkipFriendshipChange()) return FALSE;
    s32 friendship = GetMonData(mon, MON_DATA_FRIENDSHIP);
    u32 index = friendship < 100 ? 0 : friendship < 200 ? 1 : 2;
    s32 change = (s8)effect[ITEM_EFFECT_ARG_START + 1 + index];
    u32 holdEffect = GetItemHoldEffect(GetMonData(mon, MON_DATA_HELD_ITEM));
    s32 after = friendship + CalculateFriendshipBonuses(mon, change, holdEffect);
    return max(0, min(MAX_FRIENDSHIP, after)) != friendship;
}
static enum ModItemEvResult UseItemEvReduction(struct Pokemon *mon, u32 item, u32 partySlot,
                                                const u8 *effect, u32 mask, struct RhhItemEvPreview *preview)
{
    if (!RhhModsHasHook(RHH_CAN_REDUCE_ITEM_EVS) && !RhhModsHasHook(RHH_ITEM_EV_REDUCTION_PROCESSED))
        return MOD_ITEM_EV_UNHANDLED;
    s32 change = (s8)effect[ITEM_EFFECT_ARG_START];
    /* These engine item layouts have a single negative argument, or six
     * zero arguments for Fresh-Start Mochi. Other layouts use the normal path. */
    if (change < 0)
    {
        if (!mask || (mask & (mask - 1)) || (effect[5] & ITEM5_FRIENDSHIP_ALL) != ITEM5_FRIENDSHIP_ALL)
            return MOD_ITEM_EV_UNHANDLED;
    }
    else if (change || item != ITEM_FRESH_START_MOCHI || mask != ((1u << NUM_STATS) - 1)
             || (effect[5] & ITEM5_FRIENDSHIP_ALL)) return MOD_ITEM_EV_UNHANDLED;
    struct RhhItemEvReduction result = {.struct_size = sizeof(result), .stat_mask = mask, .change = change,
        .item = RhhContentKeyForId(RHH_ITEM, item), .mon.struct_size = sizeof(result.mon)};
    if (!result.item || PartyAt(partySlot, &result.mon) || result.mon.is_egg) return MOD_ITEM_EV_UNHANDLED;
    if (!CheckBagHasItem(item, 1)) return MOD_ITEM_EV_DENIED;
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    result.identity_unique = !IdentityQuery(result.mon.identity, &query) && query.unique_valid;
    bool32 changed = FALSE;
    for (u32 i = 0; i < NUM_STATS; ++i)
    {
        s32 value = result.mon.evs[i]; result.before[i] = result.after[i] = value;
        if (mask & (1u << i))
        {
            if (!change) value = 0;
            else if (value)
            {
                value += change;
                if (I_BERRY_EV_JUMP == GEN_4 && value > 100) value = 100;
                if (value < 0) value = 0;
            }
        }
        result.proposed[i] = value; changed |= result.before[i] != value;
    }
    if (preview) { preview->kind = RHH_ITEM_EV_PREVIEW_REDUCTION; preview->reduction = result; }
    if (!changed && (!change || !EvReductionChangesFriendship(mon, effect))) return MOD_ITEM_EV_DENIED;
    struct Pokemon original = *mon; u64 run = RhhModsRunId();
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_REDUCE_ITEM_EVS,
        .run_seed = RhhModsRunSeed(), .event_key = result.item, .subject = result.mon.identity,
        .args = {partySlot, mask, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sItemEvBusy = TRUE; sItemEvReduction = &result;
    DispatchEvProposal(&event, &decision); sItemEvReduction = NULL;
    if (preview) snprintf(preview->reason, sizeof(preview->reason), "%s", decision.reason);
    if (!decision.allowed || run != RhhModsRunId() || gMain.inBattle || NativeSaveTransactionActive()
        || memcmp(mon, &original, sizeof(original)) || !CheckBagHasItem(item, 1)
        || (!changed && !EvReductionChangesFriendship(mon, effect)))
    { sItemEvBusy = FALSE; return MOD_ITEM_EV_DENIED; }
    if (preview) {
        memcpy(result.after, result.proposed, sizeof(result.after)); preview->reduction = result;
        sItemEvBusy = FALSE; return MOD_ITEM_EV_APPLIED;
    }
    if (!RemoveBagItem(item, 1)) { sItemEvBusy = FALSE; return MOD_ITEM_EV_DENIED; }
    ExecuteTableBasedItemEffect(mon, item, partySlot, 0);
    for (u32 i = 0; i < NUM_STATS; ++i) result.after[i] = GetMonData(mon, MON_DATA_HP_EV + i);
    event.kind = RHH_ITEM_EV_REDUCTION_PROCESSED;
    sItemEvReduction = &result; DispatchEvObserver(&event, &decision); sItemEvReduction = NULL; sItemEvBusy = FALSE;
    return MOD_ITEM_EV_APPLIED;
}
static enum ModItemEvResult ItemEvRun(struct Pokemon *mon, u32 item, u32 partySlot, struct RhhItemEvPreview *preview)
{
    if (sItemEvBusy || !RhhModsHasActiveRun() || !RhhModsCommandsAllowed() || gMain.inBattle
        || NativeSaveTransactionActive() || !OwnedStorageAvailable() || partySlot >= PARTY_SIZE
        || mon != &gParties[B_TRAINER_PLAYER][partySlot]
        || (!RhhModsHasHook(RHH_ITEM_EV_PROPOSAL) && !RhhModsHasHook(RHH_ITEM_EV_PROCESSED)
            && !RhhModsHasHook(RHH_CAN_REDUCE_ITEM_EVS) && !RhhModsHasHook(RHH_ITEM_EV_REDUCTION_PROCESSED)))
        return MOD_ITEM_EV_UNHANDLED;
    const u8 *effect = GetItemEffect(item);
    if (!effect || effect[0] || effect[1] || effect[2] || effect[3]
        || (effect[4] & ~(ITEM4_EV_HP | ITEM4_EV_ATK)) || (effect[5] & ITEM5_PP_MAX)
        || ((effect[5] & ITEM5_FRIENDSHIP_ALL) && (effect[5] & ITEM5_FRIENDSHIP_ALL) != ITEM5_FRIENDSHIP_ALL))
        return MOD_ITEM_EV_UNHANDLED;
    u32 stat = NUM_STATS, flags4 = effect[4], flags5 = effect[5] & ~ITEM5_FRIENDSHIP_ALL;
    u32 mask = ((flags4 & ITEM4_EV_HP) ? 1u << STAT_HP : 0)
        | ((flags4 & ITEM4_EV_ATK) ? 1u << STAT_ATK : 0)
        | ((flags5 & ITEM5_EV_DEF) ? 1u << STAT_DEF : 0)
        | ((flags5 & ITEM5_EV_SPEED) ? 1u << STAT_SPEED : 0)
        | ((flags5 & ITEM5_EV_SPATK) ? 1u << STAT_SPATK : 0)
        | ((flags5 & ITEM5_EV_SPDEF) ? 1u << STAT_SPDEF : 0);
    if (!mask) return MOD_ITEM_EV_UNHANDLED;
    if (GetItemEffectType(item) == ITEM_EFFECT_HP_EV && HasShedinjaHPHandling(GetMonData(mon, MON_DATA_SPECIES)))
        return MOD_ITEM_EV_UNHANDLED;
    if ((s8)effect[ITEM_EFFECT_ARG_START] <= 0)
        return UseItemEvReduction(mon, item, partySlot, effect, mask, preview);
    if (!RhhModsHasHook(RHH_ITEM_EV_PROPOSAL) && !RhhModsHasHook(RHH_ITEM_EV_PROCESSED))
        return MOD_ITEM_EV_UNHANDLED;
    if (flags4 == ITEM4_EV_HP && !flags5) stat = STAT_HP;
    else if (flags4 == ITEM4_EV_ATK && !flags5) stat = STAT_ATK;
    else if (!flags4 && flags5 == ITEM5_EV_DEF) stat = STAT_DEF;
    else if (!flags4 && flags5 == ITEM5_EV_SPEED) stat = STAT_SPEED;
    else if (!flags4 && flags5 == ITEM5_EV_SPATK) stat = STAT_SPATK;
    else if (!flags4 && flags5 == ITEM5_EV_SPDEF) stat = STAT_SPDEF;
    if (stat >= NUM_STATS || (s8)effect[ITEM_EFFECT_ARG_START] <= 0
        || (stat == STAT_HP && HasShedinjaHPHandling(GetMonData(mon, MON_DATA_SPECIES))))
        return MOD_ITEM_EV_UNHANDLED;
    struct RhhItemEvYield result = {.struct_size = sizeof(result), .stat = stat,
        .earned = (s8)effect[ITEM_EFFECT_ARG_START], .item = RhhContentKeyForId(RHH_ITEM, item),
        .mon.struct_size = sizeof(result.mon)};
    if (!result.item || PartyAt(partySlot, &result.mon) || result.mon.is_egg) return MOD_ITEM_EV_UNHANDLED;
    if (!CheckBagHasItem(item, 1)) return MOD_ITEM_EV_DENIED;
    struct Pokemon original = *mon; u64 run = RhhModsRunId();
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    result.identity_unique = !IdentityQuery(result.mon.identity, &query) && query.unique_valid;
    u32 total = 0, cap = !B_EV_ITEMS_CAP ? MAX_TOTAL_EVS : GetCurrentEVCap(); u16 limits[NUM_STATS];
    sItemEvBusy = TRUE;
    ModsEvLimits(mon, RHH_EV_LIMIT_ITEM, item, cap, limits, &cap);
    for (u32 i = 0; i < NUM_STATS; ++i)
    { result.before[i] = result.proposed[i] = result.after[i] = result.mon.evs[i]; total += result.before[i]; }
    u32 statCap = limits[stat];
    if ((effect[10] & ITEM10_IS_VITAMIN) && I_VITAMIN_EV_CAP < GEN_8 && statCap > 100) statCap = 100;
    if (total < cap && result.before[stat] < statCap)
        result.proposed[stat] += min(result.earned, min(cap - total, statCap - result.before[stat]));
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_ITEM_EV_PROPOSAL,
        .run_seed = RhhModsRunSeed(), .event_key = result.item, .subject = result.mon.identity,
        .args = {partySlot, stat, result.earned, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = RHH_ITEM_EV_APPLY};
    sItemEvContext = &result; DispatchEvProposal(&event, &decision); sItemEvContext = NULL;
    u32 mode = decision.value <= RHH_ITEM_EV_BANK ? decision.value : RHH_ITEM_EV_APPLY;
    if (preview) {
        preview->kind = RHH_ITEM_EV_PREVIEW_GAIN; result.mode = mode; preview->gain = result;
        snprintf(preview->reason, sizeof(preview->reason), "%s", decision.reason);
    }
    if (!decision.allowed || run != RhhModsRunId() || gMain.inBattle || NativeSaveTransactionActive()
        || memcmp(mon, &original, sizeof(original)) || !CheckBagHasItem(item, 1)
        || (mode == RHH_ITEM_EV_APPLY && result.proposed[stat] == result.before[stat])
        || (mode == RHH_ITEM_EV_BANK && !RhhModsHasHook(RHH_ITEM_EV_PROCESSED)))
    { sItemEvBusy = FALSE; return MOD_ITEM_EV_DENIED; }
    if (preview) {
        if (mode == RHH_ITEM_EV_APPLY) memcpy(result.after, result.proposed, sizeof(result.after));
        preview->gain = result; sItemEvBusy = FALSE;
        return mode == RHH_ITEM_EV_BANK ? MOD_ITEM_EV_BANKED : MOD_ITEM_EV_APPLIED;
    }
    if (!RemoveBagItem(item, 1)) { sItemEvBusy = FALSE; return MOD_ITEM_EV_DENIED; }
    if (mode == RHH_ITEM_EV_APPLY)
    {
        u8 value = result.proposed[stat]; SetMonData(mon, MON_DATA_HP_EV + stat, &value);
        CalculateMonStats(mon);
    }
    if ((effect[5] & ITEM5_FRIENDSHIP_ALL) && !ShouldSkipFriendshipChange())
    {
        s32 friendship = GetMonData(mon, MON_DATA_FRIENDSHIP);
        u32 index = friendship < 100 ? 0 : friendship < 200 ? 1 : 2;
        s32 change = (s8)effect[ITEM_EFFECT_ARG_START + 1 + index];
        friendship += CalculateFriendshipBonuses(mon, change, GetItemHoldEffect(GetMonData(mon, MON_DATA_HELD_ITEM)));
        u8 value = max(0, min(MAX_FRIENDSHIP, friendship)); SetMonData(mon, MON_DATA_FRIENDSHIP, &value);
    }
    result.mode = mode;
    for (u32 i = 0; i < NUM_STATS; ++i) result.after[i] = GetMonData(mon, MON_DATA_HP_EV + i);
    event.kind = RHH_ITEM_EV_PROCESSED; decision.value = mode;
    sItemEvContext = &result; DispatchEvObserver(&event, &decision); sItemEvContext = NULL; sItemEvBusy = FALSE;
    return mode == RHH_ITEM_EV_BANK ? MOD_ITEM_EV_BANKED : MOD_ITEM_EV_APPLIED;
}
enum ModItemEvResult ModsUseItemEv(struct Pokemon *mon, u32 item, u32 partySlot)
{ return ItemEvRun(mon, item, partySlot, NULL); }
static int PreviewItemEv(const struct RhhOwnedMonSnapshot *expected, RhhContentKey item, struct RhhItemEvPreview *out)
{
    _Static_assert((int)MOD_ITEM_EV_UNHANDLED == (int)RHH_ITEM_EV_PREVIEW_UNHANDLED && (int)MOD_ITEM_EV_DENIED == (int)RHH_ITEM_EV_PREVIEW_DENIED
        && (int)MOD_ITEM_EV_APPLIED == (int)RHH_ITEM_EV_PREVIEW_APPLIED && (int)MOD_ITEM_EV_BANKED == (int)RHH_ITEM_EV_PREVIEW_BANKED,
        "EV item preview statuses match the field transaction");
    if (!expected || expected->struct_size < sizeof(*expected) || expected->location != RHH_MON_PARTY
        || expected->slot >= PARTY_SIZE || !out || out->struct_size < sizeof(*out)
        || sItemEvBusy || !RhhModsHasActiveRun() || !RhhModsCommandsAllowed() || gMain.inBattle || !OwnedStorageAvailable()) return -1;
    u32 itemId;
    struct RhhOwnedMonSnapshot current = {.struct_size = sizeof(current)};
    if (!item || RhhContentResolve(RHH_ITEM, item, &itemId) || PartyAt(expected->slot, &current)
        || current.is_egg || !RhhMonSnapshotsEqual(expected, &current)) return -1;
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][expected->slot], original = *mon;
    rng_value_t rng = gRngValue, rng2 = gRng2Value; u64 run = RhhModsRunId();
    struct RhhItemEvPreview result = {.struct_size = sizeof(result)};
    result.status = ItemEvRun(mon, itemId, expected->slot, &result);
    if (run != RhhModsRunId() || gMain.inBattle || !OwnedStorageAvailable() || memcmp(mon, &original, sizeof(original))
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))) {
        gRngValue = rng; gRng2Value = rng2; return -1;
    }
    *out = result; return 0;
}
#define OWNED_WATCH_BOX_SLOTS (TOTAL_BOXES_COUNT * IN_BOX_COUNT)
#define OWNED_WATCH_DAYCARE_SLOTS (DAYCARE_MON_COUNT + (IS_FRLG ? 1 : 0))
#define OWNED_WATCH_SLOTS (PARTY_SIZE + OWNED_WATCH_BOX_SLOTS + OWNED_WATCH_DAYCARE_SLOTS + MAX_FUSION_STORAGE)
struct OwnedWatchSlot {
    struct BoxPokemon raw;
    struct RhhOwnedMonSnapshot snapshot;
    u32 experience, hyperTraining, tera;
    bool32 eligible, present;
};
static struct OwnedWatchSlot sOwnedWatch[OWNED_WATCH_SLOTS];
static struct RhhOwnedMonChange sOwnedChanges[OWNED_WATCH_SLOTS];
static struct RhhOwnedExperienceChange sOwnedExperienceChanges[OWNED_WATCH_SLOTS];
static const struct RhhOwnedMonChange *sOwnedChangeContext;
static const struct RhhOwnedExperienceChange *sOwnedExperienceChangeContext;
static const struct RhhOwnedRelease *sOwnedReleaseContext;
static const struct RhhOwnedMove *sOwnedMoveContext;
static const struct RhhOwnedPickup *sOwnedPickupContext;
static const struct RhhOwnedStorageUnreadable *sOwnedStorageUnreadableContext;
static u64 sOwnedWatchRun, sOwnedWatchSequence;
static u64 sOwnedReleaseRun, sOwnedReleaseSequence;
static u64 sOwnedMoveRun, sOwnedMoveSequence;
static u64 sOwnedPickupRun, sOwnedPickupSequence;
static u64 sOwnedStorageUnreadableRun, sOwnedStorageUnreadableSequence;
static u64 sPartyReorderRun, sPartyReorderSequence;
static u64 sDaycareTransferRun, sDaycareTransferSequence;
static u64 sFusionTransferRun, sFusionTransferSequence;
static u64 sCatchPlacementRun, sCatchPlacementSequence;
static u64 sOwnedGiftRun, sOwnedGiftSequence, sOwnedDebugGiftRun, sOwnedDebugGiftSequence;
static u64 sOwnedMysteryGiftRun, sOwnedMysteryGiftSequence;
static u64 sOwnedEvolutionRun, sOwnedEvolutionSequence;
static u64 sIngameTradeRun, sIngameTradeSequence;
static u64 sLinkTradeRun, sLinkTradeSequence;
enum { LINK_HISTORY_HEADER_V1 = 32, LINK_HISTORY_HEADER = 64, LINK_HISTORY_SNAPSHOT = 80,
       LINK_HISTORY_RECORD_V1 = 24 + 3 * LINK_HISTORY_SNAPSHOT, LINK_HISTORY_RECORD = LINK_HISTORY_RECORD_V1 + 32 };
static struct RhhSavedLinkTrade sLinkHistory[RHH_LINK_TRADE_HISTORY_CAPACITY], sLinkHistoryStage, sLinkHistoryLast;
static u8 sLinkHistoryAnchor[32], sLinkHistoryHashes[RHH_LINK_TRADE_HISTORY_CAPACITY][32], sLinkHistoryStageHash[32];
static u32 sLinkHistoryCount;
static u64 sLinkHistoryRun, sLinkHistoryNext;
static bool32 sLinkHistoryStaged, sLinkHistoryLastValid;
static u16 HistoryRead16(const u8 *p) { return (u16)p[0] | (u16)p[1] << 8; }
static u32 HistoryRead32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u64 HistoryRead64(const u8 *p) { return (u64)HistoryRead32(p) | (u64)HistoryRead32(p + 4) << 32; }
static void HistoryWrite16(u8 *p, u16 n) { p[0] = n; p[1] = n >> 8; }
static void HistoryWrite32(u8 *p, u32 n) { for (u32 i = 0; i < 4; ++i) p[i] = n >> (i * 8); }
static void HistoryWrite64(u8 *p, u64 n) { HistoryWrite32(p, n); HistoryWrite32(p + 4, n >> 32); }
static void HistoryWriteMon(u8 *p, const struct RhhOwnedMonSnapshot *mon)
{
    HistoryWrite32(p, mon->location); HistoryWrite32(p + 4, mon->box); HistoryWrite32(p + 8, mon->slot);
    HistoryWrite64(p + 12, mon->identity); HistoryWrite64(p + 20, mon->species);
    HistoryWrite32(p + 28, mon->level); HistoryWrite32(p + 32, mon->nature);
    HistoryWrite32(p + 36, mon->stat_nature); HistoryWrite32(p + 40, mon->is_egg);
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i)
    { HistoryWrite16(p + 44 + 2 * i, mon->ivs[i]); HistoryWrite16(p + 56 + 2 * i, mon->evs[i]); }
    HistoryWrite32(p + 68, mon->max_iv); HistoryWrite32(p + 72, mon->max_stat_ev);
    HistoryWrite32(p + 76, mon->max_total_ev);
}
static void HistoryReadMon(const u8 *p, struct RhhOwnedMonSnapshot *mon)
{
    *mon = (struct RhhOwnedMonSnapshot){.struct_size = sizeof(*mon), .location = HistoryRead32(p),
        .box = HistoryRead32(p + 4), .slot = HistoryRead32(p + 8), .identity = HistoryRead64(p + 12),
        .species = HistoryRead64(p + 20), .level = HistoryRead32(p + 28), .nature = HistoryRead32(p + 32),
        .stat_nature = HistoryRead32(p + 36), .is_egg = HistoryRead32(p + 40),
        .max_iv = HistoryRead32(p + 68), .max_stat_ev = HistoryRead32(p + 72), .max_total_ev = HistoryRead32(p + 76)};
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i)
    { mon->ivs[i] = HistoryRead16(p + 44 + 2 * i); mon->evs[i] = HistoryRead16(p + 56 + 2 * i); }
}
static void HistoryWriteRecord(u8 *p, const struct RhhSavedLinkTrade *entry)
{
    memset(p, 0, LINK_HISTORY_RECORD_V1);
    HistoryWrite64(p, entry->serial); HistoryWrite32(p + 8, entry->trade.flags);
    HistoryWrite32(p + 12, entry->trade.party_slot); HistoryWrite32(p + 16, entry->trade.transport);
    HistoryWriteMon(p + 24, &entry->trade.sent);
    HistoryWriteMon(p + 24 + LINK_HISTORY_SNAPSHOT, &entry->trade.received_before);
    HistoryWriteMon(p + 24 + 2 * LINK_HISTORY_SNAPSHOT, &entry->trade.received_after);
}
static void HistoryHash(u64 run, const u8 previous[32], const u8 *record, u8 out[32])
{
    u8 input[48 + LINK_HISTORY_RECORD_V1];
    memcpy(input, "RHLINK02", 8); HistoryWrite64(input + 8, run);
    memcpy(input + 16, previous, 32); memcpy(input + 48, record, LINK_HISTORY_RECORD_V1);
    NativeMusicSha256(input, sizeof(input), out);
}
static void HistoryLegacyAnchor(u64 run, u64 first, u8 out[32])
{
    memset(out, 0, 32);
    if (first > 1)
    {
        u8 input[24]; memcpy(input, "RHLGCY01", 8);
        HistoryWrite64(input + 8, run); HistoryWrite64(input + 16, first - 1);
        NativeMusicSha256(input, sizeof(input), out);
    }
}
static void LinkHistoryReset(void)
{
    memset(sLinkHistory, 0, sizeof(sLinkHistory)); memset(&sLinkHistoryStage, 0, sizeof(sLinkHistoryStage));
    memset(&sLinkHistoryLast, 0, sizeof(sLinkHistoryLast));
    memset(sLinkHistoryAnchor, 0, sizeof(sLinkHistoryAnchor));
    memset(sLinkHistoryHashes, 0, sizeof(sLinkHistoryHashes)); memset(sLinkHistoryStageHash, 0, sizeof(sLinkHistoryStageHash));
    sLinkHistoryCount = 0; sLinkHistoryRun = 0; sLinkHistoryNext = 1;
    sLinkHistoryStaged = sLinkHistoryLastValid = FALSE; sLinkTradeReceiptContext = NULL;
}
static int LinkHistoryValidate(const u8 *data, u32 size)
{
    if (!size) return data ? -1 : 0;
    if (!data || size < LINK_HISTORY_HEADER_V1) return -1;
    bool32 legacy = !memcmp(data, "RHLTR001", 8);
    if (!legacy && (size < LINK_HISTORY_HEADER || memcmp(data, "RHLTR002", 8))) return -1;
    u32 header = legacy ? LINK_HISTORY_HEADER_V1 : LINK_HISTORY_HEADER;
    u32 recordSize = legacy ? LINK_HISTORY_RECORD_V1 : LINK_HISTORY_RECORD;
    if (size < header
        || HistoryRead32(data + 8) != size || !HistoryRead32(data + 12)
        || HistoryRead32(data + 12) > RHH_LINK_TRADE_HISTORY_CAPACITY
        || size != header + HistoryRead32(data + 12) * recordSize
        || !HistoryRead64(data + 16) || !HistoryRead64(data + 24)) return -1;
    u64 previous = 0, next = HistoryRead64(data + 24);
    u8 previousHash[32] = {0};
    if (!legacy) memcpy(previousHash, data + LINK_HISTORY_HEADER_V1, sizeof(previousHash));
    if (!legacy && HistoryRead64(data + header) == 1)
        for (u32 i = 0; i < sizeof(previousHash); ++i) if (previousHash[i]) return -1;
    for (u32 i = 0; i < HistoryRead32(data + 12); ++i)
    {
        const u8 *p = data + header + i * recordSize;
        u64 serial = HistoryRead64(p); u32 flags = HistoryRead32(p + 8), slot = HistoryRead32(p + 12);
        u32 transport = HistoryRead32(p + 16);
        if (!serial || serial <= previous || serial >= next || flags & ~7u || slot >= PARTY_SIZE
            || (transport != RHH_LINK_TRADE_CABLE && transport != RHH_LINK_TRADE_WIRELESS)
            || HistoryRead32(p + 20)) return -1;
        for (u32 j = 0; j < 3; ++j)
        {
            const u8 *mon = p + 24 + j * LINK_HISTORY_SNAPSHOT;
            if (flags & (1u << j))
            {
                if (HistoryRead32(mon) != (j == 1 ? RHH_MON_STAGED : RHH_MON_PARTY)
                    || HistoryRead32(mon + 4) || HistoryRead32(mon + 8) != (j == 1 ? 0 : slot)
                    || !HistoryRead64(mon + 20)) return -1;
            }
            else for (u32 k = 0; k < LINK_HISTORY_SNAPSHOT; ++k) if (mon[k]) return -1;
        }
        if (!legacy)
        {
            u8 hash[32]; HistoryHash(HistoryRead64(data + 16), previousHash, p, hash);
            if (memcmp(hash, p + LINK_HISTORY_RECORD_V1, sizeof(hash))) return -1;
            memcpy(previousHash, hash, sizeof(hash));
        }
        previous = serial;
    }
    return 0;
}
static int LinkHistoryExport(u8 **out, u32 *size)
{
    if (!out || !size) return -1;
    *out = NULL; *size = 0;
    bool32 staged = sLinkHistoryStaged && NativeSaveArchiveSaveExportActive();
    u32 skip = staged && sLinkHistoryCount == RHH_LINK_TRADE_HISTORY_CAPACITY;
    u32 count = sLinkHistoryCount - skip + (staged ? 1 : 0);
    if (!count) return 0;
    u32 length = LINK_HISTORY_HEADER + count * LINK_HISTORY_RECORD;
    u8 *data = calloc(length, 1); if (!data) return -1;
    memcpy(data, "RHLTR002", 8); HistoryWrite32(data + 8, length); HistoryWrite32(data + 12, count);
    HistoryWrite64(data + 16, staged ? sLinkHistoryStage.trade.run_id : sLinkHistoryRun);
    HistoryWrite64(data + 24, staged ? sLinkHistoryStage.serial + 1 : sLinkHistoryNext);
    memcpy(data + LINK_HISTORY_HEADER_V1, skip ? sLinkHistoryHashes[0] : sLinkHistoryAnchor, 32);
    for (u32 i = 0; i < count; ++i)
    {
        const struct RhhSavedLinkTrade *entry = staged && i == count - 1 ? &sLinkHistoryStage : &sLinkHistory[i + skip];
        u8 *p = data + LINK_HISTORY_HEADER + i * LINK_HISTORY_RECORD;
        HistoryWriteRecord(p, entry);
        memcpy(p + LINK_HISTORY_RECORD_V1, staged && i == count - 1 ? sLinkHistoryStageHash : sLinkHistoryHashes[i + skip], 32);
    }
    *out = data; *size = length; return 0;
}
static int LinkHistoryImport(const u8 *data, u32 size)
{
    if (LinkHistoryValidate(data, size)) return -1;
    LinkHistoryReset(); if (!size) return 0;
    bool32 legacy = !memcmp(data, "RHLTR001", 8);
    u32 header = legacy ? LINK_HISTORY_HEADER_V1 : LINK_HISTORY_HEADER;
    u32 recordSize = legacy ? LINK_HISTORY_RECORD_V1 : LINK_HISTORY_RECORD;
    sLinkHistoryRun = HistoryRead64(data + 16); sLinkHistoryNext = HistoryRead64(data + 24);
    sLinkHistoryCount = HistoryRead32(data + 12);
    if (legacy) HistoryLegacyAnchor(sLinkHistoryRun, HistoryRead64(data + header), sLinkHistoryAnchor);
    else memcpy(sLinkHistoryAnchor, data + LINK_HISTORY_HEADER_V1, 32);
    for (u32 i = 0; i < sLinkHistoryCount; ++i)
    {
        const u8 *p = data + header + i * recordSize;
        struct RhhSavedLinkTrade *entry = &sLinkHistory[i];
        *entry = (struct RhhSavedLinkTrade){.struct_size = sizeof(*entry), .serial = HistoryRead64(p),
            .trade.struct_size = sizeof(entry->trade), .trade.flags = HistoryRead32(p + 8),
            .trade.party_slot = HistoryRead32(p + 12), .trade.transport = HistoryRead32(p + 16),
            .trade.run_id = sLinkHistoryRun};
        HistoryReadMon(p + 24, &entry->trade.sent);
        HistoryReadMon(p + 24 + LINK_HISTORY_SNAPSHOT, &entry->trade.received_before);
        HistoryReadMon(p + 24 + 2 * LINK_HISTORY_SNAPSHOT, &entry->trade.received_after);
        if (legacy) HistoryHash(sLinkHistoryRun, i ? sLinkHistoryHashes[i - 1] : sLinkHistoryAnchor, p, sLinkHistoryHashes[i]);
        else memcpy(sLinkHistoryHashes[i], p + LINK_HISTORY_RECORD_V1, 32);
    }
    return 0;
}
static const struct NativeSaveArchiveCodec sLinkHistoryCodec = {
    LinkHistoryExport, LinkHistoryValidate, LinkHistoryImport, LinkHistoryReset, "rhh:link_trade_history", NULL
};
bool32 ModsLinkTradeHistoryStage(u32 transport, struct RhhLinkTrade *trade)
{
    sLinkHistoryLastValid = FALSE;
    if (!trade || trade->struct_size != sizeof(*trade) || !RhhModsHasActiveRun() || trade->run_id != RhhModsRunId()
        || sLinkHistoryStaged || (sLinkHistoryRun && sLinkHistoryRun != trade->run_id)
        || (transport != RHH_LINK_TRADE_CABLE && transport != RHH_LINK_TRADE_WIRELESS)) return FALSE;
    u64 serial = sLinkHistoryRun ? sLinkHistoryNext : 1;
    if (!serial || serial == UINT64_MAX) return FALSE;
    sLinkHistoryStage = (struct RhhSavedLinkTrade){.struct_size = sizeof(sLinkHistoryStage), .serial = serial, .trade = *trade};
    sLinkHistoryStage.trade.transport = transport; sLinkHistoryStage.trade.sequence = 0;
    u8 record[LINK_HISTORY_RECORD_V1]; HistoryWriteRecord(record, &sLinkHistoryStage);
    HistoryHash(trade->run_id, sLinkHistoryCount ? sLinkHistoryHashes[sLinkHistoryCount - 1] : sLinkHistoryAnchor,
                record, sLinkHistoryStageHash);
    sLinkHistoryStaged = TRUE; return TRUE;
}
void ModsLinkTradeHistoryFinish(bool32 saved)
{
    sLinkHistoryLastValid = FALSE;
    if (!sLinkHistoryStaged) return;
    if (saved)
    {
        if (sLinkHistoryCount == RHH_LINK_TRADE_HISTORY_CAPACITY)
        {
            memcpy(sLinkHistoryAnchor, sLinkHistoryHashes[0], sizeof(sLinkHistoryAnchor));
            memmove(&sLinkHistory[0], &sLinkHistory[1], (sLinkHistoryCount - 1) * sizeof(sLinkHistory[0]));
            memmove(&sLinkHistoryHashes[0], &sLinkHistoryHashes[1], (sLinkHistoryCount - 1) * sizeof(sLinkHistoryHashes[0]));
            --sLinkHistoryCount;
        }
        sLinkHistory[sLinkHistoryCount] = sLinkHistoryStage;
        memcpy(sLinkHistoryHashes[sLinkHistoryCount++], sLinkHistoryStageHash, 32);
        sLinkHistoryRun = sLinkHistoryStage.trade.run_id; sLinkHistoryNext = sLinkHistoryStage.serial + 1;
        sLinkHistoryLast = sLinkHistoryStage; sLinkHistoryLastValid = TRUE;
    }
    sLinkHistoryStaged = FALSE; memset(&sLinkHistoryStage, 0, sizeof(sLinkHistoryStage));
    memset(sLinkHistoryStageHash, 0, sizeof(sLinkHistoryStageHash));
}
static uint32_t LinkHistoryCount(void)
{ return RhhModsHasActiveRun() && sLinkHistoryRun == RhhModsRunId() ? sLinkHistoryCount : 0; }
static int LinkHistoryAt(uint32_t index, struct RhhSavedLinkTrade *out)
{
    if (!out || out->struct_size < sizeof(*out) || index >= LinkHistoryCount()) return -1;
    *out = sLinkHistory[index]; return 0;
}
static int LinkHistoryContext(struct RhhSavedLinkTrade *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sLinkTradeReceiptContext
        || RhhModsCurrentEvent() != RHH_OWNED_LINK_TRADE_SAVED) return -1;
    *out = *sLinkTradeReceiptContext; return 0;
}
static int LinkHistoryCheckpoint(uint32_t index, struct RhhLinkHistoryCheckpoint *out)
{
    if (!out || out->struct_size < sizeof(*out) || index >= LinkHistoryCount()) return -1;
    struct RhhLinkHistoryCheckpoint result = {.struct_size = sizeof(result), .run_id = sLinkHistoryRun,
        .serial = sLinkHistory[index].serial};
    memcpy(result.digest, sLinkHistoryHashes[index], sizeof(result.digest));
    *out = result; return 0;
}
static int LinkHistoryReconcile(const struct RhhLinkHistoryCheckpoint *checkpoint, struct RhhLinkHistoryReconcile *out)
{
    if (!checkpoint || checkpoint->struct_size < sizeof(*checkpoint) || checkpoint->reserved
        || !out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun()) return -1;
    if (!checkpoint->serial)
        for (u32 i = 0; i < sizeof(checkpoint->digest); ++i) if (checkpoint->digest[i]) return -1;
    struct RhhLinkHistoryReconcile result = {.struct_size = sizeof(result), .count = LinkHistoryCount()};
    if (result.count)
    {
        result.oldest_serial = sLinkHistory[0].serial;
        result.newest_serial = sLinkHistory[result.count - 1].serial;
    }
    if (checkpoint->run_id != RhhModsRunId()) result.status = RHH_LINK_HISTORY_WRONG_RUN;
    else if (!result.count)
        result.status = checkpoint->serial ? RHH_LINK_HISTORY_AHEAD : RHH_LINK_HISTORY_UP_TO_DATE;
    else if (checkpoint->serial > result.newest_serial) result.status = RHH_LINK_HISTORY_AHEAD;
    else if (checkpoint->serial < result.oldest_serial)
    {
        if (checkpoint->serial != result.oldest_serial - 1) result.status = RHH_LINK_HISTORY_GAP;
        else if (memcmp(checkpoint->digest, sLinkHistoryAnchor, sizeof(checkpoint->digest)))
            result.status = RHH_LINK_HISTORY_DIVERGED;
        else result.status = RHH_LINK_HISTORY_READY;
    }
    else
    {
        u32 i;
        for (i = 0; i < result.count && sLinkHistory[i].serial < checkpoint->serial; ++i) {}
        if (i == result.count || sLinkHistory[i].serial != checkpoint->serial) result.status = RHH_LINK_HISTORY_GAP;
        else if (memcmp(checkpoint->digest, sLinkHistoryHashes[i], sizeof(checkpoint->digest)))
            result.status = RHH_LINK_HISTORY_DIVERGED;
        else
        {
            result.next_index = i + 1;
            if (result.next_index == result.count) result.status = RHH_LINK_HISTORY_UP_TO_DATE;
            else if (sLinkHistory[result.next_index].serial != checkpoint->serial + 1) result.status = RHH_LINK_HISTORY_GAP;
            else result.status = RHH_LINK_HISTORY_READY;
        }
    }
    *out = result; return 0;
}
static u32 sOwnedWatchLimits[3];
static bool32 sOwnedWatchReady, sOwnedWatchBusy;
void RhhModsResetOwnedMonWatch(void)
{
    NativeOwnedTrackerReset();
    sSkyPartyActive = sSkyPartyPreparing = sSkyPartyValid = sFollowerCatchFull = sFollowerCatchLoaded = FALSE;
    sUnionPartyActive = sUnionPartyValid = sUnionPartyPreparing = FALSE; sUnionPartyCount = sUnionSavedPartyCount = sLinkProjectionKind = 0;
    sRentalPartyActive = sRecordedPartyActive = FALSE;
    sSkyPartyCount = sPartyProjectionKind = 0;
    sOwnedWatchReady = FALSE;
    sOwnedWatchRun = sOwnedWatchSequence = 0;
    sOwnedReleaseRun = sOwnedReleaseSequence = 0;
    sOwnedReleaseContext = NULL;
    sOwnedMoveRun = sOwnedMoveSequence = 0; sOwnedMoveContext = NULL;
    sOwnedPickupRun = sOwnedPickupSequence = 0; sOwnedPickupContext = NULL;
    sOwnedStorageUnreadableRun = sOwnedStorageUnreadableSequence = 0; sOwnedStorageUnreadableContext = NULL;
    sPartyReorderRun = sPartyReorderSequence = 0; sPartyReorderContext = NULL;
    sDaycareTransferRun = sDaycareTransferSequence = 0; sDaycareTransferContext = NULL;
    sFusionTransferRun = sFusionTransferSequence = 0; sFusionTransferContext = NULL;
    sCatchPlacementRun = sCatchPlacementSequence = 0; sCatchPlacementContext = NULL;
    sOwnedGiftRun = sOwnedGiftSequence = sOwnedDebugGiftRun = sOwnedDebugGiftSequence = 0; sOwnedGiftContext = NULL;
    sOwnedMysteryGiftRun = sOwnedMysteryGiftSequence = 0;
    sOwnedEvolutionRun = sOwnedEvolutionSequence = 0; sOwnedEvolutionContext = NULL;
    sIngameTradeRun = sIngameTradeSequence = 0; sIngameTradeContext = NULL;
    sLinkTradeRun = sLinkTradeSequence = 0; sLinkTradeContext = NULL;
    sLinkTradeReceiptContext = NULL; sLinkHistoryLastValid = FALSE;
}
static int OwnedLinkTradeContext(struct RhhLinkTrade *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sLinkTradeContext
        || RhhModsCurrentEvent() != RHH_OWNED_LINK_TRADE_SAVED) return -1;
    *out = *sLinkTradeContext; return 0;
}
bool32 ModsLinkTradeCapture(struct BoxPokemon *sent, struct BoxPokemon *received, u32 slot, struct RhhLinkTrade *out)
{
    if (!sent || !sent->hasSpecies || !received || !received->hasSpecies || !out || slot >= PARTY_SIZE
        || !RhhModsHasActiveRun()
        || gMain.inBattle || !gReceivedRemoteLinkPlayers || NativeSaveTransactionActive()
        || RhhModsCurrentEvent() || sLinkTradeContext) return FALSE;
    struct RhhLinkTrade result = {.struct_size = sizeof(result), .party_slot = slot, .run_id = RhhModsRunId(),
        .sent.struct_size = sizeof(result.sent), .received_before.struct_size = sizeof(result.received_before),
        .received_after.struct_size = sizeof(result.received_after)};
    if (!Snapshot(sent, RHH_MON_PARTY, 0, slot, &result.sent)) result.flags |= RHH_LINK_TRADE_SENT_VALID;
    if (!Snapshot(received, RHH_MON_STAGED, 0, 0, &result.received_before))
        result.flags |= RHH_LINK_TRADE_RECEIVED_BEFORE_VALID;
    *out = result; return TRUE;
}
bool32 ModsLinkTradePlaced(struct BoxPokemon *received, struct RhhLinkTrade *trade)
{
    if (!received || !received->hasSpecies || !trade || trade->struct_size != sizeof(*trade)
        || trade->party_slot >= PARTY_SIZE || trade->run_id != RhhModsRunId()
        || received != &gParties[B_TRAINER_PLAYER][trade->party_slot].box) return FALSE;
    struct RhhOwnedMonSnapshot after = {.struct_size = sizeof(after)};
    bool32 readable = !Snapshot(received, RHH_MON_PARTY, 0, trade->party_slot, &after);
    if ((trade->flags & RHH_LINK_TRADE_RECEIVED_BEFORE_VALID)
        && (!readable || trade->received_before.identity != after.identity
            || trade->received_before.is_egg != after.is_egg
            || memcmp(trade->received_before.ivs, after.ivs, sizeof(after.ivs)))) return FALSE;
    trade->flags &= ~RHH_LINK_TRADE_RECEIVED_AFTER_VALID;
    trade->received_after = after;
    if (readable) trade->flags |= RHH_LINK_TRADE_RECEIVED_AFTER_VALID;
    return TRUE;
}
void ModsLinkTradeSaved(u32 transport, struct RhhLinkTrade *trade)
{
    if (!trade || trade->struct_size != sizeof(*trade) || trade->party_slot >= PARTY_SIZE
        || !RhhModsHasActiveRun() || trade->run_id != RhhModsRunId() || gMain.inBattle
        || !ModsOwnedLinkTradeActive() || NativeSaveTransactionActive() || RhhModsCurrentEvent()
        || sLinkTradeContext || !RhhModsHasHook(RHH_OWNED_LINK_TRADE_SAVED)
        || (transport != RHH_LINK_TRADE_CABLE && transport != RHH_LINK_TRADE_WIRELESS)) return;
    if (sLinkTradeRun != trade->run_id) { sLinkTradeRun = trade->run_id; sLinkTradeSequence = 0; }
    if (sLinkTradeSequence == UINT64_MAX) return;
    trade->transport = transport; trade->sequence = sLinkTradeSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_OWNED_LINK_TRADE_SAVED,
        .run_seed = RhhModsRunSeed(), .ordinal = trade->sequence,
        .event_key = trade->flags & RHH_LINK_TRADE_RECEIVED_AFTER_VALID ? trade->received_after.species : 0,
        .subject = trade->flags & RHH_LINK_TRADE_SENT_VALID ? trade->sent.identity : 0,
        .args = {trade->flags, trade->party_slot, transport, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sLinkTradeContext = trade;
    if (sLinkHistoryLastValid && sLinkHistoryLast.trade.run_id == trade->run_id)
        sLinkTradeReceiptContext = &sLinkHistoryLast;
    RhhModsDispatch(&event, &decision);
    sLinkTradeContext = NULL; sLinkTradeReceiptContext = NULL; sLinkHistoryLastValid = FALSE;
}
static int OwnedIngameTradeContext(struct RhhIngameTrade *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sIngameTradeContext
        || RhhModsCurrentEvent() != RHH_OWNED_INGAME_TRADE_COMPLETED) return -1;
    *out = *sIngameTradeContext; return 0;
}
static bool32 OwnedDestinationMatches(const struct BoxPokemon *mon, u32 location, u32 box, u32 slot);
static bool32 TradeSlotValid(u32 location, u32 box, u32 slot)
{
    return (location == RHH_MON_PARTY && !box && slot < PARTY_SIZE)
        || (location == RHH_MON_BOX && box < TOTAL_BOXES_COUNT && slot < IN_BOX_COUNT);
}
bool32 ModsIngameTradeCapture(struct BoxPokemon *sent, struct BoxPokemon *received, u32 location,
                               u32 box, u32 slot, struct RhhIngameTrade *out)
{
    if (!sent || !sent->hasSpecies || !received || !received->hasSpecies || !out
        || !RhhModsHasActiveRun() || (!RhhModsHasHook(RHH_OWNED_INGAME_TRADE_COMPLETED)
            && !NativeOwnedTrackerActive(RhhModsRunId()))
        || gMain.inBattle || gReceivedRemoteLinkPlayers || !gSaveBlock1Ptr || !gPokemonStoragePtr || NativeSaveTransactionActive()
        || RhhModsCurrentEvent() || sIngameTradeContext || !TradeSlotValid(location, box, slot)
        || (location == RHH_MON_PARTY ? !OwnedDestinationMatches(sent, location, box, slot)
            : !ModsOwnedTradePcHeld() || sent != &gParties[B_TRAINER_OPPONENT_A][TRADEMON_FROM_PC].box)) return FALSE;
    struct RhhIngameTrade result = {.struct_size = sizeof(result), .source_location = location,
        .source_box = box, .source_slot = slot, .run_id = RhhModsRunId(),
        .sent.struct_size = sizeof(result.sent), .received_before.struct_size = sizeof(result.received_before),
        .received_after.struct_size = sizeof(result.received_after)};
    if (!Snapshot(sent, location, box, slot, &result.sent)) result.flags |= RHH_TRADE_SENT_VALID;
    if (!Snapshot(received, RHH_MON_STAGED, 0, 0, &result.received_before)) result.flags |= RHH_TRADE_RECEIVED_BEFORE_VALID;
    *out = result; return TRUE;
}
void ModsIngameTradeCommitted(struct BoxPokemon *received, u32 location, u32 box, u32 slot,
                               struct RhhIngameTrade *trade)
{
    if (!received || !received->hasSpecies || !trade || trade->struct_size != sizeof(*trade)
        || !RhhModsHasActiveRun() || trade->run_id != RhhModsRunId() || gMain.inBattle
        || gReceivedRemoteLinkPlayers || NativeSaveTransactionActive() || RhhModsCurrentEvent()
        || sIngameTradeContext || (!RhhModsHasHook(RHH_OWNED_INGAME_TRADE_COMPLETED)
            && !NativeOwnedTrackerActive(RhhModsRunId()))
        || !TradeSlotValid(location, box, slot) || !OwnedDestinationMatches(received, location, box, slot))
    { NativeOwnedTrackerUnsupported(); return; }
    trade->destination_location = location; trade->destination_box = box; trade->destination_slot = slot;
    if (!Snapshot(received, location, box, slot, &trade->received_after)) trade->flags |= RHH_TRADE_RECEIVED_AFTER_VALID;
    NativeOwnedTrackIngameTrade(trade);
    if (!RhhModsHasHook(RHH_OWNED_INGAME_TRADE_COMPLETED)) return;
    if (sIngameTradeRun != trade->run_id) { sIngameTradeRun = trade->run_id; sIngameTradeSequence = 0; }
    if (sIngameTradeSequence == UINT64_MAX) return;
    trade->sequence = sIngameTradeSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_OWNED_INGAME_TRADE_COMPLETED,
        .run_seed = RhhModsRunSeed(), .ordinal = trade->sequence,
        .event_key = trade->flags & RHH_TRADE_RECEIVED_AFTER_VALID ? trade->received_after.species : 0,
        .subject = trade->flags & RHH_TRADE_SENT_VALID ? trade->sent.identity : 0,
        .args = {trade->flags, trade->source_location, trade->destination_location, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sIngameTradeContext = trade; RhhModsDispatch(&event, &decision); sIngameTradeContext = NULL;
}
static int OwnedCatchPlacementContext(struct RhhCatchPlacement *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sCatchPlacementContext
        || RhhModsCurrentEvent() != RHH_OWNED_CATCH_PLACED) return -1;
    *out = *sCatchPlacementContext; return 0;
}
static int OwnedGiftContext(struct RhhOwnedGift *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sOwnedGiftContext
        || (RhhModsCurrentEvent() != RHH_OWNED_GIFT_RECEIVED && RhhModsCurrentEvent() != RHH_OWNED_DEBUG_CREATED
            && RhhModsCurrentEvent() != RHH_OWNED_MYSTERY_GIFT_RECEIVED)) return -1;
    *out = *sOwnedGiftContext; return 0;
}
static int OwnedEvolutionContext(struct RhhOwnedEvolution *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sOwnedEvolutionContext
        || RhhModsCurrentEvent() != RHH_OWNED_EVOLUTION_COMMITTED) return -1;
    *out = *sOwnedEvolutionContext; return 0;
}
static bool32 OwnedEvolutionAvailable(void)
{
    return RhhModsHasActiveRun()
        && (RhhModsHasHook(RHH_OWNED_EVOLUTION_COMMITTED) || NativeOwnedTrackerActive(RhhModsRunId())
            || ModsOwnedLinkTradeActive())
        && !RhhModsCurrentEvent() && !sOwnedEvolutionContext && !NativeSaveTransactionActive()
        && !ModsOwnedStorageBusy() && !ModsOwnedBattlePartyViewActive() && !ModsOwnedProjectedPartyActive() && gSaveBlock1Ptr && gPokemonStoragePtr
        && (!gMain.inBattle || !(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK
             | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TOWER_LINK_MULTI | BATTLE_TYPE_TRAINER_HILL
             | BATTLE_TYPE_EREADER_TRAINER | BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_CATCH_TUTORIAL)));
}
bool32 ModsOwnedEvolutionCapture(struct Pokemon *mon, u32 partySlot, bool32 trade, struct RhhOwnedEvolution *out)
{
    if (!mon || !mon->box.hasSpecies || !out || !OwnedEvolutionAvailable()
        || (!trade && (ModsOwnedLinkTradeActive() || ModsOwnedTradePcHeld()))) return FALSE;
    u32 location = RHH_MON_PARTY, slot = partySlot, box = 0, destination = RHH_MON_PARTY, destinationSlot = partySlot;
    u32 flags = trade ? RHH_EVOLUTION_TRADE : 0;
    if (trade && partySlot == PC_MON_CHOSEN)
    {
        if (mon != &gParties[B_TRAINER_OPPONENT_A][TRADEMON_FROM_PC]
            || !ModsOwnedTradePcDestination(&box, &destinationSlot)) return FALSE;
        location = RHH_MON_STAGED; slot = TRADEMON_FROM_PC; destination = RHH_MON_BOX;
        flags |= RHH_EVOLUTION_PC_PENDING;
    }
    else if (partySlot >= PARTY_SIZE || mon != &gParties[B_TRAINER_PLAYER][partySlot]) return FALSE;
    if (trade && ModsOwnedLinkTradeActive()) flags |= RHH_EVOLUTION_LINK_TRADE;
    struct RhhOwnedEvolution result = {.struct_size = sizeof(result), .cause = RHH_EVOLUTION_CHANGED, .flags = flags,
        .source_location = location, .source_slot = slot, .destination_location = destination,
        .destination_box = box, .destination_slot = destinationSlot, .run_id = RhhModsRunId(),
        .before.struct_size = sizeof(result.before), .after.struct_size = sizeof(result.after),
        .replaced.struct_size = sizeof(result.replaced)};
    if (!Snapshot(&mon->box, location, 0, slot, &result.before)) result.flags |= RHH_EVOLUTION_BEFORE_VALID;
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
        location == RHH_MON_STAGED ? NATIVE_OWNED_TRADE : NATIVE_OWNED_PARTY, 0,
        location == RHH_MON_STAGED ? 0 : slot}, &mon->box);
    *out = result; return TRUE;
}
bool32 ModsOwnedEvolutionSplitCapture(struct Pokemon *parent, u32 parentSlot, u32 destinationSlot,
                                      struct RhhOwnedEvolution *out)
{
    if (!parent || !parent->box.hasSpecies || !out || !OwnedEvolutionAvailable() || ModsOwnedLinkTradeActive()
        || ModsOwnedTradePcHeld() || parentSlot >= PARTY_SIZE || destinationSlot >= PARTY_SIZE
        || parent != &gParties[B_TRAINER_PLAYER][parentSlot]) return FALSE;
    struct RhhOwnedEvolution result = {.struct_size = sizeof(result), .cause = RHH_EVOLUTION_SPLIT,
        .source_location = RHH_MON_PARTY, .source_slot = parentSlot, .destination_location = RHH_MON_PARTY,
        .destination_slot = destinationSlot, .run_id = RhhModsRunId(),
        .before.struct_size = sizeof(result.before), .after.struct_size = sizeof(result.after),
        .replaced.struct_size = sizeof(result.replaced)};
    if (!Snapshot(&parent->box, RHH_MON_PARTY, 0, parentSlot, &result.before)) result.flags |= RHH_EVOLUTION_BEFORE_VALID;
    struct BoxPokemon *replaced = &gParties[B_TRAINER_PLAYER][destinationSlot].box;
    if (replaced->hasSpecies)
    {
        result.flags |= RHH_EVOLUTION_REPLACED;
        if (!Snapshot(replaced, RHH_MON_PARTY, 0, destinationSlot, &result.replaced)) result.flags |= RHH_EVOLUTION_REPLACED_VALID;
    }
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, parentSlot}, &parent->box);
    if (replaced->hasSpecies)
        NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, destinationSlot}, replaced);
    *out = result; return TRUE;
}
void ModsOwnedEvolutionCommitted(struct Pokemon *mon, struct RhhOwnedEvolution *evolution)
{
    if (!mon || !evolution || evolution->struct_size != sizeof(*evolution) || !OwnedEvolutionAvailable()
        || evolution->run_id != RhhModsRunId()
        || (evolution->flags & RHH_EVOLUTION_PC_PENDING
            ? mon != &gParties[B_TRAINER_OPPONENT_A][TRADEMON_FROM_PC]
            : !OwnedDestinationMatches(&mon->box, evolution->destination_location,
                                       evolution->destination_box, evolution->destination_slot)))
    { NativeOwnedTrackerUnsupported(); return; }
    u32 location = evolution->flags & RHH_EVOLUTION_PC_PENDING ? RHH_MON_STAGED : evolution->destination_location;
    u32 box = location == RHH_MON_STAGED ? 0 : evolution->destination_box;
    u32 slot = location == RHH_MON_STAGED ? evolution->source_slot : evolution->destination_slot;
    if (!Snapshot(&mon->box, location, box, slot, &evolution->after)) evolution->flags |= RHH_EVOLUTION_AFTER_VALID;
    if (evolution->cause == RHH_EVOLUTION_CHANGED)
        NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
            location == RHH_MON_STAGED ? NATIVE_OWNED_TRADE : NATIVE_OWNED_PARTY, 0,
            location == RHH_MON_STAGED ? 0 : evolution->source_slot}, &mon->box);
    else if (evolution->cause == RHH_EVOLUTION_SPLIT && evolution->source_slot < PARTY_SIZE)
        NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, evolution->source_slot},
                                     &gParties[B_TRAINER_PLAYER][evolution->source_slot].box);
    if (!mon->box.hasSpecies) NativeOwnedTrackerUnsupported();
    else NativeOwnedTrackEvolution(evolution);
    if (evolution->flags & RHH_EVOLUTION_LINK_TRADE) ModsLinkTradeEvolutionCommitted(evolution);
    if (!RhhModsHasHook(RHH_OWNED_EVOLUTION_COMMITTED)) return;
    if (sOwnedEvolutionRun != evolution->run_id) { sOwnedEvolutionRun = evolution->run_id; sOwnedEvolutionSequence = 0; }
    if (sOwnedEvolutionSequence == UINT64_MAX) return;
    evolution->sequence = sOwnedEvolutionSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_OWNED_EVOLUTION_COMMITTED,
        .run_seed = RhhModsRunSeed(), .ordinal = evolution->sequence, .subject = evolution->before.identity,
        .event_key = evolution->after.species, .args = {evolution->cause, evolution->flags, evolution->destination_location, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sOwnedEvolutionContext = evolution; RhhModsDispatch(&event, &decision); sOwnedEvolutionContext = NULL;
}
static u32 OwnedGiftEvent(u32 cause)
{
    if (cause == RHH_GIFT_DEBUG) return RHH_OWNED_DEBUG_CREATED;
    if (cause == RHH_GIFT_MYSTERY_EVENT) return RHH_OWNED_MYSTERY_GIFT_RECEIVED;
    return RHH_OWNED_GIFT_RECEIVED;
}
static bool32 OwnedDestinationMatches(const struct BoxPokemon *slot, u32 location, u32 box, u32 position)
{
    if (!slot) return FALSE;
    if (location == RHH_MON_PARTY && !box && position < PARTY_SIZE)
        return slot == &gParties[B_TRAINER_PLAYER][position].box;
    if (location == RHH_MON_BOX && gPokemonStoragePtr && box < TOTAL_BOXES_COUNT && position < IN_BOX_COUNT)
        return slot == &gPokemonStoragePtr->boxes[box][position];
    if (location == RHH_MON_DAYCARE && gSaveBlock1Ptr && position < OwnedDaycareSlots(box))
    {
#if IS_FRLG
        if (box == 1) return slot == &gSaveBlock1Ptr->route5DayCareMon.mon;
#endif
        return slot == &gSaveBlock1Ptr->daycare.mons[position].mon;
    }
    if (location == RHH_MON_FUSION_STORAGE && gPokemonStoragePtr && !box && position < MAX_FUSION_STORAGE)
        return slot == &gPokemonStoragePtr->fusions[position].box;
    return FALSE;
}
static bool32 OwnedGiftDestinationMatches(const struct BoxPokemon *slot, u32 location, u32 box, u32 position)
{
    return (location == RHH_MON_PARTY || location == RHH_MON_BOX)
        && OwnedDestinationMatches(slot, location, box, position);
}
bool32 ModsOwnedGiftCapture(struct BoxPokemon *mon, u32 cause, struct RhhOwnedGift *out)
{
    u32 event = OwnedGiftEvent(cause);
    if (!mon || !mon->hasSpecies || !out || (cause != RHH_GIFT_SCRIPTED && cause != RHH_GIFT_OWNER_ASSIGNED
        && cause != RHH_GIFT_DEBUG && cause != RHH_GIFT_MYSTERY_EVENT)
        || !RhhModsHasActiveRun() || (!RhhModsHasHook(event) && !NativeOwnedTrackerActive(RhhModsRunId()))
        || RhhModsCurrentEvent()
        || gMain.inBattle || !OwnedStorageAvailable() || sOwnedGiftContext) return FALSE;
    struct RhhOwnedGift gift = {.struct_size = sizeof(gift), .cause = cause, .run_id = RhhModsRunId(),
        .before.struct_size = sizeof(gift.before), .after.struct_size = sizeof(gift.after),
        .replaced.struct_size = sizeof(gift.replaced)};
    if (!Snapshot(mon, RHH_MON_STAGED, 0, 0, &gift.before)) gift.flags |= RHH_GIFT_BEFORE_VALID;
    *out = gift; return TRUE;
}
void ModsOwnedGiftDestination(struct BoxPokemon *slot, u32 location, u32 box, u32 position, struct RhhOwnedGift *gift)
{
    if (!gift || gift->struct_size != sizeof(*gift) || !OwnedGiftDestinationMatches(slot, location, box, position))
    { NativeOwnedTrackerUnsupported(); return; }
    gift->destination_location = location; gift->destination_box = box; gift->destination_slot = position;
    if (slot->hasSpecies)
    {
        gift->flags |= RHH_GIFT_REPLACED;
        if (!Snapshot(slot, location, box, position, &gift->replaced)) gift->flags |= RHH_GIFT_REPLACED_VALID;
        NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
            location == RHH_MON_PARTY ? NATIVE_OWNED_PARTY : NATIVE_OWNED_BOX, box, position}, slot);
    }
}
void ModsOwnedGiftCommitted(struct BoxPokemon *slot, struct RhhOwnedGift *gift)
{
    u32 eventKind = OwnedGiftEvent(gift ? gift->cause : 0);
    if (!gift || gift->struct_size != sizeof(*gift) || !slot || !RhhModsHasActiveRun() || gift->run_id != RhhModsRunId()
        || gMain.inBattle || !OwnedStorageAvailable() || RhhModsCurrentEvent() || sOwnedGiftContext
        || (!RhhModsHasHook(eventKind) && !NativeOwnedTrackerActive(RhhModsRunId()))
        || !OwnedGiftDestinationMatches(slot, gift->destination_location, gift->destination_box, gift->destination_slot))
    { NativeOwnedTrackerUnsupported(); return; }
    if (!Snapshot(slot, gift->destination_location, gift->destination_box, gift->destination_slot, &gift->after))
        gift->flags |= RHH_GIFT_AFTER_VALID;
    if (!slot->hasSpecies) NativeOwnedTrackerUnsupported();
    else NativeOwnedTrackGift(gift);
    if (!RhhModsHasHook(eventKind)) return;
    u64 *run = eventKind == RHH_OWNED_DEBUG_CREATED ? &sOwnedDebugGiftRun
        : eventKind == RHH_OWNED_MYSTERY_GIFT_RECEIVED ? &sOwnedMysteryGiftRun : &sOwnedGiftRun;
    u64 *sequence = eventKind == RHH_OWNED_DEBUG_CREATED ? &sOwnedDebugGiftSequence
        : eventKind == RHH_OWNED_MYSTERY_GIFT_RECEIVED ? &sOwnedMysteryGiftSequence : &sOwnedGiftSequence;
    if (*run != gift->run_id) { *run = gift->run_id; *sequence = 0; }
    if (*sequence == UINT64_MAX) return;
    gift->sequence = (*sequence)++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = eventKind,
        .run_seed = RhhModsRunSeed(), .ordinal = gift->sequence, .event_key = gift->after.species,
        .subject = gift->before.identity, .args = {gift->cause, gift->flags, gift->destination_location, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sOwnedGiftContext = gift; RhhModsDispatch(&event, &decision); sOwnedGiftContext = NULL;
}
static bool32 OwnedCatchAvailable(void)
{
    return gMain.inBattle && !ModsOwnedBattlePartyViewActive() && !ModsOwnedSkyPartyActive() && !ModsOwnedRentalPartyActive()
        && gSaveBlock1Ptr && gPokemonStoragePtr
        && !(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK
             | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TOWER_LINK_MULTI | BATTLE_TYPE_TRAINER_HILL
             | BATTLE_TYPE_EREADER_TRAINER | BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_CATCH_TUTORIAL
             | BATTLE_TYPE_TRAINER))
        && !NativeSaveTransactionActive() && !RhhModsCurrentEvent() && !sCatchPlacementContext;
}
bool32 ModsCatchPlacementCapture(struct BoxPokemon *source, u32 cause, u32 sourceSlot,
                                  struct RhhCatchPlacement *out)
{
    ModsWildActorCatchCapture(NULL, 0, NULL);
    if (!source || !source->hasSpecies || !out || !RhhModsHasActiveRun()
        || (!RhhModsHasHook(RHH_OWNED_CATCH_PLACED) && !RhhModsHasHook(RHH_WILD_ACTOR_BATTLE_ENDED) && !ModsWildActorBattleTracking()
            && !NativeOwnedTrackerActive(RhhModsRunId())) || !OwnedCatchAvailable()
        || (cause == RHH_CATCH_DISPLACED_PARTY && sourceSlot >= PARTY_SIZE)
        || (cause == RHH_CATCH_ACQUIRED && sourceSlot >= MAX_BATTLERS_COUNT)
        || (cause != RHH_CATCH_DISPLACED_PARTY && cause != RHH_CATCH_ACQUIRED)) return FALSE;
    u32 location = cause == RHH_CATCH_DISPLACED_PARTY ? RHH_MON_PARTY : RHH_MON_STAGED;
    struct RhhCatchPlacement result = {.struct_size = sizeof(result), .cause = cause,
        .source_location = location, .source_slot = sourceSlot, .run_id = RhhModsRunId(),
        .before.struct_size = sizeof(result.before), .after.struct_size = sizeof(result.after)};
    if (!Snapshot(source, location, 0, sourceSlot, &result.before)) result.flags |= RHH_CATCH_BEFORE_VALID;
    if (cause == RHH_CATCH_ACQUIRED) ModsWildActorCatchCapture(source, sourceSlot, &result);
    if (cause == RHH_CATCH_DISPLACED_PARTY)
        NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, sourceSlot}, source);
    *out = result; return TRUE;
}
void ModsCatchPlacementCommitted(struct BoxPokemon *destination, u32 location, u32 box, u32 slot,
                                  struct RhhCatchPlacement *placement)
{
    if (!destination || !destination->hasSpecies || !placement || placement->struct_size != sizeof(*placement)
        || !RhhModsHasActiveRun() || placement->run_id != RhhModsRunId() || !OwnedCatchAvailable()
        || (!RhhModsHasHook(RHH_OWNED_CATCH_PLACED) && !RhhModsHasHook(RHH_WILD_ACTOR_BATTLE_ENDED) && !ModsWildActorBattleTracking()
            && !NativeOwnedTrackerActive(RhhModsRunId()))
        || !OwnedGiftDestinationMatches(destination, location, box, slot))
    { NativeOwnedTrackerUnsupported(); return; }
    placement->destination_location = location; placement->destination_box = box; placement->destination_slot = slot;
    if (!Snapshot(destination, location, box, slot, &placement->after)) placement->flags |= RHH_CATCH_AFTER_VALID;
    if (placement->cause == RHH_CATCH_DISPLACED_PARTY)
        NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, placement->source_slot}, destination);
    NativeOwnedTrackCatch(placement);
    ModsWildActorCatchCommitted(placement, destination);
    if (!RhhModsHasHook(RHH_OWNED_CATCH_PLACED)) return;
    if (sCatchPlacementRun != placement->run_id) { sCatchPlacementRun = placement->run_id; sCatchPlacementSequence = 0; }
    if (sCatchPlacementSequence == UINT64_MAX) return;
    placement->sequence = sCatchPlacementSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_OWNED_CATCH_PLACED,
        .run_seed = RhhModsRunSeed(), .ordinal = placement->sequence,
        .event_key = placement->flags & RHH_CATCH_AFTER_VALID ? placement->after.species : 0,
        .subject = placement->flags & RHH_CATCH_BEFORE_VALID ? placement->before.identity : 0,
        .args = {placement->cause, placement->source_location, placement->destination_location, placement->flags}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sCatchPlacementContext = placement; RhhModsDispatch(&event, &decision); sCatchPlacementContext = NULL;
}
static int OwnedFusionTransferContext(struct RhhFusionTransfer *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sFusionTransferContext
        || RhhModsCurrentEvent() != RHH_OWNED_FUSION_TRANSFERRED) return -1;
    *out = *sFusionTransferContext; return 0;
}
bool32 ModsFusionTransferCapture(struct BoxPokemon *source, u32 cause, u32 sourceLocation, u32 sourceSlot,
                                 u32 primarySlot, struct RhhFusionTransfer *out)
{
    if (!source || !source->hasSpecies || !out || !RhhModsHasActiveRun()
        || (!RhhModsHasHook(RHH_OWNED_FUSION_TRANSFERRED) && !NativeOwnedTrackerActive(RhhModsRunId())) || gMain.inBattle
        || NativeSaveTransactionActive() || RhhModsCurrentEvent() || sFusionTransferContext
        || primarySlot >= PARTY_SIZE
        || (cause == RHH_FUSION_STORE && (sourceLocation != RHH_MON_PARTY || sourceSlot >= PARTY_SIZE || sourceSlot == primarySlot))
        || (cause == RHH_FUSION_RESTORE && (sourceLocation != RHH_MON_FUSION_STORAGE || sourceSlot >= MAX_FUSION_STORAGE))
        || (cause != RHH_FUSION_STORE && cause != RHH_FUSION_RESTORE)) return FALSE;
    struct RhhFusionTransfer result = {.struct_size = sizeof(result), .cause = cause, .primary_slot = primarySlot,
        .source_location = sourceLocation, .source_slot = sourceSlot, .run_id = RhhModsRunId(),
        .before.struct_size = sizeof(result.before), .after.struct_size = sizeof(result.after),
        .primary_before.struct_size = sizeof(result.primary_before), .primary_after.struct_size = sizeof(result.primary_after)};
    if (!Snapshot(source, sourceLocation, 0, sourceSlot, &result.before)) result.flags |= RHH_FUSION_BEFORE_VALID;
    if (!Snapshot(&gParties[B_TRAINER_PLAYER][primarySlot].box, RHH_MON_PARTY, 0, primarySlot, &result.primary_before))
        result.flags |= RHH_FUSION_PRIMARY_BEFORE_VALID;
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
        sourceLocation == RHH_MON_PARTY ? NATIVE_OWNED_PARTY : NATIVE_OWNED_FUSION, 0, sourceSlot}, source);
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, primarySlot},
                                 &gParties[B_TRAINER_PLAYER][primarySlot].box);
    *out = result; return TRUE;
}
void ModsFusionTransferCommitted(struct BoxPokemon *destination, u32 location, u32 box, u32 slot,
                                 struct RhhFusionTransfer *transfer)
{
    if (!destination || !destination->hasSpecies || !transfer || transfer->struct_size != sizeof(*transfer)
        || !RhhModsHasActiveRun() || transfer->run_id != RhhModsRunId() || gMain.inBattle
        || NativeSaveTransactionActive() || RhhModsCurrentEvent() || sFusionTransferContext
        || (!RhhModsHasHook(RHH_OWNED_FUSION_TRANSFERRED) && !NativeOwnedTrackerActive(RhhModsRunId()))
        || transfer->primary_slot >= PARTY_SIZE
        || !((location == RHH_MON_FUSION_STORAGE && !box && slot < MAX_FUSION_STORAGE)
            || (location == RHH_MON_PARTY && !box && slot < PARTY_SIZE)
            || (location == RHH_MON_BOX && box < TOTAL_BOXES_COUNT && slot < IN_BOX_COUNT))
        || !OwnedDestinationMatches(destination, location, box, slot))
    { NativeOwnedTrackerUnsupported(); return; }
    transfer->destination_location = location; transfer->destination_box = box; transfer->destination_slot = slot;
    if (!Snapshot(destination, location, box, slot, &transfer->after)) transfer->flags |= RHH_FUSION_AFTER_VALID;
    if (!Snapshot(&gParties[B_TRAINER_PLAYER][transfer->primary_slot].box, RHH_MON_PARTY, 0,
                  transfer->primary_slot, &transfer->primary_after)) transfer->flags |= RHH_FUSION_PRIMARY_AFTER_VALID;
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
        transfer->source_location == RHH_MON_PARTY ? NATIVE_OWNED_PARTY : NATIVE_OWNED_FUSION,
        0, transfer->source_slot}, destination);
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, transfer->primary_slot},
                                 &gParties[B_TRAINER_PLAYER][transfer->primary_slot].box);
    NativeOwnedTrackFusion(transfer);
    if (!RhhModsHasHook(RHH_OWNED_FUSION_TRANSFERRED)) return;
    if (sFusionTransferRun != transfer->run_id) { sFusionTransferRun = transfer->run_id; sFusionTransferSequence = 0; }
    if (sFusionTransferSequence == UINT64_MAX) return;
    transfer->sequence = sFusionTransferSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_OWNED_FUSION_TRANSFERRED,
        .run_seed = RhhModsRunSeed(), .ordinal = transfer->sequence,
        .event_key = transfer->flags & RHH_FUSION_AFTER_VALID ? transfer->after.species : 0,
        .subject = transfer->flags & RHH_FUSION_BEFORE_VALID ? transfer->before.identity : 0,
        .args = {transfer->cause, transfer->source_location, transfer->destination_location, transfer->flags}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sFusionTransferContext = transfer; RhhModsDispatch(&event, &decision); sFusionTransferContext = NULL;
}
static int OwnedDaycareTransferContext(struct RhhDaycareTransfer *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sDaycareTransferContext
        || RhhModsCurrentEvent() != RHH_OWNED_DAYCARE_TRANSFERRED) return -1;
    *out = *sDaycareTransferContext; return 0;
}
static bool32 DaycareTransferSlotValid(u32 location, u32 box, u32 slot)
{
    return (location == RHH_MON_PARTY && !box && slot < PARTY_SIZE)
        || (location == RHH_MON_BOX && box < TOTAL_BOXES_COUNT && slot < IN_BOX_COUNT)
        || (location == RHH_MON_DAYCARE && slot < OwnedDaycareSlots(box));
}
bool32 ModsDaycareTransferCapture(struct BoxPokemon *source, u32 cause, u32 sourceLocation, u32 sourceBox,
                                 u32 sourceSlot, u32 destinationLocation, u32 destinationBox,
                                 u32 destinationSlot, struct RhhDaycareTransfer *out)
{
    if (!source || !source->hasSpecies || !out || !RhhModsHasActiveRun()
        || (!RhhModsHasHook(RHH_OWNED_DAYCARE_TRANSFERRED) && !NativeOwnedTrackerActive(RhhModsRunId())) || gMain.inBattle
        || NativeSaveTransactionActive() || RhhModsCurrentEvent() || sDaycareTransferContext
        || !DaycareTransferSlotValid(sourceLocation, sourceBox, sourceSlot)
        || !DaycareTransferSlotValid(destinationLocation, destinationBox, destinationSlot)
        || (cause != RHH_DAYCARE_DEPOSIT && cause != RHH_DAYCARE_WITHDRAW && cause != RHH_DAYCARE_SHIFT))
        return FALSE;
    struct RhhDaycareTransfer result = {.struct_size = sizeof(result), .cause = cause,
        .source_location = sourceLocation, .source_box = sourceBox, .source_slot = sourceSlot,
        .destination_location = destinationLocation, .destination_box = destinationBox, .destination_slot = destinationSlot,
        .run_id = RhhModsRunId(), .before.struct_size = sizeof(result.before), .after.struct_size = sizeof(result.after)};
    if (!Snapshot(source, sourceLocation, sourceBox, sourceSlot, &result.before))
        result.flags |= RHH_DAYCARE_TRANSFER_BEFORE_VALID;
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
        sourceLocation == RHH_MON_PARTY ? NATIVE_OWNED_PARTY :
        sourceLocation == RHH_MON_BOX ? NATIVE_OWNED_BOX : NATIVE_OWNED_DAYCARE, sourceBox, sourceSlot}, source);
    *out = result; return TRUE;
}
void ModsDaycareTransferCommitted(struct BoxPokemon *destination, struct RhhDaycareTransfer *transfer)
{
    if (!destination || !destination->hasSpecies || !transfer || transfer->struct_size != sizeof(*transfer)
        || !RhhModsHasActiveRun() || transfer->run_id != RhhModsRunId() || gMain.inBattle
        || NativeSaveTransactionActive() || RhhModsCurrentEvent() || sDaycareTransferContext
        || (!RhhModsHasHook(RHH_OWNED_DAYCARE_TRANSFERRED) && !NativeOwnedTrackerActive(RhhModsRunId()))
        || !OwnedDestinationMatches(destination, transfer->destination_location,
                                    transfer->destination_box, transfer->destination_slot))
    { NativeOwnedTrackerUnsupported(); return; }
    if (!Snapshot(destination, transfer->destination_location, transfer->destination_box,
                  transfer->destination_slot, &transfer->after)) transfer->flags |= RHH_DAYCARE_TRANSFER_AFTER_VALID;
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
        transfer->source_location == RHH_MON_PARTY ? NATIVE_OWNED_PARTY :
        transfer->source_location == RHH_MON_BOX ? NATIVE_OWNED_BOX : NATIVE_OWNED_DAYCARE,
        transfer->source_box, transfer->source_slot}, destination);
    NativeOwnedTrackDaycare(transfer);
    if (!RhhModsHasHook(RHH_OWNED_DAYCARE_TRANSFERRED)) return;
    if (sDaycareTransferRun != transfer->run_id) { sDaycareTransferRun = transfer->run_id; sDaycareTransferSequence = 0; }
    if (sDaycareTransferSequence == UINT64_MAX) return;
    transfer->sequence = sDaycareTransferSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_OWNED_DAYCARE_TRANSFERRED,
        .run_seed = RhhModsRunSeed(), .ordinal = transfer->sequence,
        .event_key = transfer->flags & RHH_DAYCARE_TRANSFER_AFTER_VALID ? transfer->after.species : 0,
        .subject = transfer->flags & RHH_DAYCARE_TRANSFER_BEFORE_VALID ? transfer->before.identity : 0,
        .args = {transfer->cause, transfer->source_location, transfer->destination_location, transfer->flags}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sDaycareTransferContext = transfer; RhhModsDispatch(&event, &decision); sDaycareTransferContext = NULL;
}
static int OwnedPartyReorderContext(struct RhhPartyReorder *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sPartyReorderContext
        || RhhModsCurrentEvent() != RHH_PARTY_REORDERED) return -1;
    *out = *sPartyReorderContext; return 0;
}
bool32 ModsPartyReorderBegin(u32 cause, struct RhhPartyReorder *out)
{
    if (!out || (cause != RHH_PARTY_REORDER_SWAP && cause != RHH_PARTY_REORDER_COMPACT)
        || !RhhModsHasActiveRun() || (!RhhModsHasHook(RHH_PARTY_REORDERED) && !NativeOwnedTrackerActive(RhhModsRunId()))
        || gMain.inBattle || ModsOwnedProjectedPartyActive() || NativeSaveTransactionActive() || RhhModsCurrentEvent() || sPartyReorderContext)
        return FALSE;
    *out = (struct RhhPartyReorder){.struct_size = sizeof(*out), .cause = cause, .run_id = RhhModsRunId()};
    return TRUE;
}
void ModsPartyReorderAdd(struct RhhPartyReorder *batch, u32 source, u32 destination)
{
    if (!batch || batch->struct_size != sizeof(*batch)) return;
    if (source >= PARTY_SIZE || destination >= PARTY_SIZE || source == destination
        || batch->count >= RHH_PARTY_REORDER_CAPACITY) { batch->reserved = 1; return; }
    for (u32 i = 0; i < batch->count; ++i)
        if (batch->moves[i].source_slot == source || batch->moves[i].destination_slot == destination)
        { batch->reserved = 1; return; }
    struct RhhPartyRelocation *move = &batch->moves[batch->count++];
    *move = (struct RhhPartyRelocation){.source_slot = source, .destination_slot = destination,
        .before.struct_size = sizeof(move->before), .after.struct_size = sizeof(move->after)};
    struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][source].box;
    if (mon->hasSpecies)
    {
        move->flags |= RHH_PARTY_BEFORE_OCCUPIED;
        if (!Snapshot(mon, RHH_MON_PARTY, 0, source, &move->before)) move->flags |= RHH_PARTY_BEFORE_VALID;
        NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, source}, mon);
    }
}
void ModsPartyReorderCommitted(struct RhhPartyReorder *batch)
{
    if (batch && batch->struct_size == sizeof(*batch) && !batch->count && !batch->reserved) return;
    if (!batch || batch->struct_size != sizeof(*batch) || batch->reserved || !batch->count
        || batch->count > RHH_PARTY_REORDER_CAPACITY || !RhhModsHasActiveRun()
        || batch->run_id != RhhModsRunId() || gMain.inBattle || NativeSaveTransactionActive()
        || RhhModsCurrentEvent() || sPartyReorderContext
        || (!RhhModsHasHook(RHH_PARTY_REORDERED) && !NativeOwnedTrackerActive(RhhModsRunId())))
    { NativeOwnedTrackerUnsupported(); return; }
    for (u32 i = 0; i < batch->count; ++i)
    {
        struct RhhPartyRelocation *move = &batch->moves[i];
        struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][move->destination_slot].box;
        if (mon->hasSpecies)
        {
            move->flags |= RHH_PARTY_AFTER_OCCUPIED;
            if (!Snapshot(mon, RHH_MON_PARTY, 0, move->destination_slot, &move->after)) move->flags |= RHH_PARTY_AFTER_VALID;
            NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, move->source_slot}, mon);
        }
    }
    NativeOwnedTrackPartyReorder(batch);
    if (!RhhModsHasHook(RHH_PARTY_REORDERED)) return;
    if (sPartyReorderRun != batch->run_id) { sPartyReorderRun = batch->run_id; sPartyReorderSequence = 0; }
    if (sPartyReorderSequence == UINT64_MAX) return;
    batch->sequence = sPartyReorderSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_PARTY_REORDERED,
        .run_seed = RhhModsRunSeed(), .ordinal = batch->sequence, .args = {batch->cause, batch->count, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sPartyReorderContext = batch; RhhModsDispatch(&event, &decision); sPartyReorderContext = NULL;
}
static int OwnedMoveContext(struct RhhOwnedMove *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sOwnedMoveContext
        || RhhModsCurrentEvent() != RHH_OWNED_MON_MOVED) return -1;
    *out = *sOwnedMoveContext; return 0;
}
static bool32 OwnedPcSlotValid(u32 location, u32 box, u32 slot)
{
    return (location == RHH_MON_PARTY && !box && slot < PARTY_SIZE)
        || (location == RHH_MON_BOX && box < TOTAL_BOXES_COUNT && slot < IN_BOX_COUNT);
}
static int OwnedStorageUnreadableContext(struct RhhOwnedStorageUnreadable *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sOwnedStorageUnreadableContext
        || RhhModsCurrentEvent() != RHH_OWNED_STORAGE_UNREADABLE) return -1;
    *out = *sOwnedStorageUnreadableContext; return 0;
}
static void OwnedStorageUnreadable(struct RhhOwnedStorageUnreadable *context)
{
    if (!context || !RhhModsHasActiveRun() || context->run_id != RhhModsRunId()
        || sOwnedStorageUnreadableContext || RhhModsCurrentEvent() || !RhhModsHasHook(RHH_OWNED_STORAGE_UNREADABLE)) return;
    if (sOwnedStorageUnreadableRun != context->run_id)
    { sOwnedStorageUnreadableRun = context->run_id; sOwnedStorageUnreadableSequence = 0; }
    if (sOwnedStorageUnreadableSequence == UINT64_MAX) return;
    context->sequence = sOwnedStorageUnreadableSequence++;
    u32 flags = context->cause == RHH_OWNED_STORAGE_UNREADABLE_MOVE ? context->move.flags : context->release.flags;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_OWNED_STORAGE_UNREADABLE,
        .run_seed = RhhModsRunSeed(), .ordinal = context->sequence, .args = {context->cause, flags, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sOwnedStorageUnreadableContext = context; RhhModsDispatch(&event, &decision); sOwnedStorageUnreadableContext = NULL;
}
static int OwnedPickupContext(struct RhhOwnedPickup *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sOwnedPickupContext
        || RhhModsCurrentEvent() != RHH_OWNED_MON_PICKED_UP) return -1;
    *out = *sOwnedPickupContext; return 0;
}
bool32 ModsOwnedPickupCapture(struct BoxPokemon *mon, u32 location, u32 box, u32 slot, bool32 group,
                              struct RhhOwnedPickup *out)
{
    if (!out || !mon || !mon->hasSpecies || !RhhModsHasActiveRun()
        || (!RhhModsHasHook(RHH_OWNED_MON_PICKED_UP) && !NativeOwnedTrackerActive(RhhModsRunId()))
        || RhhModsCurrentEvent() || gMain.inBattle || NativeSaveTransactionActive()
        || !OwnedPcSlotValid(location, box, slot)) return FALSE;
    struct RhhOwnedPickup result = {.struct_size = sizeof(result), .flags = group ? RHH_OWNED_PICKUP_GROUP : 0,
        .source_location = location, .source_box = box, .source_slot = slot, .run_id = RhhModsRunId(),
        .mon.struct_size = sizeof(result.mon)};
    if (!Snapshot(mon, location, box, slot, &result.mon)) result.flags |= RHH_OWNED_PICKUP_VALID;
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
        location == RHH_MON_PARTY ? NATIVE_OWNED_PARTY : NATIVE_OWNED_BOX, box, slot}, mon);
    *out = result; return TRUE;
}
void ModsOwnedPickupCommitted(struct RhhOwnedPickup *pickup)
{
    if (!pickup || pickup->struct_size != sizeof(*pickup) || !RhhModsHasActiveRun()
        || pickup->run_id != RhhModsRunId() || sOwnedPickupContext || RhhModsCurrentEvent()
        || (!RhhModsHasHook(RHH_OWNED_MON_PICKED_UP) && !NativeOwnedTrackerActive(RhhModsRunId()))) return;
    NativeOwnedPcTrackPickup(pickup);
    if (!RhhModsHasHook(RHH_OWNED_MON_PICKED_UP)) return;
    if (sOwnedPickupRun != pickup->run_id) { sOwnedPickupRun = pickup->run_id; sOwnedPickupSequence = 0; }
    if (sOwnedPickupSequence == UINT64_MAX) return;
    pickup->sequence = sOwnedPickupSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_OWNED_MON_PICKED_UP,
        .run_seed = RhhModsRunSeed(), .ordinal = pickup->sequence,
        .event_key = pickup->mon.species, .subject = pickup->mon.identity,
        .args = {pickup->flags, pickup->source_location, pickup->source_box, pickup->source_slot}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sOwnedPickupContext = pickup; RhhModsDispatch(&event, &decision); sOwnedPickupContext = NULL;
}
bool32 ModsOwnedMoveCapture(struct BoxPokemon *moving, u32 sourceLocation, u32 sourceBox, u32 sourceSlot,
                            struct BoxPokemon *destination, u32 destinationLocation, u32 destinationBox,
                            u32 destinationSlot, bool32 group, struct RhhOwnedMove *out)
{
    if (!out || !moving || !moving->hasSpecies || !destination || !RhhModsHasActiveRun()
        || (!RhhModsHasHook(RHH_OWNED_MON_MOVED) && !RhhModsHasHook(RHH_OWNED_STORAGE_UNREADABLE)
            && !NativeOwnedTrackerActive(RhhModsRunId()))
        || RhhModsCurrentEvent() || gMain.inBattle || NativeSaveTransactionActive()
        || !OwnedPcSlotValid(sourceLocation, sourceBox, sourceSlot)
        || !OwnedPcSlotValid(destinationLocation, destinationBox, destinationSlot)) return FALSE;
    struct RhhOwnedMove result = {.struct_size = sizeof(result), .flags = group ? RHH_OWNED_MOVE_GROUP : 0,
        .source_location = sourceLocation, .source_box = sourceBox, .source_slot = sourceSlot,
        .destination_location = destinationLocation, .destination_box = destinationBox, .destination_slot = destinationSlot,
        .run_id = RhhModsRunId(), .before.struct_size = sizeof(result.before), .after.struct_size = sizeof(result.after),
        .displaced.struct_size = sizeof(result.displaced)};
    if (Snapshot(moving, RHH_MON_MOVING, 0, 0, &result.before)) result.flags |= RHH_OWNED_MOVE_BEFORE_UNREADABLE;
    if (destination->hasSpecies)
    {
        result.flags |= RHH_OWNED_MOVE_DISPLACED;
        if (Snapshot(destination, destinationLocation, destinationBox, destinationSlot, &result.displaced))
            result.flags |= RHH_OWNED_MOVE_DISPLACED_UNREADABLE;
    }
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
        group ? NATIVE_OWNED_GROUP : NATIVE_OWNED_CURSOR, 0, group ? sourceSlot : 0}, moving);
    if (destination->hasSpecies)
        NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
            destinationLocation == RHH_MON_PARTY ? NATIVE_OWNED_PARTY : NATIVE_OWNED_BOX,
            destinationBox, destinationSlot}, destination);
    *out = result; return TRUE;
}
void ModsOwnedMoveCommitted(struct BoxPokemon *placed, struct RhhOwnedMove *move)
{
    if (!placed || !move || move->struct_size != sizeof(*move) || !RhhModsHasActiveRun()
        || move->run_id != RhhModsRunId() || sOwnedMoveContext || RhhModsCurrentEvent()
        || (!RhhModsHasHook(RHH_OWNED_MON_MOVED) && !RhhModsHasHook(RHH_OWNED_STORAGE_UNREADABLE)
            && !NativeOwnedTrackerActive(RhhModsRunId()))) return;
    if (Snapshot(placed, move->destination_location, move->destination_box, move->destination_slot, &move->after))
        move->flags |= RHH_OWNED_MOVE_AFTER_UNREADABLE;
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
        move->flags & RHH_OWNED_MOVE_GROUP ? NATIVE_OWNED_GROUP : NATIVE_OWNED_CURSOR,
        0, move->flags & RHH_OWNED_MOVE_GROUP ? move->source_slot : 0}, placed);
    NativeOwnedPcTrackMove(move);
    if (move->flags & (RHH_OWNED_MOVE_BEFORE_UNREADABLE | RHH_OWNED_MOVE_AFTER_UNREADABLE))
    {
        struct RhhOwnedStorageUnreadable context = {.struct_size = sizeof(context),
            .cause = RHH_OWNED_STORAGE_UNREADABLE_MOVE, .run_id = move->run_id, .move = *move};
        OwnedStorageUnreadable(&context); return;
    }
    if (!RhhModsHasHook(RHH_OWNED_MON_MOVED)) return;
    if (sOwnedMoveRun != move->run_id) { sOwnedMoveRun = move->run_id; sOwnedMoveSequence = 0; }
    if (sOwnedMoveSequence == UINT64_MAX) return;
    move->sequence = sOwnedMoveSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_OWNED_MON_MOVED,
        .run_seed = RhhModsRunSeed(), .event_key = move->after.species, .subject = move->before.identity,
        .ordinal = move->sequence, .args = {move->flags, move->source_location, move->destination_location, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sOwnedMoveContext = move; RhhModsDispatch(&event, &decision); sOwnedMoveContext = NULL;
}
static int OwnedReleaseContext(struct RhhOwnedRelease *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sOwnedReleaseContext
        || RhhModsCurrentEvent() != RHH_OWNED_MON_RELEASED) return -1;
    *out = *sOwnedReleaseContext; return 0;
}
bool32 ModsOwnedReleaseCapture(struct BoxPokemon *mon, u32 location, u32 box, u32 slot, bool32 moving,
                               struct RhhOwnedRelease *out)
{
    if (!out || !mon || !mon->hasSpecies || !RhhModsHasActiveRun()
        || (!RhhModsHasHook(RHH_OWNED_MON_RELEASED) && !RhhModsHasHook(RHH_OWNED_STORAGE_UNREADABLE)
            && !NativeOwnedTrackerActive(RhhModsRunId()))
        || RhhModsCurrentEvent() || gMain.inBattle || NativeSaveTransactionActive()
        || (location != RHH_MON_PARTY && location != RHH_MON_BOX)
        || (location == RHH_MON_PARTY && (box || slot >= PARTY_SIZE))
        || (location == RHH_MON_BOX && (box >= TOTAL_BOXES_COUNT || slot >= IN_BOX_COUNT))) return FALSE;
    struct RhhOwnedRelease result = {.struct_size = sizeof(result), .flags = moving ? RHH_OWNED_RELEASE_MOVING : 0,
        .source_location = location, .source_box = box, .source_slot = slot, .run_id = RhhModsRunId(),
        .mon.struct_size = sizeof(result.mon)};
    if (Snapshot(mon, moving ? RHH_MON_MOVING : location, moving ? 0 : box, moving ? 0 : slot, &result.mon))
        result.flags |= RHH_OWNED_RELEASE_UNREADABLE;
    NativeOwnedTrackCheckWitness((struct NativeOwnedLedgerSlot){
        moving ? NATIVE_OWNED_CURSOR : location == RHH_MON_PARTY ? NATIVE_OWNED_PARTY : NATIVE_OWNED_BOX,
        moving ? 0 : box, moving ? 0 : slot}, mon);
    *out = result; return TRUE;
}
void ModsOwnedReleaseCommitted(struct RhhOwnedRelease *release)
{
    if (!release || release->struct_size != sizeof(*release) || !RhhModsHasActiveRun()
        || release->run_id != RhhModsRunId() || sOwnedReleaseContext || RhhModsCurrentEvent()
        || (!RhhModsHasHook(RHH_OWNED_MON_RELEASED) && !RhhModsHasHook(RHH_OWNED_STORAGE_UNREADABLE)
            && !NativeOwnedTrackerActive(RhhModsRunId()))) return;
    NativeOwnedPcTrackRelease(release);
    if (release->flags & RHH_OWNED_RELEASE_UNREADABLE)
    {
        struct RhhOwnedStorageUnreadable context = {.struct_size = sizeof(context),
            .cause = RHH_OWNED_STORAGE_UNREADABLE_RELEASE, .run_id = release->run_id, .release = *release};
        OwnedStorageUnreadable(&context); return;
    }
    if (!RhhModsHasHook(RHH_OWNED_MON_RELEASED)) return;
    if (sOwnedReleaseRun != release->run_id) { sOwnedReleaseRun = release->run_id; sOwnedReleaseSequence = 0; }
    if (sOwnedReleaseSequence == UINT64_MAX) return;
    release->sequence = sOwnedReleaseSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_OWNED_MON_RELEASED,
        .run_seed = RhhModsRunSeed(), .event_key = release->mon.species, .subject = release->mon.identity,
        .ordinal = release->sequence, .args = {release->flags, release->source_location, release->source_box, release->source_slot}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sOwnedReleaseContext = release; RhhModsDispatch(&event, &decision); sOwnedReleaseContext = NULL;
}
static int OwnedChangeContext(struct RhhOwnedMonChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sOwnedChangeContext
        || RhhModsCurrentEvent() != RHH_OWNED_MON_CHANGED) return -1;
    *out = *sOwnedChangeContext; return 0;
}
static int OwnedExperienceChangeContext(struct RhhOwnedExperienceChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sOwnedExperienceChangeContext
        || RhhModsCurrentEvent() != RHH_OWNED_MON_CHANGED) return -1;
    *out = *sOwnedExperienceChangeContext; return 0;
}
static u32 OwnedChangedFields(const struct RhhOwnedMonSnapshot *before, const struct RhhOwnedMonSnapshot *after)
{
    u32 fields = 0;
    if (before->identity != after->identity) fields |= RHH_OWNED_CHANGED_IDENTITY;
    if (before->species != after->species) fields |= RHH_OWNED_CHANGED_SPECIES;
    if (before->level != after->level) fields |= RHH_OWNED_CHANGED_LEVEL;
    if (before->nature != after->nature || before->stat_nature != after->stat_nature) fields |= RHH_OWNED_CHANGED_NATURE;
    if (memcmp(before->ivs, after->ivs, sizeof(before->ivs))) fields |= RHH_OWNED_CHANGED_IVS;
    if (memcmp(before->evs, after->evs, sizeof(before->evs))) fields |= RHH_OWNED_CHANGED_EVS;
    if (before->is_egg != after->is_egg) fields |= RHH_OWNED_CHANGED_EGG;
    if (before->max_iv != after->max_iv || before->max_stat_ev != after->max_stat_ev
        || before->max_total_ev != after->max_total_ev) fields |= RHH_OWNED_CHANGED_LIMITS;
    return fields;
}
static void NotifyOwnedChange(struct RhhOwnedMonChange *change, struct RhhOwnedExperienceChange *experience)
{
    change->run_id = sOwnedWatchRun; change->sequence = sOwnedWatchSequence++;
    if (experience) { experience->run_id = change->run_id; experience->sequence = change->sequence; }
    const struct RhhOwnedMonSnapshot *subject = (change->flags & RHH_OWNED_CHANGE_AFTER) ? &change->after : &change->before;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_OWNED_MON_CHANGED,
        .run_seed = RhhModsRunSeed(), .event_key = subject->species, .subject = subject->identity,
        .ordinal = change->sequence, .args = {change->flags, change->location, change->box, change->slot}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sOwnedChangeContext = change; sOwnedExperienceChangeContext = experience;
    RhhModsDispatch(&event, &decision); sOwnedChangeContext = NULL; sOwnedExperienceChangeContext = NULL;
}
void RhhModsObserveOwnedMons(void)
{
    if (sOwnedWatchBusy || !RhhModsHasActiveRun() || !RhhModsHasHook(RHH_OWNED_MON_CHANGED))
    { if (!sOwnedWatchBusy) sOwnedWatchReady = FALSE; return; }
    if (!OwnedStorageAvailable()) return;
    sOwnedWatchBusy = TRUE;
    bool32 initial = !sOwnedWatchReady || sOwnedWatchRun != RhhModsRunId();
    if (initial) { sOwnedWatchRun = RhhModsRunId(); sOwnedWatchSequence = 0; }
    u32 limits[] = {MAX_PER_STAT_IVS, MAX_PER_STAT_EVS, MAX_TOTAL_EVS};
    bool32 limitsChanged = initial || memcmp(limits, sOwnedWatchLimits, sizeof(limits));
    memcpy(sOwnedWatchLimits, limits, sizeof(limits));
    u32 changedCount = 0;
    for (u32 i = 0; i < OWNED_WATCH_SLOTS; ++i)
    {
        struct OwnedWatchSlot *watch = &sOwnedWatch[i];
        u32 location, box = 0, slot, index = i;
        bool32 eligible = TRUE;
        const struct BoxPokemon *raw;
        if (index < PARTY_SIZE)
        {
            location = RHH_MON_PARTY; slot = index;
            eligible = slot < gPartiesCount[B_TRAINER_PLAYER];
            raw = eligible ? &gParties[B_TRAINER_PLAYER][slot].box : NULL;
        }
        else if ((index -= PARTY_SIZE) < OWNED_WATCH_BOX_SLOTS)
        {
            location = RHH_MON_BOX; box = index / IN_BOX_COUNT; slot = index % IN_BOX_COUNT;
            raw = &gPokemonStoragePtr->boxes[box][slot];
        }
        else if ((index -= OWNED_WATCH_BOX_SLOTS) < OWNED_WATCH_DAYCARE_SLOTS)
        {
            location = RHH_MON_DAYCARE; slot = index;
#if IS_FRLG
            if (index == DAYCARE_MON_COUNT)
            { box = 1; slot = 0; raw = &gSaveBlock1Ptr->route5DayCareMon.mon; }
            else
#endif
                raw = &gSaveBlock1Ptr->daycare.mons[slot].mon;
        }
        else
        {
            location = RHH_MON_FUSION_STORAGE; slot = index - OWNED_WATCH_DAYCARE_SLOTS;
            raw = &gPokemonStoragePtr->fusions[slot].box;
        }
        if (!initial && !limitsChanged && eligible == watch->eligible
            && (!eligible || !memcmp(raw, &watch->raw, sizeof(*raw)))) continue;
        struct RhhOwnedMonSnapshot current = {.struct_size = sizeof(current)};
        bool32 present = raw && !Snapshot((struct BoxPokemon *)raw, location, box, slot, &current);
        u32 experience = 0, hyperTraining = 0, tera = 0;
        if (present)
        {
            struct BoxPokemon copy = *raw;
            experience = GetBoxMonData(&copy, MON_DATA_EXP);
            hyperTraining = HyperTrainingMask(&copy);
            u32 stored = 0;
            if (GetBoxMonStoredTeraType(&copy, &stored)) tera = (stored << 8) | GetBoxMonData(&copy, MON_DATA_TERA_TYPE);
        }
        if (!initial && (watch->present != present || (present &&
            (!RhhMonSnapshotsEqual(&watch->snapshot, &current) || watch->experience != experience
                || watch->hyperTraining != hyperTraining || watch->tera != tera))))
        {
            struct RhhOwnedMonChange *change = &sOwnedChanges[changedCount];
            struct RhhOwnedExperienceChange *xpChange = &sOwnedExperienceChanges[changedCount++];
            *change = (struct RhhOwnedMonChange){.struct_size = sizeof(*change), .location = location, .box = box, .slot = slot,
                .flags = (watch->present ? RHH_OWNED_CHANGE_BEFORE : 0) | (present ? RHH_OWNED_CHANGE_AFTER : 0),
                .before = watch->snapshot, .after = current};
            *xpChange = (struct RhhOwnedExperienceChange){.struct_size = sizeof(*xpChange), .flags = change->flags,
                .location = location, .box = box, .slot = slot,
                .before_exp = watch->present ? watch->experience : 0, .after_exp = present ? experience : 0};
            if (watch->present && present)
            {
                change->fields = OwnedChangedFields(&watch->snapshot, &current);
                if (watch->experience != experience) change->fields |= RHH_OWNED_CHANGED_EXPERIENCE;
                if (watch->hyperTraining != hyperTraining) change->fields |= RHH_OWNED_CHANGED_HYPER_TRAINING;
                if (watch->tera != tera) change->fields |= RHH_OWNED_CHANGED_TERA;
            }
        }
        if (raw) watch->raw = *raw; else memset(&watch->raw, 0, sizeof(watch->raw));
        watch->eligible = eligible; watch->present = present; watch->snapshot = current; watch->experience = experience;
        watch->hyperTraining = hyperTraining;
        watch->tera = tera;
    }
    sOwnedWatchReady = TRUE;
    if (initial)
    {
        struct RhhOwnedMonChange reset = {.struct_size = sizeof(reset), .flags = RHH_OWNED_CHANGE_RESET};
        NotifyOwnedChange(&reset, NULL);
    }
    else
        for (u32 i = 0; i < changedCount; ++i) NotifyOwnedChange(&sOwnedChanges[i], &sOwnedExperienceChanges[i]);
    sOwnedWatchBusy = FALSE;
}
_Static_assert(NUM_ABILITY_SLOTS == RHH_ABILITY_SPECIES_SLOTS, "public ability slots must match the engine");
int RhhModsAbilityInfo(RhhContentKey key, struct RhhAbilityInfo *out)
{
    u32 id;
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct RhhModAbility *custom = RhhModsCustomAbility(key);
    if (custom)
    {
        *out = (struct RhhAbilityInfo){.struct_size = sizeof(*out), .flags = custom->flags, .ability = key};
        return 0;
    }
    if (RhhContentResolve(RHH_ABILITY, key, &id)
        || !id || id >= ABILITIES_COUNT) return -1;
    const struct AbilityInfo *ability = &gAbilitiesInfo[id];
    struct RhhAbilityInfo result = {.struct_size = sizeof(result), .ability = RhhContentKeyForId(RHH_ABILITY, id),
        .flags = (ability->cantBeCopied ? RHH_ABILITY_CANNOT_COPY : 0)
               | (ability->cantBeSwapped ? RHH_ABILITY_CANNOT_SWAP : 0)
               | (ability->cantBeTraced ? RHH_ABILITY_CANNOT_TRACE : 0)
               | (ability->cantBeSuppressed ? RHH_ABILITY_CANNOT_SUPPRESS : 0)
               | (ability->cantBeOverwritten ? RHH_ABILITY_CANNOT_OVERWRITE : 0)
               | (ability->breakable ? RHH_ABILITY_BREAKABLE : 0)
               | (ability->failsOnImposter ? RHH_ABILITY_FAILS_ON_IMPOSTER : 0)};
    if (!result.ability) return -1;
    *out = result; return 0;
}
static int CopyCustomAbility(const struct RhhModAbility *ability, struct RhhCustomAbilityInfo *out)
{
    if (!ability || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhCustomAbilityInfo result = {.struct_size = sizeof(result), .flags = ability->flags,
        .ability = RhhContentKeyOf(ability->id)};
    memcpy(result.id, ability->id, strlen(ability->id) + 1);
    memcpy(result.label, ability->label, strlen(ability->label) + 1);
    memcpy(result.description, ability->description, strlen(ability->description) + 1);
    *out = result; return 0;
}
static int CustomAbilityAt(u32 index, struct RhhCustomAbilityInfo *out)
{ return CopyCustomAbility(RhhModsCustomAbilityAt(index), out); }
static int CustomAbilityInfo(RhhContentKey key, struct RhhCustomAbilityInfo *out)
{ return CopyCustomAbility(RhhModsCustomAbility(key), out); }
static char AbilityNameGlyph(u8 c)
{
    if (c >= CHAR_A && c <= CHAR_Z) return 'A' + c - CHAR_A;
    if (c >= CHAR_a && c <= CHAR_z) return 'a' + c - CHAR_a;
    if (c >= CHAR_0 && c <= CHAR_9) return '0' + c - CHAR_0;
    switch (c) {
    case CHAR_SPACE: return ' '; case CHAR_EXCL_MARK: return '!'; case CHAR_PERIOD: return '.';
    case CHAR_HYPHEN: return '-'; case CHAR_COLON: return ':'; case CHAR_COMMA: return ',';
    case CHAR_SLASH: return '/'; case CHAR_PLUS: return '+'; case CHAR_AMPERSAND: return '&';
    case CHAR_LEFT_PAREN: return '('; case CHAR_RIGHT_PAREN: return ')'; case CHAR_PERCENT: return '%';
    case CHAR_SGL_QUOTE_LEFT: case CHAR_SGL_QUOTE_RIGHT: return '\''; default: return '?';
    }
}
int RhhModsAbilityName(RhhContentKey key, char *out, uint32_t capacity)
{
    if (!RhhModsFinalized() || !out || !capacity) return -1;
    char result[33]; u32 length = 0, id;
    const struct RhhModAbility *custom = RhhModsCustomAbility(key);
    if (custom)
    {
        length = strlen(custom->label);
        if (length >= sizeof(result)) return -1;
        memcpy(result, custom->label, length);
    }
    else
    {
        if (RhhContentResolve(RHH_ABILITY, key, &id) || !id || id >= ABILITIES_COUNT) return -1;
        const u8 *name = gAbilitiesInfo[id].name;
        while (length < sizeof(result) - 1 && name[length] != EOS)
        { result[length] = AbilityNameGlyph(name[length]); ++length; }
        if (name[length] != EOS) return -1;
    }
    result[length] = 0;
    if (capacity <= length) return -1;
    memcpy(out, result, length + 1); return 0;
}
static int SpeciesAbilities(RhhContentKey key, struct RhhSpeciesAbilities *out)
{
    u32 species; struct RhhContentInfo info = {.struct_size = sizeof(info)};
    if (!out || out->struct_size < sizeof(*out) || !RhhModsFinalized()
        || RhhContentResolve(RHH_SPECIES, key, &species) || !species || species >= NUM_SPECIES
        || species == SPECIES_EGG || RhhContentInfoFor(RHH_SPECIES, key, &info)
        || !(info.flags & RHH_CONTENT_ENABLED)) return -1;
    struct RhhSpeciesAbilities result = {.struct_size = sizeof(result), .slot_count = NUM_ABILITY_SLOTS,
        .normal_slot_count = NUM_NORMAL_ABILITY_SLOTS, .species = RhhContentKeyForId(RHH_SPECIES, species)};
    for (u32 i = 0; i < NUM_ABILITY_SLOTS; ++i)
    {
        u32 ability = GetSpeciesAbility(species, i);
        if (ability && !(result.slots[i] = RhhContentKeyForId(RHH_ABILITY, ability))) return -1;
    }
    *out = result; return 0;
}
static int OwnedAbility(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedAbility *out)
{
    if (!out || out->struct_size < sizeof(*out) || !expected || expected->struct_size < sizeof(*expected)
        || (gMain.inBattle && !RhhModsAbilityViewAllowed())) return -1;
    struct RhhOwnedMonSnapshot current = {.struct_size = sizeof(current)}, unique = {.struct_size = sizeof(unique)};
    struct BoxPokemon copy;
    if (expected->location == RHH_MON_PARTY)
    {
        if (PartyAt(expected->slot, &current)) return -1;
        copy = gParties[B_TRAINER_PLAYER][expected->slot].box;
    }
    else if (expected->location == RHH_MON_BOX)
    {
        if (BoxAt(expected->box, expected->slot, &current)) return -1;
        copy = gPokemonStoragePtr->boxes[expected->box][expected->slot];
    }
    else return -1;
    if (!RhhMonSnapshotsEqual(expected, &current) || !UniqueOwnedIdentity(current.identity) || Find(current.identity, &unique)
        || !RhhMonSnapshotsEqual(&current, &unique)) return -1;
    u32 species = GetBoxMonData(&copy, MON_DATA_SPECIES), slot = GetBoxMonData(&copy, MON_DATA_ABILITY_NUM);
    u32 ability = GetAbilityBySpeciesReadOnly(species, slot);
    struct RhhOwnedAbility result = {.struct_size = sizeof(result), .selected_slot = slot, .mon = current};
    if (ability && !(result.ability = RhhContentKeyForId(RHH_ABILITY, ability))) return -1;
    *out = result; return 0;
}
static struct RhhInnateList *sInnateList;
static const struct RhhOwnedMonSnapshot *sInnatePersonalMon;
static const struct RhhInnateBattleContext *sInnateBattleContext;
static int InnateContext(struct RhhInnateList *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnateList
        || RhhModsCurrentEvent() != RHH_INNATE_PROPOSE) return -1;
    *out = *sInnateList; return 0;
}
static int OfferInnate(const struct RhhInnateOffer *offer)
{
    if (!sInnateList || (RhhModsCurrentEvent() != RHH_INNATE_PROPOSE && RhhModsCurrentEvent() != RHH_INNATE_PERSONALIZE
                         && RhhModsCurrentEvent() != RHH_INNATE_BATTLE_PERSONALIZE)
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return RHH_INNATE_UNAVAILABLE;
    struct RhhAbilityInfo ability = {.struct_size = sizeof(ability)};
    if (!offer || offer->struct_size < sizeof(*offer) || offer->reserved[0] || offer->reserved[1]
        || !offer->unlock_level || offer->unlock_level > MAX_LEVEL
        || RhhModsAbilityInfo(offer->ability, &ability) || ability.ability != offer->ability) return RHH_INNATE_INVALID;
    for (u32 i = 0; i < sInnateList->count; ++i)
        if (sInnateList->entries[i].ability == offer->ability)
            return sInnateList->entries[i].unlock_level == offer->unlock_level ? RHH_INNATE_OK : RHH_INNATE_CONFLICT;
    if (sInnateList->count >= RHH_INNATE_LIMIT) return RHH_INNATE_FULL;
    sInnateList->entries[sInnateList->count++] = (struct RhhInnateEntry){
        .ability = offer->ability, .unlock_level = offer->unlock_level,
        .unlocked = sInnateList->level >= offer->unlock_level};
    return RHH_INNATE_OK;
}
int RhhModsSpeciesInnates(RhhContentKey key, uint32_t level, struct RhhInnateList *out)
{
    u32 species; struct RhhContentInfo info = {.struct_size = sizeof(info)};
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || !RhhModsFinalized()
        || sInnateList || !level || level > MAX_LEVEL || RhhContentResolve(RHH_SPECIES, key, &species)
        || !species || species >= NUM_SPECIES || species == SPECIES_EGG
        || RhhContentInfoFor(RHH_SPECIES, key, &info) || !(info.flags & RHH_CONTENT_ENABLED)) return -1;
    struct RhhInnateList result = {.struct_size = sizeof(result), .level = level,
        .species = RhhContentKeyForId(RHH_SPECIES, species)};
    if (!result.species) return -1;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_INNATE_PROPOSE,
        .run_seed = RhhModsRunSeed(), .event_key = result.species, .args = {level}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sInnateList = &result; RhhModsDispatch(&event, &decision); sInnateList = NULL;
    bool32 usedRng = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed || usedRng) { result.count = 0; memset(result.entries, 0, sizeof(result.entries)); }
    *out = result; return 0;
}
static int PersonalInnateContext(struct RhhInnatePersonalContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnatePersonalMon || !sInnateList
        || RhhModsCurrentEvent() != RHH_INNATE_PERSONALIZE) return -1;
    const struct RhhOwnedMonSnapshot *mon = sInnatePersonalMon;
    *out = (struct RhhInnatePersonalContext){.struct_size = sizeof(*out), .location = mon->location,
        .box = mon->box, .slot = mon->slot, .identity = mon->identity, .species = mon->species,
        .level = mon->level, .list = *sInnateList};
    return 0;
}
static int RemoveInnate(RhhContentKey ability)
{
    for (u32 i = 0; i < sInnateList->count; ++i)
        if (sInnateList->entries[i].ability == ability)
        {
            --sInnateList->count;
            memmove(&sInnateList->entries[i], &sInnateList->entries[i + 1],
                    (sInnateList->count - i) * sizeof(sInnateList->entries[0]));
            memset(&sInnateList->entries[sInnateList->count], 0, sizeof(sInnateList->entries[0]));
            return RHH_INNATE_OK;
        }
    return RHH_INNATE_INVALID;
}
static int RemovePersonalInnate(RhhContentKey ability)
{
    if (!sInnateList || !sInnatePersonalMon || RhhModsCurrentEvent() != RHH_INNATE_PERSONALIZE
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return RHH_INNATE_UNAVAILABLE;
    return RemoveInnate(ability);
}
static int BattleInnateContext(struct RhhInnateBattleContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnateBattleContext || !sInnateList
        || RhhModsCurrentEvent() != RHH_INNATE_BATTLE_PERSONALIZE) return -1;
    *out = *sInnateBattleContext; out->list = *sInnateList; return 0;
}
static int RemoveBattleInnate(RhhContentKey ability)
{
    if (!sInnateList || !sInnateBattleContext || RhhModsCurrentEvent() != RHH_INNATE_BATTLE_PERSONALIZE
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return RHH_INNATE_UNAVAILABLE;
    return RemoveInnate(ability);
}
int RhhModsPersonalizeBattleInnates(const struct RhhInnateBattleContext *context, struct RhhInnateList *list)
{
    if (!context || context->struct_size < sizeof(*context) || !list || list->struct_size < sizeof(*list)
        || sInnateList || sInnatePersonalMon || sInnateBattleContext || !RhhModsHasActiveRun()
        || !RhhModsHasHook(RHH_INNATE_BATTLE_PERSONALIZE) || context->species != list->species
        || context->level != list->level) return -1;
    struct RhhInnateList original = *list;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_INNATE_BATTLE_PERSONALIZE,
        .run_seed = RhhModsRunSeed(), .event_key = context->species, .subject = context->trainer,
        .ordinal = context->battle_id, .args = {context->battler, context->owner, context->party_slot, context->level}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sInnateBattleContext = context; sInnateList = list; RhhModsDispatch(&event, &decision);
    sInnateList = NULL; sInnateBattleContext = NULL;
    bool32 usedRng = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed || usedRng) *list = original;
    return 0;
}
static int FreshPersonalMon(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedMonSnapshot *current)
{
    if (!expected || expected->struct_size < sizeof(*expected) || !current) return -1;
    current->struct_size = sizeof(*current);
    int result = expected->location == RHH_MON_PARTY ? PartyAt(expected->slot, current)
        : expected->location == RHH_MON_BOX ? BoxAt(expected->box, expected->slot, current) : -1;
    if (result || current->is_egg || !RhhMonSnapshotsEqual(expected, current)
        || !UniqueOwnedIdentity(current->identity)) return -1;
    return 0;
}
int RhhModsOwnedBattleTeraMon(u32 slot, u32 level, u64 identity,
                               struct RhhOwnedMonSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhOwnedMonSnapshot expected = {.struct_size = sizeof(expected)}, current;
    if (PartyAt(slot, &expected) || expected.level != level || expected.identity != identity
        || FreshPersonalMon(&expected, &current)) return -1;
    *out = current; return 0;
}
int RhhModsPersonalizeOwnedInnates(const struct RhhOwnedMonSnapshot *expected, struct RhhInnateList *list)
{
    if (!expected || !list || list->struct_size < sizeof(*list) || sInnateList || sInnatePersonalMon
        || !RhhModsHasActiveRun() || !RhhModsHasHook(RHH_INNATE_PERSONALIZE)) return -1;
    struct RhhOwnedMonSnapshot current;
    if (FreshPersonalMon(expected, &current) || current.species != list->species || current.level != list->level) return -1;
    struct RhhInnateList original = *list;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_INNATE_PERSONALIZE,
        .run_seed = RhhModsRunSeed(), .event_key = current.species, .subject = current.identity,
        .args = {current.level, current.location, current.box, current.slot}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sInnatePersonalMon = &current; sInnateList = list;
    RhhModsDispatch(&event, &decision);
    sInnateList = NULL; sInnatePersonalMon = NULL;
    bool32 usedRng = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed || usedRng) *list = original;
    return 0;
}
int RhhModsPersonalizePartyInnates(u32 slot, RhhContentKey species, u32 level, u64 identity, struct RhhInnateList *list)
{
    if (!RhhModsHasHook(RHH_INNATE_PERSONALIZE)) return -1;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
    if (PartyAt(slot, &mon) || mon.species != species || mon.level != level || mon.identity != identity) return -1;
    return RhhModsPersonalizeOwnedInnates(&mon, list);
}
static int OwnedInnates(const struct RhhOwnedMonSnapshot *expected, struct RhhInnateList *out)
{
    if (!out || out->struct_size < sizeof(*out) || (gMain.inBattle && !RhhModsAbilityViewAllowed())) return -1;
    struct RhhOwnedMonSnapshot current;
    if (FreshPersonalMon(expected, &current)) return -1;
    struct RhhInnateList result = {.struct_size = sizeof(result)};
    if (RhhModsSpeciesInnates(current.species, current.level, &result)) return -1;
    if (RhhModsHasHook(RHH_INNATE_PERSONALIZE) && RhhModsPersonalizeOwnedInnates(&current, &result)) return -1;
    *out = result; return 0;
}
static const struct RhhAbilityService sAbilityService = {
    {sizeof(sAbilityService), RHH_SERVICE_VERSION(1, 40)}, RhhModsAbilityInfo, SpeciesAbilities, OwnedAbility, RhhModsBattlerAbility,
    RhhModsSpeciesInnates, InnateContext, OfferInnate, RhhModsBattlerInnates, RhhModsPowerInnates,
    RhhModsInnatePowerContext, RhhModsCustomAbilityCount, CustomAbilityAt, CustomAbilityInfo,
    RhhModsAccuracyInnates, RhhModsInnateAccuracyContext, RhhModsInnateEntryContext, RhhModsInnateEntryMessage, RhhModsInnateEntryHeal,
    RhhModsInnateEntryStat, RhhModsAbilityName, OwnedInnates, PersonalInnateContext, RemovePersonalInnate,
    RhhModsInnateTurnContext, RhhModsInnateTurnMessage, RhhModsInnateTurnHeal, RhhModsInnateTurnStat,
    RhhModsAiInnateEntryContext, RhhModsOfferAiInnateEntryHeal,
    RhhModsAiInnateEntryStatContext, RhhModsOfferAiInnateEntryStat,
    RhhModsInnateTurnHealTarget, RhhModsInnateTurnStatTarget,
    RhhModsInnateEntryHealTarget, RhhModsInnateEntryStatTarget,
    RhhModsInnateEntryStatFrom, RhhModsInnateTurnStatFrom,
    RhhModsAiInnateEntryOpponentContext, RhhModsOfferAiInnateEntryOpponentStat,
    RhhModsAiInnateTurnHealContext, RhhModsOfferAiInnateTurnHeal,
    RhhModsAiInnateTurnStepContext, RhhModsOfferAiInnateTurnStepHeal,
    BattleInnateContext, RemoveBattleInnate, RhhModsAbilityObservationContext, RhhModsInnateObservationContext,
    RhhModsInnateTurnStatusTarget, RhhModsInnateEntryStatusTarget,
    RhhModsAiInnateEntryOpponentStatusContext, RhhModsOfferAiInnateEntryOpponentStatus, RhhModsAbilityMutationContext,
    RhhModsAbilityContactMutationContext, RhhModsGasChangeContext, RhhModsPriorityInnates, RhhModsInnatePriorityContext,
    RhhModsSpeedInnates, RhhModsInnateSpeedContext, RhhModsDamageStatInnates, RhhModsInnateDamageStatContext,
    RhhModsInnateEntryWeather, RhhModsInnateEntryTerrain, RhhModsInnateTurnWeather, RhhModsInnateTurnTerrain,
    RhhModsAiInnateEntryFieldContext, RhhModsOfferAiInnateEntryWeather, RhhModsOfferAiInnateEntryTerrain,
    RhhModsAiInnateEntryFieldProjected, RhhModsCriticalStageInnates, RhhModsInnateCriticalStageContext,
    RhhModsFinalDamageInnates, RhhModsInnateFinalDamageContext
};
static uint32_t PartySlots(void) { return PARTY_SIZE; }
static uint32_t BoxCount(void) { return TOTAL_BOXES_COUNT; }
static uint32_t BoxSlots(void) { return IN_BOX_COUNT; }
_Static_assert((int)RHH_STAT_HP == (int)STAT_HP && (int)RHH_STAT_ATTACK == (int)STAT_ATK
    && (int)RHH_STAT_DEFENSE == (int)STAT_DEF && (int)RHH_STAT_SPEED == (int)STAT_SPEED
    && (int)RHH_STAT_SP_ATTACK == (int)STAT_SPATK && (int)RHH_STAT_SP_DEFENSE == (int)STAT_SPDEF,
    "public nature multipliers must use the engine's stat order");
static const char *const sNatureNames[NUM_NATURES] = {
    [NATURE_HARDY] = "Hardy", [NATURE_LONELY] = "Lonely", [NATURE_BRAVE] = "Brave",
    [NATURE_ADAMANT] = "Adamant", [NATURE_NAUGHTY] = "Naughty", [NATURE_BOLD] = "Bold",
    [NATURE_DOCILE] = "Docile", [NATURE_RELAXED] = "Relaxed", [NATURE_IMPISH] = "Impish",
    [NATURE_LAX] = "Lax", [NATURE_TIMID] = "Timid", [NATURE_HASTY] = "Hasty",
    [NATURE_SERIOUS] = "Serious", [NATURE_JOLLY] = "Jolly", [NATURE_NAIVE] = "Naive",
    [NATURE_MODEST] = "Modest", [NATURE_MILD] = "Mild", [NATURE_QUIET] = "Quiet",
    [NATURE_BASHFUL] = "Bashful", [NATURE_RASH] = "Rash", [NATURE_CALM] = "Calm",
    [NATURE_GENTLE] = "Gentle", [NATURE_SASSY] = "Sassy", [NATURE_CAREFUL] = "Careful",
    [NATURE_QUIRKY] = "Quirky"
};
static RhhContentKey NatureKey(unsigned ordinal)
{
    if (ordinal >= NUM_NATURES || !sNatureNames[ordinal]) return 0;
    char name[32] = "rhh:nature/";
    unsigned p = 11;
    for (const char *s = sNatureNames[ordinal]; *s && p + 1 < sizeof(name); ++s)
        name[p++] = *s >= 'A' && *s <= 'Z' ? *s + ('a' - 'A') : *s;
    name[p] = 0;
    return RhhContentKeyOf(name);
}
static uint32_t NatureCount(void) { return NUM_NATURES; }
static int NatureAt(uint32_t ordinal, struct RhhNatureInfo *out)
{
    if (!out || out->struct_size < sizeof(*out) || ordinal >= NUM_NATURES || !sNatureNames[ordinal]) return -1;
    struct RhhNatureInfo result = {.struct_size = sizeof(result), .ordinal = ordinal, .key = NatureKey(ordinal)};
    snprintf(result.name, sizeof(result.name), "%s", sNatureNames[ordinal]);
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i)
        result.stat_percent[i] = ModifyStatByNature(ordinal, 100, (enum Stat)i);
    *out = result;
    return 0;
}
static int NatureFind(RhhContentKey key, struct RhhNatureInfo *out)
{
    if (!key || !out || out->struct_size < sizeof(*out)) return -1;
    for (unsigned i = 0; i < NUM_NATURES; ++i)
        if (NatureKey(i) == key) return NatureAt(i, out);
    return -1;
}
static int NatureEffects(uint32_t ordinal, struct RhhNatureEffects *out)
{
    _Static_assert((int)RHH_FLAVOR_COUNT == (int)FLAVOR_COUNT, "nature flavor vocabulary must match engine");
    if (!out || out->struct_size < sizeof(*out) || ordinal >= NUM_NATURES || !sNatureNames[ordinal]) return -1;
    const struct NatureInfo *nature = &gNaturesInfo[ordinal];
    struct RhhNatureEffects result = {.struct_size = sizeof(result), .ordinal = ordinal, .key = NatureKey(ordinal)};
    for (unsigned i = 0; i < RHH_FLAVOR_COUNT; ++i)
        result.flavor[i] = gPokeblockFlavorCompatibilityTable[ordinal * FLAVOR_COUNT + i];
    for (unsigned hp = 0; hp < 2; ++hp)
    {
        unsigned attack = nature->battlePalacePercents[hp * 2], cutoff = nature->battlePalacePercents[hp * 2 + 1];
        if (attack > cutoff || cutoff > 100) return -1;
        result.palace_percent[hp][RHH_PALACE_ATTACK] = attack;
        result.palace_percent[hp][RHH_PALACE_DEFENSE] = cutoff - attack;
        result.palace_percent[hp][RHH_PALACE_SUPPORT] = 100 - cutoff;
    }
    switch (nature->battlePalaceSmokescreen)
    {
    case PALACE_TARGET_STRONGER: result.palace_target = RHH_PALACE_TARGET_STRONGER; break;
    case PALACE_TARGET_WEAKER: result.palace_target = RHH_PALACE_TARGET_WEAKER; break;
    case PALACE_TARGET_RANDOM: result.palace_target = RHH_PALACE_TARGET_RANDOM; break;
    default: return -1;
    }
    *out = result; return 0;
}
static const struct RhhNatureService sNatureService = {
    {sizeof(sNatureService), RHH_SERVICE_VERSION(1, 4)}, NatureCount, NatureAt, NatureFind, NatureEffects, NatureFormatPreflight
};
static const struct RhhEggHatchContext *sEggHatchContext;
static int EggHatchContext(struct RhhEggHatchContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sEggHatchContext) return -1;
    *out = *sEggHatchContext;
    return 0;
}
static const struct RhhEggHatchService sEggHatchService = {
    {sizeof(sEggHatchService), RHH_SERVICE_VERSION(1, 1)}, EggHatchContext
};
bool32 ModsEggHatchBefore(u32 partySlot, struct RhhOwnedMonSnapshot *before)
{
    if (!RhhModsHasActiveRun() || sEggHatchContext || !before
        || PartyAt(partySlot, before) || !before->is_egg) return FALSE;
    return TRUE;
}
void ModsEggHatched(u32 partySlot, const struct RhhOwnedMonSnapshot *before)
{
    if (!before || before->struct_size < sizeof(*before) || !before->is_egg
        || before->location != RHH_MON_PARTY || before->slot != partySlot
        || !RhhModsHasActiveRun() || sEggHatchContext) return;
    struct RhhEggHatchContext context = {.struct_size = sizeof(context), .egg = *before};
    context.hatched.struct_size = sizeof(context.hatched);
    if (PartyAt(partySlot, &context.hatched) || context.hatched.is_egg
        || context.hatched.species != before->species
        || (u32)context.hatched.identity != (u32)before->identity) return;
    if (gSaveBlock1Ptr)
        context.map = RhhContentKeyForId(RHH_MAP, (gSaveBlock1Ptr->location.mapGroup << 8)
                                        | (u8)gSaveBlock1Ptr->location.mapNum);
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_EGG_HATCHED,
        .run_seed = RhhModsRunSeed(), .event_key = context.hatched.species,
        .subject = context.egg.identity, .ordinal = context.hatched.identity,
        .args = {partySlot}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1,
        .value = context.hatched.species};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    sEggHatchContext = &context; RhhModsDispatch(&event, &decision); sEggHatchContext = NULL;
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
}
static int DaycareParentAt(uint32_t slot, struct RhhDaycareParent *out);
static int sEditing;
static const struct RhhMonEditResult *sEditContext;
static const struct RhhMonEditPayment *sEditPaymentContext;
static const struct RhhMonEditRecipePayment *sEditRecipeContext;
static const struct RhhHyperTrainingEditContext *sHyperEditContext;
static const struct RhhTrueNatureEditContext *sTrueNatureEditContext;
static const struct RhhTeraEditContext *sTeraEditContext;
static int TeraEditContext(struct RhhTeraEditContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sTeraEditContext
        || (RhhModsCurrentEvent() != RHH_CAN_EDIT_MON && RhhModsCurrentEvent() != RHH_MON_EDITED)) return -1;
    *out = *sTeraEditContext; return 0;
}
static int TrueNatureEditContext(struct RhhTrueNatureEditContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sTrueNatureEditContext
        || (RhhModsCurrentEvent() != RHH_CAN_EDIT_MON && RhhModsCurrentEvent() != RHH_MON_EDITED)) return -1;
    *out = *sTrueNatureEditContext; return 0;
}
static int HyperEditContext(struct RhhHyperTrainingEditContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sHyperEditContext
        || (RhhModsCurrentEvent() != RHH_CAN_EDIT_MON && RhhModsCurrentEvent() != RHH_MON_EDITED)) return -1;
    *out = *sHyperEditContext; return 0;
}
static int EditContext(struct RhhMonEditResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sEditContext
        || (event != RHH_CAN_EDIT_MON && event != RHH_MON_EDITED)) return -1;
    *out = *sEditContext;
    return 0;
}
static int EditPaymentContext(struct RhhMonEditPayment *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sEditPaymentContext
        || (event != RHH_CAN_EDIT_MON && event != RHH_MON_EDITED)) return -1;
    *out = *sEditPaymentContext; return 0;
}
static int EditRecipeContext(struct RhhMonEditRecipePayment *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sEditRecipeContext
        || (event != RHH_CAN_EDIT_MON && event != RHH_MON_EDITED)) return -1;
    *out = *sEditRecipeContext; return 0;
}
struct EditPaymentPlan {
    u32 bag, pocket, levelMode, capacity, money, item;
    struct ItemSlot *slots, *after;
    struct ItemSlot *storage;
    bool32 compact;
};
static u32 EditPaymentBag(void)
{
    return CurrentBattlePyramidLocation() != PYRAMID_LOCATION_NONE || FlagGet(FLAG_STORING_ITEMS_IN_PYRAMID_BAG)
        ? RHH_EDIT_BAG_PYRAMID : RHH_EDIT_BAG_NORMAL;
}
static struct ItemSlot EditPaymentSlot(const struct EditPaymentPlan *plan, u32 slot)
{
    if (plan->bag == RHH_EDIT_BAG_PYRAMID)
        return (struct ItemSlot){gSaveBlock2Ptr->frontier.pyramidBag.itemId[plan->levelMode][slot],
            gSaveBlock2Ptr->frontier.pyramidBag.quantity[plan->levelMode][slot]};
    return BagPocket_GetSlotData(&gBagPockets[plan->pocket], slot);
}
static int PrepareEditPayment(const struct RhhMonEditCost *cost, struct RhhMonEditPayment *out,
                              struct EditPaymentPlan *plan, const char **reason)
{
    if (!cost || cost->struct_size < sizeof(*cost) || cost->reserved
        || (!!cost->item != !!cost->item_quantity) || !gSaveBlock1Ptr || !gSaveBlock2Ptr)
    { *reason = "invalid edit cost"; return RHH_EDIT_INVALID; }
    *out = (struct RhhMonEditPayment){.struct_size = sizeof(*out), .item = cost->item,
        .item_quantity = cost->item_quantity, .money = cost->money};
    plan->money = out->money_before = GetMoney(&gSaveBlock1Ptr->money);
    if (cost->money > plan->money)
    { *reason = "not enough money"; return RHH_EDIT_DENIED; }
    out->money_after = plan->money - cost->money;
    if (!cost->item) return RHH_EDIT_OK;
    u32 item;
    if (RhhContentResolve(RHH_ITEM, cost->item, &item) || !item || item >= ITEMS_COUNT)
    { *reason = "invalid cost item"; return RHH_EDIT_INVALID; }
    u32 itemPocket = GetItemPocket(item);
    if (itemPocket >= POCKETS_COUNT)
    { *reason = "item cannot be held in the bag"; return RHH_EDIT_INVALID; }
    plan->item = item;
    plan->bag = out->bag = EditPaymentBag();
    if (plan->bag == RHH_EDIT_BAG_PYRAMID)
    {
        plan->levelMode = gSaveBlock2Ptr->frontier.lvlMode;
        if (plan->levelMode >= FRONTIER_LVL_MODE_COUNT)
        { *reason = "invalid Pyramid bag"; return RHH_EDIT_UNAVAILABLE; }
        plan->capacity = PYRAMID_BAG_ITEMS_COUNT;
    }
    else
    {
        plan->pocket = itemPocket;
        struct BagPocket *pocket = &gBagPockets[plan->pocket];
        if (!pocket->itemSlots || pocket->id != plan->pocket || !pocket->capacity)
        { *reason = "bag is unavailable"; return RHH_EDIT_UNAVAILABLE; }
        plan->storage = pocket->itemSlots; plan->capacity = pocket->capacity;
    }
    plan->slots = calloc(plan->capacity * 2, sizeof(*plan->slots));
    if (!plan->slots) { *reason = "could not stage payment"; return RHH_EDIT_UNAVAILABLE; }
    plan->after = plan->slots + plan->capacity;
    u32 remaining = cost->item_quantity, touchedTotal = 0;
    for (u32 i = 0; i < plan->capacity; ++i)
    {
        struct ItemSlot entry = EditPaymentSlot(plan, i); plan->slots[i] = entry; plan->after[i] = entry;
        if (entry.itemId != item) continue;
        out->item_before += entry.quantity;
        if (remaining)
        {
            u32 take = min(remaining, entry.quantity);
            touchedTotal += entry.quantity; remaining -= take;
            plan->after[i].quantity -= take;
            if (!plan->after[i].quantity) plan->after[i].itemId = ITEM_NONE;
        }
    }
    if (remaining) { *reason = "not enough items"; return RHH_EDIT_DENIED; }
    out->item_after = out->item_before - cost->item_quantity;
    /* Match ordinary bag removal's compaction when whole touched stacks were
     * consumed. The Pyramid bag keeps its slots in place. */
    if (plan->bag == RHH_EDIT_BAG_NORMAL && touchedTotal == cost->item_quantity)
    {
        u32 next = 0;
        for (u32 i = 0; i < plan->capacity; ++i)
            if (plan->after[i].itemId != ITEM_NONE) plan->after[next++] = plan->after[i];
        while (next < plan->capacity) plan->after[next++] = (struct ItemSlot){ITEM_NONE, 0};
    }
    return RHH_EDIT_OK;
}
static bool32 EditPaymentUnchanged(const struct EditPaymentPlan *plan)
{
    if (!gSaveBlock1Ptr || !gSaveBlock2Ptr || GetMoney(&gSaveBlock1Ptr->money) != plan->money) return FALSE;
    if (!plan->bag) return TRUE;
    if (EditPaymentBag() != plan->bag) return FALSE;
    if (plan->bag == RHH_EDIT_BAG_PYRAMID)
    {
        if (gSaveBlock2Ptr->frontier.lvlMode != plan->levelMode) return FALSE;
    }
    else if (gBagPockets[plan->pocket].itemSlots != plan->storage
             || gBagPockets[plan->pocket].capacity != plan->capacity
             || gBagPockets[plan->pocket].id != plan->pocket) return FALSE;
    for (u32 i = 0; i < plan->capacity; ++i)
    {
        struct ItemSlot current = EditPaymentSlot(plan, i);
        if (current.itemId != plan->slots[i].itemId || current.quantity != plan->slots[i].quantity) return FALSE;
    }
    return TRUE;
}
static void CommitEditPaymentSlots(const struct EditPaymentPlan *plan)
{
    if (plan->bag == RHH_EDIT_BAG_PYRAMID)
        for (u32 i = 0; i < plan->capacity; ++i)
        {
            if (plan->after[i].itemId == plan->slots[i].itemId && plan->after[i].quantity == plan->slots[i].quantity)
                continue;
            gSaveBlock2Ptr->frontier.pyramidBag.itemId[plan->levelMode][i] = plan->after[i].itemId;
            gSaveBlock2Ptr->frontier.pyramidBag.quantity[plan->levelMode][i] = plan->after[i].quantity;
        }
    else if (plan->bag == RHH_EDIT_BAG_NORMAL)
    {
        for (u32 i = 0; i < plan->capacity; ++i)
            if (plan->after[i].itemId != plan->slots[i].itemId || plan->after[i].quantity != plan->slots[i].quantity)
                BagPocket_SetSlotData(&gBagPockets[plan->pocket], i, plan->after[i]);
    }
}
static void MarkEditBagUse(u32 bag, u32 item)
{
    if (bag == RHH_EDIT_BAG_NORMAL && CurMapIsSecretBase())
    {
        VarSet(VAR_SECRET_BASE_LOW_TV_FLAGS, VarGet(VAR_SECRET_BASE_LOW_TV_FLAGS) | SECRET_BASE_USED_BAG);
        VarSet(VAR_SECRET_BASE_LAST_ITEM_USED, item);
    }
}
static void CommitEditPayment(const struct EditPaymentPlan *plan, const struct RhhMonEditPayment *payment)
{
    MarkEditBagUse(plan->bag, plan->item);
    CommitEditPaymentSlots(plan);
    SetMoney(&gSaveBlock1Ptr->money, payment->money_after);
}
struct EditRecipePlan {
    struct EditPaymentPlan groups[RHH_MON_EDIT_MAX_COST_ITEMS];
    u32 count, bag, money, lastItem;
};
static void FreeEditRecipe(struct EditRecipePlan *plan)
{
    for (u32 i = 0; i < plan->count; ++i) free(plan->groups[i].slots);
}
static int PrepareEditRecipe(const struct RhhMonEditRecipe *recipe, struct RhhMonEditRecipePayment *out,
                             struct EditRecipePlan *plan, const char **reason)
{
    if (!recipe || recipe->struct_size < sizeof(*recipe) || recipe->reserved
        || recipe->item_count > RHH_MON_EDIT_MAX_COST_ITEMS || !gSaveBlock1Ptr || !gSaveBlock2Ptr)
    { *reason = "invalid edit recipe"; return RHH_EDIT_INVALID; }
    *out = (struct RhhMonEditRecipePayment){.struct_size = sizeof(*out),
        .item_count = recipe->item_count, .money = recipe->money};
    plan->money = out->money_before = GetMoney(&gSaveBlock1Ptr->money);
    if (recipe->money > plan->money)
    { *reason = "not enough money"; return RHH_EDIT_DENIED; }
    out->money_after = plan->money - recipe->money;
    if (recipe->item_count) plan->bag = out->bag = EditPaymentBag();
    u32 ids[RHH_MON_EDIT_MAX_COST_ITEMS] = {0};
    for (u32 line = 0; line < recipe->item_count; ++line)
    {
        const struct RhhMonEditRecipeItem *cost = &recipe->items[line];
        struct RhhMonEditRecipeLine *receipt = &out->items[line]; u32 item;
        if (cost->reserved || !cost->item || !cost->quantity
            || RhhContentResolve(RHH_ITEM, cost->item, &item) || !item || item >= ITEMS_COUNT)
        { *reason = "invalid recipe item"; return RHH_EDIT_INVALID; }
        for (u32 i = 0; i < line; ++i)
            if (ids[i] == item) { *reason = "duplicate recipe item"; return RHH_EDIT_INVALID; }
        ids[line] = item; plan->lastItem = item;
        *receipt = (struct RhhMonEditRecipeLine){.item = cost->item, .quantity = cost->quantity};
        u32 itemPocket = GetItemPocket(item);
        if (itemPocket >= POCKETS_COUNT)
        { *reason = "item cannot be held in the bag"; return RHH_EDIT_INVALID; }
        u32 pocket = plan->bag == RHH_EDIT_BAG_NORMAL ? itemPocket : 0;
        u32 group = 0;
        while (group < plan->count && plan->groups[group].pocket != pocket) ++group;
        if (group == plan->count)
        {
            struct EditPaymentPlan *p = &plan->groups[plan->count++];
            p->bag = plan->bag; p->pocket = pocket; p->money = plan->money;
            if (p->bag == RHH_EDIT_BAG_PYRAMID)
            {
                p->levelMode = gSaveBlock2Ptr->frontier.lvlMode;
                if (p->levelMode >= FRONTIER_LVL_MODE_COUNT)
                { *reason = "invalid Pyramid bag"; return RHH_EDIT_UNAVAILABLE; }
                p->capacity = PYRAMID_BAG_ITEMS_COUNT;
            }
            else
            {
                struct BagPocket *bag = &gBagPockets[pocket];
                if (!bag->itemSlots || bag->id != pocket || !bag->capacity)
                { *reason = "bag is unavailable"; return RHH_EDIT_UNAVAILABLE; }
                p->storage = bag->itemSlots; p->capacity = bag->capacity;
            }
            p->slots = calloc(p->capacity * 2, sizeof(*p->slots));
            if (!p->slots) { *reason = "could not stage payment"; return RHH_EDIT_UNAVAILABLE; }
            p->after = p->slots + p->capacity;
            for (u32 i = 0; i < p->capacity; ++i) p->slots[i] = p->after[i] = EditPaymentSlot(p, i);
        }
        struct EditPaymentPlan *p = &plan->groups[group];
        u32 remaining = cost->quantity, touchedTotal = 0;
        for (u32 i = 0; i < p->capacity; ++i)
        {
            struct ItemSlot *entry = &p->after[i];
            if (entry->itemId != item) continue;
            receipt->before += entry->quantity;
            if (remaining)
            {
                u32 take = min(remaining, entry->quantity);
                touchedTotal += entry->quantity; remaining -= take; entry->quantity -= take;
                if (!entry->quantity) entry->itemId = ITEM_NONE;
            }
        }
        if (remaining) { *reason = "not enough items"; return RHH_EDIT_DENIED; }
        receipt->after = receipt->before - cost->quantity;
        if (p->bag == RHH_EDIT_BAG_NORMAL && touchedTotal == cost->quantity) p->compact = TRUE;
    }
    for (u32 group = 0; group < plan->count; ++group)
    {
        struct EditPaymentPlan *p = &plan->groups[group];
        if (!p->compact) continue;
        u32 next = 0;
        for (u32 i = 0; i < p->capacity; ++i)
            if (p->after[i].itemId != ITEM_NONE) p->after[next++] = p->after[i];
        while (next < p->capacity) p->after[next++] = (struct ItemSlot){ITEM_NONE, 0};
    }
    return RHH_EDIT_OK;
}
static bool32 EditRecipeUnchanged(const struct EditRecipePlan *plan)
{
    if (!gSaveBlock1Ptr || !gSaveBlock2Ptr || GetMoney(&gSaveBlock1Ptr->money) != plan->money) return FALSE;
    for (u32 i = 0; i < plan->count; ++i)
        if (!EditPaymentUnchanged(&plan->groups[i])) return FALSE;
    return TRUE;
}
static void CommitEditRecipe(const struct EditRecipePlan *plan, const struct RhhMonEditRecipePayment *payment)
{
    MarkEditBagUse(plan->bag, plan->lastItem);
    for (u32 i = 0; i < plan->count; ++i) CommitEditPaymentSlots(&plan->groups[i]);
    SetMoney(&gSaveBlock1Ptr->money, payment->money_after);
}
static int EditResult(struct RhhMonEditResult *out, int status, const char *reason)
{
    out->status = status;
    if (reason) snprintf(out->reason, sizeof(out->reason), "%s", reason);
    return status;
}
int RhhMonSnapshotsEqual(const struct RhhOwnedMonSnapshot *a, const struct RhhOwnedMonSnapshot *b)
{
    return a->location == b->location && a->box == b->box && a->slot == b->slot
        && a->identity == b->identity && a->species == b->species && a->level == b->level
        && a->nature == b->nature && a->stat_nature == b->stat_nature && a->is_egg == b->is_egg
        && a->max_iv == b->max_iv && a->max_stat_ev == b->max_stat_ev && a->max_total_ev == b->max_total_ev
        && !memcmp(a->ivs, b->ivs, sizeof(a->ivs)) && !memcmp(a->evs, b->evs, sizeof(a->evs));
}
struct EditPreviewGuard {
    const struct RhhMonEditResult *edit;
    const struct RhhMonEditPayment *payment;
    const struct RhhMonEditRecipePayment *recipe;
    const struct RhhTrueNatureEditResult *trueNature;
    const struct RhhTeraEditResult *tera;
};
struct HyperEditOptions { u32 before, after; struct RhhHyperTrainingEditContext context; };
struct TrueNatureEditOptions { u64 run; u32 target, policy, activates, wasActive; struct RhhTrueNatureEditContext context; };
struct TeraEditOptions { u32 target; struct RhhOwnedTeraView expected; struct RhhTeraEditContext context; };
static bool32 TeraViewMatches(const struct RhhOwnedTeraView *a, const struct RhhOwnedTeraView *b)
{
    return a && b && a->struct_size >= sizeof(*a) && !a->reserved
        && a->source == b->source && a->flags == b->flags && a->stored_ordinal == b->stored_ordinal
        && a->resolved_ordinal == b->resolved_ordinal && a->stored_type == b->stored_type
        && a->resolved_type == b->resolved_type && RhhMonSnapshotsEqual(&a->mon, &b->mon);
}
static bool32 EditPreviewMatches(const struct RhhMonEditResult *expected, const struct RhhMonEditResult *actual)
{
    return expected && expected->struct_size >= sizeof(*expected) && expected->status == RHH_EDIT_OK
        && expected->is_preview == 1 && expected->before.struct_size >= sizeof(expected->before)
        && expected->after.struct_size >= sizeof(expected->after) && expected->fields == actual->fields
        && RhhMonSnapshotsEqual(&expected->before, &actual->before) && RhhMonSnapshotsEqual(&expected->after, &actual->after)
        && expected->current_hp == actual->current_hp && !memcmp(expected->stats, actual->stats, sizeof(actual->stats));
}
static bool32 EditPaymentMatches(const struct RhhMonEditPayment *a, const struct RhhMonEditPayment *b)
{
    return a && b && a->struct_size >= sizeof(*a) && a->bag == b->bag && a->item == b->item
        && a->item_before == b->item_before && a->item_after == b->item_after && a->item_quantity == b->item_quantity
        && a->money == b->money && a->money_before == b->money_before && a->money_after == b->money_after;
}
static bool32 EditRecipeMatches(const struct RhhMonEditRecipePayment *a, const struct RhhMonEditRecipePayment *b)
{
    if (!a || !b || a->struct_size < sizeof(*a) || a->bag != b->bag || a->item_count != b->item_count
        || a->item_count > RHH_MON_EDIT_MAX_COST_ITEMS || a->money != b->money
        || a->money_before != b->money_before || a->money_after != b->money_after) return FALSE;
    for (u32 i = 0; i < a->item_count; ++i)
        if (a->items[i].item != b->items[i].item || a->items[i].quantity != b->items[i].quantity
            || a->items[i].before != b->items[i].before || a->items[i].after != b->items[i].after) return FALSE;
    return TRUE;
}
static int EditMon(const struct RhhMonEditRequest *request, struct RhhMonEditResult *out, int preview,
                   const struct RhhMonEditCost *cost, struct RhhMonEditPayment *payment,
                   const struct RhhMonEditRecipe *recipe, struct RhhMonEditRecipePayment *recipePayment,
                   const struct EditPreviewGuard *guard, struct HyperEditOptions *hyper,
                   struct TrueNatureEditOptions *trueNature, struct TeraEditOptions *tera)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_EDIT_INVALID;
    struct RhhMonEditResult result = {.struct_size = sizeof(result), .is_preview = preview};
    struct EditPaymentPlan paymentPlan = {0};
    struct EditRecipePlan recipePlan = {0};
    struct RhhMonEditPayment compatiblePayment = {.struct_size = sizeof(compatiblePayment)};
    struct RhhMonEditRecipePayment compatibleRecipe = {.struct_size = sizeof(compatibleRecipe)};
    const struct RhhMonEditPayment *callbackPayment = NULL;
    const struct RhhMonEditRecipePayment *callbackRecipe = NULL;
    int status = RHH_EDIT_INVALID;
    if (!request || request->struct_size < sizeof(*request) || request->expected.struct_size < sizeof(request->expected)
        || (!request->fields && !hyper && !trueNature && !tera) || request->fields & ~(RHH_EDIT_IVS | RHH_EDIT_EVS | RHH_EDIT_STAT_NATURE)
        || request->reserved)
    { EditResult(&result, status, "invalid edit request"); goto done; }
    if (sEditing || !RhhModsCommandsAllowed() || !RhhModsHasActiveRun() || gMain.inBattle)
    { status = RHH_EDIT_UNAVAILABLE; EditResult(&result, status, "editing is unavailable"); goto done; }
    if ((trueNature || tera) && (!OwnedStorageAvailable() || NativeLinkSessionOpen()
        || gReceivedRemoteLinkPlayers || IsLinkConnectionEstablished()))
    { status = RHH_EDIT_UNAVAILABLE; EditResult(&result, status, "editing is unavailable during link or storage activity"); goto done; }
    sEditing = 1;
    rng_value_t editRng = gRngValue, editRng2 = gRng2Value;
    u64 editRun = RhhModsRunId();
    struct RhhOwnedMonSnapshot before = {.struct_size = sizeof(before)}, unique = {.struct_size = sizeof(unique)};
    struct Pokemon *party = NULL, staged = {0}, originalParty;
    struct BoxPokemon *box = NULL, originalBox;
    if (request->expected.location == RHH_MON_PARTY)
    {
        if (!PartyAt(request->expected.slot, &before)) party = &gParties[B_TRAINER_PLAYER][request->expected.slot];
    }
    else if (request->expected.location == RHH_MON_BOX)
    {
        if (!BoxAt(request->expected.box, request->expected.slot, &before))
            box = &gPokemonStoragePtr->boxes[request->expected.box][request->expected.slot];
    }
    if ((!party && !box) || before.is_egg || !RhhMonSnapshotsEqual(&before, &request->expected)
        || !UniqueOwnedIdentity(before.identity) || Find(before.identity, &unique) || !RhhMonSnapshotsEqual(&unique, &before))
    { status = RHH_EDIT_STALE; EditResult(&result, status, "Pokémon is unavailable, changed, or ambiguous"); goto unlock; }
    if (trueNature)
    {
        trueNature->run = RhhModsRunId();
        trueNature->wasActive = ModsNatureFormatEnabled();
        trueNature->activates = trueNature->target != before.nature && !trueNature->wasActive;
        if (trueNature->activates && !ModsNatureFormatCanEnable())
        { status = RHH_EDIT_UNAVAILABLE; EditResult(&result, status, "nature format cannot be safely enabled"); goto unlock; }
    }
    if (hyper)
    {
        struct BoxPokemon copy = party ? party->box : *box;
        if (HyperTrainingMask(&copy) != hyper->before)
        { status = RHH_EDIT_STALE; EditResult(&result, status, "Hyper Training changed"); goto unlock; }
    }
    if (tera)
    {
        struct RhhOwnedTeraView current = {.struct_size = sizeof(current)};
        if (MakeOwnedTeraView(party ? &party->box : box, &before, &current) || !TeraViewMatches(&tera->expected, &current))
        { status = RHH_EDIT_STALE; EditResult(&result, status, "Tera type changed"); goto unlock; }
        tera->context = (struct RhhTeraEditContext){.struct_size = sizeof(tera->context), .is_preview = preview,
            .type = tera->target ? OwnedTeraTypeKey(tera->target) : 0, .before = current};
    }
    struct RhhEvLimits limits = {.total = MAX_TOTAL_EVS};
    for (u32 i = 0; i < NUM_STATS; ++i) limits.per_stat[i] = MAX_PER_STAT_EVS;
    if ((request->fields & RHH_EDIT_EVS) && CalculateEvLimits(&before, RHH_EV_LIMIT_EDIT, 0, MAX_TOTAL_EVS, &limits))
    { status = RHH_EDIT_UNAVAILABLE; EditResult(&result, status, "EV limits unavailable"); goto unlock; }
    unsigned total = 0, changed = 0;
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i)
    {
        if ((request->fields & RHH_EDIT_IVS) && request->ivs[i] > MAX_PER_STAT_IVS)
        { EditResult(&result, status, "IV exceeds the engine limit"); goto unlock; }
        if ((request->fields & RHH_EDIT_EVS) && request->evs[i] > limits.per_stat[i])
        { EditResult(&result, status, "EV exceeds the per-stat limit"); goto unlock; }
        total += request->evs[i];
        if ((request->fields & RHH_EDIT_IVS) && request->ivs[i] != before.ivs[i]) changed |= RHH_EDIT_IVS;
        if ((request->fields & RHH_EDIT_EVS) && request->evs[i] != before.evs[i]) changed |= RHH_EDIT_EVS;
    }
    if ((request->fields & RHH_EDIT_EVS) && total > limits.total)
    { EditResult(&result, status, "EV total exceeds the engine limit"); goto unlock; }
    u32 newStatNature = before.stat_nature;
    if (request->fields & RHH_EDIT_STAT_NATURE)
    {
        if (request->stat_nature >= NUM_NATURES)
        { EditResult(&result, status, "invalid stat nature"); goto unlock; }
        newStatNature = request->stat_nature;
    }
    else if (trueNature && (trueNature->policy == RHH_TRUE_NATURE_FOLLOW
        || (trueNature->policy == RHH_TRUE_NATURE_FOLLOW_UNMINTED && before.nature == before.stat_nature)))
        newStatNature = trueNature->target;
    if (newStatNature != before.stat_nature) changed |= RHH_EDIT_STAT_NATURE;
    if (trueNature && trueNature->target != before.nature) changed |= RHH_EDIT_TRUE_NATURE;
    if (hyper && hyper->before != hyper->after) changed |= RHH_EDIT_HYPER_TRAINING;
    if (tera && tera->expected.stored_ordinal != tera->target) changed |= RHH_EDIT_TERA;
    if (!changed) { EditResult(&result, status, "edit has no changes"); goto unlock; }
    if (party)
    {
        originalParty = *party;
        if (!originalParty.maxHP || originalParty.hp > originalParty.maxHP)
        { status = RHH_EDIT_INVALID; EditResult(&result, status, "invalid party HP state"); goto unlock; }
        staged = originalParty;
    }
    else
    {
        originalBox = *box; staged.box = originalBox;
        u32 hpLost = GetBoxMonData(&staged.box, MON_DATA_HP_LOST);
        CalculateMonStatsPreview(&staged);
        if (hpLost > staged.maxHP) hpLost = staged.maxHP;
        staged.hp = staged.maxHP - hpLost;
        SetBoxMonData(&staged.box, MON_DATA_HP_LOST, &hpLost);
    }
    if (ModsMonIsDead(&staged) && staged.hp)
    { status = RHH_EDIT_DENIED; EditResult(&result, status, "a permanently fainted Pokémon cannot have HP restored"); goto unlock; }
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i)
    {
        if (request->fields & RHH_EDIT_IVS) { u8 value = request->ivs[i]; SetBoxMonData(&staged.box, MON_DATA_HP_IV + i, &value); }
        if (request->fields & RHH_EDIT_EVS) { u8 value = request->evs[i]; SetBoxMonData(&staged.box, MON_DATA_HP_EV + i, &value); }
        if (hyper) { u8 value = !!(hyper->after & (1u << i)); SetBoxMonData(&staged.box, MON_DATA_HYPER_TRAINED_HP + i, &value); }
    }
    if (changed & RHH_EDIT_STAT_NATURE)
    { u8 value = newStatNature; SetBoxMonData(&staged.box, MON_DATA_HIDDEN_NATURE, &value); }
    if (trueNature && (changed & RHH_EDIT_TRUE_NATURE) && NativeNatureStorageWrite(&staged.box, trueNature->target))
    { status = RHH_EDIT_INVALID; EditResult(&result, status, "could not stage true nature"); goto unlock; }
    if (tera) { u8 value = tera->target; SetBoxMonData(&staged.box, MON_DATA_TERA_TYPE, &value); }
    CalculateMonStatsPreview(&staged);
    result.before = before; result.after.struct_size = sizeof(result.after);
    if (Snapshot(&staged.box, before.location, before.box, before.slot, &result.after))
    { EditResult(&result, status, "could not stage Pokémon edit"); goto unlock; }
    if (trueNature) result.after.nature = trueNature->target;
    if (tera && MakeOwnedTeraView(&staged.box, &result.after, &tera->context.after))
    { EditResult(&result, status, "could not stage Tera type"); goto unlock; }
    result.fields = changed; result.current_hp = staged.hp;
    result.stats[RHH_STAT_HP] = staged.maxHP; result.stats[RHH_STAT_ATTACK] = staged.attack;
    result.stats[RHH_STAT_DEFENSE] = staged.defense; result.stats[RHH_STAT_SPEED] = staged.speed;
    result.stats[RHH_STAT_SP_ATTACK] = staged.spAttack; result.stats[RHH_STAT_SP_DEFENSE] = staged.spDefense;
    if (cost)
    {
        const char *reason = NULL;
        status = PrepareEditPayment(cost, payment, &paymentPlan, &reason);
        if (status != RHH_EDIT_OK) { EditResult(&result, status, reason); goto unlock; }
        compatibleRecipe.bag = payment->bag; compatibleRecipe.item_count = payment->item ? 1 : 0;
        compatibleRecipe.money = payment->money; compatibleRecipe.money_before = payment->money_before;
        compatibleRecipe.money_after = payment->money_after;
        if (payment->item)
            compatibleRecipe.items[0] = (struct RhhMonEditRecipeLine){.item = payment->item,
                .quantity = payment->item_quantity, .before = payment->item_before, .after = payment->item_after};
        callbackPayment = payment; callbackRecipe = &compatibleRecipe;
    }
    else if (recipe)
    {
        const char *reason = NULL;
        status = PrepareEditRecipe(recipe, recipePayment, &recipePlan, &reason);
        if (status != RHH_EDIT_OK) { EditResult(&result, status, reason); goto unlock; }
        callbackRecipe = recipePayment;
        if (recipePayment->item_count <= 1)
        {
            compatiblePayment.bag = recipePayment->bag; compatiblePayment.money = recipePayment->money;
            compatiblePayment.money_before = recipePayment->money_before; compatiblePayment.money_after = recipePayment->money_after;
            if (recipePayment->item_count)
            {
                const struct RhhMonEditRecipeLine *line = &recipePayment->items[0];
                compatiblePayment.item = line->item; compatiblePayment.item_quantity = line->quantity;
                compatiblePayment.item_before = line->before; compatiblePayment.item_after = line->after;
            }
            callbackPayment = &compatiblePayment;
        }
    }
    if (guard && (!EditPreviewMatches(guard->edit, &result)
        || (cost && !EditPaymentMatches(guard->payment, payment))
        || (recipe && !EditRecipeMatches(guard->recipe, recipePayment))
        || (trueNature && (!guard->trueNature || guard->trueNature->activates_format != trueNature->activates))
        || (tera && (!guard->tera || !TeraViewMatches(&guard->tera->before, &tera->context.before)
            || !TeraViewMatches(&guard->tera->after, &tera->context.after)))))
    { status = RHH_EDIT_STALE; EditResult(&result, status, "preview or payment changed"); goto unlock; }
    /* Synchronize previously acquired balls before constraints on a paid
     * commit, including when the payment consumes the final ball. */
    if ((cost || recipe) && !preview) ModsSyncInventory();
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_EDIT_MON,
        .run_seed = RhhModsRunSeed(), .event_key = before.species, .subject = before.identity,
        .args = {changed, preview, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    if (hyper) hyper->context = (struct RhhHyperTrainingEditContext){.struct_size = sizeof(hyper->context),
        .before_mask = hyper->before, .after_mask = hyper->after, .is_preview = preview};
    if (trueNature) trueNature->context = (struct RhhTrueNatureEditContext){.struct_size = sizeof(trueNature->context),
        .stat_policy = trueNature->policy, .activates_format = trueNature->activates, .is_preview = preview};
    sEditContext = &result; sEditPaymentContext = callbackPayment; sEditRecipeContext = callbackRecipe;
    sHyperEditContext = hyper ? &hyper->context : NULL;
    sTrueNatureEditContext = trueNature ? &trueNature->context : NULL;
    sTeraEditContext = tera ? &tera->context : NULL;
    RhhModsDispatch(&event, &decision); sEditContext = NULL; sEditPaymentContext = NULL; sEditRecipeContext = NULL; sHyperEditContext = NULL; sTrueNatureEditContext = NULL; sTeraEditContext = NULL;
    if (!decision.allowed)
    { status = RHH_EDIT_DENIED; EditResult(&result, status, decision.reason[0] ? decision.reason : "edit denied"); goto unlock; }
    if ((party && memcmp(party, &originalParty, sizeof(originalParty)))
        || (box && memcmp(box, &originalBox, sizeof(originalBox)))
        || (cost && !EditPaymentUnchanged(&paymentPlan)) || (recipe && !EditRecipeUnchanged(&recipePlan))
        || (tera && (!RhhModsHasActiveRun() || RhhModsRunId() != editRun || !OwnedStorageAvailable()
            || NativeLinkSessionOpen() || gReceivedRemoteLinkPlayers || IsLinkConnectionEstablished()
            || !UniqueOwnedIdentity(before.identity) || Find(before.identity, &unique) || !RhhMonSnapshotsEqual(&unique, &before)))
        || (trueNature && (!OwnedStorageAvailable() || NativeLinkSessionOpen() || gReceivedRemoteLinkPlayers
            || IsLinkConnectionEstablished() || !RhhModsHasActiveRun() || RhhModsRunId() != trueNature->run
            || ModsNatureFormatEnabled() != trueNature->wasActive
            || (trueNature->activates && !ModsNatureFormatCanEnable()))))
    { status = RHH_EDIT_STALE; EditResult(&result, status, "Pokémon or payment changed during validation"); goto unlock; }
    if (memcmp(&editRng, &gRngValue, sizeof(editRng)) || memcmp(&editRng2, &gRng2Value, sizeof(editRng2)))
    { status = RHH_EDIT_DENIED; EditResult(&result, status, "edit validation consumed gameplay RNG"); goto unlock; }
    status = RHH_EDIT_OK; result.status = status;
    if (!preview)
    {
        if (trueNature && trueNature->activates && ModsEnableNatureFormat())
        { status = RHH_EDIT_STALE; EditResult(&result, status, "nature format changed during validation"); goto unlock; }
        if (cost)
        {
            CommitEditPayment(&paymentPlan, payment);
        }
        else if (recipe) CommitEditRecipe(&recipePlan, recipePayment);
        if (party) *party = staged; else *box = staged.box;
        if (cost || recipe) ModsSyncInventory();
        event.kind = RHH_MON_EDITED;
        sEditContext = &result; sEditPaymentContext = callbackPayment; sEditRecipeContext = callbackRecipe;
        sHyperEditContext = hyper ? &hyper->context : NULL;
        sTrueNatureEditContext = trueNature ? &trueNature->context : NULL;
        sTeraEditContext = tera ? &tera->context : NULL;
        RhhModsDispatch(&event, &decision); sEditContext = NULL; sEditPaymentContext = NULL; sEditRecipeContext = NULL; sHyperEditContext = NULL; sTrueNatureEditContext = NULL; sTeraEditContext = NULL;
    }
unlock:
    gRngValue = editRng; gRng2Value = editRng2;
    free(paymentPlan.slots);
    FreeEditRecipe(&recipePlan);
    sEditing = 0;
done:
    *out = result;
    return status;
}
static int PreviewEdit(const struct RhhMonEditRequest *request, struct RhhMonEditResult *out)
{ return EditMon(request, out, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL); }
static int CommitEdit(const struct RhhMonEditRequest *request, struct RhhMonEditResult *out)
{ return EditMon(request, out, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL); }
static int CommitPreviewedEdit(const struct RhhMonEditRequest *request, const struct RhhMonEditResult *expected,
                               struct RhhMonEditResult *out)
{
    const struct EditPreviewGuard guard = {.edit = expected};
    return EditMon(request, out, 0, NULL, NULL, NULL, NULL, &guard, NULL, NULL, NULL);
}
static int PaidEdit(const struct RhhMonEditPaidRequest *request, struct RhhMonEditPaidResult *out, int preview,
                    const struct RhhMonEditPaidResult *expected)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_EDIT_INVALID;
    struct RhhMonEditPaidResult result = {.struct_size = sizeof(result), .edit.struct_size = sizeof(result.edit),
        .payment.struct_size = sizeof(result.payment)};
    if (!request || request->struct_size < sizeof(*request) || request->reserved)
        result.status = EditResult(&result.edit, RHH_EDIT_INVALID, "invalid paid edit request");
    else {
        const struct EditPreviewGuard guard = {.edit = expected ? &expected->edit : NULL,
            .payment = expected ? &expected->payment : NULL};
        result.status = EditMon(&request->edit, &result.edit, preview, &request->cost, &result.payment, NULL, NULL,
                                expected ? &guard : NULL, NULL, NULL, NULL);
    }
    *out = result; return result.status;
}
static int PreviewPaidEdit(const struct RhhMonEditPaidRequest *request, struct RhhMonEditPaidResult *out)
{ return PaidEdit(request, out, 1, NULL); }
static int CommitPaidEdit(const struct RhhMonEditPaidRequest *request, struct RhhMonEditPaidResult *out)
{ return PaidEdit(request, out, 0, NULL); }
static int CommitPaidPreviewedEdit(const struct RhhMonEditPaidRequest *request, const struct RhhMonEditPaidResult *expected,
                                   struct RhhMonEditPaidResult *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->status != RHH_EDIT_OK)
    {
        if (out && out->struct_size >= sizeof(*out)) {
            struct RhhMonEditPaidResult result = {.struct_size = sizeof(result), .status = RHH_EDIT_INVALID,
                .edit.struct_size = sizeof(result.edit), .payment.struct_size = sizeof(result.payment)};
            EditResult(&result.edit, RHH_EDIT_INVALID, "invalid edit preview"); *out = result;
        }
        return RHH_EDIT_INVALID;
    }
    return PaidEdit(request, out, 0, expected);
}
static int RecipeEdit(const struct RhhMonEditRecipeRequest *request, struct RhhMonEditRecipeResult *out, int preview,
                      const struct RhhMonEditRecipeResult *expected)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_EDIT_INVALID;
    struct RhhMonEditRecipeResult result = {.struct_size = sizeof(result), .edit.struct_size = sizeof(result.edit),
        .payment.struct_size = sizeof(result.payment)};
    if (!request || request->struct_size < sizeof(*request) || request->reserved)
        result.status = EditResult(&result.edit, RHH_EDIT_INVALID, "invalid recipe edit request");
    else {
        const struct EditPreviewGuard guard = {.edit = expected ? &expected->edit : NULL,
            .recipe = expected ? &expected->payment : NULL};
        result.status = EditMon(&request->edit, &result.edit, preview, NULL, NULL, &request->recipe, &result.payment,
                                expected ? &guard : NULL, NULL, NULL, NULL);
    }
    *out = result; return result.status;
}
static int PreviewRecipeEdit(const struct RhhMonEditRecipeRequest *request, struct RhhMonEditRecipeResult *out)
{ return RecipeEdit(request, out, 1, NULL); }
static int CommitRecipeEdit(const struct RhhMonEditRecipeRequest *request, struct RhhMonEditRecipeResult *out)
{ return RecipeEdit(request, out, 0, NULL); }
static int CommitRecipePreviewedEdit(const struct RhhMonEditRecipeRequest *request, const struct RhhMonEditRecipeResult *expected,
                                     struct RhhMonEditRecipeResult *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->status != RHH_EDIT_OK)
    {
        if (out && out->struct_size >= sizeof(*out)) {
            struct RhhMonEditRecipeResult result = {.struct_size = sizeof(result), .status = RHH_EDIT_INVALID,
                .edit.struct_size = sizeof(result.edit), .payment.struct_size = sizeof(result.payment)};
            EditResult(&result.edit, RHH_EDIT_INVALID, "invalid edit preview"); *out = result;
        }
        return RHH_EDIT_INVALID;
    }
    return RecipeEdit(request, out, 0, expected);
}
static int HyperEdit(const struct RhhHyperTrainingEditRequest *request, const struct RhhHyperTrainingEditResult *expected,
                     struct RhhHyperTrainingEditResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_EDIT_INVALID;
    struct RhhHyperTrainingEditResult result = {.struct_size = sizeof(result), .status = RHH_EDIT_INVALID,
        .edit.struct_size = sizeof(result.edit), .payment.struct_size = sizeof(result.payment)};
    const u32 validMask = (1u << RHH_STAT_COUNT) - 1u;
    if (!request || request->struct_size < sizeof(*request) || request->has_recipe > 1
        || ((request->before_mask | request->after_mask) & ~validMask)
        || (!preview && (!expected || expected->struct_size < sizeof(*expected) || expected->status != RHH_EDIT_OK)))
        EditResult(&result.edit, RHH_EDIT_INVALID, "invalid Hyper Training request or preview");
    else if (!preview && (expected->before_mask != request->before_mask || expected->after_mask != request->after_mask
        || expected->has_recipe != request->has_recipe))
        result.status = EditResult(&result.edit, RHH_EDIT_STALE, "Hyper Training preview changed");
    else
    {
        struct HyperEditOptions hyper = {.before = request->before_mask, .after = request->after_mask};
        struct EditPreviewGuard guard = {.edit = expected ? &expected->edit : NULL,
            .recipe = expected && request->has_recipe ? &expected->payment : NULL};
        result.before_mask = hyper.before; result.after_mask = hyper.after; result.has_recipe = request->has_recipe;
        result.status = EditMon(&request->edit, &result.edit, preview, NULL, NULL,
            request->has_recipe ? &request->recipe : NULL, request->has_recipe ? &result.payment : NULL,
            preview ? NULL : &guard, &hyper, NULL, NULL);
    }
    *out = result; return result.status;
}
static int PreviewHyperEdit(const struct RhhHyperTrainingEditRequest *request, struct RhhHyperTrainingEditResult *out)
{ return HyperEdit(request, NULL, out, TRUE); }
static int CommitHyperEdit(const struct RhhHyperTrainingEditRequest *request, const struct RhhHyperTrainingEditResult *expected,
                           struct RhhHyperTrainingEditResult *out)
{ return HyperEdit(request, expected, out, FALSE); }
static int TrueNatureEdit(const struct RhhTrueNatureEditRequest *request, const struct RhhTrueNatureEditResult *expected,
                          struct RhhTrueNatureEditResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_EDIT_INVALID;
    struct RhhTrueNatureEditResult result = {.struct_size = sizeof(result), .status = RHH_EDIT_INVALID,
        .edit.struct_size = sizeof(result.edit), .payment.struct_size = sizeof(result.payment)};
    struct RhhNatureInfo info = {.struct_size = sizeof(info)};
    if (!request || request->struct_size < sizeof(*request) || request->reserved || request->has_recipe > 1
        || request->stat_policy > RHH_TRUE_NATURE_FOLLOW_UNMINTED || NatureFind(request->nature, &info)
        || (!preview && (!expected || expected->struct_size < sizeof(*expected) || expected->status != RHH_EDIT_OK)))
        EditResult(&result.edit, RHH_EDIT_INVALID, "invalid true nature request or preview");
    else if (!preview && (expected->nature != request->nature || expected->stat_policy != request->stat_policy
        || expected->has_recipe != request->has_recipe))
        result.status = EditResult(&result.edit, RHH_EDIT_STALE, "true nature preview changed");
    else
    {
        struct TrueNatureEditOptions nature = {.target = info.ordinal, .policy = request->stat_policy};
        struct EditPreviewGuard guard = {.edit = expected ? &expected->edit : NULL,
            .recipe = expected && request->has_recipe ? &expected->payment : NULL, .trueNature = expected};
        result.nature = request->nature; result.stat_policy = request->stat_policy; result.has_recipe = request->has_recipe;
        result.status = EditMon(&request->edit, &result.edit, preview, NULL, NULL,
            request->has_recipe ? &request->recipe : NULL, request->has_recipe ? &result.payment : NULL,
            preview ? NULL : &guard, NULL, &nature, NULL);
        result.activates_format = nature.activates;
    }
    *out = result; return result.status;
}
static int PreviewTrueNatureEdit(const struct RhhTrueNatureEditRequest *request, struct RhhTrueNatureEditResult *out)
{ return TrueNatureEdit(request, NULL, out, TRUE); }
static int CommitTrueNatureEdit(const struct RhhTrueNatureEditRequest *request, const struct RhhTrueNatureEditResult *expected,
                                struct RhhTrueNatureEditResult *out)
{ return TrueNatureEdit(request, expected, out, FALSE); }
static int NatureTrainingEdit(const struct RhhNatureTrainingEditRequest *request,
                              const struct RhhNatureTrainingEditResult *expected,
                              struct RhhNatureTrainingEditResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_EDIT_INVALID;
    struct RhhNatureTrainingEditResult result = {.struct_size = sizeof(result), .status = RHH_EDIT_INVALID,
        .nature.struct_size = sizeof(result.nature), .nature.status = RHH_EDIT_INVALID,
        .nature.edit.struct_size = sizeof(result.nature.edit), .nature.payment.struct_size = sizeof(result.nature.payment)};
    struct RhhNatureInfo info = {.struct_size = sizeof(info)};
    const u32 validMask = (1u << RHH_STAT_COUNT) - 1u;
    if (!request || request->struct_size < sizeof(*request) || request->reserved
        || ((request->before_mask | request->after_mask) & ~validMask)
        || request->nature.struct_size < sizeof(request->nature) || request->nature.reserved
        || request->nature.has_recipe > 1 || request->nature.stat_policy > RHH_TRUE_NATURE_FOLLOW_UNMINTED
        || NatureFind(request->nature.nature, &info)
        || (!preview && (!expected || expected->struct_size < sizeof(*expected) || expected->status != RHH_EDIT_OK
            || expected->nature.struct_size < sizeof(expected->nature) || expected->nature.status != RHH_EDIT_OK)))
        EditResult(&result.nature.edit, RHH_EDIT_INVALID, "invalid nature and training request or preview");
    else if (!preview && (expected->before_mask != request->before_mask || expected->after_mask != request->after_mask
        || expected->nature.nature != request->nature.nature || expected->nature.stat_policy != request->nature.stat_policy
        || expected->nature.has_recipe != request->nature.has_recipe))
        result.status = EditResult(&result.nature.edit, RHH_EDIT_STALE, "nature or training preview changed");
    else
    {
        struct HyperEditOptions hyper = {.before = request->before_mask, .after = request->after_mask};
        struct TrueNatureEditOptions nature = {.target = info.ordinal, .policy = request->nature.stat_policy};
        struct EditPreviewGuard guard = {.edit = expected ? &expected->nature.edit : NULL,
            .recipe = expected && request->nature.has_recipe ? &expected->nature.payment : NULL,
            .trueNature = expected ? &expected->nature : NULL};
        result.before_mask = hyper.before; result.after_mask = hyper.after;
        result.nature.nature = request->nature.nature; result.nature.stat_policy = request->nature.stat_policy;
        result.nature.has_recipe = request->nature.has_recipe;
        result.status = EditMon(&request->nature.edit, &result.nature.edit, preview, NULL, NULL,
            request->nature.has_recipe ? &request->nature.recipe : NULL,
            request->nature.has_recipe ? &result.nature.payment : NULL, preview ? NULL : &guard, &hyper, &nature, NULL);
        result.nature.activates_format = nature.activates;
    }
    result.nature.status = result.status;
    *out = result; return result.status;
}
static int PreviewNatureTrainingEdit(const struct RhhNatureTrainingEditRequest *request, struct RhhNatureTrainingEditResult *out)
{ return NatureTrainingEdit(request, NULL, out, TRUE); }
static int CommitNatureTrainingEdit(const struct RhhNatureTrainingEditRequest *request,
                                    const struct RhhNatureTrainingEditResult *expected, struct RhhNatureTrainingEditResult *out)
{ return NatureTrainingEdit(request, expected, out, FALSE); }
static int TeraEdit(const struct RhhTeraEditRequest *request, const struct RhhTeraEditResult *expected,
                    struct RhhTeraEditResult *out, bool32 preview)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_EDIT_INVALID;
    struct RhhTeraEditResult result = {.struct_size = sizeof(result), .status = RHH_EDIT_INVALID,
        .edit.struct_size = sizeof(result.edit), .payment.struct_size = sizeof(result.payment)};
    u32 target = TYPE_NONE;
    if (!request || request->struct_size < sizeof(*request) || request->reserved[0] || request->reserved[1]
        || request->has_recipe > 1 || request->expected.struct_size < sizeof(request->expected)
        || request->expected.mon.struct_size < sizeof(request->expected.mon)
        || request->edit.struct_size < sizeof(request->edit) || request->edit.expected.struct_size < sizeof(request->edit.expected)
        || !RhhMonSnapshotsEqual(&request->expected.mon, &request->edit.expected)
        || (request->type && (RhhContentResolve(RHH_TYPE, request->type, &target)
            || OwnedTeraTypeKey(target) != request->type))
        || (!preview && (!expected || expected->struct_size < sizeof(*expected) || expected->status != RHH_EDIT_OK)))
        EditResult(&result.edit, RHH_EDIT_INVALID, "invalid Tera request or preview");
    else if (!preview && (expected->type != request->type || expected->has_recipe != request->has_recipe))
        result.status = EditResult(&result.edit, RHH_EDIT_STALE, "Tera preview changed");
    else
    {
        struct RhhTeraEditRequest copy = *request;
        struct TeraEditOptions tera = {.target = target, .expected = copy.expected};
        struct EditPreviewGuard guard = {.edit = expected ? &expected->edit : NULL,
            .recipe = expected && copy.has_recipe ? &expected->payment : NULL, .tera = expected};
        result.type = copy.type; result.has_recipe = copy.has_recipe;
        result.status = EditMon(&copy.edit, &result.edit, preview, NULL, NULL,
            copy.has_recipe ? &copy.recipe : NULL, copy.has_recipe ? &result.payment : NULL,
            preview ? NULL : &guard, NULL, NULL, &tera);
        result.before = tera.context.before; result.after = tera.context.after;
    }
    *out = result; return result.status;
}
static int PreviewTeraEdit(const struct RhhTeraEditRequest *request, struct RhhTeraEditResult *out)
{ return TeraEdit(request, NULL, out, TRUE); }
static int CommitTeraEdit(const struct RhhTeraEditRequest *request, const struct RhhTeraEditResult *expected,
                          struct RhhTeraEditResult *out)
{ return TeraEdit(request, expected, out, FALSE); }
static const struct RhhMonEditService sEditService = {
    {sizeof(sEditService), RHH_SERVICE_VERSION(1, 8)}, PreviewEdit, CommitEdit, EditContext,
    PreviewPaidEdit, CommitPaidEdit, EditPaymentContext, PreviewRecipeEdit, CommitRecipeEdit, EditRecipeContext,
    CommitPreviewedEdit, CommitPaidPreviewedEdit, CommitRecipePreviewedEdit,
    PreviewHyperEdit, CommitHyperEdit, HyperEditContext, PreviewTrueNatureEdit, CommitTrueNatureEdit, TrueNatureEditContext,
    PreviewNatureTrainingEdit, CommitNatureTrainingEdit, PreviewTeraEdit, CommitTeraEdit, TeraEditContext
};
static RhhContentKey MonMoveKey(u32 move)
{
    return move >= RHH_RUNTIME_MOVE_FIRST && move < RHH_RUNTIME_MOVE_FIRST + RHH_RUNTIME_MOVE_COUNT
        ? RhhModsMoveKey(move) : RhhContentKeyForId(RHH_MOVE, move);
}
int ModsPartyMoves(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedMoveSet *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->location != RHH_MON_PARTY
        || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhOwnedMonSnapshot before = {.struct_size = sizeof(before)}, unique = {.struct_size = sizeof(unique)};
    if (PartyAt(expected->slot, &before) || !RhhMonSnapshotsEqual(&before, expected)
        || !UniqueOwnedIdentity(before.identity) || Find(before.identity, &unique) || !RhhMonSnapshotsEqual(&before, &unique)) return -1;
    struct Pokemon copy = gParties[B_TRAINER_PLAYER][before.slot];
    u32 bonuses = GetMonData(&copy, MON_DATA_PP_BONUSES);
    struct RhhOwnedMoveSet result = {.struct_size = sizeof(result)};
    for (u32 i = 0; i < MAX_MON_MOVES; ++i)
    {
        u32 move = GetMonData(&copy, MON_DATA_MOVE1 + i);
        if (!move) continue;
        result.moves[i] = MonMoveKey(move);
        if (!result.moves[i]) return -1;
        result.pp[i] = GetMonData(&copy, MON_DATA_PP1 + i);
        result.pp_ups[i] = (bonuses >> (i * 2)) & 3;
        if (GetMoveEffect(move) != EFFECT_PLACEHOLDER)
        {
            result.available_mask |= 1u << i;
            result.max_pp[i] = CalculatePPWithBonus(move, bonuses, i);
        }
    }
    *out = result; return 0;
}
static const struct RhhMoveTeachResult *sTeachContext;
int ModsTeachMoveContext(struct RhhMoveTeachResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sTeachContext
        || (event != RHH_CAN_TEACH_MOVE && event != RHH_MOVE_TAUGHT)) return -1;
    *out = *sTeachContext; return 0;
}
int ModsTeachMove(const struct RhhMoveTeachRequest *request, struct RhhMoveTeachResult *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_EDIT_INVALID;
    struct RhhMoveTeachResult result = {.struct_size = sizeof(result), .status = RHH_EDIT_INVALID};
    if (!request || request->struct_size < sizeof(*request) || request->expected.struct_size < sizeof(request->expected)
        || request->reserved[0] || request->reserved[1] || request->move_slot >= MAX_MON_MOVES
        || request->expected.location != RHH_MON_PARTY || !request->move) goto done;
    if (sEditing || !RhhModsCommandsAllowed() || !RhhModsHasActiveRun() || gMain.inBattle || NativeSaveTransactionActive())
    { result.status = RHH_EDIT_UNAVAILABLE; goto done; }
    const struct RhhModMove *definition = RhhModsFindCustomMove(request->move);
    if (!ModsCustomMoveAvailable(definition)) goto done;
    sEditing = 1;
    struct RhhOwnedMonSnapshot before = {.struct_size = sizeof(before)}, unique = {.struct_size = sizeof(unique)};
    if (PartyAt(request->expected.slot, &before) || before.is_egg || !RhhMonSnapshotsEqual(&before, &request->expected)
        || !UniqueOwnedIdentity(before.identity) || Find(before.identity, &unique) || !RhhMonSnapshotsEqual(&before, &unique))
    { result.status = RHH_EDIT_STALE; goto unlock; }
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][before.slot], original = *mon, copy = *mon;
    u32 oldMove = GetMonData(&copy, MON_DATA_MOVE1 + request->move_slot);
    RhhContentKey oldKey = oldMove ? MonMoveKey(oldMove) : 0;
    if ((oldMove && !oldKey) || oldKey != request->expected_move)
    { result.status = RHH_EDIT_STALE; goto unlock; }
    for (u32 i = 0; i < MAX_MON_MOVES; ++i)
    {
        u32 move = GetMonData(&copy, MON_DATA_MOVE1 + i);
        if (move && MonMoveKey(move) == request->move) goto unlock;
    }
    result.move_slot = request->move_slot; result.move = request->move;
    result.previous_move = oldKey; result.mon = before;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_TEACH_MOVE,
        .run_seed = RhhModsRunSeed(), .event_key = request->move, .subject = before.identity,
        .args = {before.slot, request->move_slot}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sTeachContext = &result; RhhModsDispatch(&event, &decision); sTeachContext = NULL;
    if (!decision.allowed)
    { result.status = RHH_EDIT_DENIED; memcpy(result.reason, decision.reason, sizeof(result.reason)); goto unlock; }
    if (memcmp(mon, &original, sizeof(original))) { result.status = RHH_EDIT_STALE; goto unlock; }
    u32 slot;
    if (RhhModsMoveSlot(request->move, 1, &slot)) { result.status = RHH_EDIT_UNAVAILABLE; goto unlock; }
    u8 bonuses = GetMonData(mon, MON_DATA_PP_BONUSES) & gPPUpClearMask[request->move_slot];
    SetMonData(mon, MON_DATA_PP_BONUSES, &bonuses);
    SetMonMoveSlot(mon, slot, request->move_slot);
    result.status = RHH_EDIT_OK;
    event.kind = RHH_MOVE_TAUGHT;
    sTeachContext = &result; RhhModsDispatch(&event, &decision); sTeachContext = NULL;
unlock:
    sEditing = 0;
done:
    *out = result; return result.status;
}
static int DaycareParentAt(uint32_t slot, struct RhhDaycareParent *out)
{
    if (!out || out->struct_size < sizeof(*out) || !gSaveBlock1Ptr || slot >= DAYCARE_MON_COUNT) return -1;
    struct DaycareMon copy = gSaveBlock1Ptr->daycare.mons[slot];
    struct RhhDaycareParent view = {.struct_size = sizeof(view), .steps = copy.steps};
    view.mon.struct_size = sizeof(view.mon);
    if (Snapshot(&copy.mon, RHH_MON_DAYCARE, 0, slot, &view.mon) || view.mon.is_egg) return -1;
    u32 species = GetBoxMonData(&copy.mon, MON_DATA_SPECIES);
    u32 personality = GetBoxMonData(&copy.mon, MON_DATA_PERSONALITY);
    u32 gender = GetGenderFromSpeciesAndPersonality(species, personality);
    view.gender = gender == MON_MALE ? RHH_GENDER_MALE
        : gender == MON_FEMALE ? RHH_GENDER_FEMALE : RHH_GENDER_GENDERLESS;
    view.egg_groups[0] = gSpeciesInfo[species].eggGroups[0];
    view.egg_groups[1] = gSpeciesInfo[species].eggGroups[1];
    u32 item = GetBoxMonData(&copy.mon, MON_DATA_HELD_ITEM);
    if (item && !(view.held_item = RhhContentKeyForId(RHH_ITEM, item))) return -1;
    for (unsigned i = 0; i < 4; ++i)
    {
        u32 move = GetBoxMonData(&copy.mon, MON_DATA_MOVE1 + i);
        if (move && !(view.moves[i] = MonMoveKey(move))) return -1;
    }
    *out = view;
    return 0;
}
static int DaycareCurrent(struct RhhDaycareSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out) || !gSaveBlock1Ptr) return -1;
    struct DayCare copy = gSaveBlock1Ptr->daycare;
    struct RhhDaycareSnapshot view = {.struct_size = sizeof(view), .egg_pending = FlagGet(FLAG_PENDING_DAYCARE_EGG)};
    for (unsigned i = 0; i < DAYCARE_MON_COUNT; ++i)
    {
        struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
        if (!GetBoxMonData(&copy.mons[i].mon, MON_DATA_SANITY_HAS_SPECIES)) continue;
        if (Snapshot(&copy.mons[i].mon, RHH_MON_DAYCARE, 0, i, &mon) || mon.is_egg) return -1;
        ++view.parent_count;
    }
    if (view.parent_count == DAYCARE_MON_COUNT)
    {
        unsigned score = GetDaycareCompatibilityScore(&copy);
        view.compatibility_known = 1;
        switch (score)
        {
        case PARENTS_INCOMPATIBLE: view.compatibility = RHH_DAYCARE_INCOMPATIBLE; break;
        case PARENTS_LOW_COMPATIBILITY: view.compatibility = RHH_DAYCARE_LOW; break;
        case PARENTS_MED_COMPATIBILITY: view.compatibility = RHH_DAYCARE_MEDIUM; break;
        case PARENTS_MAX_COMPATIBILITY: view.compatibility = RHH_DAYCARE_HIGH; break;
        default: return -1;
        }
    }
    *out = view;
    return 0;
}
static const struct RhhBreedingContext *sBreedingContext;
static bool32 sBreedingEggHadPending;
static const struct RhhDaycareEggOrdinal *sDaycareOrdinalContext;
static const struct RhhBreedingIvCountContext *sBreedingIvCountContext;
static const struct RhhBreedingNatureContext *sBreedingNatureContext;
static const struct RhhBreedingTraitContext *sBreedingTraitContext;
static const struct RhhBreedingIvRuleContext *sBreedingIvRuleContext;
static const struct RhhBreedingMoveRuleContext *sBreedingMoveRuleContext;
static const struct RhhBreedingRngPolicy *sBreedingRngPolicyContext;
static bool32 sBreedingPreviewActive, sBreedingMovePreviewBusy;
static int BreedingPreview(struct RhhBreedingPreview *out);
static int BreedingOffer(struct RhhBreedingOffer *out);
static int BreedingAccept(const struct RhhBreedingOffer *expected, struct RhhBreedingAcceptResult *out);
static int BreedingAcceptExact(const struct RhhBreedingOffer *, const struct RhhBreedingExactPreview *, struct RhhBreedingAcceptResult *);
static int DaycareOrdinalState(struct RhhDaycareOrdinalState *out);
static int EnableDaycareOrdinal(u64 expectedRun, struct RhhDaycareOrdinalState *out);
static int DaycareEggOrdinalContext(struct RhhDaycareEggOrdinal *out);
static int DaycareTiming(struct RhhDaycareTiming *out);
static int DaycareEggProgress(u32 slot, struct RhhDaycareEggProgress *out);
static int BreedingAcceptContext(struct RhhBreedingAcceptResult *out);
static int BreedingRequest(const struct RhhBreedingOffer *expected, struct RhhBreedingRequestResult *out);
static int BreedingRequestContext(struct RhhBreedingRequestResult *out);
static int BreedingPendingContext(struct RhhDaycarePendingChange *out);
static int BreedingPendingId(struct RhhDaycareEggId *out);
static int BreedingPendingChangeId(struct RhhDaycareEggId *out);
static int BreedingPendingRandomAt(const struct RhhDaycareEggId *expected, const char *namespaceId, u64 ordinal, u64 *out);
static int BreedingRngPolicy(const struct RhhBreedingOffer *expected, struct RhhBreedingRngPolicy *out);
static int BreedingRngPolicyContext(struct RhhBreedingRngPolicy *out);
static int BreedingExactPreview(const struct RhhBreedingOffer *expected, struct RhhBreedingExactPreview *out);
static int BreedingIvPreview(const struct RhhBreedingOffer *expected, struct RhhBreedingIvPreview *out);
static int BreedingIvOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingIvOdds *out);
static int BreedingIvJointOdds(const struct RhhBreedingOffer *expected, const struct RhhBreedingIvJointRequest *request,
                               struct RhhBreedingIvJointOdds *out);
static int BreedingMovePreview(const struct RhhBreedingOffer *expected, struct RhhBreedingMovePreview *out);
static int BreedingNatureOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingNatureOdds *out);
static int BreedingBallOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingBallOdds *out);
static int BreedingAbilityOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingAbilityOdds *out);
static int BreedingOffspringOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingOffspringOdds *out);
static int BreedingGenderOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingGenderOdds *out);
static int BreedingNatureGenderOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingNatureGenderOdds *out);
static unsigned CompatibilityValue(unsigned score);
static int BreedingIvCountContext(struct RhhBreedingIvCountContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBreedingIvCountContext
        || RhhModsCurrentEvent() != RHH_BREED_IV_COUNT) return -1;
    *out = *sBreedingIvCountContext; return 0;
}
static int BreedingNatureContext(struct RhhBreedingNatureContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBreedingNatureContext
        || RhhModsCurrentEvent() != RHH_BREED_NATURE) return -1;
    *out = *sBreedingNatureContext; return 0;
}
static int BreedingIvRuleContext(struct RhhBreedingIvRuleContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBreedingIvRuleContext
        || RhhModsCurrentEvent() != RHH_BREED_IV_RULE) return -1;
    *out = *sBreedingIvRuleContext; return 0;
}
static int BreedingMoveRuleContext(struct RhhBreedingMoveRuleContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBreedingMoveRuleContext
        || RhhModsCurrentEvent() != RHH_BREED_MOVE_RULE) return -1;
    *out = *sBreedingMoveRuleContext; return 0;
}
static int BreedingTraitContext(struct RhhBreedingTraitContext *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sBreedingTraitContext
        || (event != RHH_BREED_BALL_RULE && event != RHH_BREED_ABILITY_RULE)) return -1;
    *out = *sBreedingTraitContext; return 0;
}
static int BreedingContext(struct RhhBreedingContext *out)
{
    if (!out || out->struct_size < RHH_BREEDING_CONTEXT_V1_0_SIZE || !sBreedingContext) return -1;
    size_t size = out->struct_size >= sizeof(*out) ? sizeof(*out)
        : out->struct_size >= RHH_BREEDING_CONTEXT_V1_1_SIZE ? RHH_BREEDING_CONTEXT_V1_1_SIZE
        : RHH_BREEDING_CONTEXT_V1_0_SIZE;
    memcpy(out, sBreedingContext, size);
    out->struct_size = size;
    return 0;
}
static const struct RhhBreedingService sBreedingService = {
    {sizeof(sBreedingService), RHH_SERVICE_VERSION(1, 28)}, BreedingContext, BreedingPreview,
    BreedingOffer, BreedingAccept, BreedingAcceptContext, BreedingIvPreview, BreedingIvOdds, BreedingIvJointOdds,
    BreedingIvCountContext, BreedingMovePreview, BreedingNatureOdds, BreedingNatureContext, BreedingIvRuleContext,
    BreedingMoveRuleContext, BreedingRequest, BreedingRequestContext, BreedingPendingContext,
    BreedingPendingId, BreedingPendingChangeId, BreedingPendingRandomAt, BreedingRngPolicy, BreedingRngPolicyContext,
    BreedingExactPreview, BreedingTraitContext, BreedingBallOdds, BreedingAbilityOdds, BreedingOffspringOdds, BreedingAcceptExact,
    DaycareOrdinalState, EnableDaycareOrdinal, DaycareEggOrdinalContext, BreedingGenderOdds, BreedingNatureGenderOdds
};
static u32 BreedingTraitResolve(struct RhhBreedingTraitContext *context, u32 kind, u32 base, u32 species)
{
    if (!RhhModsHasHook(kind)) return base;
    if (kind == RHH_BREED_BALL_RULE && (base <= BALL_STRANGE || base >= POKEBALL_COUNT)) return base;
    context->base = kind == RHH_BREED_ABILITY_RULE ? base : RhhContentKeyForId(RHH_ITEM, gPokeBalls[base].itemId);
    if (kind == RHH_BREED_BALL_RULE && !context->base) return base;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = kind, .run_seed = RhhModsRunSeed(),
        .event_key = context->offspring, .subject = context->original_offspring, .args = {context->compatibility}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = context->base};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBreedingTraitContext = context; RhhModsDispatch(&event, &decision); sBreedingTraitContext = NULL;
    bool32 usedRng = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed || usedRng || decision.value == context->base) return base;
    if (kind == RHH_BREED_ABILITY_RULE)
        return decision.value < NUM_ABILITY_SLOTS && GetSpeciesAbility(species, decision.value) ? decision.value : base;
    u32 item;
    if (RhhContentResolve(RHH_ITEM, decision.value, &item)) return base;
    for (u32 ball = BALL_POKE; ball < POKEBALL_COUNT; ++ball)
        if (ball != BALL_MASTER && ball != BALL_CHERISH && gPokeBalls[ball].itemId == item) return ball;
    return base;
}
void ModsBreedingEggTraits(struct DayCare *daycare, struct Pokemon *egg, u32 originalSpecies,
                           u32 compatibility, const u8 *parentSlots)
{
    if (!gSaveBlock1Ptr || daycare != &gSaveBlock1Ptr->daycare || !egg || !parentSlots
        || parentSlots[0] >= DAYCARE_MON_COUNT || parentSlots[1] >= DAYCARE_MON_COUNT
        || !RhhModsHasActiveRun() || sBreedingTraitContext || sBreedingContext || sBreedingNatureContext
        || sBreedingIvCountContext || sBreedingIvRuleContext || sBreedingMoveRuleContext || sBreedingRngPolicyContext
        || NativeSaveTransactionActive()) return;
    u32 species = GetMonData(egg, MON_DATA_SPECIES);
    if (!species || species >= NUM_SPECIES) return;
    struct RhhBreedingTraitContext context = {.struct_size = sizeof(context), .compatibility = CompatibilityValue(compatibility),
        .parent_slots = {parentSlots[0], parentSlots[1]},
        .original_offspring = RhhContentKeyForId(RHH_SPECIES, originalSpecies),
        .offspring = RhhContentKeyForId(RHH_SPECIES, species)};
    if (!context.original_offspring || !context.offspring) return;
    for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i) {
        context.parents[i].struct_size = sizeof(context.parents[i]);
        if (DaycareParentAt(i, &context.parents[i])) return;
    }
    const u32 kinds[] = {RHH_BREED_BALL_RULE, RHH_BREED_ABILITY_RULE};
    for (u32 i = 0; i < ARRAY_COUNT(kinds); ++i) {
        u32 base = GetMonData(egg, i ? MON_DATA_ABILITY_NUM : MON_DATA_POKEBALL);
        u32 resolved = BreedingTraitResolve(&context, kinds[i], base, species);
        if (resolved != base) SetMonData(egg, i ? MON_DATA_ABILITY_NUM : MON_DATA_POKEBALL, &resolved);
    }
}
u32 ModsBreedingNature(struct DayCare *daycare, u32 baseNature)
{
    if (!gSaveBlock1Ptr || daycare != &gSaveBlock1Ptr->daycare || !RhhModsHasActiveRun()
        || !RhhModsHasHook(RHH_BREED_NATURE) || sBreedingNatureContext || sBreedingTraitContext
        || sBreedingContext || sBreedingIvCountContext || sBreedingIvRuleContext || sBreedingMoveRuleContext || sBreedingRngPolicyContext
        || NativeSaveTransactionActive() || baseNature > NATURE_RANDOM)
        return baseNature;
    struct RhhBreedingNatureContext context = {.struct_size = sizeof(context),
        .generation = GetConfig(NATURE_INHERITANCE) + 1,
        .base_nature = baseNature == NATURE_RANDOM ? 0 : NatureKey(baseNature)};
    for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
    {
        context.parents[i].struct_size = sizeof(context.parents[i]);
        if (DaycareParentAt(i, &context.parents[i])) return baseNature;
    }
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BREED_NATURE,
        .run_seed = RhhModsRunSeed(), .event_key = context.base_nature,
        .subject = context.parents[0].mon.species, .args = {context.generation}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = context.base_nature};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBreedingNatureContext = &context; RhhModsDispatch(&event, &decision); sBreedingNatureContext = NULL;
    if (memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
    { gRngValue = rng; gRng2Value = rng2; return baseNature; }
    if (!decision.allowed) return baseNature;
    if (!decision.value) return NATURE_RANDOM;
    struct RhhNatureInfo resolved = {.struct_size = sizeof(resolved)};
    return NatureFind(decision.value, &resolved) ? baseNature : resolved.ordinal;
}
u32 ModsBreedingIvCount(struct DayCare *daycare, u32 baseCount)
{
    if (!gSaveBlock1Ptr || daycare != &gSaveBlock1Ptr->daycare || !RhhModsHasActiveRun()
        || !RhhModsHasHook(RHH_BREED_IV_COUNT) || sBreedingIvCountContext || sBreedingNatureContext || sBreedingTraitContext || sBreedingIvRuleContext
        || sBreedingMoveRuleContext || sBreedingContext || sBreedingRngPolicyContext
        || NativeSaveTransactionActive() || baseCount > RHH_STAT_COUNT) return baseCount;
    struct RhhBreedingIvCountContext context = {.struct_size = sizeof(context), .base_count = baseCount};
    for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
    {
        context.parents[i].struct_size = sizeof(context.parents[i]);
        if (DaycareParentAt(i, &context.parents[i])) return baseCount;
    }
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BREED_IV_COUNT,
        .run_seed = RhhModsRunSeed(), .event_key = context.parents[0].mon.species,
        .subject = context.parents[1].mon.species, .args = {baseCount}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = baseCount};
    sBreedingIvCountContext = &context; RhhModsDispatch(&event, &decision); sBreedingIvCountContext = NULL;
    return decision.allowed && decision.value <= RHH_STAT_COUNT ? decision.value : baseCount;
}
static unsigned CompatibilityValue(unsigned score)
{
    switch (score)
    {
    case PARENTS_LOW_COMPATIBILITY: return RHH_DAYCARE_LOW;
    case PARENTS_MED_COMPATIBILITY: return RHH_DAYCARE_MEDIUM;
    case PARENTS_MAX_COMPATIBILITY: return RHH_DAYCARE_HIGH;
    default: return RHH_DAYCARE_INCOMPATIBLE;
    }
}
static int BreedingPrepare(struct DayCare *daycare, unsigned base, unsigned score,
                           struct RhhBreedingContext *context)
{
    if (!gSaveBlock1Ptr || daycare != &gSaveBlock1Ptr->daycare || !RhhModsHasActiveRun() || sBreedingContext || sBreedingNatureContext || sBreedingTraitContext
        || sBreedingIvRuleContext || sBreedingMoveRuleContext || sBreedingRngPolicyContext)
        return -1;
    *context = (struct RhhBreedingContext){.struct_size = sizeof(*context),
        .base_compatibility = CompatibilityValue(base), .compatibility = CompatibilityValue(score),
        .preview = sBreedingPreviewActive};
    for (unsigned i = 0; i < 2; ++i)
    {
        context->parents[i].struct_size = sizeof(context->parents[i]);
        if (DaycareParentAt(i, &context->parents[i])) return -1;
    }
    return 0;
}
u32 ModsBreedingCompatibility(struct DayCare *daycare, u32 baseScore)
{
    struct RhhBreedingContext context;
    if (BreedingPrepare(daycare, baseScore, baseScore, &context)) return baseScore;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BREED_COMPATIBILITY,
        .run_seed = RhhModsRunSeed(), .event_key = context.parents[0].mon.species,
        .subject = context.parents[1].mon.species, .args = {context.base_compatibility}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1,
        .value = context.base_compatibility};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBreedingContext = &context; RhhModsDispatch(&event, &decision); sBreedingContext = NULL;
    bool32 rngChanged = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed) return PARENTS_INCOMPATIBLE;
    if (rngChanged) return baseScore;
    const u8 scores[] = {PARENTS_INCOMPATIBLE, PARENTS_LOW_COMPATIBILITY,
                         PARENTS_MED_COMPATIBILITY, PARENTS_MAX_COMPATIBILITY};
    return decision.value < ARRAY_COUNT(scores) ? scores[decision.value] : baseScore;
}
u32 ModsBreedingOffspring(struct DayCare *daycare, u32 species, u32 compatibility, const u8 *parentSlots)
{
    struct RhhBreedingContext context;
    if (!parentSlots || BreedingPrepare(daycare, GetDaycareCompatibilityScoreBase(daycare), compatibility, &context)) return species;
    context.parent_slots[0] = parentSlots[0]; context.parent_slots[1] = parentSlots[1];
    context.original_offspring = context.offspring = RhhContentKeyForId(RHH_SPECIES, species);
    if (!context.original_offspring) return species;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BREED_OFFSPRING,
        .run_seed = RhhModsRunSeed(), .event_key = context.original_offspring,
        .subject = context.parents[0].mon.species, .args = {context.compatibility, parentSlots[0], parentSlots[1]}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = context.original_offspring};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBreedingContext = &context; RhhModsDispatch(&event, &decision); sBreedingContext = NULL;
    bool32 rngChanged = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    u32 result;
    struct RhhContentInfo info = {.struct_size = sizeof(info)};
    if (rngChanged || !decision.allowed || RhhContentInfoFor(RHH_SPECIES, decision.value, &info)
        || !(info.flags & RHH_CONTENT_CREATABLE) || RhhContentResolve(RHH_SPECIES, decision.value, &result)) return species;
    return result;
}
static int BreedingPreview(struct RhhBreedingPreview *out)
{
    if (!out || out->struct_size < sizeof(*out) || !gSaveBlock1Ptr || !RhhModsHasActiveRun()
        || sBreedingContext || sBreedingIvCountContext || sBreedingNatureContext || sBreedingTraitContext || sBreedingIvRuleContext
        || sBreedingMoveRuleContext || sBreedingRngPolicyContext || sBreedingPreviewActive || sBreedingMovePreviewBusy) return -1;
    struct RhhDaycareParent parents[2] = {{.struct_size = sizeof(parents[0])}, {.struct_size = sizeof(parents[1])}};
    if (DaycareParentAt(0, &parents[0]) || DaycareParentAt(1, &parents[1])) return -1;
    struct DayCare *daycare = &gSaveBlock1Ptr->daycare, copy = *daycare;
    u32 base = GetDaycareCompatibilityScoreBase(&copy);
    struct RhhBreedingPreview result = {.struct_size = sizeof(result),
        .base_compatibility = CompatibilityValue(base), .egg_pending = FlagGet(FLAG_PENDING_DAYCARE_EGG)};
    sBreedingPreviewActive = TRUE;
    u32 compatibility = ModsBreedingCompatibility(daycare, base);
    result.compatibility = CompatibilityValue(compatibility);
    if (result.compatibility != RHH_DAYCARE_INCOMPATIBLE)
    {
        u32 first = result.egg_pending ? !!(copy.offspringPersonality & EGG_GENDER_MALE) : 0;
        u32 count = result.egg_pending ? 1 : 2;
        for (u32 i = 0; i < count; ++i)
        {
            u8 slots[2];
            u32 original = GetDaycareOffspringCandidate(&copy, first + i, slots);
            u32 offspring = ModsBreedingOffspring(daycare, original, compatibility, slots);
            struct RhhBreedingPreviewCandidate candidate = {
                .original_offspring = RhhContentKeyForId(RHH_SPECIES, original),
                .offspring = RhhContentKeyForId(RHH_SPECIES, offspring),
                .parent_slots = {slots[0], slots[1]}};
            if (!candidate.original_offspring || !candidate.offspring) { sBreedingPreviewActive = FALSE; return -1; }
            if (!result.candidate_count || memcmp(&result.candidates[0], &candidate, sizeof(candidate)))
                result.candidates[result.candidate_count++] = candidate;
        }
    }
    sBreedingPreviewActive = FALSE;
    *out = result; return 0;
}
static int SampleDaycareHatchDecrement(u32 *out)
{
    if (!out) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    enum Ability last = gLastUsedAbility;
    u32 decrement = NativeDaycareHatchCycleDecrement();
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
    if (!pure || !decrement) return -1;
    *out = decrement; return 0;
}
static int DaycareTiming(struct RhhDaycareTiming *out)
{
    if (!out || out->struct_size < sizeof(*out) || !gSaveBlock1Ptr || !gSaveBlock2Ptr
        || !RhhModsHasActiveRun() || NativeSaveTransactionActive()) return -1;
    struct RhhDaycareSnapshot snapshot = {.struct_size = sizeof(snapshot)};
    if (DaycareCurrent(&snapshot)) return -1;
    struct DayCare copy = gSaveBlock1Ptr->daycare;
    u32 cycle = NativeDaycareHatchCycleLength(), next = copy.stepCounter + 1, decrement;
    if (SampleDaycareHatchDecrement(&decrement)) return -1;
    struct RhhDaycareTiming result = {.struct_size = sizeof(result), .egg_pending = snapshot.egg_pending,
        .hatch_counter = copy.stepCounter, .hatch_cycle_length = cycle,
        .hatch_calls = next >= cycle ? 1 : cycle - next + 1,
        .hatch_cycle_decrement = decrement};
    if (snapshot.parent_count == DAYCARE_MON_COUNT && !snapshot.egg_pending)
    {
        if (sBreedingContext || sBreedingIvCountContext || sBreedingNatureContext || sBreedingTraitContext
            || sBreedingIvRuleContext || sBreedingMoveRuleContext || sBreedingRngPolicyContext
            || sBreedingPreviewActive || sBreedingMovePreviewBusy) return -1;
        u32 base = GetDaycareCompatibilityScoreBase(&copy);
        sBreedingPreviewActive = TRUE;
        u32 score = ModsBreedingCompatibility(&gSaveBlock1Ptr->daycare, base);
        sBreedingPreviewActive = FALSE;
        if (memcmp(&copy, &gSaveBlock1Ptr->daycare, sizeof(copy))
            || FlagGet(FLAG_PENDING_DAYCARE_EGG) != snapshot.egg_pending) return -1;
        result.egg_roll_active = 1;
        result.egg_roll_calls = (255u - (copy.mons[1].steps & 255u)) & 255u;
        if (!result.egg_roll_calls) result.egg_roll_calls = 256;
        result.egg_chance_percent = NativeDaycareEggChance(score);
    }
    *out = result; return 0;
}
static int DaycareEggProgress(u32 slot, struct RhhDaycareEggProgress *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun()
        || gMain.inBattle || !OwnedStorageAvailable() || !gSaveBlock2Ptr
        || gPartiesCount[B_TRAINER_PLAYER] > PARTY_SIZE || slot >= gPartiesCount[B_TRAINER_PLAYER]) return -1;
    struct RhhOwnedMonSnapshot egg = {.struct_size = sizeof(egg)};
    if (PartyAt(slot, &egg) || !egg.is_egg) return -1;
    struct Pokemon copy = gParties[B_TRAINER_PLAYER][slot];
    u32 cycles = GetMonData(&copy, MON_DATA_FRIENDSHIP);
    if (GetMonData(&copy, MON_DATA_SANITY_IS_BAD_EGG)) return -1;
    u32 cycle = NativeDaycareHatchCycleLength(), decrement;
    if (SampleDaycareHatchDecrement(&decrement)) return -1;
    u32 next = gSaveBlock1Ptr->daycare.stepCounter + 1;
    u32 calls = next >= cycle ? 1 : cycle - next + 1;
    u32 checks = RhhModsHasHook(RHH_EGG_CYCLE_PROGRESS) ? (cycles ? 2 : 1)
        : (cycles + decrement - 1) / decrement + 1;
    struct RhhDaycareEggProgress result = {.struct_size = sizeof(result), .cycles_remaining = cycles,
        .hatch_checks = checks, .hatch_cycle_length = cycle, .hatch_cycle_decrement = decrement,
        .hatch_calls_next = calls, .calls_lower_bound = (u64)calls + (u64)(checks - 1) * cycle, .egg = egg};
    for (u32 i = 0; i < slot; ++i)
    {
        copy = gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(&copy, MON_DATA_IS_EGG) && !GetMonData(&copy, MON_DATA_SANITY_IS_BAD_EGG)
            && !GetMonData(&copy, MON_DATA_FRIENDSHIP) && !GetMonData(&copy, MON_DATA_SANITY_IS_BAD_EGG))
            ++result.earlier_ready_eggs;
    }
    *out = result; return 0;
}
static const struct RhhEggHatchEligibility *sHatchEligibilityContext;
static int HatchEligibilityContext(struct RhhEggHatchEligibility *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sHatchEligibilityContext
        || RhhModsCurrentEvent() != RHH_CAN_HATCH_EGG) return -1;
    *out = *sHatchEligibilityContext; return 0;
}
static int HatchEligibility(u32 slot, struct RhhEggHatchEligibility *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || gMain.inBattle
        || !OwnedStorageAvailable() || !gSaveBlock1Ptr || !gSaveBlock2Ptr || sHatchEligibilityContext
        || gPartiesCount[B_TRAINER_PLAYER] > PARTY_SIZE || slot >= gPartiesCount[B_TRAINER_PLAYER]) return -1;
    struct RhhEggHatchEligibility result = {.struct_size = sizeof(result), .run_id = RhhModsRunId(),
        .egg.struct_size = sizeof(result.egg)};
    if (PartyAt(slot, &result.egg) || !result.egg.is_egg) return -1;
    struct Pokemon copy = gParties[B_TRAINER_PLAYER][slot];
    if (GetMonData(&copy, MON_DATA_SANITY_IS_BAD_EGG)) return -1;
    result.cycles_remaining = GetMonData(&copy, MON_DATA_FRIENDSHIP);
    if (result.cycles_remaining) { result.status = RHH_EGG_HATCH_NOT_READY; *out = result; return 0; }
    result.status = RHH_EGG_HATCH_ALLOWED;
    if (RhhModsHasHook(RHH_CAN_HATCH_EGG))
    {
        struct Pokemon party[PARTY_SIZE]; memcpy(party, gParties[B_TRAINER_PLAYER], sizeof(party));
        u32 count = gPartiesCount[B_TRAINER_PLAYER], counter = gSaveBlock1Ptr->daycare.stepCounter;
        struct SaveBlock1 *save = gSaveBlock1Ptr;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_HATCH_EGG,
            .run_seed = RhhModsRunSeed(), .event_key = result.egg.species, .subject = result.egg.identity,
            .args = {slot, 0, 0, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        enum Ability last = gLastUsedAbility;
        sHatchEligibilityContext = &result; RhhModsDispatch(&event, &decision); sHatchEligibilityContext = NULL;
        bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
        gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
        if (!pure || gSaveBlock1Ptr != save || RhhModsRunId() != result.run_id
            || !RhhModsHasActiveRun() || gMain.inBattle || !OwnedStorageAvailable()
            || gPartiesCount[B_TRAINER_PLAYER] != count || memcmp(party, gParties[B_TRAINER_PLAYER], sizeof(party))
            || gSaveBlock1Ptr->daycare.stepCounter != counter) result.status = RHH_EGG_HATCH_IMPURE;
        else if (!decision.allowed) result.status = RHH_EGG_HATCH_DENIED;
    }
    *out = result; return 0;
}
bool32 ModsCanHatchEgg(u32 slot)
{
    if (!RhhModsHasActiveRun() || !RhhModsHasHook(RHH_CAN_HATCH_EGG)) return TRUE;
    struct RhhEggHatchEligibility result = {.struct_size = sizeof(result)};
    return !HatchEligibility(slot, &result) && result.status == RHH_EGG_HATCH_ALLOWED;
}
static const struct RhhEggCycleProgress *sEggCycleContext;
static int EggCycleContext(struct RhhEggCycleProgress *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sEggCycleContext
        || RhhModsCurrentEvent() != RHH_EGG_CYCLE_PROGRESS) return -1;
    *out = *sEggCycleContext; return 0;
}
static int EggCycleEvaluate(u32 slot, u32 engine, struct RhhEggCycleProgress *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || gMain.inBattle
        || !OwnedStorageAvailable() || !gSaveBlock1Ptr || !gSaveBlock2Ptr || sEggCycleContext
        || gPartiesCount[B_TRAINER_PLAYER] > PARTY_SIZE || slot >= gPartiesCount[B_TRAINER_PLAYER]) return -1;
    struct RhhEggCycleProgress result = {.struct_size = sizeof(result), .run_id = RhhModsRunId(),
        .egg.struct_size = sizeof(result.egg)};
    if (PartyAt(slot, &result.egg) || !result.egg.is_egg) return -1;
    struct Pokemon copy = gParties[B_TRAINER_PLAYER][slot];
    if (GetMonData(&copy, MON_DATA_SANITY_IS_BAD_EGG)) return -1;
    result.cycles_remaining = GetMonData(&copy, MON_DATA_FRIENDSHIP);
    result.engine_decrement = min(result.cycles_remaining, engine);
    result.decrement = result.engine_decrement;
    result.remaining_after = result.cycles_remaining - result.engine_decrement;
    if (result.cycles_remaining && RhhModsHasHook(RHH_EGG_CYCLE_PROGRESS))
    {
        struct Pokemon party[PARTY_SIZE]; memcpy(party, gParties[B_TRAINER_PLAYER], sizeof(party));
        u32 count = gPartiesCount[B_TRAINER_PLAYER], counter = gSaveBlock1Ptr->daycare.stepCounter;
        struct SaveBlock1 *save = gSaveBlock1Ptr;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_EGG_CYCLE_PROGRESS,
            .run_seed = RhhModsRunSeed(), .event_key = result.egg.species, .subject = result.egg.identity,
            .args = {slot, result.cycles_remaining, result.engine_decrement, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = result.engine_decrement};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        enum Ability last = gLastUsedAbility;
        sEggCycleContext = &result; RhhModsDispatch(&event, &decision); sEggCycleContext = NULL;
        bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
        gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
        if (gSaveBlock1Ptr != save || RhhModsRunId() != result.run_id || !RhhModsHasActiveRun()
            || gMain.inBattle || !OwnedStorageAvailable() || gPartiesCount[B_TRAINER_PLAYER] != count
            || memcmp(party, gParties[B_TRAINER_PLAYER], sizeof(party))
            || gSaveBlock1Ptr->daycare.stepCounter != counter) return -1;
        if (pure && decision.allowed && decision.value <= result.cycles_remaining) result.decrement = decision.value;
    }
    result.remaining_after = result.cycles_remaining - result.decrement;
    *out = result; return 0;
}
static int EggCyclePreview(u32 slot, struct RhhEggCycleProgress *out)
{
    u32 engine;
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || gMain.inBattle
        || !OwnedStorageAvailable() || !gSaveBlock1Ptr || !gSaveBlock2Ptr || sEggCycleContext
        || slot >= gPartiesCount[B_TRAINER_PLAYER] || SampleDaycareHatchDecrement(&engine)) return -1;
    return EggCycleEvaluate(slot, engine, out);
}
bool32 ModsAdjustEggCycles(u32 slot, u32 cycles, u32 engine, u32 *remaining)
{
    if (!remaining) return FALSE;
    *remaining = cycles - min(cycles, engine);
    if (!RhhModsHasActiveRun() || !RhhModsHasHook(RHH_EGG_CYCLE_PROGRESS)) return TRUE;
    struct RhhEggCycleProgress result = {.struct_size = sizeof(result)};
    if (EggCycleEvaluate(slot, engine, &result) || result.cycles_remaining != cycles) return FALSE;
    *remaining = result.remaining_after; return TRUE;
}
static const struct RhhEggCycleChange *sEggCycleChangeContext;
static u64 sEggCycleChangeSequence;
static int EggCycleChangeContext(struct RhhEggCycleChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sEggCycleChangeContext
        || RhhModsCurrentEvent() != RHH_EGG_CYCLE_TICKED) return -1;
    *out = *sEggCycleChangeContext; return 0;
}
void ModsCaptureEggCycle(u32 slot, u32 cycles, u32 engine, u32 remaining, struct ModsEggCycleCapture *out)
{
    if (!out) return;
    *out = (struct ModsEggCycleCapture){0};
    if (!RhhModsHasActiveRun() || !RhhModsHasHook(RHH_EGG_CYCLE_TICKED) || sEggCycleChangeContext
        || sEggCycleChangeSequence == UINT64_MAX || !cycles || remaining > cycles || gMain.inBattle
        || !OwnedStorageAvailable() || !gSaveBlock1Ptr || !gSaveBlock2Ptr || slot >= PARTY_SIZE
        || slot >= gPartiesCount[B_TRAINER_PLAYER] || gPartiesCount[B_TRAINER_PLAYER] > PARTY_SIZE) return;
    struct RhhEggCycleProgress progress = {.struct_size = sizeof(progress), .run_id = RhhModsRunId(),
        .cycles_remaining = cycles, .engine_decrement = min(cycles, engine),
        .decrement = cycles - remaining, .remaining_after = remaining, .egg.struct_size = sizeof(progress.egg)};
    struct Pokemon copy = gParties[B_TRAINER_PLAYER][slot];
    if (PartyAt(slot, &progress.egg) || !progress.egg.is_egg || GetMonData(&copy, MON_DATA_SANITY_IS_BAD_EGG)
        || GetMonData(&copy, MON_DATA_FRIENDSHIP) != cycles) return;
    *out = (struct ModsEggCycleCapture){.valid = TRUE, .partyCount = gPartiesCount[B_TRAINER_PLAYER],
        .counter = gSaveBlock1Ptr->daycare.stepCounter, .save = gSaveBlock1Ptr, .progress = progress};
}
void ModsEggCycleCommitted(const struct ModsEggCycleCapture *capture)
{
    if (!capture || !capture->valid || !RhhModsHasActiveRun() || RhhModsCurrentEvent() || sEggCycleChangeContext
        || !RhhModsHasHook(RHH_EGG_CYCLE_TICKED) || sEggCycleChangeSequence == UINT64_MAX
        || capture->progress.run_id != RhhModsRunId() || gMain.inBattle || !OwnedStorageAvailable()
        || gSaveBlock1Ptr != capture->save || gSaveBlock1Ptr->daycare.stepCounter != capture->counter
        || gPartiesCount[B_TRAINER_PLAYER] != capture->partyCount || capture->progress.egg.slot >= capture->partyCount) return;
    u32 slot = capture->progress.egg.slot;
    struct Pokemon copy = gParties[B_TRAINER_PLAYER][slot];
    struct RhhOwnedMonSnapshot current = {.struct_size = sizeof(current)};
    if (PartyAt(slot, &current) || !current.is_egg || !RhhMonSnapshotsEqual(&current, &capture->progress.egg)
        || GetMonData(&copy, MON_DATA_SANITY_IS_BAD_EGG)
        || GetMonData(&copy, MON_DATA_FRIENDSHIP) != capture->progress.remaining_after) return;
    struct RhhEggCycleChange change = {.struct_size = sizeof(change), .sequence = sEggCycleChangeSequence++,
        .progress = capture->progress};
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_EGG_CYCLE_TICKED,
        .run_seed = RhhModsRunSeed(), .event_key = change.progress.egg.species,
        .subject = change.progress.egg.identity, .ordinal = change.sequence,
        .args = {slot, change.progress.cycles_remaining, change.progress.remaining_after, change.progress.engine_decrement}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    sEggCycleChangeContext = &change; RhhModsDispatch(&event, &decision); sEggCycleChangeContext = NULL;
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
}
static bool32 sBreedingOfferBusy, sBreedingAcceptBusy;
static const struct RhhBreedingAcceptResult *sBreedingAcceptContext;
static bool32 sBreedingRequestBusy;
static int DaycareOrdinalState(struct RhhDaycareOrdinalState *out)
{
    u64 next;
    if (!out || out->struct_size < sizeof(*out) || ModsDaycareOrdinalState(&next)) return -1;
    *out = (struct RhhDaycareOrdinalState){.struct_size = sizeof(*out), .run_id = RhhModsRunId(),
        .enabled = !!next, .exhausted = next == UINT64_MAX, .next = next};
    return 0;
}
static int EnableDaycareOrdinal(u64 expectedRun, struct RhhDaycareOrdinalState *out)
{
    if (!out || out->struct_size < sizeof(*out) || !expectedRun || expectedRun != RhhModsRunId()
        || sBreedingAcceptBusy || sBreedingRequestBusy || sBreedingPreviewActive || sBreedingMovePreviewBusy
        || ModsEnableDaycareOrdinal()) return -1;
    return DaycareOrdinalState(out);
}
static int DaycareEggOrdinalContext(struct RhhDaycareEggOrdinal *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sDaycareOrdinalContext
        || RhhModsCurrentEvent() != RHH_BREED_EGG_CREATED) return -1;
    *out = *sDaycareOrdinalContext; return 0;
}
static const struct RhhBreedingRequestResult *sBreedingRequestContext;
static const struct RhhDaycarePendingChange *sBreedingPendingContext;
static const struct RhhDaycareEggId *sBreedingPendingIdContext;
static int BreedingPendingId(struct RhhDaycareEggId *out)
{
    u64 high, low;
    if (!out || out->struct_size < sizeof(*out) || !gSaveBlock1Ptr || !FlagGet(FLAG_PENDING_DAYCARE_EGG)
        || ModsPendingEggId(&high, &low)) return -1;
    *out = (struct RhhDaycareEggId){.struct_size = sizeof(*out), .run_id = RhhModsRunId(), .high = high, .low = low};
    return 0;
}
static int BreedingPendingChangeId(struct RhhDaycareEggId *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    u32 event = RhhModsCurrentEvent();
    if (event == RHH_DAYCARE_PENDING_CHANGED && sBreedingPendingIdContext)
    { *out = *sBreedingPendingIdContext; return 0; }
    if (event == RHH_BREED_EGG_CREATED && sBreedingContext && sBreedingEggHadPending)
    {
        u64 high, low;
        if (ModsPendingEggId(&high, &low)) return -1;
        *out = (struct RhhDaycareEggId){.struct_size = sizeof(*out), .run_id = RhhModsRunId(), .high = high, .low = low};
        return 0;
    }
    return -1;
}
static int BreedingPendingRandomAt(const struct RhhDaycareEggId *expected, const char *namespaceId, u64 ordinal, u64 *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved || !namespaceId || !out
        || !*namespaceId || strnlen(namespaceId, 64) >= 64 || !RhhModsRegisteredKey(RhhContentKeyOf(namespaceId))) return -1;
    struct RhhDaycareEggId current = {.struct_size = sizeof(current)};
    if (BreedingPendingId(&current) && BreedingPendingChangeId(&current)) return -1;
    if (expected->run_id != current.run_id || expected->high != current.high || expected->low != current.low) return -1;
    u64 seed = RhhModRandomAt(RhhModsRunSeed(), "rhh:breeding_pending", current.run_id, current.high);
    *out = RhhModRandomAt(seed, namespaceId, current.low, ordinal);
    return 0;
}
void ModsBreedingReconcilePendingId(void)
{
    if (!gSaveBlock1Ptr || !RhhModsHasActiveRun()) return;
    u64 high, low;
    if (!FlagGet(FLAG_PENDING_DAYCARE_EGG)) ModsClearPendingEggId();
    else if (ModsPendingEggId(&high, &low)) ModsMintPendingEggId();
}
static int BreedingPendingContext(struct RhhDaycarePendingChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBreedingPendingContext
        || RhhModsCurrentEvent() != RHH_DAYCARE_PENDING_CHANGED) return -1;
    *out = *sBreedingPendingContext; return 0;
}
void ModsBreedingPendingChanged(struct DayCare *daycare, u32 cause, bool32 before, bool32 after)
{
    if (!gSaveBlock1Ptr || daycare != &gSaveBlock1Ptr->daycare || !RhhModsHasActiveRun()
        || sBreedingPendingContext
        || before == after || !!FlagGet(FLAG_PENDING_DAYCARE_EGG) != !!after) return;
    struct RhhDaycareEggId id = {.struct_size = sizeof(id), .run_id = RhhModsRunId()};
    bool32 hasId;
    if (after)
    {
        ModsClearPendingEggId();
        hasId = !ModsMintPendingEggId() && !ModsPendingEggId(&id.high, &id.low);
    }
    else
    {
        hasId = !ModsPendingEggId(&id.high, &id.low);
        ModsClearPendingEggId();
    }
    if (!RhhModsHasHook(RHH_DAYCARE_PENDING_CHANGED)) return;
    struct RhhDaycarePendingChange change = {.struct_size = sizeof(change), .cause = cause,
        .before = !!before, .after = !!after, .run_id = RhhModsRunId()};
    for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
    {
        change.parents[i].struct_size = sizeof(change.parents[i]);
        if (!DaycareParentAt(i, &change.parents[i])) change.parent_mask |= 1u << i;
    }
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_DAYCARE_PENDING_CHANGED,
        .run_seed = RhhModsRunSeed(), .args = {cause, !!before, !!after, change.parent_mask}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    sBreedingPendingContext = &change; sBreedingPendingIdContext = hasId ? &id : NULL;
    RhhModsDispatch(&event, &decision); sBreedingPendingContext = NULL; sBreedingPendingIdContext = NULL;
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
}
static u64 BreedingOfferHash(void)
{
    u64 hash = UINT64_C(14695981039346656037);
    const u8 *parts[] = {(const u8 *)&gSaveBlock1Ptr->daycare, (const u8 *)gParties[B_TRAINER_PLAYER],
        gSaveBlock2Ptr->playerName, gSaveBlock2Ptr->playerTrainerId, &gSaveBlock2Ptr->playerGender};
    const size_t lengths[] = {sizeof(gSaveBlock1Ptr->daycare), sizeof(gParties[B_TRAINER_PLAYER]),
        sizeof(gSaveBlock2Ptr->playerName), sizeof(gSaveBlock2Ptr->playerTrainerId), sizeof(gSaveBlock2Ptr->playerGender)};
    for (u32 part = 0; part < ARRAY_COUNT(parts); ++part)
        for (size_t i = 0; i < lengths[part]; ++i) { hash ^= parts[part][i]; hash *= UINT64_C(1099511628211); }
    u64 run = RhhModsRunId();
    for (u32 i = 0; i < 8; ++i) { hash ^= (u8)(run >> (i * 8)); hash *= UINT64_C(1099511628211); }
    hash ^= !!FlagGet(FLAG_PENDING_DAYCARE_EGG); hash *= UINT64_C(1099511628211);
    u64 high, low;
    if (FlagGet(FLAG_PENDING_DAYCARE_EGG) && !ModsPendingEggId(&high, &low))
    {
        for (u32 i = 0; i < 8; ++i) { hash ^= (u8)(high >> (i * 8)); hash *= UINT64_C(1099511628211); }
        for (u32 i = 0; i < 8; ++i) { hash ^= (u8)(low >> (i * 8)); hash *= UINT64_C(1099511628211); }
    }
    hash ^= gPartiesCount[B_TRAINER_PLAYER]; hash *= UINT64_C(1099511628211);
    hash ^= GetConfig(NATURE_INHERITANCE); hash *= UINT64_C(1099511628211);
    hash ^= GetConfig(BALL_INHERITANCE); hash *= UINT64_C(1099511628211);
    hash ^= GetConfig(ABILITY_INHERITANCE); hash *= UINT64_C(1099511628211);
    return hash;
}
static int BreedingOffer(struct RhhBreedingOffer *out)
{
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || !gSaveBlock1Ptr || !gSaveBlock2Ptr
        || sBreedingOfferBusy || sBreedingMovePreviewBusy || sBreedingContext || sBreedingIvCountContext || sBreedingNatureContext || sBreedingTraitContext
        || sBreedingIvRuleContext || sBreedingMoveRuleContext || sBreedingRngPolicyContext
        || sBreedingPreviewActive || NativeSaveTransactionActive()) return -1;
    sBreedingOfferBusy = TRUE;
    u64 token = BreedingOfferHash(); rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBreedingOffer result = {.struct_size = sizeof(result), .run_id = RhhModsRunId(),
        .pending = !!FlagGet(FLAG_PENDING_DAYCARE_EGG), .token = token};
    for (u32 i = 0; i < PARTY_SIZE; ++i)
    {
        struct Pokemon copy = gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(&copy, MON_DATA_SANITY_HAS_SPECIES)) ++result.party_count;
    }
    for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
    {
        result.parents[i].struct_size = sizeof(result.parents[i]);
        if (!DaycareParentAt(i, &result.parents[i])) ++result.parent_count;
    }
    if (result.parent_count == DAYCARE_MON_COUNT)
    {
        result.preview.struct_size = sizeof(result.preview);
        if (BreedingPreview(&result.preview)) { sBreedingOfferBusy = FALSE; return -1; }
        result.compatibility = result.preview.compatibility;
    }
    bool32 unchanged = token == BreedingOfferHash() && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    sBreedingOfferBusy = FALSE;
    if (!unchanged) return -1;
    *out = result; return 0;
}
static int BreedingIvRules(const struct RhhDaycareParent parents[2], u8 rules[RHH_STAT_COUNT])
{
    memset(rules, RHH_BREED_IV_ENGINE, RHH_STAT_COUNT);
    if (!RhhModsHasHook(RHH_BREED_IV_RULE)) return 0;
    if (!parents || !RhhModsHasActiveRun() || !gSaveBlock1Ptr || !gSaveBlock2Ptr
        || sBreedingIvRuleContext || sBreedingMoveRuleContext || sBreedingContext || sBreedingIvCountContext || sBreedingNatureContext || sBreedingTraitContext
        || sBreedingRngPolicyContext
        || NativeSaveTransactionActive()) return -1;
    struct RhhBreedingIvRuleContext context = {.struct_size = sizeof(context)};
    memcpy(context.parents, parents, sizeof(context.parents));
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    u64 run = RhhModsRunId(), token = BreedingOfferHash();
    for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
    {
        context.stat = stat;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BREED_IV_RULE,
            .run_seed = RhhModsRunSeed(), .event_key = parents[0].mon.species,
            .subject = parents[1].mon.species, .ordinal = stat};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = RHH_BREED_IV_ENGINE};
        sBreedingIvRuleContext = &context; RhhModsDispatch(&event, &decision); sBreedingIvRuleContext = NULL;
        if (RhhModsRunId() != run || BreedingOfferHash() != token
            || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
        { gRngValue = rng; gRng2Value = rng2; return -1; }
        if (decision.allowed && decision.value <= RHH_BREED_IV_INITIAL) rules[stat] = decision.value;
    }
    return 0;
}
static int BreedingAcceptContext(struct RhhBreedingAcceptResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBreedingAcceptContext
        || RhhModsCurrentEvent() != RHH_CAN_ACCEPT_DAYCARE_EGG) return -1;
    *out = *sBreedingAcceptContext; return 0;
}
static int BreedingAcceptFailure(struct RhhBreedingAcceptResult *result, u32 status, const char *reason)
{
    result->status = status; snprintf(result->reason, sizeof(result->reason), "%s", reason ? reason : "");
    return status;
}
static bool32 BreedingAcceptReady(void)
{
    return RhhModsHasActiveRun() && RhhModsCommandsAllowed() && gSaveBlock1Ptr && gSaveBlock2Ptr && !gMain.inBattle
        && !NativeSaveTransactionActive() && gMain.callback2 == CB2_Overworld
        && !gPaletteFade.active && !gPaletteFade.y && !ScriptContext_IsEnabled()
        && !ArePlayerFieldControlsLocked() && !IsOverworldLinkActive();
}
static bool32 SameBreedingOffer(const struct RhhBreedingOffer *a, const struct RhhBreedingOffer *b)
{
    if (a->run_id != b->run_id || a->token != b->token || a->pending != b->pending
        || a->party_count != b->party_count || a->parent_count != b->parent_count
        || a->compatibility != b->compatibility || a->preview.candidate_count > 2
        || a->preview.base_compatibility != b->preview.base_compatibility
        || a->preview.compatibility != b->preview.compatibility || a->preview.egg_pending != b->preview.egg_pending
        || a->preview.candidate_count != b->preview.candidate_count) return FALSE;
    for (u32 i = 0; i < a->preview.candidate_count; ++i)
        if (a->preview.candidates[i].original_offspring != b->preview.candidates[i].original_offspring
            || a->preview.candidates[i].offspring != b->preview.candidates[i].offspring
            || a->preview.candidates[i].parent_slots[0] != b->preview.candidates[i].parent_slots[0]
            || a->preview.candidates[i].parent_slots[1] != b->preview.candidates[i].parent_slots[1]) return FALSE;
    return TRUE;
}
static int BreedingOffspringOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingOffspringOdds *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !out || out->struct_size < sizeof(*out)) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBreedingOffer current = {.struct_size = sizeof(current)};
    if (BreedingOffer(&current) || !SameBreedingOffer(expected, &current)
        || current.parent_count != DAYCARE_MON_COUNT || current.compatibility == RHH_DAYCARE_INCOMPATIBLE
        || !current.preview.candidate_count || current.preview.candidate_count > 2) goto fail;
    struct RhhBreedingOffspringOdds result = {.struct_size = sizeof(result), .denominator = 2};
    if (RhhModsHasHook(RHH_BREED_OFFSPRING) || RhhModsHasHook(RHH_BREED_COMPATIBILITY))
        result.flags |= RHH_BREED_OFFSPRING_ODDS_RULES_ACTIVE;
    for (u32 i = 0; i < current.preview.candidate_count; ++i)
    {
        RhhContentKey key = current.preview.candidates[i].offspring;
        if (!key) goto fail;
        u32 j = 0; while (j < result.count && result.outcomes[j].offspring != key) ++j;
        if (j == result.count) { result.outcomes[j].offspring = key; ++result.count; }
        result.outcomes[j].weight += current.preview.candidate_count == 1 ? 2 : 1;
    }
    if (RhhModsRunId() != current.run_id || BreedingOfferHash() != current.token
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))) goto fail;
    *out = result; return 0;
fail:
    gRngValue = rng; gRng2Value = rng2; return -1;
}
static int BreedingRngPolicyContext(struct RhhBreedingRngPolicy *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBreedingRngPolicyContext
        || RhhModsCurrentEvent() != RHH_BREED_RNG_POLICY) return -1;
    *out = *sBreedingRngPolicyContext; return 0;
}
static int ResolveBreedingRngPolicy(struct RhhBreedingRngPolicy *out)
{
    if (!out || !gSaveBlock1Ptr || !gSaveBlock2Ptr || !RhhModsHasActiveRun()
        || NativeSaveTransactionActive() || sBreedingRngPolicyContext || sBreedingContext || sBreedingIvCountContext
        || sBreedingNatureContext || sBreedingTraitContext || sBreedingIvRuleContext || sBreedingMoveRuleContext
        || sBreedingPreviewActive || sBreedingMovePreviewBusy || sBreedingOfferBusy) return -1;
    struct RhhBreedingRngPolicy policy = {.struct_size = sizeof(policy), .id.struct_size = sizeof(policy.id)};
    if (BreedingPendingId(&policy.id)) return -1;
    for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
    {
        policy.parents[i].struct_size = sizeof(policy.parents[i]);
        if (DaycareParentAt(i, &policy.parents[i])) return -1;
    }
    if (RhhModsHasHook(RHH_BREED_RNG_POLICY))
    {
        u64 token = BreedingOfferHash(), run = RhhModsRunId();
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BREED_RNG_POLICY,
            .run_seed = RhhModsRunSeed(), .event_key = policy.parents[0].mon.species,
            .subject = policy.parents[1].mon.species};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = RHH_BREED_RNG_ENGINE};
        sBreedingRngPolicyContext = &policy; RhhModsDispatch(&event, &decision); sBreedingRngPolicyContext = NULL;
        if (run != RhhModsRunId() || token != BreedingOfferHash()
            || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
        { gRngValue = rng; gRng2Value = rng2; return -1; }
        if (decision.allowed && decision.value <= RHH_BREED_RNG_PENDING) policy.mode = decision.value;
    }
    *out = policy; return 0;
}
static int BreedingRngPolicy(const struct RhhBreedingOffer *expected, struct RhhBreedingRngPolicy *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !out || out->struct_size < sizeof(*out)) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBreedingOffer current = {.struct_size = sizeof(current)};
    struct RhhBreedingRngPolicy policy;
    if (BreedingOffer(&current) || !SameBreedingOffer(expected, &current) || !current.pending
        || current.parent_count != DAYCARE_MON_COUNT || current.compatibility == RHH_DAYCARE_INCOMPATIBLE
        || ResolveBreedingRngPolicy(&policy)
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
    { gRngValue = rng; gRng2Value = rng2; return -1; }
    *out = policy; return 0;
}
static void SeedBreedingRng(const struct RhhBreedingRngPolicy *policy, struct ModsBreedingRngScope *scope)
{
    u64 seed = RhhModRandomAt(RhhModsRunSeed(), "rhh:breeding_engine", policy->id.run_id, policy->id.high);
    scope->saved = gRngValue; scope->saved2 = gRng2Value; scope->active = TRUE;
    gRngValue = LocalRandomSeed((u32)RhhModRandomAt(seed, "rhh:breeding_engine", policy->id.low, 0));
    SeedRng2((u32)RhhModRandomAt(seed, "rhh:breeding_engine", policy->id.low, 1));
}
void ModsBreedingRngBegin(struct DayCare *daycare, struct ModsBreedingRngScope *scope)
{
    if (!scope) return;
    *scope = (struct ModsBreedingRngScope){0};
    if (!gSaveBlock1Ptr || daycare != &gSaveBlock1Ptr->daycare || !RhhModsHasHook(RHH_BREED_RNG_POLICY)) return;
    struct RhhBreedingRngPolicy policy;
    if (ResolveBreedingRngPolicy(&policy) || policy.mode != RHH_BREED_RNG_PENDING) return;
    SeedBreedingRng(&policy, scope);
}
void ModsBreedingRngEnd(struct ModsBreedingRngScope *scope)
{
    if (!scope || !scope->active) return;
    gRngValue = scope->saved; gRng2Value = scope->saved2; scope->active = FALSE;
}
static int BuildBreedingExact(struct Pokemon *egg, u32 original, const u8 slots[DAYCARE_MON_COUNT],
                              const struct RhhDaycareEggId *id, struct RhhBreedingExactPreview *out)
{
    struct RhhBreedingExactPreview result = {.struct_size = sizeof(result), .pending_id = *id,
        .egg.struct_size = sizeof(result.egg)};
    if (Snapshot(&egg->box, RHH_MON_STAGED, 0, 0, &result.egg) || !result.egg.is_egg) return -1;
    result.original_offspring = RhhContentKeyForId(RHH_SPECIES, original);
    result.ability_slot = GetMonData(egg, MON_DATA_ABILITY_NUM);
    u32 ability = GetAbilityBySpeciesReadOnly(GetMonData(egg, MON_DATA_SPECIES), result.ability_slot);
    u32 gender = GetMonGender(egg);
    result.ability = ability ? RhhContentKeyForId(RHH_ABILITY, ability) : 0;
    if (!result.original_offspring || (ability && !result.ability)) return -1;
    result.ball = GetMonData(egg, MON_DATA_POKEBALL);
    if (result.ball < POKEBALL_COUNT) result.ball_item = RhhContentKeyForId(RHH_ITEM, gPokeBalls[result.ball].itemId);
    result.gender = gender == MON_MALE ? RHH_GENDER_MALE : gender == MON_FEMALE ? RHH_GENDER_FEMALE : RHH_GENDER_GENDERLESS;
    result.shiny = !!GetMonData(egg, MON_DATA_IS_SHINY); result.parent_slots[0] = slots[0]; result.parent_slots[1] = slots[1];
    for (u32 i = 0; i < MAX_MON_MOVES; ++i)
    {
        u32 move = GetMonData(egg, MON_DATA_MOVE1 + i);
        if (move && !(result.moves[i] = MonMoveKey(move))) return -1;
    }
    *out = result; return 0;
}
static bool32 SameBreedingExact(const struct RhhBreedingExactPreview *a, const struct RhhBreedingExactPreview *b)
{
    return a->ball == b->ball && a->ball_item == b->ball_item && a->ability_slot == b->ability_slot
        && a->gender == b->gender && a->shiny == b->shiny && a->original_offspring == b->original_offspring
        && a->ability == b->ability && a->pending_id.run_id == b->pending_id.run_id
        && a->pending_id.high == b->pending_id.high && a->pending_id.low == b->pending_id.low
        && !memcmp(a->parent_slots, b->parent_slots, sizeof(a->parent_slots))
        && !memcmp(a->moves, b->moves, sizeof(a->moves)) && RhhMonSnapshotsEqual(&a->egg, &b->egg);
}
static int BreedingExactPreview(const struct RhhBreedingOffer *expected, struct RhhBreedingExactPreview *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !out || out->struct_size < RHH_BREEDING_EXACT_PREVIEW_V1_18_SIZE || gMain.inBattle || !RhhModsCommandsAllowed()
        || sBreedingPreviewActive || sBreedingMovePreviewBusy
        || RhhModsHasHook(RHH_BREED_INHERIT_IV) || RhhModsHasHook(RHH_BREED_INHERIT_MOVE)) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct BattleScripting scripting = gBattleScripting; enum Ability lastAbility = gLastUsedAbility;
    struct RhhBreedingRngPolicy policy = {.struct_size = sizeof(policy)};
    if (BreedingRngPolicy(expected, &policy) || policy.mode != RHH_BREED_RNG_PENDING) goto fail;
    struct RhhBreedingExactPreview result;
    struct ModsBreedingRngScope scope = {0}; struct Pokemon egg; u32 original; u8 slots[DAYCARE_MON_COUNT];
    SeedBreedingRng(&policy, &scope); sBreedingPreviewActive = TRUE;
    bool32 built = GetDaycareExactEggPreview(&egg, &original, slots);
    sBreedingPreviewActive = FALSE; ModsBreedingRngEnd(&scope);
    if (!built || RhhModsRunId() != expected->run_id || BreedingOfferHash() != expected->token
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))
        || BuildBreedingExact(&egg, original, slots, &policy.id, &result)) goto fail;
    if (RhhModsRunId() != expected->run_id || BreedingOfferHash() != expected->token
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))) goto fail;
    gBattleScripting = scripting; gLastUsedAbility = lastAbility;
    size_t size = out->struct_size >= sizeof(result) ? sizeof(result) : RHH_BREEDING_EXACT_PREVIEW_V1_18_SIZE;
    result.struct_size = size; memcpy(out, &result, size); return 0;
fail:
    gRngValue = rng; gRng2Value = rng2; gBattleScripting = scripting; gLastUsedAbility = lastAbility; return -1;
}
static int BreedingRequestContext(struct RhhBreedingRequestResult *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sBreedingRequestContext
        || (event != RHH_CAN_REQUEST_DAYCARE_EGG && event != RHH_DAYCARE_EGG_REQUESTED)) return -1;
    *out = *sBreedingRequestContext; return 0;
}
static int BreedingRequestFailure(struct RhhBreedingRequestResult *result, u32 status, const char *reason)
{
    result->status = status; snprintf(result->reason, sizeof(result->reason), "%s", reason ? reason : "");
    return status;
}
static int BreedingRequest(const struct RhhBreedingOffer *expected, struct RhhBreedingRequestResult *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_BREED_REQUEST_INVALID;
    struct RhhBreedingRequestResult result = {.struct_size = sizeof(result), .offer.struct_size = sizeof(result.offer)};
    u32 status;
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved)
        status = BreedingRequestFailure(&result, RHH_BREED_REQUEST_INVALID, "invalid offer");
    else if (sBreedingRequestBusy || sBreedingAcceptBusy || !BreedingAcceptReady())
        status = BreedingRequestFailure(&result, RHH_BREED_REQUEST_UNAVAILABLE, "daycare commands unavailable");
    else if (BreedingOffer(&result.offer))
        status = BreedingRequestFailure(&result, RHH_BREED_REQUEST_UNAVAILABLE, "daycare state unavailable");
    else if (!SameBreedingOffer(expected, &result.offer))
        status = BreedingRequestFailure(&result, RHH_BREED_REQUEST_STALE, "daycare offer changed");
    else if (result.offer.pending)
        status = BreedingRequestFailure(&result, RHH_BREED_REQUEST_ALREADY_PENDING, "an egg is already waiting");
    else if (result.offer.parent_count != DAYCARE_MON_COUNT || result.offer.compatibility == RHH_DAYCARE_INCOMPATIBLE
             || !result.offer.preview.candidate_count)
        status = BreedingRequestFailure(&result, RHH_BREED_REQUEST_INCOMPATIBLE, "daycare parents are incompatible");
    else
    {
        sBreedingRequestBusy = TRUE;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_REQUEST_DAYCARE_EGG,
            .run_seed = RhhModsRunSeed(), .event_key = result.offer.parents[0].mon.species,
            .subject = result.offer.parents[1].mon.species, .ordinal = result.offer.token,
            .args = {result.offer.compatibility, result.offer.party_count, 0, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        sBreedingRequestContext = &result; RhhModsDispatch(&event, &decision); sBreedingRequestContext = NULL;
        bool32 rngChanged = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
        gRngValue = rng; gRng2Value = rng2;
        if (!decision.allowed)
            status = BreedingRequestFailure(&result, RHH_BREED_REQUEST_DENIED, decision.reason);
        else
        {
            struct RhhBreedingOffer verified = {.struct_size = sizeof(verified)};
            if (rngChanged || !BreedingAcceptReady() || BreedingOffer(&verified) || !SameBreedingOffer(&result.offer, &verified)
                || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
                status = BreedingRequestFailure(&result, RHH_BREED_REQUEST_STALE, "daycare state changed during validation");
            else
            {
                TriggerPendingDaycareEggForMod();
                status = result.status = RHH_BREED_REQUEST_OK;
                event.kind = RHH_DAYCARE_EGG_REQUESTED;
                rng_value_t notifyRng = gRngValue, notifyRng2 = gRng2Value; enum Ability last = gLastUsedAbility;
                sBreedingRequestContext = &result; RhhModsDispatch(&event, &decision); sBreedingRequestContext = NULL;
                gRngValue = notifyRng; gRng2Value = notifyRng2; gLastUsedAbility = last;
            }
        }
        sBreedingRequestBusy = FALSE;
    }
    *out = result; return status;
}
static int BreedingMoveLearnable(u32 species, u32 move);
static int BreedingPureMoveRules(const struct RhhDaycareParent parents[2], u32 compatibility,
                                 RhhContentKey original, RhhContentKey offspring, const u8 parentSlots[2],
                                 u32 species, const u16 baseline[4], u16 out[4])
{
    memcpy(out, baseline, 4 * sizeof(*out));
    if (!RhhModsHasHook(RHH_BREED_MOVE_RULE)) return 0;
    if (!parents || !parentSlots || !RhhModsHasActiveRun() || !gSaveBlock1Ptr || !gSaveBlock2Ptr
        || sBreedingMoveRuleContext || sBreedingIvRuleContext || sBreedingContext || sBreedingIvCountContext
        || sBreedingNatureContext || sBreedingTraitContext || sBreedingRngPolicyContext || NativeSaveTransactionActive()) return -1;
    struct RhhBreedingMoveRuleContext context = {.struct_size = sizeof(context), .compatibility = compatibility,
        .original_offspring = original, .offspring = offspring, .parent_slots = {parentSlots[0], parentSlots[1]}};
    memcpy(context.parents, parents, sizeof(context.parents));
    for (u32 i = 0; i < 4; ++i)
        if (baseline[i] && !(context.base_moves[i] = MonMoveKey(baseline[i]))) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    u64 run = RhhModsRunId(), token = BreedingOfferHash();
    RhhContentKey choices[4];
    for (u32 slot = 0; slot < 4; ++slot)
    {
        context.slot = slot;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BREED_MOVE_RULE,
            .run_seed = RhhModsRunSeed(), .event_key = offspring, .subject = original,
            .ordinal = slot, .args = {compatibility, parentSlots[0], parentSlots[1]}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = context.base_moves[slot]};
        sBreedingMoveRuleContext = &context; RhhModsDispatch(&event, &decision); sBreedingMoveRuleContext = NULL;
        if (RhhModsRunId() != run || BreedingOfferHash() != token
            || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
        { gRngValue = rng; gRng2Value = rng2; return -1; }
        choices[slot] = decision.allowed ? decision.value : context.base_moves[slot];
    }
    u16 proposed[4] = {0}; u32 count = 0; bool32 valid = TRUE;
    for (u32 i = 0; i < 4 && valid; ++i)
    {
        RhhContentKey key = choices[i]; u32 move;
        if (!key) continue;
        if (RhhContentResolve(RHH_MOVE, key, &move))
        {
            const struct RhhModMove *custom = RhhModsFindCustomMove(key);
            if (!ModsCustomMoveAvailable(custom) || RhhModsMoveSlot(key, 0, &move)) { valid = FALSE; break; }
        }
        else if (!move || move >= MOVES_COUNT) { valid = FALSE; break; }
        if (move != baseline[i])
        {
            bool32 known = FALSE;
            for (u32 p = 0; p < 2; ++p)
                for (u32 j = 0; j < 4; ++j) known |= parents[p].moves[j] == key;
            if (!known || !BreedingMoveLearnable(species, move)) { valid = FALSE; break; }
        }
        for (u32 j = 0; j < count; ++j) if (proposed[j] == move) valid = FALSE;
        if (valid) proposed[count++] = move;
    }
    if (RhhModsRunId() != run || BreedingOfferHash() != token
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
    { gRngValue = rng; gRng2Value = rng2; return -1; }
    if (valid && count) memcpy(out, proposed, sizeof(proposed));
    return 0;
}
static int BreedingMovePreview(const struct RhhBreedingOffer *expected, struct RhhBreedingMovePreview *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !out || out->struct_size < sizeof(*out) || sBreedingMovePreviewBusy) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBreedingOffer current = {.struct_size = sizeof(current)};
    if (BreedingOffer(&current) || !SameBreedingOffer(expected, &current)
        || current.parent_count != DAYCARE_MON_COUNT || current.compatibility == RHH_DAYCARE_INCOMPATIBLE
        || !current.preview.candidate_count || current.preview.candidate_count > 2) goto fail;
    sBreedingMovePreviewBusy = TRUE;
    struct RhhBreedingMovePreview result = {.struct_size = sizeof(result),
        .flags = RhhModsHasHook(RHH_BREED_INHERIT_MOVE) ? RHH_BREED_MOVE_PREVIEW_HOOKS_ACTIVE : 0,
        .candidate_count = current.preview.candidate_count};
    bool32 valid = TRUE;
    for (u32 i = 0; i < result.candidate_count && valid; ++i)
    {
        const struct RhhBreedingPreviewCandidate *candidate = &current.preview.candidates[i];
        struct RhhBreedingMovePreviewCandidate *entry = &result.candidates[i];
        u32 species; u16 moves[4]; u8 slots[2];
        if (candidate->parent_slots[0] >= DAYCARE_MON_COUNT || candidate->parent_slots[1] >= DAYCARE_MON_COUNT
            || candidate->parent_slots[0] == candidate->parent_slots[1]
            || RhhContentResolve(RHH_SPECIES, candidate->offspring, &species) || !species || species >= NUM_SPECIES)
        { valid = FALSE; break; }
        slots[0] = candidate->parent_slots[0]; slots[1] = candidate->parent_slots[1];
        if (!GetDaycareMovePreview(&gSaveBlock1Ptr->daycare, species, slots, moves)) { valid = FALSE; break; }
        u16 resolved[4];
        if (BreedingPureMoveRules(current.parents, current.compatibility, candidate->original_offspring,
                                  candidate->offspring, slots, species, moves, resolved)) { valid = FALSE; break; }
        memcpy(moves, resolved, sizeof(moves));
        entry->original_offspring = candidate->original_offspring; entry->offspring = candidate->offspring;
        entry->parent_slots[0] = slots[0]; entry->parent_slots[1] = slots[1];
        for (u32 j = 0; j < ARRAY_COUNT(moves); ++j)
        {
            if (!moves[j]) continue;
            entry->moves[j] = moves[j] < MOVES_COUNT_ALL ? RhhContentKeyForId(RHH_MOVE, moves[j]) : RhhModsMoveKey(moves[j]);
            if (!entry->moves[j]) { valid = FALSE; break; }
        }
    }
    valid = valid && RhhModsRunId() == current.run_id && BreedingOfferHash() == current.token
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    sBreedingMovePreviewBusy = FALSE;
    if (!valid) goto fail;
    *out = result; return 0;
fail:
    gRngValue = rng; gRng2Value = rng2;
    return -1;
}
static int BreedingBallOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingBallOdds *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !out || out->struct_size < sizeof(*out) || sBreedingMovePreviewBusy) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBreedingOffer current = {.struct_size = sizeof(current)};
    if (BreedingOffer(&current) || !SameBreedingOffer(expected, &current)
        || current.parent_count != DAYCARE_MON_COUNT || current.compatibility == RHH_DAYCARE_INCOMPATIBLE
        || !current.preview.candidate_count || current.preview.candidate_count > 2) goto fail;
    u32 generation = GetConfig(BALL_INHERITANCE); u8 weights[3];
    struct DayCare copy = gSaveBlock1Ptr->daycare;
    GetDaycareBallSourceWeights(&copy, generation, weights);
    struct RhhBreedingBallOdds result = {.struct_size = sizeof(result), .generation = generation + 1,
        .denominator = 2, .candidate_count = current.preview.candidate_count};
    for (u32 i = 0; i < 3; ++i) result.source_weights[i] = weights[i];
    bool32 valid = weights[0] + weights[1] + weights[2] == 2;
    sBreedingMovePreviewBusy = TRUE;
    for (u32 i = 0; i < result.candidate_count && valid; ++i)
    {
        const struct RhhBreedingPreviewCandidate *candidate = &current.preview.candidates[i];
        struct RhhBreedingBallOddsCandidate *entry = &result.candidates[i]; u32 species;
        if (candidate->parent_slots[0] >= DAYCARE_MON_COUNT || candidate->parent_slots[1] >= DAYCARE_MON_COUNT
            || candidate->parent_slots[0] == candidate->parent_slots[1] || !candidate->original_offspring
            || RhhContentResolve(RHH_SPECIES, candidate->offspring, &species) || !species || species >= NUM_SPECIES)
        { valid = FALSE; break; }
        entry->original_offspring = candidate->original_offspring; entry->offspring = candidate->offspring;
        entry->parent_slots[0] = candidate->parent_slots[0]; entry->parent_slots[1] = candidate->parent_slots[1];
        struct RhhBreedingTraitContext context = {.struct_size = sizeof(context), .compatibility = current.compatibility,
            .parent_slots = {entry->parent_slots[0], entry->parent_slots[1]},
            .original_offspring = entry->original_offspring, .offspring = entry->offspring,
            .parents = {current.parents[0], current.parents[1]}};
        for (u32 source = 0; source < 3 && valid; ++source)
        {
            if (!weights[source]) continue;
            u32 ball = source == 2 ? BALL_POKE : GetBoxMonData(&copy.mons[source].mon, MON_DATA_POKEBALL);
            if (ball == BALL_MASTER || ball == BALL_CHERISH || ball == BALL_STRANGE) ball = BALL_POKE;
            if (ball >= POKEBALL_COUNT) { valid = FALSE; break; }
            ball = BreedingTraitResolve(&context, RHH_BREED_BALL_RULE, ball, species);
            RhhContentKey item = RhhContentKeyForId(RHH_ITEM, gPokeBalls[ball].itemId);
            if (!item) { valid = FALSE; break; }
            u32 j = 0; while (j < entry->count && entry->balls[j].ball != ball) ++j;
            if (j == entry->count) { entry->balls[j].ball = ball; entry->balls[j].item = item; ++entry->count; }
            entry->balls[j].weight += weights[source];
        }
    }
    valid = valid && RhhModsRunId() == current.run_id && BreedingOfferHash() == current.token
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    sBreedingMovePreviewBusy = FALSE;
    if (!valid) goto fail;
    *out = result; return 0;
fail:
    gRngValue = rng; gRng2Value = rng2; return -1;
}
static int BreedingAbilityOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingAbilityOdds *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !out || out->struct_size < sizeof(*out) || sBreedingMovePreviewBusy) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBreedingOffer current = {.struct_size = sizeof(current)};
    if (BreedingOffer(&current) || !SameBreedingOffer(expected, &current)
        || current.parent_count != DAYCARE_MON_COUNT || current.compatibility == RHH_DAYCARE_INCOMPATIBLE
        || !current.preview.candidate_count || current.preview.candidate_count > 2) goto fail;
    u32 generation = GetConfig(ABILITY_INHERITANCE), inherited = 0, percent = 0; s32 parent = -1;
    struct DayCare copy = gSaveBlock1Ptr->daycare;
    GetDaycareAbilityInheritance(&copy, generation, &parent, &inherited, &percent);
    bool32 valid = parent >= -1 && parent < DAYCARE_MON_COUNT && inherited < 4 && percent <= 100;
    struct RhhBreedingAbilityOdds result = {.struct_size = sizeof(result), .generation = generation + 1,
        .denominator = 200, .candidate_count = current.preview.candidate_count,
        .parent_slot = parent < 0 ? UINT32_MAX : (u32)parent, .inherited_slot = inherited, .inherited_percent = percent};
    sBreedingMovePreviewBusy = TRUE;
    for (u32 i = 0; i < result.candidate_count && valid; ++i)
    {
        const struct RhhBreedingPreviewCandidate *candidate = &current.preview.candidates[i];
        struct RhhBreedingAbilityOddsCandidate *entry = &result.candidates[i]; u32 species;
        if (candidate->parent_slots[0] >= DAYCARE_MON_COUNT || candidate->parent_slots[1] >= DAYCARE_MON_COUNT
            || candidate->parent_slots[0] == candidate->parent_slots[1] || !candidate->original_offspring
            || RhhContentResolve(RHH_SPECIES, candidate->offspring, &species) || !species || species >= NUM_SPECIES)
        { valid = FALSE; break; }
        entry->original_offspring = candidate->original_offspring; entry->offspring = candidate->offspring;
        entry->parent_slots[0] = candidate->parent_slots[0]; entry->parent_slots[1] = candidate->parent_slots[1];
        struct RhhBreedingTraitContext context = {.struct_size = sizeof(context), .compatibility = current.compatibility,
            .parent_slots = {entry->parent_slots[0], entry->parent_slots[1]},
            .original_offspring = entry->original_offspring, .offspring = entry->offspring,
            .parents = {current.parents[0], current.parents[1]}};
        u32 weights[4] = {0}, initialWeight = 100 - percent;
        if (GetSpeciesAbility(species, 1)) weights[0] = weights[1] = initialWeight;
        else weights[0] = 2 * initialWeight;
        if (parent >= 0) weights[inherited] += 2 * percent;
        for (u32 base = 0; base < ARRAY_COUNT(weights) && valid; ++base)
        {
            if (!weights[base]) continue;
            u32 slot = BreedingTraitResolve(&context, RHH_BREED_ABILITY_RULE, base, species);
            if (slot >= ARRAY_COUNT(weights)) { valid = FALSE; break; }
            u32 ability = GetAbilityBySpeciesReadOnly(species, slot);
            RhhContentKey key = ability == ABILITY_NONE ? 0 : RhhContentKeyForId(RHH_ABILITY, ability);
            if (ability != ABILITY_NONE && !key) { valid = FALSE; break; }
            u32 j = 0; while (j < entry->count && entry->abilities[j].slot != slot) ++j;
            if (j == entry->count) { entry->abilities[j].slot = slot; entry->abilities[j].ability = key; ++entry->count; }
            entry->abilities[j].weight += weights[base];
        }
    }
    valid = valid && RhhModsRunId() == current.run_id && BreedingOfferHash() == current.token
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    sBreedingMovePreviewBusy = FALSE;
    if (!valid) goto fail;
    *out = result; return 0;
fail:
    gRngValue = rng; gRng2Value = rng2; return -1;
}
static int BreedingNatureOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingNatureOdds *out)
{
    _Static_assert(NUM_NATURES == RHH_NATURE_COUNT, "public nature odds must cover every engine nature");
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !out || out->struct_size < sizeof(*out)) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBreedingOffer current = {.struct_size = sizeof(current)};
    if (BreedingOffer(&current) || !SameBreedingOffer(expected, &current)
        || current.parent_count != DAYCARE_MON_COUNT || current.compatibility == RHH_DAYCARE_INCOMPATIBLE
        || current.parents[0].mon.nature >= NUM_NATURES || current.parents[1].mon.nature >= NUM_NATURES) goto fail;
    u32 generation = GetConfig(NATURE_INHERITANCE); u8 source[3];
    GetDaycareNatureSourceWeights(&gSaveBlock1Ptr->daycare, generation, source);
    struct RhhBreedingNatureOdds result = {.struct_size = sizeof(result), .generation = generation + 1,
        .denominator = 4 * NUM_NATURES, .source_denominator = 4};
    for (u32 i = 0; i < 3; ++i) result.source_weights[i] = source[i];
    for (u32 i = 0; i < 3; ++i)
    {
        if (!source[i]) continue;
        u32 base = i == 2 ? NATURE_RANDOM : current.parents[i].mon.nature;
        u32 resolved = ModsBreedingNature(&gSaveBlock1Ptr->daycare, base);
        if (resolved == NATURE_RANDOM)
            for (u32 nature = 0; nature < NUM_NATURES; ++nature) result.weights[nature] += source[i];
        else if (resolved < NUM_NATURES) result.weights[resolved] += source[i] * NUM_NATURES;
        else goto fail;
    }
    if (RhhModsRunId() != current.run_id || BreedingOfferHash() != current.token
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))) goto fail;
    *out = result; return 0;
fail:
    gRngValue = rng; gRng2Value = rng2; return -1;
}

/* Count uniformly distributed 32-bit personalities, including the engine's
 * rejection of personalities that do not match a selected nature. For a
 * low byte b, p=b+256*k and 256 is 6 modulo 25; 21 is its inverse. */
static void BreedingGenderDistribution(u32 species, u32 nature, u32 *female, u32 *genderless)
{
    _Static_assert(NUM_NATURES == 25, "gender model assumes 25 personality natures");
    const u64 combinations = UINT64_C(1) << 24, personalities = UINT64_C(1) << 32;
    u64 total = nature == NATURE_RANDOM ? personalities : (personalities - 1 - nature) / NUM_NATURES + 1;
    u64 femaleCount = 0, genderlessCount = 0;
    for (u32 low = 0; low < 256; ++low)
    {
        u64 count = combinations;
        if (nature != NATURE_RANDOM)
        {
            u32 first = ((nature + NUM_NATURES - low % NUM_NATURES) * 21) % NUM_NATURES;
            count = (combinations - 1 - first) / NUM_NATURES + 1;
        }
        u32 gender = GetGenderFromSpeciesAndPersonality(species, low);
        if (gender == MON_FEMALE) femaleCount += count;
        else if (gender == MON_GENDERLESS) genderlessCount += count;
    }
    *female = (femaleCount * UINT64_C(1000000000) + total / 2) / total;
    *genderless = (genderlessCount * UINT64_C(1000000000) + total / 2) / total;
}
static int BreedingGenderOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingGenderOdds *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !out || out->struct_size < sizeof(*out)) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBreedingOffer current = {.struct_size = sizeof(current)};
    if (BreedingOffer(&current) || !SameBreedingOffer(expected, &current)
        || current.parent_count != DAYCARE_MON_COUNT || current.compatibility == RHH_DAYCARE_INCOMPATIBLE
        || !current.preview.candidate_count || current.preview.candidate_count > 2
        || current.parents[0].mon.nature >= NUM_NATURES || current.parents[1].mon.nature >= NUM_NATURES) goto fail;
    u8 sources[3]; u32 natures[3] = {NATURE_RANDOM, NATURE_RANDOM, NATURE_RANDOM};
    GetDaycareNatureSourceWeights(&gSaveBlock1Ptr->daycare, GetConfig(NATURE_INHERITANCE), sources);
    if ((u32)sources[0] + sources[1] + sources[2] != 4) goto fail;
    for (u32 i = 0; i < 3; ++i)
    {
        if (!sources[i]) continue;
        u32 base = i == 2 ? NATURE_RANDOM : current.parents[i].mon.nature;
        natures[i] = ModsBreedingNature(&gSaveBlock1Ptr->daycare, base);
        if (natures[i] > NATURE_RANDOM) goto fail;
    }
    struct RhhBreedingGenderOdds result = {.struct_size = sizeof(result), .denominator = 1000000000,
        .candidate_count = current.preview.candidate_count};
    if (RhhModsHasHook(RHH_BREED_OFFSPRING) || RhhModsHasHook(RHH_BREED_COMPATIBILITY))
        result.flags |= RHH_BREED_OFFSPRING_ODDS_RULES_ACTIVE;
    for (u32 i = 0; i < result.candidate_count; ++i)
    {
        const struct RhhBreedingPreviewCandidate *candidate = &current.preview.candidates[i];
        struct RhhBreedingGenderOddsCandidate *entry = &result.candidates[i]; u32 species;
        if (candidate->parent_slots[0] >= DAYCARE_MON_COUNT || candidate->parent_slots[1] >= DAYCARE_MON_COUNT
            || candidate->parent_slots[0] == candidate->parent_slots[1] || !candidate->original_offspring
            || RhhContentResolve(RHH_SPECIES, candidate->offspring, &species) || !species || species >= NUM_SPECIES) goto fail;
        entry->original_offspring = candidate->original_offspring; entry->offspring = candidate->offspring;
        entry->parent_slots[0] = candidate->parent_slots[0]; entry->parent_slots[1] = candidate->parent_slots[1];
        u64 female = 0, genderless = 0;
        for (u32 j = 0; j < 3; ++j)
        {
            if (!sources[j]) continue;
            u32 f, g; BreedingGenderDistribution(species, natures[j], &f, &g);
            female += (u64)sources[j] * f; genderless += (u64)sources[j] * g;
        }
        entry->female = (female + 2) / 4; entry->genderless = (genderless + 2) / 4;
        if ((u64)entry->female + entry->genderless > result.denominator) goto fail;
        entry->male = result.denominator - entry->female - entry->genderless;
    }
    if (RhhModsRunId() != current.run_id || BreedingOfferHash() != current.token
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))) goto fail;
    *out = result; return 0;
fail:
    gRngValue = rng; gRng2Value = rng2; return -1;
}

static int BreedingNatureGenderOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingNatureGenderOdds *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !out || out->struct_size < sizeof(*out)) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBreedingOffer current = {.struct_size = sizeof(current)};
    struct RhhBreedingNatureOdds natureOdds = {.struct_size = sizeof(natureOdds)};
    if (BreedingOffer(&current) || !SameBreedingOffer(expected, &current)
        || current.parent_count != DAYCARE_MON_COUNT || current.compatibility == RHH_DAYCARE_INCOMPATIBLE
        || !current.preview.candidate_count || current.preview.candidate_count > 2
        || BreedingNatureOdds(expected, &natureOdds) || !natureOdds.denominator) goto fail;
    const u64 scale = UINT64_C(1000000000);
    struct RhhBreedingNatureGenderOdds result = {.struct_size = sizeof(result),
        .candidate_count = current.preview.candidate_count, .denominator = scale * natureOdds.denominator};
    if (RhhModsHasHook(RHH_BREED_OFFSPRING) || RhhModsHasHook(RHH_BREED_COMPATIBILITY))
        result.flags |= RHH_BREED_OFFSPRING_ODDS_RULES_ACTIVE;
    for (u32 i = 0; i < result.candidate_count; ++i)
    {
        const struct RhhBreedingPreviewCandidate *candidate = &current.preview.candidates[i];
        struct RhhBreedingNatureGenderCandidate *entry = &result.candidates[i]; u32 species;
        if (candidate->parent_slots[0] >= DAYCARE_MON_COUNT || candidate->parent_slots[1] >= DAYCARE_MON_COUNT
            || candidate->parent_slots[0] == candidate->parent_slots[1] || !candidate->original_offspring
            || RhhContentResolve(RHH_SPECIES, candidate->offspring, &species) || !species || species >= NUM_SPECIES) goto fail;
        entry->original_offspring = candidate->original_offspring; entry->offspring = candidate->offspring;
        entry->parent_slots[0] = candidate->parent_slots[0]; entry->parent_slots[1] = candidate->parent_slots[1];
        u32 total = 0;
        for (u32 nature = 0; nature < NUM_NATURES; ++nature)
        {
            u32 weight = natureOdds.weights[nature], female, genderless; total += weight;
            if (!weight) continue;
            BreedingGenderDistribution(species, nature, &female, &genderless);
            if ((u64)female + genderless > scale) goto fail;
            entry->female[nature] = (u64)weight * female;
            entry->genderless[nature] = (u64)weight * genderless;
            entry->male[nature] = (u64)weight * (scale - female - genderless);
        }
        if (total != natureOdds.denominator) goto fail;
    }
    if (RhhModsRunId() != current.run_id || BreedingOfferHash() != current.token
        || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2))) goto fail;
    *out = result; return 0;
fail:
    gRngValue = rng; gRng2Value = rng2; return -1;
}

static int BreedingIvPreview(const struct RhhBreedingOffer *expected, struct RhhBreedingIvPreview *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhBreedingOffer current = {.struct_size = sizeof(current)};
    if (BreedingOffer(&current) || !SameBreedingOffer(expected, &current)
        || current.parent_count != DAYCARE_MON_COUNT || current.compatibility == RHH_DAYCARE_INCOMPATIBLE) return -1;
    struct DaycareIVRules rules; GetDaycareIVRules(&gSaveBlock1Ptr->daycare, &rules);
    rules.inheritedCount = ModsBreedingIvCount(&gSaveBlock1Ptr->daycare, rules.inheritedCount);
    u8 sourceRules[RHH_STAT_COUNT];
    if (BreedingIvRules(current.parents, sourceRules)) return -1;
    struct RhhBreedingIvPreview result = {.struct_size = sizeof(result), .inherited_count = rules.inheritedCount};
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i) result.values[i] = UINT32_MAX;
    if (RhhModsHasHook(RHH_BREED_INHERIT_IV)) result.flags |= RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE;
    else if (rules.inheritedCount && (rules.powerItemCount == 1 || (rules.powerItemCount == 2 && rules.powerStats[0] == rules.powerStats[1])))
    {
        u32 slot = rules.powerStats[0] < RHH_STAT_COUNT ? 0 : 1, stat = rules.powerStats[slot];
        if (stat >= RHH_STAT_COUNT) return -1;
        result.guaranteed_stats = 1u << stat;
        result.values[stat] = 1u << current.parents[slot].mon.ivs[stat];
        if (rules.powerItemCount == 2) result.values[stat] |= 1u << current.parents[1].mon.ivs[stat];
    }
    if (!(result.flags & RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE) && rules.inheritedCount == RHH_STAT_COUNT)
        for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
            if (!(result.guaranteed_stats & (1u << stat)))
                result.values[stat] = (1u << current.parents[0].mon.ivs[stat]) | (1u << current.parents[1].mon.ivs[stat]);
    for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
    {
        if (sourceRules[stat] == RHH_BREED_IV_PARENT_0 || sourceRules[stat] == RHH_BREED_IV_PARENT_1)
        {
            u32 iv = current.parents[sourceRules[stat] - RHH_BREED_IV_PARENT_0].mon.ivs[stat];
            if (iv >= 32) return -1;
            result.values[stat] = 1u << iv; result.guaranteed_stats |= 1u << stat;
        }
        else if (sourceRules[stat] == RHH_BREED_IV_INITIAL)
        { result.values[stat] = UINT32_MAX; result.guaranteed_stats &= ~(1u << stat); }
    }
    *out = result; return 0;
}
static int BreedingIvOdds(const struct RhhBreedingOffer *expected, struct RhhBreedingIvOdds *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhBreedingOffer current = {.struct_size = sizeof(current)};
    if (BreedingOffer(&current) || !SameBreedingOffer(expected, &current)
        || current.parent_count != DAYCARE_MON_COUNT || current.compatibility == RHH_DAYCARE_INCOMPATIBLE) return -1;
    struct DaycareIVRules rules; GetDaycareIVRules(&gSaveBlock1Ptr->daycare, &rules);
    rules.inheritedCount = ModsBreedingIvCount(&gSaveBlock1Ptr->daycare, rules.inheritedCount);
    if (rules.inheritedCount > RHH_STAT_COUNT || rules.powerItemCount > 2) return -1;
    if (!rules.inheritedCount) rules.powerItemCount = 0;
    u8 sourceRules[RHH_STAT_COUNT];
    if (BreedingIvRules(current.parents, sourceRules)) return -1;
    struct RhhBreedingIvOdds result = {.struct_size = sizeof(result), .inherited_count = rules.inheritedCount};
    if (RhhModsHasHook(RHH_BREED_INHERIT_IV))
        result.flags = RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE;
    else
    {
        /* 960 is divisible by the random-IV, parent and stat-selection
         * denominators for all supported power-item and Destiny Knot cases. */
        result.denominator = 960;
        u32 branches = rules.powerItemCount == 2 ? 2 : 1;
        for (u32 branch = 0; branch < branches; ++branch)
        {
            u32 forced = RHH_STAT_COUNT, parent = 0;
            if (rules.powerItemCount)
            {
                parent = branches == 2 ? branch : (rules.powerStats[0] < RHH_STAT_COUNT ? 0 : 1);
                forced = rules.powerStats[parent];
                if (forced >= RHH_STAT_COUNT) return -1;
            }
            for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
            {
                u32 iv0 = current.parents[0].mon.ivs[stat], iv1 = current.parents[1].mon.ivs[stat];
                if (iv0 >= 32 || iv1 >= 32) return -1;
                if (stat == forced)
                    result.weights[stat][current.parents[parent].mon.ivs[stat]] += 960 / branches;
                else
                {
                    u32 parentWeight = forced == RHH_STAT_COUNT ? 80 * rules.inheritedCount
                        : 96 * (rules.inheritedCount - 1);
                    u32 randomWeight = forced == RHH_STAT_COUNT ? 5 * (6 - rules.inheritedCount)
                        : 6 * (6 - rules.inheritedCount);
                    for (u32 value = 0; value < 32; ++value)
                        result.weights[stat][value] += randomWeight / branches;
                    result.weights[stat][iv0] += parentWeight / branches;
                    result.weights[stat][iv1] += parentWeight / branches;
                }
            }
        }
        for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
        {
            if (sourceRules[stat] == RHH_BREED_IV_ENGINE) continue;
            memset(result.weights[stat], 0, sizeof(result.weights[stat]));
            if (sourceRules[stat] == RHH_BREED_IV_INITIAL)
                for (u32 value = 0; value < 32; ++value) result.weights[stat][value] = result.denominator / 32;
            else
            {
                u32 iv = current.parents[sourceRules[stat] - RHH_BREED_IV_PARENT_0].mon.ivs[stat];
                if (iv >= 32) return -1;
                result.weights[stat][iv] = result.denominator;
            }
        }
    }
    *out = result; return 0;
}
static int BreedingIvJointOdds(const struct RhhBreedingOffer *expected, const struct RhhBreedingIvJointRequest *request,
                               struct RhhBreedingIvJointOdds *out)
{
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || !request || request->struct_size < sizeof(*request) || request->reserved[0] || request->reserved[1] || request->reserved[2]
        || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhBreedingOffer current = {.struct_size = sizeof(current)};
    if (BreedingOffer(&current) || !SameBreedingOffer(expected, &current)
        || current.parent_count != DAYCARE_MON_COUNT || current.compatibility == RHH_DAYCARE_INCOMPATIBLE) return -1;
    struct DaycareIVRules rules; GetDaycareIVRules(&gSaveBlock1Ptr->daycare, &rules);
    rules.inheritedCount = ModsBreedingIvCount(&gSaveBlock1Ptr->daycare, rules.inheritedCount);
    if (rules.inheritedCount > RHH_STAT_COUNT || rules.powerItemCount > 2) return -1;
    if (!rules.inheritedCount) rules.powerItemCount = 0;
    u8 sourceRules[RHH_STAT_COUNT];
    if (BreedingIvRules(current.parents, sourceRules)) return -1;
    struct RhhBreedingIvJointOdds result = {.struct_size = sizeof(result), .inherited_count = rules.inheritedCount};
    if (RhhModsHasHook(RHH_BREED_INHERIT_IV))
        result.flags = RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE;
    else
    {
        for (u32 parent = 0; parent < 2; ++parent)
            for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
                if (current.parents[parent].mon.ivs[stat] >= 32) return -1;
        u32 parentChoices = rules.inheritedCount == RHH_STAT_COUNT ? 64 : 32;
        bool32 fullRandom = FALSE;
        for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat) fullRandom |= sourceRules[stat] != RHH_BREED_IV_ENGINE;
        result.denominator = parentChoices * 720u * (UINT64_C(1) << (5 * (fullRandom ? RHH_STAT_COUNT : RHH_STAT_COUNT - rules.inheritedCount)));
        for (u32 parentBits = 0; parentBits < parentChoices; ++parentBits)
            for (u32 permutation = 0; permutation < 720; ++permutation)
            {
                u32 bits = parentBits, selection = permutation, start = 0;
                u8 available[RHH_STAT_COUNT] = {0, 1, 2, 3, 4, 5};
                u8 source[RHH_STAT_COUNT] = {2, 2, 2, 2, 2, 2};
                if (rules.powerItemCount)
                {
                    u32 parent = rules.powerStats[1] < RHH_STAT_COUNT ? 1 : 0;
                    if (rules.powerItemCount == 2) { parent = bits & 1; bits >>= 1; }
                    u32 stat = rules.powerStats[parent];
                    if (stat >= RHH_STAT_COUNT) return -1;
                    source[stat] = parent;
                    for (u32 i = stat; i + 1 < RHH_STAT_COUNT; ++i) available[i] = available[i + 1];
                    start = 1;
                }
                for (u32 i = start; i < rules.inheritedCount; ++i)
                {
                    u32 length = RHH_STAT_COUNT - i, index = selection % length;
                    selection /= length;
                    u32 stat = available[index]; source[stat] = bits & 1; bits >>= 1;
                    for (u32 j = index; j + 1 < length; ++j) available[j] = available[j + 1];
                }
                u64 weight = 1;
                for (u32 stat = 0; stat < RHH_STAT_COUNT && weight; ++stat)
                {
                    if (sourceRules[stat] == RHH_BREED_IV_INITIAL) source[stat] = 2;
                    else if (sourceRules[stat] != RHH_BREED_IV_ENGINE)
                        source[stat] = sourceRules[stat] - RHH_BREED_IV_PARENT_0;
                    if (source[stat] == 2) weight *= __builtin_popcount(request->values[stat]);
                    else if (!(request->values[stat] & (1u << current.parents[source[stat]].mon.ivs[stat]))) weight = 0;
                    else if (fullRandom) weight *= 32;
                }
                result.numerator += weight;
            }
    }
    *out = result; return 0;
}
struct BreedingExactValidation {
    const struct RhhBreedingOffer *offer;
    const struct RhhBreedingExactPreview *expected;
    rng_value_t rng, rng2;
    bool32 visited;
};
static bool32 ValidateBreedingExact(struct Pokemon *egg, u32 original, const u8 *slots, void *data)
{
    struct BreedingExactValidation *check = data;
    struct RhhDaycareEggId id = {.struct_size = sizeof(id)};
    struct RhhBreedingExactPreview actual;
    check->visited = TRUE;
    return BreedingAcceptReady() && RhhModsRunId() == check->offer->run_id
        && !memcmp(&check->rng, &gRngValue, sizeof(check->rng))
        && !memcmp(&check->rng2, &gRng2Value, sizeof(check->rng2))
        && BreedingOfferHash() == check->offer->token && !BreedingPendingId(&id)
        && !BuildBreedingExact(egg, original, slots, &id, &actual)
        && SameBreedingExact(check->expected, &actual)
        && !memcmp(&check->rng, &gRngValue, sizeof(check->rng))
        && !memcmp(&check->rng2, &gRng2Value, sizeof(check->rng2))
        && BreedingOfferHash() == check->offer->token;
}
static int BreedingAcceptCore(const struct RhhBreedingOffer *expected,
                              const struct RhhBreedingExactPreview *expectedExact,
                              struct RhhBreedingAcceptResult *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_BREED_ACCEPT_INVALID;
    struct RhhBreedingAcceptResult result = {.struct_size = sizeof(result), .party_slot = UINT32_MAX,
        .offer.struct_size = sizeof(result.offer), .egg.struct_size = sizeof(result.egg)};
    u32 status;
    if (!expected || expected->struct_size < sizeof(*expected) || expected->reserved
        || (expectedExact && (expectedExact->struct_size < sizeof(*expectedExact)
            || expectedExact->egg.struct_size < sizeof(expectedExact->egg)
            || expectedExact->pending_id.struct_size < sizeof(expectedExact->pending_id)
            || expectedExact->pending_id.reserved || expectedExact->reserved[0]
            || expectedExact->reserved[1] || expectedExact->reserved[2])))
        status = BreedingAcceptFailure(&result, RHH_BREED_ACCEPT_INVALID, "invalid offer or preview");
    else if (sBreedingAcceptBusy || !BreedingAcceptReady())
        status = BreedingAcceptFailure(&result, RHH_BREED_ACCEPT_UNAVAILABLE, "daycare commands unavailable");
    else if (BreedingOffer(&result.offer))
        status = BreedingAcceptFailure(&result, RHH_BREED_ACCEPT_UNAVAILABLE, "daycare state unavailable");
    else if (!SameBreedingOffer(expected, &result.offer))
        status = BreedingAcceptFailure(&result, RHH_BREED_ACCEPT_STALE, "daycare offer changed");
    else if (!result.offer.pending)
        status = BreedingAcceptFailure(&result, RHH_BREED_ACCEPT_NO_EGG, "no egg is waiting");
    else if (result.offer.party_count >= PARTY_SIZE)
        status = BreedingAcceptFailure(&result, RHH_BREED_ACCEPT_PARTY_FULL, "party is full");
    else if (result.offer.parent_count != DAYCARE_MON_COUNT || result.offer.compatibility == RHH_DAYCARE_INCOMPATIBLE)
        status = BreedingAcceptFailure(&result, RHH_BREED_ACCEPT_INCOMPATIBLE, "daycare parents are incompatible");
    else
    {
        sBreedingAcceptBusy = TRUE;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_ACCEPT_DAYCARE_EGG,
            .run_seed = RhhModsRunSeed(), .event_key = result.offer.parents[0].mon.species,
            .subject = result.offer.parents[1].mon.species, .ordinal = result.offer.token,
            .args = {result.offer.compatibility, result.offer.party_count, 0, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        sBreedingAcceptContext = &result; RhhModsDispatch(&event, &decision); sBreedingAcceptContext = NULL;
        bool32 rngChanged = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
        gRngValue = rng; gRng2Value = rng2;
        if (!decision.allowed)
            status = BreedingAcceptFailure(&result, RHH_BREED_ACCEPT_DENIED, decision.reason);
        else
        {
            struct RhhBreedingOffer verified = {.struct_size = sizeof(verified)};
            if (rngChanged || !BreedingAcceptReady() || BreedingOffer(&verified) || !SameBreedingOffer(&result.offer, &verified)
                || memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
                status = BreedingAcceptFailure(&result, RHH_BREED_ACCEPT_STALE, "daycare state changed during validation");
            else
            {
                struct RhhBreedingExactPreview current = {.struct_size = sizeof(current)};
                if (expectedExact && (BreedingExactPreview(&verified, &current) || !SameBreedingExact(expectedExact, &current)))
                    status = BreedingAcceptFailure(&result, RHH_BREED_ACCEPT_STALE, "expected egg changed");
                else
                {
                    u32 slot = UINT32_MAX;
                    rng_value_t beforeRng = gRngValue, beforeRng2 = gRng2Value;
                    struct BreedingExactValidation check = {&verified, expectedExact, beforeRng, beforeRng2, FALSE};
                    struct BattleScripting scripting = gBattleScripting; enum Ability lastAbility = gLastUsedAbility;
                    bool32 received = expectedExact ? GiveEggFromDaycareValidated(&slot, ValidateBreedingExact, &check)
                                                     : GiveEggFromDaycareChecked(&slot);
                    if (!received)
                    {
                        if (expectedExact)
                        { gRngValue = beforeRng; gRng2Value = beforeRng2; gBattleScripting = scripting; gLastUsedAbility = lastAbility; }
                        status = BreedingAcceptFailure(&result, check.visited ? RHH_BREED_ACCEPT_STALE : RHH_BREED_ACCEPT_INCOMPATIBLE,
                            check.visited ? "generated egg changed" : "engine did not accept the egg");
                    }
                    else
                    {
                        result.party_slot = slot; result.egg_valid = !PartyAt(slot, &result.egg) && result.egg.is_egg;
                        status = result.status = RHH_BREED_ACCEPT_OK;
                    }
                }
            }
        }
        sBreedingAcceptBusy = FALSE;
    }
    *out = result; return status;
}
static int BreedingAccept(const struct RhhBreedingOffer *expected, struct RhhBreedingAcceptResult *out)
{ return BreedingAcceptCore(expected, NULL, out); }
static int BreedingAcceptExact(const struct RhhBreedingOffer *expected, const struct RhhBreedingExactPreview *exact,
                               struct RhhBreedingAcceptResult *out)
{
    if (!exact)
    {
        if (out && out->struct_size >= sizeof(*out))
        { struct RhhBreedingAcceptResult result = {.struct_size = sizeof(result), .party_slot = UINT32_MAX};
          BreedingAcceptFailure(&result, RHH_BREED_ACCEPT_INVALID, "invalid preview"); *out = result; }
        return RHH_BREED_ACCEPT_INVALID;
    }
    return BreedingAcceptCore(expected, exact, out);
}
static int BreedingEggContext(struct RhhBreedingContext *context, struct DayCare *daycare,
                              struct Pokemon *egg, u32 originalSpecies, u32 compatibility,
                              const u8 *parentSlots, u32 location, u32 slot)
{
    if (!egg || !parentSlots || parentSlots[0] >= 2 || parentSlots[1] >= 2
        || BreedingPrepare(daycare, GetDaycareCompatibilityScoreBase(daycare), compatibility, context)) return -1;
    context->parent_slots[0] = parentSlots[0]; context->parent_slots[1] = parentSlots[1];
    context->original_offspring = RhhContentKeyForId(RHH_SPECIES, originalSpecies);
    context->offspring_mon.struct_size = sizeof(context->offspring_mon);
    if (Snapshot(&egg->box, location, 0, slot, &context->offspring_mon)) return -1;
    context->offspring = context->offspring_mon.species;
    context->egg_identity = context->offspring_mon.identity;
    struct BoxPokemon copy = egg->box;
    for (unsigned i = 0; i < 4; ++i)
    {
        u32 move = GetBoxMonData(&copy, MON_DATA_MOVE1 + i);
        if (move && !(context->offspring_moves[i] = MonMoveKey(move))) return -1;
    }
    return 0;
}
void ModsBreedingInheritIVs(struct DayCare *daycare, struct Pokemon *egg, u32 originalSpecies,
                            u32 compatibility, const u8 *parentSlots, const u8 *initialIVs)
{
    struct RhhBreedingContext context;
    if (!initialIVs || BreedingEggContext(&context, daycare, egg, originalSpecies, compatibility,
                                          parentSlots, RHH_MON_STAGED, 0)) return;
    for (unsigned i = 0; i < RHH_STAT_COUNT; ++i)
    {
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BREED_INHERIT_IV,
            .run_seed = RhhModsRunSeed(), .event_key = context.offspring, .subject = context.egg_identity,
            .ordinal = i, .args = {initialIVs[i], context.offspring_mon.ivs[i], context.compatibility}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = RHH_BREED_IV_ENGINE};
        sBreedingContext = &context; RhhModsDispatch(&event, &decision); sBreedingContext = NULL;
        if (!decision.allowed || decision.value == RHH_BREED_IV_ENGINE || decision.value > RHH_BREED_IV_INITIAL) continue;
        u32 value = decision.value == RHH_BREED_IV_INITIAL ? initialIVs[i]
            : context.parents[decision.value - RHH_BREED_IV_PARENT_0].mon.ivs[i];
        SetMonData(egg, MON_DATA_HP_IV + i, &value);
    }
    u8 rules[RHH_STAT_COUNT];
    if (BreedingIvRules(context.parents, rules)) return;
    for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
    {
        if (rules[stat] == RHH_BREED_IV_ENGINE) continue;
        u32 value = rules[stat] == RHH_BREED_IV_INITIAL ? initialIVs[stat]
            : context.parents[rules[stat] - RHH_BREED_IV_PARENT_0].mon.ivs[stat];
        SetMonData(egg, MON_DATA_HP_IV + stat, &value);
    }
}
static int BreedingMoveLearnable(u32 species, u32 move)
{
    if (SpeciesHasEggMove(species, move) || CanLearnTeachableMove(species, move)) return 1;
    const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(species);
    for (unsigned i = 0; learnset[i].move != LEVEL_UP_MOVE_END; ++i)
        if (learnset[i].move == move) return 1;
    return 0;
}
void ModsBreedingInheritMoves(struct DayCare *daycare, struct Pokemon *egg, u32 originalSpecies,
                              u32 compatibility, const u8 *parentSlots)
{
    struct RhhBreedingContext context;
    if (BreedingEggContext(&context, daycare, egg, originalSpecies, compatibility,
                           parentSlots, RHH_MON_STAGED, 0)) return;
    u32 species = GetMonData(egg, MON_DATA_SPECIES), proposed[4] = {0}, baseline[4];
    u16 initial[4], resolved[4];
    for (u32 i = 0; i < 4; ++i) initial[i] = GetMonData(egg, MON_DATA_MOVE1 + i);
    if (!BreedingPureMoveRules(context.parents, context.compatibility, context.original_offspring, context.offspring,
                               parentSlots, species, initial, resolved) && memcmp(initial, resolved, sizeof(initial)))
    {
        for (u32 i = 0; i < 4; ++i) SetMonMoveSlot(egg, resolved[i], i);
        u32 bonuses = 0; SetMonData(egg, MON_DATA_PP_BONUSES, &bonuses);
        if (BreedingEggContext(&context, daycare, egg, originalSpecies, compatibility,
                               parentSlots, RHH_MON_STAGED, 0)) return;
    }
    for (unsigned i = 0; i < 4; ++i) baseline[i] = GetMonData(egg, MON_DATA_MOVE1 + i);
    unsigned count = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BREED_INHERIT_MOVE,
            .run_seed = RhhModsRunSeed(), .event_key = context.offspring, .subject = context.egg_identity,
            .ordinal = i, .args = {i, context.compatibility}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = context.offspring_moves[i]};
        sBreedingContext = &context; RhhModsDispatch(&event, &decision); sBreedingContext = NULL;
        RhhContentKey key = decision.allowed ? decision.value : context.offspring_moves[i];
        if (!key) continue;
        u32 move;
        if (RhhContentResolve(RHH_MOVE, key, &move))
        {
            const struct RhhModMove *custom = RhhModsFindCustomMove(key);
            if (!ModsCustomMoveAvailable(custom) || RhhModsMoveSlot(key, 0, &move)) return;
        }
        else if (!move || move >= MOVES_COUNT) return;
        if (move != baseline[i])
        {
            int parentKnows = 0;
            for (unsigned p = 0; p < 2; ++p)
                for (unsigned j = 0; j < 4; ++j)
                {
                    if (context.parents[p].moves[j] == key) parentKnows = 1;
                }
            if (!parentKnows || !BreedingMoveLearnable(species, move)) return;
        }
        for (unsigned j = 0; j < count; ++j) if (proposed[j] == move) return;
        proposed[count++] = move;
    }
    if (!count || !memcmp(proposed, baseline, sizeof(proposed))) return;
    for (unsigned i = 0; i < 4; ++i) SetMonMoveSlot(egg, proposed[i], i);
    u32 bonuses = 0; SetMonData(egg, MON_DATA_PP_BONUSES, &bonuses);
}
void ModsBreedingEggCreated(struct DayCare *daycare, struct Pokemon *egg, u32 originalSpecies,
                            u32 compatibility, const u8 *parentSlots, u32 partySlot, bool32 hadPending, u64 ordinal)
{
    struct RhhBreedingContext context;
    if (BreedingEggContext(&context, daycare, egg, originalSpecies, compatibility,
                           parentSlots, RHH_MON_PARTY, partySlot) || !context.offspring_mon.is_egg) return;
    context.egg_valid = 1;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BREED_EGG_CREATED,
        .run_seed = RhhModsRunSeed(), .event_key = context.offspring, .subject = context.egg_identity,
        .args = {context.compatibility, parentSlots[0], parentSlots[1]}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = context.offspring};
    struct RhhDaycareEggOrdinal sequence = {.struct_size = sizeof(sequence), .party_slot = partySlot,
        .run_id = RhhModsRunId(), .ordinal = ordinal};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; enum Ability last = gLastUsedAbility;
    sDaycareOrdinalContext = ordinal ? &sequence : NULL;
    sBreedingEggHadPending = hadPending; sBreedingContext = &context;
    RhhModsDispatch(&event, &decision); sBreedingContext = NULL; sBreedingEggHadPending = FALSE; sDaycareOrdinalContext = NULL;
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = last;
}
static uint32_t DaycareSlots(void) { return DAYCARE_MON_COUNT; }
static const struct RhhDaycareService sDaycareService = {
    {sizeof(sDaycareService), RHH_SERVICE_VERSION(1, 5)}, DaycareSlots, DaycareCurrent, DaycareParentAt,
    DaycareTiming, DaycareEggProgress, HatchEligibility, HatchEligibilityContext, EggCyclePreview, EggCycleContext,
    EggCycleChangeContext
};
static uint32_t OwnedPartyView(void)
{ return sRecordedPartyActive ? RHH_OWNED_PARTY_RECORDED : sRentalPartyActive ? RHH_OWNED_PARTY_RENTAL
    : sUnionPartyActive ? (sLinkProjectionKind == LINK_PROJECTION_MULTI ? RHH_OWNED_PARTY_LINK_MULTI_BATTLE
    : sLinkProjectionKind == LINK_PROJECTION_FRONTIER ? RHH_OWNED_PARTY_FRONTIER : RHH_OWNED_PARTY_UNION_BATTLE)
    : ModsOwnedFollowerPartyActive() ? RHH_OWNED_PARTY_FOLLOWER_BATTLE
    : ModsOwnedSkyPartyActive() ? RHH_OWNED_PARTY_SKY_BATTLE
    : ModsOwnedBattlePartyViewActive() ? RHH_OWNED_PARTY_BATTLE_MENU : RHH_OWNED_PARTY_FIELD_ORDER; }
static int OwnedProjectedPartyFieldSlot(uint32_t viewSlot, uint32_t *out, u8 kind)
{
    if (!out || !RhhModsHasActiveRun() || !sSkyPartyActive || sPartyProjectionKind != kind || sSkyPartyPreparing || !sSkyPartyValid
        || (kind == PROJECTION_FOLLOWER && sFollowerCatchFull)
        || NativeSaveTransactionActive() || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive()) return -1;
    uint32_t reducedSlot = viewSlot; u64 identity;
    if (ModsOwnedBattlePartyViewActive())
    {
        if (ModsOwnedBattlePartyFieldSlot(viewSlot, &reducedSlot)
            || ModsOwnedBattlePartyVisibleIdentity(viewSlot, &identity)) return -1;
    }
    else
    {
        if (viewSlot >= PARTY_SIZE || !gParties[B_TRAINER_PLAYER][viewSlot].box.hasSpecies) return -1;
        struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][viewSlot].box;
        identity = (u64)mon->otId << 32 | mon->personality;
    }
    if (reducedSlot >= sSkyPartyCount) return -1;
    uint32_t field = sSkyPartySlots[reducedSlot];
    if (identity != ((u64)sSkyPartyOt[field] << 32 | sSkyPartyPersonality[field])) return -1;
    *out = field; return 0;
}
static int OwnedSkyPartyFieldSlot(uint32_t viewSlot, uint32_t *out)
{ return OwnedProjectedPartyFieldSlot(viewSlot, out, PROJECTION_SKY); }
static int OwnedFollowerPartyFieldSlot(uint32_t viewSlot, uint32_t *out)
{ return OwnedProjectedPartyFieldSlot(viewSlot, out, PROJECTION_FOLLOWER); }
static int OwnedLinkPartyFieldSlot(uint32_t viewSlot, uint32_t *out, u8 kind)
{
    if (!out || !RhhModsHasActiveRun() || !sUnionPartyActive || !sUnionPartyValid || sUnionPartyPreparing
        || sLinkProjectionKind != kind
        || !gSaveBlock1Ptr || *GetSavedPlayerPartyCount() != sUnionSavedPartyCount
        || NativeSaveTransactionActive() || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive()) return -1;
    uint32_t reduced = viewSlot; u64 identity;
    if (ModsOwnedBattlePartyViewActive())
    {
        if (ModsOwnedBattlePartyFieldSlot(viewSlot, &reduced)
            || ModsOwnedBattlePartyVisibleIdentity(viewSlot, &identity)) return -1;
    }
    else
    {
        if (viewSlot >= sUnionPartyCount || !UnionProjectionMatches()) return -1;
        struct BoxPokemon *mon = &gParties[B_TRAINER_PLAYER][viewSlot].box;
        identity = (u64)mon->otId << 32 | mon->personality;
    }
    if (reduced >= sUnionPartyCount || identity != sUnionPartyIdentity[reduced]) return -1;
    struct BoxPokemon *saved = &GetSavedPlayerPartyMon(sUnionPartySlots[reduced])->box;
    if (!saved->hasSpecies || ((u64)saved->otId << 32 | saved->personality) != identity) return -1;
    *out = sUnionPartySlots[reduced]; return 0;
}
static int OwnedUnionPartyFieldSlot(uint32_t viewSlot, uint32_t *out)
{ return OwnedLinkPartyFieldSlot(viewSlot, out, LINK_PROJECTION_UNION); }
static int OwnedLinkMultiPartyFieldSlot(uint32_t viewSlot, uint32_t *out)
{ return OwnedLinkPartyFieldSlot(viewSlot, out, LINK_PROJECTION_MULTI); }
static int OwnedFrontierPartyFieldSlot(uint32_t viewSlot, uint32_t *out)
{ return OwnedLinkPartyFieldSlot(viewSlot, out, LINK_PROJECTION_FRONTIER); }
static int OwnedBattleViewFieldSlot(uint32_t viewSlot, uint32_t *out)
{
    if (!RhhModsHasActiveRun() || NativeSaveTransactionActive() || ModsOwnedTradePcHeld() || ModsOwnedLinkTradeActive())
        return -1;
    if (sRentalPartyActive || sRecordedPartyActive) return -1;
    if (sUnionPartyActive) return ModsOwnedBattlePartyViewActive()
        ? OwnedLinkPartyFieldSlot(viewSlot, out, sLinkProjectionKind) : -1;
    if (ModsOwnedSkyPartyActive()) return OwnedSkyPartyFieldSlot(viewSlot, out);
    if (ModsOwnedFollowerPartyActive())
        return !ModsOwnedBattlePartyViewActive() ? -1
            : ModsOwnedFollowerCatchFullActive() ? ModsOwnedBattlePartyFieldSlot(viewSlot, out)
            : OwnedFollowerPartyFieldSlot(viewSlot, out);
    return ModsOwnedBattlePartyFieldSlot(viewSlot, out);
}
static uint32_t OwnedTradeState(void)
{ return ModsOwnedLinkTradeActive() ? RHH_OWNED_TRADE_LINK_ACTIVE
    : ModsOwnedTradePcHeld() ? RHH_OWNED_TRADE_PC_HELD : RHH_OWNED_TRADE_IDLE; }
static const struct RhhOwnedMonService sService = {
    {sizeof(sService), RHH_SERVICE_VERSION(1, 46)}, PartySlots, BoxCount, BoxSlots, PartyAt, BoxAt, Find, OwnedChangeContext,
    IdentityQuery, OwnedDaycareCount, OwnedDaycareSlots, OwnedFusionSlots, OwnedDaycareAt, OwnedFusionAt, OwnedReleaseContext,
    OwnedMoveContext, OwnedPartyReorderContext, OwnedDaycareTransferContext, OwnedFusionTransferContext, OwnedPartyView,
    OwnedCatchPlacementContext, OwnedIngameTradeContext, OwnedLinkTradeContext, OwnedTradeState,
    LinkHistoryCount, LinkHistoryAt, LinkHistoryContext, LinkHistoryCheckpoint, LinkHistoryReconcile, OwnedPickupContext,
    OwnedStorageUnreadableContext, OwnedGiftContext, OwnedEvolutionContext, OwnedBattleViewFieldSlot,
    OwnedSkyPartyFieldSlot, OwnedFollowerPartyFieldSlot, OwnedExperience, OwnedExperienceChangeContext, PreviewOwnedExperience,
    PreviewOwnedExperienceMoves, AwardExperience, ExperienceAwardStatus, ExperienceAwardContext, OwnedUnionPartyFieldSlot,
    OwnedLinkMultiPartyFieldSlot, OwnedFrontierPartyFieldSlot, OwnedStats, OwnedIvView, OwnedTeraView, IdentityOccurrences
};
static void OwnedMemoryWrite(void *destination, size_t size)
{
    ModsOwnedBattlePartySummaryBeforeBytes(destination, size);
    NativeOwnedTrackUnreportedCopy(destination, NULL, size);
}
int RhhModsInstallMonService(void)
{
    NativeMemorySetWriteObserver(OwnedMemoryWrite);
    sBattleEvBusy = FALSE; sBattleEvContext = NULL; sBattleEvPolicy = NULL; sItemEvBusy = FALSE;
    sItemEvContext = NULL; sItemEvReduction = NULL;
    sEvLimitBusy = FALSE; sEvLimitContext = NULL;
    if (RhhModsSetBeforeSave(ModsBreedingReconcilePendingId)) return -1;
    if (NativeOwnedLedgerInstall()) return -1;
    if (NativeOwnedTrackerInstall()) return -1;
    if (NativeSaveRegisterArchive(&sLinkHistoryCodec)) return -1;
    if (RhhModsSetLifecycleCallback(RhhModsResetOwnedMonWatch)) return -1;
    if (RhhModsSetLifecycleCallback(ResetExperienceAward)) return -1;
    if (RhhModsAddService("rhh:service/owned_mon", &sService.header)) return -1;
    if (RhhModsAddService("rhh:service/battle_ev_yield", &sBattleEvService.header)) return -1;
    if (RhhModsAddService("rhh:service/item_ev_yield", &sItemEvService.header)) return -1;
    if (RhhModsAddService("rhh:service/ev_limits", &sEvLimitService.header)) return -1;
    if (RhhModsAddService("rhh:service/abilities", &sAbilityService.header)) return -1;
    if (RhhModsAddService("rhh:service/nature", &sNatureService.header)) return -1;
    if (RhhModsAddService("rhh:service/daycare", &sDaycareService.header)) return -1;
    if (RhhModsAddService("rhh:service/breeding", &sBreedingService.header)) return -1;
    if (RhhModsAddService("rhh:service/egg_hatch", &sEggHatchService.header)) return -1;
    if (RhhModsAddService("rhh:service/field_healing", &sFieldHealingService.header)) return -1;
    return RhhModsAddService("rhh:service/mon_edit", &sEditService.header);
}
