/* Translate live engine state into the public shader contract every frame. */
#include <stdlib.h>
#include <stdio.h>
#include "global.h"
#include "main.h"
#include "battle.h"
#include "battle_main.h"
#include "battle_environment.h"
#include "battle_util.h"
#include "battle_interface.h"
#include "sprite.h"
#include "random.h"
#include "palette.h"
#include "mods.h"
#include "mod_host.h"
#include "battle_shader.h"
#include "decompress.h"

/* These keys describe selected baseline assets, not the contents of a frame:
 * animations can temporarily alter or cover the background after loading. */
static const char *const sBackdropIds[BATTLE_ENVIRONMENT_COUNT] = {
    [BATTLE_ENVIRONMENT_GRASS] = "rhh:battle_backdrop/grass",
    [BATTLE_ENVIRONMENT_LONG_GRASS] = "rhh:battle_backdrop/long_grass",
    [BATTLE_ENVIRONMENT_SAND] = "rhh:battle_backdrop/sand",
    [BATTLE_ENVIRONMENT_UNDERWATER] = "rhh:battle_backdrop/underwater",
    [BATTLE_ENVIRONMENT_WATER] = "rhh:battle_backdrop/water",
    [BATTLE_ENVIRONMENT_POND] = "rhh:battle_backdrop/pond",
    [BATTLE_ENVIRONMENT_MOUNTAIN] = "rhh:battle_backdrop/mountain",
    [BATTLE_ENVIRONMENT_CAVE] = "rhh:battle_backdrop/cave",
    [BATTLE_ENVIRONMENT_BUILDING] = "rhh:battle_backdrop/building",
    [BATTLE_ENVIRONMENT_PLAIN] = "rhh:battle_backdrop/plain",
    [BATTLE_ENVIRONMENT_FRONTIER] = "rhh:battle_backdrop/frontier",
    [BATTLE_ENVIRONMENT_GYM] = "rhh:battle_backdrop/gym",
    [BATTLE_ENVIRONMENT_LEADER] = "rhh:battle_backdrop/leader",
    [BATTLE_ENVIRONMENT_MAGMA] = "rhh:battle_backdrop/magma",
    [BATTLE_ENVIRONMENT_AQUA] = "rhh:battle_backdrop/aqua",
    [BATTLE_ENVIRONMENT_SIDNEY] = "rhh:battle_backdrop/sidney",
    [BATTLE_ENVIRONMENT_PHOEBE] = "rhh:battle_backdrop/phoebe",
    [BATTLE_ENVIRONMENT_GLACIA] = "rhh:battle_backdrop/glacia",
    [BATTLE_ENVIRONMENT_DRAKE] = "rhh:battle_backdrop/drake",
    [BATTLE_ENVIRONMENT_CHAMPION] = "rhh:battle_backdrop/champion",
    [BATTLE_ENVIRONMENT_GROUDON] = "rhh:battle_backdrop/groudon",
    [BATTLE_ENVIRONMENT_KYOGRE] = "rhh:battle_backdrop/kyogre",
    [BATTLE_ENVIRONMENT_RAYQUAZA] = "rhh:battle_backdrop/rayquaza",
    [BATTLE_ENVIRONMENT_SOARING] = "rhh:battle_backdrop/soaring",
    [BATTLE_ENVIRONMENT_SKY_PILLAR] = "rhh:battle_backdrop/sky_pillar",
    [BATTLE_ENVIRONMENT_BURIAL_GROUND] = "rhh:battle_backdrop/burial_ground",
    [BATTLE_ENVIRONMENT_PUDDLE] = "rhh:battle_backdrop/puddle",
    [BATTLE_ENVIRONMENT_MARSH] = "rhh:battle_backdrop/marsh",
    [BATTLE_ENVIRONMENT_SWAMP] = "rhh:battle_backdrop/swamp",
    [BATTLE_ENVIRONMENT_SNOW] = "rhh:battle_backdrop/snow",
    [BATTLE_ENVIRONMENT_ICE] = "rhh:battle_backdrop/ice",
    [BATTLE_ENVIRONMENT_VOLCANO] = "rhh:battle_backdrop/volcano",
    [BATTLE_ENVIRONMENT_DISTORTION_WORLD] = "rhh:battle_backdrop/distortion_world",
    [BATTLE_ENVIRONMENT_SPACE] = "rhh:battle_backdrop/space",
    [BATTLE_ENVIRONMENT_ULTRA_SPACE] = "rhh:battle_backdrop/ultra_space",
};
static u32 sBackdrop, sBackdropPart, sBackdropPartEnvironment;
static bool32 sBackdropValid;
void ModsBattleBackdropReset(void)
{ sBackdropValid = FALSE; sBackdropPart = 0; }
void ModsBattleBackdropLoaded(u32 environment)
{
    sBackdropPart = 0; sBackdropValid = environment < ARRAY_COUNT(sBackdropIds) && sBackdropIds[environment]
        && gBattleEnvironmentInfo[environment].background.tileset && gBattleEnvironmentInfo[environment].background.tilemap
        && gBattleEnvironmentInfo[environment].palette;
    if (sBackdropValid) sBackdrop = environment;
}
void ModsBattleBackdropPart(u32 part, u32 environment)
{
    if (part == 3) { sBackdropValid = FALSE; sBackdropPart = 3; sBackdropPartEnvironment = environment; return; }
    if (environment != sBackdropPartEnvironment || part != sBackdropPart + 1)
    { ModsBattleBackdropReset(); return; }
    sBackdropPart = part;
    if (part == 5) ModsBattleBackdropLoaded(environment);
}
static u32 BattleBackdropCount(void) { return ARRAY_COUNT(sBackdropIds); }
static int BattleBackdropAt(u32 index, struct RhhBattleBackdropInfo *out)
{
    if (!out || out->struct_size < sizeof(*out) || index >= ARRAY_COUNT(sBackdropIds) || !sBackdropIds[index]) return -1;
    struct RhhBattleBackdropInfo result = {.struct_size = sizeof(result), .key = RhhContentKeyOf(sBackdropIds[index]),
        .has_artwork = !!(gBattleEnvironmentInfo[index].background.tileset && gBattleEnvironmentInfo[index].background.tilemap
            && gBattleEnvironmentInfo[index].palette)};
    snprintf(result.id, sizeof(result.id), "%s", sBackdropIds[index]);
    *out = result; return 0;
}
static int BattleBackdropCurrent(struct RhhBattleBackdropInfo *out)
{
    if (!sBackdropValid || !gMain.inBattle || !gBattleStruct || gMain.callback2 != BattleMainCB2) return -1;
    return BattleBackdropAt(sBackdrop, out);
}
/* All sources here are compiled-in engine assets. The map decoder only
 * accepts the expected 64x32 text map, and every pixel is validated before
 * touching the caller's buffer. */
