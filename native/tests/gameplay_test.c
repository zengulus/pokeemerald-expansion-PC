/* Links the complete native game and its real data; no gameplay substitutes. */
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "global.h"
#include "hall_of_fame.h"
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
#include "constants/field_effects.h"
#include "constants/metatile_behaviors.h"
#include "field_effect_helpers.h"
#include "field_effect.h"
#include "event_object_movement.h"
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
extern void TrySpawnLightSprites(s16 camX, s16 camY);
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
#include "location_ecology_test.inc"
#include "authored_maps_test.inc"
#include "egg_hatch_policy_test.inc"
#include "egg_cycle_test.inc"
#include "mon_edit_purity_test.inc"
#include "hyper_training_edit_test.inc"
#include "nature_storage_test.inc"
#include "nature_format_test.inc"
#include "nature_recorded_test.inc"
#include "nature_consumers_test.inc"
#include "true_nature_edit_test.inc"
#include "tera_edit_test.inc"
#include "pending_egg_id_test.inc"
#include "daycare_ordinal_test.inc"
#include "wild_rate_test.inc"
#include "wild_spawn_interval_test.inc"
#include "breeding_accept_test.inc"
#include "breeding_request_test.inc"
#include "breeding_observer_rng_test.inc"
#include "breeding_seeded_test.inc"
#include "breeding_repeatable_test.inc"
#include "breeding_traits_test.inc"
#include "breeding_iv_preview_test.inc"
#include "breeding_iv_rule_test.inc"
#include "breeding_moves_test.inc"
#include "breeding_move_rule_test.inc"
#include "wild_trait_edit_test.inc"
#include "wild_neighbors_test.inc"
#include "habitat_search_test.inc"
#include "wild_route_test.inc"
#include "ground_effect_test.inc"
#include "natural_weather_test.inc"
#include "berry_weather_test.inc"
#include "wild_scripted_test.inc"
#include "tutorial_retry_save_test.inc"
#include "scouting_test.inc"
#include "battle_environment_test.inc"
#include "photo_test.inc"
#include "photo_album_test.inc"
#include "photo_reward_test.inc"
#include "battle_ev_test.inc"
#include "ev_limits_test.inc"
#include "item_ev_test.inc"
#include "ev_purity_test.inc"
#include "photo_album_ui_test.inc"
#include "ui_test.inc"
#include "content_description_test.inc"
#include "ui_accessibility_test.inc"
#include "party_mod_test.inc"
#include "experience_award_test.inc"
#include "photo_experience_test.inc"
#include "battle_lifecycle_test.inc"
#include "type_chart_test.inc"
#include "custom_move_test.inc"
#include "move_resolution_test.inc"
#include "battle_turn_test.inc"
#include "battle_health_test.inc"
#include "type_observation_test.inc"
#include "public_battler_types_test.inc"
#include "music_cue_test.inc"
#include "move_priority_test.inc"
#include "innate_turn_test.inc"
#include "innate_entry_test.inc"
#include "innate_turn_actions_test.inc"
#include "innate_status_test.inc"
#include "battle_status_test.inc"
#include "bag_status_test.inc"
#include "status_condition_test.inc"
#include "innate_sourced_stat_test.inc"
#include "pp_cost_test.inc"
#include "move_selection_test.inc"
#include "owned_mon_change_test.inc"
#include "abilities_test.inc"
#include "ability_mutation_test.inc"
#include "weather_presence_test.inc"
#include "battle_field_test.inc"
#include "battle_grounding_test.inc"
#include "ability_gas_test.inc"
#include "ai_move_score_test.inc"
#include "innate_ai_entry_test.inc"
#include "custom_status_test.inc"
#include "effect_heal_test.inc"
#include "effect_damage_battler_test.inc"
#include "effect_status_test.inc"
#include "effect_stat_test.inc"
#include "effect_registry_test.inc"
#include "effect_animation_test.inc"
#include "damage_effect_test.inc"
#include "move_resolution_damage_test.inc"
#include "move_accuracy_checked_test.inc"
#include "learnset_test.inc"
#include "species_types_test.inc"
#if defined(RHH_DEXNAV_TEST_FIXTURE) && DEXNAV_ENABLED
#include "dexnav_types_test.inc"
#endif
#ifdef RHH_POKEDEX_TYPE_TEST_FIXTURE
#include "pokedex_types_test.inc"
#endif
#include "trainer_source_test.inc"
#include "boss_ace_test.inc"
#include "music_assignment_test.inc"
#include "music_sha256.h"
#include "music_import_test.inc"
#include "music_preview_test.inc"
#include "music_import_ui_test.inc"
#include "owned_identity_test.inc"
#include "wild_catch_origin_test.inc"
#include "owned_rental_test.inc"
#include "breeding_purity_test.inc"
#include "type_inspector_test.inc"
#include "environment_timed_test.inc"

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "hall-of-fame-growth"))
    {
        struct HallofFameTeam team = {0};
        for (u32 i = 0; i < 1000; ++i)
        {
            team.mon[0].tid = i + 1;
            CHECK(NativeHallOfFameAppend(&team));
        }
        CHECK(gHoFSaveCount == 1000);
        for (u32 i = 0; i < 1000; ++i) CHECK(gHoFSaveBuffer[i].mon[0].tid == i + 1);
        free(gHoFSaveBuffer); gHoFSaveBuffer = NULL; gHoFSaveCount = 0;
        puts("Hall of Fame: 1000 teams retained in order"); return 0;
    }
    SeedRng(12345);
    InitHeap(gHeap, HEAP_SIZE);
    ClearSav1();
    ClearSav2();
    ClearSav3();
    SetSaveBlocksPointers(0);
    if (argc == 4 && !strcmp(argv[1], "location-ecology"))
    { CheckLocationEcology(argv[2], !strcmp(argv[3], "read")); return 0; }
    if (argc == 2 && !strcmp(argv[1], "egg-cycle-observer"))
    { CheckEggCycleObserverOnly(); return 0; }
    if (argc == 2 && !strcmp(argv[1], "egg-cycle-policy"))
    { CheckEggCyclePolicy(); return 0; }
    if (argc == 2 && !strcmp(argv[1], "egg-hatch-policy"))
    { CheckEggHatchPolicy(); return 0; }
    if (argc == 2 && !strcmp(argv[1], "daycare-timing"))
    { CheckDaycareTiming(); return 0; }
    if (argc == 2 && !strcmp(argv[1], "move-calculation-purity"))
    { CheckCalcPurity(); return 0; }
    if (argc == 4 && !strcmp(argv[1], "move-base-purity"))
    { CheckBaseMovePurity(argv[2], argv[3]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "mod-ui"))
    { CheckModUi(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "content-descriptions"))
    { CheckContentDescriptions(argv[2]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "ui-accessibility"))
    { CheckUiAccessibility(argv[2], argv[3]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "battle-ev-yield"))
    { CheckBattleEvYield(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "battle-ev-bank"))
    { CheckBattleBank(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "ev-limits"))
    { CheckEvLimits(argv[2]); return 0; }
    if (argc == 2 && !strcmp(argv[1], "ev-purity"))
    { CheckEvPurity(); return 0; }
    if (argc == 4 && !strcmp(argv[1], "item-ev-yield"))
    { CheckItemEv(argv[2], argv[3]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "item-ev-reduction"))
    { CheckItemReduction(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "item-ev-reduction-menu"))
    { CheckPartyItemReduction(argv[2]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "battle-ev-save"))
    { CheckBattleEvSave(argv[2], argv[3], FALSE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "battle-ev-load"))
    { CheckBattleEvSave(argv[2], argv[3], TRUE); return 0; }
    if (argc == 3 && !strcmp(argv[1], "party-experience-levels"))
    { CheckPartyExperienceLevels(atoi(argv[2])); return 0; }
    if (argc == 4 && !strcmp(argv[1], "experience-award"))
    { CheckExperienceAward(argv[2], atoi(argv[3])); return 0; }
    if (argc == 6 && !strcmp(argv[1], "photo-experience"))
    { CheckPhotoExperience(argv[2], argv[3], argv[4], atoi(argv[5])); return 0; }
    if (argc == 5 && !strcmp(argv[1], "battle-passes"))
    { CheckBattlePasses(argv[2], argv[3], atoi(argv[4])); return 0; }
    if (argc == 2 && !strcmp(argv[1], "world-environment-api"))
    { InitModsForTest(); CheckWorldEnvironment(); return 0; }
    if (argc == 2 && !strcmp(argv[1], "natural-weather"))
    { CheckNaturalWeather(); return 0; }
    if (argc == 3 && !strcmp(argv[1], "species-types"))
    { CheckSpeciesTypes(argv[2]); return 0; }
