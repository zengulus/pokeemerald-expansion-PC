static u32 sOwnedWatchFrameExp;
static void OwnedWatchFrameTick(void)
{
    u32 intermediate = gExperienceTables[gSpeciesInfo[SPECIES_TORCHIC].growthRate][8];
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP, &intermediate);
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP, &sOwnedWatchFrameExp);
    CalculateMonStats(&gParties[B_TRAINER_PLAYER][0]);
    gMain.callback1 = NULL;
}
static void InvalidOwnedWatchTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; (void)decision; }
#ifdef RHH_OWNED_IDENTITY_TEST
bool32 ModsOwnedStorageTestOpen(void);
void ModsOwnedStorageTestClose(void);
#endif
static void CheckOwnedMonChanges(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins());
    struct RhhModHook invalid = {sizeof(invalid), RHH_OWNED_MON_CHANGED, RHH_TRANSFORM, 0, InvalidOwnedWatchTransform, NULL};
    struct RhhModDefinition invalidMod = {.struct_size = sizeof(invalidMod), .abi_version = RHH_MOD_ABI_SERVICES,
        .id = "test:invalid_owned_watch", .hook_count = 1, .hooks = &invalid};
    CHECK(RhhModsRegister(&invalidMod));
    CHECK(!RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 41));
    const struct RhhMonEditService *edit = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 0));
    CHECK(mons && edit && host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0)) == &mons->header
        && RHH_OWNED_MON_SERVICE_V1_40_SIZE < RHH_OWNED_MON_SERVICE_V1_41_SIZE
        && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_41_SIZE);
    struct RhhOwnedMonChange outside = {.struct_size = sizeof(outside), .flags = 99}, oldOutside = outside;
    CHECK(mons->change_context(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    struct RhhOwnedExperienceChange outsideXp = {.struct_size = sizeof(outsideXp), .before_exp = 99}, oldOutsideXp = outsideXp;
    CHECK(mons->experience_change_context(&outsideXp) && !memcmp(&outsideXp, &oldOutsideXp, sizeof(outsideXp)));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(&gPokemonStoragePtr->boxes[0][0], 0, sizeof(struct BoxPokemon));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(&gPokemonStoragePtr->fusions[0], 0, sizeof(struct Pokemon));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1; gMain.inBattle = FALSE;
    struct Pokemon initialMon = gParties[B_TRAINER_PLAYER][0];
    struct RhhOwnedMonSnapshot teraMon = {.struct_size = sizeof(teraMon)};
    struct RhhOwnedTeraView tera = {.struct_size = sizeof(tera)}, smallTera = {.struct_size = 1, .flags = 99}, savedTera = smallTera;
    for (u32 personality = 0; personality < 24; ++personality)
    {
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, personality, OTID_STRUCT_PRESET(54321), 31);
        u32 stored = personality % 2 ? TYPE_WATER : TYPE_NONE;
        SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE, &stored);
        CHECK(!mons->party_at(0, &teraMon));
        struct BoxPokemon raw = gParties[B_TRAINER_PLAYER][0].box;
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        CHECK(!mons->tera_view(&teraMon, &tera) && tera.stored_ordinal == stored
            && tera.source == (stored ? RHH_OWNED_TERA_STORED : RHH_OWNED_TERA_PERSONALITY)
            && tera.resolved_ordinal == (stored ? TYPE_WATER : TYPE_FIRE)
            && tera.resolved_type == RhhContentKeyForId(RHH_TYPE, tera.resolved_ordinal) && !tera.flags
            && !memcmp(&raw, &gParties[B_TRAINER_PLAYER][0].box, sizeof(raw))
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    }
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TERAPAGOS_STELLAR, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    u32 forcedStored = TYPE_WATER; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE, &forcedStored);
    CHECK(!mons->party_at(0, &teraMon) && !mons->tera_view(&teraMon, &tera) && tera.source == RHH_OWNED_TERA_FORCED
        && tera.stored_ordinal == TYPE_WATER && tera.resolved_ordinal == TYPE_STELLAR
        && tera.resolved_type == RhhContentKeyForId(RHH_TYPE, TYPE_STELLAR));
    gParties[B_TRAINER_PLAYER][0] = initialMon;
    CHECK(!mons->party_at(0, &teraMon) && mons->tera_view(&teraMon, &smallTera) && !memcmp(&smallTera, &savedTera, sizeof(smallTera)));
    struct RhhOwnedTeraView untouched = {.struct_size = sizeof(untouched), .flags = 99}, expectedUntouched = untouched;
    struct RhhOwnedMonSnapshot staleTera = teraMon; ++staleTera.identity;
    CHECK(mons->tera_view(&staleTera, &untouched) && !memcmp(&untouched, &expectedUntouched, sizeof(untouched)));
    gMain.inBattle = TRUE;
    CHECK(mons->tera_view(&teraMon, &untouched) && !memcmp(&untouched, &expectedUntouched, sizeof(untouched)));
    gMain.inBattle = FALSE;
    u32 isEgg = TRUE; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &isEgg);
    CHECK(!mons->party_at(0, &teraMon) && teraMon.is_egg && mons->tera_view(&teraMon, &untouched)
        && !memcmp(&untouched, &expectedUntouched, sizeof(untouched)));
    gParties[B_TRAINER_PLAYER][0] = initialMon; CHECK(!mons->party_at(0, &teraMon));
    gParties[B_TRAINER_PLAYER][0].box.secure.raw[0] ^= 1;
    struct BoxPokemon corrupt = gParties[B_TRAINER_PLAYER][0].box;
    CHECK(mons->tera_view(&teraMon, &untouched) && !memcmp(&untouched, &expectedUntouched, sizeof(untouched))
        && !memcmp(&corrupt, &gParties[B_TRAINER_PLAYER][0].box, sizeof(corrupt)));
    gParties[B_TRAINER_PLAYER][0] = initialMon;
    RhhModsObserveOwnedMons(); RhhModsObserveOwnedMons();
    sOwnedWatchFrameExp = gExperienceTables[gSpeciesInfo[SPECIES_TORCHIC].growthRate][10];
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); cgb_audio_init(42060); m4aSoundInit();
    gMain.callback1 = OwnedWatchFrameTick; NativeFrame();
    CHECK(!gMain.callback1);
    CHECK(!mons->party_at(0, &teraMon) && !mons->tera_view(&teraMon, &tera));
    u32 initialTera = tera.stored_ordinal;
    const u32 teraChanges[] = {TYPE_NONE, TYPE_WATER, 31, TYPE_FIRE};
    for (u32 i = 0; i < ARRAY_COUNT(teraChanges); ++i)
    {
        SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE, &teraChanges[i]);
        CHECK(!mons->tera_view(&teraMon, &tera) && tera.stored_ordinal == teraChanges[i]);
        if (teraChanges[i] == 31) CHECK((tera.flags & (RHH_OWNED_TERA_UNSUPPORTED_STORED | RHH_OWNED_TERA_UNSUPPORTED_RESOLVED))
            == (RHH_OWNED_TERA_UNSUPPORTED_STORED | RHH_OWNED_TERA_UNSUPPORTED_RESOLVED) && !tera.stored_type && !tera.resolved_type);
        RhhModsObserveOwnedMons();
    }
    CHECK(initialTera == TYPE_FIRE);
    struct RhhMonEditRequest request = {.struct_size = sizeof(request), .fields = RHH_EDIT_IVS | RHH_EDIT_EVS,
        .expected.struct_size = sizeof(request.expected)};
    struct RhhMonEditResult result = {.struct_size = sizeof(result)};
    CHECK(!mons->party_at(0, &request.expected));
    memcpy(request.ivs, request.expected.ivs, sizeof(request.ivs));
    memcpy(request.evs, request.expected.evs, sizeof(request.evs));
    request.ivs[0] = 12; request.evs[0] = 16;
    CHECK(edit->preview(&request, &result) == RHH_EDIT_OK); RhhModsObserveOwnedMons();
    CHECK(edit->commit(&request, &result) == RHH_EDIT_OK); RhhModsObserveOwnedMons();
    u32 trained = 1;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HYPER_TRAINED_ATK, &trained); RhhModsObserveOwnedMons();
    trained = 0;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HYPER_TRAINED_ATK, &trained); RhhModsObserveOwnedMons();
    const struct RhhUiService *ui = (const void *)host->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 0));
    struct RhhModEvent tick = {.struct_size = sizeof(tick), .kind = RHH_MOD_UPDATE, .args = {77, 0, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&tick, &decision); CHECK(ui && ui->active());
    NativeFrame(); CHECK(!ui->active()); SDL_Quit();
    u32 sameLevelExp = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP) + 1;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP, &sameLevelExp); RhhModsObserveOwnedMons();
    ++sameLevelExp; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP, &sameLevelExp);
    ++sameLevelExp; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP, &sameLevelExp); RhhModsObserveOwnedMons();
    u32 transient = sameLevelExp + 1;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP, &transient);
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP, &sameLevelExp); RhhModsObserveOwnedMons();
    u8 nickname[] = {CHAR_S, CHAR_a, CHAR_m, CHAR_p, CHAR_l, CHAR_e, EOS};
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_NICKNAME, nickname);
    RhhModsObserveOwnedMons();
    u32 exp = gExperienceTables[gSpeciesInfo[SPECIES_TORCHIC].growthRate][11];
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP, &exp);
    gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_LINK; RhhModsObserveOwnedMons();
    gMain.inBattle = FALSE; gBattleTypeFlags = 0; CalculateMonStats(&gParties[B_TRAINER_PLAYER][0]);
    RhhModsObserveOwnedMons();
    gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][0].box;
    memset(&gParties[B_TRAINER_PLAYER][0], 0, sizeof(struct Pokemon)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    RhhModsObserveOwnedMons();
    request = (struct RhhMonEditRequest){.struct_size = sizeof(request), .fields = RHH_EDIT_IVS,
        .expected.struct_size = sizeof(request.expected)};
    CHECK(!mons->box_at(0, 0, &request.expected));
    CHECK(!mons->tera_view(&request.expected, &tera) && tera.mon.location == RHH_MON_BOX);
    memcpy(request.ivs, request.expected.ivs, sizeof(request.ivs)); request.ivs[1] = 11;
    CHECK(edit->commit(&request, &result) == RHH_EDIT_OK); RhhModsObserveOwnedMons();
    sameLevelExp = GetBoxMonData(&gPokemonStoragePtr->boxes[0][0], MON_DATA_EXP) + 1;
    SetBoxMonData(&gPokemonStoragePtr->boxes[0][0], MON_DATA_EXP, &sameLevelExp); RhhModsObserveOwnedMons();
    struct Pokemon replacement;
    CreateMonWithIVs(&replacement, SPECIES_POOCHYENA, 7, 12345, OTID_STRUCT_PRESET(54322), 31);
    gPokemonStoragePtr->boxes[0][0] = replacement.box; RhhModsObserveOwnedMons();
    gSaveBlock1Ptr->daycare.mons[0].mon = replacement.box;
    memset(&gPokemonStoragePtr->boxes[0][0], 0, sizeof(struct BoxPokemon)); RhhModsObserveOwnedMons();
    CHECK(!mons->daycare_at(0, 0, &teraMon) && !mons->tera_view(&teraMon, &tera) && tera.mon.location == RHH_MON_DAYCARE);
    sameLevelExp = GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_EXP) + 1;
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_EXP, &sameLevelExp); RhhModsObserveOwnedMons();
    gPokemonStoragePtr->fusions[0] = replacement;
    memset(&gSaveBlock1Ptr->daycare.mons[0].mon, 0, sizeof(struct BoxPokemon)); RhhModsObserveOwnedMons();
    CHECK(!mons->fusion_at(0, &teraMon) && !mons->tera_view(&teraMon, &tera) && tera.mon.location == RHH_MON_FUSION_STORAGE);
    u32 iv = 10; SetBoxMonData(&gPokemonStoragePtr->fusions[0].box, MON_DATA_HP_IV, &iv);
    sameLevelExp = GetBoxMonData(&gPokemonStoragePtr->fusions[0].box, MON_DATA_EXP) + 1;
    SetBoxMonData(&gPokemonStoragePtr->fusions[0].box, MON_DATA_EXP, &sameLevelExp);
    puts("OWNED_WATCH_DEFER_BEGIN");
    gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_FACTORY; RhhModsObserveOwnedMons();
    gMain.inBattle = FALSE; gBattleTypeFlags = 0;
#ifdef RHH_OWNED_IDENTITY_TEST
    MainCallback previousCallback = gMain.callback2;
    CHECK(ModsOwnedStorageTestOpen()); RhhModsObserveOwnedMons();
    ModsOwnedStorageTestClose(); SetMainCallback2(previousCallback);
#endif
    puts("OWNED_WATCH_DEFER_END");
    RhhModsObserveOwnedMons();
    memset(&gPokemonStoragePtr->fusions[0], 0, sizeof(struct Pokemon)); RhhModsObserveOwnedMons();
    RhhModsResetOwnedMonWatch(); RhhModsObserveOwnedMons();
    RhhModsSetRunContext(123, 789, 0); RhhModsObserveOwnedMons();
    CHECK(mons->change_context(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    CHECK(mons->experience_change_context(&outsideXp) && !memcmp(&outsideXp, &oldOutsideXp, sizeof(outsideXp)));
    RhhModsUnloadLibraries(); puts("owned watch: level, edits, storage, link and resets passed");
}
