/* Links the complete native game and its real data; no gameplay substitutes. */
#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "battle.h"
#include "battle_util.h"
#include "event_data.h"
#include "load_save.h"
#include "malloc.h"
#include "move.h"
#include "overworld.h"
#include "pokemon.h"
#include "random.h"
#include "script.h"
#include "constants/script_commands.h"
#include "constants/characters.h"
#include "constants/event_objects.h"
#include "gba/m4a_internal.h"
#include "bg.h"
#include "dma3.h"
#include "gpu_regs.h"
#include "menu.h"
#include "task.h"
#include "decompress.h"
#include "sprite.h"
#include "palette.h"
#include "../../src/data/map_group_count.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #c); exit(1); \
} } while (0)

extern ScrCmdFunc gScriptCmdTable[], gScriptCmdTableEnd[];
static struct BattleStruct sBattle;
extern const struct MapHeader *const *const gMapGroups[];

static void CheckPaddedGraphicsTransfer(void)
{
    const u8 packed[] = {0x10, 8, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};
    const struct BgTemplate bg = {.bg = 2};
    ResetTasks();
    InitGpuRegManager();
    InitBgsFromTemplates(0, &bg, 1);
    memset(gNativeVram, 0xcc, 64);
    DecompressAndLoadBgGfxUsingHeap(2, packed, 64, 0, 0);
    ProcessDma3Requests();
    for (unsigned i = 0; i < 64; ++i)
        CHECK(gNativeVram[i] == (i < 8 ? i + 1 : 0));
    RunTasks();
    for (unsigned i = 0; i < NUM_TASKS; ++i) CHECK(!gTasks[i].isActive);
    ResetSpriteData();
    const struct CompressedSpriteSheet sheet = {.data = (const u32 *)packed, .size = 64, .tag = 0x7788};
    u32 tile = LoadCompressedSpriteSheet(&sheet);
    for (unsigned i = 0; i < 64; ++i)
        CHECK(gNativeVram[0x10000 + tile * 32 + i] == (i < 8 ? i + 1 : 0));
    FreeSpriteTilesByTag(sheet.tag);
    puts("graphics: short compressed asset transfers have bounded zero padding");
}

static void CheckData(void)
{
    for (unsigned i = 0; i + 1 < NUM_MUSIC_PLAYERS; ++i)
        CHECK((uintptr_t)gMPlayTable[i + 1].track - (uintptr_t)gMPlayTable[i].track
              >= gMPlayTable[i].numTracks * sizeof(struct MusicPlayerTrack));
    unsigned species = 0;
    for (unsigned i = 1; i < NUM_SPECIES; ++i)
    {
        if (!gSpeciesInfo[i].baseHP) continue;
        CHECK(GetSpeciesType(i, 0) < NUMBER_OF_MON_TYPES);
        CHECK(GetSpeciesType(i, 1) < NUMBER_OF_MON_TYPES);
        CHECK(gSpeciesInfo[i].speciesName[0] != EOS);
        for (unsigned variant = 0; variant < 4; ++variant)
        {
            const u16 *palette = GetMonSpritePalFromSpecies(i, variant & 1, variant >> 1);
            CHECK(palette != NULL);
            LoadPaletteFast(palette, 0, PLTT_SIZE_4BPP);
        }
        ++species;
    }
    CHECK(species > 1000);
    for (unsigned i = 1; i < MOVES_COUNT_ALL; ++i)
    {
        CHECK(GetMoveType(i) < NUMBER_OF_MON_TYPES);
        CHECK(GetMoveCategory(i) <= DAMAGE_CATEGORY_STATUS);
        CHECK(GetMoveEffect(i) < NUM_BATTLE_MOVE_EFFECTS);
        CHECK(GetMoveName(i) != NULL);
    }
    CHECK(GetMoveCategory(MOVE_WATERFALL) == DAMAGE_CATEGORY_PHYSICAL);
    CHECK(GetMoveCategory(MOVE_SURF) == DAMAGE_CATEGORY_SPECIAL);
    CHECK(GetMoveType(MOVE_MOONBLAST) == TYPE_FAIRY);
    printf("data: %u enabled species and %u moves validated\n", species, MOVES_COUNT_ALL - 1);
}

