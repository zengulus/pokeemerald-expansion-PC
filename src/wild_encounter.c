#include "global.h"
#ifdef NATIVE
#include "mods.h"
#include "battle_frontier.h"
#endif
#include "battle_setup.h"
#include "battle_pike.h"
#include "battle_pyramid.h"
#include "event_data.h"
#include "fieldmap.h"
#include "fishing.h"
#include "follower_npc.h"
#include "item.h"
#include "random.h"
#include "field_player_avatar.h"
#include "link.h"
#include "mass_outbreak.h"
#include "metatile_behavior.h"
#include "overworld.h"
#include "ow_abilities.h"
#include "pokeblock.h"
#include "pokemon.h"
#include "random.h"
#include "roamer.h"
#include "safari_zone.h"
#include "script.h"
#include "tv.h"
#include "wild_encounter.h"
#include "battle_debug.h"
#include "battle_pike.h"
#include "battle_pyramid.h"
#include "constants/abilities.h"
#include "constants/game_stat.h"
#include "constants/item.h"
#include "constants/items.h"
#include "constants/layouts.h"
#include "constants/weather.h"

extern const u8 EventScript_SprayWoreOff[];

#define MAX_ENCOUNTER_RATE 2880
#ifdef NATIVE
_Static_assert(MAX_ENCOUNTER_RATE == RHH_WILD_RATE_MAX, "public wild encounter rate scale differs");
#define WILD_RATE_CHECK(rate, ignore, area, source, time) WildEncounterCheck(rate, ignore, area, source, time)
#else
#define WILD_RATE_CHECK(rate, ignore, area, source, time) WildEncounterCheck(rate, ignore)
#endif

#define NUM_FEEBAS_SPOTS 6

// Number of accessible fishing spots in each section of Route 119
// Each section is an area of the route between the y coordinates in sRoute119WaterTileData
#define NUM_FISHING_SPOTS_1 131
#define NUM_FISHING_SPOTS_2 167
#define NUM_FISHING_SPOTS_3 149
#define NUM_FISHING_SPOTS (NUM_FISHING_SPOTS_1 + NUM_FISHING_SPOTS_2 + NUM_FISHING_SPOTS_3)

static u16 FeebasRandom(void);
static void FeebasSeedRng(u16 seed);
static void ApplyFluteEncounterRateMod(u32 *encRate);
static void ApplyCleanseTagEncounterRateMod(u32 *encRate);
static u8 GetMaxLevelOfSpeciesInWildTable(const struct WildPokemon *wildMon, enum Species species, enum WildPokemonArea area);
#ifdef BUGFIX
static bool8 TryGetAbilityInfluencedWildMonIndex(const struct WildPokemon *wildMon, enum Type type, enum Ability ability, u8 *monIndex, u32 size);
#else
static bool8 TryGetAbilityInfluencedWildMonIndex(const struct WildPokemon *wildMon, enum Type type, enum Ability ability, u8 *monIndex);
#endif

EWRAM_DATA static u8 sWildEncountersDisabled = 0;
EWRAM_DATA static u32 sFeebasRngValue = 0;
EWRAM_DATA bool8 gIsFishingEncounter = 0;
EWRAM_DATA bool8 gIsSurfingEncounter = 0;
EWRAM_DATA u8 gChainFishingDexNavStreak = 0;

#include "data/wild_encounters.h"

const struct WildPokemon gWildFeebas = {20, 25, SPECIES_FEEBAS};

static const u16 sRoute119WaterTileData[] =
{
//yMin, yMax, numSpots in previous sections
     0,  45,  0,
    46,  91,  NUM_FISHING_SPOTS_1,
    92, 139,  NUM_FISHING_SPOTS_1 + NUM_FISHING_SPOTS_2,
};

// Each fishing spot on Route 119 is given a number between 1 and NUM_FISHING_SPOTS inclusive.
// The number is determined by counting the valid fishing spots left to right top to bottom.
// The map is divided into three sections, with each section having a pre-counted number of
// fishing spots to start from to avoid counting a large number of spots at the bottom of the map.
// Note that a spot is considered valid if it is surfable and not a waterfall. To exclude all
// of the inaccessible water metatiles (so that they can't be selected as a Feebas spot) they
// use a different metatile that isn't actually surfable because it has MB_NORMAL instead.
// This function is given the coordinates and section of a fishing spot and returns which number it is.
static u16 GetFeebasFishingSpotId(s16 targetX, s16 targetY, u8 section)
{
    u16 x, y;
    u16 yMin = sRoute119WaterTileData[section * 3 + 0];
    u16 yMax = sRoute119WaterTileData[section * 3 + 1];
    u16 spotId = sRoute119WaterTileData[section * 3 + 2];

    for (y = yMin; y <= yMax; y++)
    {
        for (x = 0; x < gMapHeader.mapLayout->width; x++)
        {
            u8 behavior = MapGridGetMetatileBehaviorAt(x + MAP_OFFSET, y + MAP_OFFSET);
            if (MetatileBehavior_IsSurfableAndNotWaterfall(behavior) == TRUE)
            {
                spotId++;
                if (targetX == x && targetY == y)
                    return spotId;
            }
        }
    }
    return spotId + 1;
}

bool8 CheckFeebasAtCoords(s16 x, s16 y)
{
    u8 i;
    u16 feebasSpots[NUM_FEEBAS_SPOTS];
    u8 route119Section = 0;
    u16 spotId;

    if (gSaveBlock1Ptr->location.mapGroup == MAP_GROUP(MAP_ROUTE119)
     && gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_ROUTE119))
    {
        x -= MAP_OFFSET;
        y -= MAP_OFFSET;

        // Get which third of the map the player is in
        if (y >= sRoute119WaterTileData[3 * 0 + 0] && y <= sRoute119WaterTileData[3 * 0 + 1])
            route119Section = 0;
        if (y >= sRoute119WaterTileData[3 * 1 + 0] && y <= sRoute119WaterTileData[3 * 1 + 1])
            route119Section = 1;
        if (y >= sRoute119WaterTileData[3 * 2 + 0] && y <= sRoute119WaterTileData[3 * 2 + 1])
            route119Section = 2;

        // 50% chance of encountering Feebas (assuming this is a Feebas spot)
        if (Random() % 100 > 49)
            return FALSE;

        FeebasSeedRng(gSaveBlock1Ptr->dewfordTrends[0].rand);

        // Assign each Feebas spot to a random fishing spot.
        // Randomness is fixed depending on the seed above.
        for (i = 0; i != NUM_FEEBAS_SPOTS;)
        {
            feebasSpots[i] = FeebasRandom() % NUM_FISHING_SPOTS;
            if (feebasSpots[i] == 0)
                feebasSpots[i] = NUM_FISHING_SPOTS;

            // < 1 below is a pointless check, it will never be TRUE.
            // >= 4 to skip fishing spots 1-3, because these are inaccessible
            // spots at the top of the map, at (9,7), (7,13), and (15,16).
            // The first accessible fishing spot is spot 4 at (18,18).
            if (feebasSpots[i] < 1 || feebasSpots[i] >= 4)
                i++;
        }

        // Check which fishing spot the player is at, and see if
        // it matches any of the Feebas spots.
        spotId = GetFeebasFishingSpotId(x, y, route119Section);
        for (i = 0; i < NUM_FEEBAS_SPOTS; i++)
        {
            if (spotId == feebasSpots[i])
                return TRUE;
        }
    }
    return FALSE;
}

static u16 FeebasRandom(void)
{
    sFeebasRngValue = ISO_RANDOMIZE2(sFeebasRngValue);
    return sFeebasRngValue >> 16;
}

static void FeebasSeedRng(u16 seed)
{
    sFeebasRngValue = seed;
}

// NUM_LAND_MONS_ENCOUNTER_SLOTS
u32 ChooseWildMonIndex_Land(void)
{
    u8 wildMonIndex = 0;
    bool8 swap = FALSE;
    u8 rand = Random() % ENCOUNTER_CHANCE_LAND_MONS_TOTAL;

    if (rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_0)
        wildMonIndex = 0;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_0 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_1)
        wildMonIndex = 1;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_1 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_2)
        wildMonIndex = 2;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_2 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_3)
        wildMonIndex = 3;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_3 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_4)
        wildMonIndex = 4;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_4 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_5)
        wildMonIndex = 5;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_5 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_6)
        wildMonIndex = 6;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_6 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_7)
        wildMonIndex = 7;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_7 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_8)
        wildMonIndex = 8;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_8 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_9)
        wildMonIndex = 9;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_9 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_10)
        wildMonIndex = 10;
    else
        wildMonIndex = 11;

    if (LURE_STEP_COUNT != 0 && (Random() % 10 < 2))
        swap = TRUE;

    if (swap)
        wildMonIndex = 11 - wildMonIndex;

    return wildMonIndex;
}

// Mostly equivalent to ChooseWildMonIndex_Land
// NUM_LAND_MONS_ENCOUNTER_SLOTS
u8 GetLandEncounterSlotForMatchCall(void)
{
    int rand = Random() % ENCOUNTER_CHANCE_LAND_MONS_TOTAL;

    if (rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_0)
        return 0;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_0 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_1)
        return 1;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_1 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_2)
        return 2;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_2 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_3)
        return 3;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_3 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_4)
        return 4;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_4 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_5)
        return 5;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_5 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_6)
        return 6;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_6 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_7)
        return 7;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_7 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_8)
        return 8;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_8 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_9)
        return 9;
    else if (rand >= ENCOUNTER_CHANCE_LAND_MONS_SLOT_9 && rand < ENCOUNTER_CHANCE_LAND_MONS_SLOT_10)
        return 10;
    else
        return 11;
}

