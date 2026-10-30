static void CheckBreedingRequest(const char *probe, const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(probe) && !RhhModsFinalize());
    CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhBreedingService *breeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 13));
    CHECK(breeding && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 12)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 14)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 15)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 16)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 17)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 18)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 19)) == &breeding->header
        && host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 23)) == &breeding->header
        && !host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 29)));
    struct RhhBreedingRequestResult result = {.struct_size = sizeof(result)}, small = {.struct_size = sizeof(small) - 1, .status = 99};
    struct RhhBreedingRequestResult beforeSmall = small;
    CHECK(breeding->request_pending(NULL, &small) == RHH_BREED_REQUEST_INVALID && !memcmp(&small, &beforeSmall, sizeof(small))
        && breeding->request_pending(NULL, &result) == RHH_BREED_REQUEST_INVALID && breeding->request_context(&result));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    gMain.inBattle = FALSE; gMain.callback2 = CB2_Overworld; memset(&gPaletteFade, 0, sizeof(gPaletteFade));
    ScriptContext_Init(); UnlockPlayerFieldControls(); FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)}, stale;
    CHECK(!breeding->offer(&offer) && breeding->request_pending(&offer, &result) == RHH_BREED_REQUEST_INCOMPATIBLE);
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12321), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    struct BoxPokemon compatibleParent = gSaveBlock1Ptr->daycare.mons[0].mon;
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CHECK(!breeding->offer(&offer) && offer.parent_count == 2 && offer.compatibility == RHH_DAYCARE_INCOMPATIBLE
        && breeding->request_pending(&offer, &result) == RHH_BREED_REQUEST_INCOMPATIBLE);
    gSaveBlock1Ptr->daycare.mons[0].mon = compatibleParent;
    CHECK(!breeding->offer(&offer) && !offer.pending && offer.compatibility != RHH_DAYCARE_INCOMPATIBLE);
    struct RhhBreedingOffspringOdds ordinaryOdds = {.struct_size = sizeof(ordinaryOdds)};
    CHECK(!breeding->offspring_odds(&offer, &ordinaryOdds) && ordinaryOdds.denominator == 2
        && ordinaryOdds.count == 1 && ordinaryOdds.outcomes[0].weight == 2 && !ordinaryOdds.flags);
    stale = offer; u16 item = ITEM_EVERSTONE;
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &item);
    CHECK(breeding->request_pending(&stale, &result) == RHH_BREED_REQUEST_STALE && !FlagGet(FLAG_PENDING_DAYCARE_EGG));
    CHECK(!breeding->offer(&offer)); ScriptContext_Enable();
    CHECK(breeding->request_pending(&offer, &result) == RHH_BREED_REQUEST_UNAVAILABLE);
    ScriptContext_Init(); UnlockPlayerFieldControls();
    CHECK(!NativeSaveBegin() && breeding->request_pending(&offer, &result) == RHH_BREED_REQUEST_UNAVAILABLE
        && NativeSaveEnd(1, FALSE));
    u32 iv = 7; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HP_IV, &iv);
    CHECK(!breeding->offer(&offer));
    rng_value_t rng = gRngValue, rng2 = gRng2Value; struct DayCare before = gSaveBlock1Ptr->daycare;
    CHECK(breeding->request_pending(&offer, &result) == RHH_BREED_REQUEST_DENIED
        && !strcmp(result.reason, "test rule denied request") && !FlagGet(FLAG_PENDING_DAYCARE_EGG)
        && !memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    iv = 31; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HP_IV, &iv);
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = parent;
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE;
    CHECK(!breeding->offer(&offer)); before = gSaveBlock1Ptr->daycare; rng = gRngValue; rng2 = gRng2Value;
    CHECK(offer.party_count == PARTY_SIZE && breeding->request_pending(&offer, &result) == RHH_BREED_REQUEST_OK
        && FlagGet(FLAG_PENDING_DAYCARE_EGG)
        && !memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before)) && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)) && breeding->request_context(&result));
    struct RhhDaycareEggId eggId = {.struct_size = sizeof(eggId)}, reloadedId = {.struct_size = sizeof(reloadedId)};
    CHECK(!breeding->pending_id(&eggId) && eggId.run_id == RhhModsRunId() && (eggId.high || eggId.low));
    CHECK(breeding->request_pending(&offer, &result) == RHH_BREED_REQUEST_STALE);
    CHECK(!breeding->offer(&offer) && breeding->request_pending(&offer, &result) == RHH_BREED_REQUEST_ALREADY_PENDING);
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && FlagGet(FLAG_PENDING_DAYCARE_EGG));
    CHECK(!breeding->pending_id(&reloadedId) && !memcmp(&eggId, &reloadedId, sizeof(eggId)));
    CHECK(!breeding->offer(&offer));
    struct RhhBreedingAcceptResult accepted = {.struct_size = sizeof(accepted)};
    CHECK(breeding->accept(&offer, &accepted) == RHH_BREED_ACCEPT_PARTY_FULL && FlagGet(FLAG_PENDING_DAYCARE_EGG));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    CHECK(!breeding->offer(&offer));
    CHECK(breeding->accept(&offer, &accepted) == RHH_BREED_ACCEPT_OK && accepted.egg_valid
        && accepted.egg.is_egg && !FlagGet(FLAG_PENDING_DAYCARE_EGG));
    CHECK(breeding->pending_id(&reloadedId));
    u8 *events = RhhModsState("test:breeding_request");
    CHECK(events && events[RHH_DAYCARE_PENDING_REQUEST] == 1 && events[RHH_DAYCARE_PENDING_ACCEPTED] == 1);
    /* Female/male counterpart selection is rolled once at the pending
     * transition, and survives repeated triggers and a save/load. */
    static const u16 parents[] = {SPECIES_NIDORAN_F, SPECIES_ILLUMISE};
    static const u16 variants[][2] = {{SPECIES_NIDORAN_F, SPECIES_NIDORAN_M}, {SPECIES_ILLUMISE, SPECIES_VOLBEAT}};
    for (u32 pair = 0; pair < ARRAY_COUNT(parents); ++pair)
        for (u32 variant = 0; variant < 2; ++variant)
        {
            memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
            gPartiesCount[B_TRAINER_PLAYER] = 0; FlagClear(FLAG_PENDING_DAYCARE_EGG);
            CreateMonWithIVs(&parent, parents[pair], 12, 12345, OTID_STRUCT_PRESET(54321), 31);
            gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
            CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12321), 31);
            gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
            CHECK(!breeding->offer(&offer) && offer.preview.candidate_count == 2);
            struct RhhBreedingOffspringOdds odds = {.struct_size = sizeof(odds)};
            struct RhhBreedingOffspringOdds tiny = {.struct_size = 1, .count = 999}, unchanged = tiny;
            rng_value_t checkRng = gRngValue, checkRng2 = gRng2Value;
            CHECK(!breeding->offspring_odds(&offer, &odds) && !odds.flags && odds.denominator == 2 && odds.count == 2
                && odds.outcomes[0].offspring == RhhContentKeyForId(RHH_SPECIES, variants[pair][0])
                && odds.outcomes[0].weight == 1 && odds.outcomes[1].offspring == RhhContentKeyForId(RHH_SPECIES, variants[pair][1])
                && odds.outcomes[1].weight == 1 && breeding->offspring_odds(&offer, &tiny)
                && !memcmp(&tiny, &unchanged, sizeof(tiny)) && !memcmp(&checkRng, &gRngValue, sizeof(checkRng))
                && !memcmp(&checkRng2, &gRng2Value, sizeof(checkRng2)));
            rng_value_t start, after; u32 seed;
            for (seed = 1; seed < 1000; ++seed)
            {
                start = LocalRandomSeed(seed); after = start;
                if (!!(LocalRandom(&after) & EGG_GENDER_MALE) == variant) break;
            }
            CHECK(seed < 1000); gRngValue = start; rng2 = gRng2Value;
            if ((pair + variant) & 1)
                CHECK(breeding->request_pending(&offer, &result) == RHH_BREED_REQUEST_OK);
            else TriggerPendingDaycareEgg();
            CHECK(FlagGet(FLAG_PENDING_DAYCARE_EGG) && !memcmp(&after, &gRngValue, sizeof(after))
                && !memcmp(&rng2, &gRng2Value, sizeof(rng2))
                && !!(gSaveBlock1Ptr->daycare.offspringPersonality & EGG_GENDER_MALE) == variant);
            unchanged = odds;
            CHECK(breeding->offspring_odds(&offer, &odds) && !memcmp(&odds, &unchanged, sizeof(odds)));
            TriggerPendingDaycareEgg();
            CHECK(!memcmp(&after, &gRngValue, sizeof(after)) && !breeding->offer(&offer)
                && offer.preview.candidate_count == 1
                && offer.preview.candidates[0].offspring == RhhContentKeyForId(RHH_SPECIES, variants[pair][variant]));
            CHECK(!breeding->offspring_odds(&offer, &odds) && odds.count == 1 && odds.denominator == 2
                && odds.outcomes[0].offspring == offer.preview.candidates[0].offspring && odds.outcomes[0].weight == 2);
            if (!pair && !variant)
            {
                CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
                FlagClear(FLAG_PENDING_DAYCARE_EGG); gSaveBlock1Ptr->daycare.offspringPersonality ^= EGG_GENDER_MALE;
                CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && FlagGet(FLAG_PENDING_DAYCARE_EGG)
                    && !breeding->offer(&offer) && offer.preview.candidate_count == 1
                    && offer.preview.candidates[0].offspring == RhhContentKeyForId(RHH_SPECIES, variants[pair][variant]));
            }
            CHECK(breeding->accept(&offer, &accepted) == RHH_BREED_ACCEPT_OK && accepted.egg_valid
                && accepted.egg.species == RhhContentKeyForId(RHH_SPECIES, variants[pair][variant]));
        }
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("breeding request: checked pending egg, save and acceptance passed");
}

