#include "global.h"
#ifdef NATIVE
#include "mods.h"
#include "mod_host.h"
#include "main.h"
#include "bg.h"
#include "gpu_regs.h"
#include "palette.h"
#include "sprite.h"
#include "task.h"
#include "window.h"
#include "text.h"
#include "text_window.h"
#include "menu.h"
#include "pokemon.h"
#include "string_util.h"
#include "sound.h"
#include "constants/songs.h"
#include "constants/characters.h"
#include "constants/rgb.h"

static void (*sResume)(void), (*sCancel)(void);
static u32 sRow, sPickIndex;
static bool32 sPicking, sHelp, sLeaving, sAccept, sEnded;
static u16 sTilemap[1024];
static const struct BgTemplate sBg = {.bg = 0, .charBaseIndex = 0, .mapBaseIndex = 31};
static const struct WindowTemplate sWindows[] = {
    {.bg = 0, .tilemapLeft = 1, .tilemapTop = 1, .width = 28, .height = 15, .paletteNum = 14, .baseBlock = 32},
    {.bg = 0, .tilemapLeft = 1, .tilemapTop = 17, .width = 28, .height = 2, .paletteNum = 14, .baseBlock = 452},
    DUMMY_WIN_TEMPLATE
};
static const u8 *const sLabels[] = {
    COMPOUND_STRING("Begin adventure"), COMPOUND_STRING("Nuzlocke"),
    COMPOUND_STRING("Wild Pokemon"), COMPOUND_STRING("Trainer Pokemon"),
    COMPOUND_STRING("Starter (middle ball)"), COMPOUND_STRING("Base stat total"),
    COMPOUND_STRING("Party level cap"), COMPOUND_STRING("Follower Pokemon"),
    COMPOUND_STRING("Battle music")
};
static const u8 *const sDescriptions[] = {
    COMPOUND_STRING("A: begin   B: back   SELECT: rules"),
    COMPOUND_STRING("First encounter, permanent fainting. SELECT: rules"),
    COMPOUND_STRING("Pool: stable area tables. Full: each encounter."),
    COMPOUND_STRING("Stable species per trainer and party slot."),
    COMPOUND_STRING("A: choose any non-legendary starter."),
    COMPOUND_STRING("600 total; original stat proportions."),
    COMPOUND_STRING("Next Gym ace, then Champion ace."),
    COMPOUND_STRING("Your first healthy Pokemon follows you."),
    COMPOUND_STRING("MIDI + SoundFont. Also changeable in Options.")
};
static const u8 *const sWild[] = {COMPOUND_STRING("Off"), COMPOUND_STRING("Pool"), COMPOUND_STRING("Full")};
static const u8 *const sMusic[] = {COMPOUND_STRING("Original"), COMPOUND_STRING("Gen I"), COMPOUND_STRING("Gen II"),
    COMPOUND_STRING("Gen III"), COMPOUND_STRING("Gen IV"), COMPOUND_STRING("Gen V")};
static const u32 sFlags[] = {0, MOD_NUZLOCKE, 0, MOD_TRAINERS, 0, MOD_EQUALIZE, MOD_LEVEL_CAP, MOD_FOLLOWER, 0};
static const u8 sHelpText[] = _("NUZLOCKE RULES\nRules start when you obtain Poke Balls.\nOne encounter attempt per named area.\nFleeing or a failed catch uses that attempt.\nA fainted Pokemon cannot be revived.\nBoxing or healing cannot clear a death.\nA wipeout ends the run.\nEncounters and deaths survive reloading.\nA / B: return to settings");

