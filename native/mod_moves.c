/* Runtime move definitions are reconstructed from stable saved keys. */
#include <string.h>
#include <stdlib.h>
#include "global.h"
#include "main.h"
#include "battle.h"
#include "move.h"
#include "mod_host.h"
#include "pokemon.h"
#include "constants/characters.h"

struct RuntimeMove {
    RhhContentKey key;
    struct MoveInfo info;
    u8 name[13], description[128];
};
static struct RuntimeMove sMoves[RHH_RUNTIME_MOVE_COUNT];

enum { MAX_MOVE_EFFECTS = 7 };
struct MoveEffectsCache {
    RhhContentKey key;
    u64 run;
    const struct MoveInfo *base;
    struct MoveInfo info;
    struct AdditionalEffect effects[MAX_MOVE_EFFECTS];
    bool32 changed;
};
struct MoveEffectsCollector {
    u32 count, capacity;
    struct AdditionalEffect effects[MAX_MOVE_EFFECTS];
};
static struct MoveEffectsCache *sEffectCaches[MOVES_COUNT_ALL + RHH_RUNTIME_MOVE_COUNT];
static struct MoveEffectsCollector *sEffectCollector;
static const u8 sAdditionalKinds[] = {
    [RHH_ADDITIONAL_SLEEP] = MOVE_EFFECT_SLEEP, [RHH_ADDITIONAL_POISON] = MOVE_EFFECT_POISON,
    [RHH_ADDITIONAL_BURN] = MOVE_EFFECT_BURN, [RHH_ADDITIONAL_FREEZE] = MOVE_EFFECT_FREEZE,
    [RHH_ADDITIONAL_PARALYSIS] = MOVE_EFFECT_PARALYSIS, [RHH_ADDITIONAL_TOXIC] = MOVE_EFFECT_TOXIC,
    [RHH_ADDITIONAL_CONFUSION] = MOVE_EFFECT_CONFUSION, [RHH_ADDITIONAL_FLINCH] = MOVE_EFFECT_FLINCH,
    [RHH_ADDITIONAL_STAT_RAISE] = MOVE_EFFECT_STAT_PLUS, [RHH_ADDITIONAL_STAT_LOWER] = MOVE_EFFECT_STAT_MINUS,
    [RHH_ADDITIONAL_SUN] = MOVE_EFFECT_SUN, [RHH_ADDITIONAL_RAIN] = MOVE_EFFECT_RAIN,
    [RHH_ADDITIONAL_SAND] = MOVE_EFFECT_SANDSTORM, [RHH_ADDITIONAL_ICE_WEATHER] = MOVE_EFFECT_HAIL,
    [RHH_ADDITIONAL_MISTY] = MOVE_EFFECT_MISTY_TERRAIN, [RHH_ADDITIONAL_GRASSY] = MOVE_EFFECT_GRASSY_TERRAIN,
    [RHH_ADDITIONAL_ELECTRIC] = MOVE_EFFECT_ELECTRIC_TERRAIN, [RHH_ADDITIONAL_PSYCHIC] = MOVE_EFFECT_PSYCHIC_TERRAIN
};
int ModsOfferMoveAdditionalEffect(const struct RhhMoveAdditionalEffect *offer)
{
    if (!sEffectCollector || RhhModsCurrentEvent() != RHH_MOVE_ADDITIONAL_EFFECTS
        || RhhModsCurrentStage() != RHH_CONSTRAINT) return RHH_ADDITIONAL_UNAVAILABLE;
    if (!offer || offer->struct_size < sizeof(*offer) || offer->reserved[0] || offer->reserved[1]
        || !offer->kind || offer->kind >= ARRAY_COUNT(sAdditionalKinds) || !sAdditionalKinds[offer->kind]
        || offer->target > RHH_ADDITIONAL_USER || !offer->chance || offer->chance > 100) return RHH_ADDITIONAL_INVALID;
    bool32 stat = offer->kind == RHH_ADDITIONAL_STAT_RAISE || offer->kind == RHH_ADDITIONAL_STAT_LOWER;
    if ((stat && (offer->stat < RHH_STAT_ATTACK || offer->stat > RHH_ADDITIONAL_EVASION
                  || !offer->stages || offer->stages > 6))
        || (!stat && (offer->stat || offer->stages || offer->target != RHH_ADDITIONAL_TARGET))) return RHH_ADDITIONAL_INVALID;
    if (sEffectCollector->count >= sEffectCollector->capacity) return RHH_ADDITIONAL_FULL;
    struct AdditionalEffect effect = {0};
    effect.moveEffect = sAdditionalKinds[offer->kind]; effect.self = offer->target == RHH_ADDITIONAL_USER;
    effect.chance = offer->chance;
    if (stat)
    {
        switch (offer->stat) {
        case RHH_STAT_ATTACK: effect.attack = offer->stages; break;
        case RHH_STAT_DEFENSE: effect.defense = offer->stages; break;
        case RHH_STAT_SPEED: effect.speed = offer->stages; break;
        case RHH_STAT_SP_ATTACK: effect.spAtk = offer->stages; break;
        case RHH_STAT_SP_DEFENSE: effect.spDef = offer->stages; break;
        case RHH_ADDITIONAL_ACCURACY: effect.accuracy = offer->stages; break;
        case RHH_ADDITIONAL_EVASION: effect.evasion = offer->stages; break;
        default: return RHH_ADDITIONAL_INVALID;
        }
    }
    sEffectCollector->effects[sEffectCollector->count++] = effect;
    return RHH_ADDITIONAL_OK;
}
static const struct MoveInfo *AdditionalMoveInfo(u32 index, RhhContentKey key, const struct MoveInfo *base)
{
    if (!key || !RhhModsHasActiveRun() || !RhhModsHasHook(RHH_MOVE_ADDITIONAL_EFFECTS) || sEffectCollector
        || base->effect != EFFECT_HIT || index >= ARRAY_COUNT(sEffectCaches)
        || (gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK)))) return base;
    struct MoveEffectsCache *cache = sEffectCaches[index];
    if (cache && cache->key == key && cache->run == RhhModsRunId() && cache->base == base)
        return cache->changed ? &cache->info : base;
    if (!cache && !(cache = sEffectCaches[index] = calloc(1, sizeof(*cache)))) return base;
    cache->key = key; cache->run = RhhModsRunId(); cache->base = base; cache->changed = FALSE;
    u32 existing = base->numAdditionalEffects;
    if (existing > MAX_MOVE_EFFECTS || (existing && !base->additionalEffects)) return base;
    struct MoveEffectsCollector collector = {.capacity = MAX_MOVE_EFFECTS - existing};
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOVE_ADDITIONAL_EFFECTS,
        .run_seed = RhhModsRunSeed(), .event_key = key, .args = {existing, collector.capacity}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sEffectCollector = &collector; RhhModsDispatch(&event, &decision); sEffectCollector = NULL;
    if (!decision.allowed || !collector.count) return base;
    if (existing) memcpy(cache->effects, base->additionalEffects, existing * sizeof(cache->effects[0]));
    memcpy(cache->effects + existing, collector.effects, collector.count * sizeof(cache->effects[0]));
    cache->info = *base; cache->info.additionalEffects = cache->effects;
    cache->info.numAdditionalEffects = existing + collector.count; cache->changed = TRUE;
    return &cache->info;
}
const struct MoveInfo *ModsBuiltinMoveInfo(u32 move)
{
    if (move >= MOVES_COUNT_ALL) return &gMovesInfo[MOVE_NONE];
    const struct MoveInfo *base = &gMovesInfo[move];
    if (move == MOVE_STRUGGLE || base->effect != EFFECT_HIT || !RhhModsHasActiveRun()
        || !RhhModsHasHook(RHH_MOVE_ADDITIONAL_EFFECTS) || sEffectCollector) return base;
    return AdditionalMoveInfo(move, RhhContentKeyForId(RHH_MOVE, move), base);
}

