/* Frozen v1.0 service layout: an older independently built module. */
#include "mod_api.h"
#include <stdio.h>
struct OldMonEditService {
    struct RhhServiceHeader header;
    int (*preview)(const struct RhhMonEditRequest *, struct RhhMonEditResult *);
    int (*commit)(const struct RhhMonEditRequest *, struct RhhMonEditResult *);
    int (*context)(struct RhhMonEditResult *);
};
static const struct OldMonEditService *sEdit;
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhMonEditResult result = {.struct_size = sizeof(result)};
    if (sEdit->context(&result) || result.before.identity != event->subject || result.fields != event->args[0])
        puts("EDIT_V1_PROBE_BAD");
    else puts("EDIT_V1_PROBE_CONSTRAINT");
}
static void Observer(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhMonEditResult result = {.struct_size = sizeof(result)};
    if (sEdit->context(&result) || result.is_preview || result.before.identity != event->subject)
        puts("EDIT_V1_PROBE_BAD");
    else puts("EDIT_V1_PROBE_COMMITTED");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_MON, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_MON_EDITED, RHH_OBSERVE, 0, NULL, Observer}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/mon_edit"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:mon_edit_v1",
    .hook_count = sizeof(sHooks) / sizeof(*sHooks), .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sEdit = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 0));
    return sEdit && sEdit->header.struct_size >= sizeof(*sEdit) && sEdit->preview && sEdit->commit && sEdit->context
        ? &sDefinition : NULL;
}
