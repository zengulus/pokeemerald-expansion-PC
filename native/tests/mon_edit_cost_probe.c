/* Independent consumer of the cost-aware edit ABI. */
#include "mod_api.h"
#include <stdio.h>
static const struct RhhMonEditService *sEdit;
static int Check(const struct RhhModEvent *event, int preview)
{
    struct RhhMonEditResult edit = {.struct_size = sizeof(edit)};
    struct RhhMonEditPayment payment = {.struct_size = sizeof(payment)};
    if (sEdit->context(&edit) || sEdit->payment_context(&payment) || edit.is_preview != (unsigned)preview
        || edit.before.identity != event->subject || payment.money_before < payment.money
        || payment.money_after != payment.money_before - payment.money
        || payment.item_before < payment.item_quantity
        || payment.item_after != payment.item_before - payment.item_quantity
        || (!!payment.item != !!payment.item_quantity) || (!!payment.item != !!payment.bag))
    { puts("EDIT_COST_PROBE_BAD"); return 0; }
    return 1;
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhMonEditPayment payment = {.struct_size = sizeof(payment)};
    if (sEdit->payment_context(&payment)) { puts("EDIT_COST_PROBE_UNPAID"); return; }
    if (Check(event, event->args[1])) puts("EDIT_COST_PROBE_CONSTRAINT");
}
static void Observer(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (Check(event, 0)) puts("EDIT_COST_PROBE_COMMITTED");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_MON, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_MON_EDITED, RHH_OBSERVE, 0, NULL, Observer}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/mon_edit"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:mon_edit_cost",
    .hook_count = sizeof(sHooks) / sizeof(*sHooks), .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sEdit = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 1));
    return sEdit && sEdit->header.struct_size >= RHH_MON_EDIT_SERVICE_V1_1_SIZE ? &sDefinition : NULL;
}
