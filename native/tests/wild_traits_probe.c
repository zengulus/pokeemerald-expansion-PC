/* Independent ecology consumer exercised across actor save and restore. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhWildTraitsService *sTraits;
static const struct RhhWildActorService *sActors;
static RhhContentKey sWary, sInvalid, sDeny;
static const struct RhhModEcologyTag sTags[] = {
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/wary", "Wary", "Vary per individual."},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/invalid", "Invalid", "Check invalid transforms."},
    {sizeof(struct RhhModEcologyTag), 0, "test:trait/deny", "Denied", "Check sticky denial."}
};
static struct RhhModSpeciesTag sAssociations[] = {
    {sizeof(struct RhhModSpeciesTag), 0, 0, 0}, {sizeof(struct RhhModSpeciesTag), 0, 0, 0}
};
static void Transform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildTraitResult context = {.struct_size = sizeof(context)}, nested = {.struct_size = sizeof(nested), .value = 17};
    if (sTraits->context(&context) || context.tag != event->event_key || context.base != event->args[0]
        || context.individual_available != event->args[1]
        || event->ordinal != (context.individual_available ? context.individual.serial : context.actor.handle)
        || !sTraits->query(context.actor.handle, context.tag, &nested) || nested.value != 17
        || sActors->remove(context.actor.handle) != RHH_WILD_ACTOR_UNAVAILABLE)
    { puts("WILD_TRAITS_BAD"); return; }
    if (context.tag == sWary) decision->value = context.individual_available ? context.individual.serial & 1 : 0;
    else if (context.tag == sInvalid) decision->value = 42;
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sDeny) decision->allowed = 0;
    decision->value = 17; /* A constraint cannot change the proposed trait. */
}
static void LaterConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    decision->allowed = 1; /* Cannot undo an earlier denial. */
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildActorEvent actor = {.struct_size = sizeof(actor)}, after = {.struct_size = sizeof(after)};
    struct RhhWildTraitResult a = {.struct_size = sizeof(a)}, b = {.struct_size = sizeof(b)};
    if (sActors->context(&actor) || sTraits->query(actor.actor.handle, sWary, &a)
        || sTraits->query(actor.actor.handle, sWary, &b) || memcmp(&a, &b, sizeof(a))
        || !a.permitted || a.base != 1 || a.value != (a.individual_available ? (a.individual.serial & 1) : 0)
        || sTraits->query(actor.actor.handle, sInvalid, &b) || !b.permitted || b.base != 1 || b.value != 1
        || sTraits->query(actor.actor.handle, sDeny, &b) || b.permitted || b.value
        || sActors->context(&after) || after.actor.handle != actor.actor.handle)
    { puts("WILD_TRAITS_BAD"); return; }
    b.value = 33;
    if (!sTraits->query(actor.actor.handle, 0, &b) || b.value != 33
        || !sTraits->context(&b) || b.value != 33)
    { puts("WILD_TRAITS_BAD"); return; }
    if (event->kind == RHH_WILD_ACTOR_IDENTIFIED || actor.cause == RHH_WILD_ACTOR_RESTORE)
    {
        if (!a.individual_available || !a.individual.run_id || !a.individual.serial)
        { puts("WILD_TRAITS_BAD"); return; }
        puts(actor.cause == RHH_WILD_ACTOR_RESTORE ? "WILD_TRAITS_RESTORED" : "WILD_TRAITS_IDENTIFIED");
    }
    else
    {
        if (a.individual_available || a.individual.serial || a.individual.run_id)
        { puts("WILD_TRAITS_BAD"); return; }
        puts("WILD_TRAITS_SESSION");
    }
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_TRAIT_QUERY, RHH_TRANSFORM, 0, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_TRAIT_QUERY, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_TRAIT_QUERY, RHH_CONSTRAINT, 1, LaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_SPAWNED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_IDENTIFIED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_traits"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_ECOLOGY, .id = "test:wild_traits",
    .hook_count = 5, .hooks = sHooks, .service_requirement_count = 1, .service_requirements = sRequirements,
    .ecology_tag_count = 3, .ecology_tags = sTags, .species_tag_count = 2, .species_tags = sAssociations
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sWary = host->content_key(sTags[0].id); sInvalid = host->content_key(sTags[1].id); sDeny = host->content_key(sTags[2].id);
    sAssociations[0].species = sAssociations[1].species = host->content_key("rhh:species/poochyena");
    sAssociations[0].tag = sWary; sAssociations[1].tag = sInvalid;
    sTraits = (const void *)host->get_service("rhh:service/wild_traits", RHH_SERVICE_VERSION(1, 0));
    sActors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    return sTraits && sActors ? &sDefinition : NULL;
}
