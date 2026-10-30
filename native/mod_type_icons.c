/* Native custom-type badges. Resources are shared by live sprites only and
 * reclaimed when those sprites stop using them or the screen resets. */
#include "global.h"
#include "fonts.h"
#include "mod_host.h"
#include "mods.h"
#include "sprite.h"
#include "constants/characters.h"
#include "constants/rgb.h"

enum { BADGE_COUNT = 12, BADGE_TAG_BASE = 0xfa00, LABEL_WIDTH = 256, BADGE_HEIGHT = 16 };
struct TypeBadge {
    RhhContentKey key;
    u32 rgb;
    char label[24];
    u16 tag, tile;
    u8 palette, compact;
    bool8 active;
};
static struct TypeBadge sBadges[BADGE_COUNT];
static const union AnimCmd sBadgeAnim[] = {ANIMCMD_FRAME(0, 0), ANIMCMD_END};
static const union AnimCmd *const sBadgeAnims[] = {sBadgeAnim};
struct TypeText { RhhContentKey key; char label[24]; u8 name[24], generic[17]; };
static struct TypeText sTypeText[RHH_RUNTIME_TYPE_COUNT];

static unsigned Glyph(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return CHAR_A + c - 'A';
    if (c >= 'a' && c <= 'z') return CHAR_a + c - 'a';
    if (c >= '0' && c <= '9') return CHAR_0 + c - '0';
    switch (c) {
    case ' ': return CHAR_SPACE; case '!': return CHAR_EXCL_MARK; case '.': return CHAR_PERIOD;
    case '-': return CHAR_HYPHEN; case ':': return CHAR_COLON; case ',': return CHAR_COMMA;
    case '/': return CHAR_SLASH; case '>': return CHAR_GREATER_THAN; case '<': return CHAR_LESS_THAN;
    case '+': return CHAR_PLUS; case '=': return CHAR_EQUALS; case '(': return CHAR_LEFT_PAREN;
    case ')': return CHAR_RIGHT_PAREN; case '%': return CHAR_PERCENT; case '\'': return CHAR_SGL_QUOTE_RIGHT;
    default: return CHAR_QUESTION_MARK;
    }
}
const u8 *ModsCustomTypeName(u32 type, bool32 generic)
{
    RhhContentKey key = RhhModsCustomTypeKey(type);
    if (!key) return NULL;
    const struct RhhTypeCatalogService *catalog = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/type_catalog", RHH_SERVICE_VERSION(1, 0));
    struct RhhTypeDefinition definition = {.struct_size = sizeof(definition)};
    if (!catalog || catalog->find(key, &definition)) return NULL;
    struct TypeText *text = &sTypeText[type - RHH_RUNTIME_TYPE_FIRST];
    if (text->key != key || strcmp(text->label, definition.label))
    {
        text->key = key; memcpy(text->label, definition.label, sizeof(text->label));
        u32 i;
        for (i = 0; i < sizeof(text->name) - 1 && definition.label[i]; ++i)
            text->name[i] = Glyph((unsigned char)definition.label[i]);
        text->name[i] = EOS;
        text->generic[0] = CHAR_a; text->generic[1] = CHAR_SPACE;
        for (i = 0; i < 9 && definition.label[i]; ++i)
            text->generic[i + 2] = Glyph((unsigned char)definition.label[i]);
        text->generic[i + 2] = CHAR_SPACE;
        text->generic[i + 3] = CHAR_m; text->generic[i + 4] = CHAR_o;
        text->generic[i + 5] = CHAR_v; text->generic[i + 6] = CHAR_e;
        text->generic[i + 7] = EOS;
    }
    return generic ? text->generic : text->name;
}
bool32 ModsCustomTypeColor(u32 type, u16 *out)
{
    RhhContentKey key = RhhModsCustomTypeKey(type);
    if (!key || !out) return FALSE;
    const struct RhhTypeCatalogService *catalog = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/type_catalog", RHH_SERVICE_VERSION(1, 0));
    struct RhhTypeDefinition definition = {.struct_size = sizeof(definition)};
    if (!catalog || catalog->find(key, &definition)) return FALSE;
    *out = RGB(((definition.rgb >> 16) & 255) / 8, ((definition.rgb >> 8) & 255) / 8,
               (definition.rgb & 255) / 8);
    return TRUE;
}
static void PutPixel(u8 *tiles, u32 width, u32 x, u32 y, u8 color)
{
    u32 tile = (y / 8) * (width / 8) + x / 8;
    u32 index = tile * 32 + (y % 8) * 4 + (x % 8) / 2;
    if (x & 1) tiles[index] = (tiles[index] & 0x0f) | (color << 4);
    else tiles[index] = (tiles[index] & 0xf0) | color;
}
static void BuildBadge(u8 *tiles, const char *label, bool32 compact)
{
    u32 width = compact ? 8 : 32;
    u8 text[BADGE_HEIGHT][LABEL_WIDTH] = {{0}};
    u32 textWidth = 0;
    memset(tiles, 0, width * BADGE_HEIGHT / 2);
    for (u32 y = 0; y < BADGE_HEIGHT; ++y)
        for (u32 x = 0; x < width; ++x)
        {
            if ((x == 0 || x == width - 1) && (y == 0 || y == BADGE_HEIGHT - 1)) continue;
            PutPixel(tiles, width, x, y, (y == 0 || y == BADGE_HEIGHT - 1 || x == 0 || x == width - 1) ? 1 : y == 1 ? 3 : 2);
        }
    for (u32 i = 0; label[i] && i < 23 && textWidth < LABEL_WIDTH - 16; ++i)
    {
        unsigned char c = label[i];
        if (compact && c == ' ' && !textWidth) continue;
        if (compact && c >= 'a' && c <= 'z') c -= 'a' - 'A';
        unsigned g = Glyph(c), gw = gFontSmallNarrowerLatinGlyphWidths[g];
        const u16 *font = gFontSmallNarrowerLatinGlyphs + 32 * g;
        for (u32 y = 0; y < 15; ++y)
            for (u32 x = 0; x < gw && x < 16; ++x)
            {
                u16 row = font[(y / 8) * 16 + (x / 8) * 8 + y % 8];
                unsigned pixel = (row >> (14 - (x % 8) * 2)) & 3;
                if (pixel == 1 || pixel == 2) text[y][textWidth + x] = pixel == 1 ? 4 : 5;
            }
        textWidth += gw + 1;
        if (compact) break;
    }
    if (!textWidth) return;
    u32 available = width - 2, drawn = textWidth < available ? textWidth : available;
    u32 left = (width - drawn) / 2;
    for (u32 y = 1; y < 15; ++y)
        for (u32 x = 0; x < drawn; ++x)
        {
            u32 sourceX = textWidth <= available ? x : (u64)x * textWidth / drawn;
            if (text[y][sourceX]) PutPixel(tiles, width, left + x, y, text[y][sourceX]);
        }
}
static void BuildPalette(u16 pal[16], u32 rgb)
{
    u32 r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
    memset(pal, 0, sizeof(u16) * 16);
    pal[1] = RGB(r / 16, g / 16, b / 16);
    pal[2] = RGB(r / 8, g / 8, b / 8);
    pal[3] = RGB((r + (255 - r) / 3) / 8, (g + (255 - g) / 3) / 8, (b + (255 - b) / 3) / 8);
    bool32 light = r * 299 + g * 587 + b * 114 >= 160000;
    pal[4] = light ? RGB(1, 1, 1) : RGB(31, 31, 31);
    pal[5] = light ? RGB(31, 31, 31) : RGB(1, 1, 1);
}
static bool32 Referenced(const struct TypeBadge *badge)
{
    for (u32 i = 0; i < MAX_SPRITES; ++i)
        if (gSprites[i].inUse && gSprites[i].anims == sBadgeAnims && gSprites[i].sheetTileStart == badge->tile)
            return TRUE;
    return FALSE;
}
void ModsReclaimCustomTypeIcons(void)
{
    for (u32 i = 0; i < BADGE_COUNT; ++i)
    {
        struct TypeBadge *badge = &sBadges[i];
        if (!badge->active || Referenced(badge)) continue;
        if (GetSpriteTileStartByTag(badge->tag) == badge->tile) FreeSpriteTilesByTag(badge->tag);
        FreeSpritePaletteSlotByTag(badge->palette, badge->tag);
        memset(badge, 0, sizeof(*badge));
    }
}
void ModsClearCustomTypeIcon(struct Sprite *sprite)
{
    if (!sprite || sprite->anims != sBadgeAnims) return;
    sprite->anims = sprite->template->anims;
    sprite->sheetTileStart = GetSpriteTileStartByTag(sprite->template->tileTag);
    sprite->oam.tileNum = sprite->sheetTileStart;
    u32 palette = IndexOfSpritePaletteTag(sprite->template->paletteTag);
    sprite->oam.paletteNum = palette < 16 ? palette : 15;
    sprite->animPaused = FALSE;
    ModsReclaimCustomTypeIcons();
}
static int FindPaletteSlot(void)
{
    /* These screens also load untagged fixed palettes in slots 12 through 15. */
    for (u32 i = gReservedSpritePaletteCount; i < 12; ++i)
    {
        if (GetSpritePaletteTagByPaletteNum(i) != TAG_NONE) continue;
        bool32 used = FALSE;
        for (u32 j = 0; j < MAX_SPRITES; ++j)
            if (gSprites[j].inUse && gSprites[j].oam.paletteNum == i) { used = TRUE; break; }
        if (!used) return i;
    }
    return -1;
}
static bool32 PaletteTagExists(u16 tag)
{
    for (u32 i = 0; i < 16; ++i)
        if (GetSpritePaletteTagByPaletteNum(i) == tag) return TRUE;
    return FALSE;
}
bool32 ModsSetCustomTypeIcon(struct Sprite *sprite, u32 type, bool32 compact)
{
    if (!sprite || !sprite->inUse || !sprite->usingSheet) return FALSE;
    RhhContentKey key = RhhModsCustomTypeKey(type);
    const struct RhhTypeCatalogService *catalog = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/type_catalog", RHH_SERVICE_VERSION(1, 0));
    struct RhhTypeDefinition definition = {.struct_size = sizeof(definition)};
    if (!key || !catalog || catalog->find(key, &definition)) return FALSE;
    if (sprite->anims == sBadgeAnims)
        for (u32 i = 0; i < BADGE_COUNT; ++i)
            if (sBadges[i].active && sBadges[i].tile == sprite->sheetTileStart
                && sBadges[i].key == key && sBadges[i].compact == !!compact
                && sBadges[i].rgb == definition.rgb && !strcmp(sBadges[i].label, definition.label)) return TRUE;
    ModsClearCustomTypeIcon(sprite);
    ModsReclaimCustomTypeIcons();
    struct TypeBadge *badge = NULL;
    for (u32 i = 0; i < BADGE_COUNT; ++i)
        if (sBadges[i].active && sBadges[i].key == key && sBadges[i].compact == !!compact
            && sBadges[i].rgb == definition.rgb && !strcmp(sBadges[i].label, definition.label))
        { badge = &sBadges[i]; break; }
    if (!badge)
    {
        int palette = FindPaletteSlot();
        if (palette < 0 || IndexOfSpriteTileTag(TAG_NONE) == 0xff || !CanAllocSpriteTiles(compact ? 2 : 8)) return FALSE;
        for (u32 i = 0; i < BADGE_COUNT; ++i)
        {
            u16 tag = BADGE_TAG_BASE + i;
            if (!sBadges[i].active && GetSpriteTileStartByTag(tag) == TAG_NONE && !PaletteTagExists(tag))
            { badge = &sBadges[i]; badge->tag = tag; break; }
        }
        if (!badge) return FALSE;
        u32 tiles[64]; u16 colors[16];
        BuildBadge((u8 *)tiles, definition.label, compact); BuildPalette(colors, definition.rgb);
        struct SpriteSheet sheet = {.data = tiles, .size = compact ? 64 : 256, .tag = badge->tag};
        u16 tile = LoadSpriteSheet(&sheet);
        if (GetSpriteTileStartByTag(badge->tag) != tile) { memset(badge, 0, sizeof(*badge)); return FALSE; }
        struct SpritePalette pal = {.data = colors, .tag = badge->tag};
        LoadSpritePaletteInSlot(&pal, palette);
        badge->key = key; badge->rgb = definition.rgb; memcpy(badge->label, definition.label, sizeof(badge->label));
        badge->tile = tile; badge->palette = palette; badge->compact = !!compact; badge->active = TRUE;
    }
    sprite->anims = sBadgeAnims; sprite->sheetTileStart = badge->tile;
    sprite->oam.tileNum = badge->tile; sprite->oam.paletteNum = badge->palette;
    sprite->animPaused = FALSE; StartSpriteAnim(sprite, 0);
    return TRUE;
}
