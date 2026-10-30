static void InvalidProfileSchemaStage(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; (void)decision; }
static void CheckWildProfileSchemaConflict(const char *first, const char *second, bool32 conflicting)
{
    RhhModsInit();
    struct RhhModHook invalid = {sizeof(invalid), RHH_WILD_PROFILE_SCHEMA, RHH_TRANSFORM, 0, InvalidProfileSchemaStage, NULL};
    struct RhhModDefinition rejected = {.struct_size = sizeof(rejected), .abi_version = RHH_MOD_ABI_SERVICES,
        .id = "test:bad_profile_schema_stage", .hook_count = 1, .hooks = &invalid};
    CHECK(RhhModsRegister(&rejected));
    invalid.event = RHH_WILD_PROFILE_SCHEMA_COLLECT; CHECK(RhhModsRegister(&rejected));
    InitModsForTestWithLibraries(first, second);
    const struct RhhWildProfileService *profile = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 2));
    struct RhhWildProfileSchema result = {.struct_size = sizeof(result), .kind = 99}, before = result;
    CHECK(profile);
    int status = profile->schema(RhhContentKeyOf("test:wild_profile_batch"), RhhContentKeyOf("test:batch/b"), &result);
    if (conflicting) CHECK(status == RHH_WILD_PROFILE_SCHEMA_CONFLICT && !memcmp(&result, &before, sizeof(result)));
    else CHECK(status == RHH_WILD_PROFILE_SCHEMA_FOUND && result.maximum == 25 && !strcmp(result.label, "Alert"));
    result = before;
    struct RhhWildProfileSchemaList list = {.struct_size = sizeof(list), .count = 99}, oldList = list;
    int listStatus = profile->schema_list(RhhContentKeyOf("test:wild_profile_batch"), &list);
    if (conflicting) CHECK(listStatus == RHH_WILD_PROFILE_SCHEMA_CONFLICT && !memcmp(&list, &oldList, sizeof(list)));
    else CHECK(listStatus == RHH_WILD_PROFILE_SCHEMA_FOUND && list.count == 2 && list.fields[0].field < list.fields[1].field);
    CHECK(profile->schema(RhhContentKeyOf("test:wild_profile_batch"), RhhContentKeyOf("test:batch/a"), &result)
        == RHH_WILD_PROFILE_SCHEMA_NOT_FOUND && !memcmp(&result, &before, sizeof(result)));
    RhhModsUnloadLibraries(); puts(conflicting ? "wild profile schema: independent conflicts fail closed"
                                             : "wild profile schema: independent matching offers compose");
}
static void CheckWildProfileSchemaLegacy(const char *first, const char *second)
{
    InitModsForTestWithLibraries(first, second);
    const struct RhhWildProfileService *profile = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 3));
    RhhContentKey owner = RhhContentKeyOf("test:wild_profile_batch"), legacy = RhhContentKeyOf("test:batch/legacy_only");
    struct RhhWildProfileSchema schema = {.struct_size = sizeof(schema)};
    struct RhhWildProfileSchemaList list = {.struct_size = sizeof(list)};
    CHECK(profile && !profile->schema(owner, legacy, &schema) && schema.field == legacy
        && !profile->schema_list(owner, &list) && list.count == 2
        && list.fields[0].field != legacy && list.fields[1].field != legacy);
    RhhModsUnloadLibraries(); puts("wild profile schema: legacy lookup remains available outside discovery");
}
static void CheckWildProfileBatch(const char *path, const char *module, bool32 load)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhWildProfileService *profiles = (const void *)host->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 1));
    const struct RhhWildActorService *actors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    CHECK(profiles && actors && profiles->header.struct_size >= RHH_WILD_PROFILE_SERVICE_V1_1_SIZE
        && host->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 0)) == &profiles->header
        && host->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 2)) == &profiles->header
        && host->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 3)) == &profiles->header
        && !host->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 5)));
    RhhContentKey owner = RhhContentKeyOf("test:wild_profile_batch");
    RhhContentKey a = RhhContentKeyOf("test:batch/a"), b = RhhContentKeyOf("test:batch/b");
    RhhContentKey boolean = RhhContentKeyOf("test:batch/boolean");
    RhhContentKey replacement = RhhContentKeyOf("test:batch/replacement"), denied = RhhContentKeyOf("test:profile/denied");
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
    struct RhhWildProfileBatchResult result = {.struct_size = sizeof(result)}, outside = result;
    CHECK(profiles->batch_context(&outside) && !memcmp(&outside, &result, sizeof(result)));
    struct RhhWildProfileSchema schema = {.struct_size = sizeof(schema), .kind = 99}, oldSchema = schema;
    CHECK(profiles->schema_context(&schema) && !memcmp(&schema, &oldSchema, sizeof(schema))
        && profiles->offer_schema(&schema));
    CHECK(profiles->schema(owner, a, &schema) == RHH_WILD_PROFILE_SCHEMA_NOT_FOUND && !memcmp(&schema, &oldSchema, sizeof(schema)));
    CHECK(profiles->schema(owner, b, &schema) == RHH_WILD_PROFILE_SCHEMA_FOUND
        && schema.kind == RHH_WILD_PROFILE_SIGNED && schema.minimum == 0 && schema.maximum == 25 && !strcmp(schema.label, "Alert"));
    CHECK(profiles->schema(owner, boolean, &schema) == RHH_WILD_PROFILE_SCHEMA_FOUND
        && schema.kind == RHH_WILD_PROFILE_BOOLEAN && !schema.minimum && schema.maximum == 1);
    struct RhhWildProfileSchemaList list = {.struct_size = sizeof(list)}, smallList = {.struct_size = sizeof(smallList) - 1, .count = 99};
    CHECK(!profiles->schema_list(owner, &list) && list.owner == owner && list.count == 2
        && list.fields[0].field < list.fields[1].field
        && ((list.fields[0].field == b && list.fields[1].field == boolean)
            || (list.fields[0].field == boolean && list.fields[1].field == b)));
    CHECK(profiles->schema_list(owner, &smallList) == RHH_WILD_PROFILE_SCHEMA_INVALID && smallList.count == 99);
    CHECK(!profiles->schema_list(RhhContentKeyOf("test:unknown_owner"), &list) && !list.count);
    if (!load)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
        struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 19);
        memset(gObjectEvents, 0, sizeof(gObjectEvents)); struct ObjectEvent *object = &gObjectEvents[0]; object->active = TRUE;
        object->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; object->localId = LOCALID_OW_ENCOUNTER_END;
        object->mapGroup = 1; object->mapNum = 0; object->trainerRange_berryTreeId = 5; object->warpArrowSpriteId = ROAMER_COUNT + 2;
        object->graphicsId = GetGraphicsIdForMon(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_IS_SHINY),
            GetGenderFromSpeciesAndPersonality(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_PERSONALITY)) == MON_FEMALE);
        ModsWildActorSpawned(object, &mon); CHECK(!actors->at(0, &actor));
        gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
        gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
        gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        gMain.callback2 = CB2_Overworld; ScriptContext_Init(); UnlockPlayerFieldControls();
        struct RhhWildProfileBatchRequest request = {.struct_size = sizeof(request), .count = 2,
            .actor_handle = actor.handle, .owner = owner,
            .items = {{.field = b, .present = 1, .value = 20}, {.field = a, .present = 1, .value = -10}}};
        request.items[0].value = 26;
        CHECK(profiles->preview_batch(&request, &result) == RHH_WILD_PROFILE_INVALID);
        struct RhhWildProfileRequest single = {.struct_size = sizeof(single), .actor_handle = actor.handle,
            .owner = owner, .field = b, .present = 1, .value = 26};
        struct RhhWildProfileResult singleResult = {.struct_size = sizeof(singleResult)};
        CHECK(profiles->apply(&single, &singleResult) == RHH_WILD_PROFILE_INVALID);
        request.items[0].value = 20;
        struct RhhModEvent update = {.struct_size = sizeof(update), .kind = RHH_MOD_UPDATE, .subject = actor.handle};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        update.args[0] = 98; RhhModsDispatch(&update, &decision); schema = oldSchema;
        CHECK(profiles->schema(owner, b, &schema) == RHH_WILD_PROFILE_SCHEMA_CONFLICT && !memcmp(&schema, &oldSchema, sizeof(schema))
            && profiles->apply_batch(&request, &result) == RHH_WILD_PROFILE_UNAVAILABLE);
        list = (struct RhhWildProfileSchemaList){.struct_size = sizeof(list), .count = 99};
        CHECK(profiles->schema_list(owner, &list) == RHH_WILD_PROFILE_SCHEMA_CONFLICT && list.count == 99);
        update.args[0] = 99; RhhModsDispatch(&update, &decision);
        CHECK(profiles->schema(owner, b, &schema) == RHH_WILD_PROFILE_SCHEMA_CONFLICT
            && profiles->apply(&single, &singleResult) == RHH_WILD_PROFILE_UNAVAILABLE);
        update.args[0] = 94; RhhModsDispatch(&update, &decision);
        CHECK(profiles->schema_list(owner, &list) == RHH_WILD_PROFILE_SCHEMA_CONFLICT && list.count == 99);
        update.args[0] = 93; RhhModsDispatch(&update, &decision);
        CHECK(profiles->schema_list(owner, &list) == RHH_WILD_PROFILE_SCHEMA_CONFLICT && list.count == 99);
        update.args[0] = 96; RhhModsDispatch(&update, &decision);
        struct RhhWildProfileBatchRequest booleanRequest = {.struct_size = sizeof(booleanRequest), .count = 1,
            .actor_handle = actor.handle, .owner = owner, .items = {{.field = boolean, .present = 1, .value = 2}}};
        CHECK(profiles->preview_batch(&booleanRequest, &result) == RHH_WILD_PROFILE_INVALID);
        booleanRequest.items[0].value = 1; CHECK(!profiles->preview_batch(&booleanRequest, &result));
        CHECK(!profiles->at(actor.handle, owner, boolean, &value) && !value.present && !value.revision);
        CHECK(!profiles->preview_batch(&request, &result) && result.is_preview && result.changed_count == 2
            && result.changes[0].after.field < result.changes[1].after.field && result.changes[0].after.revision == 1);
        CHECK(!profiles->at(actor.handle, owner, a, &value) && !value.present && !value.revision);
        request.items[1].field = denied;
        CHECK(profiles->apply_batch(&request, &result) == RHH_WILD_PROFILE_DENIED && !strcmp(result.reason, "batch veto"));
        CHECK(!profiles->at(actor.handle, owner, b, &value) && !value.present && !value.revision);
        request.items[1].field = a;
        CHECK(!profiles->apply_batch(&request, &result) && result.changed_count == 2);
        CHECK(!profiles->at(actor.handle, owner, a, &value) && value.present && value.value == -10 && value.revision == 1);
        update.args[0] = 95; RhhModsDispatch(&update, &decision);
        CHECK(!profiles->at(actor.handle, owner, b, &value) && value.present && value.value == 20 && value.revision == 1);
        single.expected_revision = 1; single.value = 20;
        CHECK(!profiles->apply(&single, &singleResult) && !singleResult.changed);
        single.value = 21; CHECK(profiles->apply(&single, &singleResult) == RHH_WILD_PROFILE_INVALID);
        update.args[0] = 96; RhhModsDispatch(&update, &decision);
        CHECK(profiles->apply_batch(&request, &result) == RHH_WILD_PROFILE_STALE);
        request.expected_revision = 1;
        CHECK(!profiles->apply_batch(&request, &result) && !result.changed_count && result.changes[0].after.revision == 1);
        request.items[1].field = request.items[0].field;
        CHECK(profiles->apply_batch(&request, &result) == RHH_WILD_PROFILE_INVALID);
        request.count = 6;
        for (u32 i = 0; i < request.count; ++i)
        { char name[32]; snprintf(name, sizeof(name), "test:batch/f%u", i); request.items[i] = (struct RhhWildProfileBatchItem){.field = RhhContentKeyOf(name), .present = 1, .value = i}; }
        CHECK(!profiles->apply_batch(&request, &result) && result.changed_count == 6 && result.changes[0].after.revision == 2);
        request.expected_revision = 2; request.count = 1;
        request.items[0] = (struct RhhWildProfileBatchItem){.field = replacement, .present = 1, .value = 42};
        CHECK(profiles->apply_batch(&request, &result) == RHH_WILD_PROFILE_FULL);
        request.count = 2; request.items[1] = (struct RhhWildProfileBatchItem){.field = a};
        CHECK(!profiles->apply_batch(&request, &result) && result.changed_count == 2 && result.changes[0].after.revision == 3);
        CHECK(!profiles->at(actor.handle, owner, a, &value) && !value.present && value.revision == 3);
        CHECK(!profiles->at(actor.handle, owner, b, &value) && value.present && value.value == 20 && value.revision == 3);
        CHECK(!profiles->at(actor.handle, owner, replacement, &value) && value.present && value.value == 42 && value.revision == 3);
        update.args[0] = 97; update.args[1] = 3;
        RhhModsDispatch(&update, &decision);
        CHECK(!profiles->at(actor.handle, owner, b, &value) && value.present && value.value == 21 && value.revision == 4);
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        update.args[0] = 98; RhhModsDispatch(&update, &decision);
        single.expected_revision = 4; single.present = 0; single.value = 0;
        CHECK(!profiles->apply(&single, &singleResult) && singleResult.changed
            && !profiles->at(actor.handle, owner, b, &value) && !value.present && value.revision == 5);
        free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
        puts("wild profile batch: atomic constraints, capacity and save passed");
    }
    else
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && !actors->at(0, &actor));
        CHECK(!profiles->at(actor.handle, owner, a, &value) && !value.present && value.revision == 4);
        CHECK(!profiles->at(actor.handle, owner, b, &value) && value.present && value.value == 21 && value.revision == 4);
        CHECK(!profiles->at(actor.handle, owner, replacement, &value) && value.present && value.value == 42 && value.revision == 4);
        puts("wild profile batch: fresh-process restore passed");
    }
    NativeSaveClose(); RhhModsUnloadLibraries();
}
static void CheckWildTraitEdit(const char *path, const char *module, bool32 load)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhWildTraitEditsService *edits = (const void *)host->get_service("rhh:service/wild_trait_edits", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWildTraitsService *traits = (const void *)host->get_service("rhh:service/wild_traits", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWildActorService *actors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    const struct RhhWildBehaviorService *behavior = (const void *)host->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 2));
    const struct RhhWildProfileService *profiles = (const void *)host->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 0));
    CHECK(edits && traits && actors && behavior && profiles);
    RhhContentKey tag = RhhContentKeyOf("test:trait/wary"), owner = RhhContentKeyOf("test:trait_edit");
    RhhContentKey field = RhhContentKeyOf("test:profile/wariness"), otherOwner = RhhContentKeyOf("test:mon_edit_rules");
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildTraitOverride stored = {.struct_size = sizeof(stored)};
    struct RhhWildTraitResult trait = {.struct_size = sizeof(trait)};
    struct RhhWildActivityState activity = {.struct_size = sizeof(activity)};
    struct RhhWildActivitySelection outside = {.struct_size = sizeof(outside), .valid = 99}, oldOutside = outside;
    struct RhhWildActivityChange change = {.struct_size = sizeof(change), .reserved = 99}, oldChange = change;
    struct RhhWildProfileValue profile = {.struct_size = sizeof(profile)}, profileSmall = {.struct_size = sizeof(profileSmall) - 1}, oldProfileSmall = profileSmall;
    struct RhhWildProfileResult profileResult = {.struct_size = sizeof(profileResult)}, outsideProfile = profileResult;
    CHECK(profiles->context(&outsideProfile) && !memcmp(&outsideProfile, &profileResult, sizeof(profileResult)));
    CHECK(behavior->selected(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside))
        && behavior->change_context(&change) && !memcmp(&change, &oldChange, sizeof(change)));
    if (!load)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
        struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 19);
        memset(gObjectEvents, 0, sizeof(gObjectEvents)); struct ObjectEvent *object = &gObjectEvents[0]; object->active = TRUE;
        object->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; object->localId = LOCALID_OW_ENCOUNTER_END;
        object->mapGroup = 1; object->mapNum = 0; object->trainerRange_berryTreeId = 5; object->warpArrowSpriteId = ROAMER_COUNT + 2;
        object->graphicsId = GetGraphicsIdForMon(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_IS_SHINY),
            GetGenderFromSpeciesAndPersonality(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_PERSONALITY)) == MON_FEMALE);
        ModsWildActorSpawned(object, &mon); CHECK(!actors->at(0, &actor));
        gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
        gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
        gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
        ResetSpriteData(); gSprites[0].inUse = TRUE; object->spriteId = 0;
        gMain.callback2 = CB2_Overworld; ScriptContext_Init(); UnlockPlayerFieldControls();
        struct RhhWildTraitEditRequest request = {.struct_size = sizeof(request), .actor_handle = actor.handle,
            .tag = tag, .owner = owner, .value = RHH_WILD_TRAIT_OFF};
        struct RhhWildTraitEditResult result = {.struct_size = sizeof(result)};
        CHECK(edits->apply(&request, &result) == RHH_WILD_TRAIT_EDIT_INVALID); /* no durable ID yet */
        struct RhhWildProfileRequest profileRequest = {.struct_size = sizeof(profileRequest), .present = 1,
            .actor_handle = actor.handle, .owner = owner, .field = field, .value = -27};
        CHECK(profiles->apply(&profileRequest, &profileResult) == RHH_WILD_PROFILE_INVALID);
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        CHECK(!profiles->at(actor.handle, owner, field, &profile) && !profile.present && !profile.revision && profile.owner_available);
        CHECK(profiles->at(actor.handle, owner, field, &profileSmall) && !memcmp(&profileSmall, &oldProfileSmall, sizeof(profileSmall)));
        CHECK(!profiles->preview(&profileRequest, &profileResult) && profileResult.changed && profileResult.is_preview
            && profileResult.after.value == -27 && profileResult.after.revision == 1);
        CHECK(!profiles->at(actor.handle, owner, field, &profile) && !profile.present && !profile.revision);
        CHECK(!profiles->apply(&profileRequest, &profileResult) && profileResult.changed && profileResult.after.revision == 1);
        CHECK(profiles->apply(&profileRequest, &profileResult) == RHH_WILD_PROFILE_STALE);
        profileRequest.expected_revision = 1;
        CHECK(!profiles->apply(&profileRequest, &profileResult) && !profileResult.changed && profileResult.after.revision == 1);
        profileRequest.field = RhhContentKeyOf("test:profile/deny");
        CHECK(profiles->apply(&profileRequest, &profileResult) == RHH_WILD_PROFILE_DENIED && !strcmp(profileResult.reason, "profile denied"));
        profileRequest.owner = otherOwner; profileRequest.field = field; profileRequest.value = INT64_MIN;
        CHECK(!profiles->apply(&profileRequest, &profileResult) && profileResult.after.revision == 2);
        CHECK(!profiles->at(actor.handle, otherOwner, field, &profile) && profile.present && profile.value == INT64_MIN);
        profileRequest.owner = owner; profileRequest.expected_revision = 2;
        for (unsigned i = 0; i < 6; ++i)
        {
            char name[32]; snprintf(name, sizeof(name), "test:profile/p%u", i);
            profileRequest.field = RhhContentKeyOf(name); profileRequest.value = i;
            CHECK(!profiles->apply(&profileRequest, &profileResult)); profileRequest.expected_revision = profileResult.after.revision;
        }
        profileRequest.field = RhhContentKeyOf("test:profile/overflow");
        CHECK(profiles->apply(&profileRequest, &profileResult) == RHH_WILD_PROFILE_FULL);
        profileRequest.present = 0; profileRequest.value = 0;
        for (unsigned i = 0; i < 6; ++i)
        {
            char name[32]; snprintf(name, sizeof(name), "test:profile/p%u", i); profileRequest.field = RhhContentKeyOf(name);
            CHECK(!profiles->apply(&profileRequest, &profileResult)); profileRequest.expected_revision = profileResult.after.revision;
        }
        profileRequest.owner = otherOwner; profileRequest.field = field;
        CHECK(!profiles->apply(&profileRequest, &profileResult) && profileResult.after.revision == 15);
        CHECK(!profiles->at(actor.handle, owner, field, &profile) && profile.present && profile.value == -27 && profile.revision == 15);
        struct ObjectEvent *laterSlot = &gObjectEvents[2], *earlierSlot = &gObjectEvents[1];
        *laterSlot = *object; laterSlot->localId = LOCALID_OW_ENCOUNTER_END - 1;
        ModsWildActorSpawned(laterSlot, &mon); ModsWildActorsUpdate();
        *earlierSlot = *object; earlierSlot->localId = LOCALID_OW_ENCOUNTER_END - 2;
        ModsWildActorSpawned(earlierSlot, &mon); ModsWildActorsUpdate();
        struct RhhWildActorSnapshot later = {.struct_size = sizeof(later)}, earlier = later;
        struct RhhWildActorIndividual laterId = {.struct_size = sizeof(laterId)}, earlierId = laterId;
        CHECK(!actors->at(2, &later) && !actors->at(1, &earlier)
            && !actors->individual(later.handle, &laterId) && !actors->individual(earlier.handle, &earlierId)
            && laterId.serial < earlierId.serial);
        rng_value_t beforeBehaviorRng = gRngValue;
        ModsWorldStep(0, 0, 0, 0); CHECK(!object->heldMovementActive
            && !memcmp(&beforeBehaviorRng, &gRngValue, sizeof(beforeBehaviorRng)));
        ScriptContext_Enable(); ModsWildActorsUpdate(); CHECK(!object->heldMovementActive);
        ScriptContext_Init(); UnlockPlayerFieldControls();
        beforeBehaviorRng = gRngValue; ModsWorldStep(0, 0, 0, 0);
        CHECK(!memcmp(&beforeBehaviorRng, &gRngValue, sizeof(beforeBehaviorRng)));
        CHECK(!NativeSaveBegin() && NativeSaveTransactionActive());
        ModsWildActorsUpdate(); CHECK(!object->heldMovementActive);
        CHECK(NativeSaveEnd(1, FALSE) && !NativeSaveTransactionActive());
        beforeBehaviorRng = gRngValue; ModsWorldStep(0, 0, 0, 0);
        CHECK(!memcmp(&beforeBehaviorRng, &gRngValue, sizeof(beforeBehaviorRng)));
        ScriptContext_Enable(); ModsWildActorsUpdate(); CHECK(!object->heldMovementActive);
        ScriptContext_Init(); UnlockPlayerFieldControls();
        CHECK(!behavior->at(actor.handle, &activity) && !activity.valid);
        ModsWorldStep(0, 0, 0, 0); ModsWildActorsUpdate();
        CHECK(!behavior->at(actor.handle, &activity) && activity.valid && activity.action == RHH_WILD_BEHAVIOR_NONE
            && activity.owner == owner && activity.since_step == 4 && activity.valid_until_step == 8);
        RhhContentKey acceptedActivity = activity.id;
        ModsWorldStep(0, 0, 0, 0); ScriptContext_Enable(); ModsWildActorsUpdate();
        ScriptContext_Init(); UnlockPlayerFieldControls();
        CHECK(!behavior->at(actor.handle, &activity) && activity.id == acceptedActivity && activity.since_step == 4);
        ModsWorldStep(0, 0, 0, 0); ModsWildActorsUpdate();
        CHECK(!behavior->at(actor.handle, &activity) && activity.id == acceptedActivity && activity.since_step == 4);
        ModsWorldStep(0, 0, 0, 0); ModsWildActorsUpdate();
        CHECK(!behavior->at(actor.handle, &activity) && activity.id == acceptedActivity && activity.since_step == 4);
        ClearObjectEvent(laterSlot); ClearObjectEvent(earlierSlot);
        CHECK(!edits->at(actor.handle, tag, &stored) && !stored.revision && !stored.value);
        CHECK(!edits->preview(&request, &result) && result.is_preview && result.after.revision == 1);
        CHECK(!edits->at(actor.handle, tag, &stored) && !stored.value && !stored.revision);
        CHECK(!edits->apply(&request, &result) && result.after.revision == 1);
        CHECK(!traits->query(actor.handle, tag, &trait) && trait.base == 1 && trait.value == 0);
        CHECK(edits->apply(&request, &result) == RHH_WILD_TRAIT_EDIT_STALE);
        request.expected_revision = 1;
        CHECK(!edits->apply(&request, &result) && result.after.revision == 1);
        request.owner = RhhContentKeyOf("test:mon_edit_rules");
        CHECK(edits->apply(&request, &result) == RHH_WILD_TRAIT_EDIT_CONFLICT);
        request.owner = owner; request.tag = RhhContentKeyOf("test:trait/deny"); request.value = RHH_WILD_TRAIT_ON;
        CHECK(edits->apply(&request, &result) == RHH_WILD_TRAIT_EDIT_DENIED && !strcmp(result.reason, "trait denied"));
        CHECK(!edits->at(actor.handle, tag, &stored) && stored.revision == 1);
        for (unsigned i = 0; i < 7; ++i)
        {
            char name[32]; snprintf(name, sizeof(name), "test:trait/t%u", i); request.tag = RhhContentKeyOf(name);
            CHECK(!edits->apply(&request, &result)); request.expected_revision = result.after.revision;
        }
        request.tag = RhhContentKeyOf("test:trait/t7");
        CHECK(edits->apply(&request, &result) == RHH_WILD_TRAIT_EDIT_FULL);
        for (unsigned i = 0; i < 7; ++i)
        {
            char name[32]; snprintf(name, sizeof(name), "test:trait/t%u", i); request.tag = RhhContentKeyOf(name);
            request.value = RHH_WILD_TRAIT_INHERIT; CHECK(!edits->apply(&request, &result));
            request.expected_revision = result.after.revision;
        }
        CHECK(request.expected_revision == 15 && !edits->at(actor.handle, tag, &stored) && stored.value == RHH_WILD_TRAIT_OFF);
        u8 *snapshot = NULL; u32 size = 0; CHECK(!RhhModsExport(&snapshot, &size));
        u8 *bad = malloc(size); CHECK(bad); memcpy(bad, snapshot, size); u32 activityAt = 24;
        u32 profileAt = 24;
        while (profileAt + 80 <= size && strcmp((const char *)bad + profileAt, "rhh:wild_profile_state"))
            profileAt += 80 + ClockTestGet32(bad + profileAt + 68);
        CHECK(profileAt + 80 + 16 + 24 <= size); memset(bad + profileAt + 80 + 16, 0, 8); ClockTestEnvelope(bad, size);
        CHECK(RhhModsImport(bad, size) && !profiles->at(actor.handle, owner, field, &profile) && profile.value == -27);
        memcpy(bad, snapshot, size); u32 profileRemoved = 80 + ClockTestGet32(bad + profileAt + 68);
        memmove(bad + profileAt, bad + profileAt + profileRemoved, size - profileAt - profileRemoved);
        ClockTestPut32(bad + 12, ClockTestGet32(bad + 12) - 1); ClockTestEnvelope(bad, size - profileRemoved);
        CHECK(!RhhModsImport(bad, size - profileRemoved) && !profiles->at(actor.handle, owner, field, &profile) && !profile.present);
        CHECK(!RhhModsImport(snapshot, size) && !profiles->at(actor.handle, owner, field, &profile) && profile.value == -27);
        memcpy(bad, snapshot, size);
        while (activityAt + 80 <= size && strcmp((const char *)bad + activityAt, "rhh:wild_activity_state"))
            activityAt += 80 + ClockTestGet32(bad + activityAt + 68);
        CHECK(activityAt + 80 + 48 <= size); bad[activityAt + 80 + 40] = 255; ClockTestEnvelope(bad, size);
        CHECK(RhhModsImport(bad, size) && !behavior->at(actor.handle, &activity) && activity.valid);
        memcpy(bad, snapshot, size); u32 activityRemoved = 80 + ClockTestGet32(bad + activityAt + 68);
        memmove(bad + activityAt, bad + activityAt + activityRemoved, size - activityAt - activityRemoved);
        ClockTestPut32(bad + 12, ClockTestGet32(bad + 12) - 1); ClockTestEnvelope(bad, size - activityRemoved);
        CHECK(!RhhModsImport(bad, size - activityRemoved) && !behavior->at(actor.handle, &activity) && !activity.valid);
        CHECK(!RhhModsImport(snapshot, size) && !behavior->at(actor.handle, &activity) && activity.valid);
        memcpy(bad, snapshot, size); u32 at = 24;
        while (at + 80 <= size && strcmp((const char *)bad + at, "rhh:wild_trait_state")) at += 80 + ClockTestGet32(bad + at + 68);
        CHECK(at + 80 + 16 + 24 <= size); bad[at + 80 + 16 + 16] = 3; ClockTestEnvelope(bad, size);
        CHECK(RhhModsImport(bad, size) && !edits->at(actor.handle, tag, &stored) && stored.value == RHH_WILD_TRAIT_OFF);
        memcpy(bad, snapshot, size);
        u32 removed = 80 + ClockTestGet32(bad + at + 68);
        memmove(bad + at, bad + at + removed, size - at - removed);
        ClockTestPut32(bad + 12, ClockTestGet32(bad + 12) - 1); ClockTestEnvelope(bad, size - removed);
        CHECK(!RhhModsImport(bad, size - removed) && !edits->at(actor.handle, tag, &stored)
            && !stored.revision && !stored.value); /* Older snapshot without the new host state. */
        CHECK(!RhhModsImport(snapshot, size) && !edits->at(actor.handle, tag, &stored)
            && stored.revision == 15 && stored.value == RHH_WILD_TRAIT_OFF);
        free(snapshot); free(bad);
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK); free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
        puts("wild trait edits: previews, ownership, capacity, constraints and state validation passed");
    }
    else
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK); CHECK(!actors->at(0, &actor));
        CHECK(!behavior->at(actor.handle, &activity) && activity.valid && activity.owner == owner
            && activity.action == RHH_WILD_BEHAVIOR_NONE && activity.since_step == 4 && activity.valid_until_step == 8);
        CHECK(!edits->at(actor.handle, tag, &stored) && stored.value == RHH_WILD_TRAIT_OFF && stored.owner == owner && stored.revision == 15);
        CHECK(!profiles->at(actor.handle, owner, field, &profile) && profile.present && profile.value == -27 && profile.revision == 15);
        CHECK(!traits->query(actor.handle, tag, &trait) && trait.base == 1 && trait.value == 0);
        u8 *state = RhhModsState("rhh:wild_trait_state"); CHECK(state);
        ClockTestPut64(state + 16 + 8, RhhContentKeyOf("test:missing_owner"));
        u8 *activityState = RhhModsState("rhh:wild_activity_state"); CHECK(activityState);
        ClockTestPut64(activityState + 16, RhhContentKeyOf("test:missing_owner"));
        u8 *profileState = RhhModsState("rhh:wild_profile_state"); CHECK(profileState);
        RhhContentKey missingOwner = RhhContentKeyOf("test:missing_owner"); ClockTestPut64(profileState + 16, missingOwner);
        ModsWildActorsAfterLoad(); CHECK(!actors->at(0, &actor));
        CHECK(!profiles->at(actor.handle, missingOwner, field, &profile) && profile.present
            && !profile.owner_available && profile.value == -27 && profile.revision == 15);
        CHECK(!edits->at(actor.handle, tag, &stored) && !stored.value && stored.revision == 16);
        CHECK(!behavior->at(actor.handle, &activity) && !activity.valid);
        CHECK(!traits->query(actor.handle, tag, &trait) && trait.value == 1);
        ClearObjectEvent(&gObjectEvents[0]);
        for (u32 i = 0; i < 16 + 8 * 24; ++i) CHECK(!state[i]);
        state = RhhModsState("rhh:wild_activity_state"); CHECK(state);
        for (u32 i = 0; i < 48; ++i) CHECK(!state[i]);
        for (u32 i = 0; i < 16 + RHH_WILD_PROFILE_CAPACITY * 24; ++i) CHECK(!profileState[i]);
        puts("wild trait edits: fresh-process restore, missing owner and removal passed");
    }
    NativeSaveClose(); RhhModsUnloadLibraries();
}
static void CheckWildProfileOwner(const char *path, const char *module, bool32 ownerPresent)
{
    InitModsForTestWithLibrary(ownerPresent ? module : NULL);
    CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhWildActorService *actors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    const struct RhhWildProfileService *profiles = (const void *)host->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 0));
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
    CHECK(actors && profiles && !actors->at(0, &actor)
        && !profiles->at(actor.handle, RhhContentKeyOf("test:trait_edit"), RhhContentKeyOf("test:profile/wariness"), &value)
        && value.present && value.owner_available == ownerPresent && value.value == -27 && value.revision == 15);
    if (!ownerPresent) CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts(ownerPresent ? "wild profile: missing owner returned" : "wild profile: missing owner saved");
}
