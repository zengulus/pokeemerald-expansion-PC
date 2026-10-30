/* Exercise committed ability mutations through the engine's script commands. */
extern void BS_TryEntrainment(void);
extern void BS_SetTracedAbility(void);
static const struct RhhAbilityService *sMutationAbilities;
static struct RhhBattlerAbilityMutation sMutationEvents[16];
static u32 sMutationCount;
static struct RhhBattlerAbilityContactMutation sContactEvents[16];
static u32 sContactCount;
static void TestAbilityMutation(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattlerAbilityMutation value = {.struct_size = sizeof(value)};
    struct RhhBattlerAbilityMutation small = {.struct_size = 1, .sequence = 99}, saved = small;
    struct RhhBattlerAbility current = {.struct_size = sizeof(current)};
    CHECK(sMutationCount < ARRAY_COUNT(sMutationEvents) && !sMutationAbilities->ability_mutation_context(&value)
        && sMutationAbilities->ability_mutation_context(&small) && !memcmp(&small, &saved, sizeof(small))
        && value.sequence == event->ordinal && value.cause == event->args[0]
        && value.participant.battler == event->args[1] && value.source_battler == event->args[2]
        && value.batch_size == event->args[3] && value.participant.identity == event->subject
        && value.move == event->event_key && !RhhModsCommandsAllowed()
        && !sMutationAbilities->battler(value.participant.battler, &current)
        && current.stored == value.after.stored && current.active_ignoring_mold_breaker == value.after.active);
    sMutationEvents[sMutationCount++] = value;
    (void)Random(); (void)Random2(); /* Observer cannot perturb gameplay RNG. */
}
static void TestContactAbilityMutation(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattlerAbilityContactMutation value = {.struct_size = sizeof(value)};
    struct RhhBattlerAbilityContactMutation small = {.struct_size = 1, .triggering_ability = 99}, saved = small;
    struct RhhBattlerAbilityMutation generic = {.struct_size = sizeof(generic)};
    struct RhhBattlerAbility current = {.struct_size = sizeof(current)};
    CHECK(sContactCount < ARRAY_COUNT(sContactEvents) && !sMutationAbilities->ability_contact_mutation_context(&value)
        && sMutationAbilities->ability_contact_mutation_context(&small) && !memcmp(&small, &saved, sizeof(small))
        && !sMutationAbilities->ability_mutation_context(&generic) && generic.sequence == value.change.sequence
        && generic.battle_id == value.change.battle_id && generic.cause == value.change.cause
        && generic.participant.battler == value.change.participant.battler && generic.after.stored == value.change.after.stored
        && value.change.sequence == event->ordinal && value.change.cause == event->args[0]
        && value.change.participant.battler == event->args[1] && value.change.source_battler == event->args[2]
        && value.change.batch_size == event->args[3] && value.change.move == event->event_key
        && !RhhModsCommandsAllowed() && !sMutationAbilities->battler(value.change.participant.battler, &current)
        && current.stored == value.change.after.stored && current.active_ignoring_mold_breaker == value.change.after.active);
    sContactEvents[sContactCount++] = value;
    (void)Random(); (void)Random2();
}
static const struct RhhModHook sMutationHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLER_ABILITY_MUTATED, RHH_OBSERVE, 0, NULL, TestAbilityMutation},
    {sizeof(struct RhhModHook), RHH_BATTLER_ABILITY_CONTACT_MUTATED, RHH_OBSERVE, 0, NULL, TestContactAbilityMutation}
};
static const struct RhhModDefinition sMutationMod = {
    .struct_size = sizeof(sMutationMod), .abi_version = RHH_MOD_ABI_V1, .id = "test:ability_mutations",
    .hooks = sMutationHooks, .hook_count = ARRAY_COUNT(sMutationHooks)
};
static void RunMutationCommand(u32 opcode, u32 battler, bool32 copy)
{
    static const u8 failure[] = {0};
    struct __attribute__((packed)) { u8 op, battler; const u8 *fail; } copyCommand = {opcode, battler, failure};
    struct __attribute__((packed)) { u8 op; const u8 *fail; } command = {opcode, failure};
    gBattlescriptCurrInstr = copy ? (const u8 *)&copyCommand : (const u8 *)&command;
    gBattleScriptingCommandsTable[opcode]();
}
static void RunMutationNative(bool32 trace)
{
    static const u8 failure[] = {0};
    struct __attribute__((packed)) { u8 op; void (*func)(void); const u8 *fail; } entrain = {B_SCR_OP_CALLNATIVE, BS_TryEntrainment, failure};
    struct __attribute__((packed)) { u8 op; void (*func)(void); u8 battler; } traced = {B_SCR_OP_CALLNATIVE, BS_SetTracedAbility, BS_TARGET};
    gBattlescriptCurrInstr = trace ? (const u8 *)&traced : (const u8 *)&entrain;
    if (trace) BS_SetTracedAbility(); else BS_TryEntrainment();
}
static u32 RunContactAbility(u32 ability, u32 move);
static const struct RhhBattleService *sSuppressionBattle;
static const struct RhhAbilityService *sSuppressionAbilities;
static struct RhhBattleWeatherSuppressionChange sSuppressionEvents[8];
static u32 sSuppressionCount;
static void ObserveWeatherSuppression(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleWeatherSuppressionChange change = {.struct_size = sizeof(change)};
    struct RhhBattleWeatherSuppressionChange tiny = {.struct_size = 1, .cause = 99}, saved = tiny;
    struct RhhBattleEnvironmentState live = {.struct_size = sizeof(live)};
    struct RhhBattlerAbility ability = {.struct_size = sizeof(ability)};
    CHECK(sSuppressionCount < ARRAY_COUNT(sSuppressionEvents) && !sSuppressionBattle->weather_suppression_change(&change)
        && sSuppressionBattle->weather_suppression_change(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny))
        && !sSuppressionBattle->environment(&live) && live.weather_suppressed == change.after.weather_suppressed
        && change.sequence == sSuppressionCount && event->ordinal == change.sequence && event->subject == change.after.battle_id
        && event->args[0] == change.cause && event->args[1] == change.source_battler
        && event->args[2] == change.before.weather_suppressed && event->args[3] == change.after.weather_suppressed
        && event->event_key == (change.move ? change.move : change.triggering_ability)
        && change.target_mask && !sSuppressionAbilities->battler((change.target_mask & 1) ? 0 : 1, &ability)
        && !RhhModsCommandsAllowed());
    sSuppressionEvents[sSuppressionCount++] = change;
    (void)Random(); (void)Random2();
}
static const struct RhhModHook sSuppressionHook = {sizeof(struct RhhModHook), RHH_BATTLE_WEATHER_SUPPRESSION_CHANGED,
    RHH_OBSERVE, 0, NULL, ObserveWeatherSuppression};
