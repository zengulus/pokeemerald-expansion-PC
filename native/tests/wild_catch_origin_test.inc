#ifdef RHH_OWNED_IDENTITY_TEST
struct CatchOriginProbeState { u32 count, linked; struct RhhWildCatchOrigin last;
    u32 ended, ended_linked; struct RhhWildActorBattleCatch battle_catch;
    u32 started, started_linked; struct RhhWildActorBattleView battle_actor;
    u32 battle_ended, battle_ended_linked; struct RhhWildActorBattleView end_actor; };
static struct ObjectEvent *PrepareCatchActorSpawn(struct Pokemon *mon, bool32 identified)
{
    gMain.inBattle = FALSE; gMain.callback2 = CB2_Overworld;
    memset(gObjectEvents, 0, sizeof(gObjectEvents));
    struct ObjectEvent *object = &gObjectEvents[0];
    object->active = TRUE; object->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER;
    object->localId = LOCALID_OW_ENCOUNTER_END; object->mapGroup = 1; object->mapNum = 0;
    object->trainerRange_berryTreeId = mon->level;
    object->graphicsId = GetGraphicsIdForMon(GetMonData(mon, MON_DATA_SPECIES), GetMonData(mon, MON_DATA_IS_SHINY),
        GetMonGender(mon) == MON_FEMALE);
    ModsWildActorSpawned(object, mon);
    if (identified) ModsWildActorsUpdate();
    return object;
}
static void PrepareCatchActor(struct Pokemon *mon, bool32 identified, u32 opponentSlot)
{
    struct ObjectEvent *object = PrepareCatchActorSpawn(mon, identified);
    ModsWildActorEngaged(object); ModsWildActorBattleOpponentSlot(opponentSlot); gSpecialVar_LastTalked = object->localId;
    ModsWildActorBattleTransition(); ClearObjectEvent(object);
}
static void PrepareCatchBattle(struct BattleStruct *battle, bool32 doubleBattle)
{
    memset(battle, 0, sizeof(*battle)); gBattleStruct = battle; gMain.inBattle = TRUE;
    gBattleTypeFlags = doubleBattle ? BATTLE_TYPE_DOUBLE : 0; gBattlersCount = doubleBattle ? 4 : 2;
    gAbsentBattlerFlags = 0; memset(gBattleMons, 0, sizeof(gBattleMons));
    for (u32 i = 0; i < gBattlersCount; ++i) { gBattlerPositions[i] = i; gBattlerPartyIndexes[i] = i / 2; }
    gBattleMons[1].hp = 1; if (doubleBattle) gBattleMons[3].hp = 1;
}
static void CheckWildCatchOrigin(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 1);
    RhhModsSetPersistence(IndividualTestPersist); sIndividualPersistFails = FALSE; sIndividualPersistCalls = 0;
    const struct RhhWildActorService *wild = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 14));
    struct RhhWildCatchOrigin outside = {.struct_size = sizeof(outside), .flags = 99}, savedOutside = outside;
    CHECK(wild && wild->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_14_SIZE
        && RHH_WILD_ACTOR_SERVICE_V1_13_SIZE < RHH_WILD_ACTOR_SERVICE_V1_14_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 19))
        && wild->catch_origin(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside)));
    struct CatchOriginProbeState *probe = RhhModsState("test:wild_catch_origin"); CHECK(probe);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 111, OTID_STRUCT_PRESET(222), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    gSaveBlock2Ptr->playerTrainerId[0] = 0x78; gSaveBlock2Ptr->playerTrainerId[1] = 0x56;
    gSaveBlock2Ptr->playerTrainerId[2] = 0x34; gSaveBlock2Ptr->playerTrainerId[3] = 0x12;
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    struct BattleStruct battle = {0}; gBattleTypeFlags = 0; gParties[B_TRAINER_OPPONENT_A][0] = mon;
    PrepareCatchActor(&mon, FALSE, 0); PrepareCatchBattle(&battle, FALSE);
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    CHECK(probe->count == 1 && probe->linked == 1 && !probe->last.flags && !probe->last.individual.serial
        && probe->last.opponent_party_slot == 0 && probe->last.placement.destination_location == RHH_MON_PARTY
        && probe->last.placement.before.identity != probe->last.placement.after.identity);
    ModsWildActorBattleFinished(B_OUTCOME_CAUGHT);
    CHECK(probe->ended == 1 && probe->ended_linked == 1 && probe->battle_catch.opponent_party_slot == 0);
    /* In a double wild battle the original actor is party slot 1, even when
     * the other opponent has identical Pokémon data. */
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE; gParties[B_TRAINER_OPPONENT_A][0] = mon; gParties[B_TRAINER_OPPONENT_A][1] = mon;
    PrepareCatchActor(&mon, TRUE, 1); PrepareCatchBattle(&battle, TRUE);
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE); CHECK(probe->count == 2 && probe->linked == 1);
    gBattleMons[1].hp = 0; ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    CHECK(probe->count == 3 && probe->linked == 2 && probe->last.opponent_party_slot == 1
        && (probe->last.flags & RHH_WILD_CATCH_INDIVIDUAL_AVAILABLE) && probe->last.individual.run_id == 456
        && probe->last.individual.serial && sIndividualPersistCalls);
    ModsWildActorBattleFinished(B_OUTCOME_CAUGHT);
    CHECK(probe->ended == 2 && probe->ended_linked == 2 && probe->battle_catch.opponent_party_slot == 1);
    /* A changed source witness cannot inherit the actor association. */
    gBattleTypeFlags = 0; gParties[B_TRAINER_OPPONENT_A][0] = mon; PrepareCatchActor(&mon, FALSE, 0);
    PrepareCatchBattle(&battle, FALSE); gParties[B_TRAINER_OPPONENT_A][0].box.otName[0] ^= 1;
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE); CHECK(probe->count == 4 && probe->linked == 2);
    ModsWildActorBattleFinished(B_OUTCOME_CAUGHT); CHECK(probe->ended == 3 && probe->ended_linked == 2);
    /* A follower's two-versus-one battle uses the double flag but retains
     * the encountered actor in opponent slot 0. */
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    gParties[B_TRAINER_OPPONENT_A][0] = mon; PrepareCatchActor(&mon, FALSE, 0);
    PrepareCatchBattle(&battle, TRUE); gBattleTypeFlags |= BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    CHECK(probe->count == 5 && probe->linked == 3 && probe->last.opponent_party_slot == 0);
    ModsWildActorBattleFinished(B_OUTCOME_CAUGHT); CHECK(probe->ended == 4 && probe->ended_linked == 3);
    /* The same species, personality and trainer are insufficient when a
     * different individual has been substituted in the opponent slot. */
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 111, OTID_STRUCT_PRESET(222), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    gBattleTypeFlags = 0; gParties[B_TRAINER_OPPONENT_A][0] = mon; PrepareCatchActor(&mon, FALSE, 0);
    PrepareCatchBattle(&battle, FALSE);
    u8 changedIv = 30; SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_HP_IV, &changedIv);
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE); CHECK(probe->count == 6 && probe->linked == 3);
    ModsWildActorBattleFinished(B_OUTCOME_CAUGHT); CHECK(probe->ended == 5 && probe->ended_linked == 3);
    CHECK(wild->catch_origin(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside)));
    ModsWildActorBattleReset(); gMain.inBattle = FALSE; gBattleStruct = NULL;
    free(sIndividualJournal); sIndividualJournal = NULL; sIndividualJournalSize = 0;
    RhhModsUnloadLibraries(); RhhModsShutdown();
    puts("wild catch origin: single, double, identical opponents, follower and witness validation passed");
}
static void CheckWildBattleCatchOnly(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 1);
    struct CatchOriginProbeState *probe = RhhModsState("test:wild_catch_origin"); CHECK(probe);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 111, OTID_STRUCT_PRESET(222), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    struct BattleStruct battle = {0}; gBattleTypeFlags = BATTLE_TYPE_DOUBLE;
    gParties[B_TRAINER_OPPONENT_A][0] = mon; gParties[B_TRAINER_OPPONENT_A][1] = mon;
    PrepareCatchActor(&mon, FALSE, 1); PrepareCatchBattle(&battle, TRUE);
    const struct RhhWildActorService *wild = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 16));
    struct RhhWildActorBattleView view = {.struct_size = sizeof(view), .flags = 99}, before = view;
    CHECK(wild && wild->battle_actor(&view) && !memcmp(&view, &before, sizeof(view)));
    gBattleMons[1].species = gBattleMons[3].species = SPECIES_POOCHYENA;
    gBattleMons[1].maxHP = gBattleMons[3].maxHP = 10;
    ModsBattleBegin();
    CHECK(probe->started == 1 && probe->started_linked == 1
        && probe->battle_actor.opponent_party_slot == 1 && probe->battle_actor.battle_id
        && (probe->battle_actor.flags & RHH_WILD_BATTLE_PARTICIPANT_AVAILABLE)
        && probe->battle_actor.participant.battler == 3);
    CHECK(!wild->battle_actor(&view) && view.participant.battler == 3);
    ModsBattleSwitchPrepared(3); view.struct_size = sizeof(view);
    CHECK(!wild->battle_actor(&view) && !(view.flags & RHH_WILD_BATTLE_PARTICIPANT_AVAILABLE));
    ModsBattleSwitchEntered(3); CHECK(!wild->battle_actor(&view) && view.participant.battler == 3);
    u8 changedIv = 30; SetMonData(&gParties[B_TRAINER_OPPONENT_A][1], MON_DATA_HP_IV, &changedIv);
    before = view; CHECK(wild->battle_actor(&view) && !memcmp(&view, &before, sizeof(view)));
    gParties[B_TRAINER_OPPONENT_A][1] = mon; ModsBattleReset();
    CHECK(wild->battle_actor(&view) && !memcmp(&view, &before, sizeof(view)));
    ModsBattleBegin(); CHECK(probe->started == 2 && probe->started_linked == 1
        && wild->battle_actor(&view) && !memcmp(&view, &before, sizeof(view)));
    ModsBattleReset();
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE); ModsWildActorBattleFinished(B_OUTCOME_CAUGHT);
    CHECK(!probe->count && probe->ended == 1 && !probe->ended_linked);
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE; gParties[B_TRAINER_OPPONENT_A][0] = mon; gParties[B_TRAINER_OPPONENT_A][1] = mon;
    PrepareCatchActor(&mon, FALSE, 1); PrepareCatchBattle(&battle, TRUE); gBattleMons[1].hp = 0;
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE); ModsWildActorBattleFinished(B_OUTCOME_CAUGHT);
    CHECK(!probe->count && probe->ended == 2 && probe->ended_linked == 1 && probe->battle_catch.opponent_party_slot == 1
        && probe->battle_catch.destination_location == RHH_MON_PARTY);
    gMain.inBattle = FALSE; gBattleStruct = NULL; RhhModsUnloadLibraries(); RhhModsShutdown();
    puts("wild battle catch: independent observer and double opponents passed");
}
static void CheckWildBattleViewOnly(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 1);
    struct CatchOriginProbeState *probe = RhhModsState("test:wild_catch_origin"); CHECK(probe);
    CHECK(!RhhModsHasHook(RHH_OWNED_CATCH_PLACED) && !RhhModsHasHook(RHH_WILD_ACTOR_BATTLE_ENDED));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 111, OTID_STRUCT_PRESET(222), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    gSaveBlock2Ptr->playerTrainerId[0] = 0x78; gSaveBlock2Ptr->playerTrainerId[1] = 0x56;
    gSaveBlock2Ptr->playerTrainerId[2] = 0x34; gSaveBlock2Ptr->playerTrainerId[3] = 0x12;
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    struct BattleStruct battle = {0}; gBattleTypeFlags = 0; gParties[B_TRAINER_OPPONENT_A][0] = mon;
    PrepareCatchActor(&mon, FALSE, 0); PrepareCatchBattle(&battle, FALSE);
    gBattleMons[1].species = SPECIES_POOCHYENA; gBattleMons[1].maxHP = 10; ModsBattleBegin();
    CHECK(probe->started == 1 && probe->started_linked == 1
        && (probe->battle_actor.flags & RHH_WILD_BATTLE_PARTICIPANT_AVAILABLE));
    u64 previousIdentity = probe->battle_actor.participant.identity;
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    const struct RhhWildActorService *wild = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 16));
    struct RhhWildActorBattleView view = {.struct_size = sizeof(view)};
    CHECK(wild && !wild->battle_actor(&view) && view.participant.identity != previousIdentity
        && view.participant.identity == ((u64)0x12345678 << 32 | 12345));
    gBattleOutcome = B_OUTCOME_CAUGHT; ModsBattleEnd(gBattleOutcome, FALSE);
    CHECK(probe->battle_ended == 1 && probe->battle_ended_linked == 1
        && probe->end_actor.participant.identity == view.participant.identity
        && probe->end_actor.battle_id == view.battle_id);
    ModsWildActorBattleReset(); gMain.inBattle = FALSE; gBattleStruct = NULL; gBattleOutcome = 0;
    RhhModsUnloadLibraries(); RhhModsShutdown();
    puts("wild battle actor: capture rekey without catch observers passed");
}
struct BattleTraitProbeState { u32 engaged, started, ended; u64 handle; };
static RhhContentKey sBattleTraitKnown, sBattleTraitImpure, sBattleTraitDenied;
static bool32 sBattleTraitFlip;
static const struct RhhModEcologyTag sBattleTraitTags[] = {
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/known", "Known", "Known trait"},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/impure", "Impure", "RNG fallback"},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/denied", "Denied", "Denied trait"}
};
static struct RhhModSpeciesTag sBattleTraitBindings[] = {
    {sizeof(struct RhhModSpeciesTag), 0, 0, 0}, {sizeof(struct RhhModSpeciesTag), 0, 0, 0}
};
static void BattleTraitTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sBattleTraitKnown) decision->value = sBattleTraitFlip ? 1 : 0;
    else if (event->event_key == sBattleTraitImpure) {
        decision->value = 1; (void)Random32(); (void)Random2_32(); gLastUsedAbility = ABILITY_STENCH;
    }
    else if (event->event_key == sBattleTraitDenied) decision->allowed = 0;
}
static void BattleTraitObserve(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)event; (void)decision; (void)state; (void)Random32(); (void)Random2_32(); gLastUsedAbility = ABILITY_STENCH; }
static const struct RhhModHook sBattleTraitHooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_TRAIT_QUERY, RHH_TRANSFORM, 0, BattleTraitTransform, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_ENGAGED, RHH_OBSERVE, 0, NULL, BattleTraitObserve},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_BATTLE_ENDED, RHH_OBSERVE, 0, NULL, BattleTraitObserve}
};
static const struct RhhModDefinition sBattleTraitMod = {
    .struct_size = sizeof(sBattleTraitMod), .abi_version = RHH_MOD_ABI_ECOLOGY, .id = "test:battle_trait_rules",
    .hooks = sBattleTraitHooks, .hook_count = ARRAY_COUNT(sBattleTraitHooks),
    .ecology_tag_count = ARRAY_COUNT(sBattleTraitTags), .ecology_tags = sBattleTraitTags,
    .species_tag_count = ARRAY_COUNT(sBattleTraitBindings), .species_tags = sBattleTraitBindings
};
static void CheckWildBattleTraits(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins());
    sBattleTraitKnown = RhhContentKeyOf("test:trait/known"); sBattleTraitImpure = RhhContentKeyOf("test:trait/impure");
    sBattleTraitDenied = RhhContentKeyOf("test:trait/denied");
    sBattleTraitBindings[0].species = sBattleTraitBindings[1].species = RhhContentKeyForId(RHH_SPECIES, SPECIES_POOCHYENA);
    sBattleTraitBindings[0].tag = sBattleTraitKnown; sBattleTraitBindings[1].tag = sBattleTraitDenied;
    CHECK(!RhhModsRegister(&sBattleTraitMod) && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 1);
    const struct RhhWildTraitsService *traits = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_traits", RHH_SERVICE_VERSION(1, 1));
    struct BattleTraitProbeState *probe = RhhModsState("test:battle_trait_probe");
    struct RhhWildTraitResult out = {.struct_size = sizeof(out), .value = 99}, unchanged = out;
    CHECK(traits && probe && traits->header.struct_size >= RHH_WILD_TRAITS_SERVICE_V1_1_SIZE
        && RHH_WILD_TRAITS_SERVICE_V1_0_SIZE < RHH_WILD_TRAITS_SERVICE_V1_1_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/wild_traits", RHH_SERVICE_VERSION(1, 2))
        && traits->request_battle(sBattleTraitKnown) && traits->battle(sBattleTraitKnown, &out)
        && !memcmp(&out, &unchanged, sizeof(out)));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 111, OTID_STRUCT_PRESET(222), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    gSaveBlock2Ptr->playerTrainerId[0] = 0x78; gSaveBlock2Ptr->playerTrainerId[1] = 0x56;
    gSaveBlock2Ptr->playerTrainerId[2] = 0x34; gSaveBlock2Ptr->playerTrainerId[3] = 0x12;
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE; gParties[B_TRAINER_OPPONENT_A][0] = gParties[B_TRAINER_OPPONENT_A][1] = mon;
    sBattleTraitFlip = FALSE; gLastUsedAbility = ABILITY_PRESSURE;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    PrepareCatchActor(&mon, FALSE, 1);
    CHECK(probe->engaged == 1 && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && gLastUsedAbility == ABILITY_PRESSURE);
    sBattleTraitFlip = TRUE;
    struct BattleStruct battle = {0}; PrepareCatchBattle(&battle, TRUE);
    gBattleMons[1].species = gBattleMons[3].species = SPECIES_POOCHYENA; gBattleMons[1].maxHP = gBattleMons[3].maxHP = 10;
    ModsBattleBegin(); CHECK(probe->started == 1 && !traits->battle(sBattleTraitKnown, &out) && !out.value);
    gBattleMons[1].hp = 0; ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    CHECK(!traits->battle(sBattleTraitKnown, &out) && !out.value);
    struct Pokemon capturedOpponent = gParties[B_TRAINER_OPPONENT_A][1];
    u8 changedIv = 30; SetMonData(&gParties[B_TRAINER_OPPONENT_A][1], MON_DATA_HP_IV, &changedIv);
    unchanged = out; CHECK(traits->battle(sBattleTraitKnown, &out) && !memcmp(&out, &unchanged, sizeof(out)));
    gParties[B_TRAINER_OPPONENT_A][1] = capturedOpponent;
    gBattleOutcome = B_OUTCOME_CAUGHT; ModsBattleEnd(gBattleOutcome, FALSE);
    rng = gRngValue; rng2 = gRng2Value; gLastUsedAbility = ABILITY_PRESSURE;
    ModsWildActorBattleFinished(B_OUTCOME_CAUGHT);
    CHECK(probe->ended == 1 && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && gLastUsedAbility == ABILITY_PRESSURE && traits->battle(sBattleTraitKnown, &out));
    gMain.inBattle = FALSE; gBattleStruct = NULL; gBattleOutcome = 0; RhhModsUnloadLibraries(); RhhModsShutdown();
    puts("wild battle traits: independent rules, capture, RNG and battle lifecycle passed");
}
struct BattleProfileProbeState { u32 engaged, started, ended, identified; };
static void CheckWildBattleProfile(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 1);
    RhhModsSetPersistence(IndividualTestPersist); sIndividualPersistFails = FALSE; sIndividualPersistCalls = 0;
    const struct RhhWildProfileService *profile = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 4));
    const struct RhhWildActorService *actors = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 16));
    struct BattleProfileProbeState *probe = RhhModsState("test:battle_profile_probe");
    RhhContentKey owner = RhhContentKeyOf("test:battle_profile_probe"), field = RhhContentKeyOf("test:profile/value");
    struct RhhWildProfileValue out = {.struct_size = sizeof(out), .value = 99}, unchanged = out;
    CHECK(profile && actors && probe && profile->header.struct_size >= RHH_WILD_PROFILE_SERVICE_V1_4_SIZE
        && RHH_WILD_PROFILE_SERVICE_V1_3_SIZE < RHH_WILD_PROFILE_SERVICE_V1_4_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 5))
        && profile->battle(owner, field, &out) && !memcmp(&out, &unchanged, sizeof(out)));
    gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    ScriptContext_Init(); UnlockPlayerFieldControls();
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 111, OTID_STRUCT_PRESET(222), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    gSaveBlock2Ptr->playerTrainerId[0] = 0x78; gSaveBlock2Ptr->playerTrainerId[1] = 0x56;
    gSaveBlock2Ptr->playerTrainerId[2] = 0x34; gSaveBlock2Ptr->playerTrainerId[3] = 0x12;
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE; gParties[B_TRAINER_OPPONENT_A][0] = gParties[B_TRAINER_OPPONENT_A][1] = mon;
    struct ObjectEvent *object = PrepareCatchActorSpawn(&mon, TRUE);
    object->currentCoords.x = object->previousCoords.x = MAP_OFFSET + 1;
    object->currentCoords.y = object->previousCoords.y = MAP_OFFSET + 1;
    object->facingDirection = DIR_SOUTH;
    object->warpArrowSpriteId = ROAMER_COUNT + 2; /* ordinary generated encounter */
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    CHECK(!actors->at(0, &actor));
    u32 dormantSize = RhhModsWildDormantRecordSize();
    u8 *dormantBefore = malloc(dormantSize), *dormantAfter = malloc(dormantSize);
    CHECK(dormantBefore && dormantAfter); memset(dormantBefore, 0xa5, dormantSize);
    CHECK(RhhModsWildDormantPrepare(actor.handle, owner, dormantBefore, dormantSize - 1)
        && dormantBefore[0] == 0xa5);
    object->warpArrowSpriteId = 0; /* A roamer has separate engine state. */
    CHECK(RhhModsWildDormantPrepare(actor.handle, owner, dormantBefore, dormantSize)
        && dormantBefore[0] == 0xa5);
    object->warpArrowSpriteId = ROAMER_COUNT; /* Outbreak and Feebas categories too. */
    CHECK(RhhModsWildDormantPrepare(actor.handle, owner, dormantBefore, dormantSize)
        && dormantBefore[0] == 0xa5);
    object->warpArrowSpriteId = ROAMER_COUNT + 2;
    CHECK(!RhhModsWildDormantPrepare(actor.handle, owner, dormantBefore, dormantSize));
    struct RhhWildProfileRequest request = {.struct_size = sizeof(request), .present = 1, .actor_handle = actor.handle,
        .owner = owner, .field = field, .value = -17};
    struct RhhWildProfileResult applied = {.struct_size = sizeof(applied)};
    CHECK(!profile->apply(&request, &applied) && applied.changed && applied.after.revision == 1);
    CHECK(!RhhModsWildDormantPrepare(actor.handle, owner, dormantAfter, dormantSize)
        && memcmp(dormantBefore, dormantAfter, dormantSize));
    const struct NativeSaveArchiveCodec *dormantCodec = RhhModsWildDormantCodec();
    u32 archiveSize = 32 + dormantSize; u8 *archive = calloc(archiveSize, 1), *roundtrip = NULL; u32 roundtripSize = 0;
    CHECK(dormantCodec && archive); memcpy(archive, "RHWDOR02", 8);
    ClockTestPut32(archive + 8, archiveSize); ClockTestPut32(archive + 12, 1); ClockTestPut64(archive + 16, 456);
    memcpy(archive + 32, dormantAfter, dormantSize);
    /* A live and dormant copy of one ID must never be accepted together. */
    CHECK(dormantCodec->validate(archive, archiveSize));
    u8 *liveActors = RhhModsState("rhh:wild_actor_state");
    const u32 liveRecordSize = 16 + sizeof(struct Pokemon); u8 savedLive[16 + sizeof(struct Pokemon)];
    CHECK(liveActors); memcpy(savedLive, liveActors + actor.slot * liveRecordSize, sizeof(savedLive));
    memset(liveActors + actor.slot * liveRecordSize, 0, sizeof(savedLive));
    CHECK(!dormantCodec->validate(archive, archiveSize) && !dormantCodec->import_data(archive, archiveSize)
        && !dormantCodec->export_data(&roundtrip, &roundtripSize) && roundtripSize == archiveSize
        && !memcmp(archive, roundtrip, archiveSize));
    /* A growing archive must retain more than the old 32 dormant actors. */
    u32 manySize = 32 + 40 * dormantSize;
    u8 *many = calloc(manySize, 1), *sequence = RhhModsState("rhh:wild_actor_sequence");
    u8 oldSequence[8]; CHECK(many && sequence); memcpy(oldSequence, sequence, 8);
    memcpy(many, "RHWDOR02", 8); ClockTestPut32(many + 8, manySize);
    ClockTestPut32(many + 12, 40); ClockTestPut64(many + 16, 456); ClockTestPut64(sequence, 40);
    for (u32 n = 0; n < 40; ++n)
    {
        u8 *entry = many + 32 + n * dormantSize;
        memcpy(entry, dormantBefore, 40 + sizeof(struct Pokemon));
        ClockTestPut64(entry, n + 1);
    }
    free(roundtrip); roundtrip = NULL; roundtripSize = 0;
    CHECK(!dormantCodec->validate(many, manySize) && !dormantCodec->import_data(many, manySize)
        && !dormantCodec->export_data(&roundtrip, &roundtripSize) && roundtripSize == manySize
        && !memcmp(many, roundtrip, manySize));
    free(many); memcpy(sequence, oldSequence, 8);
    ClockTestPut64(archive + archiveSize - 8, UINT64_MAX);
    CHECK(dormantCodec->validate(archive, archiveSize));
    ClockTestPut64(archive + archiveSize - 8, 0);
    u32 legacySize = archiveSize - 8; u8 *legacy = malloc(legacySize); CHECK(legacy);
    memcpy(legacy, archive, legacySize); memcpy(legacy, "RHWDOR01", 8); ClockTestPut32(legacy + 8, legacySize);
    ClockTestPut64(run + 24, 19);
    CHECK(!dormantCodec->validate(legacy, legacySize) && !dormantCodec->import_data(legacy, legacySize));
    free(roundtrip); roundtrip = NULL; roundtripSize = 0;
    CHECK(!dormantCodec->export_data(&roundtrip, &roundtripSize) && roundtripSize == archiveSize
        && !memcmp(roundtrip, "RHWDOR02", 8) && roundtrip[archiveSize - 8] == 19);
    free(legacy); ClockTestPut64(run + 24, 0);
    archive[32] ^= 1; CHECK(dormantCodec->validate(archive, archiveSize));
    free(archive); free(roundtrip); dormantCodec->reset();
    memcpy(liveActors + actor.slot * liveRecordSize, savedLive, sizeof(savedLive));
    free(dormantBefore); free(dormantAfter);
    ModsWildActorEngaged(object); ModsWildActorBattleOpponentSlot(1); gSpecialVar_LastTalked = object->localId;
    ModsWildActorBattleTransition();
    struct RhhWildActorSnapshot before = {.struct_size = sizeof(before)};
    CHECK(ModsWildActorBeforeRemove(object, &before)); ClearObjectEvent(object);
    ModsWildActorAfterRemove(object, &before, RHH_WILD_ACTOR_BATTLE);
    struct BattleStruct battle = {0}; PrepareCatchBattle(&battle, TRUE);
    gBattleMons[1].species = gBattleMons[3].species = SPECIES_POOCHYENA; gBattleMons[1].maxHP = gBattleMons[3].maxHP = 10;
    ModsBattleBegin(); CHECK(probe->engaged == 1 && probe->started == 1 && probe->identified
        && !profile->battle(owner, field, &out) && out.present && out.value == -17);
    gBattleMons[1].hp = 0; ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    CHECK(!profile->battle(owner, field, &out) && out.value == -17);
    struct Pokemon capturedOpponent = gParties[B_TRAINER_OPPONENT_A][1];
    u8 changedIv = 30; SetMonData(&gParties[B_TRAINER_OPPONENT_A][1], MON_DATA_HP_IV, &changedIv);
    unchanged = out; CHECK(profile->battle(owner, field, &out) && !memcmp(&out, &unchanged, sizeof(out)));
    gParties[B_TRAINER_OPPONENT_A][1] = capturedOpponent;
    gBattleOutcome = B_OUTCOME_CAUGHT; ModsBattleEnd(gBattleOutcome, FALSE); ModsWildActorBattleFinished(B_OUTCOME_CAUGHT);
    CHECK(probe->ended == 1 && profile->battle(owner, field, &out));
    gBattleTypeFlags = 0; gParties[B_TRAINER_OPPONENT_A][0] = mon;
    PrepareCatchActor(&mon, FALSE, 0); PrepareCatchBattle(&battle, FALSE);
    gBattleMons[1].species = SPECIES_POOCHYENA; gBattleMons[1].maxHP = 10;
    ModsBattleBegin(); CHECK(probe->engaged == 2 && probe->started == 2 && !probe->identified);
    gBattleOutcome = B_OUTCOME_RAN; ModsBattleEnd(gBattleOutcome, FALSE); ModsWildActorBattleFinished(B_OUTCOME_RAN);
    CHECK(probe->ended == 2);
    gMain.inBattle = FALSE; gBattleStruct = NULL; gBattleOutcome = 0;
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    free(sIndividualJournal); sIndividualJournal = NULL; sIndividualJournalSize = 0;
    RhhModsUnloadLibraries(); RhhModsShutdown();
    puts("wild battle profile: captured fields, identity, witness and battle lifecycle passed");
}
static bool32 sWildPureImpure, sWildPureBad;
static void WildPureTestReduce(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (gLastUsedAbility != ABILITY_PRESSURE) sWildPureBad = TRUE;
    gLastUsedAbility = ABILITY_STENCH;
    if (event->kind == RHH_HABITAT_QUERY) return;
    if (event->kind == RHH_WILD_WEIGHT) {
        decision->value = 80;
        if (sWildPureImpure && event->args[2] < 2) {
            (void)Random32(); (void)Random2_32();
            if (event->args[2] == 1) decision->allowed = 0;
        }
    } else {
        if (event->kind == RHH_WILD_CANDIDATE) decision->value = RhhContentKeyForId(RHH_SPECIES, SPECIES_ZIGZAGOON);
        else if (event->kind == RHH_WILD_ENCOUNTER_RATE) decision->value = 123;
        else if (event->kind == RHH_FISHING_BITE_RATE) decision->value = 60;
        else if (event->kind == RHH_WILD_SPAWN_INTERVAL) decision->value = 150;
        if (sWildPureImpure) { (void)Random32(); (void)Random2_32(); }
    }
}
static const struct RhhModHook sWildPureHooks[] = {
    {sizeof(struct RhhModHook), RHH_HABITAT_QUERY, RHH_TRANSFORM, 0, WildPureTestReduce, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_WEIGHT, RHH_TRANSFORM, 0, WildPureTestReduce, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_CANDIDATE, RHH_TRANSFORM, 0, WildPureTestReduce, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_ENCOUNTER_RATE, RHH_TRANSFORM, 0, WildPureTestReduce, NULL},
    {sizeof(struct RhhModHook), RHH_FISHING_BITE_RATE, RHH_TRANSFORM, 0, WildPureTestReduce, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_SPAWN_INTERVAL, RHH_TRANSFORM, 0, WildPureTestReduce, NULL}
};
static struct RhhModSpeciesTag sWildPurePreference;
static const struct RhhModDefinition sWildPureMod = {.struct_size = sizeof(sWildPureMod),
    .abi_version = RHH_MOD_ABI_ECOLOGY, .id = "test:wild_proposal_purity",
    .hooks = sWildPureHooks, .hook_count = ARRAY_COUNT(sWildPureHooks),
    .species_tag_count = 1, .species_tags = &sWildPurePreference};
