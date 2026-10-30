#include "mod_api.h"
#include <stdio.h>
static const struct RhhMonEditService *service;
static void Check(const struct RhhModEvent *event, int preview)
{
    if (!(event->args[0] & RHH_EDIT_TERA)) return;
    struct RhhTeraEditContext context = {.struct_size = sizeof(context)};
    struct RhhMonEditResult edit = {.struct_size = sizeof(edit)};
    if (service->tera_context(&context) || service->context(&edit) || context.is_preview != (unsigned)preview
        || edit.before.identity != context.before.mon.identity || edit.after.identity != context.after.mon.identity
        || context.before.stored_ordinal == context.after.stored_ordinal)
        puts("TERA_EDIT_EXTERNAL_BAD");
    else puts(preview ? "TERA_EDIT_EXTERNAL_PREVIEW" : "TERA_EDIT_EXTERNAL_COMMIT");
}
static void Constraint(const struct RhhModEvent *e, const void *state, struct RhhModDecision *d)
{ (void)state; (void)d; Check(e, e->args[1]); }
static void Observe(const struct RhhModEvent *e, const struct RhhModDecision *d, void *state)
{ (void)d; (void)state; Check(e, 0); }
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_MON, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_MON_EDITED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement req[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 7), RHH_SERVICE_REQUIRED, 0, "rhh:service/mon_edit"}
};
static const struct RhhModDefinition def = {.struct_size = sizeof(def), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:tera_edit_probe", .hooks = hooks, .hook_count = 2, .service_requirements = req, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    service = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 7));
    return service && service->header.struct_size >= RHH_MON_EDIT_SERVICE_V1_7_SIZE ? &def : NULL;
}
