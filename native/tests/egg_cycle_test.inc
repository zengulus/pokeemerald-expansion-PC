static const struct RhhDaycareService *sCycleDaycare;
static u32 sCycleMode, sCycleCalls, sCycleNotifications;
static void CycleTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhEggCycleProgress context = {.struct_size = sizeof(context)};
    struct RhhEggCycleProgress nested = {.struct_size = sizeof(nested), .decrement = 99}, saved = nested;
    CHECK(!sCycleDaycare->cycle_progress_context(&context) && context.egg.slot == event->args[0]
        && context.cycles_remaining == event->args[1] && context.engine_decrement == event->args[2]
        && context.remaining_after == context.cycles_remaining - context.engine_decrement
        && context.engine_decrement == decision->value && context.egg.species == event->event_key
        && context.egg.identity == event->subject && !RhhModsCommandsAllowed()
        && sCycleDaycare->cycle_progress(context.egg.slot, &nested) && !memcmp(&nested, &saved, sizeof(nested)));
    ++sCycleCalls;
    (void)GetMonAbility(&gParties[B_TRAINER_PLAYER][2]);
    if (sCycleMode == 1) decision->allowed = 0;
    else if (sCycleMode == 2) decision->value = context.cycles_remaining + 1;
    else if (sCycleMode == 3) { decision->value = 0; (void)Random32(); (void)Random2_32(); }
    else if (sCycleMode == 4) decision->value = 0;
    else decision->value += context.egg.slot ? 1 : 3;
}
static void CycleConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; if (sCycleMode == 5) decision->value = 0; }
static void CycleChanged(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhEggCycleChange context = {.struct_size = sizeof(context)};
    struct RhhEggCycleChange tiny = {.struct_size = 1, .sequence = 99}, saved = tiny;
    CHECK(!sCycleDaycare->cycle_change_context(&context)
        && sCycleDaycare->cycle_change_context(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny))
        && context.sequence == event->ordinal && context.sequence == sCycleNotifications
        && context.progress.egg.slot == event->args[0] && context.progress.cycles_remaining == event->args[1]
        && context.progress.remaining_after == event->args[2] && context.progress.engine_decrement == event->args[3]
        && context.progress.egg.species == event->event_key && context.progress.egg.identity == event->subject
        && context.progress.remaining_after + context.progress.decrement == context.progress.cycles_remaining
        && GetMonData(&gParties[B_TRAINER_PLAYER][event->args[0]], MON_DATA_FRIENDSHIP) == event->args[2]
        && !RhhModsCommandsAllowed());
    if (sCycleMode == 4) CHECK(!context.progress.decrement);
    ++sCycleNotifications; (void)Random32(); (void)Random2_32();
}
static const struct RhhModHook sCycleHooks[] = {
    {sizeof(struct RhhModHook), RHH_EGG_CYCLE_PROGRESS, RHH_TRANSFORM, 0, CycleTransform, NULL},
    {sizeof(struct RhhModHook), RHH_EGG_CYCLE_PROGRESS, RHH_CONSTRAINT, 0, CycleConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_EGG_CYCLE_TICKED, RHH_OBSERVE, 0, NULL, CycleChanged}
};
static const struct RhhModDefinition sCycleMod = {.struct_size = sizeof(sCycleMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:egg_cycle",
    .hook_count = ARRAY_COUNT(sCycleHooks), .hooks = sCycleHooks};
static const struct RhhModDefinition sCycleObserverMod = {.struct_size = sizeof(sCycleObserverMod),
    .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:egg_cycle_observer",
    .hook_count = 1, .hooks = &sCycleHooks[2]};
static void CheckEggCycleObserverOnly(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sCycleObserverMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sCycleDaycare = (const void *)RhhModsHostAPI()->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 5));
    CHECK(sCycleDaycare && !RhhModsHasHook(RHH_EGG_CYCLE_PROGRESS));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 2300, OTID_STRUCT_PRESET(123), 31);
    bool8 egg = TRUE; u8 cycles = 5;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &egg);
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_FRIENDSHIP, &cycles);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    gSaveBlock1Ptr->daycare.stepCounter = NativeDaycareHatchCycleLength() - 1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(!ShouldEggHatch() && sCycleNotifications == 1
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_FRIENDSHIP) == 4
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    RhhModsUnloadLibraries(); puts("egg cycle observer: unmodified engine updates passed");
}
static void CheckEggCyclePolicy(void)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sCycleMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    sCycleDaycare = (const void *)RhhModsHostAPI()->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 5));
    CHECK(sCycleDaycare && sCycleDaycare->header.struct_size >= RHH_DAYCARE_SERVICE_V1_5_SIZE
        && RHH_DAYCARE_SERVICE_V1_3_SIZE < RHH_DAYCARE_SERVICE_V1_4_SIZE
        && RHH_DAYCARE_SERVICE_V1_4_SIZE < RHH_DAYCARE_SERVICE_V1_5_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 3)) == &sCycleDaycare->header
        && !RhhModsHostAPI()->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 6)));
    struct DayCare savedDaycare = gSaveBlock1Ptr->daycare;
    struct Pokemon savedParty[PARTY_SIZE]; memcpy(savedParty, gParties[B_TRAINER_PLAYER], sizeof(savedParty));
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER];
    for (u32 i = 0; i < 3; ++i) {
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][i], SPECIES_TORCHIC, 5, 2300 + i, OTID_STRUCT_PRESET(123), 31);
        if (i < 2) {
            bool8 egg = TRUE; u8 cycles = i ? 3 : 8;
            SetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_IS_EGG, &egg);
            SetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_FRIENDSHIP, &cycles);
        }
    }
    gPartiesCount[B_TRAINER_PLAYER] = 3;
    struct RhhEggCycleProgress result = {.struct_size = sizeof(result)},
        tiny = {.struct_size = 1, .decrement = 99}, unchanged = tiny;
    CHECK(sCycleDaycare->cycle_progress_context(&tiny) && !memcmp(&tiny, &unchanged, sizeof(tiny))
        && sCycleDaycare->cycle_progress(0, &tiny) && !memcmp(&tiny, &unchanged, sizeof(tiny)));
    rng_value_t rng = gRngValue, rng2 = gRng2Value; gLastUsedAbility = ABILITY_TORRENT;
    CHECK(!sCycleDaycare->cycle_progress(0, &result) && result.cycles_remaining == 8
        && result.engine_decrement == 1 && result.decrement == 4 && result.remaining_after == 4
        && gLastUsedAbility == ABILITY_TORRENT && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    CHECK(!sCycleDaycare->cycle_progress(1, &result) && result.decrement == 2 && result.remaining_after == 1
        && !sCycleNotifications);
    gSaveBlock1Ptr->daycare.stepCounter = NativeDaycareHatchCycleLength() - 1;
    struct RhhDaycareEggProgress estimate = {.struct_size = sizeof(estimate)};
    CHECK(!sCycleDaycare->egg_progress(0, &estimate) && estimate.hatch_checks == 2
        && estimate.calls_lower_bound == 1 + estimate.hatch_cycle_length);
    CHECK(!ShouldEggHatch() && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_FRIENDSHIP) == 4
        && GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_FRIENDSHIP) == 1 && sCycleNotifications == 2);
    struct ModsEggCycleCapture stale;
    ModsCaptureEggCycle(0, 4, 1, 3, &stale); CHECK(stale.valid);
    u8 unexpected = 2; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_FRIENDSHIP, &unexpected);
    ModsEggCycleCommitted(&stale); CHECK(sCycleNotifications == 2);
    unexpected = 4; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_FRIENDSHIP, &unexpected);
    for (u32 mode = 1; mode <= 3; ++mode) {
        sCycleMode = mode;
        CHECK(!sCycleDaycare->cycle_progress(0, &result) && result.engine_decrement == 1
            && result.decrement == 1 && result.remaining_after == 3
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    }
    sCycleMode = 4; CHECK(!sCycleDaycare->cycle_progress(0, &result) && !result.decrement && result.remaining_after == 4);
    gSaveBlock1Ptr->daycare.stepCounter = NativeDaycareHatchCycleLength() - 1;
    CHECK(!ShouldEggHatch() && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_FRIENDSHIP) == 4
        && sCycleNotifications == 4 && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sCycleMode = 5; CHECK(!sCycleDaycare->cycle_progress(0, &result) && result.decrement == 4 && !result.remaining_after);
    gSaveBlock1Ptr->daycare = savedDaycare; memcpy(gParties[B_TRAINER_PLAYER], savedParty, sizeof(savedParty));
    gPartiesCount[B_TRAINER_PLAYER] = savedCount; RhhModsUnloadLibraries();
    puts("egg cycle policy: preview, live, fallback and RNG passed");
}
