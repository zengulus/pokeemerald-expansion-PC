static void CheckBreedingNatureOdds(const struct RhhBreedingService *breeding)
{
    struct DayCare original = gSaveBlock1Ptr->daycare, model = original;
    u8 weights[3]; u16 item = ITEM_EVERSTONE;
    GetDaycareNatureSourceWeights(&model, GEN_2, weights);
    CHECK(!weights[0] && !weights[1] && weights[2] == 4);
    SetBoxMonData(&model.mons[0].mon, MON_DATA_HELD_ITEM, &item);
    GetDaycareNatureSourceWeights(&model, GEN_3, weights);
    CHECK(!weights[0] && !weights[1] && weights[2] == 4); /* Ditto, not its partner, is eligible. */
    SetBoxMonData(&model.mons[1].mon, MON_DATA_HELD_ITEM, &item);
    GetDaycareNatureSourceWeights(&model, GEN_3, weights);
    CHECK(!weights[0] && weights[1] == 2 && weights[2] == 2);
    GetDaycareNatureSourceWeights(&model, GEN_4, weights);
    CHECK(weights[0] == 2 && weights[1] == 1 && weights[2] == 1);
    GetDaycareNatureSourceWeights(&model, GEN_5, weights);
    CHECK(weights[0] == 2 && weights[1] == 2 && !weights[2]);
    model.mons[0].mon = model.mons[1].mon;
    GetDaycareNatureSourceWeights(&model, GEN_3, weights);
    CHECK(weights[0] == 1 && weights[1] == 1 && weights[2] == 2);
    item = ITEM_NONE; SetBoxMonData(&model.mons[0].mon, MON_DATA_HELD_ITEM, &item);
    GetDaycareNatureSourceWeights(&model, GEN_3, weights);
    CHECK(!weights[0] && weights[1] == 1 && weights[2] == 3);
    GetDaycareNatureSourceWeights(&model, GEN_4, weights);
    CHECK(!weights[0] && weights[1] == 2 && weights[2] == 2);
    GetDaycareNatureSourceWeights(&model, GEN_5, weights);
    CHECK(!weights[0] && weights[1] == 4 && !weights[2]);

    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingNatureOdds odds = {.struct_size = sizeof(odds)}, unchanged = odds;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(!breeding->offer(&offer) && !breeding->nature_odds(&offer, &odds)
        && odds.denominator == 100 && odds.source_denominator == 4 && odds.source_weights[2] == 4);
    for (u32 i = 0; i < RHH_NATURE_COUNT; ++i) CHECK(odds.weights[i] == 4);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && !memcmp(&original, &gSaveBlock1Ptr->daycare, sizeof(original)));
    struct RhhBreedingNatureOdds small = {.struct_size = 1}, oldSmall = small;
    CHECK(breeding->nature_odds(&offer, &small) && !memcmp(&small, &oldSmall, sizeof(small)));
    struct RhhBreedingNatureOdds beforeUnchanged = unchanged;
    item = ITEM_EVERSTONE; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &item);
    CHECK(breeding->nature_odds(&offer, &unchanged) && !memcmp(&unchanged, &beforeUnchanged, sizeof(unchanged)));
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon, MON_DATA_HELD_ITEM, &item);
    u8 mint = (offer.parents[0].mon.nature + 1) % NUM_NATURES;
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HIDDEN_NATURE, &mint);
    CHECK(!breeding->offer(&offer) && !breeding->nature_odds(&offer, &odds)
        && odds.generation >= 5 && odds.source_weights[0] == 2 && odds.source_weights[1] == 2 && !odds.source_weights[2]
        && offer.parents[0].mon.stat_nature == mint && offer.parents[0].mon.nature != mint);
    u32 first = offer.parents[0].mon.nature, second = offer.parents[1].mon.nature;
    CHECK(first != second && odds.weights[first] == 50 && odds.weights[second] == 50);
    struct BoxPokemon savedDitto = gSaveBlock1Ptr->daycare.mons[1].mon;
    UpdateMonPersonality(&gSaveBlock1Ptr->daycare.mons[1].mon, first);
    CHECK(!breeding->offer(&offer) && !breeding->nature_odds(&offer, &odds) && odds.weights[first] == 100);
    gSaveBlock1Ptr->daycare.mons[1].mon = savedDitto;
    CHECK(!breeding->offer(&offer) && !breeding->nature_odds(&offer, &odds)
        && odds.weights[first] == 50 && odds.weights[second] == 50);
    u32 seen = 0;
    for (u32 i = 0; i < 64; ++i)
    {
        FlagSet(FLAG_PENDING_DAYCARE_EGG); GiveEggFromDaycare();
        CHECK(gPartiesCount[B_TRAINER_PLAYER] == 1);
        u32 actual = GetNature(&gParties[B_TRAINER_PLAYER][0]);
        CHECK(actual == first || actual == second); seen |= 1u << (actual == second);
        memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    }
    CHECK(seen == 3); gSaveBlock1Ptr->daycare = original; FlagClear(FLAG_PENDING_DAYCARE_EGG);
    puts("breeding nature: source weights, purity, stale offers and actual eggs passed");
}

