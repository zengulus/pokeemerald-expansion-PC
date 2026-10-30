#include "mod_ui.h"
#include "native_controls.h"
#include "fonts.h"
#include "text.h"
static unsigned sUiUnexpectedTicks;
static void UiUnexpectedTick(void) { ++sUiUnexpectedTicks; }
static void CheckModUi(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsLoadLibrary(path)); CHECK(!RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhContentDisplayService *display = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/content_display", RHH_SERVICE_VERSION(1, 0));
    CHECK(display && display->header.struct_size >= RHH_CONTENT_DISPLAY_SERVICE_V1_0_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/content_display", RHH_SERVICE_VERSION(1, 1)) == &display->header
        && RhhModsHostAPI()->get_service("rhh:service/content_display", RHH_SERVICE_VERSION(1, 2)) == &display->header
        && RhhModsHostAPI()->get_service("rhh:service/content_display", RHH_SERVICE_VERSION(1, 3)) == &display->header
        && !RhhModsHostAPI()->get_service("rhh:service/content_display", RHH_SERVICE_VERSION(1, 4)));
    char label[128], unchangedLabel[sizeof(label)]; memset(label, 0x5a, sizeof(label));
    memcpy(unchangedLabel, label, sizeof(label));
    RhhContentKey torchic = RhhContentKeyForId(RHH_SPECIES, SPECIES_TORCHIC);
    rng_value_t displayRng = gRngValue, displayRng2 = gRng2Value;
    CHECK(display->name(RHH_SPECIES, torchic, label, 7) && !memcmp(label, unchangedLabel, sizeof(label))
        && display->name(RHH_SPECIES, 0, label, sizeof(label)) && !memcmp(label, unchangedLabel, sizeof(label))
        && display->name(RHH_MAP, torchic, label, sizeof(label)) && !memcmp(label, unchangedLabel, sizeof(label))
        && display->name(RHH_ITEM, torchic, label, sizeof(label)) && !memcmp(label, unchangedLabel, sizeof(label))
        && !display->name(RHH_SPECIES, torchic, label, 8) && !strcmp(label, "Torchic")
        && !display->name(RHH_SPECIES, RhhContentKeyForId(RHH_SPECIES, SPECIES_NIDORAN_F), label, sizeof(label))
        && !strcmp(label, "Nidoran(F)")
        && !display->name(RHH_SPECIES, RhhContentKeyForId(RHH_SPECIES, SPECIES_NIDORAN_M), label, sizeof(label))
        && !strcmp(label, "Nidoran(M)")
        && !display->name(RHH_ITEM, RhhContentKeyForId(RHH_ITEM, ITEM_POKE_BALL), label, sizeof(label))
        && !strcmp(label, "Poke Ball")
        && !display->name(RHH_MOVE, RhhContentKeyForId(RHH_MOVE, MOVE_TACKLE), label, sizeof(label)) && !strcmp(label, "Tackle")
        && !display->name(RHH_ABILITY, RhhContentKeyForId(RHH_ABILITY, ABILITY_BLAZE), label, sizeof(label)) && !strcmp(label, "Blaze")
        && !display->name(RHH_TYPE, RhhContentKeyForId(RHH_TYPE, TYPE_FIRE), label, sizeof(label)) && !strcmp(label, "Fire")
        && !memcmp(&displayRng, &gRngValue, sizeof(displayRng)) && !memcmp(&displayRng2, &gRng2Value, sizeof(displayRng2)));
    CreateMonWithIVs(&gPlayerParty[0], SPECIES_TORCHIC, 5, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPlayerPartyCount = 1;
    const struct RhhUiService *ui = (const void *)RhhModsHostAPI()->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 0));
    CHECK(ui && !ui->active());
    const char *controlsPath = getenv("RHH_TEST_CONTROLS_PATH");
    if (controlsPath)
    {
        NativeControlsInit(controlsPath);
        const struct RhhUiService *newUi = (const void *)RhhModsHostAPI()->get_service(
            "rhh:service/ui", RHH_SERVICE_VERSION(1, 4));
        CHECK(newUi == ui && ui->header.struct_size >= RHH_UI_SERVICE_V1_4_SIZE
            && RHH_UI_SERVICE_V1_3_SIZE < RHH_UI_SERVICE_V1_4_SIZE
            && RhhModsHostAPI()->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 5)) == &ui->header
            && !RhhModsHostAPI()->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 6)));
        struct RhhUiKeyboardBinding binding = {.struct_size = sizeof(binding)};
        CHECK(!ui->keyboard_binding(RHH_UI_A, &binding) && binding.button == RHH_UI_A
            && binding.count == 1 && !strcmp(binding.keys[0], "Z") && !binding.keys[1][0]);
        CHECK(!ui->keyboard_binding(RHH_UI_SELECT, &binding) && binding.count == 2
            && !strcmp(binding.keys[0], "Left Shift") && !strcmp(binding.keys[1], "Right Shift"));
        struct RhhUiKeyboardBinding saved = binding, tiny = {.struct_size = 1, .button = 99}, oldTiny = tiny;
        CHECK(ui->keyboard_binding(RHH_UI_A | RHH_UI_B, &binding) && !memcmp(&binding, &saved, sizeof(binding)));
        CHECK(ui->keyboard_binding(0, &binding) && !memcmp(&binding, &saved, sizeof(binding)));
        CHECK(ui->keyboard_binding(RHH_UI_A, &tiny) && !memcmp(&tiny, &oldTiny, sizeof(tiny)));
        CHECK(ui->keyboard_binding(RHH_UI_A, NULL));
        NativeControlsBeginCapture(CONTROL_A); CHECK(NativeControlsKeyDown(SDL_SCANCODE_X, 0));
        CHECK(NativeControlsCaptureResult() == CONTROL_CHANGED);
        CHECK(!ui->keyboard_binding(RHH_UI_A, &binding) && binding.count == 1 && !strcmp(binding.keys[0], "X"));
        CHECK(!ui->keyboard_binding(RHH_UI_B, &binding) && !strcmp(binding.keys[0], "Z"));
        CHECK(!strcmp(saved.keys[0], "Left Shift") && !strcmp(saved.keys[1], "Right Shift"));
        CHECK(!NativeControlsReset());
        CHECK(!ui->keyboard_binding(RHH_UI_A, &binding) && !strcmp(binding.keys[0], "Z"));
        NativeControlsShutdown();
    }
    struct RhhUiInput sampled = {.struct_size = sizeof(sampled)};
    CHECK(ui->input(&sampled));
    gMain.heldKeysRaw = RHH_UI_SELECT | RHH_UI_R; gMain.newKeysRaw = RHH_UI_R;
    struct RhhModEvent tick = {.struct_size = sizeof(tick), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&tick, &decision); CHECK(ui->active());
    CHECK(ui->close(RhhContentKeyOf("unknown:owner")));
    u16 *frame = malloc((DISPLAY_WIDTH * DISPLAY_HEIGHT + 2) * sizeof(*frame)); CHECK(frame);
    for (unsigned i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT + 2; ++i) frame[i] = 0x1234;
    RhhUiRender(frame + 1);
    CHECK(frame[0] == 0x1234 && frame[DISPLAY_WIDTH * DISPLAY_HEIGHT + 1] == 0x1234);
    CHECK(frame[1] != 0x1234);
    {
        const unsigned glyphs[] = {CHAR_A, CHAR_a, CHAR_GREATER_THAN, CHAR_SPACE};
        union TextColor colors = {.background = 0, .foreground = 1, .shadow = 2, .accent = 0};
        union TextColor previous = SaveTextColors();
        GenerateFontHalfRowLookupTable(colors);
        for (unsigned i = 0; i < ARRAY_COUNT(glyphs); ++i) {
            const u16 *source = gFontNormalLatinGlyphs + 32 * glyphs[i];
            u32 top[8], bottom[8];
            DecompressGlyphTile(source, top); DecompressGlyphTile(source + 16, bottom);
            unsigned width = gFontNormalLatinGlyphWidths[glyphs[i]]; CHECK(width <= 8);
            for (unsigned y = 0; y < 15; ++y) for (unsigned x = 0; x < width; ++x) {
                unsigned pixel = ((y < 8 ? top[y] : bottom[y - 8]) >> (4 * x)) & 15;
                u16 expected = pixel == 1 ? 0x8004 : pixel == 2 ? 0x8005 : 0x1234;
                CHECK(frame[1 + (70 + y) * DISPLAY_WIDTH + 80 + i * 15 + x] == expected);
            }
        }
        RestoreTextColors(previous);
    }
    CHECK(GetMonData(&gPlayerParty[0], MON_DATA_HP_IV) == 31);
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); cgb_audio_init(42060); m4aSoundInit();
    gMain.callback2 = UiUnexpectedTick; gMain.vblankCallback = UiUnexpectedTick;
    u32 vblanks = gMain.vblankCounter1;
    NativeFrame(); NativeFrame();
    CHECK(!sUiUnexpectedTicks && gMain.vblankCounter1 == vblanks && ui->active());
    SDL_Quit();
    RhhUiAdvance(RHH_UI_SELECT | RHH_UI_R); CHECK(ui->active());
    RhhUiAdvance(0); RhhUiAdvance(RHH_UI_A);
    CHECK(GetMonData(&gPlayerParty[0], MON_DATA_HP_IV) == 12);
    RhhUiRender(frame + 1); CHECK(ui->active());
    RhhUiAdvance(0); RhhUiAdvance(RHH_UI_B); CHECK(!ui->active());
    CHECK(RhhUiKeysBlocked() && !RhhUiFilterKeys(RHH_UI_B));
    CHECK(!RhhUiFilterKeys(0) && !RhhUiKeysBlocked());
    CHECK(RhhUiFilterKeys(RHH_UI_A) == RHH_UI_A);
    RhhUiRestore(frame + 1);
    for (unsigned i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT + 2; ++i) CHECK(frame[i] == 0x1234);
    RhhModsDispatch(&tick, &decision); CHECK(ui->active());
    RhhModsSetRunContext(123, 457, 0); CHECK(!ui->active());
    RhhModsDispatch(&tick, &decision); CHECK(ui->active());
    RhhModsUnloadLibraries(); CHECK(!RhhUiActive());
    free(frame);
    puts("mod ui: input, live redraw, clipped drawing, edit guard and lifecycle passed");
}
