static void BreedingPurityHook(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    const u8 *mode = state;
    u32 selected = event->kind == RHH_BREED_COMPATIBILITY ? 1 : event->kind == RHH_BREED_OFFSPRING ? 2
        : event->kind == RHH_CAN_ACCEPT_DAYCARE_EGG ? 3 : 4;
    if (mode[0] != selected) return;
    if (mode[1] & 1) Random32();
    if (mode[1] & 2) Random2_32();
    if (mode[1] & 4) decision->allowed = 0;
    if (selected == 1) decision->value = RHH_DAYCARE_INCOMPATIBLE;
    if (selected == 2) decision->value = RhhContentKeyForId(RHH_SPECIES, SPECIES_EEVEE);
}
static void BreedingPurityCompatibilityDeny(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    const u8 *mode = state;
    if (mode[0] == 1 && (mode[1] & 4)) decision->allowed = 0;
}
static const struct RhhModHook sBreedingPurityHooks[] = {
    {sizeof(struct RhhModHook), RHH_BREED_COMPATIBILITY, RHH_TRANSFORM, 0, BreedingPurityHook, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_COMPATIBILITY, RHH_CONSTRAINT, 0, BreedingPurityCompatibilityDeny, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_OFFSPRING, RHH_TRANSFORM, 0, BreedingPurityHook, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_ACCEPT_DAYCARE_EGG, RHH_CONSTRAINT, 0, BreedingPurityHook, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_REQUEST_DAYCARE_EGG, RHH_CONSTRAINT, 0, BreedingPurityHook, NULL}
};
static const struct RhhModDefinition sBreedingPurityMod = {
    .struct_size = sizeof(sBreedingPurityMod), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:breeding_purity", .state_size = 2, .hook_count = 5, .hooks = sBreedingPurityHooks
};
static void CheckBreedingPurity(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sBreedingPurityMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 18));
    u8 *mode = RhhModsState("test:breeding_purity"); CHECK(breeding && mode);
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    gMain.inBattle = FALSE; gMain.callback2 = CB2_Overworld; memset(&gPaletteFade, 0, sizeof(gPaletteFade));
    ScriptContext_Init(); UnlockPlayerFieldControls(); FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct RhhBreedingPreview baseline = {.struct_size = sizeof(baseline)};
    CHECK(!breeding->preview(&baseline) && baseline.compatibility != RHH_DAYCARE_INCOMPATIBLE);
    for (u32 selected = 1; selected <= 2; ++selected)
        for (u32 stream = 1; stream <= 3; ++stream)
        {
            mode[0] = selected; mode[1] = stream;
            rng_value_t rng = gRngValue, rng2 = gRng2Value;
            struct RhhBreedingPreview preview = {.struct_size = sizeof(preview)};
            CHECK(!breeding->preview(&preview) && !memcmp(&preview, &baseline, sizeof(preview))
                && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        }
    mode[0] = 1; mode[1] = 7;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBreedingPreview deniedPreview = {.struct_size = sizeof(deniedPreview)};
    CHECK(!breeding->preview(&deniedPreview) && deniedPreview.compatibility == RHH_DAYCARE_INCOMPATIBLE
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    mode[0] = 0; TriggerPendingDaycareEgg();
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    CHECK(!breeding->offer(&offer));
    for (u32 stream = 1; stream <= 3; ++stream)
        for (u32 denied = 0; denied <= 1; ++denied)
        {
            mode[0] = 3; mode[1] = stream | (denied ? 4 : 0);
            rng_value_t rng = gRngValue, rng2 = gRng2Value;
            struct RhhBreedingAcceptResult result = {.struct_size = sizeof(result)};
            CHECK(breeding->accept(&offer, &result) == (denied ? RHH_BREED_ACCEPT_DENIED : RHH_BREED_ACCEPT_STALE)
                && FlagGet(FLAG_PENDING_DAYCARE_EGG) && !gPartiesCount[B_TRAINER_PLAYER]
                && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        }
    mode[0] = 0; FlagClear(FLAG_PENDING_DAYCARE_EGG); CHECK(!breeding->offer(&offer));
    for (u32 stream = 1; stream <= 3; ++stream)
        for (u32 denied = 0; denied <= 1; ++denied)
        {
            mode[0] = 4; mode[1] = stream | (denied ? 4 : 0);
            rng_value_t rng = gRngValue, rng2 = gRng2Value;
            struct RhhBreedingRequestResult result = {.struct_size = sizeof(result)};
            CHECK(breeding->request_pending(&offer, &result) == (denied ? RHH_BREED_REQUEST_DENIED : RHH_BREED_REQUEST_STALE)
                && !FlagGet(FLAG_PENDING_DAYCARE_EGG) && !gPartiesCount[B_TRAINER_PLAYER]
                && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        }
    RhhModsUnloadLibraries(); puts("breeding purity: both RNG streams restored and unsafe validation rejected");
}
