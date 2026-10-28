/* Engine adapter. Example mods in native/mods depend only on mod_api.h. */
#include <stdlib.h>
#include <stdio.h>
#include "global.h"
#include "mods.h"
#include "mod_host.h"
#include "mods/bundled.h"
#include "native.h"
#include "pokemon.h"
#include "battle.h"
#include "data.h"
#include "event_data.h"
#include "item.h"
#include "overworld.h"
#include "constants/region_map_sections.h"
#include "constants/maps.h"
#include "constants/form_change_types.h"
#include "mod_content.inc"

struct ModConfig gModDraft;
static u16 sStarters[NUM_SPECIES];
static u32 sStarterCount;
static u64 sSpeciesKeys[NUM_SPECIES];
static bool32 sNormalizing, sHadBalls;
static const char *sExternalDirectory;
void ModsSetExternalBuiltinDirectory(const char *directory) { sExternalDirectory = directory; }
static const char sRunId[] = "rhh:host_run";
static u64 Read64(const u8 *p) { u64 n = 0; for (u32 i = 0; i < 8; ++i) n |= (u64)p[i] << (i * 8); return n; }
static void Write64(u8 *p, u64 n) { for (u32 i = 0; i < 8; ++i) p[i] = n >> (i * 8); }
static u8 *Run(void) { return RhhModsState(sRunId); }
static u64 RunValue(u32 offset) { const u8 *s = Run(); return s ? Read64(s + offset) : 0; }
static int Persist(uint64_t run, const uint8_t *data, uint32_t size) { return NativeSaveSetJournal(run, data, size); }
static const struct RhhModDefinition sRun = {
    .struct_size = sizeof(sRun), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1, .state_schema = 1,
    .id = sRunId, .flags = RHH_MOD_REQUIRED, .state_size = 24
};
static struct RhhModEvent Event(u32 kind, u64 key, u64 subject, u64 ordinal)
{ return (struct RhhModEvent){sizeof(struct RhhModEvent), kind, RunValue(0), key, subject, ordinal, {0}}; }
static struct RhhModDecision Decision(u64 value)
{ return (struct RhhModDecision){.struct_size = sizeof(struct RhhModDecision), .allowed = 1, .value = value}; }
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
int ModsRegisterBuiltins(void)
{
    if (RhhModsSetContent(sGameContent, ARRAY_COUNT(sGameContent)) || RhhModsRegister(&sRun)) return -1;
    RhhModsSetContentMetadata(ContentMetadata); RhhModsSetPersistence(Persist);
    sStarterCount = 0;
    for (u32 i = 1; i < NUM_SPECIES; ++i)
    { sSpeciesKeys[i] = RhhContentKeyForId(RHH_SPECIES, i); if (ModsStarterEligible(i) && sSpeciesKeys[i]) sStarters[sStarterCount++] = i; }
    qsort(sStarters, sStarterCount, sizeof(*sStarters), CompareSpecies);
    const struct RhhModDefinition *(*const factories[])(const struct RhhModHostV1 *) = {
        RhhBundledNuzlocke, RhhBundledWildRandomizer, RhhBundledTrainerRandomizer, RhhBundledStarter,
        RhhBundledEqualBST, RhhBundledLevelCap, RhhBundledFollower, RhhBundledBattleMusic
    };
    static const char *const names[] = {"nuzlocke", "wild_randomizer", "trainer_randomizer", "starter",
        "equal_bst", "level_cap", "follower", "battle_music"};
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
    RhhModsResetState(); u8 *s = Run(); if (!s) return;
    u64 seed = gModDraft.seed ? gModDraft.seed : (UINT64_C(0x5248480000000000) | trainerId);
    Write64(s, seed); Write64(s + 8, RhhModRandomAt(seed, sRunId, trainerId, NativeSaveNewRunSerial()));
    RhhModsSetRunContext(seed, Read64(s + 8), 0); sHadBalls = FALSE;
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
{ return RhhContentKeyForId(RHH_MAP, (gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum); }
u32 ModsWildSpecies(u32 species, u32 kind, u32 slot, u32 time)
{
    if (!Run() || species >= NUM_SPECIES) return species;
    struct RhhModEvent e = Event(RHH_WILD_SPECIES, MapKey(), sSpeciesKeys[species], RunValue(16));
    e.args[0] = kind | (time << 16); e.args[1] = slot;
    struct RhhModDecision d = Decision(sSpeciesKeys[species]); RhhModsDispatch(&e, &d);
    return ResolveSpecies(d.value, species, FALSE);
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
    if (!Run()) return;
    RhhModsSetRunContext(RunValue(0), RunValue(8), 1);
    u32 size; const u8 *journal = NativeSaveGetJournal(RunValue(8), &size);
    if (journal && RhhModsMergeJournal(journal, size)) { fprintf(stderr, "Required mod journal rejected: %s\n", RhhModsError()); abort(); }
    struct RhhModEvent e = Event(RHH_RUN_LOADED, RunValue(8), 0, 0);
    struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d);
    for (u32 i = 0; i < PARTY_SIZE; ++i)
        if (GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_SPECIES))
        { ModsNormalizePartyMon(&gParties[B_TRAINER_PLAYER][i]); CalculateMonStats(&gParties[B_TRAINER_PLAYER][i]); }
    sHadBalls = HasAtLeastOnePokeBall();
}
void ModsAfterSave(void)
{
    if (!Run()) return;
    RhhModsSetRunContext(RunValue(0), RunValue(8), 1);
    if (RhhModsFlushDurable())
    {
        struct RhhModEvent e = Event(RHH_PERSISTENCE_FAILED, 0, 0, 0);
        struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d);
    }
}
void ModsEnforceParty(void)
{
    if (!Run()) return;
    bool32 balls = HasAtLeastOnePokeBall();
    if (balls != sHadBalls)
    {
        sHadBalls = balls; struct RhhModEvent e = Event(RHH_INVENTORY_CHANGED, RhhContentKeyOf("rhh:inventory/pokeballs"), 0, 0);
        e.args[0] = balls; struct RhhModDecision d = Decision(0); RhhModsDispatch(&e, &d);
    }
    for (u32 i = 0; i < PARTY_SIZE; ++i)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i]; if (!GetMonData(mon, MON_DATA_SPECIES)) continue;
        u32 exp = GetMonData(mon, MON_DATA_EXP);
        if (ModsClampExperience(mon, exp) != exp || GetMonData(mon, MON_DATA_LEVEL) > ModsLevelCap())
        { ModsNormalizePartyMon(mon); CalculateMonStats(mon); }
        if (ModsMonIsDead(mon) && GetMonData(mon, MON_DATA_HP)) { u16 hp = 0; SetMonData(mon, MON_DATA_HP, &hp); }
    }
}
