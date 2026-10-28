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
    CHECK(RhhModsRegister(reverse ? &sDefB : &sDefA) == 0);
    CHECK(RhhModsRegister(reverse ? &sDefA : &sDefB) == 0);
    CHECK(RhhModsFinalize() == 0);
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
    struct RhhModDefinition bad = sDefA; bad.abi_version++;
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
    puts("mod ABI, ordering, constraints, content identities and migrations passed");
    return 0;
}
