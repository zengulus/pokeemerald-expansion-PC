struct ItemEvTestState { u32 mode, proposals, processed, banked, lastUnique, lastMode, lastStat, lastEarned; };
struct ItemReductionState { u32 deny, proposed, processed, removed, mask, unique; };
static void CheckItemReduction(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0); gMain.inBattle = FALSE; gBattleTypeFlags = 0; ClearBag();
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    gPartiesCount[B_TRAINER_PLAYER] = 1; struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][0];
    struct ItemReductionState *state = RhhModsState("test:item_reduction"); CHECK(state);
    const struct RhhItemEvYieldService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/item_ev_yield", RHH_SERVICE_VERSION(1, 1));
    const struct RhhOwnedMonService *owned = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    struct RhhItemEvReduction outside = {.struct_size = sizeof(outside), .stat_mask = 999}, before = outside;
    CHECK(service && service->reduction_context(&outside) && !memcmp(&outside, &before, sizeof(outside)));
    const u16 items[] = {ITEM_POMEG_BERRY, ITEM_KELPSY_BERRY, ITEM_QUALOT_BERRY,
        ITEM_TAMATO_BERRY, ITEM_HONDEW_BERRY, ITEM_GREPA_BERRY};
    for (u32 stat = 0; stat < NUM_STATS; ++stat)
    {
        CreateMonWithIVs(mon, SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 31);
        u8 value = 25, friendship = 50; u16 held = ITEM_SOOTHE_BELL;
        SetMonData(mon, MON_DATA_HP_EV + stat, &value); SetMonData(mon, MON_DATA_FRIENDSHIP, &friendship);
        SetMonData(mon, MON_DATA_HELD_ITEM, &held); CalculateMonStats(mon);
        struct Pokemon expected = *mon; CHECK(!ExecuteTableBasedItemEffect(&expected, items[stat], 0, 0));
        CHECK(AddBagItem(items[stat], 1)); rng_value_t rng = gRngValue;
        struct RhhOwnedMonSnapshot snap = {.struct_size = sizeof(snap)};
        struct RhhItemEvPreview preview = {.struct_size = sizeof(preview)}; struct Pokemon beforeMon = *mon;
        CHECK(owned && !owned->party_at(0, &snap) && !service->preview(&snap, RhhContentKeyForId(RHH_ITEM, items[stat]), &preview)
            && preview.status == RHH_ITEM_EV_PREVIEW_APPLIED && preview.kind == RHH_ITEM_EV_PREVIEW_REDUCTION
            && preview.reduction.before[stat] == 25 && preview.reduction.after[stat] == 15
            && !memcmp(mon, &beforeMon, sizeof(beforeMon)) && CheckBagHasItem(items[stat], 1)
            && state->processed == stat && !memcmp(&rng, &gRngValue, sizeof(rng)));
        CHECK(ModsUseItemEv(mon, items[stat], 0) == MOD_ITEM_EV_APPLIED && !memcmp(mon, &expected, sizeof(expected))
            && !CheckBagHasItem(items[stat], 1) && state->processed == stat + 1 && state->mask == (1u << stat)
            && state->unique && !memcmp(&rng, &gRngValue, sizeof(rng)));
    }
    u8 zero = 0, friendship = 50; SetMonData(mon, MON_DATA_SPDEF_EV, &zero);
    SetMonData(mon, MON_DATA_FRIENDSHIP, &friendship); struct Pokemon expected = *mon;
    CHECK(!ExecuteTableBasedItemEffect(&expected, ITEM_GREPA_BERRY, 0, 0) && AddBagItem(ITEM_GREPA_BERRY, 1));
    u32 removed = state->removed;
    CHECK(ModsUseItemEv(mon, ITEM_GREPA_BERRY, 0) == MOD_ITEM_EV_APPLIED && !memcmp(mon, &expected, sizeof(expected))
        && state->processed == 7 && state->removed == removed);
    u8 value = 15;
    for (u32 i = 0; i < NUM_STATS; ++i) SetMonData(mon, MON_DATA_HP_EV + i, &value);
    CalculateMonStats(mon); expected = *mon; CHECK(!ExecuteTableBasedItemEffect(&expected, ITEM_FRESH_START_MOCHI, 0, 0));
    CHECK(AddBagItem(ITEM_FRESH_START_MOCHI, 1)); state->deny = 1; struct Pokemon original = *mon;
    struct RhhOwnedMonSnapshot reductionSnap = {.struct_size = sizeof(reductionSnap)};
    struct RhhItemEvPreview reductionPreview = {.struct_size = sizeof(reductionPreview)};
    CHECK(!owned->party_at(0, &reductionSnap)
        && !service->preview(&reductionSnap, RhhContentKeyForId(RHH_ITEM, ITEM_FRESH_START_MOCHI), &reductionPreview)
        && reductionPreview.status == RHH_ITEM_EV_PREVIEW_DENIED && !strcmp(reductionPreview.reason, "no EV reductions")
        && !memcmp(mon, &original, sizeof(original)) && state->processed == 7);
    CHECK(ModsUseItemEv(mon, ITEM_FRESH_START_MOCHI, 0) == MOD_ITEM_EV_DENIED && !memcmp(mon, &original, sizeof(original))
        && CheckBagHasItem(ITEM_FRESH_START_MOCHI, 1) && state->processed == 7);
    state->deny = 0; CHECK(ModsUseItemEv(mon, ITEM_FRESH_START_MOCHI, 0) == MOD_ITEM_EV_APPLIED
        && !memcmp(mon, &expected, sizeof(expected)) && !CheckBagHasItem(ITEM_FRESH_START_MOCHI, 1)
        && state->processed == 8 && state->mask == (1u << NUM_STATS) - 1 && state->removed == removed + 90);
    CHECK(AddBagItem(ITEM_FRESH_START_MOCHI, 1) && ModsUseItemEv(mon, ITEM_FRESH_START_MOCHI, 0) == MOD_ITEM_EV_DENIED
        && CheckBagHasItem(ITEM_FRESH_START_MOCHI, 1) && state->processed == 8);
    u8 maxFriendship = MAX_FRIENDSHIP; SetMonData(mon, MON_DATA_FRIENDSHIP, &maxFriendship);
    CHECK(AddBagItem(ITEM_POMEG_BERRY, 1)); original = *mon;
    CHECK(ModsUseItemEv(mon, ITEM_POMEG_BERRY, 0) == MOD_ITEM_EV_DENIED && !memcmp(mon, &original, sizeof(original))
        && CheckBagHasItem(ITEM_POMEG_BERRY, 1) && state->processed == 8);
    CHECK(service->reduction_context(&outside) && !memcmp(&outside, &before, sizeof(outside)));
    RhhModsUnloadLibraries(); puts("item EV reductions: berries, reset, friendship and denial passed");
}
static void CheckItemEv(const char *module, const char *limitModule)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module)
        && !RhhModsLoadLibrary(limitModule) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0); gMain.inBattle = FALSE; gBattleTypeFlags = 0; ClearBag();
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][0];
    struct ItemEvTestState *state = RhhModsState("test:item_evs"); u8 *limit = RhhModsState("test:ev_limits");
    const struct RhhItemEvYieldService *service = (const void *)RhhModsHostAPI()->get_service("rhh:service/item_ev_yield", RHH_SERVICE_VERSION(1, 0));
    const struct RhhOwnedMonService *owned = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    CHECK(state && limit && service && RhhModsHostAPI()->get_service("rhh:service/item_ev_yield", RHH_SERVICE_VERSION(1, 1)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/item_ev_yield", RHH_SERVICE_VERSION(1, 2)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/item_ev_yield", RHH_SERVICE_VERSION(1, 3)) == &service->header
        && RHH_ITEM_EV_YIELD_SERVICE_V1_2_SIZE < RHH_ITEM_EV_YIELD_SERVICE_V1_3_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/item_ev_yield", RHH_SERVICE_VERSION(1, 4)));
    struct RhhItemEvYield outside = {.struct_size = sizeof(outside), .earned = 999}, saved = outside;
    CHECK(service->context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    const u16 items[] = {ITEM_HP_UP, ITEM_PROTEIN, ITEM_IRON, ITEM_CARBOS, ITEM_CALCIUM, ITEM_ZINC,
                         ITEM_HEALTH_FEATHER, ITEM_HEALTH_MOCHI};
    const u8 stats[] = {STAT_HP, STAT_ATK, STAT_DEF, STAT_SPEED, STAT_SPATK, STAT_SPDEF, STAT_HP, STAT_HP};
    for (u32 i = 0; i < ARRAY_COUNT(items); ++i)
    {
        CreateMonWithIVs(mon, SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 31);
        u8 friendship = 50; u16 held = ITEM_SOOTHE_BELL;
        SetMonData(mon, MON_DATA_FRIENDSHIP, &friendship); SetMonData(mon, MON_DATA_HELD_ITEM, &held);
        struct Pokemon expected = *mon; CHECK(!ExecuteTableBasedItemEffect(&expected, items[i], 0, 0));
        CHECK(AddBagItem(items[i], 1)); rng_value_t rng = gRngValue, rng2 = gRng2Value;
        struct RhhOwnedMonSnapshot snap = {.struct_size = sizeof(snap)};
        struct RhhItemEvPreview preview = {.struct_size = sizeof(preview)}, tiny = {.struct_size = 1}, tinyOld = tiny;
        struct Pokemon beforeMon = *mon; u32 processedBefore = state->processed;
        CHECK(owned && !owned->party_at(0, &snap) && !service->preview(&snap, RhhContentKeyForId(RHH_ITEM, items[i]), &preview)
            && preview.status == RHH_ITEM_EV_PREVIEW_APPLIED && preview.kind == RHH_ITEM_EV_PREVIEW_GAIN
            && preview.gain.stat == stats[i] && preview.gain.after[stats[i]] == (i == 6 ? 1 : 10)
            && service->preview(&snap, RhhContentKeyForId(RHH_ITEM, items[i]), &tiny) && !memcmp(&tiny, &tinyOld, sizeof(tiny))
            && !memcmp(mon, &beforeMon, sizeof(beforeMon)) && CheckBagHasItem(items[i], 1)
            && state->processed == processedBefore && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        CHECK(ModsUseItemEv(mon, items[i], 0) == MOD_ITEM_EV_APPLIED && !memcmp(mon, &expected, sizeof(expected))
            && !CheckBagHasItem(items[i], 1) && state->lastMode == RHH_ITEM_EV_APPLY && state->lastStat == stats[i]
            && state->lastEarned == (i == 6 ? 1 : 10) && state->lastUnique
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    }
    u8 zero = 0, max = 252, six = 6, friendship = 50;
    for (u32 i = 0; i < NUM_STATS; ++i) SetMonData(mon, MON_DATA_HP_EV + i, &zero);
    SetMonData(mon, MON_DATA_HP_EV, &max); SetMonData(mon, MON_DATA_ATK_EV, &max);
    SetMonData(mon, MON_DATA_DEF_EV, &six); SetMonData(mon, MON_DATA_FRIENDSHIP, &friendship); CalculateMonStats(mon);
    struct Pokemon original = *mon, expectedFriendship = *mon;
    for (u32 i = 0; i < NUM_STATS; ++i) SetMonData(&expectedFriendship, MON_DATA_HP_EV + i, &zero);
    CHECK(!ExecuteTableBasedItemEffect(&expectedFriendship, ITEM_HP_UP, 0, 0));
    CHECK(AddBagItem(ITEM_HP_UP, 2)); u32 processed = state->processed;
    struct RhhOwnedMonSnapshot gainSnap = {.struct_size = sizeof(gainSnap)};
    struct RhhItemEvPreview gainPreview = {.struct_size = sizeof(gainPreview)};
    CHECK(!owned->party_at(0, &gainSnap) && !service->preview(&gainSnap, RhhContentKeyForId(RHH_ITEM, ITEM_HP_UP), &gainPreview)
        && gainPreview.status == RHH_ITEM_EV_PREVIEW_DENIED && gainPreview.gain.before[0] == 252);
    CHECK(ModsUseItemEv(mon, ITEM_HP_UP, 0) == MOD_ITEM_EV_DENIED && !memcmp(mon, &original, sizeof(original))
        && state->processed == processed && CheckBagHasItem(ITEM_HP_UP, 2));
    state->mode = 1;
    CHECK(!service->preview(&gainSnap, RhhContentKeyForId(RHH_ITEM, ITEM_HP_UP), &gainPreview)
        && gainPreview.status == RHH_ITEM_EV_PREVIEW_BANKED && gainPreview.gain.after[0] == 252
        && state->processed == processed && CheckBagHasItem(ITEM_HP_UP, 2));
    CHECK(ModsUseItemEv(mon, ITEM_HP_UP, 0) == MOD_ITEM_EV_BANKED && state->processed == processed + 1
        && state->banked == 10 && state->lastMode == RHH_ITEM_EV_BANK && state->lastEarned == 10
        && GetMonData(mon, MON_DATA_HP_EV) == 252 && GetMonData(mon, MON_DATA_ATK_EV) == 252
        && GetMonData(mon, MON_DATA_FRIENDSHIP) == GetMonData(&expectedFriendship, MON_DATA_FRIENDSHIP)
        && CheckBagHasItem(ITEM_HP_UP, 1) && !CheckBagHasItem(ITEM_HP_UP, 2));
    original = *mon; processed = state->processed; state->mode = 2;
    struct RhhItemEvPreview savedPreview = gainPreview;
    CHECK(!owned->party_at(0, &gainSnap)); gainSnap.evs[0]--;
    CHECK(service->preview(&gainSnap, RhhContentKeyForId(RHH_ITEM, ITEM_HP_UP), &gainPreview)
        && !memcmp(&gainPreview, &savedPreview, sizeof(gainPreview)));
    CHECK(ModsUseItemEv(mon, ITEM_HP_UP, 0) == MOD_ITEM_EV_DENIED && !memcmp(mon, &original, sizeof(original))
        && state->processed == processed && CheckBagHasItem(ITEM_HP_UP, 1));
    state->mode = 4; CHECK(ModsUseItemEv(mon, ITEM_HP_UP, 0) == MOD_ITEM_EV_DENIED && CheckBagHasItem(ITEM_HP_UP, 1));
    for (u32 i = 0; i < NUM_STATS; ++i) SetMonData(mon, MON_DATA_HP_EV + i, &zero);
    state->mode = 3; CHECK(ModsUseItemEv(mon, ITEM_HP_UP, 0) == MOD_ITEM_EV_APPLIED
        && GetMonData(mon, MON_DATA_HP_EV) == 10 && !CheckBagHasItem(ITEM_HP_UP, 1));
    *limit = 2; u8 seven = 7; SetMonData(mon, MON_DATA_HP_EV, &seven); state->mode = 0;
    CHECK(AddBagItem(ITEM_HP_UP, 1) && ModsUseItemEv(mon, ITEM_HP_UP, 0) == MOD_ITEM_EV_APPLIED
        && GetMonData(mon, MON_DATA_HP_EV) == 8);
    *limit = 0; gParties[B_TRAINER_PLAYER][1] = *mon; gPartiesCount[B_TRAINER_PLAYER] = 2; state->mode = 1;
    CHECK(AddBagItem(ITEM_HEALTH_FEATHER, 1) && ModsUseItemEv(mon, ITEM_HEALTH_FEATHER, 0) == MOD_ITEM_EV_BANKED
        && !state->lastUnique && state->banked == 11);
    CHECK(ModsUseItemEv(mon, ITEM_HP_UP, 0) == MOD_ITEM_EV_DENIED
        && ModsUseItemEv(mon, ITEM_POMEG_BERRY, 0) == MOD_ITEM_EV_UNHANDLED);
    gMain.inBattle = TRUE; CHECK(ModsUseItemEv(mon, ITEM_HEALTH_FEATHER, 0) == MOD_ITEM_EV_UNHANDLED); gMain.inBattle = FALSE;
    CHECK(service->context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    RhhModsUnloadLibraries(); puts("item EV yield: applied, banked, denied, caps and compatibility passed");
}