static void InvalidNatureReduce(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; (void)decision; }
static void InvalidNatureObserve(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)event; (void)decision; (void)state; }
static void CheckBreedingNatureRules(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins());
    struct RhhModHook invalid = {sizeof(invalid), RHH_BREED_NATURE, RHH_FINALIZE, 0, InvalidNatureReduce, NULL};
    struct RhhModDefinition invalidMod = {.struct_size = sizeof(invalidMod), .abi_version = RHH_MOD_ABI_SERVICES,
        .id = "test:invalid_nature_stage", .hook_count = 1, .hooks = &invalid};
    CHECK(RhhModsRegister(&invalidMod));
    invalid.stage = RHH_OBSERVE; invalid.reduce = NULL; invalid.observe = InvalidNatureObserve;
    CHECK(RhhModsRegister(&invalidMod));
    CHECK(!RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 10));
    CHECK(breeding && breeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_10_SIZE);
    struct RhhBreedingNatureContext outside = {.struct_size = sizeof(outside), .generation = 999}, savedOutside = outside;
    CHECK(breeding->nature_context(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside)));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    u8 *mode = RhhModsState("test:breeding_nature"); CHECK(mode);
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingNatureOdds odds = {.struct_size = sizeof(odds)};
    struct DayCare before = gSaveBlock1Ptr->daycare; rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(!breeding->offer(&offer));
    for (*mode = 0; *mode <= 5; ++*mode)
    {
        CHECK(!breeding->nature_odds(&offer, &odds) && odds.denominator == 100);
        for (u32 i = 0; i < NUM_NATURES; ++i)
            CHECK(odds.weights[i] == ((*mode == 1 || *mode == 4) ? (i == NATURE_ADAMANT ? 100 : 0) : 4));
    }
    *mode = 1;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision);
    CHECK(!memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    for (u32 i = 0; i < 8; ++i)
    {
        FlagSet(FLAG_PENDING_DAYCARE_EGG); GiveEggFromDaycare();
        CHECK(gPartiesCount[B_TRAINER_PLAYER] == 1 && GetNature(&gParties[B_TRAINER_PLAYER][0]) == NATURE_ADAMANT);
        memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    }
    u16 item = ITEM_EVERSTONE;
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &item);
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon, MON_DATA_HELD_ITEM, &item);
    CHECK(!breeding->offer(&offer));
    u32 first = offer.parents[0].mon.nature, second = offer.parents[1].mon.nature;
    CHECK(first != second && first != NATURE_ADAMANT && second != NATURE_ADAMANT);
    *mode = 3; CHECK(!breeding->nature_odds(&offer, &odds));
    for (u32 i = 0; i < NUM_NATURES; ++i) CHECK(odds.weights[i] == 4);
    *mode = 4; CHECK(!breeding->nature_odds(&offer, &odds) && odds.weights[first] == 50 && odds.weights[second] == 50);
    *mode = 5; CHECK(!breeding->nature_odds(&offer, &odds) && odds.weights[first] == 50 && odds.weights[second] == 50);
    for (u32 i = 0; i < 16; ++i)
    {
        FlagSet(FLAG_PENDING_DAYCARE_EGG); GiveEggFromDaycare();
        u32 actual = GetNature(&gParties[B_TRAINER_PLAYER][0]); CHECK(actual == first || actual == second);
        memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    }
    CHECK(breeding->nature_context(&outside) && !memcmp(&outside, &savedOutside, sizeof(outside)));
    RhhModsUnloadLibraries(); puts("breeding nature rules: transforms, constraints, preview and actual eggs passed");
}
/* Independently enumerate the engine's uniform parent bits, stat choices and
 * initial IV values, then compare the public marginal distribution. */
