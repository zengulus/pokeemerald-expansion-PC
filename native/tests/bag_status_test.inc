extern void BS_ItemCureStatus(void);
static const struct RhhBattleService *sBagStatusBattle;
static u32 sBagActiveCount, sBagPartyCount, sBagConditionProposals;
static RhhContentKey sBagConditionKey;
static void ObserveBagStatus(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleItemStatusChange change = {.struct_size = sizeof(change)};
    CHECK(!sBagStatusBattle->item_status_context(&change) && change.origin == RHH_BATTLE_ITEM_BAG
        && change.item == RhhContentKeyForId(RHH_ITEM, ITEM_AWAKENING) && change.battler == 0
        && change.removed == RHH_BATTLE_CONDITION_SLEEP && !change.added && change.participant_available
        && event->ordinal == sBagActiveCount && event->args[3] == RHH_BATTLE_ITEM_BAG
        && !sBagStatusBattle->status_reaction_heal(0, 1, 4));
    ++sBagActiveCount; (void)Random(); (void)Random2();
}
static void ObserveBagPartyStatus(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattlePartyItemStatusCure change = {.struct_size = sizeof(change)};
    struct RhhBattlePartyItemStatusCure tiny = {.struct_size = 1, .item = 99}, saved = tiny;
    CHECK(!sBagStatusBattle->party_item_status_cure_context(&change)
        && sBagStatusBattle->party_item_status_cure_context(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny))
        && change.cure.party_slot == 3 && change.cure.owner == B_TRAINER_PLAYER && change.cure.source_battler == 0
        && change.item == event->event_key && change.cure.identity == event->subject
        && change.cure.sequence == event->ordinal && event->ordinal == sBagPartyCount
        && change.cure.removed == event->args[1] && !change.cure.move
        && GetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_STATUS) == change.cure.after_raw
        && !sBagStatusBattle->status_reaction_heal(0, 1, 4));
    if (!sBagPartyCount) CHECK(change.item == RhhContentKeyForId(RHH_ITEM, ITEM_AWAKENING)
        && change.cure.before_raw == (STATUS1_SLEEP | STATUS1_POISON) && change.cure.after_raw == STATUS1_POISON
        && change.cure.removed == RHH_BATTLE_CONDITION_SLEEP);
    else CHECK(change.item == RhhContentKeyForId(RHH_ITEM, ITEM_ANTIDOTE)
        && change.cure.before_raw == STATUS1_POISON && !change.cure.after_raw
        && change.cure.removed == RHH_BATTLE_CONDITION_POISON);
    if (!sBagPartyCount) CHECK(!sBagStatusBattle->condition_set(sBagConditionKey, RHH_CONDITION_PLAYER_SIDE, 3));
    ++sBagPartyCount; (void)Random(); (void)Random2();
}
static void BagStatusConditionPolicy(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhBattleConditionProposal proposal = {.struct_size = sizeof(proposal)};
    struct RhhBattlePartyItemStatusCure cure = {.struct_size = sizeof(cure)};
    CHECK(!sBagStatusBattle->condition_proposal_context(&proposal) && !sBagStatusBattle->party_item_status_cure_context(&cure)
        && proposal.origin_event == RHH_BATTLE_PARTY_ITEM_STATUS_CURED && proposal.actor == 0
        && proposal.recipient == RHH_BATTLE_NO_BATTLER && proposal.source == cure.item && cure.cure.party_slot == 3);
    ++sBagConditionProposals;
}
static void BagStatusConditionChanged(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhBattlePartyItemStatusCure cure = {.struct_size = sizeof(cure)};
    CHECK(!sBagStatusBattle->party_item_status_cure_context(&cure)
        && !sBagStatusBattle->condition_change_animation(0, RHH_EFFECT_ANIM_HEAL));
}
static const struct RhhModBattleCondition sBagCondition = {.struct_size = sizeof(sBagCondition),
    .scope = RHH_CONDITION_SIDE, .max_turns = 5, .id = "test:condition/bag_cure", .label = "Bag cure"};
static const struct RhhModHook sBagStatusHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLER_ITEM_STATUS_CHANGED, RHH_OBSERVE, 0, NULL, ObserveBagStatus},
    {sizeof(struct RhhModHook), RHH_BATTLE_PARTY_ITEM_STATUS_CURED, RHH_OBSERVE, 0, NULL, ObserveBagPartyStatus},
    {sizeof(struct RhhModHook), RHH_BATTLE_CONDITION_PROPOSAL, RHH_CONSTRAINT, 0, BagStatusConditionPolicy},
    {sizeof(struct RhhModHook), RHH_BATTLE_CONDITION_CHANGED, RHH_OBSERVE, 0, NULL, BagStatusConditionChanged}
};
static const struct RhhModDefinition sBagStatusMod = {.struct_size = sizeof(sBagStatusMod), .abi_version = RHH_MOD_ABI_CONDITIONS,
    .id = "test:bag_status", .hooks = sBagStatusHooks, .hook_count = ARRAY_COUNT(sBagStatusHooks),
    .battle_conditions = &sBagCondition, .battle_condition_count = 1};
