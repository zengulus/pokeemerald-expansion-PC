/* Independent module exercises edit constraints and the committed context. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhWildTraitEditsService *sEdits;
static const struct RhhWildProfileService *sProfile;
static const struct RhhWildBehaviorService *sBehavior;
static const struct RhhModHostV1 *sHost;
static RhhContentKey sDenied, sWary;
static RhhContentKey sOwner, sActivityA, sActivityB, sActivityLow;
static RhhContentKey sProfileDenied;
static uint64_t sCollectStep, sExpected;
static uint64_t sFirstChoice, sSelectedFirst;
static unsigned sCollected;
static uint64_t sLastProposed, sLastReported;
static uint64_t sBehaviorStep;
static unsigned sProposed;
static const struct RhhModEcologyTag sTags[] = {
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/wary", "Wary", "Wary actors."},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/deny", "Denied", "Editing is denied."},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/t0", "Trait 0", ""},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/t1", "Trait 1", ""},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/t2", "Trait 2", ""},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/t3", "Trait 3", ""},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/t4", "Trait 4", ""},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/t5", "Trait 5", ""},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/t6", "Trait 6", ""},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/t7", "Trait 7", ""}
};
static struct RhhModSpeciesTag sAssociation = {sizeof(sAssociation), 0, 0, 0};
static int Context(const struct RhhModEvent *event, struct RhhWildTraitEditResult *context)
{
    struct RhhWildTraitEditRequest nested = {.struct_size = sizeof(nested), .value = RHH_WILD_TRAIT_ON};
    struct RhhWildTraitEditResult result = {.struct_size = sizeof(result)};
    if (sEdits->context(context) || context->after.tag != event->event_key
        || context->individual.serial != event->ordinal) return 1;
    nested.actor_handle = context->actor.handle; nested.owner = context->after.owner ? context->after.owner : context->before.owner;
    nested.tag = context->after.tag; nested.expected_revision = context->after.revision;
    return sEdits->apply(&nested, &result) != RHH_WILD_TRAIT_EDIT_UNAVAILABLE;
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; struct RhhWildTraitEditResult context = {.struct_size = sizeof(context)};
    if (Context(event, &context)) { puts("WILD_TRAIT_EDIT_BAD"); return; }
    if (event->event_key == sDenied) { decision->allowed = 0; strcpy(decision->reason, "trait denied"); }
}
static void InvalidTrait(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sWary) decision->value = 42;
}
static int ProfileContext(const struct RhhModEvent *event, struct RhhWildProfileResult *context)
{
    struct RhhWildProfileRequest nested = {.struct_size = sizeof(nested), .present = 1};
    struct RhhWildProfileResult result = {.struct_size = sizeof(result)};
    if (sProfile->context(context) || context->after.field != event->event_key
        || context->individual.serial != event->ordinal) return 1;
    nested.actor_handle = context->actor.handle; nested.owner = context->after.owner;
    nested.field = context->after.field; nested.expected_revision = context->after.revision;
    return sProfile->apply(&nested, &result) != RHH_WILD_PROFILE_UNAVAILABLE;
}
static void ProfileConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; struct RhhWildProfileResult context = {.struct_size = sizeof(context)};
    if (ProfileContext(event, &context)) { puts("WILD_PROFILE_BAD"); return; }
    if (event->event_key == sProfileDenied) { decision->allowed = 0; strcpy(decision->reason, "profile denied"); }
}
static void ProfileEdited(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state; struct RhhWildProfileResult context = {.struct_size = sizeof(context)};
    if (ProfileContext(event, &context) || !context.changed || context.is_preview) puts("WILD_PROFILE_BAD");
    else puts("WILD_PROFILE_COMMITTED");
}
static void Behavior(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhWildBehaviorContext context = {.struct_size = sizeof(context)};
    struct RhhWildActivitySelection selection = {.struct_size = sizeof(selection)};
    struct RhhWildActivitySelection small = {.struct_size = sizeof(small) - 1, .valid = 99}, old = small;
    if (sBehavior->context(&context) || context.phase != RHH_WILD_BEHAVIOR_PROPOSAL || !context.individual.serial
        || sBehavior->selected(&selection) || (selection.valid ? selection.activity.action : 0) != sExpected
        || !sBehavior->selected(&small) || memcmp(&small, &old, sizeof(small))
        || decision->value != sExpected || !sBehavior->offer(NULL))
    { puts("WILD_TRAIT_EDIT_BAD"); return; }
    if (context.step.step != sBehaviorStep)
    { sBehaviorStep = context.step.step; sProposed = 0; sLastProposed = sLastReported = 0; }
    if (context.individual.serial <= sLastProposed) { puts("WILD_TRAIT_EDIT_BAD"); return; }
    sLastProposed = context.individual.serial; ++sProposed;
    if (context.step.step <= 2) decision->value = RHH_WILD_BEHAVIOR_WEST;
    if (context.step.step == 6) decision->allowed = 0;
}
static void Activity(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildBehaviorContext context = {.struct_size = sizeof(context)};
    if (sBehavior->context(&context) || context.phase != RHH_WILD_BEHAVIOR_PROPOSAL || !context.individual.serial)
    { puts("WILD_TRAIT_EDIT_BAD"); return; }
    struct RhhWildProfileValue profile = {.struct_size = sizeof(profile)};
    if (sProfile->at(context.actor.handle, sOwner, sHost->content_key("test:profile/wariness"), &profile)
        || (context.actor.slot == 0 ? (!profile.present || profile.value != -27) : profile.present))
    { puts("WILD_PROFILE_BAD"); return; }
    if (sCollectStep != context.step.step) { sCollectStep = context.step.step; sCollected = 0; }
    unsigned index = sCollected++;
    struct RhhWildActivity a = {.struct_size = sizeof(a), .action = RHH_WILD_BEHAVIOR_WEST,
        .weight = 3, .interval = 8, .priority = 1, .id = sActivityA, .owner = sOwner};
    struct RhhWildActivity b = {.struct_size = sizeof(b), .action = RHH_WILD_BEHAVIOR_EAST,
        .weight = 5, .interval = 16, .priority = 1, .id = sActivityB, .owner = sOwner};
    struct RhhWildActivity low = {.struct_size = sizeof(low), .action = RHH_WILD_BEHAVIOR_ENGAGE,
        .weight = 1000, .interval = 1, .priority = 0, .id = sActivityLow, .owner = sOwner};
    if (context.step.step == 4 || context.step.step == 7) a.action = b.action = RHH_WILD_BEHAVIOR_NONE;
    if (sBehavior->offer(&b) || sBehavior->offer(&low) || sBehavior->offer(&a))
    { puts("WILD_TRAIT_EDIT_BAD"); return; }
    uint64_t identity = context.individual.serial ^ (context.individual.run_id * UINT64_C(0x9e3779b97f4a7c15));
    uint64_t choice = sHost->random_at(event->run_seed, "rhh:wild_activity", identity,
                                       context.step.step / 8) % 8;
    sExpected = sActivityA < sActivityB ? (choice < 3 ? a.action : b.action)
                                          : (choice < 5 ? b.action : a.action);
    if (index == 0 && context.step.step == 1) sFirstChoice = sExpected;
    if (index == 0 && context.step.step == 2 && sExpected != sFirstChoice) puts("WILD_TRAIT_EDIT_BAD");
    if (index == 0 && (context.step.step == 3 || context.step.step >= 5)) sSelectedFirst = sExpected;
    if (index == 1)
    {
        if (!sBehavior->offer(&a)) puts("WILD_TRAIT_EDIT_BAD");
        sExpected = RHH_WILD_BEHAVIOR_NONE;
    }
    else if (index == 2) { decision->allowed = 0; sExpected = RHH_WILD_BEHAVIOR_NONE; }
    else if (index != 0) puts("WILD_TRAIT_EDIT_BAD");
    puts("WILD_ACTIVITY_COLLECTED");
}
static void BehaviorResult(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhWildBehaviorContext context = {.struct_size = sizeof(context)};
    struct RhhWildActivitySelection selection = {.struct_size = sizeof(selection)};
    if (sBehavior->context(&context)) { puts("WILD_TRAIT_EDIT_BAD"); return; }
    if (sBehavior->selected(&selection) || (selection.valid && selection.interval != 8))
    { puts("WILD_TRAIT_EDIT_BAD"); return; }
    if (context.step.step == 3 || context.step.step >= 5)
    {
        if (context.phase != RHH_WILD_BEHAVIOR_REPORT || context.action != sSelectedFirst
            || context.status != (context.step.step == 6 ? RHH_WILD_ACTOR_DENIED : RHH_WILD_ACTOR_UNAVAILABLE)
            || sProposed != 3) puts("WILD_TRAIT_EDIT_BAD");
        else puts(context.step.step == 3 ? "WILD_ACTIVITY_EXECUTED_OK" : "WILD_ACTIVITY_REJECTED_OK");
        return;
    }
    if (context.phase != RHH_WILD_BEHAVIOR_REPORT
        || context.action != RHH_WILD_BEHAVIOR_WEST || context.status != RHH_WILD_ACTOR_UNAVAILABLE
        || sProposed != 3 || context.individual.serial <= sLastReported)
    { puts("WILD_TRAIT_EDIT_BAD"); return; }
    sLastReported = context.individual.serial;
    puts(context.step.step == 1 ? "WILD_BEHAVIOR_SCRIPT_PRIORITY_OK" : "WILD_BEHAVIOR_SAVE_PRIORITY_OK");
}
static void Changed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state; struct RhhWildTraitEditResult context = {.struct_size = sizeof(context)};
    if (Context(event, &context) || context.is_preview || context.after.revision != context.before.revision + 1)
    { puts("WILD_TRAIT_EDIT_BAD"); return; }
    puts("WILD_TRAIT_EDIT_COMMITTED");
}
static void ActivityChanged(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildActivityChange change = {.struct_size = sizeof(change)};
    struct RhhWildActivityState current = {.struct_size = sizeof(current)};
    if (sBehavior->change_context(&change) || sBehavior->at(change.actor.handle, &current)
        || change.before.valid || !change.after.valid || change.after.id != event->event_key
        || change.after.action != RHH_WILD_BEHAVIOR_NONE || change.after.since_step != 4
        || change.after.valid_until_step != 8 || current.id != change.after.id || current.owner != sOwner)
        puts("WILD_TRAIT_EDIT_BAD");
    else puts("WILD_ACTIVITY_CHANGED_OK");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_WILD_TRAIT, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_TRAIT_EDITED, RHH_OBSERVE, 0, NULL, Changed},
    {sizeof(struct RhhModHook), RHH_WILD_TRAIT_QUERY, RHH_TRANSFORM, 0, InvalidTrait, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_ACTIVITY_COLLECT, RHH_CONSTRAINT, 0, Activity, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_BEHAVIOR_PROPOSE, RHH_TRANSFORM, 0, Behavior, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_BEHAVIOR_RESULT, RHH_OBSERVE, 0, NULL, BehaviorResult},
    {sizeof(struct RhhModHook), RHH_WILD_ACTIVITY_CHANGED, RHH_OBSERVE, 0, NULL, ActivityChanged},
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_WILD_PROFILE, RHH_CONSTRAINT, 0, ProfileConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_PROFILE_EDITED, RHH_OBSERVE, 0, NULL, ProfileEdited}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_trait_edits"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_behavior"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_profile"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_ECOLOGY, .id = "test:trait_edit",
    .hook_count = 9, .hooks = sHooks, .service_requirement_count = 3, .service_requirements = sRequirements,
    .ecology_tag_count = sizeof(sTags) / sizeof(sTags[0]), .ecology_tags = sTags,
    .species_tag_count = 1, .species_tags = &sAssociation
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key || !host->random_at) return NULL;
    sHost = host; sOwner = host->content_key(sDefinition.id);
    sActivityA = host->content_key("test:activity/a"); sActivityB = host->content_key("test:activity/b");
    sActivityLow = host->content_key("test:activity/low");
    sDenied = host->content_key("test:trait/deny");
    sProfileDenied = host->content_key("test:profile/deny");
    sAssociation.species = host->content_key("rhh:species/poochyena");
    sWary = sAssociation.tag = host->content_key("test:trait/wary");
    sEdits = (const void *)host->get_service("rhh:service/wild_trait_edits", RHH_SERVICE_VERSION(1, 0));
    sBehavior = (const void *)host->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 2));
    sProfile = (const void *)host->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 0));
    return sEdits && sBehavior && sProfile ? &sDefinition : NULL;
}
