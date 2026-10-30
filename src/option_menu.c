#include "global.h"
#ifdef NATIVE
#include "mods.h"
#include "native_controls.h"
#include "constants/characters.h"
#define OPTION_ROW_HEIGHT 12
#define OPTION_FONT FONT_SMALL
#define OPTION_TEXT_OFFSET 0
#define OPTION_FRAME_TILE_BASE 0x1C0
#define OPTION_TOP 32
#define tMusic data[7]
#define tEnvironment data[8]
#define tControlSelection data[9]
#define tControlTop data[10]
#define tOverworldSpeed data[11]
#define tBattleSpeed data[12]
#else
#define OPTION_ROW_HEIGHT 16
#define OPTION_FONT FONT_NORMAL
#define OPTION_TEXT_OFFSET 1
#define OPTION_FRAME_TILE_BASE 0x1A2
#define OPTION_TOP 40
#endif
#include "option_menu.h"
#include "bg.h"
#include "gpu_regs.h"
#include "international_string_util.h"
#include "main.h"
#include "menu.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sprite.h"
#include "strings.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "gba/m4a_internal.h"
#include "constants/rgb.h"

#define tMenuSelection data[0]
#define tTextSpeed data[1]
#define tBattleSceneOff data[2]
#define tBattleStyle data[3]
#define tSound data[4]
#define tButtonMode data[5]
#define tWindowFrameType data[6]

enum
{
    MENUITEM_TEXTSPEED,
#ifdef NATIVE
    MENUITEM_OVERWORLD_SPEED,
    MENUITEM_BATTLE_SPEED,
#endif
    MENUITEM_BATTLESCENE,
    MENUITEM_BATTLESTYLE,
    MENUITEM_SOUND,
    MENUITEM_BUTTONMODE,
    MENUITEM_FRAMETYPE,
#ifdef NATIVE
    MENUITEM_MUSIC,
    MENUITEM_ENVIRONMENT,
    MENUITEM_CONTROLS,
#endif
    MENUITEM_CANCEL,
    MENUITEM_COUNT,
};

enum
{
    WIN_HEADER,
    WIN_OPTIONS
};

#define YPOS_TEXTSPEED    (MENUITEM_TEXTSPEED * OPTION_ROW_HEIGHT)
#define YPOS_BATTLESCENE  (MENUITEM_BATTLESCENE * OPTION_ROW_HEIGHT)
#define YPOS_BATTLESTYLE  (MENUITEM_BATTLESTYLE * OPTION_ROW_HEIGHT)
#define YPOS_SOUND        (MENUITEM_SOUND * OPTION_ROW_HEIGHT)
#define YPOS_BUTTONMODE   (MENUITEM_BUTTONMODE * OPTION_ROW_HEIGHT)
#define YPOS_FRAMETYPE    (MENUITEM_FRAMETYPE * OPTION_ROW_HEIGHT)

static void Task_OptionMenuFadeIn(u8 taskId);
static void Task_OptionMenuProcessInput(u8 taskId);
static void Task_OptionMenuSave(u8 taskId);
static void Task_OptionMenuFadeOut(u8 taskId);
static void HighlightOptionMenuItem(u8 selection);
static u8 TextSpeed_ProcessInput(u8 selection);
static void TextSpeed_DrawChoices(u8 selection);
static u8 BattleScene_ProcessInput(u8 selection);
static void BattleScene_DrawChoices(u8 selection);
static u8 BattleStyle_ProcessInput(u8 selection);
static void BattleStyle_DrawChoices(u8 selection);
static u8 Sound_ProcessInput(u8 selection);
static void Sound_DrawChoices(u8 selection);
static u8 FrameType_ProcessInput(u8 selection);
static void FrameType_DrawChoices(u8 selection);
static u8 ButtonMode_ProcessInput(u8 selection);
static void ButtonMode_DrawChoices(u8 selection);
static void DrawHeaderText(void);
static void DrawOptionMenuTexts(void);
static void DrawBgWindowFrames(void);
#ifdef NATIVE
static void Music_DrawChoice(u8 selection);
static void Environment_DrawChoice(u8 selection);
static void Task_ControlsInput(u8 taskId);
static void Task_ControlsCapture(u8 taskId);
static void DrawControls(u8 taskId, const u8 *header);
static void RestoreOptions(u8 taskId);
static void DrawNativeOptions(u8 taskId);
static void ProcessNativeOptions(u8 taskId);
static u32 sOptionTop;
static const u8 *const sControlNames[] = {
    COMPOUND_STRING("A"), COMPOUND_STRING("B"), COMPOUND_STRING("Start"), COMPOUND_STRING("Select"),
    COMPOUND_STRING("L"), COMPOUND_STRING("R"), COMPOUND_STRING("Up"), COMPOUND_STRING("Down"),
    COMPOUND_STRING("Left"), COMPOUND_STRING("Right"), COMPOUND_STRING("Reset defaults"), COMPOUND_STRING("Back")
};
#define CONTROL_VISIBLE 8
#define CONTROL_RESET CONTROL_COUNT
#define CONTROL_BACK (CONTROL_COUNT + 1)
static const u8 sControlHeader[] = _("Keyboard  A: Change  B: Back");
static const u8 sCaptureHeader[] = _("Press a key  Esc: Cancel");
static const u8 sControlError[] = _("Could not save controls");
static const u8 *const sMusicNames[] = {
    COMPOUND_STRING("{COLOR GREEN}{SHADOW LIGHT_GREEN}Original"),
    COMPOUND_STRING("{COLOR GREEN}{SHADOW LIGHT_GREEN}Gen I"),
    COMPOUND_STRING("{COLOR GREEN}{SHADOW LIGHT_GREEN}Gen II"),
    COMPOUND_STRING("{COLOR GREEN}{SHADOW LIGHT_GREEN}Gen III"),
    COMPOUND_STRING("{COLOR GREEN}{SHADOW LIGHT_GREEN}Gen IV"),
    COMPOUND_STRING("{COLOR GREEN}{SHADOW LIGHT_GREEN}Gen V")
};
#endif

