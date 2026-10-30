#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhBreedingService *sBreeding;
static void Rule(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhBreedingIvRuleContext context = {.struct_size = sizeof(context)};
    struct RhhBreedingIvRuleContext tiny = {.struct_size = 1, .stat = 99}, before = tiny;
    struct RhhBreedingContext staged = {.struct_size = sizeof(staged)};
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    if (sBreeding->iv_rule_context(&context) || !sBreeding->iv_rule_context(&tiny) || memcmp(&tiny, &before, sizeof(tiny))
        || context.stat != event->ordinal || context.parents[0].mon.ivs[0] != 31 || context.parents[1].mon.ivs[0] != 10
        || !sBreeding->context(&staged) || !sBreeding->offer(&offer))
    { puts("BREED_RULE_BAD_CONTEXT"); return; }
    unsigned mode = *(const unsigned char *)state;
    if (mode == 0)
        decision->value = context.stat == 0 ? RHH_BREED_IV_PARENT_0 : context.stat == 1 ? RHH_BREED_IV_PARENT_1
            : context.stat == 2 ? RHH_BREED_IV_INITIAL : RHH_BREED_IV_ENGINE;
    else if (mode == 1)
        decision->value = context.stat < 3 ? RHH_BREED_IV_PARENT_0 : RHH_BREED_IV_PARENT_1;
    else if (mode == 2 && context.stat == 0) decision->value = 99;
    else if (mode == 2 && context.stat == 1) decision->value = RHH_BREED_IV_PARENT_1;
}
static void Deny(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    if (*(const unsigned char *)state == 2 && event->ordinal == 1) decision->allowed = 0;
    decision->value = RHH_BREED_IV_PARENT_0;
}
static void Later(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->allowed = 1; }
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BREED_IV_RULE, RHH_TRANSFORM, 0, Rule, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_IV_RULE, RHH_CONSTRAINT, 0, Deny, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_IV_RULE, RHH_CONSTRAINT, 1, Later, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 11), RHH_SERVICE_REQUIRED, 0, "rhh:service/breeding"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:breeding_iv_rule", .state_size = 1, .hooks = hooks, .hook_count = 3,
    .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sBreeding = (const void *)host->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 11));
    return sBreeding && sBreeding->header.struct_size >= RHH_BREEDING_SERVICE_V1_11_SIZE ? &definition : NULL;
}