// NUM_WATER_MONS_ENCOUNTER_SLOTS
u32 ChooseWildMonIndex_Water(void)
{
    u32 wildMonIndex = 0;
    bool8 swap = FALSE;
    u8 rand = Random() % ENCOUNTER_CHANCE_WATER_MONS_TOTAL;

    if (rand < ENCOUNTER_CHANCE_WATER_MONS_SLOT_0)
        wildMonIndex = 0;
    else if (rand >= ENCOUNTER_CHANCE_WATER_MONS_SLOT_0 && rand < ENCOUNTER_CHANCE_WATER_MONS_SLOT_1)
        wildMonIndex = 1;
    else if (rand >= ENCOUNTER_CHANCE_WATER_MONS_SLOT_1 && rand < ENCOUNTER_CHANCE_WATER_MONS_SLOT_2)
        wildMonIndex = 2;
    else if (rand >= ENCOUNTER_CHANCE_WATER_MONS_SLOT_2 && rand < ENCOUNTER_CHANCE_WATER_MONS_SLOT_3)
        wildMonIndex = 3;
    else
        wildMonIndex = 4;

    if (LURE_STEP_COUNT != 0 && (Random() % 10 < 2))
        swap = TRUE;

    if (swap)
        wildMonIndex = 4 - wildMonIndex;

    return wildMonIndex;
}

// Mostly equivalent to ChooseWildMonIndex_WaterRock
// NUM_WATER_MONS_ENCOUNTER_SLOTS
u8 GetWaterEncounterSlotForMatchCall(void)
{
    int rand = Random() % ENCOUNTER_CHANCE_WATER_MONS_TOTAL;

    if (rand < ENCOUNTER_CHANCE_WATER_MONS_SLOT_0)
        return 0;
    else if (rand >= ENCOUNTER_CHANCE_WATER_MONS_SLOT_0 && rand < ENCOUNTER_CHANCE_WATER_MONS_SLOT_1)
        return 1;
    else if (rand >= ENCOUNTER_CHANCE_WATER_MONS_SLOT_1 && rand < ENCOUNTER_CHANCE_WATER_MONS_SLOT_2)
        return 2;
    else if (rand >= ENCOUNTER_CHANCE_WATER_MONS_SLOT_2 && rand < ENCOUNTER_CHANCE_WATER_MONS_SLOT_3)
        return 3;
    else
        return 4;
}


// NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS
u32 ChooseWildMonIndex_Rocks(void)
{
    u32 wildMonIndex = 0;
    bool8 swap = FALSE;
    u8 rand = Random() % ENCOUNTER_CHANCE_ROCK_SMASH_MONS_TOTAL;

    if (rand < ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_0)
        wildMonIndex = 0;
    else if (rand >= ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_0 && rand < ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_1)
        wildMonIndex = 1;
    else if (rand >= ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_1 && rand < ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_2)
        wildMonIndex = 2;
    else if (rand >= ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_2 && rand < ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_3)
        wildMonIndex = 3;
    else
        wildMonIndex = 4;

    if (LURE_STEP_COUNT != 0 && (Random() % 10 < 2))
        swap = TRUE;

    if (swap)
        wildMonIndex = 4 - wildMonIndex;

    return wildMonIndex;
}

// NUM_FISHING_MONS_ENCOUNTER_SLOTS
static u32 ChooseWildMonIndex_Fishing(u8 rod)
{
    u8 wildMonIndex = 0;
    bool8 swap = FALSE;
    u8 rand = Random() % max(max(ENCOUNTER_CHANCE_FISHING_MONS_OLD_ROD_TOTAL, ENCOUNTER_CHANCE_FISHING_MONS_GOOD_ROD_TOTAL),
                             ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_TOTAL);

    if (LURE_STEP_COUNT != 0 && (Random() % 10 < 2))
        swap = TRUE;

    switch (rod)
    {
    case OLD_ROD:
        if (rand < ENCOUNTER_CHANCE_FISHING_MONS_OLD_ROD_SLOT_0)
            wildMonIndex = 0;
        else
            wildMonIndex = 1;

        if (swap)
            wildMonIndex = 1 - wildMonIndex;
        break;
    case GOOD_ROD:
        if (rand < ENCOUNTER_CHANCE_FISHING_MONS_GOOD_ROD_SLOT_2)
            wildMonIndex = 2;
        if (rand >= ENCOUNTER_CHANCE_FISHING_MONS_GOOD_ROD_SLOT_2 && rand < ENCOUNTER_CHANCE_FISHING_MONS_GOOD_ROD_SLOT_3)
            wildMonIndex = 3;
        if (rand >= ENCOUNTER_CHANCE_FISHING_MONS_GOOD_ROD_SLOT_3 && rand < ENCOUNTER_CHANCE_FISHING_MONS_GOOD_ROD_SLOT_4)
            wildMonIndex = 4;

        if (swap)
            wildMonIndex = 6 - wildMonIndex;
        break;
    case SUPER_ROD:
        if (rand < ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_5)
            wildMonIndex = 5;
        if (rand >= ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_5 && rand < ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_6)
            wildMonIndex = 6;
        if (rand >= ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_6 && rand < ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_7)
            wildMonIndex = 7;
        if (rand >= ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_7 && rand < ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_8)
            wildMonIndex = 8;
        if (rand >= ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_8 && rand < ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_9)
            wildMonIndex = 9;

        if (swap)
            wildMonIndex = 14 - wildMonIndex;
        break;
    }
    return wildMonIndex;
}

u8 ChooseWildMonLevel(const struct WildPokemon *wildPokemon, u8 wildMonIndex, enum WildPokemonArea area)
{
    u8 min;
    u8 max;
    u8 range;
    u8 rand;

    if (LURE_STEP_COUNT == 0)
    {
        // Make sure minimum level is less than maximum level
        if (wildPokemon[wildMonIndex].maxLevel >= wildPokemon[wildMonIndex].minLevel)
        {
            min = wildPokemon[wildMonIndex].minLevel;
            max = wildPokemon[wildMonIndex].maxLevel;
        }
        else
        {
            min = wildPokemon[wildMonIndex].maxLevel;
            max = wildPokemon[wildMonIndex].minLevel;
        }
        range = max - min + 1;
        rand = Random() % range;

        // check ability for max level mon
        if (!GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SANITY_IS_EGG))
        {
            enum Ability ability = GetMonAbility(&gParties[B_TRAINER_PLAYER][0]);
            if (ability == ABILITY_HUSTLE || ability == ABILITY_VITAL_SPIRIT || ability == ABILITY_PRESSURE)
            {
                if (Random() % 2 == 0)
                    return max;

                if (rand != 0)
                    rand--;
            }
        }
        return min + rand;
    }
    else
    {
        // Looks for the max level of all slots that share the same species as the selected slot.
        max = GetMaxLevelOfSpeciesInWildTable(wildPokemon, wildPokemon[wildMonIndex].species, area);
        if (max > 0)
            return max + 1;
        else // Failsafe
            return wildPokemon[wildMonIndex].maxLevel + 1;
    }
}

#ifdef NATIVE
u16 ModsWildHeaderForMap(u8 group, u8 number, u32 *variant, bool32 *clamped)
{
    if (variant) *variant = 0;
    if (clamped) *clamped = FALSE;
    for (u16 i = 0; ; ++i)
    {
        const struct WildPokemonHeader *wildHeader = &gWildMonHeaders[i];
        if (wildHeader->mapGroup == MAP_GROUP(MAP_UNDEFINED)) break;
        if (wildHeader->mapGroup == group && wildHeader->mapNum == number)
        {
            if (group == MAP_GROUP(MAP_ALTERING_CAVE) && number == MAP_NUM(MAP_ALTERING_CAVE))
            {
                u16 alteringCaveId = VarGet(VAR_ALTERING_CAVE_WILD_SET);
                if (alteringCaveId >= NUM_ALTERING_CAVE_TABLES)
                { alteringCaveId = 0; if (clamped) *clamped = TRUE; }
                if (variant) *variant = alteringCaveId;
                i += alteringCaveId;
            }
            return i;
        }
    }
    return HEADER_NONE;
}
u16 GetCurrentMapWildMonHeaderId(void)
{ return ModsWildHeaderForMap(gSaveBlock1Ptr->location.mapGroup, gSaveBlock1Ptr->location.mapNum, NULL, NULL); }
#else
u16 GetCurrentMapWildMonHeaderId(void)
{
    u16 i;
    for (i = 0; ; i++)
    {
        const struct WildPokemonHeader *wildHeader = &gWildMonHeaders[i];
        if (wildHeader->mapGroup == MAP_GROUP(MAP_UNDEFINED)) break;
        if (wildHeader->mapGroup == gSaveBlock1Ptr->location.mapGroup && wildHeader->mapNum == gSaveBlock1Ptr->location.mapNum)
        {
            if (gSaveBlock1Ptr->location.mapGroup == MAP_GROUP(MAP_ALTERING_CAVE)
                && gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_ALTERING_CAVE))
            {
                u16 alteringCaveId = VarGet(VAR_ALTERING_CAVE_WILD_SET);
                if (alteringCaveId >= NUM_ALTERING_CAVE_TABLES) alteringCaveId = 0;
                i += alteringCaveId;
            }
            return i;
        }
    }
    return HEADER_NONE;
}
#endif