EWRAM_DATA static bool8 sArrowPressed = FALSE;

static const u8 gText_Option[]             = _("Options");
static const u8 gText_TextSpeedSlow[]      = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}Slow");
static const u8 gText_TextSpeedMid[]       = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}Mid");
static const u8 gText_TextSpeedFast[]      = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}Fast");
static const u8 gText_BattleSceneOn[]      = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}On");
static const u8 gText_BattleSceneOff[]     = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}Off");
static const u8 gText_BattleStyleShift[]   = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}Shift");
static const u8 gText_BattleStyleSet[]     = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}Set");
static const u8 gText_SoundMono[]          = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}Mono");
static const u8 gText_SoundStereo[]        = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}Stereo");
static const u8 gText_FrameType[]          = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}Type");
static const u8 gText_FrameTypeNumber[]    = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}");
static const u8 gText_ButtonTypeNormal[]   = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}Normal");
static const u8 gText_ButtonTypeLR[]       = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}LR");
static const u8 gText_ButtonTypeLEqualsA[] = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}L=A");

static const u16 sOptionMenuText_Pal[] = INCGFX_U16("graphics/interface/option_menu_text.pal", ".gbapal");
// note: this is only used in the Japanese release
static const u8 sEqualSignGfx[] = INCGFX_U8("graphics/interface/option_menu_equals_sign.png", ".4bpp");

static const u8 *const sOptionMenuItemsNames[MENUITEM_COUNT] =
{
    [MENUITEM_TEXTSPEED]   = COMPOUND_STRING("Text speed"),
#ifdef NATIVE
    [MENUITEM_OVERWORLD_SPEED] = COMPOUND_STRING("Overworld speed"),
    [MENUITEM_BATTLE_SPEED] = COMPOUND_STRING("Battle speed"),
#endif
    [MENUITEM_BATTLESCENE] = COMPOUND_STRING("Battle scene"),
    [MENUITEM_BATTLESTYLE] = COMPOUND_STRING("Battle style"),
    [MENUITEM_SOUND]       = COMPOUND_STRING("Sound"),
    [MENUITEM_BUTTONMODE]  = COMPOUND_STRING("Button mode"),
    [MENUITEM_FRAMETYPE]   = COMPOUND_STRING("Frame"),
#ifdef NATIVE
    [MENUITEM_MUSIC]       = COMPOUND_STRING("Battle music"),
    [MENUITEM_ENVIRONMENT] = COMPOUND_STRING("Battle effects"),
    [MENUITEM_CONTROLS]   = COMPOUND_STRING("Controls"),
#endif
    [MENUITEM_CANCEL]      = COMPOUND_STRING("Back"),
};

static const struct WindowTemplate sOptionMenuWinTemplates[] =
{
    [WIN_HEADER] = {
        .bg = 1,
        .tilemapLeft = 2,
        .tilemapTop = 1,
        .width = 26,
        .height = 2,
        .paletteNum = 1,
        .baseBlock = 2
    },
    [WIN_OPTIONS] = {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = OPTION_TOP / 8,
        .width = 26,
        .height = 19 - OPTION_TOP / 8,
        .paletteNum = 1,
        .baseBlock = 0x36
    },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sOptionMenuBgTemplates[] =
{
    {
        .bg = 1,
        .charBaseIndex = 1,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0
    },
    {
        .bg = 0,
        .charBaseIndex = 1,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0
    }
};

static const u16 sOptionMenuBg_Pal[] = {RGB(17, 18, 31)};

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

void CB2_InitOptionMenu(void)
{
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        gMain.state++;
        break;
    case 1:
        DmaClearLarge16(3, (void *)(VRAM), VRAM_SIZE, 0x1000);
        DmaClear32(3, OAM, OAM_SIZE);
        DmaClear16(3, PLTT, PLTT_SIZE);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sOptionMenuBgTemplates, ARRAY_COUNT(sOptionMenuBgTemplates));
        ChangeBgX(0, 0, BG_COORD_SET);
        ChangeBgY(0, 0, BG_COORD_SET);
        ChangeBgX(1, 0, BG_COORD_SET);
        ChangeBgY(1, 0, BG_COORD_SET);
        ChangeBgX(2, 0, BG_COORD_SET);
        ChangeBgY(2, 0, BG_COORD_SET);
        ChangeBgX(3, 0, BG_COORD_SET);
        ChangeBgY(3, 0, BG_COORD_SET);
        InitWindows(sOptionMenuWinTemplates);
        DeactivateAllTextPrinters();
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG0);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG0 | WINOUT_WIN01_BG1 | WINOUT_WIN01_CLR);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG0 | BLDCNT_EFFECT_DARKEN);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 4);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
        ShowBg(0);
        ShowBg(1);
        gMain.state++;
        break;
    case 2:
        ResetPaletteFade();
        ScanlineEffect_Stop();
        ResetTasks();
        ResetSpriteData();
        gMain.state++;
        break;
    case 3:
        LoadBgTiles(1, GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->tiles, 0x120, OPTION_FRAME_TILE_BASE);
        gMain.state++;
        break;
    case 4:
        LoadPalette(sOptionMenuBg_Pal, BG_PLTT_ID(0), sizeof(sOptionMenuBg_Pal));
        LoadPalette(GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->pal, BG_PLTT_ID(7), PLTT_SIZE_4BPP);
        gMain.state++;
        break;
    case 5:
        LoadPalette(sOptionMenuText_Pal, BG_PLTT_ID(1), sizeof(sOptionMenuText_Pal));
        gMain.state++;
        break;
    case 6:
        PutWindowTilemap(WIN_HEADER);
        DrawHeaderText();
        gMain.state++;
        break;
    case 7:
        gMain.state++;
        break;
    case 8:
        PutWindowTilemap(WIN_OPTIONS);
