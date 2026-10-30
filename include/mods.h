#ifndef RHH_GAME_MODS_H
#define RHH_GAME_MODS_H
#ifdef NATIVE
#include "mod_api.h"
void ModsBattleBackdropReset(void);
void ModsBattleBackdropLoaded(u32 environment);
void ModsBattleBackdropPart(u32 part, u32 environment);
bool32 ModsBerryManualCapture(u32 objectSlot, struct RhhBerryManualWater *out);
void ModsBerryManualCommitted(struct RhhBerryManualWater *capture);
bool32 ModsBerryInteractionCapture(u32 objectSlot, u32 cause, struct RhhBerryPlotInteraction *out);
void ModsBerryInteractionCommitted(struct RhhBerryPlotInteraction *capture);
bool32 ModsBerryHarvestCapture(u32 objectSlot, struct RhhBerryHarvest *out);
void ModsBerryHarvestCommitted(struct RhhBerryHarvest *capture, u32 berryItem, u32 berryCount, u32 mutationItem, u32 mutationCount);
u32 ModsBerryHarvestYield(u32 objectSlot, u32 treeId, u32 base);
bool32 ModsBerryClockBegin(void);
void ModsBerryClockFinish(s32 minutes);
struct MapLayout;
int RhhModsAuthoredTileForLayout(const struct MapLayout *layout, u32 mapType, RhhContentKey map,
                                 s32 x, s32 y, struct RhhAuthoredMapTile *out);
int RhhModsAuthoredScanForLayout(const struct MapLayout *layout, u32 mapType, RhhContentKey map,
                                 const struct RhhAuthoredMapScanRequest *request, struct RhhAuthoredMapScan *out);
struct MapHeader;
int RhhModsAuthoredConnectionForHeader(const struct MapHeader *header, RhhContentKey map, u32 index,
                                       struct RhhAuthoredConnection *out);
int RhhModsAuthoredWarpForHeader(const struct MapHeader *header, RhhContentKey map, u32 index,
                                struct RhhAuthoredWarp *out);
int RhhModsAuthoredObjectForHeader(const struct MapHeader *header, RhhContentKey map, u32 index,
                                  struct RhhAuthoredObject *out);
int RhhModsAuthoredCoordForHeader(const struct MapHeader *header, RhhContentKey map, u32 index,
                                 struct RhhAuthoredCoordEvent *out);
int RhhModsAuthoredBackgroundForHeader(const struct MapHeader *header, RhhContentKey map, u32 index,
                                      struct RhhAuthoredBackgroundEvent *out);
bool32 RhhModsCopyObjectPalette(u16 tag, u16 *out);
struct SpriteFrameImage;
u32 RhhModsAuthoredFrameCount(const struct SpriteFrameImage *images);
u32 RhhModsBerryFrameCount(const struct SpriteFrameImage *images);
bool32 RhhModsCopyBerryObjectPalette(u32 slot, u16 *out);
u32 RhhModsPokemonIconBytes(const u8 *pixels);
bool32 RhhModsCopyPokemonIconPalette(u32 index, u16 *out);
u32 RhhModsBerryObjectGraphicsId(u32 stage);
union AnimCmd;
u32 RhhModsAuthoredAnimCount(const union AnimCmd *const *anims);
u32 RhhModsAuthoredAnimCommandCount(const union AnimCmd *commands);
struct Tileset;
u32 RhhModsRawTilesetSize(const struct Tileset *tileset);
u32 ModsMoveContextualTarget(u32 battler, u32 move, u32 target, u32 phase);
u32 AI_ApplyWeatherSuppression(u32 weather);
u32 ModsBattlerSpeed(u32 battler, u32 ability, u32 holdEffect, u32 weather, u32 terrain, u32 speed, bool32 ai);
s32 ModsMoveBattlerPriority(u32 battler, u32 ability, u32 chosenMove, u32 move, s32 base, s32 priority, bool32 maxGuard, bool32 ai);
void ModsBattleStatusApplied(u32 source, u32 target, u32 effect, u32 trigger, u32 before, bool32 presentationReady);
void ModsBattleItemStatusChanged(u32 battler, u32 item, u32 origin, u32 before, bool32 presentationReady);
struct ModsPartyItemCureCapture { bool32 valid; struct RhhBattlePartyStatusCure cure; };
void ModsCapturePartyItemCure(struct ModsPartyItemCureCapture *out, u32 source, u32 slot);
void ModsCommitPartyItemCure(const struct ModsPartyItemCureCapture *capture, u32 item);
bool32 ModsCanHatchEgg(u32 partySlot);
bool32 ModsAdjustEggCycles(u32 partySlot, u32 cycles, u32 engineDecrement, u32 *remaining);
struct ModsEggCycleCapture {
    bool32 valid; u32 partyCount, counter;
    struct SaveBlock1 *save;
    struct RhhEggCycleProgress progress;
};
void ModsCaptureEggCycle(u32 partySlot, u32 cycles, u32 engineDecrement, u32 remaining, struct ModsEggCycleCapture *out);
void ModsEggCycleCommitted(const struct ModsEggCycleCapture *capture);
void ModsBeginBagStatusReactions(void);
bool32 ModsStartBagStatusReactions(const u8 *resume);
enum ModStatusReaction { MOD_STATUS_DONE, MOD_STATUS_ANIMATION, MOD_STATUS_HEAL, MOD_STATUS_STAT,
    MOD_STATUS_STAT_FROM, MOD_STATUS_INFLICT, MOD_STATUS_DAMAGE, MOD_STATUS_CURE };
