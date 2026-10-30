/* An independently built mod drives a real spawn and removal on an update. */
#include "mod_api.h"
#include <stdio.h>

static const struct RhhWildActorService *sActors;
static const struct RhhOverworldService *sWorld;
static const struct RhhWildBehaviorService *sBehavior;
static RhhContentKey sOwner, sActivity, sSpecies;
static unsigned sStarted, sSpawned, sRemoved;

static void Activity(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    struct RhhWildActivity activity = {.struct_size = sizeof(activity), .action = RHH_WILD_BEHAVIOR_WEST,
        .weight = 1, .interval = 8, .id = sActivity, .owner = sOwner};
    if (sBehavior->offer(&activity)) puts("WILD_ACTOR_COMMAND_BAD");
}
static void ActivityChanged(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildActivityChange context = {.struct_size = sizeof(context)};
    if (sBehavior->change_context(&context) || event->event_key != sActivity
        || (event->ordinal == 1 && (context.before.valid || !context.after.valid
            || context.after.id != sActivity || context.after.action != RHH_WILD_BEHAVIOR_WEST))
        || (event->ordinal == 5 && (!context.before.valid || context.before.id != sActivity || context.after.valid))
        || (event->ordinal != 1 && event->ordinal != 5)) puts("WILD_ACTOR_COMMAND_BAD");
    else printf("WILD_ACTIVITY_CHANGE_%u_OK\n", (unsigned)event->ordinal);
}

static void Behavior(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildBehaviorContext c = {.struct_size = sizeof(c)};
    if (sBehavior->context(&c) || c.phase != RHH_WILD_BEHAVIOR_PROPOSAL || c.step.step != event->ordinal
        || !c.individual.serial || c.individual.run_id != c.step.run_id
        || sActors->step(c.actor.handle, RHH_WILD_ACTOR_WEST) != RHH_WILD_ACTOR_UNAVAILABLE)
    { puts("WILD_ACTOR_COMMAND_BAD"); return; }
    switch (event->ordinal)
    {
    case 1: case 2: case 7: decision->value = RHH_WILD_BEHAVIOR_WEST; break;
    case 3: case 5: decision->value = RHH_WILD_BEHAVIOR_EAST; break;
    case 4: decision->value = 999; break;
    case 6: decision->value = RHH_WILD_BEHAVIOR_ENGAGE; break;
    case 8: decision->value = RHH_WILD_BEHAVIOR_NONE; break;
    }
}
static void BehaviorConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (event->ordinal == 3 || event->ordinal == 9) decision->allowed = 0;
    decision->value = RHH_WILD_BEHAVIOR_NONE;
}
static void BehaviorResult(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildBehaviorContext c = {.struct_size = sizeof(c)};
    uint32_t expected = event->ordinal == 1 || event->ordinal == 5 || event->ordinal == 10 ? RHH_WILD_ACTOR_OK
        : event->ordinal == 3 || event->ordinal == 9 ? RHH_WILD_ACTOR_DENIED
        : event->ordinal == 7 ? RHH_WILD_ACTOR_UNAVAILABLE : RHH_WILD_ACTOR_BLOCKED;
    if (sBehavior->context(&c) || c.phase != RHH_WILD_BEHAVIOR_REPORT || c.step.step != event->ordinal
        || c.status != expected || c.action != event->args[0] || c.status != event->args[1])
    { puts("WILD_ACTOR_COMMAND_BAD"); return; }
    printf("WILD_BEHAVIOR_%u_OK\n", (unsigned)event->ordinal);
}

