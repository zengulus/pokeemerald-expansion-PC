# Still-needed mod API services

This document records capabilities that mod authors need, their existing host
support, and the gaps that remain. Implemented host services are documented in
[MODDING.md](MODDING.md) and the public header.

Names such as
`actor.spawn` and `weather.changed` below describe proposed capabilities and
contracts; they are **not** functions or event constants currently available
to a mod. See MODDING.md and the public header for implemented services.

The visual direction for HD-2D presentation is the reference described in the
request: existing pixel-art characters and Pokémon as camera-facing sprites in
a dimensional scene built from original tiles and environmental artwork. The
provided text describes that direction but does not contain an image; camera,
lighting and depth values would need to be tuned against the visual reference
when it is available.

## Modding-first scope

Start with the player-visible feature and the smallest shared operations it
needs. Saved world state should describe locations and species; ordinary wild
actors are temporary representations of that state. Their visible appearance
and battle opponent must agree, but offscreen individual simulation is not a
prerequisite. Unique or named encounters can opt into individual persistence.

The default photography flow records the species and location, stores a photo
and score, and saves an ordinary first-award flag with the reward. Loading an
older save restores the earlier state, including its rewards. The existing
scouting service currently records actor IDs, and the existing durable reward
services offer stronger rollback guarantees. The location ecology service can
store species/location observation counts, and committed scouting updates
them. The simple photo reward service grants a saved item once per rule,
species and location without requiring durable receipts.

Likewise, basic breeding needs compatibility, offspring and inheritance rules
and a useful preview, not exact joint odds. A basic renderer needs scene data,
a presentation interface and a fallback; music needs import, selection and a
fallback. Detailed analysis, tracing and rollback-proof receipts are optional
extensions when a mod actually requires them. Atomic changes, valid content
references and coherent battle rules remain essential.

The sections below retain implemented capabilities for reference. Their
historic detail is not a checklist of mandatory prerequisites for these
simpler features.

## What exists today

The public native interface is [mod_api.h](include/mod_api.h); the current
contracts and demo modules are documented in [MODDING.md](MODDING.md). A mod
can query stable keys and limited content metadata, use deterministic
`random_at`, declare settings and private state, and subscribe to the
listed events. Its reducers produce scalar decisions or six base stats; its
observers receive committed notifications. Save records have validation and
migration, and durable modules can merge monotonic journal facts. The host
owns the engine objects and commits gameplay changes.

Some engine features have only partial public mod API coverage:

* [Overworld wild encounters](docs/tutorials/how_to_overworld_wild_encounters.md)
  already support generated and manually placed actors, interaction, and
  several species behaviors when configured; `WE_OW_ENCOUNTERS` defaults to
  false. The follower demo only requests the existing follower behavior. The
  API exposes active actor snapshots, normal table-based spawn and removal,
  collision-checked steps, engagement, and bounded per-actor profile fields.
  The profile service does not itself provide schedules, social behavior or
  offscreen simulation.
* Species definitions have two ordinary types and battle Pokémon traditionally
  have a third temporary type slot. The native API exposes type identities,
  validated chart hooks, custom definitions and experimental extended species
  and battler lists. Several presentation and rule consumers still use the
  original three-slot projection.
* The battle-environment hook selects a GLSL 1.20 fragment shader over the
  rendered battle scene and supplies weather, terrain, and a UI/sprite mask.
  Independent battle render passes can now compose after that shader. A
  frame-scoped shader mask identifies attributable, visible battler sprite
  pixels, allowing effects to follow their drawn positions. It does not infer
  a ground surface or provide persistent Pokémon identity. A battle-backdrop
  catalog, baseline-load view and copied source pixels let a mod prepare and
  select scene-specific ground data,
  including trainer and map overrides, without claiming that later animation
  pixels match the baseline. Separate overworld
  services expose scene geometry and an ordered
  shader-pass chain, though they do not provide a general scene graph.
* The music hooks can select the bundled battle styles or a module-declared
  MIDI/SF2 pair for battle and other BGM contexts. A read-only service reports
  declared pairs and their availability. A local asset library and host
  services support importing, previewing and assigning MIDI/SF2 pairs.
* The Pokémon and daycare engines have internal nature, IV, EV, breeding and
  stat calculation routines. The public owned-mon service now exposes copied
  IV, EV and nature snapshots. The mon-edit service supports IV, EV, stat-nature,
  true-nature and Hyper Training changes. A breeding command
  can accept an existing pending egg through the normal engine rules.

## Shared proposed contracts

The requirements below depend on further versioned host extensions. The host now
supports named service discovery, required and optional service declarations,
and read-only owned-mon, daycare and overworld snapshot services. Many of the
capabilities described below remain partial or absent. Missing required services prevent activation with a useful
reason, while optional services can be queried for a fallback. Dependency
ordering among modules is separate from service requirements.

**Content and identity.** Proposed registries would assign stable namespaced
keys to new types, moves, abilities, effects, assets, behavior profiles, and
scene descriptors. Definitions would reference other keys, declare their
schema and dependencies, and be validated before a run starts. Duplicate IDs,
missing references and incompatible edits need an explicit conflict policy;
registration order must not silently change mechanics. Existing content may be
patched only through validated, field-specific composition rules. A transient
engine index is never a durable identity. Actor and Pokémon handles must be
opaque, checked for validity and scoped to the relevant world or run. A future
persistent individual ID must survive boxing and evolution without changing
or confusing existing Nuzlocke identities.

**Pre-alpha identity direction.** Native saves and link formats may break; do
not preserve their current layouts at the expense of embedding an individual
ID in the native boxed Pokémon record. Ordinary party, PC and daycare transfers
already copy that record, so the ID can travel with the Pokémon rather than
being maintained in a separate slot ledger. The ID needs explicit creation,
copy, trade, hatch and split-evolution semantics, and failures must not silently
produce duplicate or missing identities. Public APIs must distinguish it from
the existing OT/personality-derived identity.

**PC-native save.** The native format stores length-delimited game generations,
mod snapshots, host archives and special records without GBA sector-size
limits. The Hall of Fame grows with its contents. Writes are committed together;
old native saves are rejected. Shared-section corruption currently prevents
recovery, and game blocks still use the native engine layout rather than a
field-based, cross-build schema. The embedded Pokémon ID path remains to be
implemented; the slot ledger and tracker described below remain experimental.
The photo album holds 99 entries; the photo rewards and scouting log retain
their existing limits. Per-actor profile fields still have a fixed capacity.
The current mod/archive APIs also encode lengths as 32-bit values.

**Queries, commands and events.** A proposed service should return immutable,
versioned snapshots, not writable engine pointers. Queries do not advance RNG
or change state. Mutations go through validated host commands, with a clear
transaction boundary and committed event; rejection supplies a reason and
changes nothing. Typed payloads must have enough room for the relevant IDs,
locations, quantities and causes rather than overloading the four v1 scalar
arguments. Preview/UI events must be distinguished from committed changes so
reopening a menu cannot grant rewards or consume an encounter. Mutating an
actor or Pokémon requires authorization by the host's game rules, including
existing capture, revival, party-cap and challenge constraints.

**Composition and persistence.** The host should define stages, deterministic
ordering and conflict rules for every new hook: for example, collect proposals,
apply transforms, accumulate constraints, validate, commit once, then notify.
Denials remain sticky, and one mod cannot directly write another mod's private
state. Shared environmental or encounter facts belong to a host-owned service
or a versioned read-only capability, with one authoritative writer for each
fact. Persistent state needs stable IDs, schema migration, rollback and
consistent save generation. Monotonic durable journals are not a general database. The module archive
now stores dynamically allocated owner-keyed opaque records in each
ordinary save generation; missing owners retain their records. A bounded batch
can atomically update multiple records for one owner, and verified modal and
overlay input callbacks and selected committed observers can write their
owner's records. A write failure does not undo the underlying game event. Its revisions
can repeat after rollback and it is not a durable event journal. Large photos,
arbitrary assets, non-monotonic weather timers and per-actor records still need
appropriate retention and migration policies. The save layer can retain
multiple named host archives per generation, alongside the legacy photo
archive; a permanent owned-Pokémon identity archive is not yet implemented.
An internal generation-backed slot ledger supplies atomic mapping and rollback
primitives, OS-random 128-bit ID allocation, atomic seeding and an experimental
tracker for PC, party, daycare, fusion, scripted gifts, catches, daycare eggs,
hatching, evolution and in-game and link trades, with checked battle-menu permutations
and guarded Union Room, cable-club multi-battle and Frontier party substitutions
and restorations.
Random IDs make accidental reuse after rollback overwhelmingly unlikely. The
tracker checks occupied slots against its bindings on activation, at save
boundaries and at stable native frame boundaries before modal or overlay input
and the overworld mod update; unsafe temporary views defer this live check. It
suspends itself when its archive is replaced. While active, it
also follows an OT ID, original-trainer-name and personality change detector with each random ID, checking
PC pickup, placement and release as well as save boundaries, and updating it
on hatch. Public snapshots still contain only OT ID and personality, but checked PC,
daycare, fusion, party-reorder, catch-displacement and evolution boundaries
also compare the raw trainer name, including when encrypted data is unreadable.
Overwritten gift and split-evolution destinations receive the same pre-write check.
Gift receipts also verify that the actual destination matches the reported slot;
an unavailable or mismatched commit invalidates tracking, even for identical data.
Evolution, daycare, fusion and catch receipts also check their physical destination
and fail closed if their commit becomes unsafe. PC trade evolution is checked
against its staging slot. Interrupted nonempty party reorders also invalidate
tracking, while empty reorders remain no-ops. This does not cover every possible
transition or make the tracker a public permanent-identity API.
In-game trade receipts also check their physical party or staged PC source and
their final destination; an unsafe commit invalidates the tracker.
These detectors now persist with the same save generation and are checked against
the ledger's run, revision and complete ID set when restored. Old ledgers
without detectors remain readable but cannot start the experimental tracker. Version 1
detectors are accepted using their original OT/personality checks and capture current
trainer names on activation; they cannot prove the earlier names. Saving an activated
tracker writes version 2 detectors, which older hosts cannot read.
Direct `CreateMon` and `CreateBoxMon` calls into owned party, PC, daycare or
fusion slots now invalidate active tracking immediately, even for an identical
replacement or an empty slot; so do writes through those creation functions
to a saved party while a tracked projection uses it. They also invalidate a
staged save. Bulk clearing an occupied player party also invalidates the tracker;
clearing an already-empty party, and temporary or opponent Pokémon creation,
leave it intact.
Unreported `CopyMon` writes overlapping owned storage now also invalidate the
tracker, even for identical or partial copies. Checked gift, catch, fusion,
split-evolution and trade copies use their existing ownership receipts; a
self-copy and writes to temporary or opponent storage remain unaffected. PC
trade synchronization checks both its staged Pokémon and provisional PC
destination before the final copy, and again before moving the binding.
The boxed-to-party conversion also invalidates an unreported owned destination;
converting in place or into temporary or opponent storage remains supported.
Direct `SetBoxMonAt` writes use the same check; checked PC placements and
group moves retain their existing receipts. Direct `ZeroBoxMonAt` clears of
occupied slots invalidate tracking immediately; checked pickups, releases
and group moves retain their receipts, while clearing an empty, unbound slot
is harmless. An apparently empty slot with a remaining binding fails closed.
Battle Factory's initial rental selection and subsequent rental swaps also
check their party-slot writes: an unpaired replacement invalidates tracking,
even if its bytes match, while a verified rental projection remains supported.
The link-battle self-echo of the player's party is also checked byte for byte;
an unexpected changed echo invalidates tracking before the engine copies it.
The read-only multi-battle summary's temporary party layout is checked against
a copied snapshot before slot queries, at frame audits and before restoration;
an unexpected change invalidates the tracker and projected slot view. Common
Pokémon setters also invalidate them immediately when used on a player slot
in that read-only view, including writes subsequently reversed.
Raw battle-controller writes to owned Pokémon invalidate tracking even when
the copied bytes match, while opponent writes and zero-length writes do not.
Out-of-bounds raw commands are discarded before they can touch another slot.
Name getters also guard their caller-supplied output buffers when they overlap
owned storage, including a self-alias and empty slots. The daycare nickname
helpers also guard their final copied output, including during the temporary
battle-party summary; the boxed helper checks its actual written length.
Stored Tera-type output, battle-tower conversion and the three staged-egg
preview destinations receive the same overlap checks before writing. Failed
Tera reads and unavailable egg previews leave tracking unchanged.
The engine's bad-egg conversion now also invalidates tracking and the
read-only battle-party projection before marking a corrupt owned Pokémon,
whether the conversion was triggered by a getter or setter. Temporary and
opponent Pokémon remain outside that guard.
Breeding source-weight, IV-rule, ability, move and offspring previews also
guard their output buffers, as does the committed daycare egg's returned slot.
An invalid move preview or failed egg acceptance does not write its output.
Raw copies and clears
that overlap an owned slot in the read-only multi-battle summary invalidate
its field-slot mapping even if the bytes are subsequently restored.
Ordinary party reloads without a tracked restoration invalidate the tracker;
checked sky, link and rental restorations retain their bindings for one load.
Recorded-battle playback now uses a distinct non-owned party view and keeps
the original party's bindings in place. The tracker verifies the saved party
and other owned storage before restoring those bindings; unexpected writes or
an unpaired playback replacement still invalidate tracking.
Saved-party writes during a projection verify the original slot and its live
source, so matching OT/personality values alone cannot authorize a swapped
source. A repeated full-party follower-catch load is accepted only when the
saved party exactly matches the current one.
Sky and follower projections also retain a full backup at preparation and
check it before restoration; the follower-catch flow refreshes that backup only
after the full live party and saved party agree again. This detects changes
that preserve OT/personality but differ in other saved Pokémon data. Checked
selected-party writes update the retained copy, which is checked again just
before the full party is loaded. Initial sky/follower selection and follower
catch re-entry also check the copied selected entries against their full-party
sources before retaining IDs.
The common setters and rekey helpers also invalidate tracking immediately on
unreported changes to personality, OT ID, original-trainer name, species or occupied-slot status,
including changes that might otherwise be reversed before the next frame audit.
Engine-calculated form changes and checked evolution and fusion transitions
retain their bindings. Other battle-controller species writes fail closed when
they change a tracked Pokémon. The
verified egg-hatch OT rekey and trainer-name update remain part of its checked transition; unchanged
values and writes to temporary or opponent Pokémon remain valid.
That detector is not an individual ID: collisions, identical copies and changes
that preserve all detector fields are still ambiguous, and a change reversed between
audits is not detected by sampling. Direct `ZeroBoxMonData` and `ZeroMonData`
clears in tracked storage now invalidate an occupied or bound slot immediately;
checked transfers and party projections use their existing receipts. Some raw
storage writes still lack general immediate guards. Native CPU and DMA copies,
fills and LZ/RL decompression now also invalidate tracking before an
unreported write overlaps owned storage, including byte-identical writes.
DMA is checked when it actually runs, including delayed and repeated transfers;
exact self-copies are harmless. The checked party-reduction path retains its
existing projection receipt. The tracker is not enabled in normal
gameplay or exposed to mods; other battle and creation paths,
automatic initialization and complete provenance checks remain open.
Nature-format and link-conversion output buffers now receive the same overlap
checks on successful writes, including scalar result pointers. Valid in-place
conversions preserve bindings, and failed conversions leave them untouched.
The host now allows 128 registered modules, including built-in state modules,
instead of 32; hook counts and total save and journal sizes remain bounded.

