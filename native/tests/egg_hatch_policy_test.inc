static const struct RhhDaycareService *sHatchPolicyDaycare;
static u32 sHatchPolicyMode, sHatchPolicyCalls;
static void HatchPolicyConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhEggHatchEligibility context = {.struct_size = sizeof(context)};
    struct RhhEggHatchEligibility nested = {.struct_size = sizeof(nested), .status = 99}, saved = nested;
    CHECK(!sHatchPolicyDaycare->hatch_eligibility_context(&context)
        && context.egg.slot == event->args[0] && context.egg.species == event->event_key
        && context.egg.identity == event->subject && context.status == RHH_EGG_HATCH_ALLOWED
        && !context.cycles_remaining && !RhhModsCommandsAllowed()
        && sHatchPolicyDaycare->hatch_eligibility(context.egg.slot, &nested)
        && !memcmp(&nested, &saved, sizeof(nested)));
    ++sHatchPolicyCalls;
    (void)GetMonAbility(&gParties[B_TRAINER_PLAYER][0]);
    if (sHatchPolicyMode == 1 && !context.egg.slot) decision->allowed = 0;
    if (sHatchPolicyMode == 2) decision->allowed = 0;
    if (sHatchPolicyMode == 3) { (void)Random32(); (void)Random2_32(); }
}
static const struct RhhModHook sHatchPolicyHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_HATCH_EGG, RHH_CONSTRAINT, 0, HatchPolicyConstraint, NULL}
};
static const struct RhhModDefinition sHatchPolicyMod = {.struct_size = sizeof(sHatchPolicyMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:hatch_policy",
    .hook_count = ARRAY_COUNT(sHatchPolicyHooks), .hooks = sHatchPolicyHooks};
static void CheckEggHatchPolicy(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sHatchPolicyMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sHatchPolicyDaycare = (const void *)RhhModsHostAPI()->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 3));
    CHECK(sHatchPolicyDaycare && sHatchPolicyDaycare->header.struct_size >= RHH_DAYCARE_SERVICE_V1_3_SIZE
        && RHH_DAYCARE_SERVICE_V1_2_SIZE < RHH_DAYCARE_SERVICE_V1_3_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 2)) == &sHatchPolicyDaycare->header
        && !RhhModsHostAPI()->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 6)));
    struct DayCare old = gSaveBlock1Ptr->daycare;
    struct Pokemon savedParty[PARTY_SIZE]; memcpy(savedParty, gParties[B_TRAINER_PLAYER], sizeof(savedParty));
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER];
    for (u32 i = 0; i < 3; ++i) {
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][i], SPECIES_TORCHIC, 5, 1200 + i, OTID_STRUCT_PRESET(123), 31);
        bool8 egg = TRUE; u8 cycles = i == 2 ? 1 : 0;
        SetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_IS_EGG, &egg);
        SetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_FRIENDSHIP, &cycles);
    }
    gPartiesCount[B_TRAINER_PLAYER] = 3;
    struct RhhEggHatchEligibility eligibility = {.struct_size = sizeof(eligibility)},
        tiny = {.struct_size = 1, .status = 99}, unchanged = tiny;
    CHECK(sHatchPolicyDaycare->hatch_eligibility_context(&tiny) && !memcmp(&tiny, &unchanged, sizeof(tiny))
        && sHatchPolicyDaycare->hatch_eligibility(0, &tiny) && !memcmp(&tiny, &unchanged, sizeof(tiny)));
    rng_value_t rng = gRngValue, rng2 = gRng2Value; sHatchPolicyMode = 1; gLastUsedAbility = ABILITY_TORRENT;
    CHECK(!sHatchPolicyDaycare->hatch_eligibility(2, &eligibility)
        && eligibility.status == RHH_EGG_HATCH_NOT_READY && eligibility.cycles_remaining == 1 && !sHatchPolicyCalls);
    CHECK(!sHatchPolicyDaycare->hatch_eligibility(0, &eligibility) && eligibility.status == RHH_EGG_HATCH_DENIED
        && !sHatchPolicyDaycare->hatch_eligibility(1, &eligibility) && eligibility.status == RHH_EGG_HATCH_ALLOWED
        && gLastUsedAbility == ABILITY_TORRENT);
    gSaveBlock1Ptr->daycare.stepCounter = NativeDaycareHatchCycleLength() - 1;
    CHECK(ShouldEggHatch() && gSpecialVar_0x8004 == 1);
    sHatchPolicyMode = 2; gSaveBlock1Ptr->daycare.stepCounter = NativeDaycareHatchCycleLength() - 1;
    CHECK(!ShouldEggHatch());
    sHatchPolicyMode = 3;
    CHECK(!sHatchPolicyDaycare->hatch_eligibility(0, &eligibility) && eligibility.status == RHH_EGG_HATCH_IMPURE);
    gSaveBlock1Ptr->daycare.stepCounter = NativeDaycareHatchCycleLength() - 1;
    CHECK(!ShouldEggHatch() && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    gSaveBlock1Ptr->daycare = old; memcpy(gParties[B_TRAINER_PLAYER], savedParty, sizeof(savedParty));
    gPartiesCount[B_TRAINER_PLAYER] = savedCount; RhhModsUnloadLibraries();
    puts("egg hatch policy: preview, party order, denial and RNG passed");
}