enum ModStatusReaction ModsStatusReactionNext(u32 *status, u32 *source);
const u8 *ModsStatusReactionContinuation(void);
void ModsStatusReactionCure(void);
void ModsBattleAbilityStatusCured(u32 battler, u32 ability, u32 before, u32 cause, bool32 presentationReady);
void ModsBattleEngineStatusCured(u32 battler, u32 before, u32 cause, u32 source, u32 move, bool32 presentationReady);
struct ModsStatusCureCapture { u32 before, stackSize; const u8 *resume; struct BattleScriptsStack *stack; };
void ModsCaptureStatusCure(struct ModsStatusCureCapture *capture, u32 battler);
void ModsFinishEngineStatusCure(const struct ModsStatusCureCapture *capture, u32 battler, u32 cause, u32 source, u32 move);
void ModsCaptureScriptCureActor(u32 battler, u32 cause, u32 ability);
void ModsBeginScriptCure(u32 cause, u32 battler, u32 ability, bool32 useCapturedActor, const u8 *instruction);
void ModsRecordScriptCure(u32 battler, u32 before, const u8 *instruction);
void ModsCaptureScriptPartyCure(u32 battler, u32 slots, const u8 *instruction);
void ModsPartyCureControllerBefore(u32 battler);
void ModsPartyCureControllerAfter(u32 battler);
bool32 ModsStartScriptCureReactions(const u8 *resume);
u32 ModsLegacyTypeCategory(u32 type, u32 fallback);
void ModsMoveTargetCaptureBegin(u32 battler, u32 move);
void ModsMoveTargetCaptureEnd(u32 battler, u32 move, u32 target);
#include "random.h"
#include "pokemon.h"
/* Returns 1 for a prepared proposal, 0 for the unmodified engine path, and
 * -1 when a callback made the sampled state stale and the tick must stop. */
struct ModsFieldPoisonCapture {
    struct RhhFieldPoisonContext context;
    struct RhhFieldPoisonHabitatContext habitat;
    bool32 habitatQueried;
};
int ModsFieldPoisonPrepare(u32 slot, u32 hp, u32 minimum, struct ModsFieldPoisonCapture *out);
void ModsFieldPoisonCommitted(struct ModsFieldPoisonCapture *capture, u32 afterHp, bool32 reachedThreshold);
/* Host-save feature marker; enable only alongside an atomic nature edit. */
bool32 ModsNatureFormatEnabled(void);
bool32 ModsNatureFormatCanEnable(void);
int ModsEnableNatureFormat(void);
int ModsPendingEggId(u64 *high, u64 *low);
int ModsMintPendingEggId(void);
void ModsClearPendingEggId(void);
void ModsBreedingReconcilePendingId(void);
struct Pokemon;
struct BoxPokemon;
struct DayCare;
struct WildPokemon;
struct ObjectEvent;
struct TrainerMon;

enum ModRuleFlag {
    MOD_NUZLOCKE = 1 << 0, MOD_TRAINERS = 1 << 1,
    MOD_EQUALIZE = 1 << 2, MOD_LEVEL_CAP = 1 << 3, MOD_FOLLOWER = 1 << 4
};
enum ModWildMode { MOD_WILD_OFF, MOD_WILD_POOL, MOD_WILD_FULL };
struct ModConfig { u32 flags; u8 wildMode, music; u64 starter, seed; };
extern struct ModConfig gModDraft;
int ModsRegisterBuiltins(void);
void ModsBattleBegin(void);
bool32 ModsBattleEvYieldEnabled(struct Pokemon *mon);
void ModsBattleEvYield(struct Pokemon *mon, u32 defeatedSpecies, const u8 *before,
                       const u8 *proposed, const u16 *earned);
void ModsEvLimits(struct Pokemon *mon, u32 source, u32 item, u32 defaultTotal,
                  u16 *perStat, u32 *total);
enum ModItemEvResult { MOD_ITEM_EV_UNHANDLED, MOD_ITEM_EV_DENIED, MOD_ITEM_EV_APPLIED, MOD_ITEM_EV_BANKED };
enum ModItemEvResult ModsUseItemEv(struct Pokemon *mon, u32 item, u32 partySlot);
bool32 ModsBattleTurnEnded(void);
void ModsEndTurnMessagesDone(void);
bool32 ModsConditionExpiryNextAnimation(void);
bool32 ModsConsumeEndTurnMessageResume(void);
bool32 ModsAiMoveScoreEnabled(void);
s32 ModsAiMoveScore(u32 attacker, u32 candidate, u32 move, u32 moveSlot, s32 score);
u32 ModsAiInnateEntryHealing(u32 battler, u32 partySlot, u32 hpAfterHazards);
bool32 ModsAiInnateEntryFieldPreviewEnabled(void);
bool32 ModsAiInnateEntryFieldForecast(u32 battler, u32 partySlot, u32 hpAfterHazards,
                                      u32 *weather, u32 *terrain, u32 *weatherTurns, u32 *terrainTurns);
