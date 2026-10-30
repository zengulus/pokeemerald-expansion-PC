#include "mod_api.h"
#include <stdio.h>

static const struct RhhAbilityService *sAbilities;
static const struct RhhBattleService *sBattle;
static RhhContentKey sTorchic, sCharizard, sInnate;
static uint32_t sExpectedHp[2];
static void Offer(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->event_key != sTorchic && event->event_key != sCharizard) return;
    struct RhhInnateOffer offer = {.struct_size = sizeof(offer), .unlock_level = 10, .ability = sInnate};
    if (sAbilities->offer_innate(&offer)) puts("INNATE_ACTION_BAD");
}
static void Trigger(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhInnateTurnContext c = {.struct_size = sizeof(c)};
    if (sAbilities->innate_turn_context(&c) || c.ability != sInnate || c.battler > 1 || c.turn > 9)
    { puts("INNATE_ACTION_BAD"); return; }
    if (c.turn >= 6)
    {
        if (c.battler || !sAbilities->innate_turn_heal_target(4, 1, 4)
            || !sAbilities->innate_turn_heal_target(3, 1, 4)
            || !sAbilities->innate_turn_heal_target(1, 0, 4)
            || !sAbilities->innate_turn_stat_target(1, RHH_STAT_HP, 1)
            || !sAbilities->innate_turn_stat_target(1, RHH_STAT_ATTACK, 0)
            || sAbilities->innate_turn_heal_target(1, 1, 4)
            || sAbilities->innate_turn_stat_target(1, RHH_STAT_DEFENSE, 1)) puts("INNATE_ACTION_BAD");
        printf("INNATE_ACTION_TARGET %u\n", c.turn); return;
    }
    if (!sAbilities->innate_turn_heal(0, 1) || !sAbilities->innate_turn_heal(2, 1)
        || !sAbilities->innate_turn_heal(1, 17) || !sAbilities->innate_turn_stat(RHH_STAT_HP, 1)
        || !sAbilities->innate_turn_stat(RHH_STAT_ATTACK, 0) || !sAbilities->innate_turn_stat(RHH_STAT_ATTACK, 7))
        puts("INNATE_ACTION_BAD");
    if (c.turn == 4)
    {
        for (unsigned i = 0; i < 32; ++i)
            if (sAbilities->innate_turn_heal(1, 16)) puts("INNATE_ACTION_BAD");
        if (!sAbilities->innate_turn_heal(1, 16) || !sAbilities->innate_turn_stat(RHH_STAT_ATTACK, 1)
            || !sAbilities->innate_turn_heal_target(1, 1, 4)) puts("INNATE_ACTION_BAD");
    }
    else
    {
        uint32_t denominator = c.battler ? 4 : 8;
        if (sAbilities->innate_turn_heal(1, denominator) || sAbilities->innate_turn_stat(RHH_STAT_ATTACK, c.battler ? 1 : 2))
            puts("INNATE_ACTION_BAD");
        if (c.turn == 1)
        {
            uint32_t amount = c.participant.max_hp / denominator;
            if (!amount) amount = 1;
            sExpectedHp[c.battler] = c.participant.hp + amount;
            if (!c.battler && sAbilities->innate_turn_message("Recovery complete.")) puts("INNATE_ACTION_BAD");
        }
        if (c.turn == 5 && sAbilities->innate_turn_message("Interrupted.")) puts("INNATE_ACTION_BAD");
    }
    printf("INNATE_ACTION_TRIGGER %u %u\n", c.turn, c.battler);
}
static void Complete(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBattleTurnContext c = {.struct_size = sizeof(c)};
    if (sBattle->turn_context(&c) || c.battle.turn != event->args[0]) { puts("INNATE_ACTION_BAD"); return; }
    if (c.battle.turn >= 6) { printf("INNATE_ACTION_COMPLETE %u\n", c.battle.turn); return; }
    for (unsigned i = 0; i < c.battle.participant_count; ++i)
    {
        const struct RhhBattleParticipant *p = &c.battle.participants[i];
        if (p->battler > 1 || c.battle.turn == 4) continue;
        if (p->hp != sExpectedHp[p->battler]) puts("INNATE_ACTION_BAD");
        struct RhhBattleStatStages stages = {.struct_size = sizeof(stages)};
        if (sBattle->stat_stages(p->battler, &stages)
            || stages.stages[RHH_STAT_ATTACK] != (p->battler ? 1 : 2)) puts("INNATE_ACTION_BAD");
    }
    printf("INNATE_ACTION_COMPLETE %u\n", c.battle.turn);
}
static const struct RhhModAbility abilities[] = {
    {sizeof(struct RhhModAbility), RHH_ABILITY_BREAKABLE, {0, 0}, "test:ability/turn_recovery", "Turn Recovery", "Recovers at turn end."}
};
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_INNATE_PROPOSE, RHH_CONSTRAINT, 0, Offer, NULL},
    {sizeof(struct RhhModHook), RHH_INNATE_TURN_END, RHH_OBSERVE, 0, NULL, Trigger},
    {sizeof(struct RhhModHook), RHH_BATTLE_TURN_ENDED, RHH_OBSERVE, 0, NULL, Complete}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 16), RHH_SERVICE_REQUIRED, 0, "rhh:service/abilities"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 9), RHH_SERVICE_REQUIRED, 0, "rhh:service/battle"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_ABILITIES,
    .id = "test:innate_turn_actions", .hook_count = 3, .hooks = hooks, .ability_count = 1, .abilities = abilities,
    .service_requirement_count = 2, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sAbilities = (const void *)host->get_service("rhh:service/abilities", RHH_SERVICE_VERSION(1, 16));
    sBattle = (const void *)host->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 9));
    sTorchic = host->content_key("rhh:species/torchic"); sCharizard = host->content_key("rhh:species/charizard");
    sInnate = host->content_key("test:ability/turn_recovery");
    return sAbilities && sBattle && sTorchic && sCharizard && sInnate ? &definition : NULL;
}
