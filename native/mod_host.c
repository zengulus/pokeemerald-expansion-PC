#include "mod_host.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>

enum { MAX_MODS = RHH_HOST_MAX_MODS, MAX_HOOKS = 256, MAX_SCENE_RULES = 2048, RECORD_HEADER = 80, FILE_HEADER = 24 };
struct Mod { const struct RhhModDefinition *def; uint8_t *state; };
struct Hook { unsigned mod, index, rank; };
struct TraceCache { int valid; struct RhhModEvent event; struct RhhModDecision before, after; };
static struct Mod sMods[MAX_MODS];
static struct Hook sHooks[MAX_HOOKS];
static uint8_t sHookEventPresent[RHH_MOD_EVENT_COUNT];
static struct TraceCache sLastTrace[MAX_HOOKS];
static unsigned sModCount, sHookCount;
static int sFinalized;
static FILE *sTrace;
static uint64_t sTraceSequence;
static char sError[192];
static const struct RhhContentBinding *sContent;
static size_t sContentCount;
struct ContentIndex { uint64_t key; const struct RhhContentBinding *binding; };
static struct ContentIndex *sContentIndex;
static void (*sMetadata)(uint32_t, uint32_t, struct RhhContentInfo *);
static int (*sMoveEffectValidator)(RhhContentKey, const struct RhhModMove *, uint32_t);
static int (*sPersist)(uint64_t, const uint8_t *, uint32_t);
static void (*sBeforeSave[8])(void);
static unsigned sBeforeSaveCount;
static int (*sSaveProjection)(const char *, const uint8_t *, uint32_t, uint32_t *, uint32_t *);
static uint64_t sRunSeed, sRunId;
static int sCommitted, sSettingsLocked;
static struct { const char *id; const struct RhhServiceHeader *service; } sServices[128];
static unsigned sServiceCount;
static int sWorldPresentersShutdown;
static unsigned sDispatchDepth;
static uint32_t sDispatchEvent;
static uint32_t sDispatchStage;
static RhhContentKey sDispatchOwner;
static RhhContentKey (*sUiInputOwnerProvider)(void);
static int sDrawing;
static void (*sLifecycleCallbacks[8])(void);
static unsigned sLifecycleCount;
static void RunLifecycleCallbacks(void)
{ for (unsigned i = 0; i < sLifecycleCount; ++i) sLifecycleCallbacks[i](); }

static int Fail(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(sError, sizeof(sError), format, args);
    va_end(args);
    if (sTrace) fprintf(sTrace, "{\"event\":\"error\",\"reason\":\"%s\"}\n", sError);
    return -1;
}
static int ValidName(const char *s)
{
    if (!s || !*s || strlen(s) >= RHH_MOD_ID_MAX) return 0;
    for (; *s; ++s)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9')
            || *s == ':' || *s == '/' || *s == '-' || *s == '_' || *s == '.')) return 0;
    return 1;
}
RhhContentKey RhhContentKeyOf(const char *name)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    if (!name) return 0;
    for (; *name; ++name) hash = (hash ^ (uint8_t)*name) * UINT64_C(1099511628211);
    return hash;
}
static uint64_t Mix(uint64_t x)
{
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}
uint64_t RhhModRandomAt(uint64_t seed, const char *id, uint64_t key, uint64_t ordinal)
{
    return Mix(seed ^ Mix(RhhContentKeyOf(id)) ^ Mix(key) ^ Mix(ordinal + UINT64_C(0x9e3779b97f4a7c15)));
}
static int CompareContent(const void *a, const void *b)
{
    uint64_t ka = ((const struct ContentIndex *)a)->key, kb = ((const struct ContentIndex *)b)->key;
    return (ka > kb) - (ka < kb);
}
int RhhModsSetContent(const struct RhhContentBinding *bindings, size_t count)
{
    if (sFinalized) return Fail("content registration is closed");
    struct ContentIndex *index = calloc(count ? count : 1, sizeof(*index));
    if (!index) return Fail("cannot allocate content registry");
    for (size_t i = 0; i < count; ++i)
    {
        if (!bindings[i].name || strlen(bindings[i].name) > 127)
        { free(index); return Fail("invalid content name"); }
        index[i] = (struct ContentIndex){RhhContentKeyOf(bindings[i].name), &bindings[i]};
        if (!index[i].key) { free(index); return Fail("zero content key is reserved"); }
    }
    qsort(index, count, sizeof(*index), CompareContent);
    for (size_t i = 1; i < count; ++i)
        if (index[i].key == index[i - 1].key)
        { free(index); return Fail("duplicate or colliding content identity"); }
    free(sContentIndex);
    sContentIndex = index;
    sContent = bindings;
    sContentCount = count;
    return 0;
}
int RhhContentResolve(uint32_t kind, RhhContentKey key, uint32_t *id)
{
    struct ContentIndex query = {.key = key};
    const struct ContentIndex *found = bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent);
    if (found && found->binding->kind == kind)
    { if (id) *id = found->binding->engine_id; return 0; }
    return -1;
}
const char *RhhContentName(uint32_t kind, uint32_t id)
{
    for (size_t i = 0; i < sContentCount; ++i)
        if (sContent[i].kind == kind && sContent[i].engine_id == id) return sContent[i].name;
    return NULL;
}
RhhContentKey RhhContentKeyForId(uint32_t kind, uint32_t id)
{
    const char *name = RhhContentName(kind, id);
    return name ? RhhContentKeyOf(name) : 0;
}
void RhhModsSetContentMetadata(void (*provider)(uint32_t, uint32_t, struct RhhContentInfo *)) { sMetadata = provider; }
uint32_t RhhContentCount(uint32_t kind)
{
    uint32_t n = 0;
    for (size_t i = 0; i < sContentCount; ++i) if (sContent[i].kind == kind) ++n;
    return n;
}
static int ContentInfo(const struct RhhContentBinding *binding, struct RhhContentInfo *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    memset(out, 0, sizeof(*out)); out->struct_size = sizeof(*out);
    out->kind = binding->kind; out->key = RhhContentKeyOf(binding->name);
    out->canonical_key = RhhContentKeyForId(binding->kind, binding->engine_id);
    out->flags = RHH_CONTENT_ENABLED;
    if (sMetadata) sMetadata(binding->kind, binding->engine_id, out);
    return 0;
}
int RhhContentAt(uint32_t kind, uint32_t index, struct RhhContentInfo *out)
{
    for (size_t i = 0; i < sContentCount; ++i)
        if (sContent[i].kind == kind && !index--) return ContentInfo(&sContent[i], out);
    return -1;
}
int RhhContentInfoFor(uint32_t kind, RhhContentKey key, struct RhhContentInfo *out)
{
    struct ContentIndex query = {.key = key};
    const struct ContentIndex *found = bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent);
    return found && found->binding->kind == kind ? ContentInfo(found->binding, out) : -1;
}
static const struct RhhServiceHeader *GetService(const char *id, uint32_t minimum)
{
    if (!id || !(minimum >> 16)) return NULL;
    for (unsigned i = 0; i < sServiceCount; ++i)
        if (!strcmp(id, sServices[i].id))
        {
            const struct RhhServiceHeader *service = sServices[i].service;
            return (service->version >> 16) == (minimum >> 16) && service->version >= minimum ? service : NULL;
        }
    return NULL;
}
int RhhModsAddService(const char *id, const struct RhhServiceHeader *service)
{
    if (sFinalized || !ValidName(id) || !service || service->struct_size < sizeof(*service)
        || !(service->version >> 16) || sServiceCount == sizeof(sServices) / sizeof(sServices[0]))
        return Fail("invalid service registration");
    for (unsigned i = 0; i < sServiceCount; ++i)
        if (!strcmp(id, sServices[i].id)) return Fail("duplicate service id");
    sServices[sServiceCount].id = id; sServices[sServiceCount++].service = service;
    return 0;
}
static const struct RhhModHostV1 sAPI = {
    sizeof(sAPI), RHH_MOD_ABI_V1, sizeof(void *) * 8, 0,
    RhhContentKeyOf, RhhContentResolve, RhhContentName, RhhModRandomAt,
    RhhContentCount, RhhContentAt, RhhContentInfoFor, RhhModsGetSetting, GetService
};
const struct RhhModHostV1 *RhhModsHostAPI(void) { return &sAPI; }
static unsigned ShaderCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_SHADER_SIZE ? d->shader_count : 0; }
static unsigned ShaderPhotoPolicyCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_SHADER_PHOTO_SIZE ? d->shader_photo_policy_count : 0; }
static unsigned WorldPresenterCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_WORLD_SIZE ? d->world_presenter_count : 0; }
static unsigned ServiceRequirementCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_SERVICE_SIZE ? d->service_requirement_count : 0; }
static unsigned MusicCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_MUSIC_SIZE ? d->music_count : 0; }
static unsigned SceneRuleCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_SCENE_SIZE ? d->scene_rule_count : 0; }
static unsigned SceneLightCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_SCENE_LIGHT_SIZE ? d->scene_light_count : 0; }
static unsigned EcologyTagCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_ECOLOGY_SIZE ? d->ecology_tag_count : 0; }
static unsigned SpeciesTagCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_ECOLOGY_SIZE ? d->species_tag_count : 0; }
static unsigned CustomTypeCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_TYPES_SIZE ? d->type_count : 0; }
static unsigned CustomMoveCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_MOVES_SIZE ? d->move_count : 0; }
static unsigned ModLearnsetCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_LEARNSETS_SIZE ? d->learnset_count : 0; }
static unsigned BattleConditionCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_CONDITIONS_SIZE ? d->battle_condition_count : 0; }
static unsigned MoveEffectCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_MOVE_EFFECTS_SIZE ? d->move_effect_count : 0; }
static unsigned MoveEffectBindingCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_MOVE_EFFECTS_SIZE ? d->move_effect_binding_count : 0; }
static unsigned CustomAbilityCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_ABILITIES_SIZE ? d->ability_count : 0; }
static unsigned HabitatCount(const struct RhhModDefinition *d)
{ return d->struct_size >= RHH_MOD_DEFINITION_HABITATS_SIZE ? d->habitat_count : 0; }
enum { MAX_CUSTOM_TYPES = RHH_CUSTOM_TYPE_LIMIT, MAX_CUSTOM_TYPE_RELATIONS = RHH_CUSTOM_TYPE_RELATION_LIMIT };
static const struct RhhModType *sCustomTypes[MAX_CUSTOM_TYPES];
static unsigned sCustomTypeCount;
static const struct RhhModMove *sCustomMoves[RHH_CUSTOM_MOVE_LIMIT];
static unsigned sCustomMoveCount;
static const struct RhhModLearnset *sLearnsets[RHH_MOD_LEARNSETS_LIMIT];
static unsigned sLearnsetCount;
static const struct RhhModBattleCondition *sBattleConditions[RHH_BATTLE_CONDITION_LIMIT];
static unsigned sBattleConditionCount;
static const struct RhhModMoveEffect *sMoveEffects[RHH_MOVE_EFFECT_LIMIT];
static const struct RhhModMoveEffectBinding *sMoveEffectBindings[RHH_MOVE_EFFECT_BINDING_LIMIT];
static const struct RhhModAbility *sCustomAbilities[RHH_CUSTOM_ABILITY_LIMIT];
static unsigned sCustomAbilityCount;
static unsigned sMoveEffectCount, sMoveEffectBindingCount;
int RhhModsSetMoveEffectValidator(int (*validator)(RhhContentKey, const struct RhhModMove *, uint32_t))
{
    if (sFinalized || sMoveEffectValidator || !validator) return Fail("invalid move effect validator");
    sMoveEffectValidator = validator; return 0;
}
uint32_t RhhModsRegisteredEffectCount(void) { return sFinalized ? sMoveEffectCount : 0; }
const struct RhhModMoveEffect *RhhModsRegisteredEffectAt(uint32_t index)
{ return sFinalized && index < sMoveEffectCount ? sMoveEffects[index] : NULL; }
const struct RhhModMoveEffect *RhhModsRegisteredEffect(RhhContentKey key)
{
    if (!sFinalized || !key) return NULL;
    for (unsigned i = 0; i < sMoveEffectCount; ++i)
        if (RhhContentKeyOf(sMoveEffects[i]->id) == key) return sMoveEffects[i];
    return NULL;
}
RhhContentKey RhhModsRegisteredEffectForMove(RhhContentKey move)
{
    if (!sFinalized || !move) return 0;
    unsigned lo = 0, hi = sMoveEffectBindingCount;
    while (lo < hi)
    {
        unsigned mid = lo + (hi - lo) / 2;
        if (sMoveEffectBindings[mid]->move < move) lo = mid + 1; else hi = mid;
    }
    return lo < sMoveEffectBindingCount && sMoveEffectBindings[lo]->move == move
        ? sMoveEffectBindings[lo]->effect : 0;
}
static const struct RhhModMoveEffect *FindDeclaredMoveEffect(RhhContentKey key)
{
    for (unsigned i = 0; key && i < sModCount; ++i)
        for (unsigned j = 0; j < MoveEffectCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->move_effects[j].id) == key) return &sMods[i].def->move_effects[j];
    return NULL;
}
static const struct RhhModAbility *FindDeclaredCustomAbility(RhhContentKey key)
{
    for (unsigned i = 0; key && i < sModCount; ++i)
        for (unsigned j = 0; j < CustomAbilityCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->abilities[j].id) == key) return &sMods[i].def->abilities[j];
    return NULL;
}
uint32_t RhhModsCustomAbilityCount(void) { return sFinalized ? sCustomAbilityCount : 0; }
const struct RhhModAbility *RhhModsCustomAbilityAt(uint32_t index)
{ return sFinalized && index < sCustomAbilityCount ? sCustomAbilities[index] : NULL; }
const struct RhhModAbility *RhhModsCustomAbility(RhhContentKey key)
{
    if (!sFinalized || !key) return NULL;
    for (unsigned i = 0; i < sCustomAbilityCount; ++i)
        if (RhhContentKeyOf(sCustomAbilities[i]->id) == key) return sCustomAbilities[i];
    return NULL;
}
static int CompareCustomAbilities(const void *left, const void *right)
{
    const struct RhhModAbility *const *a = left, *const *b = right;
    return strcmp((*a)->id, (*b)->id);
}
static int ValidCustomAbilityId(const char *id)
{
    if (!ValidName(id)) return 0;
    const char *colon = strchr(id, ':');
    return colon && colon != id && !strncmp(colon + 1, "ability/", 8) && colon[9]
        && !strchr(colon + 9, ':') && !strchr(colon + 9, '/')
        && !(colon - id == 3 && !strncmp(id, "rhh", 3));
}
static int CompareMoveEffects(const void *left, const void *right)
{
    RhhContentKey a = RhhContentKeyOf((*(const struct RhhModMoveEffect *const *)left)->id);
    RhhContentKey b = RhhContentKeyOf((*(const struct RhhModMoveEffect *const *)right)->id);
    return (a > b) - (a < b);
}
static int CompareMoveEffectBindings(const void *left, const void *right)
{
    RhhContentKey a = (*(const struct RhhModMoveEffectBinding *const *)left)->move;
    RhhContentKey b = (*(const struct RhhModMoveEffectBinding *const *)right)->move;
    return (a > b) - (a < b);
}
static int ValidMoveEffectId(const char *id)
{
    if (!ValidName(id)) return 0;
    const char *colon = strchr(id, ':');
    return colon && colon != id && !strncmp(colon + 1, "effect/", 7) && colon[8]
        && !strchr(colon + 8, ':') && !strchr(colon + 8, '/')
        && !(colon - id == 3 && !strncmp(id, "rhh", 3));
}
uint32_t RhhModsBattleConditionCount(void) { return sFinalized ? sBattleConditionCount : 0; }
const struct RhhModBattleCondition *RhhModsBattleConditionAt(uint32_t index)
{ return sFinalized && index < sBattleConditionCount ? sBattleConditions[index] : NULL; }
int RhhModsBattleConditionIndex(RhhContentKey key)
{
    if (!sFinalized || !key) return -1;
    for (unsigned i = 0; i < sBattleConditionCount; ++i)
        if (RhhContentKeyOf(sBattleConditions[i]->id) == key) return (int)i;
    return -1;
}
static const struct RhhModBattleCondition *FindDeclaredBattleCondition(RhhContentKey key)
{
    for (unsigned i = 0; key && i < sModCount; ++i)
        for (unsigned j = 0; j < BattleConditionCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->battle_conditions[j].id) == key)
                return &sMods[i].def->battle_conditions[j];
    return NULL;
}
static int CompareBattleConditions(const void *left, const void *right)
{
    RhhContentKey a = RhhContentKeyOf((*(const struct RhhModBattleCondition *const *)left)->id);
    RhhContentKey b = RhhContentKeyOf((*(const struct RhhModBattleCondition *const *)right)->id);
    return (a > b) - (a < b);
}
static int ValidBattleConditionId(const char *id)
{
    if (!ValidName(id)) return 0;
    const char *colon = strchr(id, ':');
    return colon && colon != id && !strncmp(colon + 1, "condition/", 10) && colon[11]
        && !strchr(colon + 11, ':') && !strchr(colon + 11, '/')
        && !(colon - id == 3 && !strncmp(id, "rhh", 3));
}
uint32_t RhhModsLearnsetCount(void) { return sFinalized ? sLearnsetCount : 0; }
const struct RhhModLearnset *RhhModsLearnsetAt(uint32_t index)
{ return sFinalized && index < sLearnsetCount ? sLearnsets[index] : NULL; }
int RhhModsLearnsetRange(RhhContentKey species, uint32_t kind, uint32_t *begin, uint32_t *end)
{
    if (!sFinalized || !species || !begin || !end) return -1;
    uint32_t lo = 0, hi = sLearnsetCount;
    while (lo < hi)
    {
        uint32_t mid = lo + (hi - lo) / 2;
        const struct RhhModLearnset *entry = sLearnsets[mid];
        if (entry->species < species || (entry->species == species && entry->kind < kind)) lo = mid + 1;
        else hi = mid;
    }
    uint32_t first = lo;
    while (lo < sLearnsetCount && sLearnsets[lo]->species == species && sLearnsets[lo]->kind == kind) ++lo;
    *begin = first; *end = lo; return 0;
}
static int CompareLearnsets(const void *left, const void *right)
{
    const struct RhhModLearnset *a = *(const struct RhhModLearnset *const *)left;
    const struct RhhModLearnset *b = *(const struct RhhModLearnset *const *)right;
    if (a->species != b->species) return a->species < b->species ? -1 : 1;
    if (a->kind != b->kind) return a->kind < b->kind ? -1 : 1;
    if (a->level != b->level) return a->level < b->level ? -1 : 1;
    return (a->move > b->move) - (a->move < b->move);
}
static const struct RhhModMove *FindDeclaredCustomMove(RhhContentKey key)
{
    if (!key) return NULL;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < CustomMoveCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->moves[j].id) == key) return &sMods[i].def->moves[j];
    return NULL;
}
const struct RhhModMove *RhhModsFindCustomMove(RhhContentKey key)
{
    if (!sFinalized || !key) return NULL;
    for (unsigned i = 0; i < sCustomMoveCount; ++i)
        if (RhhContentKeyOf(sCustomMoves[i]->id) == key) return sCustomMoves[i];
    return NULL;
}
static int ValidCustomMoveId(const char *id)
{
    if (!ValidName(id)) return 0;
    const char *colon = strchr(id, ':');
    return colon && colon != id && !strncmp(colon + 1, "move/", 5) && colon[6]
        && !strchr(colon + 6, ':') && !strchr(colon + 6, '/')
        && !(colon - id == 3 && !strncmp(id, "rhh", 3));
}
static int ValidMoveText(const char *text, size_t limit, int multiline)
{
    if (!text || !*text || strlen(text) >= limit) return 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        if ((*p < 32 && !(multiline && *p == '\n')) || *p > 126) return 0;
    return 1;
}

