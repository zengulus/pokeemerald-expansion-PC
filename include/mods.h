#ifndef RHH_GAME_MODS_H
#define RHH_GAME_MODS_H
#ifdef NATIVE
#include "mod_api.h"
struct Pokemon;

enum ModRuleFlag {
    MOD_NUZLOCKE = 1 << 0, MOD_TRAINERS = 1 << 1,
    MOD_EQUALIZE = 1 << 2, MOD_LEVEL_CAP = 1 << 3, MOD_FOLLOWER = 1 << 4
};
enum ModWildMode { MOD_WILD_OFF, MOD_WILD_POOL, MOD_WILD_FULL };
struct ModConfig { u32 flags; u8 wildMode, music; u64 starter, seed; };
extern struct ModConfig gModDraft;
int ModsRegisterBuiltins(void);
void ModsSetExternalBuiltinDirectory(const char *directory);
void ModsNewGame(u32 trainerId);
void ModsAfterLoad(void);
void ModsAfterSave(void);
struct ModConfig ModsGetConfig(void);
void ModsSetMusic(u32 music);
bool32 ModsHasRule(u32 flag);
u32 ModsStarterCount(void);
u32 ModsStarterAt(u32 index);
bool32 ModsStarterEligible(u32 species);
u32 ModsWildSpecies(u32 species, u32 kind, u32 slot, u32 timeOfDay);
void ModsWildCreated(u32 species, u32 kind, u32 slot, u32 timeOfDay);
u32 ModsTrainerSpecies(u32 species, u32 trainer, u32 slot);
u32 ModsStarterSpecies(u32 original, u32 slot);
u32 ModsBaseStat(u32 species, u32 index, u32 original);
u32 ModsLevelCap(void);
bool32 ModsHardCap(void);
void ModsNormalizePartyMon(struct Pokemon *mon);
void ModsEnforceParty(void);
u32 ModsClampExperience(struct Pokemon *mon, u32 exp);
u32 ModsFilterHP(struct Pokemon *mon, u32 oldHP, u32 newHP);
bool32 ModsMonIsDead(struct Pokemon *mon);
void ModsMonFainted(struct Pokemon *mon);
void ModsEncounterStarted(void);
bool32 ModsCanCapture(void);
void ModsCaughtMon(void);
void ModsWhiteout(void);
bool32 ModsRunFailed(void);
void ModsOpenNewGameMenu(void (*resume)(void), void (*cancel)(void));
void ModsOpenRunEndedMenu(void (*back)(void));
#endif
#endif
