/* Real engine integration: transformations, caps, item use and save recovery. */
#include "mods.h"
#include "mod_host.h"
#include "native.h"
#include "item.h"
#include "caps.h"
#include "save.h"
#include "data.h"
#include "battle_setup.h"
#include "starter_choose.h"
#include "constants/maps.h"
#include "script_pokemon_util.h"
#include "mod_music.h"
#include "native_audio.h"
#include "sound.h"
#include "m4a.h"
#include "constants/songs.h"
#include "wild_encounter.h"
#include <math.h>
#include "../mods/bundled.h"

static void CheckExampleMigrations(void)
{
    const struct RhhModDefinition *wild = RhhBundledWildRandomizer(RhhModsHostAPI());
    const struct RhhModDefinition *nuz = RhhBundledNuzlocke(RhhModsHostAPI());
    u8 oldWild[16] = {2}, oldNuz[1312] = {1, 1, 0, 1, 2, 0, 1, 0};
    u8 *state = malloc(wild->state_size);
    CHECK(state != NULL); oldWild[8] = 42;
    CHECK(wild->migrate(1, oldWild, sizeof(oldWild), state, wild->state_size) == 0);
    CHECK(wild->validate(state, wild->state_size) == 0 && state[0] == 2 && state[8] == 42);
    CHECK(state[2] || state[3]); /* Frozen roster was populated. */
    CHECK(wild->migrate(1, oldWild, sizeof(oldWild) - 1, state, wild->state_size) != 0);
    oldWild[0] = 3;
    CHECK(wild->migrate(1, oldWild, sizeof(oldWild), state, wild->state_size) != 0);
    free(state); state = malloc(nuz->state_size); CHECK(state != NULL);
    oldNuz[32] = 0x11; oldNuz[40] = 0x22; oldNuz[288] = 0x33;
    CHECK(nuz->migrate(1, oldNuz, sizeof(oldNuz), state, nuz->state_size) == 0);
    CHECK(nuz->validate(state, nuz->state_size) == 0);
    CHECK(state[0] == 1 && state[1] == 1 && state[3] == 0 && state[4] == 2 && state[6] == 1);
    CHECK(state[32] == 0x11 && state[40] == 0x22 && state[4128] == 0x33);
    oldNuz[4] = 33;
    CHECK(nuz->migrate(1, oldNuz, sizeof(oldNuz), state, nuz->state_size) != 0);
    free(state);
    puts("example migrations: frozen wild roster and expanded Nuzlocke ledgers preserve state; malformed schemas rejected");
}