**Determinism and presentation.** The host now provides a saved, run-scoped
player-step clock and committed step events. Further simulation clocks, stable event
IDs and explicit RNG ordinals should make spawning, photography rewards,
breeding and battle outcomes repeatable across reloads. Cosmetic animation
may use a separate visual clock; camera, particles and frame rate must not
change the world simulation. Presentation services should expose versioned
views of authoritative state, layer and occlusion information, and a defined
fallback. In particular, combining an HD-2D renderer with weather, terrain and
photo capture requires an ordered, compatible pass/scene contract. Battle
presentation also has an ordered shader-pass chain, separate from overworld
rendering and photo capture.

## 1. Visible overworld encounters

The species-visual service can decode source overworld Pokémon frames for an
explicit species/form, shiny and gender selection without spawning an actor.
It also exposes the corresponding source animation commands and facing mapping.
Batch decoding provides multiple frames from one sheet. Wild-actor snapshots
provide the displayed species, shiny and female flags; the caller still owns
animation state and must respect the actor's visibility and concealment.

Services need to support visible wild actors that reuse Pokémon graphics,
move and react to the player, and enter battle through collision or interaction.
Presentation needs terrain-specific cover and effects, including concealment
that can change with movement or proximity.

* **APIs and events needed:** `encounter.propose` for spawn candidates and a
  committed spawn/despawn stream; read-only map collision, metatile, position,
  elevation and actor snapshots; commands to create, move, interact with and
  remove an actor; and a `battle.beginFromActor` transaction. Proposal, failed
  allocation, actor engaged and battle outcome must be distinguishable.
  Reuse existing engine OWE collision, repel/lure, roamer and resource rules
  where applicable; expose them instead of bypassing them with raw sprites.
* **Definitions and state:** species/form animation and footprint, behavior
  profile, habitat, concealment layers and spawn policy; visible appearance,
  position and encounter eligibility. Ordinary actors can be regenerated from
  location state after leaving the map. Battle uses the same generated Pokémon
  the player saw, without rerolling it. Unique encounters may opt into identity
  and persistence.
* **Presentation:** animated facing frames, sprite depth/occlusion, grass
  foreground masks, dust/ripple effects and accessible indications when hidden.
  A scene renderer needs the same actor identity and concealment metadata.
* **Composition and gap:** ecology chooses allowed habitats and behavior;
  scouting reads the same actor; Nuzlocke consumes its attempt at the defined
  engagement point, not merely on spawn or observation. Randomization must
  choose the species once before it becomes visible. A post-randomizer wild
  candidate proposal includes the actual spawn tile; committed actor events
  distinguish allocation from Pokémon generation. Ordinary generated actors
  retain their Pokémon through save/load and battle, with session-scoped
  handles for observation. Mods can request table-based or specified-species
  spawns at a tile, subject to the encounter rules and candidate transforms,
  and remove live actors through the engine's rules when automatic
  encounters are enabled. Mods can request one animated, collision-checked
  step or queue the normal adjacent-player interaction; an engagement event
  identifies the actor when battle setup begins; a later battle-result event
  carries the same snapshot after the actor is removed. A checked catch-origin
  view can associate a committed capture placement with the engaged actor,
  including the correct opponent in a double wild battle; it is not a durable
  owned-Pokémon identity. Battle-result observers can also retrieve that
  verified placement without subscribing to the catch event; if verification
  fails, they must treat the actor's capture status as unknown. During battle,
  a checked view links the actor to its current participant when available,
  so battle rules can use the same actor without guessing from species or
  position. A captured profile snapshot also lets battle rules read an
  identified actor's bounded per-individual fields after its removal. Saved actors also get
  run-scoped IDs protected from native save-slot rollback; manual, roamer and
  outbreak encounters retain their existing Pokémon-generation behavior.
  The wild-concealment service adds hidden and engine ground-cover policies,
  with optional movement/proximity reveals. Bounded profile values survive an
  ordinary save while the individual remains present; identity across
  independent copies of a save is still missing. A read-only capacity snapshot
  exposes the current generated-encounter, object and sprite limits and usage;
  it does not reserve slots or predict graphics allocation success. A separate
  placement preflight checks the requested tile against the engine's common
  spawn rules without generating a Pokémon; later proposals, resource checks
  and world changes can still prevent the spawn. A species-and-level preflight
  additionally reports requested-content restrictions and current Repel and
  lead-ability filtering without generating or rolling an encounter.

## 2. HD-2D overworld presentation from existing assets

Presentation services need to support dimensional terrain derived from
existing tiles and camera-facing pixel-art actors, with configurable camera,
lighting, shadows and post-processing. The original gameplay simulation must
remain authoritative.

* **APIs and events needed:** a read-only map/scene snapshot containing tile
  layers, dimensions, elevation, collision, priorities, objects, animation,
  lighting inputs and map transitions; asset views with tile and palette
  identity; camera configuration and resize/lifecycle callbacks; and a
  versioned renderer registration and ordered scene/pass interface. Renderer
  output must not write game positions or run a separate simulation.
  The authored-map service also exposes original object placements on unloaded
  maps, including clone targets. It can decode the first source frame for
  supported fixed object graphics and query bounded source frames and animation
  commands, including the engine's facing mapping. It does not resolve live
  visibility or state, or variable, Pokémon, compressed and other unsupported
  sprites. A separate object-visual service can sample a saved variable's
  current graphics ID and decode supported fixed IDs without a map event;
  Pokémon IDs can be passed to the species-visual service. Its berry source
  queries use a berry key and growth stage to decode frames with the tree's
  palette and copy its animation commands, complementing current-map berry-plot
  snapshots. This does not run
  map scripts or predict a live actor's appearance. Authored background interaction records also expose static
  locations and kinds for scripts, hidden items and secret-base entrances;
  a separate read-only event-state service samples object hide flags and
  hidden-item collection flags, with temporary and process-local scopes
  identified. Runtime visibility, access and script behavior remain separate.
* **Definitions and state:** per-map or reusable scene metadata converts
  tile coordinates and elevation to geometry: ground meshes, raised edges,
  walls, cliffs, water surfaces, roofs, transparent cutouts, material/texture
  references, occlusion and shadow rules. An asset-derived default may cover
  common tiles, with explicit overrides for ambiguous art. Persist only user
  settings and metadata references; regenerate GPU resources after reload.
* **Presentation:** billboards face the camera while selecting the original
  animation frame and game-facing direction independently. Anchor each at its
  game-space foot point, sort by depth and layer, handle ties deterministically,
  and occlude it against cliffs, roofs, tall grass and water. Match map seams,
  indoor transitions, pixel filtering and transparency. Apply configurable
  lighting, shadows, depth of field, tilt-shift and environment shaders without affecting
  UI readability; provide the original renderer as a fallback.
* **Composition and gap:** visible encounters provide actor/occlusion data,
  weather and terrain provide visual state, and photography captures the
  selected presentation without changing simulation. The overworld service now
  exposes map tile entries and pixels, actor anchors, camera projection and
  individual drawable sprite parts, including recognized field-effect sprites.
  A per-part transform view and optional affine-rasterized pixel copy expose
  scaled and rotated sprites without changing engine state. A display-state
  snapshot exposes the sampled blend, mosaic and window registers to a
  presenter. A completed software-frame view also records the actual controls
  sampled at each scanline; it does not itself composite those effects. An opt-in sprite raster also applies an explicit
  screen-anchored mosaic grid before affine sampling, without layer blending.
  Declarative scene rules can annotate tiles
  by map, metatile, behavior and position with ground, upper surface, height,
  shadow and occlusion properties; the scene service resolves them with
  deterministic precedence. A scene mesh view turns those rules into textured
  ground, raised edges, walls, roofs and billboard quads; edge textures are an
  approximation. An opt-in inferred view recognizes water behavior and uses
  sign behavior and loaded pixel transparency to suggest sign billboards;
  explicit rules override it and other ambiguous assets stay flat. This limited
  heuristic does not classify most scenery or provide a general scene graph.
  An opt-in light trace now tests those same inferred meshes and transparent
  cutouts, while the original declared-rule trace remains unchanged.
  A geometry-only billboard trace accepts a presenter's oriented sprite part
  and alpha pixels, allowing it to compare sprite shadows with terrain hits
  without guessing the presenter's camera or changing gameplay.
  A bounded directional-light trace reports the checked segment and when the
  loaded window or height limits truncate it, so clear local rays are not
  mistaken for evidence about unloaded scenery.
  A combined trace compares that scene segment against a bounded set of
  presenter-positioned, alpha-tested billboards and reports the nearest hit.
  Both traces accept full 128-pixel double-size affine sprite rasters, so the
  same transformed alpha can drive rendering and occlusion.
  A selected world-environment shader and
  an ordered chain of independent shader passes run after the custom presenter
  and before the viewfinder overlay. A presenter can now expose its depth
  texture to independent world shaders. These remain proposed work; the visual
  reference remains a direction for a complete renderer.

