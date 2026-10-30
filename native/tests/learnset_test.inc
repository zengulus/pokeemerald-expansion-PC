/* Learnset declarations are exercised through creation, level-up and breeding. */
#ifdef RHH_BATTLE_TYPE_ICON_TEST
#include "constants/move_relearner.h"
extern u32 RhhTestRelearnerLevelMoves(struct BoxPokemon *mon, u16 *moves);
extern u32 RhhTestRelearnerEggMoves(struct BoxPokemon *mon, u16 *moves);
#endif
static struct RhhModMove sLearnsetAddonMove;
static struct RhhModLearnset sLearnsetAddonEntries[2];
static struct RhhModDefinition sLearnsetAddon;
static void PrepareLearnsetAddon(void)
{
    sLearnsetAddonMove = (struct RhhModMove){.struct_size = sizeof(sLearnsetAddonMove), .power = 30, .accuracy = 100,
        .pp = 20, .category = RHH_MOVE_PHYSICAL, .target = RHH_MOVE_TARGET_SELECTED,
        .template_move = RhhContentKeyOf("rhh:move/tackle"), .type = RhhContentKeyOf("rhh:type/normal"),
        .id = "aaa:move/early", .name = "Early", .description = "An extra move."};
    sLearnsetAddonEntries[0] = (struct RhhModLearnset){sizeof(sLearnsetAddonEntries[0]), RHH_LEARNSET_LEVEL, 50, 0,
        RhhContentKeyOf("rhh:species/torchic"), RhhContentKeyOf("aaa:move/early")};
    sLearnsetAddonEntries[1] = (struct RhhModLearnset){sizeof(sLearnsetAddonEntries[1]), RHH_LEARNSET_EGG, 0, 0,
        RhhContentKeyOf("rhh:species/combusken"), RhhContentKeyOf("test:move/surge")};
    sLearnsetAddon = (struct RhhModDefinition){.struct_size = sizeof(sLearnsetAddon), .abi_version = RHH_MOD_ABI_LEARNSETS,
        .id = "aaa:learnset", .move_count = 1, .moves = &sLearnsetAddonMove,
        .learnset_count = 2, .learnsets = sLearnsetAddonEntries};
}
static bool32 BoxKnows(struct BoxPokemon *mon, u32 move)
{
    for (u32 i = 0; i < MAX_MON_MOVES; ++i) if (GetBoxMonData(mon, MON_DATA_MOVE1 + i) == move) return TRUE;
    return FALSE;
}
static void CheckLearnsetFilter(const char *module)
{
    InitModsForTestWithLibrary(module);
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 2));
    RhhContentKey species = RhhContentKeyOf("rhh:species/torchic");
    u32 before[3], count;
    for (u32 kind = 1; kind <= 3; ++kind) CHECK(!moves->learnset_count(species, kind, &before[kind - 1]));
    RhhModsSetRunContext(123, 456, 0); ModsPrepareLearnsets();
    const struct LevelUpMove *level = GetSpeciesLevelUpLearnset(SPECIES_TORCHIC);
    CHECK(level[0].move == MOVE_SURF && level[0].level == 1 && level[1].move == MOVE_GROWL && level[1].level == 1);
    CHECK(level == GetSpeciesLevelUpLearnset(SPECIES_TORCHIC));
    for (u32 i = 0; level[i].move != LEVEL_UP_MOVE_END; ++i)
    { CHECK(level[i].move != MOVE_SCRATCH); if (i) CHECK(level[i - 1].level <= level[i].level); }
    CHECK(!moves->learnset_count(species, RHH_LEARNSET_LEVEL, &count) && count == before[0]);
    struct RhhLearnsetEntry first = {.struct_size = sizeof(first)};
    CHECK(!moves->learnset_at(species, RHH_LEARNSET_LEVEL, 0, &first) && first.move == RhhContentKeyOf("rhh:move/surf"));
    CHECK(!moves->learnset_count(species, RHH_LEARNSET_TEACHABLE, &count) && count == before[1]);
    CHECK(!CanLearnTeachableMove(SPECIES_TORCHIC, MOVE_PROTECT) && CanLearnTeachableMove(SPECIES_TORCHIC, MOVE_WATER_GUN));
    CHECK(!moves->learnset_count(species, RHH_LEARNSET_EGG, &count) && count == before[2] - 1);
    CHECK(GetSpeciesEggMoves(SPECIES_TORCHIC)[0] == MOVE_AGILITY && !SpeciesHasEggMove(SPECIES_TORCHIC, MOVE_COUNTER)
        && !SpeciesHasEggMove(SPECIES_TORCHIC, MOVE_WATER_GUN));
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_TORCHIC, 1, 1, OTID_STRUCT_PRESET(54321), 31);
    GiveMonInitialMoveset(&mon);
    CHECK(BoxKnows(&mon.box, MOVE_SURF) && BoxKnows(&mon.box, MOVE_GROWL) && !BoxKnows(&mon.box, MOVE_SCRATCH));
    for (u32 i = 0; i < MAX_MON_MOVES; ++i) SetMonMoveSlot(&mon, MOVE_NONE, i);
    CHECK(MonTryLearningNewMoveAtLevel(&mon, TRUE, 1) == MOVE_SURF
        && MonTryLearningNewMoveAtLevel(&mon, FALSE, 1) == MOVE_GROWL);
    CHECK(GetSpeciesLevelUpLearnset(SPECIES_POOCHYENA)[0].move == LEVEL_UP_MOVE_END);
    CreateMonWithIVs(&mon, SPECIES_POOCHYENA, 5, 1, OTID_STRUCT_PRESET(54321), 31);
    CHECK(MonTryLearningNewMoveAtLevel(&mon, TRUE, 5) == MOVE_NONE
        && MonTryLearningNewMoveAtLevel(&mon, FALSE, 5) == MOVE_NONE);
    CreateMonWithIVs(&mon, SPECIES_TORCHIC, 30, 1, OTID_STRUCT_PRESET(54321), 31);
    for (u32 i = 0; i < MAX_MON_MOVES; ++i) SetMonMoveSlot(&mon, MOVE_NONE, i);
    SetMonMoveSlot(&mon, MOVE_COUNTER, 0); SetMonMoveSlot(&mon, MOVE_AGILITY, 1);
    gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
    CreateMonWithIVs(&mon, SPECIES_DITTO, 30, 2, OTID_STRUCT_PRESET(54322), 31);
    gSaveBlock1Ptr->daycare.mons[1].mon = mon.box;
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagSet(FLAG_PENDING_DAYCARE_EGG); GiveEggFromDaycare();
    CHECK(gPartiesCount[B_TRAINER_PLAYER] == 1 && BoxKnows(&gParties[B_TRAINER_PLAYER][0].box, MOVE_AGILITY)
        && !BoxKnows(&gParties[B_TRAINER_PLAYER][0].box, MOVE_COUNTER));
    bool8 oldBattle = gMain.inBattle; u32 oldFlags = gBattleTypeFlags;
    gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_LINK;
    const struct LevelUpMove *link = GetSpeciesLevelUpLearnset(SPECIES_TORCHIC);
    CHECK(link[0].move == MOVE_SCRATCH && CanLearnTeachableMove(SPECIES_TORCHIC, MOVE_PROTECT)
        && SpeciesHasEggMove(SPECIES_TORCHIC, MOVE_COUNTER));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags;
    CHECK(GetSpeciesLevelUpLearnset(SPECIES_TORCHIC) == level);
    struct LevelUpMove crowded[256];
    for (u32 i = 0; i < 255; ++i) crowded[i] = (struct LevelUpMove){MOVE_SCRATCH, 1};
    crowded[255] = (struct LevelUpMove){LEVEL_UP_MOVE_END, 0};
    const struct LevelUpMove *filtered = ModsLevelUpLearnset(SPECIES_TORCHIC, crowded);
    CHECK(filtered[0].move == MOVE_SURF && filtered[1].move == LEVEL_UP_MOVE_END);
    RhhModsUnloadLibraries();
    puts("learnset filter: composition, ordering, consumers and link exclusions passed");
}
static void CheckLearnsetFilterAddon(const char *filterModule, const char *moveModule)
{
    InitModsForTestWithLibraries(filterModule, moveModule);
    RhhModsSetRunContext(123, 456, 0); ModsPrepareLearnsets();
    u32 slot = 0; CHECK(!RhhModsMoveSlot(RhhContentKeyOf("test:move/surge"), 0, &slot));
    CHECK(CanLearnTeachableMove(SPECIES_TORCHIC, slot) && !SpeciesHasEggMove(SPECIES_TORCHIC, slot));
    const struct LevelUpMove *level = GetSpeciesLevelUpLearnset(SPECIES_TORCHIC);
    bool32 found = FALSE;
    for (u32 i = 0; level[i].move != LEVEL_UP_MOVE_END; ++i)
        if (level[i].move == slot && level[i].level == 50) found = TRUE;
    CHECK(found);
    bool8 inBattle = gMain.inBattle; u32 flags = gBattleTypeFlags;
    gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_LINK;
    CHECK(SpeciesHasEggMove(SPECIES_TORCHIC, slot));
    gMain.inBattle = inBattle; gBattleTypeFlags = flags;
    RhhModsUnloadLibraries(); puts("learnset filter: independent registered move composition passed");
}
static void CheckLearnsetSlotsFull(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 2));
    RhhContentKey species = RhhContentKeyOf("rhh:species/torchic"), key = RhhContentKeyOf("test:move/surge");
    u32 before[3]; for (u32 kind = 1; kind <= 3; ++kind) CHECK(!moves->learnset_count(species, kind, &before[kind - 1]));
    RhhModsSetRunContext(123, 456, 0);
    for (u32 i = 0; i < RHH_RUNTIME_MOVE_COUNT; ++i)
    {
        char name[48]; snprintf(name, sizeof(name), "test:move/reserved_%03u", i);
        u32 slot; CHECK(!RhhModsMoveSlot(RhhContentKeyOf(name), 1, &slot));
    }
    ModsPrepareLearnsets();
    u32 slot = 999, count;
    CHECK(RhhModsMoveSlot(key, 0, &slot) && slot == 999
        && !moves->learnset_count(species, RHH_LEARNSET_LEVEL, &count) && count == before[0] + 2
        && !moves->learnset_count(species, RHH_LEARNSET_TEACHABLE, &count) && count == before[1]
        && !moves->learnset_count(species, RHH_LEARNSET_EGG, &count) && count == before[2]);
    RhhModsUnloadLibraries();
    puts("mod learnsets: full slot map omits unavailable moves");
}
static void CheckModLearnsets(const char *path, const char *module, bool32 write, bool32 addon)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins());
    if (addon) { PrepareLearnsetAddon(); CHECK(!RhhModsRegister(&sLearnsetAddon)); }
    if (module) CHECK(!RhhModsLoadLibrary(module));
    CHECK(!RhhModsFinalize() && !NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    const struct RhhMoveService *moves = (const void *)RhhModsHostAPI()->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 2));
    RhhContentKey species = RhhContentKeyOf("rhh:species/torchic"), key = RhhContentKeyOf("test:move/surge");
    u32 before[3];
    for (u32 kind = 1; kind <= 3; ++kind) CHECK(!moves->learnset_count(species, kind, &before[kind - 1]));
    if (write)
    {
        gModDraft = (struct ModConfig){0}; ModsNewGame(7); Save_ResetSaveCounters();
        gMapHeader = *gMapGroups[1][0]; gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    }
    else CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
    u32 custom = 0;
    if (!module)
    {
        CHECK(!RhhModsMoveSlot(key, 0, &custom) && GetMoveEffect(custom) == EFFECT_PLACEHOLDER);
        CHECK(BoxKnows(&gParties[B_TRAINER_PLAYER][0].box, custom));
        for (u32 kind = 1; kind <= 3; ++kind) { u32 count; CHECK(!moves->learnset_count(species, kind, &count) && count == before[kind - 1]); }
        puts("mod learnsets: missing module preserves learned key");
        NativeSaveClose(); RhhModsUnloadLibraries(); return;
    }
    CHECK(!RhhModsMoveSlot(key, 0, &custom) && custom == RHH_RUNTIME_MOVE_FIRST);
    u32 levelCount = 0, eggCount = 0, teachCount = 0;
    CHECK(!moves->learnset_count(species, RHH_LEARNSET_LEVEL, &levelCount) && levelCount == before[0] + 3 + !!addon);
    CHECK(!moves->learnset_count(species, RHH_LEARNSET_TEACHABLE, &teachCount) && teachCount == before[1] + 1);
    CHECK(!moves->learnset_count(species, RHH_LEARNSET_EGG, &eggCount) && eggCount == before[2] + 1);
    bool32 found = FALSE, evolution = FALSE, surf = FALSE; u32 countCustom = 0, lastLevel = 0;
    for (u32 i = 0; i < levelCount; ++i)
    {
        struct RhhLearnsetEntry entry = {.struct_size = sizeof(entry)};
        CHECK(!moves->learnset_at(species, RHH_LEARNSET_LEVEL, i, &entry) && entry.level >= lastLevel);
        lastLevel = entry.level;
        if (entry.move == key) { CHECK(entry.level == 50); ++countCustom; found = TRUE; }
        if (entry.move == RhhContentKeyOf("rhh:move/surf") && !entry.level) evolution = TRUE;
        if (entry.move == RhhContentKeyOf("rhh:move/surf") && entry.level == 50) surf = TRUE;
    }
    CHECK(found && countCustom == 1 && evolution && surf && CanLearnTeachableMove(SPECIES_TORCHIC, custom)
        && SpeciesHasEggMove(SPECIES_TORCHIC, custom));
    if (!write)
    {
        CHECK(BoxKnows(&gParties[B_TRAINER_PLAYER][0].box, custom));
        if (addon) { u32 extra; CHECK(!RhhModsMoveSlot(RhhContentKeyOf("aaa:move/early"), 0, &extra) && extra == custom + 1
            && SpeciesHasEggMove(SPECIES_COMBUSKEN, custom)); }
        puts("mod learnsets: save and slot remapping passed");
        NativeSaveClose(); RhhModsUnloadLibraries(); return;
    }
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 50, 1, OTID_STRUCT_PRESET(54321), 31);
    GiveMonInitialMoveset(&gParties[B_TRAINER_PLAYER][0]);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    CHECK(BoxKnows(&gParties[B_TRAINER_PLAYER][0].box, custom) && BoxKnows(&gParties[B_TRAINER_PLAYER][0].box, MOVE_SURF));
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
    struct Pokemon leveling; CreateMonWithIVs(&leveling, SPECIES_TORCHIC, 49, 1, OTID_STRUCT_PRESET(54321), 31);
    for (u32 i = 0; i < MAX_MON_MOVES; ++i) SetMonMoveSlot(&leveling, MOVE_NONE, i);
    u32 learnedCustom = 0, learnedSurf = 0;
    for (u32 i = 0; i < 8; ++i)
    {
        u32 result = MonTryLearningNewMoveAtLevel(&leveling, i == 0, 50);
        if (result == MOVE_NONE) break;
        if (result == custom) ++learnedCustom;
        if (result == MOVE_SURF) ++learnedSurf;
    }
    CHECK(learnedCustom == 1 && learnedSurf == 1 && BoxKnows(&leveling.box, custom));
    for (u32 i = 0; i < MAX_MON_MOVES; ++i) SetMonMoveSlot(&leveling, MOVE_NONE, i);
    CHECK(MonTryLearningNewMoveEvolution(&leveling, TRUE) == MOVE_SURF && BoxKnows(&leveling.box, MOVE_SURF));
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    struct Pokemon relearn; CreateMonWithIVs(&relearn, SPECIES_TORCHIC, 50, 1, OTID_STRUCT_PRESET(54321), 31);
    u16 candidates[MAX_RELEARNER_MOVES + 1] = {0}; candidates[MAX_RELEARNER_MOVES] = 0xbeef;
    u32 n = RhhTestRelearnerLevelMoves(&relearn.box, candidates); bool32 customLevel = FALSE;
    for (u32 i = 0; i < n; ++i) if (candidates[i] == custom) customLevel = TRUE;
    CHECK(customLevel && candidates[MAX_RELEARNER_MOVES] == 0xbeef);
    memset(candidates, 0, sizeof(candidates)); candidates[MAX_RELEARNER_MOVES] = 0xbeef;
    n = RhhTestRelearnerEggMoves(&relearn.box, candidates); bool32 customEgg = FALSE;
    for (u32 i = 0; i < n; ++i) if (candidates[i] == custom) customEgg = TRUE;
    CHECK(customEgg && candidates[MAX_RELEARNER_MOVES] == 0xbeef);