static const struct RhhModDefinition sSuppressionMod = {.struct_size = sizeof(sSuppressionMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:weather_suppression", .hooks = &sSuppressionHook, .hook_count = 1};
static void CheckWeatherSuppressionMutations(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sSuppressionMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sSuppressionBattle = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 37));
    sSuppressionAbilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 36));
    struct RhhBattleWeatherSuppressionChange outside = {.struct_size = sizeof(outside), .cause = 99}, saved = outside;
    CHECK(sSuppressionBattle && sSuppressionAbilities && RHH_BATTLE_SERVICE_V1_36_SIZE < RHH_BATTLE_SERVICE_V1_37_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 46))
        && sSuppressionBattle->weather_suppression_change(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct BattleHistory history = {0}; struct AiPartyData parties = {0}; struct AiLogicData ai = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources;
    gBattleHistory = &history; gAiPartyData = &parties; gAiLogicData = &ai;
    PrepareTurnBattle(); gBattlersCount = 2; gBattleTypeFlags &= ~BATTLE_TYPE_DOUBLE;
    memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses));
    gBattlerAttacker = 0; gBattlerTarget = 1; gBattleMons[0].ability = ABILITY_BLAZE;
    gBattleMons[1].ability = ABILITY_CLOUD_NINE; gBattleMons[0].item = gBattleMons[1].item = ITEM_NONE;
    gBattleWeather = B_WEATHER_RAIN_NORMAL; gBattleStruct->weatherDuration = 5;
    ModsBattleBegin(); sSuppressionCount = 0;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    gCurrentMove = MOVE_GASTRO_ACID; RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sSuppressionCount == 1 && sSuppressionEvents[0].cause == RHH_ABILITY_MUTATION_SUPPRESS
        && sSuppressionEvents[0].source_battler == 0 && sSuppressionEvents[0].target_mask == (1u << 1)
        && sSuppressionEvents[0].move == RhhContentKeyForId(RHH_MOVE, MOVE_GASTRO_ACID)
        && sSuppressionEvents[0].before.weather_suppressed && !sSuppressionEvents[0].after.weather_suppressed
        && sSuppressionEvents[0].after.effective_weather == RHH_WEATHER_RAIN);
    RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE); CHECK(sSuppressionCount == 1);
    gBattleMons[0].ability = ABILITY_BLAZE; gBattleMons[1].ability = ABILITY_CLOUD_NINE;
    gCurrentMove = MOVE_ROLE_PLAY; RunMutationCommand(B_SCR_OP_TRYCOPYABILITY, BS_ATTACKER, TRUE);
    CHECK(sSuppressionCount == 2 && sSuppressionEvents[1].cause == RHH_ABILITY_MUTATION_COPY
        && !sSuppressionEvents[1].before.weather_suppressed && sSuppressionEvents[1].after.weather_suppressed);
    gBattleMons[0].ability = ABILITY_AIR_LOCK; gBattleMons[1].volatiles.gastroAcid = FALSE;
    gCurrentMove = MOVE_GASTRO_ACID; RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE); CHECK(sSuppressionCount == 2);
    gBattleMons[0].ability = ABILITY_CLOUD_NINE; gBattleMons[1].ability = ABILITY_BLAZE;
    gBattleMons[1].volatiles.gastroAcid = FALSE; gBattleStruct->moveResultFlags[1] = 0; gCurrentMove = MOVE_SKILL_SWAP;
    RunMutationCommand(B_SCR_OP_TRYSWAPABILITIES, 0, FALSE); CHECK(sSuppressionCount == 2);
    gBattleMons[0].ability = ABILITY_CLOUD_NINE; gBattleMons[0].volatiles.overwrittenAbility = 0;
    gBattleMons[1].ability = ABILITY_MUMMY; gBattleMons[1].volatiles.gastroAcid = FALSE;
    gCurrentMove = MOVE_TACKLE; gSpecialStatuses[1].damagedByAttack = TRUE;
    CHECK(RunContactAbility(ABILITY_MUMMY, MOVE_NONE) && sSuppressionCount == 3
        && sSuppressionEvents[2].cause == RHH_ABILITY_MUTATION_CONTACT_COPY && sSuppressionEvents[2].source_battler == 1
        && sSuppressionEvents[2].target_mask == (1u << 0)
        && sSuppressionEvents[2].triggering_ability == RhhContentKeyForId(RHH_ABILITY, ABILITY_MUMMY)
        && sSuppressionEvents[2].before.weather_suppressed && !sSuppressionEvents[2].after.weather_suppressed);
    gBattleMons[0].ability = ABILITY_BLAZE; gBattleMons[0].volatiles.gastroAcid = TRUE;
    gBattleMons[1].ability = ABILITY_CLOUD_NINE; gBattleMons[1].volatiles.gastroAcid = FALSE;
    gBattleStruct->moveResultFlags[1] = 0; gCurrentMove = MOVE_SKILL_SWAP;
    RunMutationCommand(B_SCR_OP_TRYSWAPABILITIES, 0, FALSE);
    CHECK(sSuppressionCount == 4 && sSuppressionEvents[3].cause == RHH_ABILITY_MUTATION_SWAP
        && sSuppressionEvents[3].target_mask == 3 && sSuppressionEvents[3].before.weather_suppressed
        && !sSuppressionEvents[3].after.weather_suppressed);
    gBattleMons[0].volatiles.gastroAcid = FALSE;
    gBattleMons[0].ability = ABILITY_BLAZE; gBattleMons[1].ability = ABILITY_CLOUD_NINE;
    gBattleMons[1].volatiles.gastroAcid = FALSE; gBattleTypeFlags |= BATTLE_TYPE_LINK; gCurrentMove = MOVE_GASTRO_ACID;
    RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE); CHECK(sSuppressionCount == 4);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; gBattleMons[1].volatiles.gastroAcid = FALSE; ai.aiCalcInProgress = TRUE;
    RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE); CHECK(sSuppressionCount == 4); ai.aiCalcInProgress = FALSE;
    gBattleMons[1].volatiles.gastroAcid = FALSE; gBattleWeather = B_WEATHER_NONE;
    RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE); CHECK(sSuppressionCount == 4);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && sSuppressionBattle->weather_suppression_change(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gBattleHistory = NULL; gAiPartyData = NULL; gAiLogicData = NULL;
    RhhModsUnloadLibraries();
}
static void CheckAbilityMutations(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sMutationMod)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sMutationAbilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 28));
    CHECK(sMutationAbilities && sMutationAbilities->header.struct_size >= RHH_ABILITY_SERVICE_V1_28_SIZE
        && RHH_ABILITY_SERVICE_V1_27_SIZE < RHH_ABILITY_SERVICE_V1_28_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 41)));
    struct RhhBattlerAbilityMutation outside = {.struct_size = sizeof(outside), .sequence = 99}, saved = outside;
    CHECK(sMutationAbilities->ability_mutation_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources;
    PrepareTurnBattle(); gBattlersCount = 2; gBattleTypeFlags &= ~BATTLE_TYPE_DOUBLE;
    gBattlerAttacker = 0; gBattlerTarget = 1; gBattleMons[0].ability = ABILITY_BLAZE;
    gBattleMons[1].ability = ABILITY_LEVITATE; gBattleMons[0].item = gBattleMons[1].item = ITEM_NONE;
    ModsBattleBegin(); sMutationCount = 0;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    RhhContentKey blaze = RhhContentKeyForId(RHH_ABILITY, ABILITY_BLAZE), levitate = RhhContentKeyForId(RHH_ABILITY, ABILITY_LEVITATE);
    gCurrentMove = MOVE_GASTRO_ACID; RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sMutationCount == 1 && sMutationEvents[0].cause == RHH_ABILITY_MUTATION_SUPPRESS
        && sMutationEvents[0].participant.battler == 1 && sMutationEvents[0].before.stored == levitate
        && sMutationEvents[0].before.active == levitate && !sMutationEvents[0].after.active
        && !sMutationEvents[0].before_gastro_acid && sMutationEvents[0].after_gastro_acid
        && sMutationEvents[0].move == RhhContentKeyForId(RHH_MOVE, MOVE_GASTRO_ACID));
    RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE); CHECK(sMutationCount == 1);
    gBattleMons[1].volatiles.gastroAcid = FALSE; gBattleMons[1].ability = ABILITY_MULTITYPE;
    RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE); CHECK(sMutationCount == 1);
    gBattleMons[1].ability = ABILITY_LEVITATE; gCurrentMove = MOVE_ROLE_PLAY;
    RunMutationCommand(B_SCR_OP_TRYCOPYABILITY, BS_ATTACKER, TRUE);
    CHECK(sMutationCount == 2 && sMutationEvents[1].cause == RHH_ABILITY_MUTATION_COPY
        && sMutationEvents[1].participant.battler == 0 && sMutationEvents[1].before.stored == blaze
        && sMutationEvents[1].after.stored == levitate && sMutationEvents[1].source_battler == 0
        && sMutationEvents[1].move == RhhContentKeyForId(RHH_MOVE, MOVE_ROLE_PLAY));
    gBattleMons[0].ability = ABILITY_BLAZE; gBattleMons[0].volatiles.overwrittenAbility = 0;
    gBattleMons[1].ability = ABILITY_LEVITATE; gBattleMons[1].volatiles.overwrittenAbility = 0;
    gBattleStruct->moveResultFlags[1] = 0; gCurrentMove = MOVE_SKILL_SWAP;
    RunMutationCommand(B_SCR_OP_TRYSWAPABILITIES, 0, FALSE);
    CHECK(sMutationCount == 4 && sMutationEvents[2].cause == RHH_ABILITY_MUTATION_SWAP
        && sMutationEvents[2].participant.battler == 1 && sMutationEvents[2].before.stored == levitate && sMutationEvents[2].after.stored == blaze
        && sMutationEvents[3].participant.battler == 0 && sMutationEvents[3].before.stored == blaze && sMutationEvents[3].after.stored == levitate
        && sMutationEvents[2].batch == sMutationEvents[3].batch && sMutationEvents[2].batch_size == 2
        && !sMutationEvents[2].batch_index && sMutationEvents[3].batch_index == 1);
    gBattleMons[1].ability = ABILITY_LEVITATE; gCurrentMove = MOVE_WORRY_SEED;
    RunMutationCommand(B_SCR_OP_TRYOVERWRITEABILITY, 0, FALSE);
    CHECK(sMutationCount == 5 && sMutationEvents[4].cause == RHH_ABILITY_MUTATION_OVERWRITE
        && sMutationEvents[4].before.stored == levitate
        && sMutationEvents[4].after.stored == RhhContentKeyForId(RHH_ABILITY, GetMoveOverwriteAbility(MOVE_WORRY_SEED)));
    gBattleMons[0].ability = ABILITY_BLAZE; gCurrentMove = MOVE_ENTRAINMENT; RunMutationNative(FALSE);
    CHECK(sMutationCount == 6 && sMutationEvents[5].cause == RHH_ABILITY_MUTATION_ENTRAINMENT
        && sMutationEvents[5].after.stored == blaze);
    gBattleStruct->tracedAbility[1] = ABILITY_INTIMIDATE; RunMutationNative(TRUE);
    CHECK(sMutationCount == 7 && sMutationEvents[6].cause == RHH_ABILITY_MUTATION_TRACE
        && sMutationEvents[6].after.stored == RhhContentKeyForId(RHH_ABILITY, ABILITY_INTIMIDATE)
        && sMutationEvents[6].source_battler == RHH_BATTLE_NO_BATTLER && !sMutationEvents[6].move);
    gBattleMons[0].ability = ABILITY_NEUTRALIZING_GAS; gBattleMons[0].volatiles.neutralizingGas = TRUE;
    gBattleMons[1].ability = ABILITY_LEVITATE; gBattleMons[1].volatiles.gastroAcid = FALSE;
    gCurrentMove = MOVE_GASTRO_ACID; RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sMutationCount == 8 && !sMutationEvents[7].before.active && !sMutationEvents[7].after.active
        && !sMutationEvents[7].before_gastro_acid && sMutationEvents[7].after_gastro_acid);
    gBattleMons[0].volatiles.neutralizingGas = FALSE; gBattleMons[1].volatiles.gastroAcid = FALSE;
    gBattleMons[0].ability = gBattleMons[1].ability = ABILITY_LEVITATE;
    gBattleMons[0].volatiles.overwrittenAbility = gBattleMons[1].volatiles.overwrittenAbility = ABILITY_LEVITATE;
    gCurrentMove = MOVE_SKILL_SWAP; RunMutationCommand(B_SCR_OP_TRYSWAPABILITIES, 0, FALSE); CHECK(sMutationCount == 8);
    gBattlerTarget = 0; gBattleMons[0].volatiles.overwrittenAbility = 0;
    RunMutationCommand(B_SCR_OP_TRYSWAPABILITIES, 0, FALSE);
    CHECK(sMutationCount == 9 && sMutationEvents[8].cause == RHH_ABILITY_MUTATION_SWAP
        && sMutationEvents[8].batch_size == 1 && !sMutationEvents[8].batch_index
        && !(sMutationEvents[8].before.flags & RHH_BATTLER_ABILITY_OVERWRITTEN)
        && (sMutationEvents[8].after.flags & RHH_BATTLER_ABILITY_OVERWRITTEN));
    gBattlerTarget = 1;
    gBattleTypeFlags |= BATTLE_TYPE_LINK; gBattleMons[1].volatiles.gastroAcid = FALSE;
    gCurrentMove = MOVE_GASTRO_ACID; RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sMutationCount == 9 && gBattleMons[1].volatiles.gastroAcid); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    gBattleTypeFlags |= BATTLE_TYPE_RECORDED; gBattleMons[1].volatiles.gastroAcid = FALSE;
    RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sMutationCount == 9 && gBattleMons[1].volatiles.gastroAcid); gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED;
    gBattleMons[1].volatiles.gastroAcid = FALSE; gBattleMons[1].ability = ABILITY_LEVITATE;
    gBattleMons[1].volatiles.overwrittenAbility = ABILITY_BLAZE; gBattleStruct->tracedAbility[1] = ABILITY_LEVITATE;
    RunMutationNative(TRUE);
    CHECK(sMutationCount == 10 && sMutationEvents[9].cause == RHH_ABILITY_MUTATION_TRACE
        && sMutationEvents[9].before.stored == sMutationEvents[9].after.stored
        && sMutationEvents[9].before_overwritten == blaze && sMutationEvents[9].after_overwritten == levitate);
    for (u32 i = 0; i < sMutationCount; ++i) CHECK(sMutationEvents[i].sequence == i && sMutationEvents[i].run_id == 456);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && sMutationAbilities->ability_mutation_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    ModsBattleReset(); gBattleMons[1].volatiles.gastroAcid = FALSE; ModsBattleBegin();
    RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sMutationCount == 11 && !sMutationEvents[10].sequence && !sMutationEvents[10].batch);
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; RhhModsUnloadLibraries();
    CheckWeatherSuppressionMutations();
    puts("ability mutations: committed script changes, swaps and exclusions passed");
}

