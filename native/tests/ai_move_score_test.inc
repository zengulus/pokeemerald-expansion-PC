#include "battle_ai_main.h"
#include "battle_ai_util.h"

/* The production helper is internal to battle_util.c. */
u32 GetMoveTargetCount(struct DamageContext *ctx);

static u32 sCalcPurityMode;
static const struct RhhAbilityService *sCalcPurityAbilities;
static void CalcPurityOffer(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .unlock_level = 1,
        .ability = RhhContentKeyOf("rhh:ability/torrent")};
    CHECK(!sCalcPurityAbilities->offer_innate(&offer));
}
static void CalcPurityTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->kind == RHH_MOVE_BASE_TYPE)
        decision->value = RhhContentKeyOf("rhh:type/fire");
    else if (event->kind == RHH_MOVE_POWER || event->kind == RHH_MOVE_ACCURACY)
        decision->value += 5;
    else
        decision->value += 2;
    if ((sCalcPurityMode == 1 && event->kind == RHH_MOVE_BASE_TYPE)
        || (sCalcPurityMode == 2 && (event->kind == RHH_MOVE_POWER || event->kind == RHH_MOVE_ACCURACY))
        || (sCalcPurityMode == 3 && (event->kind == RHH_INNATE_POWER || event->kind == RHH_INNATE_ACCURACY)
            && event->args[1] == RHH_INNATE_POWER_ATTACKER))
    { (void)Random32(); (void)Random2_32(); }
}
static const struct RhhModHook sCalcPurityHooks[] = {
    {sizeof(struct RhhModHook), RHH_INNATE_PROPOSE, RHH_CONSTRAINT, 0, CalcPurityOffer, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TYPE, RHH_TRANSFORM, 0, CalcPurityTransform, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_POWER, RHH_TRANSFORM, 0, CalcPurityTransform, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_ACCURACY, RHH_TRANSFORM, 0, CalcPurityTransform, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_POWER, RHH_TRANSFORM, 0, CalcPurityTransform, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_ACCURACY, RHH_TRANSFORM, 0, CalcPurityTransform, NULL}
};
static const struct RhhModDefinition sCalcPurityMod = {.struct_size = sizeof(sCalcPurityMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:calc_purity", .hooks = sCalcPurityHooks,
    .hook_count = ARRAY_COUNT(sCalcPurityHooks)};
static void CheckCalcPurity(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sCalcPurityMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sCalcPurityAbilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 3));
    CHECK(sCalcPurityAbilities);
    struct BattleResources resources = {0}; struct AiLogicData ai = {0};
    gBattleResources = &resources; gAiLogicData = &ai; PrepareTurnBattle();
    gBattleMons[0].ability = gBattleMons[1].ability = ABILITY_BLAZE;
    ai.abilities[0] = ai.abilities[1] = ABILITY_BLAZE; ModsBattleBegin();
    struct DamageContext ctx = {.battlerAtk = 0, .battlerDef = 1, .move = MOVE_WATER_GUN,
        .chosenMove = MOVE_WATER_GUN, .baseMove = MOVE_WATER_GUN, .moveType = TYPE_WATER};
    struct BattleCalcValues cv = {.battlerAtk = 0, .battlerDef = 1, .move = MOVE_WATER_GUN};
    ctx.abilities[0] = ctx.abilities[1] = cv.abilities[0] = cv.abilities[1] = ABILITY_BLAZE;
    for (u32 mode = 0; mode <= 3; ++mode)
    {
        sCalcPurityMode = mode;
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        CHECK(GetMoveType(MOVE_WATER_GUN) == (mode == 1 ? TYPE_WATER : TYPE_FIRE));
        u32 expected = mode == 2 ? 44 : mode == 3 ? 47 : 49;
        for (u32 isAi = 0; isAi <= 1; ++isAi)
        {
            ctx.aiCalc = isAi;
            CHECK(ModsMovePower(&ctx, 40) == expected);
            CHECK(ModsMoveAccuracy(&cv, B_WEATHER_NONE, 40, isAi) == expected);
        }
        CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    }
    ModsBattleReset(); gMain.inBattle = FALSE; gAiLogicData = NULL; RhhModsUnloadLibraries();
    puts("move calculation purity: type, power, accuracy and innates passed");
}

