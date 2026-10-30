/* Independent type-chart consumer; no engine headers. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhTypeService *sTypes;
static const struct RhhMoveService *sMoves;
static RhhContentKey sFire, sGrass, sGround, sFlying, sWater, sRock, sGhost, sNormal, sCrystal;
static RhhContentKey sFighting, sEkans, sExtraKeys[40];
static RhhContentKey sFlamethrower, sTackle, sWaterGun, sAdaptability;
static RhhContentKey sPound, sScratch, sGrowl, sBulbasaur;
static RhhContentKey sMetapod, sCaterpie, sWeedle, sKakuna, sAbra, sPidgey, sRattata, sDiglett, sSteel, sBug, sIce;
static RhhContentKey sWellspring, sBlazeTauros;
static void Transform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhTypeMatchup context = {.struct_size = sizeof(context)}, nested = {.struct_size = sizeof(nested), .base = 99};
    struct RhhTypeComposition nestedComposition = {.struct_size = sizeof(nestedComposition), .effective = 99};
    if (sTypes->context(&context) || context.attack != event->event_key || context.defense != event->subject
        || context.base != event->args[0] || !!(context.flags & RHH_TYPE_MATCHUP_INVERSE) != !!event->args[1]
        || context.adjusted != context.base || decision->value != context.base
        || !sTypes->chart(context.attack, context.defense, 0, &nested) || nested.base != 99
        || !sTypes->compose(context.attack, &context.defense, 1, 0, &nestedComposition)
        || nestedComposition.effective != 99)
    { puts("TYPE_PROBE_BAD"); return; }
    if (context.attack == sFire && context.defense == sGrass) decision->value = RHH_TYPE_ONE / 2;
    if (context.attack == sGround && context.defense == sFlying) decision->value = RHH_TYPE_ONE;
    if (context.attack == sRock && context.defense == sFlying) decision->value = RHH_TYPE_ONE;
    if (context.attack == sWater && context.defense == sRock) decision->value = 17;
    if (context.attack == sGhost && context.defense == sNormal) decision->value = RHH_TYPE_ONE;
    if (context.attack == sCrystal && context.defense == sFire) decision->value = RHH_TYPE_ONE;
    if (context.attack == sFighting || context.attack == sFlying)
        for (unsigned i = 0; i < 40; ++i)
            if (context.defense == sExtraKeys[i])
                decision->value = context.attack == sFighting ? RHH_TYPE_ONE * 2 : RHH_TYPE_ONE / 2;
}
static void Constraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sGhost && event->subject == sNormal) decision->allowed = 0;
    decision->value = 0; /* Constraints cannot change the proposed chart. */
}
static void LaterConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; decision->allowed = 1; }
static void SpeciesTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhTypeList before = {.struct_size = sizeof(before)}, after = {.struct_size = sizeof(after)};
    struct RhhTypeList invalid = {.struct_size = sizeof(invalid), .count = 1, .types = {42}};
    if (sTypes->species_proposal(&before) || !sTypes->offer_species_types(&invalid)
        || sTypes->species_proposal(&after) || memcmp(&before, &after, sizeof(before))) puts("TYPE_PROBE_BAD");
    invalid.count = 2; invalid.types[0] = invalid.types[1] = sCrystal;
    if (!sTypes->offer_species_types(&invalid) || sTypes->species_proposal(&after)
        || memcmp(&before, &after, sizeof(before))) puts("TYPE_PROBE_BAD");
    if (event->event_key == sRattata) {
        struct RhhExtendedTypeList full = {.struct_size = sizeof(full), .count = 4,
            .types = {sNormal, sFire, sFlying, sSteel}};
        if (sTypes->offer_species_types_list(&full)) puts("TYPE_PROBE_BAD");
        return;
    }
    if (event->event_key == sDiglett) {
        struct RhhExtendedTypeList full = {.struct_size = sizeof(full), .count = 4,
            .types = {sFire, sBug, sIce, sCrystal}};
        if (sTypes->offer_species_types_list(&full)) puts("TYPE_PROBE_BAD");
        return;
    }
    if (event->event_key == sEkans) {
        struct RhhExtendedTypeList full = {.struct_size = sizeof(full), .count = 40};
        for (unsigned i = 0; i < 40; ++i) full.types[i] = sExtraKeys[i];
        if (sTypes->offer_species_types_list(&full)) puts("TYPE_PROBE_BAD");
        return;
    }
    if (event->event_key == sAbra || event->event_key == sPidgey) {
        struct RhhExtendedTypeList full = {.struct_size = sizeof(full), .count = event->event_key == sAbra ? 4 : 5,
            .types = {sFire, sFlying, sGrass, sCrystal, sWater}};
        struct RhhExtendedTypeList check = {.struct_size = sizeof(check)};
        if (sTypes->offer_species_types_list(&full) || sTypes->species_proposal_list(&check)
            || check.count != full.count || memcmp(check.types, full.types, full.count * sizeof(full.types[0]))
            || !sTypes->species_proposal(&after)) puts("TYPE_PROBE_BAD");
        full.types[3] = sFire;
        if (!sTypes->offer_species_types_list(&full) || sTypes->species_proposal_list(&check)
            || check.types[3] != sCrystal) puts("TYPE_PROBE_BAD");
        return;
    }
    struct RhhTypeList custom = {.struct_size = sizeof(custom)};
    if (event->event_key == sWellspring) {
        custom.count = 3; custom.types[0] = sGrass; custom.types[1] = sCrystal; custom.types[2] = sWater;
    } else if (event->event_key == sMetapod || event->event_key == sKakuna || event->event_key == sBlazeTauros) {
        custom.count = 1; custom.types[0] = sCrystal;
    }
    else if (event->event_key == sCaterpie) {
        custom.count = 3; custom.types[0] = sFire; custom.types[1] = sFlying; custom.types[2] = sCrystal;
    } else if (event->event_key == sWeedle) {
        custom.count = 3; custom.types[0] = sCrystal; custom.types[1] = sGrass; custom.types[2] = sFire;
    }
    if (custom.count && sTypes->offer_species_types(&custom)) puts("TYPE_PROBE_BAD");
}
static void SpeciesConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sKakuna) decision->allowed = 0;
}
static void StabTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhTypeStabContext context = {.struct_size = sizeof(context)};
    struct RhhTypeMatchup nested = {.struct_size = sizeof(nested)};
    if (sTypes->stab_context(&context) || context.base != event->args[0] || context.flags != event->args[1]
        || context.attacker != event->args[2] || context.defender != event->args[3]
        || context.move_type != event->event_key || context.species != event->subject
        || !context.move || !context.active_types.count || !context.original_types.count
        || sTypes->chart(sWater, sRock, 0, &nested) || nested.base != RHH_TYPE_ONE * 2
        || sTypes->stab_context(&context))
    { puts("TYPE_PROBE_BAD"); return; }
    if ((context.flags & RHH_TYPE_STAB_TERA) && context.move == sFlamethrower
        && (context.base != RHH_TYPE_STAB_MAX || context.ability != sAdaptability
            || context.active_types.count != 1 || context.active_types.types[0] != sFire
            || context.original_types.count != 1 || context.original_types.types[0] != sFire))
    { puts("TYPE_PROBE_BAD"); return; }
    if (context.move == sFlamethrower) decision->value = RHH_TYPE_ONE * 2;
    if (context.move == sTackle) decision->value = 0;
    if (context.move == sWaterGun) decision->value = RHH_TYPE_STAB_MIN;
    puts(context.flags & RHH_TYPE_STAB_AI ? "TYPE_STAB_AI" : "TYPE_STAB_COMBAT");
}
static void StabConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhTypeStabContext context = {.struct_size = sizeof(context)};
    if (sTypes->stab_context(&context)) { puts("TYPE_PROBE_BAD"); return; }
    if (context.move == sWaterGun) decision->allowed = 0;
    decision->value = 0;
}
static void ExtendedStabTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhExtendedTypeStabContext context = {.struct_size = sizeof(context)};
    struct RhhTypeStabContext legacy = {.struct_size = sizeof(legacy)};
    if (sTypes->stab_context_list(&context) || !sTypes->stab_context(&legacy)
        || context.move_type != event->event_key || context.species != event->subject
        || context.base != event->args[0] || context.flags != event->args[1]
        || context.attacker != event->args[2] || context.defender != event->args[3]
        || decision->value != context.base) { puts("TYPE_PROBE_BAD"); return; }
    if (context.species != sAbra)
    {
        /* An invalid later transform must retain a valid legacy result. */
        if (context.move == sFlamethrower && context.base == RHH_TYPE_ONE * 2) decision->value = 0;
        return;
    }
    if (context.original_types.count != 4 || context.original_types.types[3] != sCrystal
        || ((context.flags & RHH_TYPE_STAB_TERA) ? context.active_types.count != 1
            : context.active_types.count != 4 || context.active_types.types[3] != sCrystal))
    { puts("TYPE_PROBE_BAD"); return; }
    struct RhhTypeMatchup nested = {.struct_size = sizeof(nested)};
    if (sTypes->chart(sWater, sRock, 0, &nested) || sTypes->stab_context_list(&context)
        || context.species != sAbra) { puts("TYPE_PROBE_BAD"); return; }
    if (context.move == sTackle || context.move == sWaterGun) decision->value = RHH_TYPE_ONE * 2;
    puts(context.flags & RHH_TYPE_STAB_AI ? "EXTENDED_STAB_AI" : "EXTENDED_STAB_COMBAT");
}
static void ExtendedStabConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhExtendedTypeStabContext context = {.struct_size = sizeof(context)};
    if (sTypes->stab_context_list(&context)) { puts("TYPE_PROBE_BAD"); return; }
    if (context.species == sAbra && context.move == sWaterGun) decision->allowed = 0;
    decision->value = 0; /* Constraints cannot change the proposed multiplier. */
}
static void MoveTypeTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sPound || event->event_key == sScratch)
    {
        if (event->subject != sNormal || decision->value != sNormal) puts("TYPE_PROBE_BAD");
        if (event->event_key == sPound)
        {
            struct RhhMoveInfo info = {.struct_size = sizeof(info)};
            struct RhhLearnsetEntry entry = {.struct_size = sizeof(entry)};
            uint32_t count = 0;
            if (sMoves->info(sPound, &info) || info.move != sPound || info.type != sNormal
                || info.category != RHH_MOVE_PHYSICAL || info.target != RHH_MOVE_TARGET_SELECTED
                || info.power != 40 || !(info.flags & RHH_MOVE_CONTACT)
                || sMoves->learnset_count(sBulbasaur, RHH_LEARNSET_LEVEL, &count) || !count
                || sMoves->learnset_at(sBulbasaur, RHH_LEARNSET_LEVEL, 0, &entry)
                || entry.species != sBulbasaur || !entry.move || !entry.level)
                puts("TYPE_PROBE_BAD");
        }
        decision->value = sCrystal;
    }
    else if (event->event_key == sGrowl) decision->value = 42;
}
static void MoveTypeConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sScratch) decision->allowed = 0;
    decision->value = sFire;
}
static void LegacyCategory(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    struct RhhLegacyTypeCategoryResult context = {.struct_size = sizeof(context)}, nested = {.struct_size = sizeof(nested), .category = 77};
    if (sTypes->legacy_category_context(&context) || context.type != event->event_key
        || context.category != RHH_TYPE_CATEGORY_MOVE_DEFAULT
        || !sTypes->legacy_category(event->event_key, &nested) || nested.category != 77)
    { puts("TYPE_PROBE_BAD"); return; }
    if (event->event_key != sCrystal) return;
    unsigned mode = *(const unsigned char *)state;
    if (mode == 1 || mode == 4) decision->value = RHH_TYPE_CATEGORY_SPECIAL;
    else if (mode == 2) decision->value = RHH_TYPE_CATEGORY_PHYSICAL;
    else if (mode == 3) decision->value = 99;
}
static void LegacyCategoryConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event;
    if (*(const unsigned char *)state == 4) decision->allowed = 0;
    decision->value = RHH_TYPE_CATEGORY_PHYSICAL; /* Constraints cannot rewrite proposals. */
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_TYPE_MATCHUP, RHH_TRANSFORM, 0, Transform, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_MATCHUP, RHH_CONSTRAINT, 0, Constraint, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_MATCHUP, RHH_CONSTRAINT, 1, LaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_SPECIES_TYPES, RHH_TRANSFORM, 0, SpeciesTransform, NULL},
    {sizeof(struct RhhModHook), RHH_SPECIES_TYPES, RHH_CONSTRAINT, 0, SpeciesConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_STAB, RHH_TRANSFORM, 0, StabTransform, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_STAB, RHH_CONSTRAINT, 0, StabConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_STAB, RHH_CONSTRAINT, 1, LaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_STAB_EXTENDED, RHH_TRANSFORM, 0, ExtendedStabTransform, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_STAB_EXTENDED, RHH_CONSTRAINT, 0, ExtendedStabConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_STAB_EXTENDED, RHH_CONSTRAINT, 1, LaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TYPE, RHH_TRANSFORM, 0, MoveTypeTransform, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TYPE, RHH_CONSTRAINT, 0, MoveTypeConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TYPE, RHH_CONSTRAINT, 1, LaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_LEGACY_CATEGORY, RHH_TRANSFORM, 0, LegacyCategory, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_LEGACY_CATEGORY, RHH_CONSTRAINT, 0, LegacyCategoryConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_LEGACY_CATEGORY, RHH_CONSTRAINT, 1, LaterConstraint, NULL}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 11), RHH_SERVICE_REQUIRED, 0, "rhh:service/types"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/type_catalog"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static struct RhhTypeRelation sCrystalRelations[3];
