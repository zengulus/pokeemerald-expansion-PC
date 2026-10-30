/* Independent v1.9 consumer; no legacy generation hook sees scripted foes. */
#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhWildEncounterService *sWild;
static RhhContentKey sPoochyena, sZigzagoon, sElectrode, sLugia, sKecleon;
static RhhContentKey sGroudon, sKyogre, sHoOh, sLatias, sLatios, sMew, sDeoxys;
static RhhContentKey sElectrodeEncounter, sLugiaEncounter;
static RhhContentKey sMarowak, sRalts, sWeedle, sGhostEncounter, sWallyEncounter, sOldManEncounter;
static RhhContentKey sBirchEncounter;
static int DenyEncounter(const struct RhhWildEncounterContext *c)
{
    return c->level == 10 || (c->level == 30 && (c->species == sElectrode || c->species == sKecleon
        || c->species == sMew || c->species == sDeoxys))
        || (c->level == 70 && (c->species == sLugia || c->species == sGroudon || c->species == sKyogre || c->species == sHoOh))
        || (c->level == 50 && (c->species == sLatias || c->species == sLatios));
}
static void Candidate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if ((event->run_seed == 989 || event->run_seed == 991) && event->subject == sMarowak)
    { decision->value = sZigzagoon; return; }
    if (event->subject != sPoochyena) return;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    if (sWild->context(&c) || c.phase != RHH_WILD_PHASE_PROPOSAL || c.source != RHH_WILD_SCRIPTED
        || c.area || c.x != INT32_MIN || c.y != INT32_MIN || c.habitat_available || c.table_species
        || c.initial_species != sPoochyena || c.species != sPoochyena || c.species != event->subject
        || c.slot != event->args[2] || c.level != event->args[3] || c.ordinal != event->ordinal
        || c.identity || decision->value != sPoochyena)
    { puts("SCRIPTED_PROBE_BAD"); return; }
    if (c.level == 7) decision->value = sZigzagoon;
    else if (c.level == 9) decision->value = UINT64_MAX;
    else if (c.level == 10) decision->allowed = 0;
    else puts("SCRIPTED_PROBE_BAD");
    printf("SCRIPTED_CANDIDATE slot=%u level=%u\n", c.slot, c.level);
}
static void CandidateConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    /* A constraint cannot overwrite a proposal or undo an earlier denial. */
    decision->value = 0; decision->allowed = 1;
}
static void EventCandidate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    if (sWild->context(&c) || c.source != RHH_WILD_SCRIPTED || c.phase != RHH_WILD_PHASE_PROPOSAL
        || c.slot || c.area || c.table_species || c.identity || c.habitat_available
        || c.x != INT32_MIN || c.y != INT32_MIN || c.species != event->subject
        || c.initial_species != c.species || c.level != event->args[3] || decision->value != c.species)
    { puts("SCRIPTED_PROBE_BAD"); return; }
    if (c.species == sPoochyena)
    {
        if (c.level == 9) decision->value = UINT64_MAX;
        else if (c.level == 11) decision->allowed = 0;
        else decision->value = sZigzagoon;
    }
    printf("EVENT_CANDIDATE level=%u\n", c.level);
}
static void Permission(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    if (sWild->context(&c) || c.phase != RHH_WILD_PHASE_PROPOSAL || c.source != RHH_WILD_SCRIPTED
        || c.area || c.x != INT32_MIN || c.y != INT32_MIN || c.habitat_available || c.table_species
        || !c.identity || c.species != event->subject || c.initial_species != c.species
        || c.slot != event->args[2] || c.level != event->args[3])
    { puts("SCRIPTED_PROBE_BAD"); return; }
    if ((event->kind == RHH_CAN_BEGIN_SCRIPTED_WILD && (c.level == 11 || c.species == sLugia
            || c.species == sHoOh || c.species == sLatias || c.species == sLatios || c.species == sMew || c.species == sDeoxys))
        || (event->kind == RHH_CAN_BEGIN_EVENT_WILD && (c.species == sElectrode || c.species == sKecleon
            || c.species == sGroudon || c.species == sKyogre)))
    { puts("SCRIPTED_PROBE_BAD"); return; }
    if ((DenyEncounter(&c) && !((c.species == sElectrode || c.species == sLugia) && event->run_seed == 987))
        || (event->run_seed == 991 && c.level == 30 && (c.species == sMarowak || c.species == sZigzagoon)))
    {
        decision->allowed = 0;
        snprintf(decision->reason, sizeof(decision->reason), "scripted veto");
    }
    printf("SCRIPTED_PERMISSION slot=%u level=%u\n", c.slot, c.level);
}
static void Cancelled(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    struct RhhWildEncounterCancellation cancel = {.struct_size = sizeof(cancel)};
    if (sWild->context(&c) || sWild->cancellation_context(&cancel) || decision->allowed
        || c.phase != RHH_WILD_PHASE_CANCELLED || c.source != RHH_WILD_SCRIPTED
        || c.species != event->subject
        || !(DenyEncounter(&c) || (event->run_seed == 991 && c.level == 30
                                    && (c.species == sMarowak || c.species == sZigzagoon)))
        || c.slot != event->args[2]
        || cancel.candidate.identity != c.identity || strcmp(cancel.reason, "scripted veto"))
    { puts("SCRIPTED_PROBE_BAD"); return; }
    printf("%s slot=%u level=%u\n", event->kind == RHH_WILD_EVENT_CANCELLED ? "EVENT_CANCELLED" : "SCRIPTED_CANCELLED", c.slot, c.level);
}
static void CleanupFinished(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    struct RhhWildEncounterCancellation cancel = {.struct_size = sizeof(cancel)};
    if (sWild->context(&c) || sWild->cancellation_context(&cancel) || decision->allowed
        || c.phase != RHH_WILD_PHASE_CANCELLED || c.source != RHH_WILD_SCRIPTED
        || c.species != event->subject || !(DenyEncounter(&c) || (event->run_seed == 991 && c.level == 30
            && (c.species == sMarowak || c.species == sZigzagoon))) || c.slot != event->args[2]
        || cancel.candidate.identity != c.identity || strcmp(cancel.reason, "scripted veto"))
    { puts("SCRIPTED_PROBE_BAD"); return; }
    printf("%s slot=%u level=%u\n", event->kind == RHH_WILD_EVENT_CLEANUP_FINISHED ? "EVENT_CLEANUP" : "SCRIPTED_CLEANUP", c.slot, c.level);
}
static void CannotUndoVeto(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    decision->allowed = 1;
    decision->value = UINT64_MAX;
    snprintf(decision->reason, sizeof(decision->reason), "overwritten");
}
static void Ready(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    struct RhhWildEncounterCancellation cancel = {.struct_size = sizeof(cancel)};
    if (sWild->context(&c) || c.phase != RHH_WILD_PHASE_CREATED || c.source != RHH_WILD_SCRIPTED
        || c.area || c.x != INT32_MIN || c.y != INT32_MIN || c.habitat_available || c.table_species
        || c.species != event->subject || c.initial_species != c.species || c.species != decision->value
        || c.map != event->event_key || c.ordinal != event->ordinal || event->args[0] != c.area
        || event->args[1] != c.source || c.slot != event->args[2] || c.level != event->args[3]
        || ((c.species != (c.slot ? sZigzagoon : sPoochyena) || c.level != (c.slot ? 9u : 7u))
            && !(event->run_seed == 987 && !c.slot && ((c.species == sElectrode && c.level == 30)
                || (c.species == sLugia && c.level == 70))))
        || c.slot > 1 || !c.identity || !sWild->cancellation_context(&cancel))
    { puts("SCRIPTED_PROBE_BAD"); return; }
    printf("SCRIPTED_READY slot=%u\n", c.slot);
}
static void Legacy(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)event; (void)decision; (void)state; puts("SCRIPTED_PROBE_BAD"); }
static void SpecialReady(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    struct RhhScriptedEncounterSource source = {.struct_size = sizeof(source), .encounter = 777};
    int unavailable = sWild->scripted_source(&source);
    if (unavailable && event->run_seed == 989 && event->kind == RHH_WILD_TUTORIAL_READY
        && !sWild->context(&c) && c.source == RHH_WILD_TUTORIAL && c.species == sRalts
        && source.encounter == 777)
    { puts("SCRIPTED_UNMARKED_READY"); return; }
    if ((event->run_seed != 989 && event->run_seed != 993) || sWild->context(&c) || unavailable
        || c.phase != RHH_WILD_PHASE_CREATED || c.slot || !c.identity || c.species != event->subject
        || c.species != decision->value || c.species != c.initial_species
        || (event->kind == RHH_WILD_GHOST_READY
            ? (c.source != RHH_WILD_GHOST || c.level != 30 || source.encounter != sGhostEncounter
               || source.preparation != RHH_SCRIPTED_WILD_COMMAND || (c.species != sMarowak && c.species != sZigzagoon))
            : (c.source != RHH_WILD_TUTORIAL || c.level != 5 || source.preparation != RHH_SCRIPTED_DIRECT_SPECIAL
               || !((source.encounter == sWallyEncounter && c.species == sRalts)
                    || (source.encounter == sOldManEncounter && c.species == sWeedle)))))
    { puts("SCRIPTED_PROBE_BAD"); return; }
    printf("SCRIPTED_SPECIAL_READY %u\n", event->kind);
}
static void BattleSource(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhScriptedEncounterSource source = {.struct_size = sizeof(source)};
    if (sWild->scripted_battle_source(&source)) return;
    if (!((event->run_seed == 987 && ((source.encounter == sElectrodeEncounter
            && source.preparation == RHH_SCRIPTED_WILD_COMMAND)
        || (source.encounter == sLugiaEncounter && source.preparation == RHH_SCRIPTED_EVENT_MON)))
        || (event->run_seed == 990 && source.encounter == sBirchEncounter && source.preparation == RHH_SCRIPTED_DIRECT_SPECIAL)
        || (event->run_seed == 989 && ((source.encounter == sGhostEncounter && source.preparation == RHH_SCRIPTED_WILD_COMMAND)
        || ((source.encounter == sWallyEncounter || source.encounter == sOldManEncounter)
            && source.preparation == RHH_SCRIPTED_DIRECT_SPECIAL)))))
    { puts("SCRIPTED_PROBE_BAD"); return; }
    printf("SCRIPTED_BATTLE_SOURCE %u\n", event->kind);
}
static void FirstReady(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    struct RhhScriptedEncounterSource source = {.struct_size = sizeof(source)};
    if (event->run_seed != 990 || sWild->context(&c) || sWild->scripted_source(&source)
        || c.phase != RHH_WILD_PHASE_CREATED || c.source != RHH_WILD_FIRST_BATTLE || c.slot
        || c.level != 2 || c.species != sZigzagoon || c.species != event->subject
        || c.species != decision->value || !c.identity || source.encounter != sBirchEncounter
        || source.preparation != RHH_SCRIPTED_DIRECT_SPECIAL)
    { puts("SCRIPTED_PROBE_BAD"); return; }
    puts("SCRIPTED_FIRST_READY");
}
static void TutorialPermission(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    struct RhhScriptedEncounterSource source = {.struct_size = sizeof(source)};
    if (sWild->context(&c) || sWild->scripted_source(&source) || c.source != RHH_WILD_TUTORIAL
        || c.phase != RHH_WILD_PHASE_PROPOSAL || c.slot || c.level != 5 || !c.identity
        || c.species != event->subject || c.species != decision->value
        || source.preparation != RHH_SCRIPTED_DIRECT_SPECIAL
        || !((c.species == sRalts && source.encounter == sWallyEncounter)
             || (c.species == sWeedle && source.encounter == sOldManEncounter)))
    { puts("SCRIPTED_PROBE_BAD"); return; }
    if (event->run_seed == 992 || (event->run_seed == 993 && !((const unsigned char *)state)[0]))
    { decision->allowed = 0; snprintf(decision->reason, sizeof(decision->reason), "tutorial veto"); }
    printf("TUTORIAL_PERMISSION %u\n", !decision->allowed);
}
static void TutorialCancelled(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)state;
    struct RhhWildEncounterContext c = {.struct_size = sizeof(c)};
    struct RhhWildEncounterCancellation cancel = {.struct_size = sizeof(cancel)};
    struct RhhScriptedEncounterSource source = {.struct_size = sizeof(source)};
    if ((event->run_seed != 992 && event->run_seed != 993) || sWild->context(&c) || sWild->cancellation_context(&cancel)
        || sWild->scripted_source(&source) || decision->allowed || c.source != RHH_WILD_TUTORIAL
        || c.phase != RHH_WILD_PHASE_CANCELLED || c.species != event->subject
        || cancel.candidate.identity != c.identity || strcmp(cancel.reason, "tutorial veto")
        || source.preparation != RHH_SCRIPTED_DIRECT_SPECIAL
        || !((c.species == sRalts && source.encounter == sWallyEncounter)
             || (c.species == sWeedle && source.encounter == sOldManEncounter)))
    { puts("SCRIPTED_PROBE_BAD"); return; }
    puts(event->kind == RHH_WILD_TUTORIAL_CANCELLED ? "TUTORIAL_CANCELLED" : "TUTORIAL_CLEANUP");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_WILD_SCRIPTED_CANDIDATE, RHH_TRANSFORM, 0, Candidate, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_SCRIPTED_CANDIDATE, RHH_CONSTRAINT, 0, CandidateConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_EVENT_CANDIDATE, RHH_TRANSFORM, 0, EventCandidate, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_EVENT_CANDIDATE, RHH_CONSTRAINT, 0, CandidateConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_BEGIN_SCRIPTED_WILD, RHH_CONSTRAINT, 0, Permission, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_BEGIN_SCRIPTED_WILD, RHH_CONSTRAINT, 1, CannotUndoVeto, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_BEGIN_EVENT_WILD, RHH_CONSTRAINT, 0, Permission, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_BEGIN_EVENT_WILD, RHH_CONSTRAINT, 1, CannotUndoVeto, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_BEGIN_TUTORIAL_WILD, RHH_CONSTRAINT, 0, TutorialPermission, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_BEGIN_TUTORIAL_WILD, RHH_CONSTRAINT, 1, CannotUndoVeto, NULL},
    {sizeof(struct RhhModHook), RHH_WILD_SCRIPTED_CANCELLED, RHH_OBSERVE, 0, NULL, Cancelled},
    {sizeof(struct RhhModHook), RHH_WILD_EVENT_CANCELLED, RHH_OBSERVE, 0, NULL, Cancelled},
    {sizeof(struct RhhModHook), RHH_WILD_SCRIPTED_CLEANUP_FINISHED, RHH_OBSERVE, 0, NULL, CleanupFinished},
    {sizeof(struct RhhModHook), RHH_WILD_EVENT_CLEANUP_FINISHED, RHH_OBSERVE, 0, NULL, CleanupFinished},
    {sizeof(struct RhhModHook), RHH_WILD_TUTORIAL_CANCELLED, RHH_OBSERVE, 0, NULL, TutorialCancelled},
    {sizeof(struct RhhModHook), RHH_WILD_TUTORIAL_CLEANUP_FINISHED, RHH_OBSERVE, 0, NULL, TutorialCancelled},
    {sizeof(struct RhhModHook), RHH_WILD_SCRIPTED_READY, RHH_OBSERVE, 0, NULL, Ready},
    {sizeof(struct RhhModHook), RHH_WILD_GHOST_READY, RHH_OBSERVE, 0, NULL, SpecialReady},
    {sizeof(struct RhhModHook), RHH_WILD_TUTORIAL_READY, RHH_OBSERVE, 0, NULL, SpecialReady},
    {sizeof(struct RhhModHook), RHH_WILD_FIRST_BATTLE_READY, RHH_OBSERVE, 0, NULL, FirstReady},
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, BattleSource},
    {sizeof(struct RhhModHook), RHH_BATTLE_ENDED, RHH_OBSERVE, 0, NULL, BattleSource},
    {sizeof(struct RhhModHook), RHH_WILD_GENERATED, RHH_OBSERVE, 0, NULL, Legacy},
    {sizeof(struct RhhModHook), RHH_WILD_SPECIAL_GENERATED, RHH_OBSERVE, 0, NULL, Legacy},
    {sizeof(struct RhhModHook), RHH_WILD_FACILITY_GENERATED, RHH_OBSERVE, 0, NULL, Legacy},
    {sizeof(struct RhhModHook), RHH_WILD_DEXNAV_GENERATED, RHH_OBSERVE, 0, NULL, Legacy}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 19), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_encounter"}
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:wild_scripted_probe",
    .state_schema = 1, .state_size = 1,
    .hook_count = sizeof(sHooks) / sizeof(*sHooks), .hooks = sHooks,
    .service_requirement_count = 1, .service_requirements = sRequirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sPoochyena = host->content_key("rhh:species/poochyena");
    sZigzagoon = host->content_key("rhh:species/zigzagoon");
    sElectrode = host->content_key("rhh:species/electrode");
    sLugia = host->content_key("rhh:species/lugia");
    sKecleon = host->content_key("rhh:species/kecleon");
    sGroudon = host->content_key("rhh:species/groudon"); sKyogre = host->content_key("rhh:species/kyogre");
    sHoOh = host->content_key("rhh:species/ho_oh");
    sLatias = host->content_key("rhh:species/latias"); sLatios = host->content_key("rhh:species/latios");
    sMew = host->content_key("rhh:species/mew"); sDeoxys = host->content_key("rhh:species/deoxys");
    sElectrodeEncounter = host->content_key("rhh:encounter/aqua_hideout_b1f/electrode_1");
    sLugiaEncounter = host->content_key("rhh:encounter/navel_rock_bottom/lugia");
    sMarowak = host->content_key("rhh:species/marowak"); sRalts = host->content_key("rhh:species/ralts");
    sWeedle = host->content_key("rhh:species/weedle");
    sGhostEncounter = host->content_key("rhh:encounter/pokemon_tower_6f_frlg/marowak");
    sWallyEncounter = host->content_key("rhh:encounter/petalburg_city/wally_tutorial");
    sOldManEncounter = host->content_key("rhh:encounter/viridian_city_frlg/old_man_tutorial");
    sBirchEncounter = host->content_key("rhh:encounter/route101/birch_rescue");
    sWild = (const void *)host->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 19));
    return sPoochyena && sZigzagoon && sElectrode && sLugia && sKecleon && sGroudon && sKyogre
        && sHoOh && sLatias && sLatios && sMew && sDeoxys && sWild
        && sElectrodeEncounter && sLugiaEncounter && sMarowak && sRalts && sWeedle
        && sGhostEncounter && sWallyEncounter && sOldManEncounter && sBirchEncounter
        && sWild->header.struct_size >= RHH_WILD_ENCOUNTER_SERVICE_V1_19_SIZE ? &sDefinition : NULL;
}
