#ifndef RHH_MOD_API_H
#define RHH_MOD_API_H

/* Public native mod ABI. This header deliberately has no engine dependencies.
 * See MODDING.md for ownership, ordering, persistence and compatibility rules. */
#include <stddef.h>
#include <stdint.h>

#define RHH_MOD_ABI_V1 0x00010000u
#define RHH_MOD_MAX_STATE (64u * 1024u)
#define RHH_MOD_REQUIRED 1u
#define RHH_MOD_DURABLE 2u
#define RHH_MOD_ID_MAX 64

typedef uint64_t RhhContentKey;
enum RhhContentKind {
    RHH_SPECIES = 1, RHH_MOVE, RHH_ITEM, RHH_ABILITY,
    RHH_TRAINER, RHH_MAP, RHH_AREA, RHH_MUSIC, RHH_MIDI, RHH_SOUNDFONT
};
enum RhhModEventKind {
    RHH_WILD_SPECIES = 1, RHH_TRAINER_SPECIES, RHH_STARTER_SPECIES,
    RHH_BASE_STATS, RHH_LEVEL_CAP, RHH_CAN_CAPTURE, RHH_CAN_REVIVE,
    RHH_ENCOUNTER_STARTED, RHH_MON_FAINTED, RHH_RUN_STARTED,
    RHH_RUN_LOADED, RHH_BATTLE_MUSIC, RHH_FOLLOWER_ENABLED, RHH_MON_IS_DEAD,
    RHH_CAN_CONTINUE, RHH_INVENTORY_CHANGED, RHH_WHITEOUT, RHH_WILD_CREATED,
    RHH_PERSISTENCE_FAILED, RHH_MON_CAUGHT, RHH_MOD_EVENT_COUNT
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
};
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
};
/* Export this exact symbol from a trusted .dll/.so. No engine pointers cross
 * this boundary. Modules must match the executable's pointer width. */
typedef const struct RhhModDefinition *(*RhhModEntryV1)(const struct RhhModHostV1 *);

#endif