#ifdef NATIVE
        FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
#else
        DrawOptionMenuTexts();
#endif
        gMain.state++;
    case 9:
        DrawBgWindowFrames();
        gMain.state++;
        break;
    case 10:
    {
        u8 taskId = CreateTask(Task_OptionMenuFadeIn, 0);

        gTasks[taskId].tMenuSelection = 0;
        gTasks[taskId].tTextSpeed = gSaveBlock2Ptr->optionsTextSpeed;
        gTasks[taskId].tBattleSceneOff = gSaveBlock2Ptr->optionsBattleSceneOff;
        gTasks[taskId].tBattleStyle = gSaveBlock2Ptr->optionsBattleStyle;
        gTasks[taskId].tSound = gSaveBlock2Ptr->optionsSound;
        gTasks[taskId].tButtonMode = gSaveBlock2Ptr->optionsButtonMode;
        gTasks[taskId].tWindowFrameType = gSaveBlock2Ptr->optionsWindowFrameType;
#ifdef NATIVE
        gTasks[taskId].tMusic = ModsGetConfig().music;
        gTasks[taskId].tEnvironment = ModsBattleEnvironmentEnabled();
        gTasks[taskId].tOverworldSpeed = gSaveBlock2Ptr->optionsOverworldSpeed;
        gTasks[taskId].tBattleSpeed = gSaveBlock2Ptr->optionsBattleSpeed;
        sOptionTop = 0;
        DrawNativeOptions(taskId);
#else

        TextSpeed_DrawChoices(gTasks[taskId].tTextSpeed);
        BattleScene_DrawChoices(gTasks[taskId].tBattleSceneOff);
        BattleStyle_DrawChoices(gTasks[taskId].tBattleStyle);
        Sound_DrawChoices(gTasks[taskId].tSound);
        ButtonMode_DrawChoices(gTasks[taskId].tButtonMode);
        FrameType_DrawChoices(gTasks[taskId].tWindowFrameType);
        HighlightOptionMenuItem(gTasks[taskId].tMenuSelection);
#endif

        CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
        gMain.state++;
        break;
    }
    case 11:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return;
    }
}

static void Task_OptionMenuFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_OptionMenuProcessInput;
}

