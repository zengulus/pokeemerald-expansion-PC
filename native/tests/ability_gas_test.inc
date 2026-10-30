/* Observe actual Neutralizing Gas engine transitions and their affected views. */
extern void BS_TryEndNeutralizingGas(void);
static const struct RhhAbilityService *sGasAbilities;
static struct RhhGasChange sGasEvents[20];
static u32 sGasCount;
static void OfferGasTestInnate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .ability = RhhContentKeyForId(RHH_ABILITY, ABILITY_LEVITATE), .unlock_level = 1};
    CHECK(sGasAbilities->offer_innate(&offer) == RHH_INNATE_OK);
}
static void ObserveGas(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhGasChange change = {.struct_size = sizeof(change)}, small = {.struct_size = 1, .sequence = 99}, saved = small;
    CHECK(sGasCount < ARRAY_COUNT(sGasEvents) && !sGasAbilities->gas_change_context(&change)
        && sGasAbilities->gas_change_context(&small) && !memcmp(&small, &saved, sizeof(small))
        && change.sequence == event->ordinal && change.cause == event->args[0] && change.source_battler == event->args[1]
        && change.before.source_mask == event->args[2] && change.after.source_mask == event->args[3]
        && change.move == event->event_key && change.before.battle_id == change.battle_id
        && change.after.battle_id == change.battle_id && !RhhModsCommandsAllowed());
    sGasEvents[sGasCount++] = change;
    (void)Random(); (void)Random2();
}
static const struct RhhModHook sGasHooks[] = {
    {sizeof(struct RhhModHook), RHH_INNATE_PROPOSE, RHH_CONSTRAINT, 0, OfferGasTestInnate, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_NEUTRALIZING_GAS_CHANGED, RHH_OBSERVE, 0, NULL, ObserveGas}
};
static const struct RhhModDefinition sGasMod = {.struct_size = sizeof(sGasMod), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:gas_notifications", .hooks = sGasHooks, .hook_count = ARRAY_COUNT(sGasHooks)};
static u32 ActivateGas(u32 battler)
{
    static const u8 returnScript[] = {B_SCR_OP_END};
    gBattleResources->battleScriptsStack->size = 0; gBattlescriptCurrInstr = returnScript;
    return AbilityBattleEffects(ABILITYEFFECT_NEUTRALIZINGGAS, battler, ABILITY_NEUTRALIZING_GAS, MOVE_NONE, TRUE);
}
static void GasSwitchOut(void)
{
    const u8 command[] = {B_SCR_OP_SWITCHOUTABILITIES, BS_TARGET};
    gBattleResources->battleScriptsStack->size = 0; gBattlescriptCurrInstr = command;
    gBattleScriptingCommandsTable[B_SCR_OP_SWITCHOUTABILITIES]();
}
static void GasFaintCommand(void)
{
    static const u8 failure[] = {0};
    struct __attribute__((packed)) {u8 op, battler, spikes; const u8 *jump;} command = {B_SCR_OP_TRYFAINTMON, BS_TARGET, FALSE, failure};
    gBattleResources->battleScriptsStack->size = 0; gBattlescriptCurrInstr = (const u8 *)&command;
    gBattleScriptingCommandsTable[B_SCR_OP_TRYFAINTMON]();
}
static void GasFaintMoveEnd(void)
{
    static const u8 returnScript[] = {B_SCR_OP_END};
    gBattleResources->battleScriptsStack->size = 0; gBattlescriptCurrInstr = returnScript;
    gBattleScripting.moveendState = MOVEEND_FAINT_BLOCK;
    gBattleStruct->eventState.moveEndBlock = FAINT_BLOCK_END_NEUTRALIZING_GAS;
    CHECK(DoMoveEnd(1, MOVEEND_FAINT_BLOCK) == MOVEEND_RESULT_RUN_SCRIPT);
}
static void GasReplace(void)
{
    struct __attribute__((packed)) {u8 op; void (*func)(void); u8 battler;} command = {B_SCR_OP_CALLNATIVE, BS_TryEndNeutralizingGas, BS_TARGET};
    gBattleResources->battleScriptsStack->size = 0; gBattlescriptCurrInstr = (const u8 *)&command;
    BS_TryEndNeutralizingGas();
}
static const struct RhhBattleService *sGasWeatherBattle;
static struct RhhBattleWeatherSuppressionChange sGasWeatherEvents[12];
static u32 sGasWeatherCount;
static void ObserveGasWeather(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleWeatherSuppressionChange change = {.struct_size = sizeof(change)};
    struct RhhBattleWeatherSuppressionChange tiny = {.struct_size = 1, .cause = 99}, saved = tiny;
    struct RhhBattleEnvironmentState now = {.struct_size = sizeof(now)};
    CHECK(sGasWeatherCount < ARRAY_COUNT(sGasWeatherEvents)
        && !sGasWeatherBattle->weather_gas_suppression_change(&change)
        && sGasWeatherBattle->weather_gas_suppression_change(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny))
        && sGasWeatherBattle->weather_suppression_change(&change)
        && !sGasWeatherBattle->environment(&now) && now.weather_suppressed == change.after.weather_suppressed
        && change.sequence == sGasWeatherCount && event->ordinal == change.sequence && event->subject == change.after.battle_id
        && event->args[0] == change.cause && event->args[1] == change.source_battler
        && event->args[2] == change.before.weather_suppressed && event->args[3] == change.after.weather_suppressed
        && change.triggering_ability == RhhContentKeyForId(RHH_ABILITY, ABILITY_NEUTRALIZING_GAS)
        && event->event_key == (change.move ? change.move : change.triggering_ability) && !RhhModsCommandsAllowed());
    sGasWeatherEvents[sGasWeatherCount++] = change;
    (void)Random(); (void)Random2();
}
static const struct RhhModHook sGasWeatherHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_WEATHER_GAS_SUPPRESSION_CHANGED, RHH_OBSERVE, 0, NULL, ObserveGasWeather},
    {sizeof(struct RhhModHook), RHH_BATTLE_WEATHER_SUPPRESSION_CHANGED, RHH_OBSERVE, 0, NULL, ObserveWeatherSuppression}
};
static const struct RhhModDefinition sGasWeatherMod = {.struct_size = sizeof(sGasWeatherMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:gas_weather", .hooks = sGasWeatherHooks, .hook_count = ARRAY_COUNT(sGasWeatherHooks)};
static void CheckGasWeather(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sGasWeatherMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sGasWeatherBattle = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 38));
    sSuppressionBattle = sGasWeatherBattle;
    sSuppressionAbilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 36));
    struct RhhBattleWeatherSuppressionChange outside = {.struct_size = sizeof(outside), .cause = 99}, saved = outside;
    CHECK(sGasWeatherBattle && sSuppressionAbilities && RHH_BATTLE_SERVICE_V1_37_SIZE < RHH_BATTLE_SERVICE_V1_38_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 46))
        && sGasWeatherBattle->weather_gas_suppression_change(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct BattleHistory history = {0}; struct AiPartyData parties = {0}; struct AiLogicData ai = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources;
    gBattleHistory = &history; gAiPartyData = &parties; gAiLogicData = &ai;
    PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); gFieldStatuses = 0;
    gBattlerAttacker = 0; gBattlerTarget = 1; gCurrentMove = MOVE_TACKLE;
    gBattleMons[0].ability = ABILITY_CLOUD_NINE; gBattleMons[2].ability = ABILITY_BLAZE;
    gBattleMons[1].ability = gBattleMons[3].ability = ABILITY_NEUTRALIZING_GAS;
    for (u32 i = 0; i < gBattlersCount; ++i) { gBattleMons[i].item = ITEM_NONE; gBattleMons[i].volatiles.neutralizingGas = FALSE; }
    gBattleWeather = B_WEATHER_RAIN_NORMAL; gBattleStruct->weatherDuration = 5;
    ModsBattleBegin(); sGasWeatherCount = sSuppressionCount = 0;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(ActivateGas(1) && sGasWeatherCount == 1 && sGasWeatherEvents[0].cause == RHH_GAS_ACTIVATED
        && sGasWeatherEvents[0].source_battler == 1 && sGasWeatherEvents[0].target_mask == 2
        && sGasWeatherEvents[0].before.weather_suppressed && !sGasWeatherEvents[0].after.weather_suppressed);
    CHECK(ActivateGas(3) && sGasWeatherCount == 1);
    gCurrentMove = MOVE_GASTRO_ACID; gBattlerTarget = 1; RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sGasWeatherCount == 1 && !sSuppressionCount);
    gBattlerTarget = 3; RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sGasWeatherCount == 2 && sGasWeatherEvents[1].cause == RHH_GAS_SUPPRESSED
        && sGasWeatherEvents[1].source_battler == 3 && sGasWeatherEvents[1].target_mask == 8
        && sGasWeatherEvents[1].move == RhhContentKeyForId(RHH_MOVE, MOVE_GASTRO_ACID)
        && !sGasWeatherEvents[1].before.weather_suppressed && sGasWeatherEvents[1].after.weather_suppressed
        && sSuppressionCount == 1 && sSuppressionEvents[0].sequence == 0
        && sSuppressionEvents[0].cause == RHH_ABILITY_MUTATION_SUPPRESS);
    gBattleMons[1].volatiles.gastroAcid = FALSE; gBattleMons[1].volatiles.neutralizingGas = FALSE;
    gBattlerTarget = 1; gCurrentMove = MOVE_TACKLE;
    CHECK(ActivateGas(1) && sGasWeatherCount == 3); GasSwitchOut();
    CHECK(sGasWeatherCount == 4 && sGasWeatherEvents[3].cause == RHH_GAS_SWITCHED_OUT && sGasWeatherEvents[3].after.weather_suppressed);
    CHECK(ActivateGas(1) && sGasWeatherCount == 5); gBattleMons[1].hp = 0; GasFaintCommand();
    CHECK(sGasWeatherCount == 6 && sGasWeatherEvents[5].cause == RHH_GAS_FAINTED && sGasWeatherEvents[5].after.weather_suppressed);
    gBattleMons[1].hp = gBattleMons[1].maxHP; CHECK(ActivateGas(1) && sGasWeatherCount == 7);
    gSpecialStatuses[1].neutralizingGasRemoved = TRUE; GasReplace();
    CHECK(sGasWeatherCount == 8 && sGasWeatherEvents[7].cause == RHH_GAS_REPLACED && sGasWeatherEvents[7].after.weather_suppressed);
    gBattleWeather = B_WEATHER_NONE; CHECK(ActivateGas(1) && sGasWeatherCount == 8); GasSwitchOut(); CHECK(sGasWeatherCount == 8);
    gBattleWeather = B_WEATHER_RAIN_NORMAL; gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(ActivateGas(1) && sGasWeatherCount == 8); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    gBattleMons[1].volatiles.neutralizingGas = FALSE; ai.aiCalcInProgress = TRUE;
    CHECK(ActivateGas(1) && sGasWeatherCount == 8); ai.aiCalcInProgress = FALSE;
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && sGasWeatherBattle->weather_gas_suppression_change(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gBattleHistory = NULL; gAiPartyData = NULL; gAiLogicData = NULL;
    RhhModsUnloadLibraries();
}
static void CheckGasChanges(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sGasMod) && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sGasAbilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 30));
    CHECK(sGasAbilities && sGasAbilities->header.struct_size >= RHH_ABILITY_SERVICE_V1_30_SIZE
        && RHH_ABILITY_SERVICE_V1_29_SIZE < RHH_ABILITY_SERVICE_V1_30_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 41)));
    struct RhhGasChange outside = {.struct_size = sizeof(outside), .sequence = 99}, saved = outside;
    CHECK(sGasAbilities->gas_change_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct BattleHistory history = {0}; struct AiPartyData parties = {0}; struct AiLogicData logic = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources; gBattleHistory = &history; gAiPartyData = &parties; gAiLogicData = &logic;
    PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); gFieldStatuses = 0;
    gBattlerAttacker = 0; gBattlerTarget = 1; gCurrentMove = MOVE_TACKLE;
    gBattleMons[0].ability = gBattleMons[2].ability = ABILITY_BLAZE;
    gBattleMons[1].ability = gBattleMons[3].ability = ABILITY_NEUTRALIZING_GAS;
    for (u32 i = 0; i < gBattlersCount; ++i) { gBattleMons[i].item = ITEM_NONE; gBattleMons[i].volatiles.neutralizingGas = FALSE; }
    gBattleMons[2].item = ITEM_ABILITY_SHIELD; ModsBattleBegin(); sGasCount = 0;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    RhhContentKey blaze = RhhContentKeyForId(RHH_ABILITY, ABILITY_BLAZE);
    CHECK(ActivateGas(1) && sGasCount == 1 && sGasEvents[0].cause == RHH_GAS_ACTIVATED
        && sGasEvents[0].source_battler == 1 && !sGasEvents[0].before.source_mask && sGasEvents[0].after.source_mask == 2);
    const struct RhhGasSlot *before = &sGasEvents[0].before.slots[0], *after = &sGasEvents[0].after.slots[0];
    CHECK(before->flags == (RHH_GAS_PARTICIPANT_AVAILABLE | RHH_GAS_ABILITY_AVAILABLE | RHH_GAS_INNATES_AVAILABLE)
        && after->flags == before->flags && before->ability.active == blaze && !after->ability.active
        && before->innates.count == 1 && !before->innates.entries[0].status
        && after->innates.count == 1 && (after->innates.entries[0].status & RHH_INNATE_NEUTRALIZING_GAS));
    CHECK(sGasEvents[0].before.slots[2].ability.active == blaze && sGasEvents[0].after.slots[2].ability.active == blaze
        && !(sGasEvents[0].after.slots[2].innates.entries[0].status & RHH_INNATE_NEUTRALIZING_GAS));
    CHECK(!ActivateGas(1) && sGasCount == 1);
    gBattleMons[1].item = ITEM_ABILITY_SHIELD; gCurrentMove = MOVE_GASTRO_ACID; gBattlerTarget = 1;
    RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sGasCount == 1 && !gBattleMons[1].volatiles.gastroAcid && gBattleMons[1].volatiles.neutralizingGas);
    gBattleMons[1].item = ITEM_NONE; gCurrentMove = MOVE_TACKLE;
    CHECK(ActivateGas(3) && sGasCount == 2 && sGasEvents[1].before.source_mask == 2 && sGasEvents[1].after.source_mask == 10
        && !sGasEvents[1].before.slots[0].ability.active && !sGasEvents[1].after.slots[0].ability.active);
    gCurrentMove = MOVE_GASTRO_ACID; gBattlerTarget = 1; RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sGasCount == 3 && sGasEvents[2].cause == RHH_GAS_SUPPRESSED && sGasEvents[2].before.source_mask == 10
        && sGasEvents[2].after.source_mask == 8 && sGasEvents[2].move == RhhContentKeyForId(RHH_MOVE, MOVE_GASTRO_ACID)
        && !sGasEvents[2].after.slots[0].ability.active);
    gBattlerTarget = 3; RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sGasCount == 4 && sGasEvents[3].before.source_mask == 8 && !sGasEvents[3].after.source_mask
        && sGasEvents[3].after.slots[0].ability.active == blaze
        && !(sGasEvents[3].after.slots[0].innates.entries[0].status & RHH_INNATE_NEUTRALIZING_GAS));
    gBattlerTarget = 1; GasReplace(); CHECK(sGasCount == 4); /* Suppressed source was already absent. */
    gBattleMons[1].volatiles.gastroAcid = FALSE; gBattleMons[3].volatiles.neutralizingGas = FALSE;
    gCurrentMove = MOVE_TACKLE; CHECK(ActivateGas(1) && sGasCount == 5); GasSwitchOut();
    CHECK(sGasCount == 6 && sGasEvents[5].cause == RHH_GAS_SWITCHED_OUT && sGasEvents[5].before.source_mask == 2 && !sGasEvents[5].after.source_mask);
    CHECK(ActivateGas(1) && sGasCount == 7); gBattleMons[1].hp = 0; GasFaintCommand();
    CHECK(sGasCount == 8 && sGasEvents[7].cause == RHH_GAS_FAINTED && sGasEvents[7].before.source_mask == 2 && !sGasEvents[7].after.source_mask
        && !(sGasEvents[7].before.slots[1].flags & RHH_GAS_ABILITY_AVAILABLE));
    gBattleMons[1].hp = gBattleMons[1].maxHP; CHECK(ActivateGas(1) && sGasCount == 9);
    gBattleMons[1].hp = 0; GasFaintMoveEnd(); CHECK(sGasCount == 10 && sGasEvents[9].cause == RHH_GAS_FAINTED
        && sGasEvents[9].before.source_mask == 2 && !sGasEvents[9].after.source_mask);
    gBattleMons[1].hp = gBattleMons[1].maxHP; CHECK(ActivateGas(1) && sGasCount == 11);
    gSpecialStatuses[1].neutralizingGasRemoved = TRUE; GasReplace();
    CHECK(sGasCount == 12 && sGasEvents[11].cause == RHH_GAS_REPLACED && sGasEvents[11].before.source_mask == 2 && !sGasEvents[11].after.source_mask);
    gBattleMons[1].volatiles.neutralizingGas = FALSE; gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(ActivateGas(1) && sGasCount == 12); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    gBattleMons[1].volatiles.neutralizingGas = FALSE; gBattleTypeFlags |= BATTLE_TYPE_RECORDED;
    CHECK(ActivateGas(1) && sGasCount == 12); gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED;
    gBattleMons[1].volatiles.neutralizingGas = FALSE; ModsBattleSwitchPrepared(1);
    CHECK(ActivateGas(1) && sGasCount == 13 && sGasEvents[12].source_battler == 1
        && sGasEvents[12].after.source_mask == 2 && !sGasEvents[12].after.slots[1].flags);
    ModsBattleSwitchEntered(1);
    for (u32 i = 0; i < sGasCount; ++i) CHECK(sGasEvents[i].sequence == i && sGasEvents[i].run_id == 456);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && sGasAbilities->gas_change_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    ModsBattleReset(); gBattleMons[1].volatiles.neutralizingGas = FALSE; ModsBattleBegin();
    CHECK(ActivateGas(1) && sGasCount == 14 && !sGasEvents[13].sequence);
    gBattleMons[1].hp = 0; FaintClearSetData(1);
    CHECK(sGasCount == 15 && sGasEvents[14].cause == RHH_GAS_FAINTED
        && sGasEvents[14].before.source_mask == 2 && !sGasEvents[14].after.source_mask);
    FaintClearSetData(1); CHECK(sGasCount == 15); /* Cleanup after a reported removal is a no-op. */
    gBattleMons[1].hp = gBattleMons[1].maxHP; CHECK(ActivateGas(1) && sGasCount == 16);
    struct Volatiles previous = gBattleMons[1].volatiles;
    SwitchInClearSetData(1, &previous);
    CHECK(sGasCount == 17 && sGasEvents[16].cause == RHH_GAS_SWITCHED_OUT
        && sGasEvents[16].before.source_mask == 2 && !sGasEvents[16].after.source_mask);
    SwitchInClearSetData(1, &previous); CHECK(sGasCount == 17);
    gBattleMons[1].volatiles.neutralizingGas = TRUE; gBattleMons[1].volatiles.gastroAcid = TRUE;
    FaintClearSetData(1); CHECK(sGasCount == 17); /* A suppressed source was already absent. */
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gBattleHistory = NULL; gAiPartyData = NULL; gAiLogicData = NULL;
    RhhModsUnloadLibraries(); CheckGasWeather(); puts("ability gas: source transitions and affected views passed");
}
