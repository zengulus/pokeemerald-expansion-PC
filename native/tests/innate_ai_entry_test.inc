#include "battle_ai_switch.h"
#include "constants/battle_ai.h"

static bool32 sAiFieldImpure;
static void ImpureAiField(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (sAiFieldImpure) { (void)Random(); (void)Random2(); }
}
static const struct RhhModHook sAiFieldImpureHook = {sizeof(struct RhhModHook), RHH_AI_INNATE_ENTRY_FIELD_PREVIEW,
    RHH_CONSTRAINT, 100, ImpureAiField, NULL};
static const struct RhhModDefinition sAiFieldImpureMod = {.struct_size = sizeof(sAiFieldImpureMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:ai_field_impure", .hooks = &sAiFieldImpureHook, .hook_count = 1};
static void CheckInnateAiEntry(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sAiFieldImpureMod) && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct AiLogicData ai = {0}; struct AiPartyData parties = {0}; struct AiThinkingStruct thinking = {0};
    struct AiBattleData decisions = {0}; struct BattleHistory history = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources;
    gAiLogicData = &ai; gAiPartyData = &parties; gAiThinkingStruct = &thinking;
    gAiBattleData = &decisions; gBattleHistory = &history;
    PrepareTurnBattle(); gBattleTypeFlags = BATTLE_TYPE_TRAINER; gBattlersCount = 2;
    struct Pokemon *candidate = &gParties[B_TRAINER_OPPONENT_A][1];
    CreateMonWithIVs(candidate, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(888), 31);
    SetMonMoveSlot(candidate, MOVE_SCRATCH, 0);
    u16 hp = 1; SetMonData(candidate, MON_DATA_HP, &hp);
    gPartiesCount[B_TRAINER_OPPONENT_A] = 2;
    const struct RhhAbilityService *abilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 19));
    CHECK(abilities && abilities->header.struct_size >= RHH_ABILITY_SERVICE_V1_19_SIZE);
    struct RhhAiInnateEntryContext outside = {.struct_size = sizeof(outside), .hp = 999}, savedOutside = outside;
    CHECK(abilities->ai_innate_entry_context(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside))
        && abilities->offer_ai_innate_entry_heal(1, 2));
    ModsBattleBegin();
    PokemonToBattleMon(candidate, &gBattleMons[1]); ai.abilities[1] = ABILITY_NONE;
    u8 *enabled = RhhModsState("test:ai_entry"); CHECK(enabled);
    struct Pokemon savedMon = *candidate; struct AiLogicData savedAi = ai;
    rng_value_t rng = gRngValue, rng2 = gRng2Value; u32 oldSlot = gBattlerPartyIndexes[1];
    const struct RhhAbilityService *fieldAbilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 36));
    struct RhhAiInnateFieldContext outsideField = {.struct_size = sizeof(outsideField), .weather = 999}, savedField = outsideField;
    CHECK(fieldAbilities && RHH_ABILITY_SERVICE_V1_34_SIZE < RHH_ABILITY_SERVICE_V1_35_SIZE
        && RHH_ABILITY_SERVICE_V1_35_SIZE < RHH_ABILITY_SERVICE_V1_36_SIZE
        && fieldAbilities->ai_innate_entry_field_context(&outsideField) && !memcmp(&outsideField, &savedField, sizeof(outsideField))
        && fieldAbilities->ai_innate_entry_field_projected(&outsideField) && !memcmp(&outsideField, &savedField, sizeof(outsideField))
        && fieldAbilities->offer_ai_innate_entry_weather(RHH_WEATHER_RAIN)
        && fieldAbilities->offer_ai_innate_entry_terrain(RHH_TERRAIN_ELECTRIC));
    u16 savedFieldWeather = gBattleWeather; struct FieldTimer savedFieldTimers = gFieldTimers;
    gBattleWeather = B_WEATHER_SUN_NORMAL; gBattleStruct->weatherDuration = 3; gFieldTimers.terrain = B_TERRAIN_NONE;
    u32 fieldWeather = 999, fieldTerrain = 999, fieldTurns = 999, terrainTurns = 999;
    *enabled = 36;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == B_WEATHER_RAIN_NORMAL && fieldTerrain == B_TERRAIN_GRASSY
        && fieldTurns == 5 && terrainTurns == 5 && gBattleWeather == B_WEATHER_SUN_NORMAL
        && gFieldTimers.terrain == B_TERRAIN_NONE);
    *enabled = 37;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == B_WEATHER_SUN_NORMAL && fieldTerrain == B_TERRAIN_GRASSY
        && fieldTurns == 3 && terrainTurns == 5);
    *enabled = 39; ai.abilities[1] = ABILITY_LEVITATE;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldTerrain == B_TERRAIN_ELECTRIC && gBattlerPartyIndexes[1] == oldSlot);
    *enabled = 40; gFieldStatuses |= STATUS_FIELD_GRAVITY;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldTerrain == B_TERRAIN_ELECTRIC && gBattlerPartyIndexes[1] == oldSlot);
    gFieldStatuses &= ~STATUS_FIELD_GRAVITY; ai.abilities[1] = ABILITY_NONE; *enabled = 41;
    gBattleMons[1].volatiles.semiInvulnerable = STATE_UNDERGROUND;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldTerrain == B_TERRAIN_ELECTRIC && gBattlerPartyIndexes[1] == oldSlot);
    gBattleMons[1].volatiles.semiInvulnerable = STATE_NONE; *enabled = 42;
    u8 originalTypes[3]; memcpy(originalTypes, gBattleMons[1].types, sizeof(originalTypes));
    gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_FLYING; gAbsentBattlerFlags |= 1u << 1;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldTerrain == B_TERRAIN_ELECTRIC && gBattlerPartyIndexes[1] == oldSlot);
    gAbsentBattlerFlags &= ~(1u << 1); memcpy(gBattleMons[1].types, originalTypes, sizeof(originalTypes));
    u8 flyingTera = TYPE_FLYING;
    SetMonData(candidate, MON_DATA_TERA_TYPE, &flyingTera); gBattlerPartyIndexes[1] = 1;
    SetActiveGimmick(1, GIMMICK_TERA); gBattlerPartyIndexes[1] = oldSlot; *enabled = 43;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldTerrain == B_TERRAIN_ELECTRIC && gBattlerPartyIndexes[1] == oldSlot);
    gBattlerPartyIndexes[1] = 1; SetActiveGimmick(1, GIMMICK_NONE); gBattlerPartyIndexes[1] = oldSlot;
    *candidate = savedMon;
    *enabled = 38;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == B_WEATHER_SUN_NORMAL && fieldTerrain == B_TERRAIN_NONE
        && fieldTurns == 5 && terrainTurns == gFieldTimers.terrainTimer);
    *enabled = 28;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == B_WEATHER_RAIN_NORMAL && fieldTerrain == B_TERRAIN_ELECTRIC && fieldTurns == 5 && terrainTurns == 5
        && gBattleWeather == B_WEATHER_SUN_NORMAL && gBattleStruct->weatherDuration == 3
        && gFieldTimers.terrain == B_TERRAIN_NONE && !memcmp(candidate, &savedMon, sizeof(savedMon))
        && !memcmp(&ai, &savedAi, sizeof(ai)) && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    ai.holdEffects[1] = HOLD_EFFECT_DAMP_ROCK;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldTurns == 8 && terrainTurns == 5);
    ai.holdEffects[1] = HOLD_EFFECT_TERRAIN_EXTENDER;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldTurns == 5 && terrainTurns == 8);
    ai.holdEffects[1] = HOLD_EFFECT_NONE;
    sAiFieldImpure = TRUE; fieldWeather = fieldTerrain = fieldTurns = terrainTurns = 999;
    CHECK(!ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == 999 && fieldTerrain == 999 && fieldTurns == 999 && terrainTurns == 999
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sAiFieldImpure = FALSE;
    fieldWeather = fieldTerrain = fieldTurns = terrainTurns = 999; *enabled = 29;
    CHECK(!ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == 999 && fieldTerrain == 999 && fieldTurns == 999 && terrainTurns == 999);
    gBattleWeather = 0; *enabled = 30;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == B_WEATHER_SUN_NORMAL && fieldTerrain == B_TERRAIN_NONE);
    gBattleWeather = B_WEATHER_SUN_NORMAL; gBattleStruct->overworldWeatherPresent = TRUE; *enabled = 31;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == B_WEATHER_SUN_NORMAL && fieldTerrain == B_TERRAIN_ELECTRIC);
    gBattleStruct->overworldWeatherPresent = FALSE; gBattleStruct->isSkyBattle = TRUE; *enabled = 32;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == B_WEATHER_RAIN_NORMAL && fieldTerrain == B_TERRAIN_NONE);
    gBattleStruct->isSkyBattle = FALSE; ai.abilities[0] = ABILITY_CLOUD_NINE; *enabled = 33;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == B_WEATHER_RAIN_NORMAL && fieldTerrain == B_TERRAIN_ELECTRIC);
    u32 actualSpeed = GetBattlerTotalSpeedStat(1, ABILITY_NONE, HOLD_EFFECT_NONE);
    ModsAiSwitchFieldBegin(1, fieldWeather, fieldTerrain, fieldTurns);
    CHECK(!AI_GetWeather() && !AI_GetSwitchinWeather(1) && AI_GetSwitchinTerrain(1) == B_TERRAIN_ELECTRIC
        && GetAiBattlerTotalSpeedStat(1, ABILITY_SWIFT_SWIM, HOLD_EFFECT_NONE) == actualSpeed
        && GetAiBattlerTotalSpeedStat(1, ABILITY_SURGE_SURFER, HOLD_EFFECT_NONE) == actualSpeed * 2
        && GetBattlerTotalSpeedStat(1, ABILITY_SURGE_SURFER, HOLD_EFFECT_NONE) == actualSpeed);
    ai.abilities[0] = ABILITY_NONE;
    CHECK(AI_GetWeather() == B_WEATHER_RAIN_NORMAL && AI_GetSwitchinWeather(0) == B_WEATHER_RAIN_NORMAL
        && AI_GetSwitchinTerrain(0) == B_TERRAIN_ELECTRIC
        && GetAiBattlerTotalSpeedStat(1, ABILITY_SWIFT_SWIM, HOLD_EFFECT_NONE) == actualSpeed * 2);
    ModsAiSwitchFieldEnd();
    CHECK(!ModsAiSwitchFieldView(NULL, NULL, NULL) && gBattleWeather == B_WEATHER_SUN_NORMAL && gFieldTimers.terrain == B_TERRAIN_NONE);
    ai.abilities[1] = ABILITY_DROUGHT; gBattleWeather = 0; *enabled = 34;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == B_WEATHER_RAIN_NORMAL && fieldTerrain == B_TERRAIN_ELECTRIC);
    ai.abilities[1] = ABILITY_NONE; gBattleWeather = B_WEATHER_RAIN_PRIMAL; *enabled = 31;
    CHECK(ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns)
        && fieldWeather == B_WEATHER_RAIN_PRIMAL && fieldTerrain == B_TERRAIN_ELECTRIC);
    gBattleWeather = B_WEATHER_SUN_NORMAL; *enabled = 28; gBattleMons[1].volatiles.gastroAcid = TRUE;
    CHECK(!ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns));
    gBattleMons[1].volatiles.gastroAcid = FALSE; ai.abilities[1] = ABILITY_NEUTRALIZING_GAS;
    CHECK(!ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns));
    ai.abilities[1] = ABILITY_NONE; ai.aiPredictionInProgress = TRUE;
    CHECK(!ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns));
    ai.aiPredictionInProgress = FALSE; gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(!ModsAiInnateEntryFieldForecast(1, 1, 1, &fieldWeather, &fieldTerrain, &fieldTurns, &terrainTurns));
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; gBattleWeather = savedFieldWeather; gFieldTimers = savedFieldTimers; gBattleStruct->weatherDuration = 0;
    u8 stages[NUM_BATTLE_STATS];
    memset(stages, 0xff, sizeof(stages));
    *enabled = 6;
    CHECK(ModsAiInnateEntryStats(1, 1, gBattleMons[1].maxHP, stages));
    CHECK(stages[STAT_ATK] == DEFAULT_STAT_STAGE + 2 && gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE
        && !memcmp(candidate, &savedMon, sizeof(savedMon)) && !memcmp(&ai, &savedAi, sizeof(ai))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    *enabled = 7; CHECK(!ModsAiInnateEntryStats(1, 1, 1, stages));
    *enabled = 6; ai.abilities[1] = ABILITY_CONTRARY;
    CHECK(ModsAiInnateEntryStats(1, 1, 1, stages) && stages[STAT_ATK] == DEFAULT_STAT_STAGE - 2);
    ai.abilities[1] = ABILITY_SIMPLE;
    CHECK(ModsAiInnateEntryStats(1, 1, 1, stages) && stages[STAT_ATK] == DEFAULT_STAT_STAGE + 4);
    ai.abilities[1] = ABILITY_NONE;
    u8 opponentStages[NUM_BATTLE_STATS];
    struct RhhAiInnateOpponentStatContext noTarget = {.struct_size = sizeof(noTarget), .battler = 999}, oldTarget = noTarget;
    CHECK(abilities->ai_innate_entry_opponent_context(&noTarget) && !memcmp(&noTarget, &oldTarget, sizeof(noTarget))
        && abilities->offer_ai_innate_entry_opponent_stat(RHH_STAT_ATTACK, -1));
    *enabled = 9;
    CHECK(ModsAiInnateEntryStatChanges(1, 1, 1, stages, opponentStages));
    CHECK(stages[STAT_ATK] == DEFAULT_STAT_STAGE && opponentStages[STAT_ATK] == DEFAULT_STAT_STAGE - 1
        && gBattleMons[0].statStages[STAT_ATK] == DEFAULT_STAT_STAGE);
    *enabled = 10; CHECK(!ModsAiInnateEntryStatChanges(1, 1, 1, stages, opponentStages)); *enabled = 9;
    ai.abilities[0] = ABILITY_CLEAR_BODY; CHECK(!ModsAiInnateEntryStatChanges(1, 1, 1, stages, opponentStages));
    ai.abilities[0] = ABILITY_CONTRARY; CHECK(ModsAiInnateEntryStatChanges(1, 1, 1, stages, opponentStages)
        && opponentStages[STAT_ATK] == DEFAULT_STAT_STAGE + 1);
    ai.abilities[0] = ABILITY_SIMPLE; CHECK(ModsAiInnateEntryStatChanges(1, 1, 1, stages, opponentStages)
        && opponentStages[STAT_ATK] == DEFAULT_STAT_STAGE - 2);
    ai.abilities[0] = ABILITY_DEFIANT; CHECK(ModsAiInnateEntryStatChanges(1, 1, 1, stages, opponentStages)
        && opponentStages[STAT_ATK] == DEFAULT_STAT_STAGE + 1);
    ai.abilities[0] = ABILITY_MIRROR_ARMOR; CHECK(ModsAiInnateEntryStatChanges(1, 1, 1, stages, opponentStages)
        && stages[STAT_ATK] == DEFAULT_STAT_STAGE - 1 && opponentStages[STAT_ATK] == DEFAULT_STAT_STAGE);
    ai.abilities[0] = ABILITY_NONE; gSideTimers[B_SIDE_PLAYER].mistTimer = 1;
    CHECK(!ModsAiInnateEntryStatChanges(1, 1, 1, stages, opponentStages)); gSideTimers[B_SIDE_PLAYER].mistTimer = 0;
    ai.holdEffects[0] = HOLD_EFFECT_CLEAR_AMULET;
    CHECK(!ModsAiInnateEntryStatChanges(1, 1, 1, stages, opponentStages)); ai.holdEffects[0] = HOLD_EFFECT_NONE;
    gBattleMons[0].volatiles.substitute = TRUE;
    CHECK(ModsAiInnateEntryStatChanges(1, 1, 1, stages, opponentStages) && opponentStages[STAT_ATK] == DEFAULT_STAT_STAGE - 1);
    gBattleMons[0].volatiles.substitute = FALSE;
    *enabled = 12; CHECK(ModsAiInnateEntryStatChanges(1, 1, 1, stages, opponentStages) && opponentStages[STAT_ATK] == MIN_STAT_STAGE);
    const struct RhhAbilityService *newAbilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 27));
    struct RhhAiInnateOpponentStatusContext outsideStatus = {.struct_size = sizeof(outsideStatus), .battler = 999}, oldStatus = outsideStatus;
    CHECK(newAbilities && newAbilities->ai_innate_entry_opponent_status_context(&outsideStatus)
        && !memcmp(&outsideStatus, &oldStatus, sizeof(oldStatus)) && newAbilities->offer_ai_innate_entry_opponent_status(RHH_EFFECT_BURN));
    enum Type savedTypes[3]; memcpy(savedTypes, gBattleMons[0].types, sizeof(savedTypes));
    enum Ability savedAbility = gBattleMons[0].ability;
    gBattleMons[0].types[0] = gBattleMons[0].types[1] = TYPE_WATER; gBattleMons[0].types[2] = TYPE_MYSTERY;
    gBattleMons[0].status1 = 0; gBattleMons[0].ability = ABILITY_WATER_VEIL; ai.abilities[0] = ABILITY_NONE;
    u32 predictedStatus = 999;
    *enabled = 20;
    CHECK(ModsAiInnateEntryForecast(1, 1, 1, stages, opponentStages, &predictedStatus)
        && predictedStatus == STATUS1_BURN && !gBattleMons[0].status1);
    ai.abilities[0] = ABILITY_WATER_VEIL; predictedStatus = 999;
    CHECK(!ModsAiInnateEntryForecast(1, 1, 1, stages, opponentStages, &predictedStatus) && predictedStatus == 999);
    ai.abilities[0] = ABILITY_NONE; *enabled = 21;
    CHECK(!ModsAiInnateEntryForecast(1, 1, 1, stages, opponentStages, &predictedStatus));
    *enabled = 20; gSideStatuses[B_SIDE_PLAYER] |= SIDE_STATUS_SAFEGUARD;
    CHECK(!ModsAiInnateEntryForecast(1, 1, 1, stages, opponentStages, &predictedStatus));
    gSideStatuses[B_SIDE_PLAYER] &= ~SIDE_STATUS_SAFEGUARD; gFieldTimers.terrain = B_TERRAIN_MISTY;
    CHECK(!ModsAiInnateEntryForecast(1, 1, 1, stages, opponentStages, &predictedStatus));
    gFieldTimers.terrain = B_TERRAIN_NONE; gBattleMons[0].types[0] = TYPE_FIRE;
    CHECK(!ModsAiInnateEntryForecast(1, 1, 1, stages, opponentStages, &predictedStatus));
    gBattleMons[0].types[0] = TYPE_WATER; gBattleMons[0].status1 = STATUS1_POISON;
    CHECK(!ModsAiInnateEntryForecast(1, 1, 1, stages, opponentStages, &predictedStatus));
    gBattleMons[0].status1 = 0; *enabled = 22;
    CHECK(ModsAiInnateEntryForecast(1, 1, 1, stages, opponentStages, &predictedStatus) && predictedStatus == STATUS1_PARALYSIS);
    *enabled = 23;
    CHECK(ModsAiInnateEntryForecast(1, 1, 1, stages, opponentStages, &predictedStatus)
        && predictedStatus == STATUS1_BURN && opponentStages[STAT_ATK] == DEFAULT_STAT_STAGE - 1);
    enum Type ownTypes[3]; memcpy(ownTypes, gBattleMons[1].types, sizeof(ownTypes));
    gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_WATER; gBattleMons[1].types[2] = TYPE_MYSTERY;
    gBattleMons[1].status1 = 0; ai.abilities[1] = ABILITY_NONE;
    u32 ownStatus = 999; ai.abilities[0] = ABILITY_SYNCHRONIZE; *enabled = 20;
    CHECK(ModsAiInnateEntryForecastStatuses(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus)
        && ownStatus == STATUS1_BURN && predictedStatus == STATUS1_BURN
        && !gBattleMons[0].status1 && !gBattleMons[1].status1);
    ai.abilities[1] = ABILITY_WATER_VEIL;
    CHECK(ModsAiInnateEntryForecastStatuses(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus)
        && ownStatus == 0 && predictedStatus == STATUS1_BURN);
    ai.abilities[1] = ABILITY_NONE; gBattleMons[1].types[0] = TYPE_FIRE;
    CHECK(ModsAiInnateEntryForecastStatuses(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus)
        && ownStatus == 0 && predictedStatus == STATUS1_BURN);
    gBattleMons[1].types[0] = TYPE_WATER; gSideStatuses[B_SIDE_OPPONENT] |= SIDE_STATUS_SAFEGUARD;
    CHECK(ModsAiInnateEntryForecastStatuses(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus)
        && ownStatus == 0 && predictedStatus == STATUS1_BURN);
    gSideStatuses[B_SIDE_OPPONENT] &= ~SIDE_STATUS_SAFEGUARD;
    *enabled = 22; CHECK(ModsAiInnateEntryForecastStatuses(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus)
        && ownStatus == STATUS1_PARALYSIS && predictedStatus == STATUS1_PARALYSIS);
    *enabled = 25; CHECK(ModsAiInnateEntryForecastStatuses(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus)
        && predictedStatus == STATUS1_TOXIC_POISON
        && ownStatus == (GetConfig(B_SYNCHRONIZE_TOXIC) < GEN_5 ? STATUS1_POISON : STATUS1_TOXIC_POISON));
    *enabled = 26; CHECK(ModsAiInnateEntryForecastStatuses(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus)
        && !ownStatus && predictedStatus == STATUS1_SLEEP_TURN(2));
    *enabled = 20; ai.abilities[0] = ABILITY_NONE; gBattleMons[0].ability = ABILITY_SYNCHRONIZE;
    CHECK(ModsAiInnateEntryForecastStatuses(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus)
        && !ownStatus && predictedStatus == STATUS1_BURN);
    u32 consumedItems = 999;
    ai.abilities[0] = ABILITY_SYNCHRONIZE; ai.holdEffects[0] = HOLD_EFFECT_CURE_BRN;
    ai.holdEffects[1] = HOLD_EFFECT_CURE_STATUS;
    CHECK(ModsAiInnateEntryForecastDetailed(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus, &consumedItems)
        && !ownStatus && !predictedStatus && consumedItems == ((1u << 0) | (1u << 1)));
    *enabled = 27;
    CHECK(ModsAiInnateEntryForecastDetailed(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus, &consumedItems)
        && ownStatus == STATUS1_BURN && predictedStatus == STATUS1_BURN && consumedItems == ((1u << 0) | (1u << 1)));
    *enabled = 20; gFieldStatuses |= STATUS_FIELD_MAGIC_ROOM;
    CHECK(ModsAiInnateEntryForecastDetailed(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus, &consumedItems)
        && ownStatus == STATUS1_BURN && predictedStatus == STATUS1_BURN && !consumedItems);
    gFieldStatuses &= ~STATUS_FIELD_MAGIC_ROOM; gBattleMons[0].volatiles.embargoTimer = 1;
    CHECK(ModsAiInnateEntryForecastDetailed(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus, &consumedItems)
        && !ownStatus && predictedStatus == STATUS1_BURN && consumedItems == (1u << 1));
    gBattleMons[0].volatiles.embargoTimer = 0; ai.abilities[1] = ABILITY_UNNERVE;
    CHECK(ModsAiInnateEntryForecastDetailed(1, 1, 1, stages, opponentStages, &ownStatus, &predictedStatus, &consumedItems)
        && !ownStatus && predictedStatus == STATUS1_BURN && consumedItems == (1u << 1));
    ai.abilities[0] = ai.abilities[1] = ABILITY_NONE; ai.holdEffects[0] = ai.holdEffects[1] = HOLD_EFFECT_NONE;
    memcpy(gBattleMons[1].types, ownTypes, sizeof(ownTypes));
    memcpy(gBattleMons[0].types, savedTypes, sizeof(savedTypes)); gBattleMons[0].ability = savedAbility;
    CHECK(!memcmp(candidate, &savedMon, sizeof(savedMon)) && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    *enabled = 6;
    gBattleMons[1].volatiles.gastroAcid = TRUE; CHECK(!ModsAiInnateEntryStats(1, 1, 1, stages));
    gBattleMons[1].volatiles.gastroAcid = FALSE;
    ai.aiPredictionInProgress = TRUE; CHECK(!ModsAiInnateEntryStats(1, 1, 1, stages)); ai.aiPredictionInProgress = FALSE;
    gBattleTypeFlags |= BATTLE_TYPE_LINK; CHECK(!ModsAiInnateEntryStats(1, 1, 1, stages)); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    *enabled = 0;
    CHECK(!ModsAiInnateEntryHealing(1, 1, 1));
    *enabled = 1;
    u32 amount = ModsAiInnateEntryHealing(1, 1, 1);
    CHECK(amount == gBattleMons[1].maxHP / 2 && gBattlerPartyIndexes[1] == oldSlot
        && !memcmp(candidate, &savedMon, sizeof(savedMon)) && !memcmp(&ai, &savedAi, sizeof(ai))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    CHECK(ModsAiInnateEntryHealing(1, 1, gBattleMons[1].maxHP - 1) == 1);
    *enabled = 2; CHECK(!ModsAiInnateEntryHealing(1, 1, 1)); *enabled = 1;
    *enabled = 3; CHECK(ModsAiInnateEntryHealing(1, 1, 1) == gBattleMons[1].maxHP - 1); *enabled = 1;
    gBattleMons[1].volatiles.gastroAcid = TRUE; CHECK(!ModsAiInnateEntryHealing(1, 1, 1));
    gBattleMons[1].volatiles.gastroAcid = FALSE; gBattleMons[1].volatiles.healBlockTimer = 1;
    CHECK(!ModsAiInnateEntryHealing(1, 1, 1)); gBattleMons[1].volatiles.healBlockTimer = 0;
    ai.abilities[1] = ABILITY_NEUTRALIZING_GAS; CHECK(!ModsAiInnateEntryHealing(1, 1, 1)); ai.abilities[1] = ABILITY_NONE;
    u8 egg = TRUE; SetMonData(candidate, MON_DATA_IS_EGG, &egg);
    CHECK(!ModsAiInnateEntryHealing(1, 1, 1)); *candidate = savedMon;
    candidate->box.secure.raw[0] ^= 1; struct Pokemon corrupt = *candidate;
    CHECK(!ModsAiInnateEntryHealing(1, 1, 1) && !memcmp(candidate, &corrupt, sizeof(corrupt)));
    *candidate = savedMon;
    ai.aiPredictionInProgress = TRUE; CHECK(!ModsAiInnateEntryHealing(1, 1, 1)); ai.aiPredictionInProgress = FALSE;
    gBattleTypeFlags |= BATTLE_TYPE_LINK; CHECK(!ModsAiInnateEntryHealing(1, 1, 1)); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    CHECK(!ModsAiInnateEntryHealing(1, 1, 0) && !ModsAiInnateEntryHealing(1, 1, gBattleMons[1].maxHP));
    CHECK(abilities->ai_innate_entry_context(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside)));
    CHECK(abilities->ai_innate_turn_heal_context(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside))
        && abilities->offer_ai_innate_turn_heal(1, 4));
    *enabled = 13;
    CHECK(ModsAiInnateTurnHealing(1, 1, gBattleMons[1].maxHP) == max(1, gBattleMons[1].maxHP / 4));
    *enabled = 14; CHECK(!ModsAiInnateTurnHealing(1, 1, 1));
    *enabled = 15; CHECK(ModsAiInnateTurnHealing(1, 1, 1) == gBattleMons[1].maxHP);
    *enabled = 13; gBattleMons[1].volatiles.healBlockTimer = 1;
    CHECK(!ModsAiInnateTurnHealing(1, 1, 1)); gBattleMons[1].volatiles.healBlockTimer = 0;
    CHECK(abilities->ai_innate_turn_step_context(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside))
        && abilities->offer_ai_innate_turn_step_heal(1, 4));
    *enabled = 16;
    CHECK(ModsAiInnateTurnStepHealing(1, 1, 1) == max(1, gBattleMons[1].maxHP / 4));
    CHECK(!ModsAiInnateTurnStepHealing(1, 1, gBattleMons[1].maxHP));
    *enabled = 17; CHECK(!ModsAiInnateTurnStepHealing(1, 1, 1));
    *enabled = 18; CHECK(ModsAiInnateTurnStepHealing(1, 1, 1) == gBattleMons[1].maxHP);
    *enabled = 16; gBattleMons[1].volatiles.healBlockTimer = 1;
    CHECK(!ModsAiInnateTurnStepHealing(1, 1, 1)); gBattleMons[1].volatiles.healBlockTimer = 0;
    CHECK(!ModsAiInnateTurnStepHealing(1, 1, 0));
    CHECK(!memcmp(candidate, &savedMon, sizeof(savedMon)) && !memcmp(&ai, &savedAi, sizeof(ai))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    /* Exercise the real smart-switch chooser, with a single low-HP candidate.
     * Entry healing lets that candidate survive and win its simulated matchup. */
    u16 attack = 250, defense = 50, speed = 1;
    SetMonData(candidate, MON_DATA_ATK, &attack); SetMonData(candidate, MON_DATA_DEF, &defense);
    SetMonData(candidate, MON_DATA_SPEED, &speed); hp = 1; SetMonData(candidate, MON_DATA_HP, &hp);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_TACKLE, 0);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_QUICK_ATTACK, 1);
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
    gBattleMons[0].hp = 15; gBattleMons[0].attack = 50; gBattleMons[0].defense = 50; gBattleMons[0].speed = 200;
    history.usedMoves[0][0] = MOVE_TACKLE;
    history.usedMoves[0][1] = MOVE_QUICK_ATTACK;
    for (u32 i = 0; i < MAX_BATTLERS_COUNT; ++i)
    {
        gBattleStruct->monToSwitchIntoId[i] = PARTY_SIZE;
        gBattleStruct->AI_monToSwitchIntoId[i] = PARTY_SIZE;
        ai.monToSwitchInId[i] = PARTY_SIZE;
    }
    thinking.aiFlags[1] = AI_FLAG_SMART_MON_CHOICES;
    gBattlerBattleController[1] = BATTLE_CONTROLLER_OPPONENT;
    SetBattlerAiData(0, &ai); SetBattlerAiData(1, &ai);
    *enabled = 0;
    u32 baseline = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
    *enabled = 1;
    u32 predicted = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
    if (baseline != PARTY_SIZE || predicted != 1)
        fprintf(stderr, "AI entry switch choice baseline=%u predicted=%u\n", baseline, predicted);
    CHECK(baseline == PARTY_SIZE && predicted == 1);
    gBattleStruct->hazardsQueue[B_SIDE_OPPONENT][0] = HAZARDS_SPIKES;
    gSideTimers[B_SIDE_OPPONENT].spikesAmount = 1;
    CHECK(IsHazardOnSide(B_SIDE_OPPONENT, HAZARDS_SPIKES));
    CHECK(GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL) == PARTY_SIZE);
    u16 item = ITEM_ORAN_BERRY; SetMonData(candidate, MON_DATA_HELD_ITEM, &item);
    CHECK(GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL) == PARTY_SIZE);
    hp = 20; SetMonData(candidate, MON_DATA_HP, &hp); *enabled = 4;
    (void)GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
    hp = 1; SetMonData(candidate, MON_DATA_HP, &hp); *enabled = 5;
    gBattleStruct->battlerState[1].storedHealingWish = TRUE;
    (void)GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
    gBattleStruct->battlerState[1].storedHealingWish = FALSE;
    gBattleStruct->hazardsQueue[B_SIDE_OPPONENT][0] = HAZARDS_NONE;
    gSideTimers[B_SIDE_OPPONENT].spikesAmount = 0;
    hp = 1; SetMonData(candidate, MON_DATA_HP, &hp); *enabled = 0; gBattleMons[0].attack = 20;
    CHECK(GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL) == 1);
    /* Find a matchup at the KO threshold where the +2 Attack preview changes
     * the real switcher's decision, while leaving the candidate untouched. */
    item = ITEM_NONE; SetMonData(candidate, MON_DATA_HELD_ITEM, &item);
    hp = 20; SetMonData(candidate, MON_DATA_HP, &hp);
    speed = 200; SetMonData(candidate, MON_DATA_SPEED, &speed);
    bool32 statChoice = FALSE;
    for (attack = 5; attack <= 120 && !statChoice; attack += 5)
    {
        SetMonData(candidate, MON_DATA_ATK, &attack);
        struct Pokemon before = *candidate;
        *enabled = 8; baseline = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
        *enabled = 6; predicted = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
        CHECK(!memcmp(candidate, &before, sizeof(before)));
        statChoice = baseline == PARTY_SIZE && predicted == 1;
    }
    CHECK(statChoice);
    /* Opponent Attack changes must affect the real switcher's survival
     * estimate and must not escape the temporary candidate simulation. */
    attack = 120; SetMonData(candidate, MON_DATA_ATK, &attack);
    speed = 250; SetMonData(candidate, MON_DATA_SPEED, &speed);
    bool32 opponentChoice = FALSE;
    for (u16 incomingAttack = 20; incomingAttack <= 250 && !opponentChoice; incomingAttack += 5)
    {
        gBattleMons[0].attack = incomingAttack;
        for (u16 candidateHp = 5; candidateHp <= min(60, GetMonData(candidate, MON_DATA_MAX_HP)) && !opponentChoice; candidateHp += 5)
        {
            SetMonData(candidate, MON_DATA_HP, &candidateHp);
            struct Pokemon before = *candidate; struct BattlePokemon opponentBefore = gBattleMons[0];
            *enabled = 8; baseline = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
            *enabled = 9; predicted = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
            CHECK(!memcmp(candidate, &before, sizeof(before)) && !memcmp(&gBattleMons[0], &opponentBefore, sizeof(opponentBefore)));
            opponentChoice = baseline == PARTY_SIZE && predicted == 1;
        }
    }
    CHECK(opponentChoice);
    /* A predicted burn changes the smart switcher's damage estimate without
     * changing either real battler or leaking a hidden defensive ability. */
    gBattleMons[0].types[0] = gBattleMons[0].types[1] = TYPE_WATER; gBattleMons[0].types[2] = TYPE_MYSTERY;
    ai.abilities[0] = ABILITY_NONE; bool32 statusChoice = FALSE;
    for (u16 incomingAttack = 20; incomingAttack <= 250 && !statusChoice; incomingAttack += 5)
    {
        gBattleMons[0].attack = incomingAttack;
        for (u16 candidateHp = 5; candidateHp <= min(60, GetMonData(candidate, MON_DATA_MAX_HP)) && !statusChoice; candidateHp += 5)
        {
            SetMonData(candidate, MON_DATA_HP, &candidateHp);
            struct Pokemon before = *candidate; struct BattlePokemon opponentBefore = gBattleMons[0];
            *enabled = 8; baseline = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
            *enabled = 20; predicted = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
            CHECK(!memcmp(candidate, &before, sizeof(before)) && !memcmp(&gBattleMons[0], &opponentBefore, sizeof(opponentBefore)));
            statusChoice = baseline == PARTY_SIZE && predicted == 1;
        }
    }
    CHECK(statusChoice); puts("AI_ENTRY_OPPONENT_STATUS_SWITCH_SELECTED");
    /* A known Synchronize can poison the incoming candidate. Its residual
     * damage must change the actual choice, without changing either battler. */
    bool32 reflectedChoice = FALSE;
    gBattleMons[0].hp = gBattleMons[0].maxHP = 100;
    *enabled = 25;
    for (attack = 10; attack <= 120 && !reflectedChoice; attack += 10)
    {
        SetMonData(candidate, MON_DATA_ATK, &attack);
        for (u16 incomingAttack = 20; incomingAttack <= 200 && !reflectedChoice; incomingAttack += 10)
        {
            gBattleMons[0].attack = incomingAttack;
            for (u16 candidateHp = 10; candidateHp <= GetMonData(candidate, MON_DATA_MAX_HP) && !reflectedChoice; candidateHp += 5)
            {
                SetMonData(candidate, MON_DATA_HP, &candidateHp);
                struct Pokemon before = *candidate;
                struct BattlePokemon opponentBefore = gBattleMons[0], activeBefore = gBattleMons[1];
                ai.abilities[0] = ABILITY_NONE;
                baseline = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
                ai.abilities[0] = ABILITY_SYNCHRONIZE;
                predicted = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
                CHECK(!memcmp(candidate, &before, sizeof(before))
                    && !memcmp(&gBattleMons[0], &opponentBefore, sizeof(opponentBefore))
                    && !memcmp(&gBattleMons[1], &activeBefore, sizeof(activeBefore)));
                reflectedChoice = baseline == 1 && predicted == PARTY_SIZE;
            }
        }
    }
    CHECK(reflectedChoice);
    item = ITEM_PECHA_BERRY; SetMonData(candidate, MON_DATA_HELD_ITEM, &item);
    CHECK(GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL) == 1);
    item = ITEM_NONE; SetMonData(candidate, MON_DATA_HELD_ITEM, &item); ai.abilities[0] = ABILITY_NONE;
    puts("AI_ENTRY_SYNCHRONIZE_SWITCH_SELECTED");
    /* Constant turn healing must influence an actual multi-hit survival
     * estimate, while a lethal first attack still cannot be healed. */
    bool32 turnHealChoice = FALSE;
    gBattleMons[0].hp = gBattleMons[0].maxHP = 100;
    for (attack = 10; attack <= 100 && !turnHealChoice; attack += 10)
    {
        SetMonData(candidate, MON_DATA_ATK, &attack);
        for (u16 incomingAttack = 20; incomingAttack <= 200 && !turnHealChoice; incomingAttack += 10)
        {
            gBattleMons[0].attack = incomingAttack;
            for (u16 candidateHp = 10; candidateHp <= GetMonData(candidate, MON_DATA_MAX_HP) && !turnHealChoice; candidateHp += 10)
            {
                SetMonData(candidate, MON_DATA_HP, &candidateHp);
                struct Pokemon before = *candidate; struct BattlePokemon opponent = gBattleMons[0];
                *enabled = 8; baseline = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
                *enabled = 13; predicted = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
                CHECK(!memcmp(candidate, &before, sizeof(before)) && !memcmp(&gBattleMons[0], &opponent, sizeof(opponent)));
                turnHealChoice = baseline == PARTY_SIZE && predicted == 1;
            }
        }
    }
    CHECK(turnHealChoice);
    hp = 1; SetMonData(candidate, MON_DATA_HP, &hp); gBattleMons[0].attack = 250;
    *enabled = 13; CHECK(GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL) == PARTY_SIZE);
    puts("AI_TURN_HEAL_SWITCH_SELECTED");
    bool32 thresholdChoice = FALSE;
    for (attack = 10; attack <= 100 && !thresholdChoice; attack += 10)
    {
        SetMonData(candidate, MON_DATA_ATK, &attack);
        for (u16 incomingAttack = 20; incomingAttack <= 200 && !thresholdChoice; incomingAttack += 10)
        {
            gBattleMons[0].attack = incomingAttack;
            for (u16 candidateHp = 10; candidateHp <= GetMonData(candidate, MON_DATA_MAX_HP) && !thresholdChoice; candidateHp += 10)
            {
                SetMonData(candidate, MON_DATA_HP, &candidateHp);
                struct Pokemon before = *candidate; struct BattlePokemon opponent = gBattleMons[0];
                *enabled = 17; baseline = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
                *enabled = 16; predicted = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
                CHECK(!memcmp(candidate, &before, sizeof(before)) && !memcmp(&gBattleMons[0], &opponent, sizeof(opponent)));
                thresholdChoice = baseline == PARTY_SIZE && predicted == 1;
            }
        }
    }
    CHECK(thresholdChoice);
    hp = 1; SetMonData(candidate, MON_DATA_HP, &hp); gBattleMons[0].attack = 250;
    *enabled = 16; CHECK(GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL) == PARTY_SIZE);
    puts("AI_TURN_THRESHOLD_SWITCH_SELECTED");
    /* A full heal after a Focus Sash save cannot make the sash usable again. */
    hp = GetMonData(candidate, MON_DATA_MAX_HP); SetMonData(candidate, MON_DATA_HP, &hp);
    defense = 1; SetMonData(candidate, MON_DATA_DEF, &defense);
    attack = 1; SetMonData(candidate, MON_DATA_ATK, &attack);
    speed = 1; SetMonData(candidate, MON_DATA_SPEED, &speed);
    item = ITEM_FOCUS_SASH; SetMonData(candidate, MON_DATA_HELD_ITEM, &item);
    gBattleMons[0].attack = 250; gBattleMons[0].defense = 250;
    *enabled = 18; CHECK(GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL) == PARTY_SIZE);
    *enabled = 19; CHECK(GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL) == PARTY_SIZE);
    item = ITEM_NONE; SetMonData(candidate, MON_DATA_HELD_ITEM, &item);
    gBattleMons[0].defense = 50;
    puts("AI_TURN_SASH_ONCE");
    /* Rain from an innate must change an actual switch decision, without
     * replacing live field state or leaving a forecast active afterwards. */
    gBattleWeather = B_WEATHER_SUN_NORMAL; gBattleStruct->weatherDuration = 5;
    gFieldTimers.terrain = B_TERRAIN_NONE; gFieldTimers.terrainTimer = 0;
    SetMonMoveSlot(candidate, MOVE_WATER_GUN, 0);
    defense = 200; SetMonData(candidate, MON_DATA_DEF, &defense);
    speed = 250; SetMonData(candidate, MON_DATA_SPEED, &speed);
    hp = GetMonData(candidate, MON_DATA_MAX_HP); SetMonData(candidate, MON_DATA_HP, &hp);
    gBattleMons[0].types[0] = gBattleMons[0].types[1] = TYPE_NORMAL; gBattleMons[0].types[2] = TYPE_MYSTERY;
    gBattleMons[0].maxHP = 100; gBattleMons[0].attack = 20; gBattleMons[0].defense = gBattleMons[0].spDefense = 50;
    ai.abilities[0] = ABILITY_NONE; bool32 fieldChoice = FALSE;
    for (u16 specialAttack = 10; specialAttack <= 200 && !fieldChoice; specialAttack += 10)
    {
        SetMonData(candidate, MON_DATA_SPATK, &specialAttack);
        for (u16 opposingHp = 10; opposingHp <= 100 && !fieldChoice; opposingHp += 5)
        {
            gBattleMons[0].hp = opposingHp;
            struct Pokemon before = *candidate; struct BattlePokemon beforeOpponent = gBattleMons[0], beforeActive = gBattleMons[1];
            *enabled = 8; baseline = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
            *enabled = 28; predicted = GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
            CHECK(!memcmp(candidate, &before, sizeof(before)) && !memcmp(&gBattleMons[0], &beforeOpponent, sizeof(beforeOpponent))
                && !memcmp(&gBattleMons[1], &beforeActive, sizeof(beforeActive)) && gBattleWeather == B_WEATHER_SUN_NORMAL
                && gBattleStruct->weatherDuration == 5 && gFieldTimers.terrain == B_TERRAIN_NONE && !ModsAiSwitchFieldView(NULL, NULL, NULL));
            fieldChoice = baseline == PARTY_SIZE && predicted == 1;
        }
    }
    CHECK(fieldChoice); puts("AI_ENTRY_FIELD_SWITCH_SELECTED");
    /* Rain Dish can hold HP constant until the predicted rain expires. The
     * threshold-healing forecast must not mistake those turns for a cycle. */
    struct Pokemon beforeRainDish = *candidate;
    CreateMonWithIVs(candidate, SPECIES_LOTAD, 20, 321, OTID_STRUCT_PRESET(888), 31);
    u8 abilityNum = 1; SetMonData(candidate, MON_DATA_ABILITY_NUM, &abilityNum);
    CHECK(GetMonAbility(candidate) == ABILITY_RAIN_DISH);
    SetMonMoveSlot(candidate, MOVE_WATER_GUN, 0);
    defense = 250; SetMonData(candidate, MON_DATA_DEF, &defense);
    speed = 250; SetMonData(candidate, MON_DATA_SPEED, &speed);
    hp = GetMonData(candidate, MON_DATA_MAX_HP); SetMonData(candidate, MON_DATA_HP, &hp);
    gBattleMons[0].hp = gBattleMons[0].maxHP = 100; gBattleMons[0].attack = 1;
    *enabled = 35; (void)GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
    CHECK(gBattleWeather == B_WEATHER_SUN_NORMAL && !ModsAiSwitchFieldView(NULL, NULL, NULL));
    *candidate = beforeRainDish;
    gBattleWeather = 0; gBattleStruct->weatherDuration = 0;
    gParties[B_TRAINER_OPPONENT_A][2] = *candidate;
    gPartiesCount[B_TRAINER_OPPONENT_A] = 3;
    struct BattlePokemon opponentBefore = gBattleMons[0];
    *enabled = 11; (void)GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
    CHECK(!memcmp(&gBattleMons[0], &opponentBefore, sizeof(opponentBefore)));
    *enabled = 24; (void)GetMostSuitableMonToSwitchInto(1, SWITCH_MID_BATTLE_OPTIONAL);
    CHECK(!memcmp(&gBattleMons[0], &opponentBefore, sizeof(opponentBefore)));
    puts("AI_ENTRY_OPPONENT_STAT_SWITCH_SELECTED");
    puts("AI_ENTRY_STAT_SWITCH_SELECTED");
    puts("AI_ENTRY_SWITCH_SELECTED");
    ModsBattleReset(); gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("innate AI entry: scoped prediction, suppression and purity passed");
}