static void Task_OptionMenuProcessInput(u8 taskId)
{
#ifdef NATIVE
    ProcessNativeOptions(taskId);
#else
    if (JOY_NEW(A_BUTTON))
    {
        if (gTasks[taskId].tMenuSelection == MENUITEM_CANCEL)
            gTasks[taskId].func = Task_OptionMenuSave;
#ifdef NATIVE
        else if (gTasks[taskId].tMenuSelection == MENUITEM_CONTROLS)
        {
            gTasks[taskId].tControlSelection = 0;
            gTasks[taskId].tControlTop = 0;
            DrawControls(taskId, sControlHeader);
            gTasks[taskId].func = Task_ControlsInput;
        }
#endif
    }
    else if (JOY_NEW(B_BUTTON))
    {
        gTasks[taskId].func = Task_OptionMenuSave;
    }
    else if (JOY_NEW(DPAD_UP))
    {
        if (gTasks[taskId].tMenuSelection > 0)
            gTasks[taskId].tMenuSelection--;
        else
            gTasks[taskId].tMenuSelection = MENUITEM_CANCEL;
        HighlightOptionMenuItem(gTasks[taskId].tMenuSelection);
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        if (gTasks[taskId].tMenuSelection < MENUITEM_CANCEL)
            gTasks[taskId].tMenuSelection++;
        else
            gTasks[taskId].tMenuSelection = 0;
        HighlightOptionMenuItem(gTasks[taskId].tMenuSelection);
    }
    else
    {
        u8 previousOption;

        switch (gTasks[taskId].tMenuSelection)
        {
        case MENUITEM_TEXTSPEED:
            previousOption = gTasks[taskId].tTextSpeed;
            gTasks[taskId].tTextSpeed = TextSpeed_ProcessInput(gTasks[taskId].tTextSpeed);

            if (previousOption != gTasks[taskId].tTextSpeed)
                TextSpeed_DrawChoices(gTasks[taskId].tTextSpeed);
            break;
        case MENUITEM_BATTLESCENE:
            previousOption = gTasks[taskId].tBattleSceneOff;
            gTasks[taskId].tBattleSceneOff = BattleScene_ProcessInput(gTasks[taskId].tBattleSceneOff);

            if (previousOption != gTasks[taskId].tBattleSceneOff)
                BattleScene_DrawChoices(gTasks[taskId].tBattleSceneOff);
            break;
        case MENUITEM_BATTLESTYLE:
            previousOption = gTasks[taskId].tBattleStyle;
            gTasks[taskId].tBattleStyle = BattleStyle_ProcessInput(gTasks[taskId].tBattleStyle);

            if (previousOption != gTasks[taskId].tBattleStyle)
                BattleStyle_DrawChoices(gTasks[taskId].tBattleStyle);
            break;
        case MENUITEM_SOUND:
            previousOption = gTasks[taskId].tSound;
            gTasks[taskId].tSound = Sound_ProcessInput(gTasks[taskId].tSound);

            if (previousOption != gTasks[taskId].tSound)
                Sound_DrawChoices(gTasks[taskId].tSound);
            break;
        case MENUITEM_BUTTONMODE:
            previousOption = gTasks[taskId].tButtonMode;
            gTasks[taskId].tButtonMode = ButtonMode_ProcessInput(gTasks[taskId].tButtonMode);

            if (previousOption != gTasks[taskId].tButtonMode)
                ButtonMode_DrawChoices(gTasks[taskId].tButtonMode);
            break;
        case MENUITEM_FRAMETYPE:
            previousOption = gTasks[taskId].tWindowFrameType;
            gTasks[taskId].tWindowFrameType = FrameType_ProcessInput(gTasks[taskId].tWindowFrameType);

            if (previousOption != gTasks[taskId].tWindowFrameType)
                FrameType_DrawChoices(gTasks[taskId].tWindowFrameType);
            break;
#ifdef NATIVE
        case MENUITEM_MUSIC:
            if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
            {
                gTasks[taskId].tMusic = (gTasks[taskId].tMusic + (JOY_NEW(DPAD_LEFT) ? 5 : 1)) % 6;
                Music_DrawChoice(gTasks[taskId].tMusic);
                sArrowPressed = TRUE;
            }
            break;
        case MENUITEM_ENVIRONMENT:
            if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
            {
                gTasks[taskId].tEnvironment ^= 1;
                Environment_DrawChoice(gTasks[taskId].tEnvironment);
                sArrowPressed = TRUE;
            }
            break;
#endif
        default:
            return;
        }

        if (sArrowPressed)
        {
            sArrowPressed = FALSE;
            CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
        }
    }
#endif
}

static void Task_OptionMenuSave(u8 taskId)
{
    gSaveBlock2Ptr->optionsTextSpeed = gTasks[taskId].tTextSpeed;
    gSaveBlock2Ptr->optionsBattleSceneOff = gTasks[taskId].tBattleSceneOff;
    gSaveBlock2Ptr->optionsBattleStyle = gTasks[taskId].tBattleStyle;
    gSaveBlock2Ptr->optionsSound = gTasks[taskId].tSound;
    gSaveBlock2Ptr->optionsButtonMode = gTasks[taskId].tButtonMode;
    gSaveBlock2Ptr->optionsWindowFrameType = gTasks[taskId].tWindowFrameType;
#ifdef NATIVE
    ModsSetMusic(gTasks[taskId].tMusic);
    ModsSetBattleEnvironment(gTasks[taskId].tEnvironment);
    gSaveBlock2Ptr->optionsOverworldSpeed = gTasks[taskId].tOverworldSpeed;
    gSaveBlock2Ptr->optionsBattleSpeed = gTasks[taskId].tBattleSpeed;
#endif

    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_OptionMenuFadeOut;
}

static void Task_OptionMenuFadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        SetMainCallback2(gMain.savedCallback);
    }
}

static void HighlightOptionMenuItem(u8 index)
{
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(16, DISPLAY_WIDTH - 16));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(index * OPTION_ROW_HEIGHT + OPTION_TOP,
        (index + 1) * OPTION_ROW_HEIGHT + OPTION_TOP));
}

static void DrawOptionMenuChoice(const u8 *text, u8 x, u8 y, u8 style)
{
    u8 dst[16];
    u16 i;

    for (i = 0; *text != EOS && i < ARRAY_COUNT(dst) - 1; i++)
        dst[i] = *(text++);

    if (style != 0)
    {
        dst[2] = TEXT_COLOR_RED;
        dst[5] = TEXT_COLOR_LIGHT_RED;
    }

    dst[i] = EOS;
    AddTextPrinterParameterized(WIN_OPTIONS, OPTION_FONT, dst, x, y + OPTION_TEXT_OFFSET, TEXT_SKIP_DRAW, NULL);
}

