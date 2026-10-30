/* Copied, read-only battle lifecycle for native modules. */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "global.h"
#include "main.h"
#include "battle.h"
#include "battle_scripts.h"
#include "battle_ai_util.h"
#include "battle_dynamax.h"
#include "battle_main.h"
#include "battle_stat_change.h"
#include "battle_util.h"
#include "battle_type_set.h"
#include "type_icons.h"
#include "battle_controllers.h"
#include "battle_setup.h"
#include "config_changes.h"
#include "config/battle.h"
#include "debug.h"
#include "difficulty.h"
#include "trainer_pools.h"
#include "text.h"
#include "mod_api.h"
#include "mod_host.h"
#include "battle_shader.h"
#include "mods.h"
#include "constants/characters.h"
#include "constants/battle_set_effect.h"
#include "constants/battle_script_commands.h"
#include "constants/battle_anim.h"

_Static_assert(sizeof(enum Type) == sizeof(u8), "battle type array representation changed");
_Static_assert(NUMBER_OF_MON_TYPES < RHH_RUNTIME_TYPE_FIRST, "runtime type slots overlap built-in types");
_Static_assert(BATTLE_POKEMON_TRANSFER_SIZE + 4 <= sizeof(((struct BattleResources *)0)->transferBuffer),
    "battle Pokémon payload must fit the controller transfer buffer");
_Static_assert(sizeof(struct BattlePokemon) <= 0x200, "ally switch temporary storage is too small");
_Static_assert(sizeof(((struct BattlePokemon *)0)->extraTypes) + 3 == RHH_ACTIVE_TYPE_LIMIT,
    "battler overflow must hold every supported active type");
_Static_assert((int)RHH_BATTLE_OWNER_PLAYER == (int)B_TRAINER_PLAYER
    && (int)RHH_BATTLE_OWNER_OPPONENT_A == (int)B_TRAINER_OPPONENT_A
    && (int)RHH_BATTLE_OWNER_PARTNER == (int)B_TRAINER_PARTNER
    && (int)RHH_BATTLE_OWNER_OPPONENT_B == (int)B_TRAINER_OPPONENT_B,
    "battle ownership must match the public semantic values");
_Static_assert((int)RHH_BATTLE_PLAYER_LEFT == (int)B_POSITION_PLAYER_LEFT
    && (int)RHH_BATTLE_OPPONENT_LEFT == (int)B_POSITION_OPPONENT_LEFT
    && (int)RHH_BATTLE_PLAYER_RIGHT == (int)B_POSITION_PLAYER_RIGHT
    && (int)RHH_BATTLE_OPPONENT_RIGHT == (int)B_POSITION_OPPONENT_RIGHT,
    "battle positions must match the public semantic values");
_Static_assert((int)RHH_STAT_ATTACK == (int)STAT_ATK && (int)RHH_STAT_DEFENSE == (int)STAT_DEF
    && (int)RHH_STAT_SPEED == (int)STAT_SPEED && (int)RHH_STAT_SP_ATTACK == (int)STAT_SPATK
    && (int)RHH_STAT_SP_DEFENSE == (int)STAT_SPDEF && (int)RHH_BATTLE_ACCURACY == (int)STAT_ACC
    && (int)RHH_BATTLE_EVASION == (int)STAT_EVASION && (int)RHH_BATTLE_STAT_COUNT == (int)NUM_BATTLE_STATS,
    "public battle stat indices must match engine stages");

static uint64_t sSerial, sBattleId, sRunId, sSequence, sMoveSequence;
static uint64_t sTurnSequence, sEffectSequence, sConditionSequence;
static u16 sLastCompletedTurn;
static bool32 sHasCompletedTurn;
static uint32_t sPending;
static int sActive, sEnding;
struct ScriptedBattleTicket {
    struct RhhScriptedEncounterSource source;
    u64 run;
    u32 task, flags, map;
    bool32 first;
};
static struct ScriptedBattleTicket sScriptedScheduled, sScriptedInitializing, sFirstSelected;
static struct RhhScriptedEncounterSource sScriptedActive;
static bool32 sScriptedHandoff;

static u32 ScriptedBattleMap(void)
{
    return gSaveBlock1Ptr ? ((u8)gSaveBlock1Ptr->location.mapGroup << 8)
        | (u8)gSaveBlock1Ptr->location.mapNum : UINT32_MAX;
}
void ModsScriptedBattleSchedule(u32 task, struct RhhScriptedEncounterSource source)
{
    sScriptedScheduled = (struct ScriptedBattleTicket){.source = source, .task = task,
        .run = RhhModsRunId(), .flags = gBattleTypeFlags, .map = ScriptedBattleMap()};
    sScriptedHandoff = FALSE;
}
void ModsFirstBattleSelect(struct RhhScriptedEncounterSource source)
{
    sFirstSelected = (struct ScriptedBattleTicket){.source = source, .run = RhhModsRunId(),
        .map = ScriptedBattleMap(), .first = TRUE};
}
void ModsFirstBattleHandoff(void)
{
    sScriptedScheduled = (struct ScriptedBattleTicket){0};
    if (RhhModsHasActiveRun() && sFirstSelected.first && sFirstSelected.run == RhhModsRunId()
        && sFirstSelected.map == ScriptedBattleMap())
    {
        sScriptedScheduled = sFirstSelected;
        sScriptedScheduled.flags = gBattleTypeFlags;
    }
    sFirstSelected = (struct ScriptedBattleTicket){0};
    sScriptedHandoff = sScriptedScheduled.first;
}
bool32 ModsFirstBattleInitializing(struct RhhScriptedEncounterSource *out)
{
    if (!out || !sScriptedInitializing.first || !RhhModsHasActiveRun()
        || sScriptedInitializing.run != RhhModsRunId() || sScriptedInitializing.map != ScriptedBattleMap()
        || sScriptedInitializing.flags != gBattleTypeFlags) return FALSE;
    *out = sScriptedInitializing.source;
    return TRUE;
}
void ModsScriptedBattleTransitionComplete(u32 task)
{
    sScriptedHandoff = sScriptedScheduled.source.encounter && sScriptedScheduled.task == task;
}
void ModsScriptedBattleCallback(bool32 battleInit)
{
    if (battleInit)
    {
        sScriptedInitializing = sScriptedHandoff ? sScriptedScheduled : (struct ScriptedBattleTicket){0};
        sScriptedScheduled = (struct ScriptedBattleTicket){0};
    }
    sScriptedHandoff = FALSE;
}
static const struct RhhBattleEventContext *sEventContext;
static const struct RhhAiMoveScoreContext *sAiScoreContext;
static const struct RhhBattleTurnContext *sTurnContext;
static const struct RhhInnateEntryContext *sInnateEntryContext;
static const struct RhhInnateTurnContext *sInnateTurnContext;
static struct RhhAiInnateEntryContext *sAiInnateEntryContext;
static struct RhhAiInnateFieldContext *sAiInnateFieldContext;
static u32 sAiInnateEntryHealing;
static struct RhhAiInnateStatContext *sAiInnateStatContext;
static struct { u8 stat; s8 stages; bool8 opponent; u8 status; } sAiInnateStatOffers[RHH_AI_INNATE_ENTRY_STAT_LIMIT];
struct InnateTurnAction {
    u64 identity, sourceIdentity;
    struct RhhBattleStatusAttribution attribution;
    u32 battler, owner, partySlot, sourceBattler, sourceOwner, sourcePartySlot;
    u8 kind, first, second;
};
static struct InnateTurnAction sInnateTurnActions[32];
static u32 sInnateTurnActionCount, sInnateTurnActionNext;
static bool32 sInnateTurnPending;
static const u8 sStatusEffects[] = {
    [RHH_EFFECT_SLEEP] = MOVE_EFFECT_SLEEP, [RHH_EFFECT_POISON] = MOVE_EFFECT_POISON,
    [RHH_EFFECT_BURN] = MOVE_EFFECT_BURN, [RHH_EFFECT_FREEZE] = MOVE_EFFECT_FREEZE,
    [RHH_EFFECT_PARALYSIS] = MOVE_EFFECT_PARALYSIS, [RHH_EFFECT_TOXIC] = MOVE_EFFECT_TOXIC,
    [RHH_EFFECT_CONFUSION] = MOVE_EFFECT_CONFUSION
};
static struct RhhBattleTurnContext sInnateTurnCompletion;
struct InnateEntryReference { u64 identity, sequence; u32 owner, party_slot, cause; bool32 pending; };
struct InnateEntryTarget { u64 identity; u32 battler, owner, partySlot; };
static struct InnateEntryReference sInnateEntries[MAX_BATTLERS_COUNT];
static struct InnateEntryReference sInnateHealEntry;
static struct { struct InnateEntryTarget target; u8 numerator, denominator; } sInnateHeals[8];
static struct { struct InnateEntryTarget target; u8 stat; s8 stages; bool8 sourced; } sInnateStats[8];
static struct { struct InnateEntryTarget target; u8 animation; } sInnateEntryAnimations[8];
static struct { struct InnateEntryTarget target; struct RhhBattleStatusAttribution attribution; u8 status; } sInnateEntryStatuses[8];
static struct RhhBattleStatusAttribution sPendingInnateStatus;
static u32 sPendingInnateStatusSource, sPendingInnateStatusTarget;
static const struct RhhBattleStatusAttribution *sStatusAttributionContext;
static u32 sInnateEntryAnimationCount, sInnateEntryAnimationNext;
static u32 sInnateEntryStatusCount, sInnateEntryStatusNext;
static struct { RhhContentKey ability; u8 kind, value; } sInnateEntryEnvironment[8];
static u32 sInnateEntryEnvironmentCount, sInnateEntryEnvironmentNext;
static u32 sInnateStatCount, sInnateStatNext;
static u32 sInnateHealCount, sInnateHealNext, sInnateEntryBattler, sInnateHealBattler, sInnateSavedScriptingBattler;
static bool32 sInnateScriptingBattlerSaved;
static bool32 sInnateMoldBreakerSaved, sInnateSavedMoldBreaker;
static struct BattleStruct *sInnateSavedBattleStruct;
static bool32 sInnateStatIsSourced;
static u32 sInnateStatSourceBattler;
static const struct RhhMoveResolutionContext *sResolutionContext;
static const struct RhhMoveResolutionDamage *sResolutionDamageContext;
static const struct RhhMoveResolutionHealth *sResolutionHealthContext;
static const struct RhhMoveHealthComparison *sResolutionHealthComparisonContext;
struct ResolutionCue { u64 battle_id, identity; RhhContentKey species; u32 turn, battler, owner, party_slot; u8 animation, kind, first, second; };
struct StatusReactionAction { struct ResolutionCue cue; struct RhhBattleParticipant source; u32 originEvent; u64 originSequence; };
struct StatusReactionFrame {
    struct StatusReactionAction cues[8];
    u32 count, next, savedRecipient, savedAnimation;
    s32 savedPassiveHp;
    u32 savedPassiveBattler, playingKind;
    u32 savedSource, savedScripting;
    bool32 savedMoldBreaker;
    struct BattleStruct *savedBattleStruct;
    const u8 *script, *resume;
    u32 stackSize;
    bool32 playing, scriptBatchEnd;
};
static struct StatusReactionFrame sStatusReactions[8];
static struct StatusReactionFrame *sStatusReactionDispatch;
static struct StatusReactionFrame sBagStatusReaction;
static u64 sBagStatusBattleId;
static u32 sStatusReactionCount;
static const u8 *sStatusReactionResume;
struct ScriptCureActor { u64 battleId; u32 turn, cause; RhhContentKey move, ability; struct RhhBattleParticipant participant; bool32 valid; };
struct PartyCureReceipt { u64 identity; RhhContentKey species; u32 slot, before; bool32 valid, committed; };
struct ScriptCureBatch {
    u64 battleId; u32 cause, source, count; RhhContentKey ability, move;
    const u8 *instruction; bool32 active; struct StatusReactionFrame frames[PARTY_SIZE];
    struct PartyCureReceipt party[PARTY_SIZE];
    struct RhhBattleParticipant partyActor;
    u32 partyBattler, partyMask; bool32 partyCaptured, partyControllerReady, partyActorAvailable;
};
static struct ScriptCureActor sScriptCureActor;
static struct ScriptCureBatch sScriptCureBatch;
struct ScriptCureContinuation {
    u64 battleId; struct ScriptCureActor actor;
    enum BattlerId attacker, target, effect, abilityBattler, scriptingBattler;
    enum Ability lastAbility, popupAbility; enum Item lastItem;
    u8 savedBattler, animTurn, animTargetsHit, fixedPopup; u16 moveEffect;
    u8 communication[sizeof(gBattleCommunication)], text1[sizeof(gBattleTextBuff1)],
        text2[sizeof(gBattleTextBuff2)], text3[sizeof(gBattleTextBuff3)];
};
static struct ScriptCureContinuation sScriptCureContinuations[8];
static u32 sScriptCureContinuationCount;
static struct ResolutionCue sResolutionCues[8];
static u32 sResolutionCueCount, sResolutionCueNext;
static bool32 sResolutionCueResume;
static const struct RhhMoveEffectApplyContext *sEffectContext;
static const struct RhhMoveDamageEffectContext *sDamageContext;
static bool32 sDamageEffectScript;
static bool32 IsEffectApplyEvent(u32 event)
{ return event == RHH_MOVE_EFFECT_APPLY || event == RHH_MOVE_DAMAGE_EFFECT_APPLY || event == RHH_REGISTERED_MOVE_EFFECT; }
static const struct RhhBattleConditionChange *sConditionContext;
static u32 sConditionOrigin;
static const struct RhhBattleConditionProposal *sConditionProposalContext;
static const struct RhhBattleStatusApplication *sStatusApplicationContext;
static u64 sStatusApplicationSequence;
static const struct RhhBattleItemStatusChange *sItemStatusContext;
static u64 sItemStatusSequence;
static const struct RhhBattleStatusCure *sStatusCureContext;
static const struct RhhBattleStatusCureAttribution *sStatusCureAttribution;
static RhhContentKey sStatusCureMove;
static u64 sStatusCureSequence;
static const struct RhhBattlePartyStatusCure *sPartyStatusCureContext;
static u64 sPartyStatusCureSequence;
static const struct RhhBattlePartyItemStatusCure *sPartyItemStatusCureContext;
static u64 sPartyItemStatusCureSequence;
static u8 sConditionTurns[RHH_BATTLE_CONDITION_LIMIT][3];
static u32 sEffectTargets;
struct EffectParticipant { u64 identity; u32 owner, partySlot; };
struct EffectAction {
    u8 kind, attacker, target, first, second, recipient, hasTarget;
    u8 publicFirst, publicSecond, publicFlags;
    u32 amount, move;
    u64 battleId;
    struct EffectParticipant source, subject, origin;
};
static struct EffectAction sEffectActions[8];
u8 gModEffectAnimation;
static u32 sEffectActionCount, sEffectActionNext;
static u32 sEffectRecipient;
static u32 sEffectSavedScriptingBattler;
static bool32 sEffectScriptingBattlerSaved;
struct MoveDamageRecord { u32 attacker, move, hp, substitute; bool32 valid; };
static struct MoveDamageRecord sMoveDamage[MAX_BATTLERS_COUNT];
struct MoveDamageIdentity { u64 identity; u32 owner, partySlot, species; };
struct MoveDamageAggregate {
    bool32 valid; u32 attacker, move, flags, updates;
    u64 hp, substitute;
    struct MoveDamageIdentity source, target;
};
static struct MoveDamageAggregate sMoveDamageAggregate[MAX_BATTLERS_COUNT];
struct MoveHealthAggregate {
    bool32 valid; u32 flags, directUpdates, scriptedUpdates;
    u64 directLost, directRestored, scriptedLost, scriptedRestored;
    struct MoveDamageIdentity target;
};
static struct MoveHealthAggregate sMoveHealth[MAX_BATTLERS_COUNT];
struct MoveHealthEndpoint { bool32 valid; u32 hp, maxHp; struct MoveDamageIdentity identity; };
static struct { bool32 active; u32 attacker, move; struct MoveDamageIdentity source;
    struct MoveHealthEndpoint start[MAX_BATTLERS_COUNT]; } sMoveHealthWindow;
static struct MoveDamageIdentity MoveDamageIdentity(u32 battler);
static bool32 MoveDamageIdentityEqual(const struct MoveDamageIdentity *, const struct MoveDamageIdentity *);
static bool32 MoveHealthEndpoint(u32 battler, struct MoveHealthEndpoint *out);

static bool32 sEndTurnMessageResume;
enum { EFFECT_MESSAGE_LIMIT = 8, EXPIRY_MESSAGE_LIMIT = 32, EFFECT_MESSAGE_LENGTH = 120 };
static u8 sEffectMessages[EXPIRY_MESSAGE_LIMIT][EFFECT_MESSAGE_LENGTH + 1];
static u32 sEffectMessageCount, sEffectMessageNext;
static const u8 sEmptyEffectMessage[] = {EOS};
static const u8 *sEffectMessageText = sEmptyEffectMessage;
static int ConditionExpiryMessage(const char *text);
static int ConditionExpiryAnimation(u32 battler, u32 animation);
static int ConditionChangeAnimation(u32 battler, u32 animation);
struct ConditionAnimation { u64 battle_id, identity; RhhContentKey species; u32 turn, battler, owner, party_slot; u8 animation; };
static struct ConditionAnimation sConditionAnimations[32];
static u32 sConditionAnimationCount, sConditionAnimationNext;
struct TrainerSourceRecord { int valid; uint64_t run_id; u32 trainer_difficulty; struct RhhTrainerPartySource source; };
static struct TrainerSourceRecord sTrainerSources[2][PARTY_SIZE];
#ifdef RHH_TRAINER_SOURCE_TEST
void RhhTrainerSourceTestDifficulty(u32 owner, u32 slot, u32 difficulty)
{
    if ((owner == RHH_BATTLE_OWNER_OPPONENT_A || owner == RHH_BATTLE_OWNER_OPPONENT_B) && slot < PARTY_SIZE)
        sTrainerSources[owner == RHH_BATTLE_OWNER_OPPONENT_B][slot].trainer_difficulty = difficulty;
}
#endif

static int OpponentPartyIndex(const struct Pokemon *party)
{
    if (party == gParties[B_TRAINER_OPPONENT_A]) return 0;
    if (party == gParties[B_TRAINER_OPPONENT_B]) return 1;
    return -1;
}
void ModsTrainerProvenanceClear(void) { memset(sTrainerSources, 0, sizeof(sTrainerSources)); }
void ModsTrainerPartyClear(struct Pokemon *party)
{
    int owner = OpponentPartyIndex(party);
    if (owner >= 0) memset(sTrainerSources[owner], 0, sizeof(sTrainerSources[owner]));
}
void ModsTrainerPartyRecord(struct Pokemon *party, u32 slot, u32 trainer, u32 rosterTrainer,
                            u32 source, u32 sourceCount, const struct TrainerMon *entry)
{
    int owner = OpponentPartyIndex(party);
    if (owner < 0 || slot >= PARTY_SIZE || !entry || !sourceCount || source >= sourceCount
        || !RhhModsHasActiveRun() || !trainer || trainer >= TRAINERS_COUNT
        || !rosterTrainer || rosterTrainer >= TRAINERS_COUNT || gIsDebugBattle) return;
    RhhContentKey key = RhhContentKeyForId(RHH_TRAINER, trainer);
    if (!key) return;
    struct Pokemon copy = party[slot];
    u32 otId = GetMonData(&copy, MON_DATA_OT_ID), personality = GetMonData(&copy, MON_DATA_PERSONALITY);
    struct TrainerSourceRecord *record = &sTrainerSources[owner][slot];
    enum DifficultyLevel difficulty = GetTrainerDifficultyLevel(rosterTrainer);
    record->valid = 1; record->run_id = RhhModsRunId(); record->trainer_difficulty = GetTrainerDifficultyLevel(trainer);
    record->source = (struct RhhTrainerPartySource){
        .struct_size = sizeof(record->source), .owner = owner ? RHH_BATTLE_OWNER_OPPONENT_B : RHH_BATTLE_OWNER_OPPONENT_A,
        .party_slot = slot, .source_index = source, .source_count = sourceCount,
        .flags = (entry->tags & MON_POOL_TAG_LEAD ? RHH_TRAINER_SOURCE_LEAD : 0)
               | (entry->tags & MON_POOL_TAG_ACE ? RHH_TRAINER_SOURCE_ACE : 0),
        .source_level = entry->lvl, .generated_level = GetMonData(&copy, MON_DATA_LEVEL),
        .source_difficulty = difficulty == DIFFICULTY_EASY ? RHH_TRAINER_DIFFICULTY_EASY
            : difficulty == DIFFICULTY_NORMAL ? RHH_TRAINER_DIFFICULTY_NORMAL
            : difficulty == DIFFICULTY_HARD ? RHH_TRAINER_DIFFICULTY_HARD : RHH_TRAINER_DIFFICULTY_UNKNOWN,
        .trainer = key, .roster_trainer = RhhContentKeyForId(RHH_TRAINER, rosterTrainer),
        .source_species = RhhContentKeyForId(RHH_SPECIES, entry->species),
        .generated_species = RhhContentKeyForId(RHH_SPECIES, GetMonData(&copy, MON_DATA_SPECIES)),
        .identity = (u64)otId << 32 | personality
    };
}

static uint32_t BattleFlags(void)
{
    u32 f = gBattleTypeFlags, flags = 0;
    if (f & BATTLE_TYPE_TRAINER) flags |= RHH_BATTLE_TRAINER;
    if (f & BATTLE_TYPE_DOUBLE) flags |= RHH_BATTLE_DOUBLE;
    if (f & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED_LINK)) flags |= RHH_BATTLE_LINK;
    if (f & (BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK)) flags |= RHH_BATTLE_RECORDED;
    if (f & (BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_EREADER_TRAINER)) flags |= RHH_BATTLE_FACILITY;
    if (f & BATTLE_TYPE_INGAME_PARTNER) flags |= RHH_BATTLE_HAS_PARTNER;
    if (f & BATTLE_TYPE_TWO_OPPONENTS) flags |= RHH_BATTLE_TWO_OPPONENTS;
    if (f & (BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_CATCH_TUTORIAL)) flags |= RHH_BATTLE_TUTORIAL;
    if (f & BATTLE_TYPE_MULTI) flags |= RHH_BATTLE_MULTI;
    if (f & BATTLE_TYPE_SAFARI) flags |= RHH_BATTLE_SAFARI;
    if (f & BATTLE_TYPE_ROAMER) flags |= RHH_BATTLE_ROAMER;
    if (f & BATTLE_TYPE_GHOST) flags |= RHH_BATTLE_GHOST;
    return flags;
}
static uint32_t Outcome(u32 result)
{
    switch (result & 0x7f) {
    case B_OUTCOME_WON: return RHH_BATTLE_WON;
    case B_OUTCOME_LOST: return RHH_BATTLE_LOST;
    case B_OUTCOME_DREW: return RHH_BATTLE_DREW;
    case B_OUTCOME_RAN: return RHH_BATTLE_RAN;
    case B_OUTCOME_PLAYER_TELEPORTED: return RHH_BATTLE_PLAYER_TELEPORTED;
    case B_OUTCOME_MON_FLED: return RHH_BATTLE_MON_FLED;
    case B_OUTCOME_CAUGHT: return RHH_BATTLE_CAUGHT;
    case B_OUTCOME_NO_SAFARI_BALLS: return RHH_BATTLE_OUT_OF_BALLS;
    case B_OUTCOME_FORFEITED: return RHH_BATTLE_FORFEITED;
    case B_OUTCOME_MON_TELEPORTED: return RHH_BATTLE_MON_TELEPORTED;
    default: return RHH_BATTLE_OUTCOME_UNKNOWN;
    }
}
static RhhContentKey TrainerKey(u32 id)
{ return id && id < TRAINERS_COUNT ? RhhContentKeyForId(RHH_TRAINER, id) : 0; }
static RhhContentKey AuthoredTrainerKey(enum BattleTrainer owner)
{
    /* Generated, remote and special battles reuse trainer ordinals. */
    if (gIsDebugBattle || !(gBattleTypeFlags & BATTLE_TYPE_TRAINER) || (gBattleTypeFlags & (BATTLE_TYPE_LINK
        | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK | BATTLE_TYPE_FRONTIER
        | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_EREADER_TRAINER | BATTLE_TYPE_SECRET_BASE
        | BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_CATCH_TUTORIAL))) return 0;
    if (owner == B_TRAINER_OPPONENT_A) return TrainerKey(TRAINER_BATTLE_PARAM.opponentA);
    if (owner == B_TRAINER_OPPONENT_B && (gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS))
        return TrainerKey(TRAINER_BATTLE_PARAM.opponentB);
    return 0;
}
static int Participant(u32 battler, const struct RhhBattleSnapshot *battle, struct RhhBattleParticipant *out)
{
    if (battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || gBattlerPartyIndexes[battler] >= PARTY_SIZE
        || (gAbsentBattlerFlags & (1u << battler)) || (sPending & (1u << battler))
        || !gBattleMons[battler].species) return -1;
    enum BattleTrainer trainer = GetBattlerTrainer(battler);
    if (trainer >= MAX_BATTLE_TRAINERS) return -1;
    const struct BattlePokemon *b = &gBattleMons[battler];
    struct Pokemon copy = gParties[trainer][gBattlerPartyIndexes[battler]];
    u32 species = GetMonData(&copy, MON_DATA_SPECIES);
    u32 otId = GetMonData(&copy, MON_DATA_OT_ID);
    u32 personality = GetMonData(&copy, MON_DATA_PERSONALITY);
    struct RhhBattleParticipant result = {
        .struct_size = sizeof(result), .battler = battler, .position = gBattlerPositions[battler],
        .owner = trainer, .party_slot = gBattlerPartyIndexes[battler], .level = b->level,
        .hp = b->hp, .max_hp = b->maxHP, .fainted = !b->hp,
        .species = RhhContentKeyForId(RHH_SPECIES, b->species),
        .party_species = RhhContentKeyForId(RHH_SPECIES, species),
        .identity = (u64)otId << 32 | personality
    };
    if (trainer == B_TRAINER_OPPONENT_A) result.trainer = battle->opponent_trainers[0];
    if (trainer == B_TRAINER_OPPONENT_B) result.trainer = battle->opponent_trainers[1];
    *out = result; return 0;
}
static int Current(struct RhhBattleSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sActive || !gMain.inBattle
        || !RhhModsHasActiveRun() || RhhModsRunId() != sRunId || !gBattleStruct
        || !gBattlersCount || gBattlersCount > MAX_BATTLERS_COUNT
        || gBattlersCount > RHH_BATTLE_MAX_PARTICIPANTS) return -1;
    struct RhhBattleSnapshot result = {
        .struct_size = sizeof(result), .phase = sEnding ? RHH_BATTLE_PHASE_ENDED : RHH_BATTLE_PHASE_ACTIVE,
        .flags = BattleFlags(), .outcome = sEnding ? Outcome(gBattleOutcome) : RHH_BATTLE_OUTCOME_UNKNOWN,
        .turn = gBattleTurnCounter, .run_id = sRunId, .battle_id = sBattleId
    };
    if (gSaveBlock1Ptr) result.map = RhhContentKeyForId(RHH_MAP,
        ((u8)gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum);
    result.opponent_trainers[0] = AuthoredTrainerKey(B_TRAINER_OPPONENT_A);
    result.opponent_trainers[1] = AuthoredTrainerKey(B_TRAINER_OPPONENT_B);
    for (unsigned i = 0; i < gBattlersCount; ++i) {
        struct RhhBattleParticipant participant;
        if (!Participant(i, &result, &participant)) result.participants[result.participant_count++] = participant;
    }
    *out = result; return 0;
}
int ModsScriptedBattleSource(struct RhhScriptedEncounterSource *out)
{
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (!out || out->struct_size < sizeof(*out) || !sScriptedActive.encounter || Current(&battle)) return -1;
    *out = sScriptedActive;
    return 0;
}
static int Context(struct RhhBattleEventContext *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sEventContext
        || (event != RHH_BATTLE_STARTED && event != RHH_BATTLER_ENTERED && event != RHH_BATTLE_ENDED)) return -1;
    *out = *sEventContext; return 0;
}
static int TurnContext(struct RhhBattleTurnContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sTurnContext
        || RhhModsCurrentEvent() != RHH_BATTLE_TURN_ENDED) return -1;
    *out = *sTurnContext; return 0;
}
static int ConditionAt(uint32_t index, struct RhhBattleConditionInfo *out)
{
    const struct RhhModBattleCondition *condition = RhhModsBattleConditionAt(index);
    if (!condition || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhBattleConditionInfo result = {.struct_size = sizeof(result), .scope = condition->scope,
        .max_turns = condition->max_turns, .key = RhhContentKeyOf(condition->id)};
    memcpy(result.label, condition->label, strlen(condition->label) + 1);
    *out = result; return 0;
}
static int ConditionInfo(RhhContentKey key, struct RhhBattleConditionInfo *out)
{
    int index = RhhModsBattleConditionIndex(key);
    return index < 0 ? -1 : ConditionAt(index, out);
}
static bool32 ConditionsAvailable(void)
{
    return sActive && !sEnding && gMain.inBattle && gBattleStruct && !gBattleOutcome
        && gBattlersCount && gBattlersCount <= MAX_BATTLERS_COUNT
        && gBattlersCount <= RHH_BATTLE_MAX_PARTICIPANTS
        && RhhModsHasActiveRun() && sRunId == RhhModsRunId()
        && !(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK));
}
struct HealthObservationSample { bool32 valid; struct RhhBattleParticipant participant; };
static struct HealthObservationSample sHealthSamples[MAX_BATTLERS_COUNT];
static const struct RhhBattlerHealthObservation *sHealthObservation;
static u64 sHealthObservationSequence;
static int HealthObservationContext(struct RhhBattlerHealthObservation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sHealthObservation
        || RhhModsCurrentEvent() != RHH_BATTLER_HEALTH_OBSERVED) return -1;
    *out = *sHealthObservation; return 0;
}
void RhhModsObserveBattleHealth(void)
{
    if (RhhModsCurrentEvent() || sHealthObservation) return;
    if (!ConditionsAvailable()) { memset(sHealthSamples, 0, sizeof(sHealthSamples)); return; }
    if (gAiLogicData && (gAiLogicData->aiCalcInProgress || gAiLogicData->aiPredictionInProgress
        || gAiLogicData->switchInCalc)) return;
    if (!RhhModsHasHook(RHH_BATTLER_HEALTH_OBSERVED)) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    struct RhhBattlerHealthObservation changes[MAX_BATTLERS_COUNT]; u32 count = 0;
    for (u32 battler = 0; battler < MAX_BATTLERS_COUNT; ++battler)
    {
        const struct RhhBattleParticipant *participant = NULL;
        for (u32 i = 0; i < battle.participant_count; ++i)
            if (battle.participants[i].battler == battler) { participant = &battle.participants[i]; break; }
        struct HealthObservationSample *previous = &sHealthSamples[battler];
        if (!participant) { previous->valid = FALSE; continue; }
        if (previous->valid && previous->participant.identity == participant->identity
            && previous->participant.owner == participant->owner && previous->participant.party_slot == participant->party_slot)
        {
            u32 changed = (previous->participant.hp != participant->hp ? RHH_BATTLER_HP_CHANGED : 0)
                | (previous->participant.max_hp != participant->max_hp ? RHH_BATTLER_MAX_HP_CHANGED : 0);
            if (changed) changes[count++] = (struct RhhBattlerHealthObservation){.struct_size = sizeof(changes[0]),
                .changed = changed, .run_id = battle.run_id, .battle_id = battle.battle_id,
                .before = previous->participant, .after = *participant};
        }
        *previous = (struct HealthObservationSample){.valid = TRUE, .participant = *participant};
    }
    for (u32 i = 0; i < count && ConditionsAvailable() && battle.battle_id == sBattleId; ++i)
    {
        if (sHealthObservationSequence == UINT64_MAX) return;
        changes[i].sequence = sHealthObservationSequence++;
        const struct RhhBattleParticipant *p = &changes[i].after;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLER_HEALTH_OBSERVED,
            .run_seed = RhhModsRunSeed(), .event_key = p->species, .subject = p->identity,
            .ordinal = changes[i].sequence, .args = {p->battler, changes[i].before.hp, p->hp, changes[i].changed}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        sHealthObservation = &changes[i]; RhhModsDispatch(&event, &decision); sHealthObservation = NULL;
        gRngValue = rng; gRng2Value = rng2;
    }
}
int RhhModsAbilityViewAllowed(void)
{
    u32 event = RhhModsCurrentEvent();
    if (!event) return TRUE;
    if (RhhModsCurrentStage() != RHH_OBSERVE) return FALSE;
    return event == RHH_BATTLE_STARTED || event == RHH_BATTLER_ENTERED
        || event == RHH_BATTLER_ABILITY_OBSERVED || event == RHH_BATTLER_INNATES_OBSERVED
        || event == RHH_BATTLER_TYPES_OBSERVED || event == RHH_BATTLER_HEALTH_OBSERVED || event == RHH_BATTLER_ABILITY_MUTATED
        || event == RHH_BATTLER_ABILITY_CONTACT_MUTATED
        || event == RHH_BATTLE_NEUTRALIZING_GAS_CHANGED
        || event == RHH_BATTLE_WEATHER_SUPPRESSION_CHANGED
        || event == RHH_BATTLE_WEATHER_GAS_SUPPRESSION_CHANGED
        || event == RHH_BATTLE_WEATHER_PRESENCE_SUPPRESSION_CHANGED
        || event == RHH_BATTLE_TURN_ENDED || event == RHH_MOVE_RESOLVED
        || event == RHH_INNATE_ENTRY || event == RHH_INNATE_TURN_END
        || event == RHH_MOVE_EFFECT_APPLY || event == RHH_MOVE_DAMAGE_EFFECT_APPLY
        || event == RHH_REGISTERED_MOVE_EFFECT;
}
int RhhModsBattlerAbility(uint32_t battler, struct RhhBattlerAbility *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable()
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || (sPending & (1u << battler)) || !IsBattlerPresent(battler)) return -1;
    /* Only committed battle observations can expose actual abilities. Pure
     * calculations and AI callbacks must use their own supplied knowledge. */
    if (!RhhModsAbilityViewAllowed()) return -1;
    u32 stored = gBattleMons[battler].ability;
    if (stored >= ABILITIES_COUNT) return -1;
    u32 active = GetBattlerAbilityReadOnly(battler, FALSE);
    u32 ignoring = GetBattlerAbilityReadOnly(battler, TRUE);
    struct RhhBattlerAbility result = {.struct_size = sizeof(result), .battler = battler,
        .flags = gBattleMons[battler].volatiles.overwrittenAbility ? RHH_BATTLER_ABILITY_OVERWRITTEN : 0,
        .battle_id = sBattleId};
    if ((stored && !(result.stored = RhhContentKeyForId(RHH_ABILITY, stored)))
        || (active && !(result.active = RhhContentKeyForId(RHH_ABILITY, active)))
        || (ignoring && !(result.active_ignoring_mold_breaker = RhhContentKeyForId(RHH_ABILITY, ignoring)))) return -1;
    *out = result; return 0;
}
struct AbilityObservationSample {
    bool32 valid;
    struct RhhBattleParticipant participant;
    struct RhhBattlerAbility ability;
};
static struct AbilityObservationSample sAbilitySamples[MAX_BATTLERS_COUNT];
static const struct RhhBattlerAbilityObservation *sAbilityObservation;
static u64 sAbilityObservationSequence;
static const struct RhhBattlerAbilityMutation *sAbilityMutationContext;
static u64 sAbilityMutationSequence, sAbilityMutationBatch;
static const struct RhhBattlerAbilityContactMutation *sAbilityContactMutationContext;
static u64 sAbilityContactMutationSequence, sAbilityContactMutationBatch;
static const struct RhhGasChange *sGasChangeContext;
static u64 sGasChangeSequence;
static bool32 sGasSampling;
static RhhContentKey BattleMoveKey(u32 move);
static bool32 BattleWeatherSuppressionCapture(struct RhhBattleEnvironmentState *out);
static bool32 BattleWeatherGasSuppressionCapture(struct RhhBattleEnvironmentState *out);
static void BattleWeatherGasSuppressionCommitted(const struct ModsGasChangeCapture *capture, u32 cause, u32 source, u32 move);
static void BattleWeatherSuppressionCommitted(const struct ModsAbilityChangeCapture *captures, u32 count,
                                              u32 cause, u32 source, u32 move, u32 triggeringAbility);
int RhhModsGasChangeContext(struct RhhGasChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sGasChangeContext
        || RhhModsCurrentEvent() != RHH_BATTLE_NEUTRALIZING_GAS_CHANGED) return -1;
    *out = *sGasChangeContext; return 0;
}
static int GasSnapshot(struct RhhGasSnapshot *out)
{
    if (!out || sGasSampling || !ConditionsAvailable()) return -1;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return -1;
    struct RhhGasSnapshot result = {.struct_size = sizeof(result), .battler_count = gBattlersCount, .battle_id = battle.battle_id};
    sGasSampling = TRUE;
    for (u32 battler = 0; battler < gBattlersCount; ++battler)
    {
        struct RhhGasSlot *slot = &result.slots[battler]; slot->battler = battler;
        if (gBattleMons[battler].volatiles.neutralizingGas && !gBattleMons[battler].volatiles.gastroAcid)
            result.source_mask |= 1u << battler;
        for (u32 i = 0; i < battle.participant_count; ++i)
            if (battle.participants[i].battler == battler)
            { slot->participant = battle.participants[i]; slot->flags |= RHH_GAS_PARTICIPANT_AVAILABLE; break; }
        if (!(slot->flags & RHH_GAS_PARTICIPANT_AVAILABLE)) continue;
        slot->ability.struct_size = sizeof(slot->ability);
        if (!RhhModsBattlerAbility(battler, &slot->ability))
        { slot->ability.active = slot->ability.active_ignoring_mold_breaker; slot->flags |= RHH_GAS_ABILITY_AVAILABLE; }
        else memset(&slot->ability, 0, sizeof(slot->ability));
        slot->innates.struct_size = sizeof(slot->innates);
        if (!RhhModsBattlerInnates(battler, &slot->innates)) slot->flags |= RHH_GAS_INNATES_AVAILABLE;
        else memset(&slot->innates, 0, sizeof(slot->innates));
    }
    sGasSampling = FALSE; *out = result; return 0;
}
void ModsGasChangeCapture(struct ModsGasChangeCapture *out)
{
    if (!out) return;
    *out = (struct ModsGasChangeCapture){0};
    out->environmentValid = BattleWeatherGasSuppressionCapture(&out->environment);
    if (RhhModsCurrentEvent() || sGasChangeContext || !RhhModsHasHook(RHH_BATTLE_NEUTRALIZING_GAS_CHANGED)) return;
    if (!GasSnapshot(&out->before)) out->valid = TRUE;
}
static void GasChangeCommitted(const struct ModsGasChangeCapture *capture, u32 cause, u32 source, u32 move)
{
    if (!capture || !capture->valid || cause < RHH_GAS_ACTIVATED || cause > RHH_GAS_REPLACED
        || RhhModsCurrentEvent() || sGasChangeContext || sGasChangeSequence == UINT64_MAX
        || !RhhModsHasHook(RHH_BATTLE_NEUTRALIZING_GAS_CHANGED)) return;
    struct RhhGasChange change = {.struct_size = sizeof(change), .cause = cause, .run_id = sRunId,
        .battle_id = sBattleId, .before = capture->before};
    if (GasSnapshot(&change.after) || capture->before.battle_id != change.after.battle_id
        || capture->before.source_mask == change.after.source_mask) return;
    change.source_battler = source < change.after.battler_count ? source : RHH_BATTLE_NO_BATTLER;
    change.move = move ? BattleMoveKey(move) : 0; change.sequence = sGasChangeSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_NEUTRALIZING_GAS_CHANGED,
        .run_seed = RhhModsRunSeed(), .event_key = change.move, .ordinal = change.sequence,
        .subject = RhhContentKeyForId(RHH_ABILITY, ABILITY_NEUTRALIZING_GAS),
        .args = {cause, change.source_battler, change.before.source_mask, change.after.source_mask}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sGasChangeContext = &change; RhhModsDispatch(&event, &decision); sGasChangeContext = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
void ModsGasChangeCommitted(const struct ModsGasChangeCapture *capture, u32 cause, u32 source, u32 move)
{
    GasChangeCommitted(capture, cause, source, move);
    BattleWeatherGasSuppressionCommitted(capture, cause, source, move);
}
int RhhModsAbilityMutationContext(struct RhhBattlerAbilityMutation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAbilityMutationContext
        || (RhhModsCurrentEvent() != RHH_BATTLER_ABILITY_MUTATED
            && RhhModsCurrentEvent() != RHH_BATTLER_ABILITY_CONTACT_MUTATED)) return -1;
    *out = *sAbilityMutationContext; return 0;
}
int RhhModsAbilityContactMutationContext(struct RhhBattlerAbilityContactMutation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAbilityContactMutationContext
        || RhhModsCurrentEvent() != RHH_BATTLER_ABILITY_CONTACT_MUTATED) return -1;
    *out = *sAbilityContactMutationContext; return 0;
}
void ModsAbilityChangeCapture(u32 battler, struct ModsAbilityChangeCapture *out)
{
    if (!out) return;
    *out = (struct ModsAbilityChangeCapture){0};
    out->environmentValid = BattleWeatherSuppressionCapture(&out->environment);
    out->environmentBattler = battler;
    if (!ConditionsAvailable() || RhhModsCurrentEvent()
        || (!RhhModsHasHook(RHH_BATTLER_ABILITY_MUTATED) && !RhhModsHasHook(RHH_BATTLER_ABILITY_CONTACT_MUTATED))) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    struct RhhBattlerAbility ability = {.struct_size = sizeof(ability)};
    if (Current(&battle) || RhhModsBattlerAbility(battler, &ability)) return;
    u32 overwritten = gBattleMons[battler].volatiles.overwrittenAbility;
    if (overwritten >= ABILITIES_COUNT) return;
    RhhContentKey overwrittenKey = overwritten ? RhhContentKeyForId(RHH_ABILITY, overwritten) : 0;
    if (overwritten && !overwrittenKey) return;
    for (u32 i = 0; i < battle.participant_count; ++i)
        if (battle.participants[i].battler == battler)
        {
            ability.active = ability.active_ignoring_mold_breaker;
            out->valid = TRUE; out->gastroAcid = gBattleMons[battler].volatiles.gastroAcid;
            out->overwritten = overwrittenKey; out->participant = battle.participants[i]; out->ability = ability;
            return;
        }
}
static void AbilityChangesCommitted(const struct ModsAbilityChangeCapture *captures, u32 count, u32 cause,
                                    u32 source, u32 move, u32 eventKind, RhhContentKey triggeringAbility)
{
    if (!captures || !count || count > 2 || cause < RHH_ABILITY_MUTATION_COPY || cause > RHH_ABILITY_MUTATION_CONTACT_SWAP
        || !ConditionsAvailable() || RhhModsCurrentEvent() || sAbilityMutationContext
        || !RhhModsHasHook(eventKind)) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    struct RhhBattlerAbilityMutation changes[2]; u32 changed = 0;
    for (u32 i = 0; i < count; ++i)
    {
        const struct ModsAbilityChangeCapture *before = &captures[i];
        if (!before->valid || before->ability.battle_id != battle.battle_id) continue;
        u32 battler = before->participant.battler;
        bool32 duplicate = FALSE;
        for (u32 j = 0; j < i; ++j) if (captures[j].valid && captures[j].participant.battler == battler) duplicate = TRUE;
        if (duplicate) continue;
        const struct RhhBattleParticipant *participant = NULL;
        for (u32 j = 0; j < battle.participant_count; ++j)
            if (battle.participants[j].battler == battler) { participant = &battle.participants[j]; break; }
        struct RhhBattlerAbility after = {.struct_size = sizeof(after)};
        if (!participant || participant->identity != before->participant.identity || participant->owner != before->participant.owner
            || participant->party_slot != before->participant.party_slot || participant->species != before->participant.species
            || RhhModsBattlerAbility(battler, &after)) continue;
        after.active = after.active_ignoring_mold_breaker;
        bool32 gastro = gBattleMons[battler].volatiles.gastroAcid;
        u32 overwritten = gBattleMons[battler].volatiles.overwrittenAbility;
        if (overwritten >= ABILITIES_COUNT) continue;
        RhhContentKey overwrittenKey = overwritten ? RhhContentKeyForId(RHH_ABILITY, overwritten) : 0;
        if (overwritten && !overwrittenKey) continue;
        if (after.stored == before->ability.stored && after.active == before->ability.active
            && after.flags == before->ability.flags && gastro == before->gastroAcid && overwrittenKey == before->overwritten) continue;
        changes[changed++] = (struct RhhBattlerAbilityMutation){.struct_size = sizeof(changes[0]), .cause = cause,
            .source_battler = source < gBattlersCount ? source : RHH_BATTLE_NO_BATTLER,
            .before_gastro_acid = before->gastroAcid, .after_gastro_acid = gastro,
            .run_id = sRunId, .battle_id = sBattleId, .move = move ? BattleMoveKey(move) : 0,
            .before_overwritten = before->overwritten, .after_overwritten = overwrittenKey,
            .participant = *participant, .before = before->ability, .after = after};
    }
    u64 *nextBatch = eventKind == RHH_BATTLER_ABILITY_MUTATED ? &sAbilityMutationBatch : &sAbilityContactMutationBatch;
    u64 *nextSequence = eventKind == RHH_BATTLER_ABILITY_MUTATED ? &sAbilityMutationSequence : &sAbilityContactMutationSequence;
    if (!changed || *nextBatch == UINT64_MAX || *nextSequence > UINT64_MAX - changed) return;
    u64 batch = (*nextBatch)++;
    for (u32 i = 0; i < changed && ConditionsAvailable() && battle.battle_id == sBattleId; ++i)
    {
        changes[i].batch = batch; changes[i].batch_index = i; changes[i].batch_size = changed;
        changes[i].sequence = (*nextSequence)++;
        struct RhhBattlerAbilityContactMutation contact = {.struct_size = sizeof(contact),
            .triggering_ability = triggeringAbility, .change = changes[i]};
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = eventKind,
            .run_seed = RhhModsRunSeed(), .event_key = changes[i].move, .subject = changes[i].participant.identity,
            .ordinal = changes[i].sequence, .args = {cause, changes[i].participant.battler, changes[i].source_battler, changed}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        sAbilityMutationContext = &changes[i];
        sAbilityContactMutationContext = eventKind == RHH_BATTLER_ABILITY_CONTACT_MUTATED ? &contact : NULL;
        RhhModsDispatch(&event, &decision);
        sAbilityContactMutationContext = NULL; sAbilityMutationContext = NULL;
        gRngValue = rng; gRng2Value = rng2;
    }
}
void ModsAbilityChangesCommitted(const struct ModsAbilityChangeCapture *captures, u32 count, u32 cause, u32 source, u32 move)
{
    AbilityChangesCommitted(captures, count, cause, source, move, RHH_BATTLER_ABILITY_MUTATED, 0);
    BattleWeatherSuppressionCommitted(captures, count, cause, source, move, 0);
}
void ModsAbilityContactChangesCommitted(const struct ModsAbilityChangeCapture *captures, u32 count, u32 cause,
                                        u32 source, u32 move, u32 triggeringAbility)
{
    RhhContentKey ability = triggeringAbility < ABILITIES_COUNT ? RhhContentKeyForId(RHH_ABILITY, triggeringAbility) : 0;
    if (ability && (cause == RHH_ABILITY_MUTATION_CONTACT_COPY || cause == RHH_ABILITY_MUTATION_CONTACT_SWAP))
        AbilityChangesCommitted(captures, count, cause, source, move, RHH_BATTLER_ABILITY_CONTACT_MUTATED, ability);
    BattleWeatherSuppressionCommitted(captures, count, cause, source, move, triggeringAbility);
}
int RhhModsAbilityObservationContext(struct RhhBattlerAbilityObservation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAbilityObservation
        || RhhModsCurrentEvent() != RHH_BATTLER_ABILITY_OBSERVED) return -1;
    *out = *sAbilityObservation; return 0;
}
void RhhModsObserveBattleAbilities(void)
{
    if (RhhModsCurrentEvent() || sAbilityObservation) return;
    if (!ConditionsAvailable()) { memset(sAbilitySamples, 0, sizeof(sAbilitySamples)); return; }
    if (!RhhModsHasHook(RHH_BATTLER_ABILITY_OBSERVED)) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    struct RhhBattlerAbilityObservation changes[MAX_BATTLERS_COUNT];
    u32 count = 0;
    for (u32 battler = 0; battler < MAX_BATTLERS_COUNT; ++battler)
    {
        const struct RhhBattleParticipant *participant = NULL;
        for (u32 i = 0; i < battle.participant_count; ++i)
            if (battle.participants[i].battler == battler) { participant = &battle.participants[i]; break; }
        struct RhhBattlerAbility ability = {.struct_size = sizeof(ability)};
        if (!participant || RhhModsBattlerAbility(battler, &ability))
        { sAbilitySamples[battler].valid = FALSE; continue; }
        /* Move-specific Mold Breaker is not an ability-state transition. */
        ability.active = ability.active_ignoring_mold_breaker;
        struct AbilityObservationSample *previous = &sAbilitySamples[battler];
        if (previous->valid && previous->participant.identity == participant->identity
            && previous->participant.owner == participant->owner && previous->participant.party_slot == participant->party_slot
            && (previous->ability.stored != ability.stored || previous->ability.active != ability.active
                || previous->ability.flags != ability.flags))
            changes[count++] = (struct RhhBattlerAbilityObservation){.struct_size = sizeof(changes[0]),
                .participant = *participant, .before = previous->ability, .after = ability};
        *previous = (struct AbilityObservationSample){.valid = TRUE, .participant = *participant, .ability = ability};
    }
    for (u32 i = 0; i < count && ConditionsAvailable() && battle.battle_id == sBattleId; ++i)
    {
        changes[i].sequence = sAbilityObservationSequence++;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLER_ABILITY_OBSERVED,
            .run_seed = RhhModsRunSeed(), .event_key = changes[i].after.stored, .subject = changes[i].participant.identity,
            .ordinal = changes[i].sequence, .args = {changes[i].participant.battler, changes[i].participant.owner,
                changes[i].participant.party_slot, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        sAbilityObservation = &changes[i]; RhhModsDispatch(&event, &decision); sAbilityObservation = NULL;
    }
}
static int StatStages(u32 battler, struct RhhBattleStatStages *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable()
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || !IsBattlerPresent(battler)) return -1;
    struct RhhBattleStatStages result = {.struct_size = sizeof(result), .battler = battler, .battle_id = sBattleId};
    for (u32 i = RHH_STAT_ATTACK; i < RHH_BATTLE_STAT_COUNT; ++i)
    {
        u32 value = gBattleMons[battler].statStages[i];
        if (value > MAX_STAT_STAGE) return -1;
        result.stages[i] = (int)value - DEFAULT_STAT_STAGE;
    }
    *out = result; return 0;
}
static int BattleNature(u32 battler, struct RhhBattleNature *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable() || !RhhModsAbilityViewAllowed()
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || (sPending & (1u << battler)) || !IsBattlerPresent(battler)) return -1;
    const struct RhhNatureService *service = (const void *)RhhModsHostAPI()->get_service("rhh:service/nature", RHH_SERVICE_VERSION(1, 0));
    if (!service || service->header.struct_size < RHH_NATURE_SERVICE_V1_0_SIZE) return -1;
    u8 stat, flags, nature = GetBattlerNatureDetails(battler, &stat, &flags);
    struct RhhNatureInfo info = {.struct_size = sizeof(info)};
    if (service->at(nature, &info)) return -1;
    struct RhhBattleNature result = {.struct_size = sizeof(result), .battler = battler, .battle_id = sBattleId,
        .nature = nature, .nature_key = info.key};
    if (flags & NATIVE_BATTLE_NATURE_SNAPSHOT) result.flags |= RHH_BATTLE_NATURE_SNAPSHOT;
    else if (flags & NATIVE_BATTLE_NATURE_PARTY) result.flags |= RHH_BATTLE_NATURE_PARTY;
    else result.flags |= RHH_BATTLE_NATURE_PERSONALITY_FALLBACK;
    if (flags & NATIVE_BATTLE_NATURE_STAT_VALID)
    {
        info.struct_size = sizeof(info);
        if (service->at(stat, &info)) return -1;
        result.flags |= RHH_BATTLE_NATURE_STAT_AVAILABLE; result.stat_nature = stat; result.stat_nature_key = info.key;
    }
    *out = result; return 0;
}
static int ConditionIndex(RhhContentKey key, u32 target)
{
    if (!ConditionsAvailable()) return -1;
    int index = RhhModsBattleConditionIndex(key);
    if (index < 0 || target > RHH_CONDITION_OPPONENT_SIDE) return -1;
    const struct RhhModBattleCondition *condition = RhhModsBattleConditionAt(index);
    if ((condition->scope == RHH_CONDITION_FIELD && target != RHH_CONDITION_WHOLE_FIELD)
        || (condition->scope == RHH_CONDITION_SIDE && target == RHH_CONDITION_WHOLE_FIELD)) return -1;
    return index;
}
static int ActiveConditionCount(uint32_t *out)
{
    if (!out || !ConditionsAvailable()) return -1;
    u32 count = 0;
    for (u32 i = 0; i < RhhModsBattleConditionCount(); ++i)
        for (u32 target = 0; target <= RHH_CONDITION_OPPONENT_SIDE; ++target)
            if (sConditionTurns[i][target]) ++count;
    *out = count; return 0;
}
static int ActiveConditionAt(uint32_t index, struct RhhActiveBattleCondition *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable()) return -1;
    for (u32 i = 0; i < RhhModsBattleConditionCount(); ++i)
    {
        const struct RhhModBattleCondition *definition = RhhModsBattleConditionAt(i);
        for (u32 target = 0; target <= RHH_CONDITION_OPPONENT_SIDE; ++target)
        {
            if (!sConditionTurns[i][target]) continue;
            if (index--) continue;
            struct RhhActiveBattleCondition result = {.struct_size = sizeof(result), .scope = definition->scope,
                .target = target, .remaining_turns = sConditionTurns[i][target],
                .key = RhhContentKeyOf(definition->id), .battle_id = sBattleId};
            memcpy(result.label, definition->label, strlen(definition->label) + 1);
            *out = result; return 0;
        }
    }
    return -1;
}
static int ConditionState(RhhContentKey key, u32 target, struct RhhBattleConditionState *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    int index = ConditionIndex(key, target);
    if (index < 0) return -1;
    struct RhhBattleConditionState result = {.struct_size = sizeof(result), .target = target,
        .remaining_turns = sConditionTurns[index][target], .key = key, .battle_id = sBattleId};
    *out = result; return 0;
}
static int ConditionChangeContext(struct RhhBattleConditionChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sConditionContext
        || RhhModsCurrentEvent() != RHH_BATTLE_CONDITION_CHANGED) return -1;
    *out = *sConditionContext; return 0;
}
static int ConditionProposalContext(struct RhhBattleConditionProposal *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sConditionProposalContext
        || RhhModsCurrentEvent() != RHH_BATTLE_CONDITION_PROPOSAL) return -1;
    *out = *sConditionProposalContext; return 0;
}
static void NotifyConditionChange(u32 index, u32 target, u32 previous, u32 remaining, u32 reason)
{
    const struct RhhModBattleCondition *definition = RhhModsBattleConditionAt(index);
    struct RhhBattleConditionChange context = {.struct_size = sizeof(context), .reason = reason,
        .scope = definition->scope, .target = target, .previous_turns = previous,
        .remaining_turns = remaining, .turn = gBattleTurnCounter, .run_id = sRunId,
        .battle_id = sBattleId, .sequence = sConditionSequence++, .key = RhhContentKeyOf(definition->id)};
    if (!RhhModsHasHook(RHH_BATTLE_CONDITION_CHANGED)) return;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_CONDITION_CHANGED,
        .run_seed = RhhModsRunSeed(), .event_key = context.key, .ordinal = context.sequence,
        .args = {reason, target, previous, remaining}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    u32 previousOrigin = sConditionOrigin;
    sConditionOrigin = RhhModsCurrentEvent();
    sConditionContext = &context; RhhModsDispatch(&event, &decision); sConditionContext = NULL;
    sConditionOrigin = previousOrigin;
}
static int ConditionSetFailure(struct RhhBattleConditionSetResult *result, u32 status, const char *reason)
{
    result->status = status;
    snprintf(result->reason, sizeof(result->reason), "%s", reason);
    return status;
}
static bool32 PartyCureMatches(const struct RhhBattlePartyStatusCure *cure, u32 status)
{
    if (!cure || !ConditionsAvailable() || cure->battle_id != sBattleId
        || cure->run_id != sRunId || cure->owner != B_TRAINER_PLAYER || cure->party_slot >= PARTY_SIZE || sPending)
        return FALSE;
    for (u32 i = 0; i < gBattlersCount; ++i)
        if (!(gAbsentBattlerFlags & (1u << i))
            && GetBattlerTrainer(i) == B_TRAINER_PLAYER && gBattlerPartyIndexes[i] == cure->party_slot)
            return FALSE;
    struct Pokemon copy = gParties[B_TRAINER_PLAYER][cure->party_slot];
    u32 species = GetMonData(&copy, MON_DATA_SPECIES_OR_EGG);
    u64 identity = (u64)GetMonData(&copy, MON_DATA_OT_ID) << 32 | GetMonData(&copy, MON_DATA_PERSONALITY);
    return species && species != SPECIES_EGG && species < NUM_SPECIES
        && RhhContentKeyForId(RHH_SPECIES, species) == cure->species && identity == cure->identity
        && GetMonData(&copy, MON_DATA_STATUS) == status;
}
static bool32 StatusConditionTargetMatches(u32 event)
{
    const struct RhhBattleParticipant *expected;
    u32 battler, status;
    if (event == RHH_BATTLE_PARTY_STATUS_CURED)
        return sPartyStatusCureContext && PartyCureMatches(sPartyStatusCureContext, sPartyStatusCureContext->after_raw);
    if (event == RHH_BATTLE_PARTY_ITEM_STATUS_CURED)
        return sPartyItemStatusCureContext && PartyCureMatches(&sPartyItemStatusCureContext->cure,
            sPartyItemStatusCureContext->cure.after_raw);
    if (event == RHH_BATTLER_STATUS_APPLIED && sStatusApplicationContext
        && (sStatusApplicationContext->flags & RHH_BATTLE_STATUS_TARGET_AVAILABLE))
    {
        expected = &sStatusApplicationContext->target;
        battler = sStatusApplicationContext->target_battler; status = sStatusApplicationContext->after_raw;
    }
    else if (event == RHH_BATTLER_ITEM_STATUS_CHANGED && sItemStatusContext && sItemStatusContext->participant_available)
    {
        expected = &sItemStatusContext->participant;
        battler = sItemStatusContext->battler; status = sItemStatusContext->after_raw;
    }
    else if (event == RHH_BATTLER_STATUS_CURED && sStatusCureContext && sStatusCureContext->participant_available)
    {
        expected = &sStatusCureContext->participant;
        battler = sStatusCureContext->battler; status = sStatusCureContext->after_raw;
    }
    else return FALSE;
    if (!ConditionsAvailable() || battler >= gBattlersCount || gBattleMons[battler].status1 != status) return FALSE;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return FALSE;
    for (u32 i = 0; i < battle.participant_count; ++i)
    {
        const struct RhhBattleParticipant *current = &battle.participants[i];
        if (current->battler == battler)
            return current->identity == expected->identity && current->owner == expected->owner
                && current->party_slot == expected->party_slot;
    }
    return FALSE;
}
static int ConditionSetResult(RhhContentKey key, u32 target, u32 turns, struct RhhBattleConditionSetResult *out)
{
    if (!out || out->struct_size < sizeof(*out)) return RHH_CONDITION_SET_INVALID;
    struct RhhBattleConditionSetResult result = {.struct_size = sizeof(result), .key = key,
        .target = target, .requested_turns = turns, .run_id = sRunId, .battle_id = sBattleId};
    u32 event = RhhModsCurrentEvent();
    bool32 statusOrigin = event == RHH_BATTLER_STATUS_APPLIED || event == RHH_BATTLER_ITEM_STATUS_CHANGED
        || event == RHH_BATTLER_STATUS_CURED || event == RHH_BATTLE_PARTY_STATUS_CURED
        || event == RHH_BATTLE_PARTY_ITEM_STATUS_CURED;
    rng_value_t statusRng = gRngValue, statusRng2 = gRng2Value;
    if (RhhModsCurrentStage() != RHH_OBSERVE
        || !((event == RHH_MOVE_RESOLVED && sResolutionContext)
             || (event == RHH_INNATE_ENTRY && sInnateEntryContext)
             || (event == RHH_INNATE_TURN_END && sInnateTurnContext)
             || (event == RHH_BATTLER_STATUS_APPLIED && sStatusApplicationContext)
             || (event == RHH_BATTLER_ITEM_STATUS_CHANGED && sItemStatusContext)
             || (event == RHH_BATTLER_STATUS_CURED && sStatusCureContext)
             || (event == RHH_BATTLE_PARTY_STATUS_CURED && sPartyStatusCureContext)
             || (event == RHH_BATTLE_PARTY_ITEM_STATUS_CURED && sPartyItemStatusCureContext)
             || (IsEffectApplyEvent(event) && sEffectContext)) || !ConditionsAvailable())
    { ConditionSetFailure(&result, RHH_CONDITION_SET_UNAVAILABLE, "condition set unavailable"); goto done; }
    if (statusOrigin && !StatusConditionTargetMatches(event))
    { ConditionSetFailure(&result, RHH_CONDITION_SET_STALE, "status participant changed or unavailable"); goto done; }
    int index = ConditionIndex(key, target);
    if (index < 0 || turns > RhhModsBattleConditionAt(index)->max_turns)
    { ConditionSetFailure(&result, RHH_CONDITION_SET_INVALID, "invalid condition, target or duration"); goto done; }
    u32 previous = sConditionTurns[index][target];
    result.previous_turns = result.remaining_turns = previous;
    if (RhhModsHasHook(RHH_BATTLE_CONDITION_PROPOSAL))
    {
        const struct RhhModBattleCondition *definition = RhhModsBattleConditionAt(index);
        struct RhhBattleConditionProposal proposal = {.struct_size = sizeof(proposal), .origin_event = event,
            .scope = definition->scope, .target = target, .previous_turns = previous, .requested_turns = turns,
            .max_turns = definition->max_turns, .turn = gBattleTurnCounter, .run_id = sRunId, .battle_id = sBattleId,
            .key = key, .actor = RHH_BATTLE_NO_BATTLER, .recipient = RHH_BATTLE_NO_BATTLER};
        if (event == RHH_MOVE_RESOLVED)
        { proposal.source = sResolutionContext->move; proposal.actor = sResolutionContext->attacker; proposal.recipient = sResolutionContext->last_target; }
        else if (IsEffectApplyEvent(event))
        { proposal.source = sEffectContext->move; proposal.actor = sEffectContext->attacker; proposal.recipient = sEffectContext->target; }
        else if (event == RHH_INNATE_ENTRY)
        { proposal.source = sInnateEntryContext->ability; proposal.actor = proposal.recipient = sInnateEntryContext->battler; }
        else if (event == RHH_INNATE_TURN_END)
        { proposal.source = sInnateTurnContext->ability; proposal.actor = proposal.recipient = sInnateTurnContext->battler; }
        else if (event == RHH_BATTLER_STATUS_APPLIED)
        { proposal.source = sStatusApplicationContext->move; proposal.actor = sStatusApplicationContext->source_battler; proposal.recipient = sStatusApplicationContext->target_battler; }
        else if (event == RHH_BATTLER_ITEM_STATUS_CHANGED)
        { proposal.source = sItemStatusContext->item; proposal.recipient = sItemStatusContext->battler;
          if (sItemStatusContext->origin == RHH_BATTLE_ITEM_HELD) proposal.actor = proposal.recipient; }
        else if (event == RHH_BATTLER_STATUS_CURED)
        { proposal.recipient = sStatusCureContext->battler;
          if (sStatusCureAttribution && sStatusCureAttribution->cause != RHH_BATTLE_CURE_REACTION)
          { proposal.source = sStatusCureAttribution->cause == RHH_BATTLE_CURE_MOVE ? sStatusCureMove : sStatusCureAttribution->ability;
            proposal.actor = sStatusCureAttribution->source_battler; } }
        else if (event == RHH_BATTLE_PARTY_STATUS_CURED)
        { proposal.source = sPartyStatusCureContext->move; proposal.actor = sPartyStatusCureContext->source_battler; }
        else if (event == RHH_BATTLE_PARTY_ITEM_STATUS_CURED)
        { proposal.source = sPartyItemStatusCureContext->item; proposal.actor = sPartyItemStatusCureContext->cure.source_battler; }
        struct RhhModEvent request = {.struct_size = sizeof(request), .kind = RHH_BATTLE_CONDITION_PROPOSAL,
            .run_seed = RhhModsRunSeed(), .event_key = key, .subject = proposal.source, .ordinal = sConditionSequence,
            .args = {target, previous, turns, event}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = turns};
        sConditionProposalContext = &proposal; RhhModsDispatch(&request, &decision); sConditionProposalContext = NULL;
        bool32 pure = !memcmp(&statusRng, &gRngValue, sizeof(statusRng)) && !memcmp(&statusRng2, &gRng2Value, sizeof(statusRng2));
        if (statusOrigin) { gRngValue = statusRng; gRng2Value = statusRng2; }
        if (!decision.allowed)
        { ConditionSetFailure(&result, RHH_CONDITION_SET_DENIED, decision.reason[0] ? decision.reason : "condition denied"); goto done; }
        if (statusOrigin && !pure)
        { ConditionSetFailure(&result, RHH_CONDITION_SET_DENIED, "condition proposal consumed gameplay RNG"); goto done; }
        if (decision.value > definition->max_turns)
        { ConditionSetFailure(&result, RHH_CONDITION_SET_INVALID, "invalid proposed duration"); goto done; }
        if (!ConditionsAvailable() || sBattleId != proposal.battle_id || sRunId != proposal.run_id
            || sConditionTurns[index][target] != previous || (statusOrigin && !StatusConditionTargetMatches(event)))
        { ConditionSetFailure(&result, RHH_CONDITION_SET_STALE, "condition changed during validation"); goto done; }
        turns = decision.value;
    }
    sConditionTurns[index][target] = turns;
    result.remaining_turns = turns; result.changed = previous != turns;
    if (previous != turns)
        NotifyConditionChange(index, target, previous, turns, !previous ? RHH_CONDITION_APPLIED
            : !turns ? RHH_CONDITION_REMOVED : RHH_CONDITION_DURATION_CHANGED);
done:
    if (statusOrigin) { gRngValue = statusRng; gRng2Value = statusRng2; }
    *out = result; return result.status;
}
static int ConditionSet(RhhContentKey key, u32 target, u32 turns)
{
    struct RhhBattleConditionSetResult result = {.struct_size = sizeof(result)};
    return ConditionSetResult(key, target, turns, &result) == RHH_CONDITION_SET_OK ? 0 : -1;
}
static void TickConditions(void)
{
    u8 expired[RHH_BATTLE_CONDITION_LIMIT][3] = {{0}};
    for (u32 i = 0; i < RhhModsBattleConditionCount(); ++i)
        for (u32 j = 0; j < 3; ++j)
            if (sConditionTurns[i][j] && !--sConditionTurns[i][j]) expired[i][j] = TRUE;
    for (u32 i = 0; i < RhhModsBattleConditionCount(); ++i)
        for (u32 j = 0; j < 3; ++j)
            if (expired[i][j]) NotifyConditionChange(i, j, 1, 0, RHH_CONDITION_EXPIRED);
}
static int ValidTrainerPartyMon(struct Pokemon *copy)
{
    if (!GetMonData(copy, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(copy, MON_DATA_SANITY_IS_BAD_EGG)) return 0;
    u32 species = GetMonData(copy, MON_DATA_SPECIES);
    return species && species < NUM_SPECIES && !GetMonData(copy, MON_DATA_SANITY_IS_BAD_EGG);
}
static int TrainerPartySourceInternal(u32 owner, u32 slot, struct RhhTrainerPartySource *out, u32 *trainerDifficulty)
{
    if (!out || out->struct_size < sizeof(*out) || slot >= PARTY_SIZE
        || (owner != RHH_BATTLE_OWNER_OPPONENT_A && owner != RHH_BATTLE_OWNER_OPPONENT_B)) return -1;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return -1;
    unsigned index = owner == RHH_BATTLE_OWNER_OPPONENT_A ? 0 : 1;
    RhhContentKey trainer = battle.opponent_trainers[index];
    if (!trainer) return -1;
    struct Pokemon copy = gParties[index ? B_TRAINER_OPPONENT_B : B_TRAINER_OPPONENT_A][slot];
    if (!ValidTrainerPartyMon(&copy)) return -1;
    u32 otId = GetMonData(&copy, MON_DATA_OT_ID), personality = GetMonData(&copy, MON_DATA_PERSONALITY);
    u64 identity = (u64)otId << 32 | personality;
    unsigned liveMatches = 0;
    for (unsigned i = 0; i < PARTY_SIZE; ++i) {
        struct Pokemon candidate = gParties[index ? B_TRAINER_OPPONENT_B : B_TRAINER_OPPONENT_A][i];
        if (!GetMonData(&candidate, MON_DATA_SANITY_HAS_SPECIES)) continue;
        if (!ValidTrainerPartyMon(&candidate)) return -1;
        u32 candidateOt = GetMonData(&candidate, MON_DATA_OT_ID);
        u32 candidatePersonality = GetMonData(&candidate, MON_DATA_PERSONALITY);
        if (((u64)candidateOt << 32 | candidatePersonality) == identity) ++liveMatches;
    }
    if (liveMatches != 1) return -1;
    const struct TrainerSourceRecord *match = NULL;
    for (unsigned i = 0; i < PARTY_SIZE; ++i) {
        const struct TrainerSourceRecord *candidate = &sTrainerSources[index][i];
        if (!candidate->valid || candidate->run_id != battle.run_id || candidate->source.trainer != trainer
            || candidate->source.identity != identity) continue;
        if (match) return -1; /* Colliding generated identities are ambiguous. */
        match = candidate;
    }
    if (!match) return -1;
    struct RhhTrainerPartySource result = match->source; result.party_slot = slot;
    *out = result; if (trainerDifficulty) *trainerDifficulty = match->trainer_difficulty; return 0;
}
static int TrainerPartySource(u32 owner, u32 slot, struct RhhTrainerPartySource *out)
{ return TrainerPartySourceInternal(owner, slot, out, NULL); }
static u64 TrainerRosterHashValue(u64 hash, u64 value)
{
    for (u32 i = 0; i < 8; ++i) { hash ^= (u8)(value >> (i * 8)); hash *= UINT64_C(1099511628211); }
    return hash;
}
static u32 TrainerDifficultyView(enum DifficultyLevel difficulty)
{
    return difficulty == DIFFICULTY_EASY ? RHH_TRAINER_DIFFICULTY_EASY
        : difficulty == DIFFICULTY_NORMAL ? RHH_TRAINER_DIFFICULTY_NORMAL
        : difficulty == DIFFICULTY_HARD ? RHH_TRAINER_DIFFICULTY_HARD : RHH_TRAINER_DIFFICULTY_UNKNOWN;
}
#ifdef RHH_TRAINER_SOURCE_TEST
static u32 sRosterTestTrainerId, sRosterTestSourceId;
static const struct Trainer *sRosterTestTrainer, *sRosterTestSource;
void RhhTrainerRosterTestOverride(u32 trainerId, const struct Trainer *trainer, u32 sourceId, const struct Trainer *source)
{
    sRosterTestTrainerId = trainerId; sRosterTestTrainer = trainer;
    sRosterTestSourceId = sourceId; sRosterTestSource = source;
}
#endif
static const struct Trainer *TrainerRosterDefinition(u32 id)
{
#ifdef RHH_TRAINER_SOURCE_TEST
    if (sRosterTestTrainer && id == sRosterTestTrainerId) return sRosterTestTrainer;
    if (sRosterTestSource && id == sRosterTestSourceId) return sRosterTestSource;
#endif
    return GetTrainerStructFromId(id);
}
static int TrainerRosterRead(RhhContentKey key, struct RhhTrainerRoster *out, const struct TrainerMon **entries)
{
    u32 trainerId;
    if (!out || out->struct_size < sizeof(*out) || !RhhModsHasActiveRun() || !gSaveBlock1Ptr || gIsDebugBattle
        || GetCurrentDifficultyLevel() > DIFFICULTY_MAX || RhhContentResolve(RHH_TRAINER, key, &trainerId)
        || !trainerId || trainerId >= TRAINERS_COUNT) return -1;
    const struct Trainer *trainer = TrainerRosterDefinition(trainerId);
    u32 rosterId = trainer->overrideTrainer ? trainer->overrideTrainer : trainerId;
    if (!rosterId || rosterId >= TRAINERS_COUNT) return -1;
    const struct Trainer *roster = TrainerRosterDefinition(rosterId);
    u32 partySize = trainer->partySize ? trainer->partySize : trainer->overrideTrainer ? roster->partySize : 0;
    u32 count = roster->poolSize ? roster->poolSize : partySize;
    u32 available = roster->poolSize ? roster->poolSize : roster->partySize;
    if (!roster->party || !partySize || partySize > PARTY_SIZE || !count || count > available || partySize > count) return -1;
    struct RhhTrainerRoster result = {.struct_size = sizeof(result), .party_size = partySize, .source_count = count,
        .flags = (roster->poolSize ? RHH_TRAINER_ROSTER_POOL : 0) | (trainer->overrideTrainer ? RHH_TRAINER_ROSTER_OVERRIDE : 0),
        .trainer_difficulty = TrainerDifficultyView(GetTrainerDifficultyLevel(trainerId)),
        .roster_difficulty = TrainerDifficultyView(GetTrainerDifficultyLevel(rosterId)),
        .trainer = TrainerKey(trainerId), .roster_trainer = TrainerKey(rosterId)};
    if (!result.trainer || !result.roster_trainer) return -1;
    u64 hash = UINT64_C(14695981039346656037);
#define ROSTER_HASH(value) (hash = TrainerRosterHashValue(hash, (u64)(value)))
    ROSTER_HASH(result.trainer); ROSTER_HASH(result.roster_trainer); ROSTER_HASH(result.flags);
    ROSTER_HASH(partySize); ROSTER_HASH(count); ROSTER_HASH(result.trainer_difficulty); ROSTER_HASH(result.roster_difficulty);
    ROSTER_HASH(trainer->poolRuleIndex); ROSTER_HASH(trainer->poolPickIndex); ROSTER_HASH(trainer->poolPruneIndex);
    for (u32 i = 0; i < count; ++i)
    {
        const struct TrainerMon *mon = &roster->party[i];
        if (!mon->species || mon->species >= NUM_SPECIES || !RhhContentKeyForId(RHH_SPECIES, mon->species)) return -1;
        ROSTER_HASH(mon->species); ROSTER_HASH(RhhContentKeyForId(RHH_SPECIES, mon->species));
        ROSTER_HASH(mon->lvl); ROSTER_HASH(mon->tags); ROSTER_HASH(mon->iv);
        ROSTER_HASH(mon->heldItem); ROSTER_HASH(RhhContentKeyForId(RHH_ITEM, mon->heldItem));
        ROSTER_HASH(mon->ability); ROSTER_HASH(RhhContentKeyForId(RHH_ABILITY, mon->ability));
        ROSTER_HASH(mon->ball); ROSTER_HASH(mon->friendship);
        ROSTER_HASH(mon->nature); ROSTER_HASH(mon->gender); ROSTER_HASH(mon->isShiny); ROSTER_HASH(mon->teraType);
        ROSTER_HASH(mon->gigantamaxFactor); ROSTER_HASH(mon->shouldUseDynamax); ROSTER_HASH(mon->dynamaxLevel);
        for (u32 j = 0; j < MAX_MON_MOVES; ++j)
        { ROSTER_HASH(mon->moves[j]); ROSTER_HASH(RhhContentKeyForId(RHH_MOVE, mon->moves[j])); }
        ROSTER_HASH(!!mon->ev);
        if (mon->ev) for (u32 j = 0; j < NUM_STATS; ++j) ROSTER_HASH(mon->ev[j]);
        ROSTER_HASH(!!mon->nickname);
        if (mon->nickname) for (u32 j = 0; j < POKEMON_NAME_LENGTH && mon->nickname[j] != EOS; ++j) ROSTER_HASH(mon->nickname[j]);
    }
#undef ROSTER_HASH
    result.token = hash; *out = result; if (entries) *entries = roster->party; return 0;
}
static int TrainerRosterGet(RhhContentKey key, struct RhhTrainerRoster *out)
{ return TrainerRosterRead(key, out, NULL); }
static int TrainerRosterValidate(const struct RhhTrainerRoster *expected, const struct TrainerMon **entries)
{
    if (!expected || expected->struct_size < sizeof(*expected)) return -1;
    struct RhhTrainerRoster current = {.struct_size = sizeof(current)};
    if (TrainerRosterRead(expected->trainer, &current, entries) || expected->trainer != current.trainer
        || expected->roster_trainer != current.roster_trainer || expected->token != current.token
        || expected->flags != current.flags || expected->party_size != current.party_size
        || expected->source_count != current.source_count || expected->trainer_difficulty != current.trainer_difficulty
        || expected->roster_difficulty != current.roster_difficulty) return -1;
    return 0;
}
static u32 TrainerRosterEntryFlags(const struct TrainerMon *mon)
{ return (mon->tags & MON_POOL_TAG_LEAD ? RHH_TRAINER_SOURCE_LEAD : 0)
       | (mon->tags & MON_POOL_TAG_ACE ? RHH_TRAINER_SOURCE_ACE : 0); }
static int TrainerRosterEntry(const struct RhhTrainerRoster *expected, u32 index, struct RhhTrainerRosterEntry *out)
{
    const struct TrainerMon *entries;
    if (!out || out->struct_size < sizeof(*out) || TrainerRosterValidate(expected, &entries) || index >= expected->source_count) return -1;
    const struct TrainerMon *mon = &entries[index];
    *out = (struct RhhTrainerRosterEntry){.struct_size = sizeof(*out), .index = index,
        .flags = TrainerRosterEntryFlags(mon), .level = mon->lvl, .species = RhhContentKeyForId(RHH_SPECIES, mon->species)};
    return 0;
}
static int TrainerRosterMatch(const struct RhhTrainerRoster *expected, u32 index, u32 owner, u32 slot, u32 *out)
{
    const struct TrainerMon *entries;
    if (!out || TrainerRosterValidate(expected, &entries) || index >= expected->source_count) return -1;
    struct RhhTrainerPartySource source = {.struct_size = sizeof(source)};
    u32 trainerDifficulty;
    if (TrainerPartySourceInternal(owner, slot, &source, &trainerDifficulty)) return -1;
    if (source.trainer != expected->trainer) { *out = 0; return 0; }
    if (TrainerDifficultyView(trainerDifficulty) != expected->trainer_difficulty
        || source.roster_trainer != expected->roster_trainer || source.source_difficulty != expected->roster_difficulty
        || source.source_count != expected->source_count || source.source_index >= expected->source_count) return -1;
    const struct TrainerMon *actual = &entries[source.source_index];
    if (source.source_species != RhhContentKeyForId(RHH_SPECIES, actual->species)
        || source.source_level != actual->lvl || source.flags != TrainerRosterEntryFlags(actual)) return -1;
    *out = source.source_index == index; return 0;
}
static const struct RhhTrainerRosterService sTrainerRosterService = {
    {sizeof(sTrainerRosterService), RHH_SERVICE_VERSION(1, 0)}, TrainerRosterGet, TrainerRosterEntry, TrainerRosterMatch
};
static char NameGlyph(u8 c);
static RhhContentKey sTrainerCatalogKeys[TRAINERS_COUNT];
static bool32 TrainerCatalogReady(void)
{ return RhhModsHasActiveRun() && gSaveBlock1Ptr && !gIsDebugBattle && GetCurrentDifficultyLevel() <= DIFFICULTY_MAX; }
static bool32 TrainerCatalogBoss(u32 trainerClass)
{
    switch (trainerClass)
    {
    case TRAINER_CLASS_LEADER: case TRAINER_CLASS_ELITE_FOUR: case TRAINER_CLASS_CHAMPION:
    case TRAINER_CLASS_AQUA_ADMIN: case TRAINER_CLASS_AQUA_LEADER:
    case TRAINER_CLASS_MAGMA_ADMIN: case TRAINER_CLASS_MAGMA_LEADER: case TRAINER_CLASS_RIVAL:
    case TRAINER_CLASS_BOSS_FRLG: case TRAINER_CLASS_LEADER_FRLG: case TRAINER_CLASS_ELITE_FOUR_FRLG:
    case TRAINER_CLASS_CHAMPION_FRLG: case TRAINER_CLASS_RIVAL_EARLY_FRLG: case TRAINER_CLASS_RIVAL_LATE_FRLG:
        return TRUE;
    default: return FALSE;
    }
}
static const struct Trainer *TrainerCatalogDefinition(u32 id, u32 filter)
{
    if (filter > RHH_TRAINER_CATALOG_BOSSES || !id || id >= TRAINERS_COUNT || !sTrainerCatalogKeys[id]) return NULL;
    const struct Trainer *trainer = TrainerRosterDefinition(id);
    if (!trainer || trainer->trainerClass >= TRAINER_CLASS_COUNT || (!trainer->party && !trainer->overrideTrainer)
        || (filter == RHH_TRAINER_CATALOG_BOSSES && !TrainerCatalogBoss(trainer->trainerClass))) return NULL;
    u32 rosterId = trainer->overrideTrainer ? trainer->overrideTrainer : id;
    if (!rosterId || rosterId >= TRAINERS_COUNT) return NULL;
    const struct Trainer *roster = TrainerRosterDefinition(rosterId);
    u32 partySize = trainer->partySize ? trainer->partySize : trainer->overrideTrainer ? roster->partySize : 0;
    u32 count = roster->poolSize ? roster->poolSize : partySize;
    u32 available = roster->poolSize ? roster->poolSize : roster->partySize;
    if (!roster->party || !partySize || partySize > PARTY_SIZE || !count || count > available || partySize > count) return NULL;
    bool32 name = FALSE;
    for (u32 i = 0; i < sizeof(trainer->trainerName) && trainer->trainerName[i] != EOS; ++i)
        if (trainer->trainerName[i] != CHAR_SPACE) { name = TRUE; break; }
    return name ? trainer : NULL;
}
static void TrainerCatalogName(const u8 *source, u32 sourceSize, char *out, u32 capacity)
{
    u32 n = 0; bool32 start = TRUE;
    while (n + 1 < capacity && n < sourceSize && source[n] != EOS)
    {
        char c = NameGlyph(source[n]);
        if (c >= 'A' && c <= 'Z' && !start) c += 'a' - 'A';
        out[n++] = c;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) start = FALSE;
        else if (c == ' ' || c == '-') start = TRUE;
    }
    while (n && out[n - 1] == ' ') --n;
    out[n] = 0;
}
static int TrainerCatalogById(u32 id, struct RhhTrainerCatalogEntry *out)
{
    if (!out || out->struct_size < sizeof(*out) || !TrainerCatalogReady()) return -1;
    const struct Trainer *trainer = TrainerCatalogDefinition(id, RHH_TRAINER_CATALOG_ALL);
    if (!trainer) return -1;
    struct RhhTrainerCatalogEntry entry = {.struct_size = sizeof(entry), .trainer = sTrainerCatalogKeys[id],
        .flags = TrainerCatalogBoss(trainer->trainerClass) ? RHH_TRAINER_CATALOG_BOSS : 0};
    TrainerCatalogName(trainer->trainerName, sizeof(trainer->trainerName), entry.name, sizeof(entry.name));
    TrainerCatalogName(gTrainerClasses[trainer->trainerClass].name, sizeof(gTrainerClasses[0].name),
                       entry.trainer_class, sizeof(entry.trainer_class));
    const char *keyName = RhhContentName(RHH_TRAINER, id);
    if (!keyName) return -1;
    const char *slash = strrchr(keyName, '/');
    snprintf(entry.encounter_id, sizeof(entry.encounter_id), "%s", slash ? slash + 1 : keyName);
    *out = entry; return 0;
}
static uint32_t TrainerCatalogCount(u32 filter)
{
    if (!TrainerCatalogReady() || filter > RHH_TRAINER_CATALOG_BOSSES) return 0;
    u32 count = 0;
    for (u32 id = 1; id < TRAINERS_COUNT; ++id) if (TrainerCatalogDefinition(id, filter)) ++count;
    return count;
}
static int TrainerCatalogAt(u32 filter, u32 index, struct RhhTrainerCatalogEntry *out)
{
    if (!out || out->struct_size < sizeof(*out) || !TrainerCatalogReady() || filter > RHH_TRAINER_CATALOG_BOSSES) return -1;
    for (u32 id = 1; id < TRAINERS_COUNT; ++id)
        if (TrainerCatalogDefinition(id, filter) && !index--) return TrainerCatalogById(id, out);
    return -1;
}
static int TrainerCatalogFind(RhhContentKey key, struct RhhTrainerCatalogEntry *out)
{
    u32 id;
    if (RhhContentResolve(RHH_TRAINER, key, &id)) return -1;
    return TrainerCatalogById(id, out);
}
static const struct RhhTrainerCatalogService sTrainerCatalogService = {
    {sizeof(sTrainerCatalogService), RHH_SERVICE_VERSION(1, 0)}, TrainerCatalogCount, TrainerCatalogAt, TrainerCatalogFind
};
static int BattleSideOf(u32 battler, u32 *side)
{
    if (!side || !ConditionsAvailable() || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || gBattlerPositions[battler] >= MAX_BATTLERS_COUNT) return -1;
    *side = GetBattlerSide(battler) == B_SIDE_PLAYER ? RHH_CONDITION_PLAYER_SIDE : RHH_CONDITION_OPPONENT_SIDE;
    return 0;
}
enum StatusPresentation { STATUS_PRESENT_APPLICATION, STATUS_PRESENT_ITEM, STATUS_PRESENT_ABILITY, STATUS_PRESENT_NATURAL };
static void BeginStatusReaction(u32 kind)
{
    const u8 *script = gBattlescriptCurrInstr;
    bool32 supported = kind == STATUS_PRESENT_NATURAL
        ? script == BattleScript_MoveUsedWokeUp || script == BattleScript_TargetWokeUp
          || script == BattleScript_BattlerDefrosted || script == BattleScript_BattlerFrostbiteHealed
          || script == BattleScript_TakeHeart
        : kind == STATUS_PRESENT_ABILITY
        ? script == BattleScript_AbilityCuredStatus || script == BattleScript_ShedSkinActivates
        : kind == STATUS_PRESENT_ITEM ? script == BattleScript_FlameOrbActivates || script == BattleScript_ToxicOrbActivates
          || script == BattleScript_BerryCureStatusRet || script == BattleScript_BerryCureStatusAndConfusionRet
        : script == BattleScript_MoveEffectSleep || script == BattleScript_MoveEffectPoison
          || script == BattleScript_MoveEffectBurn || script == BattleScript_MoveEffectFreeze
          || script == BattleScript_MoveEffectFrostbite || script == BattleScript_MoveEffectParalysis
          || script == BattleScript_MoveEffectToxic;
    if (!supported || sStatusReactionDispatch || sStatusReactionCount >= ARRAY_COUNT(sStatusReactions)
        || !gBattleResources || !gBattleResources->battleScriptsStack) return;
    struct BattleScriptsStack *stack = gBattleResources->battleScriptsStack;
    if (!stack->size || stack->size > ARRAY_COUNT(stack->ptr) || !stack->ptr[stack->size - 1]) return;
    sStatusReactionDispatch = &sStatusReactions[sStatusReactionCount];
    *sStatusReactionDispatch = (struct StatusReactionFrame){.script = script, .stackSize = stack->size,
        .resume = stack->ptr[stack->size - 1]};
}
static void EndStatusReaction(void)
{
    struct StatusReactionFrame *frame = sStatusReactionDispatch;
    sStatusReactionDispatch = NULL;
    if (!frame || !frame->count || !gBattleResources || !gBattleResources->battleScriptsStack
        || gBattlescriptCurrInstr != frame->script || gBattleResources->battleScriptsStack->size != frame->stackSize
        || gBattleResources->battleScriptsStack->ptr[frame->stackSize - 1] != frame->resume) return;
    ++sStatusReactionCount;
    gBattleResources->battleScriptsStack->ptr[frame->stackSize - 1] = BattleScript_ModStatusReaction;
}
static int StatusApplicationContext(struct RhhBattleStatusApplication *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sStatusApplicationContext
        || (event != RHH_BATTLER_STATUS_APPLIED
            && !(event == RHH_BATTLE_CONDITION_PROPOSAL && sConditionProposalContext && sConditionProposalContext->origin_event == RHH_BATTLER_STATUS_APPLIED)
            && !(event == RHH_BATTLE_CONDITION_CHANGED && sConditionContext && sConditionOrigin == RHH_BATTLER_STATUS_APPLIED))) return -1;
    *out = *sStatusApplicationContext; return 0;
}
static int StatusAttribution(struct RhhBattleStatusAttribution *out)
{
    struct RhhBattleStatusApplication application = {.struct_size = sizeof(application)};
    if (!out || out->struct_size < sizeof(*out) || !sStatusAttributionContext
        || StatusApplicationContext(&application)) return -1;
    *out = *sStatusAttributionContext; return 0;
}
void ModsBattleStatusApplied(u32 source, u32 target, u32 effect, u32 trigger, u32 before, bool32 presentationReady)
{
    if (!ConditionsAvailable() || RhhModsCurrentEvent() || sStatusApplicationContext
        || !RhhModsHasHook(RHH_BATTLER_STATUS_APPLIED) || sStatusApplicationSequence == UINT64_MAX
        || target >= gBattlersCount || before == gBattleMons[target].status1) return;
    u32 status;
    switch (effect)
    {
    case MOVE_EFFECT_SLEEP: status = RHH_BATTLE_STATUS_SLEEP; break;
    case MOVE_EFFECT_POISON: status = RHH_BATTLE_STATUS_POISON; break;
    case MOVE_EFFECT_BURN: status = RHH_BATTLE_STATUS_BURN; break;
    case MOVE_EFFECT_FREEZE: status = RHH_BATTLE_STATUS_FREEZE; break;
    case MOVE_EFFECT_PARALYSIS: status = RHH_BATTLE_STATUS_PARALYSIS; break;
    case MOVE_EFFECT_TOXIC: status = RHH_BATTLE_STATUS_TOXIC; break;
    case MOVE_EFFECT_FROSTBITE: status = RHH_BATTLE_STATUS_FROSTBITE; break;
    default: return;
    }
    u32 origin;
    switch (trigger)
    {
    case TRIGGER_ON_MOVE: origin = RHH_BATTLE_STATUS_FROM_MOVE; break;
    case TRIGGER_ON_ABILITY: origin = RHH_BATTLE_STATUS_FROM_ABILITY; break;
    case TRIGGER_ON_PROTECT: origin = RHH_BATTLE_STATUS_FROM_PROTECT; break;
    default: return;
    }
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    struct RhhBattleStatusApplication context = {.struct_size = sizeof(context), .status = status, .trigger = origin,
        .source_battler = source < gBattlersCount ? source : RHH_BATTLE_NO_BATTLER, .target_battler = target,
        .before_raw = before, .after_raw = gBattleMons[target].status1, .run_id = sRunId, .battle_id = sBattleId,
        .sequence = sStatusApplicationSequence++, .move = origin == RHH_BATTLE_STATUS_FROM_MOVE ? BattleMoveKey(gCurrentMove) : 0};
    for (u32 i = 0; i < battle.participant_count; ++i)
    {
        const struct RhhBattleParticipant *participant = &battle.participants[i];
        if (participant->battler == context.source_battler)
        { context.source = *participant; context.flags |= RHH_BATTLE_STATUS_SOURCE_AVAILABLE; }
        if (participant->battler == target)
        { context.target = *participant; context.flags |= RHH_BATTLE_STATUS_TARGET_AVAILABLE; }
    }
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLER_STATUS_APPLIED,
        .run_seed = RhhModsRunSeed(), .event_key = context.move, .subject = context.target.identity,
        .ordinal = context.sequence, .args = {target, status, origin, context.source_battler}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhBattleStatusAttribution attribution = {.struct_size = sizeof(attribution)};
    if (trigger == TRIGGER_ON_ABILITY && sPendingInnateStatus.kind
        && source == sPendingInnateStatusSource && target == sPendingInnateStatusTarget)
        attribution = sPendingInnateStatus;
    if (presentationReady) BeginStatusReaction(STATUS_PRESENT_APPLICATION);
    sStatusApplicationContext = &context; sStatusAttributionContext = &attribution;
    RhhModsDispatch(&event, &decision);
    sStatusAttributionContext = NULL; sStatusApplicationContext = NULL;
    EndStatusReaction();
    gRngValue = rng; gRng2Value = rng2;
}
static u32 BattleStatusConditions(u32 status)
{
    return (status & STATUS1_SLEEP ? RHH_BATTLE_CONDITION_SLEEP : 0)
        | (status & STATUS1_POISON ? RHH_BATTLE_CONDITION_POISON : 0)
        | (status & STATUS1_BURN ? RHH_BATTLE_CONDITION_BURN : 0)
        | (status & STATUS1_FREEZE ? RHH_BATTLE_CONDITION_FREEZE : 0)
        | (status & STATUS1_PARALYSIS ? RHH_BATTLE_CONDITION_PARALYSIS : 0)
        | (status & STATUS1_TOXIC_POISON ? RHH_BATTLE_CONDITION_TOXIC : 0)
        | (status & STATUS1_FROSTBITE ? RHH_BATTLE_CONDITION_FROSTBITE : 0);
}
static int ItemStatusContext(struct RhhBattleItemStatusChange *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sItemStatusContext
        || (event != RHH_BATTLER_ITEM_STATUS_CHANGED
            && !(event == RHH_BATTLE_CONDITION_PROPOSAL && sConditionProposalContext && sConditionProposalContext->origin_event == RHH_BATTLER_ITEM_STATUS_CHANGED)
            && !(event == RHH_BATTLE_CONDITION_CHANGED && sConditionContext && sConditionOrigin == RHH_BATTLER_ITEM_STATUS_CHANGED))) return -1;
    *out = *sItemStatusContext; return 0;
}
void ModsBattleItemStatusChanged(u32 battler, u32 item, u32 origin, u32 before, bool32 presentationReady)
{
    if (!ConditionsAvailable() || RhhModsCurrentEvent() || sItemStatusContext
        || !RhhModsHasHook(RHH_BATTLER_ITEM_STATUS_CHANGED) || sItemStatusSequence == UINT64_MAX
        || battler >= gBattlersCount || origin < RHH_BATTLE_ITEM_HELD || origin > RHH_BATTLE_ITEM_BAG
        || (origin == RHH_BATTLE_ITEM_BAG && gAiLogicData && (gAiLogicData->aiCalcInProgress
            || gAiLogicData->aiPredictionInProgress || gAiLogicData->switchInCalc))) return;
    u32 after = gBattleMons[battler].status1;
    u32 beforeConditions = BattleStatusConditions(before), afterConditions = BattleStatusConditions(after);
    if (beforeConditions == afterConditions) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    struct RhhBattleItemStatusChange context = {.struct_size = sizeof(context), .battler = battler, .origin = origin,
        .before_raw = before, .after_raw = after, .before_conditions = beforeConditions, .after_conditions = afterConditions,
        .added = afterConditions & ~beforeConditions, .removed = beforeConditions & ~afterConditions,
        .run_id = sRunId, .battle_id = sBattleId, .sequence = sItemStatusSequence++,
        .item = item < ITEMS_COUNT ? RhhContentKeyForId(RHH_ITEM, item) : 0};
    for (u32 i = 0; i < battle.participant_count; ++i)
        if (battle.participants[i].battler == battler)
        { context.participant = battle.participants[i]; context.participant_available = TRUE; break; }
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLER_ITEM_STATUS_CHANGED,
        .run_seed = RhhModsRunSeed(), .event_key = context.item, .subject = context.participant.identity,
        .ordinal = context.sequence, .args = {battler, context.added, context.removed, origin}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    if (origin == RHH_BATTLE_ITEM_BAG && sBagStatusBattleId == sBattleId && !sStatusReactionDispatch
        && sStatusReactionCount < ARRAY_COUNT(sStatusReactions)) sStatusReactionDispatch = &sBagStatusReaction;
    else if (presentationReady) BeginStatusReaction(STATUS_PRESENT_ITEM);
    sItemStatusContext = &context; RhhModsDispatch(&event, &decision); sItemStatusContext = NULL;
    if (sStatusReactionDispatch == &sBagStatusReaction) sStatusReactionDispatch = NULL;
    else EndStatusReaction();
    gRngValue = rng; gRng2Value = rng2;
}
static int StatusReactionAnimation(u32 battler, u32 animation);
static int StatusReactionHeal(u32 battler, u32 numerator, u32 denominator);
static int StatusReactionStat(u32 battler, u32 stat, s32 stages);
static int StatusReactionStatFrom(u32 source, u32 battler, u32 stat, s32 stages);
static int StatusReactionStatusFrom(u32 source, u32 battler, u32 status);
static int StatusReactionDamage(u32 battler, u32 numerator, u32 denominator, u32 flags);
static int StatusReactionCure(u32 battler, u32 conditions);
static int StatusCureContext(struct RhhBattleStatusCure *out);
static int StatusCureAttribution(struct RhhBattleStatusCureAttribution *out);
static int StatusCureMove(RhhContentKey *out);
static int PartyStatusCureContext(struct RhhBattlePartyStatusCure *out);
static int SpeedContext(struct RhhBattleSpeedContext *out);
static int BattleEnvironment(struct RhhBattleEnvironmentState *out);
static int BattleEnvironmentObservation(struct RhhBattleEnvironmentObservation *out);
static int BattleEnvironmentCommit(struct RhhBattleEnvironmentCommit *out);
static int BattleEnvironmentTimerTick(struct RhhBattleEnvironmentTimerTick *out);
static int BattleWeatherSuppressionChange(struct RhhBattleWeatherSuppressionChange *out);
static int BattleWeatherGasSuppressionChange(struct RhhBattleWeatherSuppressionChange *out);
static int BattleWeatherPresenceSuppressionChange(struct RhhBattleWeatherSuppressionChange *out);
static int PartyItemStatusCureContext(struct RhhBattlePartyItemStatusCure *out);
static int BattleFieldRules(struct RhhBattleFieldRules *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable()) return -1;
    u32 active = 0, context = 0;
    if (gFieldStatuses & STATUS_FIELD_MAGIC_ROOM) active |= RHH_BATTLE_FIELD_MAGIC_ROOM;
    if (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) active |= RHH_BATTLE_FIELD_TRICK_ROOM;
    if (gFieldStatuses & STATUS_FIELD_WONDER_ROOM) active |= RHH_BATTLE_FIELD_WONDER_ROOM;
    if (gFieldStatuses & STATUS_FIELD_MUDSPORT) active |= RHH_BATTLE_FIELD_MUD_SPORT;
    if (gFieldStatuses & STATUS_FIELD_WATERSPORT) active |= RHH_BATTLE_FIELD_WATER_SPORT;
    if (gFieldStatuses & STATUS_FIELD_GRAVITY) active |= RHH_BATTLE_FIELD_GRAVITY;
    if (gFieldStatuses & STATUS_FIELD_ION_DELUGE) active |= RHH_BATTLE_FIELD_ION_DELUGE;
    if (gFieldStatuses & STATUS_FIELD_FAIRY_LOCK) active |= RHH_BATTLE_FIELD_FAIRY_LOCK;
    if (gAiLogicData)
    {
        if (gAiLogicData->aiCalcInProgress) context |= RHH_BATTLE_FIELD_AI_CALCULATION;
        if (gAiLogicData->aiPredictionInProgress) context |= RHH_BATTLE_FIELD_AI_PREDICTION;
        if (gAiLogicData->switchInCalc) context |= RHH_BATTLE_FIELD_AI_SWITCH;
    }
    struct RhhBattleFieldRules result = {.struct_size = sizeof(result), .active = active, .context_flags = context,
        .turn = gBattleTurnCounter, .magic_room_turns = gFieldTimers.magicRoomTimer,
        .trick_room_turns = gFieldTimers.trickRoomTimer, .wonder_room_turns = gFieldTimers.wonderRoomTimer,
        .mud_sport_turns = gFieldTimers.mudSportTimer, .water_sport_turns = gFieldTimers.waterSportTimer,
        .gravity_turns = gFieldTimers.gravityTimer, .fairy_lock_turns = gFieldTimers.fairyLockTimer,
        .run_id = sRunId, .battle_id = sBattleId};
    *out = result; return 0;
}
static struct RhhBattleFieldRules sFieldRulesSample;
static const struct RhhBattleFieldRulesObservation *sFieldRulesObservation;
static bool32 sFieldRulesValid;
static u64 sFieldRulesSequence;
static const struct RhhBattleFieldRulesCommit *sFieldRulesCommit;
static u64 sFieldRulesCommitSequence;
static int BattleFieldRulesObservation(struct RhhBattleFieldRulesObservation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sFieldRulesObservation
        || RhhModsCurrentEvent() != RHH_BATTLE_FIELD_RULES_OBSERVED) return -1;
    *out = *sFieldRulesObservation; return 0;
}
static int BattleFieldRulesCommit(struct RhhBattleFieldRulesCommit *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sFieldRulesCommit
        || RhhModsCurrentEvent() != RHH_BATTLE_FIELD_RULES_COMMITTED) return -1;
    *out = *sFieldRulesCommit; return 0;
}
static int BattleSports(struct RhhBattleSports *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhBattleFieldRules field = {.struct_size = sizeof(field)};
    if (BattleFieldRules(&field)) return -1;
    const u32 sportMask = RHH_BATTLE_FIELD_MUD_SPORT | RHH_BATTLE_FIELD_WATER_SPORT;
    struct RhhBattleSports result = {.struct_size = sizeof(result),
        .mode = B_SPORT_TURNS >= GEN_6 ? RHH_BATTLE_SPORT_FIELD_TIMER : RHH_BATTLE_SPORT_BATTLER_VOLATILE,
        .battler_count = gBattlersCount, .flags = gBattleStruct->isSkyBattle ? RHH_BATTLE_SPORTS_SKY_BATTLE : 0,
        .field_active = field.active & sportMask, .mud_turns = field.mud_sport_turns, .water_turns = field.water_sport_turns,
        .context_flags = field.context_flags, .turn = field.turn, .run_id = field.run_id, .battle_id = field.battle_id};
    for (u32 i = 0; i < gBattlersCount; ++i)
    {
        if (gBattleMons[i].volatiles.mudSport) result.mud_battlers |= 1u << i;
        if (gBattleMons[i].volatiles.waterSport) result.water_battlers |= 1u << i;
    }
    result.effective = result.field_active;
    if (result.mode == RHH_BATTLE_SPORT_BATTLER_VOLATILE)
    {
        if (result.mud_battlers) result.effective |= RHH_BATTLE_FIELD_MUD_SPORT;
        if (result.water_battlers) result.effective |= RHH_BATTLE_FIELD_WATER_SPORT;
    }
    *out = result; return 0;
}
static struct RhhBattleSports sSportsSample;
static const struct RhhBattleSportsObservation *sSportsObservation;
static bool32 sSportsValid;
static u64 sSportsSequence;
static int BattleSportsObservation(struct RhhBattleSportsObservation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sSportsObservation
        || RhhModsCurrentEvent() != RHH_BATTLE_SPORTS_OBSERVED) return -1;
    *out = *sSportsObservation; return 0;
}
static int BattleFieldQueueMoveEffect(u32 rule);
static int BattleFieldOfferAiMoveEffect(u32 rule, u32 chance);
static int BattleFieldQueueRoomToggle(u32 rule);
static int BattleFieldOfferAiRoomToggle(u32 rule, u32 chance);
static int BattleFieldQueueFieldMove(u32 rule);
static int BattleFieldOfferAiFieldMove(u32 rule, u32 chance);
static const struct RhhBattleFieldRulesService sFieldRulesService = {
    {sizeof(sFieldRulesService), RHH_SERVICE_VERSION(1, 7)}, BattleFieldRules, BattleFieldRulesObservation,
    BattleFieldRulesCommit, BattleFieldQueueMoveEffect, BattleFieldOfferAiMoveEffect,
    BattleFieldQueueRoomToggle, BattleFieldOfferAiRoomToggle,
    BattleFieldQueueFieldMove, BattleFieldOfferAiFieldMove, BattleSports, BattleSportsObservation
};
static const struct RhhBattleService sService = {
    {sizeof(sService), RHH_SERVICE_VERSION(1, 45)}, Current, Context, TrainerPartySource, TurnContext,
    RhhModsBattleConditionCount, ConditionAt, ConditionInfo, ConditionState, ConditionSet,
    ActiveConditionCount, ActiveConditionAt, ConditionChangeContext, ConditionExpiryMessage, StatStages,
    ConditionProposalContext, ConditionSetResult, BattleNature, ConditionExpiryAnimation, ConditionChangeAnimation, BattleSideOf,
    StatusApplicationContext, ItemStatusContext, StatusAttribution,
    StatusReactionAnimation, StatusReactionHeal, StatusReactionStat, StatusReactionStatFrom, StatusReactionStatusFrom,
    StatusReactionDamage, StatusReactionCure, StatusCureContext, StatusCureAttribution, StatusCureMove, PartyStatusCureContext, SpeedContext,
    BattleEnvironment, BattleEnvironmentObservation, BattleEnvironmentCommit, BattleEnvironmentTimerTick,
    BattleWeatherSuppressionChange, BattleWeatherGasSuppressionChange, BattleWeatherPresenceSuppressionChange,
    PartyItemStatusCureContext, HealthObservationContext
};
static const struct RhhTypeMatchup *sTypeContext;
static RhhContentKey sTypeKeys[NUMBER_OF_MON_TYPES];
static bool32 TypeExcluded(u32 attack, u32 defense)
{
    return !RhhModsHasActiveRun() || attack == TYPE_NONE || defense == TYPE_NONE
        || attack == TYPE_MYSTERY || defense == TYPE_MYSTERY
        || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK)));
}
static u32 TypeInverse(u32 value)
{
    if (value == 0 || value == RHH_TYPE_ONE / 2) return RHH_TYPE_ONE * 2;
    if (value == RHH_TYPE_ONE * 2) return RHH_TYPE_ONE / 2;
    return RHH_TYPE_ONE;
}
static bool32 ValidTypeMultiplier(u64 value)
{ return value == 0 || value == RHH_TYPE_ONE / 2 || value == RHH_TYPE_ONE || value == RHH_TYPE_ONE * 2; }
static u32 TypeAdjustKeys(RhhContentKey attackKey, RhhContentKey defenseKey, u32 base, bool32 inverse, bool32 excluded)
{
    if (excluded || !attackKey || !defenseKey || sTypeContext || !RhhModsHasHook(RHH_TYPE_MATCHUP)) return base;
    struct RhhTypeMatchup context = {.struct_size = sizeof(context), .flags = inverse ? RHH_TYPE_MATCHUP_INVERSE : 0,
        .base = base, .adjusted = base, .effective = inverse ? TypeInverse(base) : base,
        .attack = attackKey, .defense = defenseKey};
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_TYPE_MATCHUP,
        .run_seed = RhhModsRunSeed(), .event_key = attackKey, .subject = defenseKey,
        .args = {base, !!inverse, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = base};
    sTypeContext = &context; RhhModsDispatch(&event, &decision); sTypeContext = NULL;
    return decision.allowed && ValidTypeMultiplier(decision.value) ? decision.value : base;
}
static u32 TypeAdjust(u32 attack, u32 defense, u32 base, bool32 inverse)
{
    if (attack >= NUMBER_OF_MON_TYPES || defense >= NUMBER_OF_MON_TYPES) return base;
    return TypeAdjustKeys(sTypeKeys[attack], sTypeKeys[defense], base, inverse, TypeExcluded(attack, defense));
}
u32 ModsTypeMatchup(u32 attack, u32 defense, u32 base, bool32 inverse)
{ return TypeAdjust(attack, defense, base, inverse); }
static int TypeContext(struct RhhTypeMatchup *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sTypeContext || RhhModsCurrentEvent() != RHH_TYPE_MATCHUP) return -1;
    *out = *sTypeContext; return 0;
}
static int TypeChart(RhhContentKey attack, RhhContentKey defense, u32 flags, struct RhhTypeMatchup *out)
{
    u32 a = UINT32_MAX, d = UINT32_MAX, base, adjusted;
    if (!out || out->struct_size < sizeof(*out) || sTypeContext || (flags & ~RHH_TYPE_MATCHUP_INVERSE)) return -1;
    if (!RhhContentResolve(RHH_TYPE, attack, &a) && a < NUMBER_OF_MON_TYPES) attack = sTypeKeys[a];
    if (!RhhContentResolve(RHH_TYPE, defense, &d) && d < NUMBER_OF_MON_TYPES) defense = sTypeKeys[d];
    bool32 custom = !RhhModsCustomTypeMatchup(attack, defense, &base);
    if (custom)
    {
        RhhContentResolve(RHH_TYPE, attack, &a);
        RhhContentResolve(RHH_TYPE, defense, &d);
        adjusted = TypeAdjustKeys(attack, defense, base, !!(flags & RHH_TYPE_MATCHUP_INVERSE), TypeExcluded(a, d));
    }
    else
    {
        if (RhhContentResolve(RHH_TYPE, attack, &a) || RhhContentResolve(RHH_TYPE, defense, &d)
            || a >= NUMBER_OF_MON_TYPES || d >= NUMBER_OF_MON_TYPES) return -1;
        base = gTypeEffectivenessTable[a][d];
        adjusted = TypeAdjust(a, d, base, !!(flags & RHH_TYPE_MATCHUP_INVERSE));
    }
    struct RhhTypeMatchup result = {.struct_size = sizeof(result), .flags = flags | (TypeExcluded(a, d) ? RHH_TYPE_MATCHUP_EXCLUDED : 0),
        .base = base, .adjusted = adjusted, .effective = flags & RHH_TYPE_MATCHUP_INVERSE ? TypeInverse(adjusted) : adjusted,
        .attack = custom ? attack : sTypeKeys[a], .defense = custom ? defense : sTypeKeys[d]};
    *out = result; return 0;
}
bool32 ModsRuntimeTypeMatchup(u32 attack, u32 defense, bool32 inverse, u32 *effective)
{
    if (attack < NUMBER_OF_MON_TYPES && defense < NUMBER_OF_MON_TYPES) return FALSE;
    /* Invalid or not-yet-assigned slots are neutral, never table indexes. */
    if (!effective) return TRUE;
    *effective = RHH_TYPE_ONE;
    RhhContentKey a = attack < NUMBER_OF_MON_TYPES ? sTypeKeys[attack] : RhhModsCustomTypeKey(attack);
    RhhContentKey d = defense < NUMBER_OF_MON_TYPES ? sTypeKeys[defense] : RhhModsCustomTypeKey(defense);
    u32 base;
    if (a && d && !RhhModsCustomTypeMatchup(a, d, &base))
    {
        u32 adjusted = TypeAdjustKeys(a, d, base, inverse, TypeExcluded(attack, defense));
        *effective = inverse ? TypeInverse(adjusted) : adjusted;
    }
    return TRUE;
}
/* Additional registered types that resist the last move, in stable slot
 * order. Unregistered byte values are never battle type candidates. */
u32 ModsConversion2Types(u32 attack, u8 *types, u32 capacity)
{
    if (!types || !RhhModsHasActiveRun() || !gMain.inBattle
        || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))) return 0;
    u32 count = 0;
    for (u32 type = RHH_RUNTIME_TYPE_FIRST; type < RHH_RUNTIME_TYPE_FIRST + RHH_RUNTIME_TYPE_COUNT; ++type)
    {
        if (!RhhModsCustomTypeKey(type)) continue;
        u32 matchup = GetTypeModifier(attack, type);
        if ((matchup == UQ_4_12(0) || matchup == UQ_4_12(0.5)) && count < capacity)
            types[count++] = type;
    }
    return count;
}
/* Keep engine ordinals out of extended lists. In particular, NONE and MYSTERY
 * are slot sentinels, not types that can contribute a multiplier. */
static int CanonicalCompositionType(RhhContentKey *key)
{
    u32 id, ignored;
    if (!key || !*key) return -1;
    if (!RhhContentResolve(RHH_TYPE, *key, &id))
    {
        if (id == TYPE_NONE || id == TYPE_MYSTERY || id >= NUMBER_OF_MON_TYPES || !sTypeKeys[id]) return -1;
        *key = sTypeKeys[id]; return 0;
    }
    return RhhModsCustomTypeMatchup(*key, *key, &ignored);
}
static int TypeCompose(RhhContentKey attack, const RhhContentKey *defenses, u32 count, u32 flags,
                       struct RhhTypeComposition *out)
{
    if (!out || out->struct_size < sizeof(*out) || count > RHH_TYPE_COMPOSE_LIMIT
        || (count && !defenses) || (flags & ~RHH_TYPE_MATCHUP_INVERSE) || sTypeContext
        || CanonicalCompositionType(&attack)) return -1;
    RhhContentKey distinct[RHH_TYPE_COMPOSE_LIMIT];
    u32 unique = 0;
    /* Validate the entire request before invoking any chart hooks. */
    for (u32 i = 0; i < count; ++i)
    {
        RhhContentKey key = defenses[i];
        if (CanonicalCompositionType(&key)) return -1;
        u32 j;
        for (j = 0; j < unique; ++j) if (distinct[j] == key) break;
        if (j == unique) distinct[unique++] = key;
    }
    s32 exponent = 0; bool32 immune = FALSE, excluded = FALSE;
    for (u32 i = 0; i < unique; ++i)
    {
        struct RhhTypeMatchup matchup = {.struct_size = sizeof(matchup)};
        if (TypeChart(attack, distinct[i], flags, &matchup)) return -1;
        excluded |= !!(matchup.flags & RHH_TYPE_MATCHUP_EXCLUDED);
        if (!matchup.effective) immune = TRUE;
        else if (matchup.effective == RHH_TYPE_ONE / 2) --exponent;
        else if (matchup.effective == RHH_TYPE_ONE * 2) ++exponent;
        else if (matchup.effective != RHH_TYPE_ONE) return -1;
    }
    struct RhhTypeComposition result = {.struct_size = sizeof(result), .flags = flags,
        .count = unique, .effective = RHH_TYPE_ONE};
    if (excluded) result.flags |= RHH_TYPE_MATCHUP_EXCLUDED;
    if (immune) result.effective = 0;
    else if (exponent >= 20) { result.effective = UINT32_MAX; result.flags |= RHH_TYPE_COMPOSITION_SATURATED; }
    else if (exponent < -12) { result.effective = 1; result.flags |= RHH_TYPE_COMPOSITION_SATURATED; }
    else if (exponent >= 0) result.effective <<= exponent;
    else result.effective >>= -exponent;
    *out = result; return 0;
}
static int AddType(struct RhhTypeList *out, u32 type)
{
    if (type == TYPE_NONE || type == TYPE_MYSTERY) return 0;
    RhhContentKey key = type < NUMBER_OF_MON_TYPES ? sTypeKeys[type] : RhhModsCustomTypeKey(type);
    if (!key) return -1;
    for (u32 i = 0; i < out->count; ++i) if (out->types[i] == key) return 0;
    if (out->count == 3) return -1;
    out->types[out->count++] = key; return 0;
}
static int TypeSlotForKey(RhhContentKey key, u32 *slot)
{
    if (!RhhContentResolve(RHH_TYPE, key, slot))
        return *slot == TYPE_NONE || *slot == TYPE_MYSTERY || *slot >= NUMBER_OF_MON_TYPES ? -1 : 0;
    return RhhModsCustomTypeSlot(key, slot);
}
static const struct RhhTeraTypeContext *sTeraTypeContext;
static const struct Pokemon *sTeraTypeSource;
static int TeraTypeContext(struct RhhTeraTypeContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sTeraTypeContext
        || RhhModsCurrentEvent() != RHH_BATTLE_TERA_TYPE) return -1;
    *out = *sTeraTypeContext; return 0;
}
static int TeraOwnedContext(struct RhhOwnedMonSnapshot *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sTeraTypeContext || !sTeraTypeSource
        || RhhModsCurrentEvent() != RHH_BATTLE_TERA_TYPE || !ConditionsAvailable()) return -1;
    u32 battler = sTeraTypeContext->battler, slot = gBattlerPartyIndexes[battler];
    const struct BattlePokemon *mon = &gBattleMons[battler];
    if (slot >= PARTY_SIZE || GetBattlerTrainer(battler) != B_TRAINER_PLAYER
        || sTeraTypeSource != &gParties[B_TRAINER_PLAYER][slot] || mon->volatiles.transformed
        || gIsDebugBattle || IsAiVsAiBattle()
        || (gBattleTypeFlags & (BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_EREADER_TRAINER))) return -1;
    return RhhModsOwnedBattleTeraMon(slot, mon->level,
                                    (u64)mon->otId << 32 | mon->personality, out);
}
u32 ModsBattleTeraType(u32 battler, u32 base, const struct Pokemon *source)
{
    if (!ConditionsAvailable() || !RhhModsHasHook(RHH_BATTLE_TERA_TYPE) || sTeraTypeContext
        || battler >= gBattlersCount || base >= NUMBER_OF_MON_TYPES
        || base == TYPE_NONE || base == TYPE_MYSTERY || !sTypeKeys[base]
        || gBattleMons[battler].species >= NUM_SPECIES) return base;
    RhhContentKey species = RhhContentKeyForId(RHH_SPECIES, gBattleMons[battler].species);
    if (!species) return base;
    struct RhhTeraTypeContext context = {.struct_size = sizeof(context), .battler = battler,
        .battle_id = sBattleId, .species = species, .base_type = sTypeKeys[base]};
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_TERA_TYPE,
        .run_seed = RhhModsRunSeed(), .event_key = species, .subject = context.base_type,
        .args = {battler}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = context.base_type};
    sTeraTypeContext = &context; sTeraTypeSource = source;
    RhhModsDispatch(&event, &decision); sTeraTypeContext = NULL; sTeraTypeSource = NULL;
    u32 slot;
    return decision.allowed && !TypeSlotForKey(decision.value, &slot) ? slot : base;
}
static const struct RhhSpeciesTypesContext *sSpeciesTypesContext;
static struct RhhExtendedTypeList sSpeciesTypesProposal;
static bool32 sSpeciesTypesOffered;
static RhhContentKey sSpeciesTypeKeys[NUM_SPECIES];
static int SpeciesTypesContext(struct RhhSpeciesTypesContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sSpeciesTypesContext
        || RhhModsCurrentEvent() != RHH_SPECIES_TYPES) return -1;
    *out = *sSpeciesTypesContext; return 0;
}
static int SpeciesTypesProposal(struct RhhTypeList *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sSpeciesTypesContext
        || RhhModsCurrentEvent() != RHH_SPECIES_TYPES || sSpeciesTypesProposal.count > 3) return -1;
    struct RhhTypeList result = {.struct_size = sizeof(result), .count = sSpeciesTypesProposal.count};
    memcpy(result.types, sSpeciesTypesProposal.types, result.count * sizeof(result.types[0]));
    *out = result; return 0;
}
static int SpeciesTypesProposalList(struct RhhExtendedTypeList *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sSpeciesTypesContext
        || RhhModsCurrentEvent() != RHH_SPECIES_TYPES) return -1;
    *out = sSpeciesTypesProposal; return 0;
}
static int OfferSpeciesTypesList(const struct RhhExtendedTypeList *types)
{
    int stage = RhhModsCurrentStage();
    if (!sSpeciesTypesContext || RhhModsCurrentEvent() != RHH_SPECIES_TYPES
        || (stage != RHH_TRANSFORM && stage != RHH_FINALIZE)
        || !types || types->struct_size < sizeof(*types) || !types->count || types->count > RHH_ACTIVE_TYPE_LIMIT) return -1;
    struct RhhExtendedTypeList result = {.struct_size = sizeof(result), .count = types->count};
    for (u32 i = 0; i < types->count; ++i)
    {
        u32 id;
        if (TypeSlotForKey(types->types[i], &id)) return -1;
        for (u32 j = 0; j < i; ++j) if (types->types[j] == types->types[i]) return -1;
        result.types[i] = types->types[i];
    }
    sSpeciesTypesProposal = result; sSpeciesTypesOffered = TRUE; return 0;
}
static int OfferSpeciesTypes(const struct RhhTypeList *types)
{
    if (!types || types->struct_size < sizeof(*types) || !types->count || types->count > 3) return -1;
    for (u32 i = types->count; i < 3; ++i) if (types->types[i]) return -1;
    struct RhhExtendedTypeList result = {.struct_size = sizeof(result), .count = types->count};
    memcpy(result.types, types->types, types->count * sizeof(result.types[0]));
    return OfferSpeciesTypesList(&result);
}
static int ResolveSpeciesTypes(u32 species, struct RhhExtendedTypeList *out, bool32 *overridden)
{
    if (!out || species >= NUM_SPECIES) return -1;
    if (overridden) *overridden = FALSE;
    struct RhhTypeList original = {.struct_size = sizeof(original)};
    if (AddType(&original, GetSpeciesType(species, 0)) || AddType(&original, GetSpeciesType(species, 1))) return -1;
    struct RhhExtendedTypeList originalList = {.struct_size = sizeof(originalList), .count = original.count};
    memcpy(originalList.types, original.types, original.count * sizeof(original.types[0]));
    if (!RhhModsHasActiveRun() || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK)))
        || sSpeciesTypesContext || !RhhModsHasHook(RHH_SPECIES_TYPES)) { *out = originalList; return 0; }
    if (!sSpeciesTypeKeys[species]) sSpeciesTypeKeys[species] = RhhContentKeyForId(RHH_SPECIES, species);
    if (!sSpeciesTypeKeys[species]) { *out = originalList; return 0; }
    struct RhhSpeciesTypesContext context = {.struct_size = sizeof(context), .species = sSpeciesTypeKeys[species], .original = original};
    sSpeciesTypesProposal = originalList; sSpeciesTypesOffered = FALSE;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_SPECIES_TYPES,
        .run_seed = RhhModsRunSeed(), .event_key = context.species, .subject = context.species,
        .args = {original.count, 0, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sSpeciesTypesContext = &context; RhhModsDispatch(&event, &decision); sSpeciesTypesContext = NULL;
    bool32 usedRng = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (overridden) *overridden = decision.allowed && !usedRng && sSpeciesTypesOffered;
    *out = decision.allowed && !usedRng ? sSpeciesTypesProposal : originalList; return 0;
}
void ModsSpeciesTypes(u32 species, u8 *types)
{
    if (!types || species >= NUM_SPECIES) return;
    struct RhhExtendedTypeList result; bool32 overridden;
    if (ResolveSpeciesTypes(species, &result, &overridden) || !overridden || !result.count) return;
    u32 ids[3];
    for (u32 i = 0; i < result.count && i < 3; ++i)
        if (TypeSlotForKey(result.types[i], &ids[i])) return;
    types[0] = ids[0]; types[1] = result.count > 1 ? ids[1] : ids[0];
    types[2] = result.count > 2 ? ids[2] : TYPE_MYSTERY;
}
void ModsSpeciesTypeList(u32 species, struct BattleTypeList *types)
{
    if (!types || species >= NUM_SPECIES) return;
    struct RhhExtendedTypeList result; bool32 overridden;
    if (ResolveSpeciesTypes(species, &result, &overridden) || !overridden || !result.count) return;
    struct BattleTypeList converted = {0};
    for (u32 i = 0; i < result.count; ++i)
    {
        u32 slot;
        if (TypeSlotForKey(result.types[i], &slot)) return;
        BattleTypeListAdd(&converted, slot);
    }
    *types = converted;
}
void ModsBattleMonSpeciesTypes(struct BattlePokemon *mon)
{
    if (!mon) return;
    struct BattleTypeList types = {0};
    BattleTypeListAdd(&types, GetSpeciesType(mon->species, 0));
    BattleTypeListAdd(&types, GetSpeciesType(mon->species, 1));
    ModsSpeciesTypeList(mon->species, &types);
    mon->extraTypeCount = 0;
    mon->addedTypeValid = FALSE;
    memset(mon->extraTypes, 0, sizeof(mon->extraTypes));
    if (!types.count) return;
    mon->types[0] = types.types[0];
    mon->types[1] = types.count > 1 ? types.types[1] : types.types[0];
    mon->types[2] = types.count > 2 ? types.types[2] : TYPE_MYSTERY;
    for (u32 i = 3; i < types.count; ++i)
        mon->extraTypes[mon->extraTypeCount++] = types.types[i];
}
static int SpeciesTypesList(RhhContentKey species, struct RhhExtendedTypeList *out)
{
    u32 id; struct RhhContentInfo info = {.struct_size = sizeof(info)};
    if (!out || out->struct_size < sizeof(*out) || RhhContentResolve(RHH_SPECIES, species, &id)
        || id >= NUM_SPECIES || RhhContentInfoFor(RHH_SPECIES, species, &info) || !(info.flags & RHH_CONTENT_ENABLED)) return -1;
    struct RhhExtendedTypeList result;
    if (sSpeciesTypesContext || ResolveSpeciesTypes(id, &result, NULL)) return -1;
    *out = result; return 0;
}
static int SpeciesTypes(RhhContentKey species, struct RhhTypeList *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    struct RhhExtendedTypeList full = {.struct_size = sizeof(full)};
    if (SpeciesTypesList(species, &full) || full.count > 3) return -1;
    struct RhhTypeList result = {.struct_size = sizeof(result), .count = full.count};
    memcpy(result.types, full.types, full.count * sizeof(result.types[0]));
    *out = result; return 0;
}
static int BattlerTypesList(u32 battler, struct RhhExtendedTypeList *out)
{
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (!out || out->struct_size < sizeof(*out) || sTeraTypeContext || Current(&battle)) return -1;
    bool32 present = FALSE;
    for (u32 i = 0; i < battle.participant_count; ++i) if (battle.participants[i].battler == battler) present = TRUE;
    if (!present) return -1;
    struct BattleTypeList types; GetBattlerTypeList(battler, FALSE, &types);
    struct RhhExtendedTypeList result = {.struct_size = sizeof(result), .count = types.count};
    if (types.count > RHH_ACTIVE_TYPE_LIMIT) return -1;
    for (u32 i = 0; i < types.count; ++i)
    {
        u32 type = types.types[i];
        result.types[i] = type < NUMBER_OF_MON_TYPES ? sTypeKeys[type] : RhhModsCustomTypeKey(type);
        if (!result.types[i]) return -1;
    }
    *out = result; return 0;
}
struct TypeObservationSample {
    bool32 valid;
    struct RhhBattleParticipant participant;
    struct RhhExtendedTypeList raw, effective;
};
static struct TypeObservationSample sTypeSamples[MAX_BATTLERS_COUNT];
static const struct RhhBattlerTypeObservation *sTypeObservation;
static u64 sTypeObservationSequence;
struct PublicTypeObservationSample {
    bool32 valid;
    u64 identity;
    u32 owner, partySlot;
    struct RhhPublicBattlerTypes view;
};
static struct PublicTypeObservationSample sPublicTypeSamples[MAX_BATTLERS_COUNT];
static const struct RhhPublicBattlerTypeObservation *sPublicTypeObservation;
static u64 sPublicTypeObservationSequence;
static void ObservePublicBattleTypes(void);
static int BattlerTypeObservationContext(struct RhhBattlerTypeObservation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sTypeObservation
        || RhhModsCurrentEvent() != RHH_BATTLER_TYPES_OBSERVED) return -1;
    *out = *sTypeObservation; return 0;
}
static int RawBattlerTypesList(u32 battler, struct RhhExtendedTypeList *out)
{
    struct BattleTypeList types;
    GetBattlerRawTypeList(battler, &types);
    if (types.count > RHH_ACTIVE_TYPE_LIMIT) return -1;
    struct RhhExtendedTypeList result = {.struct_size = sizeof(result), .count = types.count};
    for (u32 i = 0; i < types.count; ++i)
    {
        u32 type = types.types[i];
        result.types[i] = type < NUMBER_OF_MON_TYPES ? sTypeKeys[type] : RhhModsCustomTypeKey(type);
        if (!result.types[i]) return -1;
    }
    *out = result; return 0;
}
static bool32 TypeListsEqual(const struct RhhExtendedTypeList *a, const struct RhhExtendedTypeList *b)
{
    return a->count == b->count && !memcmp(a->types, b->types, a->count * sizeof(a->types[0]));
}
void RhhModsObserveBattleTypes(void)
{
    ObservePublicBattleTypes();
    if (RhhModsCurrentEvent() || sTypeObservation) return;
    if (!ConditionsAvailable()) { memset(sTypeSamples, 0, sizeof(sTypeSamples)); return; }
    if (gAiLogicData && (gAiLogicData->aiCalcInProgress || gAiLogicData->aiPredictionInProgress
        || gAiLogicData->switchInCalc)) return;
    if (!RhhModsHasHook(RHH_BATTLER_TYPES_OBSERVED)) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    struct RhhBattlerTypeObservation changes[MAX_BATTLERS_COUNT];
    u32 count = 0;
    for (u32 battler = 0; battler < MAX_BATTLERS_COUNT; ++battler)
    {
        const struct RhhBattleParticipant *participant = NULL;
        for (u32 i = 0; i < battle.participant_count; ++i)
            if (battle.participants[i].battler == battler) { participant = &battle.participants[i]; break; }
        struct RhhExtendedTypeList raw = {.struct_size = sizeof(raw)}, effective = {.struct_size = sizeof(effective)};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        int failed = !participant || RawBattlerTypesList(battler, &raw) || BattlerTypesList(battler, &effective);
        gRngValue = rng; gRng2Value = rng2;
        if (failed) { sTypeSamples[battler].valid = FALSE; continue; }
        struct TypeObservationSample *previous = &sTypeSamples[battler];
        if (previous->valid && previous->participant.identity == participant->identity
            && previous->participant.owner == participant->owner && previous->participant.party_slot == participant->party_slot)
        {
            u32 changed = (!TypeListsEqual(&previous->raw, &raw) ? RHH_BATTLER_TYPES_RAW_CHANGED : 0)
                | (!TypeListsEqual(&previous->effective, &effective) ? RHH_BATTLER_TYPES_EFFECTIVE_CHANGED : 0);
            if (changed)
                changes[count++] = (struct RhhBattlerTypeObservation){.struct_size = sizeof(changes[0]), .changed = changed,
                    .before_participant = previous->participant, .after_participant = *participant,
                    .before_raw = previous->raw, .after_raw = raw,
                    .before_effective = previous->effective, .after_effective = effective};
        }
        *previous = (struct TypeObservationSample){.valid = TRUE, .participant = *participant, .raw = raw, .effective = effective};
    }
    for (u32 i = 0; i < count && ConditionsAvailable() && battle.battle_id == sBattleId; ++i)
    {
        if (sTypeObservationSequence == UINT64_MAX) return;
        changes[i].sequence = sTypeObservationSequence++;
        const struct RhhBattleParticipant *p = &changes[i].after_participant;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLER_TYPES_OBSERVED,
            .run_seed = RhhModsRunSeed(), .event_key = p->species, .subject = p->identity,
            .ordinal = changes[i].sequence, .args = {p->battler, p->owner, p->party_slot, changes[i].changed}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        sTypeObservation = &changes[i]; RhhModsDispatch(&event, &decision); sTypeObservation = NULL;
        gRngValue = rng; gRng2Value = rng2;
    }
}
static int PublicBattlerTypes(u32 battler, struct RhhPublicBattlerTypes *out)
{
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (!out || out->struct_size < sizeof(*out) || sSpeciesTypesContext || sTeraTypeContext || Current(&battle)
        || !ConditionsAvailable() || (gAiLogicData && (gAiLogicData->aiCalcInProgress
            || gAiLogicData->aiPredictionInProgress || gAiLogicData->switchInCalc))) return -1;
    bool32 present = FALSE;
    for (u32 i = 0; i < battle.participant_count; ++i)
        if (battle.participants[i].battler == battler) present = TRUE;
    if (!present) return -1;
    struct RhhPublicBattlerTypes result = {.struct_size = sizeof(result), .battler = battler,
        .battle_id = battle.battle_id, .types.struct_size = sizeof(result.types)};
    if (B_SHOW_TYPES == SHOW_TYPES_NEVER) result.flags = RHH_PUBLIC_BATTLER_TYPES_UNKNOWN;
    else
    {
        /* The ordinary indicator may lazily initialize the Illusion cache.
         * Querying it must not change subsequent battle behavior. */
        typeof(gBattleStruct->illusion[0]) illusion = gBattleStruct->illusion[battler];
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        struct BattleTypeList types;
        GetBattlerPublicTypeList(battler, &types);
        gBattleStruct->illusion[battler] = illusion; gRngValue = rng; gRng2Value = rng2;
        if (types.count == 1 && types.types[0] == TYPE_MYSTERY)
            result.flags = RHH_PUBLIC_BATTLER_TYPES_UNKNOWN;
        else {
            if (types.count > RHH_ACTIVE_TYPE_LIMIT) return -1;
            result.types.count = types.count;
            for (u32 i = 0; i < types.count; ++i) {
                u32 type = types.types[i];
                result.types.types[i] = type < NUMBER_OF_MON_TYPES ? sTypeKeys[type] : RhhModsCustomTypeKey(type);
                if (!result.types.types[i]) return -1;
            }
        }
    }
    *out = result; return 0;
}
static int PublicBattlerTypeObservationContext(struct RhhPublicBattlerTypeObservation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sPublicTypeObservation
        || RhhModsCurrentEvent() != RHH_PUBLIC_BATTLER_TYPES_OBSERVED) return -1;
    *out = *sPublicTypeObservation; return 0;
}
static void ObservePublicBattleTypes(void)
{
    if (RhhModsCurrentEvent() || sPublicTypeObservation) return;
    if (!ConditionsAvailable()) { memset(sPublicTypeSamples, 0, sizeof(sPublicTypeSamples)); return; }
    if (gAiLogicData && (gAiLogicData->aiCalcInProgress || gAiLogicData->aiPredictionInProgress
        || gAiLogicData->switchInCalc)) return;
    if (!RhhModsHasHook(RHH_PUBLIC_BATTLER_TYPES_OBSERVED)) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    struct RhhPublicBattlerTypeObservation changes[MAX_BATTLERS_COUNT];
    u32 count = 0;
    for (u32 battler = 0; battler < MAX_BATTLERS_COUNT; ++battler)
    {
        const struct RhhBattleParticipant *participant = NULL;
        for (u32 i = 0; i < battle.participant_count; ++i)
            if (battle.participants[i].battler == battler) { participant = &battle.participants[i]; break; }
        struct RhhPublicBattlerTypes current = {.struct_size = sizeof(current)};
        if (!participant || PublicBattlerTypes(battler, &current))
        { sPublicTypeSamples[battler].valid = FALSE; continue; }
        struct PublicTypeObservationSample *previous = &sPublicTypeSamples[battler];
        if (previous->valid && previous->identity == participant->identity
            && previous->owner == participant->owner && previous->partySlot == participant->party_slot
            && (previous->view.flags != current.flags || !TypeListsEqual(&previous->view.types, &current.types)))
            changes[count++] = (struct RhhPublicBattlerTypeObservation){.struct_size = sizeof(changes[0]),
                .before = previous->view, .after = current};
        *previous = (struct PublicTypeObservationSample){.valid = TRUE, .identity = participant->identity,
            .owner = participant->owner, .partySlot = participant->party_slot, .view = current};
    }
    for (u32 i = 0; i < count && ConditionsAvailable() && battle.battle_id == sBattleId; ++i)
    {
        if (sPublicTypeObservationSequence == UINT64_MAX) return;
        changes[i].sequence = sPublicTypeObservationSequence++;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_PUBLIC_BATTLER_TYPES_OBSERVED,
            .run_seed = RhhModsRunSeed(), .ordinal = changes[i].sequence,
            .args = {changes[i].after.battler, changes[i].before.flags, changes[i].after.flags, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        sPublicTypeObservation = &changes[i]; RhhModsDispatch(&event, &decision); sPublicTypeObservation = NULL;
        gRngValue = rng; gRng2Value = rng2;
    }
}
static int AiBattlerTypesList(u32 battler, struct RhhExtendedTypeList *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sAiScoreContext
        || (event != RHH_AI_MOVE_SCORE && event != RHH_AI_EFFECT_PREVIEW)) return -1;
    struct RhhExtendedTypeList checked = {.struct_size = sizeof(checked)};
    if (BattlerTypesList(battler, &checked)) return -1;
    struct BattleTypeList types;
    AI_GetKnownBattlerTypeList(sAiScoreContext->attacker, battler, &types);
    struct RhhExtendedTypeList result = {.struct_size = sizeof(result), .count = types.count};
    if (types.count > RHH_ACTIVE_TYPE_LIMIT) return -1;
    for (u32 i = 0; i < types.count; ++i)
    {
        u32 type = types.types[i];
        result.types[i] = type < NUMBER_OF_MON_TYPES ? sTypeKeys[type] : RhhModsCustomTypeKey(type);
        if (!result.types[i]) return -1;
    }
    *out = result; return 0;
}
static int AiBattlerSpecies(u32 battler, RhhContentKey *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || !sAiScoreContext || (event != RHH_AI_MOVE_SCORE && event != RHH_AI_EFFECT_PREVIEW)) return -1;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (sTeraTypeContext || Current(&battle)) return -1;
    bool32 present = FALSE;
    for (u32 i = 0; i < battle.participant_count; ++i) if (battle.participants[i].battler == battler) present = TRUE;
    if (!present) return -1;
    u32 species = AI_GetKnownBattlerSpecies(sAiScoreContext->attacker, battler);
    RhhContentKey key = species < NUM_SPECIES ? RhhContentKeyForId(RHH_SPECIES, species) : 0;
    if (!key) return -1;
    *out = key; return 0;
}
static u32 PowerTerrain(u32 terrain);
static int GroundingView(u32 battler, struct RhhBattleGrounding *out, bool32 ai, bool32 scenario, u32 terrain)
{
    static const u8 engineTerrains[] = {B_TERRAIN_NONE, B_TERRAIN_GRASSY, B_TERRAIN_MISTY,
        B_TERRAIN_ELECTRIC, B_TERRAIN_PSYCHIC};
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable() || sTeraTypeContext
        || battler >= gBattlersCount || !IsBattlerAlive(battler)
        || (scenario && terrain >= ARRAY_COUNT(engineTerrains))) return -1;
    if (ai)
    {
        if (!sAiScoreContext || !gAiLogicData || (event != RHH_AI_MOVE_SCORE && event != RHH_AI_EFFECT_PREVIEW)) return -1;
    }
    else if (sAiScoreContext || sAiInnateEntryContext || sAiInnateStatContext || sAiInnateFieldContext
             || (gAiLogicData && (gAiLogicData->aiCalcInProgress
                 || gAiLogicData->aiPredictionInProgress || gAiLogicData->switchInCalc))) return -1;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return -1;
    bool32 present = FALSE;
    for (u32 i = 0; i < battle.participant_count; ++i)
        if (battle.participants[i].battler == battler) present = TRUE;
    if (!present) return -1;
    u32 engineTerrain = scenario ? engineTerrains[terrain] : gFieldTimers.terrain;
    struct RhhBattleGrounding result = {.struct_size = sizeof(result), .battler = battler,
        .terrain = scenario ? terrain : PowerTerrain(engineTerrain), .engine_terrain = engineTerrain,
        .turn = battle.turn, .run_id = battle.run_id, .battle_id = battle.battle_id};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    bool32 grounded;
    if (ai)
    {
        enum Ability ability = gAiLogicData->abilities[battler];
        enum HoldEffect item = gAiLogicData->holdEffects[battler];
        struct BattleTypeList types;
        AI_GetKnownBattlerTypeList(sAiScoreContext->attacker, battler, &types);
        grounded = IsBattlerGroundedAssumingFlying(battler, ability, item, BattleTypeListContains(&types, TYPE_FLYING));
        result.flags = RHH_BATTLE_GROUNDING_AI
            | (ability == ABILITY_NONE ? RHH_BATTLE_GROUNDING_ABILITY_NONE_OR_UNKNOWN : 0)
            | (item == HOLD_EFFECT_NONE ? RHH_BATTLE_GROUNDING_ITEM_NONE_OR_UNKNOWN : 0);
    }
    else
    {
        enum Ability ability = GetBattlerAbilityReadOnly(battler, FALSE);
        enum HoldEffect item = GetBattlerHoldEffectReadOnly(battler, ability);
        grounded = IsBattlerGrounded(battler, ability, item);
    }
    bool32 semi = IsSemiInvulnerable(battler, CHECK_ALL);
    if (scenario) result.flags |= RHH_BATTLE_GROUNDING_SCENARIO;
    if (grounded) result.flags |= RHH_BATTLE_GROUNDING_GROUNDED;
    if (semi) result.flags |= RHH_BATTLE_GROUNDING_SEMI_INVULNERABLE;
    if (grounded && !semi && engineTerrain != B_TERRAIN_NONE) result.flags |= RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED;
    gRngValue = rng; gRng2Value = rng2;
    *out = result; return 0;
}
static int GroundingLive(u32 battler, struct RhhBattleGrounding *out) { return GroundingView(battler, out, FALSE, FALSE, 0); }
static int GroundingAi(u32 battler, struct RhhBattleGrounding *out) { return GroundingView(battler, out, TRUE, FALSE, 0); }
static int GroundingPreviewLive(u32 battler, u32 terrain, struct RhhBattleGrounding *out)
{ return GroundingView(battler, out, FALSE, TRUE, terrain); }
static int GroundingPreviewAi(u32 battler, u32 terrain, struct RhhBattleGrounding *out)
{ return GroundingView(battler, out, TRUE, TRUE, terrain); }
static int GroundingEntry(u32 terrain, bool32 projected, struct RhhBattleGrounding *out)
{
    static const u8 engineTerrains[] = {B_TERRAIN_NONE, B_TERRAIN_GRASSY, B_TERRAIN_MISTY,
        B_TERRAIN_ELECTRIC, B_TERRAIN_PSYCHIC};
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable() || !gAiLogicData || sTeraTypeContext
        || !sAiInnateFieldContext || RhhModsCurrentEvent() != RHH_AI_INNATE_ENTRY_FIELD_PREVIEW) return -1;
    struct RhhAiInnateFieldContext field = {.struct_size = sizeof(field)};
    if (projected && RhhModsAiInnateEntryFieldProjected(&field)) return -1;
    if (projected) terrain = field.terrain;
    if (terrain >= ARRAY_COUNT(engineTerrains)) return -1;
    const struct RhhAiInnateEntryContext *entry = &sAiInnateFieldContext->entry;
    u32 battler = entry->battler, slot = entry->party_slot;
    if (entry->battle_id != sBattleId || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || slot >= PARTY_SIZE
        || !entry->hp || entry->hp > gBattleMons[battler].maxHP) return -1;
    struct Pokemon candidate = GetBattlerParty(battler)[slot];
    if (!GetMonData(&candidate, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(&candidate, MON_DATA_SANITY_IS_BAD_EGG)
        || GetMonData(&candidate, MON_DATA_IS_EGG) || GetMonData(&candidate, MON_DATA_SPECIES) != gBattleMons[battler].species
        || GetMonData(&candidate, MON_DATA_LEVEL) != gBattleMons[battler].level
        || GetMonData(&candidate, MON_DATA_PERSONALITY) != gBattleMons[battler].personality
        || GetMonData(&candidate, MON_DATA_OT_ID) != gBattleMons[battler].otId) return -1;
    struct RhhBattleGrounding result = {.struct_size = sizeof(result), .battler = battler, .terrain = terrain,
        .engine_terrain = engineTerrains[terrain], .turn = gBattleTurnCounter, .run_id = sRunId, .battle_id = sBattleId,
        .flags = RHH_BATTLE_GROUNDING_AI | RHH_BATTLE_GROUNDING_SCENARIO | RHH_BATTLE_GROUNDING_AI_ENTRY};
    enum Ability ability = gAiLogicData->abilities[battler];
    enum HoldEffect item = gAiLogicData->holdEffects[battler];
    if (ability == ABILITY_NONE) result.flags |= RHH_BATTLE_GROUNDING_ABILITY_NONE_OR_UNKNOWN;
    if (item == HOLD_EFFECT_NONE) result.flags |= RHH_BATTLE_GROUNDING_ITEM_NONE_OR_UNKNOWN;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    u32 oldSlot = gBattlerPartyIndexes[battler]; gBattlerPartyIndexes[battler] = slot;
    struct BattleTypeList types; GetBattlerTypeList(battler, FALSE, &types);
    bool32 grounded = IsBattlerGroundedAssumingFlying(battler, ability, item, BattleTypeListContains(&types, TYPE_FLYING));
    bool32 semi = IsSemiInvulnerable(battler, CHECK_ALL);
    gBattlerPartyIndexes[battler] = oldSlot; gRngValue = rng; gRng2Value = rng2;
    if (grounded) result.flags |= RHH_BATTLE_GROUNDING_GROUNDED;
    if (semi) result.flags |= RHH_BATTLE_GROUNDING_SEMI_INVULNERABLE;
    if (grounded && !semi && result.engine_terrain != B_TERRAIN_NONE) result.flags |= RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED;
    *out = result; return 0;
}
static int GroundingEntryAi(struct RhhBattleGrounding *out) { return GroundingEntry(0, TRUE, out); }
static int GroundingPreviewEntryAi(u32 terrain, struct RhhBattleGrounding *out) { return GroundingEntry(terrain, FALSE, out); }
static const struct RhhBattleGroundingService sGroundingService = {
    {sizeof(sGroundingService), RHH_SERVICE_VERSION(1, 2)}, GroundingLive, GroundingAi,
    GroundingPreviewLive, GroundingPreviewAi, GroundingEntryAi, GroundingPreviewEntryAi
};
static int BattlerTypes(u32 battler, struct RhhTypeList *out)
{
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (!out || out->struct_size < sizeof(*out) || sTeraTypeContext || Current(&battle)) return -1;
    bool32 present = FALSE;
    for (u32 i = 0; i < battle.participant_count; ++i) if (battle.participants[i].battler == battler) present = TRUE;
    if (!present) return -1;
    struct BattleTypeList types; GetBattlerTypeList(battler, FALSE, &types);
    struct RhhTypeList result = {.struct_size = sizeof(result)};
    for (u32 i = 0; i < types.count; ++i) if (AddType(&result, types.types[i])) return -1;
    *out = result; return 0;
}
static const struct RhhTypeStabContext *sStabContext;
static const struct RhhExtendedTypeStabContext *sExtendedStabContext;
static RhhContentKey sStabMoves[MOVES_COUNT_ALL], sStabSpecies[NUM_SPECIES], sStabAbilities[ABILITIES_COUNT];
static RhhContentKey CachedKey(u32 kind, u32 id, RhhContentKey *cache, u32 count)
{
    if (!id || id >= count) return 0;
    if (!cache[id]) cache[id] = RhhContentKeyForId(kind, id);
    return cache[id];
}
static const u8 sPublicMoveTargets[] = {
    [TARGET_NONE] = RHH_MOVE_TARGET_NONE, [TARGET_SELECTED] = RHH_MOVE_TARGET_SELECTED,
    [TARGET_SMART] = RHH_MOVE_TARGET_SMART, [TARGET_DEPENDS] = RHH_MOVE_TARGET_DEPENDS,
    [TARGET_OPPONENT] = RHH_MOVE_TARGET_OPPONENT, [TARGET_RANDOM] = RHH_MOVE_TARGET_RANDOM,
    [TARGET_BOTH] = RHH_MOVE_TARGET_BOTH, [TARGET_USER] = RHH_MOVE_TARGET_USER,
    [TARGET_ALLY] = RHH_MOVE_TARGET_ALLY, [TARGET_USER_AND_ALLY] = RHH_MOVE_TARGET_USER_AND_ALLY,
    [TARGET_USER_OR_ALLY] = RHH_MOVE_TARGET_USER_OR_ALLY, [TARGET_FOES_AND_ALLY] = RHH_MOVE_TARGET_FOES_AND_ALLY,
    [TARGET_FIELD] = RHH_MOVE_TARGET_FIELD, [TARGET_OPPONENTS_FIELD] = RHH_MOVE_TARGET_OPPONENTS_FIELD,
    [TARGET_ALL_BATTLERS] = RHH_MOVE_TARGET_ALL_BATTLERS
};
static const u8 sEngineMoveTargets[] = {
    [RHH_MOVE_TARGET_NONE] = TARGET_NONE, [RHH_MOVE_TARGET_SELECTED] = TARGET_SELECTED,
    [RHH_MOVE_TARGET_SMART] = TARGET_SMART, [RHH_MOVE_TARGET_DEPENDS] = TARGET_DEPENDS,
    [RHH_MOVE_TARGET_OPPONENT] = TARGET_OPPONENT, [RHH_MOVE_TARGET_RANDOM] = TARGET_RANDOM,
    [RHH_MOVE_TARGET_BOTH] = TARGET_BOTH, [RHH_MOVE_TARGET_USER] = TARGET_USER,
    [RHH_MOVE_TARGET_ALLY] = TARGET_ALLY, [RHH_MOVE_TARGET_USER_AND_ALLY] = TARGET_USER_AND_ALLY,
    [RHH_MOVE_TARGET_USER_OR_ALLY] = TARGET_USER_OR_ALLY, [RHH_MOVE_TARGET_FOES_AND_ALLY] = TARGET_FOES_AND_ALLY,
    [RHH_MOVE_TARGET_FIELD] = TARGET_FIELD, [RHH_MOVE_TARGET_OPPONENTS_FIELD] = TARGET_OPPONENTS_FIELD,
    [RHH_MOVE_TARGET_ALL_BATTLERS] = TARGET_ALL_BATTLERS
};
static bool32 sResolvingMoveTarget;
static u32 PowerWeather(u32 weather);
static u32 PowerTerrain(u32 terrain);
static const struct RhhMoveTargetContext *sMoveTargetContext;
static struct MoveTargetLock {
    bool8 valid;
    u32 battler, move, target, owner, partySlot;
    u64 runId, battleId, identity;
} sMoveTargetLocks[MAX_BATTLERS_COUNT];
static struct MoveTargetCapture { bool8 active, requested; u32 battler, move; } sMoveTargetCapture;
static bool8 sMoveTargetCaptureEligible, sMoveTargetPreview;
static int MoveTargetIdentity(u32 battler, u32 *owner, u32 *slot, u64 *identity)
{
    if (battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || (sPending & (1u << battler))
        || gBattlerPartyIndexes[battler] >= PARTY_SIZE) return -1;
    u32 trainer = GetBattlerTrainer(battler), index = gBattlerPartyIndexes[battler];
    if (trainer >= MAX_BATTLE_TRAINERS) return -1;
    struct Pokemon copy = gParties[trainer][index];
    *owner = trainer; *slot = index;
    *identity = (u64)GetMonData(&copy, MON_DATA_OT_ID) << 32 | GetMonData(&copy, MON_DATA_PERSONALITY);
    return 0;
}
static int MoveTargetLock(void)
{
    if (sMoveTargetPreview || !sMoveTargetContext || RhhModsCurrentEvent() != RHH_MOVE_TARGET_RESOLVE
        || sMoveTargetContext->phase != RHH_MOVE_TARGET_EXECUTION || !sMoveTargetCapture.active || !sMoveTargetCaptureEligible
        || sMoveTargetContext->battler != sMoveTargetCapture.battler) return -1;
    sMoveTargetCapture.requested = TRUE; return 0;
}
void ModsMoveTargetCaptureBegin(u32 battler, u32 move)
{
    sMoveTargetCaptureEligible = FALSE;
    if (!sMoveHealthWindow.active && RhhModsHasHook(RHH_MOVE_RESOLVED) && ConditionsAvailable()
        && battler < gBattlersCount && battler < MAX_BATTLERS_COUNT)
    {
        sMoveHealthWindow.active = TRUE; sMoveHealthWindow.attacker = battler; sMoveHealthWindow.move = move;
        sMoveHealthWindow.source = MoveDamageIdentity(battler);
        for (u32 i = 0; i < gBattlersCount; ++i) MoveHealthEndpoint(i, &sMoveHealthWindow.start[i]);
    }
    sMoveTargetCapture = (struct MoveTargetCapture){.active = TRUE, .battler = battler, .move = move};
    if (battler < MAX_BATTLERS_COUNT) sMoveTargetLocks[battler].valid = FALSE;
}
void ModsMoveTargetCaptureEnd(u32 battler, u32 move, u32 target)
{
    u32 owner, slot; u64 identity;
    if (sMoveTargetCapture.active && sMoveTargetCapture.requested && sMoveTargetCapture.battler == battler
        && sMoveTargetCapture.move == move && battler < MAX_BATTLERS_COUNT && ConditionsAvailable()
        && target < ARRAY_COUNT(sPublicMoveTargets) && !MoveTargetIdentity(battler, &owner, &slot, &identity))
        sMoveTargetLocks[battler] = (struct MoveTargetLock){.valid = TRUE, .battler = battler, .move = move,
            .target = target, .owner = owner, .partySlot = slot, .identity = identity,
            .runId = sRunId, .battleId = sBattleId};
    sMoveTargetCapture = (struct MoveTargetCapture){0};
    sMoveTargetCaptureEligible = FALSE;
}
static int MoveTargetContext(struct RhhMoveTargetContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sMoveTargetContext
        || RhhModsCurrentEvent() != RHH_MOVE_TARGET_RESOLVE) return -1;
    *out = *sMoveTargetContext; return 0;
}
u32 ModsMoveContextualTarget(u32 battler, u32 move, u32 target, u32 phase)
{
    if (!ConditionsAvailable() || !RhhModsHasHook(RHH_MOVE_TARGET_RESOLVE) || sMoveTargetContext
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || move == MOVE_STRUGGLE
        || target >= ARRAY_COUNT(sPublicMoveTargets) || sPublicMoveTargets[target] < RHH_MOVE_TARGET_SELECTED
        || phase < RHH_MOVE_TARGET_SELECTION || phase > RHH_MOVE_TARGET_AI) return target;
    if (phase == RHH_MOVE_TARGET_EXECUTION && !sMoveTargetPreview && !sMoveTargetCapture.active && !RhhModsCurrentEvent()
        && battler == gBattlerAttacker && move == gCurrentMove && sMoveTargetLocks[battler].valid)
    {
        struct MoveTargetLock *lock = &sMoveTargetLocks[battler]; u32 owner, slot; u64 identity;
        if (lock->move == move && lock->runId == sRunId && lock->battleId == sBattleId
            && !MoveTargetIdentity(battler, &owner, &slot, &identity)
            && lock->owner == owner && lock->partySlot == slot && lock->identity == identity) return lock->target;
        lock->valid = FALSE;
    }
    RhhContentKey key = move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, move, sStabMoves, MOVES_COUNT_ALL)
                                               : RhhModsMoveKey(move);
    if (!key) return target;
    u32 base = sPublicMoveTargets[target], strikes = GetMoveInfo(move)->strikeCount;
    RhhContentKey species = RhhContentKeyForId(RHH_SPECIES, gBattleMons[battler].species);
    struct RhhMoveTargetContext context = {.struct_size = sizeof(context), .battler = battler,
        .phase = phase, .target = base, .strike_count = strikes, .weather = PowerWeather(gBattleWeather),
        .terrain = PowerTerrain(gFieldTimers.terrain), .run_id = sRunId,
        .battle_id = sBattleId, .move = key, .species = species};
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_TARGET_RESOLVE,
        .run_seed = RhhModsRunSeed(), .event_key = key, .subject = species,
        .args = {battler, phase, base, strikes}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = base};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sMoveTargetCaptureEligible = !sMoveTargetPreview && sMoveTargetCapture.active && phase == RHH_MOVE_TARGET_EXECUTION
        && sMoveTargetCapture.battler == battler && sMoveTargetCapture.move == move && !RhhModsCurrentEvent();
    sMoveTargetContext = &context; RhhModsDispatch(&event, &decision); sMoveTargetContext = NULL;
    bool32 usedRng = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (usedRng || !decision.allowed || decision.value < RHH_MOVE_TARGET_SELECTED
        || decision.value > RHH_MOVE_TARGET_ALL_BATTLERS
        || (decision.value == RHH_MOVE_TARGET_SMART && strikes <= 1)
        || (decision.value == RHH_MOVE_TARGET_DEPENDS && base != RHH_MOVE_TARGET_DEPENDS))
    { if (sMoveTargetCaptureEligible) sMoveTargetCapture.requested = FALSE; sMoveTargetCaptureEligible = FALSE; return target; }
    sMoveTargetCaptureEligible = FALSE;
    return sEngineMoveTargets[decision.value];
}
static int MoveTargetResolveKey(RhhContentKey key, u32 *move)
{
    const struct RhhModMove *custom = RhhModsFindCustomMove(key);
    if (custom) return !ModsCustomMoveAvailable(custom) || RhhModsMoveSlot(key, 0, move) ? -1 : 0;
    return RhhContentResolve(RHH_MOVE, key, move) || !*move || *move >= MOVES_COUNT_ALL ? -1 : 0;
}
static int MoveTargetPreview(u32 battler, RhhContentKey key, u32 phase, struct RhhMoveTargetPreview *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable() || sMoveTargetPreview || sMoveTargetContext
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || (sPending & (1u << battler))
        || !IsBattlerAlive(battler) || phase < RHH_MOVE_TARGET_SELECTION || phase > RHH_MOVE_TARGET_AI
        || (phase == RHH_MOVE_TARGET_AI && !gAiLogicData)) return -1;
    u32 move;
    if (MoveTargetResolveKey(key, &move)) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    u64 run = sRunId, battle = sBattleId;
    enum Ability lastAbility = gLastUsedAbility; enum Item lastItem = gLastUsedItem;
    enum BattlerId potentialItem = gPotentialItemEffectBattler;
    bool8 savedEligible = sMoveTargetCaptureEligible;
    sMoveTargetPreview = TRUE;
    u32 target = phase == RHH_MOVE_TARGET_SELECTION ? GetBattlerMoveSelectionTargetType(battler, move)
        : phase == RHH_MOVE_TARGET_EXECUTION ? GetBattlerMoveTargetType(battler, move)
        : AI_GetBattlerMoveTargetType(battler, move);
    sMoveTargetPreview = FALSE; sMoveTargetCaptureEligible = savedEligible;
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = lastAbility;
    gLastUsedItem = lastItem; gPotentialItemEffectBattler = potentialItem;
    if (!pure || !ConditionsAvailable() || sRunId != run || sBattleId != battle
        || battler >= gBattlersCount || !IsBattlerAlive(battler) || target >= ARRAY_COUNT(sPublicMoveTargets)) return -1;
    *out = (struct RhhMoveTargetPreview){.struct_size = sizeof(*out), .battler = battler,
        .phase = phase, .target = sPublicMoveTargets[target], .run_id = sRunId, .battle_id = sBattleId, .move = key};
    return 0;
}
static bool32 sResolvingMoveEffect;
static int MoveTargetFilter(u32 battler, RhhContentKey key, u32 candidate, struct RhhMoveTargetFilter *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable() || sMoveTargetPreview
        || sMoveTargetContext || sResolvingMoveEffect || battler >= gBattlersCount
        || candidate >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || candidate >= MAX_BATTLERS_COUNT
        || (sPending & (1u << battler)) || !IsBattlerAlive(battler)) return -1;
    u32 move; if (MoveTargetResolveKey(key, &move)) return -1;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    u64 run = sRunId, battle = sBattleId;
    enum Ability ability = gLastUsedAbility; enum Item item = gLastUsedItem;
    enum BattlerId potentialItem = gPotentialItemEffectBattler;
    bool8 savedEligible = sMoveTargetCaptureEligible;
    sMoveTargetPreview = TRUE;
    bool32 allowed = !(sPending & (1u << candidate)) && CanTargetBattler(battler, candidate, move);
    sMoveTargetPreview = FALSE; sMoveTargetCaptureEligible = savedEligible;
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2; gLastUsedAbility = ability;
    gLastUsedItem = item; gPotentialItemEffectBattler = potentialItem;
    if (!pure || !ConditionsAvailable() || sRunId != run || sBattleId != battle
        || battler >= gBattlersCount || candidate >= gBattlersCount || !IsBattlerAlive(battler)) return -1;
    *out = (struct RhhMoveTargetFilter){.struct_size = sizeof(*out), .battler = battler, .candidate = candidate,
        .allowed = allowed, .run_id = run, .battle_id = battle, .move = key};
    return 0;
}
static u32 AdjustMoveEffectMode(RhhContentKey key, bool32 damage)
{
    if (!key || !RhhModsHasActiveRun() || sResolvingMoveEffect
        || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))))
        return RHH_MOVE_EFFECT_INHERITED;
    RhhContentKey effectKey = RhhModsRegisteredEffectForMove(key);
    const struct RhhModMoveEffect *effect = RhhModsRegisteredEffect(effectKey);
    u32 initial = effect ? effect->mode : RHH_MOVE_EFFECT_INHERITED;
    u32 eventKind = damage ? RHH_MOVE_DAMAGE_EFFECT_MODE : RHH_MOVE_EFFECT_MODE;
    u32 expected = damage ? RHH_MOVE_EFFECT_CUSTOM_DAMAGE : RHH_MOVE_EFFECT_CUSTOM_STATUS;
    if (initial != expected) initial = RHH_MOVE_EFFECT_INHERITED;
    if (!RhhModsHasHook(eventKind)) return initial;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = eventKind,
        .run_seed = RhhModsRunSeed(), .event_key = key};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = initial};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sResolvingMoveEffect = TRUE; RhhModsDispatch(&event, &decision); sResolvingMoveEffect = FALSE;
    bool32 usedRng = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!decision.allowed) return RHH_MOVE_EFFECT_INHERITED;
    if (usedRng) return initial;
    return decision.allowed && decision.value == expected ? expected : RHH_MOVE_EFFECT_INHERITED;
}
u32 ModsMoveEffectMode(u32 move, u32 effect, u32 category, u32 additionalEffects)
{
    if (move == MOVE_STRUGGLE || effect == EFFECT_PLACEHOLDER || category != DAMAGE_CATEGORY_STATUS || additionalEffects)
        return effect;
    RhhContentKey key = move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, move, sStabMoves, MOVES_COUNT_ALL)
                                               : RhhModsMoveKey(move);
    return AdjustMoveEffectMode(key, FALSE) == RHH_MOVE_EFFECT_CUSTOM_STATUS ? EFFECT_MOD_CUSTOM_STATUS : effect;
}
static u32 AdjustMoveTarget(RhhContentKey move, u32 base, u32 strikes)
{
    if (!move || base < RHH_MOVE_TARGET_SELECTED || base > RHH_MOVE_TARGET_ALL_BATTLERS
        || !RhhModsHasActiveRun() || sResolvingMoveTarget || !RhhModsHasHook(RHH_MOVE_BASE_TARGET)
        || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK)))) return base;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_BASE_TARGET,
        .run_seed = RhhModsRunSeed(), .event_key = move, .subject = base};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = base};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sResolvingMoveTarget = TRUE; RhhModsDispatch(&event, &decision); sResolvingMoveTarget = FALSE;
    bool32 usedRng = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    return !usedRng && decision.allowed && decision.value >= RHH_MOVE_TARGET_SELECTED && decision.value <= RHH_MOVE_TARGET_ALL_BATTLERS
        && (decision.value != RHH_MOVE_TARGET_SMART || strikes > 1)
        && (decision.value != RHH_MOVE_TARGET_DEPENDS || base == RHH_MOVE_TARGET_DEPENDS)
        ? decision.value : base;
}
u32 ModsMoveBaseTarget(u32 move, u32 base)
{
    if (move == MOVE_STRUGGLE || base >= ARRAY_COUNT(sPublicMoveTargets) || !RhhModsHasActiveRun()
        || sResolvingMoveTarget || !RhhModsHasHook(RHH_MOVE_BASE_TARGET)
        || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK)))) return base;
    RhhContentKey key = move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, move, sStabMoves, MOVES_COUNT_ALL)
                                               : RhhModsMoveKey(move);
    u32 target = AdjustMoveTarget(key, sPublicMoveTargets[base], GetMoveInfo(move)->strikeCount);
    return target < ARRAY_COUNT(sEngineMoveTargets) ? sEngineMoveTargets[target] : base;
}
static bool32 sResolvingMovePriority;
static s32 AdjustMovePriority(RhhContentKey move, s32 base)
{
    if (!move || base < -RHH_MOVE_PRIORITY_BIAS || base > RHH_MOVE_PRIORITY_BIAS
        || !RhhModsHasActiveRun() || sResolvingMovePriority || !RhhModsHasHook(RHH_MOVE_BASE_PRIORITY)
        || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))))
        return base;
    u32 encoded = base + RHH_MOVE_PRIORITY_BIAS;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_BASE_PRIORITY,
        .run_seed = RhhModsRunSeed(), .event_key = move, .args = {encoded, 0, 0, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = encoded};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sResolvingMovePriority = TRUE; RhhModsDispatch(&event, &decision); sResolvingMovePriority = FALSE;
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    return pure && decision.allowed && decision.value <= RHH_MOVE_PRIORITY_ENCODED_MAX
        ? (s32)decision.value - RHH_MOVE_PRIORITY_BIAS : base;
}
s32 ModsMoveBasePriority(u32 move, s32 base)
{
    if (!move || move == MOVE_STRUGGLE || !RhhModsHasActiveRun() || sResolvingMovePriority
        || !RhhModsHasHook(RHH_MOVE_BASE_PRIORITY)
        || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))))
        return base;
    RhhContentKey key = move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, move, sStabMoves, MOVES_COUNT_ALL)
                                               : RhhModsMoveKey(move);
    return AdjustMovePriority(key, base);
}
static const struct RhhMoveBattlerPriority *sBattlerPriorityContext;
static const struct DamageContext *sPriorityDamageContext;
static u32 sPriorityAbility;
static s32 ApplyInnatePriority(const struct RhhMoveBattlerPriority *move, const struct DamageContext *calculation, u32 ability, s32 priority);
static int BattlerPriorityContext(struct RhhMoveBattlerPriority *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBattlerPriorityContext
        || RhhModsCurrentEvent() != RHH_MOVE_BATTLER_PRIORITY) return -1;
    *out = *sBattlerPriorityContext; return 0;
}
s32 ModsMoveBattlerPriority(u32 battler, u32 ability, u32 chosenMove, u32 move, s32 base, s32 priority, bool32 maxGuard, bool32 ai)
{
    if (!ConditionsAvailable() || (!RhhModsHasHook(RHH_MOVE_BATTLER_PRIORITY) && !RhhModsHasHook(RHH_INNATE_PRIORITY)) || sBattlerPriorityContext
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || !move || !chosenMove
        || chosenMove == MOVE_STRUGGLE || move == MOVE_STRUGGLE
        || priority < -RHH_MOVE_PRIORITY_BIAS
        || priority > (s32)RHH_MOVE_BATTLER_PRIORITY_ENCODED_MAX - RHH_MOVE_PRIORITY_BIAS) return priority;
    RhhContentKey key = move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, move, sStabMoves, MOVES_COUNT_ALL)
                                               : RhhModsMoveKey(move);
    RhhContentKey chosen = chosenMove < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, chosenMove, sStabMoves, MOVES_COUNT_ALL)
                                                       : RhhModsMoveKey(chosenMove);
    if (!key || !chosen) return priority;
    const struct BattlePokemon *mon = &gBattleMons[battler];
    struct RhhMoveBattlerPriority context = {.struct_size = sizeof(context), .battler = battler,
        .flags = (maxGuard ? RHH_MOVE_PRIORITY_MAX_GUARD : 0) | (ai ? RHH_MOVE_PRIORITY_AI : 0), .weather = PowerWeather(gBattleWeather),
        .terrain = PowerTerrain(gFieldTimers.terrain), .hp = mon->hp, .max_hp = mon->maxHP, .level = mon->level,
        .base_priority = base, .engine_priority = priority, .run_id = sRunId, .battle_id = sBattleId,
        .move = key, .chosen_move = chosen, .species = RhhContentKeyForId(RHH_SPECIES, mon->species),
        .ability = CachedKey(RHH_ABILITY, ability, sStabAbilities, ABILITIES_COUNT)};
    u32 encoded = priority + RHH_MOVE_PRIORITY_BIAS;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_BATTLER_PRIORITY,
        .run_seed = RhhModsRunSeed(), .event_key = key, .subject = context.species,
        .args = {battler, encoded, context.flags, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = encoded};
    struct DamageContext calculation = {.battlerAtk = battler, .battlerDef = battler, .move = move, .aiCalc = ai};
    calculation.abilities[battler] = ability;
    if (ai && gAiLogicData) calculation.holdEffects[battler] = gAiLogicData->holdEffects[battler];
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBattlerPriorityContext = &context; sPriorityDamageContext = &calculation; sPriorityAbility = ability;
    if (RhhModsHasHook(RHH_MOVE_BATTLER_PRIORITY)) RhhModsDispatch(&event, &decision);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    s32 result = pure && decision.allowed && decision.value <= RHH_MOVE_BATTLER_PRIORITY_ENCODED_MAX
        ? (s32)decision.value - RHH_MOVE_PRIORITY_BIAS : priority;
    result = ApplyInnatePriority(&context, &calculation, ability, result);
    sPriorityDamageContext = NULL; sBattlerPriorityContext = NULL;
    return result;
}
static int MoveInfo(RhhContentKey key, struct RhhMoveInfo *out)
{
    u32 move;
    if (!RhhModsFinalized() || !out || out->struct_size < sizeof(*out)) return -1;
    const struct RhhModMove *custom = RhhModsFindCustomMove(key);
    if (custom)
    {
        if (!ModsCustomMoveAvailable(custom) || RhhContentResolve(RHH_MOVE, custom->template_move, &move)) return -1;
    }
    else if (RhhContentResolve(RHH_MOVE, key, &move) || !move || move >= MOVES_COUNT_ALL) return -1;
    static const u8 categories[] = {[DAMAGE_CATEGORY_NONE] = 0, [DAMAGE_CATEGORY_PHYSICAL] = RHH_MOVE_PHYSICAL,
        [DAMAGE_CATEGORY_SPECIAL] = RHH_MOVE_SPECIAL, [DAMAGE_CATEGORY_STATUS] = RHH_MOVE_STATUS};
    u32 category = GetMoveCategory(move), target = custom ? GetMoveInfo(move)->target : GetMoveTarget(move), type = custom ? TYPE_NONE : GetMoveType(move);
    if (category >= ARRAY_COUNT(categories) || target >= ARRAY_COUNT(sPublicMoveTargets)) return -1;
    RhhContentKey typeKey = custom ? custom->type : type < NUMBER_OF_MON_TYPES ? sTypeKeys[type] : RhhModsCustomTypeKey(type);
    RhhContentKey moveKey = move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, move, sStabMoves, MOVES_COUNT_ALL)
                                                  : RhhModsMoveKey(move);
    if (!typeKey || !moveKey) return -1;
    const struct MoveInfo *info = GetMoveInfo(move);
    struct RhhMoveInfo result = {.struct_size = sizeof(result), .move = moveKey, .type = typeKey,
        .category = categories[category], .target = sPublicMoveTargets[target], .power = info->power,
        .accuracy = info->accuracy, .pp = info->pp, .priority = custom ? info->priority : GetMovePriority(move),
        .flags = (info->makesContact ? RHH_MOVE_CONTACT : 0) | (info->soundMove ? RHH_MOVE_SOUND : 0)
            | (info->punchingMove ? RHH_MOVE_PUNCH : 0) | (info->bitingMove ? RHH_MOVE_BITE : 0)
            | (info->pulseMove ? RHH_MOVE_PULSE : 0) | (info->ballisticMove ? RHH_MOVE_BALLISTIC : 0)
            | (info->powderMove ? RHH_MOVE_POWDER : 0) | (info->danceMove ? RHH_MOVE_DANCE : 0)
            | (info->windMove ? RHH_MOVE_WIND : 0) | (info->slicingMove ? RHH_MOVE_SLICING : 0)
            | (info->healingMove ? RHH_MOVE_HEALING : 0)};
    if (custom)
    {
        u32 slot;
        result.move = key; result.type = custom->type; result.category = custom->category;
        result.target = AdjustMoveTarget(key, custom->target, info->strikeCount);
        result.power = custom->power; result.accuracy = custom->accuracy; result.pp = custom->pp;
        result.priority = AdjustMovePriority(key, custom->priority);
        if (!RhhModsMoveSlot(key, 0, &slot))
        {
            type = GetMoveType(slot);
            result.type = type < NUMBER_OF_MON_TYPES ? sTypeKeys[type] : RhhModsCustomTypeKey(type);
        }
        if (!result.type) return -1;
    }
    *out = result; return 0;
}
static int MoveEffectModeQuery(RhhContentKey key, uint32_t *out)
{
    struct RhhMoveInfo info = {.struct_size = sizeof(info)};
    if (!out || MoveInfo(key, &info)) return -1;
    u32 base;
    const struct RhhModMove *custom = RhhModsFindCustomMove(key);
    if (custom)
    { if (RhhContentResolve(RHH_MOVE, custom->template_move, &base)) return -1; }
    else if (RhhContentResolve(RHH_MOVE, info.move, &base)) return -1;
    const struct MoveInfo *template = GetMoveInfo(base);
    u32 mode = RHH_MOVE_EFFECT_INHERITED;
    if (base != MOVE_STRUGGLE && info.category == RHH_MOVE_STATUS && template->effect != EFFECT_PLACEHOLDER
        && !template->numAdditionalEffects) mode = AdjustMoveEffectMode(info.move, FALSE);
    else if (base != MOVE_STRUGGLE && info.category != RHH_MOVE_STATUS && template->effect == EFFECT_HIT)
        mode = AdjustMoveEffectMode(info.move, TRUE);
    *out = mode;
    return 0;
}
static int LearnsetSpecies(RhhContentKey key, u32 *species)
{
    struct RhhContentInfo info = {.struct_size = sizeof(info)};
    if (!RhhModsFinalized() || RhhContentResolve(RHH_SPECIES, key, species)
        || !*species || *species >= NUM_SPECIES || *species == SPECIES_EGG
        || RhhContentInfoFor(RHH_SPECIES, key, &info) || !(info.flags & RHH_CONTENT_ENABLED)) return -1;
    return 0;
}
static int LearnsetCount(RhhContentKey speciesKey, u32 kind, u32 *out)
{
    u32 species, count = 0;
    if (!out || LearnsetSpecies(speciesKey, &species)) return -1;
    if (kind == RHH_LEARNSET_LEVEL)
    {
        const struct LevelUpMove *list = GetSpeciesLevelUpLearnset(species);
        while (list[count].move != LEVEL_UP_MOVE_END) ++count;
    }
    else if (kind == RHH_LEARNSET_TEACHABLE || kind == RHH_LEARNSET_EGG)
    {
        const u16 *list = kind == RHH_LEARNSET_TEACHABLE ? GetSpeciesTeachableLearnset(species) : GetSpeciesEggMoves(species);
        while (list[count] != MOVE_UNAVAILABLE) ++count;
    }
    else return -1;
    *out = count; return 0;
}
static int LearnsetAt(RhhContentKey speciesKey, u32 kind, u32 index, struct RhhLearnsetEntry *out)
{
    u32 species, count;
    if (!out || out->struct_size < sizeof(*out) || LearnsetSpecies(speciesKey, &species)
        || LearnsetCount(speciesKey, kind, &count) || index >= count) return -1;
    u32 move, level = 0;
    if (kind == RHH_LEARNSET_LEVEL)
    {
        const struct LevelUpMove *entry = &GetSpeciesLevelUpLearnset(species)[index];
        move = entry->move; level = entry->level;
    }
    else move = (kind == RHH_LEARNSET_TEACHABLE ? GetSpeciesTeachableLearnset(species) : GetSpeciesEggMoves(species))[index];
    RhhContentKey moveKey = move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, move, sStabMoves, MOVES_COUNT_ALL)
                                                  : RhhModsMoveKey(move);
    RhhContentKey canonicalSpecies = CachedKey(RHH_SPECIES, species, sStabSpecies, NUM_SPECIES);
    if (!moveKey || !canonicalSpecies) return -1;
    struct RhhLearnsetEntry result = {.struct_size = sizeof(result), .kind = kind, .level = level,
        .species = canonicalSpecies, .move = moveKey};
    *out = result; return 0;
}
static const struct RhhMovePowerContext *sPowerContext;
static const struct DamageContext *sPowerDamageContext;
static const struct RhhInnatePowerContext *sInnatePowerContext;
static const struct RhhMoveAccuracyContext *sAccuracyContext;
static const struct DamageContext *sAccuracyDamageContext;
static const struct RhhInnateAccuracyContext *sInnateAccuracyContext;
static const struct RhhAiMoveEvaluation *sAiEvaluation;
static u32 sAiSwitchCandidate = MAX_BATTLERS_COUNT, sAiSwitchPartySlot = PARTY_SIZE;
void ModsAiSwitchCandidateBegin(u32 battler, u32 partySlot)
{
    sAiSwitchCandidate = battler;
    sAiSwitchPartySlot = partySlot;
}
void ModsAiSwitchCandidateEnd(void)
{
    sAiSwitchCandidate = MAX_BATTLERS_COUNT;
    sAiSwitchPartySlot = PARTY_SIZE;
}
static int ResolveInnatesFor(u32 battler, const struct DamageContext *damage, bool32 prospective, u32 primaryOverride,
                             struct RhhInnateResolution *out)
{
    if (damage && damage->aiCalc && gAiLogicData && gAiLogicData->switchInCalc
        && battler < MAX_BATTLERS_COUNT && battler == sAiSwitchCandidate
        && sAiSwitchPartySlot < PARTY_SIZE && gBattlerPartyIndexes[battler] == sAiSwitchPartySlot)
        prospective = TRUE;
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable() || battler >= gBattlersCount
        || battler >= MAX_BATTLERS_COUNT || (!prospective && ((sPending & (1u << battler)) || !IsBattlerAlive(battler)))
        || (prospective && !gBattleMons[battler].hp)
        || (damage && damage->aiCalc && !gAiLogicData)) return -1;
    const struct BattlePokemon *mon = &gBattleMons[battler];
    RhhContentKey species = RhhContentKeyForId(RHH_SPECIES, mon->species);
    struct RhhInnateList list = {.struct_size = sizeof(list)};
    if (!species || RhhModsSpeciesInnates(species, mon->level, &list)) return -1;
    if (GetBattlerTrainer(battler) == B_TRAINER_PLAYER && !mon->volatiles.transformed
        && !gIsDebugBattle && !IsAiVsAiBattle()
        && !(gBattleTypeFlags & (BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_EREADER_TRAINER)))
        RhhModsPersonalizePartyInnates(gBattlerPartyIndexes[battler], species, mon->level,
                                       (u64)mon->otId << 32 | mon->personality, &list);
    if (RhhModsHasHook(RHH_INNATE_BATTLE_PERSONALIZE) && gBattlerPartyIndexes[battler] < PARTY_SIZE)
    {
        enum BattleTrainer owner = GetBattlerTrainer(battler);
        if (owner < MAX_BATTLE_TRAINERS)
        {
            struct RhhInnateBattleContext context = {.struct_size = sizeof(context), .battler = battler,
                .owner = owner, .party_slot = gBattlerPartyIndexes[battler], .level = mon->level,
                .battle_id = sBattleId, .species = species, .trainer = AuthoredTrainerKey(owner), .list = list};
            RhhModsPersonalizeBattleInnates(&context, &list);
        }
    }
    bool32 ai = damage && damage->aiCalc;
    u32 primary = primaryOverride != UINT32_MAX ? primaryOverride : ai ? gAiLogicData->abilities[battler] : mon->ability;
    RhhContentKey primaryKey = primary && primary < ABILITIES_COUNT ? RhhContentKeyForId(RHH_ABILITY, primary) : 0;
    bool32 neutralizingGas = IsNeutralizingGasOnField()
        || (prospective && primary == ABILITY_NEUTRALIZING_GAS && !mon->volatiles.gastroAcid);
    bool32 shield = ai ? (damage->holdEffects[battler] == HOLD_EFFECT_ABILITY_SHIELD
        && !mon->volatiles.embargoTimer && !(gFieldStatuses & STATUS_FIELD_MAGIC_ROOM))
        : GetBattlerHoldEffectIgnoreAbilityReadOnly(battler) == HOLD_EFFECT_ABILITY_SHIELD;
    bool32 bypass = FALSE;
    if (damage && battler == damage->battlerDef && battler != damage->battlerAtk && !shield)
    {
        u32 attackerAbility = damage->abilities[damage->battlerAtk];
        bool32 moldBreaker = !gBattleMons[damage->battlerAtk].volatiles.gastroAcid
            && (attackerAbility == ABILITY_MOLD_BREAKER || attackerAbility == ABILITY_TERAVOLT
                || attackerAbility == ABILITY_TURBOBLAZE
                || (attackerAbility == ABILITY_MYCELIUM_MIGHT && IsBattleMoveStatus(damage->move)));
        bypass = moldBreaker || MoveIgnoresTargetAbility(damage->move);
    }
    struct RhhInnateResolution result = {.struct_size = sizeof(result), .battler = battler,
        .count = list.count, .flags = ai ? RHH_INNATE_AI_KNOWLEDGE : 0, .battle_id = sBattleId, .species = list.species};
    for (u32 i = 0; i < list.count; ++i)
    {
        RhhContentKey ability = list.entries[i].ability;
        struct RhhAbilityInfo info = {.struct_size = sizeof(info)};
        if (RhhModsAbilityInfo(ability, &info) || info.ability != ability) return -1;
        u32 status = (!list.entries[i].unlocked ? RHH_INNATE_LOCKED : 0)
            | (primaryKey == ability ? RHH_INNATE_DUPLICATE_PRIMARY : 0)
            | ((!prospective && (gBattleStruct->battlerState[battler].notOnField || gSpecialStatuses[battler].attackerInParty))
                ? RHH_INNATE_NOT_ON_FIELD : 0);
        if (mon->volatiles.gastroAcid && (!(info.flags & RHH_ABILITY_CANNOT_SUPPRESS)
            || (mon->volatiles.transformed && ability == RhhContentKeyForId(RHH_ABILITY, ABILITY_COMATOSE)))) status |= RHH_INNATE_GASTRO_ACID;
        if (!(info.flags & RHH_ABILITY_CANNOT_SUPPRESS) && !shield && neutralizingGas
            && ability != RhhContentKeyForId(RHH_ABILITY, ABILITY_NEUTRALIZING_GAS))
            status |= RHH_INNATE_NEUTRALIZING_GAS;
        if (bypass && (info.flags & RHH_ABILITY_BREAKABLE)) status |= RHH_INNATE_MOVE_BYPASSED;
        result.entries[i] = (struct RhhInnateResolvedEntry){.ability = list.entries[i].ability,
            .unlock_level = list.entries[i].unlock_level, .status = status};
    }
    *out = result; return 0;
}
static int ResolveInnates(u32 battler, const struct DamageContext *damage, struct RhhInnateResolution *out)
{ return ResolveInnatesFor(battler, damage, FALSE, UINT32_MAX, out); }
static const struct RhhInnatePriorityContext *sInnatePriorityContext;
int RhhModsPriorityInnates(struct RhhInnateResolution *out)
{
    if (!out || out->struct_size < sizeof(*out) || RhhModsCurrentEvent() != RHH_MOVE_BATTLER_PRIORITY
        || !sBattlerPriorityContext || !sPriorityDamageContext)
        return -1;
    struct RhhInnateResolution result = {.struct_size = sizeof(result)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    int status = ResolveInnatesFor(sBattlerPriorityContext->battler, sPriorityDamageContext, FALSE, sPriorityAbility, &result);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (status || !pure) return -1;
    *out = result; return 0;
}
int RhhModsInnatePriorityContext(struct RhhInnatePriorityContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnatePriorityContext
        || RhhModsCurrentEvent() != RHH_INNATE_PRIORITY) return -1;
    *out = *sInnatePriorityContext; return 0;
}
static s32 ApplyInnatePriority(const struct RhhMoveBattlerPriority *move, const struct DamageContext *calculation, u32 ability, s32 priority)
{
    if (!RhhModsHasHook(RHH_INNATE_PRIORITY)) return priority;
    struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
    rng_value_t originalRng = gRngValue, originalRng2 = gRng2Value;
    int status = ResolveInnatesFor(move->battler, calculation, FALSE, ability, &resolution);
    bool32 pureResolution = !memcmp(&originalRng, &gRngValue, sizeof(originalRng))
        && !memcmp(&originalRng2, &gRng2Value, sizeof(originalRng2));
    gRngValue = originalRng; gRng2Value = originalRng2;
    if (status || !pureResolution) return priority;
    for (u32 i = 0; i < resolution.count; ++i) {
        if (resolution.entries[i].status) continue;
        struct RhhInnatePriorityContext context = {.struct_size = sizeof(context), .index = i,
            .current_priority = priority, .ability = resolution.entries[i].ability, .move = *move, .resolution = resolution};
        u32 encoded = priority + RHH_MOVE_PRIORITY_BIAS;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_INNATE_PRIORITY,
            .run_seed = RhhModsRunSeed(), .event_key = context.ability, .subject = move->move, .ordinal = i,
            .args = {encoded, move->battler, move->flags, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = encoded};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        sInnatePriorityContext = &context; RhhModsDispatch(&event, &decision); sInnatePriorityContext = NULL;
        bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
        gRngValue = rng; gRng2Value = rng2;
        if (pure && decision.allowed && decision.value <= RHH_MOVE_BATTLER_PRIORITY_ENCODED_MAX)
            priority = (s32)decision.value - RHH_MOVE_PRIORITY_BIAS;
    }
    return priority;
}
static const struct RhhBattleSpeedContext *sSpeedContext;
static const struct DamageContext *sSpeedDamageContext;
static const struct RhhInnateSpeedContext *sInnateSpeedContext;
static u32 sSpeedAbility;
static int SpeedContext(struct RhhBattleSpeedContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sSpeedContext
        || RhhModsCurrentEvent() != RHH_BATTLER_SPEED) return -1;
    *out = *sSpeedContext; return 0;
}
int RhhModsSpeedInnates(struct RhhInnateResolution *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sSpeedContext || !sSpeedDamageContext
        || RhhModsCurrentEvent() != RHH_BATTLER_SPEED) return -1;
    struct RhhInnateResolution result = {.struct_size = sizeof(result)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    int status = ResolveInnatesFor(sSpeedContext->battler, sSpeedDamageContext, FALSE, sSpeedAbility, &result);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (status || !pure) return -1;
    *out = result; return 0;
}
int RhhModsInnateSpeedContext(struct RhhInnateSpeedContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnateSpeedContext
        || RhhModsCurrentEvent() != RHH_INNATE_SPEED) return -1;
    *out = *sInnateSpeedContext; return 0;
}
static u32 ApplyInnateSpeed(const struct RhhBattleSpeedContext *speed, const struct DamageContext *calculation, u32 ability, u32 value)
{
    if (!RhhModsHasHook(RHH_INNATE_SPEED)) return value;
    struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    int status = ResolveInnatesFor(speed->battler, calculation, FALSE, ability, &resolution);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (status || !pure) return value;
    for (u32 i = 0; i < resolution.count; ++i) {
        if (resolution.entries[i].status) continue;
        struct RhhInnateSpeedContext context = {.struct_size = sizeof(context), .index = i,
            .current_speed = value, .ability = resolution.entries[i].ability, .speed = *speed, .resolution = resolution};
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_INNATE_SPEED,
            .run_seed = RhhModsRunSeed(), .event_key = context.ability, .subject = speed->species, .ordinal = i,
            .args = {value, speed->battler, speed->flags, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = value};
        rng = gRngValue; rng2 = gRng2Value;
        sInnateSpeedContext = &context; RhhModsDispatch(&event, &decision); sInnateSpeedContext = NULL;
        pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
        gRngValue = rng; gRng2Value = rng2;
        if (pure && decision.allowed && decision.value >= 1 && decision.value <= RHH_BATTLE_SPEED_MAX)
            value = decision.value;
    }
    return value;
}
u32 ModsBattlerSpeed(u32 battler, u32 ability, u32 holdEffect, u32 weather, u32 terrain, u32 speed, bool32 ai)
{
    if (!ConditionsAvailable() || sSpeedContext || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || (!RhhModsHasHook(RHH_BATTLER_SPEED) && !RhhModsHasHook(RHH_INNATE_SPEED))) return speed;
    const struct BattlePokemon *mon = &gBattleMons[battler];
    struct RhhBattleSpeedContext context = {.struct_size = sizeof(context), .battler = battler,
        .flags = ai ? RHH_BATTLE_SPEED_AI : 0, .engine_speed = speed, .weather = PowerWeather(weather),
        .terrain = PowerTerrain(terrain), .hp = mon->hp, .max_hp = mon->maxHP,
        .level = mon->level, .hold_effect = holdEffect, .run_id = sRunId, .battle_id = sBattleId,
        .species = RhhContentKeyForId(RHH_SPECIES, mon->species),
        .ability = CachedKey(RHH_ABILITY, ability, sStabAbilities, ABILITIES_COUNT)};
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLER_SPEED,
        .run_seed = RhhModsRunSeed(), .event_key = context.species, .subject = context.ability,
        .args = {battler, speed, context.flags, holdEffect}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = speed};
    struct DamageContext calculation = {.battlerAtk = battler, .battlerDef = battler, .aiCalc = ai};
    calculation.abilities[battler] = ability; calculation.holdEffects[battler] = holdEffect;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sSpeedContext = &context; sSpeedDamageContext = &calculation; sSpeedAbility = ability;
    if (RhhModsHasHook(RHH_BATTLER_SPEED)) RhhModsDispatch(&event, &decision);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    u32 result = pure && decision.allowed && decision.value >= 1 && decision.value <= RHH_BATTLE_SPEED_MAX
        ? decision.value : speed;
    result = ApplyInnateSpeed(&context, &calculation, ability, result);
    sSpeedDamageContext = NULL; sSpeedContext = NULL;
    return result;
}
struct InnateObservationSample {
    bool32 valid;
    struct RhhBattleParticipant participant;
    struct RhhInnateResolution resolution;
};
static struct InnateObservationSample sInnateSamples[MAX_BATTLERS_COUNT];
static const struct RhhBattlerInnateObservation *sInnateObservation;
static u64 sInnateObservationSequence;
int RhhModsInnateObservationContext(struct RhhBattlerInnateObservation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnateObservation
        || RhhModsCurrentEvent() != RHH_BATTLER_INNATES_OBSERVED) return -1;
    *out = *sInnateObservation; return 0;
}
static bool32 InnateResolutionsEqual(const struct RhhInnateResolution *a, const struct RhhInnateResolution *b)
{
    if (a->species != b->species || a->count != b->count || a->flags != b->flags) return FALSE;
    for (u32 i = 0; i < a->count; ++i)
        if (a->entries[i].ability != b->entries[i].ability || a->entries[i].unlock_level != b->entries[i].unlock_level
            || a->entries[i].status != b->entries[i].status) return FALSE;
    return TRUE;
}
void RhhModsObserveBattleInnates(void)
{
    if (RhhModsCurrentEvent() || sInnateObservation) return;
    if (!ConditionsAvailable()) { memset(sInnateSamples, 0, sizeof(sInnateSamples)); return; }
    if (!RhhModsHasHook(RHH_BATTLER_INNATES_OBSERVED)) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    struct RhhBattlerInnateObservation changes[MAX_BATTLERS_COUNT];
    u32 count = 0;
    for (u32 battler = 0; battler < MAX_BATTLERS_COUNT; ++battler)
    {
        const struct RhhBattleParticipant *participant = NULL;
        for (u32 i = 0; i < battle.participant_count; ++i)
            if (battle.participants[i].battler == battler) { participant = &battle.participants[i]; break; }
        struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
        if (!participant || ResolveInnates(battler, NULL, &resolution))
        { sInnateSamples[battler].valid = FALSE; continue; }
        struct InnateObservationSample *previous = &sInnateSamples[battler];
        if (previous->valid && previous->participant.identity == participant->identity
            && previous->participant.owner == participant->owner && previous->participant.party_slot == participant->party_slot
            && !InnateResolutionsEqual(&previous->resolution, &resolution))
            changes[count++] = (struct RhhBattlerInnateObservation){.struct_size = sizeof(changes[0]),
                .participant = *participant, .before = previous->resolution, .after = resolution};
        *previous = (struct InnateObservationSample){.valid = TRUE, .participant = *participant, .resolution = resolution};
    }
    for (u32 i = 0; i < count && ConditionsAvailable() && battle.battle_id == sBattleId; ++i)
    {
        changes[i].sequence = sInnateObservationSequence++;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLER_INNATES_OBSERVED,
            .run_seed = RhhModsRunSeed(), .event_key = changes[i].after.species, .subject = changes[i].participant.identity,
            .ordinal = changes[i].sequence, .args = {changes[i].participant.battler, changes[i].participant.owner,
                changes[i].participant.party_slot, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        sInnateObservation = &changes[i]; RhhModsDispatch(&event, &decision); sInnateObservation = NULL;
    }
}
static int AiInnateHealContext(u32 event, struct RhhAiInnateEntryContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAiInnateEntryContext
        || RhhModsCurrentEvent() != event) return -1;
    *out = *sAiInnateEntryContext; return 0;
}
int RhhModsAiInnateEntryContext(struct RhhAiInnateEntryContext *out)
{ return AiInnateHealContext(RHH_AI_INNATE_ENTRY_PREVIEW, out); }
int RhhModsAiInnateTurnHealContext(struct RhhAiInnateEntryContext *out)
{ return AiInnateHealContext(RHH_AI_INNATE_TURN_HEAL_PREVIEW, out); }
int RhhModsAiInnateTurnStepContext(struct RhhAiInnateEntryContext *out)
{ return AiInnateHealContext(RHH_AI_INNATE_TURN_STEP_PREVIEW, out); }
static int OfferAiInnateHeal(u32 event, u32 numerator, u32 denominator)
{
    if (!sAiInnateEntryContext || RhhModsCurrentEvent() != event
        || RhhModsCurrentStage() != RHH_CONSTRAINT || !numerator || numerator > denominator || denominator > 16
        || sAiInnateEntryContext->offered >= RHH_AI_INNATE_ENTRY_HEAL_LIMIT) return -1;
    u32 amount = max(1, sAiInnateEntryContext->max_hp * numerator / denominator);
    sAiInnateEntryHealing = min(sAiInnateEntryContext->max_hp, sAiInnateEntryHealing + amount);
    ++sAiInnateEntryContext->offered;
    sAiInnateEntryContext->capacity = RHH_AI_INNATE_ENTRY_HEAL_LIMIT - sAiInnateEntryContext->offered;
    return 0;
}
int RhhModsOfferAiInnateEntryHeal(u32 numerator, u32 denominator)
{ return OfferAiInnateHeal(RHH_AI_INNATE_ENTRY_PREVIEW, numerator, denominator); }
int RhhModsOfferAiInnateTurnHeal(u32 numerator, u32 denominator)
{ return OfferAiInnateHeal(RHH_AI_INNATE_TURN_HEAL_PREVIEW, numerator, denominator); }
int RhhModsOfferAiInnateTurnStepHeal(u32 numerator, u32 denominator)
{ return OfferAiInnateHeal(RHH_AI_INNATE_TURN_STEP_PREVIEW, numerator, denominator); }
static bool32 AiInnateEntryPreviewEnabled(u32 event)
{
    return ConditionsAvailable() && gAiLogicData && !gAiLogicData->aiPredictionInProgress
        && RhhModsHasHook(event) && !RhhModsCurrentEvent() && !sAiInnateEntryContext && !sAiInnateStatContext;
}
static int AiInnateCandidate(u32 battler, u32 partySlot, u32 hpAfterHazards,
                             struct RhhInnateResolution *resolution)
{
    if (battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || partySlot >= PARTY_SIZE
        || !hpAfterHazards || hpAfterHazards > gBattleMons[battler].maxHP) return -1;
    struct Pokemon candidate = GetBattlerParty(battler)[partySlot];
    if (!GetMonData(&candidate, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(&candidate, MON_DATA_SANITY_IS_BAD_EGG)
        || GetMonData(&candidate, MON_DATA_IS_EGG)
        || GetMonData(&candidate, MON_DATA_SPECIES) != gBattleMons[battler].species
        || GetMonData(&candidate, MON_DATA_LEVEL) != gBattleMons[battler].level
        || GetMonData(&candidate, MON_DATA_PERSONALITY) != gBattleMons[battler].personality
        || GetMonData(&candidate, MON_DATA_OT_ID) != gBattleMons[battler].otId) return -1;
    u32 savedParty = gBattlerPartyIndexes[battler]; gBattlerPartyIndexes[battler] = partySlot;
    struct DamageContext damage = {.battlerAtk = battler, .battlerDef = battler, .aiCalc = TRUE};
    for (u32 i = 0; i < gBattlersCount; ++i)
    { damage.abilities[i] = gAiLogicData->abilities[i]; damage.holdEffects[i] = gAiLogicData->holdEffects[i]; }
    int failed = ResolveInnatesFor(battler, &damage, TRUE, UINT32_MAX, resolution);
    gBattlerPartyIndexes[battler] = savedParty;
    return failed;
}
static u32 AiInnateHealing(u32 eventKind, u32 battler, u32 partySlot, u32 hpAfterHazards)
{
    if (!AiInnateEntryPreviewEnabled(eventKind)
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || (eventKind == RHH_AI_INNATE_ENTRY_PREVIEW && hpAfterHazards >= gBattleMons[battler].maxHP)
        || gBattleMons[battler].volatiles.healBlockTimer) return 0;
    struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
    if (AiInnateCandidate(battler, partySlot, hpAfterHazards, &resolution)) return 0;
    struct RhhAiInnateEntryContext context = {.struct_size = sizeof(context), .battler = battler,
        .party_slot = partySlot, .hp = hpAfterHazards, .max_hp = gBattleMons[battler].maxHP,
        .capacity = RHH_AI_INNATE_ENTRY_HEAL_LIMIT, .battle_id = sBattleId, .resolution = resolution};
    sAiInnateEntryHealing = 0;
    for (u32 i = 0; i < resolution.count; ++i)
    {
        if (resolution.entries[i].status) continue;
        context.index = i; context.ability = resolution.entries[i].ability;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = eventKind,
            .run_seed = RhhModsRunSeed(), .event_key = context.ability, .subject = resolution.species,
            .ordinal = i, .args = {battler, partySlot, hpAfterHazards, context.max_hp}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        u32 beforeCount = context.offered, beforeHealing = sAiInnateEntryHealing;
        sAiInnateEntryContext = &context; RhhModsDispatch(&event, &decision); sAiInnateEntryContext = NULL;
        if (!decision.allowed)
        { context.offered = beforeCount; context.capacity = RHH_AI_INNATE_ENTRY_HEAL_LIMIT - beforeCount; sAiInnateEntryHealing = beforeHealing; }
    }
    u32 healing = sAiInnateEntryHealing;
    if (eventKind == RHH_AI_INNATE_ENTRY_PREVIEW) healing = min(healing, gBattleMons[battler].maxHP - hpAfterHazards);
    sAiInnateEntryHealing = 0;
    return healing;
}
u32 ModsAiInnateEntryHealing(u32 battler, u32 partySlot, u32 hpAfterHazards)
{ return AiInnateHealing(RHH_AI_INNATE_ENTRY_PREVIEW, battler, partySlot, hpAfterHazards); }
u32 ModsAiInnateTurnHealing(u32 battler, u32 partySlot, u32 hpAfterEntry)
{ return AiInnateHealing(RHH_AI_INNATE_TURN_HEAL_PREVIEW, battler, partySlot, hpAfterEntry); }
bool32 ModsAiInnateTurnStepEnabled(void)
{ return AiInnateEntryPreviewEnabled(RHH_AI_INNATE_TURN_STEP_PREVIEW); }
u32 ModsAiInnateTurnStepHealing(u32 battler, u32 partySlot, u32 hpBeforeHeal)
{ return AiInnateHealing(RHH_AI_INNATE_TURN_STEP_PREVIEW, battler, partySlot, hpBeforeHeal); }
static struct { u8 kind, value; } sAiInnateFieldOffers[RHH_AI_INNATE_ENTRY_FIELD_LIMIT];
static struct { u32 weather, terrain, weatherTurns, terrainTurns; } sAiInnateFieldBase;
static struct { bool32 active; u32 battler, weather, terrain, weatherTurns; } sAiSwitchField;
void ModsAiSwitchFieldBegin(u32 battler, u32 weather, u32 terrain, u32 weatherTurns)
{ sAiSwitchField = (typeof(sAiSwitchField)){TRUE, battler, weather, terrain, weatherTurns}; }
void ModsAiSwitchFieldEnd(void) { memset(&sAiSwitchField, 0, sizeof(sAiSwitchField)); }
bool32 ModsAiSwitchFieldIsCandidate(u32 battler)
{ return sAiSwitchField.active && ConditionsAvailable() && sAiSwitchField.battler == battler; }
bool32 ModsAiSwitchFieldView(u32 *weather, u32 *terrain, u32 *weatherTurns)
{
    if (!sAiSwitchField.active || !ConditionsAvailable()) return FALSE;
    if (weather) *weather = sAiSwitchField.weather;
    if (terrain) *terrain = sAiSwitchField.terrain;
    if (weatherTurns) *weatherTurns = sAiSwitchField.weatherTurns;
    return TRUE;
}
int RhhModsAiInnateEntryFieldContext(struct RhhAiInnateFieldContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAiInnateFieldContext
        || RhhModsCurrentEvent() != RHH_AI_INNATE_ENTRY_FIELD_PREVIEW) return -1;
    *out = *sAiInnateFieldContext; return 0;
}
static int OfferAiInnateField(u32 kind, u32 value)
{
    if (!sAiInnateFieldContext || RhhModsCurrentEvent() != RHH_AI_INNATE_ENTRY_FIELD_PREVIEW
        || RhhModsCurrentStage() != RHH_CONSTRAINT || sAiInnateFieldContext->entry.offered >= RHH_AI_INNATE_ENTRY_FIELD_LIMIT)
        return -1;
    if ((kind == MOD_INNATE_ENV_WEATHER && (value < RHH_WEATHER_RAIN || value > RHH_WEATHER_FOG))
        || (kind == MOD_INNATE_ENV_TERRAIN && (value < RHH_TERRAIN_GRASSY || value > RHH_TERRAIN_PSYCHIC))) return -1;
    u32 i = sAiInnateFieldContext->entry.offered++;
    sAiInnateFieldOffers[i].kind = kind; sAiInnateFieldOffers[i].value = value;
    sAiInnateFieldContext->entry.capacity = RHH_AI_INNATE_ENTRY_FIELD_LIMIT - sAiInnateFieldContext->entry.offered;
    return 0;
}
int RhhModsOfferAiInnateEntryWeather(u32 weather) { return OfferAiInnateField(MOD_INNATE_ENV_WEATHER, weather); }
int RhhModsOfferAiInnateEntryTerrain(u32 terrain) { return OfferAiInnateField(MOD_INNATE_ENV_TERRAIN, terrain); }
bool32 ModsAiInnateEntryFieldPreviewEnabled(void)
{ return AiInnateEntryPreviewEnabled(RHH_AI_INNATE_ENTRY_FIELD_PREVIEW) && gAiThinkingStruct && !sAiInnateFieldContext; }
static u32 AiInnateFieldWeatherId(u32 weather)
{
    static const u8 values[] = {BATTLE_WEATHER_NONE, BATTLE_WEATHER_RAIN, BATTLE_WEATHER_SUN,
        BATTLE_WEATHER_SANDSTORM, BATTLE_WEATHER_HAIL, BATTLE_WEATHER_SNOW, BATTLE_WEATHER_FOG};
    return weather >= RHH_WEATHER_RAIN && weather <= RHH_WEATHER_FOG ? values[weather] : BATTLE_WEATHER_NONE;
}
static u32 AiInnateFieldTerrainId(u32 terrain)
{
    static const u8 values[] = {B_TERRAIN_NONE, B_TERRAIN_GRASSY, B_TERRAIN_MISTY, B_TERRAIN_ELECTRIC, B_TERRAIN_PSYCHIC};
    return terrain >= RHH_TERRAIN_GRASSY && terrain <= RHH_TERRAIN_PSYCHIC ? values[terrain] : B_TERRAIN_NONE;
}
static void AiInnateFieldApplyWeather(u32 battler, u32 weatherId, bool32 primalAbility, u32 *weather, u32 *turns)
{
    if (!weatherId || gBattleStruct->overworldWeatherPresent || (*weather & gBattleWeatherInfo[weatherId].flag)
        || ((*weather & B_WEATHER_PRIMAL_ANY) && !primalAbility)) return;
    *weather = gBattleWeatherInfo[weatherId].flag;
    if (GetConfig(B_ABILITY_WEATHER) < GEN_6) return; /* Match the engine's existing counter behavior. */
    u32 rock = gBattleWeatherInfo[weatherId].rock;
    *turns = (*weather & B_WEATHER_PRIMAL_ANY) ? 0 : rock && gAiLogicData->holdEffects[battler] == rock ? 8 : 5;
}
static void AiInnateFieldApplyOffers(u32 battler, u32 count, u32 *weather, u32 *terrain, u32 *weatherTurns, u32 *terrainTurns)
{
    for (u32 i = 0; i < count; ++i) {
        if (sAiInnateFieldOffers[i].kind == MOD_INNATE_ENV_WEATHER)
            AiInnateFieldApplyWeather(battler, AiInnateFieldWeatherId(sAiInnateFieldOffers[i].value), FALSE, weather, weatherTurns);
        else if (!gBattleStruct->isSkyBattle) {
            u32 next = AiInnateFieldTerrainId(sAiInnateFieldOffers[i].value);
            if (next && next != *terrain) {
                *terrain = next;
                *terrainTurns = gAiLogicData->holdEffects[battler] == HOLD_EFFECT_TERRAIN_EXTENDER ? 8 : 5;
            }
        }
    }
}
int RhhModsAiInnateEntryFieldProjected(struct RhhAiInnateFieldContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAiInnateFieldContext
        || RhhModsCurrentEvent() != RHH_AI_INNATE_ENTRY_FIELD_PREVIEW) return -1;
    struct RhhAiInnateFieldContext result = *sAiInnateFieldContext;
    u32 weather = sAiInnateFieldBase.weather, terrain = sAiInnateFieldBase.terrain;
    u32 weatherTurns = sAiInnateFieldBase.weatherTurns, terrainTurns = sAiInnateFieldBase.terrainTurns;
    AiInnateFieldApplyOffers(result.entry.battler, result.entry.offered, &weather, &terrain, &weatherTurns, &terrainTurns);
    result.weather = PowerWeather(weather); result.terrain = PowerTerrain(terrain);
    result.weather_turns = weatherTurns; result.terrain_turns = terrainTurns;
    result.flags = (gBattleStruct->overworldWeatherPresent || (weather & B_WEATHER_PRIMAL_ANY) ? RHH_AI_INNATE_FIELD_WEATHER_BLOCKED : 0)
        | (gBattleStruct->isSkyBattle ? RHH_AI_INNATE_FIELD_TERRAIN_BLOCKED : 0)
        | (weather && !AI_ApplyWeatherSuppression(weather) ? RHH_AI_INNATE_FIELD_WEATHER_SUPPRESSED : 0);
    *out = result;
    return 0;
}
bool32 ModsAiInnateEntryFieldForecast(u32 battler, u32 partySlot, u32 hpAfterHazards,
                                      u32 *weatherOut, u32 *terrainOut, u32 *weatherTurnsOut, u32 *terrainTurnsOut)
{
    if (!weatherOut || !terrainOut || !weatherTurnsOut || !terrainTurnsOut || !ModsAiInnateEntryFieldPreviewEnabled()
        || gBattlersCount != 2 || (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)) return FALSE;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
    if (AiInnateCandidate(battler, partySlot, hpAfterHazards, &resolution)) return FALSE;
    u32 opponent = GetOppositeBattler(battler);
    if (opponent >= gBattlersCount || !IsBattlerAlive(opponent)) return FALSE;
    u32 weather = gBattleWeather, terrain = gFieldTimers.terrain;
    u32 weatherTurns = gBattleStruct->weatherDuration, terrainTurns = gFieldTimers.terrainTimer;
    u32 primary = gAiLogicData->abilities[battler];
    bool32 shield = gAiLogicData->holdEffects[battler] == HOLD_EFFECT_ABILITY_SHIELD
        && !gBattleMons[battler].volatiles.embargoTimer && !(gFieldStatuses & STATUS_FIELD_MAGIC_ROOM);
    if (gBattleMons[battler].volatiles.gastroAcid || (IsNeutralizingGasOnField() && !shield)) primary = ABILITY_NONE;
    u32 weatherId = BATTLE_WEATHER_NONE; bool32 primal = FALSE;
    switch (primary) {
    case ABILITY_DRIZZLE: weatherId = BATTLE_WEATHER_RAIN; break;
    case ABILITY_DROUGHT: case ABILITY_ORICHALCUM_PULSE: weatherId = BATTLE_WEATHER_SUN; break;
    case ABILITY_SAND_STREAM: weatherId = BATTLE_WEATHER_SANDSTORM; break;
    case ABILITY_SNOW_WARNING: weatherId = GetConfig(B_SNOW_WARNING) >= GEN_9 ? BATTLE_WEATHER_SNOW : BATTLE_WEATHER_HAIL; break;
    case ABILITY_DESOLATE_LAND: weatherId = BATTLE_WEATHER_SUN_PRIMAL; primal = TRUE; break;
    case ABILITY_PRIMORDIAL_SEA: weatherId = BATTLE_WEATHER_RAIN_PRIMAL; primal = TRUE; break;
    case ABILITY_DELTA_STREAM: weatherId = BATTLE_WEATHER_STRONG_WINDS; primal = TRUE; break;
    }
    AiInnateFieldApplyWeather(battler, weatherId, primal, &weather, &weatherTurns);
    if (!gBattleStruct->isSkyBattle) {
        u32 next = B_TERRAIN_NONE;
        switch (primary) {
        case ABILITY_ELECTRIC_SURGE: case ABILITY_HADRON_ENGINE: next = B_TERRAIN_ELECTRIC; break;
        case ABILITY_GRASSY_SURGE: next = B_TERRAIN_GRASSY; break;
        case ABILITY_MISTY_SURGE: next = B_TERRAIN_MISTY; break;
        case ABILITY_PSYCHIC_SURGE: next = B_TERRAIN_PSYCHIC; break;
        }
        if (next && next != terrain) { terrain = next; terrainTurns = gAiLogicData->holdEffects[battler] == HOLD_EFFECT_TERRAIN_EXTENDER ? 8 : 5; }
    }
    u32 initialWeather = weather, initialTerrain = terrain;
    u32 initialWeatherTurns = weatherTurns, initialTerrainTurns = terrainTurns;
    sAiInnateFieldBase = (typeof(sAiInnateFieldBase)){weather, terrain, weatherTurns, terrainTurns};
    struct RhhAiInnateFieldContext context = {.struct_size = sizeof(context), .weather = PowerWeather(weather),
        .terrain = PowerTerrain(terrain), .weather_turns = weatherTurns, .terrain_turns = terrainTurns,
        .flags = (gBattleStruct->overworldWeatherPresent || (weather & B_WEATHER_PRIMAL_ANY) ? RHH_AI_INNATE_FIELD_WEATHER_BLOCKED : 0)
            | (gBattleStruct->isSkyBattle ? RHH_AI_INNATE_FIELD_TERRAIN_BLOCKED : 0)
            | (weather && !AI_ApplyWeatherSuppression(weather) ? RHH_AI_INNATE_FIELD_WEATHER_SUPPRESSED : 0),
        .entry = {.struct_size = sizeof(context.entry), .battler = battler, .party_slot = partySlot,
            .hp = hpAfterHazards, .max_hp = gBattleMons[battler].maxHP, .capacity = RHH_AI_INNATE_ENTRY_FIELD_LIMIT,
            .battle_id = sBattleId, .resolution = resolution}};
    for (u32 i = 0; i < resolution.count; ++i) {
        if (resolution.entries[i].status) continue;
        context.entry.index = i; context.entry.ability = resolution.entries[i].ability;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_AI_INNATE_ENTRY_FIELD_PREVIEW,
            .run_seed = RhhModsRunSeed(), .event_key = context.entry.ability, .subject = resolution.species,
            .ordinal = i, .args = {battler, partySlot, hpAfterHazards, context.entry.max_hp}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        u32 before = context.entry.offered;
        sAiInnateFieldContext = &context; RhhModsDispatch(&event, &decision); sAiInnateFieldContext = NULL;
        if (!decision.allowed) { context.entry.offered = before; context.entry.capacity = RHH_AI_INNATE_ENTRY_FIELD_LIMIT - before; }
    }
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (!pure) return FALSE;
    AiInnateFieldApplyOffers(battler, context.entry.offered, &weather, &terrain, &weatherTurns, &terrainTurns);
    if (weather == initialWeather && terrain == initialTerrain
        && weatherTurns == initialWeatherTurns && terrainTurns == initialTerrainTurns) return FALSE;
    *weatherOut = weather; *terrainOut = terrain; *weatherTurnsOut = weatherTurns; *terrainTurnsOut = terrainTurns;
    return TRUE;
}
int RhhModsAiInnateEntryStatContext(struct RhhAiInnateStatContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAiInnateStatContext
        || RhhModsCurrentEvent() != RHH_AI_INNATE_ENTRY_STAT_PREVIEW) return -1;
    *out = *sAiInnateStatContext; return 0;
}
int RhhModsOfferAiInnateEntryStat(u32 stat, s32 stages)
{
    if (!sAiInnateStatContext || RhhModsCurrentEvent() != RHH_AI_INNATE_ENTRY_STAT_PREVIEW
        || RhhModsCurrentStage() != RHH_CONSTRAINT || stat < RHH_STAT_ATTACK || stat >= RHH_BATTLE_STAT_COUNT
        || !stages || stages < -6 || stages > 6 || sAiInnateStatContext->entry.offered >= RHH_AI_INNATE_ENTRY_STAT_LIMIT) return -1;
    u32 index = sAiInnateStatContext->entry.offered++;
    sAiInnateStatOffers[index].stat = stat; sAiInnateStatOffers[index].stages = stages;
    sAiInnateStatOffers[index].opponent = FALSE; sAiInnateStatOffers[index].status = 0;
    sAiInnateStatContext->entry.capacity = RHH_AI_INNATE_ENTRY_STAT_LIMIT - sAiInnateStatContext->entry.offered;
    return 0;
}
int RhhModsAiInnateEntryOpponentContext(struct RhhAiInnateOpponentStatContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAiInnateStatContext
        || RhhModsCurrentEvent() != RHH_AI_INNATE_ENTRY_STAT_PREVIEW || gBattlersCount != 2
        || (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)) return -1;
    u32 opponent = GetOppositeBattler(sAiInnateStatContext->entry.battler);
    if (opponent >= gBattlersCount || (sPending & (1u << opponent)) || !IsBattlerAlive(opponent)) return -1;
    struct RhhAiInnateOpponentStatContext result = {.struct_size = sizeof(result), .battler = opponent};
    u32 ability = gAiLogicData->abilities[opponent], item = gAiLogicData->items[opponent];
    if (ability && ability < ABILITIES_COUNT) result.known_ability = RhhContentKeyForId(RHH_ABILITY, ability);
    if (item && item < ITEMS_COUNT) result.known_item = RhhContentKeyForId(RHH_ITEM, item);
    for (u32 i = RHH_STAT_ATTACK; i < RHH_BATTLE_STAT_COUNT; ++i)
    {
        if (gBattleMons[opponent].statStages[i] > MAX_STAT_STAGE) return -1;
        result.stages[i] = (s32)gBattleMons[opponent].statStages[i] - DEFAULT_STAT_STAGE;
    }
    *out = result; return 0;
}
int RhhModsOfferAiInnateEntryOpponentStat(u32 stat, s32 stages)
{
    struct RhhAiInnateOpponentStatContext target = {.struct_size = sizeof(target)};
    if (RhhModsCurrentStage() != RHH_CONSTRAINT || stat < RHH_STAT_ATTACK || stat >= RHH_BATTLE_STAT_COUNT
        || !stages || stages < -6 || stages > 6 || RhhModsAiInnateEntryOpponentContext(&target)
        || sAiInnateStatContext->entry.offered >= RHH_AI_INNATE_ENTRY_STAT_LIMIT) return -1;
    u32 index = sAiInnateStatContext->entry.offered++;
    sAiInnateStatOffers[index].stat = stat; sAiInnateStatOffers[index].stages = stages;
    sAiInnateStatOffers[index].opponent = TRUE; sAiInnateStatOffers[index].status = 0;
    sAiInnateStatContext->entry.capacity = RHH_AI_INNATE_ENTRY_STAT_LIMIT - sAiInnateStatContext->entry.offered;
    return 0;
}
int RhhModsAiInnateEntryOpponentStatusContext(struct RhhAiInnateOpponentStatusContext *out)
{
    struct RhhAiInnateOpponentStatContext target = {.struct_size = sizeof(target)};
    if (!out || out->struct_size < sizeof(*out) || RhhModsAiInnateEntryOpponentContext(&target)) return -1;
    u32 status = gBattleMons[target.battler].status1, kind = 0;
    if (status & STATUS1_SLEEP) kind = RHH_EFFECT_SLEEP;
    else if (status & STATUS1_POISON) kind = RHH_EFFECT_POISON;
    else if (status & STATUS1_BURN) kind = RHH_EFFECT_BURN;
    else if (status & STATUS1_FREEZE) kind = RHH_EFFECT_FREEZE;
    else if (status & STATUS1_PARALYSIS) kind = RHH_EFFECT_PARALYSIS;
    else if (status & STATUS1_TOXIC_POISON) kind = RHH_EFFECT_TOXIC;
    else if (status & STATUS1_ANY) return -1; /* No public frostbite status yet. */
    *out = (struct RhhAiInnateOpponentStatusContext){.struct_size = sizeof(*out), .battler = target.battler,
        .status = kind, .known_ability = target.known_ability, .known_item = target.known_item};
    return 0;
}
int RhhModsOfferAiInnateEntryOpponentStatus(u32 status)
{
    struct RhhAiInnateOpponentStatusContext target = {.struct_size = sizeof(target)};
    if (RhhModsCurrentStage() != RHH_CONSTRAINT || status < RHH_EFFECT_SLEEP || status > RHH_EFFECT_TOXIC
        || RhhModsAiInnateEntryOpponentStatusContext(&target)
        || sAiInnateStatContext->entry.offered >= RHH_AI_INNATE_ENTRY_STAT_LIMIT) return -1;
    u32 index = sAiInnateStatContext->entry.offered++;
    sAiInnateStatOffers[index].opponent = TRUE; sAiInnateStatOffers[index].status = status;
    sAiInnateStatContext->entry.capacity = RHH_AI_INNATE_ENTRY_STAT_LIMIT - sAiInnateStatContext->entry.offered;
    return 0;
}
/* This uses public types and cached ability/item knowledge, never the target's
 * unrevealed ability or held item. Unknown knowledge is treated as absent. */
static u32 AiInnateKnownAbility(u32 battler)
{
    u32 ability = gAiLogicData->abilities[battler], hold = gAiLogicData->holdEffects[battler];
    bool32 shield = hold == HOLD_EFFECT_ABILITY_SHIELD && !gBattleMons[battler].volatiles.embargoTimer
        && !(gFieldStatuses & STATUS_FIELD_MAGIC_ROOM);
    return gBattleMons[battler].volatiles.gastroAcid || (IsNeutralizingGasOnField() && !shield) ? ABILITY_NONE : ability;
}
/* Status cures run after Synchronize. Use only the cached hold effect and
 * opposing ability: the real item and unrevealed abilities are not inputs. */
static bool32 AiInnateStatusBerryCures(u32 battler, u32 status, u32 consumedItems)
{
    if ((consumedItems & (1u << battler)) || (gFieldStatuses & STATUS_FIELD_MAGIC_ROOM)
        || gBattleMons[battler].volatiles.embargoTimer || AiInnateKnownAbility(battler) == ABILITY_KLUTZ)
        return FALSE;
    u32 opponentAbility = AiInnateKnownAbility(GetOppositeBattler(battler));
    if (opponentAbility == ABILITY_UNNERVE || opponentAbility == ABILITY_AS_ONE_ICE_RIDER
        || opponentAbility == ABILITY_AS_ONE_SHADOW_RIDER) return FALSE;
    u32 hold = gAiLogicData->holdEffects[battler];
    return hold == HOLD_EFFECT_CURE_STATUS
        || (hold == HOLD_EFFECT_CURE_PSN && (status == RHH_EFFECT_POISON || status == RHH_EFFECT_TOXIC))
        || (hold == HOLD_EFFECT_CURE_BRN && status == RHH_EFFECT_BURN)
        || (hold == HOLD_EFFECT_CURE_PAR && status == RHH_EFFECT_PARALYSIS)
        || (hold == HOLD_EFFECT_CURE_SLP && status == RHH_EFFECT_SLEEP)
        || (hold == HOLD_EFFECT_CURE_FRZ && status == RHH_EFFECT_FREEZE);
}
static bool32 AiInnateStatusBlocked(u32 switcher, u32 target, u32 status, const struct BattleTypeList *types)
{
    if (!types->count || BattleTypeListContains(types, TYPE_MYSTERY)
        || (gBattleMons[target].status1 & STATUS1_ANY)
        || (gSideStatuses[GetBattlerSide(target)] & SIDE_STATUS_SAFEGUARD)) return TRUE;
    u32 ability = AiInnateKnownAbility(target), hold = gAiLogicData->holdEffects[target];
    u32 weather = AI_GetSwitchinWeather(switcher), terrain = AI_GetSwitchinTerrain(switcher);
    bool32 grounded = hold == HOLD_EFFECT_IRON_BALL || (gFieldStatuses & STATUS_FIELD_GRAVITY)
        || (B_ROOTED_GROUNDING >= GEN_4 && gBattleMons[target].volatiles.root) || gBattleMons[target].volatiles.smackDown;
    if (!grounded)
        grounded = !gBattleMons[target].volatiles.telekinesis && !gBattleMons[target].volatiles.magnetRiseTimer
            && hold != HOLD_EFFECT_AIR_BALLOON && ability != ABILITY_LEVITATE && ability != ABILITY_EELEVATE
            && !BattleTypeListContains(types, TYPE_FLYING);
    if (!IsSemiInvulnerable(target, CHECK_ALL) && grounded && terrain == B_TERRAIN_MISTY) return TRUE;
    if (ability == ABILITY_COMATOSE || ability == ABILITY_PURIFYING_SALT || ability == ABILITY_SHIELDS_DOWN
        || (ability == ABILITY_FLOWER_VEIL && BattleTypeListContains(types, TYPE_GRASS))
        || (ability == ABILITY_LEAF_GUARD && IsBattlerWeatherAffected(hold, weather, B_WEATHER_SUN))) return TRUE;
    switch (status)
    {
    case RHH_EFFECT_SLEEP:
        if (ability == ABILITY_INSOMNIA || ability == ABILITY_VITAL_SPIRIT || ability == ABILITY_SWEET_VEIL
            || IsSleepClauseActiveForSide(GetBattlerSide(target))
            || (!IsSemiInvulnerable(target, CHECK_ALL) && grounded && terrain == B_TERRAIN_ELECTRIC)) return TRUE;
        if (!(GetConfig(B_UPROAR_IGNORE_SOUNDPROOF) < GEN_5 && ability == ABILITY_SOUNDPROOF))
            for (u32 i = 0; i < gBattlersCount; ++i) if (gBattleMons[i].volatiles.uproarTurns) return TRUE;
        return FALSE;
    case RHH_EFFECT_POISON: case RHH_EFFECT_TOXIC:
        return BattleTypeListContains(types, TYPE_POISON) || BattleTypeListContains(types, TYPE_STEEL)
            || ability == ABILITY_IMMUNITY || ability == ABILITY_PASTEL_VEIL;
    case RHH_EFFECT_BURN:
        return BattleTypeListContains(types, TYPE_FIRE) || ability == ABILITY_WATER_VEIL
            || ability == ABILITY_WATER_BUBBLE || ability == ABILITY_THERMAL_EXCHANGE;
    case RHH_EFFECT_FREEZE:
        return BattleTypeListContains(types, TYPE_ICE) || ability == ABILITY_MAGMA_ARMOR
            || IsBattlerWeatherAffected(hold, weather, B_WEATHER_SUN);
    case RHH_EFFECT_PARALYSIS:
        return (GetConfig(B_PARALYZE_ELECTRIC) >= GEN_6 && BattleTypeListContains(types, TYPE_ELECTRIC)) || ability == ABILITY_LIMBER;
    default: return TRUE;
    }
}
bool32 ModsAiInnateEntryStatPreviewEnabled(void)
{ return AiInnateEntryPreviewEnabled(RHH_AI_INNATE_ENTRY_STAT_PREVIEW); }
bool32 ModsAiInnateEntryForecastDetailed(u32 battler, u32 partySlot, u32 hpAfterHazards, u8 *stages, u8 *opponentStages,
                                        u32 *ownStatus, u32 *opponentStatus, u32 *consumedItems)
{
    if (!stages || !opponentStages || !ownStatus || !opponentStatus || !consumedItems || !ModsAiInnateEntryStatPreviewEnabled() || gBattlersCount != 2
        || (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)) return FALSE;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
    if (AiInnateCandidate(battler, partySlot, hpAfterHazards, &resolution)) return FALSE;
    u32 opponent = GetOppositeBattler(battler);
    if (opponent >= gBattlersCount || !IsBattlerAlive(opponent)) return FALSE;
    struct RhhAiInnateStatContext context = {.struct_size = sizeof(context),
        .entry = {.struct_size = sizeof(context.entry), .battler = battler, .party_slot = partySlot,
            .hp = hpAfterHazards, .max_hp = gBattleMons[battler].maxHP, .capacity = RHH_AI_INNATE_ENTRY_STAT_LIMIT,
            .battle_id = sBattleId, .resolution = resolution}};
    for (u32 i = RHH_STAT_ATTACK; i < RHH_BATTLE_STAT_COUNT; ++i)
    {
        if (gBattleMons[battler].statStages[i] > MAX_STAT_STAGE || gBattleMons[opponent].statStages[i] > MAX_STAT_STAGE) return FALSE;
        context.stages[i] = (s32)gBattleMons[battler].statStages[i] - DEFAULT_STAT_STAGE;
    }
    for (u32 i = 0; i < resolution.count; ++i)
    {
        if (resolution.entries[i].status) continue;
        context.entry.index = i; context.entry.ability = resolution.entries[i].ability;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_AI_INNATE_ENTRY_STAT_PREVIEW,
            .run_seed = RhhModsRunSeed(), .event_key = context.entry.ability, .subject = resolution.species,
            .ordinal = i, .args = {battler, partySlot, hpAfterHazards, context.entry.max_hp}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        u32 before = context.entry.offered;
        sAiInnateStatContext = &context; RhhModsDispatch(&event, &decision); sAiInnateStatContext = NULL;
        if (!decision.allowed)
        { context.entry.offered = before; context.entry.capacity = RHH_AI_INNATE_ENTRY_STAT_LIMIT - before; }
    }
    if (memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2)))
    { gRngValue = rng; gRng2Value = rng2; return FALSE; }
    if (!context.entry.offered) return FALSE;
    u8 predicted[RHH_BATTLE_STAT_COUNT], opposing[RHH_BATTLE_STAT_COUNT];
    memcpy(predicted, gBattleMons[battler].statStages, sizeof(predicted));
    memcpy(opposing, gBattleMons[opponent].statStages, sizeof(opposing));
    u32 primary = gAiLogicData->abilities[battler];
    bool32 shield = gAiLogicData->holdEffects[battler] == HOLD_EFFECT_ABILITY_SHIELD
        && !gBattleMons[battler].volatiles.embargoTimer && !(gFieldStatuses & STATUS_FIELD_MAGIC_ROOM);
    if (gBattleMons[battler].volatiles.gastroAcid || (IsNeutralizingGasOnField() && !shield)) primary = ABILITY_NONE;
    for (u32 i = 0; i < context.entry.offered; ++i)
    {
        if (sAiInnateStatOffers[i].status) continue;
        u32 stat = sAiInnateStatOffers[i].stat; s32 delta = sAiInnateStatOffers[i].stages;
        if (!sAiInnateStatOffers[i].opponent)
        {
            if (primary == ABILITY_CONTRARY) delta = -delta;
            else if (primary == ABILITY_SIMPLE) delta *= 2;
            predicted[stat] = max(MIN_STAT_STAGE, min(MAX_STAT_STAGE, (s32)predicted[stat] + delta));
            continue;
        }
        struct BattleCalcValues cv = {.battlerAtk = battler, .battlerDef = opponent, .move = MOVE_NONE};
        for (u32 j = 0; j < gBattlersCount; ++j)
        { cv.abilities[j] = gAiLogicData->abilities[j]; cv.holdEffects[j] = gAiLogicData->holdEffects[j]; }
        cv.abilities[battler] = primary;
        u32 targetAbility = cv.abilities[opponent];
        bool32 targetShield = cv.holdEffects[opponent] == HOLD_EFFECT_ABILITY_SHIELD
            && !gBattleMons[opponent].volatiles.embargoTimer && !(gFieldStatuses & STATUS_FIELD_MAGIC_ROOM);
        if (gBattleMons[opponent].volatiles.gastroAcid || (IsNeutralizingGasOnField() && !targetShield))
            cv.abilities[opponent] = targetAbility = ABILITY_NONE;
        enum ModAbilityStatPreview result = PreviewAbilityStatChange(&cv, stat, &delta, FALSE, FALSE, TRUE);
        if (result == MOD_ABILITY_STAT_REFLECTED)
        {
            cv.battlerDef = battler;
            if (PreviewAbilityStatChange(&cv, stat, &delta, FALSE, TRUE, FALSE) == MOD_ABILITY_STAT_APPLIED)
            {
                u8 before = predicted[stat];
                predicted[stat] = max(MIN_STAT_STAGE, min(MAX_STAT_STAGE, (s32)before + delta));
                if (predicted[stat] < before && primary == ABILITY_DEFIANT)
                    predicted[STAT_ATK] = min(MAX_STAT_STAGE, predicted[STAT_ATK] + 2);
                else if (predicted[stat] < before && primary == ABILITY_COMPETITIVE)
                    predicted[STAT_SPATK] = min(MAX_STAT_STAGE, predicted[STAT_SPATK] + 2);
            }
        }
        else if (result == MOD_ABILITY_STAT_APPLIED)
        {
            u8 before = opposing[stat];
            opposing[stat] = max(MIN_STAT_STAGE, min(MAX_STAT_STAGE, (s32)before + delta));
            if (opposing[stat] < before && targetAbility == ABILITY_DEFIANT)
                opposing[STAT_ATK] = min(MAX_STAT_STAGE, opposing[STAT_ATK] + 2);
            else if (opposing[stat] < before && targetAbility == ABILITY_COMPETITIVE)
                opposing[STAT_SPATK] = min(MAX_STAT_STAGE, opposing[STAT_SPATK] + 2);
        }
    }
    u32 predictedStatus = gBattleMons[opponent].status1, predictedOwnStatus = gBattleMons[battler].status1, consumed = 0;
    struct BattleTypeList publicTypes = {0};
    for (u32 i = 0; i < context.entry.offered; ++i)
        if (sAiInnateStatOffers[i].status) { GetBattlerPublicTypeList(opponent, &publicTypes); break; }
    static const u32 statusBits[] = {0, STATUS1_SLEEP_TURN(2), STATUS1_POISON, STATUS1_BURN,
                                     STATUS1_FREEZE, STATUS1_PARALYSIS, STATUS1_TOXIC_POISON};
    for (u32 i = 0; i < context.entry.offered; ++i)
    {
        u32 status = sAiInnateStatOffers[i].status;
        if (status && !(predictedStatus & STATUS1_ANY) && !AiInnateStatusBlocked(battler, opponent, status, &publicTypes))
        {
            predictedStatus = statusBits[status];
            if ((status == RHH_EFFECT_POISON || status == RHH_EFFECT_TOXIC || status == RHH_EFFECT_BURN || status == RHH_EFFECT_PARALYSIS)
                && AiInnateKnownAbility(opponent) == ABILITY_SYNCHRONIZE && !(predictedOwnStatus & STATUS1_ANY))
            {
                struct BattleTypeList ownTypes; GetBattlerTypeList(battler, FALSE, &ownTypes);
                if (!AiInnateStatusBlocked(battler, battler, status, &ownTypes))
                {
                    u32 reflected = status == RHH_EFFECT_TOXIC && GetConfig(B_SYNCHRONIZE_TOXIC) < GEN_5
                        ? RHH_EFFECT_POISON : status;
                    predictedOwnStatus = statusBits[reflected];
                    if (AiInnateStatusBerryCures(battler, reflected, consumed))
                    { predictedOwnStatus = 0; consumed |= 1u << battler; }
                }
            }
            if (AiInnateStatusBerryCures(opponent, status, consumed))
            { predictedStatus = 0; consumed |= 1u << opponent; }
        }
    }
    bool32 changed = memcmp(predicted, gBattleMons[battler].statStages, sizeof(predicted)) != 0
        || memcmp(opposing, gBattleMons[opponent].statStages, sizeof(opposing)) != 0
        || predictedStatus != gBattleMons[opponent].status1 || predictedOwnStatus != gBattleMons[battler].status1 || consumed;
    if (changed) { memcpy(stages, predicted, sizeof(predicted)); memcpy(opponentStages, opposing, sizeof(opposing));
        *ownStatus = predictedOwnStatus; *opponentStatus = predictedStatus; *consumedItems = consumed; }
    return changed;
}
bool32 ModsAiInnateEntryForecastStatuses(u32 battler, u32 partySlot, u32 hpAfterHazards, u8 *stages, u8 *opponentStages,
                                        u32 *ownStatus, u32 *opponentStatus)
{
    u32 consumedItems;
    return ModsAiInnateEntryForecastDetailed(battler, partySlot, hpAfterHazards, stages, opponentStages, ownStatus, opponentStatus, &consumedItems);
}
bool32 ModsAiInnateEntryForecast(u32 battler, u32 partySlot, u32 hpAfterHazards, u8 *stages, u8 *opponentStages, u32 *opponentStatus)
{
    u32 ownStatus;
    return ModsAiInnateEntryForecastStatuses(battler, partySlot, hpAfterHazards, stages, opponentStages, &ownStatus, opponentStatus);
}
bool32 ModsAiInnateEntryStatChanges(u32 battler, u32 partySlot, u32 hpAfterHazards, u8 *stages, u8 *opponentStages)
{
    u32 status; u8 self[RHH_BATTLE_STAT_COUNT], opponent[RHH_BATTLE_STAT_COUNT];
    if (!stages || !opponentStages || !ModsAiInnateEntryForecast(battler, partySlot, hpAfterHazards, self, opponent, &status)
        || (!memcmp(self, gBattleMons[battler].statStages, sizeof(self))
            && !memcmp(opponent, gBattleMons[GetOppositeBattler(battler)].statStages, sizeof(opponent)))) return FALSE;
    memcpy(stages, self, sizeof(self)); memcpy(opponentStages, opponent, sizeof(opponent)); return TRUE;
}
bool32 ModsAiInnateEntryStats(u32 battler, u32 partySlot, u32 hpAfterHazards, u8 *stages)
{
    u8 self[RHH_BATTLE_STAT_COUNT], opponent[RHH_BATTLE_STAT_COUNT];
    if (!stages || !ModsAiInnateEntryStatChanges(battler, partySlot, hpAfterHazards, self, opponent)
        || !memcmp(self, gBattleMons[battler].statStages, sizeof(self))) return FALSE;
    memcpy(stages, self, sizeof(self)); return TRUE;
}
int RhhModsBattlerInnates(uint32_t battler, struct RhhInnateResolution *out)
{
    if (!RhhModsAbilityViewAllowed()) return -1;
    return ResolveInnates(battler, NULL, out);
}
int RhhModsInnateEntryContext(struct RhhInnateEntryContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnateEntryContext
        || RhhModsCurrentEvent() != RHH_INNATE_ENTRY) return -1;
    *out = *sInnateEntryContext; return 0;
}
int RhhModsInnateTurnContext(struct RhhInnateTurnContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnateTurnContext
        || RhhModsCurrentEvent() != RHH_INNATE_TURN_END) return -1;
    *out = *sInnateTurnContext; return 0;
}
int RhhModsPowerInnates(uint32_t role, struct RhhInnateResolution *out)
{
    if (!out || out->struct_size < sizeof(*out) || RhhModsCurrentEvent() != RHH_MOVE_POWER || !sPowerDamageContext
        || (role != RHH_INNATE_POWER_ATTACKER && role != RHH_INNATE_POWER_DEFENDER)) return -1;
    u32 battler = role == RHH_INNATE_POWER_ATTACKER ? sPowerDamageContext->battlerAtk : sPowerDamageContext->battlerDef;
    struct RhhInnateResolution result = {.struct_size = sizeof(result)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    int status = ResolveInnates(battler, sPowerDamageContext, &result);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (status || !pure) return -1;
    *out = result; return 0;
}
int RhhModsInnatePowerContext(struct RhhInnatePowerContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnatePowerContext
        || RhhModsCurrentEvent() != RHH_INNATE_POWER) return -1;
    *out = *sInnatePowerContext; return 0;
}
static u32 ApplyInnatePower(const struct DamageContext *damage, const struct RhhMovePowerContext *move, u32 power)
{
    if (!RhhModsHasHook(RHH_INNATE_POWER)) return power;
    for (u32 role = RHH_INNATE_POWER_ATTACKER; role <= RHH_INNATE_POWER_DEFENDER; ++role)
    {
        u32 battler = role == RHH_INNATE_POWER_ATTACKER ? damage->battlerAtk : damage->battlerDef;
        if (role == RHH_INNATE_POWER_DEFENDER && damage->battlerDef == damage->battlerAtk) continue;
        struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        int status = ResolveInnates(battler, damage, &resolution);
        bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
        gRngValue = rng; gRng2Value = rng2;
        if (status || !pure) continue;
        for (u32 i = 0; i < resolution.count; ++i)
        {
            if (resolution.entries[i].status) continue;
            struct RhhInnatePowerContext context = {.struct_size = sizeof(context), .role = role, .index = i,
                .current_power = power, .ability = resolution.entries[i].ability, .move = *move, .resolution = resolution};
            struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_INNATE_POWER,
                .run_seed = RhhModsRunSeed(), .event_key = context.ability, .subject = move->move,
                .ordinal = i, .args = {power, role, damage->battlerAtk, damage->battlerDef}};
            struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = power};
            rng = gRngValue; rng2 = gRng2Value;
            sInnatePowerContext = &context; RhhModsDispatch(&event, &decision); sInnatePowerContext = NULL;
            pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
            gRngValue = rng; gRng2Value = rng2;
            if (pure && decision.allowed && decision.value && decision.value <= RHH_MOVE_POWER_MAX) power = decision.value;
        }
    }
    return power;
}
int RhhModsInnateAccuracyContext(struct RhhInnateAccuracyContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnateAccuracyContext
        || RhhModsCurrentEvent() != RHH_INNATE_ACCURACY) return -1;
    *out = *sInnateAccuracyContext; return 0;
}
int RhhModsAccuracyInnates(uint32_t role, struct RhhInnateResolution *out)
{
    if (!out || out->struct_size < sizeof(*out) || RhhModsCurrentEvent() != RHH_MOVE_ACCURACY || !sAccuracyDamageContext
        || (role != RHH_INNATE_ACCURACY_ATTACKER && role != RHH_INNATE_ACCURACY_DEFENDER)) return -1;
    u32 battler = role == RHH_INNATE_ACCURACY_ATTACKER ? sAccuracyDamageContext->battlerAtk : sAccuracyDamageContext->battlerDef;
    struct RhhInnateResolution result = {.struct_size = sizeof(result)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    int status = ResolveInnates(battler, sAccuracyDamageContext, &result);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (status || !pure) return -1;
    *out = result; return 0;
}
static u32 ApplyInnateAccuracy(const struct DamageContext *calculation, const struct RhhMoveAccuracyContext *move, u32 accuracy)
{
    if (!RhhModsHasHook(RHH_INNATE_ACCURACY)) return accuracy;
    for (u32 role = RHH_INNATE_ACCURACY_ATTACKER; role <= RHH_INNATE_ACCURACY_DEFENDER; ++role)
    {
        u32 battler = role == RHH_INNATE_ACCURACY_ATTACKER ? calculation->battlerAtk : calculation->battlerDef;
        if (role == RHH_INNATE_ACCURACY_DEFENDER && calculation->battlerDef == calculation->battlerAtk) continue;
        struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        int status = ResolveInnates(battler, calculation, &resolution);
        bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
        gRngValue = rng; gRng2Value = rng2;
        if (status || !pure) continue;
        for (u32 i = 0; i < resolution.count; ++i)
        {
            if (resolution.entries[i].status) continue;
            struct RhhInnateAccuracyContext context = {.struct_size = sizeof(context), .role = role, .index = i,
                .current_accuracy = accuracy, .ability = resolution.entries[i].ability, .move = *move, .resolution = resolution};
            struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_INNATE_ACCURACY,
                .run_seed = RhhModsRunSeed(), .event_key = context.ability, .subject = move->move,
                .ordinal = i, .args = {accuracy, role, calculation->battlerAtk, calculation->battlerDef}};
            struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = accuracy};
            rng = gRngValue; rng2 = gRng2Value;
            sInnateAccuracyContext = &context; RhhModsDispatch(&event, &decision); sInnateAccuracyContext = NULL;
            pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
            gRngValue = rng; gRng2Value = rng2;
            if (pure && decision.allowed && decision.value <= 100) accuracy = decision.value;
        }
    }
    return accuracy;
}
struct AiEffectCollector {
    struct RhhAiEffectAction actions[RHH_AI_EFFECT_PREVIEW_LIMIT];
    u32 recipients[RHH_AI_EFFECT_PREVIEW_LIMIT];
    u32 count, mode, move;
};
static struct AiEffectCollector *sAiEffects;
static bool32 IsAiContextEvent(u32 event)
{ return event == RHH_AI_MOVE_SCORE || event == RHH_AI_EFFECT_PREVIEW; }
static int PowerContext(struct RhhMovePowerContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sPowerContext
        || RhhModsCurrentEvent() != RHH_MOVE_POWER) return -1;
    *out = *sPowerContext; return 0;
}
static int AccuracyContext(struct RhhMoveAccuracyContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAccuracyContext
        || RhhModsCurrentEvent() != RHH_MOVE_ACCURACY) return -1;
    *out = *sAccuracyContext; return 0;
}
static int ResolutionContext(struct RhhMoveResolutionContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sResolutionContext
        || RhhModsCurrentEvent() != RHH_MOVE_RESOLVED) return -1;
    *out = *sResolutionContext; return 0;
}
static int ResolutionDamage(u32 battler, struct RhhMoveResolutionDamage *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sResolutionContext || !sResolutionDamageContext
        || RhhModsCurrentEvent() != RHH_MOVE_RESOLVED || battler >= sResolutionContext->battler_count) return -1;
    *out = sResolutionDamageContext[battler]; return 0;
}
static int ResolutionHealth(u32 battler, struct RhhMoveResolutionHealth *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sResolutionContext || !sResolutionHealthContext
        || RhhModsCurrentEvent() != RHH_MOVE_RESOLVED || battler >= sResolutionContext->battler_count) return -1;
    *out = sResolutionHealthContext[battler]; return 0;
}
static int ResolutionHealthComparison(u32 battler, struct RhhMoveHealthComparison *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sResolutionContext || !sResolutionHealthComparisonContext
        || RhhModsCurrentEvent() != RHH_MOVE_RESOLVED || battler >= sResolutionContext->battler_count) return -1;
    *out = sResolutionHealthComparisonContext[battler]; return 0;
}
static int AiScoreContext(struct RhhAiMoveScoreContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAiScoreContext
        || !IsAiContextEvent(RhhModsCurrentEvent())) return -1;
    *out = *sAiScoreContext; return 0;
}
static int AiEvaluation(struct RhhAiMoveEvaluation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAiEvaluation
        || !IsAiContextEvent(RhhModsCurrentEvent())) return -1;
    *out = *sAiEvaluation; return 0;
}
static int AiEffectOffer(const struct RhhAiEffectAction *action)
{
    if (!sAiEffects || !sAiScoreContext || RhhModsCurrentEvent() != RHH_AI_EFFECT_PREVIEW
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return RHH_AI_EFFECT_UNAVAILABLE;
    if (!action || action->struct_size < sizeof(*action) || action->reserved
        || !action->chance || action->chance > 100 || action->kind < RHH_AI_EFFECT_HEAL_TARGET
        || action->kind > RHH_AI_EFFECT_CONDITION) return RHH_AI_EFFECT_INVALID;
    bool32 fraction = action->kind == RHH_AI_EFFECT_HEAL_TARGET || action->kind == RHH_AI_EFFECT_HEAL_USER
        || action->kind == RHH_AI_EFFECT_HEAL_USER_FROM_DAMAGE || action->kind == RHH_AI_EFFECT_DRAIN_USER;
    bool32 field = sAiScoreContext->move_target == RHH_MOVE_TARGET_FIELD
        || sAiScoreContext->move_target == RHH_MOVE_TARGET_OPPONENTS_FIELD;
    if ((action->kind == RHH_AI_EFFECT_HEAL_USER_FROM_DAMAGE || action->kind == RHH_AI_EFFECT_DRAIN_USER)
        && sAiEffects->mode != RHH_MOVE_EFFECT_CUSTOM_DAMAGE) return RHH_AI_EFFECT_INVALID;
    if (field && (action->kind == RHH_AI_EFFECT_HEAL_TARGET || action->kind == RHH_AI_EFFECT_STATUS_TARGET
                  || action->kind == RHH_AI_EFFECT_STAT_TARGET)) return RHH_AI_EFFECT_INVALID;
    if (fraction)
    {
        u32 allowedFlags = RHH_AI_EFFECT_AUTO_SCORE | (action->kind == RHH_AI_EFFECT_HEAL_USER_FROM_DAMAGE ? RHH_EFFECT_HEAL_INCLUDE_SUBSTITUTE
            : action->kind == RHH_AI_EFFECT_DRAIN_USER ? RHH_EFFECT_DRAIN_INCLUDE_SUBSTITUTE : 0);
        if (!action->first || action->first > action->second || action->second > 16
            || (action->flags & ~allowedFlags) || action->stages || action->condition) return RHH_AI_EFFECT_INVALID;
    }
    else if (action->kind == RHH_AI_EFFECT_STATUS_TARGET)
    {
        if ((action->flags & ~RHH_AI_EFFECT_AUTO_SCORE) || !action->first || action->first > RHH_EFFECT_CONFUSION
            || action->second || action->stages || action->condition) return RHH_AI_EFFECT_INVALID;
    }
    else if (action->kind == RHH_AI_EFFECT_STAT_TARGET)
    {
        if ((action->flags & ~RHH_AI_EFFECT_AUTO_SCORE) || action->first < RHH_STAT_ATTACK || action->first >= RHH_BATTLE_STAT_COUNT
            || action->second || action->stages < -6 || action->stages > 6 || !action->stages
            || action->condition) return RHH_AI_EFFECT_INVALID;
    }
    else
    {
        int index = ConditionIndex(action->condition, action->first);
        if ((action->flags & ~RHH_AI_EFFECT_AUTO_SCORE) || action->stages || index < 0
            || action->second > RhhModsBattleConditionAt(index)->max_turns) return RHH_AI_EFFECT_INVALID;
    }
    if (sAiEffects->count >= RHH_AI_EFFECT_PREVIEW_LIMIT) return RHH_AI_EFFECT_FULL;
    struct RhhAiEffectAction copy = *action; copy.struct_size = sizeof(copy);
    sAiEffects->actions[sAiEffects->count++] = copy; return RHH_AI_EFFECT_OK;
}
static int BattleFieldOfferAiMoveEffect(u32 rule, u32 chance)
{
    if (!sAiEffects || !sAiScoreContext || RhhModsCurrentEvent() != RHH_AI_EFFECT_PREVIEW
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return RHH_AI_EFFECT_UNAVAILABLE;
    if ((rule != RHH_BATTLE_FIELD_GRAVITY && rule != RHH_BATTLE_FIELD_ION_DELUGE) || !chance || chance > 100)
        return RHH_AI_EFFECT_INVALID;
    if (sAiEffects->count >= RHH_AI_EFFECT_PREVIEW_LIMIT) return RHH_AI_EFFECT_FULL;
    u32 index = sAiEffects->count++;
    sAiEffects->actions[index] = (struct RhhAiEffectAction){.struct_size = sizeof(struct RhhAiEffectAction),
        .kind = RHH_AI_EFFECT_FIELD_RULE, .chance = chance, .first = rule};
    sAiEffects->recipients[index] = RHH_AI_EFFECT_NO_BATTLER;
    return RHH_AI_EFFECT_OK;
}
static bool32 IsRoomRule(u32 rule)
{ return rule == RHH_BATTLE_FIELD_TRICK_ROOM || rule == RHH_BATTLE_FIELD_WONDER_ROOM || rule == RHH_BATTLE_FIELD_MAGIC_ROOM; }
static int BattleFieldOfferAiRoomToggle(u32 rule, u32 chance)
{
    if (!sAiEffects || !sAiScoreContext || RhhModsCurrentEvent() != RHH_AI_EFFECT_PREVIEW
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return RHH_AI_EFFECT_UNAVAILABLE;
    if (!IsRoomRule(rule) || !chance || chance > 100) return RHH_AI_EFFECT_INVALID;
    if (sAiEffects->count >= RHH_AI_EFFECT_PREVIEW_LIMIT) return RHH_AI_EFFECT_FULL;
    u32 index = sAiEffects->count++;
    sAiEffects->actions[index] = (struct RhhAiEffectAction){.struct_size = sizeof(struct RhhAiEffectAction),
        .kind = RHH_AI_EFFECT_ROOM_TOGGLE, .chance = chance, .first = rule};
    sAiEffects->recipients[index] = RHH_AI_EFFECT_NO_BATTLER;
    return RHH_AI_EFFECT_OK;
}
static bool32 IsFieldMoveRule(u32 rule)
{ return rule == RHH_BATTLE_FIELD_MUD_SPORT || rule == RHH_BATTLE_FIELD_WATER_SPORT || rule == RHH_BATTLE_FIELD_FAIRY_LOCK; }
static int BattleFieldOfferAiFieldMove(u32 rule, u32 chance)
{
    if (!sAiEffects || !sAiScoreContext || RhhModsCurrentEvent() != RHH_AI_EFFECT_PREVIEW
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return RHH_AI_EFFECT_UNAVAILABLE;
    if (!IsFieldMoveRule(rule) || !chance || chance > 100) return RHH_AI_EFFECT_INVALID;
    if (sAiEffects->count >= RHH_AI_EFFECT_PREVIEW_LIMIT) return RHH_AI_EFFECT_FULL;
    u32 index = sAiEffects->count++;
    sAiEffects->actions[index] = (struct RhhAiEffectAction){.struct_size = sizeof(struct RhhAiEffectAction),
        .kind = RHH_AI_EFFECT_FIELD_RULE, .chance = chance, .first = rule};
    sAiEffects->recipients[index] = RHH_AI_EFFECT_NO_BATTLER;
    return RHH_AI_EFFECT_OK;
}
static int AiEffectCount(u32 *out)
{
    if (!out || !sAiEffects || RhhModsCurrentEvent() != RHH_AI_MOVE_SCORE) return -1;
    *out = sAiEffects->count; return 0;
}
static int AiEffectOfferBattler(u32 kind, u32 battler, u32 numerator, u32 denominator, u32 chance, u32 flags)
{
    if (!sAiEffects || !sAiScoreContext || RhhModsCurrentEvent() != RHH_AI_EFFECT_PREVIEW
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return RHH_AI_EFFECT_UNAVAILABLE;
    if (battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || !IsBattlerAlive(battler)
        || !numerator || numerator > denominator || denominator > 16 || !chance || chance > 100
        || (kind == RHH_AI_EFFECT_DAMAGE_FROM_HIT_BATTLER && sAiEffects->mode != RHH_MOVE_EFFECT_CUSTOM_DAMAGE)
        || (flags & ~(RHH_AI_EFFECT_AUTO_SCORE | ((kind == RHH_AI_EFFECT_DAMAGE_BATTLER
                    || kind == RHH_AI_EFFECT_DAMAGE_FROM_HIT_BATTLER) ? RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD : 0)
                    | (kind == RHH_AI_EFFECT_DAMAGE_FROM_HIT_BATTLER ? RHH_EFFECT_DAMAGE_FROM_HIT_INCLUDE_SUBSTITUTE : 0)))) return RHH_AI_EFFECT_INVALID;
    if (sAiEffects->count >= RHH_AI_EFFECT_PREVIEW_LIMIT) return RHH_AI_EFFECT_FULL;
    u32 index = sAiEffects->count++;
    sAiEffects->actions[index] = (struct RhhAiEffectAction){.struct_size = sizeof(struct RhhAiEffectAction),
        .kind = kind, .chance = chance, .flags = flags, .first = numerator, .second = denominator};
    sAiEffects->recipients[index] = battler;
    return RHH_AI_EFFECT_OK;
}
static int AiEffectOfferHealBattler(u32 battler, u32 numerator, u32 denominator, u32 chance, u32 flags)
{ return AiEffectOfferBattler(RHH_AI_EFFECT_HEAL_BATTLER, battler, numerator, denominator, chance, flags); }
static int AiEffectOfferDamageBattler(u32 battler, u32 numerator, u32 denominator, u32 chance, u32 flags)
{ return AiEffectOfferBattler(RHH_AI_EFFECT_DAMAGE_BATTLER, battler, numerator, denominator, chance, flags); }
static int AiEffectOfferDamageFromHit(u32 battler, u32 numerator, u32 denominator, u32 chance, u32 flags)
{ return AiEffectOfferBattler(RHH_AI_EFFECT_DAMAGE_FROM_HIT_BATTLER, battler, numerator, denominator, chance, flags); }
static int AiEffectOfferStatBattler(u32 battler, u32 stat, s32 stages, u32 chance, u32 flags)
{
    if (!sAiEffects || !sAiScoreContext || RhhModsCurrentEvent() != RHH_AI_EFFECT_PREVIEW
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return RHH_AI_EFFECT_UNAVAILABLE;
    if (battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || !IsBattlerAlive(battler)
        || stat < RHH_STAT_ATTACK || stat >= RHH_BATTLE_STAT_COUNT || stages < -6 || stages > 6 || !stages
        || !chance || chance > 100 || (flags & ~RHH_AI_EFFECT_AUTO_SCORE)) return RHH_AI_EFFECT_INVALID;
    if (sAiEffects->count >= RHH_AI_EFFECT_PREVIEW_LIMIT) return RHH_AI_EFFECT_FULL;
    u32 index = sAiEffects->count++;
    sAiEffects->actions[index] = (struct RhhAiEffectAction){.struct_size = sizeof(struct RhhAiEffectAction),
        .kind = RHH_AI_EFFECT_STAT_BATTLER, .chance = chance, .flags = flags, .first = stat, .stages = stages};
    sAiEffects->recipients[index] = battler;
    return RHH_AI_EFFECT_OK;
}
static int AiEffectOfferStatusBattler(u32 battler, u32 status, u32 chance, u32 flags)
{
    if (!sAiEffects || !sAiScoreContext || RhhModsCurrentEvent() != RHH_AI_EFFECT_PREVIEW
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return RHH_AI_EFFECT_UNAVAILABLE;
    if (battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || !IsBattlerAlive(battler)
        || !status || status > RHH_EFFECT_CONFUSION || !chance || chance > 100
        || (flags & ~RHH_AI_EFFECT_AUTO_SCORE)) return RHH_AI_EFFECT_INVALID;
    if (sAiEffects->count >= RHH_AI_EFFECT_PREVIEW_LIMIT) return RHH_AI_EFFECT_FULL;
    u32 index = sAiEffects->count++;
    sAiEffects->actions[index] = (struct RhhAiEffectAction){.struct_size = sizeof(struct RhhAiEffectAction),
        .kind = RHH_AI_EFFECT_STATUS_BATTLER, .chance = chance, .flags = flags, .first = status};
    sAiEffects->recipients[index] = battler;
    return RHH_AI_EFFECT_OK;
}
static int AiEffectBattlerAt(u32 index, u32 *battler, u32 kind)
{
    if (!battler || !sAiEffects || RhhModsCurrentEvent() != RHH_AI_MOVE_SCORE
        || index >= sAiEffects->count || sAiEffects->actions[index].kind != kind) return -1;
    *battler = sAiEffects->recipients[index]; return 0;
}
static int AiEffectHealBattlerAt(u32 index, u32 *battler)
{ return AiEffectBattlerAt(index, battler, RHH_AI_EFFECT_HEAL_BATTLER); }
static int AiEffectDamageBattlerAt(u32 index, u32 *battler)
{ return AiEffectBattlerAt(index, battler, RHH_AI_EFFECT_DAMAGE_BATTLER); }
static int AiEffectDamageFromHitAt(u32 index, u32 *battler)
{ return AiEffectBattlerAt(index, battler, RHH_AI_EFFECT_DAMAGE_FROM_HIT_BATTLER); }
static int AiEffectStatBattlerAt(u32 index, u32 *battler)
{ return AiEffectBattlerAt(index, battler, RHH_AI_EFFECT_STAT_BATTLER); }
static int AiEffectStatusBattlerAt(u32 index, u32 *battler)
{ return AiEffectBattlerAt(index, battler, RHH_AI_EFFECT_STATUS_BATTLER); }
static int AiEffectAt(u32 index, struct RhhAiEffectAction *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sAiEffects
        || RhhModsCurrentEvent() != RHH_AI_MOVE_SCORE || index >= sAiEffects->count) return -1;
    *out = sAiEffects->actions[index]; return 0;
}
static u32 AiHealingAmountAt(u32 battler, u32 amount, u32 hp)
{
    u32 maxHp = gBattleMons[battler].maxHP;
    return hp < maxHp ? min(amount, maxHp - hp) : 0;
}
static bool32 AiPreviewSubstitute(u32 attacker, u32 target, u32 move)
{
    if (attacker == target || !gBattleMons[target].volatiles.substitute || GetMoveInfo(move)->ignoresSubstitute)
        return FALSE;
    /* Unknown attacker abilities are not replaced with the real ability. */
    return gAiLogicData->abilities[attacker] != ABILITY_INFILTRATOR || GetConfig(B_INFILTRATOR_SUBSTITUTE) < GEN_6;
}
static bool32 AiKnownAbilityIgnored(u32 attacker, u32 target)
{
    u32 ability = gAiLogicData->abilities[attacker];
    return attacker != target && gAiLogicData->holdEffects[target] != HOLD_EFFECT_ABILITY_SHIELD
        && (ability == ABILITY_MOLD_BREAKER || ability == ABILITY_TERAVOLT || ability == ABILITY_TURBOBLAZE);
}
static bool32 AiKnownStatusBlocked(u32 attacker, u32 target, u32 status)
{
    u32 attackAbility = gAiLogicData->abilities[attacker], ability = gAiLogicData->abilities[target];
    struct BattleTypeList types;
    AI_GetKnownBattlerTypeList(attacker, target, &types);
    if ((status == RHH_EFFECT_POISON || status == RHH_EFFECT_TOXIC)
        && attackAbility != ABILITY_CORROSION && (BattleTypeListContains(&types, TYPE_POISON)
            || BattleTypeListContains(&types, TYPE_STEEL))) return TRUE;
    if (status == RHH_EFFECT_BURN && BattleTypeListContains(&types, TYPE_FIRE)) return TRUE;
    if (status == RHH_EFFECT_FREEZE && BattleTypeListContains(&types, TYPE_ICE)) return TRUE;
    if (status == RHH_EFFECT_PARALYSIS && GetConfig(B_PARALYZE_ELECTRIC) >= GEN_6
        && BattleTypeListContains(&types, TYPE_ELECTRIC)) return TRUE;
    if (!ability || AiKnownAbilityIgnored(attacker, target)) return FALSE;
    if (status == RHH_EFFECT_CONFUSION) return ability == ABILITY_OWN_TEMPO;
    if (ability == ABILITY_COMATOSE || ability == ABILITY_PURIFYING_SALT) return TRUE;
    if (status == RHH_EFFECT_SLEEP) return ability == ABILITY_INSOMNIA || ability == ABILITY_VITAL_SPIRIT;
    if (status == RHH_EFFECT_POISON || status == RHH_EFFECT_TOXIC) return ability == ABILITY_IMMUNITY;
    if (status == RHH_EFFECT_BURN) return ability == ABILITY_WATER_VEIL || ability == ABILITY_WATER_BUBBLE || ability == ABILITY_THERMAL_EXCHANGE;
    if (status == RHH_EFFECT_FREEZE) return ability == ABILITY_MAGMA_ARMOR;
    if (status == RHH_EFFECT_PARALYSIS) return ability == ABILITY_LIMBER;
    return FALSE;
}
static int EstimateAiEffect(const struct AiEffectCollector *collector, u32 index, u32 recipient,
                            struct RhhAiEffectEstimate *out)
{
    if (!collector || !sAiScoreContext || !sAiEvaluation || !gAiLogicData || index >= collector->count) return -1;
    const struct RhhAiEffectAction *action = &collector->actions[index];
    u32 attacker = sAiScoreContext->attacker, candidate = sAiScoreContext->candidate;
    bool32 user = action->kind == RHH_AI_EFFECT_HEAL_USER || action->kind == RHH_AI_EFFECT_HEAL_USER_FROM_DAMAGE
        || action->kind == RHH_AI_EFFECT_DRAIN_USER;
    bool32 fieldAction = action->kind == RHH_AI_EFFECT_CONDITION || action->kind == RHH_AI_EFFECT_FIELD_RULE
        || action->kind == RHH_AI_EFFECT_ROOM_TOGGLE;
    if ((fieldAction && recipient != RHH_AI_EFFECT_NO_BATTLER)
        || (!fieldAction && (recipient >= gBattlersCount || recipient >= MAX_BATTLERS_COUNT))
        || (user && recipient != attacker)
        || ((action->kind == RHH_AI_EFFECT_HEAL_BATTLER || action->kind == RHH_AI_EFFECT_DAMAGE_BATTLER
             || action->kind == RHH_AI_EFFECT_DAMAGE_FROM_HIT_BATTLER
             || action->kind == RHH_AI_EFFECT_STAT_BATTLER || action->kind == RHH_AI_EFFECT_STATUS_BATTLER)
            && recipient != collector->recipients[index])) return -1;
    struct RhhAiEffectEstimate result = {.struct_size = sizeof(result), .kind = action->kind,
        .recipient = recipient, .chance = action->chance, .condition = action->condition};
    if (action->kind == RHH_AI_EFFECT_CONDITION)
    {
        int condition = ConditionIndex(action->condition, action->first);
        if (condition < 0) return -1;
        result.minimum = result.median = result.maximum = (s32)action->second - sConditionTurns[condition][action->first];
        result.flags = RHH_AI_ESTIMATE_CONDITION_SEMANTICS_UNKNOWN;
    }
    else if (action->kind == RHH_AI_EFFECT_FIELD_RULE)
    {
        bool32 blocked = FALSE;
        switch (action->first)
        {
        case RHH_BATTLE_FIELD_GRAVITY: blocked = !!(gFieldStatuses & STATUS_FIELD_GRAVITY); break;
        case RHH_BATTLE_FIELD_ION_DELUGE: blocked = !!(gFieldStatuses & STATUS_FIELD_ION_DELUGE); break;
        case RHH_BATTLE_FIELD_FAIRY_LOCK: blocked = !!(gFieldStatuses & STATUS_FIELD_FAIRY_LOCK); break;
        case RHH_BATTLE_FIELD_MUD_SPORT:
            blocked = gBattleStruct->isSkyBattle || (B_SPORT_TURNS >= GEN_6
                ? !!(gFieldStatuses & STATUS_FIELD_MUDSPORT) : gBattleMons[attacker].volatiles.mudSport); break;
        case RHH_BATTLE_FIELD_WATER_SPORT:
            blocked = gBattleStruct->isSkyBattle || (B_SPORT_TURNS >= GEN_6
                ? !!(gFieldStatuses & STATUS_FIELD_WATERSPORT) : gBattleMons[attacker].volatiles.waterSport); break;
        default: return -1;
        }
        if (blocked) result.flags |= RHH_AI_ESTIMATE_BLOCKED;
        else result.minimum = result.median = result.maximum = 1;
    }
    else if (action->kind == RHH_AI_EFFECT_ROOM_TOGGLE)
    {
        u32 flag = action->first == RHH_BATTLE_FIELD_TRICK_ROOM ? STATUS_FIELD_TRICK_ROOM
            : action->first == RHH_BATTLE_FIELD_WONDER_ROOM ? STATUS_FIELD_WONDER_ROOM : STATUS_FIELD_MAGIC_ROOM;
        result.minimum = result.median = result.maximum = (gFieldStatuses & flag) ? -1 : 1;
    }
    else if (!IsBattlerAlive(recipient)) result.flags = RHH_AI_ESTIMATE_BLOCKED;
    else if (action->kind == RHH_AI_EFFECT_DAMAGE_FROM_HIT_BATTLER)
    {
        result.flags = RHH_AI_ESTIMATE_UNCERTAIN | RHH_AI_ESTIMATE_DAMAGE_CACHE;
        bool32 substitute = AiPreviewSubstitute(attacker, candidate, collector->move);
        if (substitute) result.flags |= RHH_AI_ESTIMATE_SUBSTITUTE;
        if ((!(action->flags & RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD) && gAiLogicData->abilities[recipient] == ABILITY_MAGIC_GUARD)
            || (substitute && !(action->flags & RHH_EFFECT_DAMAGE_FROM_HIT_INCLUDE_SUBSTITUTE)))
            result.flags |= RHH_AI_ESTIMATE_BLOCKED;
        else
        {
            u32 values[] = {min(sAiEvaluation->damage_min, sAiEvaluation->damage_median), sAiEvaluation->damage_median,
                            max(sAiEvaluation->damage_max, sAiEvaluation->damage_median)};
            u32 amounts[3], hp = gBattleMons[recipient].hp;
            for (u32 i = 0; i < ARRAY_COUNT(values); ++i)
            {
                u32 hit = substitute ? values[i] : min(values[i], gBattleMons[candidate].hp);
                u32 amount = hit ? max(1, hit * action->first / action->second) : 0;
                u32 remaining = recipient == candidate && !substitute ? (hp > hit ? hp - hit : 0) : hp;
                amounts[i] = min(amount, remaining);
            }
            result.median = -(s32)amounts[1];
            if (recipient == candidate && !substitute)
            {
                /* Damage dealt and remaining HP move in opposite directions;
                 * a conservative range includes their intermediate peak. */
                result.minimum = values[2] ? -(s32)min(hp, max(1, values[2] * action->first / action->second)) : 0;
                result.maximum = 0;
            }
            else { result.minimum = -(s32)amounts[2]; result.maximum = -(s32)amounts[0]; }
            if (substitute) result.maximum = 0;
        }
    }
    else if (action->kind == RHH_AI_EFFECT_DAMAGE_BATTLER)
    {
        u32 ability = gAiLogicData->abilities[recipient];
        if (!(action->flags & RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD) && ability == ABILITY_MAGIC_GUARD)
            result.flags |= RHH_AI_ESTIMATE_BLOCKED;
        else
        {
            if (!(action->flags & RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD) && ability == ABILITY_NONE)
                result.flags |= RHH_AI_ESTIMATE_UNCERTAIN;
            u32 amount = max(1, GetNonDynamaxMaxHP(recipient) * action->first / action->second);
            u32 hp = gBattleMons[recipient].hp;
            result.minimum = result.median = result.maximum = -(s32)min(amount, hp);
            if (collector->mode == RHH_MOVE_EFFECT_CUSTOM_DAMAGE && recipient == candidate)
            {
                result.flags |= RHH_AI_ESTIMATE_UNCERTAIN | RHH_AI_ESTIMATE_DAMAGE_CACHE;
                if (AiPreviewSubstitute(attacker, candidate, collector->move))
                    result.flags |= RHH_AI_ESTIMATE_SUBSTITUTE;
                else
                {
                    u32 low = min(sAiEvaluation->damage_min, sAiEvaluation->damage_median);
                    u32 mid = sAiEvaluation->damage_median, high = max(sAiEvaluation->damage_max, mid);
                    u32 lowHp = hp > low ? hp - low : 0, midHp = hp > mid ? hp - mid : 0, highHp = hp > high ? hp - high : 0;
                    result.minimum = -(s32)min(amount, lowHp);
                    result.median = -(s32)min(amount, midHp);
                    result.maximum = -(s32)min(amount, highHp);
                }
            }
            if (!result.minimum && !result.median && !result.maximum) result.flags |= RHH_AI_ESTIMATE_BLOCKED;
        }
    }
    else if (action->kind == RHH_AI_EFFECT_HEAL_TARGET || action->kind == RHH_AI_EFFECT_HEAL_USER
             || action->kind == RHH_AI_EFFECT_HEAL_BATTLER)
    {
        if (gBattleMons[attacker].volatiles.healBlockTimer || gBattleMons[recipient].volatiles.healBlockTimer)
            result.flags = RHH_AI_ESTIMATE_BLOCKED;
        else
        {
            u32 amount = max(1, GetNonDynamaxMaxHP(recipient) * action->first / action->second);
            u32 hp = gBattleMons[recipient].hp;
            result.minimum = result.median = result.maximum = AiHealingAmountAt(recipient, amount, hp);
            if ((action->kind == RHH_AI_EFFECT_HEAL_TARGET || action->kind == RHH_AI_EFFECT_HEAL_BATTLER)
                && collector->mode == RHH_MOVE_EFFECT_CUSTOM_DAMAGE
                && recipient == candidate)
            {
                result.flags |= RHH_AI_ESTIMATE_UNCERTAIN | RHH_AI_ESTIMATE_DAMAGE_CACHE;
                if (AiPreviewSubstitute(attacker, candidate, collector->move)) result.flags |= RHH_AI_ESTIMATE_SUBSTITUTE;
                else
                {
                    u32 low = min(sAiEvaluation->damage_min, sAiEvaluation->damage_median);
                    u32 mid = sAiEvaluation->damage_median, high = max(sAiEvaluation->damage_max, mid);
                    u32 lowHp = hp > low ? hp - low : 0, midHp = hp > mid ? hp - mid : 0, highHp = hp > high ? hp - high : 0;
                    u32 lowHeal = lowHp ? AiHealingAmountAt(recipient, amount, lowHp) : 0;
                    u32 highHeal = highHp ? AiHealingAmountAt(recipient, amount, highHp) : 0;
                    result.median = midHp ? AiHealingAmountAt(recipient, amount, midHp) : 0;
                    result.minimum = min(lowHeal, highHeal);
                    result.maximum = max(result.median, max(lowHeal, highHeal));
                    if (low < hp && high >= hp - 1) result.maximum = max(result.maximum, AiHealingAmountAt(recipient, amount, 1));
                }
            }
            if (!result.minimum && !result.median && !result.maximum) result.flags |= RHH_AI_ESTIMATE_BLOCKED;
        }
    }
    else if (action->kind == RHH_AI_EFFECT_HEAL_USER_FROM_DAMAGE || action->kind == RHH_AI_EFFECT_DRAIN_USER)
    {
        result.flags = RHH_AI_ESTIMATE_UNCERTAIN | RHH_AI_ESTIMATE_DAMAGE_CACHE;
        if (gBattleMons[attacker].volatiles.healBlockTimer) result.flags |= RHH_AI_ESTIMATE_BLOCKED;
        else
        {
            bool32 substitute = AiPreviewSubstitute(attacker, candidate, collector->move);
            if (substitute) result.flags |= RHH_AI_ESTIMATE_SUBSTITUTE;
            if (substitute && !(action->flags & RHH_EFFECT_HEAL_INCLUDE_SUBSTITUTE))
                result.flags |= RHH_AI_ESTIMATE_BLOCKED;
            else
            {
                u32 values[3] = {sAiEvaluation->damage_min, sAiEvaluation->damage_median, sAiEvaluation->damage_max};
                values[0] = min(values[0], values[1]); values[2] = max(values[2], values[1]);
                bool32 ooze = action->kind == RHH_AI_EFFECT_DRAIN_USER
                    && gAiLogicData->abilities[candidate] == ABILITY_LIQUID_OOZE;
                bool32 guard = gAiLogicData->abilities[attacker] == ABILITY_MAGIC_GUARD;
                for (u32 i = 0; i < ARRAY_COUNT(values); ++i)
                {
                    if (!substitute) values[i] = min(values[i], gBattleMons[candidate].hp);
                    u32 amount = values[i] ? max(1, values[i] * action->first / action->second) : 0;
                    if (action->kind == RHH_AI_EFFECT_DRAIN_USER && amount
                        && gAiLogicData->holdEffects[attacker] == HOLD_EFFECT_BIG_ROOT)
                        amount = max(1, (values[i] * action->first / action->second) * 1300 / 1000);
                    values[i] = ooze ? (guard ? 0 : min(amount, gBattleMons[attacker].hp))
                                     : AiHealingAmountAt(attacker, amount, gBattleMons[attacker].hp);
                }
                if (ooze && !guard)
                { result.minimum = -(s32)values[2]; result.median = -(s32)values[1]; result.maximum = -(s32)values[0]; }
                else
                { result.minimum = values[0]; result.median = values[1]; result.maximum = values[2]; }
                if (substitute) { result.minimum = min(result.minimum, 0); result.maximum = max(result.maximum, 0); }
                if (!result.minimum && !result.median && !result.maximum
                    && (sAiEvaluation->damage_min || sAiEvaluation->damage_median || sAiEvaluation->damage_max))
                    result.flags |= RHH_AI_ESTIMATE_BLOCKED;
            }
        }
    }
    else if (action->kind == RHH_AI_EFFECT_STATUS_TARGET || action->kind == RHH_AI_EFFECT_STATUS_BATTLER)
    {
        bool32 confused = action->first == RHH_EFFECT_CONFUSION;
        bool32 already = confused ? gBattleMons[recipient].volatiles.confusionTimer != 0
                                  : (gBattleMons[recipient].status1 & STATUS1_ANY) != 0;
        result.flags = RHH_AI_ESTIMATE_UNCERTAIN;
        if (already || AiPreviewSubstitute(attacker, recipient, collector->move)
            || AiKnownStatusBlocked(attacker, recipient, action->first)) result.flags |= RHH_AI_ESTIMATE_BLOCKED;
        else { result.median = result.maximum = 1; }
    }
    else if (action->kind == RHH_AI_EFFECT_STAT_TARGET || action->kind == RHH_AI_EFFECT_STAT_BATTLER)
    {
        s32 stages = action->stages;
        u32 ability = gAiLogicData->abilities[recipient];
        if (ability == ABILITY_CONTRARY) stages = -stages;
        else if (ability == ABILITY_SIMPLE) stages *= 2;
        s32 current = gBattleMons[recipient].statStages[action->first];
        stages = max(-current, min(stages, 12 - current));
        result.flags = RHH_AI_ESTIMATE_UNCERTAIN;
        bool32 enemy = GetBattlerSide(attacker) != GetBattlerSide(recipient);
        bool32 knownBlock = stages < 0 && enemy
            && (gAiLogicData->holdEffects[recipient] == HOLD_EFFECT_CLEAR_AMULET
                || (ability == ABILITY_FULL_METAL_BODY)
                || ((ability == ABILITY_CLEAR_BODY || ability == ABILITY_WHITE_SMOKE) && !AiKnownAbilityIgnored(attacker, recipient)));
        if (!stages || knownBlock || AiPreviewSubstitute(attacker, recipient, collector->move)) result.flags |= RHH_AI_ESTIMATE_BLOCKED;
        else result.median = result.minimum = result.maximum = stages;
    }
    else return -1;
    if ((action->kind == RHH_AI_EFFECT_STATUS_TARGET || action->kind == RHH_AI_EFFECT_STATUS_BATTLER || action->kind == RHH_AI_EFFECT_STAT_TARGET
         || action->kind == RHH_AI_EFFECT_STAT_BATTLER)
        && collector->mode == RHH_MOVE_EFFECT_CUSTOM_DAMAGE && recipient == candidate
        && !AiPreviewSubstitute(attacker, candidate, collector->move))
    {
        u32 hp = gBattleMons[candidate].hp, median = sAiEvaluation->damage_median;
        u32 minimum = min(sAiEvaluation->damage_min, median);
        result.flags |= RHH_AI_ESTIMATE_UNCERTAIN | RHH_AI_ESTIMATE_DAMAGE_CACHE;
        if (hp && minimum >= hp) { result.minimum = result.median = result.maximum = 0; result.flags |= RHH_AI_ESTIMATE_BLOCKED; }
        else if (hp && median >= hp)
        { result.minimum = min(result.minimum, 0); result.maximum = max(result.maximum, 0); result.median = 0; }
    }
    *out = result; return 0;
}
static int AiEffectEstimate(u32 index, u32 recipient, struct RhhAiEffectEstimate *out)
{
    if (!out || out->struct_size < sizeof(*out) || RhhModsCurrentEvent() != RHH_AI_MOVE_SCORE) return -1;
    struct RhhAiEffectEstimate result;
    if (EstimateAiEffect(sAiEffects, index, recipient, &result)) return -1;
    *out = result; return 0;
}
static int EffectContext(struct RhhMoveEffectApplyContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sEffectContext
        || !IsEffectApplyEvent(RhhModsCurrentEvent())) return -1;
    *out = *sEffectContext; return 0;
}
static int EffectDamageContext(struct RhhMoveDamageEffectContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sDamageContext
        || !IsEffectApplyEvent(RhhModsCurrentEvent())) return -1;
    *out = *sDamageContext; return 0;
}
static int EffectMessageGlyph(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return CHAR_A + c - 'A';
    if (c >= 'a' && c <= 'z') return CHAR_a + c - 'a';
    if (c >= '0' && c <= '9') return CHAR_0 + c - '0';
    switch (c) {
    case ' ': return CHAR_SPACE; case '!': return CHAR_EXCL_MARK; case '?': return CHAR_QUESTION_MARK;
    case '.': return CHAR_PERIOD; case '-': return CHAR_HYPHEN; case ':': return CHAR_COLON;
    case ',': return CHAR_COMMA; case '/': return CHAR_SLASH; case '+': return CHAR_PLUS;
    case '(': return CHAR_LEFT_PAREN; case ')': return CHAR_RIGHT_PAREN; case '%': return CHAR_PERCENT;
    case '\'': return CHAR_SGL_QUOTE_RIGHT; case '"': return CHAR_DBL_QUOTE_RIGHT;
    case '&': return CHAR_AMPERSAND; case ';': return CHAR_SEMICOLON; case '=': return CHAR_EQUALS;
    case '\n': return CHAR_NEWLINE; default: return -1;
    }
}
static int QueueBattleMessage(const char *text, u32 limit)
{
    if (!text || sEffectMessageCount >= limit) return -1;
    u8 message[EFFECT_MESSAGE_LENGTH + 1]; u32 length = 0, newlines = 0, wordWidth = 0, lineWidth[2] = {0};
    while (text[length])
    {
        if (length == EFFECT_MESSAGE_LENGTH) return -1;
        int glyph = EffectMessageGlyph((unsigned char)text[length]);
        if (glyph < 0 || (glyph == CHAR_NEWLINE && ++newlines > 1)) return -1;
        if ((glyph == CHAR_SPACE || glyph == CHAR_NEWLINE)
            && (!length || message[length - 1] == CHAR_SPACE || message[length - 1] == CHAR_NEWLINE)) return -1;
        if (glyph == CHAR_SPACE || glyph == CHAR_NEWLINE) wordWidth = 0;
        else if ((wordWidth += GetGlyphWidth(glyph, FALSE, FONT_NORMAL) + 1) > 200) return -1;
        if (glyph != CHAR_NEWLINE) lineWidth[newlines] += GetGlyphWidth(glyph, FALSE, FONT_NORMAL) + 1;
        message[length++] = glyph;
    }
    if (!length || message[length - 1] == CHAR_SPACE || message[length - 1] == CHAR_NEWLINE
        || (newlines && (lineWidth[0] > 200 || lineWidth[1] > 200))) return -1;
    message[length] = EOS;
    memcpy(sEffectMessages[sEffectMessageCount++], message, length + 1); return 0;
}
static int EffectMessage(const char *text)
{
    if (!IsEffectApplyEvent(RhhModsCurrentEvent()) || RhhModsCurrentStage() != RHH_OBSERVE
        || !sEffectContext) return -1;
    return QueueBattleMessage(text, EFFECT_MESSAGE_LIMIT);
}
int RhhModsInnateEntryMessage(const char *text)
{
    if (RhhModsCurrentEvent() != RHH_INNATE_ENTRY || RhhModsCurrentStage() != RHH_OBSERVE
        || !sInnateEntryContext) return -1;
    return QueueBattleMessage(text, EFFECT_MESSAGE_LIMIT);
}
int RhhModsInnateTurnMessage(const char *text)
{
    if (RhhModsCurrentEvent() != RHH_INNATE_TURN_END || RhhModsCurrentStage() != RHH_OBSERVE
        || !sInnateTurnContext) return -1;
    return QueueBattleMessage(text, EXPIRY_MESSAGE_LIMIT);
}
static int QueueInnateTurnActionFromContext(u32 battler, u32 kind, u32 first, u32 second)
{
    if (!sInnateTurnContext || sInnateTurnActionCount >= ARRAY_COUNT(sInnateTurnActions)
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || !IsBattlerAlive(battler)) return -1;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle) || battle.battle_id != sInnateTurnContext->battle_id || battle.turn != sInnateTurnContext->turn) return -1;
    const struct RhhBattleParticipant *p = NULL, *source = &sInnateTurnContext->participant;
    bool32 sourceMatches = FALSE;
    for (u32 i = 0; i < battle.participant_count; ++i)
    {
        const struct RhhBattleParticipant *candidate = &battle.participants[i];
        if (candidate->battler == battler && !candidate->fainted) p = candidate;
        if (candidate->battler == source->battler && candidate->identity == source->identity
            && candidate->owner == source->owner && candidate->party_slot == source->party_slot && !candidate->fainted)
            sourceMatches = TRUE;
    }
    if (!p || !sourceMatches) return -1;
    sInnateTurnActions[sInnateTurnActionCount++] = (struct InnateTurnAction){
        .identity = p->identity, .battler = p->battler, .owner = p->owner, .partySlot = p->party_slot,
        .sourceIdentity = source->identity, .sourceBattler = source->battler, .sourceOwner = source->owner,
        .sourcePartySlot = source->party_slot,
        .attribution = {.struct_size = sizeof(struct RhhBattleStatusAttribution), .kind = RHH_STATUS_ATTRIBUTION_INNATE_TURN,
            .index = sInnateTurnContext->index, .sequence = sInnateTurnContext->turn_sequence, .ability = sInnateTurnContext->ability},
        .kind = kind, .first = first, .second = second};
    return 0;
}
static int QueueInnateTurnAction(u32 battler, u32 kind, u32 first, u32 second)
{
    if (RhhModsCurrentEvent() != RHH_INNATE_TURN_END || RhhModsCurrentStage() != RHH_OBSERVE) return -1;
    return QueueInnateTurnActionFromContext(battler, kind, first, second);
}
static bool32 ValidInnateWeather(u32 weather)
{ return weather >= RHH_WEATHER_RAIN && weather <= RHH_WEATHER_FOG; }
static bool32 ValidInnateTerrain(u32 terrain)
{ return terrain >= RHH_TERRAIN_GRASSY && terrain <= RHH_TERRAIN_PSYCHIC; }
int RhhModsInnateTurnWeather(u32 weather)
{
    if (!ValidInnateWeather(weather)) return -1;
    return QueueInnateTurnAction(sInnateTurnContext ? sInnateTurnContext->battler : RHH_BATTLE_NO_BATTLER,
                                 MOD_INNATE_TURN_WEATHER, weather, 0);
}
int RhhModsInnateTurnTerrain(u32 terrain)
{
    if (!ValidInnateTerrain(terrain)) return -1;
    return QueueInnateTurnAction(sInnateTurnContext ? sInnateTurnContext->battler : RHH_BATTLE_NO_BATTLER,
                                 MOD_INNATE_TURN_TERRAIN, terrain, 0);
}
int RhhModsInnateTurnHeal(u32 numerator, u32 denominator)
{
    if (!numerator || numerator > denominator || denominator > 16) return -1;
    return QueueInnateTurnAction(sInnateTurnContext ? sInnateTurnContext->battler : RHH_BATTLE_NO_BATTLER,
                                 MOD_INNATE_TURN_HEAL, numerator, denominator);
}
int RhhModsInnateTurnStat(u32 stat, s32 stages)
{
    if (stat < RHH_STAT_ATTACK || stat >= RHH_BATTLE_STAT_COUNT || !stages || stages < -6 || stages > 6) return -1;
    return QueueInnateTurnAction(sInnateTurnContext ? sInnateTurnContext->battler : RHH_BATTLE_NO_BATTLER,
                                 MOD_INNATE_TURN_STAT, stat, stages + 6);
}
int RhhModsInnateTurnHealTarget(u32 battler, u32 numerator, u32 denominator)
{
    if (!numerator || numerator > denominator || denominator > 16) return -1;
    return QueueInnateTurnAction(battler, MOD_INNATE_TURN_HEAL, numerator, denominator);
}
int RhhModsInnateTurnStatTarget(u32 battler, u32 stat, s32 stages)
{
    if (stat < RHH_STAT_ATTACK || stat >= RHH_BATTLE_STAT_COUNT || !stages || stages < -6 || stages > 6) return -1;
    return QueueInnateTurnAction(battler, MOD_INNATE_TURN_STAT, stat, stages + 6);
}
int RhhModsInnateTurnStatFrom(u32 battler, u32 stat, s32 stages)
{
    if (stat < RHH_STAT_ATTACK || stat >= RHH_BATTLE_STAT_COUNT || !stages || stages < -6 || stages > 6) return -1;
    return QueueInnateTurnAction(battler, MOD_INNATE_TURN_STAT_SOURCE, stat, stages + 6);
}
int RhhModsInnateTurnStatusTarget(u32 battler, u32 status)
{
    if (status < RHH_EFFECT_SLEEP || status > RHH_EFFECT_TOXIC || !sStatusEffects[status]) return -1;
    return QueueInnateTurnAction(battler, MOD_INNATE_TURN_STATUS, status, 0);
}
static int InnateEntryTargetFromContext(u32 battler, struct InnateEntryTarget *out)
{
    if (!sInnateEntryContext || !out || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || !IsBattlerAlive(battler)) return -1;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle) || battle.battle_id != sInnateEntryContext->battle_id) return -1;
    const struct RhhBattleParticipant *target = NULL, *source = &sInnateEntryContext->participant;
    bool32 sourceMatches = FALSE;
    for (u32 i = 0; i < battle.participant_count; ++i)
    {
        const struct RhhBattleParticipant *p = &battle.participants[i];
        if (p->battler == battler && !p->fainted) target = p;
        if (p->battler == source->battler && p->identity == source->identity && p->owner == source->owner
            && p->party_slot == source->party_slot && !p->fainted) sourceMatches = TRUE;
    }
    if (!target || !sourceMatches) return -1;
    *out = (struct InnateEntryTarget){target->identity, target->battler, target->owner, target->party_slot};
    return 0;
}
static int InnateEntryTarget(u32 battler, struct InnateEntryTarget *out)
{
    if (RhhModsCurrentEvent() != RHH_INNATE_ENTRY || RhhModsCurrentStage() != RHH_OBSERVE) return -1;
    return InnateEntryTargetFromContext(battler, out);
}
static int QueueInnateEntryEnvironment(u32 kind, u32 value)
{
    struct InnateEntryTarget target;
    if (!sInnateEntryContext || sInnateEntryEnvironmentCount >= ARRAY_COUNT(sInnateEntryEnvironment)
        || InnateEntryTarget(sInnateEntryContext->battler, &target)) return -1;
    u32 index = sInnateEntryEnvironmentCount++;
    sInnateEntryEnvironment[index].ability = sInnateEntryContext->ability;
    sInnateEntryEnvironment[index].kind = kind; sInnateEntryEnvironment[index].value = value;
    return 0;
}
int RhhModsInnateEntryWeather(u32 weather)
{ return ValidInnateWeather(weather) ? QueueInnateEntryEnvironment(MOD_INNATE_ENV_WEATHER, weather) : -1; }
int RhhModsInnateEntryTerrain(u32 terrain)
{ return ValidInnateTerrain(terrain) ? QueueInnateEntryEnvironment(MOD_INNATE_ENV_TERRAIN, terrain) : -1; }
int RhhModsInnateEntryHealTarget(u32 battler, u32 numerator, u32 denominator)
{
    struct InnateEntryTarget target;
    if (!numerator || numerator > denominator || denominator > 16 || sInnateHealCount >= ARRAY_COUNT(sInnateHeals)
        || InnateEntryTarget(battler, &target)) return -1;
    sInnateHeals[sInnateHealCount].target = target;
    sInnateHeals[sInnateHealCount].numerator = numerator;
    sInnateHeals[sInnateHealCount++].denominator = denominator;
    return 0;
}
int RhhModsInnateEntryStatTarget(u32 battler, u32 stat, s32 stages)
{
    struct InnateEntryTarget target;
    if (stat < RHH_STAT_ATTACK || stat >= RHH_BATTLE_STAT_COUNT || stages < -6 || stages > 6 || !stages
        || sInnateStatCount >= ARRAY_COUNT(sInnateStats) || InnateEntryTarget(battler, &target)) return -1;
    sInnateStats[sInnateStatCount].target = target;
    sInnateStats[sInnateStatCount].stat = stat;
    sInnateStats[sInnateStatCount].sourced = FALSE;
    sInnateStats[sInnateStatCount++].stages = stages;
    return 0;
}
int RhhModsInnateEntryStatFrom(u32 battler, u32 stat, s32 stages)
{
    if (RhhModsInnateEntryStatTarget(battler, stat, stages)) return -1;
    sInnateStats[sInnateStatCount - 1].sourced = TRUE;
    return 0;
}
int RhhModsInnateEntryStatusTarget(u32 battler, u32 status)
{
    struct InnateEntryTarget target;
    if (status < RHH_EFFECT_SLEEP || status > RHH_EFFECT_TOXIC || !sStatusEffects[status]
        || sInnateEntryStatusCount >= ARRAY_COUNT(sInnateEntryStatuses) || InnateEntryTarget(battler, &target)) return -1;
    sInnateEntryStatuses[sInnateEntryStatusCount].target = target;
    sInnateEntryStatuses[sInnateEntryStatusCount].attribution = (struct RhhBattleStatusAttribution){
        .struct_size = sizeof(struct RhhBattleStatusAttribution), .kind = RHH_STATUS_ATTRIBUTION_INNATE_ENTRY,
        .index = sInnateEntryContext->index, .sequence = sInnateEntryContext->entry_sequence, .ability = sInnateEntryContext->ability};
    sInnateEntryStatuses[sInnateEntryStatusCount++].status = status;
    return 0;
}
int RhhModsInnateEntryHeal(u32 numerator, u32 denominator)
{ return RhhModsInnateEntryHealTarget(sInnateEntryContext ? sInnateEntryContext->battler : RHH_BATTLE_NO_BATTLER, numerator, denominator); }
int RhhModsInnateEntryStat(u32 stat, s32 stages)
{ return RhhModsInnateEntryStatTarget(sInnateEntryContext ? sInnateEntryContext->battler : RHH_BATTLE_NO_BATTLER, stat, stages); }
static void RestoreInnateScriptingBattler(void)
{
    if (sInnateMoldBreakerSaved)
    {
        if (gBattleStruct == sInnateSavedBattleStruct && gBattleStruct)
            gBattleStruct->moldBreakerActive = sInnateSavedMoldBreaker;
        sInnateMoldBreakerSaved = FALSE; sInnateSavedBattleStruct = NULL;
    }
    if (sInnateScriptingBattlerSaved)
    {
        gBattleScripting.battler = sInnateSavedScriptingBattler;
        sInnateScriptingBattlerSaved = FALSE;
    }
}
static void SetInnateSourcedStatContext(void)
{
    sInnateSavedBattleStruct = gBattleStruct;
    sInnateSavedMoldBreaker = gBattleStruct->moldBreakerActive;
    sInnateMoldBreakerSaved = TRUE;
    gBattleStruct->moldBreakerActive = FALSE;
}
static bool32 InnateEntryStillValid(void)
{
    if (!ConditionsAvailable() || sInnateEntryBattler >= gBattlersCount || (sPending & (1u << sInnateEntryBattler))
        || !IsBattlerAlive(sInnateEntryBattler)) return FALSE;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return FALSE;
    for (u32 i = 0; i < battle.participant_count; ++i)
    {
        const struct RhhBattleParticipant *participant = &battle.participants[i];
        if (participant->battler == sInnateEntryBattler)
            return participant->identity == sInnateHealEntry.identity
                && participant->owner == sInnateHealEntry.owner && participant->party_slot == sInnateHealEntry.party_slot;
    }
    return FALSE;
}
static bool32 InnateEntryTargetStillValid(const struct InnateEntryTarget *target)
{
    if (!InnateEntryStillValid() || target->battler >= gBattlersCount || (sPending & (1u << target->battler))
        || !IsBattlerAlive(target->battler)) return FALSE;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return FALSE;
    for (u32 i = 0; i < battle.participant_count; ++i)
    {
        const struct RhhBattleParticipant *p = &battle.participants[i];
        if (p->battler == target->battler)
            return p->identity == target->identity && p->owner == target->owner && p->party_slot == target->partySlot;
    }
    return FALSE;
}
static void SetInnateScriptingBattler(void)
{
    sInnateSavedScriptingBattler = gBattleScripting.battler;
    sInnateScriptingBattlerSaved = TRUE;
    gBattleScripting.battler = sInnateHealBattler;
}
static bool32 InnateEnvironmentSourceUsable(u32 battler, RhhContentKey ability)
{
    struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    int result = ResolveInnates(battler, NULL, &resolution);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (result || !pure) return FALSE;
    for (u32 i = 0; i < resolution.count; ++i)
        if (resolution.entries[i].ability == ability) return !resolution.entries[i].status;
    return FALSE;
}
static bool32 ApplyInnateEnvironment(u32 battler, u32 kind, u32 value, RhhContentKey ability)
{
    if (!gBattleResources || !gBattleResources->battleScriptsStack
        || gBattleResources->battleScriptsStack->size >= ARRAY_COUNT(gBattleResources->battleScriptsStack->ptr)) return FALSE;
    if (kind == MOD_INNATE_ENV_WEATHER) {
        static const u8 weather[] = {BATTLE_WEATHER_NONE, BATTLE_WEATHER_RAIN, BATTLE_WEATHER_SUN,
            BATTLE_WEATHER_SANDSTORM, BATTLE_WEATHER_HAIL, BATTLE_WEATHER_SNOW, BATTLE_WEATHER_FOG};
        return ValidInnateWeather(value) && TryChangeBattleWeatherFromInnate(battler, weather[value], ability) == WEATHER_FAILURE_SUCCESS;
    }
    if (kind == MOD_INNATE_ENV_TERRAIN) {
        static const u8 terrain[] = {B_TERRAIN_NONE, B_TERRAIN_GRASSY, B_TERRAIN_MISTY, B_TERRAIN_ELECTRIC, B_TERRAIN_PSYCHIC};
        return ValidInnateTerrain(value) && TryChangeBattleTerrainFromInnate(battler, terrain[value], ability);
    }
    return FALSE;
}
enum ModInnateEnvironmentAction ModsInnateNextEnvironment(void)
{
    RestoreInnateScriptingBattler();
    while (sInnateEntryEnvironmentNext < sInnateEntryEnvironmentCount) {
        u32 i = sInnateEntryEnvironmentNext++;
        if (!InnateEntryStillValid() || !InnateEnvironmentSourceUsable(sInnateEntryBattler,
            sInnateEntryEnvironment[i].ability)) continue;
        sInnateHealBattler = sInnateEntryBattler; SetInnateScriptingBattler();
        if (ApplyInnateEnvironment(sInnateEntryBattler, sInnateEntryEnvironment[i].kind,
            sInnateEntryEnvironment[i].value, sInnateEntryEnvironment[i].ability)) return sInnateEntryEnvironment[i].kind;
        RestoreInnateScriptingBattler();
    }
    return MOD_INNATE_ENV_DONE;
}
bool32 ModsInnateNextHeal(void)
{
    RestoreInnateScriptingBattler();
    while (sInnateHealNext < sInnateHealCount)
    {
        u32 index = sInnateHealNext++;
        u32 battler = sInnateHeals[index].target.battler;
        if (!InnateEntryTargetStillValid(&sInnateHeals[index].target) || gBattleMons[battler].volatiles.healBlockTimer
            || gBattleMons[battler].hp >= gBattleMons[battler].maxHP) continue;
        sInnateHealBattler = battler;
        SetHealAmount(sInnateHealBattler, GetNonDynamaxMaxHP(sInnateHealBattler)
                      * sInnateHeals[index].numerator / sInnateHeals[index].denominator);
        SetInnateScriptingBattler();
        return TRUE;
    }
    return FALSE;
}
bool32 ModsInnateNextStat(void)
{
    RestoreInnateScriptingBattler();
    sInnateStatIsSourced = FALSE;
    while (sInnateStatNext < sInnateStatCount)
    {
        u32 index = sInnateStatNext++;
        if (!InnateEntryTargetStillValid(&sInnateStats[index].target) || gBattleStruct->statChangeBattler) continue;
        bool32 busy = FALSE;
        for (u32 i = 0; i < gBattlersCount; ++i)
            if (gSpecialStatuses[i].statStageAmount || gSpecialStatuses[i].statStageAmount2) busy = TRUE;
        if (busy) continue;
        sInnateHealBattler = sInnateStats[index].target.battler;
        SetStatChange(sInnateHealBattler, sInnateStats[index].stat, sInnateStats[index].stages);
        sInnateStatIsSourced = sInnateStats[index].sourced;
        sInnateStatSourceBattler = sInnateEntryBattler;
        if (sInnateStatIsSourced) SetInnateSourcedStatContext();
        SetInnateScriptingBattler();
        return TRUE;
    }
    return FALSE;
}
bool32 ModsInnateNextAnimation(void)
{
    RestoreInnateScriptingBattler();
    while (sInnateEntryAnimationNext < sInnateEntryAnimationCount) {
        u32 index = sInnateEntryAnimationNext++;
        if (!InnateEntryTargetStillValid(&sInnateEntryAnimations[index].target)) continue;
        sInnateHealBattler = sInnateEntryAnimations[index].target.battler;
        gModEffectAnimation = sInnateEntryAnimations[index].animation;
        SetInnateScriptingBattler(); return TRUE;
    }
    return FALSE;
}
bool32 ModsInnateNextStatus(u32 *status, u32 *source)
{
    RestoreInnateScriptingBattler();
    if (!status || !source) return FALSE;
    while (sInnateEntryStatusNext < sInnateEntryStatusCount)
    {
        u32 index = sInnateEntryStatusNext++;
        if (!InnateEntryTargetStillValid(&sInnateEntryStatuses[index].target)) continue;
        *source = sInnateEntryBattler; *status = sStatusEffects[sInnateEntryStatuses[index].status];
        sPendingInnateStatus = sInnateEntryStatuses[index].attribution;
        sPendingInnateStatusSource = *source; sPendingInnateStatusTarget = sInnateEntryStatuses[index].target.battler;
        sInnateHealBattler = sInnateEntryStatuses[index].target.battler;
        SetInnateSourcedStatContext(); SetInnateScriptingBattler(); return TRUE;
    }
    return FALSE;
}
void ModsInnateStatusFinished(void) { sPendingInnateStatus = (struct RhhBattleStatusAttribution){0}; }
bool32 ModsInnateStatSourced(void) { return sInnateStatIsSourced; }
u32 ModsInnateStatSource(void) { return sInnateStatSourceBattler; }
bool32 ModsInnateTurnActionsPending(void) { return sInnateTurnPending; }
enum ModInnateTurnAction ModsInnateTurnNextAction(u32 *status, u32 *source)
{
    RestoreInnateScriptingBattler();
    while (sInnateTurnPending && sInnateTurnActionNext < sInnateTurnActionCount)
    {
        const struct InnateTurnAction *action = &sInnateTurnActions[sInnateTurnActionNext++];
        u32 battler = action->battler;
        if (!ConditionsAvailable() || battler >= gBattlersCount || action->sourceBattler >= gBattlersCount
            || (sPending & ((1u << battler) | (1u << action->sourceBattler)))
            || !IsBattlerAlive(battler) || !IsBattlerAlive(action->sourceBattler)) continue;
        struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
        if (Current(&battle) || battle.battle_id != sInnateTurnCompletion.battle.battle_id
            || battle.turn != sInnateTurnCompletion.battle.turn) continue;
        bool32 matches = FALSE, sourceMatches = FALSE;
        for (u32 i = 0; i < battle.participant_count; ++i)
        {
            const struct RhhBattleParticipant *p = &battle.participants[i];
            if (p->battler == battler && p->identity == action->identity
                && p->owner == action->owner && p->party_slot == action->partySlot) matches = TRUE;
            if (p->battler == action->sourceBattler && p->identity == action->sourceIdentity
                && p->owner == action->sourceOwner && p->party_slot == action->sourcePartySlot) sourceMatches = TRUE;
        }
        if (!matches || !sourceMatches) continue;
        if (action->kind == MOD_INNATE_TURN_WEATHER || action->kind == MOD_INNATE_TURN_TERRAIN)
        {
            if (!InnateEnvironmentSourceUsable(action->sourceBattler, action->attribution.ability)) continue;
            sInnateHealBattler = action->sourceBattler; SetInnateScriptingBattler();
            u32 kind = action->kind == MOD_INNATE_TURN_WEATHER ? MOD_INNATE_ENV_WEATHER : MOD_INNATE_ENV_TERRAIN;
            if (ApplyInnateEnvironment(action->sourceBattler, kind, action->first, action->attribution.ability)) return action->kind;
            RestoreInnateScriptingBattler(); continue;
        }
        if (action->kind == MOD_INNATE_TURN_ANIMATION)
        {
            gModEffectAnimation = action->first; sInnateHealBattler = battler;
            SetInnateScriptingBattler(); return MOD_INNATE_TURN_ANIMATION;
        }
        if (action->kind == MOD_INNATE_TURN_STATUS)
        {
            if (!status || !source) continue;
            *status = sStatusEffects[action->first]; *source = action->sourceBattler;
            sPendingInnateStatus = action->attribution;
            sPendingInnateStatusSource = *source; sPendingInnateStatusTarget = battler;
            sInnateHealBattler = battler; SetInnateSourcedStatContext(); SetInnateScriptingBattler();
            return MOD_INNATE_TURN_STATUS;
        }
        if (action->kind == MOD_INNATE_TURN_HEAL)
        {
            if (gBattleMons[battler].volatiles.healBlockTimer
                || gBattleMons[battler].hp >= gBattleMons[battler].maxHP) continue;
            SetHealAmount(battler, GetNonDynamaxMaxHP(battler) * action->first / action->second);
        }
        else if (action->kind == MOD_INNATE_TURN_STAT || action->kind == MOD_INNATE_TURN_STAT_SOURCE)
        {
            bool32 busy = gBattleStruct->statChangeBattler != 0;
            for (u32 i = 0; i < gBattlersCount; ++i)
                if (gSpecialStatuses[i].statStageAmount || gSpecialStatuses[i].statStageAmount2) busy = TRUE;
            if (busy) continue;
            SetStatChange(battler, action->first, (s32)action->second - 6);
            sInnateStatSourceBattler = action->sourceBattler;
            if (action->kind == MOD_INNATE_TURN_STAT_SOURCE) SetInnateSourcedStatContext();
        }
        else continue;
        sInnateHealBattler = battler; SetInnateScriptingBattler();
        return action->kind;
    }
    return MOD_INNATE_TURN_DONE;
}
static bool32 CanQueueAnyEffectAction(void)
{
    return IsEffectApplyEvent(RhhModsCurrentEvent()) && RhhModsCurrentStage() == RHH_OBSERVE
        && sEffectContext && sEffectActionCount < ARRAY_COUNT(sEffectActions);
}
static int EffectQueue(struct RhhMoveEffectQueue *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sEffectContext
        || !IsEffectApplyEvent(RhhModsCurrentEvent()) || RhhModsCurrentStage() != RHH_OBSERVE) return -1;
    *out = (struct RhhMoveEffectQueue){.struct_size = sizeof(*out),
        .action_count = sEffectActionCount, .action_capacity = ARRAY_COUNT(sEffectActions),
        .message_count = sEffectMessageCount, .message_capacity = EFFECT_MESSAGE_LIMIT};
    return 0;
}
static bool32 CanQueueEffectAction(void)
{
    return CanQueueAnyEffectAction() && sEffectContext->move_target != RHH_MOVE_TARGET_FIELD
        && sEffectContext->move_target != RHH_MOVE_TARGET_OPPONENTS_FIELD;
}
static int EffectParticipantAt(u32 battler, struct EffectParticipant *out)
{
    struct RhhBattleSnapshot battle = {0};
    struct RhhBattleParticipant participant;
    if (!out || Participant(battler, &battle, &participant)) return -1;
    *out = (struct EffectParticipant){.identity = participant.identity, .owner = participant.owner,
        .partySlot = participant.party_slot};
    return 0;
}
static bool32 EffectParticipantMatches(u32 battler, const struct EffectParticipant *expected)
{
    struct EffectParticipant actual;
    return !EffectParticipantAt(battler, &actual) && actual.identity == expected->identity
        && actual.owner == expected->owner && actual.partySlot == expected->partySlot;
}
static u32 EffectActionSubject(const struct EffectAction *action)
{
    if (action->kind == MOD_EFFECT_HEAL_USER || action->kind == MOD_EFFECT_HEAL_DAMAGE_USER
        || action->kind == MOD_EFFECT_DRAIN_USER || action->kind == MOD_EFFECT_FIELD_RULE
        || action->kind == MOD_EFFECT_ROOM_TOGGLE || action->kind == MOD_EFFECT_FIELD_MOVE) return action->attacker;
    if (action->kind == MOD_EFFECT_HEAL_BATTLER || action->kind == MOD_EFFECT_DAMAGE_BATTLER || action->kind == MOD_EFFECT_DAMAGE_FROM_HIT
        || action->kind == MOD_EFFECT_ANIMATION || action->kind == MOD_EFFECT_STAT_BATTLER
        || action->kind == MOD_EFFECT_STATUS_BATTLER) return action->recipient;
    return action->target;
}
static int EffectQueueAt(u32 index, struct RhhMoveQueuedAction *out)
{
    static const u8 kinds[] = {
        [MOD_EFFECT_HEAL] = RHH_MOVE_QUEUED_HEAL, [MOD_EFFECT_STATUS] = RHH_MOVE_QUEUED_STATUS,
        [MOD_EFFECT_STAT] = RHH_MOVE_QUEUED_STAT, [MOD_EFFECT_HEAL_USER] = RHH_MOVE_QUEUED_HEAL_USER,
        [MOD_EFFECT_HEAL_DAMAGE_USER] = RHH_MOVE_QUEUED_HEAL_FROM_HIT, [MOD_EFFECT_DRAIN_USER] = RHH_MOVE_QUEUED_DRAIN,
        [MOD_EFFECT_HEAL_BATTLER] = RHH_MOVE_QUEUED_HEAL_BATTLER, [MOD_EFFECT_DAMAGE_BATTLER] = RHH_MOVE_QUEUED_DAMAGE_BATTLER,
        [MOD_EFFECT_ANIMATION] = RHH_MOVE_QUEUED_ANIMATION, [MOD_EFFECT_STAT_BATTLER] = RHH_MOVE_QUEUED_STAT_BATTLER,
        [MOD_EFFECT_STATUS_BATTLER] = RHH_MOVE_QUEUED_STATUS_BATTLER, [MOD_EFFECT_FIELD_RULE] = RHH_MOVE_QUEUED_FIELD_RULE,
        [MOD_EFFECT_ROOM_TOGGLE] = RHH_MOVE_QUEUED_ROOM_TOGGLE, [MOD_EFFECT_FIELD_MOVE] = RHH_MOVE_QUEUED_FIELD_MOVE,
        [MOD_EFFECT_DAMAGE_FROM_HIT] = RHH_MOVE_QUEUED_DAMAGE_FROM_HIT,
    };
    if (!out || out->struct_size < sizeof(*out) || !sEffectContext || index >= sEffectActionCount
        || !IsEffectApplyEvent(RhhModsCurrentEvent()) || RhhModsCurrentStage() != RHH_OBSERVE) return -1;
    const struct EffectAction *action = &sEffectActions[index];
    if (action->kind >= ARRAY_COUNT(kinds) || !kinds[action->kind]) return -1;
    struct RhhMoveQueuedAction result = {.struct_size = sizeof(result), .kind = kinds[action->kind],
        .attacker = action->attacker, .target = action->target, .recipient = EffectActionSubject(action),
        .has_target = action->hasTarget, .move = sEffectContext->move};
    switch (action->kind)
    {
    case MOD_EFFECT_HEAL: case MOD_EFFECT_HEAL_USER: case MOD_EFFECT_HEAL_BATTLER:
        result.first = action->first; result.second = action->second; break;
    case MOD_EFFECT_DAMAGE_BATTLER:
        result.first = action->first; result.second = action->second; result.flags = action->amount; break;
    case MOD_EFFECT_HEAL_DAMAGE_USER: case MOD_EFFECT_DAMAGE_FROM_HIT:
        result.first = action->publicFirst; result.second = action->publicSecond;
        result.flags = action->publicFlags; result.amount = action->amount; break;
    case MOD_EFFECT_DRAIN_USER:
        result.first = action->first; result.second = action->second;
        result.flags = action->publicFlags; result.amount = action->amount; break;
    case MOD_EFFECT_STAT: case MOD_EFFECT_STAT_BATTLER:
        result.first = action->first; result.stages = (s32)action->second - 6; break;
    case MOD_EFFECT_ANIMATION: result.first = action->publicFirst; break;
    case MOD_EFFECT_STATUS: case MOD_EFFECT_STATUS_BATTLER: case MOD_EFFECT_FIELD_RULE:
    case MOD_EFFECT_ROOM_TOGGLE: case MOD_EFFECT_FIELD_MOVE: result.first = action->first; break;
    default: return -1;
    }
    *out = result; return 0;
}
static int QueueEffectAction(struct EffectAction action)
{
    if (!sEffectContext || sEffectActionCount >= ARRAY_COUNT(sEffectActions)) return -1;
    u32 subject = EffectActionSubject(&action);
    action.hasTarget = sEffectContext->move_target != RHH_MOVE_TARGET_FIELD
        && sEffectContext->move_target != RHH_MOVE_TARGET_OPPONENTS_FIELD;
    action.battleId = sEffectContext->battle_id; action.move = gCurrentMove;
    if (EffectParticipantAt(action.attacker, &action.source) || EffectParticipantAt(subject, &action.subject)
        || (action.hasTarget && EffectParticipantAt(action.target, &action.origin))) return -1;
    sEffectActions[sEffectActionCount++] = action; return 0;
}
static int BattleFieldQueueMoveEffect(u32 rule)
{
    if (!CanQueueAnyEffectAction() || (rule != RHH_BATTLE_FIELD_GRAVITY && rule != RHH_BATTLE_FIELD_ION_DELUGE)) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_FIELD_RULE,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .first = rule});
}
static int BattleFieldQueueRoomToggle(u32 rule)
{
    if (!CanQueueAnyEffectAction() || !IsRoomRule(rule)) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_ROOM_TOGGLE,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .first = rule});
}
static int BattleFieldQueueFieldMove(u32 rule)
{
    if (!CanQueueAnyEffectAction() || !IsFieldMoveRule(rule)) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_FIELD_MOVE,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .first = rule});
}
static int EffectHeal(u32 numerator, u32 denominator)
{
    if (!CanQueueEffectAction() || !numerator || numerator > denominator || denominator > 16) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_HEAL,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .first = numerator, .second = denominator});
}
static int EffectHealUser(u32 numerator, u32 denominator)
{
    if (!CanQueueAnyEffectAction() || !numerator || numerator > denominator || denominator > 16) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_HEAL_USER,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .first = numerator, .second = denominator});
}
static int EffectHealBattler(u32 battler, u32 numerator, u32 denominator)
{
    if (!CanQueueAnyEffectAction() || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || !IsBattlerAlive(battler) || !numerator || numerator > denominator || denominator > 16) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_HEAL_BATTLER,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .recipient = battler,
        .first = numerator, .second = denominator});
}
static int EffectDamageBattler(u32 battler, u32 numerator, u32 denominator, u32 flags)
{
    if (!CanQueueAnyEffectAction() || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || !IsBattlerAlive(battler) || !numerator || numerator > denominator || denominator > 16
        || (flags & ~RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD)) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_DAMAGE_BATTLER,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .recipient = battler,
        .first = numerator, .second = denominator, .amount = flags});
}
static int EffectDamageFromHit(u32 battler, u32 numerator, u32 denominator, u32 flags)
{
    if (!CanQueueAnyEffectAction() || !sDamageContext || !(sDamageContext->flags & RHH_MOVE_DAMAGE_RECORDED)
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || !IsBattlerAlive(battler)
        || !numerator || numerator > denominator || denominator > 16
        || (flags & ~(RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD | RHH_EFFECT_DAMAGE_FROM_HIT_INCLUDE_SUBSTITUTE))) return -1;
    u32 damage = sDamageContext->hp_lost;
    if (flags & RHH_EFFECT_DAMAGE_FROM_HIT_INCLUDE_SUBSTITUTE) damage += sDamageContext->substitute_lost;
    u32 amount = damage ? max(1, damage * numerator / denominator) : 0;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_DAMAGE_FROM_HIT,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .recipient = battler,
        .first = flags, .amount = amount, .publicFirst = numerator, .publicSecond = denominator, .publicFlags = flags});
}
static u32 EffectAnimationId(u32 animation)
{
    static const u8 anims[] = {0, B_ANIM_SIMPLE_HEAL, B_ANIM_PROTECTED_ITSELF,
        B_ANIM_TOTEM_FLARE, B_ANIM_RAINBOW, B_ANIM_SWAMP, B_ANIM_TAILWIND};
    return animation && animation < ARRAY_COUNT(anims) ? anims[animation] : 0;
}
static int QueueEffectAnimation(u32 battler, u32 animation)
{
    u32 anim = EffectAnimationId(animation);
    if (!sEffectContext || sEffectActionCount >= ARRAY_COUNT(sEffectActions)
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || !IsBattlerAlive(battler) || !anim) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_ANIMATION,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target,
        .recipient = battler, .first = anim, .publicFirst = animation});
}
static int EffectAnimation(u32 battler, u32 animation)
{ return CanQueueAnyEffectAction() ? QueueEffectAnimation(battler, animation) : -1; }
static int QueueStatusReaction(u32 source, u32 battler, u32 kind, u32 first, u32 second, u32 flags)
{
    u32 event = RhhModsCurrentEvent();
    struct StatusReactionFrame *frame = sStatusReactionDispatch;
    if (!frame || RhhModsCurrentStage() != RHH_OBSERVE
        || !((event == RHH_BATTLER_STATUS_APPLIED && sStatusApplicationContext)
             || (event == RHH_BATTLER_ITEM_STATUS_CHANGED && sItemStatusContext)
             || (event == RHH_BATTLER_STATUS_CURED && sStatusCureContext)
             || (event == RHH_BATTLE_PARTY_STATUS_CURED && sPartyStatusCureContext)
             || (event == RHH_BATTLE_PARTY_ITEM_STATUS_CURED && sPartyItemStatusCureContext)
             || (event == RHH_BATTLE_CONDITION_CHANGED && sConditionContext
                 && (sConditionOrigin == RHH_BATTLER_STATUS_APPLIED || sConditionOrigin == RHH_BATTLER_ITEM_STATUS_CHANGED
                     || sConditionOrigin == RHH_BATTLER_STATUS_CURED || sConditionOrigin == RHH_BATTLE_PARTY_STATUS_CURED
                     || sConditionOrigin == RHH_BATTLE_PARTY_ITEM_STATUS_CURED)))
        || frame->count >= ARRAY_COUNT(frame->cues) || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || (sPending & (1u << battler)) || !IsBattlerAlive(battler)) return -1;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return -1;
    bool32 sourced = kind == MOD_STATUS_STAT_FROM || kind == MOD_STATUS_INFLICT;
    if (sourced && (source >= gBattlersCount || source >= MAX_BATTLERS_COUNT
        || (sPending & (1u << source)) || !IsBattlerAlive(source))) return -1;
    const struct RhhBattleParticipant *recipient = NULL, *actor = NULL;
    for (u32 i = 0; i < battle.participant_count; ++i) {
        const struct RhhBattleParticipant *p = &battle.participants[i];
        if (p->battler == battler && !p->fainted) recipient = p;
        if (sourced && p->battler == source && !p->fainted) actor = p;
    }
    if (!recipient || (sourced && !actor)) return -1;
    struct StatusReactionAction *action = &frame->cues[frame->count++];
    *action = (struct StatusReactionAction){.cue = {.battle_id = battle.battle_id,
        .identity = recipient->identity, .species = recipient->species, .turn = battle.turn, .battler = battler,
        .owner = recipient->owner, .party_slot = recipient->party_slot, .kind = kind, .first = first, .second = second,
        .animation = flags}};
    action->originEvent = event == RHH_BATTLE_CONDITION_CHANGED ? sConditionOrigin : event;
    if (action->originEvent == RHH_BATTLER_STATUS_APPLIED && sStatusApplicationContext)
        action->originSequence = sStatusApplicationContext->sequence;
    else if (action->originEvent == RHH_BATTLER_ITEM_STATUS_CHANGED && sItemStatusContext)
        action->originSequence = sItemStatusContext->sequence;
    else if (action->originEvent == RHH_BATTLER_STATUS_CURED && sStatusCureContext)
        action->originSequence = sStatusCureContext->sequence;
    else if (action->originEvent == RHH_BATTLE_PARTY_STATUS_CURED && sPartyStatusCureContext)
        action->originSequence = sPartyStatusCureContext->sequence;
    else if (action->originEvent == RHH_BATTLE_PARTY_ITEM_STATUS_CURED && sPartyItemStatusCureContext)
        action->originSequence = sPartyItemStatusCureContext->cure.sequence;
    if (actor) action->source = *actor;
    return 0;
}
static int StatusReactionAnimation(u32 battler, u32 animation)
{
    u32 anim = EffectAnimationId(animation);
    return anim ? QueueStatusReaction(RHH_BATTLE_NO_BATTLER, battler, MOD_STATUS_ANIMATION, anim, 0, 0) : -1;
}
static int StatusReactionHeal(u32 battler, u32 numerator, u32 denominator)
{
    return numerator && numerator <= denominator && denominator <= 16
        ? QueueStatusReaction(RHH_BATTLE_NO_BATTLER, battler, MOD_STATUS_HEAL, numerator, denominator, 0) : -1;
}
static int StatusReactionStat(u32 battler, u32 stat, s32 stages)
{
    return stat >= RHH_STAT_ATTACK && stat < RHH_BATTLE_STAT_COUNT && stages >= -6 && stages <= 6 && stages
        ? QueueStatusReaction(RHH_BATTLE_NO_BATTLER, battler, MOD_STATUS_STAT, stat, stages + 6, 0) : -1;
}
static int StatusReactionStatFrom(u32 source, u32 battler, u32 stat, s32 stages)
{
    return stat >= RHH_STAT_ATTACK && stat < RHH_BATTLE_STAT_COUNT && stages >= -6 && stages <= 6 && stages
        ? QueueStatusReaction(source, battler, MOD_STATUS_STAT_FROM, stat, stages + 6, 0) : -1;
}
static int StatusReactionStatusFrom(u32 source, u32 battler, u32 status)
{
    return status >= RHH_EFFECT_SLEEP && status <= RHH_EFFECT_TOXIC && sStatusEffects[status]
        ? QueueStatusReaction(source, battler, MOD_STATUS_INFLICT, status, 0, 0) : -1;
}
static int StatusReactionDamage(u32 battler, u32 numerator, u32 denominator, u32 flags)
{
    return numerator && numerator <= denominator && denominator <= 16 && !(flags & ~RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD)
        ? QueueStatusReaction(RHH_BATTLE_NO_BATTLER, battler, MOD_STATUS_DAMAGE, numerator, denominator, flags) : -1;
}
static int StatusReactionCure(u32 battler, u32 conditions)
{
    return conditions && !(conditions & ~RHH_BATTLE_CONDITIONS_ALL)
        ? QueueStatusReaction(RHH_BATTLE_NO_BATTLER, battler, MOD_STATUS_CURE, conditions, 0, 0) : -1;
}
static int ConditionChangeAnimation(u32 battler, u32 animation)
{
    if (RhhModsCurrentEvent() != RHH_BATTLE_CONDITION_CHANGED || RhhModsCurrentStage() != RHH_OBSERVE
        || !sConditionContext || sConditionContext->reason == RHH_CONDITION_EXPIRED) return -1;
    if (IsEffectApplyEvent(sConditionOrigin)) return QueueEffectAnimation(battler, animation);
    if (sConditionOrigin == RHH_INNATE_TURN_END) {
        u32 anim = EffectAnimationId(animation);
        return anim ? QueueInnateTurnActionFromContext(battler, MOD_INNATE_TURN_ANIMATION, anim, 0) : -1;
    }
    if (sConditionOrigin == RHH_INNATE_ENTRY) {
        u32 anim = EffectAnimationId(animation); struct InnateEntryTarget target;
        if (!anim || sInnateEntryAnimationCount >= ARRAY_COUNT(sInnateEntryAnimations)
            || InnateEntryTargetFromContext(battler, &target)) return -1;
        sInnateEntryAnimations[sInnateEntryAnimationCount].target = target;
        sInnateEntryAnimations[sInnateEntryAnimationCount++].animation = anim;
        return 0;
    }
    if (sConditionOrigin == RHH_BATTLER_STATUS_APPLIED || sConditionOrigin == RHH_BATTLER_ITEM_STATUS_CHANGED
        || sConditionOrigin == RHH_BATTLER_STATUS_CURED || sConditionOrigin == RHH_BATTLE_PARTY_STATUS_CURED
        || sConditionOrigin == RHH_BATTLE_PARTY_ITEM_STATUS_CURED) {
        return StatusReactionAnimation(battler, animation);
    }
    if (sConditionOrigin == RHH_MOVE_RESOLVED) {
        u32 anim = EffectAnimationId(animation);
        if (!anim || !sResolutionContext || sResolutionCueCount >= ARRAY_COUNT(sResolutionCues)
            || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || (sPending & (1u << battler))
            || !IsBattlerAlive(battler)) return -1;
        struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
        if (Current(&battle) || battle.battle_id != sResolutionContext->battle_id || battle.turn != sResolutionContext->turn) return -1;
        for (u32 i = 0; i < battle.participant_count; ++i) {
            const struct RhhBattleParticipant *p = &battle.participants[i];
            if (p->battler != battler || p->fainted) continue;
            sResolutionCues[sResolutionCueCount++] = (struct ResolutionCue){.battle_id = battle.battle_id,
                .identity = p->identity, .species = p->species, .turn = battle.turn, .battler = battler,
                .owner = p->owner, .party_slot = p->party_slot, .animation = anim};
            return 0;
        }
    }
    return -1;
}
static void RestoreScriptCureContinuation(void)
{
    if (!sScriptCureContinuationCount) return;
    const struct ScriptCureContinuation *saved = &sScriptCureContinuations[--sScriptCureContinuationCount];
    if (!sActive || !gMain.inBattle || sBattleId != saved->battleId) return;
    sScriptCureActor = saved->actor;
    gBattlerAttacker = saved->attacker; gBattlerTarget = saved->target; gEffectBattler = saved->effect;
    gBattlerAbility = saved->abilityBattler; gLastUsedAbility = saved->lastAbility; gLastUsedItem = saved->lastItem;
    gBattleScripting.battler = saved->scriptingBattler; gBattleScripting.savedBattler = saved->savedBattler;
    gBattleScripting.moveEffect = saved->moveEffect; gBattleScripting.animTurn = saved->animTurn;
    gBattleScripting.animTargetsHit = saved->animTargetsHit; gBattleScripting.fixedPopup = saved->fixedPopup;
    gBattleScripting.abilityPopupOverwrite = saved->popupAbility;
    memcpy(gBattleCommunication, saved->communication, sizeof(gBattleCommunication));
    memcpy(gBattleTextBuff1, saved->text1, sizeof(gBattleTextBuff1));
    memcpy(gBattleTextBuff2, saved->text2, sizeof(gBattleTextBuff2));
    memcpy(gBattleTextBuff3, saved->text3, sizeof(gBattleTextBuff3));
}
enum ModStatusReaction ModsStatusReactionNext(u32 *status, u32 *source)
{
    if (!sStatusReactionCount) return MOD_STATUS_DONE;
    struct StatusReactionFrame *frame = &sStatusReactions[sStatusReactionCount - 1];
    if (frame->playing) {
        if ((frame->playingKind == MOD_STATUS_HEAL || frame->playingKind == MOD_STATUS_DAMAGE)
            && gBattleStruct == frame->savedBattleStruct
            && frame->savedPassiveBattler < MAX_BATTLERS_COUNT
            && !gBattleStruct->passiveHpUpdate[frame->savedPassiveBattler])
            gBattleStruct->passiveHpUpdate[frame->savedPassiveBattler] = frame->savedPassiveHp;
        if (frame->playingKind == MOD_STATUS_DAMAGE || frame->playingKind == MOD_STATUS_CURE)
            gBattleScripting.battler = frame->savedScripting;
        if (frame->playingKind == MOD_STATUS_STAT_FROM || frame->playingKind == MOD_STATUS_INFLICT) {
            if (gBattleStruct == frame->savedBattleStruct && gBattleStruct)
                gBattleStruct->moldBreakerActive = frame->savedMoldBreaker;
            sInnateStatSourceBattler = frame->savedSource; gBattleScripting.battler = frame->savedScripting;
        }
        sEffectRecipient = frame->savedRecipient; gModEffectAnimation = frame->savedAnimation;
        frame->playing = FALSE;
    }
    while (frame->next < frame->count) {
        const struct StatusReactionAction *action = &frame->cues[frame->next++];
        const struct ResolutionCue *cue = &action->cue;
        if (!ConditionsAvailable() || cue->battler >= gBattlersCount || cue->battler >= MAX_BATTLERS_COUNT
            || (sPending & (1u << cue->battler)) || !IsBattlerAlive(cue->battler)) continue;
        struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
        if (Current(&battle) || battle.battle_id != cue->battle_id || battle.turn != cue->turn) continue;
        if (cue->kind == MOD_STATUS_STAT_FROM || cue->kind == MOD_STATUS_INFLICT) {
            u32 actor = action->source.battler;
            if (!source || !status || actor >= gBattlersCount || actor >= MAX_BATTLERS_COUNT
                || (sPending & (1u << actor)) || !IsBattlerAlive(actor)) continue;
            bool32 matches = FALSE;
            for (u32 i = 0; i < battle.participant_count; ++i) {
                const struct RhhBattleParticipant *p = &battle.participants[i];
                if (p->battler == actor && !p->fainted && p->identity == action->source.identity
                    && p->owner == action->source.owner && p->party_slot == action->source.party_slot
                    && p->species == action->source.species) matches = TRUE;
            }
            if (!matches) continue;
        }
        for (u32 i = 0; i < battle.participant_count; ++i) {
            const struct RhhBattleParticipant *p = &battle.participants[i];
            if (p->battler != cue->battler || p->fainted || p->identity != cue->identity
                || p->owner != cue->owner || p->party_slot != cue->party_slot || p->species != cue->species) continue;
            if (cue->kind == MOD_STATUS_HEAL || cue->kind == MOD_STATUS_DAMAGE) {
                if (cue->kind == MOD_STATUS_HEAL && (gBattleMons[cue->battler].volatiles.healBlockTimer
                    || gBattleMons[cue->battler].hp >= gBattleMons[cue->battler].maxHP)) break;
                if (cue->kind == MOD_STATUS_DAMAGE && !(cue->animation & RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD)
                    && IsAbilityAndRecord(cue->battler, GetBattlerAbilityIgnoreMoldBreaker(cue->battler), ABILITY_MAGIC_GUARD)) break;
                frame->savedBattleStruct = gBattleStruct; frame->savedPassiveBattler = cue->battler;
                frame->savedPassiveHp = gBattleStruct->passiveHpUpdate[cue->battler];
                u32 amount = GetNonDynamaxMaxHP(cue->battler) * cue->first / cue->second;
                if (cue->kind == MOD_STATUS_HEAL) SetHealAmount(cue->battler, amount);
                else { frame->savedScripting = gBattleScripting.battler; gBattleScripting.battler = cue->battler;
                    SetPassiveDamageAmount(cue->battler, amount); }
            } else if (cue->kind == MOD_STATUS_STAT || cue->kind == MOD_STATUS_STAT_FROM) {
                bool32 busy = gBattleStruct->statChangeBattler != 0;
                for (u32 j = 0; j < gBattlersCount; ++j)
                    if (gSpecialStatuses[j].statStageAmount || gSpecialStatuses[j].statStageAmount2) busy = TRUE;
                if (busy) break;
                SetStatChange(cue->battler, cue->first, (s32)cue->second - 6);
            } else if (cue->kind == MOD_STATUS_CURE) {
                if (!(BattleStatusConditions(gBattleMons[cue->battler].status1) & cue->first)
                    || !gBattleResources || !gBattleResources->battleScriptsStack
                    || gBattleResources->battleScriptsStack->size >= ARRAY_COUNT(gBattleResources->battleScriptsStack->ptr)) break;
                frame->savedScripting = gBattleScripting.battler; gBattleScripting.battler = cue->battler;
            } else if (cue->kind != MOD_STATUS_ANIMATION && cue->kind != MOD_STATUS_INFLICT) break;
            frame->savedRecipient = sEffectRecipient; frame->savedAnimation = gModEffectAnimation; frame->playing = TRUE;
            frame->playingKind = cue->kind;
            sEffectRecipient = cue->battler;
            if (cue->kind == MOD_STATUS_STAT_FROM || cue->kind == MOD_STATUS_INFLICT) {
                frame->savedBattleStruct = gBattleStruct; frame->savedMoldBreaker = gBattleStruct->moldBreakerActive;
                frame->savedSource = sInnateStatSourceBattler; frame->savedScripting = gBattleScripting.battler;
                gBattleStruct->moldBreakerActive = FALSE; gBattleScripting.battler = cue->battler;
                sInnateStatSourceBattler = action->source.battler;
                *source = action->source.battler; *status = cue->kind == MOD_STATUS_INFLICT ? sStatusEffects[cue->first] : 0;
            }
            if (cue->kind == MOD_STATUS_ANIMATION) gModEffectAnimation = cue->first;
            return cue->kind;
        }
    }
    sStatusReactionResume = frame->resume;
    if (frame->scriptBatchEnd) RestoreScriptCureContinuation();
    --sStatusReactionCount;
    return MOD_STATUS_DONE;
}
static int StatusCureContext(struct RhhBattleStatusCure *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sStatusCureContext
        || (event != RHH_BATTLER_STATUS_CURED
            && !(event == RHH_BATTLE_CONDITION_PROPOSAL && sConditionProposalContext && sConditionProposalContext->origin_event == RHH_BATTLER_STATUS_CURED)
            && !(event == RHH_BATTLE_CONDITION_CHANGED && sConditionContext && sConditionOrigin == RHH_BATTLER_STATUS_CURED))) return -1;
    *out = *sStatusCureContext; return 0;
}
static int StatusCureAttribution(struct RhhBattleStatusCureAttribution *out)
{
    struct RhhBattleStatusCure context = {.struct_size = sizeof(context)};
    if (!out || out->struct_size < sizeof(*out) || !sStatusCureAttribution || StatusCureContext(&context)) return -1;
    *out = *sStatusCureAttribution; return 0;
}
static int StatusCureMove(RhhContentKey *out)
{
    struct RhhBattleStatusCure context = {.struct_size = sizeof(context)};
    if (!out || StatusCureContext(&context)) return -1;
    *out = sStatusCureMove; return 0;
}
static int PartyStatusCureContext(struct RhhBattlePartyStatusCure *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sPartyStatusCureContext
        || (event != RHH_BATTLE_PARTY_STATUS_CURED
            && !(event == RHH_BATTLE_CONDITION_PROPOSAL && sConditionProposalContext
                 && sConditionProposalContext->origin_event == RHH_BATTLE_PARTY_STATUS_CURED)
            && !(event == RHH_BATTLE_CONDITION_CHANGED && sConditionContext
                 && sConditionOrigin == RHH_BATTLE_PARTY_STATUS_CURED))) return -1;
    *out = *sPartyStatusCureContext; return 0;
}
static int PartyItemStatusCureContext(struct RhhBattlePartyItemStatusCure *out)
{
    u32 event = RhhModsCurrentEvent();
    if (!out || out->struct_size < sizeof(*out) || !sPartyItemStatusCureContext
        || (event != RHH_BATTLE_PARTY_ITEM_STATUS_CURED
            && !(event == RHH_BATTLE_CONDITION_PROPOSAL && sConditionProposalContext
                 && sConditionProposalContext->origin_event == RHH_BATTLE_PARTY_ITEM_STATUS_CURED)
            && !(event == RHH_BATTLE_CONDITION_CHANGED && sConditionContext
                 && sConditionOrigin == RHH_BATTLE_PARTY_ITEM_STATUS_CURED))) return -1;
    *out = *sPartyItemStatusCureContext; return 0;
}
void ModsCapturePartyItemCure(struct ModsPartyItemCureCapture *out, u32 source, u32 slot)
{
    if (!out) return;
    *out = (struct ModsPartyItemCureCapture){0};
    if (!ConditionsAvailable() || RhhModsCurrentEvent() || sPartyItemStatusCureContext
        || !RhhModsHasHook(RHH_BATTLE_PARTY_ITEM_STATUS_CURED) || sPartyItemStatusCureSequence == UINT64_MAX
        || source >= gBattlersCount || GetBattlerTrainer(source) != B_TRAINER_PLAYER || slot >= PARTY_SIZE
        || (gAiLogicData && (gAiLogicData->aiCalcInProgress || gAiLogicData->aiPredictionInProgress || gAiLogicData->switchInCalc))) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    bool32 present = FALSE;
    for (u32 i = 0; i < battle.participant_count; ++i)
        if (battle.participants[i].battler == source && !battle.participants[i].fainted) present = TRUE;
    if (!present) return;
    struct Pokemon copy = gParties[B_TRAINER_PLAYER][slot];
    u32 species = GetMonData(&copy, MON_DATA_SPECIES_OR_EGG), before = GetMonData(&copy, MON_DATA_STATUS);
    if (!species || species == SPECIES_EGG || species >= NUM_SPECIES || !BattleStatusConditions(before)) return;
    struct RhhBattlePartyStatusCure cure = {.struct_size = sizeof(cure), .owner = B_TRAINER_PLAYER,
        .party_slot = slot, .source_battler = source, .before_raw = before, .run_id = sRunId, .battle_id = sBattleId,
        .identity = (u64)GetMonData(&copy, MON_DATA_OT_ID) << 32 | GetMonData(&copy, MON_DATA_PERSONALITY),
        .species = RhhContentKeyForId(RHH_SPECIES, species)};
    if (!PartyCureMatches(&cure, before)) return;
    out->valid = TRUE; out->cure = cure;
}
void ModsCommitPartyItemCure(const struct ModsPartyItemCureCapture *capture, u32 item)
{
    if (!capture || !capture->valid || !ConditionsAvailable() || RhhModsCurrentEvent() || sPartyItemStatusCureContext
        || !RhhModsHasHook(RHH_BATTLE_PARTY_ITEM_STATUS_CURED) || sPartyItemStatusCureSequence == UINT64_MAX
        || item >= ITEMS_COUNT) return;
    struct RhhBattlePartyItemStatusCure context = {.struct_size = sizeof(context), .cure = capture->cure,
        .item = RhhContentKeyForId(RHH_ITEM, item)};
    if (!context.item || context.cure.party_slot >= PARTY_SIZE) return;
    struct Pokemon copy = gParties[B_TRAINER_PLAYER][context.cure.party_slot];
    context.cure.after_raw = GetMonData(&copy, MON_DATA_STATUS);
    context.cure.removed = BattleStatusConditions(context.cure.before_raw) & ~BattleStatusConditions(context.cure.after_raw);
    if (!context.cure.removed || !PartyCureMatches(&context.cure, context.cure.after_raw)) return;
    context.cure.sequence = sPartyItemStatusCureSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_PARTY_ITEM_STATUS_CURED,
        .run_seed = RhhModsRunSeed(), .event_key = context.item, .subject = context.cure.identity,
        .ordinal = context.cure.sequence, .args = {context.cure.party_slot, context.cure.removed,
            context.cure.owner, context.cure.source_battler}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    if (sBagStatusBattleId == sBattleId && !sStatusReactionDispatch
        && sStatusReactionCount < ARRAY_COUNT(sStatusReactions)) sStatusReactionDispatch = &sBagStatusReaction;
    sPartyItemStatusCureContext = &context; RhhModsDispatch(&event, &decision); sPartyItemStatusCureContext = NULL;
    if (sStatusReactionDispatch == &sBagStatusReaction) sStatusReactionDispatch = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static void NotifyStatusCure(u32 battler, u32 before, u32 requested, u32 originEvent, u64 originSequence,
                             u32 cause, u32 source, RhhContentKey ability, RhhContentKey move,
                             bool32 presentationReady, bool32 deferred)
{
    if (!ConditionsAvailable() || RhhModsCurrentEvent() || sStatusCureContext
        || !RhhModsHasHook(RHH_BATTLER_STATUS_CURED) || sStatusCureSequence == UINT64_MAX
        || battler >= gBattlersCount) return;
    u32 after = gBattleMons[battler].status1;
    u32 removed = BattleStatusConditions(before) & ~BattleStatusConditions(after);
    if (!removed) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    struct RhhBattleStatusCure context = {.struct_size = sizeof(context), .battler = battler,
        .requested = requested, .removed = removed, .before_raw = before, .after_raw = after,
        .run_id = sRunId, .battle_id = sBattleId, .sequence = sStatusCureSequence++,
        .origin_event = originEvent, .origin_sequence = originSequence};
    for (u32 i = 0; i < battle.participant_count; ++i)
        if (battle.participants[i].battler == battler)
        { context.participant = battle.participants[i]; context.participant_available = TRUE; break; }
    struct RhhBattleStatusCureAttribution attribution = {.struct_size = sizeof(attribution), .cause = cause,
        .source_battler = source, .ability = ability};
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLER_STATUS_CURED,
        .run_seed = RhhModsRunSeed(), .event_key = ability ? ability : move, .subject = context.participant.identity, .ordinal = context.sequence,
        .args = {battler, removed, context.requested, context.origin_event}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    struct StatusReactionFrame *child = NULL;
    if (deferred) {
        if (sScriptCureBatch.active && sScriptCureBatch.count < ARRAY_COUNT(sScriptCureBatch.frames)
            && sStatusReactionCount + sScriptCureBatch.count < ARRAY_COUNT(sStatusReactions)) {
            child = &sScriptCureBatch.frames[sScriptCureBatch.count];
            *child = (struct StatusReactionFrame){0}; sStatusReactionDispatch = child;
        }
    } else if (cause == RHH_BATTLE_CURE_REACTION) {
        if (sStatusReactionCount < ARRAY_COUNT(sStatusReactions)) {
            child = &sStatusReactions[sStatusReactionCount];
            *child = (struct StatusReactionFrame){.resume = BattleScript_ModStatusReaction};
            sStatusReactionDispatch = child;
        }
    } else if (presentationReady) BeginStatusReaction(cause == RHH_BATTLE_CURE_NATURAL || cause == RHH_BATTLE_CURE_MOVE || cause == RHH_BATTLE_CURE_UPROAR
        ? STATUS_PRESENT_NATURAL : STATUS_PRESENT_ABILITY);
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sStatusCureContext = &context; sStatusCureAttribution = &attribution; sStatusCureMove = move;
    RhhModsDispatch(&event, &decision);
    sStatusCureMove = 0; sStatusCureAttribution = NULL; sStatusCureContext = NULL;
    if (deferred) {
        sStatusReactionDispatch = NULL;
        if (child && child->count) ++sScriptCureBatch.count;
    } else if (cause == RHH_BATTLE_CURE_REACTION) {
        sStatusReactionDispatch = NULL;
        if (child && child->count) ++sStatusReactionCount;
    } else EndStatusReaction();
    gRngValue = rng; gRng2Value = rng2;
}
void ModsBattleAbilityStatusCured(u32 battler, u32 ability, u32 before, u32 cause, bool32 presentationReady)
{
    if (cause != RHH_BATTLE_CURE_IMMUNITY_ABILITY && cause != RHH_BATTLE_CURE_END_TURN_ABILITY
        && cause != RHH_BATTLE_CURE_SWITCH_ABILITY) return;
    RhhContentKey key = ability && ability < ABILITIES_COUNT ? RhhContentKeyForId(RHH_ABILITY, ability) : 0;
    NotifyStatusCure(battler, before, BattleStatusConditions(before), 0, 0, cause, battler, key, 0, presentationReady, FALSE);
}
void ModsBattleEngineStatusCured(u32 battler, u32 before, u32 cause, u32 source, u32 move, bool32 presentationReady)
{
    if (cause != RHH_BATTLE_CURE_NATURAL && cause != RHH_BATTLE_CURE_MOVE && cause != RHH_BATTLE_CURE_UPROAR) return;
    RhhContentKey key = cause == RHH_BATTLE_CURE_MOVE ? BattleMoveKey(move) : 0;
    if (cause != RHH_BATTLE_CURE_MOVE || source >= gBattlersCount) source = RHH_BATTLE_NO_BATTLER;
    NotifyStatusCure(battler, before, BattleStatusConditions(before), 0, 0, cause, source, 0, key, presentationReady, FALSE);
}
void ModsCaptureScriptCureActor(u32 battler, u32 cause, u32 ability)
{
    sScriptCureActor = (struct ScriptCureActor){0};
    if (!ConditionsAvailable() || battler >= gBattlersCount || (sPending & (1u << battler))
        || (cause != RHH_BATTLE_CURE_MOVE && cause != RHH_BATTLE_CURE_SCRIPT_ABILITY)) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    for (u32 i = 0; i < battle.participant_count; ++i) {
        const struct RhhBattleParticipant *p = &battle.participants[i];
        if (p->battler != battler || p->fainted) continue;
        sScriptCureActor = (struct ScriptCureActor){.battleId = battle.battle_id, .turn = battle.turn,
            .cause = cause, .move = cause == RHH_BATTLE_CURE_MOVE ? BattleMoveKey(gCurrentMove) : 0,
            .ability = cause == RHH_BATTLE_CURE_SCRIPT_ABILITY && ability && ability < ABILITIES_COUNT
                ? RhhContentKeyForId(RHH_ABILITY, ability) : 0, .participant = *p, .valid = TRUE};
        return;
    }
}
void ModsBeginScriptCure(u32 cause, u32 battler, u32 ability, bool32 useCapturedActor, const u8 *instruction)
{
    if (sScriptCureBatch.count) return;
    sScriptCureBatch = (struct ScriptCureBatch){0};
    if (!instruction || !ConditionsAvailable() || (cause != RHH_BATTLE_CURE_MOVE && cause != RHH_BATTLE_CURE_SCRIPT_ABILITY)) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    RhhContentKey move = cause == RHH_BATTLE_CURE_MOVE ? BattleMoveKey(gCurrentMove) : 0;
    RhhContentKey key = cause == RHH_BATTLE_CURE_SCRIPT_ABILITY && ability && ability < ABILITIES_COUNT
        ? RhhContentKeyForId(RHH_ABILITY, ability) : 0;
    u32 source = RHH_BATTLE_NO_BATTLER;
    if (useCapturedActor) {
        const struct ScriptCureActor *actor = &sScriptCureActor;
        if (actor->valid && actor->cause == cause && actor->battleId == battle.battle_id && actor->turn == battle.turn
            && (cause != RHH_BATTLE_CURE_MOVE || actor->move == move) && (cause != RHH_BATTLE_CURE_SCRIPT_ABILITY || actor->ability == key))
            for (u32 i = 0; i < battle.participant_count; ++i) {
                const struct RhhBattleParticipant *p = &battle.participants[i], *saved = &actor->participant;
                if (p->battler == saved->battler && !p->fainted && p->identity == saved->identity
                    && p->owner == saved->owner && p->party_slot == saved->party_slot && p->species == saved->species)
                    source = p->battler;
            }
    } else if (battler < gBattlersCount && !(sPending & (1u << battler))) {
        for (u32 i = 0; i < battle.participant_count; ++i)
            if (battle.participants[i].battler == battler && !battle.participants[i].fainted) source = battler;
    }
    sScriptCureBatch = (struct ScriptCureBatch){.battleId = battle.battle_id, .cause = cause,
        .source = source, .ability = key, .move = move, .instruction = instruction, .active = TRUE};
}
void ModsRecordScriptCure(u32 battler, u32 before, const u8 *instruction)
{
    if (!sScriptCureBatch.active || sScriptCureBatch.instruction != instruction
        || !ConditionsAvailable() || sScriptCureBatch.battleId != sBattleId) return;
    NotifyStatusCure(battler, before, BattleStatusConditions(before), 0, 0, sScriptCureBatch.cause,
        sScriptCureBatch.source, sScriptCureBatch.ability, sScriptCureBatch.move, FALSE, TRUE);
}
void ModsCaptureScriptPartyCure(u32 battler, u32 slots, const u8 *instruction)
{
    if (!sScriptCureBatch.active || sScriptCureBatch.instruction != instruction
        || sScriptCureBatch.cause != RHH_BATTLE_CURE_MOVE || !ConditionsAvailable()
        || sScriptCureBatch.battleId != sBattleId || !RhhModsHasHook(RHH_BATTLE_PARTY_STATUS_CURED)
        || battler >= gBattlersCount || GetBattlerTrainer(battler) != B_TRAINER_PLAYER || !slots || sPending) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    sScriptCureBatch.partyCaptured = TRUE; sScriptCureBatch.partyBattler = battler; sScriptCureBatch.partyMask = slots;
    for (u32 i = 0; i < battle.participant_count; ++i)
        if (battle.participants[i].battler == sScriptCureBatch.source && !battle.participants[i].fainted)
        { sScriptCureBatch.partyActor = battle.participants[i]; sScriptCureBatch.partyActorAvailable = TRUE; }
    for (u32 slot = 0; slot < PARTY_SIZE; ++slot) {
        if (!(slots & (1u << slot))) continue;
        struct Pokemon copy = gParties[B_TRAINER_PLAYER][slot];
        u32 species = GetMonData(&copy, MON_DATA_SPECIES_OR_EGG), before = GetMonData(&copy, MON_DATA_STATUS);
        if (!species || species == SPECIES_EGG || species >= NUM_SPECIES || !BattleStatusConditions(before)) continue;
        struct RhhBattlePartyStatusCure check = {.struct_size = sizeof(check), .owner = B_TRAINER_PLAYER, .party_slot = slot,
            .run_id = sRunId, .battle_id = sBattleId, .species = RhhContentKeyForId(RHH_SPECIES, species),
            .identity = (u64)GetMonData(&copy, MON_DATA_OT_ID) << 32 | GetMonData(&copy, MON_DATA_PERSONALITY)};
        if (PartyCureMatches(&check, before))
            sScriptCureBatch.party[slot] = (struct PartyCureReceipt){.identity = check.identity, .species = check.species,
                .slot = slot, .before = before, .valid = TRUE};
    }
}
void ModsPartyCureControllerBefore(u32 battler)
{
    if (!sScriptCureBatch.partyCaptured || battler != sScriptCureBatch.partyBattler) return;
    const u8 *data = gBattleResources->bufferA[battler];
    if (!ConditionsAvailable() || sScriptCureBatch.battleId != sBattleId
        || data[0] != CONTROLLER_SETMONDATA || data[1] != REQUEST_STATUS_BATTLE
        || data[2] != sScriptCureBatch.partyMask || data[3] || data[4] || data[5] || data[6]) {
        sScriptCureBatch.partyCaptured = FALSE; return;
    }
    sScriptCureBatch.partyControllerReady = TRUE;
    for (u32 slot = 0; slot < PARTY_SIZE; ++slot) {
        struct PartyCureReceipt *receipt = &sScriptCureBatch.party[slot];
        struct RhhBattlePartyStatusCure check = {.owner = B_TRAINER_PLAYER, .party_slot = slot, .identity = receipt->identity,
            .species = receipt->species, .run_id = sRunId, .battle_id = sBattleId};
        if (receipt->valid && !PartyCureMatches(&check, receipt->before)) receipt->valid = FALSE;
    }
}
void ModsPartyCureControllerAfter(u32 battler)
{
    if (!sScriptCureBatch.partyControllerReady || battler != sScriptCureBatch.partyBattler) return;
    sScriptCureBatch.partyControllerReady = FALSE; sScriptCureBatch.partyCaptured = FALSE;
    for (u32 slot = 0; slot < PARTY_SIZE; ++slot) {
        struct PartyCureReceipt *receipt = &sScriptCureBatch.party[slot];
        struct RhhBattlePartyStatusCure check = {.owner = B_TRAINER_PLAYER, .party_slot = slot, .identity = receipt->identity,
            .species = receipt->species, .run_id = sRunId, .battle_id = sBattleId};
        receipt->committed = receipt->valid && PartyCureMatches(&check, 0);
    }
}
static void DispatchScriptPartyCures(void)
{
    if (!ConditionsAvailable() || sScriptCureBatch.battleId != sBattleId || RhhModsCurrentEvent()) return;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return;
    u32 source = RHH_BATTLE_NO_BATTLER;
    for (u32 i = 0; sScriptCureBatch.partyActorAvailable && i < battle.participant_count; ++i) {
        const struct RhhBattleParticipant *p = &battle.participants[i], *saved = &sScriptCureBatch.partyActor;
        if (!p->fainted && p->battler == saved->battler && p->identity == saved->identity
            && p->owner == saved->owner && p->party_slot == saved->party_slot && p->species == saved->species)
            source = p->battler;
    }
    for (u32 slot = 0; slot < PARTY_SIZE; ++slot) {
        struct PartyCureReceipt *receipt = &sScriptCureBatch.party[slot];
        if (!receipt->committed || sPartyStatusCureSequence == UINT64_MAX) continue;
        receipt->committed = FALSE;
        struct RhhBattlePartyStatusCure context = {.struct_size = sizeof(context), .owner = B_TRAINER_PLAYER,
            .party_slot = slot, .source_battler = source, .before_raw = receipt->before, .after_raw = 0,
            .removed = BattleStatusConditions(receipt->before), .run_id = sRunId, .battle_id = sBattleId,
            .identity = receipt->identity, .species = receipt->species, .move = sScriptCureBatch.move};
        if (!PartyCureMatches(&context, 0)) continue;
        context.sequence = sPartyStatusCureSequence++;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_PARTY_STATUS_CURED,
            .run_seed = RhhModsRunSeed(), .event_key = context.move, .subject = context.identity, .ordinal = context.sequence,
            .args = {slot, context.removed, context.owner, source}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        struct StatusReactionFrame *child = NULL;
        if (sScriptCureBatch.count < ARRAY_COUNT(sScriptCureBatch.frames)
            && sStatusReactionCount + sScriptCureBatch.count < ARRAY_COUNT(sStatusReactions)) {
            child = &sScriptCureBatch.frames[sScriptCureBatch.count];
            *child = (struct StatusReactionFrame){0}; sStatusReactionDispatch = child;
        }
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        sPartyStatusCureContext = &context; RhhModsDispatch(&event, &decision); sPartyStatusCureContext = NULL;
        sStatusReactionDispatch = NULL; if (child && child->count) ++sScriptCureBatch.count;
        gRngValue = rng; gRng2Value = rng2;
    }
}
static bool32 StartCureReactionFrames(struct StatusReactionFrame *frames, u32 count, const u8 *resume)
{
    bool32 available = resume && ConditionsAvailable()
        && count && sStatusReactionCount + count <= ARRAY_COUNT(sStatusReactions)
        && sScriptCureContinuationCount < ARRAY_COUNT(sScriptCureContinuations);
    if (available) {
        struct ScriptCureContinuation *saved = &sScriptCureContinuations[sScriptCureContinuationCount++];
        *saved = (struct ScriptCureContinuation){.battleId = sBattleId, .actor = sScriptCureActor,
            .attacker = gBattlerAttacker, .target = gBattlerTarget, .effect = gEffectBattler,
            .abilityBattler = gBattlerAbility, .scriptingBattler = gBattleScripting.battler,
            .lastAbility = gLastUsedAbility, .lastItem = gLastUsedItem, .savedBattler = gBattleScripting.savedBattler,
            .moveEffect = gBattleScripting.moveEffect, .animTurn = gBattleScripting.animTurn,
            .animTargetsHit = gBattleScripting.animTargetsHit, .fixedPopup = gBattleScripting.fixedPopup,
            .popupAbility = gBattleScripting.abilityPopupOverwrite};
        memcpy(saved->communication, gBattleCommunication, sizeof(gBattleCommunication));
        memcpy(saved->text1, gBattleTextBuff1, sizeof(gBattleTextBuff1));
        memcpy(saved->text2, gBattleTextBuff2, sizeof(gBattleTextBuff2));
        memcpy(saved->text3, gBattleTextBuff3, sizeof(gBattleTextBuff3));
        for (u32 i = count; i > 0; --i) {
            struct StatusReactionFrame frame = frames[i - 1];
            frame.resume = i == count ? resume : BattleScript_ModStatusReaction;
            frame.scriptBatchEnd = i == count;
            sStatusReactions[sStatusReactionCount++] = frame;
        }
    }
    return available;
}
bool32 ModsStartScriptCureReactions(const u8 *resume)
{
    if (!sScriptCureBatch.active) return FALSE;
    DispatchScriptPartyCures();
    bool32 started = sScriptCureBatch.battleId == sBattleId
        && StartCureReactionFrames(sScriptCureBatch.frames, sScriptCureBatch.count, resume);
    sScriptCureBatch = (struct ScriptCureBatch){0};
    return started;
}
void ModsBeginBagStatusReactions(void)
{
    sBagStatusReaction = (struct StatusReactionFrame){0};
    sBagStatusBattleId = ConditionsAvailable() && !sStatusReactionDispatch ? sBattleId : 0;
}
bool32 ModsStartBagStatusReactions(const u8 *resume)
{
    bool32 started = sBagStatusBattleId && sBagStatusBattleId == sBattleId
        && StartCureReactionFrames(&sBagStatusReaction, sBagStatusReaction.count ? 1 : 0, resume);
    sBagStatusBattleId = 0; sBagStatusReaction = (struct StatusReactionFrame){0};
    return started;
}
void ModsCaptureStatusCure(struct ModsStatusCureCapture *capture, u32 battler)
{
    if (!capture) return;
    *capture = (struct ModsStatusCureCapture){0};
    if (battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT) return;
    capture->before = gBattleMons[battler].status1;
    capture->stack = gBattleResources ? gBattleResources->battleScriptsStack : NULL;
    capture->stackSize = capture->stack ? capture->stack->size : 0;
    capture->resume = gBattlescriptCurrInstr;
}
void ModsFinishEngineStatusCure(const struct ModsStatusCureCapture *capture, u32 battler, u32 cause, u32 source, u32 move)
{
    if (!capture) return;
    struct BattleScriptsStack *stack = capture->stack;
    bool32 ready = stack && gBattleResources && gBattleResources->battleScriptsStack == stack
        && capture->stackSize < ARRAY_COUNT(stack->ptr) && stack->size == capture->stackSize + 1
        && stack->ptr[capture->stackSize] == capture->resume;
    ModsBattleEngineStatusCured(battler, capture->before, cause, source, move, ready);
}
void ModsStatusReactionCure(void)
{
    if (!sStatusReactionCount) return;
    struct StatusReactionFrame *frame = &sStatusReactions[sStatusReactionCount - 1];
    if (!frame->playing || frame->playingKind != MOD_STATUS_CURE || !frame->next) return;
    const struct StatusReactionAction *action = &frame->cues[frame->next - 1];
    u32 battler = action->cue.battler, before = gBattleMons[battler].status1;
    u32 removed = BattleStatusConditions(before) & action->cue.first;
    if (!removed) return;
    u32 clear = (removed & RHH_BATTLE_CONDITION_SLEEP ? STATUS1_SLEEP : 0)
        | (removed & RHH_BATTLE_CONDITION_POISON ? STATUS1_POISON : 0)
        | (removed & RHH_BATTLE_CONDITION_BURN ? STATUS1_BURN : 0)
        | (removed & RHH_BATTLE_CONDITION_FREEZE ? STATUS1_FREEZE : 0)
        | (removed & RHH_BATTLE_CONDITION_PARALYSIS ? STATUS1_PARALYSIS : 0)
        | (removed & RHH_BATTLE_CONDITION_TOXIC ? STATUS1_TOXIC_POISON | STATUS1_TOXIC_COUNTER : 0)
        | (removed & RHH_BATTLE_CONDITION_FROSTBITE ? STATUS1_FROSTBITE : 0);
    if (removed & RHH_BATTLE_CONDITION_SLEEP) {
        TryDeactivateSleepClause(battler, gBattlerPartyIndexes[battler]);
        gBattleMons[battler].volatiles.nightmare = FALSE;
    }
    if (removed & (RHH_BATTLE_CONDITION_FREEZE | RHH_BATTLE_CONDITION_FROSTBITE))
        GetBattlerPartyState(battler)->freezeTurns = 0;
    gBattleMons[battler].status1 = before & ~clear;
    BtlController_EmitSetMonData(battler, B_COMM_TO_CONTROLLER, REQUEST_STATUS_BATTLE, 0,
        sizeof(gBattleMons[battler].status1), &gBattleMons[battler].status1);
    MarkBattlerForControllerExec(battler);
    NotifyStatusCure(battler, before, action->cue.first, action->originEvent, action->originSequence,
        RHH_BATTLE_CURE_REACTION, RHH_BATTLE_NO_BATTLER, 0, 0, FALSE, FALSE);
}
const u8 *ModsStatusReactionContinuation(void)
{
    const u8 *resume = sStatusReactionResume;
    sStatusReactionResume = NULL;
    return resume ? resume : BattleScript_ModStatusReactionEnd;
}
bool32 ModsMoveResolutionCuesPending(void) { return sResolutionCueNext < sResolutionCueCount; }
bool32 ModsMoveResolutionNextCue(void)
{
    while (sResolutionCueNext < sResolutionCueCount) {
        const struct ResolutionCue *cue = &sResolutionCues[sResolutionCueNext++];
        if (!ConditionsAvailable() || cue->battler >= gBattlersCount || cue->battler >= MAX_BATTLERS_COUNT
            || (sPending & (1u << cue->battler)) || !IsBattlerAlive(cue->battler)) continue;
        struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
        if (Current(&battle) || battle.battle_id != cue->battle_id || battle.turn != cue->turn) continue;
        for (u32 i = 0; i < battle.participant_count; ++i) {
            const struct RhhBattleParticipant *p = &battle.participants[i];
            if (p->battler != cue->battler || p->fainted || p->identity != cue->identity
                || p->owner != cue->owner || p->party_slot != cue->party_slot || p->species != cue->species) continue;
            sEffectRecipient = cue->battler; gModEffectAnimation = cue->animation; return TRUE;
        }
    }
    return FALSE;
}
void ModsMoveResolutionCuesDone(void)
{ sResolutionCueCount = sResolutionCueNext = 0; sResolutionCueResume = TRUE; }
bool32 ModsConsumeMoveResolutionResume(void)
{ bool32 resume = sResolutionCueResume; sResolutionCueResume = FALSE; return resume; }
static int EffectHealUserFromDamage(u32 numerator, u32 denominator, u32 flags)
{
    if (!CanQueueAnyEffectAction() || !sDamageContext || !(sDamageContext->flags & RHH_MOVE_DAMAGE_RECORDED)
        || !numerator || numerator > denominator || denominator > 16
        || (flags & ~RHH_EFFECT_HEAL_INCLUDE_SUBSTITUTE)) return -1;
    u32 damage = sDamageContext->hp_lost;
    if (flags & RHH_EFFECT_HEAL_INCLUDE_SUBSTITUTE) damage += sDamageContext->substitute_lost;
    u32 amount = damage ? max(1, damage * numerator / denominator) : 0;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_HEAL_DAMAGE_USER,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .amount = amount,
        .publicFirst = numerator, .publicSecond = denominator, .publicFlags = flags});
}
static int EffectDrainUser(u32 numerator, u32 denominator, u32 flags)
{
    if (!CanQueueAnyEffectAction() || !sDamageContext || !(sDamageContext->flags & RHH_MOVE_DAMAGE_RECORDED)
        || !numerator || numerator > denominator || denominator > 16
        || (flags & ~RHH_EFFECT_DRAIN_INCLUDE_SUBSTITUTE)) return -1;
    u32 damage = sDamageContext->hp_lost;
    if (flags & RHH_EFFECT_DRAIN_INCLUDE_SUBSTITUTE) damage += sDamageContext->substitute_lost;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_DRAIN_USER,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target,
        .first = numerator, .second = denominator, .amount = damage, .publicFlags = flags});
}
static int EffectStatus(u32 status)
{
    if (!CanQueueEffectAction() || !status || status >= ARRAY_COUNT(sStatusEffects) || !sStatusEffects[status]) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_STATUS,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .first = status});
}
static int EffectStatusBattler(u32 battler, u32 status)
{
    if (!CanQueueAnyEffectAction() || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || !IsBattlerAlive(battler) || !status || status >= ARRAY_COUNT(sStatusEffects) || !sStatusEffects[status]) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_STATUS_BATTLER,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .recipient = battler, .first = status});
}
static int EffectStat(u32 stat, s32 stages)
{
    if (!CanQueueEffectAction() || stat < RHH_STAT_ATTACK || stat >= RHH_BATTLE_STAT_COUNT
        || stages < -6 || stages > 6 || !stages) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_STAT,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .first = stat, .second = stages + 6});
}
static int EffectStatBattler(u32 battler, u32 stat, s32 stages)
{
    if (!CanQueueAnyEffectAction() || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || !IsBattlerAlive(battler) || stat < RHH_STAT_ATTACK || stat >= RHH_BATTLE_STAT_COUNT
        || stages < -6 || stages > 6 || !stages) return -1;
    return QueueEffectAction((struct EffectAction){.kind = MOD_EFFECT_STAT_BATTLER,
        .attacker = sEffectContext->attacker, .target = sEffectContext->target, .recipient = battler,
        .first = stat, .second = stages + 6});
}
enum ModMoveEffectAction ModsMoveEffectNextAction(u32 *value, s32 *stages)
{
    while (sEffectActionNext < sEffectActionCount)
    {
        struct EffectAction action = sEffectActions[sEffectActionNext++];
        if (!sActive || sEnding || !gMain.inBattle || !gBattleStruct || gBattleOutcome
            || !RhhModsHasActiveRun() || sRunId != RhhModsRunId()
            || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))
            || action.battleId != sBattleId || action.move != gCurrentMove
            || action.attacker != gBattlerAttacker || action.target != gBattlerTarget
            || gBattlersCount > MAX_BATTLERS_COUNT
            || action.attacker >= gBattlersCount || action.target >= gBattlersCount
            || action.attacker >= MAX_BATTLERS_COUNT || action.target >= MAX_BATTLERS_COUNT
            || ((action.kind == MOD_EFFECT_HEAL_BATTLER || action.kind == MOD_EFFECT_DAMAGE_BATTLER || action.kind == MOD_EFFECT_DAMAGE_FROM_HIT || action.kind == MOD_EFFECT_ANIMATION
                 || action.kind == MOD_EFFECT_STAT_BATTLER || action.kind == MOD_EFFECT_STATUS_BATTLER)
                && (action.recipient >= gBattlersCount
                || action.recipient >= MAX_BATTLERS_COUNT))
            || !IsBattlerAlive(action.kind == MOD_EFFECT_HEAL_USER || action.kind == MOD_EFFECT_HEAL_DAMAGE_USER || action.kind == MOD_EFFECT_FIELD_RULE || action.kind == MOD_EFFECT_ROOM_TOGGLE || action.kind == MOD_EFFECT_FIELD_MOVE
                               || action.kind == MOD_EFFECT_DRAIN_USER ? action.attacker
                               : action.kind == MOD_EFFECT_HEAL_BATTLER || action.kind == MOD_EFFECT_DAMAGE_BATTLER || action.kind == MOD_EFFECT_DAMAGE_FROM_HIT || action.kind == MOD_EFFECT_ANIMATION
                                 || action.kind == MOD_EFFECT_STAT_BATTLER || action.kind == MOD_EFFECT_STATUS_BATTLER
                                 ? action.recipient : action.target)
            || !EffectParticipantMatches(action.attacker, &action.source)
            || !EffectParticipantMatches(EffectActionSubject(&action), &action.subject)
            || (action.hasTarget && !EffectParticipantMatches(action.target, &action.origin))) continue;
        if (action.kind == MOD_EFFECT_FIELD_RULE)
        {
            if ((action.first == RHH_BATTLE_FIELD_GRAVITY && (gFieldStatuses & STATUS_FIELD_GRAVITY))
                || (action.first == RHH_BATTLE_FIELD_ION_DELUGE && (gFieldStatuses & STATUS_FIELD_ION_DELUGE))) continue;
            *value = action.first == RHH_BATTLE_FIELD_GRAVITY ? MOVE_EFFECT_GRAVITY : MOVE_EFFECT_ION_DELUGE;
            return MOD_EFFECT_FIELD_RULE;
        }
        if (action.kind == MOD_EFFECT_ROOM_TOGGLE)
        { *value = action.first; return MOD_EFFECT_ROOM_TOGGLE; }
        if (action.kind == MOD_EFFECT_FIELD_MOVE)
        { *value = action.first; return MOD_EFFECT_FIELD_MOVE; }
        if (action.kind == MOD_EFFECT_ANIMATION)
        { sEffectRecipient = action.recipient; gModEffectAnimation = action.first; return MOD_EFFECT_ANIMATION; }
        if (action.kind == MOD_EFFECT_DAMAGE_BATTLER || action.kind == MOD_EFFECT_DAMAGE_FROM_HIT)
        {
            u32 recipient = action.recipient;
            u32 flags = action.kind == MOD_EFFECT_DAMAGE_FROM_HIT ? action.first : action.amount;
            if (action.kind == MOD_EFFECT_DAMAGE_FROM_HIT && !action.amount) continue;
            if (!(flags & RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD)
                && IsAbilityAndRecord(recipient, GetBattlerAbilityIgnoreMoldBreaker(recipient), ABILITY_MAGIC_GUARD)) continue;
            SetPassiveDamageAmount(recipient, action.kind == MOD_EFFECT_DAMAGE_FROM_HIT ? action.amount
                                   : GetNonDynamaxMaxHP(recipient) * action.first / action.second);
            sEffectRecipient = recipient;
            return MOD_EFFECT_DAMAGE_BATTLER;
        }
        if (action.kind == MOD_EFFECT_DRAIN_USER)
        {
            if (!action.amount || gBattleMons[action.attacker].volatiles.healBlockTimer) continue;
            *value = action.amount * action.first / action.second;
            return MOD_EFFECT_DRAIN_USER;
        }
        if (action.kind == MOD_EFFECT_STATUS || action.kind == MOD_EFFECT_STATUS_BATTLER)
        {
            *value = sStatusEffects[action.first];
            if (action.kind == MOD_EFFECT_STATUS_BATTLER) sEffectRecipient = action.recipient;
            return action.kind;
        }
        if (action.kind == MOD_EFFECT_STAT || action.kind == MOD_EFFECT_STAT_BATTLER)
        {
            bool32 busy = gBattleStruct->statChangeBattler != 0;
            for (u32 i = 0; i < gBattlersCount; ++i)
                if (gSpecialStatuses[i].statStageAmount || gSpecialStatuses[i].statStageAmount2) busy = TRUE;
            if (busy) continue;
            *value = action.first; *stages = (s32)action.second - 6;
            if (action.kind == MOD_EFFECT_STAT_BATTLER) sEffectRecipient = action.recipient;
            return action.kind;
        }
        bool32 user = action.kind == MOD_EFFECT_HEAL_USER || action.kind == MOD_EFFECT_HEAL_DAMAGE_USER;
        u32 recipient = user ? action.attacker : action.kind == MOD_EFFECT_HEAL_BATTLER ? action.recipient : action.target;
        if (gBattleMons[action.attacker].volatiles.healBlockTimer
            || gBattleMons[recipient].volatiles.healBlockTimer
            || gBattleMons[recipient].hp >= gBattleMons[recipient].maxHP) continue;
        u32 amount = action.kind == MOD_EFFECT_HEAL_DAMAGE_USER ? action.amount
            : GetNonDynamaxMaxHP(recipient) * action.first / action.second;
        if (action.kind == MOD_EFFECT_HEAL_DAMAGE_USER && !amount) continue;
        SetHealAmount(recipient, amount);
        if (action.kind == MOD_EFFECT_HEAL_BATTLER)
        { sEffectRecipient = recipient; return MOD_EFFECT_HEAL_BATTLER; }
        return user ? MOD_EFFECT_HEAL_USER : MOD_EFFECT_HEAL;
    }
    return MOD_EFFECT_DONE;
}
u32 ModsMoveEffectRecipient(void) { return sEffectRecipient; }
void ModsMoveEffectItemBegin(void)
{
    if (!sEffectScriptingBattlerSaved)
    { sEffectSavedScriptingBattler = gBattleScripting.battler; sEffectScriptingBattlerSaved = TRUE; }
    gBattleScripting.battler = sEffectRecipient;
}
void ModsMoveEffectItemEnd(void)
{
    if (sEffectScriptingBattlerSaved)
    { gBattleScripting.battler = sEffectSavedScriptingBattler; sEffectScriptingBattlerSaved = FALSE; }
}
static int ConditionExpiryMessage(const char *text)
{
    if (RhhModsCurrentEvent() != RHH_BATTLE_CONDITION_CHANGED || RhhModsCurrentStage() != RHH_OBSERVE
        || !sConditionContext || sConditionContext->reason != RHH_CONDITION_EXPIRED) return -1;
    return QueueBattleMessage(text, EXPIRY_MESSAGE_LIMIT);
}
static int ConditionExpiryAnimation(u32 battler, u32 animation)
{
    u32 anim = EffectAnimationId(animation);
    if (RhhModsCurrentEvent() != RHH_BATTLE_CONDITION_CHANGED || RhhModsCurrentStage() != RHH_OBSERVE
        || !sConditionContext || sConditionContext->reason != RHH_CONDITION_EXPIRED || !anim
        || sConditionAnimationCount >= ARRAY_COUNT(sConditionAnimations) || battler >= gBattlersCount
        || battler >= MAX_BATTLERS_COUNT || (sPending & (1u << battler)) || !IsBattlerAlive(battler)) return -1;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return -1;
    for (u32 i = 0; i < battle.participant_count; ++i) {
        const struct RhhBattleParticipant *p = &battle.participants[i];
        if (p->battler != battler || p->fainted) continue;
        sConditionAnimations[sConditionAnimationCount++] = (struct ConditionAnimation){
            .battle_id = battle.battle_id, .identity = p->identity, .species = p->species, .turn = battle.turn,
            .battler = battler, .owner = p->owner, .party_slot = p->party_slot, .animation = anim};
        return 0;
    }
    return -1;
}
bool32 ModsConditionExpiryNextAnimation(void)
{
    while (sConditionAnimationNext < sConditionAnimationCount) {
        const struct ConditionAnimation *action = &sConditionAnimations[sConditionAnimationNext++];
        if (!ConditionsAvailable() || action->battler >= gBattlersCount || action->battler >= MAX_BATTLERS_COUNT
            || (sPending & (1u << action->battler)) || !IsBattlerAlive(action->battler)) continue;
        struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
        if (Current(&battle) || battle.battle_id != action->battle_id || battle.turn != action->turn) continue;
        for (u32 i = 0; i < battle.participant_count; ++i) {
            const struct RhhBattleParticipant *p = &battle.participants[i];
            if (p->battler != action->battler || p->fainted || p->identity != action->identity
                || p->owner != action->owner || p->party_slot != action->party_slot || p->species != action->species) continue;
            sEffectRecipient = action->battler; gModEffectAnimation = action->animation; return TRUE;
        }
    }
    return FALSE;
}
bool32 ModsMoveEffectNextMessage(void)
{
    if (sEffectMessageNext >= sEffectMessageCount) { sEffectMessageText = sEmptyEffectMessage; return FALSE; }
    sEffectMessageText = sEffectMessages[sEffectMessageNext++]; return TRUE;
}
const u8 *ModsMoveEffectMessageText(void) { return sEffectMessageText; }
static void ClearEffectMessages(void)
{ sEffectMessageCount = sEffectMessageNext = 0; sEffectMessageText = sEmptyEffectMessage; }
static void ClearEffectActions(void)
{ ModsMoveEffectItemEnd(); sEffectActionCount = sEffectActionNext = 0; }
static char NameGlyph(u8 c)
{
    if (c >= CHAR_A && c <= CHAR_Z) return 'A' + c - CHAR_A;
    if (c >= CHAR_a && c <= CHAR_z) return 'a' + c - CHAR_a;
    if (c >= CHAR_0 && c <= CHAR_9) return '0' + c - CHAR_0;
    switch (c) {
    case CHAR_SPACE: return ' '; case CHAR_EXCL_MARK: return '!'; case CHAR_PERIOD: return '.';
    case CHAR_HYPHEN: return '-'; case CHAR_COLON: return ':'; case CHAR_COMMA: return ',';
    case CHAR_SLASH: return '/'; case CHAR_PLUS: return '+'; case CHAR_AMPERSAND: return '&';
    case CHAR_LEFT_PAREN: return '('; case CHAR_RIGHT_PAREN: return ')'; case CHAR_PERCENT: return '%';
    case CHAR_SGL_QUOTE_LEFT: case CHAR_SGL_QUOTE_RIGHT: return '\'';
    default: return '?';
    }
}
static int MoveName(RhhContentKey key, char *out, u32 capacity)
{
    if (!RhhModsFinalized() || !out || !capacity) return -1;
    char result[32]; u32 length = 0, move;
    const struct RhhModMove *custom = RhhModsFindCustomMove(key);
    if (custom)
    {
        if (!ModsCustomMoveAvailable(custom)) return -1;
        length = strlen(custom->name); if (length >= sizeof(result)) return -1;
        memcpy(result, custom->name, length);
    }
    else
    {
        if (RhhContentResolve(RHH_MOVE, key, &move) || !move || move >= MOVES_COUNT_ALL
            || GetMoveEffect(move) == EFFECT_PLACEHOLDER) return -1;
        const u8 *name = GetMoveName(move);
        while (length < sizeof(result) - 1 && name[length] != EOS)
        { result[length] = NameGlyph(name[length]); ++length; }
        if (name[length] != EOS) return -1;
    }
    result[length] = 0;
    if (capacity <= length) return -1;
    memcpy(out, result, length + 1); return 0;
}
static int CopyRegisteredEffect(const struct RhhModMoveEffect *effect, struct RhhMoveEffectInfo *out)
{
    if (!effect || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhMoveEffectInfo result = {.struct_size = sizeof(result), .mode = effect->mode,
        .key = RhhContentKeyOf(effect->id)};
    memcpy(result.id, effect->id, strlen(effect->id) + 1);
    memcpy(result.label, effect->label, strlen(effect->label) + 1);
    *out = result; return 0;
}
static int RegisteredEffectAt(u32 index, struct RhhMoveEffectInfo *out)
{ return CopyRegisteredEffect(RhhModsRegisteredEffectAt(index), out); }
static int RegisteredEffectInfo(RhhContentKey key, struct RhhMoveEffectInfo *out)
{ return CopyRegisteredEffect(RhhModsRegisteredEffect(key), out); }
static int RegisteredEffectForMove(RhhContentKey move, RhhContentKey *out)
{
    struct RhhMoveInfo info = {.struct_size = sizeof(info)};
    if (!out || MoveInfo(move, &info)) return -1;
    *out = RhhModsRegisteredEffectForMove(info.move); return 0;
}
static int MoveSelection(u32 battler, u32 slot, struct RhhMoveSelectionResult *out);
static int MoveSelectionContext(struct RhhMoveSelectionContext *out);
static int MovePPCostContext(struct RhhMovePPCostContext *out);
static int MovePPDeductionContext(struct RhhMovePPDeduction *out);
static int MoveDamageAppliedContext(struct RhhMoveDamageApplied *out);
static int MoveAccuracyCheckedContext(struct RhhMoveAccuracyCheck *out);
static int AiPPCostEstimate(struct RhhAiPPCostEstimate *out);
static int DamageStatContext(struct RhhDamageStatContext *out);
static int CriticalStageContext(struct RhhCriticalStageContext *out);
static int FinalDamageContext(struct RhhFinalDamageContext *out);
static int BeatUpContributor(struct RhhBeatUpContributor *out);
static const struct RhhMoveService sMoveService = {
    {sizeof(sMoveService), RHH_SERVICE_VERSION(1, 54)}, MoveInfo, LearnsetCount, LearnsetAt,
    ModsTeachMove, ModsTeachMoveContext, ModsPartyMoves, PowerContext, AccuracyContext, MoveName,
    ModsOfferMoveAdditionalEffect, ResolutionContext, AiScoreContext, MoveEffectModeQuery, EffectContext, EffectMessage, EffectHeal, EffectStatus, EffectStat,
    RhhModsRegisteredEffectCount, RegisteredEffectAt, RegisteredEffectInfo, RegisteredEffectForMove, EffectHealUser,
    EffectDamageContext, EffectHealUserFromDamage, EffectDrainUser, AiEvaluation,
    AiEffectOffer, AiEffectCount, AiEffectAt, AiEffectEstimate, MovePPCostContext, AiPPCostEstimate, MoveSelection, MoveSelectionContext,
    EffectHealBattler, AiEffectOfferHealBattler, AiEffectHealBattlerAt,
    EffectDamageBattler, AiEffectOfferDamageBattler, AiEffectDamageBattlerAt, EffectAnimation, ResolutionDamage,
    EffectStatBattler, AiEffectOfferStatBattler, AiEffectStatBattlerAt,
    EffectStatusBattler, AiEffectOfferStatusBattler, AiEffectStatusBattlerAt, MoveTargetContext, MoveTargetLock, ResolutionHealth,
    ResolutionHealthComparison, BattlerPriorityContext, DamageStatContext, MoveTargetPreview, MoveTargetFilter,
    MovePPDeductionContext, MoveDamageAppliedContext, MoveAccuracyCheckedContext,
    EffectDamageFromHit, AiEffectOfferDamageFromHit, AiEffectDamageFromHitAt, CriticalStageContext, EffectQueue, EffectQueueAt,
    FinalDamageContext, BeatUpContributor
};
static RhhContentKey BattleMoveKey(u32 move)
{
    return move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, move, sStabMoves, MOVES_COUNT_ALL)
                                 : RhhModsMoveKey(move);
}
static const struct RhhMovePPDeduction *sMovePPDeductionContext;
static int MovePPDeductionContext(struct RhhMovePPDeduction *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sMovePPDeductionContext
        || RhhModsCurrentEvent() != RHH_MOVE_PP_DEDUCTED) return -1;
    *out = *sMovePPDeductionContext; return 0;
}
void ModsMovePPDeducted(const struct BattleCalcValues *cv, u32 slot, u32 targetType, u32 base,
                        u32 cost, u32 before, u32 after, bool32 permanent)
{
    if (!cv || !RhhModsHasHook(RHH_MOVE_PP_DEDUCTED) || !ConditionsAvailable() || sMovePPDeductionContext
        || cv->battlerAtk >= gBattlersCount || cv->battlerAtk >= MAX_BATTLERS_COUNT || slot >= MAX_MON_MOVES
        || after > before || before > UINT8_MAX || cost > RHH_MOVE_PP_COST_MAX) return;
    const struct BattlePokemon *attacker = &gBattleMons[cv->battlerAtk];
    struct RhhMovePPDeduction context = {.struct_size = sizeof(context), .run_id = sRunId,
        .cost = cost, .paid = before - after, .pp_after = after, .controller_queued = !!permanent,
        .before = {.struct_size = sizeof(context.before), .base_cost = base, .pp_before = before,
            .attacker = cv->battlerAtk, .target = cv->battlerDef < gBattlersCount ? cv->battlerDef : RHH_BATTLE_NO_BATTLER,
            .move_slot = slot, .battle_id = sBattleId, .move = BattleMoveKey(cv->move),
            .charged_move = BattleMoveKey(attacker->moves[slot]),
            .attacker_species = CachedKey(RHH_SPECIES, attacker->species, sStabSpecies, NUM_SPECIES)}};
    if (permanent) context.before.flags |= RHH_MOVE_PP_COST_PERMANENT;
    if (gBattleStruct->submoveAnnouncement != SUBMOVE_NO_EFFECT) context.before.flags |= RHH_MOVE_PP_COST_SUBMOVE;
    if (IsSpreadMove(targetType) || targetType == TARGET_ALL_BATTLERS || targetType == TARGET_FIELD)
        context.before.flags |= RHH_MOVE_PP_COST_SPREAD;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_PP_DEDUCTED, .run_seed = RhhModsRunSeed(),
        .event_key = context.before.move, .subject = context.before.attacker_species,
        .args = {cv->battlerAtk, slot, context.paid, after}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sMovePPDeductionContext = &context; RhhModsDispatch(&event, &decision); sMovePPDeductionContext = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static const struct RhhMoveSelectionContext *sMoveSelectionContext;
static int MoveSelectionContext(struct RhhMoveSelectionContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sMoveSelectionContext
        || RhhModsCurrentEvent() != RHH_CAN_SELECT_MOVE) return -1;
    *out = *sMoveSelectionContext; return 0;
}
static int QueryMoveSelection(u32 battler, u32 slot, struct RhhMoveSelectionResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable() || sMoveSelectionContext
        || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || slot >= MAX_MON_MOVES) return -1;
    const struct BattlePokemon *mon = &gBattleMons[battler];
    u32 move = mon->moves[slot];
    if (!move || move == MOVE_STRUGGLE || move == MOVE_UNAVAILABLE) return -1;
    struct RhhMoveSelectionResult result = {.struct_size = sizeof(result), .allowed = TRUE,
        .context = {.struct_size = sizeof(result.context), .battler = battler, .move_slot = slot,
            .pp = mon->pp[slot], .level = mon->level, .hp = mon->hp, .max_hp = mon->maxHP,
            .turn = gBattleTurnCounter, .battle_id = sBattleId, .move = BattleMoveKey(move),
            .species = CachedKey(RHH_SPECIES, mon->species, sStabSpecies, NUM_SPECIES)}};
    if (!result.context.move || !result.context.species) return -1;
    if (RhhModsHasHook(RHH_CAN_SELECT_MOVE))
    {
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_CAN_SELECT_MOVE,
            .run_seed = RhhModsRunSeed(), .event_key = result.context.move, .subject = result.context.species,
            .args = {battler, slot, result.context.pp, result.context.turn}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        sMoveSelectionContext = &result.context; RhhModsDispatch(&event, &decision); sMoveSelectionContext = NULL;
        result.allowed = !!decision.allowed;
        if (!result.allowed) memcpy(result.reason, decision.reason, sizeof(result.reason));
        result.reason[sizeof(result.reason) - 1] = 0;
    }
    *out = result; return 0;
}
static int MoveSelection(u32 battler, u32 slot, struct RhhMoveSelectionResult *out)
{
    /* An AI scoring reducer must not use this service to inspect an
     * opponent's unrevealed moves or PP. Engine legality still uses the
     * internal query while preparing its own move masks. */
    if (!ConditionsAvailable() || battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT
        || GetBattlerTrainer(battler) != B_TRAINER_PLAYER || !RhhModsCommandsAllowed() || sAiScoreContext
        || (gAiLogicData && (gAiLogicData->aiCalcInProgress || gAiLogicData->aiPredictionInProgress))) return -1;
    return QueryMoveSelection(battler, slot, out);
}
enum { MOVE_SELECTION_REASON_BYTES = 97, MOVE_SELECTION_LINE_WIDTH = 200 };
static u8 sMoveSelectionMessages[MAX_BATTLERS_COUNT][MOVE_SELECTION_REASON_BYTES];
static bool8 sMoveSelectionMessageValid[MAX_BATTLERS_COUNT];
/* Convert supported ASCII and wrap at word boundaries. Reject text that
 * cannot fit two battle-window lines rather than cutting off its meaning. */
static bool32 EncodeMoveSelectionMessage(const char *reason, u8 *out)
{
    u8 word[MOVE_SELECTION_REASON_BYTES];
    u32 pos = 0, length = 0, line = 0, lineWidth = 0;
    u32 spaceWidth = GetGlyphWidth(CHAR_SPACE, FALSE, FONT_NORMAL) + 1;
    while (reason[pos])
    {
        bool32 explicitLine = FALSE;
        while (reason[pos] == ' ' || reason[pos] == '\n')
        {
            if (reason[pos] == '\n')
            {
                if (!length || explicitLine) return FALSE;
                explicitLine = TRUE;
            }
            ++pos;
        }
        if (!reason[pos]) { if (explicitLine) return FALSE; break; }
        u32 wordLength = 0, wordWidth = 0;
        while (reason[pos] && reason[pos] != ' ' && reason[pos] != '\n')
        {
            int glyph = EffectMessageGlyph((unsigned char)reason[pos++]);
            if (glyph < 0 || wordLength >= sizeof(word)) return FALSE;
            word[wordLength++] = glyph;
            wordWidth += GetGlyphWidth(glyph, FALSE, FONT_NORMAL) + 1;
        }
        if (!wordLength || wordWidth > MOVE_SELECTION_LINE_WIDTH) return FALSE;
        if (length)
        {
            if (explicitLine || lineWidth + spaceWidth + wordWidth > MOVE_SELECTION_LINE_WIDTH)
            {
                if (line++) return FALSE;
                if (length + 1 >= MOVE_SELECTION_REASON_BYTES) return FALSE;
                out[length++] = CHAR_NEWLINE; lineWidth = 0;
            }
            else
            {
                if (length + 1 >= MOVE_SELECTION_REASON_BYTES) return FALSE;
                out[length++] = CHAR_SPACE; lineWidth += spaceWidth;
            }
        }
        if (length + wordLength >= MOVE_SELECTION_REASON_BYTES) return FALSE;
        memcpy(out + length, word, wordLength); length += wordLength; lineWidth += wordWidth;
    }
    if (!length || length + 1 >= MOVE_SELECTION_REASON_BYTES) return FALSE;
    out[length++] = CHAR_PROMPT_CLEAR;
    out[length] = EOS; return TRUE;
}
const u8 *ModsMoveSelectionMessage(u32 battler)
{
    return battler < MAX_BATTLERS_COUNT && sMoveSelectionMessageValid[battler]
        ? sMoveSelectionMessages[battler] : NULL;
}
bool32 ModsPrepareMoveSelection(u32 battler, u32 slot)
{
    if (battler >= MAX_BATTLERS_COUNT) return TRUE;
    sMoveSelectionMessageValid[battler] = FALSE;
    if (!RhhModsHasHook(RHH_CAN_SELECT_MOVE)) return TRUE;
    struct RhhMoveSelectionResult result = {.struct_size = sizeof(result)};
    if (QueryMoveSelection(battler, slot, &result) || result.allowed) return TRUE;
    sMoveSelectionMessageValid[battler] = EncodeMoveSelectionMessage(result.reason, sMoveSelectionMessages[battler]);
    return FALSE;
}
bool32 ModsCanSelectMove(u32 battler, u32 slot)
{
    if (!RhhModsHasHook(RHH_CAN_SELECT_MOVE)) return TRUE;
    struct RhhMoveSelectionResult result = {.struct_size = sizeof(result)};
    return QueryMoveSelection(battler, slot, &result) || result.allowed;
}
static const struct RhhMovePPCostContext *sMovePPCostContext;
static int MovePPCostContext(struct RhhMovePPCostContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sMovePPCostContext
        || (RhhModsCurrentEvent() != RHH_MOVE_PP_COST && RhhModsCurrentEvent() != RHH_AI_MOVE_PP_COST)) return -1;
    *out = *sMovePPCostContext; return 0;
}
static u32 ResolveMovePPCost(const struct RhhMovePPCostContext *context, u32 eventKind)
{
    if (!context->move || !context->charged_move || !context->attacker_species
        || !RhhModsHasHook(eventKind) || sMovePPCostContext) return context->base_cost;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = eventKind,
        .run_seed = RhhModsRunSeed(), .event_key = context->move, .subject = context->attacker_species,
        .args = {context->base_cost, context->attacker, context->target, context->move_slot}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = context->base_cost};
    sMovePPCostContext = context; RhhModsDispatch(&event, &decision); sMovePPCostContext = NULL;
    return decision.allowed && decision.value <= RHH_MOVE_PP_COST_MAX ? decision.value : context->base_cost;
}
u32 ModsMovePPCost(const struct BattleCalcValues *cv, u32 slot, u32 moveTarget, u32 base)
{
    if (!cv || !ConditionsAvailable() || !RhhModsHasHook(RHH_MOVE_PP_COST) || sMovePPCostContext
        || cv->battlerAtk >= gBattlersCount || cv->battlerAtk >= MAX_BATTLERS_COUNT || slot >= MAX_MON_MOVES
        || cv->move == MOVE_STRUGGLE || base > RHH_MOVE_PP_COST_MAX) return base;
    const struct BattlePokemon *attacker = &gBattleMons[cv->battlerAtk];
    struct RhhMovePPCostContext context = {.struct_size = sizeof(context), .base_cost = base,
        .pp_before = attacker->pp[slot], .attacker = cv->battlerAtk,
        .target = cv->battlerDef < gBattlersCount ? cv->battlerDef : RHH_BATTLE_NO_BATTLER,
        .move_slot = slot, .battle_id = sBattleId, .move = BattleMoveKey(cv->move),
        .charged_move = BattleMoveKey(attacker->moves[slot]),
        .attacker_species = CachedKey(RHH_SPECIES, attacker->species, sStabSpecies, NUM_SPECIES)};
    if (MOVE_IS_PERMANENT(cv->battlerAtk, slot)) context.flags |= RHH_MOVE_PP_COST_PERMANENT;
    if (gBattleStruct->submoveAnnouncement != SUBMOVE_NO_EFFECT) context.flags |= RHH_MOVE_PP_COST_SUBMOVE;
    if (IsSpreadMove(moveTarget) || moveTarget == TARGET_ALL_BATTLERS || moveTarget == TARGET_FIELD)
        context.flags |= RHH_MOVE_PP_COST_SPREAD;
    return ResolveMovePPCost(&context, RHH_MOVE_PP_COST);
}
static bool32 MoveMayCallAnother(u32 move)
{
    switch (GetMoveEffect(move))
    {
    case EFFECT_MIRROR_MOVE: case EFFECT_METRONOME: case EFFECT_ASSIST:
    case EFFECT_NATURE_POWER: case EFFECT_SLEEP_TALK: case EFFECT_COPYCAT: case EFFECT_ME_FIRST:
        return TRUE;
    default: return FALSE;
    }
}
static int AiPPCostEstimate(struct RhhAiPPCostEstimate *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable() || !sAiScoreContext
        || !sAiEvaluation || !gAiLogicData || sMovePPCostContext || !IsAiContextEvent(RhhModsCurrentEvent())) return -1;
    u32 attacker = sAiScoreContext->attacker, candidate = sAiScoreContext->candidate, slot = sAiScoreContext->move_slot;
    if (attacker >= gBattlersCount || candidate >= gBattlersCount || slot >= MAX_MON_MOVES) return -1;
    const struct BattlePokemon *mon = &gBattleMons[attacker];
    u32 move = mon->moves[slot];
    if (!move || BattleMoveKey(move) != sAiScoreContext->move)
    {
        if (BattleMoveKey(MOVE_STRUGGLE) != sAiScoreContext->move) return -1;
        move = MOVE_STRUGGLE;
    }
    bool32 skipped = move == MOVE_STRUGGLE || mon->volatiles.multipleTurns || mon->volatiles.bideTurns;
    u32 targetType = AI_GetBattlerMoveTargetType(attacker, move), target = candidate, base = skipped ? 0 : 1;
    struct RhhAiPPCostEstimate result = {.struct_size = sizeof(result), .pp_before = mon->pp[slot]};
    if (skipped) result.flags |= RHH_AI_PP_DEDUCTION_SKIPPED;
    if (RhhModsHasHook(RHH_MOVE_PP_COST)) result.flags |= RHH_AI_PP_COMMIT_RULES_MAY_DIFFER;
    if (MoveMayCallAnother(move)) result.flags |= RHH_AI_PP_CALLED_MOVE;
    if (targetType == TARGET_USER) target = attacker;
    else if (targetType == TARGET_ALLY) target = GetPartnerBattler(attacker);
    else result.flags |= RHH_AI_PP_ASSUMED_TARGET;
    bool32 spread = IsSpreadMove(targetType) || targetType == TARGET_ALL_BATTLERS || targetType == TARGET_FIELD;
    if (base && (spread || MoveForcesPressure(move)))
    {
        for (u32 i = 0; i < gBattlersCount; ++i)
            if (!IsBattlerAlly(i, attacker))
            {
                enum Ability ability = gAiLogicData->abilities[i];
                if (ability == ABILITY_NONE) result.flags |= RHH_AI_PP_UNKNOWN_ABILITY;
                if (ability == ABILITY_PRESSURE) ++base;
            }
    }
    else if (base && targetType != TARGET_OPPONENTS_FIELD && target != attacker)
    {
        if (target >= gBattlersCount) result.flags |= RHH_AI_PP_ASSUMED_TARGET;
        else
        {
            enum Ability ability = gAiLogicData->abilities[target];
            if (ability == ABILITY_NONE) result.flags |= RHH_AI_PP_UNKNOWN_ABILITY;
            if (ability == ABILITY_PRESSURE) ++base;
        }
    }
    struct RhhMovePPCostContext context = {.struct_size = sizeof(context), .flags = RHH_MOVE_PP_COST_AI,
        .base_cost = base, .pp_before = mon->pp[slot], .attacker = attacker,
        .target = target < gBattlersCount ? target : RHH_BATTLE_NO_BATTLER, .move_slot = slot,
        .battle_id = sBattleId, .move = sAiScoreContext->move, .charged_move = BattleMoveKey(move),
        .attacker_species = sAiScoreContext->attacker_species};
    if (MOVE_IS_PERMANENT(attacker, slot)) context.flags |= RHH_MOVE_PP_COST_PERMANENT;
    if (spread) context.flags |= RHH_MOVE_PP_COST_SPREAD;
    result.base_cost = base;
    result.cost = base ? ResolveMovePPCost(&context, RHH_AI_MOVE_PP_COST) : 0;
    result.paid = min(result.cost, result.pp_before); result.pp_after = result.pp_before - result.paid;
    *out = result; return 0;
}
static struct MoveDamageIdentity MoveDamageIdentity(u32 battler)
{
    return (struct MoveDamageIdentity){.identity = (u64)gBattleMons[battler].otId << 32 | gBattleMons[battler].personality,
        .owner = GetBattlerTrainer(battler), .partySlot = gBattlerPartyIndexes[battler], .species = gBattleMons[battler].species};
}
static const struct RhhMoveDamageApplied *sMoveDamageAppliedContext;
static u64 sMoveDamageAppliedSequence;
static const struct RhhMoveAccuracyCheck *sMoveAccuracyCheckedContext;
static u64 sMoveAccuracyCheckedSequence;
static u32 ResolutionTargetFlags(u32 flags);
static int MoveAccuracyCheckedContext(struct RhhMoveAccuracyCheck *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sMoveAccuracyCheckedContext
        || RhhModsCurrentEvent() != RHH_MOVE_ACCURACY_CHECKED) return -1;
    *out = *sMoveAccuracyCheckedContext; return 0;
}
void ModsMoveAccuracyChecked(const struct BattleCalcValues *cv, bool32 missed)
{
    if (!cv || !RhhModsHasHook(RHH_MOVE_ACCURACY_CHECKED) || !ConditionsAvailable() || sMoveAccuracyCheckedContext
        || cv->battlerAtk >= gBattlersCount || cv->battlerDef >= gBattlersCount
        || cv->battlerAtk >= MAX_BATTLERS_COUNT || cv->battlerDef >= MAX_BATTLERS_COUNT) return;
    u32 raw = gBattleStruct->moveResultFlags[cv->battlerDef];
    struct MoveDamageIdentity source = MoveDamageIdentity(cv->battlerAtk), target = MoveDamageIdentity(cv->battlerDef);
    struct RhhMoveAccuracyCheck context = {.struct_size = sizeof(context), .attacker = cv->battlerAtk,
        .target = cv->battlerDef, .turn = gBattleTurnCounter, .missed = !!missed,
        .target_result = ResolutionTargetFlags(raw), .run_id = sRunId, .battle_id = sBattleId,
        .sequence = sMoveAccuracyCheckedSequence++, .attacker_identity = source.identity, .target_identity = target.identity,
        .move = BattleMoveKey(cv->move), .attacker_species = RhhContentKeyForId(RHH_SPECIES, source.species),
        .target_species = RhhContentKeyForId(RHH_SPECIES, target.species)};
    if (cv->moveEffect == EFFECT_OHKO) context.flags |= RHH_ACCURACY_CHECK_OHKO;
    if (raw & MOVE_RESULT_ONE_HIT_KO_STURDY) context.flags |= RHH_ACCURACY_CHECK_ENGINE_STURDY_FLAG;
    if (raw & MOVE_RESULT_ONE_HIT_KO_NO_AFFECT) context.flags |= RHH_ACCURACY_CHECK_ENGINE_OHKO_NO_EFFECT_FLAG;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_ACCURACY_CHECKED,
        .run_seed = RhhModsRunSeed(), .event_key = context.move, .subject = context.target_species,
        .ordinal = context.sequence, .args = {context.attacker, context.target, context.missed, context.flags}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sMoveAccuracyCheckedContext = &context; RhhModsDispatch(&event, &decision); sMoveAccuracyCheckedContext = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static int MoveDamageAppliedContext(struct RhhMoveDamageApplied *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sMoveDamageAppliedContext
        || RhhModsCurrentEvent() != RHH_MOVE_DAMAGE_APPLIED) return -1;
    *out = *sMoveDamageAppliedContext; return 0;
}
static bool32 MoveDamageIdentityEqual(const struct MoveDamageIdentity *a, const struct MoveDamageIdentity *b)
{
    return a->identity == b->identity && a->owner == b->owner && a->partySlot == b->partySlot && a->species == b->species;
}
static bool32 MoveHealthEndpoint(u32 battler, struct MoveHealthEndpoint *out)
{
    *out = (struct MoveHealthEndpoint){0};
    if (battler >= gBattlersCount || battler >= MAX_BATTLERS_COUNT || (gAbsentBattlerFlags & (1u << battler))
        || (sPending & (1u << battler)) || gBattleStruct->battlerState[battler].notOnField
        || gBattlerPartyIndexes[battler] >= PARTY_SIZE || !gBattleMons[battler].species
        || GetBattlerTrainer(battler) >= MAX_BATTLE_TRAINERS) return FALSE;
    *out = (struct MoveHealthEndpoint){.valid = TRUE, .hp = gBattleMons[battler].hp,
        .maxHp = gBattleMons[battler].maxHP, .identity = MoveDamageIdentity(battler)};
    return TRUE;
}
void ModsRecordMoveHealth(u32 target, u32 before, u32 after, bool32 scripted)
{
    if (before == after || !sMoveHealthWindow.active || !RhhModsHasHook(RHH_MOVE_RESOLVED)
        || !ConditionsAvailable() || target >= gBattlersCount || target >= MAX_BATTLERS_COUNT
        || sMoveHealthWindow.attacker >= gBattlersCount) return;
    struct MoveHealthAggregate *sum = &sMoveHealth[target];
    struct MoveDamageIdentity source = MoveDamageIdentity(sMoveHealthWindow.attacker), recipient = MoveDamageIdentity(target);
    if (!sum->valid) { sum->valid = TRUE; sum->target = recipient; }
    else if (!MoveDamageIdentityEqual(&sum->target, &recipient)) sum->flags |= RHH_MOVE_HEALTH_TARGET_CHANGED;
    if (!MoveDamageIdentityEqual(&sMoveHealthWindow.source, &source) || gBattlerAttacker != sMoveHealthWindow.attacker)
        sum->flags |= RHH_MOVE_HEALTH_SOURCE_CHANGED;
    if (gCurrentMove != sMoveHealthWindow.move) sum->flags |= RHH_MOVE_HEALTH_MIXED_MOVE;
    u64 amount = before > after ? before - after : after - before;
    u64 *total = scripted ? (before > after ? &sum->scriptedLost : &sum->scriptedRestored)
                          : (before > after ? &sum->directLost : &sum->directRestored);
    *total = UINT64_MAX - *total < amount ? UINT64_MAX : *total + amount;
    u32 *updates = scripted ? &sum->scriptedUpdates : &sum->directUpdates;
    if (*updates != UINT32_MAX) ++*updates;
}
void ModsRecordMoveDamage(u32 attacker, u32 target, u32 move, u32 hpLost, u32 substituteLost)
{
    if (!sActive || target >= MAX_BATTLERS_COUNT || attacker >= MAX_BATTLERS_COUNT) return;
    sMoveDamage[target] = (struct MoveDamageRecord){attacker, move, hpLost, substituteLost, TRUE};
    if (!RhhModsHasHook(RHH_MOVE_RESOLVED) && !RhhModsHasHook(RHH_MOVE_DAMAGE_APPLIED)) return;
    if (!ConditionsAvailable() || attacker >= gBattlersCount || target >= gBattlersCount) return;
    struct MoveDamageIdentity source = MoveDamageIdentity(attacker), recipient = MoveDamageIdentity(target);
    if (RhhModsHasHook(RHH_MOVE_RESOLVED))
    {
        struct MoveDamageAggregate *sum = &sMoveDamageAggregate[target];
        if (!sum->valid)
            *sum = (struct MoveDamageAggregate){.valid = TRUE, .attacker = attacker, .move = move, .source = source, .target = recipient};
        else
        {
            if (sum->attacker != attacker || sum->move != move) sum->flags |= RHH_MOVE_RESOLUTION_DAMAGE_MIXED_SOURCE;
            if (!MoveDamageIdentityEqual(&sum->source, &source)) sum->flags |= RHH_MOVE_RESOLUTION_DAMAGE_ATTACKER_CHANGED;
            if (!MoveDamageIdentityEqual(&sum->target, &recipient)) sum->flags |= RHH_MOVE_RESOLUTION_DAMAGE_TARGET_CHANGED;
        }
        sum->hp = UINT64_MAX - sum->hp < hpLost ? UINT64_MAX : sum->hp + hpLost;
        sum->substitute = UINT64_MAX - sum->substitute < substituteLost ? UINT64_MAX : sum->substitute + substituteLost;
        if (sum->updates < UINT32_MAX) ++sum->updates;
    }
    if ((!hpLost && !substituteLost) || !RhhModsHasHook(RHH_MOVE_DAMAGE_APPLIED) || sMoveDamageAppliedContext) return;
    struct RhhMoveDamageApplied context = {.struct_size = sizeof(context), .attacker = attacker, .target = target,
        .turn = gBattleTurnCounter, .hp_lost = hpLost, .substitute_lost = substituteLost,
        .target_hp = gBattleMons[target].hp, .target_max_hp = gBattleMons[target].maxHP,
        .substitute_hp = gBattleMons[target].volatiles.substituteHP,
        .attacker_owner = source.owner, .attacker_party_slot = source.partySlot,
        .target_owner = recipient.owner, .target_party_slot = recipient.partySlot,
        .run_id = sRunId, .battle_id = sBattleId, .sequence = sMoveDamageAppliedSequence++,
        .attacker_identity = source.identity, .target_identity = recipient.identity,
        .move = BattleMoveKey(move), .attacker_species = RhhContentKeyForId(RHH_SPECIES, source.species),
        .target_species = RhhContentKeyForId(RHH_SPECIES, recipient.species)};
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_DAMAGE_APPLIED,
        .run_seed = RhhModsRunSeed(), .event_key = context.move, .subject = context.target_species,
        .ordinal = context.sequence, .args = {attacker, target, hpLost, substituteLost}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sMoveDamageAppliedContext = &context; RhhModsDispatch(&event, &decision); sMoveDamageAppliedContext = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static bool32 ApplyMoveEffect(bool32 damage)
{
    ClearEffectMessages();
    ClearEffectActions();
    sDamageEffectScript = damage;
    u32 attacker = gBattlerAttacker, target = gBattlerTarget;
    if (!sActive || sEnding || !gMain.inBattle || !gBattleStruct || gBattleOutcome || !RhhModsHasActiveRun()
        || sRunId != RhhModsRunId() || sEffectContext
        || (!RhhModsHasHook(damage ? RHH_MOVE_DAMAGE_EFFECT_APPLY : RHH_MOVE_EFFECT_APPLY)
            && !RhhModsHasHook(RHH_REGISTERED_MOVE_EFFECT))
        || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))
        || attacker >= gBattlersCount || target >= gBattlersCount || attacker >= MAX_BATTLERS_COUNT || target >= MAX_BATTLERS_COUNT
        || gBattleStruct->unableToUseMove || gProtectStructs[attacker].chargingTurn
        || gBattleStruct->battlerState[attacker].originalBattlerPartyId != PARTY_SIZE
        || (damage ? (GetMoveEffect(gCurrentMove) != EFFECT_HIT || gCurrentMove == MOVE_STRUGGLE
                      || GetMoveCategory(gCurrentMove) == DAMAGE_CATEGORY_STATUS)
                   : GetMoveEffect(gCurrentMove) != EFFECT_MOD_CUSTOM_STATUS)) return FALSE;
    u32 moveTarget = GetBattlerMoveTargetType(attacker, gCurrentMove);
    if (gBattleStruct->moveResultFlags[target] & MOVE_RESULT_NO_EFFECT) return FALSE;
    if (moveTarget != TARGET_FIELD && moveTarget != TARGET_OPPONENTS_FIELD
        && ((!damage && !IsBattlerAlive(target)) || (gBattleStruct->moveResultFlags[target] & MOVE_RESULT_NOT_PRESENT))) return FALSE;
    RhhContentKey move = BattleMoveKey(gCurrentMove);
    if (!move || (damage && AdjustMoveEffectMode(move, TRUE) != RHH_MOVE_EFFECT_CUSTOM_DAMAGE)) return FALSE;
    if (sEffectTargets & (1u << target)) return FALSE;
    bool32 first = !sEffectTargets;
    sEffectTargets |= 1u << target;
    struct RhhMoveEffectApplyContext context = {.struct_size = sizeof(context),
        .flags = first ? RHH_MOVE_EFFECT_FIRST_TARGET : 0, .attacker = attacker, .target = target,
        .turn = gBattleTurnCounter, .move_target = moveTarget < ARRAY_COUNT(sPublicMoveTargets) ? sPublicMoveTargets[moveTarget] : RHH_MOVE_TARGET_NONE,
        .run_id = sRunId, .battle_id = sBattleId, .sequence = sEffectSequence++,
        .move = move, .chosen_move = BattleMoveKey(gChosenMove)};
    struct RhhMoveDamageEffectContext damageContext = {.struct_size = sizeof(damageContext),
        .attacker = attacker, .target = target, .run_id = sRunId, .battle_id = sBattleId,
        .sequence = context.sequence, .move = move};
    if (damage && sMoveDamage[target].valid && sMoveDamage[target].attacker == attacker
        && sMoveDamage[target].move == gCurrentMove)
    {
        damageContext.flags = RHH_MOVE_DAMAGE_RECORDED;
        damageContext.hp_lost = sMoveDamage[target].hp;
        damageContext.substitute_lost = sMoveDamage[target].substitute;
    }
    u32 eventKind = damage ? RHH_MOVE_DAMAGE_EFFECT_APPLY : RHH_MOVE_EFFECT_APPLY;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = eventKind,
        .run_seed = RhhModsRunSeed(), .event_key = move, .ordinal = context.sequence,
        .args = {attacker, target, context.flags, context.turn}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sEffectContext = &context;
    sDamageContext = damage ? &damageContext : NULL;
    RhhContentKey effect = RhhModsRegisteredEffectForMove(move);
    if (effect && RhhModsHasHook(RHH_REGISTERED_MOVE_EFFECT))
    {
        struct RhhModEvent registered = event;
        registered.kind = RHH_REGISTERED_MOVE_EFFECT; registered.event_key = effect; registered.subject = move;
        RhhModsDispatch(&registered, &decision);
    }
    if (RhhModsHasHook(eventKind)) RhhModsDispatch(&event, &decision);
    sEffectContext = NULL;
    sDamageContext = NULL;
    return TRUE;
}
void ModsMoveEffectApply(void) { ApplyMoveEffect(FALSE); }
bool32 ModsMoveDamageEffectApply(void) { return ApplyMoveEffect(TRUE); }
bool32 ModsMoveEffectIsDamage(void) { return sDamageEffectScript; }
void ModsMoveEffectReset(void) { memset(sMoveTargetLocks, 0, sizeof(sMoveTargetLocks)); sMoveTargetCapture = (struct MoveTargetCapture){0}; sMoveTargetCaptureEligible = FALSE;
    sEffectTargets = 0; memset(sMoveDamage, 0, sizeof(sMoveDamage));
    memset(sMoveDamageAggregate, 0, sizeof(sMoveDamageAggregate));
    memset(sMoveHealth, 0, sizeof(sMoveHealth)); memset(&sMoveHealthWindow, 0, sizeof(sMoveHealthWindow));
    ClearEffectMessages(); ClearEffectActions(); }
bool32 ModsAiMoveScoreEnabled(void)
{
    return sActive && gMain.inBattle && gBattleStruct && RhhModsHasActiveRun()
        && sRunId == RhhModsRunId() && !sAiScoreContext
        && (RhhModsHasHook(RHH_AI_MOVE_SCORE) || RhhModsHasHook(RHH_AI_EFFECT_PREVIEW))
        && !(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK));
}
static s32 AiEffectUtility(const struct RhhAiEffectAction *action, const struct RhhAiEffectEstimate *estimate)
{
    if (estimate->flags & RHH_AI_ESTIMATE_BLOCKED || !estimate->median) return 0;
    if (action->kind == RHH_AI_EFFECT_CONDITION) return 0; /* Effects have no declared benefit polarity. */
    u32 attacker = sAiScoreContext->attacker, recipient = estimate->recipient;
    bool32 ally = GetBattlerSide(attacker) == GetBattlerSide(recipient);
    s32 benefit = 0;
    if (action->kind == RHH_AI_EFFECT_STATUS_TARGET || action->kind == RHH_AI_EFFECT_STATUS_BATTLER) benefit = ally ? -10 : 10;
    else if (action->kind == RHH_AI_EFFECT_STAT_TARGET || action->kind == RHH_AI_EFFECT_STAT_BATTLER)
    {
        s32 magnitude = min(15, 5 * abs(estimate->median));
        benefit = (estimate->median > 0) == ally ? magnitude : -magnitude;
    }
    else if (estimate->median < 0) benefit = ally ? -15 : 15;
    else if (!ally) benefit = -15;
    else if (gAiLogicData->hpPercents[recipient] < 70) benefit = 15;
    u32 accuracy = sAiEvaluation->accuracy;
    if (!accuracy || accuracy > 100) accuracy = 100; /* Unknown or bypassed: do not discard a possible benefit. */
    s32 weighted = abs(benefit) * action->chance * accuracy / 10000;
    return benefit < 0 ? -weighted : weighted;
}
static s32 AiAutomaticEffectScore(const struct AiEffectCollector *collector, s32 score)
{
    u32 attacker = sAiScoreContext->attacker, candidate = sAiScoreContext->candidate;
    s32 adjustment = 0;
    for (u32 i = 0; i < collector->count; ++i)
    {
        const struct RhhAiEffectAction *action = &collector->actions[i];
        if (!(action->flags & RHH_AI_EFFECT_AUTO_SCORE) || action->kind == RHH_AI_EFFECT_CONDITION
            || action->kind == RHH_AI_EFFECT_FIELD_RULE || action->kind == RHH_AI_EFFECT_ROOM_TOGGLE) continue;
        bool32 user = action->kind == RHH_AI_EFFECT_HEAL_USER || action->kind == RHH_AI_EFFECT_HEAL_USER_FROM_DAMAGE
            || action->kind == RHH_AI_EFFECT_DRAIN_USER;
        bool32 fixed = action->kind == RHH_AI_EFFECT_HEAL_BATTLER || action->kind == RHH_AI_EFFECT_DAMAGE_BATTLER
            || action->kind == RHH_AI_EFFECT_DAMAGE_FROM_HIT_BATTLER
            || action->kind == RHH_AI_EFFECT_STAT_BATTLER || action->kind == RHH_AI_EFFECT_STATUS_BATTLER;
        u32 target = fixed ? collector->recipients[i] : user ? attacker : candidate;
        if (!user && !fixed && (sAiScoreContext->move_target == RHH_MOVE_TARGET_USER
                      || sAiScoreContext->move_target == RHH_MOVE_TARGET_USER_AND_ALLY
                      || sAiScoreContext->move_target == RHH_MOVE_TARGET_USER_OR_ALLY)) target = attacker;
        else if (!user && !fixed && sAiScoreContext->move_target == RHH_MOVE_TARGET_ALLY) target = GetPartnerBattler(attacker);
        struct RhhAiEffectEstimate estimate;
        if (!EstimateAiEffect(collector, i, target, &estimate)) adjustment += AiEffectUtility(action, &estimate);
        if (!user && !fixed && sAiScoreContext->move_target == RHH_MOVE_TARGET_USER_AND_ALLY)
        {
            u32 partner = GetPartnerBattler(attacker);
            if (partner < gBattlersCount && partner != attacker && IsBattlerAlive(partner)
                && !EstimateAiEffect(collector, i, partner, &estimate)) adjustment += AiEffectUtility(action, &estimate);
        }
    }
    return max(0, min(score + adjustment, 255));
}
s32 ModsAiMoveScore(u32 attacker, u32 candidate, u32 move, u32 moveSlot, s32 score)
{
    if (!ModsAiMoveScoreEnabled() || !gAiLogicData || score <= 0 || score > 255 || moveSlot >= MAX_MON_MOVES
        || attacker >= gBattlersCount || candidate >= gBattlersCount
        || attacker >= MAX_BATTLERS_COUNT || candidate >= MAX_BATTLERS_COUNT) return score;
    RhhContentKey moveKey = BattleMoveKey(move);
    const struct BattlePokemon *a = &gBattleMons[attacker], *d = &gBattleMons[candidate];
    u32 target = AI_GetBattlerMoveTargetType(attacker, move);
    struct RhhAiMoveScoreContext context = {.struct_size = sizeof(context), .score = score,
        .attacker = attacker, .candidate = candidate, .move_slot = moveSlot,
        .move_target = target < ARRAY_COUNT(sPublicMoveTargets) ? sPublicMoveTargets[target] : RHH_MOVE_TARGET_NONE,
        .turn = gBattleTurnCounter, .run_id = sRunId, .battle_id = sBattleId, .move = moveKey,
        .attacker_species = CachedKey(RHH_SPECIES, a->species, sStabSpecies, NUM_SPECIES),
        .candidate_species = CachedKey(RHH_SPECIES, d->species, sStabSpecies, NUM_SPECIES)};
    if (!context.move || !context.attacker_species || !context.candidate_species) return score;
    const struct SimulatedDamage *damage = &gAiLogicData->simulatedDmg[attacker][candidate][moveSlot];
    struct RhhAiMoveEvaluation evaluation = {.struct_size = sizeof(evaluation),
        .flags = (a->volatiles.healBlockTimer ? RHH_AI_EVAL_HEAL_BLOCKED : 0)
            | (d->volatiles.substitute ? RHH_AI_EVAL_CANDIDATE_SUBSTITUTE : 0)
            | (gAiLogicData->partnerMoveSimulation ? RHH_AI_EVAL_PARTNER_SIMULATION : 0),
        .attacker = attacker, .candidate = candidate, .move_slot = moveSlot,
        .damage_min = damage->minimum, .damage_median = damage->median,
        .damage_max = damage->maximum, .damage_random = damage->random,
        .accuracy = gAiLogicData->moveAccuracy[attacker][candidate][moveSlot],
        .type_effectiveness = gAiLogicData->effectiveness[attacker][candidate][moveSlot],
        .attacker_hp_percent = gAiLogicData->hpPercents[attacker],
        .candidate_hp_percent = gAiLogicData->hpPercents[candidate],
        .run_id = sRunId, .battle_id = sBattleId, .move = moveKey,
        .attacker_ability = CachedKey(RHH_ABILITY, gAiLogicData->abilities[attacker], sStabAbilities, ABILITIES_COUNT),
        .candidate_ability = CachedKey(RHH_ABILITY, gAiLogicData->abilities[candidate], sStabAbilities, ABILITIES_COUNT)};
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_AI_MOVE_SCORE,
        .run_seed = RhhModsRunSeed(), .event_key = context.move, .subject = context.attacker_species,
        .args = {score, attacker, candidate, moveSlot}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = score};
    struct AiEffectCollector effects = {.move = move};
    sAiScoreContext = &context; sAiEvaluation = &evaluation; sAiEffects = &effects;
    if (RhhModsHasHook(RHH_AI_EFFECT_PREVIEW))
    {
        u32 mode = RHH_MOVE_EFFECT_INHERITED;
        if (!MoveEffectModeQuery(context.move, &mode) && mode != RHH_MOVE_EFFECT_INHERITED)
        {
            effects.mode = mode;
            struct RhhModEvent preview = event;
            preview.kind = RHH_AI_EFFECT_PREVIEW;
            preview.subject = RhhModsRegisteredEffectForMove(context.move);
            struct RhhModDecision collected = {.struct_size = sizeof(collected), .allowed = 1};
            RhhModsDispatch(&preview, &collected);
            if (!collected.allowed) effects.count = 0;
        }
    }
    score = AiAutomaticEffectScore(&effects, score);
    if (score > 0)
    {
        context.score = score; event.args[0] = score; decision.value = score;
        RhhModsDispatch(&event, &decision);
    }
    sAiEffects = NULL; sAiEvaluation = NULL; sAiScoreContext = NULL;
    if (!score) return 0;
    return decision.allowed && decision.value <= 255 ? decision.value : score;
}
static u32 ResolutionTargetFlags(u32 flags)
{
    u32 result = 0;
    if (flags & MOVE_RESULT_MISSED) result |= RHH_MOVE_RESULT_MISSED;
    if (flags & MOVE_RESULT_FAILED) result |= RHH_MOVE_RESULT_FAILED;
    if (flags & MOVE_RESULT_PROTECTED) result |= RHH_MOVE_RESULT_PROTECTED;
    if (flags & MOVE_RESULT_DOESNT_AFFECT_FOE) result |= RHH_MOVE_RESULT_UNAFFECTED;
    if (flags & MOVE_RESULT_NOT_PRESENT) result |= RHH_MOVE_RESULT_NOT_PRESENT;
    if (flags & MOVE_RESULT_STAT_CHANGE_PREVENTED) result |= RHH_MOVE_RESULT_STAT_PREVENTED;
    if (flags & MOVE_RESULT_STAT_CHANGED) result |= RHH_MOVE_RESULT_STAT_CHANGED;
    if (flags & MOVE_RESULT_HIGH_EFFECTIVENESS) result |= RHH_MOVE_RESULT_SUPER_EFFECTIVE;
    if (flags & MOVE_RESULT_LOW_EFFECTIVENESS) result |= RHH_MOVE_RESULT_NOT_VERY_EFFECTIVE;
    return result;
}
void ModsMoveResolved(const struct BattleCalcValues *cv)
{
    sResolutionCueCount = sResolutionCueNext = 0;
    if (!cv || !sActive || !gMain.inBattle || !gBattleStruct || !RhhModsHasActiveRun()
        || sRunId != RhhModsRunId() || sResolutionContext || !RhhModsHasHook(RHH_MOVE_RESOLVED)
        || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))
        || !gBattlersCount || gBattlersCount > MAX_BATTLERS_COUNT || gBattlersCount > RHH_BATTLE_MAX_PARTICIPANTS
        || cv->battlerAtk >= gBattlersCount) return;
    RhhContentKey move = BattleMoveKey(cv->move);
    if (!move) return;
    struct RhhMoveResolutionContext context = {.struct_size = sizeof(context),
        .flags = (gBattleStruct->unableToUseMove ? RHH_MOVE_RESOLUTION_UNABLE : 0)
            | (gProtectStructs[cv->battlerAtk].chargingTurn ? RHH_MOVE_RESOLUTION_CHARGING : 0)
            | (gBattleStruct->battlerState[cv->battlerAtk].originalBattlerPartyId != PARTY_SIZE
                ? RHH_MOVE_RESOLUTION_ATTACKER_REPLACED : 0),
        .attacker = cv->battlerAtk,
        .last_target = cv->battlerDef < gBattlersCount ? cv->battlerDef : RHH_BATTLE_NO_BATTLER,
        .turn = gBattleTurnCounter, .battler_count = gBattlersCount,
        .run_id = sRunId, .battle_id = sBattleId, .sequence = sMoveSequence++,
        .move = move, .chosen_move = BattleMoveKey(gChosenMove)};
    for (u32 i = 0; i < gBattlersCount; ++i)
        context.battlers[i] = (struct RhhMoveResolutionTarget){.battler = i,
            .flags = ResolutionTargetFlags(gBattleStruct->moveResultFlags[i]),
            .last_damage = gBattleStruct->moveDamage[i]};
    struct RhhMoveResolutionDamage damage[MAX_BATTLERS_COUNT];
    for (u32 i = 0; i < gBattlersCount; ++i)
    {
        const struct MoveDamageAggregate *sum = &sMoveDamageAggregate[i];
        damage[i] = (struct RhhMoveResolutionDamage){.struct_size = sizeof(damage[i]), .battler = i};
        if (!sum->valid) continue;
        damage[i].flags = RHH_MOVE_RESOLUTION_DAMAGE_RECORDED | sum->flags;
        struct MoveDamageIdentity source = MoveDamageIdentity(cv->battlerAtk), recipient = MoveDamageIdentity(i);
        if (sum->attacker != cv->battlerAtk || BattleMoveKey(sum->move) != move)
            damage[i].flags |= RHH_MOVE_RESOLUTION_DAMAGE_MIXED_SOURCE;
        if (!MoveDamageIdentityEqual(&sum->source, &source) || (context.flags & RHH_MOVE_RESOLUTION_ATTACKER_REPLACED))
            damage[i].flags |= RHH_MOVE_RESOLUTION_DAMAGE_ATTACKER_CHANGED;
        if (!MoveDamageIdentityEqual(&sum->target, &recipient)) damage[i].flags |= RHH_MOVE_RESOLUTION_DAMAGE_TARGET_CHANGED;
        if (damage[i].flags == RHH_MOVE_RESOLUTION_DAMAGE_RECORDED)
        { damage[i].updates = sum->updates; damage[i].hp_lost = sum->hp; damage[i].substitute_lost = sum->substitute; }
    }
    struct RhhMoveResolutionHealth health[MAX_BATTLERS_COUNT];
    for (u32 i = 0; i < gBattlersCount; ++i)
    {
        const struct MoveHealthAggregate *sum = &sMoveHealth[i];
        health[i] = (struct RhhMoveResolutionHealth){.struct_size = sizeof(health[i]), .battler = i};
        if (!sum->valid) continue;
        health[i].flags = RHH_MOVE_HEALTH_RECORDED | sum->flags;
        struct MoveDamageIdentity source = MoveDamageIdentity(cv->battlerAtk), recipient = MoveDamageIdentity(i);
        if (sMoveHealthWindow.attacker != cv->battlerAtk || !MoveDamageIdentityEqual(&sMoveHealthWindow.source, &source)
            || (context.flags & RHH_MOVE_RESOLUTION_ATTACKER_REPLACED)) health[i].flags |= RHH_MOVE_HEALTH_SOURCE_CHANGED;
        if (!MoveDamageIdentityEqual(&sum->target, &recipient)) health[i].flags |= RHH_MOVE_HEALTH_TARGET_CHANGED;
        if (BattleMoveKey(sMoveHealthWindow.move) != move) health[i].flags |= RHH_MOVE_HEALTH_MIXED_MOVE;
        if (health[i].flags == RHH_MOVE_HEALTH_RECORDED)
        {
            health[i].direct_updates = sum->directUpdates; health[i].scripted_updates = sum->scriptedUpdates;
            health[i].direct_lost = sum->directLost; health[i].direct_restored = sum->directRestored;
            health[i].scripted_lost = sum->scriptedLost; health[i].scripted_restored = sum->scriptedRestored;
        }
    }
    struct RhhMoveHealthComparison comparisons[MAX_BATTLERS_COUNT];
    for (u32 i = 0; i < gBattlersCount; ++i)
    {
        struct RhhMoveHealthComparison *result = &comparisons[i];
        *result = (struct RhhMoveHealthComparison){.struct_size = sizeof(*result), .battler = i};
        if (!sMoveHealthWindow.active) continue;
        const struct MoveHealthEndpoint *start = &sMoveHealthWindow.start[i];
        struct MoveHealthEndpoint end;
        if (start->valid) { result->flags |= RHH_MOVE_HEALTH_START_AVAILABLE; result->start_hp = start->hp; result->start_max_hp = start->maxHp; }
        if (MoveHealthEndpoint(i, &end)) { result->flags |= RHH_MOVE_HEALTH_END_AVAILABLE; result->end_hp = end.hp; result->end_max_hp = end.maxHp; }
        if (start->valid && end.valid && !MoveDamageIdentityEqual(&start->identity, &end.identity))
            result->flags |= RHH_MOVE_HEALTH_PARTICIPANT_CHANGED;
        struct MoveHealthEndpoint actorEnd;
        struct MoveDamageIdentity source = MoveDamageIdentity(cv->battlerAtk);
        if (!sMoveHealthWindow.start[sMoveHealthWindow.attacker].valid || !MoveHealthEndpoint(cv->battlerAtk, &actorEnd)
            || sMoveHealthWindow.attacker != cv->battlerAtk || !MoveDamageIdentityEqual(&sMoveHealthWindow.source, &source)
            || (context.flags & RHH_MOVE_RESOLUTION_ATTACKER_REPLACED) || (health[i].flags & RHH_MOVE_HEALTH_SOURCE_CHANGED))
            result->flags |= RHH_MOVE_HEALTH_ACTOR_CHANGED;
        if (BattleMoveKey(sMoveHealthWindow.move) != move || (health[i].flags & RHH_MOVE_HEALTH_MIXED_MOVE))
            result->flags |= RHH_MOVE_HEALTH_MOVE_CHANGED;
        if (health[i].flags & RHH_MOVE_HEALTH_TARGET_CHANGED) result->flags |= RHH_MOVE_HEALTH_PARTICIPANT_CHANGED;
        if (result->flags != (RHH_MOVE_HEALTH_START_AVAILABLE | RHH_MOVE_HEALTH_END_AVAILABLE)) continue;
        u64 lost = health[i].direct_lost, restored = health[i].direct_restored;
        if (health[i].scripted_lost > INT64_MAX || lost > (u64)INT64_MAX - health[i].scripted_lost
            || health[i].scripted_restored > INT64_MAX || restored > (u64)INT64_MAX - health[i].scripted_restored)
        { result->flags |= RHH_MOVE_HEALTH_TOTAL_OVERFLOW; continue; }
        lost += health[i].scripted_lost; restored += health[i].scripted_restored;
        result->flags |= RHH_MOVE_HEALTH_NET_COMPARABLE;
        result->net_hp_change = (s64)end.hp - start->hp;
        result->recorded_hp_change = (s64)restored - (s64)lost;
        if (result->net_hp_change != result->recorded_hp_change) result->flags |= RHH_MOVE_HEALTH_NET_UNEXPLAINED;
    }
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_RESOLVED,
        .run_seed = RhhModsRunSeed(), .event_key = move, .ordinal = context.sequence,
        .args = {context.attacker, context.last_target, context.flags, context.turn}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sResolutionContext = &context; sResolutionDamageContext = damage; sResolutionHealthContext = health;
    sResolutionHealthComparisonContext = comparisons;
    RhhModsDispatch(&event, &decision);
    sResolutionHealthComparisonContext = NULL; sResolutionHealthContext = NULL; sResolutionDamageContext = NULL; sResolutionContext = NULL;
}
static u32 PowerWeather(u32 weather)
{
    if (weather & B_WEATHER_RAIN_DOWNPOUR) return RHH_WEATHER_HEAVY_RAIN;
    switch (GetBattleWeather(weather))
    {
    case BATTLE_WEATHER_RAIN: return RHH_WEATHER_RAIN;
    case BATTLE_WEATHER_RAIN_DOWNPOUR:
    case BATTLE_WEATHER_RAIN_PRIMAL: return RHH_WEATHER_HEAVY_RAIN;
    case BATTLE_WEATHER_SUN: return RHH_WEATHER_SUN;
    case BATTLE_WEATHER_SUN_PRIMAL: return RHH_WEATHER_HARSH_SUN;
    case BATTLE_WEATHER_SANDSTORM: return RHH_WEATHER_SAND;
    case BATTLE_WEATHER_HAIL: return RHH_WEATHER_HAIL;
    case BATTLE_WEATHER_SNOW: return RHH_WEATHER_SNOW;
    case BATTLE_WEATHER_FOG: return RHH_WEATHER_FOG;
    case BATTLE_WEATHER_STRONG_WINDS: return RHH_WEATHER_WIND;
    default: return RHH_WEATHER_NONE;
    }
}
static u32 PowerTerrain(u32 terrain)
{
    switch (terrain)
    {
    case B_TERRAIN_GRASSY: return RHH_TERRAIN_GRASSY;
    case B_TERRAIN_MISTY: return RHH_TERRAIN_MISTY;
    case B_TERRAIN_ELECTRIC: return RHH_TERRAIN_ELECTRIC;
    case B_TERRAIN_PSYCHIC: return RHH_TERRAIN_PSYCHIC;
    default: return RHH_TERRAIN_NONE;
    }
}
static struct RhhBattleEnvironmentState sBattleEnvironmentSample;
static const struct RhhBattleEnvironmentObservation *sBattleEnvironmentObservation;
static bool32 sBattleEnvironmentValid;
static u64 sBattleEnvironmentSequence;
static const struct RhhBattleEnvironmentCommit *sBattleEnvironmentCommit;
static u64 sBattleEnvironmentCommitSequence;
static const struct RhhBattleEnvironmentTimerTick *sBattleEnvironmentTimerTick;
static u64 sBattleEnvironmentTimerSequence;
static const struct RhhBattleWeatherSuppressionChange *sWeatherSuppressionChange;
static u64 sWeatherSuppressionSequence;
static u64 sWeatherGasSuppressionSequence;
static u64 sWeatherPresenceSuppressionSequence;
static int BattleEnvironment(struct RhhBattleEnvironmentState *out)
{
    if (!out || out->struct_size < sizeof(*out) || !ConditionsAvailable()) return -1;
    bool32 suppressed = FALSE;
    if (gBattleWeather) {
        /* Match HasWeatherEffect without the item-read side effects of its
         * ordinary ability lookup. Its Mold Breaker behavior is retained. */
        for (u32 i = 0; i < gBattlersCount; ++i) {
            u32 ability = GetBattlerAbilityReadOnly(i, FALSE);
            if (ability == ABILITY_CLOUD_NINE || ability == ABILITY_AIR_LOCK) { suppressed = TRUE; break; }
        }
    }
    struct RhhBattleEnvironmentState result = {.struct_size = sizeof(result),
        .weather = PowerWeather(gBattleWeather), .terrain = PowerTerrain(gFieldTimers.terrain),
        .weather_suppressed = suppressed, .weather_flags = gBattleWeather,
        .weather_turns = gBattleStruct->weatherDuration, .terrain_turns = gFieldTimers.terrainTimer,
        .turn = gBattleTurnCounter, .run_id = sRunId, .battle_id = sBattleId};
    result.effective_weather = suppressed ? RHH_WEATHER_NONE : result.weather;
    *out = result; return 0;
}
static int BattleEnvironmentObservation(struct RhhBattleEnvironmentObservation *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBattleEnvironmentObservation
        || RhhModsCurrentEvent() != RHH_BATTLE_ENVIRONMENT_OBSERVED) return -1;
    *out = *sBattleEnvironmentObservation; return 0;
}
static int BattleWeatherSuppressionChange(struct RhhBattleWeatherSuppressionChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWeatherSuppressionChange
        || RhhModsCurrentEvent() != RHH_BATTLE_WEATHER_SUPPRESSION_CHANGED) return -1;
    *out = *sWeatherSuppressionChange; return 0;
}
static int BattleWeatherGasSuppressionChange(struct RhhBattleWeatherSuppressionChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWeatherSuppressionChange
        || RhhModsCurrentEvent() != RHH_BATTLE_WEATHER_GAS_SUPPRESSION_CHANGED) return -1;
    *out = *sWeatherSuppressionChange; return 0;
}
static int BattleWeatherPresenceSuppressionChange(struct RhhBattleWeatherSuppressionChange *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWeatherSuppressionChange
        || RhhModsCurrentEvent() != RHH_BATTLE_WEATHER_PRESENCE_SUPPRESSION_CHANGED) return -1;
    *out = *sWeatherSuppressionChange; return 0;
}
static u64 *WeatherSuppressionSequence(u32 event)
{
    if (event == RHH_BATTLE_WEATHER_GAS_SUPPRESSION_CHANGED) return &sWeatherGasSuppressionSequence;
    if (event == RHH_BATTLE_WEATHER_PRESENCE_SUPPRESSION_CHANGED) return &sWeatherPresenceSuppressionSequence;
    return &sWeatherSuppressionSequence;
}
static bool32 BattleWeatherSuppressionCaptureEvent(struct RhhBattleEnvironmentState *out, u32 event)
{
    u64 sequence = *WeatherSuppressionSequence(event);
    if (!out || !RhhModsHasHook(event) || !ConditionsAvailable()
        || RhhModsCurrentEvent() || sWeatherSuppressionChange || sequence == UINT64_MAX
        || (gAiLogicData && (gAiLogicData->aiCalcInProgress || gAiLogicData->aiPredictionInProgress || gAiLogicData->switchInCalc))) return FALSE;
    struct RhhBattleEnvironmentState result = {.struct_size = sizeof(result)};
    if (BattleEnvironment(&result)) return FALSE;
    *out = result; return TRUE;
}
static bool32 BattleWeatherSuppressionCapture(struct RhhBattleEnvironmentState *out)
{ return BattleWeatherSuppressionCaptureEvent(out, RHH_BATTLE_WEATHER_SUPPRESSION_CHANGED); }
static bool32 BattleWeatherGasSuppressionCapture(struct RhhBattleEnvironmentState *out)
{ return BattleWeatherSuppressionCaptureEvent(out, RHH_BATTLE_WEATHER_GAS_SUPPRESSION_CHANGED); }
static void BattleWeatherSuppressionDispatch(const struct RhhBattleEnvironmentState *before, u32 eventKind,
                                             u32 cause, u32 source, u32 targetMask, u32 move, u32 triggeringAbility)
{
    u64 *sequence = WeatherSuppressionSequence(eventKind);
    if (!before || !ConditionsAvailable() || RhhModsCurrentEvent() || sWeatherSuppressionChange || *sequence == UINT64_MAX
        || !RhhModsHasHook(eventKind) || before->battle_id != sBattleId || before->run_id != sRunId
        || (gAiLogicData && (gAiLogicData->aiCalcInProgress || gAiLogicData->aiPredictionInProgress || gAiLogicData->switchInCalc))) return;
    struct RhhBattleWeatherSuppressionChange change = {.struct_size = sizeof(change), .cause = cause, .target_mask = targetMask,
        .source_battler = source < gBattlersCount ? source : RHH_BATTLE_NO_BATTLER,
        .before = *before, .after = {.struct_size = sizeof(change.after)}};
    if (BattleEnvironment(&change.after) || before->weather_flags != change.after.weather_flags
        || before->weather_suppressed == change.after.weather_suppressed) return;
    change.move = move ? BattleMoveKey(move) : 0;
    change.triggering_ability = triggeringAbility && triggeringAbility < ABILITIES_COUNT
        ? RhhContentKeyForId(RHH_ABILITY, triggeringAbility) : 0;
    change.sequence = (*sequence)++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = eventKind,
        .run_seed = RhhModsRunSeed(), .event_key = change.move ? change.move : change.triggering_ability,
        .subject = sBattleId, .ordinal = change.sequence,
        .args = {cause, change.source_battler, before->weather_suppressed, change.after.weather_suppressed}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sWeatherSuppressionChange = &change; RhhModsDispatch(&event, &decision); sWeatherSuppressionChange = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static void BattleWeatherSuppressionCommitted(const struct ModsAbilityChangeCapture *captures, u32 count,
                                              u32 cause, u32 source, u32 move, u32 triggeringAbility)
{
    if (!captures || !count || count > 2 || cause < RHH_ABILITY_MUTATION_COPY || cause > RHH_ABILITY_MUTATION_CONTACT_SWAP) return;
    const struct RhhBattleEnvironmentState *before = NULL; u32 targetMask = 0;
    for (u32 i = 0; i < count; ++i)
        if (captures[i].environmentValid) {
            if (!before) before = &captures[i].environment;
            if (captures[i].environmentBattler < gBattlersCount) targetMask |= 1u << captures[i].environmentBattler;
        }
    BattleWeatherSuppressionDispatch(before, RHH_BATTLE_WEATHER_SUPPRESSION_CHANGED, cause, source, targetMask, move, triggeringAbility);
}
static void BattleWeatherGasSuppressionCommitted(const struct ModsGasChangeCapture *capture, u32 cause, u32 source, u32 move)
{
    if (!capture || !capture->environmentValid || cause < RHH_GAS_ACTIVATED || cause > RHH_GAS_REPLACED) return;
    u32 mask = source < gBattlersCount ? 1u << source : 0;
    BattleWeatherSuppressionDispatch(&capture->environment, RHH_BATTLE_WEATHER_GAS_SUPPRESSION_CHANGED,
                                     cause, source, mask, move, ABILITY_NEUTRALIZING_GAS);
}
void ModsWeatherPresenceCapture(struct ModsWeatherPresenceCapture *out)
{
    if (!out) return;
    *out = (struct ModsWeatherPresenceCapture){0};
    out->valid = BattleWeatherSuppressionCaptureEvent(&out->environment, RHH_BATTLE_WEATHER_PRESENCE_SUPPRESSION_CHANGED);
}
void ModsWeatherPresenceCommitted(const struct ModsWeatherPresenceCapture *capture, u32 cause, u32 battler)
{
    if (!capture || !capture->valid || battler >= gBattlersCount
        || cause < RHH_WEATHER_PRESENCE_ENTERED || cause > RHH_WEATHER_PRESENCE_FAINTED) return;
    BattleWeatherSuppressionDispatch(&capture->environment, RHH_BATTLE_WEATHER_PRESENCE_SUPPRESSION_CHANGED,
                                     cause, battler, 1u << battler, 0, 0);
}
static u32 BattleEnvironmentChanged(const struct RhhBattleEnvironmentState *before, const struct RhhBattleEnvironmentState *after)
{
    u32 changed = 0;
    if (before->weather != after->weather) changed |= RHH_BATTLE_ENV_WEATHER_CHANGED;
    if (before->terrain != after->terrain) changed |= RHH_BATTLE_ENV_TERRAIN_CHANGED;
    if (before->weather_suppressed != after->weather_suppressed) changed |= RHH_BATTLE_ENV_SUPPRESSION_CHANGED;
    if (before->weather_turns != after->weather_turns) changed |= RHH_BATTLE_ENV_WEATHER_TIMER_CHANGED;
    if (before->terrain_turns != after->terrain_turns) changed |= RHH_BATTLE_ENV_TERRAIN_TIMER_CHANGED;
    if (before->weather_flags != after->weather_flags) changed |= RHH_BATTLE_ENV_RAW_WEATHER_CHANGED;
    return changed;
}
static int BattleEnvironmentCommit(struct RhhBattleEnvironmentCommit *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBattleEnvironmentCommit
        || RhhModsCurrentEvent() != RHH_BATTLE_ENVIRONMENT_COMMITTED) return -1;
    *out = *sBattleEnvironmentCommit; return 0;
}
bool32 ModsBattleEnvironmentCapture(struct RhhBattleEnvironmentState *out)
{
    if (!out || !RhhModsHasHook(RHH_BATTLE_ENVIRONMENT_COMMITTED) || !ConditionsAvailable()
        || RhhModsCurrentEvent() || sBattleEnvironmentCommit || sBattleEnvironmentCommitSequence == UINT64_MAX
        || (gAiLogicData && (gAiLogicData->aiCalcInProgress || gAiLogicData->aiPredictionInProgress || gAiLogicData->switchInCalc))) return FALSE;
    struct RhhBattleEnvironmentState result = {.struct_size = sizeof(result)};
    if (BattleEnvironment(&result)) return FALSE;
    *out = result; return TRUE;
}
static int BattleEnvironmentTimerTick(struct RhhBattleEnvironmentTimerTick *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sBattleEnvironmentTimerTick
        || RhhModsCurrentEvent() != RHH_BATTLE_ENVIRONMENT_TIMER_TICKED) return -1;
    *out = *sBattleEnvironmentTimerTick; return 0;
}
bool32 ModsBattleEnvironmentTimerCapture(struct RhhBattleEnvironmentState *out)
{
    if (!out || !RhhModsHasHook(RHH_BATTLE_ENVIRONMENT_TIMER_TICKED) || !ConditionsAvailable()
        || RhhModsCurrentEvent() || sBattleEnvironmentTimerTick || sBattleEnvironmentTimerSequence == UINT64_MAX
        || (gAiLogicData && (gAiLogicData->aiCalcInProgress || gAiLogicData->aiPredictionInProgress || gAiLogicData->switchInCalc))) return FALSE;
    struct RhhBattleEnvironmentState result = {.struct_size = sizeof(result)};
    if (BattleEnvironment(&result)) return FALSE;
    *out = result; return TRUE;
}
void ModsBattleEnvironmentTimerTicked(const struct RhhBattleEnvironmentState *before, u32 kind)
{
    if (!before || before->struct_size < sizeof(*before) || !ConditionsAvailable() || RhhModsCurrentEvent()
        || sBattleEnvironmentTimerTick || before->run_id != sRunId || before->battle_id != sBattleId
        || sBattleEnvironmentTimerSequence == UINT64_MAX
        || (gAiLogicData && (gAiLogicData->aiCalcInProgress || gAiLogicData->aiPredictionInProgress || gAiLogicData->switchInCalc))) return;
    struct RhhBattleEnvironmentTimerTick tick = {.struct_size = sizeof(tick), .kind = kind, .before = *before,
        .after = {.struct_size = sizeof(tick.after)}};
    if (BattleEnvironment(&tick.after)) return;
    u32 old, next;
    if (kind == RHH_BATTLE_ENV_TIMER_WEATHER) { old = before->weather_turns; next = tick.after.weather_turns; }
    else if (kind == RHH_BATTLE_ENV_TIMER_TERRAIN) { old = before->terrain_turns; next = tick.after.terrain_turns; }
    else return;
    if (old <= 1 || next != old - 1 || before->weather_flags != tick.after.weather_flags
        || before->terrain != tick.after.terrain) return;
    tick.sequence = sBattleEnvironmentTimerSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_ENVIRONMENT_TIMER_TICKED,
        .run_seed = RhhModsRunSeed(), .subject = sBattleId, .ordinal = tick.sequence, .args = {kind, old, next, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBattleEnvironmentTimerTick = &tick; RhhModsDispatch(&event, &decision); sBattleEnvironmentTimerTick = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static void BattleEnvironmentCommitted(const struct RhhBattleEnvironmentState *before, u32 cause, u32 battler, u32 move, u32 ability, RhhContentKey innate)
{
    if (!before || before->struct_size < sizeof(*before) || !ConditionsAvailable() || RhhModsCurrentEvent()
        || sBattleEnvironmentCommit || before->run_id != sRunId || before->battle_id != sBattleId
        || sBattleEnvironmentCommitSequence == UINT64_MAX
        || (gAiLogicData && (gAiLogicData->aiCalcInProgress || gAiLogicData->aiPredictionInProgress || gAiLogicData->switchInCalc))) return;
    struct RhhBattleEnvironmentCommit commit = {.struct_size = sizeof(commit), .cause = cause,
        .source_battler = battler < gBattlersCount ? battler : RHH_BATTLE_NO_BATTLER, .before = *before,
        .after = {.struct_size = sizeof(commit.after)}};
    if (BattleEnvironment(&commit.after)) return;
    commit.changed = BattleEnvironmentChanged(&commit.before, &commit.after);
    if (!(commit.changed & (RHH_BATTLE_ENV_RAW_WEATHER_CHANGED | RHH_BATTLE_ENV_TERRAIN_CHANGED))) return;
    commit.move = !move ? 0 : move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, move, sStabMoves, MOVES_COUNT_ALL) : RhhModsMoveKey(move);
    commit.ability = innate ? innate : ability ? CachedKey(RHH_ABILITY, ability, sStabAbilities, ABILITIES_COUNT) : 0;
    commit.sequence = sBattleEnvironmentCommitSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_ENVIRONMENT_COMMITTED,
        .run_seed = RhhModsRunSeed(), .event_key = commit.move ? commit.move : commit.ability,
        .subject = sBattleId, .ordinal = commit.sequence, .args = {cause, commit.source_battler, commit.changed, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBattleEnvironmentCommit = &commit; RhhModsDispatch(&event, &decision); sBattleEnvironmentCommit = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
void ModsBattleEnvironmentCommitted(const struct RhhBattleEnvironmentState *before, u32 cause, u32 battler, u32 move, u32 ability)
{ BattleEnvironmentCommitted(before, cause, battler, move, ability, 0); }
void ModsBattleEnvironmentCommittedInnate(const struct RhhBattleEnvironmentState *before, u32 battler, RhhContentKey ability)
{ if (ability) BattleEnvironmentCommitted(before, RHH_BATTLE_ENV_CAUSE_INNATE, battler, 0, 0, ability); }
static u32 BattleFieldRulesTimerChanged(const struct RhhBattleFieldRules *before, const struct RhhBattleFieldRules *after)
{
    u32 changed = 0;
#define FIELD_TIMER_CHANGE(member, flag) if (before->member != after->member) changed |= flag
    FIELD_TIMER_CHANGE(magic_room_turns, RHH_BATTLE_FIELD_MAGIC_ROOM);
    FIELD_TIMER_CHANGE(trick_room_turns, RHH_BATTLE_FIELD_TRICK_ROOM);
    FIELD_TIMER_CHANGE(wonder_room_turns, RHH_BATTLE_FIELD_WONDER_ROOM);
    FIELD_TIMER_CHANGE(mud_sport_turns, RHH_BATTLE_FIELD_MUD_SPORT);
    FIELD_TIMER_CHANGE(water_sport_turns, RHH_BATTLE_FIELD_WATER_SPORT);
    FIELD_TIMER_CHANGE(gravity_turns, RHH_BATTLE_FIELD_GRAVITY);
    FIELD_TIMER_CHANGE(fairy_lock_turns, RHH_BATTLE_FIELD_FAIRY_LOCK);
#undef FIELD_TIMER_CHANGE
    return changed;
}
bool32 ModsBattleFieldRulesCapture(struct RhhBattleFieldRules *out)
{
    if (!out || !RhhModsHasHook(RHH_BATTLE_FIELD_RULES_COMMITTED) || RhhModsCurrentEvent() || sFieldRulesCommit
        || sFieldRulesCommitSequence == UINT64_MAX) return FALSE;
    *out = (struct RhhBattleFieldRules){.struct_size = sizeof(*out)};
    return !BattleFieldRules(out) && !out->context_flags;
}
void ModsBattleFieldRulesCommitted(const struct RhhBattleFieldRules *before, u32 cause, u32 battler, u32 move)
{
    if (!before || before->struct_size < sizeof(*before) || !ConditionsAvailable() || RhhModsCurrentEvent()
        || sFieldRulesCommit || sFieldRulesCommitSequence == UINT64_MAX
        || before->run_id != sRunId || before->battle_id != sBattleId
        || cause < RHH_BATTLE_FIELD_CAUSE_MOVE || cause > RHH_BATTLE_FIELD_CAUSE_STARTING) return;
    struct RhhBattleFieldRulesCommit commit = {.struct_size = sizeof(commit), .before = *before,
        .after = {.struct_size = sizeof(commit.after)}, .cause = cause,
        .source_battler = battler < gBattlersCount ? battler : RHH_BATTLE_NO_BATTLER};
    if (BattleFieldRules(&commit.after) || commit.after.context_flags) return;
    commit.active_changed = before->active ^ commit.after.active;
    if (!commit.active_changed) return;
    commit.timer_changed = BattleFieldRulesTimerChanged(before, &commit.after);
    commit.move = !move ? 0 : move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, move, sStabMoves, MOVES_COUNT_ALL) : RhhModsMoveKey(move);
    commit.sequence = sFieldRulesCommitSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_FIELD_RULES_COMMITTED,
        .run_seed = RhhModsRunSeed(), .subject = sBattleId, .event_key = commit.move, .ordinal = commit.sequence,
        .args = {cause, commit.source_battler, commit.active_changed, commit.timer_changed}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sFieldRulesCommit = &commit; RhhModsDispatch(&event, &decision); sFieldRulesCommit = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static void ObserveBattleFieldRules(void)
{
    if (RhhModsCurrentEvent() || sFieldRulesObservation) return;
    if (!ConditionsAvailable()) { sFieldRulesValid = FALSE; return; }
    if (!RhhModsHasHook(RHH_BATTLE_FIELD_RULES_OBSERVED)) return;
    struct RhhBattleFieldRules current = {.struct_size = sizeof(current)};
    if (BattleFieldRules(&current)) { sFieldRulesValid = FALSE; return; }
    if (current.context_flags) return;
    if (!sFieldRulesValid) { sFieldRulesSample = current; sFieldRulesValid = TRUE; return; }
    struct RhhBattleFieldRulesObservation observation = {.struct_size = sizeof(observation),
        .before = sFieldRulesSample, .after = current, .active_changed = sFieldRulesSample.active ^ current.active};
    observation.timer_changed = BattleFieldRulesTimerChanged(&observation.before, &current);
    sFieldRulesSample = current;
    if ((!observation.active_changed && !observation.timer_changed) || sFieldRulesSequence == UINT64_MAX) return;
    observation.sequence = sFieldRulesSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_FIELD_RULES_OBSERVED,
        .run_seed = RhhModsRunSeed(), .subject = sBattleId, .ordinal = observation.sequence,
        .args = {observation.active_changed, observation.timer_changed, current.active, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sFieldRulesObservation = &observation; RhhModsDispatch(&event, &decision); sFieldRulesObservation = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static void ObserveBattleSports(void)
{
    if (RhhModsCurrentEvent() || sSportsObservation) return;
    if (!ConditionsAvailable()) { sSportsValid = FALSE; return; }
    if (!RhhModsHasHook(RHH_BATTLE_SPORTS_OBSERVED)) return;
    struct RhhBattleSports current = {.struct_size = sizeof(current)};
    if (BattleSports(&current)) { sSportsValid = FALSE; return; }
    if (current.context_flags) return;
    if (!sSportsValid) { sSportsSample = current; sSportsValid = TRUE; return; }
    struct RhhBattleSportsObservation observation = {.struct_size = sizeof(observation), .before = sSportsSample, .after = current};
    const struct RhhBattleSports *before = &observation.before;
    if (before->field_active != current.field_active) observation.changed |= RHH_BATTLE_SPORTS_CHANGED_FIELD;
    if (before->mud_battlers != current.mud_battlers) observation.changed |= RHH_BATTLE_SPORTS_CHANGED_MUD_BATTLERS;
    if (before->water_battlers != current.water_battlers) observation.changed |= RHH_BATTLE_SPORTS_CHANGED_WATER_BATTLERS;
    if (before->effective != current.effective) observation.changed |= RHH_BATTLE_SPORTS_CHANGED_EFFECTIVE;
    if (before->mud_turns != current.mud_turns || before->water_turns != current.water_turns) observation.changed |= RHH_BATTLE_SPORTS_CHANGED_TIMERS;
    if (before->battler_count != current.battler_count) observation.changed |= RHH_BATTLE_SPORTS_CHANGED_BATTLER_COUNT;
    if (before->flags != current.flags) observation.changed |= RHH_BATTLE_SPORTS_CHANGED_FLAGS;
    sSportsSample = current;
    if (!observation.changed || sSportsSequence == UINT64_MAX) return;
    observation.sequence = sSportsSequence++;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_SPORTS_OBSERVED,
        .run_seed = RhhModsRunSeed(), .subject = sBattleId, .ordinal = observation.sequence,
        .args = {observation.changed, current.mud_battlers, current.water_battlers, current.effective}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sSportsObservation = &observation; RhhModsDispatch(&event, &decision); sSportsObservation = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
void RhhModsObserveBattleEnvironment(void)
{
    ObserveBattleFieldRules();
    ObserveBattleSports();
    if (RhhModsCurrentEvent() || sBattleEnvironmentObservation) return;
    if (!ConditionsAvailable()) { sBattleEnvironmentValid = FALSE; return; }
    if (!RhhModsHasHook(RHH_BATTLE_ENVIRONMENT_OBSERVED)) return;
    struct RhhBattleEnvironmentState current = {.struct_size = sizeof(current)};
    if (BattleEnvironment(&current)) { sBattleEnvironmentValid = FALSE; return; }
    if (!sBattleEnvironmentValid) { sBattleEnvironmentSample = current; sBattleEnvironmentValid = TRUE; return; }
    struct RhhBattleEnvironmentObservation observation = {.struct_size = sizeof(observation),
        .before = sBattleEnvironmentSample, .after = current};
    observation.changed = BattleEnvironmentChanged(&observation.before, &current);
    sBattleEnvironmentSample = current;
    if (!observation.changed) return;
    observation.sequence = sBattleEnvironmentSequence++;
    RhhContentKey map = gSaveBlock1Ptr ? RhhContentKeyForId(RHH_MAP,
        ((u8)gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum) : 0;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_ENVIRONMENT_OBSERVED,
        .run_seed = RhhModsRunSeed(), .event_key = map, .ordinal = observation.sequence,
        .args = {observation.changed, current.weather, current.terrain, current.weather_suppressed}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sBattleEnvironmentObservation = &observation; RhhModsDispatch(&event, &decision); sBattleEnvironmentObservation = NULL;
    gRngValue = rng; gRng2Value = rng2;
}
static const struct RhhCriticalStageContext *sCriticalStageContext;
static const struct DamageContext *sCriticalStageCalculation;
static const struct RhhInnateCriticalStageContext *sInnateCriticalStageContext;
static int CriticalStageContext(struct RhhCriticalStageContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sCriticalStageContext
        || RhhModsCurrentEvent() != RHH_MOVE_CRITICAL_STAGE) return -1;
    *out = *sCriticalStageContext; return 0;
}
static int CriticalStageResolve(u32 role, struct RhhInnateResolution *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sCriticalStageCalculation
        || (role != RHH_INNATE_POWER_ATTACKER && role != RHH_INNATE_POWER_DEFENDER)) return -1;
    const struct DamageContext *damage = sCriticalStageCalculation;
    u32 battler = role == RHH_INNATE_POWER_ATTACKER ? damage->battlerAtk : damage->battlerDef;
    struct RhhInnateResolution result = {.struct_size = sizeof(result)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    int status = ResolveInnatesFor(battler, damage, FALSE, damage->abilities[battler], &result);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (status || !pure) return -1;
    *out = result; return 0;
}
int RhhModsCriticalStageInnates(u32 role, struct RhhInnateResolution *out)
{
    if (!sCriticalStageContext || RhhModsCurrentEvent() != RHH_MOVE_CRITICAL_STAGE) return -1;
    return CriticalStageResolve(role, out);
}
int RhhModsInnateCriticalStageContext(struct RhhInnateCriticalStageContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnateCriticalStageContext
        || RhhModsCurrentEvent() != RHH_INNATE_CRITICAL_STAGE) return -1;
    *out = *sInnateCriticalStageContext; return 0;
}
s32 ModsMoveCriticalStage(const struct DamageContext *damage, s32 base, u32 generation)
{
    if (!damage || base < 0 || generation > GEN_CHAMPIONS || !ConditionsAvailable() || sCriticalStageContext
        || damage->isSelfInflicted || (!RhhModsHasHook(RHH_MOVE_CRITICAL_STAGE) && !RhhModsHasHook(RHH_INNATE_CRITICAL_STAGE))
        || damage->battlerAtk >= gBattlersCount || damage->battlerDef >= gBattlersCount
        || damage->battlerAtk >= MAX_BATTLERS_COUNT || damage->battlerDef >= MAX_BATTLERS_COUNT
        || (gBattleTypeFlags & (BATTLE_TYPE_CATCH_TUTORIAL | BATTLE_TYPE_POKEDUDE))
        || ((gBattleTypeFlags & BATTLE_TYPE_FIRST_BATTLE) && (!IS_FRLG || !BtlCtrl_OakOldMan_TestState2Flag(1)))) return base;
    u32 maxValue = generation == GEN_1 ? 255 : 4;
    if ((u32)base > maxValue) return base;
    const struct BattlePokemon *attacker = &gBattleMons[damage->battlerAtk], *defender = &gBattleMons[damage->battlerDef];
    u32 odds = generation == GEN_1 ? base : GetCriticalHitOddsForGeneration(base, generation);
    struct RhhCriticalStageContext context = {.struct_size = sizeof(context), .flags = damage->aiCalc ? RHH_CRITICAL_AI : 0,
        .generation = generation + 1, .base_value = base, .max_value = maxValue,
        .attacker = damage->battlerAtk, .defender = damage->battlerDef,
        .numerator = generation <= GEN_2 ? odds : 1, .denominator = generation <= GEN_2 ? 256 : odds,
        .weather = PowerWeather(damage->weather), .terrain = PowerTerrain(damage->terrain),
        .move = BattleMoveKey(damage->move),
        .attacker_species = CachedKey(RHH_SPECIES, attacker->species, sStabSpecies, NUM_SPECIES),
        .defender_species = CachedKey(RHH_SPECIES, defender->species, sStabSpecies, NUM_SPECIES),
        .attacker_ability = CachedKey(RHH_ABILITY, damage->abilities[damage->battlerAtk], sStabAbilities, ABILITIES_COUNT),
        .defender_ability = CachedKey(RHH_ABILITY, damage->abilities[damage->battlerDef], sStabAbilities, ABILITIES_COUNT)};
    if (!context.move || !context.attacker_species || !context.defender_species) return base;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_CRITICAL_STAGE,
        .run_seed = RhhModsRunSeed(), .event_key = context.move, .subject = context.attacker_species,
        .args = {base, context.generation, context.attacker, context.defender}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = base};
    sCriticalStageContext = &context; sCriticalStageCalculation = damage;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    if (RhhModsHasHook(RHH_MOVE_CRITICAL_STAGE)) RhhModsDispatch(&event, &decision);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    u32 value = pure && decision.allowed && decision.value <= maxValue ? decision.value : base;
    if (RhhModsHasHook(RHH_INNATE_CRITICAL_STAGE))
        for (u32 role = RHH_INNATE_POWER_ATTACKER; role <= RHH_INNATE_POWER_DEFENDER; ++role)
        {
            if (role == RHH_INNATE_POWER_DEFENDER && damage->battlerAtk == damage->battlerDef) continue;
            struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
            if (CriticalStageResolve(role, &resolution)) continue;
            for (u32 i = 0; i < resolution.count; ++i)
            {
                if (resolution.entries[i].status) continue;
                struct RhhInnateCriticalStageContext innate = {.struct_size = sizeof(innate), .role = role, .index = i,
                    .current_value = value, .ability = resolution.entries[i].ability, .critical = context, .resolution = resolution};
                struct RhhModEvent trigger = {.struct_size = sizeof(trigger), .kind = RHH_INNATE_CRITICAL_STAGE,
                    .run_seed = RhhModsRunSeed(), .event_key = innate.ability, .subject = context.move, .ordinal = i,
                    .args = {value, role, context.attacker, context.defender}};
                struct RhhModDecision proposal = {.struct_size = sizeof(proposal), .allowed = 1, .value = value};
                rng = gRngValue; rng2 = gRng2Value;
                sInnateCriticalStageContext = &innate; RhhModsDispatch(&trigger, &proposal); sInnateCriticalStageContext = NULL;
                pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
                gRngValue = rng; gRng2Value = rng2;
                if (pure && proposal.allowed && proposal.value <= maxValue) value = proposal.value;
            }
        }
    sCriticalStageCalculation = NULL; sCriticalStageContext = NULL;
    return value;
}
static const struct RhhFinalDamageContext *sFinalDamageContext;
static const struct DamageContext *sFinalDamageCalculation;
static const struct RhhInnateFinalDamageContext *sInnateFinalDamageContext;
static int FinalDamageContext(struct RhhFinalDamageContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sFinalDamageContext
        || (RhhModsCurrentEvent() != RHH_DAMAGE_FINAL && RhhModsCurrentEvent() != RHH_SELF_DAMAGE_FINAL)) return -1;
    *out = *sFinalDamageContext; return 0;
}
static int FinalDamageResolve(u32 role, struct RhhInnateResolution *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sFinalDamageCalculation
        || (role != RHH_INNATE_POWER_ATTACKER && role != RHH_INNATE_POWER_DEFENDER)) return -1;
    const struct DamageContext *damage = sFinalDamageCalculation;
    u32 battler = role == RHH_INNATE_POWER_ATTACKER ? damage->battlerAtk : damage->battlerDef;
    struct RhhInnateResolution result = {.struct_size = sizeof(result)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    int status = ResolveInnatesFor(battler, damage, FALSE, damage->abilities[battler], &result);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (status || !pure) return -1;
    *out = result; return 0;
}
int RhhModsFinalDamageInnates(u32 role, struct RhhInnateResolution *out)
{
    if (!sFinalDamageContext || (RhhModsCurrentEvent() != RHH_DAMAGE_FINAL
        && RhhModsCurrentEvent() != RHH_SELF_DAMAGE_FINAL)) return -1;
    return FinalDamageResolve(role, out);
}
int RhhModsInnateFinalDamageContext(struct RhhInnateFinalDamageContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnateFinalDamageContext
        || (RhhModsCurrentEvent() != RHH_INNATE_DAMAGE_FINAL
            && RhhModsCurrentEvent() != RHH_INNATE_SELF_DAMAGE_FINAL)) return -1;
    *out = *sInnateFinalDamageContext; return 0;
}
static s32 FinalDamage(const struct DamageContext *damage, s32 base, bool32 selfDamage)
{
    u32 damageEvent = selfDamage ? RHH_SELF_DAMAGE_FINAL : RHH_DAMAGE_FINAL;
    u32 innateEvent = selfDamage ? RHH_INNATE_SELF_DAMAGE_FINAL : RHH_INNATE_DAMAGE_FINAL;
    if (!damage || base <= 0 || !damage->typeEffectivenessModifier || !ConditionsAvailable() || sFinalDamageContext
        || (!RhhModsHasHook(damageEvent) && !RhhModsHasHook(innateEvent))
        || damage->battlerAtk >= gBattlersCount || damage->battlerDef >= gBattlersCount
        || damage->battlerAtk >= MAX_BATTLERS_COUNT || damage->battlerDef >= MAX_BATTLERS_COUNT) return base;
    const struct BattlePokemon *atk = &gBattleMons[damage->battlerAtk], *def = &gBattleMons[damage->battlerDef];
    struct RhhFinalDamageContext context = {.struct_size = sizeof(context),
        .flags = (damage->aiCalc ? RHH_FINAL_DAMAGE_AI : 0) | (damage->isCrit ? RHH_FINAL_DAMAGE_CRITICAL : 0)
            | (damage->isSelfInflicted ? RHH_FINAL_DAMAGE_SELF_INFLICTED : 0),
        .engine_damage = base, .max_damage = RHH_FINAL_DAMAGE_MAX,
        .attacker = damage->battlerAtk, .defender = damage->battlerDef,
        .effectiveness_q12 = damage->typeEffectivenessModifier,
        .weather = PowerWeather(damage->weather), .terrain = PowerTerrain(damage->terrain),
        .move = BattleMoveKey(damage->move),
        .move_type = damage->moveType < NUMBER_OF_MON_TYPES ? sTypeKeys[damage->moveType] : RhhModsCustomTypeKey(damage->moveType),
        .attacker_species = CachedKey(RHH_SPECIES, atk->species, sStabSpecies, NUM_SPECIES),
        .defender_species = CachedKey(RHH_SPECIES, def->species, sStabSpecies, NUM_SPECIES),
        .attacker_ability = CachedKey(RHH_ABILITY, damage->abilities[damage->battlerAtk], sStabAbilities, ABILITIES_COUNT),
        .defender_ability = CachedKey(RHH_ABILITY, damage->abilities[damage->battlerDef], sStabAbilities, ABILITIES_COUNT)};
    if ((!selfDamage && (!context.move || !context.move_type))
        || !context.attacker_species || !context.defender_species) return base;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = damageEvent,
        .run_seed = RhhModsRunSeed(), .event_key = context.move, .subject = context.attacker_species,
        .args = {base, context.flags, context.attacker, context.defender}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = base};
    sFinalDamageContext = &context; sFinalDamageCalculation = damage;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    if (RhhModsHasHook(damageEvent)) RhhModsDispatch(&event, &decision);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    u32 value = pure && decision.allowed && decision.value >= 1 && decision.value <= RHH_FINAL_DAMAGE_MAX ? decision.value : base;
    if (RhhModsHasHook(innateEvent))
        for (u32 role = RHH_INNATE_POWER_ATTACKER; role <= RHH_INNATE_POWER_DEFENDER; ++role)
        {
            if (role == RHH_INNATE_POWER_DEFENDER && damage->battlerAtk == damage->battlerDef) continue;
            struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
            if (FinalDamageResolve(role, &resolution)) continue;
            for (u32 i = 0; i < resolution.count; ++i)
            {
                if (resolution.entries[i].status) continue;
                struct RhhInnateFinalDamageContext innate = {.struct_size = sizeof(innate), .role = role, .index = i,
                    .current_damage = value, .ability = resolution.entries[i].ability, .damage = context, .resolution = resolution};
                struct RhhModEvent trigger = {.struct_size = sizeof(trigger), .kind = innateEvent,
                    .run_seed = RhhModsRunSeed(), .event_key = innate.ability, .subject = context.move, .ordinal = i,
                    .args = {value, role, context.attacker, context.defender}};
                struct RhhModDecision proposal = {.struct_size = sizeof(proposal), .allowed = 1, .value = value};
                rng = gRngValue; rng2 = gRng2Value;
                sInnateFinalDamageContext = &innate; RhhModsDispatch(&trigger, &proposal); sInnateFinalDamageContext = NULL;
                pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
                gRngValue = rng; gRng2Value = rng2;
                if (pure && proposal.allowed && proposal.value >= 1 && proposal.value <= RHH_FINAL_DAMAGE_MAX) value = proposal.value;
            }
        }
    sFinalDamageCalculation = NULL; sFinalDamageContext = NULL;
    return value;
}
s32 ModsFinalDamage(const struct DamageContext *damage, s32 base) { return FinalDamage(damage, base, FALSE); }
s32 ModsSelfFinalDamage(const struct DamageContext *damage, s32 base) { return FinalDamage(damage, base, TRUE); }
static const struct RhhDamageStatContext *sDamageStatContext;
static const struct DamageContext *sDamageStatCalculation;
static const struct RhhInnateDamageStatContext *sInnateDamageStatContext;
static int BeatUpContributor(struct RhhBeatUpContributor *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    const struct DamageContext *damage = NULL;
    switch (RhhModsCurrentEvent())
    {
    case RHH_MOVE_POWER: case RHH_INNATE_POWER: damage = sPowerDamageContext; break;
    case RHH_DAMAGE_STAT: case RHH_INNATE_DAMAGE_STAT: damage = sDamageStatCalculation; break;
    case RHH_DAMAGE_FINAL: case RHH_INNATE_DAMAGE_FINAL: damage = sFinalDamageCalculation; break;
    default: return -1;
    }
    if (!damage || !(damage->beatUpContributorFlags & BEAT_UP_CONTRIBUTOR_VALID)
        || GetMoveEffect(damage->move) != EFFECT_BEAT_UP) return -1;
    RhhContentKey species = CachedKey(RHH_SPECIES, damage->beatUpContributorSpecies, sStabSpecies, NUM_SPECIES);
    if (!species) return -1;
    u32 flags = 0, internal = damage->beatUpContributorFlags;
    if (internal & BEAT_UP_CONTRIBUTOR_AI) flags |= RHH_BEAT_UP_CONTRIBUTOR_AI;
    if (internal & BEAT_UP_CONTRIBUTOR_ESTIMATED) flags |= RHH_BEAT_UP_CONTRIBUTOR_ESTIMATED;
    if (internal & BEAT_UP_CONTRIBUTOR_APPARENT) flags |= RHH_BEAT_UP_CONTRIBUTOR_APPARENT;
    if (internal & BEAT_UP_CONTRIBUTOR_OWN_PARTY) flags |= RHH_BEAT_UP_CONTRIBUTOR_OWN_PARTY;
    if (internal & BEAT_UP_CONTRIBUTOR_SLOT_KNOWN) flags |= RHH_BEAT_UP_CONTRIBUTOR_SLOT_KNOWN;
    *out = (struct RhhBeatUpContributor){.struct_size = sizeof(*out), .flags = flags,
        .hit_index = damage->beatUpContributorIndex,
        .party_slot = (flags & RHH_BEAT_UP_CONTRIBUTOR_SLOT_KNOWN)
            ? damage->beatUpContributorSlot : RHH_BEAT_UP_CONTRIBUTOR_NO_SLOT,
        .attacker = damage->battlerAtk, .defender = damage->battlerDef, .species = species};
    return 0;
}
static int DamageStatContext(struct RhhDamageStatContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sDamageStatContext
        || RhhModsCurrentEvent() != RHH_DAMAGE_STAT) return -1;
    *out = *sDamageStatContext; return 0;
}
static int DamageStatResolve(u32 role, struct RhhInnateResolution *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sDamageStatCalculation
        || (role != RHH_INNATE_POWER_ATTACKER && role != RHH_INNATE_POWER_DEFENDER)) return -1;
    u32 battler = role == RHH_INNATE_POWER_ATTACKER ? sDamageStatCalculation->battlerAtk : sDamageStatCalculation->battlerDef;
    struct RhhInnateResolution result = {.struct_size = sizeof(result)};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    int status = ResolveInnatesFor(battler, sDamageStatCalculation, FALSE, sDamageStatCalculation->abilities[battler], &result);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (status || !pure) return -1;
    *out = result; return 0;
}
int RhhModsDamageStatInnates(u32 role, struct RhhInnateResolution *out)
{
    if (!sDamageStatContext || RhhModsCurrentEvent() != RHH_DAMAGE_STAT) return -1;
    return DamageStatResolve(role, out);
}
int RhhModsInnateDamageStatContext(struct RhhInnateDamageStatContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sInnateDamageStatContext
        || RhhModsCurrentEvent() != RHH_INNATE_DAMAGE_STAT) return -1;
    *out = *sInnateDamageStatContext; return 0;
}
static u32 ApplyDamageStat(struct RhhDamageStatContext *context)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_DAMAGE_STAT,
        .run_seed = RhhModsRunSeed(), .event_key = context->move, .subject = context->attacker_species,
        .args = {context->phase, context->engine_value, context->flags, context->stat_owner}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = context->engine_value};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    if (RhhModsHasHook(RHH_DAMAGE_STAT)) RhhModsDispatch(&event, &decision);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    u32 value = pure && decision.allowed && decision.value >= 1 && decision.value <= context->max_value
        ? decision.value : context->engine_value;
    if (!RhhModsHasHook(RHH_INNATE_DAMAGE_STAT)) return value;
    for (u32 role = RHH_INNATE_POWER_ATTACKER; role <= RHH_INNATE_POWER_DEFENDER; ++role) {
        if (role == RHH_INNATE_POWER_DEFENDER && context->attacker == context->defender) continue;
        struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
        if (DamageStatResolve(role, &resolution)) continue;
        for (u32 i = 0; i < resolution.count; ++i) {
            if (resolution.entries[i].status) continue;
            struct RhhInnateDamageStatContext innate = {.struct_size = sizeof(innate), .role = role, .index = i,
                .current_value = value, .ability = resolution.entries[i].ability, .stat = *context, .resolution = resolution};
            struct RhhModEvent trigger = {.struct_size = sizeof(trigger), .kind = RHH_INNATE_DAMAGE_STAT,
                .run_seed = RhhModsRunSeed(), .event_key = innate.ability, .subject = context->move, .ordinal = i,
                .args = {value, role, context->phase, context->stat_owner}};
            struct RhhModDecision proposal = {.struct_size = sizeof(proposal), .allowed = 1, .value = value};
            rng = gRngValue; rng2 = gRng2Value;
            sInnateDamageStatContext = &innate; RhhModsDispatch(&trigger, &proposal); sInnateDamageStatContext = NULL;
            pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
            gRngValue = rng; gRng2Value = rng2;
            if (pure && proposal.allowed && proposal.value >= 1 && proposal.value <= context->max_value) value = proposal.value;
        }
    }
    return value;
}
void ModsDamageStats(const struct DamageContext *damage, u32 power, u32 *attack, u32 *defense)
{
    if (!damage || !attack || !defense || !power || !ConditionsAvailable() || sDamageStatContext
        || (!RhhModsHasHook(RHH_DAMAGE_STAT) && !RhhModsHasHook(RHH_INNATE_DAMAGE_STAT))
        || damage->battlerAtk >= gBattlersCount || damage->battlerDef >= gBattlersCount
        || damage->battlerAtk >= MAX_BATTLERS_COUNT || damage->battlerDef >= MAX_BATTLERS_COUNT) return;
    const struct BattlePokemon *atk = &gBattleMons[damage->battlerAtk], *def = &gBattleMons[damage->battlerDef];
    struct RhhDamageStatContext context = {.struct_size = sizeof(context),
        .flags = (damage->aiCalc ? RHH_DAMAGE_STAT_AI : 0) | (damage->isCrit ? RHH_DAMAGE_STAT_CRITICAL : 0)
            | (damage->isSelfInflicted ? RHH_DAMAGE_STAT_SELF_INFLICTED : 0),
        .attacker = damage->battlerAtk, .defender = damage->battlerDef, .power = power,
        .weather = PowerWeather(damage->weather), .terrain = PowerTerrain(damage->terrain), .attacker_level = atk->level,
        .move = BattleMoveKey(damage->move),
        .move_type = damage->moveType < NUMBER_OF_MON_TYPES ? sTypeKeys[damage->moveType] : RhhModsCustomTypeKey(damage->moveType),
        .attacker_species = CachedKey(RHH_SPECIES, atk->species, sStabSpecies, NUM_SPECIES),
        .defender_species = CachedKey(RHH_SPECIES, def->species, sStabSpecies, NUM_SPECIES),
        .attacker_ability = CachedKey(RHH_ABILITY, damage->abilities[damage->battlerAtk], sStabAbilities, ABILITIES_COUNT),
        .defender_ability = CachedKey(RHH_ABILITY, damage->abilities[damage->battlerDef], sStabAbilities, ABILITIES_COUNT)};
    if (!context.move || !context.move_type || !context.attacker_species || !context.defender_species) return;
    u32 effect = GetMoveEffect(damage->move);
    bool32 physical = IsBattleMovePhysical(damage->move), wonder = !!(gFieldStatuses & STATUS_FIELD_WONDER_ROOM);
    context.phase = RHH_DAMAGE_STAT_ATTACK; context.engine_value = *attack;
    context.max_value = UINT32_MAX / power / (2 * atk->level / 5 + 2);
    context.stat_owner = effect == EFFECT_FOUL_PLAY ? damage->battlerDef : damage->battlerAtk;
    context.stat = effect == EFFECT_BODY_PRESS ? (physical ? STAT_DEF : STAT_SPDEF) : (physical ? STAT_ATK : STAT_SPATK);
    context.stage_stat = effect == EFFECT_BODY_PRESS && physical && wonder ? STAT_SPDEF : context.stat;
    sDamageStatContext = &context; sDamageStatCalculation = damage;
    *attack = ApplyDamageStat(&context);
    context.phase = RHH_DAMAGE_STAT_DEFENSE; context.engine_value = *defense; context.max_value = UINT32_MAX;
    context.stat_owner = damage->battlerDef;
    bool32 defenseStage = effect == EFFECT_PSYSHOCK || physical;
    context.stage_stat = defenseStage ? STAT_DEF : STAT_SPDEF;
    context.stat = (defenseStage != wonder) ? STAT_DEF : STAT_SPDEF;
    *defense = ApplyDamageStat(&context);
    sDamageStatCalculation = NULL; sDamageStatContext = NULL;
}
u32 ModsMoveAccuracy(const struct BattleCalcValues *cv, u32 weather, u32 base, bool32 ai)
{
    if (!cv || !sActive || !gMain.inBattle || !gBattleStruct || !RhhModsHasActiveRun()
        || sRunId != RhhModsRunId() || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))
        || sAccuracyContext || (!RhhModsHasHook(RHH_MOVE_ACCURACY) && !RhhModsHasHook(RHH_INNATE_ACCURACY)) || cv->move == MOVE_STRUGGLE
        || GetMoveEffect(cv->move) == EFFECT_OHKO
        || cv->battlerAtk >= gBattlersCount || cv->battlerDef >= gBattlersCount
        || cv->battlerAtk >= MAX_BATTLERS_COUNT || cv->battlerDef >= MAX_BATTLERS_COUNT) return base;
    const struct BattlePokemon *attacker = &gBattleMons[cv->battlerAtk], *defender = &gBattleMons[cv->battlerDef];
    u32 type = GetBattleMoveType(cv->move);
    struct RhhMoveAccuracyContext context = {.struct_size = sizeof(context),
        .flags = ai ? RHH_MOVE_ACCURACY_AI : 0, .base_accuracy = base,
        .attacker = cv->battlerAtk, .defender = cv->battlerDef, .weather = PowerWeather(weather),
        .attacker_level = attacker->level, .attacker_hp = attacker->hp, .attacker_max_hp = attacker->maxHP,
        .defender_level = defender->level, .defender_hp = defender->hp, .defender_max_hp = defender->maxHP,
        .move = BattleMoveKey(cv->move),
        .move_type = type < NUMBER_OF_MON_TYPES ? sTypeKeys[type] : RhhModsCustomTypeKey(type),
        .attacker_species = CachedKey(RHH_SPECIES, attacker->species, sStabSpecies, NUM_SPECIES),
        .defender_species = CachedKey(RHH_SPECIES, defender->species, sStabSpecies, NUM_SPECIES),
        .attacker_ability = CachedKey(RHH_ABILITY, cv->abilities[cv->battlerAtk], sStabAbilities, ABILITIES_COUNT),
        .defender_ability = CachedKey(RHH_ABILITY, cv->abilities[cv->battlerDef], sStabAbilities, ABILITIES_COUNT)};
    if (!context.move || !context.attacker_species || !context.defender_species) return base;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_ACCURACY,
        .run_seed = RhhModsRunSeed(), .event_key = context.move, .subject = context.attacker_species,
        .args = {base, context.flags, cv->battlerAtk, cv->battlerDef}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = base};
    struct DamageContext calculation = {.battlerAtk = cv->battlerAtk, .battlerDef = cv->battlerDef,
        .move = cv->move, .aiCalc = ai};
    memcpy(calculation.abilities, cv->abilities, sizeof(calculation.abilities));
    memcpy(calculation.holdEffects, cv->holdEffects, sizeof(calculation.holdEffects));
    sAccuracyContext = &context; sAccuracyDamageContext = &calculation;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    if (RhhModsHasHook(RHH_MOVE_ACCURACY)) RhhModsDispatch(&event, &decision);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    u32 accuracy = pure && decision.allowed && decision.value <= 100 ? decision.value : base;
    accuracy = ApplyInnateAccuracy(&calculation, &context, accuracy);
    sAccuracyDamageContext = NULL; sAccuracyContext = NULL;
    return accuracy;
}
u32 ModsMovePower(const struct DamageContext *ctx, u32 base)
{
    if (!ctx || !base || !sActive || !gMain.inBattle || !gBattleStruct || !RhhModsHasActiveRun()
        || sRunId != RhhModsRunId() || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))
        || sPowerContext || (!RhhModsHasHook(RHH_MOVE_POWER) && !RhhModsHasHook(RHH_INNATE_POWER)) || ctx->move == MOVE_STRUGGLE
        || ctx->battlerAtk >= gBattlersCount || ctx->battlerDef >= gBattlersCount
        || ctx->battlerAtk >= MAX_BATTLERS_COUNT || ctx->battlerDef >= MAX_BATTLERS_COUNT) return base;
    const struct BattlePokemon *attacker = &gBattleMons[ctx->battlerAtk], *defender = &gBattleMons[ctx->battlerDef];
    struct RhhMovePowerContext context = {.struct_size = sizeof(context),
        .flags = (ctx->aiCalc ? RHH_MOVE_POWER_AI : 0) | (ctx->fixedBasePower ? RHH_MOVE_POWER_FIXED : 0)
               | (ctx->isCrit ? RHH_MOVE_POWER_CRITICAL : 0),
        .base_power = base, .attacker = ctx->battlerAtk, .defender = ctx->battlerDef,
        .weather = PowerWeather(ctx->weather), .terrain = PowerTerrain(ctx->terrain),
        .attacker_level = attacker->level, .attacker_hp = attacker->hp, .attacker_max_hp = attacker->maxHP,
        .defender_level = defender->level, .defender_hp = defender->hp, .defender_max_hp = defender->maxHP,
        .move = BattleMoveKey(ctx->move), .chosen_move = BattleMoveKey(ctx->chosenMove),
        .base_move = BattleMoveKey(ctx->baseMove),
        .move_type = ctx->moveType < NUMBER_OF_MON_TYPES ? sTypeKeys[ctx->moveType] : RhhModsCustomTypeKey(ctx->moveType),
        .attacker_species = CachedKey(RHH_SPECIES, attacker->species, sStabSpecies, NUM_SPECIES),
        .defender_species = CachedKey(RHH_SPECIES, defender->species, sStabSpecies, NUM_SPECIES),
        .attacker_ability = CachedKey(RHH_ABILITY, ctx->abilities[ctx->battlerAtk], sStabAbilities, ABILITIES_COUNT),
        .defender_ability = CachedKey(RHH_ABILITY, ctx->abilities[ctx->battlerDef], sStabAbilities, ABILITIES_COUNT)};
    if (!context.move || !context.move_type || !context.attacker_species || !context.defender_species) return base;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_POWER,
        .run_seed = RhhModsRunSeed(), .event_key = context.move, .subject = context.attacker_species,
        .args = {base, context.flags, ctx->battlerAtk, ctx->battlerDef}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = base};
    sPowerContext = &context; sPowerDamageContext = ctx;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    if (RhhModsHasHook(RHH_MOVE_POWER)) RhhModsDispatch(&event, &decision);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    u32 power = pure && decision.allowed && decision.value && decision.value <= RHH_MOVE_POWER_MAX ? decision.value : base;
    power = ApplyInnatePower(ctx, &context, power);
    sPowerDamageContext = NULL; sPowerContext = NULL;
    return power;
}
u32 ModsMoveBaseType(u32 move, u32 base)
{
    static bool32 resolving;
    if (!move || move == MOVE_STRUGGLE || base == TYPE_NONE || base == TYPE_MYSTERY || !RhhModsHasActiveRun()
        || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK)))
        || resolving || !RhhModsHasHook(RHH_MOVE_BASE_TYPE)) return base;
    RhhContentKey moveKey = move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, move, sStabMoves, MOVES_COUNT_ALL) : RhhModsMoveKey(move);
    RhhContentKey baseKey = base < NUMBER_OF_MON_TYPES ? sTypeKeys[base] : RhhModsCustomTypeKey(base);
    if (!moveKey || !baseKey) return base;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_BASE_TYPE,
        .run_seed = RhhModsRunSeed(), .event_key = moveKey, .subject = baseKey};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = baseKey};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    resolving = TRUE; RhhModsDispatch(&event, &decision); resolving = FALSE;
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    u32 slot;
    return pure && decision.allowed && !TypeSlotForKey(decision.value, &slot) ? slot : base;
}
static int StabContext(struct RhhTypeStabContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sStabContext || RhhModsCurrentEvent() != RHH_TYPE_STAB) return -1;
    *out = *sStabContext; return 0;
}
static int StabContextList(struct RhhExtendedTypeStabContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sExtendedStabContext
        || RhhModsCurrentEvent() != RHH_TYPE_STAB_EXTENDED) return -1;
    *out = *sExtendedStabContext; return 0;
}
u32 ModsTypeStab(const struct DamageContext *ctx, u32 base, bool32 tera)
{
    if (!ctx || !sActive || !gMain.inBattle || !gBattleStruct || !RhhModsHasActiveRun()
        || sRunId != RhhModsRunId() || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))
        || sStabContext || sExtendedStabContext
        || (!RhhModsHasHook(RHH_TYPE_STAB) && !RhhModsHasHook(RHH_TYPE_STAB_EXTENDED)) || ctx->move == MOVE_STRUGGLE
        || ctx->moveType == TYPE_NONE || ctx->moveType == TYPE_MYSTERY
        || ctx->battlerAtk >= gBattlersCount || ctx->battlerDef >= gBattlersCount
        || ctx->battlerAtk >= MAX_BATTLERS_COUNT || ctx->battlerDef >= MAX_BATTLERS_COUNT
        || gBattlerPartyIndexes[ctx->battlerAtk] >= PARTY_SIZE) return base;
    RhhContentKey moveType = ctx->moveType < NUMBER_OF_MON_TYPES ? sTypeKeys[ctx->moveType]
        : RhhModsCustomTypeKey(ctx->moveType);
    struct RhhTypeStabContext context = {.struct_size = sizeof(context),
        .flags = (tera ? RHH_TYPE_STAB_TERA : 0) | (ctx->aiCalc ? RHH_TYPE_STAB_AI : 0),
        .base = base, .attacker = ctx->battlerAtk, .defender = ctx->battlerDef,
        .move = ctx->move < MOVES_COUNT_ALL ? CachedKey(RHH_MOVE, ctx->move, sStabMoves, MOVES_COUNT_ALL)
                                           : RhhModsMoveKey(ctx->move), .move_type = moveType,
        .species = CachedKey(RHH_SPECIES, gBattleMons[ctx->battlerAtk].species, sStabSpecies, NUM_SPECIES),
        .ability = CachedKey(RHH_ABILITY, ctx->abilities[ctx->battlerAtk], sStabAbilities, ABILITIES_COUNT),
        .active_types.struct_size = sizeof(context.active_types), .original_types.struct_size = sizeof(context.original_types)};
    if (!context.move || !context.move_type || !context.species) return base;
    struct BattleTypeList active, original;
    GetBattlerTypeList(ctx->battlerAtk, FALSE, &active);
    GetBattlerTypeList(ctx->battlerAtk, TRUE, &original);
    bool32 legacyFits = active.count <= 3 && original.count <= 3;
    if (legacyFits)
    {
        for (u32 i = 0; i < active.count; ++i)
            if (AddType(&context.active_types, active.types[i])) return base;
        for (u32 i = 0; i < original.count; ++i)
            if (AddType(&context.original_types, original.types[i])) return base;
    }
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_TYPE_STAB,
        .run_seed = RhhModsRunSeed(), .event_key = context.move_type, .subject = context.species,
        .args = {base, context.flags, ctx->battlerAtk, ctx->battlerDef}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = base};
    if (legacyFits && RhhModsHasHook(RHH_TYPE_STAB))
    {
        sStabContext = &context; RhhModsDispatch(&event, &decision); sStabContext = NULL;
        if (decision.value < RHH_TYPE_STAB_MIN || decision.value > RHH_TYPE_STAB_MAX) decision.value = base;
    }
    u32 accepted = decision.allowed ? decision.value : base;
    if (RhhModsHasHook(RHH_TYPE_STAB_EXTENDED))
    {
        if (!decision.allowed) decision.value = base;
        struct RhhExtendedTypeStabContext full = {.struct_size = sizeof(full), .flags = context.flags,
            .base = decision.allowed ? decision.value : base, .attacker = context.attacker, .defender = context.defender,
            .move = context.move, .move_type = context.move_type, .species = context.species, .ability = context.ability,
            .active_types.struct_size = sizeof(full.active_types), .original_types.struct_size = sizeof(full.original_types),
            .active_types.count = active.count, .original_types.count = original.count};
        if (active.count > RHH_ACTIVE_TYPE_LIMIT || original.count > RHH_ACTIVE_TYPE_LIMIT) return accepted;
        for (u32 i = 0; i < active.count; ++i)
        {
            u32 type = active.types[i];
            full.active_types.types[i] = type < NUMBER_OF_MON_TYPES ? sTypeKeys[type] : RhhModsCustomTypeKey(type);
            if (!full.active_types.types[i]) return accepted;
        }
        for (u32 i = 0; i < original.count; ++i)
        {
            u32 type = original.types[i];
            full.original_types.types[i] = type < NUMBER_OF_MON_TYPES ? sTypeKeys[type] : RhhModsCustomTypeKey(type);
            if (!full.original_types.types[i]) return accepted;
        }
        event.kind = RHH_TYPE_STAB_EXTENDED; event.args[0] = full.base;
        sExtendedStabContext = &full; RhhModsDispatch(&event, &decision); sExtendedStabContext = NULL;
        if (decision.value < RHH_TYPE_STAB_MIN || decision.value > RHH_TYPE_STAB_MAX) decision.value = accepted;
    }
    return decision.allowed && decision.value >= RHH_TYPE_STAB_MIN && decision.value <= RHH_TYPE_STAB_MAX
        ? decision.value : base;
}
static const struct RhhLegacyTypeCategoryResult *sLegacyCategoryContext;
static int LegacyTypeCategoryContext(struct RhhLegacyTypeCategoryResult *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sLegacyCategoryContext
        || RhhModsCurrentEvent() != RHH_TYPE_LEGACY_CATEGORY) return -1;
    *out = *sLegacyCategoryContext; return 0;
}
static int LegacyTypeCategory(RhhContentKey key, struct RhhLegacyTypeCategoryResult *out)
{
    u32 slot;
    if (!out || out->struct_size < sizeof(*out) || sLegacyCategoryContext || RhhModsCustomTypeSlot(key, &slot)) return -1;
    struct RhhLegacyTypeCategoryResult result = {.struct_size = sizeof(result), .type = key};
    if (!RhhModsHasActiveRun() || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))))
    { result.flags = RHH_TYPE_CATEGORY_EXCLUDED; *out = result; return 0; }
    if (!RhhModsHasHook(RHH_TYPE_LEGACY_CATEGORY)) { *out = result; return 0; }
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_TYPE_LEGACY_CATEGORY,
        .run_seed = RhhModsRunSeed(), .event_key = key};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = RHH_TYPE_CATEGORY_MOVE_DEFAULT};
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    sLegacyCategoryContext = &result; RhhModsDispatch(&event, &decision); sLegacyCategoryContext = NULL;
    bool32 impure = memcmp(&rng, &gRngValue, sizeof(rng)) || memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    if (impure) result.flags |= RHH_TYPE_CATEGORY_IMPURE;
    if (!decision.allowed) result.flags |= RHH_TYPE_CATEGORY_DENIED;
    if (decision.value > RHH_TYPE_CATEGORY_SPECIAL) result.flags |= RHH_TYPE_CATEGORY_INVALID;
    if (!result.flags) result.category = decision.value;
    *out = result; return 0;
}
u32 ModsLegacyTypeCategory(u32 type, u32 fallback)
{
    struct RhhLegacyTypeCategoryResult result = {.struct_size = sizeof(result)};
    RhhContentKey key = RhhModsCustomTypeKey(type);
    if (!key || LegacyTypeCategory(key, &result)) return fallback;
    if (result.category == RHH_TYPE_CATEGORY_PHYSICAL) return DAMAGE_CATEGORY_PHYSICAL;
    if (result.category == RHH_TYPE_CATEGORY_SPECIAL) return DAMAGE_CATEGORY_SPECIAL;
    return fallback;
}
static const struct RhhTypeService sTypeService = {
    {sizeof(sTypeService), RHH_SERVICE_VERSION(1, 16)}, TypeChart, TypeContext, SpeciesTypes, BattlerTypes, StabContext,
    SpeciesTypesContext, SpeciesTypesProposal, OfferSpeciesTypes, TypeCompose,
    SpeciesTypesList, BattlerTypesList, SpeciesTypesProposalList, OfferSpeciesTypesList, StabContextList, TeraTypeContext, TeraOwnedContext,
    LegacyTypeCategory, LegacyTypeCategoryContext, AiBattlerTypesList, AiBattlerSpecies, PublicBattlerTypes,
    BattlerTypeObservationContext, PublicBattlerTypeObservationContext
};
static int ValidateRegisteredEffectBinding(RhhContentKey key, const struct RhhModMove *custom, u32 mode)
{
    u32 base;
    if (custom)
    {
        if (RhhContentResolve(RHH_MOVE, custom->template_move, &base)
            || !base || base >= MOVES_COUNT) return -1;
    }
    else if (RhhContentResolve(RHH_MOVE, key, &base) || !base || base >= MOVES_COUNT_ALL) return -1;
    const struct MoveInfo *info = &gMovesInfo[base];
    u32 category = custom ? custom->category : info->category == DAMAGE_CATEGORY_STATUS ? RHH_MOVE_STATUS
        : info->category == DAMAGE_CATEGORY_PHYSICAL ? RHH_MOVE_PHYSICAL : RHH_MOVE_SPECIAL;
    if (base == MOVE_STRUGGLE) return -1;
    if (mode == RHH_MOVE_EFFECT_CUSTOM_STATUS)
        return category != RHH_MOVE_STATUS || info->category != DAMAGE_CATEGORY_STATUS
            || info->effect == EFFECT_PLACEHOLDER || info->numAdditionalEffects ? -1 : 0;
    if (mode == RHH_MOVE_EFFECT_CUSTOM_DAMAGE)
        return category == RHH_MOVE_STATUS || info->effect != EFFECT_HIT ? -1 : 0;
    return -1;
}
int RhhModsInstallBattleService(void)
{
    ModsBattleReset(); ModsResetCustomMoves(); sTypeContext = NULL; sStabContext = NULL; sExtendedStabContext = NULL; sSpeciesTypesContext = NULL; sPowerContext = NULL; sPowerDamageContext = NULL; sInnatePowerContext = NULL; sAccuracyContext = NULL; sAccuracyDamageContext = NULL; sInnateAccuracyContext = NULL; sAiScoreContext = NULL; sResolutionContext = NULL; sEffectContext = NULL; sResolvingMoveTarget = sResolvingMoveEffect = FALSE;
    if (RhhModsSetLifecycleCallback(ModsClearLearnsets) || RhhModsSetLifecycleCallback(ModsResetCustomMoves)
        || RhhModsSetMoveEffectValidator(ValidateRegisteredEffectBinding)) return -1;
    memset(sStabMoves, 0, sizeof(sStabMoves)); memset(sStabSpecies, 0, sizeof(sStabSpecies));
    memset(sStabAbilities, 0, sizeof(sStabAbilities));
    memset(sSpeciesTypeKeys, 0, sizeof(sSpeciesTypeKeys));
    for (u32 i = 0; i < NUMBER_OF_MON_TYPES; ++i) sTypeKeys[i] = RhhContentKeyForId(RHH_TYPE, i);
    for (u32 i = 0; i < TRAINERS_COUNT; ++i) sTrainerCatalogKeys[i] = RhhContentKeyForId(RHH_TRAINER, i);
    if (RhhModsAddService("rhh:service/battle", &sService.header)
        || RhhModsAddService("rhh:service/battle_backdrop", ModsBattleBackdropService())
        || RhhModsAddService("rhh:service/battle_field_rules", &sFieldRulesService.header)
        || RhhModsAddService("rhh:service/battle_grounding", &sGroundingService.header)
        || RhhModsAddService("rhh:service/trainer_roster", &sTrainerRosterService.header)
        || RhhModsAddService("rhh:service/trainer_catalog", &sTrainerCatalogService.header)
        || RhhModsAddService("rhh:service/moves", &sMoveService.header)
        || RhhModsAddService("rhh:service/types", &sTypeService.header)) return -1;
    if (RhhModsInstallTypeCatalogService()) return -1;
    return RhhModsInstallBossAceService();
}
static void Notify(u32 event, u32 battler, u32 cause)
{
    if (sEventContext) return;
    struct RhhBattleEventContext context = {.struct_size = sizeof(context), .event = event,
        .battler = battler, .cause = cause, .sequence = sSequence++, .battle.struct_size = sizeof(context.battle)};
    if (Current(&context.battle)) return;
    struct RhhModEvent e = {.struct_size = sizeof(e), .kind = event, .run_seed = RhhModsRunSeed(),
        .event_key = context.battle.map, .ordinal = context.sequence, .args = {battler, cause, context.battle.outcome}};
    if (battler < MAX_BATTLERS_COUNT)
        for (unsigned i = 0; i < context.battle.participant_count; ++i)
            if (context.battle.participants[i].battler == battler)
            {
                const struct RhhBattleParticipant *participant = &context.battle.participants[i];
                e.subject = participant->identity;
                if (event == RHH_BATTLER_ENTERED)
                    sInnateEntries[battler] = (struct InnateEntryReference){.identity = participant->identity,
                        .sequence = context.sequence, .owner = participant->owner, .party_slot = participant->party_slot,
                        .cause = cause, .pending = TRUE};
                break;
            }
    struct RhhModDecision d = {.struct_size = sizeof(d), .allowed = 1};
    sEventContext = &context; RhhModsDispatch(&e, &d); sEventContext = NULL;
}
static void ResetBattleState(void)
{ RestoreInnateScriptingBattler();
  memset(sMoveTargetLocks, 0, sizeof(sMoveTargetLocks)); sMoveTargetCapture = (struct MoveTargetCapture){0}; sMoveTargetCaptureEligible = FALSE;
  sBattleEnvironmentValid = FALSE; sBattleEnvironmentObservation = NULL; sBattleEnvironmentSequence = 0;
  sFieldRulesValid = FALSE; sFieldRulesObservation = NULL; sFieldRulesSequence = 0;
  sSportsValid = FALSE; sSportsObservation = NULL; sSportsSequence = 0;
  sFieldRulesCommit = NULL; sFieldRulesCommitSequence = 0;
  sBattleEnvironmentCommit = NULL; sBattleEnvironmentCommitSequence = 0;
  sBattleEnvironmentTimerTick = NULL; sBattleEnvironmentTimerSequence = 0;
  sWeatherSuppressionChange = NULL; sWeatherSuppressionSequence = 0;
  sWeatherGasSuppressionSequence = 0; sWeatherPresenceSuppressionSequence = 0;
  memset(sAbilitySamples, 0, sizeof(sAbilitySamples)); sAbilityObservation = NULL; sAbilityObservationSequence = 0;
  memset(sHealthSamples, 0, sizeof(sHealthSamples)); sHealthObservation = NULL; sHealthObservationSequence = 0;
  memset(sTypeSamples, 0, sizeof(sTypeSamples)); sTypeObservation = NULL; sTypeObservationSequence = 0;
  memset(sPublicTypeSamples, 0, sizeof(sPublicTypeSamples)); sPublicTypeObservation = NULL; sPublicTypeObservationSequence = 0;
  sAbilityMutationContext = NULL; sAbilityMutationSequence = sAbilityMutationBatch = 0;
  sAbilityContactMutationContext = NULL; sAbilityContactMutationSequence = sAbilityContactMutationBatch = 0;
  sGasChangeContext = NULL; sGasChangeSequence = 0; sGasSampling = FALSE;
  sStatusApplicationContext = NULL; sStatusAttributionContext = NULL; sStatusApplicationSequence = 0;
  sStatusReactionDispatch = NULL; sStatusReactionCount = 0; sStatusReactionResume = NULL;
  sBagStatusReaction = (struct StatusReactionFrame){0}; sBagStatusBattleId = 0;
  sScriptCureActor = (struct ScriptCureActor){0}; sScriptCureBatch = (struct ScriptCureBatch){0};
  sScriptCureContinuationCount = 0;
  sPendingInnateStatus = (struct RhhBattleStatusAttribution){0};
  sItemStatusContext = NULL; sItemStatusSequence = 0;
  sStatusCureContext = NULL; sStatusCureAttribution = NULL; sStatusCureMove = 0; sStatusCureSequence = 0;
  sPartyStatusCureContext = NULL; sPartyStatusCureSequence = 0;
  sPartyItemStatusCureContext = NULL; sPartyItemStatusCureSequence = 0;
  memset(sInnateSamples, 0, sizeof(sInnateSamples)); sInnateObservation = NULL; sInnateObservationSequence = 0;
  sScriptedActive = (struct RhhScriptedEncounterSource){0};
  sInnateHealCount = sInnateHealNext = sInnateStatCount = sInnateStatNext = 0;
  sInnateEntryAnimationCount = sInnateEntryAnimationNext = 0;
  sInnateEntryStatusCount = sInnateEntryStatusNext = 0;
  sInnateEntryEnvironmentCount = sInnateEntryEnvironmentNext = 0;
  sActive = sEnding = 0; sBattleId = sRunId = sSequence = sMoveSequence = sTurnSequence = sEffectSequence = sConditionSequence = 0;
  sMoveDamageAppliedSequence = 0; sMoveDamageAppliedContext = NULL;
  sMoveAccuracyCheckedSequence = 0; sMoveAccuracyCheckedContext = NULL;
  sPending = 0; sHasCompletedTurn = FALSE; sLastCompletedTurn = 0;
  sEventContext = NULL; sInnateEntryContext = NULL; sInnateTurnContext = NULL; sAiInnateEntryContext = NULL; sAiInnateStatContext = NULL;
  sAiInnateEntryHealing = 0; sAiInnateFieldContext = NULL; ModsAiSwitchFieldEnd(); memset(sInnateEntries, 0, sizeof(sInnateEntries));
  sInnateTurnActionCount = sInnateTurnActionNext = 0; sInnateTurnPending = FALSE;
  memset(&sInnateTurnCompletion, 0, sizeof(sInnateTurnCompletion));
  sResolutionContext = NULL; sResolutionDamageContext = NULL; sResolutionHealthContext = NULL; sResolutionHealthComparisonContext = NULL; sTurnContext = NULL; sAiScoreContext = NULL; sAiEvaluation = NULL; sAiEffects = NULL; sEffectContext = NULL; sDamageContext = NULL; sConditionContext = NULL; sConditionProposalContext = NULL;
  sConditionOrigin = 0;
  sResolutionCueCount = sResolutionCueNext = 0; sResolutionCueResume = FALSE;
  sEffectTargets = 0; memset(sMoveDamage, 0, sizeof(sMoveDamage)); memset(sMoveDamageAggregate, 0, sizeof(sMoveDamageAggregate));
  memset(sMoveHealth, 0, sizeof(sMoveHealth)); memset(&sMoveHealthWindow, 0, sizeof(sMoveHealthWindow));
  memset(sMoveSelectionMessageValid, 0, sizeof(sMoveSelectionMessageValid));
  sEndTurnMessageResume = FALSE;
  sConditionAnimationCount = sConditionAnimationNext = 0;
  ClearEffectMessages();
  ClearEffectActions();
  memset(sConditionTurns, 0, sizeof(sConditionTurns)); }
void ModsBattleReset(void)
{
    ResetBattleState(); ModsTrainerProvenanceClear();
    sScriptedScheduled = sScriptedInitializing = sFirstSelected = (struct ScriptedBattleTicket){0};
    sScriptedHandoff = FALSE;
}
void ModsEndTurnMessagesDone(void) { sEndTurnMessageResume = TRUE; }
bool32 ModsConsumeEndTurnMessageResume(void)
{
    bool32 pending = sEndTurnMessageResume;
    sEndTurnMessageResume = FALSE;
    return pending;
}
static void NotifyCompletedTurn(struct RhhBattleTurnContext *context)
{
    if (!RhhModsHasHook(RHH_BATTLE_TURN_ENDED)) return;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_TURN_ENDED,
        .run_seed = RhhModsRunSeed(), .event_key = context->battle.map, .ordinal = context->sequence,
        .args = {context->battle.turn, context->battle.flags, context->battle.participant_count, 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sTurnContext = context; RhhModsDispatch(&event, &decision); sTurnContext = NULL;
}
void ModsFinishInnateTurnActions(void)
{
    if (!sInnateTurnPending) return;
    RestoreInnateScriptingBattler();
    sInnateTurnPending = FALSE; sInnateTurnActionCount = sInnateTurnActionNext = 0;
    struct RhhBattleSnapshot current = {.struct_size = sizeof(current)};
    if (!ConditionsAvailable() || Current(&current)
        || current.battle_id != sInnateTurnCompletion.battle.battle_id
        || current.turn != sInnateTurnCompletion.battle.turn)
    { ClearEffectMessages(); sConditionAnimationCount = sConditionAnimationNext = 0; return; }
    sInnateTurnCompletion.battle = current;
    NotifyCompletedTurn(&sInnateTurnCompletion);
}
bool32 ModsBattleTurnEnded(void)
{
    if (!sActive || sEnding || !gMain.inBattle || !gBattleStruct || gBattleOutcome
        || !RhhModsHasActiveRun() || sRunId != RhhModsRunId() || sTurnContext
        || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK))
        || (sHasCompletedTurn && sLastCompletedTurn == gBattleTurnCounter)) return FALSE;
    struct RhhBattleTurnContext context = {.struct_size = sizeof(context),
        .battle = {.struct_size = sizeof(context.battle)}};
    if (Current(&context.battle)) return FALSE;
    sHasCompletedTurn = TRUE; sLastCompletedTurn = gBattleTurnCounter;
    ClearEffectMessages();
    sConditionAnimationCount = sConditionAnimationNext = 0;
    TickConditions();
    sInnateTurnActionCount = sInnateTurnActionNext = 0;
    if (!RhhModsHasHook(RHH_BATTLE_TURN_ENDED) && !RhhModsHasHook(RHH_INNATE_TURN_END))
        return sEffectMessageCount != 0 || sConditionAnimationCount != 0;
    context.sequence = sTurnSequence++;
    if (RhhModsHasHook(RHH_INNATE_TURN_END))
    {
        u32 seen = 0;
        for (u32 order = 0; order < gBattlersCount; ++order)
        {
            u32 battler = gBattlerByTurnOrder[order];
            if (battler >= gBattlersCount || (seen & (1u << battler))) continue;
            seen |= 1u << battler;
            const struct RhhBattleParticipant *participant = NULL;
            for (u32 j = 0; j < context.battle.participant_count; ++j)
                if (context.battle.participants[j].battler == battler) { participant = &context.battle.participants[j]; break; }
            if (!participant || participant->fainted) continue;
            struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
            if (ResolveInnates(battler, NULL, &resolution)) continue;
            for (u32 i = 0; i < resolution.count; ++i)
            {
                if (resolution.entries[i].status) continue;
                struct RhhInnateTurnContext innate = {.struct_size = sizeof(innate), .battler = battler,
                    .index = i, .turn = context.battle.turn, .battle_id = sBattleId,
                    .turn_sequence = context.sequence, .ability = resolution.entries[i].ability,
                    .participant = *participant, .resolution = resolution};
                struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_INNATE_TURN_END,
                    .run_seed = RhhModsRunSeed(), .event_key = innate.ability, .subject = participant->identity,
                    .ordinal = i, .args = {battler, innate.turn, innate.turn_sequence, i}};
                struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
                sInnateTurnContext = &innate; RhhModsDispatch(&event, &decision); sInnateTurnContext = NULL;
            }
        }
    }
    if (sInnateTurnActionCount)
    {
        sInnateTurnCompletion = context;
        sInnateTurnPending = TRUE;
        return TRUE;
    }
    NotifyCompletedTurn(&context);
    return sEffectMessageCount != 0 || sConditionAnimationCount != 0;
}
void ModsBattleBegin(void)
{
    ModsOwnedBattlePartyViewReset();
    ResetBattleState();
    struct ScriptedBattleTicket ticket = sScriptedInitializing;
    sScriptedInitializing = (struct ScriptedBattleTicket){0};
    if (!RhhModsHasActiveRun() || !gMain.inBattle || !gBattleStruct || !gBattlersCount
        || gBattlersCount > MAX_BATTLERS_COUNT || gBattlersCount > RHH_BATTLE_MAX_PARTICIPANTS) return;
    if (!++sSerial) ++sSerial;
    sActive = 1; sBattleId = sSerial; sRunId = RhhModsRunId();
    ModsWildActorBattleBind(sBattleId);
    if (ticket.source.encounter && ticket.run == sRunId && ticket.map == ScriptedBattleMap()
        && ticket.flags == gBattleTypeFlags)
        sScriptedActive = ticket.source;
    Notify(RHH_BATTLE_STARTED, RHH_BATTLE_NO_BATTLER, 0);
    for (unsigned i = 0; i < gBattlersCount; ++i) {
        struct RhhBattleSnapshot view = {.struct_size = sizeof(view)};
        if (Current(&view)) break;
        for (unsigned j = 0; j < view.participant_count; ++j)
            if (view.participants[j].battler == i) { Notify(RHH_BATTLER_ENTERED, i, RHH_BATTLE_ENTRY_INITIAL); break; }
    }
    RhhModsObserveBattleEnvironment();
    RhhModsObserveBattleTypes();
    RhhModsObserveBattleHealth();
}
void ModsBattleSwitchPrepared(u32 battler)
{
    if (sActive && battler < MAX_BATTLERS_COUNT)
    { sPending |= 1u << battler; sInnateEntries[battler].pending = FALSE;
      sAbilitySamples[battler].valid = sInnateSamples[battler].valid = sTypeSamples[battler].valid = FALSE;
      sPublicTypeSamples[battler].valid = FALSE; sHealthSamples[battler].valid = FALSE; }
}
void ModsBattleSwitchEntered(u32 battler)
{
    if (!sActive || battler >= MAX_BATTLERS_COUNT || !(sPending & (1u << battler))) return;
    sPending &= ~(1u << battler);
    struct RhhBattleSnapshot view = {.struct_size = sizeof(view)};
    if (Current(&view)) return;
    for (unsigned i = 0; i < view.participant_count; ++i)
        if (view.participants[i].battler == battler) { Notify(RHH_BATTLER_ENTERED, battler, RHH_BATTLE_ENTRY_SWITCH); break; }
}
bool32 ModsInnateSwitchIn(u32 battler)
{
    if (battler >= MAX_BATTLERS_COUNT || !sInnateEntries[battler].pending
        || sInnateEntryContext || RhhModsCurrentEvent()) return FALSE;
    struct InnateEntryReference entry = sInnateEntries[battler];
    sInnateEntries[battler].pending = FALSE;
    if (!ConditionsAvailable() || !RhhModsHasHook(RHH_INNATE_ENTRY) || battler >= gBattlersCount) return FALSE;
    struct RhhBattleSnapshot battle = {.struct_size = sizeof(battle)};
    if (Current(&battle)) return FALSE;
    const struct RhhBattleParticipant *participant = NULL;
    for (u32 i = 0; i < battle.participant_count; ++i)
        if (battle.participants[i].battler == battler) { participant = &battle.participants[i]; break; }
    if (!participant || participant->fainted || participant->identity != entry.identity
        || participant->owner != entry.owner || participant->party_slot != entry.party_slot) return FALSE;
    struct RhhInnateResolution resolution = {.struct_size = sizeof(resolution)};
    if (ResolveInnates(battler, NULL, &resolution)) return FALSE;
    ClearEffectMessages();
    sInnateHealEntry = entry; sInnateEntryBattler = sInnateHealBattler = battler;
    sInnateHealCount = sInnateHealNext = sInnateStatCount = sInnateStatNext = 0;
    sInnateEntryAnimationCount = sInnateEntryAnimationNext = 0;
    sInnateEntryStatusCount = sInnateEntryStatusNext = 0;
    sInnateEntryEnvironmentCount = sInnateEntryEnvironmentNext = 0;
    for (u32 i = 0; i < resolution.count; ++i)
    {
        if (resolution.entries[i].status) continue;
        struct RhhInnateEntryContext context = {.struct_size = sizeof(context), .battler = battler,
            .cause = entry.cause, .index = i, .battle_id = sBattleId, .entry_sequence = entry.sequence,
            .ability = resolution.entries[i].ability, .participant = *participant, .resolution = resolution};
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_INNATE_ENTRY,
            .run_seed = RhhModsRunSeed(), .event_key = context.ability, .subject = participant->identity,
            .ordinal = i, .args = {battler, entry.cause, gBattleTurnCounter, entry.sequence}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        sInnateEntryContext = &context; RhhModsDispatch(&event, &decision); sInnateEntryContext = NULL;
    }
    return sEffectMessageCount != 0 || sInnateHealCount != 0 || sInnateStatCount != 0
        || sInnateEntryAnimationCount != 0 || sInnateEntryStatusCount != 0 || sInnateEntryEnvironmentCount != 0;
}
void ModsBattleEnd(u32 outcome, bool32 aborted)
{
    if (!sActive) return;
    /* Exclude scripted battles and special battle types that do not represent
     * the ordinary local population. */
    if (!aborted && outcome == gBattleOutcome && !sScriptedActive.encounter && gSaveBlock1Ptr
        && !(gBattleTypeFlags & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED
            | BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_CATCH_TUTORIAL | BATTLE_TYPE_ROAMER
            | BATTLE_TYPE_LEGENDARY | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_SAFARI | BATTLE_TYPE_RAID)))
    {
        RhhContentKey map = RhhContentKeyForId(RHH_MAP, ((u8)gSaveBlock1Ptr->location.mapGroup << 8)
                                                       | (u8)gSaveBlock1Ptr->location.mapNum);
        u64 now = RhhEcologySystemTime();
        RhhEcologyResumeClock(now);
        for (u32 i = 0; i < PARTY_SIZE; ++i)
        {
            struct Pokemon mon = gParties[B_TRAINER_OPPONENT_A][i];
            if (!GetMonData(&mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(&mon, MON_DATA_IS_EGG)
                || GetMonData(&mon, MON_DATA_HP)) continue;
            RhhContentKey species = RhhContentKeyForId(RHH_SPECIES, GetMonData(&mon, MON_DATA_SPECIES));
            if (map && species) RhhEcologyDepleteAt(map, species, now);
        }
        if (outcome == B_OUTCOME_CAUGHT && gBattleResults.caughtMonSpecies)
        {
            RhhContentKey species = RhhContentKeyForId(RHH_SPECIES, gBattleResults.caughtMonSpecies);
            if (map && species) RhhEcologyDepleteAt(map, species, now);
        }
    }
    if (!aborted && outcome == gBattleOutcome
        && !(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK)))
    {
        /* The outcome is already set, so the public in-battle snapshot is
         * unavailable here. Copy the committed raw effects before reset. */
        RhhModsEnvironmentQueueBattle(PowerWeather(gBattleWeather), PowerTerrain(gFieldTimers.terrain));
    }
    /* The engine has already committed the outcome before this call. */
    if (outcome == gBattleOutcome) {
        sEnding = 1; Notify(RHH_BATTLE_ENDED, RHH_BATTLE_NO_BATTLER,
                            aborted ? RHH_BATTLE_END_ABORTED : RHH_BATTLE_END_NORMAL);
    }
    ModsBattleReset();
}
