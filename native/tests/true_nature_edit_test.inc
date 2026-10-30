static void CheckTrueNatureEdit(const char *path)
{
    InitModsForTest(); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr, 0, sizeof(*gPokemonStoragePtr));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 10);
    gPartiesCount[B_TRAINER_PLAYER] = 1; SetMoney(&gSaveBlock1Ptr->money, 1000); CHECK(AddBagItem(ITEM_POTION, 2));
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); u32 legacyGeneration = gSaveCounter;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *owned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 39));
    const struct RhhNatureService *nature = (const void *)host->get_service("rhh:service/nature", RHH_SERVICE_VERSION(1, 0));
    sEditTestService = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 5));
    CHECK(owned && nature && sEditTestService && sEditTestService->header.struct_size >= RHH_MON_EDIT_SERVICE_V1_5_SIZE);
    struct RhhTrueNatureEditRequest req = {.struct_size = sizeof(req), .stat_policy = RHH_TRUE_NATURE_FOLLOW_UNMINTED,
        .has_recipe = 1, .edit = {.struct_size = sizeof(req.edit), .expected.struct_size = sizeof(req.edit.expected)},
        .recipe = {.struct_size = sizeof(req.recipe), .item_count = 1, .money = 200}};
    req.recipe.items[0] = (struct RhhMonEditRecipeItem){.item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION), .quantity = 1};
    CHECK(!owned->party_at(0, &req.edit.expected));
    u32 target = (req.edit.expected.nature + 1) % NUM_NATURES;
    struct RhhNatureInfo info = {.struct_size = sizeof(info)}; CHECK(!nature->at(target, &info)); req.nature = info.key;
    struct RhhMonEditRequest nested = req.edit; nested.fields = RHH_EDIT_IVS; sEditTestRequest = &nested;
    sEditConstraints = sEditNotifications = sTrueNatureProposals = sTrueNatureNotifications = 0; sEditDeny = FALSE;
    struct Pokemon original = gParties[B_TRAINER_PLAYER][0]; struct Bag bag = gSaveBlock1Ptr->bag;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhTrueNatureEditResult quote = {.struct_size = sizeof(quote)}, result = {.struct_size = sizeof(result)};
    struct RhhTrueNatureEditContext outside = {.struct_size = sizeof(outside), .stat_policy = 99}, savedOutside = outside;
    CHECK(sEditTestService->true_nature_context(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside)));
    RhhContentKey savedKey = req.nature; req.nature = 0;
    CHECK(sEditTestService->preview_true_nature(&req, &result) == RHH_EDIT_INVALID && !ModsNatureFormatEnabled());
    req.nature = savedKey; req.stat_policy = RHH_TRUE_NATURE_FOLLOW_UNMINTED + 1;
    CHECK(sEditTestService->preview_true_nature(&req, &result) == RHH_EDIT_INVALID && !ModsNatureFormatEnabled());
    req.stat_policy = RHH_TRUE_NATURE_FOLLOW_UNMINTED; ++req.edit.expected.level;
    CHECK(sEditTestService->preview_true_nature(&req, &result) == RHH_EDIT_STALE && !ModsNatureFormatEnabled());
    --req.edit.expected.level;
    gSaveBlock1Ptr->daycare.mons[0].mon = original.box;
    CHECK(sEditTestService->preview_true_nature(&req, &result) == RHH_EDIT_STALE && !ModsNatureFormatEnabled());
    memset(&gSaveBlock1Ptr->daycare.mons[0], 0, sizeof(gSaveBlock1Ptr->daycare.mons[0]));
    u8 egg = TRUE; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &egg);
    CHECK(!owned->party_at(0, &req.edit.expected) && sEditTestService->preview_true_nature(&req, &result) == RHH_EDIT_STALE
        && !ModsNatureFormatEnabled());
    gParties[B_TRAINER_PLAYER][0] = original; CHECK(!owned->party_at(0, &req.edit.expected));
    struct RhhNatureTrainingEditRequest combined = {.struct_size = sizeof(combined),
        .before_mask = 0, .after_mask = 1u << RHH_STAT_HP, .nature = req};
    struct RhhNatureTrainingEditResult combinedQuote = {.struct_size = sizeof(combinedQuote)},
        combinedResult = {.struct_size = sizeof(combinedResult)};
    unsigned natureProposals = sTrueNatureProposals, trainingProposals = sHyperProposals;
    CHECK(sEditTestService->preview_nature_training(&combined, &combinedQuote) == RHH_EDIT_OK
        && combinedQuote.nature.activates_format && combinedQuote.nature.payment.money_after == 800
        && (combinedQuote.nature.edit.fields & (RHH_EDIT_TRUE_NATURE | RHH_EDIT_HYPER_TRAINING))
            == (RHH_EDIT_TRUE_NATURE | RHH_EDIT_HYPER_TRAINING)
        && sTrueNatureProposals == natureProposals + 1 && sHyperProposals == trainingProposals + 1
        && !ModsNatureFormatEnabled() && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    combined.after_mask |= 1u << RHH_STAT_ATTACK;
    CHECK(sEditTestService->commit_nature_training(&combined, &combinedQuote, &combinedResult) == RHH_EDIT_STALE
        && !ModsNatureFormatEnabled());
    combined.after_mask = 1u << RHH_STAT_HP; sEditDeny = TRUE;
    CHECK(sEditTestService->commit_nature_training(&combined, &combinedQuote, &combinedResult) == RHH_EDIT_DENIED
        && !ModsNatureFormatEnabled() && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original))
        && GetMoney(&gSaveBlock1Ptr->money) == 1000);
    sEditDeny = FALSE; SetMoney(&gSaveBlock1Ptr->money, 1001);
    CHECK(sEditTestService->commit_nature_training(&combined, &combinedQuote, &combinedResult) == RHH_EDIT_STALE
        && !ModsNatureFormatEnabled() && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    SetMoney(&gSaveBlock1Ptr->money, 1000);
    CHECK(sEditTestService->preview_true_nature(&req, &quote) == RHH_EDIT_OK && quote.edit.is_preview
        && quote.activates_format && quote.edit.fields == (RHH_EDIT_TRUE_NATURE | RHH_EDIT_STAT_NATURE)
        && quote.edit.before.nature == original.box.personality % NUM_NATURES
        && quote.edit.after.nature == target && quote.edit.after.stat_nature == target
        && quote.payment.money_after == 800 && quote.payment.items[0].after == 1
        && !ModsNatureFormatEnabled() && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original))
        && !memcmp(&bag, &gSaveBlock1Ptr->bag, sizeof(bag)) && GetMoney(&gSaveBlock1Ptr->money) == 1000
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sEditDeny = TRUE;
    CHECK(sEditTestService->commit_true_nature(&req, &quote, &result) == RHH_EDIT_DENIED
        && !ModsNatureFormatEnabled() && !sTrueNatureNotifications);
    sEditDeny = FALSE; SetMoney(&gSaveBlock1Ptr->money, 1001);
    CHECK(sEditTestService->commit_true_nature(&req, &quote, &result) == RHH_EDIT_STALE
        && !ModsNatureFormatEnabled() && GetMoney(&gSaveBlock1Ptr->money) == 1001 && !sTrueNatureNotifications);
    SetMoney(&gSaveBlock1Ptr->money, 1000);
    sEditNatureIntruder = original.box; CHECK(!NativeNatureStorageWrite(&sEditNatureIntruder, target));
    sEditMutateNatureStorage = TRUE;
    CHECK(sEditTestService->commit_true_nature(&req, &quote, &result) == RHH_EDIT_STALE
        && !sEditMutateNatureStorage && !ModsNatureFormatEnabled() && !sTrueNatureNotifications
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)) && !memcmp(&bag, &gSaveBlock1Ptr->bag, sizeof(bag)));
    memset(&gPokemonStoragePtr->boxes[2][3], 0, sizeof(struct BoxPokemon));
    gWirelessCommType = TRUE;
    CHECK(sEditTestService->commit_true_nature(&req, &quote, &result) == RHH_EDIT_UNAVAILABLE && !ModsNatureFormatEnabled());
    gWirelessCommType = FALSE;
    CHECK(sEditTestService->commit_true_nature(&req, &quote, &result) == RHH_EDIT_OK
        && ModsNatureFormatEnabled() && sTrueNatureNotifications == 1 && GetMoney(&gSaveBlock1Ptr->money) == 800
        && CheckBagHasItem(ITEM_POTION, 1) && !CheckBagHasItem(ITEM_POTION, 2)
        && GetNature(&gParties[B_TRAINER_PLAYER][0]) == target
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HIDDEN_NATURE) == target
        && gParties[B_TRAINER_PLAYER][0].box.personality == original.box.personality
        && gParties[B_TRAINER_PLAYER][0].box.otId == original.box.otId
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_SHINY) == GetMonData(&original, MON_DATA_IS_SHINY));
    CHECK(sEditTestService->true_nature_context(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside)));
    struct BattlePokemon battler = {.personality = original.box.personality, .otId = original.box.otId};
    CaptureBattleMonNature(&battler, &gParties[B_TRAINER_PLAYER][0]);
    CHECK((battler.nativeNatureFlags & NATIVE_BATTLE_NATURE_SNAPSHOT) && battler.nativeNature == target);
    struct DayCare model = {0}; model.mons[0].mon = gParties[B_TRAINER_PLAYER][0].box;
    u16 everstone = ITEM_EVERSTONE; SetBoxMonData(&model.mons[0].mon, MON_DATA_HELD_ITEM, &everstone);
    CHECK(GetConfig(NATURE_INHERITANCE) >= GEN_5 && GetChildNature(&model) == target);
    u8 mint = (target + 2) % NUM_NATURES; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HIDDEN_NATURE, &mint);
    CalculateMonStats(&gParties[B_TRAINER_PLAYER][0]);
    req = (struct RhhTrueNatureEditRequest){.struct_size = sizeof(req), .stat_policy = RHH_TRUE_NATURE_FOLLOW_UNMINTED,
        .edit = {.struct_size = sizeof(req.edit), .expected.struct_size = sizeof(req.edit.expected)}};
    CHECK(!owned->party_at(0, &req.edit.expected)); target = (target + 2) % NUM_NATURES;
    CHECK(!nature->at(target, &info)); req.nature = info.key; nested = req.edit; nested.fields = RHH_EDIT_IVS;
    quote = (struct RhhTrueNatureEditResult){.struct_size = sizeof(quote)};
    CHECK(sEditTestService->preview_true_nature(&req, &quote) == RHH_EDIT_OK && !quote.activates_format
        && quote.edit.fields == RHH_EDIT_TRUE_NATURE && quote.edit.after.stat_nature == mint);
    CHECK(sEditTestService->commit_true_nature(&req, &quote, &result) == RHH_EDIT_OK
        && GetNature(&gParties[B_TRAINER_PLAYER][0]) == target
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HIDDEN_NATURE) == mint);
    CHECK(!owned->party_at(0, &req.edit.expected)); target = (target + 1) % NUM_NATURES;
    CHECK(!nature->at(target, &info)); req.nature = info.key; req.stat_policy = RHH_TRUE_NATURE_FOLLOW;
    nested = req.edit; nested.fields = RHH_EDIT_IVS; quote.struct_size = sizeof(quote);
    CHECK(sEditTestService->preview_true_nature(&req, &quote) == RHH_EDIT_OK
        && quote.edit.fields == (RHH_EDIT_TRUE_NATURE | RHH_EDIT_STAT_NATURE));
    CHECK(sEditTestService->commit_true_nature(&req, &quote, &result) == RHH_EDIT_OK
        && GetNature(&gParties[B_TRAINER_PLAYER][0]) == target
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HIDDEN_NATURE) == target);
    u32 lost = gParties[B_TRAINER_PLAYER][0].maxHP;
    gParties[B_TRAINER_PLAYER][0].hp = 0; SetBoxMonData(&gParties[B_TRAINER_PLAYER][0].box, MON_DATA_HP_LOST, &lost);
    gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][0].box; gPartiesCount[B_TRAINER_PLAYER] = 0;
    memset(&gParties[B_TRAINER_PLAYER][0], 0, sizeof(struct Pokemon));
    req = (struct RhhTrueNatureEditRequest){.struct_size = sizeof(req), .stat_policy = RHH_TRUE_NATURE_PRESERVE,
        .edit = {.struct_size = sizeof(req.edit), .expected.struct_size = sizeof(req.edit.expected)}};
    CHECK(!owned->box_at(0, 0, &req.edit.expected)); u32 oldStatNature = req.edit.expected.stat_nature;
    target = (target + 1) % NUM_NATURES; CHECK(!nature->at(target, &info)); req.nature = info.key;
    nested = req.edit; nested.fields = RHH_EDIT_IVS; quote.struct_size = sizeof(quote);
    CHECK(sEditTestService->preview_true_nature(&req, &quote) == RHH_EDIT_OK && !quote.edit.current_hp
        && quote.edit.after.stat_nature == oldStatNature);
    CHECK(sEditTestService->commit_true_nature(&req, &quote, &result) == RHH_EDIT_OK && !result.edit.current_hp);
    struct RhhOwnedMonSnapshot saved = {.struct_size = sizeof(saved)};
    CHECK(!owned->box_at(0, 0, &saved) && saved.nature == target && saved.stat_nature == oldStatNature);
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); u32 editedGeneration = gSaveCounter;
    CHECK(editedGeneration != legacyGeneration && !NativeSaveRestoreMods(legacyGeneration) && !ModsNatureFormatEnabled()
        && !NativeSaveRestoreMods(editedGeneration) && ModsNatureFormatEnabled());
    NativeSaveClose(); CHECK(!NativeSaveOpen(path) && LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && ModsNatureFormatEnabled());
    saved.struct_size = sizeof(saved);
    CHECK(!owned->box_at(0, 0, &saved) && saved.nature == target && saved.stat_nature == oldStatNature
        && saved.identity == req.edit.expected.identity && GetMoney(&gSaveBlock1Ptr->money) == 800);
    NativeSaveClose(); puts("true nature edit: atomic policies, costs, save and rollback passed");
}
