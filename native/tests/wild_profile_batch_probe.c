#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhWildProfileService *sProfile;
static const struct RhhModHostV1 *sHost;
static RhhContentKey sDenied, sOwner, sB, sBoolean, sExtra;
static unsigned sSchemaMode;

static void Offer(RhhContentKey field)
{
    struct RhhWildProfileSchema recursive = {.struct_size = sizeof(recursive), .kind = 99}, saved = recursive;
    if (sProfile->schema(sOwner, field, &recursive) != RHH_WILD_PROFILE_SCHEMA_INVALID
        || memcmp(&recursive, &saved, sizeof(saved))) puts("PROFILE_BATCH_BAD_SCHEMA_REENTRANT");
    struct RhhWildProfileSchemaList nested = {.struct_size = sizeof(nested), .count = 99};
    if (sProfile->schema_list(sOwner, &nested) != RHH_WILD_PROFILE_SCHEMA_INVALID || nested.count != 99)
        puts("PROFILE_BATCH_BAD_SCHEMA_LIST_REENTRANT");
    struct RhhWildProfileSchema offered = {.struct_size = sizeof(offered), .owner = sOwner, .field = field,
        .kind = field == sBoolean ? RHH_WILD_PROFILE_BOOLEAN : RHH_WILD_PROFILE_SIGNED,
        .minimum = 0, .maximum = field == sBoolean ? 1 : sSchemaMode == 3 ? 10 : 25,
        .default_value = 0, .label = "Alert"};
    if (sProfile->offer_schema(&offered) || sProfile->offer_schema(&offered)) puts("PROFILE_BATCH_BAD_SCHEMA_OFFER");
    if (sSchemaMode == 1)
    { offered.default_value = 1; if (!sProfile->offer_schema(&offered)) puts("PROFILE_BATCH_BAD_SCHEMA_CONFLICT"); }
    else if (sSchemaMode == 2)
    { offered.reserved[0] = 1; if (!sProfile->offer_schema(&offered)) puts("PROFILE_BATCH_BAD_SCHEMA_INVALID"); }
}
static void Schema(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhWildProfileSchema context = {.struct_size = sizeof(context)};
    if (sProfile->schema_context(&context) || context.owner != event->subject || context.field != event->event_key)
    { puts("PROFILE_BATCH_BAD_SCHEMA_CONTEXT"); return; }
    if (context.owner != sOwner) return;
    if (!context.field)
    {
        Offer(sB); Offer(sBoolean);
        if (sSchemaMode == 4) Offer(sExtra); /* Deliberately missing from direct queries. */
        if (sSchemaMode == 5)
            for (unsigned i = 0; i < 32; ++i)
            {
                char name[32]; snprintf(name, sizeof(name), "test:batch/many%u", i);
                struct RhhWildProfileSchema offered = {.struct_size = sizeof(offered), .owner = sOwner,
                    .field = sHost->content_key(name), .kind = RHH_WILD_PROFILE_SIGNED,
                    .minimum = 0, .maximum = 1, .label = "Extra"};
                int result = sProfile->offer_schema(&offered);
                if ((i < 30 && result) || (i >= 30 && !result)) puts("PROFILE_BATCH_BAD_SCHEMA_CAPACITY");
            }
    }
    else if (context.field == sB || context.field == sBoolean) Offer(context.field);
}

