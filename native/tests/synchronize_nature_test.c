#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "pokemon.h"
#include "ow_abilities.h"

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)

struct Pokemon gParties[MAX_BATTLE_TRAINERS][PARTY_SIZE];
u8 gPartiesCount[MAX_BATTLE_TRAINERS];
static enum Ability sAbility = ABILITY_SYNCHRONIZE;
static unsigned sEgg, sNatureCalls, sRngCalls, sRoll;

u32 GetMonData2(struct Pokemon *mon, s32 field)
{
    CHECK(mon == &gParties[B_TRAINER_PLAYER][0]);
    CHECK(field == MON_DATA_SANITY_IS_EGG);
    return sEgg;
}
enum Ability GetMonAbility(struct Pokemon *mon)
{ CHECK(mon == &gParties[B_TRAINER_PLAYER][0]); return sAbility; }
u8 GetNature(struct Pokemon *mon)
{ CHECK(mon == &gParties[B_TRAINER_PLAYER][0]); ++sNatureCalls; return NATURE_ADAMANT; }
u32 Random32(void) { ++sRngCalls; return sRoll << 16; }

int main(void)
{
    /* The accessor is deliberately different from the legacy PID remainder.
     * This tests the consumer without enabling the staged nature codec. */
    gParties[B_TRAINER_PLAYER][0].box.personality = NATURE_MODEST;
    sAbility = ABILITY_NONE;
    CHECK(GetSynchronizedNature(WILDMON_ORIGIN, SPECIES_PIKACHU) == NATURE_RANDOM);
    CHECK(!sNatureCalls && !sRngCalls);
    sAbility = ABILITY_SYNCHRONIZE; sEgg = 1;
    CHECK(GetSynchronizedNature(WILDMON_ORIGIN, SPECIES_PIKACHU) == NATURE_RANDOM);
    CHECK(!sNatureCalls && !sRngCalls);
    sEgg = 0;
#if OW_SYNCHRONIZE_NATURE == GEN_3
    sRoll = 0;
    CHECK(GetSynchronizedNature(WILDMON_ORIGIN, SPECIES_PIKACHU) == NATURE_RANDOM);
    CHECK(sRngCalls == 1 && !sNatureCalls);
    sRoll = 1;
    CHECK(GetSynchronizedNature(WILDMON_ORIGIN, SPECIES_PIKACHU) == NATURE_ADAMANT);
    CHECK(sRngCalls == 2 && sNatureCalls == 1);
    CHECK(GetSynchronizedNature(STATIC_WILDMON_ORIGIN, SPECIES_PIKACHU) == NATURE_RANDOM);
    CHECK(sRngCalls == 2 && sNatureCalls == 1);
#elif OW_SYNCHRONIZE_NATURE == GEN_8
    CHECK(GetSynchronizedNature(WILDMON_ORIGIN, SPECIES_PIKACHU) == NATURE_ADAMANT);
    CHECK(GetSynchronizedNature(ROAMER_ORIGIN, SPECIES_PIKACHU) == NATURE_ADAMANT);
    CHECK(GetSynchronizedNature(STATIC_WILDMON_ORIGIN, SPECIES_PIKACHU) == NATURE_RANDOM);
    CHECK(GetSynchronizedNature(GIFTMON_ORIGIN, SPECIES_PIKACHU) == NATURE_RANDOM);
    CHECK(!sRngCalls && sNatureCalls == 2);
#else
    CHECK(GetSynchronizedNature(WILDMON_ORIGIN, SPECIES_PIKACHU) == NATURE_RANDOM);
    CHECK(!sNatureCalls && !sRngCalls);
#endif
    puts("Synchronize nature selection passed");
    return 0;
}