static int BattleBackdropImage(RhhContentKey key, u32 x, u32 y, u32 width, u32 height, u8 *rgba, u32 capacity)
{
    if (!key || !rgba || !width || !height || x >= RHH_BATTLE_BACKDROP_WIDTH || y >= RHH_BATTLE_BACKDROP_HEIGHT
        || width > RHH_BATTLE_BACKDROP_WIDTH - x || height > RHH_BATTLE_BACKDROP_HEIGHT - y
        || capacity < width * height * 4u) return -1;
    u32 index = 0;
    while (index < ARRAY_COUNT(sBackdropIds) && (!sBackdropIds[index] || RhhContentKeyOf(sBackdropIds[index]) != key)) ++index;
    if (index == ARRAY_COUNT(sBackdropIds)) return -1;
    const struct BattleEnvironment *source = &gBattleEnvironmentInfo[index];
    if (!source->background.tileset || !source->background.tilemap || !source->palette) return -1;
    union CompressionHeader tileHeader, mapHeader;
    memcpy(&tileHeader, source->background.tileset, sizeof(tileHeader));
    memcpy(&mapHeader, source->background.tilemap, sizeof(mapHeader));
    u32 tileBytes = GetDecompressedDataSize(source->background.tileset);
    if (!tileBytes || tileBytes > MAX_DECOMPRESSION_BUFFER_SIZE || tileBytes % 32
        || tileHeader.smol.mode > ENCODE_BOTH_DELTA_SYMS
        || (tileHeader.smol.mode == MODE_LZ77 && *(const u8 *)source->background.tileset != 0x10)
        || (tileHeader.smol.mode != MODE_LZ77 && (!tileHeader.smol.loSize || !tileHeader.smol.symSize))
        || mapHeader.smol.mode != IS_TILEMAP || mapHeader.smolTilemap.tilemapSize != 4096) return -1;
    u8 *tiles = malloc(tileBytes);
    if (!tiles) return -1;
    u16 map[2048];
    if (!TryDecompressDataWithHeaderWram(source->background.tileset, tiles)) { free(tiles); return -1; }
    DecompressDataWithHeaderWram(source->background.tilemap, map);
    /* Check all references and palette banks before publishing any pixels. */
    for (u32 row = 0; row < height; ++row)
        for (u32 col = 0; col < width; ++col)
        {
            u32 px = x + col, py = y + row;
            u16 entry = map[(px / 256) * 1024 + (py / 8) * 32 + (px % 256) / 8];
            u32 tile = entry & 1023, tx = px & 7, ty = py & 7, bank = entry >> 12;
            if (entry & 0x400) tx = 7 - tx;
            if (entry & 0x800) ty = 7 - ty;
            if (tile >= tileBytes / 32) { free(tiles); return -1; }
            u8 pixel = (tiles[tile * 32 + ty * 4 + tx / 2] >> ((tx & 1) * 4)) & 15;
            if (pixel && (bank < 2 || bank > 4)) { free(tiles); return -1; }
        }
    for (u32 row = 0; row < height; ++row)
        for (u32 col = 0; col < width; ++col)
        {
            u32 px = x + col, py = y + row;
            u16 entry = map[(px / 256) * 1024 + (py / 8) * 32 + (px % 256) / 8];
            u32 tile = entry & 1023, tx = px & 7, ty = py & 7, bank = entry >> 12;
            if (entry & 0x400) tx = 7 - tx;
            if (entry & 0x800) ty = 7 - ty;
            u8 pixel = (tiles[tile * 32 + ty * 4 + tx / 2] >> ((tx & 1) * 4)) & 15;
            u8 *dest = rgba + (row * width + col) * 4;
            if (!pixel) { memset(dest, 0, 4); continue; }
            u16 color = ((const u16 *)source->palette)[(bank - 2) * 16 + pixel];
            for (u32 c = 0; c < 3; ++c) { u8 value = (color >> (5 * c)) & 31; dest[c] = (value << 3) | (value >> 2); }
            dest[3] = 255;
        }
    free(tiles); return 0;
}
static const struct RhhBattleBackdropService sBackdropService = {
    {sizeof(sBackdropService), RHH_SERVICE_VERSION(1, 1)}, BattleBackdropCount, BattleBackdropAt, BattleBackdropCurrent,
    BattleBackdropImage
};
const struct RhhServiceHeader *ModsBattleBackdropService(void) { return &sBackdropService.header; }

