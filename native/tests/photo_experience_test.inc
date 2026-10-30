static void PhotoExperienceRunMenu(const struct RhhPhotoRewardService *reward, bool32 delivered)
{
    struct RhhPhotoExperienceReceipt receipt = {.struct_size = sizeof(receipt)};
    unsigned frames = 0;
    while (frames++ < 12000)
    {
        PartyTestFrame(frames % 12 == 0 ? A_BUTTON : 0); m4aSoundMain(); m4aSoundVSync();
        CHECK(!reward->experience_at(0, &receipt));
        if (receipt.delivered == delivered && gMain.callback2 == CB2_Overworld && !gPaletteFade.active) break;
    }
    CHECK(frames < 12000 && receipt.delivered == delivered);
}
static void CheckPhotoExperience(const char *path, const char *albumModule, const char *rewardModule, u32 phase)
{
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); cgb_audio_init(42060); AgbMain();
    ClearSav1(); ClearSav2(); ClearSav3(); SetSaveBlocksPointers(0);
    InitModsForTestWithLibraries(albumModule, rewardModule);
    CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhPhotoRewardService *reward = (const void *)host->get_service("rhh:service/photo_reward", RHH_SERVICE_VERSION(1, 2));
    const struct RhhPhotoAlbumService *album = (const void *)host->get_service("rhh:service/photo_album", RHH_SERVICE_VERSION(1, 0));
    const struct RhhPhotoService *photo = (const void *)host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWildActorService *actors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    const struct RhhOwnedMonService *owned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 29));
    CHECK(reward && album && photo && actors && owned);
    struct RhhPhotoExperienceRetargetResult outside = {.struct_size = sizeof(outside), .status = 999}, oldOutside = outside;
    CHECK(reward->experience_retarget_context(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    if (!phase || phase == 4 || phase == 5 || phase == 8 || phase == 9 || phase == 10 || phase == 14 || phase == 15)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        memset(gObjectEvents, 0, sizeof(gObjectEvents)); AlbumTestMap();
        struct ObjectEvent *player = &gObjectEvents[0], *wild = &gObjectEvents[1];
        player->active = player->isPlayer = TRUE; player->mapGroup = 1; player->mapNum = 0;
        player->currentCoords.x = player->previousCoords.x = MAP_OFFSET; player->currentCoords.y = player->previousCoords.y = MAP_OFFSET;
        wild->active = TRUE; wild->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; wild->localId = LOCALID_OW_ENCOUNTER_END;
        wild->mapGroup = 1; wild->mapNum = 0; wild->trainerRange_berryTreeId = 5;
        wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 2; wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET;
        struct Pokemon wildMon; CreateMonWithIVs(&wildMon, SPECIES_POOCHYENA, 5, 54321, OTID_STRUCT_PRESET(12345), 19);
        wild->graphicsId = GetGraphicsIdForMon(SPECIES_POOCHYENA, GetMonData(&wildMon, MON_DATA_IS_SHINY),
            GetGenderFromSpeciesAndPersonality(SPECIES_POOCHYENA, GetMonData(&wildMon, MON_DATA_PERSONALITY)) == MON_FEMALE);
        ModsWildActorSpawned(wild, &wildMon);
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
        gPartiesCount[B_TRAINER_PLAYER] = 1;
        for (u32 i = 0; i < MAX_MON_MOVES; ++i) SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_NONE, i);
        if (phase == 10 || phase == 14 || phase == 15)
        {
            CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_TORCHIC, 6, 33333, OTID_STRUCT_PRESET(54321), 31);
            for (u32 i = 0; i < MAX_MON_MOVES; ++i) SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][1], MOVE_NONE, i);
            gPartiesCount[B_TRAINER_PLAYER] = 2;
        }
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)}; CHECK(!actors->at(1, &actor));
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        AlbumTestCapture(photo, actor.handle, RGB(0, 0, 31), phase == 5 ? 2 : 1);
        struct RhhPhotoAlbumEntry entry = {.struct_size = sizeof(entry)}; CHECK(!album->at(0, &entry));
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
        struct RhhPhotoExperienceRequest request = {.struct_size = sizeof(request), .amount = phase == 9 ? 799 : 800, .minimum_score = 700,
            .owner = RhhContentKeyOf("test:photo_experience"), .rule = RhhContentKeyOf("test:reward/photo_xp"),
            .photo = entry.id, .recipient.struct_size = sizeof(request.recipient)};
        CHECK(!owned->party_at(0, &mon) && !owned->experience(&mon, &request.recipient));
        struct Pokemon before = gParties[B_TRAINER_PLAYER][0];
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        struct RhhPhotoExperienceResult result = {.struct_size = sizeof(result)};
        struct RhhPhotoExperienceRequest invalid = request;
        invalid.recipient.struct_size = 1;
        CHECK(reward->claim_experience(&invalid, &result) == RHH_PHOTO_REWARD_INVALID);
        invalid = request; invalid.recipient.mon.struct_size = 1;
        CHECK(reward->claim_experience(&invalid, &result) == RHH_PHOTO_REWARD_INVALID);
        invalid = request; invalid.recipient.reserved[0] = 1;
        CHECK(reward->claim_experience(&invalid, &result) == RHH_PHOTO_REWARD_INVALID);
        if (phase == 4)
        {
            struct RhhPhotoRewardRequest item = {.struct_size = sizeof(item), .quantity = 1, .minimum_score = 700,
                .owner = request.owner, .rule = request.rule, .item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION), .photo = entry.id};
            struct RhhPhotoRewardResult itemResult = {.struct_size = sizeof(itemResult)};
            CHECK(!reward->claim(&item, &itemResult) && reward->claim_experience(&request, &result) == RHH_PHOTO_REWARD_ALREADY
                && !reward->experience_count() && !memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before)));
            puts("photo experience: item claim prevents duplicate experience");
            free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
            NativeSaveClose(); RhhModsUnloadLibraries(); SDL_Quit(); return;
        }
        if (phase == 5)
        {
            request.minimum_score = 0;
            CHECK(reward->preview_experience(&request, &result) == RHH_PHOTO_REWARD_NOT_ELIGIBLE);
            request.flags = RHH_PHOTO_EXPERIENCE_ALLOW_UNSCORED;
            request.minimum_score = 1;
            CHECK(reward->claim_experience(&request, &result) == RHH_PHOTO_REWARD_NOT_ELIGIBLE);
            request.minimum_score = 0;
        }
        u32 amount = request.amount; request.amount = 777;
        CHECK(reward->claim_experience(&request, &result) == RHH_PHOTO_REWARD_DENIED
            && !strcmp(result.reason, "test veto") && !reward->experience_count()
            && !memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before))
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        request.amount = amount;
        CHECK(!reward->preview_experience(&request, &result) && result.is_preview && result.experience.applied == request.amount
            && !reward->experience_count() && !memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before)));
        RhhModsSetPersistence(RejectPhotoJournal);
        CHECK(reward->claim_experience(&request, &result) == RHH_PHOTO_REWARD_PERSISTENCE_FAILED
            && !reward->experience_count() && !memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before)));
        RhhModsSetPersistence(SavePhotoJournal);
        CHECK(reward->claim_experience(&request, &result) == RHH_PHOTO_REWARD_PENDING && result.changed
            && reward->experience_count() == 1 && !memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before)));
        if (phase == 8 || phase == 9 || phase == 10 || phase == 14)
        {
            if (phase != 9) gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][0].box;
            PhotoExperienceRunMenu(reward, FALSE);
            CHECK(!memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before)));
            if (phase != 9) memset(&gPokemonStoragePtr->boxes[0][0], 0, sizeof(struct BoxPokemon));
            ScriptContext_Init(); UnlockPlayerFieldControls(); PhotoRewardUpdate();
            CHECK(gMain.callback2 == CB2_Overworld);
            if (phase == 10 || phase == 14)
            {
                struct RhhPhotoExperienceRetargetRequest change = {.struct_size = sizeof(change),
                    .expected.struct_size = sizeof(change.expected), .recipient.struct_size = sizeof(change.recipient)};
                struct RhhPhotoExperienceRetargetResult changed = {.struct_size = sizeof(changed)};
                struct RhhOwnedMonSnapshot target = {.struct_size = sizeof(target)};
                CHECK(!reward->experience_at(0, &change.expected) && !owned->party_at(1, &target)
                    && !owned->experience(&target, &change.recipient));
                struct RhhPhotoExperienceRetargetRequest invalid = change;
                invalid.expected.recipient_identity ^= 1;
                CHECK(reward->retarget_experience(&invalid, &changed) == RHH_PHOTO_REWARD_STALE);
                gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][1].box;
                CHECK(reward->preview_experience_retarget(&change, &changed) == RHH_PHOTO_REWARD_UNAVAILABLE);
                memset(&gPokemonStoragePtr->boxes[0][0], 0, sizeof(struct BoxPokemon));
                CHECK(!reward->preview_experience_retarget(&change, &changed) && changed.is_preview && changed.changed
                    && changed.before.recipient_identity != changed.after.recipient_identity);
                CHECK(reward->retarget_experience(&change, &changed) == RHH_PHOTO_REWARD_DENIED
                    && !strcmp(changed.reason, "retarget veto"));
                if (phase == 10)
                {
                    RhhModsSetPersistence(RejectPhotoJournal);
                    CHECK(reward->retarget_experience(&change, &changed) == RHH_PHOTO_REWARD_PERSISTENCE_FAILED);
                    RhhModsSetPersistence(SavePhotoJournal);
                    struct RhhPhotoExperienceReceipt unchanged = {.struct_size = sizeof(unchanged)};
                    CHECK(!reward->experience_at(0, &unchanged) && unchanged.recipient_identity == change.expected.recipient_identity);
                    CHECK(!reward->retarget_experience(&change, &changed) && changed.changed
                        && changed.after.recipient_identity == target.identity);
                    CHECK(reward->retarget_experience(&change, &changed) == RHH_PHOTO_REWARD_STALE);
                }
                else
                {
                    for (u32 i = 0; i < RHH_PHOTO_EXPERIENCE_RETARGET_CAPACITY; ++i)
                    {
                        u32 slot = i % 2 ? 0 : 1;
                        CHECK(!reward->experience_at(0, &change.expected) && !owned->party_at(slot, &target)
                            && !owned->experience(&target, &change.recipient));
                        CHECK(!reward->retarget_experience(&change, &changed) && changed.changed);
                    }
                    CHECK(!reward->experience_at(0, &change.expected) && !owned->party_at(1, &target)
                        && !owned->experience(&target, &change.recipient));
                    CHECK(reward->retarget_experience(&change, &changed) == RHH_PHOTO_REWARD_FULL);
                    CHECK(!owned->party_at(0, &target) && !owned->experience(&target, &change.recipient));
                    CHECK(!reward->retarget_experience(&change, &changed) && !changed.changed);
                }
                PhotoRewardUpdate();
            }
            else CHECK(reward->deliver_experience(&result) == RHH_PHOTO_REWARD_PENDING);
        }
        PhotoExperienceRunMenu(reward, TRUE);
        ScriptContext_Init(); UnlockPlayerFieldControls();
        if (phase == 10)
        {
            u32 base = gExperienceTables[gSpeciesInfo[SPECIES_TORCHIC].growthRate][6];
            CHECK(!memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before))
                && GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_EXP) == base + request.amount);
        }
        else CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP) == request.recipient.experience + request.amount);
        struct RhhPhotoRewardRequest item = {.struct_size = sizeof(item), .quantity = 1, .minimum_score = phase == 5 ? 0 : 700,
            .owner = request.owner, .rule = request.rule, .item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION), .photo = entry.id};
        struct RhhPhotoRewardResult itemResult = {.struct_size = sizeof(itemResult)};
        CHECK((phase == 5 ? reward->claim_allow_unscored(&item, &itemResult) : reward->claim(&item, &itemResult))
            == RHH_PHOTO_REWARD_ALREADY);
        puts(phase == 14 ? "photo experience: retarget history capacity enforced"
            : phase == 10 ? "photo experience: retargeted receipt and delivered to new recipient"
            : phase == 9 ? "photo experience: denied handoff waits for explicit retry"
            : phase == 8 ? "photo experience: ambiguous recipient rolled back and retried"
            : phase == 5 ? "photo experience: unscored claim and real level-up passed"
            : "photo experience: durable claim and real level-up passed");
    }
    else
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK); AlbumTestMap();
        struct RhhPhotoExperienceReceipt receipt = {.struct_size = sizeof(receipt)};
        bool32 unscored = phase == 6 || phase == 7;
        u32 slot = phase >= 11 && phase != 17 ? 1 : 0;
        CHECK(reward->experience_count() == 1 && !reward->experience_at(0, &receipt)
            && receipt.amount == 800 && receipt.score == (unscored ? 0 : 777)
            && receipt.score_available == !unscored);
        if (slot == 1 || phase == 17)
        {
            struct RhhOwnedMonSnapshot target = {.struct_size = sizeof(target)};
            CHECK(!owned->party_at(slot, &target));
            if (phase != 16) CHECK(receipt.recipient_identity == target.identity);
        }
        u32 base = gExperienceTables[gSpeciesInfo[SPECIES_TORCHIC].growthRate][slot ? 6 : 5];
        if (phase == 16)
        {
            CHECK(!receipt.delivered);
            struct RhhPhotoExperienceRetargetRequest change = {.struct_size = sizeof(change), .expected = receipt,
                .recipient.struct_size = sizeof(change.recipient)};
            struct RhhPhotoExperienceRetargetResult changed = {.struct_size = sizeof(changed)};
            struct RhhOwnedMonSnapshot target = {.struct_size = sizeof(target)};
            CHECK(!owned->party_at(1, &target) && !owned->experience(&target, &change.recipient));
            CHECK(reward->retarget_experience(&change, &changed) == RHH_PHOTO_REWARD_DENIED);
            CHECK(!reward->retarget_experience(&change, &changed) && changed.after.recipient_identity == target.identity);
            CHECK(!reward->experience_at(0, &receipt) && !receipt.delivered && receipt.recipient_identity == target.identity);
            puts("photo experience: older save retargeted without delivery");
        }
        else if (phase == 2 || phase == 7 || phase == 12 || phase == 17)
        {
            CHECK(receipt.delivered && GetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_EXP) == base + 800);
            if (phase == 12)
            {
                struct RhhPhotoExperienceRetargetRequest change = {.struct_size = sizeof(change), .expected = receipt,
                    .recipient.struct_size = sizeof(change.recipient)};
                struct RhhPhotoExperienceRetargetResult changed = {.struct_size = sizeof(changed)};
                struct RhhOwnedMonSnapshot target = {.struct_size = sizeof(target)};
                CHECK(!owned->party_at(0, &target) && !owned->experience(&target, &change.recipient)
                    && reward->retarget_experience(&change, &changed) == RHH_PHOTO_REWARD_NOT_ELIGIBLE);
            }
            PhotoRewardUpdate(); CHECK(gMain.callback2 == CB2_Overworld
                && GetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_EXP) == base + 800);
            if (phase == 17)
                CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_EXP)
                    == gExperienceTables[gSpeciesInfo[SPECIES_TORCHIC].growthRate][6]);
            puts("photo experience: saved delivery did not repeat");
        }
        else
        {
            CHECK(!receipt.delivered && GetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_EXP) == base);
            if (phase == 1)
            {
                struct RhhPhotoRewardRequest blocked = {.struct_size = sizeof(blocked), .quantity = 1, .minimum_score = 700,
                    .owner = receipt.owner, .rule = RhhContentKeyOf("test:reward/other"),
                    .item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION), .photo = receipt.photo};
                struct RhhPhotoRewardResult blockedResult = {.struct_size = sizeof(blockedResult)};
                CHECK(reward->claim(&blocked, &blockedResult) == RHH_PHOTO_REWARD_PENDING);
                gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][0].box;
                PhotoRewardUpdate(); CHECK(gMain.callback2 == CB2_Overworld && !reward->experience_at(0, &receipt) && !receipt.delivered);
                memset(&gPokemonStoragePtr->boxes[0][0], 0, sizeof(struct BoxPokemon));
            }
            PhotoRewardUpdate(); PhotoExperienceRunMenu(reward, TRUE);
            CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_EXP) == base + 800);
            if (phase == 1 || phase == 6 || phase == 11) { CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); puts("photo experience: pending delivery replayed and saved"); }
            else puts("photo experience: older save received one replayed award");
        }
    }
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    NativeSaveClose(); RhhModsUnloadLibraries(); SDL_Quit();
}