static void CheckWildProposalPurity(void)
{
    sWildPurePreference = (struct RhhModSpeciesTag){sizeof(sWildPurePreference), 0,
        RhhContentKeyOf("rhh:species/zigzagoon"), RhhContentKeyOf("rhh:preference/electric")};
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sWildPureMod) && !RhhModsFinalize());
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 0);
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE103)][MAP_NUM(MAP_ROUTE103)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE103); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE103);
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    memset(gObjectEvents, 0, sizeof(gObjectEvents)); gPlayerAvatar.objectEventId = 0;
    gObjectEvents[0].active = TRUE; gObjectEvents[0].isPlayer = TRUE;
    gObjectEvents[0].facingDirection = DIR_SOUTH;
    gObjectEvents[0].currentCoords.x = gObjectEvents[0].previousCoords.x = MAP_OFFSET + 1;
    gObjectEvents[0].currentCoords.y = gObjectEvents[0].previousCoords.y = MAP_OFFSET + 1;
    const struct RhhWildEncounterService *wild = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 20));
    const struct RhhHabitatService *habitat = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/habitat", RHH_SERVICE_VERSION(1, 7));
    CHECK(wild && habitat && wild->header.struct_size >= RHH_WILD_ENCOUNTER_SERVICE_V1_20_SIZE
        && habitat->header.struct_size >= RHH_HABITAT_SERVICE_V1_7_SIZE);
    struct WildPokemon table[3] = {{5, 5, SPECIES_POOCHYENA}, {5, 5, SPECIES_POOCHYENA}, {5, 5, SPECIES_POOCHYENA}};
    const u16 base[] = {10, 20, 30}; u16 weights[3];
    struct RhhWildEncounterContext candidate; struct RhhHabitatSample sample = {.struct_size = sizeof(sample)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; gLastUsedAbility = ABILITY_PRESSURE;
    sWildPureImpure = TRUE; sWildPureBad = FALSE;
    CHECK(!habitat->sample(1, 1, &sample) && gLastUsedAbility == ABILITY_PRESSURE);
    CHECK(ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
        TIME_OF_DAY_DEFAULT, 1, 1, 0, 3, base, weights) && weights[0] == 10 && weights[1] == 0 && weights[2] == 80);
    CHECK(ModsWildCandidate(SPECIES_POOCHYENA, SPECIES_POOCHYENA, WILD_AREA_LAND, RHH_WILD_STANDARD,
        0, 5, TIME_OF_DAY_DEFAULT, 1, 1, &candidate) == SPECIES_POOCHYENA);
    CHECK(ModsWildEncounterRate(1, 400, WILD_AREA_LAND, RHH_WILD_STANDARD, TIME_OF_DAY_DEFAULT, 1, 1, FALSE) == 400);
    CHECK(ModsFishingBiteRate(0, FALSE, RHH_FISHING_BITE_REGULAR, 25) == 25);
    CHECK(ModsWildSpawnInterval(100, RHH_WILD_SPAWN_MINIMUM, WILD_AREA_LAND, 0, 3, 0, 1, 1) == 100);
    CHECK(!sWildPureBad && gLastUsedAbility == ABILITY_PRESSURE && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sWildPureImpure = FALSE;
    CHECK(ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
        TIME_OF_DAY_DEFAULT, 1, 1, 0, 3, base, weights) && weights[0] == 80 && weights[1] == 80 && weights[2] == 80);
    CHECK(ModsWildCandidate(SPECIES_POOCHYENA, SPECIES_POOCHYENA, WILD_AREA_LAND, RHH_WILD_STANDARD,
        0, 5, TIME_OF_DAY_DEFAULT, 1, 1, &candidate) == SPECIES_ZIGZAGOON);
    CHECK(ModsWildEncounterRate(1, 400, WILD_AREA_LAND, RHH_WILD_STANDARD, TIME_OF_DAY_DEFAULT, 1, 1, FALSE) == 123);
    CHECK(ModsFishingBiteRate(0, FALSE, RHH_FISHING_BITE_REGULAR, 25) == 60);
    CHECK(ModsWildSpawnInterval(100, RHH_WILD_SPAWN_MINIMUM, WILD_AREA_LAND, 0, 3, 0, 1, 1) == 150);
    CHECK(!sWildPureBad && gLastUsedAbility == ABILITY_PRESSURE && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    /* Real-time pressure composes after mod weights and retains precision for
     * small table entries. Only this location/species is affected. */
    RhhContentKey route = RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8)
                                                     | (u8)gSaveBlock1Ptr->location.mapNum);
    RhhContentKey pooch = RhhContentKeyForId(RHH_SPECIES, SPECIES_POOCHYENA);
    u64 now = RhhEcologySystemTime(); CHECK(now && route && pooch);
    const struct RhhWorldEnvironmentService *environment = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_environment", RHH_SERVICE_VERSION(1, 7));
    const struct RhhEcologyService *ecology = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/ecology", RHH_SERVICE_VERSION(1, 0));
    CHECK(environment && ecology);
    struct RhhWorldEnvironmentSourceRequest env = {.struct_size = sizeof(env), .map = route,
        .owner = RhhContentKeyOf("test:wild_proposal_purity"), .kind = RHH_WORLD_WEATHER,
        .value = RHH_WEATHER_RAIN, .clock = RHH_WORLD_ENV_CLOCK_STEPS, .duration = 100};
    struct RhhWorldEnvironmentResult envResult = {.struct_size = sizeof(envResult)};
    CHECK(!environment->apply_source(&env, &envResult));
    table[0].species = SPECIES_LOTAD; table[1].species = SPECIES_POOCHYENA;
    CHECK(ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
        TIME_OF_DAY_DEFAULT, 1, 1, 0, 3, base, weights));
    CHECK(weights[0] * 4 == weights[1] * 5);
    env.kind = RHH_WORLD_TERRAIN; env.value = RHH_TERRAIN_GRASSY;
    CHECK(!environment->apply_source(&env, &envResult));
    CHECK(ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
        TIME_OF_DAY_DEFAULT, 1, 1, 0, 3, base, weights));
    CHECK(weights[0] * 2 == weights[1] * 3);
    env.value = RHH_TERRAIN_ELECTRIC; CHECK(!environment->apply_source(&env, &envResult));
    table[2].species = SPECIES_ZIGZAGOON;
    CHECK(ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
        TIME_OF_DAY_DEFAULT, 1, 1, 0, 3, base, weights));
    CHECK(weights[2] * 4 == weights[1] * 5);
    table[2].species = SPECIES_POOCHYENA;
    CHECK(!ecology->set(route, RHH_ECOLOGY_CONDITION, RhhContentKeyOf("rhh:ecology/preference_bonus"), 0));
    CHECK(ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
        TIME_OF_DAY_DEFAULT, 1, 1, 0, 3, base, weights));
    CHECK(weights[0] == weights[1]);
    CHECK(!ecology->remove(route, RHH_ECOLOGY_CONDITION, RhhContentKeyOf("rhh:ecology/preference_bonus")));
    env.value = 0; env.duration = 0; CHECK(!environment->apply_source(&env, &envResult));
    env.kind = RHH_WORLD_WEATHER; CHECK(!environment->apply_source(&env, &envResult));

    struct ObjectEventTemplate savedTemplates[OBJECT_EVENT_TEMPLATES_COUNT];
    memcpy(savedTemplates, gSaveBlock1Ptr->objectEventTemplates, sizeof(savedTemplates));
    memset(gSaveBlock1Ptr->objectEventTemplates, 0, sizeof(savedTemplates));
    memcpy(gSaveBlock1Ptr->objectEventTemplates, gMapHeader.events->objectEvents,
           gMapHeader.events->objectEventCount * sizeof(struct ObjectEventTemplate));
    MainCallback previousCallback = gMain.callback2; gMain.callback2 = CB2_Overworld;
    struct BerryTree *tree = GetBerryTreeInfo(BERRY_TREE_ROUTE_103_CHERI_1), oldTree = *tree;
    *tree = (struct BerryTree){.berry = ItemIdToBerryType(ITEM_CHERI_BERRY),
        .stage = BERRY_STAGE_BERRIES, .berryYield = 1};
    table[0].species = SPECIES_POOCHYENA; table[1].species = SPECIES_LOTAD;
    CHECK(ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
        TIME_OF_DAY_DEFAULT, 1, 1, 0, 3, base, weights));
    CHECK(weights[0] * 4 == weights[1] * 5);
    tree->stage = BERRY_STAGE_FLOWERING;
    CHECK(ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
        TIME_OF_DAY_DEFAULT, 1, 1, 0, 3, base, weights));
    CHECK(weights[0] == weights[1]);
    *tree = oldTree; gMain.callback2 = previousCallback;
    memcpy(gSaveBlock1Ptr->objectEventTemplates, savedTemplates, sizeof(savedTemplates));
    table[1].species = SPECIES_POOCHYENA;
    CHECK(!RhhEcologyDepleteAt(route, pooch, now));
    table[1].species = SPECIES_ZIGZAGOON;
    CHECK(ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
        TIME_OF_DAY_DEFAULT, 1, 1, 0, 3, base, weights));
    CHECK(weights[0] == weights[2] && weights[0] * 10 == weights[1] * 9);

    /* A real battle lifecycle updates defeated and caught wild species, but
     * leaves trainer battles out of local encounter pressure. */
    RhhContentKey charizard = RhhContentKeyForId(RHH_SPECIES, SPECIES_CHARIZARD);
    PrepareTurnBattle(); gBattleTypeFlags = BATTLE_TYPE_IS_MASTER;
    memset(&gParties[B_TRAINER_OPPONENT_A][1], 0, sizeof(struct Pokemon));
    ModsBattleBegin();
    u16 zeroHp = 0; SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_HP, &zeroHp);
    gBattleOutcome = B_OUTCOME_WON; ModsBattleEnd(gBattleOutcome, FALSE);
    CHECK(RhhEcologyFrequencyAt(route, charizard, 0) == 9000);
    PrepareTurnBattle(); gBattleTypeFlags = BATTLE_TYPE_IS_MASTER; ModsBattleBegin();
    gBattleResults.caughtMonSpecies = SPECIES_CHARIZARD;
    gBattleOutcome = B_OUTCOME_CAUGHT; ModsBattleEnd(gBattleOutcome, FALSE);
    CHECK(RhhEcologyFrequencyAt(route, charizard, 0) == 8000);
    CHECK(!ecology->set(route, RHH_ECOLOGY_CONDITION,
        RhhContentKeyOf("rhh:ecology/battle_environment_seconds"), 123));
    PrepareTurnBattle(); ModsBattleBegin();
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_HP, &zeroHp);
    gBattleWeather = B_WEATHER_SUN_NORMAL; gFieldTimers.terrain = B_TERRAIN_ELECTRIC;
    gBattleOutcome = B_OUTCOME_WON; ModsBattleEnd(gBattleOutcome, FALSE);
    CHECK(RhhEcologyFrequencyAt(route, charizard, 0) == 8000);
    gMain.inBattle = FALSE;
    env.kind = RHH_WORLD_WEATHER; env.value = RHH_WEATHER_RAIN; env.priority = 10; env.duration = 100;
    CHECK(!environment->apply_source(&env, &envResult));
    RhhModsEnvironmentOnFrame();
    struct RhhWorldEnvironmentSnapshot world = {.struct_size = sizeof(world)};
    struct RhhWorldEnvironmentTiming timing = {.struct_size = sizeof(timing)};
    CHECK(!environment->current(&world) && world.weather.value == RHH_WEATHER_RAIN
        && world.terrain.value == RHH_TERRAIN_ELECTRIC);
    env.value = 0; env.duration = 0; CHECK(!environment->apply_source(&env, &envResult));
    RhhModsEnvironmentQueueBattle(RHH_WEATHER_SUN, RHH_TERRAIN_ELECTRIC);
    RhhModsEnvironmentOnFrame();
    CHECK(!environment->current(&world) && world.weather.value == RHH_WEATHER_SUN
        && world.terrain.value == RHH_TERRAIN_ELECTRIC);
    CHECK(!environment->timing(route, RHH_WORLD_WEATHER, &timing)
        && timing.source == RHH_TIME_SYSTEM && timing.expires - timing.started <= 123
        && timing.expires - timing.started >= 120);
    CHECK(!environment->timing(route, RHH_WORLD_TERRAIN, &timing)
        && timing.source == RHH_TIME_SYSTEM && timing.expires - timing.started <= 123
        && timing.expires - timing.started >= 120);
    u8 *savedEnvironment = RhhModsState("rhh:world_environment"); CHECK(savedEnvironment);
    u64 wall = RhhEcologySystemTime(); CHECK(wall > 124);
    const u32 timeOffset = RHH_WORLD_ENVIRONMENT_MAP_CAPACITY * 104;
    ClockTestPut64(savedEnvironment + timeOffset, wall - 124);
    ClockTestPut64(savedEnvironment + timeOffset + 8, wall - 1);
    ClockTestPut64(savedEnvironment + timeOffset + 24, wall - 124);
    ClockTestPut64(savedEnvironment + timeOffset + 32, wall - 1);
    RhhModsEnvironmentOnFrame();
    CHECK(!environment->current(&world) && !world.weather.value && !world.terrain.value);
    RhhModsEnvironmentQueueBattle(RHH_WEATHER_HARSH_SUN, RHH_TERRAIN_NONE);
    RhhModsEnvironmentOnFrame();
    CHECK(!environment->current(&world) && world.weather.value == RHH_WEATHER_HARSH_SUN);
    gMain.inBattle = TRUE; CHECK(ModsWorldBattleWeather() == RHH_WEATHER_HARSH_SUN); gMain.inBattle = FALSE;
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; RhhModsShutdown();
    puts("wild proposals: pure rules compose, RNG fallback and habitat isolation passed");
}
static u32 sAuthoredWildMode;
static void AuthoredWildTestRule(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (sAuthoredWildMode == 1) { decision->allowed = 0; return; }
    if (sAuthoredWildMode == 2) { if (!event->args[2]) VarSet(VAR_ALTERING_CAVE_WILD_SET, 1); return; }
    if (event->args[2] == 0) { (void)Random32(); (void)Random2_32(); decision->value = 999; }
    else if (event->args[2] == 1) decision->allowed = 0;
    else if (event->args[2] == 2) decision->value = 77;
    gLastUsedAbility = ABILITY_STENCH;
}
static void AuthoredWildTestConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->value = 4095; }
static const struct RhhModHook sAuthoredWildHooks[] = {
    {sizeof(struct RhhModHook), RHH_AUTHORED_WILD_WEIGHT, RHH_TRANSFORM, 0, AuthoredWildTestRule, NULL},
    {sizeof(struct RhhModHook), RHH_AUTHORED_WILD_WEIGHT, RHH_CONSTRAINT, 0, AuthoredWildTestConstraint, NULL}
};
static const struct RhhModDefinition sAuthoredWildTest = {.struct_size = sizeof(sAuthoredWildTest),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:authored_wild_rules",
    .hooks = sAuthoredWildHooks, .hook_count = ARRAY_COUNT(sAuthoredWildHooks)};