void ModsAiSwitchFieldBegin(u32 battler, u32 weather, u32 terrain, u32 weatherTurns);
void ModsAiSwitchFieldEnd(void);
bool32 ModsAiSwitchFieldView(u32 *weather, u32 *terrain, u32 *weatherTurns);
bool32 ModsAiSwitchFieldIsCandidate(u32 battler);
void ModsAiSwitchCandidateBegin(u32 battler, u32 partySlot);
void ModsAiSwitchCandidateEnd(void);
u32 ModsAiInnateTurnHealing(u32 battler, u32 partySlot, u32 hpAfterEntry);
bool32 ModsAiInnateTurnStepEnabled(void);
u32 ModsAiInnateTurnStepHealing(u32 battler, u32 partySlot, u32 hpBeforeHeal);
bool32 ModsAiInnateEntryStatPreviewEnabled(void);
bool32 ModsAiInnateEntryStats(u32 battler, u32 partySlot, u32 hpAfterHazards, u8 *stages);
bool32 ModsAiInnateEntryStatChanges(u32 battler, u32 partySlot, u32 hpAfterHazards, u8 *stages, u8 *opponentStages);
bool32 ModsAiInnateEntryForecast(u32 battler, u32 partySlot, u32 hpAfterHazards, u8 *stages, u8 *opponentStages, u32 *opponentStatus);
bool32 ModsAiInnateEntryForecastStatuses(u32 battler, u32 partySlot, u32 hpAfterHazards, u8 *stages, u8 *opponentStages,
                                        u32 *ownStatus, u32 *opponentStatus);
bool32 ModsAiInnateEntryForecastDetailed(u32 battler, u32 partySlot, u32 hpAfterHazards, u8 *stages, u8 *opponentStages,
                                        u32 *ownStatus, u32 *opponentStatus, u32 *consumedItems);
void ModsMoveEffectApply(void);
bool32 ModsMoveDamageEffectApply(void);
bool32 ModsMoveEffectIsDamage(void);
void ModsRecordMoveDamage(u32 attacker, u32 target, u32 move, u32 hpLost, u32 substituteLost);
void ModsRecordMoveHealth(u32 target, u32 before, u32 after, bool32 scripted);
void ModsMoveEffectReset(void);
bool32 ModsMoveEffectNextMessage(void);
enum ModMoveEffectAction { MOD_EFFECT_DONE, MOD_EFFECT_HEAL, MOD_EFFECT_STATUS, MOD_EFFECT_STAT, MOD_EFFECT_HEAL_USER, MOD_EFFECT_HEAL_DAMAGE_USER, MOD_EFFECT_DRAIN_USER, MOD_EFFECT_HEAL_BATTLER, MOD_EFFECT_DAMAGE_BATTLER, MOD_EFFECT_ANIMATION, MOD_EFFECT_STAT_BATTLER, MOD_EFFECT_STATUS_BATTLER, MOD_EFFECT_FIELD_RULE, MOD_EFFECT_ROOM_TOGGLE, MOD_EFFECT_FIELD_MOVE, MOD_EFFECT_DAMAGE_FROM_HIT };
enum ModMoveEffectAction ModsMoveEffectNextAction(u32 *value, s32 *stages);
u32 ModsMoveEffectRecipient(void);
void ModsMoveEffectItemBegin(void);
void ModsMoveEffectItemEnd(void);
const u8 *ModsMoveEffectMessageText(void);
bool32 ModsBattleEnvironmentCapture(struct RhhBattleEnvironmentState *out);
bool32 ModsBattleFieldRulesCapture(struct RhhBattleFieldRules *out);
void ModsBattleFieldRulesCommitted(const struct RhhBattleFieldRules *before, u32 cause, u32 battler, u32 move);
bool32 ModsBattleEnvironmentTimerCapture(struct RhhBattleEnvironmentState *out);
void ModsBattleEnvironmentTimerTicked(const struct RhhBattleEnvironmentState *before, u32 kind);
void ModsBattleEnvironmentCommitted(const struct RhhBattleEnvironmentState *before, u32 cause, u32 battler, u32 move, u32 ability);
void ModsBattleEnvironmentCommittedInnate(const struct RhhBattleEnvironmentState *before, u32 battler, RhhContentKey ability);
void ModsBattleSwitchPrepared(u32 battler);
void ModsBattleSwitchEntered(u32 battler);
struct ModsAbilityChangeCapture {
    bool32 valid, gastroAcid;
    RhhContentKey overwritten;
    struct RhhBattleParticipant participant;
    struct RhhBattlerAbility ability;
    bool32 environmentValid;
    u32 environmentBattler;
    struct RhhBattleEnvironmentState environment;
};
void ModsAbilityChangeCapture(u32 battler, struct ModsAbilityChangeCapture *out);
void ModsAbilityChangesCommitted(const struct ModsAbilityChangeCapture *captures, u32 count, u32 cause, u32 source, u32 move);
void ModsAbilityContactChangesCommitted(const struct ModsAbilityChangeCapture *captures, u32 count, u32 cause,
                                        u32 source, u32 move, u32 triggeringAbility);
