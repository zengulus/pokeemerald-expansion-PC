#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhPhotoRewardService *reward;
static const struct RhhOwnedMonService *owned;
static unsigned awardGates;
static unsigned retargetCommits;
static void AwardGate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhExperienceAwardResult context = {.struct_size = sizeof(context)};
    if (owned->experience_award_context(&context)) { puts("PHOTO_EXPERIENCE_BAD"); return; }
    if (context.experience.requested == 799 && ++awardGates == 2)
    { decision->allowed = 0; strcpy(decision->reason, "late denial"); }
}
static void Gate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhPhotoExperienceResult context = {.struct_size = sizeof(context)};
    if (reward->experience_context(&context) || context.receipt.sequence != event->ordinal
        || context.receipt.amount != event->args[0] || !context.receipt.recipient_identity)
        puts("PHOTO_EXPERIENCE_BAD");
    else
    {
        if (context.receipt.amount == 777) { decision->allowed = 0; strcpy(decision->reason, "test veto"); }
        puts("PHOTO_EXPERIENCE_GATE");
    }
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhPhotoExperienceResult context = {.struct_size = sizeof(context)};
    struct RhhPhotoExperienceReceipt stored = {.struct_size = sizeof(stored)};
    if (reward->experience_context(&context) || context.receipt.sequence != event->ordinal
        || reward->experience_at(context.receipt.sequence - 1, &stored)
        || stored.delivered != (event->kind == RHH_PHOTO_EXPERIENCE_DELIVERED))
    { puts("PHOTO_EXPERIENCE_BAD"); return; }
    puts(event->kind == RHH_PHOTO_EXPERIENCE_CLAIMED ? "PHOTO_EXPERIENCE_CLAIMED" : "PHOTO_EXPERIENCE_DELIVERED");
}
static void Awarded(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhExperienceAwardResult context = {.struct_size = sizeof(context)};
    struct RhhPhotoExperienceReceipt receipt = {.struct_size = sizeof(receipt)};
    if (owned->experience_award_context(&context) || context.ticket != event->ordinal
        || reward->experience_at(0, &receipt) || !receipt.delivered
        || receipt.recipient_identity != context.experience.mon.identity)
    { puts("PHOTO_EXPERIENCE_BAD"); return; }
    puts("PHOTO_EXPERIENCE_ATOMIC");
}
static void RetargetGate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhPhotoExperienceRetargetResult context = {.struct_size = sizeof(context)};
    if (reward->experience_retarget_context(&context) || context.before.sequence != event->ordinal
        || !context.after.recipient_identity || context.before.delivered || context.after.delivered)
        puts("PHOTO_EXPERIENCE_BAD");
    else
    {
        struct RhhPhotoExperienceRetargetRequest nested = {.struct_size = sizeof(nested), .expected = context.before,
            .recipient.struct_size = sizeof(nested.recipient)};
        struct RhhPhotoExperienceRetargetResult rejected = {.struct_size = sizeof(rejected)};
        if (owned->experience(&context.experience.mon, &nested.recipient)
            || reward->retarget_experience(&nested, &rejected) != RHH_PHOTO_REWARD_UNAVAILABLE)
        { puts("PHOTO_EXPERIENCE_BAD"); return; }
        if (!event->args[0] && ++retargetCommits == 1)
        { decision->allowed = 0; strcpy(decision->reason, "retarget veto"); }
        puts("PHOTO_EXPERIENCE_RETARGET_GATE");
    }
}
static void Retargeted(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhPhotoExperienceRetargetResult context = {.struct_size = sizeof(context)};
    struct RhhPhotoExperienceReceipt current = {.struct_size = sizeof(current)};
    if (reward->experience_retarget_context(&context) || context.before.sequence != event->ordinal
        || reward->experience_at(context.before.sequence - 1, &current)
        || current.recipient_identity != context.after.recipient_identity || current.delivered)
        puts("PHOTO_EXPERIENCE_BAD");
    else puts("PHOTO_EXPERIENCE_RETARGETED");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_CLAIM_PHOTO_EXPERIENCE, RHH_CONSTRAINT, 0, Gate, NULL},
    {sizeof(struct RhhModHook), RHH_PHOTO_EXPERIENCE_CLAIMED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_PHOTO_EXPERIENCE_DELIVERED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_CAN_AWARD_EXPERIENCE, RHH_CONSTRAINT, 0, AwardGate, NULL},
    {sizeof(struct RhhModHook), RHH_EXPERIENCE_AWARDED, RHH_OBSERVE, 0, NULL, Awarded},
    {sizeof(struct RhhModHook), RHH_CAN_RETARGET_PHOTO_EXPERIENCE, RHH_CONSTRAINT, 0, RetargetGate, NULL},
    {sizeof(struct RhhModHook), RHH_PHOTO_EXPERIENCE_RETARGETED, RHH_OBSERVE, 0, NULL, Retargeted}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 3), RHH_SERVICE_REQUIRED, 0, "rhh:service/photo_reward"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:photo_experience", .hooks = hooks, .hook_count = 7,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    reward = (const void *)host->get_service("rhh:service/photo_reward", RHH_SERVICE_VERSION(1, 3));
    owned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 29));
    return owned && reward && reward->header.struct_size >= RHH_PHOTO_REWARD_SERVICE_V1_3_SIZE ? &definition : NULL;
}