static void CheckIvOddsEnumeration(const struct RhhBreedingService *breeding, const struct RhhBreedingOffer *offer)
{
    struct DaycareIVRules rules; GetDaycareIVRules(&gSaveBlock1Ptr->daycare, &rules);
    struct RhhBreedingIvOdds odds = {.struct_size = sizeof(odds)};
    rng_value_t rng = gRngValue; struct DayCare daycare = gSaveBlock1Ptr->daycare;
    CHECK(!breeding->iv_odds(offer, &odds) && !odds.flags && odds.denominator == 960
        && odds.inherited_count == rules.inheritedCount && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&daycare, &gSaveBlock1Ptr->daycare, sizeof(daycare)));
    u32 counts[RHH_STAT_COUNT][32] = {{0}};
    for (u32 parentBits = 0; parentBits < 32; ++parentBits)
        for (u32 permutation = 0; permutation < 720; ++permutation)
        {
            u32 bits = parentBits, indices = permutation, available[6] = {0, 1, 2, 3, 4, 5};
            u32 sources[6] = {2, 2, 2, 2, 2, 2}, start = 0;
            if (rules.powerItemCount)
            {
                u32 parent = rules.powerStats[1] < 6 ? 1 : 0;
                if (rules.powerItemCount == 2) { parent = bits & 1; bits >>= 1; }
                u32 stat = rules.powerStats[parent]; sources[stat] = parent;
                for (u32 i = stat; i < 5; ++i) available[i] = available[i + 1];
                start = 1;
            }
            for (u32 i = start; i < rules.inheritedCount; ++i)
            {
                u32 index = indices % (6 - i); indices /= 6 - i;
                u32 stat = available[index]; sources[stat] = bits & 1; bits >>= 1;
                for (u32 j = index; j + 1 < 6 - i; ++j) available[j] = available[j + 1];
            }
            for (u32 stat = 0; stat < 6; ++stat)
                if (sources[stat] < 2) counts[stat][offer->parents[sources[stat]].mon.ivs[stat]] += 32;
                else for (u32 value = 0; value < 32; ++value) ++counts[stat][value];
        }
    for (u32 stat = 0; stat < 6; ++stat)
    {
        u32 total = 0;
        for (u32 value = 0; value < 32; ++value)
        {
            CHECK(counts[stat][value] == odds.weights[stat][value] * 768u);
            total += odds.weights[stat][value];
        }
        CHECK(total == odds.denominator);
    }
}

/* Analytical checks cover joint events and correlations independently of the
 * implementation's enumeration of stat order and parent bits. */
