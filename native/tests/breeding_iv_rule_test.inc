static void CheckBreedingIvRule(const char *module, const char *legacy)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module));
    if (legacy) CHECK(!RhhModsLoadLibrary(legacy));
    CHECK(!RhhModsFinalize()); RhhModsSetRunContext(123, 456, 0);
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    u8 *mode = RhhModsState("test:breeding_iv_rule"); CHECK(mode);
    u8 *countMode = RhhModsState("test:breeding_count");
    bool32 legacyHook = legacy && !countMode;
    if (countMode) *countMode = 3;
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 11));
    CHECK(breeding && breeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_11_SIZE);
    struct RhhBreedingIvRuleContext outside = {.struct_size = sizeof(outside), .stat = 99}, savedOutside = outside;
    CHECK(breeding->iv_rule_context(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside)));
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)}; CHECK(!breeding->offer(&offer));
    struct RhhBreedingIvPreview preview = {.struct_size = sizeof(preview)}, baseline = {.struct_size = sizeof(baseline)};
    struct RhhBreedingIvOdds odds = {.struct_size = sizeof(odds)};
    struct RhhBreedingIvJointRequest request = {.struct_size = sizeof(request)};
    struct RhhBreedingIvJointOdds joint = {.struct_size = sizeof(joint)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; struct DayCare saved = gSaveBlock1Ptr->daycare;
    *mode = 3; CHECK(!breeding->iv_preview(&offer, &baseline));
    *mode = 2; CHECK(!breeding->iv_preview(&offer, &preview) && !memcmp(&preview, &baseline, sizeof(preview)));
    *mode = 0;
    CHECK(!breeding->iv_preview(&offer, &preview) && preview.values[0] == (1u << 31)
        && preview.values[1] == (1u << 10) && preview.values[2] == UINT32_MAX
        && (preview.guaranteed_stats & 3) == 3);
    CHECK(!breeding->iv_odds(&offer, &odds) && !breeding->iv_joint_odds(&offer, &request, &joint));
    if (!legacyHook)
    {
        CHECK(!odds.flags && odds.denominator == 960 && odds.weights[0][31] == 960
            && odds.weights[1][10] == 960 && odds.weights[2][5] == 30 && joint.denominator);
        for (u32 i = 0; i < RHH_STAT_COUNT; ++i) request.values[i] = UINT32_MAX;
        request.values[0] = 1u << 31; request.values[1] = 1u << 10; request.values[2] = 1u << 5;
        CHECK(!breeding->iv_joint_odds(&offer, &request, &joint) && joint.numerator * 32 == joint.denominator);
        request.values[2] = UINT32_MAX; request.values[3] = request.values[4] = 1u << 31;
        CHECK(!breeding->iv_joint_odds(&offer, &request, &joint) && joint.numerator * 1024 == joint.denominator * 61);
        *mode = 1;
        CHECK(!breeding->iv_preview(&offer, &preview) && preview.guaranteed_stats == 63
            && !breeding->iv_odds(&offer, &odds) && odds.weights[3][10] == 960);
        for (u32 i = 0; i < RHH_STAT_COUNT; ++i) request.values[i] = 1u << (i < 3 ? 31 : 10);
        CHECK(!breeding->iv_joint_odds(&offer, &request, &joint) && joint.numerator == joint.denominator);
        if (countMode)
        {
            *mode = 0;
            for (u32 count = 0; count <= RHH_STAT_COUNT; ++count)
            {
                *countMode = count;
                CHECK(!breeding->offer(&offer) && !breeding->iv_odds(&offer, &odds) && odds.inherited_count == count);
                for (u32 i = 0; i < RHH_STAT_COUNT; ++i) request.values[i] = UINT32_MAX;
                request.values[3] = 1u << 31;
                CHECK(!breeding->iv_joint_odds(&offer, &request, &joint) && joint.inherited_count == count
                    && joint.numerator * odds.denominator == (u64)odds.weights[3][31] * joint.denominator);
            }
            *countMode = 3; CHECK(!breeding->offer(&offer));
        }
    }
    else CHECK((odds.flags & RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE) && !odds.denominator
        && (joint.flags & RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE) && !joint.denominator);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && !memcmp(&saved, &gSaveBlock1Ptr->daycare, sizeof(saved)));
    *mode = 0;
    /* A pure initial-IV rule overrides even an engine power-item guarantee. */
    u16 powerBelt = ITEM_POWER_BELT;
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &powerBelt);
    CHECK(!breeding->offer(&offer) && !breeding->iv_preview(&offer, &preview)
        && preview.values[2] == UINT32_MAX && !(preview.guaranteed_stats & (1u << 2)));
    if (!legacyHook) CHECK(!breeding->iv_odds(&offer, &odds) && odds.weights[2][5] == 30);
    struct Pokemon egg; CreateMonWithIVs(&egg, SPECIES_TORCHIC, 5, 67890, OTID_STRUCT_PRESET(777), 5);
    u8 initial[NUM_STATS], slots[2] = {0, 1};
    for (u32 i = 0; i < NUM_STATS; ++i) initial[i] = GetMonData(&egg, MON_DATA_HP_IV + i);
    InheritIVs(&egg, &gSaveBlock1Ptr->daycare);
    rng = gRngValue; rng2 = gRng2Value;
    ModsBreedingInheritIVs(&gSaveBlock1Ptr->daycare, &egg, SPECIES_TORCHIC,
                           GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare), slots, initial);
    CHECK(GetMonData(&egg, MON_DATA_HP_IV) == 31 && GetMonData(&egg, MON_DATA_ATK_IV) == 10
        && GetMonData(&egg, MON_DATA_DEF_IV) == 5 && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    FlagSet(FLAG_PENDING_DAYCARE_EGG); GiveEggFromDaycare();
    CHECK(gPartiesCount[B_TRAINER_PLAYER] == 1 && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG)
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV) == 31
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ATK_IV) == 10);
    RhhModsUnloadLibraries(); puts("breeding IV rules: preview, probabilities and actual eggs passed");
}
