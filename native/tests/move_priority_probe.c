#include "mod_api.h"
#include <stdio.h>

static const struct RhhMoveService *sMoves;
static const struct RhhAbilityService *sAbilities;
static const struct RhhBattleService *sBattle;
static RhhContentKey sWaterGun, sScratch, sPound, sGrowl, sCustom, sStruggle;
static RhhContentKey sGust, sGale, sMaxGuard, sPrankster;
static RhhContentKey sCharizard, sTorrent, sLevitate, sPressure;
static RhhContentKey sFoulPlay, sBodyPress, sPsyshock;
static struct RhhModMove sMove;
static void BeatUpContributorProbe(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)decision;
    if (*(const uint32_t *)state != 42) return;
    struct RhhBeatUpContributor c = {.struct_size = sizeof(c)};
    if (sMoves->beat_up_contributor(&c) || !(c.flags & RHH_BEAT_UP_CONTRIBUTOR_AI)
        || c.hit_index > 2 || !c.species || c.attacker != 1 || c.defender != 0
        || (c.hit_index == 2 && (!(c.flags & RHH_BEAT_UP_CONTRIBUTOR_ESTIMATED)
            || c.party_slot != RHH_BEAT_UP_CONTRIBUTOR_NO_SLOT))) puts("PRIORITY_BAD");
}

