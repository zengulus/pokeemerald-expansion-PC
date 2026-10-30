#ifndef GUARD_DAYCARE_H
#define GUARD_DAYCARE_H

#include "constants/daycare.h"
#include "constants/species.h"

struct RecordMixingDaycareMail
{
    struct DaycareMail mail[DAYCARE_MON_COUNT];
    u32 numDaycareMons;
    bool16 cantHoldItem[DAYCARE_MON_COUNT];
};
struct DaycareIVRules
{
    u8 inheritedCount, powerItemCount;
    u8 powerStats[DAYCARE_MON_COUNT]; /* NUM_STATS when no power item. */
};

u8 *GetMonNicknameVanilla(struct Pokemon *mon, u8 *dest);
u8 *GetBoxMonNickname(struct BoxPokemon *mon, u8 *dest);
u8 CountPokemonInDaycare(struct DayCare *daycare);
void InitDaycareMailRecordMixing(struct DayCare *daycare, struct RecordMixingDaycareMail *mixMail);
s8 Daycare_FindEmptySpot(struct DayCare *daycare);
void StoreSelectedPokemonInDaycare(void);
u16 TakePokemonFromDaycare(void);
void GetDaycareCostAndPrepareString(void);
u8 GetNumLevelsGainedFromDaycare(void);
void TriggerPendingDaycareEgg(void);
void RejectEggFromDayCare(void);
void CreateEgg(struct Pokemon *mon, enum Species species, bool8 setHotSpringsLocation);
void GiveEggFromDaycare(void);
bool8 ShouldEggHatch(void);
enum Species GetSelectedMonNicknameAndSpecies(void);
void GetDaycareMonNicknames(void);
u8 GetDaycareState(void);
u8 GetDaycareCompatibilityScore(struct DayCare *daycare);
#ifdef NATIVE
/* Source weights out of four: parent 0, parent 1, random. */
void GetDaycareNatureSourceWeights(const struct DayCare *daycare, u32 generation, u8 weights[3]);
/* Source weights out of two: parent 0, parent 1, default Poké Ball. */
void GetDaycareBallSourceWeights(const struct DayCare *daycare, u32 generation, u8 weights[3]);
/* Parent slot (-1 means none), inherited ability slot and percentage. */
void GetDaycareAbilityInheritance(const struct DayCare *daycare, u32 generation, s32 *parent, u32 *ability, u32 *percent);
u8 GetDaycareCompatibilityScoreBase(struct DayCare *daycare);
u32 NativeDaycareEggChance(u32 score);
u32 NativeDaycareHatchCycleLength(void);
u32 NativeDaycareHatchCycleDecrement(void);
enum Species GetDaycareOffspringCandidate(struct DayCare *daycare, bool32 maleVariant, u8 *parentSlots);
bool32 GetDaycareMovePreview(const struct DayCare *daycare, enum Species species, const u8 *parentSlots, u16 moves[4]);
bool32 GiveEggFromDaycareChecked(u32 *partySlot);
/* Validates the staged egg before party, daycare and notification changes. */
bool32 GiveEggFromDaycareValidated(u32 *partySlot,
    bool32 (*validate)(struct Pokemon *, u32, const u8 *, void *), void *context);
bool32 GetDaycareExactEggPreview(struct Pokemon *egg, u32 *originalSpecies, u8 parentSlots[DAYCARE_MON_COUNT]);
void TriggerPendingDaycareEggForMod(void);
#endif
void SetDaycareCompatibilityString(void);
bool8 NameHasGenderSymbol(const u8 *name, u8 genderRatio);
void ShowDaycareLevelMenu(void);
void ChooseSendDaycareMon(void);
void StorePokemonInDaycare(struct Pokemon *mon, struct DaycareMon *daycareMon);
void IncrementDaycareSteps(void);
bool32 CheckDaycareMonReceivedMail(void);
enum Species GetEggSpecies(enum Species species);
u32 GetChildNature(struct DayCare *daycare);
void InheritIVs(struct Pokemon *egg, struct DayCare *daycare);
void GetDaycareIVRules(struct DayCare *daycare, struct DaycareIVRules *out);
void InheritAbility(struct Pokemon *egg, struct DayCare *daycare);

#endif // GUARD_DAYCARE_H
