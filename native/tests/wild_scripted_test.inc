#include "battle_setup.h"
#include "battle_main.h"
#include "battle_transition.h"
#include "battle_controllers.h"
#include "starter_choose.h"
#include "script_pokemon_util.h"
#include "constants/map_event_ids.h"
#include "constants/event_object_movement.h"
#include "constants/field_specials.h"
#include "constants/comparison_operators.h"
extern bool8 ScrCmd_setwildbattle(struct ScriptContext *ctx);
extern bool8 ScrCmd_dowildbattle(struct ScriptContext *ctx);
extern u16 ScriptedWildBattleAllowed(void);
extern u16 TutorialWildBattleAllowed(void);
extern bool8 MovementAction_InitAffineAnim_Step0(struct ObjectEvent *, struct Sprite *);
extern bool8 MovementAction_ClearAffineAnim_Step0(struct ObjectEvent *, struct Sprite *);
extern const u8 AquaHideout_B1F_EventScript_Electrode1[];
extern const u8 AquaHideout_B1F_EventScript_Electrode2[], AquaHideout_B1F_EventScript_PrepareElectrode1[];
extern const u8 NavelRock_Bottom_EventScript_PrepareLugia[];
extern const u8 EventScript_PrepareKecleon[], Route120_EventScript_PrepareBridgeKecleon[];
extern const u8 TerraCave_End_EventScript_PrepareGroudon[], MarineCave_End_EventScript_PrepareKyogre[];
extern const u8 NavelRock_Top_EventScript_PrepareHoOh[];
extern const u8 SouthernIsland_Interior_EventScript_PrepareLati[];
extern const u8 FarawayIsland_Interior_EventScript_PrepareMew[];
extern const u8 BirthIsland_Exterior_EventScript_PrepareDeoxys[], BirthIsland_Exterior_EventScript_RockResult[];
extern const u8 BirthIsland_Exterior_EventScript_Deoxys[];
extern const u8 PetalburgCity_EventScript_PrepareWallyTutorial[];
extern const u8 PetalburgCity_EventScript_WallyRetry[];
extern const u8 Route101_EventScript_PrepareStarterChoice[];
extern u16 (*const gSpecials[])(void);
extern u16 (*const gSpecials_End[])(void);
extern bool8 ScrCmd_special(struct ScriptContext *ctx);
extern bool8 ScrCmd_specialvar(struct ScriptContext *ctx);
extern void ScriptSetWildEncounterIdentity(struct ScriptContext *ctx);
extern void LoadWallyZigzagoon(void);
static u32 ScriptedTestSpecialIndex(uintptr_t function)
{
    u32 index;
    for (index = 0; gSpecials + index < gSpecials_End && (uintptr_t)gSpecials[index] != function; ++index) {}
    CHECK(gSpecials + index < gSpecials_End && index <= UINT16_MAX);
    return index;
}
static void ScriptedTestCallSpecial(struct ScriptContext *ctx, uintptr_t function)
{
    u32 index = ScriptedTestSpecialIndex(function);
    const u8 operands[] = {index & 255, index >> 8};
    ctx->scriptPtr = operands;
    CHECK(!ScrCmd_special(ctx));
}
static void ScriptedTestCallSpecialVar(struct ScriptContext *ctx, uintptr_t function)
{
    u32 index = ScriptedTestSpecialIndex(function);
    const u8 operands[] = {VAR_RESULT & 255, VAR_RESULT >> 8, index & 255, index >> 8};
    ctx->scriptPtr = operands;
    CHECK(!ScrCmd_specialvar(ctx));
}
extern u16 ScriptSnapshotMewEncounter(void);
extern u16 ScriptRestoreDeoxysRockPalette(void);
extern void DoDeoxysRockInteraction(void);
static u8 sCleanupActor, sCleanupMode;
static s16 sCleanupX, sCleanupY;
static const struct Pokemon *sExpectedTutorialPlayer;
static u32 sExpectedTutorialPlayerCount;
static void CheckTutorialCleanupWorld(void)
{
    CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked()
        && VarGet(VAR_PETALBURG_CITY_STATE) == 6 && VarGet(VAR_PETALBURG_GYM_STATE) == 0
        && FlagGet(FLAG_HIDE_PETALBURG_GYM_WALLY)
        && gPartiesCount[B_TRAINER_PLAYER] == sExpectedTutorialPlayerCount
        && !memcmp(sExpectedTutorialPlayer, gParties[B_TRAINER_PLAYER], sizeof(struct Pokemon) * PARTY_SIZE));
}
static void CheckScriptCleanupWorld(void)
{
    struct ObjectEvent *object = &gObjectEvents[sCleanupActor];
    switch (sCleanupMode)
    {
    case 1: CHECK(object->active && object->invisible); break;
    case 2: CHECK(object->active && object->currentCoords.x == sCleanupX && object->currentCoords.y == sCleanupY
                  && gSprites[object->spriteId].oam.affineMode == ST_OAM_AFFINE_OFF); break;
    case 3: CHECK(object->active && object->currentCoords.x == sCleanupX && object->currentCoords.y == sCleanupY
                  && GetSavedWeather() == WEATHER_RAIN && !VarGet(VAR_TEMP_1)); break;
    case 4: CHECK(!object->active && !FlagGet(FLAG_TEMP_2)); break;
    case 5: CHECK(object->active && object->invisible && object->currentCoords.x == sCleanupX
                  && object->currentCoords.y == sCleanupY && VarGet(VAR_FARAWAY_ISLAND_STEP_COUNTER) == 37); break;
    case 6: {
        u8 rock;
        CHECK(!TryGetObjectEventIdByLocalIdAndMap(LOCALID_BIRTH_ISLAND_EXTERIOR_ROCK,
                    gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup, &rock)
            && gObjectEvents[rock].currentCoords.x == 15 + MAP_OFFSET
            && gObjectEvents[rock].currentCoords.y == 10 + MAP_OFFSET && FlagGet(FLAG_DEOXYS_ROCK_COMPLETE));
        break;
    }
    }
}

