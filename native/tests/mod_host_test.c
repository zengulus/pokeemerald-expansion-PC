#include "mod_host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s (%s)\n", __LINE__, #x, RhhModsError()); exit(1); } } while (0)

static void Add(const struct RhhModEvent *e, const void *s, struct RhhModDecision *d)
{ (void)e; (void)s; d->value += 7; }
static void Multiply(const struct RhhModEvent *e, const void *s, struct RhhModDecision *d)
{ (void)e; (void)s; d->value *= 3; }
static void Deny(const struct RhhModEvent *e, const void *s, struct RhhModDecision *d)
{ (void)e; (void)s; d->allowed = 0; strcpy(d->reason, "first encounter spent"); }
static void Allow(const struct RhhModEvent *e, const void *s, struct RhhModDecision *d)
{ (void)e; (void)s; d->allowed = 1; strcpy(d->reason, "override"); }
static void Count(const struct RhhModEvent *e, const struct RhhModDecision *d, void *s)
{ (void)e; (void)d; ++*(uint8_t *)s; }
static const struct RhhModHook sA[] = {
    {sizeof(struct RhhModHook), RHH_WILD_SPECIES, RHH_TRANSFORM, 0, Add, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_CAPTURE, RHH_CONSTRAINT, 0, Deny, NULL},
    {sizeof(struct RhhModHook), RHH_MON_FAINTED, RHH_OBSERVE, 0, NULL, Count}
};
static const struct RhhModHook sB[] = {
    {sizeof(struct RhhModHook), RHH_WILD_SPECIES, RHH_TRANSFORM, 0, Multiply, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_CAPTURE, RHH_CONSTRAINT, 0, Allow, NULL}
};
static const struct RhhModDefinition sDefA = {
    sizeof(struct RhhModDefinition), RHH_MOD_ABI_V1, 1, 1, "test:a", 1, 1, 3, 0, sA, NULL, NULL
};
static const struct RhhModDefinition sDefB = {
    sizeof(struct RhhModDefinition), RHH_MOD_ABI_V1, 1, 1, "test:b", 1, 0, 2, 0, sB, NULL, NULL
};
static void Register(int reverse)
{
    RhhModsInit();
    CHECK(!RhhModsHasHook(RHH_WILD_SPECIES) && !RhhModsHasHook(UINT32_MAX));
    CHECK(RhhModsRegister(reverse ? &sDefB : &sDefA) == 0);
    CHECK(RhhModsRegister(reverse ? &sDefA : &sDefB) == 0);
    CHECK(RhhModsFinalize() == 0);
    CHECK(RhhModsHasHook(RHH_WILD_SPECIES) && RhhModsHasHook(RHH_CAN_CAPTURE)
        && RhhModsHasHook(RHH_MON_FAINTED) && !RhhModsHasHook(RHH_MOVE_ADDITIONAL_EFFECTS)
        && !RhhModsHasHook(0) && !RhhModsHasHook(UINT32_MAX));
}
static int Migrate(uint32_t schema, const uint8_t *old, uint32_t length, uint8_t *dst, uint32_t size)
{
    if (schema != 1 || length != 1 || size != 2) return -1;
    dst[0] = old[0]; dst[1] = 42;
    return 0;
}
static uint8_t *sJournal;
static uint32_t sJournalSize;
static int sRejectPersist;
static int Persist(uint64_t run, const uint8_t *data, uint32_t size)
{
    CHECK(run == 42);
    if (sRejectPersist) return -1;
    free(sJournal); sJournal = malloc(size); CHECK(sJournal != NULL);
    memcpy(sJournal, data, size); sJournalSize = size; return 0;
}
static int ReadSetting(const void *state, const char *id, struct RhhModValue *out)
{
    const uint8_t *s = state;
    if (!strcmp(id, "enabled")) *out = (struct RhhModValue){RHH_SETTING_BOOL, 0, s[0]};
    else if (!strcmp(id, "style")) *out = (struct RhhModValue){RHH_SETTING_CHOICE, 0, s[1]};
    else return -1;
    return 0;
}
static int WriteSetting(void *state, const char *id, const struct RhhModValue *value)
{
    uint8_t *s = state;
    if (!strcmp(id, "enabled")) s[0] = value->value;
    else if (!strcmp(id, "style")) s[1] = value->value;
    else return -1;
    return 0;
}
static int ValidateDurable(const uint8_t *s, uint32_t size)
{ return size == 4 && s[0] <= 1 && s[1] <= 5 && s[3] <= 1 ? 0 : -1; }
static int MergeDurable(uint8_t *s, const uint8_t *journal, uint32_t size)
{
    if (ValidateDurable(s, size) || ValidateDurable(journal, size) || s[0] != journal[0]) return -1;
    if (journal[2] > s[2]) s[2] = journal[2];
    s[3] |= journal[3]; return 0;
}
static void ObserveDurable(const struct RhhModEvent *e, const struct RhhModDecision *d, void *state)
{
    (void)d; uint8_t *s = state;
    if (e->kind == RHH_MON_FAINTED) ++s[2];
    else if (e->kind == RHH_PERSISTENCE_FAILED) s[3] = 1;
}
static void CheckSettingsAndDurability(void)
{
    RhhModsInit();
    const struct RhhModSetting settings[] = {
        {.struct_size = sizeof(struct RhhModSetting), .type = RHH_SETTING_BOOL,
         .scope = RHH_SETTING_NEW_GAME, .id = "enabled", .label = "Enabled", .description = "Run setting", .maximum = 1},
        {.struct_size = sizeof(struct RhhModSetting), .type = RHH_SETTING_CHOICE,
         .scope = RHH_SETTING_RUNTIME, .id = "style", .label = "Style", .description = "Runtime setting", .maximum = 5}
    };
    const struct RhhModHook hooks[] = {
        {sizeof(struct RhhModHook), RHH_MON_FAINTED, RHH_OBSERVE, 0, NULL, ObserveDurable},
        {sizeof(struct RhhModHook), RHH_PERSISTENCE_FAILED, RHH_OBSERVE, 0, NULL, ObserveDurable}
    };
    const struct RhhModDefinition mod = {
        .struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_V1, .mod_version = 1, .state_schema = 1,
        .id = "test:durable", .flags = RHH_MOD_REQUIRED | RHH_MOD_DURABLE, .state_size = 4,
        .hook_count = 2, .hooks = hooks, .validate = ValidateDurable, .merge = MergeDurable,
        .setting_count = 2, .settings = settings, .read_setting = ReadSetting, .write_setting = WriteSetting
    };
    CHECK(RhhModsRegister(&mod) == 0 && RhhModsFinalize() == 0);
    struct RhhModValue value = {RHH_SETTING_BOOL, 0, 1};
    CHECK(RhhModsConfigure(mod.id, "enabled", &value) == 0);
    struct RhhModEvent e = {.struct_size = sizeof(e), .kind = RHH_RUN_STARTED};
    struct RhhModDecision d = {.struct_size = sizeof(d), .allowed = 1};
    RhhModsDispatch(&e, &d); value.value = 0;
    CHECK(RhhModsConfigure(mod.id, "enabled", &value) == -1);
    value.type = RHH_SETTING_CHOICE; value.value = 3;
    CHECK(RhhModsConfigure(mod.id, "style", &value) == 0);
    value.value = 6; CHECK(RhhModsConfigure(mod.id, "style", &value) == -1);
    CHECK(RhhModsGetSetting(mod.id, "style", &value) == 0 && value.value == 3);
    uint8_t *snapshot; uint32_t size; CHECK(RhhModsExport(&snapshot, &size) == 0);
    RhhModsSetPersistence(Persist); RhhModsSetRunContext(123, 42, 1);
    e.kind = RHH_MON_FAINTED; RhhModsDispatch(&e, &d);
    CHECK(sJournal != NULL && ((uint8_t *)RhhModsState(mod.id))[2] == 1);
    CHECK(RhhModsImport(snapshot, size) == 0 && ((uint8_t *)RhhModsState(mod.id))[2] == 0);
    CHECK(RhhModsMergeJournal(sJournal, sJournalSize) == 0 && ((uint8_t *)RhhModsState(mod.id))[2] == 1);
    sJournal[sJournalSize - 1] ^= 1;
    CHECK(RhhModsMergeJournal(sJournal, sJournalSize) == -1 && ((uint8_t *)RhhModsState(mod.id))[2] == 1);
    sJournal[sJournalSize - 1] ^= 1;
    sRejectPersist = 1; RhhModsDispatch(&e, &d);
    CHECK(((uint8_t *)RhhModsState(mod.id))[3] == 1);
    free(snapshot); free(sJournal); sJournal = NULL; RhhModsShutdown();
}
static void CheckShaderDeclarations(void)
{
    RhhModsInit();
    /* An actual short allocation catches accidental reads of the ABI extension. */
    struct RhhModDefinition *legacy = malloc(RHH_MOD_DEFINITION_BASE_SIZE);
    CHECK(legacy != NULL);
    memcpy(legacy, &sDefA, RHH_MOD_DEFINITION_BASE_SIZE);
    legacy->struct_size = RHH_MOD_DEFINITION_BASE_SIZE;
    CHECK(!RhhModsRegister(legacy));
    CHECK(!RhhModsFindShader(RhhContentKeyOf("test:shader/rain")));
    struct RhhModShader shaders[2] = {{sizeof(*shaders), 0, "test:shader/rain", "#version 120\nvoid main(){}"}};
    struct RhhModDefinition def = {.struct_size = sizeof(def), .abi_version = RHH_MOD_ABI_V1,
        .id = "test:shader_mod", .shader_count = 1, .shaders = shaders};
    shaders[0].struct_size = 0; CHECK(RhhModsRegister(&def)); shaders[0].struct_size = sizeof(*shaders);
    def.shader_count = 9; CHECK(RhhModsRegister(&def)); def.shader_count = 1;
    def.shaders = NULL; CHECK(RhhModsRegister(&def)); def.shaders = shaders;
    shaders[0].fragment = NULL; CHECK(RhhModsRegister(&def)); shaders[0].fragment = "void main(){}";
    shaders[1] = shaders[0]; def.shader_count = 2; CHECK(RhhModsRegister(&def)); def.shader_count = 1;
    struct RhhModShaderPhotoPolicy policies[2] = {{sizeof(*policies), RHH_SHADER_PHOTO_PRESERVES_POSITION, "test:shader/rain"}};
    def.shader_photo_policy_count = 1; def.shader_photo_policies = policies;
    def.shader_photo_policies = NULL; CHECK(RhhModsRegister(&def)); def.shader_photo_policies = policies;
    policies[0].flags = 0; CHECK(RhhModsRegister(&def)); policies[0].flags = 4; CHECK(RhhModsRegister(&def));
    policies[0].flags = RHH_SHADER_PHOTO_PRESERVES_POSITION;
    policies[0].shader_id = "test:shader/not_owned"; CHECK(RhhModsRegister(&def)); policies[0].shader_id = shaders[0].id;
    def.shader_photo_policy_reserved = 1; CHECK(RhhModsRegister(&def)); def.shader_photo_policy_reserved = 0;
    shaders[1] = (struct RhhModShader){sizeof(*shaders), 0, "test:shader/other", "void main(){}"};
    def.shader_count = 2; def.shader_photo_policy_count = 2; policies[1] = policies[0];
    CHECK(RhhModsRegister(&def)); def.shader_count = 1; def.shader_photo_policy_count = 1;
    CHECK(!RhhModsRegister(&def));
    CHECK(RhhModsFindShader(RhhContentKeyOf(shaders[0].id)) == shaders);
    CHECK(RhhModsShaderPhotoPreservesPosition(RhhContentKeyOf(shaders[0].id))
        && !RhhModsShaderPhotoPreservesPosition(RhhContentKeyOf("test:shader/missing")));
    def.id = "test:duplicate_shader_mod"; CHECK(RhhModsRegister(&def));
    struct RhhModShader geometryShader = {sizeof(geometryShader), 0, "test:shader/geometry", "void main(){}"};
    struct RhhModShaderPhotoPolicy geometryPolicy = {sizeof(geometryPolicy), RHH_SHADER_PHOTO_PRESERVES_GEOMETRY, "test:shader/geometry"};
    struct RhhModDefinition geometryDef = {.struct_size = sizeof(geometryDef), .abi_version = RHH_MOD_ABI_V1,
        .id = "test:geometry_shader", .shader_count = 1, .shaders = &geometryShader,
        .shader_photo_policy_count = 1, .shader_photo_policies = &geometryPolicy};
    CHECK(!RhhModsRegister(&geometryDef) && RhhModsShaderPhotoPreservesGeometry(RhhContentKeyOf(geometryShader.id))
        && !RhhModsShaderPhotoPreservesPosition(RhhContentKeyOf(geometryShader.id))
        && RhhModsShaderPhotoPreservesGeometry(RhhContentKeyOf(shaders[0].id)));
    RhhModsShutdown(); free(legacy);
}
static void CheckServices(void)
{
    RhhModsInit();
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    CHECK(host->struct_size >= RHH_MOD_HOST_V1_BASE_SIZE + sizeof(host->get_service));
    static const struct RhhServiceHeader service = {sizeof(service), RHH_SERVICE_VERSION(1, 2)};
    CHECK(RhhModsAddService("test:service/example", &service) == 0);
    CHECK(RhhModsAddService("test:service/example", &service) != 0);
    CHECK(host->get_service("test:service/example", RHH_SERVICE_VERSION(1, 0)) == &service);
    CHECK(host->get_service("test:service/example", RHH_SERVICE_VERSION(1, 2)) == &service);
    CHECK(!host->get_service("test:service/example", RHH_SERVICE_VERSION(1, 3)));
    CHECK(!host->get_service("test:service/example", RHH_SERVICE_VERSION(2, 0)));
    CHECK(!host->get_service("test:service/missing", RHH_SERVICE_VERSION(1, 0)));
    CHECK(RhhModsFinalize() == 0);
    CHECK(RhhModsAddService("test:service/late", &service) != 0);
    RhhModsShutdown();
    CHECK(!host->get_service("test:service/example", RHH_SERVICE_VERSION(1, 0)));
}
static void CheckServiceRequirements(void)
{
    RhhModsInit();
    static const struct RhhServiceHeader service = {sizeof(service), RHH_SERVICE_VERSION(1, 2)};
    struct RhhServiceRequirement reqs[] = {
        {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 2), RHH_SERVICE_REQUIRED, 0, "test:service/example"},
        {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(2, 0), 0, 0, "test:service/optional"}
    };
    struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_SERVICES,
        .id = "test:dependent", .service_requirement_count = 2, .service_requirements = reqs};
    mod.abi_version = RHH_MOD_ABI_V1; CHECK(RhhModsRegister(&mod));
    mod.abi_version = RHH_MOD_ABI_SERVICES;
    mod.struct_size = RHH_MOD_DEFINITION_WORLD_SIZE; CHECK(RhhModsRegister(&mod));
    mod.struct_size = sizeof(mod);
    reqs[0].minimum_version = 0; CHECK(RhhModsRegister(&mod));
    reqs[0].minimum_version = RHH_SERVICE_VERSION(1, 2);
    reqs[0].flags = 2; CHECK(RhhModsRegister(&mod)); reqs[0].flags = RHH_SERVICE_REQUIRED;
    reqs[1].id = reqs[0].id; CHECK(RhhModsRegister(&mod)); reqs[1].id = "test:service/optional";
    CHECK(!RhhModsRegister(&mod));
    CHECK(RhhModsFinalize());
    CHECK(strstr(RhhModsError(), "test:dependent") && strstr(RhhModsError(), "test:service/example")
        && strstr(RhhModsError(), "1.2"));
    CHECK(!RhhModsAddService("test:service/example", &service));
    reqs[0].minimum_version = RHH_SERVICE_VERSION(1, 3); CHECK(RhhModsFinalize());
    reqs[0].minimum_version = RHH_SERVICE_VERSION(2, 0); CHECK(RhhModsFinalize());
    reqs[0].minimum_version = RHH_SERVICE_VERSION(1, 2); CHECK(!RhhModsFinalize());
    CHECK(!RhhModsHostAPI()->get_service(reqs[1].id, reqs[1].minimum_version));
    RhhModsShutdown();

    /* An old world-presenter definition is a real short allocation. */
    struct RhhModDefinition *legacy = malloc(RHH_MOD_DEFINITION_WORLD_SIZE); CHECK(legacy);
    memcpy(legacy, &sDefA, RHH_MOD_DEFINITION_WORLD_SIZE);
    legacy->struct_size = RHH_MOD_DEFINITION_WORLD_SIZE;
    RhhModsInit(); CHECK(!RhhModsRegister(legacy)); CHECK(!RhhModsFinalize());
    RhhModsShutdown(); free(legacy);
}
static void CheckMusicDeclarations(void)
{
    RhhModsInit();
    struct RhhModDefinition *legacy = malloc(RHH_MOD_DEFINITION_SERVICE_SIZE); CHECK(legacy);
    memcpy(legacy, &sDefA, RHH_MOD_DEFINITION_SERVICE_SIZE);
    legacy->struct_size = RHH_MOD_DEFINITION_SERVICE_SIZE;
    legacy->abi_version = RHH_MOD_ABI_SERVICES;
    CHECK(!RhhModsRegister(legacy));
    struct RhhModMusic tracks[] = {{sizeof(struct RhhModMusic), 0, "test:music/one", "One", "songs/one.mid", "fonts/one.sf2"},
        {sizeof(struct RhhModMusic), 0, "test:music/two", "Two", "songs/two.mid", "fonts/two.sf2"}};
    struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_MUSIC,
        .id = "test:music_mod", .music_count = 2, .music = tracks};
    mod.abi_version = RHH_MOD_ABI_SERVICES; CHECK(RhhModsRegister(&mod)); mod.abi_version = RHH_MOD_ABI_MUSIC;
    mod.struct_size = RHH_MOD_DEFINITION_SERVICE_SIZE; CHECK(RhhModsRegister(&mod)); mod.struct_size = sizeof(mod);
    tracks[0].midi_path = "../one.mid"; CHECK(RhhModsRegister(&mod));
    tracks[0].midi_path = "/one.mid"; CHECK(RhhModsRegister(&mod));
    tracks[0].midi_path = "songs/../one.mid"; CHECK(RhhModsRegister(&mod));
    tracks[0].midi_path = "songs\\one.mid"; CHECK(RhhModsRegister(&mod));
    tracks[0].midi_path = "songs/one.mid";
    tracks[1].id = tracks[0].id; CHECK(RhhModsRegister(&mod)); tracks[1].id = "test:music/two";
    CHECK(!RhhModsRegister(&mod) && RhhModsMusicCount() == 2);
    RhhContentKey key = 0; const struct RhhModMusic *track = NULL;
    CHECK(!RhhModsMusicAt(1, &key, &track) && key == RhhContentKeyOf(tracks[1].id) && track == &tracks[1]);
    CHECK(RhhModsFindMusic(key) == &tracks[1] && RhhModsMusicAt(2, &key, &track));
    mod.id = "test:duplicate_music_mod"; CHECK(RhhModsRegister(&mod));
    CHECK(!RhhModsFinalize()); RhhModsShutdown(); free(legacy);

    /* An ABI 1.2 music module really ends before the new scene extension. */
    struct RhhModDefinition *oldMusic = malloc(RHH_MOD_DEFINITION_MUSIC_SIZE); CHECK(oldMusic);
    mod.id = "test:old_music"; mod.music_count = 1;
    memcpy(oldMusic, &mod, RHH_MOD_DEFINITION_MUSIC_SIZE);
    oldMusic->struct_size = RHH_MOD_DEFINITION_MUSIC_SIZE;
    RhhModsInit(); CHECK(!RhhModsRegister(oldMusic));
    CHECK(RhhModsMusicCount() == 1 && RhhModsFindMusic(RhhContentKeyOf(tracks[0].id)) == &tracks[0]);
    CHECK(!RhhModsFinalize()); RhhModsShutdown(); free(oldMusic);
}