#ifdef NATIVE
enum TimeOfDay ModsWildTimePolicy(enum TimeOfDay requested, enum TimeOfDay mapped, bool32 present,
                                  bool32 enabled, bool32 disableFallback, bool32 facility,
                                  enum TimeOfDay fallback, u32 *flags)
{
    u32 resultFlags = 0; enum TimeOfDay result;
    if (!enabled) { resultFlags |= RHH_AUTHORED_WILD_TIME_DISABLED; result = TIME_OF_DAY_DEFAULT; }
    else if (facility || (!present && !disableFallback))
    { resultFlags |= RHH_AUTHORED_WILD_TIME_FALLBACK; result = fallback; }
    else { result = mapped; if (result != requested) resultFlags |= RHH_AUTHORED_WILD_TIME_REMAPPED; }
    if (flags) *flags = resultFlags;
    return result;
}
enum TimeOfDay ModsWildResolveTime(const struct WildPokemonHeader *header, enum WildPokemonArea area,
                                   enum TimeOfDay requested, bool32 facility, u32 *flags)
{
    bool32 present = FALSE;
    if (OW_TIME_OF_DAY_ENCOUNTERS && !facility)
    {
        const struct WildEncounterTypes *types = &header->encounterTypes[requested];
        const struct WildPokemonInfo *info = area == WILD_AREA_WATER ? types->waterMonsInfo
            : area == WILD_AREA_ROCKS ? types->rockSmashMonsInfo : area == WILD_AREA_FISHING ? types->fishingMonsInfo
            : area == WILD_AREA_HIDDEN ? types->hiddenMonsInfo : types->landMonsInfo;
        present = info != NULL;
    }
    return ModsWildTimePolicy(requested, GenConfigTimeOfDay(requested), present, OW_TIME_OF_DAY_ENCOUNTERS,
                              OW_TIME_OF_DAY_DISABLE_FALLBACK, facility, OW_TIME_OF_DAY_FALLBACK, flags);
}
enum TimeOfDay GetTimeOfDayForEncounters(u32 headerId, enum WildPokemonArea area)
{
    enum TimeOfDay timeOfDay = GetTimeOfDay();
    if (!OW_TIME_OF_DAY_ENCOUNTERS) return TIME_OF_DAY_DEFAULT;
    bool32 facility = InBattlePike() || CurrentBattlePyramidLocation() != PYRAMID_LOCATION_NONE;
    return ModsWildResolveTime(&gWildMonHeaders[headerId], area, timeOfDay, facility, NULL);
}
#else
enum TimeOfDay GetTimeOfDayForEncounters(u32 headerId, enum WildPokemonArea area)
{
    const struct WildPokemonInfo *wildMonInfo;
    enum TimeOfDay timeOfDay = GetTimeOfDay();

    if (!OW_TIME_OF_DAY_ENCOUNTERS)
        return TIME_OF_DAY_DEFAULT;

    if (InBattlePike() || CurrentBattlePyramidLocation() != PYRAMID_LOCATION_NONE)
        return OW_TIME_OF_DAY_FALLBACK;

    switch (area)
    {
    default:
    case WILD_AREA_LAND:
        wildMonInfo = gWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo;
        break;
    case WILD_AREA_WATER:
        wildMonInfo = gWildMonHeaders[headerId].encounterTypes[timeOfDay].waterMonsInfo;
        break;
    case WILD_AREA_ROCKS:
        wildMonInfo = gWildMonHeaders[headerId].encounterTypes[timeOfDay].rockSmashMonsInfo;
        break;
    case WILD_AREA_FISHING:
        wildMonInfo = gWildMonHeaders[headerId].encounterTypes[timeOfDay].fishingMonsInfo;
        break;
    case WILD_AREA_HIDDEN:
        wildMonInfo = gWildMonHeaders[headerId].encounterTypes[timeOfDay].hiddenMonsInfo;
        break;
    }

    if (wildMonInfo == NULL && !OW_TIME_OF_DAY_DISABLE_FALLBACK)
        return OW_TIME_OF_DAY_FALLBACK;
    else
        return GenConfigTimeOfDay(timeOfDay);
}
#endif

static u8 PickWildMonNature(enum Species species)
{
    u8 i;
    struct Pokeblock *safariPokeblock;
    u8 natures[NUM_NATURES];

    if (GetSafariZoneFlag() == TRUE && Random() % 100 < 80)
    {
        safariPokeblock = SafariZoneGetActivePokeblock();
        if (safariPokeblock != NULL)
        {
            for (i = 0; i < NUM_NATURES; i++)
                natures[i] = i;
            Shuffle(natures, NUM_NATURES, sizeof(natures[0]));
            for (i = 0; i < NUM_NATURES; i++)
            {
                if (PokeblockGetGain(natures[i], safariPokeblock) > 0)
                    return natures[i];
            }
        }
    }

    return GetSynchronizedNature(WILDMON_ORIGIN, species);
}

void CreateWildMon(enum Species species, u8 level)
{
    ZeroEnemyPartyMons();
    u32 personality = GetMonPersonality(species, GetSynchronizedGender(WILDMON_ORIGIN, species), PickWildMonNature(species), RANDOM_UNOWN_LETTER);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], species, level, personality, OTID_STRUCT_PLAYER_ID, USE_RANDOM_IVS);
    GiveMonInitialMoveset(&gParties[B_TRAINER_OPPONENT_A][0]);
}

#ifdef BUGFIX
#define TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildPokemon, type, ability, ptr, count) TryGetAbilityInfluencedWildMonIndex(wildPokemon, type, ability, ptr, count)
#else
#define TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildPokemon, type, ability, ptr, count) TryGetAbilityInfluencedWildMonIndex(wildPokemon, type, ability, ptr)
#endif

#ifdef NATIVE
bool32 ModsWildBaseSlotWeights(u32 area, u32 rod, u16 *weights, u32 *firstOut, u32 *countOut)
{
    static const u16 land[] = {ENCOUNTER_CHANCE_LAND_MONS_SLOT_0, ENCOUNTER_CHANCE_LAND_MONS_SLOT_1,
        ENCOUNTER_CHANCE_LAND_MONS_SLOT_2, ENCOUNTER_CHANCE_LAND_MONS_SLOT_3, ENCOUNTER_CHANCE_LAND_MONS_SLOT_4,
        ENCOUNTER_CHANCE_LAND_MONS_SLOT_5, ENCOUNTER_CHANCE_LAND_MONS_SLOT_6, ENCOUNTER_CHANCE_LAND_MONS_SLOT_7,
        ENCOUNTER_CHANCE_LAND_MONS_SLOT_8, ENCOUNTER_CHANCE_LAND_MONS_SLOT_9, ENCOUNTER_CHANCE_LAND_MONS_SLOT_10,
        ENCOUNTER_CHANCE_LAND_MONS_SLOT_11};
    static const u16 water[] = {ENCOUNTER_CHANCE_WATER_MONS_SLOT_0, ENCOUNTER_CHANCE_WATER_MONS_SLOT_1,
        ENCOUNTER_CHANCE_WATER_MONS_SLOT_2, ENCOUNTER_CHANCE_WATER_MONS_SLOT_3, ENCOUNTER_CHANCE_WATER_MONS_SLOT_4};
    static const u16 rocks[] = {ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_0, ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_1,
        ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_2, ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_3, ENCOUNTER_CHANCE_ROCK_SMASH_MONS_SLOT_4};
    static const u16 oldRod[] = {ENCOUNTER_CHANCE_FISHING_MONS_OLD_ROD_SLOT_0, ENCOUNTER_CHANCE_FISHING_MONS_OLD_ROD_SLOT_1};
    static const u16 goodRod[] = {ENCOUNTER_CHANCE_FISHING_MONS_GOOD_ROD_SLOT_2, ENCOUNTER_CHANCE_FISHING_MONS_GOOD_ROD_SLOT_3,
        ENCOUNTER_CHANCE_FISHING_MONS_GOOD_ROD_SLOT_4};
    static const u16 superRod[] = {ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_5, ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_6,
        ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_7, ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_8,
        ENCOUNTER_CHANCE_FISHING_MONS_SUPER_ROD_SLOT_9};
#ifdef ENCOUNTER_CHANCE_HIDDEN_MONS_TOTAL
    static const u16 hidden[] = {ENCOUNTER_CHANCE_HIDDEN_MONS_SLOT_0, ENCOUNTER_CHANCE_HIDDEN_MONS_SLOT_1,
        ENCOUNTER_CHANCE_HIDDEN_MONS_SLOT_2};
#endif
    const u16 *cumulative; u32 first = 0, count;
    if (!weights || !firstOut || !countOut || (area != WILD_AREA_FISHING && rod != RHH_WILD_ROD_NONE)) return FALSE;
    switch (area)
    {
    case WILD_AREA_LAND: cumulative = land; count = ARRAY_COUNT(land); break;
    case WILD_AREA_WATER: cumulative = water; count = ARRAY_COUNT(water); break;
    case WILD_AREA_ROCKS: cumulative = rocks; count = ARRAY_COUNT(rocks); break;
    case WILD_AREA_FISHING:
        if (rod == RHH_WILD_ROD_OLD) { cumulative = oldRod; count = ARRAY_COUNT(oldRod); }
        else if (rod == RHH_WILD_ROD_GOOD) { cumulative = goodRod; count = ARRAY_COUNT(goodRod); first = 2; }
        else if (rod == RHH_WILD_ROD_SUPER) { cumulative = superRod; count = ARRAY_COUNT(superRod); first = 5; }
        else return FALSE;
        break;
    case WILD_AREA_HIDDEN:
#ifdef ENCOUNTER_CHANCE_HIDDEN_MONS_TOTAL
        cumulative = hidden; count = ARRAY_COUNT(hidden); break;
#else
        *firstOut = 0; *countOut = 0; return TRUE;
#endif
    default: return FALSE;
    }
    for (u32 i = 0; i < count; ++i) weights[i] = cumulative[i] - (i ? cumulative[i - 1] : 0);
    *firstOut = first; *countOut = count; return TRUE;
}
/* Preserve the original chooser and RNG tape when no rule changes weights. */
static bool32 TryWeightedWildSlot(const struct WildPokemon *table, const struct WildPokemon *candidates,
                                  u32 area, u32 source, u32 rod, u32 time, s32 x, s32 y, u8 *slot)
{
    u32 first, count;
    u16 base[NUM_LAND_MONS_ENCOUNTER_SLOTS], weights[NUM_LAND_MONS_ENCOUNTER_SLOTS];
    if (!ModsWildBaseSlotWeights(area, rod, base, &first, &count) || !count) return FALSE;
    if (!ModsWildWeights(table, candidates, area, source, rod, time, x, y, first, count, base, weights)) return FALSE;
    u32 total = 0; for (u32 i = 0; i < count; ++i) total += weights[i];
    u32 selected = RandomWeightedArrayDefault(RNG_NONE, total, count, weights);
    if (LURE_STEP_COUNT && Random() % 10 < 2) selected = count - 1 - selected;
    *slot = first + selected; return TRUE;
}
#endif

