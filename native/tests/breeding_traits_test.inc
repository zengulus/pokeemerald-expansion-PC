static const struct RhhBreedingService *sTraitBreeding;
static bool32 sTraitBad;
static void TraitTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    u8 mode = *(const u8 *)state;
    if (mode == 1) decision->value = event->kind == RHH_BREED_BALL_RULE ? RhhContentKeyOf("rhh:item/master_ball") : 99;
    if (mode == 2) { (void)Random32(); (void)Random2_32(); }
    if (mode == 5) decision->value = event->kind == RHH_BREED_BALL_RULE ? RhhContentKeyOf("rhh:species/torchic") : 1;
}
static void TraitConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhBreedingTraitContext context = {.struct_size = sizeof(context)}, tiny = {.struct_size = 1}, old = tiny;
    struct RhhBreedingPreview nested = {.struct_size = sizeof(nested), .candidate_count = 99}, before = nested;
    if (sTraitBreeding->trait_context(&context) || !sTraitBreeding->trait_context(&tiny) || memcmp(&tiny, &old, sizeof(tiny))
        || !sTraitBreeding->preview(&nested) || memcmp(&nested, &before, sizeof(nested))
        || context.offspring != event->event_key || context.original_offspring != event->subject
        || context.parent_slots[0] >= 2 || context.parent_slots[1] >= 2
        || !context.parents[0].mon.species || !context.parents[1].mon.species
        || context.compatibility > RHH_DAYCARE_HIGH || context.compatibility != event->args[0]) sTraitBad = TRUE;
    if (*(const u8 *)state == 3) decision->allowed = 0;
    decision->value = context.base; /* Constraints cannot rewrite a transform. */
}
static const struct RhhModHook sTraitHooks[] = {
    {sizeof(struct RhhModHook), RHH_BREED_BALL_RULE, RHH_TRANSFORM, 100, TraitTransform, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_ABILITY_RULE, RHH_TRANSFORM, 100, TraitTransform, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_BALL_RULE, RHH_CONSTRAINT, 0, TraitConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_ABILITY_RULE, RHH_CONSTRAINT, 0, TraitConstraint, NULL}
};
static const struct RhhModDefinition sTraitMod = {.struct_size = sizeof(sTraitMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_trait_policy", .state_size = 1, .hook_count = ARRAY_COUNT(sTraitHooks), .hooks = sTraitHooks};
