# Native mod API checkpoints

For current requirements and open gaps, see
[Still-needed mod API services](NEEDED_MOD_API_SERVICES.md). This file records
host-service development and historical validation checkpoints.

The wild-dormant service now suspends and restores identified ordinary
generated actors with their saved Pokémon and per-actor records. It has a
bounded save-backed store and committed actor notifications; it does not
simulate dormant actors or handle special encounters. A separate native mod
probe and fresh-process save tests cover the transfer and its event boundary.
Guarded explicit relocation can update a dormant individual's authored-map
location across maps; dynamic map state is checked later when it resumes.
A separate read-only preview reports authored adjacent tiles and map seams
without claiming to simulate live movement or changing the actor.
Dormant profile queries and guarded single or atomic batch edits preserve saved
values, apply schemas and require compatible dormant hooks for loaded
live-profile rules.
An owner-controlled, bounded player-step checkpoint now tracks pending dormant
work across ordinary saves without prescribing the work or its cadence.
An explicit transaction can atomically commit a dormant actor's profile edits,
relocation and checkpoint, publishing notifications after the shared commit.

The type service now emits coalesced battler-type observations with before and
after raw and effective lists. Switches establish new baselines, and the stream
does not claim to capture every intermediate mutation or hide opponent types.
An independent player-facing stream reports indicator visibility and Illusion
changes without placing authoritative identities in its event payload.

The shared content-display service can now copy item, move and ability
descriptions for explanatory mod UI, including registered move and ability
definitions. Its ASCII conversion does not resolve dynamic placeholders.

Independent battle render modules can offer ordered shader passes after the
selected weather and terrain effect. The host restores protected interface
pixels, and rejects conflicting, denied or RNG-consuming collections. A
same-frame battler mask lets shaders identify attributable visible sprite
pixels without assuming fixed battler positions.

The move and ability services now expose an ordinary critical-hit chance phase
for battle and AI, with generation-specific bounds and ordered innate triggers.
The AI's critical estimate also avoids indexing its stage table for blocked
critical hits or Gen I thresholds.

The same services expose a bounded post-modifier ordinary damage phase and
ordered innate contributions for battle and AI. Fixed damage and immunities
remain outside this phase. Separate opt-in events cover confusion and
disobedience self-damage after their existing unrandomized formula. Native
non-Z Gen V-and-later Beat Up AI applies ordinary damage proposals per estimated hit,
using its own party where identifiable, or known party information and an
apparent-species fallback.
A scoped Beat Up contributor query supplies species, eligible-hit index and
known party slot during power, stat and final-damage hooks, with explicit AI
estimate flags and no permanent individual identity.

Move-effect observers can inspect the shared action and message queue occupancy
and the ordered action requests before offering actions. The local Z-Curse preview also uses the same effective
extended type list as battle and AI, including Terastallization.

The experimental owned-identity tracker now fails closed when a corrupt owned
Pokémon is marked as a bad egg by either a getter or setter. The read-only
battle-party projection is also invalidated before this mutation; the tracker
remains incomplete and is not exposed as a permanent identity API.

A bounded owner-keyed module archive stores opaque records with ordinary save
generations, retaining missing owners and checking revisions and write ownership.
Atomic batches validate and commit related record changes together.
Verified modal, overlay and party-action input callbacks can also commit their
owner's archive records while normal update callbacks are paused.
Selected committed event observers can write their own records while their
event data is available; failures do not roll back the underlying event.
It is not a durable journal or a permanent individual identity service.

The habitat registry can explain predicate results, distinguishing known
mismatches from missing time or saved-environment inputs while preserving the
existing Boolean matching contract.

The authored-map service pages source rectangles in row-major order, including
unavailable cells, for bounded full-map terrain inspection without loading or
simulating the map.

An authored-encounter catalog exposes source wild tables for known loaded or
unloaded maps, with explicit variant, period, area and slot metadata. An
ordinary selection query resolves the saved variant and time policy for an
explicit period; it does not simulate encounters. A shared query exposes the
engine's configured slot intervals, including the fishing-rod ranges.

The experimental owned-identity tracker now guards the final output of the
daycare nickname-copy helpers when it overlaps tracked storage or the
read-only battle-party summary. This closes one aliasing path; the tracker is
still not a public permanent-identity service.
Stored Tera reads, tower conversion and staged-egg preview outputs also guard
their caller-provided destinations against unreported owned-storage writes.
The same checks cover breeding preview buffers and the slot returned after a
daycare egg is committed, including temporary battle-party summaries.

The breeding service provides conditional gender odds for each previewed
offspring, accounting for pure nature rules and personality rejection under
an independent-uniform model.

Overworld presenters can sample the current display blend, mosaic and window
controls, including blend targets and mosaic block sizes, alongside sprite-part
flags. The snapshot does not reproduce scanline changes or compose pixels.
An optional actor or field-effect raster applies caller-supplied mosaic sizes
before affine sampling, using the screen-anchored pixel grid.
The latest completed software overworld frame also exposes the sampled display
controls for each scanline with a process-local capture sequence.

The world-environment service accepts module-owned weather and terrain sources
without a party move, with checked previews, shared priority and expiry rules,
and compatible projection to earlier save schemas when no such source remains.
Explicit source-at transactions extend those rules to known unloaded maps without
loading or simulating them.
An explicit permanent source clock supports effects retained until clear or
replacement, with a distinct save schema and timing view.

The battle-field-rules service copies Gravity, room, sport, Ion Deluge and
Fairy Lock flags and their raw timers for independent move and AI consumers.
Registered damaging effects can queue indirect damage based on a hit's capped
loss and expose an approximate cached-damage preview to AI.
Its coalesced observation event reports frame-to-frame changes outside AI
simulations; it is not a complete committed mutation stream.
Committed events also cover supported field-rule move setters, initial rooms,
normal expiries and Ion Deluge clearing, with explicit cause and attribution.
Move-effect observers can queue existing Gravity and Ion Deluge
effects through the normal engine scripts, with stale-participant checks.
Independent AI scorers can inspect matching Gravity and Ion Deluge previews;
the current-state estimate does not prescribe a scoring policy or predict execution.
Battle grounding queries report live terrain eligibility or the AI's
assumption-based view, including unknown ability and item cache values.
Conditional grounding previews also evaluate a proposed terrain without
setting it or forecasting reactions to the change.
Switch-in innate field previews can inspect grounding for their own simulated
candidate against retained and tentative terrain offers.
Queued move effects can also toggle the three rooms through the engine's
normal state and presentation logic, with independent AI previews.
Mud Sport, Water Sport and Fairy Lock are likewise queueable and previewable
with their normal generation and sky-battle rules.
An independent Sport snapshot and sampled change event expose both field and
legacy per-battler sources without treating slot numbers as permanent identity.

