static void CheckBreedingAccept(const char *rules, const char *probe)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(probe)
        && !RhhModsLoadLibrary(rules) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhBreedingService *breeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 4));
    CHECK(breeding && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 2)) == &breeding->header);
    struct RhhBreedingAcceptResult result = {.struct_size = sizeof(result)}, small = {.struct_size = sizeof(small) - 1, .status = 999}, oldSmall = small;
    CHECK(breeding->accept_context(&result) && breeding->accept(NULL, &small) == RHH_BREED_ACCEPT_INVALID
        && !memcmp(&small, &oldSmall, sizeof(small)));
    struct RhhBreedingOffer smallOffer = {.struct_size = sizeof(smallOffer) - 1, .pending = 99}, oldSmallOffer = smallOffer;
    CHECK(breeding->offer(&smallOffer) && !memcmp(&smallOffer, &oldSmallOffer, sizeof(smallOffer)));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_CHARMANDER, 12, 12345, OTID_STRUCT_PRESET(12321), 10);
    u16 shadow = MOVE_SHADOW_BALL; SetMonData(&parent, MON_DATA_MOVE2, &shadow);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    gMain.inBattle = FALSE; gMain.callback2 = CB2_Overworld; memset(&gPaletteFade, 0, sizeof(gPaletteFade));
    ScriptContext_Init(); UnlockPlayerFieldControls(); FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)}, oldOffer;
    struct DayCare before = gSaveBlock1Ptr->daycare; rng_value_t rng = gRngValue;
    CHECK(!breeding->offer(&offer) && !offer.pending && offer.parent_count == 2
        && offer.compatibility == RHH_DAYCARE_MEDIUM && !offer.party_count
        && !memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    struct RhhBreedingIvPreview ivPreview = {.struct_size = sizeof(ivPreview)};
    CHECK(!breeding->iv_preview(&offer, &ivPreview) && (ivPreview.flags & RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE)
        && !ivPreview.guaranteed_stats && ivPreview.values[RHH_STAT_HP] == UINT32_MAX);
    struct RhhBreedingIvOdds odds = {.struct_size = sizeof(odds)};
    CHECK(!breeding->iv_odds(&offer, &odds) && (odds.flags & RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE)
        && !odds.denominator && !odds.weights[RHH_STAT_HP][31]);
    struct RhhBreedingIvJointRequest jointRequest = {.struct_size = sizeof(jointRequest)};
    struct RhhBreedingIvJointOdds joint = {.struct_size = sizeof(joint)};
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i) jointRequest.values[i] = 1u << 31;
    CHECK(!breeding->iv_joint_odds(&offer, &jointRequest, &joint)
        && (joint.flags & RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE) && !joint.numerator && !joint.denominator);
    u16 power = ITEM_POWER_WEIGHT; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &power);
    CHECK(!breeding->offer(&offer) && !breeding->iv_preview(&offer, &ivPreview)
        && (ivPreview.flags & RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE)
        && !ivPreview.guaranteed_stats && ivPreview.values[RHH_STAT_HP] == UINT32_MAX);
    power = ITEM_NONE; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &power);
    CHECK(!breeding->offer(&offer));
    CHECK(breeding->accept(&offer, &result) == RHH_BREED_ACCEPT_NO_EGG && !FlagGet(FLAG_PENDING_DAYCARE_EGG));
    FlagSet(FLAG_PENDING_DAYCARE_EGG); CHECK(!breeding->offer(&offer)); oldOffer = offer;
    struct RhhModEvent tick = {.struct_size = sizeof(tick), .kind = RHH_MOD_UPDATE, .args = {77, 0, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&tick, &decision);
    CHECK(!breeding->offer(&offer) && offer.token == oldOffer.token
        && offer.preview.candidates[0].offspring != oldOffer.preview.candidates[0].offspring
        && breeding->accept(&oldOffer, &result) == RHH_BREED_ACCEPT_STALE);
    RhhModsDispatch(&tick, &decision); CHECK(!breeding->offer(&offer)); oldOffer = offer;
    gSaveBlock2Ptr->playerTrainerId[0] ^= 1;
    CHECK(breeding->accept(&oldOffer, &result) == RHH_BREED_ACCEPT_STALE);
    gSaveBlock2Ptr->playerTrainerId[0] ^= 1;
    u16 item = ITEM_EVERSTONE; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &item);
    u8 natural = GetBoxMonNature(&gSaveBlock1Ptr->daycare.mons[0].mon);
    u8 originalStatNature = GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HIDDEN_NATURE);
    u8 changedStatNature = (natural + 1) % NUM_NATURES;
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HIDDEN_NATURE, &changedStatNature);
    CHECK(GetConfig(NATURE_INHERITANCE) >= GEN_5);
    struct DayCare natureBefore = gSaveBlock1Ptr->daycare; rng = gRngValue;
    CHECK(GetChildNature(&gSaveBlock1Ptr->daycare) == natural
        && !memcmp(&natureBefore, &gSaveBlock1Ptr->daycare, sizeof(natureBefore))
        && !memcmp(&rng, &gRngValue, sizeof(rng)));
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HIDDEN_NATURE, &originalStatNature);
    before = gSaveBlock1Ptr->daycare; rng = gRngValue;
    CHECK(breeding->accept(&oldOffer, &result) == RHH_BREED_ACCEPT_STALE && FlagGet(FLAG_PENDING_DAYCARE_EGG)
        && !gPartiesCount[B_TRAINER_PLAYER] && !memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before))
        && !memcmp(&rng, &gRngValue, sizeof(rng)));
    gMain.callback2 = NULL; CHECK(breeding->accept(&oldOffer, &result) == RHH_BREED_ACCEPT_UNAVAILABLE);
    gMain.callback2 = CB2_Overworld;
    CHECK(!breeding->offer(&offer)); ScriptContext_Enable();
    CHECK(breeding->accept(&offer, &result) == RHH_BREED_ACCEPT_UNAVAILABLE); ScriptContext_Init(); UnlockPlayerFieldControls();
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = parent;
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE; CHECK(!breeding->offer(&offer)); rng = gRngValue;
    CHECK(breeding->accept(&offer, &result) == RHH_BREED_ACCEPT_PARTY_FULL && FlagGet(FLAG_PENDING_DAYCARE_EGG)
        && !memcmp(&rng, &gRngValue, sizeof(rng)));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    struct DayCare pair = gSaveBlock1Ptr->daycare;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 555, OTID_STRUCT_PRESET(777), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    CHECK(!breeding->offer(&offer) && offer.compatibility == RHH_DAYCARE_INCOMPATIBLE
        && breeding->accept(&offer, &result) == RHH_BREED_ACCEPT_INCOMPATIBLE && FlagGet(FLAG_PENDING_DAYCARE_EGG));
    gSaveBlock1Ptr->daycare = pair;
    u32 iv = 7; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HP_IV, &iv);
    CHECK(!breeding->offer(&offer)); before = gSaveBlock1Ptr->daycare; rng = gRngValue;
    CHECK(breeding->accept(&offer, &result) == RHH_BREED_ACCEPT_DENIED && !strcmp(result.reason, "test rule denied egg")
        && FlagGet(FLAG_PENDING_DAYCARE_EGG) && !gPartiesCount[B_TRAINER_PLAYER]
        && !memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    iv = 31; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HP_IV, &iv);
    CHECK(breeding->accept(&offer, &result) == RHH_BREED_ACCEPT_STALE);
    gParties[B_TRAINER_PLAYER][0] = parent; gParties[B_TRAINER_PLAYER][2] = parent;
    gPartiesCount[B_TRAINER_PLAYER] = 2;
    CHECK(!breeding->offer(&offer) && breeding->accept(&offer, &result) == RHH_BREED_ACCEPT_OK
        && result.egg_valid && result.party_slot == 2 && result.egg.is_egg && result.egg.ivs[RHH_STAT_HP] == 31
        && result.egg.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_EEVEE)
        && !FlagGet(FLAG_PENDING_DAYCARE_EGG) && gPartiesCount[B_TRAINER_PLAYER] == 3
        && GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES) == SPECIES_DITTO);
    bool32 inherited = FALSE;
    for (u32 i = 0; i < 4; ++i) inherited |= GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_MOVE1 + i) == MOVE_SHADOW_BALL;
    CHECK(inherited && breeding->accept(&offer, &result) == RHH_BREED_ACCEPT_STALE);
    CHECK(!breeding->offer(&offer) && breeding->accept(&offer, &result) == RHH_BREED_ACCEPT_NO_EGG);
    CHECK(breeding->accept_context(&result));
    RhhModsUnloadLibraries(); puts("breeding accept: live offer, constraints and committed egg passed");
}
