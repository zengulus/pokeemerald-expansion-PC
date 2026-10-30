# Native mod API v1

The native executable exposes the C ABI in `include/mod_api.h`. Load a trusted
module with `--mod path/to/module.dll` (Windows) or `--mod ./module.so` (Linux).
Modules must match the executable's architecture. They export
`RHH_ModEntry_v1(const struct RhhModHostV1 *)` and return a definition whose
storage remains valid until shutdown. The host checks ABI version, structure
sizes, duplicate IDs, dependencies, cycles, hook types and allocation limits
before opening a writable save. `native/examples/level_cap_mod.c` is a complete
external module; its level-50 constraint composes with the Gym cap by minimum.

The host accepts up to 128 registered modules, including its internal state
modules and bundled examples, and up to 256 hooks. Module state and durable
journals no longer have fixed byte quotas. The current mod ABI and encoded
extension use 32-bit lengths; allocation and representable-size failures are
reported rather than truncating data.

The native build includes **nine independent example mods**. Their complete
implementations live in `native/mods/`, use only `mod_api.h` and standard C, and
have their own settings and state. The engine adapter owns game objects, menus,
content views, run identity, and the calls into these examples.

| Module ID | Example source | Owned state |
| --- | --- | --- |
| `rhh:nuzlocke` | `nuzlocke.c` | Enabled rule, grace/status, area/death ledgers |
| `rhh:wild_randomizer` | `wild_randomizer.c` | Mode, frozen candidate keys, full-draw ordinal |
| `rhh:trainer_randomizer` | `trainer_randomizer.c` | Enabled rule and frozen candidate keys |
| `rhh:starter` | `starter.c` | Selected species key |
| `rhh:equal_bst` | `equal_bst.c` | Enabled 600-BST rule |
| `rhh:level_cap` | `level_cap.c` | Enabled Gym/Champion cap |
| `rhh:follower` | `follower.c` | Enabled follower request |
| `rhh:battle_music` | `battle_music.c` | Runtime music choice |
| `rhh:battle_environment` | `battle_environment.c` | Runtime persistent weather/terrain shaders (default off) |

Build their external versions with `make -f Makefile.native TARGET=linux64
example-mods` (or `windows64` / `windows32`). Run the game with
`--external-bundled-mods build/native/linux64/example-mods` to load the same
nine examples through `RHH_ModEntry_v1` instead of registering their built-in
copies. The Windows distribution uses `--external-bundled-mods example-mods`.
Static and external copies deliberately use identical IDs and byte schemas;
saves can move between those two loading modes. `common.h` contains shared,
public-API-only helpers. Build one independently, for example:

```sh
gcc -std=gnu17 -fPIC -shared -DRHH_MOD_SHARED -iquote include \
  native/mods/nuzlocke.c -o nuzlocke.so
```

This is a trusted native extension interface, not a sandbox or a mechanism for
untrusted downloadable code. It transforms existing engine content and permits
validated edits to owned Pokémon;
it does not promise to allocate new species, maps or move IDs in RHH's static
tables. Modules do not need to include or modify engine structs.

For service requirements and remaining API gaps, see
[Still-needed mod API services](NEEDED_MOD_API_SERVICES.md).

## Location ecology: a simple starting point

Use `rhh:service/ecology` for shared, save-backed conditions, species
populations and observation counts in a location. A condition is a signed
number named by a namespaced key; a population or observation count is
nonnegative and keyed by a species. Map
and species keys resolve through the content registry, including aliases.
There is no fixed entry quota. Values are shared between mods, so agree on the
meaning of any condition you both use. Later writes replace earlier writes in
the host's normal mod order.

For example, from an `RHH_MOD_UPDATE` observer, with `host` supplied to your
module entry point:

```c
const struct RhhEcologyService *eco = (const void *)host->get_service(
    "rhh:service/ecology", RHH_SERVICE_VERSION(1, 0));
if (eco && eco->header.struct_size >= RHH_ECOLOGY_SERVICE_V1_0_SIZE) {
    RhhContentKey route = host->content_key("rhh:map/route103");
    RhhContentKey species = host->content_key("rhh:species/poochyena");
    RhhContentKey food = host->content_key("example:ecology/food");
    int64_t count;
    if (eco->get(route, RHH_ECOLOGY_POPULATION, species, &count) == 1)
        eco->set(route, RHH_ECOLOGY_POPULATION, species, 20);
    eco->set(route, RHH_ECOLOGY_CONDITION, food, 5);
}
```

Use `RHH_ECOLOGY_OBSERVATIONS` with a species key to read or edit a simple
species/location observation count. A successful scouting observation updates
this count automatically; the separate actor-based scouting log remains available.

`get` returns 0 when present, 1 when absent, and -1 for an invalid request or
unavailable run. A stored zero is different from an absent value. `at` lists a
map's conditions, populations and observations; `remove` clears one. Writes are allowed in a
safe update or supported committed observer and are saved with the next normal
game save. Restoring an earlier generation restores its earlier values.
Queries do not advance RNG. A configured population multiplies that species'
ordinary encounter-table weight; absent populations retain the base weight,
and an all-zero table falls back to the base table. Counts do not automatically
decrement, advance time, or create actors: rules can use the existing encounter
and world-clock hooks to decide those behaviors. The
world-environment service supplies weather and terrain separately. This model
does not require offscreen individual Pokémon; the existing actor and dormant
services remain available for features that explicitly need them.

The native host also tracks encounter pressure separately from those public
population values. Each recorded wild defeat or catch reduces the affected
species' local encounter weight by 10 percentage points, down to zero; it
recovers toward the normal weight at 10 percentage points per real hour.
Recovery uses the PC system clock, including time while the game is closed,
and does not use the fake RTC or steps. An unavailable or backward clock does
not produce negative elapsed time. Pressure is saved with the game, and its
adjustment is applied after the encounter weight hooks, without changing the
stored population multiplier. Mods can set map conditions
`rhh:ecology/reduction` and `rhh:ecology/recovery_per_hour` to basis-point
values from 0 through 10000; both default to 1000. A recovery-rate change first
settles elapsed recovery at the previous rate. Invalid values use the default.
If all weights become zero, the usual base-table
fallback still applies. This pressure does not create persistent offscreen
Pokémon.

Encounter weights also reflect current conditions through species ecology tags.
A matching weather, terrain, or ripe-berry preference adds 25% to the weight;
matching categories add together. The saved population and pressure are not
modified by these bonuses. The registered tags are `rhh:preference/rain`,
`sun`, `sand`, `snow`, `fog`, `wind`, `grassy`, `misty`, `electric`, `psychic`,
and `berries` (each with the same `rhh:preference/` prefix). Rain includes heavy
rain and thunderstorms; sun includes harsh sun; snow includes hail. Ripe
berries must be present in an available current-map plot. The host supplies a
small set of species associations; mods can add associations with
`RhhModSpeciesTag` using the ecology-tags registry. A map condition named
`rhh:ecology/preference_bonus` overrides the 2500 basis-point bonus per match:
0 disables the bonus and 10000 gives 100%. The allowed range is 0..10000;
invalid values use the default. The weather sample uses the live effective
habitat weather, with saved effects taking precedence over natural weather.

Native builds enable ordinary overworld encounters by default; define
`WE_OW_ENCOUNTERS` to `FALSE` at build time to retain step encounters. Where
ordinary field spawns are possible, they replace step encounters; unsupported
areas, Safari, and locations without a usable spawn tile retain their existing
encounter handling. `WE_NATIVE_OW_REPLACE_RANDOM` can be set to `FALSE` to
allow ordinary step encounters alongside field encounters. A moving ordinary encounter shows a finite, tile-rooted grass rustle, cave or
sand dust, or water ripple. Face a visible disturbance and press R to observe
it. A successful observation reveals the same generated Pokémon with its
lower edge covered, and walking into it starts a battle with that Pokémon.
Scouting service observations also trigger the reveal. After the run has been
saved, the normal scouting policy and observation log apply; before the first
save, the reveal is transient. Special and manual encounters keep their own
presentation. If effect resources are unavailable the actor remains visible
rather than creating an unseen obstacle. The transient visuals do not simulate
or preserve ordinary individuals after they leave the field.

## Module archive records

`rhh:service/mod_archive` version 1.0 stores opaque records in the
host save extension. Records use an owner module key and a nonzero stable record
key; the module defines its own encoding and migration. `usage`, `at` and
`find` copy metadata for a registered owner in the active run. Enumeration
is ordered by record key. `copy` requires the record's observed revision and a
large enough buffer, so a replacement cannot silently change the bytes between
inspection and copying. Reads are available to trusted modules for any
registered owner; this API is not a confidentiality boundary.

`put` and `remove` work only in the owner's `RHH_MOD_UPDATE` observer, when
commands are allowed and no save transaction is active. Use expected revision
zero to create an absent record; replacing or deleting an existing record
requires its current revision. Failed operations leave records and output
buffers unchanged. Writes copy their input and change memory atomically, then
persist with the next ordinary save. They are not durable journal writes;
rollback restores the matching archive and can reuse a revision, so revisions
are not permanent IDs or rollback-proof receipts.

Records and their payloads grow with the archive, subject to available memory
and the 32-bit lengths in the current service and archive encoding. Retained
records for missing modules remain in saves until their owner returns and
removes them. A missing or different run cannot use another run's records.
Modules are trusted native code; owner checks prevent accidental cross-module
writes and are not a sandbox. Request version 1.0 and check
`RHH_MOD_ARCHIVE_SERVICE_V1_0_SIZE`.

Version 1.1 adds `batch` for one owner's 1–32 distinct record keys. Each
operation is a put or remove with the same expected-revision rules as the
individual commands. The host validates all operations and representable sizes,
copies every new value, then commits the entire batch without dispatching
callbacks in between. A removal can therefore free space or a record slot for
another operation in the same batch, regardless of their input order. The
result gives a revision for each operation in input order, including removals;
the final revision is the batch's last revision. Invalid requests, stale
revisions and allocation failures leave both records and output unchanged.
Like individual writes, a batch is held in memory until the next ordinary
save and is subject to rollback. Request version 1.1 and check
`RHH_MOD_ARCHIVE_SERVICE_V1_1_SIZE`.

Version 1.2 also permits writes from the owner's modal or overworld-overlay
input callback, including a panel opened through that owner's party-menu action.
The host verifies that the module requesting the panel or offering the action
matches its owner. A panel attributed to another module can still open under
the existing UI rules, but its input cannot write either owner's archive.
Drawing, close callbacks, nested events, callbacks after closure and active
save transactions remain unable to write. Request version 1.2 and check
`RHH_MOD_ARCHIVE_SERVICE_V1_2_SIZE`; its structure size is unchanged from 1.1.

Version 1.3 also accepts archive writes by the current owner during these
observer notifications, including nested notifications:

- `RHH_WORLD_STEP`, `RHH_WORLD_ENVIRONMENT_CHANGED` and `RHH_NATURAL_WEATHER_CHANGED`.
- `RHH_MON_EDITED`, `RHH_MON_CAUGHT`, `RHH_OWNED_MON_CHANGED`,
  `RHH_OWNED_MON_RELEASED`, `RHH_OWNED_MON_MOVED`, `RHH_OWNED_MON_PICKED_UP`,
  `RHH_OWNED_STORAGE_UNREADABLE`, `RHH_OWNED_GIFT_RECEIVED`,
  `RHH_OWNED_DEBUG_CREATED`, `RHH_OWNED_MYSTERY_GIFT_RECEIVED`,
  `RHH_OWNED_EVOLUTION_COMMITTED`, `RHH_PARTY_REORDERED`,
  `RHH_OWNED_DAYCARE_TRANSFERRED`, `RHH_OWNED_FUSION_TRANSFERRED`,
  `RHH_OWNED_CATCH_PLACED`, `RHH_OWNED_INGAME_TRADE_COMPLETED` and
  `RHH_OWNED_LINK_TRADE_SAVED`.
- `RHH_BREED_EGG_CREATED`, `RHH_EGG_HATCHED`, `RHH_EGG_CYCLE_TICKED` and
  `RHH_DAYCARE_PENDING_CHANGED`.
- `RHH_WILD_GENERATED`, `RHH_WILD_ACTOR_SPAWNED`, `RHH_WILD_ACTOR_REMOVED`,
  `RHH_WILD_ACTOR_ENGAGED`, `RHH_WILD_ACTOR_BATTLE_ENDED`,
  `RHH_WILD_ACTOR_IDENTIFIED`, `RHH_WILD_ACTOR_STEP_FINISHED`,
  `RHH_WILD_ACTIVITY_CHANGED`, `RHH_WILD_TRAIT_EDITED`,
  `RHH_WILD_PROFILE_EDITED` and `RHH_WILD_QUEUE_CHANGED`.
- `RHH_SCOUT_RECORDED`, `RHH_PHOTO_CAPTURED`, `RHH_PHOTO_STORED`,
  `RHH_PHOTO_REMOVED`, `RHH_PHOTO_REWARD_CLAIMED`, `RHH_PHOTO_REWARD_DELIVERED`,
  `RHH_PHOTO_EXPERIENCE_CLAIMED`, `RHH_PHOTO_EXPERIENCE_DELIVERED` and
  `RHH_PHOTO_EXPERIENCE_RETARGETED`.
- `RHH_MOVE_RESOLVED`, `RHH_BATTLE_TURN_ENDED`, `RHH_BATTLE_STARTED`,
  `RHH_BATTLER_ENTERED`, `RHH_BATTLE_ENDED`, `RHH_BATTLE_EVS_PROCESSED`,
  `RHH_ITEM_EV_PROCESSED`, `RHH_ITEM_EV_REDUCTION_PROCESSED`,
  `RHH_EXPERIENCE_AWARDED`, `RHH_MOVE_TAUGHT`, `RHH_BATTLE_CONDITION_CHANGED`,
  `RHH_BATTLE_ENVIRONMENT_COMMITTED` and `RHH_BATTLE_FIELD_RULES_COMMITTED`.
- `RHH_FIELD_HEALED`, `RHH_FIELD_STATUS_CURED`, `RHH_FIELD_POISON_DAMAGED`,
  `RHH_BERRY_PLOT_WATERED`, `RHH_BERRY_PLOT_MANUALLY_WATERED`,
  `RHH_BERRY_PLOT_INTERACTED`, `RHH_BERRY_PLOT_HARVESTED`,
  `RHH_BOSS_ACE_CONFIGURED`, `RHH_MUSIC_ASSIGNMENT_CHANGED` and `RHH_MUSIC_LEVEL_CHANGED`.

The existing event contracts still apply: for example, sampled changes can
coalesce and an unreadable-storage notification is not a successful transfer.
The command can fail for quotas, stale revisions or an active save
transaction, including during a listed notification; it does not roll back
the game event or guarantee delivery. Reducers, previews, drawing and other
observers remain unable to write. These writes change only module records and
do not permit other engine mutation commands. They persist on the next normal
save and roll back with its generation. Request version 1.3 and check
`RHH_MOD_ARCHIVE_SERVICE_V1_3_SIZE`. `python3 native/tests/photo_archive.py`
exercises a real capture and preview; `--gl` also checks SDL offscreen OpenGL.

## Content identity and random numbers

`rhh:service/content_display` version 1.0 copies player-facing names for
enabled species and known items, moves, abilities and types. Species and item
names come from the engine's display strings; move and ability names use their
existing service, including registered definitions, and type names use the type
catalog. Species and item names are converted to ASCII: the gender symbols
become `(F)` and `(M)`, acute e becomes `e` or `E`, and other unsupported
engine glyphs become `?`. The UI font also renders unsupported ASCII as `?`.
Names can be shared by distinct forms and are not persistent identifiers.
Unknown, disabled or unavailable content, unsupported kinds and insufficient
buffers fail without changing the output. Queries require finalized modules.
Request version 1.0 and check `RHH_CONTENT_DISPLAY_SERVICE_V1_0_SIZE`.
Version 1.1 adds `item_icon_rgba`, which copies the engine's selected 24×24
item icon and palette to tightly packed RGBA8888 pixels. Palette index zero
is transparent. TM and HM icons use the engine's current move-type palette;
missing icon data, invalid items or small buffers fail without changing output.
This is source artwork without live palette fades or sprite animation. Request
version 1.1 and check `RHH_CONTENT_DISPLAY_SERVICE_V1_1_SIZE`.
Version 1.2 adds `pokemon_icon_info` and `pokemon_icon_rgba` for the engine's
32×32 menu icons. Supply an enabled species key, personality and optional EGG
flag. The copied info identifies the personality-resolved species, actual
female artwork or placeholder when applicable, and bounded frame count.
Frames use the same species, personality, egg artwork and palette selection
as `CreateMonIconIsEgg`, with palette index zero transparent. The caller
chooses the frame and owns animation timing; no sprite or palette is loaded.
An unresolved form key is reported as zero. Unknown graphics, invalid
requests and small outputs fail without changes. Request version 1.2 and
check `RHH_CONTENT_DISPLAY_SERVICE_V1_2_SIZE`.

Version 1.3 adds `description` for known items and built-in or registered
moves and abilities. It copies the engine's base description or a registered
definition's authored ASCII without changing gameplay state. Engine line
breaks become newlines, display control codes are omitted, unsupported glyphs
and unresolved placeholders become `?`, and acute e is transliterated.
The result is a display aid, not a localized or dynamically expanded string;
effects changed by other rules do not rewrite it. Unsupported kinds, missing
or unavailable content and insufficient buffers fail without changing the
output. Request version 1.3 and check `RHH_CONTENT_DISPLAY_SERVICE_V1_3_SIZE`.
`RHH_CONTENT_DESCRIPTION_BYTES` includes space for the terminating NUL; the
query also fails if an engine description exceeds that bound.

### Versioned services

Hosts expose `get_service` as an append-only `RhhModHostV1` extension. A module
must first check that `host->struct_size` covers the pointer, then request a
named service with `RHH_SERVICE_VERSION(major, minor)`. The host returns NULL
for missing services, an unsupported major version or an insufficient minor
version. Service tables are host-owned and valid until host shutdown. Older
modules can continue using the original host layout.

Modules that depend on services declare `RhhServiceRequirement` entries in
their definition and set `abi_version` to `RHH_MOD_ABI_SERVICES` or a newer
extension ABI such as `RHH_MOD_ABI_MUSIC` or `RHH_MOD_ABI_SCENE`. Set
`RHH_SERVICE_REQUIRED` on entries needed for activation; leave flags zero for
optional services and check their availability with `get_service`. The host
checks required names and minimum versions at finalization, after services and
modules have registered but before hooks activate or a save opens. Missing or
incompatible required services reject startup with the module, service and
requested version in the error. A service's major version must match and its
minor version must be at least the requested version. Duplicate requirements
are invalid. Descriptor arrays and names must remain valid until unload.

```c
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0),
     RHH_SERVICE_REQUIRED, 0, "rhh:service/owned_mon"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0),
     0, 0, "rhh:service/overworld"}
};
/* In the module definition: .abi_version = RHH_MOD_ABI_SERVICES,
 * .service_requirement_count = 2, .service_requirements = requirements. */
```

This ABI value makes older hosts reject the module rather than ignore its
requirements. Host API entry points remain `RhhModHostV1` and
`RHH_ModEntry_v1`; modules that do not declare requirements can retain
`RHH_MOD_ABI_V1`. The historical base, shader and world-presenter definition
sizes remain supported.

```c
const struct RhhOwnedMonService *mons = NULL;
if (host->struct_size >= offsetof(struct RhhModHostV1, get_service) + sizeof(host->get_service)
    && host->get_service)
    mons = (const struct RhhOwnedMonService *)host->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
if (mons && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_0_SIZE) {
    struct RhhOwnedMonSnapshot snapshot = {.struct_size = sizeof(snapshot)};
    if (!mons->party_at(0, &snapshot)) { /* consume the copied view */ }
}
```

`rhh:service/owned_mon` version 1.0 provides copied, read-only snapshots of
the player's party and PC boxes. `party_at` and `box_at` use zero-based slots;
the service reports the number of addressable party, box and box-slot entries.
`find` accepts the existing OT-ID/personality identity used by faint hooks.
Callers initialize the snapshot's `struct_size`; failed queries leave it
unchanged. Multiple valid party/PC matches make `find` fail rather than select
a clone; this legacy lookup does not include other locations or corrupt slots.
The snapshot supplies species key, level, original and stat nature, IVs, EVs,
current engine limits and location. Nature values are engine ordinals, not
durable content keys. Queries do not offer editing or expose engine pointers.
They run on the game thread and the copied result must be refreshed after a
gameplay change. This service is available to built-in and external modules.

Version 1.1 adds `RHH_OWNED_MON_CHANGED` and the observer-scoped
`change_context`. When at least one module observes it, the host samples party
and box snapshots after the game frame, including frames in a modal mod panel.
The current service also samples daycare and fusion storage; changes are reported
in party order, then box, daycare and fusion slot order. The
context contains the run-scoped sequence, location, presence flags, changed
field mask and copied before/after snapshots. An addition or removal has one
snapshot and a zero field mask; a replacement can have both. The event key and
subject use the after snapshot when present, otherwise the before snapshot.
Identity is still OT ID and personality and can collide.

The initial sample, a new run and a loaded save produce a `RESET` notification
after establishing the baseline; consumers should refresh their caches. The
watcher defers sampling while the PC storage screen, a save transaction, a
link or recorded battle, a facility or tutorial battle is in progress, or the
save or PC storage is unavailable. Sampling also pauses while a battle party
menu temporarily rearranges the party array and resumes when field order is
restored. This is an invalidation stream, not a receipt: multiple writes within
a frame coalesce, a storage move can appear as separate
slot removal and addition, and intermediate states across frames can be
observed. The stream tracks fields in `RhhOwnedMonSnapshot` and, from version
1.25, stored experience. HP, moves,
nicknames and stored ability slots are not included. Calls to `change_context`
outside this event or with a small output fail without changing the output.
Existing `RHH_MON_EDITED` remains the synchronous, validated edit receipt.

Version 1.2 adds `identity_query`, a copied census of occupied slots with the
same OT ID and personality. It counts party, PC, daycare and fusion storage
separately, including eggs and corrupt matches. A single valid match supplies a
snapshot; multiple matches or corrupt data never supply a unique result. The
fusion-storage snapshot uses `RHH_MON_FUSION_STORAGE` and its slot index. On
FRLG builds the Route 5 daycare is `RHH_MON_DAYCARE` with box 1 and slot 0;
the regular daycare uses box 0. Queries require the save and PC storage to be
available and fail without changing output while the PC storage screen or a
save transaction is active, or during link, recorded, facility and tutorial
battles. These checks use the current in-memory game state and do not create a
durable individual ID or prove that a later matching Pokémon is the same one.
IV/EV edits, move teaching and individual innate rules use the same broader
fail-closed uniqueness check before acting.

Version 1.42 adds `identity_occurrences`, a single copied census of matching
occupied party, PC, daycare and fusion slots in that order. Each entry reports
its location and species when readable; an unreadable match has a flag and a
zero species key. As with `identity_query`, party slots beyond the engine's
current party count are not included. `matches` counts every match, while `count` is the number
copied into the bounded 512-entry array. A truncated flag means the array is
incomplete. The query shares `identity_query`'s unavailable-state checks and
leaves the output unchanged on failure. It is a diagnostic snapshot, not a
permanent individual ID or an authorization to edit a later occupant. Request
version 1.42 and check `RHH_OWNED_MON_SERVICE_V1_42_SIZE`.

Version 1.43 also invalidates the checked read-only battle-summary slot
mapping before an unreported native CPU, DMA or LZ/RL decompression write
overlaps a covered player slot. This applies even if identity tracking is not
active; an exact self-copy leaves the mapping intact. It does not make the
experimental owned-identity tracker a public permanent-ID service.

Version 1.44 extends the read-only summary guard to output buffers used by
nature-format reads and link conversions. It checks only successful writes;
an in-place conversion that leaves its source unchanged is allowed. This does
not change the limits of the slot mapping or create durable identity.

Version 1.3 adds copied slot access for every daycare and fusion-storage slot.
`daycare_count` and `daycare_slots` describe the regular daycare and, in FRLG,
the Route 5 daycare; `fusion_slots` describes fusion storage. `daycare_at` and
`fusion_at` reject empty, corrupt, out-of-range or temporarily unavailable
slots without changing the output, and use the same safety restrictions as
`identity_query`. These snapshots are selections of current locations, not
durable identities or a guarantee that a later occupant is the same Pokémon.

Version 1.4 adds the observer-only `RHH_OWNED_MON_RELEASED` event and
`release_context`. It is emitted after a confirmed PC release, with a copied
pre-release snapshot, source slot, run ID and process-local sequence. A Pokémon
held by the storage cursor uses `RHH_MON_MOVING`; its source is the last picked
slot and may already contain another Pokémon. Picking up, moving or cancelling
does not emit a release. The event does not cover other removal mechanisms and
is omitted if the Pokémon is corrupt or cannot be decoded. The PC remains busy
during the callback, so broad `identity_query` is unavailable. OT/personality
can collide and this event does not establish durable individual identity.

Version 1.5 adds the observer-only `RHH_OWNED_MON_MOVED` event and
`move_context` for PC placements. The receipt copies the held Pokémon before
placement and the destination after the engine's healing and form handling,
plus explicit source and destination slots. On a shift it also reports the
displaced Pokémon; if that occupied slot cannot be decoded, the unreadable flag
is set. The source slot is where the held Pokémon was last picked up and can
already have been reused. Picking up alone emits no move. Group moves emit one
event per successfully placed Pokémon in placement order; the group can be
partially placed during a callback, and broad identity queries remain unavailable
while the PC is open. Invalid moving Pokémon cannot produce a copied receipt.
Sequence numbers are process-local. This event covers PC placements; daycare
transfers, trades and other movement paths still need equivalent tracking.

Version 1.6 adds the observer-only `RHH_PARTY_REORDERED` event and
`party_reorder_context`. One copied batch describes all moved source and
destination slots after an overworld party-menu swap or party compaction. Flags
distinguish occupied and readable slots, so even an unreadable Pokémon retains
its slot mapping. The callback sees the completed permutation; the party count
may still be awaiting its caller's update, and broad identity queries are
unavailable during the callback. No event is emitted for a no-op or for a
battle-time reorder. The sequence is process-local and resets with the run.
These events do not make OT/personality a durable individual ID.

Version 1.7 adds the observer-only `RHH_OWNED_DAYCARE_TRANSFERRED` event and
`daycare_transfer_context`. It reports deposits from party or PC, withdrawals
to the last party slot, and the regular daycare's second-to-first slot shift.
Each receipt gives the source and destination coordinates and flags indicating
which copied snapshots are readable. An unreadable occupied Pokémon still has
a slot mapping. Deposits are reported after the source is cleared and before
party compaction; withdrawals are reported before party compaction and before a
subsequent daycare shift. Consume the separate party reorder event and daycare
shift event in order to follow the final slots. The party count may be stale
during these callbacks, and broad identity queries are unavailable. Receipts
require an empty destination, are omitted during battles or save transactions,
and have process-local sequence numbers. They do not cover trades or every
other movement path.

Version 1.8 adds `RHH_OWNED_FUSION_TRANSFERRED` and
`fusion_transfer_context`; the event is observer-only and the context is
available only during its callback. A receipt describes the second Pokémon moving
from the party into fusion storage, or being restored from fusion storage to
the party or the engine's PC fallback. It also copies the primary party
Pokémon before and after its form changes. The event occurs before any party
compaction; subscribe to `RHH_PARTY_REORDERED` to follow subsequent moves.
Flags identify readable snapshots, while source and destination slots remain
available for unreadable occupied Pokémon. Broad identity queries are
unavailable during the callback and the party count may still change. Receipts
are omitted for occupied destinations, failed PC placement, battles and save
transactions. Sequences are process-local and OT/personality is not a durable
individual identity.
The event arguments are cause, source location, destination location and
snapshot-validity flags, in that order. `subject` and `event_key` are zero
when their respective moved-Pokémon snapshot is unreadable.

Version 1.9 adds `party_view`, which distinguishes field order from the
temporary battle-menu order. During the temporary order, `party_at`, `find`,
`identity_query`, `daycare_at` and `fusion_at` fail without changing their
output; `box_at` remains available. The watcher also defers until field order
returns, so a battle-menu switch does not look like a permanent owned-party
reorder. This status reports the layout only: field order does not establish
that Pokémon in a link, recorded, facility or other special battle are owned.

Version 1.20 adds `RHH_OWNED_PARTY_SKY_BATTLE` and
`sky_party_field_slot`. When a configured sky battle temporarily selects part
of the player's party, the query maps each visible physical slot back to its
original field slot. It also accounts for a supported battle-menu reorder.
The normal party and identity queries and the owned-mon watcher remain
unavailable until the full party is restored. The mapping fails without
changing its output when the projection is invalid, the slot is not a selected
Pokémon, or the engine cannot establish the battle-menu order. The existing
`battle_view_field_slot` also reports original field slots during this view.
Sky battles require the engine's sky-battle flag and variable to be configured.

Version 1.21 adds `RHH_OWNED_PARTY_FOLLOWER_BATTLE` and
`follower_party_field_slot`. For a follower battle that selects a reduced
player party, the query maps a selected player slot to its original field
slot. It fails without changing the output while the mixed battle-party menu
is open; partner Pokémon are never presented as player-owned. Party and
identity queries and change sampling defer until the full party is restored.
The feature applies when NPC followers and a half-party battle are configured.

Version 1.22 extends that mapping through the supported mixed follower battle
menu. It follows player-only switches back to their original field slots and
rejects partner slots, including when a partner Pokémon has the same OT and
personality as a player's. Unsupported layouts, malformed orders and moves
between player and partner storage remain unavailable. The existing
`battle_view_field_slot` also returns the original field slot for supported
player entries in this view. These mappings also remain available while the
multi-battle summary temporarily combines the player and partner parties.

Version 1.23 also handles catches during reduced follower-partner battles.
The engine merges the selected battlers into the saved full party before the
catch, then restores the reduced battle party afterward. In the full-party
send-to-box menu, `battle_view_field_slot` reports the original field slot for
all six player entries. `follower_party_field_slot` and ordinary party queries
remain unavailable during that full-party transition. Catch receipts use full
field-slot coordinates. The menu prevents sending a Pokémon currently active
in battle to the PC; the existing HM restriction also applies.

Version 1.24 adds `experience`, a copied view of stored experience and the
species' current-level, next-level and maximum-level thresholds. It accepts
matching non-egg snapshots in the party, PC, daycare and fusion storage, and
fails during unsafe storage or party transitions. Experience is read at the
time of the call; the older owned-mon snapshot does not contain an experience
revision. Pending daycare steps are not included. This query does not award
experience or predict level-cap, move-learning or evolution outcomes. Add
`--engine-hard` on Linux to test an isolated engine hard-cap configuration.
The runner removes its temporary builds and files.

Version 1.39 adds `stats` for a matching, non-egg owned snapshot. It samples
the six field stats and current HP without editing the Pokémon. Party and
fusion storage report their stored values and level; PC and daycare stats are
reconstructed from a copy using the current stat rules, with the reconstructed
flag set. Pending daycare experience is excluded. If stored HP loss exceeds
the calculated maximum, current HP is clamped to zero and the clamped flag is
set. The older snapshot does not include HP, calculated stats or an experience
revision, so these values are sampled at the call. The query is unavailable
during battle and unsafe storage transitions, and leaves the output unchanged
on failure. Identical Pokémon in different slots remain independently readable.

Version 1.40 adds `iv_view`, which samples Hyper Training flags and the IV
values used for field stat calculations while retaining the inherited IVs in
the owned snapshot. Training is sampled at the call and is not part of the
older snapshot's optimistic token. The query accepts party, PC, daycare and
fusion storage, rejects eggs and unsafe or battle contexts, and leaves output
unchanged on failure. The coalesced owned-mon change stream now reports
`RHH_OWNED_CHANGED_HYPER_TRAINING`; its older snapshots do not contain the
flags, so observers should query again. This does not edit Hyper Training.

Version 1.41 adds `tera_view` for a matching non-egg party, PC, daycare or
fusion snapshot outside battle and unsafe storage transitions. It copies the
engine's stored ordinal and the resolved type, and identifies whether the
result is stored explicitly, derived from personality or forced by the
species. Known built-in types also have stable keys; unsupported stored or
resolved ordinals are flagged instead of being treated as custom type keys.
The older snapshot does not include a Tera revision, so values are sampled at
the call. `RHH_OWNED_CHANGED_TERA` invalidates this view when the stored or
resolved value changes in the coalesced owned-mon stream. This is the engine
value before battle-scoped mod overrides, and does not add custom-type storage
or edit the Pokémon.

Version 1.25 adds `RHH_OWNED_CHANGED_EXPERIENCE` to the coalesced owned-mon
invalidation stream, including changes within a level. The copied change
snapshots do not contain experience points. Version 1.26 adds
`experience_change_context`, which copies the before and after stored points
for each non-reset notification, alongside its location, run and sequence.
Its presence flags identify valid sides for additions and removals, and the
values are captured before observers run. Additions and removals retain their
existing zero field mask, and pending daycare steps are not part of stored
experience. These are sampled values, not individual award receipts.

Version 1.27 adds `preview_experience` for a fixed, already-scaled award. It
checks that the owned snapshot and stored experience still match, then reports
the points and levels after the current hard level cap and maximum-level bound.
It rejects a party snapshot whose displayed level disagrees with the stored
experience, rather than predicting an award from an inconsistent state.
It does not apply battle multipliers or soft-cap scaling, change the Pokémon,
check award eligibility, or simulate move learning or evolution. A future award
must revalidate its inputs and perform the normal level-up flow.

Version 1.28 adds `preview_experience_moves`. It applies the same fixed-award
preview, then pages through the current species' resolved level-up learnset
entries at the levels crossed. Each entry includes its level and stable move
key, including registered custom moves. The page reports its start, count and
total; a start beyond the end returns an empty page. The query consumes no
gameplay RNG, does not alter the Pokémon and fails without changing its output
if the experience snapshot is stale or the query is nested in learnset
resolution. These are candidates, not guaranteed prompts or learned moves:
already-known moves, form rules, player choices and evolution can alter the
actual sequence. It neither evolves the Pokémon nor commits an award.

Version 1.29 adds `award_experience`, `experience_award_status` and
`experience_award_context`. The request names a registered owner, a current
party experience snapshot and a fixed, already-scaled amount. It requires a
living non-egg Pokémon and an idle overworld. A successful request returns a
process-local ticket with `RHH_EXPERIENCE_AWARD_PENDING`; it opens the normal
party level-up flow and does not immediately change experience. Permission
constraints receive `RHH_CAN_AWARD_EXPERIENCE` both before opening and just
before applying the points. The latter check also revalidates the snapshot,
amount, level cap and recipient. Hooks must be pure and may deny the award.

The host emits `RHH_EXPERIENCE_AWARDED` after applying the points and updating
stats, then `RHH_EXPERIENCE_FLOW_FINISHED` after the player finishes any move
and evolution screens. A stale or denied pending request emits only the
finished notification. `experience_award_context` copies the result during
those callbacks; `experience_award_status` polls the accepted ticket until
another accepted request or a run reset replaces it. The included experience
preview describes the pre-award Pokémon, not the species after evolution.
Tickets and notifications are not durable receipts: an ordinary save rollback
can undo the award, and a reward system must separately coordinate its receipt
and delivery. The command does not apply battle multipliers or soft-cap scaling.
`python3 native/tests/experience_award.py` (or `--target windows64`) exercises
the party flow and removes its temporary artifacts.

Version 1.30 adds `RHH_OWNED_PARTY_UNION_BATTLE` to `party_view` while the
Union Room has substituted its selected two-Pokémon battle party. Ordinary
party and identity queries and change sampling remain unavailable until the
saved full party is restored; PC slot queries remain available. This view
does not make the temporary party available as ordinary owned snapshots. The internal experimental
tracker retains the original bindings across this transition only after
checking the selected party, saved full party and other storage. It refuses
saves during the projection and invalidates on a detected inconsistency.
Version 1.31 adds `union_party_field_slot`, which maps each checked selected
slot back to its original field slot, including supported single-battle menu
switches. `battle_view_field_slot` reports the same mapping while that menu is
open. Both leave outputs unchanged when the projection or menu order cannot be
verified.

Version 1.32 adds `RHH_OWNED_PARTY_LINK_MULTI_BATTLE` and
`link_multi_party_field_slot` for the cable-club multi-battle selection. The
mapping covers player slots in the supported mixed party menu and summary;
partner slots and unverified layouts remain unavailable. The experimental
tracker retains bindings only across a checked full-party restoration.

Version 1.33 adds `RHH_OWNED_PARTY_FRONTIER` and
`frontier_party_field_slot` for checked Frontier selections, including
supported single- and double-battle menus and Battle Tower's multi-battle menus and
summary view, including separate full-team pages. Partner slots are not owned player slots. During Frontier saves the engine
restores the full party for the transaction and then reinstates the selection.
Unexpected party writes or unsupported battle layouts invalidate tracking;
The Battle Dome's two-Pokémon selection also maps through its original
three-Pokémon selection. Frontier selected-party swaps retain their checked
slot mapping; the Battle Pyramid uses it to restore the correct individuals
even when selected Pokémon share a species. Frontier's shared save and Sketch
reset routines use the same mapping and update the saved order when it
describes the whole selected party. Other Frontier substitutions remain unsupported.

Version 1.34 extends `battle_view_field_slot` to full-team in-game multi
battles without a reduced-party projection. The separate player page maps back
to field slots; the partner page never returns owned slots. The same checked
page handling applies to supported Battle Tower selections.

Version 1.38 adds `RHH_OWNED_PARTY_RECORDED` during recorded-battle playback.
The playback party is not exposed as owned Pokémon. The experimental tracker
keeps field-party bindings while the original party is held separately, checks
that backup and other owned storage before restoration, and fails closed on
unsupported or changed data. This does not expose the experimental ledger or
guarantee that uninstrumented changes reversed between checks are detected.

Version 1.37 adds `RHH_OWNED_PARTY_RENTAL` while Battle Factory or the
Slateport Battle Tent substitutes a rental party. Party snapshots, identity
queries and field-slot mappings do not treat rentals as owned Pokémon. The
experimental tracker keeps the original slot bindings against the saved party,
including through rental replacement and supported battle-menu switches. A
Frontier save restores the original party for the save transaction, then
returns to the rental view; the final party restoration checks the saved
bindings. Instrumented writes to the saved original party and detected
identity or occupancy changes invalidate tracking. Unreported raw writes
that preserve the identity fields remain a limitation. This coverage does
not make the experimental identity ledger public or complete.
`python3 native/tests/owned_rental.py` (or `--target windows64`)
exercises these transitions and removes its temporary test artifacts.

Version 1.10 adds observer-only `RHH_OWNED_CATCH_PLACED` and
`catch_placement_context`. A catch that displaces a party Pokémon to the PC
emits a displacement receipt after the move succeeds. A subsequent successful
catch placement emits an acquisition receipt for its party or PC destination.
The acquisition's source is `RHH_MON_STAGED`, with the catching battler in
`source_slot`; it is not an owned slot. The before and after snapshots can
have different OT identities, because placement assigns the player's OT.
Validity flags retain the slot mapping when data is unreadable. Events are
delivered after follower-party synchronization, require an empty destination,
and are omitted for failed or cancelled placements, save transactions, and
link, recorded, trainer, facility and tutorial battles. Identity queries are
unavailable during the callback and the party count may be stale. Arguments
are cause, source location, destination location and snapshot-validity flags;
`subject` is the pre-placement identity and `event_key` is the placed
species, or zero if the corresponding snapshot is unreadable. The sequence is
process-local.

Wild-actor service version 1.14 adds `catch_origin`, available only during an
acquisition receipt. It links the placement to the engaged overworld actor
when the prepared opponent slot and original Pokémon witness still match. In a
double wild battle that creates a second opponent, the engaged actor occupies
opponent party slot 1; catching the other opponent does not acquire that
actor's association. Follower battles with one opponent retain slot 0. The returned
individual ID is optional, and the before/after Pokémon identities still are
not permanent IDs. Unavailable, changed or unreadable sources fail without
changing the output. This is a process-local association, not a durable catch
journal or a guarantee about later ownership transitions.

Version 1.11 adds observer-only `RHH_OWNED_INGAME_TRADE_COMPLETED` and
`ingame_trade_context`. The receipt reports the outgoing Pokémon's original
party or PC coordinates, the received Pokémon before exchange, and its
committed destination and snapshot before any trade evolution. A received
Pokémon's source snapshot is staged; matching OT/personality values do not
mean the two Pokémon are the same individual. Flags indicate which snapshots
are readable, while the source and destination coordinates remain available
for unreadable occupied data. The event arguments are flags, source location,
destination location and zero; `subject` is the sent identity and `event_key`
is the received species, or zero when their snapshots are unreadable. The
sequence is process-local.

In-game PC trades temporarily remove the selected Pokémon during the animation.
Until the received Pokémon is synchronized to its PC slot after any trade
evolution, `find`, `identity_query`, daycare/fusion lookups and sampled
invalidations defer. Party and individual box reads remain available. Trade
receipts are omitted if a destination cannot be placed or if a save transaction
is active. The captured party source must be its actual slot; PC trades use the
held staging slot. The committed destination must be the reported physical
slot. An unsafe or mismatched commit invalidates experimental identity
tracking instead of retaining a stale binding.

Version 1.12 adds observer-only `RHH_OWNED_LINK_TRADE_SAVED`,
`link_trade_context`, and `trade_state`. The host captures the outgoing and
incoming party Pokémon at exchange, verifies the received slot at the game's
link confirmation, then emits one receipt only after the native trade save
succeeds. The receipt includes the cable or wireless transport and the received
Pokémon after any trade evolution. Snapshot-validity flags preserve the party
slot even when data cannot be decoded. Link errors, a changed slot before
confirmation, missing save transactions and failed saves produce no receipt.
Version 1.45 additionally checks the received slot immediately before the
trade save. Its personality, OT ID and original-trainer name must still match
the confirmed Pokémon. When the incoming snapshot was readable, its egg flag
and IVs must also match and the final snapshot must remain readable. Species
may change through trade evolution. Version 1.46 accepts a readable species
change only when the engine reports a matching committed link evolution for
that slot; this tracking runs even with no evolution observer. A mismatch suppresses the receipt and
saved history, and invalidates experimental identity tracking; it does not
cancel the game's trade save. Unreadable incoming snapshots retain their
validity flags and cannot prove that all Pokémon data stayed the same.
During a link trade, party lookup, broad identity queries and change sampling
defer; individual PC-box reads remain available. `trade_state` also reports
an in-game PC Pokémon held during a trade.

The event occurs after the game save, so its observers' state changes are not
part of that completed save snapshot. Durable modules journal their changes
separately; other modules need a later save. The notification is process-local
and is not replayed automatically.

Version 1.13 adds a save-backed history of up to
`RHH_LINK_TRADE_HISTORY_CAPACITY` link trades, oldest first.
`link_trade_history_count` and `link_trade_history_at` copy records for the
active run. Each record has a run ID, a nonzero serial, the transport, party
slot and copied Pokémon snapshots. During `RHH_OWNED_LINK_TRADE_SAVED`,
`link_trade_receipt_context` copies the corresponding saved record.
The embedded trade's process-local `sequence` is zero in history; use the
record's serial for reconciliation. Record capture does not require an event
subscriber. A record is staged with the confirmed trade and included in the
same save generation; a failed save does not add it. After a crash between
that save and the live notification, a mod can query the history and reconcile
records it has not processed.

The history keeps the newest 64 records, so a consumer must detect when its
checkpoint predates the oldest retained serial. Restoring an older save
generation also restores its history and serial counter; serials can be reused
after rollback or across divergent copies of a save. A checkpoint must be tied
to the same save lineage, and these records do not provide permanent Pokémon
identities or exactly-once delivery for a mod.

Version 1.14 adds `link_trade_checkpoint` and `link_trade_reconcile`.
Checkpoints contain the run ID, record serial and a SHA-256 digest linked to
the preceding history. To begin, use a checkpoint with the current run ID,
serial zero and an all-zero digest. Reconciliation returns `READY` and the
next history index, or `UP_TO_DATE` when there is nothing to process. After
processing a record, copy its checkpoint and persist it with the mod's own
state.

`GAP` means required records are no longer retained, `AHEAD` means the
checkpoint is newer than this generation, `DIVERGED` means the serial exists
but its linked history differs, and `WRONG_RUN` means the run ID differs.
A checkpoint immediately before the oldest retained record can still be
verified against the retained predecessor digest. Existing version-1 archives
are migrated on load; when their earlier records were already discarded, the
missing prefix cannot be reconstructed. Linked digests detect differing
recorded histories but cannot distinguish separate save copies with identical
histories or provide permanent Pokémon identity.

Version 1.15 adds the observer-only `RHH_OWNED_MON_PICKED_UP` event and
`pickup_context`. A receipt identifies each occupied party or PC slot after it
is cleared into the storage cursor, including direct deposits and group moves.
`RHH_OWNED_PICKUP_VALID` marks a readable pre-removal snapshot; an unreadable
occupied slot still has its coordinates. Group receipts carry
`RHH_OWNED_PICKUP_GROUP` and are delivered as each slot is cleared, so the
selection can be partially removed during a callback. Empty slots emit nothing.
Use these receipts with the placement event's displaced Pokémon to follow a
shift; a shift does not emit a second pickup. The PC is busy during callbacks,
and sequence numbers are process-local. The older move and release receipts
omit unreadable held Pokémon; version 1.16 reports those separately.

Version 1.16 adds the observer-only `RHH_OWNED_STORAGE_UNREADABLE` event and
`storage_unreadable_context`. It reports a PC placement when its before or
after snapshot is unavailable, or a confirmed PC release when its snapshot is
unavailable. Its cause selects the copied `move` or `release` member; the
corresponding unreadable flags identify absent snapshots. Source and destination
coordinates, the group flag and displaced-slot details remain available.
Use this event alongside pickup, move and release events: a placement or release
emits either its older readable event or this event, never both. A readable
placement with an unreadable displaced Pokémon still uses the older move event.
The outer run ID and process-local sequence identify this event stream; the
embedded receipts' sequence fields are not assigned. The PC remains busy during
callbacks. These events do not provide durable individual identity or cover all
non-PC movement paths.

Version 1.17 adds the observer-only `RHH_OWNED_GIFT_RECEIVED` event and
`gift_context`. It reports successful `GiveScriptedMonToPlayer` acquisitions
and non-battle `GiveCapturedMonToPlayer` acquisitions, including script eggs.
The receipt copies the staged Pokémon and its final party or PC destination;
the owner-assignment path may change its OT identity. Flags indicate which
snapshots are readable and whether an occupied destination was overwritten,
including when the previous occupant cannot be decoded. The receipt follows
the party count and Pokédex updates where those are performed. Full storage or
a failed owner assignment emits no receipt. The event requires an active run
and an occupied source, and is suppressed while a battle, PC storage session,
save transaction or trade makes the owned storage view unsafe. Broad identity
queries are unavailable during the callback; the process-local sequence resets
with the run. This does not cover daycare eggs, wild catches, trades, or every
possible engine creation path, and does not assign a durable individual ID.
Gift receipt preparation and commit verify that the physical destination is
the reported party or box slot. If a captured write cannot be committed in
the same safe context, or the destination does not match, the experimental
identity tracker invalidates itself rather than retaining a potentially stale
binding. This includes byte-identical replacements and interrupted saves.
Evolution, daycare, fusion and catch receipts likewise check their physical
destination and invalidate tracking when a captured transition cannot be
committed safely. PC trade evolution uses its verified staging slot until the
trade places it in storage. A nonempty party reorder also invalidates tracking
if its commit is interrupted; an empty reorder leaves tracking unchanged.
These checks do not make the experimental tracker a public identity service.

Version 1.35 adds the separate observer-only `RHH_OWNED_DEBUG_CREATED` event
for the debug menu's fast and slow PC-box fill operations. Its `gift_context`
receipt has cause `RHH_GIFT_DEBUG`, a staged source and a final box slot. Each
successful placement produces one event; occupied or invalid destinations do
not. This event has its own process-local sequence, so debug fills do not
change the sequence of ordinary gift receipts. The experimental identity
tracker assigns a separate individual ID to every filled slot, including when
the fast fill copies Pokémon with identical OT and personality. The tracker
invalidates itself if it cannot safely capture a debug write. This does not
make the experimental ledger a public or complete permanent identity service.

Version 1.36 adds the separate observer-only
`RHH_OWNED_MYSTERY_GIFT_RECEIVED` event for successful Mystery Event Pokémon
gifts. `gift_context` reports cause `RHH_GIFT_MYSTERY_EVENT`, copied staged
and received Pokémon, and any overwritten occupant. The event has its own
process-local sequence and occurs after the Pokémon and mail are placed but
before the engine compacts and recounts the party. Its destination is therefore
the temporary last party slot; `party_at` may not yet expose that slot. Any
subsequent compaction is reported by `RHH_PARTY_REORDERED`, which gives the
destination after the move. A full party produces no receipt. The experimental
identity tracker allocates the new ID before following that compaction and
invalidates itself if the write cannot be captured safely.

Version 1.18 adds the observer-only `RHH_OWNED_EVOLUTION_COMMITTED` event and
`evolution_context`. A species-change receipt copies the Pokémon immediately
before and after the committed change, after its stats, nickname and Pokédex
are updated but before evolution moves are learned. A split receipt is emitted
when the additional Pokémon is created, with the evolved parent as its source
and the new party slot as its destination. It records an overwritten occupant
and whether its snapshot was readable. The split is a new individual even when
it shares the parent's OT and personality. No receipt is emitted for a
cancelled evolution or an unsuccessful split.

Trade evolutions carry a trade flag; link-trade receipts are in-memory events
before the trade save succeeds. A PC trade evolves a staged copy, so its
receipt marks `RHH_EVOLUTION_PC_PENDING` and gives the PC destination separately
from the staged before and after snapshots. The PC copy still has its previous
species during the callback and is synchronized later. Callbacks cannot run
broad identity queries. Receipts require an active run and safe owned-party
state; temporary battle parties and active save transactions are excluded.
Sequence numbers are process-local. These receipts do not provide durable
individual identity or guarantee a trade will be saved.

Version 1.19 adds `battle_view_field_slot`, which maps a physical slot in the
temporary battle party menu to the field slot it came from. It follows swaps
inside the menu using slot permutations, so matching OT and personality values
do not make the mapping ambiguous. The mapping includes empty slots and is
valid only while that menu view is active. It fails without changing the output
for invalid slots, non-permutations containing in-range slots, nested views, save or trade
transactions, and battles with borrowed, partner, link, recorded or facility
parties. The result identifies a slot at the start of that view, not a durable
Pokémon ID; ordinary party snapshot and broad identity queries remain suspended
while the view is active.

`rhh:service/daycare` version 1.0 provides copied views of the current
daycare's parents and pending-egg flag. `parent_at` reports the same owned-mon
stat snapshot plus gender, held-item and move content keys, deposited steps,
and the species' two current-engine egg-group ordinals. Empty slots and corrupt
parents fail without changing the output. `current` reports the number of
parents and the engine's unmodified compatibility category when both parents
are present; it does not predict a particular egg or apply future mod rules.
The queries copy parent data before decoding, consume no RNG and never create,
accept or reject an egg. Slot numbers and egg-group ordinals are not durable
identities. Daycare residents are queried here, not through the owned-mon
service's party/box `find`. A host-preserved custom move slot, when present,
is reported as its stable key even if its defining module is unavailable;
the key is not a guarantee that the move can currently be resolved or used.

Version 1.1 adds `timing`, a copied view of the next daycare egg-roll interval
and hatch-cycle check. It reports counts of engine step-function calls, the
current hatch-cycle length and incubator decrement, and, when two parents are
present with no pending egg, the next roll's estimated percentage including
the Oval Charm and current compatibility proposals in preview mode. A pending
egg pauses new rolls; an incompatible pair can still have a zero-percent roll.
This is not a prediction of the RNG outcome, future rule changes, or exactly
how many player movements will invoke those functions. Invalid or unsafe
queries, calls without an active run, and calls during a save transaction leave
the output unchanged and do not consume gameplay RNG.

Version 1.2 adds `egg_progress` for a valid egg in the stable field party.
It copies the egg's identity snapshot, remaining hatch cycles, current cycle
length and incubator effect, and the earliest number of `ShouldEggHatch` calls
under unchanged conditions. The engine checks an egg for hatching on the cycle
after its counter reaches zero. The result also counts earlier party eggs that
are already ready; the engine processes party order and can stop at one of
them before reaching this egg. Future changes or hatch interruptions can delay
the result. Bad eggs, non-eggs, unavailable party views and undersized outputs
fail without changing the result; no gameplay RNG is consumed.

Version 1.3 adds `hatch_eligibility` and `hatch_eligibility_context` for
ready eggs. A query returns `NOT_READY` for an egg with remaining cycles;
otherwise it dispatches the pure `RHH_CAN_HATCH_EGG` constraints. The event
key is the egg's species, the subject is its OT/personality identity, and
`args[0]` is its field-party slot. The scoped context copies the egg and run
ID; it is available only during that event. Denial is sticky, and callbacks
must produce the same result for equivalent preview and live inputs. When the
engine reaches a ready egg, it uses the same constraints; a denied egg stays
ready and the engine continues to later party members. An RNG-consuming or
stale proposal fails closed as `IMPURE`; both gameplay RNG streams are
restored. Invalid eggs and unavailable party views leave query output unchanged.
The preview describes current conditions and does not reserve a later hatch;
the existing `egg_progress` remains a lower bound, not a permission promise.

Version 1.4 adds `cycle_progress` and `cycle_progress_context`, and the pure
`RHH_EGG_CYCLE_PROGRESS` proposal. The event is dispatched for each non-ready
party egg at a hatch-cycle check, with species as event key, OT/personality as
subject, and slot, remaining cycles and the engine decrement in `args[0..2]`.
The engine decrement includes the incubator ability and is capped at the egg's
remaining cycles. Transforms may propose any decrement from zero through the
remaining cycles; constraints cannot rewrite it. Denied, invalid or
RNG-consuming proposals use the engine decrement, and both RNG streams are
restored. A changed party or clock makes the calculation stale and skips that
egg's update. `cycle_progress` previews the same rule without modifying the
egg or clock, and its copied context is available only during the proposal.
The preview does not reserve future conditions or guarantee the egg will be
reached before an earlier hatch interrupts the check. It samples the current
world and party, before the engine resets its cycle counter or updates earlier
eggs, so callbacks depending on those other values can produce a different
result during the live check. With a cycle hook active,
`egg_progress` reports the fastest possible bound: one update to reach zero
and a subsequent check to hatch. The global timing view continues to report
the engine's incubator decrement, before per-egg proposals.

Version 1.5 adds the observer-only `RHH_EGG_CYCLE_TICKED` notification and
`cycle_change_context`. In an active run with a stable field-party view, each
reports one non-ready egg after its cycle update,
including a zero decrement, with the before and after cycles, engine and
applied decrements, run ID and copied egg snapshot. Event key and subject are
the species and OT/personality; `args` contain slot, before, after and engine
decrement. The host verifies the slot, egg and resulting cycles before
dispatching, and restores both gameplay RNG streams after observers. The
sequence is process-local and is not a durable identity or rollback-proof
receipt. Previews and unrelated friendship writes do not emit notifications,
and an earlier hatch can stop the engine before later eggs are visited.

`rhh:service/abilities` version 1.11 exposes read-only ability data.
`info` copies the engine's copy, swap, trace, suppression, overwrite,
Mold Breaker and Imposter restrictions for a stable ability key. `species`
returns the three authored ability slots and the number of normal slots;
empty slots have key zero, and these slots are alternative choices, not
simultaneous innates. `owned` accepts a fresh party or PC snapshot, rejects
stale or ambiguous identities, and returns the stored slot and the resolved
ability using the engine's empty-slot fallback. The input snapshot does not
include an ability slot, so an ability-only change does not make that snapshot
stale; the query returns the current slot. These queries do not change the
Pokémon or the engine's last-used-ability bookkeeping.

`battler` returns the battle Pokémon's stored ability and effective ability,
plus an effective value ignoring the current Mold Breaker bypass. Zero means
no ability at that stage. The stored value may already reflect a battle-time
replacement or transformation; the overwritten flag reports the engine's
overwrite marker. The ordinary effective value uses the engine's current
attacker and Mold Breaker context, so use the ignoring-bypass value to inspect
the active ability outside a move. Gastro Acid, Neutralizing Gas, Ability
Shield and item suppression follow the engine's existing rules. Battler
queries require an active non-link, non-recorded battle and a present battler.
While in battle, actual owned and battler abilities are available outside
callbacks and during committed battle start, entry, turn, resolution or effect
observers. The battler view is unavailable after battle has ended. These
queries fail in pure calculation and AI callbacks, which should use
the ability information in their own contexts instead of hidden actual state.
Failed queries and undersized outputs leave the output unchanged.

Version 1.1 appends a shared, level-aware innate list while preserving the
`RHH_ABILITY_SERVICE_V1_0_SIZE` prefix. `species_innates` takes an enabled
species key and a level from 1–100 and returns up to three entries, including
locked entries and their unlock levels. It runs `RHH_INNATE_PROPOSE` as a pure
constraint event; the event key is the canonical species and `args[0]` is the
requested level. During that event, `innate_context` copies the current list,
and `offer_innate` accepts a canonical built-in or registered custom ability
key and an unlock level.
Entries follow normal hook order. Repeating the same ability and unlock level
is idempotent; offering that ability at a different level reports a conflict
and retains the earlier entry. Invalid or excess offers leave the list
unchanged, and any denial discards the whole list. Recursive queries and
out-of-scope offers fail. Querying consumes no RNG and does not mutate the
Pokémon. Mods must keep proposal callbacks pure and stable for equivalent run
state so that a summary, battle and AI receive consistent descriptions. The
host restores both gameplay RNG streams and discards a species proposal if a
callback advances either stream; an individual proposal that advances one is
discarded while retaining the species list.

Version 1.2 appends `battler_innates` and `power_innates`, preserving
`RHH_ABILITY_SERVICE_V1_1_SIZE`. They resolve the list for the current battle
species and level and report a bitset of reasons each entry is unavailable:
locked, duplicate of the primary ability, Gastro Acid, Neutralizing Gas,
move-specific bypass, or a battler temporarily off the field. Zero status
means usable under this model. The host applies each built-in ability's
suppression and Mold Breaker restrictions; an effective Ability Shield blocks
gas and move bypass. A duplicate primary is listed but is not usable as a
second copy. The battler query ignores move-specific bypass and has the same
actual-state access restrictions as `battler`. The power query is available
only during `RHH_MOVE_POWER` for its attacker or defender; the defender includes
the current move's bypass. In an AI calculation, `RHH_INNATE_AI_KNOWLEDGE`
marks that primary abilities and item effects come from the AI's cache. Unknown
values are treated as absent, so the result is an AI estimate and may differ
from the real battle. Suppression state and field gas use the current visible
battle state. Neither query records an item or ability, consumes RNG, or
changes battle state. Invalid scopes and outputs fail without modifying them.

Version 1.3 adds `RHH_INNATE_POWER`, a pure transform phase for each usable
innate in an ordinary damage calculation. It runs after `RHH_MOVE_POWER`,
first for the attacker and then the defender, in each resolved list's order.
A self-target is processed once. The event key is the innate ability, the
subject is the move, `ordinal` is the list index, and `args` contain current
power, role, attacker and defender. `innate_power_context` copies the move
inputs, current power, role, entry and resolved list during that event. It
fails outside the event or in nested callbacks without changing output.
Transforms can propose power from 1 to `RHH_MOVE_POWER_MAX`; a denied or
invalid proposal retains the power entering that individual trigger, then
the next innate still runs. Constraints cannot rewrite a proposal. A denied
general `RHH_MOVE_POWER` retains its input but does not veto innate triggers.
Gameplay RNG is restored around each trigger, and a trigger that consumes it
is discarded without suppressing later innate triggers. The same rule applies
to the separate innate accuracy triggers.
Like the general power hook, this is a calculation, not a committed event:
callbacks must be pure and may run repeatedly for AI, hits and targets.
It is not delivered in link or recorded battles or for fixed-damage effects.

Version 1.4 adds custom ability declarations. Use `RHH_MOD_ABI_ABILITIES` and
the appended `ability_count` and `abilities` fields in `RhhModDefinition`;
the preceding `RHH_MOD_DEFINITION_MOVE_EFFECTS_SIZE` prefix is unchanged.
Each declaration supplies a unique `namespace:ability/name` ID, an ASCII label
and description, and the `RHH_ABILITY_*` restriction flags. The host validates
IDs, text, flags, duplicates and content collisions before activation; a mod
can declare up to 32 abilities and the host can hold up to 256. Definitions
may coexist with the earlier declaration extensions. `custom_count`,
`custom_at` and `custom_info` enumerate copied metadata in stable ID order;
`info` also accepts custom keys. These queries require finalized definitions
for custom keys and fail without modifying undersized or invalid outputs.

Version 1.5 adds `RHH_INNATE_ACCURACY` and preserves the
`RHH_ABILITY_SERVICE_V1_4_SIZE` prefix. It follows the same resolved attacker,
then defender order and self-target rule as the innate power phase, after
`RHH_MOVE_ACCURACY`. The event key is the ability, subject is the move,
`ordinal` is the list index, and `args` contain the current accuracy, role,
attacker and defender. `innate_accuracy_context` copies the trigger context;
`accuracy_innates` provides the current resolution during the general
accuracy hook. Both queries are scoped to their respective events. Each
transform may propose 0–100; invalid or denied proposals retain the input to
that individual trigger, and constraints cannot rewrite values. A denied
general accuracy proposal does not veto the separate innate triggers. Engine input
may exceed 100, and an unchanged out-of-range input is retained. These hooks
are pure and may run repeatedly for battle and AI. Guaranteed hits, one-hit-KO
rules, link and recorded battles follow the general accuracy hook's exclusions.
AI resolution uses cached knowledge and is identified by
`RHH_INNATE_AI_KNOWLEDGE`.

Version 1.6 adds `RHH_INNATE_ENTRY`, a committed observer event for each usable
innate after the engine's entry hazards and general switch-in ability phase.
Other switch-in phases, including immunity abilities and items, run afterward.
The engine processes entering battlers in its switch-in speed order and each
battler's usable innates in list order. Initial entries and replacements use
`RHH_BATTLE_ENTRY_INITIAL` and `RHH_BATTLE_ENTRY_SWITCH`; the event includes
the corresponding `RHH_BATTLER_ENTERED` sequence and the battle ID, so it can
be correlated with the earlier animation notification. The event key is the
ability, subject is the participant's OT-ID/personality identity, ordinal is
the innate index, and `args` contain battler, cause, turn and entry sequence.
`innate_entry_context` copies the participant and resolved list only during
that observer; nested events and undersized outputs fail without changing the
output. Locked, suppressed, duplicate, absent or knocked-out entries do not
trigger. Redrawing a sprite or reprocessing a switch-in phase does not create
another entry. The identity can collide and the sequence is battle-scoped.
Link, recorded and AI-simulated entries do not emit this event. It cannot be
delivered if battle ends or the entrant is replaced before this phase.

Version 1.7 adds `innate_entry_message`, which queues copied battle text only
inside an `RHH_INNATE_ENTRY` observer. Up to eight messages are accepted per
entrant across all its innates and observers, in dispatch order. They use the
same character, length and line limits as move-effect messages; invalid text
and a full queue leave it unchanged. The engine displays them before continuing
the switch-in phases. Nested events and calls outside the observer are rejected.

Version 1.8 adds `innate_entry_heal`, which queues up to eight heals per entrant
across its innates and observers. Each uses a validated fraction of the entering
Pokémon's non-Dynamax maximum HP, rounded down with a minimum of one HP. Heals
run in dispatch order before entry messages, through the normal health-bar and
party-data controller updates. A fainted, replaced, full-health or Heal
Blocked entrant is skipped at execution. Rejected fractions, full queues,
nested events and calls outside the observer leave the queue unchanged. The
service does not infer this healing for a prospective switch; a mod can
explicitly describe it using the version 1.14 AI preview below.

Version 1.9 adds `innate_entry_stat`, which queues up to eight stage changes
per entrant. Stat changes run in dispatch order after healing and before entry
messages. They use the engine's normal stat-change scripts, including stage
limits and the recipient's Contrary and Simple. A fainted or replaced entrant
is skipped. A change is also skipped if an unrelated engine stat-change queue
is already occupied; it does not overwrite that queue. Invalid stats, zero or
out-of-range stages, a full queue and calls outside the entry observer are
rejected. Mods can explicitly describe these changes for prospective AI switch
evaluation through the version 1.15 preview below.

Version 1.10 adds `name`, which copies the display name of a built-in or
registered custom ability as UI-compatible ASCII. Invalid keys and small
buffers leave the destination unchanged; unsupported built-in glyphs become
question marks. On the native Pokémon info summary page, Select opens the
innate list when one is available. It shows the normal ability and each innate
as unlocked or with its unlock level; Select returns to the ability description
and trainer memo. The list describes species-level eligibility, not temporary
battle suppression or duplicate-primary status. Long display names are
shortened on the screen; the name query returns the full registered name.

Version 1.11 adds individual composition for owned Pokémon. `owned_innates`
accepts a fresh party or box snapshot and copies its composed list. After the
species proposal, `RHH_INNATE_PERSONALIZE` constraint hooks can use
`offer_innate` to add an entry and `remove_innate` to remove one by key.
`personal_context` copies the current list, identity, species, level and slot
only during that event. Hooks run in normal order, duplicates follow the
species offer rules, and a denial discards the entire individual composition
and retains the species list. Invalid or out-of-scope operations fail without
changing the list. Recursive innate queries are rejected. These proposals are
pure and may run repeatedly; module-owned configuration can be saved using the
normal mod-state mechanism.

The host requires the current snapshot to match its slot and counts matching
OT ID/personality pairs across the party, PC, daycare and fusion storage,
including corrupt occupied slots. Ambiguous identities or unavailable data
fail the owned query and fall back to species
rules in battle and the summary.
The key is not a permanent individual ID: a different Pokémon can later share
it, so persistent rules keyed by it can still match a replacement. Summary
personalization applies only to the player's actual party or box. Battle and
AI calculations use the same individual composition for an untransformed
player Pokémon whose battle identity, species and level match its party slot.
Link, recorded, debug, AI-versus-AI, Frontier, Trainer Hill and e-Reader battles
do not use individual composition. Opponent and partner Pokémon retain species
rules. The AI currently uses the player's individual composition without a
separate discovery model; no host-assigned persistent individual ID exists yet.

Version 1.22 adds `RHH_INNATE_BATTLE_PERSONALIZE` for battle-scoped slot
rules. After species and eligible owned-Pokémon proposals, constraint hooks
can inspect `battle_innate_context`, call `offer_innate`, or call
`remove_battle_innate`. A denial discards this phase's changes and retains the
prior list. The context includes the current composition, battle ID, battler,
owner, party slot, species, level and an authored opponent trainer key when
available. Partner and non-authored opponents have no trainer key. These are
battle-scoped coordinates, not a durable individual identity; switching or
reordering can change a slot's occupant. Callbacks must be pure and return the
same composition for the same context in battle and AI calculations. The host
restores both gameplay RNG streams and discards this phase if a callback uses
either one. The same resolved list participates in existing innate power,
accuracy, entry and turn hooks and applicable AI previews. AI sees these rules
without a separate discovery model. Link and recorded battles remain excluded.
Version 1.23 adds the observer-only `RHH_BATTLER_ABILITY_OBSERVED` event and
`ability_observation_context`. It samples present local-battle battlers once
per frame and reports copied before/after ability states and the participant
when the stored ability, suppression-adjusted active ability, or overwritten
flag changes. Both active values ignore move-specific Mold Breaker bypass.
The first valid sample and each checked switch establish a baseline; changes
between samples can be missed or coalesced. A missing or changed battler also
establishes a new baseline. The sequence resets each battle. This is an
observation, not a committed transition or a cause, and does not run in link
or recorded battles. The participant's OT/personality can collide and is not
a permanent individual ID. The event key is the new stored ability, subject
is that identity, and arguments identify battler, owner and party slot.
Version 1.24 adds the observer-only `RHH_BATTLER_INNATES_OBSERVED` event and
`innate_observation_context`. When requested by an observer, the host samples
each living battler's full resolved innate list after the game frame, including
locked, duplicate and suppressed entries. It reports a copied participant and
before/after lists when composition, unlock status or suppression changes.
The first valid sample, a switch, or an unavailable battler establishes a new
baseline; changes between samples can be missed or combined. This notification
does not identify a cause or apply effects, and does not run in link or recorded
battles. The event key is the new species key, subject is the participant's
non-unique OT/personality identity, and arguments are battler, owner and slot.
Its sequence is independent of the primary-ability observations and resets
with the battle. Sampling uses the same gameplay-RNG-safe composition rules as
the normal innate query and does not expose AI's simulated state.
Version 1.28 adds `RHH_BATTLER_ABILITY_MUTATED` and
`ability_mutation_context` for committed changes made by the engine's Role
Play/Doodle copy, Skill Swap, Gastro Acid, ability-overwriting moves,
Entrainment and Trace script commands. It copies the present participant,
before and after ability states, overwritten-ability keys and the Gastro Acid
bit, plus the cause, source battler and move when available. Both sides of a swap are sampled after both writes
and share a battle-scoped batch ID; unchanged writes do not notify. The event
requires a checked present participant and excludes pending switches, link and
recorded battles. Its sequence and batch IDs are process-local; observers
cannot issue gameplay commands, and both gameplay RNG streams are restored.
This does not cover all ability mutation paths or changes to other battlers
caused by global suppression. The existing frame observer may also report a
later coalesced view of the same change.
Version 1.29 adds the separate observer event
`RHH_BATTLER_ABILITY_CONTACT_MUTATED` and
`ability_contact_mutation_context` for Mummy, Lingering Aroma and Wandering
Spirit. The context contains the triggering ability and the same copied
participant, move, source and before/after data; the generic mutation context
is also available during the new callback. Wandering Spirit's two subjects
share a batch sampled after both writes. This event has its own battle-scoped
sequence and batch IDs, leaving the v1.28 script-mutation stream unchanged.
The engine's contact, Ability Shield and other eligibility rules still decide
whether a change occurs. The same local-battle and participant restrictions
apply; other ability changes and global suppression are not covered.
Version 1.30 adds `RHH_BATTLE_NEUTRALIZING_GAS_CHANGED` and
`gas_change_context` for the engine's gas activation, Gastro Acid, switch-out,
faint and replacement boundaries. The copied before/after snapshots report
the engine's effective gas-source mask and, for each sampleable slot, its
participant, primary ability and resolved innates. Availability flags identify
missing views; ability values ignore move-specific Mold Breaker. A source-mask
change is reported even if another source keeps gas active. The event is
observer-only, restores both gameplay RNG streams, and excludes link and
recorded battles. Its sequence is process-local; it does not cover every raw
engine state write or guarantee a durable participant identity. On Gastro Acid,
this event precedes the separate committed ability-mutation notification.
The switch-in and faint volatile-state cleanup routines also report a source
that survives until cleanup; they do not duplicate an earlier removal. At a
switch-in cleanup the source slot may already refer to the incoming Pokémon,
so consumers must not treat that slot as the identity of the departing source.
Version 1.31 adds `priority_innates` during `RHH_MOVE_BATTLER_PRIORITY` and
the pure `RHH_INNATE_PRIORITY` trigger with `innate_priority_context`. After
the general battler-priority proposal, the host visits the user's usable
innates in resolved list order. Locked, suppressed, duplicate-primary and
off-field entries do not trigger. Each callback receives the current signed
priority, move context and copied resolution; its decision uses the same
priority-plus-7 encoding and -7 through +10 bounds as the battler hook.
Constraints can deny but cannot rewrite a proposal. Invalid or RNG-consuming
proposals leave that entry's input unchanged, and both gameplay RNG streams
are restored. The calculation uses the caller's supplied primary-ability
assumption and, for confirmed AI calls, cached item knowledge. When AI
knowledge is unavailable, the innate phase is skipped. An unclassified shared
call uses the live item effect, so it is not a guaranteed AI-knowledge view.
Quash, Struggle, link
and recorded battles retain the battler hook's exclusions. This changes only
priority and does not implement built-in ability mechanics automatically.
Version 1.32 adds `speed_innates` during `RHH_BATTLER_SPEED` and
`innate_speed_context` during `RHH_INNATE_SPEED`. After the general speed
proposal, usable innates run in resolved order with locked, suppressed,
duplicate-primary and off-field entries skipped. Each can transform the
current speed within the same bounds as the general proposal. Denied, invalid
or RNG-consuming proposals leave that entry's input unchanged and both RNG
streams are restored. The caller's primary-ability assumption controls the
duplicate test; confirmed AI calculations use the caller's supplied hold
effect for Ability Shield. If AI data is unavailable the innate phase is
skipped. This is an API point; it does not automatically activate built-in
innate abilities.
Version 1.33 adds `damage_stat_innates` during `RHH_DAMAGE_STAT` and
`innate_damage_stat_context` during `RHH_INNATE_DAMAGE_STAT`. After each
general Attack or Defense proposal, usable attacker innates and then usable
defender innates receive the current value and a copied resolution; a
self-targeting calculation visits the participant once per stat. The supplied
calculation abilities determine duplicate-primary status; defender resolution
also respects move bypass and the calculation's item knowledge. Locked,
suppressed, duplicate and off-field entries are skipped. A denied, invalid or
RNG-consuming entry retains its input and both gameplay RNG streams are
restored. Innate identities do not automatically acquire built-in effects.
Version 1.34 adds `innate_entry_weather`, `innate_entry_terrain`,
`innate_turn_weather` and `innate_turn_terrain`. An entry observer can queue up
to eight field actions; they execute after entry status actions and before
entry animations and messages. Turn-end actions share the existing 32-action
queue and execute in queue order before the turn-end notification. Weather
accepts rain, sun, sand, hail, snow or fog; terrain accepts the four normal
terrain kinds. None, primal weather and strong winds are not requests these
endpoints support.

The host rechecks the source's battle identity and that the same innate is
still usable before each action. The engine then applies its normal same-field,
primal-weather, overworld-weather, sky-battle, duration and held-item rules;
a rejected or stale action is skipped. A full battle-script stack also skips
the action before changing the field. The source is rechecked by innate key,
so reordering the resolved list alone does not invalidate it. Successful changes use the normal
field message, animation and weather/terrain follow-up scripts. Battle service
version 1.35 reports them with `RHH_BATTLE_ENV_CAUSE_INNATE` and the stable
innate key. No built-in innate behavior is added automatically. Ability
service version 1.35 adds the separate pure `RHH_AI_INNATE_ENTRY_FIELD_PREVIEW`
constraint, `ai_innate_entry_field_context`, and weather/terrain offer functions.
A mod must opt in separately and describe the same deterministic entry action;
the host never executes the committed observer during a forecast. Up to eight
offers per candidate compose in innate and offer order. Denial discards that
callback's offers, and gameplay RNG use rejects the preview and restores both
streams. The copied context reports predicted underlying weather, terrain,
timers, blocked conditions and suppression based on AI-cached abilities.
Innate resolution also uses the engine's current Neutralizing Gas field marker,
as in the existing innate forecasts.

Ability service version 1.36 adds `ai_innate_entry_field_projected` during that
same preview. It returns the field after applying all offers currently retained
in order, including offers made so far in the current innate's dispatch. Those
current offers are tentative: if that dispatch is denied, all of its offers
are removed before the next innate. The original context function continues
to return the field before any innate offers. Both views use cached AI
knowledge, the same engine restrictions and timers as the forecast, and do
not change the battle. If sequential offers restore the original weather or
terrain but refresh its timer, the switch forecast retains the refreshed
timer. The projected query fails without modifying its output
outside the field-preview event or with an undersized output structure.

The single-battle smart switcher forecasts these offers after entry hazards and
existing innate stat/status forecasts, then recalculates speed, move damage and
weather impact for that candidate. Its scoped field values are discarded before
the next candidate and before returning; actual weather and terrain are not
mutated. The forecast models common primary weather and terrain abilities,
weather rocks, Terrain Extender, primal and overworld weather locks, and sky
battles. It is approximate: it does not simulate field-change follow-up
abilities, terrain seeds, future field changes or innate turn-end field actions,
and other engine code may still read the real field directly. Double battles
and opponent-switch prediction do not use this preview.

Version 1.12 adds `RHH_INNATE_TURN_END`. After the engine completes its normal
end-of-turn effects and shared condition timers tick, the host visits present,
living battlers in engine turn order and each usable innate in list order.
Locked, suppressed, duplicate-primary and off-field entries do not trigger.
The copied `innate_turn_context` identifies the ability, participant, resolved
list, completed turn and battle-scoped turn sequence. The event is delivered
once per completed turn and is excluded after battle ends and in link or
recorded battles. The identity is OT/personality, which can collide.

During this observer, battle service version 1.9 permits `condition_set`, so an
innate can apply or refresh a shared field or side condition before general
`RHH_BATTLE_TURN_ENDED` observers run. New conditions do not tick until the
next completed turn. `innate_turn_message` queues copied battle text for the
normal end-of-turn message script; it shares a 32-message limit with condition
expiry messages. Context and message calls fail outside the observer, including
inside nested notifications.

Version 1.13 adds `innate_turn_heal` and `innate_turn_stat`. They queue up to
32 self-directed actions across all battlers and observers in a completed turn.
Healing accepts fractions from 1/16 through 16/16, using the non-Dynamax maximum
HP; stat changes accept nonzero stages from -6 to 6. All innate callbacks
finish before the actions execute in offer order through the normal battle
health and stat scripts. Before each action the
host checks the battler, owner, party slot and OT/personality identity; an
absent, replaced or fainted recipient is skipped. Identity can collide. Healing
also skips full-health or Heal Blocked recipients; a stat change skips an
occupied engine stat-change queue and uses normal engine ability and stage
rules. Conditions are set during the innate observer; queued actions finish
before `RHH_BATTLE_TURN_ENDED` runs, and queued messages are displayed after
that notification. If the battle ends or changes while actions are pending,
the remaining actions, queued messages and turn notification are skipped. The
version 1.20 AI preview can model constant self-healing, but not turn stat
changes. `python3 native/tests/innate_turn.py` checks ordering, action
execution, recipient revalidation and message delivery; it removes its
temporary build artifacts.

Version 1.25 adds `innate_turn_status_target` for existing major statuses at
the same turn-end boundary. It shares the action queue and checks both the
source and recipient again at execution. The engine applies normal status,
type, ability, terrain and Safeguard protections and handles the status through
its ability-triggered script. The action does not inherit the source's primary
ability's Corrosion or Infiltrator behavior and does not use an earlier move's
Mold Breaker state. It follows ability-style status application through
Substitute; confusion and new status definitions are not supported.

Version 1.26 adds `innate_entry_status_target` with the same major-status
rules and source/recipient checks for switch-in observers. Up to eight status
actions are queued across the entry and run in offer order after entry healing
and stat actions, before entry animations and messages.

Version 1.27 lets an innate opt into forecasting an opponent-directed entry
status during the existing `RHH_AI_INNATE_ENTRY_STAT_PREVIEW` constraint.
The status context reports the opponent's visible major status and cached AI
ability and item knowledge; the status offer shares the eight-action limit
with stat offers. For single-battle smart switching, accepted offers are
modeled after entry stat changes using public types, known abilities and items,
weather, terrain and relevant status protections. Unknown abilities and items
are treated as absent; hidden types cause the status forecast to be skipped.
The model assumes success when no known protection applies and uses a fixed
two-turn sleep estimate. A known, active Synchronize can reflect poison,
burn or paralysis to the candidate, subject to its known protections and the
configured toxic rule. Damage, speed and the candidate's survival estimate
then use both predicted statuses. Known status-curing berries activate after
Synchronize, unless suppressed by the known opposing Unnerve family, Magic
Room, Embargo or Klutz. The forecast consumes each berry at most once across
the offered actions, applies Unburden to the temporary simulation, and keeps
unrevealed items unknown. It does not simulate Cheek Pouch healing or other
secondary effects of eating a berry. Separate survival estimates start with the
same toxic counter. The opponent is restored before each next candidate and
after the decision. The model does not simulate residual
damage to the opponent or turn-end status actions. Forecasts are not
committed effects.

Version 1.14 adds the pure `RHH_AI_INNATE_ENTRY_PREVIEW` constraint event and
`ai_innate_entry_context`. During the single-battle smart switcher's own-party
candidate evaluation, it resolves the candidate's usable innates using the
AI's ability and item knowledge. If the candidate is estimated to survive
entry hazards and is not already fully healed, each usable innate can offer
up to eight total deterministic self-heal fractions through
`offer_ai_innate_entry_heal`. Denial discards the offers for that innate. The
host caps the result to the candidate's missing HP and adds it after hazards
to the switcher's existing survival calculation. This does not update cached
move-damage or speed estimates. The smart switcher uses the original candidate
HP to apply hazards once, treats fatal hazards as fatal, and accounts for a
single-use healing item after entry effects without using it twice. It does not consume game RNG,
execute an entry event or change the actual Pokémon. A preview describes what
the mod expects its real entry effect to do; the host cannot guarantee that
the later battle will match. The context contains the AI's own candidate,
not another party's hidden Pokémon. Opponent-switch prediction, link and
recorded battles, vanilla switch selection and double battles do not use this
preview.

Version 1.15 adds the separate `RHH_AI_INNATE_ENTRY_STAT_PREVIEW` constraint
event. `ai_innate_entry_stat_context` supplies the same candidate and its
pre-innate stat stages, expressed from -6 to +6. Usable innates may offer up
to eight deterministic self stat changes with `offer_ai_innate_entry_stat`;
denial discards that innate's offers. The smart switcher applies those offers
in order, including Contrary and Simple, to a temporary candidate, then
refreshes its damage and speed estimates. The event also runs at full HP.
Like healing previews, these estimates are opt-in and do not execute actual
entry effects. Shared
conditions and other entry effects are not predicted.

Version 1.16 adds `innate_turn_heal_target` and `innate_turn_stat_target`.
An end-of-turn innate observer may queue either action for a present, living
battler, using the same limits and order as self-directed actions. The host
checks the source and recipient's battler, owner, party slot and OT/personality
both when queued and when executed; a replaced or fainted participant cancels
that action. Stat actions use the existing `STAT_CHANGE_CERTAIN` recipient
script and do not carry opponent-attack source attribution. Targeted turn
actions are not predicted by the AI. OT/personality can collide and is not a permanent ID.

Version 1.17 adds `innate_entry_heal_target` and `innate_entry_stat_target`.
An entry observer may queue actions for another present, living battler; the
entrant and recipient are checked again before each action. These share the
eight-heal and eight-stat limits with the existing entry calls, with heals
executed before stat changes and messages. The stat script uses the recipient
as its scripting battler and does not carry opponent-attack source attribution.
AI entry healing previews remain self-directed; opponent stat changes can be
offered separately as described below.

Version 1.18 adds `innate_entry_stat_from` and `innate_turn_stat_from` for
source-attributed ability stat changes. They use the same checked recipients,
queues and execution order as the target calls, but the engine processes the
change as originating from the innate holder. This uses ability stat-change
rules, including Mist, stat-loss prevention, Mirror Armor, Contrary, Simple and
Defiant, and ignores Substitute and a previous move's Mold Breaker state. AI
can predict explicitly offered opponent entry-stat changes as described below. The
earlier recipient-only stat calls retain their original behavior.

Version 1.19 extends the existing pure AI entry-stat preview with
`ai_innate_entry_opponent_context` and `offer_ai_innate_entry_opponent_stat`.
In a single battle, the context provides the opponent's current stages and
the AI's cached ability and item keys. Offers share the eight-action limit and
are discarded if their innate's proposal is denied. The smart switcher models
the offered source-attributed stat changes, including stat-loss protection,
Contrary, Simple, Mirror Armor and Defiant/Competitive, before recalculating
damage and speed for that candidate. Each candidate starts from the same
opponent state, and battle state is restored after the choice. A mod must
explicitly offer its deterministic preview; the host does not infer it from
entry actions. This does not predict target healing, turn stat changes,
double battles or unrelated entry effects.

Version 1.20 adds `RHH_AI_INNATE_TURN_HEAL_PREVIEW`,
`ai_innate_turn_heal_context` and `offer_ai_innate_turn_heal`. A mod can opt
in to a constant self-heal for each simulated completed turn. The context is
the candidate's estimated HP after hazards and innate entry healing; callbacks
must offer only effects that remain valid on every simulated turn, independent
of future HP, turn count and random outcomes. Up to eight fractional heals
share the same bounds as entry previews, and denied offers are discarded.
Healing is applied after the estimated attack and other end-of-turn effects,
only while the candidate remains alive. Existing Heal Block suppresses the
preview for the entire estimate; expiry, changing conditions, targeted heals
and turn stat changes are not predicted. Like other AI previews, this is an
estimate and never executes the committed battle action.

Version 1.21 adds `RHH_AI_INNATE_TURN_STEP_PREVIEW`,
`ai_innate_turn_step_context` and `offer_ai_innate_turn_step_heal`. Unlike the
constant preview, this event runs for each simulated turn after the attack and
other estimated end-of-turn effects. Its context HP is the candidate's
remaining HP before innate healing, so a mod can model a threshold-dependent
heal. No event runs if earlier damage causes a knockout. Offers from this event
are added to the constant preview; both must correspond to actions the mod
actually queues in battle. Each step has an eight-offer limit, and a denied
innate's offers are discarded. Callbacks must be deterministic and depend only
on the supplied HP and otherwise unchanged context, not on hidden mutable
state, future turn counts or RNG. The simulation detects repeated HP states
after other estimated timers and status have stabilized. Heal Block expiry,
changing field conditions, targeted healing and turn stat changes remain
outside this prediction.

Battle service version 1.8 also permits `condition_set` from an `RHH_INNATE_ENTRY` observer,
using the same validated condition targets, duration and committed change
notifications as move observers. It does not add a general battle-script
command.

The stable key can be offered by another module after finalization, and the
same innate resolution applies its declared suppression and bypass flags.
Custom keys do not occupy an engine ability slot, cannot yet be selected as a
Pokémon's ordinary ability, and are not included in the generic built-in
content resolver. Removing the defining module makes the key unavailable;
an offer of that missing key is rejected. Mods implement behavior in the
relevant hooks; neither declarations nor innate lists automatically execute
built-in ability effects. Other
battle phases, ordinary ability slots, copying and persistent individual
overrides remain open.

`rhh:service/breeding` version 1.5 exposes a copied `RhhBreedingContext` only
inside breeding callbacks. Require the service in a module definition and
initialize the context's `struct_size` before calling `context`. The context
contains both parent snapshots, the unmodified and effective compatibility
categories, and, when an egg is being created, the two inheritance slots and
the engine's original offspring key. In the committed notification it also
contains the actual offspring key and OT-ID/personality egg identity. That
identity can collide and is not a durable individual ID. Version 1.1 appends
an offspring snapshot and four move keys: the location is `RHH_MON_STAGED`
before commitment and the actual party slot after it. Older callers may supply
the `RHH_BREEDING_CONTEXT_V1_0_SIZE` or `RHH_BREEDING_CONTEXT_V1_1_SIZE`
prefix; the host copies only the requested supported prefix.

Version 1.2 adds `preview`, which evaluates the current two daycare parents
using the same compatibility and offspring proposals as egg creation. It
returns the original and resolved compatibility and up to two distinct
original/resolved offspring keys with the corresponding inheritance slots.
If an egg is pending, it uses the already selected species-variant bit;
otherwise it reports both possible variants when they differ. It uses the
engine's parent, form and incense rules and does not roll an egg, change the
daycare or consume gameplay RNG. The result is a current preview, not an egg
reservation: changes to parents, items, rules or run state can change it.
This offspring preview does not predict IVs, moves, probability, capacity or whether an egg will
be generated. The compatibility and offspring callbacks see `preview` set in
their context and must remain pure; they should use the same rules in preview
and actual generation if they want their results to agree. Recursive previews,
missing or invalid parents, inactive runs and undersized outputs fail without
modifying the output. The existing daycare snapshot remains unmodified.

Version 1.3 adds `offer`, `accept` and `accept_context`. `offer` copies the
current pending flag, party count, parent snapshots, effective compatibility
and offspring preview without creating an egg or consuming gameplay RNG. It
supplies a short-lived fingerprint of the run, player identity, daycare and
party state. Pass that offer to `accept` while the normal overworld is idle;
a changed fingerprint or resolved preview returns `STALE`.
The fingerprint is not an egg ID or reservation, and a later identical state
can produce the same value; when a pending egg ID is available it is included
in the fingerprint. Parent or party changes, including daycare step
changes, require a fresh offer.

Acceptance rejects unavailable contexts, no pending egg, a full party and
incompatible parents before generation. `RHH_CAN_ACCEPT_DAYCARE_EGG`
constraint hooks may deny it; `accept_context` copies the current proposal
only during that hook, and recursive commands are rejected. After permission,
the host rechecks the state and uses the engine's normal egg-generation path.
Acceptance and pending-request constraints must not consume either gameplay RNG
stream. The host restores both streams if they do, rejects an allowed request
as stale, and preserves a denial.
The existing inheritance hooks and `RHH_BREED_EGG_CREATED` notification run
once, after the egg is placed and the pending flag is cleared. The result
reports the party slot and a copied egg snapshot when available. An OK status
means the engine committed the egg even if its snapshot could not be copied.
The command does not create a pending egg, spend items, predict the egg or
assign it a durable identity.

Version 1.4 adds `iv_preview`, which accepts a current offer and copies a
six-stat `RhhBreedingIvPreview` without rolling an egg or consuming gameplay
RNG. Each 32-bit mask is a conservative set of possible values, not a
probability or a guarantee that every included value can occur. The engine's
held-item rules determine its inherited-IV count and any guaranteed power-item
stat. Two power items for the same stat yield the two parent values; power
items for different stats cannot guarantee either stat individually. Destiny
Knot changes the engine's inherited count from three to five. Other masks can
remain 0–31 even when additional engine conditions might narrow the result.

If any `RHH_BREED_INHERIT_IV` hook is registered, the preview sets
`RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE` and treats its results as uncertain:
hooks can depend on the staged egg and its random IVs. A later pure IV rule
can still guarantee a parent value or select an initial random IV for a stat.
The count includes any count proposal, before per-stat hooks. A stale offer,
missing or incompatible parents, unavailable run or undersized output fails
without modifying the output. The preview does not reserve an egg or describe
probabilities and correlations between stats.

Version 1.5 adds `iv_odds` for a current offer. It returns a marginal weight
for each value of each stat and a shared denominator; divide a weight by that
denominator to obtain its modeled chance. The model assumes independent,
uniform engine draws and follows the current three/five inherited stats and
power-item selection rules, including two power items. It is not a prediction
of the next egg, a distribution of complete six-stat combinations, or a claim
about correlations in the deterministic game RNG. If an older staged-egg
`RHH_BREED_INHERIT_IV` hook is registered,
`RHH_BREED_IV_PREVIEW_HOOKS_ACTIVE` is set and the denominator is zero,
meaning the weights are unavailable. Stale offers and invalid output
buffers fail without changing the output.

Version 1.6 adds `iv_joint_odds`. Supply a bitmask of acceptable values for
each stat; the result is the modeled probability that all six masks match the
same egg. It accounts for the shared choice of inherited stats and parents,
including power items and Destiny Knot, rather than multiplying marginal
probabilities. The numerator and denominator are exact for the documented
independent-uniform model; they do not predict the deterministic next egg.
Active staged-egg inheritance hooks make the result unavailable, with the same flag and
zero denominator as `iv_odds`. Invalid requests, stale offers and undersized
outputs fail without modifying the output.

Version 1.7 adds `RHH_BREED_IV_COUNT` and `iv_count_context`. A pure transform
can choose zero through six inherited stats from copied parent information;
constraints may deny the proposal, and an invalid or denied value keeps the
engine's held-item-derived count. This same rule feeds egg generation and IV
previews. Power items force one of the selected stats when the count is at
least one; at zero they force none. The engine keeps both inheritance RNG
draws, with a six-bit parent draw only when six stats are selected. The usual
three/five-stat path retains its original draws. Per-stat inheritance hooks
still run afterward and make probability previews unavailable. A preview
does not reserve the rule against later mod-setting changes. Offer and pair
preview queries are unavailable within the count callback to prevent recursive
evaluation.

Version 1.8 adds `move_preview` for a current offer. It copies the
four initial moves for each possible offspring, using the same moveset builder
as egg creation, the selected parent roles and held items, and current modded
learnsets, followed by any pure move rules described below. Moves are content
keys; zero represents an empty slot. The query
does not roll an egg or consume gameplay RNG. A stale offer, invalid output,
unavailable pair or nested breeding preview fails without changing the output.
If a `RHH_BREED_INHERIT_MOVE` hook is registered, the result sets
`RHH_BREED_MOVE_PREVIEW_HOOKS_ACTIVE`: the displayed list can change
when the real egg is created, since those hooks are not run on a synthetic egg.

Version 1.9 adds `nature_odds` for a current breeding offer. It returns the
modeled chance of each nature and separate weights for inheriting from parent
0, parent 1, or choosing a random nature. The model follows the engine's
configured generation: Gen III's eligible mother/Ditto and Everstone rule,
Gen IV's ordered 50% checks, and Gen V onward's guaranteed inheritance and
two-holder choice. It uses the parents' true natures, not mint stat natures.
Weights assume independent, uniform draws and do not predict the next RNG
result. Identical parent natures combine into one outcome. Missing or
incompatible parents, stale offers and undersized outputs fail without
changing the output; the query does not create an egg or consume RNG. The
offer fingerprint includes the configured nature-inheritance rule.

Version 1.10 adds the pure `RHH_BREED_NATURE` proposal and `nature_context`.
After the engine chooses an inherited nature or random nature, transforms may
replace it with a `rhh:nature/...` key or zero for random; constraints may
deny, and a denial or invalid key preserves the engine's choice. The context
copies the original choice, generation and parents. The engine still performs
its ordinary inheritance draws, then creates the egg with the resolved nature.
The odds query applies the same proposal to each possible engine choice, so
its nature weights include the rule; source weights continue to describe the
engine's choices before the rule. Callbacks must be pure, deterministic and
use the same rule in previews and generation. Recursive breeding queries from
the callback fail. This changes the egg being created; it does not edit the
true nature of an existing Pokémon. An offer does not reserve module settings;
refresh its odds after changing a rule.

Version 1.11 adds the pure `RHH_BREED_IV_RULE` proposal and
`iv_rule_context`. For each stat, a transform can keep the previous result,
choose either daycare parent's IV, or restore the egg's initial random IV.
The rule runs after the engine and the older staged-egg inheritance hook;
denial or an invalid source keeps that earlier result. Constraints cannot
rewrite the source. The context contains the stat and copied parents, and
callbacks must be deterministic from that context, the current pending ID and
stable settings; they must not depend on a staged egg, gameplay RNG or whether
a preview is running. Nested
breeding previews are unavailable during the callback. The engine still
performs its ordinary draws and item rules before the override.

IV masks and the independent-uniform marginal and joint odds apply these
pure rules, including an initial-IV rule that overrides a power-item guarantee.
The joint calculation retains the shared engine selection for other stats.
An older `RHH_BREED_INHERIT_IV` hook still makes odds unavailable; pure rules
can narrow masks for stats they override even when that older hook is present.
Changing a rule does not reserve an offer or the next egg, so refresh previews
after changing settings. Failed queries leave their outputs unchanged.

Version 1.12 adds the pure `RHH_BREED_MOVE_RULE` proposal and
`move_rule_context`. The context copies the parents, offspring candidate,
parent roles and complete engine moveset. Each slot starts with its baseline
move; a transform can select zero to remove it or a move known by either
parent and learnable by the offspring. Constraints can deny a slot but cannot
rewrite it. Proposals are staged together: an unknown or ineligible move,
duplicate, or empty moveset retains the complete baseline. Accepted moves are
compacted in order with normal starting PP. The same pure rules run in the
candidate preview and in egg creation before any older staged-egg move hooks.
Those older hooks still set the uncertainty flag because they can change the
final list. Pure callbacks must use the copied context, current pending ID and
stable settings, must not consume gameplay RNG or inspect the staged egg, and cannot recursively run
breeding previews. An offer does not reserve settings; refresh after changes.

Version 1.13 adds `request_pending` and `request_context`. A mod can request
an engine pending egg using a current offer for two compatible parents. This
deliberately bypasses the usual step-based chance roll; it neither rolls nor
reserves the eventual egg, and it does not require an empty party slot. For
parents whose engine offspring has two species variants, the pending
transition draws and stores the variant bit once; ordinary pairs do not
consume an additional random draw. Repeated pending triggers do not reroll it.
`RHH_CAN_REQUEST_DAYCARE_EGG` constraint hooks can deny the request and inspect
the copied pre-request offer through `request_context`. The host rechecks the
offer, field state and RNG after permission, then sets the ordinary pending
flag and sends `RHH_DAYCARE_EGG_REQUESTED`; its observer context reports the
checked pre-request offer and success. This notification covers this command,
not eggs naturally produced by walking. Failed or stale requests do not set
the flag. The existing `accept` command or daycare flow creates the egg later,
using then-current breeding rules. Requests have no built-in cost and do not
provide a durable egg identity or an exactly-once receipt across save rollback.

Version 1.14 adds `RHH_DAYCARE_PENDING_CHANGED` and `pending_context`. This
observer sees actual pending-flag transitions from walking, the engine's
explicit trigger, `request_pending`, rejection and successful acceptance.
The cause and before/after flag values are copied with the run ID and each
decodable parent; `parent_mask` marks the available parent records. Repeated
sets or clears do not send transitions. Acceptance is reported after the egg
is placed and the existing egg-created notification completes. The context is
available only during this event; no compatibility preview or RNG roll is
performed to build it. Direct flag writes and save/load restoration are not
notifications, and neither the transition nor its parent identities are a
durable or rollback-proof egg identity.

Version 1.15 adds `pending_id` and `pending_change_id`. A pending egg receives
a run-scoped, OS-random 128-bit ID without consuming gameplay RNG. The ID is
stored with the normal save generation: loading that generation restores the
same ID, while a newly pending egg receives a different ID with overwhelming
probability. A pending egg from an older save is assigned an ID on load or
before its next save. Before a save commits, the ID is only in memory; if OS
entropy is unavailable, the query fails without modifying its output and the
normal daycare flow continues. The transition ID is also available during
`RHH_DAYCARE_PENDING_CHANGED` and `RHH_BREED_EGG_CREATED`, allowing an observer
to associate acceptance with the pending event. It identifies the pending event,
not the resulting owned Pokémon. The stored variant bit fixes the engine's
base variant while the egg is pending; changed parents or breeding rules can
still change the final species. The ID does not reserve IVs, moves, nature or
the egg-generation RNG outcome. Independent copies of a save retain the same ID.

When the daycare ordinal sequence is disabled, saves containing a pending ID
use host-run schema 5; older hosts reject that retained save generation instead
of silently discarding the ID. Without a pending ID or ordinal sequence, the
host writes its previous schema 3 or 4 as appropriate.

Version 1.16 adds `pending_random_at`, a read-only stream keyed by the current
pending ID, a registered module's ID and an explicit 64-bit ordinal. It also
accepts that ID during its pending-transition and egg-created notifications.
A stale ID, an unavailable pending ID or an unregistered namespace fails
without changing the output. The fixed calculation is
`random_at(random_at(run_seed, "rhh:breeding_pending", id.run_id, id.high),
namespace_id, id.low, ordinal)`. It does not advance either gameplay RNG.
Pure breeding rules may use this stream along with their copied context and
stable settings, and must fall back when no egg is pending. This does not
reserve or predict the engine's own random egg generation.

Version 1.26 adds `gender_odds` for a checked breeding offer. For each current
offspring candidate it reports male, female and genderless weights out of one
billion. The model applies current parent nature-source weights and pure
nature rules, then counts personality values compatible with a fixed nature;
each source and the combined weights are rounded to billionths. It assumes independent uniform
draws and does not predict a particular egg or correlations with other traits.
The offspring-rule flag warns that a compatibility or species proposal may
change between preview and generation. Stale offers, unavailable pairs and
invalid outputs fail without changing the output or either gameplay RNG stream.

Version 1.27 adds `nature_gender_odds` for a checked offer. It returns joint
male, female and genderless weights for each nature, separately for each
offspring candidate. Each candidate's weights sum to the returned 64-bit
denominator. The nature-source model is the same as `nature_odds`; within a
nature, gender is calculated from compatible 32-bit personality values and
rounded to billionths. Random-nature outcomes use the uniform-nature model, so
the result is an estimate rather than exact finite-RNG probabilities. Pure
nature rules participate; the offspring-rule flag warns that candidates may
change. It does not model correlations with other egg traits or reserve an
egg. Stale or unavailable offers leave output and gameplay RNG unchanged.

Version 1.28 preserves both gameplay RNG streams and the last-used ability
around `RHH_DAYCARE_PENDING_CHANGED`, `RHH_DAYCARE_EGG_REQUESTED` and
`RHH_BREED_EGG_CREATED` observers. Normal egg generation and pending-variant
selection still consume their usual engine RNG; only observer-side changes are
discarded. Notifications remain transient and follow save rollback.

Version 1.25 adds an opt-in sequence for eggs placed by the ordinary player
daycare. `ordinal_state` copies whether it is enabled and the next ordinal;
`enable_ordinal` requires the current run ID and a safe command context. It is
idempotent and starts the sequence at 1 when first enabled. This boundary does
not count eggs made before activation. Previewing, requesting or rejecting an
egg does not advance it; each actual daycare placement advances it once, even
if no observer is installed. During `RHH_BREED_EGG_CREATED`,
`egg_ordinal_context` copies the committed ordinal and party slot when
available. Other egg-creation paths do not participate.

The counter is stored with the normal save generation. It persists after a
mod is removed and rewinds on rollback, so divergent copies may reuse the same
ordinal; it is not a permanent individual ID or an exactly-once receipt.
Unsaved changes can be lost. If the counter reaches its `UINT64_MAX` exhausted
sentinel, normal daycare creation continues without an ordinal. Enabling it
uses host-run schema 6, which older hosts reject; runs that have not enabled it
continue to project to the existing compatible schema. Invalid or stale enable
requests leave the counter and output unchanged.

Version 1.17 adds `rng_policy`, `rng_policy_context` and the pure
`RHH_BREED_RNG_POLICY` proposal. The default is `RHH_BREED_RNG_ENGINE`, which
keeps the normal game RNG behavior. A transform may choose
`RHH_BREED_RNG_PENDING`; constraints may deny it, and a denied or invalid mode
falls back to the engine mode. `rng_policy` checks a current pending offer and
copies the resolved policy, pending ID and parents without advancing either
game RNG. The callback context is available only during the proposal and must
be treated as read-only and deterministic.

In pending mode, the engine uses temporary RNG states derived from the pending
ID for egg creation and inheritance, then restores both surrounding game RNG
states before the committed egg notification. The seed is
`random_at(run_seed, "rhh:breeding_engine", id.run_id, id.high)`; the low
32 bits of `random_at(seed, "rhh:breeding_engine", id.low, 0)` and the same
call with ordinal 1 seed the two engine RNG streams. With the same saved pending ID,
parents, trainer, game context and mod rules, the generated egg repeats even if
the surrounding RNG has advanced. The policy does not freeze those inputs,
save a resolved egg, or provide a permanent identity for the resulting Pokémon.
If the ID or policy is unavailable, ordinary engine RNG is used.

Version 1.18 adds `exact_preview` for a current pending offer whose resolved
RNG policy is pending mode. It uses the same construction and pure inheritance
rules as acceptance and returns a staged egg snapshot, pending ID, offspring
and ability keys, moves, ball, ability slot, gender and shiny status. Ball and
ability-slot values are engine ordinals. The query restores both game RNG
streams and the engine's temporary battle values; it does not place an egg,
clear the pending flag or send a committed egg event.

The preview is available outside battle from an ordinary call or mod update.
It fails without changing its output if the offer is stale, the repeatable
policy is unavailable or denied, or either older staged-IV or staged-move
hook is registered. Those hooks may depend on the staged egg and cannot safely
be replayed as a pure preview. The result is not a reservation: changes to
parents, trainer, game context or mod rules can change the accepted egg. Pure
breeding callbacks must apply the same rules in preview and generation.

Version 1.24 adds `accept_exact(offer, preview, result)`. It runs the normal
acceptance constraints, rechecks the offer and preview, and compares the
previewed fields against the actual staged egg before placing it in the party.
If rules or inputs change the previewed result, or the RNG policy stops being
repeatable, it returns `RHH_BREED_ACCEPT_STALE`
without consuming the pending egg or changing the party; a rejected staged
generation restores both gameplay RNG streams and temporary battle values.
It requires the full current preview structure and only promises the fields
that preview exposes. `accept` retains its existing behavior.

Version 1.23 adds `offspring_odds` for a checked breeding offer. It combines
identical species in the current preview and reports relative weights: the
engine's two unselected counterpart variants are modeled as equally likely,
while a pending egg uses its stored variant. The result is an estimate, not a
reservation. The rule flag identifies installed compatibility or offspring
hooks, which may distinguish preview from generation or change with pending
state; changed parents and settings can also change the result. Queries leave
both gameplay RNG streams unchanged, reject stale offers and leave the output
untouched on failure.

Version 1.22 adds `ability_odds` for a checked breeding offer. It reports the
generation-specific parent, inherited slot and chance, then models each
offspring's final ability slots after pure ability rules. Ability keys use the
engine's fallback for empty slots; entries retain distinct slot numbers even
when they resolve to the same ability. The model assumes independent uniform
draws and even personality parity; it does not predict the next egg or its
correlations with nature and other traits. As with ball odds, changing the
corresponding inheritance generation invalidates an existing offer.

Version 1.21 adds `ball_odds` for a checked breeding offer. It returns the
generation-specific source weights and conditional ball probabilities for
each possible offspring, combining matching outcomes after pure ball rules.
The model uses uniform draws; it does not predict the next egg, weight the
offspring candidates, or describe correlations with other traits. It restores
gameplay RNG, rejects stale offers and leaves the output untouched on failure.
Changes to the configured ball-inheritance generation also invalidate offers.

Version 1.20 adds `ball_item` to the exact preview: it is the stable item key
for the inherited ball, or zero if no item key is available. The original
`ball` ordinal remains available. Set `struct_size` to the structure size;
older callers with `RHH_BREEDING_EXACT_PREVIEW_V1_18_SIZE` receive the original
prefix without writes beyond it. Callers using a v1.18 or v1.19 host can check
the returned `struct_size` before reading the added field.

Version 1.19 adds pure `RHH_BREED_BALL_RULE` and `RHH_BREED_ABILITY_RULE`
proposals after ordinary egg inheritance. `trait_context` copies the parents,
their roles, compatibility, offspring species and the engine's result during
either callback. The ball proposal uses a stable item key; it must resolve to
an eligible Poké Ball other than a Master, Cherish or Strange Ball. The ability
proposal uses a slot ordinal from 0 to 2, and a changed slot must exist on the
offspring. Neither rule consumes an item. Constraints may deny a proposal but
cannot rewrite it. An invalid, denied or RNG-consuming proposal retains the
engine result, and both gameplay RNG streams are restored. Callbacks must be
pure and give the same result for preview and generation. These rules also run
in `exact_preview`; ordinary pair previews do not calculate ball or ability
probabilities.

`RHH_BREED_COMPATIBILITY` is a pure proposal: the initial decision value is
the unmodified `RhhDaycareCompatibility` category. Transforms can choose any of
the four categories, including allowing an otherwise incompatible pair;
constraints can deny it. Denial means incompatible and an out-of-range value
falls back to the engine score. This hook runs when the engine checks the pair,
including the daycare display and egg-generation check, and may run repeatedly;
it must not consume gameplay RNG or mutate state. The existing daycare
snapshot intentionally continues to expose the unmodified score.
The host restores both gameplay RNG streams around compatibility and offspring
proposals. A proposal that advances either stream falls back to the engine
value; a compatibility denial remains a denial.

Once the engine has selected parent roles and applied its usual species and
incense rules, `RHH_BREED_OFFSPRING` offers that species key for transformation.
The final key must resolve to a currently creatable species; an invalid or
denied proposal falls back to the original species. The engine still handles
nature, initial IVs, default inheritance, Poké Ball and ability inheritance. For newly permitted
pairs without a conventional mother, the default inheritance roles are
distinct slots; when neither parent can supply an ordinary ball or ability,
slot zero is used as a deterministic fallback. `RHH_BREED_EGG_CREATED` is an
observer notification after the egg enters the party and the pending flag is
cleared. A full party leaves the pending egg and party unchanged and sends no
notification. Proposals run only for the active run's live daycare or its
explicit service preview, not an unrelated daycare; reentrant proposals use
the engine default.
The host has no persistent egg recipe, durable egg RNG ordinal or durable
hatch tracking yet.

`rhh:service/egg_hatch` version 1.1 exposes a copied
`RhhEggHatchContext` during `RHH_EGG_HATCHED` observer notifications. The
event applies to any valid party egg, including gifts and traded eggs, after
the engine has cleared its egg flag, assigned its hatch data, restored PP and
recalculated stats. It occurs before the optional nickname prompt. The context
contains the pre-hatch egg snapshot, the committed party snapshot and the
current map key when available. The engine assigns the current player's OT on
hatching, so the two OT-ID/personality identities can differ; both are still
potentially ambiguous rather than durable individual IDs. `context` fails
outside the notification and on undersized output without modifying it.
Replaying an older save can produce the event again; no durable hatch journal
or exactly-once delivery across save rollback is provided.
Version 1.1 also restores both gameplay RNG streams and the last-used ability
after hatch observers.

After the engine's IV inheritance, `RHH_BREED_INHERIT_IV` runs once per stat
with `ordinal` equal to `RhhMonStat`, `args[0]` the initially generated IV and
`args[1]` the engine's inherited result. Its decision defaults to
`RHH_BREED_IV_ENGINE`; a mod can choose `RHH_BREED_IV_PARENT_0`,
`RHH_BREED_IV_PARENT_1`, or `RHH_BREED_IV_INITIAL`. Invalid values and denied
decisions retain the engine result. Each callback sees the same pre-override
snapshot, so a rule can choose an exact number of inherited stats using the
egg identity, stat ordinal and `random_at` without consuming gameplay RNG.
The engine's normal random draws still occur; choosing a parent can override
the result of an inheritance item. Constraints may impose stricter policy.

After the engine builds the egg moveset, `RHH_BREED_INHERIT_MOVE` runs once
for each of four slots. The initial decision is that slot's engine move key;
return zero to remove it or a different key to select a move known by either
parent. A new move must also occur in the offspring's egg, level-up or
teachable learnset. The four proposals are staged together: an unknown,
ineligible or duplicate move, or an empty proposed moveset, discards the whole proposed moveset and keeps
the engine's; a denied individual proposal retains that slot's engine move.
Accepted moves are compacted in order with normal starting PP and no PP Ups.
All move callbacks see the same pre-override moves and IVs, and `ordinal` /
`args[0]` identify the slot. The notification reports the final values.

`rhh:service/mon_edit` version 1.0 offers `preview` and `commit` for party
and PC Pokémon. The request carries a fresh `RhhOwnedMonSnapshot`, a field
mask, and complete six-value IV and/or EV allocations and/or a stat nature.
All selected values are validated together against the engine's storage and
EV limits. The host rejects eggs, ambiguous identities (including daycare
clones), stale snapshots, invalid values, empty changes and edits in battle.
The snapshot comparison covers the fields in that view; if unrelated data such
as current HP changes, a subsequent preview or commit uses the current value.
Preview returns six calculated stats and current HP without changing the
Pokémon, RNG or battle scripting state. Hyper-training is retained and affects
the calculated stats. The original edit operations do not change it.

Before a preview or commit, `RHH_CAN_EDIT_MON` constraint hooks receive the
actual changed-field mask in `args[0]` and a preview flag in `args[1]`.
During that callback, `mon_edit->context` copies the typed before/after
proposal and calculated stats. Constraints accumulate denials in normal hook
order. A successful commit replaces the staged Pokémon once and emits one
`RHH_MON_EDITED` observer notification with the same context; a preview,
rejected request or stale request emits no notification. Fainted Pokémon
remain fainted, including when the max HP of a boxed Pokémon changes. Party
stats are recalculated and the stat nature is a mint-like modifier: the actual
personality-derived nature, personality, shiny status and identity do not
change. Edits are part of ordinary game state and are persisted on the next
normal save.

Version 1.8 preserves both gameplay RNG streams throughout all edit variants,
including committed observers. An otherwise allowed validation that consumes
either stream is denied; an explicit constraint denial keeps its reason. This
also applies to paid, recipe, Hyper Training, true-nature and Tera edits. Mod
callbacks should use deterministic `random_at` if they need a stable choice.

Version 1.1 appends `preview_paid`, `commit_paid` and `payment_context` to the
service. A paid request can consume one item type, money, or both, alongside
the same atomic IV, EV and stat-nature edit. The receipt reports the item and
money before and after, and whether the active inventory is the normal bag or
the Pyramid bag. Items in the PC and held items are not payment sources.
Insufficient funds, invalid items, a veto, stale Pokémon or a changed payment
inventory reject the whole edit without charging it. Preview changes neither
the Pokémon nor payment. Payment is committed before the single
`RHH_MON_EDITED` notification; `payment_context` supplies the receipt during
that notification and during `RHH_CAN_EDIT_MON`. It fails for unpaid edits and
outside those callbacks. Before a paid commit reaches constraints, the host
synchronizes any already-pending Poké Ball inventory change; that notification
can therefore occur even if the edit is subsequently vetoed. It synchronizes
again after payment, including when the transaction consumes the last ball.

The host does not set prices: a pricing mod can inspect `payment_context` in
its edit constraint and deny unpaid or underpriced requests, including calls to
the original v1.0 operations. Check service version 1.1 and
`RHH_MON_EDIT_SERVICE_V1_1_SIZE` before using the appended functions; older
modules retain the v1.0 prefix. Run `python3 native/tests/mon_edit_cost.py` for
real party and box edits, both bags, secret-base bookkeeping, Nuzlocke, stale
and denied transactions, independent v1.0/v1.1 modules, and fresh-process save
restoration. The script removes its test builds.
The service does not provide a save transaction, earned-EV accounting, a
true-nature editor or a menu.

`rhh:service/ev_limits` version 1.0 exposes the effective edit limits for an
unchanged party or PC snapshot through `query`. `RHH_EV_LIMITS` transforms
may replace the total in `decision.value` and the six individual limits in
`decision.stats`. Constraints can lower those values, but cannot raise an
earlier proposal. Denial, a per-stat limit above 255, or a total above 1530
falls back to the engine defaults. `context` supplies the recipient, source
and item key during the callback. Hooks must be pure and consistent.

These limits are used by the mon editor, ordinary player-party battle EV
gains and EV-changing items. A battle or item query starts from that path's
engine total cap, so a policy that raises it deliberately overrides that cap;
older-generation vitamin limits still apply. Link, recorded, facility and
other excluded battles, AI item simulations and non-party engine targets use
their ordinary rules. The legacy `max_stat_ev` and `max_total_ev` snapshot
fields remain the engine defaults; use `ev_limits->query` for policy-adjusted
edit limits. Existing EVs are not automatically rewritten when a policy changes.
If a Pokémon is already above a newly lowered cap, later battle gains follow
the engine's existing over-cap adjustment behavior. Removing a policy that
allowed values above the default caps can therefore affect later gains.
The storage fields are still bytes; limits beyond 255 per stat require a save
representation change. Run `python3 native/tests/ev_limits.py` for editing,
battle, item, constraint and fallback checks; `--target windows64` uses Wine.
The runner cleans its temporary builds.

Version 1.1 makes limit queries deterministic with respect to gameplay RNG.
The host restores both streams after `RHH_EV_LIMITS`; if any callback advances
either stream, the proposed limits are discarded and the engine defaults are
used. Modules needing repeatable variation can use the host's keyed random
service instead of consuming gameplay RNG.

`rhh:service/battle_ev_yield` version 1.0 exposes a battle EV transaction for
the player's party. Its `context` copies the recipient snapshot, defeated
species, all six pre-gain values, the held-item- and Pokérus-adjusted yield
before caps, and the engine's proposed capped allocation. A
`RHH_CAN_APPLY_BATTLE_EVS` constraint can deny that entire allocation;
`RHH_BATTLE_EVS_PROCESSED` then reports whether it was accepted and the
actual EV values; the copied Pokémon snapshot is from before the allocation.
Battle stat recalculation remains on the engine's normal schedule. The
notification also occurs when caps leave no room, so a
policy can record earned points separately. `applied` denotes acceptance, even
when the allocation changed no EVs. The proposed allocation follows the
engine's existing behavior for pre-existing over-cap EVs.

The host preserves the ordinary path when neither event has subscribers. It
does not invoke this policy for link, recorded, facility, tutorial or other
excluded battles, non-party Pokémon, vitamins or EV-reducing items. It also
does not create or spend an EV bank. The copied `identity_unique` flag is true
only when a full owned-storage query found one valid match; OT/personality is
not a permanent ID, so a mod must handle collisions and later identity changes.
The engine decides whether a recipient earns EVs, including at maximum level;
the hook does not create additional battle rewards. Run `python3
native/tests/battle_ev.py` to check the normal and denied paths, caps, held
items, Pokérus, duplicate identities and state restoration; use
`--target windows64` for Wine. The runner cleans its temporary builds.

Version 1.1 adds `RHH_BATTLE_EV_PROPOSAL` and `policy_context`. Proposal
transforms select APPLY or BANK through `decision.value`; constraints may deny
the proposal but cannot rewrite its mode. Invalid modes fall back to APPLY.
BANK leaves EVs unchanged and requires a processed-event subscriber; without
one the host falls back to APPLY. The existing `RHH_CAN_APPLY_BATTLE_EVS`
constraints still run after mode selection and can deny either mode. During the
mode proposal, transforms inspect the current `decision.value`; the policy
context reports the initial APPLY mode until selection is complete. During
validation, `policy_context` exposes the selected mode. In the processed event, its
`accepted` field distinguishes a successful BANK from a denied reward. A
banking observer should record the uncapped yield only when `accepted` is true
and the mode is BANK. As in v1.0, the processed decision's `allowed` and the
original context's `applied` indicate direct EV application; both are false
for an accepted BANK. The host does not itself store banked points.
Version 1.2 restores both gameplay RNG streams after each proposal, constraint
and processed-event dispatch. If a proposal or constraint consumes RNG, that
dispatch's changes are discarded while any denial remains in force. A pure
BANK proposal and independent constraints still compose normally.

`rhh:service/item_ev_yield` version 1.0 exposes positive, single-stat EV
items used on a player's party Pokémon from the field party menu. During
`RHH_ITEM_EV_PROPOSAL`, `context` supplies the item, Pokémon snapshot, raw
item yield and the proposed allocation after the active EV limits. Transform
hooks may select `RHH_ITEM_EV_APPLY` or `RHH_ITEM_EV_BANK`; constraints may
deny use. Invalid choices fall back to APPLY, and constraints cannot change
the selected mode. APPLY follows the normal item caps and fails without
consuming an item if no EVs can be added. BANK can succeed at the cap and
leaves the Pokémon's EVs unchanged; it requires a processed-event subscriber
that is responsible for recording the points. Both successful modes consume
one item and apply its usual friendship effect. `RHH_ITEM_EV_PROCESSED` is an
observer-only notification after these changes, with the actual EV values and
selected mode; rejected uses do not emit it. Banked use displays a distinct
message in the party menu.

The service does not itself store or spend a bank, guarantee durable
exactly-once accounting, or establish permanent Pokémon identities. Its
`identity_unique` flag only describes the current owned-storage query. Battle
use, simulations and non-party targets follow their existing paths. With no
item EV hooks, ordinary item behavior is unchanged.
Run `python3 native/tests/item_ev_disposable.py` to exercise item policies and
party callbacks using an isolated build that is removed afterward.

Version 1.1 adds `reduction_context` for EV-reducing berries and Fresh-Start
Mochi used on the player's party from the field party menu. The copied context
contains the item, recipient, affected-stat mask and before/proposed/after EVs.
The signed `change` is negative for a berry and zero for a reset; the proposal
reflects the engine's generation-dependent berry behavior. Constraints on
`RHH_CAN_REDUCE_ITEM_EVS` can deny the use before the item is consumed. On
success the engine applies its normal item effect and friendship rules, and
`RHH_ITEM_EV_REDUCTION_PROCESSED` reports the actual values. A successful
friendship-only berry use is reported with identical before and after EVs;
if neither EVs nor friendship can change, the item is not consumed. The field
party menu's dedicated berry and reset-item callbacks use this transaction as
well as the medicine callback. Failed and denied uses produce no processed event. As with positive item EVs,
identity uniqueness is a current census, not a permanent ID. Battle use,
simulations, non-party targets and HP-EV items on Pokémon with Shedinja-style
HP handling continue to use their normal paths.

Version 1.2 applies the same RNG isolation to item-EV proposals, reduction
constraints and their processed observers. Impure proposal changes are
discarded, but a denial remains effective; gameplay RNG used by the engine's
actual item effect is unaffected.

Version 1.3 adds `preview` for a checked current party snapshot and item key.
It runs the same supported gain or reduction proposal and constraints, reports
whether use would apply, bank, be denied, or fall through to the engine, and
copies the proposed EVs. It consumes no item and emits no processed event.
The forecast uses current inventory, rules and Pokémon data; it does not
reserve them and excludes friendship and other non-EV effects. An unhandled
result does not mean the engine cannot use the item. Stale snapshots, invalid
items and unavailable contexts fail without changing the output. Proposal
hooks must be pure and stable across previews and actual use.

Version 1.2 adds `preview_recipe`, `commit_recipe` and `recipe_context` for
up to eight distinct item types plus money. A recipe is staged across every
affected normal-bag pocket, or the active Pyramid bag, and commits with the
Pokémon as one edit. Duplicate item keys, zero quantities, unavailable items,
insufficient resources, vetoes and stale inventory reject the entire recipe.
The copied receipt lists the before and after quantity for every item and the
money. As with v1.1, prices are set by mods rather than the host.

`recipe_context` is available in edit constraints and notifications for both
v1.2 recipes and v1.1 paid edits. The older `payment_context` can describe a
v1.2 recipe containing zero or one item; it returns unavailable for multiple
items rather than exposing an incomplete price. An existing pricing rule that
denies when its payment context is unavailable therefore rejects multi-item
recipes until updated. Pricing mods can use the v1.2 context to inspect every
line and decide how recipes compose. The v1.0 and v1.1 service prefixes remain
available. `native/tests/mon_edit_cost.py` exercises normal and Pyramid bags,
mixed pockets, constraints, old module layouts and fresh-process saves, and
cleans its test builds.

Version 1.3 adds `commit_previewed`, `commit_paid_previewed` and
`commit_recipe_previewed`. Supply the corresponding successful preview to
require the newly staged before/after snapshots, six calculated stats and
current HP to match it. Paid calls also require the displayed payment and
balances to match. A mismatch returns `RHH_EDIT_STALE` before constraints or
payment; a matching outcome still goes through current constraints and the
normal atomic commit. This protects the displayed result, not every unshown
Pokémon field, and the preview is not a durable or cryptographic receipt.

Version 1.4 adds `preview_hyper_training`, `commit_hyper_training` and
`hyper_training_context`. Training is a separate six-bit allocation, indexed
by `RhhMonStat`; the request includes the previously sampled training mask and
can also change IVs, EVs or stat nature in the same transaction. It can charge
an optional v1.2 recipe. Setting or clearing flags is allowed; mods can impose
eligibility and cost rules through the existing edit constraints. The context
copies both masks during those constraints and the committed notification,
whose changed-field mask includes `RHH_EDIT_HYPER_TRAINING`. A commit requires
the matching successful preview and rechecks the current training mask,
calculated stats, HP and payment. Inherited IVs are not changed unless an IV
edit is requested, and a fainted Pokémon remains fainted. Like other edits,
this applies to unique, non-egg party and PC Pokémon outside battle; it does not
apply the game's ordinary Hyper Training item or level requirements implicitly.

Version 1.5 adds `preview_true_nature`, `commit_true_nature` and
`true_nature_context`. Requests use a stable nature key and can combine the
change with IV, EV, stat-nature and optional recipe edits. The stat policy can
follow the new nature, preserve the current stat nature, or follow only when
true and stat nature previously matched. An explicit stat-nature field takes
precedence. The checked preview shows both natures, resulting stats, HP, costs
and whether the save format must be activated. A commit rechecks the preview,
storage, link state and format preflight, then activates the format and commits
the Pokémon and payment together. Constraints and the committed notification
use `RHH_EDIT_TRUE_NATURE`; the context identifies the policy and activation.
Personality, shiny status and OT identity are preserved. The marker persists
even if later edits restore the personality-derived nature, so older hosts
reject the affected save generation. Native link sessions are refused while
the format is active until peer negotiation is implemented.

Version 1.6 adds `preview_nature_training` and `commit_nature_training` to
quote and commit true nature, Hyper Training, ordinary stat edits and an
optional recipe as one transaction. Both nature and training contexts are
available during constraints and notifications. The matching preview, masks,
format activation, stats and payment are rechecked before commit.
On older hosts without the combined service, nature and training drafts must
be saved separately.

Version 1.7 adds `preview_tera`, `commit_tera` and `tera_context`. A request
contains the sampled owned Tera view, a built-in type key (or zero to restore
the personality-derived value), optional IV, EV and stat-nature edits, and an
optional recipe. A commit requires the matching successful preview and
rechecks the Tera value, Pokémon, identity uniqueness and payment after
constraints. `RHH_CAN_EDIT_MON` and `RHH_MON_EDITED` receive the
`RHH_EDIT_TERA` flag and can copy the Tera context; the normal edit and recipe
contexts are available as well. An otherwise valid Tera proposal that consumes
gameplay RNG is denied, and observer RNG use is restored. For species with a forced type, the
preview shows the unchanged effective type alongside the changed stored value.
The engine's existing Tera field cannot persist custom runtime types; those
keys are rejected. Edits apply to unique non-egg party or PC Pokémon outside
battle, link sessions and unsafe storage activity, using the existing save format.

`RHH_MOD_UPDATE` is a committed observer callback at a stable overworld frame
boundary after the game loop, when an active run is not in battle, a script or
a palette fade. Its ordinal is the current process frame, not a durable clock.
A mod may queue a request in its own state and call the edit service from this
callback. Calls from other hook dispatches and recursive edits are rejected;
presentation callbacks must not change the simulation. An update may be delayed
while menus, scripts, battles or transitions are active.

`rhh:service/ui` version 1.0 lets a trusted module open one modal panel from
`RHH_MOD_UPDATE`. Its `input` function samples the player's rebound logical
buttons in that event; opening requires a registered mod's content key and a
draw callback. The host copies the panel descriptor, while the module keeps
its callback code and user pointer alive until the close callback. A second
panel or a close request from a different owner is rejected. The panel is
closed with `RHH_UI_RESET` before a run changes, mod state resets, or module
libraries unload. Reopening during a close callback is rejected.

While the panel is open the overworld simulation is paused and audio continues.
Input callbacks receive held, newly pressed and repeated logical buttons;
held buttons are suppressed until released after closing. The draw callback
receives a 240 by 160 software canvas with clipped rectangles, lines, filled
polygons (3–32 vertices), and ASCII text in the game's normal font. Colors are
RGB555; drawing coordinates must be in the signed 16-bit range for lines,
polygons and text. Canvas functions are valid only during drawing. Drawing and
close callbacks are read-only: gameplay commands, including Pokémon edits and
closing the panel, are rejected there. Handle commands in input callbacks instead and query fresh
snapshots on each redraw. Canvas text currently supports basic ASCII. The paused
backdrop uses the standard software scene rather
than an optional world presenter.

Version 1.4 adds `keyboard_binding`, a read-only query for the current
keyboard labels assigned to one `RhhUiButton`. Initialize
`RhhUiKeyboardBinding.struct_size`; the result copies one or two key names,
including both Shift keys when both are assigned. Invalid buttons, undersized
outputs or unavailable labels leave the output unchanged. Query again after a
control change. Names describe physical SDL scancodes, are not localized
keyboard-layout labels or stable key identifiers, and do not describe gamepad
bindings. The current native input loop reads keyboard input only. Earlier
service sizes are unchanged.

Version 1.5 adds `image_rgba` to the draw canvas. It accepts borrowed RGBA8888
pixels, clips and scales them with nearest-neighbor sampling, and draws pixels
whose alpha meets the supplied threshold from 1 to 255. This is a binary
cutout, including on nonmodal overlays; partially transparent pixels are not
blended. Validate `RHH_UI_CANVAS_V1_5_SIZE` during drawing and do not retain
the canvas or image pointer. Invalid requests draw nothing. It can display
source artwork from content and object-visual services over existing UI.

`rhh:service/ui_accessibility` version 1.0 lets a modal draw callback publish
copied semantic text under its panel owner key. Supply a nonempty title, body
and optional current-focus text in the fixed-size fields of
`RhhUiAccessibleContent`; they must be terminated, valid UTF-8 without control
characters other than tabs and newlines. The host validates and copies them,
and keeps the previous snapshot if a publication is rejected. A pure
`snapshot` call copies the latest content and revision. An independent
adapter can observe `RHH_UI_ACCESSIBILITY_CHANGED` and query the snapshot,
even while the modal pauses normal `RHH_MOD_UPDATE` calls. Its ordinal is the
revision and `args[0]` reports whether content is active. Identical redraws
do not notify; multiple changes before dispatch may coalesce. Closing clears
the content and normally notifies; run resets and unloads clear it without
guaranteeing a closure event. Observer callbacks cannot issue game commands,
and gameplay RNG is restored. This is a bridge for an adapter, not a built-in
operating-system screen reader or speech engine; panels must explicitly
publish their own semantic text.

Version 1.1 extends the same publication rules to live overworld overlay draw
callbacks. `overlay_snapshot` and `RHH_UI_OVERLAY_ACCESSIBILITY_CHANGED` form a
separate stream with its own revision, so version 1.0 modal adapters continue
to see only modal content. Overlay closure and host-driven reconciliation
clear the overlay snapshot; run reset and unload do not promise a closure
notification. The interface remains a semantic bridge, not speech output.

Version 1.1 adds party-menu actions without changing the version 1.0 prefix
(`RHH_UI_SERVICE_V1_0_SIZE`). When the player selects a Pokémon in the normal
field party screen, `RHH_PARTY_MENU_ACTIONS` runs constraint hooks with the
party slot in `args[0]` and the existing OT/personality identity in `subject`.
`party_context` copies the selected snapshot for that callback; eligible mods
may call `offer_party_action` with their registered owner key, a namespaced
action ID, an ASCII label of at most 24 characters, an open callback and user
pointer. Collection is read-only and does not open a panel or commit changes.
The host copies the descriptor and strings, retains at most 32 actions and
sorts them by action ID; duplicate keys are rejected in deterministic hook
order. Callback code and the user pointer must remain valid until the menu is
closed or the host resets.

If any actions are offered, the Pokémon's action menu shows **Mods**. The
host presents a paged chooser and rechecks the copied Pokémon snapshot before
calling the chosen action's read-only open callback. The callback returns a
panel owned by that mod; input and draw then follow the same rules as any
other modal panel. A stale Pokémon or invalid panel leaves the chooser open
with a message. Closing the panel refreshes the party screen. This integration
is for the player's field party menu, not battle, link, daycare or special
selection screens.

Version 1.2 adds `image_rgb` to the canvas supplied to draw callbacks. It
accepts a bounded top-to-bottom RGB24 source with explicit dimensions, stride
and buffer size, and draws a clipped, nearest-neighbor scaled image in the
RGB555 panel. Invalid dimensions, stride or short buffers fail without drawing;
the source is borrowed only for the call. Request UI service 1.2 and check
`RHH_UI_CANVAS_V1_2_SIZE` before calling it. Older canvas consumers retain
their original prefix (`RHH_UI_CANVAS_V1_0_SIZE`).

Version 1.3 adds one live overworld overlay. `overlay_open` takes an owner,
input callback and draw callback during a stable `RHH_MOD_UPDATE`; a modal
panel or another overlay cannot be open at the same time. The world simulation
continues while the overlay owns the rebound buttons, which are suppressed
from the game until released after close. Its callbacks and user pointer remain
valid until `overlay_close` or the close notification. The host closes it on a
run change, battle, script, fade, locked controls or loss of the normal
overworld context. Its drawing is read-only and uses the same clipped canvas,
with untouched pixels transparent. Input callbacks can issue allowed commands.
The overlay is composited over the final world presentation in both software
and OpenGL, including custom world presenters; it is omitted from photo capture.
The service retains its version 1.2 prefix (`RHH_UI_SERVICE_V1_2_SIZE`).

`rhh:service/nature` version 1.0 enumerates the engine's natures and their
per-stat percentage multipliers. `at` accepts the current ordinal from an
owned-mon snapshot; `find` accepts a stable `rhh:nature/<name>` content key.
Both copy their result and leave the output untouched on invalid input. HP
and unchanged stats report 100. The ordinal is an engine value and must not
be saved as a durable identity. Personality-derived nature still governs
breeding and other nature behavior; `stat_nature` is a separate, stat-only
modifier.
Battle Palace and flavor-berry checks use a nature captured from the actual
source Pokémon when a battler is created or switched in. For other battle
data, they use a matching party Pokémon when its OT and personality agree,
then fall back to the battler's personality. The captured value follows
simulated copies even if the party slot is replaced by an identical OT/PID
clone. With an active run and the internal nature-format marker active, valid stored overrides
are read by the same accessor; otherwise nature remains personality-derived.
During recorded playback, the recording's own format takes precedence over
the loaded save marker. Unnegotiated link contexts fall back to personality.
True-nature changes use the separate version 1.5 edit transaction described above.
Synchronize also reads the lead Pokémon's true nature through the shared
accessor, subject to the configured generation's encounter eligibility and
chance. A stat-only mint does not change the nature it supplies.
Version 1.1 adds `effects`, a copied view of the original nature's five flavor
preferences and Battle Palace move-group probabilities above and at or below
half HP, plus its target preference. Each probability row sums to 100. These
are true-nature rules; a stat-only mint does not change them. The method leaves
its output untouched on invalid input and does not advance RNG.

Version 1.2 adds `format_preflight`, a read-only check for the proposed
true-nature save format. It reports whether the format is already active,
whether the current state is unavailable, or the first incompatible or
unreadable party, saved-party, PC, daycare or fusion slot. The marker cannot be
enabled while legacy occupied data contains unknown upper held-item bits or a
corrupt Pokémon, or while storage, battle, save or link state is unsafe.
The check does not change the Pokémon, enable the marker or authorize an edit;
an active marker reports `ACTIVE` without revalidating all saved data.
Version 1.3 also checks the current save's recorded-battle sector, reporting
the trainer and party slot or an unreadable recording. An absent sector is
treated as no recording. Version 1.4 recognizes a checked nature-format
recording. The recording format uses invalid legacy battle flags as a version
marker, so older readers reject it; newer playback restores the original flags
and retains the recording's format independently of the loaded save generation.
The preflight does not inspect separate exported recordings. Live link
negotiation remains unimplemented.

`rhh:service/battle` version 1.0 provides copied battle snapshots and typed
event context. After the initial battlers are loaded and absent slots marked,
`RHH_BATTLE_STARTED` is followed by one `RHH_BATTLER_ENTERED` notification per
present battler, in battler-index order. A later entry notification is emitted
when a switch has loaded the replacement and queued its send-out animation;
replaying an animation without a switch does not emit one. `RHH_BATTLE_ENDED`
reports the committed outcome before battle resources are discarded. Stopping
recorded-battle playback reports an aborted end. A battle interrupted before
its intro reaches the initial-battler boundary produces neither start nor end.

`current` copies the live snapshot while that battle is active, including
during its end callback; `context` copies the event, cause and snapshot only
inside the three notifications. Failed or undersized queries leave the output
untouched. Each event has a process-scoped battle ID and a per-battle sequence;
neither is a durable encounter identifier. Participants include a stable
species key for the battle form and the party species, battler position,
owner, current party slot, HP and OT/personality identity. That identity can
collide, and party slots can change; neither is a permanent individual ID.
Slots being replaced are omitted until their switch-in is committed. Static
trainer keys are supplied when known; facility, link, recorded, e-Reader and
secret-base opponents may use unrelated trainer numbers and report zero.
For these notifications, `event_key` is the map key, `ordinal` is the
sequence, and `args[0..2]` are the battler, cause and outcome. An entry's
`subject` is its OT/personality identity; start and end use zero. The partner
flag describes an in-game NPC partner; the multi flag also covers link
multis. Recorded playback is flagged so mods can avoid treating it as new
progress.
Lifecycle callbacks are observations: they do not change battle mechanics,
and general gameplay commands remain unavailable during notification dispatch.
The condition setter described below is a separate, narrowly scoped exception
for committed move-resolution and custom-effect observers.

Battle service version 1.44 adds `RHH_BATTLER_HEALTH_OBSERVED` and its
callback-scoped `health_observation_context`. It samples active battlers after
unpaused native game frames and reports copied before/after participants and flags for HP
and maximum-HP changes. Event arguments are battler, previous HP, current HP
and change flags; the species and OT/personality identity are also supplied to
trusted mods. The initial battle sample and the first sample after a switch
establish baselines, and AI
simulations, link battles and recorded battles are excluded. The observer
restores gameplay RNG after callbacks. This is a coalesced invalidation view:
it cannot identify the cause, report changes reversed between samples, or
guarantee a permanent individual identity. It is not a player-information
boundary or a per-hit, damage or healing receipt.

`native/tests/battle_lifecycle.py` exercises actual switch script commands,
single and multi-trainer battles, outcomes and an independent module.
`native/tests/battle_lifecycle_replay.py` builds a temporary game and checks
the same start and end boundaries in a real story battle. Both clean their
test builds and temporary files.

Battle service version 1.1 appends `trainer_party_source`, preserving the
version 1.0 prefix (`RHH_BATTLE_SERVICE_V1_0_SIZE`). For an ordinary generated
opponent party, it reports the trainer key, the trainer whose roster supplied
the entry, selected difficulty, authored pool index and size, source species
and level, and the actual generated species and level. The flags report only
explicit lead and ace tags in the selected source entry. An untagged Pokémon
is not automatically an ace merely because it is highest-level or last.

The result follows the generated Pokémon if its opponent party is reordered.
Its OT/personality identity must match exactly one current Pokémon and one
source record in that party; collisions, corrupt party data, missing records,
stale trainers and unsupported facility/link/recorded/debug parties return
unavailable. The source index is an ordinal in the current roster definition
and difficulty, not a durable content key. Mods persisting a boss mapping should validate the
source trainer, species and other expected data after content updates. This
service does not choose an ace or persist a player's designation. The
independent-module test `native/tests/trainer_source.py` checks actual pool
selection, species randomization, multiple trainers and ambiguity, and removes
its test builds afterward.

`rhh:service/trainer_roster` version 1.0 lets a mod inspect a trainer's
effective authored roster before or during a battle. `get` copies the canonical
trainer and roster keys, effective difficulties, configured party size, pool
size and override/pool flags. `entry` copies a source species, level and
explicit lead/ace tags by index. The size is the configured full team; battle
rules can use a smaller team. A trainer override uses the same single roster
override and difficulty fallback as the engine's party generation.

The roster token fingerprints its current definition and entries. Store it
with the trainer key and selected index if a mod needs a checked configuration;
`entry` and `match` reject a changed token or other mismatched snapshot fields.
The token is not a permanent entry ID or a cryptographic guarantee, and a game
or difficulty change can require the player to select again. `match` compares
the selected source against an opponent's validated generated-party provenance,
so a species randomizer or party reorder does not lose the match. A different
trainer or source returns false; unavailable or ambiguous provenance, stale
rosters, unsupported battles and invalid arguments fail without changing the
output. These queries do not generate a party or consume gameplay RNG. They do
not designate the player's Pokémon or persist a selection by themselves.

`rhh:service/trainer_catalog` version 1.0 enumerates authored trainer
encounters with copied display names, classes, canonical keys and encounter
identifiers. The all-trainers and boss filters use the current run and
difficulty; the boss flag is an approximate class-based grouping. Indices and
labels are for presentation, while the trainer key identifies one authored
encounter. In particular, rematches have separate keys. Queries neither
generate parties nor consume gameplay RNG.

`rhh:service/boss_ace` version 1.0 stores up to 64 configured trainer-source
selections per run, separately for each standard effective trainer difficulty.
`get` and `at` copy a selection and the global configuration revision; `at`
orders records by trainer key and then difficulty. A selection stores
the trainer, roster, source index, species, level, explicit tags, roster token
and owning module. It is marked stale if the current roster no longer matches;
inactive if a different difficulty is current; unavailable if the trainer cannot
be inspected; and orphaned if its owner is no longer loaded. These flags do not
silently remap the selection.

`preview` and `apply` require a registered owner and the current revision.
For `SET`, pass a current `RhhTrainerRoster` and source index; for `CLEAR`,
pass the trainer and difficulty, with source index zero. A live owner's
selection can only be replaced or cleared by that owner. An orphaned selection
can be adopted or cleared by another registered module, and a removed trainer
can still be cleared. `RHH_CAN_CONFIGURE_BOSS_ACE` constraints may deny a
change; `context` copies the proposal during that hook. The host rechecks the
revision, run and roster after constraints. Successful commits send one
`RHH_BOSS_ACE_CONFIGURED` observer event; previews, no-ops and rejections do
not. The event key is the trainer, subject is the owner, and ordinal is the
new global revision; arguments contain action, source index and preview flag.
The global revision and selections follow ordinary save rollback. These owner
rules coordinate trusted modules; they are not a security boundary.

During an ordinary battle, `match` returns the configured selection and the
opponent's checked trainer-party source. It reports a match only for a current,
non-orphaned selection whose recorded trainer, difficulty, roster and entry
still agree. Missing or ambiguous provenance fails without changing the output;
no selection or a stale selection produces a non-match. Randomized species and
party reordering preserve a valid source match. The service does not mark the
player's Pokémon, infer an ace for untagged rosters, or create a battle cue.

Battle service version 1.2 adds `RHH_BATTLE_TURN_ENDED` and `turn_context`.
This observer runs once after a turn's existing end-of-turn effects finish,
before the next turn's AI is prepared. Its copied battle snapshot contains
the completed turn, and its sequence is separate from battle lifecycle and
move-resolution sequences. `turn_context` is available only during that
observer, including when nested callbacks are involved; initialize its
`struct_size` before calling. The event's key is the map, its ordinal is the
turn sequence, and its arguments are turn, battle flags, participant count,
and zero. It is omitted when the battle has ended, and in link and recorded
battles. Use `RHH_BATTLE_ENDED` for final cleanup. The 1.0 and 1.1 service
prefixes remain available.

Battle service version 1.3 adds a shared registry of temporary custom battle
conditions. Declare `RhhModBattleCondition` entries in the appended module
definition using `RHH_MOD_ABI_CONDITIONS`: each has a stable
`namespace:condition/name` key, a label, field or side scope, and a maximum
duration of 1–255 completed turns. `condition_count`, `condition_at`, and
`condition_info` enumerate copied definitions after finalization. The limit is
16 definitions per mod and 128 total. Field definitions have one instance;
side definitions have independent player and opponent instances.

Any mod can read an instance through `condition_state` during an active local
battle, including in pure battle and AI hooks. A committed
`RHH_MOVE_RESOLVED`, `RHH_MOVE_EFFECT_APPLY` or `RHH_REGISTERED_MOVE_EFFECT` observer can call
`condition_set` to set or refresh a valid instance within the definition's
duration limit, or remove it with zero. Calls compose in normal observer order. The host decrements all active
timers once per completed turn, before `RHH_BATTLE_TURN_ENDED` observers; the
triggering turn counts. Instances reset with the battle and do not persist in
saves. State access and mutation are unavailable in link and recorded battles.
Definitions do not automatically alter damage; mods supply their mechanics
through shared battle/AI hooks.

Battle service version 1.4 adds `active_condition_count` and
`active_condition_at`, which copy the live condition key, label, scope, side,
remaining turns and battle ID. Enumeration is ordered by key and target; its
indices can change when conditions change. These calls fail without changing
their outputs when a local battle is unavailable, including link and recorded
battles. The native battle screen presents registered active conditions in a
compact ribbon above the health boxes, using the game's font. When several are
active, the ribbon cycles through them every three seconds and shows its
position in the list. It disappears when none remain, during fades, and on
other screens. Labels too wide for the ribbon are shortened; the service
retains the full label.

Battle service version 1.5 adds the `RHH_BATTLE_CONDITION_CHANGED` observer
and `condition_change_context`. It delivers a copied key, scope, target,
previous and current durations, reason, turn, run and battle IDs, and a
per-battle sequence. The event key is the condition key; its arguments are
reason, target, previous duration and current duration. Application, duration
changes and explicit removal are delivered immediately after the committed
`condition_set`, nested inside the move observer. Setting the same duration is
silent. Expirations are delivered after all condition timers have ticked, in
key and target order, before `RHH_BATTLE_TURN_ENDED`. Teardown clears remaining
conditions without individual events; use `RHH_BATTLE_ENDED` for cleanup.
The context is available only within the matching observer, not in callbacks
nested inside it, and the state service already reflects the committed change.
Condition-change observers cannot themselves call `condition_set`.

Battle service version 1.10 adds `RHH_BATTLE_CONDITION_PROPOSAL` and
`condition_proposal_context`. When a committed move or innate observer calls
`condition_set`, transforms may adjust the requested duration, then
constraints may deny it. The copied context identifies the condition, target,
previous and requested durations, maximum, originating event, move or innate,
and battlers. Constraints cannot rewrite the value and a denial is sticky.
An invalid duration or denial rejects the set and emits no change event;
otherwise the resulting duration is committed and changes are notified as before. A proposal can turn
a same-duration request into a change. Proposal callbacks are read-only and
cannot recursively set conditions. Timer ticks and battle teardown do not run
this proposal; expirations retain their existing notification order.
`condition_set_result` performs the same operation with a copied status,
before/after durations and rejection reason; the original `condition_set`
continues to return zero on acceptance and -1 on rejection.

Battle service version 1.6 adds `condition_expiry_message` to the
`RHH_BATTLE_CONDITION_CHANGED` observer when its reason is
`RHH_CONDITION_EXPIRED`. The host copies up to 32 messages per completed turn,
in condition-key/target order and then observer order. Once all timers have
ticked and turn-ended observers have run, the normal battle message window
displays them before the next turn's AI is prepared. Calls in other events,
for other change reasons or from nested callbacks fail. The text validation is
the same as `effect_message` below; a full queue rejects additional messages.
Messages are not emitted automatically every turn, and remaining conditions
cleared at battle teardown do not generate expiry messages. Presentation does
not consume another speed-tie RNG roll when the end-of-turn script resumes.

Battle service version 1.12 adds `condition_expiry_animation`. An expiry
observer can queue up to 32 existing `RHH_EFFECT_ANIM_*` cues per completed
turn, independently of the message limit. Cues play in observer order after
queued innate actions and before all expiry messages, through the normal
battle controller and its animation settings. A cue alone schedules the
end-of-turn presentation. The host rechecks the battle, turn, battler slot,
owner, party slot, identity and species before playback; a fainted or changed
recipient is skipped. The copied OT/personality identity can collide, so this
is an optimistic check rather than a permanent Pokémon identifier. Calls
outside expiry observers or when the queue is full fail.

Battle service version 1.13 adds `condition_change_animation` for a non-expiry
`RHH_BATTLE_CONDITION_CHANGED` observer when the change was committed inside a
move-effect observer. It queues a cue in that effect's ordered eight-action
queue. Rejected and unchanged sets emit no change event, so they cannot queue a
cue. Calls for expiry, move-resolution or innate-origin changes fail in version
1.13; use the separate expiry command for expiration. Playback uses the same
living-recipient checks and animation settings as `moves.effect_animation`.

Version 1.14 also accepts changes made during an innate turn-end observer. Its
cue shares that turn's ordered 32-action queue with innate healing and stat
changes. Before playback the host rechecks the source and recipient against
their captured battle participants; fainted or changed slots are skipped. It
uses the normal controller wait and restores the scripting battler afterward.
Version 1.15 also accepts changes committed by innate entry callbacks. It keeps
up to eight entry cues in observer order and plays them after the existing
entry healing and stat actions, before entry messages. The entering battler
and cue recipient are rechecked at playback, including pending switches, and
the scripting battler is restored. A cue alone schedules the switch-in script.
Version 1.16 also supports changes committed by a move-resolution observer.
Up to eight cues run after final move cleanup, before the remaining move-end
phases. They use the normal animation controller and its settings; a changed,
fainted or pending recipient is skipped. The host resumes the original
move-end command without dispatching resolution again, including single-case
and bounded cleanup modes. Link and recorded battles retain their original
behavior. These checks use the copied participant identity, which is not a
permanent individual identifier.

Battle service version 1.17 adds `side_of`, which returns the player or
opponent side for a valid battle slot, using the same values as side-scoped
conditions. It is available during local AI calculations and prospective
switch forecasts even when that slot is absent or pending; it does not expose
the Pokémon occupying the slot. Invalid inputs and excluded battle modes fail
without changing the output.

Battle service version 1.18 adds the observer-only
`RHH_BATTLER_STATUS_APPLIED` event and `status_application_context`. The event
reports a real change made by the engine's common major-status setter,
including its move, ability or protect trigger, status kind, battler slots,
raw before/after status and available copied participants. A move key is
provided only for the move trigger. The battle status is already changed and the controller
update has been queued, but the party Pokémon may not yet have received that
update. The event is local-battle-only and restores both gameplay RNG streams
after observers. Its sequence resets per
battle. Direct status writes, cures, switches and other engine paths outside
this setter do not produce this event; missing participants and colliding
OT/personality identities remain possible.

Battle service version 1.19 adds the separate observer-only
`RHH_BATTLER_ITEM_STATUS_CHANGED` event and `item_status_context`. It reports
major-status additions and removals committed by the common item-effect
dispatcher, including Flame and Toxic Orbs and curing berries. The context
contains the item key when mapped, battler,
available participant, raw status values and stable condition flags. Its
origin states where the engine read the item; it does not identify who supplied
or consumed it. Confusion-only changes and unchanged major conditions do not
notify. Status has changed and the controller update has been queued, but the
item's later consumption or party-data synchronization is not guaranteed yet.
The stream has its own per-battle sequence, excludes link and recorded battles,
and restores both gameplay RNG streams. Other
item-use and direct status paths remain outside this event.

Battle service version 1.20 permits `condition_set` and `condition_set_result`
from either status observer. This commits the shared condition synchronously,
before the status presentation finishes, using the usual condition proposal
and change notifications. The affected battler must have a copied participant;
its slot, identity and status are rechecked after policy callbacks. Policy
callbacks can read the originating status context during the nested proposal
and change notification. For item events, the proposal's source is the item;
its actor is the holder only when the engine read the held item, otherwise the
actor is unknown. An otherwise allowed status-origin proposal that consumes
gameplay RNG is denied.

Battle service version 1.21 adds `status_attribution`, available in the same
callbacks as `status_application_context`. When an innate entry or turn-end
action applied the status, it identifies the originating innate key, list
index and entry or turn sequence. This is captured when the action is queued,
and remains available to nested condition proposal and change callbacks.
Other applications return `RHH_STATUS_ATTRIBUTION_UNKNOWN`; in particular,
the host does not infer an ordinary ability from mutable engine globals. The
attribution does not change the existing condition proposal source or event key.

Battle service version 1.22 permits `condition_change_animation` for a
condition committed by either status observer when the engine has a supported
returning status or item presentation script. Up to eight cues
are queued per status event. They play after that presentation, in request
order, and skip a recipient whose battle, turn, slot, species or identity has
changed or who is no longer alive. Nested presentations such as a curing berry
run their own cues before resuming the enclosing status presentation. A full
reaction-frame capacity, a missing recipient or an unsupported presentation rejects the cue;
the continuation does not add another entry to the battle-script stack;
no gameplay action is implied by a cue.

Battle service version 1.23 adds `status_reaction_animation`,
`status_reaction_heal` and `status_reaction_stat` for either status observer
and its nested condition-change observers. They share the existing eight-action
queue and execute in request order after the original presentation. A heal
uses a fraction of the recipient's non-Dynamax maximum HP, rounded down with a
minimum of one, and uses the normal HP controller update; it is skipped if the
recipient is at full HP or under Heal Block. A pending engine HP update is
restored after the heal. A stat action uses the normal unsourced battler-stat
path, including its ability and stage rules; it is skipped if the engine's
stat-change queues are busy. These are not hostile, source-attributed stat
changes. As with cues, the recipient and battle are rechecked at playback,
and an unavailable or unsupported presentation rejects the request.

Battle service version 1.24 adds `status_reaction_stat_from` and
`status_reaction_status_from`. Each names a living source and recipient, both
of which are checked again before execution. Source-attributed stat changes
use the engine's ability-style rules, including stat-loss protection, Mist,
Mirror Armor and reactions such as Defiant. Major-status actions use the same
ability-style immunity and status rules as queued innate statuses; they support
sleep, poison, burn, freeze, paralysis and toxic poison. Both ignore a previous
move's Mold Breaker state. The resulting status application produces the usual
status event with an ability trigger, but does not claim a specific ability
key. Nested status reactions use separate bounded frames and execute before the
enclosing reaction resumes. A status action is skipped if the battle-script
stack has no room for its presentation. These actions share the eight-action
limit with the earlier reaction methods.

Battle service version 1.25 adds `status_reaction_damage` for indirect
fractional damage. It uses the recipient's non-Dynamax maximum HP, rounds
down with a minimum of one, bypasses Substitute, and normally respects Magic
Guard. `RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD` opts out of that protection.
The action uses the regular HP update, faint handling and HP-threshold held-item
scripts; the recipient and prior pending HP update are handled as for reaction
healing. Damage shares the eight-action queue and can trigger nested item or
status reactions before the enclosing queue continues.

Battle service version 1.26 adds `status_reaction_cure` and the observer-only
`RHH_BATTLER_STATUS_CURED` event. A cure selects one or more
`RHH_BATTLE_CONDITION_*` major-status flags and rechecks the recipient and its
current status at playback. It clears only matching conditions, updates the
status controller and icon, and handles sleep-clause, Nightmare, toxic-counter
and freeze-turn cleanup. It also checks status-dependent form changes for the
actual recipient. A no-op or an unavailable script continuation produces no
event. `status_cure_context` reports the requested and removed flags, raw
before/after status, participant and originating status-event sequence. Version
1.26 reports cures performed by this command; version 1.27 adds the engine
paths below. Its observers can set checked shared conditions or queue
the same bounded reaction actions, including further cures. Nested condition
policies and change observers can query the cure context. The controller
update is queued before notification; party data may not yet be synchronized.

Battle service version 1.27 extends that event to the common immunity-ability
dispatcher and the end-of-turn Hydration and Shed Skin cures. For these engine
cures, the requested flags are the affected major conditions, the originating
event and sequence are zero, and the event key is the curing ability when
known. `status_cure_attribution` distinguishes those two ability paths from a
reaction-queue cure and reports the source battler and stable ability key.
Nested condition policies receive the ability as their source and the cured
battler as actor. Observers can queue the same reactions after the normal
ability presentation. Confusion-only immunity changes and other engine cure
paths are not included. Both gameplay RNG streams are restored after observers;
the engine's own Shed Skin roll still occurs normally.

Battle service version 1.28 extends cure notifications to waking or thawing
during the ordinary attack canceller and Battle Palace, a self-thawing move,
and a move-end hit that thaws a battler. `status_cure_attribution` distinguishes
natural recovery from a move cure. `status_cure_move` returns the curing move's
stable key for move cures and zero for other causes; the move and attacker also
populate nested condition proposals. Natural recovery includes Uproar wakeups
without inferring a source. Sleep-counter decrements that leave the battler
asleep do not notify. These notifications follow the engine's existing cure
and presentation behavior; other direct status writes remain outside the event.

Battle service version 1.29 also reports active-battler cures from Purify,
Jungle Healing, Lunar Blessing, Psycho Shift, Refresh, Heal Bell and
Aromatherapy, plus Healer and Pastel Veil. The event is committed when the
status changes; requested reaction actions run after the existing cure
presentation. For a party cure, active battlers are reported separately in
engine order, with each event's reactions played in that order. Off-field party
members are not represented as battle participants. `status_cure_attribution`
uses `RHH_BATTLE_CURE_SCRIPT_ABILITY` for Healer and Pastel Veil, and identifies
the original ability owner where available; move cures report the move and
original user when still identifiable. The host restores the interrupted
script's battler and text scratch state after reactions. Other direct cure
paths remain outside this event.

Battle service version 1.42 also reports Natural Cure when a battler switches
out, and Uproar's scripted wakeups. `status_cure_attribution` identifies the
switch-out ability and owner with `RHH_BATTLE_CURE_SWITCH_ABILITY`; the event
is emitted after the active status changes but before the controller updates
the party and the battler leaves the field. The switch-out path has no safe
presentation point for queued status reactions, so reaction requests fail.
Uproar wakeups use `RHH_BATTLE_CURE_UPROAR` without inferring a source or move;
their queued reactions run after the normal wake-up presentation. Soundproof
and other existing engine checks still govern whether a wakeup occurs. Request
version 1.42 and check `RHH_BATTLE_SERVICE_V1_42_SIZE` before relying on these
additional cause values.

Battle service version 1.43 also reports Take Heart's major-status cure as
`RHH_BATTLE_CURE_MOVE`, with the move and its user attributed. Reactions run
after the normal move presentation; using the move without a major status to
remove emits no cure event. Other direct cure paths remain outside this stream.

Battle service version 1.30 adds the separate observer event
`RHH_BATTLE_PARTY_STATUS_CURED` for player-owned, off-field party members
cured by Heal Bell or Aromatherapy. `party_status_cure_context` copies the
party slot, species key, OT/personality identity, before and after status,
removed major-condition flags, move, source battler when still identifiable,
and run, battle and event sequence. OT/personality is not a permanent unique
identity. The host checks the selected party member before and after the
actual status controller update, then rechecks it before notification after
the script's controller wait. Replaced, changed or newly active members are
not reported. Active battler cures remain on `RHH_BATTLER_STATUS_CURED`;
opponents' unrevealed party members are not exposed. Link and recorded
battles and pending switch transitions are excluded.

The context also remains available inside nested condition proposals and
change notifications. Observers can set shared conditions and queue existing
status-reaction actions on present battlers; a party member itself is not a
battler target. Reactions follow the active-battler reactions, in party-slot
order, after the normal cure presentation. A stale member or a full reaction
queue rejects affected commands. These events cover the specified party-wide
move scripts, not every possible party status mutation.

Battle service version 1.40 additionally reports major-status cures made by
the battle Bag's item-cure script. Active recipients use the existing
`RHH_BATTLER_ITEM_STATUS_CHANGED` stream with origin `RHH_BATTLE_ITEM_BAG`.
For a player-owned off-field recipient, the separate observer event
`RHH_BATTLE_PARTY_ITEM_STATUS_CURED` supplies `party_item_status_cure_context`:
the item key, slot, species, OT/personality, source battler, raw before/after
status and normalized removed-condition flags. The host checks the off-field member
before and after the cure; opponents' off-field parties are not exposed.
These notifications occur synchronously after the status changes and before
the cure message. Observers can set shared conditions with the usual
revalidation, including nested access to the party-item context. No event is sent
for a no-op or a volatile-only cure. Link and recorded battles and AI simulations
are excluded; pending switches are also excluded for the party event. Observer gameplay
RNG is restored. Other item and party status paths remain separate.

Battle service version 1.41 extends the existing status-reaction queue to
active and off-field Bag cures, including nested condition-change observers.
Actions target present battlers, execute in request order after the item's
cure message, and recheck their recipients at playback. The normal eight-action
and reaction-frame limits apply; a rejected or stale action does not undo the
cure. The off-field party member is not itself an action target.

Battle service version 1.31 adds the pure `RHH_BATTLER_SPEED` reducer and
`speed_context`. The proposal starts with the engine's speed after its normal
modifiers and is used by shared battle and AI speed calculations, including
turn order and speed-based moves. The context copies the battler, engine speed,
effective weather, terrain, HP, level, species, and the caller's ability and
hold-effect assumptions. The hold effect is a transient engine ordinal.
`RHH_BATTLE_SPEED_AI` marks confirmed AI calculations; an unmarked shared
call is not proof that it is live. Species and HP are engine snapshots, not a
general hidden-information boundary. Constraints may deny but cannot rewrite;
accepted speeds range from 1 through `RHH_BATTLE_SPEED_MAX` (171798691), so
speed-based move multiplication stays within 32 bits. A denied, invalid or
RNG-consuming proposal retains the engine speed, including an original zero;
both gameplay RNG streams are restored. Link and recorded battles are excluded.

Battle service version 1.32 adds `environment` and the observer event
`RHH_BATTLE_ENVIRONMENT_OBSERVED`, with copied `environment_observation`
context. The view reports underlying weather, terrain, raw weather flags,
weather suppression and effective weather, engine timer counters, turn, run
and battle IDs. A zero timer is the engine's value and can mean no effect,
an indefinite effect or a transition; it is not a guaranteed expiry time.
Weather suppression follows the engine's current ability calculation, including
its transient Mold Breaker state. These are live engine snapshots, not an AI
knowledge filter or a terrain-grounding query. Link and recorded battles,
ended battles and unavailable run contexts reject the query without changing
its output.

`rhh:service/battle_grounding` version 1.0 supplies per-battler terrain
eligibility. `live` uses the engine's current effective ability, hold effect,
active types, Gravity and other grounding effects. It reports whether the
battler is grounded, semi-invulnerable, and affected by the current terrain;
it also copies semantic and raw terrain values, turn and battle identity.
The query rejects unavailable, fainted or absent battlers and AI simulations.
It uses read-only ability and item lookups and restores gameplay RNG.

`ai` is available only during `RHH_AI_MOVE_SCORE` and
`RHH_AI_EFFECT_PREVIEW`. It applies the same grounding rules to the AI's
cached ability and hold-effect assumptions and its Illusion-aware type view.
Separate flags identify cache values that are none or unknown; the result is
conditional on those assumptions and is not a prediction of future state.
Semi-invulnerability and other engine field and battler state are current
snapshots, and other trusted APIs remain accessible. Both calls leave outputs
unchanged on failure. Request version 1.0 and check
`RHH_BATTLE_GROUNDING_SERVICE_V1_0_SIZE`.
Version 1.1 adds `preview_live` and `preview_ai` for a supplied semantic
terrain, including no terrain. Their returned terrain and engine-terrain fields
describe the supplied scenario, and `RHH_BATTLE_GROUNDING_SCENARIO` marks the
result. They use the same live or AI grounding inputs and restrictions as the
corresponding current-state query, without changing the field. These are
conditional checks: they do not determine whether terrain can be set or model
subsequent type changes, terrain seeds, abilities or other reactions. Request
version 1.1 and check `RHH_BATTLE_GROUNDING_SERVICE_V1_1_SIZE`.
Version 1.2 adds `entry_ai` and `preview_entry_ai` for the candidate in
`RHH_AI_INNATE_ENTRY_FIELD_PREVIEW`. The first uses the projected terrain
after the field forecast's retained and currently offered changes; the second
tests a supplied terrain. Both use the candidate's simulated types and the
AI's cached ability and hold effect, and mark the result as an AI entry
scenario. They verify the candidate against its party slot and restore the
slot index after resolving its types, including for a temporarily absent or
switching battler. These views do not predict terrain reactions or guarantee
that tentative offers will be accepted. They are unavailable outside that
callback. Request version 1.2 and check
`RHH_BATTLE_GROUNDING_SERVICE_V1_2_SIZE`.

The first valid environment-observer sample after battle start establishes a baseline. Subsequent
native frames notify observers when weather, terrain, suppression, raw weather
flags or either timer differs from the previous sample. Changed bits identify
the differences; the copied before and after states and a battle-local,
process-scoped sequence are available only during the callback. Sampling can
coalesce several changes or miss a change reversed before the next frame; this
is not a committed transaction or a source-attribution stream. Invalid battle
states clear the baseline. Observers cannot reenter the sampler and both
gameplay RNG streams are restored after dispatch.

`rhh:service/battle_field_rules` version 1.0 copies the engine's active Magic,
Trick and Wonder Room, Mud Sport, Water Sport, Gravity, Ion Deluge and Fairy
Lock flags, together with raw counters for rules that have timers. Active bits
and counters are independent: a zero counter can mean an indefinite effect,
and an inactive rule can retain a counter. The snapshot includes the battle,
run and turn and indicates AI calculation, prediction and switch-calculation
contexts; it reflects the current engine state rather than forecasting an AI
outcome. It does not change rules or provide committed mutation notifications. Link,
recorded, ended or unavailable battles and invalid outputs fail without
modifying the result. Request version 1.0 and check
`RHH_BATTLE_FIELD_RULES_SERVICE_V1_0_SIZE`.
Version 1.1 adds the observer-only `RHH_BATTLE_FIELD_RULES_OBSERVED` event and
its callback-scoped `observation` view. The first valid frame seeds a baseline;
later frames report changed active bits and raw counters with copied before and
after states. AI simulation samples are skipped, and invalid battles clear the
baseline. This is a coalesced observation: changes between frames can be missed,
and it does not identify the cause or guarantee a committed transaction.
Observers cannot reenter the sampler, and gameplay RNG is restored after
dispatch. Request version 1.1 and check
`RHH_BATTLE_FIELD_RULES_SERVICE_V1_1_SIZE`.
Version 1.2 adds `RHH_BATTLE_FIELD_RULES_COMMITTED` and callback-scoped `commit`
for supported engine mutations. It covers move-script Gravity, room toggles,
field-based Mud and Water Sport, Fairy Lock, Gravity and Ion Deluge additional
effects, initial room settings, the engine's Ion Deluge clear, and the normal
expiry of room, sport, Gravity and Fairy Lock timers. Events identify the cause,
changed flags and counters, and a move and source battler when the caller
supplies them. Failed setters, timer-only decrements, per-battler legacy sport
effects and other mutation paths do not produce this event. AI simulation and
unsafe or unavailable battle contexts are excluded; event observers cannot
issue commands and gameplay RNG is restored. Request version 1.2 and check
`RHH_BATTLE_FIELD_RULES_SERVICE_V1_2_SIZE`.
Version 1.3 adds `queue_move_effect` for Gravity and Ion Deluge during a
committed move-effect observer. Requests share the existing
eight-action queue and recheck the battle, source and applicable target before
execution. Already-active rules are skipped. Successful actions use the
engine's additional-effect scripts, including Gravity's airborne handling and
the normal committed field-rule event. A queued request does not guarantee
execution, add a new field rule or automatically supply AI scoring. Mods can
query the current field and provide their own AI scoring through existing
hooks. Request version 1.3 and check
`RHH_BATTLE_FIELD_RULES_SERVICE_V1_3_SIZE`.
Version 1.4 adds `offer_ai_move_effect` for Gravity and Ion Deluge in an
`RHH_AI_EFFECT_PREVIEW` constraint. It adds a field-rule action to the shared
eight-action preview list, with the rule in `first` and a chance from 1 to 100.
During `RHH_AI_MOVE_SCORE`, use the move service's preview queries and
`ai_effect_estimate` with `RHH_AI_EFFECT_NO_BATTLER`. The estimate is zero and
blocked if the rule is currently active, or one if it is not. It does not
predict intervening actions, participant validity or eventual execution. The
host does not automatically score these actions; scoring and chance weighting
remain the mod's responsibility. Request version 1.4 and check
`RHH_BATTLE_FIELD_RULES_SERVICE_V1_4_SIZE`.
Version 1.5 adds `queue_room_toggle` for Trick Room, Wonder Room and Magic Room
from a committed move-effect observer. It shares the existing action queue and
participant rechecks. Each action toggles the room as it exists when that
action runs, using the engine's five-turn timer and room message; Trick Room
also runs the normal Room Service loop. Repeated actions can therefore turn a
room on and then off. The resulting supported field-rule commit keeps the
original move and source attribution. `offer_ai_room_toggle` can describe the
same action in an AI preview; it supplies a `RHH_AI_EFFECT_ROOM_TOGGLE` action
whose `first` is the room flag and whose chance is 1–100. Estimate it through
the move service using `RHH_AI_EFFECT_NO_BATTLER`: -1 means the room is active
now, and +1 means it is inactive now. The estimate does not simulate other
actions, later state changes or participant checks, and the host supplies no
automatic score. Request version 1.5 and check
`RHH_BATTLE_FIELD_RULES_SERVICE_V1_5_SIZE`.
Version 1.6 adds `queue_field_move` and `offer_ai_field_move` for Mud Sport,
Water Sport and Fairy Lock. They use the same action and preview limits as the
earlier field-rule calls. The queued action uses the ordinary engine setter
and message, skips failed or already-active effects, and retains the original
move and source for supported field-rule commit events. Sports fail in sky
battles and use the configured generation's existing field-timer or
per-battler behavior; legacy per-battler changes are not field-rule commits.
Fairy Lock follows the engine's normal two-turn timer. Independent AI scoring
hooks can inspect the offered `RHH_AI_EFFECT_FIELD_RULE` action through the
move service. A no-battler estimate reports zero and blocked when the current
sky-battle or already-active checks fail, and one otherwise. It does not
predict intervening actions, participant validity or eventual execution, and
does not automatically score the effect. Request version 1.6 and check
`RHH_BATTLE_FIELD_RULES_SERVICE_V1_6_SIZE`.
Version 1.7 adds `sports`, a copied view of Mud Sport and Water Sport sources.
It reports the configured field-timer or battler-volatile mode, raw field
flags and counters, and raw per-slot volatile masks. Its effective mask follows
the damage engine: field flags always count, and in the older mode any raw
battler flag also counts. Like the engine, this scan includes absent or fainted
slots; a bit is not a durable Pokémon identity. The view also reports the sky
battle flag, battle and turn, and AI context. It describes the current state,
not whether a later move will successfully apply a Sport. The existing field
snapshot and committed event continue to report only field-wide state.

`RHH_BATTLE_SPORTS_OBSERVED` and callback-scoped `sports_observation` provide
coalesced before/after Sports views with a changed-fields mask and battle-local
sequence. The event subject is the battle ID; its arguments are the changed
mask, new Mud Sport battler mask, new Water Sport battler mask and new effective
mask. The first valid sample establishes a baseline; intermediate changes can
be missed. AI simulations are skipped, invalid battles clear the baseline,
and callbacks cannot reenter the sampler. Gameplay RNG is restored. This is
not a committed mutation stream or a participant identity record. Request
version 1.7 and check `RHH_BATTLE_FIELD_RULES_SERVICE_V1_7_SIZE`.

Battle service version 1.33 adds `RHH_BATTLE_ENVIRONMENT_COMMITTED` and
`environment_commit`. The observer receives copied before and after states,
changed bits, a battle-local sequence and the reason for a verified underlying
weather or terrain transition. Supported paths are the shared move and ability
setters, weather and terrain expiry, script-wide clear operations, Defog's fog
clear and primal-weather cleanup. Move and ability keys and source slots are
included only when the caller supplies them; clear and expiry paths generally
do not identify a source. Failed setters and timer-only ticks do not produce a
commit event. Battle service version 1.34 also reports explicit starting
statuses, conditions derived from natural overworld weather, and conditions
accepted from the saved-world seed proposal. The world-seed cause identifies
the proposal path: a reducer may supply a value even when no saved effect was
present. Starting conditions can replace one another under the engine's normal
precedence. Other raw writes are not covered; use the sampled observation for
a broader but coalesced view. AI calculations and forecasts, link and recorded battles,
nested mod callbacks and unavailable battle contexts do not dispatch commits.
The host restores both gameplay RNG streams after observers.

Version 1.45 supplies the source battler and move key when Defog clears fog or
terrain, and when a terrain-clearing move executes its normal clear script.
Teraform Zero's weather and terrain clears identify the source battler and
ability key. These are still clear events; other clear paths and expiry may
remain unattributed. As before, failed or no-op clears send no commit event.

Battle service version 1.36 adds the opt-in observer event
`RHH_BATTLE_ENVIRONMENT_TIMER_TICKED` and `environment_timer_tick`. It reports
each non-expiring decrement at the engine's end-of-turn weather and terrain
countdown boundaries, with copied before/after states and a battle-local
sequence. The final decrement is reported by the existing expiry commit, not
the tick event. A zero or indefinite counter does not tick, and no extra
countdown is introduced. Other timer writes are not covered by this event;
the sampled observer remains useful for them. As with committed transitions,
AI forecasts, link and recorded battles, nested callbacks and unavailable
battle contexts do not dispatch ticks. Observer gameplay RNG is restored.

Battle service version 1.37 adds the opt-in
`RHH_BATTLE_WEATHER_SUPPRESSION_CHANGED` observer and
`weather_suppression_change`. They report an actual change in effective
weather suppression across supported ability mutations: copy, swap,
suppression, overwrite, Entrainment, Trace and the contact-triggered mutation
paths. The copied before and after views include the underlying and effective
weather; the payload also identifies the mutation cause, source and targeted slots, move
and contact ability when known. A swap is reported once for the resulting
field, and retaining another suppressor produces no change event. The
notification is separate from raw weather/terrain commits and does not require
an ability-mutation observer. Weather-free fields have no weather suppression
to report. Switches, fainting, and other ability changes are not covered by
this event; use the sampled observer for those paths.
The same local-battle, AI, reentrancy and gameplay RNG protections apply.

Battle service version 1.38 adds the separate
`RHH_BATTLE_WEATHER_GAS_SUPPRESSION_CHANGED` observer and
`weather_gas_suppression_change`. It uses the same copied payload but its
cause is an `RHH_GAS_*` value; the source and target slot identify the gas
source, not necessarily the acting Pokémon or a departing individual. It
reports actual changes to effective weather at supported gas activation,
suppression, faint, switch-out and replacement boundaries, without requiring
a Neutralizing Gas observer. Its battle-local sequence is independent of the
ability-mutation suppression event. A Gastro Acid action can produce both
events when both are subscribed: they describe the same field transition from
the gas-source and ability-mutation perspectives. Other switch and faint
changes still rely on the sampled observer. The same exclusions and RNG
protections apply.

Battle service version 1.39 adds `RHH_BATTLE_WEATHER_PRESENCE_SUPPRESSION_CHANGED`
and `weather_presence_suppression_change`. They report a change in effective
weather suppression when the switch-out ability command marks a slot off-field,
the switch-data command installs a replacement and marks it on-field, or the
faint-animation command marks a slot off-field. The payload identifies the slot
and boundary, with independent battle-local sequencing; on entry it identifies
the replacement slot, not the departing Pokémon. It reports only an actual
change while the underlying weather flags remain the same. Other field-presence
or ability changes still need the sampled view or the separate mutation and gas
streams. The same local-battle, AI, reentrancy and RNG protections apply.

Battle service version 1.7 adds `stat_stages`, a copied view of a present
battler's Attack, Defense, Speed, Special Attack, Special Defense, accuracy
and evasion stages. Each is -6 through +6, with zero neutral. The array uses
`RHH_STAT_*` indices plus `RHH_BATTLE_ACCURACY` and `RHH_BATTLE_EVASION`; its
HP entry is always zero. It includes the battler and battle ID, and can be
queried by AI and battle hooks during a local active battle. Unknown, absent,
fainted or off-field battlers, invalid output sizes, link and recorded battles
fail without changing the output. This is the live battle state, not a
simulated future state.

Battle service version 1.11 adds `nature`, a copied view of a present local
battler's true nature and, when known, stat nature, with stable nature keys
and provenance flags. A captured source Pokémon is marked as a snapshot; a
matching live party fallback is marked separately. If neither is available,
the personality fallback is identified and the stat nature is unavailable.
This is the same true nature used for Palace and flavor-berry behavior, not a
claim about a future simulation. Like the ability view, it is available outside
callbacks and in committed battle observations, not in pure AI calculations;
link and recorded battles are unavailable through this service. Pending,
absent, invalid and undersized queries leave the output unchanged.

`rhh:service/types` version 1.8 exposes stable `rhh:type/...` keys for the
engine's existing types through `RHH_TYPE`. `species` copies distinct species
types, while `battler` copies distinct active types after the engine's form,
Terastallization and Roost rules. The latter requires a present battler in a
live battle. The engine's `NONE` and `MYSTERY` placeholders are omitted from
type lists; these lists hold at most three entries. Invalid keys and unavailable
or undersized queries leave output untouched.

`chart` returns the original, adjusted and effective matchup in unsigned
4.12 units (`RHH_TYPE_ONE` is 4096). Request `RHH_TYPE_MATCHUP_INVERSE` to
preview the engine's inverse-battle rule. `RHH_TYPE_MATCHUP` transforms may
change the pre-inversion value to 0, 2048, 4096 or 8192; invalid values and
denied decisions fall back to the original chart. Constraints can deny but
cannot rewrite a value, and denial is sticky. During that event, `context`
returns the original matchup; `event_key` and `subject` are the attack and
defense type keys, `args[0]` is the original multiplier, and `args[1]` is the
inverse flag. A recursive `chart` query fails; a nested engine chart lookup
uses the original entry. Reducers must be pure and return consistent values
for the same inputs because battle, previews and AI may query repeatedly.

The engine applies the chart hook before inverse-battle conversion and its
move, ability, item and weather rules. A changed Ground/Flying chart entry
affects damage without changing whether the target is grounded for terrain;
Levitate, Air Balloon and other independent airborne effects still apply.
The shared lookup is also used by AI and hazards, though their other mechanics
remain engine-owned. Hooks are excluded without an active run, for placeholder
types, and during link or recorded battles; `chart` marks those cases with
`RHH_TYPE_MATCHUP_EXCLUDED`.

Version 1.1 appends `stab_context` and preserves the version 1.0 prefix
(`RHH_TYPE_SERVICE_V1_0_SIZE`). `RHH_TYPE_STAB` transforms can adjust the
same-type damage multiplier in 4.12 units between `RHH_TYPE_STAB_MIN` (0.5x)
and `RHH_TYPE_STAB_MAX` (2.25x), inclusive. The engine first calculates the
ordinary, Adaptability or Terastallization multiplier; the hook changes that
result before type effectiveness and the remaining damage modifiers. Invalid
values or denial retain the engine value. Constraints cannot rewrite a
proposal, and denial is sticky. Context copies the move, move type, species,
effective attacker ability, attacker and defender indices, and distinct
active and pre-Terastallization types. Its flags identify Terastallization and
AI simulations. `event_key` is the move type, `subject` is the attacker
species, and `args` contain the base multiplier, flags and battler indices.
The same hook participates in actual damage and AI damage simulation. Other
AI heuristics remain engine-owned; reducers should make consistent decisions
for equivalent real and simulated states. Struggle, placeholder move types,
link and recorded battles, and calculations outside a live battle bypass the
hook. Nested STAB evaluation uses the engine value; nested chart queries are
supported. Callbacks are pure and must not consume game RNG or mutate state.

The native engine uses a bounded byte-sized type space.

Version 1.3 extends `chart` to accept keys declared by modules using
`RHH_MOD_ABI_TYPES`. Each `RhhModType` supplies an ID of the form
`namespace:type/name`, a printable label, an RGB color and optional directional
`RhhTypeRelation` entries. The `rhh` namespace is reserved. A relation's
`attack` is this type attacking `other`; `defense` is `other` attacking this
type. Values use the same four 4.12 multipliers as the chart. Set a direction
to `RHH_TYPE_UNSPECIFIED` to leave it to the other type's declaration or the
neutral 1x default. If both custom types declare the same directed matchup,
their values must agree. Duplicate IDs, duplicate relation targets, invalid
values, missing targets, noncanonical built-in keys and conflicting directions
reject activation. References can point to types supplied by another module,
regardless of load order. The host accepts up to `RHH_CUSTOM_TYPE_LIMIT`
definitions in total, `RHH_CUSTOM_TYPES_PER_MOD` per module, and
`RHH_CUSTOM_TYPE_RELATION_LIMIT` relations per definition.

In native single-player battles, a Dynamaxed damaging move with a registered
custom type uses Max Strike as its fallback but retains the resolved custom
type for STAB, effectiveness and other type checks. Max Strike's presentation
and secondary effect remain in use. Max Move previews resolve each slot under
Dynamax without retaining temporary move-type, ability or item calculation
state; the selection screen also reflects custom types changed by abilities or
Electrify. Custom Z-Crystals and type-specific Max
Moves are not registered by this service. Link and recorded battles retain
their existing engine behavior.

`rhh:service/types` version 1.11 adds `legacy_category` and
`legacy_category_context`. A `RHH_TYPE_LEGACY_CATEGORY` transform can assign a
registered custom type a physical or special category for pre-Gen IV rules.
The copied query contains the stable type key and is independent of move and
battler, so a rule must be stable for that type. Constraints can deny but
cannot rewrite the proposed category. Denied, invalid, RNG-consuming and
excluded proposals return the move's configured category; flags report why.
Both gameplay RNG streams are restored if a callback advances either one.
Queries reject recursive category lookups and leave invalid outputs unchanged.
Status moves and explicit dynamic category overrides retain their engine
precedence. The same result feeds battle, AI and supported move displays.

After finalization, `rhh:service/type_catalog` version 1.0 enumerates custom
definitions in ID order and returns copied IDs, labels, colors and stable keys.
Version 1.1 appends `describe`, which copies a display label and canonical ID
for either a built-in or custom type key. Built-in labels are generated from
their stable IDs; the color flag is absent because no RGB color is supplied.
Custom labels retain their full registered text and carry the custom and color
flags. Invalid keys and undersized outputs fail without changing the output.
The version 1.0 enumeration and lookup remain custom-only.
The normal `RHH_TYPE_MATCHUP` transform applies to keyed custom-type chart
queries, including inverse queries. The native engine's chart lookup can map
the first 224 catalog entries to temporary byte-sized slots, in catalog order;
these slots are an internal bridge, can change when the catalog changes, and
must never be saved. Battler type and STAB contexts translate an active slot
back to its stable key. `content_count`/`resolve` do not enumerate or resolve
custom types; use the catalog and chart to inspect them. Species proposals may
assign them, as can existing moves' base-type transforms and registered moves.
Battle-scoped custom Tera type rules are described below. Conversion 2 can
select an available custom type that resists the last move, using the same
chart transformations and inverse-battle rules. It excludes types the user
already has. When no custom type is a candidate, its original random selection
is unchanged; link and recorded battles do not add custom candidates.
The native summary, optional HGSS
Pokédex, DexNav and battle indicator sprite paths can draw internal custom
slots using the registered color and a generated badge. The full badge fits
the label into 32 pixels; the compact battle indicator uses its first letter.
If sprite tiles or a safe palette are unavailable, the display falls back to
the existing mystery icon. Badges share resources between sprites and reclaim
them on replacement, sprite cleanup or screen reset. Active battle types can still
differ. Migration of separately persisted custom content remains open.

Version 1.5 permits registered custom keys in `offer_species_types`. Every
offered type must have a runtime slot; because the byte-sized bridge has 224
slots, a custom definition beyond that range cannot be assigned and the entire
offer is rejected. Proposals are canonicalized, must contain one to three
distinct types, and remain subject to the normal transform and constraint
ordering. The engine uses the result in its three battle slots, shared chart,
STAB, species queries and the type displays described below. Revelation Dance
can use a custom first type, which is shown in its move preview even when
other dynamic move previews are disabled. If a custom type has no dedicated Z- or Max Move,
the engine uses its existing generic fallback. Before the physical/special
split, custom types use the move's own category unless a type-category rule
selects one. Stellar boost usage is tracked
for the full runtime slot range.

These assignments are derived from the active modules, not written to a
Pokémon's save data. An optional module that is removed therefore leaves the
original species types in place on load; required-module checks still apply.
Adding a type whose ID sorts earlier can change runtime slots without changing
the resolved stable assignment. Runtime slot numbers must never be persisted.
Version 1.6 adds `species_list`, `battler_list`, `species_proposal_list` and
`offer_species_types_list`, using `RhhExtendedTypeList`. Offers contain one to
`RHH_ACTIVE_TYPE_LIMIT` distinct, resolvable keys; invalid or duplicate keys
reject the entire offer. Proposals use the same event stages and sticky-denial
rules as the three-type API. A legacy offer replaces the whole proposal with
its one to three types. Legacy queries and proposal reads fail without changing
their output when the complete list has more than three entries.

Version 1.7 adds `RHH_TYPE_STAB_EXTENDED` and `stab_context_list`, exposing
the complete active and pre-Terastallization lists in
`RhhExtendedTypeStabContext` for both battle and AI calculations. The legacy
`RHH_TYPE_STAB` event runs first when both lists fit its three-type context;
the extended event then receives the accepted legacy multiplier as its base.
An invalid legacy value falls back to the engine value, and a denial remains
sticky across both events; an invalid extended value retains the accepted
earlier value. When either list exceeds three types, only the
extended event runs. Constraints cannot rewrite the multiplier, and the same
0.5x–2.25x bounds apply.

Version 1.8 adds `RHH_MOVE_BASE_TYPE` for changing an existing move's base
type to a built-in or registered custom type. The event's `event_key` is the
move key and `subject` is its original type key; `decision.value` starts with
that original key. Transforms and finalizers can propose a resolvable type key;
constraints cannot rewrite it, and invalid or denied proposals use the
original type. Denial is sticky. Queries are repeated across combat, AI and
presentation, so hooks must be pure and consistent for a given move and run.
The host restores gameplay RNG and discards RNG-consuming proposals.
Recursive lookups use the original type. Struggle and placeholder types are
excluded, as are link and recorded battles. Ordinary weather, item, ability
and other dynamic type changes are applied after the base type. Custom types
use the move's configured category under pre-Gen IV physical/special rules
unless a type-category rule selects one;
their TM inventory palette falls back to the mystery palette. The bag lists
their type name as text. Assignments are derived from active mods and do not
store temporary type slots in a save. This event does not register new moves.

`rhh:service/moves` version 1.8 exposes copied metadata for existing and registered moves
and the resolved level-up, teachable and egg learnsets. `info` resolves a
move key and returns its canonical move and effective base type keys, power,
accuracy, PP, priority, category, target and the declared subset of move
flags. An accuracy of zero means always hit; power zero and one can have
special effect-dependent meanings. Dynamic weather, item and ability type
changes are not included. A nested `info` call from a move base-type hook
returns the original type, as with other recursive base-type lookups.
`learnset_count` and `learnset_at` accept an enabled species key and a
`RHH_LEARNSET_*` kind, with the resolved order and surviving duplicates. Level-up
entries include their level, including level zero evolution moves; other
kinds use zero. Invalid keys, kinds, indices or output sizes fail without
changing the output. The returned lists include active mod additions and
entry-resolution hooks.
`name` copies a move's display name as UI-compatible ASCII; unsupported
engine glyphs become `?`. Invalid keys or insufficient capacity leave the
output untouched. Registered move names are available before slot assignment.

`RHH_LEARNSET_ENTRY` lets modules filter and order the merged engine and mod
learnsets. The event identifies the species and move by stable key, gives the
kind, level, original merged ordinal and whether the entry came from an engine
list or an addition. Transforms and finalizers may propose a 32-bit ordering rank; constraints
can deny an entry, with denials remaining sticky. Equal ranks preserve merged
order. Level-up lists stay ordered by level, so ranks change order only within
the same level; teachable and egg lists can be reordered throughout. Invalid
ranks keep the original ordinal. Existing engine duplicates are retained unless
individually denied; duplicate additions are merged before hooks run.

The engine and the move service use the same resolved lists for learning,
breeding and presentation. Resolution is pure and cached for the run; hooks must
not depend on battle state, time or invocation count. Recursive learnset queries
inside a hook see the original engine list. Link and recorded battles bypass
the entry hooks. If the engine's byte-sized list limit is exceeded, engine
entries are retained ahead of additions; filtering happens before that limit is
applied. If the surviving engine entries alone exceed the limit, resolution
falls back to the original engine list. The `native/tests/learnset_filter.py`
regression removes its test build and temporary saves afterward.

`RHH_MOVE_ADDITIONAL_EFFECTS` collects extra effects for existing and
registered damaging moves using the engine's ordinary hit script. In a
constraint callback, call `offer_additional_effect` with a copied description
of a supported status, stat-stage change, weather or terrain effect. The event
key is the stable move key; `args[0]` is the inherited effect count and
`args[1]` is the initially available capacity. Existing effects remain first;
accepted additions follow deterministic hook order. The engine supports at
most seven effects total. An offer reports invalid input or full capacity;
denying the collection discards all additions. Status, weather and terrain
effects target the struck Pokémon; stat effects can instead target the user.
Chance is 1–100 percent. Stat changes specify a non-HP `RHH_STAT_*`,
`RHH_ADDITIONAL_ACCURACY` or `RHH_ADDITIONAL_EVASION` and 1–6 stages. Ice
weather follows the engine's hail/snow setting.

The engine applies the resulting effects through its normal hit script,
including its immunity, substitute, ability, animation and message rules. AI
reads the same effect list. Collection is a pure, run-stable definition: it is
cached for the active run and reset on run or module lifecycle changes, so it
must not depend on a particular target or turn. Nested lookups use the original
move, and link and recorded battles bypass additions. These are compositions
of existing engine effects; they do not register arbitrary new effect scripts.

`RHH_MOVE_RESOLVED` is an observer notification at final move cleanup, after
the engine has processed the move's targets and strikes and before it clears
their result values. Follow-up Dancer and pursuit processing can still occur
after this boundary. `resolution_context` copies the battle and run IDs, turn,
attacker and last target slots, resolved and chosen move keys, and semantic
result flags for each battler slot. The event key is the resolved move;
`args` are attacker, last target, flags and turn. Its sequence is separate
from battle lifecycle notifications and resets for each battle. An unknown
chosen move is zero. `UNABLE` means the engine could not use the move;
`CHARGING` identifies a charge turn and `ATTACKER_REPLACED` indicates the
attacker slot changed Pokémon during resolution.

The last-target field is the engine's cleanup context, not proof of a hit.
The per-slot flags are the engine's values at cleanup. A zero flag does not
prove a slot was targeted or hit, and `last_damage` is the engine's last signed
value rather than accumulated damage. Switches and replacements can change a
slot's occupant. The notification is not emitted by AI simulations, link or
recorded battles, and cannot be guaranteed if battle execution ends before
cleanup. Observers may update their own module state or use the battle-condition
setter described above; general gameplay commands remain unavailable.

Move service version 1.30 adds `resolution_damage` during this callback. For
each battler slot it copies the accumulated HP and Substitute damage observed
by the engine's direct move-damage handler, plus the number of those updates.
This can include multiple hits and capped knockout damage; it is not a complete
account of indirect damage, healing or every possible hit. A zero result with
no recorded flag means no such update was observed, not that the move missed.
Mixed moves or attackers, an attacker replacement, or a changed target identity,
owner, slot or species are flagged and return zero totals and update count.
The checks use OT/personality and can miss an identical replacement. The data
is cleared at move cleanup or battle reset. Query failures, including calls
outside this callback or with an invalid slot, leave the output unchanged.

Version 1.35 adds `resolution_health` during the same callback. For each slot,
it reports actual capped HP lost and restored by the direct move-damage handler
and by scripted HP updates from target setup until cleanup. These are separate
counts and totals; Substitute damage remains in `resolution_damage`. Scripted
updates may be caused by an ability, item or other effect, so their presence
does not prove the move caused them. Other HP mutation paths and changes before
target setup are not included. Changed participants or a different move flag
ambiguous totals and clear the counts, using the same non-permanent participant
checks as the damage summary. A zero record does not prove HP was unchanged.
Version 1.36 also records the engine's attacker-HP-to-zero script command as a
scripted loss, covering moves such as Memento, Healing Wish, Final Gambit and
Explosion when it runs during the observation window. Repeating the command at
zero HP does not add an update. Other unobserved HP mutation paths and changes
before target setup still prevent this from being a complete health audit.

Version 1.37 adds `resolution_health_comparison`, which copies each available
slot's HP and maximum HP at target setup and cleanup. When the actor, move and
recipient remain comparable, it reports both the endpoint HP change and the
net change from recorded updates, and flags a difference. Missing endpoints,
changed participants, mixed moves or overflow prevent comparison. Matching
net changes do not prove complete coverage: unobserved changes can cancel,
and the participant checks can miss an identical replacement. This is a
diagnostic view of the same window, not attribution of damage or healing.

Move service version 1.38 adds `RHH_MOVE_BASE_PRIORITY`, a pure transform of
the move's base priority for existing and registered moves. The event key is
the move key. `args[0]` and the initial `decision.value` encode the original
priority by adding `RHH_MOVE_PRIORITY_BIAS` (7), so 0 represents -7, 7
represents 0 and 14 represents +7. Transforms compose in hook order;
constraints can deny but cannot rewrite the value. Invalid, denied or
gameplay-RNG-consuming proposals retain the original, with both RNG streams
restored. Recursive priority queries return the original value. Rules must be
stable throughout the run, including for AI queries.

The move-info service and engine priority getter share this rule, including
registered moves before they have a saved slot. Turn order, AI priority
comparisons and rules that read the move's base priority see the same result.
The engine still applies its battler-specific ability, field and Quash rules
afterward. Struggle and link or recorded battles use their original priority.

Move service version 1.39 adds `RHH_MOVE_BATTLER_PRIORITY` and
`battler_priority_context` after the engine's ordinary ability and terrain
adjustments. The copied context includes the effective and chosen move keys,
battler, species, HP and level, underlying weather and terrain, base and
engine priorities, run and battle IDs, and the ability passed by the caller.
The latter is an AI assumption when the AI is calculating; a zero key can
mean no ability or unknown knowledge. The species and HP are the engine's current battler values, including
its simulations, rather than an information-hiding view. Ability queries that
would reveal the actual hidden ability remain unavailable to this pure hook.

The proposed value uses the same bias of 7, with 0 through 17 representing
-7 through +10. Invalid, denied, recursive or gameplay-RNG-consuming
proposals retain the engine result. Quash and Struggle bypass the new hook.
Max Guard follows the engine's existing precedence and is identified in the
context, with its own effective move and base priority. Changing priority does
not grant Prankster's additional ability-specific behavior. Repeated queries
can observe changed battle state; the hook does not lock a result for a turn.
Move metadata remains the base view. Link and recorded battles bypass both
priority transforms.

Version 1.40 sets `RHH_MOVE_PRIORITY_AI` for direct AI priority queries and
queries made while the engine's AI damage simulation is active. This flag
identifies confirmed predictions; its absence does not prove that a shared
calculation is live battle execution. Mods must give equivalent priority
results for equivalent battle and AI inputs rather than changing mechanics
based on this flag. The AI's supplied ability remains an assumption.

Move service version 1.41 adds the pure `RHH_DAMAGE_STAT` proposal and
`damage_stat_context` for the effective Attack and Defense used by ordinary
damage. The engine first calculates each value with its usual stat stages,
abilities, items and field effects; the host then proposes Attack and Defense
in that order immediately before the base damage formula. Battle and AI use
the same hook. The copied context includes the move, type, calculation ability
assumptions, weather, terrain, effective power and the chosen stat owner, raw
stat and stage-stat identities. These distinguish Foul Play, Body Press,
Psyshock and Wonder Room. The AI flag and species are calculation snapshots,
not an information-hiding boundary. Fixed-damage effects bypass this phase.

Accepted values are positive and no greater than the context's `max_value`.
For Attack this maximum depends on power and level to avoid overflowing the
engine's next multiplication; Defense accepts a 32-bit positive value. An
invalid, denied or RNG-consuming proposal retains its engine input, even if
that input itself is outside the proposal bounds. Constraints may deny but
cannot rewrite. Both gameplay RNG streams are restored. Link and recorded
battles bypass these hooks. Mods should return consistent results for
identical real and AI inputs and should use the self-inflicted flag if their
mechanic excludes confusion damage.

Move service version 1.9 adds `RHH_AI_MOVE_SCORE` and `ai_score_context` so
mod-defined mechanics can influence actual AI move selection. After built-in
scoring, eligible scores from 1–255 for moves with PP and without a move
limitation are passed to transforms. Proposals must be in the range 0–255;
invalid or denied proposals retain the engine's score. Constraints can deny
but cannot rewrite a proposal. The hook cannot revive a score already reduced
to zero, bypass a move limitation, or change an action that does not attack.

The event key is the move; the subject is the attacker's species; `args` are
the score entering the transforms, attacker, AI evaluation candidate, and move slot. The
copied context adds stable species and move keys, run and battle IDs, turn,
and the target kind derived from the AI's current knowledge. A candidate is the battler being scored,
not necessarily the move's actual target: self, side, field, and spread moves
may be evaluated against several candidates. The AI may also score a partner's
simulated move. Callbacks must be pure and use read-only queries such as
`condition_state`; the context is available only during its own event, and
link and recorded battles bypass the hook.

Move service version 1.20 adds `ai_evaluation` during `RHH_AI_MOVE_SCORE`.
It copies the engine's cached minimum, median, maximum and previously rolled
damage estimates, accuracy, type effectiveness, HP percentages and known
abilities for the attacker and candidate. Effectiveness uses `RHH_TYPE_ONE`;
accuracy 101 denotes an accuracy-check bypass. The cache is the AI's current
estimate, not a guaranteed outcome: zero values can also mean the engine has
not populated an entry. Ability key zero means no known ability or no ability;
the query never substitutes the target's hidden actual ability. Flags report
the user's Heal Block, a candidate's substitute, and partner-move simulation.
The query copies into caller-owned storage, consumes no RNG, and fails without
changing the output outside the score event, in a nested event or with an
undersized output.

Move service version 1.21 adds `RHH_AI_EFFECT_PREVIEW`, a pure constraint
phase before `RHH_AI_MOVE_SCORE` for moves currently using a custom effect.
The event key is the move, subject is its registered effect key or zero, and
the arguments match the score event. `ai_score_context` and `ai_evaluation`
are available in this phase. Mods offer up to eight copied `RhhAiEffectAction`
descriptions in hook order; a denial discards the entire list. Invalid offers
and a full list do not change it. The accepted list is readable by any score
hook through `ai_effect_count` and `ai_effect_at`, and those queries fail
outside that event or inside a nested event. Descriptions are proposals, not
commands: the host does not execute them. They only change AI scores when a
version 1.22 action explicitly opts into automatic scoring as described below.
They may describe conditional or chance-based effects and do not guarantee a
hit, target, or state change. A field-target move cannot offer target actions,
and damage-based healing or drain requires a custom damaging effect.

Each description has a `chance` from 1–100. For target/user healing,
damage-based healing and drain, `first` and `second` are the 1–16 numerator
and denominator; the latter two accept their corresponding include-substitute
flag. For status, `first` is an `RHH_EFFECT_*` value; for a stat change,
`first` is an `RHH_STAT_*` or `RHH_BATTLE_*` stat and `stages` is -6..-1 or
1..6. For a condition, `condition` is its registered key, `first` is the
condition target and `second` is the requested duration, including zero to
remove it. Other fields must be zero. These descriptions follow the same
basic bounds as the committed commands, but do not guarantee that engine
rules will permit an action.

Move service version 1.22 adds `ai_effect_estimate(index, recipient, out)` in
the score event. It returns a conditional, per-action estimate before the
action's chance and move accuracy: signed HP or stat-stage deltas, 0 or 1 for
a status, or a signed change in condition duration. Use the attacker for
user-only actions and `RHH_AI_EFFECT_NO_BATTLER` for conditions; for target
actions provide the battler to evaluate. Invalid indices, recipients, scopes,
and undersized outputs fail without modifying the output. The range comes
from the AI's cached damage when applicable. `BLOCKED` means blocked under
the estimator's assumptions, `UNCERTAIN` indicates an approximate result,
`DAMAGE_CACHE` identifies use of cached damage, `SUBSTITUTE` identifies a
modeled substitute, and `CONDITION_SEMANTICS_UNKNOWN` means a duration change
has no declared benefit or harm. Unknown ability and item information is not
replaced with hidden actual knowledge. Substitute HP is not known, so its
damage-based range includes zero. An estimate is isolated: it does not apply
earlier offered actions, run committed callbacks, or simulate later turns.
Status immunities and other battle rules are only partially modeled; even a
blocked result marked uncertain can differ from the real outcome.

Set `RHH_AI_EFFECT_AUTO_SCORE` on an offered action to opt into the host's
approximate score adjustment before score transforms. The host uses the
median estimate and side, weighs it by chance and cached accuracy, then sums
all opted-in adjustments and clamps the result to 0–255. A missing or
accuracy-bypassing cache value is treated as 100%. Positive healing for an
ally is rewarded below 70% cached HP; other healing, drain damage, status and
stat changes are scored according to the affected side. Conditions are never
automatically scored because their benefit is unknown. User-and-ally moves
consider both live battlers, user-or-ally assumes the user, and other target
actions use the AI candidate or ally as appropriate. These are independent
action heuristics, not a complete battle simulation or a guarantee of target
selection. A resulting zero cannot be revived by a later transform. The
preview receives the original score; score hooks receive the score after
automatic adjustments. Strip `RHH_AI_EFFECT_AUTO_SCORE` when passing the
flags to a committed healing or drain command; it is a preview-only flag.

Move service version 1.10 provides a neutral script for mod-defined status
effects. `RHH_MOVE_EFFECT_MODE` transforms a move's mode between
`RHH_MOVE_EFFECT_INHERITED` and `RHH_MOVE_EFFECT_CUSTOM_STATUS`. The event key
is the stable move key and the initial decision is inherited unless the move
has a registered effect binding described below; invalid or denied proposals
select inherited behavior. An RNG-consuming proposal retains the initial mode,
including a registered binding; an independent denial still selects inherited
behavior. Both gameplay RNG streams are restored. Only status moves without inherited
additional effects are eligible. The callback must be pure and stable for the
run. `effect_mode` previews the same decision for built-in or registered moves,
including custom moves without an allocated runtime slot. Nested mode queries
and link or recorded battles retain the inherited effect.

For status moves, the neutral script performs the normal attack cancellation and PP handling,
plays the inherited move animation, then delivers `RHH_MOVE_EFFECT_APPLY`
once for each valid target before that target's move-end processing. The
callback receives a copied `effect_context` with move keys, attacker, target,
turn, target kind, run and battle IDs, and a per-battle sequence. Its event
key is the move; arguments are attacker, target, flags and turn. The
`RHH_MOVE_EFFECT_FIRST_TARGET` flag identifies the first delivered target, so
a side or field effect can be applied once while individual target effects
can use every callback. For a field-target move, the target is the engine's
placeholder battler rather than a recipient. Failed, interrupted or unavailable
targets receive no callback. The effect observer may change module state and use
`condition_set`; general gameplay commands remain unavailable. This is a
callback-based status effect rather than a general battle-script or arbitrary
target registration API.

Move service version 1.11 adds `effect_message` for committed effect
observers. It copies a 1–120 byte NUL-terminated message into a queue of up to
eight messages for the current target. Messages from observers appear in hook
order after the effect and before move-end processing, using the normal battle
message window and wait. Failed or interrupted targets do not run the hook.
The queue is cleared for each target and on battle reset; a full queue rejects
new messages without replacing earlier ones. Calling from other callbacks,
including callbacks nested inside the effect observer, fails.

Text accepts ASCII letters, digits, spaces, `! ? . - : , / + ( ) % ' " & ; =`
and at most one newline. Other characters and empty or overlong strings are
rejected. Spaces and newlines must separate nonempty words: leading, trailing,
and consecutive whitespace is rejected. A word wider than the battle window,
or a manually broken line that is too wide, is rejected. Text without a manual break uses the game's normal
wrapping and scrolling. `effect_message` is limited to effect observers;
expiry observers use `condition_expiry_message`. There is no general
battle-message API.

Move service version 1.12 adds `effect_heal` for a committed effect observer.
It queues healing actions for the current
target, in hook order. Healing, status and stat actions share a limit of eight per
target. Each request supplies a numerator and
denominator with `1 <= numerator <= denominator <= 16`; the amount is that
fraction of the target's non-Dynamax maximum HP, rounded down with a minimum
of one. Requests are copied and return success when queued, not when HP is
actually restored. Field-target effects cannot queue healing.

After all observers finish, the host processes actions through the normal
health-bar and Pokémon-data controllers before displaying effect messages.
Each action rechecks the battle, target and attacker; fainted or full-health
targets and healing blocked by the attacker's Heal Block are skipped. The
engine caps HP at the target's current maximum. Use a healing move as the
registered move's template so its inherited healing flag also prevents
selection under Heal Block. Mods should account for their healing in an
`RHH_AI_MOVE_SCORE` hook; the host does not simulate observer callbacks for
AI.

The move-effect action queue also records the battle, move and the source and
recipient's trainer, party slot and OT/personality identity. For non-field
moves it records the original target too. Actions are skipped if these no
longer match, including while a participant is pending a switch; a field
move does not require the original target slot to remain occupied. Changes to a Pokémon's form
alone do not invalidate the action. These checks cannot distinguish an exact
identity duplicate replacing the same party slot.

Move service version 1.13 adds `effect_status` for a committed effect
observer. It queues sleep, poison, burn, freeze, paralysis, toxic poison or
confusion for the current target. Status, stat and healing actions execute in the
order they were queued, after all observers return; a full queue rejects new
actions. Field-target effects cannot queue them. Queue success means the
request was accepted, not that it will take effect.

The engine applies each status as a primary move effect, using its normal
Substitute, terrain, Safeguard, type and ability checks. A successful effect
uses the usual status animation, battle text, party update and subsequent
Synchronize and item handling. A blocked request is skipped silently. Failed,
protected, absent and interrupted targets receive no effect callback. As with
healing, these observer callbacks are not simulated by the AI; use
`RHH_AI_MOVE_SCORE` to describe the effect.

Move service version 1.14 adds `effect_stat` for a committed effect observer.
It queues an Attack, Defense, Speed, Special Attack, Special Defense, accuracy
or evasion change for the current target. The signed amount is -6 through -1
or +1 through +6; HP and zero are invalid. The request shares the eight-action
queue with status and healing. The engine resolves it through its normal stat
scripts, including stage limits, Substitute, Mist, Contrary, Simple, Clear Body
and reflected stat drops. Queue success does not guarantee a change. If an
unrelated engine stat-resolution queue is still busy when the action reaches
execution, the host skips it rather than consuming that queue. Field-target
effects cannot queue stat actions.

Move service version 1.15 adds registered, reusable status effects. A module
using `RHH_MOD_ABI_MOVE_EFFECTS` may declare up to 16 `RhhModMoveEffect`
entries and 64 `RhhModMoveEffectBinding` entries. An effect has a stable
`namespace:effect/name` ID, label and `RHH_MOVE_EFFECT_CUSTOM_STATUS` mode;
bindings reference effect and move keys. The host limits the whole catalog to
128 effects and 512 bindings. An effect may be shared by several built-in or
registered moves and by separate modules. Each move has at most one binding;
conflicts are rejected rather than resolved by load order.

Finalization rejects missing effects, unknown or noncanonical moves, and moves
that cannot use the neutral status script. Eligible moves must be status moves
without inherited additional effects, with a valid non-placeholder template;
Struggle is excluded. Binding declarations and definitions may be loaded in
either order, but a missing provider prevents finalization. Bindings use stable
keys rather than saved runtime slots. When the corresponding mods are absent,
ordinary moves retain their built-in effect and missing custom move keys retain
the existing missing-content behavior.

For a bound move, the mode transform starts at `CUSTOM_STATUS`; transforms may
select `INHERITED`, and a denied or invalid result falls back to inherited.
Link and recorded battles use the inherited effect. `registered_effect_count`,
`registered_effect_at` and `registered_effect_info` expose copied definitions
in key order after finalization; `registered_effect_for_move` returns the bound
key or zero for an unbound valid move. Errors leave output arguments unchanged.
The binding query describes the declaration, even if a mode transform later
disables it for the active run.

When a bound move uses the neutral script, the host sends
`RHH_REGISTERED_MOVE_EFFECT` once per valid target before the existing
`RHH_MOVE_EFFECT_APPLY` event. Its `event_key` is the effect key, `subject` is
the move key, and its arguments and `effect_context` match the move-keyed event.
All registered-effect observers run in normal hook order and may use
`condition_set`, `effect_message`, `effect_heal`, `effect_status` and
`effect_stat`; their actions compose with the subsequent move-keyed observers.
The declaration and binding do not supply mechanics by themselves: a module
must observe the registered effect and queue the intended actions.
Failed, protected, interrupted or unavailable targets receive neither event.
Mods supply their own AI scoring; callbacks are not simulated.

Move service version 1.16 adds `effect_heal_user`, which queues healing for
the move's user even when the current effect target is another battler or a
field. It uses the same fractional amount, validation, eight-action queue,
Heal Block rules and controller updates as `effect_heal`. It is available only
in committed effect observers. The request runs once for each callback that
queues it; for spread moves, use `RHH_MOVE_EFFECT_FIRST_TARGET` if the user
should be healed only once. As with other queued actions, success means queued,
not guaranteed, and the AI does not simulate the callback.

Move service version 1.26 adds `effect_heal_battler` for a specified living,
present battler, including a battler other than the user and current target.
Request version 1.26 and check `RHH_MOVE_SERVICE_V1_26_SIZE`. It is available
in committed effect observers, including field effects, and shares the same
fraction bounds and eight-action queue. It rechecks presence, HP and Heal Block
when the action executes. The user and recipient must both be free of Heal
Block, matching the engine's targeted healing scripts; this also applies to
`effect_heal`. The engine updates the chosen battler's HP without changing the
current move target or scripting battler. For spread moves, each observer call
can queue an action, so use the first-target flag to avoid repeated healing if
that is the desired rule. AI callbacks remain separate from committed effects.

Move service version 1.27 adds `ai_effect_offer_heal_battler` for a specified
living battler during `RHH_AI_EFFECT_PREVIEW`, including previews of field
moves. It takes the same fraction as the committed command, a chance from 1 to
100, and optional `RHH_AI_EFFECT_AUTO_SCORE`. It shares the ordered eight-action
preview limit. During `RHH_AI_MOVE_SCORE`, `ai_effect_at` reports the new
`RHH_AI_EFFECT_HEAL_BATTLER` kind and fraction; `ai_effect_heal_battler_at`
copies its recipient, leaving the output untouched on failure. The existing
`ai_effect_estimate` accepts that recipient and rejects a different one. Its
estimate respects Heal Block and, when the recipient is the damaged candidate,
the cached damage range. Automatic scoring uses the specified recipient once,
even for a move that ordinarily affects multiple battlers. This remains an
approximation, not a simulation of combined actions. The original action
structure and older service sizes are unchanged; consumers should ignore action
kinds they do not recognize.

Move service version 1.28 adds `effect_damage_battler` for indirect damage to a
specified living battler. It queues a fraction of that battler's non-Dynamax
maximum HP, rounded down with a minimum of one; the normal controller update,
faint processing and HP-threshold item check run when the action executes.
It bypasses Substitute and does not count as a direct move hit. Magic Guard
prevents the damage unless `RHH_EFFECT_DAMAGE_IGNORE_MAGIC_GUARD` is explicitly
requested. The recipient is rechecked when the action executes; a fainted or
absent recipient is skipped. It shares the eight-action queue, does not change
the current move target, and can be requested from field or per-target effect
observers. Spread observers should use the first-target flag when the damage
should occur once.

`ai_effect_offer_damage_battler` offers the corresponding fractional action
during `RHH_AI_EFFECT_PREVIEW`; `ai_effect_damage_battler_at` copies its
recipient during scoring, and `ai_effect_estimate` reports signed HP loss for
that recipient. The estimate uses the AI's cached ability knowledge for Magic
Guard, marks unknown abilities as uncertain, and accounts for cached main-hit
damage if the recipient is the candidate. Optional automatic scoring considers
the selected recipient once. The estimate is approximate and does not simulate
subsequent healing, items or combined queued actions. Request version 1.28 and
check `RHH_MOVE_SERVICE_V1_28_SIZE` before using these functions.

Move service version 1.47 adds `effect_damage_from_hit`. A committed damaging
effect observer may queue indirect damage to a chosen living battler, based on
the current hit's recorded, capped HP loss. The fraction is 1..16, rounded
down with a minimum of one for nonzero damage. Substitute loss is excluded
unless `RHH_EFFECT_DAMAGE_FROM_HIT_INCLUDE_SUBSTITUTE` is requested. A recorded
zero queues no HP loss; an unavailable hit snapshot rejects the request.
It uses the same participant checks, Magic Guard option, faint and item handling
as `effect_damage_battler`. It is not the engine's recoil mechanic and does
not apply recoil-specific abilities or items. Multi-hit callbacks follow the
existing effect notification timing, not an aggregate of all hits.

`ai_effect_offer_damage_from_hit` describes this action for a damaging effect
in the AI preview, and `ai_effect_damage_from_hit_at` identifies its recipient.
`ai_effect_estimate` uses cached candidate damage and cached Magic Guard
knowledge; it marks the result uncertain, with a conservative range when the
recipient is also the candidate or a Substitute is involved. The preview does
not predict later actions or guarantee a hit. Request version 1.47 and check
`RHH_MOVE_SERVICE_V1_47_SIZE` before using these functions.

Move service 1.48 exposes `RHH_MOVE_CRITICAL_STAGE` and
`critical_stage_context` for ordinary critical-hit chance in battle and AI.
The copied context includes the engine value, its probability, generation,
participants, field state and the caller's ability assumptions. Gen I values
are thresholds from 0 to 255 out of 256; Gen II uses stages 0 to 4 with a
generation-specific numerator out of 256, and later generations use stages
0 to 4 with a one-in-N denominator. Generation 10 denotes Champions. Forced
and blocked criticals, self-inflicted damage, tutorials, link and recorded
battles bypass the phase. Rejected, out-of-range or RNG-consuming proposals
retain the engine value, and both RNG streams are restored.

Ability service 1.37 adds `critical_stage_innates` during the move phase and
`innate_critical_stage_context` during `RHH_INNATE_CRITICAL_STAGE`. Usable
attacker innates run first, followed by defender innates, using each caller's
ability knowledge and suppression rules. Each bounded pure proposal composes
with earlier proposals; a rejected or RNG-consuming entry is skipped. Request
the corresponding service versions and check `RHH_MOVE_SERVICE_V1_48_SIZE`
and `RHH_ABILITY_SERVICE_V1_37_SIZE` before using them.

Move service 1.49 adds `effect_queue`, a copied view of the shared action and
message counts and their effect-observer limits. It is available only to
committed move-effect observers, including registered and damaging effects.
The counts include earlier observers' offers; they are a point-in-time view,
not a reservation or a guarantee that an action will pass validation or execute.
Other reaction paths can share the message queue with a larger limit, so its
current count may exceed the effect-observer limit. The query fails without
modifying its output outside the applicable observer or for a short buffer.
Request version 1.49 and check `RHH_MOVE_SERVICE_V1_49_SIZE` before use.

Move service 1.50 adds `effect_queue_at` to copy an ordered request by index
during the same observer. It reports its public action kind, source, subject,
original target and whether the target is rechecked, as well as the relevant
fraction, stat stages, status, field rule, animation and flags. For effects
based on a recorded hit, `amount` reports the captured calculation input or
computed amount as specified by the public structure. This lets independent
observers identify overlaps and conflicts; it does not reveal which module
offered a request, reserve capacity, cancel requests or guarantee execution.
Short buffers, invalid indices and calls outside the observer fail without
changing output. Request 1.50 and check `RHH_MOVE_SERVICE_V1_50_SIZE`.

Move service 1.51 exposes `RHH_DAMAGE_FINAL` and `final_damage_context` for
ordinary damage after the random roll, STAB, type and engine modifiers, before
the minimum-damage rule and later survival or Substitute adjustments. Each
AI roll is evaluated separately using its caller's cached ability assumptions.
With move service 1.53 and ability service 1.40, native non-Z Gen V-and-later Beat Up
AI estimates apply the phase separately to each simulated hit and roll. When
the attacker is AI-controlled and the target is not, they use its own party;
otherwise they use recorded party knowledge and estimate unknown contributors using the active
battler's apparent species. Those estimates omit known fainted or observed
statused contributors. Unknown conditions,
eggs and other unobserved changes can make the estimate differ from battle.
Request these versions and check `RHH_MOVE_SERVICE_V1_53_SIZE` and
`RHH_ABILITY_SERVICE_V1_40_SIZE` when relying on Beat Up AI participation.
The context's species keys are authoritative
battle data, not an AI information-hiding boundary.
Fixed damage, type immunity and zero inputs bypass the phase. Accepted values
are 1–65,535; a denied, out-of-range or RNG-consuming base proposal retains
the engine value as the starting point for innate contributions. Both
gameplay RNG streams are restored. This is a calculation, not a notification
that a hit occurred.

Ability service 1.38 adds `final_damage_innates` during that calculation and
`innate_final_damage_context` during `RHH_INNATE_DAMAGE_FINAL`. Usable
attacker innates run first, then defender innates; each accepted bounded pure
proposal composes with the previous value. A rejected or RNG-consuming innate
does not cancel later entries. Link and recorded battles are excluded. Request the corresponding service versions and check
`RHH_MOVE_SERVICE_V1_51_SIZE` and `RHH_ABILITY_SERVICE_V1_38_SIZE`.

Move service 1.52 and ability service 1.39 add separate opt-in
`RHH_SELF_DAMAGE_FINAL` and `RHH_INNATE_SELF_DAMAGE_FINAL` events for confusion
and disobedience self-damage. They use the same context queries, proposal
bounds, ordering and purity rules, after the existing unrandomized formula and
before minimum damage. The context is flagged as self-inflicted, visits the
battler's innates once, and can have zero move or move-type keys; it does not
distinguish the cause. Existing ordinary damage hooks do not receive these
events. Request the new versions and check `RHH_MOVE_SERVICE_V1_52_SIZE` and
`RHH_ABILITY_SERVICE_V1_39_SIZE`.

Move service 1.54 adds `beat_up_contributor` during Gen V-and-later Beat Up
power, damage-stat and final-damage reducers, including their innate variants.
It copies the contributing species, zero-based eligible-hit index and attacker
and defender. Actual hits include the party slot captured during move setup.
AI calculations identify their own-party, apparent-species and estimated
sources with flags; an estimated unseen slot is reported as
`RHH_BEAT_UP_CONTRIBUTOR_NO_SLOT`. Recorded AI knowledge and apparent species
may be stale or disguised. This is a calculation snapshot, not a committed hit
or a permanent Pokémon identity. The query fails without changing output when
the contributor is unavailable, outside the listed callbacks or for fixed
damage and earlier-generation Beat Up. Request 1.54 and check
`RHH_MOVE_SERVICE_V1_54_SIZE`.

Move service version 1.31 adds `effect_stat_battler` for a specified living
battler, even when the move targets a field or another battler. It queues a
stat-stage change using the same eight-action limit and the engine's normal
stat scripts and rules. The recipient is rechecked at execution, and the
move's current target is preserved. Spread-move observers should use the
first-target flag when they intend to queue the action once.
`ai_effect_offer_stat_battler` describes the action during the pure AI preview;
`ai_effect_stat_battler_at` and `ai_effect_estimate` expose its recipient and
approximate stage change during scoring. Optional automatic scoring uses that
recipient's side. The estimate uses cached ability knowledge and cannot predict
every immunity or later battle change. Request version 1.31 and check
`RHH_MOVE_SERVICE_V1_31_SIZE` before using these functions.

Move service version 1.32 adds `effect_status_battler` to queue an existing
`RHH_EFFECT_*` status for a specified living battler. It uses the shared
eight-action queue and the normal engine status, Substitute, immunity,
controller and presentation handling; it rechecks the recipient at execution
and keeps the move's target intact. It also works for field effects and
damaging-effect observers. `ai_effect_offer_status_battler` describes the
possible action during AI preview, `ai_effect_status_battler_at` retrieves
its recipient, and `ai_effect_estimate` estimates that recipient's status
outcome using cached ability knowledge. Optional automatic scoring considers
the recipient's side. As with other previews, unknown protections and future
actions can make the estimate differ from the outcome. Request version 1.32
and check `RHH_MOVE_SERVICE_V1_32_SIZE` before using these functions.

Move service version 1.29 adds `effect_animation`, which queues a visual cue
for a living battler during a committed effect observer. It accepts the
documented `RHH_EFFECT_ANIM_*` choices and shares the eight-action queue with
healing, status, stat and damage actions. The recipient is checked again when
the action runs; unavailable recipients are skipped. The battle script waits
for the normal animation controller and honors the engine's animation-disable
setting. It does not change battle mechanics or define new animation assets. Request version 1.29 and check
`RHH_MOVE_SERVICE_V1_29_SIZE` before using this function.

Move service version 1.17 and `RHH_MOD_ABI_DAMAGE_EFFECTS` add registered
`RHH_MOVE_EFFECT_CUSTOM_DAMAGE` effects. Bindings may target physical or
special moves whose inherited engine effect is the ordinary hit script,
including moves with built-in additional effects. Finalization rejects
incompatible bindings. The engine retains the ordinary script, damage, AI
classification and additional effects. After those effects, it dispatches
`RHH_REGISTERED_MOVE_EFFECT` and then `RHH_MOVE_DAMAGE_EFFECT_APPLY` once
per successful target; the latter has the same context and command access as
the status event. For a multi-hit move this happens on the first qualifying
hit against each target, not after all hits. A knocked-out target still receives the notification, though
queued target actions skip fainted battlers. Missed, protected, immune, absent
and interrupted targets do not receive it. A user heal can run after a knockout.
`RHH_MOVE_DAMAGE_EFFECT_MODE` can opt an eligible move into or out of this
phase; it is separate from the status-mode hook. The decision must be pure and
stable for the run. Link and recorded battles bypass the phase. The host does
not treat the callback as a conventional chance-based secondary effect, so
Shield Dust and Sheer Force do not suppress the callback itself. Queued actions
still use their documented engine checks. The host does not simulate these
callbacks in AI; use the AI score hook for custom effects.

Move service version 1.18 exposes `effect_damage_context` in committed
damaging-effect observers. It copies the current hit's actual Pokémon HP loss
and substitute HP loss, each capped at the HP available before that hit.
`RHH_MOVE_DAMAGE_RECORDED` distinguishes a recorded zero from a hit for which
no engine HP update was available. The snapshot is taken before built-in
secondary effects and is shared by the registered and move-keyed observers.
It describes the first qualifying hit for each target, not aggregate damage
over a multi-hit move. The query fails outside a damaging-effect observer and
leaves the output untouched on failure.

`effect_heal_user_from_damage` queues healing based on that recorded loss.
The numerator and denominator are 1–16, with numerator no greater than the
denominator. By default only Pokémon HP loss counts; pass
`RHH_EFFECT_HEAL_INCLUDE_SUBSTITUTE` to include substitute loss. A positive
result rounds down to at least one HP; a recorded zero queues an action that
does no healing. Missing damage data, invalid arguments and a full action queue
are rejected. The usual user-healing checks apply, including Heal Block and
the current maximum HP. This is an HP-healing command; it does not implement
the engine's drain-specific Big Root or Liquid Ooze rules.

Move service version 1.19 adds `effect_drain_user` for engine drain behavior.
It uses the same recorded damage and fraction bounds, with
`RHH_EFFECT_DRAIN_INCLUDE_SUBSTITUTE` to count substitute loss. A recorded
zero does nothing; missing damage, invalid arguments and a full queue are
rejected. The action rechecks the user's presence and Heal Block. It then uses
the engine's absorb handler, including Big Root, Liquid Ooze, Magic Guard,
ability suppression, HP controllers, messages and faint handling. As with other
queued actions, acceptance is not a guarantee of an HP change. Mods must
account for the effect in their AI scoring; observer callbacks are not simulated.

`RHH_MOVE_BASE_TARGET` can replace an existing or registered move's base
target with another `RHH_MOVE_TARGET_*` kind. The event key is the stable move
key; the subject and initial `decision.value` are its original target. The
change is shared by move selection, battle execution, AI and `info` previews,
including registered moves without a saved runtime slot. Engine
effect-specific targeting still takes precedence. A denied or invalid
proposal retains the original; constraints cannot rewrite proposals. SMART
requires a move with more than one strike, and DEPENDS can only retain an
existing DEPENDS target. Use targets compatible with the inherited move effect
and animation. `RHH_MOVE_TARGET_USER_AND_ALLY` is the existing user-and-partner
mode; the inherited effect script must support that mode. Callbacks must be pure and stable across queries. Nested target
queries retain the original, and link and recorded battles bypass the hook.
The host restores both gameplay RNG streams and discards a proposal that uses
either stream, retaining the original target.

Move service v1.33 adds `RHH_MOVE_TARGET_RESOLVE` for targeting that depends
on the acting battler or the current battle. It runs after the engine has
resolved its selection, execution or AI target, and `target_context` copies
the battler, phase, initial target, strike count, visible underlying weather
and terrain, move and battle identifiers. The weather and terrain fields do
not account for ability suppression or a battler's grounding.
The event can select an existing target kind using the same SMART and DEPENDS
restrictions as the base hook. It does not register arbitrary target kinds or
change `info`, which remains the base metadata view. Mods must use a target
compatible with the move's effect and animation and return consistent
results across phases when their inputs agree. Without an explicit lock, a
rule that changes while the move is resolving can affect later target queries.

The AI phase starts with the AI's cached knowledge, so its initial target can
differ from execution when an ability is unknown. Hooks must be pure and
must not use hidden ability state to override that distinction. Invalid or
denied proposals, recursive queries, gameplay RNG use, link and recorded
battles retain the engine's target. The host restores both gameplay RNG
streams if a callback uses them. Constraints can deny but cannot rewrite a
proposal.
Move service v1.34 adds `target_lock` for an execution-phase resolver to
request that its accepted result remain fixed after the engine sets up that
move's targets. Call it for every relevant move, including when the result
equals the engine's initial target. It returns an error outside actual target
setup; selection and AI previews remain live. Invalid or denied proposals and
callbacks that use gameplay RNG cannot create a lock. Locks are scoped to the
battle, move and acting Pokémon and cleared at move cleanup or the next action;
different battlers can hold separate locks during reflected or snatched moves.
The actor check uses trainer, party slot and OT/personality, so an exact
identity duplicate in the same slot cannot be distinguished. A target lock
does not reserve living targets or stop normal redirection and failure checks.
Move service v1.42 adds `target_preview` to query the current target kind for
a living battler and move outside the resolver. Choose selection, execution,
or AI phase; the AI phase requires available cached AI state and follows its
knowledge of abilities. Execution previews calculate current rules without
using, creating or clearing a live target lock, so they can differ from a
move already in progress. The copied result includes the move and run and
battle identifiers. It does not select a recipient, test every recipient's
eligibility, or predict redirection or later state changes. Registered moves
need an existing runtime slot; the query does not allocate one. Invalid or
unsupported requests and calls from a target resolver fail without changing
the output. The query preserves gameplay RNG and engine item and ability
bookkeeping; callbacks must still obey their own purity requirements.
Request version 1.42 and check `RHH_MOVE_SERVICE_V1_42_SIZE` before use.
Version 1.43 adds `target_filter` for the engine's individual-candidate
check. It reports a copied allowed flag for a move, actor and candidate,
including the engine's alive, Heal Block and Dynamax restrictions and excluding
pending candidates. This is one filter used by selection and AI: it does not
apply the target kind or cursor rules, predict redirection or check hit and
immunity outcomes. Some cursor paths do not call this filter, so neither result
alone establishes the final target. It follows the same battle and
registered-move availability rules as `target_preview`; recursive target or
effect resolution and invalid requests fail with output unchanged. The query
preserves gameplay RNG and item and ability bookkeeping. Request version 1.43
and check `RHH_MOVE_SERVICE_V1_43_SIZE` before use.

Move service v1.45 adds the observer-only `RHH_MOVE_DAMAGE_APPLIED` event and
`damage_applied_context`. Each event copies one positive HP or Substitute loss
from the engine's direct move-damage handler, after the battle value changes.
It includes the acting and target slots, copied owner, party slot and
OT/personality identities, available species and move keys, remaining target
and Substitute HP, turn, run, battle and a battle-scoped event sequence. Keys
can be zero when unavailable, and identities can collide. An HP controller
update may still be pending; Substitute fading, hit bookkeeping, later effects
and move cleanup have not necessarily completed. This stream does not include
zero-damage hits, healing, indirect damage, every hit-like effect or a durable
receipt. It is unavailable in AI simulation, link and recorded battles.
Observer gameplay RNG is restored. The context exists only during the callback;
invalid requests leave outputs unchanged. Request version 1.45 and check
`RHH_MOVE_SERVICE_V1_45_SIZE`.

Version 1.46 adds the observer-only `RHH_MOVE_ACCURACY_CHECKED` event and
`accuracy_checked_context`. It copies each eligible target's actual result
immediately after the engine checks accuracy and sets the miss flag. The result
can come from a random roll, a guaranteed-hit rule or an OHKO-specific check;
the event does not distinguish these or expose a roll. OHKO and the engine's
raw OHKO result flags are identified separately. The copied context includes
the move and species keys when available, slot identities, result flags, turn,
run, battle and an independent battle-scoped sequence. The event key is the
move, its subject is the target species, and its arguments are attacker,
target, missed and the OHKO flags. The result alone does
not establish the final target, damage or effects of the move. Checks
the engine skips—including selected multi-hit continuations, Max and Z moves,
stat-change moves and skipped targets—do not emit an event. It excludes AI
previews, link and recorded battles; observers cannot change the result and
their gameplay RNG is restored. The context is available only during the
callback. Request version 1.46 and check `RHH_MOVE_SERVICE_V1_46_SIZE`.

`RHH_MOVE_POWER` transforms the effective base power of an existing or
registered damaging move after the engine's power modifiers and before the
damage formula. `power_context` copies the move, chosen and base move, type,
attacker and defender battler indices, species, abilities, levels and HP, and the weather
and terrain inputs for the current calculation. The weather input reflects
the engine's calculation and may differ from the visible weather when
suppressed or simulated. Use the public `RHH_WEATHER_*` and `RHH_TERRAIN_*`
values. Transform `decision.value` in the range 1 through
`RHH_MOVE_POWER_MAX`; invalid values or constraint denials retain the engine
value. Constraints cannot rewrite the proposal and denial is sticky. Callbacks
must be deterministic and side-effect-free: both real damage and the AI's
damage simulation use this phase. The AI, critical and fixed-base-power flags
describe the calculation; the host restores gameplay RNG and discards an
RNG-consuming general proposal without suppressing separate innate triggers.
Equivalent AI and real contexts must produce the
same result. Fixed-damage effects and Struggle do not use this hook; link and
recorded battles bypass it. The context is available only during the event,
and nested power resolution keeps the engine's value.

`RHH_MOVE_ACCURACY` transforms ordinary hit-roll accuracy after the engine's
weather, ability, item and stat-stage modifiers. `accuracy_context` copies
the effective move and type, battler indices, species, abilities, levels, HP,
and calculation weather; unavailable type or ability keys are zero. The
engine's starting value may exceed 100 before
the AI cap. Propose an integer percentage from 0 to 100; invalid
or denied proposals retain the engine value, constraints cannot rewrite it,
and denial is sticky. Callbacks must be pure and produce equivalent results
for equivalent AI and real contexts. The host restores gameplay RNG and
discards RNG-consuming general proposals without suppressing innate triggers.
Guaranteed hits, Struggle and one-hit-KO
rules bypass this hook; other checks such as semi-invulnerability still apply.
Link and recorded battles bypass the hook. Context is available only during
the callback; nested accuracy resolution retains the engine value.

An ABI `RHH_MOD_ABI_MOVES` module can declare up to 64 `RhhModMove` entries;
the host accepts up to 512 in total. Each uses a unique namespaced `move/`
ID, a stable type key, bounded ASCII name and description, power, accuracy,
PP, category, target and priority. Its built-in template provides the effect,
effect arguments, flags and animation. Use a compatible ordinary move as the
template: status and damaging categories must agree, and identity-specific
engine behavior may not be inherited just by copying an effect. Unknown
templates and types reject finalization; unusable templates cannot be taught.

An `RHH_MOD_ABI_LEARNSETS` module can declare `RhhModLearnset` additions for
enabled species, using canonical ordinary built-in or registered move keys. Level-up
entries use levels 0–100; level 0 denotes an evolution move. Teachable and egg
entries use level 0. The host accepts up to 256 additions per module, 2,048 in
total and 128 per species. References can point to another module regardless
of load order. Exact duplicate additions are applied once. Level-up additions
are merged by level, after existing entries at the same level; additions at the
same level use stable-key order. Teachable and egg additions follow existing
entries and omit duplicate moves. These are additive declarations: removing or
reordering built-in entries is not supported.

At new-game or load, the host reserves saved slots for available custom moves
referenced by these declarations. A special-only built-in move or a move that
cannot be resolved or cannot fit the remaining slot space is omitted. Pokémon
generation, level-up, evolution, the move relearner, egg inheritance,
teachability and the optional HGSS Pokédex consume the resolved lists. The
existing byte-sized level and egg lists are bounded at 255
entries, preserving built-in entries first; the relearner displays at most 60.
Saved Pokémon keep their learned move key when a module is removed, even
though that module's learnset additions disappear.

`party_moves` copies an owned party Pokémon's current move keys, PP, maximum
PP, PP Ups and available-definition mask, after validating the supplied
snapshot and identity. A retained key for an absent module is present but has
no availability bit. This supplies the current slot key for a teaching UI.
To teach a declared move, call `teach` with an up-to-date party snapshot,
the stable new move key, the selected move slot and the key currently in that
slot (zero if empty). It validates identity and freshness, rejects duplicate
moves, runs `RHH_CAN_TEACH_MOVE` constraints, then atomically replaces the
slot, resets its PP Ups and fills its declared PP. `RHH_MOVE_TAUGHT` observes
the committed result. For both events, `event_key` is the new move key,
`subject` is the Pokémon identity, and `args[0]` and `args[1]` are the party
and move slots. `teach_context` copies the proposal during either event.
Teaching is unavailable during battle or a save transaction; a module must
provide its own UI and can apply eligibility constraints. Atomic payment for
teaching is not yet supported.
The per-save mapping keeps each learned move key in its allocated slot across
reloads and mod order changes. If its module is absent the key is retained and
the move is an unavailable placeholder; restoring the module restores it.
Registered moves are queried through `rhh:service/moves`; the host's generic
`RHH_MOVE` content enumeration and resolver still cover built-in moves only.
Custom move registration, custom effects, arbitrary learnset edits and network or
recorded-battle compatibility remain experimental and incomplete.

The native battle Pokémon carries additional types without changing the
controller payload. Combat matchups, STAB membership, hazards, switch matchup
AI, type removal, Roost, Terastallization, Transform and Reflect Type use the
extended battler list. Species assignments are still derived from active mods
on load and on switching or form changes; link and recorded battles use the
unmodified species types. This is experimental: some type-dependent engine
paths still use three slots. General host-managed per-individual type
assignments and custom Tera form changes remain open; battle-scoped Tera rules
and mod-owned saved choices are described below.

Version 1.4 appends `compose`, preserving the 1.3 prefix
(`RHH_TYPE_SERVICE_V1_3_SIZE`). It evaluates one attack key against up to
`RHH_TYPE_COMPOSE_LIMIT` defense keys, including registered custom types.
Aliases are canonicalized and duplicates are applied once in first-occurrence
order. An empty defense list is neutral. Each distinct matchup runs the same
chart hooks and optional inverse rule as `chart`; immunity makes the result
zero. Nonzero factors are composed exactly as powers of two and clamped only
after all factors are combined to unsigned 4.12 values from 1 to `UINT32_MAX`.
This makes cancellation independent of intermediate overflow or underflow.
The result reports its distinct count, inverse and excluded flags, and
`RHH_TYPE_COMPOSITION_SATURATED` if its nonzero value was clamped. Invalid
keys, placeholders, flags, undersized output or too many inputs fail without
changing output; the entire list is validated before chart hooks run. A
recursive query from a chart hook fails. This is a keyed chart calculation,
not a complete damage calculation: move effects, abilities, items and
weather still belong to the battle engine, and the engine does not yet use
extended keyed lists for battlers.

Version 1.2 appends `species_context`, `species_proposal`, and
`offer_species_types`, preserving the 1.1 prefix
(`RHH_TYPE_SERVICE_V1_1_SIZE`). During `RHH_SPECIES_TYPES`, transforms and
finalizers can offer one to three distinct type keys. The original
species and its unmodified types are available through `species_context`;
`species_proposal` copies the current proposed list so later transforms can
compose with earlier ones. Offers are validated atomically, canonicalized,
and applied in the normal deterministic hook order; the last valid offer wins.
Constraints cannot offer types, and a sticky denial retains the original list.
Invalid offers leave the previous proposal untouched. Recursive species queries
fail, while nested engine lookups retain the original types. Hooks are pure and
must return the same list for equivalent run state. The host restores both
gameplay RNG streams after this event and discards the whole proposal if either
stream advanced, retaining the original species types. No hook runs without an
active run or in link and recorded battles.

The resolved list initializes the engine's legacy battle type slots at battle
entry, switching, and the existing form and Mimicry restoration paths. A
one-type list fills the first two slots with the same type; a two-type list
leaves the third slot empty. The species query, party matchup calculation,
overworld type matchup, dynamic move type lookup and Pokémon summary use the
same resolution; Pokédex type searches match the complete species list. The native
summary displays four species types at once, or three alongside a Tera icon,
and pages through the remainder every 90 frames. The optional HGSS-style
Pokédex and DexNav keep their first types visible while cycling the remainder;
DexNav falls back to two icons if no sprite transformation matrix is available.
When battle type indicators are enabled, they page through the public active
list, reflecting Roost, Terastallization and Illusion and refreshing after type
changes. Entry hazards use the full defensive list;
the switch AI uses all distinct active types and normalizes the offensive
average to its existing neutral threshold. Roost removes Flying from any slot.
Sky Battle eligibility considers the resolved species list. Illusion's AI
simulation and effectiveness comparison use the disguise's resolved types;
for extended lists, AI uses the disguise only while the real raw types still
match the species assignment, consistent with the public type indicator. AI
evaluation restores the original battler types afterward. Two-type moves combine
both attack types before their final effectiveness is clamped. The Z-Curse
preview recognizes Ghost in an extended raw type list; link and recorded
battles retain their controller snapshot behavior.
Type service v1.12 adds `ai_battler_list` during move scoring and effect
previews. It returns the scoring side's known active types using the same
Illusion projection as the engine's AI. In ordinary three-slot cases the AI
preserves a separately added type alongside the disguise; extended lists use
the disguise only while the whole original list matches. Allies,
AI-controlled battlers, a known Illusion and non-Stellar Terastallization use
the current types; Roost is applied to a disguised list when the battler is not
Terastallized.
The status-action estimate uses
the same view, including for an explicitly selected recipient. This query
does not change the battler or initialize its Illusion cache. The existing
`battler_list` remains an authoritative view and can expose hidden types;
other battle snapshots and score-context species are not a general hidden
information boundary. Queries fail outside the corresponding AI callbacks.
Type service v1.13 also adds `ai_battler_species`, returning the species key
the scoring side assumes for a present battler. An unrevealed Illusion uses
its disguise until the AI knows the ability or has seen a move the disguise
cannot learn. The query follows the same ally and AI-controlled-battler rules
as the known-type view and does not initialize the Illusion cache. Its result
can differ from the authoritative `candidate_species` in the existing score
context, which retains its earlier meaning. Both AI-known queries fail without
changing their outputs outside move-scoring and effect-preview callbacks.
Type service v1.14 adds `public_battler_types`, a copied player-facing list
for a present local battler. It uses the battle indicator's Illusion, Roost,
Tera and Pokédex visibility rules and supports the full registered type list.
When indicators are disabled or the indicator would use an unknown placeholder,
the result has the `RHH_PUBLIC_BATTLER_TYPES_UNKNOWN` flag and an empty list.
The query preserves gameplay RNG and does not initialize the engine's lazy
Illusion cache, and fails during AI simulations and link or recorded battles.
This is a presentation view, not an information-security
boundary: trusted modules can still query authoritative battle state.

Type service v1.15 adds `RHH_BATTLER_TYPES_OBSERVED` and
`battler_observation_context`. At stable native frames it compares each
present participant's ordered raw and effective lists with its previous
sample, reporting the changed views and copied before/after participants.
Raw types precede Roost and Tera; effective types match `battler_list`.
The event's key is the current species, subject is OT/personality identity,
ordinal is a battle-local sequence, and arguments are battler, owner, party
slot and change flags. Changes are dispatched in battler order after sampling
all participants; gameplay RNG is restored after observers. The initial
sample and each switch establish a baseline without notification. Link,
recorded and temporary AI simulations are excluded. The stream coalesces
changes between frames, cannot attribute causes or guarantee each mutation,
and is not a permanent identity or a player-facing visibility boundary.
Observers can also request the current authoritative ability and innate views,
subject to those queries' usual participant and eligibility checks.
The context query fails outside its observer or with a short buffer. Request
1.15 and check `RHH_TYPE_SERVICE_V1_15_SIZE`.

Type service v1.16 adds `RHH_PUBLIC_BATTLER_TYPES_OBSERVED` and
`public_battler_observation_context` for a separate coalesced player-facing
stream. Its before and after snapshots use `public_battler_types`, including
unknown indicators, Pokédex visibility and Illusion. Changes to visibility or
an Illusion can therefore notify even when authoritative types stay the same.
The event has zero key and subject; its arguments are battler, before flags,
after flags and zero. It does not include authoritative species or individual
identity. Initial samples and switches establish baselines; link, recorded
and AI-simulation states are excluded. Observers preserve gameplay RNG, and
changes between frames can coalesce. Internal baseline matching uses owner,
party slot and OT/personality, which can collide and are not permanent identity.
This is a presentation view for trusted
modules, not an information-security boundary. Request 1.16 and check
`RHH_TYPE_SERVICE_V1_16_SIZE`.

Encounter-attracting abilities check the resolved types of the selected wild
table, including after pool randomization. Type-based party evolution
conditions, Pike status immunity, randomized Pyramid type requirements, and
Factory and Dome type evaluations also use the resolved list. Dome scores
one-eighth and eightfold matchups in addition to its existing range; nonzero
matchups outside that range use the nearest supported scoring category.
Contest Curse animation selection uses the full resolved list as well.
Ordinary
in-battle type changes and Terastallization take precedence after initialization;
changing a mod setting does not rewrite a battler already in battle. Some
engine consumers may still read the original species data or only two slots.
The standard Pokédex has no type icons. Species assignments are
derived from mod state, not stored in the Pokémon save. Extended active types
remain experimental because some engine consumers still use fixed slots. `python3 native/tests/pokedex_types.py` tests the
optional HGSS Pokédex using an isolated configuration build. The disposable
`python3 native/tests/contest_type.py` checks the contest animation path with
an independently compiled type-assignment mod. All remove their test builds
afterward. `python3 native/tests/wild_dexnav.py` exercises the DexNav screen
and its three-type display in an isolated configuration build as well.

`rhh:service/world_clock` version 1.0 supplies a run-scoped 64-bit player-step
counter. It advances once when normal field input reports an accepted,
non-forced tile step, before trainer checks and step-triggered scripts. It
does not advance merely because a frame renders, or during battles, locked
controls, forced movement or elapsed time without a counted step. It counts
steps on every map; a mod decides whether steps away from a particular map
affect that map's state. Walking faster may advance it sooner in real time but
each counted tile advances it only once.
`current` returns the run identity and step count without changing state or
RNG. It fails outside an active run or for undersized output.

The committed `RHH_WORLD_STEP` observer event has the current map key in
`event_key` and the 64-bit step number in `ordinal`. During the callback,
`step_context` copies the map, local tile coordinates, elevation and current
engine metatile-behavior ordinal. The map key can be zero when no canonical
map is available; behavior and elevation are transient engine values, not
durable content keys. A callback can change its own module state but may not
issue engine mutation commands from this event; it can queue work for a safe
update. Recursive step dispatch is ignored. Both service operations leave
their output unchanged on failure.

The counter is part of the normal mod snapshot and rolls back with an older
save; it does not advance while the game is closed and is not a wall clock.
Old host-run schema 1 saves migrate with the new counter at zero while keeping
their run seed, identity and wild-creation ordinal. It saturates at
`UINT64_MAX`, after which further step notifications are suppressed rather
than reusing an ordinal. It is not an off-screen ecology simulation.

`rhh:service/world_time` version 1.0 samples the same real or fake RTC and
saved offset as the game. It returns the signed local day count, local hour,
minute and second, the effective time-of-day period and the current run/step.
The local day is the engine's offset day count, not a calendar date. Periods
match the overworld's configured morning/day/evening/night rules; a script's
hour override affects `effective_hour` and `period` without changing the local
clock fields. `override_hour` is zero when no override is active. The source
identifies a real or fake clock; `rtc_status` is the engine status and
`RHH_TIME_RTC_FALLBACK` indicates that the engine substituted its dummy RTC
value. Invalid clock data, missing game state, an inactive run and undersized
outputs fail without changing the result.

Time queries do not change game clock globals, advance simulation or consume
RNG. The real clock may nevertheless change between calls and while the game
is closed; the fake clock follows the engine's configured tick policy. Neither
is a monotonic simulation clock or guaranteed replay input. For deterministic
replays, fix the RTC source; for durable decisions, record the sampled facts.
All callbacks in one habitat query share one sample. Wild weight proposals
share one sample across their nested habitat query and all table slots, and a
wild candidate proposal shares one sample with its nested habitat query. An
unavailable reading remains unavailable throughout that scope. Separate
weight, candidate and committed events are separate samples and can straddle a
clock transition.

`rhh:service/world_environment` version 1.4 stores independent weather and
terrain effects for up to 128 maps in the normal run snapshot. `at` and
`current` return copies without changing state or RNG. A request identifies
the current map, a registered module's ID key as owner, a party slot and its
Pokémon identity, an eligible move, priority and duration. Rain Dance, Sunny
Day, Sandstorm, Hail, Snowscape and the four terrain moves are supported. The
Pokémon must be alive, not an egg, and know the move; using this API does not
consume PP. Duration is 1–1,000,000 counted world steps and cannot overflow the
clock. Steps on other maps count down the duration; time spent offline does
not. Restoring an older save restores the clock and effects together.

Surviving supported battle weather and terrain that differ from the local
conditions are queued for the battle's map after the battle ends. They use
the environment service's ordinary priority and constraint rules, so a
higher-priority source can prevent replacement. Their default duration is
1800 seconds on the PC system clock, including time outside the game. A map
condition `rhh:ecology/battle_environment_seconds` may set 1–1,000,000
seconds. Timing queries report `RHH_TIME_SYSTEM` and Unix seconds for these
records; invalid time pauses expiry and a clock rollback delays it. Explicit
scripted battle starting effects take precedence over local seeds, and saved
local effects take precedence over the map's natural weather. Strong winds,
harsh sun and heavy rain use their corresponding battle weather when seeded.

Version 1.4 adds `preview_timed` and `apply_timed` for durations of 1 to
1,000,000 seconds on the game's real or fake RTC. They use the same move,
ownership, priority and constraint rules; set `request.duration_steps` to
zero. `timing` reports an active effect's clock, source, start, deadline and
current value; `context_timing` reports the affected timer by kind inside a
constraint or change notification, including both timers in a combined expiry.
Existing snapshots retain their layout: timed effects show
`expires_step` as `UINT64_MAX`, not a step deadline. Do not subtract it to
estimate remaining time; use `timing`.

Version 1.5 adds `preview_checked` and `apply_checked`, plus timed variants.
The quote copies the request, run, both environment effects and their prior
timing, the current step and the proposed timing. A checked apply reruns normal
validation and constraints, and rejects a changed request, run, snapshot,
step, timer or clock source before committing. RTC progress is allowed, but a
rewind is stale; a timed duration starts at commit, with the same duration
shown in the quote. Quotes are not durable receipts or protection against a
malicious native module. Legacy preview and apply calls remain available.
Move-mapping proposals that consume either gameplay RNG stream are discarded
and both streams restored. Environment constraints that consume RNG are also
restored; an allowed impure proposal is rejected, while a denial remains denied.

Version 1.6 adds `inspect`, which copies both effects and their timings using
one step and time observation. `list` copies all maps with active saved effects
in canonical-key order, including unloaded maps, using one observation for
the entire result. Allocate `RHH_WORLD_ENVIRONMENT_MAP_CAPACITY` entries to
cover every possible map record, or use a null array with zero capacity to
query the current count. A too-small array fails without modifying the array
or count. These are point-in-time views, not reservations; the state can change
after a call. Expired effects are omitted without committing expiry or emitting
notifications, while timers whose saved clock source is unavailable remain
active with `time_available` clear. This enumeration does not simulate maps.

Version 1.7 adds `preview_source`, `apply_source` and their checked variants
for effects owned by a registered module without attributing them to a party
Pokémon or move. Supply a supported nonzero weather or terrain value, the
current map, owner, priority, a step or time clock and a duration from 1 to
1,000,000 in that clock's units. Set the value and duration to zero with the
step clock to clear an effect owned by the module. These commands use the same
priority ordering, constraints, expiry and notifications as move requests.
Snapshots identify a module-owned source by a zero `move` and
`actor_identity`; consumers should use its semantic value and owner. Checked
quotes also bind the source request itself, and are revalidated on apply.

Version 1.8 adds `preview_source_at`, `apply_source_at` and checked variants
that can target any known map, including an unloaded map. The v1.7 operations
remain restricted to the current map. Off-map operations use the same owner,
priority, constraint, duration, capacity and save rules, and still require a
safe overworld command context. Callbacks receive the target map in the event
key and copied environment snapshots; it may differ from the player's map.
Step durations count player steps on any map, while time durations use the
same game clock as local effects. These commands do not load a map, run its
scripts, or schedule off-screen simulation. A checked quote binds the target
map and saved environment state; apply reruns current constraints.

Version 1.9 accepts `RHH_WORLD_ENV_CLOCK_PERMANENT` for a nonzero
module-owned source with zero duration. It remains active until its owner
clears it or a permitted replacement takes precedence. `timing` and `inspect`
report the permanent clock, its start step, the current step and zero expiry;
the legacy effect snapshot uses `UINT64_MAX` for `expires_step`. Checked source
quotes report `transaction.timed` as 2 for this mode. Permanent effects use
the same current-map and source-at operations, constraints and notifications.

Version 1.10 rechecks the run, command context, target map, step, both saved
effects and timings, and any party move actor after constraints return. A
changed input makes the preview or commit stale. For timed proposals, a clock
tick during validation is also stale so the proposed start and expiry remain
the ones being committed. These checks do not undo changes made by an
independent callback. Environment-change observers now also restore both
gameplay RNG streams after notification.

RTC coordinates are seconds relative to local day `INT32_MIN`, including the
saved game-clock offset. Real time may advance while offline; fake time follows
the engine's fake clock. Clock rewinds or offset changes can extend a timer.
An unavailable, fallback or different RTC source leaves a timed effect active
and reports `time_available` as false until its original source is usable.
Expired effects disappear from queries immediately and produce their committed
expiry notification at the next safe overworld update or player step. An
allowed replacement delivers pending expiry first. Step-only saves are still
written in schema 1; saves containing timers use schema 2. Saves containing a
module-owned source use schema 3 and are rejected by hosts that predate source
support. Saves with a permanent source use schema 4. Once no permanent or
module-owned sources remain, saves can again use earlier schemas as appropriate.
Loading older schemas preserves effect deadlines.

`party_move` discovers supported moves in the four actual move slots of a
party Pokémon. It copies the move key, environment kind and value, slot,
identity and a `usable` flag for egg, fainted and challenge-death state.
Empty, unsupported and invalid slots fail without changing the output. It is
a read-only query and does not advance gameplay RNG. Usability does not promise
that a later command will succeed: the world, Pokémon, priority or another
module's policy may change. Pass the copied identity and move to `preview` or
`apply`, which revalidate them. The function is an append-only 1.2 addition;
older service prefixes remain available.

`RHH_WORLD_ENVIRONMENT_MOVE` lets a module map any available built-in or
registered move to an overworld weather or terrain value. The event key is the
canonical move key; `args[0]` and `args[1]` contain the original kind and
value. The initial decision is `RHH_WORLD_ENV_MOVE(kind, value)`, or zero if
the move is unsupported. A transform can propose zero to remove an existing
mapping, or a weather value from rain through heavy rain, or a terrain value from
grassy through psychic. Invalid or denied proposals retain the original;
constraints cannot rewrite a proposal. Queries and transactions use the same
mapping and callbacks must be pure and stable. A nested query uses the
original mapping. Effects store the resolved value and stable move key, so a
saved effect continues until expiry even if the mapping changes or the
defining module is absent; an unavailable move cannot be newly applied.

`rhh:service/ground_effect` version 1.0 exposes the transient visuals the
engine considers at the beginning or end of an object step. `evaluate` is a
copied preview for an active object slot; `context` supplies the current and
previous habitat samples during `RHH_WORLD_GROUND_EFFECT`. Transforms may
change only the phase's supported splash, ripple, footprint or dust bits;
constraints can deny but cannot rewrite the proposal. Denied, invalid or
RNG-consuming proposals preserve the original visuals, and both gameplay RNG
streams are restored. The engine applies the same query on real steps while
preserving unrelated ground effects and its jump, movement-direction,
visibility and object-link safety checks. It does not change movement,
collision, encounters or ground-cover state. Requested effects can still fail
to appear when the engine cannot allocate a sprite.

`rhh:service/berry_plot` version 1.1 enumerates visible, directly authored
berry-tree templates on the current map, including plots outside the sprite
viewport. Hidden templates and clone events are excluded; inconsistent IDs,
duplicate plot IDs or invalid hide flags make enumeration fail. `at` copies
the plot's map, coordinates, stable berry-item key, stage, watering state and
available habitat. Its opaque eight-byte state token is an exact, short-lived
engine snapshot, not a durable plot or item ID. Requery before acting.

Version 1.6 adds `authored_at(map, object_index, out)`: it pairs a directly
authored berry-tree object on any known map with a copy of its current run's
stored tree record. Obtain the object index from `rhh:service/authored_maps`;
it is not an index into current-map `count`/`at`. The result includes authored
placement, run ID, tree ID, growth and care fields, and stable berry and mulch
item keys when recognized. An unknown nonzero berry ordinal is flagged and has
no item key. Invalid maps, non-tree objects, invalid tree IDs, or an absent run
fail without changing the output. This reads stored values only: it does not
advance growth, evaluate visibility, run scripts, apply runtime object changes,
or determine whether an actor will spawn. Multiple authored objects may refer
to the same stored tree. The result cannot be used as a watering command token;
use `at` for checked current-map commands. Its berry key and stage can be
passed to `rhh:service/object_visuals` for source artwork when recognized.

Version 1.7 adds `authored_snapshot`, `preview_authored_water`,
`water_authored` and `authored_water_context` for explicit checked watering of
stored tree records on unloaded maps. A snapshot includes an opaque exact
record token and run identity. The registered owner must supply that fresh
snapshot; the host stages the engine calculation, invokes
`RHH_CAN_WATER_AUTHORED_BERRY` constraints, and rechecks the record and any
requested saved weather or terrain before committing. A successful commit
emits `RHH_AUTHORED_BERRY_WATERED`; context is available only during those
callbacks. Existing current-map hooks are separate, so a mod that constrains
both paths must subscribe to both. Constraints consuming gameplay RNG are
rejected. A loaded target must use the current-map API instead.

This operation addresses a global tree slot: any other authored object using
that slot sees the change too. The snapshot does not establish live visibility,
execute scripts, apply runtime object changes or forecast natural weather. It
does not automatically water trees or simulate unloaded maps; a caller decides
when it is appropriate to request watering. Optional requirements check only
the saved map weather and terrain, not natural weather or outdoor exposure.

Version 1.8 adds `RHH_BERRY_CLOCK_UPDATED`, with `clock_context` and
`clock_change` available during its observer callback. After the game clock
advances the saved tree records and its timestamp, one notification reports
the elapsed minutes and changed slots in ascending tree-ID order. Each copied
before/after state includes timing, care, mutation fields and an opaque record
token. Intermediate stages within the elapsed interval are not reported.
Slots are global and can belong to multiple authored objects; the event does
not infer map, weather, visibility or cause. A supported update with no changed
slots produces no event. Updates during an unsafe context or before an active
run are not reported. Observer RNG changes are restored; the event is not a
durable receipt and follows ordinary save rollback.

Version 1.9 adds `references`, a paged reverse lookup from a global tree ID
to recognized authored berry objects. Results use map-group, map-number and
object order and can be passed to `authored_at` or `authored_snapshot`. A
slot can have multiple references, and a reference does not establish that
an object is currently visible or that the map's weather affected the slot.
`unavailable_maps` reports source maps whose object data could not be checked;
an empty result is complete only when this count is zero. The scan reads every
authored map, so avoid calling it repeatedly from frame or render callbacks.
Invalid requests leave the output unchanged, and the lookup does not advance
gameplay RNG or require an active save.

`preview` and `water` use that snapshot and a registered cooperative owner
key. They reject changed plots, unavailable command contexts and stages that
do not need watering. The proposal uses the engine's watering calculation on a
copy, passes `RHH_CAN_WATER_BERRY_PLOT` constraints, rechecks the plot and then
commits once. Requests can also require specific saved weather and terrain;
these are rechecked with a permitted habitat immediately before commit, so an
expired or replaced effect rejects the command as stale. Version 1.1 also
allows requiring the natural weather captured in `expected.habitat` and an
outdoor habitat; both are checked again before commit. These flags do not
change the request layout, and version 1.0 callers remain supported.
`RHH_BERRY_PLOT_WATERED` observes a committed service command;
`context` copies before and after during either callback. A denied or
RNG-consuming constraint leaves the plot unchanged and restores both gameplay
RNG streams. This API does not consume a watering item or water unloaded maps.
Version 1.2 adds the separate observer-only `RHH_BERRY_PLOT_MANUALLY_WATERED`
event and `manual_water_context`. They report a successful ordinary watering
interaction on a verified, visible current-map plot, with copied before and
after states, the object-event slot and a changed flag. It covers use from the
field or the field Bag; ordinary plot queries and commands remain unavailable
while the Bag is open. A repeat action can
succeed without changing the plot and is still reported. Unsuccessful actions,
unmatched or hidden templates, unsafe contexts and service watering commands do
not produce this event. Observers cannot issue recursive watering commands;
their gameplay RNG use is restored. This process-local observation is not a
durable receipt.

Version 1.3 adds `care_at`, which copies a current-map plot together with its
weed and pest flags and mulch. The mulch has both a transient engine ordinal
and a stable item key when recognized; an unknown ordinal has key zero. The
observer-only `RHH_BERRY_PLOT_INTERACTED` event reports supported object
interactions for planting, removal, applying mulch, pulling weeds and clearing
pests. `interaction_context` copies the before and after care views, cause,
object slot, run ID and a flag indicating whether the plot record changed.
The event key is the map, its subject is the available before-or-after berry
item key, and its arguments are cause, tree ID, changed flag and object slot.
The host checks the authored plot and object before and after the interaction;
hidden, mismatched or unavailable plots do not emit an event. Observer RNG is
restored. These are process-local plot observations: removal is not proof of a
successful harvest or bag transfer, and direct engine initialization, timed
growth and service watering use separate paths.

Version 1.4 adds the observer-only `RHH_BERRY_PLOT_HARVESTED` event and
`harvest_context` for successful item additions from the ordinary berry-tree
interaction. The copied context identifies the verified plot and run, the
object slot, and the accepted normal and mutation berry item keys and counts.
The event key is the map; its subject is the normal berry item key, or zero
if only a mutation item was accepted. Its arguments are tree ID, normal count,
mutation count and object slot. A full Bag or other failed addition does not
emit a transfer for that item. The engine's mutation path performs two
additions; if only one succeeds, the event reports only that addition. This
observation runs after the additions and before the script removes the tree;
the separate removal event is not proof that a harvest took place. It follows
the engine's Bag routing and does not reserve items or prevent later changes.
It is process-local, not a durable or exactly-once receipt. Hidden or
unverified plots and unsafe contexts do not emit it, and observer gameplay
RNG is restored. Request version 1.4 and check
`RHH_BERRY_PLOT_SERVICE_V1_4_SIZE` before using the context query.

Version 1.5 adds `yield_preview` and the pure `RHH_BERRY_HARVEST_YIELD`
proposal. The preview takes a current-map plot index and copies the fruit-stage
plot, its engine yield and the resolved quantity; `yield_context` provides the
same inputs during the proposal. The event key is the map, subject is the
berry item, and arguments are tree ID, original yield, stage and zero.
Transforms may propose 1–255 ordinary berries; constraints can deny but cannot
rewrite the value. A denial, invalid value or use of either gameplay RNG
stream retains the engine yield and clears `accepted`; the streams are
restored. The engine uses this same rule for the tree's displayed count and
the actual harvest, with the latter calculated once before Bag checks. The
stored tree yield and mutation-berry count are unchanged. Requery before use:
state, weather or policy may change between the prompt and picking. Queries
fail on unavailable, non-fruiting, empty or stale plots, recursive yield
evaluation and unsafe contexts, leaving the output untouched. Request version
1.5 and check `RHH_BERRY_PLOT_SERVICE_V1_5_SIZE` before using these calls.

`rhh:service/field_healing` version 1.0 copies a party member's current HP
and owned-mon identity with `party_at`. `preview` and `heal` take that snapshot,
a registered owner and a positive amount; the amount is capped at missing HP.
They reject stale or ambiguous identities, eggs, fainted Pokémon, challenge-dead
Pokémon and unsafe overworld contexts. A staged engine HP update respects the
existing revival constraints without risking a partial party change.
`RHH_CAN_FIELD_HEAL` constraints can deny the proposal; `RHH_FIELD_HEALED`
observers receive one notification after a successful commit. `context`
copies the proposal during either event. Constraints must be pure; if they
consume gameplay RNG, the transaction is denied and both streams restored.
Version 1.1 adds `preview_environment` and `heal_environment`. They require
the current run and map and can additionally require saved weather, saved terrain,
and the world step. The host checks these conditions before and after the
constraints; a changed or expired effect rejects the transaction as stale.
`environment_context` copies the environmental request during its proposal
and committed notification. Version 1.0 callers retain their original API.

Version 1.2 adds `condition_at`, `preview_cure`, `cure` and `cure_context`.
The copied condition reports stable sleep, poison, toxic, burn, freeze,
paralysis and frostbite flags, plus an engine-format status token for stale
checks. A cure clears only requested conditions; curing toxic poison also
clears its counter. It rejects eggs, fainted or challenge-dead Pokémon and
ambiguous or changed identities, and can require the same map, run, weather,
terrain and step before and after constraints. `RHH_CAN_CURE_FIELD_STATUS`
is a pure constraint and `RHH_FIELD_STATUS_CURED` reports a committed cure.
Preview and denied attempts leave the Pokémon unchanged.

Version 1.3 adds `RHH_FIELD_POISON_DAMAGE`, `RHH_FIELD_POISON_DAMAGED` and
`poison_context`. During an overworld poison tick, pure transforms can select
damage from zero through the engine's HP floor; constraints can deny the tick.
The context copies the affected party member, status and, when available,
the current saved environment. Unchanged, invalid or RNG-consuming proposals
retain the engine's original behavior; denial suppresses the tick, including
the one-HP recovery case. If a callback changes the sampled Pokémon, run or
available environment, that Pokémon's tick is abandoned instead of applying
damage from the stale sample. The observer receives a committed notification
only when HP actually falls.
The default `OW_POISON_DAMAGE` configuration follows Gen V and does not schedule
overworld poison damage, so this hook runs naturally only when that engine
setting enables it. It does not alter battle poison.

Version 1.4 adds habitat-guarded heal and cure previews and commits. Capture a
permitted `habitat.player()` sample and the current run ID in the request; the
host samples again before the proposal and after constraints. Movement, tile,
natural weather, resolved habitat flags or saved environment changes make the
request stale. Existing HP, identity, status and challenge checks still apply.
`habitat_context` copies the guard during the corresponding constraint and
committed notification. A guarded request does not reserve the tile between
separate calls, and unavailable or denied habitat samples fail the check.

Version 1.5 adds `poison_habitat_context` for poison proposals and committed
damage observers. Its first call during a tick copies the player's habitat,
including natural weather, tile and saved environment; `available` is zero
when it cannot be sampled. When a proposal requests the view, the host checks
it again after the proposal and abandons the tick if availability or sampled
fields changed. The observer sees the same copy, or samples at notification time
if no proposal requested it. Hooks that do not request this view retain the
earlier poison behavior. Field heal, cure and poison damage notifications
restore both gameplay RNG streams after observers run.

`RHH_WORLD_ENVIRONMENT_PRESENTATION` selects a declared shader when the current
map has saved weather or terrain. The event carries their semantic values,
map key and step. It runs as a presentation query in the OpenGL overworld;
the selected shader receives the battle shader uniforms and top-left UVs,
with `uScene` containing the composited world presenter or original scene.
Its mask identifies protected interface pixels, sprites and backgrounds;
the host restores interface pixels and shaders should restrict terrain to background.
The pass precedes the live viewfinder overlay and is included in photographs.
A missing or invalid shader and software rendering use the original scene.
This pass uses the saved map effects, not natural or scripted field weather;
it does not alter weather mechanics or the underlying game frame. One legacy
environment shader is selected through normal reducer ordering.

`rhh:service/world_render` version 1.0 lets independent modules offer a chain
of up to 16 shader passes during the `RHH_WORLD_RENDER_PASSES` constraint
callback. Each offer specifies a unique pass key, declared shader key and
signed priority. The host copies offers, sorts by increasing priority and then
pass key, and executes the chain after the selected environment shader. It
discards the offered chain on duplicate pass IDs, a denied collection or
capacity overflow; an invalid offer fails without adding a pass. A shader that
fails to compile is skipped. Collection requires an active run but can occur
without active environment effects and must remain read-only. The event ordinal is the visual frame and
its first two arguments are the saved weather and terrain. Every pass samples
the previous composited image with the same uniforms and mask as the environment
shader. The host restores protected interface pixels after the chain; the
viewfinder is added afterward and photographs contain the composed scene.
Software rendering uses the original scene; an empty or failed chain leaves
the preceding presentation unchanged. A presenter using `RhhWorldFrame` v1.4
can publish a 240×160 depth texture. World environment and ordered pass
shaders receive `uDepth` and `uDepthAvailable`; sample the texture at
`vec2(vUv.x, 1.0-vUv.y)` because it uses framebuffer orientation. Depth is
normalized as supplied by the presenter and represents its scene before the
world-pass chain; it does not follow pixels displaced by earlier shaders.
The host rejects textures with incompatible dimensions, format, filtering or
depth-comparison mode, and clears availability when no valid depth is supplied.
Shaders should leave the scene unchanged or use a deliberate fallback when
depth is unavailable.
Version 1.2 adds `shader_photo_flags` for inspecting a registered shader's
declared photo policy by key. It returns zero flags when no policy was declared
and leaves the output unchanged for an unknown shader. A declaration is the
shader author's promise, not analysis of the shader or a guarantee that it
preserves color, contrast, visibility or focus.

`preview` runs the same validations and constraints without committing.
`apply` is available in a safe overworld command context, including
`RHH_MOD_UPDATE`, and rejects recursive or battle-time calls. Higher priority
wins; equal priority goes to the lower numeric owner key. The current owner
can replace its effect or clear it with a zero move, zero actor, slot and
duration. Modules are trusted: the owner key is checked for a unique
registered ID but is not authenticated as the calling module. A full map
table rejects new maps rather than silently evicting effects.

`RHH_CAN_WORLD_ENVIRONMENT` is a constraint callback and
`RHH_WORLD_ENVIRONMENT_CHANGED` is a committed observer. During either,
`context` returns the proposal or notification, including before/after map
snapshots, kind, cause and preview flag. Expiry is delivered before that
step's `RHH_WORLD_STEP`; simultaneous weather and terrain expiry on one map
uses kind zero in one notification. Commands cannot be issued recursively
from those callbacks; queue them for a safe update. Battle initialization
uses the current map's effects when ordinary overworld weather and explicit
trainer starting conditions do not take precedence. They persist for the
battle until battle rules replace them; battle changes do not write back.
Link, recorded and Safari battles do not receive these seeds.

Before an eligible battle adopts each map weather or terrain seed,
`RHH_BATTLE_ENVIRONMENT_SEED` reducers can transform `decision.value` or deny
it. During dispatch, the version 1.1 `battle_seed_context` copies the map
snapshot, kind and original value, semantic trainer/double/first-battle/roamer
flags, and registered opponent trainer keys when available. `event_key` is
the map, `subject` is the original effect's owner (zero when absent), and
`ordinal` is the saved step. Special or unknown trainers have zero keys.
The hook also runs when no map effect exists, allowing a mod to propose one.
Normal transform and finalizer ordering applies; constraints can deny but
cannot rewrite the candidate, and denial remains sticky. Zero means no seed.
An RNG-consuming proposal is discarded and both gameplay streams are restored;
a denial remains effective. Weather candidates include none, rain, sun, sand,
hail, snow, fog, strong winds, harsh sun and heavy rain; terrain candidates are
none, grassy, misty, electric or psychic. An invalid final value falls back to
the original map effect. Explicit trainer statuses and existing battle
weather/terrain retain precedence and skip the corresponding proposal. A
saved local effect takes precedence over the map's natural weather. The proposal changes only this
battle's initial state, never the stored map effect. It runs once per eligible
kind in the battle-start sequence, and does not run for link, recorded or
Safari battles. The context function leaves output unchanged outside dispatch
or when undersized. Version 1.0 callers retain their original service prefix;
check the service version and size before using the appended function.

The service is a foundation for mods, not a complete player-facing field
move feature: no built-in move menu, overworld effects renderer or terrain
interaction rules are supplied. The battle shader reads the resulting battle
state as usual.

`rhh:service/authored_maps` version 1.0 exposes copied static map headers and
tile data by stable map key, including for maps that are not loaded. Enumerate
candidate keys with the host's `RHH_MAP` content catalog; aliases resolve to
the canonical map key. `map` reports dimensions, map type, area key, authored
weather and movement flags. Authored weather can be dynamic and is not a
prediction of current weather. `tile` reports the original metatile entries,
collision, elevation, behavior and layer, plus baseline `RHH_HABITAT_*` flags
when `habitat_available` indicates that behavior can be classified. These are source-layout values: saved
map edits, scripts, object collisions, habitat proposals and the loaded map's
live weather are not applied. Undefined cells and invalid or unavailable maps
fail without changing the output. These queries do not load or simulate maps,
run callbacks or consume gameplay RNG; they can also run during battle after
module finalization. A catalog key does not guarantee that a map group is
present in this build.
Request version 1.0 and check `RHH_AUTHORED_MAP_SERVICE_V1_0_SIZE` before use.
Version 1.1 adds `tile_rgba`, which decodes one authored 8×8 tile entry,
including its horizontal and vertical flips, to a caller-owned RGBA buffer.
It uses the keyed map's source graphics and source palettes without loading
the map or changing live VRAM or palettes. Index zero is transparent. Runtime
tile animation, palette animation, lighting, weather and fades are not
included. Palette banks reserved for runtime use, missing or out-of-range
graphics, unknown raw asset sizes, invalid maps and undersized buffers fail
without changing the output. The query may decompress a source tileset, so
callers should cache results when drawing many tiles. Request version 1.1 and
check `RHH_AUTHORED_MAP_SERVICE_V1_1_SIZE` before using it.
Version 1.2 adds `tiles_rgba` for batches of up to
`RHH_AUTHORED_TILE_BATCH_MAX` entries from the same map. Results preserve
input order and include each original tile entry, a status and RGBA pixels.
An unavailable tile has `RHH_AUTHORED_TILE_UNAVAILABLE` and zeroed pixels;
other tiles in the batch can still succeed. The host decompresses each needed
primary or secondary tileset slot at most once per call. Invalid map, pointers, count or output
capacity, and allocation failure reject the entire request without changing
the results. A zero-count request accepts null arrays for a valid map. Output
capacity is measured in result records. The batch uses the same authored
palette and graphics rules as `tile_rgba`; it does not include runtime effects.
Request version 1.2 and check `RHH_AUTHORED_MAP_SERVICE_V1_2_SIZE` before use.
Version 1.3 adds `connection_count` and `connection_at` for authored map
connections in engine order. Each copied record names the source and, when
known, the destination map, direction and offset. If the destination layout
is available, cardinal connections also report its origin in source-local
metatile coordinates and the half-open overlap interval along the source edge.
An empty interval means the authored layouts do not overlap on that edge.
Dive and emerge connections have no planar geometry. A destination key can be
known even when its layout is unavailable; check the flags before using its
dimensions or geometry. Unknown directions remain observable with their raw
engine ordinal. These are static declarations, not proof that travel is
possible: they do not include warps, collision, scripts, saved changes,
reciprocal connections or runtime map generation. Invalid requests leave
outputs unchanged. Request version 1.3 and check
`RHH_AUTHORED_MAP_SERVICE_V1_3_SIZE` before using these calls.
Version 1.4 adds `warp_count` and `warp_at` for a map's authored warp events.
Each copied record gives the source tile, elevation, raw destination-warp
byte and destination key when known. Flags separately identify dynamic warps,
available destination layouts, available destination warp records and
whether the latter's coordinates lie within that layout. Dynamic warps do not
receive a guessed destination. Destination indices are only resolved when the
engine's signed warp ID can represent them and the target event exists; this
query does not invent the engine's fallback position. The destination elevation
is the authored warp event's elevation, not a prediction of player elevation.
Scripts, special facilities, saved dynamic warps, tile behavior and collision
can change or prevent the actual transition. Invalid requests leave outputs
unchanged. Request version 1.4 and check `RHH_AUTHORED_MAP_SERVICE_V1_4_SIZE`.
Version 1.5 adds `object_count` and `object_at` for source object-event
templates in authored order. Each record copies its map key, index, local ID,
coordinates and kind. Normal objects additionally report raw engine graphics,
elevation, movement, trainer/berry and flag values. These ordinals are not
stable content identities; a graphics-variable flag identifies graphics that
depend on runtime variables. Clones instead report the target map key and
one-based template index, with separate flags for an available target map and
an existing target template. A target key can exist when its map is unavailable.
Unknown kinds retain their raw ordinal but have no decoded kind-specific fields.
The records do not expose scripts, resolved sprite images, current positions,
visibility, saved changes or spawned actors. Invalid requests leave output
unchanged. Request version 1.5 and check `RHH_AUTHORED_MAP_SERVICE_V1_5_SIZE`.
Version 1.6 adds `coord_count` and `coord_at` to inspect authored coordinate
events in source order. Records distinguish weather events from immediate and
conditional scripts, and copy coordinates, elevation and the raw trigger and
condition values. A recognized weather event supplies the effective byte-sized
weather code, its engine weather ordinal and semantic natural weather; weather
cycles are marked dynamic. Unknown weather codes are retained without a guessed
result. The query does not run scripts, evaluate conditions or predict live
weather: crossing the tile, scripts and other game state determine what occurs.
Script pointers are not exposed. Invalid requests leave output unchanged.
Request version 1.6 and check `RHH_AUTHORED_MAP_SERVICE_V1_6_SIZE`.
Version 1.7 adds `object_image` and `object_image_rgba` for the first source
frame of supported fixed object graphics. They report dimensions and copy
tightly packed RGBA8888 pixels with transparent palette index zero. The host
uses source graphics and palettes, not live VRAM, weather, lighting, movement,
visibility or animation. Variable graphics, clones, the dynamic Mauville old
man and berry-tree graphics, Pokémon graphics, compressed or sheet-based
graphics, missing palettes and unsupported layouts fail without changing the
output; the API does not guess their current appearance. A clone's target can
be queried separately when its authored target exists. The first source frame
is not necessarily the object's initial facing or eventual rendered appearance.
Request version 1.7 and check `RHH_AUTHORED_MAP_SERVICE_V1_7_SIZE`.
Version 1.8 adds `object_frame_count` and `object_frame_rgba` for supported
fixed object graphics. Frame indices follow the engine's source image table;
the host bounds contiguous source sheets by their compiled asset size and
explicit tables by their number of entries. An unrecognized table fails the
count query rather than inventing a bound. Individual frames can still be
unavailable if their layout or data is unsupported. These indices describe
source images only: animation timing, facing, runtime changes and visibility
are not inferred. Existing v1.7 image calls continue to select frame zero.
Invalid requests leave outputs unchanged. Request version 1.8 and check
`RHH_AUTHORED_MAP_SERVICE_V1_8_SIZE`.
Version 1.9 exposes source animation tables and their bounded commands for the
same supported fixed graphics. `object_animation_count` and
`object_animation_command_count` report known bounds;
`object_animation_command` copies frame, jump, loop and end commands, including
duration, flip flags, jump target or loop count. Flags identify source frame
indices that have a known image and jump targets inside the script. These flags
do not promise that an individual frame can be decoded or that a script is a
safe, terminating program. Unknown commands retain an unknown kind. The
`object_face_animation` query uses `RhhWildActorDirection` and the engine's
facing mapping for animate objects; diagonal directions use the engine mapping.
It does not infer an object's current or initial facing, execute scripts or
advance animation. Unknown tables or scripts and invalid requests fail without
changing output. Request version 1.9 and check
`RHH_AUTHORED_MAP_SERVICE_V1_9_SIZE`.
`rhh:service/object_visuals` version 1.0 provides source object graphics by
transient engine graphics ID, independently of map events and live VRAM.
`resolve` samples one saved graphics variable when the input is variable,
reports the resolved ID and whether the engine would use its invalid-ID
fallback, and distinguishes Pokémon, shiny/female and other dynamic graphics.
For a recognized Pokémon it also supplies a species key for the separate
species-visual service. It does not run map scripts or predict a future or
already-spawned actor's appearance; a resolved ID is a point-in-time value.
Variable resolution requires an available save, while fixed ID resolution does
not. All queries require finalized modules.

`info`, `frame_rgba`, `animation_command_count`, `animation_command` and
`face_animation` accept a resolved fixed ID and use the same source palette,
frame bounds, animation tables and facing mapping as authored object graphics.
Pixels are tightly packed RGBA8888 with transparent palette index zero; zero
frame or animation counts mean the source table's bounds are unavailable.
Variable IDs, Pokémon, the dynamic Mauville old man and berry trees,
compressed and sheet-based graphics or missing source data are not decoded by
these calls. Invalid requests leave outputs unchanged. Request version 1.0 and
check `RHH_OBJECT_VISUAL_SERVICE_V1_0_SIZE`.
Version 1.1 adds `berry_info`, `berry_frame_rgba` and `berry_command` for an
explicit berry item key and growth stage. They use the engine's berry-specific
source image table, stage palette, dimensions and animation commands without
loading or changing a plot. `frame_count` bounds source image indices; frames
whose dimensions do not match the requested stage fail without changing the
output. Commands use the same frame, flip, duration and control-flow semantics
as authored object animations. An empty plot has no visible tree and is not a
valid request. The caller can obtain the current berry and stage from the
berry-plot service, but source graphics do not include live sprite animation,
lighting, sparkle effects or visibility. Request version 1.1 and check
`RHH_OBJECT_VISUAL_SERVICE_V1_1_SIZE`.
Version 1.2 adds `variant_current` for the resolved Mauville old-man graphics
ID. It samples the saved selection into a copied variant; invalid saved values
fail safely. `variant_info`, `variant_frame_rgba`,
`variant_animation_command_count`, `variant_animation_command`, and
`variant_face_animation` use that explicit variant, including its matching
source palette. They remain consistent if the saved selection changes between
calls and can also inspect a valid explicit variant without a save. Variant
ordinals are engine values, not persistent identities. The current selection
requires an available save; it does not predict live object visibility or
changes made after sampling. Invalid requests leave outputs unchanged. Request
version 1.2 and check `RHH_OBJECT_VISUAL_SERVICE_V1_2_SIZE`.
Authored-map service version 1.10 adds `background_count` and `background_at` for authored
background interactions in source order. Records copy tile coordinates,
elevation and the raw engine kind. Recognized script interactions indicate
whether a script exists and give the authored player-facing setting as a public
enum. A null script uses an engine fallback, which can behave differently.
Hidden-item records expose their raw item and hidden-item index, quantity,
underfoot setting and item key when available. Secret-base records copy their
raw base ID. Unknown kinds retain their raw kind without interpreting the
union. No script pointers, saved flags, secret-base occupancy or current
interaction availability are exposed, and no script is run. Invalid requests
leave output unchanged. Request version 1.10 and check
`RHH_AUTHORED_MAP_SERVICE_V1_10_SIZE`.

Version 1.11 adds `scan`, a bounded row-major page of an in-bounds source
rectangle. Give its origin, nonzero dimensions, first cell offset and a limit
from 1 through `RHH_AUTHORED_MAP_SCAN_MAX`; use `next` as the next offset until
it equals `total`. An offset equal to `total` returns an empty page. Each
cell carries its coordinates and either the same data as `tile` or an
unavailable status and zero tile. Invalid requests and undersized outputs
leave the output untouched. This makes full-map enumeration possible without
loading a map; like `tile`, it reports authored data only and does not run
scripts, apply saved edits, execute habitat hooks, or simulate the map.
Request version 1.11 and check `RHH_AUTHORED_MAP_SERVICE_V1_11_SIZE`.

`rhh:service/authored_event_state` version 1.0 pairs authored object and
background-event indices with current run flag values. `object_hide_flag`
samples a normal or clone template's hide flag; a zero flag has scope NONE.
`hidden_item_flag` samples the engine's collection flag for a hidden-item
background event. Results include the canonical map, event index, run ID,
transient flag ordinal, flag value and scope. SAVED flags are stored in the
save block; TEMPORARY flags belong to the current map context and are cleared
when maps load, while SPECIAL flags are process-local. In particular, a
temporary flag sampled for an unloaded map is the current context's flag,
not that map's future value. Invalid indices, event types, flags, missing runs
and undersized outputs fail without changing the output. These are sampled
authored flag values, not live visibility, spawn guarantees, item access or
the effects of scripts and runtime edits. Request version 1.0 and check
`RHH_AUTHORED_EVENT_STATE_SERVICE_V1_0_SIZE`.

Version 1.1 adds `dynamic_warp`, a copied view of the active run's saved
dynamic warp. It reports the raw signed engine map, warp and coordinates,
the stable map key when the destination's authored data is available, and the
ordinary engine arrival choice: destination warp event, saved coordinates or
map center. The in-bounds flag describes that choice; it does not validate
collision. An unknown or unavailable destination retains the raw values but
has no resolved arrival. Scripts, special warp handling or later changes may
change the destination before use; this query does not execute or reserve a
warp. A missing run or an undersized output fails without modifying it.
Request version 1.1 and check `RHH_AUTHORED_EVENT_STATE_SERVICE_V1_1_SIZE`.

`rhh:service/species_visuals` version 1.0 exposes source overworld Pokémon
sprite frames by stable species/form key, shiny flag and female flag. `info`
copies the chosen graphic's dimensions and source-frame count; `frame_rgba`
decodes a selected compressed frame with the same source palette selection as
the follower engine. The result flags identify when the engine's substitute
placeholder graphic is used. The request does not infer a Pokémon's gender,
form, shininess, animation, or live actor state; callers provide the actual
species/form and appearance. Frames use tightly packed top-left RGBA8888 with
palette index zero transparent. The query does not modify VRAM, palettes,
gameplay RNG or the world. Unsupported or missing graphics, invalid requests,
small buffers and allocation or decompression failures leave outputs unchanged.
It currently supports compressed contiguous overworld sheets; request version
1.0 and check `RHH_SPECIES_VISUAL_SERVICE_V1_0_SIZE`. Each frame query
decompresses its source sheet, so callers drawing repeatedly should cache it.
Version 1.1 adds source animation counts, bounded command records and a
facing-to-animation query for the selected species graphics. Commands use the
same frame, jump, loop, end and flag meanings as authored object animations;
frame availability is checked against the decoded sheet's frame count.
Direction uses `RhhWildActorDirection`, including the engine's diagonal
mapping. This reports source animation data without determining or advancing a
live actor's animation. Unknown tables and scripts fail without changing output.
Request version 1.1 and check `RHH_SPECIES_VISUAL_SERVICE_V1_1_SIZE`.
Version 1.2 adds `frames_rgba` to decode up to `RHH_SPECIES_VISUAL_BATCH_MAX`
selected frames from one sheet decompression. Each image is tightly packed;
the caller supplies byte stride and total output capacity. Frame indices can
repeat, padding between images is left unchanged, and invalid requests or any
failure leave the output unchanged. A zero-count query accepts null frame and
output pointers for a valid species request. Request version 1.2 and check
`RHH_SPECIES_VISUAL_SERVICE_V1_2_SIZE`.
For a visible wild actor, use the snapshot's species and its SHINY and FEMALE
flags to construct the request, and respect its visibility and concealment
state. A source image alone does not establish that an actor should be shown.

`rhh:service/habitat` version 1.0 samples a local metatile on the loaded
overworld map. It combines the copied tile and collision data with semantic
grass, tall/long grass, sand, water, shallow water, ice, encounter-tile,
underground, underwater and indoor flags. Flags can overlap and are not
permission to spawn or move: engine collision, encounter and actor rules must
still be checked. Unknown maps or metatile behaviors, out-of-bounds coordinates,
undersized outputs and unavailable maps fail without modifying the output.

The sample includes the current natural overworld weather as a stable semantic
value plus its transient engine ordinal and, independently, the saved map
environment when an active run is available. The latter contains its owner,
move and expiry and can be
absent; it does not claim that mod weather has changed the engine's overworld
renderer. `RHH_HABITAT_QUERY` reducers may transform the semantic flags or
deny the query's use by setting `decision.allowed` to zero. During dispatch,
`context` copies the original sample, `event_key` is the map, `args[0..1]` are
local x/y, and `ordinal` is the saved player step. Constraints may deny but
cannot rewrite the flags; denial remains sticky. Unsupported final bits fall
back to the base flags, while a denial returns `permitted = 0` and zero flags.
No hooks run without an active run. Recursive habitat queries fail, and
reducers cannot issue host mutation commands. Queries do not advance game RNG
or commit world state. If a reducer advances either gameplay RNG stream, the
host restores both streams and returns that sample with `permitted = 0` and
zero flags. A search skips and counts such a sample as denied. This is a shared input for ecology and encounter mods;
it does not itself spawn actors or change movement or encounter rules. Time is
available separately through `world_time`.

Version 1.2 adds `weather`, a pure resolver for any copied habitat sample,
including one from an encounter or ground-effect callback. A non-NONE saved
weather overrides the natural weather; otherwise the natural condition is
used. The result identifies its source and whether saved weather was available,
so callers can decide whether a natural result is sufficiently certain. The
categories retain natural-only cases such as ash, shade and underwater, and
distinguish thunderstorms and heavy rain. Invalid samples fail without changing
the output. This resolves a snapshot; it neither samples the live world nor
implies that the renderer has applied saved weather.

Version 1.3 adds `player`, which samples the current tile of the active player
object on the loaded map through the same habitat transforms and constraints.
It fails without changing the output if the player, map or tile is unavailable
or when called recursively from a habitat callback. The result is a momentary
view; field commands still need their own stale-state checks. Pass the copied
sample to `weather` to resolve its saved and natural weather.

Version 1.4 adds the observer-only `RHH_NATURAL_WEATHER_CHANGED` event and
`natural_weather_change` context. At safe overworld updates, the host samples
the engine's current natural weather on the loaded map. A change since the
previous safe sample reports the old and new semantic categories and engine
ordinals, map, run, a process-local sequence, and the player-step clock when
available. Different engine weather variants can produce an event even when
their semantic category is the same. The first sample after a map change,
new run or load establishes a baseline. The sequence resets on new runs and
loads; it is not a persistent event ID.

This is a coalesced observation, not a transaction log: intermediate changes
between safe samples can be missed, including changes during scripts, fades,
menus or battle. It does not report saved weather changes; use
`RHH_WORLD_ENVIRONMENT_CHANGED` for those and resolve their current precedence
with the habitat weather service. Observers run before the next overworld
`RHH_MOD_UPDATE`; both gameplay RNG streams are restored after notification.

Version 1.5 adds `neighborhood`, a copied 3×3 view around an in-map tile,
ordered by `RhhHabitatNeighborIndex`. Every in-map tile passes through the
normal habitat transforms and constraints with one shared world-time sample.
`present_mask` distinguishes map-edge omissions from samples whose rules deny
their use; `permitted_mask` identifies the tiles that rules permit.
Absent entries are zeroed. If the map, natural weather, saved environment or
sampled tiles change during collection, the query fails without changing the
output. It is a local snapshot, not a movement or spawn reservation; other
world conditions can still change after the query returns.

Version 1.6 adds `authored_sample` for a source tile on any known map. It
combines authored map and tile data with one sampled clock and the saved map
environment, without loading the map or running habitat hooks. Its nested
sample's `permitted` field means the source was decodable; it is not a live
permission check. The header's natural weather is an assumption: scripts,
coordinate weather events and saved tile changes can make the live map differ.
Dynamic or unknown header weather is not a forecast. Time and saved-environment
availability are reported separately. The period includes the current global
script override and is not a prediction of a target map's scripts. Failed
requests leave output intact.

Version 1.1 adds a bounded `search` for the first permitted tile matching all,
any and excluded habitat flags within a Manhattan radius of at most four. It
visits in increasing distance, then y and x order, clips to the current map,
and optionally includes the origin. Each visited tile goes through the usual
habitat transforms and constraints; denied tiles are counted and skipped.
The result reports the first match, its distance, and how many tiles were
sampled or denied. A not-found result is successful with `found = 0`. All
callbacks in one search share the same world-time sample. A failed tile query,
invalid request or search nested in a habitat callback leaves the output
unchanged. A match is not a path or movement permission. Request version 1.1
and check `RHH_HABITAT_SERVICE_V1_1_SIZE` before using `search`.

`rhh:service/habitat_registry` version 1.0 lets modules share named habitat
predicates. Use `RHH_MOD_ABI_HABITATS` and append `habitat_count` and
`habitats` to the module definition. Each `RhhModHabitat` has a unique stable
ID, a nonempty label, a description, an optional map key and optional masks.
`all_flags` must all be present, at least one `any_flags` must be present when
that mask is nonzero, and no `exclude_flags` may be present. Period bits are
`1u << (RHH_TIME_* - 1)`; natural weather, saved weather and saved terrain
bits are `1u << enum_value`. A zero mask is a wildcard; bit zero can explicitly
match no saved effect. Time zero means unavailable, and a required saved
environment or time never matches when unavailable. Natural weather and saved
effects are separate inputs. Unpermitted samples never match.

Registration rejects missing maps, invalid masks, contradictory requirements,
duplicates and more than 128 habitats per module or 512 overall. Map aliases
resolve to the same map. After finalization, `count`, `at` and `find` return
definitions sorted by ID, and `match` evaluates a supplied copied habitat
sample and time without changing game state. Failed queries leave their outputs
unchanged. Callers can use the habitat already provided in wild candidate and
weight contexts, and pass its corresponding effective world-time period. This
registry does not itself alter encounter weights or behavior.

Registry version 1.3 adds `evaluate_authored` for these copied baselines. It
evaluates the supplied source flags, clock and saved effects as assumptions,
not as a live spawn or movement decision. A required natural-weather predicate
is indeterminate when the header weather is dynamic or unknown. An effective
weather predicate is likewise indeterminate unless a nonzero saved effect
determines it. Known mismatches still take precedence over unavailable inputs.

Version 1.1 adds `effective_weather_mask` to habitat declarations using the
previously reserved word, preserving the declaration's size and array stride.
Its bits are `1u << RHH_HABITAT_WEATHER_*`; a zero mask remains a wildcard.
When nonzero, matching requires an available saved environment and tests the
same saved-over-natural resolution as `habitat.weather`. It composes with the
other masks, which retain their original separate meanings. Declare a required
dependency on habitat_registry 1.1 when using this field: earlier hosts reject
the nonzero reserved word. The old definition view continues to return zero in
its reserved field; the new `effective_weather_mask(key, out)` query retrieves
the registered mask.

Version 1.2 adds `evaluate`, which reports `MATCH`, `MISMATCH` or
`INDETERMINATE` with separate bitmasks for failed predicates and unavailable
inputs. A required period with period zero and a required saved weather,
terrain or effective weather with no saved-environment sample are unavailable.
A known mismatch takes precedence in the status, while both masks remain
available. Natural weather values such as UNKNOWN and DYNAMIC are explicit
enum values, so their masks are evaluated as supplied. A sample marked
unpermitted is a known mismatch; callers constructing hypothetical samples
must not mistake that value for a live permission check. Evaluation is pure,
uses the same predicates as `match`, and leaves the output untouched on error.
The older `match` continues to return false for indeterminate results. Request
version 1.2 and check `RHH_HABITAT_REGISTRY_SERVICE_V1_2_SIZE`.

`rhh:service/ecology_tags` version 1.0 supplies a shared vocabulary of
species traits. A module using `RHH_MOD_ABI_ECOLOGY` may declare
`RhhModEcologyTag` descriptors and `RhhModSpeciesTag` associations in its
definition. Create association keys with `host->content_key` in the module
entry point. A descriptor has a unique stable ID, a nonempty label of at most
63 bytes, and a description of at most 127 bytes. Registration allows up to
128 descriptors and 1024 associations per module, and 512 descriptors and 4096
associations overall. Missing species or tag references fail finalization;
references can point to another module regardless of registration order.
Duplicate tag IDs are rejected, while repeated species/tag associations are
collapsed. Species aliases resolve to the same set. `count`, `at`, `find`,
`species_count`, `species_at` and `species_has` provide copied results;
enumeration is sorted by tag ID, and failed queries leave output unchanged.
The registry is read-only once finalized. The `rhh:preference/` tags described
above affect encounter weights; other tags have no built-in weight or movement
effect. Mods can consult any tags from existing wild and habitat hooks.

`rhh:service/wild_traits` version 1.0 resolves a named trait for a live wild
actor. It starts with the species association and any saved individual
override, then dispatches
`RHH_WILD_TRAIT_QUERY` transforms and constraints. Transforms may propose zero
or one; an invalid final value falls back to the pre-transform value. Constraints
cannot rewrite the value and denial is sticky; a denied result has
`permitted = 0` and `value = 0`. `context` copies the original actor, trait,
species default, initial value and available individual ID during the hook.
`event_key` is the trait key, `subject` is the species, `args[0]` is the species
default, and `args[2]` is the saved override mode, if any.
When `individual_available` and `args[1]` are nonzero, `ordinal` is the
run-scoped individual serial; otherwise it is the session-only actor handle.
The individual snapshot is zero when unavailable. Mods that need a stable
individual variation across saves must wait for the durable identity and can
combine it with the run seed and `random_at`. The query is read-only, does not
advance game RNG, rejects recursive trait queries, and shares the enclosing
world-time sample. Unknown traits and stale handles fail without modifying the
output. It does not itself save an override or directly change actor behavior.

Version 1.1 lets a battle mod request registered trait keys with
`request_battle` while observing `RHH_WILD_ACTOR_ENGAGED`. Requests are
deduplicated, limited to `RHH_WILD_BATTLE_TRAIT_LIMIT`, and resolved in key
order immediately before the actor leaves the overworld. `battle` copies a
successfully resolved value during the verified battle and its
`RHH_WILD_ACTOR_BATTLE_ENDED` notification. Unrequested traits, invalidated
actors and mismatched opponent witnesses fail without changing the output.
The copied actor and optional wild individual ID describe the original actor;
they do not supply a permanent identity for the captured Pokémon. The host
restores both gameplay RNG streams and the last-used ability after trait
reducers; RNG-consuming proposals fall back to the original value while
denials remain effective. Queries also fail if the actor or its override
changes while reducers run. Request version 1.1 and check
`RHH_WILD_TRAITS_SERVICE_V1_1_SIZE` before using these functions.

`rhh:service/wild_trait_edits` version 1.0 manages up to eight saved trait
overrides per identified live actor. `at` returns the per-actor revision and
the tag's current owner and override (`INHERIT`, `OFF`, or `ON`). `preview`
and `apply` require a registered owner, a known tag, the current revision, a
durable individual ID and a safe overworld command context. An existing tag
override can only be changed or cleared by its owner. Changes increment one
revision shared by the actor's overrides, are checked by
`RHH_CAN_EDIT_WILD_TRAIT` constraints and emit `RHH_WILD_TRAIT_EDITED` only
after commit; no-op edits emit neither. `context` supplies copied before and
after values during those events. A stale revision, full actor record, denial
or invalid request changes no state. The owner key is a coordination convention
between trusted modules, not a security boundary.

Overrides follow normal save generations and roll back with the save; they are
not durable journal facts. They are discarded when the actor is removed or no
longer matches its saved individual ID, and missing owners or tags are pruned
on load. Thus they do not persist through despawn and respawn as a new actor.
The trait query applies a valid override before its transforms.

`rhh:service/wild_profile` version 1.0 stores up to eight signed 64-bit
values for each identified live actor. A field is addressed by a module
owner key and a namespaced field key; different owners may use the same field
key independently. `at` copies the value, presence, owner availability and
the actor-wide revision. An absent field still reports that revision. `preview`
and `apply` require a current revision and an available overworld command
context. They set or clear one field, run `RHH_CAN_EDIT_WILD_PROFILE`
constraints and emit `RHH_WILD_PROFILE_EDITED` only after a real change.
`context` supplies copied before and after values during both events. A stale
revision, full profile, denied edit or invalid request does not change state;
no-op edits emit no events. Owners are cooperative labels for trusted modules,
not an authorization boundary.

Version 1.1 adds `preview_batch` and `apply_batch` for up to eight distinct
fields belonging to one owner and actor. The host validates the final capacity
and invokes the existing per-field constraints in field-key order before any
write. If any constraint denies the batch, no field changes. A changed batch
advances the actor-wide revision once and sends per-field notifications after
all fields are committed; unchanged fields produce no notifications. During
those callbacks, `batch_context` copies the complete proposed batch. A preview
and a failed batch leave the profile unchanged. As with single-field edits,
the owner key is a cooperative label, and profiles follow normal save rollback.
`python3 native/tests/wild_profile_batch.py` tests the batch path with an
independent mod and cleans its temporary test build and saves.

Version 1.2 lets mods describe known fields with `RHH_WILD_PROFILE_SCHEMA`
constraint hooks. `schema_context` supplies the owner and field being queried;
`offer_schema` accepts a signed range or a Boolean range, a default and a
printable label. Identical offers compose; a malformed, denied or conflicting
collection fails closed. `schema` copies the result for a known field. Changed
single-field and batch writes must satisfy an available schema, while
undeclared fields keep their existing behavior, and schema validation does not
block deletion (other constraints can still deny it).
The default is metadata, not an automatically stored value. Existing saved
values stay readable if their schema changes or its owner is missing; a no-op
does not rewrite them. Their owners remain cooperative labels.

Version 1.3 adds `schema_list`, which collects up to 32 fields for an owner
from `RHH_WILD_PROFILE_SCHEMA_COLLECT` constraint hooks and returns them in
field-key order. Providers opt into discovery by offering each definition on
both the collection hook and the existing individual schema hook. The host
checks that every collected definition matches its individual query; conflicts,
invalid offers, mismatches and capacity overflow reject the whole list without
changing the output. A list can omit fields supplied by older modules that do
not implement collection, so it is not an exhaustive inventory of every field.

Version 1.4 adds `battle`, which reads the identified actor's profile as it
stood immediately before its overworld-to-battle transition. The host copies
the existing bounded profile once, so callers do not need to register fields
in advance. As with `at`, an absent field returns `present = 0` together with
the actor-wide revision and owner availability; saved fields belonging to a
missing owner remain readable. The query is available during the verified
battle and its `RHH_WILD_ACTOR_BATTLE_ENDED` notification. An unidentified
actor, a changed opponent witness, an invalid key or an undersized output
fails without modifying the output. The copy cannot be edited and does not
give the captured Pokémon a permanent identity. Request version 1.4 and check
`RHH_WILD_PROFILE_SERVICE_V1_4_SIZE` before calling it.

For example, a schema hook can copy `schema_context`, check the requested
owner and field, and offer a `RhhWildProfileSchema` with kind
`RHH_WILD_PROFILE_SIGNED`, minimum 0, maximum 12, default 4 and label
`"Alert radius"`. Use a `RHH_CONSTRAINT` hook and keep the definition stable
for the same mod settings. A Boolean definition uses minimum 0 and maximum 1.
On the collection hook, `schema_context` reports the owner and a zero field;
offer each discoverable field for that owner.

Profiles follow ordinary save generations and disappear when the actor is
removed or no longer matches its saved individual ID. A missing owner's values
are retained and still count toward capacity, so they become available again
if that owner returns. Mods must agree on field names and value meanings; the
host does not automatically change behavior. Profiles do not
survive despawn as a new actor and are not durable journal facts.

`rhh:service/wild_behavior` version 1.2 offers a step-driven decision point for
identified wild actors. After each accepted player step, the host gathers
`RHH_WILD_BEHAVIOR_PROPOSE` decisions for current-map actors within a default
12 tiles in each axis, ordered by their durable individual serial, while overworld commands
are available. Transforms may propose
`NONE`, one cardinal step, or `ENGAGE`; constraints may deny a proposal but
cannot rewrite it. Invalid actions are ignored. During each hook, `context`
copies the same player-step snapshot, the actor and its individual ID; nested
habitat, time, perception and trait queries may supply further inputs. All
proposals are collected before any of their actions run.

Version 1.1 first dispatches `RHH_WILD_ACTIVITY_COLLECT` constraints for each
actor. A callback can use `context` and the existing habitat, time, trait and
perception services to decide which activities are eligible, then call `offer`
with a copied `RhhWildActivity`. IDs must be unique in that collection and
owners must name loaded modules. Actions include `NONE`, the four directions
and `ENGAGE`; weights range from 1 to 1000, intervals from 1 to 65535 player
steps, and priorities from -1000 to 1000. At most 32 activities can be offered.
An invalid or duplicate offer, overflow, or a denied collection discards the
entire actor's activity set. Offers outside collection fail.

The highest priority wins. Within that priority, the host sorts by activity ID
and chooses by weight using the run seed, run-scoped individual ID and current
step divided by the shortest offered interval at that priority. The selection
is repeatable across reloads when the inputs and eligible offers are unchanged;
it does not advance gameplay RNG. A changed habitat, time or other eligibility
input can change the selection within an interval. The selected action is the
initial `RHH_WILD_BEHAVIOR_PROPOSE` value, so existing transforms and
constraints still compose with it. No offers select `NONE`. This is a
step-driven selection, separate from the optional saved queue below.

Version 1.2 adds `selected`, `at` and `change_context`. During the proposal and
action-result callbacks, `selected` copies the chosen offer, its effective
interval and next selection boundary; `valid` is zero when there was no valid
selection. `at` copies the last accepted activity for an identified live
actor. An accepted idle selection or successfully queued movement records the
activity's ID, owner, action, first accepted step and next selection boundary.
The boundary is a hint, not a promise of future movement or an automatic expiry.
Blocked, denied, stale and unavailable actions leave the record unchanged.
When a different accepted action replaces the chosen activity, the record is
cleared. A step with no selection and no action leaves the last record alone.

`RHH_WILD_ACTIVITY_CHANGED` observers run after a record changes or is cleared;
`change_context` copies the actor and before/after records only during that
event. The event key is the new activity ID, or the previous ID on clearing;
the subject is the actor's species, the ordinal is the player step, and
`args[0..1]` contain the new validity and action. Repeated acceptance of the
same ID, owner and action updates the boundary
without another change notification. The record follows normal save rollback,
is removed with its actor and is discarded on load if the actor identity or
owner is missing. It records acceptance, not completion of a movement animation
or offscreen simulation.

The host attempts each chosen action on the next overworld update, after the
engine has processed that player step. It rechecks the actor's identity and
position and uses the existing collision-checked movement or normal engagement
command. A script, save transaction, battle, animation, collision or changed actor can prevent
the action. `RHH_WILD_BEHAVIOR_RESULT` reports the attempted action and its
`RHH_WILD_ACTOR_*` status, including denials and failures; it does not claim a
movement animation has completed. No action produces no result. Only one
bounded batch can be pending, and a further player step while it is pending is
not proposed. A run change or load discards pending actions. The policy is
based on the player-step clock and map coordinates, not render visibility;
actors outside the configured radius or without a durable identity receive no proposal.
It does not replace the engine's existing frame-driven wild behavior or
provide automatic simulation outside the loaded map.

Version 1.10 adds revision-checked `policy_preview` and `policy_apply` for
the horizontal and vertical radii, each from 0 to 65535 tiles. The largest
value covers all representable coordinates on the current loaded map; it
does not load or simulate other maps. `policy` and `inspect` report the same
configured range used when the host collects the next batch. Edits require a
registered requester and a safe overworld context with no pending batch.
`RHH_CAN_SET_WILD_BEHAVIOR_POLICY` constraints can deny them, and
`RHH_WILD_BEHAVIOR_POLICY_CHANGED` reports a committed change. During those
callbacks, `policy_change_context` supplies the copied before/after policy.
Constraints cannot rewrite the change or consume gameplay RNG. The setting
persists with normal saves, retains its value when the requesting module is
absent, and follows rollback; older saves retain the 12-tile default. The
host still limits actor count, processes only identified loaded actors on
player steps and validates every movement through the engine.

Version 1.4 adds a saved queue of up to eight cardinal steps or engagement
commands for each identified actor. `queue_at` copies its revision, owner and
actions. `queue_replace` checks the exact revision and current map, rejects
conflicting owners and invalid actions, dispatches `RHH_CAN_EDIT_WILD_QUEUE`
constraints, then commits and notifies `RHH_WILD_QUEUE_CHANGED` observers.
`queue_context` copies the before and after state during those callbacks.
Owners are cooperative loaded-module labels, not a security boundary.
An empty replacement clears the owner while retaining the revision. Missing
owners' queues survive save/load but are suspended until the owner returns.
Queued actions take precedence over activity selection; normal behavior
proposals may override or deny them. By default, only the same action accepted by the
engine consumes the head and advances the revision. A blocked, denied,
overridden or unavailable action stays queued; acceptance does not imply its
animation has finished. Consumption also sends the committed queue notification.
The queue follows ordinary save rollback and is cleared on actor removal.

Version 1.5 adds `queue_route` and `queue_habitat_route`. They plan with the
same bounded, collision-aware route and habitat queries as the wild-actor
service, then submit the complete sequence through `queue_replace`, including
revision, owner and constraint checks. The result includes the route, habitat
sample when applicable, and queue change. An already-reached destination, no
path, search limit or path longer than eight actions leaves the old queue
untouched and returns a distinct status. Sideways stairs retain their cardinal
input from the route trace. This does not reserve the route or record its
expected landings on older saved queues.

Version 1.6 records expected origins, landings, elevations and facings for
newly queued routes. Before each unchanged queued input, the host checks the
actor's current position and the live engine's predicted landing. A changed
origin or landing reports `RHH_WILD_ACTOR_ROUTE_CHANGED` and leaves the action
queued; a collision or unfinished movement reports the usual blocked status.
`queue_route_state` copies the next expected and actual waypoints and a reason,
so a mod can distinguish a temporary blockage from a route that needs replanning.
The guard follows normal saves and is removed with the queue. Explicitly
replacing the queue with raw actions removes the guard, even if the directions
are unchanged. Older schema-1 queues migrate without a guard because their
original waypoints were not saved. No path is reserved: a move already accepted
can still be affected by changes during its animation, and later inputs recheck.

Version 1.7 lets a route opt into activity interruption using
`RHH_WILD_QUEUE_ROUTE_INTERRUPTIBLE` and an `interrupt_priority` from -1000 to
1000 in `RhhWildQueueRouteOptions`. The host collects normal activities on each
step and lets a selected activity with strictly higher priority run first;
ties leave the queued action in control. An interrupting action never consumes
the queue, even when it has the same direction. If it moves the actor, the
route guard can then require replanning. `queue_interruption` reports the saved
policy and revision. Existing route requests using the version 1.0 options
size, and routes without the flag, retain the earlier noninterruptible behavior.
Schema-1 and schema-2 queues migrate without an interruption policy.

Version 1.8 adds `RHH_WILD_QUEUE_ROUTE_PREFIX` for either route method. If a
found route is longer than the queue, it atomically commits its first eight
guarded steps instead of returning `RHH_WILD_QUEUE_ROUTE_TOO_LONG`. The result's
`route.length` still describes the complete planned path; `change.after.count`
is the number queued. Owner, revision, constraints and optional interruption
policy apply as usual. A prefix does not save its goal or reserve the remaining
path: without the v1.9 goal flag, the mod must retain its goal and replan after movement completes, and a
habitat replan can choose a different matching tile. Without the flag, oversized
routes retain their original all-or-nothing behavior.

Version 1.9 adds `RHH_WILD_QUEUE_ROUTE_RETAIN_GOAL`, `queue_goal`,
`queue_replan` and `queue_goal_context`. A retained point or habitat request
and its interruption policy are saved with the actor's queue. Its owner keeps
the reservation after the queued actions are consumed, including across saves;
other owners cannot replace it. The owner can explicitly replan using the
current revision, or clear it by replacing the queue without the flag. A
replan checks the actor's map and current movement rules; no path is reserved,
and no automatic or off-map movement is added. Other activities can move the
actor while its reserved queue is empty. Habitat radius is measured from
the actor's position at each replan and may select a different matching tile.
No-path, already-there, stale and denied replans leave the goal intact. Queue
constraints and committed notifications can inspect before/after goal snapshots
through `queue_goal_context`. Missing owners leave reservations suspended.
Older saved queues migrate without goals.

Version 1.11 adds the `RHH_WILD_QUEUE_ROUTE_ON_ARRIVAL` option for point and
habitat routes. `queue_completion` reports this policy and the current revision,
including for routes without a retained goal. The head remains queued while
its accepted movement animates; the host consumes it only after the actor reaches the predicted position,
elevation and facing. Interrupted, deviated, removed or lost movements do not
consume it; actor removal clears the entire queue. The committed queue notification precedes the step-finished
notification, whose `queue_consumed` field reports whether consumption occurred.
If a route is replaced during movement, the old movement cannot consume the
replacement. Movement tickets remain process-local: after loading a save,
the persisted head is retried only if its recorded origin still matches;
otherwise the caller can inspect the route guard and replan. Retained goals
preserve this option on explicit replans. Paths are not reserved, and the
caller remains responsible for deciding when to replan or clear a route.
Saves containing this option use queue schema 5, which older hosts cannot read;
saves without it continue to use schema 4.

Version 1.3 adds `policy` and `inspect`. `policy` copies the current step-clock,
current-map and identity requirements, the two-axis radius, actor and activity
capacities, and activity weight, interval and priority limits. Offscreen actors
on the loaded map are included; unloaded maps are not simulated. `inspect`
accepts a live actor handle and map-local coordinates for a prospective player
step, and returns the actor, available individual ID, coordinate offsets and a
bitmask of current gates. The gates identify a different map, distance,
unavailable identity or commands, an active save, a pending batch, active
dispatch, and missing behavior hooks. These are a point-in-time view: a clear
mask does not guarantee a later proposal, selection or successful movement.
During reducer callbacks the dispatch and command-unavailable gates can both
be set even for the proposal already being processed. Invalid or stale requests
leave the output unchanged. Request version 1.3 and check
`RHH_WILD_BEHAVIOR_SERVICE_V1_3_SIZE` before calling these functions.

`rhh:service/authored_encounters` version 1.0 exposes static wild encounter
tables for any known map, including unloaded maps. `variants` reports the
number of authored headers in source order; a map without a header returns
zero. `table` selects one variant, exact `RhhWildTime` and `RhhWildArea`,
returning the raw encounter-rate byte and ordered species/level slots. A
missing area or period succeeds with `present` clear; an invalid map, variant,
time, area or undersized output fails without changing the output. The morning
table is also the engine's default table. Fishing returns all ten slots; rod
selection is separate. Species keys can be zero when a source ordinal cannot
be resolved. Variant indices and species ordinals are not durable keys.

Those queries do not apply the engine's time fallback, a map's saved variant
selection, randomization, abilities, repel, weather, hooks or special scripted,
roamer and facility encounters. Encounter-rate bytes and slot positions are
not final probabilities. Queries do not load maps, generate Pokémon or use
gameplay RNG.

Version 1.1 adds `base_weights`, which copies the configured slot interval
widths for land, water, rock smash or one fishing rod. The indexed weights
refer to the slots in `table`; `first_slot` and `slot_count` identify the
selected range. A hidden-encounter chooser absent from the build is reported
as unavailable. These are nominal relative weights before lure, ability,
modification hooks and other live rules. RNG modulo effects and special
encounters mean they are not an exact prediction of the final distribution.

Version 1.2 adds `selection` for the ordinary saved map variant and the
engine's time-table policy, given an explicit period and area. It applies the
saved Altering Cave variant and its out-of-range clamp, and reports time-policy
flags and the resulting authored table. Time fallback checks the requested
period before generation-specific time remapping, as the engine does. A map
without a header succeeds with `has_header` clear and a zero table. This query
requires an active run and save outside a save transaction, but does not load
the map, sample the clock or consume RNG. It does not model facility or special
encounters, randomization, hooks or other rules that may affect an encounter;
the saved variant can also change after the query. Failed calls leave the output
unchanged.

Version 1.3 adds `preview`, a composable authored distribution for a tile on
any known map. It combines the saved ordinary table selection and base weights
with one authored-habitat, clock and saved-environment sample. An optional
`RHH_AUTHORED_WILD_WEIGHT` reducer can inspect `preview_context` for each
eligible slot and propose a relative weight; constraints can deny a slot but
cannot rewrite a proposal. Its ordinal is supplied by the caller. Gameplay RNG
and last-used ability are restored, with RNG-consuming proposals falling back
for that slot. If every slot resolves to zero, the result reports `fallback`
and retains the engine's baseline weights. Missing tables or unavailable base
weights report `available = 0`; invalid requests and changed sampled state
leave the output unchanged. Request version 1.3 and check
`RHH_AUTHORED_ENCOUNTER_SERVICE_V1_3_SIZE`.

This is a hypothetical distribution, not an encounter or a prediction of the
live engine: authored tiles omit saved map edits, scripts and actors, and the
query does not apply randomization, Repel, lead abilities, lure or the live
encounter hooks. Dynamic natural weather is not inferred. Callers choose when
and how to simulate, and must handle changes after the snapshot.

`rhh:service/wild_encounter` version 1.1 supplies copied context for
`RHH_WILD_CANDIDATE` and `RHH_WILD_GENERATED`. The candidate event runs once
after the engine selects a table slot and level and applies the relevant
repel and ability checks,
and after the existing wild randomizer has transformed that candidate. It
does not run during randomized-pool previews. Its context identifies the map,
source, area, slot, level, original table species, post-randomizer species,
saved creation ordinal and, when available, the resolved habitat snapshot.
The habitat is sampled at the actual overworld-actor spawn tile, the player's
tile for ordinary step encounters, or the tile in front of the player for
fishing. The map-local candidate coordinates are copied independently of the
habitat; negative coordinates can indicate an unavailable player position or
a tile outside the map. The habitat can be unavailable when no valid map or
tile is loaded. `table_time`
identifies the table selected by the encounter
engine; it is not a wall clock and may represent the default table.

Reducers may replace `decision.value` with a registered, creatable species key.
Constraints may deny a replacement but cannot rewrite it; denial or an invalid
result falls back to the post-randomizer species and does not cancel the
encounter. Ability-based slot selection is not rerun after a replacement.
This avoids bypassing encounter and double-battle rules. The
generated observer runs after the enemy Pokémon is created and the existing
`RHH_WILD_CREATED` notification has run. It reports the actual species, level
and OT/personality identity using the same creation ordinal. That identity
can collide and is not a durable actor ID. Generation of an overworld candidate
does not prove that the later actor allocation succeeded. Queries, rejected
generation before this point, static scripted Pokémon, roamers, outbreaks and
other paths outside the standard table/fishing generation do not produce these
events. Facility table entries are internal selectors, not normal candidates;
they no longer produce normal wild hooks or consume the creation ordinal.
Actor allocation is reported separately by the wild-actor service.

Version 1.1 adds `RHH_WILD_WEIGHT` and `weight_context`, invoked once per
eligible slot immediately before the normal land, water, rock-smash or fishing
slot draw. The context includes the original and current candidate species,
level range, base weight, slot range, rod, table time, map coordinates,
creation ordinal and available habitat. For stable-pool randomization the
candidate is the already-randomized table entry; per-encounter randomization
runs after slot selection. Fishing rules receive only the slots for the rod in
use. Feebas's special fixed encounter and other non-table paths do not use
this hook. Mods can use the copied environment or `random_at` to propose a
relative integer weight from 0 to 4095 in `decision.value`. Constraints can
deny a slot, making its weight zero, but cannot rewrite a proposal. Invalid
weights revert to that slot's original weight. If all slots resolve to zero,
or no weight changes, the host uses the original chooser; a no-op preserves
its original game RNG sequence. `weight_context` fails outside the callback or
for undersized output without modifying it.

Version 1.20 preserves both gameplay RNG streams and the last-used ability
across weight, encounter-rate, fishing-bite, spawn-interval and candidate
proposals. An RNG-consuming allowed proposal falls back to the engine value.
Weight proposals are checked per slot, so an impure slot does not discard
other slots' pure proposals; a constraint's denial still gives its slot zero
weight. Use `random_at` with explicit inputs for deterministic variation.
These guarantees cover the named proposal paths, not every encounter hook.
Habitat service version 1.7 also restores the last-used ability after habitat
reducers, in addition to its existing gameplay RNG checks.

Lead-ability slot selection happens first and can bypass weight proposals.
The existing lure rule still runs after the weighted draw and may reverse the
selected slot, including to one with zero proposed weight. Thus weights guide
the ordinary draw; they are not a guaranteed prohibition or encounter veto.
The selected candidate still passes the engine's level, repel and ability
checks before `RHH_WILD_CANDIDATE` and the committed generation notification.

Version 1.7 adds `RHH_WILD_ENCOUNTER_RATE` and `rate_context` for ordinary
land, water and rock-smash checks and the Pike and Pyramid checks. The copied
context identifies the map, player tile, area, source, encounter table time,
world step, raw table rate and the engine's final threshold. It also includes
the current habitat and its saved weather and terrain when available. The
threshold is measured out of 2880, after the engine's bike, flute, item, lure
and applicable lead-ability adjustments. A transform or finalizer may propose a
threshold from 0 to 2880; constraints may deny it but cannot rewrite it.
Invalid or denied proposals retain the engine threshold. Zero explicitly
prevents the roll from succeeding. The engine still consumes its normal one
random roll, and subsequent roamer, outbreak, repel and ability checks retain
their usual rules. The hook does not cover fishing, Sweet Scent, scripted
battles or the separate overworld-actor spawn countdown.

Version 1.8 adds `RHH_WILD_SPAWN_INTERVAL` and `spawn_interval_context` for
the separate visible-overworld-actor countdown. It runs when the engine
resets the minimum or recalculated timer, after its lure, active-count and
replacement rules. The context reports those inputs, player tile, map, saved
world step and available habitat, including saved weather and terrain. A
transform or finalizer can propose a countdown from 0 to 254 frames; 255 is
reserved for the engine's disabled sentinel. Denied or invalid proposals keep
the original countdown, and constraints cannot rewrite a proposed value. The
hook draws no game RNG and does not bypass encounter availability, placement,
repel, allocation or actor capacity checks. Changes in weather, terrain or
time affect the next timer reset; an already-running timer is not restarted.
It applies only when visible overworld encounters are enabled.

Version 1.11 adds `RHH_FISHING_BITE_RATE` and `fishing_bite_context` for
fishing. The copied context identifies the rod, the engine's percentage,
the active Sticky Hold/Suction Cups condition, the map and world step, and
the habitat at the tile in front of the player. A transform or finalizer may
offer a percentage from 0 to 100; denial or an invalid value retains the
engine's probability, and constraints cannot rewrite it. The ordinary roll
uses the engine's follower, proximity, time and current-generation ability
adjustments. In the older ability mode a separate preliminary roll is exposed
with its own phase, and the ordinary roll occurs only if it fails. Each roll
that occurs consumes the original RNG draw, even at 0 or 100 percent. This
does not bypass a missing fishing table, the minigame or later encounter
checks.

Version 1.14 adds `RHH_WILD_SCRIPTED_CLEANUP_FINISHED` and
`RHH_WILD_EVENT_CLEANUP_FINISHED` observers. For the first guarded denial in
the global script, the corresponding notification runs after that script's
explicit `end` has executed and player controls have unlocked. The copied
`context` and `cancellation_context` retain the denied candidate and first
reason, as in the earlier immediate notification. Script waits and temporary
suspension do not emit it. Reinitializing or replacing the script, changing
the run or map, or terminating without an explicit `end` discards it. The
script author must still perform appropriate cleanup before ending; the event
does not wait for asynchronous visual effects or verify the script's work.
Only `specialvar ... ScriptedWildBattleAllowed` in the global script context
queues this notification; direct calls and immediately executed local scripts
do not. The existing cancellation events keep their original earlier timing.

Version 1.19 adds a separate `RHH_CAN_BEGIN_TUTORIAL_WILD` constraint and
`RHH_WILD_TUTORIAL_CANCELLED` observer for the prepared catching-tutorial
opponent. `context`, `scripted_source` and `cancellation_context` report the
same copied information and first denial reason as other guarded encounters,
with source `RHH_WILD_TUTORIAL`. A denial restores the enemy party and both
gameplay RNG streams to immediately before opponent preparation. It does not
undo earlier scene actions. The global script's separate
`RHH_WILD_TUTORIAL_CLEANUP_FINISHED` event runs after an explicitly ended
denial script releases controls; interruption, script replacement or a changed
run or map discards it.

Wally's denial path restores the player's saved party byte-for-byte and music, leaves the
story pending, and lets the player speak to Wally to retry. Norman directs the
player back to Wally while it is pending. That state is part of the ordinary
game save; after a reload Wally's interaction is rebound to the current map
script. The old man's denial returns without
marking the tutorial complete or giving its reward. At the immediate Wally
permission and cancellation callbacks, the engine still has Wally's temporary
party loaded; the cleanup-finished event runs after the player's party returns.
The first rescue battle is
not a catching tutorial and is not covered by this permission hook. Direct
legacy tutorial starts without the explicit preparation and guard do not emit
the permission or cancellation events.

Version 1.18 adds `RHH_WILD_FIRST_BATTLE_READY` for Birch's rescue battle.
It reports the created opponent after battle initialization and form changes,
before the battle-start notification. The script's encounter key follows the
starter selection screen into the battle and is available through
`scripted_source` during the ready callback and `scripted_battle_source` from
battle start through its outcome. The event is observational; it does not
replace the opponent or allow the rescue sequence to be cancelled. An
unannotated starter selection has no authored key. A later selection replaces
the pending key, and a changed run or map prevents it from being carried into
the battle.

Version 1.17 adds separate `RHH_WILD_GHOST_READY` and
`RHH_WILD_TUTORIAL_READY` observers for the Pokémon Tower Marowak and the
Wally and old-man catching tutorials. The copied wild context reports the
actual opponent after preparation, with source `RHH_WILD_GHOST` or
`RHH_WILD_TUTORIAL` respectively. In particular, the Silph Scope path can
replace Marowak's prepared Pokémon; the notification reports the replacement.
The Pokémon Tower and old-man scripts are included only in FRLG builds; the
native Emerald build includes Wally's tutorial.
These are observations, not generation or permission hooks, and do not reach
existing scripted-ready subscribers. The scripts' authored keys are available
from `scripted_source` and `scripted_battle_source`; the catching tutorials
use preparation kind `RHH_SCRIPTED_DIRECT_SPECIAL`. Birch's first battle has
its own separate v1.18 event.

Version 1.16 adds `scripted_battle_source`, which copies the same authored key
from battle start through the battle end notification for annotated scripted
single, double and standard legendary encounters. It is available throughout
that active battle, including lifecycle callbacks, and fails without changing
the output for unrelated battles or outside a battle. The key follows the
scheduled battle transition and is discarded if the handoff, run, map or battle
type changes. It identifies the encounter site, not an individual Pokémon or
unique battle; use the battle service's battle ID for the latter. Version 1.17
also carries the Pokémon Tower ghost and catching-tutorial keys into battle.

Version 1.15 adds `scripted_source`, which copies the stable, namespaced key
of an annotated authored encounter and its preparation kind. Call it during
scripted or event-mon candidate, permission, cancellation, cleanup-finished or
scripted-ready callbacks; it fails without changing the output for unannotated
or unrelated events. Compare `encounter` with `RhhContentKeyOf` applied to an
ID from the [authored encounter catalog](docs/scripted_encounters.md). The key
identifies the story encounter, not a unique Pokémon or battle instance.
The ghost and catching-tutorial ready events are separately opted into in 1.17.

Script authors can put `identifywildencounter "namespace:encounter/name"`
before `setwildbattle`, `seteventmon`, a supported catching-tutorial special,
or `ChooseStarter`;
the next such preparation in that script consumes the annotation. IDs use lowercase ASCII letters, digits, `-`, `_`,
`.` and `/`, with one colon separating a nonempty namespace from the
`encounter/` path. The whole ID must be shorter than 64 bytes. Use a stable,
unique ID and preserve it when moving the script. Keys are hashes, so they are
not collision-proof. Both Southern Island Lati variants deliberately share an
ID because they represent the same authored encounter.

Version 1.13 adds `RHH_WILD_EVENT_CANDIDATE`, a separate species proposal for
`seteventmon` before the fateful event Pokémon is generated. It has the same
copied context and invalid/denied fallback as the `setwildbattle` candidate,
but a separate subscription; older candidate hooks do not see these events.
The authored level and item and the fateful-encounter flag are retained. The
hook does not itself cancel battle. Scripts and presentation may assume the
authored species, so transformations must account for those assumptions.
`RHH_CAN_BEGIN_EVENT_WILD` and `RHH_WILD_EVENT_CANCELLED` are separate
constraint and notification events for guarded event-mon preparations, with
the same context and rollback behavior described for v1.12 below. Existing
scripted-wild subscribers do not receive them.

Version 1.12 adds `RHH_CAN_BEGIN_SCRIPTED_WILD` and
`RHH_WILD_SCRIPTED_CANCELLED` for scripts that explicitly guard a
`setwildbattle` preparation. The constraint runs on each prepared opponent in
party order, after species replacement and generation, and sees the actual
species, level, OT/personality identity and slot in `context`. The map is
known, but area, table species, tile and habitat are unavailable. Its ordinal
is the ordinary wild-creation counter, not a unique encounter identifier.
Constraints cannot replace the Pokémon; their first denial reason is copied
to `cancellation_context`. If any slot is denied, the enemy party, count,
roamer index, both gameplay RNG streams and the prior single/double mode are
restored to their state before preparation, then one cancellation observer
runs. The observer runs before the script's cancellation branch restores
cutscene state and unlocks controls, so it must not assume those changes have
already happened. Neither the normal scripted-ready notification nor a battle
is started.

The guard currently covers the Pokémon Tower ghost, static Electrode and Voltorb encounters,
Zapdos, Moltres, Articuno, Rayquaza, Mewtwo, Mew, Deoxys, Lugia and Ho-Oh in Navel Rock,
Groudon and Kyogre in their caves, the Southern Island Lati encounter, the
three Regis, both Snorlax encounters, the Frontier Sudowoodo, Berry Forest
Hypno and Route 119/120 Kecleon encounters. Mewtwo restores map music; Kecleon
becomes invisible again. Cave legends restore their actor position and affine
sprite resources; Ho-Oh also restores the pre-cutscene saved weather and the
retry trigger. Southern Island removes the cutscene actor without changing its
hide flag and clears the temporary encounter flag. Mew's position, visibility,
priority and chase counter return to their pre-animation values. A denied
Deoxys encounter restores the solved triangle, so it can be retried during
the same visit; leaving the map retains the engine's normal puzzle reset.
An abnormal-weather
animation can restart when its saved weather is restored. If no affine matrix
is available, the cave cutscene uses the ordinary sprite instead of borrowing
another sprite's matrix. It deliberately does not apply to other scripts
automatically: some have already changed story state or need their own
cancellation path. Catching tutorials use the separate v1.19 permission family;
other event encounters may still be uncovered.
The ghost's check runs after the Silph Scope has replaced the prepared opponent,
so a veto sees the Pokémon that would actually enter battle. On denial the
original enemy party and RNG are restored and the locked scene releases; the
encounter can be retried. Its earlier scripted candidate can still differ from
that final opponent.
Script authors can place `guardwildbattle release` or `guardwildbattle
releaseall` immediately after `setwildbattle` or `seteventmon`, matching the
script's lock.
On denial it releases controls and ends the script; use it only when ending
there preserves the surrounding story and the player can retry. It must not
be separated from preparation by arbitrary commands, since the checkpoint does
not roll back arbitrary intervening script effects. The ghost script's audited
`PrepareMarowakBattle` special is an exception: it only prepares the opponent
and its changes are included in that checkpoint. Like other callbacks,
permission hooks must be read-only. The hook is optional; scripts with no
subscribed constraints retain their normal battle behavior.

Version 1.9 adds `RHH_WILD_SCRIPTED_CANDIDATE` for the `setwildbattle`
script command. It proposes each authored species in party order before any
opponent is created. The context exposes the species, level, slot and map,
but has no encounter table, known actor coordinates or habitat. A transform
may return an eligible registered species key; an invalid key or denied
proposal keeps the authored species. Constraints cannot replace the proposed
value or reverse a denial. The hook uses no gameplay RNG, leaves the authored
levels and held items intact, and does not cancel the battle or advance the
ordinary wild-creation ordinal. It does not cover opponents created through
other mechanisms. A mod changing a story encounter should account for any
species-specific assumptions in its script.

Version 1.6 adds the separate `RHH_WILD_SCRIPTED_READY` observer event for
scripted single and double wild battles and the standard legendary, Lati,
Groudon/Kyogre and Regi battle starts. It reports each prepared opponent in
party order immediately before the transition is scheduled. The copied wild
context has source `RHH_WILD_SCRIPTED`, phase `RHH_WILD_PHASE_CREATED`, the
actual species, level and OT/personality identity, and the current map key.
There is no encounter table or known wild tile: table species is zero, area is
`RHH_WILD_AREA_UNKNOWN`, coordinates are `INT32_MIN`, and habitat is unavailable.
The ordinal is the current ordinary wild-creation counter, which this event
does not increment and which is not a unique event ID. Identity can collide.
Corrupt or empty opponent slots are skipped. The ghost Marowak encounter and
catch tutorials use separate v1.17 events. This notification is observational: scripts
retain control of the encounter, and no legacy candidate, permission or
generation events are emitted for it. The separate v1.12 guard above adds
cancellation for explicitly guarded preparations. The v1.13 event candidate
above separately covers `seteventmon`.

Version 1.2 adds a separate `RHH_CAN_GENERATE_WILD` constraint after the
species proposal has resolved and before the Pokémon is created. Its
`context` copies the proposal with `species` set to the selected, validated
species; `event_key`, `subject`, `ordinal` and `args` identify the map, final
species, creation ordinal, area, source, slot and level. Only constraint hooks
are accepted. They can deny and provide a reason, but cannot rewrite the
species or undo an earlier denial. This preserves the older
`RHH_WILD_CANDIDATE` contract, where denial only rejects a replacement.
Candidate and permission callbacks share the same world-time sample.

A denied actual generation emits one `RHH_WILD_CANCELLED` observer event with
phase `RHH_WILD_PHASE_CANCELLED`. `cancellation_context` copies the candidate
and first denial reason during that event; both context functions fail outside
their corresponding callbacks or for undersized output. Cancellation emits no
`RHH_WILD_CREATED`, `RHH_WILD_GENERATED`, actor spawn or battle-start event
for the denied candidate, and does not advance the creation ordinal or replace
the enemy party. It restores the game RNG streams to the beginning of the
standard generation or fishing-selection call before notifying observers.
Earlier world-step encounter-odds rolls, fishing-minigame activity and an
already-generated first opponent are not rolled back. A cancellation ordinal
can recur; it is not a unique durable event ID. Observers may record the
notification in their own state, but must not treat it as an encounter or
photo reward.

Normal table land, water and rock encounters, ordinary generated overworld
actors, rod fishing and the special fishing Feebas path use the permission
hook. An explicitly requested actor spawn returns `RHH_WILD_ACTOR_DENIED` on
veto, distinct from allocation failure. Sweet Scent returns without starting a
battle. Fishing displays its normal "got away" message, restores field
controls and records a failed fishing attempt. If a second wild Pokémon is
denied, the already-generated first Pokémon is retained and the engine uses
its ordinary single-wild battle setup, including any partner rules. The hook
does not currently cover roamers, outbreaks, scripted/DexNav Pokémon, special
Feebas overworld actors, or Battle Pike/Pyramid encounters whose final Pokémon
is generated later. Pre-candidate repel or ability failures also produce no
cancellation event. Version 1.0 and 1.1 callers keep their original service
prefixes; check the 1.2 service version and size before calling
`cancellation_context`. `python3 native/tests/wild_cancel.py` exercises the
real encounter paths and an independent module; `python3
native/tests/wild_cancel_double.py` tests forced doubles in a temporary build.
Both remove their test builds when finished.

Version 1.3 adds an opt-in special-encounter contract for roamers, outbreaks
and overworld Feebas. `RHH_CAN_GENERATE_SPECIAL_WILD` is a constraint-only
hook; a denial produces `RHH_WILD_SPECIAL_CANCELLED` and an allowed generated
Pokémon produces `RHH_WILD_SPECIAL_GENERATED`. The same copied `context` and
`cancellation_context` operations apply. Special sources distinguish ordinary
roamers and outbreaks from their overworld-actor forms and overworld Feebas.
A roamer's slot is its engine roamer index, its saved species and level remain
fixed, and its OT/personality identity is available at permission time. An
outbreak uses `UINT32_MAX` as its slot and gains an identity only after
creation; overworld Feebas uses slot 255. Ordinary fishing Feebas remains on
the version 1.2 path. Special encounters do not run the species-replacement
hook, alter the legacy creation ordinal, or send the old normal-generation or
normal-cancellation notifications. This keeps v1.2 modules from receiving new
source kinds unexpectedly.

A veto restores both RNG streams, the enemy party and the selected-roamer
index to the checkpoint taken before the special selection; saved roamer and
outbreak data remain untouched. An earlier world-step encounter-odds roll is
not rolled back. Generated notifications describe the Pokémon, not proof of
successful overworld-actor allocation. An allowed roamer battle sends the
usual encounter-start notification once, so challenge rules consume their
attempt when battle begins; a denied roamer does not. A visible special actor
has already passed permission when spawned, so engaging it does not repeat
the veto. Scripted encounters and Battle Pike/Pyramid special
replacements remain outside this contract. Run `python3
native/tests/wild_special.py` for independent-module and Nuzlocke checks;
`native/tests/wild_actor_commands.py` exercises overworld roamers and outbreaks,
and `native/tests/wild_special_feebas.py` tests the optional overworld Feebas
configuration. The scripts clean their test builds.

Version 1.4 adds separately opted-in DexNav events:
`RHH_CAN_GENERATE_DEXNAV_WILD`, `RHH_WILD_DEXNAV_CANCELLED`, and
`RHH_WILD_DEXNAV_GENERATED`. Version 1.3 special hooks and earlier normal hooks
do not receive them. The proposal describes the searched species and level,
original land, water or hidden encounter table, the table time selected when
the search began, and the target tile in map-local coordinates. The slot is
`UINT32_MAX`; identity becomes available only after generation. A veto restores
the enemy party and RNG streams to the handoff checkpoint, ends the search,
resets its chain and shows the normal Pokémon-got-away message. Earlier search
selection, activity and RNG are not rolled back. An allowed search retains its
chosen moves, item, ability and potential, then starts the normal DexNav battle
script; generation is not itself an encounter-start notification. Run
`python3 native/tests/wild_dexnav.py` for the real search, cancellation, retry
and independent-module compatibility checks. It deletes its temporary build.

Version 1.5 adds `RHH_CAN_GENERATE_FACILITY_WILD`,
`RHH_WILD_FACILITY_CANCELLED` and `RHH_WILD_FACILITY_GENERATED` for Battle
Pike and Battle Pyramid, including generated overworld actors. Sources
distinguish the two facilities and their visible actors. The constraint sees
the final generated species, level and OT/personality identity, with slot
`UINT32_MAX`. On denial the enemy party, selected-roamer index and both RNG
streams return to their preselection checkpoint before cancellation observers
run; facility trainer-data pointers changed by open-level generation are also
restored. Earlier step odds and overworld tile selection are not rolled back. An
allowed Pike battle now sends the ordinary encounter-start notification once.

Pike and Pyramid first construct an internal provisional Pokémon. These
selectors are no longer exposed to normal wild randomization, weight,
candidate or creation hooks; Pyramid's selector species in particular encodes
an array index and cannot safely be randomized as an actual species. This
changes previous provisional-hook behavior and does not provide facility
species replacement. Facility events report the final Pokémon instead and do
not advance the legacy creation ordinal. A visible generated facility actor
in an active run retains the approved Pokémon for battle and save restoration;
generation is not proof that actor allocation succeeded. Actors saved before
these facility hooks were installed do not pass the facility permission hook.
Older saved Pyramid actors that stored a
selector in their level recover their level from experience at battle start.
Overworld encounters are disabled by default.
`python3 native/tests/wild_facility.py` tests normal paths and an independent
module on Linux or Windows; `wild_actor_commands.py` tests visible actors and
`wild_facility_random.py` tests the optional randomized Pyramid configuration.

Version 1.10 adds separate `RHH_CAN_GENERATE_MANUAL_FACILITY_WILD`,
`RHH_WILD_MANUAL_FACILITY_CANCELLED` and `RHH_WILD_MANUAL_FACILITY_GENERATED`
events for manually placed Pike and Pyramid actors. They run at interaction,
after the engine prepares the actual facility opponent and before battle
starts. The context reports the opponent and actor tile; the pre-existing
visible actor need not depict the generated opponent. A denial restores the
enemy party, facility trainer pointers and RNG streams before notification,
leaves the actor in place, and releases the interaction without starting a
battle. Earlier script movement and presentation are not rolled back. On
approval the same prepared opponent enters battle. These separate events do
not change the source kinds delivered to existing facility hooks.
All three remove their temporary builds.

`rhh:service/wild_actor` version 1.0 exposes copied snapshots of committed
overworld wild actors through `slots`, `at` and `find`. The snapshot includes
map, position, elevation, facing, species, level and appearance flags.
`RHH_WILD_ACTOR_SPAWNED` and `RHH_WILD_ACTOR_REMOVED` are observer events;
`context` provides the copied actor and cause while the callback runs. Spawn observers can query the live actor;
removal observers receive its final snapshot after it has been removed.
Generation and successful actor allocation are separate events.

Actor handles identify a lifetime in this process and become invalid on
removal, load or run change. Restored actors receive new handles and restore
events after `RHH_RUN_LOADED`; loading does not emit removal events for the
actors it replaces. An OT/personality pair is provided only for a preserved
Pokémon and can collide, so it is not a durable individual identifier. The
service exposes no engine pointers.

Version 1.18 preserves the gameplay RNG streams and last-used ability around
wild actor spawn, identification, engagement, removal and battle-end observers.

`rhh:service/wild_dormant` version 1.0 stores identified, preserved, ordinary
generated wild actors in a dynamically growing archive. `count`, `at` and `find` return copies ordered by individual serial;
the record includes its owner, map, tile, facing, elevation, species and
level. Owner availability is reported separately so an absent module does not
make its saved records disappear. A record is not a live actor handle.

`suspend` requires a registered owner, a committed run, a stationary actor,
an unblocked tile with matching non-transition elevation, and a safe overworld
command context outside a save transaction. It preserves
the generated Pokémon and associated trait, profile, activity, queue and
concealment records. The live actor is removed and a `RHH_WILD_ACTOR_REMOVED`
observer receives cause `RHH_WILD_ACTOR_SUSPENDED`; its individual is already
available through the dormant service during that notification. Roamers,
outbreaks, Feebas and manual actors are rejected because their special engine
state is not captured. The store is shared; it does not suspend
actors automatically.

`resume` requires the same registered owner and individual ID, and the saved
map must be loaded. It checks the saved tile is free and has the same elevation,
as well as object, sprite, palette and other placement resources. Unlike a
fresh encounter, restoration does not select a new Pokémon or require the
player's spawn distance or encounter terrain. Failure leaves the dormant
record and output unchanged. Success gives the actor a new session handle,
retains its individual ID and announces `RHH_WILD_ACTOR_SPAWNED` with cause
`RHH_WILD_ACTOR_RESUMED` only after its saved data is installed. Missing
concealment or activity owners cause those effects to be cleared on resume;
profile and queue data are retained. `discard` explicitly removes a dormant
record belonging to the supplied owner, without an actor event. Modules are
trusted, and owner keys are cooperative ownership rather than a security
boundary.

Dormant records persist with ordinary game saves; they are not journaled
immediately, and loading an older save can undo a suspension, restoration or
discard. An empty store adds no dormant archive to the new generation. Saves
with dormant records in any retained generation require a host that understands
the archive version, so older hosts reject them instead of silently dropping the
individuals. This service does not simulate dormant actors or reserve their
tiles. The
`wild_actor_commands.py` tests include a fresh-process save and restore and
remove their temporary build afterwards.

Version 1.1 adds explicit `relocate` and `relocation_context`. A registered
owner supplies a copied dormant snapshot and a destination map, tile and
facing. The host rejects a changed source location, invalid authored tile,
collision or ambiguous elevation; it then commits the new location and emits
`RHH_WILD_DORMANT_RELOCATED` with before-and-after context. A no-op emits no
event. Source checks compare location and facing, not a permanent revision:
an identical location reached again can match an older snapshot. A relocation
can cross maps and is an explicit placement, not proof of a traversable route.
It uses static authored map data, not saved map changes, live objects, scripts
or dynamic collision. The next `resume` checks the live tile again. Queued
actions and route goals remain attached; existing route guards can report a
changed origin and require replanning. Relocations persist on the next ordinary
save and can be undone by loading an earlier one.

Version 1.2 adds the read-only `step_preview` for an identified dormant actor
and a cardinal direction. It samples the authored source and adjacent tile,
including a unique cardinal map connection when the step crosses a seam.
The result distinguishes an open destination, authored collision, elevation
uncertainty, missing edge, unavailable data and overlapping connections. Flags
identify available tile samples, a map crossing and an authored source
collision or elevation differing from the saved actor. `OPEN` means only that the destination's
authored collision is clear and its unambiguous elevation matches the saved
actor; it is not engine movement permission. Scripts, saved map edits, other
actors, directional metatile behavior and special movement can change the
live result. The query does not relocate the actor, reserve a tile or consume
gameplay RNG. Mods can use its copied destination in an explicit relocation,
which performs its own checks.

Version 1.3 adds `profile_at` and `profile_list` for dormant individuals.
They copy fields and the actor-wide profile revision; lists are bounded and
ordered by field key. Fields from absent owners remain readable and report
that their owner is unavailable. These queries do not create or change fields.

Version 1.4 adds `profile_preview`, `profile_apply` and `profile_context`.
Edits use the same field schemas, owner and actor-wide revision rules as live
profiles. The preview and apply dispatch `RHH_CAN_EDIT_DORMANT_WILD_PROFILE`
constraints; a committed change dispatches `RHH_DORMANT_WILD_PROFILE_EDITED`
with copied before-and-after context. A no-op sends neither event. Existing
live-profile hooks receive a live actor handle and cannot safely be reused for
a dormant actor. Each loaded module with a live-profile constraint or observer
must therefore register the corresponding dormant constraint or observer
before a changed dormant edit is allowed. An unadapted module makes the edit
unavailable for every field, leaving it unchanged. This is cooperative compatibility:
the adapted module is responsible for carrying its rules across both events.
Constraints that consume gameplay RNG or change the last-used ability are
rejected and those values restored. Edits persist on ordinary saves, and
loading an earlier save can roll them back. Batch edits are not yet exposed
for dormant profiles in version 1.4.

Version 1.5 adds `profile_preview_batch`, `profile_apply_batch` and
`profile_batch_context`. A batch edits up to eight distinct fields for one
owner at one expected revision. Schema checks and per-field constraints run
before any write; a denial, invalid field or capacity failure leaves the
whole profile unchanged. On success the profile revision advances once, and
each changed field emits its committed notification after every change is
stored. Changes and notifications are ordered by field key. Both the
individual field context and the complete batch context are
available during those callbacks. A batch may remove and add fields at full
capacity; a no-op sends no events. The same legacy-hook compatibility and RNG
rules as single-field edits apply.

Version 1.6 adds `clock`, `clock_advance` and `clock_context`. Each newly
suspended individual starts with its processed step set to the current saved
player-step clock. `clock` copies that checkpoint, the current clock and their
nonnegative difference; it remains readable when the actor's owner is absent.
The registered dormant-record owner can acknowledge up to
`RHH_WILD_DORMANT_MAX_STEP_ADVANCE` pending steps in a safe update. The request
must match the observed checkpoint and cannot advance beyond the current clock.
A caller can make multiple acknowledgements; the limit bounds each command,
not the total work a module chooses to perform in an update.
A changed acknowledgement emits `RHH_WILD_DORMANT_CLOCK_ADVANCED` with copied
before-and-after context; a no-op emits no event. The host neither chooses nor
executes behavior, and the standalone checkpoint command is not atomic with
separate profile or location commands. Modules must handle failed work and decide when to
acknowledge it. Acknowledgements persist with ordinary saves and can roll back;
they are not durable receipts. Resuming or discarding an actor removes its
dormant checkpoint, so pending work must be handled or retained separately if
needed. A later suspension starts a new checkpoint at that step.
An old request can match the same checkpoint after a rollback or a new
suspension, so callers should not treat it as a unique lifetime token.

Older dormant archives migrate their checkpoint to the step in the matching
saved run snapshot; they do not retroactively create a backlog. The clock
counts player steps, not frames, elapsed real time or closed-game time. At
clock saturation no more steps accrue. Changes to the save format are rejected
by hosts that cannot read the newer dormant-record version.

Version 1.7 adds `work_preview`, `work_apply` and `work_context` for an
explicit transaction on one dormant individual. Its owner supplies a copied
expected actor, checkpoint and, when editing fields, profile revision; it can
include a bounded checkpoint advance, up to eight owner-keyed profile edits,
and an optional authored-tile relocation. All checks and profile constraints
run before any write. A rejected or stale transaction changes none of those
parts. A successful transaction installs all changes together, then sends the
existing relocation, per-field profile and checkpoint notifications in that
order. Observers can query the fully committed state, and `work_context` copies
the whole transaction during its constraints and notifications. The batch's
actor and relocation's `before` identify the original location; relocation
`after` and clock `after.actor` identify the resulting location. A preview and
a no-op do not commit or send notifications. The same legacy-hook compatibility,
schema, RNG, static-tile and step-limit rules apply as for the individual
commands. This is a transaction for facts supplied by the mod; it does not
choose behavior, prove a route, reserve live tiles or make changes durable
before an ordinary save.

Version 1.8 adds the `RHH_WILD_DORMANT_WORK_GUARD_PROFILE` flag. It checks
`expected_profile_revision` even when a transaction has no profile edits,
so a location-only or checkpoint-only decision based on a profile snapshot
is rejected as stale if any owner's fields have changed. Without the flag,
version 1.7 behavior is preserved; edits always check their expected revision.
Request service version 1.8 before using the flag.

Version 1.1 appends `spawn` and `remove`; request version 1.1 and check
`RHH_WILD_ACTOR_SERVICE_V1_1_SIZE` before using them. Version 1.0 consumers
can check `RHH_WILD_ACTOR_SERVICE_V1_0_SIZE`. Commands are accepted in a safe
overworld update, outside scripts, fades, link mode and battle. `spawn` takes
the current map key and a map-local tile and returns a copied snapshot only
on success. It requires automatic overworld encounters to be enabled, uses
the engine's encounter table, randomization, repel, ability, habitat,
distance, collision and graphics-resource rules, and does not replace an
existing actor when the generated-actor limit is full. It may consume game
RNG and emit generation notifications even when a later check or sprite
allocation fails. `remove` accepts a live handle and uses the normal despawn
path, including roamer movement and the removal notification. Removing a
manual actor does not permanently set its map-event flag. A stale handle,
wrong map, unavailable context, blocked tile and capacity failure are
reported separately; no command can be issued from an actor observer.
`python3 native/tests/wild_actor_commands.py` exercises real allocation and a
separately built mod in a temporary, automatically removed test build.

Version 1.11 adds `capacity`, a read-only snapshot for planning actor spawns.
It reports the current map, whether automatic encounters are enabled and
commands are currently allowed, generated and manual encounter counts, the
generated limit and next non-replacing slot, and active object and sprite
counts and limits. It leaves output unchanged when the world is unavailable
or the output is too small. Counts are point-in-time information, not a
reservation: map and tile rules, generation, graphics memory, palette
availability, other callbacks and subsequent world changes can still prevent
a spawn. The query neither creates an encounter nor consumes gameplay RNG.

Version 1.12 adds `spawn_preflight` for a map-local tile. It runs the same
initial availability, generated-slot, distance, collision, elevation and
encounter-tile checks used by `spawn` and `spawn_species`. Initialize both
structure sizes. A successful query returns a command status; on `OK`, it also
reports the current land/water area and free generated slot. Invalid query
structures and an unreadable world leave the output unchanged. A valid query
in an unsafe command context reports `UNAVAILABLE`. This is a point-in-time
check, not a reservation: it does not run species selection, mod proposals,
repel or ability checks, or test sprite and palette allocation. A subsequent
command repeats the checks and may still fail. The preflight does not generate
an encounter or consume gameplay RNG.

Version 1.13 adds `species_spawn_preflight` for a requested species and
level. It checks the same placement, content validity and special-only or
facility restrictions as the species spawn path. On `OK`, it reports the
area and generated slot plus whether the current Repel would block that level
and whether the current lead ability is eligible for its random encounter
filter if the spawn reaches it. Repel is checked first and can prevent that roll.
Those flags are observations, not a generated result: the query does not run
candidate proposals, the ability roll, Pokémon generation or graphics checks.
The command repeats its checks and can still produce a different outcome if
state or proposals change. Invalid structures and an unreadable world leave
output unchanged; both gameplay RNG streams are preserved.

Version 1.2 adds `step` and `engage`. Check
`RHH_WILD_ACTOR_SERVICE_V1_2_SIZE` after requesting version 1.2. `step`
requests one animated cardinal tile movement for a stationary, idle actor;
it uses the engine's movement and collision restrictions, and its collision
probe does not start encounters or remove other actors. A successful request
queues movement; the host releases its held action when the animation ends.
The actor's live snapshot shows its current position and the request can be
blocked if the actor is busy or the tile is unavailable. Other actors may move
after the check. If the game saves during a mod-directed step, the saved actor
is settled at its current destination while the live animation continues.
Version 1.10 adds `step_pending` and `RHH_WILD_ACTOR_STEP_FINISHED` with
`step_context`. An accepted step gets a process-local ticket and a predicted
landing including elevation and facing. The notification distinguishes an
arrival, a changed landing, an interrupted held action, removal, and a lost
actor; it includes the final actor snapshot when available. Steps selected by
behavior or a queue identify their source; a queue result includes its owner,
revision at selection and whether the command was actually consumed. This is
an observation after acceptance, not a reservation or rollback-proof receipt.
Pending steps and tickets are discarded on run change or reload, and a map or
restore transition can discard one before a completion notification. Observers
cannot issue actor commands directly; they can request replanning on a later
safe update. Check `RHH_WILD_ACTOR_SERVICE_V1_10_SIZE` before use.
`engage` requires a stationary player and actor on adjacent
tiles at compatible elevations and enters the normal collision interaction
script, including its repel and roamer rules. Success means that script was
queued, not that a battle has already started. `RHH_WILD_ACTOR_ENGAGED`
reports the actor after the engine begins its battle setup, including for
ordinary player-triggered interactions; its context uses the
`RHH_WILD_ACTOR_BATTLE` cause and can still be queried by handle. The actor is removed later during
the transition.

Version 1.3 adds `RHH_WILD_ACTOR_BATTLE_ENDED` and `battle_result`. Once the
engine has transitioned into the battle and returned from it, observers can
copy the original actor snapshot, semantic battle outcome and double, roamer
or Safari flags from `battle_result`. The original handle correlates this
event with engagement and removal, but it no longer resolves to a live actor.
In a double battle, the outcome describes the whole battle and does not
identify which opponent was caught. Results are transient notifications;
mods that need a history must persist their own state. A failed or abandoned
transition does not produce an outcome, and loading a save or starting a new
run discards any pending correlation. Request version 1.3 and check
`RHH_WILD_ACTOR_SERVICE_V1_3_SIZE` before using the appended function.

Version 1.15 adds `battle_catch` during `RHH_WILD_ACTOR_BATTLE_ENDED`.
When the host verified that the engaged actor was placed as a catch, it copies
the opponent party slot and the destination and Pokémon snapshot from that
placement. This also works for observers that do not subscribe to catch
placements. The copied destination may have changed since placement; it is
not a permanent individual identity or a durable receipt. Failure leaves the
output untouched and means the actor's capture is unknown, not that it was
not caught. In particular, catching the other opponent in a double battle
does not establish a capture of the engaged actor.

Version 1.16 adds `battle_actor` for a live battle originating from a visible
wild actor. It copies the original actor, its run-scoped individual ID when
available, and the opponent party slot, tied to the current battle ID. When
the actor is an active participant, it also copies the battle service's
participant snapshot. The participant is unavailable during a pending switch
or when that slot is not active. The query fails without changing its output
if the battle or source witness no longer matches. A battle ID and a
participant slot are process-scoped and do not identify an owned Pokémon.
Version 1.17 retains this association across the owner rekey performed by a
verified successful catch, even when no module observes catch placements. If
the final source and destination cannot be matched, the host does not update
the witness; a rekeyed source then makes the query unavailable.

Version 1.4 adds run-scoped individual IDs, consisting of the pair `run_id`
and `serial`. `individual` resolves a live handle to its ID, and
`find_individual` finds a currently live actor by ID. `event_individual`
copies the ID during actor spawn, restore, identification, engagement,
removal and battle-result callbacks when one is available. It remains usable
for a removal or battle result even though the session handle is then stale.
Request version 1.4 and check `RHH_WILD_ACTOR_SERVICE_V1_4_SIZE` before using
these functions; failed and undersized queries leave the output unchanged.

Version 1.5 adds `neighbors`, a pure, bounded neighborhood snapshot for a
live anchor actor. The request specifies a Manhattan radius from 0 to 64 and
can restrict results to the anchor's species or to actors with a durable
individual ID. It excludes the anchor and actors on other maps, but includes
hidden and offscreen actors. Use the perception service separately when sight
or obstacles matter. Results contain copied actors, distances and available
individual IDs, ordered by distance, then identified actors first, then
`run_id` and serial, and finally slot. Unidentified ties use slot order and
are not stable across actor lifetimes. Invalid requests and stale anchors leave
the output unchanged. The fixed capacity covers all live object slots. Request
version 1.5 and check `RHH_WILD_ACTOR_SERVICE_V1_5_SIZE` before calling it.

Version 1.6 adds `route`, a read-only breadth-first preview toward an in-map
tile or a tile adjacent to it. It returns the first cardinal direction and
distance in steps, or reports that the actor is already in range, no route was
found within four tiles of the starting point on either axis, or the 256-state
search limit was reached. Equal-length routes use north, west, east, south
order. The preview applies the engine's current tile, elevation, movement-range,
actor-collision and overworld-encounter restrictions without triggering a battle
or removing actors. Sideways stairs use the engine's diagonal destination and
facing while returning the cardinal input needed to take that step. Both the
preview and the movement command reject a diagonal landing outside the map.
Other actors and the map are sampled at query time; a later `step` rechecks
the actual scene, and can still fail. The actor must be stationary, on the
current map, with a live sprite and active player. Invalid or unavailable
requests leave the output unchanged. Request version 1.6 and check
`RHH_WILD_ACTOR_SERVICE_V1_6_SIZE` before calling it.
`python3 native/tests/wild_route.py` exercises preview purity, obstacles and a
real committed movement; it removes its temporary build and save.

Version 1.7 adds `route_habitat`, which searches reachable actor positions for
a permitted habitat matching all, any and excluded flags. The target must be
within the requested Manhattan radius of the actor's starting tile, up to
five tiles; the actor can stop on it or next to it. This can find an accessible
match when a nearer matching tile is enclosed. It uses the same route bounds,
movement restrictions and tie order as `route`, checking the current tile
then north, west, east and south for each reachable position. Habitat results
are cached within the query, and all reducers share one time sample. The result
includes the chosen habitat sample, route, and sampled and denied tile counts;
no target is returned for an exhausted search. Invalid, unavailable and
recursive habitat requests leave the output unchanged. This is still a preview:
live movement and future world changes can invalidate the suggested step.
Request version 1.7 and check `RHH_WILD_ACTOR_SERVICE_V1_7_SIZE` before use.

Version 1.8 adds `spawn_species` for a requested species and level on a
map-local tile. Request version 1.8 and check
`RHH_WILD_ACTOR_SERVICE_V1_8_SIZE` before using it. It requires a creatable,
enabled species and a level from 1 through 100. It uses the same safe-context,
automatic-encounter, tile, distance, capacity, repel, lead-ability and graphics
checks as `spawn`; special-only configurations and the Pike and Pyramid
facilities reject this regular encounter. It does not choose a roamer,
outbreak or Feebas encounter. The normal species randomizer and candidate
transforms can change the requested species, and wild-generation constraints
can deny it. Their `RHH_WILD_OVERWORLD_ACTOR` context identifies the requested
species in `table_species` and uses `RHH_WILD_NO_TABLE_SLOT` for `slot`, because
there is no table entry. The returned actor reflects the resolved species.
A denial restores both RNG streams and the prepared enemy party before the
cancellation notification. As with `spawn`, later resource or allocation
failure can occur after generation notifications and RNG use. On success,
the generated Pokémon is preserved with the actor for ordinary saves and its
eventual battle.

Version 1.9 adds `route_trace` and `route_habitat_trace`. These use the same
bounded searches and return the same results as the earlier methods, plus
every predicted step in order. Each step gives the cardinal command and the
predicted landing tile, elevation and facing; a sideways stair may produce a
diagonal landing and facing. The starting position is in the route's actor
snapshot. The trace contains `route.length` entries for a found route and is
empty for already-in-range, no-path and limit results. Both outputs require
initialized `struct_size` fields and remain unchanged on failure. This is a
snapshot, not a queued or reserved route: mods must still call `step` and
recheck the scene as the actor moves. Request version 1.9 and check
`RHH_WILD_ACTOR_SERVICE_V1_9_SIZE` before use.

IDs are not exposed before the run's first successful save. Existing actors
receive `RHH_WILD_ACTOR_IDENTIFIED` after that save; new actors in a saved run
receive it on a subsequent overworld update once their ID range is safely
journaled. The event uses the ordinary actor context and
`RHH_WILD_ACTOR_ID_ASSIGNED` cause. A restored actor with saved identity
already has its ID during the restore event. An actor restored from an older
save without an ID receives one after the next successful save, so reloading
the old save cannot change an ID that was already exposed. IDs are reserved in batches, so
unused serials are expected, and the journal prevents serial reuse when an
older native save slot is recovered. A journal failure reports
`RHH_PERSISTENCE_FAILED` and leaves new IDs unavailable until persistence
recovers on a later successful save. The guarantee is scoped to a run in a native save lineage:
independently copied saves can diverge and issue the same IDs, and the host's
64-bit run IDs are not globally collision-proof.

An ID follows an actor until it despawns; a later spawn gets a new ID. Saved
generated, manual and special actors can retain their actor IDs across a
reload. Only ordinary generated actors currently retain the full Pokémon
instance: manual, roamer and outbreak battle generation keeps its existing
behavior. Thus their actor IDs must not be treated as proof that the same
Pokémon attributes were preserved.

`rhh:service/wild_concealment` version 1.0 gives each live wild actor an
optional presentation policy. `at` returns its policy and the ground-cover
flags at its current tile. `HIDDEN` suppresses the sprite without changing the
engine's script-controlled invisibility or collision; `GROUND` enables the
engine's existing grass, sand and shallow-water covering effects when the
actor stands on a suitable tile. Ground mode does not invent a cover graphic
on other tiles. Presenters should honor the hidden flag in both wild-actor and
overworld actor snapshots. Raw sprite pixels remain available for inspection.

`preview` and `apply` take a registered module's content key as the owner and
the policy's current revision. A different owner cannot replace or clear an
active policy, and an outdated revision is rejected. The owner key is a
coordination label, not authentication of the caller. Preview and apply both
run `RHH_CAN_WILD_CONCEALMENT` constraint hooks; only apply stores a successful
change and dispatches `RHH_WILD_CONCEALMENT_CHANGED`. The service's `context`
returns a copied before/after result only during those hooks. A repeated
identical request is a no-op. Conditions may reveal the actor on its next
actual movement or when the player is within a Manhattan radius of 1–16 tiles
at a compatible elevation. Movement is observed during simulation and the
change notification is sent on the following overworld update. Automatic
reveals do not pass through the proposal hook. A concealed actor keeps normal
collision and battle behavior; use other cues when the actor must be accessible.

Policies follow ordinary game saves and actor lifetimes. They are restored only
when the saved actor still matches and the owner is registered; otherwise the
actor is shown. Session handles and revisions are not persistent identifiers;
use the individual ID when available. Clearing or revealing a policy does not
clear script-controlled invisibility. The native ground effects are the
existing engine effects, and do not provide a general-purpose occlusion or
animation system.

`rhh:service/perception` version 1.0 queries sight between the player and a
wild actor, or between two wild actors. Each wild endpoint uses a live actor
handle; stale handles, the same object at both endpoints, unavailable maps,
invalid requests and undersized outputs fail without changing the output.
Results copy the current wild snapshots and their run-scoped individual IDs
when available. Coordinates use the loaded map's local metatiles and each
wild endpoint must originate from that map. The player can cross a connected
map boundary without its object-event source map changing. Snapshots expose
species to mods and are not an information-security boundary for an
unidentified actor.

The required maximum distance is a Manhattan distance of 0–64 tiles. The
query always checks distance and the engine's elevation compatibility.
Optional checks add a 90-degree forward-facing cone, scene occlusion, collision
or other active actors. Scene occlusion treats each annotated `OCCLUDES` tile
as a full blocker; collision treats any nonzero collision tile as a blocker.
Actor blocking considers both current and previous occupied tiles at compatible
elevations, skips the endpoints and logically hidden objects, and reports the
lowest matching object slot on a tile. A center-to-center supercover checks
both neighboring tiles when the line crosses an exact corner. The first
blocker records its tile, applicable blocker flags, actor slot and scene-rule
key. Origin and destination tiles are excluded from blocker checks. When a
scene check examines a tile without a scene rule, `SCENE_UNANNOTATED` signals
that the result depends on default rather than explicitly annotated geometry.
A target beyond the requested range does not trigger a tile trace.

`LINE_CLEAR` reports the selected spatial checks. `PERCEIVABLE` additionally
requires the target not to be hidden by a script or wild-concealment policy.
The target's off-screen, missing-sprite and actual sprite-invisibility flags
are separate, because an off-screen Pokémon can still perceive the player.
The cover flags describe the target's tile; they do not guarantee that a
particular renderer obscures it. This is a 2D tile approximation, not a
pixel-accurate camera or 3D line-of-sight test: scene rules may be incomplete,
actor sprites have different sizes, and the current animation can lag the
tile position. Perception queries do not consume RNG, start battles, record
observations or award items.

`rhh:service/scouting` version 1.0 adds a host-owned observation log for up
to 256 run-scoped wild-actor individuals. `count`, `at` and `find` return
copied records, including after an actor despawns. Records expose the first map,
world step and observation count; their species key is zero until identified.
This protects the log's presentation, not information from trusted native
modules: actor and perception snapshots still contain the species. Actor IDs
must already be available, so an actor cannot be recorded until the run and
its identity have been saved. IDs retain the wild-actor service's save-lineage
limitations, and manual or special actors do not necessarily preserve the
same Pokémon instance when they enter battle.

`preview` and `apply` accept a live actor handle, action, maximum distance and
optional perception checks. They require a safe overworld command context,
and reject an actor that is hidden, offscreen, lacks a visible sprite, is out
of range or fails the requested line-of-sight checks. `OBSERVE` adds one
observation; `IDENTIFY` requires a prior observation and reveals the species.
Identification is idempotent. `RHH_CAN_SCOUT` constraint hooks can impose
additional policy, such as a minimum number of observations; they cannot change
the result or undo a denial. `context` returns copied before/after records and
the full sight sample only during that constraint or the `RHH_SCOUT_RECORDED`
committed notification. Preview, denial and repeated identification send no
committed event. A nested scouting command is unavailable. Observation neither
starts a battle nor consumes an encounter, and it does not use gameplay RNG.

The log is stored with the next normal game save and rolls back with that save;
the 256-entry limit rejects new individuals without evicting old records.
Observation count saturates with an error. The notification and count are not
rollback-proof event IDs and must not be used alone for exactly-once rewards.
A successful observation reveals an eligible ordinary overworld encounter;
R while facing its visible disturbance invokes the same transaction after the
run has been saved. This service does not capture images, store photos, award
items or experience, or provide a player-facing viewfinder. `python3 native/tests/scouting.py`
tests the transaction and a fresh-process save; `--target windows64` runs
the same test under Wine. Both remove their temporary builds and saves.

Version 1.1 adds `preview_photo` and `apply_photo` for a recently captured,
actually visible subject, including an actor visible only in a custom
presentation. Pass the latest successful photo ticket and a normal scouting
request. The host rechecks the same run, individual, sprite, map, step, camera,
billboard and visibility inputs before and after the scouting constraints.
Ordinary changes to the same sprite's animation frame do not invalidate the photo.
Normal range, line-of-sight, concealment and observation-before-identification
rules still apply. `photo_context` returns the ticket during those callbacks.
One photo can commit one observation and one identification; preview and denial
do not consume it. Photo evidence is process-local, discarded on reload or a
new successful capture, and is not an exactly-once reward receipt. A capture
without a reliable visible-subject mask supplies no evidence.
The v1.1 operations require an exact mask and return
`RHH_SCOUT_EVIDENCE_QUALITY` for approximate evidence without running policy
hooks or consuming it. Version 1.3 adds `preview_photo_geometric` and
`apply_photo_geometric` for callers that explicitly accept a pre-filter
approximation; they accept exact evidence too. The usual scouting policy
hooks still run, and v1.2 `photo_quality` identifies the quality during them.
Policies which require exact visual evidence should check that quality.

`rhh:service/photo_capture` version 1.0 accepts one pending request for a live
wild actor with an available run-scoped individual ID. The request names a
registered module owner, a handle, maximum distance and optional perception
checks. It requires a safe, unobstructed overworld command context and a
visible, onscreen sprite. `RHH_CAN_PHOTO` constraints can deny the request;
the copied `context` supplies its sight and target information. The owner key
is a coordination label, not authentication. An accepted request receives a
process-local ticket; a ticket is not a durable photo or reward identity.

At the end of that frame, the host checks the run, world, actor identity,
positions and visibility again. An invalidated request emits `RHH_PHOTO_FAILED`;
changing runs discards the request. A successful request emits
`RHH_PHOTO_CAPTURED`, whose `context` includes the frame, backend and copied
sight data. `copy_rgb` copies 240×160 top-to-bottom RGB24 pixels into a
caller-owned buffer of at least `RHH_PHOTO_RGB_BYTES` bytes. It works only
during `RHH_PHOTO_SCORE` and the successful notification; neither the host buffer nor the event is
an album. The software backend converts the presented RGB555 game frame;
OpenGL reads the final composited frame, including a selected custom world
presenter and protected game interface. A failed GL read does not silently
substitute the original frame. No gameplay RNG or capture attempt is consumed.
The image is not automatically saved and the service does not award rewards.
Version 1.1 adds `request_presented` for a potentially visible actor that the
original 2D engine has culled. It retains the same identity, line-of-sight,
concealment and policy checks and rejects link sessions. If the sprite is hidden,
the request only permits culling from an ordinary native movement callback. The request is provisional: the
capture succeeds only when a compatible active presenter provides a valid mask
for that exact frame with at least one visible subject pixel. Software output,
missing or incompatible masks and complete occlusion produce
`RHH_PHOTO_NOT_VISIBLE`; a changed actor or concealment produces
`RHH_PHOTO_STALE`. `requires_presenter` reports this request mode during the
photo callbacks. Ordinary scouting commands still use the original visibility
checks; photo-based scouting is described above.
Version 1.2 adds `request_presented_geometric`, which also accepts an
approximate pre-filter geometric mask from compatible blur and other local
filters. Its subject visibility is an approximation, not an exact count of
final output pixels. `request_mode` exposes the chosen mode during photo
callbacks, including the `RHH_CAN_PHOTO` policy check. During the resulting photo-based scouting proposal and
notification, scouting v1.2 `photo_quality` reports whether the evidence is
`RHH_PHOTO_MASK_EXACT` or `RHH_PHOTO_MASK_GEOMETRIC`.
Version 1.3 adds `render_context` during `RHH_PHOTO_SCORE` and
`RHH_PHOTO_CAPTURED`. It identifies the capture ticket, frame and backend,
the presenter that drew the scene, the environment shader and the ordered
world passes submitted for that frame. A shader that fails to compile is
excluded. The depth flag indicates a valid presenter depth texture was available
to world shaders; it does not guarantee a depth readback. Software captures
have no presenter or shader passes. The viewfinder overlay is excluded, and
the snapshot is unavailable outside these callbacks or if the renderer cannot
verify the matching frame. It is not archived and does not measure focus.
Version 1.4 adds `presenter_variant`, a capture-scoped, presenter-defined
content key identifying its actual rendering variant. Zero means unspecified;
the presenter defines and versions the meaning of its keys. Software and
fallback scenes report zero. Version 1.5 adds `request_geometric`, which uses
ordinary visibility checks but opts into approximate pre-filter subject masks
when available. Unlike presented-only requests, it can still capture when no
compatible mask is available, including with the software renderer.

`rhh:service/projected_visibility` version 1.0 supplies one asynchronous
probe at a time. `request` takes an owner and a live wild-actor handle and
returns a ticket; `result` reports pending, unavailable, stale, or the subject
silhouette and visible-pixel counts from a compatible presenter's next frame.
Only the latest ticket is retained; a new request replaces a completed result.
`cancel` releases a pending ticket with its owner key. Owner keys coordinate
modules but do not authenticate them. An actual photo has priority over a probe.
The host checks the run, actor, sprite, step and camera before publishing or
returning a result; no gameplay or photo state is changed. The result is an
advisory frame sample, not a reservation: occluders and effects can change
afterward, so commands and photos must revalidate. Software rendering,
unsupported presenters and shader passes without compatible photo masks report
unavailable.
Version 1.1 adds `request_geometric` to opt into approximate geometric masks,
and `quality` reports the quality of a current completed result.
`python3 native/tests/photo.py` checks software and OpenGL presentation on
Linux, including a custom presenter; `--target windows64` checks software
capture under Wine. Both remove their test builds and temporary files.

`rhh:service/photo_score` version 1.1 provides an optional score from 0 to
1000. The default is an estimate: 1000 minus 80 per tile of distance (capped
at 800), minus 100 if the target has ground cover. Flags identify ground
cover, missing scene annotations and omitted perception checks. This is a
tile-based estimate, not proof that the subject is clear in the image.

`rhh:service/photo_visibility` version 1.0 optionally reports the target's
on-screen sprite silhouette and the pixels where it is the topmost game layer
in a software capture or an OpenGL capture of the unchanged software frame.
`context` returns total and visible pixel counts;
`copy_mask` copies a 240×160, row-major byte mask (one means topmost). These
calls are available only during that capture's scoring and successful capture
callbacks. A custom presenter can supply both masks using the optional
`RhhWorldFrame` version 1.3 photo target; the host validates the mask values,
ticket and frame, and removes original interface pixels from the visible mask.
World effects and render passes retain the mask only when each active shader
declares `RHH_SHADER_PHOTO_PRESERVES_POSITION`. They fail for presenters that do not supply masks, unmarked
effects, untracked frames, a changed OAM mapping, or a subject with no
measurable on-screen pixels. The counts include sprite
subparts and account for the software renderer's windows, sprites and
background priorities. They exclude off-screen pixels and describe layer
ownership, not brightness or actual light contribution through blending.
They describe pre-effect ownership rather than opacity from weather particles,
focus or color. Exposure uses the final captured RGB frame. A score
transform may use the visible-to-total ratio when available and retain the
estimated score otherwise; the host does not change the default score.
Version 1.6 separately exposes `geometric_context`, `copy_geometric_mask`
and `copy_geometric_silhouette` for an opted-in geometric capture. Exact-mask
views and derived exact measurements remain unavailable for that capture.
Version 1.7 can also expose a presenter's same-frame depth at visible subject
pixels through `depth` and `copy_depth`, during scoring and capture callbacks.
The copied 16-bit map is top-row-first, with excluded pixels set to zero;
use the matching visibility mask to distinguish excluded pixels from zero
depth. The summary includes the ticket, mask quality, count, minimum, maximum
and sum. Depth uses the presenter's normalized buffer units, not world distance
or a general focus score. Approximate masks are returned only for captures that
explicitly opt into them. Software capture, missing or incompatible presenter
data and subjects with no visible pixels do not provide depth.
Version 1.8 adds geometric exposure, framing, footprint, composition and
detail calls for an opted-in approximate mask. Framing, composition and full
footprint describe pre-filter geometry; the exposure and detail calls sample
the final image at those approximate locations, which may include neighboring
pixels mixed by blur. The full footprint remains optional and is validated
against the on-screen silhouette. These calls are unavailable for exact-mask
captures and leave outputs unchanged on failure. The original methods still
require an exact mask.
Version 1.1 adds `exposure`, which measures the captured RGB values at visible
subject-mask pixels: mean, minimum and maximum luma and counts of very dark or
bright pixels. The luma formula and thresholds are defined in the public
header. A completely obscured subject returns zero pixels and zero values.
This describes the rendered colors, including software compositing; it cannot
separate a dark sprite from poor illumination, and is unavailable without a
reliable subject mask. The host's default score is unchanged.
Version 1.2 adds `copy_silhouette`, a mask of the on-screen subject before
occlusion, and `framing`, which returns separate silhouette and visible bounds
as half-open pixel rectangles. An entirely hidden subject has empty visible
bounds. Both use the same capture and availability rules as the earlier
methods; outputs are unchanged on failure. The silhouette mask and visible
mask can be compared to locate occluded pixels. Bounds are clipped to the
viewport, so touching an edge alone cannot establish how much lies off-screen.
Version 1.3 adds `footprint`, which optionally reports the whole pre-occlusion
silhouette's pixel count and signed, half-open bounds, along with the count
inside the viewport. A scoring rule can compare those counts to estimate how
much of the subject is cropped. It is available only in the same capture
callbacks and leaves its output unchanged on failure. The software renderer
extrapolates the sprite geometry beyond the viewport and checks its on-screen
part against the captured silhouette; it declines the measurement if the
sprite data or relevant display state changes during the frame.
These are geometry measurements before occlusion and post-processing, not a
measure of visible light or clarity.

Compatible custom presenters may fill the optional `footprint` in the
`RhhWorldPhotoTarget` v1.1 extension after checking its `struct_size` and
pointer. Supply the capture ticket, signed bounds and pixel counts, and set
`total_pixels` only when the full silhouette was measured. The host checks
the metadata against the validated on-screen mask but cannot independently
verify presenter-supplied pixels outside the viewport. A presenter that omits
the extension continues to provide its existing masks without a footprint.
Version 1.4 adds `composition`, which describes the on-screen silhouette and
visible masks separately. Each distribution provides a pixel count, half-pixel
coordinate sums for its centroid, and counts in a row-major three-by-three
viewport grid. The precise cell and centroid conventions are in the public
header. An obscured subject has an empty visible distribution. As with the
earlier masks, this is pre-effect layer ownership, is clipped to the viewport,
and is available only during the corresponding score and successful capture
callbacks. A mod can combine it with the optional full footprint rather than
mistaking an on-screen centroid for the center of a cropped subject.

Version 1.5 adds `detail`, which measures absolute luma differences between
horizontal and vertical neighbors where both pixels belong to the visible
subject. It returns the pair count, sum, maximum and count with a difference
of at least 32, alongside the capture ticket. Empty and isolated masks have
zero pairs. The measurement is available in the same callbacks and leaves the
output unchanged on failure. It describes the final captured colors, so a mod
can use it as one input to a scoring rule, but flat artwork, lighting and
postprocessing affect it and it does not establish whether a subject is in
focus. The host does not change its default score.

`RHH_PHOTO_SCORE` transforms can replace `decision.value`; constraints can
deny scoring but cannot rewrite that value. An out-of-range value falls back
to the default, and denial leaves the photograph available but unscored. The
score context contains the revalidated capture-time sight, habitat and world
time snapshots when available. Mods can inspect the actual composited RGB
frame through the capture service during scoring. Context initially contains
the baseline score; during `RHH_PHOTO_CAPTURED` it contains the final score,
before a photo ID has been assigned. Scoring consumes no gameplay RNG: an
RNG-consuming proposal is discarded and both streams restored, while a denial
still leaves the photograph unscored. Photo permission constraints follow the
same restoration rule; an allowed impure request is rejected.
`find` returns the score of a stored photo while it remains in the album.
Version 1.1 adds `recent`, a copied record for the latest successful capture,
including its reserved photo ID, target individual and score even when no
album entry was made. It is replaced by the next successful capture and cleared
when a save is loaded; failed captures leave it intact. The serial is zero if
durable ID reservation failed. The metadata is not itself saved.
Scores and images follow the same saved generation; albums from before scoring
remain readable and their existing photos are unscored. Saving with the new
required host state prevents older executables from silently discarding scores.

`rhh:service/photo_album` version 1.0 stores up to 99 full-resolution RGB24
frames in a host-owned archive. During `RHH_PHOTO_CAPTURED`, `store` accepts
that event's ticket and copies the frame and metadata into the album; it can
store each capture once. The receipt includes a run-scoped photo ID and the
actor's individual ID, species, map, owner, world step, renderer and frame.
`count`, `at`, `find` and `copy_rgb` query copied metadata and pixels; `remove`
deletes an entry. A full album rejects new photos without evicting old ones.
`RHH_PHOTO_STORED` and `RHH_PHOTO_REMOVED` notify observers after the in-memory
change; `context` is available during those callbacks. The album owner key is
a label, not authorization, and trusted mods can read and remove any entry.

For ordinary-save rewards, `rhh:service/photo_reward_simple` offers `claim`
with a `RhhPhotoRewardRequest` during the owner's `RHH_MOD_UPDATE` observer.
An album photo can earn an item once per owner, rule, species and location.
A positive minimum score requires a saved score; zero also accepts unscored
photos. A full bag or failed claim leaves both the bag and claim flag unchanged.
The item and flag are saved together on the next normal save; loading an older
save restores its earlier state. The existing `photo_reward` service continues
to offer durable, individual-based rewards when those are needed.

The photo ID serial is reserved through the durable journal before a capture
is delivered. A journal failure emits `RHH_PERSISTENCE_FAILED` and leaves
album storage unavailable for that capture. The run/serial pair is protected
against normal native save-slot recovery within one save lineage; independently
copied saves can diverge and reuse IDs. The photo and its deletion are saved
only on the next successful ordinary game save. Both retained native save
generations carry their own matching archive, so recovery also restores that
generation's album. The format preserves all RGB bytes; 99 images occupy about
10.9 MiB per retained generation, plus metadata. There are no per-photo files.
Old saves without an archive remain readable. Older executables that do not
understand an archive reject that save instead of silently dropping photos.
The host can retain several named archives per generation; a photo-only save
keeps the original album format. A multi-archive save requires an executable
that recognizes every stored archive. This is host infrastructure, not a
general-purpose archive API for modules.
`python3 native/tests/photo_album.py` checks the archive, corruption rejection,
save recovery, ID allocation and capacity on Linux; use `--target windows64`
for Wine. The tests remove their temporary builds and saves.

`rhh:service/photo_reward` version 1.0 grants an item for a scored photo of an
actor with a durable individual ID. It accepts an album entry or the most
recent successful capture while its transient metadata remains available. A
mod supplies its registered owner key, a stable rule key, an item key,
quantity and minimum score. Each
owner and rule can reward an individual once, even if a later photo is used.
`preview` checks the same conditions without granting the item; the
`RHH_CAN_CLAIM_PHOTO_REWARD` constraint can deny either operation. `claim`
records a durable receipt before adding the item to the normal bag. A failed
journal write leaves both the ledger and bag unchanged. An active run must
already have been saved, and commands must run in a safe overworld state; a
capture callback should queue a claim for later.
Item-reward constraints that consume gameplay RNG have both streams restored;
an allowed impure request is rejected and a denial remains denied.

The ledger holds up to 128 receipts per run. `count` and `at` expose them;
`delivered` means the receipt was applied to the current save, not that the
item remains in the bag. If an older save is loaded, the journal restores
earned receipts and the host delivers each outstanding item once during a safe
overworld update. If the bag is full, delivery waits for space, and new claims
wait behind the outstanding receipt. Mods can also call `deliver` explicitly.
Claimed and delivered events have a copied result available through `context`.
A receipt persists even if the album entry is later deleted. Separate copies
of a save are independent lineages and cannot coordinate rewards.
Version 1.1 adds `preview_allow_unscored` and `claim_allow_unscored`: these
accept an unscored album photo or the latest unarchived capture only when the
minimum score is zero. The original methods still require a score. For an
unscored receipt, the legacy `score` field is zero; `score_available` and
`context_score_available` distinguish that from an actual zero score. The
availability marker is part of the durable receipt and survives rollback.
Executables predating version 1.1 cannot read a ledger containing an unscored
receipt.
`python3 native/tests/photo_reward.py` checks validation, persistence failure,
bag-full recovery, item consumption and rollback; `--target windows64` uses
Wine. The runner cleans up its temporary builds and saves.

Version 1.2 adds durable experience rewards. `preview_experience` and
`claim_experience` take a fixed, already-scaled amount, a current party
experience snapshot, and the same photo, owner, rule and minimum-score inputs.
Set `RHH_PHOTO_EXPERIENCE_ALLOW_UNSCORED` to accept an unscored photo with a
zero minimum. A uniquely matched living non-egg recipient and a positive
award after the current cap are required to claim. Item and experience claims
share the once-per-individual, owner and rule restriction; a pending receipt
must be delivered before another reward can be claimed.

The host durably records the experience receipt before starting the normal
party level-up, move-learning and evolution flow. `experience_count` and
`experience_at` expose up to 128 experience receipts and distinguish an
unscored photo from a score of zero. `RHH_CAN_CLAIM_PHOTO_EXPERIENCE` can deny
a preview or claim; `RHH_PHOTO_EXPERIENCE_CLAIMED` and
`RHH_PHOTO_EXPERIENCE_DELIVERED` have copied results through
`experience_context`. The delivered marker is updated with the Pokémon's
experience before award observers run and is saved with that game generation.
An older save replays an outstanding receipt through the same level-up flow;
the cap and eligibility are checked again at delivery. The receipt stores the
requested amount, so a changed cap may change the amount actually applied.

The recipient is matched by OT/personality across the party, PC, daycare and
fusion storage. Delivery waits until exactly one valid match is in the party,
is alive and can gain experience. This pair is not a permanent individual ID:
collisions, identity changes and indistinguishable copies cannot be resolved
automatically. A failed handoff after the menu opens leaves the receipt pending
and requires an explicit `deliver_experience` retry; loading a save also
reenables automatic retry. Separate copied saves cannot coordinate delivery.

Version 1.3 lets a mod retarget the current undelivered experience receipt.
`preview_experience_retarget` and `retarget_experience` take a copied current
receipt and a current experience snapshot for the proposed recipient. The
recipient must be a unique, living party Pokémon able to gain experience, and
the receipt must still match and be next in the delivery queue. Permission
constraints use `RHH_CAN_RETARGET_PHOTO_EXPERIENCE`; the committed notification
is `RHH_PHOTO_EXPERIENCE_RETARGETED`. Both expose copied before/after results
through `experience_retarget_context`. A successful retarget appends a durable
change before enabling delivery; a failed journal write leaves the original
recipient in place. The host retains up to 128 changes per run, and reports a
full ledger rather than overwriting history. Reassigning to the current
recipient is a no-op and enables another delivery attempt.

The latest retarget follows the receipt when an older save is loaded, so the
new recipient must also exist uniquely in that save before delivery can
proceed. Already-delivered receipts cannot be retargeted in the current save.
Each save generation records its actual delivery recipient, so a later retarget
from an older save does not change the recipient reported by a save in which
the reward was already delivered. Older count-only delivery records migrate
using the original recipient. This still uses OT/personality rather than a
permanent Pokémon ID. Old saves without retarget history load with an empty
history. Saves written with this
required host state cannot be loaded by executables that do not recognize it.
`python3 native/tests/photo_experience.py` (or `--target windows64`) tests
journal failure, ambiguity, retargeting, normal level-up and save rollback,
then removes its builds and temporary saves.

For ordinary generated encounters, the engine retains the actual generated
Pokémon and uses it for the battle, including its personality, IVs, moves and
shiny appearance. Its state is saved with the actor and restored as part of
the native save transaction. Old saves or mismatched actor records have no
preserved Pokémon and retain the engine's generation fallback. Manual actors,
roamers and outbreaks retain their existing battle-generation behavior.
Automatic overworld encounters remain disabled by default and can be enabled
with `WE_OW_ENCOUNTERS`; the native path uses the generated Pokémon's shiny
state for its appearance instead of making a separate shiny roll.

`rhh:service/overworld` provides read-only snapshots of the loaded
map, individual metatiles and active object events. It copies the current
raw OAM sprite image or a map tile as RGBA into a caller-owned buffer; each
metatile includes its eight source tile entries for reconstructing layers.
Map coordinates are local metatile coordinates; actor screen coordinates are
pixels. `actor_slots` reports the addressable object slots. `struct_size` must
be set on each output. Map/actor graphics IDs and
animation frame numbers describe the current engine state and must not be
persisted as stable content keys; an object slot can be reused. The sprite
copy reflects current VRAM and palette contents and omits affine transforms,
subsprites, occlusion and composited effects. Callers should refresh views
when the world changes. Version 1.0 remains supported.

Version 1.1 adds `camera`, `billboard`, `sprite_part` and
`sprite_part_rgba`. Request `RHH_SERVICE_VERSION(1, 1)` and check the service
table size against `RHH_OVERWORLD_SERVICE_V1_1_SIZE` before accessing the
appended pointers. A consumer built with this header that only needs version
1.0 can check `RHH_OVERWORLD_SERVICE_V1_0_SIZE` instead of `sizeof(*service)`.
The camera view maps local
map pixels to the 240×160 viewport with `screen = world + origin`; it follows
the existing camera and pan, including smooth movement. Each billboard reports
its ground anchor in both spaces and its visual centre, with transient `x2` /
`y2` offsets separate so a jumping sprite can retain a ground shadow. Sprite
parts preserve the engine's component order, positioning, flips and priorities;
their copied RGBA pixels include transparency. Affine transforms, blending,
mosaic, window and disabled OAM modes are signalled as flags but are not baked
into those pixels. Slots and snapshots can change each frame; do not retain them as actor
identities. This is a set of scene inputs, not an actor mutation API or a
complete geometry and lighting system.

Version 1.2 adds `effect_slots`, `effect_at`, `effect_part` and
`effect_part_rgba` for live sprites using recognized engine field-effect
object templates. The copied view reports a frame-local slot, template ordinal,
visibility, camera-offset flag, anchor and part count. Tall, long and short
grass have distinct kinds; other templates are reported as OTHER. Template
ordinals and slots are not persistent identities. This does not identify every
possible field effect, determine an associated actor, or bake unsupported
OAM modes into pixels. A presenter must account for each effect's placement
and blending rather than assume all are billboards.

Version 1.3 adds `effect_at_v1_3`, which also classifies tracks, ripples and
splashes, including grass-jump variants. The 1.2 `effect_at` still reports
these as OTHER. The approximation does not add water
simulation or infer the meaning of unclassified field effects.

Version 1.4 adds `actor_shadow`, a read-only view of an actor's graphics shadow
size, the engine conditions that currently suppress its shadow, and any live
matching engine shadow effect. Suppression bits distinguish hidden actors,
explicit disablement, graphics without shadows, weather, water, tall grass,
hot springs and sand piles. The effect's slot is frame-local and can be absent
even when no suppression applies. This view does not create or destroy field
effects or determine scene material. Other renderers can interpret the conditions differently.

Version 1.5 adds `lighting_inputs`, a single read-only snapshot of the current
map, indoor/outdoor/underground/underwater context, natural engine weather,
saved mod environment, and effective clock time. The clock and saved
environment have separate availability flags; unavailable nested snapshots are
zero. The query does not dispatch habitat hooks or change game time. Consumers
choose their own lighting model and handle unavailable inputs.

Version 1.6 adds `local_light_slots` and `local_light_at` to expose the
engine's live lamp, Pokemon Center sign and Mart sign sprites. Each copied
view has current-map tile and pixel coordinates, a frame-local sprite slot,
kind and visibility. A hidden light is still queryable; a consumer should not
assume it emits light. The service reports the engine source, not authored
light colors, radii or stable identities. Modules can also declare map-local
lights as described below.

Version 1.7 adds `actor_visibility`, a copied view of the engine's off-screen
cull, object invisibility, sprite invisibility, wild concealment, link state
and whether the sprite callback matches the native movement type. These are
separate inputs rather than a guarantee of visibility: sprite invisibility can
also be set independently of the cull.
Ordinary scouting and photo requests still use the engine's original visibility
checks; the opt-in presented-photo request can validate a projected target.

Version 1.8 adds `sprite_part_transform` and `effect_part_transform` for the
current actor or recognized field-effect part. They copy source and draw
dimensions, screen position, and the signed 8.8 OAM sampling matrix. For
non-affine parts the matrix is identity and the source pixels already contain
the engine's flips. Affine double-size mode doubles the draw rectangle. These
are current CPU sprite and matrix values, not a promise that latched hardware
OAM contains the same frame.

`sprite_part_transformed_rgba` and `effect_part_transformed_rgba` also rasterize
the same part into tightly packed draw-rectangle RGBA8888, applying affine
sampling and transparent clipping. Disabled parts are transparent. Mosaic,
blending and object-window composition are still reported by the existing
part flags and are not baked into this copy. The caller supplies a buffer of
at least `draw_width * draw_height * 4` bytes; failures leave it unchanged.
Double-size parts can be up to 128×128 pixels; world-scene version 1.11 can
trace their full raster without downsampling.
Request version 1.8 and check `RHH_OVERWORLD_SERVICE_V1_8_SIZE`.

Version 1.9 adds `display_state`, a copied sample of the latched display,
background, blend, mosaic and window registers for the loaded overworld map.
It also decodes blend mode, layer target masks, raw five-bit blend coefficients
and mosaic block dimensions. These values describe the register state at the
call, not the pending CPU register buffer or the result of compositing each
sprite. Registers can change during a frame or per scanline, so one snapshot
does not prove that an entire captured frame used those settings. The window
coordinates and masks remain packed register values. Queries fail without
changing the output when the overworld is unavailable or the output is too small.

Version 1.10 adds `sprite_part_raster_rgba` and `effect_part_raster_rgba`.
They accept explicit 1–16 pixel mosaic dimensions and an optional mosaic flag;
the effect applies only when the part itself has `RHH_WORLD_PART_MOSAIC`.
Sampling uses the screen-anchored mosaic grid before the part's affine
transform, including negative screen coordinates. The caller can supply the
object mosaic dimensions from a `display_state` sample. Like the earlier
transformed raster, this produces a full draw rectangle without viewport,
window or layer composition. The inputs are current CPU sprite state and a
caller-selected display sample, not a reconstruction of scanline changes.

Version 1.11 adds `display_frame`, which copies the latched controls sampled
immediately before each of the 160 rows of the latest completed software
overworld frame on the current map and run. Its process-local sequence identifies
that capture. The query fails when no matching completed frame is available and
leaves the output untouched. A sample may predate the current UI or a newly
constructed scene, and it does not establish that the software frame was
presented or that current CPU sprite data matches its latched OAM. It reports
registers, not the final blended pixels or the results of custom presentation.

`rhh:service/world_scene` version 1.0 resolves a tile's declarative scene
annotation alongside the copied `RhhWorldTile`. Request it through
`get_service`, initialize `RhhSceneTile.struct_size`, and call `tile_at` with
local map coordinates. The returned map and rule are stable content keys; a
zero rule means the default flat, solid ground and flat upper layer receiving
shadows. Queries fail outside the loaded overworld or before finalization, do
not modify their output on failure, and do not change gameplay or RNG.

Modules declare immutable `RhhModSceneRule` arrays with `RHH_MOD_ABI_SCENE`
and `scene_rule_count` / `scene_rules` in their definitions. Rules match any
combination of map key, current-engine metatile ID and behavior; a position
match also requires a map. Omitted match fields and reserved fields must be
zero. Map aliases resolve to the same map. Among matches, higher priority
wins, then more match flags, then the lexicographically smallest rule ID. The
host rejects duplicate IDs and malformed or unresolved map references before
activation. Keep the array and its strings unchanged until unload. The current
limits are 256 rules per module and 2048 in total.

For example, a reusable metatile annotation could be declared as follows; the
metatile ordinal is tied to the current game content and should be checked
against the target tilesets:

```c
static const struct RhhModSceneRule water[] = {{
    .struct_size = sizeof(struct RhhModSceneRule), .id = "example:scene/water",
    .match = RHH_SCENE_MATCH_METATILE, .metatile = 42,
    .style = {RHH_SCENE_WATER, RHH_SCENE_UPPER_FLAT,
              RHH_SCENE_RECEIVES_SHADOW, 0, 0}
}};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SCENE,
    .id = "example:scene", .scene_rule_count = 1, .scene_rules = water
};
```

Heights are signed world pixels, with 16 pixels per metatile and positive
height upward. Ground height ranges from -2048 to 2048; vertical upper layers
require an upper height from 1 to 2048, while flat and hidden upper layers
require zero. Styles identify a solid or water ground, flat/billboard/wall/
roof/hidden upper artwork, and shadow-receiving, shadow-casting and occlusion
flags. They are renderer inputs; the host does not build meshes or infer a
physical height from the engine's elevation ordinal. Older definitions,
including `RHH_MOD_DEFINITION_MUSIC_SIZE`, retain their original layouts.

### Overworld presentation

`rhh:service/world_scene` version 1.1 can turn resolved scene tiles into
bounded textured quads. `mesh_at` resolves the current tile and rules, builds
ground and upper surfaces, and adds vertical ground edges where an adjacent
tile is lower. It emits no edge at the map boundary. `build_mesh` performs the
same conversion on a caller-supplied scene without neighbor edges. Both copy
their results and leave outputs unchanged on invalid input. Version 1.0's
`tile_at` prefix remains compatible.

The mesh uses map pixels with x east, y south and z up. Each quad identifies
one original 8×8 tile entry for `overworld->tile_rgba`; vertices correspond to
the decoded image corners in reading order. Flat upper surfaces share ground
height, so a renderer must resolve layer ordering. Roofs use the configured
height; north, south, east and west walls stand on their corresponding edge.
Billboard surfaces sit at the tile center and must be rotated around that
center to face the camera. Ground-edge textures stretch the higher tile's
ground art and are an approximation. Quads have explicit normals and are
double-sided; no winding is promised. Scene flags describe shadow and
occlusion intent, not automatically generated shadow maps or depth buffers.
The host does not infer geometry from ambiguous tile artwork. Renderers must
handle those cases or fall back to the 2D scene.
`python3 native/tests/scene_mesh.py` tests the shapes, Route 103's authored
map tiles and an independently compiled module; `--target windows64` runs under Wine. The test cleans its
build and temporary files.

Version 1.2 adds `window`, `window_tile_at` and `window_mesh_at` for the
engine's loaded render grid around the current map. Bounds are inclusive and
in current-map tile coordinates. They include the cells the engine loaded from
connections and its border fallback; tile graphics and attributes use the
currently loaded tilesets, matching the existing 2D renderer. Outside the
current map, the host supplies default flat scene styling rather than guessing
which connected map owns a cell or applying that map's annotations. The bounds
do not promise every tile is usable: missing border data makes the affected
query fail. These views are for presentation, not movement or encounter rules.
The original 1.0 and 1.1 queries retain their current-map bounds.

Version 1.3 adds `window_resolve` and `window_resolved_mesh_at`. They can apply
scene rules from a connected map when the last connection covering a cell has
compatible tilesets and the loaded block matches its authored source. Overlaps
follow the engine's connection order. The result identifies current-map,
connected-map, border or unknown origin, and includes source-map coordinates
for the first two. The scene and mesh coordinates still use the current map's
coordinate space. Border cells and cells whose source cannot be established
retain default styling. Matching authored data is evidence, not proof that a
cell was never changed and restored; dynamically edited connection cells and
incompatible tilesets may therefore need explicit presenter handling. Resolved
meshes use resolved neighbor heights, including across the connection seam.
The version 1.2 window functions retain their original default styling outside
the current map.

Version 1.4 adds `lights_at` and `light_count` for mod-authored lights. A module
using `RHH_MOD_ABI_SCENE_LIGHTS` may append `scene_light_count` and
`scene_lights` to its definition. Each declaration has a unique namespaced ID,
a stable map key, map-local pixel x/y, height above the annotated ground,
radius in pixels, nonzero `0xRRGGBB` color and intensity from 1 to 1000. Radius is
1–256 and height is 0–256 pixels. A zero period mask is always active; a
nonzero mask uses bit `(RHH_TIME_* - 1)`. Declarations and their strings must
remain unchanged until unload. For example:

```c
static struct RhhModSceneLight lamps[] = {{
    .struct_size = sizeof(struct RhhModSceneLight), .id = "example:light/route_lamp",
    .x = 8 * 16 + 8, .y = 12 * 16 + 8, .height = 24,
    .radius = 80, .rgb = 0xffbb66, .intensity = 700,
    .period_mask = 1u << (RHH_TIME_NIGHT - 1)
}};
/* In the module entry, before returning a static RhhModDefinition: */
lamps[0].map = host->content_key("rhh:map/route103");
/* Set abi_version = RHH_MOD_ABI_SCENE_LIGHTS, scene_light_count = 1,
 * and scene_lights = lamps in that definition. */
```

The host validates map identities and unique light IDs, permits at most 64
lights per module, 256 overall and eight on one canonical map tile, and
rechecks the tile limit after content aliases are finalized. Coordinates outside
the authored map do not resolve to a scene tile. `lights_at` takes
current-map tile coordinates and an effective `RHH_TIME_*` period; pass zero
when time is unavailable, in which case only always-active lights are returned.
Its bounded copied batch is ordered by ID and gives current-map pixel positions
and absolute scene height. Connected cells use their confidently resolved
source map and styling; uncertain cells do not supply a neighboring map's
lights. Out-of-window or malformed requests leave output unchanged. A tile
with no matching current-map declarations returns an empty batch without
decoding that tile's graphics. `light_count` reports the number of registered
declarations across maps so a presenter can skip empty scans. These
lights are static declarations with optional time filtering.

Version 1.5 adds `trace_light`, a read-only segment query over loaded scene
meshes. It tests quads marked `RHH_SCENE_CASTS_SHADOW` and returns the nearest
intersection and its tile, quad and normalized distance. Set
`RHH_SCENE_TRACE_ALPHA` to sample the current decoded tile alpha; without it,
cast-shadow quads are treated as opaque. Endpoints do not self-occlude. Both
endpoints must be within the loaded window, each x/y separation is limited to
512 pixels, and z coordinates must be between -4096 and 4096 pixels. The result's
`fraction_millionths` gives the distance along the segment in millionths;
invalid or unavailable queries leave output unchanged. The
query uses the same connected-map attribution as resolved meshes and does not
invoke gameplay perception hooks. `shadow_caster_count` reports the number of
registered rules that can cast shadows, as a conservative fast-path hint.

Version 1.6 adds the opt-in `window_infer` and `window_inferred_mesh_at`
presentation views. Explicit scene rules always win. For otherwise unannotated
current-map or confidently attributed connected cells, water behavior suggests
a water surface. Sign behaviors are sampled from the currently loaded tile
pixels: a mostly opaque ground layer and a partially transparent upper layer
suggest a 16-pixel billboard that casts a shadow. The result reports the pixel
counts and inference flags; ambiguous signs remain flat. Border and uncertain
cells are not inferred. This heuristic is approximate and cannot classify
general buildings, trees, cliffs or roofs. It changes neither map data nor
gameplay, and the older resolved views and `trace_light` retain their declared
rule behavior.

Version 1.7 adds `trace_inferred_light`, with the same segment bounds, alpha
sampling, tie ordering and failure behavior as `trace_light`. It tests the
cast-shadow quads from `window_inferred_mesh_at`, including inferred sign
billboards, so a presenter can query that geometry without changing the legacy
trace or gameplay perception. Ground underneath an inferred sign does not
itself cast a shadow. As with the underlying inference, this is an approximation
of loaded art, not a general scene graph or a guarantee about ambiguous scenery.

Version 1.8 adds `trace_billboard` for an individual presenter-positioned
sprite part. Supply segment endpoints and the four map-space corners of its
double-sided parallelogram in texture order, plus its 1–64 pixel dimensions.
With `RHH_SCENE_TRACE_ALPHA`, supply the part's RGBA8888 pixels, byte stride
and buffer capacity; alpha values of at least 128 block the ray. Without that
flag the part is opaque and the pixel buffer fields must be zero. A hit
reports segment fraction, UV and the nearest-filtered pixel. A miss reports
`CLEAR`, fraction 1000000 and unavailable pixel coordinates. Intersections at
or within 0.01% of either endpoint are ignored to avoid self-occlusion. The
byte stride is limited to 4096 and coordinates to finite values within
±1048576 pixels. Invalid, degenerate or non-parallelogram geometry and invalid
pixel buffers leave the output unchanged.

This pure query does not sample the active scene or determine which actors are
visible. The presenter supplies the same geometry and current sprite-part
pixels it renders, accounting for its camera, transforms and concealment. It
can compare billboard hit fractions with `trace_light` or
`trace_inferred_light` for scene occlusion. Those existing scene traces do not
automatically include billboards, and this API does not create a shadow map.

Version 1.9 adds `trace_directional_light` for a ray from a receiver toward a
directional light. Supply a finite direction with components within ±1048576
and length at least 1e-6, plus a maximum distance up to 512 pixels. The start
must lie inside the loaded window and the height bounds. The host normalizes
the direction and uses the same cast-shadow
mesh, alpha and nearest-hit rules as the segment traces. Set
`RHH_SCENE_DIRECTIONAL_INFERRED` to use inferred meshes. The returned endpoint
and measured distance describe the checked segment, while coverage flags say
whether the loaded scene window or the ±4096-pixel height bounds shortened it.
A clear result applies only to that segment; it is not proof that the sky or
unloaded maps contain no occluder. A ray directed out of the window from its
edge can validly return zero checked distance. Invalid input or unavailable
scene state leaves the output unchanged. The returned endpoint can also be
used with `trace_billboard` to compare sprite and terrain intersections.

Version 1.10 adds `trace_directional_shadow`, which combines that scene trace
with up to 64 caller-positioned sprite billboards on the same checked segment.
Each caster has a nonzero request-local ID and the same geometry and optional
alpha requirements as `trace_billboard`. The result identifies the nearest
mesh or billboard and retains the scene trace's endpoint and coverage flags.
Ties at the reported millionth precision prefer the mesh, then the lowest
billboard ID. Every caster is validated, including when an earlier hit or a
zero-length clipped ray is already known; invalid input leaves output unchanged.
The presenter remains responsible for choosing visible shadow casters and
supplying their current orientation and pixels. This query does not create a
shadow map or establish whether unexamined scenery contains an occluder.

Version 1.11 raises the accepted billboard width and height to 128 pixels in
both `trace_billboard` and `trace_directional_shadow`, allowing double-size
affine rasters from overworld version 1.8 to be used directly. Stride,
capacity, geometry and alpha checks are unchanged. The service table layout
is unchanged; request version 1.11 and check
`RHH_WORLD_SCENE_SERVICE_V1_11_SIZE` before relying on the larger limit.

Offer a copied `RhhWorldDynamicLight` with a nonzero per-frame unique ID,
current-map key, x/y in current-map pixels, absolute scene z, radius 1–256,
nonzero `0xRRGGBB` color and intensity 1–1000. A mod can combine an actor
billboard's world anchor and the scene tile's ground height to make a light
follow that actor; connected-map positions must already be converted to the
current map's coordinate space. Offers are frame-local and need to be made
again on each collection. Up to 64 are accepted, sorted by numeric ID;
collisions with another offer or a declared scene-light ID are rejected.
Invalid offers, duplicate IDs or overflow invalidate the entire dynamic batch;
a denied decision also discards it. The selected presenter can read the copied
`frame_lights` only during its draw callback, including the collection status
and the same context. Both context and offer operations fail outside the
collection callback; `frame_lights` fails outside the selected presenter. If
there is no active run, no selected presenter or no light hook, no collection
is performed. The 1.0
world-render prefix remains usable through `RHH_WORLD_RENDER_SERVICE_V1_0_SIZE`.

For example, with `host`, `render`, `world` and `scene` obtained at module
entry, a constraint hook can offer a light at the player billboard:

```c
static void OfferPlayerLight(const struct RhhModEvent *event, const void *state,
                             struct RhhModDecision *decision)
{
    (void)event; (void)decision;
    if (!state || !((const unsigned char *)state)[0]) return;
    struct RhhWorldLightContext context = {.struct_size = sizeof(context)};
    struct RhhWorldActor actor = {.struct_size = sizeof(actor)};
    struct RhhWorldBillboard billboard = {.struct_size = sizeof(billboard)};
    struct RhhSceneWindowTile tile = {.struct_size = sizeof(tile)};
    if (render->light_context(&context) || world->actor_at(0, &actor)
        || (actor.flags & (RHH_ACTOR_HIDDEN | RHH_ACTOR_OFFSCREEN))
        || world->billboard(0, &billboard) || scene->window_resolve(actor.x, actor.y, &tile)) return;
    struct RhhWorldDynamicLight light = {.struct_size = sizeof(light),
        .id = host->content_key("example:light/player"), .map = context.lighting.map,
        .x = billboard.world_anchor_x, .y = billboard.world_anchor_y,
        .z = tile.scene.style.ground_height + 16, .radius = 64,
        .rgb = 0xffbb66, .intensity = 700};
    (void)render->offer_light(&light);
}
```

Object slot zero is only illustrative: a real mod should find the desired actor
from the current snapshot rather than assume the player always occupies it.

A module can append `world_presenter_count` and `world_presenters` to its
definition, and select one by returning its key from an
`RHH_WORLD_PRESENTATION` transform hook. The native OpenGL host invokes the
selected callback on stable overworld frames, with a bound 240×160 framebuffer,
frame number, original RGB555 pixels, scene mask and GL procedure lookup.
The append-only `RHH_WORLD_FRAME_V1_1_SIZE` extension also offers a writable
`output_mask` and `output_mask_valid` flag. A presenter can fill the complete
top-left-origin mask with 128 for visible projected sprites and 255 for
background, then set the flag to 1 when it succeeds. The host validates the
whole output and combines it with the original interface protection before
using it for world shaders. A declined presenter, incomplete or invalid mask,
or an older presenter uses the original 2D mask. The pointers are borrowed for
the callback only; old `RHH_WORLD_FRAME_V1_0_SIZE` frames remain supported.
`RHH_WORLD_FRAME_V1_2_SIZE` adds `output_mask_texture`, an optional OpenGL
texture name. A presenter may publish a complete 240×160 `GL_TEXTURE_2D` in
framebuffer orientation, with red values below 0.75 for sprites and the rest
for background. It must use base level zero and a non-mipmapped minification filter and remain
alive through host composition, until the next draw or shutdown. The host
checks the texture's existence, target, dimensions, base level and filter, normalizes the
classes on the GPU, and preserves the original interface mask. A valid texture
takes precedence over a CPU mask; an invalid one falls back to a valid CPU
mask or the original mask. This avoids a GPU-to-CPU readback each frame.
`RHH_WORLD_FRAME_V1_3_SIZE` adds an optional `photo_target` only while a photo
is pending. Its actor slot and ticket identify the requested subject for this
frame. A presenter may fill both borrowed 240×160 top-left-origin byte masks
with zeros and ones, then set `valid` to 1: `silhouette` is the subject before
occlusion, and `visible` is the subset that wins scene depth and draw order.
The buffers and target are valid only during the callback. Incomplete masks,
visible pixels outside the silhouette and missing outputs are rejected; the
host separately restores original interface occlusion. Shader passes without
an explicit position-preserving declaration make these masks unavailable.
Modules can append `shader_photo_policy_count` and `shader_photo_policies` to
their definition using `RHH_MOD_DEFINITION_SHADER_PHOTO_SIZE`. Each policy
must name a shader declared by that same module and set
`RHH_SHADER_PHOTO_PRESERVES_POSITION`. This is a promise that the shader reads
the scene only at the output pixel: tinting and generated weather are allowed,
but displacement, blur, duplication and sampling neighboring scene pixels are
not. Older definitions and unmarked shaders retain the conservative fallback.
`RHH_SHADER_PHOTO_PRESERVES_GEOMETRY` is a weaker opt-in for a non-displacing
local filter with a positive contribution from the original pixel; it allows
only the approximate pre-filter geometric views. A presenter whose own
processing makes its masks approximate sets `mask_quality` to
`RHH_PHOTO_MASK_GEOMETRIC` when the version 1.2 photo target supplies it.
The module can query the overworld service and implement its own geometry,
billboards, shadows, depth of field and tilt-shift. It must restore GL state
and return zero on success. The host composites protected BG0 pixels and presents
the result; a declined callback, unavailable OpenGL, scripts, menus and palette
fades use the original presentation. Callback code and declaration storage
remain valid until unload, and its shutdown callback runs while the GL context
is still available. The host does not supply a 3D scene, depth buffer,
shadow renderer or blur pipeline; those remain the presenter's responsibility.
An empty selection keeps the original rendering. Legacy definitions ending at
`RHH_MOD_DEFINITION_BASE_SIZE` or `RHH_MOD_DEFINITION_SHADER_SIZE` remain valid.

Canonical names use a namespace, kind and source symbol, for example
`rhh:species/torchic`, `rhh:trainer/roxanne_1`, `rhh:map/route101`,
`rhh:area/route_101`, and `rhh:midi/gen5/trainer`. The generated registry binds
these names to the executable's transient enum IDs. Aliases resolve to their
existing engine object. The first canonical source symbol for an engine ID is
used when exporting its identity. Never store engine IDs or pointers in a mod
save record.

`content_key` computes fixed FNV-1a-64 over the UTF-8 canonical name. Registration
rejects duplicate names and hash collisions. `resolve` checks both key and
content kind. A key stays the same if an enum moves; renaming a canonical source
symbol is a content migration and needs an alias or explicit state migration.
This registry is generated by `native/generate_mod_content.py`.

`content_count`, `content_at` and `content_info` provide read-only metadata,
including canonical alias keys, enabled/creatable/legendary/mythical flags and
raw base stats. Enumeration may contain aliases; compare `key` with
`canonical_key` to avoid giving aliases extra randomizer weight. Views are
copied to caller-owned structs. Returned content names remain host-owned.

`random_at(seed, mod_id, event_key, ordinal)` is a fixed integer hash stream,
independent of RHH's RNG, registration order, addresses and wall time. Callers
supply ordinals explicitly. Pure queries must not advance a stream or modify
state. Built-in area pools and trainer slots use stateless draws; full wild
randomization stores its encounter ordinal. Each randomized run saves its
candidate roster as content keys, so adding species to a later executable does
not reshuffle an existing run. Removing required candidates fails state
validation instead of silently replacing its encounter tables.

## Battle environment shaders

Weather and terrain use a persistent OpenGL 2.1 / GLSL 1.20 pass in the native
presenter. Rain, heavy rain, sunlight, harsh sunlight, sandstorm, hail, snow,
fog and strong winds combine with grassy, misty, electric or psychic terrain.
The adapter reads the current battle state every frame; replacement, removal,
expiry and Cloud Nine / Air Lock suppression update the next rendered frame.
Terrain remains visible when weather is suppressed. Menus and the overworld
use the original scene. Text, health bars and other battle UI are masked out,
and the effects follow battle fades.
Battle artwork combines floor and distant scenery in one background image;
terrain therefore uses a feathered screen-space horizon across the visible
field rather than an asset-specific ground mask.

`rhh:battle_environment/enabled` is a runtime boolean setting, persisted with
the other module settings and exposed as **BATTLE FX** in Options. It defaults
off, including on older saves that have no record for this module. While the
shader is usable,
the recurring weather message and animation are skipped. Weather start/end
messages, damage, healing and weather-dependent ability processing remain.
If OpenGL or a shader fails, the original weather recap is retained.

Modules may append `shader_count` and `shaders` to `RhhModDefinition`, returning
stable `RhhModShader` declarations. Definitions ending at
`RHH_MOD_DEFINITION_BASE_SIZE` remain supported. Return the shader's content
key from a transform hook for `RHH_BATTLE_ENVIRONMENT`. Its arguments are
weather, terrain, suppression, and battle-scene visibility. The host supplies
`uScene`, `uMask`, `uResolution`, `uTime`, `uWeather`, `uTerrain`, `uSuppressed`,
`uFade`, and `uDoubles`; semantic values are in `mod_api.h`. UV origin is the
top left. The mask is zero for interface pixels, 128/255 for sprite pixels,
and one for background pixels; terrain uses background pixels while weather
can also cover sprites. The notice event
`RHH_BATTLE_ENVIRONMENT_NOTICE` reports `RHH_NOTICE_WEATHER_RECAP` and whether
the selected shader is usable, allowing the module to suppress recaps safely.

`rhh:service/battle_render` version 1.0 lets independent modules offer up to
16 declared shaders during the `RHH_BATTLE_RENDER_PASSES` constraint callback.
The event supplies the current weather, terrain, suppression and doubles flag;
its ordinal is the visual frame. Offers use unique pass keys and signed
priorities, sorted by priority and then key, and execute after the selected
battle-environment shader. A duplicate key, overflow or denied collection
discards the chain for that frame; a shader that fails to compile is skipped.
Invalid offers leave the other offers intact. Collection requires an active
run and the visible battle scene, and also runs without weather or terrain.
Both gameplay RNG streams are restored and an RNG-consuming collection is
discarded. Battle passes receive the usual shader uniforms and mask;
`uDepthAvailable` is false because battle has no presenter depth texture.
When available, `uBattlerMask` is a nearest-filtered, top-left 240×160 texture
of the original battle frame. `uBattlerMaskAvailable` indicates whether that
frame has an OAM-to-sprite mapping. Round its red channel times 255: zero is
unassigned, and values 1–4 identify visible battler slots 0–3. The renderer
attributes topmost visible sprite pixels only when the OAM entry still matches
the mapped engine sprite; UI, occluded, ambiguous and unrecognized pixels are
zero. The mask follows sprite priority, windows and affine rasterization,
but is not an individual Pokémon ID, an anatomical foot position or a ground
classification. It remains in the original frame's coordinates through the
chain, so shaders that move pixels must account for that. It is unavailable to
world shaders and can be unavailable during battle setup or transitions.
The host restores protected UI pixels from the original scene after the
environment shader and pass chain, then composites the battle-condition HUD.
The chain is presentation only and does not replace weather recap policy.
Software rendering uses the original scene.

`rhh:service/battle_backdrop` version 1.0 supplies stable keys for the
engine's battle-background catalog. `count` and `at` enumerate copied IDs and
report whether their baseline artwork exists; use keys rather than catalog
indices for saved rules. `current` returns the baseline selected by the
engine after its tiles, map and palette have all loaded, including the
trainer and map overrides. It is unavailable outside the active battle scene,
while a staged reload is incomplete, and when no complete baseline is known.
For example, a pass can query this service during `RHH_BATTLE_RENDER_PASSES`
and choose a layout for `rhh:battle_backdrop/grass` or
`rhh:battle_backdrop/frontier`. Queries copy their outputs and do not change
RNG. This identifies baseline assets, not a ground mask: battle animations,
terrain backgrounds and other VRAM writes can alter or cover the scene after
the load. A module must supply its own scene-specific ground classification.

Version 1.1 adds `image_rgba`, which copies a selected rectangle of the
compiled baseline artwork as top-left RGBA8888. Its 512×256 coordinate space
matches the engine's two BG3 tilemap screens, before scrolling, fades,
sprites, windows or animation; palette index zero is transparent. It can be
called without a battle in progress. Missing assets, unsupported source data,
invalid bounds and insufficient capacity leave the destination unchanged.
Decode once and cache the result rather than calling it every render frame.
The pixels let a mod author classify artwork; they do not label ground or
guarantee the current frame still shows that artwork.

Edit `native/mods/battle_environment.frag`; the native build regenerates the
embedded header with `native/generate_shader.py`. No runtime shader asset or
additional GL DLL is required. Screenshots capture the shader output at the
game's 240×160 resolution. The original GBA build retains its weather display.

Build `gameplay-tests` with `Makefile.native`, then run
`build/native/linux64/gameplay-tests environment` with a working OpenGL
display to check actual shader output, animation, UI masking and battle-state
transitions. Run the same command with `SDL_VIDEODRIVER=dummy` to check the
fallback. The subsystem suite also covers shader declarations and short legacy
ABI definitions.

For visual comparisons, capture a battle with the game's
`--scene-capture path/to/source.scene` option. Build `environment-gallery`
with `Makefile.native`, run that executable with the scene path and an existing
output directory, then run `native/tests/environment_gallery.py` on that
directory to generate animated GIFs and a local HTML gallery. The capture
runner defaults to 80 frames at 20 fps (four seconds); pass `--frames N` after
the output directory to change its length. Wait until the send-out animation
and screen fades finish before capturing the source scene. The gallery uses
the original battle background, Pokémon and protected UI, with weather and
terrain selected explicitly for comparison. Its shared GIF palettes preserve
the original protected UI colors on every frame. PNG exports of the first
frame remain available alongside the animations.

## Composition and ownership

The engine owns Pokémon, maps, battle state, tasks, rendering and input.
`RhhModEvent` and `RhhModDecision` are borrowed for one callback only. Module
definitions and callback code are owned by the loaded library. Each module's
private save state is allocated, zeroed, staged and freed by the host; modules
must use an explicit portable byte layout and never retain engine pointers.
Reducers receive read-only state. Observe callbacks may modify their own state;
the overworld update callback may also invoke the documented host commands.
Mutable observers are accepted only for committed notifications, never
for stat/species/permission queries. Neither callback may retain borrowed
pointers or assume a query call count. Zero bytes must be a valid initial state.

Hooks execute by stage (transform, constraint, finalize, observe), then dependency depth,
numeric priority (lower first), stable module ID, and declaration index.
Dependencies therefore order hooks within a stage; stage order takes precedence
across stages. Library discovery and registration order have no effect.
Constraints accumulate denials with logical AND, retaining the first denial's
reason. Level caps accumulate by minimum. A later module cannot permit a catch
or revival that an earlier constraint denied. Species outputs are resolved and
validated before the engine creates a Pokémon. The engine also enforces valid
stat ranges. The 600-BST example owns its proportional normalization and runs in
the finalization stage. Death queries accumulate with OR, so a later mod cannot
erase another mod's permanent-death decision. Committed notifications carry
the event actually applied by the engine.

Each definition exposes typed setting descriptors, a reader, and a writer.
`RhhModsConfigure` stages and validates changes before replacing live state.
New Game settings lock at run start/load; runtime settings, such as music,
remain editable. Modules can read public settings through the host service,
without reading another module's private buffer.

`gModDraft` belongs to the New Game screen, separately from the active run.
Every visit starts with all optional rules off and original music/starters.
Canceling that screen leaves the active save's rules and state intact.
Confirming starts a new run when the original intro calls `NewGameInitData`.
Run rules are fixed for that adventure. Battle music can be changed in Options.

## Persistence and migrations

The PC-native `RHHPCS01` save stores full game blocks, special records, mod
snapshots and host archives as length-delimited data. It rejects old native
saves and incompatible engine layouts. Normal and link saves stage game and
mod state together, flush a temporary file, and atomically replace the save.
Two game generations are retained; if one game payload is damaged, the other
can be loaded with its matching mod snapshot. The shared section is separately
checksummed, and its corruption currently rejects the save. Same-generation link
writes preserve the prior full generation.

The host stages every module before changing any live state. If schema/length
differs, its migration callback initializes a new byte buffer; validation runs
before acceptance. A failed migration, unsupported ABI, corrupt record, missing
required module or unavailable saved starter/candidate rejects the save without
overwriting it. Newly installed modules without a saved record receive zeroed
state. Optional unknown records are discarded on the next save: use that flag
only for disposable caches, never for rules or important user data.

The wild example's schema 2 migrates schema 1 by adding a frozen candidate
roster, retaining its draw ordinal. The Nuzlocke example's schema 2 expands its
older area/death ledgers using the same stable identities. Both migration
callbacks are in the example sources. The `rhh:host_run` record is engine-owned
and stores seed, run identity, the committed wild-creation counter and the
world-step clock. Schema 1 records migrate to schema 2 with zero world steps.

Challenge ledgers use portable content keys and owned Pokémon identities
(OT ID plus personality); evolution cannot clear a death. New Game drafts and
UI selections are host session data. The Nuzlocke example resets its temporary
capture permission when a run loads.

Modules with `RHH_MOD_DURABLE` supply a merge callback. The host journals those
modules' versioned records when committed notifications change their state.
Reload merges them transactionally before healing or Continue. The Nuzlocke
example merges area/death sets and armed/failed facts, preserving configuration.
A persistence failure emits a notification; this example locks the run rather
than allowing a reload to undo a death. Two run journals are retained, alongside
a persisted run serial, so repeated seeds and sector recovery cannot merge
different adventures. Starting an unsaved adventure cannot journal over the
older saved run. This protects ordinary reloads, not deliberate replacement of
the whole save with an external backup.

Development saves from the earlier combined `rhh:rules` format are rejected
as requiring an unavailable module, without overwriting them. They were not a
released schema. Public native container v1 saves remain supported with all
example settings off. Ordinary RHH game data still has its native engine ABI;
portable mod schemas do not imply automatic migration of the underlying game
between upstream engine revisions.

## Hook contexts

Keys are content keys unless the table specifies an owned-mon identity. Other
`args` words are zero/reserved. These contracts keep example code independent
of engine enums and writable game structs.

| Event | Key / subject / ordinal | Arguments and decision |
| --- | --- | --- |
| Wild species | Map / original species / creation counter | `args[0]`: kind plus time table in upper 16 bits; `args[1]`: slot; output species key |
| Trainer species | Trainer / original species / party position | Output species key |
| Starter species | Birch scene / original species / ball position | Output species key; middle ball is position 1 |
| Base stats | Species / species / 0 | `args[0]`: preserve HP 1; six raw input stats, six output stats |
| Level cap | Next Gym/Champion trainer / 0 / 0 | `args[0]`: actual ace level; output upper bound |
| Follower enabled | 0 / 0 / 0 | Output boolean request; map restrictions stay in the engine |
| Can capture | Named area / wild species / 0 | Accumulated permission and reason |
| Mon is dead / can revive | 0 / owned-mon identity / 0 | Death boolean, or permission; revive args contain dead status, old/new/max HP |
| Can continue | 0 / 0 / 0 | Accumulated permission and reason |
| Encounter started | Named area / 0 / 0 | `args[0]`: catch eligible; `args[1]`: player has a ball |
| Wild created | Map / final species / creation counter | Same kind/table/slot as species query; advances full-mode example state |
| Wild generation permission / cancelled | Map / resolved species / creation counter | `args`: area, source, slot, level; copied candidate and cancellation reason available in their callbacks |
| Special wild permission / generated / cancelled | Map / fixed species / current creation counter | `args`: area, special source, roamer index or sentinel, level; copied context and cancellation reason available in their callbacks |
| Wild actor spawned / removed / engaged | Map / species / actor handle | `args[0]`: object slot; `args[1]`: cause; copied context available during the observer |
| Wild actor identified | Map / species / actor handle | `args[0]`: object slot; `args[1]`: `RHH_WILD_ACTOR_ID_ASSIGNED`; individual ID available during the observer |
| Wild actor battle ended | Map / original actor species / actor handle | `args[0]`: semantic battle outcome; `args[1]`: battle flags; copied result available during the observer |
| Wild concealment proposal / changed | Map / species / actor handle | `args`: cause, resulting mode, reveal flags, reveal radius; copied result available during the hook |
| Mon fainted | Species / owned-mon identity / 0 | `args[0]`: player has a ball |
| Inventory changed / whiteout | Inventory marker or 0 / 0 / 0 | `args[0]`: player has a ball |
| Mon caught | Named area / 0 / 0 | Consumes current capture permission |
| Run started / loaded | Run identity / 0 / 0 | Lifecycle notification; settings then lock |
| Persistence failed | 0 / 0 / 0 | Durable storage failed; avoid recursive writes |
| Battle music | Original song / 0 / 0 | `args[0]`: wild/trainer/Gym/Champion role 0–3; output MIDI key, zero keeps original |
| Breeding compatibility | Parent 0 species / parent 1 species / 0 | `args[0]`: unmodified category; output category or deny |
| Breeding offspring | Original species / parent 0 species / 0 | `args[0]`: effective compatibility; `args[1..2]`: inheritance slots; output species key |
| Can accept daycare egg | Parent 0 species / parent 1 species / offer token | `args[0]`: effective compatibility; `args[1]`: party count; constraints may deny |
| Breeding egg created | Actual species / egg identity / 0 | Same compatibility and inheritance slots; committed observer notification |
| Breeding IV inheritance | Offspring species / egg identity / stat ordinal | `args[0]`: initial IV; `args[1]`: engine IV; `args[2]`: compatibility; output source |
| Breeding IV rule | Parent 0 species / parent 1 species / stat ordinal | Pure per-stat source selection from copied parent context |
| Breeding move inheritance | Offspring species / egg identity / slot ordinal | `args[0]`: slot; `args[1]`: compatibility; output move key or zero |
| Breeding move rule | Offspring species / original species / slot ordinal | Pure move selection from copied parent and baseline-moveset context |
| Egg hatched | Species / pre-hatch identity / post-hatch identity | `args[0]`: party slot; committed observer notification, map in the hatch context |
| World step | Map / 0 / run-scoped step number | Committed observer notification; position and metatile data in the step context |

## Built-in rule semantics

* Nuzlocke begins when the player has a Poké Ball. The first eligible encounter
  consumes that named area's attempt at battle start, including fleeing and
  failed catches. Floors sharing a region-map area share the attempt. No shiny
  or duplicate-species exception is enabled. Gifts and the chosen starter are
  not wild encounters. Fainting is permanent; healing, revival and PC conversion
  cannot restore that identity. A wipeout locks the run and shows its end screen.
* Pool randomization gives every map, encounter kind, time table and slot a
  stable species. Full randomization draws separately for each generated normal
  wild encounter, including fishing and the Feebas slot. Scripted story species
  and gifts keep their original identity. Original encounter levels and slot
  probabilities remain in effect.
* Trainer randomization keeps trainer identity, party size, levels, AI and
  items. Its species draw uses trainer identity and party position. A changed
  species receives its own initial moves and valid ability/gender handling.
* The starter picker includes every enabled, creatable, non-legendary,
  non-mythical species and permanent form. Temporary battle forms are excluded.
  The selected Pokémon replaces the middle ball in Birch's original scene.
* Equalization uses bounded proportional allocation and largest remainders to
  total exactly 600. Stats remain 1–255; Shedinja's HP remains 1.
* The party cap is the actual highest level in the next undefeated Gym Leader's
  configured party (including difficulty), then the Champion's ace after eight
  badges. EXP, candies and entering the party cannot exceed the cap. Enemy
  levels are unaffected.
* Followers use the existing RHH overworld Pokémon implementation and its
  indoor-size, NPC-follower and map restrictions.

## Execution traces

Run with `--mod-trace run.jsonl`. JSON Lines records contain hook order,
dependency depth, stable mod ID/version, event kind, run seed, event/subject
keys, explicit ordinal, scalar arguments, input/output value and stat vectors, accumulated permission/reason, and
before/after private-state hashes. Save loads and migrations identify their
schema versions. No address or wall clock participates in a mod RNG key.
Repeated unchanged query decisions are suppressed; committed notifications and
state-changing observers remain visible. Tracing does not consume RHH RNG or modify game state. The JSON is intended for
reproducible bug reports; hashes are diagnostic, not cryptographic signatures.

The implementation lives in `native/mod_host.c`, `native/mod_loader.c`,
`native/mod_game.c` and the small `src/` integration points. The public ABI has
no SDL or RHH header dependency. Host-unit tests exercise composition, sticky
constraints, dependency failures, content rebinding and transactional migration.
Native save tests exercise fresh-process reload, v1 migration, generation
recovery, missing-module rejection and failed commit rollback. Full-game tests
in `native/tests/mod_game_test.inc` use real Pokémon, items and RHH save code,
and run against both built-in and dynamically loaded copies of the examples.

## Music assets

The built-in Gen I/II styles play original Red/Gold GBS music using the Game Boy
sound engine; Gen III plays Emerald's own sequences and voicegroups. Gen IV/V
use converted Diamond and Black/White sequences with their matching instrument
banks. Gen V's Champion role uses Alder's theme. These styles preserve the
original loop points without a fade at each loop; music changes, engine fades,
mono/stereo settings and original SFX/cries remain supported. Original battle
audio is the default. Source URLs, hashes and credits are in the manifest and
`native/THIRD_PARTY.md`.

Trusted modules can declare `RhhModMusic` MIDI/SF2 pairs with
`RHH_MOD_ABI_MUSIC` and choose a declared key from `RHH_MUSIC_SELECT`. The
event's `event_key` is the original music key when known, `subject` is the
current map key, and `args[0]` is a `RhhMusicContext`; the decision selects a
replacement or zero for the original. Declarations use slash-separated paths
relative to the host music directory; absolute paths, empty components, `.` and
`..` are rejected. `rhh:service/music` version 1.0 enumerates declarations,
looks up keys, checks availability and reports the selected key. Availability
parses the assets: MIDI formats 0/1 are bounded to 4 MiB and SF2 files to
128 MiB. Missing or invalid assets fall back to the original BGM.
Version 1.4 adds `inspect_track` for a copied availability result covering
bundled Game Boy, MP2K and MIDI styles, module declarations, and imported
pairs. It reports the track kind, playback backend and bundled style ID where
known. Unknown or removed keys report an unknown kind and missing status.
The query opens and checks the current files using separate decoder instances;
it does not alter active playback or advance either gameplay RNG stream. The
result is a point-in-time check, not a guarantee that later playback will
succeed: files can change, resources can fail, and active playback may have a
cached SoundFont. The older `availability` method continues to describe only
declared or imported MIDI pairs. Both methods can perform expensive file I/O.
Version 1.5 adds `inspect_source_timing`, which performs the same type of
point-in-time validation and reports a MIDI file's last parsed event and its
validated embedded loop markers, or the host's default source loop when no
valid markers are present. The last event is not a measurement of audible
duration. This query does not dispatch loop proposals; those may change the
loop later for playback or preview. Game Boy and MP2K results identify their
engine-managed loops without claiming MIDI timing. Missing and invalid assets
have no timing flags. The query uses separate decoders and does not alter
playback or advance gameplay RNG; like availability inspection, it can perform
substantial file I/O and is not a reservation.
Version 1.6 adds `queue_battle_cue` for committed `RHH_BATTLE_STARTED`,
`RHH_BATTLER_ENTERED`, `RHH_BATTLE_TURN_ENDED`, `RHH_MOVE_RESOLVED`,
`RHH_INNATE_ENTRY`, and `RHH_INNATE_TURN_END` observers. A registered module
can request a known bundled, declared or imported track, or zero for the
original song. Only one request may wait for the next native frame: the first
valid request wins and later requests receive `RHH_MUSIC_CUE_BUSY`. The host
checks the active local battle, run, current original song and playback
sequence again before starting. A changed song, ended battle, paused or fading
BGM, modal panel, save transaction or disabled music makes a queued request
stale. A run change, module unload or audio shutdown discards it without a
callback.

At delivery the host restarts the current original BGM through the existing
selection pipeline, placing the cue after saved assignments and before
`RHH_MUSIC_SELECT`. Music selection constraints can still deny or replace it;
missing assets retain the engine's normal original-song fallback. The deferred
`RHH_MUSIC_BATTLE_CUE_FINISHED` observer and `battle_cue_result` report
played, overridden, fallback or stale, with the actual track and lifecycle
sequences where available. The event is presentation-only and preserves both
gameplay RNG streams. Cues do not change stored assignments or add a new
battle mechanic.
Version 1.7 adds `inspect_instruments`, which opens a separate MIDI/SoundFont
pair and walks the source notes once in event order. It reports each used
channel's requested bank and program, the selected SoundFont preset, its
copied name, first-note time and note count. The selection uses the same bank,
drum and nearest-program fallback behavior as playback; a fallback flag marks
any selected bank or program that differs from the request. A bank-select
message takes effect on the next program change, just as in playback.

The list retains the first 128 distinct combinations. Total and fallback
note counts include omitted combinations, while `unlisted_notes` counts
their notes and the truncated flag signals an incomplete list. Game Boy and
MP2K tracks have no MIDI instrument list. Missing and invalid files report
their availability without instrument data. This is a point-in-time source
inspection, not proof that a preset contains audible samples or that a later
playback will use unchanged files. It does not play audio, alter the active
track, dispatch loop proposals or advance gameplay RNG. As with the other
inspection functions, parsing can be expensive and must not run in rendering
or audio callbacks. Request version 1.7 and check
`RHH_MUSIC_SERVICE_V1_7_SIZE`.
Version 1.8 adds `inspect_note_coverage`. It follows the same source-note and
preset selection as `inspect_instruments`, then checks whether the selected
SoundFont preset has a region matching each note's key and velocity. The
result counts covered and uncovered notes and lists up to 64 distinct
uncovered channel/key/velocity and preset combinations, with first-note time
and repetition count. `unlisted_uncovered_notes` and the truncated flag
identify notes outside the list. Game Boy and MP2K tracks do not have MIDI
coverage data; missing or invalid files retain their availability status.

Region coverage does not prove a sound will be audible: sample contents,
envelopes, mixing, voice limits and later file changes are not simulated.
The query uses separate decoders, does not change playback or gameplay RNG,
and can perform substantial file I/O. Request version 1.8 and check
`RHH_MUSIC_SERVICE_V1_8_SIZE`.
`RHH_BATTLE_MUSIC` and `RHH_MUSIC_SELECT` are presentation-only: both gameplay
RNG streams are restored after each event. If any hook consumes gameplay RNG,
the host discards that event's selection changes while preserving a denial.
The remaining selection stages, including saved assignments, still apply.
Deferred playback lifecycle notifications also restore both RNG streams.

Version 1.1 adds `RHH_MUSIC_GAIN` for each successfully loaded replacement
MIDI/SF2 or Game Boy track. Reducers can inspect `gain_context` and compose a
gain in thousandths of the host's existing track normalization, from 0 to
4000. Constraints may deny but cannot rewrite it. Denial, out-of-range values
or gameplay RNG use restore the default 1000; both gameplay RNG streams are
restored. The result is multiplied by the saved per-track level, capped at
4000, and then multiplied by the game's live volume and fade. The original
MP2K player and modal previews do not receive this proposal. `playback` returns the active replacement's keys,
context, gain, backend, sample position, loop points and pause/transition
flags. Its gain is the target, and `RHH_MUSIC_PLAYBACK_LEVEL_RAMP` identifies
an adjustment still in progress. Sample position is mixer time, not the audio
device's playback clock.
Game Boy loops are handled by its music engine and have no exposed offsets.

Version 1.2 adds `RHH_MUSIC_PLAYBACK_CHANGED` and the `lifecycle` and
`lifecycle_context` queries. Start and stop notifications report the committed
track, backend, context, original song, map and trainer keys. When a requested
replacement is missing or invalid, the start identifies the original MP2K
fallback and retains the requested key. Native MP2K redirects are distinguished
from ordinary originals. Delivery is deferred to a native frame, including while
a modal menu is open; the event is a copied observation, and its context is
available only during that event. A 64-event queue keeps ordering, while
process-local sequence gaps and the cumulative dropped count expose overflow.
`lifecycle` reports the current committed BGM even before queued notifications
are delivered. Preview tracks are separate and do not emit these events.

Version 1.3 adds the pure `RHH_MUSIC_LOOP` MIDI proposal. `loop_context`
reports the track, phase, original marker or automatic bounds and final MIDI
event time. A transform packs start and end milliseconds with
`RHH_MUSIC_LOOP_VALUE`; constraints can deny but cannot replace the proposed
bounds. Invalid intervals, denial or gameplay RNG use preserve the source
loop, with both gameplay RNG streams restored. The end must be at most five
seconds beyond the final MIDI event. Playback and modal previews both run the
proposal; preview supplies its owner and track, while battle and map context
are available only for gameplay playback. Game Boy and MP2K loops remain under
their respective engines. Neither path adds a fade at a loop boundary.
`rhh:service/music_preview` version 1.2 adds `timing`, exposing the current
preview's sample position and resolved loop boundaries without seeking.

`rhh:service/music_imports` version 1.0 provides a host-managed library of up
to 32 MIDI/SF2 pairs. `import_pair` takes a registered requester, UTF-8 source
paths and a printable ASCII label of at most 63 characters. It synchronously
validates the pair with the playback parser, copies it into the game's user
preferences directory and returns a stable pair key, SHA-256 hashes, sizes and
loop marker times. Source paths are not stored. Duplicate imports keep the
existing label and repair any missing or damaged copied files. A MIDI or
SoundFont shared by multiple entries is stored once. The library limits unique
asset data to 256 MiB, with individual MIDI and SF2 limits of 4 MiB and
128 MiB. Content keys are collision-checked; imports are rejected on a conflict.

`status` distinguishes an available library, a missing library path and an
unreadable or invalid manifest. `count`, `at` and `find` return copied library
metadata; `rhh:service/music`'s `availability` accepts imported keys, though
its declaration enumeration continues to list module-declared tracks only.
Playback verifies copied-file hashes and falls back if a file is missing or
invalid. `remove` updates the manifest and removes assets no longer referenced
by any imported pair; already-saved assignments remain and fall back until
their track is re-imported or changed. The library is global to the local user,
not embedded in a game save; copy it or re-import files when moving to another
machine.

Version 1.1 exposes a path-independent transfer interface. `copy_pair` takes
the full metadata returned by `at` or `find` and copies both assets only after
checking their sizes and SHA-256 hashes; stale metadata, missing or damaged
files, overlapping buffers and undersized buffers fail without writing either
buffer. `import_memory` takes that metadata and both byte arrays, verifies the
hashes and playback formats, and commits through the same locked, deduplicated
library transaction as `import_pair`. The same bytes produce the same track
key, so a separately transferred save can resolve its existing assignments.
Duplicate entries retain the target library's existing label. These calls are
synchronous and can allocate memory proportional to both assets; callers own
the transfer and should not invoke them from drawing or audio callbacks.
Request version 1.1 and check `RHH_MUSIC_IMPORT_SERVICE_V1_1_SIZE`.

Library mutations use a nonblocking cross-process lock and atomic file
replacement. A competing writer is rejected, and a writer reloads the latest
manifest before changing it; other running instances see catalog changes when
they next mutate the library or restart. A corrupt manifest is preserved and
reported rather than overwritten. `RHH_MUSIC_IMPORTED` and
`RHH_MUSIC_IMPORT_REMOVED` are committed notifications; `context` copies the
receipt during them. Re-import of an existing pair does not send another import
notification. Import and removal are synchronous, can perform substantial I/O,
and are unavailable in battle, during saves or from drawing callbacks. The
requester is a coordination label for trusted modules.
`python3 native/tests/music_imports.py` exercises validation, cross-process
locking, save/restart playback, corruption, repair, deduplication and deletion,
then removes its test build and temporary library.

`rhh:service/music_preview` version 1.0 plays an imported or module-declared
MIDI/SF2 pair while its owner has an open modal panel. `start` takes a duration
from 500 to 30,000 ms and a volume percentage from 0 to 100; `current` reports
the active owner, track and elapsed time, and `stop` is restricted to that
owner. Failed loads do not interrupt the current music. The preview uses its
own synthesizer and pauses the background player while leaving sound effects
and cries running. It ends at its duration, when stopped, when the panel closes
or resets, or when the game starts or stops background music. A native MIDI or
Game Boy replacement resumes from its saved sample position; the original
MP2K player resumes its sequence but sustained notes are interrupted. A game
music change takes precedence and cancels the preview. Mono output is respected.
`rhh:service/music.current` continues to describe the background selection;
use the preview service to inspect the audition. Preview is unavailable during
battle, a save, drawing, or without the owner's modal panel. It does not change
assignments or saves.
Version 1.1 adds `start_scaled`, which accepts a gain in thousandths from 0
to 4000. The caller chooses that preview gain explicitly; it does not invoke
the replacement-track gain proposal or automatically read saved levels.

`rhh:service/file_dialog` version 1.0 opens an asynchronous, panel-owned MIDI
or SoundFont chooser. `begin` starts a request; `poll` copies its current
state, `take` consumes a completed result, and `cancel` asks the chooser to
close. A selected path is copied as UTF-8; cancelled and failed requests have
no path. Closing or resetting the owning panel cancels and discards its
request. Windows uses the system file dialog; Linux uses the desktop's
[XDG file-chooser portal](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileChooser.html)
and D-Bus. The Linux build enables that backend when
`pkg-config` finds D-Bus development headers; at runtime the D-Bus library,
session bus and desktop portal must be available. Unsupported or unavailable
backends return an error rather than blocking the game. File filters are a
convenience; the importer still validates the selected contents.

`rhh:service/text_entry` version 1.0 provides panel-owned keyboard entry for
printable ASCII names of up to 63 characters. Enter accepts, Escape cancels
and Backspace deletes; `poll` returns the draft and `take` consumes a finished
result. Closing the panel cancels entry, and held controls are suppressed
until released afterward. The game's existing Pokémon naming input is
unchanged.
Both runners remove their test builds and temporary files.

`rhh:service/music_assignments` version 1.0 stores up to 64 shared selection
rules with the current run. A rule names a registered MIDI/SF2 pair or a
built-in battle style, and can match a music context, original song, map and
authored trainer; zero keys and `RHH_MUSIC_ANY` are wildcards. More specific
rules win in this order: trainer, original song, map, then context. When both
opponents match equally specific trainer rules, the first opponent wins. The
host uses trainer identities only for ordinary local trainer battles with a
recognized trainer, Gym or Champion battle track, not facility, link,
recorded, debug or tutorial opponents. `resolve` previews the
same rule ordering from a copied `RhhMusicSituation`.

`preview` and `apply` require a registered requester and the current revision.
They reject unknown keys, unsafe command contexts, full capacity and stale
revisions. `RHH_CAN_SET_MUSIC_ASSIGNMENT` constraints can deny changes;
`RHH_MUSIC_ASSIGNMENT_CHANGED` observes a committed in-memory change, and
`context` copies its receipt only within those callbacks. The configuration
constraints restore both gameplay RNG streams; an allowed preview or change
that consumes RNG fails as stale, while a denial remains denied. Committed
assignment and level notifications restore both streams as well. The configuration
is saved with the next normal game save and follows save rollback. The requester
is a coordination label, not authorization; trusted modules share these
rules. A track that later disappears remains in the saved rule, and playback
falls back to the original song. Module-provided `RHH_BATTLE_MUSIC` choices
are evaluated first, then a matching saved assignment, then
`RHH_MUSIC_SELECT` transforms can override the assignment.

`rhh:service/music_levels` version 1.0 stores up to 64 per-track levels with
the current run. A level is 0..4000 in thousandths, with 1000 as the implicit
default; setting 1000 removes an explicit record. `preview` and `apply` use a
registered requester and a checked revision. Constraints may deny an edit,
and a committed change emits `RHH_MUSIC_LEVEL_CHANGED`; `context` supplies
the copied transaction during the constraint and notification. Both gameplay
RNG streams are restored after constraints, and an allowed edit that consumed
RNG is rejected. A track key remains in the saved state if its asset is
removed; it can still be reset to the default. Changes follow normal saves
and rollback. A changed level is applied to a matching active replacement
over approximately 10 ms; it does not restart the track or alter its loop.
The level multiplies the `RHH_MUSIC_GAIN` proposal, with the result capped at
4000; non-default levels cannot be set for the MP2K-backed style. Older saves without level records load
with default levels.

### Battle-scoped custom Tera types

The `rhh:service/types` v1.9 service exposes `tera_type_context` during
`RHH_BATTLE_TERA_TYPE`. A transform can replace the stored Tera type key with
a registered custom type key for the current battle. The event exposes the
current battler slot and battle species, not a persistent Pokémon identity;
callbacks must be pure and return a stable result throughout the battle,
including AI previews. Denied or invalid proposals use the stored type.
Battler type-list queries are unavailable recursively in this callback.
The result drives battle typing, Tera Blast and tint color, and custom types
use a generic Tera indicator. This hook does not change the
Pokémon's saved Tera type or provide persistent individual custom Tera
assignments or custom Tera form changes. Link and recorded battles retain the
original type.

Version 1.10 adds `tera_owned_context`, available only during that query. It
returns a copied party snapshot only when the queried Pokémon is the actual
player battler and its OT/personality identity is uniquely present across
party, boxes, daycare and fusion storage. It rejects transformed battlers,
party/source mismatches, corrupt or ambiguous matches, unavailable storage,
link, recorded, facility, tutorial, debug and AI-only battles. Ordinary
battle form changes may differ from the party species. A smaller or unavailable
query leaves the output unchanged. This identity can collide or later be
reused; it is not a permanent host-assigned individual ID.

A mod can store a type's stable key with its chosen identity in its versioned
mod state and propose that key during the battle query. The host falls back to
the Pokémon's stored Tera type if the key is no longer registered, leaving the
mod's state intact.
This is a mod-owned association, not a general host-managed assignment store;
mods remain responsible for migration and identity-reuse policy.

### PP cost rules

`rhh:service/moves` v1.23 adds the pure `RHH_MOVE_PP_COST` transform and
`pp_cost_context`. The engine asks after calculating its ordinary PP cost and
Pressure surcharges, immediately before deducting from the selected move slot.
The context copies the executing move, charged move, battler, selected target,
slot, current PP, engine cost and battle ID. Flags distinguish permanent and
copied moves, submoves and spread or field targeting. Transforms can propose
0–255 PP; a denied or invalid proposal falls back to the engine cost, and the
engine clamps the deduction to the PP available. Constraints may deny a
proposal but cannot rewrite it. The normal controller update still commits
permanent slot changes.

This rule does not make a zero-PP move selectable. It is skipped when the
engine skips deduction, such as for Struggle, repeat turns, reflected moves or
Dancer. Link and recorded battles use the original cost. Callbacks must be pure
and must not consume game RNG.

Version 1.44 adds the observer-only `RHH_MOVE_PP_DEDUCTED` event and
`pp_deduction_context`. It reports the actual battle-slot update, including
the pre-deduction context, engine base cost, resolved cost, amount paid and
remaining PP. A permitted zero-cost deduction still emits an event with zero
paid. The flag for a permanent move indicates that the controller update was
queued; the callback runs before that update is applied to party data. Copied
moves do not queue it. The event follows the engine's deduction path, including
Pressure, clamping and called-move slot selection; it does not report other PP
changes, later interruption or a durable save. It is skipped for AI previews,
link and recorded battles, and paths where the engine skips deduction.
Observer gameplay RNG is restored. The context is available only during the
callback; invalid requests leave outputs unchanged. Request version 1.44 and
check `RHH_MOVE_SERVICE_V1_44_SIZE`.

Version 1.24 adds `ai_pp_cost_estimate` during `RHH_AI_MOVE_SCORE` and
`RHH_AI_EFFECT_PREVIEW`. It estimates the cost if the candidate move executes
as itself, using the AI's cached abilities rather than hidden battle state. The
result reports the assumed base, adjusted cost, amount paid and remaining PP.
Flags identify unknown abilities, an assumed target, moves that can call a
different move, a known multi-turn continuation that skips deduction, and the
presence of committed PP rules that may differ from the preview. Interruptions
and future changes are not simulated. Invalid, out-of-scope and recursive calls leave the output unchanged; no automatic score
adjustment is applied.

Mods opt into preview transforms separately with `RHH_AI_MOVE_PP_COST`, which
uses the same copied context and proposal rules, with the AI flag set. Existing
v1.23 callbacks remain committed-only. A rule that handles both phases should
produce consistent results for equivalent known inputs.

### Move selection constraints

`rhh:service/moves` v1.25 adds `RHH_CAN_SELECT_MOVE`, a pure constraint on
choosing a move slot. Its copied `selection_context` identifies the battler,
slot, move and species keys, current PP, level, HP, turn and battle ID. The
host evaluates it for player selection, AI-controlled battler legality and the
engine's all-moves-unusable check. AI prediction of a human-controlled
battler does not apply the mod rule to that battler's hidden move slots. Rules
must be deterministic for the same battle state and must not mutate state or consume game RNG; the engine may ask more
than once. Denial is sticky across constraints. It applies to the chosen base
move even when a Z-Move or Dynamax is selected; Struggle remains available
as the engine's fallback. Called moves such as Sleep Talk's choice are not
blocked by this selection rule. Link and recorded battles bypass it.

`selection` previews only the mod constraint for a player-owned battler and
returns a copied result with any denial reason. It does not predict independent
engine restrictions such as zero PP or Disable. Preview is unavailable during
AI calculations, rendering, or callbacks other than the normal mod update;
invalid and unavailable requests leave the output unchanged. The standard move
menu displays a supported ASCII denial reason in the game's battle font, wrapping
at word boundaries to two lines, and waits for the player's acknowledgement.
Empty reasons, unsupported characters, words
that do not fit, or text that needs more than two lines use the generic
“This move can't be used!” message. Messages are stored separately for each
battler and cleared at battle reset.
