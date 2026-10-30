#ifndef GUARD_BATTLE_DOME_H
#define GUARD_BATTLE_DOME_H

#include "constants/battle_dome.h"

enum DomeEffectivenessMode {
    EFFECTIVENESS_MODE_GOOD,
    EFFECTIVENESS_MODE_BAD,
    EFFECTIVENESS_MODE_AI_VS_AI,
};

int GetDomeTrainerSelectedMons(u16 tournamentTrainerId);
int TrainerIdToDomeTournamentId(u16 trainerId);
int GetDomeTypeEffectivenessPoints(enum Move move, enum Species targetSpecies, enum DomeEffectivenessMode mode);

#endif // GUARD_BATTLE_DOME_H