static bool8 TryGenerateWildMonImpl(const struct WildPokemonInfo *wildMonInfo, enum WildPokemonArea area, u8 flags,
                                    s32 x, s32 y, u32 source, bool8 *cancelled)
{
    if (cancelled) *cancelled = FALSE;
#ifdef NATIVE
    rng_value_t savedRng = gRngValue, savedRng2 = gRng2Value;
#endif
    u8 wildMonIndex = 0;
    u8 level;
#ifdef NATIVE
    const struct WildPokemonInfo *originalWildMonInfo = wildMonInfo;
    /* Apply the stable pool before ability-based slot selection, so Static,
     * Magnet Pull and similar abilities inspect the actual randomized table. */
    struct WildPokemonInfo modInfo;
    struct WildPokemon modMons[NUM_LAND_MONS_ENCOUNTER_SLOTS];
    u32 modHeader = GetCurrentMapWildMonHeaderId();
    u32 modTime = modHeader == HEADER_NONE ? 0 : GetTimeOfDayForEncounters(modHeader, area);
    /* Facility tables are internal selectors, not the final encounter. In
     * particular Pyramid species 1..12 are indices into a private table. */
    bool32 facility = gMapHeader.mapLayoutId == LAYOUT_BATTLE_FRONTIER_BATTLE_PIKE_ROOM_WILD_MONS
        || gMapHeader.mapLayoutId == LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR;
    bool32 modPool = !facility && ModsGetConfig().wildMode == MOD_WILD_POOL;
    if (modPool)
    {
        static const u8 counts[] = {NUM_LAND_MONS_ENCOUNTER_SLOTS, NUM_WATER_MONS_ENCOUNTER_SLOTS,
            NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS, NUM_FISHING_MONS_ENCOUNTER_SLOTS, NUM_HIDDEN_MONS_ENCOUNTER_SLOTS};
        modInfo = *wildMonInfo;
        for (u32 i = 0; i < counts[area]; ++i)
        {
            modMons[i] = wildMonInfo->wildPokemon[i];
            modMons[i].species = ModsWildSpecies(modMons[i].species, area, i, modTime);
        }
        modInfo.wildPokemon = modMons; wildMonInfo = &modInfo;
    }
#else
    (void)x; (void)y; (void)source;
#endif

    switch (area)
    {
    case WILD_AREA_LAND:
        if (TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_STEEL, ABILITY_MAGNET_PULL, &wildMonIndex, NUM_LAND_MONS_ENCOUNTER_SLOTS))
            break;
        if (TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_ELECTRIC, ABILITY_STATIC, &wildMonIndex, NUM_LAND_MONS_ENCOUNTER_SLOTS))
            break;
        if (OW_LIGHTNING_ROD >= GEN_8 && TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_ELECTRIC, ABILITY_LIGHTNING_ROD, &wildMonIndex, NUM_LAND_MONS_ENCOUNTER_SLOTS))
            break;
        if (OW_FLASH_FIRE >= GEN_8 && TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_FIRE, ABILITY_FLASH_FIRE, &wildMonIndex, NUM_LAND_MONS_ENCOUNTER_SLOTS))
            break;
        if (OW_HARVEST >= GEN_8 && TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_GRASS, ABILITY_HARVEST, &wildMonIndex, NUM_LAND_MONS_ENCOUNTER_SLOTS))
            break;
        if (OW_STORM_DRAIN >= GEN_8 && TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_WATER, ABILITY_STORM_DRAIN, &wildMonIndex, NUM_LAND_MONS_ENCOUNTER_SLOTS))
            break;

#ifdef NATIVE
        if (facility || !TryWeightedWildSlot(originalWildMonInfo->wildPokemon, wildMonInfo->wildPokemon, area, source,
                                 RHH_WILD_ROD_NONE, modHeader == HEADER_NONE ? UINT32_MAX : modTime, x, y, &wildMonIndex))
#endif
            wildMonIndex = ChooseWildMonIndex_Land();
        break;
    case WILD_AREA_WATER:
        if (TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_STEEL, ABILITY_MAGNET_PULL, &wildMonIndex, NUM_WATER_MONS_ENCOUNTER_SLOTS))
            break;
        if (TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_ELECTRIC, ABILITY_STATIC, &wildMonIndex, NUM_WATER_MONS_ENCOUNTER_SLOTS))
            break;
        if (OW_LIGHTNING_ROD >= GEN_8 && TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_ELECTRIC, ABILITY_LIGHTNING_ROD, &wildMonIndex, NUM_WATER_MONS_ENCOUNTER_SLOTS))
            break;
        if (OW_FLASH_FIRE >= GEN_8 && TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_FIRE, ABILITY_FLASH_FIRE, &wildMonIndex, NUM_WATER_MONS_ENCOUNTER_SLOTS))
            break;
        if (OW_HARVEST >= GEN_8 && TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_GRASS, ABILITY_HARVEST, &wildMonIndex, NUM_WATER_MONS_ENCOUNTER_SLOTS))
            break;
        if (OW_STORM_DRAIN >= GEN_8 && TRY_GET_ABILITY_INFLUENCED_WILD_MON_INDEX(wildMonInfo->wildPokemon, TYPE_WATER, ABILITY_STORM_DRAIN, &wildMonIndex, NUM_WATER_MONS_ENCOUNTER_SLOTS))
            break;

#ifdef NATIVE
        if (facility || !TryWeightedWildSlot(originalWildMonInfo->wildPokemon, wildMonInfo->wildPokemon, area, source,
                                 RHH_WILD_ROD_NONE, modHeader == HEADER_NONE ? UINT32_MAX : modTime, x, y, &wildMonIndex))
#endif
            wildMonIndex = ChooseWildMonIndex_Water();
        break;
    case WILD_AREA_ROCKS:
#ifdef NATIVE
        if (facility || !TryWeightedWildSlot(originalWildMonInfo->wildPokemon, wildMonInfo->wildPokemon, area, source,
                                 RHH_WILD_ROD_NONE, modHeader == HEADER_NONE ? UINT32_MAX : modTime, x, y, &wildMonIndex))
#endif
            wildMonIndex = ChooseWildMonIndex_Rocks();
        break;
    default:
    case WILD_AREA_FISHING:
    case WILD_AREA_HIDDEN:
        break;
    }

    level = ChooseWildMonLevel(wildMonInfo->wildPokemon, wildMonIndex, area);
    if (flags & WILD_CHECK_REPEL && !IsWildLevelAllowedByRepel(level))
        return FALSE;
    if (gMapHeader.mapLayoutId != LAYOUT_BATTLE_FRONTIER_BATTLE_PIKE_ROOM_WILD_MONS && flags & WILD_CHECK_KEEN_EYE && !IsAbilityAllowingEncounter(level))
        return FALSE;

    enum Species species = wildMonInfo->wildPokemon[wildMonIndex].species;
#ifdef NATIVE
    struct RhhWildEncounterContext wildContext = {0}; struct RhhWildEncounterCancellation cancellation;
    if (!facility)
    {
        if (!modPool) species = ModsWildSpecies(species, area, wildMonIndex, modTime);
        u32 resolvedSpecies = species;
        if (!ModsWildResolve(originalWildMonInfo->wildPokemon[wildMonIndex].species, &resolvedSpecies, area, source,
                             wildMonIndex, level, modHeader == HEADER_NONE ? UINT32_MAX : modTime, x, y,
                             &wildContext, &cancellation))
        {
            gRngValue = savedRng; gRng2Value = savedRng2;
            if (cancelled) *cancelled = TRUE;
            ModsWildCancelled(&cancellation); return FALSE;
        }
        species = resolvedSpecies;
    }
#endif
    CreateWildMon(species, level);
#ifdef NATIVE
    if (!facility)
    {
        ModsWildCreated(species, area, wildMonIndex, modTime);
        ModsWildCandidateCreated(&wildContext, &gParties[B_TRAINER_OPPONENT_A][0]);
    }
#endif
    return TRUE;
}

bool8 TryGenerateWildMon(const struct WildPokemonInfo *wildMonInfo, enum WildPokemonArea area, u8 flags)
{
#ifdef NATIVE
    s16 x = -1, y = -1;
    if (gPlayerAvatar.objectEventId < OBJECT_EVENTS_COUNT && gObjectEvents[gPlayerAvatar.objectEventId].active)
    { PlayerGetDestCoords(&x, &y); x -= MAP_OFFSET; y -= MAP_OFFSET; }
    return TryGenerateWildMonImpl(wildMonInfo, area, flags, x, y, RHH_WILD_STANDARD, NULL);
#else
    return TryGenerateWildMonImpl(wildMonInfo, area, flags, 0, 0, 0, NULL);
#endif
}
#ifdef NATIVE
bool8 TryGenerateWildMonAt(const struct WildPokemonInfo *wildMonInfo, enum WildPokemonArea area, u8 flags, s32 x, s32 y)
{ return TryGenerateWildMonImpl(wildMonInfo, area, flags, x, y, RHH_WILD_OVERWORLD_ACTOR, NULL); }
bool8 TryGenerateWildMonAtStatus(const struct WildPokemonInfo *wildMonInfo, enum WildPokemonArea area, u8 flags,
                                 s32 x, s32 y, bool8 *cancelled)
{ return TryGenerateWildMonImpl(wildMonInfo, area, flags, x, y, RHH_WILD_OVERWORLD_ACTOR, cancelled); }
#endif

