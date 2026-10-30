static const struct RhhBreedingService *sRepeatableBreeding;
static bool32 sRepeatablePolicyBad;
static u32 sRepeatablePolicyCalls;
static void RepeatableConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhBreedingRngPolicy context = {.struct_size = sizeof(context)}, tiny = {.struct_size = 1, .mode = 99}, old = tiny;
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingPreview nested = {.struct_size = sizeof(nested), .candidate_count = 99}, before = nested;
    struct RhhBreedingExactPreview exact = {.struct_size = sizeof(exact), .ball = 99}, oldExact = exact;
    if (sRepeatableBreeding->rng_policy_context(&context) || !sRepeatableBreeding->rng_policy_context(&tiny)
        || memcmp(&tiny, &old, sizeof(tiny)) || context.mode != RHH_BREED_RNG_ENGINE
        || !context.id.run_id || !(context.id.high | context.id.low) || !sRepeatableBreeding->offer(&offer)
        || !sRepeatableBreeding->preview(&nested) || memcmp(&nested, &before, sizeof(nested))
        || !sRepeatableBreeding->exact_preview(&offer, &exact) || memcmp(&exact, &oldExact, sizeof(exact))
        || event->event_key != context.parents[0].mon.species || event->subject != context.parents[1].mon.species)
        sRepeatablePolicyBad = TRUE;
    if (*(const u8 *)state == 1) decision->allowed = 0;
    decision->value = RHH_BREED_RNG_ENGINE; /* Constraints cannot rewrite the transform. */
}
static void RepeatableInvalid(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    if (*(const u8 *)state == 2) decision->value = 99;
    if (*(const u8 *)state == 3) { Random32(); Random2_32(); }
    if (*(const u8 *)state == 5 && ++sRepeatablePolicyCalls >= 2) decision->value = RHH_BREED_RNG_ENGINE;
}
static void RepeatableDifferentLiveOffspring(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    struct RhhBreedingContext context = {.struct_size = sizeof(context)};
    if (*(const u8 *)state == 4 && !sRepeatableBreeding->context(&context) && !context.preview)
        decision->value = RhhContentKeyForId(RHH_SPECIES, SPECIES_PIKACHU);
}
static void RepeatableCommitted(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)event; (void)decision; ++((u8 *)state)[1]; }
static const struct RhhModHook sRepeatableHooks[] = {
    {sizeof(struct RhhModHook), RHH_BREED_RNG_POLICY, RHH_TRANSFORM, 100, RepeatableInvalid, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_RNG_POLICY, RHH_CONSTRAINT, 0, RepeatableConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_OFFSPRING, RHH_TRANSFORM, 100, RepeatableDifferentLiveOffspring, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_EGG_CREATED, RHH_OBSERVE, 0, NULL, RepeatableCommitted}
};
static const struct RhhModDefinition sRepeatableConstraint = {
    .struct_size = sizeof(sRepeatableConstraint), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:breeding_rng_constraint", .state_size = 2, .hook_count = ARRAY_COUNT(sRepeatableHooks), .hooks = sRepeatableHooks
};