The move service can preview the current selection, execution or AI target
kind for a living battler without using or changing a move's execution lock.
The result is a target kind, not a prediction of the final recipient.
An independent candidate filter reports the engine's per-battler targeting
restrictions, leaving target-kind and later outcome checks to the caller.
The UI service now exposes copied keyboard labels for rebound logical buttons,
including both keys of a dual binding, so mods can present current control hints.
A content-display service copies player-facing species and item names as well
as existing move, ability and type labels for independent mod interfaces.
It also decodes engine item icons with their selected palettes. Modal and
overlay canvases can draw RGBA source images as thresholded transparent cutouts.
The same content service exposes bounded Pokémon menu-icon frames selected
by species, personality and egg status, with the engine's matching palette.
Wild-actor placement preflight now checks a requested tile against the engine's
shared deterministic spawn rules without generating a Pokémon or reserving it.
Habitat queries can also copy a time-consistent 3×3 neighborhood, with
separate masks for map boundaries and denied tiles.
An authored-map service exposes static layouts and baseline habitat data for
loaded or unloaded maps without running scripts or simulating the world.
It can also decode authored tile graphics with their source palettes without
changing live graphics state; runtime animation and palette effects are separate.
Bounded batches reuse each decompressed tileset and report unavailable entries
individually, supporting renderers that need many authored tile images.
Authored map connections expose destination keys and available seam geometry
without claiming that a route is passable or simulating the destination.
Authored warp records likewise expose known destination events while marking
dynamic and unavailable destinations explicitly.
Authored object templates expose static placements and distinguish ordinary
objects from clones and runtime-variable graphics; they are not live actors.
Authored coordinate events distinguish static weather transitions from script
triggers, including dynamic cycles, without executing or predicting either.
Authored background events expose static interaction locations, facing kinds,
hidden-item metadata and secret-base IDs without evaluating scripts or saves.
An independent saved-event-state service samples authored object hide and
hidden-item flags, distinguishing saved, temporary and process-local scopes;
it does not predict actor visibility or execute map scripts.
For fixed uncompressed authored objects, the service can also copy a first
source frame and palette as RGBA without loading the map or altering VRAM.
Bounded source-frame queries also cover the engine's contiguous and explicit
image tables, though they do not predict animation timing or direction.
Copied source animation commands and the engine's facing-to-animation mapping
now expose frame durations, flips and control flow without running an actor.
The species-visual service can also decode compressed Pokémon overworld source
frames with the follower engine's chosen palette and explicit placeholder flag.
Its bounded animation and facing queries expose frame timing and flips for the
same graphics without instantiating a Pokémon actor.
Batch frame decoding reuses one decompression and accepts the species and
appearance already exposed by a wild-actor snapshot.
A separate object-visual service resolves saved graphics variables and exposes
fixed source frames and animations independently of an authored map event.
Its copied dynamic-variant selection also supplies the Mauville old man's
saved-choice artwork, palette and animations without instantiating an actor.
It can also decode berry-tree source frames and stage animations from a berry
key and growth stage, using the engine's stage-specific palette and dimensions.
The species-and-level preflight also reports content restrictions, Repel and
the possible lead-ability roll without consuming gameplay RNG.
The daycare service now exposes the current egg-roll and hatch-cycle timing,
including a preview of compatibility and the Oval Charm's effect on egg chance.
It also exposes each field-party egg's remaining cycles and the engine's
party-order hatch priority, with a conditional earliest hatch estimate.
A previewable hatch-permission constraint can defer a ready egg while letting
later eligible party eggs proceed.
Per-egg hatch-cycle proposals can pause or accelerate incubation and have a
matching preview, with conservative timing bounds when rules are active.
A committed cycle notification exposes the actual per-egg update, including a
paused check, to independent observers.
The berry-plot service can now observe successful ordinary watering on verified
current-map plots, including use from the field Bag, separately from its
mod-initiated watering transaction.
It also exposes weed, pest and mulch state and observes supported planting,
removal and care interactions on verified current-map plots.
An independent harvest event reports accepted ordinary berry-tree item
additions before removal, including a partial mutation harvest when applicable.
Previewable yield proposals also feed the ordinary berry count in the tree
dialogue and harvest without changing the stored tree record.
Authored berry-object queries can pair unloaded-map placements with their stored
tree state for presentation, without simulating growth or claiming live visibility.
Base-target and effect-mode proposals now restore gameplay RNG and discard
impure changes; independent effect-mode denials still take effect.
Base-type, power and accuracy proposals also preserve gameplay RNG; impure
general or innate calculations fall back independently in battle and AI.

The battle service can report a verified Heal Bell or Aromatherapy cure for
an off-field player party member after the controller applies it, and queue
reactions on active battlers. Other off-field status mutation paths remain open.
Active status-cure observations also cover Natural Cure on switch-out and
scripted Uproar wakeups; only the latter has a queued reaction point.

Move definitions can now expose a composed base-priority rule to metadata,
turn ordering and AI. A separate pure battler-priority phase sees each
caller's ability assumption and the current battler and field values; known
AI queries carry a flag without claiming that every other query is live.
An independent PP-deduction observer reports actual clamped battle-slot
changes and whether a permanent-slot controller update was queued; it does
not claim a durable party or save receipt.
Independent observers can also inspect positive direct HP or Substitute loss
as each engine damage update occurs, including multi-hit moves. This is not a
complete hit stream or an indication that subsequent battle effects have finished.
An accuracy-check observer reports actual per-target check results, including
guaranteed and OHKO paths, without claiming every check used a random roll.
An innate-priority phase now applies usable entries in order using that same
ability assumption and the AI's cached item knowledge.

Shared speed calculations now expose a bounded pure battler proposal and a
per-innate speed trigger. Confirmed AI callers supply their own ability and
hold-effect assumptions; an unmarked calculation may still be an AI path.
The hooks provide building blocks and do not implement innate mechanics.

The shared ordinary damage calculation now exposes bounded effective Attack
and Defense proposals and ordered per-innate triggers. Context distinguishes
the stat owner and raw/stat-stage choice for unusual moves and Wonder Room;
fixed damage and automatic built-in innate effects remain outside these hooks.

Battle weather and terrain now have a copied live snapshot and a sampled
change notification, including weather suppression and timer counters. It is
a coalesced frame view rather than an attributed or atomic transition stream.

Committed battle-environment notifications now cover common weather and
terrain setters, expiry, selected clears and the main starting-condition paths.
They report supplied move or ability sources and distinguish starting statuses,
natural weather and world-seed proposals; other direct writes need coverage.
An opt-in timer event also reports non-expiring end-of-turn weather and terrain
countdowns individually; expiry remains in the existing committed stream.
A separate observer reports effective-weather suppression changes caused by
supported ability mutations, even without an ability-mutation subscription.
A separate event covers supported Neutralizing Gas changes, including its
switch and faint cleanup. Other sources still use the sampled view.
An additional presence event reports effective-weather suppression changes at
the switch-out, replacement-data and faint-animation boundaries.
Wild-actor capacity queries now expose current generated-encounter, object and
sprite usage for spawn planning without reserving resources.
The type service also exposes a player-facing battler type view that follows
indicator visibility, Illusion, Roost and Terastallization.
Bag medicine now reports active-battler and checked off-field party status
cures through the battle API, including item attribution. Both kinds of cure
can also queue checked reactions on active battlers after the item message.
Experimental owned-identity tracking now fails closed when a gift receipt
addresses a different slot or can no longer be committed safely.
The same destination and safe-commit checks now cover evolution, daycare,
fusion and catch receipts. Interrupted nonempty party reorders invalidate the
tracker as well. Permanent identity remains an internal, incomplete service.
Mods can also inspect a bounded, ordered census of colliding OT/personality
locations, including unreadable matches, without selecting an arbitrary one.
In-game trade receipts also verify the physical source and destination, and
invalidate experimental tracking if their commit becomes unsafe.
Battle Factory rental selection and swaps now invalidate tracking when they
replace an owned party slot outside a checked rental projection.
Aliased name-getter outputs now invalidate experimental tracking when they
write into owned storage; the read-only battle-summary mapping also rejects
overlapping raw writes even when the tracker is inactive.
The authored-event state service can also sample the saved dynamic warp and
its ordinary arrival coordinates without executing a warp or its scripts.
Overworld actor and recognized effect parts expose their live affine matrices
and optionally rasterized pixels for external renderers.
Billboard and combined shadow traces accept the full 128-pixel affine raster.

