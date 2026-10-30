#include <stdio.h>
#include <string.h>
#include "mod_api.h"

static const struct RhhAbilityService *sAbilities;
struct ProbeState { uint64_t battle, next_sequence, contact_battle, next_contact_sequence; };
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct ProbeState *probe = state;
    struct RhhBattlerAbilityMutation change = {.struct_size = sizeof(change)};
    struct RhhBattlerAbility current = {.struct_size = sizeof(current)};
    if (sAbilities->ability_mutation_context(&change)) { puts("ABILITY_MUTATION_EXTERNAL_BAD"); return; }
    if (probe->battle != change.battle_id) { probe->battle = change.battle_id; probe->next_sequence = 0; }
    if (change.sequence != probe->next_sequence++ || event->ordinal != change.sequence
        || event->event_key != change.move || event->subject != change.participant.identity
        || !change.batch_size || change.batch_index >= change.batch_size
        || sAbilities->battler(change.participant.battler, &current)
        || current.stored != change.after.stored || current.active_ignoring_mold_breaker != change.after.active)
        puts("ABILITY_MUTATION_EXTERNAL_BAD");
    else puts("ABILITY_MUTATION_EXTERNAL_OK");
}
static void ObserveContact(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    struct ProbeState *probe = state;
    struct RhhBattlerAbilityContactMutation contact = {.struct_size = sizeof(contact)};
    struct RhhBattlerAbilityMutation generic = {.struct_size = sizeof(generic)};
    if (sAbilities->ability_contact_mutation_context(&contact) || sAbilities->ability_mutation_context(&generic))
    { puts("ABILITY_CONTACT_EXTERNAL_BAD"); return; }
    if (probe->contact_battle != contact.change.battle_id)
    { probe->contact_battle = contact.change.battle_id; probe->next_contact_sequence = 0; }
    if (contact.change.sequence != probe->next_contact_sequence++ || event->ordinal != contact.change.sequence
        || !contact.triggering_ability || generic.sequence != contact.change.sequence
        || generic.battle_id != contact.change.battle_id || generic.cause != contact.change.cause
        || generic.participant.battler != contact.change.participant.battler || generic.after.stored != contact.change.after.stored
        || event->event_key != contact.change.move || event->subject != contact.change.participant.identity
        || !contact.change.batch_size || contact.change.batch_index >= contact.change.batch_size)
        puts("ABILITY_CONTACT_EXTERNAL_BAD");
    else puts("ABILITY_CONTACT_EXTERNAL_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLER_ABILITY_MUTATED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_BATTLER_ABILITY_CONTACT_MUTATED, RHH_OBSERVE, 0, NULL, ObserveContact}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 29), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:ability_mutation_probe",
    .state_size = sizeof(struct ProbeState), .hooks = hooks, .hook_count = 2,
    .service_requirements = requirements, .service_requirement_count = 1
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sAbilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 29));
    return sAbilities && sAbilities->header.struct_size >= RHH_ABILITY_SERVICE_V1_29_SIZE ? &definition : NULL;
}
