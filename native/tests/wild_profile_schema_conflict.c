#include "mod_api.h"
#ifdef PROFILE_SCHEMA_LEGACY
#define PROFILE_API_VERSION RHH_SERVICE_VERSION(1, 2)
#else
#define PROFILE_API_VERSION RHH_SERVICE_VERSION(1, 3)
#endif

static const struct RhhWildProfileService *sProfile;
static RhhContentKey sOwner, sField;
static void Schema(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->subject != sOwner || (event->event_key && event->event_key != sField)) return;
    struct RhhWildProfileSchema schema = {.struct_size = sizeof(schema), .owner = sOwner, .field = sField,
        .kind = RHH_WILD_PROFILE_SIGNED, .minimum = 0,
#ifdef PROFILE_SCHEMA_IDENTICAL
        .maximum = 25,
#else
        .maximum = 24,
#endif
        .default_value = 0, .label = "Alert"};
    sProfile->offer_schema(&schema);
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_PROFILE_SCHEMA, RHH_CONSTRAINT, 0, Schema, NULL}
#ifndef PROFILE_SCHEMA_LEGACY
    ,
    {sizeof(struct RhhModHook), RHH_WILD_PROFILE_SCHEMA_COLLECT, RHH_CONSTRAINT, 0, Schema, NULL}
#endif
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), PROFILE_API_VERSION, RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_profile"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:zzz_conflicting_profile_schema",
    .hook_count = sizeof(hooks) / sizeof(hooks[0]), .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sProfile = (const void *)host->get_service("rhh:service/wild_profile", PROFILE_API_VERSION);
    sOwner = host->content_key("test:wild_profile_batch");
#ifdef PROFILE_SCHEMA_LEGACY
    sField = host->content_key("test:batch/legacy_only");
#else
    sField = host->content_key("test:batch/b");
#endif
    return sProfile && sOwner && sField ? &definition : NULL;
}