## 3. Persistent overworld weather and terrain

The world-environment service also accepts module-owned weather and terrain
sources without requiring a Pokémon move. Sources share the existing priority,
constraint, timing, notification and save contracts; this is a host capability,
not an implementation of the wider weather or ecology concepts.
Explicit source-at commands can also update the saved effect of an unloaded
map with the same rules, without loading the map or simulating its ecology.
Module-owned sources can also remain active until cleared or replaced, without
requiring a recurring timer refresh.

The authored-map service exposes declared coordinate weather transitions,
including dynamic cycles, for unloaded maps. These are static event records,
not a forecast of live weather or the result of executing map scripts.

Services need to create, persist and expire map-scoped weather and terrain,
apply environmental interactions, and seed battles subject to battle-specific
override rules.

* **APIs and events needed:** validated field-move invocation, map/region and
  simulation-clock views, `environment.propose`, committed start/change/end and
  expiry events, environment-interaction queries, and a battle-initialization
  proposal. Define map scope, replacement priority, duration units, whether
  time advances off-map and on reload, and battle-to-overworld transfer policy.
  The world environment service now validates selected party moves, stores
  map-scoped effects, dispatches constraints and committed changes, expires
  them on the player-step clock, and seeds ordinary battles. It counts steps
  on any map. A versioned timed transaction can instead expire effects using
  the game's real or fake clock, including after a save reload; unavailable
  or changed clock sources leave the effect active until its source returns.
  Modules can map registered or
  built-in moves to supported effects; stored effects keep their resolved
  value if a module is removed or changes its mapping. The habitat query supplies tile
  classifications and both natural and saved environment state; specialized
  interaction rules are still needed. A pure habitat-weather resolver applies
  shared saved-over-natural precedence while preserving natural-only conditions
  and reporting whether saved state was available. An encounter-rate hook can use the same
  habitat and saved environment to adjust ordinary and facility wild checks
  after existing engine modifiers, and to adjust the separate visible-actor
  spawn countdown on its next reset. A separate habitat-aware fishing bite
  proposal can adjust the ordinary roll and the legacy preliminary ability
  roll, without skipping the engine's RNG draws. A versioned
  battle-seed proposal lets reducers change or deny each eligible seed without
  mutating the overworld state; scripted and trainer conditions take precedence.
  Checked preview-and-apply calls also reject changes to the reviewed request,
  map state, step or timer before committing, for both step and RTC durations.
  Environment transactions also recheck those inputs and the move user after
  constraints, and preserve gameplay RNG across change notifications.
  A coherent view combines both saved effects and their timers; bounded
  enumeration exposes active saved effects across maps without simulating them.
* **Definitions and state:** weather/terrain keys with eligible moves, scope,
  duration, affected metatiles/objects, interaction rules and battle mapping;
  persist source, location, start/expiry and any pending changes transactionally.
  Permanent scripted weather, abilities, suppression and battle-specific
  changes need explicit precedence rather than last-writer accidents.
* **Presentation and composition:** a shared environment view drives overworld
  particles/shaders, terrain effects and the existing battle effect selector;
  ecology changes its behavior and encounter weights using that same view.
  An optional party action discovers a Pokémon's supported moves and uses the
  validated service to apply them for 100 steps or two game-clock minutes.
  A battle-field snapshot exposes the underlying weather, suppression,
  effective weather, terrain and engine counters after seeding.
  A separate grounding service reports whether current terrain affects a
  living battler, with a knowledge-aware AI view for move scoring. Conditional
  previews can evaluate proposed terrain against the same grounding inputs
  without setting terrain or simulating its later reactions.
  A coalesced native-frame observer reports sampled field changes; changes
  reversed between samples and exact mutation causes are not reported. A
  separate committed stream covers the common weather and terrain setters,
  expiries and selected clearing paths, with move or ability attribution when
  supplied by the engine caller. Defog, terrain-clearing moves and Teraform
  Zero now identify their respective sources on their normal clear paths.
  It also reports explicit starting statuses,
  natural overworld conditions and the saved-world seed proposal path as
  distinct causes; other direct writes remain outside that stream. An opt-in
  timer event reports each non-expiring engine end-of-turn countdown; the
  existing expiry commit reports the final decrement. A separate committed
  event reports effective-weather suppression changes on supported ability
  mutations and, through a separate event, supported Neutralizing Gas
  activation and removal boundaries. Other switch, faint and suppression
  paths still rely on sampling. A separate presence event reports actual
  suppression changes at the engine's switch-out, replacement-data and
  faint-animation boundaries; it identifies the slot and boundary, not a
  permanent individual identity.
  The battle shader reads battle
  state after seeding. Software rendering falls back to the original frame. Built-in overworld
  particles, environmental interactions and ecology
  composition remain to be implemented. A bounded ground-effect interaction
  hook now lets modules adjust transient step splashes, ripples, tracks and
  dust using the current and previous habitats. A checked current-map
  berry-plot service exposes normal watering as a previewable transaction;
  a separate committed observation reports supported ordinary manual watering,
  including successful repeats that do not alter the plot.
  Read-only care snapshots expose weeds, pests and mulch, and committed plot
  observations report supported planting, removal and care interactions.
  A separate ordinary-harvest observation reports accepted normal and mutation
  berry additions before tree removal; it is not a durable reward receipt.
  A pure, previewable ordinary-berry yield rule can use the plot and habitat;
  the displayed and harvested counts use that rule while Bag checks remain in
  the engine and the stored tree yield remains unchanged.
  A coalesced natural-weather event lets other mods respond at the next safe
  overworld update without waiting for a step. A separate checked host transaction can
  water an explicitly selected unloaded-map tree record, with saved-weather
  guards and its own constraint and observation hooks; it does not simulate
  natural weather, visibility, or scripts. A clock-update observation reports
  coalesced changes to the global tree slots after a supported time update,
  including unloaded slots. Tree slots can be shared by objects, and the
  observation does not infer which map or weather caused a change. A paged
  reverse lookup lists recognized authored objects that reference each slot;
  unavailable source maps are reported explicitly.
  A checked field-healing transaction respects existing challenge and revival
  constraints and can recheck map, step, weather and terrain after proposals.
  Habitat-guarded heal and cure commands can also recheck the player's tile,
  natural weather and composed habitat before committing.
  A pure poison tick hook exposes the actual affected Pokémon and saved environment and can
  adjust or deny damage while retaining engine HP floors. It abandons a tick
  if the sampled Pokémon or environment changes during a proposal. An opt-in
  player-habitat sample also rechecks tile, natural weather and availability
  before damage. The default Gen V
  overworld configuration does not schedule poison ticks. Other gameplay terrain interactions remain
  open.

## 4. Ecology and wild behavior

Ecology is modeled primarily as conditions and species populations per map or
location. Mods decide how those values change; configured populations scale
encounter weights and mods can adjust the resulting weights. The host does not
simulate individual animals offscreen. Visible
actors may have temporary behavior, and exceptional named individuals can opt
into stronger persistence. The `rhh:service/ecology` service now stores shared,
save-backed location conditions and species counts; see MODDING.md for use.

* **APIs and events needed:** read-only map habitats, time and environment;
  species tag queries, location conditions and population counts; encounter
  weight and candidate proposals; and the ordinary committed encounter events.
  Offscreen updates use an explicit game clock or player steps, not rendering
  frames. Individual perception and movement hooks are optional.
* **Definitions and state:** versioned tag vocabularies, habitat predicates,
  environmental preferences and species populations per location. Mods can
  store timestamps or other named location conditions and decide how to update
  them. Individual schedules, profiles, social rules and timers are optional.