static void RunBagStatusCure(void)
{
    static const u8 noStatus[] = {B_SCR_OP_END}, activeStatus[] = {B_SCR_OP_END};
    struct __attribute__((packed)) {u8 op; void (*func)(void); const u8 *noStatus, *activeStatus;} command = {
        B_SCR_OP_CALLNATIVE, BS_ItemCureStatus, noStatus, activeStatus};
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = (const u8 *)&command;
    BS_ItemCureStatus();
}
static void CheckBagStatusReactions(bool32 animation, bool32 replace)
{
    static const u8 resume[] = {B_SCR_OP_END};
    u32 status = 0, source = 0, species = gBattleMons[0].species;
    CHECK(ModsStartBagStatusReactions(resume));
    if (replace) gBattleMons[0].species = SPECIES_MUDKIP;
    CHECK(ModsStatusReactionNext(&status, &source) == (replace ? MOD_STATUS_DONE : MOD_STATUS_HEAL));
    if (!replace) {
        CHECK(ModsStatusReactionNext(&status, &source) == (animation ? MOD_STATUS_ANIMATION : MOD_STATUS_DONE));
        if (animation) CHECK(ModsStatusReactionNext(&status, &source) == MOD_STATUS_DONE);
    }
    gBattleMons[0].species = species;
    CHECK(ModsStatusReactionContinuation() == resume && !ModsStartBagStatusReactions(resume));
}
static void CheckBagStatus(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sBagStatusMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0); sBagConditionKey = RhhContentKeyOf(sBagCondition.id);
    sBagStatusBattle = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 41));
    CHECK(sBagStatusBattle && sBagStatusBattle->header.struct_size >= RHH_BATTLE_SERVICE_V1_40_SIZE
        && RHH_BATTLE_SERVICE_V1_39_SIZE < RHH_BATTLE_SERVICE_V1_40_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 46)));
    struct RhhBattlePartyItemStatusCure outside = {.struct_size = sizeof(outside), .item = 99}, saved = outside;
    CHECK(sBagStatusBattle->party_item_status_cure_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct AiLogicData ai = {0}; struct AiPartyData parties = {0}; struct BattleHistory history = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources;
    gAiLogicData = &ai; gAiPartyData = &parties; gBattleHistory = &history;
    PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); gBattlerAttacker = 0;
    gBattleMons[0].hp = gBattleMons[0].maxHP / 2;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][3], SPECIES_MUDKIP, 20, 4567, OTID_STRUCT_PRESET(9876), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 4; ModsBattleBegin();
    u32 status = STATUS1_SLEEP; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_STATUS, &status);
    gBattleMons[0].status1 = status; gBattleMons[0].volatiles.nightmare = TRUE;
    gBattleStruct->itemPartyIndex[0] = 0; gLastUsedItem = ITEM_AWAKENING;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    RunBagStatusCure();
    CHECK(sBagActiveCount == 1 && !sBagPartyCount && !gBattleMons[0].status1 && !gBattleMons[0].volatiles.nightmare
        && !GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_STATUS)
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    CheckBagStatusReactions(FALSE, FALSE);
    RunBagStatusCure(); CHECK(sBagActiveCount == 1);
    status = STATUS1_SLEEP | STATUS1_POISON; SetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_STATUS, &status);
    gBattleStruct->itemPartyIndex[0] = 3; RunBagStatusCure();
    CHECK(sBagPartyCount == 1 && sBagConditionProposals == 1 && sBagActiveCount == 1
        && GetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_STATUS) == STATUS1_POISON
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    CheckBagStatusReactions(TRUE, FALSE);
    gLastUsedItem = ITEM_ANTIDOTE; RunBagStatusCure();
    CHECK(sBagPartyCount == 2 && !GetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_STATUS));
    CheckBagStatusReactions(FALSE, TRUE);
    status = STATUS1_SLEEP; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_STATUS, &status);
    gBattleMons[0].status1 = status; gBattleStruct->itemPartyIndex[0] = 0; gLastUsedItem = ITEM_AWAKENING;
    ai.aiCalcInProgress = TRUE; RunBagStatusCure(); ai.aiCalcInProgress = FALSE;
    CHECK(sBagActiveCount == 1 && !gBattleMons[0].status1);
    status = STATUS1_POISON; SetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_STATUS, &status);
    gBattleStruct->itemPartyIndex[0] = 3; gLastUsedItem = ITEM_ANTIDOTE;
    gBattleTypeFlags |= BATTLE_TYPE_LINK; RunBagStatusCure(); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    CHECK(sBagPartyCount == 2 && !GetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_STATUS)
        && sBagStatusBattle->party_item_status_cure_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gAiLogicData = NULL; gAiPartyData = NULL; gBattleHistory = NULL;
    puts("bag status: active and off-field item cures, conditions and RNG passed");
}