static u16 GenerateFishingWildMon(const struct WildPokemonInfo *wildMonInfo, u8 rod, s16 x, s16 y, u32 tableTime
#ifdef NATIVE
                                  , struct RhhWildEncounterCancellation *cancellation
#endif
                                  )
{
    u8 wildMonIndex;
#ifdef NATIVE
    if (!TryWeightedWildSlot(wildMonInfo->wildPokemon, wildMonInfo->wildPokemon, WILD_AREA_FISHING, RHH_WILD_FISHING_ROD,
                             rod + RHH_WILD_ROD_OLD, tableTime, x - MAP_OFFSET, y - MAP_OFFSET, &wildMonIndex))
#else
    (void)tableTime;
#endif
        wildMonIndex = ChooseWildMonIndex_Fishing(rod);
    enum Species wildMonSpecies = wildMonInfo->wildPokemon[wildMonIndex].species;
    u8 level = ChooseWildMonLevel(wildMonInfo->wildPokemon, wildMonIndex, WILD_AREA_FISHING);

#ifdef NATIVE
    u32 header = GetCurrentMapWildMonHeaderId();
    enum Species originalSpecies = wildMonSpecies;
    u32 modTime = header == HEADER_NONE ? 0 : GetTimeOfDayForEncounters(header, WILD_AREA_FISHING);
    wildMonSpecies = ModsWildSpecies(wildMonSpecies, WILD_AREA_FISHING, wildMonIndex,
        modTime);
    struct RhhWildEncounterContext wildContext; u32 resolvedSpecies = wildMonSpecies;
    if (!ModsWildResolve(originalSpecies, &resolvedSpecies, WILD_AREA_FISHING, RHH_WILD_FISHING_ROD,
        wildMonIndex, level, header == HEADER_NONE ? UINT32_MAX : modTime, x - MAP_OFFSET, y - MAP_OFFSET,
        &wildContext, cancellation)) return SPECIES_NONE;
    wildMonSpecies = resolvedSpecies;
#else
    (void)x; (void)y;
#endif
    UpdateChainFishingStreak();
    CreateWildMon(wildMonSpecies, level);
#ifdef NATIVE
    ModsWildCreated(wildMonSpecies, WILD_AREA_FISHING, wildMonIndex, modTime);
    ModsWildCandidateCreated(&wildContext, &gParties[B_TRAINER_OPPONENT_A][0]);
#endif
    return wildMonSpecies;
}

static bool8 EncounterOddsCheck(u16 encounterRate)
{
    if (Random() % MAX_ENCOUNTER_RATE < encounterRate)
        return TRUE;
    else
        return FALSE;
}

// Returns true if it will try to create a wild encounter.
static bool8 WildEncounterCheck(u32 encounterRate, bool8 ignoreAbility
#ifdef NATIVE
                                , u32 area, u32 source, u32 time
#endif
                                )
{
#ifdef NATIVE
    u32 tableRate = encounterRate;
#endif
    encounterRate *= 16;
    if (TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_MACH_BIKE | PLAYER_AVATAR_FLAG_ACRO_BIKE))
        encounterRate = encounterRate * 80 / 100;
    ApplyFluteEncounterRateMod(&encounterRate);
    ApplyCleanseTagEncounterRateMod(&encounterRate);
    if (LURE_STEP_COUNT != 0)
        encounterRate *= 2;
    if (!ignoreAbility && !GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SANITY_IS_EGG))
    {
        enum Ability ability = GetMonAbility(&gParties[B_TRAINER_PLAYER][0]);

        if (ability == ABILITY_STENCH && gMapHeader.mapLayoutId == LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR)
            encounterRate = encounterRate * 3 / 4;
        else if (ability == ABILITY_STENCH)
            encounterRate /= 2;
        else if (ability == ABILITY_ILLUMINATE)
            encounterRate *= 2;
        else if (ability == ABILITY_WHITE_SMOKE)
            encounterRate /= 2;
        else if (ability == ABILITY_ARENA_TRAP)
            encounterRate *= 2;
        else if (ability == ABILITY_SAND_VEIL && gSaveBlock1Ptr->weather == WEATHER_SANDSTORM)
            encounterRate /= 2;
        else if (ability == ABILITY_SNOW_CLOAK && gSaveBlock1Ptr->weather == WEATHER_SNOW)
            encounterRate /= 2;
        else if (ability == ABILITY_QUICK_FEET)
            encounterRate /= 2;
        else if (ability == ABILITY_INFILTRATOR && OW_INFILTRATOR >= GEN_8)
            encounterRate /= 2;
        else if (ability == ABILITY_NO_GUARD)
            encounterRate *= 2;
    }
    if (encounterRate > MAX_ENCOUNTER_RATE)
        encounterRate = MAX_ENCOUNTER_RATE;
#ifdef NATIVE
    s32 x = -1, y = -1;
    if (gPlayerAvatar.objectEventId < OBJECT_EVENTS_COUNT && gObjectEvents[gPlayerAvatar.objectEventId].active)
    { s16 px, py; PlayerGetDestCoords(&px, &py); x = px - MAP_OFFSET; y = py - MAP_OFFSET; }
    encounterRate = ModsWildEncounterRate(tableRate, encounterRate, area, source, time, x, y, ignoreAbility);
#endif
    return EncounterOddsCheck(encounterRate);
}

// When you first step on a different type of metatile, there's a 40% chance it
// skips the wild encounter check entirely.
static bool8 AllowWildCheckOnNewMetatile(void)
{
    if (Random() % 100 >= 60)
        return FALSE;
    else
        return TRUE;
}

bool8 AreLegendariesInSootopolisPreventingEncounters(void)
{
    if (gSaveBlock1Ptr->location.mapGroup != MAP_GROUP(MAP_SOOTOPOLIS_CITY)
     || gSaveBlock1Ptr->location.mapNum != MAP_NUM(MAP_SOOTOPOLIS_CITY))
    {
        return FALSE;
    }

    return FlagGet(FLAG_LEGENDARIES_IN_SOOTOPOLIS);
}

#ifdef NATIVE
enum { SPECIAL_WILD_NONE, SPECIAL_WILD_READY, SPECIAL_WILD_DENIED, SPECIAL_WILD_FILTERED };
static void SpecialWildPlayerCoords(s32 *x, s32 *y)
{
    *x = *y = -1;
    if (gPlayerAvatar.objectEventId < OBJECT_EVENTS_COUNT && gObjectEvents[gPlayerAvatar.objectEventId].active)
    { s16 px, py; PlayerGetDestCoords(&px, &py); *x = px - MAP_OFFSET; *y = py - MAP_OFFSET; }
}
/* Facility selection uses a provisional Pokémon, then rewrites it into the
 * actual opponent. Gate only that final Pokémon and restore the preselection
 * checkpoint before notifying a veto. */