The music service can inspect MIDI source timing and loop markers without
starting playback; Game Boy and MP2K report their engine-managed loops without
claiming a decoded duration. Loop proposals remain a playback/preview concern.
It can also inspect the SoundFont preset selected for each used MIDI channel
and program, including substitutions, without changing playback.
An independent coverage view reports MIDI notes outside the selected preset's
key and velocity regions, including a bounded list of missing combinations.
Committed battle observers can also queue a checked, deferred music cue through
the existing selection pipeline and observe whether it played, was overridden,
fell back or became stale.

The existing item-EV reduction transaction now covers the field party menu's
dedicated berry and reset-item callbacks. It rejects a berry that would change
neither EVs nor friendship before consuming an item.
EV limit, battle-yield and item-yield callbacks now preserve gameplay RNG;
impure proposal changes are discarded while independent denials remain binding.
All owned-Pokémon edit variants now preserve both gameplay RNG streams across
validation and committed notifications; RNG-consuming validation is denied.

The world-scene service can now test a ray against presenter-positioned sprite
billboards with pixel alpha, supplying a reusable building block for sprite
shadow and occlusion handling. The presenter still decides its geometry,
visibility and how to combine billboard and scene intersections.
A directional-light query also reports how much of a ray was actually checked
before the loaded scene or height boundary, including a reusable endpoint.
A combined directional shadow query can compare that segment with up to 64
caller-positioned sprite parts and report the nearest scene or billboard hit.

Innate entry and turn-end callbacks can now queue normal weather and terrain
changes through engine rules and follow-up scripts. Sources are revalidated
and committed changes identify the innate. A separate opt-in single-battle
AI entry forecast evaluates scoped field changes after hazards; follow-up
abilities, terrain seeds and turn-end field changes are not simulated.
Callbacks can also query the projected field after offers made so far, while
denied offers are discarded before the next innate is evaluated.

Habitat observers can now detect coalesced natural-weather changes on the
loaded map without waiting for a player step. Saved weather changes remain a
separate event, and intermediate changes between safe samples can be missed.

The breeding service can opt into a save-backed ordinal for committed player
daycare eggs. It begins at activation, rewinds with the save and is not a
permanent individual ID; other egg-creation paths are not counted.

Modal panels can now publish bounded UTF-8 semantic text to an independent
accessibility adapter, with change notifications during the paused modal
loop. Live overworld overlays can use a separate semantic snapshot and event
stream. Existing panels and operating-system speech integration remain
separate work.

Committed ability-mutation notifications now cover selected engine move-script
operations and Trace, plus a separate stream for Mummy, Lingering Aroma and
Wandering Spirit, including both sides of a swap. Neutralizing Gas source
changes at supported engine boundaries report before/after battler views;
switch-in and faint cleanup also report an otherwise uncleared source without
duplicating an earlier removal. Switch-in cleanup may already contain the
replacement participant, so the source slot is not a departing identity.
Other mutation paths still rely on the coalesced observation stream.

Major-status applications through the engine's common setter now have a
committed observer event, including innate-queued applications. A separate
item-effect event reports additions and cures from orbs and curing berries.
Their observers can apply checked shared conditions through existing policy
hooks. Queued innate applications also expose the originating ability and
entry or turn trigger to those observers and nested condition policies. A
status observer or its condition-change observer can queue a bounded animation,
heal or unsourced stat change after the supported presentation, including
nested item presentations. Source-attributed ability-style stat changes and
major-status applications also recheck both participants at playback. Other
status reactions can queue indirect damage with normal faint and item handling.
They can also cure selected major statuses and observe committed cures from that
command, including nested reactions. The same cure stream reports immunity,
Hydration and Shed Skin cures with ability attribution, plus natural wakeups
and selected move-induced thaws with move attribution. Several inline move and
partner-ability cure scripts now report active battlers and play queued
reactions after their existing presentation. Other status mutation paths and
reaction action types remain open.

Owned-mon Tera views expose the engine's stored and resolved values and their
source, with a sampled change flag. A checked edit can change built-in stored
types atomically with stat edits and optional costs. Custom Tera storage and
the battle-scoped override remain separate concerns.

Move-resolution observers can compare observed HP updates with available
start and end HP, detecting unexplained net changes while flagging changed
participants and other ambiguous cases. Matching totals do not prove that
every HP mutation was observed.

The world-environment service now exposes coherent weather and terrain timing
views and can enumerate active saved effects across maps without simulating
unloaded areas or committing expiration during a query.

The wild-behavior service now lets guarded routes opt to consume steps on
arrival, exposes the current queue policy, and retains that option with saved
goals. Interrupted or deviated steps remain queued for inspection or replanning;
losing a step does not consume it, and removing an actor clears its queue.
Routes still do not reserve tiles or simulate unloaded maps.

The field-healing service supplies checked HP previews and commits for living
party Pokémon, using existing challenge constraints and a committed observer.
Other overworld terrain interactions remain open.
Version 1.1 also lets a healing transaction require the current map, saved
weather and terrain, or world step, rechecking them after constraints so an
expired effect cannot authorize a late heal.
Field poison hooks can opt into a copied player-habitat view, rechecked before
damage; heal, cure and poison observers preserve gameplay RNG.

The overworld service now separates culling, object and sprite invisibility,
wild concealment and native movement-callback status.
An opt-in photo request for a culled actor now succeeds only when the selected
presenter supplies a visible-subject mask for the captured frame. Scouting can
use that short-lived photo evidence once per action after checking the actor,
world and constraints.

## Battle lifecycle snapshots

Mods can observe committed battle start, initial and later entries, and battle
end with copied participant snapshots. Two-trainer doubles preserve the owner
and party-slot distinctions, and ambiguous facility or remote trainer IDs are
not reported as static trainer keys. The v1.1 service also reports selected
authored trainer-pool entries and explicit ace/lead tags after randomization,
with collision checks. A recorded-playback exit is distinguished from a
normal result. Boss-ace selections are saved and checked against their authored
rosters; durable player ace designation and gameplay battle-phase hooks remain open.

Usable innates can also trigger once per completed turn in battler order,
after condition timers tick. They can update shared conditions and queue
self-healing, stat changes and end-of-turn messages. Opt-in AI previews can
estimate constant or HP-dependent self-healing at this phase; changing field
conditions and targeted turn effects remain open.

Shared battle condition sets now pass through a typed proposal phase, where
independent modules can adjust durations or deny a change. A result-bearing
command reports rejection reasons; engine expiry retains its original timing.

A shared move-selection constraint applies to the player and AI move masks,
with a separate player-owned preview and a Struggle fallback. Sleep Talk's
called move remains independent; the standard menu displays valid mod denial
reasons with bounded wrapping and a generic fallback.

A scoped move PP-cost transform runs at actual deduction after Pressure. It
preserves the engine's zero-PP gate, copied-move handling and controller
updates. An opt-in AI estimate uses cached ability knowledge, flags uncertain
targets and called moves, and leaves older committed-only hooks unchanged.

For single-battle smart switch selection, a pure per-innate preview can offer
entry healing and stat changes to the candidate's post-hazard estimates. It leaves real
battle state unchanged and excludes opponent-switch prediction. Other entry
effects and switch-selection modes are not yet predicted. Opponent-directed
stat offers use cached AI knowledge and refresh both combatants' speed and
damage calculations; candidate simulations are restored between choices.
The survival simulation now applies hazards once, clamps lethal damage, and
handles a single-use healing item before the opponent's first attack.

## Music assignments

The music service can inspect bundled Game Boy, MP2K and MIDI styles as well
as declared or imported pairs, reporting their backend and current asset
availability without altering playback. The result is a point-in-time check.

