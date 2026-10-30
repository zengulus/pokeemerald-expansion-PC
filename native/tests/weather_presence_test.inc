static const struct RhhBattleService *sPresenceBattle;
static struct RhhBattleWeatherSuppressionChange sPresenceEvents[8];
static u32 sPresenceCount;
static void ObserveWeatherPresence(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleWeatherSuppressionChange change = {.struct_size = sizeof(change)};
    struct RhhBattleWeatherSuppressionChange tiny = {.struct_size = 1, .cause = 99}, saved = tiny;
    struct RhhBattleEnvironmentState current = {.struct_size = sizeof(current)};
    CHECK(sPresenceCount < ARRAY_COUNT(sPresenceEvents) && !sPresenceBattle->weather_presence_suppression_change(&change)
        && sPresenceBattle->weather_presence_suppression_change(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny))
        && !sPresenceBattle->environment(&current) && current.weather_suppressed == change.after.weather_suppressed
        && change.sequence == sPresenceCount && event->ordinal == change.sequence && event->subject == change.after.battle_id
        && event->args[0] == change.cause && event->args[1] == 1 && change.source_battler == 1
        && event->args[2] == change.before.weather_suppressed && event->args[3] == change.after.weather_suppressed
        && change.target_mask == 2 && !event->event_key && !change.move && !change.triggering_ability
        && !RhhModsCommandsAllowed());
    sPresenceEvents[sPresenceCount++] = change; (void)Random(); (void)Random2();
}
static const struct RhhModHook sPresenceHook = {sizeof(struct RhhModHook),
    RHH_BATTLE_WEATHER_PRESENCE_SUPPRESSION_CHANGED, RHH_OBSERVE, 0, NULL, ObserveWeatherPresence};
static const struct RhhModDefinition sPresenceMod = {.struct_size = sizeof(sPresenceMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:weather_presence", .hooks = &sPresenceHook, .hook_count = 1};
static void RunPresenceCommand(u8 opcode)
{
    const u8 command[] = {opcode, BS_TARGET};
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = command; gBattleScriptingCommandsTable[opcode]();
}
static void CheckWeatherPresence(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sPresenceMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sPresenceBattle = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 39));
    CHECK(sPresenceBattle && sPresenceBattle->header.struct_size >= RHH_BATTLE_SERVICE_V1_39_SIZE
        && RHH_BATTLE_SERVICE_V1_38_SIZE < RHH_BATTLE_SERVICE_V1_39_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 46)));
    struct RhhBattleWeatherSuppressionChange outside = {.struct_size = sizeof(outside), .cause = 99}, saved = outside;
    CHECK(sPresenceBattle->weather_presence_suppression_change(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct BattleHistory history = {0}; struct AiPartyData parties = {0}; struct AiLogicData ai = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources; gBattleHistory = &history;
    gAiPartyData = &parties; gAiLogicData = &ai; PrepareTurnBattle();
    gBattlersCount = 2; gBattleTypeFlags &= ~BATTLE_TYPE_DOUBLE; memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses));
    gBattlerAttacker = 0; gBattlerTarget = 1; gBattleMons[0].ability = ABILITY_BLAZE;
    gBattleMons[1].ability = ABILITY_CLOUD_NINE; gBattleWeather = B_WEATHER_RAIN_NORMAL;
    gBattleStruct->weatherDuration = 5; gCurrentMove = MOVE_TACKLE;
    ModsBattleBegin(); sPresenceCount = 0; rng_value_t rng = gRngValue, rng2 = gRng2Value;
    RunPresenceCommand(B_SCR_OP_SWITCHOUTABILITIES);
    CHECK(sPresenceCount == 1 && sPresenceEvents[0].cause == RHH_WEATHER_PRESENCE_SWITCHED_OUT
        && sPresenceEvents[0].before.weather_suppressed && !sPresenceEvents[0].after.weather_suppressed);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_RAYQUAZA, 20, 1000, OTID_STRUCT_PRESET(54321), 31);
    struct BattlePokemon incoming; PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &incoming);
    memcpy(&resources.bufferB[1][4], &incoming, BATTLE_POKEMON_TRANSFER_SIZE); gBattlerPartyIndexes[1] = 0;
    rng = gRngValue; rng2 = gRng2Value;
    RunPresenceCommand(B_SCR_OP_SWITCHINDATAUPDATE);
    CHECK(sPresenceCount == 2 && sPresenceEvents[1].cause == RHH_WEATHER_PRESENCE_ENTERED
        && !sPresenceEvents[1].before.weather_suppressed && sPresenceEvents[1].after.weather_suppressed);
    RunPresenceCommand(B_SCR_OP_DOFAINTANIMATION);
    CHECK(sPresenceCount == 3 && sPresenceEvents[2].cause == RHH_WEATHER_PRESENCE_FAINTED
        && sPresenceEvents[2].before.weather_suppressed && !sPresenceEvents[2].after.weather_suppressed);
    RunPresenceCommand(B_SCR_OP_DOFAINTANIMATION); CHECK(sPresenceCount == 3);
    gBattleStruct->battlerState[1].notOnField = FALSE; gBattleMons[0].ability = ABILITY_CLOUD_NINE;
    RunPresenceCommand(B_SCR_OP_SWITCHOUTABILITIES); CHECK(sPresenceCount == 3);
    gBattleStruct->battlerState[1].notOnField = FALSE; gBattleMons[0].ability = ABILITY_BLAZE;
    ai.aiCalcInProgress = TRUE; RunPresenceCommand(B_SCR_OP_SWITCHOUTABILITIES); CHECK(sPresenceCount == 3);
    ai.aiCalcInProgress = FALSE; gBattleStruct->battlerState[1].notOnField = FALSE; gBattleWeather = 0;
    RunPresenceCommand(B_SCR_OP_SWITCHOUTABILITIES); CHECK(sPresenceCount == 3);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && sPresenceBattle->weather_presence_suppression_change(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gBattleHistory = NULL; gAiPartyData = NULL; gAiLogicData = NULL;
    puts("weather presence: switch, entry and faint suppression changes passed");
}
