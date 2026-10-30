static bool32 PreviewHasMove(const struct RhhBreedingMovePreviewCandidate *candidate, RhhContentKey move)
{
    for (u32 i = 0; i < ARRAY_COUNT(candidate->moves); ++i) if (candidate->moves[i] == move) return TRUE;
    return FALSE;
}
static void CheckBreedingMoves(const char *module, const char *rules)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module));
    if (rules) CHECK(!RhhModsLoadLibrary(rules));
    CHECK(!RhhModsFinalize()); RhhModsSetRunContext(123, 456, 0);
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 8));
    CHECK(breeding && breeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_8_SIZE
        && RHH_BREEDING_SERVICE_V1_7_SIZE < RHH_BREEDING_SERVICE_V1_8_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 7)) == &breeding->header
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
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagClear(FLAG_PENDING_DAYCARE_EGG); gMain.inBattle = FALSE; gMain.callback2 = CB2_Overworld;
    memset(&gPaletteFade, 0, sizeof(gPaletteFade)); ScriptContext_Init(); UnlockPlayerFieldControls();
    struct Pokemon parent;
    if (!rules) {
        CreateMonWithIVs(&parent, SPECIES_PIKACHU, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
        gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
        CreateMonWithIVs(&parent, SPECIES_DITTO, 20, 54321, OTID_STRUCT_PRESET(12345), 10);
        gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    } else {
        CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
        gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
        CreateMonWithIVs(&parent, SPECIES_CHARMANDER, 12, 12345, OTID_STRUCT_PRESET(12321), 10);
        SetMonMoveSlot(&parent, MOVE_SHADOW_BALL, 1); gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    }
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingMovePreview preview = {.struct_size = sizeof(preview)}, unchanged;
    CHECK(!breeding->offer(&offer) && offer.compatibility != RHH_DAYCARE_INCOMPATIBLE);
    struct RhhModEvent update = {.struct_size = sizeof(update), .kind = RHH_MOD_UPDATE, .args = {73, !!rules}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1}; RhhModsDispatch(&update, &decision);
    struct DayCare daycare = gSaveBlock1Ptr->daycare; struct Pokemon party[PARTY_SIZE];
    memcpy(party, gParties[B_TRAINER_PLAYER], sizeof(party)); rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(!breeding->move_preview(&offer, &preview) && preview.candidate_count == offer.preview.candidate_count
        && !!(preview.flags & RHH_BREED_MOVE_PREVIEW_HOOKS_ACTIVE) == !!rules
        && !memcmp(&daycare, &gSaveBlock1Ptr->daycare, sizeof(daycare))
        && !memcmp(party, gParties[B_TRAINER_PLAYER], sizeof(party))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    RhhContentKey special = RhhContentKeyForId(RHH_MOVE, rules ? MOVE_SHADOW_BALL : MOVE_VOLT_TACKLE);
    if (!rules) {
        CHECK(!PreviewHasMove(&preview.candidates[0], special));
        u16 item = ITEM_LIGHT_BALL; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &item);
        unchanged = preview; CHECK(breeding->move_preview(&offer, &preview) && !memcmp(&preview, &unchanged, sizeof(preview)));
        CHECK(!breeding->offer(&offer) && !breeding->move_preview(&offer, &preview)
            && PreviewHasMove(&preview.candidates[0], special));
    } else CHECK(!PreviewHasMove(&preview.candidates[0], special));
    unchanged = preview; preview.struct_size--;
    struct RhhBreedingMovePreview shortBefore = preview;
    CHECK(breeding->move_preview(&offer, &preview) && !memcmp(&preview, &shortBefore, sizeof(preview)));
    preview = unchanged; FlagSet(FLAG_PENDING_DAYCARE_EGG);
    CHECK(breeding->move_preview(&offer, &preview) && !memcmp(&preview, &unchanged, sizeof(preview)));
    CHECK(!breeding->offer(&offer) && !breeding->move_preview(&offer, &preview) && preview.candidate_count == 1);
    struct RhhBreedingAcceptResult result = {.struct_size = sizeof(result)};
    CHECK(breeding->accept(&offer, &result) == RHH_BREED_ACCEPT_OK && result.egg_valid
        && result.egg.species == preview.candidates[0].offspring && result.party_slot == 0);
    bool32 actualSpecial = FALSE;
    for (u32 i = 0; i < 4; ++i) {
        u32 move = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + i);
        RhhContentKey key = move ? RhhContentKeyForId(RHH_MOVE, move) : 0;
        if (!rules) CHECK(key == preview.candidates[0].moves[i]);
        if (key == special) actualSpecial = TRUE;
    }
    CHECK(actualSpecial);
    if (!rules) {
        FlagClear(FLAG_PENDING_DAYCARE_EGG);
        CreateMonWithIVs(&parent, SPECIES_NIDORAN_F, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
        gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
        CreateMonWithIVs(&parent, SPECIES_DITTO, 20, 54321, OTID_STRUCT_PRESET(12345), 10);
        gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
        offer = (struct RhhBreedingOffer){.struct_size = sizeof(offer)};
        preview = (struct RhhBreedingMovePreview){.struct_size = sizeof(preview)};
        rng = gRngValue; rng2 = gRng2Value;
        CHECK(!breeding->offer(&offer) && offer.preview.candidate_count == 2 && !breeding->move_preview(&offer, &preview)
            && preview.candidate_count == 2 && preview.candidates[0].offspring == offer.preview.candidates[0].offspring
            && preview.candidates[1].offspring == offer.preview.candidates[1].offspring
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        struct RhhBreedingOffer tampered = offer; tampered.preview.candidates[1].offspring = tampered.preview.candidates[0].offspring;
        unchanged = preview; CHECK(breeding->move_preview(&tampered, &preview) && !memcmp(&preview, &unchanged, sizeof(preview)));
    }
    RhhModsUnloadLibraries(); puts(rules ? "breeding moves: mod uncertainty and committed override passed"
                                         : "breeding moves: baseline preview and committed egg passed");
}
static void CheckBreedingMovesCustom(const char *module, const char *learnsets)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module)
        && !RhhModsLoadLibrary(learnsets) && !RhhModsFinalize());
    gModDraft = (struct ModConfig){0}; ModsNewGame(7); ModsPrepareLearnsets();
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 8));
    CHECK(breeding); u32 custom = 0; RhhContentKey surge = RhhContentKeyOf("test:move/surge");
    CHECK(!RhhModsMoveSlot(surge, 0, &custom) && custom >= MOVES_COUNT_ALL);
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    struct Pokemon parent; CreateMonWithIVs(&parent, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonMoveSlot(&parent, custom, 0); gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 20, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    gMain.inBattle = FALSE; gMain.callback2 = CB2_Overworld; memset(&gPaletteFade, 0, sizeof(gPaletteFade));
    ScriptContext_Init(); UnlockPlayerFieldControls(); FlagSet(FLAG_PENDING_DAYCARE_EGG);
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingMovePreview preview = {.struct_size = sizeof(preview)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(!breeding->offer(&offer) && !breeding->move_preview(&offer, &preview) && !preview.flags
        && preview.candidate_count == 1 && PreviewHasMove(&preview.candidates[0], surge)
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    struct RhhBreedingAcceptResult result = {.struct_size = sizeof(result)};
    CHECK(breeding->accept(&offer, &result) == RHH_BREED_ACCEPT_OK && result.party_slot == 0);
    bool32 found = FALSE;
    for (u32 i = 0; i < 4; ++i) {
        u32 move = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + i);
        RhhContentKey key = move >= MOVES_COUNT_ALL ? RhhModsMoveKey(move) : move ? RhhContentKeyForId(RHH_MOVE, move) : 0;
        CHECK(key == preview.candidates[0].moves[i]);
        if (key == surge) found = TRUE;
    }
    CHECK(found); RhhModsUnloadLibraries(); puts("breeding moves: custom learnset preview and egg passed");
}