Music selection, assignment constraints and committed music notifications now
restore both gameplay RNG streams. Impure selection proposals are discarded;
impure allowed assignment edits fail without committing, and denials remain
effective. This keeps these presentation callbacks from advancing gameplay RNG.

The save-backed music-assignment service resolves shared context, song, map
and authored-trainer rules before the final music-selection transforms. Edits
have revision checks, permission hooks and committed receipts, and missing
tracks fall back to the original music. The API supports registered MIDI/SF2
pairs and bundled battle styles. A host-managed import library now validates
and hashes MIDI/SF2 pairs, deduplicates shared files and supports re-import and
removal. Native choosers, keyboard input and modal preview services support
importing and auditioning tracks, and assignment services support battle
categories, areas and authored trainers.
The trainer catalog exposes readable labels and marks likely bosses by class;
rematches are selected separately.

## Modal mod screens

An external module can open a paused-overworld panel, handle rebound controls,
draw clipped shapes and game-font text, and perform validated edits from input
callbacks. Drawing callbacks cannot issue gameplay commands. Modules
can offer actions from the selected Pokémon's field party menu; a host chooser
composes multiple mods and refreshes the menu after closing. A semantic text
bridge is available to external accessibility adapters; operating-system speech
integration and a general Pokémon-changed event remain open.

## Paid Pokémon edits

The mon-edit service can stage up to eight distinct item costs and money with
an IV, EV or stat-nature edit. Constraints see a payment receipt; a veto, stale snapshot or
insufficient payment commits neither cost nor edit. The normal and Pyramid
bags, secret-base bookkeeping, party and PC Pokémon, Nuzlocke's last-ball
transition, save restoration and older module compatibility have real-engine
tests. Pricing policy and true nature changes remain open.

## Wild encounter cancellation

A separate generation-permission constraint can veto ordinary table, fishing
and ordinary overworld-actor candidates without changing the older replacement
hook's semantics. A cancellation notification carries the selected candidate
and first denial reason. The affected paths restore selection RNG, avoid
creating an enemy or consuming a creation ordinal, and distinguish actor
allocation failure from a veto. Fishing returns through its failed-attempt UI;
Sweet Scent does not start a battle, and a denied second wild Pokémon leaves
the first intact. An opt-in special hook now covers roamers, outbreaks and
special overworld Feebas, preserving saved data and RNG when denied. Allowed
roamer battles notify Nuzlocke at battle start. Separately opted-in DexNav
hooks cover its search-to-battle handoff while preserving the earlier hook
contracts; a veto ends the search through its normal got-away path. Separate
facility hooks cover the final Pike and Pyramid Pokémon, including generated
visible actors; provisional selector Pokémon no longer reach normal wild
hooks. Separate permission and notification events now cover manually placed
Pike and Pyramid actors at engagement, with rollback on denial. Separate
scripted and event-mon permission and cancellation events now cover guarded static
encounters, including the Regis, Rayquaza, Lugia, Ho-Oh, Groudon, Kyogre,
Mew, Deoxys, Southern Island's Lati, Mewtwo, Kecleon and several item-ball
Pokémon. The catching tutorials have a separate permission and cancellation
family. Wally's denial restores the player's party and leaves a retry
interaction; a fresh-process save/load test verifies the pending state and the
rebound interaction before an allowed retry. The old man's denial leaves the
tutorial and reward pending.
The Pokémon Tower ghost is checked after any Silph Scope replacement;
denial restores the prepared party and RNG and releases the scene for retry.
Mew returns to its chase and Deoxys's solved puzzle remains retryable
while the map remains loaded. It checks the prepared party, restores preparation RNG and enemy
party on denial, and exits through the script's matching unlock path. Other
story scripts and separately prepared legendary battles still need their own
safe cancellation paths. Separate v1.14 observers report when a denied global
script explicitly ends and unlocks controls; the earlier veto events retain
their original timing. Interrupted or replaced scripts do not produce a
cleanup-finished event. A version 1.6 observer reports actual
opponents at scripted and standard legendary battle starts, including both
members of scripted doubles. Separate v1.17 observers report the final ghost
Marowak and catching-tutorial opponents, including the Silph Scope replacement;
they carry annotated keys into their battle lifecycles. None of these ready
notifications permits vetoes or species replacement. A separate opt-in
v1.18 ready event observes Birch's first opponent after it is created, and its
authored source follows starter selection into the battle and outcome. A separate opt-in
v1.15 source identifies annotated authored encounters through candidate,
permission, cancellation, cleanup and battle-ready callbacks. Version 1.16
carries the key through the matching scripted battle's lifecycle and outcome;
an interrupted or replaced transition cannot attach it to another battle.
The key distinguishes authored sites, not battle instances. A separate opt-in
proposal can replace eligible species in `setwildbattle` operands before
creation, with authored level and held-item values preserved. A separate
proposal covers `seteventmon`, retaining fateful status. These proposals do
not themselves cancel battles; other scripted creation paths remain uncovered.

## Wild-actor perception

A read-only perception service relates player and wild-actor endpoints with
lifetime and individual identities. Bounded supercover sight can check scene
annotations, collision, actor occupancy, elevation, range and facing. Logical
concealment and sprite visibility are separate from physical sight. This is a
tile approximation. A bounded scouting log now records observations and
identifications of saved actor IDs through validated, composable commands.
Unidentified log records hide their species, although trusted mods can still
read actor snapshots. A photo-capture service delivers the actual software or
OpenGL-composited world frame for a visible actor, including a custom world
presenter, with invalidation if the scene changes. A bounded host archive keeps
eight full-resolution frames with the corresponding native save generations;
photo IDs reserve journaled run-scoped serials. A modal RGB image primitive
supports interfaces for these services.
Composable photo scoring provides a distance and cover estimate, image and
environment context, and generation-matched stored scores. A software-only
visibility service supplies the subject's on-screen silhouette and topmost
pixel mask for compatible captures. It measures layer ownership rather than
blended light. Records follow
ordinary save rollback; pixel-accurate clarity and rollback-safe rewards
remain open.

## Field environment moves

The environment service exposes supported moves in a party Pokémon's actual
move slots with a read-only usability snapshot. A selected OpenGL environment shader and
an ordered chain of module-provided post-processing passes run after a custom
presenter and before viewfinder overlays, and are captured in photos. The host
protects interface pixels.
Software falls back to the original frame. Geometry, depth and environmental
interaction rules remain open.

## Scene geometry views

The world-scene service can construct bounded textured quads from resolved
ground, wall, roof and billboard rules, including edges between differing
neighbor heights. The service exposes recognized field-effect sprite frames
and parts. A read-only lighting-input view supplies map context, time and weather. Visible engine
lamps and signs are available through a read-only source view.
Modules can declare bounded map-local lights with color, radius, height,
intensity and time conditions; connected-map attribution and offscreen
influence are supported. Other field effects, inferred artwork semantics and
complete HD-2D rendering remain open. A scoped render hook collects bounded
dynamic lights, and a scene trace checks cast-shadow geometry and transparent
tile pixels.

## Existing type chart

