#include "mod_ui.h"
static unsigned sAccessibleEvents, sOverlayAccessibleEvents;
static void ObserveAccessibleTest(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    CHECK((event->kind == RHH_UI_ACCESSIBILITY_CHANGED || event->kind == RHH_UI_OVERLAY_ACCESSIBILITY_CHANGED)
        && !RhhModsCommandsAllowed());
    if (event->kind == RHH_UI_ACCESSIBILITY_CHANGED) ++sAccessibleEvents; else ++sOverlayAccessibleEvents;
    (void)Random(); (void)Random2();
}
static const struct RhhModHook sAccessibleHooks[] = {
    {sizeof(struct RhhModHook), RHH_UI_ACCESSIBILITY_CHANGED, RHH_OBSERVE, 0, NULL, ObserveAccessibleTest},
    {sizeof(struct RhhModHook), RHH_UI_OVERLAY_ACCESSIBILITY_CHANGED, RHH_OBSERVE, 0, NULL, ObserveAccessibleTest}
};
static const struct RhhModDefinition sAccessibleMod = {.struct_size = sizeof(sAccessibleMod), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:a11y_rng", .hooks = sAccessibleHooks, .hook_count = 2};
static void CheckUiAccessibility(const char *producer, const char *adapter)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sAccessibleMod)
        && !RhhModsLoadLibrary(producer) && !RhhModsLoadLibrary(adapter) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0); sAccessibleEvents = sOverlayAccessibleEvents = 0;
    const struct RhhUiAccessibilityService *a = (const void *)RhhModsHostAPI()->get_service("rhh:service/ui_accessibility", RHH_SERVICE_VERSION(1, 1));
    const struct RhhUiService *ui = (const void *)RhhModsHostAPI()->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 3));
    CHECK(a && ui && a->header.struct_size == RHH_UI_ACCESSIBILITY_SERVICE_V1_1_SIZE
        && RHH_UI_ACCESSIBILITY_SERVICE_V1_0_SIZE < RHH_UI_ACCESSIBILITY_SERVICE_V1_1_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/ui_accessibility", RHH_SERVICE_VERSION(1, 0)) == &a->header
        && !RhhModsHostAPI()->get_service("rhh:service/ui_accessibility", RHH_SERVICE_VERSION(1, 2)));
    struct RhhUiAccessibleSnapshot snapshot = {.struct_size = sizeof(snapshot)}, small = {.struct_size = 1, .revision = 55}, saved = small;
    CHECK(a->snapshot(&small) && !memcmp(&small, &saved, sizeof(small)) && !a->snapshot(&snapshot) && !snapshot.active);
    struct RhhUiAccessibleContent invalid = {.struct_size = sizeof(invalid), .title = "Outside draw"};
    CHECK(a->publish(RhhContentKeyOf("test:a11y_producer"), &invalid));
    struct RhhModEvent tick = {.struct_size = sizeof(tick), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&tick, &decision); CHECK(ui->active());
    u16 *frame = calloc(DISPLAY_WIDTH * DISPLAY_HEIGHT, sizeof(*frame)); CHECK(frame);
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    RhhUiRender(frame); CHECK(sAccessibleEvents == 1 && !a->snapshot(&snapshot) && snapshot.active
        && snapshot.owner == RhhContentKeyOf("test:a11y_producer")
        && !strcmp(snapshot.content.focus, "HP") && !strcmp(snapshot.content.body, "HP 31\nAttack 12"));
    u64 revision = snapshot.revision;
    RhhUiRender(frame); CHECK(sAccessibleEvents == 1 && !a->snapshot(&snapshot) && snapshot.revision == revision);
    RhhUiAdvance(0); RhhUiAdvance(RHH_UI_A); RhhUiRender(frame);
    CHECK(sAccessibleEvents == 2 && !a->snapshot(&snapshot) && snapshot.revision == revision + 1
        && !strcmp(snapshot.content.focus, "Attack"));
    RhhUiAdvance(0); RhhUiAdvance(RHH_UI_B);
    CHECK(!ui->active() && sAccessibleEvents == 3 && !a->snapshot(&snapshot) && !snapshot.active
        && !snapshot.owner && snapshot.revision == revision + 2 && !snapshot.content.title[0]);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    RhhModsDispatch(&tick, &decision); RhhUiRender(frame); CHECK(sAccessibleEvents == 4);
    RhhModsSetRunContext(123, 457, 0); CHECK(!ui->active() && !a->snapshot(&snapshot) && !snapshot.active);
    NativeSaveResetArchive();
    RhhUiAccessibilityFlush(); CHECK(sAccessibleEvents == 4);
    gMain.inBattle = FALSE; gMain.callback2 = CB2_Overworld; memset(&gPaletteFade, 0, sizeof(gPaletteFade));
    ScriptContext_Init(); UnlockPlayerFieldControls(); RhhUiFilterKeys(0);
    tick.args[0] = 1; RhhModsDispatch(&tick, &decision); CHECK(ui->overlay_active() && !ui->active());
    CHECK(RhhUiOverlayRender(frame) && sOverlayAccessibleEvents == 1 && sAccessibleEvents == 4
        && !a->overlay_snapshot(&snapshot) && snapshot.active && !strcmp(snapshot.content.focus, "Attack"));
    revision = snapshot.revision;
    CHECK(!a->snapshot(&snapshot) && !snapshot.active); /* v1.0 remains modal-only. */
    CHECK(RhhUiOverlayRender(frame) && sOverlayAccessibleEvents == 1);
    RhhUiOverlayAdvance(0); RhhUiOverlayAdvance(RHH_UI_A); CHECK(RhhUiOverlayRender(frame));
    CHECK(sOverlayAccessibleEvents == 2 && !a->overlay_snapshot(&snapshot) && snapshot.revision == revision + 1
        && !strcmp(snapshot.content.focus, "HP"));
    RhhUiOverlayAdvance(0); RhhUiOverlayAdvance(RHH_UI_B);
    CHECK(!ui->overlay_active() && sOverlayAccessibleEvents == 3 && !a->overlay_snapshot(&snapshot)
        && !snapshot.active && snapshot.revision == revision + 2 && sAccessibleEvents == 4);
    RhhUiFilterKeys(0); RhhModsDispatch(&tick, &decision); CHECK(ui->overlay_active() && RhhUiOverlayRender(frame));
    gMain.inBattle = TRUE; RhhUiOverlayReconcile(); RhhUiAccessibilityFlush();
    CHECK(!ui->overlay_active() && sOverlayAccessibleEvents == 5 && !a->overlay_snapshot(&snapshot) && !snapshot.active);
    gMain.inBattle = FALSE; RhhUiFilterKeys(0); RhhModsDispatch(&tick, &decision);
    CHECK(ui->overlay_active() && RhhUiOverlayRender(frame) && sOverlayAccessibleEvents == 6);
    RhhModsSetRunContext(123, 458, 0); RhhUiAccessibilityFlush();
    CHECK(!ui->overlay_active() && !a->overlay_snapshot(&snapshot) && !snapshot.active && sOverlayAccessibleEvents == 6);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    NativeSaveResetArchive();
    tick.args[0] = 0; tick.args[1] = 1; RhhModsDispatch(&tick, &decision); CHECK(ui->active());
    RhhUiAdvance(0); RhhUiAdvance(RHH_UI_A); RhhUiAdvance(0); RhhUiAdvance(RHH_UI_B); CHECK(!ui->active());
    tick.args[1] = 0;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    CHECK(RhhUiPreparePartyMenu(0) == 2 && !RhhUiOpenPartyMenu());
    RhhUiAdvance(0); RhhUiAdvance(RHH_UI_A); CHECK(ui->active());
    RhhUiAdvance(0); RhhUiAdvance(RHH_UI_A); RhhUiAdvance(0); RhhUiAdvance(RHH_UI_B); CHECK(!ui->active());
    CHECK(RhhUiPreparePartyMenu(0) == 2 && !RhhUiOpenPartyMenu());
    RhhUiAdvance(0); RhhUiAdvance(RHH_UI_DOWN); RhhUiAdvance(0); RhhUiAdvance(RHH_UI_A); CHECK(ui->active());
    RhhUiAdvance(0); RhhUiAdvance(RHH_UI_A); RhhUiAdvance(0); RhhUiAdvance(RHH_UI_B); CHECK(!ui->active());
    RhhModsUnloadLibraries(); free(frame);
    puts("ui accessibility: copied text, modal and overlay updates and lifecycle passed");
}
