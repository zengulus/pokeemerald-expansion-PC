/* A fresh-process save/load check for the denied Wally tutorial. */
static void CheckTutorialRetrySave(const char *path, const char *module, bool32 load)
{
    InitModsForTestWithLibrary(module);
    CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    if (!load)
    {
        gModDraft = (struct ModConfig){.seed = 993}; ModsNewGame(7); Save_ResetSaveCounters();
        gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_PETALBURG_CITY);
        gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_PETALBURG_CITY);
        gSaveBlock1Ptr->pos.x = 22; gSaveBlock1Ptr->pos.y = 15;
        gMapHeader = *gMapGroups[MAP_GROUP(MAP_PETALBURG_CITY)][MAP_NUM(MAP_PETALBURG_CITY)];
        LoadObjEventTemplatesFromHeader();
        ResetTasks(); ResetSpriteData(); memset(gObjectEvents, 0, sizeof(gObjectEvents));
        gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
        gObjectEvents[0].active = TRUE; gObjectEvents[0].isPlayer = TRUE;
        gObjectEvents[0].localId = LOCALID_PLAYER; gObjectEvents[0].spriteId = 0;
        gObjectEvents[0].mapGroup = MAP_GROUP(MAP_PETALBURG_CITY); gObjectEvents[0].mapNum = MAP_NUM(MAP_PETALBURG_CITY);
        gObjectEvents[0].currentCoords.x = 22 + MAP_OFFSET; gObjectEvents[0].currentCoords.y = 15 + MAP_OFFSET;
        gObjectEvents[1].active = TRUE; gObjectEvents[1].localId = LOCALID_PETALBURG_WALLY;
        gObjectEvents[1].graphicsId = OBJ_EVENT_GFX_WALLY; gObjectEvents[1].spriteId = 1;
        gObjectEvents[1].mapGroup = MAP_GROUP(MAP_PETALBURG_CITY); gObjectEvents[1].mapNum = MAP_NUM(MAP_PETALBURG_CITY);
        gObjectEvents[1].currentCoords.x = 23 + MAP_OFFSET; gObjectEvents[1].currentCoords.y = 15 + MAP_OFFSET;
        gSprites[0].inUse = gSprites[1].inUse = TRUE;
        gSprites[0].callback = gSprites[1].callback = SpriteCallbackDummy;
        memset(gParties[B_TRAINER_PLAYER], 0, sizeof(struct Pokemon) * PARTY_SIZE);
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
        gPartiesCount[B_TRAINER_PLAYER] = 1;
        struct Pokemon player = gParties[B_TRAINER_PLAYER][0];
        VarSet(VAR_PETALBURG_CITY_STATE, 2); VarSet(VAR_PETALBURG_GYM_STATE, 1);
        FlagClear(FLAG_HIDE_PETALBURG_CITY_WALLY); FlagClear(FLAG_HIDE_PETALBURG_GYM_WALLY);
        SavePlayerParty(); LoadWallyZigzagoon(); ScriptContext_Init();
        ScriptContext_SetupScript(PetalburgCity_EventScript_PrepareWallyTutorial);
        for (u32 frame = 0; frame < 100 && ScriptContext_IsEnabled(); ++frame)
        { ScriptContext_RunScript(); RunTasks(); }
        CHECK(!ScriptContext_IsEnabled() && !ArePlayerFieldControlsLocked()
            && VarGet(VAR_PETALBURG_CITY_STATE) == 6 && VarGet(VAR_PETALBURG_GYM_STATE) == 0
            && FlagGet(FLAG_HIDE_PETALBURG_GYM_WALLY) && !FlagGet(FLAG_HIDE_PETALBURG_CITY_WALLY)
            && !memcmp(&player, &gParties[B_TRAINER_PLAYER][0], sizeof(player)));
        CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
        puts("tutorial retry: denied scene and restored party saved");
    }
    else
    {
        CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK && RhhModsRunSeed() == 993
            && VarGet(VAR_PETALBURG_CITY_STATE) == 6 && VarGet(VAR_PETALBURG_GYM_STATE) == 0
            && FlagGet(FLAG_HIDE_PETALBURG_GYM_WALLY) && !FlagGet(FLAG_HIDE_PETALBURG_CITY_WALLY)
            && gPartiesCount[B_TRAINER_PLAYER] == 1
            && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_TORCHIC
            && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_PERSONALITY) == 12345
            && gObjectEvents[1].active && gObjectEvents[1].localId == LOCALID_PETALBURG_WALLY);
        gMapHeader = *gMapGroups[MAP_GROUP(MAP_PETALBURG_CITY)][MAP_NUM(MAP_PETALBURG_CITY)];
        LoadSaveblockObjEventScripts();
        const u8 *retry = GetObjectEventScriptPointerByObjectEventId(1);
        CHECK(retry == PetalburgCity_EventScript_WallyRetry);
        u8 *probe = RhhModsState("test:wild_scripted_probe"); CHECK(probe); probe[0] = 1;
        ResetTasks(); ResetSpriteData(); gPlayerAvatar.objectEventId = 0; gPlayerAvatar.spriteId = 0;
        gObjectEvents[0].spriteId = 0; gObjectEvents[1].spriteId = 1;
        gSprites[0].inUse = gSprites[1].inUse = TRUE;
        gSprites[0].callback = gSprites[1].callback = SpriteCallbackDummy;
        InitGpuRegManager(); const struct BgTemplate bg = {.bg = 0}; InitBgsFromTemplates(0, &bg, 1);
        SetDefaultFontsPointer(); InitStandardTextBoxWindows(); InitTextBoxGfxAndPrinters();
        ScriptContext_Init(); UnlockPlayerFieldControls(); ScriptContext_SetupScript(retry);
        for (u32 frame = 0; frame < 3000 && ScriptContext_IsEnabled(); ++frame)
        {
            gMain.newKeys = A_BUTTON; ScriptContext_RunScript();
            if (!ScriptContext_IsEnabled()) break;
            RunTasks(); RunTextPrinters();
        }
        gMain.newKeys = 0;
        CHECK(!ScriptContext_IsEnabled() && ArePlayerFieldControlsLocked()
            && gBattleTypeFlags == BATTLE_TYPE_CATCH_TUTORIAL
            && GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_RALTS
            && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON
            && GetMonData(GetSavedPlayerPartyMon(0), MON_DATA_SPECIES) == SPECIES_TORCHIC
            && VarGet(VAR_PETALBURG_CITY_STATE) == 6);
        RestorePlayerPartyAfterTutorialDenial(); ResetTasks(); ScriptContext_Init(); UnlockPlayerFieldControls();
        puts("tutorial retry: fresh-process reload and allowed retry passed");
    }
    NativeSaveClose(); RhhModsUnloadLibraries();
}