#ifdef NATIVE
static const u8 sTextSpeedValues[] = {OPTIONS_TEXT_SPEED_SLOW, OPTIONS_TEXT_SPEED_MID,
    OPTIONS_TEXT_SPEED_FAST, OPTIONS_TEXT_SPEED_VERY_FAST, OPTIONS_TEXT_SPEED_INSTANT};
static const u8 *const sTextSpeedLabels[] = {COMPOUND_STRING("Slow"), COMPOUND_STRING("Medium"),
    COMPOUND_STRING("Fast"), COMPOUND_STRING("Faster"), COMPOUND_STRING("Instant")};
static const u8 *const sGameSpeedLabels[] = {COMPOUND_STRING("1x"), COMPOUND_STRING("1.5x"),
    COMPOUND_STRING("2x"), COMPOUND_STRING("3x")};
static const u8 *const sButtonLabels[] = {COMPOUND_STRING("Normal"), COMPOUND_STRING("LR"), COMPOUND_STRING("L=A")};
static const u8 sOptionHint[] = _("Left/right: change  B: back");
static unsigned TextSliderPosition(unsigned value)
{
    for (unsigned i = 0; i < ARRAY_COUNT(sTextSpeedValues); ++i)
        if (sTextSpeedValues[i] == value) return i;
    return 2;
}
static void DrawSlider(unsigned row, unsigned position, unsigned count, const u8 *label)
{
    unsigned y = row * 13;
    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(3), 106, y + 5, 54, 2);
    for (unsigned i = 0; i < count; ++i)
        FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(i == position ? 2 : 3), 105 + i * 52 / (count - 1), y + 3, 4, 6);
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_SMALL, label, 164, y, TEXT_SKIP_DRAW, NULL);
}
static void DrawNativeOptions(u8 taskId)
{
    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    for (unsigned row = 0; row < 8 && sOptionTop + row < MENUITEM_COUNT; ++row)
    {
        unsigned item = sOptionTop + row, y = row * 13;
        const u8 *value = NULL;
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_SMALL, sOptionMenuItemsNames[item], 8, y, TEXT_SKIP_DRAW, NULL);
        switch (item)
        {
        case MENUITEM_TEXTSPEED:
        {
            unsigned position = TextSliderPosition(gTasks[taskId].tTextSpeed);
            DrawSlider(row, position, 5, sTextSpeedLabels[position]); break;
        }
        case MENUITEM_OVERWORLD_SPEED:
            DrawSlider(row, gTasks[taskId].tOverworldSpeed, 4, sGameSpeedLabels[gTasks[taskId].tOverworldSpeed]); break;
        case MENUITEM_BATTLE_SPEED:
            DrawSlider(row, gTasks[taskId].tBattleSpeed, 4, sGameSpeedLabels[gTasks[taskId].tBattleSpeed]); break;
        case MENUITEM_BATTLESCENE: value = gTasks[taskId].tBattleSceneOff ? gText_BattleSceneOff : gText_BattleSceneOn; break;
        case MENUITEM_BATTLESTYLE: value = gTasks[taskId].tBattleStyle ? gText_BattleStyleSet : gText_BattleStyleShift; break;
        case MENUITEM_SOUND: value = gTasks[taskId].tSound ? gText_SoundStereo : gText_SoundMono; break;
        case MENUITEM_BUTTONMODE: value = sButtonLabels[gTasks[taskId].tButtonMode % 3]; break;
        case MENUITEM_FRAMETYPE:
        {
            u8 number[8]; ConvertIntToDecimalStringN(number, gTasks[taskId].tWindowFrameType + 1, STR_CONV_MODE_LEFT_ALIGN, 2);
            AddTextPrinterParameterized(WIN_OPTIONS, FONT_SMALL, number, 106, y, TEXT_SKIP_DRAW, NULL); break;
        }
        case MENUITEM_MUSIC: value = sMusicNames[gTasks[taskId].tMusic % ARRAY_COUNT(sMusicNames)]; break;
        case MENUITEM_ENVIRONMENT: value = gTasks[taskId].tEnvironment ? gText_BattleSceneOn : gText_BattleSceneOff; break;
        case MENUITEM_CONTROLS: value = COMPOUND_STRING("Open"); break;
        }
        if (value) AddTextPrinterParameterized(WIN_OPTIONS, FONT_SMALL, value, 106, y, TEXT_SKIP_DRAW, NULL);
    }
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_SMALL, sOptionHint, 8, 108, TEXT_SKIP_DRAW, NULL);
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(16, DISPLAY_WIDTH - 16));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE((gTasks[taskId].tMenuSelection - sOptionTop) * 13 + OPTION_TOP,
        (gTasks[taskId].tMenuSelection - sOptionTop + 1) * 13 + OPTION_TOP));
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
}
static void ProcessNativeOptions(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    unsigned selected = task->tMenuSelection;
    if (JOY_NEW(B_BUTTON) || (JOY_NEW(A_BUTTON) && selected == MENUITEM_CANCEL))
    { task->func = Task_OptionMenuSave; return; }
    if (JOY_NEW(A_BUTTON) && selected == MENUITEM_CONTROLS)
    {
        task->tControlSelection = task->tControlTop = 0;
        DrawControls(taskId, sControlHeader); task->func = Task_ControlsInput; return;
    }
    if (JOY_NEW(DPAD_UP)) selected = selected ? selected - 1 : MENUITEM_CANCEL;
    else if (JOY_NEW(DPAD_DOWN)) selected = selected < MENUITEM_CANCEL ? selected + 1 : 0;
    else if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT | A_BUTTON))
    {
        int delta = JOY_NEW(DPAD_LEFT) ? -1 : 1;
        switch (selected)
        {
        case MENUITEM_TEXTSPEED:
        {
            int pos = (int)TextSliderPosition(task->tTextSpeed) + delta;
            if (pos < 0) pos = 0; if (pos > 4) pos = 4;
            task->tTextSpeed = sTextSpeedValues[pos]; break;
        }
        case MENUITEM_OVERWORLD_SPEED:
        case MENUITEM_BATTLE_SPEED:
        {
            s16 *value = selected == MENUITEM_OVERWORLD_SPEED ? &task->tOverworldSpeed : &task->tBattleSpeed;
            int next = *value + delta; if (next < 0) next = 0; if (next > 3) next = 3;
            *value = next; break;
        }
        case MENUITEM_BATTLESCENE: task->tBattleSceneOff ^= 1; break;
        case MENUITEM_BATTLESTYLE: task->tBattleStyle ^= 1; break;
        case MENUITEM_SOUND: task->tSound ^= 1; SetPokemonCryStereo(task->tSound); break;
        case MENUITEM_BUTTONMODE: task->tButtonMode = (task->tButtonMode + (delta < 0 ? 2 : 1)) % 3; break;
        case MENUITEM_FRAMETYPE:
            task->tWindowFrameType = (task->tWindowFrameType + (delta < 0 ? WINDOW_FRAMES_COUNT - 1 : 1)) % WINDOW_FRAMES_COUNT;
            LoadBgTiles(1, GetWindowFrameTilesPal(task->tWindowFrameType)->tiles, 0x120, OPTION_FRAME_TILE_BASE);
            LoadPalette(GetWindowFrameTilesPal(task->tWindowFrameType)->pal, BG_PLTT_ID(7), PLTT_SIZE_4BPP); break;
        case MENUITEM_MUSIC: task->tMusic = (task->tMusic + (delta < 0 ? 5 : 1)) % 6; break;
        case MENUITEM_ENVIRONMENT: task->tEnvironment ^= 1; break;
        default: return;
        }
    }
    else return;
    task->tMenuSelection = selected;
    if (selected < sOptionTop) sOptionTop = selected;
    if (selected >= sOptionTop + 8) sOptionTop = selected - 7;
    DrawNativeOptions(taskId);
}
static void ControlKeyText(u8 *out, const char *name)
{
    unsigned n = 0;
    while (*name && n < 15)
    {
        unsigned char c = *name++;
        if (c >= 'A' && c <= 'Z') out[n++] = CHAR_A + c - 'A';
        else if (c >= 'a' && c <= 'z') out[n++] = CHAR_a + c - 'a';
        else if (c >= '0' && c <= '9') out[n++] = CHAR_0 + c - '0';
        else if (c == ' ') out[n++] = CHAR_SPACE;
        else if (c == '-') out[n++] = CHAR_HYPHEN;
        else if (c == '/') out[n++] = CHAR_SLASH;
        else if (c == '.') out[n++] = CHAR_PERIOD;
        else out[n++] = CHAR_QUESTION_MARK;
    }
    out[n] = EOS;
}
static void DrawControls(u8 taskId, const u8 *header)
{
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(1));
    AddTextPrinterParameterized(WIN_HEADER, FONT_SMALL, header, 4, 2, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    unsigned top = gTasks[taskId].tControlTop;
    for (unsigned row = 0; row < CONTROL_VISIBLE && top + row <= CONTROL_BACK; ++row)
    {
        unsigned item = top + row;
        u8 y = row * 13 + 1;
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_SMALL, sControlNames[item], 8, y, TEXT_SKIP_DRAW, NULL);
        if (item < CONTROL_COUNT)
        {
            u8 text[16]; ControlKeyText(text, NativeControlsName(item));
            AddTextPrinterParameterized(WIN_OPTIONS, FONT_SMALL, text, 104, y, TEXT_SKIP_DRAW, NULL);
        }
    }
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(16, DISPLAY_WIDTH - 16));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE((gTasks[taskId].tControlSelection - top) * 13 + OPTION_TOP,
        (gTasks[taskId].tControlSelection - top + 1) * 13 + OPTION_TOP));
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
}
static void RestoreOptions(u8 taskId)
{
    DrawHeaderText(); DrawNativeOptions(taskId);
    gTasks[taskId].func = Task_OptionMenuProcessInput;
}
static void Task_ControlsInput(u8 taskId)
{
    int selected = gTasks[taskId].tControlSelection;
    if (JOY_NEW(B_BUTTON) || (JOY_NEW(A_BUTTON) && selected == CONTROL_BACK))
    { RestoreOptions(taskId); return; }
    if (JOY_NEW(A_BUTTON))
    {
        if (selected == CONTROL_RESET)
            DrawControls(taskId, NativeControlsReset() ? sControlError : sControlHeader);
        else
        {
            NativeControlsBeginCapture(selected);
            DrawControls(taskId, sCaptureHeader);
            gTasks[taskId].func = Task_ControlsCapture;
        }
        return;
    }
    if (JOY_NEW(DPAD_UP)) selected = selected ? selected - 1 : CONTROL_BACK;
    else if (JOY_NEW(DPAD_DOWN)) selected = selected < CONTROL_BACK ? selected + 1 : 0;
    else return;
    gTasks[taskId].tControlSelection = selected;
    if (selected < gTasks[taskId].tControlTop) gTasks[taskId].tControlTop = selected;
    if (selected >= gTasks[taskId].tControlTop + CONTROL_VISIBLE)
        gTasks[taskId].tControlTop = selected - CONTROL_VISIBLE + 1;
    DrawControls(taskId, sControlHeader);
}
static void Task_ControlsCapture(u8 taskId)
{
    int result = NativeControlsCaptureResult();
    if (result == CONTROL_WAITING) return;
    DrawControls(taskId, result == CONTROL_SAVE_FAILED ? sControlError : sControlHeader);
    gTasks[taskId].func = Task_ControlsInput;
}
static void Music_DrawChoice(u8 selection)
{
    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, MENUITEM_MUSIC * OPTION_ROW_HEIGHT, 104, OPTION_ROW_HEIGHT);
    DrawOptionMenuChoice(sMusicNames[selection % ARRAY_COUNT(sMusicNames)], 104, MENUITEM_MUSIC * OPTION_ROW_HEIGHT, 1);
}
static void Environment_DrawChoice(u8 selection)
{
    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, MENUITEM_ENVIRONMENT * OPTION_ROW_HEIGHT, 104, OPTION_ROW_HEIGHT);
    DrawOptionMenuChoice(selection ? gText_BattleSceneOn : gText_BattleSceneOff, 104,
        MENUITEM_ENVIRONMENT * OPTION_ROW_HEIGHT, 1);
}
#endif