static void CheckSceneRules(void)
{
    const struct RhhContentBinding content[] = {
        {"test:map/route", RHH_MAP, 7}, {"test:map/route_alias", RHH_MAP, 7},
        {"test:map/other", RHH_MAP, 8}, {"test:species/not_map", RHH_SPECIES, 7}
    };
    RhhContentKey route = RhhContentKeyOf(content[0].name), alias = RhhContentKeyOf(content[1].name);
    RhhContentKey other = RhhContentKeyOf(content[2].name);
    struct RhhModSceneRule rules[] = {
        {.struct_size = sizeof(*rules), .id = "test:scene/default", .priority = -1},
        {.struct_size = sizeof(*rules), .id = "test:scene/z", .match = RHH_SCENE_MATCH_MAP, .map = alias},
        {.struct_size = sizeof(*rules), .id = "test:scene/a", .match = RHH_SCENE_MATCH_MAP, .map = route},
        {.struct_size = sizeof(*rules), .id = "test:scene/tile", .match = RHH_SCENE_MATCH_MAP | RHH_SCENE_MATCH_METATILE,
         .map = route, .metatile = 4, .style = {RHH_SCENE_WATER, RHH_SCENE_UPPER_FLAT, RHH_SCENE_RECEIVES_SHADOW, -4, 0}},
        {.struct_size = sizeof(*rules), .id = "test:scene/position", .priority = 1,
         .match = RHH_SCENE_MATCH_MAP | RHH_SCENE_MATCH_POSITION, .map = alias, .x = 2, .y = 3,
         .style = {RHH_SCENE_SOLID, RHH_SCENE_UPPER_WALL_NORTH, RHH_SCENE_OCCLUDES, 8, 24}},
        {.struct_size = sizeof(*rules), .id = "test:scene/priority", .priority = 2,
         .match = RHH_SCENE_MATCH_BEHAVIOR, .behavior = 9}
    };
    struct RhhModDefinition first = {.struct_size = sizeof(first), .abi_version = RHH_MOD_ABI_SCENE,
        .id = "test:scene_first", .scene_rule_count = 3, .scene_rules = rules};
    struct RhhModDefinition second = {.struct_size = sizeof(second), .abi_version = RHH_MOD_ABI_SCENE,
        .id = "test:scene_second", .scene_rule_count = 3, .scene_rules = rules + 3};
    for (int reverse = 0; reverse < 2; ++reverse)
    {
        RhhModsInit(); CHECK(!RhhModsSetContent(content, 4));
        const struct RhhModSceneRule *selected = &rules[5];
        CHECK(RhhModsSelectSceneRule(route, 0, 0, 0, 0, &selected) && selected == &rules[5]);
        CHECK(!RhhModsRegister(reverse ? &second : &first));
        CHECK(!RhhModsRegister(reverse ? &first : &second)); CHECK(!RhhModsFinalize());
        CHECK(RhhModsSetContent(content, 4)); /* Resolved map identities stay fixed after finalization. */
        CHECK(!RhhModsSelectSceneRule(alias, 0, 0, 0, 0, &selected) && selected == &rules[2]);
        CHECK(!RhhModsSelectSceneRule(route, 0, 0, 4, 0, &selected) && selected == &rules[3]);
        CHECK(!RhhModsSelectSceneRule(route, 2, 3, 4, 0, &selected) && selected == &rules[4]);
        CHECK(!RhhModsSelectSceneRule(route, 2, 3, 4, 9, &selected) && selected == &rules[5]);
        CHECK(!RhhModsSelectSceneRule(other, 0, 0, 4, 0, &selected) && selected == &rules[0]);
        CHECK(RhhModsSelectSceneRule(0, 0, 0, 0, 0, &selected) && selected == &rules[0]);
        CHECK(RhhModsSelectSceneRule(route, 0, 0, 0, 0, NULL));
        RhhModsShutdown();
    }
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 4));
    struct RhhModSceneRule badRule = rules[4];
    struct RhhModDefinition bad = first; bad.id = "test:invalid_scene"; bad.scene_rule_count = 1; bad.scene_rules = &badRule;
    bad.abi_version = RHH_MOD_ABI_MUSIC; CHECK(RhhModsRegister(&bad)); bad.abi_version = RHH_MOD_ABI_SCENE;
    bad.struct_size = RHH_MOD_DEFINITION_MUSIC_SIZE; CHECK(RhhModsRegister(&bad)); bad.struct_size = sizeof(bad);
    bad.scene_rule_count = 257; CHECK(RhhModsRegister(&bad)); bad.scene_rule_count = 1;
    bad.scene_rules = NULL; CHECK(RhhModsRegister(&bad)); bad.scene_rules = &badRule;
    bad.scene_rule_reserved = 1; CHECK(RhhModsRegister(&bad)); bad.scene_rule_reserved = 0;
    badRule.struct_size = 0; CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    badRule.reserved[0] = 1; CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    badRule.match = 16; CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    badRule.match = RHH_SCENE_MATCH_POSITION; CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    badRule.map = RhhContentKeyOf(content[3].name); CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    badRule.map = RhhContentKeyOf("test:map/missing"); CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    badRule.x = -1; CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    badRule.metatile = 1; CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    badRule.style.upper = RHH_SCENE_UPPER_FLAT; CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    badRule.style.upper_height = 0; CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    badRule.style.flags = 8; CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    badRule.style.ground_height = 2049; CHECK(RhhModsRegister(&bad)); badRule = rules[4];
    CHECK(!RhhModsRegister(&first)); CHECK(RhhModsRegister(&first));
    badRule.id = rules[0].id; CHECK(RhhModsRegister(&bad));
    badRule = rules[4];
    const struct RhhModMusic music = {sizeof(music), 0, "test:music/scene", "Scene", "a.mid", "a.sf2"};
    bad.music_count = 1; bad.music = &music;
    CHECK(!RhhModsRegister(&bad) && RhhModsFindMusic(RhhContentKeyOf(music.id)) == &music);
    CHECK(!RhhModsFinalize());
    RhhModsShutdown();
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 4));
    CHECK(!RhhModsRegister(&bad)); CHECK(!RhhModsFinalize());
    const struct RhhModSceneRule *none = &badRule;
    CHECK(!RhhModsSelectSceneRule(other, 0, 0, 0, 0, &none) && !none);
    RhhModsShutdown();
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 4)); CHECK(!RhhModsRegister(&bad));
    CHECK(!RhhModsSetContent(content + 2, 2)); CHECK(RhhModsFinalize());
    CHECK(strstr(RhhModsError(), "missing map"));
    CHECK(!RhhModsSetContent(content, 4)); CHECK(!RhhModsFinalize()); RhhModsShutdown();
    puts("scene rules: validation, old music ABI, precedence and map aliases passed");
}
static void CheckSceneLights(void)
{
    const struct RhhContentBinding content[] = {
        {"test:map/route", RHH_MAP, 7}, {"test:map/alias", RHH_MAP, 7}, {"test:map/other", RHH_MAP, 8}
    };
    RhhContentKey route = RhhContentKeyOf(content[0].name), alias = RhhContentKeyOf(content[1].name);
    struct RhhModSceneLight a = {.struct_size = sizeof(a), .id = "test:light/z", .map = route,
        .x = 17, .y = 35, .height = 12, .radius = 48, .rgb = 0xff8040, .intensity = 600};
    struct RhhModSceneLight b = a; b.id = "test:light/a"; b.map = alias; b.period_mask = 1u << (RHH_TIME_NIGHT - 1);
    struct RhhModDefinition first = {.struct_size = RHH_MOD_DEFINITION_SCENE_LIGHT_SIZE, .abi_version = RHH_MOD_ABI_SCENE_LIGHTS,
        .id = "test:lights_first", .scene_light_count = 1, .scene_lights = &a};
    struct RhhModDefinition second = {.struct_size = sizeof(second), .abi_version = RHH_MOD_ABI_SCENE_LIGHTS,
        .id = "test:lights_second", .scene_light_count = 1, .scene_lights = &b};
    for (int reverse = 0; reverse < 2; ++reverse)
    {
        RhhModsInit(); CHECK(!RhhModsSetContent(content, 3));
        const struct RhhModSceneLight *found[RHH_SCENE_LIGHTS_PER_TILE] = {&b}; uint32_t count = 99;
        struct RhhModDefinition *oldFirst = malloc(RHH_MOD_DEFINITION_SCENE_LIGHT_SIZE);
        CHECK(oldFirst); memcpy(oldFirst, &first, RHH_MOD_DEFINITION_SCENE_LIGHT_SIZE);
        CHECK(RhhModsSceneLightsForTile(route, 1, 2, RHH_TIME_NIGHT, found, &count) && count == 99 && found[0] == &b);
        CHECK(!RhhModsRegister(reverse ? &second : oldFirst)); CHECK(!RhhModsRegister(reverse ? oldFirst : &second));
        struct RhhModSceneRule oldRule = {.struct_size = sizeof(oldRule), .id = "test:scene/old_rule"};
        struct RhhModDefinition old = {.struct_size = RHH_MOD_DEFINITION_SCENE_SIZE, .abi_version = RHH_MOD_ABI_SCENE,
            .id = "test:old_scene", .scene_rule_count = 1, .scene_rules = &oldRule};
        CHECK(!RhhModsRegister(&old) && !RhhModsFinalize());
        CHECK(!RhhModsSceneLightsForTile(alias, 1, 2, RHH_TIME_NIGHT, found, &count)
            && count == 2 && found[0] == &b && found[1] == &a);
        CHECK(!RhhModsSceneLightsForTile(route, 1, 2, RHH_TIME_DAY, found, &count) && count == 1 && found[0] == &a);
        CHECK(!RhhModsSceneLightsForTile(route, 1, 2, 0, found, &count) && count == 1 && found[0] == &a);
        count = 88; CHECK(RhhModsSceneLightsForTile(route, 1, 2, RHH_TIME_NIGHT + 1, found, &count) && count == 88);
        CHECK(!RhhModsSceneLightsForTile(route, 2, 2, RHH_TIME_NIGHT, found, &count) && !count);
        RhhModsShutdown(); free(oldFirst);
    }
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3));
    struct RhhModDefinition bad = first; struct RhhModSceneLight invalid = a;
    bad.id = "test:bad_lights"; bad.scene_lights = &invalid; bad.struct_size = sizeof(bad);
    bad.abi_version = RHH_MOD_ABI_SCENE; CHECK(RhhModsRegister(&bad)); bad.abi_version = RHH_MOD_ABI_SCENE_LIGHTS;
    bad.struct_size = RHH_MOD_DEFINITION_SCENE_SIZE; CHECK(RhhModsRegister(&bad)); bad.struct_size = sizeof(bad);
    bad.scene_light_reserved = 1; CHECK(RhhModsRegister(&bad)); bad.scene_light_reserved = 0;
    bad.scene_light_count = 65; CHECK(RhhModsRegister(&bad)); bad.scene_light_count = 1;
    bad.scene_lights = NULL; CHECK(RhhModsRegister(&bad)); bad.scene_lights = &invalid;
    invalid.radius = 0; CHECK(RhhModsRegister(&bad)); invalid = a;
    invalid.radius = 257; CHECK(RhhModsRegister(&bad)); invalid = a;
    invalid.rgb = 0x1000000; CHECK(RhhModsRegister(&bad)); invalid = a;
    invalid.intensity = 1001; CHECK(RhhModsRegister(&bad)); invalid = a;
    invalid.period_mask = 16; CHECK(RhhModsRegister(&bad)); invalid = a;
    invalid.x = -1; CHECK(RhhModsRegister(&bad)); invalid = a;
    invalid.height = 257; CHECK(RhhModsRegister(&bad)); invalid = a;
    invalid.map = RhhContentKeyOf("test:map/missing"); CHECK(RhhModsRegister(&bad)); invalid = a;
    invalid.reserved[0] = 1; CHECK(RhhModsRegister(&bad)); invalid = a;
    CHECK(!RhhModsRegister(&first)); b.id = a.id; CHECK(RhhModsRegister(&second)); b.id = "test:light/a";
    RhhModsShutdown();
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3));
    struct RhhModSceneLight crowded[9]; char ids[9][24];
    for (unsigned i = 0; i < 9; ++i) {
        snprintf(ids[i], sizeof(ids[i]), "test:light/crowd_%u", i); crowded[i] = a; crowded[i].id = ids[i]; crowded[i].map = alias;
    }
    struct RhhModDefinition group = {.struct_size = sizeof(group), .abi_version = RHH_MOD_ABI_SCENE_LIGHTS,
        .id = "test:lights_group", .scene_light_count = 9, .scene_lights = crowded};
    CHECK(RhhModsRegister(&group)); group.scene_light_count = 8; CHECK(!RhhModsRegister(&group));
    struct RhhModDefinition extra = {.struct_size = sizeof(extra), .abi_version = RHH_MOD_ABI_SCENE_LIGHTS,
        .id = "test:lights_extra", .scene_light_count = 1, .scene_lights = &crowded[8]};
    CHECK(RhhModsRegister(&extra)); crowded[8].map = RhhContentKeyOf(content[2].name);
    CHECK(!RhhModsRegister(&extra));
    const struct RhhContentBinding merged[] = {
        {"test:map/route", RHH_MAP, 7}, {"test:map/alias", RHH_MAP, 7}, {"test:map/other", RHH_MAP, 7}
    };
    CHECK(!RhhModsSetContent(merged, 3)); CHECK(RhhModsFinalize());
    CHECK(!RhhModsSetContent(content, 3) && !RhhModsFinalize() && RhhModsSceneLightCount() == 9);
    RhhModsShutdown();
    puts("scene lights: ABI compatibility, validation, aliasing and deterministic time filtering passed");
}
static void CheckEcologyTags(void)
{
    const struct RhhContentBinding content[] = {
        {"test:species/a", RHH_SPECIES, 7}, {"test:species/alias", RHH_SPECIES, 7},
        {"test:species/b", RHH_SPECIES, 8}
    };
    struct RhhModEcologyTag tags[] = {
        {sizeof(tags[0]), 0, "test:trait/wary", "Wary", "Avoids nearby players."},
        {sizeof(tags[0]), 0, "test:trait/nocturnal", "Nocturnal", "Active at night."}
    };
    struct RhhModSpeciesTag bindings[] = {
        {sizeof(bindings[0]), 0, RhhContentKeyOf(content[0].name), RhhContentKeyOf(tags[0].id)},
        {sizeof(bindings[0]), 0, RhhContentKeyOf(content[1].name), RhhContentKeyOf(tags[0].id)},
        {sizeof(bindings[0]), 0, RhhContentKeyOf(content[0].name), RhhContentKeyOf(tags[1].id)}
    };
    struct RhhModDefinition definitions = {.struct_size = sizeof(definitions), .abi_version = RHH_MOD_ABI_ECOLOGY,
        .id = "test:ecology_definitions", .ecology_tag_count = 2, .ecology_tags = tags};
    struct RhhModDefinition associations = {.struct_size = sizeof(associations), .abi_version = RHH_MOD_ABI_ECOLOGY,
        .id = "test:ecology_associations", .species_tag_count = 3, .species_tags = bindings};
    for (int reverse = 0; reverse < 2; ++reverse)
    {
        RhhModsInit(); CHECK(!RhhModsSetContent(content, 3) && !RhhModsInstallEcologyTagsService());
        const struct RhhEcologyTagsService *service = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/ecology_tags", RHH_SERVICE_VERSION(1, 0));
        CHECK(service && !service->count());
        CHECK(!RhhModsRegister(reverse ? &definitions : &associations));
        CHECK(!RhhModsRegister(reverse ? &associations : &definitions));
        CHECK(!RhhModsFinalize() && service->count() == 2);
        struct RhhEcologyTag tag = {.struct_size = sizeof(tag)};
        CHECK(!service->at(0, &tag) && !strcmp(tag.id, tags[1].id) && !strcmp(tag.label, "Nocturnal"));
        CHECK(!service->find(bindings[0].tag, &tag) && !strcmp(tag.id, tags[0].id));
        uint32_t result = 99;
        CHECK(!service->species_count(bindings[1].species, &result) && result == 2);
        CHECK(!service->species_at(bindings[0].species, 0, &tag) && !strcmp(tag.id, tags[1].id));
        CHECK(!service->species_at(bindings[0].species, 1, &tag) && !strcmp(tag.id, tags[0].id));
        CHECK(!service->species_has(bindings[1].species, bindings[0].tag, &result) && result == 1);
        CHECK(!service->species_has(RhhContentKeyOf(content[2].name), bindings[0].tag, &result) && !result);
        result = 99; CHECK(service->species_has(bindings[0].species, 42, &result) && result == 99);
        CHECK(service->species_count(42, &result) && result == 99);
        struct RhhEcologyTag before = tag;
        CHECK(service->species_at(bindings[0].species, 2, &tag) && !memcmp(&tag, &before, sizeof(tag)));
        tag.struct_size = sizeof(tag) - 1; before = tag;
        CHECK(service->at(0, &tag) && !memcmp(&tag, &before, sizeof(tag)));
        RhhModsShutdown();
    }
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3));
    struct RhhModDefinition bad = definitions; bad.id = "test:bad_ecology";
    bad.abi_version = RHH_MOD_ABI_SCENE_LIGHTS; CHECK(RhhModsRegister(&bad)); bad.abi_version = RHH_MOD_ABI_ECOLOGY;
    bad.struct_size = RHH_MOD_DEFINITION_SCENE_LIGHT_SIZE; CHECK(RhhModsRegister(&bad)); bad.struct_size = sizeof(bad);
    bad.ecology_tag_reserved = 1; CHECK(RhhModsRegister(&bad)); bad.ecology_tag_reserved = 0;
    bad.ecology_tag_count = 129; CHECK(RhhModsRegister(&bad)); bad.ecology_tag_count = 2;
    tags[1].id = tags[0].id; CHECK(RhhModsRegister(&bad)); tags[1].id = "test:trait/nocturnal";
    CHECK(!RhhModsRegister(&definitions)); CHECK(RhhModsRegister(&bad));
    bindings[0].tag = 42; CHECK(!RhhModsRegister(&associations)); CHECK(RhhModsFinalize());
    bindings[0].tag = RhhContentKeyOf(tags[0].id); bindings[0].species = 42; CHECK(RhhModsFinalize());
    bindings[0].species = RhhContentKeyOf(content[0].name); CHECK(!RhhModsFinalize());
    RhhModsShutdown();
    puts("ecology tags: validation, aliases, additive composition and deterministic enumeration passed");
}