static void Check(const struct RhhModEvent *event, struct RhhModDecision *decision, int committed)
{
    struct RhhWildProfileResult one = {.struct_size = sizeof(one)};
    struct RhhWildProfileBatchResult batch = {.struct_size = sizeof(batch)};
    if (sProfile->context(&one)) { puts("PROFILE_BATCH_BAD_CONTEXT"); return; }
    if (sProfile->batch_context(&batch)) return; /* Ordinary v1.0 edit. */
    if (!batch.count || batch.count > RHH_WILD_PROFILE_CAPACITY || one.after.field != event->event_key
        || batch.individual.serial != event->ordinal || one.is_preview != batch.is_preview)
        puts("PROFILE_BATCH_BAD_CONTEXT");
    struct RhhWildProfileBatchResult small = {.struct_size = sizeof(small) - 1, .count = 99}, saved = small;
    if (!sProfile->batch_context(&small) || memcmp(&small, &saved, sizeof(small))) puts("PROFILE_BATCH_BAD_SIZE");
    for (unsigned i = 0; i < batch.count; ++i)
    {
        const struct RhhWildProfileBatchChange *entry = &batch.changes[i];
        const struct RhhWildProfileValue *expected = committed ? &entry->after : &entry->before;
        struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
        if ((i && batch.changes[i - 1].after.field >= entry->after.field)
            || sProfile->at(batch.actor.handle, expected->owner, expected->field, &value)
            || value.present != expected->present || value.value != expected->value || value.revision != expected->revision)
            puts("PROFILE_BATCH_BAD_ATOMIC");
    }
    struct RhhWildProfileBatchRequest nested = {.struct_size = sizeof(nested), .count = 1,
        .actor_handle = batch.actor.handle, .owner = one.after.owner, .expected_revision = one.before.revision,
        .items = {{.field = one.after.field, .present = 1, .value = 99}}};
    struct RhhWildProfileBatchResult nestedResult = {.struct_size = sizeof(nestedResult)};
    if (sProfile->apply_batch(&nested, &nestedResult) != RHH_WILD_PROFILE_UNAVAILABLE) puts("PROFILE_BATCH_BAD_REENTRANT");
    if (!committed && event->event_key == sDenied) { decision->allowed = 0; strcpy(decision->reason, "batch veto"); }
    puts(committed ? "PROFILE_BATCH_COMMITTED" : "PROFILE_BATCH_CONSTRAINT");
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)state; Check(event, decision, 0); }
static void Edited(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)decision; (void)state; Check(event, NULL, 1); }
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (event->args[0] == 98) { sSchemaMode = 1; return; }
    if (event->args[0] == 99) { sSchemaMode = 2; return; }
    if (event->args[0] == 95) { sSchemaMode = 3; return; }
    if (event->args[0] == 94) { sSchemaMode = 4; return; }
    if (event->args[0] == 93) { sSchemaMode = 5; return; }
    if (event->args[0] == 96) { sSchemaMode = 0; return; }
    if (event->args[0] != 97) return;
    struct RhhWildProfileBatchRequest request = {.struct_size = sizeof(request), .count = 1,
        .actor_handle = event->subject, .expected_revision = event->args[1], .owner = sOwner,
        .items = {{.field = sB, .present = 1, .value = 21}}};
    struct RhhWildProfileBatchResult result = {.struct_size = sizeof(result)};
    if (sProfile->apply_batch(&request, &result) || result.changed_count != 1) puts("PROFILE_BATCH_BAD_UPDATE");
    else puts("PROFILE_BATCH_UPDATE_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_WILD_PROFILE, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_PROFILE_EDITED, RHH_OBSERVE, 0, NULL, Edited},
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update},
    {sizeof(struct RhhModHook), RHH_WILD_PROFILE_SCHEMA, RHH_CONSTRAINT, 0, Schema, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_PROFILE_SCHEMA_COLLECT, RHH_CONSTRAINT, 0, Schema, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 3), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_profile"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:wild_profile_batch",
    .hook_count = 5, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sHost = host;
    sProfile = (const void *)host->get_service("rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 3));
    sDenied = host->content_key("test:profile/denied");
    sOwner = host->content_key("test:wild_profile_batch"); sB = host->content_key("test:batch/b");
    sBoolean = host->content_key("test:batch/boolean");
    sExtra = host->content_key("test:batch/unpaired");
    return sProfile && sDenied && sOwner && sB && sBoolean && sExtra
        && sProfile->header.struct_size >= RHH_WILD_PROFILE_SERVICE_V1_3_SIZE ? &definition : NULL;
}