* **Presentation and composition:** actor animation, cues and concealment
  reflect the authoritative behavior. Visible encounters use the location's
  conditions and species; scouting observes them; overworld weather modifies eligibility or
  activity through documented precedence. The habitat service now exposes
  current tile classifications, collision, natural weather and saved map
  environment, with composable pure queries. A checked player-tile sample
  gives field-interaction mods the same habitat and resolved-weather inputs
  without deriving a player position from sprites. Static OWE behavior data and
  engine movement routines exist. The wild candidate proposal can choose a
  species using the sampled habitat after randomization. Relative table-slot
  weights can now be proposed for normal land, water, rock and fishing draws;
  the engine's ability and lure ordering still applies. Weight, encounter-rate,
  bite-rate, spawn-interval and candidate hooks preserve gameplay RNG. Impure
  proposals fall back without perturbing the engine's draw; weight denials
  remain effective and are isolated per slot. The world-time service
  samples the effective day period and shares one time across each habitat,
  weight or candidate proposal. The perception service adds a bounded tile
  line-of-sight query for player/wild and wild/wild pairs, with optional facing,
  scene, collision and actor checks. A separate constraint can cancel ordinary
  table and fishing candidates before generation. A separately opted-in
  special-encounter hook covers roamers, outbreaks and overworld Feebas while
  retaining their original species and saved data. Separately opted-in DexNav
  hooks cover its searched candidate and cleanly end a denied search. Separate
  facility hooks cover Pike and Pyramid generation and generated visible
  actors, preserving the checked Pokémon into battle. A separate scripted
  battle-ready notification reports opponents in common scripted single,
  double and legendary battles without changing their story scripts. Scripted
  `setwildbattle` script commands now have a separate opt-in, per-slot species
  proposal before creation, preserving authored levels and items. A separate
  event-mon proposal also preserves fateful status. Scripted
  battle cancellation now covers explicitly guarded static encounter scripts,
  with prepared-party and RNG rollback on denial. Annotated authored encounters
  now expose stable site keys during scripted candidate, permission, cleanup,
  cancellation and ready callbacks, and throughout the resulting scripted
  battle through its outcome notification.
  Separate ready observations now cover the ghost Marowak and both catching
  tutorials, and Birch's first battle, with their authored keys available
  throughout the battle. The ghost encounter now checks its final opponent
  after the Silph Scope and restores preparation state on denial. Other story
  scripts and proposals for remaining creation paths still need support.
  A separate tutorial permission family restores the prepared opponent and RNG
  on denial; Wally's scene restores the player's party and remains retryable,
  while the old man's scene returns without granting its reward. A separate opt-in permission
  and notification family checks manually placed Pike and Pyramid actors at
  engagement, using the actual generated opponent and preserving the actor on
  denial. Their authored sprites can differ from the facility opponent.
  A shared species-trait
  vocabulary supports validated, additive declarations from multiple mods.
  A composable live-actor trait query can vary those traits using the stable
  individual identity when available. A bounded, owner-controlled override
  record follows an identified actor through ordinary saves and applies before
  trait transforms; it is removed on despawn. A step-driven proposal can
  direct identified nearby actors to take collision-checked steps or engage,
  reporting the actual command status after the engine processes field input.
  A public habitat registry now lets modules declare and share named predicates
  over map, tile flags, time, natural weather and saved weather or terrain.
  Predicates can also match resolved weather with saved effects taking
  precedence over natural conditions. An explanatory evaluation distinguishes
  mismatches from unavailable time or saved environment and identifies the
  relevant predicates. Both queries can use the same samples as wild hooks;
  hypothetical samples are not live permission checks.
  A bounded activity collector composes eligible weighted actions by priority
  on the saved player-step clock, using durable actor identity for repeatable
  selection. Existing behavior hooks can override or deny the chosen action.
  A saved, constrained radius policy and inspection view exposes the actual
  loaded-map, player-step limits and current reasons an actor cannot enter a
  new proposal batch. Mods can expand the radius to cover the entire loaded
  map while retaining the bounded actor count and engine movement checks.
  A copied per-actor record retains the last accepted activity and its next
  selection boundary across ordinary saves; committed changes emit notifications.
  A bounded nearby-actor snapshot gives social behaviors deterministic
  distance and individual-ID ordering, with optional species and identity
  filters.
  Habitat searches now find nearby permitted terrain using the same composable
  tile rules. A fixed 3×3 neighborhood snapshot shares one time sample and
  distinguishes denied tiles from map edges, supporting shoreline and cover
  rules without assembling separate observations. A separate authored-map
  service exposes static headers, tiles and baseline habitat flags for unloaded
  maps, plus bounded pages covering source rectangles with explicit unavailable
  cells, single and batched decoded source tile graphics, authored
  connection geometry and warp records. A separate live-state query samples
  the saved dynamic warp and its ordinary destination-coordinate fallback;
  scripts and special warp handling can change the eventual destination.
  A separate authored-habitat baseline combines source tile flags with one
  clock and saved-environment sample. Registry predicates can evaluate that
  baseline while treating dynamic or unknown natural weather as unavailable;
  this does not evaluate live habitat hooks or predict map scripts.
  An authored-encounter catalog also enumerates explicit source table variants,
  periods and area slots for unloaded maps, with configured base slot weights
  by area and fishing rod. An ordinary selection query resolves the saved map
  variant and time fallback for a supplied period. A separate composable
  preview combines authored habitat and saved environment with nominal slot
  weights for an unloaded map; it does not apply randomization, live encounter
  rules or simulate persistent individuals.
  The map service does not apply other saved changes or simulate ecology.
  A bounded route preview now guides social and
  shoreline activity around nearby obstacles using the engine's wild movement
  restrictions, including the diagonal transitions made by sideways stairs.
  It checks a fixed local area and does not persist a path; each movement
  remains subject to the live engine checks. Point and habitat previews can
  also return every predicted cardinal input, landing tile, elevation and
  facing for mods that need to inspect the whole local route.
  A composable habitat-route query can select a reachable shoreline when a
  nearer matching tile is enclosed.
  The following existing individual services are optional, not prerequisites
  for location-based ecology. A saved profile service provides bounded, owner-keyed signed individual
  values with revisions and constraints, retaining values for missing owners.
  Atomic batches let a mod update several related fields at one revision while
  retaining per-field constraints and notifications. Optional, conflict-checked
  signed and Boolean field schemas expose defaults and validate changed values;
  opt-in providers can enumerate their fields with conflict checking.
  A bounded per-actor queue can retain up to eight movement or engagement
  commands across ordinary saves. Queued actions remain subject to behavior
  proposals and live engine checks, and
  normally advance when the unchanged command is accepted. Guarded routes can
  opt to advance only when the actor reaches the predicted waypoint, preserving
  interrupted or deviated steps for inspection and replanning. Missing owners
  suspend their queues. Point and habitat routes can atomically replace a queue when
  the complete route fits; an oversized route normally leaves the queue unchanged.
  Mods can opt to enqueue a guarded prefix and retain its point or habitat
  goal and owner across queue exhaustion and saves. The owner can explicitly
  replan or clear the reservation; paths are not reserved and habitat searches
  can select a different nearby matching tile on replan.
  Newly queued routes persist their expected waypoints and pause when the
  actor or its next predicted landing differs; a query exposes the mismatch
  for replanning. A route can opt into interruption by higher-priority
  activities, allowing wary behavior to take precedence without consuming the
  route. Accepted steps also expose a
  transient completion notification with the predicted and actual landing,
  interruption or removal, and queue-consumption details. Older saved queues remain unguarded, and no route is
  reserved against changes during an accepted animation. Fully exhaustive
  discovery and simulation outside the loaded map remain absent. The dormant
  actor service can explicitly suspend and restore an identified ordinary
  generated actor, preserving its Pokémon, location, traits, profile,
  activity, queue and concealment through ordinary saves. Restoration requires
  the saved map, a free tile and matching elevation, and announces the actor
  only after its saved data is installed. Roamers, outbreaks, Feebas and
  manual actors remain unsupported because their special engine state needs
  separate handling. Guarded explicit relocation can update a dormant actor's
  position across authored maps and notify observers. It does not prove a
  traversable route or account for saved map changes. A read-only cardinal
  step preview supplies authored collision, elevation and map-seam geometry,
  with explicit unavailable and ambiguous outcomes. Dormant profile fields
  can be queried and edited individually or in atomic batches through
  schema-checked, revision-guarded commands;
  loaded live-profile rules must opt into corresponding dormant hooks. A
  bounded saved player-step checkpoint lets an owner track pending work across
  saves without using render frames. An explicit transaction can commit an
  owner profile batch, authored-map relocation and checkpoint together after
  their checks. A profile revision guard can also protect a relocation or
  checkpoint decision that reads profile fields without editing them. The
  service does not itself schedule or simulate behavior
  while actors are dormant.

## 5. Scouting and photography

Support observation, identification and photography without starting battle.
The basic log can be keyed by species and location, with photos and first-award
flags saved together with ordinary rewards. Loading an older save restores its
older state. Observation must not implicitly consume a capture attempt. Exact
pixel analysis and rewards protected across save rollback are optional.

* **APIs and events needed:** actor visibility/identity and line-of-sight
  queries, observe/identify/photo commands, a view/camera capture request and
  committed observation/photo events. A reward is validated and saved together
  with its ordinary first-award flag. Experience flows through the normal host
  award and level-cap rules.
* **Definitions and state:** identification thresholds, photo scoring criteria,
  reward tables, repeat limits and privacy of unidentified information;
  observation log, first-award flags and photo metadata. Image blobs use a
  bounded host archive separate from the v1 private state buffer.
* **Presentation and composition:** overlay a viewfinder, show unidentified
  silhouettes and reveal observations, then show the actual 2D or HD-2D scene
  in a photo album. Ecology supplies pose and behavior; visible encounters supply
  the same actor identity; weather changes visibility/scoring if configured.
  A semantic accessibility bridge can expose explicitly published viewfinder
  text to an independent adapter; platform speech integration remains separate.
  Perception supplies actor identity, concealment and approximate tile sight.
  The scouting service records observations and identification for saved
  actor IDs, hides species in unidentified log records, and publishes a
  committed event after constraints. Other native snapshots still expose
  species to trusted mods. The log follows normal save rollback, so it cannot
  guarantee a first-photo reward. A capture service can deliver the actual
  software or composited OpenGL world frame for a visible actor, with a
  failure event if the scene changes before capture. An opt-in presented-photo
  request can target an otherwise 2D-culled actor;
  it commits only after a compatible presenter supplies a visible subject mask
  on the actual capture frame. Scouting can use the latest verified photo as
  short-lived, single-use evidence for observation and identification while
  the actor and world still match. The strict photo, scouting and projection APIs still
  require exact masks, and effects without a compatible declaration fail closed.
  Capture-scoped render provenance lets scoring rules recognize the actual
  presenter and shader passes, but does not itself measure image focus.
  A bounded host archive
  stores full-resolution frames and run-scoped photo IDs with each save
  generation. A score service supplies a distance and cover estimate,
  with composable rules able to inspect the captured pixels and available
  habitat and time. Scores follow their corresponding album generation;
  a software capture, an unchanged software frame presented through OpenGL, or
  a compatible custom presenter can expose the subject's on-screen top-layer mask
  and occlusion counts to scoring hooks. The same mask can provide rendered
  subject luma and dark/bright pixel counts; these do not separate illumination
  from sprite color. Position-preserving shader declarations let the masks
  survive compatible weather and pass chains; unmarked effects remain
  unavailable. Non-displacing local filters can instead declare geometric
  compatibility: the opt-in APIs expose the pre-filter mask as an approximation,
  while exact-mask scoring views remain unavailable. When a presenter
  publishes a compatible depth buffer, scoring hooks can
  inspect same-frame subject depths and their mask quality; depth is in the
  presenter's normalized units and is not itself a focus or clarity score.
  A pre-occlusion silhouette mask and separate on-screen bounds for it and
  the visible subject let scoring rules inspect obstruction and framing.
  A separate optional full-footprint measurement estimates the cropped portion
  from stable sprite geometry or a compatible presenter's full silhouette;
  it is unavailable when that geometry cannot be measured reliably. Explicitly
  approximate geometric captures can also expose the pre-filter footprint,
  framing and composition, and measure final image pixels at the mask's
  approximate subject positions.
  A capture-scoped composition view additionally reports the on-screen
  silhouette's and visible pixels' centroids and three-by-three distributions,
  so scoring rules can consider placement and asymmetrical obstruction.
  Photos of durable individuals can now receive item rewards, whether
  archived or the latest successful capture, through a durable receipt and
  per-save delivery state, including replay
  after rollback and deferred delivery when the bag is full. Visible-subject
  pixel contrast is available to scoring rules, though it cannot distinguish
  blur from flat artwork, lighting or other postprocessing. A reliable general
  focus or clarity score remains absent. Explicit
  zero-threshold rules can also reward unscored photos, with score availability
  retained in the durable receipt. Owned-mon experience views now expose the
  actual stored points and native growth thresholds for a future reward
  preview. A fixed already-scaled award can also be previewed against hard
  level caps and the maximum level without mutation. Its paged move-candidate
  preview uses current resolved learnsets, but learning decisions and possible
  evolution can alter the real sequence. The field level-up menu now checks
  each crossed level without temporarily changing the Pokémon's actual level,
  keeping mod-facing experience views consistent during move prompts. A
  validated party command can apply a fixed experience award through the
  normal move-learning and evolution screens, with permission checks and
  process-local completion notifications. Durable photo experience receipts
  now replay through that flow after rollback, with a per-save delivery marker
  and a checked unique party recipient. Matching relies on OT/personality,
  which can collide or change and is not a permanent individual identity;
  unavailable recipients leave their receipts pending. A pending receipt can
  be durably retargeted to another checked party Pokémon, including after
  loading an older save, without changing its original claim.

## 6. Extended types

Type services need to register new types and their attack and defense
multipliers, and support species with more than two types. An attack's
effectiveness must compose across distinct active defender types according to
a declared rule and bounded numeric format.

* **APIs and events needed:** registration and enumeration of types and chart
  entries; validated type-list queries/overrides for species and battlers;
  effectiveness, STAB, immunity, move targeting, AI and battle-rule queries.
  Form changes, added/removed temporary types, inverse battles and type-changing
  effects need a single documented ordering and duplicate-type policy.
* **Definitions and state:** type key, names, colors/icons, chart relations,
  defaults for unspecified pairs, and species/form assignments. Persist keys
  or a versioned mapping for saved custom assignments and battle state; define
  migration if a type disappears or a chart changes.
