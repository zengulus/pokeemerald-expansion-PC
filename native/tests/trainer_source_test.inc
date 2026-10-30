#include "trainer_pools.h"
static void CheckTrainerSource(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsLoadLibrary(path)); CHECK(!RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhBattleService *service = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 1));
    CHECK(service && service->header.struct_size >= sizeof(*service)
        && RHH_BATTLE_SERVICE_V1_0_SIZE < sizeof(*service));
    CHECK(RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 2)));
    CHECK(RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 3)));
    CHECK(RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 4)));
    CHECK(RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 5)));
    CHECK(RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 6)));
    CHECK(RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 7)));
    CHECK(RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 8)));
    CHECK(RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 9)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 10)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 11)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 12)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 13)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 14)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 15)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 16)) == &service->header
        && !RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 46)));
    const struct RhhTrainerRosterService *rosters = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/trainer_roster", RHH_SERVICE_VERSION(1, 0));
    CHECK(rosters && rosters->header.struct_size >= sizeof(*rosters));
    struct RhhTrainerRoster roster = {.struct_size = sizeof(roster)};
    RhhContentKey trainerKey = RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1);
    struct RhhTrainerRoster untouched = roster;
    RhhModsSetRunContext(0, 0, 0);
    CHECK(rosters->get(trainerKey, &roster) && !memcmp(&roster, &untouched, sizeof(roster)));
    RhhModsSetRunContext(123, 456, 0);
    struct RhhTrainerRoster smallRoster = {.struct_size = sizeof(smallRoster) - 1, .party_size = 99}, oldSmallRoster = smallRoster;
    CHECK(rosters->get(trainerKey, &smallRoster) && !memcmp(&smallRoster, &oldSmallRoster, sizeof(smallRoster)));
    CHECK(rosters->get(0, &roster) && !memcmp(&roster, &untouched, sizeof(roster)));
    gIsDebugBattle = TRUE; CHECK(rosters->get(trainerKey, &roster) && !memcmp(&roster, &untouched, sizeof(roster)));
    gIsDebugBattle = FALSE;
    rng_value_t beforeRoster = gRngValue;
    CHECK(!rosters->get(trainerKey, &roster) && roster.trainer == trainerKey && roster.roster_trainer == trainerKey
        && roster.source_count == 3 && roster.party_size == 3 && roster.roster_difficulty == RHH_TRAINER_DIFFICULTY_NORMAL);
    struct RhhTrainerRosterEntry entry = {.struct_size = sizeof(entry)};
    CHECK(!rosters->entry(&roster, 2, &entry) && entry.index == 2 && entry.level == 15
        && entry.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_NOSEPASS) && !entry.flags
        && !memcmp(&beforeRoster, &gRngValue, sizeof(beforeRoster)));
    struct RhhTrainerRoster stale = roster; stale.token ^= 1;
    struct RhhTrainerRosterEntry beforeEntry = entry;
    CHECK(rosters->entry(&stale, 2, &entry) && !memcmp(&entry, &beforeEntry, sizeof(entry)));
    CHECK(rosters->entry(&roster, roster.source_count, &entry) && !memcmp(&entry, &beforeEntry, sizeof(entry)));
    u32 matches = 99;
    CHECK(rosters->match(&roster, 2, RHH_BATTLE_OWNER_OPPONENT_A, 2, &matches) && matches == 99);
    struct RhhTrainerPartySource source = {.struct_size = sizeof(source), .source_index = 999};
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 0, &source) && source.source_index == 999);
    gBattleTypeFlags = BATTLE_TYPE_TRAINER; TRAINER_BATTLE_PARAM.opponentA = TRAINER_ROXANNE_1;
    TRAINER_BATTLE_PARAM.opponentB = 0xffff;
    const struct Trainer *trainer = GetTrainerStructFromId(TRAINER_ROXANNE_1);
    CHECK(trainer && trainer->partySize == 3 && !trainer->poolSize && trainer->party[2].species == SPECIES_NOSEPASS);
    CreateNPCTrainerPartyFromTrainer(gParties[B_TRAINER_OPPONENT_A], trainer);
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][2], MON_DATA_SPECIES) == SPECIES_MUDKIP);
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 2, &source));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct BattleStruct battle = {0}; gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattlersCount = 2; gAbsentBattlerFlags = 0; gBattleOutcome = 0; gBattleTurnCounter = 0;
    gBattlerPositions[0] = 0; gBattlerPositions[1] = 1; gBattlerPartyIndexes[0] = 0; gBattlerPartyIndexes[1] = 2;
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][2], &gBattleMons[1]);
    ModsBattleBegin();
    rng_value_t rng = gRngValue;
    CHECK(!service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 2, &source));
    CHECK(source.trainer == RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1) && source.roster_trainer == source.trainer
        && source.source_difficulty == RHH_TRAINER_DIFFICULTY_NORMAL
        && source.source_index == 2 && source.source_count == 3 && !source.flags
        && source.source_species == RhhContentKeyForId(RHH_SPECIES, SPECIES_NOSEPASS)
        && source.generated_species == RhhContentKeyForId(RHH_SPECIES, SPECIES_MUDKIP));
    CHECK(!rosters->match(&roster, 2, RHH_BATTLE_OWNER_OPPONENT_A, 2, &matches) && matches == 1);
    CHECK(!rosters->match(&roster, 0, RHH_BATTLE_OWNER_OPPONENT_A, 2, &matches) && !matches);
    matches = 99; CHECK(rosters->match(&stale, 2, RHH_BATTLE_OWNER_OPPONENT_A, 2, &matches) && matches == 99);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)));
    struct Pokemon pristine = gParties[B_TRAINER_OPPONENT_A][2];
    gParties[B_TRAINER_OPPONENT_A][2].box.secure.raw[0] ^= 1;
    struct Pokemon corrupt = gParties[B_TRAINER_OPPONENT_A][2];
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 2, &source));
    CHECK(!memcmp(&corrupt, &gParties[B_TRAINER_OPPONENT_A][2], sizeof(corrupt)));
    gParties[B_TRAINER_OPPONENT_A][2] = pristine;
    struct RhhTrainerPartySource small = {.struct_size = sizeof(small) - 1, .source_index = 999};
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 2, &small) && small.source_index == 999);
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_PLAYER, 0, &source));
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, PARTY_SIZE, &source));
    /* The generated identity follows the Pokémon if party slots are reordered. */
    struct Pokemon temp = gParties[B_TRAINER_OPPONENT_A][1];
    gParties[B_TRAINER_OPPONENT_A][1] = gParties[B_TRAINER_OPPONENT_A][2];
    gParties[B_TRAINER_OPPONENT_A][2] = temp; gBattlerPartyIndexes[1] = 1;
    CHECK(!service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &source)
        && source.party_slot == 1 && source.source_index == 2);
    CHECK(!rosters->match(&roster, 2, RHH_BATTLE_OWNER_OPPONENT_A, 1, &matches) && matches == 1);
    /* A duplicate live identity must not be attributed to either copy. */
    struct Pokemon saved = gParties[B_TRAINER_OPPONENT_A][0];
    gParties[B_TRAINER_OPPONENT_A][0] = gParties[B_TRAINER_OPPONENT_A][1];
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &source));
    matches = 99; CHECK(rosters->match(&roster, 2, RHH_BATTLE_OWNER_OPPONENT_A, 1, &matches) && matches == 99);
    gParties[B_TRAINER_OPPONENT_A][0] = saved;
    /* Tags come from source definitions; untagged leaders are never guessed. */
    ModsTrainerPartyClear(gParties[B_TRAINER_OPPONENT_A]);
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &source));
    struct TrainerMon tagged = trainer->party[2]; tagged.tags = MON_POOL_TAG_LEAD | MON_POOL_TAG_ACE;
    ModsTrainerPartyRecord(gParties[B_TRAINER_OPPONENT_A], 1, TRAINER_ROXANNE_1, TRAINER_ROXANNE_1, 2, 3, &tagged);
    struct Pokemon other = gParties[B_TRAINER_OPPONENT_A][2];
    gParties[B_TRAINER_OPPONENT_A][2] = gParties[B_TRAINER_OPPONENT_A][1];
    ModsTrainerPartyRecord(gParties[B_TRAINER_OPPONENT_A], 2, TRAINER_ROXANNE_1, TRAINER_ROXANNE_1, 2, 3, &tagged);
    gParties[B_TRAINER_OPPONENT_A][2] = other;
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &source));
    ModsTrainerPartyClear(gParties[B_TRAINER_OPPONENT_A]);
    ModsTrainerPartyRecord(gParties[B_TRAINER_OPPONENT_A], 1, TRAINER_ROXANNE_1, TRAINER_ROXANNE_1, 2, 3, &tagged);
    CHECK(!service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &source)
        && source.flags == (RHH_TRAINER_SOURCE_LEAD | RHH_TRAINER_SOURCE_ACE));
    matches = 99; CHECK(rosters->match(&roster, 2, RHH_BATTLE_OWNER_OPPONENT_A, 1, &matches) && matches == 99);
    const struct Trainer *override = GetTrainerStructFromId(TRAINER_BRAWLY_1);
    CHECK(override && override->partySize);
    ModsTrainerPartyClear(gParties[B_TRAINER_OPPONENT_A]);
    ModsTrainerPartyRecord(gParties[B_TRAINER_OPPONENT_A], 1, TRAINER_ROXANNE_1, TRAINER_BRAWLY_1, 0,
                           override->poolSize ? override->poolSize : override->partySize, &override->party[0]);
    CHECK(!service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &source)
        && source.trainer == RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1)
        && source.roster_trainer == RhhContentKeyForId(RHH_TRAINER, TRAINER_BRAWLY_1));
    matches = 99; CHECK(rosters->match(&roster, 0, RHH_BATTLE_OWNER_OPPONENT_A, 1, &matches) && matches == 99);
    TRAINER_BATTLE_PARAM.opponentA = TRAINER_BRAWLY_1;
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &source));
    TRAINER_BATTLE_PARAM.opponentA = TRAINER_ROXANNE_1;
    gBattleTypeFlags |= BATTLE_TYPE_BATTLE_TOWER;
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &source));
    gBattleTypeFlags = BATTLE_TYPE_TRAINER; gBattleOutcome = B_OUTCOME_WON; ModsBattleEnd(gBattleOutcome, FALSE);
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &source));
    gBattleOutcome = 0; ModsBattleBegin();
    CHECK(service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &source));
    gBattleOutcome = B_OUTCOME_WON; ModsBattleEnd(gBattleOutcome, FALSE);
    gMain.inBattle = FALSE; gBattleStruct = NULL; RhhModsUnloadLibraries();
    puts("trainer source: real generation, randomization, reordering, tags and ambiguity passed");
}
#ifdef RHH_TRAINER_SOURCE_TEST
void RhhTrainerSourceTestCreate(struct Pokemon *party, const struct Trainer *trainer, u16 trainerId, u16 rosterTrainerId);
void RhhTrainerRosterTestOverride(u32 trainerId, const struct Trainer *trainer, u32 sourceId, const struct Trainer *source);
static void CheckTrainerPoolSource(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsLoadLibrary(path)); CHECK(!RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhBattleService *service = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 1));
    CHECK(service && service->header.struct_size >= sizeof(*service));
    const struct RhhTrainerRosterService *rosters = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/trainer_roster", RHH_SERVICE_VERSION(1, 0));
    struct RhhTrainerRoster rosterA = {.struct_size = sizeof(rosterA)}, rosterB = {.struct_size = sizeof(rosterB)};
    CHECK(rosters && !rosters->get(RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1), &rosterA)
        && !rosters->get(RhhContentKeyForId(RHH_TRAINER, TRAINER_BRAWLY_1), &rosterB));
    struct TrainerMon entries[4] = {
        {.species = SPECIES_GEODUDE, .lvl = 10, .ball = POKEBALL_COUNT, .gender = TRAINER_MON_RANDOM_GENDER},
        {.species = SPECIES_ZIGZAGOON, .lvl = 20, .ball = POKEBALL_COUNT, .gender = TRAINER_MON_RANDOM_GENDER, .tags = MON_POOL_TAG_ACE},
        {.species = SPECIES_NOSEPASS, .lvl = 30, .ball = POKEBALL_COUNT, .gender = TRAINER_MON_RANDOM_GENDER},
        {.species = SPECIES_TORCHIC, .lvl = 25, .ball = POKEBALL_COUNT, .gender = TRAINER_MON_RANDOM_GENDER, .tags = MON_POOL_TAG_LEAD}
    };
    struct Trainer trainer = *GetTrainerStructFromId(TRAINER_ROXANNE_1);
    trainer.party = entries; trainer.partySize = 2; trainer.poolSize = 4;
    trainer.poolRuleIndex = POOL_RULESET_BASIC; trainer.poolPickIndex = POOL_PICK_DEFAULT; trainer.poolPruneIndex = POOL_PRUNE_NONE;
    struct Trainer rosterDefinition = trainer;
    trainer.overrideTrainer = TRAINER_BRAWLY_1;
    RhhTrainerRosterTestOverride(TRAINER_ROXANNE_1, &trainer, TRAINER_BRAWLY_1, &rosterDefinition);
    struct RhhTrainerRoster pool = {.struct_size = sizeof(pool)};
    struct RhhTrainerRosterEntry poolAce = {.struct_size = sizeof(poolAce)};
    CHECK(!rosters->get(RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1), &pool)
        && pool.flags == (RHH_TRAINER_ROSTER_POOL | RHH_TRAINER_ROSTER_OVERRIDE)
        && pool.roster_trainer == RhhContentKeyForId(RHH_TRAINER, TRAINER_BRAWLY_1)
        && pool.party_size == 2 && pool.source_count == 4
        && !rosters->entry(&pool, 1, &poolAce) && poolAce.flags == RHH_TRAINER_SOURCE_ACE
        && poolAce.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_ZIGZAGOON));
    gBattleTypeFlags = BATTLE_TYPE_TRAINER; TRAINER_BATTLE_PARAM.opponentA = TRAINER_ROXANNE_1;
    TRAINER_BATTLE_PARAM.opponentB = 0xffff;
    RhhTrainerSourceTestCreate(gParties[B_TRAINER_OPPONENT_A], &trainer, TRAINER_ROXANNE_1, TRAINER_BRAWLY_1);
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_TORCHIC);
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][1], MON_DATA_SPECIES) == SPECIES_MUDKIP);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct BattleStruct battle = {0}; gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattlersCount = 2; gAbsentBattlerFlags = 0; gBattleOutcome = 0; gBattleTurnCounter = 0;
    gBattlerPositions[0] = 0; gBattlerPositions[1] = 1; gBattlerPartyIndexes[0] = 0; gBattlerPartyIndexes[1] = 1;
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][1], &gBattleMons[1]);
    ModsBattleBegin();
    struct RhhTrainerPartySource ace = {.struct_size = sizeof(ace)}, lead = {.struct_size = sizeof(lead)};
    CHECK(!service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 0, &lead)
        && !service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_A, 1, &ace));
    CHECK(lead.source_index == 3 && lead.flags == RHH_TRAINER_SOURCE_LEAD
        && ace.source_index == 1 && ace.source_count == 4 && ace.flags == RHH_TRAINER_SOURCE_ACE
        && ace.source_species == RhhContentKeyForId(RHH_SPECIES, SPECIES_ZIGZAGOON)
        && ace.generated_species == RhhContentKeyForId(RHH_SPECIES, SPECIES_MUDKIP)
        && ace.trainer == RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1)
        && ace.roster_trainer == RhhContentKeyForId(RHH_TRAINER, TRAINER_BRAWLY_1));
    u32 matches = 99;
    CHECK(!rosters->match(&pool, 1, RHH_BATTLE_OWNER_OPPONENT_A, 1, &matches) && matches == 1);
    CHECK(!rosters->match(&pool, 3, RHH_BATTLE_OWNER_OPPONENT_A, 0, &matches) && matches == 1);
    entries[1].iv ^= 1; matches = 99;
    CHECK(rosters->match(&pool, 1, RHH_BATTLE_OWNER_OPPONENT_A, 1, &matches) && matches == 99);
    entries[1].iv ^= 1;
    CHECK(!rosters->match(&pool, 1, RHH_BATTLE_OWNER_OPPONENT_A, 1, &matches) && matches == 1);
    gBattleOutcome = B_OUTCOME_WON; ModsBattleEnd(gBattleOutcome, FALSE);
    RhhTrainerRosterTestOverride(0, NULL, 0, NULL);
    /* Real generation of two trainers keeps their separately indexed parties. */
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_TWO_OPPONENTS;
    TRAINER_BATTLE_PARAM.opponentA = TRAINER_ROXANNE_1; TRAINER_BATTLE_PARAM.opponentB = TRAINER_BRAWLY_1;
    CreateNPCTrainerPartyFromTrainer(gParties[B_TRAINER_OPPONENT_A], GetTrainerStructFromId(TRAINER_ROXANNE_1));
    CreateNPCTrainerPartyFromTrainer(gParties[B_TRAINER_OPPONENT_B], GetTrainerStructFromId(TRAINER_BRAWLY_1));
    gBattlersCount = 4; gAbsentBattlerFlags = 1u << 2; gBattleOutcome = 0;
    gBattlerPositions[0] = 0; gBattlerPositions[1] = 1; gBattlerPositions[2] = 2; gBattlerPositions[3] = 3;
    gBattlerPartyIndexes[0] = gBattlerPartyIndexes[1] = gBattlerPartyIndexes[3] = 0;
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_B][0], &gBattleMons[3]);
    ModsBattleBegin(); ace.struct_size = sizeof(ace);
    CHECK(!service->trainer_party_source(RHH_BATTLE_OWNER_OPPONENT_B, 0, &ace)
        && ace.trainer == RhhContentKeyForId(RHH_TRAINER, TRAINER_BRAWLY_1)
        && ace.roster_trainer == ace.trainer && ace.source_index == 0);
    CHECK(!rosters->match(&rosterB, 0, RHH_BATTLE_OWNER_OPPONENT_B, 0, &matches) && matches == 1);
    CHECK(!rosters->match(&rosterA, 0, RHH_BATTLE_OWNER_OPPONENT_B, 0, &matches) && matches == 0);
    gBattleOutcome = B_OUTCOME_WON; ModsBattleEnd(gBattleOutcome, FALSE);
    gMain.inBattle = FALSE; gBattleStruct = NULL; RhhModsUnloadLibraries();
    puts("trainer pool source: selected lead and ace retained through randomization and override passed");
}
#endif
