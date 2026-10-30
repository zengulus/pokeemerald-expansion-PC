/* Independent public-API consumer checks the committed actor event boundary. */
#include "mod_api.h"
#include <stdio.h>

static const struct RhhWildActorService *sActors;
static const struct RhhWildDormantService *sDormant;
static const struct RhhWildProfileService *sProfiles;
static RhhContentKey sProfileField, sProfileOwner, sWorkField;
static void ClockObserved(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildDormantClockChange change = {.struct_size = sizeof(change)};
    struct RhhWildDormantClock clock = {.struct_size = sizeof(clock)};
    if (sDormant->clock_context(&change) || !change.changed || change.after.processed_step <= change.before.processed_step
        || change.after.current_step != change.before.current_step
        || change.after.pending_steps != change.after.current_step - change.after.processed_step
        || sDormant->clock(&change.after.actor.individual, &clock) || clock.processed_step != change.after.processed_step
        || event->ordinal != change.after.actor.individual.serial || event->event_key != change.after.actor.map)
    { puts("DORMANT_PROBE_BAD"); return; }
    struct RhhWildDormantClockRequest nested = {.struct_size = sizeof(nested), .individual = clock.actor.individual,
        .owner = clock.actor.owner, .expected_step = clock.processed_step, .through_step = clock.processed_step};
    struct RhhWildDormantClockChange rejected = {.struct_size = sizeof(rejected)};
    if (sDormant->clock_advance(&nested, &rejected) != RHH_WILD_DORMANT_CLOCK_UNAVAILABLE)
    { puts("DORMANT_PROBE_BAD"); return; }
    struct RhhWildDormantWorkResult work = {.struct_size = sizeof(work)};
    if (!sDormant->work_context(&work))
    {
        if (clock.processed_step != work.clock.after.processed_step || clock.actor.map != work.relocation.after.map)
        { puts("DORMANT_PROBE_BAD"); return; }
        for (unsigned i = 0; i < work.profile.count; ++i)
        {
            const struct RhhWildProfileValue *after = &work.profile.changes[i].after;
            struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
            if (sDormant->profile_at(&clock.actor.individual, after->owner, after->field, &value)
                || value.present != after->present || value.value != after->value || value.revision != after->revision)
            { puts("DORMANT_PROBE_BAD"); return; }
        }
    }
    puts("DORMANT_CLOCK_COMMITTED");
}
static void LegacyConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; (void)decision; }
static void LegacyObserved(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)event; (void)decision; (void)state; }
static void Schema(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhWildProfileSchema request = {.struct_size = sizeof(request)};
    if (sProfiles->schema_context(&request)) { puts("DORMANT_PROBE_BAD"); return; }
    if (request.owner == sProfileOwner && request.field == sProfileField)
    {
        struct RhhWildProfileSchema schema = {.struct_size = sizeof(schema), .kind = RHH_WILD_PROFILE_SIGNED,
            .owner = sProfileOwner, .field = sProfileField, .minimum = -5000, .maximum = 5000,
            .default_value = 0, .label = "Sample"};
        if (sProfiles->offer_schema(&schema)) puts("DORMANT_PROBE_BAD");
    }
}
static void ProfileConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhWildDormantProfileResult context = {.struct_size = sizeof(context)};
    struct RhhWildProfileValue current = {.struct_size = sizeof(current)};
    if (sDormant->profile_context(&context)
        || sDormant->profile_at(&context.actor.individual, context.before.owner, context.before.field, &current)
        || current.revision != context.before.revision || current.value != context.before.value)
    { puts("DORMANT_PROBE_BAD"); return; }
    struct RhhWildDormantProfileBatchResult batch = {.struct_size = sizeof(batch)};
    if (!sDormant->profile_batch_context(&batch))
        for (unsigned i = 0; i < batch.count; ++i)
        {
            const struct RhhWildProfileValue *before = &batch.changes[i].before;
            current.struct_size = sizeof(current);
            if (sDormant->profile_at(&batch.actor.individual, before->owner, before->field, &current)
                || current.present != before->present || current.value != before->value || current.revision != before->revision)
            { puts("DORMANT_PROBE_BAD"); return; }
        }
    if (context.after.value == 777) decision->allowed = 0;
}
static void ProfileObserved(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhWildDormantProfileResult context = {.struct_size = sizeof(context)};
    struct RhhWildProfileValue current = {.struct_size = sizeof(current)};
    if (sDormant->profile_context(&context) || sDormant->profile_at(&context.actor.individual,
        context.after.owner, context.after.field, &current) || current.revision != context.after.revision
        || current.value != context.after.value) { puts("DORMANT_PROBE_BAD"); return; }
    struct RhhWildDormantWorkResult work = {.struct_size = sizeof(work)};
    if (!sDormant->work_context(&work))
    {
        struct RhhWildDormantClock clock = {.struct_size = sizeof(clock)};
        if (sDormant->clock(&context.actor.individual, &clock)
            || clock.processed_step != work.clock.after.processed_step || clock.actor.map != work.relocation.after.map
            || clock.actor.x != work.relocation.after.x || clock.actor.y != work.relocation.after.y)
        { puts("DORMANT_PROBE_BAD"); return; }
    }
    struct RhhWildDormantProfileBatchResult batch = {.struct_size = sizeof(batch)};
    if (!sDormant->profile_batch_context(&batch))
    {
        for (unsigned i = 0; i < batch.count; ++i)
        {
            const struct RhhWildProfileValue *after = &batch.changes[i].after;
            current.struct_size = sizeof(current);
            if (sDormant->profile_at(&batch.actor.individual, after->owner, after->field, &current)
                || current.present != after->present || current.value != after->value || current.revision != after->revision)
            { puts("DORMANT_PROBE_BAD"); return; }
        }
        puts("DORMANT_PROFILE_BATCH_COMMITTED");
    }
    puts("DORMANT_PROFILE_COMMITTED");
}
static void Relocated(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildDormantRelocation change = {.struct_size = sizeof(change)};
    struct RhhWildDormantActor current = {.struct_size = sizeof(current)};
    struct RhhWildDormantStepPreview step = {.struct_size = sizeof(step)};
    struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
    if (sDormant->relocation_context(&change) || !change.changed || sDormant->find(&change.after.individual, &current)
        || current.map != change.after.map || current.x != change.after.x || current.y != change.after.y
        || event->event_key != current.map || event->ordinal != current.individual.serial
        || sDormant->step_preview(&current.individual, RHH_WILD_ACTOR_NORTH, &step)
        || sDormant->profile_at(&current.individual, current.owner, sProfileField, &value)
        || !value.present || value.value != 1234)
    { puts("DORMANT_PROBE_BAD"); return; }
    struct RhhWildDormantWorkResult work = {.struct_size = sizeof(work)};
    if (!sDormant->work_context(&work))
    {
        struct RhhWildDormantClock clock = {.struct_size = sizeof(clock)};
        struct RhhWildProfileValue field = {.struct_size = sizeof(field)};
        if (sDormant->clock(&current.individual, &clock) || clock.processed_step != work.clock.after.processed_step
            || sDormant->profile_at(&current.individual, current.owner, sWorkField, &field)
            || !field.present || field.value != 12) { puts("DORMANT_PROBE_BAD"); return; }
        puts("DORMANT_WORK_ATOMIC");
    }
    puts("DORMANT_PROBE_RELOCATE");
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildActorEvent context = {.struct_size = sizeof(context)};
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id)};
    if (sActors->context(&context)) { puts("DORMANT_PROBE_BAD"); return; }
    if (context.cause != RHH_WILD_ACTOR_SUSPENDED && context.cause != RHH_WILD_ACTOR_RESUMED) return;
    struct RhhWildDormantActor record = {.struct_size = sizeof(record)};
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    if (sActors->event_individual(&id) || !id.run_id || !id.serial || event->ordinal != context.actor.handle)
    { puts("DORMANT_PROBE_BAD"); return; }
    if (context.cause == RHH_WILD_ACTOR_SUSPENDED)
    {
        if (event->kind != RHH_WILD_ACTOR_REMOVED || sDormant->find(&id, &record)
            || !sActors->find_individual(&id, &actor)
            || sDormant->resume(&id, record.owner, &actor) != RHH_WILD_ACTOR_UNAVAILABLE)
        { puts("DORMANT_PROBE_BAD"); return; }
        puts("DORMANT_PROBE_SUSPEND");
    }
    else
    {
        if (event->kind != RHH_WILD_ACTOR_SPAWNED || !sDormant->find(&id, &record)
            || sActors->find_individual(&id, &actor) || actor.handle != context.actor.handle)
        { puts("DORMANT_PROBE_BAD"); return; }
        puts("DORMANT_PROBE_RESUME");
    }
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_SPAWNED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_REMOVED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_WILD_DORMANT_RELOCATED, RHH_OBSERVE, 0, NULL, Relocated},
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_WILD_PROFILE, RHH_CONSTRAINT, 0, LegacyConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_PROFILE_EDITED, RHH_OBSERVE, 0, NULL, LegacyObserved},
    {sizeof(struct RhhModHook), RHH_WILD_PROFILE_SCHEMA, RHH_CONSTRAINT, 0, Schema, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_DORMANT_WILD_PROFILE, RHH_CONSTRAINT, 0, ProfileConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_DORMANT_WILD_PROFILE_EDITED, RHH_OBSERVE, 0, NULL, ProfileObserved},
    {sizeof(struct RhhModHook), RHH_WILD_DORMANT_CLOCK_ADVANCED, RHH_OBSERVE, 0, NULL, ClockObserved}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 4), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_actor"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 7), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_dormant"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_profile"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:wild_dormant_probe",
    .hooks = sHooks, .hook_count = 9, .service_requirements = sRequirements, .service_requirement_count = 3
};
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service || !host->content_key) return NULL;
    sProfileField = host->content_key("test:profile/dormant_save");
    sProfileOwner = host->content_key("test:mon_edit_rules");
    sWorkField = host->content_key("test:profile/work_state");
    sActors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    sDormant = (const void *)host->get_service("rhh:service/wild_dormant", RHH_SERVICE_VERSION(1, 7));
    sProfiles = (const void *)host->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 2));
    return sActors && sActors->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_4_SIZE
        && sDormant && sProfileField && sProfileOwner && sWorkField && sProfiles
        && sDormant->header.struct_size >= RHH_WILD_DORMANT_SERVICE_V1_7_SIZE ? &sDefinition : NULL;
}