static void CheckIvJointOdds(const struct RhhBreedingService *breeding, const struct RhhBreedingOffer *offer)
{
    struct DaycareIVRules rules; GetDaycareIVRules(&gSaveBlock1Ptr->daycare, &rules);
    struct RhhBreedingIvJointRequest request = {.struct_size = sizeof(request)};
    struct RhhBreedingIvJointOdds joint = {.struct_size = sizeof(joint)};
    struct RhhBreedingIvOdds marginal = {.struct_size = sizeof(marginal)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value; struct DayCare daycare = gSaveBlock1Ptr->daycare;
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i) request.values[i] = UINT32_MAX;
    CHECK(!breeding->iv_joint_odds(offer, &request, &joint) && !joint.flags && joint.denominator
        && joint.numerator == joint.denominator && joint.inherited_count == rules.inheritedCount);
    CHECK(!breeding->iv_odds(offer, &marginal));
    for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
    {
        request.values[stat] = 1u << 31;
        CHECK(!breeding->iv_joint_odds(offer, &request, &joint)
            && joint.numerator * marginal.denominator == (u64)marginal.weights[stat][31] * joint.denominator);
        request.values[stat] = UINT32_MAX;
    }
    request.values[0] = 0;
    CHECK(!breeding->iv_joint_odds(offer, &request, &joint) && !joint.numerator);
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i) request.values[i] = (1u << 31) | (1u << 10);
    CHECK(!breeding->iv_joint_odds(offer, &request, &joint)
        && joint.numerator * (UINT64_C(1) << (4 * (6 - rules.inheritedCount))) == joint.denominator);
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i) request.values[i] = 1u << 31;
    CHECK(!breeding->iv_joint_odds(offer, &request, &joint));
    u32 forced = rules.powerItemCount ? 1 : 0;
    u64 divisor = (UINT64_C(1) << (rules.inheritedCount - forced + 5 * (6 - rules.inheritedCount)))
        * (rules.powerItemCount == 2 ? 2 : 1);
    bool32 possible = rules.powerItemCount != 1 || rules.powerStats[0] < RHH_STAT_COUNT;
    CHECK(possible ? joint.numerator * divisor == joint.denominator : !joint.numerator);
    if (!rules.powerItemCount && rules.inheritedCount == 3)
    {
        for (u32 i = 0; i < RHH_STAT_COUNT; ++i) request.values[i] = UINT32_MAX;
        request.values[0] = request.values[1] = 1u << 31;
        CHECK(!breeding->iv_joint_odds(offer, &request, &joint)
            && joint.numerator * 1024 == joint.denominator * 61);
    }
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && !memcmp(&daycare, &gSaveBlock1Ptr->daycare, sizeof(daycare)));
}

static void CheckBreedingJointExternal(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    struct DayCare daycare = gSaveBlock1Ptr->daycare;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision);
    CHECK(!memcmp(&daycare, &gSaveBlock1Ptr->daycare, sizeof(daycare))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    RhhModsUnloadLibraries();
}