struct ModsGasChangeCapture {
    bool32 valid, environmentValid;
    struct RhhGasSnapshot before;
    struct RhhBattleEnvironmentState environment;
};
void ModsGasChangeCapture(struct ModsGasChangeCapture *out);
void ModsGasChangeCommitted(const struct ModsGasChangeCapture *capture, u32 cause, u32 source, u32 move);
struct ModsWeatherPresenceCapture {
    bool32 valid;
    struct RhhBattleEnvironmentState environment;
};
void ModsWeatherPresenceCapture(struct ModsWeatherPresenceCapture *out);
void ModsWeatherPresenceCommitted(const struct ModsWeatherPresenceCapture *capture, u32 cause, u32 battler);
bool32 ModsInnateSwitchIn(u32 battler);
bool32 ModsInnateNextHeal(void);
bool32 ModsInnateNextStat(void);
bool32 ModsInnateNextAnimation(void);
bool32 ModsInnateNextStatus(u32 *status, u32 *source);
void ModsInnateStatusFinished(void);
bool32 ModsInnateStatSourced(void);
u32 ModsInnateStatSource(void);
enum ModInnateTurnAction { MOD_INNATE_TURN_DONE, MOD_INNATE_TURN_HEAL, MOD_INNATE_TURN_STAT, MOD_INNATE_TURN_STAT_SOURCE, MOD_INNATE_TURN_ANIMATION, MOD_INNATE_TURN_STATUS, MOD_INNATE_TURN_WEATHER, MOD_INNATE_TURN_TERRAIN };
enum ModInnateEnvironmentAction { MOD_INNATE_ENV_DONE, MOD_INNATE_ENV_WEATHER, MOD_INNATE_ENV_TERRAIN };
enum ModInnateEnvironmentAction ModsInnateNextEnvironment(void);
bool32 ModsInnateTurnActionsPending(void);
enum ModInnateTurnAction ModsInnateTurnNextAction(u32 *status, u32 *source);
void ModsFinishInnateTurnActions(void);
void ModsBattleEnd(u32 outcome, bool32 aborted);
void ModsBattleReset(void);
void ModsScriptedBattleSchedule(u32 task, struct RhhScriptedEncounterSource source);
void ModsWildScriptedBattleSchedule(u32 task);
void ModsScriptedBattleTransitionComplete(u32 task);
void ModsScriptedBattleCallback(bool32 battleInit);
int ModsScriptedBattleSource(struct RhhScriptedEncounterSource *out);
void ModsFirstBattleSelect(struct RhhScriptedEncounterSource source);
void ModsWildFirstBattleSelect(void);
void ModsFirstBattleHandoff(void);
bool32 ModsFirstBattleInitializing(struct RhhScriptedEncounterSource *out);
void ModsWildFirstBattleReady(void);
u32 ModsTypeMatchup(u32 attack, u32 defense, u32 base, bool32 inverse);
bool32 ModsRuntimeTypeMatchup(u32 attack, u32 defense, bool32 inverse, u32 *effective);
struct DamageContext;
struct BattleCalcValues;
void ModsMoveAccuracyChecked(const struct BattleCalcValues *context, bool32 missed);
struct BattlePokemon;
struct BattleTypeList;
void ModsSpeciesTypeList(u32 species, struct BattleTypeList *types);
void ModsBattleMonSpeciesTypes(struct BattlePokemon *mon);
u32 ModsTypeStab(const struct DamageContext *context, u32 base, bool32 tera);
u32 ModsMovePower(const struct DamageContext *context, u32 base);
s32 ModsMoveCriticalStage(const struct DamageContext *context, s32 base, u32 generation);
s32 ModsFinalDamage(const struct DamageContext *context, s32 base);
s32 ModsSelfFinalDamage(const struct DamageContext *context, s32 base);
void ModsDamageStats(const struct DamageContext *context, u32 power, u32 *attack, u32 *defense);
u32 ModsMovePPCost(const struct BattleCalcValues *context, u32 slot, u32 target, u32 base);
void ModsMovePPDeducted(const struct BattleCalcValues *context, u32 slot, u32 target, u32 base,
                        u32 cost, u32 before, u32 after, bool32 permanent);
