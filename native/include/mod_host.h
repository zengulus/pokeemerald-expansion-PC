#ifndef RHH_MOD_HOST_H
#define RHH_MOD_HOST_H
#include "mod_api.h"
#include <stdio.h>

/* Includes host-owned state modules as well as external modules. */
#define RHH_HOST_MAX_MODS 128u
struct RhhContentBinding { const char *name; uint32_t kind, engine_id; };
void RhhModsInit(void);
void RhhModsShutdown(void);
int RhhModsSetBeforeSave(void (*callback)(void));
void RhhModsPrepareSave(void);
/* Internal host-owned record compatibility projection. A projection may
 * export a prefix under an older schema; it must never expand the state. */
int RhhModsSetSaveProjection(int (*callback)(const char *id, const uint8_t *state, uint32_t state_size,
                                             uint32_t *schema, uint32_t *size));
int RhhModsRegister(const struct RhhModDefinition *);
int RhhModsFinalize(void);
const char *RhhModsError(void);
void RhhModsSetTrace(FILE *);
void RhhModsSetContentMetadata(void (*provider)(uint32_t kind, uint32_t engine_id, struct RhhContentInfo *));
int RhhModsSetContent(const struct RhhContentBinding *, size_t count);
RhhContentKey RhhContentKeyOf(const char *);
int RhhContentResolve(uint32_t kind, RhhContentKey, uint32_t *engine_id);
const char *RhhContentName(uint32_t kind, uint32_t engine_id);
RhhContentKey RhhContentKeyForId(uint32_t kind, uint32_t engine_id);
uint32_t RhhContentCount(uint32_t kind);
int RhhContentAt(uint32_t kind, uint32_t index, struct RhhContentInfo *out);
int RhhContentInfoFor(uint32_t kind, RhhContentKey key, struct RhhContentInfo *out);
int RhhModsGetSetting(const char *mod_id, const char *setting_id, struct RhhModValue *out);
int RhhModsConfigure(const char *mod_id, const char *setting_id, const struct RhhModValue *value);
uint64_t RhhModRandomAt(uint64_t seed, const char *mod_id, uint64_t key, uint64_t ordinal);
const struct RhhModHostV1 *RhhModsHostAPI(void);
int RhhModsAddService(const char *id, const struct RhhServiceHeader *service);
int RhhModsInstallMonService(void);
int RhhModsPhotoExperienceCommit(uint64_t ticket, uint64_t recipient);
void RhhModsPhotoExperienceNotify(uint64_t ticket);
void RhhModsPhotoExperienceFinish(uint64_t ticket);
void RhhModsObserveOwnedMons(void);
void RhhModsResetOwnedMonWatch(void);
int RhhModsOwnedBattleTeraMon(uint32_t slot, uint32_t level, uint64_t identity, struct RhhOwnedMonSnapshot *out);
int RhhMonSnapshotsEqual(const struct RhhOwnedMonSnapshot *, const struct RhhOwnedMonSnapshot *);
int RhhModsBattlerAbility(uint32_t battler, struct RhhBattlerAbility *out);
int RhhModsAbilityObservationContext(struct RhhBattlerAbilityObservation *out);
int RhhModsAbilityMutationContext(struct RhhBattlerAbilityMutation *out);
int RhhModsAbilityContactMutationContext(struct RhhBattlerAbilityContactMutation *out);
int RhhModsGasChangeContext(struct RhhGasChange *out);
void RhhModsObserveBattleEnvironment(void);
void RhhModsObserveBattleAbilities(void);
void RhhModsObserveBattleHealth(void);
void RhhModsObserveBattleTypes(void);
int RhhModsInnateObservationContext(struct RhhBattlerInnateObservation *out);
void RhhModsObserveBattleInnates(void);
int RhhModsAbilityViewAllowed(void);
int RhhModsSpeciesInnates(RhhContentKey species, uint32_t level, struct RhhInnateList *out);
int RhhModsPersonalizeOwnedInnates(const struct RhhOwnedMonSnapshot *mon, struct RhhInnateList *list);
int RhhModsPersonalizePartyInnates(uint32_t slot, RhhContentKey species, uint32_t level, uint64_t identity, struct RhhInnateList *list);
int RhhModsPersonalizeBattleInnates(const struct RhhInnateBattleContext *context, struct RhhInnateList *list);
int RhhModsBattlerInnates(uint32_t battler, struct RhhInnateResolution *out);
int RhhModsPowerInnates(uint32_t role, struct RhhInnateResolution *out);
int RhhModsInnatePowerContext(struct RhhInnatePowerContext *out);
int RhhModsPriorityInnates(struct RhhInnateResolution *out);
int RhhModsInnatePriorityContext(struct RhhInnatePriorityContext *out);
int RhhModsSpeedInnates(struct RhhInnateResolution *out);
int RhhModsInnateSpeedContext(struct RhhInnateSpeedContext *out);
int RhhModsDamageStatInnates(uint32_t role, struct RhhInnateResolution *out);
int RhhModsInnateDamageStatContext(struct RhhInnateDamageStatContext *out);
int RhhModsCriticalStageInnates(uint32_t role, struct RhhInnateResolution *out);
int RhhModsInnateCriticalStageContext(struct RhhInnateCriticalStageContext *out);
int RhhModsFinalDamageInnates(uint32_t role, struct RhhInnateResolution *out);
int RhhModsInnateFinalDamageContext(struct RhhInnateFinalDamageContext *out);
int RhhModsInnateAccuracyContext(struct RhhInnateAccuracyContext *out);
int RhhModsAccuracyInnates(uint32_t role, struct RhhInnateResolution *out);
int RhhModsInnateEntryContext(struct RhhInnateEntryContext *out);
int RhhModsInnateTurnContext(struct RhhInnateTurnContext *out);
int RhhModsAiInnateEntryFieldContext(struct RhhAiInnateFieldContext *out);
int RhhModsAiInnateEntryFieldProjected(struct RhhAiInnateFieldContext *out);
int RhhModsOfferAiInnateEntryWeather(uint32_t weather);
int RhhModsOfferAiInnateEntryTerrain(uint32_t terrain);
int RhhModsInnateEntryWeather(uint32_t weather);
int RhhModsInnateEntryTerrain(uint32_t terrain);
int RhhModsInnateTurnWeather(uint32_t weather);
int RhhModsInnateTurnTerrain(uint32_t terrain);
int RhhModsInnateTurnMessage(const char *text);
int RhhModsInnateTurnHeal(uint32_t numerator, uint32_t denominator);
int RhhModsInnateTurnStat(uint32_t stat, int32_t stages);
int RhhModsInnateTurnHealTarget(uint32_t battler, uint32_t numerator, uint32_t denominator);
int RhhModsInnateTurnStatTarget(uint32_t battler, uint32_t stat, int32_t stages);
int RhhModsInnateTurnStatFrom(uint32_t battler, uint32_t stat, int32_t stages);
int RhhModsInnateTurnStatusTarget(uint32_t battler, uint32_t status);
int RhhModsAiInnateEntryContext(struct RhhAiInnateEntryContext *out);
int RhhModsOfferAiInnateEntryHeal(uint32_t numerator, uint32_t denominator);
int RhhModsAiInnateTurnHealContext(struct RhhAiInnateEntryContext *out);
int RhhModsOfferAiInnateTurnHeal(uint32_t numerator, uint32_t denominator);
int RhhModsAiInnateTurnStepContext(struct RhhAiInnateEntryContext *out);
int RhhModsOfferAiInnateTurnStepHeal(uint32_t numerator, uint32_t denominator);
int RhhModsAiInnateEntryStatContext(struct RhhAiInnateStatContext *out);
int RhhModsOfferAiInnateEntryStat(uint32_t stat, int32_t stages);
int RhhModsAiInnateEntryOpponentContext(struct RhhAiInnateOpponentStatContext *out);
int RhhModsOfferAiInnateEntryOpponentStat(uint32_t stat, int32_t stages);
int RhhModsInnateEntryMessage(const char *text);
int RhhModsInnateEntryHeal(uint32_t numerator, uint32_t denominator);
int RhhModsInnateEntryHealTarget(uint32_t battler, uint32_t numerator, uint32_t denominator);
int RhhModsInnateEntryStatTarget(uint32_t battler, uint32_t stat, int32_t stages);
int RhhModsInnateEntryStatFrom(uint32_t battler, uint32_t stat, int32_t stages);
int RhhModsInnateEntryStatusTarget(uint32_t battler, uint32_t status);
int RhhModsAiInnateEntryOpponentStatusContext(struct RhhAiInnateOpponentStatusContext *out);
int RhhModsOfferAiInnateEntryOpponentStatus(uint32_t status);
int RhhModsInnateEntryStat(uint32_t stat, int32_t stages);
uint32_t RhhModsCustomAbilityCount(void);
const struct RhhModAbility *RhhModsCustomAbilityAt(uint32_t index);
const struct RhhModAbility *RhhModsCustomAbility(RhhContentKey key);
int RhhModsAbilityInfo(RhhContentKey key, struct RhhAbilityInfo *out);
int RhhModsAbilityName(RhhContentKey key, char *out, uint32_t capacity);
int RhhModsInstallUiService(void);
int RhhModsInstallBattleService(void);
int RhhModsInstallBossAceService(void);
int RhhModsInstallWorldService(void);
int RhhModsInstallEcologyService(void);
uint64_t RhhEcologySystemTime(void);
void RhhEcologyResumeClock(uint64_t now);
void RhhEcologyOnFrame(void);
uint32_t RhhEcologyFrequencyAt(uint64_t map, uint64_t species, uint64_t now);
int RhhEcologyDepleteAt(uint64_t map, uint64_t species, uint64_t now);
int RhhModsInstallPhotoRewardService(void);
int RhhModsInstallRenderService(void);
int RhhModsInstallEcologyTagsService(void);
int RhhModsInstallHabitatRegistryService(void);
int RhhModsResolveHabitatWeather(const struct RhhHabitatSample *, struct RhhHabitatWeather *);
int RhhModsInstallTypeCatalogService(void);
int RhhModsCustomTypeMatchup(RhhContentKey attack, RhhContentKey defense, uint32_t *base);
const struct RhhModMove *RhhModsFindCustomMove(RhhContentKey key);
uint32_t RhhModsBattleConditionCount(void);
const struct RhhModBattleCondition *RhhModsBattleConditionAt(uint32_t index);
int RhhModsBattleConditionIndex(RhhContentKey key);
int RhhModsSetMoveEffectValidator(int (*validator)(RhhContentKey, const struct RhhModMove *, uint32_t));
uint32_t RhhModsRegisteredEffectCount(void);
const struct RhhModMoveEffect *RhhModsRegisteredEffectAt(uint32_t index);
const struct RhhModMoveEffect *RhhModsRegisteredEffect(RhhContentKey key);
RhhContentKey RhhModsRegisteredEffectForMove(RhhContentKey move);
uint32_t RhhModsLearnsetCount(void);
const struct RhhModLearnset *RhhModsLearnsetAt(uint32_t index);
int RhhModsLearnsetRange(RhhContentKey species, uint32_t kind, uint32_t *begin, uint32_t *end);
void ModsClearLearnsets(void);
void ModsPrepareLearnsets(void);
struct LevelUpMove;
const struct LevelUpMove *ModsLevelUpLearnset(uint32_t species, const struct LevelUpMove *base);
const uint16_t *ModsOtherLearnset(uint32_t species, uint32_t kind, const uint16_t *base);
void ModsResetCustomMoves(void);
int ModsOfferMoveAdditionalEffect(const struct RhhMoveAdditionalEffect *effect);
int ModsCustomMoveAvailable(const struct RhhModMove *move);
int ModsTeachMove(const struct RhhMoveTeachRequest *request, struct RhhMoveTeachResult *out);
int ModsTeachMoveContext(struct RhhMoveTeachResult *out);
int ModsPartyMoves(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedMoveSet *out);
/* Ephemeral engine slots. They are valid only while the current catalog is
 * finalized and must never be serialized or exposed as persistent IDs. */