The type service exposes stable identities and copied species and battler type
lists for the existing engine types. A bounded chart transform participates in
combat, AI and hazard lookups before inverse-battle conversion, with native
handling for changed Ground/Flying matchups and separate airborne effects.
Link and recorded battles retain the original chart. A bounded STAB transform
uses the engine's ordinary or Terastallized result as its starting point and
also applies to AI damage estimates. Species rules can initialize up to three
existing types in battle, with shared queries, summary icons, Pokédex type
searches, entry hazards and switch AI matchups. Roost removes Flying from any
active slot. The optional HGSS Pokédex and DexNav display all three species
types. AI Illusion simulations restore all three battler slots, and both the
effectiveness check for a disguise and Sky Battle eligibility use resolved
species types. Encounter-attracting abilities, type-based evolution conditions
and the Pike, randomized Pyramid, Factory and Dome type rules also use them.
Contest Curse animations use resolved type lists, including overflow types.
Custom type definitions, deterministic catalog enumeration and keyed chart
queries are available. Keyed composition handles long lists, duplicates,
inverse matchups and bounded results. The battle chart resolves internal,
temporary custom-type slots. Species proposals can assign a registered custom
type within those slots. Battle-scoped Tera type rules can resolve to registered
custom types, including for Tera Blast and AI, without writing runtime slots to
a save. Version 1.10 can expose a uniquely matched owned battler during Tera queries,
allowing mods to persist their own stable-key choices and fall back if a type
is missing. The OT/personality identity is not permanent; a general host-managed
assignment store, custom Tera form changes, full-length presentation and
expanded type slots remain open.
The native type icon paths generate color-coded badges for internal custom
slots, with shared, reclaimable graphics and a fallback when resources are full.
The Dome's party-type diversity and the Factory's type-frequency calculation
now cover all byte-sized slots. Factory dialogue uses the registered name if
a custom type is the unique leader. Conversion 2 can select registered custom
resistances, including chart-hook changes, while retaining the original random
sequence when no custom candidates are available.
When enabled, native battle type indicators show up to three distinct types,
reflecting Roost, Terastallization and Illusion.

## Ecology time view

The ecology tag registry lets modules declare stable named species traits and
contribute associations that other modules can query during encounter or
habitat decisions. It validates references before activation, deduplicates
associations across aliases and mods, and returns results in stable ID order.
The wild-trait query applies composable per-actor transforms to those defaults,
with a stable individual identity when available and explicit session-only
fallback otherwise. A bounded saved override record allows owned, revisioned
trait edits for identified actors and is cleared when they leave. These queries
do not automatically change wild behavior. A step-based proposal can direct
identified nearby actors through existing movement and engagement commands;
broader profiles, autonomous schedules and offscreen simulation remain open.
Wild profile fields can also be changed atomically in batches under one
revision, with all existing field constraints checked before commit. Optional
signed and Boolean field schemas provide checked ranges, defaults and labels;
conflicting declarations reject edits without discarding saved values. Opt-in
providers can list those fields with bounded, conflict-checked discovery.

The world-time service exposes a copied sample of the game's real or fake RTC,
saved offset and effective scripted day period. Habitat and encounter proposal
callbacks share a sample per proposal, while independent calls follow the game
clock. The service does not add an ecology scheduler or make wall time
replay-deterministic. Real-engine and independently compiled module tests cover
period boundaries, clock offsets and overrides, nested proposal consistency,
and unavailable or malformed inputs. Tests clean their build artifacts.

## Wild actor concealment

The wild-concealment service gives mods owned, revision-checked hidden and
engine-ground-cover policies, optional movement or proximity reveals, proposal
constraints and committed-change notifications. The engine applies hiding
independently of script visibility and uses its existing ground effects. Actor
state survives a save and reload; unavailable owners do not strand hidden
actors. Real-engine checks cover policy previews, vetoes, ownership conflicts,
movement, proximity, sprite visibility, ground-effect filtering and fresh
save restoration. These are building blocks for the hypothetical ecology mods;
they do not implement those mods or a general occlusion renderer.

Implemented the requested API and eight default-off examples. Platform checks,
independent Windows reproduction and standalone archive verification passed.

## Public API and independent examples

`include/mod_api.h` defines the versioned C ABI, stable content identities,
read-only metadata, typed settings, deterministic RNG and hook composition,
portable private state, transactional migrations, and generic durable journals.
`MODDING.md` documents ownership, event contexts, save compatibility and traces.

Each requested option is an independent module in `native/mods/`, using only the
public API and standard C headers: Nuzlocke, wild randomizer, trainer randomizer,
starter selection, 600 BST, next-Gym/Champion level cap, followers and battle
music. Each has its own ID, descriptors and state. All eight build as standalone
DLLs/SOs and can replace the bundled copies with `--external-bundled-mods`.
The engine owns the original menus, save container, content metadata, Pokémon
objects and gameplay hooks. It owns run identity in `rhh:host_run`.

The original New Game menu opens the rules screen, defaulting every option off.
Pool mode gives stable area tables; Full mode advances only when a wild Pokémon
is actually created. Starter selection includes creatable non-legendary,
non-mythical species and permanent forms. Equalization totals exactly 600 while
preserving proportions within stat bounds and Shedinja's 1 HP. Caps use the
actual next Gym or Champion party. Followers use RHH's existing implementation.

Nuzlocke begins when Poké Balls are obtained. Each eligible first encounter per
named area consumes its attempt, including fleeing or failure. Owned Pokémon
remain dead through healing, revival, PC conversion and reload. Wipeout ends
the run. Versioned per-module journals preserve these facts across ordinary
reloads, including unsaved deaths. A persistence error locks the challenge.

## Persistence and migration verification

Native container v1 loads with all example settings off. Container v2 retains
atomic game/mod snapshots and corresponding generations, two run journals, and
a persisted run serial. Aborted writes, failed atomic commits, corrupt data and
missing required modules leave existing files untouched. Ordinary RHH game data
still requires the matching engine architecture/layout; mod schemas do not
migrate arbitrary upstream game saves.

Strict host tests cover registration-order independence, dependency cycles,
sticky constraints, stable content rebinding, deterministic RNG, transactional
state/configuration changes, New Game setting locks, runtime settings, durable
merging and persistence failure notifications. Native save tests cover legacy
upgrade, fresh-process reload, generation recovery, rollback and missing modules.

Real-engine tests cover all enabled species at 600 BST, actual randomized trainer
parties, all Gym/Champion caps, enemy-level independence, candy refusal, 32-draw
Full save/reload tapes, and sixteen actual wild creations. They exercise real
Nuzlocke healing/item/PC paths and RHH save/journal recovery. Example-specific
migration checks preserve the wild ordinal while adding its roster and expand
the Nuzlocke area/death ledgers; malformed old schemas are rejected.

## Music and original menus

The Gen I/II styles play the original Red/Gold GBS music through Game Music
Emu's Game Boy engine, and Gen III uses Emerald's own sequences and
voicegroups. Gen IV/V use converted Diamond and Black/White sequences with
their matching instrument banks. The manifest also retains the earlier MIDI
arrangements and GM SoundFonts for custom music. URLs, hashes and credits are
in the manifest and `native/THIRD_PARTY.md`. TinySoundFont/TinyMidiLoader are
pinned at `853a0a171759f1ddba0de1442133a75912bbeffa` with their MIT/Zlib notices.

Linux64 tests cover all twenty styles, including the eight Game Boy tracks
against direct engine output and long-running playback. Real SFX-player
advancement and fade-to-silence are checked for each replacement track. The MIDI
mix uses a separate delivery buffer so it does not feed back into MP2K reverb.
Original battle audio remains the default. The actual Options screen was
inspected; selecting Gen II, saving and fresh-process Continue retain it.

## Verified host-platform evidence

* Linux64 all-off input replay: `build/native/replays/linux64-iml9veps/`.
* Linux64 external examples with all options enabled, real starter battle,
  follower, repeated Save and built-in Continue:
  `build/native/replays/linux64-qy3x09pe/`.
* Windows64 external/built-in enabled replay, including Options and another
  fresh Continue: `build/native/replays/windows64-it3hah3d/`.