static void CheckPokemonPersistence(void)
{
    struct Pokemon original;
    CreateMonWithIVs(&gPlayerParty[0], SPECIES_TORCHIC, 5, 12345,
                    OTID_STRUCT_PRESET(54321), 31);
    gPlayerPartyCount = 1;
    CHECK(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES) == SPECIES_TORCHIC);
    CHECK(GetMonData(&gPlayerParty[0], MON_DATA_HP_IV) == 31);
    original = gPlayerParty[0];
    SavePlayerParty();
    ZeroPlayerPartyMons();
    CHECK(gPlayerPartyCount == 0);
    LoadPlayerParty();
    CHECK(gPlayerPartyCount == 1);
    CHECK(memcmp(&original, &gPlayerParty[0], sizeof(original)) == 0);
    CHECK(GetMonData(&gPlayerParty[0], MON_DATA_LEVEL) == 5);
    CHECK(GetMonData(&gPlayerParty[0], MON_DATA_HP) > 0);
    puts("pokemon: encrypted data, stats and save-block party roundtrip passed");
}

static void CheckMaps(void)
{
    struct MapHeader originalHeader = gMapHeader;
    unsigned maps = 0, clones = 0;
    for (unsigned group = 0; group < MAP_GROUPS_COUNT; ++group)
    {
        if (!gMapGroups[group]) continue;
        for (unsigned number = 0; number < MAP_GROUP_COUNT[group]; ++number)
        {
            const u8 *scripts[OBJECT_EVENT_TEMPLATES_COUNT];
            gMapHeader = *Overworld_GetMapHeaderByGroupAndId(group, number);
            CHECK(gMapHeader.mapLayout != NULL && gMapHeader.events != NULL);
            CHECK(gMapHeader.mapLayout->width > 0 && gMapHeader.mapLayout->height > 0);
            CHECK(gMapHeader.events->objectEventCount <= OBJECT_EVENT_TEMPLATES_COUNT);
            LoadObjEventTemplatesFromHeader();
            for (unsigned i = 0; i < OBJECT_EVENT_TEMPLATES_COUNT; ++i)
            {
                scripts[i] = gSaveBlock1Ptr->objectEventTemplates[i].script;
                gSaveBlock1Ptr->objectEventTemplates[i].script = (const u8 *)(uintptr_t)1;
                gSaveBlock1Ptr->objectEventTemplates[i].x = 123;
                if (i < gMapHeader.events->objectEventCount
                    && gMapHeader.events->objectEvents[i].kind == OBJ_KIND_CLONE)
                    ++clones;
            }
            LoadSaveblockObjEventScripts();
            for (unsigned i = 0; i < OBJECT_EVENT_TEMPLATES_COUNT; ++i)
            {
                CHECK(gSaveBlock1Ptr->objectEventTemplates[i].script == scripts[i]);
                CHECK(gSaveBlock1Ptr->objectEventTemplates[i].x == 123);
            }
            ++maps;
        }
    }
    // Emerald omits Kanto groups. Exercise a clone pointing at a real linked
    // map so the FRLG restore path is covered with the default configuration.
    const struct MapHeader *house = Overworld_GetMapHeaderByGroupAndId(1, 0);
    const struct ObjectEventTemplate clone = {.kind = OBJ_KIND_CLONE,
        .targetMapGroup = 1, .targetMapNum = 0, .targetLocalId = 1};
    const struct MapEvents events = {.objectEventCount = 1, .objectEvents = &clone};
    gMapHeader.events = &events;
    gSaveBlock1Ptr->objectEventTemplates[0].script = NULL;
    LoadSaveblockObjEventScripts();
    CHECK(gSaveBlock1Ptr->objectEventTemplates[0].script == house->events->objectEvents[0].script);
    ++clones;
    CHECK(maps > 500 && clones > 0);
    printf("maps: %u layouts and %u clone script rebindings validated\n", maps, clones);
    gMapHeader = originalHeader;
}

static uq4_12_t Effectiveness(enum Move move, enum Species species,
                            enum Ability attackAbility, enum Ability defendAbility)
{
    struct DamageContext ctx = {0};
    memset(gBattleMons, 0, sizeof(gBattleMons));
    ctx.battlerAtk = B_BATTLER_0;
    ctx.battlerDef = B_BATTLER_1;
    ctx.move = ctx.baseMove = ctx.chosenMove = move;
    ctx.moveType = GetMoveType(move);
    ctx.abilities[0] = attackAbility;
    ctx.abilities[1] = defendAbility;
    gBattleMons[1].species = species;
    gBattleMons[1].types[0] = GetSpeciesType(species, 0);
    gBattleMons[1].types[1] = GetSpeciesType(species, 1);
    gBattleMons[1].types[2] = TYPE_MYSTERY;
    gBattleMons[1].hp = gBattleMons[1].maxHP = 100;
    return CalcTypeEffectivenessMultiplier(&ctx);
}