#ifdef RHH_POKEDEX_TYPE_TEST_FIXTURE
    if (argc == 3 && !strcmp(argv[1], "pokedex-types"))
    { CheckPokedexTypeIcons(argv[2]); return 0; }
#endif
    if (argc == 2 && !strcmp(argv[1], "public-battler-types-seen"))
    { CheckPublicBattlerSeen(); return 0; }
    if (argc == 4 && !strcmp(argv[1], "public-type-observation"))
    { CheckPublicTypeObservation(argv[2], atoi(argv[3]) != 0); return 0; }
    if (argc == 3 && !strcmp(argv[1], "battle-health-observation"))
    { CheckBattleHealthObservation(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "type-chart"))
    { CheckTypeChart(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "type-observation"))
    { CheckTypeObservation(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "move-resolution"))
    { CheckMoveResolution(argv[2], NULL); return 0; }
    if (argc == 4 && !strcmp(argv[1], "move-resolution-custom"))
    { CheckMoveResolution(argv[2], argv[3]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "battle-turn"))
    { CheckBattleTurn(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "move-priority"))
    { CheckMovePriority(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "ai-move-score"))
    { CheckAiMoveScore(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "move-target-context"))
    { CheckMoveTargetContext(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "effect-heal-battler"))
    { CheckSpecificHealing(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "effect-damage-battler"))
    { CheckSpecificDamage(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "effect-status-battler"))
    { CheckSpecificStatus(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "effect-stat-battler"))
    { CheckSpecificStat(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "damage-effect"))
    { CheckDamageEffect(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "damage-effect-invalid"))
    { CheckDamageEffectInvalid(argv[2]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "mod-learnsets-save"))
    { CheckModLearnsets(argv[2], argv[3], TRUE, FALSE); return 0; }
    if (argc == 3 && !strcmp(argv[1], "learnset-filter"))
    { CheckLearnsetFilter(argv[2]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "learnset-filter-addon"))
    { CheckLearnsetFilterAddon(argv[2], argv[3]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "mod-learnsets-load"))
    { CheckModLearnsets(argv[2], argv[3], FALSE, FALSE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "mod-learnsets-remapped"))
    { CheckModLearnsets(argv[2], argv[3], FALSE, TRUE); return 0; }
    if (argc == 3 && !strcmp(argv[1], "mod-learnsets-missing"))
    { CheckModLearnsets(argv[2], NULL, FALSE, FALSE); return 0; }
    if (argc == 3 && !strcmp(argv[1], "mod-learnsets-full"))
    { CheckLearnsetSlotsFull(argv[2]); return 0; }
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    if (argc == 2 && !strcmp(argv[1], "mod-learnsets-capacity"))
    { CheckLearnsetRelearnerCapacity(); return 0; }
