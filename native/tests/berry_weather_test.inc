#include "berry.h"
#include "event_scripts.h"
#include "item_menu.h"
#include "item_use.h"
#include "clock.h"
#include "rtc.h"
#include "constants/berry.h"
#include "constants/flags.h"
static const struct RhhBerryPlotService *sBerryTest;
static u32 sBerryMode, sBerryConstraints, sBerryCommitted, sBerryManualCount, sBerryManualChanged;
static u32 sBerryInteractionCount, sBerryInteractionCause;
static u32 sBerryHarvestCount;
static u32 sBerryYieldMode;
static u32 sBerryClockCount;
static bool32 sBerryClockTesting;
static rng_value_t sBerryClockRng, sBerryClockRng2;
static u32 sAuthoredWaterMode, sAuthoredWaterConstraints, sAuthoredWaterCommitted;
static struct RhhBerryHarvest sBerryLastHarvest;
static struct RhhBerryPlotInteraction sBerryLastInteraction;
static rng_value_t sBerryInteractionRng, sBerryInteractionRng2;
static void AuthoredWaterConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhAuthoredBerryWaterResult context = {.struct_size = sizeof(context)};
    CHECK(!sBerryTest->authored_water_context(&context) && context.changed && event->event_key == context.before.plot.map
        && event->args[0] == context.before.plot.tree_id && event->subject == context.before.plot.berry_item);
    struct RhhAuthoredBerryWaterRequest nested = {.struct_size = sizeof(nested), .owner = context.owner, .expected = context.before};
    struct RhhAuthoredBerryWaterResult result = {.struct_size = sizeof(result)};
    CHECK(sBerryTest->water_authored(&nested, &result) == RHH_BERRY_WATER_UNAVAILABLE);
    ++sAuthoredWaterConstraints;
    if (sAuthoredWaterMode == 1) { decision->allowed = 0; snprintf(decision->reason, sizeof(decision->reason), "test denial"); }
    if (sAuthoredWaterMode == 2) { (void)Random32(); (void)Random2_32(); }
    if (sAuthoredWaterMode == 3) GetBerryTreeInfo(context.before.plot.tree_id)->weeds ^= 1;
}
static void AuthoredWaterCommitted(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhAuthoredBerryWaterResult context = {.struct_size = sizeof(context)};
    CHECK(!sBerryTest->authored_water_context(&context) && !context.is_preview && context.changed
        && event->event_key == context.after.plot.map && !RhhModsCommandsAllowed());
    ++sAuthoredWaterCommitted;
}
static void BerryTestConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhBerryWaterResult context = {.struct_size = sizeof(context)};
    CHECK(sBerryTest && !sBerryTest->context(&context) && context.changed && context.before.tree_id && context.owner);
    struct RhhBerryWaterRequest nested = {.struct_size = sizeof(nested), .owner = context.owner, .expected = context.before};
    struct RhhBerryWaterResult result = {.struct_size = sizeof(result)};
    CHECK(sBerryTest->water(&nested, &result) == RHH_BERRY_WATER_UNAVAILABLE); ++sBerryConstraints;
    if (sBerryMode == 1) { decision->allowed = 0; snprintf(decision->reason, sizeof(decision->reason), "test berry denial"); }
    if (sBerryMode == 2) { (void)Random32(); (void)Random2_32(); }
    if (sBerryMode == 3) ModsWorldStep(0, 0, 0, 0); /* Expire a timed effect during a proposal. */
    if (sBerryMode == 4) gWeatherPtr->currWeather = WEATHER_SUNNY;
    if (sBerryMode == 5) gMapHeader.mapType = MAP_TYPE_INDOOR;
}
static void BerryTestCommitted(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhBerryWaterResult context = {.struct_size = sizeof(context)};
    CHECK(sBerryTest && !sBerryTest->context(&context) && !context.is_preview && context.changed
        && context.before.tree_id == context.after.tree_id); ++sBerryCommitted;
}
static void BerryTestManual(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBerryManualWater context = {.struct_size = sizeof(context)}, tiny = {.struct_size = 1, .changed = 99}, saved = tiny;
    CHECK(sBerryTest && !sBerryTest->manual_water_context(&context)
        && sBerryTest->manual_water_context(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny))
        && context.run_id && context.changed == sBerryManualChanged && context.object_slot == 0
        && context.before.tree_id == context.after.tree_id && event->event_key == context.before.map
        && event->subject == context.before.berry_item && event->args[0] == context.before.tree_id
        && event->args[1] == context.before.stage && event->args[2] == context.changed
        && event->args[3] == context.object_slot);
    struct RhhBerryWaterRequest nested = {.struct_size = sizeof(nested),
        .owner = RhhContentKeyOf("test:berry_weather"), .expected = context.after};
    struct RhhBerryWaterResult result = {.struct_size = sizeof(result)};
    CHECK(sBerryTest->water(&nested, &result) == RHH_BERRY_WATER_UNAVAILABLE);
    (void)Random32(); (void)Random2_32(); ++sBerryManualCount;
}
static void BerryTestInteraction(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBerryPlotInteraction change = {.struct_size = sizeof(change)},
        tiny = {.struct_size = 1, .changed = 99}, saved = tiny;
    CHECK(!sBerryTest->interaction_context(&change)
        && sBerryTest->interaction_context(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny))
        && change.run_id && change.cause == sBerryInteractionCause && change.object_slot == 0
        && change.before.plot.tree_id == change.after.plot.tree_id && event->event_key == change.before.plot.map
        && event->args[0] == change.cause && event->args[1] == change.before.plot.tree_id
        && event->args[2] == change.changed && event->args[3] == change.object_slot && !RhhModsCommandsAllowed());
    sBerryLastInteraction = change; sBerryInteractionRng = gRngValue; sBerryInteractionRng2 = gRng2Value;
    (void)Random32(); (void)Random2_32(); ++sBerryInteractionCount;
}
static void BerryTestHarvest(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhBerryHarvest harvest = {.struct_size = sizeof(harvest)}, tiny = {.struct_size = 1}, saved = tiny;
    CHECK(!sBerryTest->harvest_context(&harvest) && sBerryTest->harvest_context(&tiny)
        && !memcmp(&tiny, &saved, sizeof(tiny)) && harvest.run_id && harvest.object_slot == 0
        && event->event_key == harvest.plot.plot.map && event->subject == harvest.berry_item
        && event->args[0] == harvest.plot.plot.tree_id && event->args[1] == harvest.berry_count
        && event->args[2] == harvest.mutation_count && event->args[3] == harvest.object_slot
        && !RhhModsCommandsAllowed());
    sBerryLastHarvest = harvest; (void)Random32(); (void)Random2_32(); ++sBerryHarvestCount;
}
static void BerryTestYield(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhBerryYield context = {.struct_size = sizeof(context)}, nested = {.struct_size = sizeof(nested), .value = 999}, saved = nested;
    CHECK(!sBerryTest->yield_context(&context) && context.base == event->args[1]
        && context.plot.plot.map == event->event_key && context.plot.plot.berry_item == event->subject
        && sBerryTest->yield_preview(context.plot.plot.index, &nested) && !memcmp(&nested, &saved, sizeof(nested)));
    if (sBerryYieldMode == 1) decision->value = context.base + 2;
    if (sBerryYieldMode == 2) decision->value = 0;
    if (sBerryYieldMode == 3) { decision->value = context.base + 2; (void)Random32(); (void)Random2_32(); }
    if (sBerryYieldMode == 4) decision->allowed = 0;
}
static void BerryTestYieldConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sBerryYieldMode == 5) decision->value = 200;
}
static void BerryTestClock(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!sBerryClockTesting) return;
    struct RhhBerryClockUpdate context = {.struct_size = sizeof(context)}, tiny = {.struct_size = 1, .elapsed_minutes = 99}, saved = tiny;
    struct RhhBerryClockChange first = {.struct_size = sizeof(first)}, second = {.struct_size = sizeof(second)},
        bad = {.struct_size = 1, .tree_id = 999}, savedBad = bad;
    CHECK(!sBerryTest->clock_context(&context) && context.run_id == RhhModsRunId()
        && context.elapsed_minutes >= 2 && context.elapsed_minutes < 20 && context.change_count == 2
        && event->args[0] == context.elapsed_minutes && event->args[1] == 2
        && sBerryTest->clock_context(&tiny) && !memcmp(&tiny, &saved, sizeof(tiny))
        && !sBerryTest->clock_change(0, &first) && !sBerryTest->clock_change(1, &second)
        && sBerryTest->clock_change(0, &bad) && !memcmp(&bad, &savedBad, sizeof(bad))
        && sBerryTest->clock_change(2, &first) && first.tree_id == 5 && second.tree_id == 110
        && first.before.stage == BERRY_STAGE_PLANTED && first.after.stage == BERRY_STAGE_SPROUTED
        && second.before.minutes_to_stage > second.after.minutes_to_stage
        && first.before.berry_item == RhhContentKeyForId(RHH_ITEM, ITEM_ORAN_BERRY)
        && !memcmp(&gSaveBlock2Ptr->lastBerryTreeUpdate, &gLocalTime, sizeof(gLocalTime))
        && !RhhModsCommandsAllowed());
    sBerryClockRng = gRngValue; sBerryClockRng2 = gRng2Value; (void)Random32(); (void)Random2_32(); ++sBerryClockCount;
}
static const struct RhhModHook sBerryTestHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_WATER_AUTHORED_BERRY, RHH_CONSTRAINT, 0, AuthoredWaterConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_AUTHORED_BERRY_WATERED, RHH_OBSERVE, 0, NULL, AuthoredWaterCommitted},
    {sizeof(struct RhhModHook), RHH_CAN_WATER_BERRY_PLOT, RHH_CONSTRAINT, 0, BerryTestConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_BERRY_PLOT_WATERED, RHH_OBSERVE, 0, NULL, BerryTestCommitted},
    {sizeof(struct RhhModHook), RHH_BERRY_PLOT_MANUALLY_WATERED, RHH_OBSERVE, 0, NULL, BerryTestManual},
    {sizeof(struct RhhModHook), RHH_BERRY_PLOT_INTERACTED, RHH_OBSERVE, 0, NULL, BerryTestInteraction},
    {sizeof(struct RhhModHook), RHH_BERRY_PLOT_HARVESTED, RHH_OBSERVE, 0, NULL, BerryTestHarvest},
    {sizeof(struct RhhModHook), RHH_BERRY_CLOCK_UPDATED, RHH_OBSERVE, 0, NULL, BerryTestClock},
    {sizeof(struct RhhModHook), RHH_BERRY_HARVEST_YIELD, RHH_TRANSFORM, 0, BerryTestYield, NULL},
    {sizeof(struct RhhModHook), RHH_BERRY_HARVEST_YIELD, RHH_CONSTRAINT, 0, BerryTestYieldConstraint, NULL}
};
static const struct RhhModDefinition sBerryTestMod = {.struct_size = sizeof(sBerryTestMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:berry_weather", .hooks = sBerryTestHooks, .hook_count = ARRAY_COUNT(sBerryTestHooks)};
static void BerryTestUpdate(void)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1}; RhhModsDispatch(&event, &decision);
}