static u8 TextSpeed_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_RIGHT))
    {
        if (selection <= 1)
            selection++;
        else
            selection = 0;

        sArrowPressed = TRUE;
    }
    if (JOY_NEW(DPAD_LEFT))
    {
        if (selection != 0)
            selection--;
        else
            selection = 2;

        sArrowPressed = TRUE;
    }
    return selection;
}

static void TextSpeed_DrawChoices(u8 selection)
{
    u8 styles[3];
    s32 widthSlow, widthMid, widthFast, xMid;

    styles[0] = 0;
    styles[1] = 0;
    styles[2] = 0;
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_TextSpeedSlow, 104, YPOS_TEXTSPEED, styles[0]);

    widthSlow = GetStringWidth(OPTION_FONT, gText_TextSpeedSlow, 0);
    widthMid = GetStringWidth(OPTION_FONT, gText_TextSpeedMid, 0);
    widthFast = GetStringWidth(OPTION_FONT, gText_TextSpeedFast, 0);

    widthMid -= 94;
    xMid = (widthSlow - widthMid - widthFast) / 2 + 104;
    DrawOptionMenuChoice(gText_TextSpeedMid, xMid, YPOS_TEXTSPEED, styles[1]);

    DrawOptionMenuChoice(gText_TextSpeedFast, GetStringRightAlignXOffset(OPTION_FONT, gText_TextSpeedFast, 198), YPOS_TEXTSPEED, styles[2]);
}