bool32 ModsCanSelectMove(u32 battler, u32 slot);
bool32 ModsPrepareMoveSelection(u32 battler, u32 slot);
const u8 *ModsMoveSelectionMessage(u32 battler);
u32 ModsMoveAccuracy(const struct BattleCalcValues *context, u32 weather, u32 base, bool32 ai);
void ModsMoveResolved(const struct BattleCalcValues *context);
bool32 ModsMoveResolutionCuesPending(void);
bool32 ModsMoveResolutionNextCue(void);
void ModsMoveResolutionCuesDone(void);
bool32 ModsConsumeMoveResolutionResume(void);
void ModsSpeciesTypes(u32 species, u8 *types);
const struct LevelUpMove *ModsLevelUpLearnset(u32 species, const struct LevelUpMove *base);
bool32 ModsLearnsetResolutionActive(void);
bool32 ModsExperienceFlowApply(u32 slot, u32 *beforeLevel, u32 *afterLevel, u32 *applied);
void ModsExperienceFlowFinish(void);
const u16 *ModsOtherLearnset(u32 species, u32 kind, const u16 *base);
struct Sprite;
bool32 ModsSetCustomTypeIcon(struct Sprite *sprite, u32 type, bool32 compact);
void ModsClearCustomTypeIcon(struct Sprite *sprite);
void ModsReclaimCustomTypeIcons(void);
const u8 *ModsCustomTypeName(u32 type, bool32 generic);
bool32 ModsCustomTypeColor(u32 type, u16 *out);
u32 ModsBattleTeraType(u32 battler, u32 base, const struct Pokemon *source);
u32 ModsConversion2Types(u32 attack, u8 *types, u32 capacity);
void ModsTrainerProvenanceClear(void);
void ModsTrainerPartyClear(struct Pokemon *party);
void ModsTrainerPartyRecord(struct Pokemon *party, u32 slot, u32 trainer, u32 rosterTrainer,
                            u32 source, u32 sourceCount, const struct TrainerMon *entry);
void ModsSetExternalBuiltinDirectory(const char *directory);
void ModsNewGame(u32 trainerId);
void ModsAfterLoad(void);
void ModsAfterSave(void);
struct ModConfig ModsGetConfig(void);
void ModsSetMusic(u32 music);
void ModsSetBattleEnvironment(unsigned enabled);
unsigned ModsBattleEnvironmentEnabled(void);
int ModsShowWeatherRecap(void);
bool32 ModsHasRule(u32 flag);
u32 ModsStarterCount(void);
u32 ModsStarterAt(u32 index);
bool32 ModsStarterEligible(u32 species);
u32 ModsBreedingCompatibility(struct DayCare *daycare, u32 baseScore);
struct ModsBreedingRngScope { rng_value_t saved, saved2; bool32 active; };
void ModsBreedingRngBegin(struct DayCare *daycare, struct ModsBreedingRngScope *scope);
void ModsBreedingRngEnd(struct ModsBreedingRngScope *scope);
void ModsBreedingPendingChanged(struct DayCare *daycare, u32 cause, bool32 before, bool32 after);
u32 ModsBreedingOffspring(struct DayCare *daycare, u32 species, u32 compatibility, const u8 *parentSlots);
u32 ModsBreedingIvCount(struct DayCare *daycare, u32 baseCount);
u32 ModsBreedingNature(struct DayCare *daycare, u32 baseNature);
void ModsBreedingInheritIVs(struct DayCare *daycare, struct Pokemon *egg, u32 originalSpecies,
                            u32 compatibility, const u8 *parentSlots, const u8 *initialIVs);
void ModsBreedingInheritMoves(struct DayCare *daycare, struct Pokemon *egg, u32 originalSpecies,
                              u32 compatibility, const u8 *parentSlots);
void ModsBreedingEggTraits(struct DayCare *daycare, struct Pokemon *egg, u32 originalSpecies,
                           u32 compatibility, const u8 *parentSlots);
int ModsDaycareOrdinalState(u64 *next);
int ModsEnableDaycareOrdinal(void);
u64 ModsCommitDaycareOrdinal(void);
void ModsBreedingEggCreated(struct DayCare *daycare, struct Pokemon *egg, u32 originalSpecies,
                            u32 compatibility, const u8 *parentSlots, u32 partySlot, bool32 hadPending, u64 ordinal);
bool32 ModsEggHatchBefore(u32 partySlot, struct RhhOwnedMonSnapshot *before);
void ModsEggHatched(u32 partySlot, const struct RhhOwnedMonSnapshot *before);
void ModsWorldStep(s32 x, s32 y, s32 elevation, u32 behavior);
bool32 ModsOwnedReleaseCapture(struct BoxPokemon *mon, u32 location, u32 box, u32 slot, bool32 moving,
                               struct RhhOwnedRelease *out);
void ModsOwnedReleaseCommitted(struct RhhOwnedRelease *release);
bool32 ModsOwnedMoveCapture(struct BoxPokemon *moving, u32 sourceLocation, u32 sourceBox, u32 sourceSlot,
                            struct BoxPokemon *destination, u32 destinationLocation, u32 destinationBox,
                            u32 destinationSlot, bool32 group, struct RhhOwnedMove *out);
