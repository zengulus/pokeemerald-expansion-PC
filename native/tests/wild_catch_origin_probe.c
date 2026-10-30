#include "mod_api.h"
#include <stdio.h>
#include <string.h>

struct CatchOriginProbeState { uint32_t count, linked; struct RhhWildCatchOrigin last;
    uint32_t ended, ended_linked; struct RhhWildActorBattleCatch battle_catch;
    uint32_t started, started_linked; struct RhhWildActorBattleView battle_actor;
    uint32_t battle_ended, battle_ended_linked; struct RhhWildActorBattleView end_actor; };
static const struct RhhWildActorService *sWild;
#if !defined(BATTLE_ONLY) && !defined(VIEW_ONLY)
static const struct RhhOwnedMonService *sOwned;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct CatchOriginProbeState *probe = state;
    struct RhhCatchPlacement placement = {.struct_size = sizeof(placement)};
    struct RhhWildCatchOrigin origin = {.struct_size = sizeof(origin)}, tiny = {.struct_size = 1, .flags = 99}, saved = tiny;
    if (sOwned->catch_placement_context(&placement) || placement.cause != RHH_CATCH_ACQUIRED
        || event->ordinal != placement.sequence || !sWild->catch_origin(&tiny) || memcmp(&tiny, &saved, sizeof(tiny)))
    { puts("WILD_CATCH_ORIGIN_BAD"); return; }
    ++probe->count;
    if (!sWild->catch_origin(&origin)) {
        if (origin.placement.sequence != placement.sequence || origin.placement.run_id != placement.run_id
            || origin.placement.after.identity != placement.after.identity || !origin.actor.handle
            || ((origin.flags & RHH_WILD_CATCH_INDIVIDUAL_AVAILABLE) && (!origin.individual.run_id || !origin.individual.serial)))
        { puts("WILD_CATCH_ORIGIN_BAD"); return; }
        ++probe->linked; probe->last = origin;
    }
}
#endif
#ifndef VIEW_ONLY
static void ObserveEnd(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision;
    struct CatchOriginProbeState *probe = state;
    struct RhhWildActorBattleCatch caught = {.struct_size = sizeof(caught)}, tiny = {.struct_size = 1, .reserved = 99}, saved = tiny;
    if (!sWild->battle_catch(&tiny) || memcmp(&tiny, &saved, sizeof(tiny))) { puts("WILD_CATCH_ORIGIN_BAD"); return; }
    ++probe->ended;
    if (!sWild->battle_catch(&caught)) { ++probe->ended_linked; probe->battle_catch = caught; }
}
#endif
static void ObserveStart(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision;
    struct CatchOriginProbeState *probe = state;
    struct RhhWildActorBattleView view = {.struct_size = sizeof(view)};
    ++probe->started;
    if (!sWild->battle_actor(&view)) { ++probe->started_linked; probe->battle_actor = view; }
}
#ifdef VIEW_ONLY
static void ObserveBattleEnd(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision;
    struct CatchOriginProbeState *probe = state;
    struct RhhWildActorBattleView view = {.struct_size = sizeof(view)};
    ++probe->battle_ended;
    if (!sWild->battle_actor(&view)) { ++probe->battle_ended_linked; probe->end_actor = view; }
}
#endif
static const struct RhhModHook hooks[] = {
#if !defined(BATTLE_ONLY) && !defined(VIEW_ONLY)
    {sizeof(struct RhhModHook), RHH_OWNED_CATCH_PLACED, RHH_OBSERVE, 0, NULL, Observe},
#endif
#ifndef VIEW_ONLY
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_BATTLE_ENDED, RHH_OBSERVE, 0, NULL, ObserveEnd},
#endif
#ifdef VIEW_ONLY
    {sizeof(struct RhhModHook), RHH_BATTLE_ENDED, RHH_OBSERVE, 0, NULL, ObserveBattleEnd},
#endif
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, ObserveStart}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 17), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_actor"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 10), RHH_SERVICE_REQUIRED, 0, "rhh:service/owned_mon"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:wild_catch_origin",
    .state_size = sizeof(struct CatchOriginProbeState), .hooks = hooks, .hook_count = sizeof(hooks) / sizeof(hooks[0]),
    .service_requirements = requirements, .service_requirement_count = 2
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    sWild = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 17));
#if !defined(BATTLE_ONLY) && !defined(VIEW_ONLY)
    sOwned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 10));
    if (!sOwned) return NULL;
#endif
    return sWild && sWild->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_17_SIZE ? &definition : NULL;
}