static u8 BattleScene_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
    {
        selection ^= 1;
        sArrowPressed = TRUE;
    }

    return selection;
}

static void BattleScene_DrawChoices(u8 selection)
{
    u8 styles[2];

    styles[0] = 0;
    styles[1] = 0;
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, YPOS_BATTLESCENE, styles[0]);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(OPTION_FONT, gText_BattleSceneOff, 198), YPOS_BATTLESCENE, styles[1]);
}

static u8 BattleStyle_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
    {
        selection ^= 1;
        sArrowPressed = TRUE;
    }

    return selection;
}

static void BattleStyle_DrawChoices(u8 selection)
{
    u8 styles[2];

    styles[0] = 0;
    styles[1] = 0;
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_BattleStyleShift, 104, YPOS_BATTLESTYLE, styles[0]);
    DrawOptionMenuChoice(gText_BattleStyleSet, GetStringRightAlignXOffset(OPTION_FONT, gText_BattleStyleSet, 198), YPOS_BATTLESTYLE, styles[1]);
}

static u8 Sound_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
    {
        selection ^= 1;
        SetPokemonCryStereo(selection);
        sArrowPressed = TRUE;
    }

    return selection;
}

static void Sound_DrawChoices(u8 selection)
{
    u8 styles[2];

    styles[0] = 0;
    styles[1] = 0;
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_SoundMono, 104, YPOS_SOUND, styles[0]);
    DrawOptionMenuChoice(gText_SoundStereo, GetStringRightAlignXOffset(OPTION_FONT, gText_SoundStereo, 198), YPOS_SOUND, styles[1]);
}

