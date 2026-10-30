#include "battle_hold_effects.h"
static const struct RhhBattleService *sStatusBattle;
static struct RhhBattleStatusApplication sStatusEvents[32];
static struct RhhBattleStatusAttribution sStatusAttributions[32];
static u32 sStatusCount;
static struct RhhBattleItemStatusChange sItemStatusEvents[32];
static u32 sItemStatusCount;
static rng_value_t sStatusObserverRng, sStatusObserverRng2;
static void ObserveStatus(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleStatusApplication change = {.struct_size = sizeof(change)}, small = {.struct_size = 1, .sequence = 77}, saved = small;
    struct RhhBattleStatusAttribution attribution = {.struct_size = sizeof(attribution)}, tiny = {.struct_size = 1, .sequence = 7}, tinySaved = tiny;
    CHECK(sStatusCount < ARRAY_COUNT(sStatusEvents) && !sStatusBattle->status_application_context(&change)
        && !sStatusBattle->status_attribution(&attribution) && sStatusBattle->status_attribution(&tiny)
        && !memcmp(&tiny, &tinySaved, sizeof(tiny))
        && sStatusBattle->status_application_context(&small) && !memcmp(&small, &saved, sizeof(small))
        && change.sequence == event->ordinal && change.target_battler == event->args[0]
        && change.status == event->args[1] && change.trigger == event->args[2] && change.source_battler == event->args[3]
        && change.move == event->event_key && change.after_raw == gBattleMons[change.target_battler].status1
        && !RhhModsCommandsAllowed());
    sStatusEvents[sStatusCount] = change; sStatusAttributions[sStatusCount++] = attribution;
    sStatusObserverRng = gRngValue; sStatusObserverRng2 = gRng2Value; (void)Random(); (void)Random2();
}
static void ObserveItemStatus(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleItemStatusChange change = {.struct_size = sizeof(change)}, small = {.struct_size = 1, .sequence = 99}, saved = small;
    CHECK(sItemStatusCount < ARRAY_COUNT(sItemStatusEvents) && !sStatusBattle->item_status_context(&change)
        && sStatusBattle->item_status_context(&small) && !memcmp(&small, &saved, sizeof(small))
        && change.sequence == event->ordinal && change.battler == event->args[0]
        && change.added == event->args[1] && change.removed == event->args[2] && change.origin == event->args[3]
        && change.item == event->event_key && change.after_raw == gBattleMons[change.battler].status1
        && !RhhModsCommandsAllowed());
    sItemStatusEvents[sItemStatusCount++] = change;
    sStatusObserverRng = gRngValue; sStatusObserverRng2 = gRng2Value; (void)Random(); (void)Random2();
}
static const struct RhhModHook sStatusHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLER_STATUS_APPLIED, RHH_OBSERVE, 0, NULL, ObserveStatus},
    {sizeof(struct RhhModHook), RHH_BATTLER_ITEM_STATUS_CHANGED, RHH_OBSERVE, 0, NULL, ObserveItemStatus}
};
static const struct RhhModDefinition sStatusMod = {.struct_size = sizeof(sStatusMod), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:battle_status", .hooks = sStatusHooks, .hook_count = 2};
static void ApplyStatusForTest(u32 source, u32 target, u32 effect, u32 trigger)
{
    static const u8 script[] = {B_SCR_OP_END};
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = script;
    SetNonVolatileStatus(source, target, effect, script, trigger);
}
static enum ItemEffect ApplyStatusItemForTest(u32 battler, enum HoldEffect effect, ActivationTiming timing)
{
    static const u8 script[] = {B_SCR_OP_END};
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = script;
    return ItemBattleEffects(battler, 0, effect, timing);
}
static void CheckBattleStatus(const char *innate, const char *probe)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sStatusMod)
        && !RhhModsLoadLibrary(innate) && !RhhModsLoadLibrary(probe) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0); sStatusCount = sItemStatusCount = 0;
    sStatusBattle = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 19));
    CHECK(sStatusBattle && sStatusBattle->header.struct_size >= RHH_BATTLE_SERVICE_V1_19_SIZE
        && RHH_BATTLE_SERVICE_V1_17_SIZE < RHH_BATTLE_SERVICE_V1_18_SIZE
        && RHH_BATTLE_SERVICE_V1_18_SIZE < RHH_BATTLE_SERVICE_V1_19_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 17)) == &sStatusBattle->header
        && RHH_BATTLE_SERVICE_V1_20_SIZE < RHH_BATTLE_SERVICE_V1_21_SIZE
        && RHH_BATTLE_SERVICE_V1_22_SIZE < RHH_BATTLE_SERVICE_V1_23_SIZE
        && RHH_BATTLE_SERVICE_V1_23_SIZE < RHH_BATTLE_SERVICE_V1_24_SIZE
        && RHH_BATTLE_SERVICE_V1_24_SIZE < RHH_BATTLE_SERVICE_V1_25_SIZE
        && RHH_BATTLE_SERVICE_V1_25_SIZE < RHH_BATTLE_SERVICE_V1_26_SIZE
        && RHH_BATTLE_SERVICE_V1_26_SIZE < RHH_BATTLE_SERVICE_V1_27_SIZE
        && RHH_BATTLE_SERVICE_V1_27_SIZE < RHH_BATTLE_SERVICE_V1_28_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 21)) == &sStatusBattle->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 23)) == &sStatusBattle->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 24)) == &sStatusBattle->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 25)) == &sStatusBattle->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 26)) == &sStatusBattle->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 27)) == &sStatusBattle->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 28)) == &sStatusBattle->header
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 29)) == &sStatusBattle->header
        && !RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 46)));
    struct RhhBattleStatusApplication outside = {.struct_size = sizeof(outside), .sequence = 99}, saved = outside;
    CHECK(sStatusBattle->status_application_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    struct RhhBattleStatusAttribution outsideAttribution = {.struct_size = sizeof(outsideAttribution), .sequence = 77}, savedAttribution = outsideAttribution;
    CHECK(sStatusBattle->status_attribution(&outsideAttribution) && !memcmp(&outsideAttribution, &savedAttribution, sizeof(outsideAttribution)));
    struct RhhBattleItemStatusChange outsideItem = {.struct_size = sizeof(outsideItem), .sequence = 99}, savedItem = outsideItem;
    CHECK(sStatusBattle->item_status_context(&outsideItem) && !memcmp(&outsideItem, &savedItem, sizeof(outsideItem)));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct BattleSpriteInfo spriteInfo[MAX_BATTLERS_COUNT] = {0}; struct BattleSpriteData spriteData = {.battlerData = spriteInfo};
    struct AiLogicData ai = {0}; struct AiPartyData parties = {0}; struct AiThinkingStruct thinking = {0}; struct BattleHistory history = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources; gBattleSpritesDataPtr = &spriteData;
    gAiLogicData = &ai; gAiPartyData = &parties; gAiThinkingStruct = &thinking; gBattleHistory = &history;
    PrepareTurnBattle(); gBattleMons[2].level = 9; gBattleMons[3].hp = 0;
    for (u32 i = 0; i < MAX_BATTLERS_COUNT; ++i) gBattlerControllerEndFuncs[i] = EntryHealControllerComplete;
    gBattleMons[0].ability = gBattleMons[1].ability = ABILITY_NONE;
    gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_WATER; gBattleMons[1].types[2] = TYPE_MYSTERY;
    gBattlerAttacker = 0; gBattlerTarget = 1; gCurrentMove = MOVE_WILL_O_WISP; ModsBattleBegin();
    const u32 effects[] = {MOVE_EFFECT_SLEEP, MOVE_EFFECT_POISON, MOVE_EFFECT_BURN, MOVE_EFFECT_FREEZE,
        MOVE_EFFECT_PARALYSIS, MOVE_EFFECT_TOXIC, MOVE_EFFECT_FROSTBITE};
    for (u32 i = 0; i < ARRAY_COUNT(effects); ++i)
    {
        gBattleMons[1].status1 = 0; ApplyStatusForTest(0, 1, effects[i], TRIGGER_ON_MOVE);
        CHECK(sStatusCount == i + 1 && sStatusEvents[i].status == i + RHH_BATTLE_STATUS_SLEEP
            && sStatusEvents[i].trigger == RHH_BATTLE_STATUS_FROM_MOVE && !sStatusEvents[i].before_raw
            && sStatusEvents[i].move == RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP)
            && sStatusEvents[i].flags == (RHH_BATTLE_STATUS_SOURCE_AVAILABLE | RHH_BATTLE_STATUS_TARGET_AVAILABLE)
            && sStatusEvents[i].source.battler == 0 && sStatusEvents[i].target.battler == 1
            && !memcmp(&sStatusObserverRng, &gRngValue, sizeof(gRngValue))
            && !memcmp(&sStatusObserverRng2, &gRng2Value, sizeof(gRng2Value)));
    }
    const u8 command[] = {B_SCR_OP_SETNONVOLATILESTATUS, TRIGGER_ON_MOVE};
    gBattleMons[1].status1 = 0; gBattleResources->battleScriptsStack->size = 0;
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = command;
    gBattleScriptingCommandsTable[B_SCR_OP_SETNONVOLATILESTATUS]();
    CHECK(sStatusCount == 8 && sStatusEvents[7].status == RHH_BATTLE_STATUS_BURN
        && sStatusEvents[7].move == RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP));
    gBattleMons[1].status1 = STATUS1_BURN; ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE); CHECK(sStatusCount == 8);
    gBattleMons[1].status1 = 0; ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_PROTECT);
    CHECK(sStatusCount == 9 && sStatusEvents[8].trigger == RHH_BATTLE_STATUS_FROM_PROTECT && !sStatusEvents[8].move);
    gBattleMons[1].status1 = 0; gBattleTypeFlags |= BATTLE_TYPE_LINK;
    gBattleMons[1].status1 = STATUS1_BURN; ModsBattleStatusApplied(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE, 0, FALSE); CHECK(sStatusCount == 9);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; gBattleTypeFlags |= BATTLE_TYPE_RECORDED; gBattleMons[1].status1 = 0;
    gBattleMons[1].status1 = STATUS1_BURN; ModsBattleStatusApplied(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE, 0, FALSE);
    CHECK(sStatusCount == 9); gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED;
    gBattleMons[1].status1 = 0; ModsBattleSwitchPrepared(1); ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_ABILITY);
    CHECK(sStatusCount == 10 && sStatusEvents[9].trigger == RHH_BATTLE_STATUS_FROM_ABILITY
        && sStatusEvents[9].flags == RHH_BATTLE_STATUS_SOURCE_AVAILABLE && !sStatusEvents[9].target.identity);
    ModsBattleSwitchEntered(1); gBattleMons[1].status1 = 0;
    u8 *innateMode = RhhModsState("test:innate_status"); CHECK(innateMode); *innateMode = RHH_EFFECT_BURN;
    RunInnateStatusTurn(1, STATUS1_BURN, MAX_BATTLERS_COUNT);
    CHECK(sStatusCount == 11 && sStatusEvents[10].trigger == RHH_BATTLE_STATUS_FROM_ABILITY
        && sStatusEvents[10].source_battler == 0 && sStatusEvents[10].target_battler == 1
        && sStatusAttributions[10].kind == RHH_STATUS_ATTRIBUTION_INNATE_TURN
        && sStatusAttributions[10].ability == RhhContentKeyOf("test:ability/turn_status")
        && sStatusAttributions[10].index == 0 && sStatusAttributions[10].sequence == 0);
    gBattleMons[1].status1 = 0; gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_FIRE;
    RunInnateStatusTurn(2, 0, MAX_BATTLERS_COUNT); CHECK(sStatusCount == 11);
    gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_WATER;
    ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_ABILITY);
    CHECK(sStatusCount == 12 && sStatusAttributions[11].kind == RHH_STATUS_ATTRIBUTION_UNKNOWN
        && !sStatusAttributions[11].ability);
    gBattleMons[1].status1 = 0;
    RunInnateStatusEntry(STATUS1_BURN, MAX_BATTLERS_COUNT);
    CHECK(sStatusCount == 13 && sStatusAttributions[12].kind == RHH_STATUS_ATTRIBUTION_INNATE_ENTRY
        && sStatusAttributions[12].ability == RhhContentKeyOf("test:ability/turn_status")
        && sStatusAttributions[12].index == 0 && sStatusAttributions[12].sequence == 1);
    for (u32 i = 0; i < 10; ++i) CHECK(sStatusAttributions[i].kind == RHH_STATUS_ATTRIBUTION_UNKNOWN && !sStatusAttributions[i].ability);
    for (u32 i = 0; i < sStatusCount; ++i) CHECK(sStatusEvents[i].sequence == i && sStatusEvents[i].run_id == 456);
    gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_WATER; gBattleMons[1].status1 = 0;
    gBattleMons[1].item = ITEM_FLAME_ORB;
    CHECK(ApplyStatusItemForTest(1, HOLD_EFFECT_FLAME_ORB, IsOrbsWhiteHerbActivation) == ITEM_STATUS_CHANGE
        && sItemStatusCount == 1 && sItemStatusEvents[0].added == RHH_BATTLE_CONDITION_BURN
        && sItemStatusEvents[0].item == RhhContentKeyForId(RHH_ITEM, ITEM_FLAME_ORB)
        && sItemStatusEvents[0].origin == RHH_BATTLE_ITEM_HELD && sItemStatusEvents[0].participant_available
        && !memcmp(&sStatusObserverRng, &gRngValue, sizeof(gRngValue)) && !memcmp(&sStatusObserverRng2, &gRng2Value, sizeof(gRng2Value)));
    gBattleMons[1].item = ITEM_RAWST_BERRY;
    CHECK(ApplyStatusItemForTest(1, HOLD_EFFECT_CURE_BRN, IsOnStatusChangeActivation) == ITEM_STATUS_CHANGE
        && sItemStatusCount == 2 && sItemStatusEvents[1].removed == RHH_BATTLE_CONDITION_BURN
        && sItemStatusEvents[1].item == RhhContentKeyForId(RHH_ITEM, ITEM_RAWST_BERRY));
    gBattleMons[1].item = ITEM_TOXIC_ORB;
    CHECK(ApplyStatusItemForTest(1, HOLD_EFFECT_TOXIC_ORB, IsOrbsWhiteHerbActivation) == ITEM_STATUS_CHANGE
        && sItemStatusCount == 3 && sItemStatusEvents[2].added == RHH_BATTLE_CONDITION_TOXIC);
    gBattleMons[1].item = ITEM_NONE; gLastUsedItem = ITEM_PECHA_BERRY;
    CHECK(ApplyStatusItemForTest(1, HOLD_EFFECT_CURE_PSN, IsOnBerryActivation) == ITEM_STATUS_CHANGE
        && sItemStatusCount == 4 && sItemStatusEvents[3].removed == RHH_BATTLE_CONDITION_TOXIC
        && sItemStatusEvents[3].origin == RHH_BATTLE_ITEM_LAST_USED_BERRY
        && sItemStatusEvents[3].item == RhhContentKeyForId(RHH_ITEM, ITEM_PECHA_BERRY));
    gBattleMons[1].status1 = STATUS1_POISON | STATUS1_BURN | STATUS1_SLEEP_TURN(2); gBattleMons[1].item = ITEM_LUM_BERRY;
    CHECK(ApplyStatusItemForTest(1, HOLD_EFFECT_CURE_STATUS, IsOnStatusChangeActivation) == ITEM_STATUS_CHANGE
        && sItemStatusCount == 5 && sItemStatusEvents[4].removed == (RHH_BATTLE_CONDITION_POISON | RHH_BATTLE_CONDITION_BURN | RHH_BATTLE_CONDITION_SLEEP)
        && sItemStatusEvents[4].origin == RHH_BATTLE_ITEM_HELD);
    gBattleMons[1].volatiles.confusionTimer = 2; gLastUsedItem = ITEM_LUM_BERRY;
    CHECK(ApplyStatusItemForTest(1, HOLD_EFFECT_CURE_STATUS, IsOnBerryActivation) == ITEM_STATUS_CHANGE
        && !gBattleMons[1].volatiles.confusionTimer && sItemStatusCount == 5);
    gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_FIRE; gBattleMons[1].item = ITEM_FLAME_ORB;
    CHECK(ApplyStatusItemForTest(1, HOLD_EFFECT_FLAME_ORB, IsOrbsWhiteHerbActivation) == ITEM_NO_EFFECT && sItemStatusCount == 5);
    gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_WATER; ModsBattleSwitchPrepared(1);
    CHECK(ApplyStatusItemForTest(1, HOLD_EFFECT_FLAME_ORB, IsOrbsWhiteHerbActivation) == ITEM_STATUS_CHANGE
        && sItemStatusCount == 6 && !sItemStatusEvents[5].participant_available);
    ModsBattleSwitchEntered(1);
    gBattleTypeFlags |= BATTLE_TYPE_LINK; ModsBattleItemStatusChanged(1, ITEM_FLAME_ORB, RHH_BATTLE_ITEM_HELD, 0, FALSE);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; gBattleTypeFlags |= BATTLE_TYPE_RECORDED;
    ModsBattleItemStatusChanged(1, ITEM_FLAME_ORB, RHH_BATTLE_ITEM_HELD, 0, FALSE); gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED;
    CHECK(sItemStatusCount == 6);
    for (u32 i = 0; i < sItemStatusCount; ++i) CHECK(sItemStatusEvents[i].sequence == i && sItemStatusEvents[i].run_id == 456);
    ModsBattleReset(); gBattleMons[1].status1 = 0; ModsBattleBegin();
    ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE);
    CHECK(sStatusCount == 14 && !sStatusEvents[13].sequence && sStatusAttributions[13].kind == RHH_STATUS_ATTRIBUTION_UNKNOWN
        && sStatusBattle->status_application_context(&outside)
        && !memcmp(&outside, &saved, sizeof(outside)));
    gBattleMons[1].item = ITEM_RAWST_BERRY;
    CHECK(ApplyStatusItemForTest(1, HOLD_EFFECT_CURE_BRN, IsOnStatusChangeActivation) == ITEM_STATUS_CHANGE
        && sItemStatusCount == 7 && !sItemStatusEvents[6].sequence && sStatusBattle->item_status_context(&outsideItem)
        && !memcmp(&outsideItem, &savedItem, sizeof(outsideItem)));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gBattleSpritesDataPtr = NULL;
    gAiLogicData = NULL; gAiPartyData = NULL; gAiThinkingStruct = NULL; gBattleHistory = NULL;
    RhhModsUnloadLibraries(); puts("battle status: committed setter and innate application passed");
}
