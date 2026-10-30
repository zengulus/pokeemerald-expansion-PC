InteractWithOverworldWildEncounter::
    lock
    overworldwildencounterapproach
    applymovement VAR_LAST_TALKED, Common_Movement_ExclamationMark
    facetogether
    getoverworldwildencounterspecies VAR_0x8005
    playmoncry VAR_0x8005, CRY_MODE_DOUBLES
    waitmoncry
    tryoverworldwildencounter
    goto_if_eq VAR_RESULT, FALSE, InteractWithOverworldWildEncounter_Cancelled
    waitstate
    end

InteractWithOverworldWildEncounter_Cancelled:
    releaseall
    end

