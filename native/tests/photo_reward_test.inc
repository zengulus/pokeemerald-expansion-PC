static void PhotoRewardUpdate(void)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE, .run_seed = RhhModsRunSeed()};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision);
}
static void PhotoRewardFillBag(void)
{
    for (u32 i = 0; i < BAG_ITEMS_COUNT; ++i) CHECK(AddBagItem(ITEM_POTION, MAX_BAG_ITEM_CAPACITY));
    CHECK(!CheckBagHasSpace(ITEM_POTION, 1));
}
static void CheckPhotoReward(const char *path, const char *albumModule, const char *rewardModule, u32 phase)
{
    InitModsForTestWithLibraries(albumModule, rewardModule);
    CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhPhotoRewardService *reward = (const void *)host->get_service("rhh:service/photo_reward", RHH_SERVICE_VERSION(1, 0));
    const struct RhhPhotoAlbumService *album = (const void *)host->get_service("rhh:service/photo_album", RHH_SERVICE_VERSION(1, 0));
    const struct RhhPhotoService *photo = (const void *)host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 0));
    const struct RhhPhotoScoreService *scores = (const void *)host->get_service("rhh:service/photo_score", RHH_SERVICE_VERSION(1, 1));
    const struct RhhWildActorService *actors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    CHECK(reward && album && photo && actors && scores
        && host->get_service("rhh:service/photo_reward", RHH_SERVICE_VERSION(1, 1)) == &reward->header
        && host->get_service("rhh:service/photo_reward", RHH_SERVICE_VERSION(1, 2)) == &reward->header
        && host->get_service("rhh:service/photo_reward", RHH_SERVICE_VERSION(1, 3)) == &reward->header
        && !host->get_service("rhh:service/photo_reward", RHH_SERVICE_VERSION(1, 4)));
    struct RhhPhotoRewardResult result = {.struct_size = sizeof(result)};
    struct RhhPhotoRewardResult outside = {.struct_size = sizeof(outside), .status = 999}, unchanged = outside;
    CHECK(reward->context(&outside) && !memcmp(&outside, &unchanged, sizeof(outside)));
    u32 available = 99; CHECK(reward->context_score_available(&available) && available == 99);
    if (phase == 11 || phase == 12)
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
        AlbumTestMap(); SetBagItemsPointers();
        u8 *state = RhhModsState("test:simple_photo_reward"); CHECK(state);
        state[0] = 1; PhotoRewardUpdate();
        CHECK(state[1] == (phase == 11 ? RHH_PHOTO_REWARD_ALREADY : RHH_PHOTO_REWARD_OK)
            && CountTotalItemQuantityInBag(ITEM_POTION) == 2);
        puts(phase == 11 ? "simple photo reward: saved claim and item restored together"
                         : "simple photo reward: older save restores the unclaimed state");
        free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
        NativeSaveClose(); RhhModsUnloadLibraries(); return;
    }
    if (!phase || phase == 4 || phase == 5 || phase == 6 || phase == 9 || phase == 10)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        memset(gObjectEvents, 0, sizeof(gObjectEvents)); AlbumTestMap();
        struct ObjectEvent *player = &gObjectEvents[0], *wild = &gObjectEvents[1];
        player->active = player->isPlayer = TRUE; player->mapGroup = 1; player->mapNum = 0;
        player->currentCoords.x = player->previousCoords.x = MAP_OFFSET; player->currentCoords.y = player->previousCoords.y = MAP_OFFSET;
        wild->active = TRUE; wild->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; wild->localId = LOCALID_OW_ENCOUNTER_END;
        wild->mapGroup = 1; wild->mapNum = 0; wild->trainerRange_berryTreeId = 5;
        wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 2; wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET;
        struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 19);
        wild->graphicsId = GetGraphicsIdForMon(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_IS_SHINY),
            GetGenderFromSpeciesAndPersonality(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_PERSONALITY)) == MON_FEMALE);
        ModsWildActorSpawned(wild, &mon);
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)}; CHECK(!actors->at(1, &actor));
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        if (!phase)
        {
            rng_value_t rng = gRngValue, rng2 = gRng2Value; sPhotoScoreImpure = TRUE;
            AlbumTestCapture(photo, actor.handle, RGB(0, 0, 31), 10); sPhotoScoreImpure = FALSE;
            struct RhhPhotoRecent tested = {.struct_size = sizeof(tested)};
            struct RhhPhotoAlbumEntry remove = {.struct_size = sizeof(remove)};
            CHECK(!scores->recent(&tested) && (tested.score.flags & RHH_PHOTO_SCORE_AVAILABLE)
                && tested.score.value == tested.score.base && !album->at(0, &remove) && !album->remove(&remove.id)
                && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        }
        AlbumTestCapture(photo, actor.handle, RGB(0, 0, 31), phase == 4 ? 4 : phase == 5 ? 2 : phase == 6 ? 5 : phase == 9 ? 6 : 1);
        struct RhhPhotoAlbumEntry entry = {.struct_size = sizeof(entry)};
        struct RhhPhotoRecent initialRecent = {.struct_size = sizeof(initialRecent)};
        CHECK(!scores->recent(&initialRecent));
        if (phase == 4 || phase == 6)
        { CHECK(!album->count()); entry.id = initialRecent.photo; entry.individual = initialRecent.individual; }
        else CHECK(album->count() == 1 && !album->at(0, &entry));
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        if (phase == 10)
        {
            u8 *state = RhhModsState("test:simple_photo_reward"); CHECK(state);
            SetBagItemsPointers(); ClearBag(); PhotoRewardFillBag();
            state[0] = 1; PhotoRewardUpdate(); CHECK(state[1] == RHH_PHOTO_REWARD_BAG_FULL);
            ClearBag(); PhotoRewardUpdate();
            CHECK(state[1] == RHH_PHOTO_REWARD_OK && CountTotalItemQuantityInBag(ITEM_POTION) == 2);
            PhotoRewardUpdate(); CHECK(state[1] == RHH_PHOTO_REWARD_ALREADY
                && CountTotalItemQuantityInBag(ITEM_POTION) == 2);
            CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
            puts("simple photo reward: full bag is safe and first claim succeeds once");
            free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
            NativeSaveClose(); RhhModsUnloadLibraries(); return;
        }
        struct RhhPhotoRewardRequest request = {.struct_size = sizeof(request), .quantity = 2, .minimum_score = 700,
            .owner = RhhContentKeyOf("test:photo_reward_probe"), .rule = RhhContentKeyOf("test:reward/first_clear"),
            .item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION), .photo = entry.id};
        if (phase == 9)
        {
            CHECK((initialRecent.score.flags & RHH_PHOTO_SCORE_AVAILABLE) && !initialRecent.score.value);
            request.minimum_score = 0;
            CHECK(!reward->claim(&request, &result) && result.receipt.score == 0
                && result.receipt.delivered && !reward->score_available(0, &available) && available);
            puts("photo reward: scored zero remains distinguishable");
            free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
            NativeSaveClose(); RhhModsUnloadLibraries(); return;
        }
        if (phase == 5 || phase == 6)
        {
            CHECK(!initialRecent.score.flags);
            if (phase == 5)
            {
                struct RhhPhotoScore missing = {.struct_size = sizeof(missing)};
                CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && scores->recent(&initialRecent)
                    && !album->find(&entry.id, &entry) && scores->find(&entry.id, &missing));
                AlbumTestMap();
            }
            request.rule = RhhContentKeyOf("test:reward/unscored"); request.minimum_score = 0;
            CHECK(reward->claim(&request, &result) == RHH_PHOTO_REWARD_NOT_ELIGIBLE && !reward->count());
            CHECK(!reward->preview_allow_unscored(&request, &result) && result.is_preview
                && !reward->count() && !CheckBagHasItem(ITEM_POTION, 1));
            request.minimum_score = 1;
            CHECK(reward->claim_allow_unscored(&request, &result) == RHH_PHOTO_REWARD_NOT_ELIGIBLE);
            request.minimum_score = 0;
            RhhModsSetPersistence(RejectPhotoJournal);
            CHECK(reward->claim_allow_unscored(&request, &result) == RHH_PHOTO_REWARD_PERSISTENCE_FAILED
                && !reward->count() && !CheckBagHasItem(ITEM_POTION, 1));
            RhhModsSetPersistence(SavePhotoJournal);
            CHECK(!reward->claim_allow_unscored(&request, &result) && result.receipt.score == 0
                && result.receipt.delivered && CheckBagHasItem(ITEM_POTION, 2));
            CHECK(!reward->score_available(0, &available) && !available
                && reward->claim_allow_unscored(&request, &result) == RHH_PHOTO_REWARD_ALREADY);
            available = 99; CHECK(reward->score_available(1, &available) && available == 99);
            puts("photo reward: unscored capture earned a durable reward");
            free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
            NativeSaveClose(); RhhModsUnloadLibraries(); return;
        }
        if (phase == 4)
        {
            struct RhhPhotoRecent recent = {.struct_size = sizeof(recent)};
            CHECK(!scores->recent(&recent) && recent.photo.serial == entry.id.serial
                && recent.score.value == 777 && recent.individual.serial == entry.individual.serial);
            CHECK(!album->count() && album->find(&entry.id, &entry));
            CHECK(!reward->preview(&request, &result) && !reward->claim(&request, &result)
                && result.receipt.delivered && CheckBagHasItem(ITEM_POTION, 2));
            AlbumTestCapture(photo, actor.handle, RGB(31, 0, 0), 2);
            CHECK(!scores->recent(&recent) && recent.photo.serial != entry.id.serial && !recent.score.flags);
            request.rule = RhhContentKeyOf("test:reward/another_rule");
            CHECK(reward->claim(&request, &result) == RHH_PHOTO_REWARD_NOT_ELIGIBLE);
            request.photo = recent.photo;
            CHECK(reward->claim(&request, &result) == RHH_PHOTO_REWARD_NOT_ELIGIBLE);
            CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && scores->recent(&recent));
            puts("photo reward: unarchived capture earned a durable reward");
            free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
            NativeSaveClose(); RhhModsUnloadLibraries(); return;
        }
        CHECK(!reward->count() && !CheckBagHasItem(ITEM_POTION, 1));
        rng_value_t rewardRng = gRngValue, rewardRng2 = gRng2Value; sPhotoRewardImpure = TRUE;
        CHECK(reward->preview(&request, &result) == RHH_PHOTO_REWARD_UNAVAILABLE
            && !reward->count() && !CheckBagHasItem(ITEM_POTION, 1)
            && !memcmp(&rewardRng, &gRngValue, sizeof(rewardRng)) && !memcmp(&rewardRng2, &gRng2Value, sizeof(rewardRng2)));
        request.quantity = 3;
        CHECK(reward->claim(&request, &result) == RHH_PHOTO_REWARD_DENIED
            && !reward->count() && !CheckBagHasItem(ITEM_POTION, 1)
            && !memcmp(&rewardRng, &gRngValue, sizeof(rewardRng)) && !memcmp(&rewardRng2, &gRng2Value, sizeof(rewardRng2)));
        request.quantity = 2; sPhotoRewardImpure = FALSE;
        CHECK(reward->preview(&request, &result) == RHH_PHOTO_REWARD_OK && result.is_preview && result.changed
            && !reward->count() && !CheckBagHasItem(ITEM_POTION, 1));
        request.minimum_score = 778;
        CHECK(reward->claim(&request, &result) == RHH_PHOTO_REWARD_NOT_ELIGIBLE && !reward->count());
        request.minimum_score = 700; request.quantity = 3;
        CHECK(reward->claim(&request, &result) == RHH_PHOTO_REWARD_DENIED && !strcmp(result.reason, "test reward veto")
            && !reward->count() && !CheckBagHasItem(ITEM_POTION, 1));
        request.quantity = 2;
        struct Bag beforeBag = gSaveBlock1Ptr->bag; PhotoRewardFillBag();
        CHECK(reward->claim(&request, &result) == RHH_PHOTO_REWARD_BAG_FULL && !reward->count());
        gSaveBlock1Ptr->bag = beforeBag;
        RhhModsSetPersistence(RejectPhotoJournal);
        CHECK(reward->claim(&request, &result) == RHH_PHOTO_REWARD_PERSISTENCE_FAILED
            && !reward->count() && !memcmp(&beforeBag, &gSaveBlock1Ptr->bag, sizeof(beforeBag)));
        RhhModsSetPersistence(SavePhotoJournal);
        CHECK(reward->claim(&request, &result) == RHH_PHOTO_REWARD_OK && result.changed
            && result.receipt.delivered && result.receipt.sequence == 1 && reward->count() == 1
            && CheckBagHasItem(ITEM_POTION, 2) && !CheckBagHasItem(ITEM_POTION, 3));
        CHECK(reward->claim(&request, &result) == RHH_PHOTO_REWARD_ALREADY && !result.changed
            && CheckBagHasItem(ITEM_POTION, 2));
        AlbumTestCapture(photo, actor.handle, RGB(0, 31, 0), 3);
        struct RhhPhotoAlbumEntry second = {.struct_size = sizeof(second)};
        CHECK(!album->at(1, &second)); request.photo = second.id; request.minimum_score = 0;
        CHECK(reward->claim(&request, &result) == RHH_PHOTO_REWARD_ALREADY && reward->count() == 1);
        puts("photo reward: validated, persisted and delivered once");
    }
    else
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK); AlbumTestMap();
        struct RhhPhotoRecent recent = {.struct_size = sizeof(recent)};
        CHECK(scores->recent(&recent));
        struct RhhPhotoRewardReceipt receipt = {.struct_size = sizeof(receipt)};
        CHECK(reward->count() == 1 && !reward->at(0, &receipt) && receipt.quantity == 2
            && receipt.score == (phase == 7 || phase == 8 ? 0 : 777) && receipt.sequence == 1
            && !reward->score_available(0, &available) && available == (phase != 7 && phase != 8));
        if (phase == 7)
        {
            CHECK(!receipt.delivered && !CheckBagHasItem(ITEM_POTION, 1));
            PhotoRewardUpdate(); CHECK(!reward->at(0, &receipt) && receipt.delivered
                && CheckBagHasItem(ITEM_POTION, 2));
            CHECK(RemoveBagItem(ITEM_POTION, 1) && TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
            puts("photo reward: unscored reward replayed and consumption saved");
        }
        else if (phase == 8)
        {
            CHECK(receipt.delivered && CheckBagHasItem(ITEM_POTION, 1) && !CheckBagHasItem(ITEM_POTION, 2));
            PhotoRewardUpdate(); CHECK(CheckBagHasItem(ITEM_POTION, 1) && !CheckBagHasItem(ITEM_POTION, 2));
            puts("photo reward: unscored consumption did not trigger another grant");
        }
        else if (phase == 1 || phase == 3)
        {
            CHECK(!receipt.delivered && !CheckBagHasItem(ITEM_POTION, 1));
            if (phase == 1)
            {
                struct Bag beforeBag = gSaveBlock1Ptr->bag; PhotoRewardFillBag();
                PhotoRewardUpdate(); CHECK(!reward->at(0, &receipt) && !receipt.delivered);
                gSaveBlock1Ptr->bag = beforeBag;
            }
            PhotoRewardUpdate(); CHECK(!reward->at(0, &receipt) && receipt.delivered
                && CheckBagHasItem(ITEM_POTION, 2) && !CheckBagHasItem(ITEM_POTION, 3));
            PhotoRewardUpdate(); CHECK(CheckBagHasItem(ITEM_POTION, 2) && !CheckBagHasItem(ITEM_POTION, 3));
            if (phase == 1)
            {
                CHECK(RemoveBagItem(ITEM_POTION, 1) && TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
                puts("photo reward: pending delivery replayed and consumption saved");
            }
            else puts("photo reward: older save received one replayed reward");
        }
        else
        {
            CHECK(receipt.delivered && CheckBagHasItem(ITEM_POTION, 1) && !CheckBagHasItem(ITEM_POTION, 2));
            PhotoRewardUpdate(); CHECK(CheckBagHasItem(ITEM_POTION, 1) && !CheckBagHasItem(ITEM_POTION, 2));
            puts("photo reward: saved consumption did not trigger another grant");
        }
    }
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    NativeSaveClose(); RhhModsUnloadLibraries();
}
