static void AbilityObservationFrameTick(void)
{
    gBattleMons[0].volatiles.gastroAcid = TRUE;
    gMain.callback1 = NULL;
}
static void CheckAbilities(const char *module, const char *observer)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module)
        && !RhhModsLoadLibrary(observer) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhAbilityService *abilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 0));
    const struct RhhOwnedMonService *owned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    CHECK(abilities && owned && abilities->header.struct_size >= sizeof(*abilities));
    CHECK(host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 10)) == &abilities->header);
    CHECK(host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 23)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 28)) == &abilities->header
        && host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 29)) == &abilities->header
        && !host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 41)));
    struct RhhBattlerAbilityObservation outside = {.struct_size = sizeof(outside), .sequence = 99}, oldOutside = outside;
    CHECK(abilities->ability_observation_context(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    char abilityName[33] = "unchanged";
    CHECK(!abilities->name(RhhContentKeyForId(RHH_ABILITY, ABILITY_LEVITATE), abilityName, sizeof(abilityName))
        && !strcmp(abilityName, "Levitate"));
    strcpy(abilityName, "unchanged");
    CHECK(abilities->name(RhhContentKeyForId(RHH_ABILITY, ABILITY_LEVITATE), abilityName, 3)
        && !strcmp(abilityName, "unchanged")
        && abilities->name(RhhContentKeyOf("test:ability/missing"), abilityName, sizeof(abilityName))
        && !strcmp(abilityName, "unchanged"));
    struct RhhAbilityInfo info = {.struct_size = sizeof(info)};
    CHECK(!abilities->info(RhhContentKeyForId(RHH_ABILITY, ABILITY_MULTITYPE), &info)
        && (info.flags & RHH_ABILITY_CANNOT_SUPPRESS));
    CHECK(!abilities->info(RhhContentKeyForId(RHH_ABILITY, ABILITY_LEVITATE), &info)
        && (info.flags & RHH_ABILITY_BREAKABLE));
    struct RhhSpeciesAbilities species = {.struct_size = sizeof(species)};
    CHECK(!abilities->species(RhhContentKeyForId(RHH_SPECIES, SPECIES_TORCHIC), &species)
        && species.slot_count == NUM_ABILITY_SLOTS && species.normal_slot_count == NUM_NORMAL_ABILITY_SLOTS
        && species.slots[0] == RhhContentKeyForId(RHH_ABILITY, GetSpeciesAbility(SPECIES_TORCHIC, 0)));
    struct RhhSpeciesAbilities invalid = {.struct_size = sizeof(invalid), .slot_count = 99}, savedInvalid = invalid;
    CHECK(abilities->species(RhhContentKeyOf("test:species/missing"), &invalid)
        && !memcmp(&invalid, &savedInvalid, sizeof(invalid)));
    struct BattleResources resources = {0}; struct AiLogicData ai = {0};
    gBattleResources = &resources; gAiLogicData = &ai;
    PrepareTurnBattle();
    gBattlersCount = 2; gBattleTypeFlags &= ~BATTLE_TYPE_DOUBLE;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_POOCHYENA, 20, 555, OTID_STRUCT_PRESET(222), 31);
    u32 slot = 1; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ABILITY_NUM, &slot);
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)}; CHECK(!owned->party_at(0, &mon));
    struct RhhOwnedAbility selected = {.struct_size = sizeof(selected)};
    enum Ability last = gLastUsedAbility; rng_value_t rng = gRngValue;
    struct Pokemon before = gParties[B_TRAINER_PLAYER][0];
    CHECK(!abilities->owned(&mon, &selected) && selected.selected_slot == slot
        && selected.ability == RhhContentKeyForId(RHH_ABILITY, GetAbilityBySpeciesReadOnly(SPECIES_TORCHIC, slot))
        && gLastUsedAbility == last && !memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before))
        && !memcmp(&rng, &gRngValue, sizeof(rng)));
    struct BoxPokemon savedBox = gPokemonStoragePtr->boxes[0][0];
    gPokemonStoragePtr->boxes[0][0] = before.box;
    struct RhhOwnedAbility failed = {.struct_size = sizeof(failed), .selected_slot = 99}, oldFailed = failed;
    CHECK(abilities->owned(&mon, &failed) && !memcmp(&failed, &oldFailed, sizeof(failed)));
    gPokemonStoragePtr->boxes[0][0] = savedBox;
    struct Pokemon savedFusion = gPokemonStoragePtr->fusions[0];
    gPokemonStoragePtr->fusions[0] = before;
    CHECK(abilities->owned(&mon, &failed) && !memcmp(&failed, &oldFailed, sizeof(failed)));
    gPokemonStoragePtr->fusions[0] = savedFusion;
    ++mon.level; CHECK(abilities->owned(&mon, &failed) && !memcmp(&failed, &oldFailed, sizeof(failed)));
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
    gBattleMons[0].ability = ABILITY_LEVITATE; gBattleMons[0].item = ITEM_NONE;
    gBattleMons[1].ability = ABILITY_NEUTRALIZING_GAS;
    gBattlerAttacker = 1; gFieldStatuses = 0; ModsBattleBegin();
    RhhModsObserveBattleAbilities(); /* Initial samples do not notify. */
    struct RhhBattlerAbility battle = {.struct_size = sizeof(battle)};
    gPotentialItemEffectBattler = 3; last = gLastUsedAbility; rng = gRngValue;
    struct BattleStruct oldBattle = *gBattleStruct; struct BattlePokemon beforeBattle = gBattleMons[0];
    CHECK(!abilities->battler(0, &battle) && battle.stored == RhhContentKeyForId(RHH_ABILITY, ABILITY_LEVITATE)
        && battle.active == battle.stored && battle.active_ignoring_mold_breaker == battle.stored
        && gPotentialItemEffectBattler == 3 && gLastUsedAbility == last && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&oldBattle, gBattleStruct, sizeof(oldBattle)) && !memcmp(&beforeBattle, &gBattleMons[0], sizeof(beforeBattle)));
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); cgb_audio_init(42060); m4aSoundInit();
    gMain.callback1 = AbilityObservationFrameTick; NativeFrame();
    CHECK(!gMain.callback1 && gBattleMons[0].volatiles.gastroAcid); SDL_Quit();
    CHECK(!abilities->battler(0, &battle) && !battle.active && !battle.active_ignoring_mold_breaker);
    gBattleMons[0].ability = ABILITY_MULTITYPE;
    RhhModsObserveBattleAbilities();
    CHECK(!abilities->battler(0, &battle) && battle.active == RhhContentKeyForId(RHH_ABILITY, ABILITY_MULTITYPE));
    gBattleMons[0].ability = ABILITY_LEVITATE; gBattleMons[0].volatiles.gastroAcid = FALSE;
    RhhModsObserveBattleAbilities();
    gBattleMons[1].volatiles.neutralizingGas = TRUE;
    RhhModsObserveBattleAbilities();
    CHECK(!abilities->battler(0, &battle) && !battle.active);
    gBattleMons[0].item = ITEM_ABILITY_SHIELD;
    RhhModsObserveBattleAbilities();
    CHECK(!abilities->battler(0, &battle) && battle.active == battle.stored);
    gBattleMons[0].volatiles.embargoTimer = 1;
    RhhModsObserveBattleAbilities();
    CHECK(!abilities->battler(0, &battle) && !battle.active);
    gBattleMons[0].volatiles.embargoTimer = 0; gBattleMons[0].item = ITEM_NONE;
    gBattleMons[1].volatiles.neutralizingGas = FALSE; gBattleStruct->moldBreakerActive = TRUE;
    gBattleMons[0].volatiles.overwrittenAbility = ABILITY_LEVITATE;
    RhhModsObserveBattleAbilities();
    CHECK(!abilities->battler(0, &battle) && !battle.active && battle.active_ignoring_mold_breaker == battle.stored
        && (battle.flags & RHH_BATTLER_ABILITY_OVERWRITTEN));
    gBattleStruct->moldBreakerActive = FALSE;
    RhhModsObserveBattleAbilities(); /* Mold Breaker alone does not notify. */
    ModsBattleSwitchPrepared(0);
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][1], &gBattleMons[0]); gBattlerPartyIndexes[0] = 1;
    RhhModsObserveBattleAbilities(); ModsBattleSwitchEntered(0); RhhModsObserveBattleAbilities();
    gBattleTypeFlags |= BATTLE_TYPE_LINK; RhhModsObserveBattleAbilities();
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; RhhModsObserveBattleAbilities();
    CHECK(ModsAiMoveScore(0, 1, MOVE_TACKLE, 0, 100) == 100);
    gBattleTypeFlags |= BATTLE_TYPE_LINK;
    struct RhhBattlerAbility failedBattle = {.struct_size = sizeof(failedBattle), .stored = 999}, oldBattleResult = failedBattle;
    CHECK(abilities->battler(0, &failedBattle) && !memcmp(&failedBattle, &oldBattleResult, sizeof(failedBattle)));
    ModsBattleReset(); gMain.inBattle = FALSE; gAiLogicData = NULL; RhhModsUnloadLibraries();
    puts("abilities: species, owned and pure battle state passed");
}

static void InvalidInnateTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; (void)decision; }
