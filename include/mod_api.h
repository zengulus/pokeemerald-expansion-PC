#ifndef RHH_MOD_API_H
#define RHH_MOD_API_H

/* Public native mod ABI. This header deliberately has no engine dependencies.
 * See MODDING.md for ownership, ordering, persistence and compatibility rules. */
#include <stddef.h>
#include <stdint.h>

#define RHH_MOD_ABI_V1 0x00010000u
/* Modules declaring service requirements must use this ABI so older hosts
 * reject them instead of silently ignoring those requirements. */
#define RHH_MOD_ABI_SERVICES 0x00010001u
/* Older hosts reject music declarations rather than silently ignoring them. */
#define RHH_MOD_ABI_MUSIC 0x00010002u
/* Scene rules require a host that understands and validates their extension. */
#define RHH_MOD_ABI_SCENE 0x00010003u
/* Declarative scene lights require the append-only light declaration ABI. */
#define RHH_MOD_ABI_SCENE_LIGHTS 0x00010004u
#define RHH_MOD_ABI_ECOLOGY 0x00010005u
/* Hosts predating this ABI must reject declared custom type definitions. */
#define RHH_MOD_ABI_TYPES 0x00010006u
/* Hosts predating this ABI must reject declared custom move definitions. */
#define RHH_MOD_ABI_MOVES 0x00010007u
/* Hosts predating this ABI must reject learnset additions. */
#define RHH_MOD_ABI_LEARNSETS 0x00010008u
/* Hosts predating this ABI must reject battle condition declarations. */
#define RHH_MOD_ABI_CONDITIONS 0x00010009u
/* Effect declarations and bindings require a host that validates them. */
#define RHH_MOD_ABI_MOVE_EFFECTS 0x0001000au
#define RHH_MOD_ABI_DAMAGE_EFFECTS 0x0001000bu
/* Custom ability definitions require a host that validates this extension. */
#define RHH_MOD_ABI_ABILITIES 0x0001000cu
#define RHH_MOD_ABI_HABITATS 0x0001000du
#define RHH_MOD_REQUIRED 1u
#define RHH_MOD_DURABLE 2u
#define RHH_MOD_ID_MAX 64

typedef uint64_t RhhContentKey;
enum RhhContentKind {
    RHH_SPECIES = 1, RHH_MOVE, RHH_ITEM, RHH_ABILITY,
    RHH_TRAINER, RHH_MAP, RHH_AREA, RHH_MUSIC, RHH_MIDI, RHH_SOUNDFONT, RHH_TYPE
};
enum RhhModEventKind {
    RHH_WILD_SPECIES = 1, RHH_TRAINER_SPECIES, RHH_STARTER_SPECIES,
    RHH_BASE_STATS, RHH_LEVEL_CAP, RHH_CAN_CAPTURE, RHH_CAN_REVIVE,
    RHH_ENCOUNTER_STARTED, RHH_MON_FAINTED, RHH_RUN_STARTED,
    RHH_RUN_LOADED, RHH_BATTLE_MUSIC, RHH_FOLLOWER_ENABLED, RHH_MON_IS_DEAD,
    RHH_CAN_CONTINUE, RHH_INVENTORY_CHANGED, RHH_WHITEOUT, RHH_WILD_CREATED,
    RHH_PERSISTENCE_FAILED, RHH_MON_CAUGHT,
    RHH_BATTLE_ENVIRONMENT, RHH_BATTLE_ENVIRONMENT_NOTICE,
    RHH_WORLD_PRESENTATION, RHH_CAN_EDIT_MON, RHH_MON_EDITED, RHH_MOD_UPDATE,
    RHH_MUSIC_SELECT, RHH_BREED_COMPATIBILITY, RHH_BREED_OFFSPRING,
    RHH_BREED_EGG_CREATED, RHH_BREED_INHERIT_IV, RHH_BREED_INHERIT_MOVE, RHH_EGG_HATCHED, RHH_WORLD_STEP,
    RHH_CAN_WORLD_ENVIRONMENT, RHH_WORLD_ENVIRONMENT_CHANGED,
    RHH_BATTLE_ENVIRONMENT_SEED,
    RHH_HABITAT_QUERY,
    RHH_WILD_CANDIDATE, RHH_WILD_GENERATED,
    RHH_WILD_WEIGHT,
    RHH_WILD_ACTOR_SPAWNED, RHH_WILD_ACTOR_REMOVED, RHH_WILD_ACTOR_ENGAGED,
    RHH_WILD_ACTOR_BATTLE_ENDED, RHH_WILD_ACTOR_IDENTIFIED,
    RHH_CAN_WILD_CONCEALMENT, RHH_WILD_CONCEALMENT_CHANGED,
    RHH_CAN_GENERATE_WILD, RHH_WILD_CANCELLED, RHH_WILD_SPECIAL_GENERATED, RHH_CAN_GENERATE_SPECIAL_WILD, RHH_WILD_SPECIAL_CANCELLED,
    RHH_CAN_GENERATE_DEXNAV_WILD, RHH_WILD_DEXNAV_GENERATED, RHH_WILD_DEXNAV_CANCELLED,
    RHH_CAN_GENERATE_FACILITY_WILD, RHH_WILD_FACILITY_GENERATED, RHH_WILD_FACILITY_CANCELLED,
    RHH_PARTY_MENU_ACTIONS,
    RHH_BATTLE_STARTED, RHH_BATTLER_ENTERED, RHH_BATTLE_ENDED,
    RHH_WILD_SCRIPTED_READY,
    RHH_CAN_SCOUT, RHH_SCOUT_RECORDED,
    RHH_CAN_PHOTO, RHH_PHOTO_CAPTURED, RHH_PHOTO_FAILED,
    RHH_PHOTO_STORED, RHH_PHOTO_REMOVED,
    RHH_PHOTO_SCORE,
    RHH_WORLD_ENVIRONMENT_PRESENTATION,
    RHH_WORLD_RENDER_PASSES,
    RHH_WORLD_LIGHTS,
    RHH_WILD_TRAIT_QUERY,
    RHH_CAN_EDIT_WILD_TRAIT, RHH_WILD_TRAIT_EDITED,
    RHH_WILD_BEHAVIOR_PROPOSE, RHH_WILD_BEHAVIOR_RESULT,
    RHH_TYPE_MATCHUP,
    RHH_TYPE_STAB,
    RHH_SPECIES_TYPES,
    RHH_TYPE_STAB_EXTENDED,
    RHH_MOVE_BASE_TYPE,
    RHH_CAN_TEACH_MOVE,
    RHH_MOVE_TAUGHT,
    RHH_MOVE_POWER,
    RHH_MOVE_ACCURACY,
    RHH_MOVE_BASE_TARGET,
    RHH_WORLD_ENVIRONMENT_MOVE,
    RHH_MOVE_ADDITIONAL_EFFECTS,
    RHH_MOVE_RESOLVED,
    RHH_BATTLE_TURN_ENDED,
    RHH_AI_MOVE_SCORE,
    RHH_MOVE_EFFECT_MODE,
    RHH_MOVE_EFFECT_APPLY,
    RHH_BATTLE_CONDITION_CHANGED,
    RHH_REGISTERED_MOVE_EFFECT,
    RHH_MOVE_DAMAGE_EFFECT_MODE,
    RHH_MOVE_DAMAGE_EFFECT_APPLY,
    RHH_AI_EFFECT_PREVIEW,
    RHH_INNATE_PROPOSE,
    RHH_INNATE_POWER,
    RHH_INNATE_ACCURACY,
    RHH_INNATE_ENTRY,
    RHH_OWNED_MON_CHANGED,
    RHH_INNATE_PERSONALIZE,
    RHH_CAN_ACCEPT_DAYCARE_EGG,
    RHH_WILD_ACTIVITY_COLLECT,
    RHH_WILD_ACTIVITY_CHANGED,
    RHH_CAN_CONFIGURE_BOSS_ACE,
    RHH_BOSS_ACE_CONFIGURED,
    RHH_CAN_SET_MUSIC_ASSIGNMENT,
    RHH_MUSIC_ASSIGNMENT_CHANGED,
    RHH_MUSIC_IMPORTED,
    RHH_MUSIC_IMPORT_REMOVED,
    RHH_CAN_EDIT_WILD_PROFILE,
    RHH_WILD_PROFILE_EDITED,
    RHH_LEARNSET_ENTRY,
    RHH_WILD_ENCOUNTER_RATE,
    RHH_WILD_SPAWN_INTERVAL,
    RHH_WILD_SCRIPTED_CANDIDATE,
    RHH_CAN_GENERATE_MANUAL_FACILITY_WILD,
    RHH_WILD_MANUAL_FACILITY_GENERATED,
    RHH_WILD_MANUAL_FACILITY_CANCELLED,
    RHH_INNATE_TURN_END,
    RHH_AI_INNATE_ENTRY_PREVIEW,
    RHH_AI_INNATE_ENTRY_STAT_PREVIEW,
    RHH_FISHING_BITE_RATE,
    RHH_BATTLE_CONDITION_PROPOSAL,
    RHH_OWNED_MON_RELEASED,
    RHH_BREED_IV_COUNT,
    RHH_BATTLE_TERA_TYPE,
    RHH_MOVE_PP_COST,
    RHH_AI_MOVE_PP_COST,
    RHH_CAN_SELECT_MOVE,
    RHH_CAN_BEGIN_SCRIPTED_WILD,
    RHH_WILD_SCRIPTED_CANCELLED,
    RHH_WILD_EVENT_CANDIDATE,
    RHH_CAN_BEGIN_EVENT_WILD,
    RHH_WILD_EVENT_CANCELLED,
    RHH_WILD_SCRIPTED_CLEANUP_FINISHED,
    RHH_WILD_EVENT_CLEANUP_FINISHED,
    RHH_WILD_GHOST_READY,
    RHH_WILD_TUTORIAL_READY,
    RHH_WILD_FIRST_BATTLE_READY,
    RHH_CAN_BEGIN_TUTORIAL_WILD,
    RHH_WILD_TUTORIAL_CANCELLED,
    RHH_WILD_TUTORIAL_CLEANUP_FINISHED,
    RHH_CAN_CLAIM_PHOTO_REWARD,
    RHH_PHOTO_REWARD_CLAIMED,
    RHH_PHOTO_REWARD_DELIVERED,
    RHH_CAN_APPLY_BATTLE_EVS,
    RHH_BATTLE_EVS_PROCESSED,
    RHH_EV_LIMITS,
    RHH_OWNED_MON_MOVED,
    RHH_PARTY_REORDERED,
    RHH_OWNED_DAYCARE_TRANSFERRED,
    RHH_OWNED_FUSION_TRANSFERRED,
    RHH_OWNED_CATCH_PLACED,
    RHH_OWNED_INGAME_TRADE_COMPLETED,
    RHH_OWNED_LINK_TRADE_SAVED,
    RHH_OWNED_MON_PICKED_UP,
    RHH_OWNED_STORAGE_UNREADABLE,
    RHH_OWNED_GIFT_RECEIVED,
    RHH_OWNED_EVOLUTION_COMMITTED,
    RHH_WILD_PROFILE_SCHEMA,
    RHH_WILD_PROFILE_SCHEMA_COLLECT,
    RHH_CAN_AWARD_EXPERIENCE, RHH_EXPERIENCE_AWARDED, RHH_EXPERIENCE_FLOW_FINISHED,
    RHH_CAN_CLAIM_PHOTO_EXPERIENCE, RHH_PHOTO_EXPERIENCE_CLAIMED, RHH_PHOTO_EXPERIENCE_DELIVERED,
    RHH_CAN_RETARGET_PHOTO_EXPERIENCE, RHH_PHOTO_EXPERIENCE_RETARGETED,
    RHH_AI_INNATE_TURN_HEAL_PREVIEW,
    RHH_AI_INNATE_TURN_STEP_PREVIEW,
    RHH_OWNED_DEBUG_CREATED,
    RHH_OWNED_MYSTERY_GIFT_RECEIVED,
    RHH_BREED_NATURE,
    RHH_ITEM_EV_PROPOSAL, RHH_ITEM_EV_PROCESSED,
    RHH_BATTLE_EV_PROPOSAL,
    RHH_CAN_REDUCE_ITEM_EVS, RHH_ITEM_EV_REDUCTION_PROCESSED,
    RHH_BREED_IV_RULE,
    RHH_BREED_MOVE_RULE,
    RHH_CAN_REQUEST_DAYCARE_EGG, RHH_DAYCARE_EGG_REQUESTED,
    RHH_DAYCARE_PENDING_CHANGED,
    RHH_BREED_RNG_POLICY,
    RHH_CAN_EDIT_WILD_QUEUE, RHH_WILD_QUEUE_CHANGED,
    RHH_WORLD_GROUND_EFFECT,
    RHH_CAN_WATER_BERRY_PLOT, RHH_BERRY_PLOT_WATERED,
    RHH_BREED_BALL_RULE, RHH_BREED_ABILITY_RULE,
    RHH_INNATE_BATTLE_PERSONALIZE,
    RHH_BATTLER_ABILITY_OBSERVED,
    RHH_BATTLER_INNATES_OBSERVED,
    RHH_CAN_FIELD_HEAL, RHH_FIELD_HEALED,
    RHH_MOVE_TARGET_RESOLVE,
    RHH_WILD_ACTOR_STEP_FINISHED,
    RHH_CAN_CURE_FIELD_STATUS, RHH_FIELD_STATUS_CURED,
    RHH_FIELD_POISON_DAMAGE, RHH_FIELD_POISON_DAMAGED,
    RHH_TYPE_LEGACY_CATEGORY,
    RHH_MUSIC_GAIN,
    RHH_CAN_SET_MUSIC_LEVEL, RHH_MUSIC_LEVEL_CHANGED,
    RHH_CAN_SET_WILD_BEHAVIOR_POLICY, RHH_WILD_BEHAVIOR_POLICY_CHANGED,
    RHH_MUSIC_PLAYBACK_CHANGED, RHH_MUSIC_LOOP,
    RHH_BATTLER_ABILITY_MUTATED,
    RHH_BATTLER_ABILITY_CONTACT_MUTATED,
    RHH_BATTLE_NEUTRALIZING_GAS_CHANGED,
    RHH_UI_ACCESSIBILITY_CHANGED,
    RHH_UI_OVERLAY_ACCESSIBILITY_CHANGED,
    RHH_BATTLER_STATUS_APPLIED,
    RHH_BATTLER_ITEM_STATUS_CHANGED,
    RHH_BATTLER_STATUS_CURED,
    RHH_BATTLE_PARTY_STATUS_CURED,
    RHH_MOVE_BASE_PRIORITY,
    RHH_MOVE_BATTLER_PRIORITY,
    RHH_NATURAL_WEATHER_CHANGED,
    RHH_INNATE_PRIORITY,
    RHH_BATTLER_SPEED, RHH_INNATE_SPEED,
    RHH_DAMAGE_STAT, RHH_INNATE_DAMAGE_STAT,
    RHH_BATTLE_ENVIRONMENT_OBSERVED, RHH_BATTLE_ENVIRONMENT_COMMITTED,
    RHH_AI_INNATE_ENTRY_FIELD_PREVIEW,
    RHH_BATTLE_ENVIRONMENT_TIMER_TICKED,
    RHH_BATTLE_WEATHER_SUPPRESSION_CHANGED,
    RHH_BATTLE_WEATHER_GAS_SUPPRESSION_CHANGED,
    RHH_MUSIC_BATTLE_CUE_FINISHED,
    RHH_BATTLE_WEATHER_PRESENCE_SUPPRESSION_CHANGED,
    RHH_BATTLE_PARTY_ITEM_STATUS_CURED,
    RHH_BERRY_PLOT_MANUALLY_WATERED,
    RHH_CAN_HATCH_EGG,
    RHH_EGG_CYCLE_PROGRESS,
    RHH_EGG_CYCLE_TICKED,
    RHH_BERRY_PLOT_INTERACTED,
    RHH_BERRY_PLOT_HARVESTED,
    RHH_BERRY_HARVEST_YIELD,
    RHH_BATTLE_FIELD_RULES_OBSERVED,
    RHH_BATTLE_FIELD_RULES_COMMITTED,
    RHH_BATTLE_SPORTS_OBSERVED,
    RHH_MOVE_PP_DEDUCTED,
    RHH_MOVE_DAMAGE_APPLIED,
    RHH_MOVE_ACCURACY_CHECKED,
    RHH_MOVE_CRITICAL_STAGE,
    RHH_INNATE_CRITICAL_STAGE,
    RHH_BATTLE_RENDER_PASSES,
    RHH_DAMAGE_FINAL,
    RHH_INNATE_DAMAGE_FINAL,
    RHH_SELF_DAMAGE_FINAL,
    RHH_INNATE_SELF_DAMAGE_FINAL,
    RHH_BATTLER_TYPES_OBSERVED,
    RHH_PUBLIC_BATTLER_TYPES_OBSERVED,
    RHH_CAN_WATER_AUTHORED_BERRY, RHH_AUTHORED_BERRY_WATERED,
    RHH_BATTLER_HEALTH_OBSERVED,
    RHH_BERRY_CLOCK_UPDATED,
    RHH_AUTHORED_WILD_WEIGHT,
    RHH_WILD_DORMANT_RELOCATED,
    RHH_CAN_EDIT_DORMANT_WILD_PROFILE, RHH_DORMANT_WILD_PROFILE_EDITED,
    RHH_WILD_DORMANT_CLOCK_ADVANCED,
    RHH_MOD_EVENT_COUNT
};
/* RHH_MOVE_BASE_TYPE: event_key is the move's stable key, subject is
 * its original type key. decision.value starts as that type key; transforms
 * may replace it with a registered type key. Constraints can deny but cannot
 * rewrite the proposal. Invalid or denied proposals retain the original.
 * This changes the base type before ordinary dynamic move/ability rules; it
 * does not register a move. Callbacks must be pure and consistent; gameplay
 * RNG is restored and RNG-consuming proposals are discarded. */
/* Stable semantic values, independent of the engine's weather/terrain enums. */
enum RhhBattleWeather { RHH_WEATHER_NONE, RHH_WEATHER_RAIN, RHH_WEATHER_SUN,
    RHH_WEATHER_SAND, RHH_WEATHER_HAIL, RHH_WEATHER_SNOW, RHH_WEATHER_FOG,
    RHH_WEATHER_WIND, RHH_WEATHER_HARSH_SUN, RHH_WEATHER_HEAVY_RAIN };
enum RhhBattleTerrain { RHH_TERRAIN_NONE, RHH_TERRAIN_GRASSY, RHH_TERRAIN_MISTY,
    RHH_TERRAIN_ELECTRIC, RHH_TERRAIN_PSYCHIC };
enum RhhEnvironmentNotice { RHH_NOTICE_WEATHER_RECAP = 1 };
/* RHH_BATTLE_ENVIRONMENT args: weather, terrain, suppressed, battle scene.
 * Transform decision.value selects a declared shader's content key.
 * RHH_BATTLE_ENVIRONMENT_NOTICE args: notice kind, selected shader usable.
 * Deny only the recap; start/end text and weather mechanics run normally. */
/* GLSL 1.20 fragment program, borrowed until module unload. The host supplies
 * uScene/uMask samplers, uTime (seconds), uWeather/uTerrain semantic values,
 * uSuppressed, uFade (effect visibility), uDoubles, and uResolution (240,160).
 * UV origin is the top left. uMask is 0 for interface, 128/255 for sprite,
 * and 1 for background pixels. A mask greater than 0 permits weather; terrain
 * should be restricted to background pixels. World environment and ordered
 * pass shaders may also use sampler2D uDepth and bool uDepthAvailable. When
 * available, sample uDepth at vec2(vUv.x, 1.0-vUv.y); it is the selected
 * presenter's pre-pass framebuffer depth and does not follow displaced pixels.
 * Battle shaders can also use sampler2D uBattlerMask and bool
 * uBattlerMaskAvailable. When available, uBattlerMask uses top-left UVs and
 * nearest sampling: round(red * 255) is zero for unassigned pixels or 1..4
 * for battler slots 0..3. It describes topmost pixels attributed to verified
 * engine battler sprites before any shader passes; protected UI is zero.
 * Unattributed or mismatched OAM and obscured pixels are zero. The mask does
 * not follow displaced pixels, reveal hidden sprites or identify a durable
 * Pokémon. No battler mask is supplied to world shaders. */
struct RhhModShader { uint32_t struct_size, reserved; const char *id, *fragment; };
/* Optional declaration for a shader owned by the same module. A shader with
 * this flag samples the scene only at the current pixel: it may tint or add
 * effects, but must not displace, blur, duplicate or resample scene pixels.
 * Photo masks still describe layer ownership, not particle opacity. */
/* POSITION is exact per-pixel sampling. GEOMETRY permits a non-displacing
 * local filter that retains a positive contribution from each output pixel's
 * original scene sample; its pre-filter mask is only an approximation. */
enum RhhShaderPhotoFlags { RHH_SHADER_PHOTO_PRESERVES_POSITION = 1u,
    RHH_SHADER_PHOTO_PRESERVES_GEOMETRY = 2u };
struct RhhModShaderPhotoPolicy { uint32_t struct_size, flags; const char *shader_id; };
/* During RHH_WORLD_RENDER_PASSES constraint callbacks, offer copied passes to
 * the world render service. Pass IDs are stable namespaced content keys;
 * shaders must be declared by a loaded module. Lower priority executes first,
 * then lower pass ID. Duplicate IDs or more than the capacity discard the
 * entire offered list for that frame. */
#define RHH_WORLD_RENDER_PASS_CAPACITY 16u
struct RhhWorldRenderPass {
    uint32_t struct_size, reserved;
    int32_t priority;
    uint32_t reserved2;
    RhhContentKey id, shader;
};
/* Battle passes are collected independently on each visible OpenGL battle
 * frame, including when no weather or terrain is active. args are weather,
 * terrain, weather suppression and doubles; ordinal is the visual frame.
 * Lower priority and then lower ID executes first after the selected battle
 * environment shader. Duplicate IDs, overflow or denial discard the chain. */
#define RHH_BATTLE_RENDER_PASS_CAPACITY 16u
struct RhhBattleRenderPass {
    uint32_t struct_size, reserved;
    int32_t priority;
    uint32_t reserved2;
    RhhContentKey id, shader;
};
/* RHH_WORLD_ENVIRONMENT_PRESENTATION selects a declared shader in
 * decision.value. args[0..1] are the current map's semantic weather and
 * terrain; event_key is the map and ordinal is the world step. It runs only
 * for an active effect on the OpenGL overworld. The shader receives the same
 * uniforms and top-left UV convention as battle shaders, after the selected
 * world presenter and before viewfinder overlays. Respect uMask: 0 protects
 * interface pixels, 128 is sprite and 255 is background. The host restores
 * interface pixels after the complete effect chain. Selection is a
 * presentation-only query; the original frame is the fallback. */
/* A world presenter draws into the bound 240x160 OpenGL framebuffer. It may
 * use temporary framebuffers for depth, shadows or post-processing, and must
 * restore GL state before returning. The host composites protected BG0 pixels.
 * Pixels and mask are borrowed for the call, top-left origin, GBA RGB555;
 * the bound OpenGL framebuffer has the usual bottom-left origin.
 * Return zero after rendering or nonzero to fall back to the original scene. */
/* Full subject silhouette before occlusion. Bounds are signed, half-open
 * viewport coordinates; pixels outside the viewport are an extrapolation of
 * the captured sprite geometry. inside_pixels equals the viewport silhouette. */
struct RhhPhotoFootprint {
    uint32_t struct_size, total_pixels, inside_pixels, reserved;
    int32_t left, top, right, bottom;
    uint64_t ticket;
};
/* Borrowed for one presenter draw; masks use top-left row-major bytes 0/1.
 * The presenter sets valid to 1 only after both masks are complete. */
struct RhhWorldPhotoTarget {
    uint32_t struct_size, actor_slot;
    uint64_t ticket;
    uint8_t *visible, *silhouette;
    uint32_t *valid;
    /* v1.1: optional full footprint. Set total_pixels only on success. */
    uint64_t reserved_v1_1;
    struct RhhPhotoFootprint *footprint;
    /* v1.2: optional output; set GEOMETRIC if the presenter's own processing
     * mixes neighboring scene pixels without displacing or removing them. */
    uint32_t *mask_quality;
    /* v1.3: optional presenter-defined, namespaced rendering variant for
     * this draw. Zero means unspecified; the presenter documents its keys. */
    RhhContentKey *render_variant;
};
#define RHH_WORLD_PHOTO_TARGET_V1_0_SIZE offsetof(struct RhhWorldPhotoTarget, reserved_v1_1)
#define RHH_WORLD_PHOTO_TARGET_V1_1_SIZE offsetof(struct RhhWorldPhotoTarget, mask_quality)
#define RHH_WORLD_PHOTO_TARGET_V1_2_SIZE offsetof(struct RhhWorldPhotoTarget, render_variant)
#define RHH_WORLD_PHOTO_TARGET_V1_3_SIZE sizeof(struct RhhWorldPhotoTarget)
enum RhhPhotoMaskQuality { RHH_PHOTO_MASK_EXACT = 0, RHH_PHOTO_MASK_GEOMETRIC = 1 };
struct RhhWorldFrame {
    uint32_t struct_size, width, height, frame;
    const uint16_t *pixels;
    const uint8_t *mask;
    void *(*gl_proc)(const char *name);
    /* Optional append-only output: fill width*height top-left-origin bytes
     * with 128 for visible sprites and 255 for background, then set *valid
     * to 1 only on success. The host preserves original interface pixels and
     * ignores an invalid or absent output. Both pointers are borrowed for
     * this call; check struct_size before accessing them. */
    uint8_t *output_mask;
    uint32_t *output_mask_valid;
    /* Optional OpenGL path: publish a complete width-by-height GL_TEXTURE_2D
     * by writing its name here. Red values below .75 indicate sprites; other
     * values indicate background. The texture uses framebuffer orientation,
     * must be complete without mipmaps, and stays alive until the next draw or
     * shutdown. The host preserves original interface pixels and prefers a
     * valid texture to the CPU output. Zero means no texture. */
    uint32_t *output_mask_texture;
    /* v1.3: optional photo subject and caller-owned mask outputs. */
    const struct RhhWorldPhotoTarget *photo_target;
    /* v1.4: optional output for a 240x160 GL_TEXTURE_2D depth texture. The
     * presenter owns it until the next draw or shutdown. Zero means no depth.
     * It describes the scene before independent world passes, in OpenGL
     * framebuffer orientation, with normalized depth in [0,1]. */
    uint32_t *output_depth_texture;
};
#define RHH_WORLD_FRAME_V1_0_SIZE offsetof(struct RhhWorldFrame, output_mask)
#define RHH_WORLD_FRAME_V1_1_SIZE offsetof(struct RhhWorldFrame, output_mask_texture)
#define RHH_WORLD_FRAME_V1_2_SIZE offsetof(struct RhhWorldFrame, photo_target)
#define RHH_WORLD_FRAME_V1_3_SIZE offsetof(struct RhhWorldFrame, output_depth_texture)
#define RHH_WORLD_FRAME_V1_4_SIZE sizeof(struct RhhWorldFrame)
struct RhhWorldPresenter {
    uint32_t struct_size, reserved;
    const char *id;
    int (*draw)(const struct RhhWorldFrame *, const void *state);
    void (*shutdown)(void);
};
enum RhhModStage { RHH_TRANSFORM = 0, RHH_CONSTRAINT = 1, RHH_FINALIZE = 2, RHH_OBSERVE = 3 };
enum RhhContentFlags {
    RHH_CONTENT_ENABLED = 1u, RHH_CONTENT_CREATABLE = 2u,
    RHH_CONTENT_LEGENDARY = 4u, RHH_CONTENT_MYTHICAL = 8u
};
struct RhhContentInfo {
    uint32_t struct_size, kind;
    RhhContentKey key, canonical_key;
    uint32_t flags, reserved;
    uint16_t base_stats[6];
};
enum RhhModSettingType { RHH_SETTING_BOOL = 1, RHH_SETTING_CHOICE, RHH_SETTING_CONTENT };
enum RhhModSettingScope { RHH_SETTING_NEW_GAME = 0, RHH_SETTING_RUNTIME = 1 };
struct RhhModValue { uint32_t type, reserved; uint64_t value; };
struct RhhModSetting {
    uint32_t struct_size, type, scope, content_kind;
    const char *id, *label, *description;
    uint64_t minimum, maximum;
    uint32_t choice_count, reserved;
    const char *const *choices;
};

/* Borrowed for one call. event_key is a stable map/table/trainer identity;
 * subject is a stable content key (or an owned-mon identity for fainting).
 * ordinal is explicit, never inferred from callback invocation count. */
struct RhhModEvent {
    uint32_t struct_size, kind;
    uint64_t run_seed, event_key, subject, ordinal;
    uint32_t args[4];
};
struct RhhModDecision {
    uint32_t struct_size, allowed;
    uint64_t value;
    uint16_t stats[6]; /* HP, Attack, Defense, Speed, Sp. Attack, Sp. Defense */
    char reason[96];
};
typedef void (*RhhModHookFn)(const struct RhhModEvent *,
                            const void *state, struct RhhModDecision *);
typedef void (*RhhModObserveFn)(const struct RhhModEvent *,
                               const struct RhhModDecision *, void *state);
/* Services are host-owned, immutable tables borrowed until host shutdown.
 * Discover them by name and version; unavailable services return NULL. */
#define RHH_SERVICE_VERSION(major, minor) (((uint32_t)(major) << 16) | (uint32_t)(minor))
struct RhhServiceHeader { uint32_t struct_size, version; };
enum RhhPokemonIconFlags { RHH_POKEMON_ICON_EGG = 1u, RHH_POKEMON_ICON_FEMALE = 2u,
    RHH_POKEMON_ICON_PLACEHOLDER = 4u };
/* Species and personality are explicit; EGG is the only request flag.
 * The resolved species records engine form selection, not a durable identity. */
struct RhhPokemonIconRequest { uint32_t struct_size, flags, personality, reserved; RhhContentKey species; };
struct RhhPokemonIcon {
    uint32_t struct_size, flags, width, height, frame_count, reserved;
    RhhContentKey species, resolved_species;
};
/* "rhh:service/content_display" v1.3: copy a NUL-terminated ASCII display
 * name for an enabled species, item, move, ability or type key. The UI font
 * may substitute unsupported ASCII glyphs. Names are not identities and may
 * be shared by different forms. */
#define RHH_CONTENT_DESCRIPTION_BYTES 4096u /* Includes the terminating NUL. */
struct RhhContentDisplayService {
    struct RhhServiceHeader header;
    int (*name)(uint32_t kind, RhhContentKey key, char *out, uint32_t capacity);
    /* v1.1: 24x24 tightly packed RGBA8888; palette index zero is transparent. */
    int (*item_icon_rgba)(RhhContentKey item, uint8_t *out, uint32_t capacity);
    /* v1.2: engine-selected 32x32 Pokémon menu-icon source frames. */
    int (*pokemon_icon_info)(const struct RhhPokemonIconRequest *request, struct RhhPokemonIcon *out);
    int (*pokemon_icon_rgba)(const struct RhhPokemonIconRequest *request, uint32_t frame, uint8_t *out, uint32_t capacity);
    /* v1.3: copied ASCII description for an item, move or ability. Engine
     * line breaks become newlines; control codes are omitted and unsupported
     * glyphs or unresolved placeholders become '?'. Registered descriptions
     * use their authored ASCII. Invalid content or insufficient capacity
     * fails without changing the output. */
    int (*description)(uint32_t kind, RhhContentKey key, char *out, uint32_t capacity);
};
#define RHH_CONTENT_DISPLAY_SERVICE_V1_0_SIZE offsetof(struct RhhContentDisplayService, item_icon_rgba)
#define RHH_CONTENT_DISPLAY_SERVICE_V1_1_SIZE offsetof(struct RhhContentDisplayService, pokemon_icon_info)
#define RHH_CONTENT_DISPLAY_SERVICE_V1_2_SIZE offsetof(struct RhhContentDisplayService, description)
#define RHH_CONTENT_DISPLAY_SERVICE_V1_3_SIZE sizeof(struct RhhContentDisplayService)
struct RhhWorldDynamicLight;
struct RhhWorldLightContext;
struct RhhWorldLightFrame;
/* "rhh:service/battle_backdrop" v1.1. Catalog entries identify the engine's
 * baseline artwork, not a ground mask or a guarantee that an animation has
 * not since overwritten pixels. IDs are stable; catalog order is not. */
struct RhhBattleBackdropInfo {
    uint32_t struct_size, has_artwork;
    RhhContentKey key;
    char id[64];
};
struct RhhBattleBackdropService {
    struct RhhServiceHeader header;
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhBattleBackdropInfo *out);
    /* Available in the active battle scene after a complete baseline load. */
    int (*current)(struct RhhBattleBackdropInfo *out);
    /* v1.1: copy a rectangular region of the original 512x256 BG3 artwork
     * as tightly packed, top-left RGBA8888. Transparent color zero has alpha
     * zero. Returns -1 without writing on failure. This is source art, not the
     * current composited or scrolled frame; cache it outside render callbacks. */
    int (*image_rgba)(RhhContentKey key, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                      uint8_t *rgba, uint32_t capacity);
};
#define RHH_BATTLE_BACKDROP_WIDTH 512u
#define RHH_BATTLE_BACKDROP_HEIGHT 256u
#define RHH_BATTLE_BACKDROP_SERVICE_V1_0_SIZE offsetof(struct RhhBattleBackdropService, image_rgba)
#define RHH_BATTLE_BACKDROP_SERVICE_V1_1_SIZE sizeof(struct RhhBattleBackdropService)
struct RhhBattleRenderService {
    struct RhhServiceHeader header;
    /* Only during RHH_BATTLE_RENDER_PASSES constraint callbacks. */
    int (*offer)(const struct RhhBattleRenderPass *pass);
};
#define RHH_BATTLE_RENDER_SERVICE_V1_0_SIZE sizeof(struct RhhBattleRenderService)
/* "rhh:service/world_render" v1.0. offer is valid only during a
 * RHH_WORLD_RENDER_PASSES constraint callback and copies the descriptor.
 * The event ordinal is the presentation frame; args[0..1] are current saved
 * weather and terrain values (zero when absent). The collector is pure and
 * runs on OpenGL overworld frames with an active run, even when no field
 * effect is active. */
struct RhhWorldRenderService {
    struct RhhServiceHeader header;
    int (*offer)(const struct RhhWorldRenderPass *pass);
    /* v1.1: context and offer_light are valid only during the
     * RHH_WORLD_LIGHTS constraint event; frame_lights is valid only while the
     * selected presenter is drawing that frame. All values are copied. */
    int (*light_context)(struct RhhWorldLightContext *out);
    int (*offer_light)(const struct RhhWorldDynamicLight *light);
    int (*frame_lights)(struct RhhWorldLightFrame *out);
    /* v1.2: returns the registered shader's declared RhhShaderPhotoFlags, or
     * zero for a shader with no declaration. Unknown keys fail unchanged. */
    int (*shader_photo_flags)(RhhContentKey shader, uint32_t *out);
};
#define RHH_WORLD_RENDER_SERVICE_V1_0_SIZE offsetof(struct RhhWorldRenderService, light_context)
#define RHH_WORLD_RENDER_SERVICE_V1_1_SIZE offsetof(struct RhhWorldRenderService, shader_photo_flags)
#define RHH_WORLD_RENDER_SERVICE_V1_2_SIZE sizeof(struct RhhWorldRenderService)
/* Software UI canvas. Coordinates are screen pixels, origin top left;
 * colors are RGB555. Canvas functions are valid only during draw. */
enum RhhUiButton { RHH_UI_A = 1, RHH_UI_B = 2, RHH_UI_SELECT = 4,
    RHH_UI_START = 8, RHH_UI_RIGHT = 16, RHH_UI_LEFT = 32,
    RHH_UI_UP = 64, RHH_UI_DOWN = 128, RHH_UI_R = 256, RHH_UI_L = 512 };
struct RhhUiInput { uint32_t struct_size, held, pressed, repeated; };
struct RhhUiPoint { int32_t x, y; };
struct RhhUiRgbImage {
    uint32_t struct_size, width, height, stride, size;
    const uint8_t *pixels;
};
/* RGBA pixels are borrowed for one draw call. Alpha is tested against a
 * caller-chosen threshold in 1..255; this is a cutout, not alpha blending. */
struct RhhUiRgbaImage {
    uint32_t struct_size, width, height, stride, size, alpha_threshold;
    const uint8_t *pixels;
};
struct RhhUiCanvas {
    uint32_t struct_size, width, height;
    void (*rect)(int32_t x, int32_t y, int32_t width, int32_t height, uint16_t color);
    void (*line)(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t color);
    void (*polygon)(const struct RhhUiPoint *points, uint32_t count, uint16_t color);
    void (*text)(int32_t x, int32_t y, const char *ascii, uint16_t color, uint16_t shadow);
    /* v1.2: clipped nearest-neighbor RGB24 image, borrowed for this call. */
    int (*image_rgb)(int32_t x, int32_t y, int32_t width, int32_t height, const struct RhhUiRgbImage *image);
    /* v1.5: nearest-neighbor RGBA image with binary alpha cutout. */
    int (*image_rgba)(int32_t x, int32_t y, int32_t width, int32_t height, const struct RhhUiRgbaImage *image);
};
#define RHH_UI_CANVAS_V1_0_SIZE offsetof(struct RhhUiCanvas, image_rgb)
#define RHH_UI_CANVAS_V1_2_SIZE offsetof(struct RhhUiCanvas, image_rgba)
#define RHH_UI_CANVAS_V1_5_SIZE sizeof(struct RhhUiCanvas)
enum RhhUiCloseReason { RHH_UI_CLOSED = 1, RHH_UI_RESET = 2 };
struct RhhUiPanel {
    uint32_t struct_size, reserved;
    RhhContentKey owner;
    void (*input)(const struct RhhUiInput *, void *user);
    void (*draw)(const struct RhhUiCanvas *, void *user);
    void (*closed)(uint32_t reason, void *user);
    void *user;
};
/* Native file chooser. Paths are copied UTF-8; a cancelled/error result has
 * an empty path. The dialog belongs to a modal panel and does not grant access
 * to arbitrary files until a player chooses one. */
#define RHH_FILE_DIALOG_PATH_SIZE 4096u
enum RhhFileDialogKind { RHH_FILE_DIALOG_MIDI = 1, RHH_FILE_DIALOG_SOUNDFONT };
enum RhhFileDialogState { RHH_FILE_DIALOG_PENDING = 1, RHH_FILE_DIALOG_SELECTED,
    RHH_FILE_DIALOG_CANCELLED, RHH_FILE_DIALOG_ERROR };
enum RhhFileDialogStatus { RHH_FILE_DIALOG_OK, RHH_FILE_DIALOG_INVALID,
    RHH_FILE_DIALOG_UNAVAILABLE, RHH_FILE_DIALOG_BUSY };
struct RhhFileDialogResult {
    uint32_t struct_size, state, kind, reserved;
    RhhContentKey owner;
    char path[RHH_FILE_DIALOG_PATH_SIZE];
};
/* "rhh:service/file_dialog" v1.0. begin is asynchronous; poll does not
 * consume the result, take consumes a terminal result, and cancel requests
 * closure. Closing the owning panel cancels and discards its request. */
struct RhhFileDialogService {
    struct RhhServiceHeader header;
    int (*begin)(RhhContentKey owner, uint32_t kind);
    int (*poll)(RhhContentKey owner, struct RhhFileDialogResult *out);
    int (*take)(RhhContentKey owner, struct RhhFileDialogResult *out);
    int (*cancel)(RhhContentKey owner);
};
#define RHH_TEXT_ENTRY_CAPACITY 64u
enum RhhTextEntryState { RHH_TEXT_ENTRY_EDITING = 1, RHH_TEXT_ENTRY_ACCEPTED, RHH_TEXT_ENTRY_CANCELLED };
struct RhhTextEntrySnapshot {
    uint32_t struct_size, state, max_length, reserved;
    RhhContentKey owner;
    char text[RHH_TEXT_ENTRY_CAPACITY];
};
/* "rhh:service/text_entry" v1.0. Panel-owned native keyboard input for
 * printable ASCII; Enter accepts, Escape cancels, Backspace removes a byte.
 * The owner may inspect while editing and take a terminal result once. */
struct RhhTextEntryService {
    struct RhhServiceHeader header;
    int (*begin)(RhhContentKey owner, const char *initial, uint32_t max_length);
    int (*poll)(RhhContentKey owner, struct RhhTextEntrySnapshot *out);
    int (*take)(RhhContentKey owner, struct RhhTextEntrySnapshot *out);
};
/* An overlay draws over a live world frame and owns input while open. */
struct RhhUiOverlay {
    uint32_t struct_size, reserved;
    RhhContentKey owner;
    void (*input)(const struct RhhUiInput *, void *user);
    void (*draw)(const struct RhhUiCanvas *, void *user);
    void (*closed)(uint32_t reason, void *user);
    void *user;
};
struct RhhOwnedMonSnapshot;
struct RhhUiPartyAction;
#define RHH_UI_KEY_NAME_SIZE 64u
/* Copied physical-key labels for one logical button; count is 1 or 2. These
 * are SDL scancode names, not localized layout labels, portable identifiers
 * or controller bindings. */
struct RhhUiKeyboardBinding {
    uint32_t struct_size, button, count, reserved;
    char keys[2][RHH_UI_KEY_NAME_SIZE];
};
/* rhh:service/ui v1.5. open is allowed in a stable RHH_MOD_UPDATE; input
 * samples rebound keys in that event. v1.1 collects party actions in
 * RHH_PARTY_MENU_ACTIONS constraint callbacks, with args[0] as party slot.
 * The copied panel and its user pointer remain valid until closed; drawing,
 * action collection, action opening and closing are read-only. */
struct RhhUiService {
    struct RhhServiceHeader header;
    int (*input)(struct RhhUiInput *out);
    int (*open)(const struct RhhUiPanel *panel);
    int (*close)(RhhContentKey owner);
    int (*active)(void);
    /* v1.1: available only inside RHH_PARTY_MENU_ACTIONS constraints. */
    int (*party_context)(struct RhhOwnedMonSnapshot *out);
    int (*offer_party_action)(const struct RhhUiPartyAction *action);
    /* v1.3: one nonmodal world overlay; opening requires RHH_MOD_UPDATE. */
    int (*overlay_open)(const struct RhhUiOverlay *overlay);
    int (*overlay_close)(RhhContentKey owner);
    int (*overlay_active)(void);
    /* v1.4: copy the current keyboard binding for exactly one RhhUiButton.
     * Query again after controls change; failure leaves out unchanged. */
    int (*keyboard_binding)(uint32_t button, struct RhhUiKeyboardBinding *out);
};
#define RHH_UI_SERVICE_V1_0_SIZE offsetof(struct RhhUiService, party_context)
#define RHH_UI_SERVICE_V1_1_SIZE offsetof(struct RhhUiService, overlay_open)
#define RHH_UI_SERVICE_V1_2_SIZE RHH_UI_SERVICE_V1_1_SIZE
#define RHH_UI_SERVICE_V1_3_SIZE offsetof(struct RhhUiService, keyboard_binding)
#define RHH_UI_SERVICE_V1_4_SIZE sizeof(struct RhhUiService)
#define RHH_UI_SERVICE_V1_5_SIZE RHH_UI_SERVICE_V1_4_SIZE
/* Copied semantic text for a modal panel or world overlay. Each field is NUL-terminated UTF-8;
 * tabs and newlines are permitted, other control characters are rejected.
 * Focus is optional and describes the current selection or control. */
#define RHH_UI_ACCESSIBLE_TITLE_SIZE 128u
#define RHH_UI_ACCESSIBLE_BODY_SIZE 2048u
#define RHH_UI_ACCESSIBLE_FOCUS_SIZE 256u
struct RhhUiAccessibleContent {
    uint32_t struct_size, reserved;
    char title[RHH_UI_ACCESSIBLE_TITLE_SIZE];
    char body[RHH_UI_ACCESSIBLE_BODY_SIZE];
    char focus[RHH_UI_ACCESSIBLE_FOCUS_SIZE];
};
struct RhhUiAccessibleSnapshot {
    uint32_t struct_size, active;
    RhhContentKey owner;
    uint64_t revision;
    struct RhhUiAccessibleContent content;
};
/* rhh:service/ui_accessibility v1.0. A modal panel publishes from its draw
 * callback using its owner key. The host copies and validates the content;
 * identical publications do not change the revision. snapshot is a pure copy
 * and reports active=0 when no published modal content is available.
 * RHH_UI_ACCESSIBILITY_CHANGED is observer-only and delivered after drawing
 * or normal panel closure, including while the game is paused by a modal.
 * Changes before delivery can coalesce. Its ordinal is the revision and
 * args[0] is active. Run resets and unloads
 * clear the snapshot without promising a closure event. This is a bridge for
 * an accessibility adapter; the host does not itself provide speech.
 * v1.1 also accepts publications from the active world overlay's draw callback.
 * Its independently sequenced overlay_snapshot and observer-only
 * RHH_UI_OVERLAY_ACCESSIBILITY_CHANGED event use the same lifecycle and
 * copied-content rules, without changing the v1.0 modal stream. */
struct RhhUiAccessibilityService {
    struct RhhServiceHeader header;
    int (*publish)(RhhContentKey owner, const struct RhhUiAccessibleContent *content);
    int (*snapshot)(struct RhhUiAccessibleSnapshot *out);
    /* v1.1: separately copied overlay content. */
    int (*overlay_snapshot)(struct RhhUiAccessibleSnapshot *out);
};
#define RHH_UI_ACCESSIBILITY_SERVICE_V1_0_SIZE offsetof(struct RhhUiAccessibilityService, overlay_snapshot)
#define RHH_UI_ACCESSIBILITY_SERVICE_V1_1_SIZE sizeof(struct RhhUiAccessibilityService)
enum RhhMonLocation { RHH_MON_PARTY = 1, RHH_MON_BOX = 2, RHH_MON_DAYCARE = 3, RHH_MON_STAGED = 4,
    RHH_MON_FUSION_STORAGE = 5, RHH_MON_MOVING = 6 };
enum RhhMonStat { RHH_STAT_HP, RHH_STAT_ATTACK, RHH_STAT_DEFENSE,
    RHH_STAT_SPEED, RHH_STAT_SP_ATTACK, RHH_STAT_SP_DEFENSE, RHH_STAT_COUNT };
enum RhhBattleStat { RHH_BATTLE_ACCURACY = RHH_STAT_COUNT, RHH_BATTLE_EVASION, RHH_BATTLE_STAT_COUNT };
enum RhhBattleFlags {
    RHH_BATTLE_TRAINER = 1u, RHH_BATTLE_DOUBLE = 2u, RHH_BATTLE_LINK = 4u,
    RHH_BATTLE_RECORDED = 8u, RHH_BATTLE_FACILITY = 16u, RHH_BATTLE_HAS_PARTNER = 32u,
    RHH_BATTLE_TWO_OPPONENTS = 64u, RHH_BATTLE_TUTORIAL = 128u,
    RHH_BATTLE_MULTI = 256u, RHH_BATTLE_SAFARI = 512u, RHH_BATTLE_ROAMER = 1024u,
    RHH_BATTLE_GHOST = 2048u
};
enum RhhBattleOwner { RHH_BATTLE_OWNER_PLAYER, RHH_BATTLE_OWNER_OPPONENT_A,
    RHH_BATTLE_OWNER_PARTNER, RHH_BATTLE_OWNER_OPPONENT_B };
enum RhhTrainerCatalogFilter { RHH_TRAINER_CATALOG_ALL = 0, RHH_TRAINER_CATALOG_BOSSES = 1 };
enum RhhTrainerCatalogFlags { RHH_TRAINER_CATALOG_BOSS = 1u };
/* Trainer keys identify authored encounters, including separate rematches;
 * labels and ordinal positions are presentation data, not durable identity. */
struct RhhTrainerCatalogEntry {
    uint32_t struct_size, flags;
    RhhContentKey trainer;
    char name[32], trainer_class[32], encounter_id[64];
};
/* "rhh:service/trainer_catalog" v1.0. Lists valid authored trainers for the
 * current run and difficulty in engine order. The boss filter is an
 * approximate class-based grouping, not a promise of story importance. */
struct RhhTrainerCatalogService {
    struct RhhServiceHeader header;
    uint32_t (*count)(uint32_t filter);
    int (*at)(uint32_t filter, uint32_t index, struct RhhTrainerCatalogEntry *out);
    int (*find)(RhhContentKey trainer, struct RhhTrainerCatalogEntry *out);
};
enum RhhBattlePosition { RHH_BATTLE_PLAYER_LEFT, RHH_BATTLE_OPPONENT_LEFT,
    RHH_BATTLE_PLAYER_RIGHT, RHH_BATTLE_OPPONENT_RIGHT };
enum RhhBattleOutcome { RHH_BATTLE_OUTCOME_UNKNOWN, RHH_BATTLE_WON, RHH_BATTLE_LOST,
    RHH_BATTLE_DREW, RHH_BATTLE_RAN, RHH_BATTLE_PLAYER_TELEPORTED, RHH_BATTLE_MON_FLED,
    RHH_BATTLE_CAUGHT, RHH_BATTLE_OUT_OF_BALLS, RHH_BATTLE_FORFEITED, RHH_BATTLE_MON_TELEPORTED };
enum RhhBattlePhase { RHH_BATTLE_PHASE_ACTIVE = 1, RHH_BATTLE_PHASE_ENDED };
enum RhhBattleEntryCause { RHH_BATTLE_ENTRY_INITIAL = 1, RHH_BATTLE_ENTRY_SWITCH };
enum RhhBattleEndReason { RHH_BATTLE_END_NORMAL = 1, RHH_BATTLE_END_ABORTED };
#define RHH_BATTLE_NO_BATTLER UINT32_MAX
#define RHH_BATTLE_MAX_PARTICIPANTS 4u
struct RhhBattleParticipant {
    uint32_t struct_size, battler, position, owner, party_slot, level, hp, max_hp, fainted;
    RhhContentKey species, party_species, trainer;
    uint64_t identity; /* OT ID << 32 | personality; can collide. */
};
enum RhhBattlerHealthChange { RHH_BATTLER_HP_CHANGED = 1u, RHH_BATTLER_MAX_HP_CHANGED = 2u };
/* Coalesced native-frame samples for one continuing battle slot. The copies
 * are authoritative trusted-mod views, not a player-information boundary.
 * Changes reversed between samples and their causes are not reported. */
struct RhhBattlerHealthObservation {
    uint32_t struct_size, changed;
    uint64_t run_id, battle_id, sequence;
    struct RhhBattleParticipant before, after;
};
struct RhhBattleSnapshot {
    uint32_t struct_size, phase, flags, outcome, turn, participant_count;
    uint64_t run_id, battle_id; /* battle_id is process-scoped, not durable. */
    RhhContentKey map, opponent_trainers[2];
    struct RhhBattleParticipant participants[RHH_BATTLE_MAX_PARTICIPANTS];
};
struct RhhBattleEventContext {
    uint32_t struct_size, event, battler, cause;
    uint64_t sequence;
    struct RhhBattleSnapshot battle;
};
enum RhhBattleStatusKind { RHH_BATTLE_STATUS_SLEEP = 1, RHH_BATTLE_STATUS_POISON,
    RHH_BATTLE_STATUS_BURN, RHH_BATTLE_STATUS_FREEZE, RHH_BATTLE_STATUS_PARALYSIS,
    RHH_BATTLE_STATUS_TOXIC, RHH_BATTLE_STATUS_FROSTBITE };
enum RhhBattleStatusTrigger { RHH_BATTLE_STATUS_FROM_MOVE = 1, RHH_BATTLE_STATUS_FROM_ABILITY,
    RHH_BATTLE_STATUS_FROM_PROTECT };
enum RhhBattleStatusParticipantFlags { RHH_BATTLE_STATUS_SOURCE_AVAILABLE = 1u,
    RHH_BATTLE_STATUS_TARGET_AVAILABLE = 2u };
/* Committed status application at the engine's common status setter. The
 * before/after values use the engine status representation, including sleep
 * counters. Missing participant copies are flagged; slots are not durable
 * identities. move is supplied only for move-triggered applications. */
struct RhhBattleStatusApplication {
    uint32_t struct_size, status, trigger, flags;
    uint32_t source_battler, target_battler, before_raw, after_raw;
    uint64_t run_id, battle_id, sequence;
    RhhContentKey move;
    struct RhhBattleParticipant source, target;
};
enum RhhBattleStatusAttributionKind { RHH_STATUS_ATTRIBUTION_UNKNOWN = 0,
    RHH_STATUS_ATTRIBUTION_INNATE_ENTRY, RHH_STATUS_ATTRIBUTION_INNATE_TURN };
/* Attribution is known only for status actions queued by the innate API.
 * sequence and index identify the originating entry or completed-turn event;
 * they are scoped to this battle, not durable identifiers. */
struct RhhBattleStatusAttribution {
    uint32_t struct_size, kind, index, reserved;
    uint64_t sequence;
    RhhContentKey ability;
};
enum RhhBattleStatusFlags { RHH_BATTLE_CONDITION_SLEEP = 1u, RHH_BATTLE_CONDITION_POISON = 2u,
    RHH_BATTLE_CONDITION_BURN = 4u, RHH_BATTLE_CONDITION_FREEZE = 8u,
    RHH_BATTLE_CONDITION_PARALYSIS = 16u, RHH_BATTLE_CONDITION_TOXIC = 32u,
    RHH_BATTLE_CONDITION_FROSTBITE = 64u };
#define RHH_BATTLE_CONDITIONS_ALL 127u
/* A major-status cure committed by a supported engine path or reaction queue.
 * For reaction cures, the originating event and sequence identify the status
 * event that queued the action; engine cures use zero for both.
 * RHH_BATTLER_STATUS_CURED uses event_key = the curing ability or move when known,
 * subject = participant.identity, ordinal = sequence, and args = battler,
 * removed, requested, origin_event. */
struct RhhBattleStatusCure {
    uint32_t struct_size, battler, requested, removed;
    uint32_t before_raw, after_raw, participant_available, origin_event;
    uint64_t run_id, battle_id, sequence, origin_sequence;
    struct RhhBattleParticipant participant;
};
enum RhhBattleCureCause { RHH_BATTLE_CURE_REACTION = 1, RHH_BATTLE_CURE_IMMUNITY_ABILITY,
    RHH_BATTLE_CURE_END_TURN_ABILITY, RHH_BATTLE_CURE_NATURAL, RHH_BATTLE_CURE_MOVE,
    RHH_BATTLE_CURE_SCRIPT_ABILITY, RHH_BATTLE_CURE_SWITCH_ABILITY, RHH_BATTLE_CURE_UPROAR };
/* Reaction and natural cures have no inferred source or ability. An ability
 * cure names its owner; a move cure names its attacker. Unknown keys are zero. */
struct RhhBattleStatusCureAttribution {
    uint32_t struct_size, cause, source_battler, reserved;
    RhhContentKey ability;
};
/* A player-owned, off-field party member cured by a supported party-wide
 * move. identity combines OT ID and personality and is not a permanent unique
 * ID. The event is reported after the controller applies the cure and before
 * queued reactions; it does not disclose unrevealed opponent party members.
 * event_key is move, subject is identity, ordinal is sequence, and args are
 * party_slot, removed, owner, source_battler. */
struct RhhBattlePartyStatusCure {
    uint32_t struct_size, owner, party_slot, source_battler;
    uint32_t before_raw, after_raw, removed, reserved;
    uint64_t run_id, battle_id, sequence, identity;
    RhhContentKey species, move;
};
/* LAST_USED values describe where the engine obtained the item, not who
 * supplied or consumed it. A Fling may affect a different battler. */
enum RhhBattleItemStatusOrigin { RHH_BATTLE_ITEM_HELD = 1,
    RHH_BATTLE_ITEM_LAST_USED_BERRY, RHH_BATTLE_ITEM_LAST_USED_FLING, RHH_BATTLE_ITEM_BAG };
/* An item cured a player-owned off-field party member. The embedded cure's
 * move is zero. This is emitted before the item-cure message; battle service
 * v1.41 supports reactions after that message. event_key is item; subject, ordinal and args follow the
 * existing party-cure event. */
struct RhhBattlePartyItemStatusCure {
    uint32_t struct_size, reserved;
    RhhContentKey item;
    struct RhhBattlePartyStatusCure cure;
};
struct RhhBattleItemStatusChange {
    uint32_t struct_size, battler, origin, participant_available;
    uint32_t before_raw, after_raw, before_conditions, after_conditions;
    uint32_t added, removed, reserved[2];
    uint64_t run_id, battle_id, sequence;
    RhhContentKey item;
    struct RhhBattleParticipant participant;
};
/* Completed end-of-turn effects, before the next turn's AI preparation.
 * sequence counts only these notifications and resets for each battle.
 * No notification is sent for a battle-ending partial turn, link battles,
 * or recorded battles. Use BATTLE_ENDED to clean up at battle end. */
struct RhhBattleTurnContext {
    uint32_t struct_size, reserved;
    uint64_t sequence;
    struct RhhBattleSnapshot battle;
};
#define RHH_BATTLE_CONDITION_LIMIT 128u
#define RHH_BATTLE_CONDITIONS_PER_MOD 16u
enum RhhBattleConditionScope { RHH_CONDITION_FIELD = 1, RHH_CONDITION_SIDE = 2 };
enum RhhBattleConditionTarget { RHH_CONDITION_WHOLE_FIELD = 0, RHH_CONDITION_PLAYER_SIDE = 1,
    RHH_CONDITION_OPPONENT_SIDE = 2 };
/* Stable id: namespace:condition/name. A condition has one instance per
 * field or per side and a bounded countdown in completed turns. */
struct RhhModBattleCondition {
    uint32_t struct_size, scope, max_turns, reserved;
    const char *id, *label;
};
struct RhhBattleConditionInfo {
    uint32_t struct_size, scope, max_turns, reserved;
    RhhContentKey key;
    char label[32];
};
struct RhhBattleConditionState {
    uint32_t struct_size, target, remaining_turns, reserved;
    RhhContentKey key;
    uint64_t battle_id;
};
/* Copied active instance, enumerated in condition-key and target order.
 * Indices are transient and can change when a condition changes or expires. */
struct RhhActiveBattleCondition {
    uint32_t struct_size, scope, target, remaining_turns;
    RhhContentKey key;
    uint64_t battle_id;
    char label[32];
};
/* Stage values use -6..+6, with 0 neutral. HP has no stage and is always 0. */
struct RhhBattleStatStages {
    uint32_t struct_size, battler;
    uint64_t battle_id;
    int8_t stages[RHH_BATTLE_STAT_COUNT];
};
enum RhhBattleNatureFlags {
    RHH_BATTLE_NATURE_SNAPSHOT = 1u, RHH_BATTLE_NATURE_PARTY = 2u,
    RHH_BATTLE_NATURE_STAT_AVAILABLE = 4u, RHH_BATTLE_NATURE_PERSONALITY_FALLBACK = 8u
};
/* Nature used by the battler's Palace and flavor rules. When the stat nature
 * is unavailable, its ordinal and key are zero and the flag is absent.
 * Personality fallback is not proof of the source Pokémon's stored nature. */
struct RhhBattleNature {
    uint32_t struct_size, battler, flags, nature, stat_nature, reserved;
    uint64_t battle_id;
    RhhContentKey nature_key, stat_nature_key;
};
enum RhhBattleConditionChangeReason {
    RHH_CONDITION_APPLIED = 1, RHH_CONDITION_DURATION_CHANGED,
    RHH_CONDITION_REMOVED, RHH_CONDITION_EXPIRED
};
/* Committed change to one condition instance. Expirations are delivered after
 * every timer has ticked, in key/target order, before TURN_ENDED. Same-value
 * sets are silent; battle teardown clears state without change notifications. */
struct RhhBattleConditionChange {
    uint32_t struct_size, reason, scope, target, previous_turns, remaining_turns, turn, reserved;
    uint64_t run_id, battle_id, sequence;
    RhhContentKey key;
};
/* A requested condition set from a committed move or innate callback.
 * actor and recipient are battler slots, or RHH_BATTLE_NO_BATTLER. source is
 * the move or innate key. Expiry ticks and battle teardown are not proposals. */
struct RhhBattleConditionProposal {
    uint32_t struct_size, origin_event, scope, target, previous_turns, requested_turns, max_turns, turn;
    uint32_t actor, recipient, reserved[2];
    uint64_t run_id, battle_id;
    RhhContentKey key, source;
};
enum RhhBattleConditionSetStatus { RHH_CONDITION_SET_OK = 0, RHH_CONDITION_SET_INVALID,
    RHH_CONDITION_SET_UNAVAILABLE, RHH_CONDITION_SET_DENIED, RHH_CONDITION_SET_STALE };
struct RhhBattleConditionSetResult {
    uint32_t struct_size, status, target, requested_turns, previous_turns, remaining_turns, changed, reserved;
    RhhContentKey key;
    uint64_t run_id, battle_id;
    char reason[96];
};
/* RHH_BATTLE_CONDITION_PROPOSAL transforms decision.value from requested_turns
 * to a value in 0..max_turns. Denial or an invalid value rejects the set;
 * constraints cannot rewrite the proposed duration. No change notification
 * is sent on rejection or when the resulting value is unchanged. */
enum RhhTrainerSourceFlags { RHH_TRAINER_SOURCE_LEAD = 1u, RHH_TRAINER_SOURCE_ACE = 2u };
enum RhhTrainerSourceDifficulty { RHH_TRAINER_DIFFICULTY_UNKNOWN, RHH_TRAINER_DIFFICULTY_EASY,
    RHH_TRAINER_DIFFICULTY_NORMAL, RHH_TRAINER_DIFFICULTY_HARD };
/* Copied provenance of a generated opponent in the current battle. Source
 * index is an ordinal in the selected difficulty's authored pool, not a
 * durable key. Flags are explicit source tags, not inferred from level/order. */
struct RhhTrainerPartySource {
    uint32_t struct_size, owner, party_slot, source_index, source_count, flags;
    uint32_t source_level, generated_level, source_difficulty;
    RhhContentKey trainer, roster_trainer, source_species, generated_species;
    uint64_t identity;
};
enum RhhTrainerRosterFlags { RHH_TRAINER_ROSTER_POOL = 1u, RHH_TRAINER_ROSTER_OVERRIDE = 2u };
/* A roster is a copied view at the current effective difficulty. Its token is
 * a fingerprint for detecting changed definitions, not a durable entry ID. */
struct RhhTrainerRoster {
    uint32_t struct_size, flags, party_size, source_count, trainer_difficulty, roster_difficulty;
    RhhContentKey trainer, roster_trainer;
    uint64_t token;
};
struct RhhTrainerRosterEntry {
    uint32_t struct_size, index, flags, level;
    RhhContentKey species;
};
/* "rhh:service/trainer_roster" v1.0. get and entry are pure queries; match
 * compares an unchanged roster and entry with a generated opponent's recorded
 * source. It returns zero in *out for a different trainer or source. Missing
 * provenance, stale rosters and invalid inputs fail without changing output. */
struct RhhTrainerRosterService {
    struct RhhServiceHeader header;
    int (*get)(RhhContentKey trainer, struct RhhTrainerRoster *out);
    int (*entry)(const struct RhhTrainerRoster *, uint32_t index, struct RhhTrainerRosterEntry *out);
    int (*match)(const struct RhhTrainerRoster *, uint32_t index, uint32_t owner, uint32_t party_slot, uint32_t *out);
};
#define RHH_BOSS_ACE_CAPACITY 64u
enum RhhBossAceFlags { RHH_BOSS_ACE_STALE = 1u, RHH_BOSS_ACE_INACTIVE = 2u,
    RHH_BOSS_ACE_UNAVAILABLE = 4u, RHH_BOSS_ACE_ORPHANED = 8u };
enum RhhBossAceAction { RHH_BOSS_ACE_SET = 1, RHH_BOSS_ACE_CLEAR };
enum RhhBossAceStatus { RHH_BOSS_ACE_OK = 0, RHH_BOSS_ACE_INVALID, RHH_BOSS_ACE_UNAVAILABLE_COMMAND,
    RHH_BOSS_ACE_STALE_REQUEST, RHH_BOSS_ACE_CONFLICT, RHH_BOSS_ACE_FULL, RHH_BOSS_ACE_DENIED };
struct RhhBossAceSelection {
    uint32_t struct_size, valid, flags, source_index, trainer_difficulty, roster_difficulty, source_level, source_flags;
    RhhContentKey trainer, roster_trainer, species, owner;
    uint64_t token, revision;
};
struct RhhBossAceRequest {
    uint32_t struct_size, action, trainer_difficulty, source_index;
    RhhContentKey trainer, owner;
    uint64_t expected_revision;
    struct RhhTrainerRoster roster; /* Required for SET; current checked roster. */
};
struct RhhBossAceResult {
    uint32_t struct_size, status, is_preview, changed;
    struct RhhBossAceSelection before, after;
    char reason[96];
};
struct RhhBossAceMatch {
    uint32_t struct_size, matched;
    struct RhhBossAceSelection selection;
    struct RhhTrainerPartySource source;
};
/* "rhh:service/boss_ace" v1.0. Configurations are per trainer and effective
 * trainer difficulty, and follow ordinary save rollback. get and at copy even
 * stale or orphaned selections. preview/apply use the global revision; context
 * is scoped to permission and committed notification hooks. match never treats
 * stale or unavailable selections as a match. */
struct RhhBossAceService {
    struct RhhServiceHeader header;
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhBossAceSelection *out);
    int (*get)(RhhContentKey trainer, uint32_t trainer_difficulty, struct RhhBossAceSelection *out);
    int (*preview)(const struct RhhBossAceRequest *, struct RhhBossAceResult *out);
    int (*apply)(const struct RhhBossAceRequest *, struct RhhBossAceResult *out);
    int (*context)(struct RhhBossAceResult *out);
    int (*match)(uint32_t owner, uint32_t party_slot, struct RhhBossAceMatch *out);
};
/* A pure post-engine speed proposal. Ability and hold effect are the caller's
 * assumptions; hold_effect is a transient engine ordinal. Species and HP are
 * engine snapshots and the AI flag does not guarantee a hidden-information boundary.
 * event_key is species, subject is ability, args are battler, engine speed,
 * flags and hold effect. Only speeds in 1..RHH_BATTLE_SPEED_MAX are accepted. */
#define RHH_BATTLE_SPEED_AI 1u
#define RHH_BATTLE_SPEED_MAX 171798691u
struct RhhBattleSpeedContext {
    uint32_t struct_size, battler, flags, engine_speed;
    uint32_t weather, terrain, hp, max_hp, level, hold_effect;
    uint64_t run_id, battle_id;
    RhhContentKey species, ability;
};
enum RhhBattleEnvironmentChangeFlags {
    RHH_BATTLE_ENV_WEATHER_CHANGED = 1u, RHH_BATTLE_ENV_TERRAIN_CHANGED = 2u,
    RHH_BATTLE_ENV_SUPPRESSION_CHANGED = 4u, RHH_BATTLE_ENV_WEATHER_TIMER_CHANGED = 8u,
    RHH_BATTLE_ENV_TERRAIN_TIMER_CHANGED = 16u, RHH_BATTLE_ENV_RAW_WEATHER_CHANGED = 32u
};
/* Copied live battle field. weather is the underlying field weather;
 * effective_weather is NONE when suppressed. weather_flags is a transient
 * engine bitmask and the two turn counts are raw engine counters: zero may
 * mean absent, indefinite or a transition. They are not a duration promise. */
struct RhhBattleEnvironmentState {
    uint32_t struct_size, weather, effective_weather, terrain, weather_suppressed;
    uint32_t weather_flags, weather_turns, terrain_turns, turn;
    uint64_t run_id, battle_id;
};
enum RhhBattleFieldRuleFlags {
    RHH_BATTLE_FIELD_MAGIC_ROOM = 1u, RHH_BATTLE_FIELD_TRICK_ROOM = 2u,
    RHH_BATTLE_FIELD_WONDER_ROOM = 4u, RHH_BATTLE_FIELD_MUD_SPORT = 8u,
    RHH_BATTLE_FIELD_WATER_SPORT = 16u, RHH_BATTLE_FIELD_GRAVITY = 32u,
    RHH_BATTLE_FIELD_ION_DELUGE = 64u, RHH_BATTLE_FIELD_FAIRY_LOCK = 128u
};
enum RhhBattleFieldContextFlags { RHH_BATTLE_FIELD_AI_CALCULATION = 1u,
    RHH_BATTLE_FIELD_AI_PREDICTION = 2u, RHH_BATTLE_FIELD_AI_SWITCH = 4u };
/* Active flags and raw engine counters are copied independently; zero does
 * not necessarily mean a rule is inactive or guarantee expiry timing. */
struct RhhBattleFieldRules {
    uint32_t struct_size, active, context_flags, turn;
    uint32_t magic_room_turns, trick_room_turns, wonder_room_turns, mud_sport_turns;
    uint32_t water_sport_turns, gravity_turns, fairy_lock_turns, reserved;
    uint64_t run_id, battle_id;
};
/* A coalesced observation: masks use RhhBattleFieldRuleFlags. timer_changed
 * covers raw counters; Ion Deluge has no counter. Turn alone is not a change. */
struct RhhBattleFieldRulesObservation {
    uint32_t struct_size, active_changed, timer_changed, reserved;
    uint64_t sequence;
    struct RhhBattleFieldRules before, after;
};
enum RhhBattleFieldRulesCause { RHH_BATTLE_FIELD_CAUSE_MOVE = 1,
    RHH_BATTLE_FIELD_CAUSE_EXPIRED, RHH_BATTLE_FIELD_CAUSE_CLEARED, RHH_BATTLE_FIELD_CAUSE_STARTING };
struct RhhBattleFieldRulesCommit {
    uint32_t struct_size, active_changed, timer_changed, cause, source_battler, reserved;
    uint64_t sequence;
    RhhContentKey move;
    struct RhhBattleFieldRules before, after;
};
enum RhhBattleSportMode { RHH_BATTLE_SPORT_FIELD_TIMER = 1, RHH_BATTLE_SPORT_BATTLER_VOLATILE };
enum RhhBattleSportsFlags { RHH_BATTLE_SPORTS_SKY_BATTLE = 1u };
/* Masks of battlers use slot bits; field_active/effective use
 * RHH_BATTLE_FIELD_MUD_SPORT and RHH_BATTLE_FIELD_WATER_SPORT. Raw battler
 * flags include every engine slot, including absent or fainted slots. */
struct RhhBattleSports {
    uint32_t struct_size, mode, battler_count, flags, mud_battlers, water_battlers;
    uint32_t field_active, effective, mud_turns, water_turns, context_flags, turn;
    uint64_t run_id, battle_id;
};
enum RhhBattleSportsChanged {
    RHH_BATTLE_SPORTS_CHANGED_FIELD = 1u, RHH_BATTLE_SPORTS_CHANGED_MUD_BATTLERS = 2u,
    RHH_BATTLE_SPORTS_CHANGED_WATER_BATTLERS = 4u, RHH_BATTLE_SPORTS_CHANGED_EFFECTIVE = 8u,
    RHH_BATTLE_SPORTS_CHANGED_TIMERS = 16u, RHH_BATTLE_SPORTS_CHANGED_BATTLER_COUNT = 32u,
    RHH_BATTLE_SPORTS_CHANGED_FLAGS = 64u
};
struct RhhBattleSportsObservation {
    uint32_t struct_size, changed;
    uint64_t sequence;
    struct RhhBattleSports before, after;
};
/* "rhh:service/battle_field_rules" v1.7. The current query copies engine
 * state for ordinary active local battles; it is not a forecast.
 * Link, recorded, ended and unavailable battles fail without changing out. */
struct RhhBattleFieldRulesService {
    struct RhhServiceHeader header;
    int (*current)(struct RhhBattleFieldRules *out);
    /* v1.1: only during RHH_BATTLE_FIELD_RULES_OBSERVED. Frame samples can
     * coalesce or miss changes; initial sample seeds silently. The event uses
     * subject=battle_id, ordinal=sequence and args=(active_changed,
     * timer_changed, new active flags, 0). AI simulation samples are skipped. */
    int (*observation)(struct RhhBattleFieldRulesObservation *out);
    /* v1.2: available only during RHH_BATTLE_FIELD_RULES_COMMITTED. The
     * event reports supported mutation boundaries, not every possible write. */
    int (*commit)(struct RhhBattleFieldRulesCommit *out);
    /* v1.3: during a committed move-effect observer, queue the
     * engine's Gravity or Ion Deluge additional effect. One flag per call;
     * action capacity and participant checks match move-effect actions. */
    int (*queue_move_effect)(uint32_t rule);
    /* v1.4: offer Gravity or Ion Deluge during RHH_AI_EFFECT_PREVIEW. The
     * shared AI effect list copies rule into first; chance is 1..100. */
    int (*offer_ai_move_effect)(uint32_t rule, uint32_t chance);
    /* v1.5: toggle one room during a committed move-effect observer, or
     * offer that toggle during RHH_AI_EFFECT_PREVIEW. Shared limits apply. */
    int (*queue_room_toggle)(uint32_t rule);
    int (*offer_ai_room_toggle)(uint32_t rule, uint32_t chance);
    /* v1.6: queue or offer Mud Sport, Water Sport or Fairy Lock. */
    int (*queue_field_move)(uint32_t rule);
    int (*offer_ai_field_move)(uint32_t rule, uint32_t chance);
    /* v1.7: current Sport sources and a coalesced observation, respectively.
     * Observation is available only in RHH_BATTLE_SPORTS_OBSERVED. */
    int (*sports)(struct RhhBattleSports *out);
    int (*sports_observation)(struct RhhBattleSportsObservation *out);
};
#define RHH_BATTLE_FIELD_RULES_SERVICE_V1_0_SIZE offsetof(struct RhhBattleFieldRulesService, observation)
#define RHH_BATTLE_FIELD_RULES_SERVICE_V1_1_SIZE offsetof(struct RhhBattleFieldRulesService, commit)
#define RHH_BATTLE_FIELD_RULES_SERVICE_V1_2_SIZE offsetof(struct RhhBattleFieldRulesService, queue_move_effect)
#define RHH_BATTLE_FIELD_RULES_SERVICE_V1_3_SIZE offsetof(struct RhhBattleFieldRulesService, offer_ai_move_effect)
#define RHH_BATTLE_FIELD_RULES_SERVICE_V1_4_SIZE offsetof(struct RhhBattleFieldRulesService, queue_room_toggle)
#define RHH_BATTLE_FIELD_RULES_SERVICE_V1_5_SIZE offsetof(struct RhhBattleFieldRulesService, queue_field_move)
#define RHH_BATTLE_FIELD_RULES_SERVICE_V1_6_SIZE offsetof(struct RhhBattleFieldRulesService, sports)
#define RHH_BATTLE_FIELD_RULES_SERVICE_V1_7_SIZE sizeof(struct RhhBattleFieldRulesService)
enum RhhBattleGroundingFlags {
    RHH_BATTLE_GROUNDING_GROUNDED = 1u, RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED = 2u,
    RHH_BATTLE_GROUNDING_SEMI_INVULNERABLE = 4u, RHH_BATTLE_GROUNDING_AI = 8u,
    RHH_BATTLE_GROUNDING_ABILITY_NONE_OR_UNKNOWN = 16u, RHH_BATTLE_GROUNDING_ITEM_NONE_OR_UNKNOWN = 32u,
    RHH_BATTLE_GROUNDING_SCENARIO = 64u, RHH_BATTLE_GROUNDING_AI_ENTRY = 128u
};
struct RhhBattleGrounding {
    uint32_t struct_size, battler, flags, terrain, engine_terrain, turn;
    uint64_t run_id, battle_id;
};
/* "rhh:service/battle_grounding" v1.2. All views are copied and local to an
 * active, non-link, non-recorded battle. Live queries reject AI simulations;
 * ai and preview_ai are limited to RHH_AI_MOVE_SCORE and
 * RHH_AI_EFFECT_PREVIEW. Entry queries have the separate scope below. */
struct RhhBattleGroundingService {
    struct RhhServiceHeader header;
    int (*live)(uint32_t battler, struct RhhBattleGrounding *out);
    int (*ai)(uint32_t battler, struct RhhBattleGrounding *out);
    /* v1.1: evaluate a supplied RhhBattleTerrain against current grounding
     * inputs. These calls do not set terrain or simulate its side effects. */
    int (*preview_live)(uint32_t battler, uint32_t terrain, struct RhhBattleGrounding *out);
    int (*preview_ai)(uint32_t battler, uint32_t terrain, struct RhhBattleGrounding *out);
    /* v1.2: the current candidate in RHH_AI_INNATE_ENTRY_FIELD_PREVIEW.
     * entry_ai uses accumulated field offers; preview_entry_ai tests a
     * supplied terrain. Neither predicts subsequent reactions. */
    int (*entry_ai)(struct RhhBattleGrounding *out);
    int (*preview_entry_ai)(uint32_t terrain, struct RhhBattleGrounding *out);
};
#define RHH_BATTLE_GROUNDING_SERVICE_V1_0_SIZE offsetof(struct RhhBattleGroundingService, preview_live)
#define RHH_BATTLE_GROUNDING_SERVICE_V1_1_SIZE offsetof(struct RhhBattleGroundingService, entry_ai)
#define RHH_BATTLE_GROUNDING_SERVICE_V1_2_SIZE sizeof(struct RhhBattleGroundingService)
/* Coalesced frame observation. event_key is map, ordinal is process-local
 * sequence, args are changed bits, new weather, new terrain and suppression.
 * The first sample seeds the baseline; intermediate states can be missed. */
struct RhhBattleEnvironmentObservation {
    uint32_t struct_size, changed;
    uint64_t sequence;
    struct RhhBattleEnvironmentState before, after;
};
enum RhhBattleEnvironmentCause { RHH_BATTLE_ENV_CAUSE_UNKNOWN = 0,
    RHH_BATTLE_ENV_CAUSE_MOVE, RHH_BATTLE_ENV_CAUSE_ABILITY,
    RHH_BATTLE_ENV_CAUSE_EXPIRED, RHH_BATTLE_ENV_CAUSE_CLEARED,
    RHH_BATTLE_ENV_CAUSE_PRIMAL_ENDED, RHH_BATTLE_ENV_CAUSE_STARTING_STATUS,
    RHH_BATTLE_ENV_CAUSE_NATURAL_WEATHER, RHH_BATTLE_ENV_CAUSE_WORLD_SEED,
    RHH_BATTLE_ENV_CAUSE_INNATE };
/* Verified transition at a supported engine mutation boundary. The source
 * slot may be RHH_BATTLE_NO_BATTLER; move and ability are zero when the
 * caller does not supply them. Not all engine paths are covered. event_key
 * is move or ability, subject is battle_id, ordinal is sequence; args are
 * cause, source slot, change bits and zero. */
struct RhhBattleEnvironmentCommit {
    uint32_t struct_size, changed, cause, source_battler;
    uint64_t sequence;
    RhhContentKey move, ability;
    struct RhhBattleEnvironmentState before, after;
};
enum RhhBattleEnvironmentTimerKind { RHH_BATTLE_ENV_TIMER_WEATHER = 1, RHH_BATTLE_ENV_TIMER_TERRAIN };
/* A non-expiring decrement at an engine end-of-turn timer boundary. The final
 * decrement is reported by the existing expiry commit instead. event_key is
 * zero, subject is battle_id, ordinal is sequence, and args are kind, old
 * counter, new counter and zero. Zero counters are not decremented. */
struct RhhBattleEnvironmentTimerTick {
    uint32_t struct_size, kind;
    uint64_t sequence;
    struct RhhBattleEnvironmentState before, after;
};
/* Committed effective-weather suppression change caused by a supported
 * ability mutation. cause is an RHH_ABILITY_MUTATION_* value; triggering_ability
 * is present for contact mutations only. target_mask marks slots addressed by
 * the mutation, including both slots of a swap. event_key is move or triggering_ability,
 * subject is battle_id, ordinal is sequence; args are cause, source battler,
 * prior suppression and new suppression. The v1.38 gas-specific event uses
 * the same payload with an RHH_GAS_* cause, the gas source slot as source and
 * target, and Neutralizing Gas as triggering_ability. The v1.39 presence
 * event uses RhhWeatherPresenceCause; source and target identify the slot,
 * and move and triggering_ability are zero. */
struct RhhBattleWeatherSuppressionChange {
    uint32_t struct_size, cause, source_battler, target_mask;
    uint64_t sequence;
    RhhContentKey move, triggering_ability;
    struct RhhBattleEnvironmentState before, after;
};
/* A field slot entered, switched out or reached its faint-animation boundary.
 * The slot can contain the replacement by the time an entry is reported. */
enum RhhWeatherPresenceCause { RHH_WEATHER_PRESENCE_ENTERED = 1,
    RHH_WEATHER_PRESENCE_SWITCHED_OUT, RHH_WEATHER_PRESENCE_FAINTED };
/* rhh:service/battle v1.45: copied views. current is available from
 * intro completion through the end notification. context is available only
 * in RHH_BATTLE_STARTED, RHH_BATTLER_ENTERED and RHH_BATTLE_ENDED callbacks. */
struct RhhBattleService {
    struct RhhServiceHeader header;
    int (*current)(struct RhhBattleSnapshot *out);
    int (*context)(struct RhhBattleEventContext *out);
    /* v1.1: original trainer-pool selection for a generated opponent slot. */
    int (*trainer_party_source)(uint32_t owner, uint32_t party_slot, struct RhhTrainerPartySource *out);
    /* v1.2: available only inside a BATTLE_TURN_ENDED observer. */
    int (*turn_context)(struct RhhBattleTurnContext *out);
    /* v1.3: definitions are available after finalization. State is valid only
     * during an ordinary active battle. set is allowed during committed
     * MOVE_RESOLVED or MOVE_EFFECT_APPLY observers; v1.8 also permits it during
     * INNATE_ENTRY; v1.9 also permits INNATE_TURN_END; v1.20 also permits
     * status and item-status observers with a checked target; v1.26 also
     * permits cure observers. Zero removes the condition. Timer ticks before
     * TURN_ENDED observers and expires after the stated completed turns. */
    uint32_t (*condition_count)(void);
    int (*condition_at)(uint32_t index, struct RhhBattleConditionInfo *out);
    int (*condition_info)(RhhContentKey key, struct RhhBattleConditionInfo *out);
    int (*condition_state)(RhhContentKey key, uint32_t target, struct RhhBattleConditionState *out);
    int (*condition_set)(RhhContentKey key, uint32_t target, uint32_t remaining_turns);
    /* v1.4: enumerate live instances in an ordinary active battle. */
    int (*active_condition_count)(uint32_t *out);
    int (*active_condition_at)(uint32_t index, struct RhhActiveBattleCondition *out);
    /* v1.5: available only during RHH_BATTLE_CONDITION_CHANGED. */
    int (*condition_change_context)(struct RhhBattleConditionChange *out);
    /* v1.6: queue a message only from a condition-expiration observer. The
     * same text rules as effect_message apply; up to 32 messages per turn. */
    int (*condition_expiry_message)(const char *text);
    /* v1.7: copied stages for a present battler in a local active battle. */
    int (*stat_stages)(uint32_t battler, struct RhhBattleStatStages *out);
    /* v1.10: copied request during RHH_BATTLE_CONDITION_PROPOSAL only. */
    int (*condition_proposal_context)(struct RhhBattleConditionProposal *out);
    /* Same command as condition_set, with a copied result and rejection reason. */
    int (*condition_set_result)(RhhContentKey key, uint32_t target, uint32_t remaining_turns,
                                struct RhhBattleConditionSetResult *out);
    /* v1.11: true/stat nature and provenance for a present local battler. */
    int (*nature)(uint32_t battler, struct RhhBattleNature *out);
    /* v1.12: queue an RHH_EFFECT_ANIM_* cue during a condition-expiry
     * observer. At most 32 cues per turn; playback follows innate actions
     * and precedes expiry messages. The original recipient is rechecked. */
    int (*condition_expiry_animation)(uint32_t battler, uint32_t animation);
    /* v1.13: queue a cue for a non-expiry condition change committed from a
     * move-effect observer. It shares that effect's eight-action queue.
     * v1.14 also supports innate turn-end changes in their action queue;
     * v1.15 supports innate entry changes after entry healing and stat actions;
     * v1.16 supports move-resolution changes during final cleanup. v1.22
     * supports status and item-status changes after their presentation;
     * v1.26 also supports changes from cure observers. */
    int (*condition_change_animation)(uint32_t battler, uint32_t animation);
    /* v1.17: side of a battle slot, including a prospective AI switch-in.
     * Returns an RHH_CONDITION_*_SIDE value without revealing its occupant. */
    int (*side_of)(uint32_t battler, uint32_t *side);
    /* v1.18: copied during RHH_BATTLER_STATUS_APPLIED; v1.20 also permits
     * nested condition proposals and notifications originating there. */
    int (*status_application_context)(struct RhhBattleStatusApplication *out);
    /* v1.19: copied during RHH_BATTLER_ITEM_STATUS_CHANGED; v1.20 also permits
     * nested condition proposals and notifications originating there. */
    int (*item_status_context)(struct RhhBattleItemStatusChange *out);
    /* v1.21: copied attribution for the current status application, also
     * available in its nested condition proposal and change callbacks. */
    int (*status_attribution)(struct RhhBattleStatusAttribution *out);
    /* v1.23: queue actions during a committed status/item-status observer or
     * its nested condition-change observer; v1.26 also permits cure observers.
     * v1.41 adds active and off-field Bag cures and nested condition changes.
     * All share the eight-action status
     * queue and execute after the supported presentation. Stat changes use
     * the engine's unsourced battler-stat rules. */
    int (*status_reaction_animation)(uint32_t battler, uint32_t animation);
    int (*status_reaction_heal)(uint32_t battler, uint32_t numerator, uint32_t denominator);
    int (*status_reaction_stat)(uint32_t battler, uint32_t stat, int32_t stages);
    /* v1.24: source-attributed ability-style actions. Both participants are
     * checked at queue and playback time; status uses RHH_EFFECT_SLEEP through
     * RHH_EFFECT_TOXIC. They share the status reaction queue. */
    int (*status_reaction_stat_from)(uint32_t source, uint32_t battler, uint32_t stat, int32_t stages);
    int (*status_reaction_status_from)(uint32_t source, uint32_t battler, uint32_t status);
    /* v1.25: indirect fractional damage, with the same flags as move-effect
     * damage. Shares the status reaction queue and processes fainting and
     * HP-threshold held items through the engine. */
    int (*status_reaction_damage)(uint32_t battler, uint32_t numerator, uint32_t denominator, uint32_t flags);
    /* v1.26: cure selected RHH_BATTLE_CONDITION_* major statuses, and query
     * a committed RHH_BATTLER_STATUS_CURED event or its nested condition hooks. */
    int (*status_reaction_cure)(uint32_t battler, uint32_t conditions);
    int (*status_cure_context)(struct RhhBattleStatusCure *out);
    /* v1.27: cause and ability for a supported cure event; available in the
     * same callbacks as status_cure_context. */
    int (*status_cure_attribution)(struct RhhBattleStatusCureAttribution *out);
    /* v1.28: stable curing move key, or zero when the cure has no known move.
     * Available in the same callbacks as status_cure_context. */
    int (*status_cure_move)(RhhContentKey *out);
    /* v1.30: available during a party cure observer and nested condition hooks. */
    int (*party_status_cure_context)(struct RhhBattlePartyStatusCure *out);
    /* v1.31: available only inside RHH_BATTLER_SPEED. */
    int (*speed_context)(struct RhhBattleSpeedContext *out);
    /* v1.32: current field and copied coalesced-observation context. */
    int (*environment)(struct RhhBattleEnvironmentState *out);
    int (*environment_observation)(struct RhhBattleEnvironmentObservation *out);
    /* v1.33: copied only during RHH_BATTLE_ENVIRONMENT_COMMITTED.
     * v1.34 adds starting-status, natural-weather and world-seed causes;
     * v1.35 adds innate attribution. */
    int (*environment_commit)(struct RhhBattleEnvironmentCommit *out);
    /* v1.36: copied only during RHH_BATTLE_ENVIRONMENT_TIMER_TICKED. */
    int (*environment_timer_tick)(struct RhhBattleEnvironmentTimerTick *out);
    /* v1.37: copied only during RHH_BATTLE_WEATHER_SUPPRESSION_CHANGED. */
    int (*weather_suppression_change)(struct RhhBattleWeatherSuppressionChange *out);
    /* v1.38: copied only during RHH_BATTLE_WEATHER_GAS_SUPPRESSION_CHANGED. */
    int (*weather_gas_suppression_change)(struct RhhBattleWeatherSuppressionChange *out);
    /* v1.39: copied only during RHH_BATTLE_WEATHER_PRESENCE_SUPPRESSION_CHANGED;
     * cause is RhhWeatherPresenceCause, source and target mask identify the slot. */
    int (*weather_presence_suppression_change)(struct RhhBattleWeatherSuppressionChange *out);
    /* v1.40: copied only in the party-item cure observer and its nested
     * condition proposal/change callbacks. */
    int (*party_item_status_cure_context)(struct RhhBattlePartyItemStatusCure *out);
    /* v1.44: copied only during RHH_BATTLER_HEALTH_OBSERVED. */
    int (*health_observation_context)(struct RhhBattlerHealthObservation *out);
};
#define RHH_BATTLE_SERVICE_V1_0_SIZE offsetof(struct RhhBattleService, trainer_party_source)
#define RHH_BATTLE_SERVICE_V1_1_SIZE offsetof(struct RhhBattleService, turn_context)
#define RHH_BATTLE_SERVICE_V1_2_SIZE offsetof(struct RhhBattleService, condition_count)
#define RHH_BATTLE_SERVICE_V1_3_SIZE offsetof(struct RhhBattleService, active_condition_count)
#define RHH_BATTLE_SERVICE_V1_4_SIZE offsetof(struct RhhBattleService, condition_change_context)
#define RHH_BATTLE_SERVICE_V1_5_SIZE offsetof(struct RhhBattleService, condition_expiry_message)
#define RHH_BATTLE_SERVICE_V1_6_SIZE offsetof(struct RhhBattleService, stat_stages)
#define RHH_BATTLE_SERVICE_V1_7_SIZE offsetof(struct RhhBattleService, condition_proposal_context)
#define RHH_BATTLE_SERVICE_V1_8_SIZE RHH_BATTLE_SERVICE_V1_7_SIZE
#define RHH_BATTLE_SERVICE_V1_9_SIZE RHH_BATTLE_SERVICE_V1_7_SIZE
#define RHH_BATTLE_SERVICE_V1_10_SIZE offsetof(struct RhhBattleService, nature)
#define RHH_BATTLE_SERVICE_V1_11_SIZE offsetof(struct RhhBattleService, condition_expiry_animation)
#define RHH_BATTLE_SERVICE_V1_12_SIZE offsetof(struct RhhBattleService, condition_change_animation)
#define RHH_BATTLE_SERVICE_V1_13_SIZE offsetof(struct RhhBattleService, side_of)
#define RHH_BATTLE_SERVICE_V1_14_SIZE RHH_BATTLE_SERVICE_V1_13_SIZE
#define RHH_BATTLE_SERVICE_V1_15_SIZE RHH_BATTLE_SERVICE_V1_13_SIZE
#define RHH_BATTLE_SERVICE_V1_16_SIZE RHH_BATTLE_SERVICE_V1_13_SIZE
#define RHH_BATTLE_SERVICE_V1_17_SIZE offsetof(struct RhhBattleService, status_application_context)
#define RHH_BATTLE_SERVICE_V1_18_SIZE offsetof(struct RhhBattleService, item_status_context)
#define RHH_BATTLE_SERVICE_V1_19_SIZE offsetof(struct RhhBattleService, status_attribution)
#define RHH_BATTLE_SERVICE_V1_20_SIZE RHH_BATTLE_SERVICE_V1_19_SIZE
#define RHH_BATTLE_SERVICE_V1_21_SIZE offsetof(struct RhhBattleService, status_reaction_animation)
#define RHH_BATTLE_SERVICE_V1_22_SIZE RHH_BATTLE_SERVICE_V1_21_SIZE
#define RHH_BATTLE_SERVICE_V1_23_SIZE offsetof(struct RhhBattleService, status_reaction_stat_from)
#define RHH_BATTLE_SERVICE_V1_24_SIZE offsetof(struct RhhBattleService, status_reaction_damage)
#define RHH_BATTLE_SERVICE_V1_25_SIZE offsetof(struct RhhBattleService, status_reaction_cure)
#define RHH_BATTLE_SERVICE_V1_26_SIZE offsetof(struct RhhBattleService, status_cure_attribution)
#define RHH_BATTLE_SERVICE_V1_27_SIZE offsetof(struct RhhBattleService, status_cure_move)
#define RHH_BATTLE_SERVICE_V1_28_SIZE offsetof(struct RhhBattleService, party_status_cure_context)
#define RHH_BATTLE_SERVICE_V1_29_SIZE RHH_BATTLE_SERVICE_V1_28_SIZE
#define RHH_BATTLE_SERVICE_V1_30_SIZE offsetof(struct RhhBattleService, speed_context)
#define RHH_BATTLE_SERVICE_V1_31_SIZE offsetof(struct RhhBattleService, environment)
#define RHH_BATTLE_SERVICE_V1_32_SIZE offsetof(struct RhhBattleService, environment_commit)
#define RHH_BATTLE_SERVICE_V1_33_SIZE offsetof(struct RhhBattleService, environment_timer_tick)
#define RHH_BATTLE_SERVICE_V1_34_SIZE RHH_BATTLE_SERVICE_V1_33_SIZE
#define RHH_BATTLE_SERVICE_V1_35_SIZE RHH_BATTLE_SERVICE_V1_34_SIZE
#define RHH_BATTLE_SERVICE_V1_36_SIZE offsetof(struct RhhBattleService, weather_suppression_change)
#define RHH_BATTLE_SERVICE_V1_37_SIZE offsetof(struct RhhBattleService, weather_gas_suppression_change)
#define RHH_BATTLE_SERVICE_V1_38_SIZE offsetof(struct RhhBattleService, weather_presence_suppression_change)
#define RHH_BATTLE_SERVICE_V1_39_SIZE offsetof(struct RhhBattleService, party_item_status_cure_context)
#define RHH_BATTLE_SERVICE_V1_40_SIZE offsetof(struct RhhBattleService, health_observation_context)
#define RHH_BATTLE_SERVICE_V1_41_SIZE RHH_BATTLE_SERVICE_V1_40_SIZE
#define RHH_BATTLE_SERVICE_V1_42_SIZE RHH_BATTLE_SERVICE_V1_41_SIZE
/* v1.43 also reports Take Heart's move-attributed major-status cure. */
#define RHH_BATTLE_SERVICE_V1_43_SIZE RHH_BATTLE_SERVICE_V1_42_SIZE
#define RHH_BATTLE_SERVICE_V1_44_SIZE sizeof(struct RhhBattleService)
/* v1.45 attributes Defog, terrain-clearing moves and Teraform Zero clears. */
#define RHH_BATTLE_SERVICE_V1_45_SIZE RHH_BATTLE_SERVICE_V1_44_SIZE
/* Type multipliers use the engine's unsigned 4.12 scale (4096 = 1x).
 * Chart transforms are restricted to 0, 2048, 4096, or 8192. */
#define RHH_TYPE_ONE 4096u
enum RhhTypeMatchupFlags { RHH_TYPE_MATCHUP_INVERSE = 1u, RHH_TYPE_MATCHUP_EXCLUDED = 2u };
struct RhhTypeMatchup {
    uint32_t struct_size, flags, base, adjusted, effective, reserved;
    RhhContentKey attack, defense;
};
struct RhhTypeList {
    uint32_t struct_size, count;
    RhhContentKey types[3];
};
/* Distinct active types are bounded by the byte-sized runtime type space;
 * NONE and MYSTERY are reserved and cannot be assigned. */
#define RHH_ACTIVE_TYPE_LIMIT 254u
struct RhhExtendedTypeList {
    uint32_t struct_size, count;
    RhhContentKey types[RHH_ACTIVE_TYPE_LIMIT];
};
enum RhhTypeStabFlags { RHH_TYPE_STAB_TERA = 1u, RHH_TYPE_STAB_AI = 2u };
#define RHH_TYPE_STAB_MIN (RHH_TYPE_ONE / 2u)
#define RHH_TYPE_STAB_MAX (RHH_TYPE_ONE * 9u / 4u)
struct RhhTypeStabContext {
    uint32_t struct_size, flags, base, attacker, defender, reserved;
    RhhContentKey move, move_type, species, ability;
    struct RhhTypeList active_types, original_types;
};
struct RhhExtendedTypeStabContext {
    uint32_t struct_size, flags, base, attacker, defender, reserved;
    RhhContentKey move, move_type, species, ability;
    struct RhhExtendedTypeList active_types, original_types;
};
struct RhhSpeciesTypesContext {
    uint32_t struct_size, reserved;
    RhhContentKey species;
    struct RhhTypeList original;
};
/* Each relation describes this custom type's attack on, and defense against,
 * another type. RHH_TYPE_UNSPECIFIED leaves that direction to the other
 * declaration or the neutral default. All identities are stable keys. */
#define RHH_TYPE_UNSPECIFIED UINT32_MAX
#define RHH_CUSTOM_TYPE_LIMIT 256u
#define RHH_CUSTOM_TYPES_PER_MOD 64u
#define RHH_CUSTOM_TYPE_RELATION_LIMIT 256u
struct RhhTypeRelation {
    uint32_t struct_size, attack, defense, reserved;
    RhhContentKey other;
};
struct RhhModType {
    uint32_t struct_size, rgb, relation_count, reserved;
    const char *id, *label;
    const struct RhhTypeRelation *relations;
};
struct RhhTypeDefinition {
    uint32_t struct_size, rgb;
    RhhContentKey key;
    char id[RHH_MOD_ID_MAX], label[24];
};
enum RhhTypeDisplayFlags { RHH_TYPE_DISPLAY_CUSTOM = 1u, RHH_TYPE_DISPLAY_COLOR = 2u };
struct RhhTypeDisplayInfo {
    uint32_t struct_size, flags, rgb, reserved;
    RhhContentKey key;
    char id[RHH_MOD_ID_MAX], label[24];
};
/* "rhh:service/type_catalog" v1.1. count/at/find enumerate custom definitions
 * after finalization, in canonical ID order. describe also accepts canonical
 * built-in type keys. Built-in labels are derived from their stable IDs, and
 * their color is unavailable. Outputs are copied and failures leave them unchanged. */
struct RhhTypeCatalogService {
    struct RhhServiceHeader header;
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhTypeDefinition *out);
    int (*find)(RhhContentKey key, struct RhhTypeDefinition *out);
    int (*describe)(RhhContentKey key, struct RhhTypeDisplayInfo *out);
};
#define RHH_TYPE_CATALOG_SERVICE_V1_0_SIZE offsetof(struct RhhTypeCatalogService, describe)
#define RHH_TYPE_CATALOG_SERVICE_V1_1_SIZE sizeof(struct RhhTypeCatalogService)
/* Composition accepts up to this many input entries and removes duplicate
 * canonical keys, preserving first-occurrence order for chart callbacks. An
 * empty list is neutral. The result uses unsigned 4.12, with nonzero values
 * clamped to [1, UINT32_MAX] only after all factors have been composed. The
 * flags contain the requested inverse bit, EXCLUDED if any lookup was
 * excluded, and SATURATED if a nonzero result was clamped. Invalid requests
 * leave the output unchanged. NONE and MYSTERY are not valid list types. */
#define RHH_TYPE_COMPOSE_LIMIT 512u
enum RhhTypeCompositionFlags { RHH_TYPE_COMPOSITION_SATURATED = 1u << 2 };
struct RhhTypeComposition {
    uint32_t struct_size, flags, count, effective;
};
/* RHH_BATTLE_TERA_TYPE is a pure battle-scoped query. event_key is the
 * current battle species, subject and decision.value are the stored Tera
 * type key, and args[0] is the battler slot. Transforms may propose a valid
 * registered type key; constraints may deny but cannot rewrite it. Invalid or
 * denied proposals fall back to the stored type. Callbacks must be deterministic
 * and stable throughout the battle: the query also runs during AI evaluation.
 * The result is not saved and must not be interpreted as owned-mon identity. */
struct RhhTeraTypeContext {
    uint32_t struct_size, battler;
    uint64_t battle_id;
    RhhContentKey species, base_type;
};
enum RhhLegacyTypeCategory { RHH_TYPE_CATEGORY_MOVE_DEFAULT, RHH_TYPE_CATEGORY_PHYSICAL, RHH_TYPE_CATEGORY_SPECIAL };
enum RhhLegacyTypeCategoryFlags { RHH_TYPE_CATEGORY_EXCLUDED = 1u, RHH_TYPE_CATEGORY_DENIED = 2u,
    RHH_TYPE_CATEGORY_INVALID = 4u, RHH_TYPE_CATEGORY_IMPURE = 8u };
struct RhhLegacyTypeCategoryResult {
    uint32_t struct_size, category, flags, reserved;
    RhhContentKey type;
};
/* RHH_TYPE_LEGACY_CATEGORY is a pure type-wide proposal for registered custom
 * types. event_key identifies the type; decision.value starts at MOVE_DEFAULT.
 * Transforms may propose PHYSICAL or SPECIAL, constraints can deny but cannot
 * rewrite the proposal. Denied, invalid and impure proposals retain the
 * move's configured category. Recursive category queries fail. */
enum RhhPublicBattlerTypeFlags { RHH_PUBLIC_BATTLER_TYPES_UNKNOWN = 1u };
/* Player-facing type list. UNKNOWN covers disabled or unavailable indicators
 * and the engine's unknown-type placeholder; types.count is zero in that case. */
struct RhhPublicBattlerTypes {
    uint32_t struct_size, battler, flags, reserved;
    uint64_t battle_id;
    struct RhhExtendedTypeList types;
};
enum RhhBattlerTypeChangeFlags { RHH_BATTLER_TYPES_RAW_CHANGED = 1u,
    RHH_BATTLER_TYPES_EFFECTIVE_CHANGED = 2u };
/* Coalesced frame observation, not a committed mutation. Raw lists precede
 * Roost and Tera; effective lists match battler_list. Switches establish new
 * baselines. These authoritative views are not player-facing visibility. */
struct RhhBattlerTypeObservation {
    uint32_t struct_size, changed;
    uint64_t sequence;
    struct RhhBattleParticipant before_participant, after_participant;
    struct RhhExtendedTypeList before_raw, after_raw, before_effective, after_effective;
};
/* Coalesced player-facing observation. Unknown lists remain empty, and no
 * authoritative species or individual identity is included. */
struct RhhPublicBattlerTypeObservation {
    uint32_t struct_size, reserved;
    uint64_t sequence;
    struct RhhPublicBattlerTypes before, after;
};
/* "rhh:service/types" v1.16. Species proposals and existing move base-type
 * transforms can use custom type keys with an available runtime slot.
 * chart uses an explicitly requested inverse flag;
 * context copies the unmodified chart during RHH_TYPE_MATCHUP. species and
 * battler return distinct current engine types in their original order. */
struct RhhTypeService {
    struct RhhServiceHeader header;
    int (*chart)(RhhContentKey attack, RhhContentKey defense, uint32_t flags, struct RhhTypeMatchup *out);
    int (*context)(struct RhhTypeMatchup *out);
    int (*species)(RhhContentKey species, struct RhhTypeList *out);
    int (*battler)(uint32_t battler, struct RhhTypeList *out);
    int (*stab_context)(struct RhhTypeStabContext *out);
    int (*species_context)(struct RhhSpeciesTypesContext *out);
    int (*species_proposal)(struct RhhTypeList *out);
    int (*offer_species_types)(const struct RhhTypeList *types);
    int (*compose)(RhhContentKey attack, const RhhContentKey *defenses, uint32_t count,
                   uint32_t flags, struct RhhTypeComposition *out);
    /* v1.6: full ordered lists. Legacy three-type queries fail if the result
     * is larger than their output, rather than returning a partial result. */
    int (*species_list)(RhhContentKey species, struct RhhExtendedTypeList *out);
    int (*battler_list)(uint32_t battler, struct RhhExtendedTypeList *out);
    int (*species_proposal_list)(struct RhhExtendedTypeList *out);
    int (*offer_species_types_list)(const struct RhhExtendedTypeList *types);
    /* v1.7: full type context during RHH_TYPE_STAB_EXTENDED. */
    int (*stab_context_list)(struct RhhExtendedTypeStabContext *out);
    /* v1.9: copied only during RHH_BATTLE_TERA_TYPE. */
    int (*tera_type_context)(struct RhhTeraTypeContext *out);
    /* v1.10: current uniquely matched player-owned battler during the Tera
     * query. Fails without changing output for opponents, ambiguous identities,
     * unavailable storage, simulation-only or mismatched party Pokémon.
     * OT/personality is not a permanent individual ID. */
    int (*tera_owned_context)(struct RhhOwnedMonSnapshot *out);
    /* v1.11: resolve a registered custom type's pre-Gen IV category, and copy
     * the query context during its proposal. Queries do not consume RNG. */
    int (*legacy_category)(RhhContentKey type, struct RhhLegacyTypeCategoryResult *out);
    int (*legacy_category_context)(struct RhhLegacyTypeCategoryResult *out);
    /* v1.12: AI-known active types during move scoring and effect preview.
     * Unrevealed Illusion follows the engine's AI type simulation, including
     * its ordinary added-type and extended-list rules. */
    int (*ai_battler_list)(uint32_t battler, struct RhhExtendedTypeList *out);
    /* v1.13: species key assumed by the scoring side, including an unrevealed
     * Illusion. Available only during AI move scoring and effect preview. */
    int (*ai_battler_species)(uint32_t battler, RhhContentKey *out);
    /* v1.14: player-facing indicator types, including Illusion and the
     * indicator visibility policy. Does not initialize the Illusion cache. */
    int (*public_battler_types)(uint32_t battler, struct RhhPublicBattlerTypes *out);
    /* v1.15: copied only during RHH_BATTLER_TYPES_OBSERVED. */
    int (*battler_observation_context)(struct RhhBattlerTypeObservation *out);
    /* v1.16: copied only during RHH_PUBLIC_BATTLER_TYPES_OBSERVED. */
    int (*public_battler_observation_context)(struct RhhPublicBattlerTypeObservation *out);
};
#define RHH_TYPE_SERVICE_V1_0_SIZE offsetof(struct RhhTypeService, stab_context)
#define RHH_TYPE_SERVICE_V1_1_SIZE offsetof(struct RhhTypeService, species_context)
#define RHH_TYPE_SERVICE_V1_2_SIZE offsetof(struct RhhTypeService, compose)
#define RHH_TYPE_SERVICE_V1_3_SIZE offsetof(struct RhhTypeService, compose)
#define RHH_TYPE_SERVICE_V1_4_SIZE offsetof(struct RhhTypeService, species_list)
#define RHH_TYPE_SERVICE_V1_5_SIZE offsetof(struct RhhTypeService, species_list)
#define RHH_TYPE_SERVICE_V1_6_SIZE offsetof(struct RhhTypeService, stab_context_list)
#define RHH_TYPE_SERVICE_V1_7_SIZE offsetof(struct RhhTypeService, tera_type_context)
#define RHH_TYPE_SERVICE_V1_8_SIZE RHH_TYPE_SERVICE_V1_7_SIZE
#define RHH_TYPE_SERVICE_V1_9_SIZE offsetof(struct RhhTypeService, tera_owned_context)
#define RHH_TYPE_SERVICE_V1_10_SIZE offsetof(struct RhhTypeService, legacy_category)
#define RHH_TYPE_SERVICE_V1_11_SIZE offsetof(struct RhhTypeService, ai_battler_list)
#define RHH_TYPE_SERVICE_V1_12_SIZE offsetof(struct RhhTypeService, ai_battler_species)
#define RHH_TYPE_SERVICE_V1_13_SIZE offsetof(struct RhhTypeService, public_battler_types)
#define RHH_TYPE_SERVICE_V1_14_SIZE offsetof(struct RhhTypeService, battler_observation_context)
#define RHH_TYPE_SERVICE_V1_15_SIZE offsetof(struct RhhTypeService, public_battler_observation_context)
#define RHH_TYPE_SERVICE_V1_16_SIZE sizeof(struct RhhTypeService)
/* Move metadata and the current resolved species learnsets. Results use
 * stable keys, preserve engine ordering and duplicates, and are copied into
 * caller-owned storage. Accuracy 0 denotes the engine's always-hit value;
 * power 0 or 1 can have special meaning for a move's effect. The type is the
 * effective base type, before battle-dependent type changes. */
enum RhhMoveCategory { RHH_MOVE_CATEGORY_UNSPECIFIED, RHH_MOVE_PHYSICAL, RHH_MOVE_SPECIAL, RHH_MOVE_STATUS };
/* RHH_MOVE_EFFECT_MODE and RHH_MOVE_DAMAGE_EFFECT_MODE transform an eligible
 * status or damaging move's effect mode respectively. Each event's
 * event_key is the stable move key and decision.value starts at INHERITED,
 * or the declared mode when the move has a registered effect binding.
 * CUSTOM_STATUS selects a neutral status script; CUSTOM_DAMAGE observes the
 * normal hit script after built-in additional effects. Status moves require
 * no inherited additional effects; damaging moves require the ordinary hit
 * effect. Invalid modes or denial retain inherited behavior. RNG-consuming
 * proposals are discarded in favor of the initial mode; denial remains
 * effective. Both gameplay RNG streams are restored. Callbacks must be pure
 * and stable for the run; link and recorded battles use the inherited effect. */
enum RhhMoveEffectMode { RHH_MOVE_EFFECT_INHERITED, RHH_MOVE_EFFECT_CUSTOM_STATUS, RHH_MOVE_EFFECT_CUSTOM_DAMAGE };
#define RHH_MOVE_EFFECT_LIMIT 128u
#define RHH_MOVE_EFFECTS_PER_MOD 16u
#define RHH_MOVE_EFFECT_BINDING_LIMIT 512u
#define RHH_MOVE_EFFECT_BINDINGS_PER_MOD 64u
/* Stable ids use namespace:effect/name. Bindings are unique per move. */
struct RhhModMoveEffect {
    uint32_t struct_size, mode, reserved[2];
    const char *id, *label;
};
struct RhhModMoveEffectBinding {
    uint32_t struct_size, reserved;
    RhhContentKey move, effect;
};
struct RhhMoveEffectInfo {
    uint32_t struct_size, mode;
    RhhContentKey key;
    char id[RHH_MOD_ID_MAX], label[32];
};
enum RhhMoveTarget { RHH_MOVE_TARGET_NONE, RHH_MOVE_TARGET_SELECTED, RHH_MOVE_TARGET_SMART,
    RHH_MOVE_TARGET_DEPENDS, RHH_MOVE_TARGET_OPPONENT, RHH_MOVE_TARGET_RANDOM,
    RHH_MOVE_TARGET_BOTH, RHH_MOVE_TARGET_USER, RHH_MOVE_TARGET_ALLY,
    RHH_MOVE_TARGET_USER_AND_ALLY, RHH_MOVE_TARGET_USER_OR_ALLY,
    RHH_MOVE_TARGET_FOES_AND_ALLY, RHH_MOVE_TARGET_FIELD,
    RHH_MOVE_TARGET_OPPONENTS_FIELD, RHH_MOVE_TARGET_ALL_BATTLERS };
enum RhhMoveTargetPhase { RHH_MOVE_TARGET_SELECTION = 1, RHH_MOVE_TARGET_EXECUTION, RHH_MOVE_TARGET_AI };
/* RHH_MOVE_TARGET_RESOLVE runs on the engine's battler-specific target after
 * its native selection, execution or AI rules. event_key is the move,
 * subject the user's species and args are battler, phase, target and strike
 * count. Transforms can propose existing RHH_MOVE_TARGET_* kinds; constraints
 * may deny but cannot rewrite. The SMART and DEPENDS restrictions match the
 * base-target hook. Invalid, denied, recursive and unsupported battle queries
 * retain the engine target. Callbacks must be pure and choose consistently
 * across phases when their inputs agree. weather and terrain report the
 * underlying visible field conditions, not ability suppression or grounding.
 * AI queries do not expose live hidden abilities; the initial target reflects
 * the engine's AI knowledge. */
struct RhhMoveTargetContext {
    uint32_t struct_size, battler, phase, target, strike_count, weather, terrain, reserved;
    uint64_t run_id, battle_id;
    RhhContentKey move, species;
};
/* Current, unlocked target kind for a battler and phase. This is not a
 * selected recipient or a promise against later redirection or state changes. */
struct RhhMoveTargetPreview {
    uint32_t struct_size, battler, phase, target;
    uint64_t run_id, battle_id;
    RhhContentKey move;
};
/* Result of the engine's per-battler target filter, before target-kind,
 * cursor, redirection, immunity and move-outcome checks. */
struct RhhMoveTargetFilter {
    uint32_t struct_size, battler, candidate, allowed;
    uint64_t run_id, battle_id;
    RhhContentKey move;
};
/* RHH_MOVE_BASE_TARGET: event_key is the stable move key, subject and
 * decision.value are the original RHH_MOVE_TARGET_* value. Transforms may
 * propose an existing target kind; constraints may deny but cannot rewrite.
 * Invalid or denied proposals retain the original. SMART requires a
 * multi-strike move; DEPENDS can only retain an existing DEPENDS target.
 * Engine effect-specific target changes take precedence. Callbacks must be
 * pure and stable across selection, battle, AI and preview queries. Both
 * gameplay RNG streams are restored, and RNG-consuming proposals are ignored. */
enum RhhMoveFlags { RHH_MOVE_CONTACT = 1u, RHH_MOVE_SOUND = 2u, RHH_MOVE_PUNCH = 4u,
    RHH_MOVE_BITE = 8u, RHH_MOVE_PULSE = 16u, RHH_MOVE_BALLISTIC = 32u,
    RHH_MOVE_POWDER = 64u, RHH_MOVE_DANCE = 128u, RHH_MOVE_WIND = 256u,
    RHH_MOVE_SLICING = 512u, RHH_MOVE_HEALING = 1024u };
/* A registered move inherits an existing move's effect, flags, effect
 * arguments and animation, while supplying its own displayed text and core
 * parameters. The template and type are stable keys. */
struct RhhModMove {
    uint32_t struct_size, power, accuracy, pp, category, target, reserved;
    int32_t priority;
    RhhContentKey template_move, type;
    const char *id, *name, *description;
};
#define RHH_CUSTOM_MOVE_LIMIT 512u
#define RHH_CUSTOM_MOVES_PER_MOD 64u
struct RhhMoveInfo {
    uint32_t struct_size, category, target, flags, power, accuracy, pp;
    int32_t priority;
    RhhContentKey move, type;
};
enum RhhLearnsetKind { RHH_LEARNSET_LEVEL = 1, RHH_LEARNSET_TEACHABLE, RHH_LEARNSET_EGG };
/* RHH_LEARNSET_ENTRY resolves each entry after declared additions are merged.
 * event_key is the move, subject the species, ordinal the merged index;
 * args are kind, level, source and zero. decision.value starts at ordinal;
 * transforms and finalizers can set a 32-bit ordering rank; constraints may deny the
 * entry. Ranks reorder only equal-level entries in level-up lists. Equal
 * ranks retain merged order. Results are cached per run: hooks must be pure
 * and run-stable. Recursive learnset lookups return the engine definition. */
enum RhhLearnsetSource { RHH_LEARNSET_ENGINE = 0, RHH_LEARNSET_ADDITION = 1 };
/* Level 0 is an evolution move. Teachable and egg entries use level 0.
 * Declarations are borrowed and must remain unchanged until module unload. */
struct RhhModLearnset {
    uint32_t struct_size, kind, level, reserved;
    RhhContentKey species, move;
};
#define RHH_MOD_LEARNSETS_PER_MOD 256u
#define RHH_MOD_LEARNSETS_LIMIT 2048u
#define RHH_MOD_LEARNSETS_PER_SPECIES 128u
struct RhhLearnsetEntry {
    uint32_t struct_size, kind, level, reserved;
    RhhContentKey species, move;
};
struct RhhMoveTeachRequest;
struct RhhMoveTeachResult;
struct RhhOwnedMoveSet;
enum RhhMovePowerFlags { RHH_MOVE_POWER_AI = 1u, RHH_MOVE_POWER_FIXED = 2u,
    RHH_MOVE_POWER_CRITICAL = 4u };
#define RHH_MOVE_POWER_MAX 1024u
/* RHH_MOVE_POWER transforms the effective base power after the engine's
 * power modifiers and before the damage formula. event_key is move, subject
 * is attacker_species, and args are base_power, flags, attacker, defender.
 * Values outside 1..RHH_MOVE_POWER_MAX or denied proposals retain the engine
 * power. Constraints cannot rewrite proposals. Fixed-damage effects bypass
 * this phase. Callbacks must be pure and give the same result for equivalent
 * real and AI contexts; the AI flag is informational, not a balance switch.
 * Gameplay RNG is restored and an RNG-consuming proposal is discarded. */
struct RhhMovePowerContext {
    uint32_t struct_size, flags, base_power, attacker, defender;
    uint32_t weather, terrain; /* RHH_WEATHER_* and RHH_TERRAIN_* calculation inputs */
    uint32_t attacker_level, attacker_hp, attacker_max_hp;
    uint32_t defender_level, defender_hp, defender_max_hp;
    RhhContentKey move, chosen_move, base_move, move_type;
    RhhContentKey attacker_species, defender_species, attacker_ability, defender_ability;
};
enum RhhCriticalStageFlags { RHH_CRITICAL_AI = 1u };
/* Point-in-time shared effect observer queues; a successful query reserves
 * nothing and individual action validation can still reject an offer. */
struct RhhMoveEffectQueue {
    uint32_t struct_size, action_count, action_capacity, message_count, message_capacity;
};
enum RhhMoveQueuedActionKind {
    RHH_MOVE_QUEUED_HEAL = 1, RHH_MOVE_QUEUED_STATUS, RHH_MOVE_QUEUED_STAT,
    RHH_MOVE_QUEUED_HEAL_USER, RHH_MOVE_QUEUED_HEAL_FROM_HIT, RHH_MOVE_QUEUED_DRAIN,
    RHH_MOVE_QUEUED_HEAL_BATTLER, RHH_MOVE_QUEUED_DAMAGE_BATTLER,
    RHH_MOVE_QUEUED_ANIMATION, RHH_MOVE_QUEUED_STAT_BATTLER,
    RHH_MOVE_QUEUED_STATUS_BATTLER, RHH_MOVE_QUEUED_FIELD_RULE,
    RHH_MOVE_QUEUED_ROOM_TOGGLE, RHH_MOVE_QUEUED_FIELD_MOVE, RHH_MOVE_QUEUED_DAMAGE_FROM_HIT
};
/* Ordered queued request, not an execution receipt. recipient identifies the
 * action's subject; has_target says whether the original target is checked.
 * For heal/damage/drain first and second are the requested fraction. For
 * status, stat, animation and field actions first is the corresponding public
 * status, stat, animation or field rule; stages is set only for stat actions.
 * flags retains the corresponding public action flags. amount is the computed
 * HP amount for HEAL_FROM_HIT and DAMAGE_FROM_HIT, or the recorded damage
 * basis for DRAIN; it is zero for other kinds and is not an outcome. */
struct RhhMoveQueuedAction {
    uint32_t struct_size, kind, attacker, target, recipient, has_target;
    uint32_t first, second, flags, amount;
    int32_t stages;
    uint32_t reserved;
    RhhContentKey move;
};
/* RHH_MOVE_CRITICAL_STAGE is a pure transform/constraint on the ordinary
 * engine critical chance after ability and field immunities. Gen I uses a
 * 0..255 threshold out of 256; later generations use a 0..4 stage. Forced
 * or blocked criticals and self-inflicted damage bypass this event. args are
 * base value, generation (1..10, 10 denotes Champions), attacker and defender. decision.value starts
 * at the base and must remain within max_value. Denial, invalid values and
 * RNG-consuming callbacks retain the engine value. */
struct RhhCriticalStageContext {
    uint32_t struct_size, flags, generation, base_value, max_value, attacker, defender, reserved;
    uint32_t numerator, denominator, weather, terrain;
    RhhContentKey move, attacker_species, defender_species, attacker_ability, defender_ability;
};
enum RhhDamageStatPhase { RHH_DAMAGE_STAT_ATTACK = 1, RHH_DAMAGE_STAT_DEFENSE };
enum RhhDamageStatFlags { RHH_DAMAGE_STAT_AI = 1u, RHH_DAMAGE_STAT_CRITICAL = 2u,
    RHH_DAMAGE_STAT_SELF_INFLICTED = 4u };
/* Pure post-engine Attack/Defense proposal immediately before the ordinary
 * damage formula. stat_owner identifies whose raw stat was selected; stat
 * and stage_stat use RHH_STAT_* and can differ under Wonder Room. event_key
 * is move, subject attacker species, args are phase, engine value, flags and
 * stat owner. Only 1..max_value is accepted; max_value for Attack depends on
 * power and level to prevent overflow in the following damage calculation. */
struct RhhDamageStatContext {
    uint32_t struct_size, phase, flags, engine_value, max_value;
    uint32_t attacker, defender, stat_owner, stat, stage_stat;
    uint32_t power, weather, terrain, attacker_level;
    RhhContentKey move, move_type, attacker_species, defender_species;
    RhhContentKey attacker_ability, defender_ability;
};
enum RhhFinalDamageFlags { RHH_FINAL_DAMAGE_AI = 1u, RHH_FINAL_DAMAGE_CRITICAL = 2u,
    RHH_FINAL_DAMAGE_SELF_INFLICTED = 4u };
#define RHH_FINAL_DAMAGE_MAX 65535u
/* RHH_DAMAGE_FINAL: ordinary formula damage after the random roll, STAB, type
 * and other engine modifiers, before minimum damage and later survival or
 * Substitute adjustment. Each ordinary AI roll is a separate query. Immunity,
 * zero, fixed damage and other engine paths that return before post-roll
 * modifiers bypass it. Non-Z Gen V+ Beat Up uses an estimated contributor
 * list in native AI calculations. RHH_SELF_DAMAGE_FINAL is a separate opt-in event
 * for confusion and disobedience damage after their unrandomized formula;
 * its move and move_type keys can be zero, and its self-inflicted flag is set.
 * decision.value starts at engine_damage and accepted proposals are 1..MAX.
 * args are engine_damage, flags, attacker, defender; event_key is move and
 * subject is attacker species. Both gameplay RNG streams are restored and an
 * impure or denied proposal is discarded. This is not a committed hit. */
struct RhhFinalDamageContext {
    uint32_t struct_size, flags, engine_damage, max_damage, attacker, defender;
    uint32_t effectiveness_q12, weather, terrain, reserved;
    RhhContentKey move, move_type, attacker_species, defender_species, attacker_ability, defender_ability;
};
enum RhhMoveAccuracyFlags { RHH_MOVE_ACCURACY_AI = 1u };
/* RHH_MOVE_ACCURACY transforms an ordinary hit roll after engine modifiers.
 * event_key is the move, subject is attacker_species, and args are
 * base_accuracy, flags, attacker, defender. The engine's base_accuracy may
 * exceed 100 before the AI cap. Proposed values are integer percentages
 * 0..100; invalid or denied proposals retain the engine result. Guaranteed
 * hits and one-hit-KO rules bypass this phase; it does not override
 * semi-invulnerability. Mod
 * callbacks must be pure and produce equivalent results in battle and AI.
 * Gameplay RNG is restored and an RNG-consuming proposal is discarded. */
struct RhhMoveAccuracyContext {
    uint32_t struct_size, flags, base_accuracy, attacker, defender, weather;
    uint32_t attacker_level, attacker_hp, attacker_max_hp;
    uint32_t defender_level, defender_hp, defender_max_hp;
    RhhContentKey move, move_type, attacker_species, defender_species, attacker_ability, defender_ability;
};
enum RhhMoveAccuracyCheckFlags { RHH_ACCURACY_CHECK_OHKO = 1u,
    RHH_ACCURACY_CHECK_ENGINE_STURDY_FLAG = 2u, RHH_ACCURACY_CHECK_ENGINE_OHKO_NO_EFFECT_FLAG = 4u };
/* One engine accuracy check, not necessarily a random roll or final hit. */
struct RhhMoveAccuracyCheck {
    uint32_t struct_size, flags, attacker, target, turn, missed, target_result, reserved;
    uint64_t run_id, battle_id, sequence, attacker_identity, target_identity;
    RhhContentKey move, attacker_species, target_species;
};
/* RHH_CAN_SELECT_MOVE is a pure, repeatable constraint on choosing an
 * existing move slot. event_key is the move, subject is the battler species,
 * and args are battler, slot, current PP and turn. Denial is sticky. The
 * same rule is evaluated for player selection and AI-controlled battler
 * legality. Player-move prediction does not evaluate hidden player slots.
 * It does not restrict moves called by Sleep Talk or other move effects. Struggle is never
 * restricted. Callbacks must not mutate state or consume RNG. */
struct RhhMoveSelectionContext {
    uint32_t struct_size, battler, move_slot, pp, level, hp, max_hp, turn;
    uint64_t battle_id;
    RhhContentKey move, species;
};
struct RhhMoveSelectionResult {
    uint32_t struct_size, allowed;
    struct RhhMoveSelectionContext context;
    char reason[96];
};
enum RhhMovePPCostFlags { RHH_MOVE_PP_COST_PERMANENT = 1u, RHH_MOVE_PP_COST_SUBMOVE = 2u,
    RHH_MOVE_PP_COST_SPREAD = 4u, RHH_MOVE_PP_COST_AI = 8u };
#define RHH_MOVE_PP_COST_MAX 255u
/* RHH_MOVE_PP_COST runs only at an actual engine PP deduction, after the
 * ordinary cost and Pressure. event_key is the executing move, subject is
 * the attacker's current species, and args are base cost, attacker, target
 * and move slot. Transforms may propose 0..255; constraints cannot rewrite
 * the proposal. Denied or invalid proposals retain the engine cost. The engine
 * saturates deduction at the slot's current PP. This phase does not make a
 * zero-PP move selectable, and is not run for skipped deductions or AI
 * previews. Callbacks must be pure; no game RNG or persistent mutation. */
struct RhhMovePPCostContext {
    uint32_t struct_size, flags, base_cost, pp_before;
    uint32_t attacker, target, move_slot, reserved;
    uint64_t battle_id;
    RhhContentKey move, charged_move, attacker_species;
};
/* RHH_MOVE_PP_DEDUCTED observes the engine's PP-deduction phase after the
 * battle slot changes and, for permanent moves, its controller update is
 * queued. A zero paid value is possible. It is not a durable party receipt. */
struct RhhMovePPDeduction {
    uint32_t struct_size, cost, paid, pp_after, controller_queued, reserved;
    uint64_t run_id;
    struct RhhMovePPCostContext before;
};
enum RhhAiPPCostEstimateFlags {
    RHH_AI_PP_UNKNOWN_ABILITY = 1u, RHH_AI_PP_ASSUMED_TARGET = 2u,
    RHH_AI_PP_CALLED_MOVE = 4u, RHH_AI_PP_COMMIT_RULES_MAY_DIFFER = 8u,
    RHH_AI_PP_DEDUCTION_SKIPPED = 16u
};
/* Available during RHH_AI_MOVE_SCORE or RHH_AI_EFFECT_PREVIEW. Assumes the
 * candidate executes as itself and uses the AI's cached ability knowledge.
 * UNKNOWN_ABILITY means at least one relevant ability was none or unknown;
 * ASSUMED_TARGET means the actual recipient could differ from the candidate;
 * CALLED_MOVE means the selected move may execute a different move; and
 * COMMIT_RULES_MAY_DIFFER marks the presence of real PP rules. DEDUCTION_SKIPPED
 * identifies Struggle or an already active multi-turn/Bide continuation.
 * An estimate is not a guarantee: interruptions and future effects are not simulated. The
 * separate RHH_AI_MOVE_PP_COST hook uses RhhMovePPCostContext with the AI flag
 * and lets rules opt in without changing existing committed-only callbacks. */
struct RhhAiPPCostEstimate {
    uint32_t struct_size, flags, base_cost, cost;
    uint32_t pp_before, paid, pp_after, reserved;
};
enum RhhMoveAdditionalEffectKind {
    RHH_ADDITIONAL_SLEEP = 1, RHH_ADDITIONAL_POISON, RHH_ADDITIONAL_BURN,
    RHH_ADDITIONAL_FREEZE, RHH_ADDITIONAL_PARALYSIS, RHH_ADDITIONAL_TOXIC,
    RHH_ADDITIONAL_CONFUSION, RHH_ADDITIONAL_FLINCH,
    RHH_ADDITIONAL_STAT_RAISE, RHH_ADDITIONAL_STAT_LOWER,
    RHH_ADDITIONAL_SUN, RHH_ADDITIONAL_RAIN, RHH_ADDITIONAL_SAND,
    RHH_ADDITIONAL_ICE_WEATHER, RHH_ADDITIONAL_MISTY, RHH_ADDITIONAL_GRASSY,
    RHH_ADDITIONAL_ELECTRIC, RHH_ADDITIONAL_PSYCHIC
};
enum RhhMoveAdditionalTarget { RHH_ADDITIONAL_TARGET = 0, RHH_ADDITIONAL_USER = 1 };
enum RhhMoveAdditionalStat { RHH_ADDITIONAL_ACCURACY = RHH_BATTLE_ACCURACY, RHH_ADDITIONAL_EVASION = RHH_BATTLE_EVASION };
enum RhhMoveAdditionalStatus { RHH_ADDITIONAL_OK, RHH_ADDITIONAL_INVALID,
    RHH_ADDITIONAL_FULL, RHH_ADDITIONAL_UNAVAILABLE };
struct RhhMoveAdditionalEffect {
    uint32_t struct_size, kind, target, chance, stat, stages, reserved[2];
};
enum RhhMoveResolutionFlags { RHH_MOVE_RESOLUTION_UNABLE = 1u, RHH_MOVE_RESOLUTION_CHARGING = 2u,
    RHH_MOVE_RESOLUTION_ATTACKER_REPLACED = 4u };
enum RhhMoveResolutionTargetFlags {
    RHH_MOVE_RESULT_MISSED = 1u, RHH_MOVE_RESULT_FAILED = 2u, RHH_MOVE_RESULT_PROTECTED = 4u,
    RHH_MOVE_RESULT_UNAFFECTED = 8u, RHH_MOVE_RESULT_NOT_PRESENT = 16u,
    RHH_MOVE_RESULT_STAT_PREVENTED = 32u, RHH_MOVE_RESULT_STAT_CHANGED = 64u,
    RHH_MOVE_RESULT_SUPER_EFFECTIVE = 128u, RHH_MOVE_RESULT_NOT_VERY_EFFECTIVE = 256u
};
struct RhhMoveResolutionTarget {
    uint32_t battler, flags;
    int32_t last_damage; /* Engine's last signed value, not accumulated damage. */
    uint32_t reserved;
};
/* RHH_MOVE_RESOLVED is an observer-only notification at the engine's final
 * move cleanup, after target and multi-hit processing and before result flags
 * are cleared. UNABLE means the engine could not use the move; CHARGING is a
 * charge turn. Results describe all battler slots, not a list of targets or a
 * guarantee of success. Replacements can change the Pokémon in a slot. This
 * event does not run for AI simulation, link or recorded battles, and is not
 * guaranteed if battle execution ends before move cleanup. event_key is the
 * resolved move; args are attacker, last_target, flags and turn. sequence and
 * event.ordinal count these notifications separately within each battle.
 * chosen_move is zero when no stable key is available. */
struct RhhMoveResolutionContext {
    uint32_t struct_size, flags, attacker, last_target, turn, battler_count;
    uint64_t run_id, battle_id, sequence;
    RhhContentKey move, chosen_move;
    struct RhhMoveResolutionTarget battlers[RHH_BATTLE_MAX_PARTICIPANTS];
};
enum RhhMoveResolutionDamageFlags {
    RHH_MOVE_RESOLUTION_DAMAGE_RECORDED = 1u, RHH_MOVE_RESOLUTION_DAMAGE_MIXED_SOURCE = 2u,
    RHH_MOVE_RESOLUTION_DAMAGE_TARGET_CHANGED = 4u, RHH_MOVE_RESOLUTION_DAMAGE_ATTACKER_CHANGED = 8u
};
/* Damage updates observed by the engine's direct move-damage handler. These
 * are not a complete accounting of indirect damage, healing or every hit.
 * Ambiguous source or occupant flags clear the totals and update count. */
struct RhhMoveResolutionDamage {
    uint32_t struct_size, battler, flags, updates;
    uint64_t hp_lost, substitute_lost;
};
/* One positive HP or Substitute loss from the engine's direct move-damage
 * update. Slot identities are OT/personality and may collide or change. */
struct RhhMoveDamageApplied {
    uint32_t struct_size, attacker, target, turn, hp_lost, substitute_lost;
    uint32_t target_hp, target_max_hp, substitute_hp, attacker_owner, attacker_party_slot;
    uint32_t target_owner, target_party_slot, reserved;
    uint64_t run_id, battle_id, sequence, attacker_identity, target_identity;
    RhhContentKey move, attacker_species, target_species;
};
enum RhhMoveResolutionHealthFlags { RHH_MOVE_HEALTH_RECORDED = 1u,
    RHH_MOVE_HEALTH_TARGET_CHANGED = 2u, RHH_MOVE_HEALTH_SOURCE_CHANGED = 4u,
    RHH_MOVE_HEALTH_MIXED_MOVE = 8u };
/* Actual capped HP deltas from the direct move-damage handler, scripted HP
 * updates and the attacker-HP-to-zero command between target setup and move
 * cleanup. The attacker-HP-to-zero command is included starting in move
 * service v1.36. Scripted updates can be
 * caused by items, abilities or other effects, so they are not attributed to
 * the move. This is not a complete audit of all battle HP mutations. When a
 * participant or move is ambiguous, flags are retained but all totals and
 * update counts are zero. Substitute HP is reported separately by v1.30. */
struct RhhMoveResolutionHealth {
    uint32_t struct_size, battler, flags, direct_updates, scripted_updates, reserved;
    uint64_t direct_lost, direct_restored, scripted_lost, scripted_restored;
};
enum RhhMoveHealthComparisonFlags {
    RHH_MOVE_HEALTH_START_AVAILABLE = 1u, RHH_MOVE_HEALTH_END_AVAILABLE = 2u,
    RHH_MOVE_HEALTH_PARTICIPANT_CHANGED = 4u, RHH_MOVE_HEALTH_ACTOR_CHANGED = 8u,
    RHH_MOVE_HEALTH_MOVE_CHANGED = 16u, RHH_MOVE_HEALTH_NET_COMPARABLE = 32u,
    RHH_MOVE_HEALTH_NET_UNEXPLAINED = 64u, RHH_MOVE_HEALTH_TOTAL_OVERFLOW = 128u
};
/* Copied at RHH_MOVE_RESOLVED. Endpoints cover the existing health window
 * from target setup to cleanup. When NET_COMPARABLE is set, net_hp_change is
 * end minus start, and recorded_hp_change is restored minus lost from the
 * observed health updates. NET_UNEXPLAINED means those net changes differ.
 * ACTOR_CHANGED also covers an unavailable attacker endpoint. Missing target
 * endpoints cannot be compared.
 * Equal nets do not prove all mutations were observed: unrecorded changes can
 * cancel, and identity checks can miss identical replacements. */
struct RhhMoveHealthComparison {
    uint32_t struct_size, battler, flags, start_hp, start_max_hp, end_hp, end_max_hp, reserved;
    int64_t net_hp_change, recorded_hp_change;
};
/* RHH_AI_MOVE_SCORE transforms the AI's final positive score for a legal
 * move candidate after the built-in scoring phases. event_key is the move,
 * subject is attacker species, and args are score, attacker, candidate and
 * move slot. The candidate is the battler being evaluated, not necessarily
 * the move's eventual target. decision.value must be 0..255; denied or
 * invalid proposals retain the built-in score. Callbacks must be pure. */
struct RhhAiMoveScoreContext {
    uint32_t struct_size, score, attacker, candidate, move_slot, move_target, turn, reserved;
    uint64_t run_id, battle_id;
    RhhContentKey move, attacker_species, candidate_species;
};
enum RhhAiEvaluationFlags {
    RHH_AI_EVAL_HEAL_BLOCKED = 1u,
    RHH_AI_EVAL_CANDIDATE_SUBSTITUTE = 2u,
    RHH_AI_EVAL_PARTNER_SIMULATION = 4u
};
/* Copied AI cache for the current scoring candidate. Accuracy 101 means
 * bypassing the accuracy check; zero cache entries may be unavailable.
 * Ability keys are the AI's known values, with zero for none or unknown. */
struct RhhAiMoveEvaluation {
    uint32_t struct_size, flags, attacker, candidate, move_slot;
    uint32_t damage_min, damage_median, damage_max, damage_random;
    uint32_t accuracy, type_effectiveness, attacker_hp_percent, candidate_hp_percent;
    uint32_t reserved[3];
    uint64_t run_id, battle_id;
    RhhContentKey move, attacker_ability, candidate_ability;
};
/* Pure descriptions offered during RHH_AI_EFFECT_PREVIEW. They are possible
 * actions, not committed commands or guaranteed outcomes. */
#define RHH_AI_EFFECT_PREVIEW_LIMIT 8u
enum RhhAiEffectKind {
    RHH_AI_EFFECT_HEAL_TARGET = 1, RHH_AI_EFFECT_HEAL_USER,
    RHH_AI_EFFECT_STATUS_TARGET, RHH_AI_EFFECT_STAT_TARGET,
    RHH_AI_EFFECT_HEAL_USER_FROM_DAMAGE, RHH_AI_EFFECT_DRAIN_USER,
    RHH_AI_EFFECT_CONDITION, RHH_AI_EFFECT_HEAL_BATTLER, RHH_AI_EFFECT_DAMAGE_BATTLER,
    RHH_AI_EFFECT_STAT_BATTLER, RHH_AI_EFFECT_STATUS_BATTLER, RHH_AI_EFFECT_FIELD_RULE,
    RHH_AI_EFFECT_ROOM_TOGGLE, RHH_AI_EFFECT_DAMAGE_FROM_HIT_BATTLER
};
enum RhhAiEffectOfferResult { RHH_AI_EFFECT_OK, RHH_AI_EFFECT_INVALID,
    RHH_AI_EFFECT_FULL, RHH_AI_EFFECT_UNAVAILABLE };
struct RhhAiEffectAction {
    uint32_t struct_size, kind, chance, flags, first, second;
    int32_t stages;
    uint32_t reserved;
    RhhContentKey condition;
};
/* Opt in per offered action to the host's approximate score adjustment. */
#define RHH_AI_EFFECT_AUTO_SCORE 2u
#define RHH_AI_EFFECT_NO_BATTLER UINT32_MAX
enum RhhAiEffectEstimateFlags {
    RHH_AI_ESTIMATE_BLOCKED = 1u,
    RHH_AI_ESTIMATE_UNCERTAIN = 2u,
    RHH_AI_ESTIMATE_DAMAGE_CACHE = 4u,
    RHH_AI_ESTIMATE_SUBSTITUTE = 8u,
    RHH_AI_ESTIMATE_CONDITION_SEMANTICS_UNKNOWN = 16u
};
/* Deltas are conditional estimates for the requested recipient, before the
 * action chance or move accuracy. HP and stat deltas are signed; a status
 * uses 0 or 1; a condition uses the signed change in remaining turns. A field
 * rule uses 0 or 1 for its current inactive/active transition; a room toggle
 * uses -1 or 1 for its current active/inactive state. */
struct RhhAiEffectEstimate {
    uint32_t struct_size, kind, flags, recipient, chance;
    int32_t minimum, median, maximum;
    uint32_t reserved[2];
    RhhContentKey condition;
};
enum RhhMoveEffectApplyFlags { RHH_MOVE_EFFECT_FIRST_TARGET = 1u };
/* Indirect fractional damage normally respects Magic Guard. */
enum RhhEffectDamageFlags { RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD = 1u };
/* Bit 2 is separate from both IGNORE_MAGIC_GUARD and AI AUTO_SCORE. */
#define RHH_EFFECT_DAMAGE_FROM_HIT_INCLUDE_SUBSTITUTE 4u
enum RhhMoveStatusEffect { RHH_EFFECT_SLEEP = 1, RHH_EFFECT_POISON, RHH_EFFECT_BURN,
    RHH_EFFECT_FREEZE, RHH_EFFECT_PARALYSIS, RHH_EFFECT_TOXIC, RHH_EFFECT_CONFUSION };
/* Stable choices for the engine's existing general battle animations. */
enum RhhEffectAnimation { RHH_EFFECT_ANIM_HEAL = 1, RHH_EFFECT_ANIM_PROTECT,
    RHH_EFFECT_ANIM_AURA, RHH_EFFECT_ANIM_RAINBOW, RHH_EFFECT_ANIM_SWAMP, RHH_EFFECT_ANIM_TAILWIND };
/* RHH_MOVE_EFFECT_APPLY and RHH_MOVE_DAMAGE_EFFECT_APPLY run after the neutral
 * status animation or an ordinary damaging hit's built-in additional effects;
 * RHH_REGISTERED_MOVE_EFFECT runs before the appropriate move-keyed event,
 * before move-end processing once per valid target (on the first qualifying
 * hit for multi-hit moves). event_key is the move, args are
 * attacker, target, flags and turn. The sequence is separate per battle.
 * FIRST_TARGET identifies the first delivered target event for this move.
 * An interrupted, failed or unavailable target receives no event. A damaging
 * hit can notify for a target knocked out by the hit. The
 * registered event uses the effect key as event_key and the move as subject;
 * it runs before the move-keyed event with the same context and sequence. */
struct RhhMoveEffectApplyContext {
    uint32_t struct_size, flags, attacker, target, turn, move_target, reserved[2];
    uint64_t run_id, battle_id, sequence;
    RhhContentKey move, chosen_move;
};
/* Damage recorded at the engine's HP update for this hit, before any
 * additional effects. Zero damage is distinct from unavailable data. */
enum RhhMoveDamageFlags { RHH_MOVE_DAMAGE_RECORDED = 1u };
enum RhhEffectDamageHealingFlags { RHH_EFFECT_HEAL_INCLUDE_SUBSTITUTE = 1u };
enum RhhEffectDrainFlags { RHH_EFFECT_DRAIN_INCLUDE_SUBSTITUTE = 1u };
struct RhhMoveDamageEffectContext {
    uint32_t struct_size, flags, attacker, target, hp_lost, substitute_lost, reserved[2];
    uint64_t run_id, battle_id, sequence;
    RhhContentKey move;
};
/* RHH_MOVE_ADDITIONAL_EFFECTS is a constraint-stage collection event for
 * damaging moves using the engine's ordinary hit script. event_key is the
 * canonical move key, args[0] is the inherited effect count and args[1] the
 * remaining capacity. offer_additional_effect copies one proposal into that
 * capacity, in hook order. A denied collection discards all additions.
 * chance is 1..100. stat/stages are zero except for stat changes, which use
 * RHH_STAT_* (except HP), RHH_ADDITIONAL_ACCURACY or RHH_ADDITIONAL_EVASION,
 * and 1..6 stages. Status, weather and terrain effects require TARGET; stat
 * effects may target USER or TARGET. Descriptions
 * must be pure and stable for the active run; results are cached until its
 * lifecycle changes. Link and recorded battles use original effects. */
/* RHH_MOVE_BASE_PRIORITY is a pure, run-stable move-definition transform.
 * event_key is the move key; args[0] and decision.value start at the base
 * priority plus RHH_MOVE_PRIORITY_BIAS. Valid encoded values are 0..14,
 * corresponding to priorities -7..7. Constraints may deny but not rewrite;
 * invalid, denied or RNG-consuming proposals retain the original. Recursive
 * queries return the original. Struggle, link and recorded battles retain
 * their original priority. The engine applies ability and field rules after
 * this transform. The move-info service returns the transformed base value. */
#define RHH_MOVE_PRIORITY_BIAS 7
#define RHH_MOVE_PRIORITY_ENCODED_MAX 14u
/* RHH_MOVE_BATTLER_PRIORITY transforms the engine's battler-specific result
 * after its ability and terrain rules. It uses priority + 7 in decision.value;
 * valid encoded values 0..17 represent -7..+10. The event key is the
 * effective move, subject is species, and args are battler, encoded engine
 * priority, flags and zero. ability is the caller-supplied assumption, not a
 * fresh lookup: zero can mean no ability or unknown AI knowledge. v1.40 sets
 * RHH_MOVE_PRIORITY_AI for confirmed AI priority queries and AI damage
 * simulations; an unset flag does not prove a live battle calculation.
 * Rules must give consistent results for equivalent battle and AI inputs.
 * Quash and Struggle bypass
 * this event; Max Guard has its effective move and flag. Weather and terrain
 * are the underlying field values, without grounding or suppression checks.
 * Rejected, invalid, RNG-consuming and recursive proposals retain the engine
 * result; link and recorded battles bypass the hook. The event changes only
 * priority and does not set ability-specific flags such as Prankster. */
enum RhhMoveBattlerPriorityFlags { RHH_MOVE_PRIORITY_MAX_GUARD = 1u, RHH_MOVE_PRIORITY_AI = 2u };
#define RHH_MOVE_BATTLER_PRIORITY_ENCODED_MAX 17u
struct RhhMoveBattlerPriority {
    uint32_t struct_size, battler, flags, weather, terrain, hp, max_hp, level;
    int32_t base_priority, engine_priority;
    uint64_t run_id, battle_id;
    RhhContentKey move, chosen_move, species, ability;
};
enum RhhBeatUpContributorFlags {
    RHH_BEAT_UP_CONTRIBUTOR_AI = 1u,
    RHH_BEAT_UP_CONTRIBUTOR_ESTIMATED = 2u,
    RHH_BEAT_UP_CONTRIBUTOR_APPARENT = 4u,
    RHH_BEAT_UP_CONTRIBUTOR_OWN_PARTY = 8u,
    RHH_BEAT_UP_CONTRIBUTOR_SLOT_KNOWN = 16u,
};
#define RHH_BEAT_UP_CONTRIBUTOR_NO_SLOT UINT32_MAX
/* Per-calculation Gen V+ Beat Up source. Hit indices count eligible hits from
 * zero. AI entries may be estimates; an apparent species can be a disguise.
 * A party slot is not a permanent individual identity. */
struct RhhBeatUpContributor {
    uint32_t struct_size, flags, hit_index, party_slot, attacker, defender;
    RhhContentKey species;
};
/* "rhh:service/moves" v1.54. Counts and entries include repeated moves.
 * level is zero for teachable and egg moves. Invalid/disabled species, unknown
 * moves, invalid kinds, out-of-range indices and undersized outputs fail
 * without changing outputs. teach changes an owned party Pokémon only when
 * its expected snapshot and move slot still match, after constraints agree. */
struct RhhMoveService {
    struct RhhServiceHeader header;
    int (*info)(RhhContentKey move, struct RhhMoveInfo *out);
    int (*learnset_count)(RhhContentKey species, uint32_t kind, uint32_t *out);
    int (*learnset_at)(RhhContentKey species, uint32_t kind, uint32_t index, struct RhhLearnsetEntry *out);
    int (*teach)(const struct RhhMoveTeachRequest *request, struct RhhMoveTeachResult *out);
    int (*teach_context)(struct RhhMoveTeachResult *out);
    int (*party_moves)(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedMoveSet *out);
    /* Available only during RHH_MOVE_POWER; copies into caller-owned storage. */
    int (*power_context)(struct RhhMovePowerContext *out);
    /* Available only during RHH_MOVE_ACCURACY; copies into caller-owned storage. */
    int (*accuracy_context)(struct RhhMoveAccuracyContext *out);
    /* Copies a NUL-terminated UI-compatible ASCII name; unsupported glyphs
     * become '?'. Invalid keys or insufficient capacity leave out unchanged. */
    int (*name)(RhhContentKey move, char *out, uint32_t capacity);
    /* Available only to RHH_MOVE_ADDITIONAL_EFFECTS constraint callbacks. */
    int (*offer_additional_effect)(const struct RhhMoveAdditionalEffect *effect);
    /* Available only during RHH_MOVE_RESOLVED; copies into caller storage. */
    int (*resolution_context)(struct RhhMoveResolutionContext *out);
    /* Available during RHH_AI_EFFECT_PREVIEW and RHH_AI_MOVE_SCORE. */
    int (*ai_score_context)(struct RhhAiMoveScoreContext *out);
    /* v1.10: pure, run-stable effect mode preview and committed target phase.
     * The context is also available during RHH_REGISTERED_MOVE_EFFECT and
     * RHH_MOVE_DAMAGE_EFFECT_APPLY. */
    int (*effect_mode)(RhhContentKey move, uint32_t *out);
    int (*effect_context)(struct RhhMoveEffectApplyContext *out);
    /* v1.11: queue a copied battle message during a committed effect observer.
     * Printable supported ASCII and at most one newline; 1..120 bytes, at most
     * eight messages per target. Whitespace must separate nonempty words.
     * Rejected text or a full queue changes nothing. */
    int (*effect_message)(const char *text);
    /* v1.12: queue healing of the current effect target by numerator /
     * denominator of its non-Dynamax maximum HP, rounded down (minimum 1).
     * 1 <= numerator <= denominator <= 16; at most eight per target. This
     * requests an engine action, not an immediate or guaranteed HP change. */
    int (*effect_heal)(uint32_t numerator, uint32_t denominator);
    /* v1.13: queue an RHH_EFFECT_* status for the current target. Status,
     * healing and stat changes share eight slots in observer call order. */
    int (*effect_status)(uint32_t status);
    /* v1.14: queue a stat change for the current target. stat uses RHH_STAT_*
     * (except HP) or RHH_BATTLE_ACCURACY/EVASION; stages is -6..-1 or 1..6. */
    int (*effect_stat)(uint32_t stat, int32_t stages);
    /* v1.15: copied registered effects and the binding for a valid move. A
     * valid unbound move returns key zero; failures leave outputs unchanged. */
    uint32_t (*registered_effect_count)(void);
    int (*registered_effect_at)(uint32_t index, struct RhhMoveEffectInfo *out);
    int (*registered_effect_info)(RhhContentKey effect, struct RhhMoveEffectInfo *out);
    int (*registered_effect_for_move)(RhhContentKey move, RhhContentKey *out);
    /* v1.16: queue healing for the move's user. The fraction and shared
     * queue limits match effect_heal; this may also be used for field targets. */
    int (*effect_heal_user)(uint32_t numerator, uint32_t denominator);
    /* v1.18: copied damage for the current damaging effect, and a user-heal
     * request based on its recorded damage. Valid only in its observers. */
    int (*effect_damage_context)(struct RhhMoveDamageEffectContext *out);
    int (*effect_heal_user_from_damage)(uint32_t numerator, uint32_t denominator, uint32_t flags);
    /* v1.19: queue engine drain behavior from the recorded damaging hit. */
    int (*effect_drain_user)(uint32_t numerator, uint32_t denominator, uint32_t flags);
    /* v1.20: cached estimates during RHH_AI_MOVE_SCORE; v1.21 also makes
     * them available during RHH_AI_EFFECT_PREVIEW. */
    int (*ai_evaluation)(struct RhhAiMoveEvaluation *out);
    /* v1.21: offer copied descriptions during the preview constraint; read
     * the accepted ordered list during the corresponding AI score hook. */
    int (*ai_effect_offer)(const struct RhhAiEffectAction *action);
    int (*ai_effect_count)(uint32_t *out);
    int (*ai_effect_at)(uint32_t index, struct RhhAiEffectAction *out);
    /* v1.22: estimate one previewed action for a hypothetical recipient. */
    int (*ai_effect_estimate)(uint32_t index, uint32_t recipient, struct RhhAiEffectEstimate *out);
    /* v1.23: copied during RHH_MOVE_PP_COST; v1.24 also permits
     * RHH_AI_MOVE_PP_COST. */
    int (*pp_cost_context)(struct RhhMovePPCostContext *out);
    /* v1.24: read-only estimate during the current AI scoring/preview event. */
    int (*ai_pp_cost_estimate)(struct RhhAiPPCostEstimate *out);
    /* v1.25: preview this mod constraint for a player-owned battler, then
     * read its callback context. This does not evaluate the engine's separate
     * move limitations. Preview is unavailable during AI calculation,
     * rendering and nested reducers; failures leave output unchanged. */
    int (*selection)(uint32_t battler, uint32_t move_slot, struct RhhMoveSelectionResult *out);
    int (*selection_context)(struct RhhMoveSelectionContext *out);
    /* v1.26: queue healing for a specified living battler, including one
     * other than the user and current target. Uses the shared action limit. */
    int (*effect_heal_battler)(uint32_t battler, uint32_t numerator, uint32_t denominator);
    /* v1.27: offer a specific-battler heal during AI effect preview, and
     * read its recipient during AI scoring. Shares the normal preview limit. */
    int (*ai_effect_offer_heal_battler)(uint32_t battler, uint32_t numerator, uint32_t denominator,
                                        uint32_t chance, uint32_t flags);
    int (*ai_effect_heal_battler_at)(uint32_t index, uint32_t *battler);
    /* v1.28: queue indirect damage for a living battler, and offer/read the
     * corresponding optional AI preview. Fraction is 1..16; the damage
     * bypasses Substitute and respects Magic Guard unless explicitly opted out. */
    int (*effect_damage_battler)(uint32_t battler, uint32_t numerator, uint32_t denominator, uint32_t flags);
    int (*ai_effect_offer_damage_battler)(uint32_t battler, uint32_t numerator, uint32_t denominator,
                                          uint32_t chance, uint32_t flags);
    int (*ai_effect_damage_battler_at)(uint32_t index, uint32_t *battler);
    /* v1.29: queue a visual cue on a present living battler. Shares the
     * effect action limit and respects the engine's animation settings. */
    int (*effect_animation)(uint32_t battler, uint32_t animation);
    /* v1.30: copied per-slot damage summary during RHH_MOVE_RESOLVED. */
    int (*resolution_damage)(uint32_t battler, struct RhhMoveResolutionDamage *out);
    /* v1.31: request an engine stat-stage action for a specified living
     * battler, including for field moves, and describe it to AI independently. */
    int (*effect_stat_battler)(uint32_t battler, uint32_t stat, int32_t stages);
    int (*ai_effect_offer_stat_battler)(uint32_t battler, uint32_t stat, int32_t stages,
                                        uint32_t chance, uint32_t flags);
    int (*ai_effect_stat_battler_at)(uint32_t index, uint32_t *battler);
    /* v1.32: request an existing status for a specified living battler and
     * describe it to AI. The ordinary status and Substitute rules apply. */
    int (*effect_status_battler)(uint32_t battler, uint32_t status);
    int (*ai_effect_offer_status_battler)(uint32_t battler, uint32_t status, uint32_t chance, uint32_t flags);
    int (*ai_effect_status_battler_at)(uint32_t index, uint32_t *battler);
    /* v1.33: copied only during RHH_MOVE_TARGET_RESOLVE. */
    int (*target_context)(struct RhhMoveTargetContext *out);
    /* v1.34: opt the current move into a target lock at real target setup.
     * Available only to its execution-phase resolver at that boundary. */
    int (*target_lock)(void);
    /* v1.35: observed HP changes in a slot during RHH_MOVE_RESOLVED. */
    int (*resolution_health)(uint32_t battler, struct RhhMoveResolutionHealth *out);
    /* v1.37: compare observed HP updates with the same participant's
     * endpoint HP, only during RHH_MOVE_RESOLVED. */
    int (*resolution_health_comparison)(uint32_t battler, struct RhhMoveHealthComparison *out);
    /* v1.39: copied only during RHH_MOVE_BATTLER_PRIORITY. */
    int (*battler_priority_context)(struct RhhMoveBattlerPriority *out);
    /* v1.41: copied only during RHH_DAMAGE_STAT. */
    int (*damage_stat_context)(struct RhhDamageStatContext *out);
    /* v1.42: resolve without using or creating a live execution lock. */
    int (*target_preview)(uint32_t battler, RhhContentKey move, uint32_t phase, struct RhhMoveTargetPreview *out);
    /* v1.43: copy the engine's individual-candidate filter result. */
    int (*target_filter)(uint32_t battler, RhhContentKey move, uint32_t candidate, struct RhhMoveTargetFilter *out);
    /* v1.44: copy only during RHH_MOVE_PP_DEDUCTED. */
    int (*pp_deduction_context)(struct RhhMovePPDeduction *out);
    /* v1.45: copy only during RHH_MOVE_DAMAGE_APPLIED. */
    int (*damage_applied_context)(struct RhhMoveDamageApplied *out);
    /* v1.46: copy only during RHH_MOVE_ACCURACY_CHECKED. */
    int (*accuracy_checked_context)(struct RhhMoveAccuracyCheck *out);
    /* v1.47: indirect damage for a specified living battler, based on the
     * recorded damage of this hit. Fractions require 1 <= numerator <=
     * denominator <= 16. Flags permit IGNORE_MAGIC_GUARD and
     * FROM_HIT_INCLUDE_SUBSTITUTE; the AI offer also permits AUTO_SCORE.
     * A recorded zero queues an action that does no damage. An unavailable
     * hit or invalid request fails. Shares the effect queue and participant
     * checks; not the engine's recoil mechanic. AI offers use cached damage
     * and are uncertain, not guarantees or a multi-hit aggregate. */
    int (*effect_damage_from_hit)(uint32_t battler, uint32_t numerator, uint32_t denominator, uint32_t flags);
    int (*ai_effect_offer_damage_from_hit)(uint32_t battler, uint32_t numerator, uint32_t denominator,
                                           uint32_t chance, uint32_t flags);
    int (*ai_effect_damage_from_hit_at)(uint32_t index, uint32_t *battler);
    /* v1.48: copied only during RHH_MOVE_CRITICAL_STAGE. */
    int (*critical_stage_context)(struct RhhCriticalStageContext *out);
    /* v1.49: copied only during a committed move-effect observer. */
    int (*effect_queue)(struct RhhMoveEffectQueue *out);
    /* v1.50: copy an ordered queued action during that observer. */
    int (*effect_queue_at)(uint32_t index, struct RhhMoveQueuedAction *out);
    /* v1.51: copied during RHH_DAMAGE_FINAL; v1.52 also supports
     * RHH_SELF_DAMAGE_FINAL. v1.53 includes non-Z Gen V+ Beat Up AI hits. */
    int (*final_damage_context)(struct RhhFinalDamageContext *out);
    /* v1.54: contributor in the current power, damage-stat or final-damage
     * reducer (including innate variants); unavailable elsewhere. */
    int (*beat_up_contributor)(struct RhhBeatUpContributor *out);
};
#define RHH_MOVE_SERVICE_V1_0_SIZE offsetof(struct RhhMoveService, teach)
#define RHH_MOVE_SERVICE_V1_1_SIZE offsetof(struct RhhMoveService, party_moves)
#define RHH_MOVE_SERVICE_V1_2_SIZE offsetof(struct RhhMoveService, power_context)
#define RHH_MOVE_SERVICE_V1_3_SIZE offsetof(struct RhhMoveService, accuracy_context)
#define RHH_MOVE_SERVICE_V1_4_SIZE offsetof(struct RhhMoveService, name)
#define RHH_MOVE_SERVICE_V1_5_SIZE offsetof(struct RhhMoveService, name)
#define RHH_MOVE_SERVICE_V1_6_SIZE offsetof(struct RhhMoveService, offer_additional_effect)
#define RHH_MOVE_SERVICE_V1_7_SIZE offsetof(struct RhhMoveService, resolution_context)
#define RHH_MOVE_SERVICE_V1_8_SIZE offsetof(struct RhhMoveService, ai_score_context)
#define RHH_MOVE_SERVICE_V1_9_SIZE offsetof(struct RhhMoveService, effect_mode)
#define RHH_MOVE_SERVICE_V1_10_SIZE offsetof(struct RhhMoveService, effect_message)
#define RHH_MOVE_SERVICE_V1_11_SIZE offsetof(struct RhhMoveService, effect_heal)
#define RHH_MOVE_SERVICE_V1_12_SIZE offsetof(struct RhhMoveService, effect_status)
#define RHH_MOVE_SERVICE_V1_13_SIZE offsetof(struct RhhMoveService, effect_stat)
#define RHH_MOVE_SERVICE_V1_14_SIZE offsetof(struct RhhMoveService, registered_effect_count)
#define RHH_MOVE_SERVICE_V1_15_SIZE offsetof(struct RhhMoveService, effect_heal_user)
#define RHH_MOVE_SERVICE_V1_16_SIZE offsetof(struct RhhMoveService, effect_damage_context)
#define RHH_MOVE_SERVICE_V1_17_SIZE offsetof(struct RhhMoveService, effect_damage_context)
#define RHH_MOVE_SERVICE_V1_18_SIZE offsetof(struct RhhMoveService, effect_drain_user)
#define RHH_MOVE_SERVICE_V1_19_SIZE offsetof(struct RhhMoveService, ai_evaluation)
#define RHH_MOVE_SERVICE_V1_20_SIZE offsetof(struct RhhMoveService, ai_effect_offer)
#define RHH_MOVE_SERVICE_V1_21_SIZE offsetof(struct RhhMoveService, ai_effect_estimate)
#define RHH_MOVE_SERVICE_V1_22_SIZE offsetof(struct RhhMoveService, pp_cost_context)
#define RHH_MOVE_SERVICE_V1_23_SIZE offsetof(struct RhhMoveService, ai_pp_cost_estimate)
#define RHH_MOVE_SERVICE_V1_24_SIZE offsetof(struct RhhMoveService, selection)
#define RHH_MOVE_SERVICE_V1_25_SIZE offsetof(struct RhhMoveService, effect_heal_battler)
#define RHH_MOVE_SERVICE_V1_26_SIZE offsetof(struct RhhMoveService, ai_effect_offer_heal_battler)
#define RHH_MOVE_SERVICE_V1_27_SIZE offsetof(struct RhhMoveService, effect_damage_battler)
#define RHH_MOVE_SERVICE_V1_28_SIZE offsetof(struct RhhMoveService, effect_animation)
#define RHH_MOVE_SERVICE_V1_29_SIZE offsetof(struct RhhMoveService, resolution_damage)
#define RHH_MOVE_SERVICE_V1_30_SIZE offsetof(struct RhhMoveService, effect_stat_battler)
#define RHH_MOVE_SERVICE_V1_31_SIZE offsetof(struct RhhMoveService, effect_status_battler)
#define RHH_MOVE_SERVICE_V1_32_SIZE offsetof(struct RhhMoveService, target_context)
#define RHH_MOVE_SERVICE_V1_33_SIZE offsetof(struct RhhMoveService, target_lock)
#define RHH_MOVE_SERVICE_V1_34_SIZE offsetof(struct RhhMoveService, resolution_health)
#define RHH_MOVE_SERVICE_V1_35_SIZE offsetof(struct RhhMoveService, resolution_health_comparison)
#define RHH_MOVE_SERVICE_V1_36_SIZE RHH_MOVE_SERVICE_V1_35_SIZE
#define RHH_MOVE_SERVICE_V1_37_SIZE offsetof(struct RhhMoveService, battler_priority_context)
#define RHH_MOVE_SERVICE_V1_38_SIZE RHH_MOVE_SERVICE_V1_37_SIZE
#define RHH_MOVE_SERVICE_V1_39_SIZE offsetof(struct RhhMoveService, damage_stat_context)
#define RHH_MOVE_SERVICE_V1_40_SIZE RHH_MOVE_SERVICE_V1_39_SIZE
#define RHH_MOVE_SERVICE_V1_41_SIZE offsetof(struct RhhMoveService, target_preview)
#define RHH_MOVE_SERVICE_V1_42_SIZE offsetof(struct RhhMoveService, target_filter)
#define RHH_MOVE_SERVICE_V1_43_SIZE offsetof(struct RhhMoveService, pp_deduction_context)
#define RHH_MOVE_SERVICE_V1_44_SIZE offsetof(struct RhhMoveService, damage_applied_context)
#define RHH_MOVE_SERVICE_V1_45_SIZE offsetof(struct RhhMoveService, accuracy_checked_context)
#define RHH_MOVE_SERVICE_V1_46_SIZE offsetof(struct RhhMoveService, effect_damage_from_hit)
#define RHH_MOVE_SERVICE_V1_47_SIZE offsetof(struct RhhMoveService, critical_stage_context)
#define RHH_MOVE_SERVICE_V1_48_SIZE offsetof(struct RhhMoveService, effect_queue)
#define RHH_MOVE_SERVICE_V1_49_SIZE offsetof(struct RhhMoveService, effect_queue_at)
#define RHH_MOVE_SERVICE_V1_50_SIZE offsetof(struct RhhMoveService, final_damage_context)
#define RHH_MOVE_SERVICE_V1_51_SIZE offsetof(struct RhhMoveService, beat_up_contributor)
#define RHH_MOVE_SERVICE_V1_52_SIZE RHH_MOVE_SERVICE_V1_51_SIZE
#define RHH_MOVE_SERVICE_V1_53_SIZE RHH_MOVE_SERVICE_V1_52_SIZE
#define RHH_MOVE_SERVICE_V1_54_SIZE sizeof(struct RhhMoveService)
/* Caller initializes struct_size. Identity is OT ID << 32 | personality,
 * matching existing faint/capture hooks; duplicate identities are ambiguous. */
struct RhhOwnedMonSnapshot {
    uint32_t struct_size, location, box, slot;
    uint64_t identity;
    RhhContentKey species;
    /* Nature values are current engine ordinals, not persistent content keys. */
    uint32_t level, nature, stat_nature, is_egg;
    uint16_t ivs[RHH_STAT_COUNT], evs[RHH_STAT_COUNT];
    uint32_t max_iv, max_stat_ev, max_total_ev;
};
/* Field healing uses a copied party and HP snapshot as an optimistic token.
 * It never revives fainted, egg, or challenge-dead Pokémon. The owner is a
 * registered module key. Constraints must be pure and stable for previews. */
enum RhhFieldHealStatus { RHH_FIELD_HEAL_OK, RHH_FIELD_HEAL_INVALID,
    RHH_FIELD_HEAL_UNAVAILABLE, RHH_FIELD_HEAL_STALE, RHH_FIELD_HEAL_DENIED,
    RHH_FIELD_HEAL_NOT_NEEDED };
struct RhhFieldHealth {
    uint32_t struct_size, hp, max_hp, reserved;
    struct RhhOwnedMonSnapshot mon;
};
struct RhhFieldHealRequest {
    uint32_t struct_size, amount, reserved[2];
    RhhContentKey owner;
    struct RhhFieldHealth expected;
};
struct RhhFieldHealResult {
    uint32_t struct_size, status, is_preview, applied;
    RhhContentKey owner;
    struct RhhFieldHealth before, after;
    char reason[96];
};
/* Run and map always match. Selected conditions must match before proposal and
 * immediately before completion. Weather and terrain are saved semantic
 * values (zero means absent); step is the committed world-step clock. */
enum RhhFieldHealEnvironmentFlags { RHH_FIELD_HEAL_REQUIRE_WEATHER = 1u,
    RHH_FIELD_HEAL_REQUIRE_TERRAIN = 2u, RHH_FIELD_HEAL_REQUIRE_STEP = 4u };
struct RhhFieldHealEnvironmentRequest {
    uint32_t struct_size, flags, expected_weather, expected_terrain;
    RhhContentKey expected_map;
    uint64_t expected_run_id, expected_step;
    struct RhhFieldHealRequest heal;
};
enum RhhFieldConditionFlag { RHH_FIELD_SLEEP = 1u, RHH_FIELD_POISON = 2u, RHH_FIELD_TOXIC = 4u,
    RHH_FIELD_BURN = 8u, RHH_FIELD_FREEZE = 16u, RHH_FIELD_PARALYSIS = 32u, RHH_FIELD_FROSTBITE = 64u };
/* raw_status is an engine-format optimistic token; conditions are stable
 * semantic flags. Sleep and toxic counters are retained in raw_status. */
struct RhhFieldCondition {
    uint32_t struct_size, conditions, raw_status, reserved;
    struct RhhFieldHealth health;
};
/* Environment flags use RhhFieldHealEnvironmentFlags. A nonzero map or run
 * requires both; guarded requests always require both. Zero flags and zero
 * map/run omit the environment check. */
struct RhhFieldStatusRequest {
    uint32_t struct_size, cure, flags, expected_weather, expected_terrain, reserved;
    RhhContentKey owner, expected_map;
    uint64_t expected_run_id, expected_step;
    struct RhhFieldCondition expected;
};
struct RhhFieldStatusResult {
    uint32_t struct_size, status, is_preview, cured;
    struct RhhFieldStatusRequest request;
    struct RhhFieldCondition before, after;
    char reason[96];
};
struct RhhFieldPoisonContext;
struct RhhFieldPoisonHabitatContext;
struct RhhFieldHabitatGuard;
struct RhhFieldHabitatHealRequest;
struct RhhFieldHabitatCureRequest;
/* RHH_CAN_FIELD_HEAL is a constraint and RHH_FIELD_HEALED is an observer.
 * context() copies the proposal during either callback. Commands are allowed
 * only at safe overworld update points. Preview never changes party state. */
struct RhhFieldHealingService {
    struct RhhServiceHeader header;
    int (*party_at)(uint32_t slot, struct RhhFieldHealth *out);
    int (*preview)(const struct RhhFieldHealRequest *, struct RhhFieldHealResult *);
    int (*heal)(const struct RhhFieldHealRequest *, struct RhhFieldHealResult *);
    int (*context)(struct RhhFieldHealResult *out);
    /* v1.1: require saved environmental state as part of the transaction.
     * environment_context() succeeds only for such a proposal/notification. */
    int (*preview_environment)(const struct RhhFieldHealEnvironmentRequest *, struct RhhFieldHealResult *);
    int (*heal_environment)(const struct RhhFieldHealEnvironmentRequest *, struct RhhFieldHealResult *);
    int (*environment_context)(struct RhhFieldHealEnvironmentRequest *out);
    /* v1.2: cure selected major conditions through a checked transaction. */
    int (*condition_at)(uint32_t slot, struct RhhFieldCondition *out);
    int (*preview_cure)(const struct RhhFieldStatusRequest *, struct RhhFieldStatusResult *);
    int (*cure)(const struct RhhFieldStatusRequest *, struct RhhFieldStatusResult *);
    int (*cure_context)(struct RhhFieldStatusResult *out);
    /* v1.3: copied context for field-poison proposal and damage notification. */
    int (*poison_context)(struct RhhFieldPoisonContext *out);
    /* v1.4: heal or cure only while a sampled player habitat still matches. */
    int (*preview_habitat_heal)(const struct RhhFieldHabitatHealRequest *, struct RhhFieldHealResult *);
    int (*habitat_heal)(const struct RhhFieldHabitatHealRequest *, struct RhhFieldHealResult *);
    int (*preview_habitat_cure)(const struct RhhFieldHabitatCureRequest *, struct RhhFieldStatusResult *);
    int (*habitat_cure)(const struct RhhFieldHabitatCureRequest *, struct RhhFieldStatusResult *);
    int (*habitat_context)(struct RhhFieldHabitatGuard *out);
    /* v1.5: lazily sampled player habitat for poison proposals and receipts. */
    int (*poison_habitat_context)(struct RhhFieldPoisonHabitatContext *out);
};
#define RHH_FIELD_HEALING_SERVICE_V1_0_SIZE offsetof(struct RhhFieldHealingService, preview_environment)
#define RHH_FIELD_HEALING_SERVICE_V1_1_SIZE offsetof(struct RhhFieldHealingService, condition_at)
#define RHH_FIELD_HEALING_SERVICE_V1_2_SIZE offsetof(struct RhhFieldHealingService, poison_context)
#define RHH_FIELD_HEALING_SERVICE_V1_3_SIZE offsetof(struct RhhFieldHealingService, preview_habitat_heal)
#define RHH_FIELD_HEALING_SERVICE_V1_4_SIZE offsetof(struct RhhFieldHealingService, poison_habitat_context)
#define RHH_FIELD_HEALING_SERVICE_V1_5_SIZE sizeof(struct RhhFieldHealingService)
/* Stored experience and the species' native growth curve. At maximum level,
 * next_level_exp equals maximum_exp and to_next_level is zero. Daycare steps
 * not yet credited to the Pokémon are not included. */
struct RhhOwnedExperience {
    uint32_t struct_size, experience, level_start_exp, next_level_exp;
    uint32_t to_next_level, maximum_exp, reserved[2];
    struct RhhOwnedMonSnapshot mon;
};
enum RhhOwnedStatsFlags {
    RHH_OWNED_STATS_RECONSTRUCTED = 1u,
    RHH_OWNED_STATS_HP_LOSS_CLAMPED = 2u
};
/* stats[HP] is maximum HP; current_hp is separate. Party and fusion values
 * are copied from the stored Pokemon, including its stored level. Box and
 * daycare values are reconstructed from stored data and current stat rules;
 * pending daycare experience is excluded. The snapshot does not protect HP,
 * stored stats or experience from changes between snapshot and query. */
struct RhhOwnedStats {
    uint32_t struct_size, flags, level, current_hp;
    uint32_t stats[RHH_STAT_COUNT];
    struct RhhOwnedMonSnapshot mon;
};
/* Hyper Training changes the IV used for stat calculation, not the inherited
 * IV. Bits in hyper_trained use RhhMonStat indices. These values are sampled
 * at the call; the older owned-mon snapshot does not include training flags. */
struct RhhOwnedIvView {
    uint32_t struct_size, hyper_trained;
    uint16_t effective_ivs[RHH_STAT_COUNT];
    struct RhhOwnedMonSnapshot mon;
};
/* HARD_CAP means a hard-cap rule is active; MAX_LEVEL means the Pokémon was
 * already at the engine's maximum level before the proposed award. */
enum RhhExperiencePreviewFlags { RHH_EXPERIENCE_HARD_CAP = 1u, RHH_EXPERIENCE_MAX_LEVEL = 2u };
/* A preview of a fixed, already-scaled award. It does not simulate moves,
 * evolution, HP, animations, eligibility constraints, or a committed award. */
struct RhhExperiencePreview {
    uint32_t struct_size, flags, requested, applied;
    uint32_t before_exp, after_exp, before_level, after_level;
    uint32_t level_cap, cap_exp, reserved[2];
    struct RhhOwnedMonSnapshot mon;
};
/* Level-up learnset entries in engine order, before move-learning decisions.
 * Entries may already be known, suppressed by form rules, or require a slot
 * choice. Evolution can change the species and its later learnset. */
#define RHH_EXPERIENCE_MOVE_PAGE_SIZE 16u
struct RhhExperienceMoveCandidate { uint32_t level, reserved; RhhContentKey move; };
struct RhhExperienceMovePage {
    uint32_t struct_size, start, count, total;
    struct RhhExperiencePreview experience;
    struct RhhExperienceMoveCandidate candidates[RHH_EXPERIENCE_MOVE_PAGE_SIZE];
};
enum RhhExperienceAwardStatus {
    RHH_EXPERIENCE_AWARD_OK, RHH_EXPERIENCE_AWARD_INVALID, RHH_EXPERIENCE_AWARD_UNAVAILABLE,
    RHH_EXPERIENCE_AWARD_STALE, RHH_EXPERIENCE_AWARD_DENIED, RHH_EXPERIENCE_AWARD_NO_GAIN,
    RHH_EXPERIENCE_AWARD_PENDING
};
struct RhhExperienceAwardRequest {
    uint32_t struct_size, amount;
    RhhContentKey owner;
    struct RhhOwnedExperience expected;
};
/* A ticket identifies one process-local flow; it is not a durable receipt.
 * PENDING means the party menu has not applied the points yet. OK means the
 * points were applied; finished is set after move and evolution screens. */
struct RhhExperienceAwardResult {
    uint32_t struct_size, status, finished, reserved;
    uint64_t ticket;
    RhhContentKey owner;
    struct RhhExperiencePreview experience;
    char reason[96];
};
/* Battle EV yields use the stat order of RhhMonStat. earned is the engine's
 * held-item/Pokérus-adjusted yield before either EV cap; proposed is the
 * ordinary capped allocation; it may reduce an already over-cap EV in the
 * same cases as the engine. A denied allocation leaves after equal to
 * before. The notification is emitted even when caps prevent all gains. */
struct RhhBattleEvYield {
    uint32_t struct_size, applied, identity_unique, reserved;
    struct RhhOwnedMonSnapshot mon;
    RhhContentKey defeated_species;
    uint16_t before[RHH_STAT_COUNT], earned[RHH_STAT_COUNT];
    uint16_t proposed[RHH_STAT_COUNT], after[RHH_STAT_COUNT];
    char reason[96];
};
enum RhhBattleEvMode { RHH_BATTLE_EV_APPLY, RHH_BATTLE_EV_BANK };
/* During proposal/validation, accepted is zero. During the processed event it
 * distinguishes an accepted bank from a denied reward. */
struct RhhBattleEvPolicy {
    uint32_t struct_size, mode, accepted, reserved;
};
/* rhh:service/battle_ev_yield v1.2: context copies the transaction during
 * RHH_BATTLE_EV_PROPOSAL, RHH_CAN_APPLY_BATTLE_EVS and
 * RHH_BATTLE_EVS_PROCESSED. Constraints may deny the whole allocation; the
 * committed notification reports the result. */
struct RhhBattleEvYieldService {
    struct RhhServiceHeader header;
    int (*context)(struct RhhBattleEvYield *out);
    /* v1.1: valid during proposal, validation and processed callbacks. */
    int (*policy_context)(struct RhhBattleEvPolicy *out);
    /* v1.2: proposal and observer dispatches preserve gameplay RNG. */
};
#define RHH_BATTLE_EV_YIELD_SERVICE_V1_0_SIZE offsetof(struct RhhBattleEvYieldService, policy_context)
#define RHH_BATTLE_EV_YIELD_SERVICE_V1_1_SIZE sizeof(struct RhhBattleEvYieldService)
#define RHH_BATTLE_EV_YIELD_SERVICE_V1_2_SIZE RHH_BATTLE_EV_YIELD_SERVICE_V1_1_SIZE
enum RhhItemEvMode { RHH_ITEM_EV_APPLY, RHH_ITEM_EV_BANK };
/* One positive, single-stat EV item used on the player's field party. earned
 * is the item's uncapped amount; proposed is the engine's capped allocation.
 * The processed event fires only after a successful item removal. A BANK
 * proposal consumes the item and leaves the EVs unchanged; the observer is
 * responsible for recording any unspent points. */
struct RhhItemEvYield {
    uint32_t struct_size, mode, stat, earned, identity_unique, reserved[3];
    RhhContentKey item;
    struct RhhOwnedMonSnapshot mon;
    uint16_t before[RHH_STAT_COUNT], proposed[RHH_STAT_COUNT], after[RHH_STAT_COUNT];
};
/* A negative change reduces one stat; zero resets the marked stats.
 * The processed event also covers a successful friendship-only berry use. */
struct RhhItemEvReduction {
    uint32_t struct_size, identity_unique, stat_mask, reserved;
    int32_t change;
    uint32_t reserved2;
    RhhContentKey item;
    struct RhhOwnedMonSnapshot mon;
    uint16_t before[RHH_STAT_COUNT], proposed[RHH_STAT_COUNT], after[RHH_STAT_COUNT];
};
enum RhhItemEvPreviewStatus { RHH_ITEM_EV_PREVIEW_UNHANDLED, RHH_ITEM_EV_PREVIEW_DENIED,
    RHH_ITEM_EV_PREVIEW_APPLIED, RHH_ITEM_EV_PREVIEW_BANKED };
enum RhhItemEvPreviewKind { RHH_ITEM_EV_PREVIEW_NONE, RHH_ITEM_EV_PREVIEW_GAIN, RHH_ITEM_EV_PREVIEW_REDUCTION };
/* A forecast of the supported field-item transaction, not a reservation.
 * after holds the proposed post-use EVs; friendship and other non-EV effects
 * are not included. Unhandled means the normal engine path may still apply. */
struct RhhItemEvPreview {
    uint32_t struct_size, status, kind, reserved;
    struct RhhItemEvYield gain;
    struct RhhItemEvReduction reduction;
    char reason[96];
};
/* RHH_ITEM_EV_PROPOSAL transforms choose APPLY or BANK, constraints may deny.
 * Invalid choices retain APPLY. RHH_ITEM_EV_PROCESSED is an observer-only
 * committed notification; context is valid only in these two callbacks.
 * Version 1.2 preserves gameplay RNG across proposals and observers. */
struct RhhItemEvYieldService {
    struct RhhServiceHeader header;
    int (*context)(struct RhhItemEvYield *out);
    /* v1.1: current RHH_CAN_REDUCE_ITEM_EVS or processed reduction. */
    int (*reduction_context)(struct RhhItemEvReduction *out);
    /* v1.3: check a current party snapshot and an item key without using it. */
    int (*preview)(const struct RhhOwnedMonSnapshot *expected, RhhContentKey item, struct RhhItemEvPreview *out);
};
#define RHH_ITEM_EV_YIELD_SERVICE_V1_0_SIZE offsetof(struct RhhItemEvYieldService, reduction_context)
#define RHH_ITEM_EV_YIELD_SERVICE_V1_1_SIZE offsetof(struct RhhItemEvYieldService, preview)
#define RHH_ITEM_EV_YIELD_SERVICE_V1_2_SIZE RHH_ITEM_EV_YIELD_SERVICE_V1_1_SIZE
#define RHH_ITEM_EV_YIELD_SERVICE_V1_3_SIZE sizeof(struct RhhItemEvYieldService)
enum RhhEvLimitSource { RHH_EV_LIMIT_EDIT = 1, RHH_EV_LIMIT_BATTLE, RHH_EV_LIMIT_ITEM };
/* The six limits are bounded by the Pokémon's byte-sized EV fields. */
struct RhhEvLimits {
    uint32_t struct_size, source, total, reserved;
    uint16_t per_stat[RHH_STAT_COUNT];
    RhhContentKey item;
    struct RhhOwnedMonSnapshot mon;
};
/* RHH_EV_LIMITS transforms may set decision.value (total) and decision.stats
 * (per-stat caps). Constraints may only lower them; denial or out-of-range
 * values retain the engine's defaults. context is callback-scoped. query
 * previews edit limits for an unchanged party or PC snapshot. Version 1.1
 * restores gameplay RNG and ignores impure proposals. */
struct RhhEvLimitService {
    struct RhhServiceHeader header;
    int (*query)(const struct RhhOwnedMonSnapshot *expected, struct RhhEvLimits *out);
    int (*context)(struct RhhEvLimits *out);
};
#define RHH_EV_LIMIT_SERVICE_V1_0_SIZE sizeof(struct RhhEvLimitService)
#define RHH_EV_LIMIT_SERVICE_V1_1_SIZE RHH_EV_LIMIT_SERVICE_V1_0_SIZE
enum RhhOwnedIdentityFlags { RHH_OWNED_IDENTITY_CORRUPT = 1u };
/* OT/personality can collide and is not a permanent ID. Matching occupied
 * slots are counted even if corrupt; unique is copied only for one valid match. */
struct RhhOwnedIdentityQuery {
    uint32_t struct_size, matches, flags, unique_valid;
    uint32_t party_matches, box_matches, daycare_matches, fusion_matches;
    struct RhhOwnedMonSnapshot unique;
};
#define RHH_OWNED_IDENTITY_OCCURRENCE_LIMIT 512u
enum RhhOwnedIdentityOccurrenceFlags { RHH_OWNED_OCCURRENCE_UNREADABLE = 1u };
enum RhhOwnedIdentityOccurrencesFlags { RHH_OWNED_OCCURRENCES_TRUNCATED = 1u };
/* A current matching slot. species is zero when the occupied Pokémon cannot
 * be decoded. Locations and OT/personality are not durable identities. */
struct RhhOwnedIdentityOccurrence {
    uint32_t location, box, slot, flags;
    RhhContentKey species;
};
/* One bounded census in party, PC, daycare, then fusion order. matches counts
 * every slot; count is the number copied. A truncated result is incomplete. */
struct RhhOwnedIdentityOccurrences {
    uint32_t struct_size, flags, matches, count;
    uint64_t identity;
    struct RhhOwnedIdentityOccurrence entries[RHH_OWNED_IDENTITY_OCCURRENCE_LIMIT];
};
enum RhhOwnedMonChangeFlags {
    RHH_OWNED_CHANGE_RESET = 1u, RHH_OWNED_CHANGE_BEFORE = 2u, RHH_OWNED_CHANGE_AFTER = 4u
};
enum RhhOwnedMonChangedFields {
    RHH_OWNED_CHANGED_IDENTITY = 1u, RHH_OWNED_CHANGED_SPECIES = 2u,
    RHH_OWNED_CHANGED_LEVEL = 4u, RHH_OWNED_CHANGED_NATURE = 8u,
    RHH_OWNED_CHANGED_IVS = 16u, RHH_OWNED_CHANGED_EVS = 32u,
    RHH_OWNED_CHANGED_EGG = 64u, RHH_OWNED_CHANGED_LIMITS = 128u,
    RHH_OWNED_CHANGED_EXPERIENCE = 256u, RHH_OWNED_CHANGED_HYPER_TRAINING = 512u,
    RHH_OWNED_CHANGED_TERA = 1024u
};
/* Frame-sampled invalidation, not a transaction receipt. A RESET means all
 * cached slots must be refreshed. BEFORE/AFTER mark which snapshots exist. */
struct RhhOwnedMonChange {
    uint32_t struct_size, flags, fields, location, box, slot;
    uint64_t run_id, sequence;
    struct RhhOwnedMonSnapshot before, after;
};
/* Stored XP captured with an individual non-reset invalidation. flags use
 * RHH_OWNED_CHANGE_BEFORE/AFTER; absent sides have zero points. Like the
 * parent notification, this is frame-sampled rather than a transaction. */
struct RhhOwnedExperienceChange {
    uint32_t struct_size, flags, location, box, slot, before_exp, after_exp, reserved;
    uint64_t run_id, sequence;
};
enum RhhOwnedReleaseFlags { RHH_OWNED_RELEASE_MOVING = 1u, RHH_OWNED_RELEASE_UNREADABLE = 2u };
/* A confirmed PC release, captured before removal and delivered afterward.
 * For a held Pokémon, mon.location is MOVING and source is its last picked-up
 * slot, which may already have been reused. Identity remains OT/personality
 * and can collide. sequence is process-local and resets for each run. */
struct RhhOwnedRelease {
    uint32_t struct_size, flags, source_location, source_box, source_slot, reserved;
    uint64_t run_id, sequence;
    struct RhhOwnedMonSnapshot mon;
};
enum RhhOwnedMoveFlags { RHH_OWNED_MOVE_DISPLACED = 1u, RHH_OWNED_MOVE_DISPLACED_UNREADABLE = 2u,
    RHH_OWNED_MOVE_GROUP = 4u, RHH_OWNED_MOVE_BEFORE_UNREADABLE = 8u, RHH_OWNED_MOVE_AFTER_UNREADABLE = 16u };
/* A committed PC placement, including a shift that leaves another Pokémon
 * held. Source identifies where the held Pokémon was picked up even if that
 * slot was reused. Group placements report each member after it is placed. */
struct RhhOwnedMove {
    uint32_t struct_size, flags, source_location, source_box, source_slot;
    uint32_t destination_location, destination_box, destination_slot;
    uint64_t run_id, sequence;
    struct RhhOwnedMonSnapshot before, after, displaced;
};
enum RhhOwnedPickupFlags { RHH_OWNED_PICKUP_GROUP = 1u, RHH_OWNED_PICKUP_VALID = 2u };
/* A PC slot was cleared into the storage cursor. For a group, each occupied
 * source emits a receipt as it is cleared. Invalid Pokémon retain coordinates
 * but have no readable snapshot. Shifts use the displaced field of a move. */
struct RhhOwnedPickup {
    uint32_t struct_size, flags, source_location, source_box, source_slot, reserved;
    uint64_t run_id, sequence;
    struct RhhOwnedMonSnapshot mon;
};
enum RhhOwnedStorageUnreadableCause { RHH_OWNED_STORAGE_UNREADABLE_MOVE = 1, RHH_OWNED_STORAGE_UNREADABLE_RELEASE };
/* A placement or release for which the older event cannot supply every
 * required snapshot. Only the member selected by cause is meaningful. */
struct RhhOwnedStorageUnreadable {
    uint32_t struct_size, cause;
    uint64_t run_id, sequence;
    struct RhhOwnedMove move;
    struct RhhOwnedRelease release;
};
#define RHH_PARTY_REORDER_CAPACITY 6u
enum RhhPartyReorderCause { RHH_PARTY_REORDER_SWAP = 1, RHH_PARTY_REORDER_COMPACT };
enum RhhPartyRelocationFlags { RHH_PARTY_BEFORE_OCCUPIED = 1u, RHH_PARTY_BEFORE_VALID = 2u,
    RHH_PARTY_AFTER_OCCUPIED = 4u, RHH_PARTY_AFTER_VALID = 8u };
struct RhhPartyRelocation {
    uint32_t source_slot, destination_slot, flags, reserved;
    struct RhhOwnedMonSnapshot before, after;
};
/* One simultaneous party permutation. Only moved slots are listed, and an
 * unreadable occupied slot retains its indices without a valid snapshot. */
struct RhhPartyReorder {
    uint32_t struct_size, cause, count, reserved;
    uint64_t run_id, sequence;
    struct RhhPartyRelocation moves[RHH_PARTY_REORDER_CAPACITY];
};
enum RhhDaycareTransferCause { RHH_DAYCARE_DEPOSIT = 1, RHH_DAYCARE_WITHDRAW, RHH_DAYCARE_SHIFT };
enum RhhDaycareTransferFlags { RHH_DAYCARE_TRANSFER_BEFORE_VALID = 1u, RHH_DAYCARE_TRANSFER_AFTER_VALID = 2u };
/* Committed move before any subsequent party compaction or daycare shift.
 * Slot coordinates remain available even when a snapshot is unreadable. */
struct RhhDaycareTransfer {
    uint32_t struct_size, cause, flags, source_location, source_box, source_slot;
    uint32_t destination_location, destination_box, destination_slot, reserved;
    uint64_t run_id, sequence;
    struct RhhOwnedMonSnapshot before, after;
};
enum RhhFusionTransferCause { RHH_FUSION_STORE = 1, RHH_FUSION_RESTORE };
enum RhhFusionTransferFlags { RHH_FUSION_BEFORE_VALID = 1u, RHH_FUSION_AFTER_VALID = 2u,
    RHH_FUSION_PRIMARY_BEFORE_VALID = 4u, RHH_FUSION_PRIMARY_AFTER_VALID = 8u };
/* Captured before the moved Pokémon changes slots and reported after the
 * primary form changes, but before any following party compaction. */
struct RhhFusionTransfer {
    uint32_t struct_size, cause, flags, primary_slot, source_location, source_slot;
    uint32_t destination_location, destination_box, destination_slot, reserved;
    uint64_t run_id, sequence;
    struct RhhOwnedMonSnapshot before, after, primary_before, primary_after;
};
enum RhhCatchPlacementCause { RHH_CATCH_DISPLACED_PARTY = 1, RHH_CATCH_ACQUIRED };
enum RhhCatchPlacementFlags { RHH_CATCH_BEFORE_VALID = 1u, RHH_CATCH_AFTER_VALID = 2u };
/* A completed party-to-PC displacement or newly caught Pokémon placement.
 * ACQUIRED has a STAGED source and may change OT identity on placement. */
struct RhhCatchPlacement {
    uint32_t struct_size, cause, flags, source_location, source_slot;
    uint32_t destination_location, destination_box, destination_slot;
    uint64_t run_id, sequence;
    struct RhhOwnedMonSnapshot before, after;
};
enum RhhGiftCause { RHH_GIFT_SCRIPTED = 1, RHH_GIFT_OWNER_ASSIGNED, RHH_GIFT_DEBUG, RHH_GIFT_MYSTERY_EVENT };
enum RhhGiftFlags { RHH_GIFT_BEFORE_VALID = 1u, RHH_GIFT_AFTER_VALID = 2u,
    RHH_GIFT_REPLACED = 4u, RHH_GIFT_REPLACED_VALID = 8u };
/* A successful scripted acquisition, including an explicitly overwritten
 * party slot. OWNER_ASSIGNED includes script eggs and may change OT identity.
 * DEBUG and MYSTERY_EVENT receipts have separate events and sequences. */
struct RhhOwnedGift {
    uint32_t struct_size, cause, flags, destination_location, destination_box, destination_slot;
    uint64_t run_id, sequence;
    struct RhhOwnedMonSnapshot before, after, replaced;
};
enum RhhEvolutionCause { RHH_EVOLUTION_CHANGED = 1, RHH_EVOLUTION_SPLIT };
enum RhhEvolutionFlags { RHH_EVOLUTION_BEFORE_VALID = 1u, RHH_EVOLUTION_AFTER_VALID = 2u,
    RHH_EVOLUTION_REPLACED = 4u, RHH_EVOLUTION_REPLACED_VALID = 8u,
    RHH_EVOLUTION_TRADE = 16u, RHH_EVOLUTION_LINK_TRADE = 32u, RHH_EVOLUTION_PC_PENDING = 64u };
/* A committed species change or split. PC_PENDING means the changed Pokémon
 * is still staged and the PC destination has yet to be synchronized. */
struct RhhOwnedEvolution {
    uint32_t struct_size, cause, flags, source_location, source_slot;
    uint32_t destination_location, destination_box, destination_slot;
    uint64_t run_id, sequence;
    struct RhhOwnedMonSnapshot before, after, replaced;
};
enum RhhIngameTradeFlags { RHH_TRADE_SENT_VALID = 1u, RHH_TRADE_RECEIVED_BEFORE_VALID = 2u,
    RHH_TRADE_RECEIVED_AFTER_VALID = 4u };
/* An in-memory in-game exchange, before any subsequent trade evolution. */
struct RhhIngameTrade {
    uint32_t struct_size, flags, source_location, source_box, source_slot;
    uint32_t destination_location, destination_box, destination_slot;
    uint64_t run_id, sequence;
    struct RhhOwnedMonSnapshot sent, received_before, received_after;
};
enum RhhLinkTradeFlags { RHH_LINK_TRADE_SENT_VALID = 1u, RHH_LINK_TRADE_RECEIVED_BEFORE_VALID = 2u,
    RHH_LINK_TRADE_RECEIVED_AFTER_VALID = 4u };
enum RhhLinkTradeTransport { RHH_LINK_TRADE_CABLE = 1, RHH_LINK_TRADE_WIRELESS };
struct RhhLinkTrade {
    uint32_t struct_size, flags, party_slot, transport;
    uint64_t run_id, sequence;
    struct RhhOwnedMonSnapshot sent, received_before, received_after;
};
#define RHH_LINK_TRADE_HISTORY_CAPACITY 64u
struct RhhSavedLinkTrade {
    uint32_t struct_size, reserved;
    uint64_t serial;
    struct RhhLinkTrade trade;
};
enum RhhLinkHistoryStatus { RHH_LINK_HISTORY_UP_TO_DATE = 0, RHH_LINK_HISTORY_READY,
    RHH_LINK_HISTORY_GAP, RHH_LINK_HISTORY_DIVERGED, RHH_LINK_HISTORY_AHEAD, RHH_LINK_HISTORY_WRONG_RUN };
struct RhhLinkHistoryCheckpoint {
    uint32_t struct_size, reserved;
    uint64_t run_id, serial;
    uint8_t digest[32];
};
struct RhhLinkHistoryReconcile {
    uint32_t struct_size, status, next_index, count;
    uint64_t oldest_serial, newest_serial;
};
enum RhhOwnedTradeState { RHH_OWNED_TRADE_IDLE = 0, RHH_OWNED_TRADE_PC_HELD, RHH_OWNED_TRADE_LINK_ACTIVE };
#define RHH_ABILITY_SPECIES_SLOTS 3u
enum RhhAbilityFlags {
    RHH_ABILITY_CANNOT_COPY = 1u, RHH_ABILITY_CANNOT_SWAP = 2u,
    RHH_ABILITY_CANNOT_TRACE = 4u, RHH_ABILITY_CANNOT_SUPPRESS = 8u,
    RHH_ABILITY_CANNOT_OVERWRITE = 16u, RHH_ABILITY_BREAKABLE = 32u,
    RHH_ABILITY_FAILS_ON_IMPOSTER = 64u
};
#define RHH_ABILITY_FLAGS_ALL 127u
#define RHH_CUSTOM_ABILITY_LIMIT 256u
#define RHH_MOD_ABILITIES_PER_MOD 32u
/* Declarations and strings are borrowed until module unload. Custom abilities
 * have stable keys but no engine ability slot; mods supply their behavior. */
struct RhhModAbility {
    uint32_t struct_size, flags, reserved[2];
    const char *id, *label, *description;
};
struct RhhCustomAbilityInfo {
    uint32_t struct_size, flags;
    RhhContentKey ability;
    char id[RHH_MOD_ID_MAX], label[33], description[129];
};
struct RhhAbilityInfo {
    uint32_t struct_size, flags;
    RhhContentKey ability;
};
struct RhhSpeciesAbilities {
    uint32_t struct_size, slot_count, normal_slot_count, reserved;
    RhhContentKey species, slots[RHH_ABILITY_SPECIES_SLOTS];
};
struct RhhOwnedAbility {
    uint32_t struct_size, selected_slot;
    RhhContentKey ability;
    struct RhhOwnedMonSnapshot mon;
};
struct RhhBattlerAbility {
    uint32_t struct_size, battler, flags, reserved;
    uint64_t battle_id;
    RhhContentKey stored, active, active_ignoring_mold_breaker;
};
enum RhhBattlerAbilityFlags { RHH_BATTLER_ABILITY_OVERWRITTEN = 1u };
/* A coalesced frame observation, not an ability-change transaction. The
 * active fields ignore move-specific Mold Breaker bypass. Switches establish
 * a fresh baseline; changes between samples may not be observed. */
struct RhhBattlerAbilityObservation {
    uint32_t struct_size, reserved;
    uint64_t sequence;
    struct RhhBattleParticipant participant;
    struct RhhBattlerAbility before, after;
};
enum RhhBattlerAbilityMutationCause { RHH_ABILITY_MUTATION_COPY = 1, RHH_ABILITY_MUTATION_SWAP,
    RHH_ABILITY_MUTATION_SUPPRESS, RHH_ABILITY_MUTATION_OVERWRITE, RHH_ABILITY_MUTATION_ENTRAINMENT,
    RHH_ABILITY_MUTATION_TRACE, RHH_ABILITY_MUTATION_CONTACT_COPY, RHH_ABILITY_MUTATION_CONTACT_SWAP };
/* A committed raw ability or Gastro Acid change from the supported engine
 * script commands. The two subjects of a swap share batch and are captured
 * before either notification. Active values ignore move-specific Mold Breaker.
 * No-op writes do not notify. The event requires an identified, present
 * local-battle participant; pending switches, link and recorded battles are
 * excluded. Other mutation paths and suppression changes on other battlers
 * are not included; the frame observation remains separate. Both gameplay
 * RNG streams are restored after observers. */
struct RhhBattlerAbilityMutation {
    uint32_t struct_size, cause, source_battler, batch_index, batch_size, before_gastro_acid, after_gastro_acid, reserved;
    uint64_t run_id, battle_id, sequence, batch;
    RhhContentKey move, before_overwritten, after_overwritten;
    struct RhhBattleParticipant participant;
    struct RhhBattlerAbility before, after;
};
/* Contact-triggered changes use their own observer event, sequence and batch
 * IDs so existing script-mutation listeners retain their original stream.
 * The triggering ability is captured before the mutation; the source is its
 * battler and move is the engine's contact move. The same participant and
 * local-battle restrictions as script-mutation notifications apply. */
struct RhhBattlerAbilityContactMutation {
    uint32_t struct_size, reserved;
    RhhContentKey triggering_ability;
    struct RhhBattlerAbilityMutation change;
};
#define RHH_INNATE_LIMIT 3u
enum RhhInnateOfferResult { RHH_INNATE_OK, RHH_INNATE_INVALID, RHH_INNATE_FULL,
    RHH_INNATE_UNAVAILABLE, RHH_INNATE_CONFLICT };
struct RhhInnateOffer {
    uint32_t struct_size, unlock_level, reserved[2];
    RhhContentKey ability;
};
struct RhhInnateEntry {
    RhhContentKey ability;
    uint32_t unlock_level, unlocked;
};
struct RhhInnateList {
    uint32_t struct_size, level, count, reserved;
    RhhContentKey species;
    struct RhhInnateEntry entries[RHH_INNATE_LIMIT];
};
/* Current owned-mon proposal. The list is the current composition and may
 * change as constraint callbacks offer or remove entries. */
struct RhhInnatePersonalContext {
    uint32_t struct_size, location, box, slot;
    uint64_t identity;
    RhhContentKey species;
    uint32_t level, reserved;
    struct RhhInnateList list;
};
/* RHH_INNATE_BATTLE_PERSONALIZE: event key is species, subject is trainer,
 * ordinal is battle_id, and args are battler, owner, party_slot and level.
 * Trainer is zero when no stable authored opponent trainer is available. Slot
 * and battle_id are not durable Pokémon identities. Hooks must be pure and
 * use the same rule for battle and AI; AI has no separate discovery model.
 * Denial or gameplay RNG use discards this phase and restores both streams. */
struct RhhInnateBattleContext {
    uint32_t struct_size, battler, owner, party_slot, level, reserved;
    uint64_t battle_id;
    RhhContentKey species, trainer;
    struct RhhInnateList list;
};
enum RhhInnateResolutionFlags { RHH_INNATE_AI_KNOWLEDGE = 1u };
enum RhhInnateStatus {
    RHH_INNATE_LOCKED = 1u, RHH_INNATE_DUPLICATE_PRIMARY = 2u,
    RHH_INNATE_GASTRO_ACID = 4u, RHH_INNATE_NEUTRALIZING_GAS = 8u,
    RHH_INNATE_MOVE_BYPASSED = 16u, RHH_INNATE_NOT_ON_FIELD = 32u
};
enum RhhInnatePowerRole { RHH_INNATE_POWER_ATTACKER, RHH_INNATE_POWER_DEFENDER };
enum RhhInnateAccuracyRole { RHH_INNATE_ACCURACY_ATTACKER = RHH_INNATE_POWER_ATTACKER,
    RHH_INNATE_ACCURACY_DEFENDER = RHH_INNATE_POWER_DEFENDER };
struct RhhInnateResolvedEntry {
    RhhContentKey ability;
    uint32_t unlock_level, status;
};
struct RhhInnateResolution {
    uint32_t struct_size, battler, count, flags;
    uint64_t battle_id;
    RhhContentKey species;
    struct RhhInnateResolvedEntry entries[RHH_INNATE_LIMIT];
};
enum RhhGasChangeCause { RHH_GAS_ACTIVATED = 1, RHH_GAS_SUPPRESSED, RHH_GAS_FAINTED,
    RHH_GAS_SWITCHED_OUT, RHH_GAS_REPLACED };
enum RhhGasSlotFlags { RHH_GAS_PARTICIPANT_AVAILABLE = 1u, RHH_GAS_ABILITY_AVAILABLE = 2u,
    RHH_GAS_INNATES_AVAILABLE = 4u };
struct RhhGasSlot {
    uint32_t battler, flags;
    struct RhhBattleParticipant participant;
    struct RhhBattlerAbility ability;
    struct RhhInnateResolution innates;
};
/* The source mask follows the engine's active gas flags, including sources
 * whose participant cannot currently be sampled. Per-slot flags indicate
 * which copied views are available; ability values ignore Mold Breaker. */
struct RhhGasSnapshot {
    uint32_t struct_size, source_mask, battler_count, reserved;
    uint64_t battle_id;
    struct RhhGasSlot slots[RHH_BATTLE_MAX_PARTICIPANTS];
};
/* Committed changes at supported engine gas activation, Gastro Acid,
 * switch-out, faint and replacement boundaries. The event is sent when the
 * effective source mask changes, even if another source keeps gas active.
 * Before/after views are independent point-in-time samples: missing slots are
 * flagged, identity may collide, and innate views use the normal pure rules.
 * Switch-in cleanup can report a stale source flag after the slot has already
 * been loaded with its replacement; source_battler identifies the slot, not
 * necessarily the departing participant.
 * Link, recorded and unavailable battles are excluded. Observers cannot
 * issue gameplay commands, and both gameplay RNG streams are restored. */
struct RhhGasChange {
    uint32_t struct_size, cause, source_battler, reserved;
    uint64_t run_id, battle_id, sequence;
    RhhContentKey move;
    struct RhhGasSnapshot before, after;
};
/* A coalesced frame observation of the current resolved innate list. This is
 * not a committed transition and does not identify the cause of a change. */
struct RhhBattlerInnateObservation {
    uint32_t struct_size, reserved;
    uint64_t sequence;
    struct RhhBattleParticipant participant;
    struct RhhInnateResolution before, after;
};
/* Pure power trigger for one usable entry. event_key is the ability, subject
 * is the move, ordinal is the entry index, and args are current power, role,
 * attacker and defender. The move context contains the original input power. */
struct RhhInnatePowerContext {
    uint32_t struct_size, role, index, current_power;
    RhhContentKey ability;
    struct RhhMovePowerContext move;
    struct RhhInnateResolution resolution;
};
/* Per-usable-innate damage-stat proposal, attacker then defender; subject is
 * move, ordinal is list index, args are current value, role, phase and owner. */
struct RhhInnateDamageStatContext {
    uint32_t struct_size, role, index, current_value;
    RhhContentKey ability;
    struct RhhDamageStatContext stat;
    struct RhhInnateResolution resolution;
};
/* RHH_INNATE_CRITICAL_STAGE visits usable attacker then defender innates;
 * args are current value, role, attacker and defender. It shares the critical
 * context's bounds and purity rules, applying each accepted proposal in order. */
struct RhhInnateCriticalStageContext {
    uint32_t struct_size, role, index, current_value;
    RhhContentKey ability;
    struct RhhCriticalStageContext critical;
    struct RhhInnateResolution resolution;
};
/* Usable attacker then defender innates see the current proposal and the
 * original final-damage context. Each bounded, pure proposal composes in
 * order; rejected entries do not cancel later entries. */
struct RhhInnateFinalDamageContext {
    uint32_t struct_size, role, index, current_damage;
    RhhContentKey ability;
    struct RhhFinalDamageContext damage;
    struct RhhInnateResolution resolution;
};
/* Accuracy event fields match the innate power event, with current accuracy
 * as args[0]. The copied move context contains the original engine input. */
struct RhhInnateAccuracyContext {
    uint32_t struct_size, role, index, current_accuracy;
    RhhContentKey ability;
    struct RhhMoveAccuracyContext move;
    struct RhhInnateResolution resolution;
};
/* Pure priority trigger for one usable innate on the move user. The event key
 * is the ability, subject is the effective move, ordinal is the entry index,
 * and args are the current encoded priority, battler, AI flags and zero. */
struct RhhInnatePriorityContext {
    uint32_t struct_size, index;
    int32_t current_priority;
    uint32_t reserved;
    RhhContentKey ability;
    struct RhhMoveBattlerPriority move;
    struct RhhInnateResolution resolution;
};
/* Pure per-usable-innate speed proposal. event_key is the innate, subject
 * is species, ordinal is the entry index; args are current speed, battler,
 * flags and zero. */
struct RhhInnateSpeedContext {
    uint32_t struct_size, index, current_speed, reserved;
    RhhContentKey ability;
    struct RhhBattleSpeedContext speed;
    struct RhhInnateResolution resolution;
};
/* Committed switch-in phase, after hazards and ordinary switch-in abilities.
 * event_key is the innate, subject is the participant's OT/personality
 * identity, ordinal is the list index; args are battler, cause, turn and the
 * corresponding BATTLER_ENTERED sequence. */
struct RhhInnateEntryContext {
    uint32_t struct_size, battler, cause, index;
    uint64_t battle_id, entry_sequence;
    RhhContentKey ability;
    struct RhhBattleParticipant participant;
    struct RhhInnateResolution resolution;
};
/* Completed turn, after engine end-turn effects and condition timers tick,
 * before RHH_BATTLE_TURN_ENDED. event_key is the innate, subject is the
 * participant identity, ordinal is its list index; args are battler, turn,
 * low 32 bits of turn sequence and list index. Identity is OT/personality
 * and may collide. */
struct RhhInnateTurnContext {
    uint32_t struct_size, battler, index, turn;
    uint64_t battle_id, turn_sequence;
    RhhContentKey ability;
    struct RhhBattleParticipant participant;
    struct RhhInnateResolution resolution;
};
/* Pure smart-switch preview for one usable innate of the AI's own candidate.
 * The HP is the engine's estimate after entry hazards and any pending switch
 * heal. event_key is the innate, ordinal its list index, and args contain
 * battler, party slot, estimated HP and maximum HP. This is a prediction, not
 * an entry event; it does not run during opponent-switch prediction. The
 * constant turn-healing preview uses HP after entry healing; the per-turn
 * preview uses HP immediately before the predicted turn healing. */
struct RhhAiInnateEntryContext {
    uint32_t struct_size, battler, party_slot, index, hp, max_hp, offered, capacity;
    uint64_t battle_id;
    RhhContentKey ability;
    struct RhhInnateResolution resolution;
};
/* The stat preview has its own event so existing healing-only consumers do
 * not receive new callbacks. Stages are the AI's simulated pre-innate values
 * from -6 to +6. The entry data has the same meaning as the healing preview. */
struct RhhAiInnateStatContext {
    uint32_t struct_size, reserved;
    struct RhhAiInnateEntryContext entry;
    int8_t stages[RHH_BATTLE_STAT_COUNT];
};
/* Cached AI knowledge of the single opponent during an entry-stat preview. */
struct RhhAiInnateOpponentStatContext {
    uint32_t struct_size, battler, reserved[2];
    RhhContentKey known_ability, known_item;
    int8_t stages[RHH_BATTLE_STAT_COUNT];
};
/* The opponent's visible major status and cached AI knowledge during the
 * entry-stat preview. status is zero or an RHH_EFFECT_* major status. */
struct RhhAiInnateOpponentStatusContext {
    uint32_t struct_size, battler, status, reserved;
    RhhContentKey known_ability, known_item;
};
enum RhhAiInnateFieldFlags { RHH_AI_INNATE_FIELD_WEATHER_BLOCKED = 1u,
    RHH_AI_INNATE_FIELD_TERRAIN_BLOCKED = 2u, RHH_AI_INNATE_FIELD_WEATHER_SUPPRESSED = 4u };
/* Per-innate, pure switch-forecast offer context. Field values are the
 * predicted underlying values after normal switch-in ability rules but before
 * any innate field offers. Weather suppression uses cached AI knowledge.
 * event_key is the innate, subject is species, ordinal is the entry index,
 * and args are battler, party slot, estimated HP and maximum HP. */
struct RhhAiInnateFieldContext {
    uint32_t struct_size, weather, terrain, weather_turns, terrain_turns, flags;
    struct RhhAiInnateEntryContext entry;
};
#define RHH_AI_INNATE_ENTRY_FIELD_LIMIT 8u
#define RHH_AI_INNATE_ENTRY_HEAL_LIMIT 8u
#define RHH_AI_INNATE_ENTRY_STAT_LIMIT 8u
/* "rhh:service/abilities", version 1.40. Species slots are alternative
 * choices, not simultaneous effects. The battler view contains actual battle
 * state and is unavailable inside AI and other pure calculation callbacks.
 * Innate entries are descriptions; their battle behavior is provided by mods. */
struct RhhAbilityService {
    struct RhhServiceHeader header;
    int (*info)(RhhContentKey ability, struct RhhAbilityInfo *out);
    int (*species)(RhhContentKey species, struct RhhSpeciesAbilities *out);
    int (*owned)(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedAbility *out);
    int (*battler)(uint32_t battler, struct RhhBattlerAbility *out);
    /* v1.1: query a species' level-aware list or offer entries in the pure
     * RHH_INNATE_PROPOSE constraint phase. Nested queries fail. */
    int (*species_innates)(RhhContentKey species, uint32_t level, struct RhhInnateList *out);
    int (*innate_context)(struct RhhInnateList *out);
    int (*offer_innate)(const struct RhhInnateOffer *offer);
    /* v1.2: a status of zero denotes a currently usable entry. The power
     * query uses the current real or AI calculation's own knowledge. */
    int (*battler_innates)(uint32_t battler, struct RhhInnateResolution *out);
    int (*power_innates)(uint32_t role, struct RhhInnateResolution *out);
    /* v1.3: copied context only during RHH_INNATE_POWER. */
    int (*innate_power_context)(struct RhhInnatePowerContext *out);
    /* v1.4: custom declarations in stable ID order. */
    uint32_t (*custom_count)(void);
    int (*custom_at)(uint32_t index, struct RhhCustomAbilityInfo *out);
    int (*custom_info)(RhhContentKey ability, struct RhhCustomAbilityInfo *out);
    /* v1.5: accuracy-included view during RHH_MOVE_ACCURACY, and the copied
     * trigger context during RHH_INNATE_ACCURACY. */
    int (*accuracy_innates)(uint32_t role, struct RhhInnateResolution *out);
    int (*innate_accuracy_context)(struct RhhInnateAccuracyContext *out);
    /* v1.6: copied context only during a committed RHH_INNATE_ENTRY. */
    int (*innate_entry_context)(struct RhhInnateEntryContext *out);
    /* v1.7: queue a copied battle message during an innate entry observer.
     * Messages are shown in observer order before switch-in processing resumes.
     * Up to eight messages per entrant share the normal battle text limits. */
    int (*innate_entry_message)(const char *text);
    /* v1.8: queue healing of the entering battler by numerator/denominator
     * of its non-Dynamax maximum HP. Both values must be 1..16 and numerator
     * may not exceed denominator. At most eight heals per entrant; heals run
     * in offer order before entry messages and respect Heal Block. */
    int (*innate_entry_heal)(uint32_t numerator, uint32_t denominator);
    /* v1.9: queue a stat stage change for the entering battler. stat uses
     * RHH_STAT_ATTACK through RHH_BATTLE_EVASION; stages is -6..-1 or 1..6.
     * Up to eight changes per entrant run after heals and before messages. */
    int (*innate_entry_stat)(uint32_t stat, int32_t stages);
    /* v1.10: copy a built-in or custom ability's display name as UI-compatible
     * ASCII, including its NUL. Failure leaves the destination unchanged. */
    int (*name)(RhhContentKey ability, char *out, uint32_t capacity);
    /* v1.11: a fresh uniquely identifiable owned Pokémon's composed list;
     * context and remove are scoped to RHH_INNATE_PERSONALIZE. offer_innate
     * can also add entries during that event. */
    int (*owned_innates)(const struct RhhOwnedMonSnapshot *expected, struct RhhInnateList *out);
    int (*personal_context)(struct RhhInnatePersonalContext *out);
    int (*remove_innate)(RhhContentKey ability);
    /* v1.12: available only to an INNATE_TURN_END observer. Messages share
     * the 32-message end-of-turn queue with condition expiry notifications. */
    int (*innate_turn_context)(struct RhhInnateTurnContext *out);
    int (*innate_turn_message)(const char *text);
    /* v1.13: queue a heal or stat change for this innate's battler. Up to 32
     * actions across the entire turn execute in offer order through normal
     * battle scripts before the turn-ended notification and queued messages.
     * Heals accept fractions 1..16 and respect Heal Block; stats accept
     * RHH_STAT_ATTACK..RHH_BATTLE_EVASION and nonzero stages from -6 to 6. */
    int (*innate_turn_heal)(uint32_t numerator, uint32_t denominator);
    int (*innate_turn_stat)(uint32_t stat, int32_t stages);
    /* v1.14: pure preview context and a deterministic self-heal offer during
     * RHH_AI_INNATE_ENTRY_PREVIEW constraints. Offers are explicitly used by
     * the smart switcher's survival estimate, not executed in battle. At most
     * eight offers per candidate; fractions obey the entry-heal bounds. */
    int (*ai_innate_entry_context)(struct RhhAiInnateEntryContext *out);
    int (*offer_ai_innate_entry_heal)(uint32_t numerator, uint32_t denominator);
    /* v1.15: a separate pure per-innate stat preview. Up to eight self-stat
     * offers per candidate use the same valid stats and stages as entry_stat. */
    int (*ai_innate_entry_stat_context)(struct RhhAiInnateStatContext *out);
    int (*offer_ai_innate_entry_stat)(uint32_t stat, int32_t stages);
    /* v1.16: queue the same healing or stat action for a present, living
     * battler. Both the innate's source and recipient must still match when
     * the action executes. These share the existing turn action limit. */
    int (*innate_turn_heal_target)(uint32_t battler, uint32_t numerator, uint32_t denominator);
    int (*innate_turn_stat_target)(uint32_t battler, uint32_t stat, int32_t stages);
    /* v1.17: queue healing or a stat change for a present, living battler
     * during an innate entry observer. Both source and recipient are checked
     * again at execution; these share the existing entry action limits. */
    int (*innate_entry_heal_target)(uint32_t battler, uint32_t numerator, uint32_t denominator);
    int (*innate_entry_stat_target)(uint32_t battler, uint32_t stat, int32_t stages);
    /* v1.18: source-attributed ability stat changes. These share the entry
     * or turn queues and use the engine's ability stat-change rules. */
    int (*innate_entry_stat_from)(uint32_t battler, uint32_t stat, int32_t stages);
    int (*innate_turn_stat_from)(uint32_t battler, uint32_t stat, int32_t stages);
    /* v1.19: inspect and offer an opponent-directed entry-stat prediction
     * during the existing pure AI stat-preview constraint. */
    int (*ai_innate_entry_opponent_context)(struct RhhAiInnateOpponentStatContext *out);
    int (*offer_ai_innate_entry_opponent_stat)(uint32_t stat, int32_t stages);
    /* v1.20: opt-in, constant self-healing each completed turn in the smart
     * switcher's survival estimate. Uses RhhAiInnateEntryContext for the
     * candidate's post-entry starting HP; offers must remain valid on every
     * simulated turn and must not depend on future HP, turn or RNG. */
    int (*ai_innate_turn_heal_context)(struct RhhAiInnateEntryContext *out);
    int (*offer_ai_innate_turn_heal)(uint32_t numerator, uint32_t denominator);
    /* v1.21: per-turn self-healing preview, with context.hp after the
     * estimated attack and other turn effects. Each callback must be pure and
     * yield the same offers for the same HP and otherwise unchanged context;
     * do not depend on simulated turn count, hidden state or RNG. */
    int (*ai_innate_turn_step_context)(struct RhhAiInnateEntryContext *out);
    int (*offer_ai_innate_turn_step_heal)(uint32_t numerator, uint32_t denominator);
    /* v1.22: copied only in RHH_INNATE_BATTLE_PERSONALIZE constraints.
     * offer_innate also works there; a denial discards this phase's changes. */
    int (*battle_innate_context)(struct RhhInnateBattleContext *out);
    int (*remove_battle_innate)(RhhContentKey ability);
    /* v1.23: copied only during RHH_BATTLER_ABILITY_OBSERVED. */
    int (*ability_observation_context)(struct RhhBattlerAbilityObservation *out);
    /* v1.24: copied only during RHH_BATTLER_INNATES_OBSERVED. */
    int (*innate_observation_context)(struct RhhBattlerInnateObservation *out);
    /* v1.25: queue a major status on a living battler during the innate
     * turn-end observer. Accepts RHH_EFFECT_SLEEP through RHH_EFFECT_TOXIC;
     * shares the turn action limit and revalidates source and recipient. */
    int (*innate_turn_status_target)(uint32_t battler, uint32_t status);
    /* v1.26: queue a major status during an innate entry observer. Executes
     * after entry healing and stat actions, before entry animations/messages. */
    int (*innate_entry_status_target)(uint32_t battler, uint32_t status);
    /* v1.27: describe an opponent-directed entry status during the existing
     * AI entry-stat preview. Shares its eight-offer limit and denial rules. */
    int (*ai_innate_entry_opponent_status_context)(struct RhhAiInnateOpponentStatusContext *out);
    int (*offer_ai_innate_entry_opponent_status)(uint32_t status);
    /* v1.28: copied during RHH_BATTLER_ABILITY_MUTATED; v1.29 also supports
     * RHH_BATTLER_ABILITY_CONTACT_MUTATED. */
    int (*ability_mutation_context)(struct RhhBattlerAbilityMutation *out);
    /* v1.29: copied only during RHH_BATTLER_ABILITY_CONTACT_MUTATED. */
    int (*ability_contact_mutation_context)(struct RhhBattlerAbilityContactMutation *out);
    /* v1.30: copied only during RHH_BATTLE_NEUTRALIZING_GAS_CHANGED. */
    int (*gas_change_context)(struct RhhGasChange *out);
    /* v1.31: the move user's resolved innates during a battler-priority
     * proposal, and the current per-innate priority trigger. */
    int (*priority_innates)(struct RhhInnateResolution *out);
    int (*innate_priority_context)(struct RhhInnatePriorityContext *out);
    /* v1.32: resolution during the speed proposal and per-innate context. */
    int (*speed_innates)(struct RhhInnateResolution *out);
    int (*innate_speed_context)(struct RhhInnateSpeedContext *out);
    /* v1.33: role uses RHH_INNATE_POWER_ATTACKER / DEFENDER. */
    int (*damage_stat_innates)(uint32_t role, struct RhhInnateResolution *out);
    int (*innate_damage_stat_context)(struct RhhInnateDamageStatContext *out);
    /* v1.34: queue supported weather/terrain on entry or at turn end.
     * Weather accepts RHH_WEATHER_RAIN through RHH_WEATHER_FOG; terrain
     * accepts the four nonzero RHH_TERRAIN_* values. */
    int (*innate_entry_weather)(uint32_t weather);
    int (*innate_entry_terrain)(uint32_t terrain);
    int (*innate_turn_weather)(uint32_t weather);
    int (*innate_turn_terrain)(uint32_t terrain);
    /* v1.35: pure switch-forecast field offers, not committed actions. */
    int (*ai_innate_entry_field_context)(struct RhhAiInnateFieldContext *out);
    int (*offer_ai_innate_entry_weather)(uint32_t weather);
    int (*offer_ai_innate_entry_terrain)(uint32_t terrain);
    /* v1.36: project all offers so far, including this callback's tentative
     * offers. They may still be discarded if the callback is denied. */
    int (*ai_innate_entry_field_projected)(struct RhhAiInnateFieldContext *out);
    /* v1.37: role uses RHH_INNATE_POWER_ATTACKER / DEFENDER. */
    int (*critical_stage_innates)(uint32_t role, struct RhhInnateResolution *out);
    int (*innate_critical_stage_context)(struct RhhInnateCriticalStageContext *out);
    /* v1.38: role uses RHH_INNATE_POWER_ATTACKER / DEFENDER. */
    /* v1.38: ordinary damage events; v1.39 adds self-damage events;
     * v1.40 includes non-Z Gen V+ Beat Up AI hits. */
    int (*final_damage_innates)(uint32_t role, struct RhhInnateResolution *out);
    int (*innate_final_damage_context)(struct RhhInnateFinalDamageContext *out);
};
#define RHH_ABILITY_SERVICE_V1_0_SIZE offsetof(struct RhhAbilityService, species_innates)
#define RHH_ABILITY_SERVICE_V1_1_SIZE offsetof(struct RhhAbilityService, battler_innates)
#define RHH_ABILITY_SERVICE_V1_2_SIZE offsetof(struct RhhAbilityService, innate_power_context)
#define RHH_ABILITY_SERVICE_V1_3_SIZE offsetof(struct RhhAbilityService, custom_count)
#define RHH_ABILITY_SERVICE_V1_4_SIZE offsetof(struct RhhAbilityService, accuracy_innates)
#define RHH_ABILITY_SERVICE_V1_5_SIZE offsetof(struct RhhAbilityService, innate_entry_context)
#define RHH_ABILITY_SERVICE_V1_6_SIZE offsetof(struct RhhAbilityService, innate_entry_message)
#define RHH_ABILITY_SERVICE_V1_7_SIZE offsetof(struct RhhAbilityService, innate_entry_heal)
#define RHH_ABILITY_SERVICE_V1_8_SIZE offsetof(struct RhhAbilityService, innate_entry_stat)
#define RHH_ABILITY_SERVICE_V1_9_SIZE offsetof(struct RhhAbilityService, name)
#define RHH_ABILITY_SERVICE_V1_10_SIZE offsetof(struct RhhAbilityService, owned_innates)
#define RHH_ABILITY_SERVICE_V1_11_SIZE offsetof(struct RhhAbilityService, innate_turn_context)
#define RHH_ABILITY_SERVICE_V1_12_SIZE offsetof(struct RhhAbilityService, innate_turn_heal)
#define RHH_ABILITY_SERVICE_V1_13_SIZE offsetof(struct RhhAbilityService, ai_innate_entry_context)
#define RHH_ABILITY_SERVICE_V1_14_SIZE offsetof(struct RhhAbilityService, ai_innate_entry_stat_context)
#define RHH_ABILITY_SERVICE_V1_15_SIZE offsetof(struct RhhAbilityService, innate_turn_heal_target)
#define RHH_ABILITY_SERVICE_V1_16_SIZE offsetof(struct RhhAbilityService, innate_entry_heal_target)
#define RHH_ABILITY_SERVICE_V1_17_SIZE offsetof(struct RhhAbilityService, innate_entry_stat_from)
#define RHH_ABILITY_SERVICE_V1_18_SIZE offsetof(struct RhhAbilityService, ai_innate_entry_opponent_context)
#define RHH_ABILITY_SERVICE_V1_19_SIZE offsetof(struct RhhAbilityService, ai_innate_turn_heal_context)
#define RHH_ABILITY_SERVICE_V1_20_SIZE offsetof(struct RhhAbilityService, ai_innate_turn_step_context)
#define RHH_ABILITY_SERVICE_V1_21_SIZE offsetof(struct RhhAbilityService, battle_innate_context)
#define RHH_ABILITY_SERVICE_V1_22_SIZE offsetof(struct RhhAbilityService, ability_observation_context)
#define RHH_ABILITY_SERVICE_V1_23_SIZE offsetof(struct RhhAbilityService, innate_observation_context)
#define RHH_ABILITY_SERVICE_V1_24_SIZE offsetof(struct RhhAbilityService, innate_turn_status_target)
#define RHH_ABILITY_SERVICE_V1_25_SIZE offsetof(struct RhhAbilityService, innate_entry_status_target)
#define RHH_ABILITY_SERVICE_V1_26_SIZE offsetof(struct RhhAbilityService, ai_innate_entry_opponent_status_context)
#define RHH_ABILITY_SERVICE_V1_27_SIZE offsetof(struct RhhAbilityService, ability_mutation_context)
#define RHH_ABILITY_SERVICE_V1_28_SIZE offsetof(struct RhhAbilityService, ability_contact_mutation_context)
#define RHH_ABILITY_SERVICE_V1_29_SIZE offsetof(struct RhhAbilityService, gas_change_context)
#define RHH_ABILITY_SERVICE_V1_30_SIZE offsetof(struct RhhAbilityService, priority_innates)
#define RHH_ABILITY_SERVICE_V1_31_SIZE offsetof(struct RhhAbilityService, speed_innates)
#define RHH_ABILITY_SERVICE_V1_32_SIZE offsetof(struct RhhAbilityService, damage_stat_innates)
#define RHH_ABILITY_SERVICE_V1_33_SIZE offsetof(struct RhhAbilityService, innate_entry_weather)
#define RHH_ABILITY_SERVICE_V1_34_SIZE offsetof(struct RhhAbilityService, ai_innate_entry_field_context)
#define RHH_ABILITY_SERVICE_V1_35_SIZE offsetof(struct RhhAbilityService, ai_innate_entry_field_projected)
#define RHH_ABILITY_SERVICE_V1_36_SIZE offsetof(struct RhhAbilityService, critical_stage_innates)
#define RHH_ABILITY_SERVICE_V1_37_SIZE offsetof(struct RhhAbilityService, final_damage_innates)
#define RHH_ABILITY_SERVICE_V1_38_SIZE sizeof(struct RhhAbilityService)
#define RHH_ABILITY_SERVICE_V1_39_SIZE RHH_ABILITY_SERVICE_V1_38_SIZE
#define RHH_ABILITY_SERVICE_V1_40_SIZE RHH_ABILITY_SERVICE_V1_39_SIZE
/* Stable keys, current and maximum PP and PP Up counts. available_mask has a
 * bit for each usable definition; a retained key for a missing module has no
 * bit and a maximum PP of zero. A stale or ambiguous Pokémon fails without
 * changing the output. */
struct RhhOwnedMoveSet {
    uint32_t struct_size, available_mask;
    RhhContentKey moves[4];
    uint32_t pp[4], max_pp[4], pp_ups[4];
};
/* expected_move is zero for an empty slot. A custom move replaces exactly
 * that slot, clears its PP Ups and starts with its declared PP. Constraints
 * receive RHH_CAN_TEACH_MOVE; RHH_MOVE_TAUGHT is a committed notification. */
struct RhhMoveTeachRequest {
    uint32_t struct_size, move_slot, reserved[2];
    RhhContentKey move, expected_move;
    struct RhhOwnedMonSnapshot expected;
};
struct RhhMoveTeachResult {
    uint32_t struct_size, status, move_slot, reserved;
    RhhContentKey move, previous_move;
    struct RhhOwnedMonSnapshot mon;
    char reason[96];
};
/* The host copies the descriptor and strings for the current menu. IDs must
 * be namespaced and unique; labels are ASCII and at most 24 characters. open
 * is called read-only after selection, and fills a panel owned by this mod.
 * The snapshot and descriptor arguments are borrowed for that call only;
 * callback code and user remain valid until the menu closes or host resets. */
struct RhhUiPartyAction {
    uint32_t struct_size, reserved;
    RhhContentKey owner;
    const char *id, *label;
    int (*open)(const struct RhhOwnedMonSnapshot *mon, struct RhhUiPanel *panel, void *user);
    void *user;
};
enum RhhOwnedPartyView { RHH_OWNED_PARTY_FIELD_ORDER = 0, RHH_OWNED_PARTY_BATTLE_MENU,
    RHH_OWNED_PARTY_SKY_BATTLE, RHH_OWNED_PARTY_FOLLOWER_BATTLE, RHH_OWNED_PARTY_UNION_BATTLE,
    RHH_OWNED_PARTY_LINK_MULTI_BATTLE, RHH_OWNED_PARTY_FRONTIER, RHH_OWNED_PARTY_RENTAL,
    RHH_OWNED_PARTY_RECORDED };
enum RhhOwnedTeraSource { RHH_OWNED_TERA_PERSONALITY = 1, RHH_OWNED_TERA_STORED, RHH_OWNED_TERA_FORCED };
enum RhhOwnedTeraFlags { RHH_OWNED_TERA_UNSUPPORTED_STORED = 1u, RHH_OWNED_TERA_UNSUPPORTED_RESOLVED = 2u };
/* Stored ordinals are engine-format values, not durable custom-type keys.
 * A zero stored ordinal is the personality-derived sentinel. A forced
 * species type takes precedence over the stored value. */
struct RhhOwnedTeraView {
    uint32_t struct_size, source, flags, stored_ordinal, resolved_ordinal, reserved;
    RhhContentKey stored_type, resolved_type;
    struct RhhOwnedMonSnapshot mon;
};
/* "rhh:service/owned_mon", version 1.44. All operations copy a snapshot;
 * no borrowed engine object is exposed. Box 0 is the first PC box. */
struct RhhOwnedMonService {
    struct RhhServiceHeader header;
    uint32_t (*party_slots)(void);
    uint32_t (*box_count)(void);
    uint32_t (*box_slots)(void);
    int (*party_at)(uint32_t slot, struct RhhOwnedMonSnapshot *out);
    int (*box_at)(uint32_t box, uint32_t slot, struct RhhOwnedMonSnapshot *out);
    int (*find)(uint64_t identity, struct RhhOwnedMonSnapshot *out);
    /* v1.1: copy the current RHH_OWNED_MON_CHANGED context. */
    int (*change_context)(struct RhhOwnedMonChange *out);
    /* v1.2: count an identity across party, PC, daycare and fusion storage.
     * Unavailable while a PC storage transaction or save is in progress. */
    int (*identity_query)(uint64_t identity, struct RhhOwnedIdentityQuery *out);
    /* v1.3: enumerate regular daycare (0), FRLG Route 5 daycare (1), and
     * fusion storage. Access has the same safety restrictions as identity_query. */
    uint32_t (*daycare_count)(void);
    uint32_t (*daycare_slots)(uint32_t daycare);
    uint32_t (*fusion_slots)(void);
    int (*daycare_at)(uint32_t daycare, uint32_t slot, struct RhhOwnedMonSnapshot *out);
    int (*fusion_at)(uint32_t slot, struct RhhOwnedMonSnapshot *out);
    /* v1.4: available only during a confirmed RHH_OWNED_MON_RELEASED event. */
    int (*release_context)(struct RhhOwnedRelease *out);
    /* v1.5: available only during RHH_OWNED_MON_MOVED. */
    int (*move_context)(struct RhhOwnedMove *out);
    /* v1.6: available only during RHH_PARTY_REORDERED. */
    int (*party_reorder_context)(struct RhhPartyReorder *out);
    /* v1.7: available only during RHH_OWNED_DAYCARE_TRANSFERRED. */
    int (*daycare_transfer_context)(struct RhhDaycareTransfer *out);
    /* v1.8: available only during RHH_OWNED_FUSION_TRANSFERRED. */
    int (*fusion_transfer_context)(struct RhhFusionTransfer *out);
    /* v1.9: reports whether the engine has temporarily rearranged the party
     * for the battle menu. This does not certify that a battle owns its party.
     * v1.37 also reports the Battle Factory/Slateport rental-party view;
     * v1.38 reports the recorded-battle playback view. */
    uint32_t (*party_view)(void);
    /* v1.10: available only during RHH_OWNED_CATCH_PLACED. */
    int (*catch_placement_context)(struct RhhCatchPlacement *out);
    /* v1.11: available only during RHH_OWNED_INGAME_TRADE_COMPLETED. */
    int (*ingame_trade_context)(struct RhhIngameTrade *out);
    /* v1.12: link exchange after the game's trade save succeeds. */
    int (*link_trade_context)(struct RhhLinkTrade *out);
    uint32_t (*trade_state)(void);
    /* v1.13: bounded save-generation history, oldest first. */
    uint32_t (*link_trade_history_count)(void);
    int (*link_trade_history_at)(uint32_t index, struct RhhSavedLinkTrade *out);
    int (*link_trade_receipt_context)(struct RhhSavedLinkTrade *out);
    /* v1.14: content-linked checkpoints for rollback and retention detection. */
    int (*link_trade_checkpoint)(uint32_t index, struct RhhLinkHistoryCheckpoint *out);
    int (*link_trade_reconcile)(const struct RhhLinkHistoryCheckpoint *checkpoint,
                                struct RhhLinkHistoryReconcile *out);
    /* v1.15: available only during RHH_OWNED_MON_PICKED_UP. */
    int (*pickup_context)(struct RhhOwnedPickup *out);
    /* v1.16: available only during RHH_OWNED_STORAGE_UNREADABLE. */
    int (*storage_unreadable_context)(struct RhhOwnedStorageUnreadable *out);
    /* v1.17: available during RHH_OWNED_GIFT_RECEIVED; v1.35 also permits
     * RHH_OWNED_DEBUG_CREATED, and v1.36 RHH_OWNED_MYSTERY_GIFT_RECEIVED.
     * Each event has its own sequence. */
    int (*gift_context)(struct RhhOwnedGift *out);
    /* v1.18: available only during RHH_OWNED_EVOLUTION_COMMITTED. */
    int (*evolution_context)(struct RhhOwnedEvolution *out);
    /* v1.19: map a temporary battle-menu slot to its field slot. v1.23 also
     * covers the full-player-party menu during a reduced follower catch.
     * Fails without modifying out when the view cannot be proven player-owned. */
    int (*battle_view_field_slot)(uint32_t view_slot, uint32_t *out);
    /* v1.20: map a visible sky-battle slot to its original field slot,
     * including while the battle menu has temporarily rearranged it. */
    int (*sky_party_field_slot)(uint32_t view_slot, uint32_t *out);
    /* v1.21 maps selected follower-battle player slots to their original field
     * slots. v1.22 also maps supported mixed battle-menu slots; partner slots
     * and unsupported menu layouts remain unavailable. */
    int (*follower_party_field_slot)(uint32_t view_slot, uint32_t *out);
    /* v1.24: read current stored experience and growth thresholds for a
     * matching non-egg party, PC, daycare or fusion snapshot. Experience is
     * sampled at the call: expected does not itself contain an XP revision. */
    int (*experience)(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedExperience *out);
    /* v1.25 adds RHH_OWNED_CHANGED_EXPERIENCE to sampled change events. */
    /* v1.26: available during each non-reset RHH_OWNED_MON_CHANGED event. */
    int (*experience_change_context)(struct RhhOwnedExperienceChange *out);
    /* v1.27: calculate a fixed award after multiplier/soft-cap adjustments,
     * then apply the battle hard cap and maximum-level bound. expected's
     * stored experience must still match. Does not grant XP. */
    int (*preview_experience)(const struct RhhOwnedExperience *expected, uint32_t amount,
                              struct RhhExperiencePreview *out);
    /* v1.28: page through the current resolved learnset entries at levels
     * crossed by that same fixed award; no moves are learned or reserved. */
    int (*preview_experience_moves)(const struct RhhOwnedExperience *expected, uint32_t amount,
                                    uint32_t start, struct RhhExperienceMovePage *out);
    /* v1.29: start one overworld party experience flow, poll its latest
     * process-local result, or read the current permission/notification. */
    int (*award_experience)(const struct RhhExperienceAwardRequest *, struct RhhExperienceAwardResult *out);
    int (*experience_award_status)(uint64_t ticket, struct RhhExperienceAwardResult *out);
    int (*experience_award_context)(struct RhhExperienceAwardResult *out);
    /* v1.31: map a checked Union Room selected slot to its original field
     * slot, including supported single-battle menu permutations. */
    int (*union_party_field_slot)(uint32_t view_slot, uint32_t *out);
    /* v1.32: map a checked cable-club multi-battle player slot back to its
     * original field slot, including supported mixed-menu permutations. */
    int (*link_multi_party_field_slot)(uint32_t view_slot, uint32_t *out);
    /* v1.33: map a checked Frontier selected slot to its full-party slot. */
    int (*frontier_party_field_slot)(uint32_t view_slot, uint32_t *out);
    /* v1.39: sample field stats for a matching non-egg owned snapshot.
     * Unavailable in battle and during unsafe storage transitions. */
    int (*stats)(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedStats *out);
    /* v1.40: sample stored Hyper Training and effective IVs outside battle.
     * Also adds RHH_OWNED_CHANGED_HYPER_TRAINING to sampled changes. */
    int (*iv_view)(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedIvView *out);
    /* v1.41: sample the engine's stored and resolved Tera type outside battle;
     * expected does not contain a Tera revision. Also adds the Tera change bit. */
    int (*tera_view)(const struct RhhOwnedMonSnapshot *expected, struct RhhOwnedTeraView *out);
    /* v1.42: one bounded census with locations, including unreadable matches.
     * Shares identity_query's unavailable-state and read-only guarantees. */
    int (*identity_occurrences)(uint64_t identity, struct RhhOwnedIdentityOccurrences *out);
};
#define RHH_OWNED_MON_SERVICE_V1_0_SIZE offsetof(struct RhhOwnedMonService, change_context)
#define RHH_OWNED_MON_SERVICE_V1_1_SIZE offsetof(struct RhhOwnedMonService, identity_query)
#define RHH_OWNED_MON_SERVICE_V1_2_SIZE offsetof(struct RhhOwnedMonService, daycare_count)
#define RHH_OWNED_MON_SERVICE_V1_3_SIZE offsetof(struct RhhOwnedMonService, release_context)
#define RHH_OWNED_MON_SERVICE_V1_4_SIZE offsetof(struct RhhOwnedMonService, move_context)
#define RHH_OWNED_MON_SERVICE_V1_5_SIZE offsetof(struct RhhOwnedMonService, party_reorder_context)
#define RHH_OWNED_MON_SERVICE_V1_6_SIZE offsetof(struct RhhOwnedMonService, daycare_transfer_context)
#define RHH_OWNED_MON_SERVICE_V1_7_SIZE offsetof(struct RhhOwnedMonService, fusion_transfer_context)
#define RHH_OWNED_MON_SERVICE_V1_8_SIZE offsetof(struct RhhOwnedMonService, party_view)
#define RHH_OWNED_MON_SERVICE_V1_9_SIZE offsetof(struct RhhOwnedMonService, catch_placement_context)
#define RHH_OWNED_MON_SERVICE_V1_10_SIZE offsetof(struct RhhOwnedMonService, ingame_trade_context)
#define RHH_OWNED_MON_SERVICE_V1_11_SIZE offsetof(struct RhhOwnedMonService, link_trade_context)
#define RHH_OWNED_MON_SERVICE_V1_12_SIZE offsetof(struct RhhOwnedMonService, link_trade_history_count)
#define RHH_OWNED_MON_SERVICE_V1_13_SIZE offsetof(struct RhhOwnedMonService, link_trade_checkpoint)
#define RHH_OWNED_MON_SERVICE_V1_14_SIZE offsetof(struct RhhOwnedMonService, pickup_context)
#define RHH_OWNED_MON_SERVICE_V1_15_SIZE offsetof(struct RhhOwnedMonService, storage_unreadable_context)
#define RHH_OWNED_MON_SERVICE_V1_16_SIZE offsetof(struct RhhOwnedMonService, gift_context)
#define RHH_OWNED_MON_SERVICE_V1_17_SIZE offsetof(struct RhhOwnedMonService, evolution_context)
#define RHH_OWNED_MON_SERVICE_V1_18_SIZE offsetof(struct RhhOwnedMonService, battle_view_field_slot)
#define RHH_OWNED_MON_SERVICE_V1_19_SIZE offsetof(struct RhhOwnedMonService, sky_party_field_slot)
#define RHH_OWNED_MON_SERVICE_V1_20_SIZE offsetof(struct RhhOwnedMonService, follower_party_field_slot)
#define RHH_OWNED_MON_SERVICE_V1_21_SIZE offsetof(struct RhhOwnedMonService, experience)
#define RHH_OWNED_MON_SERVICE_V1_22_SIZE offsetof(struct RhhOwnedMonService, experience)
#define RHH_OWNED_MON_SERVICE_V1_23_SIZE offsetof(struct RhhOwnedMonService, experience)
#define RHH_OWNED_MON_SERVICE_V1_24_SIZE offsetof(struct RhhOwnedMonService, experience_change_context)
#define RHH_OWNED_MON_SERVICE_V1_25_SIZE offsetof(struct RhhOwnedMonService, experience_change_context)
#define RHH_OWNED_MON_SERVICE_V1_26_SIZE offsetof(struct RhhOwnedMonService, preview_experience)
#define RHH_OWNED_MON_SERVICE_V1_27_SIZE offsetof(struct RhhOwnedMonService, preview_experience_moves)
#define RHH_OWNED_MON_SERVICE_V1_28_SIZE offsetof(struct RhhOwnedMonService, award_experience)
#define RHH_OWNED_MON_SERVICE_V1_29_SIZE offsetof(struct RhhOwnedMonService, union_party_field_slot)
#define RHH_OWNED_MON_SERVICE_V1_30_SIZE offsetof(struct RhhOwnedMonService, union_party_field_slot)
#define RHH_OWNED_MON_SERVICE_V1_31_SIZE offsetof(struct RhhOwnedMonService, link_multi_party_field_slot)
#define RHH_OWNED_MON_SERVICE_V1_32_SIZE offsetof(struct RhhOwnedMonService, frontier_party_field_slot)
#define RHH_OWNED_MON_SERVICE_V1_33_SIZE offsetof(struct RhhOwnedMonService, stats)
#define RHH_OWNED_MON_SERVICE_V1_34_SIZE offsetof(struct RhhOwnedMonService, stats)
#define RHH_OWNED_MON_SERVICE_V1_35_SIZE offsetof(struct RhhOwnedMonService, stats)
#define RHH_OWNED_MON_SERVICE_V1_36_SIZE offsetof(struct RhhOwnedMonService, stats)
#define RHH_OWNED_MON_SERVICE_V1_37_SIZE offsetof(struct RhhOwnedMonService, stats)
#define RHH_OWNED_MON_SERVICE_V1_38_SIZE offsetof(struct RhhOwnedMonService, stats)
#define RHH_OWNED_MON_SERVICE_V1_39_SIZE offsetof(struct RhhOwnedMonService, iv_view)
#define RHH_OWNED_MON_SERVICE_V1_40_SIZE offsetof(struct RhhOwnedMonService, tera_view)
#define RHH_OWNED_MON_SERVICE_V1_41_SIZE offsetof(struct RhhOwnedMonService, identity_occurrences)
#define RHH_OWNED_MON_SERVICE_V1_42_SIZE sizeof(struct RhhOwnedMonService)
/* v1.43 invalidates read-only summary mappings on native memory writes. */
#define RHH_OWNED_MON_SERVICE_V1_43_SIZE RHH_OWNED_MON_SERVICE_V1_42_SIZE
/* v1.44 extends read-only summary guards to nature-format and link buffers. */
#define RHH_OWNED_MON_SERVICE_V1_44_SIZE RHH_OWNED_MON_SERVICE_V1_43_SIZE
/* v1.45 rechecks the received Pokémon at the link-trade save boundary. */
#define RHH_OWNED_MON_SERVICE_V1_45_SIZE RHH_OWNED_MON_SERVICE_V1_44_SIZE
/* v1.46 checks link-trade species changes against committed evolution. */
#define RHH_OWNED_MON_SERVICE_V1_46_SIZE RHH_OWNED_MON_SERVICE_V1_45_SIZE
/* Nature keys use rhh:nature/<lowercase name>. The ordinal matches the
 * nature and stat_nature fields in RhhOwnedMonSnapshot but is not durable.
 * Percentages are the engine's stat multipliers; HP and neutral stats are 100. */
struct RhhNatureInfo {
    uint32_t struct_size, ordinal;
    RhhContentKey key;
    uint16_t stat_percent[RHH_STAT_COUNT];
    char name[16];
};
enum RhhFlavor { RHH_FLAVOR_SPICY, RHH_FLAVOR_DRY, RHH_FLAVOR_SWEET,
    RHH_FLAVOR_BITTER, RHH_FLAVOR_SOUR, RHH_FLAVOR_COUNT };
enum RhhPalaceMoveGroup { RHH_PALACE_ATTACK, RHH_PALACE_DEFENSE, RHH_PALACE_SUPPORT, RHH_PALACE_GROUP_COUNT };
enum RhhPalaceTarget { RHH_PALACE_TARGET_STRONGER, RHH_PALACE_TARGET_WEAKER, RHH_PALACE_TARGET_RANDOM };
/* Flavor relations are -1, 0 or 1. Palace percentages are indexed by
 * [HP above half / HP at or below half][move group] and sum to 100. These
 * describe true nature rules, not a stat-only mint's effects. */
struct RhhNatureEffects {
    uint32_t struct_size, ordinal;
    RhhContentKey key;
    int8_t flavor[RHH_FLAVOR_COUNT];
    uint8_t palace_percent[2][RHH_PALACE_GROUP_COUNT];
    uint8_t reserved;
    uint32_t palace_target;
};
enum RhhNatureFormatStatus { RHH_NATURE_FORMAT_READY, RHH_NATURE_FORMAT_ACTIVE,
    RHH_NATURE_FORMAT_UNAVAILABLE, RHH_NATURE_FORMAT_INCOMPATIBLE, RHH_NATURE_FORMAT_UNREADABLE };
/* Saved full-party backup and recorded-battle parties are separate from the
 * live party. For a recording, box is the trainer and slot the party slot;
 * both are UINT32_MAX when the recording itself is unreadable. */
enum { RHH_NATURE_SAVED_PARTY = 256u, RHH_NATURE_RECORDED_BATTLE = 257u };
struct RhhNatureFormatCheck {
    uint32_t struct_size, status, location, box, slot, reserved[3];
};
/* rhh:service/nature v1.4. Nature metadata is available outside a run. */
struct RhhNatureService {
    struct RhhServiceHeader header;
    uint32_t (*count)(void);
    int (*at)(uint32_t ordinal, struct RhhNatureInfo *out);
    int (*find)(RhhContentKey key, struct RhhNatureInfo *out);
    int (*effects)(uint32_t ordinal, struct RhhNatureEffects *out);
    /* v1.2: read-only legacy-format preflight, not permission to edit. v1.3
     * also examines the current saved recorded-battle sector; v1.4 recognizes
     * checked nature-format recordings.
     * ACTIVE reports the marker and does not validate already-active data. */
    int (*format_preflight)(struct RhhNatureFormatCheck *out);
};
#define RHH_NATURE_SERVICE_V1_0_SIZE offsetof(struct RhhNatureService, effects)
#define RHH_NATURE_SERVICE_V1_1_SIZE offsetof(struct RhhNatureService, format_preflight)
#define RHH_NATURE_SERVICE_V1_2_SIZE sizeof(struct RhhNatureService)
#define RHH_NATURE_SERVICE_V1_3_SIZE sizeof(struct RhhNatureService)
#define RHH_NATURE_SERVICE_V1_4_SIZE sizeof(struct RhhNatureService)
enum RhhMonEditFields { RHH_EDIT_IVS = 1u, RHH_EDIT_EVS = 2u, RHH_EDIT_STAT_NATURE = 4u,
    RHH_EDIT_HYPER_TRAINING = 8u, RHH_EDIT_TRUE_NATURE = 16u, RHH_EDIT_TERA = 32u };
enum RhhMonEditStatus { RHH_EDIT_OK, RHH_EDIT_INVALID, RHH_EDIT_STALE,
    RHH_EDIT_DENIED, RHH_EDIT_UNAVAILABLE };
/* Expected is an unmodified owned-mon snapshot obtained for this slot.
 * All six values are supplied for every selected IV/EV field. */
struct RhhMonEditRequest {
    uint32_t struct_size, fields, stat_nature, reserved;
    struct RhhOwnedMonSnapshot expected;
    uint16_t ivs[RHH_STAT_COUNT], evs[RHH_STAT_COUNT];
};
struct RhhMonEditResult {
    uint32_t struct_size, status, fields, is_preview;
    struct RhhOwnedMonSnapshot before, after;
    uint32_t stats[RHH_STAT_COUNT], current_hp;
    char reason[96];
};
enum RhhMonEditBag { RHH_EDIT_BAG_NONE, RHH_EDIT_BAG_NORMAL, RHH_EDIT_BAG_PYRAMID };
/* A zero item key requires zero quantity; a zero quantity requires a zero key.
 * Money and the item are consumed together with the edit on commit. */
struct RhhMonEditCost {
    uint32_t struct_size, item_quantity, money, reserved;
    RhhContentKey item;
};
struct RhhMonEditPayment {
    uint32_t struct_size, bag, item_before, item_after;
    uint32_t money_before, money_after, item_quantity, money;
    RhhContentKey item;
};
struct RhhMonEditPaidRequest {
    uint32_t struct_size, reserved;
    struct RhhMonEditRequest edit;
    struct RhhMonEditCost cost;
};
struct RhhMonEditPaidResult {
    uint32_t struct_size, status;
    struct RhhMonEditResult edit;
    struct RhhMonEditPayment payment;
};
#define RHH_MON_EDIT_MAX_COST_ITEMS 8u
struct RhhMonEditRecipeItem { RhhContentKey item; uint32_t quantity, reserved; };
struct RhhMonEditRecipe {
    uint32_t struct_size, item_count, money, reserved;
    struct RhhMonEditRecipeItem items[RHH_MON_EDIT_MAX_COST_ITEMS];
};
struct RhhMonEditRecipeLine {
    RhhContentKey item;
    uint32_t quantity, before, after, reserved;
};
struct RhhMonEditRecipePayment {
    uint32_t struct_size, bag, item_count, money;
    uint32_t money_before, money_after, reserved[2];
    struct RhhMonEditRecipeLine items[RHH_MON_EDIT_MAX_COST_ITEMS];
};
struct RhhMonEditRecipeRequest {
    uint32_t struct_size, reserved;
    struct RhhMonEditRequest edit;
    struct RhhMonEditRecipe recipe;
};
struct RhhMonEditRecipeResult {
    uint32_t struct_size, status;
    struct RhhMonEditResult edit;
    struct RhhMonEditRecipePayment payment;
};
/* Training bits use RhhMonStat indices. The normal edit fields may be zero
 * for a training-only edit, or combine IVs, EVs and stat nature atomically.
 * Set has_recipe to charge the supplied recipe; zero means a free edit. */
struct RhhHyperTrainingEditRequest {
    uint32_t struct_size, before_mask, after_mask, has_recipe;
    struct RhhMonEditRequest edit;
    struct RhhMonEditRecipe recipe;
};
struct RhhHyperTrainingEditResult {
    uint32_t struct_size, status, before_mask, after_mask;
    uint32_t has_recipe, reserved[3];
    struct RhhMonEditResult edit;
    struct RhhMonEditRecipePayment payment;
};
struct RhhHyperTrainingEditContext {
    uint32_t struct_size, before_mask, after_mask, is_preview;
};
enum RhhTrueNatureStatPolicy { RHH_TRUE_NATURE_FOLLOW, RHH_TRUE_NATURE_PRESERVE,
    RHH_TRUE_NATURE_FOLLOW_UNMINTED };
/* A selected stat-nature edit takes precedence over stat_policy. Otherwise
 * FOLLOW sets the stat nature to the new true nature, PRESERVE leaves it as
 * it was, and FOLLOW_UNMINTED follows only when both previously matched. */
struct RhhTrueNatureEditRequest {
    uint32_t struct_size, stat_policy, has_recipe, reserved;
    RhhContentKey nature;
    struct RhhMonEditRequest edit;
    struct RhhMonEditRecipe recipe;
};
struct RhhTrueNatureEditResult {
    uint32_t struct_size, status, stat_policy, activates_format;
    uint32_t has_recipe, reserved[3];
    RhhContentKey nature;
    struct RhhMonEditResult edit;
    struct RhhMonEditRecipePayment payment;
};
struct RhhTrueNatureEditContext {
    uint32_t struct_size, stat_policy, activates_format, is_preview;
};
/* Combines true nature, Hyper Training and ordinary stat edits in one quote
 * and transaction. Training masks use RhhMonStat indices. */
struct RhhNatureTrainingEditRequest {
    uint32_t struct_size, before_mask, after_mask, reserved;
    struct RhhTrueNatureEditRequest nature;
};
struct RhhNatureTrainingEditResult {
    uint32_t struct_size, status, before_mask, after_mask;
    struct RhhTrueNatureEditResult nature;
};
/* type=0 restores the engine's personality-derived default. Nonzero types
 * must be built-in stable type keys that fit the existing storage format;
 * custom runtime type keys cannot be persisted by this transaction. */
struct RhhTeraEditRequest {
    uint32_t struct_size, has_recipe, reserved[2];
    RhhContentKey type;
    struct RhhOwnedTeraView expected;
    struct RhhMonEditRequest edit;
    struct RhhMonEditRecipe recipe;
};
struct RhhTeraEditResult {
    uint32_t struct_size, status, has_recipe, reserved;
    RhhContentKey type;
    struct RhhOwnedTeraView before, after;
    struct RhhMonEditResult edit;
    struct RhhMonEditRecipePayment payment;
};
struct RhhTeraEditContext {
    uint32_t struct_size, is_preview;
    RhhContentKey type;
    struct RhhOwnedTeraView before, after;
};
/* "rhh:service/mon_edit", version 1.8. preview and commit return a status
 * and fill result; context copies the current proposal only during a matching
 * RHH_CAN_EDIT_MON or RHH_MON_EDITED callback. v1.1 appends paid operations;
 * payment_context is available during those same callbacks for paid edits.
 * v1.2 adds recipes of up to eight distinct item types and money.
 * v1.8 preserves both gameplay RNG streams across all edit variants; an
 * allowed validation that consumes either stream is denied. Calls run on
 * the game thread. */
struct RhhMonEditService {
    struct RhhServiceHeader header;
    int (*preview)(const struct RhhMonEditRequest *, struct RhhMonEditResult *);
    int (*commit)(const struct RhhMonEditRequest *, struct RhhMonEditResult *);
    int (*context)(struct RhhMonEditResult *);
    int (*preview_paid)(const struct RhhMonEditPaidRequest *, struct RhhMonEditPaidResult *);
    int (*commit_paid)(const struct RhhMonEditPaidRequest *, struct RhhMonEditPaidResult *);
    int (*payment_context)(struct RhhMonEditPayment *);
    int (*preview_recipe)(const struct RhhMonEditRecipeRequest *, struct RhhMonEditRecipeResult *);
    int (*commit_recipe)(const struct RhhMonEditRecipeRequest *, struct RhhMonEditRecipeResult *);
    int (*recipe_context)(struct RhhMonEditRecipePayment *);
    /* v1.3: commit only if the newly staged outcome matches a successful
     * preview's snapshots, stats and HP. Paid variants also compare the
     * displayed payment, including item quantities and money balances. */
    int (*commit_previewed)(const struct RhhMonEditRequest *, const struct RhhMonEditResult *, struct RhhMonEditResult *);
    int (*commit_paid_previewed)(const struct RhhMonEditPaidRequest *, const struct RhhMonEditPaidResult *, struct RhhMonEditPaidResult *);
    int (*commit_recipe_previewed)(const struct RhhMonEditRecipeRequest *, const struct RhhMonEditRecipeResult *, struct RhhMonEditRecipeResult *);
    /* v1.4: atomic Hyper Training edits, optionally with an item recipe.
     * Commit requires a matching successful preview. Context is available
     * during the existing edit constraint and notification callbacks. */
    int (*preview_hyper_training)(const struct RhhHyperTrainingEditRequest *, struct RhhHyperTrainingEditResult *);
    int (*commit_hyper_training)(const struct RhhHyperTrainingEditRequest *, const struct RhhHyperTrainingEditResult *, struct RhhHyperTrainingEditResult *);
    int (*hyper_training_context)(struct RhhHyperTrainingEditContext *);
    /* v1.5: checked true-nature edit; an optional recipe is committed with it.
     * The first committed override activates the checked save format. Native
     * link sessions remain unavailable while that format is active. */
    int (*preview_true_nature)(const struct RhhTrueNatureEditRequest *, struct RhhTrueNatureEditResult *);
    int (*commit_true_nature)(const struct RhhTrueNatureEditRequest *, const struct RhhTrueNatureEditResult *, struct RhhTrueNatureEditResult *);
    int (*true_nature_context)(struct RhhTrueNatureEditContext *);
    /* v1.6: quote and commit nature and training together. Both respective
     * contexts are available to constraints during this transaction. */
    int (*preview_nature_training)(const struct RhhNatureTrainingEditRequest *, struct RhhNatureTrainingEditResult *);
    int (*commit_nature_training)(const struct RhhNatureTrainingEditRequest *, const struct RhhNatureTrainingEditResult *, struct RhhNatureTrainingEditResult *);
    /* v1.7: atomically stage Tera and optional ordinary stat fields and
     * recipe. Commit requires the successful, unchanged preview. */
    int (*preview_tera)(const struct RhhTeraEditRequest *, struct RhhTeraEditResult *);
    int (*commit_tera)(const struct RhhTeraEditRequest *, const struct RhhTeraEditResult *, struct RhhTeraEditResult *);
    int (*tera_context)(struct RhhTeraEditContext *);
};
#define RHH_MON_EDIT_SERVICE_V1_0_SIZE offsetof(struct RhhMonEditService, preview_paid)
#define RHH_MON_EDIT_SERVICE_V1_1_SIZE offsetof(struct RhhMonEditService, preview_recipe)
#define RHH_MON_EDIT_SERVICE_V1_2_SIZE offsetof(struct RhhMonEditService, commit_previewed)
#define RHH_MON_EDIT_SERVICE_V1_3_SIZE offsetof(struct RhhMonEditService, preview_hyper_training)
#define RHH_MON_EDIT_SERVICE_V1_4_SIZE offsetof(struct RhhMonEditService, preview_true_nature)
#define RHH_MON_EDIT_SERVICE_V1_5_SIZE offsetof(struct RhhMonEditService, preview_nature_training)
#define RHH_MON_EDIT_SERVICE_V1_6_SIZE offsetof(struct RhhMonEditService, preview_tera)
#define RHH_MON_EDIT_SERVICE_V1_7_SIZE sizeof(struct RhhMonEditService)
#define RHH_MON_EDIT_SERVICE_V1_8_SIZE sizeof(struct RhhMonEditService)
enum RhhMonGender { RHH_GENDER_MALE, RHH_GENDER_FEMALE, RHH_GENDER_GENDERLESS };
enum RhhDaycareCompatibility { RHH_DAYCARE_INCOMPATIBLE, RHH_DAYCARE_LOW,
    RHH_DAYCARE_MEDIUM, RHH_DAYCARE_HIGH };
struct RhhDaycareParent {
    uint32_t struct_size, gender, egg_groups[2], steps;
    struct RhhOwnedMonSnapshot mon;
    RhhContentKey held_item, moves[4];
};
struct RhhDaycareSnapshot {
    uint32_t struct_size, parent_count, egg_pending, compatibility_known, compatibility;
};
/* Point-in-time engine counters. Counts are calls to the corresponding engine
 * step function, not a promise about player movement or the next RNG result.
 * egg_roll_calls and egg_chance_percent are valid only when egg_roll_active.
 * Compatibility proposals run in preview mode and may change before the roll. */
struct RhhDaycareTiming {
    uint32_t struct_size, egg_pending, egg_roll_active, egg_roll_calls;
    uint32_t egg_chance_percent, hatch_counter, hatch_cycle_length, hatch_calls;
    uint32_t hatch_cycle_decrement, reserved[3];
};
/* A field-party egg's current hatch counters. calls_lower_bound assumes the
 * cycle length and incubator remain unchanged and no earlier egg interrupts
 * the loop; it is a count of ShouldEggHatch calls, not player movements. An
 * egg reaches zero cycles before it can hatch on a later check. When a cycle
 * hook is active, hatch_checks and calls_lower_bound use the fastest possible
 * policy (one cycle update plus a later hatch check), not a fixed-rate estimate. */
struct RhhDaycareEggProgress {
    uint32_t struct_size, cycles_remaining, hatch_checks, earlier_ready_eggs;
    uint32_t hatch_cycle_length, hatch_cycle_decrement, hatch_calls_next, reserved;
    uint64_t calls_lower_bound;
    struct RhhOwnedMonSnapshot egg;
};
enum RhhEggHatchEligibilityStatus { RHH_EGG_HATCH_NOT_READY, RHH_EGG_HATCH_ALLOWED,
    RHH_EGG_HATCH_DENIED, RHH_EGG_HATCH_IMPURE };
/* Copied ready-egg permission. NOT_READY never dispatches a hook. Impure
 * callbacks fail closed and gameplay RNG is restored. Identity is the
 * existing OT/personality snapshot, not a permanent individual ID. */
struct RhhEggHatchEligibility {
    uint32_t struct_size, status, cycles_remaining, reserved;
    uint64_t run_id;
    struct RhhOwnedMonSnapshot egg;
};
/* One predicted cycle check, not a reservation. The engine decrement already
 * accounts for its incubator rule and is capped at cycles_remaining. A pure
 * RHH_EGG_CYCLE_PROGRESS transform may select 0..cycles_remaining; denial,
 * invalid values or gameplay RNG use retain the engine decrement. */
struct RhhEggCycleProgress {
    uint32_t struct_size, cycles_remaining, engine_decrement, decrement;
    uint32_t remaining_after, reserved[3];
    uint64_t run_id;
    struct RhhOwnedMonSnapshot egg;
};
/* Emitted after an actual cycle check updates a non-ready egg, including a
 * zero-decrement policy. sequence is process-local, not a durable receipt. */
struct RhhEggCycleChange {
    uint32_t struct_size, reserved;
    uint64_t sequence;
    struct RhhEggCycleProgress progress;
};
/* "rhh:service/daycare", version 1.5. Parent slots are zero based. Egg
 * group values are current engine ordinals, not durable keys. Compatibility
 * is the unmodified engine pair score, not an egg-generation prediction. */
struct RhhDaycareService {
    struct RhhServiceHeader header;
    uint32_t (*parent_slots)(void);
    int (*current)(struct RhhDaycareSnapshot *out);
    int (*parent_at)(uint32_t slot, struct RhhDaycareParent *out);
    /* v1.1: copied roll and hatch-cycle timing with compatibility preview;
     * requires an active run outside a save transaction. */
    int (*timing)(struct RhhDaycareTiming *out);
    /* v1.2: read one valid egg in the stable field party. */
    int (*egg_progress)(uint32_t party_slot, struct RhhDaycareEggProgress *out);
    /* v1.3: preview a field-party egg's current hatch permission. context
     * copies the same input only during RHH_CAN_HATCH_EGG constraints. */
    int (*hatch_eligibility)(uint32_t party_slot, struct RhhEggHatchEligibility *out);
    int (*hatch_eligibility_context)(struct RhhEggHatchEligibility *out);
    /* v1.4: preview the next cycle update; context is only available during
     * RHH_EGG_CYCLE_PROGRESS. Neither call changes the egg or clock. */
    int (*cycle_progress)(uint32_t party_slot, struct RhhEggCycleProgress *out);
    int (*cycle_progress_context)(struct RhhEggCycleProgress *out);
    /* v1.5: copied only during RHH_EGG_CYCLE_TICKED observers. */
    int (*cycle_change_context)(struct RhhEggCycleChange *out);
};
#define RHH_DAYCARE_SERVICE_V1_0_SIZE offsetof(struct RhhDaycareService, timing)
#define RHH_DAYCARE_SERVICE_V1_1_SIZE offsetof(struct RhhDaycareService, egg_progress)
#define RHH_DAYCARE_SERVICE_V1_2_SIZE offsetof(struct RhhDaycareService, hatch_eligibility)
#define RHH_DAYCARE_SERVICE_V1_3_SIZE offsetof(struct RhhDaycareService, cycle_progress)
#define RHH_DAYCARE_SERVICE_V1_4_SIZE offsetof(struct RhhDaycareService, cycle_change_context)
#define RHH_DAYCARE_SERVICE_V1_5_SIZE sizeof(struct RhhDaycareService)
/* Breeding proposals use the copied parent snapshots. Compatibility decision
 * values are RhhDaycareCompatibility; offspring decisions use a species key.
 * The final notification carries the committed egg's identity and species.
 * Parent slots are the engine's inheritance roles, not guaranteed genders. */
struct RhhBreedingContext {
    uint32_t struct_size, base_compatibility, compatibility, egg_valid;
    struct RhhDaycareParent parents[2];
    uint32_t parent_slots[2];
    RhhContentKey original_offspring, offspring;
    uint64_t egg_identity;
    /* Version 1.1: the staged offspring during inheritance proposals and
     * the committed egg during notification, with its current moves. */
    struct RhhOwnedMonSnapshot offspring_mon;
    RhhContentKey offspring_moves[4];
    /* Version 1.2: nonzero when this is a read-only pair preview. */
    uint32_t preview, reserved[3];
};
enum RhhBreedIVSource { RHH_BREED_IV_ENGINE, RHH_BREED_IV_PARENT_0,
    RHH_BREED_IV_PARENT_1, RHH_BREED_IV_INITIAL };
#define RHH_BREEDING_CONTEXT_V1_0_SIZE offsetof(struct RhhBreedingContext, offspring_mon)
#define RHH_BREEDING_CONTEXT_V1_1_SIZE offsetof(struct RhhBreedingContext, preview)
struct RhhBreedingPreviewCandidate {
    RhhContentKey original_offspring, offspring;
    uint32_t parent_slots[2];
};
struct RhhBreedingPreview {
    uint32_t struct_size, base_compatibility, compatibility, egg_pending, candidate_count;
    uint32_t reserved[3];
    struct RhhBreedingPreviewCandidate candidates[2];
};
enum RhhBreedingAcceptStatus { RHH_BREED_ACCEPT_OK, RHH_BREED_ACCEPT_INVALID,
    RHH_BREED_ACCEPT_UNAVAILABLE, RHH_BREED_ACCEPT_NO_EGG, RHH_BREED_ACCEPT_PARTY_FULL,
    RHH_BREED_ACCEPT_STALE, RHH_BREED_ACCEPT_DENIED, RHH_BREED_ACCEPT_INCOMPATIBLE };
/* The token is a short-lived fingerprint of run, trainer, daycare and party
 * state, not a durable egg identity or a reservation. */
struct RhhBreedingOffer {
    uint32_t struct_size, pending, party_count, parent_count, compatibility, reserved;
    uint64_t run_id, token;
    struct RhhDaycareParent parents[2];
    struct RhhBreedingPreview preview;
};
struct RhhBreedingAcceptResult {
    uint32_t struct_size, status, egg_valid, party_slot;
    struct RhhBreedingOffer offer;
    struct RhhOwnedMonSnapshot egg;
    char reason[96];
};
enum RhhBreedingRequestStatus { RHH_BREED_REQUEST_OK, RHH_BREED_REQUEST_INVALID,
    RHH_BREED_REQUEST_UNAVAILABLE, RHH_BREED_REQUEST_ALREADY_PENDING,
    RHH_BREED_REQUEST_STALE, RHH_BREED_REQUEST_DENIED, RHH_BREED_REQUEST_INCOMPATIBLE };
/* The offer is the checked pre-request state. This requests a pending egg;
 * it does not roll or reserve the egg that will later be accepted. */
struct RhhBreedingRequestResult {
    uint32_t struct_size, status;
    struct RhhBreedingOffer offer;
    char reason[96];
};
enum RhhDaycarePendingCause { RHH_DAYCARE_PENDING_WALK = 1, RHH_DAYCARE_PENDING_MANUAL,
    RHH_DAYCARE_PENDING_REQUEST, RHH_DAYCARE_PENDING_REJECTED, RHH_DAYCARE_PENDING_ACCEPTED };
/* A live transition, not a durable egg identity or a replay-proof receipt.
 * parent_mask identifies copied parent records that were decodable. */
struct RhhDaycarePendingChange {
    uint32_t struct_size, cause, before, after, parent_mask, reserved;
    uint64_t run_id;
    struct RhhDaycareParent parents[2];
};
/* This identifies a pending daycare event, not the resulting owned Pokémon.
 * It is stable across restoration of the save generation that contains it. */
struct RhhDaycareEggId {
    uint32_t struct_size, reserved;
    uint64_t run_id, high, low;
};
enum RhhBreedingRngMode { RHH_BREED_RNG_ENGINE, RHH_BREED_RNG_PENDING };
/* The pending mode repeats engine egg-generation draws for this ID and the
 * same inputs. It does not freeze parents, settings or the resulting egg. */
struct RhhBreedingRngPolicy {
    uint32_t struct_size, mode, reserved[2];
    struct RhhDaycareEggId id;
    struct RhhDaycareParent parents[2];
};
/* A copied staged egg for the current repeatable policy and unchanged offer.
 * ball and ability_slot are engine ordinals; moves and ability are stable keys.
 * ball_item is a stable item key, or zero for an unknown/non-item ball. */
struct RhhBreedingExactPreview {
    uint32_t struct_size, ball, ability_slot, gender, shiny, reserved[3];
    struct RhhDaycareEggId pending_id;
    struct RhhOwnedMonSnapshot egg;
    RhhContentKey original_offspring, ability, moves[4];
    uint32_t parent_slots[2];
    RhhContentKey ball_item; /* breeding service v1.20 */
};
#define RHH_BREEDING_EXACT_PREVIEW_V1_18_SIZE offsetof(struct RhhBreedingExactPreview, ball_item)
#define RHH_BREEDING_EXACT_PREVIEW_V1_20_SIZE sizeof(struct RhhBreedingExactPreview)
enum RhhBreedingIvPreviewFlags { RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE = 1u };
/* Each values bitset is a safe superset of possible IV values 0..31, not a
 * probability or a promise that every value will occur. Active inheritance
 * Older staged-egg hooks make engine-selected values uncertain. Pure IV source
 * rules can still guarantee a parent's value or select an initial random IV.
 * guaranteed_stats marks power-item and explicitly forced parent guarantees;
 * other stats may also always be inherited. inherited_count is the engine
 * count before later per-stat overrides. */
struct RhhBreedingIvPreview {
    uint32_t struct_size, flags, inherited_count, guaranteed_stats;
    uint32_t values[RHH_STAT_COUNT];
};
/* Marginal value weights under independent, uniform engine draws. These are
 * a model, not a prediction of the next egg or a joint distribution. When
 * an older RHH_BREED_INHERIT_IV hook is active, denominator is zero and
 * weights are unavailable. Otherwise every stat's weights sum to denominator;
 * pure RHH_BREED_IV_RULE overrides are included. */
struct RhhBreedingIvOdds {
    uint32_t struct_size, flags, denominator, inherited_count;
    uint16_t weights[RHH_STAT_COUNT][32];
};
/* Each bitset permits IV values 0..31 for one stat; zero permits no values.
 * Joint odds are the chance that all six conditions hold together under the
 * same independent-uniform model as iv_odds. This is not the next RNG result.
 * Older staged-egg inheritance hooks make the model unavailable (flag set,
 * denominator zero); pure IV source rules are included. */
struct RhhBreedingIvJointRequest {
    uint32_t struct_size, reserved[3];
    uint32_t values[RHH_STAT_COUNT];
};
struct RhhBreedingIvJointOdds {
    uint32_t struct_size, flags, inherited_count, reserved;
    uint64_t numerator, denominator;
};
/* RHH_BREED_IV_COUNT proposes 0..6 inherited stats before any inheritance
 * draws, using only copied parents and the held-item-derived baseline. It
 * runs in previews and egg generation with the same context. A denied or
 * invalid proposal retains the baseline. Callbacks must be deterministic and
 * pure; they may use the current pending ID but cannot inspect the staged egg
 * or consume gameplay RNG. At zero, power
 * items cannot force an IV; at six the parent-bit draw uses 0..63. */
struct RhhBreedingIvCountContext {
    uint32_t struct_size, base_count, reserved[2];
    struct RhhDaycareParent parents[2];
};
/* RHH_BREED_IV_RULE is a pure transform/constraint for one stat. It starts
 * with RHH_BREED_IV_ENGINE, meaning retain the result of the engine and any
 * older RHH_BREED_INHERIT_IV hook. A transform can instead select parent 0,
 * parent 1 or the egg's initial random IV. Denial or an invalid selection
 * retains that earlier result. The same rule is used in previews and actual
 * eggs; callbacks must depend only on this copied context, the current pending
 * ID and stable settings, not on the staged egg, gameplay RNG, or whether this
 * is a preview. A pending-ID stream is unavailable before an egg is pending. */
struct RhhBreedingIvRuleContext {
    uint32_t struct_size, stat, reserved[2];
    struct RhhDaycareParent parents[2];
};
enum RhhBreedingMovePreviewFlags { RHH_BREED_MOVE_PREVIEW_HOOKS_ACTIVE = 1u };
/* Moves after normal inheritance and pure move rules for each candidate in
 * the matching offer. Active staged-egg hooks can change the final egg, so
 * their flag indicates the displayed list is not a promise. */
struct RhhBreedingMovePreviewCandidate {
    RhhContentKey original_offspring, offspring;
    uint32_t parent_slots[2];
    RhhContentKey moves[4];
};
struct RhhBreedingMovePreview {
    uint32_t struct_size, flags, candidate_count, reserved;
    struct RhhBreedingMovePreviewCandidate candidates[2];
};
/* RHH_BREED_MOVE_RULE is a pure per-slot transform/constraint over the
 * engine moveset, before older staged-egg move hooks. The decision starts as
 * base_moves[slot]; zero removes that move. Replacements must be known by a
 * parent and learnable by the offspring. The complete proposal is discarded
 * if it is empty, contains duplicates, or has an invalid move. A denied slot
 * retains its baseline. Callbacks must depend only on this copied context, the
 * current pending ID and stable settings, not on gameplay RNG, the staged egg
 * or whether it is a preview. */
struct RhhBreedingMoveRuleContext {
    uint32_t struct_size, slot, compatibility, reserved;
    RhhContentKey original_offspring, offspring;
    uint32_t parent_slots[2];
    RhhContentKey base_moves[4];
    struct RhhDaycareParent parents[2];
};
#define RHH_NATURE_COUNT 25u
/* Modeled nature probabilities under independent uniform draws. generation is
 * the human-readable generation number (1 for Gen I). Source weights are
 * parent 0, parent 1 and random before nature transforms, out of
 * source_denominator. Nature weights include transforms, use the nature-service
 * ordinals and sum to denominator. This describes the
 * distribution, not the next egg or deterministic RNG correlations. */
struct RhhBreedingNatureOdds {
    uint32_t struct_size, generation, denominator, source_denominator;
    uint32_t source_weights[3];
    uint16_t weights[RHH_NATURE_COUNT];
};
/* Pure RHH_BREED_NATURE transform/constraint context. base_nature is a nature
 * key or zero for the engine's random-nature choice; generation is 1-based.
 * decision.value starts as base_nature. A transform can select a nature key
 * or zero; denial or an invalid key retains the engine choice. Callbacks must
 * be deterministic for the copied context, current pending ID and stable
 * settings, and cannot depend on gameplay RNG or whether they run during
 * generation or an odds query. */
struct RhhBreedingNatureContext {
    uint32_t struct_size, generation, reserved[2];
    RhhContentKey base_nature;
    struct RhhDaycareParent parents[2];
};
/* Pure post-inheritance rules for a staged egg. For BALL_RULE, base is the
 * inherited ball's item key; decision.value may select another eligible ball
 * item key. For ABILITY_RULE, base and decision.value are ability-slot
 * ordinals 0..2. A changed slot must exist on the resolved offspring species.
 * Denial, an invalid proposal or gameplay RNG use retains the engine result.
 * Event key is offspring, subject is original offspring, and args[0] is the
 * resolved RHH_DAYCARE_* compatibility. Callbacks must depend only on this
 * context, the pending ID and stable settings, and must behave identically
 * during preview and generation. */
struct RhhBreedingTraitContext {
    uint32_t struct_size, compatibility, parent_slots[2];
    RhhContentKey original_offspring, offspring;
    uint64_t base;
    struct RhhDaycareParent parents[2];
};
/* Conditional ball probabilities per candidate under uniform engine draws.
 * Weights sum to denominator for each candidate; source weights describe the
 * untransformed parent 0, parent 1 and default sources. Ball rules are applied
 * to each source, and equal results are combined. This is not a prediction of
 * the next egg, candidate probabilities or correlations with other traits. */
struct RhhBreedingBallOddsEntry {
    RhhContentKey item;
    uint32_t ball, weight;
};
struct RhhBreedingBallOddsCandidate {
    RhhContentKey original_offspring, offspring;
    uint32_t parent_slots[2], count, reserved;
    struct RhhBreedingBallOddsEntry balls[3];
};
struct RhhBreedingBallOdds {
    uint32_t struct_size, generation, denominator, candidate_count;
    uint32_t source_weights[3], reserved;
    struct RhhBreedingBallOddsCandidate candidates[2];
};
/* Ability-slot probabilities conditional on each offspring candidate. The
 * model assumes independent uniform draws and an even personality parity;
 * this is not a prediction or a joint distribution with nature or other
 * traits. Slot 3 can occur with malformed parent data and resolves using the
 * engine's ordinary ability fallback. Equal slots are combined, not equal
 * ability keys. parent_slot is UINT32_MAX if no parent can pass an ability. */
struct RhhBreedingAbilityOddsEntry {
    RhhContentKey ability;
    uint32_t slot, weight;
};
struct RhhBreedingAbilityOddsCandidate {
    RhhContentKey original_offspring, offspring;
    uint32_t parent_slots[2], count, reserved;
    struct RhhBreedingAbilityOddsEntry abilities[4];
};
struct RhhBreedingAbilityOdds {
    uint32_t struct_size, generation, denominator, candidate_count;
    uint32_t parent_slot, inherited_slot, inherited_percent, reserved;
    struct RhhBreedingAbilityOddsCandidate candidates[2];
};
/* Weights for the currently previewed offspring species, assuming the
 * engine's variant bit is equally likely when no egg is pending. Pending
 * eggs use their stored variant. Rules may give different answers during
 * generation or when the egg becomes pending; the flag signals that such
 * rules are installed. The result is not an egg reservation. */
enum RhhBreedingOffspringOddsFlags { RHH_BREED_OFFSPRING_ODDS_RULES_ACTIVE = 1u };
struct RhhBreedingOffspringOddsEntry { RhhContentKey offspring; uint32_t weight, reserved; };
struct RhhBreedingOffspringOdds {
    uint32_t struct_size, flags, denominator, count;
    struct RhhBreedingOffspringOddsEntry outcomes[2];
};
/* Gender probabilities conditional on each current offspring candidate.
 * Weights sum to denominator for each candidate. The independent-uniform
 * personality model includes nature selection and rejection sampling; it
 * does not predict the next egg or joint odds with other traits. Species
 * proposal rules may change between the preview and egg generation; flags
 * uses RHH_BREED_OFFSPRING_ODDS_RULES_ACTIVE. */
struct RhhBreedingGenderOddsCandidate {
    RhhContentKey original_offspring, offspring;
    uint32_t parent_slots[2], male, female, genderless, reserved;
};
struct RhhBreedingGenderOdds {
    uint32_t struct_size, denominator, candidate_count, flags;
    struct RhhBreedingGenderOddsCandidate candidates[2];
};
/* Joint nature and gender weights, conditional on each candidate. Nature
 * uses the existing independent-uniform source model; gender is conditioned
 * on that nature's personality remainder, rounded to billionths. Each
 * candidate's 25 x 3 weights sum to denominator. The result does not model
 * correlations with other traits or predict a particular egg. */
struct RhhBreedingNatureGenderCandidate {
    RhhContentKey original_offspring, offspring;
    uint32_t parent_slots[2];
    uint64_t male[RHH_NATURE_COUNT], female[RHH_NATURE_COUNT], genderless[RHH_NATURE_COUNT];
};
struct RhhBreedingNatureGenderOdds {
    uint32_t struct_size, candidate_count, flags, reserved;
    uint64_t denominator;
    struct RhhBreedingNatureGenderCandidate candidates[2];
};
/* An opt-in, save-backed sequence for eggs committed by the ordinary player
 * daycare. next starts at 1 when enabled and advances only after placement.
 * UINT64_MAX means exhausted; normal egg creation continues without an ordinal.
 * The counter rewinds with save rollback and does not identify a Pokémon. */
struct RhhDaycareOrdinalState {
    uint32_t struct_size, enabled, exhausted, reserved;
    uint64_t run_id, next;
};
/* Available only during the committed egg-created notification when the
 * counter was enabled and had capacity. party_slot is the placed egg's slot. */
struct RhhDaycareEggOrdinal {
    uint32_t struct_size, party_slot, reserved[2];
    uint64_t run_id, ordinal;
};
/* "rhh:service/breeding", version 1.28 (older prefixes supported). context() is
 * available only during a breeding callback; the copied data is transient. */
struct RhhBreedingService {
    struct RhhServiceHeader header;
    int (*context)(struct RhhBreedingContext *out);
    /* Preview the current pair without creating an egg or consuming RNG. */
    int (*preview)(struct RhhBreedingPreview *out);
    /* Inspect the live offer and accept it only while its token and resolved
     * preview still match. accept_context copies the permission proposal
     * during RHH_CAN_ACCEPT_DAYCARE_EGG constraint hooks. */
    int (*offer)(struct RhhBreedingOffer *out);
    int (*accept)(const struct RhhBreedingOffer *expected, struct RhhBreedingAcceptResult *out);
    int (*accept_context)(struct RhhBreedingAcceptResult *out);
    int (*iv_preview)(const struct RhhBreedingOffer *expected, struct RhhBreedingIvPreview *out);
    int (*iv_odds)(const struct RhhBreedingOffer *expected, struct RhhBreedingIvOdds *out);
    int (*iv_joint_odds)(const struct RhhBreedingOffer *expected, const struct RhhBreedingIvJointRequest *request,
                         struct RhhBreedingIvJointOdds *out);
    int (*iv_count_context)(struct RhhBreedingIvCountContext *out);
    /* v1.8: preview moves for every current offspring candidate without
     * creating an egg. Pure move rules participate; staged-egg hooks do not. */
    int (*move_preview)(const struct RhhBreedingOffer *expected, struct RhhBreedingMovePreview *out);
    int (*nature_odds)(const struct RhhBreedingOffer *expected, struct RhhBreedingNatureOdds *out);
    int (*nature_context)(struct RhhBreedingNatureContext *out);
    int (*iv_rule_context)(struct RhhBreedingIvRuleContext *out);
    int (*move_rule_context)(struct RhhBreedingMoveRuleContext *out);
    int (*request_pending)(const struct RhhBreedingOffer *expected, struct RhhBreedingRequestResult *out);
    int (*request_context)(struct RhhBreedingRequestResult *out);
    int (*pending_context)(struct RhhDaycarePendingChange *out);
    int (*pending_id)(struct RhhDaycareEggId *out);
    int (*pending_change_id)(struct RhhDaycareEggId *out);
    /* A deterministic, named stream for the currently checked pending ID.
     * namespace_id must name a registered module. Does not use game RNG. */
    int (*pending_random_at)(const struct RhhDaycareEggId *expected, const char *namespace_id,
                             uint64_t ordinal, uint64_t *out);
    int (*rng_policy)(const struct RhhBreedingOffer *expected, struct RhhBreedingRngPolicy *out);
    int (*rng_policy_context)(struct RhhBreedingRngPolicy *out);
    int (*exact_preview)(const struct RhhBreedingOffer *expected, struct RhhBreedingExactPreview *out);
    /* v1.19: copied only during BALL_RULE or ABILITY_RULE. */
    int (*trait_context)(struct RhhBreedingTraitContext *out);
    /* v1.21: conditional ball inheritance odds for a checked pair. */
    int (*ball_odds)(const struct RhhBreedingOffer *expected, struct RhhBreedingBallOdds *out);
    /* v1.22: conditional ability-slot odds for a checked pair. */
    int (*ability_odds)(const struct RhhBreedingOffer *expected, struct RhhBreedingAbilityOdds *out);
    /* v1.23: estimated offspring species weights for a checked offer. */
    int (*offspring_odds)(const struct RhhBreedingOffer *expected, struct RhhBreedingOffspringOdds *out);
    /* v1.24: accept only if the current repeatable preview and the actual
     * generated egg still match expected_preview. A mismatch returns STALE
     * before changing the party or consuming the pending egg. */
    int (*accept_exact)(const struct RhhBreedingOffer *expected,
                        const struct RhhBreedingExactPreview *expected_preview,
                        struct RhhBreedingAcceptResult *out);
    /* v1.25: inspect or explicitly enable the daycare creation sequence.
     * Enabling is idempotent, requires the current run ID and a safe command
     * context, and activates a newer host-save schema. */
    int (*ordinal_state)(struct RhhDaycareOrdinalState *out);
    int (*enable_ordinal)(uint64_t expected_run, struct RhhDaycareOrdinalState *out);
    int (*egg_ordinal_context)(struct RhhDaycareEggOrdinal *out);
    /* v1.26: conditional gender odds under the current checked offer. */
    int (*gender_odds)(const struct RhhBreedingOffer *expected, struct RhhBreedingGenderOdds *out);
    /* v1.27: conditional joint nature/gender distribution. */
    int (*nature_gender_odds)(const struct RhhBreedingOffer *expected, struct RhhBreedingNatureGenderOdds *out);
};
#define RHH_BREEDING_SERVICE_V1_1_SIZE offsetof(struct RhhBreedingService, preview)
#define RHH_BREEDING_SERVICE_V1_2_SIZE offsetof(struct RhhBreedingService, offer)
#define RHH_BREEDING_SERVICE_V1_3_SIZE offsetof(struct RhhBreedingService, iv_preview)
#define RHH_BREEDING_SERVICE_V1_4_SIZE offsetof(struct RhhBreedingService, iv_odds)
#define RHH_BREEDING_SERVICE_V1_5_SIZE offsetof(struct RhhBreedingService, iv_joint_odds)
#define RHH_BREEDING_SERVICE_V1_6_SIZE offsetof(struct RhhBreedingService, iv_count_context)
#define RHH_BREEDING_SERVICE_V1_7_SIZE offsetof(struct RhhBreedingService, move_preview)
#define RHH_BREEDING_SERVICE_V1_8_SIZE offsetof(struct RhhBreedingService, nature_odds)
#define RHH_BREEDING_SERVICE_V1_9_SIZE offsetof(struct RhhBreedingService, nature_context)
#define RHH_BREEDING_SERVICE_V1_10_SIZE offsetof(struct RhhBreedingService, iv_rule_context)
#define RHH_BREEDING_SERVICE_V1_11_SIZE offsetof(struct RhhBreedingService, move_rule_context)
#define RHH_BREEDING_SERVICE_V1_12_SIZE offsetof(struct RhhBreedingService, request_pending)
#define RHH_BREEDING_SERVICE_V1_13_SIZE offsetof(struct RhhBreedingService, pending_context)
#define RHH_BREEDING_SERVICE_V1_14_SIZE offsetof(struct RhhBreedingService, pending_id)
#define RHH_BREEDING_SERVICE_V1_15_SIZE offsetof(struct RhhBreedingService, pending_random_at)
#define RHH_BREEDING_SERVICE_V1_16_SIZE offsetof(struct RhhBreedingService, rng_policy)
#define RHH_BREEDING_SERVICE_V1_17_SIZE offsetof(struct RhhBreedingService, exact_preview)
#define RHH_BREEDING_SERVICE_V1_18_SIZE offsetof(struct RhhBreedingService, trait_context)
#define RHH_BREEDING_SERVICE_V1_19_SIZE offsetof(struct RhhBreedingService, ball_odds)
#define RHH_BREEDING_SERVICE_V1_20_SIZE offsetof(struct RhhBreedingService, ball_odds)
#define RHH_BREEDING_SERVICE_V1_21_SIZE offsetof(struct RhhBreedingService, ability_odds)
#define RHH_BREEDING_SERVICE_V1_22_SIZE offsetof(struct RhhBreedingService, offspring_odds)
#define RHH_BREEDING_SERVICE_V1_23_SIZE offsetof(struct RhhBreedingService, accept_exact)
#define RHH_BREEDING_SERVICE_V1_24_SIZE offsetof(struct RhhBreedingService, ordinal_state)
#define RHH_BREEDING_SERVICE_V1_25_SIZE offsetof(struct RhhBreedingService, gender_odds)
#define RHH_BREEDING_SERVICE_V1_26_SIZE offsetof(struct RhhBreedingService, nature_gender_odds)
#define RHH_BREEDING_SERVICE_V1_27_SIZE sizeof(struct RhhBreedingService)
/* v1.28 restores gameplay RNG and last-used ability after committed pending,
 * egg-request and egg-created observers. */
#define RHH_BREEDING_SERVICE_V1_28_SIZE RHH_BREEDING_SERVICE_V1_27_SIZE
/* A hatch changes the engine's OT/personality identity when ownership is
 * assigned. These two snapshots refer to the same party slot at that moment;
 * neither identity is guaranteed unique or durable across later operations. */
struct RhhEggHatchContext {
    uint32_t struct_size, reserved;
    RhhContentKey map;
    struct RhhOwnedMonSnapshot egg, hatched;
};
/* "rhh:service/egg_hatch", version 1.1. context() copies the current
 * transition only during a committed RHH_EGG_HATCHED observer callback.
 * v1.1 restores gameplay RNG and last-used ability after observers. */
struct RhhEggHatchService {
    struct RhhServiceHeader header;
    int (*context)(struct RhhEggHatchContext *out);
};
#define RHH_EGG_HATCH_SERVICE_V1_0_SIZE sizeof(struct RhhEggHatchService)
#define RHH_EGG_HATCH_SERVICE_V1_1_SIZE RHH_EGG_HATCH_SERVICE_V1_0_SIZE
/* Transient, read-only overworld snapshots. Map x/y are local metatile
 * coordinates; screen coordinates are sprite centers in pixels. Graphics and animation IDs are
 * current-engine values, not stable content keys. */
struct RhhWorldSnapshot {
    uint32_t struct_size, width, height, map_type;
    RhhContentKey map;
};
/* The clock counts accepted, non-forced player tile steps, not render frames
 * or elapsed real time. It is saved with the run and may rewind on save
 * rollback. Run IDs and step ordinals are scoped to the current save. */
struct RhhWorldClockSnapshot {
    uint32_t struct_size, reserved;
    uint64_t run_id, steps;
};
struct RhhWorldStepSnapshot {
    uint32_t struct_size, reserved;
    uint64_t run_id, step;
    RhhContentKey map;
    int32_t x, y, elevation;
    uint32_t behavior;
};
/* "rhh:service/world_clock", version 1.0. current() copies the clock;
 * step_context() is available only during a committed RHH_WORLD_STEP observer.
 * Both fail without modifying output when unavailable or undersized. */
struct RhhWorldClockService {
    struct RhhServiceHeader header;
    int (*current)(struct RhhWorldClockSnapshot *out);
    int (*step_context)(struct RhhWorldStepSnapshot *out);
};
/* Save-generation-backed opaque records for modules. Keys and owner
 * are stable content keys; data belongs to the named module, not to the host's
 * game objects. Zero-length records are not stored. Revisions are scoped to
 * the current save history and can repeat after rollback. */
struct RhhModArchiveRecord {
    uint32_t struct_size, size;
    RhhContentKey owner, key;
    uint64_t run_id, revision;
};
struct RhhModArchiveUsage {
    uint32_t struct_size, record_count, owner_bytes, total_bytes;
    uint64_t run_id, revision;
};
#define RHH_MOD_ARCHIVE_BATCH_MAX 32u
enum RhhModArchiveOperationKind { RHH_MOD_ARCHIVE_PUT = 1, RHH_MOD_ARCHIVE_REMOVE };
struct RhhModArchiveOperation {
    uint32_t struct_size, kind, size, reserved;
    RhhContentKey key;
    uint64_t expected_revision;
    const void *data;
};
struct RhhModArchiveOperationResult {
    uint32_t kind, size;
    RhhContentKey key;
    uint64_t revision;
};
struct RhhModArchiveBatchResult {
    uint32_t struct_size, count;
    RhhContentKey owner;
    uint64_t run_id, revision;
    struct RhhModArchiveOperationResult operations[RHH_MOD_ARCHIVE_BATCH_MAX];
};
/* "rhh:service/mod_archive" v1.3. Reads require an active run and registered
 * owner. at() enumerates that owner's records in key order. copy() requires
 * the observed revision and enough space. put() creates with expected=0 or
 * replaces the exact observed revision; remove() requires that revision.
 * Writes are accepted from that owner's RHH_MOD_UPDATE and selected committed
 * observers, or its verified modal/overlay input callback, outside a save
 * transaction. The supported events are listed in MODDING.md. They
 * are atomic in memory and saved with the next
 * ordinary save, not durable journals. Failed calls leave outputs unchanged. */
struct RhhModArchiveService {
    struct RhhServiceHeader header;
    int (*usage)(RhhContentKey owner, struct RhhModArchiveUsage *out);
    int (*at)(RhhContentKey owner, uint32_t index, struct RhhModArchiveRecord *out);
    int (*find)(RhhContentKey owner, RhhContentKey key, struct RhhModArchiveRecord *out);
    int (*copy)(RhhContentKey owner, RhhContentKey key, uint64_t revision, void *out, uint32_t capacity);
    int (*put)(RhhContentKey owner, RhhContentKey key, uint64_t expected_revision,
               const void *data, uint32_t size, struct RhhModArchiveRecord *out);
    int (*remove)(RhhContentKey owner, RhhContentKey key, uint64_t expected_revision);
    /* v1.1: atomically apply 1..BATCH_MAX distinct keys for one owner.
     * Each operation has the same revision and size rules as put/remove;
     * REMOVE requires data=NULL and size=0. Limits apply to the final state.
     * Revisions and results follow input order, including removals. */
    int (*batch)(RhhContentKey owner, const struct RhhModArchiveOperation *operations,
                 uint32_t count, struct RhhModArchiveBatchResult *out);
};
#define RHH_MOD_ARCHIVE_SERVICE_V1_0_SIZE offsetof(struct RhhModArchiveService, batch)
#define RHH_MOD_ARCHIVE_SERVICE_V1_1_SIZE sizeof(struct RhhModArchiveService)
#define RHH_MOD_ARCHIVE_SERVICE_V1_2_SIZE sizeof(struct RhhModArchiveService)
#define RHH_MOD_ARCHIVE_SERVICE_V1_3_SIZE sizeof(struct RhhModArchiveService)
/* Local clock time is a sampled wall/fake clock, not the deterministic step
 * clock. local_day is the engine's signed day count after the saved clock
 * offset, not a calendar date. period matches the effective overworld time,
 * including a script override. override_hour is zero when there is no override. */
enum RhhWorldTimePeriod { RHH_TIME_MORNING = 1, RHH_TIME_DAY, RHH_TIME_EVENING, RHH_TIME_NIGHT };
/* SYSTEM is used by environment timing records; world_time.sample continues
 * to report REAL or FAKE. SYSTEM is unadjusted Unix time from the PC. */
enum RhhWorldTimeSource { RHH_TIME_REAL = 1, RHH_TIME_FAKE, RHH_TIME_SYSTEM };
enum RhhWorldTimeFlags { RHH_TIME_RTC_FALLBACK = 1u };
struct RhhWorldTimeSnapshot {
    uint32_t struct_size, source, period, flags;
    int32_t local_day;
    uint32_t hour, minute, second, effective_hour, override_hour, rtc_status, reserved;
    uint64_t run_id, step;
};
/* "rhh:service/world_time", version 1.0. sample() takes one read of the
 * game clock and copies the resulting snapshot; invalid/unavailable reads and
 * undersized outputs fail without changing the output or game clock globals. */
struct RhhWorldTimeService {
    struct RhhServiceHeader header;
    int (*sample)(struct RhhWorldTimeSnapshot *out);
};
enum RhhWorldEnvironmentKind { RHH_WORLD_WEATHER = 1, RHH_WORLD_TERRAIN = 2 };
/* RHH_WORLD_ENVIRONMENT_MOVE resolves a known move's field effect. event_key
 * is its canonical stable move key; args[0..1] are the original kind and
 * value. decision.value starts as RHH_WORLD_ENV_MOVE(kind,value), or zero for
 * an unsupported move. Transforms may propose zero or a supported nonzero
 * pair; constraints cannot rewrite it. Invalid, denied or RNG-consuming
 * proposals retain the original and both gameplay RNG streams are restored.
 * Callbacks must be pure and stable across preview and apply. */
#define RHH_WORLD_ENV_MOVE(kind, value) (((uint64_t)(kind) << 32) | (uint32_t)(value))
enum RhhWorldEnvironmentStatus { RHH_WORLD_ENV_OK, RHH_WORLD_ENV_INVALID, RHH_WORLD_ENV_STALE,
    RHH_WORLD_ENV_CONFLICT, RHH_WORLD_ENV_DENIED, RHH_WORLD_ENV_UNAVAILABLE, RHH_WORLD_ENV_FULL };
enum RhhWorldEnvironmentCause { RHH_WORLD_ENV_SET = 1, RHH_WORLD_ENV_REPLACED,
    RHH_WORLD_ENV_CLEARED, RHH_WORLD_ENV_EXPIRED };
struct RhhWorldEnvironmentEffect {
    uint32_t value; /* RhhBattleWeather or RhhBattleTerrain; zero means absent. */
    int32_t priority;
    RhhContentKey owner, move; /* move is zero for module-owned sources. */
    uint64_t actor_identity, started_step, expires_step;
};
struct RhhWorldEnvironmentSnapshot {
    uint32_t struct_size, reserved;
    RhhContentKey map;
    uint64_t step;
    struct RhhWorldEnvironmentEffect weather, terrain;
};
/* The poison proposal starts at base_damage. A transform can select 0 through
 * max_damage; a constraint can deny damage. Unchanged, invalid or impure
 * proposals retain the engine's original behavior, including the Gen IV
 * one-HP recovery case. environment_available indicates a saved-map sample.
 * The committed notification occurs only for actual HP loss. */
struct RhhFieldPoisonContext {
    uint32_t struct_size, base_damage, max_damage, selected_damage, applied, after_hp;
    uint32_t overridden, reached_threshold, environment_available, reserved;
    uint64_t run_id;
    struct RhhFieldCondition before;
    struct RhhWorldEnvironmentSnapshot environment;
};
/* owner is the registered module ID's content key. move zero clears that
 * owner's effect; otherwise it must be an eligible move known by the party
 * Pokémon at party_slot with the supplied identity. map must be current. */
struct RhhWorldEnvironmentRequest {
    uint32_t struct_size, kind, party_slot, reserved;
    int32_t priority;
    uint32_t reserved2;
    RhhContentKey map, owner, move;
    uint64_t actor_identity, duration_steps;
};
struct RhhWorldEnvironmentResult {
    uint32_t struct_size, status, cause, is_preview, kind, reserved;
    struct RhhWorldEnvironmentSnapshot before, after;
    char reason[96];
};
enum RhhBattleSeedFlags { RHH_BATTLE_SEED_TRAINER = 1u, RHH_BATTLE_SEED_DOUBLE = 2u,
    RHH_BATTLE_SEED_FIRST = 4u, RHH_BATTLE_SEED_ROAMER = 8u };
/* Copied during RHH_BATTLE_ENVIRONMENT_SEED reducers. The original map
 * state is immutable; decision.value contains the current proposed semantic
 * weather or terrain value and decision.allowed can reject the seed.
 * Opponent trainer keys are zero when unavailable or not applicable. */
struct RhhBattleEnvironmentSeed {
    uint32_t struct_size, kind, initial_value, battle_flags;
    RhhContentKey opponent_trainers[2];
    struct RhhWorldEnvironmentSnapshot environment;
};
/* A supported move currently known by a party member. usable covers only
 * the member's egg, HP and challenge-death state; a later preview/apply can
 * still reject the world context, ownership, priority or module policy. */
struct RhhWorldEnvironmentMove {
    uint32_t struct_size, party_slot, move_slot, usable, kind, value;
    uint64_t actor_identity;
    RhhContentKey move;
};
enum RhhWorldEnvironmentClock { RHH_WORLD_ENV_CLOCK_STEPS = 1, RHH_WORLD_ENV_CLOCK_TIME = 2,
    RHH_WORLD_ENV_CLOCK_PERMANENT = 3 };
/* Wall/fake-clock coordinates are seconds since local day INT32_MIN; they
 * use the saved game clock offset, not Unix time. A clock change or rewind can
 * extend a duration. time_available is false if the saved source does not
 * match the current source or the RTC is invalid/fallback. SYSTEM timing uses
 * unadjusted Unix seconds, independently of the game RTC. A permanent
 * source reports its start step, the current step and a zero expiry. */
struct RhhWorldEnvironmentTiming {
    uint32_t struct_size, clock, source, time_available;
    uint64_t started, expires, now;
};
/* Both active effects and their timings sampled against one step and clock
 * observation. An absent or expired effect has value and timing.clock zero.
 * A time-based effect remains active when its saved source is unavailable. */
struct RhhWorldEnvironmentView {
    uint32_t struct_size, reserved;
    struct RhhWorldEnvironmentSnapshot environment;
    struct RhhWorldEnvironmentTiming weather_timing, terrain_timing;
};
#define RHH_WORLD_ENVIRONMENT_MAP_CAPACITY 128u
/* A timed request uses the same owner, map, move and actor checks as an
 * ordinary request. request.duration_steps and both reserved fields must be
 * zero. duration_seconds is 1..1000000. The legacy result represents its
 * expiry as UINT64_MAX steps; query timing for the real deadline. */
struct RhhWorldEnvironmentTimedRequest {
    uint32_t struct_size, reserved;
    uint64_t duration_seconds;
    struct RhhWorldEnvironmentRequest request;
};
/* A checked quote captures the request, both environment kinds, the selected
 * prior timing for both effects and the proposed timing. Timed durations start
 * at commit; their clock source and relative duration must still match. */
struct RhhWorldEnvironmentQuote {
    uint32_t struct_size, timed; /* 0 steps, 1 time, 2 permanent source */
    uint64_t run_id, duration_seconds;
    struct RhhWorldEnvironmentRequest request;
    struct RhhWorldEnvironmentResult result;
    struct RhhWorldEnvironmentTiming before_timing, other_before_timing, after_timing;
};
/* Module-owned sources use semantic weather or terrain values without a
 * Pokémon move. duration uses clock units; zero value clears the owner's
 * effect and requires STEPS with zero duration. PERMANENT sources have zero
 * duration and remain until replaced or cleared; they require service 1.9. */
struct RhhWorldEnvironmentSourceRequest {
    uint32_t struct_size, kind, value, clock;
    int32_t priority;
    uint32_t reserved;
    RhhContentKey map, owner;
    uint64_t duration;
};
struct RhhWorldEnvironmentSourceQuote {
    uint32_t struct_size, reserved;
    struct RhhWorldEnvironmentSourceRequest request;
    struct RhhWorldEnvironmentQuote transaction;
};
/* "rhh:service/world_environment", version 1.10. at/current are pure
 * snapshots; preview and apply validate an entire field-move transaction.
 * context copies the current proposal/notification only during its callback.
 * battle_seed_context is an append-only 1.1 addition and is available only
 * during RHH_BATTLE_ENVIRONMENT_SEED. Check header.struct_size and version. */
struct RhhWorldEnvironmentService {
    struct RhhServiceHeader header;
    int (*at)(RhhContentKey map, struct RhhWorldEnvironmentSnapshot *out);
    int (*current)(struct RhhWorldEnvironmentSnapshot *out);
    int (*preview)(const struct RhhWorldEnvironmentRequest *, struct RhhWorldEnvironmentResult *);
    int (*apply)(const struct RhhWorldEnvironmentRequest *, struct RhhWorldEnvironmentResult *);
    int (*context)(struct RhhWorldEnvironmentResult *out);
    int (*battle_seed_context)(struct RhhBattleEnvironmentSeed *out);
    /* v1.2: query a party member's actual move slot (0..3). Unsupported,
     * empty and invalid slots fail without modifying out. This is a pure view. */
    int (*party_move)(uint32_t party_slot, uint32_t move_slot, struct RhhWorldEnvironmentMove *out);
    /* v1.4: timing queries an active effect; context_timing reads the
     * proposed or changed effect of the requested kind during its callback. */
    int (*timing)(RhhContentKey map, uint32_t kind, struct RhhWorldEnvironmentTiming *out);
    int (*context_timing)(uint32_t kind, struct RhhWorldEnvironmentTiming *out);
    int (*preview_timed)(const struct RhhWorldEnvironmentTimedRequest *, struct RhhWorldEnvironmentResult *);
    int (*apply_timed)(const struct RhhWorldEnvironmentTimedRequest *, struct RhhWorldEnvironmentResult *);
    /* v1.5: apply only if a successful quote still matches the request,
     * environment, step and selected effect's timing. Both apply operations
     * rerun current constraints and reject stale quotes without changing state. */
    int (*preview_checked)(const struct RhhWorldEnvironmentRequest *, struct RhhWorldEnvironmentQuote *);
    int (*apply_checked)(const struct RhhWorldEnvironmentRequest *, const struct RhhWorldEnvironmentQuote *, struct RhhWorldEnvironmentResult *);
    int (*preview_timed_checked)(const struct RhhWorldEnvironmentTimedRequest *, struct RhhWorldEnvironmentQuote *);
    int (*apply_timed_checked)(const struct RhhWorldEnvironmentTimedRequest *, const struct RhhWorldEnvironmentQuote *, struct RhhWorldEnvironmentResult *);
    /* v1.6: one coherent view, or a single bounded snapshot of all maps with
     * active effects, ordered by canonical map key. A null array with zero
     * capacity queries the count. Too-small arrays fail without changing the
     * array or count. Enumeration does not simulate or expire unloaded maps. */
    int (*inspect)(RhhContentKey map, struct RhhWorldEnvironmentView *out);
    int (*list)(struct RhhWorldEnvironmentView *out, uint32_t capacity, uint32_t *count);
    /* v1.7: source changes use the same priority, constraints, clocks,
     * notifications and checked transactions as field-move changes. */
    int (*preview_source)(const struct RhhWorldEnvironmentSourceRequest *, struct RhhWorldEnvironmentResult *out);
    int (*apply_source)(const struct RhhWorldEnvironmentSourceRequest *, struct RhhWorldEnvironmentResult *out);
    int (*preview_source_checked)(const struct RhhWorldEnvironmentSourceRequest *, struct RhhWorldEnvironmentSourceQuote *out);
    int (*apply_source_checked)(const struct RhhWorldEnvironmentSourceRequest *, const struct RhhWorldEnvironmentSourceQuote *,
                                struct RhhWorldEnvironmentResult *out);
    /* v1.8: source operations may target any known map, including unloaded
     * maps. They change saved environment state without loading or simulating
     * the target map. The original source operations still require the
     * current map; quotes use the same request and state checks. */
    int (*preview_source_at)(const struct RhhWorldEnvironmentSourceRequest *, struct RhhWorldEnvironmentResult *out);
    int (*apply_source_at)(const struct RhhWorldEnvironmentSourceRequest *, struct RhhWorldEnvironmentResult *out);
    int (*preview_source_at_checked)(const struct RhhWorldEnvironmentSourceRequest *, struct RhhWorldEnvironmentSourceQuote *out);
    int (*apply_source_at_checked)(const struct RhhWorldEnvironmentSourceRequest *, const struct RhhWorldEnvironmentSourceQuote *,
                                   struct RhhWorldEnvironmentResult *out);
};
#define RHH_WORLD_ENVIRONMENT_SERVICE_V1_0_SIZE offsetof(struct RhhWorldEnvironmentService, battle_seed_context)
#define RHH_WORLD_ENVIRONMENT_SERVICE_V1_1_SIZE offsetof(struct RhhWorldEnvironmentService, party_move)
#define RHH_WORLD_ENVIRONMENT_SERVICE_V1_2_SIZE offsetof(struct RhhWorldEnvironmentService, timing)
#define RHH_WORLD_ENVIRONMENT_SERVICE_V1_3_SIZE RHH_WORLD_ENVIRONMENT_SERVICE_V1_2_SIZE
#define RHH_WORLD_ENVIRONMENT_SERVICE_V1_4_SIZE offsetof(struct RhhWorldEnvironmentService, preview_checked)
#define RHH_WORLD_ENVIRONMENT_SERVICE_V1_5_SIZE offsetof(struct RhhWorldEnvironmentService, inspect)
#define RHH_WORLD_ENVIRONMENT_SERVICE_V1_6_SIZE offsetof(struct RhhWorldEnvironmentService, preview_source)
#define RHH_WORLD_ENVIRONMENT_SERVICE_V1_7_SIZE offsetof(struct RhhWorldEnvironmentService, preview_source_at)
#define RHH_WORLD_ENVIRONMENT_SERVICE_V1_8_SIZE sizeof(struct RhhWorldEnvironmentService)
#define RHH_WORLD_ENVIRONMENT_SERVICE_V1_9_SIZE RHH_WORLD_ENVIRONMENT_SERVICE_V1_8_SIZE
/* v1.10 rechecks state after constraints and preserves RNG in observers. */
#define RHH_WORLD_ENVIRONMENT_SERVICE_V1_10_SIZE RHH_WORLD_ENVIRONMENT_SERVICE_V1_9_SIZE
struct RhhWorldTile {
    uint32_t struct_size, metatile, behavior, elevation, collision, layer;
    /* Eight engine tile entries, lower four then upper four. */
    uint16_t tiles[8];
};
/* Authored map data is static and may differ from a loaded or saved map.
 * map_type and engine_weather are engine ordinals; natural_weather uses
 * RhhNaturalWeather and may be DYNAMIC or UNKNOWN. */
enum RhhAuthoredMapFlags { RHH_AUTHORED_MAP_FRLG = 1u, RHH_AUTHORED_MAP_CYCLING = 2u,
    RHH_AUTHORED_MAP_ESCAPE = 4u, RHH_AUTHORED_MAP_RUNNING = 8u, RHH_AUTHORED_MAP_CAVE = 16u };
struct RhhAuthoredMap {
    uint32_t struct_size, width, height, map_type, engine_weather, natural_weather, flags, reserved;
    RhhContentKey map, area;
};
struct RhhAuthoredMapTile {
    uint32_t struct_size, habitat_flags, habitat_available, reserved;
    int32_t x, y;
    RhhContentKey map;
    struct RhhWorldTile tile;
};
/* A row-major page of a rectangle in one source layout. Unavailable cells
 * retain their coordinates, with a zero tile; their cause is not inferred. */
#define RHH_AUTHORED_MAP_SCAN_MAX 128u
enum RhhAuthoredMapCellStatus { RHH_AUTHORED_MAP_CELL_OK, RHH_AUTHORED_MAP_CELL_UNAVAILABLE };
struct RhhAuthoredMapScanRequest {
    uint32_t struct_size, x, y, width, height, first, limit, reserved;
    RhhContentKey map;
};
struct RhhAuthoredMapCell {
    uint32_t status, x, y, reserved;
    struct RhhAuthoredMapTile tile;
};
struct RhhAuthoredMapScan {
    uint32_t struct_size, count, next, total;
    RhhContentKey map;
    struct RhhAuthoredMapCell cells[RHH_AUTHORED_MAP_SCAN_MAX];
};
#define RHH_AUTHORED_TILE_BATCH_MAX 256u
enum RhhAuthoredTileStatus { RHH_AUTHORED_TILE_OK, RHH_AUTHORED_TILE_UNAVAILABLE };
/* One fixed-size batch result; unavailable entries have zeroed pixels. */
struct RhhAuthoredTileImage {
    uint32_t status;
    uint16_t tile, reserved;
    uint8_t rgba[8 * 8 * 4];
};
enum RhhAuthoredConnectionDirection { RHH_AUTHORED_CONNECTION_UNKNOWN, RHH_AUTHORED_CONNECTION_SOUTH,
    RHH_AUTHORED_CONNECTION_NORTH, RHH_AUTHORED_CONNECTION_WEST, RHH_AUTHORED_CONNECTION_EAST,
    RHH_AUTHORED_CONNECTION_DIVE, RHH_AUTHORED_CONNECTION_EMERGE };
enum RhhAuthoredConnectionFlags { RHH_AUTHORED_CONNECTION_TARGET_AVAILABLE = 1u,
    RHH_AUTHORED_CONNECTION_GEOMETRY_AVAILABLE = 2u };
/* Source-local destination origin and half-open seam interval are supplied for
 * available cardinal destinations. DIVE/EMERGE have no planar geometry. */
struct RhhAuthoredConnection {
    uint32_t struct_size, index, direction, engine_direction, flags, target_width, target_height, reserved;
    int32_t offset, origin_x, origin_y, seam_start, seam_end;
    RhhContentKey map, target;
};
enum RhhAuthoredWarpFlags { RHH_AUTHORED_WARP_DYNAMIC = 1u, RHH_AUTHORED_WARP_TARGET_AVAILABLE = 2u,
    RHH_AUTHORED_WARP_TARGET_EVENT_AVAILABLE = 4u, RHH_AUTHORED_WARP_TARGET_IN_BOUNDS = 8u };
/* A copied authored warp and, when available, the referenced destination
 * warp. target_warp is the raw byte; dynamic destinations are not resolved. */
struct RhhAuthoredWarp {
    uint32_t struct_size, index, flags, source_elevation, target_warp, target_elevation, reserved[2];
    int32_t x, y, target_x, target_y;
    RhhContentKey map, target;
};
enum RhhAuthoredObjectKind { RHH_AUTHORED_OBJECT_NORMAL, RHH_AUTHORED_OBJECT_CLONE,
    RHH_AUTHORED_OBJECT_UNKNOWN };
enum RhhAuthoredObjectFlags { RHH_AUTHORED_OBJECT_GRAPHICS_VARIABLE = 1u,
    RHH_AUTHORED_OBJECT_TARGET_AVAILABLE = 2u, RHH_AUTHORED_OBJECT_TARGET_EVENT_AVAILABLE = 4u };
/* Raw engine values are transient. Normal-only fields and clone-only fields
 * are zero for other kinds. target_local_id is a one-based template index. */
struct RhhAuthoredObject {
    uint32_t struct_size, index, kind, engine_kind, flags, local_id;
    int32_t x, y;
    uint32_t graphics_id, elevation, movement_type, movement_range_x, movement_range_y;
    uint32_t trainer_type, trainer_range_or_berry_id, flag_id, target_local_id;
    RhhContentKey map, target;
};
enum RhhAuthoredCoordKind { RHH_AUTHORED_COORD_WEATHER, RHH_AUTHORED_COORD_SCRIPT_IMMEDIATE,
    RHH_AUTHORED_COORD_SCRIPT_CONDITIONAL };
enum RhhAuthoredCoordFlags { RHH_AUTHORED_COORD_WEATHER_KNOWN = 1u };
/* trigger and condition are transient engine ordinals. Weather fields are
 * valid only for WEATHER_KNOWN; weather_code is the effective low byte. */
struct RhhAuthoredCoordEvent {
    uint32_t struct_size, index, kind, flags;
    int32_t x, y;
    uint32_t elevation, trigger, condition, weather_code, engine_weather, natural_weather;
    RhhContentKey map;
};
enum RhhAuthoredBackgroundKind { RHH_AUTHORED_BACKGROUND_SCRIPT,
    RHH_AUTHORED_BACKGROUND_HIDDEN_ITEM, RHH_AUTHORED_BACKGROUND_SECRET_BASE,
    RHH_AUTHORED_BACKGROUND_UNKNOWN };
enum RhhAuthoredBackgroundFlags { RHH_AUTHORED_BACKGROUND_HAS_SCRIPT = 1u,
    RHH_AUTHORED_BACKGROUND_UNDERFOOT = 2u };
enum RhhAuthoredBackgroundFacing { RHH_AUTHORED_BACKGROUND_FACING_NONE,
    RHH_AUTHORED_BACKGROUND_FACING_ANY, RHH_AUTHORED_BACKGROUND_FACING_NORTH,
    RHH_AUTHORED_BACKGROUND_FACING_SOUTH, RHH_AUTHORED_BACKGROUND_FACING_EAST,
    RHH_AUTHORED_BACKGROUND_FACING_WEST };
/* Authored background interaction, not current availability. engine_kind,
 * item_id, hidden_item_id and secret_base_id are transient engine ordinals.
 * facing is the authored script-facing setting, not a live interaction check;
 * other kind-specific fields are zero
 * for other kinds. No script pointer is exposed. */
struct RhhAuthoredBackgroundEvent {
    uint32_t struct_size, index, kind, engine_kind, flags, x, y, elevation;
    uint32_t facing, item_id, hidden_item_id, quantity, secret_base_id;
    RhhContentKey map, item;
};
/* A source image, not an actor's current animation or visibility. */
struct RhhAuthoredObjectImage {
    uint32_t struct_size, index, width, height;
    RhhContentKey map;
};
enum RhhAuthoredAnimCommandKind { RHH_AUTHORED_ANIM_FRAME, RHH_AUTHORED_ANIM_JUMP,
    RHH_AUTHORED_ANIM_LOOP, RHH_AUTHORED_ANIM_END, RHH_AUTHORED_ANIM_UNKNOWN };
enum RhhAuthoredAnimFlags { RHH_AUTHORED_ANIM_HFLIP = 1u, RHH_AUTHORED_ANIM_VFLIP = 2u,
    RHH_AUTHORED_ANIM_FRAME_AVAILABLE = 4u, RHH_AUTHORED_ANIM_JUMP_IN_BOUNDS = 8u };
/* FRAME: image and duration; JUMP: argument is target command index; LOOP:
 * argument is repeat count. END and UNKNOWN have no decoded parameters. */
struct RhhAuthoredAnimCommand {
    uint32_t struct_size, object_index, animation, command, kind, flags;
    uint32_t image, duration, argument, reserved;
    RhhContentKey map;
};
enum RhhObjectVisualResolutionFlags { RHH_OBJECT_VISUAL_VARIABLE = 1u, RHH_OBJECT_VISUAL_FALLBACK = 2u,
    RHH_OBJECT_VISUAL_POKEMON = 4u, RHH_OBJECT_VISUAL_SHINY = 8u, RHH_OBJECT_VISUAL_FEMALE = 16u,
    RHH_OBJECT_VISUAL_DYNAMIC = 32u };
/* Graphics IDs and variable slots are transient engine values. A Pokémon
 * resolution supplies a species key when known for species_visuals. */
struct RhhObjectVisualResolution {
    uint32_t struct_size, flags, input_id, resolved_id, variable_slot, reserved;
    RhhContentKey species;
};
enum RhhObjectVisualFlags { RHH_OBJECT_VISUAL_INANIMATE = 1u };
struct RhhObjectVisual {
    uint32_t struct_size, graphics_id, flags, width, height, frame_count, animation_count, reserved;
};
struct RhhObjectVisualAnimCommand {
    uint32_t struct_size, graphics_id, animation, command, kind, flags;
    uint32_t image, duration, argument, reserved;
};
/* Berry item keys and RhhBerryStage select the engine's source graphic; empty
 * plots have no visible tree. These are not live sprite or plot snapshots. */
struct RhhBerryVisualRequest { uint32_t struct_size, stage; RhhContentKey berry_item; };
struct RhhBerryVisual {
    uint32_t struct_size, stage, width, height, frame_count, command_count;
    RhhContentKey berry_item;
};
struct RhhBerryVisualCommand {
    uint32_t struct_size, stage, command, kind, flags, image, duration, argument;
    RhhContentKey berry_item;
};
/* A point-in-time dynamic selection. The variant is an engine ordinal, not a
 * durable identity; retain this copy to query the same source across changes. */
enum RhhObjectVisualVariantKind { RHH_OBJECT_VARIANT_MAUVILLE_OLD_MAN = 1 };
struct RhhObjectVisualVariant { uint32_t struct_size, kind, variant, reserved; };
/* "rhh:service/object_visuals" v1.2. Source graphics by resolved engine ID,
 * independent of map loading. resolve samples saved graphics variables but
 * does not run map scripts; source frames are RGBA8888, top-left origin. */
struct RhhObjectVisualService {
    struct RhhServiceHeader header;
    int (*resolve)(uint32_t graphics_id, struct RhhObjectVisualResolution *out);
    int (*info)(uint32_t graphics_id, struct RhhObjectVisual *out);
    int (*frame_rgba)(uint32_t graphics_id, uint32_t frame, uint8_t *rgba, uint32_t capacity);
    int (*animation_command_count)(uint32_t graphics_id, uint32_t animation, uint32_t *out);
    int (*animation_command)(uint32_t graphics_id, uint32_t animation, uint32_t command,
                             struct RhhObjectVisualAnimCommand *out);
    int (*face_animation)(uint32_t graphics_id, uint32_t direction, uint32_t *out);
    /* v1.1: source frames and stage animation for an explicit berry and
     * growth stage; no plot is loaded or changed. */
    int (*berry_info)(const struct RhhBerryVisualRequest *request, struct RhhBerryVisual *out);
    int (*berry_frame_rgba)(const struct RhhBerryVisualRequest *request, uint32_t frame, uint8_t *rgba, uint32_t capacity);
    int (*berry_command)(const struct RhhBerryVisualRequest *request, uint32_t command, struct RhhBerryVisualCommand *out);
    /* v1.2: capture a supported dynamic choice, then query its source. */
    int (*variant_current)(uint32_t resolved_graphics_id, struct RhhObjectVisualVariant *out);
    int (*variant_info)(const struct RhhObjectVisualVariant *variant, struct RhhObjectVisual *out);
    int (*variant_frame_rgba)(const struct RhhObjectVisualVariant *variant, uint32_t frame, uint8_t *rgba, uint32_t capacity);
    int (*variant_animation_command_count)(const struct RhhObjectVisualVariant *variant, uint32_t animation, uint32_t *out);
    int (*variant_animation_command)(const struct RhhObjectVisualVariant *variant, uint32_t animation, uint32_t command,
                                     struct RhhObjectVisualAnimCommand *out);
    int (*variant_face_animation)(const struct RhhObjectVisualVariant *variant, uint32_t direction, uint32_t *out);
};
#define RHH_OBJECT_VISUAL_SERVICE_V1_0_SIZE offsetof(struct RhhObjectVisualService, berry_info)
#define RHH_OBJECT_VISUAL_SERVICE_V1_1_SIZE offsetof(struct RhhObjectVisualService, variant_current)
#define RHH_OBJECT_VISUAL_SERVICE_V1_2_SIZE sizeof(struct RhhObjectVisualService)
/* "rhh:service/authored_maps" v1.11. Read-only static data for a keyed map,
 * including maps that are not loaded. No live weather, scripts, saved edits,
 * actors, habitat hooks or simulation are applied. Queries require finalized
 * modules. Outputs are copied; all failures leave them untouched. Enumerate
 * keys through host content_at. */
struct RhhAuthoredMapService {
    struct RhhServiceHeader header;
    int (*map)(RhhContentKey map, struct RhhAuthoredMap *out);
    int (*tile)(RhhContentKey map, int32_t x, int32_t y, struct RhhAuthoredMapTile *out);
    /* v1.1: decode one authored 4bpp tile entry to 8x8 RGBA. The source
     * palettes and graphics do not include live animation, lighting or fades. */
    int (*tile_rgba)(RhhContentKey map, uint16_t tile, uint8_t *rgba, uint32_t capacity);
    /* v1.2: decode up to RHH_AUTHORED_TILE_BATCH_MAX entries in input order.
     * Invalid requests fail without modifying results; unavailable individual
     * tiles are reported in their result. A zero count accepts null arrays. */
    int (*tiles_rgba)(RhhContentKey map, const uint16_t *tiles, uint32_t count,
                      struct RhhAuthoredTileImage *out, uint32_t capacity);
    /* v1.3: authored connection order, not live movement or warp permission. */
    int (*connection_count)(RhhContentKey map, uint32_t *out);
    int (*connection_at)(RhhContentKey map, uint32_t index, struct RhhAuthoredConnection *out);
    /* v1.4: authored warp events; dynamic and script-dependent destinations
     * are not resolved or executed. */
    int (*warp_count)(RhhContentKey map, uint32_t *out);
    int (*warp_at)(RhhContentKey map, uint32_t index, struct RhhAuthoredWarp *out);
    /* v1.5: source object templates, not current actors or visibility. */
    int (*object_count)(RhhContentKey map, uint32_t *out);
    int (*object_at)(RhhContentKey map, uint32_t index, struct RhhAuthoredObject *out);
    /* v1.6: source coordinate events, without running scripts or weather. */
    int (*coord_count)(RhhContentKey map, uint32_t *out);
    int (*coord_at)(RhhContentKey map, uint32_t index, struct RhhAuthoredCoordEvent *out);
    /* v1.7: first source image of supported fixed object graphics. RGBA8888,
     * tightly packed, top-left origin; palette index zero is transparent. */
    int (*object_image)(RhhContentKey map, uint32_t index, struct RhhAuthoredObjectImage *out);
    int (*object_image_rgba)(RhhContentKey map, uint32_t index, uint8_t *rgba, uint32_t capacity);
    /* v1.8: bounded engine image indices for supported fixed graphics. These
     * are source frames, not timed animations or guaranteed facing directions. */
    int (*object_frame_count)(RhhContentKey map, uint32_t index, uint32_t *out);
    int (*object_frame_rgba)(RhhContentKey map, uint32_t index, uint32_t frame, uint8_t *rgba, uint32_t capacity);
    /* v1.9: bounded source animation scripts and their engine facing mapping.
     * direction uses RhhWildActorDirection. These do not run animation. */
    int (*object_animation_count)(RhhContentKey map, uint32_t index, uint32_t *out);
    int (*object_animation_command_count)(RhhContentKey map, uint32_t index, uint32_t animation, uint32_t *out);
    int (*object_animation_command)(RhhContentKey map, uint32_t index, uint32_t animation, uint32_t command,
                                    struct RhhAuthoredAnimCommand *out);
    int (*object_face_animation)(RhhContentKey map, uint32_t index, uint32_t direction, uint32_t *out);
    /* v1.10: authored background events in source order, without evaluating
     * scripts, hidden-item flags or secret-base state. */
    int (*background_count)(RhhContentKey map, uint32_t *out);
    int (*background_at)(RhhContentKey map, uint32_t index, struct RhhAuthoredBackgroundEvent *out);
    /* v1.11: page a nonempty in-bounds rectangle in row-major order. first
     * is an offset within the rectangle, limit is 1..SCAN_MAX, and first may
     * equal total for an empty final page. next is the next offset. Invalid
     * requests fail untouched; each undecodable source cell is reported. */
    int (*scan)(const struct RhhAuthoredMapScanRequest *request, struct RhhAuthoredMapScan *out);
};
#define RHH_AUTHORED_MAP_SERVICE_V1_0_SIZE offsetof(struct RhhAuthoredMapService, tile_rgba)
#define RHH_AUTHORED_MAP_SERVICE_V1_1_SIZE offsetof(struct RhhAuthoredMapService, tiles_rgba)
#define RHH_AUTHORED_MAP_SERVICE_V1_2_SIZE offsetof(struct RhhAuthoredMapService, connection_count)
#define RHH_AUTHORED_MAP_SERVICE_V1_3_SIZE offsetof(struct RhhAuthoredMapService, warp_count)
#define RHH_AUTHORED_MAP_SERVICE_V1_4_SIZE offsetof(struct RhhAuthoredMapService, object_count)
#define RHH_AUTHORED_MAP_SERVICE_V1_5_SIZE offsetof(struct RhhAuthoredMapService, coord_count)
#define RHH_AUTHORED_MAP_SERVICE_V1_6_SIZE offsetof(struct RhhAuthoredMapService, object_image)
#define RHH_AUTHORED_MAP_SERVICE_V1_7_SIZE offsetof(struct RhhAuthoredMapService, object_frame_count)
#define RHH_AUTHORED_MAP_SERVICE_V1_8_SIZE offsetof(struct RhhAuthoredMapService, object_animation_count)
#define RHH_AUTHORED_MAP_SERVICE_V1_9_SIZE offsetof(struct RhhAuthoredMapService, background_count)
#define RHH_AUTHORED_MAP_SERVICE_V1_10_SIZE offsetof(struct RhhAuthoredMapService, scan)
#define RHH_AUTHORED_MAP_SERVICE_V1_11_SIZE sizeof(struct RhhAuthoredMapService)
/* A sampled engine flag associated with an authored event. TEMPORARY flags
 * belong to the current map context; SPECIAL flags are process-local. NONE
 * means the object has no hide flag, with value zero. IDs are engine ordinals. */
enum RhhAuthoredFlagScope { RHH_AUTHORED_FLAG_NONE, RHH_AUTHORED_FLAG_SAVED,
    RHH_AUTHORED_FLAG_TEMPORARY, RHH_AUTHORED_FLAG_SPECIAL };
struct RhhAuthoredEventFlag {
    uint32_t struct_size, index, flag_id, scope, value, reserved;
    uint64_t run_id;
    RhhContentKey map;
};
enum RhhDynamicWarpFlags { RHH_DYNAMIC_WARP_MAP_AVAILABLE = 1u, RHH_DYNAMIC_WARP_ARRIVAL_EVENT = 2u,
    RHH_DYNAMIC_WARP_ARRIVAL_COORDS = 4u, RHH_DYNAMIC_WARP_ARRIVAL_CENTER = 8u,
    RHH_DYNAMIC_WARP_ARRIVAL_IN_BOUNDS = 16u };
/* Saved dynamic WarpData and the engine's ordinary destination-coordinate
 * choice for an available authored map. Raw map/warp numbers are transient;
 * map is the stable key when known. Arrival coordinates are map-local. */
struct RhhDynamicWarp {
    uint32_t struct_size, flags;
    int32_t map_group, map_number, warp_id, saved_x, saved_y, arrival_x, arrival_y;
    uint64_t run_id;
    RhhContentKey map;
};
/* "rhh:service/authored_event_state" v1.1 pairs authored event indices with
 * current flag values and exposes saved dynamic warp data. This is not a live
 * actor, spawn, interaction result, or guarantee that a later warp uses it. */
struct RhhAuthoredEventStateService {
    struct RhhServiceHeader header;
    int (*object_hide_flag)(RhhContentKey map, uint32_t index, struct RhhAuthoredEventFlag *out);
    int (*hidden_item_flag)(RhhContentKey map, uint32_t index, struct RhhAuthoredEventFlag *out);
    /* v1.1: copied current save state, without executing or predicting scripts. */
    int (*dynamic_warp)(struct RhhDynamicWarp *out);
};
#define RHH_AUTHORED_EVENT_STATE_SERVICE_V1_0_SIZE offsetof(struct RhhAuthoredEventStateService, dynamic_warp)
#define RHH_AUTHORED_EVENT_STATE_SERVICE_V1_1_SIZE sizeof(struct RhhAuthoredEventStateService)
enum RhhSpeciesVisualFlags { RHH_SPECIES_VISUAL_SHINY = 1u, RHH_SPECIES_VISUAL_FEMALE = 2u,
    RHH_SPECIES_VISUAL_PLACEHOLDER = 4u };
struct RhhSpeciesVisualRequest {
    uint32_t struct_size, flags;
    RhhContentKey species;
};
struct RhhSpeciesVisual {
    uint32_t struct_size, flags, width, height, frame_count, reserved;
    RhhContentKey species;
};
struct RhhSpeciesVisualAnimCommand {
    uint32_t struct_size, animation, command, kind, flags;
    uint32_t image, duration, argument, reserved;
    RhhContentKey species;
};
#define RHH_SPECIES_VISUAL_BATCH_MAX 64u
/* "rhh:service/species_visuals" v1.2: copied source overworld sprite frames
 * and source palette for a species/form. No live actor or VRAM is required.
 * request flags accept only SHINY and FEMALE. The PLACEHOLDER result flag
 * reports the engine's fallback graphic, not the requested species' artwork.
 * Frame indices are source images, not facing or animation state. RGBA8888 is
 * tightly packed, top-left origin, with palette index zero transparent. All
 * failures leave outputs untouched. Available after module finalization. */
struct RhhSpeciesVisualService {
    struct RhhServiceHeader header;
    int (*info)(const struct RhhSpeciesVisualRequest *request, struct RhhSpeciesVisual *out);
    int (*frame_rgba)(const struct RhhSpeciesVisualRequest *request, uint32_t frame, uint8_t *rgba, uint32_t capacity);
    /* v1.1: source animation scripts, using RhhAuthoredAnimCommandKind and
     * RhhAuthoredAnimFlags, plus the engine's RhhWildActorDirection mapping. */
    int (*animation_count)(const struct RhhSpeciesVisualRequest *request, uint32_t *out);
    int (*animation_command_count)(const struct RhhSpeciesVisualRequest *request, uint32_t animation, uint32_t *out);
    int (*animation_command)(const struct RhhSpeciesVisualRequest *request, uint32_t animation, uint32_t command,
                             struct RhhSpeciesVisualAnimCommand *out);
    int (*face_animation)(const struct RhhSpeciesVisualRequest *request, uint32_t direction, uint32_t *out);
    /* v1.2: decode up to BATCH_MAX frame indices in order with one sheet
     * decompression. stride and capacity are bytes. Each image occupies
     * width*height*4 bytes; stride must be at least that size. Padding is
     * untouched. Invalid input or any failure leaves the output unchanged.
     * A zero count accepts null frame/output pointers for a valid request. */
    int (*frames_rgba)(const struct RhhSpeciesVisualRequest *request, const uint32_t *frames, uint32_t count,
                       uint8_t *rgba, uint32_t stride, uint32_t capacity);
};
#define RHH_SPECIES_VISUAL_SERVICE_V1_0_SIZE offsetof(struct RhhSpeciesVisualService, animation_count)
#define RHH_SPECIES_VISUAL_SERVICE_V1_1_SIZE offsetof(struct RhhSpeciesVisualService, frames_rgba)
#define RHH_SPECIES_VISUAL_SERVICE_V1_2_SIZE sizeof(struct RhhSpeciesVisualService)
/* Semantic habitat bits may coexist; they describe the current tile and map,
 * not a guarantee that an encounter, movement or spawn is permitted. */
enum RhhHabitatFlags {
    RHH_HABITAT_GRASS = 1u, RHH_HABITAT_TALL_GRASS = 2u, RHH_HABITAT_LONG_GRASS = 4u,
    RHH_HABITAT_SAND = 8u, RHH_HABITAT_WATER = 16u, RHH_HABITAT_SHALLOW_WATER = 32u,
    RHH_HABITAT_ICE = 64u, RHH_HABITAT_ENCOUNTER_TILE = 128u, RHH_HABITAT_UNDERGROUND = 256u,
    RHH_HABITAT_UNDERWATER = 512u, RHH_HABITAT_INDOORS = 1024u
};
#define RHH_HABITAT_FLAGS_ALL 2047u
enum RhhNaturalWeather {
    RHH_NATURAL_WEATHER_NONE, RHH_NATURAL_WEATHER_SUNNY_CLOUDS, RHH_NATURAL_WEATHER_SUNNY,
    RHH_NATURAL_WEATHER_RAIN, RHH_NATURAL_WEATHER_SNOW, RHH_NATURAL_WEATHER_THUNDERSTORM,
    RHH_NATURAL_WEATHER_FOG, RHH_NATURAL_WEATHER_ASH, RHH_NATURAL_WEATHER_SANDSTORM,
    RHH_NATURAL_WEATHER_SHADE, RHH_NATURAL_WEATHER_DROUGHT, RHH_NATURAL_WEATHER_DOWNPOUR,
    RHH_NATURAL_WEATHER_UNDERWATER, RHH_NATURAL_WEATHER_UNDERWATER_BUBBLES,
    RHH_NATURAL_WEATHER_DYNAMIC, RHH_NATURAL_WEATHER_UNKNOWN
};
enum RhhWorldLightingFlags { RHH_WORLD_LIGHT_OUTDOORS = 1u, RHH_WORLD_LIGHT_INDOORS = 2u,
    RHH_WORLD_LIGHT_UNDERGROUND = 4u, RHH_WORLD_LIGHT_UNDERWATER = 8u };
/* Copied presentation inputs. A missing clock or saved environment leaves its
 * availability flag zero and the corresponding nested snapshot all-zero. */
enum RhhWorldLocalLightKind { RHH_WORLD_LOCAL_LIGHT_UNKNOWN = 0, RHH_WORLD_LOCAL_LIGHT_LAMP,
    RHH_WORLD_LOCAL_LIGHT_POKEMON_CENTER, RHH_WORLD_LOCAL_LIGHT_MART };
enum RhhWorldLocalLightFlags { RHH_WORLD_LOCAL_LIGHT_VISIBLE = 1u, RHH_WORLD_LOCAL_LIGHT_MAP_OFFSET = 2u };
/* A live engine light sprite. Slots, tile positions and pixel coordinates are
 * frame-local; world pixels and tile coordinates are relative to the current
 * map's loaded grid. Colors and falloff are a presenter's interpretation. */
struct RhhWorldLocalLight {
    uint32_t struct_size, slot, kind, flags;
    RhhContentKey map;
    int32_t tile_x, tile_y, screen_x, screen_y, world_x, world_y;
};
struct RhhWorldLightingInputs {
    uint32_t struct_size, flags, natural_weather, engine_weather;
    RhhContentKey map;
    uint32_t time_available, environment_available;
    struct RhhWorldTimeSnapshot time;
    struct RhhWorldEnvironmentSnapshot environment;
};
#define RHH_WORLD_DYNAMIC_LIGHT_CAPACITY 64u
enum RhhWorldLightCollectionStatus { RHH_WORLD_LIGHTS_OK = 0, RHH_WORLD_LIGHTS_INVALID, RHH_WORLD_LIGHTS_DENIED };
/* Current-map pixels and absolute scene height; map is the current map key.
 * IDs are unique per frame. Radius is 1..256, rgb is nonzero 0xRRGGBB,
 * and intensity is 1..1000. Offers are transient and never persisted. */
struct RhhWorldDynamicLight {
    uint32_t struct_size, reserved;
    RhhContentKey id, map;
    int32_t x, y, z;
    uint32_t radius, rgb, intensity, reserved2;
};
struct RhhHabitatSample {
    uint32_t struct_size, base_flags, flags, permitted;
    RhhContentKey map;
    int32_t x, y;
    uint32_t natural_weather, engine_weather, environment_available, reserved;
    struct RhhWorldTile tile;
    struct RhhWorldEnvironmentSnapshot environment;
};
/* Static source tile and map weather paired with one current clock and saved
 * environment sample. sample.permitted=1 means only that the source tile is
 * decodable; it is not a live permission result. No habitat hooks, saved tile
 * edits, map scripts or live actors are applied. Header weather may differ
 * from live weather; DYNAMIC and UNKNOWN cannot predict it. */
struct RhhAuthoredHabitatSample {
    uint32_t struct_size, time_available, reserved[2];
    struct RhhAuthoredMap map;
    struct RhhHabitatSample sample;
    struct RhhWorldTimeSnapshot time;
};
/* During a poison proposal or receipt, the first query captures the player
 * habitat. If queried during a proposal, the host rechecks its availability
 * and all sampled fields before damage; a changed sample abandons that tick.
 * An unavailable sample is a successful query with available=0. A receipt
 * reuses the proposal sample, or samples at receipt time if none was requested. */
struct RhhFieldPoisonHabitatContext {
    uint32_t struct_size, available;
    struct RhhHabitatSample sample;
};
/* An exact, permitted player sample and run ID guard field transactions.
 * Capture with habitat.player(); all sampled fields must still match after
 * constraints. This includes tile, natural weather and saved environment. */
struct RhhFieldHabitatGuard {
    uint32_t struct_size, reserved;
    uint64_t run_id;
    struct RhhHabitatSample expected;
};
struct RhhFieldHabitatHealRequest {
    uint32_t struct_size, reserved;
    struct RhhFieldHabitatGuard habitat;
    struct RhhFieldHealRequest heal;
};
struct RhhFieldHabitatCureRequest {
    uint32_t struct_size, reserved;
    struct RhhFieldHabitatGuard habitat;
    struct RhhFieldStatusRequest cure;
};
#define RHH_HABITAT_SEARCH_MAX_RADIUS 4u
enum RhhHabitatSearchFlags { RHH_HABITAT_SEARCH_INCLUDE_ORIGIN = 1u };
struct RhhHabitatSearchRequest {
    uint32_t struct_size, radius, flags, reserved;
    int32_t x, y;
    uint32_t all_flags, any_flags, exclude_flags, reserved2;
};
/* Search stops at the first permitted match, in distance, y, x order. scanned
 * and denied count only in-map samples actually considered. A match does not
 * imply a path, movement permission, or spawn eligibility. */
struct RhhHabitatSearchResult {
    uint32_t struct_size, found, distance, scanned, denied, reserved[3];
    struct RhhHabitatSample sample;
};
enum RhhHabitatNeighborIndex { RHH_HABITAT_CENTER, RHH_HABITAT_NORTH, RHH_HABITAT_NORTHEAST,
    RHH_HABITAT_EAST, RHH_HABITAT_SOUTHEAST, RHH_HABITAT_SOUTH, RHH_HABITAT_SOUTHWEST,
    RHH_HABITAT_WEST, RHH_HABITAT_NORTHWEST, RHH_HABITAT_NEIGHBOR_COUNT };
/* Fixed 3x3 current-map view, indexed by RhhHabitatNeighborIndex. Present
 * distinguishes map edges from denied samples; absent entries are zeroed. */
struct RhhHabitatNeighborhood {
    uint32_t struct_size, present_mask, permitted_mask, reserved;
    struct RhhHabitatSample samples[RHH_HABITAT_NEIGHBOR_COUNT];
};
/* Weather categories preserve natural conditions that have no battle-weather
 * equivalent. CLOUDS is ordinary sunny-clouds weather, not intense sunshine. */
enum RhhHabitatWeatherKind { RHH_HABITAT_WEATHER_CLEAR, RHH_HABITAT_WEATHER_CLOUDS,
    RHH_HABITAT_WEATHER_SUN, RHH_HABITAT_WEATHER_HARSH_SUN, RHH_HABITAT_WEATHER_RAIN,
    RHH_HABITAT_WEATHER_HEAVY_RAIN, RHH_HABITAT_WEATHER_THUNDERSTORM, RHH_HABITAT_WEATHER_SNOW,
    RHH_HABITAT_WEATHER_HAIL, RHH_HABITAT_WEATHER_FOG, RHH_HABITAT_WEATHER_ASH,
    RHH_HABITAT_WEATHER_SAND, RHH_HABITAT_WEATHER_SHADE, RHH_HABITAT_WEATHER_WIND,
    RHH_HABITAT_WEATHER_UNDERWATER, RHH_HABITAT_WEATHER_DYNAMIC, RHH_HABITAT_WEATHER_UNKNOWN };
enum RhhHabitatWeatherSource { RHH_HABITAT_WEATHER_NATURAL = 1, RHH_HABITAT_WEATHER_SAVED };
struct RhhHabitatWeather {
    uint32_t struct_size, kind, source, saved_available;
};
/* Coalesced natural-weather change on the same loaded map. Engine weather is
 * a transient engine ordinal; natural weather uses RHH_NATURAL_WEATHER_*.
 * This does not itself report changes to saved weather overrides. event_key
 * is map, ordinal is sequence, and args are before/after natural weather and
 * before/after engine weather. sequence resets when a run starts or loads. */
struct RhhNaturalWeatherChange {
    uint32_t struct_size, before_natural, after_natural, before_engine, after_engine, step_available, reserved[2];
    uint64_t run_id, step, sequence;
    RhhContentKey map;
};
/* "rhh:service/habitat", version 1.7. sample is a pure, current-map query.
 * Natural weather has a stable semantic value and a transient engine ordinal;
 * it and the saved mod environment are reported separately.
 * environment_available is zero when no active saved world clock exists.
 * During RHH_HABITAT_QUERY reducers, context copies the unmodified sample;
 * decision.value contains proposed flags, and denial marks it unavailable.
 * If a reducer advances either gameplay RNG stream, both are restored and
 * the result is returned with permitted=0 and flags=0. */
struct RhhHabitatService {
    struct RhhServiceHeader header;
    int (*sample)(int32_t x, int32_t y, struct RhhHabitatSample *out);
    int (*context)(struct RhhHabitatSample *out);
    /* v1.1: bounded Manhattan search; fails without changing out for invalid
     * requests, unavailable tiles, or calls nested inside HABITAT_QUERY. */
    int (*search)(const struct RhhHabitatSearchRequest *request, struct RhhHabitatSearchResult *out);
    /* v1.2: pure resolution of a copied sample. A non-NONE saved effect
     * overrides natural weather. saved_available indicates whether absence
     * of a saved override was actually known. Does not query or mutate state. */
    int (*weather)(const struct RhhHabitatSample *sample, struct RhhHabitatWeather *out);
    /* v1.3: sample the player object's current tile on the loaded map. Fails
     * without modifying out if the player or map is unavailable, the tile is
     * outside the map, or the query is nested in a habitat callback. */
    int (*player)(struct RhhHabitatSample *out);
    /* v1.4: copied only during RHH_NATURAL_WEATHER_CHANGED observers. */
    int (*natural_weather_change)(struct RhhNaturalWeatherChange *out);
    /* v1.5: sample a 3x3 neighborhood with one shared world-time scope.
     * Changed map, weather, saved environment or sampled tiles leave output unchanged. */
    int (*neighborhood)(int32_t x, int32_t y, struct RhhHabitatNeighborhood *out);
    /* v1.6: authored baseline on any known map; never runs live habitat hooks. */
    int (*authored_sample)(RhhContentKey map, int32_t x, int32_t y, struct RhhAuthoredHabitatSample *out);
};
#define RHH_HABITAT_SERVICE_V1_0_SIZE offsetof(struct RhhHabitatService, search)
#define RHH_HABITAT_SERVICE_V1_1_SIZE offsetof(struct RhhHabitatService, weather)
#define RHH_HABITAT_SERVICE_V1_2_SIZE offsetof(struct RhhHabitatService, player)
#define RHH_HABITAT_SERVICE_V1_3_SIZE offsetof(struct RhhHabitatService, natural_weather_change)
#define RHH_HABITAT_SERVICE_V1_4_SIZE offsetof(struct RhhHabitatService, neighborhood)
#define RHH_HABITAT_SERVICE_V1_5_SIZE offsetof(struct RhhHabitatService, authored_sample)
#define RHH_HABITAT_SERVICE_V1_6_SIZE sizeof(struct RhhHabitatService)
/* v1.7 preserves the last-used ability across habitat proposals. */
#define RHH_HABITAT_SERVICE_V1_7_SIZE RHH_HABITAT_SERVICE_V1_6_SIZE
/* Transient engine step visuals; these do not change collision, encounters,
 * object cover flags, or simulation. BEGIN supports SPLASH and both TRACKS;
 * FINISH supports SPLASH, RIPPLE and DUST. Tracks are mutually exclusive. */
enum RhhGroundEffectPhase { RHH_GROUND_BEGIN = 1, RHH_GROUND_FINISH };
enum RhhGroundEffectBits { RHH_GROUND_SPLASH = 1u, RHH_GROUND_RIPPLE = 2u,
    RHH_GROUND_TRACKS = 4u, RHH_GROUND_DEEP_TRACKS = 8u, RHH_GROUND_DUST = 16u };
enum RhhGroundActorFlags { RHH_GROUND_ACTOR_PLAYER = 1u, RHH_GROUND_ACTOR_WILD = 2u,
    RHH_GROUND_ACTOR_INVISIBLE = 4u, RHH_GROUND_ACTOR_OFFSCREEN = 8u,
    RHH_GROUND_ACTOR_LANDING_JUMP = 16u, RHH_GROUND_ACTOR_DIRECTION_OVERWRITE = 32u,
    RHH_GROUND_ACTOR_UNLINKED = 64u };
struct RhhGroundEffectRequest { uint32_t struct_size, slot, phase, base; };
struct RhhGroundEffectResult {
    uint32_t struct_size, phase, base, value, permitted, is_preview, slot, actor_flags;
    uint64_t wild_handle;
    uint32_t current_available, previous_available;
    struct RhhHabitatSample current, previous;
};
/* "rhh:service/ground_effect", v1.0. evaluate is a copied preview; context
 * exists during RHH_WORLD_GROUND_EFFECT. decision.value is a combination of
 * the phase's valid bits. Denied or invalid proposals preserve engine visuals.
 * Invisible, offscreen, concealed or unlinked actors cannot gain new effects;
 * jump and track safety filters remain authoritative. Evaluation never
 * advances gameplay RNG. */
struct RhhGroundEffectService {
    struct RhhServiceHeader header;
    int (*evaluate)(const struct RhhGroundEffectRequest *, struct RhhGroundEffectResult *out);
    int (*context)(struct RhhGroundEffectResult *out);
};
/* Current-map berry plots are copied from the engine's active object
 * templates. Indices are transient; map, local ID, tree ID, coordinates and
 * the opaque state token together form an optimistic concurrency check. */
enum RhhBerryPlotFlags { RHH_BERRY_PLOT_WATERABLE = 1u, RHH_BERRY_PLOT_MOISTURE = 2u,
    RHH_BERRY_PLOT_ALWAYS_WATERABLE = 4u, RHH_BERRY_PLOT_GROWTH_STOPPED = 8u };
enum RhhBerryStage { RHH_BERRY_EMPTY = 0, RHH_BERRY_PLANTED, RHH_BERRY_SPROUTED, RHH_BERRY_TALLER,
    RHH_BERRY_FLOWERING, RHH_BERRY_FRUIT, RHH_BERRY_TRUNK, RHH_BERRY_BUDDING };
enum RhhBerryWaterStatus { RHH_BERRY_WATER_OK, RHH_BERRY_WATER_INVALID, RHH_BERRY_WATER_UNAVAILABLE,
    RHH_BERRY_WATER_STALE, RHH_BERRY_WATER_NOT_NEEDED, RHH_BERRY_WATER_DENIED };
enum RhhBerryWaterFlags { RHH_BERRY_WATER_REQUIRE_WEATHER = 1u, RHH_BERRY_WATER_REQUIRE_TERRAIN = 2u,
    RHH_BERRY_WATER_REQUIRE_NATURAL_WEATHER = 4u, RHH_BERRY_WATER_REQUIRE_OUTDOORS = 8u };
struct RhhBerryPlot {
    uint32_t struct_size, index, local_id, tree_id, flags, berry, stage, watered;
    uint32_t moisture, minutes_to_stage, yield, regrowth, habitat_available, reserved;
    int32_t x, y;
    RhhContentKey map, berry_item;
    uint8_t state_token[8]; /* exact opaque engine record; use only for a fresh checked command */
    struct RhhHabitatSample habitat;
};
struct RhhBerryWaterRequest {
    uint32_t struct_size, flags, expected_weather, expected_terrain;
    RhhContentKey owner;
    struct RhhBerryPlot expected;
};
struct RhhBerryWaterResult {
    uint32_t struct_size, status, is_preview, changed;
    RhhContentKey owner;
    struct RhhBerryPlot before, after;
    char reason[96];
};
/* Successful engine watering interaction, including an unchanged repeat.
 * The event key is the map, subject is the berry item and args are tree ID,
 * stage, changed and object-event slot. It is not a durable receipt. */
struct RhhBerryManualWater {
    uint32_t struct_size, changed, object_slot, reserved;
    uint64_t run_id;
    struct RhhBerryPlot before, after;
};
/* Care details are decoded from the copied plot token. mulch_ordinal is an
 * engine value; mulch_item is a stable key when it names a known mulch. */
struct RhhBerryPlotCare {
    uint32_t struct_size, weeds, pests, mulch_ordinal;
    RhhContentKey mulch_item;
    struct RhhBerryPlot plot;
};
enum RhhBerryInteractionCause { RHH_BERRY_INTERACTION_PLANTED = 1, RHH_BERRY_INTERACTION_REMOVED,
    RHH_BERRY_INTERACTION_MULCHED, RHH_BERRY_INTERACTION_WEED_CLEARED, RHH_BERRY_INTERACTION_PESTS_CLEARED };
/* A supported object interaction, not a bag receipt or durable event. */
struct RhhBerryPlotInteraction {
    uint32_t struct_size, cause, object_slot, changed;
    uint64_t run_id;
    struct RhhBerryPlotCare before, after;
};
/* Bag additions accepted while picking a verified current-map tree. The tree
 * is still present during this event; removal is a separate interaction.
 * This is not a durable or exactly-once receipt. */
struct RhhBerryHarvest {
    uint32_t struct_size, object_slot, berry_count, mutation_count;
    uint64_t run_id;
    RhhContentKey berry_item, mutation_item;
    struct RhhBerryPlotCare plot;
};
/* A copied fruit-stage yield calculation. value is the quantity used for
 * the ordinary berry item; mutation berries follow the engine's rules. */
struct RhhBerryYield {
    uint32_t struct_size, base, value, accepted;
    uint64_t run_id;
    struct RhhBerryPlotCare plot;
};
/* Authored object placement paired with the current run's stored tree record.
 * This is not a live object, a visibility result, or a checked command token. */
enum RhhAuthoredBerryFlags { RHH_AUTHORED_BERRY_UNKNOWN_ITEM = 1u };
struct RhhAuthoredBerryPlot {
    uint32_t struct_size, object_index, local_id, tree_id, elevation, flags;
    int32_t x, y;
    uint32_t berry, stage, watered, moisture, minutes_to_stage, yield, regrowth;
    uint32_t growth_stopped, weeds, pests, mulch_ordinal;
    uint64_t run_id;
    RhhContentKey map, berry_item, mulch_item;
};
/* An authored object addresses a global saved tree slot, which may be shared
 * by other objects. This token is transient; it is not a visibility result. */
struct RhhAuthoredBerrySnapshot {
    uint32_t struct_size, flags;
    struct RhhAuthoredBerryPlot plot;
    uint8_t state_token[8];
};
struct RhhAuthoredBerryWaterRequest {
    uint32_t struct_size, flags, expected_weather, expected_terrain;
    RhhContentKey owner;
    struct RhhAuthoredBerrySnapshot expected;
};
struct RhhAuthoredBerryWaterResult {
    uint32_t struct_size, status, is_preview, changed;
    RhhContentKey owner;
    struct RhhAuthoredBerrySnapshot before, after;
    char reason[96];
};
/* One coalesced change to a global saved tree slot, not an authored map
 * object. More than one authored object may refer to the same slot. */
struct RhhBerryClockState {
    uint32_t berry, stage, watered, moisture, moisture_clock, minutes_to_stage;
    uint32_t yield, regrowth, growth_stopped, weeds, pests, mulch_ordinal, mutation_a, mutation_b;
    RhhContentKey berry_item, mulch_item;
    uint8_t state_token[8];
};
struct RhhBerryClockChange {
    uint32_t struct_size, tree_id;
    struct RhhBerryClockState before, after;
};
struct RhhBerryClockUpdate {
    uint32_t struct_size, elapsed_minutes, change_count, reserved;
    uint64_t run_id;
};
/* Static authored objects that reference one global tree slot. Pages use
 * engine map-group, map-number and object order; they are not live objects. */
#define RHH_BERRY_REFERENCE_PAGE_MAX 16u
struct RhhBerryReference {
    RhhContentKey map;
    uint32_t object_index, local_id, elevation, reserved;
    int32_t x, y;
};
struct RhhBerryReferenceRequest {
    uint32_t struct_size, tree_id, first, limit;
};
struct RhhBerryReferencePage {
    uint32_t struct_size, tree_id, first, count, next, total, unavailable_maps, reserved;
    struct RhhBerryReference references[RHH_BERRY_REFERENCE_PAGE_MAX];
};
/* "rhh:service/berry_plot" v1.0: count/at enumerate the current map;
 * preview and water use the engine's normal watering rules on a checked
 * snapshot. Constraints and committed observers can copy context. Automated
 * watering never repeats while the current stage is already watered or the
 * moisture system says the soil does not need it. Optional weather/terrain
 * requirements are rechecked before commit against the saved map effects.
 * Version 1.1 adds natural-weather and outdoor requirements; the expected
 * natural weather comes from expected.habitat.natural_weather. v1.2 adds a
 * separate observer for supported ordinary watering interactions. v1.3
 * adds care snapshots and committed object-interaction observations. v1.4
 * reports successful ordinary harvest bag additions. v1.5 provides a pure,
 * bounded ordinary-berry yield proposal and a matching read-only preview.
 * v1.6 pairs a specific authored tree object on any map with stored run state.
 * v1.7 adds checked watering of an unloaded map's stored tree slot. Its
 * separate constraint and notification carry map as event_key, berry item as
 * subject and tree ID, stage and preview flag in args 0..2. Other authored
 * objects referencing that same global slot also see the change. v1.8
 * reports a copied batch after a supported game-clock berry update. v1.9
 * scans authored maps for references to one global tree slot. */
struct RhhBerryPlotService {
    struct RhhServiceHeader header;
    int (*count)(uint32_t *out);
    int (*at)(uint32_t index, struct RhhBerryPlot *out);
    int (*preview)(const struct RhhBerryWaterRequest *, struct RhhBerryWaterResult *out);
    int (*water)(const struct RhhBerryWaterRequest *, struct RhhBerryWaterResult *out);
    int (*context)(struct RhhBerryWaterResult *out);
    /* v1.2: only during RHH_BERRY_PLOT_MANUALLY_WATERED. */
    int (*manual_water_context)(struct RhhBerryManualWater *out);
    int (*care_at)(uint32_t index, struct RhhBerryPlotCare *out);
    int (*interaction_context)(struct RhhBerryPlotInteraction *out);
    /* v1.4: only during RHH_BERRY_PLOT_HARVESTED. */
    int (*harvest_context)(struct RhhBerryHarvest *out);
    /* v1.5: preview on a current-map plot, or copy the active yield context. */
    int (*yield_preview)(uint32_t index, struct RhhBerryYield *out);
    int (*yield_context)(struct RhhBerryYield *out);
    /* v1.6: index is an authored object index, not a current-map plot index. */
    int (*authored_at)(RhhContentKey map, uint32_t object_index, struct RhhAuthoredBerryPlot *out);
    /* v1.7: checked unloaded-map operations; flags accept only saved weather
     * and terrain requirements. These do not simulate natural weather. */
    int (*authored_snapshot)(RhhContentKey map, uint32_t object_index, struct RhhAuthoredBerrySnapshot *out);
    int (*preview_authored_water)(const struct RhhAuthoredBerryWaterRequest *, struct RhhAuthoredBerryWaterResult *out);
    int (*water_authored)(const struct RhhAuthoredBerryWaterRequest *, struct RhhAuthoredBerryWaterResult *out);
    int (*authored_water_context)(struct RhhAuthoredBerryWaterResult *out);
    /* v1.8: copied batch and indexed slot changes during BERRY_CLOCK_UPDATED. */
    int (*clock_context)(struct RhhBerryClockUpdate *out);
    int (*clock_change)(uint32_t index, struct RhhBerryClockChange *out);
    /* v1.9: potentially expensive scan; unavailable_maps reports source maps
     * with unreadable object data, so an empty result may be incomplete. */
    int (*references)(const struct RhhBerryReferenceRequest *, struct RhhBerryReferencePage *out);
};
#define RHH_BERRY_PLOT_SERVICE_V1_1_SIZE offsetof(struct RhhBerryPlotService, manual_water_context)
#define RHH_BERRY_PLOT_SERVICE_V1_0_SIZE RHH_BERRY_PLOT_SERVICE_V1_1_SIZE
#define RHH_BERRY_PLOT_SERVICE_V1_2_SIZE offsetof(struct RhhBerryPlotService, care_at)
#define RHH_BERRY_PLOT_SERVICE_V1_3_SIZE offsetof(struct RhhBerryPlotService, harvest_context)
#define RHH_BERRY_PLOT_SERVICE_V1_4_SIZE offsetof(struct RhhBerryPlotService, yield_preview)
#define RHH_BERRY_PLOT_SERVICE_V1_5_SIZE offsetof(struct RhhBerryPlotService, authored_at)
#define RHH_BERRY_PLOT_SERVICE_V1_6_SIZE offsetof(struct RhhBerryPlotService, authored_snapshot)
#define RHH_BERRY_PLOT_SERVICE_V1_7_SIZE offsetof(struct RhhBerryPlotService, clock_context)
#define RHH_BERRY_PLOT_SERVICE_V1_8_SIZE offsetof(struct RhhBerryPlotService, references)
#define RHH_BERRY_PLOT_SERVICE_V1_9_SIZE sizeof(struct RhhBerryPlotService)
/* Named, declarative predicates over a copied habitat sample. Zero masks are
 * wildcards. Period bits use (RHH_TIME_* - 1); weather and terrain bits use
 * their enum values, including bit zero for NONE. A missing time or saved
 * environment cannot satisfy a predicate that requires it. Version 1.1 of the
 * habitat registry uses the former reserved2 field for an optional mask of
 * RHH_HABITAT_WEATHER_* kinds after saved-over-natural resolution. Such a
 * predicate requires saved-environment availability. Declare a requirement on
 * registry v1.1 when using it; old hosts reject nonzero reserved2. */
struct RhhModHabitat {
    uint32_t struct_size, reserved;
    const char *id, *label, *description;
    RhhContentKey map; /* zero matches any map */
    uint32_t all_flags, any_flags, exclude_flags, period_mask;
    uint32_t natural_weather_mask, weather_mask, terrain_mask;
    union { uint32_t reserved2; uint32_t effective_weather_mask; };
};
struct RhhHabitatDefinition {
    uint32_t struct_size, reserved;
    RhhContentKey key, map;
    uint32_t all_flags, any_flags, exclude_flags, period_mask;
    uint32_t natural_weather_mask, weather_mask, terrain_mask, reserved2;
    char id[RHH_MOD_ID_MAX], label[64], description[128];
};
enum RhhHabitatEvaluationStatus { RHH_HABITAT_EVALUATION_MATCH = 1,
    RHH_HABITAT_EVALUATION_MISMATCH, RHH_HABITAT_EVALUATION_INDETERMINATE };
enum RhhHabitatEvaluationReason {
    RHH_HABITAT_REASON_PERMISSION = 1u, RHH_HABITAT_REASON_MAP = 2u,
    RHH_HABITAT_REASON_ALL_FLAGS = 4u, RHH_HABITAT_REASON_ANY_FLAGS = 8u,
    RHH_HABITAT_REASON_EXCLUDED_FLAGS = 16u, RHH_HABITAT_REASON_PERIOD = 32u,
    RHH_HABITAT_REASON_NATURAL_WEATHER = 64u, RHH_HABITAT_REASON_SAVED_WEATHER = 128u,
    RHH_HABITAT_REASON_SAVED_TERRAIN = 256u, RHH_HABITAT_REASON_EFFECTIVE_WEATHER = 512u
};
/* Known mismatches take precedence over unavailable inputs. Both reason masks
 * can be nonzero. Availability only describes the supplied sample; this is
 * not a prediction of a live map or a guarantee of movement or spawning. */
struct RhhHabitatEvaluation {
    uint32_t struct_size, status, mismatch, unavailable;
};
/* "rhh:service/habitat_registry" v1.3. Definitions are sorted by ID;
 * match is pure and uses the provided sample and period (zero means unknown).
 * Unpermitted samples never match. Failed queries leave outputs unchanged. */
struct RhhHabitatRegistryService {
    struct RhhServiceHeader header;
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhHabitatDefinition *out);
    int (*find)(RhhContentKey key, struct RhhHabitatDefinition *out);
    int (*match)(RhhContentKey key, const struct RhhHabitatSample *sample,
                 uint32_t period, uint32_t *out);
    /* v1.1: copied effective-weather mask of a registered habitat. */
    int (*effective_weather_mask)(RhhContentKey key, uint32_t *out);
    /* v1.2: explain a match, distinguishing a known mismatch from missing
     * required period or saved environment. Failed calls leave out unchanged. */
    int (*evaluate)(RhhContentKey key, const struct RhhHabitatSample *sample,
                    uint32_t period, struct RhhHabitatEvaluation *out);
    /* v1.3: evaluate an authored baseline, treating dynamic/unknown natural
     * weather as unavailable unless a saved effect determines effective weather. */
    int (*evaluate_authored)(RhhContentKey key, const struct RhhAuthoredHabitatSample *sample,
                             struct RhhHabitatEvaluation *out);
};
#define RHH_HABITAT_REGISTRY_SERVICE_V1_0_SIZE offsetof(struct RhhHabitatRegistryService, effective_weather_mask)
#define RHH_HABITAT_REGISTRY_SERVICE_V1_1_SIZE offsetof(struct RhhHabitatRegistryService, evaluate)
#define RHH_HABITAT_REGISTRY_SERVICE_V1_2_SIZE offsetof(struct RhhHabitatRegistryService, evaluate_authored)
#define RHH_HABITAT_REGISTRY_SERVICE_V1_3_SIZE sizeof(struct RhhHabitatRegistryService)
/* Shared, declarative species traits. Associations are additive; they do not
 * themselves change spawning or behavior. References are validated at finalization. */
struct RhhModEcologyTag {
    uint32_t struct_size, reserved;
    const char *id, *label, *description;
};
struct RhhModSpeciesTag {
    uint32_t struct_size, reserved;
    RhhContentKey species, tag;
};
struct RhhEcologyTag {
    uint32_t struct_size, reserved;
    RhhContentKey key;
    char id[RHH_MOD_ID_MAX], label[64], description[128];
};
/* "rhh:service/ecology_tags" v1.0. Enumeration is sorted by tag ID. Species
 * aliases resolve to the same associations; duplicate contributions collapse.
 * Failed queries leave outputs unchanged. */
struct RhhEcologyTagsService {
    struct RhhServiceHeader header;
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhEcologyTag *out);
    int (*find)(RhhContentKey key, struct RhhEcologyTag *out);
    int (*species_count)(RhhContentKey species, uint32_t *out);
    int (*species_at)(RhhContentKey species, uint32_t index, struct RhhEcologyTag *out);
    int (*species_has)(RhhContentKey species, RhhContentKey tag, uint32_t *out);
};
enum RhhWildArea { RHH_WILD_AREA_UNKNOWN = 0, RHH_WILD_LAND = 1, RHH_WILD_WATER, RHH_WILD_ROCKS, RHH_WILD_FISHING, RHH_WILD_HIDDEN };
enum RhhWildSource { RHH_WILD_STANDARD = 1, RHH_WILD_OVERWORLD_ACTOR, RHH_WILD_FISHING_ROD, RHH_WILD_FEEBAS,
    RHH_WILD_ROAMER, RHH_WILD_OUTBREAK, RHH_WILD_OVERWORLD_ROAMER,
    RHH_WILD_OVERWORLD_OUTBREAK, RHH_WILD_OVERWORLD_FEEBAS, RHH_WILD_DEXNAV,
    RHH_WILD_PIKE, RHH_WILD_PYRAMID, RHH_WILD_OVERWORLD_PIKE, RHH_WILD_OVERWORLD_PYRAMID,
    RHH_WILD_SCRIPTED, RHH_WILD_MANUAL_PIKE, RHH_WILD_MANUAL_PYRAMID,
    RHH_WILD_GHOST, RHH_WILD_TUTORIAL, RHH_WILD_FIRST_BATTLE };
enum RhhWildTime { RHH_WILD_TIME_UNKNOWN, RHH_WILD_TIME_MORNING, RHH_WILD_TIME_DAY,
    RHH_WILD_TIME_EVENING, RHH_WILD_TIME_NIGHT };
/* Authored slots are static source data. species is a stable key when the
 * engine species has one; species_ordinal is the transient source value.
 * Fishing slots contain all rods' entries, not one rod's probability table. */
#define RHH_AUTHORED_WILD_SLOT_CAPACITY 12u
struct RhhAuthoredWildSlot {
    uint32_t index, min_level, max_level, species_ordinal;
    RhhContentKey species;
};
struct RhhAuthoredWildTable {
    uint32_t struct_size, area, time, variant, present, encounter_rate, slot_count, reserved;
    RhhContentKey map;
    struct RhhAuthoredWildSlot slots[RHH_AUTHORED_WILD_SLOT_CAPACITY];
};
/* Configured slot intervals for a single area and, for fishing, one rod.
 * weights use original table indices; entries outside the selected range are
 * zero. These are nominal relative weights before lure, abilities, hooks and
 * other live rules, not an exact probability for an encounter or RNG result. */
struct RhhAuthoredWildWeights {
    uint32_t struct_size, area, rod, available, first_slot, slot_count, total, reserved;
    uint32_t weights[RHH_AUTHORED_WILD_SLOT_CAPACITY];
};
enum RhhAuthoredWildSelectionFlags { RHH_AUTHORED_WILD_TIME_DISABLED = 1u,
    RHH_AUTHORED_WILD_TIME_FALLBACK = 2u, RHH_AUTHORED_WILD_TIME_REMAPPED = 4u,
    RHH_AUTHORED_WILD_VARIANT_CLAMPED = 8u };
/* Current saved ordinary-map variant and time-table policy for a caller's
 * explicit period. An absent map header has has_header=0 and a zero table.
 * This does not model facility, scripted, roaming or other special sources. */
struct RhhAuthoredWildSelection {
    uint32_t struct_size, requested_time, selected_time, area;
    uint32_t has_header, variant, variant_count, flags;
    RhhContentKey map;
    uint64_t run_id;
    struct RhhAuthoredWildTable table;
};
/* Hypothetical, unloaded-map distribution. This uses authored slots and
 * habitat, not randomization, lead abilities, lure, scripts or live actors. */
struct RhhAuthoredWildPreviewRequest {
    uint32_t struct_size, area, time, rod;
    int32_t x, y;
    uint32_t reserved[2];
    RhhContentKey map;
    uint64_t ordinal;
};
struct RhhAuthoredWildPreview {
    uint32_t struct_size, available, fallback, total;
    struct RhhAuthoredWildPreviewRequest request;
    struct RhhAuthoredWildSelection selection;
    struct RhhAuthoredHabitatSample habitat;
    struct RhhAuthoredWildWeights weights;
};
/* During RHH_AUTHORED_WILD_WEIGHT, event_key is map, subject is the
 * authored species and ordinal is caller-supplied. decision.value begins at
 * base_weight; transforms propose 0..RHH_WILD_MAX_WEIGHT and constraints may
 * deny a slot. The copied context exists only during that proposal. */
struct RhhAuthoredWildWeightContext {
    uint32_t struct_size, slot, base_weight, reserved;
    uint32_t selected_time, variant, selection_flags, reserved2;
    struct RhhAuthoredWildPreviewRequest request;
    struct RhhAuthoredWildSlot candidate;
    struct RhhAuthoredHabitatSample habitat;
};
/* "rhh:service/authored_encounters" v1.3. variants reports the number of
 * source headers for a known map, including zero. table selects one header in
 * source order and an exact RhhWildTime and RhhWildArea; a missing table is a
 * successful result with present=0. Morning is also the engine's default
 * table. Those queries do not select saved variants, perform time fallback,
 * apply randomization/hooks or simulate encounters. Failed calls leave outputs
 * untouched; no run or loaded map is required once modules are finalized. */
struct RhhAuthoredEncounterService {
    struct RhhServiceHeader header;
    int (*variants)(RhhContentKey map, uint32_t *out);
    int (*table)(RhhContentKey map, uint32_t variant, uint32_t time, uint32_t area,
                 struct RhhAuthoredWildTable *out);
    /* v1.1: source interval weights, independent of map/time. Fishing needs a
     * RhhWildRod; other areas require NONE. A hidden chooser absent from this
     * build returns available=0. Failed queries leave the output unchanged. */
    int (*base_weights)(uint32_t area, uint32_t rod, struct RhhAuthoredWildWeights *out);
    /* v1.2: resolve the ordinary saved variant and time-table policy for an
     * explicit period, without loading the target map or sampling the clock.
     * Requires an active run and save, outside a save transaction. */
    int (*selection)(RhhContentKey map, uint32_t time, uint32_t area, struct RhhAuthoredWildSelection *out);
    /* v1.3: pure authored distribution at one map tile and explicit ordinal.
     * Missing tables or unavailable weights return available=0. If every
     * proposal is zero the engine's baseline weights are returned with
     * fallback=1. Failed calls leave output unchanged. */
    int (*preview)(const struct RhhAuthoredWildPreviewRequest *, struct RhhAuthoredWildPreview *out);
    int (*preview_context)(struct RhhAuthoredWildWeightContext *out);
};
#define RHH_AUTHORED_ENCOUNTER_SERVICE_V1_0_SIZE offsetof(struct RhhAuthoredEncounterService, base_weights)
#define RHH_AUTHORED_ENCOUNTER_SERVICE_V1_1_SIZE offsetof(struct RhhAuthoredEncounterService, selection)
#define RHH_AUTHORED_ENCOUNTER_SERVICE_V1_2_SIZE offsetof(struct RhhAuthoredEncounterService, preview)
#define RHH_AUTHORED_ENCOUNTER_SERVICE_V1_3_SIZE sizeof(struct RhhAuthoredEncounterService)
enum RhhWildPhase { RHH_WILD_PHASE_PROPOSAL = 1, RHH_WILD_PHASE_CREATED, RHH_WILD_PHASE_CANCELLED };
/* A requested actor has no encounter-table slot. In its wild context,
 * table_species is the requested species and slot is this sentinel. */
#define RHH_WILD_NO_TABLE_SLOT UINT32_MAX
enum RhhScriptedEncounterPreparation { RHH_SCRIPTED_WILD_COMMAND = 1, RHH_SCRIPTED_EVENT_MON,
    RHH_SCRIPTED_DIRECT_SPECIAL };
struct RhhScriptedEncounterSource {
    uint32_t struct_size, preparation;
    RhhContentKey encounter;
};
/* The generated identity is OT ID << 32 | personality. It can collide and is
 * not a durable actor identifier. habitat is valid only when habitat_available. */
struct RhhWildEncounterContext {
    uint32_t struct_size, phase, area, source, slot, level, table_time, habitat_available;
    int32_t x, y; /* Map-local candidate tile; may be negative or unavailable. */
    RhhContentKey map, table_species, initial_species, species;
    uint64_t ordinal, identity;
    struct RhhHabitatSample habitat;
};
/* Available only during a wild cancellation notification. */
struct RhhWildEncounterCancellation {
    uint32_t struct_size, reserved;
    struct RhhWildEncounterContext candidate;
    char reason[96];
};
#define RHH_WILD_MAX_WEIGHT 4095u
enum RhhWildRod { RHH_WILD_ROD_NONE, RHH_WILD_ROD_OLD, RHH_WILD_ROD_GOOD, RHH_WILD_ROD_SUPER };
struct RhhWildWeightContext {
    uint32_t struct_size, area, source, slot, rod, base_weight, table_time, habitat_available;
    uint32_t min_level, max_level, first_slot, slot_count;
    int32_t x, y;
    RhhContentKey map, table_species, candidate_species;
    uint64_t ordinal;
    struct RhhHabitatSample habitat;
};
#define RHH_WILD_RATE_MAX 2880u
/* The engine compares Random() % RHH_WILD_RATE_MAX with the threshold. The
 * base is after bike, flute, held-item, lure and lead-ability adjustments. */
struct RhhWildRateContext {
    uint32_t struct_size, area, source, table_time, table_rate, base_rate, habitat_available, ignore_ability;
    int32_t x, y;
    RhhContentKey map;
    uint64_t step;
    struct RhhHabitatSample habitat;
};
#define RHH_WILD_SPAWN_MAX_FRAMES 254u
enum RhhWildSpawnReset { RHH_WILD_SPAWN_MINIMUM = 1, RHH_WILD_SPAWN_RECALCULATED };
enum RhhWildSpawnFlags { RHH_WILD_SPAWN_LURE = 1u, RHH_WILD_SPAWN_REPLACEMENT = 2u };
struct RhhWildSpawnIntervalContext {
    uint32_t struct_size, cause, area, base_frames, active_count, capacity, flags, habitat_available;
    int32_t x, y;
    RhhContentKey map;
    uint64_t step;
    struct RhhHabitatSample habitat;
};
/* Fishing performs an optional legacy Sticky Hold/Suction Cups roll before
 * its ordinary bite roll. The event runs independently for each roll that
 * actually occurs; percentages are 0..100. The habitat is the facing tile. */
enum RhhFishingBitePhase { RHH_FISHING_BITE_REGULAR = 1, RHH_FISHING_BITE_STICKY };
struct RhhFishingBiteContext {
    uint32_t struct_size, phase, rod, base_percent, sticky_ability, habitat_available, reserved[2];
    int32_t x, y;
    RhhContentKey map;
    uint64_t step;
    struct RhhHabitatSample habitat;
};
/* RHH_FISHING_BITE_RATE transforms decision.value in 0..100. Invalid or
 * denied proposals retain the engine probability; a zero proposal still
 * consumes the normal random roll. Context and callbacks are read-only. */
/* RHH_WILD_ENCOUNTER_RATE transforms the final engine threshold, in the range
 * 0..RHH_WILD_RATE_MAX; zero explicitly disables the roll. The event key is
 * the current map, ordinal is the saved world step, and args contain area,
 * source, original threshold and table time. Invalid or denied proposals use
 * the original threshold. The normal RNG roll still happens once. */
/* RHH_WILD_SPAWN_INTERVAL transforms an overworld actor spawn countdown when
 * the engine resets it. Values are frames in 0..254; 255 is an engine
 * sentinel. The event key is the map, ordinal is the world step, and args
 * are area, cause, base_frames and flags. Denied or invalid proposals keep
 * base_frames. The engine still controls placement, availability, repel,
 * species generation and limits. Changes apply at the next countdown reset. */
/* RHH_WILD_SCRIPTED_CANDIDATE proposes a species for each nonempty operand
 * of the setwildbattle script command before any opponent is generated.
 * decision.value is a species key; denial or an invalid key keeps the authored
 * species. This is not a battle cancellation and does not cover separately
 * generated legendary, roamer, ghost or tutorial encounters. The copied
 * context identifies the authored species, level, party slot and current map;
 * the actor tile and habitat are unknown. The script's levels and items stay
 * unchanged, and normal story scripts retain control of the battle. */
/* v1.10 adds separate permission, generated and cancellation events for
 * manually placed Pike/Pyramid actors. These run at engagement after the
 * engine has prepared the actual opponent, not when its actor was placed.
 * Denial restores the enemy party and both RNG streams, leaves the actor in
 * place, and resumes the interaction without starting battle. Depending on
 * facility rules, the prepared opponent may differ from the visible actor. */
/* v1.12 adds an opt-in permission check for explicitly guarded scripted wild
 * preparations. It checks each prepared opponent in party order, after any
 * scripted species replacement and generation. A veto restores the enemy
 * party and both RNG streams to before preparation and emits a cancellation
 * notification before the script's cutscene cleanup and unlock. Only scripts
 * with an explicit cancellation branch use this
 * check; other scripts retain their behavior.
 * The copied context identifies the actual prepared species, level, slot,
 * OT/personality identity and map; area, tile and habitat are unknown. */
/* v1.13 adds RHH_WILD_EVENT_CANDIDATE for seteventmon. This separate
 * transform proposes the species before event-mon generation; denied or
 * invalid keys retain the authored species. Level, held item and the fateful
 * encounter flag retain their normal behavior. Explicitly guarded event-mon
 * scripts use the separate RHH_CAN_BEGIN_EVENT_WILD constraint and
 * RHH_WILD_EVENT_CANCELLED notification, with the same rollback contract as
 * the v1.12 scripted permission and cancellation events. */
/* v1.14 adds separate cleanup-finished observers for scripted and event-mon
 * cancellations. The first denied guarded preparation in a global script
 * is reported after that script explicitly ends and player controls unlock.
 * The same copied cancellation context and reason are available. Script
 * replacement, a changed run or map, and non-explicit termination discard
 * the pending notification. Script authors remain responsible for cleanup. */
/* v1.15 adds scripted_source, a copied, stable authored key for annotated
 * script preparations. It is scoped to scripted candidate, permission,
 * cancellation, cleanup and battle-ready callbacks, and fails for unmarked
 * or unrelated events. Keys name authored encounter sites, not individuals. */
/* v1.16 adds scripted_battle_source, available during an annotated scripted
 * battle from its start notification through its end notification. */
/* v1.17 adds separate ghost and catching-tutorial ready notifications. */
/* v1.18 adds a separate ready notification for Birch's first battle. */
/* v1.19 adds separate catching-tutorial permission, cancellation and
 * cleanup-finished events. The guard sees the prepared opponent with source
 * RHH_WILD_TUTORIAL. Denial restores the pre-preparation enemy party and RNG;
 * the immediate callback precedes the script's cleanup. Wally's temporary
 * party is restored by that script before cleanup-finished is delivered. */
/* "rhh:service/wild_encounter", version 1.20. context is available during
 * candidate, normal/special permission, generated and cancelled callbacks.
 * weight_context was appended in 1.1; cancellation_context in 1.2. v1.3 adds
 * separate special permission and notification events without changing the
 * structure or delivering new source kinds to legacy event subscriptions.
 * v1.4 adds separate DexNav events; v1.5 adds separate facility events;
 * v1.6 adds an observational scripted-battle-ready event. v1.7 adds the
 * rate hook and its copied context for standard and facility checks. v1.8
 * adds the overworld actor spawn interval hook and copied context; v1.9 adds
 * the separate scripted-candidate proposal. */
struct RhhWildEncounterService {
    struct RhhServiceHeader header;
    int (*context)(struct RhhWildEncounterContext *out);
    int (*weight_context)(struct RhhWildWeightContext *out);
    int (*cancellation_context)(struct RhhWildEncounterCancellation *out);
    int (*rate_context)(struct RhhWildRateContext *out);
    int (*spawn_interval_context)(struct RhhWildSpawnIntervalContext *out);
    int (*fishing_bite_context)(struct RhhFishingBiteContext *out);
    int (*scripted_source)(struct RhhScriptedEncounterSource *out);
    int (*scripted_battle_source)(struct RhhScriptedEncounterSource *out);
};
#define RHH_WILD_ENCOUNTER_SERVICE_V1_0_SIZE offsetof(struct RhhWildEncounterService, weight_context)
#define RHH_WILD_ENCOUNTER_SERVICE_V1_1_SIZE offsetof(struct RhhWildEncounterService, cancellation_context)
#define RHH_WILD_ENCOUNTER_SERVICE_V1_2_SIZE offsetof(struct RhhWildEncounterService, rate_context)
#define RHH_WILD_ENCOUNTER_SERVICE_V1_3_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_2_SIZE
#define RHH_WILD_ENCOUNTER_SERVICE_V1_4_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_2_SIZE
#define RHH_WILD_ENCOUNTER_SERVICE_V1_5_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_2_SIZE
#define RHH_WILD_ENCOUNTER_SERVICE_V1_6_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_2_SIZE
#define RHH_WILD_ENCOUNTER_SERVICE_V1_7_SIZE offsetof(struct RhhWildEncounterService, spawn_interval_context)
#define RHH_WILD_ENCOUNTER_SERVICE_V1_8_SIZE offsetof(struct RhhWildEncounterService, fishing_bite_context)
#define RHH_WILD_ENCOUNTER_SERVICE_V1_9_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_8_SIZE
#define RHH_WILD_ENCOUNTER_SERVICE_V1_10_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_8_SIZE
#define RHH_WILD_ENCOUNTER_SERVICE_V1_11_SIZE offsetof(struct RhhWildEncounterService, scripted_source)
#define RHH_WILD_ENCOUNTER_SERVICE_V1_12_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_11_SIZE
#define RHH_WILD_ENCOUNTER_SERVICE_V1_13_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_11_SIZE
#define RHH_WILD_ENCOUNTER_SERVICE_V1_14_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_11_SIZE
#define RHH_WILD_ENCOUNTER_SERVICE_V1_15_SIZE offsetof(struct RhhWildEncounterService, scripted_battle_source)
#define RHH_WILD_ENCOUNTER_SERVICE_V1_16_SIZE sizeof(struct RhhWildEncounterService)
#define RHH_WILD_ENCOUNTER_SERVICE_V1_17_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_16_SIZE
#define RHH_WILD_ENCOUNTER_SERVICE_V1_18_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_17_SIZE
#define RHH_WILD_ENCOUNTER_SERVICE_V1_19_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_18_SIZE
/* v1.20 preserves gameplay RNG across weight, rate, interval and candidate proposals. */
#define RHH_WILD_ENCOUNTER_SERVICE_V1_20_SIZE RHH_WILD_ENCOUNTER_SERVICE_V1_19_SIZE
enum RhhWildActorFlags {
    RHH_WILD_ACTOR_GENERATED = 1u, RHH_WILD_ACTOR_SHINY = 2u, RHH_WILD_ACTOR_FEMALE = 4u,
    RHH_WILD_ACTOR_HIDDEN = 8u, RHH_WILD_ACTOR_OFFSCREEN = 16u, RHH_WILD_ACTOR_PRESERVED = 32u
};
enum RhhWildActorCause { RHH_WILD_ACTOR_SPAWN = 1, RHH_WILD_ACTOR_REMOVE, RHH_WILD_ACTOR_CLEAR,
    RHH_WILD_ACTOR_RESTORE, RHH_WILD_ACTOR_BATTLE, RHH_WILD_ACTOR_ID_ASSIGNED,
    RHH_WILD_ACTOR_SUSPENDED, RHH_WILD_ACTOR_RESUMED };
/* Handles are process-local and valid only for this actor's current lifetime.
 * Pokémon identity is OT ID << 32 | personality when PRESERVED is set; it
 * is not guaranteed unique and is not a durable individual identifier. */
struct RhhWildActorSnapshot {
    uint32_t struct_size, slot, local_id, flags, level, elevation, facing, reserved;
    uint64_t handle, identity;
    RhhContentKey map, species;
    int32_t x, y;
};
struct RhhWildActorEvent {
    uint32_t struct_size, cause;
    struct RhhWildActorSnapshot actor;
};
enum RhhWildActorCommandStatus {
    RHH_WILD_ACTOR_OK = 0, RHH_WILD_ACTOR_INVALID = 1, RHH_WILD_ACTOR_UNAVAILABLE = 2,
    RHH_WILD_ACTOR_BLOCKED = 3, RHH_WILD_ACTOR_FULL = 4, RHH_WILD_ACTOR_FAILED = 5,
    RHH_WILD_ACTOR_DENIED = 6, RHH_WILD_ACTOR_ROUTE_CHANGED = 7
};
/* Point-in-time local resource counts, not a reservation or spawn guarantee.
 * next_generated_slot is UINT32_MAX when no generated slot is available
 * without replacing an existing encounter. Object and sprite counts include
 * every active object and sprite, not only wild encounters. */
struct RhhWildActorCapacity {
    uint32_t struct_size, automatic_enabled, commands_allowed, next_generated_slot;
    uint32_t generated_count, generated_limit, manual_count, object_count;
    uint32_t object_limit, sprite_count, sprite_limit, reserved;
    RhhContentKey map;
};
struct RhhWildActorSpawnRequest {
    uint32_t struct_size, reserved;
    RhhContentKey map;
    int32_t x, y;
};
/* A placement preflight is a snapshot, not a reservation. Slot and area are
 * valid only when status is OK. It does not generate a candidate or check
 * species, repel, graphics allocation or mod proposals. */
struct RhhWildActorSpawnPreflight {
    uint32_t struct_size, status, area, generated_slot;
};
struct RhhWildActorSpeciesSpawnRequest {
    uint32_t struct_size, level;
    RhhContentKey map, species;
    int32_t x, y;
    uint32_t reserved[2];
};
enum RhhWildSpeciesPreflightFlags {
    /* ABILITY_ROLL is conditional on reaching that filter; Repel is checked first. */
    RHH_WILD_SPECIES_REPEL_BLOCKS = 1u, RHH_WILD_SPECIES_ABILITY_ROLL = 2u
};
/* Placement, species validity and current level filters, without proposals,
 * generation or RNG. Flags and placement fields are valid only for OK.
 * A later proposal or world change can alter the actual outcome. */
struct RhhWildActorSpeciesPreflight {
    uint32_t struct_size, status, area, generated_slot, flags, reserved[3];
};
enum RhhWildActorDirection { RHH_WILD_ACTOR_SOUTH = 1, RHH_WILD_ACTOR_NORTH,
    RHH_WILD_ACTOR_WEST, RHH_WILD_ACTOR_EAST,
    /* Diagonal values describe facing; step commands take cardinal inputs. */
    RHH_WILD_ACTOR_SOUTHWEST, RHH_WILD_ACTOR_SOUTHEAST,
    RHH_WILD_ACTOR_NORTHWEST, RHH_WILD_ACTOR_NORTHEAST };
enum RhhWildBattleOutcome { RHH_WILD_BATTLE_UNKNOWN = 0, RHH_WILD_BATTLE_WON,
    RHH_WILD_BATTLE_LOST, RHH_WILD_BATTLE_DREW, RHH_WILD_BATTLE_RAN,
    RHH_WILD_BATTLE_PLAYER_TELEPORTED, RHH_WILD_BATTLE_MON_FLED, RHH_WILD_BATTLE_CAUGHT,
    RHH_WILD_BATTLE_OUT_OF_BALLS, RHH_WILD_BATTLE_FORFEITED, RHH_WILD_BATTLE_MON_TELEPORTED };
enum RhhWildBattleFlags { RHH_WILD_BATTLE_DOUBLE = 1u, RHH_WILD_BATTLE_ROAMER = 2u,
    RHH_WILD_BATTLE_SAFARI = 4u };
struct RhhWildActorBattleResult {
    uint32_t struct_size, outcome, flags, reserved;
    struct RhhWildActorSnapshot actor;
};
/* A durable run-scoped individual identity; both words must be nonzero.
 * It is distinct from the session handle and the Pokémon OT/personality. */
struct RhhWildActorIndividual {
    uint32_t struct_size, reserved;
    uint64_t run_id, serial;
};
enum RhhWildCatchOriginFlags { RHH_WILD_CATCH_INDIVIDUAL_AVAILABLE = 1u };
/* A checked association between an engaged actor and a committed catch
 * placement, available only during RHH_OWNED_CATCH_PLACED. The owned-mon
 * snapshots are not permanent individual identities or durable receipts. */
struct RhhWildCatchOrigin {
    uint32_t struct_size, flags, opponent_party_slot, reserved;
    struct RhhWildActorSnapshot actor;
    struct RhhWildActorIndividual individual;
    struct RhhCatchPlacement placement;
};
/* Verified capture of the engaged actor, copied at placement time. The
 * destination and snapshot may have changed by the battle-end callback. */
struct RhhWildActorBattleCatch {
    uint32_t struct_size, opponent_party_slot, destination_location, destination_box, destination_slot, reserved;
    struct RhhOwnedMonSnapshot after;
};
enum RhhWildActorBattleViewFlags { RHH_WILD_BATTLE_INDIVIDUAL_AVAILABLE = 1u,
    RHH_WILD_BATTLE_PARTICIPANT_AVAILABLE = 2u };
/* Live battle-scoped association. The participant is present only when the
 * battle service can currently identify the actor's active battler. */
struct RhhWildActorBattleView {
    uint32_t struct_size, flags, opponent_party_slot, reserved;
    uint64_t battle_id;
    struct RhhWildActorSnapshot actor;
    struct RhhWildActorIndividual individual;
    struct RhhBattleParticipant participant;
};
/* A copied, composable trait query for one live actor. If individual_available
 * is zero, individual is zero and the handle is only session-scoped. */
struct RhhWildTraitResult {
    uint32_t struct_size, individual_available, base, value, permitted, reserved;
    RhhContentKey tag;
    struct RhhWildActorSnapshot actor;
    struct RhhWildActorIndividual individual;
};
#define RHH_WILD_BATTLE_TRAIT_LIMIT 64u
/* "rhh:service/wild_traits" v1.1. Reducers may transform decision.value to
 * zero or one, or deny the query. context is available during the query.
 * RNG-consuming proposals fall back to the pre-transform value; denial is
 * retained and both gameplay RNG streams are restored. */
struct RhhWildTraitsService {
    struct RhhServiceHeader header;
    int (*query)(uint64_t actor_handle, RhhContentKey tag, struct RhhWildTraitResult *out);
    int (*context)(struct RhhWildTraitResult *out);
    /* v1.1: request a registered tag during an ENGAGED observer. The host
     * resolves requested traits once at the overworld-to-battle transition.
     * battle copies a requested, successfully captured result during that
     * verified battle or its BATTLE_ENDED actor notification. */
    int (*request_battle)(RhhContentKey tag);
    int (*battle)(RhhContentKey tag, struct RhhWildTraitResult *out);
};
#define RHH_WILD_TRAITS_SERVICE_V1_0_SIZE offsetof(struct RhhWildTraitsService, request_battle)
#define RHH_WILD_TRAITS_SERVICE_V1_1_SIZE sizeof(struct RhhWildTraitsService)
#define RHH_WILD_TRAIT_OVERRIDE_CAPACITY 8u
enum RhhWildTraitOverrideValue { RHH_WILD_TRAIT_INHERIT = 0, RHH_WILD_TRAIT_OFF, RHH_WILD_TRAIT_ON };
enum RhhWildTraitEditStatus { RHH_WILD_TRAIT_EDIT_OK = 0, RHH_WILD_TRAIT_EDIT_INVALID,
    RHH_WILD_TRAIT_EDIT_UNAVAILABLE, RHH_WILD_TRAIT_EDIT_STALE, RHH_WILD_TRAIT_EDIT_CONFLICT,
    RHH_WILD_TRAIT_EDIT_FULL, RHH_WILD_TRAIT_EDIT_DENIED };
struct RhhWildTraitOverride {
    uint32_t struct_size, value;
    uint64_t revision;
    RhhContentKey tag, owner;
};
struct RhhWildTraitEditRequest {
    uint32_t struct_size, value;
    uint64_t actor_handle, expected_revision;
    RhhContentKey tag, owner;
};
struct RhhWildTraitEditResult {
    uint32_t struct_size, status, is_preview, reserved;
    struct RhhWildActorSnapshot actor;
    struct RhhWildActorIndividual individual;
    struct RhhWildTraitOverride before, after;
    char reason[96];
};
/* "rhh:service/wild_trait_edits" v1.0. Saved overrides apply to an
 * identified live actor until it is removed; all results are copied. */
struct RhhWildTraitEditsService {
    struct RhhServiceHeader header;
    int (*at)(uint64_t actor_handle, RhhContentKey tag, struct RhhWildTraitOverride *out);
    int (*preview)(const struct RhhWildTraitEditRequest *, struct RhhWildTraitEditResult *out);
    int (*apply)(const struct RhhWildTraitEditRequest *, struct RhhWildTraitEditResult *out);
    int (*context)(struct RhhWildTraitEditResult *out);
};
/* Location-wide ecology state. Conditions are namespaced keys chosen by mods;
 * populations and observations are nonnegative counts keyed by species. Values are shared by all mods
 * and follow ordinary save generations. An absent value returns 1; invalid
 * requests return -1. Writes are allowed during a safe update or supported
 * committed observer, and take effect in memory until the next game save.
 * No offscreen individual actors are created or simulated by this service. */
enum RhhEcologyValueKind { RHH_ECOLOGY_CONDITION = 1, RHH_ECOLOGY_POPULATION = 2, RHH_ECOLOGY_OBSERVATIONS = 3 };
struct RhhEcologyValue {
    uint32_t struct_size, kind;
    RhhContentKey map, key;
    int64_t value;
};
/* "rhh:service/ecology", version 1.0. at enumerates a map's values ordered
 * by kind and key. set replaces one value; remove deletes it. */
struct RhhEcologyService {
    struct RhhServiceHeader header;
    int (*get)(RhhContentKey map, uint32_t kind, RhhContentKey key, int64_t *value);
    int (*set)(RhhContentKey map, uint32_t kind, RhhContentKey key, int64_t value);
    int (*remove)(RhhContentKey map, uint32_t kind, RhhContentKey key);
    int (*at)(RhhContentKey map, uint32_t index, struct RhhEcologyValue *out);
};
#define RHH_ECOLOGY_SERVICE_V1_0_SIZE sizeof(struct RhhEcologyService)

/* A bounded set of signed values attached to an identified live actor. Each
 * field belongs to an (owner module, namespaced field key) pair. Missing
 * owners' fields survive normal save/load and still occupy capacity. */
#define RHH_WILD_PROFILE_CAPACITY 8u
enum RhhWildProfileStatus { RHH_WILD_PROFILE_OK = 0, RHH_WILD_PROFILE_INVALID,
    RHH_WILD_PROFILE_UNAVAILABLE, RHH_WILD_PROFILE_STALE, RHH_WILD_PROFILE_FULL,
    RHH_WILD_PROFILE_DENIED };
struct RhhWildProfileValue {
    uint32_t struct_size, present, owner_available, reserved;
    uint64_t revision;
    RhhContentKey owner, field;
    int64_t value;
};
struct RhhWildProfileRequest {
    uint32_t struct_size, present;
    uint64_t actor_handle, expected_revision;
    RhhContentKey owner, field;
    int64_t value;
};
struct RhhWildProfileResult {
    uint32_t struct_size, status, is_preview, changed;
    struct RhhWildActorSnapshot actor;
    struct RhhWildActorIndividual individual;
    struct RhhWildProfileValue before, after;
    char reason[96];
};
struct RhhWildProfileBatchItem {
    RhhContentKey field;
    int64_t value;
    uint32_t present, reserved;
};
struct RhhWildProfileBatchRequest {
    uint32_t struct_size, count;
    uint64_t actor_handle, expected_revision;
    RhhContentKey owner;
    struct RhhWildProfileBatchItem items[RHH_WILD_PROFILE_CAPACITY];
};
struct RhhWildProfileBatchChange { struct RhhWildProfileValue before, after; };
struct RhhWildProfileBatchResult {
    uint32_t struct_size, status, is_preview, count, changed_count, reserved[3];
    struct RhhWildActorSnapshot actor;
    struct RhhWildActorIndividual individual;
    struct RhhWildProfileBatchChange changes[RHH_WILD_PROFILE_CAPACITY];
    char reason[96];
};
enum RhhWildProfileSchemaKind { RHH_WILD_PROFILE_SIGNED = 1, RHH_WILD_PROFILE_BOOLEAN = 2 };
enum RhhWildProfileSchemaStatus { RHH_WILD_PROFILE_SCHEMA_FOUND = 0, RHH_WILD_PROFILE_SCHEMA_NOT_FOUND,
    RHH_WILD_PROFILE_SCHEMA_INVALID, RHH_WILD_PROFILE_SCHEMA_CONFLICT };
/* A default describes an absent field; querying a schema does not create it.
 * Boolean fields must use the range 0..1. Labels are printable ASCII. */
struct RhhWildProfileSchema {
    uint32_t struct_size, kind, reserved[2];
    RhhContentKey owner, field;
    int64_t minimum, maximum, default_value;
    char label[48];
};
#define RHH_WILD_PROFILE_SCHEMA_CAPACITY 32u
struct RhhWildProfileSchemaList {
    uint32_t struct_size, count;
    RhhContentKey owner;
    struct RhhWildProfileSchema fields[RHH_WILD_PROFILE_SCHEMA_CAPACITY];
};
/* "rhh:service/wild_profile" v1.4. at copies a field or an absent value
 * with the actor-wide revision. Edits require a loaded owner and exact
 * expected revision. Values follow ordinary saves and are removed on despawn.
 * A loaded module ID is a cooperative owner label, not a security boundary. */
struct RhhWildProfileService {
    struct RhhServiceHeader header;
    int (*at)(uint64_t actor_handle, RhhContentKey owner, RhhContentKey field,
              struct RhhWildProfileValue *out);
    int (*preview)(const struct RhhWildProfileRequest *, struct RhhWildProfileResult *out);
    int (*apply)(const struct RhhWildProfileRequest *, struct RhhWildProfileResult *out);
    int (*context)(struct RhhWildProfileResult *out);
    /* v1.1: atomically edit distinct fields for one owner and actor. The
     * existing per-field constraints run before any write, and committed
     * per-field notifications run after the entire batch is stored. Results
     * and notifications are ordered by field key. batch_context is available
     * during those callbacks only. A changed batch advances the actor-wide
     * revision once; no-op batches leave it unchanged. */
    int (*preview_batch)(const struct RhhWildProfileBatchRequest *, struct RhhWildProfileBatchResult *out);
    int (*apply_batch)(const struct RhhWildProfileBatchRequest *, struct RhhWildProfileBatchResult *out);
    int (*batch_context)(struct RhhWildProfileBatchResult *out);
    /* v1.2: a schema collection queries one owner/field. Constraint hooks for
     * RHH_WILD_PROFILE_SCHEMA can offer matching definitions; identical offers
     * compose, while invalid or conflicting offers reject the query and edits.
     * Existing fields without a schema remain valid; schema validation does
     * not block deletion. Missing/out-of-range saved values remain readable. */
    int (*schema)(RhhContentKey owner, RhhContentKey field, struct RhhWildProfileSchema *out);
    int (*schema_context)(struct RhhWildProfileSchema *out);
    int (*offer_schema)(const struct RhhWildProfileSchema *schema);
    /* v1.3: collect up to 32 schemas from opt-in
     * RHH_WILD_PROFILE_SCHEMA_COLLECT hooks, sorted by field key. During
     * collection schema_context has field zero. Each offered field must also
     * resolve identically through the v1.2 single-field query. This list only
     * includes providers that participate in collection. Conflicts, invalid
     * offers and overflow fail without modifying out. */
    int (*schema_list)(RhhContentKey owner, struct RhhWildProfileSchemaList *out);
    /* v1.4: read the identified actor's profile captured immediately before
     * its transition to battle. The snapshot is available only during the
     * verified battle and its wild actor battle-ended notification. */
    int (*battle)(RhhContentKey owner, RhhContentKey field, struct RhhWildProfileValue *out);
};
#define RHH_WILD_PROFILE_SERVICE_V1_0_SIZE offsetof(struct RhhWildProfileService, preview_batch)
#define RHH_WILD_PROFILE_SERVICE_V1_1_SIZE offsetof(struct RhhWildProfileService, schema)
#define RHH_WILD_PROFILE_SERVICE_V1_2_SIZE offsetof(struct RhhWildProfileService, schema_list)
#define RHH_WILD_PROFILE_SERVICE_V1_3_SIZE offsetof(struct RhhWildProfileService, battle)
#define RHH_WILD_PROFILE_SERVICE_V1_4_SIZE sizeof(struct RhhWildProfileService)
/* One action is considered per identified actor per accepted player step.
 * Directions deliberately match RhhWildActorDirection. */
enum RhhWildBehaviorAction { RHH_WILD_BEHAVIOR_NONE = 0, RHH_WILD_BEHAVIOR_SOUTH,
    RHH_WILD_BEHAVIOR_NORTH, RHH_WILD_BEHAVIOR_WEST, RHH_WILD_BEHAVIOR_EAST,
    RHH_WILD_BEHAVIOR_ENGAGE };
enum RhhWildBehaviorPhase { RHH_WILD_BEHAVIOR_PROPOSAL = 1, RHH_WILD_BEHAVIOR_REPORT };
struct RhhWildBehaviorContext {
    uint32_t struct_size, phase, action, status;
    struct RhhWorldStepSnapshot step;
    struct RhhWildActorSnapshot actor;
    struct RhhWildActorIndividual individual;
};
#define RHH_WILD_ACTIVITY_CAPACITY 32u
/* Activity IDs must be unique per collection; owner is a loaded module ID.
 * The highest priority wins, then a deterministic weighted selection chooses
 * an action. The shortest interval among that priority is its shared epoch. */
struct RhhWildActivity {
    uint32_t struct_size, action, weight, interval;
    int32_t priority;
    uint32_t reserved;
    RhhContentKey id, owner;
};
struct RhhWildActivitySelection {
    uint32_t struct_size, valid, interval, reserved;
    uint64_t until_step;
    struct RhhWildActivity activity;
};
/* Last accepted activity. valid_until_step is the next selection boundary,
 * not a promise that an actor will keep moving or be simulated off-map. */
struct RhhWildActivityState {
    uint32_t struct_size, valid, action, reserved;
    RhhContentKey id, owner;
    uint64_t since_step, valid_until_step;
    struct RhhWildActorIndividual individual;
};
struct RhhWildActivityChange {
    uint32_t struct_size, reserved;
    struct RhhWildActorSnapshot actor;
    struct RhhWildActivityState before, after;
};
enum RhhWildBehaviorPolicyFlags {
    RHH_WILD_POLICY_PLAYER_STEP = 1u, RHH_WILD_POLICY_CURRENT_MAP = 2u,
    RHH_WILD_POLICY_INCLUDES_OFFSCREEN = 4u, RHH_WILD_POLICY_REQUIRES_ID = 8u,
    RHH_WILD_POLICY_ONE_PENDING_BATCH = 16u
};
struct RhhWildBehaviorPolicy {
    uint32_t struct_size, flags, radius_x, radius_y;
    uint32_t actor_capacity, activity_capacity, max_weight, max_interval;
    int32_t min_priority, max_priority;
    uint32_t reserved[2];
};
#define RHH_WILD_BEHAVIOR_RADIUS_MAX 65535u
enum RhhWildPolicyStatus { RHH_WILD_POLICY_OK, RHH_WILD_POLICY_INVALID,
    RHH_WILD_POLICY_UNAVAILABLE, RHH_WILD_POLICY_STALE, RHH_WILD_POLICY_DENIED };
struct RhhWildBehaviorPolicyRequest {
    uint32_t struct_size, radius_x, radius_y, reserved;
    RhhContentKey requester;
    uint64_t expected_revision;
};
struct RhhWildBehaviorPolicyChange {
    uint32_t struct_size, status, changed, is_preview;
    uint64_t before_revision, after_revision;
    RhhContentKey requester;
    struct RhhWildBehaviorPolicy before, after;
    char reason[96];
};
enum RhhWildBehaviorGate {
    RHH_WILD_GATE_OTHER_MAP = 1u, RHH_WILD_GATE_OUTSIDE_RADIUS = 2u,
    RHH_WILD_GATE_NO_INDIVIDUAL = 4u, RHH_WILD_GATE_COMMANDS_UNAVAILABLE = 8u,
    RHH_WILD_GATE_SAVE_ACTIVE = 16u, RHH_WILD_GATE_BATCH_PENDING = 32u,
    RHH_WILD_GATE_DISPATCHING = 64u, RHH_WILD_GATE_NO_HOOKS = 128u
};
/* Coordinates use the same map-local tiles as RhhWorldStepSnapshot. */
struct RhhWildBehaviorInspectionRequest {
    uint32_t struct_size, reserved;
    uint64_t actor_handle;
    RhhContentKey map;
    int32_t x, y;
};
struct RhhWildBehaviorInspection {
    uint32_t struct_size, gates, individual_available, reserved;
    int64_t dx, dy;
    struct RhhWildActorSnapshot actor;
    struct RhhWildActorIndividual individual;
};
#define RHH_WILD_QUEUE_CAPACITY 8u
enum RhhWildQueueStatus { RHH_WILD_QUEUE_OK, RHH_WILD_QUEUE_INVALID, RHH_WILD_QUEUE_UNAVAILABLE,
    RHH_WILD_QUEUE_STALE, RHH_WILD_QUEUE_CONFLICT, RHH_WILD_QUEUE_DENIED,
    RHH_WILD_QUEUE_ALREADY_THERE, RHH_WILD_QUEUE_NO_PATH, RHH_WILD_QUEUE_ROUTE_LIMIT, RHH_WILD_QUEUE_ROUTE_TOO_LONG };
struct RhhWildQueue {
    uint32_t struct_size, count, owner_available, reserved;
    uint64_t revision;
    RhhContentKey owner;
    uint8_t actions[RHH_WILD_QUEUE_CAPACITY];
    struct RhhWildActorIndividual individual;
};
struct RhhWildQueueRequest {
    uint32_t struct_size, count;
    uint64_t actor_handle, expected_revision;
    RhhContentKey owner;
    uint8_t actions[RHH_WILD_QUEUE_CAPACITY];
};
enum RhhWildQueueChangeCause { RHH_WILD_QUEUE_REPLACED = 1, RHH_WILD_QUEUE_CONSUMED };
struct RhhWildQueueChange {
    uint32_t struct_size, status, cause, changed;
    struct RhhWildActorSnapshot actor;
    struct RhhWildQueue before, after;
    char reason[96];
};
struct RhhWildQueueRouteOptions {
    uint32_t struct_size, reserved;
    uint64_t expected_revision;
    RhhContentKey owner;
    /* v1.7: when INTERRUPTIBLE is set, collected activities strictly above
     * interrupt_priority may run before a queued route action. */
    int32_t interrupt_priority;
    uint32_t flags;
};
enum RhhWildQueueRouteFlags { RHH_WILD_QUEUE_ROUTE_INTERRUPTIBLE = 1u,
    /* v1.8: if the full route exceeds queue capacity, enqueue its first
     * eight steps. The caller must replan after those steps complete. */
    RHH_WILD_QUEUE_ROUTE_PREFIX = 2u,
    /* v1.9: retain the destination and owner after the queue is consumed. */
    RHH_WILD_QUEUE_ROUTE_RETAIN_GOAL = 4u,
    /* v1.11: consume a guarded route step only after the actor arrives at
     * its predicted waypoint. Interrupted, deviated or lost steps do not
     * consume it; removing the actor clears its queue. Check route state and
     * replan if the actor no longer matches the recorded origin. Pending
     * movement tickets do not survive saves. */
    RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL = 8u };
#define RHH_WILD_QUEUE_ROUTE_OPTIONS_V1_0_SIZE offsetof(struct RhhWildQueueRouteOptions, interrupt_priority)
#define RHH_WILD_QUEUE_ROUTE_OPTIONS_V1_1_SIZE sizeof(struct RhhWildQueueRouteOptions)
/* Reports the current queue policy, including routes without a retained goal.
 * An empty or unguarded queue reports on_arrival=0. */
struct RhhWildQueueCompletion {
    uint32_t struct_size, on_arrival;
    uint64_t revision;
};
struct RhhWildQueueInterruption {
    uint32_t struct_size, enabled;
    int32_t priority;
    uint32_t reserved;
    uint64_t revision;
};
enum RhhWildRouteGuardReason { RHH_WILD_ROUTE_GUARD_READY, RHH_WILD_ROUTE_GUARD_BUSY,
    RHH_WILD_ROUTE_GUARD_BLOCKED, RHH_WILD_ROUTE_GUARD_ORIGIN_CHANGED,
    RHH_WILD_ROUTE_GUARD_LANDING_CHANGED, RHH_WILD_ROUTE_GUARD_UNAVAILABLE };
struct RhhWildRouteWaypoint { int32_t x, y; uint32_t elevation, facing; };
struct RhhWildQueueRouteState {
    uint32_t struct_size, guarded, status, reason;
    uint64_t revision;
    struct RhhWildRouteWaypoint origin, landing, actual_origin, actual_landing;
};
struct RhhWildRouteRequest;
struct RhhWildHabitatRouteRequest;
struct RhhWildQueueRouteResult;
enum RhhWildQueueGoalKind { RHH_WILD_QUEUE_GOAL_NONE, RHH_WILD_QUEUE_GOAL_POINT, RHH_WILD_QUEUE_GOAL_HABITAT };
struct RhhWildQueueGoal {
    uint32_t struct_size, kind, flags, stop_distance, owner_available;
    int32_t interrupt_priority, target_x, target_y;
    uint32_t radius, all_flags, any_flags, exclude_flags;
    uint64_t revision;
    RhhContentKey owner, map;
    struct RhhWildActorIndividual individual;
};
struct RhhWildQueueGoalChange {
    uint32_t struct_size, reserved;
    struct RhhWildQueueGoal before, after;
};
/* "rhh:service/wild_behavior" v1.11. During ACTIVITY_COLLECT constraints,
 * context copies the actor and offer adds an eligible activity. During
 * PROPOSE transforms may replace the selected action. RESULT reports the
 * host command status. selected is available during PROPOSE and RESULT;
 * at reads the last accepted activity, and change_context copies before/after
 * during the committed ACTIVITY_CHANGED notification. */
struct RhhWildBehaviorService {
    struct RhhServiceHeader header;
    int (*context)(struct RhhWildBehaviorContext *out);
    int (*offer)(const struct RhhWildActivity *activity);
    int (*selected)(struct RhhWildActivitySelection *out);
    int (*at)(uint64_t actor_handle, struct RhhWildActivityState *out);
    int (*change_context)(struct RhhWildActivityChange *out);
    /* v1.3: copied policy and a point-in-time inspection. A clear gates
     * mask means the actor could enter a new batch for the supplied step;
     * this does not predict a chosen action or successful movement. */
    int (*policy)(struct RhhWildBehaviorPolicy *out);
    int (*inspect)(const struct RhhWildBehaviorInspectionRequest *request, struct RhhWildBehaviorInspection *out);
    /* v1.4: one bounded queue per identified actor. Edits are revision-checked
     * and constrained; owner is a cooperative loaded-module label. Queued
     * actions precede activity selection, remain subject to proposal hooks and
     * engine checks, and are consumed only on successful unchanged commands.
     * Missing owners suspend their saved queue. Context is available during
     * CAN_EDIT_WILD_QUEUE and WILD_QUEUE_CHANGED. */
    int (*queue_at)(uint64_t actor_handle, struct RhhWildQueue *out);
    int (*queue_replace)(const struct RhhWildQueueRequest *, struct RhhWildQueueChange *out);
    int (*queue_context)(struct RhhWildQueueChange *out);
    /* v1.5: plan then atomically replace the queue with the full route.
     * Existing queue-edit constraints apply. New plans retain waypoints and
     * recheck them before each action; older saved routes remain unguarded.
     * v1.8 permits an opt-in prefix, reported by comparing route.length to
     * change.after.count. v1.9 can also retain and reserve the goal. */
    int (*queue_route)(const struct RhhWildQueueRouteOptions *, const struct RhhWildRouteRequest *, struct RhhWildQueueRouteResult *out);
    int (*queue_habitat_route)(const struct RhhWildQueueRouteOptions *, const struct RhhWildHabitatRouteRequest *, struct RhhWildQueueRouteResult *out);
    /* v1.6: copy expected and current head-step geometry and the current gate.
     * Unguarded or empty queues report guarded=0. */
    int (*queue_route_state)(uint64_t actor_handle, struct RhhWildQueueRouteState *out);
    /* v1.7: copy the opt-in route interruption policy. */
    int (*queue_interruption)(uint64_t actor_handle, struct RhhWildQueueInterruption *out);
    /* v1.9: a retained goal reserves its actor for its owner even when the
     * prefix is empty. Replan is explicit and uses saved options; clearing or
     * replacing the queue without RETAIN_GOAL also clears the reservation. */
    int (*queue_goal)(uint64_t actor_handle, struct RhhWildQueueGoal *out);
    int (*queue_replan)(uint64_t actor_handle, uint64_t expected_revision, RhhContentKey owner, struct RhhWildQueueRouteResult *out);
    /* Available in queue-edit constraints and committed queue notifications. */
    int (*queue_goal_context)(struct RhhWildQueueGoalChange *out);
    /* v1.10: checked, saved radius changes; 65535 covers all representable
     * map-local coordinates. Only loaded, identified actors on the current
     * map are simulated, still on player steps and subject to engine checks. */
    uint64_t (*policy_revision)(void);
    int (*policy_preview)(const struct RhhWildBehaviorPolicyRequest *, struct RhhWildBehaviorPolicyChange *out);
    int (*policy_apply)(const struct RhhWildBehaviorPolicyRequest *, struct RhhWildBehaviorPolicyChange *out);
    int (*policy_change_context)(struct RhhWildBehaviorPolicyChange *out);
    /* v1.11: inspect the saved consumption policy of the current queue. */
    int (*queue_completion)(uint64_t actor_handle, struct RhhWildQueueCompletion *out);
};
#define RHH_WILD_BEHAVIOR_SERVICE_V1_0_SIZE offsetof(struct RhhWildBehaviorService, offer)
#define RHH_WILD_BEHAVIOR_SERVICE_V1_1_SIZE offsetof(struct RhhWildBehaviorService, selected)
#define RHH_WILD_BEHAVIOR_SERVICE_V1_2_SIZE offsetof(struct RhhWildBehaviorService, policy)
#define RHH_WILD_BEHAVIOR_SERVICE_V1_3_SIZE offsetof(struct RhhWildBehaviorService, queue_at)
#define RHH_WILD_BEHAVIOR_SERVICE_V1_4_SIZE offsetof(struct RhhWildBehaviorService, queue_route)
#define RHH_WILD_BEHAVIOR_SERVICE_V1_5_SIZE offsetof(struct RhhWildBehaviorService, queue_route_state)
#define RHH_WILD_BEHAVIOR_SERVICE_V1_6_SIZE offsetof(struct RhhWildBehaviorService, queue_interruption)
#define RHH_WILD_BEHAVIOR_SERVICE_V1_7_SIZE offsetof(struct RhhWildBehaviorService, queue_goal)
#define RHH_WILD_BEHAVIOR_SERVICE_V1_8_SIZE offsetof(struct RhhWildBehaviorService, queue_goal)
#define RHH_WILD_BEHAVIOR_SERVICE_V1_9_SIZE offsetof(struct RhhWildBehaviorService, policy_revision)
#define RHH_WILD_BEHAVIOR_SERVICE_V1_10_SIZE offsetof(struct RhhWildBehaviorService, queue_completion)
#define RHH_WILD_BEHAVIOR_SERVICE_V1_11_SIZE sizeof(struct RhhWildBehaviorService)
/* Nearby actors are measured in Manhattan tiles on the anchor's map. Hidden
 * and offscreen actors are included; use perception for line-of-sight checks.
 * Unidentified actors are included unless IDENTIFIED is requested. */
#define RHH_WILD_NEIGHBOR_CAPACITY 16u
enum RhhWildNeighborFlags { RHH_WILD_NEIGHBOR_SAME_SPECIES = 1u, RHH_WILD_NEIGHBOR_IDENTIFIED = 2u };
struct RhhWildNeighborRequest {
    uint32_t struct_size, radius, flags, reserved;
    uint64_t anchor_handle;
};
struct RhhWildNeighbor {
    uint32_t distance, individual_available;
    struct RhhWildActorSnapshot actor;
    struct RhhWildActorIndividual individual;
};
struct RhhWildNeighborList {
    uint32_t struct_size, count;
    struct RhhWildActorSnapshot anchor;
    struct RhhWildNeighbor neighbors[RHH_WILD_NEIGHBOR_CAPACITY];
};
#define RHH_WILD_ROUTE_RADIUS 4u
#define RHH_WILD_ROUTE_NODE_LIMIT 256u
enum RhhWildRouteStatus { RHH_WILD_ROUTE_FOUND = 0, RHH_WILD_ROUTE_ALREADY_THERE,
    RHH_WILD_ROUTE_NO_PATH, RHH_WILD_ROUTE_LIMIT };
struct RhhWildRouteRequest {
    uint32_t struct_size, stop_distance, reserved[2];
    uint64_t actor_handle;
    int32_t target_x, target_y;
};
struct RhhWildRouteResult {
    uint32_t struct_size, status, first_direction, length, visited, reserved[3];
    struct RhhWildActorSnapshot actor;
};
/* Each entry is one cardinal step command and its predicted landing tile,
 * elevation and facing. The origin is in the route's actor snapshot. A
 * sideways stair can make the landing and facing diagonal. The trace is
 * empty unless the result status is FOUND. It does not reserve a path. */
struct RhhWildRouteStep {
    int32_t x, y;
    uint32_t direction, elevation, facing, reserved;
};
struct RhhWildRouteTrace {
    uint32_t struct_size, count;
    struct RhhWildRouteStep steps[RHH_WILD_ROUTE_NODE_LIMIT];
};
#define RHH_WILD_HABITAT_ROUTE_MAX_RADIUS (RHH_WILD_ROUTE_RADIUS + 1u)
/* radius is Manhattan distance from the actor's starting tile; stop_distance
 * is zero (stand on a match) or one (stand on or beside it). A zero any_flags
 * mask is a wildcard. Invalid flags or unavailable samples fail without
 * modifying the result. */
struct RhhWildHabitatRouteRequest {
    uint32_t struct_size, stop_distance, radius, reserved;
    uint64_t actor_handle;
    uint32_t all_flags, any_flags, exclude_flags, reserved2;
};
struct RhhWildHabitatRouteResult {
    uint32_t struct_size, sampled, denied, reserved;
    struct RhhWildRouteResult route;
    struct RhhHabitatSample target;
};
/* Route details are copied for successful planning, including no-path and
 * limit statuses. Habitat is populated only for habitat-route requests.
 * No-path, already-there and too-long results leave the existing queue intact. */
struct RhhWildQueueRouteResult {
    uint32_t struct_size, status, habitat_available, reserved;
    struct RhhWildRouteResult route;
    struct RhhWildHabitatRouteResult habitat;
    struct RhhWildQueueChange change;
};
enum RhhWildStepStatus { RHH_WILD_STEP_PENDING = 0, RHH_WILD_STEP_ARRIVED,
    RHH_WILD_STEP_DEVIATED, RHH_WILD_STEP_INTERRUPTED, RHH_WILD_STEP_REMOVED, RHH_WILD_STEP_LOST };
enum RhhWildStepSource { RHH_WILD_STEP_DIRECT = 1, RHH_WILD_STEP_BEHAVIOR, RHH_WILD_STEP_QUEUE };
/* Tickets and pending movement are process-local and are not save receipts.
 * after_valid marks whether the actor could still be sampled. Expected is a
 * prediction made when the engine accepts the step, not a reservation. */
struct RhhWildStepResult {
    uint32_t struct_size, status, source, direction, after_valid, queue_consumed, reserved[2];
    uint64_t run_id, ticket, queue_revision;
    RhhContentKey queue_owner;
    struct RhhWildActorSnapshot before, after;
    struct RhhWildActorIndividual individual;
    struct RhhWildRouteWaypoint expected;
};
/* "rhh:service/wild_actor", version 1.18. All results are copied. Event
 * context exists only during actor observer notifications.
 * Commands are accepted only in a safe overworld update context. */
struct RhhWildActorService {
    struct RhhServiceHeader header;
    uint32_t (*slots)(void);
    int (*at)(uint32_t slot, struct RhhWildActorSnapshot *out);
    int (*find)(uint64_t handle, struct RhhWildActorSnapshot *out);
    int (*context)(struct RhhWildActorEvent *out);
    int (*spawn)(const struct RhhWildActorSpawnRequest *request, struct RhhWildActorSnapshot *out);
    int (*remove)(uint64_t handle);
    int (*step)(uint64_t handle, uint32_t direction);
    int (*engage)(uint64_t handle);
    int (*battle_result)(struct RhhWildActorBattleResult *out);
    int (*individual)(uint64_t handle, struct RhhWildActorIndividual *out);
    int (*find_individual)(const struct RhhWildActorIndividual *id, struct RhhWildActorSnapshot *out);
    int (*event_individual)(struct RhhWildActorIndividual *out);
    /* v1.5: excludes the anchor, requires radius <= 64, and orders by distance,
     * then available individual ID, then slot. Fails without changing output. */
    int (*neighbors)(const struct RhhWildNeighborRequest *request, struct RhhWildNeighborList *out);
    /* v1.6: bounded, read-only route toward an in-map target, ending at or
     * within one Manhattan tile of it. No path is queued; step revalidates. */
    int (*route)(const struct RhhWildRouteRequest *request, struct RhhWildRouteResult *out);
    /* v1.7: find a reachable matching habitat tile, using route's bounded
     * movement search. Habitat matches use the normal composable query. */
    int (*route_habitat)(const struct RhhWildHabitatRouteRequest *request, struct RhhWildHabitatRouteResult *out);
    /* v1.8: generate a requested species and level through the normal wild
     * species and candidate hooks. Rules may change or deny the species. */
    int (*spawn_species)(const struct RhhWildActorSpeciesSpawnRequest *request, struct RhhWildActorSnapshot *out);
    /* v1.9: the same bounded previews with every predicted step. Both outputs
     * remain unchanged on failure; callers initialize both struct_size fields. */
    int (*route_trace)(const struct RhhWildRouteRequest *request, struct RhhWildRouteResult *out,
                       struct RhhWildRouteTrace *trace);
    int (*route_habitat_trace)(const struct RhhWildHabitatRouteRequest *request, struct RhhWildHabitatRouteResult *out,
                               struct RhhWildRouteTrace *trace);
    /* v1.10: pending accepted step, and result during STEP_FINISHED only. */
    int (*step_pending)(uint64_t handle, struct RhhWildStepResult *out);
    int (*step_context)(struct RhhWildStepResult *out);
    /* v1.11: copied local resource counts; no generation or RNG is used. */
    int (*capacity)(struct RhhWildActorCapacity *out);
    /* v1.12: check shared deterministic spawn placement rules without
     * generating a Pokémon. A valid query returns a status in out; invalid
     * arguments or an unreadable world leave out unchanged. */
    int (*spawn_preflight)(const struct RhhWildActorSpawnRequest *request,
                           struct RhhWildActorSpawnPreflight *out);
    /* v1.13: assess a requested species, level and current encounter filters.
     * No candidate, Pokémon or random ability roll is generated. */
    int (*species_spawn_preflight)(const struct RhhWildActorSpeciesSpawnRequest *request,
                                   struct RhhWildActorSpeciesPreflight *out);
    /* v1.14: checked actor source during an acquired catch placement. */
    int (*catch_origin)(struct RhhWildCatchOrigin *out);
    /* v1.15: verified actor capture during BATTLE_ENDED; failure means
     * unknown, not that the actor was not caught. */
    int (*battle_catch)(struct RhhWildActorBattleCatch *out);
    /* v1.16: checked association during the current battle. */
    int (*battle_actor)(struct RhhWildActorBattleView *out);
};
#define RHH_WILD_ACTOR_SERVICE_V1_0_SIZE offsetof(struct RhhWildActorService, spawn)
#define RHH_WILD_ACTOR_SERVICE_V1_1_SIZE offsetof(struct RhhWildActorService, step)
#define RHH_WILD_ACTOR_SERVICE_V1_2_SIZE offsetof(struct RhhWildActorService, battle_result)
#define RHH_WILD_ACTOR_SERVICE_V1_3_SIZE offsetof(struct RhhWildActorService, individual)
#define RHH_WILD_ACTOR_SERVICE_V1_4_SIZE offsetof(struct RhhWildActorService, neighbors)
#define RHH_WILD_ACTOR_SERVICE_V1_5_SIZE offsetof(struct RhhWildActorService, route)
#define RHH_WILD_ACTOR_SERVICE_V1_6_SIZE offsetof(struct RhhWildActorService, route_habitat)
#define RHH_WILD_ACTOR_SERVICE_V1_7_SIZE offsetof(struct RhhWildActorService, spawn_species)
#define RHH_WILD_ACTOR_SERVICE_V1_8_SIZE offsetof(struct RhhWildActorService, route_trace)
#define RHH_WILD_ACTOR_SERVICE_V1_9_SIZE offsetof(struct RhhWildActorService, step_pending)
#define RHH_WILD_ACTOR_SERVICE_V1_10_SIZE offsetof(struct RhhWildActorService, capacity)
#define RHH_WILD_ACTOR_SERVICE_V1_11_SIZE offsetof(struct RhhWildActorService, spawn_preflight)
#define RHH_WILD_ACTOR_SERVICE_V1_12_SIZE offsetof(struct RhhWildActorService, species_spawn_preflight)
#define RHH_WILD_ACTOR_SERVICE_V1_13_SIZE offsetof(struct RhhWildActorService, catch_origin)
#define RHH_WILD_ACTOR_SERVICE_V1_14_SIZE offsetof(struct RhhWildActorService, battle_catch)
#define RHH_WILD_ACTOR_SERVICE_V1_15_SIZE offsetof(struct RhhWildActorService, battle_actor)
#define RHH_WILD_ACTOR_SERVICE_V1_16_SIZE sizeof(struct RhhWildActorService)
/* v1.17 retains a verified actor association through catch-owner rekey. */
#define RHH_WILD_ACTOR_SERVICE_V1_17_SIZE RHH_WILD_ACTOR_SERVICE_V1_16_SIZE
/* v1.18 restores gameplay RNG and last-used ability after actor observers. */
#define RHH_WILD_ACTOR_SERVICE_V1_18_SIZE RHH_WILD_ACTOR_SERVICE_V1_17_SIZE
/* A suspended individual retains its run identity and host-owned actor data.
 * The snapshot is copied; it is not a live actor handle or a reservation. */
struct RhhWildDormantActor {
    uint32_t struct_size, level, facing, elevation, no_despawn, owner_available, reserved[2];
    struct RhhWildActorIndividual individual;
    RhhContentKey owner, map, species;
    int32_t x, y;
};
/* A guarded location edit; expected is a previously copied snapshot. The
 * destination uses static authored tiles and does not represent a path. */
struct RhhWildDormantRelocationRequest {
    uint32_t struct_size, facing, reserved[2];
    struct RhhWildDormantActor expected;
    RhhContentKey map;
    int32_t x, y;
};
struct RhhWildDormantRelocation {
    uint32_t struct_size, changed, reserved[2];
    struct RhhWildDormantActor before, after;
    struct RhhAuthoredMapTile destination;
};
enum RhhWildDormantStepStatus { RHH_WILD_DORMANT_STEP_OPEN, RHH_WILD_DORMANT_STEP_COLLISION,
    RHH_WILD_DORMANT_STEP_EDGE, RHH_WILD_DORMANT_STEP_UNAVAILABLE,
    RHH_WILD_DORMANT_STEP_AMBIGUOUS, RHH_WILD_DORMANT_STEP_ELEVATION };
enum RhhWildDormantStepFlags { RHH_WILD_DORMANT_STEP_CROSSES_MAP = 1u,
    RHH_WILD_DORMANT_STEP_SOURCE_DIFFERS = 2u, RHH_WILD_DORMANT_STEP_SOURCE_AVAILABLE = 4u,
    RHH_WILD_DORMANT_STEP_DESTINATION_AVAILABLE = 8u };
/* A static adjacent-tile observation, not a movement permission or path.
 * Source and destination tiles are populated when available. */
struct RhhWildDormantStepPreview {
    uint32_t struct_size, status, flags, direction;
    struct RhhWildDormantActor actor;
    struct RhhAuthoredMapTile source, destination;
    struct RhhAuthoredConnection connection;
};
/* Bounded fields for one owner, in field-key order. Missing owners' retained
 * fields remain visible; revision belongs to the entire individual profile. */
struct RhhWildDormantProfileList {
    uint32_t struct_size, count, owner_available, reserved;
    uint64_t revision;
    RhhContentKey owner;
    struct RhhWildActorIndividual individual;
    struct RhhWildProfileValue fields[RHH_WILD_PROFILE_CAPACITY];
};
struct RhhWildDormantProfileRequest {
    uint32_t struct_size, present;
    struct RhhWildActorIndividual individual;
    uint64_t expected_revision;
    RhhContentKey owner, field;
    int64_t value;
};
struct RhhWildDormantProfileResult {
    uint32_t struct_size, status, is_preview, changed;
    struct RhhWildDormantActor actor;
    struct RhhWildProfileValue before, after;
    char reason[96];
};
struct RhhWildDormantProfileBatchRequest {
    uint32_t struct_size, count;
    struct RhhWildActorIndividual individual;
    uint64_t expected_revision;
    RhhContentKey owner;
    struct RhhWildProfileBatchItem items[RHH_WILD_PROFILE_CAPACITY];
};
struct RhhWildDormantProfileBatchResult {
    uint32_t struct_size, status, is_preview, count, changed_count, reserved[3];
    struct RhhWildDormantActor actor;
    struct RhhWildProfileBatchChange changes[RHH_WILD_PROFILE_CAPACITY];
    char reason[96];
};
/* A saved checkpoint on the run's player-step clock, not a simulation.
 * RHH_WILD_DORMANT_CLOCK_ADVANCED observers obtain before/after with
 * clock_context; event_key is map, subject is species, ordinal is serial. */
#define RHH_WILD_DORMANT_MAX_STEP_ADVANCE 256u
enum RhhWildDormantClockStatus { RHH_WILD_DORMANT_CLOCK_OK, RHH_WILD_DORMANT_CLOCK_INVALID,
    RHH_WILD_DORMANT_CLOCK_UNAVAILABLE, RHH_WILD_DORMANT_CLOCK_STALE, RHH_WILD_DORMANT_CLOCK_LIMIT };
struct RhhWildDormantClock {
    uint32_t struct_size, reserved;
    struct RhhWildDormantActor actor;
    uint64_t current_step, processed_step, pending_steps;
};
struct RhhWildDormantClockRequest {
    uint32_t struct_size, reserved;
    struct RhhWildActorIndividual individual;
    RhhContentKey owner;
    uint64_t expected_step, through_step;
};
struct RhhWildDormantClockChange {
    uint32_t struct_size, changed;
    struct RhhWildDormantClock before, after;
};
enum RhhWildDormantWorkFlags { RHH_WILD_DORMANT_WORK_RELOCATE = 1u,
    /* v1.8: check expected_profile_revision even when count is zero. */
    RHH_WILD_DORMANT_WORK_GUARD_PROFILE = 2u };
enum RhhWildDormantWorkStatus { RHH_WILD_DORMANT_WORK_OK, RHH_WILD_DORMANT_WORK_INVALID,
    RHH_WILD_DORMANT_WORK_UNAVAILABLE, RHH_WILD_DORMANT_WORK_STALE, RHH_WILD_DORMANT_WORK_BLOCKED,
    RHH_WILD_DORMANT_WORK_LIMIT, RHH_WILD_DORMANT_WORK_DENIED, RHH_WILD_DORMANT_WORK_FULL };
struct RhhWildDormantWorkRequest {
    uint32_t struct_size, flags, count, facing;
    struct RhhWildDormantActor expected;
    uint64_t expected_profile_revision, expected_step, through_step;
    RhhContentKey map;
    int32_t x, y;
    struct RhhWildProfileBatchItem items[RHH_WILD_PROFILE_CAPACITY];
};
struct RhhWildDormantWorkResult {
    uint32_t struct_size, status, is_preview, changed;
    struct RhhWildDormantProfileBatchResult profile;
    struct RhhWildDormantRelocation relocation;
    struct RhhWildDormantClockChange clock;
    char reason[96];
};
/* profile.actor and relocation.before are the pre-transaction actor;
 * relocation.after and clock.after.actor contain its committed location. */
/* Dormant profile constraint and committed events expose profile_context:
 * event_key is the field, subject is species, ordinal is the individual
 * serial, and args contain before-present, after-present and preview. */
/* "rhh:service/wild_dormant" v1.8. Queries do not mutate the world. Actor
 * commands return RhhWildActorCommandStatus and leave outputs unchanged on
 * failure; profile edits return RhhWildProfileStatus and report details.
 * RHH_WILD_DORMANT_RELOCATED is a committed observer notification; its event
 * key, subject and ordinal are the destination map, species and serial. */
struct RhhWildDormantService {
    struct RhhServiceHeader header;
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhWildDormantActor *out);
    int (*find)(const struct RhhWildActorIndividual *id, struct RhhWildDormantActor *out);
    int (*suspend)(uint64_t handle, RhhContentKey owner, struct RhhWildDormantActor *out);
    int (*resume)(const struct RhhWildActorIndividual *id, RhhContentKey owner, struct RhhWildActorSnapshot *out);
    int (*discard)(const struct RhhWildActorIndividual *id, RhhContentKey owner);
    /* v1.1: explicitly relocate using an authored tile baseline. */
    int (*relocate)(const struct RhhWildDormantRelocationRequest *request, struct RhhWildDormantRelocation *out);
    int (*relocation_context)(struct RhhWildDormantRelocation *out);
    /* v1.2: observe one cardinal step on the authored map baseline. */
    int (*step_preview)(const struct RhhWildActorIndividual *id, uint32_t direction,
                        struct RhhWildDormantStepPreview *out);
    /* v1.3: read saved profile fields while the individual is suspended. */
    int (*profile_at)(const struct RhhWildActorIndividual *id, RhhContentKey owner, RhhContentKey field,
                      struct RhhWildProfileValue *out);
    int (*profile_list)(const struct RhhWildActorIndividual *id, RhhContentKey owner,
                        struct RhhWildDormantProfileList *out);
    /* v1.4: guarded, schema-checked dormant profile edits and event context.
     * Loaded live-profile constraint/observer modules must provide matching
     * dormant hooks before a changed edit can proceed. */
    int (*profile_preview)(const struct RhhWildDormantProfileRequest *, struct RhhWildDormantProfileResult *out);
    int (*profile_apply)(const struct RhhWildDormantProfileRequest *, struct RhhWildDormantProfileResult *out);
    int (*profile_context)(struct RhhWildDormantProfileResult *out);
    /* v1.5: atomic multi-field edits with per-field constraints and events.
     * Batch changes and notifications are ordered by field key. */
    int (*profile_preview_batch)(const struct RhhWildDormantProfileBatchRequest *, struct RhhWildDormantProfileBatchResult *out);
    int (*profile_apply_batch)(const struct RhhWildDormantProfileBatchRequest *, struct RhhWildDormantProfileBatchResult *out);
    int (*profile_batch_context)(struct RhhWildDormantProfileBatchResult *out);
    /* v1.6: read and acknowledge bounded player-step work. clock/context
     * return zero or -1; advance returns RhhWildDormantClockStatus. All
     * leave outputs unchanged on failure. */
    int (*clock)(const struct RhhWildActorIndividual *, struct RhhWildDormantClock *out);
    int (*clock_advance)(const struct RhhWildDormantClockRequest *, struct RhhWildDormantClockChange *out);
    int (*clock_context)(struct RhhWildDormantClockChange *out);
    /* v1.7: commit profile, location and checkpoint as one transaction.
     * work_context is available during its profile constraints and its
     * relocation, profile-edited and clock-advanced notifications. */
    int (*work_preview)(const struct RhhWildDormantWorkRequest *, struct RhhWildDormantWorkResult *out);
    int (*work_apply)(const struct RhhWildDormantWorkRequest *, struct RhhWildDormantWorkResult *out);
    int (*work_context)(struct RhhWildDormantWorkResult *out);
};
#define RHH_WILD_DORMANT_SERVICE_V1_0_SIZE offsetof(struct RhhWildDormantService, relocate)
#define RHH_WILD_DORMANT_SERVICE_V1_1_SIZE offsetof(struct RhhWildDormantService, step_preview)
#define RHH_WILD_DORMANT_SERVICE_V1_2_SIZE offsetof(struct RhhWildDormantService, profile_at)
#define RHH_WILD_DORMANT_SERVICE_V1_3_SIZE offsetof(struct RhhWildDormantService, profile_preview)
#define RHH_WILD_DORMANT_SERVICE_V1_4_SIZE offsetof(struct RhhWildDormantService, profile_preview_batch)
#define RHH_WILD_DORMANT_SERVICE_V1_5_SIZE offsetof(struct RhhWildDormantService, clock)
#define RHH_WILD_DORMANT_SERVICE_V1_6_SIZE offsetof(struct RhhWildDormantService, work_preview)
#define RHH_WILD_DORMANT_SERVICE_V1_7_SIZE sizeof(struct RhhWildDormantService)
/* v1.8 adds RHH_WILD_DORMANT_WORK_GUARD_PROFILE without changing the layout. */
#define RHH_WILD_DORMANT_SERVICE_V1_8_SIZE RHH_WILD_DORMANT_SERVICE_V1_7_SIZE
enum RhhWildConcealMode { RHH_WILD_CONCEAL_NONE = 0, RHH_WILD_CONCEAL_HIDDEN, RHH_WILD_CONCEAL_GROUND };
enum RhhWildConcealTrigger { RHH_WILD_REVEAL_ON_MOVE = 1u, RHH_WILD_REVEAL_ON_APPROACH = 2u };
enum RhhWildConcealCover { RHH_WILD_COVER_TALL_GRASS = 1u, RHH_WILD_COVER_LONG_GRASS = 2u,
    RHH_WILD_COVER_SHORT_GRASS = 4u, RHH_WILD_COVER_SAND = 8u, RHH_WILD_COVER_SHALLOW_WATER = 16u };
enum RhhWildConcealStatus { RHH_WILD_CONCEAL_OK = 0, RHH_WILD_CONCEAL_INVALID,
    RHH_WILD_CONCEAL_UNAVAILABLE, RHH_WILD_CONCEAL_STALE, RHH_WILD_CONCEAL_CONFLICT, RHH_WILD_CONCEAL_DENIED };
enum RhhWildConcealCause { RHH_WILD_CONCEAL_SET = 1, RHH_WILD_CONCEAL_CHANGED,
    RHH_WILD_CONCEAL_CLEARED, RHH_WILD_CONCEAL_MOVED, RHH_WILD_CONCEAL_APPROACHED };
struct RhhWildConcealment {
    uint32_t struct_size, mode, reveal_flags, reveal_radius, cover, reserved;
    uint64_t revision;
    RhhContentKey owner;
};
struct RhhWildConcealRequest {
    uint32_t struct_size, mode, reveal_flags, reveal_radius;
    uint64_t handle, expected_revision;
    RhhContentKey owner;
};
struct RhhWildConcealResult {
    uint32_t struct_size, status, cause, is_preview;
    struct RhhWildActorSnapshot actor;
    struct RhhWildActorIndividual individual;
    struct RhhWildConcealment before, after;
    char reason[96];
};
/* "rhh:service/wild_concealment", version 1.0. Read-only query, pure
 * preview, and validated update; context is copied only in the corresponding
 * constraint or committed-change notification. */
struct RhhWildConcealService {
    struct RhhServiceHeader header;
    int (*at)(uint64_t handle, struct RhhWildConcealment *out);
    int (*preview)(const struct RhhWildConcealRequest *, struct RhhWildConcealResult *out);
    int (*apply)(const struct RhhWildConcealRequest *, struct RhhWildConcealResult *out);
    int (*context)(struct RhhWildConcealResult *out);
};
/* Player and wild endpoints are checked against the current map on every
 * query. Wild endpoints use lifetime-scoped wild-actor handles. */
enum RhhPerceptionEndpointKind { RHH_PERCEPTION_PLAYER = 1, RHH_PERCEPTION_WILD };
struct RhhPerceptionEndpoint { uint32_t kind, reserved; uint64_t wild_handle; };
enum RhhPerceptionChecks {
    RHH_PERCEPTION_CHECK_SCENE = 1u, RHH_PERCEPTION_CHECK_COLLISION = 2u,
    RHH_PERCEPTION_CHECK_ACTORS = 4u, RHH_PERCEPTION_CHECK_FACING = 8u
};
enum RhhPerceptionFlags {
    RHH_PERCEPTION_LINE_CLEAR = 1u, RHH_PERCEPTION_PERCEIVABLE = 2u,
    RHH_PERCEPTION_OUT_OF_RANGE = 4u, RHH_PERCEPTION_ELEVATION_MISMATCH = 8u,
    RHH_PERCEPTION_OUTSIDE_FACING = 16u, RHH_PERCEPTION_TARGET_HIDDEN = 32u,
    RHH_PERCEPTION_TARGET_OFFSCREEN = 64u, RHH_PERCEPTION_TARGET_NO_SPRITE = 128u,
    RHH_PERCEPTION_TARGET_SPRITE_HIDDEN = 256u, RHH_PERCEPTION_SCENE_UNANNOTATED = 512u
};
enum RhhPerceptionBlockers {
    RHH_PERCEPTION_BLOCK_SCENE = 1u, RHH_PERCEPTION_BLOCK_COLLISION = 2u,
    RHH_PERCEPTION_BLOCK_ACTOR = 4u
};
struct RhhPerceptionRequest {
    uint32_t struct_size, checks, max_distance, reserved;
    struct RhhPerceptionEndpoint viewer, target;
};
struct RhhPerceptionResult {
    uint32_t struct_size, flags, distance, checks;
    RhhContentKey map;
    int32_t viewer_x, viewer_y, target_x, target_y, blocker_x, blocker_y;
    uint32_t viewer_elevation, target_elevation, viewer_facing, target_cover;
    uint32_t blocker_flags, blocker_actor_slot, target_conceal_mode, reserved;
    RhhContentKey blocker_scene_rule;
    struct RhhWildActorSnapshot viewer_wild, target_wild;
    struct RhhWildActorIndividual viewer_individual, target_individual;
};
/* "rhh:service/perception", version 1.0. A pure, bounded metatile sight
 * query; it reports logical perception and presentation visibility separately. */
struct RhhPerceptionService {
    struct RhhServiceHeader header;
    int (*query)(const struct RhhPerceptionRequest *, struct RhhPerceptionResult *out);
};
#define RHH_SCOUT_MAX_RECORDS 256u
enum RhhScoutingAction { RHH_SCOUT_OBSERVE = 1, RHH_SCOUT_IDENTIFY };
enum RhhScoutingFlags { RHH_SCOUT_IDENTIFIED = 1u };
enum RhhScoutingStatus { RHH_SCOUT_OK = 0, RHH_SCOUT_INVALID, RHH_SCOUT_UNAVAILABLE,
    RHH_SCOUT_NOT_VISIBLE, RHH_SCOUT_NOT_OBSERVED, RHH_SCOUT_FULL, RHH_SCOUT_DENIED,
    RHH_SCOUT_STALE, RHH_SCOUT_EVIDENCE_USED, RHH_SCOUT_EVIDENCE_QUALITY };
struct RhhScoutingRecord {
    uint32_t struct_size, flags, observations, reserved;
    struct RhhWildActorIndividual individual;
    RhhContentKey species, first_map;
    uint64_t first_step;
};
struct RhhScoutingRequest {
    uint32_t struct_size, action, checks, max_distance;
    uint64_t handle;
};
struct RhhScoutingResult {
    uint32_t struct_size, status, is_preview, changed, action, reserved;
    struct RhhScoutingRecord before, after;
    struct RhhPerceptionResult sight;
    char reason[96];
};
struct RhhScoutingPhotoRequest {
    uint32_t struct_size, reserved;
    uint64_t ticket;
    struct RhhScoutingRequest scout;
};
/* "rhh:service/scouting", version 1.3. Records follow normal game saves.
 * A committed event is not a rollback-proof reward or photo identifier. */
struct RhhScoutingService {
    struct RhhServiceHeader header;
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhScoutingRecord *out);
    int (*find)(const struct RhhWildActorIndividual *, struct RhhScoutingRecord *out);
    int (*preview)(const struct RhhScoutingRequest *, struct RhhScoutingResult *out);
    int (*apply)(const struct RhhScoutingRequest *, struct RhhScoutingResult *out);
    int (*context)(struct RhhScoutingResult *out);
    /* These v1.1 operations accept only exact subject masks. The latest
     * capture is short-lived proof only when a subject mask showed
     * visible pixels. The actor, sprite, map, step, camera and billboard must still
     * match; normal perception and policy apply. Each capture can commit at
     * most one observation and one identification. photo_context() copies
     * the ticket only during the corresponding proposal or notification. */
    int (*preview_photo)(const struct RhhScoutingPhotoRequest *, struct RhhScoutingResult *out);
    int (*apply_photo)(const struct RhhScoutingPhotoRequest *, struct RhhScoutingResult *out);
    int (*photo_context)(uint64_t *ticket);
    /* During photo-based scouting events, returns enum RhhPhotoMaskQuality. */
    int (*photo_quality)(uint32_t *quality);
    /* v1.3: explicitly also accept approximate pre-filter geometric evidence.
     * Policy hooks receive the normal scouting events and can inspect quality. */
    int (*preview_photo_geometric)(const struct RhhScoutingPhotoRequest *, struct RhhScoutingResult *out);
    int (*apply_photo_geometric)(const struct RhhScoutingPhotoRequest *, struct RhhScoutingResult *out);
};
#define RHH_SCOUTING_SERVICE_V1_0_SIZE offsetof(struct RhhScoutingService, preview_photo)
#define RHH_SCOUTING_SERVICE_V1_1_SIZE offsetof(struct RhhScoutingService, photo_quality)
#define RHH_SCOUTING_SERVICE_V1_2_SIZE offsetof(struct RhhScoutingService, preview_photo_geometric)
#define RHH_SCOUTING_SERVICE_V1_3_SIZE sizeof(struct RhhScoutingService)
/* A single asynchronous, presentation-only subject visibility probe. It uses
 * the next available compatible presenter and does not capture a photo or
 * record an observation. Only the latest ticket is retained. Results are
 * frame-local hints; revalidate actual
 * gameplay actions and captures independently. */
enum RhhProjectedVisibilityStatus { RHH_PROJECTED_OK, RHH_PROJECTED_INVALID,
    RHH_PROJECTED_UNAVAILABLE, RHH_PROJECTED_PENDING, RHH_PROJECTED_STALE };
struct RhhProjectedVisibilityRequest {
    uint32_t struct_size, reserved;
    RhhContentKey owner;
    uint64_t actor_handle;
};
struct RhhProjectedVisibilityResult {
    uint32_t struct_size, status, frame, total_pixels, visible_pixels, reserved;
    RhhContentKey owner;
    uint64_t actor_handle, ticket, run_id;
};
struct RhhProjectedVisibilityService {
    struct RhhServiceHeader header;
    int (*request)(const struct RhhProjectedVisibilityRequest *, struct RhhProjectedVisibilityResult *out);
    int (*result)(uint64_t ticket, struct RhhProjectedVisibilityResult *out);
    int (*cancel)(RhhContentKey owner, uint64_t ticket);
    /* v1.1: accepts explicitly declared geometric masks through non-displacing
     * filters. Counts describe the pre-filter mask, not exact output pixels. */
    int (*request_geometric)(const struct RhhProjectedVisibilityRequest *, struct RhhProjectedVisibilityResult *out);
    /* Returns enum RhhPhotoMaskQuality for a completed, current result. */
    int (*quality)(uint64_t ticket, uint32_t *quality);
};
#define RHH_PROJECTED_VISIBILITY_SERVICE_V1_0_SIZE offsetof(struct RhhProjectedVisibilityService, request_geometric)
#define RHH_PROJECTED_VISIBILITY_SERVICE_V1_1_SIZE sizeof(struct RhhProjectedVisibilityService)
#define RHH_PHOTO_WIDTH 240u
#define RHH_PHOTO_HEIGHT 160u
#define RHH_PHOTO_RGB_BYTES (RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT * 3u)
enum RhhPhotoStatus { RHH_PHOTO_OK = 0, RHH_PHOTO_INVALID, RHH_PHOTO_UNAVAILABLE,
    RHH_PHOTO_NOT_VISIBLE, RHH_PHOTO_DENIED, RHH_PHOTO_STALE, RHH_PHOTO_RENDER_FAILED };
enum RhhPhotoBackend { RHH_PHOTO_BACKEND_NONE, RHH_PHOTO_BACKEND_SOFTWARE, RHH_PHOTO_BACKEND_OPENGL };
struct RhhPhotoRequest {
    uint32_t struct_size, checks, max_distance, reserved;
    RhhContentKey owner;
    uint64_t actor_handle;
};
struct RhhPhotoCapture {
    uint32_t struct_size, status, width, height, stride, backend, frame, reserved;
    RhhContentKey owner;
    uint64_t ticket;
    struct RhhPerceptionResult sight;
    char reason[96];
};
enum RhhPhotoRequestMode { RHH_PHOTO_REQUEST_ORDINARY, RHH_PHOTO_REQUEST_PRESENTED,
    RHH_PHOTO_REQUEST_PRESENTED_GEOMETRIC, RHH_PHOTO_REQUEST_ORDINARY_GEOMETRIC };
enum RhhPhotoRenderFlags { RHH_PHOTO_RENDER_DEPTH_TEXTURE = 1u };
/* Applied world passes are ordered as rendered; the environment shader runs
 * before them. A zero presenter means the original software scene was used.
 * DEPTH_TEXTURE means presenter depth was available to world passes; it does not
 * guarantee the separate photo-depth readback succeeded. Overlay UI is not
 * part of the captured image. */
struct RhhPhotoRenderContext {
    uint32_t struct_size, backend, flags, frame, pass_count, reserved;
    uint64_t ticket;
    RhhContentKey presenter, environment_shader;
    struct RhhWorldRenderPass passes[RHH_WORLD_RENDER_PASS_CAPACITY];
};
/* "rhh:service/photo_capture", version 1.5. One pending request; pixels are
 * copied only during RHH_PHOTO_SCORE and RHH_PHOTO_CAPTURED. Tickets are process-local. */
struct RhhPhotoService {
    struct RhhServiceHeader header;
    int (*request)(const struct RhhPhotoRequest *, struct RhhPhotoCapture *out);
    int (*context)(struct RhhPhotoCapture *out);
    int (*copy_rgb)(uint8_t *out, uint32_t size);
    /* v1.1: allow a normally culled actor as a provisional target. Capture
     * succeeds only if a compatible active presenter supplies a valid mask
     * with at least one visible subject pixel for that exact capture frame.
     * The original sight, identity, permission and concealment rules apply.
     * requires_presenter() is available during the matching photo callbacks. */
    int (*request_presented)(const struct RhhPhotoRequest *, struct RhhPhotoCapture *out);
    int (*requires_presenter)(uint32_t *out);
    /* v1.2: explicitly accepts a pre-filter geometric subject mask. */
    int (*request_presented_geometric)(const struct RhhPhotoRequest *, struct RhhPhotoCapture *out);
    /* Reports enum RhhPhotoRequestMode during the matching photo callbacks,
     * including RHH_CAN_PHOTO before a capture has been accepted. */
    int (*request_mode)(uint32_t *out);
    /* v1.3: copied render provenance during PHOTO_SCORE or PHOTO_CAPTURED.
     * Fails without changing out if the exact frame cannot be verified. */
    int (*render_context)(struct RhhPhotoRenderContext *out);
    /* v1.4: selected presenter's capture-scoped variant, or zero when it did
     * not publish one. Same callback and failure rules as render_context. */
    int (*presenter_variant)(RhhContentKey *out);
    /* v1.5: ordinary visibility checks, with optional geometric evidence.
     * Capture still succeeds when no compatible approximate mask is available. */
    int (*request_geometric)(const struct RhhPhotoRequest *, struct RhhPhotoCapture *out);
};
#define RHH_PHOTO_SERVICE_V1_0_SIZE offsetof(struct RhhPhotoService, request_presented)
#define RHH_PHOTO_SERVICE_V1_1_SIZE offsetof(struct RhhPhotoService, request_presented_geometric)
#define RHH_PHOTO_SERVICE_V1_2_SIZE offsetof(struct RhhPhotoService, render_context)
#define RHH_PHOTO_SERVICE_V1_3_SIZE offsetof(struct RhhPhotoService, presenter_variant)
#define RHH_PHOTO_SERVICE_V1_4_SIZE offsetof(struct RhhPhotoService, request_geometric)
#define RHH_PHOTO_SERVICE_V1_5_SIZE sizeof(struct RhhPhotoService)
/* Topmost-layer ownership in a software-rendered photo, after window and sprite/BG
 * priority compositing. Counts are clipped to the 240x160 viewport; total is
 * the subject's nontransparent sprite silhouette before occlusion. Blending,
 * color, lighting and postprocessing are not measured. During PHOTO_SCORE or
 * PHOTO_CAPTURED only. An OpenGL capture can use the same mask when it
 * presents the unchanged software frame or a presenter supplies both subject
 * masks. World effects are supported only when every active shader declares
 * position preservation. Stale and untracked frames are unavailable.
 * The optional mask has one byte per pixel (0 or 1), row-major, top to bottom. */
struct RhhPhotoVisibility {
    uint32_t struct_size, total_pixels, visible_pixels, reserved;
    uint64_t ticket;
};
/* Rendered RGB measurements for visible subject pixels. Luma uses integer
 * (54*R + 183*G + 19*B) / 256; dark and bright count luma <=16 and >=240.
 * This measures the captured image, not scene illumination or sprite color in
 * isolation. Empty masks have zero for all measurements. */
struct RhhPhotoExposure {
    uint32_t struct_size, visible_pixels, mean_luma, min_luma, max_luma;
    uint32_t dark_pixels, bright_pixels, reserved;
    uint64_t ticket;
};
/* Half-open pixel bounds in the viewport. An empty visible mask has zero
 * bounds. Silhouette bounds describe only pixels inside the viewport; touching
 * an edge does not prove that the sprite extends outside it. */
struct RhhPhotoBounds { uint32_t left, top, right, bottom; };
struct RhhPhotoFraming {
    uint32_t struct_size, reserved;
    uint64_t ticket;
    struct RhhPhotoBounds silhouette, visible;
};
/* On-screen distribution of mask pixels. Each pixel contributes its center,
 * expressed in half-pixel units: x2_sum += 2*x+1 and y2_sum += 2*y+1.
 * Divide each sum by 2*count for the centroid; empty masks have zero sums.
 * Grid cells are row-major with column = x*3/240 and row = y*3/160. */
struct RhhPhotoDistribution {
    uint32_t pixels, x2_sum, y2_sum, cells[9];
};
struct RhhPhotoComposition {
    uint32_t struct_size, reserved;
    uint64_t ticket;
    struct RhhPhotoDistribution silhouette, visible;
};
/* Adjacent visible subject pixels only; each horizontal and vertical pair is
 * counted once. Luma uses the same formula as RhhPhotoExposure. High contrast
 * means an absolute luma difference >= 32. Empty or isolated masks have zero
 * counts. This is image contrast, not an intrinsic sharpness or focus score:
 * the subject's texture, lighting and postprocessing also affect it. */
struct RhhPhotoDetail {
    uint32_t struct_size, neighbor_pairs, luma_delta_sum, high_contrast_pairs;
    uint32_t max_luma_delta, reserved;
    uint64_t ticket;
};
/* Presenter depth for visible subject pixels, in the presenter's normalized
 * depth-buffer units (0..65535), not world distance or a focus score. The
 * mask quality identifies exact or explicitly opted-in approximate geometry. */
struct RhhPhotoDepth {
    uint32_t struct_size, mask_quality, visible_pixels, min_depth, max_depth, reserved;
    uint64_t ticket, depth_sum;
};
/* "rhh:service/photo_visibility", version 1.8. Exact methods fail when no
 * exact subject mask is available; geometric methods require an explicitly
 * opted-in approximate mask. All calls are scoped to PHOTO_SCORE and
 * PHOTO_CAPTURED. */
struct RhhPhotoVisibilityService {
    struct RhhServiceHeader header;
    int (*context)(struct RhhPhotoVisibility *out);
    int (*copy_mask)(uint8_t *out, uint32_t size);
    int (*exposure)(struct RhhPhotoExposure *out);
    /* v1.2: pre-occlusion silhouette mask and its viewport bounds. */
    int (*copy_silhouette)(uint8_t *out, uint32_t size);
    int (*framing)(struct RhhPhotoFraming *out);
    /* v1.3: full pre-occlusion silhouette and signed bounds when available. */
    int (*footprint)(struct RhhPhotoFootprint *out);
    /* v1.4: pixel distribution of the on-screen silhouette and visible mask. */
    int (*composition)(struct RhhPhotoComposition *out);
    /* v1.5: rendered contrast between adjacent visible subject pixels. */
    int (*detail)(struct RhhPhotoDetail *out);
    /* v1.6: explicitly approximate pre-filter geometry, available only when
     * exact masks are unavailable and the capture opted into geometric masks. */
    int (*geometric_context)(struct RhhPhotoVisibility *out);
    int (*copy_geometric_mask)(uint8_t *out, uint32_t size);
    int (*copy_geometric_silhouette)(uint8_t *out, uint32_t size);
    /* v1.7: same-frame presenter depth. copy_depth writes 240*160 uint16_t
     * samples, top row first; pixels outside the visible mask are zero. Size
     * is measured in samples. Consult the matching mask to distinguish zero
     * depth from an excluded pixel. Available only during scoring/capture. */
    int (*depth)(struct RhhPhotoDepth *out);
    int (*copy_depth)(uint16_t *out, uint32_t size);
    /* v1.8: metrics using the explicitly opted-in pre-filter geometric mask.
     * Exposure and detail sample the final image at those approximate subject
     * locations. Framing and footprint describe pre-filter geometry, not the
     * pixels spread by blur. These methods do not fall back to exact masks. */
    int (*geometric_exposure)(struct RhhPhotoExposure *out);
    int (*geometric_framing)(struct RhhPhotoFraming *out);
    int (*geometric_footprint)(struct RhhPhotoFootprint *out);
    int (*geometric_composition)(struct RhhPhotoComposition *out);
    int (*geometric_detail)(struct RhhPhotoDetail *out);
};
#define RHH_PHOTO_VISIBILITY_SERVICE_V1_0_SIZE offsetof(struct RhhPhotoVisibilityService, exposure)
#define RHH_PHOTO_VISIBILITY_SERVICE_V1_1_SIZE offsetof(struct RhhPhotoVisibilityService, copy_silhouette)
#define RHH_PHOTO_VISIBILITY_SERVICE_V1_2_SIZE offsetof(struct RhhPhotoVisibilityService, footprint)
#define RHH_PHOTO_VISIBILITY_SERVICE_V1_3_SIZE offsetof(struct RhhPhotoVisibilityService, composition)
#define RHH_PHOTO_VISIBILITY_SERVICE_V1_4_SIZE offsetof(struct RhhPhotoVisibilityService, detail)
#define RHH_PHOTO_VISIBILITY_SERVICE_V1_5_SIZE offsetof(struct RhhPhotoVisibilityService, geometric_context)
#define RHH_PHOTO_VISIBILITY_SERVICE_V1_6_SIZE offsetof(struct RhhPhotoVisibilityService, depth)
#define RHH_PHOTO_VISIBILITY_SERVICE_V1_7_SIZE offsetof(struct RhhPhotoVisibilityService, geometric_exposure)
#define RHH_PHOTO_VISIBILITY_SERVICE_V1_8_SIZE sizeof(struct RhhPhotoVisibilityService)
#define RHH_PHOTO_ALBUM_CAPACITY 99u
struct RhhPhotoId { uint64_t run_id, serial; };
struct RhhPhotoAlbumEntry {
    uint32_t struct_size, backend, frame, reserved;
    struct RhhPhotoId id;
    struct RhhWildActorIndividual individual;
    RhhContentKey species, map, owner;
    uint64_t step;
};
enum RhhPhotoAlbumStatus { RHH_ALBUM_OK = 0, RHH_ALBUM_INVALID, RHH_ALBUM_UNAVAILABLE,
    RHH_ALBUM_FULL, RHH_ALBUM_DUPLICATE };
/* "rhh:service/photo_album", version 1.0. store is available only during
 * RHH_PHOTO_CAPTURED for that ticket. Images follow normal save generations. */
struct RhhPhotoAlbumService {
    struct RhhServiceHeader header;
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhPhotoAlbumEntry *out);
    int (*find)(const struct RhhPhotoId *, struct RhhPhotoAlbumEntry *out);
    int (*copy_rgb)(const struct RhhPhotoId *, uint8_t *out, uint32_t size);
    int (*store)(uint64_t ticket, struct RhhPhotoAlbumEntry *out);
    int (*remove)(const struct RhhPhotoId *);
    int (*context)(struct RhhPhotoAlbumEntry *out);
};
#define RHH_PHOTO_SCORE_MAX 1000u
enum RhhPhotoScoreFlags { RHH_PHOTO_SCORE_AVAILABLE = 1u, RHH_PHOTO_SCORE_ESTIMATED = 2u,
    RHH_PHOTO_SCORE_GROUND_COVER = 4u, RHH_PHOTO_SCORE_UNANNOTATED = 8u,
    RHH_PHOTO_SCORE_PARTIAL_CHECKS = 16u };
struct RhhPhotoScore {
    uint32_t struct_size, flags, base, value, distance, cover, checks, reserved;
    struct RhhPhotoId photo;
};
struct RhhPhotoScoreContext {
    uint32_t struct_size, habitat_available, time_available, reserved;
    struct RhhPhotoCapture capture;
    struct RhhPhotoScore score;
    struct RhhHabitatSample habitat;
    struct RhhWorldTimeSnapshot time;
};
/* The most recent successful capture is ephemeral and may have no album entry.
 * Its ID was reserved durably, but this metadata is discarded on load or when
 * another successful capture replaces it. A missing ID means reservation failed. */
struct RhhPhotoRecent {
    uint32_t struct_size, reserved;
    uint64_t ticket;
    struct RhhPhotoId photo;
    struct RhhWildActorIndividual individual;
    struct RhhPhotoScore score;
};
/* "rhh:service/photo_score", version 1.1. During RHH_PHOTO_SCORE, reducers
 * can transform decision.value (0..1000) or deny scoring; the photo remains
 * available. context also supplies the final score during PHOTO_CAPTURED. */
struct RhhPhotoScoreService {
    struct RhhServiceHeader header;
    int (*context)(struct RhhPhotoScoreContext *out);
    int (*find)(const struct RhhPhotoId *, struct RhhPhotoScore *out);
    /* v1.1: copy the last capture's metadata, including an unavailable score. */
    int (*recent)(struct RhhPhotoRecent *out);
};
#define RHH_PHOTO_SCORE_SERVICE_V1_0_SIZE offsetof(struct RhhPhotoScoreService, recent)
#define RHH_PHOTO_SCORE_SERVICE_V1_1_SIZE sizeof(struct RhhPhotoScoreService)
/* Durable item rewards for photos of identified wild actors.
 * A rule may award each run-scoped individual once per owner. */
#define RHH_PHOTO_REWARD_CAPACITY 128u
enum RhhPhotoRewardStatus { RHH_PHOTO_REWARD_OK, RHH_PHOTO_REWARD_INVALID,
    RHH_PHOTO_REWARD_UNAVAILABLE, RHH_PHOTO_REWARD_NOT_ELIGIBLE, RHH_PHOTO_REWARD_ALREADY,
    RHH_PHOTO_REWARD_DENIED, RHH_PHOTO_REWARD_BAG_FULL, RHH_PHOTO_REWARD_PENDING,
    RHH_PHOTO_REWARD_FULL, RHH_PHOTO_REWARD_PERSISTENCE_FAILED, RHH_PHOTO_REWARD_STALE };
struct RhhPhotoRewardRequest {
    uint32_t struct_size, quantity, minimum_score, reserved;
    RhhContentKey owner, rule, item;
    struct RhhPhotoId photo;
};
/* Simple ordinary-save rewards. A rule awards a species once per location;
 * the photo must be in the album. claim is called by the owner during its
 * RHH_MOD_UPDATE observer. The item and claim flag follow normal save rollback. */
struct RhhSimplePhotoRewardService {
    struct RhhServiceHeader header;
    int (*claim)(const struct RhhPhotoRewardRequest *request);
};
#define RHH_SIMPLE_PHOTO_REWARD_SERVICE_V1_0_SIZE sizeof(struct RhhSimplePhotoRewardService)
struct RhhPhotoRewardReceipt {
    uint32_t struct_size, quantity, score, delivered;
    uint64_t sequence;
    struct RhhPhotoId photo;
    struct RhhWildActorIndividual individual;
    RhhContentKey owner, rule, item;
};
struct RhhPhotoRewardResult {
    uint32_t struct_size, status, is_preview, changed;
    struct RhhPhotoRewardReceipt receipt;
    char reason[96];
};
enum RhhPhotoExperienceFlags { RHH_PHOTO_EXPERIENCE_ALLOW_UNSCORED = 1u };
struct RhhPhotoExperienceRequest {
    uint32_t struct_size, amount, minimum_score, flags;
    RhhContentKey owner, rule;
    struct RhhPhotoId photo;
    struct RhhOwnedExperience recipient;
};
struct RhhPhotoExperienceReceipt {
    uint32_t struct_size, amount, score, score_available, delivered, reserved;
    uint64_t sequence, recipient_identity;
    struct RhhPhotoId photo;
    struct RhhWildActorIndividual individual;
    RhhContentKey owner, rule;
};
struct RhhPhotoExperienceResult {
    uint32_t struct_size, status, is_preview, changed;
    struct RhhPhotoExperienceReceipt receipt;
    struct RhhExperiencePreview experience;
    char reason[96];
};
#define RHH_PHOTO_EXPERIENCE_RETARGET_CAPACITY 128u
struct RhhPhotoExperienceRetargetRequest {
    uint32_t struct_size, reserved;
    struct RhhPhotoExperienceReceipt expected;
    struct RhhOwnedExperience recipient;
};
struct RhhPhotoExperienceRetargetResult {
    uint32_t struct_size, status, is_preview, changed;
    struct RhhPhotoExperienceReceipt before, after;
    struct RhhExperiencePreview experience;
    char reason[96];
};
/* "rhh:service/photo_reward" v1.3. A claim is durably recorded before the
 * item enters the normal bag; delivery is replayed after loading an older
 * save. One undelivered receipt blocks later claims until it can be delivered.
 * A failed durable write changes neither the ledger nor the bag. */
struct RhhPhotoRewardService {
    struct RhhServiceHeader header;
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhPhotoRewardReceipt *out);
    int (*preview)(const struct RhhPhotoRewardRequest *, struct RhhPhotoRewardResult *out);
    int (*claim)(const struct RhhPhotoRewardRequest *, struct RhhPhotoRewardResult *out);
    int (*deliver)(struct RhhPhotoRewardResult *out);
    int (*context)(struct RhhPhotoRewardResult *out);
    /* v1.1: these variants also accept unscored photos when minimum_score is
     * zero. The v1.0 methods continue to require an available score. Legacy
     * receipts report zero for an unscored photo; use score_available to
     * distinguish it from a scored zero. context_score_available works during
     * the reward constraint and committed notifications. */
    int (*preview_allow_unscored)(const struct RhhPhotoRewardRequest *, struct RhhPhotoRewardResult *out);
    int (*claim_allow_unscored)(const struct RhhPhotoRewardRequest *, struct RhhPhotoRewardResult *out);
    int (*score_available)(uint32_t index, uint32_t *out);
    int (*context_score_available)(uint32_t *out);
    /* v1.2: durable experience claims, delivered through the party level-up
     * flow to a uniquely matched OT/personality. This is not a permanent ID. */
    uint32_t (*experience_count)(void);
    int (*experience_at)(uint32_t index, struct RhhPhotoExperienceReceipt *out);
    int (*preview_experience)(const struct RhhPhotoExperienceRequest *, struct RhhPhotoExperienceResult *out);
    int (*claim_experience)(const struct RhhPhotoExperienceRequest *, struct RhhPhotoExperienceResult *out);
    int (*deliver_experience)(struct RhhPhotoExperienceResult *out);
    int (*experience_context)(struct RhhPhotoExperienceResult *out);
    /* v1.3: change the recipient of a pending experience receipt. */
    int (*preview_experience_retarget)(const struct RhhPhotoExperienceRetargetRequest *, struct RhhPhotoExperienceRetargetResult *out);
    int (*retarget_experience)(const struct RhhPhotoExperienceRetargetRequest *, struct RhhPhotoExperienceRetargetResult *out);
    int (*experience_retarget_context)(struct RhhPhotoExperienceRetargetResult *out);
};
#define RHH_PHOTO_REWARD_SERVICE_V1_0_SIZE offsetof(struct RhhPhotoRewardService, preview_allow_unscored)
#define RHH_PHOTO_REWARD_SERVICE_V1_1_SIZE offsetof(struct RhhPhotoRewardService, experience_count)
#define RHH_PHOTO_REWARD_SERVICE_V1_2_SIZE offsetof(struct RhhPhotoRewardService, preview_experience_retarget)
#define RHH_PHOTO_REWARD_SERVICE_V1_3_SIZE sizeof(struct RhhPhotoRewardService)
struct RhhWorldActor {
    uint32_t struct_size, slot, local_id, graphics_id;
    RhhContentKey map;
    int32_t x, y, previous_x, previous_y, screen_x, screen_y;
    uint32_t elevation, facing, movement, flags, animation, animation_frame;
    uint32_t sprite_width, sprite_height, sprite_priority, sprite_subpriority;
};
enum RhhWorldActorFlags {
    RHH_ACTOR_PLAYER = 1u, RHH_ACTOR_HIDDEN = 2u, RHH_ACTOR_OFFSCREEN = 4u,
    RHH_ACTOR_IN_SHORT_GRASS = 8u, RHH_ACTOR_IN_SHALLOW_WATER = 16u, RHH_ACTOR_IN_SAND_PILE = 32u
};
/* Raw visibility inputs, not a replacement for RHH_ACTOR_HIDDEN. The engine
 * normally sets SPRITE_HIDDEN when it culls an off-screen object, but other
 * code may also set it independently. OBJECT_HIDDEN and CONCEALED must always
 * be respected by custom presenters. ENGINE_MOVEMENT means the current sprite
 * callback matches the object's native movement type; it is not proof that
 * another caller did not also set SPRITE_HIDDEN. Views are frame-local. */
enum RhhWorldActorVisibilityFlags {
    RHH_WORLD_ACTOR_CULLED = 1u, RHH_WORLD_ACTOR_OBJECT_HIDDEN = 2u,
    RHH_WORLD_ACTOR_SPRITE_HIDDEN = 4u, RHH_WORLD_ACTOR_CONCEALED = 8u,
    RHH_WORLD_ACTOR_LINK_ACTIVE = 16u, RHH_WORLD_ACTOR_ENGINE_MOVEMENT = 32u
};
struct RhhWorldActorVisibility {
    uint32_t struct_size, slot, flags, reserved;
};
/* World-scene coordinates are measured in pixels, with (0,0) at the top left
 * of the current map and +y down. Origin projects map-local pixels into the
 * 240x160 field viewport: screen = world + origin. Camera data is transient. */
struct RhhWorldCamera {
    uint32_t struct_size, viewport_width, viewport_height, tile_pixels;
    RhhContentKey map;
    int32_t origin_x, origin_y;
};
/* One coherent, frame-local observation shared by every light-offer callback
 * and the selected presenter. */
struct RhhWorldLightContext {
    uint32_t struct_size, reserved;
    uint64_t frame;
    struct RhhWorldCamera camera;
    struct RhhWorldLightingInputs lighting;
};
struct RhhWorldLightFrame {
    uint32_t struct_size, count, status, reserved;
    struct RhhWorldLightContext context;
    struct RhhWorldDynamicLight lights[RHH_WORLD_DYNAMIC_LIGHT_CAPACITY];
};
/* A billboard's ground anchor excludes temporary sprite x2/y2 offsets so a
 * jumping/levitating sprite can cast a shadow on the ground. The visual centre
 * includes those offsets. world_anchor is in the camera's map-pixel space. */
struct RhhWorldBillboard {
    uint32_t struct_size, slot, part_count, flags;
    int32_t anchor_x, anchor_y, world_anchor_x, world_anchor_y;
    int32_t visual_center_x, visual_center_y, offset_x, offset_y;
    uint32_t elevation, facing, subpriority, reserved;
};
enum RhhWorldPartFlags {
    RHH_WORLD_PART_AFFINE = 1u, RHH_WORLD_PART_SEMITRANSPARENT = 2u,
    RHH_WORLD_PART_WINDOW = 4u, RHH_WORLD_PART_MOSAIC = 8u,
    RHH_WORLD_PART_DISABLED = 16u
};
/* An OAM/subsprite component in original draw order. screen_x/y are the top
 * left before affine transformation. RGBA is tightly packed, top-left-origin
 * RGBA8888, with alpha 0 for transparent pixels and 255 otherwise. Non-affine
 * flips are applied; affine matrices, mosaic, blending and UI are not baked in. */
struct RhhWorldSpritePart {
    uint32_t struct_size, index, width, height;
    int32_t screen_x, screen_y;
    uint32_t priority, subpriority, flags, reserved;
};
/* OAM's signed 8.8 matrix maps coordinates relative to the draw rectangle's
 * center back into the source image. Non-affine flips are already baked into
 * the source RGBA. Double-size affine parts have a twice-sized draw rectangle.
 * Matrix and pixels describe current CPU sprite state, not latched hardware OAM. */
struct RhhWorldSpriteTransform {
    uint32_t struct_size, index, flags, source_width, source_height, draw_width, draw_height, matrix_index;
    int32_t screen_x, screen_y, a, b, c, d;
};
enum RhhWorldSpriteRasterFlags { RHH_WORLD_RASTER_MOSAIC = 1u };
/* Mosaic block dimensions are 1..16 pixels, supplied by the caller from the
 * desired display-state sample. Mosaic applies only to parts with the mosaic
 * flag and is anchored to screen coordinates before affine sampling. Results
 * are not clipped to the viewport or composited with other layers. */
struct RhhWorldSpriteRasterOptions {
    uint32_t struct_size, flags, mosaic_width, mosaic_height, reserved;
};
enum RhhWorldEffectKind { RHH_WORLD_EFFECT_OTHER = 0, RHH_WORLD_EFFECT_TALL_GRASS,
    RHH_WORLD_EFFECT_LONG_GRASS, RHH_WORLD_EFFECT_SHORT_GRASS,
    /* These kinds are returned by effect_at_v1_3 only. */
    RHH_WORLD_EFFECT_TRACKS, RHH_WORLD_EFFECT_RIPPLE, RHH_WORLD_EFFECT_SPLASH };
enum RhhWorldEffectFlags { RHH_WORLD_EFFECT_HIDDEN = 1u, RHH_WORLD_EFFECT_MAP_OFFSET = 2u };
/* A recognized engine field-effect sprite, identified by its live sprite slot.
 * template_index is an engine ordinal, not a persistent identity. Anchor and
 * center coordinates include camera and transient sprite offsets. */
struct RhhWorldEffectSprite {
    uint32_t struct_size, slot, kind, template_index, flags, part_count;
    int32_t center_x, center_y, anchor_x, anchor_y, world_anchor_x, world_anchor_y;
};
enum RhhWorldShadowSize { RHH_WORLD_SHADOW_NONE = 0, RHH_WORLD_SHADOW_SMALL,
    RHH_WORLD_SHADOW_MEDIUM, RHH_WORLD_SHADOW_LARGE };
enum RhhWorldShadowSuppression {
    RHH_WORLD_SHADOW_HIDDEN = 1u, RHH_WORLD_SHADOW_DISABLED = 2u,
    RHH_WORLD_SHADOW_GRAPHICS_NONE = 4u, RHH_WORLD_SHADOW_WEATHER = 8u,
    RHH_WORLD_SHADOW_WATER = 16u, RHH_WORLD_SHADOW_GRASS = 32u,
    RHH_WORLD_SHADOW_HOT_SPRINGS = 64u, RHH_WORLD_SHADOW_SAND_PILE = 128u
};
enum RhhWorldShadowEffectFlags { RHH_WORLD_SHADOW_EFFECT_ACTIVE = 1u, RHH_WORLD_SHADOW_EFFECT_VISIBLE = 2u };
/* A copied actor shadow policy and the corresponding live engine effect, if
 * one exists. effect_slot is UINT32_MAX when absent; slots are frame-local.
 * Suppressions describe engine conditions, independent of scene materials. */
struct RhhWorldActorShadow {
    uint32_t struct_size, slot, size, suppressions;
    uint32_t effect_slot, effect_flags, reserved[2];
};
enum RhhWorldDisplayLayer { RHH_WORLD_DISPLAY_BG0 = 1u, RHH_WORLD_DISPLAY_BG1 = 2u,
    RHH_WORLD_DISPLAY_BG2 = 4u, RHH_WORLD_DISPLAY_BG3 = 8u, RHH_WORLD_DISPLAY_OBJ = 16u,
    RHH_WORLD_DISPLAY_BACKDROP = 32u };
enum RhhWorldDisplayControlFlags { RHH_WORLD_DISPLAY_FORCED_BLANK = 0x0080u,
    RHH_WORLD_DISPLAY_BG0_ON = 0x0100u, RHH_WORLD_DISPLAY_BG1_ON = 0x0200u,
    RHH_WORLD_DISPLAY_BG2_ON = 0x0400u, RHH_WORLD_DISPLAY_BG3_ON = 0x0800u,
    RHH_WORLD_DISPLAY_OBJ_ON = 0x1000u, RHH_WORLD_DISPLAY_WIN0_ON = 0x2000u,
    RHH_WORLD_DISPLAY_WIN1_ON = 0x4000u, RHH_WORLD_DISPLAY_OBJWIN_ON = 0x8000u };
/* In a window mask, bits 0..4 use RhhWorldDisplayLayer and bit 5 enables
 * color effects; it does not mean BACKDROP as it does for blend targets. */
#define RHH_WORLD_WINDOW_COLOR_EFFECTS 32u
enum RhhWorldBlendMode { RHH_WORLD_BLEND_NONE, RHH_WORLD_BLEND_ALPHA,
    RHH_WORLD_BLEND_LIGHTEN, RHH_WORLD_BLEND_DARKEN };
/* Latched display-register values sampled at the call, not the CPU's pending
 * register buffer. Window endpoints and masks retain their packed hardware
 * representation. Mosaic widths/heights are pixel counts (1..16). Blend
 * coefficients are raw 5-bit values in sixteenths. Registers can change
 * during a frame, including per scanline; this is not a composited frame or a
 * guarantee that all rows used these values. Sprite pixels remain unblended. */
struct RhhWorldDisplayState {
    uint32_t struct_size, display_control;
    RhhContentKey map;
    uint32_t bg_control[4];
    uint32_t blend_control, blend_alpha, blend_y, mosaic;
    uint32_t win0_h, win0_v, win1_h, win1_v, win_in, win_out;
    uint32_t blend_mode, target1, target2, alpha_a, alpha_b, brightness;
    uint32_t bg_mosaic_width, bg_mosaic_height, obj_mosaic_width, obj_mosaic_height;
};
/* Raw latched controls sampled immediately before rendering one software
 * scanline. Window fields retain packed register form. */
struct RhhWorldDisplayScanline {
    uint16_t display_control, bg_control[4], blend_control, blend_alpha, blend_y, mosaic;
    uint16_t win0_h, win0_v, win1_h, win1_v, win_in, win_out;
};
#define RHH_WORLD_DISPLAY_SCANLINES 160u
/* Latest completed software frame captured on this map and run. sequence
 * changes on each completed captured frame and is process-local. This is not proof that
 * the frame was presented, or that current CPU sprites match its latched OAM. */
struct RhhWorldDisplayFrame {
    uint32_t struct_size, count;
    uint64_t sequence, run_id;
    RhhContentKey map;
    struct RhhWorldDisplayScanline rows[RHH_WORLD_DISPLAY_SCANLINES];
};
/* "rhh:service/overworld", version 1.11. sprite_rgba copies the current raw
 * OAM tile image, not affine transforms, subsprites or composited effects. */
struct RhhOverworldService {
    struct RhhServiceHeader header;
    uint32_t (*actor_slots)(void);
    int (*current)(struct RhhWorldSnapshot *out);
    int (*tile_at)(int32_t x, int32_t y, struct RhhWorldTile *out);
    int (*actor_at)(uint32_t slot, struct RhhWorldActor *out);
    int (*sprite_rgba)(uint32_t slot, uint8_t *rgba, uint32_t capacity);
    /* Decodes a tile entry from RhhWorldTile into 8x8 RGBA. */
    int (*tile_rgba)(uint16_t tile, uint8_t *rgba, uint32_t capacity);
    /* Version 1.1 append-only scene views; callers must request 1.1 and check
     * header.struct_size. A zero part count means the actor is unavailable or
     * has no drawable parts. All outputs are copied, valid for the current
     * frame, and never mutate the simulation. */
    int (*camera)(struct RhhWorldCamera *out);
    int (*billboard)(uint32_t slot, struct RhhWorldBillboard *out);
    int (*sprite_part)(uint32_t slot, uint32_t part, struct RhhWorldSpritePart *out);
    int (*sprite_part_rgba)(uint32_t slot, uint32_t part, uint8_t *rgba, uint32_t capacity);
    /* Version 1.2 exposes recognized field-effect sprites separately from
     * object-event actors. Slots are frame-local and are not stable IDs. */
    uint32_t (*effect_slots)(void);
    int (*effect_at)(uint32_t slot, struct RhhWorldEffectSprite *out);
    int (*effect_part)(uint32_t slot, uint32_t part, struct RhhWorldSpritePart *out);
    int (*effect_part_rgba)(uint32_t slot, uint32_t part, uint8_t *rgba, uint32_t capacity);
    /* Version 1.3 additionally classifies tracks, ripples and splashes.
     * effect_at retains its version 1.2 classification for older consumers. */
    int (*effect_at_v1_3)(uint32_t slot, struct RhhWorldEffectSprite *out);
    /* Version 1.4: graphics shadow size, engine suppression reasons and
     * optional live field-effect association. Does not create a shadow. */
    int (*actor_shadow)(uint32_t slot, struct RhhWorldActorShadow *out);
    /* Version 1.5: one read-only lighting input snapshot; no habitat hooks
     * are dispatched and the time/environment may be unavailable. */
    int (*lighting_inputs)(struct RhhWorldLightingInputs *out);
    /* Version 1.6: addressable live engine lamp/sign sprites. Invisible
     * sprites remain queryable and do not imply active illumination. */
    uint32_t (*local_light_slots)(void);
    int (*local_light_at)(uint32_t slot, struct RhhWorldLocalLight *out);
    /* Version 1.7: copied individual visibility inputs. */
    int (*actor_visibility)(uint32_t slot, struct RhhWorldActorVisibility *out);
    /* Version 1.8: per-part affine geometry and rasterization for actors and
     * recognized field effects. Raster output is tightly packed RGBA8888 at
     * draw_width*draw_height; affine sampling and clipping are applied, but
     * mosaic, blending and window composition are not. Disabled parts produce
     * transparent pixels. Failures leave outputs untouched. */
    int (*sprite_part_transform)(uint32_t slot, uint32_t part, struct RhhWorldSpriteTransform *out);
    int (*sprite_part_transformed_rgba)(uint32_t slot, uint32_t part, uint8_t *rgba, uint32_t capacity);
    int (*effect_part_transform)(uint32_t slot, uint32_t part, struct RhhWorldSpriteTransform *out);
    int (*effect_part_transformed_rgba)(uint32_t slot, uint32_t part, uint8_t *rgba, uint32_t capacity);
    /* Version 1.9: sample the display controls used by the native renderer. */
    int (*display_state)(struct RhhWorldDisplayState *out);
    /* Version 1.10: transformed RGBA with optional screen-anchored mosaic.
     * Failures leave the destination unchanged. */
    int (*sprite_part_raster_rgba)(uint32_t slot, uint32_t part, const struct RhhWorldSpriteRasterOptions *,
                                   uint8_t *rgba, uint32_t capacity);
    int (*effect_part_raster_rgba)(uint32_t slot, uint32_t part, const struct RhhWorldSpriteRasterOptions *,
                                   uint8_t *rgba, uint32_t capacity);
    /* Version 1.11: all scanline controls from the latest completed software
     * frame on the current map. Unavailable or invalid outputs are untouched. */
    int (*display_frame)(struct RhhWorldDisplayFrame *out);
};
#define RHH_OVERWORLD_SERVICE_V1_0_SIZE offsetof(struct RhhOverworldService, camera)
#define RHH_OVERWORLD_SERVICE_V1_1_SIZE offsetof(struct RhhOverworldService, effect_slots)
#define RHH_OVERWORLD_SERVICE_V1_2_SIZE offsetof(struct RhhOverworldService, effect_at_v1_3)
#define RHH_OVERWORLD_SERVICE_V1_3_SIZE offsetof(struct RhhOverworldService, actor_shadow)
#define RHH_OVERWORLD_SERVICE_V1_4_SIZE offsetof(struct RhhOverworldService, lighting_inputs)
#define RHH_OVERWORLD_SERVICE_V1_5_SIZE offsetof(struct RhhOverworldService, local_light_slots)
#define RHH_OVERWORLD_SERVICE_V1_6_SIZE offsetof(struct RhhOverworldService, actor_visibility)
#define RHH_OVERWORLD_SERVICE_V1_7_SIZE offsetof(struct RhhOverworldService, sprite_part_transform)
#define RHH_OVERWORLD_SERVICE_V1_8_SIZE offsetof(struct RhhOverworldService, display_state)
#define RHH_OVERWORLD_SERVICE_V1_9_SIZE offsetof(struct RhhOverworldService, sprite_part_raster_rgba)
#define RHH_OVERWORLD_SERVICE_V1_10_SIZE offsetof(struct RhhOverworldService, display_frame)
#define RHH_OVERWORLD_SERVICE_V1_11_SIZE sizeof(struct RhhOverworldService)
/* Scene rules annotate existing map tiles; they never change collision or
 * gameplay. Heights are signed pixels in a world where a metatile is 16 px;
 * positive height is upward. Unannotated tiles are flat, solid, shadow-receiving
 * ground with a flat upper layer. Matching uses current-engine metatile and
 * behavior ordinals, which must not be persisted as independent identities. */
enum RhhSceneMatch {
    RHH_SCENE_MATCH_MAP = 1u, RHH_SCENE_MATCH_METATILE = 2u,
    RHH_SCENE_MATCH_BEHAVIOR = 4u, RHH_SCENE_MATCH_POSITION = 8u
};
enum RhhSceneSurface { RHH_SCENE_SOLID, RHH_SCENE_WATER };
enum RhhSceneUpper {
    RHH_SCENE_UPPER_FLAT, RHH_SCENE_UPPER_BILLBOARD,
    RHH_SCENE_UPPER_WALL_NORTH, RHH_SCENE_UPPER_WALL_SOUTH,
    RHH_SCENE_UPPER_WALL_WEST, RHH_SCENE_UPPER_WALL_EAST,
    RHH_SCENE_UPPER_ROOF, RHH_SCENE_UPPER_HIDDEN
};
enum RhhSceneFlags {
    RHH_SCENE_RECEIVES_SHADOW = 1u, RHH_SCENE_CASTS_SHADOW = 2u,
    RHH_SCENE_OCCLUDES = 4u
};
struct RhhSceneStyle {
    uint32_t surface, upper, flags;
    int32_t ground_height, upper_height;
};
/* IDs and declarations are borrowed and must remain unchanged until module
 * unload. The host selects
 * the highest-priority matching rule, then the most specific match, then the
 * lexicographically smallest ID. POSITION requires MAP. All unselected match
 * fields and reserved words must be zero. MAP aliases resolve canonically. */
struct RhhModSceneRule {
    uint32_t struct_size, match, reserved[2];
    const char *id;
    int32_t priority, x, y;
    uint32_t metatile, behavior;
    RhhContentKey map;
    struct RhhSceneStyle style;
};
#define RHH_SCENE_LIGHTS_PER_TILE 8u
/* Pixels are local to map; height is above that tile's annotated ground.
 * height is 0..256, radius is 1..256 pixels, rgb is nonzero 0xRRGGBB and intensity is 1..1000. A zero period mask means all
 * periods; otherwise bit (RHH_TIME_* - 1) selects an effective time period.
 * IDs and declarations are borrowed unchanged until module unload. */
struct RhhModSceneLight {
    uint32_t struct_size, period_mask;
    const char *id;
    RhhContentKey map;
    int32_t x, y, height;
    uint32_t radius, rgb, intensity, reserved[2];
};
struct RhhSceneLight {
    RhhContentKey id, source_map;
    int32_t x, y, z; /* current-map pixels; z is absolute scene height */
    uint32_t radius, rgb, intensity;
};
struct RhhSceneLightBatch {
    uint32_t struct_size, count;
    struct RhhSceneLight lights[RHH_SCENE_LIGHTS_PER_TILE];
};
enum RhhSceneTraceFlags { RHH_SCENE_TRACE_ALPHA = 1u };
enum RhhSceneTraceStatus { RHH_SCENE_TRACE_CLEAR = 0, RHH_SCENE_TRACE_BLOCKED };
/* Integer map-pixel endpoints, +z up. X/Y separation is bounded to 512 px.
 * Alpha testing uses the current decoded 8x8 tile; without it, cast-shadow
 * quads are treated as opaque. Segment endpoints do not self-occlude. */
struct RhhSceneTraceRequest {
    uint32_t struct_size, flags;
    int32_t from[3], to[3];
};
struct RhhSceneTraceResult {
    uint32_t struct_size, status;
    RhhContentKey map;
    int32_t tile_x, tile_y;
    uint32_t quad_index, quad_kind, tile, fraction_millionths;
};
struct RhhSceneTile {
    uint32_t struct_size, reserved;
    RhhContentKey map, rule; /* rule is zero for the default */
    int32_t x, y;
    struct RhhWorldTile tile;
    struct RhhSceneStyle style;
};
#define RHH_SCENE_MESH_MAX_QUADS 16u
enum RhhSceneQuadKind { RHH_SCENE_QUAD_GROUND = 1, RHH_SCENE_QUAD_UPPER,
    RHH_SCENE_QUAD_BILLBOARD, RHH_SCENE_QUAD_GROUND_EDGE };
/* Coordinates are map pixels, +x east, +y south, +z up. Vertices correspond
 * to the top-left, top-right, bottom-right and bottom-left of tile_rgba's
 * decoded 8x8 image. Normals are explicit; quads are double-sided and winding
 * is not guaranteed. A billboard is drawn in a +x/+z plane at the tile center;
 * rotate it around that tile center to face the camera. Ground edges are
 * inferred between adjacent ground heights and stretch an edge tile. */
struct RhhSceneQuad {
    uint32_t kind, flags;
    uint16_t tile, reserved;
    float vertex[4][3], normal[3];
};
struct RhhSceneMesh {
    uint32_t struct_size, count;
    struct RhhSceneTile scene;
    struct RhhSceneQuad quads[RHH_SCENE_MESH_MAX_QUADS];
};
/* Loaded render-grid bounds in current-map tile coordinates, inclusive. The
 * window includes border and neighboring cells already loaded by the engine;
 * it is not a gameplay map or a source-map identity. */
struct RhhSceneWindow {
    uint32_t struct_size, reserved;
    RhhContentKey map;
    int32_t min_x, min_y, max_x, max_y;
};
enum RhhSceneWindowOrigin { RHH_SCENE_WINDOW_CURRENT = 1, RHH_SCENE_WINDOW_CONNECTED,
    RHH_SCENE_WINDOW_BORDER, RHH_SCENE_WINDOW_UNKNOWN };
/* Source coordinates are meaningful only for CURRENT and CONNECTED. Scene
 * coordinates always refer to the current map's loaded render grid. */
struct RhhSceneWindowTile {
    uint32_t struct_size, origin;
    RhhContentKey source_map;
    int32_t source_x, source_y;
    struct RhhSceneTile scene;
};
enum RhhSceneInferenceFlags { RHH_SCENE_INFERRED_WATER = 1u, RHH_SCENE_INFERRED_SIGN = 2u,
    RHH_SCENE_INFERENCE_SAMPLED = 4u };
/* Conservative inferred style for the loaded scene. Counts describe opaque
 * pixels across each four-tile layer when SAMPLED is set. Explicit rules win;
 * unrecognized or ambiguous artwork stays flat. */
struct RhhSceneInference {
    uint32_t struct_size, flags, ground_pixels, upper_pixels;
    struct RhhSceneWindowTile tile;
};
/* Intersect one caller-positioned, double-sided sprite parallelogram. Corners
 * are top-left, top-right, bottom-right, bottom-left in map-pixel space, +z
 * up; the presenter supplies the actual orientation and animation pixels.
 * Width and height are 1..128 on world-scene v1.11 (1..64 on older hosts).
 * With RHH_SCENE_TRACE_ALPHA, rgba is RGBA8888
 * with a byte row stride and capacity; alpha >= 128 casts a shadow. Without
 * the flag rgba, stride and capacity must be zero. Stride is at most 4096
 * bytes; coordinates must be finite and within +/-1048576 pixels. Hits at or
 * within 0.01% of either segment endpoint are ignored. */
struct RhhSceneBillboardTraceRequest {
    uint32_t struct_size, flags, width, height, stride, rgba_size, reserved[2];
    float from[3], to[3], corners[4][3];
    const uint8_t *rgba;
};
#define RHH_SCENE_BILLBOARD_MAX_DIMENSION 128u
/* CLEAR leaves pixel coordinates UINT32_MAX and fraction 1000000. BLOCKED
 * reports the nearest intersection, source-image pixel and clamped UV. */
struct RhhSceneBillboardTraceResult {
    uint32_t struct_size, status, fraction_millionths, u_millionths, v_millionths, pixel_x, pixel_y, reserved;
};
enum RhhSceneDirectionalFlags { RHH_SCENE_DIRECTIONAL_INFERRED = 2u };
enum RhhSceneDirectionalCoverage { RHH_SCENE_DIRECTIONAL_WINDOW_CLIPPED = 1u, RHH_SCENE_DIRECTIONAL_HEIGHT_CLIPPED = 2u };
/* Trace from a receiver toward a directional light. direction need not be
 * normalized: its finite components are bounded to +/-1048576 and its length
 * must be at least 1e-6. from must be in the loaded window and +/-4096 height;
 * max_distance is >0 and <=512 pixels. RHH_SCENE_TRACE_ALPHA and
 * RHH_SCENE_DIRECTIONAL_INFERRED opt into alpha and inferred meshes. */
struct RhhSceneDirectionalTraceRequest {
    uint32_t struct_size, flags, reserved[2];
    float from[3], direction[3], max_distance;
};
/* Distance and end describe the portion actually checked. Coverage flags
 * indicate clipping at the loaded window or +/-4096 height; a clear result
 * says nothing about occluders beyond that checked segment. The nested hit's
 * fraction is relative to the returned segment, and end can be reused with
 * trace_billboard. */
struct RhhSceneDirectionalTraceResult {
    uint32_t struct_size, coverage;
    float distance, end[3];
    struct RhhSceneTraceResult hit;
};
#define RHH_SCENE_SHADOW_CASTER_LIMIT 64u
/* Caller-positioned sprite part for a combined directional trace. IDs are
 * nonzero and unique within this request, and need not be persistent. Flags,
 * corners and pixel buffers obey the trace_billboard contract. */
struct RhhSceneShadowCaster {
    uint32_t struct_size, flags, width, height, stride, rgba_size, reserved[2];
    uint64_t id;
    float corners[4][3];
    const uint8_t *rgba;
};
struct RhhSceneShadowTraceRequest {
    uint32_t struct_size, caster_count, reserved[2];
    struct RhhSceneDirectionalTraceRequest ray;
    const struct RhhSceneShadowCaster *casters;
};
enum RhhSceneShadowHitKind { RHH_SCENE_SHADOW_CLEAR, RHH_SCENE_SHADOW_MESH, RHH_SCENE_SHADOW_BILLBOARD };
/* Fractions are millionths of the checked segment. At equal rounded fractions
 * a mesh wins; ties between billboards use the lowest ID. CLEAR or MESH has
 * caster_index UINT32_MAX and caster_id zero. The nested ray retains the mesh
 * result and coverage even when a billboard is nearer. */
struct RhhSceneShadowTraceResult {
    uint32_t struct_size, kind, caster_index, fraction_millionths;
    uint64_t caster_id;
    struct RhhSceneDirectionalTraceResult ray;
    struct RhhSceneBillboardTraceResult billboard;
};
/* "rhh:service/world_scene", version 1.11. Current-map queries fail before
 * host finalization or outside an available overworld. Results are copied;
 * no function dispatches hooks or advances random state. */
struct RhhWorldSceneService {
    struct RhhServiceHeader header;
    int (*tile_at)(int32_t x, int32_t y, struct RhhSceneTile *out);
    /* mesh_at resolves the actual tile/rule and adjacent height edges.
     * build_mesh converts a caller-supplied scene without adjacent edges. Both
     * fail without changing out for invalid inputs or insufficient struct size. */
    int (*mesh_at)(int32_t x, int32_t y, struct RhhSceneMesh *out);
    int (*build_mesh)(const struct RhhSceneTile *scene, struct RhhSceneMesh *out);
    /* Loaded render-grid views, including the engine's border fallback.
     * Outside the current map, scene rules are not inferred or attributed to
     * a neighboring map. Output coordinates remain current-map coordinates. */
    int (*window)(struct RhhSceneWindow *out);
    int (*window_tile_at)(int32_t x, int32_t y, struct RhhSceneTile *out);
    int (*window_mesh_at)(int32_t x, int32_t y, struct RhhSceneMesh *out);
    /* Conservative connected-map attribution; UNKNOWN and BORDER retain
     * default styling. The resolved mesh also uses resolved neighbor heights. */
    int (*window_resolve)(int32_t x, int32_t y, struct RhhSceneWindowTile *out);
    int (*window_resolved_mesh_at)(int32_t x, int32_t y, struct RhhSceneMesh *out);
    /* Version 1.4: resolve declared lights belonging to this loaded tile.
     * period is RHH_TIME_* or zero if time is unavailable. Results are
     * ordered by stable declaration ID and use current-map coordinates. */
    int (*lights_at)(int32_t x, int32_t y, uint32_t period, struct RhhSceneLightBatch *out);
    uint32_t (*light_count)(void); /* all declarations, not just this map */
    /* Version 1.5: nearest cast-shadow mesh intersection in the loaded grid.
     * Invalid, unavailable or out-of-window requests leave output unchanged. */
    int (*trace_light)(const struct RhhSceneTraceRequest *, struct RhhSceneTraceResult *out);
    uint32_t (*shadow_caster_count)(void); /* declarative rules that can cast shadows */
    /* v1.6: opt-in material/geometry inference for loaded, attributable tiles.
     * The mesh resolves inferred neighboring heights and preserves rule priority.
     * trace_light remains the declared-rule view. */
    int (*window_infer)(int32_t x, int32_t y, struct RhhSceneInference *out);
    int (*window_inferred_mesh_at)(int32_t x, int32_t y, struct RhhSceneMesh *out);
    /* v1.7: trace cast-shadow quads from the inferred mesh view. The original
     * trace_light keeps its declared-rule behavior. Same bounds and flags. */
    int (*trace_inferred_light)(const struct RhhSceneTraceRequest *, struct RhhSceneTraceResult *out);
    /* v1.8: geometry-only billboard test; does not query the active scene,
     * infer visibility or create shadows. Invalid input leaves out unchanged. */
    int (*trace_billboard)(const struct RhhSceneBillboardTraceRequest *, struct RhhSceneBillboardTraceResult *out);
    /* v1.9: bounded directional light trace with explicit coverage. */
    int (*trace_directional_light)(const struct RhhSceneDirectionalTraceRequest *, struct RhhSceneDirectionalTraceResult *out);
    /* v1.10: nearest mesh or caller-supplied billboard along the same bounded
     * directional segment. All casters are validated even after a hit. */
    int (*trace_directional_shadow)(const struct RhhSceneShadowTraceRequest *, struct RhhSceneShadowTraceResult *out);
    /* v1.11: trace_billboard and trace_directional_shadow accept source
     * dimensions up to RHH_SCENE_BILLBOARD_MAX_DIMENSION. */
};
#define RHH_WORLD_SCENE_SERVICE_V1_0_SIZE offsetof(struct RhhWorldSceneService, mesh_at)
#define RHH_WORLD_SCENE_SERVICE_V1_1_SIZE offsetof(struct RhhWorldSceneService, window)
#define RHH_WORLD_SCENE_SERVICE_V1_2_SIZE offsetof(struct RhhWorldSceneService, window_resolve)
#define RHH_WORLD_SCENE_SERVICE_V1_3_SIZE offsetof(struct RhhWorldSceneService, lights_at)
#define RHH_WORLD_SCENE_SERVICE_V1_4_SIZE offsetof(struct RhhWorldSceneService, trace_light)
#define RHH_WORLD_SCENE_SERVICE_V1_5_SIZE offsetof(struct RhhWorldSceneService, window_infer)
#define RHH_WORLD_SCENE_SERVICE_V1_6_SIZE offsetof(struct RhhWorldSceneService, trace_inferred_light)
#define RHH_WORLD_SCENE_SERVICE_V1_7_SIZE offsetof(struct RhhWorldSceneService, trace_billboard)
#define RHH_WORLD_SCENE_SERVICE_V1_8_SIZE offsetof(struct RhhWorldSceneService, trace_directional_light)
#define RHH_WORLD_SCENE_SERVICE_V1_9_SIZE offsetof(struct RhhWorldSceneService, trace_directional_shadow)
#define RHH_WORLD_SCENE_SERVICE_V1_10_SIZE sizeof(struct RhhWorldSceneService)
#define RHH_WORLD_SCENE_SERVICE_V1_11_SIZE RHH_WORLD_SCENE_SERVICE_V1_10_SIZE
enum RhhServiceRequirementFlags { RHH_SERVICE_REQUIRED = 1u };
/* Declarations and IDs are borrowed until module unload. A missing optional
 * service does not prevent activation; query it through get_service. */
struct RhhServiceRequirement {
    uint32_t struct_size, minimum_version, flags, reserved;
    const char *id;
};
/* Paths are relative to the host's music directory and use '/' separators.
 * These declarations and strings are borrowed until module unload.
 * BATTLE_MUSIC and MUSIC_SELECT restore both gameplay RNG streams; if a hook
 * consumes them, that event's selection changes are discarded and any denial
 * remains effective. */
struct RhhModMusic {
    uint32_t struct_size, reserved;
    const char *id, *label, *midi_path, *soundfont_path;
};
enum RhhMusicAvailability { RHH_MUSIC_READY, RHH_MUSIC_MISSING, RHH_MUSIC_INVALID };
enum RhhMusicContext { RHH_MUSIC_OTHER, RHH_MUSIC_WILD, RHH_MUSIC_TRAINER,
    RHH_MUSIC_GYM, RHH_MUSIC_CHAMPION };
#define RHH_MUSIC_GAIN_ONE 1000u
#define RHH_MUSIC_GAIN_MAX 4000u
enum RhhMusicBackend { RHH_MUSIC_BACKEND_MIDI = 1, RHH_MUSIC_BACKEND_GAMEBOY = 2,
    RHH_MUSIC_BACKEND_MP2K = 3 };
enum RhhMusicPlaybackFlags { RHH_MUSIC_PLAYBACK_PAUSED = 1u, RHH_MUSIC_PLAYBACK_FADING_IN = 2u,
    RHH_MUSIC_PLAYBACK_TRANSITION_TAIL = 4u, RHH_MUSIC_PLAYBACK_EXPLICIT_LOOP = 8u,
    RHH_MUSIC_PLAYBACK_LEVEL_RAMP = 16u };
/* A post-load, pre-playback proposal for external MIDI/SF2 or Game Boy music.
 * Gain is in thousandths of the host's track-specific normalization, before
 * saved per-track level and the game's live volume and fade. Denial, RNG use,
 * or a value above MAX restores ONE for this proposal. The event key is track;
 * subject is original. */
struct RhhMusicGainContext {
    uint32_t struct_size, backend, context, asset;
    RhhContentKey track, original, map, trainers[2];
};
/* Gain is the current target; LEVEL_RAMP indicates an in-progress level
 * adjustment. Mixer position counts rendered source samples, not audible hardware time.
 * Game Boy loops are engine-managed, so loop offsets are zero. MIDI loop points
 * may include the automatic fallback loop when EXPLICIT_LOOP is absent. */
struct RhhMusicPlayback {
    uint32_t struct_size, backend, context, flags, gain, sample_rate, asset, reserved;
    RhhContentKey track, original, map, trainers[2];
    uint64_t position, loop_start, loop_end;
};
/* Committed BGM lifecycle. Events are delivered in order on a later native
 * frame, outside the audio mixer. Sequences are process-local; gaps and the
 * cumulative dropped count indicate overflow of the bounded event queue.
 * The original MP2K song uses its original content key as track; a redirected
 * MP2K song uses the selected replacement key. requested records the proposed
 * replacement even when it could not be loaded. Observer delivery restores
 * both gameplay RNG streams. Preview playback is separate. */
enum RhhMusicLifecycleCause { RHH_MUSIC_STARTED = 1, RHH_MUSIC_STOPPED };
enum RhhMusicLifecycleFlags { RHH_MUSIC_FALLBACK = 1u };
struct RhhMusicLifecycle {
    uint32_t struct_size, cause, backend, context, flags, asset, reserved[2];
    RhhContentKey track, requested, original, map, trainers[2];
    uint64_t sequence;
};
struct RhhMusicLifecycleState {
    uint32_t struct_size, active, reserved[2];
    uint64_t latest_sequence, dropped;
    struct RhhMusicLifecycle current;
};
enum RhhMusicLoopPhase { RHH_MUSIC_LOOP_PLAYBACK = 1, RHH_MUSIC_LOOP_PREVIEW };
enum RhhMusicLoopFlags { RHH_MUSIC_LOOP_EMBEDDED = 1u };
/* MIDI-only pure proposal. Pack start milliseconds into the low 32 bits of
 * decision.value and end milliseconds into the high 32 bits. The initial
 * value is the validated embedded loop or the automatic whole-song loop.
 * A changed interval must have start < end <= song_end_ms + 5000. Denial,
 * invalid bounds or gameplay RNG use retains the original interval. Preview
 * has its owner and track but no battle/map context. Game Boy and MP2K loops
 * are managed by their own engines and do not receive this proposal. */
#define RHH_MUSIC_LOOP_VALUE(start, end) ((uint64_t)(uint32_t)(start) | ((uint64_t)(uint32_t)(end) << 32))
struct RhhMusicLoopContext {
    uint32_t struct_size, phase, context, flags, asset, song_end_ms, start_ms, end_ms;
    RhhContentKey track, original, map, trainers[2], owner;
};
enum RhhMusicTrackKind { RHH_MUSIC_TRACK_UNKNOWN = 0, RHH_MUSIC_TRACK_BUNDLED,
    RHH_MUSIC_TRACK_DECLARED, RHH_MUSIC_TRACK_IMPORTED };
/* A point-in-time asset inspection, not a playback reservation. Unknown keys
 * report MISSING and UNKNOWN; a removed import cannot be identified as an
 * import from its key alone. asset is the bundled style ID or zero. */
struct RhhMusicTrackInfo {
    uint32_t struct_size, availability, kind, backend, asset, reserved[3];
    RhhContentKey track;
};
enum RhhMusicSourceTimingFlags { RHH_MUSIC_SOURCE_MIDI_TIMING = 1u,
    RHH_MUSIC_SOURCE_EMBEDDED_LOOP = 2u, RHH_MUSIC_SOURCE_ENGINE_LOOP = 4u };
/* Source metadata, not a playback prediction. last_event_ms is the final
 * parsed MIDI event, not audible duration. loop_start_ms and loop_end_ms are
 * the validated embedded markers or the default 0..last_event_ms loop before
 * any mod loop proposals. Game Boy and MP2K use engine-managed loops and do
 * not report MIDI timing. Missing or invalid assets have no timing flags. */
struct RhhMusicSourceTiming {
    uint32_t struct_size, availability, kind, backend, asset, flags;
    uint32_t last_event_ms, loop_start_ms, loop_end_ms, reserved;
    RhhContentKey track;
};
#define RHH_MUSIC_INSTRUMENT_LIMIT 128u
enum RhhMusicInstrumentFlags { RHH_MUSIC_INSTRUMENT_FALLBACK = 1u };
enum RhhMusicInstrumentInspectionFlags { RHH_MUSIC_INSTRUMENT_MIDI = 1u, RHH_MUSIC_INSTRUMENT_TRUNCATED = 2u };
/* One distinct channel/requested preset/selected preset combination, in
 * first-note order. Names are copied SoundFont bytes, not a stable key. */
struct RhhMusicInstrumentUse {
    uint32_t channel, requested_bank, requested_program, selected_bank, selected_program, flags;
    uint32_t notes, first_note_ms;
    char preset_name[21];
};
/* Source-note inspection for one linear traversal of the MIDI. Counts include
 * notes whose combinations exceed the bounded list; unlisted_notes counts
 * those notes, not distinct combinations. Backend and availability follow
 * inspect_track; Game Boy and MP2K have no MIDI instrument list. */
struct RhhMusicInstrumentInspection {
    uint32_t struct_size, availability, kind, backend, asset, flags, count, total_notes;
    uint32_t fallback_notes, unlisted_notes, reserved[2];
    RhhContentKey track;
    struct RhhMusicInstrumentUse uses[RHH_MUSIC_INSTRUMENT_LIMIT];
};
#define RHH_MUSIC_NOTE_GAP_LIMIT 64u
enum RhhMusicNoteCoverageFlags { RHH_MUSIC_COVERAGE_MIDI = 1u, RHH_MUSIC_COVERAGE_TRUNCATED = 2u };
/* An unmatched key/velocity and selected preset, grouped in first-note order. */
struct RhhMusicNoteGap {
    uint32_t channel, key, velocity, requested_bank, requested_program, selected_bank, selected_program;
    uint32_t notes, first_note_ms;
};
/* A MIDI note is covered when at least one selected preset region matches its
 * key and velocity. This does not guarantee an audible or available voice. */
struct RhhMusicNoteCoverage {
    uint32_t struct_size, availability, kind, backend, asset, flags, count, total_notes;
    uint32_t covered_notes, uncovered_notes, unlisted_uncovered_notes, reserved;
    RhhContentKey track;
    struct RhhMusicNoteGap gaps[RHH_MUSIC_NOTE_GAP_LIMIT];
};
enum RhhMusicCueRequestStatus { RHH_MUSIC_CUE_QUEUED, RHH_MUSIC_CUE_INVALID,
    RHH_MUSIC_CUE_UNAVAILABLE, RHH_MUSIC_CUE_BUSY };
enum RhhMusicCueOutcome { RHH_MUSIC_CUE_PLAYED = 1, RHH_MUSIC_CUE_OVERRIDDEN,
    RHH_MUSIC_CUE_FALLBACK, RHH_MUSIC_CUE_STALE };
/* A zero track requests the original engine song. A nonzero track must be a
 * bundled style, declared pair or current imported track. The first valid
 * request waiting for a native frame wins; later requests return BUSY. */
struct RhhMusicBattleCueRequest {
    uint32_t struct_size, reserved;
    RhhContentKey requester, track;
};
/* Delivered once on a native frame after a queued request is attempted or
 * found stale. actual is zero if no resulting lifecycle is available.
 * event_key is requester, subject is requested track; args[0] is outcome. */
struct RhhMusicBattleCueResult {
    uint32_t struct_size, outcome;
    RhhContentKey requester, requested, actual, original;
    uint64_t run_id, battle_id, before_sequence, after_sequence;
};
/* "rhh:service/music", version 1.8. Tracks are indexed in registration order;
 * availability parses assets and may be expensive. Key and returned strings
 * remain valid until shutdown. */
struct RhhMusicService {
    struct RhhServiceHeader header;
    uint32_t (*count)(void);
    int (*at)(uint32_t index, RhhContentKey *key, const struct RhhModMusic **track);
    int (*find)(RhhContentKey key, const struct RhhModMusic **track);
    int (*availability)(RhhContentKey key);
    RhhContentKey (*current)(void);
    /* v1.1: proposal context during RHH_MUSIC_GAIN; snapshot while a
     * replacement is active. Both fail without changing out when unavailable. */
    int (*gain_context)(struct RhhMusicGainContext *out);
    int (*playback)(struct RhhMusicPlayback *out);
    /* v1.2: current lifecycle state and the current committed event context.
     * Failure leaves the output untouched. The state is process-local. */
    int (*lifecycle)(struct RhhMusicLifecycleState *out);
    int (*lifecycle_context)(struct RhhMusicLifecycle *out);
    /* v1.3: available only during RHH_MUSIC_LOOP. */
    int (*loop_context)(struct RhhMusicLoopContext *out);
    /* v1.4: inspect bundled styles, declared pairs and imports without
     * changing playback. Parsing can perform substantial file I/O; do not
     * call from rendering or audio callbacks. */
    int (*inspect_track)(RhhContentKey key, struct RhhMusicTrackInfo *out);
    /* v1.5: inspect source timing without playback or loop proposals. Like
     * inspect_track, this can parse large assets and perform file I/O. */
    int (*inspect_source_timing)(RhhContentKey key, struct RhhMusicSourceTiming *out);
    /* v1.6: request from supported committed battle observers, then inspect
     * the deferred RHH_MUSIC_BATTLE_CUE_FINISHED result. */
    int (*queue_battle_cue)(const struct RhhMusicBattleCueRequest *request);
    int (*battle_cue_result)(struct RhhMusicBattleCueResult *out);
    /* v1.7: inspect MIDI notes and the selected SoundFont presets without
     * playing audio. Like inspect_track, this can perform substantial I/O. */
    int (*inspect_instruments)(RhhContentKey key, struct RhhMusicInstrumentInspection *out);
    /* v1.8: inspect selected preset regions for source-note key/velocity
     * coverage. Uses separate decoders and may perform substantial I/O. */
    int (*inspect_note_coverage)(RhhContentKey key, struct RhhMusicNoteCoverage *out);
};
#define RHH_MUSIC_SERVICE_V1_0_SIZE offsetof(struct RhhMusicService, gain_context)
#define RHH_MUSIC_SERVICE_V1_1_SIZE offsetof(struct RhhMusicService, lifecycle)
#define RHH_MUSIC_SERVICE_V1_2_SIZE offsetof(struct RhhMusicService, loop_context)
#define RHH_MUSIC_SERVICE_V1_3_SIZE offsetof(struct RhhMusicService, inspect_track)
#define RHH_MUSIC_SERVICE_V1_4_SIZE offsetof(struct RhhMusicService, inspect_source_timing)
#define RHH_MUSIC_SERVICE_V1_5_SIZE offsetof(struct RhhMusicService, queue_battle_cue)
#define RHH_MUSIC_SERVICE_V1_6_SIZE offsetof(struct RhhMusicService, inspect_instruments)
#define RHH_MUSIC_SERVICE_V1_7_SIZE offsetof(struct RhhMusicService, inspect_note_coverage)
#define RHH_MUSIC_SERVICE_V1_8_SIZE sizeof(struct RhhMusicService)
enum RhhMusicPreviewStatus { RHH_MUSIC_PREVIEW_OK, RHH_MUSIC_PREVIEW_INVALID,
    RHH_MUSIC_PREVIEW_UNAVAILABLE, RHH_MUSIC_PREVIEW_BUSY,
    RHH_MUSIC_PREVIEW_MISSING, RHH_MUSIC_PREVIEW_AUDIO_ERROR };
struct RhhMusicPreviewRequest {
    uint32_t struct_size, duration_ms, volume_percent, reserved;
    RhhContentKey owner, track;
};
struct RhhMusicPreviewSnapshot {
    uint32_t struct_size, active, duration_ms, elapsed_ms;
    RhhContentKey owner, track;
};
struct RhhMusicScaledPreviewRequest {
    uint32_t struct_size, duration_ms, gain, reserved;
    RhhContentKey owner, track;
};
/* Mixer position is relative to the current loop, and is not audio device
 * time. These sample counts follow the same rule as the playback snapshot. */
struct RhhMusicPreviewTiming {
    uint32_t struct_size, sample_rate, flags, reserved;
    uint64_t position, loop_start, loop_end;
};
/* "rhh:service/music_preview" v1.2. A registered module can preview an
 * imported or declared MIDI/SF2 pair while it owns a modal panel. Duration
 * is 500..30000 ms, volume is 0..100. Closing the panel, a run reset, or a
 * new game BGM automatically ends the preview. */
struct RhhMusicPreviewService {
    struct RhhServiceHeader header;
    int (*start)(const struct RhhMusicPreviewRequest *);
    int (*stop)(RhhContentKey owner);
    int (*current)(struct RhhMusicPreviewSnapshot *out);
    /* v1.1: preview at 0..RHH_MUSIC_GAIN_MAX thousandths. */
    int (*start_scaled)(const struct RhhMusicScaledPreviewRequest *request);
    /* v1.2: available while the preview is active. */
    int (*timing)(struct RhhMusicPreviewTiming *out);
};
#define RHH_MUSIC_PREVIEW_SERVICE_V1_0_SIZE offsetof(struct RhhMusicPreviewService, start_scaled)
#define RHH_MUSIC_PREVIEW_SERVICE_V1_1_SIZE offsetof(struct RhhMusicPreviewService, timing)
#define RHH_MUSIC_PREVIEW_SERVICE_V1_2_SIZE sizeof(struct RhhMusicPreviewService)
/* Host-owned library of imported MIDI/SF2 pairs, independent of game saves.
 * Paths passed to import_pair are UTF-8 and used only for that operation.
 * Hashes identify copied contents; labels are printable ASCII. */
#define RHH_MUSIC_IMPORT_LIMIT 32u
#define RHH_MUSIC_IMPORT_LABEL_SIZE 64u
enum RhhMusicImportStatus { RHH_MUSIC_IMPORT_OK, RHH_MUSIC_IMPORT_INVALID,
    RHH_MUSIC_IMPORT_UNAVAILABLE, RHH_MUSIC_IMPORT_MISSING, RHH_MUSIC_IMPORT_IO,
    RHH_MUSIC_IMPORT_FULL, RHH_MUSIC_IMPORT_CONFLICT };
struct RhhImportedMusic {
    uint32_t struct_size, midi_size, soundfont_size, has_loop;
    uint32_t loop_start_ms, loop_end_ms;
    RhhContentKey key;
    uint8_t midi_sha256[32], soundfont_sha256[32];
    char label[RHH_MUSIC_IMPORT_LABEL_SIZE];
};
struct RhhMusicImportRequest {
    uint32_t struct_size, reserved;
    RhhContentKey requester;
    const char *midi_path, *soundfont_path, *label;
};
struct RhhMusicImportResult {
    uint32_t struct_size, status, changed, reserved;
    RhhContentKey requester;
    struct RhhImportedMusic track;
};
/* A portable pair must match this full metadata snapshot, including both
 * content hashes. The host validates formats before adding it to a library. */
struct RhhMusicMemoryImportRequest {
    uint32_t struct_size, midi_size, soundfont_size, reserved;
    RhhContentKey requester;
    const struct RhhImportedMusic *expected;
    const void *midi, *soundfont;
};
/* "rhh:service/music_imports" v1.1. import_pair and remove are synchronous
 * and may perform substantial file I/O; do not call them from rendering or
 * audio callbacks. Context is available in the two committed notifications. */
struct RhhMusicImportService {
    struct RhhServiceHeader header;
    int (*status)(void); /* OK, UNAVAILABLE (no library path), or IO (invalid/unreadable manifest) */
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhImportedMusic *out);
    int (*find)(RhhContentKey key, struct RhhImportedMusic *out);
    int (*import_pair)(const struct RhhMusicImportRequest *, struct RhhMusicImportResult *out);
    int (*remove)(RhhContentKey requester, RhhContentKey key, struct RhhMusicImportResult *out);
    int (*context)(struct RhhMusicImportResult *out);
    /* v1.1: copy both verified blobs using the exact metadata returned by
     * at/find; undersized, overlapping or stale outputs fail unchanged.
     * import_memory validates the metadata, hashes and formats before commit.
     * These synchronous operations may use memory proportional to both files. */
    int (*copy_pair)(const struct RhhImportedMusic *expected, void *midi, uint32_t midi_capacity,
                     void *soundfont, uint32_t soundfont_capacity);
    int (*import_memory)(const struct RhhMusicMemoryImportRequest *, struct RhhMusicImportResult *out);
};
#define RHH_MUSIC_IMPORT_SERVICE_V1_0_SIZE offsetof(struct RhhMusicImportService, copy_pair)
#define RHH_MUSIC_IMPORT_SERVICE_V1_1_SIZE sizeof(struct RhhMusicImportService)
/* Save-backed selection rules. Zero keys and RHH_MUSIC_ANY match any value.
 * Rules are unique by context/original/map/trainer. Most specific wins:
 * trainer, original song, map, then context. In a two-trainer battle, the
 * first trainer wins an otherwise identical tie. */
#define RHH_MUSIC_ANY UINT32_MAX
#define RHH_MUSIC_ASSIGNMENT_CAPACITY 64u
enum RhhMusicAssignmentAction { RHH_MUSIC_ASSIGN_SET = 1, RHH_MUSIC_ASSIGN_CLEAR };
enum RhhMusicAssignmentStatus { RHH_MUSIC_ASSIGN_OK, RHH_MUSIC_ASSIGN_INVALID,
    RHH_MUSIC_ASSIGN_UNAVAILABLE, RHH_MUSIC_ASSIGN_STALE, RHH_MUSIC_ASSIGN_FULL,
    RHH_MUSIC_ASSIGN_DENIED };
struct RhhMusicAssignment {
    uint32_t struct_size, context;
    RhhContentKey original, map, trainer, track;
};
struct RhhMusicSituation {
    uint32_t struct_size, context;
    RhhContentKey original, map, trainers[2];
};
struct RhhMusicAssignmentRequest {
    uint32_t struct_size, action;
    RhhContentKey requester;
    uint64_t expected_revision;
    struct RhhMusicAssignment rule;
};
struct RhhMusicAssignmentResult {
    uint32_t struct_size, status, changed, is_preview;
    uint64_t before_revision, after_revision;
    RhhContentKey requester;
    struct RhhMusicAssignment before, after;
    char reason[96];
};
/* "rhh:service/music_assignments" v1.0. Commands use a revision from
 * revision(), validate keys, and commit on the next normal game save.
 * Constraints restore both gameplay RNG streams. An allowed change that uses
 * RNG fails as STALE; a denial stays DENIED. Committed observers restore both
 * streams too. A resolved track may be missing at playback; the host then
 * falls back. */
struct RhhMusicAssignmentService {
    struct RhhServiceHeader header;
    uint64_t (*revision)(void);
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhMusicAssignment *out);
    int (*resolve)(const struct RhhMusicSituation *, struct RhhMusicAssignment *out);
    int (*preview)(const struct RhhMusicAssignmentRequest *, struct RhhMusicAssignmentResult *out);
    int (*apply)(const struct RhhMusicAssignmentRequest *, struct RhhMusicAssignmentResult *out);
    int (*context)(struct RhhMusicAssignmentResult *out);
};
/* Saved per-track mixer levels. Setting ONE removes an explicit entry;
 * missing imported or module assets retain their stored keys. */
#define RHH_MUSIC_LEVEL_CAPACITY 64u
enum RhhMusicLevelStatus { RHH_MUSIC_LEVEL_OK, RHH_MUSIC_LEVEL_INVALID,
    RHH_MUSIC_LEVEL_UNAVAILABLE, RHH_MUSIC_LEVEL_STALE, RHH_MUSIC_LEVEL_FULL, RHH_MUSIC_LEVEL_DENIED };
struct RhhMusicLevel { uint32_t struct_size, gain; RhhContentKey track; };
struct RhhMusicLevelRequest {
    uint32_t struct_size, gain;
    RhhContentKey requester, track;
    uint64_t expected_revision;
};
struct RhhMusicLevelResult {
    uint32_t struct_size, status, changed, is_preview, before_gain, after_gain;
    uint64_t before_revision, after_revision;
    RhhContentKey requester, track;
    char reason[96];
};
/* "rhh:service/music_levels" v1.0. Entries are sorted by key. get returns
 * ONE for any nonzero key without an entry; commands use checked revisions.
 * Constraints and committed observers restore both gameplay RNG streams;
 * allowed constraints that use RNG fail as STALE. Updates smoothly affect
 * the matching active replacement and are saved normally. */
struct RhhMusicLevelService {
    struct RhhServiceHeader header;
    uint64_t (*revision)(void);
    uint32_t (*count)(void);
    int (*at)(uint32_t index, struct RhhMusicLevel *out);
    int (*get)(RhhContentKey track, uint32_t *gain);
    int (*preview)(const struct RhhMusicLevelRequest *, struct RhhMusicLevelResult *out);
    int (*apply)(const struct RhhMusicLevelRequest *, struct RhhMusicLevelResult *out);
    int (*context)(struct RhhMusicLevelResult *out);
};
struct RhhModHook {
    uint32_t struct_size, event, stage;
    int32_t priority;
    RhhModHookFn reduce;
    RhhModObserveFn observe;
};
/* State is a portable byte layout, not a dumped C struct containing pointers.
 * migrate must initialize all destination bytes and return 0 on success.
 * A failed migration leaves all live state untouched. */
struct RhhModDefinition {
    uint32_t struct_size, abi_version, mod_version, state_schema;
    const char *id;
    uint32_t flags, state_size, hook_count, dependency_count;
    const struct RhhModHook *hooks;
    const char *const *dependencies;
    int (*migrate)(uint32_t old_schema, const uint8_t *old_data,
                   uint32_t old_size, uint8_t *new_data, uint32_t new_size);
    int (*validate)(const uint8_t *data, uint32_t size);
    uint32_t setting_count, reserved;
    const struct RhhModSetting *settings;
    int (*read_setting)(const void *state, const char *id, struct RhhModValue *value);
    int (*write_setting)(void *state, const char *id, const struct RhhModValue *value);
    /* Merge a schema-migrated durable journal into a staged state buffer.
     * Preserve run configuration and merge only monotonic facts. */
    int (*merge)(uint8_t *state, const uint8_t *journal, uint32_t size);
    /* Optional append-only extension. Legacy definitions end before this. */
    uint32_t shader_count, shader_reserved;
    const struct RhhModShader *shaders;
    uint32_t world_presenter_count, world_presenter_reserved;
    const struct RhhWorldPresenter *world_presenters;
    uint32_t service_requirement_count, service_requirement_reserved;
    const struct RhhServiceRequirement *service_requirements;
    uint32_t music_count, music_reserved;
    const struct RhhModMusic *music;
    uint32_t scene_rule_count, scene_rule_reserved;
    const struct RhhModSceneRule *scene_rules;
    uint32_t scene_light_count, scene_light_reserved;
    const struct RhhModSceneLight *scene_lights;
    uint32_t ecology_tag_count, ecology_tag_reserved;
    const struct RhhModEcologyTag *ecology_tags;
    uint32_t species_tag_count, species_tag_reserved;
    const struct RhhModSpeciesTag *species_tags;
    uint32_t type_count, type_reserved;
    const struct RhhModType *types;
    uint32_t move_count, move_reserved;
    const struct RhhModMove *moves;
    uint32_t learnset_count, learnset_reserved;
    const struct RhhModLearnset *learnsets;
    uint32_t battle_condition_count, battle_condition_reserved;
    const struct RhhModBattleCondition *battle_conditions;
    uint32_t move_effect_count, move_effect_reserved;
    const struct RhhModMoveEffect *move_effects;
    uint32_t move_effect_binding_count, move_effect_binding_reserved;
    const struct RhhModMoveEffectBinding *move_effect_bindings;
    uint32_t ability_count, ability_reserved;
    const struct RhhModAbility *abilities;
    uint32_t habitat_count, habitat_reserved;
    const struct RhhModHabitat *habitats;
    uint32_t shader_photo_policy_count, shader_photo_policy_reserved;
    const struct RhhModShaderPhotoPolicy *shader_photo_policies;
};
#define RHH_MOD_DEFINITION_BASE_SIZE offsetof(struct RhhModDefinition, shader_count)
#define RHH_MOD_DEFINITION_SHADER_SIZE offsetof(struct RhhModDefinition, world_presenter_count)
#define RHH_MOD_DEFINITION_WORLD_SIZE offsetof(struct RhhModDefinition, service_requirement_count)
#define RHH_MOD_DEFINITION_SERVICE_SIZE offsetof(struct RhhModDefinition, music_count)
#define RHH_MOD_DEFINITION_MUSIC_SIZE offsetof(struct RhhModDefinition, scene_rule_count)
#define RHH_MOD_DEFINITION_SCENE_SIZE offsetof(struct RhhModDefinition, scene_light_count)
#define RHH_MOD_DEFINITION_SCENE_LIGHT_SIZE offsetof(struct RhhModDefinition, ecology_tag_count)
#define RHH_MOD_DEFINITION_ECOLOGY_SIZE offsetof(struct RhhModDefinition, type_count)
#define RHH_MOD_DEFINITION_TYPES_SIZE offsetof(struct RhhModDefinition, move_count)
#define RHH_MOD_DEFINITION_MOVES_SIZE offsetof(struct RhhModDefinition, learnset_count)
#define RHH_MOD_DEFINITION_LEARNSETS_SIZE offsetof(struct RhhModDefinition, battle_condition_count)
#define RHH_MOD_DEFINITION_CONDITIONS_SIZE offsetof(struct RhhModDefinition, move_effect_count)
#define RHH_MOD_DEFINITION_MOVE_EFFECTS_SIZE offsetof(struct RhhModDefinition, ability_count)
#define RHH_MOD_DEFINITION_ABILITIES_SIZE offsetof(struct RhhModDefinition, habitat_count)
#define RHH_MOD_DEFINITION_HABITATS_SIZE offsetof(struct RhhModDefinition, shader_photo_policy_count)
#define RHH_MOD_DEFINITION_SHADER_PHOTO_SIZE sizeof(struct RhhModDefinition)
struct RhhModHostV1 {
    uint32_t struct_size, abi_version, pointer_bits, reserved;
    RhhContentKey (*content_key)(const char *canonical_name);
    int (*resolve)(uint32_t kind, RhhContentKey, uint32_t *engine_id);
    const char *(*content_name)(uint32_t kind, uint32_t engine_id);
    uint64_t (*random_at)(uint64_t seed, const char *mod_id,
                          uint64_t event_key, uint64_t ordinal);
    uint32_t (*content_count)(uint32_t kind);
    int (*content_at)(uint32_t kind, uint32_t index, struct RhhContentInfo *out);
    int (*content_info)(uint32_t kind, RhhContentKey key, struct RhhContentInfo *out);
    int (*get_setting)(const char *mod_id, const char *setting_id, struct RhhModValue *out);
    /* Append-only extension: check struct_size before using this member. */
    const struct RhhServiceHeader *(*get_service)(const char *id, uint32_t minimum_version);
};
#define RHH_MOD_HOST_V1_BASE_SIZE offsetof(struct RhhModHostV1, get_service)
/* Export this exact symbol from a trusted .dll/.so. No engine pointers cross
 * this boundary. Modules must match the executable's pointer width. */
typedef const struct RhhModDefinition *(*RhhModEntryV1)(const struct RhhModHostV1 *);

#endif
