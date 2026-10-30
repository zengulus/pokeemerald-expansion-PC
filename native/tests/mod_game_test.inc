/* Real engine integration: transformations, caps, item use and save recovery. */
#include "mods.h"
#include "mod_host.h"
#include "native.h"
#include "item.h"
#include "money.h"
#include "caps.h"
#include "save.h"
#include "data.h"
#include "battle_setup.h"
#include "battle_frontier.h"
#include "battle_main.h"
#include "safari_zone.h"
#include "starter_choose.h"
#include "constants/maps.h"
#include "constants/field_effects.h"
#include "constants/rgb.h"
extern const struct SpriteTemplate *const gFieldEffectObjectTemplatePointers[];
#include "script_pokemon_util.h"
#include "mod_music.h"
#include "native_audio.h"
#include "sound.h"
#include "m4a.h"
#include "constants/songs.h"
#include "wild_encounter.h"
#include "dexnav.h"
#include "wild_encounter_ow.h"
#include "roamer.h"
#include "event_object_movement.h"
#include "pokemon_storage_system.h"
#include "fieldmap.h"
#include "field_player_avatar.h"
#include "metatile_behavior.h"
#include "field_weather.h"
#include "fishing.h"
#include "rtc.h"
#include "fake_rtc.h"
#include "field_camera.h"
#include "native_speed.h"
#include "daycare.h"
#include "egg_hatch.h"
#include "event_data.h"
#include "string_util.h"
#include "constants/items.h"
#include "constants/game_stat.h"
#include "constants/layouts.h"
#include "constants/moves.h"
#include "constants/weather.h"
#include "constants/battle_util.h"
#include "constants/battle_script_commands.h"
#include "constants/battle_anim.h"
#include "constants/metatile_behaviors.h"
#include "constants/map_types.h"
#include "constants/abilities.h"
#include "constants/item.h"
#include "config/wild_encounter.h"
#include "constants/trainer_types.h"
#include "constants/tv.h"
#include "battle_script_commands.h"
#include <math.h>
#include "../mods/bundled.h"
#include "../third_party/gme/gme.h"
#undef byte

static void CheckExampleMigrations(void)
{
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    struct RhhModHostV1 legacy = *host; legacy.struct_size = RHH_MOD_HOST_V1_BASE_SIZE;
    CHECK(RhhBundledBattleMusic(&legacy) != NULL);
    CHECK(RhhBundledBattleMusic(host) != NULL);
    const struct RhhModDefinition *wild = RhhBundledWildRandomizer(RhhModsHostAPI());
    const struct RhhModDefinition *nuz = RhhBundledNuzlocke(RhhModsHostAPI());
    u8 oldWild[16] = {2}, oldNuz[1312] = {1, 1, 0, 1, 2, 0, 1, 0};
    u8 *state = malloc(wild->state_size);
    CHECK(state != NULL); oldWild[8] = 42;
    CHECK(wild->migrate(1, oldWild, sizeof(oldWild), state, wild->state_size) == 0);
    CHECK(wild->validate(state, wild->state_size) == 0 && state[0] == 2 && state[8] == 42);
    CHECK(state[2] || state[3]); /* Frozen roster was populated. */
    CHECK(wild->migrate(1, oldWild, sizeof(oldWild) - 1, state, wild->state_size) != 0);
    oldWild[0] = 3;
    CHECK(wild->migrate(1, oldWild, sizeof(oldWild), state, wild->state_size) != 0);
    free(state); state = malloc(nuz->state_size); CHECK(state != NULL);
    oldNuz[32] = 0x11; oldNuz[40] = 0x22; oldNuz[288] = 0x33;
    CHECK(nuz->migrate(1, oldNuz, sizeof(oldNuz), state, nuz->state_size) == 0);
    CHECK(nuz->validate(state, nuz->state_size) == 0);
    CHECK(state[0] == 1 && state[1] == 1 && state[3] == 0 && state[4] == 2 && state[6] == 1);
    CHECK(state[32] == 0x11 && state[40] == 0x22 && state[4128] == 0x33);
    oldNuz[4] = 33;
    CHECK(nuz->migrate(1, oldNuz, sizeof(oldNuz), state, nuz->state_size) != 0);
    free(state);
    puts("example migrations: frozen wild roster and expanded Nuzlocke ledgers preserve state; malformed schemas rejected");
}
static void CheckSpeedSettings(void)
{
    const unsigned delays[] = {8, 4, 1, 1, 1};
    const unsigned modifiers[] = {1, 1, 1, TEXT_SPEED_INSTANT_MODIFIER, 2};
    unsigned oldSpeed = gSaveBlock2Ptr->optionsTextSpeed;
    bool8 oldForced = gTextFlags.forceMidTextSpeed;
    gTextFlags.forceMidTextSpeed = FALSE;
    for (unsigned i = 0; i < 5; ++i)
    {
        gSaveBlock2Ptr->optionsTextSpeed = i;
        CHECK(GetPlayerTextSpeed() == i && GetPlayerTextSpeedDelay() == delays[i]
            && GetPlayerTextSpeedModifier() == modifiers[i]);
        CHECK(!!IsPlayerTextSpeedInstant() == (i == OPTIONS_TEXT_SPEED_INSTANT));
    }
    CHECK(offsetof(struct SaveBlock2, pokedex) == 0x18);
    for (unsigned speed = 0; speed < 4; ++speed)
    {
        unsigned phase = 0, extra = 0;
        for (unsigned i = 0; i < 8; ++i) extra += NativeSpeedExtraSteps(speed, &phase);
        CHECK(extra == ((unsigned[]){0, 4, 8, 16})[speed] && !phase);
    }
    gSaveBlock2Ptr->optionsTextSpeed = oldSpeed; gTextFlags.forceMidTextSpeed = oldForced;
    puts("speed settings: five text rates, compatible save layout and four timing rates passed");
}

static const struct RhhMonEditService *sEditTestService;
static const struct RhhWorldEnvironmentService *sWorldEnvironmentTest;
static const struct RhhWorldEnvironmentRequest *sWorldEnvironmentRequest;
static struct RhhWorldEnvironmentResult sLastEnvironment;
static unsigned sEnvironmentConstraints, sEnvironmentNotifications;
static bool32 sEnvironmentDeny;
static const struct RhhWildActorService *sWildActorTest;
static struct RhhWildStepResult sLastWildStep;
static u32 sWildStepCount;
static void CheckWildStep(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sWildActorTest) return;
    struct RhhWildStepResult result = {.struct_size = sizeof(result)};
    struct RhhWildStepResult small = {.struct_size = 1, .ticket = 999};
    CHECK(!sWildActorTest->step_context(&result) && result.ticket == event->ordinal
        && result.status == event->args[0] && result.source == event->args[1]
        && result.direction == event->args[2] && result.after_valid == event->args[3]
        && result.before.map == event->event_key && result.before.species == event->subject
        && result.run_id == RhhModsRunId() && sWildActorTest->step_context(&small) && small.ticket == 999
        && !RhhModsCommandsAllowed());
    sLastWildStep = result; ++sWildStepCount;
}
static const struct RhhWildEncounterService *sWildGateService;
static bool32 sWildNestedCancelCheck;
static unsigned sWildNestedCancelCount;
static const struct RhhWildConcealService *sWildConcealTest;
static unsigned sConcealConstraints, sConcealNotifications, sConcealLastCause;
static bool32 sConcealDeny;
static void CheckConcealConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildConcealResult view = {.struct_size = sizeof(view)};
    CHECK(sWildConcealTest && !sWildConcealTest->context(&view) && view.actor.handle == event->ordinal
        && view.after.mode == event->args[1] && !RhhModsCommandsAllowed());
    ++sConcealConstraints;
    if (sConcealDeny) { decision->allowed = 0; strcpy(decision->reason, "test denied"); }
}
static void CheckConcealNotification(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhWildConcealResult view = {.struct_size = sizeof(view)};
    CHECK(sWildConcealTest && !sWildConcealTest->context(&view) && !view.is_preview
        && view.actor.handle == event->ordinal && view.cause == event->args[0]
        && !!(view.actor.flags & RHH_WILD_ACTOR_HIDDEN) == (view.after.mode == RHH_WILD_CONCEAL_HIDDEN));
    ++sConcealNotifications; sConcealLastCause = view.cause;
}
static unsigned sWildActorSpawned, sWildActorRemoved, sWildActorRestored, sWildActorEngaged, sWildActorIdentified;
static unsigned sWildActorBattleEnded;
static struct RhhWildActorBattleResult sLastWildBattle;
static struct RhhWildActorIndividual sLastWildEventIndividual, sLastWildBattleIndividual;
static struct RhhWildActorEvent sLastWildActor;
static bool32 sRejectWildActorCommands;
static unsigned sObservedPersistenceFailures;
static bool32 sTestNestedActorContext;
static unsigned sNestedActorContextChecks;
static void ObservePersistenceFailure(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state; ++sObservedPersistenceFailures;
    if (sWildNestedCancelCheck)
    {
        struct RhhWildEncounterContext context = {.struct_size = sizeof(context), .ordinal = 999};
        struct RhhWildEncounterCancellation cancellation = {.struct_size = sizeof(cancellation), .reason = "untouched"};
        CHECK(sWildGateService->context(&context) && context.ordinal == 999
            && sWildGateService->cancellation_context(&cancellation) && !strcmp(cancellation.reason, "untouched"));
        sWildNestedCancelCheck = FALSE; ++sWildNestedCancelCount;
    }
    if (sTestNestedActorContext)
    {
        struct RhhWildActorIndividual id = {.struct_size = sizeof(id), .serial = 999};
        struct RhhWildActorEvent actor = {.struct_size = sizeof(actor), .cause = 999};
        struct RhhWildActorBattleResult battle = {.struct_size = sizeof(battle), .outcome = 999};
        CHECK(sWildActorTest->event_individual(&id) && id.serial == 999
            && sWildActorTest->context(&actor) && actor.cause == 999
            && sWildActorTest->battle_result(&battle) && battle.outcome == 999);
        sTestNestedActorContext = FALSE; ++sNestedActorContextChecks;
    }
}
#if WE_OW_ENCOUNTERS
static bool32 sCaptureAutomaticWild;
static struct Pokemon sAutomaticWildMon;
static void CaptureAutomaticWild(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (sCaptureAutomaticWild && event->args[1] == RHH_WILD_OVERWORLD_ACTOR)
        sAutomaticWildMon = gParties[B_TRAINER_OPPONENT_A][0];
}
#endif
static void CheckWildActorEvent(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sWildActorTest) return;
    sLastWildActor.struct_size = sizeof(sLastWildActor);
    CHECK(!sWildActorTest->context(&sLastWildActor) && sLastWildActor.actor.map == event->event_key
        && sLastWildActor.actor.species == event->subject && sLastWildActor.actor.handle == event->ordinal
        && sLastWildActor.actor.slot == event->args[0] && sLastWildActor.cause == event->args[1]);
    struct RhhWildActorSnapshot current = {.struct_size = sizeof(current)};
    sLastWildEventIndividual = (struct RhhWildActorIndividual){.struct_size = sizeof(sLastWildEventIndividual)};
    (void)sWildActorTest->event_individual(&sLastWildEventIndividual);
    if (sLastWildActor.cause == RHH_WILD_ACTOR_SUSPENDED || sLastWildActor.cause == RHH_WILD_ACTOR_RESUMED)
    {
        const struct RhhWildDormantService *dormant = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/wild_dormant", RHH_SERVICE_VERSION(1, 0));
        struct RhhWildDormantActor record = {.struct_size = sizeof(record)};
        CHECK(dormant && sLastWildEventIndividual.run_id && sLastWildEventIndividual.serial);
        if (sLastWildActor.cause == RHH_WILD_ACTOR_SUSPENDED)
        {
            struct RhhWildActorSnapshot rejected = {.struct_size = sizeof(rejected)};
            CHECK(!dormant->find(&sLastWildEventIndividual, &record)
                && dormant->resume(&sLastWildEventIndividual, record.owner, &rejected) == RHH_WILD_ACTOR_UNAVAILABLE);
        }
        else
        {
            const struct RhhWildProfileService *profile = (const void *)RhhModsHostAPI()->get_service(
                "rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 0));
            struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
            RhhContentKey owner = RhhContentKeyOf("test:mon_edit_rules");
            CHECK(dormant->find(&sLastWildEventIndividual, &record) && profile
                && (!profile->at(event->ordinal, owner, RhhContentKeyOf("test:profile/dormant"), &value) && value.present
                    ? value.value == -73
                    : !profile->at(event->ordinal, owner, RhhContentKeyOf("test:profile/dormant_save"), &value)
                        && value.present && value.value == 1234));
        }
    }
    struct RhhWildActorEvent small = {.struct_size = 1, .cause = 999};
    CHECK(sWildActorTest->context(&small) && small.cause == 999);
    if (sRejectWildActorCommands)
        CHECK(sWildActorTest->remove(sLastWildActor.actor.handle) == RHH_WILD_ACTOR_UNAVAILABLE);
    if (event->kind == RHH_WILD_ACTOR_SPAWNED)
    {
        CHECK((sLastWildActor.cause == RHH_WILD_ACTOR_SPAWN || sLastWildActor.cause == RHH_WILD_ACTOR_RESTORE
            || sLastWildActor.cause == RHH_WILD_ACTOR_RESUMED)
            && !sWildActorTest->at(event->args[0], &current)
            && current.handle == sLastWildActor.actor.handle && !sWildActorTest->find(current.handle, &current));
        if (sLastWildActor.cause == RHH_WILD_ACTOR_RESTORE) ++sWildActorRestored;
        else ++sWildActorSpawned;
    }
    else if (event->kind == RHH_WILD_ACTOR_ENGAGED)
    {
        CHECK(sLastWildActor.cause == RHH_WILD_ACTOR_BATTLE && !sWildActorTest->at(event->args[0], &current)
            && current.handle == sLastWildActor.actor.handle && !sWildActorTest->find(current.handle, &current));
        ++sWildActorEngaged;
    }
    else if (event->kind == RHH_WILD_ACTOR_IDENTIFIED)
    {
        struct RhhWildActorIndividual id = {.struct_size = sizeof(id)}, live = id;
        struct RhhWildActorIndividual smallId = {.struct_size = 1, .serial = 999};
        CHECK(sLastWildActor.cause == RHH_WILD_ACTOR_ID_ASSIGNED
            && !sWildActorTest->event_individual(&id) && id.run_id && id.serial
            && !sWildActorTest->individual(event->ordinal, &live) && live.run_id == id.run_id && live.serial == id.serial
            && !sWildActorTest->find_individual(&id, &current) && current.handle == event->ordinal
            && sWildActorTest->event_individual(&smallId) && smallId.serial == 999);
        struct RhhWildActorSnapshot saved = current;
        id.reserved = 1;
        CHECK(sWildActorTest->find_individual(&id, &current) && !memcmp(&saved, &current, sizeof(current)));
        if (sTestNestedActorContext)
        {
            struct RhhModEvent nested = {.struct_size = sizeof(nested), .kind = RHH_PERSISTENCE_FAILED};
            struct RhhModDecision d = {.struct_size = sizeof(d), .allowed = 1};
            RhhModsDispatch(&nested, &d);
            CHECK(!sWildActorTest->event_individual(&live) && live.serial == id.serial);
        }
        ++sWildActorIdentified;
    }
    else
    {
        CHECK(event->kind == RHH_WILD_ACTOR_REMOVED && sWildActorTest->at(event->args[0], &current)
            && sWildActorTest->find(sLastWildActor.actor.handle, &current));
        ++sWildActorRemoved;
    }
}
static void CheckWildActorBattleEnd(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sWildActorTest) return;
    sLastWildBattle.struct_size = sizeof(sLastWildBattle);
    sLastWildBattleIndividual = (struct RhhWildActorIndividual){.struct_size = sizeof(sLastWildBattleIndividual)};
    (void)sWildActorTest->event_individual(&sLastWildBattleIndividual);
    CHECK(!sWildActorTest->battle_result(&sLastWildBattle)
        && sLastWildBattle.actor.handle == event->ordinal && sLastWildBattle.actor.map == event->event_key
        && sLastWildBattle.actor.species == event->subject && sLastWildBattle.outcome == event->args[0]
        && sLastWildBattle.flags == event->args[1]);
    struct RhhWildActorBattleResult small = {.struct_size = 1, .outcome = 999};
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildActorEvent actorEvent = {.struct_size = sizeof(actorEvent), .cause = 999};
    CHECK(sWildActorTest->battle_result(&small) && small.outcome == 999
        && sWildActorTest->context(&actorEvent) && actorEvent.cause == 999
        && sWildActorTest->find(event->ordinal, &actor)
        && sWildActorTest->remove(event->ordinal) == RHH_WILD_ACTOR_UNAVAILABLE);
    ++sWildActorBattleEnded;
}
static bool32 sWildGateDeny, sWildGateCheckRng, sWildGateTryUndeny, sWildGateTimeCheck;
static unsigned sWildGateCalls, sWildCancelCalls, sWildGateDenyAfter;
static unsigned sSpecialGateCalls, sSpecialCancelCalls, sSpecialGeneratedCalls;
static struct RhhWildEncounterContext sLastSpecialGenerated;
static bool32 sTrackWildFirst, sTrackEncounterStarts;
static unsigned sEncounterStartCount;
static u64 sFirstGeneratedIdentity;
static void CheckEncounterStartedForCancellation(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)event; (void)decision; (void)state; if (sTrackEncounterStarts) ++sEncounterStartCount; }
static rng_value_t sWildGateExpectedRng, sWildGateExpectedRng2;
static struct RhhWildEncounterCancellation sLastWildCancellation;
static void CheckWildGate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sWildGateService) return;
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context)};
    struct RhhWildEncounterCancellation unavailable = {.struct_size = sizeof(unavailable), .reason = "untouched"};
    CHECK(!sWildGateService->context(&context) && context.phase == RHH_WILD_PHASE_PROPOSAL
        && context.species == event->subject && context.species == decision->value && context.ordinal == event->ordinal
        && context.map == event->event_key && context.area == event->args[0] && context.source == event->args[1]
        && context.slot == event->args[2] && context.level == event->args[3]
        && sWildGateService->cancellation_context(&unavailable) && !strcmp(unavailable.reason, "untouched")
        && !RhhModsCommandsAllowed());
    if (sWildGateTimeCheck)
    {
        const struct RhhWorldTimeService *clock = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/world_time", RHH_SERVICE_VERSION(1, 0));
        struct RhhWorldTimeSnapshot sample = {.struct_size = sizeof(sample)};
        CHECK(clock && !clock->sample(&sample) && sample.hour == 21);
    }
    ++sWildGateCalls;
    if (event->kind == RHH_CAN_GENERATE_SPECIAL_WILD) ++sSpecialGateCalls;
    if (sWildGateDeny && sWildGateCalls > sWildGateDenyAfter)
    { decision->allowed = FALSE; strcpy(decision->reason, "habitat excludes candidate"); }
    decision->value = UINT64_MAX; /* Permission constraints cannot rewrite the selected species. */
}
static void CheckWildGateSticky(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sWildGateService && sWildGateDeny && sWildGateCalls > sWildGateDenyAfter && sWildGateTryUndeny)
    { CHECK(!decision->allowed && !strcmp(decision->reason, "habitat excludes candidate"));
      decision->allowed = TRUE; strcpy(decision->reason, "attempted override"); }
}
static void CheckWildCancelled(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state;
    if (!sWildGateService) return;
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context)};
    struct RhhWildEncounterCancellation small = {.struct_size = 1, .reason = "untouched"};
    sLastWildCancellation.struct_size = sizeof(sLastWildCancellation);
    CHECK(!sWildGateService->cancellation_context(&sLastWildCancellation)
        && !sWildGateService->context(&context) && context.phase == RHH_WILD_PHASE_CANCELLED
        && !memcmp(&context, &sLastWildCancellation.candidate, sizeof(context))
        && !decision->allowed && context.map == event->event_key && context.species == event->subject
        && context.ordinal == event->ordinal && !strcmp(decision->reason, "habitat excludes candidate")
        && !strcmp(sLastWildCancellation.reason, decision->reason)
        && sWildGateService->cancellation_context(&small) && !strcmp(small.reason, "untouched"));
    if (sWildGateCheckRng)
        CHECK(!memcmp(&gRngValue, &sWildGateExpectedRng, sizeof(gRngValue))
            && !memcmp(&gRng2Value, &sWildGateExpectedRng2, sizeof(gRng2Value)));
    if (sWildNestedCancelCheck)
    {
        struct RhhModEvent nested = {.struct_size = sizeof(nested), .kind = RHH_PERSISTENCE_FAILED};
        struct RhhModDecision nestedDecision = {.struct_size = sizeof(nestedDecision), .allowed = TRUE};
        RhhModsDispatch(&nested, &nestedDecision);
        CHECK(!sWildNestedCancelCheck && sWildNestedCancelCount == 1
            && !sWildGateService->cancellation_context(&sLastWildCancellation));
    }
    ++sWildCancelCalls;
    if (event->kind == RHH_WILD_SPECIAL_CANCELLED) ++sSpecialCancelCalls;
}
static void CheckWildSpecialGenerated(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sWildGateService) return;
    sLastSpecialGenerated.struct_size = sizeof(sLastSpecialGenerated);
    CHECK(!sWildGateService->context(&sLastSpecialGenerated)
        && sLastSpecialGenerated.phase == RHH_WILD_PHASE_CREATED
        && sLastSpecialGenerated.species == event->subject && sLastSpecialGenerated.map == event->event_key
        && sLastSpecialGenerated.source == event->args[1]);
    ++sSpecialGeneratedCalls;
}
static const struct RhhWildEncounterService *sWildTest;
static unsigned sWildProposals, sWildGenerated, sWildExpectedSource;
static s32 sWildExpectedX, sWildExpectedY;
static bool32 sWildReplace, sWildDeny, sWildInvalid, sWildTryUndeny, sWildAllowMissing;
static bool32 sWildCheckTable;
static unsigned sWeightMode, sWeightCalls, sWeightTarget;
static struct RhhWildEncounterContext sLastWild;
static void CheckWildWeight(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sWildTest || !sWeightMode) return;
    struct RhhWildWeightContext context = {.struct_size = sizeof(context)}, small = {.struct_size = 1, .ordinal = 999};
    CHECK(sWildTest->weight_context(&small) && small.ordinal == 999);
    CHECK(!sWildTest->weight_context(&context) && context.map == event->event_key
        && context.candidate_species == event->subject && context.ordinal == event->ordinal
        && context.area == event->args[0] && context.source == event->args[1]
        && context.slot == event->args[2] && context.rod == event->args[3]
        && context.source == sWildExpectedSource && context.x == sWildExpectedX && context.y == sWildExpectedY
        && context.base_weight == decision->value && context.slot >= context.first_slot
        && context.slot - context.first_slot < context.slot_count && context.habitat_available
        && context.habitat.environment.weather.value == RHH_WEATHER_RAIN);
    ++sWeightCalls;
    if (sWeightMode == 2) decision->value = context.slot == sWeightTarget ? 100 : 0;
    else if (sWeightMode == 3) decision->value = UINT64_MAX;
    else if (sWeightMode == 6) decision->value = context.slot == 0 ? 2 : context.slot == 1 ? 5 : 0;
    else if (sWeightMode == 7) decision->value = context.slot == sWeightTarget ? RHH_WILD_MAX_WEIGHT : 0;
    else if (sWeightMode == 8) decision->value = RHH_WILD_MAX_WEIGHT + 1;
}
static void CheckWildWeightConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sWildTest || !sWeightMode) return;
    if (sWeightMode == 4 || (sWeightMode == 5 && event->args[2] != sWeightTarget)) decision->allowed = 0;
    decision->value = UINT64_MAX;
}
static bool32 sReplaceRequestedWild;
static void CheckWildCandidate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (sReplaceRequestedWild && event->args[1] == RHH_WILD_OVERWORLD_ACTOR
        && event->args[2] == RHH_WILD_NO_TABLE_SLOT)
        decision->value = RhhContentKeyForId(RHH_SPECIES, SPECIES_ZIGZAGOON);
    if (!sWildTest) return;
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context)};
    struct RhhWildEncounterContext small = {.struct_size = 1, .ordinal = 999};
    CHECK(sWildTest->context(&small) && small.ordinal == 999);
    CHECK(!sWildTest->context(&context) && context.phase == RHH_WILD_PHASE_PROPOSAL
        && context.map == event->event_key && context.initial_species == event->subject
        && context.ordinal == event->ordinal && context.area == event->args[0]
        && context.source == sWildExpectedSource && context.source == event->args[1]
        && context.x == sWildExpectedX && context.y == sWildExpectedY
        && context.slot == event->args[2] && context.level == event->args[3]
        && (!sWildCheckTable || context.table_species == RhhContentKeyForId(RHH_SPECIES, SPECIES_POOCHYENA))
        && context.initial_species == decision->value
        && (sWildAllowMissing ? !context.habitat_available
            : context.habitat_available && context.habitat.x == sWildExpectedX && context.habitat.y == sWildExpectedY
              && context.habitat.map == context.map && context.habitat.environment.weather.value == RHH_WEATHER_RAIN));
    ++sWildProposals;
    if (sWildInvalid) decision->value = UINT64_MAX;
    else if (sWildReplace) decision->value = RhhContentKeyForId(RHH_SPECIES, SPECIES_ZIGZAGOON);
}
static void CheckWildConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (!sWildTest) return;
    if (sWildDeny) decision->allowed = 0;
    decision->value = UINT64_MAX; /* Constraints cannot rewrite the species. */
}
static void CheckWildFinalize(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sWildTest && sWildTryUndeny) decision->allowed = 1;
}
static void CheckWildGenerated(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (sTrackWildFirst && !sFirstGeneratedIdentity)
    {
        struct RhhWildEncounterContext first = {.struct_size = sizeof(first)};
        CHECK(sWildGateService && !sWildGateService->context(&first) && first.phase == RHH_WILD_PHASE_CREATED);
        sFirstGeneratedIdentity = first.identity;
    }
    if (!sWildTest) return;
    sLastWild.struct_size = sizeof(sLastWild);
    CHECK(!sWildTest->context(&sLastWild) && sLastWild.phase == RHH_WILD_PHASE_CREATED
        && sLastWild.map == event->event_key && sLastWild.species == event->subject
        && sLastWild.ordinal == event->ordinal && sLastWild.source == sWildExpectedSource
        && sLastWild.level == GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_LEVEL)
        && sLastWild.identity == ((u64)GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_OT_ID) << 32
                                  | GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_PERSONALITY))
        && sLastWild.habitat.x == sWildExpectedX && sLastWild.habitat.y == sWildExpectedY);
    ++sWildGenerated;
}
static const struct RhhHabitatService *sHabitatTest;
static unsigned sHabitatQueries;
static bool32 sHabitatTransform, sHabitatDeny, sHabitatInvalid, sTimeScopeTest, sTimeScopeSkip, sTimeScopeIgnore;
static bool32 sHabitatMutateTile;
static u32 sHabitatImpure;
static unsigned sTimeScopeChecks;
static const struct RhhWorldTimeService *sTimeScopeService;
static void CheckHabitatTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sHabitatTest) return;
    struct RhhHabitatSample context = {.struct_size = sizeof(context)}, nested = {.struct_size = sizeof(nested)};
    CHECK(!sHabitatTest->context(&context) && context.map == event->event_key
        && context.x == (s32)event->args[0] && context.y == (s32)event->args[1]
        && context.base_flags == decision->value && context.flags == context.base_flags && context.permitted);
    CHECK(sHabitatTest->sample(context.x, context.y, &nested));
    struct RhhHabitatSearchRequest nestedRequest = {.struct_size = sizeof(nestedRequest), .x = context.x, .y = context.y, .radius = 1};
    struct RhhHabitatSearchResult nestedSearch = {.struct_size = sizeof(nestedSearch), .scanned = 99}, originalSearch = nestedSearch;
    CHECK(sHabitatTest->search(&nestedRequest, &nestedSearch) && !memcmp(&nestedSearch, &originalSearch, sizeof(nestedSearch)));
    if (sTimeScopeTest && sTimeScopeSkip) { sTimeScopeSkip = FALSE; sTimeScopeIgnore = TRUE; }
    else if (sTimeScopeTest)
    {
        sTimeScopeIgnore = FALSE;
        struct RhhWorldTimeSnapshot first = {.struct_size = sizeof(first)}, second = {.struct_size = sizeof(second)};
        CHECK(!sTimeScopeService->sample(&first) && first.hour == 21);
        CHECK(!NativeRtcSetFixed("2024-06-02T04:15:25"));
        CHECK(!sTimeScopeService->sample(&second) && !memcmp(&first, &second, sizeof(first)));
        ++sTimeScopeChecks;
    }
    ++sHabitatQueries;
    if (sHabitatMutateTile) {
        sHabitatMutateTile = FALSE;
        gBackupMapLayout.map[(context.y + MAP_OFFSET) * gBackupMapLayout.width + context.x + MAP_OFFSET] ^= 1;
    }
    if (sHabitatImpure & 1) { (void)Random32(); (void)Random2_32(); }
    if (sHabitatInvalid) decision->value = UINT64_MAX;
    else if (sHabitatTransform && context.environment_available && context.environment.weather.value == RHH_WEATHER_RAIN)
        decision->value |= RHH_HABITAT_WATER;
}
static void CheckHabitatConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (!sHabitatTest) return;
    if (sTimeScopeTest && !sTimeScopeIgnore)
    {
        struct RhhWorldTimeSnapshot view = {.struct_size = sizeof(view)};
        CHECK(!sTimeScopeService->sample(&view) && view.hour == 21);
        ++sTimeScopeChecks;
    }
    if (sHabitatDeny) decision->allowed = 0;
    if (sHabitatImpure & 2) { (void)Random32(); (void)Random2_32(); }
    decision->value = UINT64_MAX; /* Constraints may deny, not transform flags. */
}
static u64 sSeedWeather, sSeedTerrain;
static unsigned sSeedTransforms, sSeedConstraints, sSeedDenyKind;
static u32 sSeedExpectedFlags;
static RhhContentKey sSeedExpectedTrainers[2];
static bool32 sSeedOverride, sSeedTryUndeny, sSeedImpure;
static void CheckBattleSeedTransform(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sWorldEnvironmentTest) return;
    struct RhhBattleEnvironmentSeed context = {.struct_size = sizeof(context)};
    CHECK(!sWorldEnvironmentTest->battle_seed_context(&context) && context.kind == event->args[0]
        && context.initial_value == event->args[1] && context.environment.map == event->event_key
        && context.environment.step == event->ordinal && decision->value == context.initial_value
        && context.battle_flags == sSeedExpectedFlags && context.opponent_trainers[0] == sSeedExpectedTrainers[0]
        && context.opponent_trainers[1] == sSeedExpectedTrainers[1]);
    struct RhhWorldEnvironmentSnapshot current = {.struct_size = sizeof(current)};
    CHECK(!sWorldEnvironmentTest->current(&current) && current.weather.value == context.environment.weather.value
        && current.terrain.value == context.environment.terrain.value);
    ++sSeedTransforms;
    if (sSeedImpure) { (void)Random32(); (void)Random2_32(); }
    if (sSeedOverride) decision->value = context.kind == RHH_WORLD_WEATHER ? sSeedWeather : sSeedTerrain;
}
static void CheckBattleSeedConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sWorldEnvironmentTest) return;
    ++sSeedConstraints;
    if (sSeedDenyKind == event->args[0]) decision->allowed = 0;
    decision->value = UINT64_MAX; /* Constraints may deny, but cannot rewrite a proposal. */
}
static void CheckBattleSeedFinalize(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sWorldEnvironmentTest && sSeedTryUndeny) decision->allowed = 1;
}
static void CheckEnvironmentConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sWorldEnvironmentTest) return;
    struct RhhWorldEnvironmentResult proposal = {.struct_size = sizeof(proposal)};
    CHECK(!sWorldEnvironmentTest->context(&proposal) && proposal.kind == event->args[0]
        && proposal.is_preview == event->args[1] && proposal.before.map == event->event_key);
    struct RhhWorldEnvironmentSnapshot current = {.struct_size = sizeof(current)};
    CHECK(!sWorldEnvironmentTest->at(proposal.before.map, &current)
        && current.weather.value == proposal.before.weather.value && current.terrain.value == proposal.before.terrain.value);
    struct RhhWorldEnvironmentResult nested = {.struct_size = sizeof(nested)};
    CHECK(sWorldEnvironmentTest->apply(sWorldEnvironmentRequest, &nested) == RHH_WORLD_ENV_UNAVAILABLE);
    ++sEnvironmentConstraints;
    if (sEnvironmentDeny) { decision->allowed = 0; strcpy(decision->reason, "environment denied"); }
}
static void CheckEnvironmentNotification(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sWorldEnvironmentTest) return;
    sLastEnvironment.struct_size = sizeof(sLastEnvironment);
    CHECK(!sWorldEnvironmentTest->context(&sLastEnvironment) && !sLastEnvironment.is_preview
        && event->event_key == sLastEnvironment.after.map && event->ordinal == sLastEnvironment.after.step
        && event->args[0] == sLastEnvironment.kind && event->args[1] == sLastEnvironment.cause);
    struct RhhWorldEnvironmentSnapshot current = {.struct_size = sizeof(current)};
    CHECK(!sWorldEnvironmentTest->at(sLastEnvironment.after.map, &current)
        && current.weather.value == sLastEnvironment.after.weather.value
        && current.terrain.value == sLastEnvironment.after.terrain.value);
    ++sEnvironmentNotifications;
}
static const struct RhhWorldClockService *sClockTestService;
static struct RhhWorldStepSnapshot sLastWorldStep;
static unsigned sWorldStepNotifications;
static void CheckWorldStepNotification(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sClockTestService) return;
    sLastWorldStep.struct_size = sizeof(sLastWorldStep);
    CHECK(!sClockTestService->step_context(&sLastWorldStep));
    struct RhhWorldClockSnapshot current = {.struct_size = sizeof(current)};
    CHECK(!sClockTestService->current(&current) && current.steps == event->ordinal
        && current.steps == sLastWorldStep.step && current.run_id == sLastWorldStep.run_id
        && event->event_key == sLastWorldStep.map);
    ++sWorldStepNotifications;
    ModsWorldStep(999, 999, 0, 0); /* Recursive notification cannot advance the clock. */
    CHECK(!sClockTestService->current(&current) && current.steps == event->ordinal);
}
static const struct RhhEggHatchService *sEggHatchTestService;
static struct RhhEggHatchContext sLastHatch;
static unsigned sHatchNotifications;
static void CheckHatchNotification(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sEggHatchTestService) return;
    sLastHatch.struct_size = sizeof(sLastHatch);
    CHECK(!sEggHatchTestService->context(&sLastHatch));
    CHECK(sLastHatch.egg.is_egg && !sLastHatch.hatched.is_egg);
    CHECK(event->subject == sLastHatch.egg.identity && event->ordinal == sLastHatch.hatched.identity);
    CHECK(event->event_key == sLastHatch.hatched.species && event->args[0] == sLastHatch.hatched.slot);
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    struct RhhOwnedMonSnapshot current = {.struct_size = sizeof(current)};
    CHECK(mons && !mons->party_at(sLastHatch.hatched.slot, &current)
        && current.identity == sLastHatch.hatched.identity && !current.is_egg);
    ++sHatchNotifications;
}
static const struct RhhBreedingService *sBreedingTestService;
static int sBreedingChoice = -1, sBreedingDeny;
static RhhContentKey sBreedingSpecies;
static unsigned sBreedingQueries, sBreedingEggs;
static unsigned sBreedingPreviewQueries;
static struct RhhBreedingContext sLastBreeding;
static int sInheritanceMode, sInheritanceMoveMode;
static unsigned sInheritanceCalls, sMoveCalls;
static u8 sInitialIVs[RHH_STAT_COUNT], sEngineIVs[RHH_STAT_COUNT];
static RhhContentKey sInheritanceMove;
static RhhContentKey sEngineMoves[4];
static void CheckBreedingProposal(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sBreedingTestService) return;
    struct RhhBreedingContext context = {.struct_size = sizeof(context)};
    CHECK(!sBreedingTestService->context(&context));
    CHECK(context.parents[0].mon.location == RHH_MON_DAYCARE && context.parents[1].mon.location == RHH_MON_DAYCARE);
    if (context.preview)
    {
        struct RhhBreedingPreview nested = {.struct_size = sizeof(nested), .candidate_count = 99}, unchanged = nested;
        CHECK(sBreedingTestService->preview(&nested) && !memcmp(&nested, &unchanged, sizeof(nested)));
        ++sBreedingPreviewQueries;
    }
    if (event->kind == RHH_BREED_COMPATIBILITY)
    {
        u8 *memory = malloc(RHH_BREEDING_CONTEXT_V1_0_SIZE + 8); CHECK(memory);
        memset(memory, 0xA5, RHH_BREEDING_CONTEXT_V1_0_SIZE + 8);
        struct RhhBreedingContext *old = (void *)memory;
        old->struct_size = RHH_BREEDING_CONTEXT_V1_0_SIZE;
        CHECK(!sBreedingTestService->context(old) && old->struct_size == RHH_BREEDING_CONTEXT_V1_0_SIZE
            && old->parents[0].mon.identity == context.parents[0].mon.identity);
        for (u32 i = 0; i < 8; ++i) CHECK(memory[RHH_BREEDING_CONTEXT_V1_0_SIZE + i] == 0xA5);
        free(memory);
        memory = malloc(RHH_BREEDING_CONTEXT_V1_1_SIZE + 8); CHECK(memory);
        memset(memory, 0xA5, RHH_BREEDING_CONTEXT_V1_1_SIZE + 8);
        old = (void *)memory; old->struct_size = RHH_BREEDING_CONTEXT_V1_1_SIZE;
        CHECK(!sBreedingTestService->context(old) && old->struct_size == RHH_BREEDING_CONTEXT_V1_1_SIZE);
        for (u32 i = 0; i < 8; ++i) CHECK(memory[RHH_BREEDING_CONTEXT_V1_1_SIZE + i] == 0xA5);
        free(memory);
    }
    if (event->kind == RHH_BREED_COMPATIBILITY)
    {
        ++sBreedingQueries;
        if (sBreedingChoice >= 0) decision->value = sBreedingChoice;
        if (sBreedingDeny) decision->allowed = 0;
    }
    else if (sBreedingSpecies) decision->value = sBreedingSpecies;
}
static void CheckBreedingCommitted(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sBreedingTestService) return;
    sLastBreeding.struct_size = sizeof(sLastBreeding);
    CHECK(sBreedingTestService && !sBreedingTestService->context(&sLastBreeding));
    CHECK(sLastBreeding.egg_valid && event->subject == sLastBreeding.egg_identity
        && event->event_key == sLastBreeding.offspring && !FlagGet(FLAG_PENDING_DAYCARE_EGG));
    ++sBreedingEggs;
}
static void CheckInheritanceProposal(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!sBreedingTestService) return;
    struct RhhBreedingContext context = {.struct_size = sizeof(context)};
    CHECK(!sBreedingTestService->context(&context) && context.offspring_mon.location == RHH_MON_STAGED
        && context.offspring_mon.identity == event->subject && context.offspring == event->event_key);
    if (event->kind == RHH_BREED_INHERIT_IV)
    {
        unsigned stat = event->ordinal; CHECK(stat < RHH_STAT_COUNT && event->args[1] == context.offspring_mon.ivs[stat]);
        ++sInheritanceCalls; sInitialIVs[stat] = event->args[0]; sEngineIVs[stat] = event->args[1];
        if (sInheritanceMode == 1)
            decision->value = stat < 2 ? RHH_BREED_IV_PARENT_0 : stat < 4 ? RHH_BREED_IV_PARENT_1 : RHH_BREED_IV_INITIAL;
        else if (sInheritanceMode == 2) decision->value = 999;
    }
    else
    {
        unsigned slot = event->ordinal; CHECK(slot < 4 && event->args[0] == slot);
        ++sMoveCalls; sEngineMoves[slot] = context.offspring_moves[slot];
        if (slot == 3 && sInheritanceMoveMode == 1) decision->value = sInheritanceMove;
        if (slot == 3 && sInheritanceMoveMode == 2) decision->value = context.offspring_moves[0];
        if (slot == 3 && sInheritanceMoveMode == 3) decision->value = RhhContentKeyOf("test:move/missing");
        if (sInheritanceMoveMode == 4 && slot == 0) decision->value = 0;
        if (sInheritanceMoveMode == 4 && slot == 3) decision->value = sInheritanceMove;
        if (sInheritanceMoveMode == 5 && slot == 3) decision->value = sInheritanceMove;
        if (sInheritanceMoveMode == 6) decision->value = 0;
    }
}
static const struct RhhMonEditRequest *sEditTestRequest;
static unsigned sEditConstraints, sEditNotifications;
static unsigned sHyperProposals, sHyperNotifications;
static unsigned sTrueNatureProposals, sTrueNatureNotifications;
static bool32 sEditMutateNatureStorage;
static struct BoxPokemon sEditNatureIntruder;
static bool32 sEditDeny, sEditOnUpdate, sEditCheckUnsafe, sEditMutateMoney, sEditMutateBag, sEditMutateBall;
static bool32 sEditNestedContext, sEditRequireCost, sEditRequireRecipe;
static void CheckEditUnsafeDispatch(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (sEditCheckUnsafe)
    {
        struct RhhMonEditResult out = {.struct_size = sizeof(out)};
        CHECK(sEditTestService->commit(sEditTestRequest, &out) == RHH_EDIT_UNAVAILABLE);
        sEditCheckUnsafe = FALSE;
    }
    if (sEditNestedContext)
    {
        struct RhhMonEditResult edit = {.struct_size = sizeof(edit)};
        struct RhhMonEditPayment payment = {.struct_size = sizeof(payment)};
        struct RhhMonEditRecipePayment recipe = {.struct_size = sizeof(recipe)};
        CHECK(sEditTestService->context(&edit) && sEditTestService->payment_context(&payment)
            && sEditTestService->recipe_context(&recipe));
        sEditNestedContext = FALSE;
    }
}
static void CheckEditConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhMonEditResult view = {.struct_size = sizeof(view)};
    CHECK(sEditTestService && !sEditTestService->context(&view));
    CHECK(view.fields == event->args[0] && view.is_preview == event->args[1] && view.before.identity == event->subject);
    if (view.fields & RHH_EDIT_HYPER_TRAINING)
    {
        struct RhhHyperTrainingEditContext hyper = {.struct_size = sizeof(hyper)};
        CHECK(!sEditTestService->hyper_training_context(&hyper) && hyper.before_mask != hyper.after_mask
            && hyper.is_preview == view.is_preview);
        ++sHyperProposals;
    }
    if (view.fields & RHH_EDIT_TRUE_NATURE)
    {
        struct RhhTrueNatureEditContext nature = {.struct_size = sizeof(nature)};
        CHECK(!sEditTestService->true_nature_context(&nature) && nature.is_preview == view.is_preview
            && view.before.nature != view.after.nature);
        ++sTrueNatureProposals;
    }
    struct RhhMonEditResult nested = {.struct_size = sizeof(nested)};
    CHECK(sEditTestService->commit(sEditTestRequest, &nested) == RHH_EDIT_UNAVAILABLE);
    if (sEditNestedContext)
    {
        struct RhhModEvent nestedEvent = {.struct_size = sizeof(nestedEvent), .kind = RHH_WILD_SPECIES};
        struct RhhModDecision nestedDecision = {.struct_size = sizeof(nestedDecision), .allowed = TRUE};
        RhhModsDispatch(&nestedEvent, &nestedDecision); CHECK(!sEditNestedContext);
    }
    ++sEditConstraints;
    if (sEditMutateMoney)
    { SetMoney(&gSaveBlock1Ptr->money, GetMoney(&gSaveBlock1Ptr->money) - 1); sEditMutateMoney = FALSE; }
    if (sEditMutateBag) { CHECK(AddBagItem(ITEM_POTION, 1)); sEditMutateBag = FALSE; }
    if (sEditMutateBall) { CHECK(AddBagItem(ITEM_POKE_BALL, 1)); sEditMutateBall = FALSE; }
    if (sEditMutateNatureStorage) { gPokemonStoragePtr->boxes[2][3] = sEditNatureIntruder; sEditMutateNatureStorage = FALSE; }
    if (sEditRequireCost)
    {
        struct RhhMonEditPayment payment = {.struct_size = sizeof(payment)};
        if (sEditTestService->payment_context(&payment) || payment.money < 500 || !payment.item_quantity)
        { decision->allowed = FALSE; strcpy(decision->reason, "minimum payment required"); }
    }
    if (sEditRequireRecipe)
    {
        struct RhhMonEditRecipePayment recipe = {.struct_size = sizeof(recipe)};
        if (sEditTestService->recipe_context(&recipe) || recipe.item_count < 2)
        { decision->allowed = FALSE; strcpy(decision->reason, "recipe required"); }
    }
    if (sEditDeny) { decision->allowed = 0; strcpy(decision->reason, "test rule denied edit"); }
}
static void CheckEditNotification(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhMonEditResult view = {.struct_size = sizeof(view)};
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    struct RhhOwnedMonSnapshot actual = {.struct_size = sizeof(actual)};
    CHECK(!sEditTestService->context(&view) && !view.is_preview && view.after.identity == event->subject);
    if (view.fields & RHH_EDIT_HYPER_TRAINING)
    {
        struct RhhHyperTrainingEditContext hyper = {.struct_size = sizeof(hyper)};
        CHECK(!sEditTestService->hyper_training_context(&hyper) && hyper.before_mask != hyper.after_mask
            && !hyper.is_preview);
        ++sHyperNotifications;
    }
    if (view.fields & RHH_EDIT_TRUE_NATURE)
    {
        struct RhhTrueNatureEditContext nature = {.struct_size = sizeof(nature)};
        CHECK(!sEditTestService->true_nature_context(&nature) && !nature.is_preview
            && view.before.nature != view.after.nature);
        ++sTrueNatureNotifications;
    }
    CHECK(!mons->find(view.after.identity, &actual) && !memcmp(actual.ivs, view.after.ivs, sizeof(actual.ivs))
        && !memcmp(actual.evs, view.after.evs, sizeof(actual.evs)) && actual.stat_nature == view.after.stat_nature
        && actual.nature == view.after.nature);
    struct RhhMonEditPayment payment = {.struct_size = sizeof(payment)};
    if (!sEditTestService->payment_context(&payment))
    {
        CHECK(GetMoney(&gSaveBlock1Ptr->money) == payment.money_after);
        if (payment.item)
        {
            u32 item; CHECK(!RhhContentResolve(RHH_ITEM, payment.item, &item) && payment.item_after < UINT16_MAX);
            CHECK((!payment.item_after || CheckBagHasItem(item, payment.item_after))
                && !CheckBagHasItem(item, payment.item_after + 1));
        }
    }
    struct RhhMonEditRecipePayment recipe = {.struct_size = sizeof(recipe)};
    if (!sEditTestService->recipe_context(&recipe))
    {
        CHECK(GetMoney(&gSaveBlock1Ptr->money) == recipe.money_after);
        for (u32 i = 0; i < recipe.item_count; ++i)
        {
            u32 item; CHECK(!RhhContentResolve(RHH_ITEM, recipe.items[i].item, &item)
                && recipe.items[i].after < UINT16_MAX);
            CHECK((!recipe.items[i].after || CheckBagHasItem(item, recipe.items[i].after))
                && !CheckBagHasItem(item, recipe.items[i].after + 1));
        }
    }
    ++sEditNotifications;
}
static void CheckEditUpdate(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    if (sEditOnUpdate)
    {
        struct RhhMonEditResult out = {.struct_size = sizeof(out)};
        CHECK(sEditTestService->commit(sEditTestRequest, &out) == RHH_EDIT_OK);
        sEditOnUpdate = FALSE;
    }
}
static bool32 sPresentedPhotoTracking;
static u32 sPresentedPhotoGates, sPresentedPhotoCaptured, sPresentedPhotoFailed, sPresentedPhotoLastStatus;
static bool32 sExpectedGeometricPhoto;
static bool32 sExpectedOrdinaryGeometricPhoto;
static u32 sGeometricPhotoGateSeen, sGeometricPhotoScoreSeen, sGeometricPhotoCaptureSeen;
static void CheckGeometricPhotoViews(void)
{
    const struct RhhPhotoVisibilityService *visibility = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 8));
    const struct RhhPhotoService *photo = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 2));
    struct RhhPhotoVisibility view = {.struct_size = sizeof(view)}, exact = {.struct_size = sizeof(exact), .ticket = 999};
    struct RhhPhotoCapture capture = {.struct_size = sizeof(capture)};
    static u8 mask[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT], silhouette[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
    CHECK(visibility && photo && !photo->context(&capture) && visibility->context(&exact) && exact.ticket == 999
        && !visibility->geometric_context(&view) && view.ticket == capture.ticket && view.visible_pixels
        && !visibility->copy_geometric_mask(mask, sizeof(mask))
        && !visibility->copy_geometric_silhouette(silhouette, sizeof(silhouette)));
    u32 shown = 0, total = 0;
    for (u32 i = 0; i < ARRAY_COUNT(mask); ++i) { CHECK(mask[i] <= silhouette[i]); shown += mask[i]; total += silhouette[i]; }
    CHECK(shown == view.visible_pixels && total == view.total_pixels);
    struct RhhPhotoDepth depth = {.struct_size = sizeof(depth)};
    static u16 depthPixels[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
    CHECK(!visibility->depth(&depth) && depth.ticket == capture.ticket
        && depth.mask_quality == RHH_PHOTO_MASK_GEOMETRIC && depth.visible_pixels == shown
        && !visibility->copy_depth(depthPixels, ARRAY_COUNT(depthPixels)));
    u64 sum = 0; u32 minimum = 65535, maximum = 0;
    for (u32 i = 0; i < ARRAY_COUNT(mask); ++i)
    {
        if (!mask[i]) { CHECK(!depthPixels[i]); continue; }
        if (depthPixels[i] < minimum) minimum = depthPixels[i];
        if (depthPixels[i] > maximum) maximum = depthPixels[i];
        sum += depthPixels[i];
    }
    CHECK(depth.depth_sum == sum && depth.min_depth == minimum && depth.max_depth == maximum);
    struct RhhPhotoExposure exposure = {.struct_size = sizeof(exposure)}, tiny = {.struct_size = 1, .ticket = 999};
    struct RhhPhotoFraming framing = {.struct_size = sizeof(framing)};
    struct RhhPhotoComposition composition = {.struct_size = sizeof(composition)};
    struct RhhPhotoDetail detail = {.struct_size = sizeof(detail)};
    struct RhhPhotoFootprint footprint = {.struct_size = sizeof(footprint)};
    static u8 rgb[RHH_PHOTO_RGB_BYTES];
    CHECK(!photo->copy_rgb(rgb, sizeof(rgb)) && !visibility->geometric_exposure(&exposure)
        && !visibility->geometric_framing(&framing) && !visibility->geometric_composition(&composition)
        && !visibility->geometric_detail(&detail) && visibility->geometric_exposure(&tiny) && tiny.ticket == 999
        && exposure.ticket == capture.ticket && framing.ticket == capture.ticket
        && composition.ticket == capture.ticket && detail.ticket == capture.ticket);
    struct RhhPhotoExposure expectedExposure = {.struct_size = sizeof(expectedExposure), .ticket = capture.ticket};
    struct RhhPhotoComposition expectedComposition = {.struct_size = sizeof(expectedComposition), .ticket = capture.ticket};
    struct RhhPhotoDetail expectedDetail = {.struct_size = sizeof(expectedDetail), .ticket = capture.ticket};
    u32 lumaSum = 0;
    for (u32 y = 0; y < RHH_PHOTO_HEIGHT; ++y) for (u32 x = 0; x < RHH_PHOTO_WIDTH; ++x)
    {
        u32 i = y * RHH_PHOTO_WIDTH + x, cell = (y * 3 / RHH_PHOTO_HEIGHT) * 3 + x * 3 / RHH_PHOTO_WIDTH;
        if (silhouette[i])
        {
            ++expectedComposition.silhouette.pixels; expectedComposition.silhouette.x2_sum += 2*x+1;
            expectedComposition.silhouette.y2_sum += 2*y+1; ++expectedComposition.silhouette.cells[cell];
        }
        if (!mask[i]) continue;
        ++expectedComposition.visible.pixels; expectedComposition.visible.x2_sum += 2*x+1;
        expectedComposition.visible.y2_sum += 2*y+1; ++expectedComposition.visible.cells[cell];
        u32 luma = (54u*rgb[i*3] + 183u*rgb[i*3+1] + 19u*rgb[i*3+2])/256u;
        if (!expectedExposure.visible_pixels || luma < expectedExposure.min_luma) expectedExposure.min_luma = luma;
        if (luma > expectedExposure.max_luma) expectedExposure.max_luma = luma;
        ++expectedExposure.visible_pixels; expectedExposure.dark_pixels += luma <= 16; expectedExposure.bright_pixels += luma >= 240; lumaSum += luma;
        for (u32 direction = 0; direction < 2; ++direction)
        {
            if ((!direction && x+1 >= RHH_PHOTO_WIDTH) || (direction && y+1 >= RHH_PHOTO_HEIGHT)) continue;
            u32 n = i + (direction ? RHH_PHOTO_WIDTH : 1);
            if (!mask[n]) continue;
            u32 other = (54u*rgb[n*3] + 183u*rgb[n*3+1] + 19u*rgb[n*3+2])/256u;
            u32 delta = luma > other ? luma-other : other-luma;
            ++expectedDetail.neighbor_pairs; expectedDetail.luma_delta_sum += delta;
            expectedDetail.high_contrast_pairs += delta >= 32;
            if (delta > expectedDetail.max_luma_delta) expectedDetail.max_luma_delta = delta;
        }
    }
    expectedExposure.mean_luma = lumaSum / shown;
    CHECK(!memcmp(&exposure, &expectedExposure, sizeof(exposure))
        && !memcmp(&composition, &expectedComposition, sizeof(composition))
        && !memcmp(&detail, &expectedDetail, sizeof(detail)));
    for (u32 y = 0; y < RHH_PHOTO_HEIGHT; ++y) for (u32 x = 0; x < RHH_PHOTO_WIDTH; ++x)
    {
        u32 i = y * RHH_PHOTO_WIDTH + x;
        if (mask[i]) CHECK(x >= framing.visible.left && x < framing.visible.right && y >= framing.visible.top && y < framing.visible.bottom);
        if (silhouette[i]) CHECK(x >= framing.silhouette.left && x < framing.silhouette.right && y >= framing.silhouette.top && y < framing.silhouette.bottom);
    }
    if (sExpectedOrdinaryGeometricPhoto)
        CHECK(!visibility->geometric_footprint(&footprint) && footprint.ticket == capture.ticket
            && footprint.inside_pixels == total && footprint.total_pixels >= total);
    struct RhhPhotoExposure exactExposure = {.struct_size = sizeof(exactExposure), .ticket = 999};
    struct RhhPhotoFraming exactFraming = {.struct_size = sizeof(exactFraming), .ticket = 999};
    struct RhhPhotoFootprint exactFootprint = {.struct_size = sizeof(exactFootprint), .ticket = 999};
    struct RhhPhotoComposition exactComposition = {.struct_size = sizeof(exactComposition), .ticket = 999};
    struct RhhPhotoDetail exactDetail = {.struct_size = sizeof(exactDetail), .ticket = 999};
    CHECK(visibility->exposure(&exactExposure) && exactExposure.ticket == 999
        && visibility->framing(&exactFraming) && exactFraming.ticket == 999
        && visibility->footprint(&exactFootprint) && exactFootprint.ticket == 999
        && visibility->composition(&exactComposition) && exactComposition.ticket == 999
        && visibility->detail(&exactDetail) && exactDetail.ticket == 999);
}
static void CheckGeometricPhotoScore(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; (void)decision; if (sExpectedGeometricPhoto) { CheckGeometricPhotoViews(); ++sGeometricPhotoScoreSeen; } }
static void CheckPresentedPhotoGate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (sExpectedGeometricPhoto)
    {
        const struct RhhPhotoService *service = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 2));
        u32 mode = 99, presented = 99;
        CHECK(service && !service->request_mode(&mode) && !service->requires_presenter(&presented)
            && mode == (sExpectedOrdinaryGeometricPhoto ? RHH_PHOTO_REQUEST_ORDINARY_GEOMETRIC : RHH_PHOTO_REQUEST_PRESENTED_GEOMETRIC)
            && presented == !sExpectedOrdinaryGeometricPhoto);
        ++sGeometricPhotoGateSeen;
    }
    if (!sPresentedPhotoTracking) return;
    const struct RhhPhotoService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 1));
    u32 presented = 0; CHECK(service && !service->requires_presenter(&presented) && presented == 1);
    ++sPresentedPhotoGates;
}
static void CheckPresentedPhotoOutcome(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (sExpectedGeometricPhoto && event->kind == RHH_PHOTO_CAPTURED) { CheckGeometricPhotoViews(); ++sGeometricPhotoCaptureSeen; }
    if (!sPresentedPhotoTracking) return;
    const struct RhhPhotoService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 1));
    struct RhhPhotoCapture capture = {.struct_size = sizeof(capture)}; u32 presented = 0;
    CHECK(service && !service->requires_presenter(&presented) && presented == 1
        && !service->context(&capture) && capture.ticket == event->ordinal && capture.status == event->args[0]);
    sPresentedPhotoLastStatus = capture.status;
    if (event->kind == RHH_PHOTO_CAPTURED) ++sPresentedPhotoCaptured; else ++sPresentedPhotoFailed;
}
static u32 sScoutPhotoTestMode, sScoutPhotoGates, sScoutPhotoNotifications;
static u32 sExpectedScoutPhotoQuality;
static u64 sScoutPhotoExpectedTicket;
static void CheckScoutPhotoGate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (!sScoutPhotoExpectedTicket) return;
    const struct RhhScoutingService *scout = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/scouting", RHH_SERVICE_VERSION(1, 2));
    u64 ticket = 0; CHECK(scout && !scout->photo_context(&ticket) && ticket == sScoutPhotoExpectedTicket);
    u32 quality = 99; CHECK(!scout->photo_quality(&quality) && quality == sExpectedScoutPhotoQuality);
    ++sScoutPhotoGates;
    if (sScoutPhotoTestMode == 1) { decision->allowed = 0; snprintf(decision->reason, sizeof(decision->reason), "photo denied"); }
    if (sScoutPhotoTestMode == 2) { (void)Random32(); (void)Random2_32(); }
    if (sScoutPhotoTestMode == 3) gObjectEvents[1].invisible = TRUE;
}
static void CheckScoutPhotoNotification(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    if (!sScoutPhotoExpectedTicket) return;
    const struct RhhScoutingService *scout = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/scouting", RHH_SERVICE_VERSION(1, 2));
    u64 ticket = 0; CHECK(scout && !scout->photo_context(&ticket) && ticket == sScoutPhotoExpectedTicket);
    u32 quality = 99; CHECK(!scout->photo_quality(&quality) && quality == sExpectedScoutPhotoQuality);
    ++sScoutPhotoNotifications;
}
static bool32 sPhotoGateImpure, sPhotoScoreImpure, sPhotoRewardImpure;
static void PhotoPurityGate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if ((event->kind == RHH_CAN_PHOTO && sPhotoGateImpure)
        || (event->kind == RHH_CAN_CLAIM_PHOTO_REWARD && sPhotoRewardImpure))
    { (void)Random32(); (void)Random2_32(); }
}
static void PhotoPurityScore(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (sPhotoScoreImpure) { (void)Random32(); (void)Random2_32(); }
}
static bool32 sDormantProfileImpure;
static void CheckDormantProfilePurity(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; (void)decision; if (sDormantProfileImpure) (void)Random(); }
static const struct RhhModHook sEditHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_DORMANT_WILD_PROFILE, RHH_CONSTRAINT, 0, CheckDormantProfilePurity, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_SPECIES, RHH_TRANSFORM, 0, CheckEditUnsafeDispatch, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_MON, RHH_CONSTRAINT, 0, CheckEditConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_MON_EDITED, RHH_OBSERVE, 0, NULL, CheckEditNotification},
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, CheckEditUpdate},
    {sizeof(struct RhhModHook), RHH_BREED_COMPATIBILITY, RHH_TRANSFORM, 0, CheckBreedingProposal, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_OFFSPRING, RHH_TRANSFORM, 0, CheckBreedingProposal, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_EGG_CREATED, RHH_OBSERVE, 0, NULL, CheckBreedingCommitted},
    {sizeof(struct RhhModHook), RHH_BREED_INHERIT_IV, RHH_TRANSFORM, 0, CheckInheritanceProposal, NULL},
    {sizeof(struct RhhModHook), RHH_BREED_INHERIT_MOVE, RHH_TRANSFORM, 0, CheckInheritanceProposal, NULL},
    {sizeof(struct RhhModHook), RHH_EGG_HATCHED, RHH_OBSERVE, 0, NULL, CheckHatchNotification},
    {sizeof(struct RhhModHook), RHH_WORLD_STEP, RHH_OBSERVE, 0, NULL, CheckWorldStepNotification},
    {sizeof(struct RhhModHook), RHH_CAN_WORLD_ENVIRONMENT, RHH_CONSTRAINT, 0, CheckEnvironmentConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_PHOTO, RHH_CONSTRAINT, 0, PhotoPurityGate, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_CLAIM_PHOTO_REWARD, RHH_CONSTRAINT, 0, PhotoPurityGate, NULL},
    {sizeof(struct RhhModHook), RHH_PHOTO_SCORE, RHH_TRANSFORM, 0, PhotoPurityScore, NULL},
    {sizeof(struct RhhModHook), RHH_WORLD_ENVIRONMENT_CHANGED, RHH_OBSERVE, 0, NULL, CheckEnvironmentNotification},
    {sizeof(struct RhhModHook), RHH_BATTLE_ENVIRONMENT_SEED, RHH_TRANSFORM, 0, CheckBattleSeedTransform, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_ENVIRONMENT_SEED, RHH_CONSTRAINT, 0, CheckBattleSeedConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_ENVIRONMENT_SEED, RHH_FINALIZE, 0, CheckBattleSeedFinalize, NULL},
    {sizeof(struct RhhModHook), RHH_HABITAT_QUERY, RHH_TRANSFORM, 0, CheckHabitatTransform, NULL},
    {sizeof(struct RhhModHook), RHH_HABITAT_QUERY, RHH_CONSTRAINT, 0, CheckHabitatConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_CANDIDATE, RHH_TRANSFORM, 0, CheckWildCandidate, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_CANDIDATE, RHH_CONSTRAINT, 0, CheckWildConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_CANDIDATE, RHH_FINALIZE, 0, CheckWildFinalize, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_WILD, RHH_CONSTRAINT, 0, CheckWildGate, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_SPECIAL_WILD, RHH_CONSTRAINT, 0, CheckWildGate, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_DEXNAV_WILD, RHH_CONSTRAINT, 0, CheckWildGate, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_FACILITY_WILD, RHH_CONSTRAINT, 0, CheckWildGate, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_MANUAL_FACILITY_WILD, RHH_CONSTRAINT, 0, CheckWildGate, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_WILD, RHH_CONSTRAINT, 0, CheckWildGateSticky, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_SPECIAL_WILD, RHH_CONSTRAINT, 0, CheckWildGateSticky, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_DEXNAV_WILD, RHH_CONSTRAINT, 0, CheckWildGateSticky, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_FACILITY_WILD, RHH_CONSTRAINT, 0, CheckWildGateSticky, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_GENERATE_MANUAL_FACILITY_WILD, RHH_CONSTRAINT, 0, CheckWildGateSticky, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_CANCELLED, RHH_OBSERVE, 0, NULL, CheckWildCancelled},
    {sizeof(struct RhhModHook), RHH_WILD_SPECIAL_CANCELLED, RHH_OBSERVE, 0, NULL, CheckWildCancelled},
    {sizeof(struct RhhModHook), RHH_WILD_DEXNAV_CANCELLED, RHH_OBSERVE, 0, NULL, CheckWildCancelled},
    {sizeof(struct RhhModHook), RHH_WILD_FACILITY_CANCELLED, RHH_OBSERVE, 0, NULL, CheckWildCancelled},
    {sizeof(struct RhhModHook), RHH_WILD_MANUAL_FACILITY_CANCELLED, RHH_OBSERVE, 0, NULL, CheckWildCancelled},
    {sizeof(struct RhhModHook), RHH_WILD_SPECIAL_GENERATED, RHH_OBSERVE, 0, NULL, CheckWildSpecialGenerated},
    {sizeof(struct RhhModHook), RHH_WILD_DEXNAV_GENERATED, RHH_OBSERVE, 0, NULL, CheckWildSpecialGenerated},
    {sizeof(struct RhhModHook), RHH_WILD_FACILITY_GENERATED, RHH_OBSERVE, 0, NULL, CheckWildSpecialGenerated},
    {sizeof(struct RhhModHook), RHH_WILD_MANUAL_FACILITY_GENERATED, RHH_OBSERVE, 0, NULL, CheckWildSpecialGenerated},
    {sizeof(struct RhhModHook), RHH_ENCOUNTER_STARTED, RHH_OBSERVE, 0, NULL, CheckEncounterStartedForCancellation},
    {sizeof(struct RhhModHook), RHH_WILD_GENERATED, RHH_OBSERVE, 0, NULL, CheckWildGenerated},
    {sizeof(struct RhhModHook), RHH_WILD_WEIGHT, RHH_TRANSFORM, 0, CheckWildWeight, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_WEIGHT, RHH_CONSTRAINT, 0, CheckWildWeightConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_SPAWNED, RHH_OBSERVE, 0, NULL, CheckWildActorEvent},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_REMOVED, RHH_OBSERVE, 0, NULL, CheckWildActorEvent},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_ENGAGED, RHH_OBSERVE, 0, NULL, CheckWildActorEvent},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_BATTLE_ENDED, RHH_OBSERVE, 0, NULL, CheckWildActorBattleEnd},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_IDENTIFIED, RHH_OBSERVE, 0, NULL, CheckWildActorEvent},
    {sizeof(struct RhhModHook), RHH_WILD_ACTOR_STEP_FINISHED, RHH_OBSERVE, 0, NULL, CheckWildStep},
    {sizeof(struct RhhModHook), RHH_PERSISTENCE_FAILED, RHH_OBSERVE, 0, NULL, ObservePersistenceFailure},
    {sizeof(struct RhhModHook), RHH_CAN_WILD_CONCEALMENT, RHH_CONSTRAINT, 0, CheckConcealConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_CONCEALMENT_CHANGED, RHH_OBSERVE, 0, NULL, CheckConcealNotification},
    {sizeof(struct RhhModHook), RHH_CAN_PHOTO, RHH_CONSTRAINT, 0, CheckPresentedPhotoGate, NULL},
    {sizeof(struct RhhModHook), RHH_PHOTO_SCORE, RHH_CONSTRAINT, 0, CheckGeometricPhotoScore, NULL},
    {sizeof(struct RhhModHook), RHH_PHOTO_CAPTURED, RHH_OBSERVE, 0, NULL, CheckPresentedPhotoOutcome},
    {sizeof(struct RhhModHook), RHH_PHOTO_FAILED, RHH_OBSERVE, 0, NULL, CheckPresentedPhotoOutcome},
    {sizeof(struct RhhModHook), RHH_CAN_SCOUT, RHH_CONSTRAINT, 0, CheckScoutPhotoGate, NULL},
    {sizeof(struct RhhModHook), RHH_SCOUT_RECORDED, RHH_OBSERVE, 0, NULL, CheckScoutPhotoNotification},
#if WE_OW_ENCOUNTERS
    {sizeof(struct RhhModHook), RHH_WILD_GENERATED, RHH_OBSERVE, 0, NULL, CaptureAutomaticWild}
#endif
};
static const struct RhhModDefinition sEditTestMod = {
    .struct_size = sizeof(sEditTestMod), .abi_version = RHH_MOD_ABI_V1, .id = "test:mon_edit_rules",
    .hook_count = ARRAY_COUNT(sEditHooks), .hooks = sEditHooks
};
static RhhContentKey sTestMusicSelection;
static u32 sTestMusicRngMode, sTestAssignmentRngMode, sTestMusicObserveRng;
static unsigned sTestMusicContext;
static RhhContentKey sTestMusicOriginal, sTestMusicMap;
static u32 sTestGainMode, sTestGainValue = RHH_MUSIC_GAIN_ONE, sTestGainCalls;
static struct RhhMusicGainContext sTestGainContext;
static u32 sTestLoopMode, sTestLoopCalls;
static struct RhhMusicLoopContext sTestLoopContext;
static void SelectTestMusicLoop(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    const struct RhhMusicService *music = (const void *)RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 3));
    sTestLoopContext = (struct RhhMusicLoopContext){.struct_size = sizeof(sTestLoopContext)};
    CHECK(music && !music->loop_context(&sTestLoopContext) && sTestLoopContext.track == event->event_key
        && sTestLoopContext.phase == event->args[0]);
    ++sTestLoopCalls;
    if (sTestLoopMode == 1 || sTestLoopMode == 3 || sTestLoopMode == 4 || sTestLoopMode == 5)
        decision->value = RHH_MUSIC_LOOP_VALUE(100, 400);
    if (sTestLoopMode == 2) decision->value = RHH_MUSIC_LOOP_VALUE(400, 100);
    if (sTestLoopMode == 7) decision->value = RHH_MUSIC_LOOP_VALUE(0, UINT32_MAX);
    if (sTestLoopMode == 3) decision->allowed = FALSE;
    if (sTestLoopMode == 4) (void)Random();
    if (sTestLoopMode == 6) PlayBGM(MUS_VS_WILD);
}
static void ConstrainTestMusicLoop(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; if (sTestLoopMode == 5) decision->value = RHH_MUSIC_LOOP_VALUE(200, 300); }

static struct RhhMusicLifecycle sTestMusicLifecycle[256];
static u32 sTestMusicLifecycleCount, sTestMusicLifecycleRestart;
static void ObserveTestMusicLifecycle(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    const struct RhhMusicService *service = (const void *)RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 2));
    struct RhhMusicLifecycle item = {.struct_size = sizeof(item)};
    CHECK(service && !service->lifecycle_context(&item) && item.sequence == event->ordinal
        && item.track == event->event_key && item.cause == event->args[0]);
    if (sTestMusicLifecycleCount < ARRAY_COUNT(sTestMusicLifecycle)) sTestMusicLifecycle[sTestMusicLifecycleCount++] = item;
    if (sTestMusicObserveRng) { (void)Random(); (void)Random2(); }
    if (sTestMusicLifecycleRestart && item.cause == RHH_MUSIC_STARTED)
    {
        sTestMusicLifecycleRestart = 0; PlayBGM(MUS_ROUTE110);
        NativeMusicFlushLifecycle(); /* Reentrant delivery waits for the next flush. */
    }
}

static void SelectTestMusic(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    sTestMusicContext = event->args[0]; sTestMusicOriginal = event->event_key; sTestMusicMap = event->subject;
    if (sTestMusicSelection && sTestMusicRngMode != 3 && sTestMusicRngMode != 4) decision->value = sTestMusicSelection;
    if (sTestMusicRngMode == 1 || sTestMusicRngMode == 2) { (void)Random(); (void)Random2(); }
    if (sTestMusicRngMode == 2) decision->allowed = FALSE;
}
static void SelectTestBattleMusic(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sTestMusicRngMode == 3 || sTestMusicRngMode == 4) {
        decision->value = sTestMusicSelection; (void)Random(); (void)Random2();
        if (sTestMusicRngMode == 4) decision->allowed = FALSE;
    }
}
static void CheckTestMusicAssignmentRng(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sTestAssignmentRngMode) { (void)Random(); (void)Random2(); }
    if (sTestAssignmentRngMode == 2) { decision->allowed = FALSE; strcpy(decision->reason, "impure denial"); }
}
static void ObserveTestMusicAssignmentRng(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    if (sTestMusicObserveRng) { (void)Random(); (void)Random2(); }
}
static void SelectTestMusicGain(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    const struct RhhMusicService *music = (const void *)RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 1));
    struct RhhMusicPlayback pending = {.struct_size = sizeof(pending), .track = 999};
    sTestGainContext = (struct RhhMusicGainContext){.struct_size = sizeof(sTestGainContext)};
    CHECK(music && !music->gain_context(&sTestGainContext) && sTestGainContext.track == event->event_key
        && sTestGainContext.original == event->subject && music->playback(&pending) && pending.track == 999);
    ++sTestGainCalls;
    if (sTestGainMode) decision->value = sTestGainValue;
    if (sTestGainMode == 2) decision->allowed = FALSE;
    if (sTestGainMode == 3) (void)Random();
    if (sTestGainMode == 5) PlayBGM(MUS_VS_WILD);
}
static void ConstrainTestMusicGain(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; if (sTestGainMode == 4) decision->value = 0; }
static void CheckTestMusicLevelRng(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)state; (void)decision; if (event->args[0] == 2100) (void)Random(); }
static void CheckTestWildPolicyRng(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)state; (void)decision; if (event->args[0] == 8) (void)Random(); }
static const struct RhhModMusic sTestMusic[] = {
    {sizeof(struct RhhModMusic), 0, "test:music/custom", "Custom", "gen1/wild.mid", "fonts/gen4.sf2"},
    {sizeof(struct RhhModMusic), 0, "test:music/missing", "Missing", "missing.mid", "fonts/gen4.sf2"},
    {sizeof(struct RhhModMusic), 0, "test:music/invalid_font", "Invalid font", "gen1/wild.mid", "gen1/wild.mid"},
    {sizeof(struct RhhModMusic), 0, "test:music/invalid_midi", "Invalid MIDI", "fonts/gen4.sf2", "fonts/gen4.sf2"}
};
static const struct RhhModHook sTestMusicHooks[] = {
    {sizeof(struct RhhModHook), RHH_MUSIC_SELECT, RHH_TRANSFORM, 100, SelectTestMusic, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_MUSIC, RHH_TRANSFORM, 100, SelectTestBattleMusic, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_SET_MUSIC_ASSIGNMENT, RHH_CONSTRAINT, 100, CheckTestMusicAssignmentRng, NULL},
    {sizeof(struct RhhModHook), RHH_MUSIC_ASSIGNMENT_CHANGED, RHH_OBSERVE, 0, NULL, ObserveTestMusicAssignmentRng},
    {sizeof(struct RhhModHook), RHH_MUSIC_LEVEL_CHANGED, RHH_OBSERVE, 0, NULL, ObserveTestMusicAssignmentRng},
    {sizeof(struct RhhModHook), RHH_MUSIC_PLAYBACK_CHANGED, RHH_OBSERVE, 0, NULL, ObserveTestMusicLifecycle},
    {sizeof(struct RhhModHook), RHH_MUSIC_LOOP, RHH_TRANSFORM, 0, SelectTestMusicLoop, NULL},
    {sizeof(struct RhhModHook), RHH_MUSIC_LOOP, RHH_CONSTRAINT, 0, ConstrainTestMusicLoop, NULL},
    {sizeof(struct RhhModHook), RHH_MUSIC_GAIN, RHH_TRANSFORM, 0, SelectTestMusicGain, NULL},
    {sizeof(struct RhhModHook), RHH_MUSIC_GAIN, RHH_CONSTRAINT, 0, ConstrainTestMusicGain, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_SET_MUSIC_LEVEL, RHH_CONSTRAINT, 0, CheckTestMusicLevelRng, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_SET_WILD_BEHAVIOR_POLICY, RHH_CONSTRAINT, 0, CheckTestWildPolicyRng, NULL}
};
static const struct RhhModDefinition sTestMusicMod = {
    .struct_size = sizeof(sTestMusicMod), .abi_version = RHH_MOD_ABI_MUSIC, .id = "test:music_catalog",
    .hook_count = ARRAY_COUNT(sTestMusicHooks), .hooks = sTestMusicHooks,
    .music_count = ARRAY_COUNT(sTestMusic), .music = sTestMusic
};
static struct RhhModSceneRule sTestSceneRule = {
    .struct_size = sizeof(sTestSceneRule), .id = "test:scene/lab_wall",
    .match = RHH_SCENE_MATCH_MAP | RHH_SCENE_MATCH_POSITION, .x = 1,
    .style = {RHH_SCENE_SOLID, RHH_SCENE_UPPER_WALL_NORTH,
              RHH_SCENE_RECEIVES_SHADOW | RHH_SCENE_CASTS_SHADOW | RHH_SCENE_OCCLUDES, 8, 24}
};
static const struct RhhModShader sTestProjectedMask = {sizeof(sTestProjectedMask), 0, "test:shader/projected_mask",
    "#version 120\nuniform sampler2D uMask; varying vec2 vUv; void main(){float m=texture2D(uMask,vUv).r;"
    "gl_FragColor=m<.75 ? vec4(1.,0.,0.,1.) : vec4(0.,1.,0.,1.);}"};
static const struct RhhModDefinition sTestSceneMod = {
    .struct_size = sizeof(sTestSceneMod), .abi_version = RHH_MOD_ABI_SCENE,
    .id = "test:scene_catalog", .scene_rule_count = 1, .scene_rules = &sTestSceneRule,
    .shader_count = 1, .shaders = &sTestProjectedMask
};
static int sSceneNeighborRuleEnabled, sSceneNeighborLightsEnabled;
static const char *sTestThirdLibrary;
static const char *sTestFourthLibrary;
static const char *sTestFifthLibrary;
static const char *sTestSixthLibrary;
static struct RhhModSceneRule sSceneNeighborRule = {.struct_size = sizeof(sSceneNeighborRule),
    .id = "test:scene/oldale_link", .match = RHH_SCENE_MATCH_MAP | RHH_SCENE_MATCH_POSITION,
    .x = 2, .y = 0, .style = {RHH_SCENE_SOLID, RHH_SCENE_UPPER_FLAT, RHH_SCENE_RECEIVES_SHADOW, 8, 0}};
static struct RhhModSceneLight sSceneNeighborLights[] = {
    {.struct_size = sizeof(struct RhhModSceneLight), .id = "test:light/oldale_fixture", .x = 8 * 16 + 8,
     .y = 8, .height = 12, .radius = 48, .rgb = 0xff20ff, .intensity = 900, .period_mask = 1u << (RHH_TIME_NIGHT - 1)},
    {.struct_size = sizeof(struct RhhModSceneLight), .id = "test:light/oldale_offscreen", .x = 18 * 16 + 8,
     .y = 8, .height = 12, .radius = 64, .rgb = 0x20ffff, .intensity = 900, .period_mask = 1u << (RHH_TIME_NIGHT - 1)}
};
static struct RhhModDefinition sSceneNeighborMod = {.struct_size = sizeof(sSceneNeighborMod),
    .abi_version = RHH_MOD_ABI_SCENE_LIGHTS, .id = "test:scene_neighbor", .scene_rule_count = 1, .scene_rules = &sSceneNeighborRule,
    .scene_light_count = ARRAY_COUNT(sSceneNeighborLights), .scene_lights = sSceneNeighborLights};
static int sHd2dWallRuleEnabled;
static struct RhhModSceneRule sHd2dWallRules[] = {
    {.struct_size = sizeof(struct RhhModSceneRule), .id = "test:scene/hd2d_wall",
     .match = RHH_SCENE_MATCH_MAP | RHH_SCENE_MATCH_POSITION, .x = 33, .y = 13,
     .style = {RHH_SCENE_SOLID, RHH_SCENE_UPPER_WALL_NORTH,
        RHH_SCENE_RECEIVES_SHADOW | RHH_SCENE_CASTS_SHADOW | RHH_SCENE_OCCLUDES, 0, 48}},
    {.struct_size = sizeof(struct RhhModSceneRule), .id = "test:scene/no_shadow_receiver",
     .match = RHH_SCENE_MATCH_MAP | RHH_SCENE_MATCH_POSITION, .x = 34, .y = 14,
     .style = {RHH_SCENE_SOLID, RHH_SCENE_UPPER_FLAT, 0, 0, 0}}
};
static const struct RhhModDefinition sHd2dWallMod = {.struct_size = sizeof(sHd2dWallMod),
    .abi_version = RHH_MOD_ABI_SCENE, .id = "test:hd2d_wall", .scene_rule_count = ARRAY_COUNT(sHd2dWallRules), .scene_rules = sHd2dWallRules};
static bool32 sEnableScriptCleanupProbe;
static u32 sScriptCleanupCount;
static RhhContentKey sExpectedScriptCleanupSubject;
static RhhContentKey sExpectedAuthoredEncounter;
static u32 sExpectedAuthoredPreparation, sAuthoredEncounterChecks;
static bool32 sExpectNoAuthoredEncounter;
static RhhContentKey sExpectedAuthoredBattle;
static u32 sAuthoredBattleChecks, sExpectedAuthoredBattlePreparation;
static void (*sScriptCleanupPredicate)(void);
static void (*sTutorialCleanupPredicate)(void);
static void CheckAuthoredEncounter(void)
{
    if (!sExpectedAuthoredEncounter && !sExpectNoAuthoredEncounter) return;
    const struct RhhWildEncounterService *wild = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 15));
    struct RhhScriptedEncounterSource source = {.struct_size = sizeof(source)};
    if (sExpectNoAuthoredEncounter)
    {
        source.encounter = 123;
        CHECK(wild && wild->scripted_source(&source) && source.encounter == 123);
        ++sAuthoredEncounterChecks;
        return;
    }
    CHECK(wild && !wild->scripted_source(&source) && source.encounter == sExpectedAuthoredEncounter
        && source.preparation == sExpectedAuthoredPreparation);
    ++sAuthoredEncounterChecks;
}
static void CheckAuthoredEncounterConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; (void)decision; CheckAuthoredEncounter(); }
static void CheckAuthoredEncounterNotification(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)event; (void)decision; (void)state; CheckAuthoredEncounter(); }
static void CheckTutorialCleanupNotification(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    CheckAuthoredEncounterNotification(event, decision, state);
    if (sTutorialCleanupPredicate) sTutorialCleanupPredicate();
}
static void CheckAuthoredBattle(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    if (!sExpectedAuthoredBattle) return;
    const struct RhhWildEncounterService *wild = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 16));
    struct RhhScriptedEncounterSource source = {.struct_size = sizeof(source)};
    CHECK(wild && !wild->scripted_battle_source(&source) && source.encounter == sExpectedAuthoredBattle
        && source.preparation == sExpectedAuthoredBattlePreparation);
    ++sAuthoredBattleChecks;
}
static void ObserveScriptCleanup(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state;
    const struct RhhWildEncounterService *wild = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 14));
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context)};
    struct RhhWildEncounterCancellation cancel = {.struct_size = sizeof(cancel)};
    CHECK(wild && !wild->context(&context) && !wild->cancellation_context(&cancel)
        && context.phase == RHH_WILD_PHASE_CANCELLED && context.species == event->subject
        && !memcmp(&context, &cancel.candidate, sizeof(context)) && !strcmp(cancel.reason, "scripted veto")
        && !decision->allowed && !RhhModsCommandsAllowed()
        && !ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked());
    if (sExpectedScriptCleanupSubject) CHECK(event->subject == sExpectedScriptCleanupSubject);
    CheckAuthoredEncounter();
    if (sScriptCleanupPredicate) sScriptCleanupPredicate();
    ++sScriptCleanupCount;
}
static const struct RhhModHook sScriptCleanupHooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_SCRIPTED_CANDIDATE, RHH_CONSTRAINT, 0, CheckAuthoredEncounterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_EVENT_CANDIDATE, RHH_CONSTRAINT, 0, CheckAuthoredEncounterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_BEGIN_SCRIPTED_WILD, RHH_CONSTRAINT, 0, CheckAuthoredEncounterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_BEGIN_EVENT_WILD, RHH_CONSTRAINT, 0, CheckAuthoredEncounterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_BEGIN_TUTORIAL_WILD, RHH_CONSTRAINT, 0, CheckAuthoredEncounterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_SCRIPTED_CANCELLED, RHH_OBSERVE, 0, NULL, CheckAuthoredEncounterNotification},
    {sizeof(struct RhhModHook), RHH_WILD_EVENT_CANCELLED, RHH_OBSERVE, 0, NULL, CheckAuthoredEncounterNotification},
    {sizeof(struct RhhModHook), RHH_WILD_TUTORIAL_CANCELLED, RHH_OBSERVE, 0, NULL, CheckAuthoredEncounterNotification},
    {sizeof(struct RhhModHook), RHH_WILD_TUTORIAL_CLEANUP_FINISHED, RHH_OBSERVE, 0, NULL, CheckTutorialCleanupNotification},
    {sizeof(struct RhhModHook), RHH_WILD_SCRIPTED_READY, RHH_OBSERVE, 0, NULL, CheckAuthoredEncounterNotification},
    {sizeof(struct RhhModHook), RHH_WILD_GHOST_READY, RHH_OBSERVE, 0, NULL, CheckAuthoredEncounterNotification},
    {sizeof(struct RhhModHook), RHH_WILD_TUTORIAL_READY, RHH_OBSERVE, 0, NULL, CheckAuthoredEncounterNotification},
    {sizeof(struct RhhModHook), RHH_WILD_FIRST_BATTLE_READY, RHH_OBSERVE, 0, NULL, CheckAuthoredEncounterNotification},
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, CheckAuthoredBattle},
    {sizeof(struct RhhModHook), RHH_BATTLE_ENDED, RHH_OBSERVE, 0, NULL, CheckAuthoredBattle},
    {sizeof(struct RhhModHook), RHH_WILD_SCRIPTED_CLEANUP_FINISHED, RHH_OBSERVE, 0, NULL, ObserveScriptCleanup},
    {sizeof(struct RhhModHook), RHH_WILD_EVENT_CLEANUP_FINISHED, RHH_OBSERVE, 0, NULL, ObserveScriptCleanup}
};
static const struct RhhModDefinition sScriptCleanupMod = {
    .struct_size = sizeof(sScriptCleanupMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:script_cleanup", .hook_count = ARRAY_COUNT(sScriptCleanupHooks), .hooks = sScriptCleanupHooks
};
static void InitModsForTestWithLibraries(const char *path, const char *extra)
{
    RhhModsInit(); CHECK(ModsRegisterBuiltins() == 0); CHECK(!RhhModsRegister(&sEditTestMod));
    if (sEnableScriptCleanupProbe) CHECK(!RhhModsRegister(&sScriptCleanupMod));
    sTestSceneRule.map = RhhContentKeyForId(RHH_MAP, (1 << 8)); CHECK(sTestSceneRule.map);
    CHECK(!RhhModsRegister(&sTestMusicMod)); CHECK(!RhhModsRegister(&sTestSceneMod));
    if (sSceneNeighborRuleEnabled)
    {
        sSceneNeighborRule.map = RhhContentKeyForId(RHH_MAP, MAP_OLDALE_TOWN); CHECK(sSceneNeighborRule.map);
        for (unsigned i = 0; i < ARRAY_COUNT(sSceneNeighborLights); ++i) sSceneNeighborLights[i].map = sSceneNeighborRule.map;
        sSceneNeighborMod.scene_light_count = sSceneNeighborLightsEnabled ? ARRAY_COUNT(sSceneNeighborLights) : 0;
        CHECK(!RhhModsRegister(&sSceneNeighborMod));
    }
    if (sHd2dWallRuleEnabled)
    {
        for (unsigned i = 0; i < ARRAY_COUNT(sHd2dWallRules); ++i) {
            sHd2dWallRules[i].map = RhhContentKeyForId(RHH_MAP, MAP_ROUTE103); CHECK(sHd2dWallRules[i].map);
        }
        CHECK(!RhhModsRegister(&sHd2dWallMod));
    }
    if (path) CHECK(!RhhModsLoadLibrary(path));
    if (extra) CHECK(!RhhModsLoadLibrary(extra));
    if (sTestThirdLibrary) CHECK(!RhhModsLoadLibrary(sTestThirdLibrary));
    if (sTestFourthLibrary) CHECK(!RhhModsLoadLibrary(sTestFourthLibrary));
    if (sTestFifthLibrary) CHECK(!RhhModsLoadLibrary(sTestFifthLibrary));
    if (sTestSixthLibrary) CHECK(!RhhModsLoadLibrary(sTestSixthLibrary));
    CHECK(RhhModsFinalize() == 0);
}
static void InitModsForTestWithLibrary(const char *path) { InitModsForTestWithLibraries(path, NULL); }
static void InitModsForTest(void) { InitModsForTestWithLibrary(NULL); }
static void CheckOwnedMonService(void)
{
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *service = (const void *)host->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    CHECK(service && service->header.struct_size >= sizeof(*service));
    CHECK(service->party_slots() == PARTY_SIZE && service->box_count() == TOTAL_BOXES_COUNT
        && service->box_slots() == IN_BOX_COUNT);
    struct Pokemon saved = gParties[B_TRAINER_PLAYER][0];
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER];
    struct BoxPokemon savedBox = gPokemonStoragePtr->boxes[0][0];
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 12345,
                     OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    u8 ev = 100; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPATK_EV, &ev);
    struct RhhOwnedMonSnapshot out = {.struct_size = sizeof(out)};
    CHECK(!service->party_at(0, &out));
    CHECK(out.location == RHH_MON_PARTY && out.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_TORCHIC));
    CHECK(out.level == 5 && out.ivs[RHH_STAT_HP] == 31 && out.evs[RHH_STAT_SP_ATTACK] == 100);
    CHECK(out.max_iv == 31 && out.max_total_ev == 510);
    u64 identity = out.identity;
    CHECK(!service->find(identity, &out) && out.location == RHH_MON_PARTY);
    CHECK(service->party_at(PARTY_SIZE, &out) && service->box_at(TOTAL_BOXES_COUNT, 0, &out));
    struct RhhOwnedMonSnapshot small = {.struct_size = sizeof(small) - 1, .identity = 42};
    CHECK(service->party_at(0, &small) && small.identity == 42);
    gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][0].box;
    CHECK(!service->box_at(0, 0, &out) && out.identity == identity && out.location == RHH_MON_BOX);
    out.identity = 42; CHECK(service->find(identity, &out) && out.identity == 42);
    gPartiesCount[B_TRAINER_PLAYER] = 0;
    CHECK(!service->find(identity, &out) && out.location == RHH_MON_BOX);
    gPokemonStoragePtr->boxes[0][0].secure.raw[0] ^= 1;
    struct BoxPokemon corrupt = gPokemonStoragePtr->boxes[0][0];
    out.identity = 42;
    CHECK(service->box_at(0, 0, &out) && out.identity == 42);
    CHECK(!memcmp(&corrupt, &gPokemonStoragePtr->boxes[0][0], sizeof(corrupt)));
    gPokemonStoragePtr->boxes[0][0] = savedBox;
    gParties[B_TRAINER_PLAYER][0] = saved; gPartiesCount[B_TRAINER_PLAYER] = savedCount;
    puts("owned-mon service: party/PC snapshots, stable identity and ambiguous clones passed");
}
static void CheckOwnedExperienceService(bool32 testCap)
{
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 24));
    CHECK(mons && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_24_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 27)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 28)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 29)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 30)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 31)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 32)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 33)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 34)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 37)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 39)) == &mons->header
        && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_39_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 40)) == &mons->header
        && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_40_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 47)));
    struct Pokemon saved = gParties[B_TRAINER_PLAYER][0], savedFusion = gPokemonStoragePtr->fusions[0];
    struct BoxPokemon savedBox = gPokemonStoragePtr->boxes[0][0], savedDaycare = gSaveBlock1Ptr->daycare.mons[0].mon;
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER];
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
    struct RhhOwnedExperience exp = {.struct_size = sizeof(exp)};
    CHECK(!mons->party_at(0, &mon));
    struct Pokemon original = gParties[B_TRAINER_PLAYER][0]; rng_value_t rng = gRngValue;
    rng_value_t rng2 = gRng2Value;
    struct RhhOwnedStats stats = {.struct_size = sizeof(stats)};
    struct RhhOwnedIvView ivView = {.struct_size = sizeof(ivView)};
    CHECK(!mons->iv_view(&mon, &ivView) && !ivView.hyper_trained
        && ivView.effective_ivs[RHH_STAT_ATTACK] == mon.ivs[RHH_STAT_ATTACK]);
    u32 lowIv = 3, trained = 1;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ATK_IV, &lowIv);
    CHECK(!mons->party_at(0, &mon));
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HYPER_TRAINED_ATK, &trained);
    CHECK(!mons->iv_view(&mon, &ivView) && ivView.mon.ivs[RHH_STAT_ATTACK] == 3
        && ivView.hyper_trained == (1u << RHH_STAT_ATTACK) && ivView.effective_ivs[RHH_STAT_ATTACK] == mon.max_iv);
    gParties[B_TRAINER_PLAYER][0] = original; CHECK(!mons->party_at(0, &mon));
    struct RhhOwnedIvView shortIv = {.struct_size = sizeof(shortIv) - 1, .hyper_trained = 999}, oldShortIv = shortIv;
    CHECK(mons->iv_view(&mon, &shortIv) && !memcmp(&shortIv, &oldShortIv, sizeof(shortIv)));
    CHECK(!mons->stats(&mon, &stats) && !stats.flags && stats.level == original.level
        && stats.current_hp == original.hp && stats.stats[RHH_STAT_HP] == original.maxHP
        && stats.stats[RHH_STAT_ATTACK] == original.attack && stats.stats[RHH_STAT_DEFENSE] == original.defense
        && stats.stats[RHH_STAT_SPEED] == original.speed && stats.stats[RHH_STAT_SP_ATTACK] == original.spAttack
        && stats.stats[RHH_STAT_SP_DEFENSE] == original.spDefense);
    gParties[B_TRAINER_PLAYER][0].hp--;
    CHECK(!mons->stats(&mon, &stats) && stats.current_hp == original.hp - 1);
    gParties[B_TRAINER_PLAYER][0].hp = original.maxHP + 1;
    struct RhhOwnedStats invalidHp = {.struct_size = sizeof(invalidHp), .current_hp = 999}, savedInvalidHp = invalidHp;
    CHECK(mons->stats(&mon, &invalidHp) && !memcmp(&invalidHp, &savedInvalidHp, sizeof(invalidHp)));
    gParties[B_TRAINER_PLAYER][0] = original;
    struct RhhOwnedStats unchanged = {.struct_size = sizeof(unchanged), .current_hp = 999}, savedStats = unchanged;
    struct RhhOwnedMonSnapshot staleStats = mon; ++staleStats.level;
    CHECK(mons->stats(&staleStats, &unchanged) && !memcmp(&unchanged, &savedStats, sizeof(unchanged)));
    struct RhhOwnedIvView staleIv = {.struct_size = sizeof(staleIv), .hyper_trained = 999}, savedStaleIv = staleIv;
    CHECK(mons->iv_view(&staleStats, &staleIv) && !memcmp(&staleIv, &savedStaleIv, sizeof(staleIv)));
    unchanged.struct_size--; savedStats = unchanged;
    CHECK(mons->stats(&mon, &unchanged) && !memcmp(&unchanged, &savedStats, sizeof(unchanged)));
    bool8 savedBattle = gMain.inBattle; gMain.inBattle = TRUE;
    unchanged = (struct RhhOwnedStats){.struct_size = sizeof(unchanged), .current_hp = 999}; savedStats = unchanged;
    struct RhhOwnedIvView unchangedIv = {.struct_size = sizeof(unchangedIv), .hyper_trained = 999}, savedIv = unchangedIv;
    CHECK(mons->stats(&mon, &unchanged) && !memcmp(&unchanged, &savedStats, sizeof(unchanged))
        && mons->iv_view(&mon, &unchangedIv) && !memcmp(&unchangedIv, &savedIv, sizeof(unchangedIv)));
    gMain.inBattle = savedBattle;
    CHECK(!mons->experience(&mon, &exp));
    u32 growth = gSpeciesInfo[SPECIES_TORCHIC].growthRate;
    CHECK(exp.experience == gExperienceTables[growth][5] && exp.level_start_exp == exp.experience
        && exp.next_level_exp == gExperienceTables[growth][6] && exp.to_next_level == exp.next_level_exp - exp.experience
        && exp.maximum_exp == gExperienceTables[growth][MAX_LEVEL] && exp.mon.identity == mon.identity
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    struct RhhExperiencePreview preview = {.struct_size = sizeof(preview)};
    u32 baselineCap = B_EXP_CAP_TYPE == EXP_CAP_HARD ? GetCurrentLevelCap() : MAX_LEVEL;
    u32 baselineFlags = B_EXP_CAP_TYPE == EXP_CAP_HARD ? RHH_EXPERIENCE_HARD_CAP : 0;
    CHECK(baselineCap > 5 && baselineCap <= MAX_LEVEL);
    u32 baselineLimit = gExperienceTables[growth][baselineCap];
    CHECK(!mons->preview_experience(&exp, 1, &preview) && preview.before_exp == exp.experience
        && preview.after_exp == exp.experience + 1 && preview.applied == 1 && preview.after_level == 5);
    CHECK(!mons->preview_experience(&exp, UINT32_MAX, &preview) && preview.applied == baselineLimit - exp.experience
        && preview.after_exp == baselineLimit && preview.after_level == baselineCap && preview.flags == baselineFlags
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    u8 temporaryLevel = 4;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL, &temporaryLevel);
    struct RhhOwnedMonSnapshot inconsistentMon = {.struct_size = sizeof(inconsistentMon)};
    struct RhhOwnedExperience inconsistentExp = {.struct_size = sizeof(inconsistentExp)};
    struct RhhExperiencePreview inconsistentPreview = {.struct_size = sizeof(inconsistentPreview), .applied = 999};
    struct RhhExperiencePreview savedInconsistent = inconsistentPreview;
    CHECK(!mons->party_at(0, &inconsistentMon) && !mons->experience(&inconsistentMon, &inconsistentExp)
        && mons->preview_experience(&inconsistentExp, 100, &inconsistentPreview)
        && !memcmp(&inconsistentPreview, &savedInconsistent, sizeof(inconsistentPreview)));
    temporaryLevel = 5; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL, &temporaryLevel);
    struct RhhExperienceMovePage page = {.struct_size = sizeof(page)};
    CHECK(!mons->preview_experience_moves(&exp, 1, 0, &page) && !page.count && !page.total);
    const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(SPECIES_TORCHIC);
    u32 total = 0;
    for (u32 i = 0; learnset[i].move != LEVEL_UP_MOVE_END; ++i)
        if (learnset[i].level > 5 && learnset[i].level <= baselineCap) ++total;
    for (u32 start = 0; start < total + RHH_EXPERIENCE_MOVE_PAGE_SIZE; start += RHH_EXPERIENCE_MOVE_PAGE_SIZE)
    {
        CHECK(!mons->preview_experience_moves(&exp, UINT32_MAX, start, &page)
            && page.start == start && page.total == total && page.experience.after_level == baselineCap);
        u32 index = 0, found = 0;
        for (u32 i = 0; learnset[i].move != LEVEL_UP_MOVE_END; ++i)
        {
            if (learnset[i].level <= 5 || learnset[i].level > baselineCap) continue;
            if (index++ < start || found == RHH_EXPERIENCE_MOVE_PAGE_SIZE) continue;
            CHECK(found < page.count && page.candidates[found].level == learnset[i].level
                && page.candidates[found].move == RhhContentKeyForId(RHH_MOVE, learnset[i].move));
            ++found;
        }
        CHECK(found == page.count);
    }
    CHECK(!memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision);
    struct RhhOwnedExperience oldExp = exp;
    u32 points = exp.experience + 1; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP, &points);
    struct RhhExperiencePreview untouchedPreview = {.struct_size = sizeof(untouchedPreview), .applied = 999}, oldPreview = untouchedPreview;
    CHECK(mons->preview_experience(&oldExp, 1, &untouchedPreview) && !memcmp(&untouchedPreview, &oldPreview, sizeof(oldPreview)));
    struct RhhExperienceMovePage stalePage = {.struct_size = sizeof(stalePage), .total = 999}, oldPage = stalePage;
    CHECK(mons->preview_experience_moves(&oldExp, UINT32_MAX, 0, &stalePage) && !memcmp(&stalePage, &oldPage, sizeof(oldPage)));
    CHECK(!mons->experience(&mon, &exp) && exp.experience == points && exp.to_next_level == exp.next_level_exp - points);
    struct RhhOwnedExperience untouched = {.struct_size = sizeof(untouched), .experience = 777}, before = untouched;
    struct RhhOwnedMonSnapshot stale = mon; stale.level++;
    CHECK(mons->experience(&stale, &untouched) && !memcmp(&untouched, &before, sizeof(before)));
    struct RhhOwnedExperience shortResult = {.struct_size = sizeof(shortResult) - 1, .experience = 777};
    CHECK(mons->experience(&mon, &shortResult) && shortResult.experience == 777);
    gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][0].box;
    gSaveBlock1Ptr->daycare.mons[0].mon = gParties[B_TRAINER_PLAYER][0].box;
    gPokemonStoragePtr->fusions[0] = gParties[B_TRAINER_PLAYER][0];
    CHECK(!mons->box_at(0, 0, &mon) && !mons->experience(&mon, &exp) && exp.experience == points);
    CHECK(!mons->iv_view(&mon, &ivView) && !ivView.hyper_trained);
    if (testCap) { event.args[0] = 98; RhhModsDispatch(&event, &decision); }
    struct BoxPokemon boxBefore = gPokemonStoragePtr->boxes[0][0];
    CHECK(!mons->stats(&mon, &stats) && stats.flags == RHH_OWNED_STATS_RECONSTRUCTED
        && stats.level == original.level && stats.current_hp == original.hp
        && stats.stats[RHH_STAT_HP] == original.maxHP && stats.stats[RHH_STAT_ATTACK] == original.attack
        && stats.stats[RHH_STAT_DEFENSE] == original.defense && stats.stats[RHH_STAT_SPEED] == original.speed
        && stats.stats[RHH_STAT_SP_ATTACK] == original.spAttack && stats.stats[RHH_STAT_SP_DEFENSE] == original.spDefense
        && !memcmp(&boxBefore, &gPokemonStoragePtr->boxes[0][0], sizeof(boxBefore))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    CHECK(!mons->daycare_at(0, 0, &mon) && !mons->experience(&mon, &exp) && exp.experience == points);
    CHECK(!mons->iv_view(&mon, &ivView) && !ivView.hyper_trained);
    CHECK(!mons->stats(&mon, &stats) && stats.flags == RHH_OWNED_STATS_RECONSTRUCTED
        && stats.stats[RHH_STAT_HP] == original.maxHP && stats.current_hp == original.hp);
    CHECK(!mons->fusion_at(0, &mon) && !mons->experience(&mon, &exp) && exp.experience == points);
    CHECK(!mons->iv_view(&mon, &ivView) && !ivView.hyper_trained);
    CHECK(!mons->stats(&mon, &stats) && !stats.flags && stats.stats[RHH_STAT_HP] == original.maxHP
        && stats.current_hp == original.hp);
    u32 lost = UINT16_MAX; SetBoxMonData(&gPokemonStoragePtr->boxes[0][0], MON_DATA_HP_LOST, &lost);
    CHECK(!mons->box_at(0, 0, &mon) && !mons->stats(&mon, &stats)
        && stats.flags == (RHH_OWNED_STATS_RECONSTRUCTED | RHH_OWNED_STATS_HP_LOSS_CLAMPED) && !stats.current_hp);
    gPokemonStoragePtr->boxes[0][0].secure.raw[0] ^= 1;
    boxBefore = gPokemonStoragePtr->boxes[0][0];
    unchanged = (struct RhhOwnedStats){.struct_size = sizeof(unchanged), .current_hp = 999}; savedStats = unchanged;
    CHECK(mons->stats(&mon, &unchanged) && !memcmp(&unchanged, &savedStats, sizeof(unchanged))
        && !memcmp(&boxBefore, &gPokemonStoragePtr->boxes[0][0], sizeof(boxBefore)) && mons->box_at(0, 0, &mon));
    u32 growthSeen = 0;
    for (u32 species = 1; species < NUM_SPECIES && growthSeen != 0x3f; ++species)
    {
        u32 curve = gSpeciesInfo[species].growthRate;
        if (!gSpeciesInfo[species].baseHP || curve > GROWTH_SLOW || (growthSeen & (1u << curve))) continue;
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], species, 5, 56789, OTID_STRUCT_PRESET(54321), 31);
        CHECK(!mons->party_at(0, &mon) && !mons->experience(&mon, &exp)
            && exp.level_start_exp == gExperienceTables[curve][5] && exp.next_level_exp == gExperienceTables[curve][6]
            && exp.maximum_exp == gExperienceTables[curve][MAX_LEVEL]);
        growthSeen |= 1u << curve;
    }
    CHECK(growthSeen == 0x3f);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, MAX_LEVEL, 98765, OTID_STRUCT_PRESET(54321), 31);
    CHECK(!mons->party_at(0, &mon) && !mons->experience(&mon, &exp)
        && exp.experience == exp.maximum_exp && exp.next_level_exp == exp.maximum_exp && !exp.to_next_level);
    CHECK(!mons->preview_experience(&exp, UINT32_MAX, &preview) && !preview.applied
        && preview.flags == (RHH_EXPERIENCE_MAX_LEVEL | baselineFlags) && preview.after_level == MAX_LEVEL);
    struct RhhExperiencePreview smallPreview = {.struct_size = sizeof(smallPreview) - 1, .applied = 999};
    CHECK(mons->preview_experience(&exp, 10, &smallPreview) && smallPreview.applied == 999);
    bool8 egg = TRUE; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &egg);
    CHECK(!mons->party_at(0, &mon) && mons->experience(&mon, &exp) && mons->stats(&mon, &stats)
        && mons->iv_view(&mon, &ivView));
    if (testCap)
    {
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
        RhhModsSetRunContext(123, 456, 0);
        event.args[0] = 99; RhhModsDispatch(&event, &decision);
        CHECK(!mons->party_at(0, &mon) && !mons->experience(&mon, &exp));
        original = gParties[B_TRAINER_PLAYER][0]; rng = gRngValue;
        CHECK(!mons->preview_experience(&exp, UINT32_MAX, &preview) && preview.flags == RHH_EXPERIENCE_HARD_CAP
            && preview.level_cap == 6 && preview.cap_exp == gExperienceTables[growth][6]
            && preview.after_exp == preview.cap_exp && preview.after_level == 6
            && preview.applied == preview.cap_exp - exp.experience
            && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
        CHECK(!mons->preview_experience_moves(&exp, UINT32_MAX, 0, &page) && page.experience.after_level == 6);
        for (u32 i = 0; i < page.count; ++i) CHECK(page.candidates[i].level == 6);
        points = preview.cap_exp; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP, &points);
        CalculateMonStats(&gParties[B_TRAINER_PLAYER][0]);
        CHECK(!mons->party_at(0, &mon) && !mons->experience(&mon, &exp)
            && !mons->preview_experience(&exp, 100, &preview) && !preview.applied && preview.before_level == 6);
        RhhModsSetRunContext(0, 0, 0);
    }
    gParties[B_TRAINER_PLAYER][0] = saved; gPartiesCount[B_TRAINER_PLAYER] = savedCount;
    gPokemonStoragePtr->boxes[0][0] = savedBox; gSaveBlock1Ptr->daycare.mons[0].mon = savedDaycare;
    gPokemonStoragePtr->fusions[0] = savedFusion;
    puts("owned experience: growth thresholds and safe copied views passed");
}
static void CheckExperienceMoveCustom(const char *module)
{
    InitModsForTestWithLibrary(module); ModsNewGame(7); ModsPrepareLearnsets();
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 28));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
    struct RhhOwnedExperience exp = {.struct_size = sizeof(exp)};
    CHECK(mons && !mons->party_at(0, &mon) && !mons->experience(&mon, &exp));
    struct Pokemon original = gParties[B_TRAINER_PLAYER][0];
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    u32 target = gExperienceTables[gSpeciesInfo[SPECIES_TORCHIC].growthRate][50];
    bool32 found = FALSE;
    struct RhhExperienceMovePage page = {.struct_size = sizeof(page)};
    for (u32 start = 0;; start += RHH_EXPERIENCE_MOVE_PAGE_SIZE)
    {
        CHECK(!mons->preview_experience_moves(&exp, target - exp.experience, start, &page)
            && page.experience.after_level == 50);
        for (u32 i = 0; i < page.count; ++i)
            if (page.candidates[i].move == RhhContentKeyOf("test:move/surge"))
            { CHECK(page.candidates[i].level == 50 && !found); found = TRUE; }
        if (start + page.count >= page.total) break;
        CHECK(page.count == RHH_EXPERIENCE_MOVE_PAGE_SIZE);
    }
    CHECK(found && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    RhhModsUnloadLibraries(); puts("experience moves: registered learnset candidate passed");
}
static void CheckMonEditService(void)
{
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    sEditTestService = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 0));
    CHECK(mons && sEditTestService && sEditTestService->header.struct_size >= sizeof(*sEditTestService));
    struct Pokemon savedParty[PARTY_SIZE]; memcpy(savedParty, gParties[B_TRAINER_PLAYER], sizeof(savedParty));
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER];
    struct BoxPokemon savedBox = gPokemonStoragePtr->boxes[0][0];
    struct DayCare savedDaycare = gSaveBlock1Ptr->daycare;
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    memset(&gPokemonStoragePtr->boxes[0][0], 0, sizeof(savedBox));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 50, 12345, OTID_STRUCT_PRESET(54321), 31);
    u8 hyperTrained = 1;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HYPER_TRAINED_SPDEF, &hyperTrained);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct RhhMonEditRequest req = {.struct_size = sizeof(req), .fields = RHH_EDIT_IVS | RHH_EDIT_EVS | RHH_EDIT_STAT_NATURE};
    req.expected.struct_size = sizeof(req.expected); CHECK(!mons->party_at(0, &req.expected));
    memcpy(req.ivs, req.expected.ivs, sizeof(req.ivs)); memcpy(req.evs, req.expected.evs, sizeof(req.evs));
    req.ivs[RHH_STAT_HP] = 0; req.ivs[RHH_STAT_ATTACK] = 0; req.ivs[RHH_STAT_SP_DEFENSE] = 0;
    req.evs[RHH_STAT_HP] = 252; req.evs[RHH_STAT_ATTACK] = 252;
    req.stat_nature = (req.expected.stat_nature + 1) % NUM_NATURES;
    struct RhhMonEditResult result = {.struct_size = sizeof(result)}, proposal = {.struct_size = sizeof(proposal)};
    sEditTestRequest = &req; sEditConstraints = sEditNotifications = 0; sEditDeny = FALSE;
    CHECK(sEditTestService->preview(&req, &result) == RHH_EDIT_UNAVAILABLE);
    RhhModsSetRunContext(123, 456, 0);
    sEditCheckUnsafe = TRUE;
    struct RhhModEvent unsafe = {.struct_size = sizeof(unsafe), .kind = RHH_WILD_SPECIES};
    struct RhhModDecision unsafeDecision = {.struct_size = sizeof(unsafeDecision), .allowed = 1};
    RhhModsDispatch(&unsafe, &unsafeDecision); CHECK(!sEditCheckUnsafe);
    struct Pokemon original = gParties[B_TRAINER_PLAYER][0];
    rng_value_t rng = gRngValue; s32 battleHP = gBattleScripting.levelUpHP;
    CHECK(sEditTestService->preview(&req, &result) == RHH_EDIT_OK && result.is_preview);
    CHECK(sEditConstraints == 1 && !sEditNotifications && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && battleHP == gBattleScripting.levelUpHP);
    CHECK(result.fields == req.fields && result.after.ivs[RHH_STAT_HP] == 0 && result.after.evs[RHH_STAT_HP] == 252
        && result.after.stat_nature == req.stat_nature && result.after.nature == req.expected.nature);
    CHECK(sEditTestService->context(&proposal)); proposal = result;
    struct RhhMonEditResult tampered = proposal; ++tampered.stats[RHH_STAT_ATTACK];
    CHECK(sEditTestService->commit_previewed(&req, &tampered, &result) == RHH_EDIT_STALE
        && sEditConstraints == 1 && !sEditNotifications && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    gParties[B_TRAINER_PLAYER][0].hp = 1;
    CHECK(sEditTestService->commit_previewed(&req, &proposal, &result) == RHH_EDIT_STALE && !sEditNotifications);
    gParties[B_TRAINER_PLAYER][0] = original;
    hyperTrained = 0; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HYPER_TRAINED_SPDEF, &hyperTrained);
    CHECK(sEditTestService->commit_previewed(&req, &proposal, &result) == RHH_EDIT_STALE && !sEditNotifications);
    gParties[B_TRAINER_PLAYER][0] = original;
    sEditDeny = TRUE;
    CHECK(sEditTestService->commit(&req, &result) == RHH_EDIT_DENIED && !strcmp(result.reason, "test rule denied edit"));
    CHECK(!memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)) && !sEditNotifications);
    sEditDeny = FALSE;
    req.evs[RHH_STAT_SPEED] = 7;
    CHECK(sEditTestService->commit(&req, &result) == RHH_EDIT_INVALID && !sEditNotifications);
    req.evs[RHH_STAT_SPEED] = 0;
    CHECK(sEditTestService->commit_previewed(&req, &proposal, &result) == RHH_EDIT_OK && sEditNotifications == 1);
    CHECK(!result.is_preview && !memcmp(result.stats, proposal.stats, sizeof(result.stats)) && result.current_hp == proposal.current_hp);
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MAX_HP) == result.stats[RHH_STAT_HP]
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ATK) == result.stats[RHH_STAT_ATTACK]
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_SHINY) == GetMonData(&original, MON_DATA_IS_SHINY)
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HYPER_TRAINED_SPDEF));
    struct Pokemon recalculated = gParties[B_TRAINER_PLAYER][0];
    CalculateMonStats(&recalculated);
    CHECK(recalculated.maxHP == result.stats[RHH_STAT_HP] && recalculated.attack == result.stats[RHH_STAT_ATTACK]
        && recalculated.defense == result.stats[RHH_STAT_DEFENSE] && recalculated.speed == result.stats[RHH_STAT_SPEED]
        && recalculated.spAttack == result.stats[RHH_STAT_SP_ATTACK] && recalculated.spDefense == result.stats[RHH_STAT_SP_DEFENSE]);
    gBattleScripting.levelUpHP = battleHP;
    CHECK(sEditTestService->commit(&req, &result) == RHH_EDIT_STALE && sEditNotifications == 1);
    req.expected.struct_size = sizeof(req.expected); CHECK(!mons->party_at(0, &req.expected));
    req.fields = RHH_EDIT_IVS; memcpy(req.ivs, req.expected.ivs, sizeof(req.ivs));
    CHECK(sEditTestService->commit(&req, &result) == RHH_EDIT_INVALID);
    req.ivs[RHH_STAT_HP] = 32;
    CHECK(sEditTestService->commit(&req, &result) == RHH_EDIT_INVALID);
    req.ivs[RHH_STAT_HP] = 31;
    struct Pokemon savedFusion = gPokemonStoragePtr->fusions[0];
    gPokemonStoragePtr->fusions[0] = gParties[B_TRAINER_PLAYER][0];
    CHECK(sEditTestService->commit(&req, &result) == RHH_EDIT_STALE);
    gPokemonStoragePtr->fusions[0].box.secure.raw[0] ^= 1;
    CHECK(sEditTestService->commit(&req, &result) == RHH_EDIT_STALE);
    gPokemonStoragePtr->fusions[0] = savedFusion;
    gSaveBlock1Ptr->daycare.mons[0].mon = gParties[B_TRAINER_PLAYER][0].box;
    CHECK(sEditTestService->commit(&req, &result) == RHH_EDIT_STALE);
    gSaveBlock1Ptr->daycare.mons[0].mon.secure.raw[0] ^= 1;
    CHECK(sEditTestService->commit(&req, &result) == RHH_EDIT_STALE);
    memset(&gSaveBlock1Ptr->daycare.mons[0], 0, sizeof(gSaveBlock1Ptr->daycare.mons[0]));
    gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][0].box;
    CHECK(sEditTestService->commit(&req, &result) == RHH_EDIT_STALE);
    gPartiesCount[B_TRAINER_PLAYER] = 0;
    u32 lost = gParties[B_TRAINER_PLAYER][0].maxHP;
    gParties[B_TRAINER_PLAYER][0].hp = 0;
    SetBoxMonData(&gParties[B_TRAINER_PLAYER][0].box, MON_DATA_HP_LOST, &lost);
    gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][0].box;
    req.expected.struct_size = sizeof(req.expected); CHECK(!mons->box_at(0, 0, &req.expected));
    CHECK(sEditTestService->preview(&req, &proposal) == RHH_EDIT_OK && !proposal.current_hp);
    CHECK(sEditTestService->commit_previewed(&req, &proposal, &result) == RHH_EDIT_OK
        && result.current_hp == 0 && sEditNotifications == 2);
    struct BoxPokemon boxCopy = gPokemonStoragePtr->boxes[0][0];
    CHECK(GetBoxMonData(&boxCopy, MON_DATA_HP_LOST) == result.stats[RHH_STAT_HP]);
    req.expected.struct_size = sizeof(req.expected); CHECK(!mons->box_at(0, 0, &req.expected));
    req.ivs[RHH_STAT_HP] = 0;
    gMain.inBattle = TRUE; CHECK(sEditTestService->commit(&req, &result) == RHH_EDIT_UNAVAILABLE); gMain.inBattle = FALSE;
    CHECK(sEditNotifications == 2);
    sEditOnUpdate = TRUE;
    struct RhhModEvent tick = {.struct_size = sizeof(tick), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&tick, &decision);
    CHECK(!sEditOnUpdate && sEditNotifications == 3);
    memcpy(gParties[B_TRAINER_PLAYER], savedParty, sizeof(savedParty)); gPartiesCount[B_TRAINER_PLAYER] = savedCount;
    gPokemonStoragePtr->boxes[0][0] = savedBox; gSaveBlock1Ptr->daycare = savedDaycare;
    RhhModsSetRunContext(0, 0, 0); sEditTestRequest = NULL; sEditTestService = NULL;
    puts("mon edit service: atomic previews, constraints, stale/invalid rejection and fainted HP passed");
}
static void CheckDaycareService(void)
{
    const struct RhhDaycareService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/daycare", RHH_SERVICE_VERSION(1, 0));
    CHECK(service && service->header.struct_size >= sizeof(*service) && service->parent_slots() == 2);
    struct DayCare saved = gSaveBlock1Ptr->daycare;
    bool8 pending = FlagGet(FLAG_PENDING_DAYCARE_EGG);
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct RhhDaycareSnapshot state = {.struct_size = sizeof(state)};
    struct RhhDaycareParent parent = {.struct_size = sizeof(parent)};
    CHECK(!service->current(&state) && !state.parent_count && !state.compatibility_known && !state.egg_pending);
    CHECK(service->parent_at(0, &parent) && service->parent_at(2, &parent));
    struct Pokemon mon;
    CreateMonWithIVs(&mon, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    u16 item = ITEM_EVERSTONE, move = MOVE_TACKLE;
    SetMonData(&mon, MON_DATA_HELD_ITEM, &item); SetMonData(&mon, MON_DATA_MOVE1, &move);
    gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
    gSaveBlock1Ptr->daycare.mons[0].steps = 42;
    CHECK(!service->current(&state) && state.parent_count == 1 && !state.compatibility_known);
    CHECK(!service->parent_at(0, &parent) && parent.mon.location == RHH_MON_DAYCARE && parent.mon.level == 12
        && parent.mon.ivs[RHH_STAT_HP] == 31 && parent.steps == 42
        && parent.held_item == RhhContentKeyForId(RHH_ITEM, ITEM_EVERSTONE)
        && parent.moves[0] == RhhContentKeyForId(RHH_MOVE, MOVE_TACKLE));
    CreateMonWithIVs(&mon, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = mon.box;
    FlagSet(FLAG_PENDING_DAYCARE_EGG);
    struct DayCare before = gSaveBlock1Ptr->daycare;
    rng_value_t rng = gRngValue;
    CHECK(!service->current(&state) && state.parent_count == 2 && state.egg_pending
        && state.compatibility_known && state.compatibility == RHH_DAYCARE_MEDIUM);
    CHECK(!service->parent_at(1, &parent) && parent.gender == RHH_GENDER_GENDERLESS);
    CHECK(!memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    gSaveBlock1Ptr->daycare.mons[0].mon = gSaveBlock1Ptr->daycare.mons[1].mon;
    CHECK(!service->current(&state) && state.compatibility_known && state.compatibility == RHH_DAYCARE_INCOMPATIBLE);
    gSaveBlock1Ptr->daycare = before;
    u8 isEgg = 1; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon, MON_DATA_IS_EGG, &isEgg);
    CHECK(service->current(&state) && service->parent_at(1, &parent));
    gSaveBlock1Ptr->daycare = before;
    gSaveBlock1Ptr->daycare.mons[1].mon.secure.raw[0] ^= 1;
    before = gSaveBlock1Ptr->daycare;
    state.parent_count = 99; parent.steps = 99;
    CHECK(service->current(&state) && state.parent_count == 99);
    CHECK(service->parent_at(1, &parent) && parent.steps == 99);
    CHECK(!memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before)));
    gSaveBlock1Ptr->daycare = saved;
    if (pending) FlagSet(FLAG_PENDING_DAYCARE_EGG); else FlagClear(FLAG_PENDING_DAYCARE_EGG);
    puts("daycare service: parent snapshots and pure compatibility queries passed");
}
static void CheckDaycareTiming(void)
{
    InitModsForTest(); RhhModsSetRunContext(123, 456, 0);
    const struct RhhDaycareService *daycare = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/daycare", RHH_SERVICE_VERSION(1, 1));
    CHECK(daycare && daycare->header.struct_size >= RHH_DAYCARE_SERVICE_V1_1_SIZE
        && RHH_DAYCARE_SERVICE_V1_0_SIZE < RHH_DAYCARE_SERVICE_V1_1_SIZE
        && RHH_DAYCARE_SERVICE_V1_1_SIZE < RHH_DAYCARE_SERVICE_V1_2_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 0)) == &daycare->header
        && RhhModsHostAPI()->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 2)) == &daycare->header
        && !RhhModsHostAPI()->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 6)));
    struct DayCare old = gSaveBlock1Ptr->daycare;
    struct Pokemon savedParty[PARTY_SIZE]; memcpy(savedParty, gParties[B_TRAINER_PLAYER], sizeof(savedParty));
    u8 count = gPartiesCount[B_TRAINER_PLAYER]; bool32 pending = FlagGet(FLAG_PENDING_DAYCARE_EGG);
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    gPartiesCount[B_TRAINER_PLAYER] = 0; FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct RhhDaycareTiming timing = {.struct_size = sizeof(timing)}, small = {.struct_size = 1, .egg_pending = 99}, saved = small;
    CHECK(daycare->timing(&small) && !memcmp(&small, &saved, sizeof(small)) && daycare->timing(NULL));
    CHECK(!daycare->timing(&timing) && !timing.egg_roll_active && !timing.egg_pending
        && timing.hatch_cycle_length == NativeDaycareHatchCycleLength()
        && timing.hatch_calls == timing.hatch_cycle_length && timing.hatch_cycle_decrement == 1);
    struct Pokemon mon;
    CreateMonWithIVs(&mon, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
    CreateMonWithIVs(&mon, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = mon.box;
    bool32 hadCharm = CheckBagHasItem(ITEM_OVAL_CHARM, 1);
    if (hadCharm) CHECK(RemoveBagItem(ITEM_OVAL_CHARM, 1));
    sBreedingTestService = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 2));
    sBreedingChoice = -1; sBreedingDeny = FALSE;
    gSaveBlock1Ptr->daycare.mons[1].steps = 252;
    CHECK(!daycare->timing(&timing) && timing.egg_roll_calls == 3);
    IncrementDaycareSteps(); CHECK(!daycare->timing(&timing) && timing.egg_roll_calls == 2);
    IncrementDaycareSteps();
    struct DayCare before = gSaveBlock1Ptr->daycare;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(!daycare->timing(&timing) && timing.egg_roll_active && timing.egg_roll_calls == 1
        && timing.egg_chance_percent == 50 && !memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    sBreedingChoice = RHH_DAYCARE_HIGH;
    CHECK(!daycare->timing(&timing) && timing.egg_chance_percent == 70);
    CHECK(AddBagItem(ITEM_OVAL_CHARM, 1));
    CHECK(!daycare->timing(&timing) && timing.egg_chance_percent == 88);
    sBreedingDeny = TRUE; CHECK(!daycare->timing(&timing) && timing.egg_chance_percent == 0);
    sBreedingDeny = FALSE; sBreedingChoice = -1;
    CHECK(RemoveBagItem(ITEM_OVAL_CHARM, 1)); if (hadCharm) CHECK(AddBagItem(ITEM_OVAL_CHARM, 1));
    gSaveBlock1Ptr->daycare.mons[1].steps = 255;
    CHECK(!daycare->timing(&timing) && timing.egg_roll_calls == 256);
    FlagSet(FLAG_PENDING_DAYCARE_EGG);
    gSaveBlock1Ptr->daycare.stepCounter = timing.hatch_cycle_length - 1;
    CHECK(!daycare->timing(&timing) && timing.egg_pending && !timing.egg_roll_active && timing.hatch_calls == 1);
    CHECK(!ShouldEggHatch() && !daycare->timing(&timing) && !timing.hatch_counter
        && timing.hatch_calls == timing.hatch_cycle_length);
    gSaveBlock1Ptr->daycare.stepCounter = UINT32_MAX;
    CHECK(!daycare->timing(&timing) && timing.hatch_calls == timing.hatch_cycle_length + 1);
    CHECK(!ShouldEggHatch() && !daycare->timing(&timing) && !timing.hatch_counter);
    struct RhhDaycareEggProgress progress = {.struct_size = sizeof(progress)},
        tinyProgress = {.struct_size = 1, .cycles_remaining = 999}, savedProgress = tinyProgress;
    CHECK(daycare->egg_progress(0, &progress) && daycare->egg_progress(0, &tinyProgress)
        && !memcmp(&tinyProgress, &savedProgress, sizeof(tinyProgress)));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_MAGCARGO, 20, 123, OTID_STRUCT_PRESET(54321), 31);
    for (u32 i = 1; i < 3; ++i)
    {
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][i], SPECIES_TORCHIC, 5, 12345 + i, OTID_STRUCT_PRESET(54321), 31);
        bool8 egg = TRUE; u8 cycles = i == 1 ? 4 : 1;
        SetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_IS_EGG, &egg);
        SetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_FRIENDSHIP, &cycles);
    }
    gPartiesCount[B_TRAINER_PLAYER] = 3;
    gSaveBlock1Ptr->daycare.stepCounter = NativeDaycareHatchCycleLength() - 1;
    struct Pokemon sampleParty[PARTY_SIZE]; memcpy(sampleParty, gParties[B_TRAINER_PLAYER], sizeof(sampleParty));
    rng = gRngValue; rng2 = gRng2Value;
    CHECK(!daycare->egg_progress(1, &progress) && progress.egg.is_egg && progress.egg.slot == 1
        && progress.cycles_remaining == 4 && progress.hatch_cycle_decrement == 2 && progress.hatch_checks == 3
        && progress.hatch_calls_next == 1 && progress.calls_lower_bound == 1 + 2 * progress.hatch_cycle_length
        && !progress.earlier_ready_eggs && !memcmp(sampleParty, gParties[B_TRAINER_PLAYER], sizeof(sampleParty))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    struct RhhDaycareEggProgress priorProgress = progress;
    CHECK(daycare->egg_progress(0, &progress) && daycare->egg_progress(3, &progress)
        && !memcmp(&progress, &priorProgress, sizeof(progress)));
    gMain.inBattle = TRUE; CHECK(daycare->egg_progress(1, &progress)
        && !memcmp(&progress, &priorProgress, sizeof(progress))); gMain.inBattle = FALSE;
    CHECK(!ShouldEggHatch() && !daycare->egg_progress(1, &progress) && progress.cycles_remaining == 2);
    CHECK(!daycare->egg_progress(2, &progress) && !progress.cycles_remaining && !progress.earlier_ready_eggs);
    gSaveBlock1Ptr->daycare.stepCounter = NativeDaycareHatchCycleLength() - 1;
    CHECK(ShouldEggHatch() && gSpecialVar_0x8004 == 2);
    CHECK(!daycare->egg_progress(2, &progress) && progress.earlier_ready_eggs == 1 && progress.hatch_checks == 1);
    gSaveBlock1Ptr->daycare.stepCounter = NativeDaycareHatchCycleLength() - 1;
    CHECK(ShouldEggHatch() && gSpecialVar_0x8004 == 1);
    sBreedingTestService = NULL; gSaveBlock1Ptr->daycare = old;
    memcpy(gParties[B_TRAINER_PLAYER], savedParty, sizeof(savedParty)); gPartiesCount[B_TRAINER_PLAYER] = count;
    if (pending) FlagSet(FLAG_PENDING_DAYCARE_EGG); else FlagClear(FLAG_PENDING_DAYCARE_EGG);
    RhhModsUnloadLibraries();
    puts("daycare timing: egg roll preview, charm and hatch cycle passed");
}
static void CheckBreedingHooks(void)
{
    sBreedingTestService = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 2));
    CHECK(sBreedingTestService && sBreedingTestService->header.struct_size >= sizeof(*sBreedingTestService));
    struct RhhBreedingContext absent = {.struct_size = sizeof(absent)};
    CHECK(sBreedingTestService->context(&absent));
    struct DayCare savedDaycare = gSaveBlock1Ptr->daycare;
    struct Pokemon savedParty[PARTY_SIZE]; memcpy(savedParty, gParties[B_TRAINER_PLAYER], sizeof(savedParty));
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER];
    bool8 pending = FlagGet(FLAG_PENDING_DAYCARE_EGG);
    rng_value_t savedRng = gRngValue;
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    u16 firstParentMove = MOVE_QUICK_ATTACK; SetMonData(&parent, MON_DATA_MOVE2, &firstParentMove);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_CHARMANDER, 12, 12345, OTID_STRUCT_PRESET(12321), 10);
    u16 inheritedMove = MOVE_SHADOW_BALL; SetMonData(&parent, MON_DATA_MOVE2, &inheritedMove);
    u16 unlearnableMove = MOVE_FLY; SetMonData(&parent, MON_DATA_MOVE3, &unlearnableMove);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    CHECK(GetDaycareCompatibilityScoreBase(&gSaveBlock1Ptr->daycare) == PARENTS_INCOMPATIBLE);
    RhhModsSetRunContext(123, 456, 0);
    sBreedingChoice = RHH_DAYCARE_MEDIUM; sBreedingDeny = FALSE;
    sBreedingQueries = sBreedingEggs = 0;
    sInheritanceMode = sInheritanceMoveMode = 0; sInheritanceCalls = sMoveCalls = 0;
    sInheritanceMove = RhhContentKeyForId(RHH_MOVE, MOVE_SHADOW_BALL); CHECK(sInheritanceMove);
    struct DayCare before = gSaveBlock1Ptr->daycare;
    rng_value_t rng = gRngValue;
    CHECK(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare) == PARENTS_MED_COMPATIBILITY);
    CHECK(sBreedingQueries == 1 && !memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before))
        && !memcmp(&rng, &gRngValue, sizeof(rng)));
    struct RhhDaycareSnapshot view = {.struct_size = sizeof(view)};
    const struct RhhDaycareService *daycare = (const void *)RhhModsHostAPI()->get_service("rhh:service/daycare", RHH_SERVICE_VERSION(1, 0));
    CHECK(!daycare->current(&view) && view.compatibility == RHH_DAYCARE_INCOMPATIBLE && sBreedingQueries == 1);
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct RhhBreedingPreview preview = {.struct_size = sizeof(preview)};
    sBreedingSpecies = RhhContentKeyForId(RHH_SPECIES, SPECIES_EEVEE);
    sBreedingPreviewQueries = 0;
    CHECK(!sBreedingTestService->preview(&preview) && preview.base_compatibility == RHH_DAYCARE_INCOMPATIBLE
        && preview.compatibility == RHH_DAYCARE_MEDIUM && !preview.egg_pending && preview.candidate_count == 1
        && preview.candidates[0].original_offspring == RhhContentKeyForId(RHH_SPECIES, SPECIES_TORCHIC)
        && preview.candidates[0].offspring == sBreedingSpecies && sBreedingPreviewQueries == 3
        && !memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    sBreedingSpecies = 0;
    struct RhhBreedingPreview invalid = {.struct_size = sizeof(invalid) - 1, .candidate_count = 99}, savedPreview = invalid;
    CHECK(sBreedingTestService->preview(&invalid) && !memcmp(&invalid, &savedPreview, sizeof(invalid)));
    sBreedingDeny = TRUE;
    CHECK(!sBreedingTestService->preview(&preview) && preview.compatibility == RHH_DAYCARE_INCOMPATIBLE && !preview.candidate_count);
    sBreedingDeny = FALSE;
    CreateMonWithIVs(&parent, SPECIES_NIDORAN_F, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 54321, OTID_STRUCT_PRESET(12345), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    before = gSaveBlock1Ptr->daycare; rng = gRngValue;
    CHECK(!sBreedingTestService->preview(&preview) && preview.candidate_count == 2
        && preview.candidates[0].offspring == RhhContentKeyForId(RHH_SPECIES, SPECIES_NIDORAN_F)
        && preview.candidates[1].offspring == RhhContentKeyForId(RHH_SPECIES, SPECIES_NIDORAN_M)
        && !memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    struct RhhBreedingOffer oddsOffer = {.struct_size = sizeof(oddsOffer)}, staleOddsOffer;
    struct RhhBreedingOffspringOdds offspringOdds = {.struct_size = sizeof(offspringOdds)}, unchangedOdds;
    CHECK(!sBreedingTestService->offer(&oddsOffer) && !sBreedingTestService->offspring_odds(&oddsOffer, &offspringOdds)
        && offspringOdds.flags == RHH_BREED_OFFSPRING_ODDS_RULES_ACTIVE && offspringOdds.count == 2
        && offspringOdds.outcomes[0].weight == 1 && offspringOdds.outcomes[1].weight == 1);
    staleOddsOffer = oddsOffer; unchangedOdds = offspringOdds;
    sBreedingSpecies = RhhContentKeyForId(RHH_SPECIES, SPECIES_EEVEE);
    CHECK(sBreedingTestService->offspring_odds(&staleOddsOffer, &offspringOdds)
        && !memcmp(&unchangedOdds, &offspringOdds, sizeof(offspringOdds)));
    CHECK(!sBreedingTestService->offer(&oddsOffer) && !sBreedingTestService->offspring_odds(&oddsOffer, &offspringOdds)
        && offspringOdds.count == 1 && offspringOdds.outcomes[0].offspring == sBreedingSpecies
        && offspringOdds.outcomes[0].weight == 2 && offspringOdds.denominator == 2);
    sBreedingSpecies = 0;
    FlagSet(FLAG_PENDING_DAYCARE_EGG); gSaveBlock1Ptr->daycare.offspringPersonality |= EGG_GENDER_MALE;
    CHECK(!sBreedingTestService->preview(&preview) && preview.egg_pending && preview.candidate_count == 1
        && preview.candidates[0].offspring == RhhContentKeyForId(RHH_SPECIES, SPECIES_NIDORAN_M));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(savedParty)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    GiveEggFromDaycare();
    CHECK(sBreedingEggs == 1 && gPartiesCount[B_TRAINER_PLAYER] == 1
        && sLastBreeding.offspring == preview.candidates[0].offspring
        && sLastBreeding.original_offspring == preview.candidates[0].original_offspring
        && sLastBreeding.parent_slots[0] == preview.candidates[0].parent_slots[0]
        && sLastBreeding.parent_slots[1] == preview.candidates[0].parent_slots[1]);
    sBreedingEggs = sInheritanceCalls = sMoveCalls = 0;
    gSaveBlock1Ptr->daycare = before;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonData(&parent, MON_DATA_MOVE2, &firstParentMove); gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_CHARMANDER, 12, 12345, OTID_STRUCT_PRESET(12321), 10);
    SetMonData(&parent, MON_DATA_MOVE2, &inheritedMove); SetMonData(&parent, MON_DATA_MOVE3, &unlearnableMove);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box; before = gSaveBlock1Ptr->daycare;
    sBreedingDeny = TRUE; CHECK(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare) == PARENTS_INCOMPATIBLE);
    sBreedingDeny = FALSE; sBreedingChoice = 999;
    CHECK(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare) == PARENTS_INCOMPATIBLE);
    sBreedingChoice = RHH_DAYCARE_HIGH; FlagClear(FLAG_PENDING_DAYCARE_EGG);
    for (u32 attempt = 0; attempt < 64 && !FlagGet(FLAG_PENDING_DAYCARE_EGG); ++attempt)
    {
        gSaveBlock1Ptr->daycare.mons[1].steps = 254;
        IncrementDaycareSteps();
    }
    CHECK(FlagGet(FLAG_PENDING_DAYCARE_EGG));
    gSaveBlock1Ptr->daycare = before;
    sBreedingChoice = RHH_DAYCARE_MEDIUM;
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = parent;
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE; FlagSet(FLAG_PENDING_DAYCARE_EGG);
    struct Pokemon full[PARTY_SIZE]; memcpy(full, gParties[B_TRAINER_PLAYER], sizeof(full));
    rng = gRngValue; GiveEggFromDaycare();
    CHECK(!sBreedingEggs && FlagGet(FLAG_PENDING_DAYCARE_EGG)
        && !memcmp(full, gParties[B_TRAINER_PLAYER], sizeof(full)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(full)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    sBreedingSpecies = RhhContentKeyOf("test:species/missing");
    GiveEggFromDaycare();
    CHECK(sBreedingEggs == 1 && gPartiesCount[B_TRAINER_PLAYER] == 1
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG)
        && sLastBreeding.offspring == sLastBreeding.original_offspring
        && sLastBreeding.parent_slots[0] == 0 && sLastBreeding.parent_slots[1] == 1);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(full)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagSet(FLAG_PENDING_DAYCARE_EGG); sBreedingSpecies = RhhContentKeyForId(RHH_SPECIES, SPECIES_EEVEE);
    sInheritanceMode = 1; sInheritanceMoveMode = 1;
    GiveEggFromDaycare();
    CHECK(sBreedingEggs == 2 && gPartiesCount[B_TRAINER_PLAYER] == 1
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_EEVEE
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG)
        && sLastBreeding.offspring == sBreedingSpecies && !FlagGet(FLAG_PENDING_DAYCARE_EGG));
    CHECK(sInheritanceCalls == 12 && sMoveCalls == 8 && sLastBreeding.offspring_mon.is_egg
        && sLastBreeding.offspring_mon.location == RHH_MON_PARTY && sLastBreeding.offspring_mon.slot == 0);
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i)
        CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV + i)
              == (i < 2 ? 31 : i < 4 ? 10 : sInitialIVs[i]));
    bool8 foundMove = FALSE;
    for (u32 i = 0; i < 4; ++i)
        if (GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + i) == MOVE_SHADOW_BALL)
        { foundMove = TRUE; CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_PP1 + i) > 0); }
    CHECK(foundMove && !GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_PP_BONUSES));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(full)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagSet(FLAG_PENDING_DAYCARE_EGG); sInheritanceMode = 0; sInheritanceMoveMode = 4;
    sInheritanceMove = RhhContentKeyForId(RHH_MOVE, MOVE_QUICK_ATTACK); GiveEggFromDaycare();
    CHECK(sBreedingEggs == 3 && sEngineMoves[0] && sEngineMoves[1]
        && RhhContentKeyForId(RHH_MOVE, GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1)) == sEngineMoves[1]);
    foundMove = FALSE;
    for (u32 i = 0; i < 4; ++i)
    {
        u32 move = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + i);
        CHECK(!move || RhhContentKeyForId(RHH_MOVE, move) != sEngineMoves[0]);
        if (move == MOVE_QUICK_ATTACK) foundMove = TRUE;
    }
    CHECK(foundMove);
    /* A learnable move absent from both parents and a parent move absent from
     * the offspring's learnsets must each leave the whole moveset unchanged. */
    const u16 rejectedMoves[] = {MOVE_BITE, MOVE_FLY};
    for (u32 n = 0; n < ARRAY_COUNT(rejectedMoves); ++n)
    {
        memset(gParties[B_TRAINER_PLAYER], 0, sizeof(full)); gPartiesCount[B_TRAINER_PLAYER] = 0;
        FlagSet(FLAG_PENDING_DAYCARE_EGG); sInheritanceMoveMode = 5;
        sInheritanceMove = RhhContentKeyForId(RHH_MOVE, rejectedMoves[n]); GiveEggFromDaycare();
        for (u32 i = 0; i < 4; ++i)
        {
            u32 move = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + i);
            CHECK((move ? RhhContentKeyForId(RHH_MOVE, move) : 0) == sEngineMoves[i]);
        }
    }
    CHECK(sBreedingEggs == 5);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(full)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagSet(FLAG_PENDING_DAYCARE_EGG); sInheritanceMoveMode = 6; GiveEggFromDaycare();
    for (u32 i = 0; i < 4; ++i)
    {
        u32 move = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + i);
        CHECK((move ? RhhContentKeyForId(RHH_MOVE, move) : 0) == sEngineMoves[i]);
    }
    CHECK(sBreedingEggs == 6);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(full)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 3333, OTID_STRUCT_PRESET(98765), 10);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    FlagSet(FLAG_PENDING_DAYCARE_EGG); sInheritanceMode = 2; sInheritanceMoveMode = 3; GiveEggFromDaycare();
    CHECK(sBreedingEggs == 7 && gPartiesCount[B_TRAINER_PLAYER] == 1
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_EEVEE
        && sLastBreeding.parent_slots[0] != sLastBreeding.parent_slots[1]);
    for (u32 i = 0; i < RHH_STAT_COUNT; ++i)
        CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV + i) == sEngineIVs[i]);
    for (u32 i = 0; i < 4; ++i)
    {
        u32 move = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + i);
        CHECK((move ? RhhContentKeyForId(RHH_MOVE, move) : 0) == sEngineMoves[i]);
    }
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(full));
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = parent;
    CreateMonWithIVs(&parent, SPECIES_CHARMANDER, 12, 12345, OTID_STRUCT_PRESET(12321), 10);
    gParties[B_TRAINER_PLAYER][5] = parent;
    sInheritanceMode = 0; sInheritanceMoveMode = 2;
    gPartiesCount[B_TRAINER_PLAYER] = 2; FlagSet(FLAG_PENDING_DAYCARE_EGG); GiveEggFromDaycare();
    CHECK(sBreedingEggs == 8 && gPartiesCount[B_TRAINER_PLAYER] == 3
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_TORCHIC
        && GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES) == SPECIES_CHARMANDER
        && GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_SPECIES) == SPECIES_EEVEE);
    for (u32 i = 0; i < 4; ++i)
    {
        u32 move = GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_MOVE1 + i);
        CHECK((move ? RhhContentKeyForId(RHH_MOVE, move) : 0) == sEngineMoves[i]);
    }
    for (u32 i = 0; i < 4; ++i)
        for (u32 j = i + 1; j < 4; ++j)
        {
            u32 left = GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_MOVE1 + i);
            u32 right = GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_MOVE1 + j);
            CHECK(!left || left != right);
        }
    /* With all decisions left at their defaults, staging must preserve both
     * the entire egg and the engine RNG tape. */
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 12, 3333, OTID_STRUCT_PRESET(98765), 10);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(full)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagSet(FLAG_PENDING_DAYCARE_EGG); rng = gRngValue;
    RhhModsSetRunContext(0, 0, 0); GiveEggFromDaycare();
    struct Pokemon defaultEgg = gParties[B_TRAINER_PLAYER][0]; rng_value_t defaultRng = gRngValue;
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(full)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagSet(FLAG_PENDING_DAYCARE_EGG); gRngValue = rng;
    sBreedingChoice = -1; sBreedingSpecies = 0; sInheritanceMode = sInheritanceMoveMode = 0;
    RhhModsSetRunContext(123, 456, 0); GiveEggFromDaycare();
    CHECK(!memcmp(&defaultEgg, &gParties[B_TRAINER_PLAYER][0], sizeof(defaultEgg))
        && !memcmp(&defaultRng, &gRngValue, sizeof(defaultRng)));
    CHECK(sBreedingTestService->context(&absent));
    RhhModsSetRunContext(0, 0, 0); sBreedingTestService = NULL; sBreedingChoice = -1; sBreedingSpecies = 0;
    sInheritanceMode = sInheritanceMoveMode = 0;
    gSaveBlock1Ptr->daycare = savedDaycare;
    memcpy(gParties[B_TRAINER_PLAYER], savedParty, sizeof(savedParty)); gPartiesCount[B_TRAINER_PLAYER] = savedCount;
    if (pending) FlagSet(FLAG_PENDING_DAYCARE_EGG); else FlagClear(FLAG_PENDING_DAYCARE_EGG);
    gRngValue = savedRng;
    puts("breeding: compatible overrides, pure previews, offspring validation, capacity and committed eggs passed");
}
static void CheckExternalBreeding(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsLoadLibrary(path)); CHECK(!RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    struct Pokemon parent;
    CreateMonWithIVs(&parent, SPECIES_TORCHIC, 12, 12345, OTID_STRUCT_PRESET(54321), 31);
    gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_CHARMANDER, 12, 12345, OTID_STRUCT_PRESET(12321), 10);
    u16 move = MOVE_SHADOW_BALL; SetMonData(&parent, MON_DATA_MOVE2, &move);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    CHECK(GetDaycareCompatibilityScoreBase(&gSaveBlock1Ptr->daycare) == PARENTS_INCOMPATIBLE);
    CHECK(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare) == PARENTS_MED_COMPATIBILITY);
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/breeding", RHH_SERVICE_VERSION(1, 2));
    struct RhhBreedingPreview preview = {.struct_size = sizeof(preview)};
    struct DayCare before = gSaveBlock1Ptr->daycare; rng_value_t rng = gRngValue;
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    CHECK(breeding && !breeding->preview(&preview) && preview.compatibility == RHH_DAYCARE_MEDIUM
        && preview.candidate_count == 1 && preview.candidates[0].offspring == RhhContentKeyForId(RHH_SPECIES, SPECIES_EEVEE)
        && !memcmp(&before, &gSaveBlock1Ptr->daycare, sizeof(before)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    memset(gParties[B_TRAINER_PLAYER], 0, PARTY_SIZE * sizeof(struct Pokemon));
    gPartiesCount[B_TRAINER_PLAYER] = 0; FlagSet(FLAG_PENDING_DAYCARE_EGG);
    GiveEggFromDaycare();
    CHECK(gPartiesCount[B_TRAINER_PLAYER] == 1 && !FlagGet(FLAG_PENDING_DAYCARE_EGG)
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG)
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_EEVEE);
    u32 oldTrainerId = 0x11223344;
    CHECK(UpdateBoxMonOtId(&gParties[B_TRAINER_PLAYER][0].box, oldTrainerId));
    gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    gSpecialVar_0x8004 = 0; ScriptHatchMon();
    CHECK(!GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG));
    RhhModsUnloadLibraries();
}
static void CheckEggHatch(void)
{
    sEggHatchTestService = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/egg_hatch", RHH_SERVICE_VERSION(1, 0));
    CHECK(sEggHatchTestService && sEggHatchTestService->header.struct_size >= sizeof(*sEggHatchTestService));
    struct RhhEggHatchContext unavailable = {.struct_size = sizeof(unavailable), .map = 777};
    CHECK(sEggHatchTestService->context(&unavailable) && unavailable.map == 777);
    struct Pokemon savedParty[PARTY_SIZE]; memcpy(savedParty, gParties[B_TRAINER_PLAYER], sizeof(savedParty));
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER];
    struct WarpData savedLocation = gSaveBlock1Ptr->location;
    u8 savedSeen[sizeof(gSaveBlock1Ptr->dexSeen)], savedCaught[sizeof(gSaveBlock1Ptr->dexCaught)];
    memcpy(savedSeen, gSaveBlock1Ptr->dexSeen, sizeof(savedSeen));
    memcpy(savedCaught, gSaveBlock1Ptr->dexCaught, sizeof(savedCaught));
    u8 savedTrainerId[sizeof(gSaveBlock2Ptr->playerTrainerId)], savedName[sizeof(gSaveBlock2Ptr->playerName)];
    memcpy(savedTrainerId, gSaveBlock2Ptr->playerTrainerId, sizeof(savedTrainerId));
    memcpy(savedName, gSaveBlock2Ptr->playerName, sizeof(savedName));
    u8 savedString[sizeof(gStringVar1)]; memcpy(savedString, gStringVar1, sizeof(savedString));
    u16 savedSpecial = gSpecialVar_0x8004;
    rng_value_t savedRng = gRngValue;
    gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    memset(gSaveBlock2Ptr->playerTrainerId, 0x55, sizeof(savedTrainerId));
    memset(gSaveBlock2Ptr->playerName, EOS, sizeof(savedName));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(savedParty));
    CreateEgg(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, FALSE);
    struct BoxPokemon originalBox = gParties[B_TRAINER_PLAYER][0].box;
    CHECK(UpdateBoxMonOtId(&gParties[B_TRAINER_PLAYER][0].box, READ_OTID_FROM_SAVE)
        && !memcmp(&originalBox, &gParties[B_TRAINER_PLAYER][0].box, sizeof(originalBox)));
    struct BoxPokemon corruptBox = originalBox; corruptBox.secure.raw[0] ^= 1;
    struct BoxPokemon corruptBefore = corruptBox;
    CHECK(!UpdateBoxMonOtId(&corruptBox, 0x11223344)
        && !memcmp(&corruptBox, &corruptBefore, sizeof(corruptBox)));
    u32 oldTrainerId = 0x11223344;
    CHECK(UpdateBoxMonOtId(&gParties[B_TRAINER_PLAYER][0].box, oldTrainerId));
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_TORCHIC
        && !GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SANITY_IS_BAD_EGG));
    gPartiesCount[B_TRAINER_PLAYER] = 1; gSpecialVar_0x8004 = 0;
    RhhModsSetRunContext(123, 456, 0); sHatchNotifications = 0;
    rng_value_t beforeHatch = gRngValue;
    ScriptHatchMon();
    CHECK(sHatchNotifications == 1 && sLastHatch.egg.identity != sLastHatch.hatched.identity
        && (u32)sLastHatch.egg.identity == (u32)sLastHatch.hatched.identity
        && (u32)(sLastHatch.egg.identity >> 32) == oldTrainerId
        && (u32)(sLastHatch.hatched.identity >> 32) == READ_OTID_FROM_SAVE
        && sLastHatch.egg.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_TORCHIC)
        && sLastHatch.map == RhhContentKeyForId(RHH_MAP, 1 << 8)
        && !memcmp(sLastHatch.egg.ivs, sLastHatch.hatched.ivs, sizeof(sLastHatch.egg.ivs))
        && !GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SANITY_IS_BAD_EGG)
        && !memcmp(&beforeHatch, &gRngValue, sizeof(beforeHatch)));
    CHECK(sEggHatchTestService->context(&unavailable));
    ScriptHatchMon(); CHECK(sHatchNotifications == 1); /* A non-egg is not a second hatch. */
    gSpecialVar_0x8004 = PARTY_SIZE; ScriptHatchMon(); CHECK(sHatchNotifications == 1);
    u32 friendship = 77; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_FRIENDSHIP, &friendship);
    struct Pokemon beforeInvalid = gParties[B_TRAINER_PLAYER][0];
    gSpecialVar_0x8004 = 256; ScriptHatchMon();
    CHECK(sHatchNotifications == 1 && !memcmp(&beforeInvalid, &gParties[B_TRAINER_PLAYER][0], sizeof(beforeInvalid)));
    RhhModsSetRunContext(0, 0, 0); sEggHatchTestService = NULL;
    memcpy(gParties[B_TRAINER_PLAYER], savedParty, sizeof(savedParty)); gPartiesCount[B_TRAINER_PLAYER] = savedCount;
    gSaveBlock1Ptr->location = savedLocation;
    memcpy(gSaveBlock1Ptr->dexSeen, savedSeen, sizeof(savedSeen));
    memcpy(gSaveBlock1Ptr->dexCaught, savedCaught, sizeof(savedCaught));
    memcpy(gSaveBlock2Ptr->playerTrainerId, savedTrainerId, sizeof(savedTrainerId));
    memcpy(gSaveBlock2Ptr->playerName, savedName, sizeof(savedName));
    memcpy(gStringVar1, savedString, sizeof(savedString)); gSpecialVar_0x8004 = savedSpecial;
    gRngValue = savedRng;
    puts("egg hatch: committed snapshots, OT identity transition and no duplicate event passed");
}
static u32 ClockTestGet32(const u8 *p)
{ return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static void ClockTestPut32(u8 *p, u32 n)
{ for (u32 i = 0; i < 4; ++i) p[i] = n >> (i * 8); }
static void ClockTestPut64(u8 *p, u64 n)
{ for (u32 i = 0; i < 8; ++i) p[i] = n >> (i * 8); }
static void ClockTestEnvelope(u8 *data, u32 size)
{
    u32 checksum = 2166136261u;
    for (u32 i = 24; i < size; ++i) checksum = (checksum ^ data[i]) * 16777619u;
    ClockTestPut32(data + 8, size); ClockTestPut32(data + 16, checksum);
}
static void CheckWorldClock(void)
{
    sClockTestService = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_clock", RHH_SERVICE_VERSION(1, 0));
    CHECK(sClockTestService && sClockTestService->header.struct_size >= sizeof(*sClockTestService));
    struct RhhWorldClockSnapshot clock = {.struct_size = sizeof(clock), .steps = 777};
    struct RhhWorldStepSnapshot context = {.struct_size = sizeof(context), .x = 777};
    CHECK(sClockTestService->current(&clock) && clock.steps == 777);
    CHECK(sClockTestService->step_context(&context) && context.x == 777);
    sWorldStepNotifications = 0; ModsWorldStep(1, 2, 3, 4); CHECK(!sWorldStepNotifications);
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); ClockTestPut64(run + 16, 77);
    RhhModsSetRunContext(123, 456, 0);
    struct WarpData savedLocation = gSaveBlock1Ptr->location;
    gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    rng_value_t rng = gRngValue;
    CHECK(!sClockTestService->current(&clock) && clock.run_id == 456 && clock.steps == 0);
    gMain.inBattle = TRUE; ModsWorldStep(1, 2, 3, 4); gMain.inBattle = FALSE;
    CHECK(!sClockTestService->current(&clock) && !clock.steps && !sWorldStepNotifications);
    ModsWorldStep(-1, 9, 3, 42);
    CHECK(sWorldStepNotifications == 1 && sLastWorldStep.step == 1
        && sLastWorldStep.map == RhhContentKeyForId(RHH_MAP, 1 << 8)
        && sLastWorldStep.x == -1 && sLastWorldStep.y == 9
        && sLastWorldStep.elevation == 3 && sLastWorldStep.behavior == 42);
    ModsWorldStep(2, 3, 0, 8);
    CHECK(sWorldStepNotifications == 2 && !sClockTestService->current(&clock) && clock.steps == 2);
    CHECK(sClockTestService->step_context(&context) && context.x == 777);
    clock.struct_size = 1; clock.steps = 999;
    CHECK(sClockTestService->current(&clock) && clock.steps == 999);
    clock.struct_size = sizeof(clock);
    ClockTestPut64(run + 24, UINT64_C(0xffffffff)); ModsWorldStep(0, 0, 0, 0);
    CHECK(!sClockTestService->current(&clock) && clock.steps == UINT64_C(0x100000000)
        && sWorldStepNotifications == 3);
    RhhContentKey customKey = RhhContentKeyOf("test:move/world_clock"); u32 customSlot = 0;
    CHECK(!RhhModsMoveSlot(customKey, 1, &customSlot) && customSlot == RHH_RUNTIME_MOVE_FIRST);
    ClockTestPut64(run + 40, customKey); /* Duplicate slot mappings are invalid. */
    u8 *invalidExport = NULL; u32 invalidLength = 0;
    CHECK(RhhModsExport(&invalidExport, &invalidLength) && !invalidExport);
    ClockTestPut64(run + 40, 0);
    u8 *snapshot = NULL; u32 length = 0; CHECK(!RhhModsExport(&snapshot, &length));
    ModsWorldStep(0, 0, 0, 0); CHECK(!RhhModsImport(snapshot, length));
    CHECK(!sClockTestService->current(&clock) && clock.steps == UINT64_C(0x100000000));
    /* Convert an actual exported record to the old 24-byte host-run schema. */
    u32 at = 24;
    while (at + 80 <= length && strcmp((const char *)snapshot + at, "rhh:host_run"))
        at += 80 + ClockTestGet32(snapshot + at + 68);
    u32 currentSize = 32 + RHH_RUNTIME_MOVE_COUNT * 8;
    CHECK(at + 80 + currentSize <= length && ClockTestGet32(snapshot + at + 64) == 3
        && ClockTestGet32(snapshot + at + 68) == currentSize);
    u8 *corrupt = malloc(length); CHECK(corrupt); memcpy(corrupt, snapshot, length);
    ClockTestPut64(corrupt + at + 80 + 40, customKey); ClockTestEnvelope(corrupt, length);
    CHECK(RhhModsImport(corrupt, length) && RhhModsMoveKey(customSlot) == customKey);
    free(corrupt);
    /* First migrate the immediately previous 32-byte format. */
    u8 *schema2 = malloc(length); CHECK(schema2);
    memcpy(schema2, snapshot, length);
    u32 length2 = length - (currentSize - 32);
    memmove(schema2 + at + 112, schema2 + at + 80 + currentSize, length - at - 80 - currentSize);
    ClockTestPut32(schema2 + at + 64, 2); ClockTestPut32(schema2 + at + 68, 32); ClockTestEnvelope(schema2, length2);
    CHECK(!RhhModsImport(schema2, length2) && !sClockTestService->current(&clock)
        && clock.run_id == 456 && clock.steps == UINT64_C(0x100000000) && !RhhModsMoveKey(customSlot));
    free(schema2);
    memmove(snapshot + at + 104, snapshot + at + 80 + currentSize, length - at - 80 - currentSize);
    length -= currentSize - 24; ClockTestPut32(snapshot + at + 64, 1); ClockTestPut32(snapshot + at + 68, 24);
    ClockTestEnvelope(snapshot, length);
    CHECK(!RhhModsImport(snapshot, length) && !sClockTestService->current(&clock)
        && clock.run_id == 456 && clock.steps == 0 && run[16] == 77);
    memmove(snapshot + at + 103, snapshot + at + 104, length - at - 104);
    --length; ClockTestPut32(snapshot + at + 68, 23); ClockTestEnvelope(snapshot, length);
    CHECK(RhhModsImport(snapshot, length) && !sClockTestService->current(&clock)
        && clock.steps == 0 && run[16] == 77);
    free(snapshot);
    ClockTestPut64(run + 24, UINT64_MAX); ModsWorldStep(0, 0, 0, 0);
    CHECK(!sClockTestService->current(&clock) && clock.steps == UINT64_MAX && sWorldStepNotifications == 4);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)));
    for (u32 i = 0; i < RHH_RUNTIME_MOVE_COUNT; ++i)
    {
        char id[48]; snprintf(id, sizeof(id), "test:move/fixture_%03u", i);
        u32 slot = 0;
        CHECK(!RhhModsMoveSlot(RhhContentKeyOf(id), 1, &slot) && slot == RHH_RUNTIME_MOVE_FIRST + i
            && RhhModsMoveKey(slot) == RhhContentKeyOf(id));
    }
    u32 overflow = 999;
    CHECK(RhhModsMoveSlot(RhhContentKeyOf("test:move/overflow"), 1, &overflow) && overflow == 999
        && !RhhModsMoveKey(RHH_RUNTIME_MOVE_FIRST - 1) && !RhhModsMoveKey(2048));
    u8 *full = NULL; u32 fullSize = 0; CHECK(!RhhModsExport(&full, &fullSize) && fullSize); free(full);
    gSaveBlock1Ptr->location = savedLocation;
    sClockTestService = NULL; RhhModsResetState(); RhhModsSetRunContext(0, 0, 0);
    puts("world clock: pure queries, step events, rollback, old-save migration and saturation passed");
}
static void CheckWorldTime(const char *module)
{
    InitModsForTestWithLibrary(module);
    const struct RhhWorldTimeService *time = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_time", RHH_SERVICE_VERSION(1, 0));
    CHECK(time && time->header.struct_size >= sizeof(*time)
        && !RhhModsHostAPI()->get_service("rhh:service/world_time", RHH_SERVICE_VERSION(1, 1)));
    struct RhhWorldTimeSnapshot view = {.struct_size = sizeof(view), .local_day = 777};
    CHECK(time->sample(&view) && view.local_day == 777);
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); ClockTestPut64(run + 24, 789);
    RhhModsSetRunContext(123, 456, 0);
    memset(&gSaveBlock2Ptr->localTimeOffset, 0, sizeof(gSaveBlock2Ptr->localTimeOffset));
    rng_value_t random = gRngValue;
    for (u32 hour = 0; hour < 24; ++hour)
    {
        char stamp[32]; snprintf(stamp, sizeof(stamp), "2024-06-01T%02u:15:25", hour);
        CHECK(!NativeRtcSetFixed(stamp));
        struct Time previousTime = gLocalTime; u8 previousPeriod = gTimeOfDay;
        struct TimeBlendSettings previousBlend = gTimeBlend;
        CHECK(!time->sample(&view) && view.source == RHH_TIME_REAL && view.hour == hour
            && view.minute == 15 && view.second == 25 && view.effective_hour == hour
            && !view.override_hour && view.run_id == 456 && view.step == 789
            && !memcmp(&previousTime, &gLocalTime, sizeof(previousTime)) && previousPeriod == gTimeOfDay
            && !memcmp(&previousBlend, &gTimeBlend, sizeof(previousBlend)));
        u32 middle = MORNING_HOUR_BEGIN + (MORNING_HOUR_END - MORNING_HOUR_BEGIN) / 2;
        u32 oldPeriod = IsBetweenHours(hour, MORNING_HOUR_BEGIN, middle)
            || IsBetweenHours(hour, middle, MORNING_HOUR_END) ? TIME_MORNING
            : IsBetweenHours(hour, EVENING_HOUR_BEGIN, EVENING_HOUR_END) ? TIME_EVENING
            : IsBetweenHours(hour, NIGHT_HOUR_BEGIN, NIGHT_HOUR_BEGIN + 1)
                || IsBetweenHours(hour, NIGHT_HOUR_BEGIN, NIGHT_HOUR_END) ? TIME_NIGHT : TIME_DAY;
        u32 gamePeriod = GetTimeOfDay(); CHECK(gamePeriod == oldPeriod);
        CHECK(view.period == (gamePeriod == TIME_MORNING ? RHH_TIME_MORNING
            : gamePeriod == TIME_DAY ? RHH_TIME_DAY : gamePeriod == TIME_EVENING ? RHH_TIME_EVENING : RHH_TIME_NIGHT));
    }
    CHECK(!NativeRtcSetFixed("2024-06-01T05:10:05"));
    CHECK(!time->sample(&view)); s32 originalDay = view.local_day;
    gSaveBlock2Ptr->localTimeOffset.hours = 10; gSaveBlock2Ptr->localTimeOffset.minutes = 20;
    gSaveBlock2Ptr->localTimeOffset.seconds = 30;
    CHECK(!time->sample(&view) && view.local_day == originalDay - 1 && view.hour == 18
        && view.minute == 49 && view.second == 35);
    CHECK(SetTimeOfDay(8) == 0 && !time->sample(&view) && view.hour == 18
        && view.override_hour == 8 && view.effective_hour == 8
        && view.period == (GetTimeOfDay() == TIME_MORNING ? RHH_TIME_MORNING : RHH_TIME_DAY));
    CHECK(SetTimeOfDay(0) == 8);
    /* Each ecology proposal sees a single time, even if the RTC changes
     * during its nested habitat callback. Outside the proposal it is live. */
    memset(&gSaveBlock2Ptr->localTimeOffset, 0, sizeof(gSaveBlock2Ptr->localTimeOffset));
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE101)][MAP_NUM(MAP_ROUTE101)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE101);
    gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE101);
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16));
    CHECK(gBackupMapLayout.map); ResetTasks(); StartWeather();
    sHabitatTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 0));
    CHECK(sHabitatTest); sTimeScopeService = time; sTimeScopeTest = TRUE;
    struct RhhHabitatSample habitat = {.struct_size = sizeof(habitat)};
    CHECK(!NativeRtcSetFixed("2024-06-01T21:15:25") && !sHabitatTest->sample(2, 2, &habitat));
    CHECK(sTimeScopeChecks == 2 && !time->sample(&view) && view.hour == 4);
    struct WildPokemon table[2] = {{.species = SPECIES_POOCHYENA, .minLevel = 2, .maxLevel = 4},
        {.species = SPECIES_ZIGZAGOON, .minLevel = 2, .maxLevel = 4}};
    u16 base[] = {60, 40}, weights[2];
    CHECK(!NativeRtcSetFixed("2024-06-01T21:15:25"));
    CHECK(!ModsWildWeights(table, table, WILD_AREA_LAND, RHH_WILD_STANDARD, RHH_WILD_ROD_NONE,
        TIME_DAY, 2, 2, 0, 2, base, weights));
    CHECK(sTimeScopeChecks == 4 && !time->sample(&view) && view.hour == 4);
    struct RhhWildEncounterContext candidate;
    CHECK(!NativeRtcSetFixed("2024-06-01T21:15:25"));
    CHECK(ModsWildCandidate(SPECIES_POOCHYENA, SPECIES_POOCHYENA, WILD_AREA_LAND, RHH_WILD_STANDARD,
        0, 4, TIME_DAY, 2, 2, &candidate) == SPECIES_POOCHYENA);
    CHECK(sTimeScopeChecks == 6 && !time->sample(&view) && view.hour == 4);
    sTimeScopeTest = FALSE; sTimeScopeService = NULL; sHabitatTest = NULL;
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    gSaveBlock2Ptr->localTimeOffset.hours = 100;
    struct RhhWorldTimeScope unavailable; RhhModsWorldTimeScopeBegin(&unavailable);
    CHECK(!unavailable.available); gSaveBlock2Ptr->localTimeOffset.hours = 0;
    view.local_day = 777; CHECK(time->sample(&view) && view.local_day == 777);
    RhhModsWorldTimeScopeEnd(&unavailable); CHECK(!time->sample(&view) && view.hour == 4);
    struct RhhWorldTimeSnapshot small = {.struct_size = 1, .local_day = 777};
    CHECK(time->sample(&small) && small.local_day == 777 && time->sample(NULL));
    RhhModsSetRunContext(0, 0, 0); view.local_day = 777;
    CHECK(time->sample(&view) && view.local_day == 777 && !memcmp(&random, &gRngValue, sizeof(random)));
    if (module) RhhModsUnloadLibraries();
    puts("world time: sampled clock, periods, offsets, overrides and scoped ecology proposals passed");
}
#if OW_USE_FAKE_RTC
static void CheckWorldTimeFake(void)
{
    InitModsForTest(); u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); ClockTestPut64(run + 24, 789);
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhWorldTimeService *time = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_time", RHH_SERVICE_VERSION(1, 0)); CHECK(time);
    FakeRtc_ManuallySetTime(12, 21, 14, 55);
    struct SiiRtcInfo original = gSaveBlock3Ptr->fakeRTC;
    struct RhhWorldTimeSnapshot sample = {.struct_size = sizeof(sample)};
    rng_value_t random = gRngValue;
    CHECK(!time->sample(&sample) && sample.source == RHH_TIME_FAKE && !sample.flags
        && sample.hour == 21 && sample.minute == 14 && sample.second == 55
        && sample.run_id == 456 && sample.step == 789
        && !memcmp(&original, &gSaveBlock3Ptr->fakeRTC, sizeof(original)));
    FakeRtc_AdvanceTimeBy(0, 3, 0, 10);
    CHECK(!time->sample(&sample) && sample.hour == 0 && sample.minute == 15 && sample.second == 5);
    gSaveBlock3Ptr->fakeRTC.month = 0; sample.local_day = 777;
    CHECK(time->sample(&sample) && sample.local_day == 777 && !memcmp(&random, &gRngValue, sizeof(random)));
    FakeRtc_ManuallySetTime(12, 12, 0, 0);
    gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0; gMain.inBattle = FALSE;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_RAIN_DANCE, 0); gPartiesCount[B_TRAINER_PLAYER] = 1;
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWorldEnvironmentService *env = (const void *)RhhModsHostAPI()->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 4));
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)}; CHECK(mons && env && !mons->party_at(0, &mon));
    struct RhhWorldEnvironmentTimedRequest timed = {.struct_size = sizeof(timed), .duration_seconds = 10,
        .request = {.struct_size = sizeof(timed.request), .kind = RHH_WORLD_WEATHER,
            .map = RhhContentKeyForId(RHH_MAP, 1 << 8), .owner = RhhContentKeyOf("test:mon_edit_rules"),
            .move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE), .actor_identity = mon.identity}};
    struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result)};
    struct RhhWorldEnvironmentSnapshot view = {.struct_size = sizeof(view)};
    struct RhhWorldEnvironmentTiming timing = {.struct_size = sizeof(timing)};
    CHECK(!env->apply_timed(&timed, &result) && !env->timing(timed.request.map, RHH_WORLD_WEATHER, &timing)
        && timing.source == RHH_TIME_FAKE && timing.expires - timing.started == 10);
    FakeRtc_AdvanceTimeBy(0, 0, 0, 9);
    CHECK(!env->current(&view) && view.weather.value == RHH_WEATHER_RAIN);
    FakeRtc_AdvanceTimeBy(0, 0, 0, 1);
    CHECK(!env->current(&view) && !view.weather.value); RhhModsEnvironmentOnFrame();
    CHECK(env->timing(timed.request.map, RHH_WORLD_WEATHER, &timing));
    puts("world time: fake RTC progression, invalid-date rejection and pure query passed");
}
#endif
static void CheckWorldEnvironment(void)
{
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    sWorldEnvironmentTest = (const void *)host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 2));
    CHECK(mons && sWorldEnvironmentTest && sWorldEnvironmentTest->header.struct_size >= sizeof(*sWorldEnvironmentTest));
    CHECK(host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0)) == &sWorldEnvironmentTest->header
        && host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 1)) == &sWorldEnvironmentTest->header
        && host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 3)) == &sWorldEnvironmentTest->header
        && host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 4)) == &sWorldEnvironmentTest->header
        && host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 5)) == &sWorldEnvironmentTest->header
        && host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 6)) == &sWorldEnvironmentTest->header
        && host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 7)) == &sWorldEnvironmentTest->header
        && host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 8)) == &sWorldEnvironmentTest->header
        && host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 9)) == &sWorldEnvironmentTest->header
        && !host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 11)));
    struct Pokemon savedMon = gParties[B_TRAINER_PLAYER][0];
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER], savedInBattle = gMain.inBattle;
    struct WarpData savedLocation = gSaveBlock1Ptr->location;
    rng_value_t savedRng = gRngValue;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_RAIN_DANCE, 0);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_SUNNY_DAY, 1);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_GRASSY_TERRAIN, 2);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_MISTY_TERRAIN, 3);
    gPartiesCount[B_TRAINER_PLAYER] = 1; gMain.inBattle = FALSE;
    gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); ClockTestPut64(run + 24, 0);
    RhhModsSetRunContext(123, 456, 0);
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)}; CHECK(!mons->party_at(0, &mon));
    struct RhhWorldEnvironmentMove move = {.struct_size = sizeof(move), .usable = 77};
    struct Pokemon queryMon = gParties[B_TRAINER_PLAYER][0]; rng_value_t queryRng = gRngValue;
    CHECK(!sWorldEnvironmentTest->party_move(0, 0, &move) && move.usable && move.kind == RHH_WORLD_WEATHER
        && move.value == RHH_WEATHER_RAIN && move.move == RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE)
        && move.actor_identity == mon.identity && move.party_slot == 0 && move.move_slot == 0);
    CHECK(!sWorldEnvironmentTest->party_move(0, 2, &move) && move.kind == RHH_WORLD_TERRAIN
        && move.value == RHH_TERRAIN_GRASSY && move.move == RhhContentKeyForId(RHH_MOVE, MOVE_GRASSY_TERRAIN));
    move.usable = 77; CHECK(sWorldEnvironmentTest->party_move(0, 4, &move) && move.usable == 77);
    CHECK(sWorldEnvironmentTest->party_move(1, 0, &move) && move.usable == 77);
    u32 oldSize = move.struct_size; move.struct_size = 1;
    CHECK(sWorldEnvironmentTest->party_move(0, 0, &move) && move.usable == 77); move.struct_size = oldSize;
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_TACKLE, 3);
    CHECK(sWorldEnvironmentTest->party_move(0, 3, &move) && move.usable == 77);
    gParties[B_TRAINER_PLAYER][0] = queryMon; gParties[B_TRAINER_PLAYER][0].hp = 0;
    CHECK(!sWorldEnvironmentTest->party_move(0, 0, &move) && !move.usable);
    gParties[B_TRAINER_PLAYER][0] = queryMon;
    u8 egg = 1; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &egg);
    CHECK(!sWorldEnvironmentTest->party_move(0, 0, &move) && !move.usable);
    gParties[B_TRAINER_PLAYER][0] = queryMon;
    CHECK(!memcmp(&queryMon, &gParties[B_TRAINER_PLAYER][0], sizeof(queryMon)) && !memcmp(&queryRng, &gRngValue, sizeof(queryRng)));
    RhhContentKey ownerA = RhhContentKeyOf("test:mon_edit_rules"), ownerB = RhhContentKeyOf("test:music_catalog");
    RhhContentKey low = ownerA < ownerB ? ownerA : ownerB, high = ownerA < ownerB ? ownerB : ownerA;
    struct RhhWorldEnvironmentRequest request = {.struct_size = sizeof(request), .kind = RHH_WORLD_WEATHER,
        .map = RhhContentKeyForId(RHH_MAP, 1 << 8), .owner = high, .priority = 10,
        .move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE), .actor_identity = mon.identity, .duration_steps = 3};
    struct RhhWorldEnvironmentResult result = {.struct_size = sizeof(result)};
    struct RhhWorldEnvironmentSnapshot view = {.struct_size = sizeof(view)};
    sWorldEnvironmentRequest = &request; sEnvironmentConstraints = sEnvironmentNotifications = 0; sEnvironmentDeny = FALSE;
    CHECK(sWorldEnvironmentTest->context(&result) && !sWorldEnvironmentTest->current(&view) && !view.weather.value);
    struct Pokemon initialMon = gParties[B_TRAINER_PLAYER][0]; rng_value_t rng = gRngValue;
    request.actor_identity ^= 1; CHECK(sWorldEnvironmentTest->apply(&request, &result) == RHH_WORLD_ENV_STALE); request.actor_identity ^= 1;
    request.duration_steps = 0; CHECK(sWorldEnvironmentTest->apply(&request, &result) == RHH_WORLD_ENV_INVALID); request.duration_steps = 3;
    request.duration_steps = 1000001; CHECK(sWorldEnvironmentTest->apply(&request, &result) == RHH_WORLD_ENV_INVALID); request.duration_steps = 3;
    request.move = RhhContentKeyForId(RHH_MOVE, MOVE_SANDSTORM);
    CHECK(sWorldEnvironmentTest->apply(&request, &result) == RHH_WORLD_ENV_STALE); request.move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE);
    request.owner = 0; CHECK(sWorldEnvironmentTest->apply(&request, &result) == RHH_WORLD_ENV_INVALID); request.owner = high;
    CHECK(!sWorldEnvironmentTest->preview(&request, &result) && result.is_preview && result.after.weather.value == RHH_WEATHER_RAIN
        && result.after.weather.expires_step == 3 && !sEnvironmentNotifications);
    CHECK(!sWorldEnvironmentTest->current(&view) && !view.weather.value);
    struct RhhWorldEnvironmentQuote quote = {.struct_size = sizeof(quote)};
    CHECK(!sWorldEnvironmentTest->preview_checked(&request, &quote) && quote.result.is_preview
        && quote.result.after.weather.value == RHH_WEATHER_RAIN && quote.run_id == 456 && !sEnvironmentNotifications);
    CHECK(sWorldEnvironmentTest->apply_checked(&request, NULL, &result) == RHH_WORLD_ENV_INVALID);
    request.duration_steps = 4;
    CHECK(sWorldEnvironmentTest->apply_checked(&request, &quote, &result) == RHH_WORLD_ENV_STALE);
    request.duration_steps = 3; sEnvironmentDeny = TRUE;
    CHECK(sWorldEnvironmentTest->apply_checked(&request, &quote, &result) == RHH_WORLD_ENV_DENIED
        && !sEnvironmentNotifications);
    sEnvironmentDeny = FALSE;
    struct RhhWorldEnvironmentRequest other = request;
    other.kind = RHH_WORLD_TERRAIN; other.move = RhhContentKeyForId(RHH_MOVE, MOVE_GRASSY_TERRAIN);
    CHECK(!sWorldEnvironmentTest->apply(&other, &result));
    CHECK(sWorldEnvironmentTest->apply_checked(&request, &quote, &result) == RHH_WORLD_ENV_STALE);
    other.move = 0; other.actor_identity = 0; other.duration_steps = 0;
    CHECK(!sWorldEnvironmentTest->apply(&other, &result));
    CHECK(!sWorldEnvironmentTest->preview_checked(&request, &quote)
        && !sWorldEnvironmentTest->apply_checked(&request, &quote, &result)
        && result.after.weather.value == RHH_WEATHER_RAIN);
    other = request; other.move = 0; other.actor_identity = 0; other.duration_steps = 0;
    CHECK(!sWorldEnvironmentTest->preview_checked(&other, &quote)
        && quote.before_timing.clock == RHH_WORLD_ENV_CLOCK_STEPS && !quote.after_timing.clock
        && !sWorldEnvironmentTest->apply_checked(&other, &quote, &result));
    sEnvironmentConstraints = sEnvironmentNotifications = 0;
    sEnvironmentDeny = TRUE;
    CHECK(sWorldEnvironmentTest->apply(&request, &result) == RHH_WORLD_ENV_DENIED && !strcmp(result.reason, "environment denied"));
    sEnvironmentDeny = FALSE;
    CHECK(!sWorldEnvironmentTest->apply(&request, &result) && result.cause == RHH_WORLD_ENV_SET && sEnvironmentNotifications == 1);
    request.owner = low; request.move = RhhContentKeyForId(RHH_MOVE, MOVE_SUNNY_DAY);
    CHECK(!sWorldEnvironmentTest->apply(&request, &result) && result.cause == RHH_WORLD_ENV_REPLACED && result.after.weather.value == RHH_WEATHER_SUN);
    request.owner = high; request.move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE);
    CHECK(sWorldEnvironmentTest->apply(&request, &result) == RHH_WORLD_ENV_CONFLICT);
    request.move = 0; request.actor_identity = 0; request.duration_steps = 0;
    CHECK(sWorldEnvironmentTest->apply(&request, &result) == RHH_WORLD_ENV_CONFLICT);
    request.move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE); request.actor_identity = mon.identity; request.duration_steps = 3;
    request.priority = 11;
    CHECK(!sWorldEnvironmentTest->apply(&request, &result) && result.after.weather.value == RHH_WEATHER_RAIN);
    request.kind = RHH_WORLD_TERRAIN; request.move = RhhContentKeyForId(RHH_MOVE, MOVE_GRASSY_TERRAIN); request.duration_steps = 5;
    CHECK(!sWorldEnvironmentTest->apply(&request, &result) && result.after.terrain.value == RHH_TERRAIN_GRASSY);
    CHECK(!memcmp(&initialMon, &gParties[B_TRAINER_PLAYER][0], sizeof(initialMon)) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    u8 *snapshot = NULL; u32 length = 0; CHECK(!RhhModsExport(&snapshot, &length));

    /* Exercise the actual battle-start paths with real engine globals. */
    struct BattleResources *savedResources = gBattleResources; struct BattleStruct *savedStruct = gBattleStruct;
    struct BattleScriptsStack scripts = {0}; struct BattleCallbacksStack callbacks = {0};
    struct BattleResources resources = {.battleScriptsStack = &scripts, .battleCallbackStack = &callbacks};
    struct BattleStruct battle = {0}; typeof(gFieldTimers) savedTimers = gFieldTimers;
    typeof(gStartingStatuses) savedStatuses = gStartingStatuses; typeof(gBattleScripting) savedScripting = gBattleScripting;
    TrainerBattleParameter savedTrainerParameter = gTrainerBattleParameter;
    u8 savedCommunication[sizeof(gBattleCommunication)]; memcpy(savedCommunication, gBattleCommunication, sizeof(savedCommunication));
    u16 savedWeather = gBattleWeather; u32 savedFlags = gBattleTypeFlags; u8 savedFieldWeather = gWeatherPtr->currWeather;
    u32 savedExecFlags = gBattleControllerExecFlags, savedHitMarker = gHitMarker;
    const u8 *savedInstruction = gBattlescriptCurrInstr; void (*savedMain)(void) = gBattleMainFunc;
    gBattleResources = &resources; gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattleTypeFlags = 0; gBattleWeather = 0; gWeatherPtr->currWeather = WEATHER_NONE;
    memset(&gFieldTimers, 0, sizeof(gFieldTimers)); memset(&gStartingStatuses, 0, sizeof(gStartingStatuses));
    /* Real battle script opcodes allow the animation argument pointer to be NULL. */
    u8 animation = B_ANIM_SUN_CONTINUES; u16 animationArg = 0x1234;
    struct __attribute__((packed)) { u8 opcode, battler, animation; const u16 *arg; } fixedAnimation =
        {B_SCR_OP_PLAYANIMATION, BS_BATTLER_0, B_ANIM_SUN_CONTINUES, NULL};
    struct __attribute__((packed)) { u8 opcode, battler; const u8 *animation; const u16 *arg; } variableAnimation =
        {B_SCR_OP_PLAYANIMATION_VAR, BS_BATTLER_0, &animation, NULL};
    gHitMarker = 0; gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = (const u8 *)&fixedAnimation;
    gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION]();
    CHECK(resources.bufferA[0][1] == B_ANIM_SUN_CONTINUES && !resources.bufferA[0][2] && !resources.bufferA[0][3]
        && gBattlescriptCurrInstr == (const u8 *)&fixedAnimation + sizeof(fixedAnimation));
    fixedAnimation.arg = &animationArg; gBattlescriptCurrInstr = (const u8 *)&fixedAnimation;
    gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION]();
    CHECK(resources.bufferA[0][2] == 0x34 && resources.bufferA[0][3] == 0x12);
    gBattlescriptCurrInstr = (const u8 *)&variableAnimation;
    gBattleScriptingCommandsTable[B_SCR_OP_PLAYANIMATION_VAR]();
    CHECK(resources.bufferA[0][1] == B_ANIM_SUN_CONTINUES && !resources.bufferA[0][2] && !resources.bufferA[0][3]
        && gBattlescriptCurrInstr == (const u8 *)&variableAnimation + sizeof(variableAnimation));
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = NULL;
    sSeedTransforms = sSeedConstraints = sSeedDenyKind = sSeedExpectedFlags = 0;
    sSeedExpectedTrainers[0] = sSeedExpectedTrainers[1] = 0; sSeedOverride = sSeedTryUndeny = sSeedImpure = FALSE;
    struct RhhBattleEnvironmentSeed seedContext = {.struct_size = sizeof(seedContext), .initial_value = 42};
    CHECK(sWorldEnvironmentTest->battle_seed_context(&seedContext) && seedContext.initial_value == 42);
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && (gBattleWeather & B_WEATHER_RAIN) && !gBattleStruct->weatherDuration);
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_TERRAIN) && gFieldTimers.terrain == B_TERRAIN_GRASSY && !gFieldTimers.terrainTimer);
    CHECK(sSeedTransforms == 2 && sSeedConstraints == 2 && sWorldEnvironmentTest->battle_seed_context(&seedContext));
    scripts.size = callbacks.size = 0; gBattleWeather = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers));
    sSeedOverride = TRUE; sSeedWeather = RHH_WEATHER_SUN; sSeedTerrain = RHH_TERRAIN_MISTY;
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && (gBattleWeather & B_WEATHER_SUN));
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_TERRAIN) && gFieldTimers.terrain == B_TERRAIN_MISTY);
    scripts.size = callbacks.size = 0; gBattleWeather = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers));
    rng_value_t seedRng = gRngValue, seedRng2 = gRng2Value;
    sSeedImpure = TRUE;
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && (gBattleWeather & B_WEATHER_RAIN)
        && !memcmp(&seedRng, &gRngValue, sizeof(seedRng)) && !memcmp(&seedRng2, &gRng2Value, sizeof(seedRng2)));
    scripts.size = callbacks.size = 0; gBattleWeather = 0; sSeedDenyKind = RHH_WORLD_WEATHER;
    CHECK(!TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && !gBattleWeather
        && !memcmp(&seedRng, &gRngValue, sizeof(seedRng)) && !memcmp(&seedRng2, &gRng2Value, sizeof(seedRng2)));
    sSeedImpure = FALSE;
    sSeedDenyKind = RHH_WORLD_WEATHER; sSeedTryUndeny = TRUE;
    CHECK(!TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && !gBattleWeather);
    sSeedDenyKind = 0; sSeedTryUndeny = FALSE; sSeedWeather = UINT64_MAX;
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && (gBattleWeather & B_WEATHER_RAIN));
    scripts.size = callbacks.size = 0; gBattleWeather = 0; sSeedWeather = RHH_WEATHER_NONE;
    CHECK(!TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && !gBattleWeather);
    sSeedWeather = RHH_WEATHER_FOG;
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && (gBattleWeather & B_WEATHER_FOG));
    scripts.size = callbacks.size = 0; gBattleWeather = 0; sSeedWeather = RHH_WEATHER_WIND;
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && (gBattleWeather & B_WEATHER_STRONG_WINDS));
    scripts.size = callbacks.size = 0; gBattleWeather = 0; sSeedWeather = RHH_WEATHER_HARSH_SUN;
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && (gBattleWeather & B_WEATHER_SUN_PRIMAL));
    scripts.size = callbacks.size = 0; gBattleWeather = 0; sSeedWeather = RHH_WEATHER_HEAVY_RAIN;
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && (gBattleWeather & B_WEATHER_RAIN_PRIMAL));
    scripts.size = callbacks.size = 0; gBattleWeather = 0; gSaveBlock1Ptr->location.mapNum = 1; sSeedWeather = RHH_WEATHER_SUN;
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && (gBattleWeather & B_WEATHER_SUN));
    gSaveBlock1Ptr->location.mapNum = 0; sSeedOverride = FALSE;
    scripts.size = callbacks.size = 0; gBattleWeather = 0;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE;
    TRAINER_BATTLE_PARAM.opponentA = TRAINER_ROXANNE_1; TRAINER_BATTLE_PARAM.opponentB = TRAINER_SAWYER_1;
    sSeedExpectedFlags = RHH_BATTLE_SEED_TRAINER | RHH_BATTLE_SEED_DOUBLE;
    sSeedExpectedTrainers[0] = RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1);
    sSeedExpectedTrainers[1] = RhhContentKeyForId(RHH_TRAINER, TRAINER_SAWYER_1);
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && (gBattleWeather & B_WEATHER_RAIN));
    gBattleTypeFlags = 0; sSeedExpectedFlags = 0; sSeedExpectedTrainers[0] = sSeedExpectedTrainers[1] = 0;
    scripts.size = callbacks.size = 0; gBattleWeather = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers));
    unsigned seedCalls = sSeedTransforms;
    gStartingStatuses.weatherSunTemporary = TRUE; gStartingStatuses.mistyTerrainTemporary = TRUE;
    CHECK(!TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && !gBattleWeather);
    CHECK(!TryFieldEffects(FIELD_EFFECT_OVERWORLD_TERRAIN) && !gFieldTimers.terrain && sSeedTransforms == seedCalls);
    memset(&gStartingStatuses, 0, sizeof(gStartingStatuses)); gWeatherPtr->currWeather = WEATHER_SANDSTORM;
    /* A saved local effect is the effective weather even when the map has
     * different natural weather; explicit starting statuses still win. */
    CHECK(TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && (gBattleWeather & B_WEATHER_RAIN));
    ++seedCalls;
    scripts.size = callbacks.size = 0; gBattleWeather = 0; gWeatherPtr->currWeather = WEATHER_NONE; gBattleTypeFlags = BATTLE_TYPE_LINK;
    CHECK(!TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && !TryFieldEffects(FIELD_EFFECT_OVERWORLD_TERRAIN));
    gBattleTypeFlags = BATTLE_TYPE_RECORDED;
    CHECK(!TryFieldEffects(FIELD_EFFECT_OVERWORLD_WEATHER) && !TryFieldEffects(FIELD_EFFECT_OVERWORLD_TERRAIN)
        && sSeedTransforms == seedCalls);
    gBattleResources = savedResources; gBattleStruct = savedStruct; gFieldTimers = savedTimers; gStartingStatuses = savedStatuses;
    gTrainerBattleParameter = savedTrainerParameter;
    gBattleScripting = savedScripting; memcpy(gBattleCommunication, savedCommunication, sizeof(savedCommunication));
    gBattleWeather = savedWeather; gBattleTypeFlags = savedFlags; gWeatherPtr->currWeather = savedFieldWeather;
    gBattleControllerExecFlags = savedExecFlags; gHitMarker = savedHitMarker;
    gBattlescriptCurrInstr = savedInstruction; gBattleMainFunc = savedMain; gMain.inBattle = FALSE;

    gSaveBlock1Ptr->location.mapNum = 1;
    for (unsigned i = 0; i < 3; ++i) ModsWorldStep(0, 0, 0, 0);
    CHECK(!sWorldEnvironmentTest->at(request.map, &view) && !view.weather.value && view.terrain.value == RHH_TERRAIN_GRASSY
        && sLastEnvironment.cause == RHH_WORLD_ENV_EXPIRED && sLastEnvironment.kind == RHH_WORLD_WEATHER);
    for (unsigned i = 0; i < 2; ++i) ModsWorldStep(0, 0, 0, 0);
    CHECK(!sWorldEnvironmentTest->at(request.map, &view) && !view.weather.value && !view.terrain.value);
    CHECK(!RhhModsImport(snapshot, length) && !sWorldEnvironmentTest->at(request.map, &view)
        && view.step == 0 && view.weather.value == RHH_WEATHER_RAIN && view.terrain.value == RHH_TERRAIN_GRASSY);
    free(snapshot);
    gSaveBlock1Ptr->location.mapNum = 0; request.duration_steps = 3;
    CHECK(!sWorldEnvironmentTest->apply(&request, &result));
    unsigned notifications = sEnvironmentNotifications;
    for (unsigned i = 0; i < 3; ++i) ModsWorldStep(0, 0, 0, 0);
    CHECK(sEnvironmentNotifications == notifications + 1 && sLastEnvironment.kind == 0
        && sLastEnvironment.cause == RHH_WORLD_ENV_EXPIRED && !sLastEnvironment.after.weather.value && !sLastEnvironment.after.terrain.value);
    request.kind = RHH_WORLD_WEATHER; request.move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE);
    CHECK(!sWorldEnvironmentTest->apply(&request, &result));
    request.move = 0; request.actor_identity = 0; request.duration_steps = 0;
    CHECK(!sWorldEnvironmentTest->apply(&request, &result) && result.cause == RHH_WORLD_ENV_CLEARED);
    CHECK(!sWorldEnvironmentTest->current(&view) && !view.weather.value);
    gSaveBlock1Ptr->location = savedLocation; gParties[B_TRAINER_PLAYER][0] = savedMon;
    gPartiesCount[B_TRAINER_PLAYER] = savedCount; gMain.inBattle = savedInBattle; gRngValue = savedRng;
    sWorldEnvironmentTest = NULL; sWorldEnvironmentRequest = NULL;
    RhhModsResetState(); RhhModsSetRunContext(0, 0, 0);
    puts("world environment: preview, conflicts, battle proposals and precedence, expiry and rollback passed");
}
static void CheckHabitatService(void)
{
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhWorldEnvironmentService *env = (const void *)host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    const struct RhhOwnedMonService *mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    sHabitatTest = (const void *)host->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 0));
    CHECK(env && mons && sHabitatTest && sHabitatTest->header.struct_size >= sizeof(*sHabitatTest));
    CHECK(host->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 1)) == &sHabitatTest->header
        && host->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 2)) == &sHabitatTest->header
        && host->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 3)) == &sHabitatTest->header
        && !host->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 8)));
    const struct MapLayout *savedLayout = gMapHeader.mapLayout;
    struct MapLayout layout = *savedLayout; struct Tileset primary = *layout.primaryTileset;
    u16 attributes[1] = {MB_TALL_GRASS}; primary.metatileAttributes = attributes;
    layout.primaryTileset = &primary; CHECK(!layout.isFrlg); gMapHeader.mapLayout = &layout;
    u8 savedMapType = gMapHeader.mapType, savedWeather = gWeatherPtr->currWeather;
    struct Pokemon savedMon = gParties[B_TRAINER_PLAYER][0]; u8 savedCount = gPartiesCount[B_TRAINER_PLAYER];
    gMapHeader.mapType = MAP_TYPE_ROUTE; gWeatherPtr->currWeather = WEATHER_SANDSTORM;
    struct RhhHabitatSample sample = {.struct_size = sizeof(sample)}, original;
    sHabitatQueries = 0; sHabitatTransform = sHabitatDeny = sHabitatInvalid = FALSE;
    CHECK(!sHabitatTest->sample(0, 0, &sample) && !sample.environment_available && !sHabitatQueries
        && sample.natural_weather == RHH_NATURAL_WEATHER_SANDSTORM && sample.engine_weather == WEATHER_SANDSTORM
        && sample.tile.behavior == MB_TALL_GRASS
        && sample.base_flags == (RHH_HABITAT_GRASS | RHH_HABITAT_TALL_GRASS | RHH_HABITAT_ENCOUNTER_TILE)
        && sample.flags == sample.base_flags && sample.permitted);
    CHECK(sHabitatTest->context(&sample));
    const struct { u8 engine; u32 semantic; } weatherCases[] = {
        {WEATHER_NONE, RHH_NATURAL_WEATHER_NONE}, {WEATHER_SUNNY, RHH_NATURAL_WEATHER_SUNNY},
        {WEATHER_RAIN, RHH_NATURAL_WEATHER_RAIN}, {WEATHER_RAIN_THUNDERSTORM, RHH_NATURAL_WEATHER_THUNDERSTORM},
        {WEATHER_FOG_DIAGONAL, RHH_NATURAL_WEATHER_FOG}, {WEATHER_VOLCANIC_ASH, RHH_NATURAL_WEATHER_ASH},
        {WEATHER_DROUGHT, RHH_NATURAL_WEATHER_DROUGHT}, {WEATHER_DOWNPOUR, RHH_NATURAL_WEATHER_DOWNPOUR},
        {WEATHER_ROUTE119_CYCLE, RHH_NATURAL_WEATHER_DYNAMIC}, {255, RHH_NATURAL_WEATHER_UNKNOWN}
    };
    for (unsigned i = 0; i < ARRAY_COUNT(weatherCases); ++i)
    {
        gWeatherPtr->currWeather = weatherCases[i].engine;
        CHECK(!sHabitatTest->sample(0, 0, &sample) && sample.engine_weather == weatherCases[i].engine
            && sample.natural_weather == weatherCases[i].semantic);
    }
    gWeatherPtr->currWeather = WEATHER_SANDSTORM;
    original = sample; CHECK(sHabitatTest->sample(-1, 0, &sample) && !memcmp(&sample, &original, sizeof(sample)));
    struct WarpData knownLocation = gSaveBlock1Ptr->location; gSaveBlock1Ptr->location.mapGroup = 255;
    CHECK(sHabitatTest->sample(0, 0, &sample) && !memcmp(&sample, &original, sizeof(sample)));
    gSaveBlock1Ptr->location = knownLocation;
    sample.struct_size = 1; original = sample;
    CHECK(sHabitatTest->sample(0, 0, &sample) && !memcmp(&sample, &original, sizeof(sample)));
    sample.struct_size = sizeof(sample);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_RAIN_DANCE, 0); gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)}; CHECK(!mons->party_at(0, &mon));
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); ClockTestPut64(run + 24, 0); RhhModsSetRunContext(123, 456, 0);
    struct RhhWorldEnvironmentRequest request = {.struct_size = sizeof(request), .kind = RHH_WORLD_WEATHER,
        .map = RhhContentKeyForId(RHH_MAP, 1 << 8), .owner = RhhContentKeyOf("test:music_catalog"),
        .move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE), .actor_identity = mon.identity, .duration_steps = 1};
    struct RhhWorldEnvironmentResult changed = {.struct_size = sizeof(changed)};
    CHECK(!env->apply(&request, &changed));
    rng_value_t rng = gRngValue; sHabitatTransform = TRUE;
    CHECK(!sHabitatTest->sample(0, 0, &sample) && sample.environment_available
        && sample.environment.weather.value == RHH_WEATHER_RAIN && sample.environment.step == 0
        && sample.natural_weather == RHH_NATURAL_WEATHER_SANDSTORM && sample.engine_weather == WEATHER_SANDSTORM
        && (sample.flags & RHH_HABITAT_WATER)
        && !(sample.base_flags & RHH_HABITAT_WATER) && sHabitatQueries == 1);
    sHabitatInvalid = TRUE; CHECK(!sHabitatTest->sample(0, 0, &sample) && sample.flags == sample.base_flags && sample.permitted);
    sHabitatInvalid = FALSE; sHabitatDeny = TRUE;
    CHECK(!sHabitatTest->sample(0, 0, &sample) && !sample.permitted && !sample.flags);
    rng_value_t rng2 = gRng2Value;
    for (sHabitatImpure = 1; sHabitatImpure <= 2; ++sHabitatImpure)
    {
        sHabitatDeny = FALSE;
        CHECK(!sHabitatTest->sample(0, 0, &sample) && !sample.permitted && !sample.flags
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        struct RhhHabitatSearchRequest search = {.struct_size = sizeof(search), .x = 0, .y = 0,
            .flags = RHH_HABITAT_SEARCH_INCLUDE_ORIGIN};
        struct RhhHabitatSearchResult found = {.struct_size = sizeof(found)};
        CHECK(!sHabitatTest->search(&search, &found) && !found.found && found.scanned == 1 && found.denied == 1
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        sHabitatDeny = TRUE;
        CHECK(!sHabitatTest->sample(0, 0, &sample) && !sample.permitted && !sample.flags
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    }
    sHabitatImpure = 0;
    sHabitatDeny = FALSE; sHabitatTransform = FALSE;
    const struct { u16 behavior; u32 flags; } cases[] = {
        {MB_LONG_GRASS, RHH_HABITAT_GRASS | RHH_HABITAT_LONG_GRASS | RHH_HABITAT_ENCOUNTER_TILE},
        {MB_SHORT_GRASS, RHH_HABITAT_GRASS}, {MB_DEEP_WATER, RHH_HABITAT_WATER},
        {MB_SHALLOW_WATER, RHH_HABITAT_WATER | RHH_HABITAT_SHALLOW_WATER},
        {MB_SAND, RHH_HABITAT_SAND}, {MB_ICE, RHH_HABITAT_ICE}
    };
    for (unsigned i = 0; i < ARRAY_COUNT(cases); ++i)
    { attributes[0] = cases[i].behavior; CHECK(!sHabitatTest->sample(0, 0, &sample) && (sample.flags & cases[i].flags) == cases[i].flags); }
    gMapHeader.mapType = MAP_TYPE_UNDERGROUND; CHECK(!sHabitatTest->sample(0, 0, &sample) && (sample.flags & RHH_HABITAT_UNDERGROUND));
    gMapHeader.mapType = MAP_TYPE_UNDERWATER; CHECK(!sHabitatTest->sample(0, 0, &sample) && (sample.flags & RHH_HABITAT_UNDERWATER));
    gMapHeader.mapType = MAP_TYPE_INDOOR; CHECK(!sHabitatTest->sample(0, 0, &sample) && (sample.flags & RHH_HABITAT_INDOORS));
    attributes[0] = MB_INVALID; original = sample;
    CHECK(sHabitatTest->sample(0, 0, &sample) && !memcmp(&sample, &original, sizeof(sample)));
    attributes[0] = MB_TALL_GRASS; ModsWorldStep(0, 0, 0, 0);
    CHECK(!sHabitatTest->sample(0, 0, &sample) && sample.environment.step == 1 && !sample.environment.weather.value);
    gMain.inBattle = TRUE; original = sample;
    CHECK(sHabitatTest->sample(0, 0, &sample) && !memcmp(&sample, &original, sizeof(sample))); gMain.inBattle = FALSE;
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)));
    gMapHeader.mapLayout = savedLayout; gMapHeader.mapType = savedMapType; gWeatherPtr->currWeather = savedWeather;
    gParties[B_TRAINER_PLAYER][0] = savedMon; gPartiesCount[B_TRAINER_PLAYER] = savedCount;
    sHabitatTest = NULL; RhhModsResetState(); RhhModsSetRunContext(0, 0, 0);
    puts("habitat: semantic tiles, environment, mod composition, denial and pure queries passed");
}
static void CheckWildEncounterService(void)
{
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhWorldEnvironmentService *env = (const void *)host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    const struct RhhOwnedMonService *mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    sWildTest = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 0));
    CHECK(env && mons && sWildTest && sWildTest->header.struct_size >= sizeof(*sWildTest)
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 1)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 2)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 3)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 4)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 5)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 6)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 7)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 8)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 9)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 10)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 11)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 12)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 13)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 14)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 15)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 16)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 17)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 18)) == &sWildTest->header
        && host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 19)) == &sWildTest->header
        && !host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 21)));
    struct Pokemon savedMon = gParties[B_TRAINER_PLAYER][0], savedEnemy[PARTY_SIZE];
    memcpy(savedEnemy, gParties[B_TRAINER_OPPONENT_A], sizeof(savedEnemy));
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER], savedEnemyCount = gPartiesCount[B_TRAINER_OPPONENT_A];
    struct ObjectEvent savedObject = gObjectEvents[0]; u8 savedObjectId = gPlayerAvatar.objectEventId;
    rng_value_t savedRng = gRngValue;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_RAIN_DANCE, 0); gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)}; CHECK(!mons->party_at(0, &mon));
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); ClockTestPut64(run + 16, 0); ClockTestPut64(run + 24, 0);
    RhhModsSetRunContext(123, 456, 0);
    struct RhhWorldEnvironmentRequest request = {.struct_size = sizeof(request), .kind = RHH_WORLD_WEATHER,
        .map = RhhContentKeyForId(RHH_MAP, 1 << 8), .owner = RhhContentKeyOf("test:music_catalog"),
        .move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE), .actor_identity = mon.identity, .duration_steps = 20};
    struct RhhWorldEnvironmentResult changed = {.struct_size = sizeof(changed)}; CHECK(!env->apply(&request, &changed));
    struct RhhWildEncounterContext context = {.struct_size = sizeof(context), .ordinal = 999};
    CHECK(sWildTest->context(&context) && context.ordinal == 999);
    sWildProposals = sWildGenerated = 0; sWildExpectedSource = RHH_WILD_OVERWORLD_ACTOR;
    sWildExpectedX = 2; sWildExpectedY = 1; sWildReplace = TRUE;
    sWildDeny = sWildInvalid = sWildTryUndeny = sWildAllowMissing = FALSE; sWildCheckTable = TRUE; sWeightMode = sWeightCalls = 0;
    rng_value_t rng = gRngValue; struct Pokemon enemy = gParties[B_TRAINER_OPPONENT_A][0];
    CHECK(ModsWildCandidate(SPECIES_POOCHYENA, SPECIES_POOCHYENA, WILD_AREA_LAND, RHH_WILD_OVERWORLD_ACTOR,
        3, 5, TIME_DAY, 2, 1, &context) == SPECIES_ZIGZAGOON);
    CHECK(context.phase == RHH_WILD_PHASE_PROPOSAL && context.table_time == RHH_WILD_TIME_DAY
        && context.ordinal == 0 && sWildProposals == 1 && !sWildGenerated && !ClockTestGet32(run + 16)
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&enemy, &gParties[B_TRAINER_OPPONENT_A][0], sizeof(enemy)));
    struct WildPokemon table[NUM_LAND_MONS_ENCOUNTER_SLOTS];
    for (unsigned i = 0; i < ARRAY_COUNT(table); ++i) table[i] = (struct WildPokemon){5, 5, SPECIES_POOCHYENA};
    struct WildPokemonInfo wild = {.encounterRate = 20, .wildPokemon = table};
    u16 savedRepel = VarGet(VAR_REPEL_STEP_COUNT); VarSet(VAR_REPEL_STEP_COUNT, 20);
    CHECK(!TryGenerateWildMonAt(&wild, WILD_AREA_LAND, WILD_CHECK_REPEL, 2, 1)
        && sWildProposals == 1 && !sWildGenerated && !ClockTestGet32(run + 16)
        && !memcmp(&enemy, &gParties[B_TRAINER_OPPONENT_A][0], sizeof(enemy)));
    VarSet(VAR_REPEL_STEP_COUNT, savedRepel);
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1)
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON
        && sWildGenerated == 1 && sLastWild.ordinal == 0 && ClockTestGet32(run + 16) == 1);
    sWildDeny = sWildTryUndeny = TRUE;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1)
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_POOCHYENA
        && sWildGenerated == 2 && sLastWild.ordinal == 1);
    sWildDeny = sWildTryUndeny = FALSE; sWildInvalid = TRUE;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1)
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_POOCHYENA
        && sWildGenerated == 3 && sLastWild.ordinal == 2);
    sWildInvalid = FALSE;
    sWildGateService = sWildTest; sWildGateDeny = sWildGateTryUndeny = sWildGateCheckRng = TRUE;
    sWildNestedCancelCheck = TRUE; sWildNestedCancelCount = 0;
    sWildGateExpectedRng = gRngValue; sWildGateExpectedRng2 = gRng2Value;
    struct Pokemon beforeCancelled[PARTY_SIZE]; memcpy(beforeCancelled, gParties[B_TRAINER_OPPONENT_A], sizeof(beforeCancelled));
    u8 beforeCount = gPartiesCount[B_TRAINER_OPPONENT_A]; u64 beforeOrdinal = ClockTestGet32(run + 16);
    bool8 cancelled = FALSE; unsigned proposedBefore = sWildProposals, generatedBefore = sWildGenerated;
    CHECK(!TryGenerateWildMonAtStatus(&wild, WILD_AREA_LAND, 0, 2, 1, &cancelled) && cancelled
        && sWildProposals == proposedBefore + 1 && sWildGateCalls == 1 && sWildCancelCalls == 1
        && sWildNestedCancelCount == 1
        && sWildGenerated == generatedBefore && ClockTestGet32(run + 16) == beforeOrdinal
        && gPartiesCount[B_TRAINER_OPPONENT_A] == beforeCount
        && !memcmp(beforeCancelled, gParties[B_TRAINER_OPPONENT_A], sizeof(beforeCancelled))
        && !memcmp(&sWildGateExpectedRng, &gRngValue, sizeof(gRngValue))
        && sLastWildCancellation.candidate.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_ZIGZAGOON));
    struct RhhWildEncounterCancellation noContext = {.struct_size = sizeof(noContext), .reason = "untouched"};
    CHECK(sWildGateService->cancellation_context(&noContext) && !strcmp(noContext.reason, "untouched"));
    /* Frontier facilities replace this provisional Pokémon after generation;
     * the cancellation hook must not claim to approve that final result. */
    u16 previousLayout = gMapHeader.mapLayoutId;
    const u16 specialLayouts[] = {LAYOUT_BATTLE_FRONTIER_BATTLE_PIKE_ROOM_WILD_MONS, LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR};
    for (u32 special = 0; special < ARRAY_COUNT(specialLayouts); ++special)
    {
        gMapHeader.mapLayoutId = specialLayouts[special]; u32 provisional = SPECIES_POOCHYENA;
        struct RhhWildEncounterCancellation ignored;
        CHECK(ModsWildResolve(SPECIES_POOCHYENA, &provisional, WILD_AREA_LAND, RHH_WILD_OVERWORLD_ACTOR,
            3, 5, TIME_DAY, 2, 1, &context, &ignored) && provisional == SPECIES_ZIGZAGOON
            && sWildGateCalls == 1 && sWildCancelCalls == 1);
    }
    gMapHeader.mapLayoutId = previousLayout;
    sWildGateDeny = sWildGateTryUndeny = sWildGateCheckRng = FALSE; sWildGateService = NULL;
    sWildExpectedSource = RHH_WILD_STANDARD; sWildExpectedX = 1; sWildExpectedY = 2;
    gPlayerAvatar.objectEventId = 0; memset(&gObjectEvents[0], 0, sizeof(gObjectEvents[0]));
    gObjectEvents[0].active = TRUE; gObjectEvents[0].currentCoords.x = 1 + MAP_OFFSET; gObjectEvents[0].currentCoords.y = 2 + MAP_OFFSET;
    CHECK(TryGenerateWildMon(&wild, WILD_AREA_LAND, 0)
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON
        && sWildGenerated == 4 && sLastWild.ordinal == 3 && ClockTestGet32(run + 16) == 4);
    struct RhhModValue mode = {.type = RHH_SETTING_CHOICE, .value = MOD_WILD_FULL};
    CHECK(!RhhModsConfigure("rhh:wild_randomizer", "mode", &mode));
    sWildExpectedSource = RHH_WILD_OVERWORLD_ACTOR; sWildExpectedX = 2; sWildExpectedY = 1;
    u32 randomSpecies = ModsWildSpecies(SPECIES_POOCHYENA, WILD_AREA_LAND, 0, 0);
    unsigned proposals = sWildProposals;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1) && sWildProposals == proposals + 1
        && sLastWild.initial_species == RhhContentKeyForId(RHH_SPECIES, randomSpecies)
        && sLastWild.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_ZIGZAGOON));
    mode.value = MOD_WILD_POOL; CHECK(!RhhModsConfigure("rhh:wild_randomizer", "mode", &mode));
    proposals = sWildProposals;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1) && sWildProposals == proposals + 1);
    u32 poolTime = sLastWild.table_time == RHH_WILD_TIME_UNKNOWN ? 0 : sLastWild.table_time - RHH_WILD_TIME_MORNING;
    randomSpecies = ModsWildSpecies(SPECIES_POOCHYENA, WILD_AREA_LAND, sLastWild.slot, poolTime);
    CHECK(sLastWild.initial_species == RhhContentKeyForId(RHH_SPECIES, randomSpecies)
        && sLastWild.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_ZIGZAGOON));
    mode.value = MOD_WILD_OFF; CHECK(!RhhModsConfigure("rhh:wild_randomizer", "mode", &mode));
    VarSet(VAR_REPEL_STEP_COUNT, 0); sWeightTarget = 3;
    struct RhhWildWeightContext weightContext = {.struct_size = sizeof(weightContext), .ordinal = 999};
    CHECK(sWildTest->weight_context(&weightContext) && weightContext.ordinal == 999);
    u8 *rewind = NULL; u32 rewindSize = 0; CHECK(!RhhModsExport(&rewind, &rewindSize));
    rng = gRngValue; sWeightMode = 0;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1));
    struct Pokemon baseline = gParties[B_TRAINER_OPPONENT_A][0]; rng_value_t baselineRng = gRngValue;
    unsigned baselineSlot = sLastWild.slot;
    const unsigned fallbackModes[] = {1, 3, 4, 8};
    for (unsigned i = 0; i < ARRAY_COUNT(fallbackModes); ++i)
    {
        CHECK(!RhhModsImport(rewind, rewindSize)); gRngValue = rng; sWeightMode = fallbackModes[i]; sWeightCalls = 0;
        CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1) && sWeightCalls == NUM_LAND_MONS_ENCOUNTER_SLOTS
            && sLastWild.slot == baselineSlot && !memcmp(&baseline, &gParties[B_TRAINER_OPPONENT_A][0], sizeof(baseline))
            && !memcmp(&baselineRng, &gRngValue, sizeof(baselineRng)));
    }
    free(rewind); sWeightMode = 2; sWeightCalls = 0;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1) && sLastWild.slot == 3
        && sWeightCalls == NUM_LAND_MONS_ENCOUNTER_SLOTS);
    sWeightMode = 5; sWeightCalls = 0;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1) && sLastWild.slot == 3
        && sWeightCalls == NUM_LAND_MONS_ENCOUNTER_SLOTS);
    sWeightMode = 6; u16 mixedWeights[NUM_LAND_MONS_ENCOUNTER_SLOTS] = {2, 5};
    rng = gRngValue; u32 expectedSlot = RandomWeightedArrayDefault(RNG_NONE, 7, ARRAY_COUNT(mixedWeights), mixedWeights); gRngValue = rng;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1) && sLastWild.slot == expectedSlot);
    sWeightMode = 7; CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1) && sLastWild.slot == 3);
    sWeightMode = 2; sWeightCalls = 0;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_WATER, 0, 2, 1) && sLastWild.slot == 3
        && sWeightCalls == NUM_WATER_MONS_ENCOUNTER_SLOTS);
    sWeightCalls = 0;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_ROCKS, 0, 2, 1) && sLastWild.slot == 3
        && sWeightCalls == NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS);
    sWeightMode = 2; VarSet(VAR_REPEL_STEP_COUNT, REPEL_LURE_MASK | 20);
    rng = gRngValue; Random(); bool32 lureFlip = Random() % 10 < 2; gRngValue = rng;
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1) && sLastWild.slot == (lureFlip ? 8 : 3));
    VarSet(VAR_REPEL_STEP_COUNT, 0);
    struct Pokemon lead = gParties[B_TRAINER_PLAYER][0];
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_PIKACHU, 20, 22222, OTID_STRUCT_PRESET(54321), 31);
    u8 abilitySlot = 0; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ABILITY_NUM, &abilitySlot);
    CHECK(GetMonAbility(&gParties[B_TRAINER_PLAYER][0]) == ABILITY_STATIC);
    for (unsigned i = 1; i < ARRAY_COUNT(table); ++i) table[i].species = SPECIES_PIKACHU;
    sWildCheckTable = FALSE; sWeightTarget = 0; bool32 sawAbilityOverride = FALSE;
    for (unsigned i = 0; i < 64 && !sawAbilityOverride; ++i)
    {
        unsigned beforeWeights = sWeightCalls;
        CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1));
        if (sLastWild.slot != 0) { sawAbilityOverride = TRUE; CHECK(sWeightCalls == beforeWeights); }
    }
    CHECK(sawAbilityOverride); gParties[B_TRAINER_PLAYER][0] = lead;
    VarSet(VAR_REPEL_STEP_COUNT, savedRepel); sWeightMode = 0; sWildCheckTable = TRUE;
    sWildExpectedSource = RHH_WILD_STANDARD; sWildExpectedX = sWildExpectedY = -1; sWildAllowMissing = TRUE;
    CHECK(ModsWildCandidate(SPECIES_POOCHYENA, SPECIES_POOCHYENA, WILD_AREA_LAND, RHH_WILD_STANDARD,
        0, 5, UINT32_MAX, -1, -1, &context) == SPECIES_ZIGZAGOON
        && !context.habitat_available && context.x == -1 && context.y == -1 && context.table_time == RHH_WILD_TIME_UNKNOWN);
    CHECK(sWildTest->context(&context));
    gParties[B_TRAINER_PLAYER][0] = savedMon; gPartiesCount[B_TRAINER_PLAYER] = savedCount;
    memcpy(gParties[B_TRAINER_OPPONENT_A], savedEnemy, sizeof(savedEnemy)); gPartiesCount[B_TRAINER_OPPONENT_A] = savedEnemyCount;
    gObjectEvents[0] = savedObject; gPlayerAvatar.objectEventId = savedObjectId; gRngValue = savedRng;
    sWildTest = NULL; RhhModsResetState(); RhhModsSetRunContext(0, 0, 0);
    puts("wild encounter: habitat-aware proposals, weights, RNG fallback, lure, ability and generation passed");
}
static void CheckExternalWildEncounter(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsLoadLibrary(path)); CHECK(!RhhModsFinalize());
    gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 0);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_RAIN_DANCE, 0); gPartiesCount[B_TRAINER_PLAYER] = 1;
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWorldEnvironmentService *env = (const void *)host->get_service("rhh:service/world_environment", RHH_SERVICE_VERSION(1, 0));
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)}; CHECK(mons && env && !mons->party_at(0, &mon));
    struct RhhWorldEnvironmentRequest request = {.struct_size = sizeof(request), .kind = RHH_WORLD_WEATHER,
        .map = RhhContentKeyForId(RHH_MAP, 1 << 8), .owner = RhhContentKeyOf("test:wild_probe"),
        .move = RhhContentKeyForId(RHH_MOVE, MOVE_RAIN_DANCE), .actor_identity = mon.identity, .duration_steps = 20};
    struct RhhWorldEnvironmentResult changed = {.struct_size = sizeof(changed)}; CHECK(!env->apply(&request, &changed));
    struct WildPokemon table[NUM_LAND_MONS_ENCOUNTER_SLOTS];
    for (unsigned i = 0; i < ARRAY_COUNT(table); ++i) table[i] = (struct WildPokemon){5, 5, SPECIES_POOCHYENA};
    struct WildPokemonInfo wild = {.encounterRate = 20, .wildPokemon = table};
    CHECK(TryGenerateWildMonAt(&wild, WILD_AREA_LAND, 0, 2, 1)
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON);
    free(gBackupMapLayout.map);
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE103)][MAP_NUM(MAP_ROUTE103)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE103); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE103);
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    request.map = RhhContentKeyForId(RHH_MAP, (MAP_GROUP(MAP_ROUTE103) << 8) | MAP_NUM(MAP_ROUTE103));
    CHECK(!env->apply(&request, &changed));
    ResetTasks(); gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
    memset(&gObjectEvents[0], 0, sizeof(gObjectEvents[0])); gObjectEvents[0].active = TRUE;
    gObjectEvents[0].currentCoords.x = MAP_OFFSET + 1; gObjectEvents[0].currentCoords.y = MAP_OFFSET + 1;
    gObjectEvents[0].facingDirection = DIR_EAST; gObjectEvents[0].spriteId = 0; gSprites[0].inUse = TRUE;
    for (unsigned rod = OLD_ROD; rod <= SUPER_ROD; ++rod)
    {
        ResetTasks(); gIsFishingEncounter = FALSE; FishingWildEncounter(rod);
        CHECK(gIsFishingEncounter && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON);
    }
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; RhhModsUnloadLibraries();
}
static void SetupCancellationFixtureWithLibraries(const char *module, const char *extra)
{
    InitModsForTestWithLibraries(module, extra); gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE103)][MAP_NUM(MAP_ROUTE103)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE103); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE103);
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 0);
    ResetTasks(); ResetSpriteData(); memset(gObjectEvents, 0, sizeof(gObjectEvents));
    gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
    gObjectEvents[0].active = TRUE; gObjectEvents[0].isPlayer = TRUE; gObjectEvents[0].spriteId = 0;
    gObjectEvents[0].mapGroup = MAP_GROUP(MAP_ROUTE103); gObjectEvents[0].mapNum = MAP_NUM(MAP_ROUTE103);
    gObjectEvents[0].currentCoords.x = gObjectEvents[0].previousCoords.x = MAP_OFFSET + 1;
    gObjectEvents[0].currentCoords.y = gObjectEvents[0].previousCoords.y = MAP_OFFSET + 1;
    gObjectEvents[0].facingDirection = DIR_EAST; gSprites[0].inUse = TRUE;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_POOCHYENA, 7, 54321, OTID_STRUCT_PRESET(12345), 19);
    gPartiesCount[B_TRAINER_OPPONENT_A] = 1;
    gMain.callback2 = CB2_Overworld; ScriptContext_Init(); UnlockPlayerFieldControls();
    sWildGateService = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 2));
    CHECK(sWildGateService); sWildGateDeny = sWildGateTryUndeny = TRUE; sWildGateCheckRng = FALSE;
    sWildGateCalls = sWildCancelCalls = sWildGateDenyAfter = 0;
}
static void SetupCancellationFixture(const char *module) { SetupCancellationFixtureWithLibraries(module, NULL); }
static void CheckPaidMonEdit(u32 mode, const char *module)
{
    bool32 pyramid = mode == 1, secretBase = mode == 2;
    SetupCancellationFixture(module);
    if (pyramid)
    {
        u32 map = MAP_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR;
        gMapHeader = *gMapGroups[MAP_GROUP(map)][MAP_NUM(map)];
        gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(map); gSaveBlock1Ptr->location.mapNum = MAP_NUM(map);
        gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    }
    else if (secretBase)
    {
        u32 map = MAP_SECRET_BASE_RED_CAVE1;
        gMapHeader = *gMapGroups[MAP_GROUP(map)][MAP_NUM(map)];
        gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(map); gSaveBlock1Ptr->location.mapNum = MAP_NUM(map);
        VarSet(VAR_SECRET_BASE_LOW_TV_FLAGS, 0); VarSet(VAR_SECRET_BASE_LAST_ITEM_USED, 0);
    }
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    sEditTestService = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 1));
    CHECK(mons && sEditTestService && sEditTestService->header.struct_size >= RHH_MON_EDIT_SERVICE_V1_1_SIZE
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 0)) == &sEditTestService->header
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 2)) == &sEditTestService->header
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 3)) == &sEditTestService->header
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 4)) == &sEditTestService->header
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 5)) == &sEditTestService->header
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 6)) == &sEditTestService->header
        && !host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 9)));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 25, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1; SetMoney(&gSaveBlock1Ptr->money, 5000);
    if (pyramid)
    {
        struct PyramidBag *bag = &gSaveBlock2Ptr->frontier.pyramidBag; u32 level = FRONTIER_LVL_50;
        bag->itemId[level][0] = ITEM_POTION; bag->quantity[level][0] = 2;
        bag->itemId[level][1] = ITEM_ANTIDOTE; bag->quantity[level][1] = 1;
        bag->itemId[level][2] = ITEM_POTION; bag->quantity[level][2] = 3;
    }
    else
    {
        struct BagPocket *bag = &gBagPockets[POCKET_ITEMS];
        BagPocket_SetSlotItemIdAndCount(bag, 0, ITEM_POTION, 2);
        BagPocket_SetSlotItemIdAndCount(bag, 1, ITEM_ANTIDOTE, 1);
        BagPocket_SetSlotItemIdAndCount(bag, 2, ITEM_POTION, 3);
    }
    struct RhhMonEditPaidRequest request = {.struct_size = sizeof(request),
        .edit = {.struct_size = sizeof(request.edit), .fields = RHH_EDIT_IVS},
        .cost = {.struct_size = sizeof(request.cost), .item_quantity = 3, .money = 1500,
                 .item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION)}};
    request.edit.expected.struct_size = sizeof(request.edit.expected);
    CHECK(!mons->party_at(0, &request.edit.expected));
    memcpy(request.edit.ivs, request.edit.expected.ivs, sizeof(request.edit.ivs)); request.edit.ivs[RHH_STAT_HP] = 20;
    sEditTestRequest = &request.edit; sEditConstraints = sEditNotifications = 0; sEditDeny = FALSE;
    struct RhhMonEditPaidResult result = {.struct_size = sizeof(result)};
    CHECK(sEditTestService->commit_paid_previewed(&request, NULL, &result) == RHH_EDIT_INVALID
        && result.status == RHH_EDIT_INVALID && result.edit.status == RHH_EDIT_INVALID
        && result.edit.struct_size == sizeof(result.edit) && result.payment.struct_size == sizeof(result.payment));
    struct RhhMonEditPayment unavailable = {.struct_size = sizeof(unavailable), .money = 999};
    CHECK(sEditTestService->payment_context(&unavailable) && unavailable.money == 999);
    struct Pokemon original = gParties[B_TRAINER_PLAYER][0]; struct Bag bagBefore = gSaveBlock1Ptr->bag;
    struct PyramidBag pyramidBefore = gSaveBlock2Ptr->frontier.pyramidBag;
    rng_value_t rng = gRngValue;
    sEditNestedContext = TRUE;
    CHECK(sEditTestService->preview_paid(&request, &result) == RHH_EDIT_OK && result.edit.is_preview
        && result.payment.bag == (pyramid ? RHH_EDIT_BAG_PYRAMID : RHH_EDIT_BAG_NORMAL)
        && result.payment.item_before == 5 && result.payment.item_after == 2
        && result.payment.money_before == 5000 && result.payment.money_after == 3500
        && sEditConstraints == 1 && !sEditNotifications && GetMoney(&gSaveBlock1Ptr->money) == 5000
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original))
        && !memcmp(&bagBefore, &gSaveBlock1Ptr->bag, sizeof(bagBefore))
        && !memcmp(&pyramidBefore, &gSaveBlock2Ptr->frontier.pyramidBag, sizeof(pyramidBefore))
        && !memcmp(&rng, &gRngValue, sizeof(rng))
        && (!secretBase || (!VarGet(VAR_SECRET_BASE_LOW_TV_FLAGS) && !VarGet(VAR_SECRET_BASE_LAST_ITEM_USED))));
    struct RhhMonEditPaidResult paidQuote = result;
    SetMoney(&gSaveBlock1Ptr->money, 5001);
    CHECK(sEditTestService->commit_paid_previewed(&request, &paidQuote, &result) == RHH_EDIT_STALE
        && GetMoney(&gSaveBlock1Ptr->money) == 5001 && sEditConstraints == 1 && !sEditNotifications
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    SetMoney(&gSaveBlock1Ptr->money, 5000);
    CHECK(AddBagItem(ITEM_POTION, 1));
    CHECK(sEditTestService->commit_paid_previewed(&request, &paidQuote, &result) == RHH_EDIT_STALE
        && CheckBagHasItem(ITEM_POTION, 6) && !sEditNotifications
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    gSaveBlock1Ptr->bag = bagBefore; gSaveBlock2Ptr->frontier.pyramidBag = pyramidBefore;
    request.cost.item_quantity = 6;
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_DENIED
        && !strcmp(result.edit.reason, "not enough items") && sEditConstraints == 1);
    request.cost.item_quantity = 3; request.cost.money = 6000;
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_DENIED
        && !strcmp(result.edit.reason, "not enough money") && sEditConstraints == 1);
    request.cost.money = 1500; request.cost.item = RhhContentKeyForId(RHH_SPECIES, SPECIES_TORCHIC);
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_INVALID && sEditConstraints == 1);
    request.cost.item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION); sEditDeny = TRUE;
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_DENIED
        && !strcmp(result.edit.reason, "test rule denied edit") && !sEditNotifications);
    sEditDeny = FALSE; sEditMutateMoney = TRUE;
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_STALE && !sEditMutateMoney
        && GetMoney(&gSaveBlock1Ptr->money) == 4999 && !sEditNotifications
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original))
        && !memcmp(&bagBefore, &gSaveBlock1Ptr->bag, sizeof(bagBefore))
        && !memcmp(&pyramidBefore, &gSaveBlock2Ptr->frontier.pyramidBag, sizeof(pyramidBefore)));
    SetMoney(&gSaveBlock1Ptr->money, 5000);
    sEditMutateBag = TRUE;
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_STALE && !sEditMutateBag
        && !sEditNotifications && CheckBagHasItem(ITEM_POTION, 6)
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    gSaveBlock1Ptr->bag = bagBefore; gSaveBlock2Ptr->frontier.pyramidBag = pyramidBefore;
    if (secretBase) CHECK(!VarGet(VAR_SECRET_BASE_LOW_TV_FLAGS) && !VarGet(VAR_SECRET_BASE_LAST_ITEM_USED));
    CHECK(sEditTestService->commit_paid_previewed(&request, &paidQuote, &result) == RHH_EDIT_OK && !result.edit.is_preview
        && result.payment.item_before == 5 && result.payment.item_after == 2
        && GetMoney(&gSaveBlock1Ptr->money) == 3500 && !CheckBagHasItem(ITEM_POTION, 3)
        && CheckBagHasItem(ITEM_POTION, 2) && CheckBagHasItem(ITEM_ANTIDOTE, 1)
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV) == 20 && sEditNotifications == 1);
    if (secretBase) CHECK((VarGet(VAR_SECRET_BASE_LOW_TV_FLAGS) & SECRET_BASE_USED_BAG)
        && VarGet(VAR_SECRET_BASE_LAST_ITEM_USED) == ITEM_POTION);
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_STALE
        && GetMoney(&gSaveBlock1Ptr->money) == 3500 && CheckBagHasItem(ITEM_POTION, 2) && sEditNotifications == 1);
    /* Spending the last ball must still activate a newly enabled Nuzlocke. */
    struct RhhModValue enabled = {.type = RHH_SETTING_BOOL, .value = 1};
    CHECK(!RhhModsConfigure("rhh:nuzlocke", "enabled", &enabled) && AddBagItem(ITEM_POKE_BALL, 1));
    u8 *nuz = RhhModsState("rhh:nuzlocke"); CHECK(nuz && !nuz[1]);
    request.edit.expected.struct_size = sizeof(request.edit.expected); CHECK(!mons->party_at(0, &request.edit.expected));
    memcpy(request.edit.ivs, request.edit.expected.ivs, sizeof(request.edit.ivs)); request.edit.ivs[RHH_STAT_ATTACK] = 21;
    request.cost.item = RhhContentKeyForId(RHH_ITEM, ITEM_POKE_BALL); request.cost.item_quantity = 1; request.cost.money = 0;
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_OK && nuz[1]
        && !HasAtLeastOnePokeBall() && sEditNotifications == 2 && GetMoney(&gSaveBlock1Ptr->money) == 3500);
    /* The same paid transaction works on a boxed Pokémon. */
    gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][0].box; gPartiesCount[B_TRAINER_PLAYER] = 0;
    request.edit.expected.struct_size = sizeof(request.edit.expected); CHECK(!mons->box_at(0, 0, &request.edit.expected));
    memcpy(request.edit.ivs, request.edit.expected.ivs, sizeof(request.edit.ivs)); request.edit.ivs[RHH_STAT_DEFENSE] = 22;
    request.cost.item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION); request.cost.item_quantity = 2; request.cost.money = 1000;
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_OK && sEditNotifications == 3
        && GetMoney(&gSaveBlock1Ptr->money) == 2500 && !CheckBagHasItem(ITEM_POTION, 1)
        && result.edit.after.location == RHH_MON_BOX && result.edit.after.ivs[RHH_STAT_DEFENSE] == 22);
    request.edit.expected.struct_size = sizeof(request.edit.expected); CHECK(!mons->box_at(0, 0, &request.edit.expected));
    request.edit.fields = RHH_EDIT_EVS; memcpy(request.edit.evs, request.edit.expected.evs, sizeof(request.edit.evs));
    request.edit.evs[RHH_STAT_HP] = 10; request.cost.item = 0; request.cost.item_quantity = 0; request.cost.money = 500;
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_OK && sEditNotifications == 4
        && result.payment.bag == RHH_EDIT_BAG_NONE && GetMoney(&gSaveBlock1Ptr->money) == 2000
        && result.edit.after.evs[RHH_STAT_HP] == 10);
    /* A pricing constraint can also reject old, unpaid API calls. */
    request.edit.expected.struct_size = sizeof(request.edit.expected); CHECK(!mons->box_at(0, 0, &request.edit.expected));
    request.edit.fields = RHH_EDIT_IVS; memcpy(request.edit.ivs, request.edit.expected.ivs, sizeof(request.edit.ivs));
    request.edit.ivs[RHH_STAT_SPEED] = 23; sEditRequireCost = TRUE;
    struct RhhMonEditResult oldResult = {.struct_size = sizeof(oldResult)};
    CHECK(sEditTestService->commit(&request.edit, &oldResult) == RHH_EDIT_DENIED
        && !strcmp(oldResult.reason, "minimum payment required") && sEditNotifications == 4);
    request.cost.money = 100;
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_DENIED
        && !strcmp(result.edit.reason, "minimum payment required") && GetMoney(&gSaveBlock1Ptr->money) == 2000);
    request.cost.item = RhhContentKeyForId(RHH_ITEM, ITEM_ANTIDOTE); request.cost.item_quantity = 1; request.cost.money = 500;
    CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_OK && sEditNotifications == 5
        && GetMoney(&gSaveBlock1Ptr->money) == 1500 && !CheckBagHasItem(ITEM_ANTIDOTE, 1));
    sEditRequireCost = FALSE;
    CHECK(sEditTestService->payment_context(&unavailable) && unavailable.money == 999);
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; if (module) RhhModsUnloadLibraries();
    puts(pyramid ? "paid edit: Pyramid bag atomic costs passed"
        : secretBase ? "paid edit: secret-base bag atomic costs passed" : "paid edit: normal bag atomic costs passed");
}
static void CheckPaidEditSave(const char *path, bool32 load, const char *module)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    if (!load)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 25, 12345, OTID_STRUCT_PRESET(54321), 31);
        gPartiesCount[B_TRAINER_PLAYER] = 1; CHECK(AddBagItem(ITEM_POTION, 5)); SetMoney(&gSaveBlock1Ptr->money, 5000);
        const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
        sEditTestService = (const void *)RhhModsHostAPI()->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 1));
        CHECK(mons && sEditTestService);
        struct RhhMonEditPaidRequest request = {.struct_size = sizeof(request),
            .edit = {.struct_size = sizeof(request.edit), .fields = RHH_EDIT_IVS},
            .cost = {.struct_size = sizeof(request.cost), .item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION),
                     .item_quantity = 3, .money = 1500}};
        request.edit.expected.struct_size = sizeof(request.edit.expected); CHECK(!mons->party_at(0, &request.edit.expected));
        memcpy(request.edit.ivs, request.edit.expected.ivs, sizeof(request.edit.ivs)); request.edit.ivs[RHH_STAT_HP] = 20;
        sEditTestRequest = &request.edit; struct RhhMonEditPaidResult result = {.struct_size = sizeof(result)};
        CHECK(sEditTestService->commit_paid(&request, &result) == RHH_EDIT_OK && TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        puts("paid edit: saved Pokémon and payment");
    }
    else
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
        CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_TORCHIC
            && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV) == 20
            && GetMoney(&gSaveBlock1Ptr->money) == 3500 && CheckBagHasItem(ITEM_POTION, 2)
            && !CheckBagHasItem(ITEM_POTION, 3));
        puts("paid edit: fresh-process Pokémon and payment restored");
    }
    NativeSaveClose(); if (module) RhhModsUnloadLibraries();
}
static void CheckRecipeMonEdit(u32 mode, const char *module)
{
    bool32 pyramid = mode == 1, secretBase = mode == 2;
    SetupCancellationFixture(module);
    if (pyramid)
    {
        u32 map = MAP_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR;
        gMapHeader = *gMapGroups[MAP_GROUP(map)][MAP_NUM(map)];
        gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(map); gSaveBlock1Ptr->location.mapNum = MAP_NUM(map);
        gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    }
    else if (secretBase)
    {
        u32 map = MAP_SECRET_BASE_RED_CAVE1;
        gMapHeader = *gMapGroups[MAP_GROUP(map)][MAP_NUM(map)];
        gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(map); gSaveBlock1Ptr->location.mapNum = MAP_NUM(map);
        VarSet(VAR_SECRET_BASE_LOW_TV_FLAGS, 0); VarSet(VAR_SECRET_BASE_LAST_ITEM_USED, 0);
    }
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOwnedMonService *mons = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    sEditTestService = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 2));
    CHECK(mons && sEditTestService && sEditTestService->header.struct_size >= RHH_MON_EDIT_SERVICE_V1_2_SIZE
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 1)) == &sEditTestService->header
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 3)) == &sEditTestService->header
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 4)) == &sEditTestService->header
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 5)) == &sEditTestService->header
        && host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 6)) == &sEditTestService->header
        && !host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 9)));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 25, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1; SetMoney(&gSaveBlock1Ptr->money, 5000);
    CHECK(AddBagItem(ITEM_POTION, 5) && AddBagItem(ITEM_ANTIDOTE, 4)
        && AddBagItem(ITEM_POKE_BALL, 2) && AddBagItem(ITEM_ORAN_BERRY, 3));
    struct RhhModValue enabled = {.type = RHH_SETTING_BOOL, .value = 1};
    CHECK(!RhhModsConfigure("rhh:nuzlocke", "enabled", &enabled));
    u8 *nuz = RhhModsState("rhh:nuzlocke"); CHECK(nuz && !nuz[1]);
    struct RhhMonEditRecipeRequest request = {.struct_size = sizeof(request),
        .edit = {.struct_size = sizeof(request.edit), .fields = RHH_EDIT_IVS},
        .recipe = {.struct_size = sizeof(request.recipe), .item_count = 4, .money = 1500}};
    request.recipe.items[0] = (struct RhhMonEditRecipeItem){.item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION), .quantity = 3};
    request.recipe.items[1] = (struct RhhMonEditRecipeItem){.item = RhhContentKeyForId(RHH_ITEM, ITEM_ANTIDOTE), .quantity = 2};
    request.recipe.items[2] = (struct RhhMonEditRecipeItem){.item = RhhContentKeyForId(RHH_ITEM, ITEM_POKE_BALL), .quantity = 1};
    request.recipe.items[3] = (struct RhhMonEditRecipeItem){.item = RhhContentKeyForId(RHH_ITEM, ITEM_ORAN_BERRY), .quantity = 2};
    request.edit.expected.struct_size = sizeof(request.edit.expected); CHECK(!mons->party_at(0, &request.edit.expected));
    memcpy(request.edit.ivs, request.edit.expected.ivs, sizeof(request.edit.ivs)); request.edit.ivs[RHH_STAT_HP] = 20;
    sEditTestRequest = &request.edit; sEditConstraints = sEditNotifications = 0; sEditDeny = FALSE;
    struct Pokemon original = gParties[B_TRAINER_PLAYER][0]; struct Bag bagBefore = gSaveBlock1Ptr->bag;
    struct PyramidBag pyramidBefore = gSaveBlock2Ptr->frontier.pyramidBag; rng_value_t rng = gRngValue;
    struct RhhMonEditRecipeResult result = {.struct_size = sizeof(result)};
    CHECK(sEditTestService->commit_recipe_previewed(&request, NULL, &result) == RHH_EDIT_INVALID
        && result.status == RHH_EDIT_INVALID && result.edit.status == RHH_EDIT_INVALID
        && result.edit.struct_size == sizeof(result.edit) && result.payment.struct_size == sizeof(result.payment));
    struct RhhMonEditRecipePayment unavailable = {.struct_size = sizeof(unavailable), .money = 999};
    CHECK(sEditTestService->recipe_context(&unavailable) && unavailable.money == 999);
    sEditNestedContext = TRUE;
    CHECK(sEditTestService->preview_recipe(&request, &result) == RHH_EDIT_OK && result.edit.is_preview
        && result.payment.item_count == 4 && result.payment.bag == (pyramid ? RHH_EDIT_BAG_PYRAMID : RHH_EDIT_BAG_NORMAL)
        && result.payment.money_before == 5000 && result.payment.money_after == 3500
        && result.payment.items[0].before == 5 && result.payment.items[0].after == 2
        && result.payment.items[1].before == 4 && result.payment.items[1].after == 2
        && result.payment.items[2].before == 2 && result.payment.items[2].after == 1
        && result.payment.items[3].before == 3 && result.payment.items[3].after == 1
        && !sEditNotifications && !nuz[1] && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original))
        && !memcmp(&bagBefore, &gSaveBlock1Ptr->bag, sizeof(bagBefore))
        && !memcmp(&pyramidBefore, &gSaveBlock2Ptr->frontier.pyramidBag, sizeof(pyramidBefore))
        && (!secretBase || (!VarGet(VAR_SECRET_BASE_LOW_TV_FLAGS) && !VarGet(VAR_SECRET_BASE_LAST_ITEM_USED))));
    struct RhhMonEditRecipeResult recipeQuote = result;
    SetMoney(&gSaveBlock1Ptr->money, 5001);
    CHECK(sEditTestService->commit_recipe_previewed(&request, &recipeQuote, &result) == RHH_EDIT_STALE
        && GetMoney(&gSaveBlock1Ptr->money) == 5001 && !sEditNotifications
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    SetMoney(&gSaveBlock1Ptr->money, 5000);
    CHECK(AddBagItem(ITEM_ORAN_BERRY, 1));
    CHECK(sEditTestService->commit_recipe_previewed(&request, &recipeQuote, &result) == RHH_EDIT_STALE
        && CheckBagHasItem(ITEM_ORAN_BERRY, 4) && !sEditNotifications
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    gSaveBlock1Ptr->bag = bagBefore; gSaveBlock2Ptr->frontier.pyramidBag = pyramidBefore;
    struct RhhMonEditRecipeItem saved = request.recipe.items[1]; request.recipe.items[1] = request.recipe.items[0];
    CHECK(sEditTestService->commit_recipe(&request, &result) == RHH_EDIT_INVALID
        && !strcmp(result.edit.reason, "duplicate recipe item")); request.recipe.items[1] = saved;
    request.recipe.items[2].quantity = 3;
    CHECK(sEditTestService->commit_recipe(&request, &result) == RHH_EDIT_DENIED
        && !strcmp(result.edit.reason, "not enough items") && !nuz[1]); request.recipe.items[2].quantity = 1;
    request.recipe.item_count = RHH_MON_EDIT_MAX_COST_ITEMS + 1;
    CHECK(sEditTestService->commit_recipe(&request, &result) == RHH_EDIT_INVALID); request.recipe.item_count = 4;
    sEditRequireRecipe = TRUE; struct RhhMonEditResult unpaid = {.struct_size = sizeof(unpaid)};
    CHECK(sEditTestService->commit(&request.edit, &unpaid) == RHH_EDIT_DENIED
        && !strcmp(unpaid.reason, "recipe required") && !sEditNotifications);
    sEditRequireRecipe = FALSE;
    sEditRequireCost = TRUE;
    CHECK(sEditTestService->commit_recipe(&request, &result) == RHH_EDIT_DENIED
        && !strcmp(result.edit.reason, "minimum payment required") && nuz[1] && !sEditNotifications);
    sEditRequireCost = FALSE; sEditDeny = TRUE;
    CHECK(sEditTestService->commit_recipe(&request, &result) == RHH_EDIT_DENIED
        && !strcmp(result.edit.reason, "test rule denied edit") && !sEditNotifications);
    sEditDeny = FALSE; sEditMutateBall = TRUE;
    CHECK(sEditTestService->commit_recipe(&request, &result) == RHH_EDIT_STALE && !sEditMutateBall
        && CheckBagHasItem(ITEM_POKE_BALL, 3) && !sEditNotifications
        && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    gSaveBlock1Ptr->bag = bagBefore; gSaveBlock2Ptr->frontier.pyramidBag = pyramidBefore;
    if (secretBase) CHECK(!VarGet(VAR_SECRET_BASE_LOW_TV_FLAGS) && !VarGet(VAR_SECRET_BASE_LAST_ITEM_USED));
    CHECK(sEditTestService->commit_recipe_previewed(&request, &recipeQuote, &result) == RHH_EDIT_OK && !result.edit.is_preview
        && sEditNotifications == 1 && GetMoney(&gSaveBlock1Ptr->money) == 3500
        && CheckBagHasItem(ITEM_POTION, 2) && !CheckBagHasItem(ITEM_POTION, 3)
        && CheckBagHasItem(ITEM_ANTIDOTE, 2) && !CheckBagHasItem(ITEM_ANTIDOTE, 3)
        && CheckBagHasItem(ITEM_POKE_BALL, 1) && !CheckBagHasItem(ITEM_POKE_BALL, 2)
        && CheckBagHasItem(ITEM_ORAN_BERRY, 1) && !CheckBagHasItem(ITEM_ORAN_BERRY, 2));
    if (secretBase) CHECK((VarGet(VAR_SECRET_BASE_LOW_TV_FLAGS) & SECRET_BASE_USED_BAG)
        && VarGet(VAR_SECRET_BASE_LAST_ITEM_USED) == ITEM_ORAN_BERRY);
    CHECK(sEditTestService->commit_recipe(&request, &result) == RHH_EDIT_STALE
        && GetMoney(&gSaveBlock1Ptr->money) == 3500 && sEditNotifications == 1);
    gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][0].box; gPartiesCount[B_TRAINER_PLAYER] = 0;
    request.edit.expected.struct_size = sizeof(request.edit.expected); CHECK(!mons->box_at(0, 0, &request.edit.expected));
    memcpy(request.edit.ivs, request.edit.expected.ivs, sizeof(request.edit.ivs)); request.edit.ivs[RHH_STAT_ATTACK] = 21;
    request.recipe.items[0] = request.recipe.items[2]; request.recipe.item_count = 1; request.recipe.money = 500;
    sEditRequireCost = TRUE;
    CHECK(sEditTestService->commit_recipe(&request, &result) == RHH_EDIT_OK && sEditNotifications == 2
        && nuz[1] && !HasAtLeastOnePokeBall() && GetMoney(&gSaveBlock1Ptr->money) == 3000);
    sEditRequireCost = FALSE;
    request.edit.expected.struct_size = sizeof(request.edit.expected); CHECK(!mons->box_at(0, 0, &request.edit.expected));
    request.edit.fields = RHH_EDIT_EVS; memcpy(request.edit.evs, request.edit.expected.evs, sizeof(request.edit.evs));
    request.edit.evs[RHH_STAT_HP] = 10; request.recipe.item_count = 0; request.recipe.money = 100;
    CHECK(sEditTestService->commit_recipe(&request, &result) == RHH_EDIT_OK && sEditNotifications == 3
        && result.payment.bag == RHH_EDIT_BAG_NONE && GetMoney(&gSaveBlock1Ptr->money) == 2900);
    /* A v1.1 paid call exposes a complete one-line v1.2 recipe context. */
    struct RhhMonEditPaidRequest paid = {.struct_size = sizeof(paid), .edit.struct_size = sizeof(paid.edit),
        .cost = {.struct_size = sizeof(paid.cost), .item = RhhContentKeyForId(RHH_ITEM, ITEM_ANTIDOTE), .item_quantity = 1}};
    paid.edit.expected.struct_size = sizeof(paid.edit.expected); CHECK(!mons->box_at(0, 0, &paid.edit.expected));
    paid.edit.fields = RHH_EDIT_IVS; memcpy(paid.edit.ivs, paid.edit.expected.ivs, sizeof(paid.edit.ivs));
    paid.edit.ivs[RHH_STAT_DEFENSE] = 22; sEditTestRequest = &paid.edit;
    struct RhhMonEditPaidResult paidResult = {.struct_size = sizeof(paidResult)};
    CHECK(sEditTestService->commit_paid(&paid, &paidResult) == RHH_EDIT_OK && sEditNotifications == 4
        && CheckBagHasItem(ITEM_ANTIDOTE, 1) && !CheckBagHasItem(ITEM_ANTIDOTE, 2));
    const u32 maximumItems[RHH_MON_EDIT_MAX_COST_ITEMS] = {ITEM_POTION, ITEM_ANTIDOTE, ITEM_ORAN_BERRY,
        ITEM_SUPER_POTION, ITEM_REPEL, ITEM_REVIVE, ITEM_ESCAPE_ROPE, ITEM_PECHA_BERRY};
    for (u32 i = 3; i < RHH_MON_EDIT_MAX_COST_ITEMS; ++i) CHECK(AddBagItem(maximumItems[i], 1));
    request.edit.expected.struct_size = sizeof(request.edit.expected); CHECK(!mons->box_at(0, 0, &request.edit.expected));
    request.edit.fields = RHH_EDIT_IVS; memcpy(request.edit.ivs, request.edit.expected.ivs, sizeof(request.edit.ivs));
    request.edit.ivs[RHH_STAT_SPEED] = 23; sEditTestRequest = &request.edit;
    request.recipe.item_count = RHH_MON_EDIT_MAX_COST_ITEMS; request.recipe.money = 0;
    for (u32 i = 0; i < RHH_MON_EDIT_MAX_COST_ITEMS; ++i)
        request.recipe.items[i] = (struct RhhMonEditRecipeItem){.item = RhhContentKeyForId(RHH_ITEM, maximumItems[i]), .quantity = 1};
    CHECK(sEditTestService->commit_recipe(&request, &result) == RHH_EDIT_OK && sEditNotifications == 5
        && result.payment.item_count == RHH_MON_EDIT_MAX_COST_ITEMS && CheckBagHasItem(ITEM_POTION, 1)
        && !CheckBagHasItem(ITEM_POTION, 2) && !CheckBagHasItem(ITEM_ANTIDOTE, 1)
        && !CheckBagHasItem(ITEM_ORAN_BERRY, 1));
    for (u32 i = 3; i < RHH_MON_EDIT_MAX_COST_ITEMS; ++i) CHECK(!CheckBagHasItem(maximumItems[i], 1));
    if (secretBase) CHECK(VarGet(VAR_SECRET_BASE_LAST_ITEM_USED) == ITEM_PECHA_BERRY);
    CHECK(sEditTestService->recipe_context(&unavailable) && unavailable.money == 999);
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; if (module) RhhModsUnloadLibraries();
    puts(pyramid ? "recipe edit: Pyramid bag atomic recipe passed" : secretBase
        ? "recipe edit: secret-base atomic recipe passed" : "recipe edit: multiple pockets atomic recipe passed");
}
static void CheckRecipeEditSave(const char *path, bool32 load, const char *module)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    if (!load)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 25, 12345, OTID_STRUCT_PRESET(54321), 31);
        gPartiesCount[B_TRAINER_PLAYER] = 1; SetMoney(&gSaveBlock1Ptr->money, 5000);
        CHECK(AddBagItem(ITEM_POTION, 5) && AddBagItem(ITEM_ANTIDOTE, 4)
            && AddBagItem(ITEM_POKE_BALL, 2) && AddBagItem(ITEM_ORAN_BERRY, 3));
        const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
        sEditTestService = (const void *)RhhModsHostAPI()->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 2));
        CHECK(mons && sEditTestService);
        struct RhhMonEditRecipeRequest request = {.struct_size = sizeof(request),
            .edit = {.struct_size = sizeof(request.edit), .fields = RHH_EDIT_IVS},
            .recipe = {.struct_size = sizeof(request.recipe), .item_count = 4, .money = 1500}};
        request.recipe.items[0] = (struct RhhMonEditRecipeItem){.item = RhhContentKeyForId(RHH_ITEM, ITEM_POTION), .quantity = 3};
        request.recipe.items[1] = (struct RhhMonEditRecipeItem){.item = RhhContentKeyForId(RHH_ITEM, ITEM_ANTIDOTE), .quantity = 2};
        request.recipe.items[2] = (struct RhhMonEditRecipeItem){.item = RhhContentKeyForId(RHH_ITEM, ITEM_POKE_BALL), .quantity = 1};
        request.recipe.items[3] = (struct RhhMonEditRecipeItem){.item = RhhContentKeyForId(RHH_ITEM, ITEM_ORAN_BERRY), .quantity = 2};
        request.edit.expected.struct_size = sizeof(request.edit.expected); CHECK(!mons->party_at(0, &request.edit.expected));
        memcpy(request.edit.ivs, request.edit.expected.ivs, sizeof(request.edit.ivs)); request.edit.ivs[RHH_STAT_HP] = 20;
        sEditTestRequest = &request.edit; struct RhhMonEditRecipeResult result = {.struct_size = sizeof(result)};
        CHECK(sEditTestService->commit_recipe(&request, &result) == RHH_EDIT_OK && TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        puts("recipe edit: saved Pokémon and payment");
    }
    else
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
        CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_TORCHIC
            && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV) == 20
            && GetMoney(&gSaveBlock1Ptr->money) == 3500 && CheckBagHasItem(ITEM_POTION, 2) && !CheckBagHasItem(ITEM_POTION, 3)
            && CheckBagHasItem(ITEM_ANTIDOTE, 2) && !CheckBagHasItem(ITEM_ANTIDOTE, 3)
            && CheckBagHasItem(ITEM_POKE_BALL, 1) && !CheckBagHasItem(ITEM_POKE_BALL, 2)
            && CheckBagHasItem(ITEM_ORAN_BERRY, 1) && !CheckBagHasItem(ITEM_ORAN_BERRY, 2));
        puts("recipe edit: fresh-process Pokémon and payment restored");
    }
    NativeSaveClose(); if (module) RhhModsUnloadLibraries();
}
static void CheckCancellationIntegration(const char *module)
{
    SetupCancellationFixture(module); sTrackEncounterStarts = TRUE; sEncounterStartCount = 0;
    struct MapLayout layout = *gMapHeader.mapLayout; struct Tileset primary = *layout.primaryTileset;
    u16 attributes[1] = {MB_TALL_GRASS}; primary.metatileAttributes = attributes; layout.primaryTileset = &primary;
    gMapHeader.mapLayout = &layout;
    struct Pokemon before[PARTY_SIZE]; memcpy(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before));
    u8 oldCount = gPartiesCount[B_TRAINER_OPPONENT_A]; u32 oldFishingStat = GetGameStat(GAME_STAT_FISHING_ENCOUNTERS);
    u8 *run = RhhModsState("rhh:host_run"); u64 originalOrdinal = ClockTestGet32(run + 16);
    gIsFishingEncounter = FALSE; gChainFishingDexNavStreak = 5;
    /* Habitat and permission hooks share the candidate's clock sample even
     * if the underlying clock changes between their callbacks. */
    sHabitatTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/habitat", RHH_SERVICE_VERSION(1, 0));
    sTimeScopeService = (const void *)RhhModsHostAPI()->get_service("rhh:service/world_time", RHH_SERVICE_VERSION(1, 0));
    CHECK(sHabitatTest && sTimeScopeService && !NativeRtcSetFixed("2024-06-01T21:15:25"));
    sTimeScopeTest = TRUE; sTimeScopeChecks = 0; sTimeScopeSkip = TRUE; sWildGateTimeCheck = TRUE;
    struct WildPokemon timeTable[NUM_LAND_MONS_ENCOUNTER_SLOTS];
    for (u32 i = 0; i < ARRAY_COUNT(timeTable); ++i) timeTable[i] = (struct WildPokemon){5, 5, SPECIES_POOCHYENA};
    struct WildPokemonInfo timedWild = {.encounterRate = 20, .wildPokemon = timeTable};
    CHECK(!TryGenerateWildMonAt(&timedWild, WILD_AREA_LAND, 0, 2, 1)
        && sTimeScopeChecks == 2 && sWildGateCalls == 1 && sWildCancelCalls == 1);
    struct RhhWorldTimeSnapshot afterTime = {.struct_size = sizeof(afterTime)};
    CHECK(!sTimeScopeService->sample(&afterTime) && afterTime.hour == 4);
    sHabitatTest = NULL; sTimeScopeService = NULL;
    sTimeScopeTest = sTimeScopeSkip = sTimeScopeIgnore = sWildGateTimeCheck = FALSE;
    sWildGateCalls = sWildCancelCalls = 0;
    for (u32 rod = OLD_ROD; rod <= SUPER_ROD; ++rod)
    {
        sWildGateExpectedRng = gRngValue; sWildGateExpectedRng2 = gRng2Value; sWildGateCheckRng = TRUE;
        CHECK(!FishingWildEncounter(rod) && !gIsFishingEncounter && gChainFishingDexNavStreak == 5
            && GetGameStat(GAME_STAT_FISHING_ENCOUNTERS) == oldFishingStat && !ClockTestGet32(run + 16)
            && gPartiesCount[B_TRAINER_OPPONENT_A] == oldCount
            && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before))
            && !memcmp(&gRngValue, &sWildGateExpectedRng, sizeof(gRngValue))
            && sLastWildCancellation.candidate.source == RHH_WILD_FISHING_ROD);
    }
    CHECK(sWildCancelCalls == 3 && sWildGateCalls == 3 && originalOrdinal == 0);
    sWildGateCheckRng = FALSE;
    CHECK(!SweetScentWildEncounter() && sWildCancelCalls == 4
        && sLastWildCancellation.candidate.area == RHH_WILD_LAND
        && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)));
    attributes[0] = MB_DEEP_WATER;
    CHECK(!SweetScentWildEncounter() && sWildCancelCalls == 5
        && sLastWildCancellation.candidate.area == RHH_WILD_WATER
        && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)));
    attributes[0] = MB_TALL_GRASS;
    for (u32 i = 0; i < 10000 && sWildCancelCalls == 5; ++i)
        CHECK(!StandardWildEncounter(MB_TALL_GRASS, MB_TALL_GRASS));
    CHECK(sWildCancelCalls == 6 && !ClockTestGet32(run + 16)
        && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)));
    /* Enter the real fishing task at the post-hook step, then drive its text
     * printer until the cancelled attempt releases field controls. */
    ResetTasks(); InitGpuRegManager();
    const struct BgTemplate fishingBg = {.bg = 0}; InitBgsFromTemplates(0, &fishingBg, 1);
    SetDefaultFontsPointer(); InitStandardTextBoxWindows(); InitTextBoxGfxAndPrinters();
    StartFishing(OLD_ROD); u32 fishingTask = 0;
    while (fishingTask < NUM_TASKS && !gTasks[fishingTask].isActive) ++fishingTask;
    CHECK(fishingTask < NUM_TASKS && ArePlayerFieldControlsLocked() && gPlayerAvatar.preventStep);
    gTasks[fishingTask].data[0] = 12; /* FISHING_START_ENCOUNTER */
    gTasks[fishingTask].data[1] = 1; /* the hook message has already finished */
    RunTasks(); CHECK(gTasks[fishingTask].isActive && gTasks[fishingTask].data[0] == 17
        && sWildCancelCalls == 7 && gPlayerAvatar.preventStep && ArePlayerFieldControlsLocked()
        && IsTextPrinterActiveOnWindow(0));
    for (u32 i = 0; i < 3000 && gTasks[fishingTask].isActive; ++i)
    { gMain.newKeys = A_BUTTON; RunTasks(); }
    CHECK(!gTasks[fishingTask].isActive && !gPlayerAvatar.preventStep && !ArePlayerFieldControlsLocked()
        && !gIsFishingEncounter && !gChainFishingDexNavStreak
        && GetGameStat(GAME_STAT_FISHING_ENCOUNTERS) == oldFishingStat);
    gMain.newKeys = 0;
    /* Find an actual Route 119 Feebas tile, then replay its RNG state so the
     * direct Feebas branch is vetoed before creation as well. */
    struct MapHeader previousHeader = gMapHeader; struct BackupMapLayout previousGrid = gBackupMapLayout;
    struct WarpData previousLocation = gSaveBlock1Ptr->location;
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE119)][MAP_NUM(MAP_ROUTE119)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE119); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE119);
    const struct MapLayout *feebasLayout = gMapHeader.mapLayout;
    gBackupMapLayout.width = feebasLayout->width + MAP_OFFSET_W; gBackupMapLayout.height = feebasLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    for (s32 y = 0; y < feebasLayout->height; ++y) for (s32 x = 0; x < feebasLayout->width; ++x)
        gBackupMapLayout.map[(y + MAP_OFFSET) * gBackupMapLayout.width + x + MAP_OFFSET] = feebasLayout->map[y * feebasLayout->width + x];
    bool32 foundFeebas = FALSE;
    for (s32 y = 0; y < feebasLayout->height && !foundFeebas; ++y)
        for (s32 x = 1; x < feebasLayout->width && !foundFeebas; ++x)
        {
            if (!MetatileBehavior_IsSurfableAndNotWaterfall(MapGridGetMetatileBehaviorAt(x + MAP_OFFSET, y + MAP_OFFSET))) continue;
            for (u32 attempt = 0; attempt < 8; ++attempt)
            {
                rng_value_t replay = gRngValue;
                if (!CheckFeebasAtCoords(x + MAP_OFFSET, y + MAP_OFFSET)) continue;
                gRngValue = replay; gObjectEvents[0].currentCoords.x = gObjectEvents[0].previousCoords.x = x - 1 + MAP_OFFSET;
                gObjectEvents[0].currentCoords.y = gObjectEvents[0].previousCoords.y = y + MAP_OFFSET;
                sWildGateExpectedRng = gRngValue; sWildGateExpectedRng2 = gRng2Value; sWildGateCheckRng = TRUE;
                CHECK(!FishingWildEncounter(OLD_ROD) && sWildCancelCalls == 8
                    && sLastWildCancellation.candidate.source == RHH_WILD_FEEBAS
                    && !gIsFishingEncounter && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)));
                foundFeebas = TRUE; break;
            }
        }
    CHECK(foundFeebas && !sEncounterStartCount); sWildGateCheckRng = FALSE;
    free(gBackupMapLayout.map); gBackupMapLayout = previousGrid; gMapHeader = previousHeader;
    gSaveBlock1Ptr->location = previousLocation;
    gObjectEvents[0].currentCoords.x = gObjectEvents[0].previousCoords.x = MAP_OFFSET + 1;
    gObjectEvents[0].currentCoords.y = gObjectEvents[0].previousCoords.y = MAP_OFFSET + 1;
    previousHeader = gMapHeader; previousGrid = gBackupMapLayout; previousLocation = gSaveBlock1Ptr->location;
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE111)][MAP_NUM(MAP_ROUTE111)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE111); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE111);
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    for (u32 i = 0; i < 10000 && sWildCancelCalls == 8; ++i)
    { gSpecialVar_Result = 999; RockSmashWildEncounter(); CHECK(!gSpecialVar_Result); }
    CHECK(sWildCancelCalls == 9 && sLastWildCancellation.candidate.area == RHH_WILD_ROCKS
        && !sEncounterStartCount && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)));
    free(gBackupMapLayout.map); gBackupMapLayout = previousGrid; gMapHeader = previousHeader;
    gSaveBlock1Ptr->location = previousLocation;
    sWildGateDeny = FALSE;
    CHECK(FishingWildEncounter(OLD_ROD) && gIsFishingEncounter && sWildCancelCalls == 9
        && GetGameStat(GAME_STAT_FISHING_ENCOUNTERS) == oldFishingStat + 1 && ClockTestGet32(run + 16) == 1
        && sEncounterStartCount == 1);
    sTrackEncounterStarts = FALSE; sWildGateService = NULL; free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    if (module) RhhModsUnloadLibraries();
    puts("wild cancellation: fishing, Sweet Scent, natural encounters and recovery passed");
}
#if WE_DOUBLE_WILD_CHANCE == 100
static void CheckCancellationDouble(void)
{
    SetupCancellationFixture(NULL);
    sWildGateDenyAfter = 1; sTrackWildFirst = sTrackEncounterStarts = TRUE;
    sFirstGeneratedIdentity = 0; sEncounterStartCount = 0;
    for (u32 i = 0; i < 10000 && !sWildCancelCalls; ++i)
        (void)StandardWildEncounter(MB_TALL_GRASS, MB_TALL_GRASS);
    CHECK(sWildCancelCalls == 1 && sWildGateCalls == 2 && sFirstGeneratedIdentity && sEncounterStartCount == 1
        && !(gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
        && ((u64)GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_OT_ID) << 32
            | GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_PERSONALITY)) == sFirstGeneratedIdentity
        && !GetMonData(&gParties[B_TRAINER_OPPONENT_A][1], MON_DATA_SPECIES));
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    puts("wild cancellation: denied second encounter preserves first in a single battle");
}
#endif
static void CheckSpecialOutbreak(const char *module)
{
    SetupCancellationFixture(module); sTrackEncounterStarts = TRUE; sEncounterStartCount = 0;
    gSaveBlock1Ptr->outbreakPokemonSpecies = SPECIES_SEEDOT; gSaveBlock1Ptr->outbreakPokemonLevel = 12;
    gSaveBlock1Ptr->outbreakPokemonMoves[0] = MOVE_TACKLE; gSaveBlock1Ptr->outbreakPokemonMoves[1] = MOVE_HARDEN;
    gSaveBlock1Ptr->outbreakLocationMapGroup = MAP_GROUP(MAP_ROUTE103);
    gSaveBlock1Ptr->outbreakLocationMapNum = MAP_NUM(MAP_ROUTE103);
    gSaveBlock1Ptr->outbreakPokemonProbability = 100; gSaveBlock1Ptr->outbreakDaysLeft = 3;
    struct MapLayout layout = *gMapHeader.mapLayout; struct Tileset primary = *layout.primaryTileset;
    u16 attributes[1] = {MB_TALL_GRASS}; primary.metatileAttributes = attributes; layout.primaryTileset = &primary;
    gMapHeader.mapLayout = &layout;
    struct Pokemon before[PARTY_SIZE]; memcpy(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before));
    sWildGateExpectedRng = gRngValue; sWildGateExpectedRng2 = gRng2Value; sWildGateCheckRng = TRUE;
    CHECK(!SweetScentWildEncounter() && sSpecialGateCalls == 1 && sSpecialCancelCalls == 1
        && !sSpecialGeneratedCalls && !sEncounterStartCount
        && sLastWildCancellation.candidate.source == RHH_WILD_OUTBREAK
        && sLastWildCancellation.candidate.slot == UINT32_MAX && sLastWildCancellation.candidate.level == 12
        && sLastWildCancellation.candidate.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_SEEDOT)
        && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before))
        && gSaveBlock1Ptr->outbreakDaysLeft == 3 && gSaveBlock1Ptr->outbreakPokemonMoves[0] == MOVE_TACKLE
        && !memcmp(&gRngValue, &sWildGateExpectedRng, sizeof(gRngValue)));
    sWildGateCheckRng = FALSE;
    for (u32 i = 0; i < 10000 && sSpecialCancelCalls == 1; ++i)
        CHECK(!StandardWildEncounter(MB_TALL_GRASS, MB_TALL_GRASS));
    CHECK(sSpecialCancelCalls == 2 && sSpecialGateCalls == 2 && !sEncounterStartCount
        && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)));
    sWildGateDeny = FALSE;
    CHECK(SweetScentWildEncounter() && sSpecialGateCalls == 3 && sSpecialCancelCalls == 2
        && sSpecialGeneratedCalls == 1 && sEncounterStartCount == 1
        && sLastSpecialGenerated.source == RHH_WILD_OUTBREAK && sLastSpecialGenerated.level == 12
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_SEEDOT
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_MOVE1) == MOVE_TACKLE
        && !ClockTestGet32((u8 *)RhhModsState("rhh:host_run") + 16));
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; if (module) RhhModsUnloadLibraries();
    puts("special wild: outbreak veto, rollback and original moves passed");
}
static void CheckSpecialRoamer(const char *module)
{
    SetupCancellationFixture(module); sTrackEncounterStarts = TRUE; sEncounterStartCount = 0;
    struct RhhModValue enabled = {.type = RHH_SETTING_BOOL, .value = 1};
    CHECK(!RhhModsConfigure("rhh:nuzlocke", "enabled", &enabled) && AddBagItem(ITEM_POKE_BALL, 1));
    u8 *nuzlocke = RhhModsState("rhh:nuzlocke"); CHECK(nuzlocke && !nuzlocke[4] && !nuzlocke[5]);
    DeactivateAllRoamers(); CHECK(TryAddRoamer(SPECIES_LATIAS, 40));
    u8 group, number; GetRoamerLocation(0, &group, &number);
    free(gBackupMapLayout.map); gMapHeader = *gMapGroups[group][number];
    gSaveBlock1Ptr->location.mapGroup = group; gSaveBlock1Ptr->location.mapNum = number;
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    u32 header = GetCurrentMapWildMonHeaderId(); CHECK(header != HEADER_NONE);
    bool32 land = gWildMonHeaders[header].encounterTypes[TIME_OF_DAY_DEFAULT].landMonsInfo != NULL;
    CHECK(land || gWildMonHeaders[header].encounterTypes[TIME_OF_DAY_DEFAULT].waterMonsInfo);
    struct MapLayout layout = *gMapHeader.mapLayout; struct Tileset primary = *layout.primaryTileset;
    u16 attributes[1] = {land ? MB_TALL_GRASS : MB_DEEP_WATER}; primary.metatileAttributes = attributes;
    layout.primaryTileset = &primary; gMapHeader.mapLayout = &layout;
    gObjectEvents[0].mapGroup = group; gObjectEvents[0].mapNum = number;
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_POOCHYENA, 7, 54321, OTID_STRUCT_PRESET(12345), 19);
    struct Pokemon before[PARTY_SIZE]; memcpy(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before));
    struct Roamer original = gSaveBlock1Ptr->roamer[0]; gEncounteredRoamerIndex = ROAMER_COUNT + 1;
    bool32 ready = FALSE;
    for (u32 i = 0; i < 1000; ++i)
    { rng_value_t previous = gRngValue; if (Random() % 4 == 0) { gRngValue = previous; ready = TRUE; break; } }
    CHECK(ready); sWildGateExpectedRng = gRngValue; sWildGateExpectedRng2 = gRng2Value; sWildGateCheckRng = TRUE;
    CHECK(!SweetScentWildEncounter() && sSpecialGateCalls == 1 && sSpecialCancelCalls == 1
        && !sSpecialGeneratedCalls && !sEncounterStartCount && !nuzlocke[4] && !nuzlocke[5]
        && sLastWildCancellation.candidate.source == RHH_WILD_ROAMER && !sLastWildCancellation.candidate.slot
        && sLastWildCancellation.candidate.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_LATIAS)
        && (u32)sLastWildCancellation.candidate.identity == original.personality
        && gEncounteredRoamerIndex == ROAMER_COUNT + 1 && !memcmp(&original, &gSaveBlock1Ptr->roamer[0], sizeof(original))
        && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before))
        && !memcmp(&gRngValue, &sWildGateExpectedRng, sizeof(gRngValue)));
    sWildGateCheckRng = FALSE;
    u16 behavior = land ? MB_TALL_GRASS : MB_DEEP_WATER;
    for (u32 i = 0; i < 10000 && sSpecialCancelCalls == 1; ++i)
        CHECK(!StandardWildEncounter(behavior, behavior));
    CHECK(sSpecialCancelCalls == 2 && sSpecialGateCalls == 2 && !sEncounterStartCount
        && gEncounteredRoamerIndex == ROAMER_COUNT + 1
        && !memcmp(&original, &gSaveBlock1Ptr->roamer[0], sizeof(original))
        && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)));
    sWildGateDeny = FALSE;
    CHECK(SweetScentWildEncounter() && sSpecialGateCalls == 3 && sSpecialCancelCalls == 2
        && sSpecialGeneratedCalls == 1 && sEncounterStartCount == 1 && gEncounteredRoamerIndex == 0
        && nuzlocke[4] == 1 && !nuzlocke[5] && ModsCanCapture()
        && sLastSpecialGenerated.source == RHH_WILD_ROAMER
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_LATIAS
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_PERSONALITY) == original.personality
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_HP) == original.hp
        && !memcmp(&original, &gSaveBlock1Ptr->roamer[0], sizeof(original))
        && !ClockTestGet32((u8 *)RhhModsState("rhh:host_run") + 16));
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; if (module) RhhModsUnloadLibraries();
    puts("special wild: roamer veto, rollback and retained identity passed");
}
static void CheckFacilityWild(bool32 pike, bool32 open, const char *module)
{
    SetupCancellationFixture(module); sTrackEncounterStarts = TRUE; sEncounterStartCount = 0;
    struct RhhModValue enabled = {.type = RHH_SETTING_BOOL, .value = 1};
    CHECK(!RhhModsConfigure("rhh:nuzlocke", "enabled", &enabled));
    u8 *nuzlocke = RhhModsState("rhh:nuzlocke"); CHECK(nuzlocke && !nuzlocke[4] && !nuzlocke[5]);
    u32 map = pike ? MAP_BATTLE_FRONTIER_BATTLE_PIKE_ROOM_WILD_MONS : MAP_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR;
    free(gBackupMapLayout.map); gMapHeader = *gMapGroups[MAP_GROUP(map)][MAP_NUM(map)];
    gSaveBlock2Ptr->frontier.lvlMode = open ? FRONTIER_LVL_OPEN : FRONTIER_LVL_50;
    CHECK(AddBagItem(ITEM_POKE_BALL, 1));
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(map); gSaveBlock1Ptr->location.mapNum = MAP_NUM(map);
    gObjectEvents[0].mapGroup = MAP_GROUP(map); gObjectEvents[0].mapNum = MAP_NUM(map);
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    CHECK(GetCurrentMapWildMonHeaderId() == HEADER_NONE);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 0;
    gSaveBlock2Ptr->frontier.pikeWinStreaks[gSaveBlock2Ptr->frontier.lvlMode] = 0;
    gSaveBlock2Ptr->frontier.pyramidWinStreaks[gSaveBlock2Ptr->frontier.lvlMode] = 0;
    gFacilityTrainers = NULL; gFacilityTrainerMons = NULL;
    struct RhhModValue mode = {.type = RHH_SETTING_CHOICE, .value = pike ? MOD_WILD_FULL : MOD_WILD_POOL};
    CHECK(!RhhModsConfigure("rhh:wild_randomizer", "mode", &mode));
    sWildTest = sWildGateService; sWildProposals = sWildGenerated = 0;
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run); u64 ordinal = ClockTestGet32(run + 16);
    struct Pokemon before[PARTY_SIZE]; memcpy(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before));
    u8 count = gPartiesCount[B_TRAINER_OPPONENT_A];
    sWildGateExpectedRng = gRngValue; sWildGateExpectedRng2 = gRng2Value; sWildGateCheckRng = TRUE;
    CHECK(!SweetScentWildEncounter() && sWildGateCalls == 1 && sWildCancelCalls == 1
        && !sSpecialGateCalls && !sSpecialCancelCalls && !sSpecialGeneratedCalls && !sEncounterStartCount
        && !sWildProposals && !sWildGenerated && !nuzlocke[4] && !nuzlocke[5] && ClockTestGet32(run + 16) == ordinal
        && !gFacilityTrainers && !gFacilityTrainerMons
        && gPartiesCount[B_TRAINER_OPPONENT_A] == count && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before))
        && !memcmp(&gRngValue, &sWildGateExpectedRng, sizeof(gRngValue))
        && !memcmp(&gRng2Value, &sWildGateExpectedRng2, sizeof(gRng2Value)));
    struct RhhWildEncounterContext denied = sLastWildCancellation.candidate;
    CHECK(denied.source == (pike ? RHH_WILD_PIKE : RHH_WILD_PYRAMID) && denied.area == RHH_WILD_LAND
        && denied.slot == UINT32_MAX && denied.identity && denied.level >= 1 && denied.level <= MAX_LEVEL);
    sWildGateCheckRng = FALSE;
    for (u32 i = 0; i < 10000 && sWildCancelCalls == 1; ++i)
        CHECK(!StandardWildEncounter(MB_TALL_GRASS, MB_TALL_GRASS));
    CHECK(sWildCancelCalls == 2 && sWildGateCalls == 2 && !sEncounterStartCount
        && !sWildProposals && !sWildGenerated && ClockTestGet32(run + 16) == ordinal
        && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)));
    /* Rewind to the exact Sweet Scent checkpoint: the allowed encounter must
     * be the same final Pokémon described by the earlier veto. */
    gRngValue = sWildGateExpectedRng; gRng2Value = sWildGateExpectedRng2; sWildGateDeny = FALSE;
    CHECK(SweetScentWildEncounter() && sWildGateCalls == 3 && sWildCancelCalls == 2
        && sSpecialGeneratedCalls == 1 && sEncounterStartCount == 1 && !sWildProposals && !sWildGenerated
        && nuzlocke[4] == 1 && !nuzlocke[5] && ModsCanCapture()
        && ClockTestGet32(run + 16) == ordinal && sLastSpecialGenerated.source == denied.source
        && sLastSpecialGenerated.species == denied.species && sLastSpecialGenerated.level == denied.level
        && sLastSpecialGenerated.identity == denied.identity
        && RhhContentKeyForId(RHH_SPECIES, GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES)) == denied.species
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_LEVEL) == denied.level
        && ((u64)GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_OT_ID) << 32
            | GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_PERSONALITY)) == denied.identity);
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; if (module) RhhModsUnloadLibraries();
    puts(pike ? "facility wild: pike veto, rollback and battle passed" : "facility wild: pyramid veto, rollback and battle passed");
}
#if defined(RHH_DEXNAV_TEST_FIXTURE) && DEXNAV_ENABLED
extern bool32 DexNavTestGetTile(s16 *x, s16 *y);
static void CheckDexNavCancellation(const char *module)
{
    SetupCancellationFixture(module); sTrackEncounterStarts = TRUE; sEncounterStartCount = 0;
    CHECK(RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 4)));
    struct MapLayout layout = *gMapHeader.mapLayout; struct Tileset primary = *layout.primaryTileset;
    u16 attributes[1] = {MB_TALL_GRASS}; primary.metatileAttributes = attributes;
    layout.primaryTileset = &primary; gMapHeader.mapLayout = &layout;
    gSaveBlock1Ptr->pos.x = 10; gSaveBlock1Ptr->pos.y = 10;
    ResetTasks(); ResetObjectEvents(); ResetSpriteData();
    InitPlayerAvatar(MAP_OFFSET + 10, MAP_OFFSET + 10, DIR_SOUTH, MALE);
    cgb_audio_init(42060); m4aSoundInit();
    InitGpuRegManager();
    const struct BgTemplate bg = {.bg = 0}; InitBgsFromTemplates(0, &bg, 1);
    SetDefaultFontsPointer(); InitStandardTextBoxWindows(); InitTextBoxGfxAndPrinters();
    StartWeather();
    u32 header = GetCurrentMapWildMonHeaderId(); CHECK(header != HEADER_NONE);
    const struct WildPokemonInfo *land = gWildMonHeaders[header].encounterTypes[TIME_OF_DAY_DEFAULT].landMonsInfo;
    CHECK(land); u32 species = land->wildPokemon[0].species;
    for (u32 pass = 0; pass < 2; ++pass)
    {
        VarSet(DN_VAR_SPECIES, species | (ENCOUNTER_TYPE_LAND << 14));
        bool32 found = FALSE; s16 x = 0, y = 0;
        for (u32 attempt = 0; attempt < 100 && !found; ++attempt)
        {
            ScriptContext_Init();
            (void)TryStartDexNavSearch();
            found = DexNavTestGetTile(&x, &y);
        }
        CHECK(found); gSaveBlock1Ptr->pos.x = x - MAP_OFFSET; gSaveBlock1Ptr->pos.y = y - MAP_OFFSET;
        gPlayerAvatar.creeping = TRUE; gSaveBlock3Ptr->dexNavChain = 5;
        struct Pokemon before[PARTY_SIZE]; memcpy(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before));
        sWildGateExpectedRng = gRngValue; sWildGateExpectedRng2 = gRng2Value; sWildGateCheckRng = !pass;
        sWildGateDeny = !pass;
        CHECK(OnStep_DexNavSearch() && !FlagGet(DN_FLAG_SEARCHING) && !DexNavTestGetTile(&x, &y)
            && ScriptContext_IsEnabled() && sWildGateCalls == pass + 1 && !sSpecialGateCalls
            && !sSpecialCancelCalls && !sEncounterStartCount);
        if (!pass)
        {
            CHECK(sWildCancelCalls == 1 && !sSpecialGeneratedCalls && !gSaveBlock3Ptr->dexNavChain
                && sLastWildCancellation.candidate.source == RHH_WILD_DEXNAV
                && sLastWildCancellation.candidate.area == RHH_WILD_LAND
                && sLastWildCancellation.candidate.slot == UINT32_MAX
                && sLastWildCancellation.candidate.x == x - MAP_OFFSET
                && sLastWildCancellation.candidate.y == y - MAP_OFFSET
                && sLastWildCancellation.candidate.table_time == RHH_WILD_TIME_MORNING
                && sLastWildCancellation.candidate.species == RhhContentKeyForId(RHH_SPECIES, species)
                && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before))
                && !memcmp(&gRngValue, &sWildGateExpectedRng, sizeof(gRngValue))
                && !memcmp(&gRng2Value, &sWildGateExpectedRng2, sizeof(gRng2Value)));
        }
        else
        {
            CHECK(sWildCancelCalls == 1 && sSpecialGeneratedCalls == 1 && gSaveBlock3Ptr->dexNavChain == 5
                && sLastSpecialGenerated.source == RHH_WILD_DEXNAV && sLastSpecialGenerated.identity
                && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == species
                && gDexNavSpecies == species);
        }
        /* Advance the actual got-away or DexNav battle script and its field
         * tasks, rather than treating an enabled script as a completed handoff. */
        for (u32 frame = 0; frame < 3000 && (ScriptContext_IsEnabled() || (pass && !sEncounterStartCount)); ++frame)
        {
            gMain.newKeys = A_BUTTON;
            ScriptContext_RunScript(); RunTasks(); RunTextPrinters(); AnimateSprites(); m4aSoundMain();
        }
        gMain.newKeys = 0;
        if (!pass) CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked() && !sEncounterStartCount);
        else CHECK(sEncounterStartCount == 1 && !ScriptContext_IsEnabled());
        gSaveBlock1Ptr->pos.x = 10; gSaveBlock1Ptr->pos.y = 10;
    }
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; if (module) RhhModsUnloadLibraries();
    puts("dexnav: search veto, rollback and generated battle candidate passed");
}
#endif
static void CheckWildActorService(void)
{
    sWildActorTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 0));
    CHECK(sWildActorTest && sWildActorTest->header.struct_size >= sizeof(*sWildActorTest)
        && sWildActorTest->slots() == OBJECT_EVENTS_COUNT);
    CHECK(RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 1)) == &sWildActorTest->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 2)) == &sWildActorTest->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 3)) == &sWildActorTest->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 5)) == &sWildActorTest->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 6)) == &sWildActorTest->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 7)) == &sWildActorTest->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 8)) == &sWildActorTest->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 9)) == &sWildActorTest->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 10)) == &sWildActorTest->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 11)) == &sWildActorTest->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 12)) == &sWildActorTest->header
        && !RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 19)));
    struct ObjectEvent savedObjects[OBJECT_EVENTS_COUNT], savedSavedObjects[OBJECT_EVENTS_COUNT];
    memcpy(savedObjects, gObjectEvents, sizeof(savedObjects));
    memcpy(savedSavedObjects, gSaveBlock1Ptr->objectEvents, sizeof(savedSavedObjects));
    memset(gObjectEvents, 0, sizeof(gObjectEvents)); struct Sprite savedSprite = gSprites[0];
    rng_value_t savedRng = gRngValue;
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 0);
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor), .handle = 999}, unchanged = actor;
    struct RhhWildActorEvent event = {.struct_size = sizeof(event), .cause = 999};
    CHECK(sWildActorTest->at(0, &actor) && !memcmp(&actor, &unchanged, sizeof(actor))
        && sWildActorTest->context(&event) && event.cause == 999);
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    u16 monGraphics = GetGraphicsIdForMon(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_IS_SHINY),
        GetGenderFromSpeciesAndPersonality(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_PERSONALITY)) == MON_FEMALE);
    memset(&gObjectEvents[0], 0, sizeof(gObjectEvents[0]));
    gObjectEvents[0].active = TRUE; gObjectEvents[0].trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER;
    gObjectEvents[0].localId = LOCALID_OW_ENCOUNTER_END; gObjectEvents[0].mapGroup = 1; gObjectEvents[0].mapNum = 0;
    gObjectEvents[0].graphicsId = monGraphics; gObjectEvents[0].trainerRange_berryTreeId = 5;
    gObjectEvents[0].currentCoords.x = 2 + MAP_OFFSET; gObjectEvents[0].currentCoords.y = 1 + MAP_OFFSET;
    sWildActorSpawned = sWildActorRemoved = sWildActorRestored = 0; rng_value_t rng = gRngValue;
    ModsWildActorSpawned(&gObjectEvents[0], &mon);
    CHECK(sWildActorSpawned == 1 && !sWildActorTest->at(0, &actor) && actor.x == 2 && actor.y == 1
        && actor.level == 5 && actor.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_POOCHYENA)
        && (actor.flags & (RHH_WILD_ACTOR_GENERATED | RHH_WILD_ACTOR_PRESERVED))
            == (RHH_WILD_ACTOR_GENERATED | RHH_WILD_ACTOR_PRESERVED));
    u64 firstHandle = actor.handle; struct Pokemon copy;
    CHECK(ModsWildActorCopyMon(&gObjectEvents[0], &copy) && !memcmp(&copy, &mon, sizeof(mon))
        && actor.identity == ((u64)GetMonData(&mon, MON_DATA_OT_ID) << 32 | GetMonData(&mon, MON_DATA_PERSONALITY)));
    gObjectEvents[0].mapNum = 1;
    CHECK(sWildActorTest->find(firstHandle, &actor) && !ModsWildActorCopyMon(&gObjectEvents[0], &copy));
    gObjectEvents[0].mapNum = 0;
    CHECK(!sWildActorTest->find(firstHandle, &actor));
    SaveObjectEvents(); ModsWildActorsBeforeSave(); u8 *snapshot = NULL; u32 snapshotSize = 0;
    CHECK(!RhhModsExport(&snapshot, &snapshotSize));
    u8 *bad = malloc(snapshotSize); CHECK(bad); memcpy(bad, snapshot, snapshotSize);
    u32 at = 24;
    while (at + 80 <= snapshotSize && strcmp((const char *)bad + at, "rhh:wild_actor_state"))
        at += 80 + ClockTestGet32(bad + at + 68);
    CHECK(at + 80 + 2 * (16 + sizeof(mon)) <= snapshotSize);
    memset(bad + at + 80 + 8, 0, 8); ClockTestPut32(bad + at + 64, 1);
    ClockTestEnvelope(bad, snapshotSize);
    CHECK(!RhhModsImport(bad, snapshotSize) && ModsWildActorCopyMon(&gObjectEvents[0], &copy)
        && !memcmp(&copy, &mon, sizeof(mon)) && !RhhModsImport(snapshot, snapshotSize));
    memcpy(bad, snapshot, snapshotSize);
    memcpy(bad + at + 80 + 16 + sizeof(mon), bad + at + 80, 16 + sizeof(mon));
    bad[at + 80 + 16 + sizeof(mon) + 1] = LOCALID_OW_ENCOUNTER_END - 1;
    ClockTestEnvelope(bad, snapshotSize);
    CHECK(RhhModsImport(bad, snapshotSize) && !sWildActorTest->find(firstHandle, &actor));
    memcpy(bad, snapshot, snapshotSize);
    bad[at + 80 + 16 + offsetof(struct Pokemon, box) + offsetof(struct BoxPokemon, checksum)] ^= 1;
    ClockTestEnvelope(bad, snapshotSize);
    CHECK(RhhModsImport(bad, snapshotSize) && !sWildActorTest->find(firstHandle, &actor)
        && ModsWildActorCopyMon(&gObjectEvents[0], &copy) && !memcmp(&copy, &mon, sizeof(mon)));
    free(bad);
    ClearObjectEvent(&gObjectEvents[0]); CHECK(sWildActorRemoved == 1 && sWildActorTest->find(firstHandle, &actor));
    CHECK(!RhhModsImport(snapshot, snapshotSize)); free(snapshot); LoadObjectEvents(); ModsWildActorsAfterLoad();
    CHECK(sWildActorRestored == 1 && sLastWildActor.cause == RHH_WILD_ACTOR_RESTORE
        && sWildActorTest->find(firstHandle, &actor) && !sWildActorTest->at(0, &actor)
        && actor.handle != firstHandle && (actor.flags & RHH_WILD_ACTOR_PRESERVED)
        && ModsWildActorCopyMon(&gObjectEvents[0], &copy) && !memcmp(&copy, &mon, sizeof(mon)));
    firstHandle = actor.handle;
    ModsWildActorsDiscardLive();
    CHECK(sWildActorTest->find(firstHandle, &actor));
    ModsWildActorsAfterLoad();
    CHECK(sWildActorRestored == 2 && !sWildActorTest->at(0, &actor) && actor.handle != firstHandle
        && ModsWildActorCopyMon(&gObjectEvents[0], &copy) && !memcmp(&copy, &mon, sizeof(mon)));
    firstHandle = actor.handle;
    gObjectEvents[0].currentCoords.x++;
    CHECK(!sWildActorTest->find(firstHandle, &actor) && actor.x == 3 && actor.handle == firstHandle);
    gObjectEvents[0].graphicsId = OBJ_EVENT_GFX_SPECIES(ZIGZAGOON);
    CHECK(!ModsWildActorCopyMon(&gObjectEvents[0], &copy) && !sWildActorTest->at(0, &actor)
        && !(actor.flags & RHH_WILD_ACTOR_PRESERVED));
    gObjectEvents[0].graphicsId = monGraphics;
    ClearObjectEvent(&gObjectEvents[0]);
    CHECK(sWildActorRemoved == 2 && sLastWildActor.cause == RHH_WILD_ACTOR_CLEAR
        && sWildActorTest->find(firstHandle, &actor));
    gObjectEvents[0].active = TRUE; gObjectEvents[0].trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER;
    gObjectEvents[0].localId = LOCALID_OW_ENCOUNTER_END; gObjectEvents[0].mapGroup = 1; gObjectEvents[0].mapNum = 0;
    gObjectEvents[0].graphicsId = OBJ_EVENT_GFX_SPECIES(POOCHYENA); gObjectEvents[0].spriteId = 0;
    gObjectEvents[0].warpArrowSpriteId = ROAMER_COUNT + 2;
    gObjectEvents[0].currentCoords.x = 2 + MAP_OFFSET; gObjectEvents[0].currentCoords.y = 1 + MAP_OFFSET;
    ModsWildActorSpawned(&gObjectEvents[0], NULL);
    CHECK(sWildActorSpawned == 2 && !sWildActorTest->at(0, &actor) && actor.handle != firstHandle
        && !(actor.flags & RHH_WILD_ACTOR_PRESERVED) && sWildActorTest->find(firstHandle, &actor));
    u64 secondHandle = sLastWildActor.actor.handle;
    ClearObjectEvent(&gObjectEvents[0]);
    CHECK(sWildActorRemoved == 3 && sLastWildActor.cause == RHH_WILD_ACTOR_CLEAR
        && sWildActorTest->find(secondHandle, &actor) && !memcmp(&rng, &gRngValue, sizeof(rng)));
    gObjectEvents[0].active = TRUE; gObjectEvents[0].trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER;
    gObjectEvents[0].localId = LOCALID_OW_ENCOUNTER_END; gObjectEvents[0].mapGroup = 1; gObjectEvents[0].mapNum = 0;
    gObjectEvents[0].graphicsId = monGraphics; gObjectEvents[0].trainerRange_berryTreeId = 5;
    ModsWildActorSpawned(&gObjectEvents[0], &mon); u64 thirdHandle = sLastWildActor.actor.handle;
    CHECK(sWildActorSpawned == 3 && !sWildActorTest->find(thirdHandle, &actor));
    RhhModsSetRunContext(123, 789, 0);
    CHECK(sWildActorTest->find(thirdHandle, &actor) && !ModsWildActorCopyMon(&gObjectEvents[0], &copy));
    ClearObjectEvent(&gObjectEvents[0]); RhhModsSetRunContext(123, 456, 0);
    CHECK(sWildActorRemoved == 3 && sWildActorTest->find(thirdHandle, &actor));
    sWildActorTest = NULL; memcpy(gObjectEvents, savedObjects, sizeof(savedObjects));
    memcpy(gSaveBlock1Ptr->objectEvents, savedSavedObjects, sizeof(savedSavedObjects)); gSprites[0] = savedSprite; gRngValue = savedRng;
    RhhModsResetState(); RhhModsSetRunContext(0, 0, 0);
    puts("wild actors: committed lifetimes, stale handles, save restoration and preserved Pokémon passed");
}
extern void StartWildBattleWithOWE(struct ScriptContext *ctx);
static unsigned sIndividualPersistCalls;
static bool32 sIndividualPersistFails;
static u8 *sIndividualJournal;
static u32 sIndividualJournalSize;
static int IndividualTestPersist(u64 runId, const u8 *data, u32 size)
{
    CHECK(runId == 456 && data && size); ++sIndividualPersistCalls;
    if (sIndividualPersistFails) return -1;
    u8 *copy = malloc(size); CHECK(copy); memcpy(copy, data, size);
    free(sIndividualJournal); sIndividualJournal = copy; sIndividualJournalSize = size;
    return 0;
}
static void CheckWildIndividualPersistenceFailure(void)
{
    InitModsForTest();
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456);
    RhhModsSetRunContext(123, 456, 1); RhhModsSetPersistence(IndividualTestPersist);
    gMain.callback2 = CB2_Overworld;
    sWildActorTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    CHECK(sWildActorTest);
    memset(gObjectEvents, 0, sizeof(gObjectEvents));
    sIndividualPersistFails = TRUE; sIndividualPersistCalls = 0;
    CHECK(!NativeSaveBegin() && NativeSaveTransactionActive());
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 19);
    struct ObjectEvent *object = &gObjectEvents[0]; memset(object, 0, sizeof(*object)); object->active = TRUE;
    object->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; object->localId = LOCALID_OW_ENCOUNTER_END;
    object->mapGroup = 1; object->mapNum = 0; object->trainerRange_berryTreeId = 5;
    object->graphicsId = GetGraphicsIdForMon(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_IS_SHINY),
        GetGenderFromSpeciesAndPersonality(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_PERSONALITY)) == MON_FEMALE);
    ModsWildActorSpawned(object, &mon); u64 handle = sLastWildActor.actor.handle;
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id), .serial = 999};
    ModsWildActorsUpdate();
    CHECK(!sIndividualPersistCalls && sWildActorTest->individual(handle, &id) && id.serial == 999);
    CHECK(NativeSaveEnd(1, FALSE) && !NativeSaveTransactionActive());
    SaveObjectEvents(); u8 *legacy = NULL; u32 legacySize = 0;
    CHECK(!RhhModsExport(&legacy, &legacySize));
    ModsWildActorEngaged(object);
    CHECK(!sLastWildEventIndividual.serial);
    ModsWildActorsUpdate(); ModsWildActorsUpdate();
    u8 zero[8] = {0}; const u8 *sequence = RhhModsState("rhh:wild_actor_sequence");
    CHECK(sequence && !memcmp(sequence, zero, sizeof(zero)) && sIndividualPersistCalls == 1 && sObservedPersistenceFailures == 1
        && sWildActorTest->individual(handle, &id) && id.serial == 999 && !sWildActorIdentified);
    sIndividualPersistFails = FALSE;
    CHECK(!RhhModsFlushDurable()); ModsWildActorsAfterSave(); sTestNestedActorContext = TRUE; ModsWildActorsUpdate();
    CHECK(sIndividualPersistCalls == 3 && sWildActorIdentified == 1
        && sNestedActorContextChecks == 1 && !sWildActorTest->individual(handle, &id) && id.run_id == 456 && id.serial);
    u64 firstSerial = id.serial;
    gSpecialVar_LastTalked = object->localId; ModsWildActorBattleTransition();
    ClearObjectEvent(object); ModsWildActorBattleFinished(B_OUTCOME_WON);
    CHECK(sWildActorBattleEnded == 1 && sLastWildBattleIndividual.run_id == id.run_id
        && sLastWildBattleIndividual.serial == firstSerial);
    CHECK(!RhhModsImport(legacy, legacySize) && sIndividualJournal
        && !RhhModsMergeJournal(sIndividualJournal, sIndividualJournalSize));
    free(legacy); LoadObjectEvents(); ModsWildActorsAfterLoad();
    struct RhhWildActorSnapshot restored = {.struct_size = sizeof(restored)};
    CHECK(!sWildActorTest->at(0, &restored) && restored.handle != handle);
    handle = restored.handle; id.serial = 999;
    ModsWildActorsUpdate();
    CHECK(sIndividualPersistCalls == 3 && sWildActorTest->individual(handle, &id) && id.serial == 999);
    ModsWildActorsBeforeSave();
    CHECK(sIndividualPersistCalls == 4 && sWildActorTest->individual(handle, &id) && id.serial == 999);
    CHECK(!RhhModsFlushDurable()); ModsWildActorsAfterSave();
    CHECK(sIndividualPersistCalls == 5 && sWildActorIdentified == 2
        && !sWildActorTest->individual(handle, &id) && id.serial > firstSerial);
    puts("wild actor: identity journal failure and recovery passed");
    free(sIndividualJournal); sIndividualJournal = NULL; sIndividualJournalSize = 0;
    sWildActorTest = NULL; RhhModsShutdown();
}
static void CheckWildActorSave(const char *path, const char *expectedPath, bool32 load, const char *module)
{
    InitModsForTestWithLibrary(module);
    CHECK(NativeSaveOpen(path) == 0); gFlashMemoryPresent = TRUE;
    sWildActorTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 0));
    CHECK(sWildActorTest);
    sWildConcealTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_concealment", RHH_SERVICE_VERSION(1, 0));
    CHECK(sWildConcealTest);
    if (!load)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
        struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 19);
        SetMonMoveSlot(&mon, MOVE_TACKLE, 0); u16 held = ITEM_ORAN_BERRY; SetMonData(&mon, MON_DATA_HELD_ITEM, &held);
        memset(gObjectEvents, 0, sizeof(gObjectEvents));
        struct ObjectEvent *object = &gObjectEvents[0]; object->active = TRUE;
        object->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; object->localId = LOCALID_OW_ENCOUNTER_END;
        object->mapGroup = 1; object->mapNum = 0; object->trainerRange_berryTreeId = 5;
        object->warpArrowSpriteId = ROAMER_COUNT + 2;
        object->graphicsId = GetGraphicsIdForMon(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_IS_SHINY),
            GetGenderFromSpeciesAndPersonality(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_PERSONALITY)) == MON_FEMALE);
        ModsWildActorSpawned(object, &mon); u64 generatedHandle = sLastWildActor.actor.handle;
        struct ObjectEvent *manual = &gObjectEvents[1]; *manual = *object; manual->localId = 12;
        ModsWildActorSpawned(manual, NULL); u64 manualHandle = sLastWildActor.actor.handle;
        struct RhhWildActorIndividual id = {.struct_size = sizeof(id), .serial = 999};
        CHECK(sWildActorTest->individual(generatedHandle, &id) && id.serial == 999);
        gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
        gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
        gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16));
        CHECK(gBackupMapLayout.map); ResetSpriteData(); gSprites[0].inUse = TRUE; object->spriteId = 0;
        gMain.callback2 = CB2_Overworld; ScriptContext_Init(); UnlockPlayerFieldControls();
        struct RhhWildConcealRequest request = {.struct_size = sizeof(request), .handle = generatedHandle,
            .owner = RhhContentKeyOf("test:mon_edit_rules"), .mode = RHH_WILD_CONCEAL_HIDDEN,
            .reveal_flags = RHH_WILD_REVEAL_ON_APPROACH, .reveal_radius = 3};
        struct RhhWildConcealResult conceal = {.struct_size = sizeof(conceal)};
        CHECK(!sWildConcealTest->apply(&request, &conceal));
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
        CHECK(sWildActorIdentified == 2 && !sWildActorTest->individual(generatedHandle, &id)
            && id.run_id && id.serial);
        struct RhhWildActorIndividual manualId = {.struct_size = sizeof(manualId)};
        CHECK(!sWildActorTest->individual(manualHandle, &manualId) && manualId.run_id == id.run_id
            && manualId.serial && manualId.serial != id.serial);
        struct { struct Pokemon mon; u64 runId, serial, manualSerial; } expectedData = {mon, id.run_id, id.serial, manualId.serial};
        FILE *expected = fopen(expectedPath, "wb"); CHECK(expected);
        CHECK(fwrite(&expectedData, sizeof(expectedData), 1, expected) == 1 && !fclose(expected));
    }
    else
    {
        struct { struct Pokemon mon; u64 runId, serial, manualSerial; } expectedData;
        struct Pokemon actual; FILE *file = fopen(expectedPath, "rb"); CHECK(file);
        CHECK(fread(&expectedData, sizeof(expectedData), 1, file) == 1);
        u64 previous = expectedData.manualSerial, seen;
        while (fread(&seen, sizeof(seen), 1, file) == 1) { CHECK(seen > previous); previous = seen; }
        CHECK(feof(file) && !fclose(file));
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
        struct RhhWildActorIndividual id = {.struct_size = sizeof(id)}, untouched = {.struct_size = 1, .serial = 999};
        CHECK(sWildActorRestored == 2 && !sWildActorTest->at(0, &actor)
            && (actor.flags & RHH_WILD_ACTOR_PRESERVED)
            && ModsWildActorCopyMon(&gObjectEvents[0], &actual) && !memcmp(&expectedData.mon, &actual, sizeof(actual))
            && !sWildActorTest->individual(actor.handle, &id) && id.run_id == expectedData.runId
            && id.serial == expectedData.serial && !sWildActorTest->find_individual(&id, &actor)
            && sWildActorTest->individual(actor.handle, &untouched) && untouched.serial == 999);
        struct RhhWildConcealment restored = {.struct_size = sizeof(restored)};
        CHECK((actor.flags & RHH_WILD_ACTOR_HIDDEN) && !sWildConcealTest->at(actor.handle, &restored)
            && restored.mode == RHH_WILD_CONCEAL_HIDDEN && restored.reveal_radius == 3
            && restored.owner == RhhContentKeyOf("test:mon_edit_rules") && restored.revision == 1);
        /* A save can outlive its owner module. A missing owner must not leave the actor hidden. */
        u8 *concealState = RhhModsState("rhh:wild_conceal_state"); CHECK(concealState);
        ClockTestPut64(concealState + 8, RhhContentKeyOf("test:absent_conceal_owner"));
        ModsWildActorsAfterLoad(); actor.struct_size = sizeof(actor);
        CHECK(!sWildActorTest->at(0, &actor) && !(actor.flags & RHH_WILD_ACTOR_HIDDEN)
            && !sWildConcealTest->at(actor.handle, &restored) && !restored.mode);
        struct RhhWildActorIndividual manualId = {.struct_size = sizeof(manualId),
            .run_id = expectedData.runId, .serial = expectedData.manualSerial};
        CHECK(!sWildActorTest->find_individual(&manualId, &actor) && actor.local_id == 12
            && !(actor.flags & RHH_WILD_ACTOR_PRESERVED) && !ModsWildActorCopyMon(&gObjectEvents[1], &actual));
        struct ObjectEvent *other = &gObjectEvents[2]; *other = gObjectEvents[0];
        other->localId = LOCALID_OW_ENCOUNTER_END - 1;
        ModsWildActorSpawned(other, &expectedData.mon); u64 otherHandle = sLastWildActor.actor.handle;
        struct RhhWildActorIndividual next = {.struct_size = sizeof(next)};
        CHECK(sWildActorTest->individual(otherHandle, &next));
        gMain.callback2 = CB2_Overworld; ModsWildActorsUpdate();
        CHECK(!sWildActorTest->individual(otherHandle, &next) && next.run_id == id.run_id && next.serial > previous);
        file = fopen(expectedPath, "ab"); CHECK(file);
        CHECK(fwrite(&next.serial, sizeof(next.serial), 1, file) == 1 && !fclose(file));
        gMapHeader = *gMapGroups[1][0];
        const u8 localId[] = {LOCALID_OW_ENCOUNTER_END, 0}; struct ScriptContext script = {.scriptPtr = localId};
        StartWildBattleWithOWE(&script);
        CHECK(!memcmp(&expectedData.mon, &gParties[B_TRAINER_OPPONENT_A][0], sizeof(actual)));
        gSpecialVar_LastTalked = LOCALID_OW_ENCOUNTER_END; ModsWildActorBattleTransition();
        ClearObjectEvent(&gObjectEvents[0]);
        CHECK(sLastWildEventIndividual.run_id == expectedData.runId && sLastWildEventIndividual.serial == expectedData.serial
            && sWildActorTest->find_individual(&id, &actor));
        ModsWildActorBattleFinished(B_OUTCOME_WON);
        CHECK(sWildActorBattleEnded == 1 && sLastWildBattleIndividual.run_id == expectedData.runId
            && sLastWildBattleIndividual.serial == expectedData.serial);
        puts("wild actor: fresh-process identity, rollback-safe allocation and battle identity passed");
    }
    sWildActorTest = NULL; sWildConcealTest = NULL; NativeSaveClose();
    if (module) RhhModsUnloadLibraries();
}
static void CheckManualFacilityWild(bool32 pike, const char *module)
{
    SetupCancellationFixture(module);
    sTrackEncounterStarts = TRUE; sEncounterStartCount = 0;
    u32 map = pike ? MAP_BATTLE_FRONTIER_BATTLE_PIKE_ROOM_WILD_MONS : MAP_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR;
    free(gBackupMapLayout.map); gMapHeader = *gMapGroups[MAP_GROUP(map)][MAP_NUM(map)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(map); gSaveBlock1Ptr->location.mapNum = MAP_NUM(map);
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16));
    CHECK(gBackupMapLayout.map && GetCurrentMapWildMonHeaderId() == HEADER_NONE);
    gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 0;
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50] = 0;
    gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50] = 0;
    ResetTasks();
    struct ObjectEvent *actor = &gObjectEvents[1]; memset(actor, 0, sizeof(*actor));
    actor->active = TRUE; actor->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; actor->localId = 12;
    actor->mapGroup = MAP_GROUP(map); actor->mapNum = MAP_NUM(map);
    actor->graphicsId = pike ? OBJ_EVENT_GFX_SPECIES(SEVIPER) : OBJ_EVENT_GFX_SPECIES(POOCHYENA);
    actor->trainerRange_berryTreeId = pike ? 5 : 1;
    actor->warpArrowSpriteId = ROAMER_COUNT + 2;
    actor->currentCoords.x = actor->previousCoords.x = MAP_OFFSET + 2;
    actor->currentCoords.y = actor->previousCoords.y = MAP_OFFSET + 1;
    struct ObjectEvent actorBefore = *actor;
    struct Pokemon enemyBefore[PARTY_SIZE]; memcpy(enemyBefore, gParties[B_TRAINER_OPPONENT_A], sizeof(enemyBefore));
    u32 countBefore = gPartiesCount[B_TRAINER_OPPONENT_A];
    sWildGateExpectedRng = gRngValue; sWildGateExpectedRng2 = gRng2Value; sWildGateCheckRng = TRUE;
    const u8 localId[] = {12, 0}; struct ScriptContext script = {.scriptPtr = localId};
    StartWildBattleWithOWE(&script);
    CHECK(!gSpecialVar_Result && sWildGateCalls == 1 && sWildCancelCalls == 1 && !sEncounterStartCount
        && sLastWildCancellation.candidate.source == (pike ? RHH_WILD_MANUAL_PIKE : RHH_WILD_MANUAL_PYRAMID)
        && sLastWildCancellation.candidate.x == 2 && sLastWildCancellation.candidate.y == 1
        && !memcmp(&actorBefore, actor, sizeof(actorBefore))
        && !memcmp(enemyBefore, gParties[B_TRAINER_OPPONENT_A], sizeof(enemyBefore))
        && countBefore == gPartiesCount[B_TRAINER_OPPONENT_A]);
    /* Inspect and execute the actual interaction script's branch after the
     * callnative operands: a rejected battle must skip waitstate. */
    const u8 *call = InteractWithOverworldWildEncounter;
    unsigned offset = 0;
    while (offset < 120 && !Script_MatchesCallNative(call + offset, StartWildBattleWithOWE, TRUE)) ++offset;
    CHECK(offset < 120);
    const u8 *branch = call + offset + 1 + sizeof(uintptr_t) + 2;
    CHECK(branch[0] == SCR_OP_COMPARE_VAR_TO_VALUE);
    struct ScriptContext next = {.scriptPtr = branch + 1};
    CHECK(!gScriptCmdTable[SCR_OP_COMPARE_VAR_TO_VALUE](&next) && *next.scriptPtr++ == SCR_OP_GOTO_IF);
    CHECK(!gScriptCmdTable[SCR_OP_GOTO_IF](&next) && *next.scriptPtr == SCR_OP_RELEASEALL);
    struct RhhWildEncounterContext denied = sLastWildCancellation.candidate;
    sWildGateDeny = FALSE; sWildGateCheckRng = FALSE;
    script = (struct ScriptContext){.scriptPtr = localId}; StartWildBattleWithOWE(&script);
    struct Pokemon *opponent = &gParties[B_TRAINER_OPPONENT_A][0];
    u64 identity = (u64)GetMonData(opponent, MON_DATA_OT_ID) << 32 | GetMonData(opponent, MON_DATA_PERSONALITY);
    CHECK(gSpecialVar_Result && sWildGateCalls == 2 && sWildCancelCalls == 1 && sSpecialGeneratedCalls == 1
        && sEncounterStartCount == 1 && sLastSpecialGenerated.source == denied.source
        && sLastSpecialGenerated.species == denied.species && sLastSpecialGenerated.identity == denied.identity
        && identity == denied.identity && RhhContentKeyForId(RHH_SPECIES, GetMonData(opponent, MON_DATA_SPECIES)) == denied.species);
    next = (struct ScriptContext){.scriptPtr = branch + 1};
    CHECK(!gScriptCmdTable[SCR_OP_COMPARE_VAR_TO_VALUE](&next) && *next.scriptPtr++ == SCR_OP_GOTO_IF);
    CHECK(!gScriptCmdTable[SCR_OP_GOTO_IF](&next) && *next.scriptPtr == SCR_OP_WAITSTATE);
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; if (module) RhhModsUnloadLibraries();
    puts(pike ? "manual facility: pike veto and battle passed" : "manual facility: pyramid veto and battle passed");
}
static void CheckExternalWildActor(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsLoadLibrary(path)); CHECK(!RhhModsFinalize());
    gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 0);
    ResetTasks(); ResetSpriteData(); FreeAllSpritePalettes(); StartWeather();
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 12345, OTID_STRUCT_PRESET(54321), 19);
    SetMonMoveSlot(&mon, MOVE_TACKLE, 0); u16 held = ITEM_ORAN_BERRY; SetMonData(&mon, MON_DATA_HELD_ITEM, &held);
    bool32 shiny = GetMonData(&mon, MON_DATA_IS_SHINY);
    bool32 female = GetGenderFromSpeciesAndPersonality(SPECIES_POOCHYENA, GetMonData(&mon, MON_DATA_PERSONALITY)) == MON_FEMALE;
    memset(&gObjectEvents[0], 0, sizeof(gObjectEvents[0])); gObjectEvents[0].active = TRUE;
    gObjectEvents[0].trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; gObjectEvents[0].localId = LOCALID_OW_ENCOUNTER_END;
    gObjectEvents[0].mapGroup = 1; gObjectEvents[0].mapNum = 0; gObjectEvents[0].trainerRange_berryTreeId = 5;
    gObjectEvents[0].warpArrowSpriteId = ROAMER_COUNT + 2; gObjectEvents[0].spriteId = 0;
    gObjectEvents[0].graphicsId = GetGraphicsIdForMon(SPECIES_POOCHYENA, shiny, female);
    gObjectEvents[0].currentCoords.x = 2 + MAP_OFFSET; gObjectEvents[0].currentCoords.y = 1 + MAP_OFFSET;
    gPlayerAvatar.objectEventId = 1; gPlayerAvatar.spriteId = 1; gObjectEvents[1].active = TRUE; gObjectEvents[1].isPlayer = TRUE;
    gObjectEvents[1].spriteId = 1; gSprites[1].inUse = TRUE;
    ModsWildActorSpawned(&gObjectEvents[0], &mon);
    const u8 localId[] = {LOCALID_OW_ENCOUNTER_END, 0}; struct ScriptContext script = {.scriptPtr = localId};
    StartWildBattleWithOWE(&script);
    CHECK(!memcmp(&mon, &gParties[B_TRAINER_OPPONENT_A][0], sizeof(mon)));
    puts("WILD_ACTOR_BATTLE_IDENTITY");
    gSpecialVar_LastTalked = LOCALID_OW_ENCOUNTER_END; DespawnOWEOnBattleStart();
    ResetTasks();
    struct ObjectEventTemplate manual = {.localId = 12, .graphicsId = OBJ_EVENT_GFX_SPECIES(POOCHYENA),
        .x = 2, .y = 1, .trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER, .trainerRange_berryTreeId = 5};
    u32 slot = SpawnSpecialObjectEvent(&manual); CHECK(slot < OBJECT_EVENTS_COUNT);
    RemoveObjectEvent(&gObjectEvents[slot]);
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; RhhModsUnloadLibraries();
}
#if WE_OW_ENCOUNTERS
static void CheckFacilityOverworld(bool32 pike, const char *module)
{
    SetupCancellationFixture(module); sTrackEncounterStarts = TRUE; sEncounterStartCount = 0;
    struct RhhModValue enabled = {.type = RHH_SETTING_BOOL, .value = 1};
    CHECK(!RhhModsConfigure("rhh:nuzlocke", "enabled", &enabled));
    u8 *nuzlocke = RhhModsState("rhh:nuzlocke"); CHECK(nuzlocke && !nuzlocke[4] && !nuzlocke[5]);
    u32 map = pike ? MAP_BATTLE_FRONTIER_BATTLE_PIKE_ROOM_WILD_MONS : MAP_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR;
    free(gBackupMapLayout.map); gMapHeader = *gMapGroups[MAP_GROUP(map)][MAP_NUM(map)];
    CHECK(AddBagItem(ITEM_POKE_BALL, 1));
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(map); gSaveBlock1Ptr->location.mapNum = MAP_NUM(map);
    struct MapLayout layout = *gMapHeader.mapLayout; struct Tileset primary = *layout.primaryTileset;
    u16 attributes[1] = {MB_TALL_GRASS}; primary.metatileAttributes = attributes;
    layout.primaryTileset = &primary; gMapHeader.mapLayout = &layout;
    gBackupMapLayout.width = layout.width + MAP_OFFSET_W; gBackupMapLayout.height = layout.height + MAP_OFFSET_H;
    u32 size = gBackupMapLayout.width * gBackupMapLayout.height;
    gBackupMapLayout.map = calloc(size, sizeof(u16)); CHECK(gBackupMapLayout.map);
    for (u32 i = 0; i < size; ++i) gBackupMapLayout.map[i] = PACK_ELEVATION(3);
    gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 0;
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50] = 0;
    gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50] = 0;
    ResetTasks(); ResetSpriteData(); FreeAllSpritePalettes(); StartWeather();
    memset(gObjectEvents, 0, sizeof(gObjectEvents)); gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
    gPlayerAvatar.flags = PLAYER_AVATAR_FLAG_ON_FOOT;
    struct ObjectEvent *player = &gObjectEvents[0]; player->active = TRUE; player->isPlayer = TRUE;
    player->mapGroup = MAP_GROUP(map); player->mapNum = MAP_NUM(map); player->spriteId = 0;
    player->graphicsId = OBJ_EVENT_GFX_BRENDAN_NORMAL; player->currentElevation = player->previousElevation = 3;
    player->currentCoords.x = player->previousCoords.x = MAP_OFFSET + 1;
    player->currentCoords.y = player->previousCoords.y = MAP_OFFSET + 1;
    gSprites[0].inUse = TRUE; const struct ObjectEventGraphicsInfo *graphics = GetObjectEventGraphicsInfo(OBJ_EVENT_GFX_BRENDAN_NORMAL);
    gSprites[0].anims = graphics->anims; gSprites[0].images = graphics->images;
    gSprites[0].animBeginning = FALSE; gSprites[0].animPaused = TRUE;
    ScriptContext_Init(); UnlockPlayerFieldControls(); gMain.callback2 = CB2_Overworld;
    sWildActorTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 0));
    CHECK(sWildActorTest); sWildTest = sWildGateService;
    struct RhhWildActorSpawnRequest request = {.struct_size = sizeof(request),
        .map = RhhContentKeyForId(RHH_MAP, map), .x = 4, .y = 4};
    struct Pokemon before[PARTY_SIZE]; memcpy(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before));
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor), .handle = 999}, unchanged = actor;
    CHECK(sWildActorTest->spawn(&request, &actor) == RHH_WILD_ACTOR_DENIED
        && !memcmp(&actor, &unchanged, sizeof(actor)) && !sWildActorSpawned && sWildCancelCalls == 1
        && sLastWildCancellation.candidate.source == (pike ? RHH_WILD_OVERWORLD_PIKE : RHH_WILD_OVERWORLD_PYRAMID)
        && sLastWildCancellation.candidate.x == 4 && sLastWildCancellation.candidate.y == 4
        && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)) && !sEncounterStartCount
        && !nuzlocke[4] && !nuzlocke[5]);
    struct RhhWildEncounterContext denied = sLastWildCancellation.candidate;
    sWildGateDeny = FALSE;
    CHECK(sWildActorTest->spawn(&request, &actor) == RHH_WILD_ACTOR_OK && sSpecialGeneratedCalls == 1
        && sWildActorSpawned == 1 && !sEncounterStartCount
        && sLastSpecialGenerated.source == denied.source && sLastSpecialGenerated.species == denied.species
        && sLastSpecialGenerated.identity == denied.identity && actor.species == denied.species
        && actor.level == denied.level && (actor.flags & RHH_WILD_ACTOR_PRESERVED));
    struct Pokemon preserved; CHECK(ModsWildActorCopyMon(&gObjectEvents[actor.slot], &preserved));
    /* Restore the actor from the host's saved record before engagement. */
    ModsWildActorsDiscardLive(); ModsWildActorsAfterLoad();
    struct Pokemon restored;
    CHECK(ModsWildActorCopyMon(&gObjectEvents[actor.slot], &restored)
        && !memcmp(&preserved, &restored, sizeof(preserved)) && sWildActorRestored == 1);
    const u8 localId[] = {actor.local_id, 0}; struct ScriptContext script = {.scriptPtr = localId};
    StartWildBattleWithOWE(&script);
    CHECK(sEncounterStartCount == 1 && sWildGateCalls == 2 && sSpecialGeneratedCalls == 1
        && nuzlocke[4] == 1 && !nuzlocke[5] && ModsCanCapture()
        && !memcmp(&preserved, &gParties[B_TRAINER_OPPONENT_A][0], sizeof(preserved)));
    if (!pike)
    {
        /* Old Pyramid saves put the selector into level while retaining the
         * generated mon's experience. Its real level must be recovered. */
        ResetTasks(); UnlockPlayerFieldControls(); UnfreezeObjectEvents();
        struct Pokemon legacy = preserved; u32 selector = 1;
        SetMonData(&legacy, MON_DATA_LEVEL, &selector);
        gObjectEvents[actor.slot].trainerRange_berryTreeId = selector;
        ModsWildActorSpawned(&gObjectEvents[actor.slot], &legacy);
        struct ScriptContext again = {.scriptPtr = localId};
        StartWildBattleWithOWE(&again);
        CHECK(sEncounterStartCount == 2 && sWildGateCalls == 2
            && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_LEVEL) == denied.level
            && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_EXP) == GetMonData(&preserved, MON_DATA_EXP)
            && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_PERSONALITY) == GetMonData(&preserved, MON_DATA_PERSONALITY));
    }
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; if (module) RhhModsUnloadLibraries();
    puts(pike ? "facility wild: overworld pike veto and retained battle passed"
              : "facility wild: overworld pyramid veto and retained battle passed");
}
static void CheckSpecialOverworld(bool32 useRoamer)
{
    SetupCancellationFixture(NULL); sTrackEncounterStarts = TRUE; sEncounterStartCount = 0;
    struct Roamer original = {0}; bool32 water = FALSE;
    if (useRoamer)
    {
        DeactivateAllRoamers(); CHECK(TryAddRoamer(SPECIES_LATIAS, 40)); original = gSaveBlock1Ptr->roamer[0];
        u8 group, number; GetRoamerLocation(0, &group, &number);
        free(gBackupMapLayout.map); gMapHeader = *gMapGroups[group][number];
        gSaveBlock1Ptr->location.mapGroup = group; gSaveBlock1Ptr->location.mapNum = number;
        u32 header = GetCurrentMapWildMonHeaderId(); CHECK(header != HEADER_NONE);
        water = gWildMonHeaders[header].encounterTypes[TIME_OF_DAY_DEFAULT].landMonsInfo == NULL;
        CHECK(!water || gWildMonHeaders[header].encounterTypes[TIME_OF_DAY_DEFAULT].waterMonsInfo);
    }
    else
    {
        gSaveBlock1Ptr->outbreakPokemonSpecies = SPECIES_SEEDOT; gSaveBlock1Ptr->outbreakPokemonLevel = 12;
        gSaveBlock1Ptr->outbreakPokemonMoves[0] = MOVE_TACKLE; gSaveBlock1Ptr->outbreakPokemonProbability = 100;
        gSaveBlock1Ptr->outbreakDaysLeft = 3; gSaveBlock1Ptr->outbreakLocationMapGroup = MAP_GROUP(MAP_ROUTE103);
        gSaveBlock1Ptr->outbreakLocationMapNum = MAP_NUM(MAP_ROUTE103);
        free(gBackupMapLayout.map);
    }
    struct MapLayout layout = *gMapHeader.mapLayout; struct Tileset primary = *layout.primaryTileset;
    u16 attributes[1] = {water ? MB_DEEP_WATER : MB_TALL_GRASS}; primary.metatileAttributes = attributes;
    layout.primaryTileset = &primary; CHECK(layout.width > 10 && layout.height > 8); gMapHeader.mapLayout = &layout;
    gBackupMapLayout.width = layout.width + MAP_OFFSET_W; gBackupMapLayout.height = layout.height + MAP_OFFSET_H;
    u32 count = gBackupMapLayout.width * gBackupMapLayout.height;
    gBackupMapLayout.map = calloc(count, sizeof(u16)); CHECK(gBackupMapLayout.map);
    for (u32 i = 0; i < count; ++i) gBackupMapLayout.map[i] = PACK_ELEVATION(3);
    ResetTasks(); ResetSpriteData(); FreeAllSpritePalettes(); StartWeather();
    memset(gObjectEvents, 0, sizeof(gObjectEvents)); gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
    gPlayerAvatar.flags = water ? PLAYER_AVATAR_FLAG_SURFING : PLAYER_AVATAR_FLAG_ON_FOOT;
    struct ObjectEvent *player = &gObjectEvents[0]; player->active = TRUE; player->isPlayer = TRUE;
    player->mapGroup = gSaveBlock1Ptr->location.mapGroup; player->mapNum = gSaveBlock1Ptr->location.mapNum;
    player->graphicsId = OBJ_EVENT_GFX_BRENDAN_NORMAL; player->spriteId = 0;
    player->currentElevation = player->previousElevation = 3;
    player->currentCoords.x = player->previousCoords.x = MAP_OFFSET + layout.width / 2;
    player->currentCoords.y = player->previousCoords.y = MAP_OFFSET + layout.height / 2;
    gSprites[0].inUse = TRUE; const struct ObjectEventGraphicsInfo *graphics = GetObjectEventGraphicsInfo(OBJ_EVENT_GFX_BRENDAN_NORMAL);
    gSprites[0].anims = graphics->anims; gSprites[0].images = graphics->images;
    gSprites[0].animBeginning = FALSE; gSprites[0].animPaused = TRUE;
    ScriptContext_Init(); UnlockPlayerFieldControls(); gMain.callback2 = CB2_Overworld;
    sWildActorTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 0));
    CHECK(sWildActorTest); struct RhhWildActorSpawnRequest request = {.struct_size = sizeof(request),
        .map = RhhContentKeyForId(RHH_MAP, ((u8)player->mapGroup << 8) | player->mapNum),
        .x = layout.width / 2 + 3, .y = layout.height / 2 + 3};
    if (useRoamer)
    {
        bool32 ready = FALSE; for (u32 i = 0; i < 1000; ++i)
        { rng_value_t previous = gRngValue; if (Random() % 4 == 0) { gRngValue = previous; ready = TRUE; break; } }
        CHECK(ready); gEncounteredRoamerIndex = ROAMER_COUNT + 1;
    }
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_POOCHYENA, 7, 54321, OTID_STRUCT_PRESET(12345), 19);
    if (useRoamer)
    {
        bool32 ready = FALSE; for (u32 i = 0; i < 1000; ++i)
        { rng_value_t previous = gRngValue; if (Random() % 4 == 0) { gRngValue = previous; ready = TRUE; break; } }
        CHECK(ready);
    }
    struct Pokemon before[PARTY_SIZE]; memcpy(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before));
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor), .handle = 999}, unchanged = actor;
    sWildGateCheckRng = FALSE;
    CHECK(sWildActorTest->spawn(&request, &actor) == RHH_WILD_ACTOR_DENIED
        && !memcmp(&actor, &unchanged, sizeof(actor)) && !sWildActorSpawned && sSpecialCancelCalls == 1
        && sLastWildCancellation.candidate.source == (useRoamer ? RHH_WILD_OVERWORLD_ROAMER : RHH_WILD_OVERWORLD_OUTBREAK)
        && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)) && !sEncounterStartCount);
    if (useRoamer) CHECK(gEncounteredRoamerIndex == ROAMER_COUNT + 1
        && !memcmp(&original, &gSaveBlock1Ptr->roamer[0], sizeof(original)));
    sWildGateDeny = FALSE;
    CHECK(sWildActorTest->spawn(&request, &actor) == RHH_WILD_ACTOR_OK && sSpecialGeneratedCalls == 1
        && sWildActorSpawned == 1 && actor.species == RhhContentKeyForId(RHH_SPECIES, useRoamer ? SPECIES_LATIAS : SPECIES_SEEDOT)
        && !sEncounterStartCount);
    if (useRoamer)
    {
        /* A duplicate roamer is skipped; vetoing the fallback must not expose
         * its temporary enemy mon or replace the existing visible actor. */
        bool32 ready = FALSE; for (u32 i = 0; i < 1000; ++i)
        { rng_value_t previous = gRngValue; if (Random() % 4 == 0) { gRngValue = previous; ready = TRUE; break; } }
        CHECK(ready); gEncounteredRoamerIndex = ROAMER_COUNT + 1;
        CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_POOCHYENA, 7, 54321, OTID_STRUCT_PRESET(12345), 19);
        memcpy(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before));
        /* Re-select the roamer after creating the sentinel, which may draw RNG. */
        ready = FALSE; for (u32 i = 0; i < 1000; ++i)
        { rng_value_t previous = gRngValue; if (Random() % 4 == 0) { gRngValue = previous; ready = TRUE; break; } }
        CHECK(ready); sWildGateDeny = TRUE; request.x++;
        struct RhhWildActorSnapshot rejected = {.struct_size = sizeof(rejected), .handle = 999};
        CHECK(sWildActorTest->spawn(&request, &rejected) == RHH_WILD_ACTOR_DENIED
            && rejected.handle == 999 && sWildActorSpawned == 1
            && sLastWildCancellation.candidate.source == RHH_WILD_OVERWORLD_ACTOR
            && gEncounteredRoamerIndex == ROAMER_COUNT + 1
            && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)));
        sWildGateDeny = FALSE;
    }
    const u8 localId[] = {actor.local_id, 0}; struct ScriptContext script = {.scriptPtr = localId};
    StartWildBattleWithOWE(&script);
    CHECK(sEncounterStartCount == 1 && sSpecialGeneratedCalls == 1
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == (useRoamer ? SPECIES_LATIAS : SPECIES_SEEDOT));
    if (useRoamer) CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_PERSONALITY) == original.personality);
    else CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_MOVE1) == MOVE_TACKLE);
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    puts(useRoamer ? "special wild: overworld roamer rollback, duplicate fallback and encounter passed"
                   : "special wild: overworld outbreak rollback and encounter passed");
}
#if WE_OWE_FEEBAS_SPOTS
static void CheckSpecialOverworldFeebas(void)
{
    SetupCancellationFixture(NULL); sTrackEncounterStarts = TRUE; sEncounterStartCount = 0;
    free(gBackupMapLayout.map); gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE119)][MAP_NUM(MAP_ROUTE119)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE119); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE119);
    const struct MapLayout *layout = gMapHeader.mapLayout;
    gBackupMapLayout.width = layout->width + MAP_OFFSET_W; gBackupMapLayout.height = layout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    for (s32 y = 0; y < layout->height; ++y) for (s32 x = 0; x < layout->width; ++x)
        gBackupMapLayout.map[(y + MAP_OFFSET) * gBackupMapLayout.width + x + MAP_OFFSET] = layout->map[y * layout->width + x];
    ResetTasks(); ResetSpriteData(); FreeAllSpritePalettes(); StartWeather();
    memset(gObjectEvents, 0, sizeof(gObjectEvents)); gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
    gPlayerAvatar.flags = PLAYER_AVATAR_FLAG_SURFING;
    struct ObjectEvent *player = &gObjectEvents[0]; player->active = TRUE; player->isPlayer = TRUE;
    player->mapGroup = MAP_GROUP(MAP_ROUTE119); player->mapNum = MAP_NUM(MAP_ROUTE119);
    player->graphicsId = OBJ_EVENT_GFX_BRENDAN_NORMAL; player->spriteId = 0; player->currentElevation = 3;
    gSprites[0].inUse = TRUE; const struct ObjectEventGraphicsInfo *graphics = GetObjectEventGraphicsInfo(OBJ_EVENT_GFX_BRENDAN_NORMAL);
    gSprites[0].anims = graphics->anims; gSprites[0].images = graphics->images;
    gSprites[0].animBeginning = FALSE; gSprites[0].animPaused = TRUE;
    ScriptContext_Init(); UnlockPlayerFieldControls(); gMain.callback2 = CB2_Overworld;
    sWildActorTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 0));
    CHECK(sWildActorTest);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_POOCHYENA, 7, 54321, OTID_STRUCT_PRESET(12345), 19);
    struct Pokemon before[PARTY_SIZE]; memcpy(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before));
    bool32 found = FALSE;
    for (s32 y = 3; y < layout->height && !found; ++y)
        for (s32 x = 3; x < layout->width && !found; ++x)
        {
            s32 gridX = x + MAP_OFFSET, gridY = y + MAP_OFFSET;
            u32 behavior = MapGridGetMetatileBehaviorAt(gridX, gridY), elevation = MapGridGetElevationAt(gridX, gridY);
            if (!MetatileBehavior_IsWaterWildEncounter(behavior) || !MetatileBehavior_IsSurfableAndNotWaterfall(behavior)
                || elevation == ELEVATION_TRANSITION || elevation == ELEVATION_MULTI_LEVEL || MapGridGetCollisionAt(gridX, gridY)) continue;
            for (u32 attempt = 0; attempt < 8; ++attempt)
            {
                rng_value_t replay = gRngValue;
                if (!CheckFeebasAtCoords(gridX, gridY)) continue;
                gRngValue = replay; player->currentCoords.x = player->previousCoords.x = x - 3 + MAP_OFFSET;
                player->currentCoords.y = player->previousCoords.y = y - 3 + MAP_OFFSET;
                struct RhhWildActorSpawnRequest request = {.struct_size = sizeof(request),
                    .map = RhhContentKeyForId(RHH_MAP, (MAP_GROUP(MAP_ROUTE119) << 8) | MAP_NUM(MAP_ROUTE119)), .x = x, .y = y};
                struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor), .handle = 999};
                sWildGateExpectedRng = gRngValue; sWildGateExpectedRng2 = gRng2Value; sWildGateCheckRng = TRUE;
                CHECK(sWildActorTest->spawn(&request, &actor) == RHH_WILD_ACTOR_DENIED && actor.handle == 999
                    && sSpecialCancelCalls == 1 && sLastWildCancellation.candidate.source == RHH_WILD_OVERWORLD_FEEBAS
                    && !memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)) && !sEncounterStartCount);
                sWildGateDeny = sWildGateCheckRng = FALSE;
                CHECK(sWildActorTest->spawn(&request, &actor) == RHH_WILD_ACTOR_OK && sWildActorSpawned == 1
                    && actor.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_FEEBAS)
                    && sSpecialGeneratedCalls == 1 && sLastSpecialGenerated.source == RHH_WILD_OVERWORLD_FEEBAS);
                const u8 localId[] = {actor.local_id, 0}; struct ScriptContext script = {.scriptPtr = localId};
                StartWildBattleWithOWE(&script);
                CHECK(sEncounterStartCount == 1 && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_FEEBAS);
                found = TRUE; break;
            }
        }
    CHECK(found); free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    puts("special wild: overworld Feebas rollback and encounter passed");
}
#endif
extern void DrawFrame(u16 *pixels);
static void CheckAutomaticWildActor(const char *module, bool32 requestedSpecies)
{
    InitModsForTestWithLibrary(module);
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE101)][MAP_NUM(MAP_ROUTE101)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE101); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE101);
    struct MapLayout layout = *gMapHeader.mapLayout; struct Tileset primary = *layout.primaryTileset;
    u16 attributes[1] = {MB_TALL_GRASS}; primary.metatileAttributes = attributes; layout.primaryTileset = &primary;
    CHECK(!layout.isFrlg && layout.width > 14 && layout.height > 9); gMapHeader.mapLayout = &layout;
    gBackupMapLayout.width = layout.width + MAP_OFFSET_W; gBackupMapLayout.height = layout.height + MAP_OFFSET_H;
    u32 gridSize = gBackupMapLayout.width * gBackupMapLayout.height;
    gBackupMapLayout.map = calloc(gridSize, sizeof(u16)); CHECK(gBackupMapLayout.map);
    for (u32 i = 0; i < gridSize; ++i) gBackupMapLayout.map[i] = PACK_ELEVATION(3);
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 0);
    ResetTasks(); ResetSpriteData(); FreeAllSpritePalettes(); StartWeather();
    memset(gObjectEvents, 0, sizeof(gObjectEvents));
    gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
    gObjectEvents[0].active = TRUE; gObjectEvents[0].isPlayer = TRUE; gObjectEvents[0].localId = LOCALID_PLAYER;
    gObjectEvents[0].mapGroup = MAP_GROUP(MAP_ROUTE101); gObjectEvents[0].mapNum = MAP_NUM(MAP_ROUTE101);
    gObjectEvents[0].graphicsId = OBJ_EVENT_GFX_BRENDAN_NORMAL;
    gObjectEvents[0].spriteId = 0; gObjectEvents[0].currentElevation = 3; gObjectEvents[0].previousElevation = 3;
    gObjectEvents[0].currentCoords.x = MAP_OFFSET + layout.width / 2; gObjectEvents[0].currentCoords.y = MAP_OFFSET + layout.height / 2;
    gObjectEvents[0].previousCoords = gObjectEvents[0].currentCoords; gSprites[0].inUse = TRUE;
    const struct ObjectEventGraphicsInfo *playerGraphics = GetObjectEventGraphicsInfo(OBJ_EVENT_GFX_BRENDAN_NORMAL);
    gSprites[0].anims = playerGraphics->anims; gSprites[0].images = playerGraphics->images;
    gSprites[0].animBeginning = FALSE; gSprites[0].animPaused = TRUE;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    sWildActorTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 0));
    CHECK(sWildActorTest && sWildActorTest->header.version >= RHH_SERVICE_VERSION(1, 3)
        && sWildActorTest->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_3_SIZE);
    struct RhhWildActorBattleResult noBattle = {.struct_size = sizeof(noBattle), .outcome = 999};
    CHECK(sWildActorTest->battle_result(&noBattle) && noBattle.outcome == 999);
    ModsWildActorBattleFinished(B_OUTCOME_WON); CHECK(!sWildActorBattleEnded);
    ScriptContext_Init(); UnlockPlayerFieldControls();
    gMain.callback2 = CB2_Overworld; gPlayerAvatar.flags = PLAYER_AVATAR_FLAG_ON_FOOT;
    rng_value_t routeRng = gRngValue, routeRng2 = gRng2Value;
    CHECK(NativeOWEReplacesRandom(MB_TALL_GRASS) && !NativeOWEReplacesRandom(MB_NORMAL));
    SetSafariZoneFlag(); CHECK(!NativeOWEReplacesRandom(MB_TALL_GRASS)); ResetSafariZoneFlag();
    for (u32 i = 0; i < gridSize; ++i) gBackupMapLayout.map[i] = PACK_ELEVATION(3) | PACK_COLLISION(1);
    CHECK(!NativeOWEReplacesRandom(MB_TALL_GRASS));
    for (u32 i = 0; i < gridSize; ++i) gBackupMapLayout.map[i] = PACK_ELEVATION(3);
    CHECK(!memcmp(&routeRng, &gRngValue, sizeof(routeRng)) && !memcmp(&routeRng2, &gRng2Value, sizeof(routeRng2)));
    sRejectWildActorCommands = TRUE;
    const struct RhhWildActorService *capacityService = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 11));
    struct RhhWildActorCapacity capacity = {.struct_size = sizeof(capacity)}, tinyCapacity = {.struct_size = 1, .object_count = 99};
    struct RhhWildActorCapacity savedTinyCapacity = tinyCapacity;
    rng_value_t capacityRng = gRngValue, capacityRng2 = gRng2Value;
    CHECK(capacityService && capacityService->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_11_SIZE
        && capacityService->capacity(&tinyCapacity) && !memcmp(&tinyCapacity, &savedTinyCapacity, sizeof(tinyCapacity))
        && !capacityService->capacity(&capacity) && capacity.automatic_enabled && capacity.commands_allowed
        && capacity.generated_limit == OWE_SPAWNS_MAX && !capacity.generated_count && !capacity.manual_count
        && capacity.next_generated_slot == 0 && capacity.object_limit == OBJECT_EVENTS_COUNT
        && capacity.object_count == 1 && capacity.sprite_limit == MAX_SPRITES && capacity.sprite_count == 1
        && capacity.map == RhhContentKeyForId(RHH_MAP, MAP_ROUTE101)
        && !memcmp(&capacityRng, &gRngValue, sizeof(capacityRng)) && !memcmp(&capacityRng2, &gRng2Value, sizeof(capacityRng2)));
    for (u32 i = 0; i < OWE_SPAWNS_MAX; ++i) {
        gObjectEvents[i + 1] = (struct ObjectEvent){.active = TRUE, .trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER,
            .localId = LOCALID_OW_ENCOUNTER_END - i, .graphicsId = OBJ_EVENT_GFX_SPECIES(TREECKO)};
        gSprites[i + 1].inUse = TRUE;
    }
    gObjectEvents[OWE_SPAWNS_MAX + 1] = (struct ObjectEvent){.active = TRUE,
        .trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER, .localId = 5, .graphicsId = OBJ_EVENT_GFX_SPECIES(TREECKO)};
    CHECK(!capacityService->capacity(&capacity) && capacity.generated_count == OWE_SPAWNS_MAX
        && capacity.manual_count == 1 && capacity.next_generated_slot == UINT32_MAX
        && capacity.object_count == OWE_SPAWNS_MAX + 2
        && capacity.sprite_count == OWE_SPAWNS_MAX + 1);
    struct RhhWildActorSpawnRequest fullRequest = {.struct_size = sizeof(fullRequest), .map = capacity.map,
        .x = layout.width / 2 + 2, .y = layout.height / 2 + 2};
    const struct RhhWildActorService *preflightService = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 12));
    struct RhhWildActorSpawnPreflight preflight = {.struct_size = sizeof(preflight)},
        tinyPreflight = {.struct_size = 1, .status = 99}, savedTinyPreflight = tinyPreflight;
    CHECK(preflightService && preflightService->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_12_SIZE
        && RHH_WILD_ACTOR_SERVICE_V1_11_SIZE < RHH_WILD_ACTOR_SERVICE_V1_12_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 19))
        && preflightService->spawn_preflight(&fullRequest, &tinyPreflight)
        && !memcmp(&tinyPreflight, &savedTinyPreflight, sizeof(tinyPreflight))
        && !preflightService->spawn_preflight(&fullRequest, &preflight)
        && preflight.status == RHH_WILD_ACTOR_FULL && preflight.generated_slot == UINT32_MAX);
    struct RhhWildActorSnapshot fullResult = {.struct_size = sizeof(fullResult), .handle = 99}, savedFullResult = fullResult;
    CHECK(capacityService->spawn(&fullRequest, &fullResult) == RHH_WILD_ACTOR_FULL
        && !memcmp(&fullResult, &savedFullResult, sizeof(fullResult))
        && !memcmp(&capacityRng, &gRngValue, sizeof(capacityRng)) && !memcmp(&capacityRng2, &gRng2Value, sizeof(capacityRng2)));
    memset(&gObjectEvents[OWE_SPAWNS_MAX + 1], 0, sizeof(gObjectEvents[OWE_SPAWNS_MAX + 1]));
    for (u32 i = 0; i < OWE_SPAWNS_MAX; ++i) {
        memset(&gObjectEvents[i + 1], 0, sizeof(gObjectEvents[i + 1])); gSprites[i + 1].inUse = FALSE;
    }
    struct RhhWildActorSpawnRequest request = {.struct_size = sizeof(request),
        .map = RhhContentKeyForId(RHH_MAP, (MAP_GROUP(MAP_ROUTE101) << 8) | MAP_NUM(MAP_ROUTE101)),
        .x = layout.width / 2 + 2, .y = layout.height / 2 + 2};
    CHECK(!preflightService->spawn_preflight(&request, &preflight)
        && preflight.status == RHH_WILD_ACTOR_OK && preflight.area == RHH_WILD_LAND && preflight.generated_slot == 0
        && !memcmp(&capacityRng, &gRngValue, sizeof(capacityRng)) && !memcmp(&capacityRng2, &gRng2Value, sizeof(capacityRng2)));
    const struct RhhWildActorService *speciesPreflightService = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 13));
    struct RhhWildActorSpeciesSpawnRequest preflightSpeciesRequest = {.struct_size = sizeof(preflightSpeciesRequest),
        .map = request.map, .x = request.x, .y = request.y,
        .species = RhhContentKeyForId(RHH_SPECIES, SPECIES_TREECKO), .level = 3};
    struct RhhWildActorSpeciesPreflight speciesCheck = {.struct_size = sizeof(speciesCheck)},
        tinySpeciesCheck = {.struct_size = 1, .flags = 99}, savedTinySpeciesCheck = tinySpeciesCheck;
    CHECK(speciesPreflightService && speciesPreflightService->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_13_SIZE
        && RHH_WILD_ACTOR_SERVICE_V1_12_SIZE < RHH_WILD_ACTOR_SERVICE_V1_13_SIZE
        && speciesPreflightService->species_spawn_preflight(&preflightSpeciesRequest, &tinySpeciesCheck)
        && !memcmp(&tinySpeciesCheck, &savedTinySpeciesCheck, sizeof(tinySpeciesCheck))
        && !speciesPreflightService->species_spawn_preflight(&preflightSpeciesRequest, &speciesCheck)
        && speciesCheck.status == RHH_WILD_ACTOR_OK && speciesCheck.area == RHH_WILD_LAND
        && speciesCheck.generated_slot == 0 && !speciesCheck.flags);
    /* Restoration can place a preserved Pokémon without announcing a partial
     * actor or changing the enemy party. */
    struct Pokemon restoredMon; CreateMonWithIVs(&restoredMon, SPECIES_TREECKO, 17, 76543, OTID_STRUCT_PRESET(23456), 23);
    struct Pokemon enemyBefore[PARTY_SIZE]; memcpy(enemyBefore, gParties[B_TRAINER_OPPONENT_A], sizeof(enemyBefore));
    rng_value_t preservedRng = gRngValue, preservedRng2 = gRng2Value;
    u32 restoredSlot = UINT32_MAX, previousSpawned = sWildActorSpawned;
    CHECK(TrySpawnModOverworldWildPreserved(request.x, request.y, &restoredMon, DIR_EAST, 3, TRUE, &restoredSlot) == RHH_WILD_ACTOR_OK
        && restoredSlot < OBJECT_EVENTS_COUNT && gObjectEvents[restoredSlot].facingDirection == DIR_EAST
        && (gObjectEvents[restoredSlot].trainerRange_berryTreeId & 0x80)
        && sWildActorSpawned == previousSpawned && !memcmp(enemyBefore, gParties[B_TRAINER_OPPONENT_A], sizeof(enemyBefore))
        && !memcmp(&preservedRng, &gRngValue, sizeof(preservedRng))
        && !memcmp(&preservedRng2, &gRng2Value, sizeof(preservedRng2)));
    struct RhhWildActorSnapshot partial = {.struct_size = sizeof(partial), .handle = 999}, partialBefore = partial;
    CHECK(sWildActorTest->at(restoredSlot, &partial) && !memcmp(&partial, &partialBefore, sizeof(partial)));
    RemoveObjectEvent(&gObjectEvents[restoredSlot]);
    CHECK(TrySpawnModOverworldWildPreserved(request.x, request.y, &restoredMon, 0, 3, FALSE, &restoredSlot) == RHH_WILD_ACTOR_INVALID);
    /* A restored actor may be on an ordinary tile next to the player, but
     * occupied tiles and changed elevations must not replace another object. */
    s32 nearX = layout.width / 2 + 1, nearY = layout.height / 2;
    attributes[0] = MB_NORMAL;
    CHECK(TrySpawnModOverworldWildPreserved(nearX, nearY, &restoredMon, DIR_WEST, 2, FALSE, &restoredSlot) == RHH_WILD_ACTOR_BLOCKED
        && TrySpawnModOverworldWildPreserved(layout.width / 2, nearY, &restoredMon, DIR_WEST, 3, FALSE, &restoredSlot) == RHH_WILD_ACTOR_BLOCKED
        && TrySpawnModOverworldWildPreserved(nearX, nearY, &restoredMon, DIR_WEST, 3, FALSE, &restoredSlot) == RHH_WILD_ACTOR_OK
        && gObjectEvents[restoredSlot].facingDirection == DIR_WEST && sWildActorSpawned == previousSpawned);
    RemoveObjectEvent(&gObjectEvents[restoredSlot]);
    attributes[0] = MB_TALL_GRASS;
    struct Pokemon savedLead = gParties[B_TRAINER_PLAYER][0];
    enum Ability savedLastAbility = gLastUsedAbility;
    u16 savedRepel = VarGet(VAR_REPEL_STEP_COUNT);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_MIGHTYENA, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    u8 abilitySlot = 0; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ABILITY_NUM, &abilitySlot);
    CHECK(GetMonAbility(&gParties[B_TRAINER_PLAYER][0]) == ABILITY_INTIMIDATE);
    VarSet(VAR_REPEL_STEP_COUNT, 50);
    rng_value_t speciesRng = gRngValue, speciesRng2 = gRng2Value;
    gLastUsedAbility = ABILITY_BLAZE;
    CHECK(!speciesPreflightService->species_spawn_preflight(&preflightSpeciesRequest, &speciesCheck)
        && speciesCheck.status == RHH_WILD_ACTOR_OK
        && speciesCheck.flags == (RHH_WILD_SPECIES_REPEL_BLOCKS | RHH_WILD_SPECIES_ABILITY_ROLL)
        && gLastUsedAbility == ABILITY_BLAZE
        && !memcmp(&speciesRng, &gRngValue, sizeof(speciesRng))
        && !memcmp(&speciesRng2, &gRng2Value, sizeof(speciesRng2)));
    bool32 expectedAbilityResult = Random() % 2;
    rng_value_t rngAfterAbilityRoll = gRngValue;
    gRngValue = speciesRng;
    CHECK(IsAbilityAllowingEncounter(3) == expectedAbilityResult
        && !memcmp(&rngAfterAbilityRoll, &gRngValue, sizeof(rngAfterAbilityRoll))
        && !memcmp(&speciesRng2, &gRng2Value, sizeof(speciesRng2)));
    gRngValue = speciesRng;
    preflightSpeciesRequest.level = 20;
    CHECK(!speciesPreflightService->species_spawn_preflight(&preflightSpeciesRequest, &speciesCheck) && !speciesCheck.flags);
    CHECK(IsAbilityAllowingEncounter(20) && !memcmp(&speciesRng, &gRngValue, sizeof(speciesRng)));
    preflightSpeciesRequest.level = 0;
    CHECK(!speciesPreflightService->species_spawn_preflight(&preflightSpeciesRequest, &speciesCheck)
        && speciesCheck.status == RHH_WILD_ACTOR_INVALID && speciesCheck.generated_slot == UINT32_MAX);
    preflightSpeciesRequest.level = 3; preflightSpeciesRequest.reserved[0] = 1;
    struct RhhWildActorSpeciesPreflight savedSpeciesCheck = speciesCheck;
    CHECK(speciesPreflightService->species_spawn_preflight(&preflightSpeciesRequest, &speciesCheck)
        && !memcmp(&speciesCheck, &savedSpeciesCheck, sizeof(speciesCheck)));
    gParties[B_TRAINER_PLAYER][0] = savedLead; VarSet(VAR_REPEL_STEP_COUNT, savedRepel);
    gLastUsedAbility = savedLastAbility;
    gRngValue = capacityRng; gRng2Value = capacityRng2;
    request.x = -1; CHECK(!preflightService->spawn_preflight(&request, &preflight)
        && preflight.status == RHH_WILD_ACTOR_INVALID && preflight.generated_slot == UINT32_MAX); request.x = layout.width / 2;
    CHECK(!preflightService->spawn_preflight(&request, &preflight) && preflight.status == RHH_WILD_ACTOR_BLOCKED);
    request.x += 2; request.reserved = 1; struct RhhWildActorSpawnPreflight savedPreflight = preflight;
    CHECK(preflightService->spawn_preflight(&request, &preflight) && !memcmp(&preflight, &savedPreflight, sizeof(preflight)));
    request.reserved = 0;
    struct RhhWildActorSnapshot created = {.struct_size = sizeof(created), .handle = 999}, unchanged = created;
    request.reserved = 1; CHECK(sWildActorTest->spawn(&request, &created) == RHH_WILD_ACTOR_INVALID
        && !memcmp(&created, &unchanged, sizeof(created))); request.reserved = 0;
    request.x = layout.width / 2; int blockedStatus = sWildActorTest->spawn(&request, &created);
    if (blockedStatus != RHH_WILD_ACTOR_BLOCKED)
        fprintf(stderr, "wild actor blocked-tile status=%d, world-ready=%d script=%d locked=%d fade=%d/%d\n",
            blockedStatus, !!layout.secondaryTileset && !!layout.secondaryTileset->metatileAttributes,
            ScriptContext_IsEnabled(), ArePlayerFieldControlsLocked(), gPaletteFade.active, gPaletteFade.y);
    CHECK(blockedStatus == RHH_WILD_ACTOR_BLOCKED
        && !memcmp(&created, &unchanged, sizeof(created))); request.x += 2;
    const struct RhhWildActorService *speciesActors = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 8));
    CHECK(speciesActors && speciesActors->header.struct_size >= RHH_WILD_ACTOR_SERVICE_V1_8_SIZE);
    struct RhhWildActorSpeciesSpawnRequest speciesRequest = {.struct_size = sizeof(speciesRequest),
        .map = request.map, .x = request.x, .y = request.y,
        .species = RhhContentKeyForId(RHH_SPECIES, SPECIES_TREECKO), .level = 17};
    struct RhhWildActorSnapshot specified = {.struct_size = sizeof(specified), .handle = 999}, specifiedBefore = specified;
    speciesRequest.level = 0;
    CHECK(speciesActors->spawn_species(&speciesRequest, &specified) == RHH_WILD_ACTOR_INVALID
        && !memcmp(&specified, &specifiedBefore, sizeof(specified)));
    speciesRequest.level = 17; speciesRequest.reserved[0] = 1;
    CHECK(speciesActors->spawn_species(&speciesRequest, &specified) == RHH_WILD_ACTOR_INVALID);
    speciesRequest.reserved[0] = 0;
    sWildActorSpawned = sWildActorRemoved = sWildCancelCalls = 0;
    sWildGateService = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 2));
    CHECK(sWildGateService); sWildGateDeny = sWildGateTryUndeny = TRUE; sWildGateCheckRng = FALSE;
    struct Pokemon specifiedEnemy[PARTY_SIZE]; memcpy(specifiedEnemy, gParties[B_TRAINER_OPPONENT_A], sizeof(specifiedEnemy));
    rng_value_t specifiedRng = gRngValue, specifiedRng2 = gRng2Value;
    CHECK(speciesActors->spawn_species(&speciesRequest, &specified) == RHH_WILD_ACTOR_DENIED
        && !memcmp(&specified, &specifiedBefore, sizeof(specified)) && !sWildActorSpawned && sWildCancelCalls == 1
        && sLastWildCancellation.candidate.source == RHH_WILD_OVERWORLD_ACTOR
        && sLastWildCancellation.candidate.slot == RHH_WILD_NO_TABLE_SLOT
        && sLastWildCancellation.candidate.table_species == speciesRequest.species
        && sLastWildCancellation.candidate.level == speciesRequest.level
        && !memcmp(specifiedEnemy, gParties[B_TRAINER_OPPONENT_A], sizeof(specifiedEnemy))
        && !memcmp(&specifiedRng, &gRngValue, sizeof(specifiedRng))
        && !memcmp(&specifiedRng2, &gRng2Value, sizeof(specifiedRng2)));
    sWildGateDeny = sWildGateTryUndeny = FALSE;
    CHECK(speciesActors->spawn_species(&speciesRequest, &specified) == RHH_WILD_ACTOR_OK
        && specified.species == speciesRequest.species && specified.level == speciesRequest.level
        && (specified.flags & RHH_WILD_ACTOR_PRESERVED) && sWildActorSpawned == 1);
    CHECK(!capacityService->capacity(&capacity) && capacity.generated_count == 1 && capacity.next_generated_slot == 1
        && capacity.object_count == 2 && capacity.sprite_count >= 2);
    struct Pokemon specifiedMon;
    CHECK(ModsWildActorCopyMon(&gObjectEvents[specified.slot], &specifiedMon)
        && GetMonData(&specifiedMon, MON_DATA_SPECIES) == SPECIES_TREECKO
        && GetMonData(&specifiedMon, MON_DATA_LEVEL) == speciesRequest.level);
    CHECK(speciesActors->remove(specified.handle) == RHH_WILD_ACTOR_OK);
    CHECK(!capacityService->capacity(&capacity) && !capacity.generated_count && capacity.next_generated_slot == 0
        && capacity.object_count == 1);
    sReplaceRequestedWild = TRUE;
    CHECK(speciesActors->spawn_species(&speciesRequest, &specified) == RHH_WILD_ACTOR_OK
        && specified.species == RhhContentKeyForId(RHH_SPECIES, SPECIES_ZIGZAGOON)
        && specified.level == speciesRequest.level
        && ModsWildActorCopyMon(&gObjectEvents[specified.slot], &specifiedMon)
        && GetMonData(&specifiedMon, MON_DATA_SPECIES) == SPECIES_ZIGZAGOON);
    sReplaceRequestedWild = FALSE;
    CHECK(speciesActors->remove(specified.handle) == RHH_WILD_ACTOR_OK);
    sWildGateService = NULL; sWildCancelCalls = 0;
    sWildActorSpawned = sWildActorRemoved = 0; sCaptureAutomaticWild = TRUE;
    sWildGateService = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 2));
    CHECK(sWildGateService); sWildGateDeny = sWildGateTryUndeny = TRUE; sWildGateCheckRng = FALSE;
    struct Pokemon beforeDeniedParty[PARTY_SIZE];
    memcpy(beforeDeniedParty, gParties[B_TRAINER_OPPONENT_A], sizeof(beforeDeniedParty));
    u8 beforeDeniedCount = gPartiesCount[B_TRAINER_OPPONENT_A]; struct RhhWildActorSnapshot beforeDeniedOutput = created;
    CHECK(sWildActorTest->spawn(&request, &created) == RHH_WILD_ACTOR_DENIED
        && !memcmp(&created, &beforeDeniedOutput, sizeof(created)) && !sWildActorSpawned && sWildCancelCalls == 1
        && sLastWildCancellation.candidate.source == RHH_WILD_OVERWORLD_ACTOR
        && beforeDeniedCount == gPartiesCount[B_TRAINER_OPPONENT_A]
        && !memcmp(beforeDeniedParty, gParties[B_TRAINER_OPPONENT_A], sizeof(beforeDeniedParty)));
    sWildGateService = NULL; sWildGateDeny = sWildGateTryUndeny = FALSE;
    CHECK(sWildActorTest->spawn(&request, &created) == RHH_WILD_ACTOR_OK
        && created.x == request.x && created.y == request.y && (created.flags & RHH_WILD_ACTOR_PRESERVED)
        && sWildActorSpawned == 1);
    u64 commandHandle = created.handle;
    struct ObjectEvent *moving = &gObjectEvents[created.slot];
    if (module)
    {
        RhhModsSetPersistence(IndividualTestPersist); RhhModsSetRunContext(123, 456, 1);
        ModsWildActorsUpdate();
        struct RhhWildActorIndividual individual = {.struct_size = sizeof(individual)};
        CHECK(!sWildActorTest->individual(commandHandle, &individual) && individual.serial);
        s32 playerX = gObjectEvents[0].currentCoords.x - MAP_OFFSET;
        s32 playerY = gObjectEvents[0].currentCoords.y - MAP_OFFSET;
        ModsWorldStep(playerX, playerY, 3, MB_TALL_GRASS);
        CHECK(!moving->heldMovementActive); /* Decisions cannot start movement inside field input. */
        ModsWildActorsUpdate(); CHECK(moving->heldMovementActive);
        struct RhhWildStepResult behaviorStep = {.struct_size = sizeof(behaviorStep)};
        CHECK(!sWildActorTest->step_pending(commandHandle, &behaviorStep) && behaviorStep.ticket
            && behaviorStep.status == RHH_WILD_STEP_PENDING && behaviorStep.source == RHH_WILD_STEP_BEHAVIOR
            && behaviorStep.individual.serial == individual.serial && !behaviorStep.queue_consumed);
        ModsWorldStep(playerX, playerY, 3, MB_TALL_GRASS); ModsWildActorsUpdate(); /* busy */
        ModsWorldStep(playerX, playerY, 3, MB_TALL_GRASS); ModsWildActorsUpdate(); /* denied */
        ModsWorldStep(playerX, playerY, 3, MB_TALL_GRASS); ModsWildActorsUpdate(); /* invalid */
        for (u32 i = 0; i < 100 && moving->heldMovementActive; ++i) { AnimateSprites(); ModsWildActorsUpdate(); }
        struct RhhWildActorSnapshot after = {.struct_size = sizeof(after)};
        CHECK(!moving->heldMovementActive && !sWildActorTest->find(commandHandle, &after) && after.x == created.x - 1);
        ModsWorldStep(playerX, playerY, 3, MB_TALL_GRASS); ModsWildActorsUpdate();
        for (u32 i = 0; i < 100 && moving->heldMovementActive; ++i) { AnimateSprites(); ModsWildActorsUpdate(); }
        CHECK(!moving->heldMovementActive && !sWildActorTest->find(commandHandle, &after) && after.x == created.x);
        ModsWorldStep(playerX, playerY, 3, MB_TALL_GRASS); ModsWildActorsUpdate(); /* not adjacent */
        ModsWorldStep(playerX, playerY, 3, MB_TALL_GRASS);
        ScriptContext_Enable(); ModsWildActorsUpdate(); CHECK(!moving->heldMovementActive);
        ScriptContext_Init(); UnlockPlayerFieldControls();
        const struct RhhWildBehaviorService *queueBehavior = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 4)); CHECK(queueBehavior);
        struct RhhWildQueueRequest queued = {.struct_size = sizeof(queued), .actor_handle = commandHandle,
            .owner = RhhContentKeyOf("test:mon_edit_rules"), .count = 1, .actions = {RHH_WILD_BEHAVIOR_WEST}};
        struct RhhWildQueueChange queueChange = {.struct_size = sizeof(queueChange)};
        struct RhhWildQueue queuedState = {.struct_size = sizeof(queuedState)};
        CHECK(!queueBehavior->queue_replace(&queued, &queueChange));
        ModsWorldStep(playerX, playerY, 3, MB_TALL_GRASS); ModsWildActorsUpdate(); /* overridden */
        CHECK(!moving->heldMovementActive && !queueBehavior->queue_at(commandHandle, &queuedState)
            && queuedState.count == 1 && queuedState.revision == 1);
        ModsWorldStep(playerX, playerY, 3, MB_TALL_GRASS); ModsWildActorsUpdate(); /* denied */
        CHECK(!moving->heldMovementActive && !queueBehavior->queue_at(commandHandle, &queuedState)
            && queuedState.count == 1 && queuedState.revision == 1);
        ModsWorldStep(playerX, playerY, 3, MB_TALL_GRASS); ModsWildActorsUpdate(); /* accepted */
        CHECK(moving->heldMovementActive && !queueBehavior->queue_at(commandHandle, &queuedState)
            && !queuedState.count && queuedState.revision == 2);
        struct RhhWildStepResult queuedStep = {.struct_size = sizeof(queuedStep)};
        CHECK(!sWildActorTest->step_pending(commandHandle, &queuedStep) && queuedStep.source == RHH_WILD_STEP_QUEUE
            && queuedStep.queue_consumed && queuedStep.queue_revision == 1 && queuedStep.queue_owner == queued.owner);
        for (u32 i = 0; i < 100 && moving->heldMovementActive; ++i) { AnimateSprites(); ModsWildActorsUpdate(); }
        CHECK(sLastWildStep.ticket == queuedStep.ticket && sLastWildStep.source == RHH_WILD_STEP_QUEUE
            && sLastWildStep.queue_consumed && sLastWildStep.queue_revision == 1);
        CHECK(!moving->heldMovementActive && sWildActorTest->step(commandHandle, RHH_WILD_ACTOR_EAST) == RHH_WILD_ACTOR_OK);
        for (u32 i = 0; i < 100 && moving->heldMovementActive; ++i) { AnimateSprites(); ModsWildActorsUpdate(); }
        CHECK(!moving->heldMovementActive);
        RhhModsSetPersistence(NULL); RhhModsSetRunContext(123, 456, 0);
        free(sIndividualJournal); sIndividualJournal = NULL; sIndividualJournalSize = 0;
    }
    else if (!requestedSpecies)
    {
        RhhModsSetPersistence(IndividualTestPersist); RhhModsSetRunContext(123, 456, 1); ModsWildActorsUpdate();
        const struct RhhWildBehaviorService *behavior = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 4)); CHECK(behavior);
        struct RhhWildQueueRequest queueRequest = {.struct_size = sizeof(queueRequest), .actor_handle = commandHandle,
            .owner = RhhContentKeyOf("test:mon_edit_rules"), .count = 2,
            .actions = {RHH_WILD_BEHAVIOR_WEST, RHH_WILD_BEHAVIOR_EAST}};
        struct RhhWildQueueChange change = {.struct_size = sizeof(change)};
        struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
        CHECK(!behavior->queue_replace(&queueRequest, &change) && change.after.revision == 1);
        struct Coords16 oldPlayer = gObjectEvents[0].currentCoords;
        gObjectEvents[0].currentCoords = moving->currentCoords; --gObjectEvents[0].currentCoords.x;
        gObjectEvents[0].previousCoords = gObjectEvents[0].currentCoords;
        ModsWorldStep(oldPlayer.x - MAP_OFFSET, oldPlayer.y - MAP_OFFSET, 3, MB_TALL_GRASS); ModsWildActorsUpdate();
        CHECK(!moving->heldMovementActive && !behavior->queue_at(commandHandle, &queue) && queue.count == 2 && queue.revision == 1);
        gObjectEvents[0].currentCoords = gObjectEvents[0].previousCoords = oldPlayer;
        ModsWorldStep(oldPlayer.x - MAP_OFFSET, oldPlayer.y - MAP_OFFSET, 3, MB_TALL_GRASS); ModsWildActorsUpdate();
        CHECK(moving->heldMovementActive && !behavior->queue_at(commandHandle, &queue) && queue.count == 1
            && queue.revision == 2 && queue.actions[0] == RHH_WILD_BEHAVIOR_EAST);
        ModsWorldStep(oldPlayer.x - MAP_OFFSET, oldPlayer.y - MAP_OFFSET, 3, MB_TALL_GRASS); ModsWildActorsUpdate();
        CHECK(!behavior->queue_at(commandHandle, &queue) && queue.count == 1 && queue.revision == 2);
        for (u32 i = 0; i < 100 && moving->heldMovementActive; ++i) { AnimateSprites(); ModsWildActorsUpdate(); }
        CHECK(!moving->heldMovementActive);
        ModsWorldStep(oldPlayer.x - MAP_OFFSET, oldPlayer.y - MAP_OFFSET, 3, MB_TALL_GRASS); ModsWildActorsUpdate();
        CHECK(moving->heldMovementActive && !behavior->queue_at(commandHandle, &queue) && !queue.count && queue.revision == 3);
        for (u32 i = 0; i < 100 && moving->heldMovementActive; ++i) { AnimateSprites(); ModsWildActorsUpdate(); }
        struct RhhWildActorSnapshot afterQueue = {.struct_size = sizeof(afterQueue)};
        CHECK(!moving->heldMovementActive && !sWildActorTest->find(commandHandle, &afterQueue)
            && afterQueue.x == created.x && afterQueue.y == created.y);
        RhhModsSetPersistence(NULL); RhhModsSetRunContext(123, 456, 0);
        free(sIndividualJournal); sIndividualJournal = NULL; sIndividualJournalSize = 0;
    }
    const struct RhhPerceptionService *perception = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/perception", RHH_SERVICE_VERSION(1, 0)); CHECK(perception);
    struct RhhPerceptionRequest perceptionRequest = {.struct_size = sizeof(perceptionRequest), .max_distance = 8,
        .viewer = {RHH_PERCEPTION_PLAYER, 0, 0}, .target = {RHH_PERCEPTION_WILD, 0, commandHandle}};
    struct RhhPerceptionResult seen = {.struct_size = sizeof(seen)};
    CHECK(!perception->query(&perceptionRequest, &seen) && (seen.flags & RHH_PERCEPTION_PERCEIVABLE)
        && seen.target_wild.handle == commandHandle && seen.target_cover & RHH_WILD_COVER_TALL_GRASS);
    gSprites[moving->spriteId].x = 120; gSprites[moving->spriteId].y = 80;
    gSprites[moving->spriteId].coordOffsetEnabled = FALSE;
    sWildConcealTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_concealment", RHH_SERVICE_VERSION(1, 0));
    CHECK(sWildConcealTest);
    struct RhhWildConcealment policy = {.struct_size = sizeof(policy)};
    struct RhhWildConcealResult conceal = {.struct_size = sizeof(conceal)};
    struct RhhWildConcealRequest concealRequest = {.struct_size = sizeof(concealRequest), .handle = commandHandle,
        .owner = RhhContentKeyOf("test:mon_edit_rules"), .mode = RHH_WILD_CONCEAL_HIDDEN,
        .reveal_flags = RHH_WILD_REVEAL_ON_MOVE};
    rng_value_t randomBefore = gRngValue;
    CHECK(!sWildConcealTest->preview(&concealRequest, &conceal) && conceal.is_preview
        && conceal.before.mode == RHH_WILD_CONCEAL_NONE && conceal.after.mode == RHH_WILD_CONCEAL_HIDDEN
        && sConcealConstraints == 1 && !sConcealNotifications && !memcmp(&gRngValue, &randomBefore, sizeof(randomBefore))
        && !sWildConcealTest->at(commandHandle, &policy) && !policy.mode);
    sConcealDeny = TRUE; CHECK(sWildConcealTest->apply(&concealRequest, &conceal) == RHH_WILD_CONCEAL_DENIED
        && !strcmp(conceal.reason, "test denied") && !ModsWildActorHidden(moving) && !sConcealNotifications);
    sConcealDeny = FALSE; CHECK(!sWildConcealTest->apply(&concealRequest, &conceal)
        && conceal.after.revision == 1 && ModsWildActorHidden(moving) && !moving->invisible && sConcealNotifications == 1);
    const struct RhhOverworldService *visibilityWorld = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/overworld", RHH_SERVICE_VERSION(1, 7));
    struct RhhWorldActorVisibility visibility = {.struct_size = sizeof(visibility)};
    CHECK(visibilityWorld && !visibilityWorld->actor_visibility(moving - gObjectEvents, &visibility)
        && (visibility.flags & RHH_WORLD_ACTOR_CONCEALED) && !(visibility.flags & RHH_WORLD_ACTOR_OBJECT_HIDDEN));
    CHECK(!perception->query(&perceptionRequest, &seen) && (seen.flags & RHH_PERCEPTION_TARGET_HIDDEN)
        && !(seen.flags & RHH_PERCEPTION_PERCEIVABLE));
    CHECK(sWildConcealTest->apply(&concealRequest, &conceal) == RHH_WILD_CONCEAL_STALE);
    concealRequest.expected_revision = 1; concealRequest.owner = RhhContentKeyOf("test:music_catalog");
    CHECK(sWildConcealTest->apply(&concealRequest, &conceal) == RHH_WILD_CONCEAL_CONFLICT);
    concealRequest.owner = RhhContentKeyOf("test:mon_edit_rules");
    struct Coords16 savedPlayer = gObjectEvents[0].currentCoords;
    gObjectEvents[0].currentCoords.x = moving->currentCoords.x + 1;
    gObjectEvents[0].currentCoords.y = moving->currentCoords.y;
    gObjectEvents[0].previousCoords = gObjectEvents[0].currentCoords;
    CHECK(sWildActorTest->step(commandHandle, RHH_WILD_ACTOR_EAST) == RHH_WILD_ACTOR_BLOCKED
        && !ScriptContext_IsEnabled() && moving->active && !ArePlayerFieldControlsLocked());
    gObjectEvents[0].currentCoords = gObjectEvents[0].previousCoords = savedPlayer;
    CHECK(sWildActorTest->step(commandHandle, 0) == RHH_WILD_ACTOR_INVALID);
    CHECK(sWildActorTest->engage(commandHandle) == RHH_WILD_ACTOR_BLOCKED);
    CHECK(sWildActorTest->step(commandHandle, RHH_WILD_ACTOR_WEST) == RHH_WILD_ACTOR_OK
        && sWildActorTest->step(commandHandle, RHH_WILD_ACTOR_WEST) == RHH_WILD_ACTOR_BLOCKED
        && sWildActorTest->engage(commandHandle) == RHH_WILD_ACTOR_BLOCKED);
    struct RhhWildStepResult pending = {.struct_size = sizeof(pending)}, shortStep = {.struct_size = 1, .ticket = 999};
    CHECK(!sWildActorTest->step_pending(commandHandle, &pending) && pending.ticket
        && pending.status == RHH_WILD_STEP_PENDING && pending.source == RHH_WILD_STEP_DIRECT
        && pending.direction == RHH_WILD_ACTOR_WEST && pending.expected.x == created.x - 1
        && pending.expected.y == created.y && sWildActorTest->step_pending(commandHandle, &shortStep)
        && shortStep.ticket == 999 && sWildActorTest->step_context(&shortStep));
    sWildStepCount = 0;
    SaveObjectEvents();
    const struct ObjectEvent *savedMoving = &gSaveBlock1Ptr->objectEvents[created.slot];
    CHECK(moving->heldMovementActive && !savedMoving->heldMovementActive
        && savedMoving->movementActionId == MOVEMENT_ACTION_NONE
        && savedMoving->previousCoords.x == savedMoving->currentCoords.x
        && savedMoving->previousCoords.y == savedMoving->currentCoords.y);
    for (u32 i = 0; i < 100 && moving->heldMovementActive; ++i)
    {
        AnimateSprites();
        if (!i) CHECK(!moving->offScreen && gSprites[moving->spriteId].invisible);
        ModsWildActorsUpdate();
    }
    struct RhhWildActorSnapshot moved = {.struct_size = sizeof(moved)};
    CHECK(!moving->heldMovementActive && !sWildActorTest->find(commandHandle, &moved)
        && moved.x == created.x - 1 && moved.y == created.y);
    CHECK(sWildStepCount == 1 && sLastWildStep.ticket == pending.ticket && sLastWildStep.status == RHH_WILD_STEP_ARRIVED
        && sLastWildStep.after_valid && sLastWildStep.after.x == moved.x && sLastWildStep.after.y == moved.y
        && sWildActorTest->step_pending(commandHandle, &pending));
    /* Interrupted, displaced and untracked movement each receive one outcome. */
    struct ObjectEvent settled = *moving; struct Sprite settledSprite = gSprites[moving->spriteId];
    CHECK(sWildActorTest->step(commandHandle, RHH_WILD_ACTOR_EAST) == RHH_WILD_ACTOR_OK);
    ObjectEventClearHeldMovement(moving); ModsWildActorsUpdate();
    CHECK(sWildStepCount == 2 && sLastWildStep.status == RHH_WILD_STEP_INTERRUPTED && sLastWildStep.after_valid);
    *moving = settled; gSprites[moving->spriteId] = settledSprite;
    CHECK(sWildActorTest->step(commandHandle, RHH_WILD_ACTOR_EAST) == RHH_WILD_ACTOR_OK);
    moving->currentCoords.x += 2; moving->previousCoords = moving->currentCoords; moving->heldMovementFinished = TRUE;
    ModsWildActorsUpdate();
    CHECK(sWildStepCount == 3 && sLastWildStep.status == RHH_WILD_STEP_DEVIATED && sLastWildStep.after_valid);
    *moving = settled; gSprites[moving->spriteId] = settledSprite;
    CHECK(sWildActorTest->step(commandHandle, RHH_WILD_ACTOR_EAST) == RHH_WILD_ACTOR_OK);
    moving->active = FALSE; ModsWildActorsUpdate();
    CHECK(sWildStepCount == 4 && sLastWildStep.status == RHH_WILD_STEP_LOST && !sLastWildStep.after_valid);
    *moving = settled; gSprites[moving->spriteId] = settledSprite;
    CHECK(!sWildConcealTest->at(commandHandle, &policy) && !policy.mode && policy.revision == 2
        && sConcealLastCause == RHH_WILD_CONCEAL_MOVED && sConcealNotifications == 2);
    CHECK(!gSprites[moving->spriteId].invisible && !moving->invisible);
    concealRequest.expected_revision = 2; concealRequest.mode = RHH_WILD_CONCEAL_GROUND;
    concealRequest.reveal_flags = 0; CHECK(!sWildConcealTest->apply(&concealRequest, &conceal));
    CHECK(ModsWildActorGroundCover(moving) && (conceal.after.cover & RHH_WILD_COVER_TALL_GRASS));
    extern void filters_out_some_ground_effects(struct ObjectEvent *, u32 *);
    u32 groundFlags = GROUND_EFFECT_FLAG_TALL_GRASS_ON_MOVE; moving->disableCoveringGroundEffects = TRUE;
    filters_out_some_ground_effects(moving, &groundFlags); CHECK(groundFlags & GROUND_EFFECT_FLAG_TALL_GRASS_ON_MOVE);
    concealRequest.expected_revision = 3; concealRequest.mode = RHH_WILD_CONCEAL_HIDDEN;
    concealRequest.reveal_flags = RHH_WILD_REVEAL_ON_APPROACH; concealRequest.reveal_radius = 1;
    CHECK(!sWildConcealTest->apply(&concealRequest, &conceal));
    groundFlags = GROUND_EFFECT_FLAG_TALL_GRASS_ON_MOVE; filters_out_some_ground_effects(moving, &groundFlags);
    CHECK(!groundFlags); moving->disableCoveringGroundEffects = FALSE;
    gObjectEvents[0].currentCoords = moving->currentCoords; gObjectEvents[0].currentCoords.x++;
    gObjectEvents[0].previousCoords = gObjectEvents[0].currentCoords;
    ModsWildActorsUpdate(); CHECK(!sWildConcealTest->at(commandHandle, &policy) && !policy.mode
        && policy.revision == 5 && sConcealLastCause == RHH_WILD_CONCEAL_APPROACHED);
    gObjectEvents[0].currentCoords = gObjectEvents[0].previousCoords = savedPlayer;
    CHECK(sWildActorTest->step(commandHandle, RHH_WILD_ACTOR_EAST) == RHH_WILD_ACTOR_OK);
    for (u32 i = 0; i < 100 && moving->heldMovementActive; ++i)
    { AnimateSprites(); ModsWildActorsUpdate(); }
    CHECK(!moving->heldMovementActive && !sWildActorTest->find(commandHandle, &moved)
        && moved.x == created.x && moved.y == created.y);
    request.x = moved.x;
    CHECK(sWildActorTest->step(commandHandle, RHH_WILD_ACTOR_WEST) == RHH_WILD_ACTOR_OK);
    CHECK(sWildActorTest->spawn(&request, &created) == RHH_WILD_ACTOR_BLOCKED
        && sWildActorTest->remove(commandHandle) == RHH_WILD_ACTOR_OK && sWildActorRemoved == 1
        && sWildActorTest->remove(commandHandle) == RHH_WILD_ACTOR_INVALID
        && sLastWildStep.status == RHH_WILD_STEP_REMOVED && !sLastWildStep.after_valid);
    request.x = layout.width / 2 + 2;
    for (u32 i = 0; i < OWE_SPAWNS_MAX; ++i)
    {
        struct ObjectEvent *occupied = &gObjectEvents[i + 1];
        memset(occupied, 0, sizeof(*occupied)); occupied->active = TRUE;
        occupied->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER;
        occupied->localId = LOCALID_OW_ENCOUNTER_END - i;
    }
    CHECK(sWildActorTest->spawn(&request, &created) == RHH_WILD_ACTOR_FULL);
    for (u32 i = 0; i < OWE_SPAWNS_MAX; ++i) memset(&gObjectEvents[i + 1], 0, sizeof(gObjectEvents[i + 1]));
    if (module)
    {
        struct RhhModEvent tick = {.struct_size = sizeof(tick), .kind = RHH_MOD_UPDATE};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        RhhModsDispatch(&tick, &decision);
        CHECK(sWildActorSpawned == 2 && sWildActorRemoved == 2);
    }
    sRejectWildActorCommands = FALSE; sWildActorSpawned = sWildActorRemoved = 0;
    if (!module && !requestedSpecies)
    {
        const struct RhhWildDormantService *dormant = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/wild_dormant", RHH_SERVICE_VERSION(1, 0));
        const struct RhhWildProfileService *profiles = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 0));
        const struct RhhWildBehaviorService *behavior = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 4));
        RhhContentKey owner = RhhContentKeyOf("test:mon_edit_rules"), field = RhhContentKeyOf("test:profile/dormant");
        CHECK(dormant && profiles && behavior && !dormant->count());
        RhhModsSetPersistence(IndividualTestPersist); RhhModsSetRunContext(123, 456, 1);
        struct RhhWildActorSnapshot live = {.struct_size = sizeof(live)};
        CHECK(speciesActors->spawn_species(&speciesRequest, &live) == RHH_WILD_ACTOR_OK);
        ModsWildActorsUpdate();
        struct RhhWildActorIndividual id = {.struct_size = sizeof(id)};
        struct Pokemon before, after;
        CHECK(!sWildActorTest->individual(live.handle, &id) && ModsWildActorCopyMon(&gObjectEvents[live.slot], &before));
        struct RhhWildProfileRequest edit = {.struct_size = sizeof(edit), .present = 1, .actor_handle = live.handle,
            .owner = owner, .field = field, .value = -73};
        struct RhhWildProfileResult changed = {.struct_size = sizeof(changed)};
        struct RhhWildQueueRequest queue = {.struct_size = sizeof(queue), .actor_handle = live.handle,
            .owner = owner, .count = 1, .actions = {RHH_WILD_BEHAVIOR_WEST}};
        struct RhhWildQueueChange queueChanged = {.struct_size = sizeof(queueChanged)};
        CHECK(!profiles->apply(&edit, &changed) && !behavior->queue_replace(&queue, &queueChanged));
        struct RhhWildConcealRequest hidden = {.struct_size = sizeof(hidden), .handle = live.handle,
            .owner = owner, .mode = RHH_WILD_CONCEAL_HIDDEN};
        struct RhhWildConcealResult hideResult = {.struct_size = sizeof(hideResult)};
        CHECK(!sWildConcealTest->apply(&hidden, &hideResult));
        struct RhhWildDormantActor saved = {.struct_size = sizeof(saved), .level = 999}, untouchedSaved = saved;
        gObjectEvents[live.slot].currentElevation = 2;
        CHECK(dormant->suspend(live.handle, owner, &saved) == RHH_WILD_ACTOR_BLOCKED
            && !memcmp(&saved, &untouchedSaved, sizeof(saved)) && !dormant->count());
        gObjectEvents[live.slot].currentElevation = 3;
        CHECK(dormant->suspend(live.handle, owner, &saved) == RHH_WILD_ACTOR_OK
            && sLastWildActor.cause == RHH_WILD_ACTOR_SUSPENDED && dormant->count() == 1
            && saved.individual.run_id == id.run_id && saved.individual.serial == id.serial
            && saved.owner == owner && saved.species == live.species && saved.x == live.x && saved.y == live.y
            && sWildActorTest->find_individual(&id, &live));
        struct RhhWildDormantActor found = {.struct_size = sizeof(found)};
        CHECK(!dormant->find(&id, &found) && !memcmp(&found, &saved, sizeof(found)));
        const struct RhhWildDormantService *profilesDormant = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/wild_dormant", RHH_SERVICE_VERSION(1, 3));
        struct RhhWildProfileValue dormantValue = {.struct_size = sizeof(dormantValue)};
        struct RhhWildDormantProfileList fields = {.struct_size = sizeof(fields)};
        struct RhhWildDormantProfileList tinyFields = {.struct_size = 1, .count = 99}, tinyBefore = tinyFields;
        CHECK(profilesDormant && profilesDormant->header.struct_size >= RHH_WILD_DORMANT_SERVICE_V1_3_SIZE
            && !profilesDormant->profile_at(&id, owner, field, &dormantValue)
            && dormantValue.present && dormantValue.value == -73 && dormantValue.revision == 1
            && !profilesDormant->profile_list(&id, owner, &fields) && fields.count == 1 && fields.revision == 1
            && fields.owner_available && fields.fields[0].field == field && fields.fields[0].value == -73
            && profilesDormant->profile_list(&id, owner, &tinyFields) && !memcmp(&tinyFields, &tinyBefore, sizeof(tinyFields)));
        RhhContentKey missingOwner = RhhContentKeyOf("test:missing_profile_owner");
        CHECK(!profilesDormant->profile_list(&id, missingOwner, &fields) && !fields.count
            && !fields.owner_available && fields.revision == 1);
        struct Coords16 playerBefore = gObjectEvents[0].currentCoords;
        gObjectEvents[0].currentCoords.x = gObjectEvents[0].previousCoords.x = saved.x + MAP_OFFSET;
        gObjectEvents[0].currentCoords.y = gObjectEvents[0].previousCoords.y = saved.y + MAP_OFFSET;
        struct RhhWildActorSnapshot restored = {.struct_size = sizeof(restored), .handle = 999}, untouched = restored;
        CHECK(dormant->resume(&id, owner, &restored) == RHH_WILD_ACTOR_BLOCKED
            && !memcmp(&untouched, &restored, sizeof(restored)) && dormant->count() == 1);
        gObjectEvents[0].currentCoords = gObjectEvents[0].previousCoords = playerBefore;
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        CHECK(dormant->resume(&id, owner, &restored) == RHH_WILD_ACTOR_OK
            && sLastWildActor.cause == RHH_WILD_ACTOR_RESUMED && !dormant->count()
            && restored.handle != live.handle && !sWildActorTest->individual(restored.handle, &id)
            && id.serial == saved.individual.serial && ModsWildActorCopyMon(&gObjectEvents[restored.slot], &after)
            && !memcmp(&before, &after, sizeof(before)) && !memcmp(&rng, &gRngValue, sizeof(rng))
            && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        struct RhhWildProfileValue beforeMissing = dormantValue;
        CHECK(profilesDormant->profile_at(&id, owner, field, &dormantValue)
            && !memcmp(&dormantValue, &beforeMissing, sizeof(dormantValue)));
        struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
        struct RhhWildQueue restoredQueue = {.struct_size = sizeof(restoredQueue)};
        struct RhhWildConcealment restoredHidden = {.struct_size = sizeof(restoredHidden)};
        CHECK(!profiles->at(restored.handle, owner, field, &value) && value.present && value.value == -73
            && !behavior->queue_at(restored.handle, &restoredQueue) && restoredQueue.count == 1
            && restoredQueue.actions[0] == RHH_WILD_BEHAVIOR_WEST
            && !sWildConcealTest->at(restored.handle, &restoredHidden) && restoredHidden.mode == RHH_WILD_CONCEAL_HIDDEN);
        CHECK(dormant->suspend(restored.handle, owner, &saved) == RHH_WILD_ACTOR_OK && dormant->count() == 1
            && dormant->discard(&id, RhhContentKeyOf("rhh:nuzlocke")) == RHH_WILD_ACTOR_INVALID
            && dormant->count() == 1 && dormant->discard(&id, owner) == RHH_WILD_ACTOR_OK && !dormant->count());
        RhhModsSetRunContext(123, 456, 0); sWildActorSpawned = sWildActorRemoved = 0;
        puts("wild dormant: identity, saved state, failed placement and restoration passed");
    }
    memset(&sAutomaticWildMon, 0, sizeof(sAutomaticWildMon));
    if (requestedSpecies)
    {
        struct RhhWildActorSnapshot requested = {.struct_size = sizeof(requested)};
        CHECK(speciesActors->spawn_species(&speciesRequest, &requested) == RHH_WILD_ACTOR_OK
            && requested.species == speciesRequest.species && requested.level == speciesRequest.level);
        struct Pokemon beforeRestore, afterRestore;
        CHECK(ModsWildActorCopyMon(&gObjectEvents[requested.slot], &beforeRestore));
        ModsWildActorsBeforeSave(); ModsWildActorsDiscardLive(); ModsWildActorsAfterLoad();
        CHECK(ModsWildActorCopyMon(&gObjectEvents[requested.slot], &afterRestore)
            && !memcmp(&beforeRestore, &afterRestore, sizeof(beforeRestore))
            && !speciesActors->at(requested.slot, &requested));
    }
    else
        for (u32 i = 0; i < 5000 && !sWildActorSpawned; ++i) UpdateOverworldWildEncounter();
    CHECK(sWildActorSpawned == 1 && (sLastWildActor.actor.flags & (RHH_WILD_ACTOR_GENERATED | RHH_WILD_ACTOR_PRESERVED))
        == (RHH_WILD_ACTOR_GENERATED | RHH_WILD_ACTOR_PRESERVED));
    u32 slot = sLastWildActor.actor.slot; struct Pokemon preserved;
    CHECK(slot < OBJECT_EVENTS_COUNT && ModsWildActorCopyMon(&gObjectEvents[slot], &preserved)
        && !memcmp(&preserved, &sAutomaticWildMon, sizeof(preserved)));
    if (!requestedSpecies)
    {
        struct ObjectEvent *wild = &gObjectEvents[slot], *player = &gObjectEvents[0];
        s32 coverY = -1;
        wild->offScreen = FALSE; gSprites[wild->spriteId].invisible = FALSE;
        for (u32 i = 0; i < MAX_SPRITES; ++i)
            if (gSprites[i].inUse && gSprites[i].data[7] == (s16)(0x4200 | slot)) gSprites[i].invisible = FALSE;
        ModsWildActorsUpdate(); ModsWildActorsUpdate();
        CHECK(ModsWildActorPresentation(wild->spriteId, &coverY) == 1 && ModsWildActorCueVisible(wild));
        u32 firstCue = MAX_SPRITES;
        for (u32 i = 0; i < MAX_SPRITES; ++i)
            if (gSprites[i].inUse && gSprites[i].data[7] == (s16)(0x4200 | slot)) { firstCue = i; break; }
        CHECK(firstCue < MAX_SPRITES && gSprites[firstCue].template == gFieldEffectObjectTemplatePointers[FLDEFFOBJ_JUMP_TALL_GRASS]);
        s16 cueX = gSprites[firstCue].x, cueY = gSprites[firstCue].y;
        struct Coords16 original = wild->currentCoords;
        s16 step = original.x + 2 < MAP_OFFSET + layout.width ? 1 : -1;
        u8 usedBefore[MAX_SPRITES];
        for (u32 i = 0; i < MAX_SPRITES; ++i) usedBefore[i] = gSprites[i].inUse;
        attributes[0] = MB_CAVE; wild->currentCoords.x += step; ModsWildActorsUpdate();
        CHECK(gSprites[firstCue].inUse && gSprites[firstCue].x == cueX && gSprites[firstCue].y == cueY);
        bool32 dust = FALSE;
        for (u32 i = 0; i < MAX_SPRITES; ++i)
            if (!usedBefore[i] && gSprites[i].inUse && gSprites[i].data[7] == (s16)(0x4200 | slot)
                && gSprites[i].template == gFieldEffectObjectTemplatePointers[FLDEFFOBJ_GROUND_IMPACT_DUST]) dust = TRUE;
        CHECK(dust);
        for (u32 i = 0; i < MAX_SPRITES; ++i) usedBefore[i] = gSprites[i].inUse;
        attributes[0] = MB_DEEP_WATER; wild->currentCoords.x += step; ModsWildActorsUpdate();
        bool32 ripple = FALSE;
        for (u32 i = 0; i < MAX_SPRITES; ++i)
            if (!usedBefore[i] && gSprites[i].inUse && gSprites[i].data[7] == (s16)(0x4200 | slot)
                && gSprites[i].template == gFieldEffectObjectTemplatePointers[FLDEFFOBJ_RIPPLE]) ripple = TRUE;
        CHECK(ripple);
        attributes[0] = MB_TALL_GRASS; wild->currentCoords = original; ModsWildActorsUpdate(); ModsWildActorsUpdate();
        for (u32 i = 0; i < MAX_SPRITES; ++i)
            if (gSprites[i].inUse && gSprites[i].data[7] == (s16)(0x4200 | slot)) gSprites[i].invisible = FALSE;
        CHECK(ModsWildActorCueVisible(wild));
        player->currentCoords.y = player->previousCoords.y = wild->currentCoords.y;
        if (wild->currentCoords.x > MAP_OFFSET) {
            player->currentCoords.x = player->previousCoords.x = wild->currentCoords.x - 1;
            player->facingDirection = DIR_EAST;
        } else {
            player->currentCoords.x = player->previousCoords.x = wild->currentCoords.x + 1;
            player->facingDirection = DIR_WEST;
        }
        memset(&gPaletteFade, 0, sizeof(gPaletteFade));
        cgb_audio_init(42060); m4aSoundInit();
        /* Draw actual OAM through the PC renderer: hidden before observation,
         * then visible above the habitat surface and clipped below it. */
        static u8 savedObjTiles[0x8000], savedPalette[0x400], oldInvisible[MAX_SPRITES];
        static u16 hiddenFrame[DISPLAY_WIDTH * DISPLAY_HEIGHT], revealFrame[DISPLAY_WIDTH * DISPLAY_HEIGHT];
        struct Sprite *actorSprite = &gSprites[wild->spriteId], savedSprite = *actorSprite;
        memcpy(savedObjTiles, gNativeVram + 0x10000, sizeof(savedObjTiles));
        memcpy(savedPalette, gNativePalette, sizeof(savedPalette));
        u16 oldDisplay = REG_DISPCNT, oldBlend = REG_BLDCNT, oldIme = REG_IME;
        for (u32 i = 0; i < MAX_SPRITES; ++i) { oldInvisible[i] = gSprites[i].invisible; gSprites[i].invisible = TRUE; }
        memset(gNativeVram + 0x10000, 0x11, sizeof(savedObjTiles));
        memset(gNativePalette, 0, sizeof(savedPalette)); ((u16 *)gNativePalette)[0x101] = RGB(31, 31, 31);
        u32 height = GetObjectEventGraphicsInfo(wild->graphicsId)->height;
        CHECK(height == 32 || height == 64);
        actorSprite->invisible = FALSE; actorSprite->x = 100; actorSprite->y = 80;
        actorSprite->x2 = actorSprite->y2 = 0; actorSprite->coordOffsetEnabled = FALSE;
        actorSprite->centerToCornerVecX = actorSprite->centerToCornerVecY = -(s16)(height / 2);
        actorSprite->oam.shape = 0; actorSprite->oam.size = height == 64 ? 3 : 2;
        actorSprite->oam.affineMode = actorSprite->oam.objMode = actorSprite->oam.bpp = 0;
        actorSprite->oam.tileNum = actorSprite->oam.paletteNum = 0; actorSprite->subspriteMode = SUBSPRITES_OFF;
        REG_DISPCNT = DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP; REG_BLDCNT = 0; REG_IME = 0;
        BuildOamBuffer(); LoadOam(); DrawFrame(hiddenFrame);
        for (u32 i = 0; i < MAX_SPRITES; ++i) gSprites[i].invisible = oldInvisible[i];
        CHECK(ModsWildScoutFacing() && ModsWildActorPresentation(wild->spriteId, &coverY) == 2);
        for (u32 i = 0; i < MAX_SPRITES; ++i) gSprites[i].invisible = TRUE;
        actorSprite->invisible = FALSE;
        BuildOamBuffer(); LoadOam(); DrawFrame(revealFrame);
        CHECK(coverY > 0 && coverY < DISPLAY_HEIGHT
            && !(hiddenFrame[(coverY - 1) * DISPLAY_WIDTH + 100] & 0x7fff)
            && (revealFrame[(coverY - 1) * DISPLAY_WIDTH + 100] & 0x7fff)
            && !(revealFrame[coverY * DISPLAY_WIDTH + 100] & 0x7fff));
        *actorSprite = savedSprite;
        for (u32 i = 0; i < MAX_SPRITES; ++i) gSprites[i].invisible = oldInvisible[i];
        memcpy(gNativeVram + 0x10000, savedObjTiles, sizeof(savedObjTiles));
        memcpy(gNativePalette, savedPalette, sizeof(savedPalette));
        REG_DISPCNT = oldDisplay; REG_BLDCNT = oldBlend; REG_IME = oldIme;
        struct Pokemon afterScout;
        CHECK(ModsWildActorCopyMon(wild, &afterScout) && !memcmp(&preserved, &afterScout, sizeof(preserved)));
        puts("WILD_PRESENTATION_SCOUT_OK");
    }
    u64 engagementHandle = sLastWildActor.actor.handle;
    u16 battleGraphics = gObjectEvents[slot].graphicsId;
    ModsWildActorEngaged(&gObjectEvents[slot]);
    gSpecialVar_LastTalked = LOCALID_NONE; ModsWildActorBattleTransition();
    ModsWildActorBattleFinished(B_OUTCOME_WON); CHECK(!sWildActorBattleEnded);
    ModsWildActorEngaged(&gObjectEvents[slot]);
    gSpecialVar_LastTalked = gObjectEvents[slot].localId; ModsWildActorBattleTransition();
    RhhModsSetRunContext(123, 789, 0); ModsWildActorBattleFinished(B_OUTCOME_WON);
    CHECK(!sWildActorBattleEnded); RhhModsSetRunContext(123, 456, 0); sWildActorEngaged = 0;
    gObjectEvents[0].currentCoords.x = gObjectEvents[slot].currentCoords.x - 1;
    gObjectEvents[0].currentCoords.y = gObjectEvents[slot].currentCoords.y;
    gObjectEvents[0].previousCoords = gObjectEvents[0].currentCoords;
    CHECK(sWildActorTest->engage(engagementHandle) == RHH_WILD_ACTOR_OK && ScriptContext_IsEnabled()
        && gSpecialVar_LastTalked == sLastWildActor.actor.local_id && sWildActorEngaged == 0);
    cgb_audio_init(42060); m4aSoundInit();
    for (u32 i = 0; i < 2000 && !sWildActorEngaged; ++i)
    {
        ScriptContext_RunScript(); RunTasks(); AnimateSprites(); ModsWildActorsUpdate(); m4aSoundMain();
    }
    CHECK(sWildActorEngaged == 1 && gMain.savedCallback != NULL
        && !memcmp(&sAutomaticWildMon, &gParties[B_TRAINER_OPPONENT_A][0], sizeof(sAutomaticWildMon)));
    MainCallback endBattle = gMain.savedCallback;
    for (u32 i = 0; i < 2000 && gMain.callback2 != CB2_InitBattle; ++i)
    { RunTasks(); AnimateSprites(); UpdatePaletteFade(); m4aSoundMain(); }
    CHECK(gMain.callback2 == CB2_InitBattle && sWildActorRemoved == 1
        && sLastWildActor.cause == RHH_WILD_ACTOR_CLEAR && !sWildActorBattleEnded);
    gBattleOutcome = B_OUTCOME_CAUGHT; endBattle();
    CHECK(sWildActorBattleEnded == 1 && sLastWildBattle.actor.handle == engagementHandle
        && sLastWildBattle.outcome == RHH_WILD_BATTLE_CAUGHT && !sLastWildBattle.flags);
    ModsWildActorBattleFinished(B_OUTCOME_WON);
    CHECK(sWildActorBattleEnded == 1 && sWildActorTest->battle_result(&noBattle) && noBattle.outcome == 999);
    struct ObjectEvent *safariActor = &gObjectEvents[slot];
    memset(safariActor, 0, sizeof(*safariActor)); safariActor->active = TRUE;
    safariActor->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER;
    safariActor->localId = LOCALID_OW_ENCOUNTER_END; safariActor->graphicsId = battleGraphics;
    safariActor->mapGroup = MAP_GROUP(MAP_ROUTE101); safariActor->mapNum = MAP_NUM(MAP_ROUTE101);
    safariActor->trainerRange_berryTreeId = GetMonData(&preserved, MON_DATA_LEVEL);
    ModsWildActorSpawned(safariActor, &preserved); u64 safariHandle = sLastWildActor.actor.handle;
    ModsWildActorEngaged(safariActor); gSpecialVar_LastTalked = safariActor->localId;
    gBattleTypeFlags = BATTLE_TYPE_SAFARI | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_ROAMER;
    ModsWildActorBattleTransition(); ClearObjectEvent(safariActor);
    gBattleOutcome = B_OUTCOME_CAUGHT; gNumSafariBalls = 1; CB2_EndSafariBattle();
    CHECK(sWildActorBattleEnded == 2 && sLastWildBattle.actor.handle == safariHandle
        && sLastWildBattle.outcome == RHH_WILD_BATTLE_CAUGHT
        && sLastWildBattle.flags == (RHH_WILD_BATTLE_SAFARI | RHH_WILD_BATTLE_DOUBLE | RHH_WILD_BATTLE_ROAMER));
    CB2_EndSafariBattle(); CHECK(sWildActorBattleEnded == 2);
    sCaptureAutomaticWild = FALSE; sWildActorTest = NULL; sWildConcealTest = NULL;
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    if (module) RhhModsUnloadLibraries();
    puts("wild actor: scripted engagement, battle transition and correlated outcome passed");
}
struct DormantSaveExpected { struct Pokemon mon; struct RhhWildActorIndividual id; RhhContentKey map;
    s32 x, y; u64 current_step, processed_step; };
static void FindDormantAuthoredTile(RhhContentKey map, struct RhhAuthoredMapTile *out)
{
    const struct RhhAuthoredMapService *maps = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 0));
    struct RhhAuthoredMap info = {.struct_size = sizeof(info)}; CHECK(maps && !maps->map(map, &info));
    for (u32 y = 0; y < info.height; ++y) for (u32 x = 0; x < info.width; ++x)
    {
        struct RhhAuthoredMapTile tile = {.struct_size = sizeof(tile)};
        if (!maps->tile(map, x, y, &tile) && !tile.tile.collision
            && tile.tile.elevation != ELEVATION_TRANSITION && tile.tile.elevation != ELEVATION_MULTI_LEVEL)
        { *out = tile; return; }
    }
    CHECK(FALSE);
}
static void FindDormantAuthoredSeam(RhhContentKey map, struct RhhAuthoredMapTile *source,
                                   struct RhhAuthoredConnection *connection, struct RhhAuthoredMapTile *destination)
{
    const struct RhhAuthoredMapService *maps = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 3));
    struct RhhAuthoredMap info = {.struct_size = sizeof(info)}; u32 count = 0;
    CHECK(maps && !maps->map(map, &info) && !maps->connection_count(map, &count));
    for (u32 i = 0; i < count; ++i)
    {
        struct RhhAuthoredConnection c = {.struct_size = sizeof(c)};
        CHECK(!maps->connection_at(map, i, &c));
        if (!(c.flags & RHH_AUTHORED_CONNECTION_GEOMETRY_AVAILABLE)
            || c.direction < RHH_AUTHORED_CONNECTION_SOUTH || c.direction > RHH_AUTHORED_CONNECTION_EAST) continue;
        for (s32 along = c.seam_start; along < c.seam_end; ++along)
        {
            s32 x = c.direction == RHH_AUTHORED_CONNECTION_WEST ? 0
                : c.direction == RHH_AUTHORED_CONNECTION_EAST ? info.width - 1 : along;
            s32 y = c.direction == RHH_AUTHORED_CONNECTION_NORTH ? 0
                : c.direction == RHH_AUTHORED_CONNECTION_SOUTH ? info.height - 1 : along;
            s32 nx = x + (c.direction == RHH_AUTHORED_CONNECTION_EAST) - (c.direction == RHH_AUTHORED_CONNECTION_WEST);
            s32 ny = y + (c.direction == RHH_AUTHORED_CONNECTION_SOUTH) - (c.direction == RHH_AUTHORED_CONNECTION_NORTH);
            struct RhhAuthoredMapTile a = {.struct_size = sizeof(a)}, b = {.struct_size = sizeof(b)};
            if (!maps->tile(map, x, y, &a) && !a.tile.collision && a.tile.elevation != ELEVATION_TRANSITION
                && a.tile.elevation != ELEVATION_MULTI_LEVEL
                && !maps->tile(c.target, nx - c.origin_x, ny - c.origin_y, &b))
            { *source = a; *destination = b; *connection = c; return; }
        }
    }
    CHECK(FALSE);
}
static void CheckDormantSave(const char *path, const char *expectedPath, u32 phase, const char *module, const char *extra)
{
    InitModsForTestWithLibraries(module, extra); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    sWildActorTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWildDormantService *dormant = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_dormant", RHH_SERVICE_VERSION(1, 8));
    const struct RhhWildProfileService *profile = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_profile", RHH_SERVICE_VERSION(1, 0));
    sWildConcealTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_concealment", RHH_SERVICE_VERSION(1, 0));
    CHECK(sWildActorTest && dormant && profile && sWildConcealTest);
    if (phase) CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
    else { gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters(); }
    if (phase == 2 || phase == 3 || phase == 5)
    {
        struct DormantSaveExpected expected;
        FILE *file = fopen(expectedPath, "rb"); CHECK(file && fread(&expected, sizeof(expected), 1, file) == 1 && !fclose(file));
        if (phase == 5)
        {
            struct RhhWildDormantClock clock = {.struct_size = sizeof(clock)};
            CHECK(!dormant->clock(&expected.id, &clock) && clock.current_step == expected.current_step
                && clock.processed_step == expected.current_step && !clock.pending_steps);
            sWildActorTest = NULL; sWildConcealTest = NULL; NativeSaveClose();
            puts("wild dormant clock: legacy archive starts at saved step"); return;
        }
        if (phase == 3)
        {
            RhhContentKey missingOwner = RhhContentKeyOf("test:wild_dormant_probe");
            RhhContentKey missingField = RhhContentKeyOf("test:profile/missing_dormant_owner");
            struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
            struct RhhWildDormantProfileList fields = {.struct_size = sizeof(fields)};
            struct RhhWildDormantClock clock = {.struct_size = sizeof(clock)};
            CHECK(!dormant->profile_at(&expected.id, missingOwner, missingField, &value)
                && value.present && value.value == -2222 && value.revision == 12 && !value.owner_available
                && !dormant->profile_list(&expected.id, missingOwner, &fields) && fields.count == 2
                && !fields.owner_available && fields.revision == 12 && fields.fields[0].field < fields.fields[1].field
                && !dormant->clock(&expected.id, &clock) && clock.current_step == expected.current_step
                && clock.processed_step == expected.processed_step && clock.pending_steps == 1);
            sWildActorTest = NULL; sWildConcealTest = NULL; NativeSaveClose();
            puts("wild dormant profile: missing owner fields retained"); return;
        }
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)}; struct Pokemon mon;
        CHECK(!dormant->count() && !sWildActorTest->find_individual(&expected.id, &actor)
            && ModsWildActorCopyMon(&gObjectEvents[actor.slot], &mon) && !memcmp(&mon, &expected.mon, sizeof(mon)));
        if (module)
        {
            struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
            CHECK(!profile->at(actor.handle, RhhContentKeyOf("test:wild_dormant_probe"),
                RhhContentKeyOf("test:profile/missing_dormant_owner"), &value)
                && value.present && value.value == -2222 && value.revision == 12);
            CHECK(!profile->at(actor.handle, RhhContentKeyOf("test:mon_edit_rules"),
                RhhContentKeyOf("test:profile/work_state"), &value)
                && value.present && value.value == 12 && value.revision == 12);
        }
        sWildActorTest = NULL; sWildConcealTest = NULL; NativeSaveClose(); if (module) RhhModsUnloadLibraries();
        puts("wild dormant save: resumed actor survives another restart"); return;
    }
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE101)][MAP_NUM(MAP_ROUTE101)];
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE101); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE101);
    struct MapLayout layout = *gMapHeader.mapLayout; struct Tileset primary = *layout.primaryTileset;
    u16 attributes[1] = {MB_TALL_GRASS}; primary.metatileAttributes = attributes; layout.primaryTileset = &primary;
    gMapHeader.mapLayout = &layout;
    gBackupMapLayout.width = layout.width + MAP_OFFSET_W; gBackupMapLayout.height = layout.height + MAP_OFFSET_H;
    u32 gridSize = gBackupMapLayout.width * gBackupMapLayout.height;
    gBackupMapLayout.map = calloc(gridSize, sizeof(u16)); CHECK(gBackupMapLayout.map);
    for (u32 i = 0; i < gridSize; ++i) gBackupMapLayout.map[i] = PACK_ELEVATION(3);
    ResetTasks(); ResetSpriteData(); FreeAllSpritePalettes(); StartWeather();
    memset(gObjectEvents, 0, sizeof(gObjectEvents)); gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
    struct ObjectEvent *player = &gObjectEvents[0]; player->active = TRUE; player->isPlayer = TRUE;
    player->localId = LOCALID_PLAYER; player->mapGroup = MAP_GROUP(MAP_ROUTE101); player->mapNum = MAP_NUM(MAP_ROUTE101);
    player->graphicsId = OBJ_EVENT_GFX_BRENDAN_NORMAL; player->spriteId = 0; player->currentElevation = 3;
    player->currentCoords.x = MAP_OFFSET + layout.width / 2; player->currentCoords.y = MAP_OFFSET + layout.height / 2;
    player->previousCoords = player->currentCoords; player->previousElevation = 3; gSprites[0].inUse = TRUE;
    const struct ObjectEventGraphicsInfo *graphics = GetObjectEventGraphicsInfo(OBJ_EVENT_GFX_BRENDAN_NORMAL);
    gSprites[0].anims = graphics->anims; gSprites[0].images = graphics->images;
    gSprites[0].animBeginning = FALSE; gSprites[0].animPaused = TRUE;
    ScriptContext_Init(); UnlockPlayerFieldControls(); gMain.callback2 = CB2_Overworld;
    RhhContentKey owner = RhhContentKeyOf("test:mon_edit_rules"), field = RhhContentKeyOf("test:profile/dormant_save");
    struct DormantSaveExpected expected = {0};
    if (phase == 4)
    {
        FILE *file = fopen(expectedPath, "rb"); CHECK(file && fread(&expected, sizeof(expected), 1, file) == 1 && !fclose(file));
        struct RhhWildDormantProfileRequest request = {.struct_size = sizeof(request), .present = 1,
            .individual = expected.id, .owner = owner, .field = field, .expected_revision = 12, .value = 12};
        struct RhhWildDormantProfileResult result = {.struct_size = sizeof(result)};
        struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
        CHECK(dormant->profile_apply(&request, &result) == RHH_WILD_PROFILE_UNAVAILABLE
            && strstr(result.reason, "live profile hooks") && !dormant->profile_at(&expected.id, owner, field, &value)
            && value.present && value.value == 1234 && value.revision == 12);
        struct RhhWildDormantProfileBatchRequest batch = {.struct_size = sizeof(batch), .count = 1,
            .individual = expected.id, .owner = owner, .expected_revision = 12,
            .items = {{.field = field, .present = 1, .value = 12}}};
        struct RhhWildDormantProfileBatchResult batchResult = {.struct_size = sizeof(batchResult)};
        CHECK(dormant->profile_apply_batch(&batch, &batchResult) == RHH_WILD_PROFILE_UNAVAILABLE
            && strstr(batchResult.reason, "live profile hooks"));
        struct RhhWildDormantActor stored = {.struct_size = sizeof(stored)};
        CHECK(!dormant->find(&expected.id, &stored));
        struct RhhWildDormantWorkRequest work = {.struct_size = sizeof(work), .count = 1, .expected = stored,
            .expected_profile_revision = 12, .expected_step = expected.processed_step,
            .through_step = expected.processed_step + 1, .items = {{.field = field, .present = 1, .value = 12}}};
        struct RhhWildDormantWorkResult workResult = {.struct_size = sizeof(workResult)};
        CHECK(dormant->work_apply(&work, &workResult) == RHH_WILD_DORMANT_WORK_UNAVAILABLE
            && strstr(workResult.reason, "live profile hooks"));
        free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; sWildActorTest = NULL; sWildConcealTest = NULL;
        NativeSaveClose(); RhhModsUnloadLibraries();
        puts("wild dormant profile: incompatible legacy rules prevent edits"); return;
    }
    if (!phase)
    {
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
        gPartiesCount[B_TRAINER_PLAYER] = 1;
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        struct RhhWildActorSpeciesSpawnRequest request = {.struct_size = sizeof(request),
            .map = RhhContentKeyForId(RHH_MAP, MAP_ROUTE101), .species = RhhContentKeyForId(RHH_SPECIES, SPECIES_TREECKO),
            .level = 17, .x = layout.width / 2 + 2, .y = layout.height / 2 + 2};
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
        CHECK(sWildActorTest->spawn_species(&request, &actor) == RHH_WILD_ACTOR_OK);
        ModsWildActorsUpdate(); expected.id = (struct RhhWildActorIndividual){.struct_size = sizeof(expected.id)};
        CHECK(!sWildActorTest->individual(actor.handle, &expected.id)
            && ModsWildActorCopyMon(&gObjectEvents[actor.slot], &expected.mon));
        struct RhhWildProfileRequest requestEdit = {.struct_size = sizeof(requestEdit), .present = 1,
            .actor_handle = actor.handle, .owner = owner, .field = field, .value = 1234};
        struct RhhWildProfileResult edit = {.struct_size = sizeof(edit)};
        CHECK(!profile->apply(&requestEdit, &edit));
        if (module)
        {
            requestEdit.owner = RhhContentKeyOf("test:wild_dormant_probe");
            requestEdit.field = RhhContentKeyOf("test:profile/missing_dormant_owner");
            requestEdit.value = -4321; requestEdit.expected_revision = 1;
            CHECK(!profile->apply(&requestEdit, &edit));
        }
        struct RhhWildConcealRequest hidden = {.struct_size = sizeof(hidden), .handle = actor.handle,
            .owner = owner, .mode = RHH_WILD_CONCEAL_HIDDEN};
        struct RhhWildConcealResult hideResult = {.struct_size = sizeof(hideResult)};
        CHECK(!sWildConcealTest->apply(&hidden, &hideResult));
        struct RhhWildDormantActor stored = {.struct_size = sizeof(stored)};
        CHECK(dormant->suspend(actor.handle, owner, &stored) == RHH_WILD_ACTOR_OK && dormant->count() == 1);
        struct RhhWildDormantClock clock = {.struct_size = sizeof(clock)};
        CHECK(!dormant->clock(&expected.id, &clock) && clock.current_step == clock.processed_step && !clock.pending_steps);
        u64 startStep = clock.current_step;
        gMain.inBattle = TRUE; ModsWorldStep(0, 0, 3, MB_TALL_GRASS); gMain.inBattle = FALSE;
        CHECK(!dormant->clock(&expected.id, &clock) && clock.current_step == startStep);
        for (u32 i = 0; i < RHH_WILD_DORMANT_MAX_STEP_ADVANCE + 4; ++i) ModsWorldStep(0, 0, 3, MB_TALL_GRASS);
        CHECK(!dormant->clock(&expected.id, &clock) && clock.current_step == startStep + 260
            && clock.processed_step == startStep && clock.pending_steps == 260);
        struct RhhWildDormantClockRequest acknowledge = {.struct_size = sizeof(acknowledge), .individual = expected.id,
            .owner = owner, .expected_step = startStep, .through_step = clock.current_step};
        struct RhhWildDormantClockChange advanced = {.struct_size = sizeof(advanced), .changed = 99}, unchangedClock = advanced;
        struct RhhWildDormantClock tiny = {.struct_size = 1, .pending_steps = 99}, tinyBefore = tiny;
        CHECK(dormant->clock(&expected.id, &tiny) && !memcmp(&tiny, &tinyBefore, sizeof(tiny))
            && dormant->clock_context(&advanced) && !memcmp(&advanced, &unchangedClock, sizeof(advanced)));
        acknowledge.owner = RhhContentKeyOf("rhh:nuzlocke");
        CHECK(dormant->clock_advance(&acknowledge, &advanced) == RHH_WILD_DORMANT_CLOCK_INVALID
            && !memcmp(&advanced, &unchangedClock, sizeof(advanced)));
        acknowledge.owner = owner;
        CHECK(dormant->clock_advance(&acknowledge, &advanced) == RHH_WILD_DORMANT_CLOCK_LIMIT
            && !memcmp(&advanced, &unchangedClock, sizeof(advanced)));
        acknowledge.expected_step = startStep + 1;
        CHECK(dormant->clock_advance(&acknowledge, &advanced) == RHH_WILD_DORMANT_CLOCK_STALE);
        acknowledge.expected_step = startStep; acknowledge.through_step = clock.current_step + 1;
        CHECK(dormant->clock_advance(&acknowledge, &advanced) == RHH_WILD_DORMANT_CLOCK_INVALID);
        acknowledge.through_step = startStep + RHH_WILD_DORMANT_MAX_STEP_ADVANCE;
        CHECK(dormant->clock_advance(&acknowledge, &advanced) == RHH_WILD_DORMANT_CLOCK_OK
            && advanced.changed && advanced.after.processed_step == acknowledge.through_step && advanced.after.pending_steps == 4);
        acknowledge.expected_step = acknowledge.through_step;
        CHECK(dormant->clock_advance(&acknowledge, &advanced) == RHH_WILD_DORMANT_CLOCK_OK && !advanced.changed);
        expected.current_step = clock.current_step; expected.processed_step = acknowledge.expected_step;
        if (module)
        {
            struct RhhAuthoredMapTile target = {.struct_size = sizeof(target)};
            FindDormantAuthoredTile(RhhContentKeyForId(RHH_MAP, MAP_LITTLEROOT_TOWN), &target);
            struct RhhWildDormantWorkRequest work = {.struct_size = sizeof(work), .flags = RHH_WILD_DORMANT_WORK_RELOCATE,
                .count = 1, .facing = RHH_WILD_ACTOR_NORTH, .expected = stored, .expected_profile_revision = 2,
                .expected_step = acknowledge.expected_step, .through_step = acknowledge.expected_step + 2,
                .map = target.map, .x = target.x, .y = target.y,
                .items = {{.field = RhhContentKeyOf("test:profile/work_state"), .present = 1, .value = 12}}};
            struct RhhWildDormantWorkResult result = {.struct_size = sizeof(result)};
            struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
            struct RhhWildDormantActor current = {.struct_size = sizeof(current)};
            CHECK(dormant->work_preview(&work, &result) == RHH_WILD_DORMANT_WORK_OK && result.is_preview
                && result.profile.changed_count == 1 && result.relocation.changed && result.clock.changed
                && !dormant->profile_at(&expected.id, owner, work.items[0].field, &value) && !value.present
                && !dormant->find(&expected.id, &current) && current.map == stored.map
                && current.x == stored.x && current.y == stored.y);
            work.items[0].value = 777;
            CHECK(dormant->work_apply(&work, &result) == RHH_WILD_DORMANT_WORK_DENIED
                && !dormant->clock(&expected.id, &clock) && clock.processed_step == acknowledge.expected_step
                && !dormant->profile_at(&expected.id, owner, work.items[0].field, &value) && !value.present
                && !dormant->find(&expected.id, &current) && current.map == stored.map
                && current.x == stored.x && current.y == stored.y);
            work.items[0].value = 12; work.expected_step++;
            CHECK(dormant->work_apply(&work, &result) == RHH_WILD_DORMANT_WORK_STALE);
            work.expected_step--;
            CHECK(dormant->work_apply(&work, &result) == RHH_WILD_DORMANT_WORK_OK && result.changed
                && result.profile.changed_count == 1 && result.clock.after.pending_steps == 2
                && result.relocation.after.map == target.map);
            stored = result.relocation.after; expected.processed_step = result.clock.after.processed_step;
            work.expected = stored; work.expected_step = work.through_step; work.expected_profile_revision = 3;
            CHECK(dormant->work_apply(&work, &result) == RHH_WILD_DORMANT_WORK_OK && !result.changed);
            work.items[0].value = 13; work.expected_profile_revision = 2;
            CHECK(dormant->work_apply(&work, &result) == RHH_WILD_DORMANT_WORK_STALE
                && !dormant->profile_at(&expected.id, owner, work.items[0].field, &value)
                && value.present && value.value == 12 && value.revision == 3);
            /* Location/checkpoint work can depend on profile data without editing it. */
            struct RhhWildDormantWorkRequest guarded = {.struct_size = sizeof(guarded),
                .flags = RHH_WILD_DORMANT_WORK_RELOCATE | RHH_WILD_DORMANT_WORK_GUARD_PROFILE,
                .facing = stored.facing == RHH_WILD_ACTOR_NORTH ? RHH_WILD_ACTOR_SOUTH : RHH_WILD_ACTOR_NORTH,
                .expected = stored, .expected_profile_revision = 2,
                .expected_step = expected.processed_step, .through_step = expected.processed_step + 1,
                .map = stored.map, .x = stored.x, .y = stored.y};
            CHECK(dormant->work_preview(&guarded, &result) == RHH_WILD_DORMANT_WORK_STALE
                && dormant->work_apply(&guarded, &result) == RHH_WILD_DORMANT_WORK_STALE
                && !dormant->find(&expected.id, &current) && current.map == stored.map
                && current.x == stored.x && current.y == stored.y && current.facing == stored.facing
                && !dormant->clock(&expected.id, &clock) && clock.processed_step == expected.processed_step);
            guarded.expected_profile_revision = 3;
            CHECK(dormant->work_preview(&guarded, &result) == RHH_WILD_DORMANT_WORK_OK
                && result.relocation.changed && result.clock.changed
                && !dormant->clock(&expected.id, &clock) && clock.processed_step == expected.processed_step);
            guarded.flags = RHH_WILD_DORMANT_WORK_GUARD_PROFILE;
            guarded.map = 0; guarded.x = guarded.y = guarded.facing = 0;
            guarded.through_step = guarded.expected_step;
            CHECK(dormant->work_apply(&guarded, &result) == RHH_WILD_DORMANT_WORK_OK && !result.changed);
            guarded.expected_profile_revision = 2;
            CHECK(dormant->work_apply(&guarded, &result) == RHH_WILD_DORMANT_WORK_STALE);
            guarded.flags = 0; /* Preserve the v1.7 no-edit contract. */
            CHECK(dormant->work_apply(&guarded, &result) == RHH_WILD_DORMANT_WORK_OK && !result.changed);
        }
        if (module)
        {
            struct RhhWildDormantProfileRequest dormantEdit = {.struct_size = sizeof(dormantEdit), .present = 1,
                .individual = expected.id, .owner = RhhContentKeyOf("test:wild_dormant_probe"),
                .field = RhhContentKeyOf("test:profile/missing_dormant_owner"), .expected_revision = 3, .value = -3210};
            struct RhhWildDormantProfileResult changed = {.struct_size = sizeof(changed)};
            struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
            CHECK(dormant->profile_preview(&dormantEdit, &changed) == RHH_WILD_PROFILE_OK
                && changed.changed && changed.is_preview && changed.after.revision == 4
                && !dormant->profile_at(&expected.id, dormantEdit.owner, dormantEdit.field, &value)
                && value.value == -4321 && value.revision == 3);
            dormantEdit.value = 777;
            CHECK(dormant->profile_apply(&dormantEdit, &changed) == RHH_WILD_PROFILE_DENIED
                && !dormant->profile_at(&expected.id, dormantEdit.owner, dormantEdit.field, &value)
                && value.value == -4321 && value.revision == 3);
            dormantEdit.value = -3210; dormantEdit.expected_revision = 2;
            CHECK(dormant->profile_apply(&dormantEdit, &changed) == RHH_WILD_PROFILE_STALE);
            rng_value_t editRng = gRngValue, editRng2 = gRng2Value;
            dormantEdit.expected_revision = 3; sDormantProfileImpure = TRUE;
            CHECK(dormant->profile_apply(&dormantEdit, &changed) == RHH_WILD_PROFILE_DENIED
                && !memcmp(&editRng, &gRngValue, sizeof(editRng)) && !memcmp(&editRng2, &gRng2Value, sizeof(editRng2))
                && !dormant->profile_at(&expected.id, dormantEdit.owner, dormantEdit.field, &value)
                && value.value == -4321 && value.revision == 3);
            sDormantProfileImpure = FALSE;
            dormantEdit.owner = owner; dormantEdit.field = field; dormantEdit.expected_revision = 3; dormantEdit.value = 6001;
            CHECK(dormant->profile_apply(&dormantEdit, &changed) == RHH_WILD_PROFILE_INVALID);
            dormantEdit.owner = RhhContentKeyOf("test:wild_dormant_probe");
            dormantEdit.field = RhhContentKeyOf("test:profile/missing_dormant_owner"); dormantEdit.value = -3210;
            CHECK(dormant->profile_apply(&dormantEdit, &changed) == RHH_WILD_PROFILE_OK
                && changed.changed && changed.after.revision == 4 && !changed.is_preview);
            dormantEdit.expected_revision = 4;
            CHECK(dormant->profile_apply(&dormantEdit, &changed) == RHH_WILD_PROFILE_OK && !changed.changed
                && changed.after.revision == 4);
            dormantEdit.field = RhhContentKeyOf("test:profile/temporary_dormant"); dormantEdit.value = 48;
            CHECK(dormant->profile_apply(&dormantEdit, &changed) == RHH_WILD_PROFILE_OK && changed.after.revision == 5);
            dormantEdit.present = 0; dormantEdit.value = 0; dormantEdit.expected_revision = 5;
            CHECK(dormant->profile_apply(&dormantEdit, &changed) == RHH_WILD_PROFILE_OK && changed.after.revision == 6
                && !dormant->profile_at(&expected.id, dormantEdit.owner, dormantEdit.field, &value)
                && !value.present && value.revision == 6);
            struct RhhWildDormantProfileBatchRequest batch = {.struct_size = sizeof(batch), .count = 2,
                .individual = expected.id, .owner = RhhContentKeyOf("test:wild_dormant_probe"), .expected_revision = 6,
                .items = {{.field = RhhContentKeyOf("test:profile/missing_dormant_owner"), .present = 1, .value = -2222},
                          {.field = RhhContentKeyOf("test:profile/dormant_batch"), .present = 1, .value = 88}}};
            struct RhhWildDormantProfileBatchResult batchResult = {.struct_size = sizeof(batchResult)};
            CHECK(dormant->profile_preview_batch(&batch, &batchResult) == RHH_WILD_PROFILE_OK
                && batchResult.is_preview && batchResult.changed_count == 2
                && batchResult.changes[0].after.revision == 7 && batchResult.changes[1].after.revision == 7
                && !dormant->profile_at(&expected.id, batch.owner, batch.items[0].field, &value)
                && value.value == -3210 && value.revision == 6);
            batch.items[1].value = 777;
            CHECK(dormant->profile_apply_batch(&batch, &batchResult) == RHH_WILD_PROFILE_DENIED
                && !dormant->profile_at(&expected.id, batch.owner, batch.items[0].field, &value)
                && value.value == -3210 && value.revision == 6);
            batch.items[1].value = 88; batch.expected_revision = 5;
            CHECK(dormant->profile_apply_batch(&batch, &batchResult) == RHH_WILD_PROFILE_STALE);
            batch.expected_revision = 6; sDormantProfileImpure = TRUE;
            rng_value_t batchRng = gRngValue, batchRng2 = gRng2Value;
            CHECK(dormant->profile_apply_batch(&batch, &batchResult) == RHH_WILD_PROFILE_DENIED
                && !memcmp(&batchRng, &gRngValue, sizeof(batchRng)) && !memcmp(&batchRng2, &gRng2Value, sizeof(batchRng2))
                && !dormant->profile_at(&expected.id, batch.owner, batch.items[0].field, &value)
                && value.value == -3210 && value.revision == 6);
            sDormantProfileImpure = FALSE; batch.items[1].field = batch.items[0].field;
            CHECK(dormant->profile_apply_batch(&batch, &batchResult) == RHH_WILD_PROFILE_INVALID);
            batch.items[1].field = RhhContentKeyOf("test:profile/dormant_batch");
            CHECK(dormant->profile_apply_batch(&batch, &batchResult) == RHH_WILD_PROFILE_OK
                && batchResult.changed_count == 2 && batchResult.changes[0].after.revision == 7
                && batchResult.changes[1].after.revision == 7);
            batch.expected_revision = 7;
            CHECK(dormant->profile_apply_batch(&batch, &batchResult) == RHH_WILD_PROFILE_OK && !batchResult.changed_count);
            const char *names[] = {"test:profile/fill0", "test:profile/fill1", "test:profile/fill2", "test:profile/fill3"};
            RhhContentKey keys[4];
            memset(batch.items, 0, sizeof(batch.items)); batch.count = 4;
            for (u32 i = 0; i < 4; ++i)
            { keys[i] = RhhContentKeyOf(names[i]); batch.items[i] = (struct RhhWildProfileBatchItem){.field = keys[i], .present = 1, .value = i + 1}; }
            CHECK(dormant->profile_apply_batch(&batch, &batchResult) == RHH_WILD_PROFILE_OK
                && batchResult.changed_count == 4 && batchResult.changes[0].after.revision == 8);
            RhhContentKey workField = RhhContentKeyOf("test:profile/work_state");
            RhhContentKey swapField = RhhContentKeyOf("test:profile/work_swap");
            struct RhhWildDormantWorkRequest swap = {.struct_size = sizeof(swap), .count = 2, .expected = stored,
                .expected_profile_revision = 8, .expected_step = expected.processed_step,
                .through_step = expected.processed_step + 1,
                .items = {{.field = workField}, {.field = swapField, .present = 1, .value = 99}}};
            struct RhhWildDormantWorkResult swapResult = {.struct_size = sizeof(swapResult)};
            CHECK(dormant->work_preview(&swap, &swapResult) == RHH_WILD_DORMANT_WORK_OK
                && swapResult.profile.changed_count == 2 && swapResult.clock.changed);
            CHECK(dormant->work_apply(&swap, &swapResult) == RHH_WILD_DORMANT_WORK_OK
                && swapResult.profile.changed_count == 2 && swapResult.profile.changes[0].after.revision == 9
                && swapResult.clock.after.pending_steps == 1);
            expected.processed_step = swapResult.clock.after.processed_step;
            swap.expected_profile_revision = 9; swap.expected_step = swap.through_step;
            swap.items[0] = (struct RhhWildProfileBatchItem){.field = workField, .present = 1, .value = 12};
            swap.items[1] = (struct RhhWildProfileBatchItem){.field = swapField};
            CHECK(dormant->work_apply(&swap, &swapResult) == RHH_WILD_DORMANT_WORK_OK
                && swapResult.profile.changed_count == 2 && swapResult.profile.changes[0].after.revision == 10
                && !swapResult.clock.changed);
            RhhContentKey replacement = RhhContentKeyOf("test:profile/fill_replacement");
            memset(batch.items, 0, sizeof(batch.items)); batch.count = 2; batch.expected_revision = 10;
            batch.items[0] = (struct RhhWildProfileBatchItem){.field = keys[0]};
            batch.items[1] = (struct RhhWildProfileBatchItem){.field = replacement, .present = 1, .value = 9};
            CHECK(dormant->profile_apply_batch(&batch, &batchResult) == RHH_WILD_PROFILE_OK
                && batchResult.changed_count == 2 && batchResult.changes[0].after.revision == 11);
            batch.count = 1; batch.expected_revision = 11;
            batch.items[0] = (struct RhhWildProfileBatchItem){.field = RhhContentKeyOf("test:profile/overflow"), .present = 1, .value = 1};
            CHECK(dormant->profile_apply_batch(&batch, &batchResult) == RHH_WILD_PROFILE_FULL);
            memset(batch.items, 0, sizeof(batch.items)); batch.count = 4;
            for (u32 i = 0; i < 3; ++i) batch.items[i].field = keys[i + 1];
            batch.items[3].field = replacement;
            CHECK(dormant->profile_apply_batch(&batch, &batchResult) == RHH_WILD_PROFILE_OK
                && batchResult.changed_count == 4 && batchResult.changes[0].after.revision == 12);
            /* Another owner's edits also invalidate a guard taken at revision 3. */
            struct RhhWildDormantWorkRequest guardedClock = {.struct_size = sizeof(guardedClock),
                .flags = RHH_WILD_DORMANT_WORK_GUARD_PROFILE, .expected = stored,
                .expected_profile_revision = 3, .expected_step = expected.processed_step,
                .through_step = expected.processed_step + 1};
            struct RhhWildDormantWorkResult guardedResult = {.struct_size = sizeof(guardedResult)};
            CHECK(dormant->work_apply(&guardedClock, &guardedResult) == RHH_WILD_DORMANT_WORK_STALE
                && !dormant->clock(&expected.id, &clock) && clock.processed_step == expected.processed_step);
            guardedClock.expected_profile_revision = 12;
            CHECK(dormant->work_apply(&guardedClock, &guardedResult) == RHH_WILD_DORMANT_WORK_OK
                && guardedResult.clock.changed && !guardedResult.relocation.changed
                && guardedResult.clock.after.processed_step == guardedClock.through_step);
            expected.processed_step = guardedClock.through_step;
            ModsWorldStep(0, 0, 3, MB_TALL_GRASS);
            ++expected.current_step;
        }
        struct RhhAuthoredMapTile target = {.struct_size = sizeof(target)}, across = {.struct_size = sizeof(across)};
        struct RhhAuthoredConnection seam = {.struct_size = sizeof(seam)};
        FindDormantAuthoredSeam(RhhContentKeyForId(RHH_MAP, MAP_OLDALE_TOWN), &target, &seam, &across);
        struct RhhWildDormantRelocationRequest move = {.struct_size = sizeof(move), .expected = stored,
            .map = target.map, .x = target.x, .y = target.y, .facing = RHH_WILD_ACTOR_NORTH};
        struct RhhWildDormantRelocation moved = {.struct_size = sizeof(moved), .changed = 999}, untouched = moved;
        struct RhhWildDormantRelocationRequest invalid = move; invalid.x = -1;
        CHECK(dormant->relocate(&invalid, &moved) == RHH_WILD_ACTOR_INVALID
            && !memcmp(&moved, &untouched, sizeof(moved)) && dormant->count() == 1);
        CHECK(dormant->relocate(&move, &moved) == RHH_WILD_ACTOR_OK && moved.changed
            && moved.before.individual.serial == expected.id.serial && moved.after.map == target.map);
        untouched = moved;
        CHECK(dormant->relocate(&move, &moved) == RHH_WILD_ACTOR_ROUTE_CHANGED
            && !memcmp(&moved, &untouched, sizeof(moved)));
        move.expected = moved.after;
        CHECK(dormant->relocate(&move, &moved) == RHH_WILD_ACTOR_OK && !moved.changed);
        struct RhhWildDormantStepPreview step = {.struct_size = sizeof(step)}, stepBefore = step;
        CHECK(dormant->step_preview(&expected.id, 0, &step) && !memcmp(&step, &stepBefore, sizeof(step)));
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        CHECK(!dormant->step_preview(&expected.id, seam.direction, &step)
            && (step.flags & (RHH_WILD_DORMANT_STEP_CROSSES_MAP | RHH_WILD_DORMANT_STEP_SOURCE_AVAILABLE
                              | RHH_WILD_DORMANT_STEP_DESTINATION_AVAILABLE))
                == (RHH_WILD_DORMANT_STEP_CROSSES_MAP | RHH_WILD_DORMANT_STEP_SOURCE_AVAILABLE
                    | RHH_WILD_DORMANT_STEP_DESTINATION_AVAILABLE)
            && step.actor.map == target.map && step.connection.index == seam.index
            && step.destination.map == across.map && step.destination.x == across.x && step.destination.y == across.y
            && step.status == (across.tile.collision ? RHH_WILD_DORMANT_STEP_COLLISION
                : across.tile.elevation == ELEVATION_TRANSITION || across.tile.elevation == ELEVATION_MULTI_LEVEL
                    || across.tile.elevation != target.tile.elevation ? RHH_WILD_DORMANT_STEP_ELEVATION
                : RHH_WILD_DORMANT_STEP_OPEN)
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        u32 inward = seam.direction == DIR_NORTH ? DIR_SOUTH : seam.direction == DIR_SOUTH ? DIR_NORTH
            : seam.direction == DIR_WEST ? DIR_EAST : DIR_WEST;
        s32 innerX = target.x + (inward == DIR_EAST) - (inward == DIR_WEST);
        s32 innerY = target.y + (inward == DIR_SOUTH) - (inward == DIR_NORTH);
        const struct RhhAuthoredMapService *maps = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/authored_maps", RHH_SERVICE_VERSION(1, 0));
        struct RhhAuthoredMapTile inner = {.struct_size = sizeof(inner)};
        CHECK(maps && !maps->tile(target.map, innerX, innerY, &inner)
            && !dormant->step_preview(&expected.id, inward, &step)
            && !(step.flags & RHH_WILD_DORMANT_STEP_CROSSES_MAP)
            && (step.flags & RHH_WILD_DORMANT_STEP_DESTINATION_AVAILABLE)
            && step.destination.map == target.map && step.destination.x == innerX && step.destination.y == innerY
            && step.status == (inner.tile.collision ? RHH_WILD_DORMANT_STEP_COLLISION
                : inner.tile.elevation == ELEVATION_TRANSITION || inner.tile.elevation == ELEVATION_MULTI_LEVEL
                    || inner.tile.elevation != target.tile.elevation ? RHH_WILD_DORMANT_STEP_ELEVATION
                : RHH_WILD_DORMANT_STEP_OPEN));
        struct RhhWildDormantActor stillStored = {.struct_size = sizeof(stillStored)};
        CHECK(!dormant->find(&expected.id, &stillStored) && stillStored.map == moved.after.map
            && stillStored.x == moved.after.x && stillStored.y == moved.after.y);
        expected.map = target.map; expected.x = target.x; expected.y = target.y;
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        FILE *file = fopen(expectedPath, "wb"); CHECK(file && fwrite(&expected, sizeof(expected), 1, file) == 1 && !fclose(file));
        puts("wild dormant save: suspended individual committed");
    }
    else
    {
        FILE *file = fopen(expectedPath, "rb"); CHECK(file && fread(&expected, sizeof(expected), 1, file) == 1 && !fclose(file));
        struct RhhWildDormantActor stored = {.struct_size = sizeof(stored)};
        CHECK(dormant->count() == 1 && !dormant->find(&expected.id, &stored)
            && stored.map == expected.map && stored.x == expected.x && stored.y == expected.y);
        struct RhhWildDormantClock clock = {.struct_size = sizeof(clock)};
        CHECK(!dormant->clock(&expected.id, &clock) && clock.current_step == expected.current_step
            && clock.processed_step == expected.processed_step && clock.pending_steps == (module ? 1 : 4));
        struct RhhWildProfileValue dormantValue = {.struct_size = sizeof(dormantValue)};
        struct RhhWildDormantProfileList fields = {.struct_size = sizeof(fields)};
        CHECK(!dormant->profile_at(&expected.id, owner, field, &dormantValue) && dormantValue.present
            && dormantValue.value == 1234 && dormantValue.revision == (module ? 12 : 1)
            && !dormant->profile_list(&expected.id, owner, &fields) && fields.count == (module ? 2 : 1));
        if (module)
        {
            CHECK(!dormant->profile_at(&expected.id, owner, RhhContentKeyOf("test:profile/work_state"), &dormantValue)
                && dormantValue.present && dormantValue.value == 12 && dormantValue.revision == 12);
        }
        struct RhhAuthoredMapTile target = {.struct_size = sizeof(target)};
        FindDormantAuthoredTile(RhhContentKeyForId(RHH_MAP, MAP_ROUTE101), &target);
        struct RhhWildDormantRelocationRequest move = {.struct_size = sizeof(move), .expected = stored,
            .map = target.map, .x = target.x, .y = target.y, .facing = RHH_WILD_ACTOR_SOUTH};
        struct RhhWildDormantRelocation moved = {.struct_size = sizeof(moved)};
        CHECK(dormant->relocate(&move, &moved) == RHH_WILD_ACTOR_OK && moved.changed);
        gBackupMapLayout.map[(target.y + MAP_OFFSET) * gBackupMapLayout.width + target.x + MAP_OFFSET]
            = PACK_ELEVATION(target.tile.elevation);
        struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)}; struct Pokemon mon;
        CHECK(dormant->resume(&expected.id, owner, &actor) == RHH_WILD_ACTOR_OK && !dormant->count()
            && ModsWildActorCopyMon(&gObjectEvents[actor.slot], &mon) && !memcmp(&mon, &expected.mon, sizeof(mon)));
        struct RhhWildProfileValue value = {.struct_size = sizeof(value)};
        struct RhhWildConcealment conceal = {.struct_size = sizeof(conceal)};
        CHECK(!profile->at(actor.handle, owner, field, &value) && value.present && value.value == 1234
            && !sWildConcealTest->at(actor.handle, &conceal) && conceal.mode == RHH_WILD_CONCEAL_HIDDEN
            && conceal.owner == owner);
        if (module)
        {
            CHECK(!profile->at(actor.handle, RhhContentKeyOf("test:wild_dormant_probe"),
                RhhContentKeyOf("test:profile/missing_dormant_owner"), &value)
                && value.present && value.value == -2222 && value.revision == 12);
            CHECK(!profile->at(actor.handle, RhhContentKeyOf("test:wild_dormant_probe"),
                RhhContentKeyOf("test:profile/dormant_batch"), &value)
                && value.present && value.value == 88 && value.revision == 12);
            CHECK(!profile->at(actor.handle, owner, RhhContentKeyOf("test:profile/work_state"), &value)
                && value.present && value.value == 12 && value.revision == 12);
        }
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        puts("wild dormant save: fresh-process restore and subsequent save passed");
    }
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; sWildActorTest = NULL; sWildConcealTest = NULL; NativeSaveClose();
    if (module) RhhModsUnloadLibraries();
}
#endif
static int AcceptPerceptionJournal(uint64_t run, const uint8_t *data, uint32_t size)
{ (void)run; (void)data; (void)size; return 0; }
/* Independent continuous segment/closed-square intersection reference for the
 * metatile supercover. Endpoints are handled by the calling test. */
static bool32 PerceptionReferenceIntersects(s32 x0, s32 y0, s32 x1, s32 y1, s32 tileX, s32 tileY)
{
    double lo = 0.0, hi = 1.0;
    const double starts[2] = {x0 + 0.5, y0 + 0.5}, delta[2] = {x1 - x0, y1 - y0};
    const s32 tile[2] = {tileX, tileY};
    for (u32 axis = 0; axis < 2; ++axis)
    {
        if (!delta[axis])
        { if (starts[axis] < tile[axis] || starts[axis] > tile[axis] + 1) return FALSE; }
        else
        {
            double a = (tile[axis] - starts[axis]) / delta[axis];
            double b = (tile[axis] + 1 - starts[axis]) / delta[axis];
            if (a > b) { double t = a; a = b; b = t; }
            if (a > lo) lo = a;
            if (b < hi) hi = b;
            if (lo > hi + 1e-10) return FALSE;
        }
    }
    return TRUE;
}
static void CheckPerception(const char *module)
{
    InitModsForTestWithLibrary(module);
    u8 *run = RhhModsState("rhh:host_run"); CHECK(run);
    ClockTestPut64(run, 123); ClockTestPut64(run + 8, 456); RhhModsSetRunContext(123, 456, 0);
    gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    struct MapLayout layout = *gMapHeader.mapLayout; struct Tileset primary = *layout.primaryTileset;
    u16 attributes[1] = {MB_TALL_GRASS}; primary.metatileAttributes = attributes; layout.primaryTileset = &primary;
    CHECK(layout.width > 5 && layout.height > 2); gMapHeader.mapLayout = &layout;
    gBackupMapLayout.width = layout.width + MAP_OFFSET_W; gBackupMapLayout.height = layout.height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    memset(gObjectEvents, 0, sizeof(gObjectEvents)); ResetSpriteData();
    gPlayerAvatar.objectEventId = 0;
    struct ObjectEvent *player = &gObjectEvents[0], *wild = &gObjectEvents[1], *obstacle = &gObjectEvents[2];
    player->active = TRUE; player->isPlayer = TRUE; player->mapGroup = 1; player->mapNum = 0; player->spriteId = 0;
    player->currentCoords.x = player->previousCoords.x = MAP_OFFSET; player->currentCoords.y = player->previousCoords.y = MAP_OFFSET;
    player->currentElevation = 3; player->facingDirection = DIR_EAST; gSprites[0].inUse = TRUE;
    wild->active = TRUE; wild->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; wild->localId = LOCALID_OW_ENCOUNTER_END;
    wild->mapGroup = 1; wild->mapNum = 0; wild->spriteId = 1; wild->graphicsId = OBJ_EVENT_GFX_SPECIES(POOCHYENA);
    wild->trainerRange_berryTreeId = 5; wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 4;
    wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET; wild->currentElevation = 3;
    wild->facingDirection = DIR_WEST; gSprites[1].inUse = TRUE; ModsWildActorSpawned(wild, NULL);
    const struct RhhWildActorService *actors = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 0));
    const struct RhhPerceptionService *sight = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/perception", RHH_SERVICE_VERSION(1, 0));
    sWildConcealTest = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_concealment", RHH_SERVICE_VERSION(1, 0));
    CHECK(actors && sight && sWildConcealTest && sight->header.struct_size >= sizeof(*sight)
        && !RhhModsHostAPI()->get_service("rhh:service/perception", RHH_SERVICE_VERSION(1, 1)));
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)}; CHECK(!actors->at(1, &actor));
    RhhModsSetPersistence(AcceptPerceptionJournal); RhhModsSetRunContext(123, 456, 1);
    gMain.callback2 = CB2_Overworld; ModsWildActorsUpdate();
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id)};
    CHECK(!actors->individual(actor.handle, &id) && id.run_id == 456 && id.serial);
    struct RhhPerceptionRequest request = {.struct_size = sizeof(request), .max_distance = 4,
        .viewer = {RHH_PERCEPTION_PLAYER, 0, 0}, .target = {RHH_PERCEPTION_WILD, 0, actor.handle}};
    struct RhhPerceptionResult result = {.struct_size = sizeof(result)};
    rng_value_t random = gRngValue;
    CHECK(!sight->query(&request, &result) && result.distance == 4 && result.viewer_x == 0 && result.target_x == 4
        && result.flags == (RHH_PERCEPTION_LINE_CLEAR | RHH_PERCEPTION_PERCEIVABLE)
        && result.target_wild.handle == actor.handle && result.target_individual.run_id == id.run_id
        && result.target_individual.serial == id.serial && !result.viewer_individual.serial
        && result.target_cover == RHH_WILD_COVER_TALL_GRASS && !result.blocker_flags);
    /* A connected-map transition can leave the player's source-map fields
     * unchanged while its coordinates are already in the loaded map. */
    player->mapGroup = 0; player->mapNum = 0;
    CHECK(!sight->query(&request, &result) && result.map == sTestSceneRule.map
        && (result.flags & RHH_PERCEPTION_PERCEIVABLE));
    struct RhhPerceptionEndpoint reverse = request.viewer; request.viewer = request.target; request.target = reverse;
    CHECK(!sight->query(&request, &result) && result.map == sTestSceneRule.map
        && result.target_cover == RHH_WILD_COVER_TALL_GRASS);
    reverse = request.viewer; request.viewer = request.target; request.target = reverse;
    player->mapGroup = 1; player->mapNum = 0;
    request.max_distance = 3; CHECK(!sight->query(&request, &result) && (result.flags & RHH_PERCEPTION_OUT_OF_RANGE)
        && !(result.flags & RHH_PERCEPTION_LINE_CLEAR)); request.max_distance = 4;
    request.checks = RHH_PERCEPTION_CHECK_FACING; player->facingDirection = DIR_WEST;
    CHECK(!sight->query(&request, &result) && (result.flags & RHH_PERCEPTION_OUTSIDE_FACING)
        && !(result.flags & RHH_PERCEPTION_PERCEIVABLE)); player->facingDirection = DIR_EAST;
    CHECK(!sight->query(&request, &result) && (result.flags & RHH_PERCEPTION_LINE_CLEAR));
    wild->currentElevation = 4; CHECK(!sight->query(&request, &result)
        && (result.flags & RHH_PERCEPTION_ELEVATION_MISMATCH)); wild->currentElevation = 3;
    wild->invisible = TRUE; CHECK(!sight->query(&request, &result)
        && (result.flags & RHH_PERCEPTION_LINE_CLEAR) && (result.flags & RHH_PERCEPTION_TARGET_HIDDEN)
        && !(result.flags & RHH_PERCEPTION_PERCEIVABLE)); wild->invisible = FALSE;
    wild->offScreen = TRUE; gSprites[1].invisible = TRUE; CHECK(!sight->query(&request, &result)
        && (result.flags & RHH_PERCEPTION_TARGET_OFFSCREEN) && (result.flags & RHH_PERCEPTION_TARGET_SPRITE_HIDDEN)
        && (result.flags & RHH_PERCEPTION_PERCEIVABLE)); wild->offScreen = FALSE; gSprites[1].invisible = FALSE;
    gSprites[1].inUse = FALSE; CHECK(!sight->query(&request, &result) && (result.flags & RHH_PERCEPTION_TARGET_NO_SPRITE));
    gSprites[1].inUse = TRUE;
    request.checks = RHH_PERCEPTION_CHECK_SCENE; CHECK(!sight->query(&request, &result)
        && result.blocker_flags == RHH_PERCEPTION_BLOCK_SCENE && result.blocker_x == 1 && result.blocker_y == 0
        && result.blocker_scene_rule == RhhContentKeyOf(sTestSceneRule.id)
        && (result.flags & RHH_PERCEPTION_SCENE_UNANNOTATED) && !(result.flags & RHH_PERCEPTION_LINE_CLEAR));
    wild->currentCoords.x = MAP_OFFSET + 1; wild->currentCoords.y = MAP_OFFSET + 1;
    CHECK(!sight->query(&request, &result) && result.blocker_flags == RHH_PERCEPTION_BLOCK_SCENE
        && result.blocker_x == 1 && result.blocker_y == 0);
    struct RhhPerceptionEndpoint temp = request.viewer; request.viewer = request.target; request.target = temp;
    CHECK(!sight->query(&request, &result) && result.blocker_flags == RHH_PERCEPTION_BLOCK_SCENE
        && result.blocker_x == 1 && result.blocker_y == 0 && result.viewer_wild.handle == actor.handle);
    temp = request.viewer; request.viewer = request.target; request.target = temp;
    wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 4;
    wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET;
    u32 tile = (MAP_OFFSET * gBackupMapLayout.width) + MAP_OFFSET + 2;
    gBackupMapLayout.map[tile] = PACK_COLLISION(1); request.checks = RHH_PERCEPTION_CHECK_COLLISION;
    CHECK(!sight->query(&request, &result) && result.blocker_flags == RHH_PERCEPTION_BLOCK_COLLISION
        && result.blocker_x == 2 && result.blocker_y == 0); gBackupMapLayout.map[tile] = 0;
    CHECK(layout.width >= 8 && layout.height >= 8);
    player->currentCoords.x = player->previousCoords.x = MAP_OFFSET + 4;
    player->currentCoords.y = player->previousCoords.y = MAP_OFFSET + 4; request.max_distance = 6;
    for (s32 dx = -3; dx <= 3; ++dx) for (s32 dy = -3; dy <= 3; ++dy)
    {
        wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 4 + dx;
        wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET + 4 + dy;
        for (s32 tx = 1; tx <= 7; ++tx) for (s32 ty = 1; ty <= 7; ++ty)
        {
            u32 index = (MAP_OFFSET + ty) * gBackupMapLayout.width + MAP_OFFSET + tx;
            gBackupMapLayout.map[index] = PACK_COLLISION(1);
            bool32 expected = (tx != 4 || ty != 4) && (tx != 4 + dx || ty != 4 + dy)
                && PerceptionReferenceIntersects(4, 4, 4 + dx, 4 + dy, tx, ty);
            CHECK(!sight->query(&request, &result));
            if (!!result.blocker_flags != !!expected)
                fprintf(stderr, "perception mismatch: target %d,%d tile %d,%d expected %d\n", dx, dy, tx, ty, expected);
            CHECK(!!result.blocker_flags == !!expected);
            gBackupMapLayout.map[index] = 0;
        }
    }
    player->currentCoords.x = player->previousCoords.x = MAP_OFFSET;
    player->currentCoords.y = player->previousCoords.y = MAP_OFFSET;
    wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 4;
    wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET; request.max_distance = 4;
    obstacle->active = TRUE; obstacle->mapGroup = 0; obstacle->mapNum = 0; obstacle->currentElevation = 3;
    obstacle->currentCoords.x = obstacle->previousCoords.x = MAP_OFFSET + 2;
    obstacle->currentCoords.y = obstacle->previousCoords.y = MAP_OFFSET;
    request.checks = RHH_PERCEPTION_CHECK_ACTORS;
    CHECK(!sight->query(&request, &result) && result.blocker_flags == RHH_PERCEPTION_BLOCK_ACTOR
        && result.blocker_actor_slot == 2 && result.blocker_x == 2);
    obstacle->invisible = TRUE; CHECK(!sight->query(&request, &result) && (result.flags & RHH_PERCEPTION_LINE_CLEAR));
    gMain.callback2 = CB2_Overworld; ScriptContext_Init(); UnlockPlayerFieldControls();
    struct RhhWildConcealRequest concealRequest = {.struct_size = sizeof(concealRequest), .handle = actor.handle,
        .owner = RhhContentKeyOf("test:mon_edit_rules"), .mode = RHH_WILD_CONCEAL_HIDDEN};
    struct RhhWildConcealResult conceal = {.struct_size = sizeof(conceal)};
    CHECK(!sWildConcealTest->apply(&concealRequest, &conceal));
    CHECK(!sight->query(&request, &result) && (result.flags & RHH_PERCEPTION_TARGET_HIDDEN)
        && !(result.flags & RHH_PERCEPTION_PERCEIVABLE) && result.target_conceal_mode == RHH_WILD_CONCEAL_HIDDEN);
    concealRequest.expected_revision = 1; concealRequest.mode = RHH_WILD_CONCEAL_GROUND;
    CHECK(!sWildConcealTest->apply(&concealRequest, &conceal));
    CHECK(!sight->query(&request, &result) && (result.flags & RHH_PERCEPTION_PERCEIVABLE)
        && result.target_conceal_mode == RHH_WILD_CONCEAL_GROUND && result.target_cover == RHH_WILD_COVER_TALL_GRASS);
    if (module)
    {
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        RhhModsDispatch(&event, &decision);
    }
    /* Two wild actors can use the same query for social behavior. */
    memset(obstacle, 0, sizeof(*obstacle)); obstacle->active = TRUE;
    obstacle->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; obstacle->localId = LOCALID_OW_ENCOUNTER_END - 1;
    obstacle->mapGroup = 1; obstacle->mapNum = 0; obstacle->graphicsId = OBJ_EVENT_GFX_SPECIES(ZIGZAGOON);
    obstacle->trainerRange_berryTreeId = 5; obstacle->spriteId = 2; obstacle->currentElevation = 3;
    obstacle->currentCoords.x = obstacle->previousCoords.x = MAP_OFFSET + 2;
    obstacle->currentCoords.y = obstacle->previousCoords.y = MAP_OFFSET; gSprites[2].inUse = TRUE;
    ModsWildActorSpawned(obstacle, NULL);
    struct RhhWildActorSnapshot other = {.struct_size = sizeof(other)}; CHECK(!actors->at(2, &other));
    request.viewer = (struct RhhPerceptionEndpoint){RHH_PERCEPTION_WILD, 0, actor.handle};
    request.target = (struct RhhPerceptionEndpoint){RHH_PERCEPTION_WILD, 0, other.handle}; request.checks = 0;
    CHECK(!sight->query(&request, &result) && (result.flags & RHH_PERCEPTION_PERCEIVABLE)
        && result.viewer_wild.handle == actor.handle && result.target_wild.handle == other.handle
        && result.viewer_individual.serial == id.serial && !result.target_individual.serial);
    struct RhhPerceptionResult unchanged = {.struct_size = sizeof(unchanged), .distance = 999}; result = unchanged;
    request.target.wild_handle = actor.handle;
    CHECK(sight->query(&request, &result) && !memcmp(&result, &unchanged, sizeof(result)));
    request.viewer = (struct RhhPerceptionEndpoint){RHH_PERCEPTION_PLAYER, 0, 0};
    request.target = (struct RhhPerceptionEndpoint){RHH_PERCEPTION_WILD, 0, actor.handle};
    request.reserved = 1; CHECK(sight->query(&request, &result) && !memcmp(&result, &unchanged, sizeof(result)));
    request.reserved = 0; request.max_distance = 65;
    CHECK(sight->query(&request, &result) && !memcmp(&result, &unchanged, sizeof(result))); request.max_distance = 4;
    request.checks = 0x80000000u; CHECK(sight->query(&request, &result) && !memcmp(&result, &unchanged, sizeof(result)));
    request.checks = 0; request.viewer.wild_handle = actor.handle;
    CHECK(sight->query(&request, &result) && !memcmp(&result, &unchanged, sizeof(result))); request.viewer.wild_handle = 0;
    request.target.kind = 99; CHECK(sight->query(&request, &result) && !memcmp(&result, &unchanged, sizeof(result)));
    request.target.kind = RHH_PERCEPTION_WILD;
    RhhModsSetRunContext(0, 0, 0); CHECK(sight->query(&request, &result) && !memcmp(&result, &unchanged, sizeof(result)));
    RhhModsSetRunContext(123, 456, 1); request.target.wild_handle = UINT64_MAX; CHECK(sight->query(&request, &result) && !memcmp(&result, &unchanged, sizeof(result)));
    request.target.wild_handle = actor.handle; gMain.inBattle = TRUE;
    CHECK(sight->query(&request, &result) && !memcmp(&result, &unchanged, sizeof(result))); gMain.inBattle = FALSE;
    wild->mapNum = 1; CHECK(sight->query(&request, &result) && !memcmp(&result, &unchanged, sizeof(result))); wild->mapNum = 0;
    ClearObjectEvent(wild); CHECK(sight->query(&request, &result) && !memcmp(&result, &unchanged, sizeof(result)));
    CHECK(!memcmp(&random, &gRngValue, sizeof(random)));
    sWildConcealTest = NULL; free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    if (module) RhhModsUnloadLibraries();
    puts("perception: distance, facing, concealment, supercover and blockers passed");
}
extern void DrawFrame(u16 *pixels);
static void DisplayCaptureTestHBlank(void)
{
    REG_BLDALPHA = 0x500 + REG_VCOUNT;
    REG_MOSAIC = 0x700 + REG_VCOUNT;
}
static void CheckOverworldService(void)
{
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhOverworldService *service = (const void *)host->get_service(
        "rhh:service/overworld", RHH_SERVICE_VERSION(1, 1));
    const struct RhhWorldSceneService *scene = (const void *)host->get_service(
        "rhh:service/world_scene", RHH_SERVICE_VERSION(1, 2));
    CHECK(scene && scene->header.struct_size >= sizeof(*scene));
    CHECK(host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 0)) == &scene->header
        && host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 1)) == &scene->header
        && host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 3)) == &scene->header
        && host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 4)) == &scene->header
        && host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 5)) == &scene->header
        && host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 6)) == &scene->header
        && host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 7)) == &scene->header
        && host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 8)) == &scene->header
        && host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 9)) == &scene->header
        && host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 10)) == &scene->header
        && host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 11)) == &scene->header
        && !host->get_service("rhh:service/world_scene", RHH_SERVICE_VERSION(1, 12))
        && RHH_WORLD_SCENE_SERVICE_V1_0_SIZE < RHH_WORLD_SCENE_SERVICE_V1_1_SIZE
        && RHH_WORLD_SCENE_SERVICE_V1_1_SIZE < RHH_WORLD_SCENE_SERVICE_V1_2_SIZE
        && RHH_WORLD_SCENE_SERVICE_V1_2_SIZE < RHH_WORLD_SCENE_SERVICE_V1_3_SIZE
        && RHH_WORLD_SCENE_SERVICE_V1_3_SIZE < RHH_WORLD_SCENE_SERVICE_V1_4_SIZE
        && RHH_WORLD_SCENE_SERVICE_V1_4_SIZE < RHH_WORLD_SCENE_SERVICE_V1_5_SIZE
        && RHH_WORLD_SCENE_SERVICE_V1_5_SIZE < RHH_WORLD_SCENE_SERVICE_V1_6_SIZE
        && RHH_WORLD_SCENE_SERVICE_V1_6_SIZE < RHH_WORLD_SCENE_SERVICE_V1_7_SIZE
        && RHH_WORLD_SCENE_SERVICE_V1_7_SIZE < RHH_WORLD_SCENE_SERVICE_V1_8_SIZE
        && RHH_WORLD_SCENE_SERVICE_V1_8_SIZE < RHH_WORLD_SCENE_SERVICE_V1_9_SIZE
        && RHH_WORLD_SCENE_SERVICE_V1_9_SIZE < RHH_WORLD_SCENE_SERVICE_V1_10_SIZE
        && RHH_WORLD_SCENE_SERVICE_V1_10_SIZE == RHH_WORLD_SCENE_SERVICE_V1_11_SIZE);
    struct RhhSceneBillboardTraceRequest billboardTrace = {.struct_size = sizeof(billboardTrace), .width = 8, .height = 8,
        .from = {0, -5, 4}, .to = {0, 5, 4},
        .corners = {{-4, 0, 8}, {4, 0, 8}, {4, 0, 0}, {-4, 0, 0}}};
    struct RhhSceneBillboardTraceResult billboardHit = {.struct_size = sizeof(billboardHit)};
    rng_value_t billboardRng = gRngValue, billboardRng2 = gRng2Value;
    CHECK(!scene->trace_billboard(&billboardTrace, &billboardHit) && billboardHit.status == RHH_SCENE_TRACE_BLOCKED
        && billboardHit.fraction_millionths == 500000 && billboardHit.pixel_x == 4 && billboardHit.pixel_y == 4
        && !memcmp(&billboardRng, &gRngValue, sizeof(billboardRng)) && !memcmp(&billboardRng2, &gRng2Value, sizeof(billboardRng2)));
    CHECK(service && service->header.struct_size >= sizeof(*service));
    CHECK(host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 0)) == &service->header);
    CHECK(host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 2)) == &service->header);
    CHECK(host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 3)) == &service->header);
    CHECK(host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 4)) == &service->header);
    CHECK(host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 5)) == &service->header);
    CHECK(host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 6)) == &service->header);
    CHECK(host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 7)) == &service->header);
    CHECK(host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 8)) == &service->header);
    CHECK(host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 9)) == &service->header);
    CHECK(host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 10)) == &service->header);
    CHECK(host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 11)) == &service->header);
    CHECK(!host->get_service("rhh:service/overworld", RHH_SERVICE_VERSION(1, 12)));
    CHECK(RHH_OVERWORLD_SERVICE_V1_0_SIZE < RHH_OVERWORLD_SERVICE_V1_1_SIZE
        && RHH_OVERWORLD_SERVICE_V1_1_SIZE < RHH_OVERWORLD_SERVICE_V1_2_SIZE
        && RHH_OVERWORLD_SERVICE_V1_2_SIZE < RHH_OVERWORLD_SERVICE_V1_3_SIZE
        && RHH_OVERWORLD_SERVICE_V1_3_SIZE < RHH_OVERWORLD_SERVICE_V1_4_SIZE
        && RHH_OVERWORLD_SERVICE_V1_4_SIZE < RHH_OVERWORLD_SERVICE_V1_5_SIZE
        && RHH_OVERWORLD_SERVICE_V1_5_SIZE < RHH_OVERWORLD_SERVICE_V1_6_SIZE
        && RHH_OVERWORLD_SERVICE_V1_6_SIZE < RHH_OVERWORLD_SERVICE_V1_7_SIZE
        && RHH_OVERWORLD_SERVICE_V1_7_SIZE < RHH_OVERWORLD_SERVICE_V1_8_SIZE
        && RHH_OVERWORLD_SERVICE_V1_8_SIZE < RHH_OVERWORLD_SERVICE_V1_9_SIZE
        && RHH_OVERWORLD_SERVICE_V1_9_SIZE < RHH_OVERWORLD_SERVICE_V1_10_SIZE
        && RHH_OVERWORLD_SERVICE_V1_10_SIZE < RHH_OVERWORLD_SERVICE_V1_11_SIZE);
    CHECK(service->actor_slots() == OBJECT_EVENTS_COUNT);
    struct MapHeader oldHeader = gMapHeader;
    struct BackupMapLayout oldGrid = gBackupMapLayout;
    struct ObjectEvent oldActor = gObjectEvents[0];
    struct Sprite oldSprite = gSprites[0];
    struct OamMatrix oldMatrix = gOamMatrices[8];
    s16 oldOffsetX = gSpriteCoordOffsetX, oldOffsetY = gSpriteCoordOffsetY;
    struct CameraObject oldCamera = gFieldCamera;
    u16 oldCameraX = gTotalCameraPixelOffsetX, oldCameraY = gTotalCameraPixelOffsetY;
    struct Coords16 oldPos = gSaveBlock1Ptr->pos;
    struct WarpData oldLocation = gSaveBlock1Ptr->location;
    u16 oldDisp = REG_DISPCNT;
    u8 oldTile[128], oldPalette[64];
    u8 oldBgTile[32], oldBgPalette[32]; u16 oldBg2 = REG_BG2CNT;
    memcpy(oldTile, gNativeVram + 0x10000, sizeof(oldTile));
    memcpy(oldPalette, gNativePalette + 0x200, sizeof(oldPalette));
    memcpy(oldBgTile, gNativeVram, sizeof(oldBgTile));
    memcpy(oldBgPalette, gNativePalette, sizeof(oldBgPalette));
    gMapHeader = *gMapGroups[1][0];
    gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    gBackupMapLayout.width = gMapHeader.mapLayout->width + MAP_OFFSET_W;
    gBackupMapLayout.height = gMapHeader.mapLayout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16));
    CHECK(gBackupMapLayout.map);
    struct RhhWorldSnapshot world = {.struct_size = sizeof(world)};
    CHECK(!service->current(&world) && world.width == gMapHeader.mapLayout->width);
    {
        const struct RhhAuthoredEncounterService *authored = (const void *)host->get_service(
            "rhh:service/authored_encounters", RHH_SERVICE_VERSION(1, 0));
        RhhContentKey route = RhhContentKeyForId(RHH_MAP, MAP_ROUTE101);
        RhhContentKey cave = RhhContentKeyForId(RHH_MAP, MAP_ALTERING_CAVE);
        RhhContentKey town = RhhContentKeyForId(RHH_MAP, MAP_LITTLEROOT_TOWN);
        struct RhhAuthoredWildTable table = {.struct_size = sizeof(table)};
        struct RhhAuthoredWildTable tiny = {.struct_size = 1, .slot_count = 99}, savedTiny = tiny;
        u32 variants = 99; rng_value_t rng = gRngValue, rng2 = gRng2Value;
        struct WarpData location = gSaveBlock1Ptr->location;
        CHECK(authored && authored->header.struct_size >= sizeof(*authored)
            && host->get_service("rhh:service/authored_encounters", RHH_SERVICE_VERSION(1, 1)) == &authored->header
            && RHH_AUTHORED_ENCOUNTER_SERVICE_V1_0_SIZE < RHH_AUTHORED_ENCOUNTER_SERVICE_V1_1_SIZE
            && host->get_service("rhh:service/authored_encounters", RHH_SERVICE_VERSION(1, 2)) == &authored->header
            && RHH_AUTHORED_ENCOUNTER_SERVICE_V1_1_SIZE < RHH_AUTHORED_ENCOUNTER_SERVICE_V1_2_SIZE
            && !host->get_service("rhh:service/authored_encounters", RHH_SERVICE_VERSION(1, 4))
            && !authored->variants(route, &variants) && variants == 1
            && !authored->table(route, 0, RHH_WILD_TIME_MORNING, RHH_WILD_LAND, &table)
            && table.map == route && table.present && table.encounter_rate == 20
            && table.slot_count == NUM_LAND_MONS_ENCOUNTER_SLOTS
            && table.slots[0].index == 0 && table.slots[0].species == RhhContentKeyForId(RHH_SPECIES, SPECIES_WURMPLE)
            && table.slots[0].species_ordinal == SPECIES_WURMPLE && table.slots[0].min_level == 2 && table.slots[0].max_level == 2);
        CHECK(!authored->table(route, 0, RHH_WILD_TIME_DAY, RHH_WILD_LAND, &table)
            && !table.present && !table.slot_count && !table.encounter_rate
            && !authored->table(route, 0, RHH_WILD_TIME_MORNING, RHH_WILD_WATER, &table) && !table.present);
        RhhContentKey waterRoute = RhhContentKeyForId(RHH_MAP, MAP_ROUTE102);
        RhhContentKey rockRoute = RhhContentKeyForId(RHH_MAP, MAP_ROUTE111);
        CHECK(!authored->table(waterRoute, 0, RHH_WILD_TIME_MORNING, RHH_WILD_WATER, &table)
            && table.present && table.encounter_rate == 4 && table.slot_count == NUM_WATER_MONS_ENCOUNTER_SLOTS
            && !authored->table(waterRoute, 0, RHH_WILD_TIME_MORNING, RHH_WILD_FISHING, &table)
            && table.present && table.encounter_rate == 30 && table.slot_count == NUM_FISHING_MONS_ENCOUNTER_SLOTS
            && !authored->table(rockRoute, 0, RHH_WILD_TIME_MORNING, RHH_WILD_ROCKS, &table)
            && table.present && table.slot_count == NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS
            && !authored->table(route, 0, RHH_WILD_TIME_MORNING, RHH_WILD_HIDDEN, &table) && !table.present);
        CHECK(!authored->variants(cave, &variants) && variants == NUM_ALTERING_CAVE_TABLES
            && !authored->table(cave, 0, RHH_WILD_TIME_MORNING, RHH_WILD_LAND, &table)
            && table.slots[0].species == RhhContentKeyForId(RHH_SPECIES, SPECIES_ZUBAT));
        struct RhhAuthoredWildTable first = table;
        CHECK(!authored->table(cave, 1, RHH_WILD_TIME_MORNING, RHH_WILD_LAND, &table)
            && table.variant == 1 && table.slots[0].species != first.slots[0].species);
        CHECK(!authored->variants(town, &variants) && !variants);
        struct RhhAuthoredWildWeights weights = {.struct_size = sizeof(weights)};
        CHECK(!authored->base_weights(RHH_WILD_LAND, RHH_WILD_ROD_NONE, &weights)
            && weights.available && weights.first_slot == 0 && weights.slot_count == 12 && weights.total == 100
            && weights.weights[0] == 20 && weights.weights[1] == 20 && weights.weights[2] == 10
            && weights.weights[10] == 1 && weights.weights[11] == 1);
        CHECK(!authored->base_weights(RHH_WILD_WATER, RHH_WILD_ROD_NONE, &weights)
            && weights.available && weights.slot_count == 5 && weights.total == 100
            && weights.weights[0] == 60 && weights.weights[1] == 30 && weights.weights[4] == 1);
        CHECK(!authored->base_weights(RHH_WILD_ROCKS, RHH_WILD_ROD_NONE, &weights)
            && weights.available && weights.slot_count == 5 && weights.total == 100
            && weights.weights[0] == 60 && weights.weights[4] == 1);
        CHECK(!authored->base_weights(RHH_WILD_FISHING, RHH_WILD_ROD_OLD, &weights)
            && weights.first_slot == 0 && weights.slot_count == 2 && weights.total == 100
            && weights.weights[0] == 70 && weights.weights[1] == 30 && !weights.weights[2]);
        CHECK(!authored->base_weights(RHH_WILD_FISHING, RHH_WILD_ROD_GOOD, &weights)
            && weights.first_slot == 2 && weights.slot_count == 3 && weights.total == 100
            && !weights.weights[0] && weights.weights[2] == 60 && weights.weights[3] == 20 && weights.weights[4] == 20);
        CHECK(!authored->base_weights(RHH_WILD_FISHING, RHH_WILD_ROD_SUPER, &weights)
            && weights.first_slot == 5 && weights.slot_count == 5 && weights.total == 100
            && weights.weights[5] == 40 && weights.weights[6] == 40 && weights.weights[7] == 15
            && weights.weights[8] == 4 && weights.weights[9] == 1 && !weights.weights[10]);
        CHECK(!authored->base_weights(RHH_WILD_HIDDEN, RHH_WILD_ROD_NONE, &weights)
            && (!weights.available ? !weights.slot_count && !weights.total : weights.slot_count == NUM_HIDDEN_MONS_ENCOUNTER_SLOTS));
        struct RhhAuthoredWildWeights oldWeights = weights, tinyWeights = {.struct_size = 1, .total = 99}, savedTinyWeights = tinyWeights;
        CHECK(authored->base_weights(RHH_WILD_FISHING, RHH_WILD_ROD_NONE, &weights)
            && !memcmp(&weights, &oldWeights, sizeof(weights))
            && authored->base_weights(RHH_WILD_LAND, RHH_WILD_ROD_OLD, &weights)
            && !memcmp(&weights, &oldWeights, sizeof(weights))
            && authored->base_weights(RHH_WILD_AREA_UNKNOWN, RHH_WILD_ROD_NONE, &weights)
            && !memcmp(&weights, &oldWeights, sizeof(weights))
            && authored->base_weights(RHH_WILD_LAND, RHH_WILD_ROD_NONE, &tinyWeights)
            && !memcmp(&tinyWeights, &savedTinyWeights, sizeof(tinyWeights)));
        u32 policyFlags;
        CHECK(ModsWildTimePolicy(TIME_NIGHT, TIME_NIGHT, FALSE, FALSE, FALSE, FALSE, TIME_DAY, &policyFlags)
            == TIME_OF_DAY_DEFAULT && policyFlags == RHH_AUTHORED_WILD_TIME_DISABLED);
        CHECK(ModsWildTimePolicy(TIME_NIGHT, TIME_NIGHT, FALSE, TRUE, FALSE, FALSE, TIME_DAY, &policyFlags)
            == TIME_DAY && policyFlags == RHH_AUTHORED_WILD_TIME_FALLBACK);
        CHECK(ModsWildTimePolicy(TIME_NIGHT, TIME_NIGHT, FALSE, TRUE, TRUE, FALSE, TIME_DAY, &policyFlags)
            == TIME_NIGHT && !policyFlags);
        CHECK(ModsWildTimePolicy(TIME_MORNING, TIME_DAY, TRUE, TRUE, FALSE, FALSE, TIME_NIGHT, &policyFlags)
            == TIME_DAY && policyFlags == RHH_AUTHORED_WILD_TIME_REMAPPED);
        CHECK(ModsWildTimePolicy(TIME_NIGHT, TIME_NIGHT, TRUE, TRUE, FALSE, TRUE, TIME_DAY, &policyFlags)
            == TIME_DAY && policyFlags == RHH_AUTHORED_WILD_TIME_FALLBACK);
        struct RhhAuthoredWildSelection selection = {.struct_size = sizeof(selection)}, savedSelection = selection;
        struct RhhAuthoredWildSelection smallSelection = {.struct_size = 1, .variant = 99}, savedSmall = smallSelection;
        CHECK(!RhhModsHasActiveRun() && authored->selection(route, RHH_WILD_TIME_NIGHT, RHH_WILD_LAND, &selection)
            && !memcmp(&selection, &savedSelection, sizeof(selection)));
        RhhModsSetRunContext(123, 456, 0);
        u16 caveVar = VarGet(VAR_ALTERING_CAVE_WILD_SET);
        VarSet(VAR_ALTERING_CAVE_WILD_SET, 1);
        CHECK(!authored->selection(cave, RHH_WILD_TIME_NIGHT, RHH_WILD_LAND, &selection)
            && selection.run_id == 456 && selection.has_header && selection.variant == 1
            && selection.variant_count == NUM_ALTERING_CAVE_TABLES
            && selection.table.variant == 1 && selection.table.time == selection.selected_time);
        u32 engineVariant, engineFlags; bool32 clamped;
        u16 engineHeader = ModsWildHeaderForMap(MAP_GROUP(MAP_ALTERING_CAVE), MAP_NUM(MAP_ALTERING_CAVE), &engineVariant, &clamped);
        CHECK(engineVariant == 1 && !clamped
            && selection.selected_time == ModsWildResolveTime(&gWildMonHeaders[engineHeader], WILD_AREA_LAND, TIME_NIGHT, FALSE, &engineFlags) + RHH_WILD_TIME_MORNING
            && selection.flags == engineFlags);
        VarSet(VAR_ALTERING_CAVE_WILD_SET, NUM_ALTERING_CAVE_TABLES);
        CHECK(!authored->selection(cave, RHH_WILD_TIME_MORNING, RHH_WILD_LAND, &selection)
            && !selection.variant && (selection.flags & RHH_AUTHORED_WILD_VARIANT_CLAMPED)
            && selection.table.slots[0].species == RhhContentKeyForId(RHH_SPECIES, SPECIES_ZUBAT));
        CHECK(!authored->selection(town, RHH_WILD_TIME_MORNING, RHH_WILD_LAND, &selection)
            && !selection.has_header && !selection.variant_count && !selection.selected_time && !selection.table.struct_size);
        savedSelection = selection;
        CHECK(authored->selection(RhhContentKeyOf("test:unknown_map"), RHH_WILD_TIME_MORNING, RHH_WILD_LAND, &selection)
            && !memcmp(&selection, &savedSelection, sizeof(selection))
            && authored->selection(route, RHH_WILD_TIME_UNKNOWN, RHH_WILD_LAND, &selection)
            && !memcmp(&selection, &savedSelection, sizeof(selection))
            && authored->selection(route, RHH_WILD_TIME_MORNING, RHH_WILD_AREA_UNKNOWN, &selection)
            && !memcmp(&selection, &savedSelection, sizeof(selection))
            && authored->selection(route, RHH_WILD_TIME_MORNING, RHH_WILD_LAND, &smallSelection)
            && !memcmp(&smallSelection, &savedSmall, sizeof(smallSelection)));
        VarSet(VAR_ALTERING_CAVE_WILD_SET, caveVar); RhhModsSetRunContext(0, 0, 0);
        variants = 99; struct RhhAuthoredWildTable before = table;
        CHECK(authored->variants(RhhContentKeyOf("test:unknown_map"), &variants) && variants == 99
            && authored->table(route, 1, RHH_WILD_TIME_MORNING, RHH_WILD_LAND, &table)
            && !memcmp(&table, &before, sizeof(table))
            && authored->table(route, 0, RHH_WILD_TIME_UNKNOWN, RHH_WILD_LAND, &table)
            && !memcmp(&table, &before, sizeof(table))
            && authored->table(route, 0, RHH_WILD_TIME_MORNING, RHH_WILD_AREA_UNKNOWN, &table)
            && !memcmp(&table, &before, sizeof(table))
            && authored->table(route, 0, RHH_WILD_TIME_MORNING, RHH_WILD_LAND, &tiny)
            && !memcmp(&tiny, &savedTiny, sizeof(tiny))
            && !memcmp(&location, &gSaveBlock1Ptr->location, sizeof(location))
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    }
    {
        u16 oldDisplay = REG_DISPCNT, oldAlpha = REG_BLDALPHA, oldMosaic = REG_MOSAIC;
        u16 oldIE = REG_IE, oldIME = REG_IME, oldDispstat = REG_DISPSTAT, oldVcount = REG_VCOUNT;
        MainCallback oldCallback = gMain.callback2;
        IntrFunc oldHBlank = gIntrTable[3];
        struct RhhWorldDisplayFrame frame = {.struct_size = sizeof(frame)};
        struct RhhWorldDisplayFrame tiny = {.struct_size = 1, .count = 99}, saved = tiny;
        u16 *pixels = malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(*pixels)); CHECK(pixels);
        CHECK(service->display_frame(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny)) && service->display_frame(NULL));
        REG_DISPCNT = 0; REG_BLDALPHA = 0x400; REG_MOSAIC = 0x600;
        REG_DISPSTAT = 0; REG_IE = INTR_FLAG_HBLANK; REG_IME = 1; gIntrTable[3] = DisplayCaptureTestHBlank;
        gMain.callback2 = CB2_Overworld;
        DrawFrame(pixels);
        CHECK(!service->display_frame(&frame) && frame.count == RHH_WORLD_DISPLAY_SCANLINES
            && frame.map == world.map && frame.sequence && frame.run_id == RhhModsRunId()
            && frame.rows[0].blend_alpha == 0x400 && frame.rows[0].mosaic == 0x600
            && frame.rows[1].blend_alpha == 0x500 && frame.rows[1].mosaic == 0x700
            && frame.rows[159].blend_alpha == 0x59e && frame.rows[159].mosaic == 0x79e);
        u64 sequence = frame.sequence; DrawFrame(pixels);
        CHECK(!service->display_frame(&frame) && frame.sequence == sequence + 1);
        saved = frame; gSaveBlock1Ptr->location.mapNum = 1;
        CHECK(service->display_frame(&frame) && !memcmp(&frame, &saved, sizeof(frame)));
        gSaveBlock1Ptr->location.mapNum = 0; gMain.inBattle = TRUE; DrawFrame(pixels); gMain.inBattle = FALSE;
        CHECK(service->display_frame(&frame) && !memcmp(&frame, &saved, sizeof(frame)));
        gIntrTable[3] = oldHBlank; REG_IE = oldIE; REG_IME = oldIME;
        gMain.callback2 = oldCallback;
        REG_DISPSTAT = oldDispstat; REG_VCOUNT = oldVcount; REG_DISPCNT = oldDisplay;
        REG_BLDALPHA = oldAlpha; REG_MOSAIC = oldMosaic; free(pixels);
    }
    {
        u16 regs[] = {REG_DISPCNT, REG_BG0CNT, REG_BG1CNT, REG_BG2CNT, REG_BG3CNT,
            REG_BLDCNT, REG_BLDALPHA, REG_BLDY, REG_MOSAIC, REG_WIN0H, REG_WIN0V,
            REG_WIN1H, REG_WIN1V, REG_WININ, REG_WINOUT};
        REG_DISPCNT = 0xf100; REG_BG0CNT = 0x1234; REG_BG1CNT = 0x2345;
        REG_BG2CNT = 0x3456; REG_BG3CNT = 0x4567; REG_BLDCNT = 0x1553;
        REG_BLDALPHA = 0x1c12; REG_BLDY = 30; REG_MOSAIC = 0xf321;
        REG_WIN0H = 0x1020; REG_WIN0V = 0x3040; REG_WIN1H = 0x5060;
        REG_WIN1V = 0x7080; REG_WININ = 0x152a; REG_WINOUT = 0x2a15;
        struct RhhWorldDisplayState display = {.struct_size = sizeof(display)};
        struct RhhWorldDisplayState tiny = {.struct_size = 1, .display_control = 99}, saved = tiny;
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        CHECK(!service->display_state(&display) && display.map == world.map && display.display_control == 0xf100
            && display.bg_control[0] == 0x1234 && display.bg_control[3] == 0x4567
            && display.blend_control == 0x1553 && display.blend_mode == RHH_WORLD_BLEND_ALPHA
            && display.target1 == 0x13 && display.target2 == 0x15 && display.alpha_a == 18
            && display.alpha_b == 28 && display.brightness == 30
            && display.bg_mosaic_width == 2 && display.bg_mosaic_height == 3
            && display.obj_mosaic_width == 4 && display.obj_mosaic_height == 16
            && display.win0_h == 0x1020 && display.win0_v == 0x3040
            && display.win1_h == 0x5060 && display.win1_v == 0x7080
            && display.win_in == 0x152a && display.win_out == 0x2a15
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        CHECK(service->display_state(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny)) && service->display_state(NULL));
        gMain.inBattle = TRUE; saved = display;
        CHECK(service->display_state(&display) && !memcmp(&display, &saved, sizeof(display)));
        gMain.inBattle = FALSE;
        REG_DISPCNT = regs[0]; REG_BG0CNT = regs[1]; REG_BG1CNT = regs[2]; REG_BG2CNT = regs[3]; REG_BG3CNT = regs[4];
        REG_BLDCNT = regs[5]; REG_BLDALPHA = regs[6]; REG_BLDY = regs[7]; REG_MOSAIC = regs[8];
        REG_WIN0H = regs[9]; REG_WIN0V = regs[10]; REG_WIN1H = regs[11]; REG_WIN1V = regs[12];
        REG_WININ = regs[13]; REG_WINOUT = regs[14];
    }
    struct RhhWorldTile tile = {.struct_size = sizeof(tile)};
    CHECK(!service->tile_at(0, 0, &tile) && !tile.metatile && !tile.collision);
    CHECK(!memcmp(tile.tiles, gMapHeader.mapLayout->primaryTileset->metatiles, sizeof(tile.tiles)));
    CHECK(service->tile_at(-1, 0, &tile) && service->tile_at(world.width, 0, &tile));
    CheckHabitatService();
    CheckWildEncounterService();
    CheckWildActorService();
    rng_value_t sceneRng = gRngValue;
    /* Exercise inference using known tile pixels, without changing the map or
     * legacy scene queries. Explicit rules must always take precedence. */
    {
        const struct MapLayout *original = gMapHeader.mapLayout;
        struct MapLayout inferredLayout = *original;
        struct Tileset inferredTileset = *original->primaryTileset;
        u16 attributes[] = {MB_SIGNPOST};
        u16 tiles[] = {0, 0, 0, 0, 1, 1, 1, 1};
        u8 savedPixels[64]; memcpy(savedPixels, gNativeVram, sizeof(savedPixels));
        u16 savedBg2 = REG_BG2CNT; REG_BG2CNT &= ~(3 << 2);
        memset(gNativeVram, 0x11, 32); memset(gNativeVram + 32, 0, 32);
        memset(gNativeVram + 32, 0x11, 16);
        inferredTileset.metatileAttributes = attributes; inferredTileset.metatiles = tiles;
        inferredLayout.primaryTileset = &inferredTileset; gMapHeader.mapLayout = &inferredLayout;
        struct RhhSceneInference inference = {.struct_size = sizeof(inference)};
        CHECK(!scene->window_infer(0, 0, &inference)
            && inference.flags == (RHH_SCENE_INFERENCE_SAMPLED | RHH_SCENE_INFERRED_SIGN)
            && inference.ground_pixels == 256 && inference.upper_pixels == 128
            && inference.tile.scene.style.upper == RHH_SCENE_UPPER_BILLBOARD);
        struct RhhSceneMesh inferredMesh = {.struct_size = sizeof(inferredMesh)};
        CHECK(!scene->window_inferred_mesh_at(0, 0, &inferredMesh) && inferredMesh.count >= 8);
        for (unsigned i = 0; i < inferredMesh.count; ++i)
            if (inferredMesh.quads[i].kind == RHH_SCENE_QUAD_BILLBOARD)
                CHECK(inferredMesh.quads[i].flags & RHH_SCENE_CASTS_SHADOW);
            else CHECK(!(inferredMesh.quads[i].flags & RHH_SCENE_CASTS_SHADOW));
        struct RhhSceneWindowTile legacy = {.struct_size = sizeof(legacy)};
        CHECK(!scene->window_resolve(0, 0, &legacy) && legacy.scene.style.upper == RHH_SCENE_UPPER_FLAT);
        struct RhhSceneTraceRequest ray = {.struct_size = sizeof(ray), .flags = RHH_SCENE_TRACE_ALPHA,
            .from = {4, 0, 14}, .to = {4, 15, 14}};
        struct RhhSceneTraceResult hit = {.struct_size = sizeof(hit)}, oldHit = {.struct_size = sizeof(oldHit)};
        CHECK(!scene->trace_light(&ray, &oldHit) && oldHit.status == RHH_SCENE_TRACE_CLEAR);
        CHECK(!scene->trace_inferred_light(&ray, &hit) && hit.status == RHH_SCENE_TRACE_BLOCKED
            && hit.tile_x == 0 && hit.tile_y == 0 && hit.quad_kind == RHH_SCENE_QUAD_BILLBOARD
            && hit.fraction_millionths > 533000 && hit.fraction_millionths < 534000);
        struct RhhSceneDirectionalTraceRequest directional = {.struct_size = sizeof(directional),
            .flags = RHH_SCENE_TRACE_ALPHA | RHH_SCENE_DIRECTIONAL_INFERRED,
            .from = {4, 0, 14}, .direction = {0, 2, 0}, .max_distance = 15};
        struct RhhSceneDirectionalTraceResult directionHit = {.struct_size = sizeof(directionHit)};
        CHECK(!scene->trace_directional_light(&directional, &directionHit) && !directionHit.coverage
            && directionHit.distance == 15 && directionHit.end[1] == 15 && directionHit.hit.status == RHH_SCENE_TRACE_BLOCKED
            && directionHit.hit.fraction_millionths == hit.fraction_millionths);
        u8 silhouette[8 * 8 * 4] = {0}; silhouette[(2 * 8 + 4) * 4 + 3] = 255;
        struct RhhSceneBillboardTraceRequest actorRay = {.struct_size = sizeof(actorRay), .flags = RHH_SCENE_TRACE_ALPHA,
            .width = 8, .height = 8, .stride = 32, .rgba_size = sizeof(silhouette), .rgba = silhouette,
            .from = {4, 0, 14}, .corners = {{0, 8, 16}, {8, 8, 16}, {8, 8, 8}, {0, 8, 8}}};
        memcpy(actorRay.to, directionHit.end, sizeof(actorRay.to));
        struct RhhSceneBillboardTraceResult actorHit = {.struct_size = sizeof(actorHit)};
        CHECK(!scene->trace_billboard(&actorRay, &actorHit) && actorHit.status == RHH_SCENE_TRACE_BLOCKED
            && actorHit.fraction_millionths == directionHit.hit.fraction_millionths);
        struct RhhSceneShadowCaster casters[2] = {
            {.struct_size = sizeof(casters[0]), .flags = RHH_SCENE_TRACE_ALPHA, .width = 8, .height = 8,
             .stride = 32, .rgba_size = sizeof(silhouette), .id = 10, .rgba = silhouette},
            {.struct_size = sizeof(casters[1]), .flags = RHH_SCENE_TRACE_ALPHA, .width = 8, .height = 8,
             .stride = 32, .rgba_size = sizeof(silhouette), .id = 2, .rgba = silhouette}};
        memcpy(casters[0].corners, actorRay.corners, sizeof(actorRay.corners));
        memcpy(casters[1].corners, actorRay.corners, sizeof(actorRay.corners));
        struct RhhSceneShadowTraceRequest shadow = {.struct_size = sizeof(shadow), .caster_count = 2,
            .ray = directional, .casters = casters};
        struct RhhSceneShadowTraceResult combined = {.struct_size = sizeof(combined)};
        rng_value_t shadowRng2 = gRng2Value;
        CHECK(!scene->trace_directional_shadow(&shadow, &combined) && combined.kind == RHH_SCENE_SHADOW_MESH
            && combined.caster_index == UINT32_MAX && !combined.caster_id
            && combined.fraction_millionths == directionHit.hit.fraction_millionths);
        for (u32 i = 0; i < 4; ++i) casters[0].corners[i][1] = casters[1].corners[i][1] = 4;
        CHECK(!scene->trace_directional_shadow(&shadow, &combined) && combined.kind == RHH_SCENE_SHADOW_BILLBOARD
            && combined.caster_index == 1 && combined.caster_id == 2
            && combined.fraction_millionths > 266000 && combined.fraction_millionths < 267000
            && combined.ray.hit.status == RHH_SCENE_TRACE_BLOCKED && combined.billboard.pixel_x == 4
            && combined.billboard.pixel_y == 2);
        u8 transparent[8 * 8 * 4] = {0}; casters[1].rgba = transparent;
        CHECK(!scene->trace_directional_shadow(&shadow, &combined) && combined.caster_id == 10 && combined.caster_index == 0);
        u8 largeSilhouette[128 * 128 * 4] = {0}; largeSilhouette[(32 * 128 + 64) * 4 + 3] = 255;
        casters[0].width = casters[0].height = 128; casters[0].stride = 128 * 4;
        casters[0].rgba_size = sizeof(largeSilhouette); casters[0].rgba = largeSilhouette;
        CHECK(!scene->trace_directional_shadow(&shadow, &combined) && combined.kind == RHH_SCENE_SHADOW_BILLBOARD
            && combined.caster_id == 10 && combined.billboard.pixel_x == 64 && combined.billboard.pixel_y == 32);
        casters[0].width = casters[0].height = 8; casters[0].stride = 32;
        casters[0].rgba_size = sizeof(silhouette); casters[0].rgba = silhouette;
        struct RhhSceneShadowTraceResult savedCombined = combined;
        casters[1].id = 10;
        CHECK(scene->trace_directional_shadow(&shadow, &combined) && !memcmp(&combined, &savedCombined, sizeof(combined)));
        casters[1].id = 2; casters[1].stride = 1;
        CHECK(scene->trace_directional_shadow(&shadow, &combined) && !memcmp(&combined, &savedCombined, sizeof(combined)));
        casters[1].stride = 32; shadow.caster_count = RHH_SCENE_SHADOW_CASTER_LIMIT + 1;
        CHECK(scene->trace_directional_shadow(&shadow, &combined) && !memcmp(&combined, &savedCombined, sizeof(combined)));
        shadow.caster_count = 0; shadow.casters = NULL;
        CHECK(!scene->trace_directional_shadow(&shadow, &combined) && combined.kind == RHH_SCENE_SHADOW_MESH
            && !memcmp(&sceneRng, &gRngValue, sizeof(sceneRng)) && !memcmp(&shadowRng2, &gRng2Value, sizeof(shadowRng2)));
        directional.flags = RHH_SCENE_TRACE_ALPHA;
        CHECK(!scene->trace_directional_light(&directional, &directionHit) && directionHit.hit.status == RHH_SCENE_TRACE_CLEAR);
        directional.flags |= RHH_SCENE_DIRECTIONAL_INFERRED; directional.from[2] = 10;
        CHECK(!scene->trace_directional_light(&directional, &directionHit) && directionHit.hit.status == RHH_SCENE_TRACE_CLEAR);
        struct RhhSceneWindow bounds = {.struct_size = sizeof(bounds)}; CHECK(!scene->window(&bounds));
        directional.flags = 0; directional.from[0] = (bounds.max_x + 1) * 16.f - 2.f; directional.from[1] = 0;
        directional.from[2] = 100; directional.direction[0] = 1; directional.direction[1] = 0; directional.max_distance = 10;
        CHECK(!scene->trace_directional_light(&directional, &directionHit)
            && directionHit.coverage == RHH_SCENE_DIRECTIONAL_WINDOW_CLIPPED && directionHit.distance > 1.9f && directionHit.distance < 2.f
            && directionHit.end[0] < (bounds.max_x + 1) * 16.f);
        directional.from[0] = bounds.min_x * 16.f; directional.direction[0] = -1;
        CHECK(!scene->trace_directional_light(&directional, &directionHit)
            && directionHit.coverage == RHH_SCENE_DIRECTIONAL_WINDOW_CLIPPED && directionHit.distance == 0
            && directionHit.hit.status == RHH_SCENE_TRACE_CLEAR);
        shadow.ray = directional; shadow.caster_count = 1; shadow.casters = casters;
        CHECK(!scene->trace_directional_shadow(&shadow, &combined) && combined.kind == RHH_SCENE_SHADOW_CLEAR
            && combined.ray.coverage == RHH_SCENE_DIRECTIONAL_WINDOW_CLIPPED && !combined.ray.distance
            && combined.caster_index == UINT32_MAX && !combined.caster_id);
        savedCombined = combined; casters[0].stride = 1;
        CHECK(scene->trace_directional_shadow(&shadow, &combined) && !memcmp(&combined, &savedCombined, sizeof(combined)));
        casters[0].stride = 32;
        directional.from[0] = 4; directional.from[1] = 4; directional.from[2] = 4090;
        directional.direction[0] = 0; directional.direction[2] = 1; directional.max_distance = 50;
        CHECK(!scene->trace_directional_light(&directional, &directionHit)
            && directionHit.coverage == RHH_SCENE_DIRECTIONAL_HEIGHT_CLIPPED && directionHit.distance == 6 && directionHit.end[2] == 4096);
        directional.from[0] = 4; directional.from[1] = 0; directional.from[2] = 14;
        directional.direction[0] = 3; directional.direction[1] = 4; directional.direction[2] = 0; directional.max_distance = 5;
        CHECK(!scene->trace_directional_light(&directional, &directionHit) && !directionHit.coverage
            && fabsf(directionHit.distance - 5.f) < .0001f && fabsf(directionHit.end[0] - 7.f) < .0001f
            && fabsf(directionHit.end[1] - 4.f) < .0001f);
        struct RhhSceneDirectionalTraceResult savedDirection = directionHit;
        directional.direction[0] = directional.direction[1] = directional.direction[2] = 0;
        CHECK(scene->trace_directional_light(&directional, &directionHit) && !memcmp(&directionHit, &savedDirection, sizeof(directionHit)));
        directional.direction[2] = 1; directional.max_distance = 513;
        CHECK(scene->trace_directional_light(&directional, &directionHit) && !memcmp(&directionHit, &savedDirection, sizeof(directionHit)));
        directional.max_distance = 50; directional.from[2] = NAN;
        CHECK(scene->trace_directional_light(&directional, &directionHit) && !memcmp(&directionHit, &savedDirection, sizeof(directionHit)));
        ray.from[2] = ray.to[2] = 10;
        CHECK(!scene->trace_inferred_light(&ray, &hit) && hit.status == RHH_SCENE_TRACE_CLEAR);
        ray.flags = 0;
        CHECK(!scene->trace_inferred_light(&ray, &hit) && hit.status == RHH_SCENE_TRACE_BLOCKED);
        ray.from[0] = ray.to[0] = 20; ray.from[1] = -8; ray.to[1] = 8; ray.from[2] = ray.to[2] = 20;
        CHECK(!scene->trace_light(&ray, &oldHit) && !scene->trace_inferred_light(&ray, &hit)
            && oldHit.status == RHH_SCENE_TRACE_BLOCKED && !memcmp(&hit, &oldHit, sizeof(hit)));
        struct RhhSceneTraceResult savedHit = hit; ray.flags = 2;
        CHECK(scene->trace_inferred_light(&ray, &hit) && !memcmp(&hit, &savedHit, sizeof(hit)));
        ray.flags = 0; ray.to[0] = 100000;
        CHECK(scene->trace_inferred_light(&ray, &hit) && !memcmp(&hit, &savedHit, sizeof(hit)));
        CHECK(!scene->window_infer(1, 0, &inference) && !inference.flags
            && inference.tile.scene.rule == RhhContentKeyOf(sTestSceneRule.id)
            && inference.tile.scene.style.upper == RHH_SCENE_UPPER_WALL_NORTH);
        attributes[0] = MB_POND_WATER;
        CHECK(!scene->window_infer(0, 0, &inference) && inference.flags == RHH_SCENE_INFERRED_WATER
            && inference.tile.scene.style.surface == RHH_SCENE_WATER);
        attributes[0] = MB_SIGNPOST; memset(gNativeVram + 32, 0, 32);
        CHECK(!scene->window_infer(0, 0, &inference) && inference.flags == RHH_SCENE_INFERENCE_SAMPLED
            && inference.tile.scene.style.upper == RHH_SCENE_UPPER_FLAT);
        struct RhhSceneInference unchanged = inference;
        CHECK(scene->window_infer(-1000, 0, &inference) && !memcmp(&inference, &unchanged, sizeof(inference)));
        inference.struct_size = 1; unchanged = inference;
        CHECK(scene->window_infer(0, 0, &inference) && !memcmp(&inference, &unchanged, sizeof(inference)));
        CHECK(!memcmp(&sceneRng, &gRngValue, sizeof(sceneRng)));
        gMapHeader.mapLayout = original; REG_BG2CNT = savedBg2;
        memcpy(gNativeVram, savedPixels, sizeof(savedPixels));
    }
    struct RhhSceneTile sceneTile = {.struct_size = sizeof(sceneTile)};
    CHECK(!scene->tile_at(0, 0, &sceneTile) && sceneTile.map == sTestSceneRule.map
        && !sceneTile.rule && sceneTile.tile.metatile == tile.metatile
        && sceneTile.style.surface == RHH_SCENE_SOLID && sceneTile.style.upper == RHH_SCENE_UPPER_FLAT
        && sceneTile.style.flags == RHH_SCENE_RECEIVES_SHADOW);
    CHECK(!scene->tile_at(1, 0, &sceneTile) && sceneTile.rule == RhhContentKeyOf(sTestSceneRule.id)
        && sceneTile.x == 1 && sceneTile.y == 0 && sceneTile.tile.struct_size == sizeof(tile)
        && sceneTile.style.upper == RHH_SCENE_UPPER_WALL_NORTH
        && sceneTile.style.ground_height == 8 && sceneTile.style.upper_height == 24
        && sceneTile.style.flags == (RHH_SCENE_RECEIVES_SHADOW | RHH_SCENE_CASTS_SHADOW | RHH_SCENE_OCCLUDES));
    struct RhhSceneMesh mesh = {.struct_size = sizeof(mesh)};
    CHECK(!scene->mesh_at(1, 0, &mesh) && mesh.count == 14
        && mesh.scene.rule == sceneTile.rule && mesh.quads[0].kind == RHH_SCENE_QUAD_GROUND
        && mesh.quads[0].vertex[0][0] == 16 && mesh.quads[0].vertex[0][1] == 0
        && mesh.quads[0].vertex[0][2] == 8 && mesh.quads[0].normal[2] == 1
        && mesh.quads[4].kind == RHH_SCENE_QUAD_UPPER && mesh.quads[4].normal[1] == -1
        && mesh.quads[4].vertex[0][2] == 32 && mesh.quads[4].vertex[3][2] == 20
        && mesh.quads[8].kind == RHH_SCENE_QUAD_GROUND_EDGE && mesh.quads[8].normal[1] == 1
        && mesh.quads[8].vertex[0][2] == 8 && mesh.quads[8].vertex[3][2] == 0);
    struct RhhSceneMesh unchangedMesh = mesh;
    CHECK(scene->mesh_at(-1, 0, &mesh) && !memcmp(&mesh, &unchangedMesh, sizeof(mesh)));
    mesh.struct_size = 1; unchangedMesh = mesh;
    CHECK(scene->mesh_at(0, 0, &mesh) && !memcmp(&mesh, &unchangedMesh, sizeof(mesh)));
    mesh.struct_size = sizeof(mesh);
    struct RhhSceneTile source = sceneTile;
    source.x = 2; source.y = 3; source.style.ground_height = -2; source.style.upper_height = 17;
    for (unsigned i = 0; i < 8; ++i) source.tile.tiles[i] = i + 100;
    for (unsigned upper = RHH_SCENE_UPPER_FLAT; upper <= RHH_SCENE_UPPER_HIDDEN; ++upper)
    {
        source.style.upper = upper;
        source.style.upper_height = upper == RHH_SCENE_UPPER_FLAT || upper == RHH_SCENE_UPPER_HIDDEN ? 0 : 17;
        CHECK(!scene->build_mesh(&source, &mesh) && mesh.count == (upper == RHH_SCENE_UPPER_HIDDEN ? 4 : 8));
        for (unsigned i = 0; i < mesh.count; ++i) CHECK(mesh.quads[i].tile == i + 100);
        CHECK(mesh.quads[0].vertex[0][0] == 32 && mesh.quads[0].vertex[0][1] == 48
            && mesh.quads[0].vertex[0][2] == -2);
        if (upper == RHH_SCENE_UPPER_BILLBOARD)
            CHECK(mesh.quads[4].kind == RHH_SCENE_QUAD_BILLBOARD && mesh.quads[4].vertex[0][1] == 56
                && mesh.quads[4].vertex[0][2] == 15 && mesh.quads[4].vertex[3][2] == 6.5f);
        if (upper == RHH_SCENE_UPPER_WALL_WEST) CHECK(mesh.quads[4].normal[0] == -1 && mesh.quads[4].vertex[0][0] == 32);
        if (upper == RHH_SCENE_UPPER_WALL_EAST) CHECK(mesh.quads[4].normal[0] == 1 && mesh.quads[4].vertex[0][0] == 48);
        if (upper == RHH_SCENE_UPPER_WALL_SOUTH) CHECK(mesh.quads[4].normal[1] == 1 && mesh.quads[4].vertex[0][1] == 64);
        if (upper == RHH_SCENE_UPPER_ROOF) CHECK(mesh.quads[4].normal[2] == 1 && mesh.quads[4].vertex[0][2] == 15);
    }
    unchangedMesh = mesh; source.style.upper = RHH_SCENE_UPPER_FLAT; source.style.upper_height = 1;
    CHECK(scene->build_mesh(&source, &mesh) && !memcmp(&mesh, &unchangedMesh, sizeof(mesh)));
    CHECK(scene->build_mesh(NULL, &mesh) && scene->build_mesh(&source, NULL));
    struct RhhSceneTile unchanged = sceneTile;
    CHECK(scene->tile_at(-1, 0, &sceneTile) && !memcmp(&sceneTile, &unchanged, sizeof(sceneTile)));
    sceneTile.struct_size = 1; unchanged = sceneTile;
    CHECK(scene->tile_at(0, 0, &sceneTile) && !memcmp(&sceneTile, &unchanged, sizeof(sceneTile)));
    CHECK(scene->tile_at(0, 0, NULL));
    sceneTile.struct_size = sizeof(sceneTile); unchanged = sceneTile;
    gMain.inBattle = TRUE;
    CHECK(scene->tile_at(0, 0, &sceneTile) && !memcmp(&sceneTile, &unchanged, sizeof(sceneTile)));
    gMain.inBattle = FALSE;
    CHECK(!memcmp(&sceneRng, &gRngValue, sizeof(sceneRng)));
    for (u32 i = 0; i < gBackupMapLayout.width * gBackupMapLayout.height; ++i) CHECK(!gBackupMapLayout.map[i]);
    REG_BG2CNT &= ~(3 << 2);
    memset(gNativeVram, 0, 32); gNativeVram[0] = 1;
    u16 blue = 31 << 10; memcpy(gNativePalette + 2, &blue, sizeof(blue));
    u8 tileRgba[8 * 8 * 4]; memset(tileRgba, 99, sizeof(tileRgba));
    CHECK(service->tile_rgba(0, tileRgba, sizeof(tileRgba) - 1) && tileRgba[0] == 99);
    CHECK(!service->tile_rgba(0, tileRgba, sizeof(tileRgba)));
    CHECK(tileRgba[0] == 0 && tileRgba[1] == 0 && tileRgba[2] == 255 && tileRgba[3] == 255 && tileRgba[7] == 0);
    CHECK(!service->tile_rgba(1 << 10, tileRgba, sizeof(tileRgba)) && tileRgba[3] == 0 && tileRgba[7 * 4 + 3] == 255);
    memset(&gObjectEvents[0], 0, sizeof(gObjectEvents[0]));
    memset(&gSprites[0], 0, sizeof(gSprites[0]));
    gObjectEvents[0].active = TRUE; gObjectEvents[0].isPlayer = TRUE;
    gObjectEvents[0].spriteId = 0; gObjectEvents[0].currentCoords.x = MAP_OFFSET + 3;
    gObjectEvents[0].currentCoords.y = MAP_OFFSET + 4;
    gSprites[0].inUse = TRUE; gSprites[0].x = 12; gSprites[0].y = 34;
    gSprites[0].coordOffsetEnabled = TRUE; gSpriteCoordOffsetX = 3; gSpriteCoordOffsetY = -2;
    gSprites[0].oam.paletteNum = 1; REG_DISPCNT |= 0x40;
    memset(gNativeVram + 0x10000, 0, sizeof(oldTile)); gNativeVram[0x10000] = 1;
    u16 red = 31; memcpy(gNativePalette + 0x222, &red, sizeof(red));
    struct RhhWorldActor actor = {.struct_size = sizeof(actor)};
    CHECK(!service->actor_at(0, &actor) && actor.x == 3 && actor.y == 4);
    CHECK(actor.screen_x == 15 && actor.screen_y == 32);
    CHECK((actor.flags & RHH_ACTOR_PLAYER) && actor.sprite_width == 8 && actor.sprite_height == 8);
    u8 rgba[8 * 8 * 4]; memset(rgba, 99, sizeof(rgba));
    CHECK(service->sprite_rgba(0, rgba, sizeof(rgba) - 1) && rgba[0] == 99);
    CHECK(!service->sprite_rgba(0, rgba, sizeof(rgba)));
    CHECK(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255 && rgba[7] == 0);
    gSprites[0].oam.matrixNum = ST_OAM_HFLIP;
    CHECK(!service->sprite_rgba(0, rgba, sizeof(rgba)) && rgba[3] == 0 && rgba[7 * 4 + 3] == 255);
    gSprites[0].oam.matrixNum = 0; gSprites[0].oam.bpp = 1;
    gNativeVram[0x10000] = 2;
    u16 green = 31 << 5; memcpy(gNativePalette + 0x204, &green, sizeof(green));
    CHECK(!service->sprite_rgba(0, rgba, sizeof(rgba)));
    CHECK(rgba[0] == 0 && rgba[1] == 255 && rgba[2] == 0 && rgba[3] == 255);
    gSprites[0].oam.bpp = 0; gSprites[0].oam.size = 1;
    gNativeVram[0x10000] = 1; gNativeVram[0x10020] = 1;
    u8 larger[16 * 16 * 4];
    CHECK(!service->sprite_rgba(0, larger, sizeof(larger)));
    CHECK(larger[3] == 255 && larger[8 * 4 + 3] == 255);
    /* Camera projection uses the engine's map-to-screen conversion, including
     * camera motion and panning. A jumping sprite's shadow anchor stays put. */
    gSaveBlock1Ptr->pos.x = 2; gSaveBlock1Ptr->pos.y = 3;
    gTotalCameraPixelOffsetX = 11; gTotalCameraPixelOffsetY = 6;
    gFieldCamera.x = 4; gFieldCamera.y = -5;
    struct RhhWorldCamera camera = {.struct_size = sizeof(camera)};
    struct RhhWorldCamera invalidCamera = {.struct_size = 1, .origin_x = 777};
    CHECK(service->camera(&invalidCamera) && invalidCamera.origin_x == 777 && service->camera(NULL));
    CHECK(!service->camera(&camera) && camera.tile_pixels == 16
        && camera.viewport_width == 240 && camera.viewport_height == 160
        && camera.origin_x == 84 && camera.origin_y == 45);
    gSprites[0].oam.size = 0; gSprites[0].centerToCornerVecX = -4;
    gSprites[0].centerToCornerVecY = -16; gSprites[0].x2 = 4; gSprites[0].y2 = -7;
    struct RhhWorldBillboard billboard = {.struct_size = sizeof(billboard)};
    struct RhhWorldBillboard invalidBillboard = {.struct_size = 1, .anchor_x = 777};
    CHECK(service->billboard(0, &invalidBillboard) && invalidBillboard.anchor_x == 777
        && service->billboard(OBJECT_EVENTS_COUNT, &billboard));
    CHECK(!service->billboard(0, &billboard) && billboard.part_count == 1
        && billboard.anchor_x == 15 && billboard.anchor_y == 48
        && billboard.world_anchor_x == -69 && billboard.world_anchor_y == 3
        && billboard.visual_center_x == 19 && billboard.visual_center_y == 25
        && billboard.offset_x == 4 && billboard.offset_y == -7);
    struct RhhWorldSpritePart part = {.struct_size = sizeof(part)};
    struct RhhWorldSpritePart invalidPart = {.struct_size = 1, .screen_x = 777};
    CHECK(service->sprite_part(0, 0, &invalidPart) && invalidPart.screen_x == 777
        && service->sprite_part(OBJECT_EVENTS_COUNT, 0, &part));
    CHECK(!service->sprite_part(0, 0, &part) && part.width == 8 && part.height == 8
        && part.screen_x == 15 && part.screen_y == 9);
    CHECK(service->sprite_part(0, 1, &part));
    struct Subsprite components[] = {
        {.x = -16, .y = -24, .shape = 0, .size = 0, .tileOffset = 0, .priority = 2},
        {.x = 0, .y = -24, .shape = 0, .size = 0, .tileOffset = 1, .priority = 1}
    };
    const struct SubspriteTable table[] = {{ARRAY_COUNT(components), components}};
    gSprites[0].subspriteTables = table; gSprites[0].subspriteMode = SUBSPRITES_ON;
    gSprites[0].oam.priority = 3; gSprites[0].subpriority = 12;
    gNativeVram[0x10000] = 1; gNativeVram[0x10020] = 2;
    memcpy(gNativePalette + 0x224, &green, sizeof(green));
    CHECK(!service->billboard(0, &billboard) && billboard.part_count == 2);
    CHECK(!service->sprite_part(0, 0, &part) && part.screen_x == 3 && part.screen_y == 1
        && part.priority == 2 && part.subpriority == 12);
    CHECK(!service->sprite_part(0, 1, &part) && part.screen_x == 19 && part.screen_y == 1 && part.priority == 1);
    memset(rgba, 99, sizeof(rgba));
    CHECK(service->sprite_part_rgba(0, 1, rgba, sizeof(rgba) - 1) && rgba[0] == 99);
    CHECK(!service->sprite_part_rgba(0, 1, rgba, sizeof(rgba))
        && rgba[0] == 0 && rgba[1] == 255 && rgba[2] == 0 && rgba[3] == 255);
    gSprites[0].oam.matrixNum = ST_OAM_HFLIP;
    CHECK(!service->sprite_part(0, 0, &part) && part.screen_x == 27);
    CHECK(!service->sprite_part(0, 1, &part) && part.screen_x == 11);
    CHECK(!service->sprite_part_rgba(0, 1, rgba, sizeof(rgba)) && !rgba[3] && rgba[7 * 4 + 3] == 255);
    struct RhhWorldSpriteTransform transform = {.struct_size = sizeof(transform)};
    struct RhhWorldSpriteTransform smallTransform = {.struct_size = 1, .a = 99}, savedTransform = smallTransform;
    u8 transformed[16 * 16 * 4]; memset(transformed, 77, sizeof(transformed));
    CHECK(service->sprite_part_transform(0, 1, &smallTransform) && !memcmp(&smallTransform, &savedTransform, sizeof(smallTransform))
        && service->sprite_part_transform(OBJECT_EVENTS_COUNT, 0, &transform)
        && service->sprite_part_transformed_rgba(0, 1, transformed, sizeof(rgba) - 1) && transformed[0] == 77);
    CHECK(!service->sprite_part_transform(0, 1, &transform) && transform.matrix_index == UINT32_MAX
        && transform.a == 256 && !transform.b && !transform.c && transform.d == 256
        && transform.source_width == 8 && transform.draw_width == 8 && transform.draw_height == 8
        && !service->sprite_part_transformed_rgba(0, 1, transformed, sizeof(transformed))
        && !memcmp(transformed, rgba, sizeof(rgba)));
    struct RhhWorldSpriteRasterOptions raster = {.struct_size = sizeof(raster), .flags = RHH_WORLD_RASTER_MOSAIC,
        .mosaic_width = 2, .mosaic_height = 1};
    u8 mosaicPixels[sizeof(transformed)];
    CHECK(!service->sprite_part_raster_rgba(0, 1, &raster, mosaicPixels, sizeof(mosaicPixels))
        && !memcmp(mosaicPixels, rgba, sizeof(rgba))); /* Part mosaic is disabled. */
    memset(mosaicPixels, 77, sizeof(mosaicPixels)); raster.mosaic_width = 0;
    CHECK(service->sprite_part_raster_rgba(0, 1, &raster, mosaicPixels, sizeof(mosaicPixels)) && mosaicPixels[0] == 77);
    raster.mosaic_width = 2; raster.reserved = 1;
    CHECK(service->sprite_part_raster_rgba(0, 1, &raster, mosaicPixels, sizeof(mosaicPixels)) && mosaicPixels[0] == 77);
    raster.reserved = 0; gSprites[0].oam.mosaic = 1;
    CHECK(service->sprite_part_raster_rgba(0, 1, NULL, mosaicPixels, sizeof(mosaicPixels))
        && service->sprite_part_raster_rgba(0, 1, &raster, mosaicPixels, sizeof(rgba) - 1) && mosaicPixels[0] == 77);
    CHECK(!service->sprite_part_raster_rgba(0, 1, &raster, mosaicPixels, sizeof(mosaicPixels))
        && mosaicPixels[7 * 4 + 1] == 255 && mosaicPixels[7 * 4 + 3] == 255 && !mosaicPixels[6 * 4 + 3]);
    ++gSprites[0].x;
    CHECK(!service->sprite_part_raster_rgba(0, 1, &raster, mosaicPixels, sizeof(mosaicPixels)) && !mosaicPixels[7 * 4 + 3]);
    --gSprites[0].x; --gSprites[0].y; raster.mosaic_height = 2;
    CHECK(!service->sprite_part_raster_rgba(0, 1, &raster, mosaicPixels, sizeof(mosaicPixels))
        && mosaicPixels[7 * 4 + 3] == 255 && mosaicPixels[(8 + 7) * 4 + 3] == 255
        && !mosaicPixels[(16 + 7) * 4 + 3]);
    ++gSprites[0].y; raster.mosaic_height = 1;
    u8 oldMosaicTile = gNativeVram[0x10023]; gNativeVram[0x10023] = 0x22; gSprites[0].x -= 12;
    CHECK(!service->sprite_part_raster_rgba(0, 1, &raster, mosaicPixels, sizeof(mosaicPixels))
        && !mosaicPixels[3] && mosaicPixels[7] == 255 && mosaicPixels[11] == 255);
    gSprites[0].x += 12; gNativeVram[0x10023] = oldMosaicTile; gSprites[0].oam.mosaic = 0;
    gSprites[0].subspriteMode = SUBSPRITES_IGNORE_PRIORITY;
    CHECK(!service->sprite_part(0, 1, &part) && part.priority == 3);
    gSprites[0].oam.affineMode = ST_OAM_AFFINE_NORMAL;
    gSprites[0].oam.objMode = ST_OAM_OBJ_BLEND; gSprites[0].oam.mosaic = 1;
    CHECK(!service->sprite_part(0, 1, &part)
        && (part.flags & (RHH_WORLD_PART_AFFINE | RHH_WORLD_PART_SEMITRANSPARENT | RHH_WORLD_PART_MOSAIC))
            == (RHH_WORLD_PART_AFFINE | RHH_WORLD_PART_SEMITRANSPARENT | RHH_WORLD_PART_MOSAIC));
    gOamMatrices[8] = (struct OamMatrix){0, -256, 256, 0};
    gSprites[0].oam.affineMode = ST_OAM_AFFINE_DOUBLE;
    CHECK(!service->sprite_part_transform(0, 1, &transform) && transform.matrix_index == 8
        && transform.a == 0 && transform.b == -256 && transform.c == 256 && transform.d == 0
        && transform.draw_width == 16 && transform.draw_height == 16
        && !service->sprite_part_transformed_rgba(0, 1, transformed, sizeof(transformed))
        && transformed[(12 * 16 + 4) * 4 + 1] == 255 && transformed[(12 * 16 + 4) * 4 + 3] == 255
        && transformed[3] == 0);
    raster.mosaic_width = 3; raster.mosaic_height = 1;
    CHECK(!service->sprite_part_raster_rgba(0, 1, &raster, mosaicPixels, sizeof(mosaicPixels))
        && mosaicPixels[(12 * 16 + 4) * 4 + 3] == 255 && mosaicPixels[(12 * 16 + 5) * 4 + 3] == 255
        && mosaicPixels[(12 * 16 + 6) * 4 + 3] == 255 && !mosaicPixels[(12 * 16 + 3) * 4 + 3]);
    struct RhhSceneBillboardTraceRequest transformedRay = {.struct_size = sizeof(transformedRay),
        .flags = RHH_SCENE_TRACE_ALPHA, .width = 16, .height = 16, .stride = 64,
        .rgba_size = sizeof(transformed), .rgba = transformed, .from = {-4, -5, 4}, .to = {-4, 5, 4},
        .corners = {{-8, 0, 16}, {8, 0, 16}, {8, 0, 0}, {-8, 0, 0}}};
    struct RhhSceneBillboardTraceResult transformedHit = {.struct_size = sizeof(transformedHit)};
    CHECK(!scene->trace_billboard(&transformedRay, &transformedHit) && transformedHit.status == RHH_SCENE_TRACE_BLOCKED
        && transformedHit.pixel_x == 4 && transformedHit.pixel_y == 12);
    transformed[(12 * 16 + 4) * 4 + 3] = 0;
    CHECK(!scene->trace_billboard(&transformedRay, &transformedHit) && transformedHit.status == RHH_SCENE_TRACE_CLEAR);
    transformed[(12 * 16 + 4) * 4 + 3] = 255;
    gSprites[0].template = gFieldEffectObjectTemplatePointers[FLDEFFOBJ_TALL_GRASS];
    struct RhhWorldSpriteTransform effectTransform = {.struct_size = sizeof(effectTransform)};
    u8 effectPixels[sizeof(transformed)];
    CHECK(!service->effect_part_transform(0, 1, &effectTransform)
        && !memcmp(&effectTransform, &transform, sizeof(transform))
        && !service->effect_part_transformed_rgba(0, 1, effectPixels, sizeof(effectPixels))
        && !memcmp(effectPixels, transformed, sizeof(transformed))
        && !service->effect_part_raster_rgba(0, 1, &raster, effectPixels, sizeof(effectPixels))
        && !memcmp(effectPixels, mosaicPixels, sizeof(effectPixels))
        && service->effect_part_transform(MAX_SPRITES, 0, &effectTransform));
    gSprites[0].oam.affineMode = ST_OAM_AFFINE_NORMAL;
    gSprites[0].oam.objMode = ST_OAM_OBJ_WINDOW;
    CHECK(!service->sprite_part(0, 1, &part) && (part.flags & RHH_WORLD_PART_WINDOW));
    gSprites[0].oam.affineMode = ST_OAM_AFFINE_ERASE;
    CHECK(!service->sprite_part(0, 1, &part) && (part.flags & RHH_WORLD_PART_DISABLED)
        && !(part.flags & RHH_WORLD_PART_AFFINE));
    memset(transformed, 77, sizeof(transformed));
    CHECK(!service->sprite_part_transformed_rgba(0, 1, transformed, sizeof(transformed)));
    for (u32 i = 0; i < sizeof(rgba); ++i) CHECK(!transformed[i]);
    CHECK(service->sprite_part(0, 2, &part) && service->sprite_part_rgba(0, 2, rgba, sizeof(rgba)));
    free(gBackupMapLayout.map); gBackupMapLayout = oldGrid; gMapHeader = oldHeader;
    gObjectEvents[0] = oldActor; gSprites[0] = oldSprite; gOamMatrices[8] = oldMatrix; REG_DISPCNT = oldDisp;
    gSaveBlock1Ptr->pos = oldPos; gSaveBlock1Ptr->location = oldLocation; gFieldCamera = oldCamera;
    gTotalCameraPixelOffsetX = oldCameraX; gTotalCameraPixelOffsetY = oldCameraY;
    gSpriteCoordOffsetX = oldOffsetX; gSpriteCoordOffsetY = oldOffsetY;
    REG_BG2CNT = oldBg2; memcpy(gNativeVram, oldBgTile, sizeof(oldBgTile));
    memcpy(gNativePalette, oldBgPalette, sizeof(oldBgPalette));
    memcpy(gNativeVram + 0x10000, oldTile, sizeof(oldTile));
    memcpy(gNativePalette + 0x200, oldPalette, sizeof(oldPalette));
    puts("overworld service: map, camera, shadow anchors, subsprite frames and transparent pixels passed");
}
static void CheckActualSceneMesh(void)
{
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE103)][MAP_NUM(MAP_ROUTE103)];
    const struct MapLayout *layout = gMapHeader.mapLayout; CHECK(layout && layout->map);
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE103); gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE103);
    gBackupMapLayout.width = layout->width + MAP_OFFSET_W; gBackupMapLayout.height = layout->height + MAP_OFFSET_H;
    gBackupMapLayout.map = calloc(gBackupMapLayout.width * gBackupMapLayout.height, sizeof(u16)); CHECK(gBackupMapLayout.map);
    for (int i = 0; i < gBackupMapLayout.width * gBackupMapLayout.height; ++i)
        gBackupMapLayout.map[i] = MAPGRID_UNDEFINED;
    for (int y = 0; y < layout->height; ++y)
        memcpy(gBackupMapLayout.map + (y + MAP_OFFSET) * gBackupMapLayout.width + MAP_OFFSET,
               layout->map + y * layout->width, layout->width * sizeof(u16));
    const struct MapLayout *adjacent = gMapGroups[MAP_GROUP(MAP_OLDALE_TOWN)][MAP_NUM(MAP_OLDALE_TOWN)]->mapLayout;
    CHECK(adjacent && adjacent->map && adjacent->width > 0 && adjacent->height >= MAP_OFFSET);
    int connectionWidth = adjacent->width < layout->width ? adjacent->width : layout->width;
    for (int y = 0; y < MAP_OFFSET; ++y)
        memcpy(gBackupMapLayout.map + (y + layout->height + MAP_OFFSET) * gBackupMapLayout.width + MAP_OFFSET,
               adjacent->map + y * adjacent->width, connectionWidth * sizeof(u16));
    const struct RhhWorldSceneService *scene = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/world_scene", RHH_SERVICE_VERSION(1, 5));
    CHECK(scene && scene->light_count() == 4 && scene->shadow_caster_count() >= 1); rng_value_t rng = gRngValue; unsigned nonzero = 0, water = 0;
    struct RhhSceneWindow window = {.struct_size = sizeof(window)};
    CHECK(!scene->window(&window) && window.map == RhhContentKeyForId(RHH_MAP, MAP_ROUTE103)
        && window.min_x == -MAP_OFFSET && window.min_y == -MAP_OFFSET
        && window.max_x == layout->width + MAP_OFFSET && window.max_y == layout->height + MAP_OFFSET - 1);
    struct RhhSceneTile border = {.struct_size = sizeof(border)}, neighbor = {.struct_size = sizeof(neighbor)};
    CHECK(!scene->window_tile_at(-1, 0, &border) && border.x == -1 && border.y == 0 && !border.rule
        && border.tile.metatile == MapGridGetMetatileIdAt(MAP_OFFSET - 1, MAP_OFFSET));
    struct MapLayout invalidBorder = *layout; struct RhhSceneTile unchangedBorder = border;
    invalidBorder.isFrlg = TRUE; invalidBorder.borderWidth = 0; gMapHeader.mapLayout = &invalidBorder;
    CHECK(scene->window_tile_at(-1, 0, &border) && !memcmp(&border, &unchangedBorder, sizeof(border)));
    invalidBorder.isFrlg = FALSE; invalidBorder.border = NULL;
    CHECK(scene->window_tile_at(-1, 0, &border) && !memcmp(&border, &unchangedBorder, sizeof(border)));
    gMapHeader.mapLayout = layout;
    CHECK(!scene->window_tile_at(0, layout->height, &neighbor) && !neighbor.rule
        && neighbor.tile.metatile == UNPACK_METATILE(adjacent->map[0]));
    struct RhhSceneWindowTile resolved = {.struct_size = sizeof(resolved)};
    CHECK(!scene->window_resolve(0, 0, &resolved) && resolved.origin == RHH_SCENE_WINDOW_CURRENT
        && resolved.source_map == RhhContentKeyForId(RHH_MAP, MAP_ROUTE103) && resolved.source_x == 0 && resolved.source_y == 0);
    struct RhhSceneWindowTile savedResolved = resolved;
    CHECK(scene->window_resolve(window.min_x - 1, 0, &resolved) && !memcmp(&resolved, &savedResolved, sizeof(resolved)));
    CHECK(!scene->window_resolve(-1, 0, &resolved) && resolved.origin == RHH_SCENE_WINDOW_BORDER && !resolved.source_map);
    CHECK(!scene->window_resolve(2, layout->height, &resolved)
        && resolved.origin == RHH_SCENE_WINDOW_CONNECTED && resolved.source_map == RhhContentKeyForId(RHH_MAP, MAP_OLDALE_TOWN)
        && resolved.source_x == 2 && resolved.source_y == 0 && resolved.scene.x == 2 && resolved.scene.y == layout->height
        && resolved.scene.rule == RhhContentKeyOf("test:scene/oldale_link") && resolved.scene.style.ground_height == 8);
    struct RhhSceneLightBatch lights = {.struct_size = sizeof(lights)};
    CHECK(!scene->lights_at(1, 1, RHH_TIME_DAY, &lights) && lights.count == 1
        && lights.lights[0].id == RhhContentKeyOf("test:light/route")
        && lights.lights[0].x == 24 && lights.lights[0].y == 21 && lights.lights[0].z == 12);
    CHECK(!scene->lights_at(2, layout->height, RHH_TIME_DAY, &lights) && !lights.count);
    CHECK(!scene->lights_at(2, layout->height, RHH_TIME_NIGHT, &lights) && lights.count == 1
        && lights.lights[0].id == RhhContentKeyOf("test:light/oldale")
        && lights.lights[0].source_map == RhhContentKeyForId(RHH_MAP, MAP_OLDALE_TOWN)
        && lights.lights[0].x == 37 && lights.lights[0].y == layout->height * 16 + 3 && lights.lights[0].z == 20
        && lights.lights[0].radius == 40 && lights.lights[0].rgb == 0x55aaff && lights.lights[0].intensity == 500);
    CHECK(!scene->lights_at(2, layout->height, 0, &lights) && !lights.count);
    CHECK(!scene->lights_at(-1, 0, RHH_TIME_NIGHT, &lights) && !lights.count);
    struct RhhSceneLightBatch savedLights = lights;
    CHECK(scene->lights_at(2, layout->height, RHH_TIME_NIGHT + 1, &lights) && !memcmp(&lights, &savedLights, sizeof(lights)));
    CHECK(scene->lights_at(window.min_x - 1, 0, RHH_TIME_NIGHT, &lights) && !memcmp(&lights, &savedLights, sizeof(lights)));
    lights.struct_size = 1; savedLights = lights;
    CHECK(scene->lights_at(1, 1, RHH_TIME_DAY, &lights) && !memcmp(&lights, &savedLights, sizeof(lights)));
    lights.struct_size = sizeof(lights);
    /* Trace the north wall of Oldale across the real connection. */
    struct RhhSceneTraceRequest trace = {.struct_size = sizeof(trace),
        .from = {3 * 16 + 4, layout->height * 16 - 8, 20}, .to = {3 * 16 + 4, layout->height * 16 + 8, 20}};
    struct RhhSceneTraceResult hit = {.struct_size = sizeof(hit)};
    CHECK(!scene->trace_light(&trace, &hit) && hit.status == RHH_SCENE_TRACE_BLOCKED
        && hit.tile_x == 3 && hit.tile_y == layout->height && hit.quad_kind == RHH_SCENE_QUAD_UPPER
        && hit.fraction_millionths == 500000);
    struct RhhSceneMesh traceWall = {.struct_size = sizeof(traceWall)};
    CHECK(!scene->window_resolved_mesh_at(3, layout->height, &traceWall) && hit.quad_index < traceWall.count);
    uint16_t shadowTile = traceWall.quads[hit.quad_index].tile;
    unsigned shadowBase = ((REG_BG2CNT >> 2) & 3) * 0x4000 + (shadowTile & 1023) * 32;
    u8 savedShadowTile[32]; memcpy(savedShadowTile, gNativeVram + shadowBase, sizeof(savedShadowTile));
    memset(gNativeVram + shadowBase, 0, 32); trace.flags = RHH_SCENE_TRACE_ALPHA;
    CHECK(!scene->trace_light(&trace, &hit) && hit.status == RHH_SCENE_TRACE_CLEAR);
    unsigned sampleX = shadowTile & (1 << 10) ? 3 : 4, sampleY = shadowTile & (1 << 11) ? 1 : 6;
    gNativeVram[shadowBase + sampleY * 4 + sampleX / 2] = 1u << ((sampleX & 1) * 4);
    CHECK(!scene->trace_light(&trace, &hit) && hit.status == RHH_SCENE_TRACE_BLOCKED && hit.fraction_millionths == 500000);
    trace.from[2] = trace.to[2] = 40;
    CHECK(!scene->trace_light(&trace, &hit) && hit.status == RHH_SCENE_TRACE_CLEAR);
    trace.from[2] = trace.to[2] = 20; trace.flags = 0;
    int32_t swapY = trace.from[1]; trace.from[1] = trace.to[1]; trace.to[1] = swapY;
    CHECK(!scene->trace_light(&trace, &hit) && hit.status == RHH_SCENE_TRACE_BLOCKED && hit.fraction_millionths == 500000);
    trace.from[0] = trace.to[0] = 4 * 16 + 4;
    CHECK(!scene->trace_light(&trace, &hit) && hit.status == RHH_SCENE_TRACE_CLEAR); /* sight-only wall */
    trace.from[0] = trace.to[0] = 5 * 16 + 4;
    CHECK(!scene->trace_light(&trace, &hit) && hit.status == RHH_SCENE_TRACE_BLOCKED); /* shadow-only wall */
    trace.from[0] = trace.to[0] = 3 * 16 + 4;
    CHECK(!scene->trace_light(&trace, &hit) && hit.status == RHH_SCENE_TRACE_BLOCKED);
    struct RhhSceneTraceResult savedHit = hit; trace.to[0] = trace.from[0] + 513;
    CHECK(scene->trace_light(&trace, &hit) && !memcmp(&hit, &savedHit, sizeof(hit)));
    trace.to[0] = trace.from[0]; trace.flags = 2;
    CHECK(scene->trace_light(&trace, &hit) && !memcmp(&hit, &savedHit, sizeof(hit)));
    memcpy(gNativeVram + shadowBase, savedShadowTile, sizeof(savedShadowTile));
    struct RhhSceneTile legacy = {.struct_size = sizeof(legacy)};
    CHECK(!scene->window_tile_at(2, layout->height, &legacy) && !legacy.rule && legacy.style.ground_height == 0);
    struct RhhSceneMesh resolvedMesh = {.struct_size = sizeof(resolvedMesh)};
    CHECK(!scene->window_resolved_mesh_at(2, layout->height, &resolvedMesh) && resolvedMesh.scene.rule == resolved.scene.rule);
    bool32 northEdge = FALSE;
    for (unsigned i = 0; i < resolvedMesh.count; ++i)
        if (resolvedMesh.quads[i].kind == RHH_SCENE_QUAD_GROUND_EDGE && resolvedMesh.quads[i].normal[1] == -1.f)
            northEdge = TRUE;
    CHECK(northEdge);
    u16 *resolvedBlock = &gBackupMapLayout.map[(layout->height + MAP_OFFSET) * gBackupMapLayout.width + MAP_OFFSET + 2];
    u16 savedBlock = *resolvedBlock; *resolvedBlock = (savedBlock & ~MAPGRID_METATILE_ID_MASK) | (UNPACK_METATILE(savedBlock) ? 0 : 1);
    CHECK(!scene->window_resolve(2, layout->height, &resolved) && resolved.origin == RHH_SCENE_WINDOW_UNKNOWN
        && !resolved.source_map && !resolved.scene.rule);
    CHECK(!scene->lights_at(2, layout->height, RHH_TIME_NIGHT, &lights) && !lights.count);
    *resolvedBlock = savedBlock;
    const struct MapConnections *originalConnections = gMapHeader.connections;
    struct MapConnection overlapping[] = {
        {.direction = CONNECTION_SOUTH, .offset = 0, .mapGroup = MAP_GROUP(MAP_OLDALE_TOWN), .mapNum = MAP_NUM(MAP_OLDALE_TOWN)},
        {.direction = CONNECTION_SOUTH, .offset = 1, .mapGroup = MAP_GROUP(MAP_OLDALE_TOWN), .mapNum = MAP_NUM(MAP_OLDALE_TOWN)}
    };
    struct MapConnections testConnections = {.count = 2, .connections = overlapping};
    gMapHeader.connections = &testConnections; *resolvedBlock = adjacent->map[1];
    CHECK(!scene->window_resolve(2, layout->height, &resolved) && resolved.origin == RHH_SCENE_WINDOW_CONNECTED
        && resolved.source_x == 1 && !resolved.scene.rule);
    const struct MapLayout *incompatible = gMapGroups[MAP_GROUP(MAP_SLATEPORT_CITY)][MAP_NUM(MAP_SLATEPORT_CITY)]->mapLayout;
    CHECK(incompatible && incompatible->map && incompatible->width > 2
        && incompatible->secondaryTileset != layout->secondaryTileset);
    overlapping[1].offset = 0; overlapping[1].mapGroup = MAP_GROUP(MAP_SLATEPORT_CITY);
    overlapping[1].mapNum = MAP_NUM(MAP_SLATEPORT_CITY); *resolvedBlock = incompatible->map[2];
    CHECK(!scene->window_resolve(2, layout->height, &resolved) && resolved.origin == RHH_SCENE_WINDOW_UNKNOWN
        && !resolved.source_map && !resolved.scene.rule);
    *resolvedBlock = savedBlock; gMapHeader.connections = originalConnections;
    struct RhhSceneMesh borderMesh = {.struct_size = sizeof(borderMesh)};
    CHECK(!scene->window_mesh_at(-1, 0, &borderMesh) && borderMesh.scene.x == -1
        && borderMesh.quads[0].vertex[0][0] == -16 && borderMesh.count == 8);
    struct RhhSceneMesh savedMesh = borderMesh;
    CHECK(scene->window_mesh_at(window.min_x - 1, 0, &borderMesh) && !memcmp(&borderMesh, &savedMesh, sizeof(savedMesh))
        && scene->mesh_at(-1, 0, &borderMesh));
    u16 *loadedNeighbor = &gBackupMapLayout.map[(layout->height + MAP_OFFSET) * gBackupMapLayout.width + MAP_OFFSET];
    u16 oldNeighbor = *loadedNeighbor;
    *loadedNeighbor = (oldNeighbor & ~MAPGRID_METATILE_ID_MASK) | (UNPACK_METATILE(oldNeighbor) ? 0 : 1);
    CHECK(!scene->window_tile_at(0, layout->height, &neighbor) && neighbor.tile.metatile == UNPACK_METATILE(*loadedNeighbor));
    *loadedNeighbor = oldNeighbor;
    for (int y = 0; y < layout->height; ++y)
        for (int x = 0; x < layout->width; ++x)
        {
            struct RhhSceneMesh mesh = {.struct_size = sizeof(mesh)};
            CHECK(!scene->mesh_at(x, y, &mesh) && mesh.count == 8 && mesh.scene.x == x && mesh.scene.y == y);
            for (unsigned i = 0; i < 8; ++i)
            {
                CHECK(mesh.quads[i].tile == mesh.scene.tile.tiles[i]);
                nonzero += mesh.quads[i].tile != 0;
            }
            struct RhhSceneInference inferred = {.struct_size = sizeof(inferred)};
            CHECK(!scene->window_infer(x, y, &inferred));
            if (inferred.flags & RHH_SCENE_INFERRED_WATER)
            { CHECK(inferred.tile.scene.style.surface == RHH_SCENE_WATER); ++water; }
            CHECK(gBackupMapLayout.map[(y + MAP_OFFSET) * gBackupMapLayout.width + x + MAP_OFFSET]
                == layout->map[y * layout->width + x]);
        }
    CHECK(nonzero && water && !memcmp(&rng, &gRngValue, sizeof(rng)));
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    /* Let the actual map loader populate its static grid and connection cells. */
    struct BackupMapLayout previousGrid = gBackupMapLayout;
    InitMap(); CHECK(gBackupMapLayout.map && gBackupMapLayout.width == layout->width + MAP_OFFSET_W);
    u16 actualConnected = gBackupMapLayout.map[(layout->height + MAP_OFFSET) * gBackupMapLayout.width + MAP_OFFSET];
    CHECK(actualConnected == adjacent->map[0]);
    CHECK(!scene->lights_at(2, layout->height, RHH_TIME_NIGHT, &lights) && lights.count == 1
        && lights.lights[0].id == RhhContentKeyOf("test:light/oldale"));
    CHECK(!scene->window_resolve(2, layout->height, &resolved) && resolved.origin == RHH_SCENE_WINDOW_CONNECTED
        && resolved.source_x == 2 && resolved.scene.style.ground_height == 8);
    CHECK(!scene->window_tile_at(0, layout->height, &neighbor)
        && neighbor.tile.metatile == MapGridGetMetatileIdAt(MAP_OFFSET, layout->height + MAP_OFFSET));
    gBackupMapLayout = previousGrid;
    puts("scene mesh: actual Route 103 map tiles and pure queries passed");
}
static void CheckSavedNuzlocke(const char *path)
{
    CHECK(NativeSaveOpen(path) == 0); gFlashMemoryPresent = TRUE;
    CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
    CHECK(ModsRunFailed() && ModsMonIsDead(&gParties[B_TRAINER_PLAYER][0]));
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP) == 0);
    NativeSaveClose();
    puts("Nuzlocke death and failed-run journal survived a fresh process");
}
static void CheckMods(const char *path)
{
    InitModsForTest();
    CheckSpeedSettings();
    CheckOwnedMonService();
    CheckMonEditService();
    CheckDaycareService();
    CheckBreedingHooks();
    CheckEggHatch();
    CheckWorldClock();
    CheckWorldEnvironment();
    CheckOverworldService();
    CheckExampleMigrations();
    CHECK(ModsGetConfig().flags == 0 && ModsGetConfig().wildMode == 0 && ModsGetConfig().music == 0);
    CHECK(RhhContentKeyForId(RHH_TRAINER, TRAINER_SAWYER_1) != 0);
    CHECK(RhhContentKeyForId(RHH_TRAINER, TRAINER_SAWYER_1) != RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1));
    CHECK(RhhContentKeyForId(RHH_MUSIC, MUS_VS_WILD) != 0);
    rng_value_t rng = gRngValue;
    CHECK(ModsWildSpecies(SPECIES_POOCHYENA, 0, 0, 0) == SPECIES_POOCHYENA);
    CHECK(GetStarterPokemon(1) == SPECIES_TORCHIC && GetSpeciesBaseStatTotal(SPECIES_TORCHIC) == 310);
    CHECK(ModsStarterCount() > 900 && ModsStarterEligible(SPECIES_DRAGONITE));
    CHECK(!ModsStarterEligible(SPECIES_MEWTWO) && !ModsStarterEligible(SPECIES_MEW));
    CHECK(!ModsStarterEligible(SPECIES_CHARIZARD_MEGA_X));
    gModDraft = (struct ModConfig){.flags = MOD_EQUALIZE | MOD_LEVEL_CAP | MOD_TRAINERS,
        .wildMode = MOD_WILD_POOL, .starter = RhhContentKeyForId(RHH_SPECIES, SPECIES_DRAGONITE), .seed = 123456};
    ModsNewGame(7);
    CHECK(GetStarterPokemon(1) == SPECIES_DRAGONITE && GetStarterPokemon(0) == SPECIES_TREECKO);
    for (u32 species = 1; species < NUM_SPECIES; ++species)
    {
        if (!gSpeciesInfo[species].baseHP) continue;
        CHECK(GetSpeciesBaseStatTotal(species) == 600);
        for (u32 stat = 0; stat < 6; ++stat)
            CHECK(GetSpeciesBaseStat(species, stat) >= 1 && GetSpeciesBaseStat(species, stat) <= 255);
    }
    CHECK(GetSpeciesBaseHP(SPECIES_SHEDINJA) == 1);
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE101);
    gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE101);
    u32 species = ModsWildSpecies(SPECIES_POOCHYENA, 0, 3, 0);
    for (u32 i = 0; i < 100; ++i) CHECK(ModsWildSpecies(SPECIES_ZIGZAGOON, 0, 3, 0) == species);
    for (u32 slot = 0; slot < 6; ++slot)
        CHECK(ModsTrainerSpecies(SPECIES_ZIGZAGOON, TRAINER_SAWYER_1, slot)
            == ModsTrainerSpecies(SPECIES_POOCHYENA, TRAINER_SAWYER_1, slot));
    CHECK(!memcmp(&gRngValue, &rng, sizeof(rng)));
    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    const struct Trainer *trainer = GetTrainerStructFromId(TRAINER_SAWYER_1);
    CreateNPCTrainerPartyFromTrainer(gParties[B_TRAINER_OPPONENT_A], trainer);
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES)
        == ModsTrainerSpecies(trainer->party[0].species, TRAINER_SAWYER_1, 0));
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_MOVE1) != MOVE_NONE);
    const u16 leaders[] = {TRAINER_ROXANNE_1, TRAINER_BRAWLY_1, TRAINER_WATTSON_1, TRAINER_FLANNERY_1,
        TRAINER_NORMAN_1, TRAINER_WINONA_1, TRAINER_TATE_AND_LIZA_1, TRAINER_JUAN_1, TRAINER_WALLACE};
    for (u32 i = 0; i < 8; ++i) FlagClear(FLAG_BADGE01_GET + i);
    for (u32 badge = 0; badge <= 8; ++badge)
    {
        const struct Trainer *leader = GetTrainerStructFromId(leaders[badge]);
        u32 ace = 1;
        for (u32 i = 0; i < leader->partySize; ++i) ace = max(ace, leader->party[i].lvl);
        CHECK(GetCurrentLevelCap() == ace);
        if (badge < 8) FlagSet(FLAG_BADGE01_GET + badge);
    }
    for (u32 i = 0; i < 8; ++i) FlagClear(FLAG_BADGE01_GET + i);
    u32 cap = GetCurrentLevelCap();
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 80, 123,
        OTID_STRUCT_PRESET(456), 31);
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL) == cap);
    CHECK(ExecuteTableBasedItemEffect(&gParties[B_TRAINER_PLAYER][0], ITEM_RARE_CANDY, 0, 0) == TRUE);
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL) == cap);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_TORCHIC, 80, 123,
        OTID_STRUCT_PRESET(456), 31);
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_LEVEL) == 80);
    rng = gRngValue;
    gModDraft.wildMode = MOD_WILD_FULL; ModsNewGame(7);
    u8 *snapshot; u32 length, draws[32];
    CHECK(RhhModsExport(&snapshot, &length) == 0);
    for (u32 i = 0; i < ARRAY_COUNT(draws); ++i)
    { draws[i] = ModsWildSpecies(SPECIES_POOCHYENA, 0, 3, 0); ModsWildCreated(draws[i], 0, 3, 0); }
    bool32 different = FALSE;
    for (u32 i = 1; i < ARRAY_COUNT(draws); ++i) different |= draws[i] != draws[0];
    CHECK(different && RhhModsImport(snapshot, length) == 0);
    for (u32 i = 0; i < ARRAY_COUNT(draws); ++i)
    { CHECK(ModsWildSpecies(SPECIES_POOCHYENA, 0, 3, 0) == draws[i]); ModsWildCreated(draws[i], 0, 3, 0); }
    free(snapshot); CHECK(!memcmp(&gRngValue, &rng, sizeof(rng)));
    u32 header = GetCurrentMapWildMonHeaderId(); CHECK(header != HEADER_NONE);
    const struct WildPokemonInfo *wild = gWildMonHeaders[header].encounterTypes[TIME_OF_DAY_DEFAULT].landMonsInfo;
    CHECK(wild != NULL);
    u32 firstWild = SPECIES_NONE; bool32 varied = FALSE;
    for (u32 i = 0; i < 16; ++i)
    {
        u32 expected = ModsWildSpecies(SPECIES_POOCHYENA, WILD_AREA_LAND, 0, 0);
        CHECK(TryGenerateWildMon(wild, WILD_AREA_LAND, 0));
        u32 actual = GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES);
        CHECK(actual == expected);
        if (!i) firstWild = actual; else varied |= actual != firstWild;
    }
    CHECK(varied); /* Actual creation, rather than queries, advances full mode. */
    puts("mods: all species reach 600 BST; stable pools/trainers, full RNG save/reload, starters and all Gym/Champion caps passed");

    gModDraft = (struct ModConfig){.flags = MOD_NUZLOCKE, .seed = 987}; ModsNewGame(7);
    gBattleTypeFlags = 0; ClearBag();
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 123,
        OTID_STRUCT_PRESET(456), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    u16 hp = 0; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP, &hp);
    CHECK(!ModsMonIsDead(&gParties[B_TRAINER_PLAYER][0])); /* before balls */
    hp = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MAX_HP);
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP, &hp);
    CHECK(AddBagItem(ITEM_POKE_BALL, 5));
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE101)][MAP_NUM(MAP_ROUTE101)];
    ModsEncounterStarted(); CHECK(ModsCanCapture());
    if (path)
    {
        CHECK(NativeSaveOpen(path) == 0); /* opening resets state; recreate this unsaved run */
        ModsNewGame(7); ModsEncounterStarted();
        gFlashMemoryPresent = TRUE; Save_ResetSaveCounters();
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
    }
    hp = 0; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP, &hp);
    CHECK(ModsMonIsDead(&gParties[B_TRAINER_PLAYER][0]));
    hp = 100; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP, &hp);
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP) == 0);
    CHECK(ExecuteTableBasedItemEffect(&gParties[B_TRAINER_PLAYER][0], ITEM_MAX_REVIVE, 0, 0) == TRUE);
    HealPlayerParty(); CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP) == 0);
    gBattlerPartyIndexes[0] = 0; gBattleTypeFlags = 0;
    CHECK(GetFirstFaintedPartyIndex(0) == PARTY_SIZE); /* Revival Blessing cannot target the dead. */
    struct Pokemon boxed;
    BoxMonToMon(&gParties[B_TRAINER_PLAYER][0].box, &boxed);
    CHECK(GetMonData(&boxed, MON_DATA_HP) == 0);
    ModsEncounterStarted(); CHECK(!ModsCanCapture());
    if (path)
    {
        NativeSaveClose(); CHECK(NativeSaveOpen(path) == 0);
        ZeroPlayerPartyMons(); CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
        CHECK(ModsMonIsDead(&gParties[B_TRAINER_PLAYER][0]) && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP) == 0);
        ModsEncounterStarted(); CHECK(!ModsCanCapture());
        ModsWhiteout(); CHECK(ModsRunFailed()); NativeSaveClose();
    }
    puts("Nuzlocke: grace period, encounter consumption, permanent fainting, revive refusal, PC conversion and journal reload passed");
}

static void CheckMusicInstruments(const char *root, u32 mode)
{
    InitModsForTest(); NativeMusicSetAssetRoot(root);
    const struct RhhMusicService *music = (const void *)RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 8));
    struct RhhMusicInstrumentInspection view = {.struct_size = sizeof(view)};
    struct RhhMusicNoteCoverage coverage = {.struct_size = sizeof(coverage)};
    struct RhhMusicNoteCoverage smallCoverage = {.struct_size = 1, .total_notes = 99}, oldCoverage = smallCoverage;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(music && music->inspect_note_coverage(0, &coverage)
        && music->inspect_note_coverage(RhhContentKeyOf(sTestMusic[0].id), &smallCoverage)
        && !memcmp(&smallCoverage, &oldCoverage, sizeof(smallCoverage))
        && !music->inspect_note_coverage(RhhContentKeyOf(sTestMusic[0].id), &coverage)
        && coverage.kind == RHH_MUSIC_TRACK_DECLARED && coverage.availability == RHH_MUSIC_READY
        && coverage.flags & RHH_MUSIC_COVERAGE_MIDI
        && coverage.total_notes == coverage.covered_notes + coverage.uncovered_notes);
    if (mode == 2)
    {
        CHECK(coverage.total_notes == 129 && coverage.covered_notes == 61 && coverage.uncovered_notes == 68
            && coverage.count == RHH_MUSIC_NOTE_GAP_LIMIT && (coverage.flags & RHH_MUSIC_COVERAGE_TRUNCATED)
            && coverage.unlisted_uncovered_notes == coverage.uncovered_notes - 65
            && coverage.gaps[0].channel == 9 && coverage.gaps[0].key == 0 && coverage.gaps[0].velocity == 64
            && coverage.gaps[0].requested_bank == 128 && coverage.gaps[0].selected_bank == 128
            && coverage.gaps[0].selected_program == 0 && coverage.gaps[0].notes == 2);
        CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        NativeMusicSetAssetRoot(NULL); RhhModsUnloadLibraries();
        puts("music instruments: source presets, bank changes, fallback and bounds passed"); return;
    }
    CHECK(music && !music->inspect_instruments(RhhContentKeyOf(sTestMusic[0].id), &view)
        && view.availability == RHH_MUSIC_READY && view.kind == RHH_MUSIC_TRACK_DECLARED
        && view.backend == RHH_MUSIC_BACKEND_MIDI && (view.flags & RHH_MUSIC_INSTRUMENT_MIDI));
    if (mode == 1)
    {
        CHECK(view.flags & RHH_MUSIC_INSTRUMENT_TRUNCATED);
        CHECK(view.count == RHH_MUSIC_INSTRUMENT_LIMIT && view.total_notes == 130 && view.unlisted_notes == 1
            && view.uses[0].requested_program == 0 && view.uses[0].notes == 2);
    }
    else
    {
        CHECK(view.count == 4 && view.total_notes == 6 && view.fallback_notes == 4 && !view.unlisted_notes);
        CHECK(view.uses[0].channel == 0 && view.uses[0].requested_bank == 0 && view.uses[0].requested_program == 0
            && view.uses[0].selected_bank == 0 && view.uses[0].selected_program == 0 && !view.uses[0].flags
            && view.uses[0].notes == 2 && view.uses[0].preset_name[0]);
        CHECK(view.uses[1].channel == 0 && view.uses[1].requested_bank == 0 && view.uses[1].requested_program == 1
            && view.uses[1].selected_program != 1 && view.uses[1].flags == RHH_MUSIC_INSTRUMENT_FALLBACK && view.uses[1].notes == 2);
        CHECK(view.uses[2].channel == 0 && view.uses[2].requested_bank == 1 && view.uses[2].requested_program == 0
            && view.uses[2].selected_bank == 0 && view.uses[2].flags == RHH_MUSIC_INSTRUMENT_FALLBACK);
        CHECK(view.uses[3].channel == 9 && view.uses[3].requested_bank == 128 && view.uses[3].requested_program == 127
            && view.uses[3].selected_bank == 128 && view.uses[3].selected_program == 0
            && view.uses[3].flags == RHH_MUSIC_INSTRUMENT_FALLBACK);
    }
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    NativeMusicSetAssetRoot(NULL); RhhModsUnloadLibraries();
    puts("music instruments: source presets, bank changes, fallback and bounds passed");
}

static void CheckMusic(const char *root)
{
    InitModsForTest(); gModDraft = (struct ModConfig){0}; ModsNewGame(99);
    NativeMusicSetAssetRoot(root); cgb_audio_init(42060); m4aSoundInit();
    unsigned oldSound = gSaveBlock2Ptr->optionsSound;
    gSaveBlock2Ptr->optionsSound = OPTIONS_SOUND_STEREO;
    const struct RhhMusicService *music = (const void *)RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 0));
    CHECK(music && music->header.struct_size >= RHH_MUSIC_SERVICE_V1_1_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 1))
        && RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 2))
        && RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 3))
        && RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 4))
        && RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 5))
        && RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 6))
        && RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 7))
        && RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 8))
        && !RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 9))
        && RHH_MUSIC_SERVICE_V1_3_SIZE < RHH_MUSIC_SERVICE_V1_4_SIZE
        && RHH_MUSIC_SERVICE_V1_4_SIZE < RHH_MUSIC_SERVICE_V1_5_SIZE
        && RHH_MUSIC_SERVICE_V1_5_SIZE < RHH_MUSIC_SERVICE_V1_6_SIZE
        && RHH_MUSIC_SERVICE_V1_6_SIZE < RHH_MUSIC_SERVICE_V1_7_SIZE
        && RHH_MUSIC_SERVICE_V1_7_SIZE < RHH_MUSIC_SERVICE_V1_8_SIZE
        && music->count() == ARRAY_COUNT(sTestMusic));
    struct RhhMusicPlayback playback = {.struct_size = sizeof(playback), .track = 999};
    struct RhhMusicGainContext gainOutside = {.struct_size = sizeof(gainOutside), .track = 999};
    CHECK(music->playback(&playback) && playback.track == 999
        && music->gain_context(&gainOutside) && gainOutside.track == 999);
    RhhContentKey key = 0; const struct RhhModMusic *track = NULL;
    CHECK(!music->at(0, &key, &track) && track == &sTestMusic[0] && key == RhhContentKeyOf(sTestMusic[0].id));
    CHECK(!music->find(key, &track) && music->availability(key) == RHH_MUSIC_READY);
    CHECK(music->availability(RhhContentKeyOf(sTestMusic[1].id)) == RHH_MUSIC_MISSING);
    CHECK(music->availability(RhhContentKeyOf(sTestMusic[2].id)) == RHH_MUSIC_INVALID);
    CHECK(music->availability(RhhContentKeyOf(sTestMusic[3].id)) == RHH_MUSIC_INVALID);
    CHECK(music->availability(0) == RHH_MUSIC_MISSING);
    struct RhhMusicTrackInfo info = {.struct_size = sizeof(info)};
    struct RhhMusicTrackInfo shortInfo = {.struct_size = 1, .track = 999}, oldInfo = shortInfo;
    struct RhhMusicSourceTiming timing = {.struct_size = sizeof(timing)}, smallTiming = {.struct_size = 1, .track = 999}, oldTiming = smallTiming;
    struct RhhMusicInstrumentInspection instruments = {.struct_size = sizeof(instruments)};
    struct RhhMusicNoteCoverage coverage = {.struct_size = sizeof(coverage)};
    struct RhhMusicInstrumentInspection smallInstruments = {.struct_size = 1, .track = 999}, oldInstruments = smallInstruments;
    CHECK(music->inspect_instruments(key, &smallInstruments) && !memcmp(&smallInstruments, &oldInstruments, sizeof(smallInstruments))
        && music->inspect_instruments(0, &instruments)
        && !music->inspect_instruments(key, &instruments) && instruments.kind == RHH_MUSIC_TRACK_DECLARED
        && instruments.availability == RHH_MUSIC_READY && instruments.flags & RHH_MUSIC_INSTRUMENT_MIDI
        && instruments.total_notes && instruments.count && instruments.fallback_notes <= instruments.total_notes);
    CHECK(!music->inspect_note_coverage(RhhContentKeyOf(sTestMusic[1].id), &coverage)
        && coverage.availability == RHH_MUSIC_MISSING && !coverage.flags && !coverage.total_notes
        && !music->inspect_note_coverage(RhhContentKeyOf(sTestMusic[2].id), &coverage)
        && coverage.availability == RHH_MUSIC_INVALID && !coverage.flags && !coverage.total_notes
        && !music->inspect_note_coverage(RhhContentKeyOf("test:unknown_music_track"), &coverage)
        && coverage.availability == RHH_MUSIC_MISSING && coverage.kind == RHH_MUSIC_TRACK_UNKNOWN);
    CHECK(music->inspect_source_timing(key, &smallTiming) && !memcmp(&smallTiming, &oldTiming, sizeof(smallTiming))
        && music->inspect_source_timing(0, &timing));
    CHECK(!music->inspect_source_timing(key, &timing) && timing.track == key && timing.kind == RHH_MUSIC_TRACK_DECLARED
        && timing.backend == RHH_MUSIC_BACKEND_MIDI && timing.availability == RHH_MUSIC_READY
        && (timing.flags & RHH_MUSIC_SOURCE_MIDI_TIMING) && timing.last_event_ms > 0
        && timing.loop_end_ms > timing.loop_start_ms);
    CHECK(!music->inspect_source_timing(RhhContentKeyOf(sTestMusic[1].id), &timing)
        && timing.availability == RHH_MUSIC_MISSING && !timing.flags
        && !music->inspect_source_timing(RhhContentKeyOf(sTestMusic[2].id), &timing)
        && timing.availability == RHH_MUSIC_INVALID && !timing.flags);
    CHECK(music->inspect_track(key, &shortInfo) && !memcmp(&shortInfo, &oldInfo, sizeof(shortInfo))
        && music->inspect_track(0, &info));
    CHECK(!music->inspect_track(key, &info) && info.track == key && info.kind == RHH_MUSIC_TRACK_DECLARED
        && info.backend == RHH_MUSIC_BACKEND_MIDI && info.availability == RHH_MUSIC_READY && !info.asset);
    CHECK(!music->inspect_track(RhhContentKeyOf(sTestMusic[1].id), &info) && info.kind == RHH_MUSIC_TRACK_DECLARED
        && info.availability == RHH_MUSIC_MISSING);
    CHECK(!music->inspect_track(RhhContentKeyOf(sTestMusic[2].id), &info) && info.availability == RHH_MUSIC_INVALID);
    RhhContentKey unknown = RhhContentKeyOf("test:unknown_music_track");
    CHECK(!music->inspect_track(unknown, &info) && info.track == unknown && info.kind == RHH_MUSIC_TRACK_UNKNOWN
        && info.availability == RHH_MUSIC_MISSING && !info.backend);
    rng_value_t inspectRng = gRngValue, inspectRng2 = gRng2Value;
    for (u32 asset = 1; asset <= 20; ++asset)
    {
        RhhContentKey builtIn = RhhContentKeyForId(RHH_MIDI, asset);
        CHECK(!music->inspect_track(builtIn, &info) && info.kind == RHH_MUSIC_TRACK_BUNDLED && info.asset == asset
            && info.track == builtIn && info.availability == RHH_MUSIC_READY
            && info.backend == (asset <= 8 ? RHH_MUSIC_BACKEND_GAMEBOY : asset <= 12 ? RHH_MUSIC_BACKEND_MP2K : RHH_MUSIC_BACKEND_MIDI));
        CHECK(!music->inspect_source_timing(builtIn, &timing) && timing.track == builtIn && timing.kind == info.kind
            && timing.asset == asset && timing.availability == info.availability && timing.backend == info.backend
            && (asset <= 12 ? timing.flags == RHH_MUSIC_SOURCE_ENGINE_LOOP && !timing.last_event_ms && !timing.loop_end_ms
                : (timing.flags & RHH_MUSIC_SOURCE_MIDI_TIMING) && timing.last_event_ms && timing.loop_end_ms > timing.loop_start_ms));
        CHECK(!music->inspect_instruments(builtIn, &instruments) && instruments.kind == info.kind
            && instruments.backend == info.backend && instruments.availability == info.availability
            && (asset <= 12 ? !instruments.flags && !instruments.total_notes
                : (instruments.flags & RHH_MUSIC_INSTRUMENT_MIDI) && instruments.total_notes));
        CHECK(!music->inspect_note_coverage(builtIn, &coverage) && coverage.kind == info.kind
            && coverage.backend == info.backend && coverage.availability == info.availability
            && (asset <= 12 ? !coverage.flags && !coverage.total_notes
                : (coverage.flags & RHH_MUSIC_COVERAGE_MIDI) && coverage.total_notes == instruments.total_notes
                    && coverage.covered_notes + coverage.uncovered_notes == coverage.total_notes));
    }
    CHECK(!memcmp(&inspectRng, &gRngValue, sizeof(inspectRng)) && !memcmp(&inspectRng2, &gRng2Value, sizeof(inspectRng2)));
    NativeMusicSetAssetRoot(NULL);
    CHECK(!music->inspect_track(RhhContentKeyForId(RHH_MIDI, 1), &info) && info.availability == RHH_MUSIC_MISSING
        && !music->inspect_track(RhhContentKeyForId(RHH_MIDI, 13), &info) && info.availability == RHH_MUSIC_MISSING
        && !music->inspect_track(RhhContentKeyForId(RHH_MIDI, 9), &info) && info.availability == RHH_MUSIC_READY);
    const char *badRoot = getenv("RHH_TEST_BAD_MUSIC_ROOT");
    if (badRoot && *badRoot)
    {
        NativeMusicSetAssetRoot(badRoot);
        CHECK(!music->inspect_track(RhhContentKeyForId(RHH_MIDI, 1), &info) && info.availability == RHH_MUSIC_INVALID
            && !music->inspect_track(RhhContentKeyForId(RHH_MIDI, 13), &info) && info.availability == RHH_MUSIC_INVALID
            && !music->inspect_track(RhhContentKeyForId(RHH_MIDI, 17), &info) && info.availability == RHH_MUSIC_MISSING);
        sTestMusicSelection = RhhContentKeyForId(RHH_MIDI, 1); PlayBGM(MUS_VS_WILD); CHECK(!NativeMusicActive());
        sTestMusicSelection = RhhContentKeyForId(RHH_MIDI, 13); PlayBGM(MUS_VS_WILD); CHECK(!NativeMusicActive());
        sTestMusicSelection = 0;
    }
    NativeMusicSetAssetRoot(root);
    /* Lifecycle events are queued outside mixer/selection callbacks and report
     * the actual backend, including an unavailable replacement's fallback. */
    NativeMusicOnStop(&gMPlayInfo_BGM); NativeMusicFlushLifecycle(); sTestMusicLifecycleCount = 0;
    struct RhhMusicLifecycleState lifecycle = {.struct_size = sizeof(lifecycle)};
    struct RhhMusicLifecycle outside = {.struct_size = sizeof(outside), .track = 999};
    CHECK(music->lifecycle_context(&outside) && outside.track == 999);
    sTestMusicSelection = key; PlayBGM(MUS_ROUTE101);
    CHECK(!music->lifecycle(&lifecycle) && lifecycle.active
        && lifecycle.current.backend == RHH_MUSIC_BACKEND_MIDI && lifecycle.current.track == key
        && lifecycle.current.original == RhhContentKeyForId(RHH_MUSIC, MUS_ROUTE101));
    struct RhhMusicPlayback beforeInspection = {.struct_size = sizeof(beforeInspection)}, afterInspection = {.struct_size = sizeof(afterInspection)};
    CHECK(!music->playback(&beforeInspection) && !music->inspect_track(RhhContentKeyForId(RHH_MIDI, 1), &info)
        && info.availability == RHH_MUSIC_READY && !music->playback(&afterInspection)
        && !memcmp(&beforeInspection, &afterInspection, sizeof(beforeInspection)) && music->current() == key);
    u32 loopCalls = sTestLoopCalls;
    CHECK(!music->inspect_source_timing(key, &timing) && sTestLoopCalls == loopCalls
        && timing.flags & RHH_MUSIC_SOURCE_MIDI_TIMING && !music->playback(&afterInspection)
        && !memcmp(&beforeInspection, &afterInspection, sizeof(beforeInspection))
        && beforeInspection.loop_start == ((u64)timing.loop_start_ms * beforeInspection.sample_rate + 999) / 1000
        && beforeInspection.loop_end == ((u64)timing.loop_end_ms * beforeInspection.sample_rate + 999) / 1000
            + !(timing.flags & RHH_MUSIC_SOURCE_EMBEDDED_LOOP));
    CHECK(!music->inspect_instruments(key, &instruments) && instruments.total_notes && sTestLoopCalls == loopCalls
        && !music->playback(&afterInspection) && !memcmp(&beforeInspection, &afterInspection, sizeof(beforeInspection)));
    CHECK(!music->inspect_note_coverage(key, &coverage) && coverage.total_notes == instruments.total_notes
        && coverage.covered_notes + coverage.uncovered_notes == coverage.total_notes && sTestLoopCalls == loopCalls
        && !music->playback(&afterInspection) && !memcmp(&beforeInspection, &afterInspection, sizeof(beforeInspection)));
    CHECK(!sTestMusicLifecycleCount); NativeMusicFlushLifecycle();
    CHECK(sTestMusicLifecycleCount == 1 && sTestMusicLifecycle[0].cause == RHH_MUSIC_STARTED
        && sTestMusicLifecycle[0].track == key && sTestMusicLifecycle[0].sequence == lifecycle.current.sequence);
    NativeMusicOnStop(&gMPlayInfo_BGM); NativeMusicFlushLifecycle();
    CHECK(sTestMusicLifecycleCount == 2 && sTestMusicLifecycle[1].cause == RHH_MUSIC_STOPPED
        && sTestMusicLifecycle[1].track == key && !music->lifecycle(&lifecycle) && !lifecycle.active);
    sTestMusicSelection = RhhContentKeyOf(sTestMusic[1].id); PlayBGM(MUS_ROUTE101);
    CHECK(!music->lifecycle(&lifecycle) && lifecycle.active && lifecycle.current.backend == RHH_MUSIC_BACKEND_MP2K
        && lifecycle.current.flags == RHH_MUSIC_FALLBACK && lifecycle.current.requested == sTestMusicSelection
        && lifecycle.current.track == RhhContentKeyForId(RHH_MUSIC, MUS_ROUTE101));
    NativeMusicFlushLifecycle(); CHECK(sTestMusicLifecycleCount == 3 && sTestMusicLifecycle[2].flags == RHH_MUSIC_FALLBACK);
    sTestMusicSelection = 0; PlayBGM(MUS_ROUTE101); NativeMusicFlushLifecycle();
    CHECK(sTestMusicLifecycleCount == 5 && sTestMusicLifecycle[3].cause == RHH_MUSIC_STOPPED
        && sTestMusicLifecycle[4].cause == RHH_MUSIC_STARTED && !sTestMusicLifecycle[4].flags
        && sTestMusicLifecycle[4].backend == RHH_MUSIC_BACKEND_MP2K);
    u64 droppedBefore = lifecycle.dropped;
    for (u32 i = 0; i < 40; ++i) PlayBGM(MUS_ROUTE101);
    CHECK(!music->lifecycle(&lifecycle) && lifecycle.dropped > droppedBefore);
    NativeMusicFlushLifecycle(); u64 last = sTestMusicLifecycle[sTestMusicLifecycleCount - 1].sequence;
    PlayBGM(MUS_ROUTE101); NativeMusicFlushLifecycle();
    CHECK(sTestMusicLifecycle[sTestMusicLifecycleCount - 2].sequence > last + 1);
    NativeMusicOnStop(&gMPlayInfo_BGM); NativeMusicFlushLifecycle();
    sTestMusicLifecycleCount = 0; sTestMusicLifecycleRestart = 1;
    PlayBGM(MUS_ROUTE101); NativeMusicFlushLifecycle();
    CHECK(sTestMusicLifecycleCount == 1 && sTestMusicLifecycle[0].cause == RHH_MUSIC_STARTED);
    NativeMusicFlushLifecycle();
    CHECK(sTestMusicLifecycleCount == 3 && sTestMusicLifecycle[1].cause == RHH_MUSIC_STOPPED
        && sTestMusicLifecycle[2].cause == RHH_MUSIC_STARTED
        && sTestMusicLifecycle[2].original == RhhContentKeyForId(RHH_MUSIC, MUS_ROUTE110));
    NativeMusicOnStop(&gMPlayInfo_BGM); NativeMusicFlushLifecycle();
    struct RhhMusicLifecycleState tinyLifecycle = {.struct_size = 1, .active = 99};
    CHECK(music->lifecycle(&tinyLifecycle) && tinyLifecycle.active == 99);
    sTestMusicSelection = key;
    PlayBGM(MUS_ROUTE101);
    CHECK(NativeMusicActive() && !NativeMusicAsset() && music->current() == key && sTestMusicContext == RHH_MUSIC_OTHER
        && sTestMusicOriginal == RhhContentKeyForId(RHH_MUSIC, MUS_ROUTE101));
    CHECK(!music->playback(&playback) && playback.backend == RHH_MUSIC_BACKEND_MIDI && playback.track == key
        && playback.original == sTestMusicOriginal && playback.context == RHH_MUSIC_OTHER && playback.gain == RHH_MUSIC_GAIN_ONE
        && playback.sample_rate == 42060 && !playback.position && playback.loop_end > playback.loop_start
        && (playback.flags & RHH_MUSIC_PLAYBACK_FADING_IN));
    struct RhhMusicPlayback tinyPlayback = {.struct_size = 1, .track = 999};
    CHECK(music->playback(&tinyPlayback) && tinyPlayback.track == 999);
    CHECK(sTestGainCalls && sTestGainContext.backend == RHH_MUSIC_BACKEND_MIDI && sTestGainContext.track == key);
    struct RhhMusicLoopContext outsideLoop = {.struct_size = sizeof(outsideLoop), .track = 999};
    CHECK(music->loop_context(&outsideLoop) && outsideLoop.track == 999 && sTestLoopCalls
        && sTestLoopContext.phase == RHH_MUSIC_LOOP_PLAYBACK && sTestLoopContext.track == key
        && sTestLoopContext.flags == RHH_MUSIC_LOOP_EMBEDDED);
    for (u32 mode = 1; mode <= 5; ++mode)
    {
        sTestLoopMode = mode; rng_value_t rng = gRngValue, rng2 = gRng2Value;
        PlayBGM(MUS_ROUTE101); CHECK(!music->playback(&playback)
            && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
        if (mode == 1 || mode == 5)
            CHECK(playback.loop_start == 4206 && playback.loop_end == 16824);
        else CHECK(playback.loop_start == 15u * 42060 && playback.loop_end == 45u * 42060);
    }
    sTestLoopMode = 7; PlayBGM(MUS_ROUTE101); CHECK(!music->playback(&playback)
        && playback.loop_start == 15u * 42060 && playback.loop_end == 45u * 42060);
    sTestLoopMode = 1; PlayBGM(MUS_ROUTE101);
    static float loopAudio[20000 * 2]; NativeMusicMix(loopAudio, 20000);
    CHECK(!music->playback(&playback) && playback.position == 4206 + 20000 - 16824);
    sTestLoopMode = 6; PlayBGM(MUS_ROUTE101);
    CHECK(!music->playback(&playback) && playback.context == RHH_MUSIC_WILD
        && playback.loop_start == 15u * 42060 && playback.loop_end == 45u * 42060);
    sTestLoopMode = 0; PlayBGM(MUS_ROUTE101);

    NativeMusicOnStop(&gMPlayInfo_BGM);
    float drained[3000 * 2] = {0}; NativeMusicMix(drained, 3000);
    sTestGainMode = 1; sTestGainValue = 0; PlayBGM(MUS_ROUTE101);
    CHECK(!music->playback(&playback) && playback.gain == 0);
    float muted[4096 * 2] = {0}; NativeMusicMix(muted, 4096);
    for (u32 i = 0; i < ARRAY_COUNT(muted); ++i) CHECK(muted[i] == 0.f);
    CHECK(!music->playback(&playback) && playback.position == 4096 && !(playback.flags & RHH_MUSIC_PLAYBACK_FADING_IN));
    NativeMusicOnStop(&gMPlayInfo_BGM); memset(drained, 0, sizeof(drained)); NativeMusicMix(drained, 3000);
    sTestGainValue = 500; PlayBGM(MUS_ROUTE101);
    CHECK(!music->playback(&playback) && playback.gain == 500);
    float half[4096 * 2] = {0}; NativeMusicMix(half, 4096);
    NativeMusicOnStop(&gMPlayInfo_BGM); memset(drained, 0, sizeof(drained)); NativeMusicMix(drained, 3000);
    sTestGainMode = 0; PlayBGM(MUS_ROUTE101);
    float full[4096 * 2] = {0}; NativeMusicMix(full, 4096);
    double halfEnergy = 0, fullEnergy = 0;
    for (u32 i = 0; i < ARRAY_COUNT(full); ++i) { halfEnergy += half[i] * half[i]; fullEnergy += full[i] * full[i]; }
    CHECK(fullEnergy > 0 && fabs(halfEnergy / fullEnergy - .25) < .01);
    sTestGainMode = 2; sTestGainValue = 200; PlayBGM(MUS_ROUTE101);
    CHECK(!music->playback(&playback) && playback.gain == RHH_MUSIC_GAIN_ONE);
    sTestGainMode = 1; sTestGainValue = RHH_MUSIC_GAIN_MAX + 1; PlayBGM(MUS_ROUTE101);
    CHECK(!music->playback(&playback) && playback.gain == RHH_MUSIC_GAIN_ONE);
    rng_value_t savedRng = gRngValue, savedRng2 = gRng2Value;
    sTestGainMode = 3; sTestGainValue = 200; PlayBGM(MUS_ROUTE101);
    CHECK(!music->playback(&playback) && playback.gain == RHH_MUSIC_GAIN_ONE
        && !memcmp(&savedRng, &gRngValue, sizeof(savedRng)) && !memcmp(&savedRng2, &gRng2Value, sizeof(savedRng2)));
    sTestGainMode = 4; sTestGainValue = 500; PlayBGM(MUS_ROUTE101);
    CHECK(!music->playback(&playback) && playback.gain == 500);
    sTestGainMode = 5; sTestGainValue = 200; PlayBGM(MUS_ROUTE101);
    CHECK(!music->playback(&playback) && playback.context == RHH_MUSIC_WILD && playback.gain == RHH_MUSIC_GAIN_ONE
        && playback.original == RhhContentKeyForId(RHH_MUSIC, MUS_VS_WILD));
    sTestGainMode = 0; sTestGainValue = RHH_MUSIC_GAIN_ONE; PlayBGM(MUS_ROUTE101);
    double customEnergy = 0;
    for (u32 block = 0; block < 100; ++block)
    {
        float audio[701 * 2] = {0}; NativeMusicMix(audio, 701);
        for (u32 i = 0; i < ARRAY_COUNT(audio); ++i) { CHECK(isfinite(audio[i])); customEnergy += audio[i] * audio[i]; }
    }
    CHECK(customEnergy > 0.001);
    uint64_t loopStart, loopEnd, position;
    NativeMusicLoopInfo(&loopStart, &loopEnd, &position);
    CHECK(loopStart == 15u * 42060 && loopEnd == 45u * 42060 && position == 70100);
    while (position < loopEnd)
    {
        float audio[1024 * 2] = {0};
        unsigned count = loopEnd - position < 1024 ? loopEnd - position : 1024;
        NativeMusicMix(audio, count); position += count;
    }
    float afterLoop[512 * 2] = {0}; NativeMusicMix(afterLoop, 512);
    NativeMusicLoopInfo(NULL, NULL, &position); CHECK(position == loopStart + 512);
    double loopEnergy = 0;
    for (u32 i = 0; i < ARRAY_COUNT(afterLoop); ++i) loopEnergy += afterLoop[i] * afterLoop[i];
    CHECK(loopEnergy > 0.00001);
    PlayBGM(MUS_VS_TRAINER);
    CHECK(NativeMusicActive() && music->current() == key && sTestMusicContext == RHH_MUSIC_TRAINER
        && sTestMusicOriginal == RhhContentKeyForId(RHH_MUSIC, MUS_VS_TRAINER));
    sTestMusicSelection = RhhContentKeyOf(sTestMusic[1].id);
    PlayBGM(MUS_VS_WILD); CHECK(!NativeMusicActive() && !music->current());
    sTestMusicSelection = 0;
    const u16 songs[] = {MUS_VS_WILD, MUS_VS_TRAINER, MUS_VS_GYM_LEADER, MUS_VS_CHAMPION};
    const unsigned gbTracks[2][4] = {{23, 22, 21, 24}, {40, 41, 45, 46}};
    const float gbGains[2][4] = {{0.102f, 0.109f, 0.119f, 0.111f}, {0.099f, 0.111f, 0.112f, 0.140f}};
    NativeMusicOnStop(&gMPlayInfo_BGM); memset(drained, 0, sizeof(drained)); NativeMusicMix(drained, 3000);
    sTestGainMode = 1; sTestGainValue = 500; ModsSetMusic(1); PlayBGM(MUS_VS_WILD);
    CHECK(!music->playback(&playback) && playback.backend == RHH_MUSIC_BACKEND_GAMEBOY && playback.gain == 500);
    CHECK(!music->lifecycle(&lifecycle) && lifecycle.active && lifecycle.current.backend == RHH_MUSIC_BACKEND_GAMEBOY
        && lifecycle.current.asset == 1 && lifecycle.current.track == music->current());
    CHECK(!music->playback(&beforeInspection) && !music->inspect_track(RhhContentKeyForId(RHH_MIDI, 5), &info)
        && info.availability == RHH_MUSIC_READY && !music->inspect_track(RhhContentKeyForId(RHH_MIDI, 17), &info)
        && info.availability == RHH_MUSIC_READY && !music->playback(&afterInspection)
        && !memcmp(&beforeInspection, &afterInspection, sizeof(beforeInspection)));
    char referencePath[1024]; snprintf(referencePath, sizeof(referencePath), "%s/gameboy/gen1.gbs", root);
    Music_Emu *gainReference = NULL;
    CHECK(!gme_open_file(referencePath, &gainReference, 42060) && gainReference);
    gme_ignore_silence(gainReference, 1); gme_set_autoload_playback_limit(gainReference, 0);
    CHECK(!gme_start_track(gainReference, gbTracks[0][0]));
    static float halfGameBoy[5000 * 2]; static short rawGameBoy[5000 * 2];
    NativeMusicMix(halfGameBoy, 5000); CHECK(!gme_play(gainReference, ARRAY_COUNT(rawGameBoy), rawGameBoy));
    u32 gbAudible = 0;
    for (u32 i = 2524 * 2; i < ARRAY_COUNT(halfGameBoy); ++i)
    {
        CHECK(fabsf(halfGameBoy[i] - rawGameBoy[i] / 32768.f * gbGains[0][0] * .5f) < .00001f);
        gbAudible += rawGameBoy[i] != 0;
    }
    CHECK(gbAudible);
    const struct RhhMusicLevelService *levels = (const void *)RhhModsHostAPI()->get_service("rhh:service/music_levels", RHH_SERVICE_VERSION(1, 0));
    CHECK(levels);
    struct RhhMusicLevelRequest levelRequest = {.struct_size = sizeof(levelRequest), .requester = RhhContentKeyOf("test:music_catalog"),
        .track = music->current(), .gain = 500, .expected_revision = levels->revision()};
    struct RhhMusicLevelResult levelResult = {.struct_size = sizeof(levelResult)};
    CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_OK
        && !music->playback(&playback) && playback.gain == 250 && playback.position == 5000
        && (playback.flags & RHH_MUSIC_PLAYBACK_LEVEL_RAMP));
    static float ramped[560 * 2]; static short rawRamped[560 * 2];
    NativeMusicMix(ramped, 560); CHECK(!gme_play(gainReference, ARRAY_COUNT(rawRamped), rawRamped));
    for (u32 i = 0; i < ARRAY_COUNT(ramped); ++i)
    {
        float progress = (i / 2 + 1) / 420.f; if (progress > 1.f) progress = 1.f;
        float gain = .5f - .25f * progress;
        CHECK(fabsf(ramped[i] - rawRamped[i] / 32768.f * gbGains[0][0] * gain) < .00001f);
    }
    CHECK(!music->playback(&playback) && playback.position == 5560 && playback.gain == 250
        && !(playback.flags & RHH_MUSIC_PLAYBACK_LEVEL_RAMP));
    u8 *savedLevels = NULL; u32 savedLevelsSize = 0; CHECK(!RhhModsExport(&savedLevels, &savedLevelsSize));
    levelRequest.gain = RHH_MUSIC_GAIN_ONE; levelRequest.expected_revision = levels->revision();
    CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_OK
        && !music->playback(&playback) && playback.gain == 500 && (playback.flags & RHH_MUSIC_PLAYBACK_LEVEL_RAMP));
    CHECK(!RhhModsImport(savedLevels, savedLevelsSize) && !music->playback(&playback) && playback.gain == 250
        && !(playback.flags & RHH_MUSIC_PLAYBACK_LEVEL_RAMP));
    free(savedLevels); levelRequest.expected_revision = levels->revision();
    CHECK(levels->apply(&levelRequest, &levelResult) == RHH_MUSIC_LEVEL_OK);
    gme_delete(gainReference); sTestGainMode = 0; sTestGainValue = RHH_MUSIC_GAIN_ONE;
    for (u32 gen = 1; gen <= 5; ++gen)
    {
        ModsSetMusic(gen);
        for (u32 role = 0; role < 4; ++role)
        {
            PlayBGM(songs[role]);
            CHECK(NativeMusicAsset() == (gen - 1) * 4 + role + 1);
            if (gen == 3)
            {
                CHECK(!NativeMusicActive() && gMPlayInfo_BGM.songHeader == gSongTable[songs[role]].header);
                playback.track = 999; CHECK(music->playback(&playback) && playback.track == 999);
                u32 oldClock = gMPlayInfo_BGM.clock;
                for (u32 i = 0; i < 8; ++i) m4aSoundMain();
                CHECK(gMPlayInfo_BGM.clock > oldClock);
                continue;
            }
            CHECK(NativeMusicActive());
            CHECK(!music->playback(&playback) && playback.asset == (gen - 1) * 4 + role + 1
                && playback.backend == (gen <= 2 ? RHH_MUSIC_BACKEND_GAMEBOY : RHH_MUSIC_BACKEND_MIDI)
                && playback.gain == RHH_MUSIC_GAIN_ONE && !playback.position);
            if (gen <= 2) CHECK(!playback.loop_start && !playback.loop_end);
            if (gen >= 4)
            {
                NativeMusicLoopInfo(&loopStart, &loopEnd, &position);
                CHECK(loopStart > 0 && loopEnd > loopStart && position == 0);
            }
            Music_Emu *reference = NULL;
            if (gen <= 2)
            {
                char path[1024]; snprintf(path, sizeof(path), "%s/gameboy/gen%u.gbs", root, gen);
                CHECK(!gme_open_file(path, &reference, 42060) && reference);
                gme_ignore_silence(reference, 1); gme_set_autoload_playback_limit(reference, 0);
                CHECK(!gme_start_track(reference, gbTracks[gen - 1][role]));
            }
            u32 nonzero = 0; double energy = 0; float peak = 0;
            for (u32 block = 0; block < 300; ++block)
            {
                float audio[701 * 2] = {0}; NativeMusicMix(audio, 701);
                short expected[701 * 2];
                if (reference) CHECK(!gme_play(reference, ARRAY_COUNT(expected), expected));
                for (u32 i = 0; i < ARRAY_COUNT(audio); ++i)
                {
                    CHECK(isfinite(audio[i])); if (audio[i] != 0) ++nonzero;
                    energy += (double)audio[i] * audio[i]; peak = fmaxf(peak, fabsf(audio[i]));
                    if (reference && block >= 4)
                        CHECK(fabsf(audio[i] - expected[i] / 32768.0f * gbGains[gen - 1][role]) < 0.00001f);
                }
            }
            printf("music: gen %u role %u samples=%u energy=%.6f peak=%.6f\n", gen, role, nonzero, energy, peak);
            CHECK(nonzero > 1000 && energy > 55 && energy < 175 && peak > 0.0001f && peak < 1);
            if (gen <= 2)
            {
                const uint64_t finish = 165u * 42060u;
                NativeMusicLoopInfo(NULL, NULL, &position);
                while (position < finish)
                {
                    float audio[1024 * 2] = {0};
                    unsigned count = finish - position < 1024 ? finish - position : 1024;
                    short expected[1024 * 2];
                    NativeMusicMix(audio, count);
                    CHECK(!gme_play(reference, count * 2, expected));
                    position += count;
                }
                float audio[4206 * 2] = {0}; NativeMusicMix(audio, 4206);
                short expected[4206 * 2]; CHECK(!gme_play(reference, ARRAY_COUNT(expected), expected));
                NativeMusicLoopInfo(NULL, NULL, &position);
                CHECK(position == finish + 4206 && NativeMusicActive());
                double longEnergy = 0;
                for (u32 i = 0; i < ARRAY_COUNT(audio); ++i)
                {
                    longEnergy += audio[i] * audio[i];
                    CHECK(fabsf(audio[i] - expected[i] / 32768.0f * gbGains[gen - 1][role]) < 0.00001f);
                }
                CHECK(longEnergy > 0.001);
            }
            if (reference) gme_delete(reference);
            if (gen >= 4 && role == 0)
            {
                NativeMusicLoopInfo(&loopStart, &loopEnd, &position);
                while (position < loopEnd)
                {
                    float audio[1024 * 2] = {0};
                    unsigned count = loopEnd - position < 1024 ? loopEnd - position : 1024;
                    NativeMusicMix(audio, count); position += count;
                }
                float audio[512 * 2] = {0}; NativeMusicMix(audio, 512);
                NativeMusicLoopInfo(NULL, NULL, &position); CHECK(position == loopStart + 512);
                double energyAfterLoop = 0;
                for (u32 i = 0; i < ARRAY_COUNT(audio); ++i) energyAfterLoop += audio[i] * audio[i];
                CHECK(energyAfterLoop > 0.00001);
            }
            PlaySE(SE_SELECT);
            struct MusicPlayerInfo *fx = gMPlayTable[gSongTable[SE_SELECT].ms].info;
            u32 oldClock = fx->clock;
            for (u32 i = 0; i < 4; ++i) m4aSoundMain();
            CHECK(fx->clock > oldClock); /* The SFX player still advances. */
            m4aMPlayFadeOut(&gMPlayInfo_BGM, 1);
            for (u32 i = 0; i < 20; ++i) m4aSoundMain();
            CHECK(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE);
            float faded[128] = {0}; NativeMusicMix(faded, 64);
            for (u32 i = 0; i < ARRAY_COUNT(faded); ++i) CHECK(faded[i] == 0);
        }
    }
    gSaveBlock2Ptr->optionsSound = OPTIONS_SOUND_MONO;
    ModsSetMusic(1); PlayBGM(MUS_VS_TRAINER);
    float mono[3000 * 2] = {0}; NativeMusicMix(mono, 3000);
    for (u32 i = 0; i < 3000; ++i) CHECK(mono[i * 2] == mono[i * 2 + 1]);
    gSaveBlock2Ptr->optionsSound = OPTIONS_SOUND_STEREO;
    NativeMusicSetAssetRoot("__missing_rhh_gameboy_music__");
    PlayBGM(MUS_VS_GYM_LEADER);
    CHECK(!NativeMusicActive() && !music->current()); /* Missing GBS falls back to the original BGM. */
    NativeMusicSetAssetRoot(root);
    ModsSetMusic(4); PlayBGM(MUS_VS_WILD);
    float playing[12000 * 2] = {0}; NativeMusicMix(playing, 12000);
    ModsSetMusic(0); PlayBGM(MUS_ROUTE101);
    CHECK(!NativeMusicActive() && NativeMusicHasAudio());
    float tail[3000 * 2] = {0}; NativeMusicMix(tail, 3000);
    double tailEnergy = 0; for (u32 i = 0; i < ARRAY_COUNT(tail); ++i) tailEnergy += tail[i] * tail[i];
    CHECK(tailEnergy > 0.001 && !NativeMusicHasAudio());
    PlayBGM(MUS_VS_WILD); CHECK(!NativeMusicActive());
    NativeMusicShutdown();
    gSaveBlock2Ptr->optionsSound = oldSound;
    puts("custom music: battle and route selection, real audio, asset validation and fallback passed");
    puts("all generation styles played; Game Boy and Emerald use their original music engines");
}