void ModsOwnedMoveCommitted(struct BoxPokemon *placed, struct RhhOwnedMove *move);
bool32 ModsOwnedPickupCapture(struct BoxPokemon *mon, u32 location, u32 box, u32 slot, bool32 group,
                              struct RhhOwnedPickup *out);
void ModsOwnedPickupCommitted(struct RhhOwnedPickup *pickup);
bool32 ModsOwnedGiftCapture(struct BoxPokemon *mon, u32 cause, struct RhhOwnedGift *out);
void ModsOwnedGiftDestination(struct BoxPokemon *slot, u32 location, u32 box, u32 position, struct RhhOwnedGift *gift);
void ModsOwnedGiftCommitted(struct BoxPokemon *slot, struct RhhOwnedGift *gift);
bool32 ModsOwnedEvolutionCapture(struct Pokemon *mon, u32 partySlot, bool32 trade, struct RhhOwnedEvolution *out);
bool32 ModsOwnedEvolutionSplitCapture(struct Pokemon *parent, u32 parentSlot, u32 destinationSlot,
                                      struct RhhOwnedEvolution *out);
void ModsOwnedEvolutionCommitted(struct Pokemon *mon, struct RhhOwnedEvolution *evolution);
void ModsLinkTradeEvolutionCommitted(const struct RhhOwnedEvolution *evolution);
bool32 ModsOwnedTradePcDestination(u32 *box, u32 *slot);
bool32 ModsPartyReorderBegin(u32 cause, struct RhhPartyReorder *out);
void ModsPartyReorderAdd(struct RhhPartyReorder *batch, u32 source, u32 destination);
void ModsPartyReorderCommitted(struct RhhPartyReorder *batch);
bool32 ModsOwnedBattlePartyViewActive(void);
void ModsOwnedBattlePartyViewReset(void);
bool32 ModsOwnedSkyPartyActive(void);
bool32 ModsOwnedProjectedPartyActive(void);
bool32 ModsOwnedRentalPartyActive(void);
void ModsOwnedRentalPartyBegin(void);
void ModsOwnedRentalPartyBeforeRestore(void);
void ModsOwnedRentalPartyAfterRestore(void);
void ModsOwnedRecordedPartyBegin(const struct Pokemon *backup, u32 count);
void ModsOwnedRecordedPartyBeforeRestore(const struct Pokemon *backup);
void ModsOwnedRecordedPartyAfterRestore(void);
void ModsOwnedBeforePartyReduction(void);
void ModsOwnedProjectedPartyInvalidate(void);
bool32 ModsOwnedFollowerPartyActive(void);
bool32 ModsOwnedFollowerPartyReady(void);
bool32 ModsOwnedSkyPartyPreparing(void);
void ModsOwnedSkyPartyBegin(u32 mask);
void ModsOwnedSkyPartyPrepared(void);
void ModsOwnedSkyPartyBeforeRestore(u32 mask);
void ModsOwnedSkyPartyAfterRestore(void);
void ModsOwnedFollowerPartyBegin(const u8 *order, u32 count);
void ModsOwnedFollowerPartyPrepared(void);
void ModsOwnedFollowerPartyBeforeRestore(const u16 *order, u32 count);
void ModsOwnedFollowerPartyAfterRestore(void);
void ModsOwnedUnionPartyBegin(const u8 *order, u32 count);
bool32 ModsOwnedUnionPartyReady(void);
void ModsOwnedLinkMultiPartyBegin(const u8 *order, u32 count);
bool32 ModsOwnedLinkMultiPartyReady(void);
void ModsOwnedFrontierPartyBegin(const u8 *order, u32 count);
void ModsOwnedFrontierPartyReselect(const u8 *order, u32 count, u32 capacity);
bool32 ModsOwnedFrontierPartyBeforeSwap(u32 first, u32 second);
bool32 ModsOwnedFrontierPartyActive(void);
bool32 ModsOwnedFrontierPartyReady(void);
bool32 ModsOwnedFrontierMultiPartyReady(void);
u32 ModsOwnedFrontierPartySelection(u8 *order, u32 capacity);
void ModsOwnedFrontierBeforeSaveParty(void);
void ModsOwnedFrontierBeforeSavePartyMon(u32 index, struct Pokemon *mon);
void NativeOwnedTrackFrontierCapture(const u8 *selected, u32 count, const struct Pokemon *party);
void NativeOwnedTrackFrontierResume(const u8 *selected, u32 count, const struct Pokemon *party);
void ModsOwnedUnionPartyPrepared(void);
void ModsOwnedUnionPartyBeforeRestore(void);
void ModsOwnedUnionPartyAfterRestore(void);
bool32 ModsOwnedFollowerCatchFullActive(void);
void ModsOwnedFollowerCatchBeforeLoad(void);
void ModsOwnedFollowerCatchLoaded(void);
void ModsOwnedFollowerCatchBeforeReduce(void);
void ModsOwnedFollowerCatchReduced(void);
int ModsOwnedBattlePartyFieldSlot(u32 viewSlot, u32 *fieldSlot);
int ModsOwnedBattlePartyVisibleIdentity(u32 viewSlot, u64 *identity);
bool32 ModsOwnedBattlePartySummaryCheck(void);
void ModsOwnedBattlePartySummaryBeforeWrite(const struct BoxPokemon *mon);
void ModsOwnedBattlePartySummaryBeforeBytes(const void *dest, size_t size);
void NativeOwnedTrackerUnsupported(void);
void NativeOwnedTrackerUnreportedPartySwap(u32 first, u32 second);
void NativeOwnedTrackerBattleViewUnsupported(void);
void NativeOwnedTrackEggCreated(u32 partySlot);
int NativeOwnedTrackHatchBefore(u32 partySlot);
void NativeOwnedTrackHatchAfter(u32 partySlot);
bool32 ModsDaycareTransferCapture(struct BoxPokemon *source, u32 cause, u32 sourceLocation, u32 sourceBox,
                                 u32 sourceSlot, u32 destinationLocation, u32 destinationBox,
                                 u32 destinationSlot, struct RhhDaycareTransfer *out);
