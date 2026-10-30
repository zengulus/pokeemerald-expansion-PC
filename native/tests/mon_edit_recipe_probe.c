/* Independent v1.2 consumer, including v1.1 receipt compatibility. */
#include "mod_api.h"
#include <stdio.h>
static const struct RhhMonEditService *sEdit;
static int Check(const struct RhhModEvent *event, int preview)
{
    struct RhhMonEditResult edit = {.struct_size = sizeof(edit)};
    struct RhhMonEditRecipePayment receipt = {.struct_size = sizeof(receipt)};
    struct RhhMonEditPayment old = {.struct_size = sizeof(old)};
    if (sEdit->context(&edit) || sEdit->recipe_context(&receipt) || edit.before.identity != event->subject
        || edit.is_preview != (unsigned)preview || receipt.item_count > RHH_MON_EDIT_MAX_COST_ITEMS
        || receipt.money_before < receipt.money || receipt.money_after != receipt.money_before - receipt.money)
    { puts("EDIT_RECIPE_PROBE_BAD"); return 0; }
    for (unsigned i = 0; i < receipt.item_count; ++i)
        if (!receipt.items[i].item || !receipt.items[i].quantity || receipt.items[i].before < receipt.items[i].quantity
            || receipt.items[i].after != receipt.items[i].before - receipt.items[i].quantity)
        { puts("EDIT_RECIPE_PROBE_BAD"); return 0; }
    int oldResult = sEdit->payment_context(&old);
    if ((receipt.item_count > 1 && !oldResult) || (receipt.item_count <= 1 && oldResult))
    { puts("EDIT_RECIPE_PROBE_BAD"); return 0; }
    if (receipt.item_count == 1 && (old.item != receipt.items[0].item || old.item_quantity != receipt.items[0].quantity
        || old.item_before != receipt.items[0].before || old.item_after != receipt.items[0].after))
    { puts("EDIT_RECIPE_PROBE_BAD"); return 0; }
    return 1;
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhMonEditRecipePayment receipt = {.struct_size = sizeof(receipt)};
    if (sEdit->recipe_context(&receipt)) { puts("EDIT_RECIPE_PROBE_UNPAID"); return; }
    if (Check(event, event->args[1])) puts("EDIT_RECIPE_PROBE_CONSTRAINT");
}
static void Observer(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)decision; (void)state; if (Check(event, 0)) puts("EDIT_RECIPE_PROBE_COMMITTED"); }
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_MON, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_MON_EDITED, RHH_OBSERVE, 0, NULL, Observer}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/mon_edit"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:mon_edit_recipe",
    .hook_count = sizeof(sHooks) / sizeof(*sHooks), .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sEdit = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 2));
    return sEdit && sEdit->header.struct_size >= RHH_MON_EDIT_SERVICE_V1_2_SIZE ? &sDefinition : NULL;
}
