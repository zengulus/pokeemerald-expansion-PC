#include "battle_controllers.h"
#include "battle_ai_switch.h"
#include "battle_ai_util.h"
#include "battle_main.h"
#include "battle_dome.h"
#include "battle_factory.h"
#include "battle_type_set.h"
#include "battle_gimmick.h"
#include "type_icons.h"
#include "battle_frontier.h"
#include "battle_pike.h"
#include "battle_pyramid.h"
#include "pokedex.h"
#include "follower_helper.h"
#include "wild_encounter.h"
#include "constants/battle_factory.h"
#include "constants/battle_frontier_mons.h"
#include "constants/battle_pike.h"
#include "constants/maps.h"
#include "util.h"

extern void IsGrassTypeInParty(void);
extern bool32 CheckMsgCondition(const struct MsgCondition *, struct Pokemon *, enum Species, struct ObjectEvent *);

static const struct RhhTypeService *sSpeciesTypesRngService;
static bool32 sSpeciesTypesRngBad;
static void SpeciesTypesRngRule(const struct RhhModEvent *event, const void *data, struct RhhModDecision *decision)
{
    (void)decision;
    u8 mode = *(const u8 *)data;
    if (!mode || event->event_key != RhhContentKeyOf("rhh:species/charizard")) return;
    struct RhhTypeList proposal = {.struct_size = sizeof(proposal), .count = 2,
        .types = {RhhContentKeyOf("rhh:type/fire"), RhhContentKeyOf("rhh:type/water")}};
    if (sSpeciesTypesRngService->offer_species_types(&proposal)) sSpeciesTypesRngBad = TRUE;
    if (mode & 1) (void)Random32();
    if (mode & 2) (void)Random2_32();
}
static const struct RhhModHook sSpeciesTypesRngHook = {
    sizeof(struct RhhModHook), RHH_SPECIES_TYPES, RHH_TRANSFORM, 100, SpeciesTypesRngRule, NULL
};
static const struct RhhModDefinition sSpeciesTypesRngMod = {.struct_size = sizeof(sSpeciesTypesRngMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:species_types_rng", .state_size = 1,
    .hook_count = 1, .hooks = &sSpeciesTypesRngHook};

static void CheckSpeciesTypePartySpecial(void)
{
    struct Pokemon savedParty[PARTY_SIZE];
    memcpy(savedParty, gParties[B_TRAINER_PLAYER], sizeof(savedParty));
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER];
    u16 savedResult = gSpecialVar_Result;
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(savedParty));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_CHARIZARD, 20, 5001, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct MsgCondition grass = MATCH_TYPES(TYPE_GRASS, TYPE_GRASS);
    struct MsgCondition notGrass = MATCH_NOT_TYPES(TYPE_GRASS, TYPE_GRASS);
    RhhModsSetRunContext(0, 0, 0);
    IsGrassTypeInParty(); CHECK(!gSpecialVar_Result);
    CHECK(!CheckMsgCondition(&grass, &gParties[B_TRAINER_PLAYER][0], SPECIES_CHARIZARD, NULL));
    CHECK(CheckMsgCondition(&notGrass, &gParties[B_TRAINER_PLAYER][0], SPECIES_CHARIZARD, NULL));
    RhhModsSetRunContext(123, 456, 0);
    IsGrassTypeInParty(); CHECK(gSpecialVar_Result);
    CHECK(CheckMsgCondition(&grass, &gParties[B_TRAINER_PLAYER][0], SPECIES_CHARIZARD, NULL));
    CHECK(!CheckMsgCondition(&notGrass, &gParties[B_TRAINER_PLAYER][0], SPECIES_CHARIZARD, NULL));
    u8 egg = TRUE;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &egg);
    IsGrassTypeInParty(); CHECK(!gSpecialVar_Result);
    memcpy(gParties[B_TRAINER_PLAYER], savedParty, sizeof(savedParty));
    gPartiesCount[B_TRAINER_PLAYER] = savedCount;
    gSpecialVar_Result = savedResult;
}