#define RHH_RUNTIME_TYPE_FIRST 32u
#define RHH_RUNTIME_TYPE_COUNT (256u - RHH_RUNTIME_TYPE_FIRST)
int RhhModsCustomTypeSlot(RhhContentKey key, uint32_t *slot);
RhhContentKey RhhModsCustomTypeKey(uint32_t slot);
/* Host-only persisted move-slot mapping. Allocate only after validating a
 * registered custom move; an occupied slot is never reused within a run. */
#define RHH_RUNTIME_MOVE_FIRST 1536u
#define RHH_RUNTIME_MOVE_COUNT (2048u - RHH_RUNTIME_MOVE_FIRST)
int RhhModsMoveSlot(RhhContentKey key, int allocate, uint32_t *slot);
RhhContentKey RhhModsMoveKey(uint32_t slot);
int RhhModsInstallEnvironmentService(void);
uint32_t RhhModsSceneLightCount(void);
uint32_t RhhModsSceneShadowCasterCount(void);
int RhhModsSceneLightsForTile(RhhContentKey map, int32_t x, int32_t y, uint32_t period,
                              const struct RhhModSceneLight **out, uint32_t *count);
int RhhModsRegisteredKey(RhhContentKey key);
void RhhModsEnvironmentOnStep(uint64_t step);
void RhhModsEnvironmentOnFrame(void);
void RhhModsEnvironmentQueueBattle(uint32_t weather, uint32_t terrain);
void RhhModsObserveNaturalWeather(void);
void RhhModsResetNaturalWeatherObservation(void);
int RhhModsEnvironmentProjectSave(const char *id, const uint8_t *state, uint32_t stateSize,
                                  uint32_t *schema, uint32_t *size);