static bool32 sTargetContextUseRng;
static void TargetContextRng(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sTargetContextUseRng) { (void)Random32(); (void)Random2_32(); decision->value = RHH_MOVE_TARGET_BOTH; }
}
static const struct RhhModHook sTargetContextRngHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_TARGET_RESOLVE, RHH_TRANSFORM, 0, TargetContextRng, NULL}
};
static const struct RhhModDefinition sTargetContextRngMod = {.struct_size = sizeof(struct RhhModDefinition),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:target_context_rng", .hook_count = 1, .hooks = sTargetContextRngHooks};

static u32 sBaseMovePurityMode;
static void BaseMovePurityTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sBaseMovePurityMode) return;
    if (event->kind == RHH_MOVE_BASE_TARGET && event->event_key == RhhContentKeyOf("rhh:move/water_gun"))
        decision->value = RHH_MOVE_TARGET_BOTH;
    else if (event->kind == RHH_MOVE_EFFECT_MODE && event->event_key == RhhContentKeyOf("rhh:move/splash"))
        decision->value = RHH_MOVE_EFFECT_INHERITED;
    else if (event->kind == RHH_MOVE_EFFECT_MODE && event->event_key == RhhContentKeyOf("rhh:move/teleport"))
        decision->value = RHH_MOVE_EFFECT_CUSTOM_STATUS;
    else if (event->kind == RHH_MOVE_DAMAGE_EFFECT_MODE && event->event_key == RhhContentKeyOf("rhh:move/tackle"))
        decision->value = RHH_MOVE_EFFECT_CUSTOM_DAMAGE;
    else return;
    if (sBaseMovePurityMode >= 2) { (void)Random32(); (void)Random2_32(); }
}
static void BaseMovePurityConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sBaseMovePurityMode == 3) decision->allowed = 0;
}
static const struct RhhModHook sBaseMovePurityHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TARGET, RHH_TRANSFORM, 0, BaseMovePurityTransform, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_EFFECT_MODE, RHH_TRANSFORM, 0, BaseMovePurityTransform, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_DAMAGE_EFFECT_MODE, RHH_TRANSFORM, 0, BaseMovePurityTransform, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_EFFECT_MODE, RHH_CONSTRAINT, 0, BaseMovePurityConstraint, NULL}
};
static const struct RhhModDefinition sBaseMovePurityMod = {.struct_size = sizeof(sBaseMovePurityMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:base_move_purity", .hook_count = ARRAY_COUNT(sBaseMovePurityHooks),
    .hooks = sBaseMovePurityHooks};
static void CheckBaseMovePurity(const char *provider, const char *consumer)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sBaseMovePurityMod)
        && !RhhModsLoadLibrary(provider) && !RhhModsLoadLibrary(consumer) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 16));
    RhhContentKey splash = RhhContentKeyOf("rhh:move/splash"), teleport = RhhContentKeyOf("rhh:move/teleport"),
        tackle = RhhContentKeyOf("rhh:move/tackle"), water = RhhContentKeyOf("rhh:move/water_gun");
    struct RhhMoveInfo info = {.struct_size = sizeof(info)};
    RhhContentKey bound = 0;
    u32 status = 0, damage = 0;
    CHECK(moves && !moves->registered_effect_for_move(splash, &bound) && bound
        && !moves->effect_mode(splash, &status) && status == RHH_MOVE_EFFECT_CUSTOM_STATUS);
    for (u32 policy = 1; policy <= 3; ++policy)
    {
        sBaseMovePurityMode = policy;
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        u32 expectedTarget = policy == 1 ? RHH_MOVE_TARGET_BOTH : RHH_MOVE_TARGET_SELECTED;
        u32 expectedStatus = policy == 2 ? RHH_MOVE_EFFECT_CUSTOM_STATUS : RHH_MOVE_EFFECT_INHERITED;
        u32 expectedOther = policy == 1 ? RHH_MOVE_EFFECT_CUSTOM_STATUS : RHH_MOVE_EFFECT_INHERITED;
        u32 expectedDamage = policy == 1 ? RHH_MOVE_EFFECT_CUSTOM_DAMAGE : RHH_MOVE_EFFECT_INHERITED;
        CHECK(!moves->info(water, &info) && info.target == expectedTarget);
        CHECK(GetMoveTarget(MOVE_WATER_GUN) == (policy == 1 ? TARGET_BOTH : TARGET_SELECTED));
        CHECK(!moves->effect_mode(splash, &status) && status == expectedStatus);
        CHECK(!moves->effect_mode(teleport, &status) && status == expectedOther);
        CHECK(!moves->effect_mode(tackle, &damage) && damage == expectedDamage);
        CHECK(GetMoveEffect(MOVE_SPLASH) == (expectedStatus == RHH_MOVE_EFFECT_CUSTOM_STATUS
            ? EFFECT_MOD_CUSTOM_STATUS : gMovesInfo[MOVE_SPLASH].effect));
        CHECK(GetMoveEffect(MOVE_TELEPORT) == (expectedOther == RHH_MOVE_EFFECT_CUSTOM_STATUS
            ? EFFECT_MOD_CUSTOM_STATUS : gMovesInfo[MOVE_TELEPORT].effect));
        CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    }
    sBaseMovePurityMode = 0; RhhModsUnloadLibraries();
    puts("move base purity: target and effect proposals preserve RNG and bindings");
}

