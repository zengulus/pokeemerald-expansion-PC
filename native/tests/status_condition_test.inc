#include "constants/battle_anim.h"
#include "battle_util2.h"
extern u8 gModEffectAnimation;
struct StatusConditionProbeState { u32 enabled, attempts, status, previous, remaining, changed, actions, nested, cured, scriptOrder, partyCured; };
static const struct RhhBattleService *sStatusConditionBattle;
static u32 sStatusPolicyMode, sStatusPolicyCalls, sStatusConditionChanges, sStatusExpectedOrigin;
static RhhContentKey sStatusConditionKey, sStatusExpectedSource;
static u32 sStatusExpectedActor, sStatusExpectedRecipient;
static u32 sStatusCueMode;
static bool32 sCureObserverEnabled;
static u32 sCureObserverCount;
static rng_value_t sCureObserverRng, sCureObserverRng2;
static bool32 sPartyCureObserverEnabled;
static u32 sPartyCureObserverCount;
static rng_value_t sPartyCureObserverRng, sPartyCureObserverRng2;
static void StatusConditionPolicy(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhBattleConditionProposal proposal = {.struct_size = sizeof(proposal)};
    CHECK(!sStatusConditionBattle->condition_proposal_context(&proposal) && proposal.key == sStatusConditionKey
        && proposal.origin_event == sStatusExpectedOrigin && proposal.source == sStatusExpectedSource
        && proposal.actor == sStatusExpectedActor && proposal.recipient == sStatusExpectedRecipient
        && proposal.target == RHH_CONDITION_PLAYER_SIDE && proposal.requested_turns == 3
        && event->args[3] == sStatusExpectedOrigin && event->subject == proposal.source);
    if (sStatusExpectedOrigin == RHH_BATTLER_STATUS_APPLIED) {
        struct RhhBattleStatusApplication status = {.struct_size = sizeof(status)};
        struct RhhBattleStatusAttribution attribution = {.struct_size = sizeof(attribution)};
        struct RhhBattleItemStatusChange item = {.struct_size = sizeof(item)};
        CHECK(!sStatusConditionBattle->status_application_context(&status)
            && !sStatusConditionBattle->status_attribution(&attribution)
            && attribution.kind == (sStatusExpectedSource ? RHH_STATUS_ATTRIBUTION_UNKNOWN : RHH_STATUS_ATTRIBUTION_INNATE_TURN)
            && attribution.ability == (sStatusExpectedSource ? 0 : RhhContentKeyOf("test:ability/turn_status"))
            && status.target_battler == proposal.recipient && sStatusConditionBattle->item_status_context(&item));
    } else if (sStatusExpectedOrigin == RHH_BATTLER_STATUS_CURED) {
        struct RhhBattleStatusCure cure = {.struct_size = sizeof(cure)};
        struct RhhBattleStatusCureAttribution attribution = {.struct_size = sizeof(attribution)};
        RhhContentKey move = 99;
        CHECK(!sStatusConditionBattle->status_cure_context(&cure) && cure.battler == proposal.recipient
            && !sStatusConditionBattle->status_cure_attribution(&attribution) && !sStatusConditionBattle->status_cure_move(&move)
            && (attribution.cause == RHH_BATTLE_CURE_MOVE ? move : attribution.ability) == sStatusExpectedSource
            && attribution.source_battler == sStatusExpectedActor);
    } else if (sStatusExpectedOrigin == RHH_BATTLE_PARTY_STATUS_CURED) {
        struct RhhBattlePartyStatusCure cure = {.struct_size = sizeof(cure)};
        CHECK(!sStatusConditionBattle->party_status_cure_context(&cure) && (cure.party_slot == 3 || cure.party_slot == 4)
            && cure.source_battler == proposal.actor && cure.move == proposal.source);
    } else {
        struct RhhBattleItemStatusChange item = {.struct_size = sizeof(item)};
        struct RhhBattleStatusApplication status = {.struct_size = sizeof(status)};
        struct RhhBattleStatusAttribution attribution = {.struct_size = sizeof(attribution)};
        CHECK(!sStatusConditionBattle->item_status_context(&item) && item.battler == proposal.recipient
            && sStatusConditionBattle->status_application_context(&status) && sStatusConditionBattle->status_attribution(&attribution));
    }
    CHECK(sStatusConditionBattle->condition_set(sStatusConditionKey, RHH_CONDITION_PLAYER_SIDE, 1));
    CHECK(sStatusConditionBattle->status_reaction_heal(1, 1, 4));
    ++sStatusPolicyCalls;
    if (sStatusPolicyMode == 1) decision->value = 2;
    if (sStatusPolicyMode == 3) { (void)Random(); (void)Random2(); }
    if (sStatusPolicyMode == 4) gBattlerPartyIndexes[1] ^= 1;
    if (sStatusPolicyMode == 5) gBattleMons[1].status1 = 0;
}
static void StatusConditionDeny(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sStatusPolicyMode == 2) { decision->allowed = 0; snprintf(decision->reason, sizeof(decision->reason), "status rule denied"); }
}
static void StatusConditionChanged(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleConditionChange change = {.struct_size = sizeof(change)};
    CHECK(!sStatusConditionBattle->condition_change_context(&change) && change.key == sStatusConditionKey
        && event->event_key == change.key && sStatusConditionBattle->condition_set(change.key, change.target, 1));
    if (sStatusCueMode) {
        CHECK(sStatusConditionBattle->condition_change_animation(999, RHH_EFFECT_ANIM_PROTECT)
            && sStatusConditionBattle->condition_change_animation(1, 0));
        u32 count = sStatusCueMode == 3 ? 8 : 1;
        for (u32 i = 0; i < count; ++i)
            CHECK(!!sStatusConditionBattle->condition_change_animation(1, RHH_EFFECT_ANIM_PROTECT) == (sStatusCueMode == 2));
        if (sStatusCueMode == 3) CHECK(sStatusConditionBattle->condition_change_animation(1, RHH_EFFECT_ANIM_PROTECT)
            && sStatusConditionBattle->status_reaction_heal(1, 1, 4)
            && sStatusConditionBattle->status_reaction_damage(1, 1, 4, 0));
        if (sStatusCueMode == 4) CHECK(!sStatusConditionBattle->status_reaction_heal(1, 1, 4)
            && !sStatusConditionBattle->status_reaction_stat(1, RHH_STAT_DEFENSE, 1));
    }
    if (sStatusExpectedOrigin == RHH_BATTLER_STATUS_APPLIED) {
        struct RhhBattleStatusApplication status = {.struct_size = sizeof(status)};
        struct RhhBattleStatusAttribution attribution = {.struct_size = sizeof(attribution)};
        CHECK(!sStatusConditionBattle->status_application_context(&status) && !sStatusConditionBattle->status_attribution(&attribution)
            && attribution.kind == (sStatusExpectedSource ? RHH_STATUS_ATTRIBUTION_UNKNOWN : RHH_STATUS_ATTRIBUTION_INNATE_TURN));
    } else if (sStatusExpectedOrigin == RHH_BATTLER_STATUS_CURED) {
        struct RhhBattleStatusCure cure = {.struct_size = sizeof(cure)};
        struct RhhBattleStatusCureAttribution attribution = {.struct_size = sizeof(attribution)};
        RhhContentKey move = 99;
        CHECK(!sStatusConditionBattle->status_cure_context(&cure)
            && !sStatusConditionBattle->status_cure_attribution(&attribution) && !sStatusConditionBattle->status_cure_move(&move)
            && (attribution.cause == RHH_BATTLE_CURE_MOVE ? move : attribution.ability) == sStatusExpectedSource);
    } else if (sStatusExpectedOrigin == RHH_BATTLE_PARTY_STATUS_CURED) {
        struct RhhBattlePartyStatusCure cure = {.struct_size = sizeof(cure)};
        CHECK(!sStatusConditionBattle->party_status_cure_context(&cure) && cure.party_slot == 3);
    } else {
        struct RhhBattleItemStatusChange item = {.struct_size = sizeof(item)};
        CHECK(!sStatusConditionBattle->item_status_context(&item));
    }
    ++sStatusConditionChanges;
}
static void ObserveCureRng(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sCureObserverEnabled) return;
    struct RhhBattleStatusCureAttribution attribution = {.struct_size = sizeof(attribution)};
    RhhContentKey move = 0;
    CHECK(!sStatusConditionBattle->status_cure_attribution(&attribution) && !sStatusConditionBattle->status_cure_move(&move)
        && (attribution.cause == RHH_BATTLE_CURE_MOVE ? move : attribution.ability) == sStatusExpectedSource
        && attribution.source_battler == sStatusExpectedActor && event->event_key == sStatusExpectedSource);
    ++sCureObserverCount; sCureObserverRng = gRngValue; sCureObserverRng2 = gRng2Value;
    (void)Random(); (void)Random2();
}
static void ObservePartyCureRng(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sPartyCureObserverEnabled) return;
    struct RhhBattlePartyStatusCure cure = {.struct_size = sizeof(cure)};
    CHECK(!sStatusConditionBattle->party_status_cure_context(&cure) && cure.party_slot == 3 + sPartyCureObserverCount
        && event->subject == cure.identity);
    ++sPartyCureObserverCount; sPartyCureObserverRng = gRngValue; sPartyCureObserverRng2 = gRng2Value;
    (void)Random(); (void)Random2();
}
static const struct RhhModHook sStatusPolicyHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_CONDITION_PROPOSAL, RHH_TRANSFORM, 0, StatusConditionPolicy, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_CONDITION_PROPOSAL, RHH_CONSTRAINT, 0, StatusConditionDeny, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_CONDITION_CHANGED, RHH_OBSERVE, 0, NULL, StatusConditionChanged},
    {sizeof(struct RhhModHook), RHH_BATTLER_STATUS_CURED, RHH_OBSERVE, 0, NULL, ObserveCureRng},
    {sizeof(struct RhhModHook), RHH_BATTLE_PARTY_STATUS_CURED, RHH_OBSERVE, 0, NULL, ObservePartyCureRng}
};
static const struct RhhModDefinition sStatusPolicyMod = {.struct_size = sizeof(sStatusPolicyMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:status_condition_policy", .hooks = sStatusPolicyHooks, .hook_count = ARRAY_COUNT(sStatusPolicyHooks)};
static void ExpectStatusOrigin(u32 origin, RhhContentKey source, u32 actor)
{ sStatusExpectedOrigin = origin; sStatusExpectedSource = source; sStatusExpectedActor = actor; sStatusExpectedRecipient = 1; }
static const u8 sStatusReactionResume[] = {B_SCR_OP_END};
static void RunToStatusReaction(void)
{
    for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModStatusReaction; ++step) {
        CHECK(step < 4096 && *gBattlescriptCurrInstr != B_SCR_OP_END);
        if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
        u32 opcode = *gBattlescriptCurrInstr;
        if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[opcode](); AnimateSprites();
    }
}
static void PlayStatusReaction(u32 count)
{
    u32 savedRecipient = ModsMoveEffectRecipient(), savedAnimation = gModEffectAnimation;
    for (u32 i = 0; i < count; ++i) {
        CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReaction);
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_PLAYANIMATION_VAR);
        gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION_VAR]();
        CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[1][0] == CONTROLLER_BATTLEANIMATION
            && gBattleResources->bufferA[1][1] == B_ANIM_PROTECTED_ITSELF);
        gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE]();
        gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr != BattleScript_ModStatusReaction
        && ModsMoveEffectRecipient() == savedRecipient && gModEffectAnimation == savedAnimation);
}
static void PlayMixedStatusReaction(void)
{
    u32 savedRecipient = ModsMoveEffectRecipient(), savedAnimation = gModEffectAnimation;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_PLAYANIMATION_VAR);
    gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION_VAR]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[1][0] == CONTROLLER_BATTLEANIMATION
        && gBattleResources->bufferA[1][1] == B_ANIM_PROTECTED_ITSELF);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE]();
    gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReactionHeal);
    gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[1][0] == CONTROLLER_HEALTHBARUPDATE);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[1][0] == CONTROLLER_SETMONDATA);
    BtlController_HandleSetMonData(1); CHECK(!gBattleControllerExecFlags);
    gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReactionStat);
    for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModStatusReaction; ++step) {
        CHECK(step < 4096 && *gBattlescriptCurrInstr != B_SCR_OP_END);
        if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
        u32 opcode = *gBattlescriptCurrInstr;
        if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[opcode](); AnimateSprites();
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && ModsMoveEffectRecipient() == savedRecipient
        && gModEffectAnimation == savedAnimation);
}
static void PlaySourcedStatusReaction(bool32 applied)
{
    u32 savedScripting = gBattleScripting.battler, savedSource = ModsInnateStatSource();
    bool32 savedMoldBreaker = gBattleStruct->moldBreakerActive;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReactionSourcedStat);
    for (u32 step = 0; gBattlescriptCurrInstr != BattleScript_ModStatusReaction; ++step) {
        CHECK(step < 4096 && *gBattlescriptCurrInstr != B_SCR_OP_END);
        if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
        u32 opcode = *gBattlescriptCurrInstr;
        if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[opcode](); AnimateSprites();
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    if (applied) {
        CHECK(gBattlescriptCurrInstr == BattleScript_MoveEffectPoison);
        RunToStatusReaction();
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_PLAYANIMATION_VAR);
        gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION_VAR]();
        CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[2][0] == CONTROLLER_BATTLEANIMATION
            && gBattleResources->bufferA[2][1] == B_ANIM_PROTECTED_ITSELF);
        gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE]();
        gBattleScriptingCommandsTable[B_SCR_OP_GOTO](); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReaction);
    } else CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReaction || gBattlescriptCurrInstr == sStatusReactionResume);
    if (gBattlescriptCurrInstr == BattleScript_ModStatusReaction)
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && gBattleScripting.battler == savedScripting
        && ModsInnateStatSource() == savedSource && gBattleStruct->moldBreakerActive == savedMoldBreaker);
}
static void PlayStatusDamage(u32 expectedDamageHp, bool32 fainted)
{
    u32 savedScripting = gBattleScripting.battler, savedRecipient = ModsMoveEffectRecipient();
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReactionDamage && gBattleScripting.battler == 2
        && gBattleStruct->passiveHpUpdate[2] > 0);
    gBattleScriptingCommandsTable[B_SCR_OP_HEALTHBARUPDATE]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[2][0] == CONTROLLER_HEALTHBARUPDATE);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_DATAHPUPDATE]();
    CHECK(gBattleControllerExecFlags && gBattleMons[2].hp == expectedDamageHp
        && gBattleResources->bufferA[2][0] == CONTROLLER_SETMONDATA);
    BtlController_HandleSetMonData(2); CHECK(!gBattleControllerExecFlags);
    gBattleScriptingCommandsTable[B_SCR_OP_TRYFAINTMON]();
    if (fainted) CHECK(gBattlescriptCurrInstr == BattleScript_FaintBattler && (gHitMarker & HITMARKER_FAINTED(2)));
    RunToStatusReaction(); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && gBattleScripting.battler == savedScripting
        && ModsMoveEffectRecipient() == savedRecipient && gBattleStruct->passiveHpUpdate[2] == 7);
}
static void CheckAbilityCure(struct StatusConditionProbeState *probe, u32 ability, u32 mode, u32 status)
{
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[1].ability = ability; gBattleMons[1].status1 = status;
    SetMonData(GetBattlerMon(1), MON_DATA_STATUS, &status);
    gBattlerAttacker = 1; gBattlerTarget = 1; gCurrentMove = MOVE_WILL_O_WISP; gHitMarker = 0;
    gBattleWeather = ability == ABILITY_HYDRATION ? B_WEATHER_RAIN_NORMAL : B_WEATHER_NONE;
    probe->actions = mode; sStatusPolicyMode = 0; sStatusCueMode = 1;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_CURED, RhhContentKeyForId(RHH_ABILITY, ability), 1);
    u32 beforeCured = probe->cured; sCureObserverEnabled = TRUE; sCureObserverCount = 0;
    u32 effect = 0;
    for (u32 attempt = 0; attempt < 512 && !effect; ++attempt) {
        gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0;
        gBattlescriptCurrInstr = sStatusReactionResume;
        effect = AbilityBattleEffects(mode == 10 ? ABILITYEFFECT_IMMUNITY : ABILITYEFFECT_ENDTURN,
            1, ability, MOVE_NONE, TRUE);
    }
    sCureObserverEnabled = FALSE;
    CHECK(effect && !gBattleMons[1].status1 && probe->cured == beforeCured + 1 && sCureObserverCount == 1
        && !memcmp(&sCureObserverRng, &gRngValue, sizeof(gRngValue))
        && !memcmp(&sCureObserverRng2, &gRng2Value, sizeof(gRng2Value))
        && gBattleControllerExecFlags && gBattleResources->battleScriptsStack->size == 1);
    BtlController_HandleSetMonData(1); CHECK(!gBattleControllerExecFlags && !GetMonData(GetBattlerMon(1), MON_DATA_STATUS));
    RunToStatusReaction(); PlayStatusReaction(2);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && !gBattleResources->battleScriptsStack->size);
    gBattleWeather = B_WEATHER_NONE;
}
static void CheckEngineStatusCure(struct StatusConditionProbeState *probe, u32 mode)
{
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    bool32 moveCure = mode == 14 || mode == 17 || mode == 35;
    u32 move = mode == 14 ? MOVE_FLAME_WHEEL : mode == 17 ? MOVE_FLAMETHROWER : mode == 35 ? MOVE_TAKE_HEART : MOVE_TACKLE;
    u32 status = mode == 13 || mode == 15 ? STATUS1_SLEEP_TURN(1)
        : mode == 14 ? STATUS1_FROSTBITE : mode == 35 ? STATUS1_BURN : STATUS1_FREEZE;
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].status1 = status;
    SetMonData(GetBattlerMon(1), MON_DATA_STATUS, &status);
    GetBattlerPartyState(1)->freezeTurns = 2; gBattleMons[1].volatiles.nightmare = TRUE;
    gBattlerAttacker = mode == 17 ? 0 : 1; gBattlerTarget = mode == 17 ? 1 : 0;
    gCurrentMove = move; gBattleWeather = B_WEATHER_NONE; gHitMarker = 0;
    probe->actions = mode; sStatusPolicyMode = 0; sStatusCueMode = 1;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_CURED, moveCure ? RhhContentKeyForId(RHH_MOVE, move) : 0,
        moveCure ? gBattlerAttacker : RHH_BATTLE_NO_BATTLER);
    u32 beforeCured = probe->cured; sCureObserverEnabled = TRUE; sCureObserverCount = 0;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    if (mode == 15 || mode == 16) {
        gBattleCommunication[MULTIUSE_STATE] = 0;
        CHECK(BattlePalace_TryEscapeStatus(1) == 2);
    } else if (mode == 35) {
        gHitMarker |= HITMARKER_NO_ANIMATIONS;
        gBattleStruct->eventState.resolution = STAT_CHANGE_BEFORE_CHANGE;
        CHECK(DoStatChange() == MOVE_RESULT_RUN_SCRIPT_INCREMENT);
    } else if (mode == 17) {
        gSpecialStatuses[1].damagedByAttack = TRUE; gBattleScripting.moveendState = MOVEEND_DEFROST;
        gBattleStruct->eventState.moveEndBattler = 0;
        CHECK(DoMoveEnd(1, MOVEEND_DEFROST) == MOVEEND_RESULT_RUN_SCRIPT);
    } else {
        gBattleStruct->eventState.atkCanceler = mode == 14 ? CANCELER_THAW : CANCELER_ASLEEP_OR_FROZEN;
        CHECK(DoAttackCanceler() == CANCELER_RESULT_RUN_SCRIPT_AND_INCREMENT);
    }
    sCureObserverEnabled = FALSE;
    CHECK(!gBattleMons[1].status1 && probe->cured == beforeCured + 1 && sCureObserverCount == 1
        && !memcmp(&sCureObserverRng, &gRngValue, sizeof(gRngValue))
        && !memcmp(&sCureObserverRng2, &gRng2Value, sizeof(gRng2Value))
        && gBattleControllerExecFlags && gBattleResources->battleScriptsStack->size == 1);
    BtlController_HandleSetMonData(1); CHECK(!gBattleControllerExecFlags && !GetMonData(GetBattlerMon(1), MON_DATA_STATUS));
    RunToStatusReaction();
    if (mode == 35) gHitMarker &= ~HITMARKER_NO_ANIMATIONS;
    PlayStatusReaction(2);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && !gBattleResources->battleScriptsStack->size);
    if (mode == 35) {
        gBattleStruct->eventState.resolution = STAT_CHANGE_BEFORE_CHANGE;
        gBattleStruct->moveResultFlags[1] |= MOVE_RESULT_ATTEMPT_STAT_CHANGE;
        gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
        CHECK(DoStatChange() == MOVE_RESULT_RUN_SCRIPT_INCREMENT && probe->cured == beforeCured + 1
            && gBattleResources->battleScriptsStack->size == 1 && !gBattleMons[1].status1);
        gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0;
    }
}
extern void BS_TryWakeBattlersUproar(void);
static void CheckSwitchAndUproarCures(struct StatusConditionProbeState *probe)
{
    static const u8 switchCommand[] = {B_SCR_OP_SWITCHOUTABILITIES, BS_TARGET, B_SCR_OP_END};
    ModsBattleReset(); PrepareTurnBattle(); ModsBattleBegin();
    gBattlerTarget = 1; gBattleMons[1].ability = ABILITY_NATURAL_CURE;
    u32 status = STATUS1_BURN; gBattleMons[1].status1 = status; SetMonData(GetBattlerMon(1), MON_DATA_STATUS, &status);
    probe->actions = 33; sStatusCueMode = 0; sStatusPolicyMode = 0;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_CURED, RhhContentKeyForId(RHH_ABILITY, ABILITY_NATURAL_CURE), 1);
    u32 beforeCured = probe->cured; sCureObserverEnabled = TRUE; sCureObserverCount = 0;
    gBattleControllerExecFlags = 0; gBattleResources->battleScriptsStack->size = 0; gBattlescriptCurrInstr = switchCommand;
    gBattleScriptingCommandsTable[B_SCR_OP_SWITCHOUTABILITIES](); sCureObserverEnabled = FALSE;
    CHECK(probe->cured == beforeCured + 1 && sCureObserverCount == 1 && !gBattleMons[1].status1
        && gBattleStruct->battlerState[1].notOnField && gBattlescriptCurrInstr == switchCommand + 2
        && !gBattleResources->battleScriptsStack->size && gBattleControllerExecFlags
        && !memcmp(&sCureObserverRng, &gRngValue, sizeof(gRngValue))
        && !memcmp(&sCureObserverRng2, &gRng2Value, sizeof(gRng2Value)));
    BtlController_HandleSetMonData(1); CHECK(!gBattleControllerExecFlags && !GetMonData(GetBattlerMon(1), MON_DATA_STATUS));
    gBattlescriptCurrInstr = switchCommand; gBattleScriptingCommandsTable[B_SCR_OP_SWITCHOUTABILITIES]();
    CHECK(probe->cured == beforeCured + 1); gBattleControllerExecFlags = 0;

    ModsBattleReset(); PrepareTurnBattle(); ModsBattleBegin();
    gBattleMons[1].ability = ABILITY_NONE; status = STATUS1_SLEEP_TURN(2);
    gBattleMons[1].status1 = status; SetMonData(GetBattlerMon(1), MON_DATA_STATUS, &status);
    probe->actions = 34; sStatusCueMode = 1; sStatusPolicyMode = 0;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_CURED, 0, RHH_BATTLE_NO_BATTLER);
    beforeCured = probe->cured; sCureObserverEnabled = TRUE; sCureObserverCount = 0;
    gBattleScripting.battler = 1; gBattleControllerExecFlags = 0; gBattleResources->battleScriptsStack->size = 0;
    gBattlescriptCurrInstr = sStatusReactionResume; BS_TryWakeBattlersUproar(); sCureObserverEnabled = FALSE;
    CHECK(probe->cured == beforeCured + 1 && sCureObserverCount == 1 && !gBattleMons[1].status1
        && gBattleControllerExecFlags && gBattleResources->battleScriptsStack->size == 1
        && !memcmp(&sCureObserverRng, &gRngValue, sizeof(gRngValue))
        && !memcmp(&sCureObserverRng2, &gRng2Value, sizeof(gRng2Value)));
    BtlController_HandleSetMonData(1); CHECK(!gBattleControllerExecFlags && !GetMonData(GetBattlerMon(1), MON_DATA_STATUS));
    RunToStatusReaction(); PlayStatusReaction(2);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && !gBattleResources->battleScriptsStack->size);
}
extern void BS_ModMoveCureBegin(void);
extern void BS_ModJungleCureBegin(void);
extern void BS_ModCaptureJungleCureActor(void);
extern void BS_ModHealerCureBegin(void);
extern void BS_ModPastelCureBegin(void);
extern void BS_ModCapturePastelCureActor(void);
extern void BS_ModScriptCureReactions(void);
extern void BS_CureStatus(void);
static const u8 *FindScriptCureMarker(const u8 *script, void (*function)(void))
{
    for (u32 i = 0; i < 2048; ++i)
        if (script[i] == B_SCR_OP_CALLNATIVE && !memcmp(script + i + 1, &function, sizeof(function))) return script + i;
    CHECK(FALSE); return NULL;
}
static void PlayScriptCureCue(u32 battler)
{
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReaction);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(*gBattlescriptCurrInstr == B_SCR_OP_PLAYANIMATION_VAR);
    gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION_VAR]();
    CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[battler][0] == CONTROLLER_BATTLEANIMATION
        && gBattleResources->bufferA[battler][1] == B_ANIM_PROTECTED_ITSELF);
    gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE]();
    gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
}
static void PrepareScriptCure(struct StatusConditionProbeState *probe, u32 mode, u32 move)
{
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[0].ability = gBattleMons[1].ability = gBattleMons[2].ability = ABILITY_NONE;
    gBattlerAttacker = 0; gBattlerTarget = 1; gCurrentMove = move; gHitMarker = 0;
    probe->actions = mode; sStatusPolicyMode = 0; sStatusCueMode = mode == 19 ? 1 : 0;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_CURED, RhhContentKeyForId(RHH_MOVE, move), 0);
}
static void CommitScriptCure(u32 battler, const u8 *script, void (*begin)(void))
{
    u32 poison = STATUS1_POISON;
    gBattleMons[battler].status1 = poison; SetMonData(GetBattlerMon(battler), MON_DATA_STATUS, &poison);
    gBattlescriptCurrInstr = FindScriptCureMarker(script, begin);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    u32 opcode = *gBattlescriptCurrInstr;
    CHECK(opcode == B_SCR_OP_CALLNATIVE || opcode == B_SCR_OP_CURESTATUSWITHMOVE || opcode == B_SCR_OP_HEALPARTYSTATUS);
    gBattleScriptingCommandsTable[opcode]();
    CHECK(!gBattleMons[battler].status1 && gBattleControllerExecFlags && gBattlescriptCurrInstr != BattleScript_ModStatusReaction);
    BtlController_HandleSetMonData(battler); CHECK(!gBattleControllerExecFlags);
}
static void PreparePartyCure(struct StatusConditionProbeState *probe, u32 mode)
{
    PrepareScriptCure(probe, mode, MOVE_HEAL_BELL);
    memset(&gParties[B_TRAINER_PLAYER][2], 0, 4 * sizeof(struct Pokemon));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][3], SPECIES_PIKACHU, 20, 45678, OTID_STRUCT_PRESET(23456), 31);
    u32 poison = STATUS1_POISON;
    SetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_STATUS, &poison);
    gPartiesCount[B_TRAINER_PLAYER] = 4;
    if (mode == 28) {
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][4], SPECIES_EEVEE, 20, 45679, OTID_STRUCT_PRESET(23456), 31);
        SetMonData(&gParties[B_TRAINER_PLAYER][4], MON_DATA_STATUS, &poison);
        gPartiesCount[B_TRAINER_PLAYER] = 5;
    }
    ExpectStatusOrigin(RHH_BATTLE_PARTY_STATUS_CURED, RhhContentKeyForId(RHH_MOVE, MOVE_HEAL_BELL), 0);
    sStatusExpectedRecipient = RHH_BATTLE_NO_BATTLER;
    gBattlescriptCurrInstr = FindScriptCureMarker(BattleScript_EffectHealBell, BS_ModMoveCureBegin);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_HEALPARTYSTATUS);
    gBattleScriptingCommandsTable[B_SCR_OP_HEALPARTYSTATUS]();
    CHECK(gBattleControllerExecFlags && GetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_STATUS) == poison);
}
static void FlushPartyCure(void)
{
    CHECK(!gBattleControllerExecFlags);
    gBattlescriptCurrInstr = FindScriptCureMarker(BattleScript_EffectHealBell, BS_ModScriptCureReactions);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
}
static void CheckStatusConditions(const char *probePath, const char *innatePath)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sStatusPolicyMod)
        && !RhhModsLoadLibrary(probePath) && !RhhModsLoadLibrary(innatePath) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0); sStatusPolicyMode = sStatusPolicyCalls = sStatusConditionChanges = 0;
    sStatusConditionBattle = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 20));
    CHECK(sStatusConditionBattle && sStatusConditionBattle->header.struct_size >= RHH_BATTLE_SERVICE_V1_20_SIZE
        && sStatusConditionBattle->status_reaction_animation(1, RHH_EFFECT_ANIM_PROTECT)
        && sStatusConditionBattle->status_reaction_heal(1, 1, 4)
        && sStatusConditionBattle->status_reaction_stat(1, RHH_STAT_DEFENSE, 1)
        && sStatusConditionBattle->status_reaction_stat_from(0, 1, RHH_STAT_ATTACK, -1)
        && sStatusConditionBattle->status_reaction_status_from(0, 1, RHH_EFFECT_POISON)
        && sStatusConditionBattle->status_reaction_damage(1, 1, 4, 0)
        && sStatusConditionBattle->status_reaction_cure(1, RHH_BATTLE_CONDITION_BURN)
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 42)) == &sStatusConditionBattle->header
        && sStatusConditionBattle->header.struct_size >= RHH_BATTLE_SERVICE_V1_42_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 43)) == &sStatusConditionBattle->header
        && sStatusConditionBattle->header.struct_size >= RHH_BATTLE_SERVICE_V1_43_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 46)));
    sStatusConditionKey = RhhContentKeyOf("test:condition/status_ward");
    struct StatusConditionProbeState *probe = RhhModsState("test:status_condition_probe"); CHECK(probe); probe->enabled = 1;
    struct RhhBattleStatusCure outsideCure = {.struct_size = sizeof(outsideCure), .sequence = 99}, savedCure = outsideCure;
    CHECK(sStatusConditionBattle->status_cure_context(&outsideCure) && !memcmp(&outsideCure, &savedCure, sizeof(outsideCure)));
    struct RhhBattleStatusCureAttribution outsideAttribution = {.struct_size = sizeof(outsideAttribution), .ability = 77}, savedAttribution = outsideAttribution;
    CHECK(sStatusConditionBattle->status_cure_attribution(&outsideAttribution)
        && !memcmp(&outsideAttribution, &savedAttribution, sizeof(outsideAttribution)));
    RhhContentKey outsideMove = 77;
    CHECK(sStatusConditionBattle->status_cure_move(&outsideMove) && outsideMove == 77);
    struct RhhBattlePartyStatusCure outsideParty = {.struct_size = sizeof(outsideParty), .sequence = 77}, savedParty = outsideParty;
    CHECK(sStatusConditionBattle->party_status_cure_context(&outsideParty) && !memcmp(&outsideParty, &savedParty, sizeof(outsideParty)));
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
    struct RhhBattleConditionSetResult unavailable = {.struct_size = sizeof(unavailable)};
    CHECK(sStatusConditionBattle->condition_set_result(sStatusConditionKey, RHH_CONDITION_PLAYER_SIDE, 3, &unavailable) == RHH_CONDITION_SET_UNAVAILABLE);
    struct RhhBattleConditionState condition = {.struct_size = sizeof(condition)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE);
    CHECK(probe->attempts == 1 && probe->status == RHH_CONDITION_SET_OK && probe->changed && probe->remaining == 3
        && sStatusPolicyCalls == 1 && sStatusConditionChanges == 1
        && !sStatusConditionBattle->condition_state(sStatusConditionKey, RHH_CONDITION_PLAYER_SIDE, &condition)
        && condition.remaining_turns == 3 && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gBattleMons[1].item = ITEM_RAWST_BERRY;
    ExpectStatusOrigin(RHH_BATTLER_ITEM_STATUS_CHANGED, RhhContentKeyForId(RHH_ITEM, ITEM_RAWST_BERRY), 1);
    CHECK(ApplyStatusItemForTest(1, HOLD_EFFECT_CURE_BRN, IsOnStatusChangeActivation) == ITEM_STATUS_CHANGE
        && probe->attempts == 2 && probe->status == RHH_CONDITION_SET_OK && !probe->changed
        && sStatusPolicyCalls == 2 && sStatusConditionChanges == 1);
    gBattleMons[1].status1 = STATUS1_POISON; gBattleMons[1].item = ITEM_NONE; gLastUsedItem = ITEM_PECHA_BERRY;
    ExpectStatusOrigin(RHH_BATTLER_ITEM_STATUS_CHANGED, RhhContentKeyForId(RHH_ITEM, ITEM_PECHA_BERRY), RHH_BATTLE_NO_BATTLER);
    CHECK(ApplyStatusItemForTest(1, HOLD_EFFECT_CURE_PSN, IsOnBerryActivation) == ITEM_STATUS_CHANGE
        && probe->attempts == 3 && probe->status == RHH_CONDITION_SET_OK && !probe->changed && sStatusPolicyCalls == 3);
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    sStatusPolicyMode = 1; ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE);
    CHECK(probe->attempts == 4 && probe->status == RHH_CONDITION_SET_OK && probe->remaining == 2 && sStatusConditionChanges == 2);
    gBattleMons[1].status1 = 0; sStatusPolicyMode = 2; ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE);
    CHECK(probe->attempts == 5 && probe->status == RHH_CONDITION_SET_DENIED && !probe->changed && probe->remaining == 2);
    gBattleMons[1].status1 = 0; sStatusPolicyMode = 3; rng = gRngValue; rng2 = gRng2Value;
    ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE);
    CHECK(probe->attempts == 6 && probe->status == RHH_CONDITION_SET_DENIED && probe->remaining == 2
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gBattleMons[1].status1 = 0; sStatusPolicyMode = 4; ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE);
    CHECK(probe->attempts == 7 && probe->status == RHH_CONDITION_SET_STALE && probe->remaining == 2); gBattlerPartyIndexes[1] ^= 1;
    gBattleMons[1].status1 = 0; sStatusPolicyMode = 5; ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE);
    CHECK(probe->attempts == 8 && probe->status == RHH_CONDITION_SET_STALE && probe->remaining == 2);
    sStatusPolicyMode = 0; gBattleMons[1].status1 = 0; ModsBattleSwitchPrepared(1);
    ApplyStatusForTest(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE);
    CHECK(probe->attempts == 9 && probe->status == RHH_CONDITION_SET_STALE && sStatusPolicyCalls == 8);
    ModsBattleSwitchEntered(1);
    ModsBattleReset(); gBattleMons[1].status1 = 0; ModsBattleBegin();
    CHECK(!sStatusConditionBattle->condition_state(sStatusConditionKey, RHH_CONDITION_PLAYER_SIDE, &condition) && !condition.remaining_turns);
    u8 *innate = RhhModsState("test:innate_status"); CHECK(innate); *innate = RHH_EFFECT_BURN;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, 0, 0);
    RunInnateStatusTurn(1, STATUS1_BURN, MAX_BATTLERS_COUNT);
    CHECK(probe->attempts == 10 && probe->status == RHH_CONDITION_SET_OK && probe->remaining == 3
        && sStatusPolicyCalls == 9 && sStatusConditionChanges == 3);
    /* Eight cues run after the status script, then restore the prior globals. */
    ModsBattleReset(); PrepareTurnBattle(); ModsBattleBegin();
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_WATER;
    gCurrentMove = MOVE_WILL_O_WISP; gBattlerAttacker = 0; gBattlerTarget = 1;
    sStatusPolicyMode = 0; sStatusCueMode = 3;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    CHECK(gBattleResources->battleScriptsStack->size == 1);
    RunToStatusReaction(); PlayStatusReaction(8);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && !gBattleResources->battleScriptsStack->size);
    /* A curing berry nested inside the status presentation runs its cue first. */
    ModsBattleReset(); PrepareTurnBattle(); ModsBattleBegin();
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_WATER;
    gCurrentMove = MOVE_WILL_O_WISP; gBattlerAttacker = 0; gBattlerTarget = 1; sStatusCueMode = 1;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    gBattleMons[1].item = ITEM_RAWST_BERRY; gBattleControllerExecFlags = 0; sStatusPolicyMode = 1;
    ExpectStatusOrigin(RHH_BATTLER_ITEM_STATUS_CHANGED, RhhContentKeyForId(RHH_ITEM, ITEM_RAWST_BERRY), 1);
    u32 attemptsBeforeBerry = probe->attempts;
    RunToStatusReaction();
    CHECK(probe->attempts == attemptsBeforeBerry + 1 && gBattleResources->battleScriptsStack->size == 1);
    PlayStatusReaction(1);
    CHECK(gBattlescriptCurrInstr != sStatusReactionResume && gBattleResources->battleScriptsStack->size == 1);
    RunToStatusReaction(); PlayStatusReaction(1);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && !gBattleResources->battleScriptsStack->size);
    /* Replaced recipients are skipped and a full script stack gains no entry. */
    ModsBattleReset(); PrepareTurnBattle(); ModsBattleBegin();
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_WATER;
    gCurrentMove = MOVE_WILL_O_WISP; gBattlerAttacker = 0; gBattlerTarget = 1; sStatusPolicyMode = 0;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    ModsBattleSwitchPrepared(1); RunToStatusReaction(); PlayStatusReaction(0);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume);
    ModsBattleReset(); PrepareTurnBattle(); ModsBattleBegin();
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_WATER;
    gCurrentMove = MOVE_WILL_O_WISP; gBattlerAttacker = 0; gBattlerTarget = 1; sStatusCueMode = 1;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    gBattleResources->battleScriptsStack->size = 7; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    CHECK(gBattleResources->battleScriptsStack->size == 8
        && gBattleResources->battleScriptsStack->ptr[7] == BattleScript_ModStatusReaction);
    /* A presentation without a newly pushed return slot cannot queue a cue. */
    ModsBattleReset(); PrepareTurnBattle(); ModsBattleBegin();
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].status1 = STATUS1_BURN;
    gCurrentMove = MOVE_WILL_O_WISP; gBattlerAttacker = 0; gBattlerTarget = 1; sStatusCueMode = 2;
    gBattleResources->battleScriptsStack->size = 1;
    gBattleResources->battleScriptsStack->ptr[0] = sStatusReactionResume; gBattlescriptCurrInstr = BattleScript_MoveEffectBurn;
    ModsBattleStatusApplied(0, 1, MOVE_EFFECT_BURN, TRIGGER_ON_MOVE, 0, FALSE);
    CHECK(gBattleResources->battleScriptsStack->size == 1
        && gBattleResources->battleScriptsStack->ptr[0] == sStatusReactionResume && probe->attempts == 16);
    /* An external status observer queues an animation, heal and stat change. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].types[0] = gBattleMons[1].types[1] = TYPE_WATER;
    u16 lowHp = 1; gBattleMons[1].hp = lowHp; SetMonData(GetBattlerMon(1), MON_DATA_HP, &lowHp);
    gBattleMons[1].statStages[STAT_DEF] = DEFAULT_STAT_STAGE;
    u32 expectedHp = min(gBattleMons[1].maxHP, 1 + max(1, GetNonDynamaxMaxHP(1) / 4));
    gCurrentMove = MOVE_WILL_O_WISP; gBattlerAttacker = 0; gBattlerTarget = 1;
    sStatusCueMode = sStatusPolicyMode = 0; probe->actions = 1;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    gBattleStruct->passiveHpUpdate[1] = 7;
    RunToStatusReaction(); PlayMixedStatusReaction();
    CHECK(gBattleMons[1].hp == expectedHp && GetMonData(GetBattlerMon(1), MON_DATA_HP) == expectedHp
        && gBattleMons[1].statStages[STAT_DEF] == DEFAULT_STAT_STAGE + 1
        && gBattleStruct->passiveHpUpdate[1] == 7 && probe->attempts == 17);
    /* Heal Block and a busy stat queue skip their actions at playback. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].status1 = STATUS1_BURN; gBattleMons[1].item = ITEM_RAWST_BERRY;
    gBattleMons[1].hp = lowHp; SetMonData(GetBattlerMon(1), MON_DATA_HP, &lowHp);
    gBattleMons[1].statStages[STAT_DEF] = DEFAULT_STAT_STAGE;
    ExpectStatusOrigin(RHH_BATTLER_ITEM_STATUS_CHANGED, RhhContentKeyForId(RHH_ITEM, ITEM_RAWST_BERRY), 1);
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    CHECK(ItemBattleEffects(1, 0, HOLD_EFFECT_CURE_BRN, IsOnStatusChangeActivation) == ITEM_STATUS_CHANGE);
    gBattleMons[1].volatiles.healBlockTimer = 1; gBattleStruct->statChangeBattler = 1;
    RunToStatusReaction(); PlayStatusReaction(1);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && gBattleMons[1].hp == 1
        && gBattleMons[1].statStages[STAT_DEF] == DEFAULT_STAT_STAGE && probe->attempts == 18);
    /* A changed participant skips all three queued actions. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].hp = lowHp; SetMonData(GetBattlerMon(1), MON_DATA_HP, &lowHp);
    gBattleMons[1].statStages[STAT_DEF] = DEFAULT_STAT_STAGE; gCurrentMove = MOVE_WILL_O_WISP;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    ModsBattleSwitchPrepared(1); RunToStatusReaction(); PlayStatusReaction(0);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && gBattleMons[1].hp == 1
        && gBattleMons[1].statStages[STAT_DEF] == DEFAULT_STAT_STAGE && probe->attempts == 19);
    /* Nested condition observers can use the same gameplay queue. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[1].ability = ABILITY_NONE; gBattleMons[1].hp = lowHp; SetMonData(GetBattlerMon(1), MON_DATA_HP, &lowHp);
    gBattleMons[1].statStages[STAT_DEF] = DEFAULT_STAT_STAGE; gCurrentMove = MOVE_WILL_O_WISP;
    expectedHp = min(gBattleMons[1].maxHP, 1 + max(1, GetNonDynamaxMaxHP(1) / 4));
    sStatusCueMode = 4; probe->actions = 0;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction(); PlayMixedStatusReaction();
    CHECK(gBattleMons[1].hp == expectedHp && gBattleMons[1].statStages[STAT_DEF] == DEFAULT_STAT_STAGE + 1
        && probe->attempts == 20);
    /* A sourced stat change and a status can trigger a nested reaction. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[0].ability = gBattleMons[1].ability = gBattleMons[2].ability = ABILITY_NONE;
    gBattleMons[1].statStages[STAT_ATK] = DEFAULT_STAT_STAGE;
    gBattleMons[2].types[0] = gBattleMons[2].types[1] = TYPE_WATER; gBattleMons[2].types[2] = TYPE_MYSTERY;
    gCurrentMove = MOVE_WILL_O_WISP; gBattlerAttacker = 0; gBattlerTarget = 1;
    sStatusCueMode = 0; probe->actions = 2;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction(); gBattleScripting.battler = 3; gBattleStruct->moldBreakerActive = TRUE;
    PlaySourcedStatusReaction(TRUE);
    CHECK(gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE - 1
        && (gBattleMons[2].status1 & STATUS1_POISON) && probe->nested == 1 && probe->attempts == 21);
    /* Clear Body and Immunity apply despite a previous Mold Breaker flag. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[0].ability = ABILITY_NONE; gBattleMons[1].ability = ABILITY_CLEAR_BODY; gBattleMons[2].ability = ABILITY_IMMUNITY;
    gBattleMons[1].statStages[STAT_ATK] = DEFAULT_STAT_STAGE;
    gBattleMons[2].types[0] = gBattleMons[2].types[1] = TYPE_WATER; gBattleMons[2].types[2] = TYPE_MYSTERY;
    gCurrentMove = MOVE_WILL_O_WISP; gBattlerAttacker = 0; gBattlerTarget = 1;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction(); gBattleScripting.battler = 3; gBattleStruct->moldBreakerActive = TRUE;
    PlaySourcedStatusReaction(FALSE);
    CHECK(gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE && !gBattleMons[2].status1
        && probe->nested == 1 && probe->attempts == 22);
    /* Changing the source invalidates both queued actions. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[0].ability = gBattleMons[1].ability = gBattleMons[2].ability = ABILITY_NONE;
    gBattleMons[1].statStages[STAT_ATK] = DEFAULT_STAT_STAGE; gCurrentMove = MOVE_WILL_O_WISP;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    ModsBattleSwitchPrepared(0); RunToStatusReaction(); PlayStatusReaction(0);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE
        && !gBattleMons[2].status1 && probe->nested == 1 && probe->attempts == 23);
    /* A replaced status recipient is skipped without suppressing the stat action. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[0].ability = gBattleMons[1].ability = gBattleMons[2].ability = ABILITY_NONE;
    gBattleMons[1].statStages[STAT_ATK] = DEFAULT_STAT_STAGE; gCurrentMove = MOVE_WILL_O_WISP;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    ModsBattleSwitchPrepared(2); RunToStatusReaction(); PlaySourcedStatusReaction(FALSE);
    CHECK(gBattleMons[1].statStages[STAT_ATK] == DEFAULT_STAT_STAGE - 1
        && !gBattleMons[2].status1 && probe->nested == 1 && probe->attempts == 24);
    /* An infliction does not run when its status presentation cannot be pushed. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[0].ability = gBattleMons[1].ability = gBattleMons[2].ability = ABILITY_NONE;
    gCurrentMove = MOVE_WILL_O_WISP; probe->actions = 3;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction();
    for (u32 i = 0; i < ARRAY_COUNT(gBattleResources->battleScriptsStack->ptr); ++i)
        gBattleResources->battleScriptsStack->ptr[i] = sStatusReactionResume;
    gBattleResources->battleScriptsStack->size = ARRAY_COUNT(gBattleResources->battleScriptsStack->ptr);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReaction);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && !gBattleMons[2].status1 && probe->attempts == 25
        && gBattleResources->battleScriptsStack->size == ARRAY_COUNT(gBattleResources->battleScriptsStack->ptr));
    /* Indirect reaction damage uses the HP controller and preserves Substitute. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[0].ability = gBattleMons[1].ability = gBattleMons[2].ability = ABILITY_NONE;
    gBattleMons[2].volatiles.substitute = TRUE; gBattleMons[2].volatiles.substituteHP = 9;
    u32 maxHp = gBattleMons[2].maxHP, damage = max(1, GetNonDynamaxMaxHP(2) / 4);
    gCurrentMove = MOVE_WILL_O_WISP; probe->actions = 4; gHitMarker = 0;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction(); gBattleScripting.battler = 3; gBattleStruct->passiveHpUpdate[2] = 7;
    gBattleControllerExecFlags = 1; gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReaction && gBattleMons[2].hp == maxHp
        && gBattleStruct->passiveHpUpdate[2] == 7); gBattleControllerExecFlags = 0;
    PlayStatusDamage(maxHp - damage, FALSE);
    CHECK(gBattleMons[2].hp == maxHp - damage && gBattleMons[2].volatiles.substituteHP == 9 && probe->attempts == 26);
    /* Magic Guard blocks unless the caller opts out. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[2].ability = ABILITY_MAGIC_GUARD; gCurrentMove = MOVE_WILL_O_WISP; gHitMarker = 0;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction(); PlayStatusReaction(0);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && gBattleMons[2].hp == maxHp && probe->attempts == 27);
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[2].ability = ABILITY_MAGIC_GUARD; gCurrentMove = MOVE_WILL_O_WISP; probe->actions = 5; gHitMarker = 0;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction(); gBattleScripting.battler = 3; gBattleStruct->passiveHpUpdate[2] = 7;
    PlayStatusDamage(maxHp - damage, FALSE); CHECK(gBattleMons[2].hp == maxHp - damage && probe->attempts == 28);
    /* Lethal damage executes the engine's faint presentation. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[2].ability = ABILITY_NONE; lowHp = 1; gBattleMons[2].hp = lowHp; SetMonData(GetBattlerMon(2), MON_DATA_HP, &lowHp);
    gCurrentMove = MOVE_WILL_O_WISP; probe->actions = 4; gHitMarker = 0;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction(); gBattleScripting.battler = 3; gBattleStruct->passiveHpUpdate[2] = 7;
    PlayStatusDamage(0, TRUE); CHECK(!gBattleMons[2].hp && gBattleStruct->battlerState[2].notOnField && probe->attempts == 29);
    /* HP-threshold items activate for the actual recipient after damage. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[2].ability = ABILITY_NONE; u16 berry = ITEM_ORAN_BERRY; gBattleMons[2].item = berry;
    SetMonData(GetBattlerMon(2), MON_DATA_HELD_ITEM, &berry);
    u16 beforeDamage = maxHp / 2 + damage; gBattleMons[2].hp = beforeDamage;
    SetMonData(GetBattlerMon(2), MON_DATA_HP, &beforeDamage);
    gCurrentMove = MOVE_WILL_O_WISP; gHitMarker = 0;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction(); gBattleScripting.battler = 3; gBattleStruct->passiveHpUpdate[2] = 7;
    PlayStatusDamage(maxHp / 2, FALSE);
    CHECK(gBattleMons[2].hp == min(maxHp, maxHp / 2 + 10) && gBattleMons[2].item == ITEM_NONE && probe->attempts == 30);
    /* A switched recipient does not receive queued damage. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[2].ability = ABILITY_NONE; gCurrentMove = MOVE_WILL_O_WISP; gHitMarker = 0;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    ModsBattleSwitchPrepared(2); RunToStatusReaction(); PlayStatusReaction(0);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && gBattleMons[2].hp == maxHp && probe->attempts == 31);
    /* A selected cure emits a committed event; its nested cue runs afterward. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[0].ability = gBattleMons[1].ability = ABILITY_NONE;
    gCurrentMove = MOVE_WILL_O_WISP; probe->actions = 6; sStatusCueMode = sStatusPolicyMode = 0; gHitMarker = 0;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction(); gBattleScripting.battler = 3;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_CURED, 0, RHH_BATTLE_NO_BATTLER); sStatusPolicyMode = 1; sStatusCueMode = 1;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReactionCure && !gBattleMons[1].status1
        && gBattleControllerExecFlags && probe->cured == 1 && probe->attempts == 32);
    BtlController_HandleSetMonData(1); CHECK(!gBattleControllerExecFlags && !GetMonData(GetBattlerMon(1), MON_DATA_STATUS));
    RunToStatusReaction();
    for (u32 i = 0; i < 2; ++i) {
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_PLAYANIMATION_VAR);
        gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION_VAR]();
        CHECK(gBattleControllerExecFlags && gBattleResources->bufferA[1][0] == CONTROLLER_BATTLEANIMATION
            && gBattleResources->bufferA[1][1] == B_ANIM_PROTECTED_ITSELF);
        gBattleControllerExecFlags = 0; gBattleScriptingCommandsTable[B_SCR_OP_WAITSTATE]();
        gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReaction);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && gBattleScripting.battler == 3
        && sStatusConditionBattle->status_cure_context(&outsideCure) && !memcmp(&outsideCure, &savedCure, sizeof(outsideCure)));
    /* A mask that does not match the current condition produces no cure event. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[0].ability = gBattleMons[1].ability = ABILITY_NONE;
    gCurrentMove = MOVE_WILL_O_WISP; probe->actions = 7; sStatusPolicyMode = sStatusCueMode = 0; gHitMarker = 0;
    ExpectStatusOrigin(RHH_BATTLER_STATUS_APPLIED, RhhContentKeyForId(RHH_MOVE, MOVE_WILL_O_WISP), 0);
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction(); PlayStatusReaction(0);
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && (gBattleMons[1].status1 & STATUS1_BURN)
        && probe->cured == 1 && probe->attempts == 33);
    /* Selective cures clean sleep, toxic and freeze metadata, preserving burn. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[0].ability = gBattleMons[1].ability = gBattleMons[2].ability = ABILITY_NONE;
    u32 multiple = STATUS1_SLEEP_TURN(2) | STATUS1_TOXIC_POISON | STATUS1_TOXIC_COUNTER | STATUS1_FREEZE | STATUS1_BURN;
    gBattleMons[2].status1 = multiple; SetMonData(GetBattlerMon(2), MON_DATA_STATUS, &multiple);
    gBattleMons[2].volatiles.nightmare = TRUE; GetBattlerPartyState(2)->freezeTurns = 2;
    TryActivateSleepClause(2, gBattlerPartyIndexes[2]);
    gCurrentMove = MOVE_WILL_O_WISP; probe->actions = 8; gHitMarker = 0;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    SetNonVolatileStatus(0, 1, MOVE_EFFECT_BURN, sStatusReactionResume, TRIGGER_ON_MOVE);
    RunToStatusReaction(); gBattleScripting.battler = 3;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReactionCure && gBattleMons[2].status1 == STATUS1_BURN
        && !gBattleMons[2].volatiles.nightmare && !GetBattlerPartyState(2)->freezeTurns
        && (!IsSleepClauseEnabled() || !IsSleepClauseActiveForSide(GetBattlerSide(2)))
        && gBattleControllerExecFlags && probe->cured == 2 && probe->attempts == 34);
    BtlController_HandleSetMonData(2); CHECK(!gBattleControllerExecFlags && GetMonData(GetBattlerMon(2), MON_DATA_STATUS) == STATUS1_BURN);
    RunToStatusReaction(); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && gBattleScripting.battler == 3);
    /* An item-status event can originate a cure of another participant. */
    ModsBattleReset(); PrepareTurnBattle(); memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses)); ModsBattleBegin();
    gBattleMons[1].status1 = STATUS1_BURN; gBattleMons[1].item = ITEM_RAWST_BERRY;
    u32 poison = STATUS1_POISON; gBattleMons[2].status1 = poison; SetMonData(GetBattlerMon(2), MON_DATA_STATUS, &poison);
    probe->actions = 9; sStatusPolicyMode = sStatusCueMode = 0; gHitMarker = 0;
    ExpectStatusOrigin(RHH_BATTLER_ITEM_STATUS_CHANGED, RhhContentKeyForId(RHH_ITEM, ITEM_RAWST_BERRY), 1);
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    CHECK(ItemBattleEffects(1, 0, HOLD_EFFECT_CURE_BRN, IsOnStatusChangeActivation) == ITEM_STATUS_CHANGE);
    RunToStatusReaction(); gBattleScripting.battler = 3; gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReactionCure && !gBattleMons[2].status1
        && gBattleControllerExecFlags && probe->cured == 3 && probe->attempts == 35);
    BtlController_HandleSetMonData(2); CHECK(!gBattleControllerExecFlags && !GetMonData(GetBattlerMon(2), MON_DATA_STATUS));
    RunToStatusReaction(); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == sStatusReactionResume && gBattleScripting.battler == 3);
    /* Engine immunity, Hydration and Shed Skin expose their actual ability. */
    CheckAbilityCure(probe, ABILITY_IMMUNITY, 10, STATUS1_POISON);
    CheckAbilityCure(probe, ABILITY_HYDRATION, 11, STATUS1_BURN);
    CheckAbilityCure(probe, ABILITY_SHED_SKIN, 12, STATUS1_PARALYSIS);
    CHECK(probe->cured == 6 && probe->attempts == 35);
    /* Natural recovery, self-thaw, Battle Palace and thawing hits. */
    CheckEngineStatusCure(probe, 13);
    CheckEngineStatusCure(probe, 18);
    CheckEngineStatusCure(probe, 14);
    CheckEngineStatusCure(probe, 15);
    CheckEngineStatusCure(probe, 16);
    CheckEngineStatusCure(probe, 17);
    CHECK(probe->cured == 12 && probe->attempts == 35);
    /* A sleep counter decrement is not a cure. */
    ModsBattleReset(); PrepareTurnBattle(); ModsBattleBegin(); gBattleMons[1].ability = ABILITY_NONE;
    gBattleMons[1].status1 = STATUS1_SLEEP_TURN(2); gBattlerAttacker = 1; gBattlerTarget = 0; gCurrentMove = MOVE_TACKLE;
    gBattleStruct->eventState.atkCanceler = CANCELER_ASLEEP_OR_FROZEN; probe->actions = 13;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    CHECK(DoAttackCanceler() == CANCELER_RESULT_FAILURE && (gBattleMons[1].status1 & STATUS1_SLEEP)
        && probe->cured == 12 && !gBattleResources->battleScriptsStack->size);
    /* Volatile-only immunity and link battles do not publish major-status cures. */
    ModsBattleReset(); PrepareTurnBattle(); ModsBattleBegin(); gBattleMons[1].ability = ABILITY_OWN_TEMPO;
    gBattleMons[1].volatiles.confusionTimer = 2; probe->actions = 10;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    CHECK(AbilityBattleEffects(ABILITYEFFECT_IMMUNITY, 1, ABILITY_OWN_TEMPO, MOVE_NONE, TRUE) == IMMUNITY_CONFUSION_CLEARED
        && probe->cured == 12 && gBattleResources->battleScriptsStack->size == 1
        && gBattleResources->battleScriptsStack->ptr[0] == sStatusReactionResume);
    ModsBattleReset(); PrepareTurnBattle(); ModsBattleBegin(); gBattleTypeFlags |= BATTLE_TYPE_LINK;
    gBattleMons[1].ability = ABILITY_IMMUNITY; gBattleMons[1].status1 = 0;
    gBattleResources->battleScriptsStack->size = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = sStatusReactionResume;
    ModsBattleAbilityStatusCured(1, ABILITY_IMMUNITY, STATUS1_POISON, RHH_BATTLE_CURE_IMMUNITY_ABILITY, FALSE);
    ModsBattleEngineStatusCured(1, STATUS1_SLEEP_TURN(1), RHH_BATTLE_CURE_NATURAL, RHH_BATTLE_NO_BATTLER, MOVE_NONE, FALSE);
    CHECK(probe->cured == 12 && !gBattleResources->battleScriptsStack->size);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    /* Purify reports before its presentation and resumes at the heal command. */
    PrepareScriptCure(probe, 19, MOVE_PURIFY);
    CommitScriptCure(1, BattleScript_EffectPurify, BS_ModMoveCureBegin);
    CHECK(probe->cured == 13); RunToStatusReaction(); PlayStatusReaction(2);
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_TRYHEALHALFHEALTH);
    /* Jungle Healing preserves its original source while the script changes attacker. */
    PrepareScriptCure(probe, 20, MOVE_JUNGLE_HEALING);
    gBattlescriptCurrInstr = FindScriptCureMarker(BattleScript_EffectJungleHealing, BS_ModCaptureJungleCureActor);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); gBattlerAttacker = gBattlerTarget = 2;
    CommitScriptCure(2, BattleScript_EffectJungleHealing, BS_ModJungleCureBegin);
    CHECK(probe->cured == 14); RunToStatusReaction(); PlayScriptCureCue(2);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr != BattleScript_ModStatusReaction && gBattlerAttacker == 2);
    /* Healer and Pastel Veil identify the owner rather than the cured partner. */
    PrepareScriptCure(probe, 24, MOVE_TACKLE); gBattleScripting.battler = 2;
    CommitScriptCure(2, BattleScript_HealerActivates, BS_ModHealerCureBegin);
    CHECK(probe->cured == 15); RunToStatusReaction(); PlayScriptCureCue(2);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(*gBattlescriptCurrInstr == B_SCR_OP_RETURN);
    PrepareScriptCure(probe, 25, MOVE_TACKLE); gBattlerTarget = 0;
    gBattlescriptCurrInstr = FindScriptCureMarker(BattleScript_PastelVeilActivates, BS_ModCapturePastelCureActor);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); gBattlerTarget = 2;
    CommitScriptCure(2, BattleScript_PastelVeilActivates, BS_ModPastelCureBegin);
    CHECK(probe->cured == 16); RunToStatusReaction(); PlayScriptCureCue(2);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr != BattleScript_ModStatusReaction);
    /* Psycho Shift and Refresh cure the move user. */
    PrepareScriptCure(probe, 22, MOVE_PSYCHO_SHIFT);
    CommitScriptCure(0, BattleScript_EffectPsychoShift, BS_ModMoveCureBegin);
    CHECK(probe->cured == 17); RunToStatusReaction(); PlayScriptCureCue(0);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
    PrepareScriptCure(probe, 23, MOVE_REFRESH);
    CommitScriptCure(0, BattleScript_EffectRefresh, BS_ModMoveCureBegin);
    CHECK(probe->cured == 18); RunToStatusReaction(); PlayScriptCureCue(0);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
    /* Heal Bell reports both active party members and plays their cues in order. */
    PrepareScriptCure(probe, 21, MOVE_HEAL_BELL); probe->scriptOrder = 0;
    u32 burn = STATUS1_BURN; gBattleMons[2].status1 = burn; SetMonData(GetBattlerMon(2), MON_DATA_STATUS, &burn);
    CommitScriptCure(0, BattleScript_EffectHealBell, BS_ModMoveCureBegin);
    CHECK(!gBattleMons[2].status1 && probe->cured == 20 && probe->scriptOrder == 2);
    RunToStatusReaction(); PlayScriptCureCue(0); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReaction); PlayScriptCureCue(2);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
    /* Replacing the captured actor leaves the move source unknown. */
    PrepareScriptCure(probe, 26, MOVE_JUNGLE_HEALING);
    gBattlescriptCurrInstr = FindScriptCureMarker(BattleScript_EffectJungleHealing, BS_ModCaptureJungleCureActor);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); gBattlerPartyIndexes[0] ^= 1; gBattlerAttacker = gBattlerTarget = 2;
    CommitScriptCure(2, BattleScript_EffectJungleHealing, BS_ModJungleCureBegin);
    CHECK(probe->cured == 21); RunToStatusReaction(); PlayScriptCureCue(2);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr != BattleScript_ModStatusReaction);
    /* A failed Refresh cannot attribute an unrelated, unmarked cure. */
    PrepareScriptCure(probe, 23, MOVE_REFRESH); gBattleMons[0].status1 = 0;
    gBattlescriptCurrInstr = FindScriptCureMarker(BattleScript_EffectRefresh, BS_ModMoveCureBegin);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); gBattleScriptingCommandsTable[B_SCR_OP_CURESTATUSWITHMOVE]();
    CHECK(probe->cured == 21 && gBattlescriptCurrInstr != BattleScript_ModStatusReaction);
    struct __attribute__((packed)) { u8 op; void (*function)(void); u8 battler; } unmarked = {B_SCR_OP_CALLNATIVE, BS_CureStatus, BS_ATTACKER};
    gBattleMons[0].status1 = STATUS1_POISON; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = (const u8 *)&unmarked;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(!gBattleMons[0].status1 && probe->cured == 21 && gBattlescriptCurrInstr != BattleScript_ModStatusReaction);
    /* A nested status reaction preserves the inline script's scratch context. */
    PrepareScriptCure(probe, 27, MOVE_JUNGLE_HEALING);
    gBattlescriptCurrInstr = FindScriptCureMarker(BattleScript_EffectJungleHealing, BS_ModCaptureJungleCureActor);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); gBattlerAttacker = gBattlerTarget = 2;
    CommitScriptCure(2, BattleScript_EffectJungleHealing, BS_ModJungleCureBegin);
    const u8 *flush = FindScriptCureMarker(BattleScript_EffectJungleHealing, BS_ModScriptCureReactions);
    for (u32 step = 0; gBattlescriptCurrInstr != flush; ++step) {
        CHECK(step < 4096 && *gBattlescriptCurrInstr != B_SCR_OP_END);
        if (gBattleControllerExecFlags) gBattleControllerExecFlags = 0;
        u32 opcode = *gBattlescriptCurrInstr;
        if (opcode == B_SCR_OP_WAITMESSAGE) gBattleCommunication[MSG_DISPLAY] = MSG_DISPLAY_CONTINUE;
        gBattleScriptingCommandsTable[opcode](); AnimateSprites();
    }
    gBattleCommunication[0] = 0x31; gBattleCommunication[1] = 0x42; gBattleCommunication[MULTISTRING_CHOOSER] = 0x53;
    gEffectBattler = 3; gBattlerAbility = 3; gLastUsedAbility = ABILITY_BLAZE; gLastUsedItem = ITEM_ORAN_BERRY;
    gBattleScripting.savedBattler = 3; gBattleScripting.moveEffect = MOVE_EFFECT_BURN;
    gBattleTextBuff1[0] = 0x12; gBattleTextBuff2[0] = 0x34; gBattleTextBuff3[0] = 0x56;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReaction);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(gBattlescriptCurrInstr == BattleScript_MoveEffectParalysis
        && (gBattleMons[1].status1 & STATUS1_PARALYSIS) && gBattleControllerExecFlags && probe->cured == 22);
    BtlController_HandleSetMonData(1); CHECK(!gBattleControllerExecFlags);
    RunToStatusReaction(); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr != BattleScript_ModStatusReaction && gBattlerAttacker == 2 && gBattlerTarget == 2
        && gEffectBattler == 3 && gBattlerAbility == 3 && gLastUsedAbility == ABILITY_BLAZE && gLastUsedItem == ITEM_ORAN_BERRY
        && gBattleCommunication[0] == 0x31 && gBattleCommunication[1] == 0x42 && gBattleCommunication[MULTISTRING_CHOOSER] == 0x53
        && gBattleScripting.savedBattler == 3 && gBattleScripting.moveEffect == MOVE_EFFECT_BURN
        && gBattleTextBuff1[0] == 0x12 && gBattleTextBuff2[0] == 0x34 && gBattleTextBuff3[0] == 0x56);
    /* Lunar Blessing uses the same ally cure script and preserves the source. */
    PrepareScriptCure(probe, 20, MOVE_LUNAR_BLESSING); sCureObserverEnabled = TRUE; sCureObserverCount = 0;
    gBattlescriptCurrInstr = FindScriptCureMarker(BattleScript_EffectJungleHealing, BS_ModCaptureJungleCureActor);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); gBattlerAttacker = gBattlerTarget = 2;
    CommitScriptCure(2, BattleScript_EffectJungleHealing, BS_ModJungleCureBegin);
    sCureObserverEnabled = FALSE; CHECK(probe->cured == 23 && sCureObserverCount == 1);
    RunToStatusReaction(); PlayScriptCureCue(2); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr != BattleScript_ModStatusReaction);
    /* The Aromatherapy effect also preserves two-recipient order. */
    PrepareScriptCure(probe, 21, MOVE_AROMATHERAPY); probe->scriptOrder = 0; sCureObserverEnabled = TRUE; sCureObserverCount = 0;
    burn = STATUS1_BURN; gBattleMons[2].status1 = burn; SetMonData(GetBattlerMon(2), MON_DATA_STATUS, &burn);
    CommitScriptCure(0, BattleScript_MoveEffectAromatherapy, BS_ModMoveCureBegin);
    sCureObserverEnabled = FALSE; CHECK(!gBattleMons[2].status1 && probe->cured == 25 && probe->scriptOrder == 2 && sCureObserverCount == 2);
    RunToStatusReaction(); PlayScriptCureCue(0); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReaction); PlayScriptCureCue(2);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE](); CHECK(*gBattlescriptCurrInstr == B_SCR_OP_RETURN);
    /* Heal Bell leaves a Soundproof partner unchanged and emits no event for it. */
    PrepareScriptCure(probe, 21, MOVE_HEAL_BELL); probe->scriptOrder = 0; gBattleMons[2].ability = ABILITY_SOUNDPROOF;
    burn = STATUS1_BURN; gBattleMons[2].status1 = burn; SetMonData(GetBattlerMon(2), MON_DATA_STATUS, &burn);
    CommitScriptCure(0, BattleScript_EffectHealBell, BS_ModMoveCureBegin);
    CHECK(gBattleMons[2].status1 == STATUS1_BURN && probe->cured == 26 && probe->scriptOrder == 1);
    RunToStatusReaction(); PlayScriptCureCue(0); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
    /* A benched cure is observed only after the controller and script wait. */
    PreparePartyCure(probe, 28); CHECK(!probe->partyCured);
    sPartyCureObserverEnabled = TRUE; sPartyCureObserverCount = 0;
    rng_value_t partyRng = gRngValue, partyRng2 = gRng2Value;
    BtlController_HandleSetMonData(0);
    CHECK(!gBattleControllerExecFlags && !GetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_STATUS) && !probe->partyCured);
    RunToStatusReaction(); sPartyCureObserverEnabled = FALSE;
    CHECK(probe->partyCured == 2 && sPartyCureObserverCount == 2
        && !memcmp(&sPartyCureObserverRng, &partyRng, sizeof(partyRng))
        && !memcmp(&sPartyCureObserverRng2, &partyRng2, sizeof(partyRng2))
        && !memcmp(&gRngValue, &partyRng, sizeof(partyRng)) && !memcmp(&gRng2Value, &partyRng2, sizeof(partyRng2)));
    PlayScriptCureCue(1); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModStatusReaction); PlayScriptCureCue(1);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr != BattleScript_ModStatusReaction);
    /* A replacement before or after the controller must not inherit the receipt. */
    for (u32 replaceAfter = 0; replaceAfter < 2; ++replaceAfter) {
        PreparePartyCure(probe, 29);
        if (!replaceAfter) CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][3], SPECIES_EEVEE, 20, 987, OTID_STRUCT_PRESET(222), 31);
        BtlController_HandleSetMonData(0);
        if (replaceAfter) CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][3], SPECIES_EEVEE, 20, 987, OTID_STRUCT_PRESET(222), 31);
        FlushPartyCure(); CHECK(probe->partyCured == 2 && gBattlescriptCurrInstr != BattleScript_ModStatusReaction);
    }
    /* Clearing the wait flag without running the controller is not a cure. */
    PreparePartyCure(probe, 30); gBattleControllerExecFlags = 0; FlushPartyCure();
    CHECK(probe->partyCured == 2 && GetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_STATUS) == STATUS1_POISON);
    /* A different controller write is not attributed to the captured cure. */
    PreparePartyCure(probe, 30); gBattleResources->bufferA[0][3] = 1;
    BtlController_HandleSetMonData(0); FlushPartyCure(); CHECK(probe->partyCured == 2);
    /* A changed actor does not become the source of an otherwise valid cure. */
    PreparePartyCure(probe, 32); BtlController_HandleSetMonData(0); gBattlerPartyIndexes[0] = 2;
    sStatusExpectedActor = RHH_BATTLE_NO_BATTLER;
    FlushPartyCure(); CHECK(probe->partyCured == 3 && gBattlescriptCurrInstr == BattleScript_ModStatusReaction);
    PlayScriptCureCue(1); gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr != BattleScript_ModStatusReaction);
    /* A link transition cancels a captured local receipt. */
    PreparePartyCure(probe, 30); gBattleTypeFlags |= BATTLE_TYPE_LINK;
    BtlController_HandleSetMonData(0); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    FlushPartyCure(); CHECK(probe->partyCured == 3);
    CheckSwitchAndUproarCures(probe);
    CheckEngineStatusCure(probe, 35);
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gBattleSpritesDataPtr = NULL;
    gAiLogicData = NULL; gAiPartyData = NULL; gAiThinkingStruct = NULL; gBattleHistory = NULL;
    RhhModsUnloadLibraries(); puts("status conditions: proposals, revalidation and innate reactions passed");
}