static u8 Glyph(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return CHAR_A + c - 'A';
    if (c >= 'a' && c <= 'z') return CHAR_a + c - 'a';
    if (c >= '0' && c <= '9') return CHAR_0 + c - '0';
    switch (c) {
    case ' ': return CHAR_SPACE; case '!': return CHAR_EXCL_MARK; case '.': return CHAR_PERIOD;
    case '-': return CHAR_HYPHEN; case ':': return CHAR_COLON; case ',': return CHAR_COMMA;
    case '/': return CHAR_SLASH; case '+': return CHAR_PLUS; case '(': return CHAR_LEFT_PAREN;
    case ')': return CHAR_RIGHT_PAREN; case '%': return CHAR_PERCENT; case '\'': return CHAR_SGL_QUOTE_RIGHT;
    case '\n': return CHAR_NEWLINE; default: return CHAR_QUESTION_MARK;
    }
}
static void CopyText(u8 *dest, size_t capacity, const char *source)
{
    size_t i = 0;
    if (source) while (i + 1 < capacity && source[i]) { dest[i] = Glyph(source[i]); ++i; }
    dest[i] = EOS;
}
void ModsResetCustomMoves(void)
{
    for (u32 i = 0; i < ARRAY_COUNT(sEffectCaches); ++i) { free(sEffectCaches[i]); sEffectCaches[i] = NULL; }
    memset(sMoves, 0, sizeof(sMoves)); sEffectCollector = NULL;
}
bool32 ModsIsCustomMove(u32 move) { return RhhModsMoveKey(move) != 0; }
static const struct RhhModMove *Definition(u32 move)
{ return RhhModsFindCustomMove(RhhModsMoveKey(move)); }
int ModsCustomMoveAvailable(const struct RhhModMove *def)
{
    u32 base, type;
    if (!def || def->struct_size < sizeof(*def) || def->category < RHH_MOVE_PHYSICAL || def->category > RHH_MOVE_STATUS
        || def->target < RHH_MOVE_TARGET_SELECTED || def->target > RHH_MOVE_TARGET_ALL_BATTLERS
        || def->power > 511 || def->accuracy > 100 || !def->pp || def->pp > 40
        || def->priority < -7 || def->priority > 7
        || RhhContentResolve(RHH_MOVE, def->template_move, &base) || !base || base >= MOVES_COUNT
        || base == MOVE_STRUGGLE || gMovesInfo[base].effect == EFFECT_PLACEHOLDER
        || ((gMovesInfo[base].category == DAMAGE_CATEGORY_STATUS) != (def->category == RHH_MOVE_STATUS))
        || (def->target == RHH_MOVE_TARGET_SMART && gMovesInfo[base].strikeCount <= 1)) return 0;
    if (RhhContentResolve(RHH_TYPE, def->type, &type))
        return !RhhModsCustomTypeSlot(def->type, &type);
    return type < NUMBER_OF_MON_TYPES && type != TYPE_NONE && type != TYPE_MYSTERY;
}
u32 ModsCustomMoveType(u32 move)
{
    const struct RhhModMove *def = Definition(move); u32 type;
    if (!def) return TYPE_MYSTERY;
    if (!RhhContentResolve(RHH_TYPE, def->type, &type) && type < NUMBER_OF_MON_TYPES) return type;
    return !RhhModsCustomTypeSlot(def->type, &type) ? type : TYPE_MYSTERY;
}
const struct MoveInfo *ModsCustomMoveInfo(u32 move)
{
    static const u8 missing[] = {CHAR_QUESTION_MARK, CHAR_QUESTION_MARK, CHAR_QUESTION_MARK, EOS};
    RhhContentKey key = RhhModsMoveKey(move);
    if (!key || move < RHH_RUNTIME_MOVE_FIRST || move >= RHH_RUNTIME_MOVE_FIRST + RHH_RUNTIME_MOVE_COUNT)
        return &gMovesInfo[MOVE_NONE];
    struct RuntimeMove *result = &sMoves[move - RHH_RUNTIME_MOVE_FIRST];
    if (result->key == key) return AdditionalMoveInfo(MOVES_COUNT_ALL + move - RHH_RUNTIME_MOVE_FIRST, key, &result->info);
    result->key = key; result->info = gMovesInfo[MOVE_NONE];
    result->info.name = missing; result->info.description = missing;
    result->info.effect = EFFECT_PLACEHOLDER; result->info.type = TYPE_MYSTERY;
    const struct RhhModMove *def = Definition(move); u32 base;
    if (!ModsCustomMoveAvailable(def) || RhhContentResolve(RHH_MOVE, def->template_move, &base)) return &result->info;
    static const u8 categories[] = {DAMAGE_CATEGORY_NONE, DAMAGE_CATEGORY_PHYSICAL, DAMAGE_CATEGORY_SPECIAL, DAMAGE_CATEGORY_STATUS};
    static const u8 targets[] = {TARGET_NONE, TARGET_SELECTED, TARGET_SMART, TARGET_DEPENDS, TARGET_OPPONENT,
        TARGET_RANDOM, TARGET_BOTH, TARGET_USER, TARGET_ALLY, TARGET_USER_AND_ALLY, TARGET_USER_OR_ALLY,
        TARGET_FOES_AND_ALLY, TARGET_FIELD, TARGET_OPPONENTS_FIELD, TARGET_ALL_BATTLERS};
    result->info = gMovesInfo[base];
    CopyText(result->name, sizeof(result->name), def->name);
    CopyText(result->description, sizeof(result->description), def->description);
    result->info.name = result->name; result->info.description = result->description;
    result->info.power = def->power; result->info.accuracy = def->accuracy; result->info.pp = def->pp;
    result->info.priority = def->priority; result->info.category = categories[def->category];
    result->info.target = targets[def->target];
    return AdditionalMoveInfo(MOVES_COUNT_ALL + move - RHH_RUNTIME_MOVE_FIRST, key, &result->info);
}

