static const struct RhhMonEditService *sEditPurityService;
static u32 sEditPurityMode, sEditPurityReceipts;
static void EditPurityConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhMonEditResult context = {.struct_size = sizeof(context)};
    CHECK(!sEditPurityService->context(&context));
    if (sEditPurityMode == 1 || sEditPurityMode == 2) { (void)Random(); (void)Random2(); }
    if (sEditPurityMode == 2) { decision->allowed = 0; strcpy(decision->reason, "edit veto"); }
}
static void EditPurityReceipt(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhMonEditResult context = {.struct_size = sizeof(context)};
    CHECK(!sEditPurityService->context(&context) && !context.is_preview && context.status == RHH_EDIT_OK);
    ++sEditPurityReceipts;
    if (sEditPurityMode == 3) { (void)Random(); (void)Random2(); }
}
static const struct RhhModHook sEditPurityHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_MON, RHH_CONSTRAINT, 0, EditPurityConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_MON_EDITED, RHH_OBSERVE, 0, NULL, EditPurityReceipt}
};
static const struct RhhModDefinition sEditPurityMod = {.struct_size = sizeof(sEditPurityMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:edit_purity",
    .hooks = sEditPurityHooks, .hook_count = ARRAY_COUNT(sEditPurityHooks)};
static void CheckMonEditPurity(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sEditPurityMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0); gMain.inBattle = FALSE; gBattleTypeFlags = 0; ClearBag();
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][0];
    CreateMonWithIVs(mon, SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMoney(&gSaveBlock1Ptr->money, 1000); CHECK(AddBagItem(ITEM_POTION, 2));
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *owned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    sEditPurityService = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 8));
    CHECK(owned && sEditPurityService && sEditPurityService->header.struct_size >= RHH_MON_EDIT_SERVICE_V1_8_SIZE
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 7)) == &sEditPurityService->header
        && !host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 9)));
    struct RhhMonEditPaidRequest request = {.struct_size = sizeof(request),
        .edit = {.struct_size = sizeof(request.edit), .fields = RHH_EDIT_IVS},
        .cost = {.struct_size = sizeof(request.cost), .item_quantity = 1, .money = 200}};
    request.cost.item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION);
    request.edit.expected.struct_size = sizeof(request.edit.expected); CHECK(!owned->party_at(0, &request.edit.expected));
    memcpy(request.edit.ivs, request.edit.expected.ivs, sizeof(request.edit.ivs)); request.edit.ivs[0] = 20;
    struct RhhMonEditPaidResult quote = {.struct_size = sizeof(quote)}, result = {.struct_size = sizeof(result)};
    struct Pokemon original = *mon; struct Bag bag = gSaveBlock1Ptr->bag;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sEditPurityMode = 1;
    struct RhhMonEditResult unpaid = {.struct_size = sizeof(unpaid)};
    CHECK(sEditPurityService->preview(&request.edit, &unpaid) == RHH_EDIT_DENIED
        && sEditPurityService->commit(&request.edit, &unpaid) == RHH_EDIT_DENIED
        && !sEditPurityReceipts && !memcmp(mon, &original, sizeof(original))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    CHECK(sEditPurityService->preview_paid(&request, &result) == RHH_EDIT_DENIED
        && strstr(result.edit.reason, "RNG") && !sEditPurityReceipts);
    CHECK(!memcmp(mon, &original, sizeof(original)) && !memcmp(&bag, &gSaveBlock1Ptr->bag, sizeof(bag))
        && GetMoney(&gSaveBlock1Ptr->money) == 1000 && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sEditPurityMode = 2;
    CHECK(sEditPurityService->preview_paid(&request, &result) == RHH_EDIT_DENIED
        && !strcmp(result.edit.reason, "edit veto") && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sEditPurityMode = 0; CHECK(sEditPurityService->preview_paid(&request, &quote) == RHH_EDIT_OK);
    sEditPurityMode = 1;
    CHECK(sEditPurityService->commit_paid_previewed(&request, &quote, &result) == RHH_EDIT_DENIED
        && !sEditPurityReceipts && !memcmp(mon, &original, sizeof(original))
        && !memcmp(&bag, &gSaveBlock1Ptr->bag, sizeof(bag)) && GetMoney(&gSaveBlock1Ptr->money) == 1000
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sEditPurityMode = 3;
    CHECK(sEditPurityService->commit_paid_previewed(&request, &quote, &result) == RHH_EDIT_OK
        && sEditPurityReceipts == 1 && GetMonData(mon, MON_DATA_HP_IV) == 20
        && GetMoney(&gSaveBlock1Ptr->money) == 800 && CheckBagHasItem(ITEM_POTION, 1)
        && !CheckBagHasItem(ITEM_POTION, 2) && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    puts("mon edit purity: paid preview, veto, checked commit and observer preserve both RNG streams");
}