static void CheckBreedingCount(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 7));
    CHECK(breeding && breeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_7_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 6)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 8)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 9)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 10)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 11)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 12)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 13)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 14)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 15)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 16)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 17)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 18)) == &breeding->header
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 19)) == &breeding->header
        && !RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 29)));
    struct RhhBreedingIvCountContext outside = {.struct_size = sizeof(outside), .base_count = 999}, saved = outside;
    CHECK(breeding->iv_count_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct Pokemon parent, egg;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    CreateMonWithIVs(&egg, SPECIES_TORCHIC, 5, 67890, OTID_STRUCT_PRESET(777), 5);
    struct Pokemon initial = egg;
    u8 *mode = RhhModsState("test:breeding_count"); CHECK(mode);
    const u16 items[][2] = {{ITEM_NONE, ITEM_NONE}, {ITEM_POWER_WEIGHT, ITEM_NONE},
                            {ITEM_POWER_WEIGHT, ITEM_POWER_ANKLET}, {ITEM_DESTINY_KNOT, ITEM_POWER_WEIGHT}};
    for (u32 setup = 0; setup < ARRAY_COUNT(items); ++setup)
    {
        for (u32 p = 0; p < 2; ++p) SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[p].mon, MON_DATA_HELD_ITEM, &items[setup][p]);
        for (u32 count = 0; count <= RHH_STAT_COUNT; ++count)
        {
            *mode = count;
            struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
            struct RhhBreedingIvPreview preview = {.struct_size = sizeof(preview)};
            struct RhhBreedingIvOdds marginal = {.struct_size = sizeof(marginal)};
            struct RhhBreedingIvJointOdds joint = {.struct_size = sizeof(joint)};
            struct RhhBreedingIvJointRequest request = {.struct_size = sizeof(request)};
            for (u32 i = 0; i < RHH_STAT_COUNT; ++i) request.values[i] = 1u << 31;
            rng_value_t before = gRngValue, before2 = gRng2Value; struct DayCare daycare = gSaveBlock1Ptr->daycare;
            CHECK(!breeding->offer(&offer) && !breeding->iv_preview(&offer, &preview)
                && !breeding->iv_odds(&offer, &marginal) && !breeding->iv_joint_odds(&offer, &request, &joint));
            CHECK(preview.inherited_count == count && marginal.inherited_count == count
                && marginal.denominator == 960 && joint.inherited_count == count && joint.denominator
                && !joint.flags && !memcmp(&before, &gRngValue, sizeof(before))
                && !memcmp(&before2, &gRng2Value, sizeof(before2))
                && !memcmp(&daycare, &gSaveBlock1Ptr->daycare, sizeof(daycare)));
            for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
            {
                u32 sum = 0; for (u32 iv = 0; iv < 32; ++iv) sum += marginal.weights[stat][iv];
                CHECK(sum == marginal.denominator);
            }
            u32 forced = count && setup; u64 divisor = UINT64_C(1) << (count - !!forced + 5 * (6 - count));
            if (count && setup == 2) divisor *= 2;
            CHECK(count && setup == 3 ? !joint.numerator : joint.numerator * divisor == joint.denominator);
            if (count == 6)
                for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
                    CHECK(preview.values[stat] && !(preview.values[stat] & ~((1u << 31) | (1u << 10))));
            RandomUniform(RNG_DAYCARE_PICK_IVS_PARENT, 0, count == 6 ? 63 : 31);
            RandomUniform(RNG_DAYCARE_INHERITED_STATS, 0, 719);
            rng_value_t expectedRng = gRngValue; gRngValue = before; egg = initial;
            InheritIVs(&egg, &gSaveBlock1Ptr->daycare);
            CHECK(!memcmp(&expectedRng, &gRngValue, sizeof(expectedRng)) && !memcmp(&before2, &gRng2Value, sizeof(before2)));
            u32 inherited = 0;
            for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
            {
                u32 iv = GetMonData(&egg, MON_DATA_HP_IV + stat);
                CHECK(iv == 5 || iv == 10 || iv == 31);
                inherited += iv != 5;
            }
            CHECK(inherited == count);
            if (count && setup == 1) CHECK(GetMonData(&egg, MON_DATA_HP_IV) == 31);
            if (count && setup == 3) CHECK(GetMonData(&egg, MON_DATA_HP_IV) == 10);
        }
    }
    u16 none = ITEM_NONE;
    for (u32 p = 0; p < 2; ++p) SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[p].mon, MON_DATA_HELD_ITEM, &none);
    rng_value_t seed = gRngValue; struct Pokemon baseline = initial;
    RhhModsSetRunContext(0, 0, 0); InheritIVs(&baseline, &gSaveBlock1Ptr->daycare); rng_value_t baselineRng = gRngValue;
    RhhModsSetRunContext(123, 456, 0);
    for (u32 invalid = 7; invalid <= 9; ++invalid)
    {
        *mode = invalid; egg = initial; gRngValue = seed;
        InheritIVs(&egg, &gSaveBlock1Ptr->daycare);
        CHECK(!memcmp(&egg, &baseline, sizeof(egg)) && !memcmp(&gRngValue, &baselineRng, sizeof(baselineRng)));
    }
    CHECK(breeding->iv_count_context(&outside) && !memcmp(&outside, &saved, sizeof(outside)));
    RhhModsUnloadLibraries(); puts("breeding count: zero through six, previews, items and RNG passed");
}

