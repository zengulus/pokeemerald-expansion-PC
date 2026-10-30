static void CheckBreedingMoveRule(const char *module, const char *legacy)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module));
    if (legacy) CHECK(!RhhModsLoadLibrary(legacy));
    CHECK(!RhhModsFinalize()); RhhModsSetRunContext(123, 456, 0);
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_EEVEE, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonMoveSlot(&parent, MOVE_SHADOW_BALL, 1); SetMonMoveSlot(&parent, MOVE_HYDRO_PUMP, 2);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 20, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box; FlagSet(FLAG_PENDING_DAYCARE_EGG);
    u8 *mode = RhhModsState("test:breeding_move_rule"); CHECK(mode);
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 12));
    CHECK(breeding && breeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_12_SIZE);
    struct RhhBreedingMoveRuleContext outside = {.struct_size = sizeof(outside), .slot = 99}, beforeOutside = outside;
    CHECK(breeding->move_rule_context(&outside) && !memcmp(&outside, &beforeOutside, sizeof(outside)));
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingMovePreview baseline = {.struct_size = sizeof(baseline)}, preview = {.struct_size = sizeof(preview)};
    CHECK(!breeding->offer(&offer) && offer.preview.candidate_count == 1 && !breeding->move_preview(&offer, &baseline)
        && baseline.candidate_count == 1 && baseline.candidates[0].moves[0] && baseline.candidates[0].moves[1]
        && !!(baseline.flags & RHH_BREED_MOVE_PREVIEW_HOOKS_ACTIVE) == !!legacy);
    RhhContentKey shadow = RhhContentKeyForId(RHH_MOVE, MOVE_SHADOW_BALL);
    CHECK(!PreviewHasMove(&baseline.candidates[0], shadow));
    rng_value_t rng = gRngValue, rng2 = gRng2Value; struct DayCare daycare = gSaveBlock1Ptr->daycare;
    for (u32 i = 2; i <= 4; ++i)
    {
        *mode = i; CHECK(!breeding->move_preview(&offer, &preview) && !memcmp(&preview, &baseline, sizeof(preview)));
    }
    *mode = 6; CHECK(!breeding->move_preview(&offer, &preview) && !memcmp(&preview, &baseline, sizeof(preview)));
    for (u32 i = 8; i <= 9; ++i)
    {
        *mode = i; CHECK(!breeding->move_preview(&offer, &preview) && !memcmp(&preview, &baseline, sizeof(preview)));
    }
    *mode = 1; CHECK(!breeding->move_preview(&offer, &preview) && PreviewHasMove(&preview.candidates[0], shadow));
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
        && !memcmp(&daycare, &gSaveBlock1Ptr->daycare, sizeof(daycare)));
    struct RhhBreedingMovePreview tiny = {.struct_size = 1, .flags = 99}, savedTiny = tiny;
    CHECK(breeding->move_preview(&offer, &tiny) && !memcmp(&tiny, &savedTiny, sizeof(tiny)));
    GiveEggFromDaycare(); CHECK(gPartiesCount[B_TRAINER_PLAYER] == 1 && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG));
    bool32 actualShadow = FALSE;
    for (u32 i = 0; i < 4; ++i)
    {
        u32 move = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + i);
        RhhContentKey key = move ? RhhContentKeyForId(RHH_MOVE, move) : 0;
        if (!legacy) CHECK(key == preview.candidates[0].moves[i]);
        actualShadow |= key == shadow;
    }
    CHECK(actualShadow == !legacy);
    *mode = 5; FlagSet(FLAG_PENDING_DAYCARE_EGG);
    CHECK(!breeding->offer(&offer) && !breeding->move_preview(&offer, &preview)
        && preview.candidates[0].moves[0] == baseline.candidates[0].moves[1]);
    GiveEggFromDaycare(); CHECK(gPartiesCount[B_TRAINER_PLAYER] == 2);
    for (u32 i = 0; i < 4; ++i)
    {
        u32 move = GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_MOVE1 + i);
        RhhContentKey key = move ? RhhContentKeyForId(RHH_MOVE, move) : 0;
        CHECK(key == preview.candidates[0].moves[i]);
    }
    *mode = 0; FlagClear(FLAG_PENDING_DAYCARE_EGG);
    CreateMonWithIVs(&parent, SPECIES_NIDORAN_F, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CHECK(!breeding->offer(&offer) && offer.preview.candidate_count == 2 && !breeding->move_preview(&offer, &preview)
        && preview.candidate_count == 2 && preview.candidates[0].offspring == offer.preview.candidates[0].offspring
        && preview.candidates[1].offspring == offer.preview.candidates[1].offspring);
    RhhModsUnloadLibraries(); puts("breeding move rules: preview, validation and real eggs passed");
}
static void CheckBreedingMoveRuleCustom(const char *module, const char *learnsets)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module)
        && !RhhModsLoadLibrary(learnsets) && !RhhModsFinalize());
    gModDraft = (struct ModConfig){0}; ModsNewGame(7); ModsPrepareLearnsets();
    u32 custom = 0; RhhContentKey key = RhhContentKeyOf("test:move/surge");
    CHECK(!RhhModsMoveSlot(key, 0, &custom) && custom >= MOVES_COUNT_ALL);
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    struct Pokemon parent; CreateMonWithIVs(&parent, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonMoveSlot(&parent, custom, 0); gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 20, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box; FlagSet(FLAG_PENDING_DAYCARE_EGG);
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 12));
    u8 *mode = RhhModsState("test:breeding_move_rule"); CHECK(breeding && mode);
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingMovePreview baseline = {.struct_size = sizeof(baseline)}, preview = {.struct_size = sizeof(preview)};
    CHECK(!breeding->offer(&offer) && !breeding->move_preview(&offer, &baseline)
        && PreviewHasMove(&baseline.candidates[0], key) && baseline.candidates[0].moves[0] != key);
    *mode = 7; CHECK(!breeding->move_preview(&offer, &preview) && preview.candidates[0].moves[0] == key);
    GiveEggFromDaycare(); CHECK(gPartiesCount[B_TRAINER_PLAYER] == 1);
    for (u32 i = 0; i < 4; ++i)
    {
        u32 move = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + i);
        RhhContentKey actual = move >= MOVES_COUNT_ALL ? RhhModsMoveKey(move) : move ? RhhContentKeyForId(RHH_MOVE, move) : 0;
        CHECK(actual == preview.candidates[0].moves[i]);
    }
    RhhModsUnloadLibraries(); puts("breeding move rules: custom move preview and real egg passed");
}
