#include "mod_ui.h"
#include "battle_shader.h"
#include "battle_message.h"
#include "string_util.h"

static void CheckConditionHudGl(const u16 *hud)
{
    CHECK(!SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_Window *window = SDL_CreateWindow("Battle conditions", 0, 0, 480, 320, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    CHECK(window && !NativeShaderInit(window));
    RhhContentKey shader = RhhContentKeyOf("rhh:shader/battle_environment"); CHECK(NativeShaderReady(shader));
    static u16 scene[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    static u8 clean[DISPLAY_WIDTH * DISPLAY_HEIGHT * 3], preview[DISPLAY_WIDTH * DISPLAY_HEIGHT * 3];
    for (u32 i = 0; i < ARRAY_COUNT(scene); ++i) scene[i] = RGB(8, 10, 13);
    const char *background = getenv("RHH_TEST_HUD_BACKGROUND");
    if (background)
    {
        SDL_Surface *source = SDL_LoadBMP(background);
        CHECK(source && source->w == DISPLAY_WIDTH && source->h == DISPLAY_HEIGHT);
        SDL_Surface *converted = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_ABGR1555, 0);
        CHECK(converted);
        for (u32 y = 0; y < DISPLAY_HEIGHT; ++y)
            memcpy(scene + y * DISPLAY_WIDTH, (const u8 *)converted->pixels + y * converted->pitch, DISPLAY_WIDTH * sizeof(u16));
        SDL_FreeSurface(converted); SDL_FreeSurface(source);
    }
    memset(gNativeShaderSceneMask, 255, sizeof(gNativeShaderSceneMask));
    struct NativeBattleEnvironment env = {.shader = shader, .weather = RHH_WEATHER_RAIN, .visibility = 1.f};
    NativeShaderPresentBattleOverlay(scene, &env, 73, hud);
    CHECK(!NativeShaderCapture(clean) && !NativeShaderCapturePreview(preview));
    u32 shown = 0, untouched = 0;
    for (u32 i = 0; i < ARRAY_COUNT(scene); ++i)
    {
        if (hud[i] & 0x8000)
        {
            for (u32 c = 0; c < 3; ++c)
            {
                u32 value = (hud[i] >> (5 * c)) & 31;
                CHECK(preview[3 * i + c] == (value << 3 | value >> 2));
            }
            ++shown;
        }
        else { CHECK(!memcmp(clean + i * 3, preview + i * 3, 3)); ++untouched; }
    }
    CHECK(shown && untouched);
    const char *capture = getenv("RHH_TEST_HUD_CAPTURE");
    if (capture)
    {
        SDL_Surface *image = SDL_CreateRGBSurfaceWithFormatFrom(preview, DISPLAY_WIDTH, DISPLAY_HEIGHT, 24,
                                                                DISPLAY_WIDTH * 3, SDL_PIXELFORMAT_RGB24);
        CHECK(image && !SDL_SaveBMP(image, capture)); SDL_FreeSurface(image);
    }
    NativeShaderShutdown(); SDL_DestroyWindow(window); SDL_Quit();
    puts("condition HUD: composited after the battle shader");
}
static void RunCustomStatusCommand(const u8 *script)
{
    CHECK(script[0] == B_SCR_OP_ATTACKCANCELER && script[1] == B_SCR_OP_ATTACKANIMATION
        && script[2] == B_SCR_OP_WAITANIMATION && script[3] == B_SCR_OP_CALLNATIVE);
    gBattlescriptCurrInstr = script + 3;
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == script + 4 + sizeof(void *));
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
}
static void RunCustomStatusAnimation(const u8 *script)
{
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = script + 1;
    gBattleScriptingCommandsTable[B_SCR_OP_ATTACKANIMATION]();
    CHECK(gBattlescriptCurrInstr == script + 2 && gBattleControllerExecFlags);
    gBattleControllerExecFlags = 0; /* Simulate the controller completing the animation. */
    gBattleScriptingCommandsTable[B_SCR_OP_WAITANIMATION]();
    CHECK(gBattlescriptCurrInstr == script + 3);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == script + 4 + sizeof(void *));
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectNextMessage);
}
static bool32 TextEqualsAscii(const u8 *encoded, const char *ascii)
{
    for (; *ascii; ++ascii, ++encoded)
    {
        u8 expected = *ascii >= 'A' && *ascii <= 'Z' ? CHAR_A + *ascii - 'A'
            : *ascii >= 'a' && *ascii <= 'z' ? CHAR_a + *ascii - 'a'
            : *ascii == ' ' ? CHAR_SPACE : *ascii == '\n' ? CHAR_NEWLINE : *ascii == '!' ? CHAR_EXCL_MARK
            : *ascii == '\'' ? CHAR_SGL_QUOTE_RIGHT : CHAR_PERIOD;
        if (*encoded != expected) return FALSE;
    }
    return *encoded == EOS;
}
static void CheckExpiryMessages(void)
{
    gBattleTurnCounter = 3;
    gBattleStruct->eventState.endTurn = ENDTURN_COUNT;
    gBattleStruct->eventState.endTurnBattler = gBattleStruct->eventState.battlerSide = 0;
    gBattlescriptCurrInstr = BattleScript_EndTurnEvents; gBattleControllerExecFlags = 0;
    CHECK(DoEndTurnEffects() && gBattlescriptCurrInstr == BattleScript_ModConditionMessages);
    for (u32 i = 0; i < 32; ++i)
    {
        CHECK(gBattlescriptCurrInstr == BattleScript_ModConditionMessages);
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_WAITMESSAGE && gBattleControllerExecFlags);
        u32 battler = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
        u8 *buffer = gBattleResources->bufferA[battler];
        CHECK(buffer[0] == CONTROLLER_PRINTSTRING && (buffer[2] | buffer[3] << 8) == STRINGID_MOD_EFFECT_MESSAGE);
        BufferStringBattle(STRINGID_MOD_EFFECT_MESSAGE, battler);
        CHECK(TextEqualsAscii(gDisplayedStringBattle, i ? "Expired." : "Your team's Aqua Veil faded!"));
        gBattleControllerExecFlags = 0; gPauseCounterBattle = 0;
        const u8 *wait = gBattlescriptCurrInstr;
        for (u32 frame = 0; frame <= B_WAIT_TIME_LONG && gBattlescriptCurrInstr == wait; ++frame)
            gBattleScriptingCommandsTable[B_SCR_OP_WAITMESSAGE]();
        CHECK(gBattlescriptCurrInstr != wait && *gBattlescriptCurrInstr == B_SCR_OP_GOTO);
        gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    }
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModConditionMessagesEnd && !gBattleControllerExecFlags);
    CHECK(*gBattlescriptCurrInstr == B_SCR_OP_RETURN);
    gBattleScriptingCommandsTable[B_SCR_OP_RETURN]();
    CHECK(gBattlescriptCurrInstr == BattleScript_EndTurnEvents && !DoEndTurnEffects());
    /* Resuming the real end-turn command must not roll the speed tie again. */
    rng_value_t rngBefore = gRngValue;
    u32 tiesBefore = gBattleStruct->speedTieBreaks;
    struct BattleCallbacksStack callbacks = {0};
    struct BattleCallbacksStack *savedCallbacks = gBattleResources->battleCallbackStack;
    void (*savedMain)(void) = gBattleMainFunc;
    u32 savedAction = gCurrentActionFuncId;
    gBattleResources->battleCallbackStack = &callbacks; gBattleOutcome = B_OUTCOME_WON;
    CHECK(!EndTurnEvents() && !memcmp(&rngBefore, &gRngValue, sizeof(rngBefore))
        && tiesBefore == gBattleStruct->speedTieBreaks && !ModsConsumeEndTurnMessageResume());
    gBattleOutcome = 0; gBattleResources->battleCallbackStack = savedCallbacks;
    gBattleMainFunc = savedMain; gCurrentActionFuncId = savedAction;
}
static void CheckCustomStatusMessages(const char *const *expected, u32 count)
{
    const u8 *nextMessage = gBattlescriptCurrInstr;
    for (u32 i = 0; i < count; ++i)
    {
        CHECK(gBattlescriptCurrInstr == nextMessage && *gBattlescriptCurrInstr == B_SCR_OP_CALLNATIVE);
        gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_WAITMESSAGE && gBattleControllerExecFlags);
        u8 *buffer = gBattleResources->bufferA[gBattlerAttacker];
        CHECK(buffer[0] == CONTROLLER_PRINTSTRING && (buffer[2] | buffer[3] << 8) == STRINGID_MOD_EFFECT_MESSAGE);
        BufferStringBattle(STRINGID_MOD_EFFECT_MESSAGE, gBattlerAttacker);
        CHECK(TextEqualsAscii(gDisplayedStringBattle, expected[i]));
        gBattleControllerExecFlags = 0; gPauseCounterBattle = 0;
        const u8 *wait = gBattlescriptCurrInstr;
        for (u32 frame = 0; frame <= B_WAIT_TIME_LONG && gBattlescriptCurrInstr == wait; ++frame)
            gBattleScriptingCommandsTable[B_SCR_OP_WAITMESSAGE]();
        CHECK(gBattlescriptCurrInstr != wait && gBattleCommunication[MSG_DISPLAY] == MSG_DISPLAY_CONTINUE);
        CHECK(*gBattlescriptCurrInstr == B_SCR_OP_GOTO);
        gBattleScriptingCommandsTable[B_SCR_OP_GOTO]();
    }
    CHECK(gBattlescriptCurrInstr == nextMessage);
    gBattleScriptingCommandsTable[B_SCR_OP_CALLNATIVE]();
    CHECK(gBattlescriptCurrInstr == BattleScript_ModEffectAfterMessages && !gBattleControllerExecFlags);
}