static unsigned Weather(void)
{
    /* The engine's unused downpour table entry aliases normal rain's flag. */
    if (gBattleWeather & B_WEATHER_RAIN_DOWNPOUR) return RHH_WEATHER_HEAVY_RAIN;
    switch (GetBattleWeather(gBattleWeather))
    {
    case BATTLE_WEATHER_RAIN: return RHH_WEATHER_RAIN;
    case BATTLE_WEATHER_RAIN_DOWNPOUR:
    case BATTLE_WEATHER_RAIN_PRIMAL: return RHH_WEATHER_HEAVY_RAIN;
    case BATTLE_WEATHER_SUN: return RHH_WEATHER_SUN;
    case BATTLE_WEATHER_SUN_PRIMAL: return RHH_WEATHER_HARSH_SUN;
    case BATTLE_WEATHER_SANDSTORM: return RHH_WEATHER_SAND;
    case BATTLE_WEATHER_HAIL: return RHH_WEATHER_HAIL;
    case BATTLE_WEATHER_SNOW: return RHH_WEATHER_SNOW;
    case BATTLE_WEATHER_FOG: return RHH_WEATHER_FOG;
    case BATTLE_WEATHER_STRONG_WINDS: return RHH_WEATHER_WIND;
    default: return RHH_WEATHER_NONE;
    }
}