int RhhModsWildQueueProjectSave(const char *id, const uint8_t *state, uint32_t stateSize,
                                uint32_t *schema, uint32_t *size);
void RhhModsWildBehaviorOnStep(const struct RhhWorldStepSnapshot *step);
/* Engine adapter scope: all nested time samples share one observation. */
struct RhhWorldTimeScope {
    const struct RhhWorldTimeScope *previous;
    struct RhhWorldTimeSnapshot sample;
    int available;
};
void RhhModsWorldTimeScopeBegin(struct RhhWorldTimeScope *scope);
void RhhModsWorldTimeScopeEnd(struct RhhWorldTimeScope *scope);
const struct RhhModShader *RhhModsFindShader(RhhContentKey key);
int RhhModsShaderPhotoPreservesPosition(RhhContentKey key);
int RhhModsShaderPhotoPreservesGeometry(RhhContentKey key);
const struct RhhModMusic *RhhModsFindMusic(RhhContentKey key);
uint32_t RhhModsMusicCount(void);
int RhhModsMusicAt(uint32_t index, RhhContentKey *key, const struct RhhModMusic **out);
int NativeMusicInstallService(void);
int RhhModsSelectSceneRule(RhhContentKey map, int32_t x, int32_t y, uint32_t metatile,
                           uint32_t behavior, const struct RhhModSceneRule **out);