* **Presentation and composition:** summary and battle menus show all active
  types, including overflow. Moves and abilities reference the same keys, and
  damage, immunities and AI consult the same resolved chart. The engine's
  two species slots and three battle slots are not an arbitrary-length type
  system; type enums, bit widths, fixed tables and consumers need expansion.
  The native API now exposes existing type keys, species and active battler
  type lists, and a bounded chart transform used by the engine's shared
  matchup calculation. A bounded STAB modifier also participates in ordinary,
  Terastallized and AI damage calculations, with an extended event that exposes
  full active and original type lists. Species assignments can now fill
  the three existing battle slots, with composable proposals, and the summary
  and optional HGSS Pokédex display the resolved types. Species-type queries
  preserve both gameplay random streams and reject a proposal if a hook advances
  either stream. Pokédex type searches,
  DexNav, entry hazards and switch AI matchups use the resolved types.
  When enabled, native battle type indicators page through the distinct active
  types, reflecting Roost, Terastallization and Illusion.
  Illusion simulation and disguise checks use resolved types and restore all
  three slots after AI calculations; Sky Battle eligibility uses them too.
  Wild encounter attraction, party-type evolution conditions, Pike status
  immunity, randomized Pyramid requirements and Factory/Dome type evaluations
  also consult the resolved species list, as do follower type conditions and
  the overworld party Grass-type check. Ivy Cudgel and Raging Bull use the
  resolved form's secondary species type, including custom types, for combat
  and supported move displays. Contest Curse animations also use the
  resolved species list, including Ghost in an overflow slot.
  Custom definitions can now be registered and queried by stable key, with
  deterministic ordering and conflict validation. A keyed composition query
  combines longer lists with duplicate removal and bounded results. The engine
  chart resolves temporary custom-type slots in damage calculations, and the
  species proposal API can assign them in an extended native battler list.
  Combat, party matchup, overworld effectiveness and entry-hazard damage now
  compose a supplied type list before rounding and clamping, although their
  battler and species sources can supply more than three types. Those paths
  and switch AI obtain their inputs through an ordered, deduplicated type-list
  view that can hold the runtime type space. Legacy three-entry API queries
  reject larger results rather than returning a partial list.
  Two-type moves combine both attack types before saturation; Illusion's AI
  simulation tracks changes to extended lists and restores the real list.
  Repeated type-adding moves replace their prior temporary type without
  removing inherent overflow types; Reflect Type, Transform and AI snapshots
  carry the temporary-type distinction. Conversion 2 includes registered
  custom resistances and excludes every type already held by the battler.
  Dome and Factory type counts now cover those slots, and Factory dialogue
  names a custom-type leader. The native summary, HGSS
  Pokédex, DexNav and battle indicator sprite paths can render colored badges
  for those slots. The summary, optional HGSS Pokédex, DexNav and battle
  indicators cycle through longer lists. A versioned display lookup returns
  full built-in and custom labels; an optional party action pages through all
  resolved species types using those labels. Battle-scoped Tera rules can now use
  registered custom type keys; an optional unique-owned-battler view lets a
  mod save its own type choice and fall back when that key is unavailable.
  A separate owned-mon view exposes the engine's stored, personality-derived or
  forced Tera value and invalidates on sampled changes; it does not persist
  custom keys or predict battle-scoped overrides. A checked edit can set a
  built-in stored type or restore the personality default, with ordinary stat
  edits, optional payment and existing edit constraints in one transaction.
  Species-forced types retain precedence; custom stored keys remain open.
  A Dynamaxed custom-typed damaging move uses the engine's Max Strike fallback
  while retaining its resolved type for damage and type interactions; the
  fallback still uses Max Strike's animation and secondary effect.
  The Z-Curse selection preview uses the same effective extended type list as
  battle and AI, including Tera, in supported local battles.
  Custom types can also declare a consistent physical or special category for
  the pre-Gen IV split through a pure, constrained type-category proposal;
  absent or rejected proposals retain the move's configured category.
  OT/personality is not a permanent identity, and this does not yet provide a
  general host-owned assignment store or custom Tera form changes. Full-length
  label presentation in other native screens, remaining three-slot rule
  consumers, and broader content migration remain open.
  A separate AI-known type-list query lets move-scoring hooks and status
  estimates use an unrevealed Illusion's disguise rather than its hidden true
  types. A companion species query reports the AI's assumed species during
  those callbacks. A separate player-facing battler-type view follows the
  type indicator's disguise and visibility rules, including unknown results.
  Other authoritative battle views remain accessible to trusted mods;
  they are not a general information-hiding boundary.
  A coalesced battler-type observation now reports changes to raw and effective
  lists, including Roost and Terastallization, while switches establish fresh
  baselines. It is not a per-mutation transaction or a public-visibility view.
  A separate player-facing observation tracks the indicator view, including
  changes in Pokédex visibility or Illusion, without including the authoritative
  participant identity in its payload.

## 7. Extended moves

The battle-field-rules service exposes current Gravity, room, sport, Ion
Deluge and Fairy Lock state and raw counters to move and AI consumers. It does
not itself add effects or predict future changes. A coalesced observer reports
sampled changes, while a separate committed stream reports supported move,
starting-status, expiry and clear paths with cause; other writes remain outside
that stream. Registered move effects can queue existing Gravity or Ion Deluge
through normal engine handling; AI scoring for those requests remains mod-owned.
Move effects can also queue normal Trick, Wonder and Magic Room toggles and
offer current-state AI previews without choosing a scoring policy.
Mud Sport, Water Sport and Fairy Lock are available through the same queued
move-effect and independent AI-preview model, with engine restrictions.
An independent Sport view and coalesced observation distinguish field flags
from older per-battler sources and report the damage engine's effective state.

Existing moves can now receive a registered custom base type via the
`RHH_MOVE_BASE_TYPE` transform. Battle, AI and move presentation share the
lookup, with built-in dynamic type rules taking precedence. The move service
also exposes keyed base metadata and the engine's ordered level-up, teachable
and egg learnsets. A module can declare a move that inherits an existing
move's effect and animation, teach it through a constrained transaction, and
add built-in or registered moves to species' level-up, teachable and egg
learnsets. The same resolved lists feed creation, level-up, evolution,
relearning, breeding and displays. Stable move keys survive saves and missing
modules; registered moves participate in battle and AI. A copied-context power
phase can modify effective power for ordinary damage in both battle and AI;
ordinary hit-roll accuracy also has a shared battle and AI phase. A critical-hit
chance phase likewise offers bounded, generation-aware transforms for ordinary
battle and AI calculations; forced or blocked criticals bypass it. A pure
damage phase also accepts bounded proposals after the engine modifiers, with
separate ordered innate proposals for battle and AI. Separate opt-in events
cover confusion and disobedience self-damage using their existing unrandomized formula.
Fixed damage and immunities remain on their engine paths. Native Gen V-and-later
non-Z Beat Up AI evaluates each estimated hit separately using its own party when
the attacker is AI-controlled and the target is not, or known party data and
an apparent-species fallback for unknown contributors otherwise; its estimate can differ
from the actual party. A scoped contributor query identifies the species and
known party slot for Gen V-and-later Beat Up calculations, with explicit AI
estimate and apparent-species flags. An observer reports each eligible
engine accuracy check's actual result, while
distinguishing the OHKO path; later move outcomes remain separate. A pure
PP-cost phase applies at real deduction after Pressure, with slot and submove
context. A separate opt-in AI estimate uses cached ability knowledge and
reports uncertainty rather than exposing hidden abilities. An independent
observer can inspect the actual clamped PP deduction and whether the permanent-slot
controller update has been queued; it is not a save receipt. A pure base
priority transform is shared by move metadata, turn ordering and AI priority
checks; a separate battler-priority phase can use HP, field conditions and
the ability supplied by each real or AI caller. Direct AI priority queries
and engine AI damage simulations are flagged, while shared unclassified paths
remain possible. A shared, pure selection constraint can deny chosen move slots for both player
and AI, without affecting called moves or the Struggle fallback. Supported
denial reasons appear in the normal battle selection message. A base target
transform selects existing engine target kinds for built-in and
registered moves across selection, battle execution and AI.
An additional battler-specific target phase can adapt those existing target
kinds to battle context, including visible weather and terrain; selection,
execution and the AI receive distinct phase inputs, with the AI's cached
knowledge preserved. A rule can opt to lock its accepted execution target
from target setup through move cleanup, while selection and AI previews stay
current. Independent target previews can query current selection, execution
or AI target kinds without creating or consulting an execution lock; they do
not register new target kinds or predict individual recipients. A separate
candidate filter exposes the engine's living, Heal Block and Dynamax checks;
target kind, cursor rules and later redirection still determine actual targets.
Base-target
and effect-mode proposals preserve both gameplay RNG
streams; an impure proposal cannot silently change metadata or the live battle
result. Base-type, power and accuracy proposals likewise restore gameplay RNG,
discarding impure proposals independently of later innate contributions.
Mods can also map available built-in or registered moves to existing overworld weather or terrain
effects; saved effects retain their resolved value if their module disappears.
An effect collection adds existing status, stat, weather and terrain effects
to compatible damaging moves; battle scripts and AI consume the same list.
An observer notification copies the engine's completion, interruption and
per-battler result state at the final move-cleanup boundary. An optional
per-slot view aggregates direct HP and Substitute damage updates across hits,
and marks mixed sources or changed participants rather than attributing an
ambiguous total. Direct losses can also be observed individually as they occur,
with copied source, target, remaining HP and Substitute state; this is not a
complete hit stream. A separate view reports capped HP gains and losses from direct
move updates and scripted updates during resolution, without attributing scripted
item or ability effects to the move. The attacker-HP-to-zero command is also
included, allowing observers to see self-sacrifice HP loss. Other HP mutation
paths remain unobserved. A separate endpoint comparison can flag a net HP
change that the recorded updates do not explain; matching totals do not prove
that every mutation was observed.
A pure final AI score hook can account for mod-defined effects when choosing a move. It can
read the engine's cached damage, accuracy, effectiveness and known-ability
estimates without executing a committed effect callback. A separate pure
preview phase lets modules describe bounded candidate actions for independent
AI scoring hooks. An opt-in host heuristic estimates and scores those actions
individually; it is approximate and does not simulate their combined outcome.

The engine's move-property getters share one definition lookup. Learned moves have 11-bit save
fields, and `MOVE_RANDOM_TEACHABLE` is a separate engine sentinel near the
built-in range. The native host reserves 512 non-conflicting values and keeps
their stable key-to-slot mappings in each save generation; missing keys keep
their slots and new keys cannot reuse them. Daycare and breeding snapshots
can report a preserved move key even if its defining mod is absent. Host-run
schemas 1 and 2 migrate to the new format. An entry-resolution hook filters
built-in or added moves and reorders them within level groups or across
teachable and egg lists, with learning, breeding and displays using the same
result. General damaging-effect and target registration, full presentation, other battle
phase hooks, and network and recorded-battle compatibility remain open.

Move services need to support both inherited effects and new effects, field
conditions and target modes.
The battle service now registers keyed field and side conditions with shared,
battle-scoped timers. Committed move-resolution and custom status-effect
observers can apply them, and other modules can query them in battle and AI
hooks. Active
conditions have a persistent battle ribbon with side and remaining turns.
Committed lifecycle notifications also expose applications, duration changes,
removals and expiration to independent modules without polling.
An independent proposal phase lets modules adjust or deny committed move and
innate condition sets before the shared condition changes.
Effect observers and condition-expiration observers can queue ordered custom
messages in the battle window. Effect observers can also queue per-target
healing, stat changes and existing status effects through the engine's normal battle
scripts and controller updates. Healing can also be directed to the move's user,
including during an effect targeting another battler. Registered status effects now have stable keys
and can be bound to multiple compatible moves across modules; conflicting and
incompatible bindings are rejected at finalization.
Registered damaging effects can also run after ordinary hit moves' existing
secondary effects, including on knockouts, using the same action queue. They
can read capped Pokémon and substitute damage from the current hit and queue
user healing or engine-rule drain based on those amounts.
They can also queue fractional healing for another present battler, with the
same action limit and Heal Block checks. AI previews can offer and estimate the
specified recipient, including for field effects, and opt into approximate
automatic scoring without substituting the move's usual target.
They can also queue fractional indirect damage to a specified battler through
HP, faint and item handling, with an explicit Magic Guard override and a
corresponding knowledge-aware AI preview. This is one bounded action, not a
general replacement for damage formulas or custom targeting.
They can also base that indirect damage on the current hit's recorded HP loss,
optionally including Substitute loss, with a cached-damage AI estimate. This
does not invoke recoil-specific rules or aggregate later hits.
They can also queue a stat change for a specified living battler, including
from a field effect, with an AI preview and optional recipient-aware scoring.
Existing status effects can likewise target a specified battler, with normal
immunity and presentation handling and a recipient-aware AI preview.
Queued Gravity and Ion Deluge effects also have current-state AI previews;
mods decide their own scoring and account for uncertain eventual execution.
Queued effect actions recheck the source, intended recipient and, for
non-field moves, original target before running; they skip a replaced or
switching participant. Those checks use the trainer, party slot and
OT/personality rather than a permanent individual identity.
An effect observer can query current shared action and message queue occupancy
and inspect the ordered requests, including their recipients and action
parameters, to coordinate with earlier observers. The snapshot does not
reserve space or guarantee that a queued action will later execute.
Committed effect observers can also queue bounded existing animation cues for
living battlers through the normal battle controller. Independent condition
observers can cue changes committed from an effect, move resolution, innate
entry or innate turn-end callback, and expiry observers can queue cues that
play before expiry messages after rechecking the recipient. General damaging-effect
and target registration, move-phase scripting, and custom animation assets
remain open.