static void Priority(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sStruggle) { puts("PRIORITY_BAD"); return; }
    if (event->event_key == sWaterGun) {
        struct RhhMoveInfo recursive = {.struct_size = sizeof(recursive)};
        if (event->args[0] != 7 || decision->value != 7 || sMoves->info(sWaterGun, &recursive)
            || recursive.priority != 0) puts("PRIORITY_BAD");
        decision->value = 9;
    } else if (event->event_key == sScratch) decision->value = 14;
    else if (event->event_key == sPound) decision->value = 15;
    else if (event->event_key == sGrowl) decision->value = 0;
    else if (event->event_key == sCustom) decision->value = 8;
}
static void Compose(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sWaterGun) {
        if (decision->value != 9) puts("PRIORITY_BAD");
        decision->value = 10;
    }
}
static void Constrain(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sWaterGun) decision->value = 0;
    if (event->event_key == sScratch) decision->allowed = 0;
}
static void CannotUndo(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->event_key == sScratch) decision->allowed = 1;
}
static void BattlerPriority(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    uint32_t mode = *(const uint32_t *)state;
    if (!mode) return;
    struct RhhMoveBattlerPriority context = {.struct_size = sizeof(context)}, small = {.struct_size = 1, .hp = 99};
    if (sMoves->battler_priority_context(&context) || !sMoves->battler_priority_context(&small) || small.hp != 99
        || context.move != event->event_key || context.species != event->subject || context.battler != event->args[0]
        || context.flags != event->args[2] || (uint32_t)(context.engine_priority + RHH_MOVE_PRIORITY_BIAS) != event->args[1]
        || (mode != 6 && decision->value != event->args[1]) || !context.run_id || !context.battle_id)
    { puts("PRIORITY_BAD"); return; }
    if (mode >= 12 && mode <= 15 && context.move == sWaterGun && context.battler == 1) {
        struct RhhInnateResolution list = {.struct_size = sizeof(list)};
        if (sAbilities->priority_innates(&list) || list.battler != 1 || list.count != 3
            || !!(list.flags & RHH_INNATE_AI_KNOWLEDGE) != !!(context.flags & RHH_MOVE_PRIORITY_AI)) puts("PRIORITY_BAD");
    } else if (mode == 1 && context.move == sWaterGun && context.battler == 1) {
        if (context.base_priority != 3 || context.engine_priority != 3 || !context.max_hp) puts("PRIORITY_BAD");
        decision->value = context.hp == context.max_hp ? 11 : 5;
    } else if (mode == 2 && context.move == sGust && context.battler == 1) {
        struct RhhBattlerAbility hidden = {.struct_size = sizeof(hidden)};
        if (!sAbilities->battler(context.battler, &hidden)
            || (context.ability != 0 && context.ability != sGale)
            || context.engine_priority != (context.ability == sGale ? 1 : 0)) puts("PRIORITY_BAD");
        if (context.ability == sGale) decision->value = 9;
    } else if (mode == 3 && context.move == sGrowl && context.battler == 0) {
        if (context.ability != sPrankster || context.base_priority != -7 || context.engine_priority != -6) puts("PRIORITY_BAD");
        decision->value = 2;
    } else if (mode == 4 && context.move == sWaterGun) decision->value = RHH_MOVE_BATTLER_PRIORITY_ENCODED_MAX + 1;
    else if (mode == 5 && context.move == sWaterGun) decision->value = 0;
    else if ((mode == 7 || mode == 11) && context.move == sMaxGuard) {
        if (context.flags != (RHH_MOVE_PRIORITY_MAX_GUARD | (mode == 11 ? RHH_MOVE_PRIORITY_AI : 0)) || context.chosen_move != sGrowl
            || context.base_priority != context.engine_priority) puts("PRIORITY_BAD");
        decision->value = (uint32_t)(context.engine_priority - 1 + RHH_MOVE_PRIORITY_BIAS);
    } else if ((mode == 8 || mode == 9) && context.move == sWaterGun && context.battler == 1) {
        if (context.flags != (mode == 9 ? RHH_MOVE_PRIORITY_AI : 0)) puts("PRIORITY_BAD");
    }
}
static void OfferPriorityInnates(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->event_key != sCharizard) return;
    const RhhContentKey keys[] = {sTorrent, sLevitate, sPressure};
    const uint32_t levels[] = {10, 20, 50};
    for (uint32_t i = 0; i < 3; ++i) {
        struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .ability = keys[i], .unlock_level = levels[i]};
        if (sAbilities->offer_innate(&offer) != RHH_INNATE_OK) puts("PRIORITY_BAD");
    }
}
static void InnatePriority(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    uint32_t mode = *(const uint32_t *)state;
    if (mode < 12 || mode > 15 || event->subject != sWaterGun) return;
    struct RhhInnatePriorityContext context = {.struct_size = sizeof(context)}, tiny = {.struct_size = 1, .index = 99};
    if (sAbilities->innate_priority_context(&context) || !sAbilities->innate_priority_context(&tiny) || tiny.index != 99
        || context.ability != event->event_key || context.index != event->ordinal || context.index >= context.resolution.count
        || context.resolution.entries[context.index].status || context.move.move != event->subject
        || context.move.battler != event->args[1] || context.move.flags != event->args[2]
        || (uint32_t)(context.current_priority + RHH_MOVE_PRIORITY_BIAS) != event->args[0])
    { puts("PRIORITY_BAD"); return; }
    if (context.index == 1 && context.current_priority != (3 + (mode == 12 && !context.resolution.entries[0].status ? 1 : 0)))
        puts("PRIORITY_BAD");
    if (context.index == 2 && context.current_priority != (3 + (!context.resolution.entries[0].status ? 1 : 0)
        + (!context.resolution.entries[1].status ? 2 : 0))) puts("PRIORITY_BAD");
    if (mode == 13 && context.index == 0) decision->value = RHH_MOVE_BATTLER_PRIORITY_ENCODED_MAX + 1;
    else if (mode != 15 || context.index != 0) decision->value = event->args[0] + context.index + 1;
}
static void InnatePriorityConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    uint32_t mode = *(const uint32_t *)state;
    if (mode == 14 && event->event_key == sTorrent) decision->allowed = 0;
    if (mode == 12 && event->event_key == sLevitate) decision->value = 0;
}
static void BattlerConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    uint32_t mode = *(const uint32_t *)state;
    if (mode == 1 && event->event_key == sWaterGun) decision->value = 0;
    if (mode == 5 && event->event_key == sWaterGun) decision->allowed = 0;
}
static void Speed(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    uint32_t mode = *(const uint32_t *)state;
    if (mode < 16 || event->args[0] != 1) return;
    struct RhhBattleSpeedContext context = {.struct_size = sizeof(context)}, small = {.struct_size = 1, .hp = 99};
    if (sBattle->speed_context(&context) || !sBattle->speed_context(&small) || small.hp != 99
        || context.battler != event->args[0] || context.engine_speed != event->args[1]
        || context.flags != event->args[2] || context.hold_effect != event->args[3]
        || context.species != event->event_key || context.ability != event->subject
        || !context.run_id || !context.battle_id) puts("PRIORITY_BAD");
    if (mode == 16) {
        struct RhhInnateResolution list = {.struct_size = sizeof(list)};
        if (sAbilities->speed_innates(&list) || list.battler != 1 || list.count != 3
            || !!(list.flags & RHH_INNATE_AI_KNOWLEDGE) != !!(context.flags & RHH_BATTLE_SPEED_AI)) puts("PRIORITY_BAD");
        decision->value = context.engine_speed * 2;
    } else if (mode == 17) decision->value = 0;
    else if (mode == 18) decision->value = 15;
    else if (mode == 22 || mode == 23) {
        if (!!(context.flags & RHH_BATTLE_SPEED_AI) != (mode == 22)) puts("PRIORITY_BAD");
        decision->value = 30;
    }
}
static void SpeedConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    if (*(const uint32_t *)state == 18 && event->args[0] == 1) decision->allowed = 0;
    if (*(const uint32_t *)state == 16) decision->value = 0;
}
static void InnateSpeed(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    uint32_t mode = *(const uint32_t *)state;
    if ((mode != 16 && mode != 20 && mode != 21) || event->args[1] != 1) return;
    struct RhhInnateSpeedContext context = {.struct_size = sizeof(context)}, small = {.struct_size = 1, .index = 99};
    if (sAbilities->innate_speed_context(&context) || !sAbilities->innate_speed_context(&small) || small.index != 99
        || context.ability != event->event_key || context.index != event->ordinal || context.index >= context.resolution.count
        || context.resolution.entries[context.index].status || context.speed.species != event->subject
        || context.current_speed != event->args[0] || context.speed.battler != event->args[1]
        || context.speed.flags != event->args[2]) puts("PRIORITY_BAD");
    if (mode == 20 && context.index == 0) decision->value = RHH_BATTLE_SPEED_MAX + 1u;
    else if (mode != 21 || context.index != 0) decision->value = context.current_speed + context.index + 1;
}
static void DamageStat(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    uint32_t mode = *(const uint32_t *)state;
    if (mode < 30) return;
    struct RhhDamageStatContext context = {.struct_size = sizeof(context)}, small = {.struct_size = 1, .power = 99};
    if (sMoves->damage_stat_context(&context) || !sMoves->damage_stat_context(&small) || small.power != 99
        || context.move != event->event_key || context.attacker_species != event->subject
        || context.phase != event->args[0] || context.engine_value != event->args[1]
        || context.flags != event->args[2] || context.stat_owner != event->args[3]
        || !context.power || !context.max_value) puts("PRIORITY_BAD");
    if (mode == 30) {
        struct RhhInnateResolution list = {.struct_size = sizeof(list)};
        if (sAbilities->damage_stat_innates(RHH_INNATE_POWER_ATTACKER, &list) || list.count != 3
            || !!(list.flags & RHH_INNATE_AI_KNOWLEDGE) != !!(context.flags & RHH_DAMAGE_STAT_AI)) puts("PRIORITY_BAD");
        decision->value = context.phase == RHH_DAMAGE_STAT_ATTACK ? context.engine_value * 2 : context.engine_value / 2;
    } else if (mode == 32) decision->value = context.phase == RHH_DAMAGE_STAT_ATTACK ? (uint64_t)context.max_value + 1 : 0;
    else if (mode == 37) decision->value = context.engine_value + 10;
    else if (mode == 38 || mode == 39) {
        struct RhhInnateResolution list = {.struct_size = sizeof(list)};
        if (sAbilities->damage_stat_innates(RHH_INNATE_POWER_DEFENDER, &list) || list.count != 3
            || !(list.flags & RHH_INNATE_AI_KNOWLEDGE)
            || !!(list.entries[1].status & RHH_INNATE_MOVE_BYPASSED) != (mode == 38)) puts("PRIORITY_BAD");
    } else if (mode == 36) {
        if (event->event_key == sFoulPlay && context.phase == RHH_DAMAGE_STAT_ATTACK
            && (context.stat_owner != context.defender || context.stat != RHH_STAT_ATTACK || context.stage_stat != RHH_STAT_ATTACK)) puts("PRIORITY_BAD");
        if (event->event_key == sBodyPress && context.phase == RHH_DAMAGE_STAT_ATTACK
            && (context.stat_owner != context.attacker || context.stat != RHH_STAT_DEFENSE || context.stage_stat != RHH_STAT_SP_DEFENSE)) puts("PRIORITY_BAD");
        if (event->event_key == sPsyshock && context.phase == RHH_DAMAGE_STAT_DEFENSE
            && (context.stat_owner != context.defender || context.stat != RHH_STAT_SP_DEFENSE || context.stage_stat != RHH_STAT_DEFENSE)) puts("PRIORITY_BAD");
    }
}
static void DamageStatConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    if (*(const uint32_t *)state == 30) decision->value = 0;
    if (*(const uint32_t *)state == 37 && event->args[0] == RHH_DAMAGE_STAT_ATTACK) decision->allowed = 0;
}
static void InnateDamageStat(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    uint32_t mode = *(const uint32_t *)state;
    if ((mode != 31 && mode != 34 && mode != 35) || event->args[1] != RHH_INNATE_POWER_ATTACKER) return;
    struct RhhInnateDamageStatContext context = {.struct_size = sizeof(context)}, small = {.struct_size = 1, .index = 99};
    if (sAbilities->innate_damage_stat_context(&context) || !sAbilities->innate_damage_stat_context(&small) || small.index != 99
        || context.ability != event->event_key || context.index != event->ordinal || context.index >= context.resolution.count
        || context.resolution.entries[context.index].status || context.stat.move != event->subject
        || context.current_value != event->args[0] || context.role != event->args[1]
        || context.stat.phase != event->args[2] || context.stat.stat_owner != event->args[3]) puts("PRIORITY_BAD");
    if (mode == 34 && context.index == 0) decision->value = 0;
    else if (mode != 35 || context.index != 0) decision->value = context.current_value + context.index + 1;
}
static void FinalDamage(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    uint32_t mode = *(const uint32_t *)state;
    if ((event->kind == RHH_DAMAGE_FINAL && mode != 40)
        || (event->kind == RHH_SELF_DAMAGE_FINAL && mode != 41)) return;
    struct RhhFinalDamageContext context = {.struct_size = sizeof(context)}, small = {.struct_size = 1, .engine_damage = 99};
    struct RhhInnateResolution list = {.struct_size = sizeof(list)};
    if (sMoves->final_damage_context(&context) || !sMoves->final_damage_context(&small) || small.engine_damage != 99
        || sAbilities->final_damage_innates(RHH_INNATE_POWER_ATTACKER, &list)
        || context.move != event->event_key || context.engine_damage != event->args[0]
        || context.flags != event->args[1] || context.max_damage != RHH_FINAL_DAMAGE_MAX
        || list.count != 3 || !!(list.flags & RHH_INNATE_AI_KNOWLEDGE) != (mode == 40)
        || (mode == 41 && (!(context.flags & RHH_FINAL_DAMAGE_SELF_INFLICTED) || context.move))) puts("PRIORITY_BAD");
    decision->value += 10;
}
static void InnateFinalDamage(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    uint32_t mode = *(const uint32_t *)state;
    if ((event->kind == RHH_INNATE_DAMAGE_FINAL && mode != 40)
        || (event->kind == RHH_INNATE_SELF_DAMAGE_FINAL && mode != 41)
        || event->args[1] != RHH_INNATE_POWER_ATTACKER) return;
    struct RhhInnateFinalDamageContext context = {.struct_size = sizeof(context)};
    if (sAbilities->innate_final_damage_context(&context) || context.ability != event->event_key
        || context.index != event->ordinal || context.current_damage != event->args[0]
        || context.damage.move != event->subject
        || !!(context.damage.flags & RHH_FINAL_DAMAGE_AI) != (mode == 40)
        || context.index >= context.resolution.count || context.resolution.entries[context.index].status)
        puts("PRIORITY_BAD");
    decision->value = context.current_damage + context.index + 1;
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_PRIORITY, RHH_TRANSFORM, 0, Priority, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_PRIORITY, RHH_TRANSFORM, 1, Compose, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_PRIORITY, RHH_CONSTRAINT, 0, Constrain, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BASE_PRIORITY, RHH_CONSTRAINT, 1, CannotUndo, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BATTLER_PRIORITY, RHH_TRANSFORM, 0, BattlerPriority, NULL},
    {sizeof(struct RhhModHook), RHH_MOVE_BATTLER_PRIORITY, RHH_CONSTRAINT, 0, BattlerConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_PROPOSE, RHH_CONSTRAINT, 0, OfferPriorityInnates, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_PRIORITY, RHH_TRANSFORM, 0, InnatePriority, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_PRIORITY, RHH_CONSTRAINT, 0, InnatePriorityConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLER_SPEED, RHH_TRANSFORM, 0, Speed, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLER_SPEED, RHH_CONSTRAINT, 0, SpeedConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_SPEED, RHH_TRANSFORM, 0, InnateSpeed, NULL},
    {sizeof(struct RhhModHook), RHH_DAMAGE_STAT, RHH_TRANSFORM, 0, DamageStat, NULL},
    {sizeof(struct RhhModHook), RHH_DAMAGE_STAT, RHH_CONSTRAINT, 0, DamageStatConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_DAMAGE_STAT, RHH_TRANSFORM, 0, InnateDamageStat, NULL},
    {sizeof(struct RhhModHook), RHH_DAMAGE_FINAL, RHH_TRANSFORM, 0, FinalDamage, NULL},
    {sizeof(struct RhhModHook), RHH_DAMAGE_FINAL, RHH_TRANSFORM, 1, BeatUpContributorProbe, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_DAMAGE_FINAL, RHH_TRANSFORM, 0, InnateFinalDamage, NULL},
    {sizeof(struct RhhModHook), RHH_SELF_DAMAGE_FINAL, RHH_TRANSFORM, 0, FinalDamage, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_SELF_DAMAGE_FINAL, RHH_TRANSFORM, 0, InnateFinalDamage, NULL},
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 54), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 39), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 31), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_MOVES,
    .id = "test:move_priority", .state_size = sizeof(uint32_t), .hooks = sHooks, .hook_count = sizeof(sHooks) / sizeof(sHooks[0]), .move_count = 1, .moves = &sMove,
    .service_requirement_count = 3, .service_requirements = sRequirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 54));
    sAbilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 38));
    sBattle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 31));
    sWaterGun = host->content_key("rhh:move/water_gun"); sScratch = host->content_key("rhh:move/scratch");
    sPound = host->content_key("rhh:move/pound"); sGrowl = host->content_key("rhh:move/growl");
    sStruggle = host->content_key("rhh:move/struggle"); sCustom = host->content_key("test:move/priority_probe");
    sGust = host->content_key("rhh:move/gust"); sGale = host->content_key("rhh:ability/gale_wings");
    sMaxGuard = host->content_key("rhh:move/max_guard"); sPrankster = host->content_key("rhh:ability/prankster");
    sCharizard = host->content_key("rhh:species/charizard"); sTorrent = host->content_key("rhh:ability/torrent");
    sLevitate = host->content_key("rhh:ability/levitate"); sPressure = host->content_key("rhh:ability/pressure");
    sFoulPlay = host->content_key("rhh:move/foul_play"); sBodyPress = host->content_key("rhh:move/body_press");
    sPsyshock = host->content_key("rhh:move/psyshock");
    sMove = (struct RhhModMove){.struct_size = sizeof(sMove), .power = 40, .accuracy = 100, .pp = 20,
        .priority = -2, .category = RHH_MOVE_SPECIAL, .target = RHH_MOVE_TARGET_SELECTED,
        .template_move = sWaterGun, .type = host->content_key("rhh:type/water"),
        .id = "test:move/priority_probe", .name = "Prio Test", .description = "A test move."};
    return sMoves && sAbilities && sBattle && sWaterGun && sScratch && sPound && sGrowl && sCustom && sStruggle
        && sGust && sGale && sMaxGuard && sPrankster && sCharizard && sTorrent && sLevitate && sPressure
        && sFoulPlay && sBodyPress && sPsyshock ? &sDefinition : NULL;
}