bool32 TryGenerateFacilityWild(const struct WildPokemonInfo *table, u32 flags, bool32 pike,
                               bool32 checkPikeAbility, u32 time, s32 x, s32 y,
                               bool32 overworld, bool8 *cancelled)
{
    if (cancelled) *cancelled = FALSE;
    struct RhhWildCheckpoint checkpoint; ModsWildCheckpointSave(&checkpoint);
    const struct BattleFrontierTrainer *trainers = gFacilityTrainers;
    const struct TrainerMon *trainerMons = gFacilityTrainerMons;
    if (!TryGenerateWildMon(table, WILD_AREA_LAND, flags)) return FALSE;
    if (pike)
    {
        if (!TryGenerateBattlePikeWildMon(checkPikeAbility)) return FALSE;
    }
    else GenerateBattlePyramidWildMon(SPECIES_NONE);
    struct Pokemon *mon = &gParties[B_TRAINER_OPPONENT_A][0];
    u32 species = GetMonData(mon, MON_DATA_SPECIES), level = GetMonData(mon, MON_DATA_LEVEL);
    u64 identity = (u64)GetMonData(mon, MON_DATA_OT_ID) << 32 | GetMonData(mon, MON_DATA_PERSONALITY);
    struct RhhWildEncounterContext context; struct RhhWildEncounterCancellation cancel;
    u32 source = overworld ? (pike ? RHH_WILD_OVERWORLD_PIKE : RHH_WILD_OVERWORLD_PYRAMID)
        : (pike ? RHH_WILD_PIKE : RHH_WILD_PYRAMID);
    if (!ModsWildSpecialPermission(species, WILD_AREA_LAND, source,
                                   UINT32_MAX, level, time, x, y, identity, &context, &cancel))
    {
        ModsWildCheckpointRestore(&checkpoint);
        gFacilityTrainers = trainers; gFacilityTrainerMons = trainerMons;
        if (cancelled) *cancelled = TRUE;
        ModsWildCancelled(&cancel); return FALSE;
    }
    ModsWildSpecialCreated(&context, mon); return TRUE;
}
static bool32 TryGenerateFacilityAtPlayer(const struct WildPokemonInfo *table, u32 flags, bool32 pike,
                                           bool32 checkPikeAbility, u32 time)
{
    s32 x, y; SpecialWildPlayerCoords(&x, &y);
    return TryGenerateFacilityWild(table, flags, pike, checkPikeAbility, time, x, y, FALSE, NULL);
}
static u32 TryRoamerSpecial(u32 area, u32 time, bool32 checkRepel)
{
    struct RhhWildCheckpoint checkpoint; ModsWildCheckpointSave(&checkpoint);
    if (!TryStartRoamerEncounter()) return SPECIAL_WILD_NONE;
    struct Roamer *roamer = &gSaveBlock1Ptr->roamer[gEncounteredRoamerIndex];
    if (checkRepel && !IsWildLevelAllowedByRepel(roamer->level)) return SPECIAL_WILD_FILTERED;
    struct Pokemon *mon = &gParties[B_TRAINER_OPPONENT_A][0];
    u64 identity = (u64)GetMonData(mon, MON_DATA_OT_ID) << 32 | GetMonData(mon, MON_DATA_PERSONALITY);
    s32 x, y; SpecialWildPlayerCoords(&x, &y);
    struct RhhWildEncounterContext context; struct RhhWildEncounterCancellation cancel;
    if (!ModsWildSpecialPermission(roamer->species, area, RHH_WILD_ROAMER, gEncounteredRoamerIndex,
                                   roamer->level, time, x, y, identity, &context, &cancel))
    { ModsWildCheckpointRestore(&checkpoint); ModsWildCancelled(&cancel); return SPECIAL_WILD_DENIED; }
    ModsWildSpecialCreated(&context, mon); return SPECIAL_WILD_READY;
}
static u32 GenerateOutbreakSpecial(u32 flags, u32 time, const struct RhhWildCheckpoint *checkpoint)
{
    u32 species = gSaveBlock1Ptr->outbreakPokemonSpecies, level = gSaveBlock1Ptr->outbreakPokemonLevel;
    if ((flags & WILD_CHECK_REPEL) && !IsWildLevelAllowedByRepel(level)) return SPECIAL_WILD_FILTERED;
    s32 x, y; SpecialWildPlayerCoords(&x, &y);
    struct RhhWildEncounterContext context; struct RhhWildEncounterCancellation cancel;
    if (!ModsWildSpecialPermission(species, WILD_AREA_LAND, RHH_WILD_OUTBREAK, UINT32_MAX,
                                   level, time, x, y, 0, &context, &cancel))
    { ModsWildCheckpointRestore(checkpoint); ModsWildCancelled(&cancel); return SPECIAL_WILD_DENIED; }
    if (!SetUpMassOutbreakEncounter(flags)) return SPECIAL_WILD_FILTERED;
    ModsWildSpecialCreated(&context, &gParties[B_TRAINER_OPPONENT_A][0]); return SPECIAL_WILD_READY;
}
#endif
bool8 StandardWildEncounter(u16 curMetatileBehavior, u16 prevMetatileBehavior)
{
    u32 headerId;
    enum TimeOfDay timeOfDay;
    struct Roamer *roamer;

    if (sWildEncountersDisabled == TRUE)
        return FALSE;

    headerId = GetCurrentMapWildMonHeaderId();
    if (headerId == HEADER_NONE)
    {
        if (gMapHeader.mapLayoutId == LAYOUT_BATTLE_FRONTIER_BATTLE_PIKE_ROOM_WILD_MONS)
        {
            headerId = GetBattlePikeWildMonHeaderId();
            timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_LAND);

            if (prevMetatileBehavior != curMetatileBehavior && !AllowWildCheckOnNewMetatile())
                return FALSE;
            else if (WILD_RATE_CHECK(gBattlePikeWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo->encounterRate, FALSE, WILD_AREA_LAND, RHH_WILD_PIKE, timeOfDay) != TRUE)
                return FALSE;
#ifdef NATIVE
            else if (!TryGenerateFacilityAtPlayer(gBattlePikeWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo,
                                               WILD_CHECK_KEEN_EYE, TRUE, TRUE, timeOfDay))
                return FALSE;
#else
            else if (TryGenerateWildMon(gBattlePikeWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo, WILD_AREA_LAND, WILD_CHECK_KEEN_EYE) != TRUE)
                return FALSE;
            else if (!TryGenerateBattlePikeWildMon(TRUE))
                return FALSE;
#endif

            BattleSetup_StartBattlePikeWildBattle();
            return TRUE;
        }
        if (gMapHeader.mapLayoutId == LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR)
        {
            headerId = gSaveBlock2Ptr->frontier.curChallengeBattleNum;
            timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_LAND);

            if (prevMetatileBehavior != curMetatileBehavior && !AllowWildCheckOnNewMetatile())
                return FALSE;
            else if (WILD_RATE_CHECK(gBattlePyramidWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo->encounterRate, FALSE, WILD_AREA_LAND, RHH_WILD_PYRAMID, timeOfDay) != TRUE)
                return FALSE;
#ifdef NATIVE
            else if (!TryGenerateFacilityAtPlayer(gBattlePyramidWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo,
                                               WILD_CHECK_KEEN_EYE, FALSE, FALSE, timeOfDay))
                return FALSE;
#else
            else if (TryGenerateWildMon(gBattlePyramidWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo, WILD_AREA_LAND, WILD_CHECK_KEEN_EYE) != TRUE)
                return FALSE;

            GenerateBattlePyramidWildMon(SPECIES_NONE);
#endif
            BattleSetup_StartWildBattle();
            return TRUE;
        }
    }
    else
    {
        if (MetatileBehavior_IsLandWildEncounter(curMetatileBehavior) == TRUE)
        {
            timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_LAND);

            if (gWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo == NULL)
                return FALSE;
            else if (prevMetatileBehavior != curMetatileBehavior && !AllowWildCheckOnNewMetatile())
                return FALSE;
            else if (WILD_RATE_CHECK(gWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo->encounterRate, FALSE, WILD_AREA_LAND, RHH_WILD_STANDARD, timeOfDay) != TRUE)
                return FALSE;

#ifdef NATIVE
            u32 roamerResult = TryRoamerSpecial(WILD_AREA_LAND, timeOfDay, TRUE);
            if (roamerResult == SPECIAL_WILD_DENIED || roamerResult == SPECIAL_WILD_FILTERED) return FALSE;
            if (roamerResult == SPECIAL_WILD_READY)
            { BattleSetup_StartRoamerBattle(); return TRUE; }
            {
                struct RhhWildCheckpoint checkpoint; ModsWildCheckpointSave(&checkpoint);
                if (DoMassOutbreakEncounterTest())
                {
                    u32 outbreak = GenerateOutbreakSpecial(WILD_CHECK_REPEL | WILD_CHECK_KEEN_EYE, timeOfDay, &checkpoint);
                    if (outbreak == SPECIAL_WILD_DENIED) return FALSE;
                    if (outbreak == SPECIAL_WILD_READY) { BattleSetup_StartWildBattle(); return TRUE; }
                }
#else
            if (TryStartRoamerEncounter())
            {
                roamer = &gSaveBlock1Ptr->roamer[gEncounteredRoamerIndex];
                if (!IsWildLevelAllowedByRepel(roamer->level)) return FALSE;
                BattleSetup_StartRoamerBattle(); return TRUE;
            }
            else
            {
                if (DoMassOutbreakEncounterTest() == TRUE && SetUpMassOutbreakEncounter(WILD_CHECK_REPEL | WILD_CHECK_KEEN_EYE) == TRUE)
                { BattleSetup_StartWildBattle(); return TRUE; }
#endif
                // try a regular wild land encounter
                if (TryGenerateWildMon(gWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo, WILD_AREA_LAND, WILD_CHECK_REPEL | WILD_CHECK_KEEN_EYE) == TRUE)
                {
                    if (TryDoDoubleWildBattle())
                    {
                        struct Pokemon mon1 = gParties[B_TRAINER_OPPONENT_A][0];
                        if (TryGenerateWildMon(gWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo, WILD_AREA_LAND, WILD_CHECK_KEEN_EYE))
                        {
                            gParties[B_TRAINER_OPPONENT_A][1] = mon1;
                            BattleSetup_StartDoubleWildBattle();
                        }
                        else BattleSetup_StartWildBattle();
                    }
                    else
                    {
                        BattleSetup_StartWildBattle();
                    }
                    return TRUE;
                }

                return FALSE;
            }
        }
        else if (MetatileBehavior_IsWaterWildEncounter(curMetatileBehavior) == TRUE
                 || (TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING) && MetatileBehavior_IsBridgeOverWater(curMetatileBehavior) == TRUE))
        {
            timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_WATER);

            if (AreLegendariesInSootopolisPreventingEncounters() == TRUE)
                return FALSE;
            else if (gWildMonHeaders[headerId].encounterTypes[timeOfDay].waterMonsInfo == NULL)
                return FALSE;
            else if (prevMetatileBehavior != curMetatileBehavior && !AllowWildCheckOnNewMetatile())
                return FALSE;
            else if (WILD_RATE_CHECK(gWildMonHeaders[headerId].encounterTypes[timeOfDay].waterMonsInfo->encounterRate, FALSE, WILD_AREA_WATER, RHH_WILD_STANDARD, timeOfDay) != TRUE)
                return FALSE;