static void CheckAuthoredWildPreview(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sAuthoredWildTest)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 0);
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE103)][MAP_NUM(MAP_ROUTE103)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE103); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE103);
    struct WarpData location = gSaveBlock1Ptr->location;
    RhhContentKey route = RhhContentKeyForId(RHH_MAP, MAP_ROUTE101), town = RhhContentKeyForId(RHH_MAP, MAP_LITTLEROOT_TOWN);
    const struct RhhAuthoredEncounterService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/authored_encounters", RHH_SERVICE_VERSION(1, 3));
    CHECK(service && service->header.struct_size >= RHH_AUTHORED_ENCOUNTER_SERVICE_V1_3_SIZE
        && RHH_AUTHORED_ENCOUNTER_SERVICE_V1_2_SIZE < RHH_AUTHORED_ENCOUNTER_SERVICE_V1_3_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/authored_encounters", RHH_SERVICE_VERSION(1, 4)));
    struct RhhAuthoredWildPreviewRequest request = {.struct_size = sizeof(request), .area = RHH_WILD_LAND,
        .time = RHH_WILD_TIME_MORNING, .rod = RHH_WILD_ROD_NONE, .map = route, .ordinal = 42};
    struct RhhAuthoredWildPreview result = {.struct_size = sizeof(result)}, saved = result;
    struct RhhAuthoredWildWeightContext outside = {.struct_size = sizeof(outside), .slot = 99}, oldOutside = outside;
    CHECK(service->preview_context(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    rng_value_t rng = gRngValue, rng2 = gRng2Value; gLastUsedAbility = ABILITY_PRESSURE; sAuthoredWildMode = 0;
    CHECK(!service->preview(&request, &result) && result.available && !result.fallback
        && result.selection.map == route && result.habitat.sample.map == route
        && result.selection.table.slots[0].species == RhhContentKeyForId(RHH_SPECIES, SPECIES_WURMPLE)
        && result.weights.weights[0] == 20 && !result.weights.weights[1] && result.weights.weights[2] == 77
        && result.total == 147 && result.weights.total == result.total);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && gLastUsedAbility == ABILITY_PRESSURE && !memcmp(&location, &gSaveBlock1Ptr->location, sizeof(location)));
    sAuthoredWildMode = 1; CHECK(!service->preview(&request, &result) && result.available && result.fallback
        && result.total == 100 && result.weights.weights[0] == 20 && result.weights.weights[1] == 20);
    sAuthoredWildMode = 0; request.map = RhhContentKeyForId(RHH_MAP, MAP_ROUTE102);
    request.area = RHH_WILD_FISHING; request.rod = RHH_WILD_ROD_GOOD;
    CHECK(!service->preview(&request, &result) && result.available && !result.fallback
        && result.weights.first_slot == 2 && result.weights.slot_count == 3
        && !result.weights.weights[0] && result.weights.weights[2] == 77 && result.total == 117);
    request.area = RHH_WILD_LAND; request.rod = RHH_WILD_ROD_NONE;
    request.map = town; CHECK(!service->preview(&request, &result) && !result.available && !result.selection.has_header);
    request.map = route; request.rod = RHH_WILD_ROD_OLD; result = saved;
    CHECK(service->preview(&request, &result) && !memcmp(&result, &saved, sizeof(result)));
    request.rod = RHH_WILD_ROD_NONE; request.x = -1;
    CHECK(service->preview(&request, &result) && !memcmp(&result, &saved, sizeof(result)));
    request.x = 0; request.map = RhhContentKeyForId(RHH_MAP, MAP_ALTERING_CAVE);
    VarSet(VAR_ALTERING_CAVE_WILD_SET, 0); sAuthoredWildMode = 2;
    CHECK(service->preview(&request, &result) && !memcmp(&result, &saved, sizeof(result))
        && VarGet(VAR_ALTERING_CAVE_WILD_SET) == 1);
    VarSet(VAR_ALTERING_CAVE_WILD_SET, 0);
    RhhModsUnloadLibraries(); RhhModsShutdown();
    puts("authored wild preview: unloaded habitat, composition, fallback and RNG passed");
}
#endif