* **APIs and events needed:** move registration, learnset access, validation
  and execution; typed phases for selection, target resolution, cost/PP,
  accuracy, damage, effect application, interruption and completion. New
  mechanics need a versioned effect/command extension and target resolver,
  with explicit ordering, cancellation and AI/simulation support. Existing
  [move source definitions](docs/tutorials/how_to_new_move.md) are engine
  integration examples, not public dynamic-mod APIs.
* **Definitions and state:** stable move/effect/target keys, power, type,
  category, accuracy, PP, priority, flags, animations, learnsets and any new
  effect state/timers. Persist learned move keys and migrate removed content;
  register text and battle presentation assets as well as mechanics.
* **Presentation and composition:** animate and message each committed phase;
  an overworld-compatible weather move can call the environment service, and
  new type or ability rules participate in damage and targeting. A shared
  content-display query also copies built-in and registered move and ability
  descriptions for explanatory UI; they describe base definitions and do not
  automatically reflect later rule changes. The current
registration inherits existing animations; compatible hit moves can gain
additional existing effects, and status moves can run a mod-defined per-target
effect callback with custom messages, healing, stat changes and existing status effects. It cannot add target kinds, arbitrary scripts or general
battle callbacks.

## 8. Extended abilities and innates

Ability services need to support registered abilities and multiple innate
abilities with configurable unlock conditions and battle behavior.

* **APIs and events needed:** ability/effect registration; resolved active
  ability and innate-list queries; level-up, switch, suppression, replacement,
  trigger and battle-phase hooks. Specify ordering, duplicate abilities,
  interactions among simultaneous effects and whether each innate can be
  copied, swapped, suppressed, bypassed or overwritten. AI must use the same
  active set as battle execution.
* **Definitions and state:** stable ability keys, behavior and restrictions,
  species/form innate slots and unlock levels, plus optional individual
  overrides. Save keys and unlock configuration; derive eligibility from the
  authoritative level so editing or lowering levels has defined results.
* **Presentation and composition:** summary UI lists the normal ability and
  unlocked/locked innates; battle popups identify the actual trigger. A new
  ability may alter a new move, type, weather or ecology rule through the
  corresponding shared service. The ability service exposes species choices,
  stored owned abilities, restrictions and current battle ability state,
  including suppression and replacement. These species ability slots are
  choices, not three extra simultaneous innates. Mods can now compose and
  query a separate bounded, level-aware innate list, and implement individual
  behaviors through existing battle and AI hooks. A shared battle/power view
  resolves suppression, duplicate primaries and move bypass using actual state
  or the AI's cached knowledge. Pure per-innate power, accuracy, critical-hit
  and final-damage triggers
  run usable entries in deterministic attacker/defender order for battle and AI;
  an RNG-consuming trigger is discarded without cancelling subsequent entries. The
  list does not itself activate built-in effects. Mods can declare custom
  ability identities with restriction flags, offer them as innates and handle
  their power behavior across modules. A pure innate-priority trigger visits
  usable entries in order after the battler-priority proposal and uses each
  battle or AI caller's ability assumption. A shared speed proposal and
  per-innate speed trigger also feed battle and AI calculations, with caller
  ability and hold-effect assumptions. Confirmed AI paths are marked; an
  unmarked shared call may still be an AI calculation, and engine species and
  HP snapshots are not a hidden-information boundary. Effective Attack and
  Defense proposals and per-innate triggers now run before the ordinary damage
  formula in battle and AI, reporting the actual stat owner and stat/stage
  choice for moves with unusual calculations. Fixed damage and built-in
  innate activation remain separate. A committed switch-in trigger runs
  usable innates after hazards and ordinary switch-in abilities and can apply
  shared battle conditions, queue healing and stat changes for present battlers through the
  engine, and queue identified battle messages. A completed-turn trigger also
  runs usable innates after normal end-of-turn effects, with shared condition
  updates and queued healing and stat changes for present battlers before turn-end observers;
  messages follow the completed actions. Entry and turn-end innates can queue
  supported normal weather and terrain changes through the engine, with
  source revalidation and innate-attributed committed events. An opt-in,
  single-battle entry preview can offer corresponding field changes to the
  smart switcher after hazards; the resulting speed, damage and weather-impact
  estimates use scoped field values. A separate projected query lets each
  preview callback inspect retained earlier offers and its own tentative offers.
  A grounding query can evaluate that projected terrain for the simulated
  switch candidate, or compare a different terrain without applying it.
  It does not simulate terrain seeds,
  follow-up abilities, double battles or later turn-end field actions. Source-attributed entry and turn stat
  changes use the engine's ability protection and reflection rules, with checked
  source and recipient slots. The native Pokémon info summary can show the
  normal ability and proposed innates with unlock levels.
  Entry and turn-end innates can also request an existing major status for a
  checked recipient using the engine's ability status and immunity rules. An
  opt-in single-battle switch forecast models opponent-directed entry status
  with public types and cached AI knowledge, then recalculates speed and move
  damage. Known Synchronize can reflect the status to the candidate, with its
  protections and configured toxic behavior included in the forecast. Known
  status-curing berries are consumed in action order after Synchronize, with
  item suppression and Unburden represented in the temporary simulation. It
  remains approximate and does not forecast turn-end status actions.
  Individual constraints can add or remove entries for uniquely identifiable
  owned Pokémon, and a mod can save its own choice. This uses OT/personality,
  which may be reused, and does not supply a permanent host individual ID.
  Pure opt-in AI previews can estimate entry healing and stat changes when
  the smart switcher evaluates its own candidate after hazards. Its survival estimate uses
  original HP, applies hazards once and models a single-use healing item.
  An opt-in single-battle preview also models source-attributed stat changes
  to the opponent using cached AI knowledge, including protection, reflection
  and Defiant/Competitive, before the switcher evaluates damage and speed.
  Additional opt-in previews model constant and HP-dependent self-healing
  after completed turns in the switcher's survival estimate, without reviving
  fainted candidates. They do not predict changing field conditions, Heal Block
  expiry, targeted healing or other turn actions.
  A battle-scoped proposal now lets mods compose innates for opponent or
  partner slots as well as player battlers, with the same rule in battle and
  AI and without a separate discovery model. Switch forecasts retain the
  candidate's slot-specific innates when recalculating damage after predicted
  entry stat changes, including when the outgoing battler is absent or its
  switch is pending. An AI-safe slot-to-side query also lets innate rules use
  shared side conditions during prospective switch forecasts without reading
  the occupant. Trainer keys are supplied when
  available; slots and battle IDs are not durable individual identities.
  An observer can also receive coalesced frame samples of primary ability
  replacements, overwrite flags and suppression, with checked switch baselines.
  A separate observer samples the complete resolved innate list and its unlock
  and suppression status; species and individual proposals preserve gameplay
  RNG, including when run during sampling. A separate committed notification
  identifies supported move-script ability changes and Trace, with copied
  before/after states and grouped swap results. A separate contact event
  covers Mummy, Lingering Aroma and Wandering Spirit and identifies the
  triggering ability. A gas-field event also reports supported activation,
  suppression, switch-out, faint and replacement boundaries, with source masks
  and available before/after primary and innate views for battlers. These
  include fallback reporting when switch-in or faint cleanup clears a remaining
  gas source. A switch-in cleanup slot may already contain the replacement;
  these streams do not cover every raw engine mutation path.
  Separate observers report major-status applications through the common engine
  setter, including those queued by innate effects, and additions or cures from
  the common item-effect dispatcher. Their observers can apply shared battle
  conditions through the normal proposal policy, with target revalidation.
  Innate-queued status applications expose the originating innate and entry or
  turn trigger to status observers and their nested condition policies.
  Status and condition observers can queue animations, healing and unsourced
  stat changes after supported status and item presentations, including nested
  item presentations. Source-attributed stat and major-status actions use
  ability-style rules and recheck both participants. Indirect reaction damage
  uses normal faint and HP-threshold item handling. Selected major-status cures
  provide their own committed event and can trigger nested reactions. Take
  Heart's cure also reports its move and user before its queued reactions.
  The cure
  event also covers immunity abilities, Hydration and Shed Skin, with ability
  attribution, as well as natural wakeups, selected move-induced thaws and
  inline move and partner-ability cures. Natural Cure's switch-out status
  removal and scripted Uproar wakeups also
  report their distinct causes; only the latter has a queued reaction point.
  A separate party-cure event reports
  verified off-field player-party cures by Heal Bell and Aromatherapy, with
  reactions directed at active battlers. Bag medicine cures now report active
  battler status changes and a separate checked off-field party-item event;
  observers and their nested condition changes can queue reactions on active
  battlers after the Bag's cure message.
  Other direct writes and cures and
  additional reaction actions remain open.
  Ordinary ability slots, other battle phases, permanent individual rules and
  broader AI prediction of switch-in effects remain open.

## 9. IV and EV radar charts

Stat presentation services need six-stat IV and EV snapshots, drawing and
accessible text whose values agree across display styles.

* **APIs and events needed:** an immutable owned-Pokémon stat snapshot,
  effective per-stat/total limits, and versioned menu extension and redraw
  callbacks. A `mon.changed` event invalidates the chart after editing, level
  changes, boxing or reload; merely opening the chart is a pure query.
* **Definitions and state:** six stat identities/labels and chart styles;
  optional player display preferences only. Raw IVs, EVs and effective values
  remain engine-owned, not copied into mod state as competing truth.
* **Presentation and composition:** a polygon/line/text drawing surface and
  controller/keyboard navigation, plus numeric/accessible alternative text.
  Nature, IV and EV editors invalidate the same snapshot. Existing base-stat
  metadata and `RHH_BASE_STATS` do not expose an individual's IVs or EVs. The
  owned-mon service provides snapshots, and the v1 modal UI service supplies
  clipped polygon, line, text and thresholded RGBA image drawing plus rebound
  controls. The content-display service can supply decoded item icon pixels
  for independent interfaces; it does not add a menu. A keyboard
  binding query supplies current key labels for hints and semantic text; it
  does not describe controller bindings; the current native input loop reads
  keyboard input only. Mods can now
  offer actions for the selected Pokémon in the field party menu. Screen-reader
  output remains open. A modal accessibility bridge now lets panels publish
  copied semantic text and notify independent adapters while normal updates are
  paused; it does not implement platform screen-reader integration or
  automatically describe existing panels. An owned-mon invalidation stream now samples party and PC
  snapshots, including level and IV/EV changes and storage-slot additions and
  removals. It also covers daycare and fusion storage and defers unsafe
  storage and battle states; it is a coalesced frame view rather than an atomic
  transaction event.

## 10. Nature editing