static struct RhhTypeRelation sExtraRelations[40];
static char sExtraIds[40][32];
static struct RhhModType sCustomTypes[41];
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_TYPES, .id = "test:type_chart",
    .state_size = 1, .hook_count = 17, .hooks = sHooks, .service_requirement_count = 3, .service_requirements = sRequirements,
    .type_count = 41, .types = sCustomTypes
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sFire = host->content_key("rhh:type/fire"); sGrass = host->content_key("rhh:type/grass");
    sGround = host->content_key("rhh:type/ground"); sFlying = host->content_key("rhh:type/flying");
    sWater = host->content_key("rhh:type/water"); sRock = host->content_key("rhh:type/rock");
    sGhost = host->content_key("rhh:type/ghost"); sNormal = host->content_key("rhh:type/normal");
    sCrystal = host->content_key("test:type/crystal");
    sMetapod = host->content_key("rhh:species/metapod"); sCaterpie = host->content_key("rhh:species/caterpie");
    sWeedle = host->content_key("rhh:species/weedle");
    sKakuna = host->content_key("rhh:species/kakuna");
    sAbra = host->content_key("rhh:species/abra");
    sPidgey = host->content_key("rhh:species/pidgey");
    sRattata = host->content_key("rhh:species/rattata"); sSteel = host->content_key("rhh:type/steel");
    sDiglett = host->content_key("rhh:species/diglett"); sBug = host->content_key("rhh:type/bug");
    sWellspring = host->content_key("rhh:species/ogerpon_wellspring");
    sBlazeTauros = host->content_key("rhh:species/tauros_paldea_blaze");
    sIce = host->content_key("rhh:type/ice");
    sFighting = host->content_key("rhh:type/fighting"); sEkans = host->content_key("rhh:species/ekans");
    sCustomTypes[0] = (struct RhhModType){sizeof(struct RhhModType), 0x86aee0, 3, 0,
        "test:type/crystal", "Crystal", sCrystalRelations};
    for (unsigned i = 0; i < 40; ++i) {
        snprintf(sExtraIds[i], sizeof(sExtraIds[i]), "test:type/extra_%02u", i);
        sExtraKeys[i] = host->content_key(sExtraIds[i]);
        sExtraRelations[i] = (struct RhhTypeRelation){sizeof(struct RhhTypeRelation), RHH_TYPE_UNSPECIFIED,
            i < 20 ? RHH_TYPE_ONE * 2 : RHH_TYPE_ONE / 2, 0, sCrystal};
        sCustomTypes[i + 1] = (struct RhhModType){sizeof(struct RhhModType), 0, 1, 0,
            sExtraIds[i], "Extra", &sExtraRelations[i]};
    }
    sCrystalRelations[0] = (struct RhhTypeRelation){sizeof(struct RhhTypeRelation), RHH_TYPE_ONE * 2, RHH_TYPE_ONE / 2, 0, sFire};
    sCrystalRelations[1] = (struct RhhTypeRelation){sizeof(struct RhhTypeRelation), RHH_TYPE_ONE / 2, RHH_TYPE_UNSPECIFIED, 0, sWater};
    sCrystalRelations[2] = (struct RhhTypeRelation){sizeof(struct RhhTypeRelation), RHH_TYPE_UNSPECIFIED, RHH_TYPE_ONE * 2, 0, sRock};
    sFlamethrower = host->content_key("rhh:move/flamethrower"); sTackle = host->content_key("rhh:move/tackle");
    sWaterGun = host->content_key("rhh:move/water_gun");
    sPound = host->content_key("rhh:move/pound"); sScratch = host->content_key("rhh:move/scratch");
    sGrowl = host->content_key("rhh:move/growl");
    sBulbasaur = host->content_key("rhh:species/bulbasaur");
    sAdaptability = host->content_key("rhh:ability/adaptability");
    sTypes = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 11));
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 0));
    return sTypes && sMoves ? &sDefinition : NULL;
}