static void CheckBreedingPendingLifecycle(const char *probe)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(probe) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 14));
    u8 *events = RhhModsState("test:breeding_request"); CHECK(breeding && events);
    struct RhhDaycarePendingChange outside = {.struct_size = sizeof(outside), .cause = 99}, old = outside;
    CHECK(breeding->pending_context(&outside) && !memcmp(&outside, &old, sizeof(outside)));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12321), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    gMain.inBattle = FALSE; gMain.callback2 = CB2_Overworld; memset(&gPaletteFade, 0, sizeof(gPaletteFade));
    ScriptContext_Init(); UnlockPlayerFieldControls(); FlagClear(FLAG_PENDING_DAYCARE_EGG);
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    TriggerPendingDaycareEgg(); TriggerPendingDaycareEgg();
    CHECK(FlagGet(FLAG_PENDING_DAYCARE_EGG) && events[RHH_DAYCARE_PENDING_MANUAL] == 1);
    RejectEggFromDayCare(); RejectEggFromDayCare();
    CHECK(!FlagGet(FLAG_PENDING_DAYCARE_EGG) && events[RHH_DAYCARE_PENDING_REJECTED] == 1
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    for (u32 i = 0; i < 1024 && !FlagGet(FLAG_PENDING_DAYCARE_EGG); ++i)
    {
        gSaveBlock1Ptr->daycare.mons[1].steps = 254;
        IncrementDaycareSteps();
    }
    CHECK(FlagGet(FLAG_PENDING_DAYCARE_EGG) && events[RHH_DAYCARE_PENDING_WALK] == 1);
    IncrementDaycareSteps(); CHECK(events[RHH_DAYCARE_PENDING_WALK] == 1);
    RejectEggFromDayCare(); CHECK(events[RHH_DAYCARE_PENDING_REJECTED] == 2);
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingRequestResult result = {.struct_size = sizeof(result)};
    CHECK(!breeding->offer(&offer) && breeding->request_pending(&offer, &result) == RHH_BREED_REQUEST_OK
        && events[RHH_DAYCARE_PENDING_REQUEST] == 1);
    CHECK(!breeding->offer(&offer));
    struct RhhBreedingAcceptResult accepted = {.struct_size = sizeof(accepted)};
    CHECK(breeding->accept(&offer, &accepted) == RHH_BREED_ACCEPT_OK && events[RHH_DAYCARE_PENDING_ACCEPTED] == 1
        && !FlagGet(FLAG_PENDING_DAYCARE_EGG));
    events[6] = 1;
    gSaveBlock1Ptr->daycare.mons[1].mon.secure.raw[0] ^= 1;
    TriggerPendingDaycareEgg();
    CHECK(FlagGet(FLAG_PENDING_DAYCARE_EGG) && events[RHH_DAYCARE_PENDING_MANUAL] == 2);
    RejectEggFromDayCare(); CHECK(events[RHH_DAYCARE_PENDING_REJECTED] == 3);
    gSaveBlock1Ptr->daycare.mons[1].mon.secure.raw[0] ^= 1;
    events[6] = 0; TriggerPendingDaycareEgg(); FlagClear(FLAG_PENDING_DAYCARE_EGG);
    events[6] = 2; GiveEggFromDaycare();
    CHECK(events[0] == 2 && events[RHH_DAYCARE_PENDING_ACCEPTED] == 1);
    ModsBreedingReconcilePendingId();
    CHECK(breeding->pending_context(&outside) && !memcmp(&outside, &old, sizeof(outside)));
    RhhModsUnloadLibraries(); puts("breeding pending: walking, requests, rejection and acceptance passed");
}
