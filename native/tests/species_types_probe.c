/* Independent module proposing species types through the public ABI. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhTypeService *sTypes;
static const struct RhhBattleService *sBattle;
static RhhContentKey sCharizard, sTorchic, sTreecko, sMudkip, sGengar, sPikachu, sEevee, sMareep, sCaterpie, sWeedle;
static RhhContentKey sFire, sFlying, sGrass, sWater, sGhost, sPoison, sNormal, sElectric, sSteel, sDark, sIce, sFighting, sGround;
static void Transform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhSpeciesTypesContext context = {.struct_size = sizeof(context)};
    struct RhhTypeList current = {.struct_size = sizeof(current)}, recursive = {.struct_size = sizeof(recursive), .count = 99};
    if (sTypes->species_context(&context) || context.species != event->event_key
        || context.species != event->subject || context.original.count != event->args[0]
        || sTypes->species_proposal(&current) || memcmp(&current, &context.original, sizeof(current))
        || !sTypes->species(context.species, &recursive) || recursive.count != 99)
    { puts("SPECIES_TYPES_BAD"); return; }
    /* Nested dispatch must restore the species event and its transform stage. */
    struct RhhTypeMatchup matchup = {.struct_size = sizeof(matchup)};
    if (sTypes->chart(sFire, sGrass, 0, &matchup))
    { puts("SPECIES_TYPES_BAD"); return; }
    struct RhhTypeList invalid = {.struct_size = sizeof(invalid), .count = 2, .types = {sFire, sFire, 0}};
    if (!sTypes->offer_species_types(&invalid) || sTypes->species_proposal(&current)
        || memcmp(&current, &context.original, sizeof(current)))
    { puts("SPECIES_TYPES_BAD"); return; }
    struct RhhTypeList proposal = {.struct_size = sizeof(proposal), .count = 3};
    if (context.species == sCharizard)
    { proposal.count = 2; proposal.types[0] = sFire; proposal.types[1] = sFlying; }
    else if (context.species == sTorchic)
    { proposal.types[0] = sFire; proposal.types[1] = sGrass; proposal.types[2] = sWater; }
    else if (context.species == sTreecko)
    { proposal.types[0] = sGrass; proposal.types[1] = sWater; proposal.types[2] = sFire; }
    else if (context.species == sMudkip)
    { proposal.types[0] = sWater; proposal.types[1] = sGrass; proposal.types[2] = sFire; }
    else if (context.species == sGengar)
    { proposal.types[0] = sGhost; proposal.types[1] = sPoison; proposal.types[2] = sWater; }
    else if (context.species == sEevee)
    { proposal.types[0] = sNormal; proposal.types[1] = sGrass; proposal.types[2] = sFlying; }
    else if (context.species == sMareep)
    { proposal.types[0] = sElectric; proposal.types[1] = sDark; proposal.types[2] = sSteel; }
    else if (context.species == sCaterpie)
    { proposal.types[0] = sFire; proposal.types[1] = sFlying; proposal.types[2] = sIce; }
    else if (context.species == sWeedle)
    { proposal.types[0] = sFighting; proposal.types[1] = sGround; proposal.types[2] = sSteel; }
    else if (context.species == sPikachu) return;
    else return;
    if (sTypes->offer_species_types(&proposal) || sTypes->species_proposal(&current)
        || memcmp(&current, &proposal, sizeof(current))) puts("SPECIES_TYPES_BAD");
}
static void AddThird(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->event_key != sCharizard) return;
    struct RhhTypeList current = {.struct_size = sizeof(current)};
    if (sTypes->species_proposal(&current) || current.count != 2 || current.types[0] != sFire || current.types[1] != sFlying)
    { puts("SPECIES_TYPES_BAD"); return; }
    current.count = 3; current.types[2] = sGrass;
    if (sTypes->offer_species_types(&current)) puts("SPECIES_TYPES_BAD");
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhTypeList proposal = {.struct_size = sizeof(proposal)};
    if (sTypes->species_proposal(&proposal))
    {
        struct RhhExtendedTypeList full = {.struct_size = sizeof(full)};
        if (sTypes->header.version < RHH_SERVICE_VERSION(1, 6)
            || sTypes->header.struct_size < sizeof(*sTypes)
            || sTypes->species_proposal_list(&full) || full.count <= 3) puts("SPECIES_TYPES_BAD");
    }
    else if (!sTypes->offer_species_types(&proposal)) puts("SPECIES_TYPES_BAD");
    if (event->event_key == sGengar) decision->allowed = 0;
    decision->value = 777;
}
static void LaterConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->allowed = 1; }
static void NestedMatchup(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhSpeciesTypesContext context = {.struct_size = sizeof(context)};
    struct RhhTypeList proposal = {.struct_size = sizeof(proposal), .count = 1, .types = {sFire}};
    if (!sTypes->species_context(&context) || !sTypes->offer_species_types(&proposal)) puts("SPECIES_TYPES_BAD");
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleEventContext battle = {.struct_size = sizeof(battle)};
    if (sBattle->context(&battle)) { puts("SPECIES_TYPES_BAD"); return; }
    RhhContentKey species = 0;
    for (unsigned i = 0; i < battle.battle.participant_count; ++i)
        if (battle.battle.participants[i].battler == event->args[0]) species = battle.battle.participants[i].species;
    if (species != sTorchic && species != sCharizard && species != sTreecko && species != sMudkip) return;
    struct RhhTypeList list = {.struct_size = sizeof(list)};
    RhhContentKey first = species == sTreecko ? sGrass : species == sMudkip ? sWater : sFire;
    RhhContentKey second = species == sCharizard ? sFlying : species == sTreecko ? sWater : sGrass;
    RhhContentKey third = species == sCharizard ? sGrass : species == sTorchic ? sWater : sFire;
    if (sTypes->battler(event->args[0], &list) || list.count != 3 || list.types[0] != first
        || list.types[1] != second || list.types[2] != third)
    { puts("SPECIES_TYPES_BAD"); return; }
    puts("SPECIES_TYPES_ENTERED");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_SPECIES_TYPES, RHH_TRANSFORM, 0, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_SPECIES_TYPES, RHH_TRANSFORM, 1, AddThird, NULL},
    {sizeof(struct RhhModHook), RHH_SPECIES_TYPES, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_SPECIES_TYPES, RHH_CONSTRAINT, 1, LaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_MATCHUP, RHH_CONSTRAINT, 0, NestedMatchup, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLER_ENTERED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/types"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:species_types",
    .hook_count = 6, .hooks = sHooks, .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sCharizard = host->content_key("rhh:species/charizard"); sTorchic = host->content_key("rhh:species/torchic");
    sTreecko = host->content_key("rhh:species/treecko"); sMudkip = host->content_key("rhh:species/mudkip");
    sGengar = host->content_key("rhh:species/gengar"); sPikachu = host->content_key("rhh:species/pikachu");
    sEevee = host->content_key("rhh:species/eevee");
    sMareep = host->content_key("rhh:species/mareep");
    sCaterpie = host->content_key("rhh:species/caterpie"); sWeedle = host->content_key("rhh:species/weedle");
    sFire = host->content_key("rhh:type/fire"); sFlying = host->content_key("rhh:type/flying");
    sGrass = host->content_key("rhh:type/grass"); sWater = host->content_key("rhh:type/water");
    sGhost = host->content_key("rhh:type/ghost"); sPoison = host->content_key("rhh:type/poison");
    sNormal = host->content_key("rhh:type/normal");
    sElectric = host->content_key("rhh:type/electric"); sSteel = host->content_key("rhh:type/steel");
    sDark = host->content_key("rhh:type/dark");
    sIce = host->content_key("rhh:type/ice"); sFighting = host->content_key("rhh:type/fighting");
    sGround = host->content_key("rhh:type/ground");
    sTypes = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 2));
    sBattle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 0));
    return sTypes && sBattle ? &sDefinition : NULL;
}