static u32 RunContactAbility(u32 ability, u32 move)
{
    static const u8 returnScript[] = {B_SCR_OP_END};
    gBattleResources->battleScriptsStack->size = 0;
    gBattlescriptCurrInstr = returnScript;
    return AbilityBattleEffects(ABILITYEFFECT_MOVE_END, 1, ability, move, TRUE);
}
static void CheckAbilityContacts(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sMutationMod)
        && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sMutationAbilities = (const void *)RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 29));
    CHECK(sMutationAbilities && sMutationAbilities->header.struct_size >= RHH_ABILITY_SERVICE_V1_29_SIZE
        && RHH_ABILITY_SERVICE_V1_28_SIZE < RHH_ABILITY_SERVICE_V1_29_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 41)));
    struct RhhBattlerAbilityContactMutation outside = {.struct_size = sizeof(outside), .triggering_ability = 99}, saved = outside;
    CHECK(sMutationAbilities->ability_contact_mutation_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct BattleHistory history = {0}; struct AiPartyData parties = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources; gBattleHistory = &history; gAiPartyData = &parties;
    PrepareTurnBattle(); gBattlersCount = 2; gBattleTypeFlags &= ~BATTLE_TYPE_DOUBLE;
    memset(gSpecialStatuses, 0, sizeof(gSpecialStatuses));
    gBattlerAttacker = 0; gBattlerTarget = 1; gCurrentMove = MOVE_TACKLE;
    gBattleMons[0].ability = ABILITY_BLAZE; gBattleMons[1].ability = ABILITY_MUMMY;
    gBattleMons[0].item = gBattleMons[1].item = ITEM_NONE; gSpecialStatuses[1].damagedByAttack = TRUE;
    ModsBattleBegin(); sMutationCount = sContactCount = 0;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    RhhContentKey blaze = RhhContentKeyForId(RHH_ABILITY, ABILITY_BLAZE);
    RhhContentKey mummy = RhhContentKeyForId(RHH_ABILITY, ABILITY_MUMMY);
    RhhContentKey aroma = RhhContentKeyForId(RHH_ABILITY, ABILITY_LINGERING_AROMA);
    RhhContentKey spirit = RhhContentKeyForId(RHH_ABILITY, ABILITY_WANDERING_SPIRIT);
    RhhContentKey tackle = RhhContentKeyForId(RHH_MOVE, MOVE_TACKLE);
    CHECK(RunContactAbility(ABILITY_MUMMY, MOVE_NONE) && sContactCount == 1 && !sMutationCount
        && sContactEvents[0].triggering_ability == mummy && sContactEvents[0].change.cause == RHH_ABILITY_MUTATION_CONTACT_COPY
        && sContactEvents[0].change.participant.battler == 0 && sContactEvents[0].change.source_battler == 1
        && sContactEvents[0].change.before.stored == blaze && sContactEvents[0].change.after.stored == mummy
        && sContactEvents[0].change.move == tackle && sContactEvents[0].change.batch_size == 1);
    gBattleMons[0].ability = ABILITY_BLAZE; gBattleMons[0].volatiles.overwrittenAbility = 0;
    gBattleMons[1].ability = ABILITY_LINGERING_AROMA; gCurrentMove = MOVE_SCRATCH;
    CHECK(RunContactAbility(ABILITY_LINGERING_AROMA, MOVE_TACKLE) && sContactCount == 2
        && sContactEvents[1].triggering_ability == aroma && sContactEvents[1].change.move == tackle
        && sContactEvents[1].change.after.stored == aroma);
    gBattleMons[0].ability = ABILITY_BLAZE; gBattleMons[0].volatiles.overwrittenAbility = 0;
    gBattleMons[1].ability = ABILITY_WANDERING_SPIRIT; gBattleMons[1].volatiles.overwrittenAbility = 0;
    gCurrentMove = MOVE_TACKLE;
    CHECK(RunContactAbility(ABILITY_WANDERING_SPIRIT, MOVE_NONE) && sContactCount == 4
        && sContactEvents[2].triggering_ability == spirit && sContactEvents[3].triggering_ability == spirit
        && sContactEvents[2].change.cause == RHH_ABILITY_MUTATION_CONTACT_SWAP
        && sContactEvents[2].change.participant.battler == 0 && sContactEvents[2].change.before.stored == blaze
        && sContactEvents[2].change.after.stored == spirit && sContactEvents[3].change.participant.battler == 1
        && sContactEvents[3].change.before.stored == spirit && sContactEvents[3].change.after.stored == blaze
        && sContactEvents[2].change.source_battler == 1 && sContactEvents[3].change.source_battler == 1
        && sContactEvents[2].change.batch == sContactEvents[3].change.batch
        && sContactEvents[2].change.batch_size == 2 && !sContactEvents[2].change.batch_index
        && sContactEvents[3].change.batch_index == 1);
    gBattleMons[0].ability = ABILITY_BLAZE; gBattleMons[0].volatiles.overwrittenAbility = 0;
    gBattleMons[1].ability = ABILITY_MUMMY; gBattleMons[1].volatiles.overwrittenAbility = 0;
    gCurrentMove = MOVE_THUNDERBOLT; CHECK(!RunContactAbility(ABILITY_MUMMY, MOVE_NONE) && sContactCount == 4);
    gCurrentMove = MOVE_TACKLE; gBattleMons[0].item = ITEM_PROTECTIVE_PADS;
    CHECK(!RunContactAbility(ABILITY_MUMMY, MOVE_NONE) && sContactCount == 4);
    gBattleMons[0].item = ITEM_ABILITY_SHIELD; CHECK(!RunContactAbility(ABILITY_MUMMY, MOVE_NONE) && sContactCount == 4);
    gBattleMons[0].item = ITEM_NONE; gBattleMons[1].item = ITEM_ABILITY_SHIELD; gBattleMons[1].ability = ABILITY_WANDERING_SPIRIT;
    CHECK(!RunContactAbility(ABILITY_WANDERING_SPIRIT, MOVE_NONE) && sContactCount == 4);
    gBattleMons[1].item = ITEM_NONE; gBattleMons[1].ability = ABILITY_MUMMY; gBattleMons[0].ability = ABILITY_MULTITYPE;
    CHECK(!RunContactAbility(ABILITY_MUMMY, MOVE_NONE) && sContactCount == 4);
    gBattleMons[0].ability = ABILITY_BLAZE; gSpecialStatuses[1].damagedByAttack = FALSE;
    CHECK(!RunContactAbility(ABILITY_MUMMY, MOVE_NONE) && sContactCount == 4);
    gSpecialStatuses[1].damagedByAttack = TRUE; gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(RunContactAbility(ABILITY_MUMMY, MOVE_NONE) && sContactCount == 4 && gBattleMons[0].ability == ABILITY_MUMMY);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; gBattleMons[0].ability = ABILITY_BLAZE; gBattleMons[0].volatiles.overwrittenAbility = 0;
    gBattleTypeFlags |= BATTLE_TYPE_RECORDED;
    CHECK(RunContactAbility(ABILITY_MUMMY, MOVE_NONE) && sContactCount == 4 && gBattleMons[0].ability == ABILITY_MUMMY);
    gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED;
    gBattleMons[1].ability = ABILITY_LEVITATE; gBattleMons[1].volatiles.gastroAcid = FALSE; gCurrentMove = MOVE_GASTRO_ACID;
    RunMutationCommand(B_SCR_OP_SETGASTROACID, 0, FALSE);
    CHECK(sMutationCount == 1 && !sMutationEvents[0].sequence && !sMutationEvents[0].batch && sContactCount == 4);
    gBattleMons[1].ability = ABILITY_MUMMY; gBattleMons[1].volatiles.gastroAcid = FALSE;
    gBattleMons[0].ability = ABILITY_BLAZE; gBattleMons[0].volatiles.overwrittenAbility = 0; gCurrentMove = MOVE_TACKLE;
    CHECK(RunContactAbility(ABILITY_MUMMY, MOVE_NONE) && sContactCount == 5
        && sContactEvents[4].change.sequence == 4 && sContactEvents[4].change.batch == 3 && sMutationCount == 1);
    for (u32 i = 0; i < sContactCount; ++i) CHECK(sContactEvents[i].change.sequence == i && sContactEvents[i].change.run_id == 456);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && sMutationAbilities->ability_contact_mutation_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    ModsBattleReset(); gBattleMons[0].ability = ABILITY_BLAZE; gBattleMons[0].volatiles.overwrittenAbility = 0; ModsBattleBegin();
    CHECK(RunContactAbility(ABILITY_MUMMY, MOVE_NONE) && sContactCount == 6
        && !sContactEvents[5].change.sequence && !sContactEvents[5].change.batch);
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gBattleHistory = NULL; gAiPartyData = NULL;
    RhhModsUnloadLibraries(); puts("ability contacts: committed changes, protections and exclusions passed");
}