static u8 ScriptedTestActor(u32 graphics, u8 localId)
{
    ResetTasks(); ResetSpriteData(); FreeAllSpritePalettes(); ScriptContext_Init(); UnlockPlayerFieldControls();
    memset(gObjectEvents, 0, sizeof(gObjectEvents));
    gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
    gObjectEvents[0].active = TRUE; gObjectEvents[0].isPlayer = TRUE;
    gObjectEvents[0].localId = LOCALID_PLAYER; gObjectEvents[0].spriteId = 0;
    gSprites[0].inUse = TRUE; gSprites[0].callback = SpriteCallbackDummy;
    u8 actor = SpawnSpecialObjectEventParameterized(graphics, MOVEMENT_TYPE_FACE_DOWN,
                                                     localId, MAP_OFFSET + 2, MAP_OFFSET + 2, 3);
    CHECK(actor < OBJECT_EVENTS_COUNT); gSelectedObjectEvent = actor; gSpecialVar_LastTalked = localId;
    return actor;
}
static void CheckScriptedWild(const char *module)
{
    sEnableScriptCleanupProbe = TRUE;
    SetupCancellationFixture(module);
    sEnableScriptCleanupProbe = FALSE; sScriptCleanupCount = 0; sCleanupMode = 0;
    sScriptCleanupPredicate = CheckScriptCleanupWorld;
    const struct RhhWildEncounterService *wild = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 6));
    CHECK(wild && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 7)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 8)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 9)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 10)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 11)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 12)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 13)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 14)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 15)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 16)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 17)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 18)) == &wild->header
        && RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 19)) == &wild->header
        && !RhhModsHostAPI()->get_service("rhh:service/wild_encounter", RHH_SERVICE_VERSION(1, 21)));
    struct RhhWildEncounterContext unavailable = {.struct_size = sizeof(unavailable), .slot = 999};
    CHECK(wild->context(&unavailable) && unavailable.slot == 999);
    struct RhhScriptedEncounterSource noSource = {.struct_size = sizeof(noSource), .encounter = 999};
    CHECK(wild->scripted_source(&noSource) && noSource.encounter == 999);
    void (*const start[])(void) = {BattleSetup_StartScriptedWildBattle, BattleSetup_StartScriptedDoubleWildBattle,
        BattleSetup_StartLatiBattle, BattleSetup_StartLegendaryBattle, StartGroudonKyogreBattle, StartRegiBattle};
    for (u32 i = 0; i < ARRAY_COUNT(start); ++i)
    {
        ResetTasks(); UnlockPlayerFieldControls();
        if (i == 1) CreateScriptedDoubleWildMon(SPECIES_POOCHYENA, 7, ITEM_NONE, SPECIES_ZIGZAGOON, 9, ITEM_NONE);
        else CreateScriptedWildMon(SPECIES_POOCHYENA, 7, ITEM_NONE);
        struct Pokemon before[2]; memcpy(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before));
        start[i]();
        CHECK(!memcmp(before, gParties[B_TRAINER_OPPONENT_A], sizeof(before)));
        CHECK(gTasks[0].isActive);
        CHECK(wild->context(&unavailable) && unavailable.slot == 999);
    }
    /* Exercise the real script operands: one replacement, one invalid key,
     * and a separate denied proposal. Authored levels and held items survive. */
    const u8 pair[] = {SPECIES_POOCHYENA & 255, SPECIES_POOCHYENA >> 8, 7,
        ITEM_POTION & 255, ITEM_POTION >> 8, SPECIES_POOCHYENA & 255, SPECIES_POOCHYENA >> 8, 9,
        ITEM_ORAN_BERRY & 255, ITEM_ORAN_BERRY >> 8};
    struct ScriptContext command = {.scriptPtr = pair};
    sExpectNoAuthoredEncounter = TRUE; sAuthoredEncounterChecks = 0;
    CHECK(!ScrCmd_setwildbattle(&command) && command.scriptPtr == pair + sizeof(pair));
    CHECK(sAuthoredEncounterChecks == 2); sExpectNoAuthoredEncounter = FALSE;
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_LEVEL) == 7
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_HELD_ITEM) == ITEM_POTION
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][1], MON_DATA_SPECIES) == SPECIES_POOCHYENA
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][1], MON_DATA_LEVEL) == 9
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][1], MON_DATA_HELD_ITEM) == ITEM_ORAN_BERRY);
    CHECK(ScriptedWildBattleAllowed() && ScriptedWildBattleAllowed());
    const u8 single[] = {SPECIES_POOCHYENA & 255, SPECIES_POOCHYENA >> 8, 10, 0, 0, 0, 0, 0, 0, 0};
    struct RhhWildCheckpoint beforeSingle, afterSingle;
    ModsWildCheckpointSave(&beforeSingle);
    command = (struct ScriptContext){.scriptPtr = single};
    CHECK(!ScrCmd_setwildbattle(&command));
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_POOCHYENA
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_LEVEL) == 10);
    CHECK(!ScriptedWildBattleAllowed() && ScriptedWildBattleAllowed());
    ModsWildCheckpointSave(&afterSingle);
    CHECK(!memcmp(beforeSingle.enemy, afterSingle.enemy, sizeof(beforeSingle.enemy))
        && beforeSingle.enemyCount == afterSingle.enemyCount && beforeSingle.roamerIndex == afterSingle.roamerIndex
        && !memcmp(&beforeSingle.rng, &afterSingle.rng, sizeof(beforeSingle.rng))
        && !memcmp(&beforeSingle.rng2, &afterSingle.rng2, sizeof(beforeSingle.rng2)));
    const u8 deniedPair[] = {SPECIES_POOCHYENA & 255, SPECIES_POOCHYENA >> 8, 7, 0, 0,
        SPECIES_POOCHYENA & 255, SPECIES_POOCHYENA >> 8, 10, 0, 0};
    command = (struct ScriptContext){.scriptPtr = deniedPair};
    CHECK(!ScrCmd_setwildbattle(&command) && !ScriptedWildBattleAllowed());
    ModsWildCheckpointSave(&afterSingle);
    CHECK(!memcmp(beforeSingle.enemy, afterSingle.enemy, sizeof(beforeSingle.enemy))
        && !memcmp(&beforeSingle.rng, &afterSingle.rng, sizeof(beforeSingle.rng))
        && !memcmp(&beforeSingle.rng2, &afterSingle.rng2, sizeof(beforeSingle.rng2)));
    /* Run a real authored event through lock, guard, release and end. */
    ResetTasks(); ScriptContext_Init(); UnlockPlayerFieldControls();
    gObjectEvents[0].localId = LOCALID_PLAYER; gSelectedObjectEvent = 1;
    FlagClear(FLAG_SYS_CTRL_OBJ_DELETE); FlagClear(FLAG_DEFEATED_ELECTRODE_1_AQUA_HIDEOUT);
    sExpectedAuthoredEncounter = RhhContentKeyOf("rhh:encounter/aqua_hideout_b1f/electrode_1");
    sExpectedAuthoredPreparation = RHH_SCRIPTED_WILD_COMMAND; sAuthoredEncounterChecks = 0;
    ModsWildCheckpointSave(&beforeSingle);
    ScriptContext_SetupScript(AquaHideout_B1F_EventScript_Electrode1);
    for (u32 frame = 0; frame < 100 && ScriptContext_IsEnabled(); ++frame)
    { ScriptContext_RunScript(); RunTasks(); }
    ModsWildCheckpointSave(&afterSingle);
    CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked()
        && !gTasks[0].isActive
        && !FlagGet(FLAG_SYS_CTRL_OBJ_DELETE) && !FlagGet(FLAG_DEFEATED_ELECTRODE_1_AQUA_HIDEOUT)
        && !memcmp(beforeSingle.enemy, afterSingle.enemy, sizeof(beforeSingle.enemy))
        && !memcmp(&beforeSingle.rng, &afterSingle.rng, sizeof(beforeSingle.rng))
        && !memcmp(&beforeSingle.rng2, &afterSingle.rng2, sizeof(beforeSingle.rng2)));
    CHECK(sScriptCleanupCount == 1);
    CHECK(sAuthoredEncounterChecks == 4); sExpectedAuthoredEncounter = 0;
    /* The other same-species actor on the same map has its own stable ID. */
    sExpectedAuthoredEncounter = RhhContentKeyOf("rhh:encounter/aqua_hideout_b1f/electrode_2");
    sAuthoredEncounterChecks = 0; ResetTasks(); ScriptContext_Init(); UnlockPlayerFieldControls();
    ScriptContext_SetupScript(AquaHideout_B1F_EventScript_Electrode2);
    for (u32 frame = 0; frame < 100 && ScriptContext_IsEnabled(); ++frame)
    { ScriptContext_RunScript(); RunTasks(); }
    CHECK(!ScriptContext_IsEnabled() && sScriptCleanupCount == 2 && sAuthoredEncounterChecks == 4);
    sExpectedAuthoredEncounter = 0;
    /* Execute the real annotation and preparation in an allowed script, then
     * invoke its real battle command to check the battle-ready source. */
    u8 *hostRun = RhhModsState("rhh:host_run"); CHECK(hostRun); ClockTestPut64(hostRun, 987);
    RhhModsSetRunContext(987, 456, 0); ResetTasks(); ScriptContext_Init(); UnlockPlayerFieldControls();
    sExpectedAuthoredEncounter = RhhContentKeyOf("rhh:encounter/aqua_hideout_b1f/electrode_1");
    sAuthoredEncounterChecks = 0;
    struct ScriptContext marked; InitScriptContext(&marked, gScriptCmdTable, gScriptCmdTableEnd);
    SetupBytecodeScript(&marked, AquaHideout_B1F_EventScript_PrepareElectrode1);
    CHECK(RunScriptCommand(&marked));
    CHECK(marked.wildPreparedKey == sExpectedAuthoredEncounter && marked.wildPreparedKind == RHH_SCRIPTED_WILD_COMMAND
        && ScrCmd_dowildbattle(&marked) && !marked.wildPreparedKey && sAuthoredEncounterChecks == 3);
    /* Follow the real scheduled task through its handoff, then exercise the
     * start and outcome notifications with initialized battle resources. */
    sExpectedAuthoredBattle = sExpectedAuthoredEncounter; sExpectedAuthoredEncounter = 0;
    sExpectedAuthoredBattlePreparation = RHH_SCRIPTED_WILD_COMMAND; sAuthoredBattleChecks = 0;
    struct BattleStruct battle = {0}; struct BattleStruct *oldBattle = gBattleStruct;
    MainCallback oldCallback = gMain.callback2;
    bool8 oldInBattle = gMain.inBattle; u32 oldCount = gBattlersCount, oldOutcome = gBattleOutcome;
    CHECK(gTasks[0].isActive);
    /* Complete the actual battle-start task and transition, including its
     * handoff to CB2_InitBattle, rather than calling the handoff hook directly. */
    BattleTransition_Start(0);
    CHECK(gTasks[1].isActive);
    gTasks[0].data[0] = 1; gTasks[1].data[15] = 1;
    gTasks[0].func(0);
    CHECK(!gTasks[0].isActive && gMain.callback2 == CB2_InitBattle);
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattlersCount = 2; gBattleOutcome = 0;
    gBattlerPositions[0] = 0; gBattlerPositions[1] = 1;
    gBattlerPartyIndexes[0] = gBattlerPartyIndexes[1] = 0;
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    ModsBattleBegin();
    struct RhhScriptedEncounterSource battleSource = {.struct_size = sizeof(battleSource)};
    CHECK(!wild->scripted_battle_source(&battleSource) && battleSource.encounter == sExpectedAuthoredBattle);
    struct RhhScriptedEncounterSource ticketSource = battleSource;
    gBattleOutcome = B_OUTCOME_CAUGHT; ModsBattleEnd(gBattleOutcome, FALSE);
    CHECK(sAuthoredBattleChecks == 2 && wild->scripted_battle_source(&battleSource));
    sExpectedAuthoredBattle = 0;
    /* A replacement battle initializer cannot consume the preceding ticket. */
    ModsScriptedBattleSchedule(7, battleSource); ModsScriptedBattleTransitionComplete(7);
    SetMainCallback2(CB2_InitBattle); SetMainCallback2(CB2_InitBattle);
    gBattleOutcome = 0; ModsBattleBegin();
    battleSource.encounter = 999;
    CHECK(wild->scripted_battle_source(&battleSource) && battleSource.encounter == 999);
    ModsBattleEnd(0, TRUE);
    /* Wrong task and changed run invalidate a transition as well. */
    ModsScriptedBattleSchedule(7, ticketSource); ModsScriptedBattleTransitionComplete(8);
    SetMainCallback2(CB2_InitBattle); ModsBattleBegin();
    CHECK(wild->scripted_battle_source(&battleSource) && battleSource.encounter == 999);
    ModsBattleEnd(0, TRUE);
    ModsScriptedBattleSchedule(7, ticketSource); ModsScriptedBattleTransitionComplete(7);
    SetMainCallback2(CB2_InitBattle); RhhModsSetRunContext(987, 457, 0); ModsBattleBegin();
    CHECK(wild->scripted_battle_source(&battleSource) && battleSource.encounter == 999);
    ModsBattleEnd(0, TRUE); RhhModsSetRunContext(987, 456, 0);
    /* Follow the event-mon script through its actual legendary special. */
    ResetTasks(); gMain.inBattle = FALSE;
    bool32 hadObjectDelete = FlagGet(FLAG_SYS_CTRL_OBJ_DELETE);
    sExpectedAuthoredEncounter = RhhContentKeyOf("rhh:encounter/navel_rock_bottom/lugia");
    sExpectedAuthoredPreparation = RHH_SCRIPTED_EVENT_MON; sAuthoredEncounterChecks = 0;
    InitScriptContext(&marked, gScriptCmdTable, gScriptCmdTableEnd);
    SetupBytecodeScript(&marked, NavelRock_Bottom_EventScript_PrepareLugia);
    for (u32 i = 0; i < 30 && !gTasks[0].isActive; ++i)
    {
        u8 commandCode = *marked.scriptPtr++;
        CHECK(marked.cmdTable + commandCode < marked.cmdTableEnd);
        CHECK(!marked.cmdTable[commandCode](&marked));
    }
    CHECK(gTasks[0].isActive && !marked.wildPreparedKey && sAuthoredEncounterChecks == 3);
    sExpectedAuthoredBattle = sExpectedAuthoredEncounter; sExpectedAuthoredEncounter = 0;
    sExpectedAuthoredBattlePreparation = RHH_SCRIPTED_EVENT_MON; sAuthoredBattleChecks = 0;
    ModsScriptedBattleTransitionComplete(0); SetMainCallback2(CB2_InitBattle);
    gMain.inBattle = TRUE; gBattleOutcome = 0;
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    ModsBattleBegin();
    CHECK(!wild->scripted_battle_source(&battleSource) && battleSource.encounter == sExpectedAuthoredBattle
        && battleSource.preparation == RHH_SCRIPTED_EVENT_MON);
    gBattleOutcome = B_OUTCOME_CAUGHT; ModsBattleEnd(gBattleOutcome, FALSE);
    CHECK(sAuthoredBattleChecks == 2 && wild->scripted_battle_source(&battleSource));
    sExpectedAuthoredBattle = 0;
    if (!hadObjectDelete) FlagClear(FLAG_SYS_CTRL_OBJ_DELETE);
    /* Ghost creation differs with the Silph Scope; tutorials create their
     * opponents directly. Each exposes its real final foe and source. */
    const char *const specialIds[] = {"rhh:encounter/pokemon_tower_6f_frlg/marowak",
        "rhh:encounter/pokemon_tower_6f_frlg/marowak", "rhh:encounter/petalburg_city/wally_tutorial",
        "rhh:encounter/viridian_city_frlg/old_man_tutorial"};
    const u32 specialSpecies[] = {SPECIES_ZIGZAGOON, SPECIES_MAROWAK, SPECIES_RALTS, SPECIES_WEEDLE};
    /* Deny the actual prepared ghost, both before and after Silph Scope's
     * replacement, and verify the original checkpoint is restored. */
    ClockTestPut64(hostRun, 991); RhhModsSetRunContext(991, 456, 0);
    CHECK(!CheckBagHasItem(ITEM_SILPH_SCOPE, 1));
    for (u32 scope = 0; scope < 2; ++scope)
    {
        if (scope) CHECK(AddBagItem(ITEM_SILPH_SCOPE, 1));
        ResetTasks(); gMain.inBattle = FALSE;
        sExpectedAuthoredEncounter = RhhContentKeyOf(specialIds[0]);
        sExpectedAuthoredPreparation = RHH_SCRIPTED_WILD_COMMAND; sAuthoredEncounterChecks = 0;
        InitScriptContext(&marked, gScriptCmdTable, gScriptCmdTableEnd);
        marked.wildPendingKey = sExpectedAuthoredEncounter;
        struct RhhWildCheckpoint beforeGhost, afterGhost; ModsWildCheckpointSave(&beforeGhost);
        const u8 operands[] = {SPECIES_MAROWAK & 255, SPECIES_MAROWAK >> 8, 30, 0, 0, 0, 0, 0, 0, 0};
        marked.scriptPtr = operands; CHECK(!ScrCmd_setwildbattle(&marked));
        ScriptedTestCallSpecial(&marked, (uintptr_t)PrepareMarowakBattle);
        CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES)
            == (scope ? SPECIES_MAROWAK : SPECIES_ZIGZAGOON));
        ScriptedTestCallSpecialVar(&marked, (uintptr_t)ScriptedWildBattleAllowed);
        CHECK(!gSpecialVar_Result && !marked.wildPreparedKey && sAuthoredEncounterChecks == 3
            && !gTasks[0].isActive);
        ModsWildCheckpointSave(&afterGhost);
        CHECK(!memcmp(beforeGhost.enemy, afterGhost.enemy, sizeof(beforeGhost.enemy))
            && beforeGhost.enemyCount == afterGhost.enemyCount && beforeGhost.roamerIndex == afterGhost.roamerIndex
            && !memcmp(&beforeGhost.rng, &afterGhost.rng, sizeof(beforeGhost.rng))
            && !memcmp(&beforeGhost.rng2, &afterGhost.rng2, sizeof(beforeGhost.rng2)));
        sExpectedAuthoredEncounter = 0;
        if (scope) CHECK(RemoveBagItem(ITEM_SILPH_SCOPE, 1));
    }
    ClockTestPut64(hostRun, 989); RhhModsSetRunContext(989, 456, 0);
    CHECK(!CheckBagHasItem(ITEM_SILPH_SCOPE, 1));
    for (u32 i = 0; i < ARRAY_COUNT(specialIds); ++i)
    {
        if (i == 1) CHECK(AddBagItem(ITEM_SILPH_SCOPE, 1));
        ResetTasks(); gMain.inBattle = FALSE;
        sExpectedAuthoredEncounter = RhhContentKeyOf(specialIds[i]);
        sExpectedAuthoredPreparation = i < 2 ? RHH_SCRIPTED_WILD_COMMAND : RHH_SCRIPTED_DIRECT_SPECIAL;
        sAuthoredEncounterChecks = 0;
        InitScriptContext(&marked, gScriptCmdTable, gScriptCmdTableEnd);
        if (i == 2)
        {
            SetupBytecodeScript(&marked, PetalburgCity_EventScript_PrepareWallyTutorial);
            for (u32 j = 0; j < 30 && !gTasks[0].isActive; ++j)
            {
                u8 commandCode = *marked.scriptPtr++;
                CHECK(marked.cmdTable + commandCode < marked.cmdTableEnd);
                CHECK(!marked.cmdTable[commandCode](&marked));
            }
        }
        else
        {
            /* FRLG scripts are not included in an Emerald build; exercise
             * their same preparation and special commands directly. */
            marked.wildPendingKey = sExpectedAuthoredEncounter;
            if (i < 2)
            {
                const u8 operands[] = {SPECIES_MAROWAK & 255, SPECIES_MAROWAK >> 8, 30, 0, 0, 0, 0, 0, 0, 0};
                marked.scriptPtr = operands;
                CHECK(!ScrCmd_setwildbattle(&marked));
                ScriptedTestCallSpecial(&marked, (uintptr_t)PrepareMarowakBattle);
                ScriptedTestCallSpecialVar(&marked, (uintptr_t)ScriptedWildBattleAllowed);
                CHECK(gSpecialVar_Result);
                ScriptedTestCallSpecial(&marked, (uintptr_t)StartPreparedMarowakBattle);
            }
            else ScriptedTestCallSpecial(&marked, (uintptr_t)StartOldManTutorialBattle);
        }
        CHECK(gTasks[0].isActive && !marked.wildPendingKey && !marked.wildPreparedKey
            && sAuthoredEncounterChecks == (i < 2 ? 3u : (i == 2 ? 2u : 1u))
            && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == specialSpecies[i]);
        if (i < 2) CHECK(ScriptedWildBattleAllowed());
        sExpectedAuthoredBattle = sExpectedAuthoredEncounter; sExpectedAuthoredEncounter = 0;
        sExpectedAuthoredBattlePreparation = sExpectedAuthoredPreparation; sAuthoredBattleChecks = 0;
        ModsScriptedBattleTransitionComplete(0); SetMainCallback2(CB2_InitBattle);
        gMain.inBattle = TRUE; gBattleOutcome = 0;
        PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
        ModsBattleBegin();
        CHECK(!wild->scripted_battle_source(&battleSource) && battleSource.encounter == sExpectedAuthoredBattle
            && battleSource.preparation == sExpectedAuthoredPreparation);
        gBattleOutcome = B_OUTCOME_WON; ModsBattleEnd(gBattleOutcome, FALSE);
        CHECK(sAuthoredBattleChecks == 2 && wild->scripted_battle_source(&battleSource));
        sExpectedAuthoredBattle = 0;
        if (i == 1) CHECK(RemoveBagItem(ITEM_SILPH_SCOPE, 1));
    }
    /* An unannotated direct tutorial cannot inherit the preceding ID. */
    ResetTasks(); gMain.inBattle = FALSE; StartWallyTutorialBattle();
    ModsScriptedBattleTransitionComplete(0); SetMainCallback2(CB2_InitBattle);
    gMain.inBattle = TRUE; gBattleOutcome = 0; ModsBattleBegin();
    battleSource.encounter = 999;
    CHECK(wild->scripted_battle_source(&battleSource) && battleSource.encounter == 999);
    ModsBattleEnd(0, TRUE);
    /* The actual script hands off through starter selection and the first
     * battle transition. The engine creates Zigzagoon during battle setup. */
    ClockTestPut64(hostRun, 990); RhhModsSetRunContext(990, 456, 0);
    ResetTasks(); gMain.inBattle = FALSE;
    struct Pokemon savedPlayerParty[PARTY_SIZE];
    memcpy(savedPlayerParty, gParties[B_TRAINER_PLAYER], sizeof(savedPlayerParty));
    u32 savedPlayerCount = gPartiesCount[B_TRAINER_PLAYER];
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(savedPlayerParty)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    u16 oldStarter = VarGet(VAR_STARTER_MON), oldResult = gSpecialVar_Result;
    MainCallback oldSavedCallback = gMain.savedCallback;
    InitScriptContext(&marked, gScriptCmdTable, gScriptCmdTableEnd);
    SetupBytecodeScript(&marked, Route101_EventScript_PrepareStarterChoice);
    for (u32 j = 0; j < 10 && gMain.callback2 != CB2_ChooseStarter; ++j)
    {
        u8 commandCode = *marked.scriptPtr++;
        CHECK(marked.cmdTable + commandCode < marked.cmdTableEnd);
        CHECK(!marked.cmdTable[commandCode](&marked));
    }
    CHECK(gMain.callback2 == CB2_ChooseStarter && gMain.savedCallback && !marked.wildPendingKey && !marked.wildPreparedKey);
    gSpecialVar_Result = 0; gMain.savedCallback();
    CHECK(gPartiesCount[B_TRAINER_PLAYER] == 1 && gTasks[0].isActive);
    gTasks[0].data[0] = 3; gMain.callback2();
    CHECK(gMain.callback2 == CB2_InitBattle && gBattleTypeFlags == BATTLE_TYPE_FIRST_BATTLE);
    struct BattleHistory history = {0}; struct AiThinkingStruct thinking = {0};
    struct BattleHistory *oldHistory = gBattleHistory; struct AiThinkingStruct *oldThinking = gAiThinkingStruct;
    gBattleHistory = &history; gAiThinkingStruct = &thinking;
    SetUpBattleVarsAndBirchZigzagoon();
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_LEVEL) == 2);
    sExpectedAuthoredEncounter = RhhContentKeyOf("rhh:encounter/route101/birch_rescue");
    sExpectedAuthoredPreparation = RHH_SCRIPTED_DIRECT_SPECIAL; sAuthoredEncounterChecks = 0;
    ModsWildFirstBattleReady(); CHECK(sAuthoredEncounterChecks == 1);
    sExpectedAuthoredBattle = sExpectedAuthoredEncounter; sExpectedAuthoredEncounter = 0;
    sExpectedAuthoredBattlePreparation = RHH_SCRIPTED_DIRECT_SPECIAL; sAuthoredBattleChecks = 0;
    gMain.inBattle = TRUE; gBattleOutcome = 0; gBattlersCount = 2;
    gBattlerPositions[0] = 0; gBattlerPositions[1] = 1;
    gBattlerPartyIndexes[0] = gBattlerPartyIndexes[1] = 0;
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    ModsBattleBegin(); ModsWildFirstBattleReady();
    CHECK(sAuthoredEncounterChecks == 1 && !wild->scripted_battle_source(&battleSource)
        && battleSource.encounter == sExpectedAuthoredBattle);
    gBattleOutcome = B_OUTCOME_WON; ModsBattleEnd(gBattleOutcome, FALSE);
    CHECK(sAuthoredBattleChecks == 2 && wild->scripted_battle_source(&battleSource));
    sExpectedAuthoredBattle = 0;
    /* An unannotated selection is still recognized as the first battle but
     * cannot inherit its predecessor's encounter key. */
    ModsFirstBattleSelect((struct RhhScriptedEncounterSource){0});
    ModsFirstBattleHandoff(); SetMainCallback2(CB2_InitBattle);
    struct RhhScriptedEncounterSource firstSource = {.struct_size = sizeof(firstSource), .encounter = 999};
    CHECK(ModsFirstBattleInitializing(&firstSource) && !firstSource.encounter);
    gBattleOutcome = 0; ModsBattleBegin(); battleSource.encounter = 999;
    CHECK(wild->scripted_battle_source(&battleSource) && battleSource.encounter == 999);
    ModsBattleEnd(0, TRUE);
    /* A run change between selection and the transition rejects the ticket. */
    ModsFirstBattleSelect(ticketSource); RhhModsSetRunContext(990, 457, 0);
    ModsFirstBattleHandoff(); SetMainCallback2(CB2_InitBattle);
    firstSource.encounter = 999;
    CHECK(!ModsFirstBattleInitializing(&firstSource) && firstSource.encounter == 999);
    RhhModsSetRunContext(990, 456, 0);
    gBattleHistory = oldHistory; gAiThinkingStruct = oldThinking; gMain.savedCallback = oldSavedCallback;
    memcpy(gParties[B_TRAINER_PLAYER], savedPlayerParty, sizeof(savedPlayerParty)); gPartiesCount[B_TRAINER_PLAYER] = savedPlayerCount;
    VarSet(VAR_STARTER_MON, oldStarter); gSpecialVar_Result = oldResult;
    gBattleStruct = oldBattle; gMain.inBattle = oldInBattle; gBattlersCount = oldCount; gBattleOutcome = oldOutcome;
    SetMainCallback2(oldCallback);
    ClockTestPut64(hostRun, 123); RhhModsSetRunContext(123, 456, 0);
    CHECK(ModsWildScriptedKey("test:encounter/a") == RhhContentKeyOf("test:encounter/a"));
    CHECK(!ModsWildScriptedKey("test:encounter/") && !ModsWildScriptedKey("test:other/a")
        && !ModsWildScriptedKey("Test:encounter/a") && !ModsWildScriptedKey("test:encounter/a:b"));
    ResetTasks(); ScriptContext_Init(); UnlockPlayerFieldControls();
    /* Both Kecleon scripts must hide the revealed actor again on denial. */
    const u8 *const kecleonScripts[] = {EventScript_PrepareKecleon, Route120_EventScript_PrepareBridgeKecleon};
    for (u32 i = 0; i < ARRAY_COUNT(kecleonScripts); ++i)
    {
        ResetTasks(); ResetSpriteData(); FreeAllSpritePalettes(); ScriptContext_Init(); UnlockPlayerFieldControls();
        memset(gObjectEvents, 0, sizeof(gObjectEvents));
        gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
        gObjectEvents[0].active = TRUE; gObjectEvents[0].isPlayer = TRUE;
        gObjectEvents[0].localId = LOCALID_PLAYER; gObjectEvents[0].spriteId = 0;
        gSprites[0].inUse = TRUE; gSprites[0].callback = SpriteCallbackDummy;
        u8 localId = i ? LOCALID_BRIDGE_KECLEON : 12;
        u8 actor = SpawnSpecialObjectEventParameterized(OBJ_EVENT_GFX_KECLEON, MOVEMENT_TYPE_FACE_DOWN,
                                                         localId, MAP_OFFSET + 2, MAP_OFFSET + 2, 3);
        CHECK(actor < OBJECT_EVENTS_COUNT); gSelectedObjectEvent = actor; gSpecialVar_LastTalked = localId;
        gObjectEvents[actor].invisible = FALSE; FlagClear(FLAG_SYS_CTRL_OBJ_DELETE);
        sCleanupMode = 1; sCleanupActor = actor; u32 cleanupsBefore = sScriptCleanupCount;
        ModsWildCheckpointSave(&beforeSingle); ScriptContext_SetupScript(kecleonScripts[i]);
        for (u32 frame = 0; frame < 100 && ScriptContext_IsEnabled(); ++frame)
        {
            ScriptContext_RunScript();
            if (!i && !frame && ScriptContext_IsEnabled())
            {
                ScriptContext_Stop(); CHECK(!ScriptContext_RunScript() && sScriptCleanupCount == cleanupsBefore);
                ScriptContext_Enable();
            }
            RunTasks();
            struct Sprite *sprite = &gSprites[gObjectEvents[actor].spriteId];
            sprite->callback(sprite); AnimateSprite(sprite);
            if (ScriptContext_IsEnabled()) CHECK(sScriptCleanupCount == cleanupsBefore);
        }
        ModsWildCheckpointSave(&afterSingle);
        CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked()
            && gObjectEvents[actor].active && gObjectEvents[actor].invisible && !FlagGet(FLAG_SYS_CTRL_OBJ_DELETE)
            && !memcmp(beforeSingle.enemy, afterSingle.enemy, sizeof(beforeSingle.enemy))
            && !memcmp(&beforeSingle.rng, &afterSingle.rng, sizeof(beforeSingle.rng))
            && !memcmp(&beforeSingle.rng2, &afterSingle.rng2, sizeof(beforeSingle.rng2)));
        CHECK(sScriptCleanupCount == cleanupsBefore + 1); sCleanupMode = 0;
    }
    /* Replacing or reinitializing the global script, changing maps, or
     * replacing the run cannot report cleanup for an abandoned script. */
    for (u32 mode = 0; mode < 6; ++mode)
    {
        u8 actor = ScriptedTestActor(OBJ_EVENT_GFX_KECLEON, 12);
        gObjectEvents[actor].invisible = FALSE; sCleanupMode = 0;
        u32 cleanupsBefore = sScriptCleanupCount;
        ScriptContext_SetupScript(EventScript_PrepareKecleon);
        CHECK(ScriptContext_RunScript() && ScriptContext_IsEnabled() && sScriptCleanupCount == cleanupsBefore);
        const u8 end[] = {SCR_OP_END};
        if (mode == 0) { ScriptContext_Init(); ScriptContext_SetupScript(end); }
        else if (mode == 1) ScriptContext_SetupScript(end);
        else if (mode == 2)
        {
            struct ScriptContext replacement;
            InitScriptContext(&replacement, gScriptCmdTable, gScriptCmdTableEnd);
            SetupBytecodeScript(&replacement, end); ScriptContext_ContinueScript(&replacement);
        }
        else if (mode == 3) ++gSaveBlock1Ptr->location.mapNum;
        else if (mode == 4) { RhhModsSetRunContext(123, 999, 0); RhhModsSetRunContext(123, 456, 0); }
        else
        {
            struct ScriptStack replacement; InitScriptStack(&replacement); CHECK(ScriptStackPush(&replacement, end));
            ScriptContext_SetupGlobalContextFromStack(&replacement);
        }
        for (u32 frame = 0; frame < 100 && ScriptContext_IsEnabled(); ++frame)
        {
            ScriptContext_RunScript(); RunTasks();
            struct Sprite *sprite = &gSprites[gObjectEvents[actor].spriteId];
            sprite->callback(sprite); AnimateSprite(sprite);
        }
        CHECK(!ScriptContext_IsEnabled() && sScriptCleanupCount == cleanupsBefore);
        if (mode == 3) --gSaveBlock1Ptr->location.mapNum;
    }
    /* A local script can execute while the global one is suspended; its veto
     * must not be attached to the unrelated global script's end. */
    {
        ScriptedTestActor(OBJ_EVENT_GFX_KECLEON, 12);
        const u8 end[] = {SCR_OP_END}; u32 cleanupsBefore = sScriptCleanupCount;
        ScriptContext_SetupScript(end); ScriptContext_Stop();
        struct ScriptContext local; InitScriptContext(&local, gScriptCmdTable, gScriptCmdTableEnd);
        SetupBytecodeScript(&local, EventScript_PrepareKecleon);
        CHECK(RunScriptCommand(&local)); ScriptContext_Enable(); ScriptContext_RunScript();
        CHECK(!ScriptContext_IsEnabled() && sScriptCleanupCount == cleanupsBefore);
    }
    /* A return from an empty script stack is not an explicit end. Two
     * denials in one script report the first one exactly once. */
    {
        CHECK(EventScript_PrepareKecleon[0] == SCR_OP_SETWILDBATTLE
            && EventScript_PrepareKecleon[11] == SCR_OP_SPECIALVAR);
        u8 aborted[17], twice[33];
        memcpy(aborted, EventScript_PrepareKecleon, 16); aborted[16] = SCR_OP_RETURN;
        memcpy(twice, EventScript_PrepareKecleon, 16); memcpy(twice + 16, EventScript_PrepareKecleon, 16);
        twice[17] = SPECIES_ELECTRODE & 255; twice[18] = SPECIES_ELECTRODE >> 8; twice[32] = SCR_OP_END;
        ScriptedTestActor(OBJ_EVENT_GFX_KECLEON, 12); u32 cleanupsBefore = sScriptCleanupCount;
        ScriptContext_SetupScript(aborted); ScriptContext_RunScript();
        CHECK(!ScriptContext_IsEnabled() && sScriptCleanupCount == cleanupsBefore);
        sExpectedScriptCleanupSubject = RhhContentKeyForId(RHH_SPECIES, SPECIES_KECLEON);
        ScriptContext_SetupScript(twice); ScriptContext_RunScript();
        CHECK(!ScriptContext_IsEnabled() && sScriptCleanupCount == cleanupsBefore + 1);
        sExpectedScriptCleanupSubject = 0;
    }
    /* Event Pokémon keep their authored item and fateful flag, and the
     * separate proposal can replace their species before creation. */
    gSpecialVar_0x8004 = SPECIES_POOCHYENA; gSpecialVar_0x8005 = 7; gSpecialVar_0x8006 = ITEM_POTION;
    CreateEnemyEventMon();
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_HELD_ITEM) == ITEM_POTION
        && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_MODERN_FATEFUL_ENCOUNTER)
        && ScriptedWildBattleAllowed());
    for (u32 level = 9; level <= 11; level += 2)
    {
        gSpecialVar_0x8005 = level; CreateEnemyEventMon();
        CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_POOCHYENA
            && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_LEVEL) == level
            && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_HELD_ITEM) == ITEM_POTION
            && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_MODERN_FATEFUL_ENCOUNTER)
            && ScriptedWildBattleAllowed());
    }
    ModsWildCheckpointSave(&beforeSingle);
    gSpecialVar_0x8005 = 10;
    CreateEnemyEventMon();
    CHECK(!ScriptedWildBattleAllowed());
    ModsWildCheckpointSave(&afterSingle);
    CHECK(!memcmp(beforeSingle.enemy, afterSingle.enemy, sizeof(beforeSingle.enemy))
        && !memcmp(&beforeSingle.rng, &afterSingle.rng, sizeof(beforeSingle.rng))
        && !memcmp(&beforeSingle.rng2, &afterSingle.rng2, sizeof(beforeSingle.rng2)));
    /* Exercise the real event-mon script and its guarded exit. */
    ResetTasks(); ScriptContext_Init(); UnlockPlayerFieldControls();
    FlagClear(FLAG_SYS_CTRL_OBJ_DELETE); FlagClear(FLAG_CAUGHT_LUGIA); FlagClear(FLAG_DEFEATED_LUGIA);
    sExpectedAuthoredEncounter = RhhContentKeyOf("rhh:encounter/navel_rock_bottom/lugia");
    sExpectedAuthoredPreparation = RHH_SCRIPTED_EVENT_MON; sAuthoredEncounterChecks = 0;
    ModsWildCheckpointSave(&beforeSingle);
    ScriptContext_SetupScript(NavelRock_Bottom_EventScript_PrepareLugia);
    for (u32 frame = 0; frame < 100 && ScriptContext_IsEnabled(); ++frame)
    { ScriptContext_RunScript(); RunTasks(); }
    ModsWildCheckpointSave(&afterSingle);
    CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked()
        && !FlagGet(FLAG_SYS_CTRL_OBJ_DELETE) && !FlagGet(FLAG_CAUGHT_LUGIA) && !FlagGet(FLAG_DEFEATED_LUGIA)
        && !memcmp(beforeSingle.enemy, afterSingle.enemy, sizeof(beforeSingle.enemy))
        && !memcmp(&beforeSingle.rng, &afterSingle.rng, sizeof(beforeSingle.rng))
        && !memcmp(&beforeSingle.rng2, &afterSingle.rng2, sizeof(beforeSingle.rng2)));
    CHECK(sAuthoredEncounterChecks == 4); sExpectedAuthoredEncounter = 0;
    /* The cave legends have already moved and acquired affine matrices when
     * their battle is prepared. A veto must restore the actor and trigger. */
    const u8 *const caveScripts[] = {TerraCave_End_EventScript_PrepareGroudon, MarineCave_End_EventScript_PrepareKyogre};
    const u32 caveGraphics[] = {OBJ_EVENT_GFX_GROUDON_FRONT, OBJ_EVENT_GFX_KYOGRE_FRONT};
    const u16 caveX[] = {17, 9};
    for (u32 i = 0; i < ARRAY_COUNT(caveScripts); ++i)
    {
        u8 actor = ScriptedTestActor(caveGraphics[i], 1);
        struct Sprite *sprite = &gSprites[gObjectEvents[actor].spriteId];
        sprite->oam.affineMode = ST_OAM_AFFINE_DOUBLE; InitSpriteAffineAnim(sprite);
        CHECK(sprite->oam.affineMode != ST_OAM_AFFINE_OFF);
        sprite->subspriteMode = SUBSPRITES_OFF; VarSet(VAR_TEMP_1, 1);
        sCleanupMode = 2; sCleanupActor = actor; sCleanupX = caveX[i] + MAP_OFFSET; sCleanupY = 22 + MAP_OFFSET;
        FlagClear(FLAG_SYS_CTRL_OBJ_DELETE); FlagClear(FLAG_DEFEATED_GROUDON); FlagClear(FLAG_DEFEATED_KYOGRE);
        ModsWildCheckpointSave(&beforeSingle); ScriptContext_SetupScript(caveScripts[i]);
        ScriptContext_RunScript(); ModsWildCheckpointSave(&afterSingle);
        CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked()
            && gObjectEvents[actor].currentCoords.x == caveX[i] + MAP_OFFSET
            && gObjectEvents[actor].currentCoords.y == 22 + MAP_OFFSET
            && sprite->oam.affineMode == ST_OAM_AFFINE_OFF && sprite->subspriteMode == SUBSPRITES_ON
            && VarGet(VAR_TEMP_1) == 1 && !FlagGet(FLAG_SYS_CTRL_OBJ_DELETE)
            && !FlagGet(FLAG_DEFEATED_GROUDON) && !FlagGet(FLAG_DEFEATED_KYOGRE)
            && !memcmp(beforeSingle.enemy, afterSingle.enemy, sizeof(beforeSingle.enemy))
            && !memcmp(&beforeSingle.rng, &afterSingle.rng, sizeof(beforeSingle.rng))
            && !memcmp(&beforeSingle.rng2, &afterSingle.rng2, sizeof(beforeSingle.rng2)));
        sCleanupMode = 0;
    }
    /* Exhaustion must not let a failed affine setup or cancellation release
     * another sprite's matrix. */
    {
        u8 actor = ScriptedTestActor(OBJ_EVENT_GFX_GROUDON_FRONT, LOCALID_TERRA_CAVE_GROUDON);
        struct Sprite *sprite = &gSprites[gObjectEvents[actor].spriteId];
        for (u32 i = 0; i < OAM_MATRIX_COUNT; ++i) CHECK(AllocOamMatrix() != 0xFF);
        CHECK(AllocOamMatrix() == 0xFF);
        CHECK(MovementAction_InitAffineAnim_Step0(&gObjectEvents[actor], sprite)
            && sprite->oam.affineMode == ST_OAM_AFFINE_OFF);
        CHECK(MovementAction_ClearAffineAnim_Step0(&gObjectEvents[actor], sprite)
            && AllocOamMatrix() == 0xFF);
        sCleanupMode = 2; sCleanupActor = actor; sCleanupX = 17 + MAP_OFFSET; sCleanupY = 22 + MAP_OFFSET;
        VarSet(VAR_TEMP_1, 1); ScriptContext_SetupScript(TerraCave_End_EventScript_PrepareGroudon);
        ScriptContext_RunScript();
        CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked()
            && sprite->oam.affineMode == ST_OAM_AFFINE_OFF && AllocOamMatrix() == 0xFF);
        sCleanupMode = 0;
        for (u32 i = 0; i < OAM_MATRIX_COUNT; ++i) FreeOamMatrix(i);
    }
    /* Ho-Oh also needs the original weather and the map trigger restored. */
    const u8 *const hoOhScripts[] = {NavelRock_Top_EventScript_PrepareHoOh};
    const u16 hoOhX[] = {12}, hoOhY[] = {4};
    for (u32 i = 0; i < ARRAY_COUNT(hoOhScripts); ++i)
    {
        u8 actor = ScriptedTestActor(OBJ_EVENT_GFX_HOOH, LOCALID_NAVEL_ROCK_HO_OH);
        sCleanupMode = 3; sCleanupActor = actor; sCleanupX = hoOhX[i] + MAP_OFFSET; sCleanupY = hoOhY[i] + MAP_OFFSET;
        VarSet(VAR_TEMP_1, 1); VarSet(VAR_TEMP_2, WEATHER_RAIN); gSaveBlock1Ptr->weather = WEATHER_NONE;
        u32 rainCount = GetGameStat(GAME_STAT_GOT_RAINED_ON);
        FlagClear(FLAG_SYS_CTRL_OBJ_DELETE); FlagClear(FLAG_CAUGHT_HO_OH); FlagClear(FLAG_DEFEATED_HO_OH);
        ModsWildCheckpointSave(&beforeSingle); ScriptContext_SetupScript(hoOhScripts[i]);
        ScriptContext_RunScript(); ModsWildCheckpointSave(&afterSingle);
        CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked()
            && gObjectEvents[actor].currentCoords.x == hoOhX[i] + MAP_OFFSET
            && gObjectEvents[actor].currentCoords.y == hoOhY[i] + MAP_OFFSET
            && VarGet(VAR_TEMP_1) == 0 && GetSavedWeather() == WEATHER_RAIN
            && GetGameStat(GAME_STAT_GOT_RAINED_ON) == rainCount
            && !FlagGet(FLAG_SYS_CTRL_OBJ_DELETE) && !FlagGet(FLAG_CAUGHT_HO_OH) && !FlagGet(FLAG_DEFEATED_HO_OH)
            && !memcmp(beforeSingle.enemy, afterSingle.enemy, sizeof(beforeSingle.enemy))
            && !memcmp(&beforeSingle.rng, &afterSingle.rng, sizeof(beforeSingle.rng))
            && !memcmp(&beforeSingle.rng2, &afterSingle.rng2, sizeof(beforeSingle.rng2)));
        sCleanupMode = 0;
    }
    /* Southern Island's spawned Pokémon and temporary flag are unwound,
     * while pre-existing hide and encountered flags are left untouched. */
    for (u32 i = 0; i < 2; ++i)
    {
        u8 actor = ScriptedTestActor(OBJ_EVENT_GFX_LATIOS, LOCALID_SOUTHERN_ISLAND_LATI);
        sCleanupMode = 4; sCleanupActor = actor;
        sExpectedAuthoredEncounter = RhhContentKeyOf("rhh:encounter/southern_island_interior/lati");
        sExpectedAuthoredPreparation = RHH_SCRIPTED_EVENT_MON; sAuthoredEncounterChecks = 0;
        VarSet(VAR_ROAMER_POKEMON, i); FlagSet(FLAG_TEMP_2); FlagClear(FLAG_SYS_CTRL_OBJ_DELETE);
        if (i) { FlagSet(FLAG_HIDE_SOUTHERN_ISLAND_UNCHOSEN_EON_DUO_MON); FlagSet(FLAG_ENCOUNTERED_LATIAS_OR_LATIOS); }
        else { FlagClear(FLAG_HIDE_SOUTHERN_ISLAND_UNCHOSEN_EON_DUO_MON); FlagClear(FLAG_ENCOUNTERED_LATIAS_OR_LATIOS); }
        ModsWildCheckpointSave(&beforeSingle); ScriptContext_SetupScript(SouthernIsland_Interior_EventScript_PrepareLati);
        ScriptContext_RunScript(); ModsWildCheckpointSave(&afterSingle);
        CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked() && !gObjectEvents[actor].active
            && !FlagGet(FLAG_TEMP_2) && !FlagGet(FLAG_SYS_CTRL_OBJ_DELETE)
            && !!FlagGet(FLAG_HIDE_SOUTHERN_ISLAND_UNCHOSEN_EON_DUO_MON) == !!i
            && !!FlagGet(FLAG_ENCOUNTERED_LATIAS_OR_LATIOS) == !!i
            && !memcmp(beforeSingle.enemy, afterSingle.enemy, sizeof(beforeSingle.enemy))
            && !memcmp(&beforeSingle.rng, &afterSingle.rng, sizeof(beforeSingle.rng))
            && !memcmp(&beforeSingle.rng2, &afterSingle.rng2, sizeof(beforeSingle.rng2)));
        CHECK(sAuthoredEncounterChecks == 4); sExpectedAuthoredEncounter = 0;
        sCleanupMode = 0;
    }
    /* Mew's emergence temporarily moves and reveals it, suppresses the
     * chase counter, and changes its sprite priority. A veto resumes the
     * original hide-and-seek state. */
    {
        u8 actor = ScriptedTestActor(OBJ_EVENT_GFX_MEW, LOCALID_FARAWAY_ISLAND_MEW);
        struct ObjectEvent *mew = &gObjectEvents[actor]; struct Sprite *sprite = &gSprites[mew->spriteId];
        mew->invisible = TRUE; mew->fixedPriority = FALSE; mew->facingDirection = DIR_EAST;
        mew->movementDirection = DIR_NORTH; mew->previousMovementDirection = DIR_WEST;
        sprite->subspriteMode = SUBSPRITES_ON; sprite->subspriteTableNum = 2; sprite->subpriority = 9;
        VarSet(VAR_FARAWAY_ISLAND_STEP_COUNTER, 37); FlagClear(FLAG_SYS_CTRL_OBJ_DELETE);
        FlagClear(FLAG_CAUGHT_MEW); FlagClear(FLAG_DEFEATED_MEW);
        s16 x = mew->currentCoords.x, y = mew->currentCoords.y;
        sCleanupMode = 5; sCleanupActor = actor; sCleanupX = x; sCleanupY = y;
        CHECK(!ScriptSnapshotMewEncounter());
        MoveObjectEventToMapCoords(mew, x + 3, y + 2); mew->invisible = FALSE; mew->fixedPriority = TRUE;
        mew->facingDirection = DIR_SOUTH; sprite->subspriteMode = SUBSPRITES_IGNORE_PRIORITY;
        sprite->subspriteTableNum = 1; sprite->subpriority = 1; VarSet(VAR_FARAWAY_ISLAND_STEP_COUNTER, 0xFFFF);
        ModsWildCheckpointSave(&beforeSingle); ScriptContext_SetupScript(FarawayIsland_Interior_EventScript_PrepareMew);
        ScriptContext_RunScript(); ModsWildCheckpointSave(&afterSingle);
        CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked() && mew->active
            && mew->currentCoords.x == x && mew->currentCoords.y == y && mew->invisible && !mew->fixedPriority
            && mew->facingDirection == DIR_EAST && mew->movementDirection == DIR_NORTH
            && mew->previousMovementDirection == DIR_WEST && sprite->subspriteMode == SUBSPRITES_ON
            && sprite->subspriteTableNum == 2 && sprite->subpriority == 9 && VarGet(VAR_FARAWAY_ISLAND_STEP_COUNTER) == 37
            && !FlagGet(FLAG_SYS_CTRL_OBJ_DELETE) && !FlagGet(FLAG_CAUGHT_MEW) && !FlagGet(FLAG_DEFEATED_MEW)
            && !memcmp(beforeSingle.enemy, afterSingle.enemy, sizeof(beforeSingle.enemy))
            && !memcmp(&beforeSingle.rng, &afterSingle.rng, sizeof(beforeSingle.rng))
            && !memcmp(&beforeSingle.rng2, &afterSingle.rng2, sizeof(beforeSingle.rng2)));
        sCleanupMode = 0;
    }
    /* The solved triangle respawns after a veto and still points to the
     * encounter on the next interaction; no second puzzle is required. */
    {
        u32 map = MAP_BIRTH_ISLAND_EXTERIOR;
        gMapHeader = *gMapGroups[MAP_GROUP(map)][MAP_NUM(map)];
        gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(map); gSaveBlock1Ptr->location.mapNum = MAP_NUM(map);
        memset(gSaveBlock1Ptr->objectEventTemplates, 0, sizeof(gSaveBlock1Ptr->objectEventTemplates));
        memcpy(gSaveBlock1Ptr->objectEventTemplates, gMapHeader.events->objectEvents,
               gMapHeader.events->objectEventCount * sizeof(struct ObjectEventTemplate));
        SetObjEventTemplateCoords(LOCALID_BIRTH_ISLAND_EXTERIOR_ROCK, 15, 10);
        for (u32 i = 0; i < 2; ++i)
        {
            u8 actor = ScriptedTestActor(OBJ_EVENT_GFX_DEOXYS, LOCALID_BIRTH_ISLAND_DEOXYS);
            (void)actor; FlagSet(FLAG_DEOXYS_ROCK_COMPLETE); FlagSet(FLAG_HIDE_BIRTH_ISLAND_DEOXYS_TRIANGLE);
            FlagClear(FLAG_SYS_CTRL_OBJ_DELETE); FlagClear(FLAG_BATTLED_DEOXYS); FlagClear(FLAG_DEFEATED_DEOXYS);
            if (i) FlagSet(FLAG_HIDE_DEOXYS); else FlagClear(FLAG_HIDE_DEOXYS);
            VarSet(VAR_DEOXYS_ROCK_LEVEL, 10); VarSet(VAR_DEOXYS_ROCK_STEP_COUNT, 0);
            sCleanupMode = 6;
            ModsWildCheckpointSave(&beforeSingle); ScriptContext_SetupScript(BirthIsland_Exterior_EventScript_PrepareDeoxys);
            ScriptContext_RunScript(); ModsWildCheckpointSave(&afterSingle);
            u8 rock, deoxys;
            CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked()
                && TryGetObjectEventIdByLocalIdAndMap(LOCALID_BIRTH_ISLAND_DEOXYS, MAP_NUM(map), MAP_GROUP(map), &deoxys)
                && !TryGetObjectEventIdByLocalIdAndMap(LOCALID_BIRTH_ISLAND_EXTERIOR_ROCK, MAP_NUM(map), MAP_GROUP(map), &rock)
                && gObjectEvents[rock].currentCoords.x == 15 + MAP_OFFSET && gObjectEvents[rock].currentCoords.y == 10 + MAP_OFFSET
                && FlagGet(FLAG_DEOXYS_ROCK_COMPLETE) && !FlagGet(FLAG_HIDE_BIRTH_ISLAND_DEOXYS_TRIANGLE)
                && !!FlagGet(FLAG_HIDE_DEOXYS) == !!i && !FlagGet(FLAG_SYS_CTRL_OBJ_DELETE)
                && !FlagGet(FLAG_BATTLED_DEOXYS) && !FlagGet(FLAG_DEFEATED_DEOXYS)
                && VarGet(VAR_DEOXYS_ROCK_LEVEL) == 10 && IndexOfSpritePaletteTag(OBJ_EVENT_PAL_TAG_BIRTH_ISLAND_STONE) < 16
                && !memcmp(beforeSingle.enemy, afterSingle.enemy, sizeof(beforeSingle.enemy))
                && !memcmp(&beforeSingle.rng, &afterSingle.rng, sizeof(beforeSingle.rng))
                && !memcmp(&beforeSingle.rng2, &afterSingle.rng2, sizeof(beforeSingle.rng2)));
            sCleanupMode = 0;
            ResetTasks(); DoDeoxysRockInteraction(); RunTasks();
            CHECK(gSpecialVar_Result == DEOXYS_ROCK_COMPLETE && VarGet(VAR_DEOXYS_ROCK_LEVEL) == 10);
            struct ScriptContext retry; InitScriptContext(&retry, gScriptCmdTable, gScriptCmdTableEnd);
            SetupBytecodeScript(&retry, BirthIsland_Exterior_EventScript_RockResult);
            CHECK(RunScriptCommand(&retry) && retry.scriptPtr == BirthIsland_Exterior_EventScript_Deoxys + 1);
        }
        /* Alternate graphics have a different tag, as in FireRed. */
        ScriptedTestActor(OBJ_EVENT_GFX_DEOXYS, LOCALID_BIRTH_ISLAND_EXTERIOR_ROCK);
        u32 alternatePalette = IndexOfSpritePaletteTag(OBJ_EVENT_PAL_TAG_DEOXYS);
        CHECK(alternatePalette < 16); VarSet(VAR_DEOXYS_ROCK_LEVEL, 10);
        u16 *colors = &gPlttBufferUnfaded[OBJ_PLTT_ID(alternatePalette)];
        memset(colors, 0, PLTT_SIZE_4BPP); CHECK(!ScriptRestoreDeoxysRockPalette());
        bool32 colored = FALSE;
        for (u32 i = 0; i < 16; ++i) colored |= colors[i] != 0;
        CHECK(colored);
    }
    CHECK(sScriptCleanupCount == 15);
    /* Run the ghost's denial path in the global interpreter, including the
     * release and cleanup notification. FRLG map scripts are not linked into
     * this Emerald build, so encode the same preparation and guard commands. */
    ClockTestPut64(hostRun, 991); RhhModsSetRunContext(991, 456, 0);
    for (u32 scope = 0; scope < 2; ++scope)
    {
        if (scope) CHECK(AddBagItem(ITEM_SILPH_SCOPE, 1));
        ResetTasks(); ScriptContext_Init(); UnlockPlayerFieldControls();
        u8 script[128], *p = script;
        *p++ = SCR_OP_CALLNATIVE;
        uintptr_t nativeFunction = (uintptr_t)ScriptSetWildEncounterIdentity;
        memcpy(p, &nativeFunction, sizeof(nativeFunction)); p += sizeof(nativeFunction);
        const char *ghostId = "rhh:encounter/pokemon_tower_6f_frlg/marowak";
        memcpy(p, ghostId, strlen(ghostId) + 1); p += strlen(ghostId) + 1;
        const u8 wildOperands[] = {SPECIES_MAROWAK & 255, SPECIES_MAROWAK >> 8, 30, 0, 0, 0, 0, 0, 0, 0};
        *p++ = SCR_OP_SETWILDBATTLE; memcpy(p, wildOperands, sizeof(wildOperands)); p += sizeof(wildOperands);
        u32 specialIndex = ScriptedTestSpecialIndex((uintptr_t)PrepareMarowakBattle);
        *p++ = SCR_OP_SPECIAL; *p++ = specialIndex; *p++ = specialIndex >> 8;
        specialIndex = ScriptedTestSpecialIndex((uintptr_t)ScriptedWildBattleAllowed);
        *p++ = SCR_OP_SPECIALVAR; *p++ = VAR_RESULT & 255; *p++ = VAR_RESULT >> 8;
        *p++ = specialIndex; *p++ = specialIndex >> 8;
        *p++ = SCR_OP_RELEASEALL; *p++ = SCR_OP_END;
        CHECK((size_t)(p - script) <= sizeof(script));
        sExpectedAuthoredEncounter = RhhContentKeyOf(ghostId);
        sExpectedAuthoredPreparation = RHH_SCRIPTED_WILD_COMMAND; sAuthoredEncounterChecks = 0;
        struct RhhWildCheckpoint beforeGhost, afterGhost; ModsWildCheckpointSave(&beforeGhost);
        ScriptContext_SetupScript(script);
        for (u32 frame = 0; frame < 100 && ScriptContext_IsEnabled(); ++frame)
        { ScriptContext_RunScript(); RunTasks(); }
        ModsWildCheckpointSave(&afterGhost);
        CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked() && !gSpecialVar_Result
            && sAuthoredEncounterChecks == 4 && sScriptCleanupCount == 16 + scope
            && !gTasks[0].isActive && !memcmp(beforeGhost.enemy, afterGhost.enemy, sizeof(beforeGhost.enemy))
            && beforeGhost.enemyCount == afterGhost.enemyCount && beforeGhost.roamerIndex == afterGhost.roamerIndex
            && !memcmp(&beforeGhost.rng, &afterGhost.rng, sizeof(beforeGhost.rng))
            && !memcmp(&beforeGhost.rng2, &afterGhost.rng2, sizeof(beforeGhost.rng2)));
        sExpectedAuthoredEncounter = 0;
        if (scope) CHECK(RemoveBagItem(ITEM_SILPH_SCOPE, 1));
    }
    /* Wally's real post-movement script must restore the saved player party
     * and enter a stable retry state when a tutorial-specific constraint vetoes. */
    ClockTestPut64(hostRun, 992); RhhModsSetRunContext(992, 456, 0);
    struct WarpData previousLocation = gSaveBlock1Ptr->location;
    struct MapHeader previousMap = gMapHeader;
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_PETALBURG_CITY);
    gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_PETALBURG_CITY);
    gMapHeader = *gMapGroups[MAP_GROUP(MAP_PETALBURG_CITY)][MAP_NUM(MAP_PETALBURG_CITY)];
    const struct ObjectEventTemplate *wallyTemplate = FindObjectEventTemplateByLocalId(
        LOCALID_PETALBURG_WALLY, gMapHeader.events->objectEvents, gMapHeader.events->objectEventCount);
    CHECK(wallyTemplate && wallyTemplate->script == PetalburgCity_EventScript_WallyRetry);
    ResetTasks(); ScriptContext_Init(); UnlockPlayerFieldControls();
    VarSet(VAR_PETALBURG_CITY_STATE, 2); VarSet(VAR_PETALBURG_GYM_STATE, 1);
    FlagClear(FLAG_HIDE_PETALBURG_GYM_WALLY);
    /* Old saves can contain party HP and cached box damage that disagree;
     * a denial must not silently migrate the saved Pokémon. */
    gParties[B_TRAINER_PLAYER][0].box.hpLost ^= 1;
    struct Pokemon playerBefore[PARTY_SIZE]; memcpy(playerBefore, gParties[B_TRAINER_PLAYER], sizeof(playerBefore));
    u32 playerCountBefore = gPartiesCount[B_TRAINER_PLAYER];
    sExpectedTutorialPlayer = playerBefore; sExpectedTutorialPlayerCount = playerCountBefore;
    sTutorialCleanupPredicate = CheckTutorialCleanupWorld;
    SavePlayerParty(); LoadWallyZigzagoon();
    struct RhhWildCheckpoint beforeTutorial, afterTutorial; ModsWildCheckpointSave(&beforeTutorial);
    sExpectedAuthoredEncounter = RhhContentKeyOf("rhh:encounter/petalburg_city/wally_tutorial");
    sExpectedAuthoredPreparation = RHH_SCRIPTED_DIRECT_SPECIAL; sAuthoredEncounterChecks = 0;
    ScriptContext_SetupScript(PetalburgCity_EventScript_PrepareWallyTutorial);
    for (u32 frame = 0; frame < 100 && ScriptContext_IsEnabled(); ++frame)
    { ScriptContext_RunScript(); RunTasks(); }
    ModsWildCheckpointSave(&afterTutorial);
    CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked() && !gTasks[0].isActive
        && !gSpecialVar_Result && sAuthoredEncounterChecks == 3
        && VarGet(VAR_PETALBURG_CITY_STATE) == 6 && VarGet(VAR_PETALBURG_GYM_STATE) == 0
        && FlagGet(FLAG_HIDE_PETALBURG_GYM_WALLY)
        && gPartiesCount[B_TRAINER_PLAYER] == playerCountBefore
        && !memcmp(playerBefore, gParties[B_TRAINER_PLAYER], sizeof(playerBefore))
        && !memcmp(beforeTutorial.enemy, afterTutorial.enemy, sizeof(beforeTutorial.enemy))
        && beforeTutorial.enemyCount == afterTutorial.enemyCount
        && !memcmp(&beforeTutorial.rng, &afterTutorial.rng, sizeof(beforeTutorial.rng))
        && !memcmp(&beforeTutorial.rng2, &afterTutorial.rng2, sizeof(beforeTutorial.rng2)));
    sExpectedAuthoredEncounter = 0;
    /* Use Wally's actual retry interaction: a second veto must restore the
     * party again, and a later allowed attempt must schedule the battle. */
    ResetSpriteData(); memset(gObjectEvents, 0, sizeof(gObjectEvents));
    gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
    gObjectEvents[0].active = TRUE; gObjectEvents[0].isPlayer = TRUE;
    gObjectEvents[0].localId = LOCALID_PLAYER; gObjectEvents[0].spriteId = 0;
    gSprites[0].inUse = TRUE; gSprites[0].callback = SpriteCallbackDummy;
    InitGpuRegManager();
    const struct BgTemplate tutorialBg = {.bg = 0}; InitBgsFromTemplates(0, &tutorialBg, 1);
    SetDefaultFontsPointer(); InitStandardTextBoxWindows(); InitTextBoxGfxAndPrinters();
    for (u32 pass = 0; pass < 2; ++pass)
    {
        u32 seed = pass ? 989 : 992;
        ClockTestPut64(hostRun, seed); RhhModsSetRunContext(seed, 456, 0);
        ResetTasks(); ScriptContext_Init(); UnlockPlayerFieldControls();
        sExpectedAuthoredEncounter = RhhContentKeyOf("rhh:encounter/petalburg_city/wally_tutorial");
        sAuthoredEncounterChecks = 0;
        ScriptContext_SetupScript(PetalburgCity_EventScript_WallyRetry);
        for (u32 frame = 0; frame < 3000 && ScriptContext_IsEnabled(); ++frame)
        {
            gMain.newKeys = A_BUTTON; ScriptContext_RunScript();
            if (!ScriptContext_IsEnabled()) break;
            RunTasks(); RunTextPrinters();
        }
        gMain.newKeys = 0;
        CHECK(!ScriptContext_IsEnabled() && sAuthoredEncounterChecks == (pass ? 2u : 3u));
        if (!pass)
            CHECK(!ArePlayerFieldControlsLocked() && VarGet(VAR_PETALBURG_CITY_STATE) == 6
                && VarGet(VAR_PETALBURG_GYM_STATE) == 0 && gPartiesCount[B_TRAINER_PLAYER] == playerCountBefore
                && !memcmp(playerBefore, gParties[B_TRAINER_PLAYER], sizeof(playerBefore)));
        else
        {
            CHECK(ArePlayerFieldControlsLocked() && gBattleTypeFlags == BATTLE_TYPE_CATCH_TUTORIAL
                && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_RALTS);
            LoadPlayerParty(); ResetTasks(); ScriptContext_Init(); UnlockPlayerFieldControls();
        }
        sExpectedAuthoredEncounter = 0;
    }
    sTutorialCleanupPredicate = NULL; sExpectedTutorialPlayer = NULL;
    ClockTestPut64(hostRun, 992); RhhModsSetRunContext(992, 456, 0);
    /* The FRLG old-man script uses the same explicit preparation and guard. */
    ResetTasks(); InitScriptContext(&marked, gScriptCmdTable, gScriptCmdTableEnd);
    sExpectedAuthoredEncounter = RhhContentKeyOf("rhh:encounter/viridian_city_frlg/old_man_tutorial");
    sAuthoredEncounterChecks = 0; marked.wildPendingKey = sExpectedAuthoredEncounter;
    ModsWildCheckpointSave(&beforeTutorial);
    ScriptedTestCallSpecial(&marked, (uintptr_t)PrepareOldManTutorialBattle);
    ScriptedTestCallSpecialVar(&marked, (uintptr_t)TutorialWildBattleAllowed);
    ModsWildCheckpointSave(&afterTutorial);
    CHECK(!gSpecialVar_Result && !marked.wildPreparedKey && sAuthoredEncounterChecks == 2
        && !gTasks[0].isActive && !memcmp(beforeTutorial.enemy, afterTutorial.enemy, sizeof(beforeTutorial.enemy))
        && !memcmp(&beforeTutorial.rng, &afterTutorial.rng, sizeof(beforeTutorial.rng))
        && !memcmp(&beforeTutorial.rng2, &afterTutorial.rng2, sizeof(beforeTutorial.rng2)));
    sExpectedAuthoredEncounter = 0;
    /* Exercise the FRLG subroutine's denied return in a global script,
     * leaving its scene and item reward unchanged. */
    ResetTasks(); ScriptContext_Init(); UnlockPlayerFieldControls();
    u8 oldScript[160], *write = oldScript;
    *write++ = SCR_OP_CALL; u8 *callTarget = write; write += sizeof(uintptr_t);
    *write++ = SCR_OP_RELEASE; *write++ = SCR_OP_END;
    uintptr_t subroutine = (uintptr_t)write; memcpy(callTarget, &subroutine, sizeof(subroutine));
    *write++ = SCR_OP_CALLNATIVE;
    uintptr_t identify = (uintptr_t)ScriptSetWildEncounterIdentity;
    memcpy(write, &identify, sizeof(identify)); write += sizeof(identify);
    const char *oldManId = "rhh:encounter/viridian_city_frlg/old_man_tutorial";
    memcpy(write, oldManId, strlen(oldManId) + 1); write += strlen(oldManId) + 1;
    u32 tutorialSpecial = ScriptedTestSpecialIndex((uintptr_t)PrepareOldManTutorialBattle);
    *write++ = SCR_OP_SPECIAL; *write++ = tutorialSpecial; *write++ = tutorialSpecial >> 8;
    tutorialSpecial = ScriptedTestSpecialIndex((uintptr_t)TutorialWildBattleAllowed);
    *write++ = SCR_OP_SPECIALVAR; *write++ = VAR_RESULT & 255; *write++ = VAR_RESULT >> 8;
    *write++ = tutorialSpecial; *write++ = tutorialSpecial >> 8;
    *write++ = SCR_OP_COMPARE_VAR_TO_VALUE; *write++ = VAR_RESULT & 255; *write++ = VAR_RESULT >> 8;
    *write++ = FALSE; *write++ = 0;
    *write++ = SCR_OP_GOTO_IF; *write++ = EQUAL; u8 *cancelTarget = write; write += sizeof(uintptr_t);
    tutorialSpecial = ScriptedTestSpecialIndex((uintptr_t)StartPreparedTutorialBattle);
    *write++ = SCR_OP_SPECIAL; *write++ = tutorialSpecial; *write++ = tutorialSpecial >> 8;
    *write++ = SCR_OP_WAITSTATE; *write++ = SCR_OP_RETURN;
    uintptr_t cancelled = (uintptr_t)write; memcpy(cancelTarget, &cancelled, sizeof(cancelled));
    *write++ = SCR_OP_RETURN; CHECK((size_t)(write - oldScript) <= sizeof(oldScript));
    VarSet(VAR_MAP_SCENE_VIRIDIAN_CITY_OLD_MAN, 1);
    bool32 hadTeachyTv = CheckBagHasItem(ITEM_TEACHY_TV, 1);
    sExpectedAuthoredEncounter = RhhContentKeyOf(oldManId); sAuthoredEncounterChecks = 0;
    ModsWildCheckpointSave(&beforeTutorial); ScriptContext_SetupScript(oldScript);
    for (u32 frame = 0; frame < 100 && ScriptContext_IsEnabled(); ++frame)
    { ScriptContext_RunScript(); RunTasks(); }
    ModsWildCheckpointSave(&afterTutorial);
    CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked() && !gSpecialVar_Result
        && sAuthoredEncounterChecks == 3 && VarGet(VAR_MAP_SCENE_VIRIDIAN_CITY_OLD_MAN) == 1
        && CheckBagHasItem(ITEM_TEACHY_TV, 1) == hadTeachyTv && !gTasks[0].isActive
        && !memcmp(beforeTutorial.enemy, afterTutorial.enemy, sizeof(beforeTutorial.enemy))
        && !memcmp(&beforeTutorial.rng, &afterTutorial.rng, sizeof(beforeTutorial.rng))
        && !memcmp(&beforeTutorial.rng2, &afterTutorial.rng2, sizeof(beforeTutorial.rng2)));
    sExpectedAuthoredEncounter = 0;
    gSaveBlock1Ptr->location = previousLocation; gMapHeader = previousMap;
    ClockTestPut64(hostRun, 123); RhhModsSetRunContext(123, 456, 0);
    struct RhhWildEncounterCancellation outside = {.struct_size = sizeof(outside), .reason = "untouched"};
    CHECK(wild->context(&unavailable) && wild->cancellation_context(&outside)
        && !strcmp(outside.reason, "untouched"));
    sScriptCleanupPredicate = NULL;
    rng_value_t beforeProposal = gRngValue, beforeProposal2 = gRng2Value;
    CHECK(ModsWildScriptedCandidate(SPECIES_POOCHYENA, 7, 0) == SPECIES_ZIGZAGOON
        && !memcmp(&beforeProposal, &gRngValue, sizeof(beforeProposal))
        && !memcmp(&beforeProposal2, &gRng2Value, sizeof(beforeProposal2)));
    /* A malformed party member is ignored and decoding cannot alter it. */
    gParties[B_TRAINER_OPPONENT_A][0].box.secure.raw[0] ^= 1;
    struct Pokemon corrupt = gParties[B_TRAINER_OPPONENT_A][0]; rng_value_t rng = gRngValue;
    ModsWildScriptedReady(1);
    CHECK(!memcmp(&corrupt, &gParties[B_TRAINER_OPPONENT_A][0], sizeof(corrupt))
        && !memcmp(&rng, &gRngValue, sizeof(rng)));
    RhhModsSetRunContext(0, 0, 0); ModsWildScriptedReady(1);
    rng_value_t eventRng = gRngValue, eventRng2 = gRng2Value;
    gSpecialVar_0x8004 = SPECIES_POOCHYENA; gSpecialVar_0x8005 = 7; gSpecialVar_0x8006 = ITEM_POTION;
    ZeroEnemyPartyMons();
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_POOCHYENA, 7, Random32(), OTID_STRUCT_PLAYER_ID, USE_RANDOM_IVS);
    GiveMonInitialMoveset(&gParties[B_TRAINER_OPPONENT_A][0]);
    bool32 fateful = TRUE; u16 eventItem = ITEM_POTION;
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_MODERN_FATEFUL_ENCOUNTER, &fateful);
    CalculateMonStats(&gParties[B_TRAINER_OPPONENT_A][0]);
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_HELD_ITEM, &eventItem);
    struct Pokemon eventOriginal = gParties[B_TRAINER_OPPONENT_A][0];
    rng_value_t eventAfter = gRngValue, eventAfter2 = gRng2Value;
    gRngValue = eventRng; gRng2Value = eventRng2;
    CreateEnemyEventMon();
    CHECK(ScriptedWildBattleAllowed()
        && !memcmp(&eventOriginal, &gParties[B_TRAINER_OPPONENT_A][0], sizeof(eventOriginal))
        && !memcmp(&eventAfter, &gRngValue, sizeof(eventAfter))
        && !memcmp(&eventAfter2, &gRng2Value, sizeof(eventAfter2)));
    /* With the hook inactive, the script command must generate exactly the
     * engine's original opponents and consume the same random streams. */
    rng_value_t rngBefore = gRngValue, rngBefore2 = gRng2Value;
    CreateScriptedDoubleWildMon(SPECIES_POOCHYENA, 7, ITEM_POTION, SPECIES_POOCHYENA, 9, ITEM_ORAN_BERRY);
    struct Pokemon originalParty[2]; memcpy(originalParty, gParties[B_TRAINER_OPPONENT_A], sizeof(originalParty));
    rng_value_t rngAfter = gRngValue, rngAfter2 = gRng2Value;
    gRngValue = rngBefore; gRng2Value = rngBefore2;
    command = (struct ScriptContext){.scriptPtr = pair};
    CHECK(!ScrCmd_setwildbattle(&command) && ScriptedWildBattleAllowed()
        && !memcmp(originalParty, gParties[B_TRAINER_OPPONENT_A], sizeof(originalParty))
        && !memcmp(&rngAfter, &gRngValue, sizeof(rngAfter))
        && !memcmp(&rngAfter2, &gRng2Value, sizeof(rngAfter2)));
    /* With no active mod run, the guarded ghost path must match the old
     * direct start, including its RNG draws, with and without Silph Scope. */
    CHECK(!CheckBagHasItem(ITEM_SILPH_SCOPE, 1));
    u32 savedBattleFlags = gBattleTypeFlags;
    MainCallback savedBattleReturn = gMain.savedCallback;
    for (u32 scope = 0; scope < 2; ++scope)
    {
        if (scope) CHECK(AddBagItem(ITEM_SILPH_SCOPE, 1));
        ResetTasks(); struct RhhWildCheckpoint initial, legacy, guarded;
        ModsWildCheckpointSave(&initial); gBattleTypeFlags = savedBattleFlags;
        CreateScriptedWildMon(SPECIES_MAROWAK, 30, ITEM_NONE);
        StartMarowakBattle(); ModsWildCheckpointSave(&legacy);
        ResetTasks(); UnlockPlayerFieldControls(); ModsWildCheckpointRestore(&initial);
        gBattleTypeFlags = savedBattleFlags;
        const u8 ghostOperands[] = {SPECIES_MAROWAK & 255, SPECIES_MAROWAK >> 8, 30, 0, 0, 0, 0, 0, 0, 0};
        struct ScriptContext ghost = {.scriptPtr = ghostOperands};
        CHECK(!ScrCmd_setwildbattle(&ghost));
        ScriptedTestCallSpecial(&ghost, (uintptr_t)PrepareMarowakBattle);
        ScriptedTestCallSpecialVar(&ghost, (uintptr_t)ScriptedWildBattleAllowed);
        CHECK(gSpecialVar_Result);
        ScriptedTestCallSpecial(&ghost, (uintptr_t)StartPreparedMarowakBattle);
        ModsWildCheckpointSave(&guarded);
        CHECK(!memcmp(legacy.enemy, guarded.enemy, sizeof(legacy.enemy))
            && legacy.enemyCount == guarded.enemyCount && legacy.roamerIndex == guarded.roamerIndex
            && !memcmp(&legacy.rng, &guarded.rng, sizeof(legacy.rng))
            && !memcmp(&legacy.rng2, &guarded.rng2, sizeof(legacy.rng2)));
        ResetTasks(); UnlockPlayerFieldControls();
        if (scope) CHECK(RemoveBagItem(ITEM_SILPH_SCOPE, 1));
    }
    for (u32 tutorial = 0; tutorial < 2; ++tutorial)
    {
        ResetTasks(); struct RhhWildCheckpoint initial, legacy, guarded;
        ModsWildCheckpointSave(&initial); gBattleTypeFlags = savedBattleFlags;
        if (tutorial) StartOldManTutorialBattle(); else StartWallyTutorialBattle();
        ModsWildCheckpointSave(&legacy);
        ResetTasks(); UnlockPlayerFieldControls(); ModsWildCheckpointRestore(&initial);
        gBattleTypeFlags = savedBattleFlags;
        struct ScriptContext tutorialCtx = {0};
        ScriptedTestCallSpecial(&tutorialCtx, tutorial ? (uintptr_t)PrepareOldManTutorialBattle
                                                       : (uintptr_t)PrepareWallyTutorialBattle);
        ScriptedTestCallSpecialVar(&tutorialCtx, (uintptr_t)TutorialWildBattleAllowed);
        CHECK(gSpecialVar_Result);
        ScriptedTestCallSpecial(&tutorialCtx, (uintptr_t)StartPreparedTutorialBattle);
        ModsWildCheckpointSave(&guarded);
        CHECK(!memcmp(legacy.enemy, guarded.enemy, sizeof(legacy.enemy))
            && legacy.enemyCount == guarded.enemyCount && legacy.roamerIndex == guarded.roamerIndex
            && !memcmp(&legacy.rng, &guarded.rng, sizeof(legacy.rng))
            && !memcmp(&legacy.rng2, &guarded.rng2, sizeof(legacy.rng2)));
        ResetTasks(); UnlockPlayerFieldControls();
    }
    gBattleTypeFlags = savedBattleFlags; gMain.savedCallback = savedBattleReturn;
    free(gBackupMapLayout.map); gBackupMapLayout.map = NULL; RhhModsUnloadLibraries();
    puts("scripted wild: real single, double and legendary battle starts passed");
}