#ifdef NATIVE
            u32 roamerResult = TryRoamerSpecial(WILD_AREA_WATER, timeOfDay, TRUE);
            if (roamerResult == SPECIAL_WILD_DENIED || roamerResult == SPECIAL_WILD_FILTERED) return FALSE;
            if (roamerResult == SPECIAL_WILD_READY) { BattleSetup_StartRoamerBattle(); return TRUE; }
            { // try a regular surfing encounter
#else
            if (TryStartRoamerEncounter())
            {
                roamer = &gSaveBlock1Ptr->roamer[gEncounteredRoamerIndex];
                if (!IsWildLevelAllowedByRepel(roamer->level)) return FALSE;
                BattleSetup_StartRoamerBattle(); return TRUE;
            }
            else // try a regular surfing encounter
            {
#endif
                if (TryGenerateWildMon(gWildMonHeaders[headerId].encounterTypes[timeOfDay].waterMonsInfo, WILD_AREA_WATER, WILD_CHECK_REPEL | WILD_CHECK_KEEN_EYE) == TRUE)
                {
                    gIsSurfingEncounter = TRUE;
                    if (TryDoDoubleWildBattle())
                    {
                        struct Pokemon mon1 = gParties[B_TRAINER_OPPONENT_A][0];
                        if (TryGenerateWildMon(gWildMonHeaders[headerId].encounterTypes[timeOfDay].waterMonsInfo, WILD_AREA_WATER, WILD_CHECK_KEEN_EYE))
                        {
                            gParties[B_TRAINER_OPPONENT_A][1] = mon1;
                            BattleSetup_StartDoubleWildBattle();
                        }
                        else BattleSetup_StartWildBattle();
                    }
                    else
                    {
                        BattleSetup_StartWildBattle();
                    }
                    return TRUE;
                }

                return FALSE;
            }
        }
    }

    return FALSE;
}

void RockSmashWildEncounter(void)
{
    u32 headerId = GetCurrentMapWildMonHeaderId();
    enum TimeOfDay timeOfDay;

    if (headerId != HEADER_NONE)
    {
        timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_ROCKS);

        const struct WildPokemonInfo *wildPokemonInfo = gWildMonHeaders[headerId].encounterTypes[timeOfDay].rockSmashMonsInfo;

        if (wildPokemonInfo == NULL)
        {
            gSpecialVar_Result = FALSE;
        }
        else if (WILD_RATE_CHECK(wildPokemonInfo->encounterRate, TRUE, WILD_AREA_ROCKS, RHH_WILD_STANDARD, timeOfDay) == TRUE
         && TryGenerateWildMon(wildPokemonInfo, WILD_AREA_ROCKS, WILD_CHECK_REPEL | WILD_CHECK_KEEN_EYE) == TRUE)
        {
            if (TryDoDoubleWildBattle())
            {
                struct Pokemon mon1 = gParties[B_TRAINER_OPPONENT_A][0];
                if (TryGenerateWildMon(wildPokemonInfo, WILD_AREA_ROCKS, WILD_CHECK_REPEL | WILD_CHECK_KEEN_EYE))
                {
                    gParties[B_TRAINER_OPPONENT_A][1] = mon1;
                    BattleSetup_StartDoubleWildBattle();
                }
                else BattleSetup_StartWildBattle();
                gSpecialVar_Result = TRUE;
            }
            else {
                BattleSetup_StartWildBattle();
                gSpecialVar_Result = TRUE;
            }
        }
        else
        {
            gSpecialVar_Result = FALSE;
        }
    }
    else
    {
        gSpecialVar_Result = FALSE;
    }
}

bool8 SweetScentWildEncounter(void)
{
    s16 x, y;
    u32 headerId;
    enum TimeOfDay timeOfDay;

    PlayerGetDestCoords(&x, &y);
    headerId = GetCurrentMapWildMonHeaderId();
    if (headerId == HEADER_NONE)
    {
        if (gMapHeader.mapLayoutId == LAYOUT_BATTLE_FRONTIER_BATTLE_PIKE_ROOM_WILD_MONS)
        {
            headerId = GetBattlePikeWildMonHeaderId();
            timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_LAND);

#ifdef NATIVE
            if (!TryGenerateFacilityAtPlayer(gBattlePikeWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo,
                                         0, TRUE, FALSE, timeOfDay)) return FALSE;
#else
            if (TryGenerateWildMon(gBattlePikeWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo, WILD_AREA_LAND, 0) != TRUE)
                return FALSE;

            TryGenerateBattlePikeWildMon(FALSE);
#endif
            BattleSetup_StartBattlePikeWildBattle();
            return TRUE;
        }
        if (gMapHeader.mapLayoutId == LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR)
        {
            headerId = gSaveBlock2Ptr->frontier.curChallengeBattleNum;
            timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_LAND);

#ifdef NATIVE
            if (!TryGenerateFacilityAtPlayer(gBattlePyramidWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo,
                                         0, FALSE, FALSE, timeOfDay)) return FALSE;
#else
            if (TryGenerateWildMon(gBattlePyramidWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo, WILD_AREA_LAND, 0) != TRUE)
                return FALSE;

            GenerateBattlePyramidWildMon(SPECIES_NONE);
#endif
            BattleSetup_StartWildBattle();
            return TRUE;
        }
    }
    else
    {
        if (MetatileBehavior_IsLandWildEncounter(MapGridGetMetatileBehaviorAt(x, y)) == TRUE)
        {
            timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_LAND);

            if (gWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo == NULL)
                return FALSE;

#ifdef NATIVE
            u32 roamerResult = TryRoamerSpecial(WILD_AREA_LAND, timeOfDay, FALSE);
            if (roamerResult == SPECIAL_WILD_DENIED) return FALSE;
            if (roamerResult == SPECIAL_WILD_READY) { BattleSetup_StartRoamerBattle(); return TRUE; }
            struct RhhWildCheckpoint checkpoint; ModsWildCheckpointSave(&checkpoint);
            if (DoMassOutbreakEncounterTest())
            {
                u32 outbreak = GenerateOutbreakSpecial(0, timeOfDay, &checkpoint);
                if (outbreak != SPECIAL_WILD_READY) return FALSE;
            }
#else
            if (TryStartRoamerEncounter()) { BattleSetup_StartRoamerBattle(); return TRUE; }
            if (DoMassOutbreakEncounterTest() == TRUE)
            { if (!SetUpMassOutbreakEncounter(0)) return FALSE; }
#endif
            else if (!TryGenerateWildMon(gWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo, WILD_AREA_LAND, 0))
                return FALSE;

            BattleSetup_StartWildBattle();
            return TRUE;
        }
        else if (MetatileBehavior_IsWaterWildEncounter(MapGridGetMetatileBehaviorAt(x, y)) == TRUE)
        {
            timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_WATER);

            if (AreLegendariesInSootopolisPreventingEncounters() == TRUE)
                return FALSE;
            if (gWildMonHeaders[headerId].encounterTypes[timeOfDay].waterMonsInfo == NULL)
                return FALSE;

#ifdef NATIVE
            u32 roamerResult = TryRoamerSpecial(WILD_AREA_WATER, timeOfDay, FALSE);
            if (roamerResult == SPECIAL_WILD_DENIED) return FALSE;
            if (roamerResult == SPECIAL_WILD_READY) { BattleSetup_StartRoamerBattle(); return TRUE; }
#else
            if (TryStartRoamerEncounter()) { BattleSetup_StartRoamerBattle(); return TRUE; }
#endif
            if (!TryGenerateWildMon(gWildMonHeaders[headerId].encounterTypes[timeOfDay].waterMonsInfo, WILD_AREA_WATER, 0))
                return FALSE;
            BattleSetup_StartWildBattle();
            return TRUE;
        }
    }

    return FALSE;
}

bool8 DoesCurrentMapHaveFishingMons(void)
{
    u32 headerId = GetCurrentMapWildMonHeaderId();
    enum TimeOfDay timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_FISHING);

    if (headerId != HEADER_NONE && gWildMonHeaders[headerId].encounterTypes[timeOfDay].fishingMonsInfo != NULL)
        return TRUE;
    else
        return FALSE;
}

bool8 FishingWildEncounter(u8 rod)
{
#ifdef NATIVE
    rng_value_t savedRng = gRngValue, savedRng2 = gRng2Value;
    bool8 previousFishing = gIsFishingEncounter;
    struct RhhWildEncounterCancellation cancellation;
#endif
    enum Species species;
    u32 headerId;
    s16 x, y;
    enum TimeOfDay timeOfDay;

    gIsFishingEncounter = TRUE;
    GetXYCoordsOneStepInFrontOfPlayer(&x, &y);
    if (CheckFeebasAtCoords(x, y) == TRUE)
    {
        u8 level = ChooseWildMonLevel(&gWildFeebas, 0, WILD_AREA_FISHING);

        species = gWildFeebas.species;
#ifdef NATIVE
        species = ModsWildSpecies(species, WILD_AREA_FISHING, 255, 0);
        struct RhhWildEncounterContext wildContext; u32 resolvedSpecies = species;
        if (!ModsWildResolve(gWildFeebas.species, &resolvedSpecies, WILD_AREA_FISHING, RHH_WILD_FEEBAS,
                             255, level, UINT32_MAX, x - MAP_OFFSET, y - MAP_OFFSET, &wildContext, &cancellation))
            goto cancelled;
        species = resolvedSpecies;
#endif
        CreateWildMon(species, level);
#ifdef NATIVE
        ModsWildCreated(species, WILD_AREA_FISHING, 255, 0);
        ModsWildCandidateCreated(&wildContext, &gParties[B_TRAINER_OPPONENT_A][0]);
#endif
    }
    else
    {
        headerId = GetCurrentMapWildMonHeaderId();
        timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_FISHING);
        species = GenerateFishingWildMon(gWildMonHeaders[headerId].encounterTypes[timeOfDay].fishingMonsInfo,
                                         rod, x, y, timeOfDay
#ifdef NATIVE
                                         , &cancellation
#endif
                                         );
#ifdef NATIVE
        if (species == SPECIES_NONE) goto cancelled;
#endif
    }

    IncrementGameStat(GAME_STAT_FISHING_ENCOUNTERS);
    SetPokemonAnglerSpecies(species);
    BattleSetup_StartWildBattle();
    return TRUE;