static u8 FrameType_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_RIGHT))
    {
        if (selection < WINDOW_FRAMES_COUNT - 1)
            selection++;
        else
            selection = 0;

        LoadBgTiles(1, GetWindowFrameTilesPal(selection)->tiles, 0x120, OPTION_FRAME_TILE_BASE);
        LoadPalette(GetWindowFrameTilesPal(selection)->pal, BG_PLTT_ID(7), PLTT_SIZE_4BPP);
        sArrowPressed = TRUE;
    }
    if (JOY_NEW(DPAD_LEFT))
    {
        if (selection != 0)
            selection--;
        else
            selection = WINDOW_FRAMES_COUNT - 1;

        LoadBgTiles(1, GetWindowFrameTilesPal(selection)->tiles, 0x120, OPTION_FRAME_TILE_BASE);
        LoadPalette(GetWindowFrameTilesPal(selection)->pal, BG_PLTT_ID(7), PLTT_SIZE_4BPP);
        sArrowPressed = TRUE;
    }
    return selection;
}

static void FrameType_DrawChoices(u8 selection)
{
    u8 text[16] = {EOS};
    u8 n = selection + 1;
    u16 i;

    for (i = 0; gText_FrameTypeNumber[i] != EOS && i <= 5; i++)
        text[i] = gText_FrameTypeNumber[i];

    // Convert a number to decimal string
    if (n / 10 != 0)
    {
        text[i] = n / 10 + CHAR_0;
        i++;
        text[i] = n % 10 + CHAR_0;
        i++;
    }
    else
    {
        text[i] = n % 10 + CHAR_0;
        i++;
        text[i] = CHAR_SPACER;
        i++;
    }

    text[i] = EOS;

    DrawOptionMenuChoice(gText_FrameType, 104, YPOS_FRAMETYPE, 0);
    DrawOptionMenuChoice(text, 128, YPOS_FRAMETYPE, 1);
}

static u8 ButtonMode_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_RIGHT))
    {
        if (selection <= 1)
            selection++;
        else
            selection = 0;

        sArrowPressed = TRUE;
    }
    if (JOY_NEW(DPAD_LEFT))
    {
        if (selection != 0)
            selection--;
        else
            selection = 2;

        sArrowPressed = TRUE;
    }
    return selection;
}

static void ButtonMode_DrawChoices(u8 selection)
{
    s32 widthNormal, widthLR, widthLA, xLR;
    u8 styles[3];

    styles[0] = 0;
    styles[1] = 0;
    styles[2] = 0;
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_ButtonTypeNormal, 104, YPOS_BUTTONMODE, styles[0]);

    widthNormal = GetStringWidth(OPTION_FONT, gText_ButtonTypeNormal, 0);
    widthLR = GetStringWidth(OPTION_FONT, gText_ButtonTypeLR, 0);
    widthLA = GetStringWidth(OPTION_FONT, gText_ButtonTypeLEqualsA, 0);

    widthLR -= 94;
    xLR = (widthNormal - widthLR - widthLA) / 2 + 104;
    DrawOptionMenuChoice(gText_ButtonTypeLR, xLR, YPOS_BUTTONMODE, styles[1]);

    DrawOptionMenuChoice(gText_ButtonTypeLEqualsA, GetStringRightAlignXOffset(OPTION_FONT, gText_ButtonTypeLEqualsA, 198), YPOS_BUTTONMODE, styles[2]);
}

static void DrawHeaderText(void)
{
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(1));
    AddTextPrinterParameterized(WIN_HEADER, FONT_NORMAL, gText_Option, 8, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
}

static void DrawOptionMenuTexts(void)
{
    u8 i;

    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    for (i = 0; i < MENUITEM_COUNT; i++)
        AddTextPrinterParameterized(WIN_OPTIONS, OPTION_FONT, sOptionMenuItemsNames[i], 8, (i * OPTION_ROW_HEIGHT) + OPTION_TEXT_OFFSET, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
}

#define TILE_TOP_CORNER_L (OPTION_FRAME_TILE_BASE + 0)
#define TILE_TOP_EDGE (OPTION_FRAME_TILE_BASE + 1)
#define TILE_TOP_CORNER_R (OPTION_FRAME_TILE_BASE + 2)
#define TILE_LEFT_EDGE (OPTION_FRAME_TILE_BASE + 3)
#define TILE_RIGHT_EDGE (OPTION_FRAME_TILE_BASE + 5)
#define TILE_BOT_CORNER_L (OPTION_FRAME_TILE_BASE + 6)
#define TILE_BOT_EDGE (OPTION_FRAME_TILE_BASE + 7)
#define TILE_BOT_CORNER_R (OPTION_FRAME_TILE_BASE + 8)

static void DrawBgWindowFrames(void)
{
    //                     bg, tile,              x, y, width, height, palNum
    // Draw title window frame
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1,  0,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2,  0, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28,  0,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1,  1,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28,  1,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1,  3,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2,  3, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28,  3,  1,  1,  7);

    // Draw options list window frame
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1, OPTION_TOP / 8 - 1, 1, 1, 7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2, OPTION_TOP / 8 - 1, 26, 1, 7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28, OPTION_TOP / 8 - 1, 1, 1, 7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1, OPTION_TOP / 8, 1, 19 - OPTION_TOP / 8, 7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28, OPTION_TOP / 8, 1, 19 - OPTION_TOP / 8, 7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1, 19,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2, 19, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28, 19,  1,  1,  7);

    CopyBgTilemapBufferToVram(1);
}
