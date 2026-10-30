#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhAbilityService *sAbilities;
static const struct RhhBattleService *sBattle;
static RhhContentKey sTorchic, sCharizard, sInnate, sWard;
static int sInside;

static void Offer(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->event_key != sTorchic && event->event_key != sCharizard) return;
    struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .unlock_level = 10, .ability = sInnate};
    if (sAbilities->offer_innate(&offer)) puts("INNATE_TURN_BAD");
}
static void Changed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    if (!sInside) return;
    struct RhhInnateTurnContext c = {.struct_size = sizeof(c), .battler = 999}, old = c;
    if (!sAbilities->innate_turn_context(&c) || memcmp(&c, &old, sizeof(c))
        || !sAbilities->innate_turn_message("nested")) puts("INNATE_TURN_BAD");
}
static void ConditionProposal(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhBattleConditionProposal p = {.struct_size = sizeof(p)};
    struct RhhInnateTurnContext turn = {.struct_size = sizeof(turn)};
    if (!sInside || sBattle->condition_proposal_context(&p) || !sAbilities->innate_turn_context(&turn)
        || p.origin_event != RHH_INNATE_TURN_END || p.key != sWard || p.source != sInnate
        || p.key != event->event_key || p.source != event->subject || p.actor != p.recipient
        || p.actor > 1 || p.previous_turns || p.requested_turns != 2 || p.max_turns != 2)
        puts("INNATE_TURN_BAD");
}
static void Turn(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhInnateTurnContext c = {.struct_size = sizeof(c)}, small = {.struct_size = 1, .battler = 999}, saved = small;
    if (sAbilities->innate_turn_context(&c) || !sAbilities->innate_turn_context(&small)
        || memcmp(&small, &saved, sizeof(small)) || c.ability != sInnate || c.ability != event->event_key
        || c.battler != event->args[0] || c.turn != event->args[1] || c.index != event->ordinal
        || c.participant.battler != c.battler || c.participant.identity != event->subject
        || c.index >= c.resolution.count || c.resolution.entries[c.index].ability != c.ability
        || c.resolution.entries[c.index].status || c.battle_id != c.resolution.battle_id
        || c.turn != 1 || (c.battler != 0 && c.battler != 1)) { puts("INNATE_TURN_BAD"); return; }
    uint32_t side = c.participant.position == RHH_BATTLE_PLAYER_LEFT || c.participant.position == RHH_BATTLE_PLAYER_RIGHT
        ? RHH_CONDITION_PLAYER_SIDE : RHH_CONDITION_OPPONENT_SIDE;
    sInside = 1;
    if (sBattle->condition_set(sWard, side, 2)) puts("INNATE_TURN_BAD");
    sInside = 0;
    struct RhhInnateTurnContext again = {.struct_size = sizeof(again)};
    if (sAbilities->innate_turn_context(&again) || memcmp(&c, &again, sizeof(c))) puts("INNATE_TURN_BAD");
    if (!c.battler && (sAbilities->innate_turn_message("Turn ward is active.")
        || !sAbilities->innate_turn_message("bad @"))) puts("INNATE_TURN_BAD");
    printf("INNATE_TURN_TRIGGER %u\n", c.battler);
}
static void Complete(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleTurnContext turn = {.struct_size = sizeof(turn)};
    struct RhhBattleConditionState a = {.struct_size = sizeof(a)}, b = {.struct_size = sizeof(b)};
    if (sBattle->turn_context(&turn) || turn.battle.turn != event->args[0]
        || sBattle->condition_state(sWard, RHH_CONDITION_PLAYER_SIDE, &a)
        || sBattle->condition_state(sWard, RHH_CONDITION_OPPONENT_SIDE, &b)
        || a.remaining_turns != (turn.battle.turn == 1 ? 2u : 1u) || a.remaining_turns != b.remaining_turns)
        puts("INNATE_TURN_BAD");
    printf("INNATE_TURN_COMPLETE %u\n", turn.battle.turn);
}
static const struct RhhModAbility abilities[] = {
    {sizeof(struct RhhModAbility), RHH_ABILITY_BREAKABLE, {0, 0}, "test:ability/turn_ward", "Turn Ward", "Sets a ward at turn end."}
};
static const struct RhhModBattleCondition conditions[] = {
    {sizeof(struct RhhModBattleCondition), RHH_CONDITION_SIDE, 2, 0, "test:condition/turn_ward", "Turn Ward"}
};
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_INNATE_PROPOSE, RHH_CONSTRAINT, 0, Offer, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_TURN_END, RHH_OBSERVE, 0, NULL, Turn},
    {sizeof(struct RhhModHook), RHH_BATTLE_CONDITION_PROPOSAL, RHH_CONSTRAINT, 0, ConditionProposal, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_CONDITION_CHANGED, RHH_OBSERVE, 0, NULL, Changed},
    {sizeof(struct RhhModHook), RHH_BATTLE_TURN_ENDED, RHH_OBSERVE, 0, NULL, Complete}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 12), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 10), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_ABILITIES,
    .id = "test:innate_turn", .hook_count = 5, .hooks = hooks, .ability_count = 1, .abilities = abilities,
    .battle_condition_count = 1, .battle_conditions = conditions, .service_requirement_count = 2, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sAbilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 12));
    sBattle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 10));
    sTorchic = host->content_key("rhh:species/torchic"); sCharizard = host->content_key("rhh:species/charizard");
    sInnate = host->content_key("test:ability/turn_ward"); sWard = host->content_key("test:condition/turn_ward");
    return sAbilities && sBattle && sTorchic && sCharizard && sInnate && sWard ? &definition : NULL;
}