static void Event(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sStarted) return;
    if (event->kind == RHH_WILD_ACTOR_BATTLE_ENDED)
    {
        struct RhhWildActorBattleResult result = {.struct_size = sizeof(result)};
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
        if (sActors->battle_result(&result) || result.actor.handle != event->ordinal
            || result.outcome != RHH_WILD_BATTLE_CAUGHT || !sActors->find(event->ordinal, &actor))
        { puts("WILD_ACTOR_COMMAND_BAD"); return; }
        puts("WILD_ACTOR_BATTLE_RESULT_EXTERNAL_OK");
        return;
    }
    struct RhhWildActorEvent context = {.struct_size = sizeof(context)};
    if (sActors->context(&context) || sActors->remove(context.actor.handle) != RHH_WILD_ACTOR_UNAVAILABLE)
    { puts("WILD_ACTOR_COMMAND_BAD"); return; }
    if (event->kind == RHH_WILD_ACTOR_SPAWNED) ++sSpawned;
    else if (event->kind == RHH_WILD_ACTOR_REMOVED) ++sRemoved;
}
static void StepFinished(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildStepResult result = {.struct_size = sizeof(result)};
    if (sActors->step_context(&result) || result.ticket != event->ordinal || result.status != event->args[0]
        || result.source != event->args[1] || result.direction != event->args[2]
        || result.after_valid != event->args[3] || result.before.map != event->event_key
        || sActors->step(result.before.handle, result.direction) != RHH_WILD_ACTOR_UNAVAILABLE)
    { puts("WILD_ACTOR_COMMAND_BAD"); return; }
    if (result.status == RHH_WILD_STEP_REMOVED && result.source == RHH_WILD_STEP_DIRECT && !result.after_valid)
        puts("WILD_STEP_REMOVED_EXTERNAL_OK");
    if (result.status == RHH_WILD_STEP_ARRIVED && result.source == RHH_WILD_STEP_QUEUE && result.queue_consumed)
        puts("WILD_STEP_QUEUE_EXTERNAL_OK");
}
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    if (sStarted) return;
    struct RhhWorldActor player = {.struct_size = sizeof(player)};
    unsigned slot;
    for (slot = 0; slot < sWorld->actor_slots(); ++slot)
        if (!sWorld->actor_at(slot, &player) && (player.flags & RHH_ACTOR_PLAYER)) break;
    if (slot == sWorld->actor_slots()) { puts("WILD_ACTOR_COMMAND_BAD"); return; }
    struct RhhWildActorSpeciesSpawnRequest request = {.struct_size = sizeof(request), .map = player.map,
        .x = player.x + 2, .y = player.y + 2, .species = sSpecies, .level = 17};
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildActorCapacity before = {.struct_size = sizeof(before)}, after = {.struct_size = sizeof(after)};
    if (sActors->capacity(&before) || !before.commands_allowed || !before.automatic_enabled
        || before.map != player.map || before.generated_count >= before.generated_limit
        || before.next_generated_slot >= before.generated_limit) { puts("WILD_ACTOR_COMMAND_BAD"); return; }
    struct RhhWildActorSpawnRequest tile = {.struct_size = sizeof(tile), .map = request.map, .x = request.x, .y = request.y};
    struct RhhWildActorSpawnPreflight preflight = {.struct_size = sizeof(preflight)};
    if (sActors->spawn_preflight(&tile, &preflight) || preflight.status != RHH_WILD_ACTOR_OK
        || preflight.area != RHH_WILD_LAND || preflight.generated_slot != before.next_generated_slot)
    { puts("WILD_ACTOR_COMMAND_BAD"); return; }
    sStarted = 1;
    if (sActors->spawn_species(&request, &actor) != RHH_WILD_ACTOR_OK || !sSpawned
        || sActors->capacity(&after) || after.generated_count != before.generated_count + 1
        || after.object_count != before.object_count + 1
        || sActors->spawn_preflight(&tile, &preflight) || preflight.status != RHH_WILD_ACTOR_BLOCKED
        || actor.species != sSpecies || actor.level != 17
        || !(actor.flags & RHH_WILD_ACTOR_PRESERVED)
        || sActors->engage(actor.handle) != RHH_WILD_ACTOR_BLOCKED
        || sActors->step(actor.handle, RHH_WILD_ACTOR_SOUTH) != RHH_WILD_ACTOR_OK
        || sActors->remove(actor.handle) != RHH_WILD_ACTOR_OK || !sRemoved)
    { puts("WILD_ACTOR_COMMAND_BAD"); return; }
    puts("WILD_ACTOR_COMMAND_EXTERNAL_OK");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_SPAWNED, RHH_OBSERVE, 0, NULL, Event},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_REMOVED, RHH_OBSERVE, 0, NULL, Event},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_STEP_FINISHED, RHH_OBSERVE, 0, NULL, StepFinished},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_BATTLE_ENDED, RHH_OBSERVE, 0, NULL, Event},
    {sizeof(struct RhhModHook), RHH_WILD_BEHAVIOR_PROPOSE, RHH_TRANSFORM, 0, Behavior, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_BEHAVIOR_PROPOSE, RHH_CONSTRAINT, 0, BehaviorConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_BEHAVIOR_RESULT, RHH_OBSERVE, 0, NULL, BehaviorResult},
    {sizeof(struct RhhModHook), RHH_WILD_ACTIVITY_COLLECT, RHH_CONSTRAINT, 0, Activity, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_ACTIVITY_CHANGED, RHH_OBSERVE, 0, NULL, ActivityChanged}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 12), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_actor"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/overworld"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_behavior"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:wild_actor_command", .hook_count = 10, .hooks = sHooks,
    .service_requirement_count = 3, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service || !host->content_key) return NULL;
    sOwner = host->content_key(sDefinition.id); sActivity = host->content_key("test:activity/west");
    sSpecies = host->content_key("rhh:species/treecko");
    sActors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 12));
    sWorld = (const void *)host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 0));
    sBehavior = (const void *)host->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 2));
    return sActors && sActors->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_12_SIZE && sSpecies
        && sWorld && sWorld->header.struct_size >= RHH_OVERWORLD_SERVICE_V1_0_SIZE
        && sBehavior && sOwner && sActivity ? &sDefinition : NULL;
}
