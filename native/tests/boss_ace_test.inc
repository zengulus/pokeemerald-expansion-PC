#ifdef RHH_TRAINER_SOURCE_TEST
void RhhTrainerSourceTestDifficulty(u32 owner, u32 slot, u32 difficulty);
#endif
static void CheckBossAce(const char *path, const char *module, bool32 load)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhBossAceService *aces = (const void *)host->get_service("rhh:service/boss_ace", RHH_SERVICE_VERSION(1, 0));
    const struct RhhTrainerRosterService *rosters = (const void *)host->get_service("rhh:service/trainer_roster", RHH_SERVICE_VERSION(1, 0));
    CHECK(aces && rosters && !host->get_service("rhh:service/boss_ace", RHH_SERVICE_VERSION(1, 1)));
    RhhContentKey trainer = RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1);
    RhhContentKey owner = RhhContentKeyOf("test:boss_ace_probe"), other = RhhContentKeyOf("test:music_catalog");
    struct RhhBossAceSelection selection = {.struct_size = sizeof(selection)};
    struct RhhBossAceResult result = {.struct_size = sizeof(result)};
    struct RhhBossAceMatch match = {.struct_size = sizeof(match)};
    if (!load)
    {
        CHECK(aces->get(trainer, RHH_TRAINER_DIFFICULTY_NORMAL, &selection));
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
        struct RhhTrainerRoster roster = {.struct_size = sizeof(roster)};
        CHECK(!rosters->get(trainer, &roster) && roster.trainer_difficulty == RHH_TRAINER_DIFFICULTY_NORMAL);
        CHECK(!aces->count() && !aces->get(trainer, roster.trainer_difficulty, &selection)
            && !selection.valid && !selection.revision);
        struct RhhBossAceResult small = {.struct_size = sizeof(small) - 1, .status = 99}, oldSmall = small;
        CHECK(aces->context(&result) && aces->apply(NULL, &small) == RHH_BOSS_ACE_INVALID
            && !memcmp(&small, &oldSmall, sizeof(small)));
        struct RhhBossAceRequest request = {.struct_size = sizeof(request), .action = RHH_BOSS_ACE_SET,
            .trainer_difficulty = roster.trainer_difficulty, .source_index = 2, .trainer = trainer,
            .owner = owner, .roster = roster};
        rng_value_t rng = gRngValue;
        CHECK(aces->preview(&request, &result) == RHH_BOSS_ACE_OK && result.is_preview && result.changed
            && !result.before.valid && result.after.valid && result.after.source_index == 2 && result.after.revision == 1
            && !aces->count() && !memcmp(&rng, &gRngValue, sizeof(rng)));
        request.source_index = 0;
        CHECK(aces->apply(&request, &result) == RHH_BOSS_ACE_DENIED && !strcmp(result.reason, "test denied") && !aces->count());
        request.source_index = 2; request.expected_revision = 1;
        CHECK(aces->apply(&request, &result) == RHH_BOSS_ACE_STALE_REQUEST && !aces->count());
        request.expected_revision = 0; request.roster.token ^= 1;
        CHECK(aces->apply(&request, &result) == RHH_BOSS_ACE_STALE_REQUEST && !aces->count());
        request.roster = roster;
        CHECK(!NativeSaveBegin() && NativeSaveTransactionActive());
        CHECK(aces->apply(&request, &result) == RHH_BOSS_ACE_UNAVAILABLE_COMMAND);
        CHECK(NativeSaveEnd(1, FALSE) && !NativeSaveTransactionActive());
        CHECK(aces->apply(&request, &result) == RHH_BOSS_ACE_OK && result.changed && result.after.revision == 1
            && aces->count() == 1 && !aces->get(trainer, roster.trainer_difficulty, &selection)
            && selection.valid && !selection.flags && selection.owner == owner && selection.source_index == 2);
        request.expected_revision = 1;
        CHECK(aces->apply(&request, &result) == RHH_BOSS_ACE_OK && !result.changed && result.after.revision == 1);
        request.owner = other; request.source_index = 1;
        CHECK(aces->apply(&request, &result) == RHH_BOSS_ACE_CONFLICT);
        request.action = RHH_BOSS_ACE_CLEAR; request.source_index = 0;
        CHECK(aces->apply(&request, &result) == RHH_BOSS_ACE_CONFLICT);
        request.action = RHH_BOSS_ACE_SET;
        request.owner = owner; request.source_index = 2;
        struct RhhBossAceSelection at = {.struct_size = sizeof(at)};
        CHECK(!aces->at(0, &at) && at.trainer == trainer && at.source_index == 2 && aces->at(1, &at));
        u8 *state = RhhModsState("rhh:boss_ace_state"); CHECK(state);
        ClockTestPut32(state + 8 + 44, RHH_TRAINER_DIFFICULTY_EASY);
        CHECK(!aces->get(trainer, RHH_TRAINER_DIFFICULTY_EASY, &selection) && selection.valid
            && (selection.flags & RHH_BOSS_ACE_INACTIVE));
        ClockTestPut32(state + 8 + 44, RHH_TRAINER_DIFFICULTY_NORMAL);
        CHECK(!aces->get(trainer, roster.trainer_difficulty, &selection) && selection.valid && !selection.flags);
        ClockTestPut64(state, UINT64_MAX); request.expected_revision = UINT64_MAX; request.source_index = 1;
        CHECK(aces->apply(&request, &result) == RHH_BOSS_ACE_UNAVAILABLE_COMMAND);
        ClockTestPut64(state, 1); request.expected_revision = 1; request.source_index = 2;
        u8 savedState[8 + RHH_BOSS_ACE_CAPACITY * 64]; memcpy(savedState, state, sizeof(savedState));
        memset(state, 0, sizeof(savedState)); ClockTestPut64(state, 10); CHECK(trainer > RHH_BOSS_ACE_CAPACITY);
        for (u32 i = 0; i < RHH_BOSS_ACE_CAPACITY; ++i)
        {
            u8 *record = state + 8 + i * 64;
            ClockTestPut64(record, i + 1); ClockTestPut64(record + 8, owner);
            ClockTestPut64(record + 24, trainer); ClockTestPut64(record + 32, RhhContentKeyForId(RHH_SPECIES, SPECIES_TORCHIC));
            ClockTestPut32(record + 44, RHH_TRAINER_DIFFICULTY_NORMAL);
            ClockTestPut32(record + 48, RHH_TRAINER_DIFFICULTY_NORMAL); ClockTestPut32(record + 52, 5);
        }
        request.expected_revision = 10;
        CHECK(aces->count() == RHH_BOSS_ACE_CAPACITY && aces->apply(&request, &result) == RHH_BOSS_ACE_FULL);
        memcpy(state, savedState, sizeof(savedState)); request.expected_revision = 1;
        u8 *snapshot = NULL; u32 size = 0; CHECK(!RhhModsExport(&snapshot, &size));
        u8 *bad = malloc(size); CHECK(bad); memcpy(bad, snapshot, size); u32 offset = 24;
        while (offset + 80 <= size && strcmp((const char *)bad + offset, "rhh:boss_ace_state"))
            offset += 80 + ClockTestGet32(bad + offset + 68);
        CHECK(offset + 80 + 8 + 64 <= size); ClockTestPut32(bad + offset + 80 + 8 + 44, 0);
        ClockTestEnvelope(bad, size);
        CHECK(RhhModsImport(bad, size) && aces->count() == 1);
        memcpy(bad, snapshot, size); u32 removed = 80 + ClockTestGet32(bad + offset + 68);
        memmove(bad + offset, bad + offset + removed, size - offset - removed);
        ClockTestPut32(bad + 12, ClockTestGet32(bad + 12) - 1); ClockTestEnvelope(bad, size - removed);
        CHECK(!RhhModsImport(bad, size - removed) && !aces->count());
        CHECK(!RhhModsImport(snapshot, size) && aces->count() == 1); free(snapshot); free(bad);
        gBattleTypeFlags = BATTLE_TYPE_TRAINER; TRAINER_BATTLE_PARAM.opponentA = TRAINER_ROXANNE_1;
        TRAINER_BATTLE_PARAM.opponentB = 0xffff;
        CreateNPCTrainerPartyFromTrainer(gParties[B_TRAINER_OPPONENT_A], GetTrainerStructFromId(TRAINER_ROXANNE_1));
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
        gPartiesCount[B_TRAINER_PLAYER] = 1;
        struct BattleStruct battle = {0}; gBattleStruct = &battle; gMain.inBattle = TRUE;
        gBattlersCount = 2; gAbsentBattlerFlags = 0; gBattleOutcome = 0; gBattleTurnCounter = 0;
        gBattlerPositions[0] = 0; gBattlerPositions[1] = 1; gBattlerPartyIndexes[0] = 0; gBattlerPartyIndexes[1] = 2;
        PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
        PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][2], &gBattleMons[1]); ModsBattleBegin();
        CHECK(!aces->match(RHH_BATTLE_OWNER_OPPONENT_A, 2, &match) && match.matched && match.selection.source_index == 2
            && match.source.source_species == RhhContentKeyForId(RHH_SPECIES, SPECIES_NOSEPASS)
            && match.source.generated_species == RhhContentKeyForId(RHH_SPECIES, SPECIES_MUDKIP));
        CHECK(!aces->match(RHH_BATTLE_OWNER_OPPONENT_A, 0, &match) && !match.matched);
