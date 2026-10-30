#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "battle.h"
#include "pokemon.h"

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); exit(1); } } while (0)

struct Pokemon gParties[MAX_BATTLE_TRAINERS][PARTY_SIZE];
struct BattlePokemon gBattleMons[MAX_BATTLERS_COUNT];
u16 gBattlerPartyIndexes[MAX_BATTLERS_COUNT];
static enum BattleTrainer sTrainers[MAX_BATTLERS_COUNT];
static struct Pokemon *sExpected;
static unsigned sNatureCalls;
static u8 sNatureValue = 7, sStatNature = 3;
static int sCorrupt;

enum BattleTrainer GetBattlerTrainer(enum BattlerId battler) { return sTrainers[battler]; }
u8 GetNature(struct Pokemon *mon)
{ CHECK(sExpected && mon->box.personality == sExpected->box.personality && mon->box.otId == sExpected->box.otId); ++sNatureCalls; return sNatureValue; }
u8 GetNatureFromPersonality(u32 personality) { return personality % NUM_NATURES; }
u32 GetMonData2(struct Pokemon *mon, s32 field)
{ if (field == MON_DATA_SPECIES) { if (sCorrupt) mon->box.isBadEgg = TRUE; return 1; }
  CHECK(field == MON_DATA_HIDDEN_NATURE); return sStatNature; }
void CaptureBattleMonNature(struct BattlePokemon *battle, struct Pokemon *mon)
{
    battle->nativeNature = GetNature(mon); battle->nativeStatNature = sStatNature;
    battle->nativeNatureFlags = NATIVE_BATTLE_NATURE_SNAPSHOT | NATIVE_BATTLE_NATURE_STAT_VALID;
    battle->nativeNaturePersonality = battle->personality; battle->nativeNatureOtId = battle->otId;
}

int main(void)
{
    sTrainers[0] = B_TRAINER_PLAYER; sTrainers[1] = B_TRAINER_OPPONENT_A;
    sTrainers[2] = B_TRAINER_PARTNER; sTrainers[3] = B_TRAINER_OPPONENT_B;
    for (unsigned battler = 0; battler < MAX_BATTLERS_COUNT; ++battler)
    {
        unsigned slot = battler + 1;
        struct Pokemon *mon = &gParties[sTrainers[battler]][slot];
        mon->box.hasSpecies = TRUE; mon->box.personality = 123 + battler; mon->box.otId = 1000 + battler;
        gBattlerPartyIndexes[battler] = slot;
        gBattleMons[battler].personality = mon->box.personality; gBattleMons[battler].otId = mon->box.otId;
        sExpected = mon; sNatureCalls = 0;
        CHECK(GetBattlerNature(battler) == 7 && sNatureCalls == 1);
        mon->box.otId ^= 1; sNatureCalls = 0;
        CHECK(GetBattlerNature(battler) == gBattleMons[battler].personality % NUM_NATURES && !sNatureCalls);
        mon->box.otId ^= 1; mon->box.personality ^= 1;
        CHECK(GetBattlerNature(battler) == gBattleMons[battler].personality % NUM_NATURES && !sNatureCalls);
        mon->box.personality ^= 1; mon->box.isBadEgg = TRUE;
        CHECK(GetBattlerNature(battler) == gBattleMons[battler].personality % NUM_NATURES && !sNatureCalls);
        mon->box.isBadEgg = FALSE; mon->box.hasSpecies = FALSE;
        CHECK(GetBattlerNature(battler) == gBattleMons[battler].personality % NUM_NATURES && !sNatureCalls);
        gBattlerPartyIndexes[battler] = PARTY_SIZE;
        CHECK(GetBattlerNature(battler) == gBattleMons[battler].personality % NUM_NATURES && !sNatureCalls);
        mon->box.hasSpecies = TRUE; gBattlerPartyIndexes[battler] = slot; sNatureValue = 7;
        sCorrupt = 1; CHECK(GetBattlerNature(battler) == gBattleMons[battler].personality % NUM_NATURES
            && !mon->box.isBadEgg && !sNatureCalls); sCorrupt = 0;
        CaptureBattlerNature(battler); CHECK(sNatureCalls == 1);
        u8 stat, flags; sNatureCalls = 0;
        CHECK(GetBattlerNatureDetails(battler, &stat, &flags) == 7 && stat == 3
            && flags == (NATIVE_BATTLE_NATURE_SNAPSHOT | NATIVE_BATTLE_NATURE_STAT_VALID) && !sNatureCalls);
        /* The party can move or contain an indistinguishable PID/OT clone.
         * The battle snapshot keeps the original source until refreshed. */
        unsigned other = slot == 1 ? 0 : 1;
        struct Pokemon *clone = &gParties[sTrainers[battler]][other]; *clone = *mon;
        gBattlerPartyIndexes[battler] = other; sExpected = clone; sNatureValue = 9;
        CHECK(GetBattlerNature(battler) == 7 && !sNatureCalls);
        CaptureBattlerNature(battler); CHECK(GetBattlerNature(battler) == 9 && sNatureCalls == 1);
        clone->box.otId ^= 1; CaptureBattlerNature(battler);
        CHECK(GetBattlerNature(battler) == gBattleMons[battler].personality % NUM_NATURES);
        sNatureValue = 7;
    }
    puts("battle nature: checked player, partner and opponent provenance and fallback");
    return 0;
}