static void CheckSpeciesTypePikeStatus(void)
{
    struct Pokemon savedParty[PARTY_SIZE]; memcpy(savedParty, gParties[B_TRAINER_PLAYER], sizeof(savedParty));
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER];
    u8 savedHint = gSaveBlock2Ptr->frontier.pikeHintedRoomType;
    u8 savedRoom = gSaveBlock2Ptr->frontier.pikeHintedRoomIndex;
    u8 savedBattle = gSaveBlock2Ptr->frontier.curChallengeBattleNum;
    u16 savedFunction = gSpecialVar_0x8004, savedRoomVar = gSpecialVar_0x8007;
    rng_value_t savedRng = gRngValue, candidate = savedRng, selected = savedRng;
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(savedParty));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_MAREEP, 20, 5678, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    gSaveBlock2Ptr->frontier.pikeHintedRoomType = PIKE_ROOM_STATUS;
    gSaveBlock2Ptr->frontier.pikeHintedRoomIndex = 0;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 0;
    gSpecialVar_0x8004 = BATTLE_PIKE_FUNC_SET_ROOM_TYPE; gSpecialVar_0x8007 = 0;
    bool32 found = FALSE;
    for (u32 i = 0; i < 256 && !found; ++i)
    {
        gRngValue = candidate; Random(); candidate = gRngValue;
        gRngValue = candidate; u8 slots[FRONTIER_PARTY_SIZE] = {0, 1, 2};
        Shuffle(slots, ARRAY_COUNT(slots), sizeof(slots[0]));
        u32 first = Random() % 100, second = Random() % 100;
        if (first < 35 && second >= 35 && second < 60) { selected = candidate; found = TRUE; }
    }
    CHECK(found);
    RhhModsSetRunContext(0, 0, 0); gRngValue = selected; CallBattlePikeFunction();
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_STATUS) == STATUS1_TOXIC_POISON);
    u32 clear = 0; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_STATUS, &clear);
    RhhModsSetRunContext(123, 456, 0); gRngValue = selected; CallBattlePikeFunction();
    u32 status = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_STATUS);
    CHECK(status == STATUS1_FREEZE || status == STATUS1_FROSTBITE);
    memcpy(gParties[B_TRAINER_PLAYER], savedParty, sizeof(savedParty)); gPartiesCount[B_TRAINER_PLAYER] = savedCount;
    gSaveBlock2Ptr->frontier.pikeHintedRoomType = savedHint;
    gSaveBlock2Ptr->frontier.pikeHintedRoomIndex = savedRoom;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = savedBattle;
    gSpecialVar_0x8004 = savedFunction; gSpecialVar_0x8007 = savedRoomVar; gRngValue = savedRng;
}

static void CheckSpeciesTypeWildAttraction(void)
{
    struct Pokemon savedPlayer[PARTY_SIZE], savedEnemy[PARTY_SIZE];
    memcpy(savedPlayer, gParties[B_TRAINER_PLAYER], sizeof(savedPlayer));
    memcpy(savedEnemy, gParties[B_TRAINER_OPPONENT_A], sizeof(savedEnemy));
    u8 savedPlayerCount = gPartiesCount[B_TRAINER_PLAYER], savedEnemyCount = gPartiesCount[B_TRAINER_OPPONENT_A];
    struct MapHeader savedMap = gMapHeader;
    u8 savedGroup = gSaveBlock1Ptr->location.mapGroup, savedNum = gSaveBlock1Ptr->location.mapNum;
    u16 savedRepel = VarGet(VAR_REPEL_STEP_COUNT);
    rng_value_t savedRng = gRngValue, savedRng2 = gRng2Value, candidate = savedRng, selected = savedRng;
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE103)][MAP_NUM(MAP_ROUTE103)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE103); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE103);
    VarSet(VAR_REPEL_STEP_COUNT, 0);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_MAGNEMITE, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    u8 abilitySlot = 0; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ABILITY_NUM, &abilitySlot);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    CHECK(GetMonAbility(&gParties[B_TRAINER_PLAYER][0]) == ABILITY_MAGNET_PULL);
    struct WildPokemon table[NUM_LAND_MONS_ENCOUNTER_SLOTS];
    for (u32 i = 0; i < ARRAY_COUNT(table); ++i) table[i] = (struct WildPokemon){5, 5, SPECIES_POOCHYENA};
    table[11].species = SPECIES_MAREEP;
    struct WildPokemonInfo wild = {.encounterRate = 20, .wildPokemon = table};
    bool32 found = FALSE;
    for (u32 i = 0; i < 128 && !found; ++i)
    {
        gRngValue = candidate; Random(); candidate = gRngValue;
        gRngValue = candidate;
        if (!(Random() % 2) && ChooseWildMonIndex_Land() != 11) { selected = candidate; found = TRUE; }
    }
    CHECK(found);
    RhhModsSetRunContext(0, 0, 0); gRngValue = selected;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, -1, -1));
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_POOCHYENA);
    RhhModsSetRunContext(123, 456, 0); gRngValue = selected;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, -1, -1));
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_MAREEP);
    memcpy(gParties[B_TRAINER_PLAYER], savedPlayer, sizeof(savedPlayer));
    memcpy(gParties[B_TRAINER_OPPONENT_A], savedEnemy, sizeof(savedEnemy));
    gPartiesCount[B_TRAINER_PLAYER] = savedPlayerCount; gPartiesCount[B_TRAINER_OPPONENT_A] = savedEnemyCount;
    gMapHeader = savedMap; gSaveBlock1Ptr->location.mapGroup = savedGroup; gSaveBlock1Ptr->location.mapNum = savedNum;
    VarSet(VAR_REPEL_STEP_COUNT, savedRepel); gRngValue = savedRng; gRng2Value = savedRng2;
}