static void CheckHabitatRegistry(void)
{
    const struct RhhContentBinding content[] = {
        {"test:map/route", RHH_MAP, 7}, {"test:map/alias", RHH_MAP, 7}, {"test:map/other", RHH_MAP, 8}
    };
    struct RhhModHabitat habitats[] = {
        {.struct_size = sizeof(habitats[0]), .id = "test:habitat/night_water", .label = "Night water",
         .description = "Rainy water at night.", .all_flags = RHH_HABITAT_WATER,
         .any_flags = RHH_HABITAT_GRASS | RHH_HABITAT_SHALLOW_WATER, .exclude_flags = RHH_HABITAT_INDOORS,
         .period_mask = 1u << (RHH_TIME_NIGHT - 1), .natural_weather_mask = 1u << RHH_NATURAL_WEATHER_RAIN,
         .weather_mask = 1u << RHH_WEATHER_RAIN, .terrain_mask = 1u << RHH_TERRAIN_NONE},
        {.struct_size = sizeof(habitats[0]), .id = "test:habitat/any", .label = "Anywhere", .description = "",
         .effective_weather_mask = (1u << RHH_HABITAT_WEATHER_RAIN) | (1u << RHH_HABITAT_WEATHER_THUNDERSTORM)}
    };
    habitats[0].map = RhhContentKeyOf(content[0].name);
    struct RhhModDefinition def = {.struct_size = sizeof(def), .abi_version = RHH_MOD_ABI_HABITATS,
        .id = "test:habitats", .habitat_count = 2, .habitats = habitats};
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3) && !RhhModsInstallHabitatRegistryService());
    const struct RhhHabitatRegistryService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/habitat_registry", RHH_SERVICE_VERSION(1, 0));
    CHECK(service && !service->count() && service->header.struct_size >= RHH_HABITAT_REGISTRY_SERVICE_V1_2_SIZE
        && RHH_HABITAT_REGISTRY_SERVICE_V1_0_SIZE < RHH_HABITAT_REGISTRY_SERVICE_V1_1_SIZE
        && RHH_HABITAT_REGISTRY_SERVICE_V1_1_SIZE < RHH_HABITAT_REGISTRY_SERVICE_V1_2_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/habitat_registry", RHH_SERVICE_VERSION(1, 1)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/habitat_registry", RHH_SERVICE_VERSION(1, 2)) == &service->header
        && !RhhModsHostAPI()->get_service("rhh:service/habitat_registry", RHH_SERVICE_VERSION(1, 4)));
    def.abi_version = RHH_MOD_ABI_ABILITIES; CHECK(RhhModsRegister(&def)); def.abi_version = RHH_MOD_ABI_HABITATS;
    def.struct_size = RHH_MOD_DEFINITION_ABILITIES_SIZE; CHECK(RhhModsRegister(&def)); def.struct_size = sizeof(def);
    habitats[0].exclude_flags |= RHH_HABITAT_WATER; CHECK(RhhModsRegister(&def)); habitats[0].exclude_flags &= ~RHH_HABITAT_WATER;
    habitats[0].period_mask = 16; CHECK(RhhModsRegister(&def)); habitats[0].period_mask = 1u << (RHH_TIME_NIGHT - 1);
    habitats[0].map = 42; CHECK(RhhModsRegister(&def)); habitats[0].map = RhhContentKeyOf(content[0].name);
    uint32_t savedMask = habitats[1].effective_weather_mask;
    habitats[1].effective_weather_mask = 1u << (RHH_HABITAT_WEATHER_UNKNOWN + 1); CHECK(RhhModsRegister(&def));
    habitats[1].effective_weather_mask = savedMask;
    habitats[1].id = habitats[0].id; CHECK(RhhModsRegister(&def)); habitats[1].id = "test:habitat/any";
    CHECK(!RhhModsRegister(&def) && !RhhModsFinalize() && service->count() == 2);
    struct RhhHabitatDefinition info = {.struct_size = sizeof(info)};
    CHECK(!service->at(0, &info) && !strcmp(info.id, habitats[1].id));
    uint32_t effectiveMask = 99;
    CHECK(!info.reserved2 && !service->effective_weather_mask(RhhContentKeyOf(habitats[1].id), &effectiveMask)
        && effectiveMask == savedMask);
    effectiveMask = 99; CHECK(service->effective_weather_mask(42, &effectiveMask) && effectiveMask == 99);
    CHECK(!service->find(RhhContentKeyOf(habitats[0].id), &info) && info.map == habitats[0].map);
    struct RhhHabitatDefinition before = info;
    CHECK(service->at(2, &info) && !memcmp(&before, &info, sizeof(info)));
    struct RhhHabitatSample sample = {.struct_size = sizeof(sample), .permitted = 1,
        .map = RhhContentKeyOf(content[1].name), .flags = RHH_HABITAT_WATER | RHH_HABITAT_SHALLOW_WATER,
        .natural_weather = RHH_NATURAL_WEATHER_RAIN, .environment_available = 1,
        .environment = {.struct_size = sizeof(sample.environment), .weather.value = RHH_WEATHER_RAIN}};
    uint32_t result = 99; RhhContentKey key = RhhContentKeyOf(habitats[0].id);
    {
        struct RhhHabitatEvaluation evaluation = {.struct_size = sizeof(evaluation)};
        struct RhhHabitatEvaluation tiny = {.struct_size = 1, .status = 99}, savedTiny = tiny;
        struct RhhHabitatSample test = sample;
        CHECK(!service->evaluate(key, &test, RHH_TIME_NIGHT, &evaluation)
            && evaluation.status == RHH_HABITAT_EVALUATION_MATCH && !evaluation.mismatch && !evaluation.unavailable);
        CHECK(!service->evaluate(key, &test, RHH_TIME_DAY, &evaluation)
            && evaluation.status == RHH_HABITAT_EVALUATION_MISMATCH && evaluation.mismatch == RHH_HABITAT_REASON_PERIOD);
        test.environment_available = 0;
        CHECK(!service->evaluate(key, &test, 0, &evaluation)
            && evaluation.status == RHH_HABITAT_EVALUATION_INDETERMINATE && !evaluation.mismatch
            && evaluation.unavailable == (RHH_HABITAT_REASON_PERIOD | RHH_HABITAT_REASON_SAVED_WEATHER | RHH_HABITAT_REASON_SAVED_TERRAIN));
        test.flags = RHH_HABITAT_INDOORS;
        test.natural_weather = RHH_NATURAL_WEATHER_SUNNY;
        test.map = RhhContentKeyOf(content[2].name); test.permitted = 0;
        CHECK(!service->evaluate(key, &test, 0, &evaluation)
            && evaluation.status == RHH_HABITAT_EVALUATION_MISMATCH
            && evaluation.mismatch == (RHH_HABITAT_REASON_PERMISSION | RHH_HABITAT_REASON_MAP
                | RHH_HABITAT_REASON_ALL_FLAGS | RHH_HABITAT_REASON_ANY_FLAGS | RHH_HABITAT_REASON_EXCLUDED_FLAGS
                | RHH_HABITAT_REASON_NATURAL_WEATHER)
            && evaluation.unavailable == (RHH_HABITAT_REASON_PERIOD | RHH_HABITAT_REASON_SAVED_WEATHER | RHH_HABITAT_REASON_SAVED_TERRAIN));
        test = sample; test.environment.weather.value = RHH_WEATHER_SUN; test.environment.terrain.value = RHH_TERRAIN_GRASSY;
        CHECK(!service->evaluate(key, &test, RHH_TIME_NIGHT, &evaluation)
            && evaluation.mismatch == (RHH_HABITAT_REASON_SAVED_WEATHER | RHH_HABITAT_REASON_SAVED_TERRAIN));
        RhhContentKey effectiveKey = RhhContentKeyOf(habitats[1].id);
        CHECK(!service->evaluate(effectiveKey, &test, RHH_TIME_DAY, &evaluation)
            && evaluation.status == RHH_HABITAT_EVALUATION_MISMATCH && evaluation.mismatch == RHH_HABITAT_REASON_EFFECTIVE_WEATHER);
        test.environment_available = 0;
        CHECK(!service->evaluate(effectiveKey, &test, RHH_TIME_DAY, &evaluation)
            && evaluation.status == RHH_HABITAT_EVALUATION_INDETERMINATE && evaluation.unavailable == RHH_HABITAT_REASON_EFFECTIVE_WEATHER);
        struct RhhAuthoredHabitatSample authored = {.struct_size = sizeof(authored), .time_available = 1,
            .map = {.struct_size = sizeof(authored.map), .map = sample.map, .natural_weather = sample.natural_weather},
            .sample = sample, .time = {.struct_size = sizeof(authored.time), .period = RHH_TIME_NIGHT}};
        CHECK(service->header.struct_size >= RHH_HABITAT_REGISTRY_SERVICE_V1_3_SIZE
            && RhhModsHostAPI()->get_service("rhh:service/habitat_registry", RHH_SERVICE_VERSION(1, 3)) == &service->header
            && !service->evaluate_authored(key, &authored, &evaluation) && evaluation.status == RHH_HABITAT_EVALUATION_MATCH);
        authored.map.natural_weather = authored.sample.natural_weather = RHH_NATURAL_WEATHER_DYNAMIC;
        CHECK(!service->evaluate_authored(key, &authored, &evaluation)
            && evaluation.status == RHH_HABITAT_EVALUATION_INDETERMINATE && !evaluation.mismatch
            && evaluation.unavailable == RHH_HABITAT_REASON_NATURAL_WEATHER);
        CHECK(!service->evaluate_authored(effectiveKey, &authored, &evaluation)
            && evaluation.status == RHH_HABITAT_EVALUATION_MATCH);
        authored.sample.environment.weather.value = RHH_WEATHER_NONE;
        CHECK(!service->evaluate_authored(effectiveKey, &authored, &evaluation)
            && evaluation.status == RHH_HABITAT_EVALUATION_INDETERMINATE
            && evaluation.unavailable == RHH_HABITAT_REASON_EFFECTIVE_WEATHER);
        authored.sample.environment.weather.value = RHH_WEATHER_SUN;
        CHECK(!service->evaluate_authored(effectiveKey, &authored, &evaluation)
            && evaluation.status == RHH_HABITAT_EVALUATION_MISMATCH && evaluation.mismatch == RHH_HABITAT_REASON_EFFECTIVE_WEATHER);
        authored.time_available = 0; authored.sample.map = authored.map.map = RhhContentKeyOf(content[2].name);
        authored.sample.environment.weather.value = RHH_WEATHER_RAIN;
        CHECK(!service->evaluate_authored(key, &authored, &evaluation)
            && evaluation.status == RHH_HABITAT_EVALUATION_MISMATCH && evaluation.mismatch == RHH_HABITAT_REASON_MAP
            && evaluation.unavailable == (RHH_HABITAT_REASON_NATURAL_WEATHER | RHH_HABITAT_REASON_PERIOD));
        struct RhhHabitatEvaluation authoredSaved = evaluation;
        authored.reserved[0] = 1;
        CHECK(service->evaluate_authored(key, &authored, &evaluation) && !memcmp(&evaluation, &authoredSaved, sizeof(evaluation)));
        struct RhhHabitatEvaluation beforeEvaluation = evaluation;
        CHECK(service->evaluate(key, &test, RHH_TIME_DAY, &tiny) && !memcmp(&tiny, &savedTiny, sizeof(tiny))
            && service->evaluate(42, &test, RHH_TIME_DAY, &evaluation) && !memcmp(&evaluation, &beforeEvaluation, sizeof(evaluation))
            && service->evaluate(key, NULL, RHH_TIME_DAY, &evaluation) && !memcmp(&evaluation, &beforeEvaluation, sizeof(evaluation))
            && service->evaluate(key, &test, RHH_TIME_NIGHT + 1, &evaluation) && !memcmp(&evaluation, &beforeEvaluation, sizeof(evaluation)));
        for (unsigned available = 0; available <= 1; ++available)
        for (unsigned period = 0; period <= RHH_TIME_NIGHT; ++period)
        for (unsigned flags = 0; flags < 8; ++flags)
        {
            test = sample; test.environment_available = available;
            test.flags = (flags & 1 ? RHH_HABITAT_WATER : 0) | (flags & 2 ? RHH_HABITAT_SHALLOW_WATER : 0)
                | (flags & 4 ? RHH_HABITAT_INDOORS : 0);
            CHECK(!service->evaluate(key, &test, period, &evaluation) && !service->match(key, &test, period, &result)
                && result == (evaluation.status == RHH_HABITAT_EVALUATION_MATCH));
        }
    }
    CHECK(!service->match(key, &sample, RHH_TIME_NIGHT, &result) && result == 1);
    CHECK(!service->match(key, &sample, RHH_TIME_DAY, &result) && !result);
    CHECK(!service->match(key, &sample, 0, &result) && !result);
    sample.environment_available = 0; CHECK(!service->match(key, &sample, RHH_TIME_NIGHT, &result) && !result);
    sample.environment_available = 1; sample.flags |= RHH_HABITAT_INDOORS;
    CHECK(!service->match(key, &sample, RHH_TIME_NIGHT, &result) && !result);
    sample.flags &= ~RHH_HABITAT_INDOORS; sample.map = RhhContentKeyOf(content[2].name);
    CHECK(!service->match(key, &sample, RHH_TIME_NIGHT, &result) && !result);
    sample.map = RhhContentKeyOf(content[1].name); sample.permitted = 0;
    CHECK(!service->match(RhhContentKeyOf(habitats[1].id), &sample, RHH_TIME_NIGHT, &result) && !result);
    result = 99; sample.permitted = 2; CHECK(service->match(key, &sample, RHH_TIME_NIGHT, &result) && result == 99);
    sample.permitted = 1; CHECK(service->match(42, &sample, RHH_TIME_NIGHT, &result) && result == 99);
    RhhContentKey rainKey = RhhContentKeyOf(habitats[1].id); sample.environment.weather.value = RHH_WEATHER_NONE;
    CHECK(!service->match(rainKey, &sample, RHH_TIME_DAY, &result) && result);
    sample.environment.weather.value = RHH_WEATHER_SUN;
    CHECK(!service->match(rainKey, &sample, RHH_TIME_DAY, &result) && !result);
    sample.environment.weather.value = RHH_WEATHER_RAIN; sample.natural_weather = RHH_NATURAL_WEATHER_SUNNY;
    CHECK(!service->match(rainKey, &sample, RHH_TIME_DAY, &result) && result);
    sample.environment.weather.value = RHH_WEATHER_NONE; sample.natural_weather = RHH_NATURAL_WEATHER_THUNDERSTORM;
    CHECK(!service->match(rainKey, &sample, RHH_TIME_DAY, &result) && result);
    sample.environment_available = 0;
    CHECK(!service->match(rainKey, &sample, RHH_TIME_DAY, &result) && !result);
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3) && !RhhModsRegister(&def));
    CHECK(!RhhModsSetContent(&content[2], 1) && RhhModsFinalize());
    CHECK(!RhhModsSetContent(content, 3) && !RhhModsFinalize());
    RhhModsShutdown(); puts("habitat registry: validation, matching, aliases and stable enumeration passed");
}