Nature services need to preview and apply true-nature and stat-only changes,
and expose the rules affected by each mode.

* **APIs and events needed:** owned-Pokémon nature and effective-stat-nature
  snapshots; a validated, atomic `mon.changeNature` command; stat recalculation
  and a committed nature-changed event. Distinguish an actual nature change
  from a stat-only modifier, including flavor, breeding and Battle Palace
  behavior. Avoid silently changing personality, shiny status or identity.
* **Definitions and state:** nature keys, stat modifiers, optional eligibility,
  costs and mode; the authoritative nature/modifier and any transaction
  receipt persist with the Pokémon. Internal personality-based nature and
  `hiddenNatureModifier` require a deliberate migration/representation policy.
  Breeding, field behavior and Synchronize now use a common Pokémon-nature
  accessor. Battle
  Palace and flavor effects use a nature captured from the source Pokémon,
  then a matching battler's party Pokémon, with a personality fallback when
  neither is available. The battle API exposes that value, stat nature when
  known, and its provenance to committed local-battle consumers. The shared
  accessor can read a validated override when the internal format is active,
  or when a checked nature-format recording is playing; legacy recordings
  and unnegotiated link contexts retain personality-derived values. Rewriting it
  would change the identity used by challenge journals
  and may change other personality-derived properties. A true-nature field must
  therefore preserve existing Pokémon and save layouts, or supply an explicit
  migration, and reach those consumers including battle and link data.
  An internal staging-only codec exercises a zero-as-legacy override in the
  existing encrypted layout across all substructure permutations. Older link
  peers interpret those bits as part of the held-item field, so native link
  entry and block sends fail closed while the format is active. A conditional save-version marker lets a
  future edit identify incompatible save generations while keeping unmarked
  saves readable by the previous host. A read-only format preflight checks
  live and saved parties, PC, daycare and fusion slots before the marker can
  first be enabled; it identifies incompatible or unreadable legacy data and
  refuses unsafe battle, save, storage and link states. It also checks every
  party in the current save's recorded-battle sector. A versioned recording
  marker is rejected by older readers; current playback validates it and retains
  its format independently of the loaded save, and nature consumers honor that
  provenance. Separate exported recordings still need handling. Staged link conversion now rejects an
  override when a peer lacks support and rejects unnegotiated upper item bits
  on legacy imports instead of silently truncating a held item. A staged,
  all-or-nothing party conversion checks every occupied slot before publishing
  any result. A checked true-nature edit now activates the marker atomically
  with its Pokémon and optional item recipe; it offers explicit stat-nature
  policies and retains personality and identity. Link paths still need
  capability negotiation before format-enabled sessions can be supported.
* **Presentation and composition:** show before/after stats and rule effects;
  radar and breeding read the committed result. Level caps, challenge rules
  and item costs must compose as constraints. The mon-edit service supports
  a constrained, atomic stat-only nature change. Paid edits can atomically
  consume up to eight item types and money, with cost-aware constraints. The
  nature service exposes names, stable keys, stat multipliers, flavor
  preferences and Battle Palace behavior. True-nature edits can be committed
  alongside training and other stat changes as one transaction.

## 11. IV editing

IV services need configurable limits and atomic edits with stat previews.

* **APIs and events needed:** immutable per-stat IV and effective-value views,
  a multi-field atomic edit proposal, permission/cost constraints, stat
  recalculation and committed IV-changed event. Reject invalid edits without
  partially changing stats, HP, items or save state.
* **Definitions and state:** per-stat bounds and eligibility rules; explicitly
  decide whether hyper training is displayed or edited separately. Current
  IVs have five-bit storage and normally range 0–31; larger configured limits
  require an engine storage/serialization change, not just a UI setting.
* **Presentation and composition:** show current, preview and final values;
  radar and breeding inheritance read the same committed IVs. Nuzlocke and
  other restrictions can deny the edit. The mon-edit service validates and
  commits whole IV allocations with a constraint and a committed notification;
  it preserves hyper training. Item recipes and money can be charged atomically.
  Owned-mon `iv_view` also exposes Hyper Training flags and the effective
  stat-calculation IVs separately from inherited IVs, and sampled changes can
  invalidate cached views.
  A separate atomic training edit can set or clear the flags alongside an IV
  allocation and an optional recipe, with the same edit constraints and a
  checked preview; eligibility policies remain the responsibility of mods.

## 12. Manual EV assignment

EV services need configurable limits, atomic allocations and policies for
resetting, redistribution and earned unspent points.

* **APIs and events needed:** read current EVs, configurable per-stat and
  total limits and earned/unspent budget; validate an entire allocation;
  atomically commit, recalculate stats and issue one EV-changed event. Define
  how ordinary battle, vitamin, reset-item and max-level EV gains interact with
  the policy and with a level cap.
* **Definitions and state:** rules for earning, spending, reset costs and
  maximums; per-mon unspent budget if the chosen rules introduce one. The
  current engine uses byte-sized EV fields, a generation-dependent per-stat
  cap of 252/255 and a 510 total; exceeding storage needs a migration.
* **Presentation and composition:** show all six values, total/remaining
  budget, caps and a preview, optionally with the radar chart. Breeding and
  other edits observe the committed Pokémon, not a UI draft. The owned-mon
  service can read EVs, and the mon-edit service validates and commits whole
  allocations within engine limits and can charge an item and money atomically.
  A battle EV yield transaction now exposes the full earned yield
  before caps and can apply or bank the yield, including when the current EVs
  are full. Its outcome distinguishes an accepted bank from an independent
  denial. A separate limit
  policy can adjust per-stat and total limits for edits, ordinary battle gains
  and EV items within the existing byte-sized storage. An item EV transaction
  also permits positive single-stat field items to be applied or consumed for
  separately recorded points, including at the cap. A checked item preview
  exposes supported gain, banking and reduction decisions without consuming
  the item; it does not reserve inventory or preview friendship changes.
  Constraints and committed
  receipts also cover EV-reducing berries and the reset item in the field party
  menu, including their dedicated menu callbacks; no-effect berry use does not
  consume an item. Limit, battle-yield and item-yield hooks restore both
  gameplay RNG streams; impure proposals fall back while denials remain sticky.
  A general reliable earned-points bank remains open because durable individual
  identity and complete reward-path coverage are not yet available. Other item use paths
  retain their existing behavior. Limits above 255 per stat need a storage migration.

## 13. Advanced breeding

Breeding services need composable compatibility, offspring and inheritance
rules, with previews that do not roll an egg or spend items.

* **APIs and events needed:** read parent snapshots, egg groups, held items and
  compatibility; propose compatibility, offspring species/form, inheritance,
  egg creation and hatch; validate and commit with explicit deterministic RNG
  ordinals. Preserve daycare ownership, capacity, pending-egg, item and hatch
  rules, and report the actual result only after commit.
* **Definitions and state:** breeding groups, pair exceptions, inheritance
  rules and offspring/learnset references; save parent identities, pending egg
  recipe or resolved egg and its RNG identity. Specify precedence for items,
  form inheritance, ditto-like rules and conflicting mods.
* **Presentation and composition:** daycare UI explains compatibility and
  predicted ranges; nature and IV editors affect inheritance through the same
  authoritative snapshots. New types, moves, abilities and innates must resolve
  to valid offspring content. The daycare service exposes copied parent data,
  held items, moves, egg groups, pending-egg status and the engine's current
  compatibility category without advancing RNG. A read-only timing view reports
  the engine's next egg-roll and hatch-cycle
  intervals and the current previewed egg chance, including the Oval Charm;
  later rule changes and RNG outcomes can change whether an egg appears.
  A separate party-egg view reports remaining hatch cycles and an earliest
  hatch-check count, accounting for the current incubator and already-ready
  earlier eggs without promising that the party or rules will stay unchanged.
  A previewable pure hatch-permission constraint can defer individual ready
  eggs while allowing the engine to continue to later party members.
  A separate pure per-egg cycle proposal can pause or accelerate incubation;
  its preview shares the live rule and timing bounds remain conservative when
  a proposal is active. A committed cycle notification reports the actual
  per-egg update, including paused checks, without claiming rollback-proof identity.
  Public compatibility and offspring proposals participate in the live daycare flow, with copied
  parent context and a committed egg-created notification. A read-only pair
  preview runs the same compatibility and offspring rules without rolling an
  egg, including the possible species variants or the pending variant.
  IV-source and
  inherited-move proposals can override the engine's staged results with
  validation. Hatch notifications report the before/after owned-mon snapshots,
  including a possible OT identity change. A validated command can accept an
  existing pending egg through the normal engine path after constraints. A
  checked command can also request a pending egg for a compatible pair,
  bypassing the step-based chance with constraints and a committed event;
  it does not reserve the eventual result. Compatibility and offspring
  rules can still change a waiting egg, but a Nidoran or Illumise/Volbeat
  counterpart is now drawn once when the egg becomes pending and persisted
  with the daycare; repeated triggers cannot reroll that variant. Ordinary
  pairs consume no additional gameplay RNG for this selection.
  A checked offspring-odds preview combines equal species and reports the
  engine variant weights, with a flag when breeding hooks can make generation
  differ from preview.
  Compatibility and offspring
  proposals restore both gameplay RNG streams and discard a result that
  consumes either stream; compatibility denials remain effective. Request and
  acceptance constraints also restore both streams and reject an allowed
  command if a constraint consumed RNG. Committed pending, request, creation
  and hatch observers also preserve both gameplay RNG streams and the last-used
  ability, without undoing the engine's own generation draws. A shared transition notification
  also reports eggs made pending by walking or an explicit engine trigger,
  plus rejection and successful acceptance. It includes copied parents when
  available. A run-scoped random pending-event ID now follows save generations
  and can be read at acceptance. A checked namespaced stream lets pure breeding
  rules use repeatable, explicit ordinals for that ID. The ID does not identify
  the resulting owned Pokémon. A separate opt-in policy can derive the
  engine's egg-generation RNG from that ID, replaying the full egg for the
  same parents, trainer, game context and rules while preserving the surrounding
  gameplay RNG. A read-only exact preview exposes the staged egg and its
  inherited moves, ball and ability under that policy; it rejects older staged
  hooks that cannot safely run in a preview. It does not freeze those inputs
  or save a resolved egg. Checked acceptance revalidates the preview and actual
  staged egg before committing; changes to the previewed result or repeatable
  RNG policy leave the egg pending instead of giving a different result. Pure ball and ability-slot rules can change the
  inherited traits in both exact preview and the committed egg. A conditional
  ball-odds query models generation-specific parent selection and fallback
  balls for each offspring candidate, including pure ball rules. A separate
  ability-odds query models inherited and initial slots, including pure ability
  rules and engine fallback; it assumes independent draws and even personality
  parity rather than predicting a particular egg. Pure move rules participate in both
  previews and actual eggs; staged-egg move hooks can still change the final
  moves and are reported as an uncertainty. The IV preview
  accounts for power items and Destiny Knot. Older staged-egg inheritance
  hooks introduce uncertainty; a pure per-stat source rule can still narrow
  individual masks. A separate
  IV-odds model accounts for engine held-item rules under independent, uniform
  draws. A joint query can calculate the probability of matching six value
  masks together, including correlations between inherited stats. It does not
  predict deterministic RNG outcomes and declines to give odds when
  staged-egg inheritance hooks are active. Pure per-stat source rules can
  select either parent or the initial random IV after the engine rules, and
  are included in both marginal and joint odds without rolling an egg.
  A shared count proposal can select zero through six inherited IVs; its
  resolved count is also used by the preview and odds models.
  A nature-odds query models the engine's generation-specific Everstone and
  random-nature rules without rolling an egg. This uses true parent natures, not mint effects.
  A checked conditional gender-odds view also models species ratios and the
  personality rejection used for inherited natures. A per-candidate joint
  nature-and-gender view preserves that dependence under the same estimated
  model; neither view predicts other traits or reserves an egg.
  A pure nature proposal can select a known nature or random for a new
  egg, and the same deterministic rule participates in the odds preview.
  An opt-in save-backed sequence now assigns deterministic creation ordinals
  to committed player-daycare eggs from its activation point. Rollback can
  reuse an ordinal, so it is not a durable individual identity. Durable egg
  identities and registration or migration of new content remain absent.

