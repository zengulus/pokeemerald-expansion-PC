struct BreedingObserverTrace { u32 count; rng_value_t rng, rng2; enum Ability ability; };
static struct BreedingObserverTrace sBreedingObserverTrace[4];
static struct RhhDaycareEggId sObservedPendingId;
static const struct RhhBreedingService *sObservedBreeding;
static void BreedingObserverRng(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    u32 index;
    switch (event->kind) {
    case RHH_DAYCARE_PENDING_CHANGED: index = 0; break;
    case RHH_DAYCARE_EGG_REQUESTED: index = 1; break;
    case RHH_BREED_EGG_CREATED: index = 2; break;
    case RHH_EGG_HATCHED: index = 3; break;
    default: CHECK(FALSE); return;
    }
    struct BreedingObserverTrace *trace = &sBreedingObserverTrace[index];
    ++trace->count; trace->rng = gRngValue; trace->rng2 = gRng2Value; trace->ability = gLastUsedAbility;
    if (event->kind == RHH_DAYCARE_PENDING_CHANGED || event->kind == RHH_BREED_EGG_CREATED) {
        struct RhhDaycareEggId id = {.struct_size = sizeof(id)};
        CHECK(!sObservedBreeding->pending_change_id(&id));
        if (!sObservedPendingId.high && !sObservedPendingId.low) sObservedPendingId = id;
        else CHECK(id.run_id == sObservedPendingId.run_id && id.high == sObservedPendingId.high && id.low == sObservedPendingId.low);
    }
    (void)Random32(); (void)Random2_32(); gLastUsedAbility = ABILITY_STENCH;
}
static const struct RhhModHook sBreedingObserverRngHooks[] = {
    {sizeof(struct RhhModHook), RHH_DAYCARE_PENDING_CHANGED, RHH_OBSERVE, 0, NULL, BreedingObserverRng},
    {sizeof(struct RhhModHook), RHH_DAYCARE_EGG_REQUESTED, RHH_OBSERVE, 0, NULL, BreedingObserverRng},
    {sizeof(struct RhhModHook), RHH_BREED_EGG_CREATED, RHH_OBSERVE, 0, NULL, BreedingObserverRng},
    {sizeof(struct RhhModHook), RHH_EGG_HATCHED, RHH_OBSERVE, 0, NULL, BreedingObserverRng}
};
static const struct RhhModDefinition sBreedingObserverRngMod = {
    .struct_size = sizeof(sBreedingObserverRngMod), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:breeding_observer_rng", .hooks = sBreedingObserverRngHooks,
    .hook_count = ARRAY_COUNT(sBreedingObserverRngHooks)
};
static bool32 BreedingObserverRngMatches(const struct BreedingObserverTrace *trace)
{
    return !memcmp(&trace->rng, &gRngValue, sizeof(trace->rng))
        && !memcmp(&trace->rng2, &gRng2Value, sizeof(trace->rng2)) && trace->ability == gLastUsedAbility;
}
static void CheckBreedingObserverRng(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sBreedingObserverRngMod) && !RhhModsFinalize());
    gModDraft = (struct ModConfig){0}; ModsNewGame(7);
    sObservedBreeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 28));
    const struct RhhEggHatchService *hatch = (const void *)RhhModsHostAPI()->get_service("rhh:service/egg_hatch", RHH_SERVICE_VERSION(1, 1));
    CHECK(sObservedBreeding && hatch && hatch->header.struct_size >= RHH_EGG_HATCH_SERVICE_V1_1_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 29))
        && !RhhModsHostAPI()->get_service("rhh:service/egg_hatch", RHH_SERVICE_VERSION(1, 2)));
    memset(sBreedingObserverTrace, 0, sizeof(sBreedingObserverTrace)); memset(&sObservedPendingId, 0, sizeof(sObservedPendingId));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    struct Pokemon parent; CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12321), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    gMain.inBattle = FALSE; gMain.callback2 = CB2_Overworld; memset(&gPaletteFade, 0, sizeof(gPaletteFade));
    ScriptContext_Init(); UnlockPlayerFieldControls(); FlagClear(FLAG_PENDING_DAYCARE_EGG);
    gLastUsedAbility = ABILITY_PRESSURE;
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingRequestResult request = {.struct_size = sizeof(request)};
    CHECK(!sObservedBreeding->offer(&offer) && sObservedBreeding->request_pending(&offer, &request) == RHH_BREED_REQUEST_OK);
    CHECK(sBreedingObserverTrace[0].count == 1 && sBreedingObserverTrace[1].count == 1
        && BreedingObserverRngMatches(&sBreedingObserverTrace[0]) && BreedingObserverRngMatches(&sBreedingObserverTrace[1]));
    struct RhhBreedingAcceptResult accepted = {.struct_size = sizeof(accepted)};
    CHECK(!sObservedBreeding->offer(&offer) && sObservedBreeding->accept(&offer, &accepted) == RHH_BREED_ACCEPT_OK
        && accepted.egg_valid && accepted.egg.is_egg && accepted.party_slot == 0);
    CHECK(sBreedingObserverTrace[0].count == 2 && sBreedingObserverTrace[2].count == 1
        && BreedingObserverRngMatches(&sBreedingObserverTrace[0]) && BreedingObserverRngMatches(&sBreedingObserverTrace[2]));
    gSpecialVar_0x8004 = accepted.party_slot; ScriptHatchMon();
    CHECK(sBreedingObserverTrace[3].count == 1 && BreedingObserverRngMatches(&sBreedingObserverTrace[3])
        && !GetMonData(&gParties[B_TRAINER_PLAYER][accepted.party_slot], MON_DATA_IS_EGG));
    RhhModsShutdown(); puts("breeding observers: pending, request, creation and hatch preserve gameplay RNG");
}
