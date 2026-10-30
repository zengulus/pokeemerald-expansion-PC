#ifndef GUARD_FISHING_H
#define GUARD_FISHING_H

void StartFishing(u8 rod);
bool32 FishingRollForBite(u32 rod, bool32 isStickyHold, bool32 legacySticky);
void UpdateChainFishingStreak();
u32 CalculateChainFishingShinyRolls(void);
bool32 ShouldUseFishingEnvironmentInBattle();

#endif // GUARD_FISHING_H