static void CheckCustomTypes(void)
{
    const struct RhhContentBinding content[] = {{"rhh:type/fire", RHH_TYPE, 11}, {"rhh:type/water", RHH_TYPE, 12}};
    RhhContentKey fire = RhhContentKeyOf(content[0].name), water = RhhContentKeyOf(content[1].name);
    RhhContentKey crystal = RhhContentKeyOf("example:type/crystal"), mist = RhhContentKeyOf("other:type/mist");
    struct RhhTypeRelation crystalRelations[] = {
        {sizeof(struct RhhTypeRelation), RHH_TYPE_ONE * 2, RHH_TYPE_ONE / 2, 0, fire},
        {sizeof(struct RhhTypeRelation), RHH_TYPE_ONE * 2, RHH_TYPE_ONE / 2, 0, mist},
    };
    struct RhhTypeRelation mistRelations[] = {
        {sizeof(struct RhhTypeRelation), RHH_TYPE_ONE / 2, RHH_TYPE_ONE * 2, 0, crystal},
    };
    struct RhhModType crystalType = {sizeof(crystalType), 0x8899cc, 2, 0, "example:type/crystal", "Crystal", crystalRelations};
    struct RhhModType mistType = {sizeof(mistType), 0xaaccff, 1, 0, "other:type/mist", "Mist", mistRelations};
    struct RhhModDefinition a = {.struct_size = sizeof(a), .abi_version = RHH_MOD_ABI_TYPES,
        .id = "example:types", .type_count = 1, .types = &crystalType};
    struct RhhModDefinition b = {.struct_size = sizeof(b), .abi_version = RHH_MOD_ABI_TYPES,
        .id = "other:types", .type_count = 1, .types = &mistType};
    for (unsigned reverse = 0; reverse < 2; ++reverse)
    {
        RhhModsInit(); CHECK(!RhhModsSetContent(content, 2) && !RhhModsInstallTypeCatalogService());
        const struct RhhTypeCatalogService *catalog = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/type_catalog", RHH_SERVICE_VERSION(1, 1));
        CHECK(catalog && !catalog->count() && catalog->header.struct_size >= RHH_TYPE_CATALOG_SERVICE_V1_1_SIZE
            && RhhModsHostAPI()->get_service("rhh:service/type_catalog", RHH_SERVICE_VERSION(1, 0)) == &catalog->header
            && !RhhModsHostAPI()->get_service("rhh:service/type_catalog", RHH_SERVICE_VERSION(1, 2)));
        struct RhhTypeDisplayInfo display = {.struct_size = sizeof(display), .rgb = 123}, oldDisplay = display;
        CHECK(catalog->describe(fire, &display) && !memcmp(&display, &oldDisplay, sizeof(display)));
        uint32_t slot = 999;
        CHECK(RhhModsCustomTypeSlot(crystal, &slot) && slot == 999 && !RhhModsCustomTypeKey(RHH_RUNTIME_TYPE_FIRST));
        struct RhhTypeDefinition result = {.struct_size = sizeof(result), .rgb = 123}, unchanged = result;
        CHECK(catalog->find(crystal, &result) && !memcmp(&result, &unchanged, sizeof(result)));
        CHECK(!RhhModsRegister(reverse ? &b : &a) && !RhhModsRegister(reverse ? &a : &b));
        CHECK(!RhhModsFinalize() && catalog->count() == 2);
        CHECK(!RhhModsCustomTypeSlot(crystal, &slot) && slot == RHH_RUNTIME_TYPE_FIRST
            && RhhModsCustomTypeKey(slot) == crystal);
        CHECK(!RhhModsCustomTypeSlot(mist, &slot) && slot == RHH_RUNTIME_TYPE_FIRST + 1
            && RhhModsCustomTypeKey(slot) == mist);
        CHECK(!catalog->at(0, &result) && result.key == crystal && !strcmp(result.id, crystalType.id)
            && !strcmp(result.label, "Crystal") && result.rgb == 0x8899cc);
        CHECK(!catalog->at(1, &result) && result.key == mist && !strcmp(result.id, mistType.id));
        unchanged = result; CHECK(catalog->at(2, &result) && !memcmp(&result, &unchanged, sizeof(result)));
        CHECK(!catalog->find(crystal, &result) && result.key == crystal);
        CHECK(!catalog->describe(fire, &display) && display.key == fire && !display.flags && !display.rgb
            && !strcmp(display.id, "rhh:type/fire") && !strcmp(display.label, "Fire"));
        CHECK(!catalog->describe(crystal, &display) && display.key == crystal
            && display.flags == (RHH_TYPE_DISPLAY_CUSTOM | RHH_TYPE_DISPLAY_COLOR)
            && display.rgb == crystalType.rgb && !strcmp(display.label, crystalType.label));
        oldDisplay = display; CHECK(catalog->describe(42, &display) && !memcmp(&display, &oldDisplay, sizeof(display)));
        display.struct_size = 1; oldDisplay = display;
        CHECK(catalog->describe(fire, &display) && !memcmp(&display, &oldDisplay, sizeof(display)));
        result.struct_size = 1; unchanged = result;
        CHECK(catalog->find(crystal, &result) && !memcmp(&result, &unchanged, sizeof(result)));
        uint32_t value = 99;
        CHECK(!RhhModsCustomTypeMatchup(crystal, fire, &value) && value == RHH_TYPE_ONE * 2);
        CHECK(!RhhModsCustomTypeMatchup(fire, crystal, &value) && value == RHH_TYPE_ONE / 2);
        CHECK(!RhhModsCustomTypeMatchup(crystal, mist, &value) && value == RHH_TYPE_ONE * 2);
        CHECK(!RhhModsCustomTypeMatchup(mist, crystal, &value) && value == RHH_TYPE_ONE / 2);
        CHECK(!RhhModsCustomTypeMatchup(water, crystal, &value) && value == RHH_TYPE_ONE);
        value = 99; CHECK(RhhModsCustomTypeMatchup(crystal, 42, &value) && value == 99);
        CHECK(RhhModsCustomTypeMatchup(fire, water, &value) && value == 99);
    }
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 2) && !RhhModsInstallTypeCatalogService());
    struct RhhModDefinition bad = a; bad.abi_version = RHH_MOD_ABI_ECOLOGY;
    CHECK(RhhModsRegister(&bad)); bad = a;
    crystalType.id = "rhh:type/crystal"; CHECK(RhhModsRegister(&bad)); crystalType.id = "example:type/crystal";
    crystalType.label = ""; CHECK(RhhModsRegister(&bad)); crystalType.label = "Crystal";
    crystalType.rgb = 0x1000000; CHECK(RhhModsRegister(&bad)); crystalType.rgb = 0x8899cc;
    crystalRelations[0].attack = 17; CHECK(RhhModsRegister(&bad)); crystalRelations[0].attack = RHH_TYPE_ONE * 2;
    crystalRelations[1].other = fire; CHECK(RhhModsRegister(&bad)); crystalRelations[1].other = mist;
    crystalRelations[0].attack = crystalRelations[0].defense = RHH_TYPE_UNSPECIFIED;
    CHECK(RhhModsRegister(&bad)); crystalRelations[0].attack = RHH_TYPE_ONE * 2; crystalRelations[0].defense = RHH_TYPE_ONE / 2;
    CHECK(!RhhModsRegister(&a)); CHECK(RhhModsRegister(&a));
    bad = b; bad.types = &crystalType; CHECK(RhhModsRegister(&bad));
    CHECK(RhhModsFinalize()); /* Missing mist must not publish a partial catalogue. */
    const struct RhhTypeCatalogService *catalog = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/type_catalog", RHH_SERVICE_VERSION(1, 0));
    CHECK(catalog && !catalog->count());
    CHECK(!RhhModsRegister(&b) && !RhhModsFinalize() && catalog->count() == 2);
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 2));
    mistRelations[0].defense = RHH_TYPE_ONE / 2;
    CHECK(!RhhModsRegister(&a) && !RhhModsRegister(&b) && RhhModsFinalize());
    mistRelations[0].defense = RHH_TYPE_ONE * 2;
    /* The catalog can hold more types than fit in a byte-sized engine slot. */
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 2));
    struct RhhModType many[RHH_CUSTOM_TYPE_LIMIT] = {0};
    char names[RHH_CUSTOM_TYPE_LIMIT][32], moduleIds[4][24];
    struct RhhModDefinition groups[4] = {0};
    for (unsigned i = 0; i < RHH_CUSTOM_TYPE_LIMIT; ++i)
    {
        snprintf(names[i], sizeof(names[i]), "test:type/many_%03u", i);
        many[i] = (struct RhhModType){sizeof(struct RhhModType), 0, 0, 0, names[i], "Test", NULL};
    }
    for (unsigned i = 0; i < 4; ++i)
    {
        snprintf(moduleIds[i], sizeof(moduleIds[i]), "test:many_%u", i);
        groups[i] = (struct RhhModDefinition){.struct_size = sizeof(groups[i]), .abi_version = RHH_MOD_ABI_TYPES,
            .id = moduleIds[i], .type_count = RHH_CUSTOM_TYPES_PER_MOD, .types = many + i * RHH_CUSTOM_TYPES_PER_MOD};
    }
    for (unsigned i = 4; i-- > 0;) CHECK(!RhhModsRegister(&groups[i]));
    CHECK(!RhhModsFinalize());
    uint32_t last = 999;
    RhhContentKey lastKey = RhhContentKeyOf(names[RHH_RUNTIME_TYPE_COUNT - 1]);
    CHECK(!RhhModsCustomTypeSlot(lastKey, &last) && last == 255 && RhhModsCustomTypeKey(last) == lastKey);
    last = 999; CHECK(RhhModsCustomTypeSlot(RhhContentKeyOf(names[RHH_RUNTIME_TYPE_COUNT]), &last)
        && last == 999 && !RhhModsCustomTypeKey(256));
    RhhModsShutdown(); CHECK(!RhhModsCustomTypeKey(RHH_RUNTIME_TYPE_FIRST));
    RhhModsInit();
    struct RhhModDefinition *legacy = calloc(1, RHH_MOD_DEFINITION_ECOLOGY_SIZE);
    CHECK(legacy); legacy->struct_size = RHH_MOD_DEFINITION_ECOLOGY_SIZE;
    legacy->abi_version = RHH_MOD_ABI_ECOLOGY; legacy->id = "test:legacy_ecology";
    const struct RhhModEcologyTag legacyTag = {sizeof(legacyTag), 0, "test:trait/old", "Old", "Legacy tag."};
    legacy->ecology_tag_count = 1; legacy->ecology_tags = &legacyTag;
    CHECK(!RhhModsInstallEcologyTagsService() && !RhhModsRegister(legacy) && !RhhModsFinalize());
    const struct RhhEcologyTagsService *ecology = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/ecology_tags", RHH_SERVICE_VERSION(1, 0));
    CHECK(ecology && ecology->count() == 1); RhhModsShutdown(); free(legacy);
}
static void CheckCustomMoves(void)
{
    const struct RhhContentBinding content[] = {{"rhh:move/water_gun", RHH_MOVE, 2}, {"rhh:type/water", RHH_TYPE, 3}};
    struct RhhModMove move = {.struct_size = sizeof(move), .power = 70, .accuracy = 95, .pp = 20,
        .category = RHH_MOVE_SPECIAL, .target = RHH_MOVE_TARGET_SELECTED,
        .template_move = RhhContentKeyOf("rhh:move/water_gun"), .type = RhhContentKeyOf("rhh:type/water"),
        .id = "test:move/tidal", .name = "Tidal", .description = "A water attack."};
    struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_MOVES,
        .id = "test:moves", .move_count = 1, .moves = &move};
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 2));
    struct RhhModDefinition old = mod; old.abi_version = RHH_MOD_ABI_TYPES;
    CHECK(RhhModsRegister(&old));
    move.struct_size = sizeof(move) - 1; CHECK(RhhModsRegister(&mod)); move.struct_size = sizeof(move);
    move.id = "rhh:move/stolen"; CHECK(RhhModsRegister(&mod)); move.id = "test:move/tidal";
    move.pp = 41; CHECK(RhhModsRegister(&mod)); move.pp = 20;
    move.power = 512; CHECK(RhhModsRegister(&mod)); move.power = 70;
    move.target = 999; CHECK(RhhModsRegister(&mod)); move.target = RHH_MOVE_TARGET_SELECTED;
    move.priority = 8; CHECK(RhhModsRegister(&mod)); move.priority = 0;
    CHECK(!RhhModsRegister(&mod) && RhhModsRegister(&mod));
    CHECK(!RhhModsFindCustomMove(RhhContentKeyOf(move.id)) && !RhhModsFinalize());
    CHECK(RhhModsFindCustomMove(RhhContentKeyOf(move.id)) == &move);
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 2));
    move.type = 42; CHECK(!RhhModsRegister(&mod) && RhhModsFinalize());
    move.type = RhhContentKeyOf("rhh:type/water"); move.template_move = 42;
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 2) && !RhhModsRegister(&mod) && RhhModsFinalize());
    RhhModsInit();
    struct RhhModDefinition *legacy = calloc(1, RHH_MOD_DEFINITION_TYPES_SIZE);
    CHECK(legacy); legacy->struct_size = RHH_MOD_DEFINITION_TYPES_SIZE;
    legacy->abi_version = RHH_MOD_ABI_TYPES; legacy->id = "test:legacy_type_abi";
    CHECK(!RhhModsRegister(legacy) && !RhhModsFinalize());
    RhhModsShutdown(); free(legacy);
}
static void CheckHatchHookValidation(void)
{
    RhhModsInit();
    struct RhhModHook hook = {sizeof(hook), RHH_EGG_HATCHED, RHH_TRANSFORM, 0, Add, NULL};
    struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_V1,
        .id = "test:hatch", .state_size = 1, .hook_count = 1, .hooks = &hook};
    CHECK(RhhModsRegister(&mod));
    hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize());
    RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_WORLD_STEP; hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod));
    hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_WILD_ACTOR_STEP_FINISHED; hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_FIELD_STATUS_CURED; hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_CAN_CURE_FIELD_STATUS; hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_CONSTRAINT;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_FIELD_POISON_DAMAGE; hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_FIELD_POISON_DAMAGED; hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_CAN_GENERATE_WILD; hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod));
    hook.stage = RHH_CONSTRAINT; CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_WILD_CANCELLED; hook.stage = RHH_CONSTRAINT;
    CHECK(RhhModsRegister(&mod));
    hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_CAN_GENERATE_SPECIAL_WILD; hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_CONSTRAINT;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_CAN_GENERATE_DEXNAV_WILD; hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_CONSTRAINT;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_CAN_GENERATE_FACILITY_WILD; hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_CONSTRAINT;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    const uint32_t specialNotifications[] = {RHH_WILD_SPECIAL_GENERATED, RHH_WILD_SPECIAL_CANCELLED,
        RHH_WILD_DEXNAV_GENERATED, RHH_WILD_DEXNAV_CANCELLED,
        RHH_WILD_FACILITY_GENERATED, RHH_WILD_FACILITY_CANCELLED};
    for (uint32_t i = 0; i < sizeof(specialNotifications) / sizeof(*specialNotifications); ++i)
    {
        RhhModsInit(); hook.event = specialNotifications[i]; hook.stage = RHH_CONSTRAINT; hook.reduce = Add; hook.observe = NULL;
        CHECK(RhhModsRegister(&mod)); hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
        CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsShutdown();
    }
    RhhModsInit(); hook.event = RHH_MOVE_ADDITIONAL_EFFECTS; hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_CONSTRAINT;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize() && RhhModsHasHook(RHH_MOVE_ADDITIONAL_EFFECTS));
    RhhModsShutdown(); CHECK(!RhhModsHasHook(RHH_MOVE_ADDITIONAL_EFFECTS));
    RhhModsInit(); hook.event = RHH_WILD_SPECIES; hook.stage = RHH_TRANSFORM;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize() && RhhModsHasHook(RHH_WILD_SPECIES)
        && !RhhModsHasHook(RHH_MOVE_ADDITIONAL_EFFECTS));
    RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_MOVE_RESOLVED; hook.stage = RHH_CONSTRAINT;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize() && RhhModsHasHook(RHH_MOVE_RESOLVED));
    RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_AI_MOVE_SCORE; hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize() && RhhModsHasHook(RHH_AI_MOVE_SCORE));
    RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_MOVE_TARGET_RESOLVE; hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize() && RhhModsHasHook(RHH_MOVE_TARGET_RESOLVE));
    RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_MOVE_EFFECT_MODE; hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize() && RhhModsHasHook(RHH_MOVE_EFFECT_MODE));
    RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_MOVE_EFFECT_APPLY; hook.stage = RHH_CONSTRAINT; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize() && RhhModsHasHook(RHH_MOVE_EFFECT_APPLY));
    RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_BATTLE_CONDITION_CHANGED; hook.stage = RHH_CONSTRAINT; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize() && RhhModsHasHook(RHH_BATTLE_CONDITION_CHANGED));
    RhhModsShutdown();
    RhhModsInit(); hook.event = RHH_REGISTERED_MOVE_EFFECT; hook.stage = RHH_CONSTRAINT; hook.reduce = Add; hook.observe = NULL;
    CHECK(RhhModsRegister(&mod)); hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize() && RhhModsHasHook(RHH_REGISTERED_MOVE_EFFECT));
    RhhModsShutdown();
}
static void CheckLearnsetDeclarations(void)
{
    const struct RhhContentBinding content[] = {
        {"rhh:species/torchic", RHH_SPECIES, 1}, {"rhh:move/water_gun", RHH_MOVE, 2},
        {"rhh:type/water", RHH_TYPE, 3}
    };
    struct RhhModMove move = {.struct_size = sizeof(move), .power = 50, .accuracy = 100, .pp = 20,
        .category = RHH_MOVE_SPECIAL, .target = RHH_MOVE_TARGET_SELECTED,
        .template_move = RhhContentKeyOf("rhh:move/water_gun"), .type = RhhContentKeyOf("rhh:type/water"),
        .id = "test:move/surge", .name = "Surge", .description = "A water attack."};
    struct RhhModLearnset entry = {sizeof(entry), RHH_LEARNSET_LEVEL, 50, 0,
        RhhContentKeyOf("rhh:species/torchic"), RhhContentKeyOf(move.id)};
    struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_LEARNSETS,
        .id = "test:learnsets", .move_count = 1, .moves = &move, .learnset_count = 1, .learnsets = &entry};
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3));
    struct RhhModDefinition old = mod; old.abi_version = RHH_MOD_ABI_MOVES; CHECK(RhhModsRegister(&old));
    entry.struct_size = sizeof(entry) - 1; CHECK(RhhModsRegister(&mod)); entry.struct_size = sizeof(entry);
    entry.level = 101; CHECK(RhhModsRegister(&mod)); entry.level = 50;
    entry.kind = RHH_LEARNSET_EGG; CHECK(RhhModsRegister(&mod)); entry.kind = RHH_LEARNSET_LEVEL;
    CHECK(!RhhModsRegister(&mod) && !RhhModsLearnsetCount() && !RhhModsFinalize()
        && RhhModsLearnsetCount() == 1 && RhhModsLearnsetAt(0) == &entry && !RhhModsLearnsetAt(1));
    uint32_t begin = 99, end = 99;
    CHECK(!RhhModsLearnsetRange(entry.species, RHH_LEARNSET_LEVEL, &begin, &end) && begin == 0 && end == 1);
    CHECK(!RhhModsLearnsetRange(entry.species, RHH_LEARNSET_EGG, &begin, &end) && begin == 1 && end == 1);
    CHECK(RhhModsLearnsetRange(0, RHH_LEARNSET_LEVEL, &begin, &end) && begin == 1 && end == 1);
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3)); entry.species = 42;
    CHECK(!RhhModsRegister(&mod) && RhhModsFinalize()); entry.species = RhhContentKeyOf("rhh:species/torchic");
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3)); entry.move = 42;
    CHECK(!RhhModsRegister(&mod) && RhhModsFinalize());
    RhhModsInit();
    struct RhhModDefinition *legacy = calloc(1, RHH_MOD_DEFINITION_MOVES_SIZE);
    CHECK(legacy); legacy->struct_size = RHH_MOD_DEFINITION_MOVES_SIZE;
    legacy->abi_version = RHH_MOD_ABI_MOVES; legacy->id = "test:legacy_moves";
    CHECK(!RhhModsRegister(legacy) && !RhhModsFinalize());
    RhhModsShutdown(); free(legacy);
}
static void CheckBattleConditionDeclarations(void)
{
    struct RhhModBattleCondition conditions[] = {
        {sizeof(conditions[0]), RHH_CONDITION_SIDE, 3, 0, "test:condition/ward", "Ward"},
        {sizeof(conditions[0]), RHH_CONDITION_FIELD, 5, 0, "test:condition/mist", "Mist"}
    };
    struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_CONDITIONS,
        .id = "test:conditions", .battle_condition_count = 2, .battle_conditions = conditions};
    RhhModsInit();
    struct RhhModDefinition old = mod; old.abi_version = RHH_MOD_ABI_LEARNSETS; CHECK(RhhModsRegister(&old));
    old = mod; old.struct_size = RHH_MOD_DEFINITION_LEARNSETS_SIZE; CHECK(RhhModsRegister(&old));
    conditions[0].scope = 0; CHECK(RhhModsRegister(&mod)); conditions[0].scope = RHH_CONDITION_SIDE;
    conditions[0].max_turns = 256; CHECK(RhhModsRegister(&mod)); conditions[0].max_turns = 3;
    conditions[0].id = "rhh:condition/ward"; CHECK(RhhModsRegister(&mod)); conditions[0].id = "test:condition/ward";
    conditions[1].id = conditions[0].id; CHECK(RhhModsRegister(&mod)); conditions[1].id = "test:condition/mist";
    CHECK(!RhhModsRegister(&mod));
    struct RhhModDefinition duplicate = mod; duplicate.id = "test:duplicate"; CHECK(RhhModsRegister(&duplicate));
    CHECK(!RhhModsBattleConditionCount() && !RhhModsFinalize() && RhhModsBattleConditionCount() == 2);
    int ward = RhhModsBattleConditionIndex(RhhContentKeyOf(conditions[0].id));
    int mist = RhhModsBattleConditionIndex(RhhContentKeyOf(conditions[1].id));
    CHECK(ward >= 0 && mist >= 0 && ward != mist && RhhModsBattleConditionAt(ward) == &conditions[0]
        && RhhModsBattleConditionAt(mist) == &conditions[1] && RhhModsBattleConditionIndex(0) < 0);
    RhhModsInit();
    struct RhhModDefinition *legacy = calloc(1, RHH_MOD_DEFINITION_LEARNSETS_SIZE);
    CHECK(legacy); legacy->struct_size = RHH_MOD_DEFINITION_LEARNSETS_SIZE;
    legacy->abi_version = RHH_MOD_ABI_LEARNSETS; legacy->id = "test:legacy_learnsets";
    CHECK(!RhhModsRegister(legacy) && !RhhModsFinalize());
    RhhModsShutdown(); free(legacy);
    legacy = calloc(1, RHH_MOD_DEFINITION_CONDITIONS_SIZE);
    CHECK(legacy); legacy->struct_size = RHH_MOD_DEFINITION_CONDITIONS_SIZE;
    legacy->abi_version = RHH_MOD_ABI_CONDITIONS; legacy->id = "test:legacy_conditions";
    legacy->battle_condition_count = 1; legacy->battle_conditions = conditions;
    RhhModsInit(); CHECK(!RhhModsRegister(legacy) && !RhhModsFinalize() && RhhModsBattleConditionCount() == 1);
    RhhModsShutdown(); free(legacy);
}
static int sRejectEffectMove;
static int ValidateTestEffectMove(RhhContentKey move, const struct RhhModMove *custom, uint32_t mode)
{ (void)custom; (void)mode; return !move || sRejectEffectMove ? -1 : 0; }
static void CheckAiPreviewStages(void)
{
    struct RhhModHook hook = {sizeof(hook), RHH_AI_EFFECT_PREVIEW, RHH_TRANSFORM, 0, Add, NULL};
    struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_SERVICES,
        .id = "test:ai_preview_stage", .hook_count = 1, .hooks = &hook};
    RhhModsInit(); CHECK(RhhModsRegister(&mod));
    hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count; CHECK(RhhModsRegister(&mod));
    hook.stage = RHH_CONSTRAINT; hook.reduce = Add; hook.observe = NULL;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize());
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_AI_EFFECT_PREVIEW};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = 15};
    RhhModsDispatch(&event, &decision); CHECK(decision.allowed && decision.value == 15);
    RhhModsShutdown();
    hook.event = RHH_WILD_ACTIVITY_COLLECT; hook.stage = RHH_TRANSFORM;
    RhhModsInit(); CHECK(RhhModsRegister(&mod));
    hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count; CHECK(RhhModsRegister(&mod));
    hook.stage = RHH_CONSTRAINT; hook.reduce = Add; hook.observe = NULL;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize());
    event.kind = RHH_WILD_ACTIVITY_COLLECT; decision.value = 15;
    RhhModsDispatch(&event, &decision); CHECK(decision.allowed && decision.value == 15);
    RhhModsShutdown();
    hook.event = RHH_WILD_ACTIVITY_CHANGED; hook.stage = RHH_CONSTRAINT; hook.reduce = Add; hook.observe = NULL;
    RhhModsInit(); CHECK(RhhModsRegister(&mod));
    hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count; mod.state_size = 1;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize());
    event.kind = RHH_WILD_ACTIVITY_CHANGED; RhhModsDispatch(&event, &decision);
    CHECK(*(uint8_t *)RhhModsState(mod.id) == 1); RhhModsShutdown();
    hook.event = RHH_CAN_CONFIGURE_BOSS_ACE; hook.stage = RHH_TRANSFORM; hook.reduce = Add; hook.observe = NULL;
    RhhModsInit(); CHECK(RhhModsRegister(&mod)); hook.stage = RHH_CONSTRAINT;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize());
    event.kind = RHH_CAN_CONFIGURE_BOSS_ACE; decision.value = 15;
    RhhModsDispatch(&event, &decision); CHECK(decision.value == 15); RhhModsShutdown();
    hook.event = RHH_BOSS_ACE_CONFIGURED; RhhModsInit(); CHECK(RhhModsRegister(&mod));
    hook.stage = RHH_OBSERVE; hook.reduce = NULL; hook.observe = Count;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize());
    event.kind = RHH_BOSS_ACE_CONFIGURED; RhhModsDispatch(&event, &decision);
    CHECK(*(uint8_t *)RhhModsState(mod.id) == 1); RhhModsShutdown();
}
static void CheckRegisteredMoveEffects(void)
{
    const struct RhhContentBinding content[] = {
        {"rhh:move/growl", RHH_MOVE, 1}, {"rhh:move/growl_alias", RHH_MOVE, 1},
        {"rhh:move/splash", RHH_MOVE, 2}
    };
    struct RhhModMoveEffect effect = {sizeof(effect), RHH_MOVE_EFFECT_CUSTOM_STATUS, {0, 0},
        "test:effect/shared", "Shared"};
    struct RhhModMoveEffectBinding bindings[] = {
        {sizeof(bindings[0]), 0, 0, 0}, {sizeof(bindings[1]), 0, 0, 0}
    };
    bindings[0].move = RhhContentKeyOf("rhh:move/growl"); bindings[0].effect = RhhContentKeyOf(effect.id);
    bindings[1].move = RhhContentKeyOf("rhh:move/splash"); bindings[1].effect = bindings[0].effect;
    struct RhhModDefinition provider = {.struct_size = sizeof(provider), .abi_version = RHH_MOD_ABI_MOVE_EFFECTS,
        .id = "test:effect_provider", .move_effect_count = 1, .move_effects = &effect};
    struct RhhModDefinition consumer = {.struct_size = sizeof(consumer), .abi_version = RHH_MOD_ABI_MOVE_EFFECTS,
        .id = "test:effect_consumer", .move_effect_binding_count = 2, .move_effect_bindings = bindings};
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3) && !RhhModsSetMoveEffectValidator(ValidateTestEffectMove));
    struct RhhModDefinition invalid = provider; invalid.abi_version = RHH_MOD_ABI_CONDITIONS;
    CHECK(RhhModsRegister(&invalid)); invalid = provider; invalid.struct_size = RHH_MOD_DEFINITION_CONDITIONS_SIZE;
    CHECK(RhhModsRegister(&invalid)); effect.mode = RHH_MOVE_EFFECT_INHERITED; CHECK(RhhModsRegister(&provider));
    effect.mode = RHH_MOVE_EFFECT_CUSTOM_DAMAGE; CHECK(RhhModsRegister(&provider));
    effect.mode = RHH_MOVE_EFFECT_CUSTOM_STATUS; effect.id = "rhh:effect/shared"; CHECK(RhhModsRegister(&provider));
    effect.id = "test:effect/shared"; effect.reserved[0] = 1; CHECK(RhhModsRegister(&provider)); effect.reserved[0] = 0;
    bindings[1].move = bindings[0].move; CHECK(RhhModsRegister(&consumer)); bindings[1].move = RhhContentKeyOf("rhh:move/splash");
    CHECK(!RhhModsRegister(&consumer) && !RhhModsRegister(&provider));
    struct RhhModDefinition duplicate = provider; duplicate.id = "test:duplicate_effect";
    CHECK(RhhModsRegister(&duplicate)); duplicate = consumer; duplicate.id = "test:duplicate_binding";
    CHECK(RhhModsRegister(&duplicate));
    CHECK(!RhhModsRegisteredEffectCount() && !RhhModsFinalize());
    CHECK(RhhModsRegisteredEffectCount() == 1 && RhhModsRegisteredEffectAt(0) == &effect
        && RhhModsRegisteredEffect(bindings[0].effect) == &effect && !RhhModsRegisteredEffectAt(1)
        && RhhModsRegisteredEffectForMove(bindings[0].move) == bindings[0].effect
        && RhhModsRegisteredEffectForMove(bindings[1].move) == bindings[1].effect
        && !RhhModsRegisteredEffectForMove(0));
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3) && !RhhModsSetMoveEffectValidator(ValidateTestEffectMove));
    CHECK(!RhhModsRegister(&consumer) && RhhModsFinalize()); /* Missing effect. */
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3) && !RhhModsSetMoveEffectValidator(ValidateTestEffectMove));
    bindings[0].move = RhhContentKeyOf("rhh:move/growl_alias");
    CHECK(!RhhModsRegister(&consumer) && !RhhModsRegister(&provider) && RhhModsFinalize());
    bindings[0].move = RhhContentKeyOf("rhh:move/growl");
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3) && !RhhModsSetMoveEffectValidator(ValidateTestEffectMove));
    sRejectEffectMove = 1;
    CHECK(!RhhModsRegister(&provider) && !RhhModsRegister(&consumer) && RhhModsFinalize());
    sRejectEffectMove = 0;
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3));
    CHECK(!RhhModsRegister(&provider) && !RhhModsRegister(&consumer) && RhhModsFinalize());
    RhhModsInit(); CHECK(!RhhModsSetContent(content, 3) && !RhhModsSetMoveEffectValidator(ValidateTestEffectMove));
    effect.mode = RHH_MOVE_EFFECT_CUSTOM_DAMAGE; provider.abi_version = RHH_MOD_ABI_DAMAGE_EFFECTS;
    CHECK(!RhhModsRegister(&provider) && !RhhModsRegister(&consumer) && !RhhModsFinalize());
    CHECK(RhhModsRegisteredEffect(bindings[0].effect)->mode == RHH_MOVE_EFFECT_CUSTOM_DAMAGE);
    RhhModsShutdown();
}
static void CheckCustomAbilities(void)
{
    struct RhhModAbility entries[] = {
        {.struct_size = sizeof(entries[0]), .flags = RHH_ABILITY_BREAKABLE,
         .id = "test:ability/zeta", .label = "Zeta", .description = "An example ability."},
        {.struct_size = sizeof(entries[1]), .flags = RHH_ABILITY_CANNOT_SUPPRESS,
         .id = "test:ability/alpha", .label = "Alpha", .description = "Another ability."}
    };
    struct RhhModMoveEffect effect = {sizeof(effect), RHH_MOVE_EFFECT_CUSTOM_DAMAGE, {0, 0}, "test:effect/with_ability", "Effect"};
    struct RhhModDefinition provider = {.struct_size = sizeof(provider), .abi_version = RHH_MOD_ABI_ABILITIES,
        .id = "test:ability_provider", .ability_count = 2, .abilities = entries,
        .move_effect_count = 1, .move_effects = &effect};
    RhhModsInit();
    struct RhhModDefinition invalid = provider; invalid.abi_version = RHH_MOD_ABI_DAMAGE_EFFECTS;
    CHECK(RhhModsRegister(&invalid));
    invalid = provider; invalid.struct_size = RHH_MOD_DEFINITION_MOVE_EFFECTS_SIZE;
    CHECK(RhhModsRegister(&invalid));
    invalid = provider; invalid.struct_size = RHH_MOD_DEFINITION_MOVE_EFFECTS_SIZE + 1;
    CHECK(RhhModsRegister(&invalid));
    entries[0].id = "rhh:ability/reserved"; CHECK(RhhModsRegister(&provider));
    entries[0].id = "test:ability/zeta"; entries[0].flags = 128; CHECK(RhhModsRegister(&provider));
    entries[0].flags = RHH_ABILITY_BREAKABLE; entries[0].reserved[0] = 1; CHECK(RhhModsRegister(&provider));
    entries[0].reserved[0] = 0; entries[1].id = entries[0].id; CHECK(RhhModsRegister(&provider));
    entries[1].id = "test:ability/alpha"; CHECK(!RhhModsRegister(&provider));
    struct RhhModDefinition duplicate = provider; duplicate.id = "test:duplicate_ability";
    duplicate.move_effect_count = 0; duplicate.move_effects = NULL; CHECK(RhhModsRegister(&duplicate));
    CHECK(!RhhModsCustomAbilityCount() && !RhhModsCustomAbility(RhhContentKeyOf(entries[0].id)));
    CHECK(!RhhModsFinalize() && RhhModsCustomAbilityCount() == 2
        && RhhModsCustomAbilityAt(0) == &entries[1] && RhhModsCustomAbilityAt(1) == &entries[0]
        && !RhhModsCustomAbilityAt(2) && RhhModsCustomAbility(RhhContentKeyOf(entries[0].id)) == &entries[0]);
    RhhModsInit();
    struct RhhModDefinition *oldAbility = malloc(RHH_MOD_DEFINITION_ABILITIES_SIZE);
    CHECK(oldAbility); memcpy(oldAbility, &provider, RHH_MOD_DEFINITION_ABILITIES_SIZE);
    oldAbility->struct_size = RHH_MOD_DEFINITION_ABILITIES_SIZE;
    CHECK(!RhhModsRegister(oldAbility) && !RhhModsFinalize() && RhhModsCustomAbilityCount() == 2);
    RhhModsInit(); free(oldAbility);
    struct RhhModDefinition legacy = provider; legacy.abi_version = RHH_MOD_ABI_DAMAGE_EFFECTS;
    legacy.struct_size = RHH_MOD_DEFINITION_MOVE_EFFECTS_SIZE;
    CHECK(!RhhModsRegister(&legacy) && !RhhModsFinalize() && !RhhModsCustomAbilityCount());
    RhhModsInit();
    const struct RhhContentBinding content[] = {{"test:ability/zeta", RHH_ABILITY, 123}};
    CHECK(!RhhModsSetContent(content, 1) && RhhModsRegister(&provider));
    RhhModsInit(); CHECK(!RhhModsRegister(&provider) && !RhhModsSetContent(content, 1) && RhhModsFinalize());
    RhhModsShutdown();
}
static unsigned sWorldDraws, sWorldShutdown;
static int sWorldFails;
static void SelectWorldTest(const struct RhhModEvent *event, const void *state, struct RhhModDecision *out)
{ (void)event; (void)state; out->value = RhhContentKeyOf("test:world/presenter"); }
static int DrawWorldTest(const struct RhhWorldFrame *frame, const void *state)
{ (void)frame; (void)state; ++sWorldDraws; return sWorldFails; }
static void ShutdownWorldTest(void) { ++sWorldShutdown; }
static void *WorldProcTest(const char *name) { (void)name; return NULL; }
static const struct RhhWorldRenderService *sDynamicRender;
static struct RhhWorldLightFrame sCapturedLights;
static RhhContentKey sDynamicMap, sDynamicAlias;
static unsigned sDynamicMode, sDynamicCalls;
static int sExpectNoLightFrame;
static const struct RhhWorldFrame *sReentrantLightFrame;
static int DynamicCamera(struct RhhWorldCamera *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    *out = (struct RhhWorldCamera){.struct_size = sizeof(*out), .map = sDynamicMap,
        .viewport_width = 240, .viewport_height = 160, .tile_pixels = 16, .origin_x = -32, .origin_y = -48}; return 0;
}
static int DynamicInputs(struct RhhWorldLightingInputs *out)
{
    if (!out || out->struct_size < sizeof(*out)) return -1;
    *out = (struct RhhWorldLightingInputs){.struct_size = sizeof(*out), .map = sDynamicMap,
        .flags = RHH_WORLD_LIGHT_OUTDOORS, .time_available = 1,
        .time = {.struct_size = sizeof(out->time), .period = RHH_TIME_NIGHT, .effective_hour = 22}}; return 0;
}
static void OfferDynamicTest(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; ++sDynamicCalls;
    struct RhhWorldLightContext context = {.struct_size = sizeof(context)};
    CHECK(!RhhModsCommandsAllowed() && !sDynamicRender->light_context(&context)
        && event->ordinal == context.frame && event->event_key == sDynamicMap && event->args[0] == RHH_TIME_NIGHT
        && context.camera.map == sDynamicMap && context.lighting.time.effective_hour == 22);
    struct RhhWorldDynamicLight light = {.struct_size = sizeof(light), .id = 20, .map = sDynamicAlias,
        .x = 12, .y = -7, .z = 16, .radius = 48, .rgb = 0xffaa44, .intensity = 700};
    CHECK(!sDynamicRender->offer_light(&light));
    light.id = 10; CHECK(!sDynamicRender->offer_light(&light));
    if (sDynamicMode == 1) CHECK(sDynamicRender->offer_light(&light)); /* duplicate */
    if (sDynamicMode == 2) { light.id = 30; light.radius = 0; CHECK(sDynamicRender->offer_light(&light)); }
    if (sDynamicMode == 3) decision->allowed = 0;
    if (sDynamicMode == 4) {
        for (unsigned i = 0; i < RHH_WORLD_DYNAMIC_LIGHT_CAPACITY - 2; ++i) {
            light.id = 100 + i; CHECK(!sDynamicRender->offer_light(&light));
        }
        light.id = 1000; CHECK(sDynamicRender->offer_light(&light));
    }
    if (sDynamicMode == 5) { light.id = RhhContentKeyOf("test:light/static"); CHECK(sDynamicRender->offer_light(&light)); }
    if (sDynamicMode == 6) { light.id = 30; light.map = RhhContentKeyOf("test:map/other"); CHECK(sDynamicRender->offer_light(&light)); }
    if (sDynamicMode == 7) { light.id = 30; light.x = 1000001; CHECK(sDynamicRender->offer_light(&light)); }
    if (sDynamicMode == 8) { light.id = 30; light.reserved2 = 1; CHECK(sDynamicRender->offer_light(&light)); }
    if (sDynamicMode == 9) { light.id = 30; light.rgb = 0; CHECK(sDynamicRender->offer_light(&light)); }
    if (sDynamicMode == 10) CHECK(!RhhModsDrawWorld(sReentrantLightFrame));
}
static int DrawDynamicTest(const struct RhhWorldFrame *frame, const void *state)
{
    (void)state; sCapturedLights = (struct RhhWorldLightFrame){.struct_size = sizeof(sCapturedLights)};
    if (sExpectNoLightFrame) { CHECK(sDynamicRender->frame_lights(&sCapturedLights)); return 0; }
    CHECK(!sDynamicRender->frame_lights(&sCapturedLights) && sCapturedLights.context.frame == frame->frame);
    struct RhhWorldLightFrame shortFrame = {.struct_size = 1, .count = 88}, saved = shortFrame;
    CHECK(sDynamicRender->frame_lights(&shortFrame) && !memcmp(&shortFrame, &saved, sizeof(saved)));
    return 0;
}
static void CheckDynamicLights(void)
{
    const struct RhhContentBinding maps[] = {{"test:map/route", RHH_MAP, 7},
        {"test:map/alias", RHH_MAP, 7}, {"test:map/other", RHH_MAP, 8}};
    sDynamicMap = RhhContentKeyOf(maps[0].name); sDynamicAlias = RhhContentKeyOf(maps[1].name);
    static const struct RhhOverworldService world = {.header = {sizeof(world), RHH_SERVICE_VERSION(1, 6)},
        .camera = DynamicCamera, .lighting_inputs = DynamicInputs};
    static const struct RhhModSceneLight declared = {.struct_size = sizeof(declared), .id = "test:light/static",
        .x = 8, .y = 8, .height = 8, .radius = 32, .rgb = 0xffffff, .intensity = 500};
    struct RhhModSceneLight staticLight = declared; staticLight.map = sDynamicMap;
    struct RhhModHook hooks[] = {{sizeof(struct RhhModHook), RHH_WORLD_PRESENTATION, RHH_TRANSFORM, 0, SelectWorldTest, NULL},
        {sizeof(struct RhhModHook), RHH_WORLD_LIGHTS, RHH_TRANSFORM, 0, OfferDynamicTest, NULL}};
    const struct RhhWorldPresenter presenter = {sizeof(presenter), 0, "test:world/presenter", DrawDynamicTest, NULL};
    struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_SCENE_LIGHTS,
        .id = "test:dynamic_light_mod", .hook_count = 2, .hooks = hooks,
        .world_presenter_count = 1, .world_presenters = &presenter, .scene_light_count = 1, .scene_lights = &staticLight};
    RhhModsInit(); CHECK(!RhhModsSetContent(maps, 3)); CHECK(!RhhModsInstallRenderService());
    CHECK(!RhhModsAddService("rhh:service/overworld", &world.header));
    sDynamicRender = (const void *)RhhModsHostAPI()->get_service("rhh:service/world_render", RHH_SERVICE_VERSION(1, 1));
    CHECK(sDynamicRender && sDynamicRender->header.struct_size >= RHH_WORLD_RENDER_SERVICE_V1_1_SIZE);
    CHECK(RhhModsRegister(&mod)); hooks[1].stage = RHH_CONSTRAINT;
    CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize()); RhhModsSetRunContext(11, 22, 0);
    struct RhhWorldLightContext context = {.struct_size = sizeof(context)}, savedContext = context;
    struct RhhWorldDynamicLight light = {.struct_size = sizeof(light)};
    struct RhhWorldLightFrame outside = {.struct_size = sizeof(outside)}, savedOutside = outside;
    CHECK(sDynamicRender->light_context(&context) && !memcmp(&context, &savedContext, sizeof(context))
        && sDynamicRender->offer_light(&light) && sDynamicRender->frame_lights(&outside)
        && !memcmp(&outside, &savedOutside, sizeof(outside)));
    uint16_t pixel = 0; uint8_t mask = 0;
    struct RhhWorldFrame frame = {.struct_size = sizeof(frame), .width = 1, .height = 1, .frame = 71,
        .pixels = &pixel, .mask = &mask, .gl_proc = WorldProcTest};
    sReentrantLightFrame = &frame;
    sDynamicCalls = 0; sDynamicMode = 0; sExpectNoLightFrame = 0;
    CHECK(RhhModsDrawWorld(&frame) && sDynamicCalls == 1 && sCapturedLights.status == RHH_WORLD_LIGHTS_OK
        && sCapturedLights.count == 2 && sCapturedLights.lights[0].id == 10 && sCapturedLights.lights[1].id == 20
        && sCapturedLights.lights[0].map == sDynamicMap && sCapturedLights.lights[0].y == -7);
    CHECK(sDynamicRender->frame_lights(&outside));
    for (sDynamicMode = 1; sDynamicMode <= 9; ++sDynamicMode) {
        CHECK(RhhModsDrawWorld(&frame) && sCapturedLights.count == 0
            && sCapturedLights.status == (sDynamicMode == 3 ? RHH_WORLD_LIGHTS_DENIED : RHH_WORLD_LIGHTS_INVALID));
        CHECK(!sCapturedLights.lights[0].id);
    }
    sDynamicMode = 10; CHECK(RhhModsDrawWorld(&frame) && sCapturedLights.status == RHH_WORLD_LIGHTS_OK
        && sCapturedLights.count == 2);
    RhhModsSetRunContext(0, 0, 0); unsigned calls = sDynamicCalls; sExpectNoLightFrame = 1;
    CHECK(RhhModsDrawWorld(&frame) && sDynamicCalls == calls);
    RhhModsShutdown(); sDynamicRender = NULL; sReentrantLightFrame = NULL; sExpectNoLightFrame = 0;
    puts("dynamic world lights: scoped collection, canonical maps, ordering and atomic rejection passed");
}
static void CheckWorldPresenters(void)
{
    RhhModsInit();
    struct RhhModDefinition *old = malloc(RHH_MOD_DEFINITION_SHADER_SIZE); CHECK(old);
    memcpy(old, &sDefA, RHH_MOD_DEFINITION_SHADER_SIZE);
    old->struct_size = RHH_MOD_DEFINITION_SHADER_SIZE;
    CHECK(!RhhModsRegister(old)); /* A pre-presenter shader ABI remains valid. */
    const struct RhhModHook hook = {sizeof(hook), RHH_WORLD_PRESENTATION, RHH_TRANSFORM, 0, SelectWorldTest, NULL};
    struct RhhWorldPresenter presenters[2] = {{sizeof(*presenters), 0, "test:world/presenter", DrawWorldTest, ShutdownWorldTest}};
    struct RhhModDefinition def = {.struct_size = sizeof(def), .abi_version = RHH_MOD_ABI_V1,
        .id = "test:world_mod", .hook_count = 1, .hooks = &hook,
        .world_presenter_count = 1, .world_presenters = presenters};
    presenters[0].draw = NULL; CHECK(RhhModsRegister(&def)); presenters[0].draw = DrawWorldTest;
    presenters[1] = presenters[0]; def.world_presenter_count = 2; CHECK(RhhModsRegister(&def));
    def.world_presenter_count = 1; CHECK(!RhhModsRegister(&def)); CHECK(!RhhModsFinalize());
    uint16_t pixel = 0; uint8_t mask = 0;
    struct RhhWorldFrame frame = {.struct_size = sizeof(frame), .width = 1, .height = 1,
        .pixels = &pixel, .mask = &mask, .gl_proc = WorldProcTest};
    RhhContentKey selected = 0;
    CHECK(RhhModsDrawWorldWithIdentity(&frame, &selected) && sWorldDraws == 1
        && selected == RhhContentKeyOf("test:world/presenter"));
    struct RhhWorldFrame *legacy = malloc(RHH_WORLD_FRAME_V1_0_SIZE); CHECK(legacy);
    memcpy(legacy, &frame, RHH_WORLD_FRAME_V1_0_SIZE); legacy->struct_size = RHH_WORLD_FRAME_V1_0_SIZE;
    CHECK(RhhModsDrawWorld(legacy) && sWorldDraws == 2); free(legacy);
    sWorldFails = 1; CHECK(!RhhModsDrawWorldWithIdentity(&frame, &selected) && sWorldDraws == 3 && !selected);
    RhhModsShutdownWorldPresenters(); RhhModsShutdownWorldPresenters();
    CHECK(sWorldShutdown == 1 && !RhhModsDrawWorld(&frame));
    RhhModsShutdown(); free(old);
}
static void CheckLargeModSets(void)
{
    struct RhhModDefinition defs[RHH_HOST_MAX_MODS];
    char ids[RHH_HOST_MAX_MODS][24];
    const struct RhhModHook add = {sizeof(add), RHH_WILD_SPECIES, RHH_TRANSFORM, 0, Add, NULL};
    const struct RhhModHook multiply = {sizeof(multiply), RHH_WILD_SPECIES, RHH_TRANSFORM, 0, Multiply, NULL};
    const char *dependency[] = {ids[RHH_HOST_MAX_MODS - 1]};
    uint8_t *saved[2]; uint32_t sizes[2];
    for (unsigned i = 0; i < RHH_HOST_MAX_MODS; ++i)
    {
        snprintf(ids[i], sizeof(ids[i]), "test:module%03u", i);
        defs[i] = (struct RhhModDefinition){.struct_size = sizeof(defs[i]), .abi_version = RHH_MOD_ABI_V1,
            .id = ids[i], .state_schema = 1, .state_size = 1};
    }
    defs[0].dependencies = dependency; defs[0].dependency_count = 1;
    defs[0].hooks = &multiply; defs[0].hook_count = 1;
    defs[RHH_HOST_MAX_MODS - 1].hooks = &add; defs[RHH_HOST_MAX_MODS - 1].hook_count = 1;
    for (unsigned reverse = 0; reverse < 2; ++reverse)
    {
        RhhModsInit();
        for (unsigned i = 0; i < RHH_HOST_MAX_MODS; ++i)
            CHECK(!RhhModsRegister(&defs[reverse ? RHH_HOST_MAX_MODS - 1 - i : i]));
        struct RhhModDefinition extra = {.struct_size = sizeof(extra), .abi_version = RHH_MOD_ABI_V1,
            .id = "test:overflow"};
        CHECK(RhhModsRegister(&extra) && !RhhModsFinalize());
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WILD_SPECIES};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1, .value = 2};
        RhhModsDispatch(&event, &decision); CHECK(decision.value == 27);
        for (unsigned i = 0; i < RHH_HOST_MAX_MODS; ++i) *(uint8_t *)RhhModsState(ids[i]) = i;
        CHECK(!RhhModsExport(&saved[reverse], &sizes[reverse]));
    }
    CHECK(sizes[0] == sizes[1] && !memcmp(saved[0], saved[1], sizes[0]));
    RhhModsResetState(); CHECK(!RhhModsImport(saved[0], sizes[0]));
    for (unsigned i = 0; i < RHH_HOST_MAX_MODS; ++i) CHECK(*(uint8_t *)RhhModsState(ids[i]) == i);
    /* Duplicate two records above the old 32-bit boundary, preserving the
     * envelope checksum so rejection exercises duplicate detection. */
    memcpy(saved[0] + 24 + 110 * 81, saved[0] + 24 + 100 * 81, 81);
    uint32_t checksum = 2166136261u;
    for (uint32_t i = 24; i < sizes[0]; ++i) checksum = (checksum ^ saved[0][i]) * 16777619u;
    for (unsigned i = 0; i < 4; ++i) saved[0][16 + i] = checksum >> (i * 8);
    CHECK(RhhModsImport(saved[0], sizes[0]) && strstr(RhhModsError(), "duplicate saved mod"));
    for (unsigned i = 0; i < RHH_HOST_MAX_MODS; ++i) CHECK(*(uint8_t *)RhhModsState(ids[i]) == i);
    RhhModsShutdown(); free(saved[0]); free(saved[1]);
}
int main(void)
{
    uint8_t *saved[2]; uint32_t size[2];
    for (int reverse = 0; reverse < 2; ++reverse)
    {
        Register(reverse);
        struct RhhModEvent e = {.struct_size = sizeof(e), .kind = RHH_WILD_SPECIES};
        struct RhhModDecision d = {.struct_size = sizeof(d), .allowed = 1, .value = 2};
        RhhModsDispatch(&e, &d); CHECK(d.value == 27);
        e.kind = RHH_CAN_CAPTURE;
        RhhModsDispatch(&e, &d); CHECK(!d.allowed && !strcmp(d.reason, "first encounter spent"));
        e.kind = RHH_MON_FAINTED;
        RhhModsDispatch(&e, &d);
        CHECK(*(uint8_t *)RhhModsState("test:a") == 1);
        CHECK(RhhModsExport(&saved[reverse], &size[reverse]) == 0);
    }
    CHECK(size[0] == size[1] && !memcmp(saved[0], saved[1], size[0]));
    RhhModsResetState();
    CHECK(RhhModsImport(saved[0], size[0]) == 0);
    CHECK(*(uint8_t *)RhhModsState("test:a") == 1);
    saved[0][size[0] - 1] ^= 1;
    CHECK(RhhModsImport(saved[0], size[0]) == -1);
    CHECK(*(uint8_t *)RhhModsState("test:a") == 1); /* transactional rejection */
    saved[0][size[0] - 1] ^= 1;
    for (unsigned n = 0; n < size[0]; ++n) CHECK(RhhModsImport(saved[0], n) == -1);
    RhhModsInit();
    struct RhhModDefinition next = sDefA;
    next.state_schema = 2; next.state_size = 2; next.migrate = Migrate;
    CHECK(RhhModsRegister(&next) == 0);
    CHECK(RhhModsRegister(&sDefB) == 0);
    CHECK(RhhModsFinalize() == 0);
    CHECK(RhhModsImport(saved[0], size[0]) == 0);
    CHECK(((uint8_t *)RhhModsState("test:a"))[1] == 42);
    RhhModsInit();
    CHECK(RhhModsImport(saved[0], size[0]) == -1); /* missing required module */
    struct RhhModDefinition bad = sDefA; bad.abi_version = RHH_MOD_ABI_HABITATS + 1;
    CHECK(RhhModsRegister(&bad) == -1);
    const char *dependsA[] = {"test:b"}, *dependsB[] = {"test:a"};
    bad = sDefA; bad.dependencies = dependsA; bad.dependency_count = 1;
    CHECK(RhhModsRegister(&bad) == 0);
    struct RhhModDefinition cycle = sDefB;
    cycle.dependencies = dependsB; cycle.dependency_count = 1;
    CHECK(RhhModsRegister(&cycle) == 0); CHECK(RhhModsFinalize() == -1);
    RhhModsInit();
    const struct RhhContentBinding first[] = {{"rhh:species/torchic", RHH_SPECIES, 255}};
    const struct RhhContentBinding reordered[] = {{"rhh:species/torchic", RHH_SPECIES, 7}};
    uint64_t key = RhhContentKeyOf("rhh:species/torchic"); uint32_t id;
    CHECK(RhhModsSetContent(first, 1) == 0 && RhhContentResolve(RHH_SPECIES, key, &id) == 0 && id == 255);
    CHECK(RhhModsSetContent(reordered, 1) == 0 && RhhContentResolve(RHH_SPECIES, key, &id) == 0 && id == 7);
    CHECK(RhhModRandomAt(123, "test:a", key, 5) == UINT64_C(0x519a30aa9f8344d5));
    CHECK(RhhModRandomAt(123, "test:a", key, 5) != RhhModRandomAt(123, "test:b", key, 5));
    free(saved[0]); free(saved[1]); RhhModsShutdown();
    CheckSettingsAndDurability();
    CheckShaderDeclarations();
    CheckServices();
    CheckServiceRequirements();
    CheckMusicDeclarations();
    CheckSceneRules();
    CheckSceneLights();
    CheckEcologyTags();
    CheckHabitatRegistry();
    CheckCustomTypes();
    CheckCustomMoves();
    CheckLearnsetDeclarations();
    CheckBattleConditionDeclarations();
    CheckRegisteredMoveEffects();
    CheckCustomAbilities();
    CheckAiPreviewStages();
    CheckHatchHookValidation();
    CheckWorldPresenters();
    CheckDynamicLights();
    CheckLargeModSets();
    puts("mod ABI, ordering, constraints, content identities and migrations passed");
    return 0;
}