#if BATTLE_PYRAMID_RANDOM_ENCOUNTERS == TRUE
static void CheckSpeciesTypePyramid(void)
{
    struct Pokemon saved = gParties[B_TRAINER_OPPONENT_A][0];
    u8 savedMode = gSaveBlock2Ptr->frontier.lvlMode;
    u16 savedStreak = gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50];
    rng_value_t savedRng = gRngValue;
    gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50] = 6 * 7; /* Ice-type round. */
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_BULBASAUR, 50, 6789, OTID_STRUCT_PRESET(54321), 31);
    GenerateBattlePyramidWildMon(SPECIES_CATERPIE);
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_CATERPIE);
    struct BattlePokemon battleMon; PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &battleMon);
    CHECK(battleMon.types[0] == TYPE_FIRE && battleMon.types[1] == TYPE_FLYING && battleMon.types[2] == TYPE_ICE);
    gParties[B_TRAINER_OPPONENT_A][0] = saved;
    gSaveBlock2Ptr->frontier.lvlMode = savedMode;
    gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50] = savedStreak;
    gRngValue = savedRng;
    puts("species types: randomized Pyramid requirement passed");
}
#endif

static void CheckSpeciesTypes(const char *module)
{
    struct BattleTypeList wideList = {0};
    for (u32 i = 0; i < BATTLE_TYPE_SET_CAPACITY; ++i)
        BattleTypeListAdd(&wideList, i);
    BattleTypeListAdd(&wideList, TYPE_FIRE);
    CHECK(wideList.count == BATTLE_TYPE_SET_CAPACITY - 2
        && BattleTypeListContains(&wideList, TYPE_FIRE) && BattleTypeListContains(&wideList, 255)
        && !BattleTypeListContains(&wideList, TYPE_NONE) && !BattleTypeListContains(&wideList, TYPE_MYSTERY));
    struct BattleTypeSet wideTypes = {0};
    CHECK(!BattleTypeSetCount(&wideTypes));
    BattleTypeSetAdd(&wideTypes, TYPE_FIRE);
    BattleTypeSetAdd(&wideTypes, 31); BattleTypeSetAdd(&wideTypes, 32);
    BattleTypeSetAdd(&wideTypes, 255); BattleTypeSetAdd(&wideTypes, 255);
    BattleTypeSetAdd(&wideTypes, 256); /* Out of range must not modify memory. */
    CHECK(BattleTypeSetCount(&wideTypes) == 4);
    u8 counts[BATTLE_TYPE_SET_CAPACITY] = {0};
    CHECK(BattleTypeCountsUniqueLeader(counts) == BATTLE_TYPE_SET_CAPACITY);
    counts[TYPE_FIRE] = 2; counts[32] = 3; counts[255] = 3;
    CHECK(BattleTypeCountsUniqueLeader(counts) == BATTLE_TYPE_SET_CAPACITY);
    counts[255] = 4; CHECK(BattleTypeCountsUniqueLeader(counts) == 255);
    counts[TYPE_FIRE] = 5; CHECK(BattleTypeCountsUniqueLeader(counts) == TYPE_FIRE);
    enum Type firstTypes[3] = {TYPE_FIRE, 32, 32};
    enum Type secondTypes[3] = {32, 255, TYPE_MYSTERY};
    enum Type thirdTypes[3] = {255, 255, TYPE_MYSTERY};
    struct BattleTypeSet partyTypes = {0};
    BattleTypeSetAddSpecies(&partyTypes, firstTypes);
    BattleTypeSetAddSpecies(&partyTypes, secondTypes);
    BattleTypeSetAddSpecies(&partyTypes, thirdTypes);
    CHECK(BattleTypeSetCount(&partyTypes) == 3);
    memset(counts, 0, sizeof(counts));
    BattleTypeCountsAddSpecies(counts, firstTypes);
    BattleTypeCountsAddSpecies(counts, secondTypes);
    CHECK(counts[TYPE_FIRE] == 1 && counts[32] == 2 && counts[255] == 1
        && counts[TYPE_MYSTERY] == 0 && BattleTypeCountsUniqueLeader(counts) == 32);
    BattleTypeCountsAddSpecies(counts, thirdTypes);
    CHECK(counts[255] == 2 && BattleTypeCountsUniqueLeader(counts) == BATTLE_TYPE_SET_CAPACITY);
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsLoadLibrary(module));
    CHECK(!RhhModsRegister(&sSpeciesTypesRngMod) && !RhhModsFinalize());
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhTypeService *types = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 2));
    CHECK(types && types->header.struct_size >= sizeof(*types));
    sSpeciesTypesRngService = types;
    RhhContentKey charizard = host->content_key("rhh:species/charizard"), torchic = host->content_key("rhh:species/torchic");
    RhhContentKey fire = host->content_key("rhh:type/fire"), flying = host->content_key("rhh:type/flying");
    RhhContentKey grass = host->content_key("rhh:type/grass"), water = host->content_key("rhh:type/water");
    struct RhhTypeList list = {.struct_size = sizeof(list)};
    CHECK(!types->species(charizard, &list) && list.count == 2 && list.types[0] == fire && list.types[1] == flying);
    GetSpeciesTypeList(SPECIES_CHARIZARD, &wideList);
    CHECK(wideList.count == 2 && wideList.types[0] == TYPE_FIRE && wideList.types[1] == TYPE_FLYING);
    CHECK(PokedexSpeciesMatchesTypes(SPECIES_TORCHIC, TYPE_FIRE, TYPE_FIRE));
    CHECK(!PokedexSpeciesMatchesTypes(SPECIES_TORCHIC, TYPE_WATER, TYPE_NONE));
    CHECK(PokedexSpeciesMatchesTypes(SPECIES_CHARIZARD, TYPE_FLYING, TYPE_FIRE));
    struct Pokemon skyMon;
    CreateMonWithIVs(&skyMon, SPECIES_EEVEE, 20, 3001, OTID_STRUCT_PRESET(54321), 31);
    CHECK(!CanMonParticipateInSkyBattle(&skyMon));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_PANCHAM, 32, 3002, OTID_STRUCT_PRESET(54321), 31);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_MAREEP, 20, 3003, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 2;
    bool32 canStop = TRUE;
    const struct EvolutionParam steelCondition[] = {{IF_TYPE_IN_PARTY, TYPE_STEEL, 0, 0}, {CONDITIONS_END, 0, 0, 0}};
    CHECK(GetEvolutionTargetSpecies(&gParties[B_TRAINER_PLAYER][0], EVO_MODE_NORMAL, 0, NULL, &canStop, CHECK_EVO) == SPECIES_NONE);
    CHECK(!DoesMonMeetAdditionalConditions(&gParties[B_TRAINER_PLAYER][0], steelCondition, NULL, 0, &canStop, CHECK_EVO));
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_BRICK_BREAK, SPECIES_MAREEP, EFFECTIVENESS_MODE_GOOD) == 2);
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_CATERPIE, EFFECTIVENESS_MODE_GOOD) == 4);
    u16 savedFactory[FRONTIER_PARTY_SIZE]; memcpy(savedFactory, gFrontierTempParty, sizeof(savedFactory));
    u16 savedSpecial = gSpecialVar_0x8004, savedResult = gSpecialVar_Result;
    const struct TrainerMon *savedFactoryMons = gFacilityTrainerMons;
    gFrontierTempParty[0] = FRONTIER_MON_MAREEP;
    gFrontierTempParty[1] = FRONTIER_MON_ARON;
    gFrontierTempParty[2] = FRONTIER_MON_EEVEE;
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_GET_OPPONENT_MON_TYPE;
    CallBattleFactoryFunction(); CHECK(gSpecialVar_Result == NUMBER_OF_MON_TYPES);
    CHECK(types->offer_species_types(&list) && types->species_proposal(&list));
    RhhModsSetRunContext(123, 456, 0); rng_value_t rng = gRngValue;
    CHECK(CanMonParticipateInSkyBattle(&skyMon));
    CHECK(IsSpeciesOfType(SPECIES_MAREEP, TYPE_STEEL) && IsSpeciesOfType(SPECIES_MAREEP, TYPE_DARK));
    CHECK(!IsSpeciesOfType(SPECIES_MAREEP, TYPE_MYSTERY));
    CHECK(GetEvolutionTargetSpecies(&gParties[B_TRAINER_PLAYER][0], EVO_MODE_NORMAL, 0, NULL, &canStop, CHECK_EVO) == SPECIES_PANGORO);
    CHECK(DoesMonMeetAdditionalConditions(&gParties[B_TRAINER_PLAYER][0], steelCondition, NULL, 0, &canStop, CHECK_EVO));
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_BRICK_BREAK, SPECIES_MAREEP, EFFECTIVENESS_MODE_GOOD) == 8);
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_BRICK_BREAK, SPECIES_MAREEP, EFFECTIVENESS_MODE_BAD) == -4);
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_CATERPIE, EFFECTIVENESS_MODE_GOOD) == 16);
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_CATERPIE, EFFECTIVENESS_MODE_BAD) == -8);
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_CATERPIE, EFFECTIVENESS_MODE_AI_VS_AI) == 28);
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_WEEDLE, EFFECTIVENESS_MODE_GOOD) == 0);
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_WEEDLE, EFFECTIVENESS_MODE_BAD) == 6);
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_WEEDLE, EFFECTIVENESS_MODE_AI_VS_AI) == -12);
    CallBattleFactoryFunction(); CHECK(gSpecialVar_Result == TYPE_STEEL);
    memcpy(gFrontierTempParty, savedFactory, sizeof(savedFactory));
    gSpecialVar_0x8004 = savedSpecial; gSpecialVar_Result = savedResult; gFacilityTrainerMons = savedFactoryMons;
    CheckSpeciesTypePikeStatus();
    CheckSpeciesTypeWildAttraction();
    CheckSpeciesTypePartySpecial();