static void CheckBreedingIvPreview(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsFinalize()); RhhModsSetRunContext(123, 456, 0);
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhBreedingService *breeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 6));
    CHECK(breeding && breeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_6_SIZE
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 5)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 7)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 8)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 9)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 10)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 11)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 12)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 13)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 14)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 15)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 16)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 17)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 18)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 19)) == &breeding->header
        && !host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 29))
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 4)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 3)) == &breeding->header);
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)}, stale;
    struct RhhBreedingIvPreview preview = {.struct_size = sizeof(preview)}, before;
    CHECK(!breeding->offer(&offer)); before = preview;
    CHECK(breeding->iv_preview(&offer, &preview) && !memcmp(&before, &preview, sizeof(preview)));
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    CheckBreedingNatureOdds(breeding);
    CHECK(!breeding->offer(&offer)); stale = offer; rng_value_t rng = gRngValue;
    struct DayCare daycare = gSaveBlock1Ptr->daycare;
    CHECK(!breeding->iv_preview(&offer, &preview) && !preview.flags && preview.inherited_count == 3
        && !preview.guaranteed_stats && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&daycare, &gSaveBlock1Ptr->daycare, sizeof(daycare)));
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i) CHECK(preview.values[i] == UINT32_MAX);
    CheckIvOddsEnumeration(breeding, &offer); CheckIvJointOdds(breeding, &offer);
    u16 knot = ITEM_DESTINY_KNOT; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon, MON_DATA_HELD_ITEM, &knot);
    CHECK(!breeding->offer(&offer)); CheckIvOddsEnumeration(breeding, &offer); CheckIvJointOdds(breeding, &offer);
    knot = ITEM_NONE; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon, MON_DATA_HELD_ITEM, &knot);
    CHECK(!breeding->offer(&offer)); stale = offer;
    struct RhhBreedingIvOdds odds = {.struct_size = sizeof(odds)}, oldOdds = odds;
    struct RhhBreedingIvJointRequest jointRequest = {.struct_size = sizeof(jointRequest)};
    struct RhhBreedingIvJointOdds joint = {.struct_size = sizeof(joint), .numerator = 999}, oldJoint = joint;
    struct RhhBreedingIvJointRequest tinyRequest = {.struct_size = 1};
    struct RhhBreedingIvJointOdds tinyJoint = {.struct_size = 1, .numerator = 999}, oldTinyJoint = tinyJoint;
    CHECK(breeding->iv_joint_odds(&offer, &tinyRequest, &joint) && !memcmp(&joint, &oldJoint, sizeof(joint)));
    CHECK(breeding->iv_joint_odds(&offer, &jointRequest, &tinyJoint) && !memcmp(&tinyJoint, &oldTinyJoint, sizeof(tinyJoint)));
    jointRequest.reserved[0] = 1;
    CHECK(breeding->iv_joint_odds(&offer, &jointRequest, &joint) && !memcmp(&joint, &oldJoint, sizeof(joint)));
    jointRequest.reserved[0] = 0;
    struct RhhBreedingIvOdds smallOdds = {.struct_size = sizeof(smallOdds) - 1}, oldSmallOdds = smallOdds;
    CHECK(breeding->iv_odds(&offer, &smallOdds) && !memcmp(&smallOdds, &oldSmallOdds, sizeof(smallOdds)));
    struct RhhBreedingIvPreview small = {.struct_size = sizeof(small) - 1, .flags = 99}, oldSmall = small;
    CHECK(breeding->iv_preview(&offer, &small) && !memcmp(&small, &oldSmall, sizeof(small)));
    u16 item = ITEM_POWER_WEIGHT; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &item);
    before = preview; CHECK(breeding->iv_preview(&stale, &preview) && !memcmp(&before, &preview, sizeof(preview)));
    CHECK(breeding->iv_odds(&stale, &odds) && !memcmp(&odds, &oldOdds, sizeof(odds)));
    CHECK(breeding->iv_joint_odds(&stale, &jointRequest, &joint) && !memcmp(&joint, &oldJoint, sizeof(joint)));
    CHECK(!breeding->offer(&offer) && !breeding->iv_preview(&offer, &preview)
        && preview.guaranteed_stats == (1u << RHH_STAT_HP) && preview.values[RHH_STAT_HP] == (1u << 31));
    CheckIvOddsEnumeration(breeding, &offer); CheckIvJointOdds(breeding, &offer);
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon, MON_DATA_HELD_ITEM, &item);
    CHECK(!breeding->offer(&offer) && !breeding->iv_preview(&offer, &preview)
        && preview.guaranteed_stats == (1u << RHH_STAT_HP) && preview.values[RHH_STAT_HP] == ((1u << 31) | (1u << 10)));
    CheckIvOddsEnumeration(breeding, &offer); CheckIvJointOdds(breeding, &offer);
    item = ITEM_POWER_ANKLET; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon, MON_DATA_HELD_ITEM, &item);
    CHECK(!breeding->offer(&offer) && !breeding->iv_preview(&offer, &preview)
        && !preview.guaranteed_stats && preview.values[RHH_STAT_HP] == UINT32_MAX && preview.values[RHH_STAT_SPEED] == UINT32_MAX);
    CheckIvOddsEnumeration(breeding, &offer); CheckIvJointOdds(breeding, &offer);
    item = ITEM_DESTINY_KNOT; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &item);
    item = ITEM_POWER_WEIGHT; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon, MON_DATA_HELD_ITEM, &item);
    CHECK(!breeding->offer(&offer) && !breeding->iv_preview(&offer, &preview)
        && preview.inherited_count == 5 && preview.guaranteed_stats == (1u << RHH_STAT_HP)
        && preview.values[RHH_STAT_HP] == (1u << 10));
    CheckIvOddsEnumeration(breeding, &offer); CheckIvJointOdds(breeding, &offer);
    for (u32 i = 0; i < 64; ++i)
    {
        FlagSet(FLAG_PENDING_DAYCARE_EGG); GiveEggFromDaycare();
        CHECK(gPartiesCount[B_TRAINER_PLAYER] == 1 && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG));
        for (u32 stat = 0; stat < RHH_STAT_COUNT; ++stat)
            CHECK(preview.values[stat] & (1u << GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV + stat)));
        memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    }
    item = ITEM_POWER_WEIGHT; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &item);
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon, MON_DATA_HELD_ITEM, &item);
    CHECK(!breeding->offer(&offer) && !breeding->iv_preview(&offer, &preview));
    u32 seenHp = 0;
    for (u32 i = 0; i < 64; ++i)
    {
        FlagSet(FLAG_PENDING_DAYCARE_EGG); GiveEggFromDaycare();
        u32 hp = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV);
        CHECK(preview.values[RHH_STAT_HP] & (1u << hp)); seenHp |= 1u << hp;
        memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    }
    CHECK(seenHp == ((1u << 31) | (1u << 10)));
    item = ITEM_POWER_ANKLET; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon, MON_DATA_HELD_ITEM, &item);
    CHECK(!breeding->offer(&offer) && !breeding->iv_preview(&offer, &preview) && !preview.guaranteed_stats);
    for (u32 i = 0; i < 64; ++i)
    {
        FlagSet(FLAG_PENDING_DAYCARE_EGG); GiveEggFromDaycare();
        CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV) == 31
            || GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPEED_IV) == 10);
        memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    }
    RhhModsUnloadLibraries(); puts("breeding IV preview: held items, stale offers and generated eggs passed");
}