* Windows64 and Windows32 production subsystem/API/save suites passed.
* Linux64 strict API, real-engine migration/mechanics and external SO tests
  passed. The sanitizer subsystem, mechanics and all-twenty-music tests passed.
* The original ARM `modern` build passed after the integration.

The sanitizer identified fixed-length string reads past short rules-screen
labels and an existing short default-player-name string. Both now stop at their
actual terminators. The full sanitizer replay passed at
`build/native/replays/linux64-on4l5861/`, including the rules screen, naming,
battle, follower, Options, repeated Save and fresh Continue. Linux32 passed
at `build/native/replays/linux32-tacxgnd0/`; Windows32 passed at
`build/native/replays/windows32-exjuujg7/`. Both used external examples for
writes and built-in copies for Continue, including the music change.

A real Windows WASAPI device opened with Gen I MIDI/SoundFont music active
inside Birch's battle (`/tmp/rhh-wasapi-mods.json`). It generated 22,432,000
samples and queued 1,626,320 samples to the device. All twenty music renders
also passed AddressSanitizer. The native music and example sources have no
runtime network dependency.

Two independent clean Windows source exports produced identical executable,
SDL2 DLL and all eight example DLLs. The report is
`build/native/reproducible/windows64-ri9_g38j/result.json`; the executable's
SHA-256 is `5f9d6296068baddd14dd85e894eb732b48784a613444407ffb94f48b3e707703`.
`build/native/evidence/mods/verification.json` links the platform evidence and
archives the successful regression logs.

The Windows64 archive was extracted outside the repository. Every packaged
hash was verified, including all eight external DLLs and their source files.
The included public-API-only additional cap example compiled from those files.
The extracted executable loaded it alongside the eight external examples,
played the packaged Gen I battle MIDI/SoundFont, won Birch's battle, saved,
and preserved the party/configuration in a fresh process using the built-in
examples. Its level-50 constraint composed with the Gym cap by minimum (15).
The final archive is `build/native/dist/pokeemerald-expansion-windows64.zip`;
it includes the public header, all example sources, build instructions,
credits, music assets and reproducible build information.

These checks cover the opening gameplay replay and the targeted mechanics/API
regressions. Broad endgame and minigame coverage, physical GBA link replacement,
raw GBA save import and arbitrary upstream engine-save migration remain outside
this implementation's verified scope, as documented in `PORTING.md`.


## Persistent battle environment shaders

The ninth bundled module, `rhh:battle_environment`, now selects a persistent
GLSL pass from live battle weather and terrain. It defaults off for new runs
and older saves, is enabled with BATTLE FX in Options, and is also built as an
external library. The append-only shader declaration API accepts existing v1
definitions that end before the extension.

The native presenter loads OpenGL through SDL and renders the shader at
240×160 before scaling. The compositor supplies a mask for battle text,
health bars, ability popups and other UI. All weather types and four terrains
can be combined; replacement, clearing, weather suppression, menu transitions
and battle exit update the rendered state directly. Palette fades also apply
to the effect. Screenshots include the shader output.

Only the recurring weather message and animation are skipped when the chosen
shader compiles successfully. The same script tail still processes weather
abilities. Weather expiry, start/end messages, damage and healing retain their
existing paths. Missing OpenGL, disabled settings and shader compilation
failures retain the original recap.

Verification completed for this change:

* Linux64 and Windows64 native executable and external-module builds passed.
* The original ARM `modern` build passed.
* The Linux64 subsystem/API/save suite passed, including shader declaration
  validation and a short legacy ABI allocation. AddressSanitizer passed with
  leak detection disabled because LeakSanitizer cannot run under this
  environment's tracing.
* Real-engine environment tests passed with Mesa llvmpipe and with SDL's dummy
  driver: all semantic mappings, actual GLSL output, persistent motion,
  combined effects, UI masking, weather expiry, suppression, double-battle
  state, fades, menus, battle exit, settings persistence and deliberately
  broken shader fallback.
* Built-in and external real-engine mechanics/save tests passed.
* The full external-module gameplay replay passed at
  `build/native/replays/linux64-rud8h9nh/`, including the starter battle,
  bag/party/summary, Options and repeated fresh-process Save/Continue. The
  `environment-options` and `environment-continue` phases also enabled BATTLE
  FX through the actual menu and verified the saved choice in a fresh process.

Controlled previews over a scene captured from the running game are at
`build/native/evidence/battle-environment/index.html`. The gallery reads back
actual GLSL output for eighteen combinations, with 80 frames at 20 fps for
each. The source is a clear in-game grass battle after the send-out flash,
with Pokémon, health bars and the action menu visible. Individual GIFs and
animated weather, terrain and combined comparison sheets loop for four
seconds. Every rendered frame checks protected UI pixels and suppressed rain
against terrain alone. GdkPixbuf independently decoded all GIF frames,
verified motion and confirmed protected UI colors remained exact; evidence is
in `gif-verification.json` beside the gallery. Rebuild its runner with
`make -f Makefile.native environment-gallery`; its sources are
`native/tests/environment_gallery.c`, `native/tests/environment_gallery.py`
and the dependency-free `native/tests/gif_encode.py`.

Terrain covers the visible battle floor across the full field, fading near the
horizon, and is masked behind sprite pixels. Weather remains visible over
sprites. All weather and terrain effects use layered animated lighting and
were regenerated as GIFs over the captured battle scene. Every terrain frame
was checked against the original sprite pixels.

The final Options layout uses nine 13-pixel rows. Frame graphics start after
the complete options window tile allocation, preventing the last row from
overwriting the border. The actual settings screen was inspected, and enabling
BATTLE FX survived an ordinary Save and fresh-process Continue. The shader
unit suite also passed in the Windows64 build with real OpenGL and fallback.

The final shader suite passed on Windows x86 and x64 with real OpenGL, and
with AddressSanitizer on Linux64. Two clean Windows64 builds matched for the
executable, SDL2 and all nine example DLLs; the report is
`build/native/reproducible/windows64-_zzzjgfk/result.json`. The current
executable SHA-256 is `d856770aec9a2e149578e8bb92ff27d031932cc1e2e0ab832630e937da1da575`.

## Shared API extensions in progress

The native host now exposes versioned service discovery while preserving the
original v1 host prefix. `rhh:service/owned_mon` returns copied party and PC
snapshots, with IVs, EVs, nature and limits. Lookup by the existing identity
rejects duplicates. `rhh:service/overworld` provides copied map, tile and
actor views and RGBA copies of current map tiles and object sprite frames.
`rhh:service/daycare` exposes copied parents, items, moves, egg groups,
pending-egg status and the engine's current pair compatibility without
generating an egg or advancing RNG. Modules can declare required and optional
service versions; the host rejects missing requirements before activation. The
snapshot services are read-only.
The mon-edit service now previews and atomically commits validated IV, EV and
stat-nature allocations, with typed constraint/notification context, optional
item and money costs, and a safe overworld update callback. True-nature editing, earned-EV accounting
and automatic save transactions remain open.

`rhh:service/world_clock` provides a saved 64-bit count of accepted,
non-forced player tile steps and a committed step event with copied map and
position data. Old host-run schema 1 saves migrate with zero steps while
preserving the original run data. The clock is run-scoped, counts steps on all
maps and rolls back with the normal save; it is not a wall clock or an
environment timer. Linux and Windows 32- and 64-bit tests cover migration,
rollback, high ordinals and saturation. An external mod observed steps on a
real map, and replay checks exercised normal and accelerated movement and
fresh-process reload. Temporary test builds and replay artifacts were removed.

