#ifndef GUARD_WILD_ENCOUNTER_OW_H
#define GUARD_WILD_ENCOUNTER_OW_H

#define OWE_SPAWNS_MAX                  4
#define OWE_APPROACH_DISTANCE           2
#define OWE_APPROACH_JUMP_TIMER_MIN     16
#define OWE_APPROACH_JUMP_TIMER_MAX     64
#define OWE_FLEE_COLLISION_TIME         6   // If a fleeing mon is unable to take a step for this many tries it will despawn. (Multiply this value by 16 to get number of frames.)
#define OWE_DESPAWN_FRAMES              30  // Number of frames before a mon despawns after noticing the player (OWE_BEHAVIOR_DESPAWN)

enum SpawnDespawnTypeOWE
{
    OWE_SPAWN_ANIM_GRASS,
    OWE_SPAWN_ANIM_LONG_GRASS,
    OWE_SPAWN_ANIM_WATER,
    OWE_SPAWN_ANIM_UNDERWATER,
    OWE_SPAWN_ANIM_CAVE,
    OWE_SPAWN_ANIM_SHINY
};

enum TypeOWE
{
    OWE_ANY,
    OWE_GENERATED,
    OWE_MANUAL,
    OWE_NONE
};

// OWE_SPEED_FASTER seems to visually bug out sometimes.
enum SpeedOWE
{
    OWE_SPEED_NORMAL,
    OWE_SPEED_SLOW,
    OWE_SPEED_FAST,
    OWE_SPEED_FASTER
};

enum ReturnToIdleOWE
{
    NEVER_RETURN,
    PLAYER_OUTSIDE_ACTIVE_RANGE,
    PLAYER_CANT_BE_SEEN
};

struct BehaviorOWE
{
    u32 movementType:8;
    u32 viewDistance:4;
    u32 viewWidth:4;
    u32 activeDistance:4;
    enum SpeedOWE idleSpeed:2;
    enum SpeedOWE activeSpeed:2;
    enum ReturnToIdleOWE returnToIdle:2;
    u32 padding:6;
};

enum __attribute__((packed)) OverworldWildEncounterBehaviors
{
    OWE_IGNORE_PLAYER,
    OWE_CHASE_PLAYER_SLOW,
    OWE_FLEE_PLAYER_NORMAL,
    OWE_WATCH_PLAYER_NORMAL,
    OWE_APPROACH_PLAYER_SLOW,
    OWE_DESPAWN_ON_NOTICE,
    OWE_SPECIES_BEHAVIOR_COUNT
};

struct FieldEffectInfoOWE
{
    s16 xOffset;
    s16 yOffset;
    u32 visual;
};

extern const struct FieldEffectInfoOWE gOverworldWildEncounterFieldEffectInfo[];

void UpdateOverworldWildEncounter(void);
#ifdef NATIVE
u32 PreflightModOverworldWildEncounter(s32 x, s32 y, u32 *spawnSlot, bool32 *water);
u32 PreflightModOverworldWildSpecies(s32 x, s32 y, u32 species, u32 level, u32 *spawnSlot, bool32 *water);
u32 TrySpawnModOverworldWildEncounter(s32 x, s32 y, u32 *slot);
u32 TrySpawnModOverworldWildSpecies(s32 x, s32 y, u32 species, u32 level, u32 *slot);
bool32 IsOrdinaryGeneratedOverworldWildEncounter(struct ObjectEvent *object);
bool32 NativeOWEReplacesRandom(u16 metatileBehavior);
struct Pokemon;
u32 TrySpawnModOverworldWildPreserved(s32 x, s32 y, const struct Pokemon *source, u32 facing, u32 elevation, bool32 noDespawn, u32 *slot);
#endif
bool32 IsOverworldWildEncounter(struct ObjectEvent *owe, enum TypeOWE oweType);
void SetMinimumOWESpawnTimer(void);
void TryTriggerOverworldWildEncounter(struct ObjectEvent *obstacle, struct ObjectEvent *collider);
const u8 *GetOverworlWildEncounterScript(u32 objectEventId);
void OnOverworldWildEncounterSpawn(struct ObjectEvent *owe);
void OnOverworldWildEncounterDespawn(struct ObjectEvent *owe);
bool32 IsOWEDespawnExempt(struct ObjectEvent *owe);
bool32 DespawnOWEDueToNPCCollision(struct ObjectEvent *obstacle, struct ObjectEvent *activeObject);
void DespawnAllOverworldWildEncounters(enum TypeOWE oweType, u32 flags);
u32 TryAndDespawnOldestGeneratedOWE_ToFreeObject();
void DespawnOWEOnBattleStart(void);
void TryDespawnOWEsCrossingMapConnection(void);
void RestoreSavedOWEBehaviorState(struct ObjectEvent *owe, struct Sprite *sprite);
void SetSavedOWEMovementState(struct ObjectEvent *owe);
void ClearSavedOWEMovementState(struct ObjectEvent *owe);
bool32 CheckRestrictedOWEMovement(struct ObjectEvent *owe, enum Direction direction);
#ifdef NATIVE
bool32 CheckRestrictedOWEMovementNoEffects(struct ObjectEvent *owe, enum Direction direction, u8 *overwrite);
bool32 CheckRestrictedOWEMovementFromNoEffects(const struct ObjectEvent *owe, s16 x, s16 y,
                                                u8 elevation, u8 behavior, u8 facing,
                                                enum Direction direction, u8 *overwrite);
#endif
bool32 CanAwareOWESeePlayer(struct ObjectEvent *owe);
bool32 IsPlayerInsideOWEActiveDistance(struct ObjectEvent *owe);
bool32 IsOWENextToPlayer(struct ObjectEvent *owe);
enum Direction DirectionOfOWEToPlayerFromCollision(struct ObjectEvent *owe);
u32 GetApproachingOWEDistanceToPlayer(struct ObjectEvent *owe, bool32 *equalDistances);
u32 GetOWEWalkMovementActionInDirectionWithSpeed(enum Direction direction, enum SpeedOWE speed);
bool32 TryPlayAmbientCryOWE(void);
u32 GetNumberOfActiveOWEs(enum TypeOWE oweType);
#ifdef NATIVE
u32 ModsWildGeneratedFreeSlot(void);
#endif
const struct ObjectEventTemplate TryGetObjectEventTemplateForOWE(const struct ObjectEventTemplate *template);
struct SpritePalette GetOWESpawnDespawnAnimFldEffPalette(enum SpawnDespawnTypeOWE spawnAnim);
u32 RemoveOldestGeneratedOWE(void);
bool32 CanRemoveObjectForOWEMovement(struct ObjectEvent *objectEvent);

extern const u8 InteractWithOverworldWildEncounter[];

#endif // GUARD_WILD_ENCOUNTER_OW_H