#ifdef RHH_TRAINER_SOURCE_TEST
        struct RhhBossAceMatch beforeDifficulty = match;
        RhhTrainerSourceTestDifficulty(RHH_BATTLE_OWNER_OPPONENT_A, 2, DIFFICULTY_EASY);
        CHECK(aces->match(RHH_BATTLE_OWNER_OPPONENT_A, 2, &match) && !memcmp(&match, &beforeDifficulty, sizeof(match)));
        RhhTrainerSourceTestDifficulty(RHH_BATTLE_OWNER_OPPONENT_A, 2, DIFFICULTY_NORMAL);
#endif
        struct Pokemon swapped = gParties[B_TRAINER_OPPONENT_A][1];
        gParties[B_TRAINER_OPPONENT_A][1] = gParties[B_TRAINER_OPPONENT_A][2];
        gParties[B_TRAINER_OPPONENT_A][2] = swapped; gBattlerPartyIndexes[1] = 1;
        CHECK(!aces->match(RHH_BATTLE_OWNER_OPPONENT_A, 1, &match) && match.matched);
        gParties[B_TRAINER_OPPONENT_A][2] = gParties[B_TRAINER_OPPONENT_A][1];
        gParties[B_TRAINER_OPPONENT_A][1] = swapped; gBattlerPartyIndexes[1] = 2;
        CHECK(aces->apply(&request, &result) == RHH_BOSS_ACE_UNAVAILABLE_COMMAND);
        struct Pokemon original = gParties[B_TRAINER_OPPONENT_A][0];
        gParties[B_TRAINER_OPPONENT_A][0] = gParties[B_TRAINER_OPPONENT_A][2];
        struct RhhBossAceMatch oldMatch = match;
        CHECK(aces->match(RHH_BATTLE_OWNER_OPPONENT_A, 2, &match) && !memcmp(&match, &oldMatch, sizeof(match)));
        gParties[B_TRAINER_OPPONENT_A][0] = original;
        state = RhhModsState("rhh:boss_ace_state"); CHECK(state);
        u64 token = selection.token; ClockTestPut64(state + 8 + 16, token ^ 1);
        CHECK(!aces->get(trainer, roster.trainer_difficulty, &selection) && (selection.flags & RHH_BOSS_ACE_STALE)
            && !aces->match(RHH_BATTLE_OWNER_OPPONENT_A, 2, &match) && !match.matched);
        ClockTestPut64(state + 8 + 16, token);
        gBattleOutcome = B_OUTCOME_WON; ModsBattleEnd(gBattleOutcome, FALSE); gMain.inBattle = FALSE; gBattleStruct = NULL;
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        request.source_index = 1;
        CHECK(aces->apply(&request, &result) == RHH_BOSS_ACE_OK && result.after.revision == 2);
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && !aces->get(trainer, roster.trainer_difficulty, &selection)
            && selection.valid && selection.source_index == 2 && selection.revision == 1);
        puts("boss ace: preview, constraints, battle match and rollback passed");
    }
    else
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && aces->count() == 1
            && !aces->get(trainer, RHH_TRAINER_DIFFICULTY_NORMAL, &selection) && selection.valid
            && selection.owner == owner && selection.source_index == 2 && selection.revision == 1 && !selection.flags);
        u8 *state = RhhModsState("rhh:boss_ace_state"); CHECK(state);
        u8 previousRecord[64]; memcpy(previousRecord, state + 8, sizeof(previousRecord));
        ClockTestPut64(state + 8 + 8, RhhContentKeyOf("test:missing_owner"));
        CHECK(!aces->get(trainer, RHH_TRAINER_DIFFICULTY_NORMAL, &selection) && (selection.flags & RHH_BOSS_ACE_ORPHANED));
        struct RhhBossAceRequest clear = {.struct_size = sizeof(clear), .action = RHH_BOSS_ACE_CLEAR,
            .trainer = trainer, .owner = other, .trainer_difficulty = RHH_TRAINER_DIFFICULTY_NORMAL, .expected_revision = 1};
        CHECK(aces->apply(&clear, &result) == RHH_BOSS_ACE_OK && result.changed && !result.after.valid
            && result.after.revision == 2 && !aces->count());
        RhhContentKey removedTrainer = RhhContentKeyOf("test:removed_trainer");
        memcpy(state + 8, previousRecord, sizeof(previousRecord));
        ClockTestPut64(state + 8, removedTrainer); ClockTestPut64(state + 8 + 8, other);
        CHECK(!aces->get(removedTrainer, RHH_TRAINER_DIFFICULTY_NORMAL, &selection)
            && selection.valid && (selection.flags & RHH_BOSS_ACE_UNAVAILABLE));
        clear.trainer = removedTrainer; clear.expected_revision = 2;
        CHECK(aces->apply(&clear, &result) == RHH_BOSS_ACE_OK && result.after.revision == 3 && !aces->count());
        clear.expected_revision = 3;
        CHECK(aces->apply(&clear, &result) == RHH_BOSS_ACE_OK && !result.changed && result.after.revision == 3);
        puts("boss ace: fresh-process restore and orphan cleanup passed");
    }
    NativeSaveClose(); RhhModsUnloadLibraries();
}
