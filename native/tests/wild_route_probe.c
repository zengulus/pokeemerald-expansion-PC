#include "mod_api.h"
#include <stdio.h>
static const struct RhhWildActorService *actors;
static const struct RhhWildBehaviorService *behavior;
static RhhContentKey owner, activity;
static unsigned mode;
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state; mode = (unsigned)event->args[0];
    if (mode == 7) {
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
        struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
        if (actors->at(1, &actor) || behavior->queue_at(actor.handle, &queue)) { puts("WILD_ROUTE_BAD_PREFIX"); return; }
        struct RhhWildRouteRequest request = {.struct_size = sizeof(request), .actor_handle = actor.handle,
            .target_x = 7, .target_y = 7};
        struct RhhWildRouteTrace trace = {.struct_size = sizeof(trace)};
        struct RhhWildRouteResult preview = {.struct_size = sizeof(preview)};
        struct RhhWildQueueRouteOptions options = {.struct_size = sizeof(options), .owner = owner,
            .expected_revision = queue.revision, .flags = RHH_WILD_QUEUE_ROUTE_PREFIX};
        struct RhhWildQueueRouteResult result = {.struct_size = sizeof(result)};
        if (actors->route_trace(&request, &preview, &trace) || trace.count <= RHH_WILD_QUEUE_CAPACITY
            || behavior->queue_route(&options, &request, &result) || result.route.length != trace.count
            || result.change.after.count != RHH_WILD_QUEUE_CAPACITY) { puts("WILD_ROUTE_BAD_PREFIX"); return; }
        for (unsigned i = 0; i < RHH_WILD_QUEUE_CAPACITY; ++i)
            if (result.change.after.actions[i] != trace.steps[i].direction) { puts("WILD_ROUTE_BAD_PREFIX"); return; }
        puts("WILD_ROUTE_PREFIX_EXTERNAL_OK");
        return;
    }
    if (mode != 2) return;
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
    if (actors->at(1, &actor) || behavior->queue_at(actor.handle, &queue)) { puts("WILD_ROUTE_BAD_QUEUE"); return; }
    struct RhhWildRouteRequest request = {.struct_size = sizeof(request), .actor_handle = actor.handle,
        .target_x = 6, .target_y = 3, .stop_distance = 1};
    struct RhhWildQueueRouteOptions options = {.struct_size = sizeof(options), .owner = owner, .expected_revision = queue.revision};
    struct RhhWildQueueRouteResult result = {.struct_size = sizeof(result)};
    if (behavior->queue_route(&options, &request, &result) || result.route.length != 4
        || result.change.after.count != 4 || result.change.after.actions[0] != RHH_WILD_ACTOR_NORTH)
    { puts("WILD_ROUTE_BAD_QUEUE"); return; }
    struct RhhWildQueueRequest clear = {.struct_size = sizeof(clear), .actor_handle = actor.handle,
        .owner = owner, .expected_revision = result.change.after.revision};
    struct RhhWildQueueChange change = {.struct_size = sizeof(change)};
    if (behavior->queue_replace(&clear, &change) || change.after.count) puts("WILD_ROUTE_BAD_QUEUE");
    else puts("WILD_ROUTE_QUEUE_EXTERNAL_OK");
}
static void Collect(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhWildBehaviorContext context = {.struct_size = sizeof(context)};
    if (behavior->context(&context) || context.actor.slot != 1) return;
    if (mode == 4 || mode == 5 || mode == 6) {
        struct RhhWildActivity urgent = {.struct_size = sizeof(urgent), .id = activity, .owner = owner,
            .action = mode == 5 ? RHH_WILD_BEHAVIOR_NORTH : RHH_WILD_BEHAVIOR_NONE,
            .weight = 1, .interval = 1, .priority = mode == 6 ? 1 : 10};
        if (behavior->offer(&urgent)) puts("WILD_ROUTE_BAD_INTERRUPT_OFFER");
        return;
    }
    if (mode != 1) return;
    struct RhhWildRouteRequest request = {.struct_size = sizeof(request), .actor_handle = context.actor.handle,
        .target_x = 6, .target_y = 3, .stop_distance = 1};
    struct RhhWildRouteResult route = {.struct_size = sizeof(route)};
    if (actors->route(&request, &route) || route.status != RHH_WILD_ROUTE_FOUND
        || route.first_direction != RHH_WILD_ACTOR_NORTH || route.length != 4)
    { puts("WILD_ROUTE_BAD_PREVIEW"); return; }
    struct RhhWildRouteTrace trace = {.struct_size = sizeof(trace)};
    if (actors->route_trace(&request, &route, &trace) || trace.count != 4
        || trace.steps[0].direction != RHH_WILD_ACTOR_NORTH || trace.steps[0].x != 3 || trace.steps[0].y != 2
        || trace.steps[3].x != 6 || trace.steps[3].y != 2)
    { puts("WILD_ROUTE_BAD_TRACE"); return; }
    puts("WILD_ROUTE_TRACE_OK");
    struct RhhWildActivity step = {.struct_size = sizeof(step), .id = activity, .owner = owner,
        .action = route.first_direction, .weight = 1, .interval = 1};
    if (behavior->offer(&step)) puts("WILD_ROUTE_BAD_OFFER");
    else puts("WILD_ROUTE_ACTIVITY_OK");
}
static void Proposed(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (mode < 4 || mode > 6) return;
    struct RhhWildBehaviorContext context = {.struct_size = sizeof(context)};
    struct RhhWildActivitySelection selected = {.struct_size = sizeof(selected)};
    if (behavior->context(&context) || context.actor.slot != 1 || behavior->selected(&selected)) return;
    if ((mode == 6 && (selected.valid || decision->value != RHH_WILD_BEHAVIOR_NORTH))
        || (mode != 6 && (!selected.valid || selected.activity.priority != 10
            || decision->value != (mode == 5 ? RHH_WILD_BEHAVIOR_NORTH : RHH_WILD_BEHAVIOR_NONE))))
        puts("WILD_ROUTE_BAD_INTERRUPT_SELECTION");
    else printf("WILD_ROUTE_INTERRUPT_%u_OK\n", mode);
}
static void Result(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhWildBehaviorContext context = {.struct_size = sizeof(context)};
    if (behavior->context(&context) || context.actor.slot != 1) return;
    if (mode == 5 || mode == 6) {
        if (context.action != RHH_WILD_BEHAVIOR_NORTH || context.status != RHH_WILD_ACTOR_OK)
            puts("WILD_ROUTE_BAD_INTERRUPT_RESULT");
        return;
    }
    if (mode == 3) {
        struct RhhWildQueueRouteState guard = {.struct_size = sizeof(guard)};
        if (context.status != RHH_WILD_ACTOR_ROUTE_CHANGED || behavior->queue_route_state(context.actor.handle, &guard)
            || !guard.guarded || guard.status != context.status
            || (guard.reason != RHH_WILD_ROUTE_GUARD_ORIGIN_CHANGED && guard.reason != RHH_WILD_ROUTE_GUARD_LANDING_CHANGED))
            puts("WILD_ROUTE_BAD_GUARD");
        else puts("WILD_ROUTE_GUARD_EXTERNAL_OK");
        return;
    }
    if (mode != 1) return;
    if (context.action != RHH_WILD_ACTOR_NORTH || context.status != RHH_WILD_ACTOR_OK)
        printf("WILD_ROUTE_BAD_COMMIT action=%u status=%u\n", context.action, context.status);
    else puts("WILD_ROUTE_COMMIT_OK");
}
static void QueueConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhWildQueueGoalChange goal = {.struct_size = sizeof(goal)};
    if (behavior->queue_goal_context(&goal)) puts("WILD_ROUTE_BAD_GOAL_CONTEXT");
    if (mode == 8 && goal.after.kind) decision->allowed = 0;
}
static void QueueChanged(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhWildQueueGoalChange goal = {.struct_size = sizeof(goal)};
    if (behavior->queue_goal_context(&goal) || goal.after.revision != goal.before.revision + 1)
        puts("WILD_ROUTE_BAD_GOAL_CONTEXT");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update},
    {sizeof(struct RhhModHook), RHH_WILD_ACTIVITY_COLLECT, RHH_CONSTRAINT, 0, Collect, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_BEHAVIOR_PROPOSE, RHH_TRANSFORM, 0, Proposed, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_BEHAVIOR_RESULT, RHH_OBSERVE, 0, NULL, Result},
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_WILD_QUEUE, RHH_CONSTRAINT, 0, QueueConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_QUEUE_CHANGED, RHH_OBSERVE, 0, NULL, QueueChanged}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 9), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_actor"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 9), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_behavior"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:wild_route", .hooks = hooks, .hook_count = 6, .service_requirements = requirements, .service_requirement_count = 2};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->content_key || !host->get_service) return NULL;
    actors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 9));
    behavior = (const void *)host->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 9));
    owner = host->content_key(definition.id); activity = host->content_key("test:activity/route");
    return actors && actors->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_9_SIZE && behavior
        && behavior->header.struct_size >= RHH_WILD_BEHAVIOR_SERVICE_V1_9_SIZE && owner && activity ? &definition : NULL;
}
