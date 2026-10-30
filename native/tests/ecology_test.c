#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mod_host.h"
#include "native.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "ecology test line %d: %s (%s)\n", __LINE__, #x, RhhModsError()); exit(1); } } while (0)
static const struct RhhContentBinding sContent[] = {
    {"test:map/route", RHH_MAP, 1}, {"test:map/forest", RHH_MAP, 2},
    {"test:species/bird", RHH_SPECIES, 1}, {"test:species/fish", RHH_SPECIES, 2}
};
static const struct RhhModDefinition sRun = {
    .struct_size = sizeof(sRun), .abi_version = RHH_MOD_ABI_V1,
    .id = "rhh:host_run", .state_size = 16, .state_schema = 1, .flags = RHH_MOD_REQUIRED
};
static const struct RhhEcologyService *sEco;
static uint64_t sRoute, sForest, sBird, sFish, sFood;
static int sObserverResult, sTransformResult;
static void ObserveStep(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{ (void)event; (void)decision; (void)state; sObserverResult = sEco->set(sRoute, RHH_ECOLOGY_CONDITION, sFood, -8); }
static void TransformScore(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; (void)decision; sTransformResult = sEco->set(sRoute, RHH_ECOLOGY_CONDITION, sFood, 100); }
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_WORLD_STEP, RHH_OBSERVE, 0, NULL, ObserveStep},
    {sizeof(struct RhhModHook), RHH_PHOTO_SCORE, RHH_TRANSFORM, 0, TransformScore, NULL}
};
static const struct RhhModDefinition sObserver = {
    .struct_size = sizeof(sObserver), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:ecology_observer", .hook_count = 2, .hooks = sHooks
};
static void SetRun(uint64_t id)
{
    uint8_t *state = RhhModsState("rhh:host_run"); CHECK(state);
    for (unsigned i = 0; i < 8; ++i) state[8 + i] = id >> (8 * i);
    RhhModsSetRunContext(123, id, 1);
}
static void Setup(const char *path, int reject)
{
    RhhModsInit(); CHECK(!RhhModsSetContent(sContent, sizeof(sContent) / sizeof(*sContent)));
    CHECK(!RhhModsInstallEcologyService() && !RhhModsRegister(&sRun)
        && !RhhModsRegister(&sObserver) && !RhhModsFinalize());
    sEco = (const void *)RhhModsHostAPI()->get_service("rhh:service/ecology", RHH_SERVICE_VERSION(1, 0));
    CHECK(sEco && sEco->header.struct_size >= RHH_ECOLOGY_SERVICE_V1_0_SIZE);
    sRoute = RhhContentKeyOf("test:map/route"); sForest = RhhContentKeyOf("test:map/forest");
    sBird = RhhContentKeyOf("test:species/bird"); sFish = RhhContentKeyOf("test:species/fish");
    sFood = RhhContentKeyOf("test:ecology/food");
    int result = NativeSaveOpen(path);
    if (reject) { CHECK(result); return; }
    CHECK(!result); SetRun(42);
}
static void Save(uint32_t generation) { CHECK(!NativeSaveBegin() && !NativeSaveEnd(generation, 1)); }
static void CheckValue(uint64_t map, uint32_t kind, uint64_t key, int64_t expected)
{ int64_t value = -999; CHECK(!sEco->get(map, kind, key, &value) && value == expected); }
int main(int argc, char **argv)
{
    CHECK(argc == 3); int reject = !strcmp(argv[1], "reject"); Setup(argv[2], reject);
    if (reject) { NativeSaveClose(); RhhModsShutdown(); return 0; }
    if (!strcmp(argv[1], "write"))
    {
        int64_t missing = 55;
        CHECK(sEco->get(sRoute, RHH_ECOLOGY_POPULATION, sBird, &missing) == 1 && missing == 55);
        CHECK(!sEco->set(sRoute, RHH_ECOLOGY_POPULATION, sBird, 20));
        CHECK(!sEco->set(sRoute, RHH_ECOLOGY_POPULATION, sFish, 0));
        CHECK(!sEco->set(sRoute, RHH_ECOLOGY_CONDITION, sFood, -7));
        CHECK(!sEco->set(sForest, RHH_ECOLOGY_POPULATION, sBird, 12));
        CHECK(sEco->set(sRoute, RHH_ECOLOGY_POPULATION, sBird, -1));
        CHECK(sEco->set(sRoute, RHH_ECOLOGY_POPULATION, sFood, 1));
        CHECK(sEco->set(0, RHH_ECOLOGY_CONDITION, sFood, 1));
        struct RhhEcologyValue item = {.struct_size = sizeof(item)};
        CHECK(!sEco->at(sRoute, 0, &item) && item.kind == RHH_ECOLOGY_CONDITION && item.value == -7);
        CHECK(!sEco->at(sRoute, 1, &item) && item.kind == RHH_ECOLOGY_POPULATION);
        CHECK(!sEco->at(sRoute, 2, &item) && item.kind == RHH_ECOLOGY_POPULATION);
        CHECK(sEco->at(sRoute, 3, &item) == 1);
        CHECK(!sEco->set(sRoute, RHH_ECOLOGY_OBSERVATIONS, sBird, 5));
        CHECK(sEco->set(sRoute, RHH_ECOLOGY_OBSERVATIONS, sBird, -1));
        CHECK(!sEco->at(sRoute, 3, &item) && item.kind == RHH_ECOLOGY_OBSERVATIONS && item.value == 5);
        for (unsigned i = 0; i < 300; ++i)
        {
            char name[64]; snprintf(name, sizeof(name), "test:ecology/condition_%u", i);
            CHECK(!sEco->set(sForest, RHH_ECOLOGY_CONDITION, RhhContentKeyOf(name), i));
        }
        RhhModsSetDrawing(1); CHECK(sEco->set(sRoute, RHH_ECOLOGY_CONDITION, sFood, 5)); RhhModsSetDrawing(0);
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WORLD_STEP};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        sObserverResult = -1; RhhModsDispatch(&event, &decision); CHECK(!sObserverResult);
        event.kind = RHH_PHOTO_SCORE; sTransformResult = 0; RhhModsDispatch(&event, &decision);
        CHECK(sTransformResult && !sEco->set(sRoute, RHH_ECOLOGY_CONDITION, sFood, -7));
        CHECK(RhhEcologyFrequencyAt(sRoute, sBird, 100000) == 10000);
        CHECK(!RhhEcologyDepleteAt(sRoute, sBird, 100000));
        CHECK(RhhEcologyFrequencyAt(sRoute, sBird, 100000) == 9000);
        CHECK(RhhEcologyFrequencyAt(sRoute, sBird, 101800) == 9500);
        CHECK(RhhEcologyFrequencyAt(sRoute, sBird, 103600) == 10000);
        CHECK(RhhEcologyFrequencyAt(sRoute, sBird, 90000) == 9000);
        CHECK(RhhEcologyFrequencyAt(sRoute, sBird, 0) == 9000);
        RhhEcologyResumeClock(101800);
        CHECK(RhhEcologyFrequencyAt(sRoute, sBird, 101000) == 9500);
        CHECK(RhhEcologyFrequencyAt(sRoute, sBird, 101800) == 9500);
        CHECK(RhhEcologyFrequencyAt(sForest, sBird, 100000) == 10000);
        CHECK(RhhEcologyFrequencyAt(sRoute, sFish, 100000) == 10000);
        uint64_t recovery = RhhContentKeyOf("rhh:ecology/recovery_per_hour");
        uint64_t reduction = RhhContentKeyOf("rhh:ecology/reduction");
        CHECK(!sEco->set(sForest, RHH_ECOLOGY_CONDITION, recovery, 0));
        CHECK(!sEco->set(sForest, RHH_ECOLOGY_CONDITION, reduction, 500));
        CHECK(!RhhEcologyDepleteAt(sForest, sBird, 100000));
        CHECK(RhhEcologyFrequencyAt(sForest, sBird, 200000) == 9500);
        CHECK(!sEco->set(sForest, RHH_ECOLOGY_CONDITION, recovery, 2000));
        uint64_t currentTime = RhhEcologySystemTime(); CHECK(currentTime);
        CHECK(RhhEcologyFrequencyAt(sForest, sBird, currentTime) == 9500);
        CHECK(RhhEcologyFrequencyAt(sForest, sBird, currentTime + 900) == 10000);
        CHECK(!sEco->remove(sForest, RHH_ECOLOGY_CONDITION, recovery));
        CHECK(!sEco->remove(sForest, RHH_ECOLOGY_CONDITION, reduction));
        Save(1); CHECK(!sEco->set(sRoute, RHH_ECOLOGY_POPULATION, sBird, 19)); Save(2);
        CHECK(!NativeSaveRestoreMods(1)); CheckValue(sRoute, RHH_ECOLOGY_POPULATION, sBird, 20);
        CHECK(!NativeSaveRestoreMods(2)); CheckValue(sRoute, RHH_ECOLOGY_POPULATION, sBird, 19);
        CHECK(!NativeSaveBegin()); CHECK(sEco->set(sRoute, RHH_ECOLOGY_POPULATION, sBird, 18));
        CHECK(NativeSaveEnd(3, 0)); CheckValue(sRoute, RHH_ECOLOGY_POPULATION, sBird, 19);
    }
    else if (!strcmp(argv[1], "read"))
    {
        CheckValue(sRoute, RHH_ECOLOGY_POPULATION, sBird, 19);
        CHECK(RhhEcologyFrequencyAt(sRoute, sBird, 101800) == 9500);
        CHECK(!RhhEcologyDepleteAt(sRoute, sBird, 90000));
        CHECK(RhhEcologyFrequencyAt(sRoute, sBird, 100000) == 8500);
        CHECK(RhhEcologyFrequencyAt(sRoute, sBird, 103600) == 9000);
        CHECK(!RhhEcologyDepleteAt(sForest, sFish, 0));
        CHECK(RhhEcologyFrequencyAt(sForest, sFish, 200000) == 9000);
        RhhEcologyResumeClock(200000);
        CHECK(RhhEcologyFrequencyAt(sForest, sFish, 203600) == 10000);
        CheckValue(sRoute, RHH_ECOLOGY_POPULATION, sFish, 0);
        CheckValue(sRoute, RHH_ECOLOGY_CONDITION, sFood, -7);
        CheckValue(sRoute, RHH_ECOLOGY_OBSERVATIONS, sBird, 5);
        CheckValue(sForest, RHH_ECOLOGY_POPULATION, sBird, 12);
        for (unsigned i = 0; i < 300; ++i)
        {
            char name[64]; snprintf(name, sizeof(name), "test:ecology/condition_%u", i);
            CheckValue(sForest, RHH_ECOLOGY_CONDITION, RhhContentKeyOf(name), i);
        }
        CHECK(!sEco->remove(sRoute, RHH_ECOLOGY_POPULATION, sFish));
        CHECK(sEco->remove(sRoute, RHH_ECOLOGY_POPULATION, sFish) == 1);
        Save(3);
    }
    else if (!strcmp(argv[1], "removed"))
    { int64_t value; CHECK(sEco->get(sRoute, RHH_ECOLOGY_POPULATION, sFish, &value) == 1); }
    else CHECK(0);
    NativeSaveClose(); RhhModsShutdown(); puts("location ecology: saved values and rollback passed"); return 0;
}