void ModsDaycareTransferCommitted(struct BoxPokemon *destination, struct RhhDaycareTransfer *transfer);
bool32 ModsFusionTransferCapture(struct BoxPokemon *source, u32 cause, u32 sourceLocation, u32 sourceSlot,
                                 u32 primarySlot, struct RhhFusionTransfer *out);
void ModsFusionTransferCommitted(struct BoxPokemon *destination, u32 location, u32 box, u32 slot,
                                 struct RhhFusionTransfer *transfer);
bool32 ModsCatchPlacementCapture(struct BoxPokemon *source, u32 cause, u32 sourceSlot,
                                  struct RhhCatchPlacement *out);
void ModsCatchPlacementCommitted(struct BoxPokemon *destination, u32 location, u32 box, u32 slot,
                                  struct RhhCatchPlacement *placement);
bool32 ModsOwnedTradePcHeld(void);
void ModsOwnedTradeReset(void);
bool32 ModsOwnedLinkTradeActive(void);
bool32 ModsIngameTradeCapture(struct BoxPokemon *sent, struct BoxPokemon *received, u32 location,
                               u32 box, u32 slot, struct RhhIngameTrade *out);
void ModsIngameTradeCommitted(struct BoxPokemon *received, u32 location, u32 box, u32 slot,
                               struct RhhIngameTrade *trade);
bool32 ModsLinkTradeCapture(struct BoxPokemon *sent, struct BoxPokemon *received, u32 slot, struct RhhLinkTrade *out);
bool32 ModsLinkTradePlaced(struct BoxPokemon *received, struct RhhLinkTrade *trade);
void ModsLinkTradeSaved(u32 transport, struct RhhLinkTrade *trade);
bool32 ModsLinkTradeHistoryStage(u32 transport, struct RhhLinkTrade *trade);
void ModsLinkTradeHistoryFinish(bool32 saved);
u32 ModsWorldBattleWeather(void);
u32 ModsWorldBattleTerrain(void);
u32 ModsWildSpecies(u32 species, u32 kind, u32 slot, u32 timeOfDay);
u32 ModsWildEncounterRate(u32 tableRate, u32 baseRate, u32 area, u32 source, u32 timeOfDay,
                          s32 x, s32 y, bool32 ignoreAbility);
u32 ModsFishingBiteRate(u32 rod, bool32 sticky, u32 phase, u32 basePercent);
u32 ModsWildSpawnInterval(u32 baseFrames, u32 cause, u32 area, u32 activeCount, u32 capacity,
                          u32 flags, s32 x, s32 y);
void ModsWildCreated(u32 species, u32 kind, u32 slot, u32 timeOfDay);
u32 ModsWildCandidate(u32 tableSpecies, u32 species, u32 kind, u32 source, u32 slot,
                      u32 level, u32 timeOfDay, s32 x, s32 y, struct RhhWildEncounterContext *context);
bool32 ModsWildResolve(u32 tableSpecies, u32 *species, u32 kind, u32 source, u32 slot,
                       u32 level, u32 timeOfDay, s32 x, s32 y, struct RhhWildEncounterContext *context,
                       struct RhhWildEncounterCancellation *cancel);
void ModsWildCancelled(const struct RhhWildEncounterCancellation *cancel);
struct RhhWildCheckpoint {
    struct Pokemon enemy[PARTY_SIZE];
    rng_value_t rng, rng2;
    u8 enemyCount, roamerIndex;
};
void ModsWildCheckpointSave(struct RhhWildCheckpoint *checkpoint);
void ModsWildCheckpointRestore(const struct RhhWildCheckpoint *checkpoint);
bool32 ModsWildSpecialPermission(u32 species, u32 kind, u32 source, u32 slot, u32 level,
                                 u32 timeOfDay, s32 x, s32 y, u64 identity,
                                 struct RhhWildEncounterContext *context,
                                 struct RhhWildEncounterCancellation *cancel);