#endif
    struct Pokemon parent; CreateMonWithIVs(&parent, SPECIES_TORCHIC, 30, 1, OTID_STRUCT_PRESET(54321), 31);
    for (u32 i = 0; i < MAX_MON_MOVES; ++i) SetMonMoveSlot(&parent, MOVE_NONE, i);
    SetMonMoveSlot(&parent, custom, 0); gSaveBlock1Ptr->daycare.mons[0].mon = parent.box;
    CreateMonWithIVs(&parent, SPECIES_DITTO, 30, 2, OTID_STRUCT_PRESET(54322), 31);
    gSaveBlock1Ptr->daycare.mons[1].mon = parent.box;
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER])); gPartiesCount[B_TRAINER_PLAYER] = 0;
    FlagSet(FLAG_PENDING_DAYCARE_EGG); GiveEggFromDaycare();
    CHECK(gPartiesCount[B_TRAINER_PLAYER] == 1 && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_TORCHIC
        && BoxKnows(&gParties[B_TRAINER_PLAYER][0].box, custom));
    puts("mod learnsets: creation, level-up, breeding and keyed queries passed");
    NativeSaveClose(); RhhModsUnloadLibraries();
}
#ifdef RHH_BATTLE_TYPE_ICON_TEST
static void CheckLearnsetRelearnerCapacity(void)
{
    for (u32 kind = RHH_LEARNSET_LEVEL; kind <= RHH_LEARNSET_EGG; kind += 2)
    {
        RhhModsInit(); CHECK(!ModsRegisterBuiltins());
        struct RhhModLearnset entries[120];
        for (u32 i = 0; i < ARRAY_COUNT(entries); ++i)
        {
            RhhContentKey move = RhhContentKeyForId(RHH_MOVE, i + 1); CHECK(move);
            entries[i] = (struct RhhModLearnset){sizeof(entries[i]), kind,
                kind == RHH_LEARNSET_LEVEL ? 50 : 0, 0, RhhContentKeyOf("rhh:species/torchic"), move};
        }
        struct RhhModDefinition mod = {.struct_size = sizeof(mod), .abi_version = RHH_MOD_ABI_LEARNSETS,
            .id = "test:relearner_capacity", .learnset_count = ARRAY_COUNT(entries), .learnsets = entries};
        CHECK(!RhhModsRegister(&mod) && !RhhModsFinalize());
        gModDraft = (struct ModConfig){0}; ModsNewGame(7);
        struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_TORCHIC, 50, 1, OTID_STRUCT_PRESET(54321), 31);
        u16 candidates[MAX_RELEARNER_MOVES + 1] = {0}; candidates[MAX_RELEARNER_MOVES] = 0xbeef;
        u32 count = kind == RHH_LEARNSET_LEVEL ? RhhTestRelearnerLevelMoves(&mon.box, candidates)
            : RhhTestRelearnerEggMoves(&mon.box, candidates);
        CHECK(count == MAX_RELEARNER_MOVES && candidates[MAX_RELEARNER_MOVES] == 0xbeef);
        RhhModsShutdown();
    }
    puts("mod learnsets: relearner bounds passed");
}
#endif