#if BATTLE_PYRAMID_RANDOM_ENCOUNTERS == TRUE
    CheckSpeciesTypePyramid();
#endif
    memset(&gParties[B_TRAINER_PLAYER][1], 0, sizeof(struct Pokemon));
    CHECK(!types->species(charizard, &list) && list.count == 3 && list.types[0] == fire && list.types[1] == flying && list.types[2] == grass);
    u8 *rngMode = RhhModsState("test:species_types_rng"); CHECK(rngMode);
    rng_value_t rng1Before = gRngValue, rng2Before = gRng2Value;
    for (u8 mode = 1; mode <= 3; ++mode)
    {
        *rngMode = mode;
        CHECK(!types->species(charizard, &list) && list.count == 2 && list.types[0] == fire && list.types[1] == flying
            && !memcmp(&rng1Before, &gRngValue, sizeof(rng1Before)) && !memcmp(&rng2Before, &gRng2Value, sizeof(rng2Before))
            && !sSpeciesTypesRngBad);
        GetSpeciesTypeList(SPECIES_CHARIZARD, &wideList);
        CHECK(wideList.count == 2 && wideList.types[0] == TYPE_FIRE && wideList.types[1] == TYPE_FLYING
            && !memcmp(&rng1Before, &gRngValue, sizeof(rng1Before)) && !memcmp(&rng2Before, &gRng2Value, sizeof(rng2Before)));
    }
    *rngMode = 0;
    GetSpeciesTypeList(SPECIES_CHARIZARD, &wideList);
    CHECK(wideList.count == 3 && wideList.types[2] == TYPE_GRASS);
    CHECK(!types->species(torchic, &list) && list.count == 3 && list.types[0] == fire && list.types[1] == grass && list.types[2] == water);
    CHECK(PokedexSpeciesMatchesTypes(SPECIES_TORCHIC, TYPE_WATER, TYPE_NONE));
    CHECK(PokedexSpeciesMatchesTypes(SPECIES_TORCHIC, TYPE_NONE, TYPE_WATER));
    CHECK(PokedexSpeciesMatchesTypes(SPECIES_TORCHIC, TYPE_WATER, TYPE_GRASS));
    CHECK(PokedexSpeciesMatchesTypes(SPECIES_TORCHIC, TYPE_GRASS, TYPE_WATER));
    CHECK(!PokedexSpeciesMatchesTypes(SPECIES_TORCHIC, TYPE_FLYING, TYPE_WATER));
    CHECK(!PokedexSpeciesMatchesTypes(SPECIES_TORCHIC, TYPE_FIRE, TYPE_FIRE));
    CHECK(!types->species(host->content_key("rhh:species/pikachu"), &list) && list.count == 1
        && list.types[0] == host->content_key("rhh:type/electric"));
    CHECK(!types->species(host->content_key("rhh:species/gengar"), &list) && list.count == 2
        && list.types[0] == host->content_key("rhh:type/ghost") && list.types[1] == host->content_key("rhh:type/poison"));
    struct RhhSpeciesTypesContext outside = {.struct_size = sizeof(outside), .species = 99};
    CHECK(types->species_context(&outside) && outside.species == 99);
    struct BattleResources resources = {0}; struct AiLogicData ai = {0}; struct AiPartyData aiParties = {0};
    struct AiThinkingStruct thinking = {0}; gAiThinkingStruct = &thinking;
    struct BattleHistory history = {0}; gBattleHistory = &history;
    aiParties.mons[B_TRAINER_PLAYER][1].wasSentInBattle = TRUE;
    memset(&sBattle, 0, sizeof(sBattle)); gBattleStruct = &sBattle; gBattleResources = &resources;
    gAiLogicData = &ai; gAiPartyData = &aiParties; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER; gBattlersCount = 2; gAbsentBattlerFlags = 0;
    for (u32 i = 0; i < 2; ++i)
    {
        CreateMonWithIVs(&gParties[i][0], i ? SPECIES_CHARIZARD : SPECIES_TORCHIC, 20, 1000 + i, OTID_STRUCT_PRESET(54321 + i), 31);
        gPartiesCount[i] = 1; gBattlerPositions[i] = i; gBattlerPartyIndexes[i] = 0;
        PokemonToBattleMon(&gParties[i][0], &gBattleMons[i]);
    }
    CHECK(gBattleMons[0].types[0] == TYPE_FIRE && gBattleMons[0].types[1] == TYPE_GRASS && gBattleMons[0].types[2] == TYPE_WATER);
    CHECK(gBattleMons[1].types[0] == TYPE_FIRE && gBattleMons[1].types[1] == TYPE_FLYING && gBattleMons[1].types[2] == TYPE_GRASS);
    enum Type publicIcons[3];
    CHECK(GetBattlerPublicTypeIcons(1, publicIcons) == 3 && publicIcons[0] == TYPE_FIRE
        && publicIcons[1] == TYPE_FLYING && publicIcons[2] == TYPE_GRASS);
    gBattleMons[1].types[1] = TYPE_FIRE;
    CHECK(GetBattlerPublicTypeIcons(1, publicIcons) == 2 && publicIcons[0] == TYPE_FIRE
        && publicIcons[1] == TYPE_GRASS);
    gBattleMons[1].types[1] = TYPE_FLYING;
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    ResetSpriteData(); FreeAllSpritePalettes();
    LoadTypeIcons(B_BATTLER_0);
    u32 iconCount = 0, iconTypes = 0;
    for (u32 i = 0; i < MAX_SPRITES; ++i)
    {
        const struct Sprite *sprite = &gSprites[i];
        if (!sprite->inUse || sprite->tMonPosition != B_POSITION_OPPONENT_LEFT) continue;
        ++iconCount;
        if (sprite->animNum == TYPE_FIRE) iconTypes |= 1;
        if (sprite->animNum == TYPE_FLYING) iconTypes |= 2;
        if (sprite->animNum == TYPE_GRASS) iconTypes |= 4;
    }
    CHECK(iconCount == 3 && iconTypes == 7);
    ResetSpriteData(); FreeAllSpritePalettes();