static void CheckBattles(void)
{
    gBattleStruct = &sBattle;
    gBattlersCount = 2;
    gBattlerPositions[0] = B_POSITION_PLAYER_LEFT;
    gBattlerPositions[1] = B_POSITION_OPPONENT_LEFT;
    CHECK(Effectiveness(MOVE_DRAGON_PULSE, SPECIES_CLEFABLE, ABILITY_NONE, ABILITY_NONE) == UQ_4_12(0));
    CHECK(Effectiveness(MOVE_MOONBLAST, SPECIES_HYDREIGON, ABILITY_NONE, ABILITY_NONE) == UQ_4_12(4));
    CHECK(Effectiveness(MOVE_POUND, SPECIES_GENGAR, ABILITY_NONE, ABILITY_NONE) == UQ_4_12(0));
    CHECK(Effectiveness(MOVE_POUND, SPECIES_GENGAR, ABILITY_SCRAPPY, ABILITY_NONE) == UQ_4_12(1));
    CHECK(Effectiveness(MOVE_FREEZE_DRY, SPECIES_BLASTOISE, ABILITY_NONE, ABILITY_NONE) == UQ_4_12(2));
    CHECK(Effectiveness(MOVE_FLYING_PRESS, SPECIES_ABOMASNOW, ABILITY_NONE, ABILITY_NONE) == UQ_4_12(4));
    CHECK(Effectiveness(MOVE_THOUSAND_ARROWS, SPECIES_CHARIZARD, ABILITY_NONE, ABILITY_NONE) == UQ_4_12(1));
    CHECK(Effectiveness(MOVE_BRICK_BREAK, SPECIES_TERAPAGOS_TERASTAL, ABILITY_NONE, ABILITY_TERA_SHELL) == UQ_4_12(0.5));
    puts("battle: Fairy, Scrappy, Freeze-Dry, Flying Press, Thousand Arrows and Tera Shell passed");
}

static void CheckScripts(void)
{
    struct ScriptContext ctx;
    u8 sub[] = {SCR_OP_ADDVAR, 0x00, 0x40, 7, 0, SCR_OP_RETURN};
    u8 script[7 + sizeof(uintptr_t)] = {SCR_OP_SETVAR, 0x00, 0x40, 42, 0, SCR_OP_CALL};
    const u8 noEffect[] = {SCR_OP_END};
    uintptr_t address = (uintptr_t)sub;
    memcpy(script + 6, &address, sizeof(address));
    script[6 + sizeof(address)] = SCR_OP_END;
    CHECK(Script_HasNoEffect(noEffect));
    CHECK(!Script_HasNoEffect(script));
    InitScriptContext(&ctx, gScriptCmdTable, gScriptCmdTableEnd);
    SetupBytecodeScript(&ctx, script);
    unsigned frames = 0;
    while (RunScriptCommand(&ctx)) CHECK(++frames < 8);
    CHECK(VarGet(0x4000) == 49);
    puts("script: effect metadata, variables, native-address call and return passed");
}

#include "mod_game_test.inc"

int main(int argc, char **argv)
{
    SeedRng(12345);
    InitHeap(gHeap, HEAP_SIZE);
    ClearSav1();
    ClearSav2();
    ClearSav3();
    SetSaveBlocksPointers(0);
    if (argc == 4 && !strcmp(argv[1], "external"))
    { ModsSetExternalBuiltinDirectory(argv[2]); CheckMods(argv[3]); RhhModsUnloadLibraries(); return 0; }
    if (argc == 3 && !strcmp(argv[2], "music"))
    { CheckMusic(argv[1]); return 0; }
    if (argc == 3 && !strcmp(argv[2], "readmods"))
    { InitModsForTest(); CheckSavedNuzlocke(argv[1]); return 0; }
    CheckPaddedGraphicsTransfer();
    CheckData();
    CheckMaps();
    CheckPokemonPersistence();
    CheckBattles();
    CheckScripts();
    CheckMods(argc == 2 ? argv[1] : NULL);
    puts("native gameplay regression checks passed");
    return 0;
}
