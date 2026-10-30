#include "mod_api.h"
#include <stdio.h>
static struct RhhModMove sMovesDefined[2];
static struct RhhTypeRelation sRelation;
static const struct RhhModType sType = {sizeof(sType), 0x86aee0, 1, 0, "test:type/prism", "Prism", &sRelation};
static const struct RhhMoveService *sMoves;
static const struct RhhTypeService *sTypes;
static RhhContentKey sTidal, sPrism, sPrismType, sWater, sPound, sScratch, sWaterGun, sGrowl, sSwift, sCurse, sFirePunch;
static void MoveType(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key != sTidal && event->event_key != sPrism) return;
    RhhContentKey expected = event->event_key == sTidal ? sWater : sPrismType;
    struct RhhMoveInfo info = {.struct_size = sizeof(info)};
    if (event->subject != expected || decision->value != expected || sMoves->info(event->event_key, &info)
        || info.move != event->event_key || info.type != expected) puts("CUSTOM_MOVE_BAD");
}
static void Stab(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhTypeStabContext context = {.struct_size = sizeof(context)};
    if (sTypes->stab_context(&context)) { puts("CUSTOM_MOVE_BAD"); return; }
    if (context.move == sTidal)
    {
        if (context.move_type != sWater) puts("CUSTOM_MOVE_BAD");
        puts("CUSTOM_MOVE_STAB");
    }
}
static void Power(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhMovePowerContext context = {.struct_size = sizeof(context)};
    struct RhhMovePowerContext tooSmall = {.struct_size = sizeof(tooSmall) - 1}, unchanged = tooSmall;
    if (!sMoves->power_context(&tooSmall) || tooSmall.struct_size != unchanged.struct_size)
        puts("CUSTOM_MOVE_BAD");
    if (sMoves->power_context(&context) || context.move != event->event_key
        || context.attacker_species != event->subject || context.base_power != event->args[0]
        || context.flags != event->args[1] || context.attacker != event->args[2]
        || context.defender != event->args[3] || !context.attacker_hp || !context.defender_hp)
        puts("CUSTOM_MOVE_BAD");
    if (context.move == sTidal)
    {
        if (context.move_type != sWater || context.base_move != sTidal || context.chosen_move != sTidal)
            puts("CUSTOM_MOVE_BAD");
        if ((context.flags & RHH_MOVE_POWER_FIXED)
            && (context.weather != RHH_WEATHER_SUN || context.terrain != RHH_TERRAIN_ELECTRIC))
            puts("CUSTOM_MOVE_BAD");
        decision->value = context.base_power * 2;
        puts(context.flags & RHH_MOVE_POWER_AI ? "CUSTOM_POWER_AI" : "CUSTOM_POWER_REAL");
    }
    else if (context.move == sPound) decision->value = 0;
    else if (context.move == sScratch) decision->value = 100;
    else if (context.move == sWaterGun) decision->value = 80;
}
static void PowerConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sScratch) decision->allowed = 0;
    if (event->event_key == sWaterGun) decision->value = 999;
}
static void PowerLaterConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sScratch) decision->allowed = 1;
}
static void Accuracy(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhMoveAccuracyContext context = {.struct_size = sizeof(context)};
    struct RhhMoveAccuracyContext small = {.struct_size = sizeof(small) - 1};
    if (!sMoves->accuracy_context(&small) || small.struct_size != sizeof(small) - 1
        || sMoves->accuracy_context(&context) || context.move != event->event_key
        || context.attacker_species != event->subject || context.base_accuracy != event->args[0]
        || context.flags != event->args[1] || context.attacker != event->args[2]
        || context.defender != event->args[3]) puts("CUSTOM_MOVE_BAD");
    if (context.move == sTidal)
    {
        if (context.move_type != sWater) puts("CUSTOM_MOVE_BAD");
        decision->value = 0;
        puts(context.flags & RHH_MOVE_ACCURACY_AI ? "CUSTOM_ACCURACY_AI" : "CUSTOM_ACCURACY_REAL");
    }
    else if (context.move == sPound) decision->value = 101;
    else if (context.move == sScratch) decision->value = 0;
    else if (context.move == sWaterGun) decision->value = 50;
}
static void AccuracyConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sScratch) decision->allowed = 0;
    if (event->event_key == sWaterGun) decision->value = 100;
}
static void Target(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sPrism)
    {
        struct RhhMoveInfo info = {.struct_size = sizeof(info)};
        if (event->subject != RHH_MOVE_TARGET_BOTH || decision->value != RHH_MOVE_TARGET_BOTH
            || sMoves->info(sPrism, &info) || info.target != RHH_MOVE_TARGET_BOTH) puts("CUSTOM_MOVE_BAD");
        decision->value = RHH_MOVE_TARGET_USER_AND_ALLY;
        puts("CUSTOM_TARGET");
    }
    else if (event->event_key == sPound) decision->value = RHH_MOVE_TARGET_BOTH;
    else if (event->event_key == sScratch) decision->value = RHH_MOVE_TARGET_USER;
    else if (event->event_key == sWaterGun) decision->value = RHH_MOVE_TARGET_SMART;
    else if (event->event_key == sGrowl) decision->value = 999;
    else if (event->event_key == sSwift) decision->value = RHH_MOVE_TARGET_DEPENDS;
    else if (event->event_key == sCurse) decision->value = RHH_MOVE_TARGET_BOTH;
}
static void TargetConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sScratch) decision->allowed = 0;
    if (event->event_key == sPound) decision->value = RHH_MOVE_TARGET_USER;
}
static void TargetLaterConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sScratch) decision->allowed = 1;
}
static void AdditionalEffects(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhMoveAdditionalEffect effect = {.struct_size = sizeof(effect), .kind = RHH_ADDITIONAL_RAIN,
        .target = RHH_ADDITIONAL_TARGET, .chance = 100};
    if (event->event_key == sTidal)
    {
        struct RhhMoveInfo nested = {.struct_size = sizeof(nested)};
        if (event->args[0] != 0 || event->args[1] != 7 || sMoves->info(sTidal, &nested)
            || nested.move != sTidal || sMoves->offer_additional_effect(&effect) != RHH_ADDITIONAL_OK)
            puts("CUSTOM_MOVE_BAD");
        puts("CUSTOM_ADDITIONAL");
    }
    else if (event->event_key == sFirePunch)
    {
        effect.kind = RHH_ADDITIONAL_PARALYSIS; effect.chance = 50;
        if (event->args[0] != 1 || event->args[1] != 6
            || sMoves->offer_additional_effect(&effect) != RHH_ADDITIONAL_OK) puts("CUSTOM_MOVE_BAD");
        effect.kind = RHH_ADDITIONAL_STAT_RAISE; effect.target = RHH_ADDITIONAL_USER;
        effect.chance = 100; effect.stat = RHH_STAT_ATTACK; effect.stages = 2;
        if (sMoves->offer_additional_effect(&effect) != RHH_ADDITIONAL_OK) puts("CUSTOM_MOVE_BAD");
    }
    else if (event->event_key == sWaterGun)
    {
        effect.kind = RHH_ADDITIONAL_STAT_RAISE; effect.target = RHH_ADDITIONAL_USER;
        effect.stat = RHH_STAT_DEFENSE; effect.stages = 1;
        if (sMoves->offer_additional_effect(&effect) != RHH_ADDITIONAL_OK) puts("CUSTOM_MOVE_BAD");
    }
    else if (event->event_key == sScratch)
    {
        effect.kind = RHH_ADDITIONAL_POISON;
        for (unsigned i = 0; i < 7; ++i)
            if (sMoves->offer_additional_effect(&effect) != RHH_ADDITIONAL_OK) puts("CUSTOM_MOVE_BAD");
        if (sMoves->offer_additional_effect(&effect) != RHH_ADDITIONAL_FULL) puts("CUSTOM_MOVE_BAD");
    }
    else if (event->event_key == sPound)
    {
        effect.kind = RHH_ADDITIONAL_STAT_RAISE; effect.target = RHH_ADDITIONAL_USER;
        effect.stat = RHH_STAT_HP; effect.stages = 2;
        if (sMoves->offer_additional_effect(&effect) != RHH_ADDITIONAL_INVALID) puts("CUSTOM_MOVE_BAD");
        effect.stat = RHH_STAT_ATTACK;
        for (unsigned i = 0; i < 7; ++i)
            if (sMoves->offer_additional_effect(&effect) != RHH_ADDITIONAL_OK) puts("CUSTOM_MOVE_BAD");
        if (sMoves->offer_additional_effect(&effect) != RHH_ADDITIONAL_FULL) puts("CUSTOM_MOVE_BAD");
        decision->allowed = 0;
    }
}
static void AdditionalLaterConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sPound) decision->allowed = 1;
}
static void CanTeach(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhMoveTeachResult context = {.struct_size = sizeof(context)};
    if (sMoves->teach_context(&context) || context.move != event->event_key
        || context.mon.identity != event->subject || context.move_slot != event->args[1])
        puts("CUSTOM_MOVE_BAD");
    if (event->args[1] == 3) decision->allowed = 0;
}
static void Taught(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state; (void)decision;
    struct RhhMoveTeachResult context = {.struct_size = sizeof(context)};
    if (sMoves->teach_context(&context) || context.move != event->event_key || context.status != RHH_EDIT_OK)
        puts("CUSTOM_MOVE_BAD");
    puts("CUSTOM_MOVE_TAUGHT");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TYPE, RHH_TRANSFORM, 0, MoveType, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_STAB, RHH_TRANSFORM, 0, Stab, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_POWER, RHH_TRANSFORM, 0, Power, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_POWER, RHH_CONSTRAINT, 0, PowerConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_POWER, RHH_CONSTRAINT, 1, PowerLaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_ACCURACY, RHH_TRANSFORM, 0, Accuracy, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_ACCURACY, RHH_CONSTRAINT, 0, AccuracyConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TARGET, RHH_TRANSFORM, 0, Target, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TARGET, RHH_CONSTRAINT, 0, TargetConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_TARGET, RHH_CONSTRAINT, 1, TargetLaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_ADDITIONAL_EFFECTS, RHH_CONSTRAINT, 0, AdditionalEffects, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_ADDITIONAL_EFFECTS, RHH_CONSTRAINT, 1, AdditionalLaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_TEACH_MOVE, RHH_CONSTRAINT, 0, CanTeach, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_TAUGHT, RHH_OBSERVE, 0, NULL, Taught}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 7), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/types"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_MOVES, .id = "test:custom_move",
    .hook_count = 14, .hooks = sHooks, .service_requirement_count = 2, .service_requirements = sRequirements,
    .type_count = 1, .types = &sType, .move_count = 2, .moves = sMovesDefined
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 7));
    sTypes = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 0));
    if (!sMoves || !sTypes) return NULL;
    sTidal = host->content_key("test:move/tidal_pulse"); sPrism = host->content_key("test:move/prism_burst");
    sPrismType = host->content_key("test:type/prism"); sWater = host->content_key("rhh:type/water");
    sPound = host->content_key("rhh:move/pound"); sScratch = host->content_key("rhh:move/scratch");
    sWaterGun = host->content_key("rhh:move/water_gun");
    sGrowl = host->content_key("rhh:move/growl"); sSwift = host->content_key("rhh:move/swift");
    sCurse = host->content_key("rhh:move/curse");
    sFirePunch = host->content_key("rhh:move/fire_punch");
    sMovesDefined[0] = (struct RhhModMove){.struct_size = sizeof(sMovesDefined[0]), .power = 70, .accuracy = 95, .pp = 20,
        .category = RHH_MOVE_SPECIAL, .target = RHH_MOVE_TARGET_SELECTED, .priority = 1,
        .template_move = host->content_key("rhh:move/water_gun"), .type = host->content_key("rhh:type/water"),
        .id = "test:move/tidal_pulse", .name = "Tidal Pulse", .description = "A strong pulse.\nIt strikes the foe."};
    sRelation = (struct RhhTypeRelation){sizeof(sRelation), RHH_TYPE_ONE * 2, RHH_TYPE_UNSPECIFIED, 0,
        host->content_key("rhh:type/fire")};
    sMovesDefined[1] = sMovesDefined[0];
    sMovesDefined[1].id = "test:move/prism_burst"; sMovesDefined[1].name = "Prism Burst";
    sMovesDefined[1].type = host->content_key("test:type/prism");
    sMovesDefined[1].target = RHH_MOVE_TARGET_BOTH;
    return &sDefinition;
}