void ModsWildSpecialCreated(struct RhhWildEncounterContext *context, struct Pokemon *mon);
void ModsWildScriptedReady(u32 count);
void ModsWildSpecialScriptedReady(bool32 tutorial);
bool32 ModsWildScriptedPermission(struct Pokemon *prepared, u32 slot, bool32 eventMon, struct RhhWildEncounterCancellation *cancel);
bool32 ModsWildTutorialPermission(struct Pokemon *prepared, struct RhhWildEncounterCancellation *cancel);
void ModsWildTutorialCheckpointBegin(void);
void ModsWildTutorialCancelled(const struct RhhWildEncounterCancellation *cancel);
void ModsWildEventCancelled(const struct RhhWildEncounterCancellation *cancel);
void ModsWildScriptCleanupQueue(const struct RhhWildEncounterCancellation *cancel, u32 kind);
void ModsWildScriptCleanupFinish(void);
void ModsWildScriptCleanupDiscard(void);
u64 ModsWildScriptedKey(const char *id);
struct RhhScriptedEncounterSource ModsWildScriptedScopeSwap(struct RhhScriptedEncounterSource source);
void ModsWildScriptedCheckpointBegin(u32 count, bool32 eventMon);
u32 ModsWildScriptedCandidate(u32 species, u32 level, u32 slot);
u32 ModsWildEventCandidate(u32 species, u32 level);
void ModsWildCandidateCreated(struct RhhWildEncounterContext *context, struct Pokemon *mon);
bool32 ModsWildWeights(const struct WildPokemon *table, const struct WildPokemon *candidates,
                       u32 area, u32 source, u32 rod, u32 timeOfDay, s32 x, s32 y,
                       u32 first, u32 count, const u16 *base, u16 *weights);
void ModsWildActorSpawned(struct ObjectEvent *object, const struct Pokemon *preserved);
u32 ModsWildActorPresentation(u32 spriteId, s32 *coverY);
bool32 ModsWildActorCueVisible(const struct ObjectEvent *object);
bool32 ModsWildScoutFacing(void);
u32 RhhModsWildDormantRecordSize(void);
int RhhModsWildDormantPrepare(u64 handle, RhhContentKey owner, void *out, u32 capacity);
struct NativeSaveArchiveCodec;
const struct NativeSaveArchiveCodec *RhhModsWildDormantCodec(void);
void ModsWildActorEngaged(struct ObjectEvent *object);
void ModsWildActorBattleTransition(void);
void ModsWildActorBattleFinished(u32 outcome);
void ModsWildActorBattleReset(void);
void ModsWildActorsUpdate(void);
void ModsWildActorPrepareSaveObject(u32 slot, struct ObjectEvent *copy);
bool32 ModsWildActorBeforeRemove(struct ObjectEvent *object, struct RhhWildActorSnapshot *out);
void ModsWildActorAfterRemove(struct ObjectEvent *object, const struct RhhWildActorSnapshot *before, u32 cause);
bool32 ModsWildActorCopyMon(struct ObjectEvent *object, struct Pokemon *out);
void ModsWildActorCatchCapture(const struct BoxPokemon *source, u32 battler, const struct RhhCatchPlacement *placement);
bool32 ModsWildActorBattleTracking(void);
void ModsWildActorCatchCommitted(const struct RhhCatchPlacement *placement, const struct BoxPokemon *destination);
void ModsWildActorBattleBind(u64 battleId);
void ModsWildActorBattleOpponentSlot(u32 slot);
void ModsWildActorsBeforeSave(void);
void ModsWildActorsAfterSave(void);
void ModsWildActorsAfterLoad(void);
void ModsWildActorsDiscardLive(void);
bool32 ModsWildActorHidden(const struct ObjectEvent *object);
bool32 ModsWildActorGroundCover(const struct ObjectEvent *object);
u32 ModsGroundEffectFlags(const struct ObjectEvent *object, u32 phase, u32 flags);
void ModsWildActorNoteStep(struct ObjectEvent *object);
u32 ModsTrainerSpecies(u32 species, u32 trainer, u32 slot);
u32 ModsStarterSpecies(u32 original, u32 slot);
u32 ModsBaseStat(u32 species, u32 index, u32 original);
u32 ModsLevelCap(void);
bool32 ModsHardCap(void);
void ModsNormalizePartyMon(struct Pokemon *mon);
void ModsEnforceParty(void);
void ModsSyncInventory(void);
u32 ModsClampExperience(struct Pokemon *mon, u32 exp);
u32 ModsFilterHP(struct Pokemon *mon, u32 oldHP, u32 newHP);
bool32 ModsMonIsDead(struct Pokemon *mon);
void ModsMonFainted(struct Pokemon *mon);
void ModsEncounterStarted(void);
bool32 ModsCanCapture(void);
void ModsCaughtMon(void);
void ModsWhiteout(void);
bool32 ModsRunFailed(void);
void ModsOpenNewGameMenu(void (*resume)(void), void (*cancel)(void));
void ModsOpenRunEndedMenu(void (*back)(void));
#endif
#endif