static unsigned Terrain(void)
{
    switch (gFieldTimers.terrain)
    {
    case B_TERRAIN_GRASSY: return RHH_TERRAIN_GRASSY;
    case B_TERRAIN_MISTY: return RHH_TERRAIN_MISTY;
    case B_TERRAIN_ELECTRIC: return RHH_TERRAIN_ELECTRIC;
    case B_TERRAIN_PSYCHIC: return RHH_TERRAIN_PSYCHIC;
    default: return RHH_TERRAIN_NONE;
    }
}

void ModsBattleShaderUpdateUiPalettes(void)
{
    gNativeShaderUiPalettes = 0;
    if (!gMain.inBattle || !gBattleStruct || gMain.callback2 != BattleMainCB2) return;
    const u16 tags[] = {TAG_HEALTHBOX_PAL, TAG_HEALTHBAR_PAL, TAG_HEALTHBOX_PALS_1,
        TAG_HEALTHBOX_PALS_2, TAG_STATUS_SUMMARY_BAR_PAL, TAG_STATUS_SUMMARY_BALLS_PAL,
        TAG_GIMMICK_TRIGGER_PAL, TAG_MEGA_INDICATOR_PAL, TAG_MISC_INDICATOR_PAL, TAG_TERA_INDICATOR_PAL,
        TAG_ABILITY_POP_UP};
    for (unsigned i = 0; i < ARRAY_COUNT(tags); ++i)
    {
        unsigned palette = IndexOfSpritePaletteTag(tags[i]);
        if (palette < 16) gNativeShaderUiPalettes |= 1u << palette;
    }
}

struct NativeBattleEnvironment ModsBattleEnvironment(void)
{
    struct NativeBattleEnvironment env = {0};
    ModsBattleShaderUpdateUiPalettes();
    /* Party, bag, summary, evolution and battle setup use other callbacks.
     * Their graphics must never inherit an active battle's environment. */
    if (!gMain.inBattle || !gBattleStruct || gMain.callback2 != BattleMainCB2) return env;
    env.battle_scene = 1;
    env.weather = Weather(); env.terrain = Terrain();
    env.suppressed = !HasWeatherEffect();
    env.doubles = !!(gBattleTypeFlags & BATTLE_TYPE_DOUBLE);
    env.visibility = (REG_DISPCNT & DISPCNT_FORCED_BLANK) ? 0.f : 1.f - gPaletteFade.y / 16.f;
    struct RhhModEvent e = {.struct_size = sizeof(e), .kind = RHH_BATTLE_ENVIRONMENT,
        .run_seed = ModsGetConfig().seed, .args = {env.weather, env.terrain, env.suppressed, 1}};
    struct RhhModDecision d = {.struct_size = sizeof(d), .allowed = 1};
    RhhModsDispatch(&e, &d);
    if (d.allowed && (env.weather || env.terrain)) env.shader = d.value;
    return env;
}

unsigned ModsCollectBattleRenderPasses(const struct NativeBattleEnvironment *env, unsigned frame,
                                       struct RhhBattleRenderPass *out)
{
    if (!env || !env->battle_scene || !out) return 0;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    unsigned count = RhhModsCollectBattlePasses(frame, env->weather, env->terrain, env->suppressed, env->doubles, out);
    bool32 pure = !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2));
    gRngValue = rng; gRng2Value = rng2;
    return pure ? count : 0;
}

void ModsSetBattleEnvironment(unsigned enabled)
{
    const struct RhhModValue v = {RHH_SETTING_BOOL, 0, !!enabled};
    RhhModsConfigure("rhh:battle_environment", "enabled", &v);
}
unsigned ModsBattleEnvironmentEnabled(void)
{
    struct RhhModValue v;
    return !RhhModsGetSetting("rhh:battle_environment", "enabled", &v) && v.value;
}
int ModsShowWeatherRecap(void)
{
    struct NativeBattleEnvironment env = ModsBattleEnvironment();
    /* Keep the original notification if no usable visual can replace it. */
    struct RhhModEvent e = {.struct_size = sizeof(e), .kind = RHH_BATTLE_ENVIRONMENT_NOTICE,
        .args = {RHH_NOTICE_WEATHER_RECAP, env.weather && NativeShaderReady(env.shader)}};
    struct RhhModDecision d = {.struct_size = sizeof(d), .allowed = 1};
    RhhModsDispatch(&e, &d);
    return d.allowed;
}