static void InitModsForTest(void)
{
    RhhModsInit(); CHECK(ModsRegisterBuiltins() == 0); CHECK(RhhModsFinalize() == 0);
}
static void CheckSavedNuzlocke(const char *path)
{
    CHECK(NativeSaveOpen(path) == 0); gFlashMemoryPresent = TRUE;
    CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
    CHECK(ModsRunFailed() && ModsMonIsDead(&gParties[B_TRAINER_PLAYER][0]));
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP) == 0);
    NativeSaveClose();
    puts("Nuzlocke death and failed-run journal survived a fresh process");
}
static void CheckMods(const char *path)
{
    InitModsForTest();
    CheckExampleMigrations();
    CHECK(ModsGetConfig().flags == 0 && ModsGetConfig().wildMode == 0 && ModsGetConfig().music == 0);
    CHECK(RhhContentKeyForId(RHH_TRAINER, TRAINER_SAWYER_1) != 0);
    CHECK(RhhContentKeyForId(RHH_TRAINER, TRAINER_SAWYER_1) != RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1));
    CHECK(RhhContentKeyForId(RHH_MUSIC, MUS_VS_WILD) != 0);
    rng_value_t rng = gRngValue;
    CHECK(ModsWildSpecies(SPECIES_POOCHYENA, 0, 0, 0) == SPECIES_POOCHYENA);
    CHECK(GetStarterPokemon(1) == SPECIES_TORCHIC && GetSpeciesBaseStatTotal(SPECIES_TORCHIC) == 310);
    CHECK(ModsStarterCount() > 900 && ModsStarterEligible(SPECIES_DRAGONITE));
    CHECK(!ModsStarterEligible(SPECIES_MEWTWO) && !ModsStarterEligible(SPECIES_MEW));
    CHECK(!ModsStarterEligible(SPECIES_CHARIZARD_MEGA_X));
    gModDraft = (struct ModConfig){.flags = MOD_EQUALIZE | MOD_LEVEL_CAP | MOD_TRAINERS,
        .wildMode = MOD_WILD_POOL, .starter = RhhContentKeyForId(RHH_SPECIES, SPECIES_DRAGONITE), .seed = 123456};
    ModsNewGame(7);
    CHECK(GetStarterPokemon(1) == SPECIES_DRAGONITE && GetStarterPokemon(0) == SPECIES_TREECKO);
    for (u32 species = 1; species < NUM_SPECIES; ++species)
    {
        if (!gSpeciesInfo[species].baseHP) continue;
        CHECK(GetSpeciesBaseStatTotal(species) == 600);
        for (u32 stat = 0; stat < 6; ++stat)
            CHECK(GetSpeciesBaseStat(species, stat) >= 1 && GetSpeciesBaseStat(species, stat) <= 255);
    }
    CHECK(GetSpeciesBaseHP(SPECIES_SHEDINJA) == 1);
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE101);
    gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE101);
    u32 species = ModsWildSpecies(SPECIES_POOCHYENA, 0, 3, 0);
    for (u32 i = 0; i < 100; ++i) CHECK(ModsWildSpecies(SPECIES_ZIGZAGOON, 0, 3, 0) == species);
    for (u32 slot = 0; slot < 6; ++slot)
        CHECK(ModsTrainerSpecies(SPECIES_ZIGZAGOON, TRAINER_SAWYER_1, slot)
            == ModsTrainerSpecies(SPECIES_POOCHYENA, TRAINER_SAWYER_1, slot));
    CHECK(!memcmp(&gRngValue, &rng, sizeof(rng)));
    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    const struct Trainer *trainer = GetTrainerStructFromId(TRAINER_SAWYER_1);
    CreateNPCTrainerPartyFromTrainer(gParties[B_TRAINER_OPPONENT_A], trainer);
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES)
        == ModsTrainerSpecies(trainer->party[0].species, TRAINER_SAWYER_1, 0));
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_MOVE1) != MOVE_NONE);
    const u16 leaders[] = {TRAINER_ROXANNE_1, TRAINER_BRAWLY_1, TRAINER_WATTSON_1, TRAINER_FLANNERY_1,
        TRAINER_NORMAN_1, TRAINER_WINONA_1, TRAINER_TATE_AND_LIZA_1, TRAINER_JUAN_1, TRAINER_WALLACE};
    for (u32 i = 0; i < 8; ++i) FlagClear(FLAG_BADGE01_GET + i);
    for (u32 badge = 0; badge <= 8; ++badge)
    {
        const struct Trainer *leader = GetTrainerStructFromId(leaders[badge]);
        u32 ace = 1;
        for (u32 i = 0; i < leader->partySize; ++i) ace = max(ace, leader->party[i].lvl);
        CHECK(GetCurrentLevelCap() == ace);
        if (badge < 8) FlagSet(FLAG_BADGE01_GET + badge);
    }
    for (u32 i = 0; i < 8; ++i) FlagClear(FLAG_BADGE01_GET + i);
    u32 cap = GetCurrentLevelCap();
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 80, 123,
        OTID_STRUCT_PRESET(456), 31);
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL) == cap);
    CHECK(ExecuteTableBasedItemEffect(&gParties[B_TRAINER_PLAYER][0], ITEM_RARE_CANDY, 0, 0) == TRUE);
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL) == cap);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_TORCHIC, 80, 123,
        OTID_STRUCT_PRESET(456), 31);
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_LEVEL) == 80);
    rng = gRngValue;
    gModDraft.wildMode = MOD_WILD_FULL; ModsNewGame(7);
    u8 *snapshot; u32 length, draws[32];
    CHECK(RhhModsExport(&snapshot, &length) == 0);
    for (u32 i = 0; i < ARRAY_COUNT(draws); ++i)
    { draws[i] = ModsWildSpecies(SPECIES_POOCHYENA, 0, 3, 0); ModsWildCreated(draws[i], 0, 3, 0); }
    bool32 different = FALSE;
    for (u32 i = 1; i < ARRAY_COUNT(draws); ++i) different |= draws[i] != draws[0];
    CHECK(different && RhhModsImport(snapshot, length) == 0);
    for (u32 i = 0; i < ARRAY_COUNT(draws); ++i)
    { CHECK(ModsWildSpecies(SPECIES_POOCHYENA, 0, 3, 0) == draws[i]); ModsWildCreated(draws[i], 0, 3, 0); }
    free(snapshot); CHECK(!memcmp(&gRngValue, &rng, sizeof(rng)));
    u32 header = GetCurrentMapWildMonHeaderId(); CHECK(header != HEADER_NONE);
    const struct WildPokemonInfo *wild = gWildMonHeaders[header].encounterTypes[TIME_OF_DAY_DEFAULT].landMonsInfo;
    CHECK(wild != NULL);
    u32 firstWild = SPECIES_NONE; bool32 varied = FALSE;
    for (u32 i = 0; i < 16; ++i)
    {
        u32 expected = ModsWildSpecies(SPECIES_POOCHYENA, WILD_AREA_LAND, 0, 0);
        CHECK(TryGenerateWildMon(wild, WILD_AREA_LAND, 0));
        u32 actual = GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES);
        CHECK(actual == expected);
        if (!i) firstWild = actual; else varied |= actual != firstWild;
    }
    CHECK(varied); /* Actual creation, rather than queries, advances full mode. */
    puts("mods: all species reach 600 BST; stable pools/trainers, full RNG save/reload, starters and all Gym/Champion caps passed");

    gModDraft = (struct ModConfig){.flags = MOD_NUZLOCKE, .seed = 987}; ModsNewGame(7);
    gBattleTypeFlags = 0; ClearBag();
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 5, 123,
        OTID_STRUCT_PRESET(456), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    u16 hp = 0; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP, &hp);
    CHECK(!ModsMonIsDead(&gParties[B_TRAINER_PLAYER][0])); /* before balls */
    hp = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MAX_HP);
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP, &hp);
    CHECK(AddBagItem(ITEM_POKE_BALL, 5));
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_ROUTE101)][MAP_NUM(MAP_ROUTE101)];
    ModsEncounterStarted(); CHECK(ModsCanCapture());
    if (path)
    {
        CHECK(NativeSaveOpen(path) == 0); /* opening resets state; recreate this unsaved run */
        ModsNewGame(7); ModsEncounterStarted();
        gFlashMemoryPresent = TRUE; Save_ResetSaveCounters();
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
    }
    hp = 0; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP, &hp);
    CHECK(ModsMonIsDead(&gParties[B_TRAINER_PLAYER][0]));
    hp = 100; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP, &hp);
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP) == 0);
    CHECK(ExecuteTableBasedItemEffect(&gParties[B_TRAINER_PLAYER][0], ITEM_MAX_REVIVE, 0, 0) == TRUE);
    HealPlayerParty(); CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP) == 0);
    gBattlerPartyIndexes[0] = 0; gBattleTypeFlags = 0;
    CHECK(GetFirstFaintedPartyIndex(0) == PARTY_SIZE); /* Revival Blessing cannot target the dead. */
    struct Pokemon boxed;
    BoxMonToMon(&gParties[B_TRAINER_PLAYER][0].box, &boxed);
    CHECK(GetMonData(&boxed, MON_DATA_HP) == 0);
    ModsEncounterStarted(); CHECK(!ModsCanCapture());
    if (path)
    {
        NativeSaveClose(); CHECK(NativeSaveOpen(path) == 0);
        ZeroPlayerPartyMons(); CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
        CHECK(ModsMonIsDead(&gParties[B_TRAINER_PLAYER][0]) && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP) == 0);
        ModsEncounterStarted(); CHECK(!ModsCanCapture());
        ModsWhiteout(); CHECK(ModsRunFailed()); NativeSaveClose();
    }
    puts("Nuzlocke: grace period, encounter consumption, permanent fainting, revive refusal, PC conversion and journal reload passed");
}