#endif
    if (argc == 4 && !strcmp(argv[1], "pp-cost-legacy"))
    { CheckPPCostLegacy(argv[2], argv[3]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "battle-lifecycle"))
    { CheckBattleLifecycle(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "trainer-source"))
    { CheckTrainerSource(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "breeding-nature"))
    { CheckBreedingNatureRules(argv[2]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "owned-identity"))
    { CheckOwnedIdentity(argv[2], argv[3]); return 0; }
#ifdef RHH_OWNED_IDENTITY_TEST
    if (argc == 3 && !strcmp(argv[1], "authored-wild-preview"))
    { CheckAuthoredWildPreview(argv[2]); return 0; }
    if (argc == 2 && !strcmp(argv[1], "wild-proposal-purity"))
    { CheckWildProposalPurity(); return 0; }
    if (argc == 3 && !strcmp(argv[1], "wild-battle-profile"))
    { CheckWildBattleProfile(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "wild-battle-traits"))
    { CheckWildBattleTraits(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "wild-catch-origin"))
    { CheckWildCatchOrigin(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "wild-battle-catch-only"))
    { CheckWildBattleCatchOnly(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "wild-battle-view-only"))
    { CheckWildBattleViewOnly(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-pc-ledger"))
    { CheckOwnedPcLedger(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-world-ledger"))
    { CheckOwnedWorldLedger(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-egg-ledger"))
    { CheckOwnedEggLedger(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-catch-ledger"))
    { CheckOwnedCatchLedger(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-trade-ledger"))
    { CheckOwnedTradeLedger(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-trade-copy-guard"))
    { CheckOwnedTradeCopyGuard(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-consistency-ledger"))
    { CheckOwnedConsistencyLedger(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-live-audit"))
    { CheckOwnedLiveAudit(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-live-audit-capacity"))
    { CheckOwnedAuditCapacity(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-creation-audit"))
    { CheckOwnedCreationAudit(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-copy-audit"))
    { CheckOwnedCopyAudit(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-identity-write-audit"))
    { CheckOwnedIdentityWriteAudit(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-consistency-reload"))
    { CheckOwnedConsistencyReload(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-consistency-v1"))
    { CheckOwnedConsistencyV1(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-consistency-legacy"))
    { CheckOwnedConsistencyLegacy(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-consistency-reject"))
    { CheckOwnedConsistencyReject(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-consistency-rollback"))
    { CheckOwnedConsistencyRollback(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-sky-ledger"))
    { CheckOwnedSkyLedger(argv[2], NULL); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-union-ledger"))
    { CheckOwnedUnionLedger(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-link-multi-ledger"))
    { CheckOwnedLinkMultiLedger(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-frontier-ledger"))
    { CheckOwnedFrontierLedger(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-rental-ledger"))
    { CheckOwnedRentalLedger(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-dome-ledger"))
    { CheckOwnedDomeLedger(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-pyramid-ledger"))
    { CheckOwnedPyramidLedger(argv[2]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "owned-sky-probe"))
    { CheckOwnedSkyLedger(argv[2], argv[3]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-follower-ledger"))
    { CheckOwnedFollowerLedger(argv[2], NULL); return 0; }
    if (argc == 4 && !strcmp(argv[1], "owned-follower-probe"))
    { CheckOwnedFollowerLedger(argv[2], argv[3]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-follower-catch-ledger"))
    { CheckOwnedFollowerCatchLedger(argv[2], NULL); return 0; }
    if (argc == 4 && !strcmp(argv[1], "owned-follower-catch-probe"))
    { CheckOwnedFollowerCatchLedger(argv[2], argv[3]); return 0; }
#endif
    if (argc == 4 && !strcmp(argv[1], "owned-history-read"))
    { CheckOwnedHistoryRead(argv[2], argv[3]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "boss-ace-save"))
    { CheckBossAce(argv[2], argv[3], FALSE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "boss-ace-load"))
    { CheckBossAce(argv[2], argv[3], TRUE); return 0; }
    if (argc == 5 && !strcmp(argv[1], "music-assignment-save"))
    { CheckMusicAssignments(argv[2], argv[3], argv[4], FALSE); return 0; }
    if (argc == 5 && !strcmp(argv[1], "music-assignment-load"))
    { CheckMusicAssignments(argv[2], argv[3], argv[4], TRUE); return 0; }
    if (argc == 5 && !strcmp(argv[1], "music-transfer"))
    { CheckMusicTransfer(argv[2], argv[3], argv[4]); return 0; }
    if (argc == 6 && !strcmp(argv[1], "music-import-save"))
    { CheckMusicImports(argv[2], argv[3], argv[4], argv[5], 0); return 0; }
    if (argc == 6 && !strcmp(argv[1], "music-import-load"))
    { CheckMusicImports(argv[2], argv[3], argv[4], argv[5], 1); return 0; }
    if (argc == 6 && !strcmp(argv[1], "music-import-delete"))
    { CheckMusicImports(argv[2], argv[3], argv[4], argv[5], 2); return 0; }
    if (argc == 6 && !strcmp(argv[1], "music-import-corrupt"))
    { CheckMusicImports(argv[2], argv[3], argv[4], argv[5], 3); return 0; }
    if (argc == 6 && !strcmp(argv[1], "music-import-locked"))
    { CheckMusicImports(argv[2], argv[3], argv[4], argv[5], 4); return 0; }
    if (argc == 6 && !strcmp(argv[1], "music-import-io"))
    { CheckMusicImports(argv[2], argv[3], argv[4], argv[5], 5); return 0; }
    if (argc == 5 && !strcmp(argv[1], "music-preview"))
    { CheckMusicPreview(argv[2], argv[3], argv[4]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "windows-file-dialog"))
    { CheckWindowsFileDialog(argv[2]); return 0; }
#ifdef RHH_TRAINER_SOURCE_TEST
    if (argc == 3 && !strcmp(argv[1], "trainer-pool-source"))
    { CheckTrainerPoolSource(argv[2]); return 0; }
#endif
#if OW_USE_FAKE_RTC
    if (argc == 2 && !strcmp(argv[1], "world-time-fake"))
    { CheckWorldTimeFake(); return 0; }
#endif
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "perception"))
    { CheckPerception(argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "world-time"))
    { CheckWorldTime(argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "environment"))
    { if (argc == 3) ModsSetExternalBuiltinDirectory(argv[2]); CheckBattleEnvironment(); return 0; }
    if (argc == 4 && !strcmp(argv[1], "external"))
    { ModsSetExternalBuiltinDirectory(argv[2]); CheckMods(argv[3]); RhhModsUnloadLibraries(); return 0; }
    if (argc == 3 && !strcmp(argv[1], "breeding-external"))
    { CheckExternalBreeding(argv[2]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "breeding-accept"))
    { CheckBreedingAccept(argv[2], argv[3]); return 0; }
    if (argc == 2 && !strcmp(argv[1], "breeding-purity"))
    { CheckBreedingPurity(); return 0; }
    if (argc == 2 && !strcmp(argv[1], "breeding-observer-rng"))
    { CheckBreedingObserverRng(); return 0; }
    if (argc == 2 && !strcmp(argv[1], "timed-environment"))
    { CheckTimedEnvironment(); return 0; }
    if (argc == 4 && !strcmp(argv[1], "type-inspector"))
    { CheckTypeInspector(argv[2], argv[3]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "breeding-request"))
    { CheckBreedingRequest(argv[2], argv[3]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "breeding-pending"))
    { CheckBreedingPendingLifecycle(argv[2]); return 0; }
    if (argc == 2 && !strcmp(argv[1], "breeding-iv-preview"))
    { CheckBreedingIvPreview(); return 0; }
    if (argc == 3 && !strcmp(argv[1], "nature-storage"))
    { CheckNatureStorage(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "nature-format"))
    { CheckNatureFormatSave(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "pending-egg-id"))
    { CheckPendingEggIds(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "daycare-ordinal"))
    { CheckDaycareOrdinal(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "daycare-ordinal-reload"))
    { CheckDaycareOrdinalReload(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "pending-egg-id-reload"))
    { CheckPendingEggIdReload(argv[2]); return 0; }
    if ((argc == 3 || argc == 4) && !strcmp(argv[1], "breeding-iv-rule"))
    { CheckBreedingIvRule(argv[2], argc == 4 ? argv[3] : NULL); return 0; }
    if ((argc == 3 || argc == 4) && !strcmp(argv[1], "breeding-moves"))
    { CheckBreedingMoves(argv[2], argc == 4 ? argv[3] : NULL); return 0; }
    if ((argc == 3 || argc == 4) && !strcmp(argv[1], "breeding-move-rule"))
    { CheckBreedingMoveRule(argv[2], argc == 4 ? argv[3] : NULL); return 0; }
    if (argc == 4 && !strcmp(argv[1], "breeding-move-rule-custom"))
    { CheckBreedingMoveRuleCustom(argv[2], argv[3]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "breeding-moves-custom"))
    { CheckBreedingMovesCustom(argv[2], argv[3]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "breeding-joint-external"))
    { CheckBreedingJointExternal(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "breeding-count"))
    { CheckBreedingCount(argv[2]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "abilities"))
    { CheckAbilities(argv[2], argv[3]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "ability-mutations"))
    { CheckAbilityMutations(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "ability-contacts"))
    { CheckAbilityContacts(argv[2]); return 0; }
    if (argc == 2 && !strcmp(argv[1], "weather-presence"))
    { CheckWeatherPresence(); return 0; }
    if (argc == 3 && !strcmp(argv[1], "battle-field-rules"))
    { CheckBattleFieldRules(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "battle-field-effect"))
    { CheckFieldRuleEffect(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "battle-field-legacy-sport"))
    { CheckFieldRuleLegacySport(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "battle-grounding"))
    { CheckBattleGrounding(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "ability-gas"))
    { CheckGasChanges(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "move-resolution-damage"))
    { CheckMoveResolutionDamage(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "move-accuracy-checked"))
    { CheckMoveAccuracyChecked(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "owned-mon-changes"))
    { CheckOwnedMonChanges(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "innate-entry-target"))
    { CheckInnateEntryTarget(argv[2]); return 0; }
    if (argc == 2 && !strcmp(argv[1], "breeding-tests"))
    { InitModsForTest(); CheckBreedingHooks(); return 0; }
#if WE_DOUBLE_WILD_CHANCE == 100
    if (argc == 2 && !strcmp(argv[1], "wild-cancel-double"))
    { CheckCancellationDouble(); return 0; }
#endif
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-special-outbreak"))
    { CheckSpecialOutbreak(argc == 3 ? argv[2] : NULL); return 0; }
    if (argc == 3 && !strcmp(argv[1], "wild-scripted"))
    { CheckScriptedWild(argv[2]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "tutorial-retry-save"))
    { CheckTutorialRetrySave(argv[2], argv[3], FALSE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "tutorial-retry-load"))
    { CheckTutorialRetrySave(argv[2], argv[3], TRUE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "scouting-save"))
    { CheckScouting(argv[2], argv[3], FALSE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "scouting-load"))
    { CheckScouting(argv[2], argv[3], TRUE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "photo"))
    { CheckPhotoCapture(argv[2], atoi(argv[3])); return 0; }
    if (argc == 5 && !strcmp(argv[1], "photo-album-save"))
    { CheckPhotoAlbum(argv[2], argv[3], argv[4], 0); return 0; }
    if (argc == 6 && !strcmp(argv[1], "photo-reward"))
    { CheckPhotoReward(argv[2], argv[3], argv[4], atoi(argv[5])); return 0; }
    if (argc == 5 && !strcmp(argv[1], "photo-album-load"))
    { CheckPhotoAlbum(argv[2], argv[3], argv[4], 1); return 0; }
    if (argc == 5 && !strcmp(argv[1], "photo-album-reject"))
    { CheckPhotoAlbum(argv[2], argv[3], argv[4], 2); return 0; }
    if (argc == 5 && !strcmp(argv[1], "photo-album-full-load"))
    { CheckPhotoAlbum(argv[2], argv[3], argv[4], 3); return 0; }
    if (argc == 5 && !strcmp(argv[1], "photo-album-legacy"))
    { CheckPhotoAlbum(argv[2], argv[3], argv[4], 4); return 0; }
    if (argc == 5 && !strcmp(argv[1], "photo-album-legacy-reload"))
    { CheckPhotoAlbum(argv[2], argv[3], argv[4], 5); return 0; }
    if (argc == 3 && !strcmp(argv[1], "ui-image"))
    { CheckUiImage(argv[2]); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-special-roamer"))
    { CheckSpecialRoamer(argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-facility-pike"))
    { CheckFacilityWild(TRUE, FALSE, argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-facility-manual-pike"))
    { CheckManualFacilityWild(TRUE, argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-facility-manual-pyramid"))
    { CheckManualFacilityWild(FALSE, argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-facility-pyramid"))
    { CheckFacilityWild(FALSE, FALSE, argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-facility-pyramid-open"))
    { CheckFacilityWild(FALSE, TRUE, argc == 3 ? argv[2] : NULL); return 0; }
#if defined(RHH_DEXNAV_TEST_FIXTURE) && DEXNAV_ENABLED
    if (argc == 4 && !strcmp(argv[1], "dexnav-types"))
    { CheckDexNavTypeIcons(argv[2], argv[3]); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-dexnav"))
    { CheckDexNavCancellation(argc == 3 ? argv[2] : NULL); return 0; }
#endif
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-cancel"))
    { CheckCancellationIntegration(argc == 3 ? argv[2] : NULL); return 0; }
    if (argc == 2 && !strcmp(argv[1], "mon-edit-purity"))
    { CheckMonEditPurity(); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "mon-edit-paid"))
    { CheckPaidMonEdit(0, argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "owned-experience"))
    { InitModsForTestWithLibrary(argc == 3 ? argv[2] : NULL); CheckOwnedExperienceService(argc == 3);
      if (argc == 3) RhhModsUnloadLibraries(); return 0; }
    if (argc == 3 && !strcmp(argv[1], "experience-moves-custom"))
    { CheckExperienceMoveCustom(argv[2]); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "mon-edit-legacy"))
    { InitModsForTestWithLibrary(argc == 3 ? argv[2] : NULL); CheckOwnedMonService(); CheckMonEditService();
      if (argc == 3) RhhModsUnloadLibraries(); return 0; }
    if (argc == 2 && !strcmp(argv[1], "hyper-training-edit"))
    { CheckHyperTrainingEdit(); return 0; }
    if (argc == 3 && !strcmp(argv[1], "nature-recorded-playback"))
    { CheckNatureRecordedPlayback(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "nature-consumers"))
    { CheckNatureConsumers(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "true-nature-edit"))
    { CheckTrueNatureEdit(argv[2]); return 0; }
    if (argc == 2 && !strcmp(argv[1], "bag-status"))
    { CheckBagStatus(); return 0; }
    if (argc == 4 && !strcmp(argv[1], "battle-status"))
    { CheckBattleStatus(argv[2], argv[3]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "status-condition"))
    { CheckStatusConditions(argv[2], argv[3]); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "mon-edit-paid-pyramid"))
    { CheckPaidMonEdit(1, argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "mon-edit-paid-secret-base"))
    { CheckPaidMonEdit(2, argc == 3 ? argv[2] : NULL); return 0; }
    if (argc == 4 && !strcmp(argv[1], "mon-edit-paid-save"))
    { CheckPaidEditSave(argv[2], FALSE, argv[3]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "mon-edit-paid-load"))
    { CheckPaidEditSave(argv[2], TRUE, argv[3]); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "mon-edit-recipe"))
    { CheckRecipeMonEdit(0, argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "mon-edit-recipe-pyramid"))
    { CheckRecipeMonEdit(1, argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "mon-edit-recipe-secret-base"))
    { CheckRecipeMonEdit(2, argc == 3 ? argv[2] : NULL); return 0; }
    if (argc == 4 && !strcmp(argv[1], "mon-edit-recipe-save"))
    { CheckRecipeEditSave(argv[2], FALSE, argv[3]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "mon-edit-recipe-load"))
    { CheckRecipeEditSave(argv[2], TRUE, argv[3]); return 0; }
    if (argc == 2 && !strcmp(argv[1], "wild-rules"))
    { InitModsForTest(); CheckOverworldService(); return 0; }
    if (argc == 3 && !strcmp(argv[1], "authored-maps"))
    { InitModsForTestWithLibrary(argv[2]); CheckAuthoredMaps(); return 0; }
    if (argc == 3 && !strcmp(argv[1], "scene-mesh"))
    {
        sSceneNeighborRuleEnabled = TRUE; sSceneNeighborLightsEnabled = TRUE; InitModsForTestWithLibrary(argv[2]); CheckOverworldService(); CheckActualSceneMesh();
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_WORLD_RENDER_PASSES};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        RhhModsDispatch(&event, &decision); RhhModsUnloadLibraries(); return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "wild-encounter-external"))
    { CheckExternalWildEncounter(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "wild-rate"))
    { CheckWildRate(argv[2]); return 0; }
#ifdef RHH_WILD_SPAWN_TEST
    if (argc == 3 && !strcmp(argv[1], "wild-spawn-interval"))
    { CheckWildSpawnInterval(argv[2]); return 0; }
#endif
    if (argc == 4 && !strcmp(argv[1], "wild-trait-edits-save"))
    { CheckWildTraitEdit(argv[2], argv[3], FALSE); return 0; }
    if (argc == 7 && !strcmp(argv[1], "wild-queue-missing"))
    { CheckWildQueueMissingOwner(argv[2], argv[6]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-route"))
    { CheckWildRoute(argv[2], argv[3]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-route-load"))
    { CheckWildRouteLoad(argv[2], argv[3]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-trait-edits-load"))
    { CheckWildTraitEdit(argv[2], argv[3], TRUE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-profile-batch-save"))
    { CheckWildProfileBatch(argv[2], argv[3], FALSE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-profile-batch-load"))
    { CheckWildProfileBatch(argv[2], argv[3], TRUE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-profile-schema-conflict"))
    { CheckWildProfileSchemaConflict(argv[2], argv[3], TRUE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-profile-schema-compatible"))
    { CheckWildProfileSchemaConflict(argv[2], argv[3], FALSE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-profile-schema-legacy"))
    { CheckWildProfileSchemaLegacy(argv[2], argv[3]); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-profile-missing"))
    { CheckWildProfileOwner(argv[2], argv[3], FALSE); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-profile-owner-restored"))
    { CheckWildProfileOwner(argv[2], argv[3], TRUE); return 0; }
    if (argc == 3 && !strcmp(argv[1], "wild-actor-external"))
    { CheckExternalWildActor(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "innate-turn"))
    { CheckInnateTurn(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "innate-turn-actions"))
    { CheckInnateTurnActions(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "innate-status"))
    { CheckInnateStatus(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "innate-sourced-stat"))
    { CheckInnateSourcedStat(argv[2]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "innate-ai-entry"))
    { CheckInnateAiEntry(argv[2]); return 0; }
    if (argc == 2 && !strcmp(argv[1], "wild-actor-id-failure"))
    { CheckWildIndividualPersistenceFailure(); return 0; }
    if ((argc == 4 || argc == 5) && !strcmp(argv[1], "wild-actor-save"))
    { CheckWildActorSave(argv[2], argv[3], FALSE, argc == 5 ? argv[4] : NULL); return 0; }
    if ((argc == 4 || argc == 5) && !strcmp(argv[1], "wild-actor-load"))
    { CheckWildActorSave(argv[2], argv[3], TRUE, argc == 5 ? argv[4] : NULL); return 0; }
#if WE_OW_ENCOUNTERS
    if ((argc == 4 || argc == 5) && !strcmp(argv[1], "wild-dormant-save"))
    { CheckDormantSave(argv[2], argv[3], 0, argc == 5 ? argv[4] : NULL, NULL); return 0; }
    if ((argc == 4 || argc == 5) && !strcmp(argv[1], "wild-dormant-load"))
    { CheckDormantSave(argv[2], argv[3], 1, argc == 5 ? argv[4] : NULL, NULL); return 0; }
    if ((argc == 4 || argc == 5) && !strcmp(argv[1], "wild-dormant-verify"))
    { CheckDormantSave(argv[2], argv[3], 2, argc == 5 ? argv[4] : NULL, NULL); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-dormant-owner-missing"))
    { CheckDormantSave(argv[2], argv[3], 3, NULL, NULL); return 0; }
    if (argc == 4 && !strcmp(argv[1], "wild-dormant-legacy-archive"))
    { CheckDormantSave(argv[2], argv[3], 5, NULL, NULL); return 0; }
    if (argc == 6 && !strcmp(argv[1], "wild-dormant-legacy"))
    { CheckDormantSave(argv[2], argv[3], 4, argv[4], argv[5]); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-facility-owe-pike"))
    { CheckFacilityOverworld(TRUE, argc == 3 ? argv[2] : NULL); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-facility-owe-pyramid"))
    { CheckFacilityOverworld(FALSE, argc == 3 ? argv[2] : NULL); return 0; }
#if WE_OWE_FEEBAS_SPOTS
    if (argc == 2 && !strcmp(argv[1], "wild-special-owe-feebas"))
    { CheckSpecialOverworldFeebas(); return 0; }
#endif
    if (argc == 2 && !strcmp(argv[1], "wild-special-owe-roamer"))
    { CheckSpecialOverworld(TRUE); return 0; }
    if (argc == 2 && !strcmp(argv[1], "wild-special-owe-outbreak"))
    { CheckSpecialOverworld(FALSE); return 0; }
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wild-actor-auto"))
    { CheckAutomaticWildActor(argc == 3 ? argv[2] : NULL, FALSE); return 0; }
    if (argc == 2 && !strcmp(argv[1], "wild-actor-species"))
    { CheckAutomaticWildActor(NULL, TRUE); return 0; }
#endif
    if (argc == 3 && !strcmp(argv[2], "music"))
    { CheckMusic(argv[1]); return 0; }
    if (argc == 4 && !strcmp(argv[2], "music-instruments"))
    { CheckMusicInstruments(argv[1], atoi(argv[3])); return 0; }
    if (argc == 3 && !strcmp(argv[2], "music-cue"))
    { CheckMusicBattleCue(argv[1]); return 0; }
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