#endif
    CHECK(AI_GetBattlerTypeMatchup(0, 1) == (UQ_4_12(1) + UQ_4_12(0.125) + UQ_4_12(1)) * 2 / 3);
    gBattleMons[0].types[2] = TYPE_MYSTERY;
    CHECK(AI_GetBattlerTypeMatchup(0, 1) == UQ_4_12(1.125));
    gBattleMons[1].types[2] = TYPE_MYSTERY;
    CHECK(AI_GetBattlerTypeMatchup(0, 1) == UQ_4_12(0.75));
    gBattleMons[0].types[2] = TYPE_WATER;
    gBattleMons[1].types[2] = TYPE_GRASS;
    u32 maxHp = gBattleMons[1].maxHP;
    gBattleMons[1].types[2] = TYPE_ICE;
    CHECK(GetStealthHazardDamage(TYPE_SIDE_HAZARD_POINTED_STONES, 1) == maxHp);
    gBattleMons[1].types[0] = TYPE_FIGHTING;
    gBattleMons[1].types[1] = TYPE_GROUND;
    gBattleMons[1].types[2] = TYPE_STEEL;
    CHECK(GetStealthHazardDamage(TYPE_SIDE_HAZARD_POINTED_STONES, 1) == (maxHp / 64 ? maxHp / 64 : 1));
    gBattleMons[1].types[0] = TYPE_FIRE;
    gBattleMons[1].types[1] = TYPE_FLYING;
    gBattleMons[1].types[2] = TYPE_GRASS;
    gBattleMons[0].types[0] = gBattleMons[0].types[1] = TYPE_ELECTRIC;
    gBattleMons[0].types[2] = TYPE_MYSTERY;
    gBattleMons[1].types[1] = TYPE_WATER;
    gBattleMons[1].types[2] = TYPE_FLYING;
    CHECK(AI_GetBattlerTypeMatchup(0, 1) == UQ_4_12(8));
    gBattleMons[1].volatiles.roostActive = TRUE;
    CHECK(GetBattlerPublicTypeIcons(1, publicIcons) == 2 && publicIcons[0] == TYPE_FIRE
        && publicIcons[1] == TYPE_WATER);
    enum Type roostTypes[3]; GetBattlerTypes(1, FALSE, roostTypes);
    CHECK(roostTypes[0] == TYPE_FIRE && roostTypes[1] == TYPE_WATER && roostTypes[2] == TYPE_MYSTERY);
    GetBattlerTypeList(1, FALSE, &wideList);
    CHECK(wideList.count == 2 && wideList.types[0] == TYPE_FIRE && wideList.types[1] == TYPE_WATER);
    CHECK(AI_GetBattlerTypeMatchup(0, 1) == UQ_4_12(4));
    CHECK(GetStealthHazardDamage(TYPE_SIDE_HAZARD_POINTED_STONES, 1) == maxHp / 4);
    gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_FLYING;
    gBattleMons[1].types[2] = TYPE_GRASS;
    GetBattlerTypes(1, FALSE, roostTypes);
    CHECK(roostTypes[0] == TYPE_MYSTERY && roostTypes[1] == TYPE_MYSTERY && roostTypes[2] == TYPE_GRASS);
    CHECK(GetBattlerPublicTypeIcons(1, publicIcons) == 1 && publicIcons[0] == TYPE_GRASS);
    CHECK(GetDynamicMoveType(&gParties[B_TRAINER_OPPONENT_A][0], MOVE_REVELATION_DANCE, 1,
                             ABILITY_NONE, HOLD_EFFECT_NONE, MON_IN_BATTLE) == TYPE_GRASS);
    gBattleMons[1].volatiles.roostActive = FALSE;
    gBattleMons[0].types[0] = TYPE_FIRE; gBattleMons[0].types[1] = TYPE_GRASS; gBattleMons[0].types[2] = TYPE_WATER;
    gBattleMons[1].types[0] = TYPE_FIRE; gBattleMons[1].types[1] = TYPE_FLYING; gBattleMons[1].types[2] = TYPE_GRASS;
    ModsBattleBegin(); CHECK(!types->battler(1, &list) && list.count == 3 && list.types[2] == grass);
    u8 oldTera = GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE), waterTera = TYPE_WATER;
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE, &waterTera);
    SetActiveGimmick(1, GIMMICK_TERA);
    CHECK(GetBattlerPublicTypeIcons(1, publicIcons) == 1 && publicIcons[0] == TYPE_WATER);
    GetBattlerTypeList(1, FALSE, &wideList);
    CHECK(wideList.count == 1 && wideList.types[0] == TYPE_WATER);
    GetBattlerTypeList(1, TRUE, &wideList);
    CHECK(wideList.count == 3 && wideList.types[0] == TYPE_FIRE && wideList.types[2] == TYPE_GRASS);
    SetActiveGimmick(1, GIMMICK_NONE);
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE, &oldTera);
    gBattleStruct->illusion[0].state = ILLUSION_ON;
    gBattleStruct->illusion[0].mon = &gParties[B_TRAINER_OPPONENT_A][0];
    CHECK(GetBattlerPublicTypeIcons(0, publicIcons) == 3 && publicIcons[0] == TYPE_FIRE
        && publicIcons[1] == TYPE_FLYING && publicIcons[2] == TYPE_GRASS);
    gBattleMons[0].volatiles.roostActive = TRUE;
    CHECK(GetBattlerPublicTypeIcons(0, publicIcons) == 2 && publicIcons[0] == TYPE_FIRE
        && publicIcons[1] == TYPE_GRASS);
    gBattleMons[0].volatiles.roostActive = FALSE;
    ClearIllusionMon(0);
    gBattlerBattleController[0] = BATTLE_CONTROLLER_PLAYER;
    gBattleStruct->illusion[0].state = ILLUSION_ON;
    gBattleStruct->illusion[0].mon = &gParties[B_TRAINER_OPPONENT_A][0];
    struct BattleTypeList knownTypes, simulatedTypes;
    struct BattlePokemon knownBefore = gBattleMons[0];
    AI_GetKnownBattlerTypeList(1, 0, &knownTypes);
    CHECK(!memcmp(&knownBefore, &gBattleMons[0], sizeof(knownBefore)));
    SaveBattlerData(0); SetBattlerData(0);
    GetBattlerTypeList(0, FALSE, &simulatedTypes);
    CHECK(BattleTypeListsEqual(&knownTypes, &simulatedTypes));
    CHECK(gBattleMons[0].types[0] == TYPE_FIRE && gBattleMons[0].types[1] == TYPE_FLYING
        && gBattleMons[0].types[2] == TYPE_GRASS);
    RestoreBattlerData(0);
    CHECK(gBattleMons[0].types[0] == TYPE_FIRE && gBattleMons[0].types[1] == TYPE_GRASS
        && gBattleMons[0].types[2] == TYPE_WATER);
    gBattlerBattleController[1] = BATTLE_CONTROLLER_OPPONENT;
    CHECK(AI_GetMoveEffectiveness(MOVE_WATER_GUN, 1, 0) == UQ_4_12(1));
    CHECK(gBattleMons[0].types[0] == TYPE_FIRE && gBattleMons[0].types[1] == TYPE_GRASS
        && gBattleMons[0].types[2] == TYPE_WATER && gBattleMons[1].types[2] == TYPE_GRASS);
    struct Pokemon disguise;
    CreateMonWithIVs(&disguise, SPECIES_PIKACHU, 20, 4001, OTID_STRUCT_PRESET(54321), 31);
    gBattleStruct->illusion[0].mon = &disguise;
    SaveBattlerData(0); SetBattlerData(0);
    CHECK(gBattleMons[0].types[0] == TYPE_ELECTRIC && gBattleMons[0].types[1] == TYPE_ELECTRIC
        && gBattleMons[0].types[2] == TYPE_MYSTERY);
    RestoreBattlerData(0); CHECK(gBattleMons[0].types[2] == TYPE_WATER);
    gBattleMons[0].types[2] = TYPE_ELECTRIC;
    gBattleStruct->illusion[0].mon = &gParties[B_TRAINER_OPPONENT_A][0];
    AI_GetKnownBattlerTypeList(1, 0, &knownTypes);
    SaveBattlerData(0); SetBattlerData(0);
    GetBattlerTypeList(0, FALSE, &simulatedTypes);
    CHECK(BattleTypeListsEqual(&knownTypes, &simulatedTypes)
        && BattleTypeListContains(&knownTypes, TYPE_ELECTRIC));
    CHECK(gBattleMons[0].types[0] == TYPE_FIRE && gBattleMons[0].types[1] == TYPE_FLYING
        && gBattleMons[0].types[2] == TYPE_ELECTRIC);
    RestoreBattlerData(0); CHECK(gBattleMons[0].types[2] == TYPE_ELECTRIC);
    u8 oldTera0 = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE), stellarTera = TYPE_STELLAR;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE, &stellarTera);
    SetActiveGimmick(0, GIMMICK_TERA); gBattleMons[0].volatiles.roostActive = TRUE;
    AI_GetKnownBattlerTypeList(1, 0, &knownTypes);
    SaveBattlerData(0); SetBattlerData(0); GetBattlerTypeList(0, FALSE, &simulatedTypes);
    CHECK(BattleTypeListsEqual(&knownTypes, &simulatedTypes) && BattleTypeListContains(&knownTypes, TYPE_FLYING));
    RestoreBattlerData(0); gBattleMons[0].volatiles.roostActive = FALSE;
    SetActiveGimmick(0, GIMMICK_NONE); SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE, &oldTera0);
    gBattlerBattleController[0] = BATTLE_CONTROLLER_OPPONENT;
    AI_GetKnownBattlerTypeList(1, 0, &knownTypes); GetBattlerTypeList(0, FALSE, &simulatedTypes);
    CHECK(BattleTypeListsEqual(&knownTypes, &simulatedTypes));
    gBattlerBattleController[0] = BATTLE_CONTROLLER_PLAYER;
    gBattleMons[0].types[2] = TYPE_WATER; gBattleStruct->illusion[0].state = ILLUSION_OFF;
    gBattleStruct->illusion[1].state = ILLUSION_ON;
    gBattleStruct->illusion[1].mon = &gParties[B_TRAINER_PLAYER][0];
    struct DamageContext reveal = {.battlerAtk = 0, .battlerDef = 1, .move = MOVE_THUNDERBOLT,
        .chosenMove = MOVE_THUNDERBOLT, .baseMove = MOVE_THUNDERBOLT, .moveType = TYPE_ELECTRIC, .updateFlags = TRUE};
    CHECK(CalcTypeEffectivenessMultiplier(&reveal) == UQ_4_12(1));
    CHECK(history.abilities[1] == ABILITY_NONE);
    reveal.move = reveal.chosenMove = reveal.baseMove = MOVE_WATER_GUN; reveal.moveType = TYPE_WATER;
    CHECK(CalcTypeEffectivenessMultiplier(&reveal) == UQ_4_12(1));
    CHECK(history.abilities[1] == ABILITY_ILLUSION);
    history.abilities[1] = ABILITY_NONE; aiParties.mons[B_TRAINER_OPPONENT_A][0].ability = ABILITY_NONE;
    gBattleStruct->illusion[1].state = ILLUSION_OFF;
    struct DamageContext damage = {.battlerAtk = B_BATTLER_0, .battlerDef = B_BATTLER_1,
        .move = MOVE_WATER_GUN, .chosenMove = MOVE_WATER_GUN, .baseMove = MOVE_WATER_GUN, .moveType = TYPE_WATER};
    damage.typeEffectivenessModifier = CalcTypeEffectivenessMultiplier(&damage);
    CHECK(damage.typeEffectivenessModifier == UQ_4_12(1) && ApplyModifiersAfterDmgRoll(&damage, 100) == 150);
    damage.aiCalc = TRUE; CHECK(ApplyModifiersAfterDmgRoll(&damage, 100) == 150);
    CHECK(CalcPartyMonTypeEffectivenessMultiplier(MOVE_WATER_GUN, SPECIES_CHARIZARD, ABILITY_NONE) == UQ_4_12(1));
    SET_BATTLER_TYPE(0, TYPE_ELECTRIC);
    CHECK(!types->battler(0, &list) && list.count == 1 && list.types[0] == host->content_key("rhh:type/electric"));
    CopyMonAbilityAndTypesToBattleMon(0, &gParties[B_TRAINER_PLAYER][0]);
    CHECK(!types->battler(0, &list) && list.count == 3 && list.types[1] == grass && list.types[2] == water);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_CHARIZARD, 20, 2001, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 2;
    struct BattlePokemon next; PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][1], &next);
    memcpy(&resources.bufferB[0][4], &next, BATTLE_POKEMON_TRANSFER_SIZE); gBattlerPartyIndexes[0] = 1;
    gBattleControllerExecFlags = 0; const u8 update[] = {B_SCR_OP_SWITCHINDATAUPDATE, BS_BATTLER_0};
    gBattlescriptCurrInstr = update; gBattleScriptingCommandsTable[B_SCR_OP_SWITCHINDATAUPDATE]();
    CHECK(gBattleMons[0].species == SPECIES_CHARIZARD && gBattleMons[0].types[0] == TYPE_FIRE
        && gBattleMons[0].types[1] == TYPE_FLYING && gBattleMons[0].types[2] == TYPE_GRASS);
    gBattleTypeFlags = BATTLE_TYPE_LINK;
    CHECK(!types->species(charizard, &list) && list.count == 2);
    struct BattlePokemon linkMon; PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][1], &linkMon);
    CHECK(linkMon.types[0] == TYPE_FIRE && linkMon.types[1] == TYPE_FLYING && linkMon.types[2] == TYPE_MYSTERY);
    gBattleTypeFlags = BATTLE_TYPE_RECORDED;
    CHECK(!types->species(torchic, &list) && list.count == 1 && list.types[0] == fire);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)));
    ModsBattleReset(); gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("species types: composition, three-type combat, switching and link exclusions passed");
}
