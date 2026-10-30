static void CheckHyperTrainingEdit(void)
{
    InitModsForTest(); RhhModsSetRunContext(123, 456, 0);
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 40));
    sEditTestService = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 4));
    CHECK(mons && sEditTestService && sEditTestService->header.struct_size >= RHH_MON_EDIT_SERVICE_V1_4_SIZE);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr, 0, sizeof(*gPokemonStoragePtr));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 0);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    SetMoney(&gSaveBlock1Ptr->money, 1000); CHECK(AddBagItem(ITEM_POTION, 2));
    struct RhhHyperTrainingEditRequest req = {.struct_size = sizeof(req), .after_mask = 3, .has_recipe = 1,
        .edit = {.struct_size = sizeof(req.edit), .fields = RHH_EDIT_IVS, .expected.struct_size = sizeof(req.edit.expected)},
        .recipe = {.struct_size = sizeof(req.recipe), .item_count = 1, .money = 500}};
    req.recipe.items[0] = (struct RhhMonEditRecipeItem){.item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION), .quantity = 1};
    CHECK(!mons->party_at(0, &req.edit.expected));
    memcpy(req.edit.ivs, req.edit.expected.ivs, sizeof(req.edit.ivs)); req.edit.ivs[RHH_STAT_ATTACK] = 5;
    sEditTestRequest = &req.edit; sEditConstraints = sEditNotifications = sHyperProposals = sHyperNotifications = 0;
    sEditDeny = FALSE;
    struct Pokemon original = gParties[B_TRAINER_PLAYER][0]; rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhHyperTrainingEditResult quote = {.struct_size = sizeof(quote)}, result = {.struct_size = sizeof(result)};
    struct RhhHyperTrainingEditContext outside = {.struct_size = sizeof(outside), .before_mask = 99}, oldOutside = outside;
    CHECK(sEditTestService->hyper_training_context(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    CHECK(sEditTestService->preview_hyper_training(&req, &quote) == RHH_EDIT_OK && quote.edit.is_preview
        && quote.before_mask == 0 && quote.after_mask == 3 && quote.has_recipe
        && quote.edit.fields == (RHH_EDIT_IVS | RHH_EDIT_HYPER_TRAINING)
        && quote.edit.after.ivs[RHH_STAT_ATTACK] == 5 && quote.payment.items[0].after == 1
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)) && GetMoney(&gSaveBlock1Ptr->money) == 1000
        && CheckBagHasItem(ITEM_POTION, 2) && sHyperProposals == 1 && !sHyperNotifications
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sEditDeny = TRUE;
    CHECK(sEditTestService->commit_hyper_training(&req, &quote, &result) == RHH_EDIT_DENIED && !sHyperNotifications);
    sEditDeny = FALSE;
    SetMoney(&gSaveBlock1Ptr->money, 1001);
    CHECK(sEditTestService->commit_hyper_training(&req, &quote, &result) == RHH_EDIT_STALE
        && GetMoney(&gSaveBlock1Ptr->money) == 1001 && CheckBagHasItem(ITEM_POTION, 2) && !sHyperNotifications);
    SetMoney(&gSaveBlock1Ptr->money, 1000);
    u8 trained = 1; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HYPER_TRAINED_DEF, &trained);
    CHECK(sEditTestService->commit_hyper_training(&req, &quote, &result) == RHH_EDIT_STALE && !sHyperNotifications);
    gParties[B_TRAINER_PLAYER][0] = original;
    CHECK(sEditTestService->commit_hyper_training(&req, &quote, &result) == RHH_EDIT_OK
        && !result.edit.is_preview && sHyperNotifications == 1 && GetMoney(&gSaveBlock1Ptr->money) == 500
        && CheckBagHasItem(ITEM_POTION, 1) && !CheckBagHasItem(ITEM_POTION, 2));
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
    struct RhhOwnedIvView iv = {.struct_size = sizeof(iv)};
    CHECK(!mons->party_at(0, &mon) && !mons->iv_view(&mon, &iv) && iv.hyper_trained == 3
        && iv.mon.ivs[RHH_STAT_ATTACK] == 5 && iv.effective_ivs[RHH_STAT_ATTACK] == 31);
    CHECK(sEditTestService->hyper_training_context(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    u32 lost = gParties[B_TRAINER_PLAYER][0].maxHP;
    gParties[B_TRAINER_PLAYER][0].hp = 0;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_LOST, &lost);
    gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][0].box; gPartiesCount[B_TRAINER_PLAYER] = 0;
    req = (struct RhhHyperTrainingEditRequest){.struct_size = sizeof(req), .before_mask = 3, .after_mask = 0,
        .edit = {.struct_size = sizeof(req.edit), .expected.struct_size = sizeof(req.edit.expected)}};
    CHECK(!mons->box_at(0, 0, &req.edit.expected));
    struct RhhMonEditRequest nested = req.edit; nested.fields = RHH_EDIT_IVS; sEditTestRequest = &nested;
    quote = (struct RhhHyperTrainingEditResult){.struct_size = sizeof(quote)};
    CHECK(sEditTestService->preview_hyper_training(&req, &quote) == RHH_EDIT_OK
        && quote.edit.fields == RHH_EDIT_HYPER_TRAINING && !quote.edit.current_hp);
    CHECK(sEditTestService->commit_hyper_training(&req, &quote, &result) == RHH_EDIT_OK
        && !result.edit.current_hp && sHyperNotifications == 2);
    CHECK(!mons->box_at(0, 0, &mon) && !mons->iv_view(&mon, &iv) && !iv.hyper_trained);
    req.after_mask = 1u << RHH_STAT_COUNT;
    CHECK(sEditTestService->preview_hyper_training(&req, &result) == RHH_EDIT_INVALID && sHyperNotifications == 2);
    puts("hyper training edit: preview, constraints, recipe, staleness and boxed fainting passed");
}
