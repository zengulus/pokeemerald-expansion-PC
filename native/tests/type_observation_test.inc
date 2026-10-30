#include "battle_gimmick.h"

struct TypeObservationProbeState { u32 count; struct RhhBattlerTypeObservation last; };
static u32 sTypeObservationLocalCalls;
static void TypeObservationLocal(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    ++sTypeObservationLocalCalls;
    (void)Random(); (void)Random2();
    RhhModsObserveBattleTypes(); /* Recursion must not sample or dispatch. */
}
static const struct RhhModHook sTypeObservationHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLER_TYPES_OBSERVED, RHH_OBSERVE, 0, NULL, TypeObservationLocal}
};
static const struct RhhModDefinition sTypeObservationMod = {
    .struct_size = sizeof(sTypeObservationMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:type_observation_rng", .hooks = sTypeObservationHooks,
    .hook_count = ARRAY_COUNT(sTypeObservationHooks)
};
static void CheckTypeObservation(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sTypeObservationMod)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    const struct RhhTypeService *types = (const void *)RhhModsHostAPI()->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 15));
    CHECK(types && types->header.struct_size >= RHH_TYPE_SERVICE_V1_15_SIZE
        && RHH_TYPE_SERVICE_V1_14_SIZE < RHH_TYPE_SERVICE_V1_15_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 16)) == &types->header
        && RHH_TYPE_SERVICE_V1_15_SIZE < RHH_TYPE_SERVICE_V1_16_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 17)));
    struct RhhBattlerTypeObservation outside = {.struct_size = sizeof(outside), .changed = 99}, saved = outside;
    CHECK(types->battler_observation_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    RhhModsSetRunContext(123, 456, 0);
    struct BattleResources resources = {0}; struct AiLogicData ai = {0};
    struct AiPartyData parties = {0}; struct AiThinkingStruct thinking = {0};
    gBattleResources = &resources; gAiLogicData = &ai; gAiPartyData = &parties; gAiThinkingStruct = &thinking;
    PrepareTurnBattle(); sTypeObservationLocalCalls = 0;
    ModsBattleBegin();
    struct TypeObservationProbeState *probe = RhhModsState("test:type_observation");
    CHECK(probe && !probe->count);
    RhhContentKey fire = RhhContentKeyForId(RHH_TYPE, TYPE_FIRE);
    RhhContentKey flying = RhhContentKeyForId(RHH_TYPE, TYPE_FLYING);
    RhhContentKey water = RhhContentKeyForId(RHH_TYPE, TYPE_WATER);
    RhhContentKey grass = RhhContentKeyForId(RHH_TYPE, TYPE_GRASS);
    SET_BATTLER_TYPE(1, TYPE_WATER);
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    RhhModsObserveBattleTypes();
    CHECK(probe->count == 1 && sTypeObservationLocalCalls == 1
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && probe->last.changed == (RHH_BATTLER_TYPES_RAW_CHANGED | RHH_BATTLER_TYPES_EFFECTIVE_CHANGED)
        && probe->last.before_raw.count == 2 && probe->last.before_raw.types[0] == fire
        && probe->last.before_raw.types[1] == flying && probe->last.after_raw.count == 1
        && probe->last.after_raw.types[0] == water && probe->last.sequence == 0);
    RhhModsObserveBattleTypes(); CHECK(probe->count == 1);
    ai.aiCalcInProgress = TRUE; SET_BATTLER_TYPE(1, TYPE_ICE); RhhModsObserveBattleTypes();
    SET_BATTLER_TYPE(1, TYPE_WATER); ai.aiCalcInProgress = FALSE; RhhModsObserveBattleTypes(); CHECK(probe->count == 1);
    u32 tera = TYPE_DRAGON; SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE, &tera);
    SetActiveGimmick(1, GIMMICK_TERA); RhhModsObserveBattleTypes();
    CHECK(probe->count == 2 && probe->last.changed == RHH_BATTLER_TYPES_EFFECTIVE_CHANGED
        && probe->last.after_effective.count == 1
        && probe->last.after_effective.types[0] == RhhContentKeyForId(RHH_TYPE, TYPE_DRAGON));
    SET_BATTLER_TYPE(1, TYPE_GRASS); RhhModsObserveBattleTypes();
    CHECK(probe->count == 3 && probe->last.changed == RHH_BATTLER_TYPES_RAW_CHANGED
        && probe->last.after_raw.count == 1 && probe->last.after_raw.types[0] == grass
        && probe->last.before_effective.types[0] == probe->last.after_effective.types[0]);
    ModsBattleSwitchPrepared(1); SET_BATTLER_TYPE(1, TYPE_FIRE); RhhModsObserveBattleTypes();
    ModsBattleSwitchEntered(1); RhhModsObserveBattleTypes(); CHECK(probe->count == 3);
    SetActiveGimmick(1, GIMMICK_NONE); RhhModsObserveBattleTypes(); CHECK(probe->count == 4);
    gBattleTypeFlags |= BATTLE_TYPE_LINK; SET_BATTLER_TYPE(1, TYPE_WATER); RhhModsObserveBattleTypes();
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; RhhModsObserveBattleTypes(); CHECK(probe->count == 4);
    SET_BATTLER_TYPE(0, TYPE_GRASS); SET_BATTLER_TYPE(1, TYPE_ICE); RhhModsObserveBattleTypes();
    CHECK(probe->count == 6 && sTypeObservationLocalCalls == 6 && probe->last.sequence == 5
        && probe->last.after_participant.battler == 1);
    gBattleMons[1].extraTypeCount = 4;
    gBattleMons[1].extraTypes[0] = TYPE_FIRE; gBattleMons[1].extraTypes[1] = TYPE_FLYING;
    gBattleMons[1].extraTypes[2] = TYPE_GRASS; gBattleMons[1].extraTypes[3] = TYPE_GHOST;
    RhhModsObserveBattleTypes();
    CHECK(probe->count == 7 && probe->last.after_raw.count == 5
        && probe->last.after_effective.count == 5 && probe->last.after_raw.types[4] == RhhContentKeyForId(RHH_TYPE, TYPE_GHOST));
    SET_BATTLER_TYPE(0, TYPE_FLYING); RhhModsObserveBattleTypes(); CHECK(probe->count == 8);
    gBattleMons[0].volatiles.roostActive = TRUE; RhhModsObserveBattleTypes();
    CHECK(probe->count == 9 && probe->last.changed == RHH_BATTLER_TYPES_EFFECTIVE_CHANGED
        && probe->last.before_raw.types[0] == flying && probe->last.after_raw.types[0] == flying
        && probe->last.after_effective.count == 1 && probe->last.after_effective.types[0] == RhhContentKeyForId(RHH_TYPE, TYPE_NORMAL));
    gAbsentBattlerFlags |= 1u << 1; RhhModsObserveBattleTypes();
    SET_BATTLER_TYPE(1, TYPE_WATER); gAbsentBattlerFlags &= ~(1u << 1); RhhModsObserveBattleTypes();
    CHECK(probe->count == 9); /* Returning participants establish a new baseline. */
    CHECK(types->battler_observation_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gAiLogicData = NULL;
    gAiPartyData = NULL; gAiThinkingStruct = NULL; RhhModsUnloadLibraries();
    puts("type observation: raw, effective, simulation, switch, link and RNG passed");
}