`rhh:service/world_environment` adds save-backed map weather and terrain
transactions driven by eligible moves from the active party. It supplies
previews, constraints, committed notifications, deterministic replacement and
step expiry. Ordinary battles can inherit the active effects while retaining
the engine's natural-weather and trainer-status precedence. This does not yet
add a field-move menu, overworld effects renderer or terrain interactions.

Version 1.1 of that service adds a typed battle-seed context and proposal
hook. Mods can transform or deny each eligible battle's initial weather and
terrain without changing the stored overworld state; scripted conditions,
trainer statuses and excluded battle modes retain precedence. The real
battle path also handles omitted animation arguments as zero, avoiding a
native null-pointer crash when weather or terrain starts.

The habitat service combines copied map tiles and semantic habitat flags with
natural weather and saved environment snapshots. Modules can compose or deny
pure habitat queries, with deterministic ordering and safe invalid-value
fallback. This supplies shared inputs for ecology without spawning actors or
changing the engine's movement and encounter rules.

The wild-encounter service now supplies a typed, habitat-aware candidate
proposal after slot selection and randomization, followed by a committed
Pokémon-generation event. It uses the actual spawn tile for generated
overworld encounters and the facing tile for fishing. This remains distinct
from actor allocation, durable individual identity and encounter cancellation.
Version 1.1 also allows habitat-aware relative slot weights for ordinary land,
water, rock-smash and fishing draws, with original behavior and RNG preserved
for no-op or unusable proposals. Lead abilities and lures retain their engine
ordering; zero weights are not a general encounter veto.

The wild-actor service now reports committed overworld actor creation,
restoration and removal with copied snapshots and process-local handles.
Ordinary generated actors retain their actual Pokémon through native saves
and into battle. The service can request a table-based or specified-species
spawn at a tile or remove a live actor using engine rules. A requested species
uses the normal species and candidate transforms and encounter veto, and its
actual generated Pokémon is retained for battle. Mods can also request an animated,
collision-checked step or queue an adjacent interaction; battle setup and its
eventual outcome report the engaged actor. Run-scoped IDs survive actor
save/restore and native save-slot rollback. Concealment, per-actor profiles and
step-driven activity have separate bounded services. Manual, roamer and outbreak encounters retain
their existing Pokémon-generation behavior. Automatic overworld encounters
remain opt-in.

The photo reward service now records first-photo item rewards durably, then
delivers them to the normal bag using per-save delivery state. Pending grants
are retried after rollback or when bag space becomes available; a saved and
consumed reward is not granted again. Version 1.1 can explicitly reward an
unscored album entry or the latest unarchived capture when the minimum score is
zero. Receipts retain whether scoring was available; the original API still
requires a score. A durable actor ID remains required. Experience rewards
remain open. The owned-mon service now supplies copied stored-experience and
growth-threshold views as a prerequisite for reward previews; it does not
perform experience awards or level-up processing. The coalesced owned-mon
change stream also reports stored experience changes, including within a level.
An observer-scoped context copies both sampled experience values, including
for additions and removals, without exposing engine pointers.
Fixed, already-scaled experience awards can be previewed against the current
hard level cap and maximum level; no award or level-up side effects are applied.

The host and dynamic loader now support up to 128 registered modules rather
than 32, including host-owned modules. Tests exercise dependency ordering and
transactional save import at the new limit, and load 40 separate native
libraries. Existing hook and aggregate save-size limits still apply.

The battle EV yield service reports the pre-cap yield and the engine's
allocation and can deny direct allocation before a committed notification.
An explicit bank mode distinguishes accepted banked points from rewards denied
by another constraint. It supports building an earned-points policy without hiding yields at the
current cap. A field item EV transaction likewise allows positive single-stat
items to be applied or consumed for points recorded by a mod. The host does not
yet implement the bank or its spending policy. Field use of EV-reducing berries
and the reset item now has a separate veto and committed receipt; other item use
paths are unchanged.
The EV limit service also supports per-stat and total policy limits for edits,
ordinary battle gains and EV items within the existing byte-sized fields.

The native save layer now supports multiple named host archives per generation,
while continuing to read and write the original photo-only format. Generation
restore rolls back mod state and archives when an archive import fails. This is
infrastructure for further host-managed records, not a Pokémon identity system.
An internal owned-identity ledger now stores run-scoped 128-bit slot bindings
in the same save generations, with atomic simultaneous edits, duplicate-ID and
stale-revision checks, and rejection of saves while tracked cursor, group or
trade slots are occupied. An internal allocator obtains 128-bit IDs from the
operating system and checks active bindings for collisions, making accidental
reuse after rollback overwhelmingly unlikely. An experimental, explicitly
started tracker can atomically seed existing occupied slots and apply PC,
party-reorder, daycare, fusion, scripted-gift, catch, daycare-egg, hatch,
evolution, debug PC fills, Mystery Event gifts, and in-game and link trade transactions even without
mod observers. Debug fills emit a separate receipt stream and allocate IDs per
slot, including the fast-fill mode's identical copies.
Mystery Event gifts receive an ID before the engine compacts the party; their
separate creation receipt precedes the corresponding reorder event.
Trade acquisitions receive new IDs; PC trades keep their incoming ID staged
until the evolved Pokémon is synchronized, and link trades retire the outgoing
ID only as part of the confirmed save. Failed or abandoned trades invalidate
the tracker. Activation and save boundaries check ledger occupancy against
party, boxes, daycare and fusion storage; changes during a staged save cause
rollback. Replacing the archive deactivates the tracker. While active, a
per-ID OT/personality change detector is checked on readable movement receipts
and saves, with hatch updating it after the verified transition. Safe native
frame boundaries also check current occupancy and detectors before modal and
overlay input or the overworld mod update, invalidating tracking on a mismatch.
Battle, storage, trade, hatch and save intermediates defer this check. Its separate
archive is bound to the ledger's run, revision and full ID set in each saved
generation; older ledgers without detectors cannot start the tracker. It detects
some occupied-slot replacements across reloads, but collisions and identical
copies remain ambiguous, and transient changes between checks can be missed. Supported battle
menu permutations are checked on return to field order. An inconsistent or
unsupported transaction invalidates its ledger rather than guessing; if this
happens during a staged save, that save is rolled back. It is not exposed to
mods or enabled for normal gameplay: other battle and creation paths,
automatic initialization and complete provenance checks remain necessary
before the bindings can be trusted as Pokémon identities.

Direct `CreateMon` and `CreateBoxMon` calls into tracked owned storage now
invalidate tracking even when the generated Pokémon shares the previous
OT/personality or the destination was empty. Creation into a saved party during
a tracked sky or Union projection is also caught; temporary and opponent
creation is unaffected. If a save is in progress, it is rolled back. The
same applies to bulk clearing an occupied player party, while clearing an empty
party preserves tracking. The disposable `native/tests/owned_identity_audit.py`
runner covers these paths and removes its test builds.
The shared `CopyMon` entry point also invalidates on unreported writes into
owned slots, including partial or identical copies, and rolls back a staged
save. Existing receipt-backed gift, catch, fusion, split-evolution and trade
paths use checked copies; self-copies and non-owned destinations remain
available. A PC trade checks the staged Pokémon and provisional destination
before its final copy, preventing a changed destination from being silently
overwritten before validation. Raw assignments and other write paths still require provenance
coverage before the tracker can become a public identity service.
The common setters and rekey helpers now also fail closed immediately when an
unreported write changes personality, OT ID or an owned slot's occupied status.
Unchanged values are allowed, and the checked hatch operation can rekey its
own Pokémon's OT before updating its witness. This catches these changes even
when they are reversed before a frame audit; it does not cover raw writes or
turn the witness into a permanent identity.