/* Added learnsets use stable keys in the declarations. Runtime arrays are
 * rebuilt when their saved slot mapping changes and remain borrowed until the
 * next run transition or module shutdown. */
struct LearnsetExtra { u16 move, level; };
struct LearnsetCache {
    const void *base;
    u32 count;
    struct LearnsetExtra extras[RHH_MOD_LEARNSETS_PER_SPECIES];
    void *data;
};
static struct LearnsetCache *sLearnsetCaches[NUM_SPECIES][3];
static struct LearnsetCache *sFilteredLearnsetCaches[NUM_SPECIES][3];
static RhhContentKey sLearnsetSpecies[NUM_SPECIES];
static bool32 sLearnsetFilterBusy;
bool32 ModsLearnsetResolutionActive(void) { return sLearnsetFilterBusy; }

void ModsClearLearnsets(void)
{
    for (u32 species = 0; species < NUM_SPECIES; ++species)
        for (u32 kind = 0; kind < 3; ++kind)
        {
            struct LearnsetCache *cache = sLearnsetCaches[species][kind];
            if (cache) { free(cache->data); free(cache); sLearnsetCaches[species][kind] = NULL; }
            cache = sFilteredLearnsetCaches[species][kind];
            if (cache) { free(cache->data); free(cache); sFilteredLearnsetCaches[species][kind] = NULL; }
        }
    memset(sLearnsetSpecies, 0, sizeof(sLearnsetSpecies));
}
void ModsPrepareLearnsets(void)
{
    ModsClearLearnsets();
    if (!RhhModsHasActiveRun()) return;
    for (u32 i = 0; i < RhhModsLearnsetCount(); ++i)
    {
        const struct RhhModLearnset *entry = RhhModsLearnsetAt(i);
        const struct RhhModMove *move = RhhModsFindCustomMove(entry->move);
        u32 slot;
        if (move && ModsCustomMoveAvailable(move)) (void)RhhModsMoveSlot(entry->move, 1, &slot);
    }
}
static u32 CollectLearnsetExtras(u32 species, u32 kind, struct LearnsetExtra *extras)
{
    if (!RhhModsHasActiveRun() || !species || species >= NUM_SPECIES || !RhhModsLearnsetCount()) return 0;
    if (!sLearnsetSpecies[species]) sLearnsetSpecies[species] = RhhContentKeyForId(RHH_SPECIES, species);
    RhhContentKey key = sLearnsetSpecies[species];
    if (!key) return 0;
    u32 count = 0, begin, end;
    if (RhhModsLearnsetRange(key, kind, &begin, &end)) return 0;
    for (u32 i = begin; i < end; ++i)
    {
        const struct RhhModLearnset *entry = RhhModsLearnsetAt(i);
        u32 move;
        if (RhhContentResolve(RHH_MOVE, entry->move, &move))
        {
            const struct RhhModMove *custom = RhhModsFindCustomMove(entry->move);
            if (!ModsCustomMoveAvailable(custom) || RhhModsMoveSlot(entry->move, 0, &move)) continue;
        }
        else if (!move || move >= MOVES_COUNT) continue;
        if (count < RHH_MOD_LEARNSETS_PER_SPECIES)
            extras[count++] = (struct LearnsetExtra){move, entry->level};
    }
    return count;
}
static struct LearnsetCache *FindLearnsetCache(u32 species, u32 kind, const void *base,
                                              const struct LearnsetExtra *extras, u32 count)
{
    struct LearnsetCache *cache = sLearnsetCaches[species][kind - 1];
    if (cache && cache->base == base && cache->count == count
        && !memcmp(cache->extras, extras, count * sizeof(*extras))) return cache;
    return NULL;
}
static void StoreLearnsetCache(u32 species, u32 kind, struct LearnsetCache *cache)
{
    struct LearnsetCache *old = sLearnsetCaches[species][kind - 1];
    if (old) { free(old->data); free(old); }
    sLearnsetCaches[species][kind - 1] = cache;
}
struct ResolvedLearnsetEntry { u32 move, level, ordinal, rank; bool32 addition, keep; };
static int CompareResolvedLearnset(const void *a, const void *b)
{
    const struct ResolvedLearnsetEntry *x = a, *y = b;
    if (x->level != y->level) return x->level < y->level ? -1 : 1;
    if (x->rank != y->rank) return x->rank < y->rank ? -1 : 1;
    return (x->ordinal > y->ordinal) - (x->ordinal < y->ordinal);
}
static bool32 FilterLearnsetsEnabled(void)
{
    return !sLearnsetFilterBusy && RhhModsHasActiveRun() && RhhModsHasHook(RHH_LEARNSET_ENTRY)
        && !(gMain.inBattle && (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_RECORDED_LINK)));
}
/* Build the complete merged list before filtering so an entry removed by a
 * mod can make room for an addition at the engine's byte-sized limit. */