int RhhModsDrawWorld(const struct RhhWorldFrame *frame);
int RhhModsDrawWorldWithIdentity(const struct RhhWorldFrame *frame, RhhContentKey *presenter);
unsigned RhhModsCollectWorldPasses(uint32_t frame, uint32_t weather, uint32_t terrain,
                                  struct RhhWorldRenderPass *out);
unsigned RhhModsCollectBattlePasses(uint32_t frame, uint32_t weather, uint32_t terrain,
                                   uint32_t suppressed, uint32_t doubles, struct RhhBattleRenderPass *out);
void RhhModsPhotoPresent(const uint16_t *pixels, int shader, int world, uint32_t frame);
int RhhModsPhotoTarget(uint32_t *slot, uint64_t *ticket);
int RhhModsProjectedVisibilityTarget(uint32_t *slot, uint64_t *ticket);
void RhhModsProjectedVisibilityFinish(const uint16_t *pixels, uint32_t frame, int shader, int world);
void RhhModsShutdownWorldPresenters(void);
void *RhhModsState(const char *id); /* host adapter only; modules receive borrowed state */
void RhhModsResetState(void);
/* Save-loader checkpoint only: preserve new-game setting locks on rollback. */
int RhhModsSettingsLocked(void);
void RhhModsRestoreSettingsLock(int locked);
void RhhModsDispatch(const struct RhhModEvent *, struct RhhModDecision *);
int RhhModsHasActiveRun(void);
int RhhModsFinalized(void);
int RhhModsRunCommitted(void);
int RhhModsCanPersistDurably(void);
int RhhModsHasHook(uint32_t event);
int RhhModsDormantProfileHooksCompatible(void);
int RhhModsCommandsAllowed(void);
void RhhModsSetDrawing(int drawing);
int RhhModsSetLifecycleCallback(void (*callback)(void));
uint32_t RhhModsCurrentEvent(void);
RhhContentKey RhhModsCurrentOwner(void);
int RhhModsArchiveObserverAllowed(void);
int RhhModsSetUiInputOwnerProvider(RhhContentKey (*provider)(void));
RhhContentKey RhhModsUiInputOwner(void);
int RhhModsCurrentStage(void);
uint64_t RhhModsRunSeed(void);
uint64_t RhhModsRunId(void);
/* Bounded, checksummed, little-endian records. Caller owns exported buffer. */
int RhhModsExport(uint8_t **data, uint32_t *size);
int RhhModsImport(const uint8_t *data, uint32_t size);
int RhhModsMergeJournal(const uint8_t *data, uint32_t size);
void RhhModsSetPersistence(int (*persist)(uint64_t run_id, const uint8_t *data, uint32_t size));
void RhhModsSetRunContext(uint64_t seed, uint64_t run_id, int committed);
int RhhModsFlushDurable(void);
int RhhModsLoadLibrary(const char *path);
void RhhModsUnloadLibraries(void);

#endif
