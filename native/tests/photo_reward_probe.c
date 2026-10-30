#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhPhotoRewardService *sReward;
static RhhContentKey sUnscoredRule;
static int CheckScoreContext(const struct RhhPhotoRewardResult *context)
{
    uint32_t available = 99;
    return sReward->context_score_available(&available)
        || available != (context->receipt.rule != sUnscoredRule);
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhPhotoRewardResult context = {.struct_size = sizeof(context)};
    if (sReward->context(&context) || context.receipt.quantity != event->args[0]
        || context.receipt.score != event->args[1] || context.is_preview != event->args[2]
        || !context.receipt.individual.serial || !context.receipt.photo.serial || CheckScoreContext(&context))
    { puts("PHOTO_REWARD_BAD"); return; }
    if (context.receipt.quantity == 3)
    { decision->allowed = 0; snprintf(decision->reason, sizeof(decision->reason), "test reward veto"); }
    puts("PHOTO_REWARD_CONSTRAINT");
}
static void CannotUndo(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; decision->allowed = 1; decision->value = UINT64_MAX;
    snprintf(decision->reason, sizeof(decision->reason), "overwritten");
}
static void Observed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhPhotoRewardResult context = {.struct_size = sizeof(context)};
    struct RhhPhotoRewardReceipt stored = {.struct_size = sizeof(stored)};
    if (sReward->context(&context) || sReward->at(context.receipt.sequence - 1, &stored)
        || stored.sequence != event->ordinal || stored.item != context.receipt.item
        || stored.delivered != (event->kind == RHH_PHOTO_REWARD_DELIVERED) || CheckScoreContext(&context))
    { puts("PHOTO_REWARD_BAD"); return; }
    puts(event->kind == RHH_PHOTO_REWARD_DELIVERED ? "PHOTO_REWARD_DELIVERED" : "PHOTO_REWARD_CLAIMED");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_CLAIM_PHOTO_REWARD, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_CLAIM_PHOTO_REWARD, RHH_CONSTRAINT, 1, CannotUndo, NULL},
    {sizeof(struct RhhModHook), RHH_PHOTO_REWARD_CLAIMED, RHH_OBSERVE, 0, NULL, Observed},
    {sizeof(struct RhhModHook), RHH_PHOTO_REWARD_DELIVERED, RHH_OBSERVE, 0, NULL, Observed}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/photo_reward"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:photo_reward_probe",
    .hook_count = sizeof(sHooks) / sizeof(*sHooks), .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sUnscoredRule = host->content_key("test:reward/unscored");
    sReward = (const void *)host->get_service("rhh:service/photo_reward", RHH_SERVICE_VERSION(1, 1));
    return sReward && sReward->header.struct_size >= sizeof(*sReward) ? &sDefinition : NULL;
}