#ifdef NATIVE
cancelled:
    gRngValue = savedRng; gRng2Value = savedRng2; gIsFishingEncounter = previousFishing;
    ModsWildCancelled(&cancellation);
    return FALSE;
#endif
}

u16 GetLocalWildMon(bool8 *isWaterMon)
{
    u32 headerId;
    enum TimeOfDay timeOfDay;
    const struct WildPokemonInfo *landMonsInfo;
    const struct WildPokemonInfo *waterMonsInfo;

    *isWaterMon = FALSE;
    headerId = GetCurrentMapWildMonHeaderId();
    if (headerId == HEADER_NONE)
        return SPECIES_NONE;

    timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_LAND);
    landMonsInfo = gWildMonHeaders[headerId].encounterTypes[timeOfDay].landMonsInfo;

    timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_WATER);
    waterMonsInfo = gWildMonHeaders[headerId].encounterTypes[timeOfDay].waterMonsInfo;

    // Neither
    if (landMonsInfo == NULL && waterMonsInfo == NULL)
        return SPECIES_NONE;
    // Land Pokémon
    else if (landMonsInfo != NULL && waterMonsInfo == NULL)
        return landMonsInfo->wildPokemon[ChooseWildMonIndex_Land()].species;
    // Water Pokémon
    else if (landMonsInfo == NULL && waterMonsInfo != NULL)
    {
        *isWaterMon = TRUE;
        return waterMonsInfo->wildPokemon[ChooseWildMonIndex_Water()].species;
    }
    // Either land or water Pokémon
    if ((Random() % 100) < 80)
    {
        return landMonsInfo->wildPokemon[ChooseWildMonIndex_Land()].species;
    }
    else
    {
        *isWaterMon = TRUE;
        return waterMonsInfo->wildPokemon[ChooseWildMonIndex_Water()].species;
    }
}

u16 GetLocalWaterMon(void)
{
    u32 headerId = GetCurrentMapWildMonHeaderId();
    enum TimeOfDay timeOfDay;

    if (headerId != HEADER_NONE)
    {
        timeOfDay = GetTimeOfDayForEncounters(headerId, WILD_AREA_WATER);

        const struct WildPokemonInfo *waterMonsInfo = gWildMonHeaders[headerId].encounterTypes[timeOfDay].waterMonsInfo;

        if (waterMonsInfo)
            return waterMonsInfo->wildPokemon[ChooseWildMonIndex_Water()].species;
    }
    return SPECIES_NONE;
}

bool8 UpdateRepelCounter(void)
{
    u16 repelLureVar = VarGet(VAR_REPEL_STEP_COUNT);
    u16 steps = REPEL_LURE_STEPS(repelLureVar);
    bool32 isLure = IS_LAST_USED_LURE(repelLureVar);

    if (InBattlePike() || CurrentBattlePyramidLocation() != PYRAMID_LOCATION_NONE)
        return FALSE;
    if (InUnionRoom() == TRUE)
        return FALSE;

    if (steps != 0)
    {
        steps--;
        if (!isLure)
        {
            VarSet(VAR_REPEL_STEP_COUNT, steps);
            if (steps == 0)
            {
                ScriptContext_SetupScript(EventScript_SprayWoreOff);
                return TRUE;
            }
        }
        else
        {
            VarSet(VAR_REPEL_STEP_COUNT, steps | REPEL_LURE_MASK);
            if (steps == 0)
            {
                ScriptContext_SetupScript(EventScript_SprayWoreOff);
                return TRUE;
            }
        }

    }
    return FALSE;
}

bool8 IsWildLevelAllowedByRepel(u8 wildLevel)
{
    u8 i;

    if (!REPEL_STEP_COUNT)
        return TRUE;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (I_REPEL_INCLUDE_FAINTED == GEN_1 || I_REPEL_INCLUDE_FAINTED >= GEN_6 || GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_HP))
        {
            if (!GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_IS_EGG))
                return wildLevel >= GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_LEVEL);
        }
    }

    return FALSE;
}

bool8 IsEncounterAbilityFilterPossible(u8 level)
{
    if (GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SANITY_IS_EGG))
        return FALSE;

    enum Ability ability = GetMonAbility(&gParties[B_TRAINER_PLAYER][0]);
    if (ability == ABILITY_KEEN_EYE || ability == ABILITY_INTIMIDATE)
    {
        u8 playerMonLevel = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL);
        return playerMonLevel > 5 && level <= playerMonLevel - 5;
    }
    return FALSE;
}

bool8 IsAbilityAllowingEncounter(u8 level)
{
    return !IsEncounterAbilityFilterPossible(level) || (Random() % 2);
}

static bool8 TryGetRandomWildMonIndexByType(const struct WildPokemon *wildMon, enum Type type, u8 numMon, u8 *monIndex)
{
    u8 validIndexes[numMon]; // variable length array, an interesting feature
    u8 i, validMonCount;

    for (i = 0; i < numMon; i++)
        validIndexes[i] = 0;

    for (validMonCount = 0, i = 0; i < numMon; i++)
    {
        if (IsSpeciesOfType(wildMon[i].species, type))
            validIndexes[validMonCount++] = i;
    }

    if (validMonCount == 0 || validMonCount == numMon)
        return FALSE;

    *monIndex = validIndexes[Random() % validMonCount];
    return TRUE;
}

#include "data.h"

static u8 GetMaxLevelOfSpeciesInWildTable(const struct WildPokemon *wildMon, enum Species species, enum WildPokemonArea area)
{
    u8 i, maxLevel = 0, numMon = 0;

    switch (area)
    {
    case WILD_AREA_LAND:
        numMon = NUM_LAND_MONS_ENCOUNTER_SLOTS;
        break;
    case WILD_AREA_WATER:
        numMon = NUM_WATER_MONS_ENCOUNTER_SLOTS;
        break;
    case WILD_AREA_ROCKS:
        numMon = NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS;
        break;
    default:
    case WILD_AREA_FISHING:
    case WILD_AREA_HIDDEN:
        break;
    }

    for (i = 0; i < numMon; i++)
    {
        if (wildMon[i].species == species && wildMon[i].maxLevel > maxLevel)
            maxLevel = wildMon[i].maxLevel;
    }

    return maxLevel;
}

#ifdef BUGFIX
static bool8 TryGetAbilityInfluencedWildMonIndex(const struct WildPokemon *wildMon, enum Type type, enum Ability ability, u8 *monIndex, u32 size)
#else
static bool8 TryGetAbilityInfluencedWildMonIndex(const struct WildPokemon *wildMon, enum Type type, enum Ability ability, u8 *monIndex)
#endif
{
    if (GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SANITY_IS_EGG))
        return FALSE;
    else if (GetMonAbility(&gParties[B_TRAINER_PLAYER][0]) != ability)
        return FALSE;
    else if (Random() % 2 != 0)
        return FALSE;

#ifdef BUGFIX
    return TryGetRandomWildMonIndexByType(wildMon, type, size, monIndex);
#else
    return TryGetRandomWildMonIndexByType(wildMon, type, NUM_LAND_MONS_ENCOUNTER_SLOTS, monIndex);
#endif
}

static void ApplyFluteEncounterRateMod(u32 *encRate)
{
    if (FlagGet(FLAG_SYS_ENC_UP_ITEM) == TRUE)
        *encRate += *encRate / 2;
    else if (FlagGet(FLAG_SYS_ENC_DOWN_ITEM) == TRUE)
        *encRate = *encRate / 2;
}

static void ApplyCleanseTagEncounterRateMod(u32 *encRate)
{
    enum Item heldItem = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HELD_ITEM);
    if (gItemsInfo[heldItem].holdEffect == HOLD_EFFECT_REPEL)
        *encRate = *encRate * 2 / 3;
}

bool8 TryDoDoubleWildBattle(void)
{
    if (GetSafariZoneFlag()
      || (WE_DOUBLE_WILD_REQUIRE_2_MONS && GetMonsStateToDoubles() != PLAYER_HAS_TWO_USABLE_MONS))
        return FALSE;
    if (FollowerNPCIsBattlePartner() && FNPC_FLAG_PARTNER_WILD_BATTLES != 0
     && (FNPC_FLAG_PARTNER_WILD_BATTLES == FNPC_ALWAYS || FlagGet(FNPC_FLAG_PARTNER_WILD_BATTLES)) && FNPC_NPC_FOLLOWER_WILD_BATTLE_VS_2 == TRUE)
        return TRUE;
    else if (FlagGet(WE_FLAG_FORCE_DOUBLE_WILD))
        return TRUE;
    else if (RandomPercentage(RNG_NONE, WE_DOUBLE_WILD_CHANCE))
        return TRUE;
    return FALSE;
}

u32 ChooseHiddenMonIndex(void)
{
    #ifdef ENCOUNTER_CHANCE_HIDDEN_MONS_TOTAL
        u8 rand = Random() % ENCOUNTER_CHANCE_HIDDEN_MONS_TOTAL;

        if (rand < ENCOUNTER_CHANCE_HIDDEN_MONS_SLOT_0)
            return 0;
        else if (rand >= ENCOUNTER_CHANCE_HIDDEN_MONS_SLOT_0 && rand < ENCOUNTER_CHANCE_HIDDEN_MONS_SLOT_1)
            return 1;
        else
            return 2;
    #else
        return 0xFF;
    #endif
}

bool32 MapHasNoEncounterData(void)
{
    return (GetCurrentMapWildMonHeaderId() == HEADER_NONE);
}