static void Print(u32 win, const u8 *text, u32 x, u32 y, bool32 selected)
{
    const u8 colors[3] = {TEXT_COLOR_WHITE, selected ? TEXT_COLOR_RED : TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
    AddTextPrinterParameterized3(win, FONT_SMALL, x, y, colors, TEXT_SKIP_DRAW, text);
}
static void SpeciesLabel(u8 *dest, u32 species)
{
    /* Canonical names distinguish permanent forms with identical display names. */
    const char *name = RhhContentName(RHH_SPECIES, species);
    if (!name) { StringCopy(dest, gSpeciesInfo[species].speciesName); return; }
    name += sizeof("rhh:species/") - 1;
    u32 i = 0;
    for (; name[i] && i < 36; ++i)
    {
        char c = name[i];
        dest[i] = c >= 'a' && c <= 'z' ? CHAR_A + c - 'a'
            : c >= '0' && c <= '9' ? CHAR_0 + c - '0' : CHAR_SPACE;
    }
    dest[i] = EOS;
}
static void Draw(void)
{
    FillWindowPixelBuffer(0, PIXEL_FILL(1)); FillWindowPixelBuffer(1, PIXEL_FILL(1));
    if (sEnded)
    {
        Print(0, COMPOUND_STRING("NUZLOCKE RUN ENDED\n\nYour party was defeated.\nFainted Pokemon remain permanently lost.\n\nStart a new adventure from the title menu.\n\nA / B: return to title"), 4, 0, FALSE);
    }
    else if (sHelp) Print(0, sHelpText, 4, 0, FALSE);
    else if (sPicking)
    {
        Print(0, COMPOUND_STRING("CHOOSE STARTER"), 4, 0, FALSE);
        u32 page = sPickIndex / 9 * 9;
        for (u32 i = 0; i < 9 && page + i <= ModsStarterCount(); ++i)
        {
            u8 label[40];
            if (!page && !i) StringCopy(label, COMPOUND_STRING("Original starters"));
            else SpeciesLabel(label, ModsStarterAt(page + i - 1));
            Print(0, label, 12, 12 + i * 12, page + i == sPickIndex);
            if (page + i == sPickIndex) Print(0, COMPOUND_STRING(">"), 2, 12 + i * 12, TRUE);
        }
        Print(1, COMPOUND_STRING("Up/Down: choose  L/R: page  A: set  B: back"), 0, 0, FALSE);
    }
    else
    {
        Print(0, COMPOUND_STRING("NEW GAME - RULES"), 4, 0, FALSE);
        for (u32 i = 0; i < ARRAY_COUNT(sLabels); ++i)
        {
            const u8 *value = NULL;
            if (i == 2) value = sWild[gModDraft.wildMode];
            else if (i == 8) value = sMusic[gModDraft.music];
            else if (i == 4)
            {
                u32 species;
                value = COMPOUND_STRING("Original");
                if (gModDraft.starter && !RhhContentResolve(RHH_SPECIES, gModDraft.starter, &species))
                    value = gSpeciesInfo[species].speciesName;
            }
            else if (i) value = gModDraft.flags & sFlags[i]
                ? (i == 5 ? COMPOUND_STRING("600") : COMPOUND_STRING("On")) : COMPOUND_STRING("Off");
            Print(0, sLabels[i], 12, 12 + i * 12, sRow == i);
            if (value) Print(0, value, 130, 12 + i * 12, sRow == i);
            if (sRow == i) Print(0, COMPOUND_STRING(">"), 2, 12 + i * 12, TRUE);
        }
        Print(1, sDescriptions[sRow], 0, 0, FALSE);
    }
    CopyWindowToVram(0, COPYWIN_FULL); CopyWindowToVram(1, COPYWIN_FULL);
}
static void VBlank(void) { LoadOam(); ProcessSpriteCopyRequests(); TransferPlttBuffer(); }
static void Main(void)
{
    if (sLeaving)
    {
        if (!gPaletteFade.active)
        {
            SetVBlankCallback(NULL); FreeAllWindowBuffers(); UnsetBgTilemapBuffer(0);
            SetMainCallback2(sAccept ? sResume : sCancel);
            return;
        }
    }
    else if (!gPaletteFade.active)
    {
        bool32 redraw = TRUE;
        if (sEnded)
        {
            if (JOY_NEW(A_BUTTON | B_BUTTON))
            { sAccept = FALSE; sLeaving = TRUE; BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK); }
            else redraw = FALSE;
        }
        else if (sHelp)
        { if (JOY_NEW(A_BUTTON | B_BUTTON | SELECT_BUTTON)) sHelp = FALSE; else redraw = FALSE; }
        else if (sPicking)
        {
            u32 total = ModsStarterCount() + 1;
            if (JOY_REPEAT(DPAD_UP)) sPickIndex = (sPickIndex + total - 1) % total;
            else if (JOY_REPEAT(DPAD_DOWN)) sPickIndex = (sPickIndex + 1) % total;
            else if (JOY_NEW(L_BUTTON | DPAD_LEFT)) sPickIndex = sPickIndex >= 9 ? sPickIndex - 9 : 0;
            else if (JOY_NEW(R_BUTTON | DPAD_RIGHT)) sPickIndex = min(sPickIndex + 9, total - 1);
            else if (JOY_NEW(A_BUTTON))
            {
                gModDraft.starter = sPickIndex ? RhhContentKeyForId(RHH_SPECIES, ModsStarterAt(sPickIndex - 1)) : 0;
                sPicking = FALSE;
            }
            else if (JOY_NEW(B_BUTTON)) sPicking = FALSE;
            else redraw = FALSE;
        }
        else if (JOY_NEW(SELECT_BUTTON)) sHelp = TRUE;
        else if (JOY_REPEAT(DPAD_UP)) sRow = (sRow + ARRAY_COUNT(sLabels) - 1) % ARRAY_COUNT(sLabels);
        else if (JOY_REPEAT(DPAD_DOWN)) sRow = (sRow + 1) % ARRAY_COUNT(sLabels);
        else if (JOY_NEW(B_BUTTON) || (JOY_NEW(A_BUTTON | START_BUTTON) && !sRow))
        {
            sAccept = !JOY_NEW(B_BUTTON); sLeaving = TRUE;
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        }
        else if (JOY_NEW(A_BUTTON | DPAD_LEFT | DPAD_RIGHT))
        {
            bool32 left = JOY_NEW(DPAD_LEFT);
            if (sRow == 4)
            {
                sPicking = TRUE; sPickIndex = 0;
                for (u32 i = 0; i < ModsStarterCount(); ++i)
                    if (gModDraft.starter == RhhContentKeyForId(RHH_SPECIES, ModsStarterAt(i))) sPickIndex = i + 1;
            }
            else if (sRow == 2) gModDraft.wildMode = (gModDraft.wildMode + (left ? 2 : 1)) % 3;
            else if (sRow == 8) gModDraft.music = (gModDraft.music + (left ? 5 : 1)) % 6;
            else gModDraft.flags ^= sFlags[sRow];
        }
        else redraw = FALSE;
        if (redraw) { PlaySE(SE_SELECT); Draw(); }
    }
    RunTasks(); AnimateSprites(); BuildOamBuffer(); UpdatePaletteFade();
}
static void Init(void)
{
    SetVBlankCallback(NULL); ResetTasks(); ResetSpriteData(); FreeAllSpritePalettes(); ResetPaletteFade();
    SetGpuReg(REG_OFFSET_DISPCNT, 0); SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_WININ, 0); SetGpuReg(REG_OFFSET_WINOUT, 0);
    DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000);
    ResetBgsAndClearDma3BusyFlags(0); InitBgsFromTemplates(0, &sBg, 1);
    memset(sTilemap, 0, sizeof(sTilemap)); SetBgTilemapBuffer(0, sTilemap);
    ChangeBgX(0, 0, BG_COORD_SET); ChangeBgY(0, 0, BG_COORD_SET);
    InitWindows(sWindows); DeactivateAllTextPrinters();
    LoadUserWindowBorderGfx(0, 1, BG_PLTT_ID(13)); Menu_LoadStdPalAt(BG_PLTT_ID(14));
    const u16 backdrop = RGB(12, 17, 23); LoadPalette(&backdrop, 0, sizeof(backdrop));
    PutWindowTilemap(0); PutWindowTilemap(1);
    DrawTextBorderOuter(0, 1, 13); DrawTextBorderOuter(1, 1, 13);
    Draw(); CopyBgTilemapBufferToVram(0); ShowBg(0);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_OBJ_1D_MAP);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(VBlank); SetMainCallback2(Main);
}
void ModsOpenNewGameMenu(void (*resume)(void), void (*cancel)(void))
{
    gModDraft = (struct ModConfig){0}; sRow = sPickIndex = 0;
    sPicking = sHelp = sLeaving = sAccept = sEnded = FALSE;
    sResume = resume; sCancel = cancel; SetMainCallback2(Init);
}
void ModsOpenRunEndedMenu(void (*back)(void))
{
    sPicking = sHelp = sLeaving = sAccept = FALSE; sEnded = TRUE;
    sResume = sCancel = back; SetMainCallback2(Init);
}
#endif