## 14. Player ace designation

Ace services need stable player and trainer selections and matchup events for
cosmetic cues or explicitly configured gameplay effects.

* **APIs and events needed:** stable owned-mon selection and validation;
  trainer/boss and trainer-party ace metadata; committed designation,
  battle-start, send-out, matchup and battle-end events. A gameplay modifier
  requires typed battle-phase hooks and a defined stacking/expiry rule; a
  cosmetic cue must not modify combat.
* **Definitions and state:** boss-ace mapping, cue or effect key and triggering
  policy; save the designated individual ID, with behavior for release, trade,
  fainting, evolution, absence from the party and new runs. Never designate a
  species alone when several owned individuals can share it.
* **Presentation and composition:** an ace badge and matchup animation can
  combine with a boss-specific music selection. Nuzlocke still prevents dead
  Pokémon from being revived and trainer randomization retains the designated
  trainer/party slot identity. The battle service now exposes copied active
  participants, known trainer keys, and start, entry and end notifications;
  mods can compare the same snapshots for a matchup. A coalesced health
  observation reports sampled HP and maximum-HP changes for continuing
  battlers, with switch baselines; it does not identify the cause or every hit.
  A deferred battle-music
  cue request can use those committed events to select a track through the
  normal resolver, with stale-state checks and an outcome notification.
  Version 1.1 also reports
  each generated opponent's authored pool selection and explicit lead/ace tags
  through species randomization. A roster service exposes authored entries
  ahead of battle and validates a saved fingerprint and index against generated
  opponents, including after randomization and party reordering. Pool indices
  are not durable keys, and untagged teams need a mod-defined mapping. A
  save-backed boss-ace service now stores checked selections per trainer and
  difficulty, supports constrained updates, and matches generated opponents
  without silently remapping stale rosters. Owned-identity queries now detect
  collisions across party, PC, daycare and fusion storage, including corrupt
  matches. A bounded single census also identifies the locations of those
  matches, including unreadable ones; it does not create a durable identity.
  A committed PC-release event reports a copied Pokémon and the source
  slot, including when a held Pokémon's former slot has already been reused.
  PC placements, shifts and group moves now also emit copied movement receipts
  with source and destination slots. Overworld party swaps and compaction emit
  complete slot mappings. Daycare deposits, withdrawals and internal shifts emit
  ordered slot-transfer receipts. Fusion storage and restoration now report
  the transferred Pokémon and primary form change; battle reordering and other
  paths still need equivalent tracking before a permanent identity
  can follow them. The temporary battle-menu order now suspends owned-party
  lookup and change sampling until field order is restored. A checked mapping
  exposes field-slot origins for supported temporary battle-menu permutations;
  configured sky battles also expose the selected slots' original field origins
  through their reduced party and supported menu permutations. The experimental
  tracker preserves those original bindings through the checked restore.
  Reduced follower-partner battles also expose selected player slots and restore
  checked bindings, including player-only switches through the supported mixed
  menu and its temporarily combined summary view. Partner slots and
  unsupported layouts remain unavailable. Follower wild
  catches now reconcile the full and selected party views, including party and
  PC placement and the full-party send-to-box menu.
  The Union Room's selected pair has a separate party view and checked
  selected-to-field slot mapping, including supported single-battle menu
  switches. The experimental tracker preserves the saved full party bindings
  through its checked restoration, including arbitrary selection order.
  Cable-club multi battles also map selected player slots through the mixed
  menu and summary, excluding partner Pokémon, and restore checked bindings.
  This coverage uses engine-level tests rather than a live link session.
  Checked Frontier selections now expose their full-party slot origins, keep
  bindings through supported single- and double-battle menus and Battle Tower
  multi-battle menus, including full-team pages, and restore the full party for Frontier saves. Partner
  slots are excluded; unsupported substitutions and writes suspend tracking.
  The Dome's second selection composes through the first selection and is
  checked again before its two Pokémon are copied back or saved.
  Frontier selected-party swaps also retain their origins; the Pyramid restores
  those slots without guessing individual identity from species. The shared
  selected-party save and Sketch reset paths follow the current mapped order.
  Full-team in-game multi battles also expose checked player page mappings
  without treating the partner's separate page as owned Pokémon.
  Battle Factory and Slateport rental parties now have a distinct public
  party view. The experimental tracker keeps the saved original party's
  bindings through rental setup, replacement, battle-menu switching, Frontier
  saves and restoration; rentals are not exposed as owned party snapshots.
  Union, selected Frontier and rental projections now require their initial
  saved full-party backup to match the live party byte for byte, so a stale
  backup with matching OT/personality cannot silently acquire those bindings.
  They also retain that backup while projected; checked selected-party writes
  update the retained copy, while other changes invalidate tracking before a
  write or restoration. The tracker rechecks the backup immediately before
  loading it.
  Selected-party preparation and Frontier reselection also compare each
  copied Pokémon with the actual source bytes. Frontier's temporary save flow
  carries a checked capture of the projected party through the full-party save
  and revalidates it when restoring the projection.
  Wild-catch party-to-PC displacement and acquisition now report ordered
  receipts, including the newly assigned OT identity and final destination.
  Scripted gifts and non-battle owner-assigned acquisitions, including gift
  eggs, report their actual destination and any overwritten occupant. Other
  creation paths still require tracking. Debug PC-box fills now report separate
  committed creation receipts and the experimental ledger assigns distinct
  IDs even to the identical copies made by its fast-fill mode.
  Mystery Event gifts report their temporary party placement before compaction,
  and the experimental ledger follows the gift through that reorder.
  Evolution receipts cover ordinary and trade species changes and split-created
  Pokémon, including the temporarily staged PC-trade copy. Link-trade evolution
  events precede the save and do not establish a durable individual identity.
  In-game trades report outgoing and incoming Pokémon, including PC slots.
  Link trades now also retain a bounded history in the same save generation,
  allowing a missed process-local notification to be reconciled after a crash.
  Before the trade save, the received Pokémon's identity fields and readable
  IV and egg data are checked again against the exchange; a readable species
  change requires a matching committed trade evolution. A mismatched receipt is discarded and the private
  identity tracker is invalidated without cancelling the underlying trade.
  Content-linked checkpoints detect gaps, rollback and differing histories;
  identical divergent copies and permanent Pokémon identities remain unresolved.
  A trainer catalog exposes readable encounter labels and an
  approximate boss filter. Owned-mon selection and invalidation also cover
  daycare and fusion-storage slots with the same safety checks. Player designation, durable
  individual identity and a player-facing designation UI remain absent.

## 15. Arbitrary MIDI and SoundFont imports

Music services need validated MIDI/SF2 import, naming, context assignment and
fallback behavior for missing assets.

* **APIs and events needed:** host-managed import/validation and asset
  registration, stable MIDI/SoundFont keys, selection and context lifecycle
  events, audio playback/loop/volume/fade control and an asset availability
  query. Define supported MIDI/SF2 variants, resource limits, format errors,
  and original BGM/SFX mixing and fallback behavior.
* **Definitions and state:** asset manifest with names, content hashes,
  instrument/loop metadata and context mappings; persist stable references
  and user selections rather than a machine-specific absolute path. A host
  asset store or re-import process is needed for moves between machines.
* **Presentation and composition:** file picker/import feedback, preview,
  context assignment and missing-asset status; an ace encounter or ecology
  context can propose a track through the same resolver. A module can now
  declare relative MIDI/SF2 pairs and select them in battle or other BGM
  contexts; the music service reports availability. A separate track
  inspection also covers bundled Game Boy, MP2K and MIDI styles without
  disturbing current playback. Its results are point-in-time asset checks,
  not reservations. A separate source-timing query exposes validated MIDI
  loop markers and the last parsed event without starting playback or running
  loop proposals; native engine loops are identified without invented timing.
  A source-instrument inspection reports each used MIDI channel/program and
  the actual SoundFont preset or playback fallback, with a bounded list and
  aggregate note counts. It does not establish sample quality or preserve
  assets on a different machine.
  A companion note-coverage query checks each MIDI key and velocity against
  the selected preset's regions and reports bounded missing-note details;
  matching regions do not guarantee audible samples or available voices.
  A save-backed assignment
  service now resolves choices for contexts, original songs, maps and authored
  trainers, with constrained edits and fallback when a track is missing. A
  host-owned, content-addressed local library imports validated MIDI/SF2
  pairs, deduplicates shared files and exposes stable keys. A path-independent
  pair export and verified in-memory import let an external tool transfer the
  same content and retain that key on another host; saves do not embed the
  files or transfer them automatically. A modal preview service supports
  auditioning tracks. Rematches are separate authored selections. Windows uses
  a system file chooser and Linux uses the desktop portal when available.
  A composable, bounded gain proposal can balance each successfully loaded
  MIDI or Game Boy replacement using track and context keys while preserving
  normal game fades and volume controls; a playback snapshot exposes its
  mixer position and MIDI loop points. A versioned, deferred start/stop stream
  identifies the actual replacement or MP2K fallback and exposes queue overflow;
  a bounded battle-event cue request can restart the current original song
  with a requested replacement, subject to music selection and fallback;
  a bounded pure MIDI loop proposal applies to playback and previews, whose
  sample position and resolved loop bounds are available separately. Game Boy
  and MP2K retain their native loop engines. Music selection and assignment
  constraints restore both gameplay RNG streams, rejecting impure proposals
  while preserving denials. Modal previews remain separate. MP2K tracks and modal previews retain
  their existing controls. Save-backed per-track levels multiply that proposal;
  track levels follow normal save rollback and ramp briefly when changed during playback.

## 16. Cross-service composition

The following is the desired simple flow, not a claim that every step is
implemented. Advanced actor and durable-reward services remain optional.

The wild traits API provides one building block for this concept: an
engagement observer can request individual trait values for a verified battle,
without retaining a live overworld actor. This is an API capability, not a
completed implementation of the interaction below.

1. A field move requests rain on a route. The host validates the move and
   environment policy, commits a scoped timer and publishes one environment
   change. An ecology query sees rain and proposes a water-preferring species.
2. Ecology selects encounter weights from the location's conditions and
   species populations. A visible actor has a temporary handle and shared grass
   or ripple concealment. Scouting records the species and location; a first
   photo can grant a reward and save its claim flag in the same game state.
3. Interacting with the actor starts a battle with that same individual. A
   single battle-initialization resolver considers overworld rain, scripted
   weather and ability rules in documented order. The battle shader reads the
   result; it does not decide the mechanical weather.
4. A custom move references a registered type; the shared chart and the target's
   full active type list determine effectiveness. Normal abilities and unlocked
   innates execute in a defined order. The AI, messages and animation consume
   the same resolved outcome, with each mod's constraints accumulated.
5. After battle, the authoritative result updates location populations,
   rewards and any persistent environment state according to explicit policies. Later nature,
   IV or EV edits use atomic commands; radar charts and breeding read their
   committed results. If the designated ace meets a boss ace, a presentation
   cue and selected imported MIDI may play without changing the combat result
   unless a separate gameplay modifier was explicitly configured.

**Failure and compatibility:** a denied capture, failed photo reward, missing
asset or incompatible content definition must not partially commit unrelated
state. A failed optional renderer or music asset falls back to the original
presentation. Save/load and migration need tests for missing mods and content,
reordered registration, interrupted commands and changed schemas. A replay should preserve gameplay outcomes across both presentations; ordinary
wild actors do not require a permanent identity. None of the additional shared services above is implied by the
current v1 API merely because an analogous engine routine exists.