static const void *FilterLearnset(u32 species, u32 kind, const void *base,
                                  const struct LearnsetExtra *extras, u32 extraCount)
{
    struct LearnsetCache *cache = sFilteredLearnsetCaches[species][kind - 1];
    if (cache && cache->base == base && cache->count == extraCount
        && !memcmp(cache->extras, extras, extraCount * sizeof(*extras))) return cache->data;
    u32 baseCount = 0;
    const struct LevelUpMove *levels = base; const u16 *other = base;
    if (kind == RHH_LEARNSET_LEVEL)
        while (levels[baseCount].move != LEVEL_UP_MOVE_END) ++baseCount;
    else while (other[baseCount] != MOVE_UNAVAILABLE) ++baseCount;
    if (baseCount > UINT32_MAX - extraCount) return base;
    u32 capacity = baseCount + extraCount;
    struct ResolvedLearnsetEntry *entries = calloc(capacity ? capacity : 1, sizeof(*entries));
    if (!entries) return base;
    u32 b = 0, e = 0, n = 0;
    while (b < baseCount || e < extraCount)
    {
        if (b < baseCount && (kind != RHH_LEARNSET_LEVEL || e == extraCount || levels[b].level <= extras[e].level))
        {
            entries[n++] = (struct ResolvedLearnsetEntry){.move = kind == RHH_LEARNSET_LEVEL ? levels[b].move : other[b],
                .level = kind == RHH_LEARNSET_LEVEL ? levels[b].level : 0, .keep = TRUE};
            ++b;
        }
        else
        {
            struct LearnsetExtra extra = extras[e++]; bool32 duplicate = FALSE;
            for (u32 j = 0; j < baseCount; ++j)
                if ((kind == RHH_LEARNSET_LEVEL ? levels[j].move : other[j]) == extra.move
                    && (kind != RHH_LEARNSET_LEVEL || levels[j].level == extra.level)) { duplicate = TRUE; break; }
            for (u32 j = 0; !duplicate && j < n; ++j)
                if (entries[j].move == extra.move && entries[j].level == extra.level) duplicate = TRUE;
            if (!duplicate) entries[n++] = (struct ResolvedLearnsetEntry){.move = extra.move, .level = extra.level,
                .addition = TRUE, .keep = TRUE};
        }
    }
    RhhContentKey speciesKey = RhhContentKeyForId(RHH_SPECIES, species);
    sLearnsetFilterBusy = TRUE;
    for (u32 i = 0; i < n; ++i)
    {
        struct ResolvedLearnsetEntry *entry = &entries[i]; entry->ordinal = entry->rank = i;
        RhhContentKey key = entry->move < MOVES_COUNT_ALL ? RhhContentKeyForId(RHH_MOVE, entry->move)
                                                            : RhhModsMoveKey(entry->move);
        if (!speciesKey || !key) continue;
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_LEARNSET_ENTRY,
            .run_seed = RhhModsRunSeed(), .event_key = key, .subject = speciesKey, .ordinal = i,
            .args = {kind, entry->level, entry->addition ? RHH_LEARNSET_ADDITION : RHH_LEARNSET_ENGINE, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = i};
        RhhModsDispatch(&event, &decision);
        entry->keep = !!decision.allowed;
        if (decision.value <= UINT32_MAX) entry->rank = decision.value;
    }
    sLearnsetFilterBusy = FALSE;
    qsort(entries, n, sizeof(*entries), CompareResolvedLearnset);
    u32 kept = 0, baseKept = 0;
    for (u32 i = 0; i < n; ++i) if (entries[i].keep) { ++kept; if (!entries[i].addition) ++baseKept; }
    u32 limit = kind == RHH_LEARNSET_TEACHABLE ? UINT32_MAX : 255;
    if (baseKept > limit) { free(entries); return base; }
    for (u32 i = n; kept > limit && i;)
        if (entries[--i].keep && entries[i].addition) { entries[i].keep = FALSE; --kept; }
    cache = calloc(1, sizeof(*cache));
    void *result = calloc((size_t)kept + 1, kind == RHH_LEARNSET_LEVEL ? sizeof(struct LevelUpMove) : sizeof(u16));
    if (!cache || !result) { free(cache); free(result); free(entries); return base; }
    u32 at = 0;
    for (u32 i = 0; i < n; ++i) if (entries[i].keep)
    {
        if (kind == RHH_LEARNSET_LEVEL)
            ((struct LevelUpMove *)result)[at++] = (struct LevelUpMove){entries[i].move, entries[i].level};
        else ((u16 *)result)[at++] = entries[i].move;
    }
    if (kind == RHH_LEARNSET_LEVEL) ((struct LevelUpMove *)result)[at] = (struct LevelUpMove){LEVEL_UP_MOVE_END, 0};
    else ((u16 *)result)[at] = MOVE_UNAVAILABLE;
    free(entries); cache->base = base; cache->count = extraCount;
    memcpy(cache->extras, extras, extraCount * sizeof(*extras)); cache->data = result;
    struct LearnsetCache *old = sFilteredLearnsetCaches[species][kind - 1];
    if (old) { free(old->data); free(old); }
    sFilteredLearnsetCaches[species][kind - 1] = cache; return result;
}
const struct LevelUpMove *ModsLevelUpLearnset(u32 species, const struct LevelUpMove *base)
{
    if (!base || sLearnsetFilterBusy) return base;
    struct LearnsetExtra extras[RHH_MOD_LEARNSETS_PER_SPECIES];
    u32 count = CollectLearnsetExtras(species, RHH_LEARNSET_LEVEL, extras);
    if (species && species < NUM_SPECIES && FilterLearnsetsEnabled())
        return FilterLearnset(species, RHH_LEARNSET_LEVEL, base, extras, count);
    if (!count || !base) return base;
    struct LearnsetCache *cache = FindLearnsetCache(species, RHH_LEARNSET_LEVEL, base, extras, count);
    if (cache) return cache->data;
    u32 baseCount = 0;
    while (base[baseCount].move != LEVEL_UP_MOVE_END) ++baseCount;
    /* The level-up learning cursor is byte-sized. */
    if (baseCount >= 255) return base;
    u32 capacity = baseCount + count < 255 ? baseCount + count : 255;
    cache = calloc(1, sizeof(*cache));
    struct LevelUpMove *result = calloc(capacity + 1, sizeof(*result));
    if (!cache || !result) { free(cache); free(result); return base; }
    u32 b = 0, e = 0, n = 0;
    while (b < baseCount || e < count)
    {
        if (b < baseCount && (e == count || base[b].level <= extras[e].level)) result[n++] = base[b++];
        else
        {
            struct LearnsetExtra extra = extras[e++]; bool32 duplicate = FALSE;
            for (u32 j = 0; j < baseCount; ++j)
                if (base[j].move == extra.move && base[j].level == extra.level) { duplicate = TRUE; break; }
            for (u32 j = 0; !duplicate && j < n; ++j)
                if (result[j].move == extra.move && result[j].level == extra.level) duplicate = TRUE;
            /* Preserve every engine entry if the cursor limit is reached. */
            if (!duplicate && n + (baseCount - b) < capacity) result[n++] = (struct LevelUpMove){extra.move, extra.level};
        }
    }
    result[n] = (struct LevelUpMove){LEVEL_UP_MOVE_END, 0};
    cache->base = base; cache->count = count; memcpy(cache->extras, extras, count * sizeof(*extras)); cache->data = result;
    StoreLearnsetCache(species, RHH_LEARNSET_LEVEL, cache); return result;
}
const u16 *ModsOtherLearnset(u32 species, u32 kind, const u16 *base)
{
    if (!base || sLearnsetFilterBusy || (kind != RHH_LEARNSET_TEACHABLE && kind != RHH_LEARNSET_EGG)) return base;
    struct LearnsetExtra extras[RHH_MOD_LEARNSETS_PER_SPECIES];
    u32 count = CollectLearnsetExtras(species, kind, extras);
    if (species && species < NUM_SPECIES && FilterLearnsetsEnabled())
        return FilterLearnset(species, kind, base, extras, count);
    if (!count || !base) return base;
    struct LearnsetCache *cache = FindLearnsetCache(species, kind, base, extras, count);
    if (cache) return cache->data;
    u32 baseCount = 0; while (base[baseCount] != MOVE_UNAVAILABLE) ++baseCount;
    /* The HGSS Pokédex stores its egg-move count in a byte. */
    if (kind == RHH_LEARNSET_EGG && baseCount >= 255) return base;
    u32 capacity = baseCount + count;
    if (kind == RHH_LEARNSET_EGG && capacity > 255) capacity = 255;
    cache = calloc(1, sizeof(*cache)); u16 *result = calloc(capacity + 1, sizeof(*result));
    if (!cache || !result) { free(cache); free(result); return base; }
    memcpy(result, base, baseCount * sizeof(*result)); u32 n = baseCount;
    for (u32 i = 0; i < count; ++i)
    {
        u32 j; for (j = 0; j < n; ++j) if (result[j] == extras[i].move) break;
        if (j == n && n < capacity) result[n++] = extras[i].move;
    }
    result[n] = MOVE_UNAVAILABLE;
    cache->base = base; cache->count = count; memcpy(cache->extras, extras, count * sizeof(*extras)); cache->data = result;
    StoreLearnsetCache(species, kind, cache); return result;
}