The owned-mon service reports PC pickups as well as committed placements,
shifts and group moves, with source and destination coordinates and copied
Pokémon where readable. Pickups identify cleared slots before they can be reused;
separate incomplete receipts preserve placement and release coordinates when
the held Pokémon cannot be decoded. This is not yet a complete movement stream
or a durable individual identity.
Party-menu swaps outside battle and party compaction now also report a single
copied mapping of all moved slots, including occupied unreadable slots.
Daycare deposits, withdrawals and internal shifts report ordered source and
destination transfers before any following compaction or shift, including
unreadable occupied Pokémon. Battle and other movement paths remain open.
Fusion and unfusion also emit committed slot transfers with before and after
views of the primary Pokémon, including the underlying PC fallback. Battle,
trade and other movement paths remain open.
Battle party menus temporarily rearrange the physical party array; owned-party
queries and sampled invalidations now defer until field order returns, and a
service status lets mods detect the temporary view. A checked slot permutation
now maps supported battle-menu slots back to their field positions without
relying on OT/personality; borrowed and unsupported parties fail closed.
The owned-mon service's v1.20 sky-battle projection maps selected Pokémon back
to their original field slots, including supported battle-menu permutations.
Normal owned-party queries and change sampling defer until restoration. The
experimental identity tracker preserves original IDs across the reduction and
checks both selected Pokémon and the saved full party before restoring; a
configured sky battle is still required to use the engine feature.
Version 1.21 also maps the selected player slots in a reduced follower-partner
battle and defers full-party queries until restoration. Version 1.22 extends
that mapping through supported mixed menus and player-only switches while
excluding partner slots. The experimental tracker checks the selection and
saved party during the merge. Version 1.23 reconciles catches through the full
party, reports the full-party selection menu's field slots, and restores the
reduced battle party. Unsupported menu layouts and cross-owner moves still
invalidate the tracker. NPC followers are disabled in the default config.
`python3 native/tests/follower_enabled.py` builds an isolated configuration
with followers, wild partner battles and half parties enabled. It exercises the
real follower preparation, catch reconciliation and restoration entry points,
including an independently compiled receipt observer. Its temporary build,
test saves and observer are removed on completion or failure.
The wild-catch flow now reports committed party-to-PC displacement and
subsequent catch placement separately, including the change from wild OT to
player OT. Catch placement now rekeys encrypted Pokémon data when assigning
the player OT and preserves the caught Pokémon's shiny state.
Scripted gifts and non-battle owner-assigned acquisitions, including gift eggs,
report their final slot and any replaced occupant, even if that occupant is
unreadable. Other creation paths and durable identities remain open.
Evolution reports committed species changes and split-created Pokémon; PC trade
evolutions identify the staged Pokémon and its pending PC destination. Link
trade evolution notifications precede the trade save and are not durable.
In-game trades report the outgoing and received Pokémon at the exchange,
including PC slots, and defer broad queries while a PC Pokémon is held. The
cable animation now uses the selected Pokémon rather than the trade ID, and a
received PC Pokémon is synchronized after trade evolution. Link trades capture
the exchange, check the handshake, and notify only after the game save succeeds;
durable observers journal separately and non-durable observers need a later
save. A bounded host archive now records the trade in that same save generation,
so consumers can reconcile a notification missed by a crash. Its serials are
save-lineage scoped and can be reused after rollback. Content-linked
checkpoints now detect replaced records, older generations and retention gaps;
legacy histories migrate without recovering their already-discarded prefix.

The live daycare now offers compatibility and offspring-species proposals and
a committed egg-created notification, with copied parent and inheritance-role
context through `rhh:service/breeding`. An incompatible pair can be allowed,
invalid offspring proposals fall back to the original species, and a full
party cannot be overwritten. Engine tests cover repeated pure compatibility
queries, daycare step generation, same-gender and two-Ditto pairs, invalid
offspring, party capacity and the committed result. The breeding
service is now version 1.1: mods can select each IV's parent or initial value
and propose parent-known, offspring-learnable moves as a validated moveset.
The original engine results remain the default, including its RNG draws; a
version 1.0 module and short context buffer remain compatible. Linux and
Windows 32- and 64-bit real-engine tests and an AddressSanitizer run cover the
new hooks, including external modules, invalid moves, party order and the
unchanged default RNG tape. `rhh:service/egg_hatch` exposes before/after
snapshots during the committed hatch event, preserving the transition when
the engine assigns a new trainer ID. Hatching now re-encrypts the Pokémon
with the new OT key, so a traded egg retains its valid data. Durable egg
identity and rollback-safe notification delivery remain unimplemented.

Breeding service 1.9 models nature inheritance from the engine's configured
Everstone rules and parent natures. It exposes source and per-nature weights
without rolling an egg.
The model assumes uniform independent draws; true-nature editing remains open.
Breeding service 1.10 also accepts a deterministic nature proposal for new
eggs and applies it to the same odds model. Denied or invalid proposals keep
the engine choice; existing Pokémon's true-nature editing remains open.

The overworld service is now version 1.1. It additionally exposes the current
map-to-screen camera projection, actor ground and visual anchors, and the
individual OAM/subsprite components and copied RGBA pixels. The ground anchor
is independent of temporary sprite offsets, while each component preserves
position, flips and priority. Affine transforms, blending and mosaic are
identified but not applied to the copies. Version 1.0 clients retain the same
service prefix. An external test module checks the player anchor and pixel data
in a loaded map and verifies smooth map-space motion while the camera scrolls.
The independent module was also loaded into the Windows64 build under Wine;
the Linux and Windows gameplay tests exercise the new views and old-version
service discovery. This does not establish the missing 3D presenter or effects.

The version 1.0 `rhh:service/world_scene` now combines the existing map-tile
view with declarative scene rules. Modules can annotate ground and upper
surfaces, pixel heights, shadows and occlusion by map, metatile, behavior or
map position. Priority, match specificity and stable rule ID resolve
conflicts independently of registration order; aliases refer to the same map.
The host validates declarations and retains support for older music ABI
definitions. Real-engine tests cover default and explicit annotations,
out-of-bounds/battle rejection, unchanged RNG and map data, on Linux64 and
Windows64. A separately built Windows DLL also queried its scene rule in a
running map. The rules describe geometry for a presenter; they do not create
meshes, shadows or an HD-2D rendering pipeline.

An OpenGL overworld presenter hook lets a module render a custom framebuffer
and use those snapshots; protected BG0 UI is composed by the host. It is a
foundation, not the full API-gap implementation. The HD-2D geometry generation,
built-in billboard presentation, shadows, depth of field and tilt-shift
pipeline remain unimplemented. Actor transactions, other Pokémon transactions,
field weather, ecology, photography, extensible battle content, editing UI,
breeding, ace events, host-managed music import and the corresponding mod composition
contracts also remain open. See [Still-needed mod API services](NEEDED_MOD_API_SERVICES.md) for their requirements.

Host tests cover service discovery and requirements, version mismatch, duplicate service and
presenter IDs, shutdown, and definitions from both earlier v1 layouts. Real
engine tests cover party/PC/daycare snapshots, atomic stat edit previews and
commits, duplicate identities, malformed Pokémon,
map/tile/actor snapshots, 4/8-bpp sprite decoding and 4-bpp map tile decoding. A test presenter
was exercised with Linux and Windows OpenGL; its output preserved protected
BG0 pixels and its declined frame matched the original. An external test
module also drew through the hook in the running overworld; its declined frame
matched a no-module screenshot. These checks do not establish the missing
HD-2D effects or the other unfinished APIs.