static void CheckMusic(const char *root)
{
    InitModsForTest(); gModDraft = (struct ModConfig){0}; ModsNewGame(99);
    NativeMusicSetAssetRoot(root); cgb_audio_init(42060); m4aSoundInit();
    const u16 songs[] = {MUS_VS_WILD, MUS_VS_TRAINER, MUS_VS_GYM_LEADER, MUS_VS_CHAMPION};
    for (u32 gen = 1; gen <= 5; ++gen)
    {
        ModsSetMusic(gen);
        for (u32 role = 0; role < 4; ++role)
        {
            PlayBGM(songs[role]);
            CHECK(NativeMusicActive() && NativeMusicAsset() == (gen - 1) * 4 + role + 1);
            u32 nonzero = 0; double energy = 0; float peak = 0;
            for (u32 block = 0; block < 300; ++block)
            {
                float audio[701 * 2] = {0}; NativeMusicMix(audio, 701);
                for (u32 i = 0; i < ARRAY_COUNT(audio); ++i)
                {
                    CHECK(isfinite(audio[i])); if (audio[i] != 0) ++nonzero;
                    energy += (double)audio[i] * audio[i]; peak = fmaxf(peak, fabsf(audio[i]));
                }
            }
            CHECK(nonzero > 1000 && energy > 0.001 && peak > 0.0001f && peak < 4);
            PlaySE(SE_SELECT);
            struct MusicPlayerInfo *fx = gMPlayTable[gSongTable[SE_SELECT].ms].info;
            u32 oldClock = fx->clock;
            for (u32 i = 0; i < 4; ++i) m4aSoundMain();
            CHECK(fx->clock > oldClock); /* The SFX player still advances. */
            m4aMPlayFadeOut(&gMPlayInfo_BGM, 1);
            for (u32 i = 0; i < 20; ++i) m4aSoundMain();
            CHECK(gMPlayInfo_BGM.status & MUSICPLAYER_STATUS_PAUSE);
            float faded[128] = {0}; NativeMusicMix(faded, 64);
            for (u32 i = 0; i < ARRAY_COUNT(faded); ++i) CHECK(faded[i] == 0);
            printf("music: gen %u role %u samples=%u energy=%.6f peak=%.6f; SFX and fade passed\n", gen, role, nonzero, energy, peak);
        }
    }
    ModsSetMusic(0); PlayBGM(MUS_VS_WILD); CHECK(!NativeMusicActive());
    NativeMusicShutdown();
    puts("all twenty genuine battle MIDIs and all five generation styles rendered through real SoundFonts");
}
