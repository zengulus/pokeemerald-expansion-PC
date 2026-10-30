#include <stdio.h>
#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "contest.h"
#include "mods.h"
#include "mod_host.h"
#include "constants/moves.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #c); return 1; } } while (0)
void ModsContestTestSelectMove(u8 contestant);

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(argv[1]) && !RhhModsFinalize());
    struct ContestResources resources = {0}; struct ContestMoveAnimData animation = {0};
    struct ContestantStatus status[CONTESTANT_COUNT] = {0};
    resources.moveAnim = &animation; resources.status = status; gContestResources = &resources;
    gBattlerSpriteIds[B_BATTLER_3] = 3; status[0].currMove = MOVE_CURSE;
    RhhModsSetRunContext(123, 456, 0);
    gContestMons[0].species = SPECIES_TORCHIC; ModsContestTestSelectMove(0); CHECK(gAnimMoveTurn == 0);
    gContestMons[0].species = SPECIES_GENGAR; ModsContestTestSelectMove(0); CHECK(gAnimMoveTurn == 1);
    gContestMons[0].species = SPECIES_PIKACHU; ModsContestTestSelectMove(0); CHECK(gAnimMoveTurn == 1);
    RhhModsSetRunContext(0, 0, 0);
    gContestMons[0].species = SPECIES_TORCHIC; ModsContestTestSelectMove(0); CHECK(gAnimMoveTurn == 1);
    gContestMons[0].species = SPECIES_GENGAR; ModsContestTestSelectMove(0); CHECK(gAnimMoveTurn == 0);
    gContestResources = NULL; RhhModsUnloadLibraries();
    puts("contest Curse animation follows resolved and original type lists");
    return 0;
}
