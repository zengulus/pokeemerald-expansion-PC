enum {
    EV_PURITY_LIMIT_RANDOM = 1u, EV_PURITY_LIMIT_PURE = 2u,
    EV_PURITY_BATTLE_BANK_RANDOM = 4u, EV_PURITY_BATTLE_DENY_RANDOM = 8u,
    EV_PURITY_ITEM_BANK_RANDOM = 16u, EV_PURITY_ITEM_DENY_RANDOM = 32u,
    EV_PURITY_REDUCE_DENY_RANDOM = 64u, EV_PURITY_OBSERVER_RANDOM = 128u,
    EV_PURITY_ITEM_BANK_PURE = 256u
};
static u32 sEvPurityMode, sEvBattleObserved, sEvItemObserved, sEvReductionObserved;
static struct RhhBattleEvPolicy sEvBattlePolicy;
static u32 sEvItemMode;
static void EvPurityTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->kind == RHH_EV_LIMITS && (sEvPurityMode & (EV_PURITY_LIMIT_RANDOM | EV_PURITY_LIMIT_PURE))) {
        decision->value = 20; for (u32 i = 0; i < RHH_STAT_COUNT; ++i) decision->stats[i] = 5;
        if (sEvPurityMode & EV_PURITY_LIMIT_RANDOM) { (void)Random(); (void)Random2(); }
    } else if (event->kind == RHH_BATTLE_EV_PROPOSAL && (sEvPurityMode & EV_PURITY_BATTLE_BANK_RANDOM)) {
        decision->value = RHH_BATTLE_EV_BANK; (void)Random(); (void)Random2();
    } else if (event->kind == RHH_ITEM_EV_PROPOSAL && (sEvPurityMode & (EV_PURITY_ITEM_BANK_RANDOM | EV_PURITY_ITEM_BANK_PURE))) {
        decision->value = RHH_ITEM_EV_BANK;
        if (sEvPurityMode & EV_PURITY_ITEM_BANK_RANDOM) { (void)Random(); (void)Random2(); }
    }
}
static void EvPurityConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if ((event->kind == RHH_CAN_APPLY_BATTLE_EVS && (sEvPurityMode & EV_PURITY_BATTLE_DENY_RANDOM))
        || (event->kind == RHH_ITEM_EV_PROPOSAL && (sEvPurityMode & EV_PURITY_ITEM_DENY_RANDOM))
        || (event->kind == RHH_CAN_REDUCE_ITEM_EVS && (sEvPurityMode & EV_PURITY_REDUCE_DENY_RANDOM))) {
        (void)Random(); (void)Random2(); decision->allowed = 0; strcpy(decision->reason, "deny EV change");
    }
}
static void EvPurityObserve(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (event->kind == RHH_BATTLE_EVS_PROCESSED) {
        const struct RhhBattleEvYieldService *service = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle_ev_yield", RHH_SERVICE_VERSION(1, 2));
        sEvBattlePolicy = (struct RhhBattleEvPolicy){.struct_size = sizeof(sEvBattlePolicy)};
        CHECK(service && !service->policy_context(&sEvBattlePolicy)); ++sEvBattleObserved;
    } else if (event->kind == RHH_ITEM_EV_PROCESSED) {
        const struct RhhItemEvYieldService *service = (const void *)RhhModsHostAPI()->get_service("rhh:service/item_ev_yield", RHH_SERVICE_VERSION(1, 2));
        struct RhhItemEvYield value = {.struct_size = sizeof(value)};
        CHECK(service && !service->context(&value)); sEvItemMode = value.mode; ++sEvItemObserved;
    } else ++sEvReductionObserved;
    if (sEvPurityMode & EV_PURITY_OBSERVER_RANDOM) { (void)Random(); (void)Random2(); }
}
static const struct RhhModHook sEvPurityHooks[] = {
    {sizeof(struct RhhModHook), RHH_EV_LIMITS, RHH_TRANSFORM, 0, EvPurityTransform, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_EV_PROPOSAL, RHH_TRANSFORM, 0, EvPurityTransform, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_APPLY_BATTLE_EVS, RHH_CONSTRAINT, 0, EvPurityConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_ITEM_EV_PROPOSAL, RHH_TRANSFORM, 0, EvPurityTransform, NULL},
    {sizeof(struct RhhModHook), RHH_ITEM_EV_PROPOSAL, RHH_CONSTRAINT, 0, EvPurityConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_REDUCE_ITEM_EVS, RHH_CONSTRAINT, 0, EvPurityConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_EVS_PROCESSED, RHH_OBSERVE, 0, NULL, EvPurityObserve},
    {sizeof(struct RhhModHook), RHH_ITEM_EV_PROCESSED, RHH_OBSERVE, 0, NULL, EvPurityObserve},
    {sizeof(struct RhhModHook), RHH_ITEM_EV_REDUCTION_PROCESSED, RHH_OBSERVE, 0, NULL, EvPurityObserve}
};
static const struct RhhModDefinition sEvPurityMod = {.struct_size = sizeof(sEvPurityMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:ev_purity", .hooks = sEvPurityHooks, .hook_count = ARRAY_COUNT(sEvPurityHooks)};
static void CheckEvPurity(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sEvPurityMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0); gMain.inBattle = FALSE; gBattleTypeFlags = 0; ClearBag();
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][0];
    CreateMonWithIVs(mon, SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 31);
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhEvLimitService *limits = (const void *)host->get_service("rhh:service/ev_limits", RHH_SERVICE_VERSION(1, 1));
    const struct RhhOwnedMonService *owned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    CHECK(limits && owned && host->get_service("rhh:service/battle_ev_yield", RHH_SERVICE_VERSION(1, 2))
        && host->get_service("rhh:service/item_ev_yield", RHH_SERVICE_VERSION(1, 2)));
    struct RhhOwnedMonSnapshot snapshot = {.struct_size = sizeof(snapshot)};
    struct RhhEvLimits value = {.struct_size = sizeof(value)}; CHECK(!owned->party_at(0, &snapshot));
    rng_value_t rng = gRngValue, rng2 = gRng2Value; struct Pokemon original = *mon;
    sEvPurityMode = EV_PURITY_LIMIT_RANDOM;
    CHECK(!limits->query(&snapshot, &value) && value.total == MAX_TOTAL_EVS && value.per_stat[0] == MAX_PER_STAT_EVS
        && !memcmp(mon, &original, sizeof(original)) && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sEvPurityMode = EV_PURITY_LIMIT_PURE;
    CHECK(!limits->query(&snapshot, &value) && value.total == 20 && value.per_stat[0] == 5);
    sEvBattleObserved = sEvItemObserved = sEvReductionObserved = 0;
    sEvPurityMode = EV_PURITY_BATTLE_BANK_RANDOM | EV_PURITY_OBSERVER_RANDOM; gMain.inBattle = TRUE;
    MonGainEVs(mon, SPECIES_ZIGZAGOON);
    CHECK(GetMonData(mon, MON_DATA_SPEED_EV) == 1 && sEvBattleObserved == 1
        && sEvBattlePolicy.mode == RHH_BATTLE_EV_APPLY && sEvBattlePolicy.accepted
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    original = *mon; sEvPurityMode = EV_PURITY_BATTLE_DENY_RANDOM | EV_PURITY_OBSERVER_RANDOM;
    MonGainEVs(mon, SPECIES_ZIGZAGOON);
    CHECK(!memcmp(mon, &original, sizeof(original)) && sEvBattleObserved == 2 && !sEvBattlePolicy.accepted
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gMain.inBattle = FALSE; CHECK(AddBagItem(ITEM_HP_UP, 2)); sEvPurityMode = EV_PURITY_ITEM_BANK_RANDOM | EV_PURITY_OBSERVER_RANDOM;
    CHECK(ModsUseItemEv(mon, ITEM_HP_UP, 0) == MOD_ITEM_EV_APPLIED && GetMonData(mon, MON_DATA_HP_EV) == 10
        && sEvItemObserved == 1 && sEvItemMode == RHH_ITEM_EV_APPLY && CheckBagHasItem(ITEM_HP_UP, 1)
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    original = *mon; sEvPurityMode = EV_PURITY_ITEM_DENY_RANDOM | EV_PURITY_OBSERVER_RANDOM;
    CHECK(ModsUseItemEv(mon, ITEM_HP_UP, 0) == MOD_ITEM_EV_DENIED && !memcmp(mon, &original, sizeof(original))
        && CheckBagHasItem(ITEM_HP_UP, 1) && sEvItemObserved == 1
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    CHECK(AddBagItem(ITEM_POMEG_BERRY, 1)); sEvPurityMode = EV_PURITY_REDUCE_DENY_RANDOM | EV_PURITY_OBSERVER_RANDOM;
    CHECK(ModsUseItemEv(mon, ITEM_POMEG_BERRY, 0) == MOD_ITEM_EV_DENIED && !memcmp(mon, &original, sizeof(original))
        && CheckBagHasItem(ITEM_POMEG_BERRY, 1) && !sEvReductionObserved
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sEvPurityMode = EV_PURITY_OBSERVER_RANDOM;
    CHECK(ModsUseItemEv(mon, ITEM_POMEG_BERRY, 0) == MOD_ITEM_EV_APPLIED && !GetMonData(mon, MON_DATA_HP_EV)
        && sEvReductionObserved == 1 && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sEvPurityMode = EV_PURITY_ITEM_BANK_PURE | EV_PURITY_OBSERVER_RANDOM;
    CHECK(ModsUseItemEv(mon, ITEM_HP_UP, 0) == MOD_ITEM_EV_BANKED && !GetMonData(mon, MON_DATA_HP_EV)
        && sEvItemObserved == 2 && sEvItemMode == RHH_ITEM_EV_BANK
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    RhhModsUnloadLibraries(); puts("EV purity: limits, decisions, denials and observers preserve both RNG streams");
}