static void CheckMoveTargetContext(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sTargetContextRngMod)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct AiLogicData ai = {0}; struct AiPartyData parties = {0};
    struct AiThinkingStruct thinking = {0}; struct AiBattleData decisions = {0}; struct BattleHistory history = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources; gAiLogicData = &ai; gAiPartyData = &parties;
    gAiThinkingStruct = &thinking; gAiBattleData = &decisions; gBattleHistory = &history;
    PrepareTurnBattle();
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 43));
    struct RhhMoveTargetContext outside = {.struct_size = sizeof(outside), .battler = 999}, unchanged = outside;
    CHECK(moves && moves->header.struct_size >= RHH_MOVE_SERVICE_V1_43_SIZE
        && RHH_MOVE_SERVICE_V1_41_SIZE < RHH_MOVE_SERVICE_V1_42_SIZE
        && RHH_MOVE_SERVICE_V1_42_SIZE < RHH_MOVE_SERVICE_V1_43_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 33)) == &moves->header
        && RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 42)) == &moves->header
        && !RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 55))
        && moves->target_lock() && moves->target_context(&outside) && !memcmp(&outside, &unchanged, sizeof(outside)));
    u8 *mode = RhhModsState("test:target_context"); CHECK(mode); *mode = 0;
    CHECK(GetBattlerMoveSelectionTargetType(1, MOVE_WATER_GUN) == TARGET_SELECTED);
    struct RhhMoveTargetPreview inactive = {.struct_size = sizeof(inactive), .target = 999}, savedInactive = inactive;
    CHECK(moves->target_preview(1, RhhContentKeyOf("rhh:move/water_gun"), RHH_MOVE_TARGET_SELECTION, &inactive)
        && !memcmp(&inactive, &savedInactive, sizeof(inactive)));
    ModsBattleBegin();
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(GetMoveTarget(MOVE_WATER_GUN) == TARGET_SELECTED
        && GetBattlerMoveSelectionTargetType(0, MOVE_WATER_GUN) == TARGET_SELECTED
        && GetBattlerMoveSelectionTargetType(1, MOVE_WATER_GUN) == TARGET_BOTH
        && GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == TARGET_BOTH
        && AI_GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == TARGET_BOTH);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    RhhContentKey waterKey = RhhContentKeyOf("rhh:move/water_gun");
    struct RhhMoveTargetPreview preview = {.struct_size = sizeof(preview)},
        tinyPreview = {.struct_size = 1, .target = 999}, savedTinyPreview = tinyPreview;
    struct BattleStruct savedBattle = *gBattleStruct;
    struct BattlePokemon savedMons[MAX_BATTLERS_COUNT]; memcpy(savedMons, gBattleMons, sizeof(savedMons));
    enum Ability savedAbility = gLastUsedAbility; enum Item savedItem = gLastUsedItem;
    enum BattlerId savedPotential = gPotentialItemEffectBattler;
    gLastUsedAbility = ABILITY_BLAZE; gLastUsedItem = ITEM_POTION; gPotentialItemEffectBattler = 3;
    CHECK(moves->target_preview(1, waterKey, RHH_MOVE_TARGET_SELECTION, &tinyPreview)
        && !memcmp(&tinyPreview, &savedTinyPreview, sizeof(tinyPreview)));
    for (u32 phase = RHH_MOVE_TARGET_SELECTION; phase <= RHH_MOVE_TARGET_AI; ++phase)
        CHECK(!moves->target_preview(1, waterKey, phase, &preview) && preview.target == RHH_MOVE_TARGET_BOTH
            && preview.battler == 1 && preview.move == waterKey && preview.phase == phase
            && preview.run_id == 456 && preview.battle_id);
    struct RhhMoveTargetPreview savedPreview = preview;
    CHECK(moves->target_preview(1, waterKey, 0, &preview)
        && moves->target_preview(99, waterKey, RHH_MOVE_TARGET_SELECTION, &preview)
        && moves->target_preview(1, RhhContentKeyOf("test:move/missing"), RHH_MOVE_TARGET_SELECTION, &preview)
        && !memcmp(&preview, &savedPreview, sizeof(preview)));
    gAiLogicData = NULL; CHECK(moves->target_preview(1, waterKey, RHH_MOVE_TARGET_AI, &preview)
        && !memcmp(&preview, &savedPreview, sizeof(preview))); gAiLogicData = &ai;
    CHECK(!memcmp(&savedBattle, gBattleStruct, sizeof(savedBattle)) && !memcmp(savedMons, gBattleMons, sizeof(savedMons))
        && gLastUsedAbility == ABILITY_BLAZE && gLastUsedItem == ITEM_POTION && gPotentialItemEffectBattler == 3
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gLastUsedAbility = savedAbility; gLastUsedItem = savedItem; gPotentialItemEffectBattler = savedPotential;
    struct RhhMoveTargetFilter filter = {.struct_size = sizeof(filter)};
    RhhContentKey pollenKey = RhhContentKeyOf("rhh:move/pollen_puff");
    CHECK(!moves->target_filter(1, pollenKey, 3, &filter) && filter.allowed
        && filter.battler == 1 && filter.candidate == 3 && filter.move == pollenKey
        && filter.run_id == 456 && filter.battle_id);
    gBattleMons[1].volatiles.healBlockTimer = 1;
    CHECK(!moves->target_filter(1, pollenKey, 3, &filter) && !filter.allowed
        && !moves->target_filter(1, pollenKey, 0, &filter) && filter.allowed);
    gBattleMons[1].volatiles.healBlockTimer = 0;
    gBattleStruct->gimmick.usableGimmick[1] = GIMMICK_DYNAMAX; gBattleStruct->gimmick.playerSelect = TRUE;
    CHECK(!moves->target_filter(1, waterKey, 3, &filter) && !filter.allowed
        && !moves->target_filter(1, waterKey, 0, &filter) && filter.allowed);
    gBattleStruct->gimmick.usableGimmick[1] = GIMMICK_NONE; gBattleStruct->gimmick.playerSelect = FALSE;
    u32 candidateHp = gBattleMons[3].hp; gBattleMons[3].hp = 0;
    CHECK(!moves->target_filter(1, waterKey, 3, &filter) && !filter.allowed);
    gBattleMons[3].hp = candidateHp;
    struct RhhMoveTargetFilter savedFilter = filter, tinyFilter = {.struct_size = 1}, oldTinyFilter = tinyFilter;
    CHECK(moves->target_filter(1, waterKey, 99, &filter)
        && moves->target_filter(1, RhhContentKeyOf("test:move/missing"), 0, &filter)
        && !memcmp(&filter, &savedFilter, sizeof(filter))
        && moves->target_filter(1, waterKey, 0, &tinyFilter) && !memcmp(&tinyFilter, &oldTinyFilter, sizeof(tinyFilter))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    for (u32 i = 1; i <= 5; ++i)
    {
        *mode = i;
        u32 expected = i == 3 ? TARGET_BOTH : TARGET_SELECTED;
        CHECK(GetBattlerMoveSelectionTargetType(1, MOVE_WATER_GUN) == expected
            && GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == expected
            && AI_GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == expected);
    }
    *mode = 6; gBattleWeather = B_WEATHER_RAIN_NORMAL;
    CHECK(GetBattlerMoveSelectionTargetType(0, MOVE_WATER_GUN) == TARGET_BOTH
        && GetBattlerMoveTargetType(0, MOVE_WATER_GUN) == TARGET_BOTH
        && AI_GetBattlerMoveTargetType(0, MOVE_WATER_GUN) == TARGET_BOTH);
    gBattleWeather = B_WEATHER_NONE; CHECK(GetBattlerMoveTargetType(0, MOVE_WATER_GUN) == TARGET_SELECTED);
    *mode = 7; gFieldTimers.terrain = B_TERRAIN_PSYCHIC;
    CHECK(GetBattlerMoveSelectionTargetType(0, MOVE_WATER_GUN) == TARGET_BOTH
        && GetBattlerMoveTargetType(0, MOVE_WATER_GUN) == TARGET_BOTH
        && AI_GetBattlerMoveTargetType(0, MOVE_WATER_GUN) == TARGET_BOTH);
    gFieldTimers.terrain = B_TERRAIN_NONE; CHECK(AI_GetBattlerMoveTargetType(0, MOVE_WATER_GUN) == TARGET_SELECTED);
    *mode = 0;
    gBattleStruct->moveTarget[3] = 2;
    CHECK(DoesPartnerHaveSameMoveEffect(3, 0, MOVE_WATER_GUN, MOVE_SCRATCH));
    *mode = 1;
    CHECK(!DoesPartnerHaveSameMoveEffect(3, 0, MOVE_WATER_GUN, MOVE_SCRATCH));
    gBattleStruct->moveTarget[3] = 0;
    CHECK(DoesPartnerHaveSameMoveEffect(3, 0, MOVE_WATER_GUN, MOVE_SCRATCH));
    *mode = 0;
    gBattleMons[1].types[0] = TYPE_NORMAL; gBattleMons[1].types[1] = gBattleMons[1].types[2] = TYPE_MYSTERY;
    gBattleMons[1].ability = ABILITY_LEVITATE; ai.abilities[1] = ABILITY_NONE; gFieldTimers.terrain = B_TERRAIN_PSYCHIC;
    CHECK(GetBattlerMoveSelectionTargetType(1, MOVE_EXPANDING_FORCE) == TARGET_SELECTED
        && GetBattlerMoveTargetType(1, MOVE_EXPANDING_FORCE) == TARGET_SELECTED
        && AI_GetBattlerMoveTargetType(1, MOVE_EXPANDING_FORCE) == TARGET_BOTH);
    RhhContentKey expandingKey = RhhContentKeyOf("rhh:move/expanding_force");
    CHECK(!moves->target_preview(1, expandingKey, RHH_MOVE_TARGET_EXECUTION, &preview)
        && preview.target == RHH_MOVE_TARGET_SELECTED
        && !moves->target_preview(1, expandingKey, RHH_MOVE_TARGET_AI, &preview)
        && preview.target == RHH_MOVE_TARGET_BOTH);
    gFieldTimers.terrain = B_TERRAIN_NONE;
    rng = gRngValue; rng2 = gRng2Value; sTargetContextUseRng = TRUE;
    CHECK(GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == TARGET_SELECTED
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sTargetContextUseRng = FALSE;
    gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(GetBattlerMoveSelectionTargetType(1, MOVE_WATER_GUN) == TARGET_SELECTED
        && GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == TARGET_SELECTED
        && AI_GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == TARGET_SELECTED);
    savedPreview = preview;
    CHECK(moves->target_preview(1, waterKey, RHH_MOVE_TARGET_SELECTION, &preview)
        && !memcmp(&preview, &savedPreview, sizeof(preview)));
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; gBattleTypeFlags |= BATTLE_TYPE_RECORDED;
    CHECK(GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == TARGET_SELECTED); gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED;
    u32 savedHp = gBattleMons[1].hp; gBattleMons[1].hp = 0;
    CHECK(moves->target_preview(1, waterKey, RHH_MOVE_TARGET_SELECTION, &preview)
        && !memcmp(&preview, &savedPreview, sizeof(preview))); gBattleMons[1].hp = savedHp;
    CHECK(GetBattlerMoveTargetType(1, MOVE_STRUGGLE) == GetMoveTarget(MOVE_STRUGGLE));
    CHECK(moves->target_context(&outside) && !memcmp(&outside, &unchanged, sizeof(outside)));
    /* Invalid, denied and RNG-consuming rules cannot commit a lock. */
    gBattlerAttacker = 1; gCurrentMove = MOVE_WATER_GUN;
    *mode = 0; ModsMoveTargetCaptureBegin(1, MOVE_WATER_GUN);
    CHECK(!moves->target_preview(1, waterKey, RHH_MOVE_TARGET_EXECUTION, &preview)
        && preview.target == RHH_MOVE_TARGET_BOTH);
    ModsMoveTargetCaptureEnd(1, MOVE_WATER_GUN, TARGET_BOTH);
    *mode = 1; CHECK(GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == TARGET_SELECTED);
    for (u32 invalid = 1; invalid <= 2; ++invalid)
    {
        *mode = invalid; ModsMoveTargetCaptureBegin(1, MOVE_WATER_GUN);
        u32 initial = GetBattlerMoveTargetType(1, MOVE_WATER_GUN);
        ModsMoveTargetCaptureEnd(1, MOVE_WATER_GUN, initial);
        *mode = 0; CHECK(initial == TARGET_SELECTED && GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == TARGET_BOTH);
    }
    sTargetContextUseRng = TRUE; rng = gRngValue; rng2 = gRng2Value;
    ModsMoveTargetCaptureBegin(1, MOVE_WATER_GUN);
    u32 initial = GetBattlerMoveTargetType(1, MOVE_WATER_GUN);
    ModsMoveTargetCaptureEnd(1, MOVE_WATER_GUN, initial); sTargetContextUseRng = FALSE;
    CHECK(initial == TARGET_SELECTED && GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == TARGET_BOTH
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    /* Real target setup and move-end iteration honor the contextual spread. */
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].moves[0] = MOVE_WATER_GUN; gBattleMons[1].pp[0] = 20;
    struct DamageContext damage = {.battlerAtk = 1, .battlerDef = 0, .move = MOVE_WATER_GUN};
    CHECK(GetMoveTargetCount(&damage) == 2);
    gBattlerAttacker = 1; gBattlerTarget = 0; gCurrentMove = gChosenMove = MOVE_WATER_GUN; gCurrMovePos = 0;
    gBattleStruct->eventState.atkCanceler = CANCELER_SET_TARGETS;
    CHECK(DoAttackCanceler() == CANCELER_RESULT_RUN_SCRIPT_AND_INCREMENT && gBattlerTarget == 0
        && !gBattleStruct->battlerState[1].targetsDone[0] && gBattleStruct->battlerState[1].targetsDone[1]
        && !gBattleStruct->battlerState[1].targetsDone[2] && gBattleStruct->battlerState[1].targetsDone[3]);
    gBattleScripting.moveendState = MOVEEND_NEXT_TARGET;
    CHECK(DoMoveEnd(0, MOVEEND_NEXT_TARGET) == MOVEEND_RESULT_BREAK && gBattlerTarget == 2 && scripts.size == 1);
    ModsBattleReset(); CHECK(GetBattlerMoveTargetType(1, MOVE_WATER_GUN) == TARGET_SELECTED);
    gMain.inBattle = FALSE; gAiLogicData = NULL; gAiPartyData = NULL; gAiThinkingStruct = NULL;
    gAiBattleData = NULL; gBattleHistory = NULL; RhhModsUnloadLibraries();
    puts("move target context: selection, execution, AI, purity and fallback passed");
}

static void CheckAiMoveScore(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    struct BattleResources resources = {0}; struct AiLogicData ai = {0}; struct AiPartyData parties = {0};
    struct AiThinkingStruct thinking = {0}; struct AiBattleData decisions = {0}; struct BattleHistory history = {0};
    gBattleResources = &resources; gAiLogicData = &ai; gAiPartyData = &parties;
    gAiThinkingStruct = &thinking; gAiBattleData = &decisions; gBattleHistory = &history;
    PrepareTurnBattle(); gBattleTypeFlags = BATTLE_TYPE_TRAINER; gBattlersCount = 2;
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 20));
    struct RhhAiMoveScoreContext outside = {.struct_size = sizeof(outside), .score = 999}, saved = outside;
    CHECK(moves && moves->header.struct_size >= RHH_MOVE_SERVICE_V1_9_SIZE);
    CHECK(moves->ai_score_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    struct RhhAiMoveEvaluation evalOutside = {.struct_size = sizeof(evalOutside), .damage_median = 999}, evalSaved = evalOutside;
    CHECK(moves->ai_evaluation(&evalOutside) && !memcmp(&evalOutside, &evalSaved, sizeof(evalOutside)));
    u32 custom; CHECK(!RhhModsMoveSlot(RhhContentKeyOf("test:move/ai_probe"), TRUE, &custom));
    gBattleMons[1].moves[0] = MOVE_TACKLE; gBattleMons[1].moves[1] = MOVE_SCRATCH;
    gBattleMons[1].moves[2] = MOVE_WATER_GUN; gBattleMons[1].moves[3] = MOVE_GROWL;
    for (u32 i = 0; i < 4; ++i) gBattleMons[1].pp[i] = 20;
    ai.hpPercents[1] = 42; ai.hpPercents[0] = ai.hpPercents[2] = 70;
    for (u32 candidate = 0; candidate <= 2; candidate += 2)
    {
        ai.simulatedDmg[1][candidate][1] = (struct SimulatedDamage){17, 19, 22, 20};
        ai.moveAccuracy[1][candidate][1] = 87; ai.effectiveness[1][candidate][1] = UQ_4_12(1.0);
    }
    gBattleMons[0].ability = ABILITY_LIQUID_OOZE; ai.abilities[0] = ABILITY_NONE;
    ModsBattleBegin();
    rng_value_t rng = gRngValue;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 1, 100) == 201 && !memcmp(&rng, &gRngValue, sizeof(rng)));
    ai.abilities[0] = ABILITY_LIQUID_OOZE;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 1, 100) == 70);
    BattleAI_ChooseMoveIndex(1); CHECK(decisions.finalScore[1][0][1] == 70); ai.abilities[0] = ABILITY_NONE;
    ai.simulatedDmg[1][0][1].median = 0;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 1, 100) == 80); ai.simulatedDmg[1][0][1].median = 19;
    gBattleMons[1].volatiles.healBlockTimer = 1;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 1, 100) == 70); gBattleMons[1].volatiles.healBlockTimer = 0;
    gBattleMons[0].volatiles.substitute = TRUE;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 1, 100) == 80); gBattleMons[0].volatiles.substitute = FALSE;
    ai.partnerMoveSimulation = TRUE;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 1, 100) == 75); ai.partnerMoveSimulation = FALSE;
    CHECK(BattleAI_ChooseMoveIndex(1) == 1);
    CHECK(decisions.finalScore[1][0][0] == 100 && decisions.finalScore[1][0][1] == 201
        && decisions.finalScore[1][0][2] == 100 && decisions.finalScore[1][0][3] == 100);
    CHECK(moves->ai_score_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    ai.moveLimitations[1] = 1u << 1; BattleAI_ChooseMoveIndex(1);
    CHECK(decisions.finalScore[1][0][1] == 0); ai.moveLimitations[1] = 0;
    gBattleMons[1].pp[1] = 0; BattleAI_ChooseMoveIndex(1);
    CHECK(decisions.finalScore[1][0][1] == 100); gBattleMons[1].pp[1] = 20;
    gBattleMons[1].moves[3] = custom; CHECK(BattleAI_ChooseMoveIndex(1) == 3);
    CHECK(decisions.finalScore[1][0][3] == 220);
    gBattleTypeFlags |= BATTLE_TYPE_LINK; BattleAI_ChooseMoveIndex(1);
    CHECK(decisions.finalScore[1][0][1] == 100 && decisions.finalScore[1][0][3] == 100);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    gBattleTypeFlags |= BATTLE_TYPE_RECORDED; BattleAI_ChooseMoveIndex(1);
    CHECK(decisions.finalScore[1][0][1] == 100 && decisions.finalScore[1][0][3] == 100);
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE; gBattlersCount = 4; gBattleMons[3].hp = 0;
    CHECK(BattleAI_ChooseMoveIndex(1) == 3);
    CHECK(decisions.finalScore[1][0][3] == 220 && decisions.finalScore[1][2][3] == 220);
    /* The AI's known ability can differ from the real ability. Its target
     * context must use the same knowledge as the built-in AI, not leak it. */
    gBattleTypeFlags = BATTLE_TYPE_TRAINER; gBattlersCount = 2;
    gBattleMons[1].types[0] = TYPE_NORMAL; gBattleMons[1].types[1] = TYPE_MYSTERY;
    gBattleMons[1].types[2] = TYPE_MYSTERY; gBattleMons[1].ability = ABILITY_LEVITATE;
    ai.abilities[1] = ABILITY_NONE; gFieldTimers.terrain = B_TERRAIN_PSYCHIC;
    CHECK(GetBattlerMoveTargetType(1, MOVE_EXPANDING_FORCE) == TARGET_SELECTED);
    CHECK(AI_GetBattlerMoveTargetType(1, MOVE_EXPANDING_FORCE) == TARGET_BOTH);
    gBattleMons[1].moves[0] = MOVE_EXPANDING_FORCE; BattleAI_ChooseMoveIndex(1);
    CHECK(decisions.finalScore[1][0][0] == 100); gFieldTimers.terrain = B_TERRAIN_NONE;
    ModsBattleReset(); gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("AI score: real choices, context, bounds and exclusions passed");
}

