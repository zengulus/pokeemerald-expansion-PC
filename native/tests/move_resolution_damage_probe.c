#include <stdio.h>
#include <string.h>
#include "mod_api.h"

static const struct RhhMoveService *sMoves;
static void ObserveHit(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhMoveDamageApplied hit = {.struct_size = sizeof(hit)};
    struct RhhMoveDamageApplied tiny = {.struct_size = 1, .hp_lost = 99}, saved = tiny;
    if (sMoves->damage_applied_context(&hit) || !sMoves->damage_applied_context(&tiny)
        || memcmp(&tiny, &saved, sizeof(tiny)) || hit.run_id != 456 || !hit.battle_id
        || hit.sequence != event->ordinal || hit.move != event->event_key || hit.target_species != event->subject
        || hit.attacker != event->args[0] || hit.target != event->args[1]
        || hit.hp_lost != event->args[2] || hit.substitute_lost != event->args[3]
        || (!hit.hp_lost && !hit.substitute_lost)) puts("MOVE_DAMAGE_SUMMARY_BAD_HIT");
    else puts("MOVE_DAMAGE_HIT_OK");
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision;
    unsigned *sequence = state;
    struct RhhMoveResolutionDamage target = {.struct_size = sizeof(target)}, other = {.struct_size = sizeof(other)};
    struct RhhMoveResolutionDamage small = {.struct_size = 1, .hp_lost = 99}, before = small;
    if (sMoves->resolution_damage(1, &target) || !sMoves->resolution_damage(1, &small)
        || memcmp(&small, &before, sizeof(small)) || !sMoves->resolution_damage(4, &small)
        || memcmp(&small, &before, sizeof(small)) || target.battler != 1)
        puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 0 && (target.flags != RHH_MOVE_RESOLUTION_DAMAGE_RECORDED || target.updates != 2
                           || target.hp_lost != 10 || target.substitute_lost)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    else if (*sequence == 1 && (target.flags != RHH_MOVE_RESOLUTION_DAMAGE_RECORDED || target.updates != 2
                                || target.hp_lost != 3 || target.substitute_lost != 5)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    else if (*sequence == 2 && (target.flags != (RHH_MOVE_RESOLUTION_DAMAGE_RECORDED | RHH_MOVE_RESOLUTION_DAMAGE_TARGET_CHANGED)
                                || target.updates || target.hp_lost || target.substitute_lost)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    else if (*sequence == 3 && (target.flags != (RHH_MOVE_RESOLUTION_DAMAGE_RECORDED | RHH_MOVE_RESOLUTION_DAMAGE_MIXED_SOURCE)
                                || target.updates || target.hp_lost || target.substitute_lost)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    else if (*sequence == 4 && (target.flags != (RHH_MOVE_RESOLUTION_DAMAGE_RECORDED | RHH_MOVE_RESOLUTION_DAMAGE_ATTACKER_CHANGED)
                                || target.updates || target.hp_lost || target.substitute_lost)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    else if (*sequence == 5 && (target.flags != RHH_MOVE_RESOLUTION_DAMAGE_RECORDED || target.updates != 1 || target.hp_lost != 5
        || sMoves->resolution_damage(3, &other) || other.flags != RHH_MOVE_RESOLUTION_DAMAGE_RECORDED
        || other.updates != 1 || other.hp_lost != 4 || other.substitute_lost != 2)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    else if (*sequence == 6 && (target.flags || target.updates || target.hp_lost || target.substitute_lost))
        puts("MOVE_DAMAGE_SUMMARY_BAD");
    else if (*sequence > 26) puts("MOVE_DAMAGE_SUMMARY_BAD");
    struct RhhMoveResolutionHealth health = {.struct_size = sizeof(health)}, tinyHealth = {.struct_size = 1, .battler = 99}, oldHealth = tinyHealth;
    if (sMoves->resolution_health(1, &health) || !sMoves->resolution_health(1, &tinyHealth)
        || memcmp(&tinyHealth, &oldHealth, sizeof(tinyHealth)) || !sMoves->resolution_health(4, &tinyHealth)
        || health.battler != 1) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence < 7 && (health.flags || health.direct_updates || health.scripted_updates)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 7 && (health.flags != RHH_MOVE_HEALTH_RECORDED || health.direct_updates != 2
        || health.direct_lost != 6 || health.direct_restored != 5 || health.scripted_updates != 2
        || health.scripted_lost != 7 || health.scripted_restored != 26)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence >= 8 && *sequence <= 10)
    {
        unsigned expected = *sequence == 8 ? RHH_MOVE_HEALTH_SOURCE_CHANGED :
            *sequence == 9 ? RHH_MOVE_HEALTH_TARGET_CHANGED : RHH_MOVE_HEALTH_MIXED_MOVE;
        if (health.flags != (RHH_MOVE_HEALTH_RECORDED | expected) || health.direct_updates || health.scripted_updates
            || health.direct_lost || health.direct_restored || health.scripted_lost || health.scripted_restored)
            puts("MOVE_DAMAGE_SUMMARY_BAD");
    }
    if (*sequence == 11 && (health.flags || health.direct_updates || health.scripted_updates)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 12 && (health.flags != RHH_MOVE_HEALTH_RECORDED || health.direct_updates
        || health.scripted_updates != 1 || health.scripted_restored != 4 || health.scripted_lost))
        puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence >= 13 && *sequence <= 15)
    {
        struct RhhMoveResolutionHealth attacker = {.struct_size = sizeof(attacker)};
        if (sMoves->resolution_health(0, &attacker) || health.flags || health.direct_updates || health.scripted_updates)
            puts("MOVE_DAMAGE_SUMMARY_BAD");
        if (*sequence == 13 && (attacker.flags != RHH_MOVE_HEALTH_RECORDED || attacker.direct_updates
            || attacker.scripted_updates != 1 || attacker.scripted_lost != 23 || attacker.scripted_restored))
            puts("MOVE_DAMAGE_SUMMARY_BAD");
        if (*sequence == 14 && (attacker.flags || attacker.direct_updates || attacker.scripted_updates))
            puts("MOVE_DAMAGE_SUMMARY_BAD");
        if (*sequence == 15 && (attacker.flags != (RHH_MOVE_HEALTH_RECORDED | RHH_MOVE_HEALTH_SOURCE_CHANGED)
            || attacker.direct_updates || attacker.scripted_updates || attacker.scripted_lost || attacker.scripted_restored))
            puts("MOVE_DAMAGE_SUMMARY_BAD");
    }
    struct RhhMoveHealthComparison comparison = {.struct_size = sizeof(comparison)};
    struct RhhMoveHealthComparison smallComparison = {.struct_size = 1, .start_hp = 999}, savedComparison = smallComparison;
    if (sMoves->resolution_health_comparison(1, &comparison) || comparison.battler != 1
        || !sMoves->resolution_health_comparison(1, &smallComparison) || memcmp(&smallComparison, &savedComparison, sizeof(smallComparison))
        || !sMoves->resolution_health_comparison(4, &smallComparison)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    const unsigned endpoints = RHH_MOVE_HEALTH_START_AVAILABLE | RHH_MOVE_HEALTH_END_AVAILABLE;
    if (*sequence == 7 && (comparison.flags != (endpoints | RHH_MOVE_HEALTH_NET_COMPARABLE)
        || comparison.start_hp != 10 || comparison.end_hp != 28 || comparison.net_hp_change != 18
        || comparison.recorded_hp_change != 18)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 8 && !(comparison.flags & RHH_MOVE_HEALTH_ACTOR_CHANGED)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 9 && !(comparison.flags & RHH_MOVE_HEALTH_PARTICIPANT_CHANGED)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 10 && !(comparison.flags & RHH_MOVE_HEALTH_MOVE_CHANGED)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 16 && (comparison.flags != (endpoints | RHH_MOVE_HEALTH_NET_COMPARABLE | RHH_MOVE_HEALTH_NET_UNEXPLAINED)
        || comparison.start_hp != 20 || comparison.end_hp != 14 || comparison.net_hp_change != -6
        || comparison.recorded_hp_change)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 17 && (comparison.flags != (endpoints | RHH_MOVE_HEALTH_NET_COMPARABLE | RHH_MOVE_HEALTH_NET_UNEXPLAINED)
        || comparison.start_hp != 20 || comparison.end_hp != 19 || comparison.net_hp_change != -1
        || comparison.recorded_hp_change != -4)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 18 && comparison.flags != RHH_MOVE_HEALTH_START_AVAILABLE) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 19 && comparison.flags != (endpoints | RHH_MOVE_HEALTH_PARTICIPANT_CHANGED)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 20 && comparison.flags) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 21 && comparison.flags != (endpoints | RHH_MOVE_HEALTH_ACTOR_CHANGED)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 22 && (comparison.flags != (endpoints | RHH_MOVE_HEALTH_NET_COMPARABLE)
        || comparison.start_hp != 20 || comparison.end_hp != 20 || comparison.net_hp_change || comparison.recorded_hp_change))
        puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 23 && comparison.flags != RHH_MOVE_HEALTH_END_AVAILABLE) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if (*sequence == 24 && comparison.flags != (endpoints | RHH_MOVE_HEALTH_MOVE_CHANGED)) puts("MOVE_DAMAGE_SUMMARY_BAD");
    if ((*sequence == 25 || *sequence == 26) && comparison.flags != (endpoints | RHH_MOVE_HEALTH_ACTOR_CHANGED))
        puts("MOVE_DAMAGE_SUMMARY_BAD");
    ++*sequence; puts("MOVE_DAMAGE_SUMMARY_OK");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOVE_RESOLVED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_MOVE_DAMAGE_APPLIED, RHH_OBSERVE, 0, NULL, ObserveHit}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 45), RHH_SERVICE_REQUIRED, 0, "rhh:service/moves"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:move_damage_summary", .state_size = sizeof(unsigned), .hook_count = 2, .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 45));
    return sMoves && sMoves->header.struct_size >= RHH_MOVE_SERVICE_V1_45_SIZE ? &sDefinition : NULL;
}