static int ValidCustomTypeId(const char *id)
{
    if (!ValidName(id)) return 0;
    const char *colon = strchr(id, ':');
    return colon && colon != id && !strncmp(colon + 1, "type/", 5) && colon[6]
        && !strchr(colon + 6, ':') && !strchr(colon + 6, '/')
        && !(colon - id == 3 && !strncmp(id, "rhh", 3));
}
static int ValidTypeLabel(const char *label)
{
    if (!label || !*label || strlen(label) >= sizeof(((struct RhhTypeDefinition *)0)->label)) return 0;
    for (const unsigned char *p = (const unsigned char *)label; *p; ++p)
        if (*p < 32 || *p > 126) return 0;
    return 1;
}
static int ValidTypeRelationMultiplier(uint32_t value)
{
    return value == RHH_TYPE_UNSPECIFIED || value == 0 || value == RHH_TYPE_ONE / 2
        || value == RHH_TYPE_ONE || value == RHH_TYPE_ONE * 2;
}
static const struct RhhModType *FindDeclaredCustomType(RhhContentKey key)
{
    if (!key) return NULL;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < CustomTypeCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->types[j].id) == key) return &sMods[i].def->types[j];
    return NULL;
}
static const struct RhhModType *FindFinalCustomType(RhhContentKey key)
{
    for (unsigned i = 0; i < sCustomTypeCount; ++i)
        if (RhhContentKeyOf(sCustomTypes[i]->id) == key) return sCustomTypes[i];
    return NULL;
}
static const struct RhhTypeRelation *FindCustomTypeRelation(const struct RhhModType *type, RhhContentKey other)
{
    if (type)
        for (unsigned i = 0; i < type->relation_count; ++i)
            if (type->relations[i].other == other) return &type->relations[i];
    return NULL;
}
static int CompareCustomTypes(const void *left, const void *right)
{
    const struct RhhModType *const *a = left, *const *b = right;
    return strcmp((*a)->id, (*b)->id);
}
static int CopyCustomType(const struct RhhModType *type, struct RhhTypeDefinition *out)
{
    if (!sFinalized || !type || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhTypeDefinition result = {.struct_size = sizeof(result), .rgb = type->rgb, .key = RhhContentKeyOf(type->id)};
    memcpy(result.id, type->id, strlen(type->id) + 1);
    memcpy(result.label, type->label, strlen(type->label) + 1);
    *out = result; return 0;
}
static uint32_t CustomCatalogCount(void) { return sFinalized ? sCustomTypeCount : 0; }
static int CustomCatalogAt(uint32_t index, struct RhhTypeDefinition *out)
{ return CopyCustomType(index < sCustomTypeCount ? sCustomTypes[index] : NULL, out); }
static int CustomCatalogFind(RhhContentKey key, struct RhhTypeDefinition *out)
{ return CopyCustomType(FindFinalCustomType(key), out); }
static int DescribeType(RhhContentKey key, struct RhhTypeDisplayInfo *out)
{
    if (!sFinalized || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhTypeDisplayInfo result = {.struct_size = sizeof(result), .key = key};
    const struct RhhModType *custom = FindFinalCustomType(key);
    if (custom)
    {
        result.flags = RHH_TYPE_DISPLAY_CUSTOM | RHH_TYPE_DISPLAY_COLOR;
        result.rgb = custom->rgb;
        memcpy(result.id, custom->id, strlen(custom->id) + 1);
        memcpy(result.label, custom->label, strlen(custom->label) + 1);
    }
    else
    {
        uint32_t id;
        if (RhhContentResolve(RHH_TYPE, key, &id)) return -1;
        const char *name = RhhContentName(RHH_TYPE, id);
        if (!name || strncmp(name, "rhh:type/", 9) || RhhContentKeyOf(name) != key) return -1;
        const char *label = name + 9;
        size_t length = strlen(label);
        if (!length || length >= sizeof(result.label) || strlen(name) >= sizeof(result.id)) return -1;
        memcpy(result.id, name, strlen(name) + 1);
        for (size_t i = 0; i < length; ++i)
        {
            char c = label[i] == '_' || label[i] == '-' ? ' ' : label[i];
            if ((i == 0 || result.label[i - 1] == ' ') && c >= 'a' && c <= 'z') c -= 'a' - 'A';
            result.label[i] = c;
        }
    }
    *out = result; return 0;
}
static const struct RhhTypeCatalogService sTypeCatalog = {
    {sizeof(sTypeCatalog), RHH_SERVICE_VERSION(1, 1)}, CustomCatalogCount, CustomCatalogAt, CustomCatalogFind, DescribeType
};
int RhhModsInstallTypeCatalogService(void)
{ return RhhModsAddService("rhh:service/type_catalog", &sTypeCatalog.header); }
int RhhModsCustomTypeSlot(RhhContentKey key, uint32_t *slot)
{
    if (!sFinalized || !key || !slot) return -1;
    for (unsigned i = 0; i < sCustomTypeCount && i < RHH_RUNTIME_TYPE_COUNT; ++i)
        if (RhhContentKeyOf(sCustomTypes[i]->id) == key)
        { *slot = RHH_RUNTIME_TYPE_FIRST + i; return 0; }
    return -1;
}
RhhContentKey RhhModsCustomTypeKey(uint32_t slot)
{
    if (!sFinalized || slot < RHH_RUNTIME_TYPE_FIRST || slot >= 256u) return 0;
    unsigned index = slot - RHH_RUNTIME_TYPE_FIRST;
    return index < sCustomTypeCount ? RhhContentKeyOf(sCustomTypes[index]->id) : 0;
}
int RhhModsCustomTypeMatchup(RhhContentKey attack, RhhContentKey defense, uint32_t *base)
{
    if (!sFinalized || !base || !attack || !defense) return -1;
    const struct RhhModType *a = FindFinalCustomType(attack), *d = FindFinalCustomType(defense);
    if ((!a && !d) || (!a && RhhContentResolve(RHH_TYPE, attack, NULL))
        || (!d && RhhContentResolve(RHH_TYPE, defense, NULL))) return -1;
    const struct RhhTypeRelation *ar = FindCustomTypeRelation(a, defense);
    const struct RhhTypeRelation *dr = FindCustomTypeRelation(d, attack);
    uint32_t av = ar ? ar->attack : RHH_TYPE_UNSPECIFIED;
    uint32_t dv = dr ? dr->defense : RHH_TYPE_UNSPECIFIED;
    if (av != RHH_TYPE_UNSPECIFIED && dv != RHH_TYPE_UNSPECIFIED && av != dv) return -1;
    *base = av != RHH_TYPE_UNSPECIFIED ? av : dv != RHH_TYPE_UNSPECIFIED ? dv : RHH_TYPE_ONE;
    return 0;
}
enum { MAX_ECOLOGY_TAGS = 512, MAX_SPECIES_TAGS = 4096 };
static const struct RhhModEcologyTag *sEcologyTags[MAX_ECOLOGY_TAGS];
static unsigned sEcologyTagCount;
struct SpeciesTagIndex { uint32_t species; const struct RhhModEcologyTag *tag; };
static struct SpeciesTagIndex sSpeciesTags[MAX_SPECIES_TAGS];
static unsigned sSpeciesTagCount;
static const struct RhhModEcologyTag *FindEcologyTag(RhhContentKey key)
{
    if (!key) return NULL;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < EcologyTagCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->ecology_tags[j].id) == key) return &sMods[i].def->ecology_tags[j];
    return NULL;
}
static int CompareEcologyTags(const void *left, const void *right)
{
    const struct RhhModEcologyTag *const *a = left, *const *b = right;
    return strcmp((*a)->id, (*b)->id);
}
static int CompareSpeciesTags(const void *left, const void *right)
{
    const struct SpeciesTagIndex *a = left, *b = right;
    if (a->species != b->species) return a->species < b->species ? -1 : 1;
    return strcmp(a->tag->id, b->tag->id);
}
static int CopyEcologyTag(const struct RhhModEcologyTag *tag, struct RhhEcologyTag *out)
{
    if (!sFinalized || !tag || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhEcologyTag result = {.struct_size = sizeof(result), .key = RhhContentKeyOf(tag->id)};
    memcpy(result.id, tag->id, strlen(tag->id) + 1);
    memcpy(result.label, tag->label, strlen(tag->label) + 1);
    memcpy(result.description, tag->description, strlen(tag->description) + 1);
    *out = result; return 0;
}
static uint32_t EcologyTagsCount(void) { return sFinalized ? sEcologyTagCount : 0; }
static int EcologyTagsAt(uint32_t index, struct RhhEcologyTag *out)
{ return index < sEcologyTagCount ? CopyEcologyTag(sEcologyTags[index], out) : -1; }
static int EcologyTagsFind(RhhContentKey key, struct RhhEcologyTag *out)
{ return CopyEcologyTag(sFinalized ? FindEcologyTag(key) : NULL, out); }
static int EcologySpeciesCount(RhhContentKey species, uint32_t *out)
{
    uint32_t id, count = 0;
    if (!sFinalized || !out || RhhContentResolve(RHH_SPECIES, species, &id)) return -1;
    for (unsigned i = 0; i < sSpeciesTagCount; ++i) count += sSpeciesTags[i].species == id;
    *out = count; return 0;
}
static int EcologySpeciesAt(RhhContentKey species, uint32_t index, struct RhhEcologyTag *out)
{
    uint32_t id;
    if (!sFinalized || RhhContentResolve(RHH_SPECIES, species, &id)) return -1;
    for (unsigned i = 0; i < sSpeciesTagCount; ++i)
        if (sSpeciesTags[i].species == id && !index--) return CopyEcologyTag(sSpeciesTags[i].tag, out);
    return -1;
}
static int EcologySpeciesHas(RhhContentKey species, RhhContentKey tag, uint32_t *out)
{
    uint32_t id, found = 0;
    if (!sFinalized || !out || !FindEcologyTag(tag) || RhhContentResolve(RHH_SPECIES, species, &id)) return -1;
    for (unsigned i = 0; i < sSpeciesTagCount; ++i)
        if (sSpeciesTags[i].species == id && RhhContentKeyOf(sSpeciesTags[i].tag->id) == tag) { found = 1; break; }
    *out = found; return 0;
}
static const struct RhhEcologyTagsService sEcologyTagsService = {
    {sizeof(sEcologyTagsService), RHH_SERVICE_VERSION(1, 0)}, EcologyTagsCount, EcologyTagsAt,
    EcologyTagsFind, EcologySpeciesCount, EcologySpeciesAt, EcologySpeciesHas
};
int RhhModsInstallEcologyTagsService(void)
{ return RhhModsAddService("rhh:service/ecology_tags", &sEcologyTagsService.header); }
int RhhModsResolveHabitatWeather(const struct RhhHabitatSample *sample, struct RhhHabitatWeather *out)
{
    static const uint8_t natural[] = {RHH_HABITAT_WEATHER_CLEAR, RHH_HABITAT_WEATHER_CLOUDS,
        RHH_HABITAT_WEATHER_SUN, RHH_HABITAT_WEATHER_RAIN, RHH_HABITAT_WEATHER_SNOW,
        RHH_HABITAT_WEATHER_THUNDERSTORM, RHH_HABITAT_WEATHER_FOG, RHH_HABITAT_WEATHER_ASH,
        RHH_HABITAT_WEATHER_SAND, RHH_HABITAT_WEATHER_SHADE, RHH_HABITAT_WEATHER_HARSH_SUN,
        RHH_HABITAT_WEATHER_HEAVY_RAIN, RHH_HABITAT_WEATHER_UNDERWATER, RHH_HABITAT_WEATHER_UNDERWATER,
        RHH_HABITAT_WEATHER_DYNAMIC, RHH_HABITAT_WEATHER_UNKNOWN};
    static const uint8_t saved[] = {RHH_HABITAT_WEATHER_CLEAR, RHH_HABITAT_WEATHER_RAIN,
        RHH_HABITAT_WEATHER_SUN, RHH_HABITAT_WEATHER_SAND, RHH_HABITAT_WEATHER_HAIL,
        RHH_HABITAT_WEATHER_SNOW, RHH_HABITAT_WEATHER_FOG, RHH_HABITAT_WEATHER_WIND,
        RHH_HABITAT_WEATHER_HARSH_SUN, RHH_HABITAT_WEATHER_HEAVY_RAIN};
    if (!sample || sample->struct_size < sizeof(*sample) || sample->natural_weather >= (sizeof(natural) / sizeof(natural[0]))
        || sample->environment_available > 1 || (sample->environment_available
            && sample->environment.weather.value >= (sizeof(saved) / sizeof(saved[0]))) || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhHabitatWeather result = {.struct_size = sizeof(result), .saved_available = sample->environment_available};
    if (sample->environment_available && sample->environment.weather.value != RHH_WEATHER_NONE) {
        result.source = RHH_HABITAT_WEATHER_SAVED; result.kind = saved[sample->environment.weather.value];
    } else {
        result.source = RHH_HABITAT_WEATHER_NATURAL; result.kind = natural[sample->natural_weather];
    }
    *out = result; return 0;
}
enum { MAX_HABITATS = 512 };
static const struct RhhModHabitat *sHabitats[MAX_HABITATS];
static unsigned sHabitatCount;
static const struct RhhModHabitat *FindHabitat(RhhContentKey key)
{
    if (!key) return NULL;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < HabitatCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->habitats[j].id) == key) return &sMods[i].def->habitats[j];
    return NULL;
}
static int CompareHabitats(const void *a, const void *b)
{
    const struct RhhModHabitat *const *left = a, *const *right = b;
    return strcmp((*left)->id, (*right)->id);
}
static int CopyHabitat(const struct RhhModHabitat *habitat, struct RhhHabitatDefinition *out)
{
    if (!sFinalized || !habitat || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhHabitatDefinition result = {.struct_size = sizeof(result), .key = RhhContentKeyOf(habitat->id),
        .map = habitat->map, .all_flags = habitat->all_flags, .any_flags = habitat->any_flags,
        .exclude_flags = habitat->exclude_flags, .period_mask = habitat->period_mask,
        .natural_weather_mask = habitat->natural_weather_mask, .weather_mask = habitat->weather_mask,
        .terrain_mask = habitat->terrain_mask};
    memcpy(result.id, habitat->id, strlen(habitat->id) + 1);
    memcpy(result.label, habitat->label, strlen(habitat->label) + 1);
    memcpy(result.description, habitat->description, strlen(habitat->description) + 1);
    *out = result; return 0;
}
static uint32_t HabitatsCount(void) { return sFinalized ? sHabitatCount : 0; }
static int HabitatsAt(uint32_t index, struct RhhHabitatDefinition *out)
{ return index < sHabitatCount ? CopyHabitat(sHabitats[index], out) : -1; }
static int HabitatsFind(RhhContentKey key, struct RhhHabitatDefinition *out)
{ return CopyHabitat(sFinalized ? FindHabitat(key) : NULL, out); }
static int HabitatsEvaluate(RhhContentKey key, const struct RhhHabitatSample *sample, uint32_t period,
                            struct RhhHabitatEvaluation *out)
{
    const struct RhhModHabitat *habitat = sFinalized ? FindHabitat(key) : NULL;
    if (!habitat || !sample || sample->struct_size < sizeof(*sample) || !out || out->struct_size < sizeof(*out)
        || sample->permitted > 1 || sample->environment_available > 1 || period > RHH_TIME_NIGHT
        || (sample->flags & ~RHH_HABITAT_FLAGS_ALL) || sample->natural_weather > RHH_NATURAL_WEATHER_UNKNOWN
        || (sample->environment_available && (sample->environment.struct_size < sizeof(sample->environment)
            || sample->environment.weather.value > RHH_WEATHER_HEAVY_RAIN
            || sample->environment.terrain.value > RHH_TERRAIN_PSYCHIC))) return -1;
    struct RhhHabitatEvaluation result = {.struct_size = sizeof(result)};
    if (!sample->permitted) result.mismatch |= RHH_HABITAT_REASON_PERMISSION;
    if ((sample->flags & habitat->all_flags) != habitat->all_flags) result.mismatch |= RHH_HABITAT_REASON_ALL_FLAGS;
    if (habitat->any_flags && !(sample->flags & habitat->any_flags)) result.mismatch |= RHH_HABITAT_REASON_ANY_FLAGS;
    if (sample->flags & habitat->exclude_flags) result.mismatch |= RHH_HABITAT_REASON_EXCLUDED_FLAGS;
    if (habitat->period_mask)
    {
        if (!period) result.unavailable |= RHH_HABITAT_REASON_PERIOD;
        else if (!(habitat->period_mask & (1u << (period - 1)))) result.mismatch |= RHH_HABITAT_REASON_PERIOD;
    }
    if (habitat->natural_weather_mask && !(habitat->natural_weather_mask & (1u << sample->natural_weather)))
        result.mismatch |= RHH_HABITAT_REASON_NATURAL_WEATHER;
    if (habitat->weather_mask)
    {
        if (!sample->environment_available) result.unavailable |= RHH_HABITAT_REASON_SAVED_WEATHER;
        else if (!(habitat->weather_mask & (1u << sample->environment.weather.value))) result.mismatch |= RHH_HABITAT_REASON_SAVED_WEATHER;
    }
    if (habitat->terrain_mask)
    {
        if (!sample->environment_available) result.unavailable |= RHH_HABITAT_REASON_SAVED_TERRAIN;
        else if (!(habitat->terrain_mask & (1u << sample->environment.terrain.value))) result.mismatch |= RHH_HABITAT_REASON_SAVED_TERRAIN;
    }
    if (habitat->effective_weather_mask)
    {
        struct RhhHabitatWeather weather = {.struct_size = sizeof(weather)};
        if (RhhModsResolveHabitatWeather(sample, &weather)) return -1;
        if (!weather.saved_available) result.unavailable |= RHH_HABITAT_REASON_EFFECTIVE_WEATHER;
        else if (!(habitat->effective_weather_mask & (1u << weather.kind))) result.mismatch |= RHH_HABITAT_REASON_EFFECTIVE_WEATHER;
    }
    if (habitat->map)
    {
        uint32_t expected, actual;
        if (RhhContentResolve(RHH_MAP, habitat->map, &expected) || RhhContentResolve(RHH_MAP, sample->map, &actual)) return -1;
        if (expected != actual) result.mismatch |= RHH_HABITAT_REASON_MAP;
    }
    result.status = result.mismatch ? RHH_HABITAT_EVALUATION_MISMATCH
        : result.unavailable ? RHH_HABITAT_EVALUATION_INDETERMINATE : RHH_HABITAT_EVALUATION_MATCH;
    *out = result; return 0;
}
static int HabitatsMatch(RhhContentKey key, const struct RhhHabitatSample *sample, uint32_t period, uint32_t *out)
{
    if (!out) return -1;
    struct RhhHabitatEvaluation result = {.struct_size = sizeof(result)};
    if (HabitatsEvaluate(key, sample, period, &result)) return -1;
    *out = result.status == RHH_HABITAT_EVALUATION_MATCH; return 0;
}
static int HabitatEffectiveWeatherMask(RhhContentKey key, uint32_t *out)
{
    const struct RhhModHabitat *habitat = sFinalized ? FindHabitat(key) : NULL;
    if (!habitat || !out) return -1;
    *out = habitat->effective_weather_mask; return 0;
}
static int HabitatsEvaluateAuthored(RhhContentKey key, const struct RhhAuthoredHabitatSample *sample,
                                    struct RhhHabitatEvaluation *out)
{
    const struct RhhModHabitat *habitat = sFinalized ? FindHabitat(key) : NULL;
    if (!habitat || !sample || sample->struct_size < sizeof(*sample) || sample->time_available > 1
        || sample->reserved[0] || sample->reserved[1] || sample->map.struct_size < sizeof(sample->map)
        || sample->map.map != sample->sample.map || sample->map.natural_weather != sample->sample.natural_weather
        || sample->map.engine_weather != sample->sample.engine_weather
        || (sample->time_available && (sample->time.struct_size < sizeof(sample->time)
            || sample->time.period < RHH_TIME_MORNING || sample->time.period > RHH_TIME_NIGHT))
        || !out || out->struct_size < sizeof(*out)) return -1;
    struct RhhHabitatEvaluation result = {.struct_size = sizeof(result)};
    if (HabitatsEvaluate(key, &sample->sample, sample->time_available ? sample->time.period : 0, &result)) return -1;
    if (sample->sample.natural_weather == RHH_NATURAL_WEATHER_DYNAMIC
        || sample->sample.natural_weather == RHH_NATURAL_WEATHER_UNKNOWN)
    {
        if (habitat->natural_weather_mask) {
            result.mismatch &= ~RHH_HABITAT_REASON_NATURAL_WEATHER;
            result.unavailable |= RHH_HABITAT_REASON_NATURAL_WEATHER;
        }
        if (habitat->effective_weather_mask && (!sample->sample.environment_available
            || sample->sample.environment.weather.value == RHH_WEATHER_NONE)) {
            result.mismatch &= ~RHH_HABITAT_REASON_EFFECTIVE_WEATHER;
            result.unavailable |= RHH_HABITAT_REASON_EFFECTIVE_WEATHER;
        }
    }
    result.status = result.mismatch ? RHH_HABITAT_EVALUATION_MISMATCH
        : result.unavailable ? RHH_HABITAT_EVALUATION_INDETERMINATE : RHH_HABITAT_EVALUATION_MATCH;
    *out = result; return 0;
}
static const struct RhhHabitatRegistryService sHabitatRegistryService = {
    {sizeof(sHabitatRegistryService), RHH_SERVICE_VERSION(1, 3)}, HabitatsCount, HabitatsAt, HabitatsFind,
    HabitatsMatch, HabitatEffectiveWeatherMask, HabitatsEvaluate, HabitatsEvaluateAuthored
};
int RhhModsInstallHabitatRegistryService(void)
{ return RhhModsAddService("rhh:service/habitat_registry", &sHabitatRegistryService.header); }
struct SceneLightIndex { uint32_t map; int32_t x, y; const struct RhhModSceneLight *light; };
static struct SceneLightIndex sSceneLightIndex[256];
static unsigned sSceneLightIndexCount;
static int CompareSceneLightIndex(const void *left, const void *right)
{
    const struct SceneLightIndex *a = left, *b = right;
    if (a->map != b->map) return a->map < b->map ? -1 : 1;
    if (a->x != b->x) return a->x < b->x ? -1 : 1;
    if (a->y != b->y) return a->y < b->y ? -1 : 1;
    return strcmp(a->light->id, b->light->id);
}
uint32_t RhhModsSceneLightCount(void) { return sFinalized ? sSceneLightIndexCount : 0; }
static const struct RhhModSceneLight *FindSceneLight(RhhContentKey key)
{
    if (!key) return NULL;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < SceneLightCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->scene_lights[j].id) == key) return &sMods[i].def->scene_lights[j];
    return NULL;
}
int RhhModsSceneLightsForTile(RhhContentKey map, int32_t x, int32_t y, uint32_t period,
                              const struct RhhModSceneLight **out, uint32_t *count)
{
    uint32_t mapId;
    if (!sFinalized || !out || !count || x < 0 || y < 0 || period > RHH_TIME_NIGHT
        || RhhContentResolve(RHH_MAP, map, &mapId)) return -1;
    unsigned lo = 0, hi = sSceneLightIndexCount;
    while (lo < hi)
    {
        unsigned middle = lo + (hi - lo) / 2; const struct SceneLightIndex *entry = &sSceneLightIndex[middle];
        if (entry->map < mapId || (entry->map == mapId && (entry->x < x || (entry->x == x && entry->y < y)))) lo = middle + 1;
        else hi = middle;
    }
    const struct RhhModSceneLight *result[RHH_SCENE_LIGHTS_PER_TILE]; unsigned n = 0;
    for (unsigned i = lo; i < sSceneLightIndexCount; ++i)
    {
        const struct SceneLightIndex *entry = &sSceneLightIndex[i]; const struct RhhModSceneLight *light = entry->light;
        if (entry->map != mapId || entry->x != x || entry->y != y) break;
        if (light->period_mask && (!period || !(light->period_mask & (1u << (period - 1))))) continue;
        if (n == RHH_SCENE_LIGHTS_PER_TILE) return -1;
        result[n++] = light;
    }
    memcpy(out, result, n * sizeof(result[0])); *count = n; return 0;
}
uint32_t RhhModsSceneShadowCasterCount(void)
{
    if (!sFinalized) return 0;
    unsigned count = 0;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < SceneRuleCount(sMods[i].def); ++j)
            if (sMods[i].def->scene_rules[j].style.flags & RHH_SCENE_CASTS_SHADOW) ++count;
    return count;
}
static const struct RhhModSceneRule *FindSceneRule(RhhContentKey key)
{
    if (!key) return NULL;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < SceneRuleCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->scene_rules[j].id) == key) return &sMods[i].def->scene_rules[j];
    return NULL;
}
static unsigned SceneSpecificity(unsigned mask)
{
    unsigned bits = 0;
    while (mask) { bits += mask & 1u; mask >>= 1; }
    return bits;
}
int RhhModsSelectSceneRule(RhhContentKey map, int32_t x, int32_t y, uint32_t metatile,
                           uint32_t behavior, const struct RhhModSceneRule **out)
{
    uint32_t mapId;
    if (!out || !sFinalized || RhhContentResolve(RHH_MAP, map, &mapId)) return -1;
    const struct RhhModSceneRule *best = NULL;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < SceneRuleCount(sMods[i].def); ++j)
        {
            const struct RhhModSceneRule *rule = &sMods[i].def->scene_rules[j];
            if (rule->match & RHH_SCENE_MATCH_MAP)
            {
                uint32_t id;
                if (RhhContentResolve(RHH_MAP, rule->map, &id)) return -1;
                if (id != mapId) continue;
            }
            if ((rule->match & RHH_SCENE_MATCH_POSITION) && (rule->x != x || rule->y != y)) continue;
            if ((rule->match & RHH_SCENE_MATCH_METATILE) && rule->metatile != metatile) continue;
            if ((rule->match & RHH_SCENE_MATCH_BEHAVIOR) && rule->behavior != behavior) continue;
            if (!best || rule->priority > best->priority
                || (rule->priority == best->priority && SceneSpecificity(rule->match) > SceneSpecificity(best->match))
                || (rule->priority == best->priority && SceneSpecificity(rule->match) == SceneSpecificity(best->match)
                    && strcmp(rule->id, best->id) < 0)) best = rule;
        }
    *out = best;
    return 0;
}
uint32_t RhhModsMusicCount(void)
{
    uint32_t count = 0;
    for (unsigned i = 0; i < sModCount; ++i) count += MusicCount(sMods[i].def);
    return count;
}
int RhhModsMusicAt(uint32_t index, RhhContentKey *key, const struct RhhModMusic **out)
{
    if (!key || !out) return -1;
    for (unsigned i = 0; i < sModCount; ++i)
    {
        unsigned count = MusicCount(sMods[i].def);
        if (index < count)
        {
            *out = &sMods[i].def->music[index]; *key = RhhContentKeyOf((*out)->id);
            return 0;
        }
        index -= count;
    }
    return -1;
}
const struct RhhModMusic *RhhModsFindMusic(RhhContentKey key)
{
    if (!key) return NULL;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < MusicCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->music[j].id) == key) return &sMods[i].def->music[j];
    return NULL;
}
static int ValidMusicPath(const char *path)
{
    if (!path || !*path || strlen(path) > 240 || *path == '/') return 0;
    const char *part = path;
    for (const char *p = path;; ++p)
    {
        if (*p == '/' || !*p)
        {
            size_t len = p - part;
            if (!len || (len == 1 && part[0] == '.') || (len == 2 && part[0] == '.' && part[1] == '.')) return 0;
            if (!*p) return 1;
            part = p + 1;
        }
        else if ((unsigned char)*p < 32 || *p == '\\' || *p == ':') return 0;
    }
}
static const struct RhhWorldPresenter *FindWorldPresenter(RhhContentKey key, unsigned *owner)
{
    if (!key) return NULL;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < WorldPresenterCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->world_presenters[j].id) == key)
            { if (owner) *owner = i; return &sMods[i].def->world_presenters[j]; }
    return NULL;
}
static struct RhhWorldLightFrame sWorldLightFrame;
static int sWorldLightCollecting, sWorldLightInvalid, sWorldLightReady, sWorldLightDrawing;
static void PrepareWorldLights(const struct RhhWorldFrame *frame);
int RhhModsDrawWorldWithIdentity(const struct RhhWorldFrame *frame, RhhContentKey *selected)
{
    if (selected) *selected = 0;
    if (sWorldPresentersShutdown || sWorldLightDrawing || sWorldLightCollecting || !frame || frame->struct_size < RHH_WORLD_FRAME_V1_0_SIZE
        || !frame->pixels || !frame->mask || !frame->gl_proc) return 0;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WORLD_PRESENTATION, .run_seed = sRunSeed};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision);
    unsigned owner;
    const struct RhhWorldPresenter *presenter = decision.allowed ? FindWorldPresenter(decision.value, &owner) : NULL;
    if (!presenter) return 0;
    PrepareWorldLights(frame);
    sWorldLightDrawing = 1;
    int result = presenter->draw(frame, sMods[owner].state);
    sWorldLightDrawing = sWorldLightReady = 0;
    if (!result && selected) *selected = decision.value;
    return !result;
}
int RhhModsDrawWorld(const struct RhhWorldFrame *frame)
{ return RhhModsDrawWorldWithIdentity(frame, NULL); }
void RhhModsShutdownWorldPresenters(void)
{
    if (sWorldPresentersShutdown) return;
    sWorldPresentersShutdown = 1;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < WorldPresenterCount(sMods[i].def); ++j)
            if (sMods[i].def->world_presenters[j].shutdown) sMods[i].def->world_presenters[j].shutdown();
}
const struct RhhModShader *RhhModsFindShader(RhhContentKey key)
{
    if (!key) return NULL;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < ShaderCount(sMods[i].def); ++j)
            if (RhhContentKeyOf(sMods[i].def->shaders[j].id) == key) return &sMods[i].def->shaders[j];
    return NULL;
}
static uint32_t ShaderPhotoFlags(RhhContentKey key)
{
    if (!key) return 0;
    for (unsigned i = 0; i < sModCount; ++i)
    {
        const struct RhhModDefinition *def = sMods[i].def;
        for (unsigned j = 0; j < ShaderPhotoPolicyCount(def); ++j)
            if (RhhContentKeyOf(def->shader_photo_policies[j].shader_id) == key)
                return def->shader_photo_policies[j].flags;
    }
    return 0;
}
int RhhModsShaderPhotoPreservesPosition(RhhContentKey key)
{ return !!(ShaderPhotoFlags(key) & RHH_SHADER_PHOTO_PRESERVES_POSITION); }
int RhhModsShaderPhotoPreservesGeometry(RhhContentKey key)
{ return !!(ShaderPhotoFlags(key) & (RHH_SHADER_PHOTO_PRESERVES_POSITION | RHH_SHADER_PHOTO_PRESERVES_GEOMETRY)); }
static int WorldShaderPhotoFlags(RhhContentKey key, uint32_t *out)
{
    if (!out || !RhhModsFindShader(key)) return -1;
    *out = ShaderPhotoFlags(key); return 0;
}
static struct RhhWorldRenderPass sWorldPasses[RHH_WORLD_RENDER_PASS_CAPACITY];
static unsigned sWorldPassCount;
static int sWorldPassCollecting, sWorldPassInvalid;
static int OfferWorldPass(const struct RhhWorldRenderPass *pass)
{
    if (!sWorldPassCollecting || RhhModsCurrentEvent() != RHH_WORLD_RENDER_PASSES || !pass
        || pass->struct_size < sizeof(*pass) || pass->reserved || pass->reserved2
        || !pass->id || !RhhModsFindShader(pass->shader)) return -1;
    for (unsigned i = 0; i < sWorldPassCount; ++i)
        if (sWorldPasses[i].id == pass->id) { sWorldPassInvalid = 1; return -1; }
    if (sWorldPassCount == RHH_WORLD_RENDER_PASS_CAPACITY) { sWorldPassInvalid = 1; return -1; }
    sWorldPasses[sWorldPassCount++] = *pass; return 0;
}
static int CompareWorldPasses(const void *left, const void *right)
{
    const struct RhhWorldRenderPass *a = left, *b = right;
    if (a->priority != b->priority) return a->priority < b->priority ? -1 : 1;
    return (a->id > b->id) - (a->id < b->id);
}
unsigned RhhModsCollectWorldPasses(uint32_t frame, uint32_t weather, uint32_t terrain, struct RhhWorldRenderPass *out)
{
    if (!out || sWorldPassCollecting || !RhhModsHasActiveRun()) return 0;
    sWorldPassCount = 0; sWorldPassInvalid = 0; sWorldPassCollecting = 1;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WORLD_RENDER_PASSES,
        .run_seed = RhhModsRunSeed(), .ordinal = frame, .args = {weather, terrain}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision);
    sWorldPassCollecting = 0;
    if (!decision.allowed || sWorldPassInvalid) return 0;
    qsort(sWorldPasses, sWorldPassCount, sizeof(sWorldPasses[0]), CompareWorldPasses);
    memcpy(out, sWorldPasses, sWorldPassCount * sizeof(*out)); return sWorldPassCount;
}
static struct RhhBattleRenderPass sBattlePasses[RHH_BATTLE_RENDER_PASS_CAPACITY];
static unsigned sBattlePassCount;
static int sBattlePassCollecting, sBattlePassInvalid;
static int OfferBattlePass(const struct RhhBattleRenderPass *pass)
{
    if (!sBattlePassCollecting || RhhModsCurrentEvent() != RHH_BATTLE_RENDER_PASSES || !pass
        || pass->struct_size < sizeof(*pass) || pass->reserved || pass->reserved2
        || !pass->id || !RhhModsFindShader(pass->shader)) return -1;
    for (unsigned i = 0; i < sBattlePassCount; ++i)
        if (sBattlePasses[i].id == pass->id) { sBattlePassInvalid = 1; return -1; }
    if (sBattlePassCount == RHH_BATTLE_RENDER_PASS_CAPACITY) { sBattlePassInvalid = 1; return -1; }
    sBattlePasses[sBattlePassCount++] = *pass; return 0;
}
static int CompareBattlePasses(const void *left, const void *right)
{
    const struct RhhBattleRenderPass *a = left, *b = right;
    if (a->priority != b->priority) return a->priority < b->priority ? -1 : 1;
    return (a->id > b->id) - (a->id < b->id);
}
unsigned RhhModsCollectBattlePasses(uint32_t frame, uint32_t weather, uint32_t terrain,
                                   uint32_t suppressed, uint32_t doubles, struct RhhBattleRenderPass *out)
{
    if (!out || sBattlePassCollecting || !RhhModsHasActiveRun()) return 0;
    sBattlePassCount = 0; sBattlePassInvalid = 0; sBattlePassCollecting = 1;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_BATTLE_RENDER_PASSES,
        .run_seed = RhhModsRunSeed(), .ordinal = frame, .args = {weather, terrain, suppressed, doubles}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision); sBattlePassCollecting = 0;
    if (!decision.allowed || sBattlePassInvalid) return 0;
    qsort(sBattlePasses, sBattlePassCount, sizeof(sBattlePasses[0]), CompareBattlePasses);
    memcpy(out, sBattlePasses, sBattlePassCount * sizeof(*out)); return sBattlePassCount;
}
static int LightContext(struct RhhWorldLightContext *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWorldLightCollecting
        || RhhModsCurrentEvent() != RHH_WORLD_LIGHTS) return -1;
    *out = sWorldLightFrame.context; return 0;
}
static int OfferDynamicLight(const struct RhhWorldDynamicLight *light)
{
    if (!sWorldLightCollecting || RhhModsCurrentEvent() != RHH_WORLD_LIGHTS) return -1;
    uint32_t sourceMap, currentMap;
    if (!light || light->struct_size < sizeof(*light) || light->reserved || light->reserved2 || !light->id
        || RhhContentResolve(RHH_MAP, light->map, &sourceMap)
        || RhhContentResolve(RHH_MAP, sWorldLightFrame.context.lighting.map, &currentMap) || sourceMap != currentMap
        || light->x < -1000000 || light->x > 1000000 || light->y < -1000000 || light->y > 1000000
        || light->z < -2048 || light->z > 2304 || !light->radius || light->radius > 256
        || !light->rgb || (light->rgb & 0xff000000u) || !light->intensity || light->intensity > 1000
        || FindSceneLight(light->id))
    { sWorldLightInvalid = 1; return -1; }
    unsigned pos = 0;
    while (pos < sWorldLightFrame.count && sWorldLightFrame.lights[pos].id < light->id) ++pos;
    if (sWorldLightFrame.count == RHH_WORLD_DYNAMIC_LIGHT_CAPACITY
        || (pos < sWorldLightFrame.count && sWorldLightFrame.lights[pos].id == light->id))
    { sWorldLightInvalid = 1; return -1; }
    memmove(&sWorldLightFrame.lights[pos + 1], &sWorldLightFrame.lights[pos],
            (sWorldLightFrame.count - pos) * sizeof(sWorldLightFrame.lights[0]));
    sWorldLightFrame.lights[pos] = *light;
    sWorldLightFrame.lights[pos].map = sWorldLightFrame.context.lighting.map;
    ++sWorldLightFrame.count; return 0;
}
static int FrameLights(struct RhhWorldLightFrame *out)
{
    if (!out || out->struct_size < sizeof(*out) || !sWorldLightDrawing || !sWorldLightReady) return -1;
    *out = sWorldLightFrame; return 0;
}
static void PrepareWorldLights(const struct RhhWorldFrame *frame)
{
    sWorldLightReady = 0;
    if (!RhhModsHasActiveRun() || !RhhModsHasHook(RHH_WORLD_LIGHTS)) return;
    const struct RhhOverworldService *world = (const void *)GetService("rhh:service/overworld", RHH_SERVICE_VERSION(1, 5));
    if (!world || world->header.struct_size < RHH_OVERWORLD_SERVICE_V1_5_SIZE || !world->camera || !world->lighting_inputs) return;
    memset(&sWorldLightFrame, 0, sizeof(sWorldLightFrame));
    sWorldLightFrame.struct_size = sizeof(sWorldLightFrame);
    sWorldLightFrame.context.struct_size = sizeof(sWorldLightFrame.context); sWorldLightFrame.context.frame = frame->frame;
    sWorldLightFrame.context.camera.struct_size = sizeof(sWorldLightFrame.context.camera);
    sWorldLightFrame.context.lighting.struct_size = sizeof(sWorldLightFrame.context.lighting);
    if (world->camera(&sWorldLightFrame.context.camera) || world->lighting_inputs(&sWorldLightFrame.context.lighting)
        || !sWorldLightFrame.context.lighting.map
        || sWorldLightFrame.context.camera.map != sWorldLightFrame.context.lighting.map) return;
    sWorldLightCollecting = 1; sWorldLightInvalid = 0;
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WORLD_LIGHTS, .run_seed = sRunSeed,
        .event_key = sWorldLightFrame.context.lighting.map, .ordinal = frame->frame,
        .args = {sWorldLightFrame.context.lighting.time_available ? sWorldLightFrame.context.lighting.time.period : 0,
                 sWorldLightFrame.context.lighting.environment_available ? sWorldLightFrame.context.lighting.environment.weather.value : 0}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision); sWorldLightCollecting = 0;
    if (!decision.allowed) { sWorldLightFrame.status = RHH_WORLD_LIGHTS_DENIED; sWorldLightFrame.count = 0; }
    else if (sWorldLightInvalid) { sWorldLightFrame.status = RHH_WORLD_LIGHTS_INVALID; sWorldLightFrame.count = 0; }
    if (sWorldLightFrame.status != RHH_WORLD_LIGHTS_OK) memset(sWorldLightFrame.lights, 0, sizeof(sWorldLightFrame.lights));
    sWorldLightReady = 1;
}
static const struct RhhWorldRenderService sWorldRenderService = {
    {sizeof(sWorldRenderService), RHH_SERVICE_VERSION(1, 2)}, OfferWorldPass, LightContext, OfferDynamicLight, FrameLights,
    WorldShaderPhotoFlags
};
static const struct RhhBattleRenderService sBattleRenderService = {
    {sizeof(sBattleRenderService), RHH_SERVICE_VERSION(1, 0)}, OfferBattlePass
};
int RhhModsInstallRenderService(void)
{
    if (RhhModsAddService("rhh:service/world_render", &sWorldRenderService.header)) return -1;
    return RhhModsAddService("rhh:service/battle_render", &sBattleRenderService.header);
}
const char *RhhModsError(void) { return sError; }
void RhhModsSetTrace(FILE *file) { sTrace = file; sTraceSequence = 0; memset(sLastTrace, 0, sizeof(sLastTrace)); }
void RhhModsShutdown(void)
{
    RunLifecycleCallbacks();
    memset(sLifecycleCallbacks, 0, sizeof(sLifecycleCallbacks)); sLifecycleCount = 0;
    sDrawing = 0;
    for (unsigned i = 0; i < sModCount; ++i) free(sMods[i].state);
    memset(sMods, 0, sizeof(sMods));
    sModCount = sHookCount = sFinalized = 0; sSceneLightIndexCount = 0;
    sEcologyTagCount = sSpeciesTagCount = sCustomTypeCount = sCustomMoveCount = sLearnsetCount = sBattleConditionCount = 0;
    sHabitatCount = 0;
    sMoveEffectCount = sMoveEffectBindingCount = sCustomAbilityCount = 0; sMoveEffectValidator = NULL;
    sServiceCount = 0; memset(sServices, 0, sizeof(sServices));
    sWorldPresentersShutdown = 0;
    sWorldPassCount = 0; sWorldPassCollecting = sWorldPassInvalid = 0;
    sBattlePassCount = 0; sBattlePassCollecting = sBattlePassInvalid = 0;
    sWorldLightCollecting = sWorldLightInvalid = sWorldLightReady = sWorldLightDrawing = 0;
    memset(&sWorldLightFrame, 0, sizeof(sWorldLightFrame));
    sDispatchDepth = sDispatchEvent = sDispatchStage = 0; sDispatchOwner = 0; sUiInputOwnerProvider = NULL;
    memset(sLastTrace, 0, sizeof(sLastTrace));
    sContent = NULL;
    sContentCount = 0;
    sMetadata = NULL; sPersist = NULL; memset(sBeforeSave, 0, sizeof(sBeforeSave)); sBeforeSaveCount = 0; sSaveProjection = NULL;
    sRunSeed = sRunId = 0; sCommitted = sSettingsLocked = 0;
    free(sContentIndex); sContentIndex = NULL;
}
void RhhModsInit(void) { RhhModsShutdown(); sError[0] = 0; }
int RhhModsSetBeforeSave(void (*callback)(void))
{
    if (sFinalized || !callback || sBeforeSaveCount >= sizeof(sBeforeSave) / sizeof(sBeforeSave[0]))
        return Fail("invalid before-save callback");
    for (unsigned i = 0; i < sBeforeSaveCount; ++i)
        if (sBeforeSave[i] == callback) return Fail("duplicate before-save callback");
    sBeforeSave[sBeforeSaveCount++] = callback;
    return 0;
}
void RhhModsPrepareSave(void) { for (unsigned i = 0; i < sBeforeSaveCount; ++i) sBeforeSave[i](); }
int RhhModsSetSaveProjection(int (*callback)(const char *, const uint8_t *, uint32_t, uint32_t *, uint32_t *))
{
    if (sFinalized || sSaveProjection || !callback) return Fail("invalid save projection callback");
    sSaveProjection = callback; return 0;
}
int RhhModsRegisteredKey(RhhContentKey key)
{
    unsigned matches = 0;
    if (!key) return 0;
    for (unsigned i = 0; i < sModCount; ++i)
        if (RhhContentKeyOf(sMods[i].def->id) == key) ++matches;
    return matches == 1;
}
static int FindMod(const char *id)
{
    if (!id) return -1;
    for (unsigned i = 0; i < sModCount; ++i)
        if (!strcmp(sMods[i].def->id, id)) return i;
    return -1;
}
int RhhModsGetSetting(const char *id, const char *setting, struct RhhModValue *out)
{
    int m = FindMod(id);
    if (m < 0 || !setting || !sMods[m].def->read_setting || !out) return -1;
    return sMods[m].def->read_setting(sMods[m].state, setting, out);
}
int RhhModsConfigure(const char *id, const char *setting, const struct RhhModValue *value)
{
    int m = FindMod(id);
    if (m < 0 || !setting || !value || value->reserved) return Fail("unknown module or invalid setting value");
    const struct RhhModDefinition *d = sMods[m].def;
    const struct RhhModSetting *found = NULL;
    for (unsigned i = 0; i < d->setting_count; ++i)
        if (!strcmp(setting, d->settings[i].id)) found = &d->settings[i];
    if (!found || !d->write_setting || found->type != value->type
        || (sSettingsLocked && found->scope == RHH_SETTING_NEW_GAME)) return Fail("setting is unavailable or locked");
    if (found->type != RHH_SETTING_CONTENT && (value->value < found->minimum || value->value > found->maximum))
        return Fail("setting value is out of range");
    uint8_t *staged = malloc(d->state_size ? d->state_size : 1);
    if (!staged) return Fail("cannot stage setting change");
    memcpy(staged, sMods[m].state, d->state_size);
    int result = d->write_setting(staged, setting, value);
    if (!result && d->validate) result = d->validate(staged, d->state_size);
    if (!result) memcpy(sMods[m].state, staged, d->state_size);
    free(staged);
    if (result) return Fail("setting rejected by %s", id);
    if (sTrace) fprintf(sTrace, "{\"event\":\"setting\",\"mod\":\"%s\",\"setting\":\"%s\",\"value\":%" PRIu64 "}\n", id, setting, value->value);
    return 0;
}
int RhhModsRegister(const struct RhhModDefinition *def)
{
    if (sFinalized) return Fail("registration is closed");
    if (!def || def->struct_size < RHH_MOD_DEFINITION_BASE_SIZE
        || (def->abi_version != RHH_MOD_ABI_V1 && def->abi_version != RHH_MOD_ABI_SERVICES
            && def->abi_version != RHH_MOD_ABI_MUSIC && def->abi_version != RHH_MOD_ABI_SCENE
            && def->abi_version != RHH_MOD_ABI_SCENE_LIGHTS && def->abi_version != RHH_MOD_ABI_ECOLOGY
            && def->abi_version != RHH_MOD_ABI_TYPES && def->abi_version != RHH_MOD_ABI_MOVES && def->abi_version != RHH_MOD_ABI_LEARNSETS && def->abi_version != RHH_MOD_ABI_CONDITIONS && def->abi_version != RHH_MOD_ABI_MOVE_EFFECTS && def->abi_version != RHH_MOD_ABI_DAMAGE_EFFECTS && (def->abi_version != RHH_MOD_ABI_ABILITIES && def->abi_version != RHH_MOD_ABI_HABITATS))
        || (def->struct_size > RHH_MOD_DEFINITION_BASE_SIZE && def->struct_size < RHH_MOD_DEFINITION_SHADER_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_SHADER_SIZE && def->struct_size < RHH_MOD_DEFINITION_WORLD_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_WORLD_SIZE && def->struct_size < RHH_MOD_DEFINITION_SERVICE_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_SERVICE_SIZE && def->struct_size < RHH_MOD_DEFINITION_MUSIC_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_MUSIC_SIZE && def->struct_size < RHH_MOD_DEFINITION_SCENE_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_SCENE_SIZE && def->struct_size < RHH_MOD_DEFINITION_SCENE_LIGHT_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_SCENE_LIGHT_SIZE && def->struct_size < RHH_MOD_DEFINITION_ECOLOGY_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_ECOLOGY_SIZE && def->struct_size < RHH_MOD_DEFINITION_TYPES_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_TYPES_SIZE && def->struct_size < RHH_MOD_DEFINITION_MOVES_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_MOVES_SIZE && def->struct_size < RHH_MOD_DEFINITION_LEARNSETS_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_LEARNSETS_SIZE && def->struct_size < RHH_MOD_DEFINITION_CONDITIONS_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_CONDITIONS_SIZE && def->struct_size < RHH_MOD_DEFINITION_MOVE_EFFECTS_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_MOVE_EFFECTS_SIZE && def->struct_size < RHH_MOD_DEFINITION_ABILITIES_SIZE)
        || (def->abi_version == RHH_MOD_ABI_SERVICES && def->struct_size < RHH_MOD_DEFINITION_SERVICE_SIZE)
        || (def->abi_version == RHH_MOD_ABI_MUSIC && def->struct_size < RHH_MOD_DEFINITION_MUSIC_SIZE)
        || (def->abi_version == RHH_MOD_ABI_SCENE && def->struct_size < RHH_MOD_DEFINITION_SCENE_SIZE)
        || (def->abi_version == RHH_MOD_ABI_SCENE_LIGHTS && def->struct_size < RHH_MOD_DEFINITION_SCENE_LIGHT_SIZE)
        || (def->abi_version == RHH_MOD_ABI_ECOLOGY && def->struct_size < RHH_MOD_DEFINITION_ECOLOGY_SIZE)
        || (def->abi_version == RHH_MOD_ABI_TYPES && def->struct_size < RHH_MOD_DEFINITION_TYPES_SIZE)
        || (def->abi_version == RHH_MOD_ABI_MOVES && def->struct_size < RHH_MOD_DEFINITION_MOVES_SIZE)
        || (def->abi_version == RHH_MOD_ABI_LEARNSETS && def->struct_size < RHH_MOD_DEFINITION_LEARNSETS_SIZE)
        || (def->abi_version == RHH_MOD_ABI_CONDITIONS && def->struct_size < RHH_MOD_DEFINITION_CONDITIONS_SIZE)
        || ((def->abi_version == RHH_MOD_ABI_MOVE_EFFECTS || def->abi_version == RHH_MOD_ABI_DAMAGE_EFFECTS) && def->struct_size < RHH_MOD_DEFINITION_MOVE_EFFECTS_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_ABILITIES_SIZE && def->struct_size < RHH_MOD_DEFINITION_HABITATS_SIZE)
        || (def->abi_version == RHH_MOD_ABI_ABILITIES && def->struct_size < RHH_MOD_DEFINITION_ABILITIES_SIZE)
        || (def->abi_version == RHH_MOD_ABI_HABITATS && def->struct_size < RHH_MOD_DEFINITION_HABITATS_SIZE)
        || (def->struct_size > RHH_MOD_DEFINITION_HABITATS_SIZE && def->struct_size < RHH_MOD_DEFINITION_SHADER_PHOTO_SIZE))
        return Fail("unsupported mod ABI");
    if (!ValidName(def->id) || FindMod(def->id) >= 0) return Fail("invalid or duplicate mod id");
    if (def->flags & ~(RHH_MOD_REQUIRED | RHH_MOD_DURABLE)) return Fail("unsupported mod flags");
    unsigned shaders = ShaderCount(def);
    if (shaders > 8 || (shaders && !def->shaders)) return Fail("invalid shader declarations");
    for (unsigned i = 0; i < shaders; ++i)
    {
        const struct RhhModShader *shader = &def->shaders[i];
        if (shader->struct_size < sizeof(*shader) || !ValidName(shader->id) || !shader->fragment
            || !*shader->fragment || strlen(shader->fragment) > 65536
            || RhhModsFindShader(RhhContentKeyOf(shader->id))) return Fail("invalid or duplicate shader");
        for (unsigned j = 0; j < i; ++j)
            if (RhhContentKeyOf(def->shaders[j].id) == RhhContentKeyOf(shader->id)) return Fail("duplicate shader key");
    }
    unsigned photoPolicies = ShaderPhotoPolicyCount(def);
    if (photoPolicies > shaders || (photoPolicies && !def->shader_photo_policies)
        || (def->struct_size >= RHH_MOD_DEFINITION_SHADER_PHOTO_SIZE && def->shader_photo_policy_reserved))
        return Fail("invalid shader photo policies in %s", def->id);
    for (unsigned i = 0; i < photoPolicies; ++i)
    {
        const struct RhhModShaderPhotoPolicy *policy = &def->shader_photo_policies[i];
        if (policy->struct_size < sizeof(*policy) || !policy->flags
            || (policy->flags & ~(RHH_SHADER_PHOTO_PRESERVES_POSITION | RHH_SHADER_PHOTO_PRESERVES_GEOMETRY))
            || !ValidName(policy->shader_id)) return Fail("invalid shader photo policy in %s", def->id);
        unsigned j;
        for (j = 0; j < shaders; ++j) if (!strcmp(policy->shader_id, def->shaders[j].id)) break;
        if (j == shaders) return Fail("shader photo policy must refer to an owned shader in %s", def->id);
        for (j = 0; j < i; ++j)
            if (!strcmp(policy->shader_id, def->shader_photo_policies[j].shader_id))
                return Fail("duplicate shader photo policy in %s", def->id);
    }
    unsigned presenters = WorldPresenterCount(def);
    if (presenters > 8 || (presenters && !def->world_presenters)) return Fail("invalid world presenter declarations");
    for (unsigned i = 0; i < presenters; ++i)
    {
        const struct RhhWorldPresenter *p = &def->world_presenters[i];
        if (p->struct_size < sizeof(*p) || !ValidName(p->id) || !p->draw
            || FindWorldPresenter(RhhContentKeyOf(p->id), NULL)) return Fail("invalid or duplicate world presenter");
        for (unsigned j = 0; j < i; ++j)
            if (RhhContentKeyOf(def->world_presenters[j].id) == RhhContentKeyOf(p->id)) return Fail("duplicate world presenter key");
    }
    unsigned requirements = ServiceRequirementCount(def);
    if (requirements > 32 || (requirements && (!def->service_requirements || def->abi_version == RHH_MOD_ABI_V1))
        || (def->struct_size >= RHH_MOD_DEFINITION_SERVICE_SIZE && def->service_requirement_reserved))
        return Fail("invalid service requirements in %s", def->id);
    for (unsigned i = 0; i < requirements; ++i)
    {
        const struct RhhServiceRequirement *req = &def->service_requirements[i];
        if (req->struct_size < sizeof(*req) || !ValidName(req->id) || !(req->minimum_version >> 16)
            || (req->flags & ~RHH_SERVICE_REQUIRED) || req->reserved)
            return Fail("invalid service requirement in %s", def->id);
        for (unsigned j = 0; j < i; ++j)
            if (!strcmp(req->id, def->service_requirements[j].id))
                return Fail("duplicate service requirement in %s", def->id);
    }
    unsigned music = MusicCount(def);
    if (music > 32 || (music && (!def->music || (def->abi_version != RHH_MOD_ABI_MUSIC && def->abi_version != RHH_MOD_ABI_SCENE
            && def->abi_version != RHH_MOD_ABI_SCENE_LIGHTS && def->abi_version != RHH_MOD_ABI_ECOLOGY
            && def->abi_version != RHH_MOD_ABI_TYPES && def->abi_version != RHH_MOD_ABI_MOVES && def->abi_version != RHH_MOD_ABI_LEARNSETS && def->abi_version != RHH_MOD_ABI_CONDITIONS && def->abi_version != RHH_MOD_ABI_MOVE_EFFECTS && def->abi_version != RHH_MOD_ABI_DAMAGE_EFFECTS && (def->abi_version != RHH_MOD_ABI_ABILITIES && def->abi_version != RHH_MOD_ABI_HABITATS))))
        || (def->struct_size >= RHH_MOD_DEFINITION_MUSIC_SIZE && def->music_reserved))
        return Fail("invalid music declarations in %s", def->id);
    for (unsigned i = 0; i < music; ++i)
    {
        const struct RhhModMusic *track = &def->music[i];
        RhhContentKey key = track->id ? RhhContentKeyOf(track->id) : 0;
        uint32_t ignored;
        if (track->struct_size < sizeof(*track) || track->reserved || !ValidName(track->id)
            || !track->label || !*track->label || strlen(track->label) > 127
            || !ValidMusicPath(track->midi_path) || !ValidMusicPath(track->soundfont_path)
            || RhhModsFindMusic(key) || !RhhContentResolve(RHH_MIDI, key, &ignored))
            return Fail("invalid or duplicate music in %s", def->id);
        for (unsigned j = 0; j < i; ++j)
            if (RhhContentKeyOf(def->music[j].id) == key) return Fail("duplicate music key in %s", def->id);
    }
    unsigned sceneRules = SceneRuleCount(def), existingSceneRules = 0;
    for (unsigned i = 0; i < sModCount; ++i) existingSceneRules += SceneRuleCount(sMods[i].def);
    if (sceneRules > 256 || sceneRules > MAX_SCENE_RULES - existingSceneRules
        || (sceneRules && (!def->scene_rules || (def->abi_version != RHH_MOD_ABI_SCENE
            && def->abi_version != RHH_MOD_ABI_SCENE_LIGHTS && def->abi_version != RHH_MOD_ABI_ECOLOGY
            && def->abi_version != RHH_MOD_ABI_TYPES && def->abi_version != RHH_MOD_ABI_MOVES && def->abi_version != RHH_MOD_ABI_LEARNSETS && def->abi_version != RHH_MOD_ABI_CONDITIONS && def->abi_version != RHH_MOD_ABI_MOVE_EFFECTS && def->abi_version != RHH_MOD_ABI_DAMAGE_EFFECTS && (def->abi_version != RHH_MOD_ABI_ABILITIES && def->abi_version != RHH_MOD_ABI_HABITATS))))
        || (def->struct_size >= RHH_MOD_DEFINITION_SCENE_SIZE && def->scene_rule_reserved))
        return Fail("invalid scene declarations in %s", def->id);
    for (unsigned i = 0; i < sceneRules; ++i)
    {
        const struct RhhModSceneRule *rule = &def->scene_rules[i];
        const struct RhhSceneStyle *style = &rule->style;
        uint32_t mapId;
        if (rule->struct_size < sizeof(*rule) || !ValidName(rule->id) || !RhhContentKeyOf(rule->id)
            || rule->reserved[0] || rule->reserved[1]
            || (rule->match & ~(RHH_SCENE_MATCH_MAP | RHH_SCENE_MATCH_METATILE | RHH_SCENE_MATCH_BEHAVIOR | RHH_SCENE_MATCH_POSITION))
            || ((rule->match & RHH_SCENE_MATCH_POSITION) && !(rule->match & RHH_SCENE_MATCH_MAP))
            || ((rule->match & RHH_SCENE_MATCH_MAP) ? RhhContentResolve(RHH_MAP, rule->map, &mapId) : rule->map != 0)
            || ((rule->match & RHH_SCENE_MATCH_POSITION) ? (rule->x < 0 || rule->y < 0 || rule->x > 65535 || rule->y > 65535) : (rule->x || rule->y))
            || ((rule->match & RHH_SCENE_MATCH_METATILE) ? rule->metatile > 1023 : rule->metatile != 0)
            || ((rule->match & RHH_SCENE_MATCH_BEHAVIOR) ? rule->behavior > 65535 : rule->behavior != 0)
            || style->surface > RHH_SCENE_WATER || style->upper > RHH_SCENE_UPPER_HIDDEN
            || (style->flags & ~(RHH_SCENE_RECEIVES_SHADOW | RHH_SCENE_CASTS_SHADOW | RHH_SCENE_OCCLUDES))
            || style->ground_height < -2048 || style->ground_height > 2048 || style->upper_height < 0 || style->upper_height > 2048
            || ((style->upper == RHH_SCENE_UPPER_FLAT || style->upper == RHH_SCENE_UPPER_HIDDEN) ? style->upper_height != 0 : style->upper_height == 0)
            || FindSceneRule(RhhContentKeyOf(rule->id)))
            return Fail("invalid or duplicate scene rule in %s", def->id);
        for (unsigned j = 0; j < i; ++j)
            if (RhhContentKeyOf(def->scene_rules[j].id) == RhhContentKeyOf(rule->id))
                return Fail("duplicate scene rule key in %s", def->id);
    }
    unsigned sceneLights = SceneLightCount(def), existingSceneLights = 0;
    for (unsigned i = 0; i < sModCount; ++i) existingSceneLights += SceneLightCount(sMods[i].def);
    if (sceneLights > 64 || sceneLights > 256 - existingSceneLights
        || (sceneLights && (!def->scene_lights || (def->abi_version != RHH_MOD_ABI_SCENE_LIGHTS
            && def->abi_version != RHH_MOD_ABI_ECOLOGY && def->abi_version != RHH_MOD_ABI_TYPES && def->abi_version != RHH_MOD_ABI_MOVES && def->abi_version != RHH_MOD_ABI_LEARNSETS && def->abi_version != RHH_MOD_ABI_CONDITIONS && def->abi_version != RHH_MOD_ABI_MOVE_EFFECTS && def->abi_version != RHH_MOD_ABI_DAMAGE_EFFECTS && (def->abi_version != RHH_MOD_ABI_ABILITIES && def->abi_version != RHH_MOD_ABI_HABITATS))))
        || (def->struct_size >= RHH_MOD_DEFINITION_SCENE_LIGHT_SIZE && def->scene_light_reserved))
        return Fail("invalid scene light declarations in %s", def->id);
    for (unsigned i = 0; i < sceneLights; ++i)
    {
        const struct RhhModSceneLight *light = &def->scene_lights[i]; uint32_t mapId;
        RhhContentKey key = light->id ? RhhContentKeyOf(light->id) : 0;
        if (light->struct_size < sizeof(*light) || !ValidName(light->id) || !key
            || light->reserved[0] || light->reserved[1] || (light->period_mask & ~15u)
            || RhhContentResolve(RHH_MAP, light->map, &mapId) || light->x < 0 || light->y < 0
            || light->x > 1000000 || light->y > 1000000 || light->height < 0 || light->height > 256
            || !light->radius || light->radius > 256 || !light->rgb || (light->rgb & 0xff000000u)
            || !light->intensity || light->intensity > 1000 || FindSceneLight(key))
            return Fail("invalid or duplicate scene light in %s", def->id);
        unsigned sameTile = 0;
        for (unsigned j = 0; j < i; ++j)
        {
            uint32_t otherMap;
            if (RhhContentKeyOf(def->scene_lights[j].id) == key) return Fail("duplicate scene light key in %s", def->id);
            if (!RhhContentResolve(RHH_MAP, def->scene_lights[j].map, &otherMap) && otherMap == mapId
                && def->scene_lights[j].x / 16 == light->x / 16 && def->scene_lights[j].y / 16 == light->y / 16) ++sameTile;
        }
        for (unsigned j = 0; j < sModCount; ++j)
            for (unsigned k = 0; k < SceneLightCount(sMods[j].def); ++k)
            {
                const struct RhhModSceneLight *other = &sMods[j].def->scene_lights[k]; uint32_t otherMap;
                if (!RhhContentResolve(RHH_MAP, other->map, &otherMap) && otherMap == mapId
                    && other->x / 16 == light->x / 16 && other->y / 16 == light->y / 16) ++sameTile;
            }
        if (sameTile >= RHH_SCENE_LIGHTS_PER_TILE) return Fail("too many scene lights on a tile in %s", def->id);
    }
    unsigned habitats = HabitatCount(def), existingHabitats = 0;
    for (unsigned i = 0; i < sModCount; ++i) existingHabitats += HabitatCount(sMods[i].def);
    if (habitats > 128 || habitats > MAX_HABITATS - existingHabitats
        || (habitats && (!def->habitats || def->abi_version != RHH_MOD_ABI_HABITATS))
        || (def->struct_size >= RHH_MOD_DEFINITION_HABITATS_SIZE && def->habitat_reserved))
        return Fail("invalid habitat declarations in %s", def->id);
    for (unsigned i = 0; i < habitats; ++i)
    {
        const struct RhhModHabitat *habitat = &def->habitats[i];
        RhhContentKey key = habitat->id ? RhhContentKeyOf(habitat->id) : 0;
        uint32_t map;
        if (habitat->struct_size < sizeof(*habitat) || habitat->reserved
            || !ValidName(habitat->id) || !key || !habitat->label || !*habitat->label
            || strlen(habitat->label) >= 64 || !habitat->description || strlen(habitat->description) >= 128
            || ((habitat->all_flags | habitat->any_flags | habitat->exclude_flags) & ~RHH_HABITAT_FLAGS_ALL)
            || (habitat->all_flags & habitat->exclude_flags)
            || (habitat->any_flags && !(habitat->any_flags & ~habitat->exclude_flags))
            || (habitat->period_mask & ~15u)
            || (habitat->natural_weather_mask & ~((1u << (RHH_NATURAL_WEATHER_UNKNOWN + 1)) - 1))
            || (habitat->weather_mask & ~((1u << (RHH_WEATHER_HEAVY_RAIN + 1)) - 1))
            || (habitat->terrain_mask & ~((1u << (RHH_TERRAIN_PSYCHIC + 1)) - 1))
            || (habitat->effective_weather_mask & ~((1u << (RHH_HABITAT_WEATHER_UNKNOWN + 1)) - 1))
            || (habitat->map && RhhContentResolve(RHH_MAP, habitat->map, &map)) || FindHabitat(key))
            return Fail("invalid or duplicate habitat in %s", def->id);
        for (unsigned j = 0; j < i; ++j)
            if (RhhContentKeyOf(def->habitats[j].id) == key) return Fail("duplicate habitat in %s", def->id);
    }
    unsigned tags = EcologyTagCount(def), bindings = SpeciesTagCount(def), existingTags = 0, existingBindings = 0;
    for (unsigned i = 0; i < sModCount; ++i)
    { existingTags += EcologyTagCount(sMods[i].def); existingBindings += SpeciesTagCount(sMods[i].def); }
    if (tags > 128 || bindings > 1024 || tags > MAX_ECOLOGY_TAGS - existingTags
        || bindings > MAX_SPECIES_TAGS - existingBindings
        || ((tags || bindings) && def->abi_version != RHH_MOD_ABI_ECOLOGY && def->abi_version != RHH_MOD_ABI_TYPES && def->abi_version != RHH_MOD_ABI_MOVES && def->abi_version != RHH_MOD_ABI_LEARNSETS && def->abi_version != RHH_MOD_ABI_CONDITIONS && def->abi_version != RHH_MOD_ABI_MOVE_EFFECTS && def->abi_version != RHH_MOD_ABI_DAMAGE_EFFECTS && (def->abi_version != RHH_MOD_ABI_ABILITIES && def->abi_version != RHH_MOD_ABI_HABITATS))
        || (tags && !def->ecology_tags) || (bindings && !def->species_tags)
        || (def->struct_size >= RHH_MOD_DEFINITION_ECOLOGY_SIZE && (def->ecology_tag_reserved || def->species_tag_reserved)))
        return Fail("invalid ecology declarations in %s", def->id);
    for (unsigned i = 0; i < tags; ++i)
    {
        const struct RhhModEcologyTag *tag = &def->ecology_tags[i];
        RhhContentKey key = tag->id ? RhhContentKeyOf(tag->id) : 0;
        if (tag->struct_size < sizeof(*tag) || tag->reserved || !ValidName(tag->id) || !key
            || !tag->label || !*tag->label || strlen(tag->label) >= 64
            || !tag->description || strlen(tag->description) >= 128 || FindEcologyTag(key))
            return Fail("invalid or duplicate ecology tag in %s", def->id);
        for (unsigned j = 0; j < i; ++j)
            if (RhhContentKeyOf(def->ecology_tags[j].id) == key) return Fail("duplicate ecology tag in %s", def->id);
    }
    for (unsigned i = 0; i < bindings; ++i)
    {
        const struct RhhModSpeciesTag *binding = &def->species_tags[i];
        if (binding->struct_size < sizeof(*binding) || binding->reserved || !binding->species || !binding->tag)
            return Fail("invalid species tag in %s", def->id);
    }
    unsigned types = CustomTypeCount(def), existingTypes = 0;
    for (unsigned i = 0; i < sModCount; ++i) existingTypes += CustomTypeCount(sMods[i].def);
    if (types > RHH_CUSTOM_TYPES_PER_MOD || types > MAX_CUSTOM_TYPES - existingTypes
        || (types && (!def->types || (def->abi_version != RHH_MOD_ABI_TYPES && def->abi_version != RHH_MOD_ABI_MOVES && def->abi_version != RHH_MOD_ABI_LEARNSETS && def->abi_version != RHH_MOD_ABI_CONDITIONS && def->abi_version != RHH_MOD_ABI_MOVE_EFFECTS && def->abi_version != RHH_MOD_ABI_DAMAGE_EFFECTS && (def->abi_version != RHH_MOD_ABI_ABILITIES && def->abi_version != RHH_MOD_ABI_HABITATS))))
        || (def->struct_size >= RHH_MOD_DEFINITION_TYPES_SIZE && def->type_reserved))
        return Fail("invalid custom type declarations in %s", def->id);
    for (unsigned i = 0; i < types; ++i)
    {
        const struct RhhModType *type = &def->types[i];
        RhhContentKey key = type->id ? RhhContentKeyOf(type->id) : 0;
        struct ContentIndex query = {.key = key};
        if (type->struct_size < sizeof(*type) || type->reserved || type->rgb > 0xFFFFFF
            || !ValidCustomTypeId(type->id) || !ValidTypeLabel(type->label) || !key
            || type->relation_count > MAX_CUSTOM_TYPE_RELATIONS || (type->relation_count && !type->relations)
            || FindDeclaredCustomType(key) || FindDeclaredCustomMove(key) || FindDeclaredBattleCondition(key)
            || (sContentCount && bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent)))
            return Fail("invalid or duplicate custom type in %s", def->id);
        for (unsigned j = 0; j < i; ++j)
            if (RhhContentKeyOf(def->types[j].id) == key) return Fail("duplicate custom type in %s", def->id);
        for (unsigned j = 0; j < type->relation_count; ++j)
        {
            const struct RhhTypeRelation *relation = &type->relations[j];
            if (relation->struct_size < sizeof(*relation) || relation->reserved || !relation->other
                || !ValidTypeRelationMultiplier(relation->attack) || !ValidTypeRelationMultiplier(relation->defense)
                || (relation->attack == RHH_TYPE_UNSPECIFIED && relation->defense == RHH_TYPE_UNSPECIFIED))
                return Fail("invalid custom type relation in %s", def->id);
            for (unsigned k = 0; k < j; ++k)
                if (type->relations[k].other == relation->other) return Fail("duplicate custom type relation in %s", def->id);
        }
    }
    unsigned moves = CustomMoveCount(def), existingMoves = 0;
    for (unsigned i = 0; i < sModCount; ++i) existingMoves += CustomMoveCount(sMods[i].def);
    if (moves > RHH_CUSTOM_MOVES_PER_MOD || moves > RHH_CUSTOM_MOVE_LIMIT - existingMoves
        || (moves && (!def->moves || (def->abi_version != RHH_MOD_ABI_MOVES && def->abi_version != RHH_MOD_ABI_LEARNSETS && def->abi_version != RHH_MOD_ABI_CONDITIONS && def->abi_version != RHH_MOD_ABI_MOVE_EFFECTS && def->abi_version != RHH_MOD_ABI_DAMAGE_EFFECTS && (def->abi_version != RHH_MOD_ABI_ABILITIES && def->abi_version != RHH_MOD_ABI_HABITATS))))
        || (def->struct_size >= RHH_MOD_DEFINITION_MOVES_SIZE && def->move_reserved))
        return Fail("invalid custom move declarations in %s", def->id);
    for (unsigned i = 0; i < moves; ++i)
    {
        const struct RhhModMove *move = &def->moves[i];
        if (move->struct_size < sizeof(*move)) return Fail("undersized custom move in %s", def->id);
        RhhContentKey key = move->id ? RhhContentKeyOf(move->id) : 0;
        struct ContentIndex query = {.key = key};
        if (move->reserved || !ValidCustomMoveId(move->id)
            || !ValidMoveText(move->name, 13, 0) || !ValidMoveText(move->description, 128, 1)
            || !key || !move->template_move || !move->type || move->power > 511 || move->accuracy > 100
            || !move->pp || move->pp > 40 || move->category < RHH_MOVE_PHYSICAL || move->category > RHH_MOVE_STATUS
            || move->target < RHH_MOVE_TARGET_SELECTED || move->target > RHH_MOVE_TARGET_ALL_BATTLERS
            || move->priority < -7 || move->priority > 7 || FindDeclaredCustomMove(key)
            || FindDeclaredCustomType(key) || FindDeclaredBattleCondition(key)
            || (sContentCount && bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent)))
            return Fail("invalid or duplicate custom move in %s", def->id);
        for (unsigned j = 0; j < i; ++j)
            if (RhhContentKeyOf(def->moves[j].id) == key) return Fail("duplicate custom move key in %s", def->id);
        for (unsigned j = 0; j < types; ++j)
            if (RhhContentKeyOf(def->types[j].id) == key) return Fail("custom move collides with type in %s", def->id);
    }
    unsigned learnsets = ModLearnsetCount(def), existingLearnsets = 0;
    for (unsigned i = 0; i < sModCount; ++i) existingLearnsets += ModLearnsetCount(sMods[i].def);
    if (learnsets > RHH_MOD_LEARNSETS_PER_MOD || learnsets > RHH_MOD_LEARNSETS_LIMIT - existingLearnsets
        || (learnsets && (!def->learnsets || (def->abi_version != RHH_MOD_ABI_LEARNSETS && def->abi_version != RHH_MOD_ABI_CONDITIONS && def->abi_version != RHH_MOD_ABI_MOVE_EFFECTS && def->abi_version != RHH_MOD_ABI_DAMAGE_EFFECTS && (def->abi_version != RHH_MOD_ABI_ABILITIES && def->abi_version != RHH_MOD_ABI_HABITATS))))
        || (def->struct_size >= RHH_MOD_DEFINITION_LEARNSETS_SIZE && def->learnset_reserved))
        return Fail("invalid learnset declarations in %s", def->id);
    for (unsigned i = 0; i < learnsets; ++i)
    {
        const struct RhhModLearnset *entry = &def->learnsets[i];
        if (entry->struct_size < sizeof(*entry) || entry->reserved || !entry->species || !entry->move
            || entry->kind < RHH_LEARNSET_LEVEL || entry->kind > RHH_LEARNSET_EGG
            || (entry->kind == RHH_LEARNSET_LEVEL ? entry->level > 100 : entry->level != 0))
            return Fail("invalid learnset entry in %s", def->id);
    }
    unsigned conditions = BattleConditionCount(def), existingConditions = 0;
    for (unsigned i = 0; i < sModCount; ++i) existingConditions += BattleConditionCount(sMods[i].def);
    if (conditions > RHH_BATTLE_CONDITIONS_PER_MOD || conditions > RHH_BATTLE_CONDITION_LIMIT - existingConditions
        || (conditions && (!def->battle_conditions || (def->abi_version != RHH_MOD_ABI_CONDITIONS && def->abi_version != RHH_MOD_ABI_MOVE_EFFECTS && def->abi_version != RHH_MOD_ABI_DAMAGE_EFFECTS && (def->abi_version != RHH_MOD_ABI_ABILITIES && def->abi_version != RHH_MOD_ABI_HABITATS))))
        || (def->struct_size >= RHH_MOD_DEFINITION_CONDITIONS_SIZE && def->battle_condition_reserved))
        return Fail("invalid battle condition declarations in %s", def->id);
    for (unsigned i = 0; i < conditions; ++i)
    {
        const struct RhhModBattleCondition *condition = &def->battle_conditions[i];
        if (condition->struct_size < sizeof(*condition)) return Fail("undersized battle condition in %s", def->id);
        RhhContentKey key = condition->id ? RhhContentKeyOf(condition->id) : 0;
        struct ContentIndex query = {.key = key};
        if (condition->reserved || !ValidBattleConditionId(condition->id) || !key
            || !ValidMoveText(condition->label, 32, 0)
            || (condition->scope != RHH_CONDITION_FIELD && condition->scope != RHH_CONDITION_SIDE)
            || !condition->max_turns || condition->max_turns > 255
            || FindDeclaredBattleCondition(key) || FindDeclaredCustomType(key) || FindDeclaredCustomMove(key)
            || (sContentCount && bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent)))
            return Fail("invalid or duplicate battle condition in %s", def->id);
        for (unsigned j = 0; j < i; ++j)
            if (RhhContentKeyOf(def->battle_conditions[j].id) == key) return Fail("duplicate battle condition in %s", def->id);
        for (unsigned j = 0; j < types; ++j)
            if (RhhContentKeyOf(def->types[j].id) == key) return Fail("condition collides with type in %s", def->id);
        for (unsigned j = 0; j < moves; ++j)
            if (RhhContentKeyOf(def->moves[j].id) == key) return Fail("condition collides with move in %s", def->id);
    }
    unsigned effects = MoveEffectCount(def), effectBindings = MoveEffectBindingCount(def);
    unsigned existingEffects = 0, existingEffectBindings = 0;
    for (unsigned i = 0; i < sModCount; ++i)
    { existingEffects += MoveEffectCount(sMods[i].def); existingEffectBindings += MoveEffectBindingCount(sMods[i].def); }
    if (effects > RHH_MOVE_EFFECTS_PER_MOD || effects > RHH_MOVE_EFFECT_LIMIT - existingEffects
        || effectBindings > RHH_MOVE_EFFECT_BINDINGS_PER_MOD || effectBindings > RHH_MOVE_EFFECT_BINDING_LIMIT - existingEffectBindings
        || ((effects || effectBindings) && def->abi_version != RHH_MOD_ABI_MOVE_EFFECTS && def->abi_version != RHH_MOD_ABI_DAMAGE_EFFECTS && (def->abi_version != RHH_MOD_ABI_ABILITIES && def->abi_version != RHH_MOD_ABI_HABITATS))
        || (effects && !def->move_effects) || (effectBindings && !def->move_effect_bindings)
        || (def->struct_size >= RHH_MOD_DEFINITION_MOVE_EFFECTS_SIZE
            && (def->move_effect_reserved || def->move_effect_binding_reserved)))
        return Fail("invalid move effect declarations in %s", def->id);
    for (unsigned i = 0; i < effects; ++i)
    {
        const struct RhhModMoveEffect *effect = &def->move_effects[i];
        if (effect->struct_size < sizeof(*effect)) return Fail("undersized move effect in %s", def->id);
        RhhContentKey key = effect->id ? RhhContentKeyOf(effect->id) : 0;
        struct ContentIndex query = {.key = key};
        if (!key || !ValidMoveEffectId(effect->id) || !ValidMoveText(effect->label, 32, 0)
            || (effect->mode != RHH_MOVE_EFFECT_CUSTOM_STATUS
                && !(effect->mode == RHH_MOVE_EFFECT_CUSTOM_DAMAGE
                     && (def->abi_version == RHH_MOD_ABI_DAMAGE_EFFECTS || def->abi_version == RHH_MOD_ABI_ABILITIES || def->abi_version == RHH_MOD_ABI_HABITATS)))
            || effect->reserved[0] || effect->reserved[1]
            || FindDeclaredMoveEffect(key) || FindDeclaredBattleCondition(key) || FindDeclaredCustomMove(key)
            || FindDeclaredCustomType(key) || (sContentCount && bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent)))
            return Fail("invalid or duplicate move effect in %s", def->id);
        for (unsigned j = 0; j < i; ++j)
            if (RhhContentKeyOf(def->move_effects[j].id) == key) return Fail("duplicate move effect in %s", def->id);
        for (unsigned j = 0; j < conditions; ++j)
            if (RhhContentKeyOf(def->battle_conditions[j].id) == key) return Fail("effect collides with condition in %s", def->id);
        for (unsigned j = 0; j < moves; ++j)
            if (RhhContentKeyOf(def->moves[j].id) == key) return Fail("effect collides with move in %s", def->id);
        for (unsigned j = 0; j < types; ++j)
            if (RhhContentKeyOf(def->types[j].id) == key) return Fail("effect collides with type in %s", def->id);
    }
    for (unsigned i = 0; i < effectBindings; ++i)
    {
        const struct RhhModMoveEffectBinding *binding = &def->move_effect_bindings[i];
        if (binding->struct_size < sizeof(*binding) || binding->reserved || !binding->move || !binding->effect)
            return Fail("invalid move effect binding in %s", def->id);
        for (unsigned j = 0; j < i; ++j)
            if (def->move_effect_bindings[j].move == binding->move) return Fail("duplicate move effect binding in %s", def->id);
        for (unsigned m = 0; m < sModCount; ++m)
            for (unsigned j = 0; j < MoveEffectBindingCount(sMods[m].def); ++j)
                if (sMods[m].def->move_effect_bindings[j].move == binding->move)
                    return Fail("conflicting move effect binding in %s", def->id);
    }
    unsigned abilities = CustomAbilityCount(def), existingAbilities = 0;
    for (unsigned i = 0; i < sModCount; ++i) existingAbilities += CustomAbilityCount(sMods[i].def);
    if (abilities > RHH_MOD_ABILITIES_PER_MOD || abilities > RHH_CUSTOM_ABILITY_LIMIT - existingAbilities
        || (abilities && (!def->abilities || (def->abi_version != RHH_MOD_ABI_ABILITIES && def->abi_version != RHH_MOD_ABI_HABITATS)))
        || (def->struct_size >= RHH_MOD_DEFINITION_ABILITIES_SIZE && def->ability_reserved))
        return Fail("invalid ability declarations in %s", def->id);
    for (unsigned i = 0; i < abilities; ++i)
    {
        const struct RhhModAbility *ability = &def->abilities[i];
        RhhContentKey key = ability->id ? RhhContentKeyOf(ability->id) : 0;
        struct ContentIndex query = {.key = key};
        if (ability->struct_size < sizeof(*ability) || ability->reserved[0] || ability->reserved[1]
            || !key || !ValidCustomAbilityId(ability->id) || (ability->flags & ~RHH_ABILITY_FLAGS_ALL)
            || !ValidMoveText(ability->label, 33, 0) || !ValidMoveText(ability->description, 129, 1)
            || FindDeclaredCustomAbility(key) || FindDeclaredMoveEffect(key) || FindDeclaredBattleCondition(key)
            || FindDeclaredCustomMove(key) || FindDeclaredCustomType(key)
            || (sContentCount && bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent)))
            return Fail("invalid or duplicate ability in %s", def->id);
        for (unsigned j = 0; j < i; ++j)
            if (RhhContentKeyOf(def->abilities[j].id) == key) return Fail("duplicate ability in %s", def->id);
        for (unsigned j = 0; j < types; ++j)
            if (RhhContentKeyOf(def->types[j].id) == key) return Fail("ability collides with type in %s", def->id);
        for (unsigned j = 0; j < moves; ++j)
            if (RhhContentKeyOf(def->moves[j].id) == key) return Fail("ability collides with move in %s", def->id);
        for (unsigned j = 0; j < conditions; ++j)
            if (RhhContentKeyOf(def->battle_conditions[j].id) == key) return Fail("ability collides with condition in %s", def->id);
        for (unsigned j = 0; j < effects; ++j)
            if (RhhContentKeyOf(def->move_effects[j].id) == key) return Fail("ability collides with effect in %s", def->id);
    }
    if ((def->flags & RHH_MOD_DURABLE) && !def->merge) return Fail("durable module requires merge callback");
    if (def->setting_count > 16 || (def->setting_count && (!def->settings || !def->read_setting || !def->write_setting)))
        return Fail("invalid module settings");
    for (unsigned i = 0; i < def->setting_count; ++i)
    {
        const struct RhhModSetting *s = &def->settings[i];
        if (s->struct_size < sizeof(*s) || !ValidName(s->id) || !s->label || !s->description
            || s->type < RHH_SETTING_BOOL || s->type > RHH_SETTING_CONTENT
            || s->scope > RHH_SETTING_RUNTIME || s->choice_count > 32 || s->minimum > s->maximum
            || (s->choice_count && !s->choices)) return Fail("invalid setting descriptor");
        for (unsigned j = 0; j < i; ++j) if (!strcmp(s->id, def->settings[j].id)) return Fail("duplicate setting id");
    }
    if (sModCount == MAX_MODS
        || def->hook_count > MAX_HOOKS - sHookCount || def->dependency_count > MAX_MODS
        || (def->hook_count && !def->hooks) || (def->dependency_count && !def->dependencies))
        return Fail("mod exceeds host limits");
    for (unsigned i = 0; i < def->dependency_count; ++i)
        if (!ValidName(def->dependencies[i])) return Fail("invalid dependency id");
    for (unsigned i = 0; i < def->hook_count; ++i)
    {
        const struct RhhModHook *h = &def->hooks[i];
        if (h->struct_size < sizeof(*h) || !h->event || h->event >= RHH_MOD_EVENT_COUNT
            || h->stage > RHH_OBSERVE || (h->stage == RHH_OBSERVE ? !h->observe : !h->reduce))
            return Fail("invalid hook in %s", def->id);
        if (h->event == RHH_CAN_HATCH_EGG && h->stage != RHH_CONSTRAINT)
            return Fail("hatch permission hooks must be constraints");
        if (h->event == RHH_AUTHORED_WILD_WEIGHT && h->stage != RHH_TRANSFORM && h->stage != RHH_CONSTRAINT)
            return Fail("authored wild weight hooks must be pure reducers");
        if (h->event == RHH_WILD_DORMANT_RELOCATED && h->stage != RHH_OBSERVE)
            return Fail("dormant relocation hooks must be observers");
        if (h->event == RHH_WILD_DORMANT_CLOCK_ADVANCED && h->stage != RHH_OBSERVE)
            return Fail("dormant clock hooks must be observers");
        if ((h->event == RHH_MOVE_TARGET_RESOLVE || h->event == RHH_FIELD_POISON_DAMAGE
             || h->event == RHH_MOVE_BASE_PRIORITY || h->event == RHH_MOVE_BATTLER_PRIORITY || h->event == RHH_INNATE_PRIORITY
             || h->event == RHH_BATTLER_SPEED || h->event == RHH_INNATE_SPEED
             || h->event == RHH_DAMAGE_STAT || h->event == RHH_INNATE_DAMAGE_STAT
             || h->event == RHH_MOVE_CRITICAL_STAGE || h->event == RHH_INNATE_CRITICAL_STAGE
             || h->event == RHH_DAMAGE_FINAL || h->event == RHH_INNATE_DAMAGE_FINAL
             || h->event == RHH_SELF_DAMAGE_FINAL || h->event == RHH_INNATE_SELF_DAMAGE_FINAL
             || h->event == RHH_EGG_CYCLE_PROGRESS) && h->stage == RHH_OBSERVE)
            return Fail("target and field-poison hooks must be pure reducers");
        int notification = h->event == RHH_ENCOUNTER_STARTED || h->event == RHH_MON_FAINTED
            || h->event == RHH_RUN_STARTED || h->event == RHH_RUN_LOADED
            || h->event == RHH_INVENTORY_CHANGED || h->event == RHH_WHITEOUT
            || h->event == RHH_WILD_CREATED || h->event == RHH_PERSISTENCE_FAILED || h->event == RHH_MON_CAUGHT
            || h->event == RHH_MON_EDITED || h->event == RHH_MOD_UPDATE
            || h->event == RHH_BREED_EGG_CREATED || h->event == RHH_EGG_HATCHED || h->event == RHH_EGG_CYCLE_TICKED
            || h->event == RHH_DAYCARE_EGG_REQUESTED || h->event == RHH_DAYCARE_PENDING_CHANGED
            || h->event == RHH_WORLD_STEP || h->event == RHH_WORLD_ENVIRONMENT_CHANGED || h->event == RHH_NATURAL_WEATHER_CHANGED
            || h->event == RHH_WILD_GENERATED || h->event == RHH_WILD_ACTOR_SPAWNED
            || h->event == RHH_WILD_ACTOR_REMOVED || h->event == RHH_WILD_ACTOR_ENGAGED || h->event == RHH_WILD_ACTOR_STEP_FINISHED
            || h->event == RHH_WILD_ACTOR_BATTLE_ENDED || h->event == RHH_WILD_ACTOR_IDENTIFIED
            || h->event == RHH_MOVE_TAUGHT || h->event == RHH_FIELD_HEALED || h->event == RHH_FIELD_STATUS_CURED || h->event == RHH_FIELD_POISON_DAMAGED || h->event == RHH_MOVE_RESOLVED || h->event == RHH_MOVE_PP_DEDUCTED || h->event == RHH_MOVE_DAMAGE_APPLIED || h->event == RHH_MOVE_ACCURACY_CHECKED || h->event == RHH_BATTLE_TURN_ENDED
            || h->event == RHH_MOVE_EFFECT_APPLY || h->event == RHH_BATTLE_CONDITION_CHANGED
            || h->event == RHH_REGISTERED_MOVE_EFFECT || h->event == RHH_MOVE_DAMAGE_EFFECT_APPLY
            || h->event == RHH_INNATE_ENTRY || h->event == RHH_INNATE_TURN_END || h->event == RHH_BATTLER_ABILITY_OBSERVED
            || h->event == RHH_BATTLER_TYPES_OBSERVED
            || h->event == RHH_PUBLIC_BATTLER_TYPES_OBSERVED
            || h->event == RHH_BATTLER_HEALTH_OBSERVED
            || h->event == RHH_BATTLER_INNATES_OBSERVED || h->event == RHH_BATTLER_ABILITY_MUTATED
            || h->event == RHH_BATTLER_ABILITY_CONTACT_MUTATED || h->event == RHH_BATTLE_NEUTRALIZING_GAS_CHANGED
            || h->event == RHH_UI_ACCESSIBILITY_CHANGED || h->event == RHH_UI_OVERLAY_ACCESSIBILITY_CHANGED
            || h->event == RHH_BATTLER_STATUS_APPLIED || h->event == RHH_BATTLER_ITEM_STATUS_CHANGED
            || h->event == RHH_BATTLER_STATUS_CURED || h->event == RHH_BATTLE_PARTY_STATUS_CURED
            || h->event == RHH_OWNED_MON_CHANGED
            || h->event == RHH_OWNED_MON_RELEASED || h->event == RHH_OWNED_MON_MOVED || h->event == RHH_OWNED_MON_PICKED_UP
            || h->event == RHH_OWNED_STORAGE_UNREADABLE || h->event == RHH_OWNED_GIFT_RECEIVED
            || h->event == RHH_OWNED_DEBUG_CREATED || h->event == RHH_OWNED_MYSTERY_GIFT_RECEIVED
            || h->event == RHH_OWNED_EVOLUTION_COMMITTED || h->event == RHH_ITEM_EV_PROCESSED
            || h->event == RHH_ITEM_EV_REDUCTION_PROCESSED
            || h->event == RHH_PARTY_REORDERED || h->event == RHH_OWNED_DAYCARE_TRANSFERRED
            || h->event == RHH_OWNED_FUSION_TRANSFERRED || h->event == RHH_OWNED_CATCH_PLACED
            || h->event == RHH_OWNED_INGAME_TRADE_COMPLETED || h->event == RHH_OWNED_LINK_TRADE_SAVED;
        notification |= h->event == RHH_WILD_CONCEALMENT_CHANGED || h->event == RHH_WILD_CANCELLED
            || h->event == RHH_WILD_SPECIAL_GENERATED || h->event == RHH_WILD_SPECIAL_CANCELLED
            || h->event == RHH_WILD_DEXNAV_GENERATED || h->event == RHH_WILD_DEXNAV_CANCELLED
            || h->event == RHH_WILD_FACILITY_GENERATED || h->event == RHH_WILD_FACILITY_CANCELLED
            || h->event == RHH_WILD_MANUAL_FACILITY_GENERATED || h->event == RHH_WILD_MANUAL_FACILITY_CANCELLED
            || h->event == RHH_WILD_SCRIPTED_CANCELLED || h->event == RHH_WILD_EVENT_CANCELLED
            || h->event == RHH_WILD_SCRIPTED_CLEANUP_FINISHED || h->event == RHH_WILD_EVENT_CLEANUP_FINISHED
            || h->event == RHH_WILD_TUTORIAL_CANCELLED || h->event == RHH_WILD_TUTORIAL_CLEANUP_FINISHED;
        notification |= h->event == RHH_PHOTO_REWARD_CLAIMED || h->event == RHH_PHOTO_REWARD_DELIVERED
            || h->event == RHH_BATTLE_EVS_PROCESSED || h->event == RHH_EXPERIENCE_AWARDED
            || h->event == RHH_EXPERIENCE_FLOW_FINISHED || h->event == RHH_PHOTO_EXPERIENCE_CLAIMED
            || h->event == RHH_PHOTO_EXPERIENCE_DELIVERED || h->event == RHH_PHOTO_EXPERIENCE_RETARGETED;
        notification |= h->event == RHH_BERRY_CLOCK_UPDATED || h->event == RHH_AUTHORED_BERRY_WATERED || h->event == RHH_BATTLE_SPORTS_OBSERVED || h->event == RHH_BATTLE_FIELD_RULES_OBSERVED || h->event == RHH_BATTLE_FIELD_RULES_COMMITTED || h->event == RHH_BATTLE_ENVIRONMENT_OBSERVED || h->event == RHH_BATTLE_ENVIRONMENT_COMMITTED || h->event == RHH_BATTLE_ENVIRONMENT_TIMER_TICKED || h->event == RHH_BATTLE_WEATHER_SUPPRESSION_CHANGED || h->event == RHH_BATTLE_WEATHER_GAS_SUPPRESSION_CHANGED || h->event == RHH_BATTLE_WEATHER_PRESENCE_SUPPRESSION_CHANGED || h->event == RHH_BATTLE_PARTY_ITEM_STATUS_CURED || h->event == RHH_MUSIC_BATTLE_CUE_FINISHED || h->event == RHH_BATTLE_STARTED || h->event == RHH_BATTLER_ENTERED || h->event == RHH_BATTLE_ENDED
            || h->event == RHH_WILD_SCRIPTED_READY || h->event == RHH_WILD_GHOST_READY
            || h->event == RHH_WILD_TUTORIAL_READY || h->event == RHH_WILD_FIRST_BATTLE_READY
            || h->event == RHH_SCOUT_RECORDED
            || h->event == RHH_PHOTO_CAPTURED || h->event == RHH_PHOTO_FAILED
            || h->event == RHH_PHOTO_STORED || h->event == RHH_PHOTO_REMOVED
            || h->event == RHH_WILD_TRAIT_EDITED || h->event == RHH_WILD_BEHAVIOR_RESULT
            || h->event == RHH_WILD_ACTIVITY_CHANGED || h->event == RHH_WILD_QUEUE_CHANGED || h->event == RHH_WILD_DORMANT_RELOCATED || h->event == RHH_WILD_BEHAVIOR_POLICY_CHANGED || h->event == RHH_BERRY_PLOT_WATERED || h->event == RHH_BERRY_PLOT_MANUALLY_WATERED || h->event == RHH_BERRY_PLOT_INTERACTED || h->event == RHH_BERRY_PLOT_HARVESTED || h->event == RHH_BOSS_ACE_CONFIGURED
            || h->event == RHH_MUSIC_ASSIGNMENT_CHANGED || h->event == RHH_MUSIC_LEVEL_CHANGED || h->event == RHH_MUSIC_IMPORTED
            || h->event == RHH_MUSIC_IMPORT_REMOVED || h->event == RHH_MUSIC_PLAYBACK_CHANGED || h->event == RHH_WILD_PROFILE_EDITED
            || h->event == RHH_DORMANT_WILD_PROFILE_EDITED || h->event == RHH_WILD_DORMANT_CLOCK_ADVANCED;
        if ((h->stage == RHH_OBSERVE) != notification)
            return Fail("mutable observers require committed notification events");
        if (h->event == RHH_CAN_EDIT_MON && h->stage != RHH_CONSTRAINT)
            return Fail("mon edit proposals require constraint hooks");
        if (h->event == RHH_CAN_EDIT_WILD_QUEUE && h->stage != RHH_CONSTRAINT)
            return Fail("wild queue edits require constraint hooks");
        if ((h->event == RHH_CAN_WATER_BERRY_PLOT || h->event == RHH_CAN_WATER_AUTHORED_BERRY) && h->stage != RHH_CONSTRAINT)
            return Fail("berry watering proposals require constraint hooks");
        if (h->event == RHH_CAN_AWARD_EXPERIENCE && h->stage != RHH_CONSTRAINT)
            return Fail("experience award proposals require constraint hooks");
        if (h->event == RHH_CAN_CLAIM_PHOTO_EXPERIENCE && h->stage != RHH_CONSTRAINT)
            return Fail("photo experience proposals require constraint hooks");
        if (h->event == RHH_CAN_RETARGET_PHOTO_EXPERIENCE && h->stage != RHH_CONSTRAINT)
            return Fail("photo experience retarget proposals require constraint hooks");
        if (h->event == RHH_CAN_APPLY_BATTLE_EVS && h->stage != RHH_CONSTRAINT)
            return Fail("battle EV proposals require constraint hooks");
        if (h->event == RHH_PARTY_MENU_ACTIONS && h->stage != RHH_CONSTRAINT)
            return Fail("party action collection requires constraint hooks");
        if (h->event == RHH_MOVE_ADDITIONAL_EFFECTS && h->stage != RHH_CONSTRAINT)
            return Fail("move effect collection requires constraint hooks");
        if (h->event == RHH_AI_EFFECT_PREVIEW && h->stage != RHH_CONSTRAINT)
            return Fail("AI effect previews require constraint hooks");
        if ((h->event == RHH_INNATE_PROPOSE || h->event == RHH_INNATE_PERSONALIZE
             || h->event == RHH_INNATE_BATTLE_PERSONALIZE) && h->stage != RHH_CONSTRAINT)
            return Fail("innate proposals require constraint hooks");
        if (h->event == RHH_BREED_NATURE && h->stage != RHH_TRANSFORM && h->stage != RHH_CONSTRAINT)
            return Fail("breeding nature proposals require pure transform or constraint hooks");
        if (h->event == RHH_BREED_IV_RULE && h->stage != RHH_TRANSFORM && h->stage != RHH_CONSTRAINT)
            return Fail("breeding IV rules require pure transform or constraint hooks");
        if (h->event == RHH_BREED_MOVE_RULE && h->stage != RHH_TRANSFORM && h->stage != RHH_CONSTRAINT)
            return Fail("breeding move rules require pure transform or constraint hooks");
        if (h->event == RHH_BREED_RNG_POLICY && h->stage != RHH_TRANSFORM && h->stage != RHH_CONSTRAINT)
            return Fail("breeding RNG policy requires pure transform or constraint hooks");
        if ((h->event == RHH_BREED_BALL_RULE || h->event == RHH_BREED_ABILITY_RULE)
            && h->stage != RHH_TRANSFORM && h->stage != RHH_CONSTRAINT)
            return Fail("breeding trait rules require pure transform or constraint hooks");
        if (h->event == RHH_ITEM_EV_PROPOSAL && h->stage != RHH_TRANSFORM && h->stage != RHH_CONSTRAINT)
            return Fail("item EV proposals require pure transform or constraint hooks");
        if (h->event == RHH_CAN_REDUCE_ITEM_EVS && h->stage != RHH_CONSTRAINT)
            return Fail("item EV reductions require constraint hooks");
        if (h->event == RHH_BATTLE_EV_PROPOSAL && h->stage != RHH_TRANSFORM && h->stage != RHH_CONSTRAINT)
            return Fail("battle EV mode proposals require pure transform or constraint hooks");
        if ((h->event == RHH_AI_INNATE_ENTRY_PREVIEW || h->event == RHH_AI_INNATE_ENTRY_STAT_PREVIEW || h->event == RHH_AI_INNATE_ENTRY_FIELD_PREVIEW
             || h->event == RHH_AI_INNATE_TURN_HEAL_PREVIEW || h->event == RHH_AI_INNATE_TURN_STEP_PREVIEW)
            && h->stage != RHH_CONSTRAINT)
            return Fail("AI innate previews require constraint hooks");
        if (h->event == RHH_WORLD_RENDER_PASSES && h->stage != RHH_CONSTRAINT)
            return Fail("world render pass collection requires constraint hooks");
        if (h->event == RHH_BATTLE_RENDER_PASSES && h->stage != RHH_CONSTRAINT)
            return Fail("battle render pass collection requires constraint hooks");
        if (h->event == RHH_WORLD_LIGHTS && h->stage != RHH_CONSTRAINT)
            return Fail("world light collection requires constraint hooks");
        if (h->event == RHH_CAN_WORLD_ENVIRONMENT && h->stage != RHH_CONSTRAINT)
            return Fail("world environment proposals require constraint hooks");
        if (h->event == RHH_CAN_WILD_CONCEALMENT && h->stage != RHH_CONSTRAINT)
            return Fail("wild concealment proposals require constraint hooks");
        if (h->event == RHH_CAN_EDIT_WILD_TRAIT && h->stage != RHH_CONSTRAINT)
            return Fail("wild trait edits require constraint hooks");
        if ((h->event == RHH_CAN_EDIT_WILD_PROFILE || h->event == RHH_CAN_EDIT_DORMANT_WILD_PROFILE) && h->stage != RHH_CONSTRAINT)
            return Fail("wild profile edits require constraint hooks");
        if ((h->event == RHH_WILD_PROFILE_SCHEMA || h->event == RHH_WILD_PROFILE_SCHEMA_COLLECT)
            && h->stage != RHH_CONSTRAINT)
            return Fail("wild profile schema collection requires constraint hooks");
        if (h->event == RHH_CAN_SCOUT && h->stage != RHH_CONSTRAINT)
            return Fail("scouting proposals require constraint hooks");
        if (h->event == RHH_CAN_PHOTO && h->stage != RHH_CONSTRAINT)
            return Fail("photo proposals require constraint hooks");
        if (h->event == RHH_CAN_CLAIM_PHOTO_REWARD && h->stage != RHH_CONSTRAINT)
            return Fail("photo reward proposals require constraint hooks");
        if (h->event == RHH_CAN_TEACH_MOVE && h->stage != RHH_CONSTRAINT)
            return Fail("move teaching permission requires constraint hooks");
        if ((h->event == RHH_CAN_FIELD_HEAL || h->event == RHH_CAN_CURE_FIELD_STATUS) && h->stage != RHH_CONSTRAINT)
            return Fail("field healing permission requires constraint hooks");
        if ((h->event == RHH_CAN_ACCEPT_DAYCARE_EGG || h->event == RHH_CAN_REQUEST_DAYCARE_EGG) && h->stage != RHH_CONSTRAINT)
            return Fail("daycare egg permission requires constraint hooks");
        if (h->event == RHH_WILD_ACTIVITY_COLLECT && h->stage != RHH_CONSTRAINT)
            return Fail("wild activity collection requires constraint hooks");
        if (h->event == RHH_CAN_CONFIGURE_BOSS_ACE && h->stage != RHH_CONSTRAINT)
            return Fail("boss ace configuration requires constraint hooks");
        if (h->event == RHH_CAN_SET_MUSIC_ASSIGNMENT && h->stage != RHH_CONSTRAINT)
            return Fail("music assignment requires constraint hooks");
        if (h->event == RHH_CAN_SET_MUSIC_LEVEL && h->stage != RHH_CONSTRAINT)
            return Fail("music level requires constraint hooks");
        if (h->event == RHH_CAN_SET_WILD_BEHAVIOR_POLICY && h->stage != RHH_CONSTRAINT)
            return Fail("wild behavior policy requires constraint hooks");
        if ((h->event == RHH_CAN_GENERATE_WILD || h->event == RHH_CAN_GENERATE_SPECIAL_WILD
             || h->event == RHH_CAN_GENERATE_DEXNAV_WILD || h->event == RHH_CAN_GENERATE_FACILITY_WILD
             || h->event == RHH_CAN_GENERATE_MANUAL_FACILITY_WILD || h->event == RHH_CAN_BEGIN_SCRIPTED_WILD
             || h->event == RHH_CAN_BEGIN_EVENT_WILD || h->event == RHH_CAN_BEGIN_TUTORIAL_WILD)
            && h->stage != RHH_CONSTRAINT)
            return Fail("wild generation permission requires constraint hooks");
    }
    uint8_t *state = calloc(def->state_size ? def->state_size : 1, 1);
    if (!state) return Fail("cannot allocate mod state");
    if (def->validate && def->validate(state, def->state_size))
    { free(state); return Fail("zero default state is invalid for %s", def->id); }
    sMods[sModCount++] = (struct Mod){def, state};
    sHookCount += def->hook_count;
    return 0;
}
static int CompareHooks(const void *va, const void *vb)
{
    const struct Hook *a = va, *b = vb;
    const struct RhhModHook *ha = &sMods[a->mod].def->hooks[a->index];
    const struct RhhModHook *hb = &sMods[b->mod].def->hooks[b->index];
    if (ha->stage != hb->stage) return ha->stage < hb->stage ? -1 : 1;
    if (a->rank != b->rank) return a->rank < b->rank ? -1 : 1;
    if (ha->priority != hb->priority) return ha->priority < hb->priority ? -1 : 1;
    int id = strcmp(sMods[a->mod].def->id, sMods[b->mod].def->id);
    if (id) return id;
    return (a->index > b->index) - (a->index < b->index);
}
int RhhModsFinalize(void)
{
    uint8_t done[MAX_MODS] = {0};
    unsigned rank[MAX_MODS] = {0}, n = 0;
    sHabitatCount = 0;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < HabitatCount(sMods[i].def); ++j)
        {
            const struct RhhModHabitat *habitat = &sMods[i].def->habitats[j];
            if (habitat->map && RhhContentResolve(RHH_MAP, habitat->map, NULL))
                return Fail("habitat %s references a missing map", habitat->id);
            sHabitats[sHabitatCount++] = habitat;
        }
    qsort(sHabitats, sHabitatCount, sizeof(sHabitats[0]), CompareHabitats);
    sBattleConditionCount = 0;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < BattleConditionCount(sMods[i].def); ++j)
            sBattleConditions[sBattleConditionCount++] = &sMods[i].def->battle_conditions[j];
    qsort(sBattleConditions, sBattleConditionCount, sizeof(sBattleConditions[0]), CompareBattleConditions);
    sEcologyTagCount = sSpeciesTagCount = 0;
    sCustomTypeCount = 0;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < CustomTypeCount(sMods[i].def); ++j)
            sCustomTypes[sCustomTypeCount++] = &sMods[i].def->types[j];
    qsort(sCustomTypes, sCustomTypeCount, sizeof(sCustomTypes[0]), CompareCustomTypes);
    for (unsigned i = 0; i < sCustomTypeCount; ++i)
    {
        const struct RhhModType *type = sCustomTypes[i];
        RhhContentKey key = RhhContentKeyOf(type->id);
        struct ContentIndex query = {.key = key};
        if (sContentCount && bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent))
            return Fail("custom type %s collides with existing content", type->id);
        for (unsigned j = 0; j < type->relation_count; ++j)
        {
            const struct RhhTypeRelation *relation = &type->relations[j];
            const struct RhhModType *other = FindFinalCustomType(relation->other);
            if (!other)
            {
                uint32_t id;
                if (RhhContentResolve(RHH_TYPE, relation->other, &id)
                    || RhhContentKeyForId(RHH_TYPE, id) != relation->other)
                    return Fail("custom type %s references a missing or noncanonical type", type->id);
            }
            const struct RhhTypeRelation *reverse = FindCustomTypeRelation(other, key);
            if (reverse && ((relation->attack != RHH_TYPE_UNSPECIFIED && reverse->defense != RHH_TYPE_UNSPECIFIED
                             && relation->attack != reverse->defense)
                         || (relation->defense != RHH_TYPE_UNSPECIFIED && reverse->attack != RHH_TYPE_UNSPECIFIED
                             && relation->defense != reverse->attack)))
                return Fail("conflicting custom type relations for %s and %s", type->id, other->id);
        }
    }
    sCustomMoveCount = 0;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < CustomMoveCount(sMods[i].def); ++j)
            sCustomMoves[sCustomMoveCount++] = &sMods[i].def->moves[j];
    for (unsigned i = 0; i < sCustomMoveCount; ++i)
    {
        const struct RhhModMove *move = sCustomMoves[i];
        struct ContentIndex query = {.key = RhhContentKeyOf(move->id)};
        uint32_t id;
        if ((sContentCount && bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent))
            || FindFinalCustomType(query.key))
            return Fail("custom move %s collides with existing content", move->id);
        if (RhhContentResolve(RHH_MOVE, move->template_move, &id) || !id
            || RhhContentKeyForId(RHH_MOVE, id) != move->template_move)
            return Fail("custom move %s references an unknown template", move->id);
        if (!FindFinalCustomType(move->type)
            && (RhhContentResolve(RHH_TYPE, move->type, &id)
                || RhhContentKeyForId(RHH_TYPE, id) != move->type))
            return Fail("custom move %s references an unknown type", move->id);
    }
    sLearnsetCount = 0;
    for (unsigned i = 0; i < sBattleConditionCount; ++i)
    {
        struct ContentIndex query = {.key = RhhContentKeyOf(sBattleConditions[i]->id)};
        if ((sContentCount && bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent))
            || FindFinalCustomType(query.key) || FindDeclaredCustomMove(query.key))
            return Fail("battle condition %s collides with existing content", sBattleConditions[i]->id);
    }
    sMoveEffectCount = sMoveEffectBindingCount = 0;
    for (unsigned i = 0; i < sModCount; ++i)
    {
        for (unsigned j = 0; j < MoveEffectCount(sMods[i].def); ++j)
            sMoveEffects[sMoveEffectCount++] = &sMods[i].def->move_effects[j];
        for (unsigned j = 0; j < MoveEffectBindingCount(sMods[i].def); ++j)
            sMoveEffectBindings[sMoveEffectBindingCount++] = &sMods[i].def->move_effect_bindings[j];
    }
    qsort(sMoveEffects, sMoveEffectCount, sizeof(sMoveEffects[0]), CompareMoveEffects);
    qsort(sMoveEffectBindings, sMoveEffectBindingCount, sizeof(sMoveEffectBindings[0]), CompareMoveEffectBindings);
    for (unsigned i = 0; i < sMoveEffectCount; ++i)
    {
        RhhContentKey key = RhhContentKeyOf(sMoveEffects[i]->id);
        struct ContentIndex query = {.key = key};
        if ((sContentCount && bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent))
            || FindFinalCustomType(key) || FindDeclaredCustomMove(key) || FindDeclaredBattleCondition(key))
            return Fail("move effect %s collides with existing content", sMoveEffects[i]->id);
    }
    for (unsigned i = 0; i < sMoveEffectBindingCount; ++i)
    {
        const struct RhhModMoveEffectBinding *binding = sMoveEffectBindings[i];
        const struct RhhModMove *custom = FindDeclaredCustomMove(binding->move);
        uint32_t id;
        const struct RhhModMoveEffect *effect = FindDeclaredMoveEffect(binding->effect);
        if (!effect) return Fail("move effect binding references a missing effect");
        if (!custom && (RhhContentResolve(RHH_MOVE, binding->move, &id) || !id
                        || RhhContentKeyForId(RHH_MOVE, id) != binding->move))
            return Fail("move effect binding references an unknown or noncanonical move");
        if (!sMoveEffectValidator || sMoveEffectValidator(binding->move, custom, effect->mode))
            return Fail("move effect binding is incompatible with its move");
    }
    sCustomAbilityCount = 0;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < CustomAbilityCount(sMods[i].def); ++j)
            sCustomAbilities[sCustomAbilityCount++] = &sMods[i].def->abilities[j];
    qsort(sCustomAbilities, sCustomAbilityCount, sizeof(sCustomAbilities[0]), CompareCustomAbilities);
    for (unsigned i = 0; i < sCustomAbilityCount; ++i)
    {
        RhhContentKey key = RhhContentKeyOf(sCustomAbilities[i]->id);
        struct ContentIndex query = {.key = key};
        if ((sContentCount && bsearch(&query, sContentIndex, sContentCount, sizeof(query), CompareContent))
            || FindFinalCustomType(key) || FindDeclaredCustomMove(key)
            || FindDeclaredBattleCondition(key) || FindDeclaredMoveEffect(key))
            return Fail("custom ability %s collides with existing content", sCustomAbilities[i]->id);
    }
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < ModLearnsetCount(sMods[i].def); ++j)
            sLearnsets[sLearnsetCount++] = &sMods[i].def->learnsets[j];
    qsort(sLearnsets, sLearnsetCount, sizeof(sLearnsets[0]), CompareLearnsets);
    for (unsigned i = 0; i < sLearnsetCount; ++i)
    {
        const struct RhhModLearnset *entry = sLearnsets[i];
        struct RhhContentInfo info = {.struct_size = sizeof(info)};
        uint32_t id;
        if (RhhContentResolve(RHH_SPECIES, entry->species, &id) || !id
            || RhhContentKeyForId(RHH_SPECIES, id) != entry->species
            || RhhContentInfoFor(RHH_SPECIES, entry->species, &info) || !(info.flags & RHH_CONTENT_ENABLED))
            return Fail("learnset references an unknown or disabled species");
        if (!FindDeclaredCustomMove(entry->move)
            && (RhhContentResolve(RHH_MOVE, entry->move, &id) || !id
                || RhhContentKeyForId(RHH_MOVE, id) != entry->move))
            return Fail("learnset references an unknown or noncanonical move");
        unsigned sameSpecies = 1;
        for (unsigned j = i; j && sLearnsets[j - 1]->species == entry->species; --j) ++sameSpecies;
        if (sameSpecies > RHH_MOD_LEARNSETS_PER_SPECIES) return Fail("too many learnset entries for a species");
    }
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < EcologyTagCount(sMods[i].def); ++j)
            sEcologyTags[sEcologyTagCount++] = &sMods[i].def->ecology_tags[j];
    qsort(sEcologyTags, sEcologyTagCount, sizeof(sEcologyTags[0]), CompareEcologyTags);
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < SpeciesTagCount(sMods[i].def); ++j)
        {
            const struct RhhModSpeciesTag *binding = &sMods[i].def->species_tags[j]; uint32_t species;
            const struct RhhModEcologyTag *tag = FindEcologyTag(binding->tag);
            if (!tag || RhhContentResolve(RHH_SPECIES, binding->species, &species))
                return Fail("%s species tag references missing content", sMods[i].def->id);
            sSpeciesTags[sSpeciesTagCount++] = (struct SpeciesTagIndex){species, tag};
        }
    qsort(sSpeciesTags, sSpeciesTagCount, sizeof(sSpeciesTags[0]), CompareSpeciesTags);
    unsigned unique = 0;
    for (unsigned i = 0; i < sSpeciesTagCount; ++i)
        if (!unique || CompareSpeciesTags(&sSpeciesTags[i], &sSpeciesTags[unique - 1]))
            sSpeciesTags[unique++] = sSpeciesTags[i];
    sSpeciesTagCount = unique;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < SceneRuleCount(sMods[i].def); ++j)
        {
            const struct RhhModSceneRule *rule = &sMods[i].def->scene_rules[j];
            if ((rule->match & RHH_SCENE_MATCH_MAP) && RhhContentResolve(RHH_MAP, rule->map, NULL))
                return Fail("%s scene rule %s references a missing map", sMods[i].def->id, rule->id);
        }
    sSceneLightIndexCount = 0;
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < SceneLightCount(sMods[i].def); ++j)
        {
            const struct RhhModSceneLight *light = &sMods[i].def->scene_lights[j]; uint32_t map;
            if (RhhContentResolve(RHH_MAP, light->map, &map))
                return Fail("%s scene light references a missing map", sMods[i].def->id);
            if (sSceneLightIndexCount == sizeof(sSceneLightIndex) / sizeof(sSceneLightIndex[0]))
                return Fail("too many scene lights");
            sSceneLightIndex[sSceneLightIndexCount++] = (struct SceneLightIndex){map, light->x / 16, light->y / 16, light};
        }
    qsort(sSceneLightIndex, sSceneLightIndexCount, sizeof(sSceneLightIndex[0]), CompareSceneLightIndex);
    unsigned sameTile = 0;
    for (unsigned i = 0; i < sSceneLightIndexCount; ++i)
    {
        const struct SceneLightIndex *entry = &sSceneLightIndex[i];
        if (i && entry->map == sSceneLightIndex[i - 1].map && entry->x == sSceneLightIndex[i - 1].x
            && entry->y == sSceneLightIndex[i - 1].y) ++sameTile;
        else sameTile = 1;
        if (sameTile > RHH_SCENE_LIGHTS_PER_TILE) return Fail("too many scene lights on a tile");
    }
    for (unsigned i = 0; i < sModCount; ++i)
        for (unsigned j = 0; j < ServiceRequirementCount(sMods[i].def); ++j)
        {
            const struct RhhServiceRequirement *req = &sMods[i].def->service_requirements[j];
            if ((req->flags & RHH_SERVICE_REQUIRED) && !GetService(req->id, req->minimum_version))
                return Fail("%s requires service %s version %u.%u", sMods[i].def->id, req->id,
                            req->minimum_version >> 16, req->minimum_version & 0xffff);
        }
    while (n < sModCount)
    {
        int best = -1;
        for (unsigned i = 0; i < sModCount; ++i)
        {
            if (done[i]) continue;
            int ready = 1;
            for (unsigned d = 0; d < sMods[i].def->dependency_count; ++d)
            {
                int dep = FindMod(sMods[i].def->dependencies[d]);
                if (dep < 0) return Fail("missing dependency for %s", sMods[i].def->id);
                if (!done[dep]) ready = 0;
            }
            if (ready && (best < 0 || strcmp(sMods[i].def->id, sMods[best].def->id) < 0)) best = i;
        }
        if (best < 0) return Fail("mod dependency cycle");
        /* Dependency depth, not registration order, controls execution. */
        for (unsigned d = 0; d < sMods[best].def->dependency_count; ++d)
        {
            unsigned depth = rank[FindMod(sMods[best].def->dependencies[d])] + 1;
            if (depth > rank[best]) rank[best] = depth;
        }
        done[best] = 1;
        ++n;
    }
    n = 0;
    memset(sHookEventPresent, 0, sizeof(sHookEventPresent));
    for (unsigned m = 0; m < sModCount; ++m)
        for (unsigned h = 0; h < sMods[m].def->hook_count; ++h)
        {
            sHooks[n++] = (struct Hook){m, h, rank[m]};
            sHookEventPresent[sMods[m].def->hooks[h].event] = 1;
        }
    qsort(sHooks, sHookCount, sizeof(*sHooks), CompareHooks);
    if (sTrace)
        for (unsigned i = 0; i < sHookCount; ++i)
        {
            const struct RhhModDefinition *m = sMods[sHooks[i].mod].def;
            const struct RhhModHook *h = &m->hooks[sHooks[i].index];
            fprintf(sTrace, "{\"event\":\"hook_order\",\"index\":%u,\"mod\":\"%s\",\"hook\":%u,\"stage\":%u,\"dependency_depth\":%u,\"priority\":%d}\n",
                i, m->id, h->event, h->stage, sHooks[i].rank, h->priority);
        }
    sFinalized = 1;
    return 0;
}
void *RhhModsState(const char *id)
{
    int i = FindMod(id);
    return i < 0 ? NULL : sMods[i].state;
}
int RhhModsHasActiveRun(void) { return sFinalized && sRunId != 0; }
int RhhModsFinalized(void) { return sFinalized; }
int RhhModsRunCommitted(void) { return RhhModsHasActiveRun() && sCommitted; }
int RhhModsCanPersistDurably(void) { return RhhModsRunCommitted() && sPersist != NULL; }
int RhhModsHasHook(uint32_t event)
{
    return sFinalized && event < RHH_MOD_EVENT_COUNT && sHookEventPresent[event];
}
int RhhModsDormantProfileHooksCompatible(void)
{
    if (!sFinalized) return 0;
    for (unsigned i = 0; i < sModCount; ++i)
    {
        const struct RhhModDefinition *def = sMods[i].def;
        int liveConstraint = 0, dormantConstraint = 0, liveObserver = 0, dormantObserver = 0;
        for (unsigned j = 0; j < def->hook_count; ++j)
        {
            uint32_t event = def->hooks[j].event;
            liveConstraint |= event == RHH_CAN_EDIT_WILD_PROFILE;
            dormantConstraint |= event == RHH_CAN_EDIT_DORMANT_WILD_PROFILE;
            liveObserver |= event == RHH_WILD_PROFILE_EDITED;
            dormantObserver |= event == RHH_DORMANT_WILD_PROFILE_EDITED;
        }
        if ((liveConstraint && !dormantConstraint) || (liveObserver && !dormantObserver)) return 0;
    }
    return 1;
}
void RhhModsSetDrawing(int drawing) { sDrawing = drawing; }
int RhhModsSetLifecycleCallback(void (*callback)(void))
{
    if (!callback || sLifecycleCount >= sizeof(sLifecycleCallbacks) / sizeof(sLifecycleCallbacks[0])) return -1;
    for (unsigned i = 0; i < sLifecycleCount; ++i) if (sLifecycleCallbacks[i] == callback) return -1;
    sLifecycleCallbacks[sLifecycleCount++] = callback; return 0;
}
int RhhModsCommandsAllowed(void)
{ return !sDrawing && (!sDispatchDepth || (sDispatchDepth == 1 && sDispatchEvent == RHH_MOD_UPDATE)); }
uint32_t RhhModsCurrentEvent(void) { return sDispatchDepth ? sDispatchEvent : 0; }
RhhContentKey RhhModsCurrentOwner(void) { return sDispatchDepth ? sDispatchOwner : 0; }
int RhhModsArchiveObserverAllowed(void)
{
    if (!sDispatchDepth || sDrawing || sDispatchStage != RHH_OBSERVE) return 0;
    switch (sDispatchEvent)
    {
    case RHH_MOD_UPDATE:
    case RHH_WORLD_STEP: case RHH_WORLD_ENVIRONMENT_CHANGED: case RHH_NATURAL_WEATHER_CHANGED:
    case RHH_MON_EDITED: case RHH_MON_CAUGHT: case RHH_OWNED_EVOLUTION_COMMITTED:
    case RHH_OWNED_MON_CHANGED: case RHH_OWNED_MON_RELEASED: case RHH_OWNED_MON_MOVED:
    case RHH_OWNED_MON_PICKED_UP: case RHH_OWNED_STORAGE_UNREADABLE: case RHH_OWNED_GIFT_RECEIVED:
    case RHH_OWNED_DEBUG_CREATED: case RHH_OWNED_MYSTERY_GIFT_RECEIVED: case RHH_PARTY_REORDERED:
    case RHH_OWNED_DAYCARE_TRANSFERRED: case RHH_OWNED_FUSION_TRANSFERRED: case RHH_OWNED_CATCH_PLACED:
    case RHH_OWNED_INGAME_TRADE_COMPLETED: case RHH_OWNED_LINK_TRADE_SAVED:
    case RHH_BREED_EGG_CREATED: case RHH_EGG_HATCHED: case RHH_EGG_CYCLE_TICKED: case RHH_DAYCARE_PENDING_CHANGED:
    case RHH_WILD_GENERATED: case RHH_WILD_ACTOR_SPAWNED: case RHH_WILD_ACTOR_REMOVED:
    case RHH_WILD_ACTOR_ENGAGED: case RHH_WILD_ACTOR_BATTLE_ENDED: case RHH_WILD_ACTOR_IDENTIFIED:
    case RHH_WILD_ACTOR_STEP_FINISHED: case RHH_WILD_ACTIVITY_CHANGED: case RHH_WILD_TRAIT_EDITED:
    case RHH_WILD_PROFILE_EDITED: case RHH_DORMANT_WILD_PROFILE_EDITED: case RHH_WILD_QUEUE_CHANGED: case RHH_WILD_DORMANT_RELOCATED:
    case RHH_WILD_DORMANT_CLOCK_ADVANCED:
    case RHH_SCOUT_RECORDED: case RHH_PHOTO_CAPTURED: case RHH_PHOTO_STORED: case RHH_PHOTO_REMOVED:
    case RHH_PHOTO_REWARD_CLAIMED: case RHH_PHOTO_REWARD_DELIVERED:
    case RHH_PHOTO_EXPERIENCE_CLAIMED: case RHH_PHOTO_EXPERIENCE_DELIVERED: case RHH_PHOTO_EXPERIENCE_RETARGETED:
    case RHH_MOVE_RESOLVED: case RHH_BATTLE_TURN_ENDED: case RHH_BATTLE_STARTED:
    case RHH_BATTLER_ENTERED: case RHH_BATTLE_ENDED: case RHH_BATTLE_EVS_PROCESSED:
    case RHH_ITEM_EV_PROCESSED: case RHH_ITEM_EV_REDUCTION_PROCESSED: case RHH_EXPERIENCE_AWARDED:
    case RHH_MOVE_TAUGHT: case RHH_FIELD_HEALED: case RHH_FIELD_STATUS_CURED: case RHH_FIELD_POISON_DAMAGED:
    case RHH_BERRY_PLOT_WATERED: case RHH_BERRY_PLOT_MANUALLY_WATERED: case RHH_BERRY_PLOT_INTERACTED:
    case RHH_BERRY_PLOT_HARVESTED: case RHH_AUTHORED_BERRY_WATERED: case RHH_BERRY_CLOCK_UPDATED: case RHH_BATTLE_CONDITION_CHANGED:
    case RHH_BATTLE_ENVIRONMENT_COMMITTED: case RHH_BATTLE_FIELD_RULES_COMMITTED:
    case RHH_BOSS_ACE_CONFIGURED: case RHH_MUSIC_ASSIGNMENT_CHANGED: case RHH_MUSIC_LEVEL_CHANGED:
        return 1;
    default: return 0;
    }
}
int RhhModsSetUiInputOwnerProvider(RhhContentKey (*provider)(void))
{ if (sFinalized || !provider || sUiInputOwnerProvider) return -1; sUiInputOwnerProvider = provider; return 0; }
RhhContentKey RhhModsUiInputOwner(void) { return sUiInputOwnerProvider ? sUiInputOwnerProvider() : 0; }
int RhhModsCurrentStage(void) { return sDispatchDepth ? (int)sDispatchStage : -1; }
uint64_t RhhModsRunSeed(void) { return sRunSeed; }
uint64_t RhhModsRunId(void) { return sRunId; }
void RhhModsResetState(void)
{
    RunLifecycleCallbacks();
    memset(sLastTrace, 0, sizeof(sLastTrace));
    sSettingsLocked = sCommitted = 0; sRunSeed = sRunId = 0;
    for (unsigned i = 0; i < sModCount; ++i) memset(sMods[i].state, 0, sMods[i].def->state_size);
}
int RhhModsSettingsLocked(void) { return sSettingsLocked; }
void RhhModsRestoreSettingsLock(int locked) { sSettingsLocked = !!locked; }
static uint32_t Checksum(const uint8_t *p, uint32_t size)
{
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < size; ++i) hash = (hash ^ p[i]) * 16777619u;
    return hash;
}
static int SameDecision(const struct RhhModDecision *a, const struct RhhModDecision *b)
{
    return a->allowed == b->allowed && a->value == b->value
        && !memcmp(a->stats, b->stats, sizeof(a->stats)) && !strcmp(a->reason, b->reason);
}
void RhhModsDispatch(const struct RhhModEvent *event, struct RhhModDecision *decision)
{
    if (!sFinalized || event->struct_size < sizeof(*event) || decision->struct_size < sizeof(*decision)) return;
    uint32_t previousEvent = sDispatchEvent, previousStage = sDispatchStage;
    RhhContentKey previousOwner = sDispatchOwner;
    sDispatchEvent = event->kind;
    ++sDispatchDepth;
    if (event->kind == RHH_RUN_STARTED || event->kind == RHH_RUN_LOADED) sSettingsLocked = 1;
    int durableChanged = 0;
    if (sTrace && (event->kind == RHH_ENCOUNTER_STARTED || event->kind == RHH_MON_FAINTED
        || event->kind == RHH_RUN_STARTED || event->kind == RHH_RUN_LOADED))
    {
        fprintf(sTrace, "{\"seq\":%" PRIu64 ",\"event\":\"notification\",\"kind\":%u,\"seed\":\"%016" PRIx64 "\",\"key\":\"%016" PRIx64 "\",\"subject\":\"%016" PRIx64 "\",\"ordinal\":%" PRIu64 ",\"value\":%" PRIu64 "}\n",
            ++sTraceSequence, event->kind, event->run_seed, event->event_key, event->subject, event->ordinal, decision->value);
        fflush(sTrace);
    }
    for (unsigned i = 0; i < sHookCount; ++i)
    {
        struct Mod *m = &sMods[sHooks[i].mod];
        const struct RhhModHook *h = &m->def->hooks[sHooks[i].index];
        if (h->event != event->kind) continue;
        sDispatchStage = h->stage;
        sDispatchOwner = RhhContentKeyOf(m->def->id);
        struct RhhModDecision before = *decision;
        uint32_t stateBefore = h->stage == RHH_OBSERVE && (sTrace || (m->def->flags & RHH_MOD_DURABLE))
            ? Checksum(m->state, m->def->state_size) : 0;
        if (h->stage == RHH_OBSERVE) h->observe(event, decision, m->state);
        else
        {
            h->reduce(event, m->state, decision);
            decision->allowed = before.allowed && decision->allowed;
            if ((event->kind == RHH_BATTLE_ENVIRONMENT_SEED || event->kind == RHH_HABITAT_QUERY
                 || event->kind == RHH_WILD_CANDIDATE || event->kind == RHH_WILD_SCRIPTED_CANDIDATE
                 || event->kind == RHH_WILD_EVENT_CANDIDATE || event->kind == RHH_WILD_WEIGHT
                 || event->kind == RHH_AUTHORED_WILD_WEIGHT
                 || event->kind == RHH_CAN_GENERATE_WILD || event->kind == RHH_CAN_GENERATE_SPECIAL_WILD
                 || event->kind == RHH_CAN_GENERATE_DEXNAV_WILD || event->kind == RHH_CAN_GENERATE_FACILITY_WILD
                 || event->kind == RHH_CAN_GENERATE_MANUAL_FACILITY_WILD || event->kind == RHH_CAN_BEGIN_SCRIPTED_WILD
                 || event->kind == RHH_CAN_BEGIN_EVENT_WILD || event->kind == RHH_CAN_BEGIN_TUTORIAL_WILD
                 || event->kind == RHH_CAN_CLAIM_PHOTO_REWARD || event->kind == RHH_CAN_AWARD_EXPERIENCE
                 || event->kind == RHH_CAN_CLAIM_PHOTO_EXPERIENCE
                 || event->kind == RHH_CAN_RETARGET_PHOTO_EXPERIENCE
                 || event->kind == RHH_CAN_APPLY_BATTLE_EVS
                 || event->kind == RHH_PHOTO_SCORE || event->kind == RHH_WORLD_LIGHTS
                 || event->kind == RHH_WILD_TRAIT_QUERY || event->kind == RHH_WILD_BEHAVIOR_PROPOSE
                 || event->kind == RHH_WILD_PROFILE_SCHEMA || event->kind == RHH_WILD_PROFILE_SCHEMA_COLLECT
                 || event->kind == RHH_CAN_EDIT_DORMANT_WILD_PROFILE
                 || event->kind == RHH_TYPE_MATCHUP || event->kind == RHH_TYPE_STAB || event->kind == RHH_TYPE_STAB_EXTENDED
                 || event->kind == RHH_TYPE_LEGACY_CATEGORY
                 || event->kind == RHH_SPECIES_TYPES || event->kind == RHH_MOVE_BASE_TYPE
                 || event->kind == RHH_MOVE_POWER || event->kind == RHH_MOVE_ACCURACY
                 || event->kind == RHH_MOVE_BASE_PRIORITY || event->kind == RHH_MOVE_BATTLER_PRIORITY
                 || event->kind == RHH_BATTLER_SPEED || event->kind == RHH_INNATE_SPEED
                 || event->kind == RHH_DAMAGE_STAT || event->kind == RHH_INNATE_DAMAGE_STAT
                 || event->kind == RHH_MOVE_CRITICAL_STAGE || event->kind == RHH_INNATE_CRITICAL_STAGE
                 || event->kind == RHH_DAMAGE_FINAL || event->kind == RHH_INNATE_DAMAGE_FINAL
                 || event->kind == RHH_SELF_DAMAGE_FINAL || event->kind == RHH_INNATE_SELF_DAMAGE_FINAL
                 || event->kind == RHH_MOVE_BASE_TARGET || event->kind == RHH_MOVE_TARGET_RESOLVE || event->kind == RHH_WORLD_ENVIRONMENT_MOVE
                 || event->kind == RHH_AI_MOVE_SCORE || event->kind == RHH_MOVE_EFFECT_MODE
                 || event->kind == RHH_MOVE_DAMAGE_EFFECT_MODE || event->kind == RHH_AI_EFFECT_PREVIEW
                 || event->kind == RHH_INNATE_PROPOSE || event->kind == RHH_INNATE_PERSONALIZE
                 || event->kind == RHH_INNATE_BATTLE_PERSONALIZE || event->kind == RHH_INNATE_POWER
                 || event->kind == RHH_INNATE_ACCURACY || event->kind == RHH_INNATE_PRIORITY || event->kind == RHH_CAN_ACCEPT_DAYCARE_EGG
                 || event->kind == RHH_CAN_REQUEST_DAYCARE_EGG || event->kind == RHH_CAN_HATCH_EGG
                 || event->kind == RHH_EGG_CYCLE_PROGRESS
                 || event->kind == RHH_WILD_ACTIVITY_COLLECT || event->kind == RHH_CAN_CONFIGURE_BOSS_ACE
                 || event->kind == RHH_CAN_SET_MUSIC_ASSIGNMENT || event->kind == RHH_LEARNSET_ENTRY
                 || event->kind == RHH_WILD_ENCOUNTER_RATE || event->kind == RHH_WILD_SPAWN_INTERVAL
                 || event->kind == RHH_FISHING_BITE_RATE || event->kind == RHH_WORLD_GROUND_EFFECT || event->kind == RHH_BERRY_HARVEST_YIELD || event->kind == RHH_BATTLE_CONDITION_PROPOSAL
                 || event->kind == RHH_BREED_IV_COUNT || event->kind == RHH_BREED_NATURE || event->kind == RHH_BREED_IV_RULE
                 || event->kind == RHH_BREED_MOVE_RULE || event->kind == RHH_BREED_RNG_POLICY
                 || event->kind == RHH_BREED_BALL_RULE || event->kind == RHH_BREED_ABILITY_RULE
                 || event->kind == RHH_ITEM_EV_PROPOSAL || event->kind == RHH_CAN_REDUCE_ITEM_EVS
                 || event->kind == RHH_BATTLE_EV_PROPOSAL || event->kind == RHH_BATTLE_TERA_TYPE
                 || event->kind == RHH_MOVE_PP_COST || event->kind == RHH_AI_MOVE_PP_COST
                 || event->kind == RHH_CAN_SELECT_MOVE || event->kind == RHH_CAN_FIELD_HEAL || event->kind == RHH_CAN_CURE_FIELD_STATUS
                 || event->kind == RHH_FIELD_POISON_DAMAGE || event->kind == RHH_MUSIC_GAIN || event->kind == RHH_MUSIC_LOOP
                 || event->kind == RHH_CAN_SET_MUSIC_LEVEL || event->kind == RHH_CAN_SET_WILD_BEHAVIOR_POLICY)
                && h->stage == RHH_CONSTRAINT)
                decision->value = before.value;
            if (event->kind == RHH_LEVEL_CAP && decision->value > before.value) decision->value = before.value;
            if (event->kind == RHH_EV_LIMITS && h->stage == RHH_CONSTRAINT)
            {
                if (decision->value > before.value) decision->value = before.value;
                for (unsigned stat = 0; stat < RHH_STAT_COUNT; ++stat)
                    if (decision->stats[stat] > before.stats[stat]) decision->stats[stat] = before.stats[stat];
            }
            if (event->kind == RHH_MON_IS_DEAD) decision->value = before.value || decision->value;
            decision->reason[sizeof(decision->reason) - 1] = 0;
            if (!before.allowed) memcpy(decision->reason, before.reason, sizeof(decision->reason));
        }
        if (h->stage == RHH_OBSERVE && (m->def->flags & RHH_MOD_DURABLE)
            && stateBefore != Checksum(m->state, m->def->state_size)) durableChanged = 1;
        if (sTrace)
        {
            struct TraceCache *cached = &sLastTrace[i];
            if (h->stage != RHH_OBSERVE)
            {
                if (SameDecision(&before, decision)) continue;
                if (cached->valid && !memcmp(event, &cached->event, sizeof(*event))
                    && SameDecision(&before, &cached->before) && SameDecision(decision, &cached->after)) continue;
                stateBefore = Checksum(m->state, m->def->state_size);
                cached->valid = 1; cached->event = *event; cached->before = before; cached->after = *decision;
            }
            /* Module reasons are escaped so a trace remains valid JSONL. */
            char reason[sizeof(decision->reason) * 2]; unsigned at = 0;
            for (unsigned j = 0; j < sizeof(decision->reason) && decision->reason[j]; ++j)
            {
                unsigned char c = decision->reason[j];
                if (c == '"' || c == '\\') reason[at++] = '\\';
                if (c >= 32) reason[at++] = c;
            }
            reason[at] = 0;
            fprintf(sTrace, "{\"seq\":%" PRIu64 ",\"event\":%u,\"mod\":\"%s\",\"version\":%u,\"stage\":%u,\"seed\":\"%016" PRIx64 "\",\"key\":\"%016" PRIx64 "\",\"subject\":\"%016" PRIx64 "\",\"ordinal\":%" PRIu64 ",\"args\":[%u,%u,%u,%u],\"before\":\"%016" PRIx64 "\",\"after\":\"%016" PRIx64 "\",\"allowed\":%s,\"reason\":\"%s\",\"state_before\":%u,\"state_after\":%u,\"stats_before\":[%u,%u,%u,%u,%u,%u],\"stats_after\":[%u,%u,%u,%u,%u,%u]}\n",
                ++sTraceSequence, event->kind, m->def->id, m->def->mod_version, h->stage,
                event->run_seed, event->event_key, event->subject, event->ordinal,
                event->args[0], event->args[1], event->args[2], event->args[3],
                before.value, decision->value, decision->allowed ? "true" : "false", reason,
                stateBefore, h->stage == RHH_OBSERVE ? Checksum(m->state, m->def->state_size) : stateBefore,
                before.stats[0], before.stats[1], before.stats[2], before.stats[3], before.stats[4], before.stats[5],
                decision->stats[0], decision->stats[1], decision->stats[2], decision->stats[3], decision->stats[4], decision->stats[5]);
            fflush(sTrace);
        }
    }
    if (durableChanged && sCommitted && sPersist && event->kind != RHH_PERSISTENCE_FAILED && RhhModsFlushDurable())
    {
        struct RhhModEvent failed = {.struct_size = sizeof(failed), .kind = RHH_PERSISTENCE_FAILED, .run_seed = sRunSeed};
        struct RhhModDecision out = {.struct_size = sizeof(out), .allowed = 1};
        RhhModsDispatch(&failed, &out);
    }
    --sDispatchDepth;
    sDispatchEvent = previousEvent; sDispatchStage = previousStage; sDispatchOwner = previousOwner;
}
static void Put32(uint8_t *p, uint32_t n) { for (unsigned i = 0; i < 4; ++i) p[i] = n >> (8 * i); }
static uint32_t Get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int ExportRecords(uint8_t **data, uint32_t *size, uint32_t mask)
{
    uint32_t length = FILE_HEADER;
    struct { uint32_t schema, size; } projections[MAX_MODS] = {{0}};
    unsigned count = 0;
    for (unsigned i = 0; i < sModCount; ++i)
    {
        const struct Mod *m = &sMods[i];
        if (mask && !(m->def->flags & mask)) continue;
        if (m->def->validate && m->def->validate(m->state, m->def->state_size))
            return Fail("invalid live state for %s", m->def->id);
        projections[i].schema = m->def->state_schema; projections[i].size = m->def->state_size;
        if (sSaveProjection && sSaveProjection(m->def->id, m->state, m->def->state_size,
                                               &projections[i].schema, &projections[i].size))
            return Fail("cannot project save state for %s", m->def->id);
        if (projections[i].schema > m->def->state_schema || projections[i].size > m->def->state_size)
            return Fail("invalid save projection for %s", m->def->id);
        if (length > UINT32_MAX - RECORD_HEADER || projections[i].size > UINT32_MAX - RECORD_HEADER - length)
            return Fail("mod save records exceed their representable size");
        length += RECORD_HEADER + projections[i].size;
        ++count;
    }
    uint8_t *buffer = calloc(length, 1);
    if (!buffer) return Fail("cannot allocate save records");
    memcpy(buffer, "RHHMOD01", 8);
    Put32(buffer + 8, length);
    Put32(buffer + 12, count);
    uint32_t at = FILE_HEADER;
    /* Stable save ordering, independent of library discovery/registration. */
    const char *previous = "";
    for (unsigned n = 0; n < count; ++n)
    {
        int best = -1;
        for (unsigned i = 0; i < sModCount; ++i)
            if ((!mask || (sMods[i].def->flags & mask)) && strcmp(sMods[i].def->id, previous) > 0
                && (best < 0 || strcmp(sMods[i].def->id, sMods[best].def->id) < 0)) best = i;
        const struct Mod *m = &sMods[best];
        strcpy((char *)buffer + at, m->def->id);
        Put32(buffer + at + 64, projections[best].schema);
        Put32(buffer + at + 68, projections[best].size);
        Put32(buffer + at + 72, m->def->flags);
        Put32(buffer + at + 76, m->def->mod_version);
        memcpy(buffer + at + RECORD_HEADER, m->state, projections[best].size);
        at += RECORD_HEADER + projections[best].size;
        previous = m->def->id;
    }
    Put32(buffer + 16, Checksum(buffer + FILE_HEADER, length - FILE_HEADER));
    *data = buffer; *size = length;
    return 0;
}
static int ImportRecords(const uint8_t *data, uint32_t size, int merge)
{
    uint8_t *staged[MAX_MODS] = {0};
    uint8_t seen[MAX_MODS] = {0};
    int result = -1;
    if (size < FILE_HEADER || memcmp(data, "RHHMOD01", 8) || Get32(data + 8) != size
        || Get32(data + 12) > MAX_MODS || Get32(data + 20)
        || Checksum(data + FILE_HEADER, size - FILE_HEADER) != Get32(data + 16))
        return Fail("invalid mod save envelope");
    for (unsigned i = 0; i < sModCount; ++i)
    {
        if (!(staged[i] = calloc(sMods[i].def->state_size ? sMods[i].def->state_size : 1, 1)))
        { Fail("cannot stage mod migration"); goto cleanup; }
        if (merge) memcpy(staged[i], sMods[i].state, sMods[i].def->state_size);
    }
    uint32_t at = FILE_HEADER;
    for (uint32_t n = 0; n < Get32(data + 12); ++n)
    {
        if (size - at < RECORD_HEADER || !memchr(data + at, 0, 64) || !ValidName((const char *)data + at))
        { Fail("invalid mod save record"); goto cleanup; }
        uint32_t schema = Get32(data + at + 64), length = Get32(data + at + 68);
        uint32_t flags = Get32(data + at + 72);
        if (flags & ~(RHH_MOD_REQUIRED | RHH_MOD_DURABLE)) { Fail("unsupported saved mod flags"); goto cleanup; }
        if (length > size - at - RECORD_HEADER)
        { Fail("invalid mod state length"); goto cleanup; }
        int m = FindMod((const char *)data + at);
        if (m < 0 && (flags & RHH_MOD_REQUIRED))
        { Fail("save requires missing mod %s", data + at); goto cleanup; }
        if (m >= 0)
        {
            const struct RhhModDefinition *d = sMods[m].def;
            if (merge && (!(d->flags & RHH_MOD_DURABLE) || !d->merge))
            { Fail("journal contains non-durable module"); goto cleanup; }
            if (seen[m]) { Fail("duplicate saved mod"); goto cleanup; }
            seen[m] = 1;
            uint8_t *incoming = calloc(d->state_size ? d->state_size : 1, 1);
            if (!incoming) { Fail("cannot stage incoming module"); goto cleanup; }
            int bad = 0;
            if (schema == d->state_schema && length == d->state_size) memcpy(incoming, data + at + RECORD_HEADER, length);
            else if (!d->migrate || d->migrate(schema, data + at + RECORD_HEADER, length, incoming, d->state_size)) bad = 1;
            if (!bad && d->validate) bad = d->validate(incoming, d->state_size);
            if (!bad)
            {
                if (merge) bad = d->merge(staged[m], incoming, d->state_size);
                else memcpy(staged[m], incoming, d->state_size);
            }
            free(incoming);
            if (bad) { Fail("state migration or merge failed for %s", d->id); goto cleanup; }
            if (d->validate && d->validate(staged[m], d->state_size))
            { Fail("state validation failed for %s", d->id); goto cleanup; }
            if (sTrace) fprintf(sTrace, "{\"event\":\"load_state\",\"mod\":\"%s\",\"from_schema\":%u,\"to_schema\":%u}\n", d->id, schema, d->state_schema);
        }
        at += RECORD_HEADER + length;
    }
    if (at != size) { Fail("trailing mod save bytes"); goto cleanup; }
    for (unsigned i = 0; i < sModCount; ++i)
        if (sMods[i].def->validate && sMods[i].def->validate(staged[i], sMods[i].def->state_size))
        { Fail("default or merged state is invalid for %s", sMods[i].def->id); goto cleanup; }
    for (unsigned i = 0; i < sModCount; ++i) memcpy(sMods[i].state, staged[i], sMods[i].def->state_size);
    if (!merge) sSettingsLocked = 1;
    result = 0;
cleanup:
    for (unsigned i = 0; i < sModCount; ++i) free(staged[i]);
    return result;
}
int RhhModsExport(uint8_t **data, uint32_t *size) { return ExportRecords(data, size, 0); }
int RhhModsImport(const uint8_t *data, uint32_t size) { return ImportRecords(data, size, 0); }
int RhhModsMergeJournal(const uint8_t *data, uint32_t size) { return ImportRecords(data, size, 1); }
void RhhModsSetPersistence(int (*persist)(uint64_t, const uint8_t *, uint32_t)) { sPersist = persist; }
void RhhModsSetRunContext(uint64_t seed, uint64_t runId, int committed)
{ if (sRunId != runId) RunLifecycleCallbacks();
  sRunSeed = seed; sRunId = runId; sCommitted = committed; }
int RhhModsFlushDurable(void)
{
    if (!sCommitted || !sPersist) return 0;
    uint8_t *data; uint32_t size;
    if (ExportRecords(&data, &size, RHH_MOD_DURABLE)) return -1;
    int result = sPersist(sRunId, data, size); free(data);
    return result;
}
