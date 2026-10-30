static void CheckNatureRecordedPlayback(const char *path)
{
    InitModsForTest(); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    RhhModsSetRunContext(123, 456, 0);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr, 0, sizeof(*gPokemonStoragePtr));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 10);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    u8 *legacyState = NULL; u32 legacyStateSize = 0; CHECK(!RhhModsExport(&legacyState, &legacyStateSize));
    CHECK(!ModsEnableNatureFormat() && ModsNatureFormatEnabled() && NativeRecordedBattleNatureMode() == -1);
    struct Pokemon original = gParties[B_TRAINER_PLAYER][0];
    struct RecordedBattleSave record = {0}, encoded;
    record.battleFlags = BATTLE_TYPE_TRAINER; record.parties[B_TRAINER_PLAYER][0] = original;
    u32 natural = GetNatureFromPersonality(original.box.personality), altered = (natural + 1) % NUM_NATURES;
    CHECK(!NativeNatureStorageWrite(&record.parties[B_TRAINER_PLAYER][0].box, altered));
    record.checksum = CalcByteArraySum((const u8 *)&record, sizeof(record) - sizeof(record.checksum));
    u8 sector[SECTOR_SIZE] = {0};
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); cgb_audio_init(42060); m4aSoundInit();
    for (u32 format = 0; format < 2; ++format)
    {
        record.checksum = CalcByteArraySum((const u8 *)&record, sizeof(record) - sizeof(record.checksum));
        CHECK(!NativeNatureRecordingEncode(&record, format, &encoded));
        memcpy(sector, &encoded, sizeof(encoded));
        CHECK(!NativeSaveSpecialWrite(SECTOR_ID_RECORDED_BATTLE, &encoded, sizeof(encoded))
            && CanCopyRecordedBattleSaveData() && NativeRecordedBattleNatureMode() == -1);
        ResetTasks(); gMain.savedCallback = NULL; PlayRecordedBattle(CB2_Overworld);
        CHECK(NativeRecordedBattleNatureMode() == (int)format
            && GetNature(&gParties[B_TRAINER_PLAYER][0]) == (format ? altered : natural));
        for (u32 frame = 0; frame < 128; ++frame) RunTasks();
        CHECK(gMain.savedCallback); gMain.savedCallback();
        CHECK(NativeRecordedBattleNatureMode() == -1 && gPartiesCount[B_TRAINER_PLAYER] == 1
            && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    }
    struct AiThinkingStruct ai = {0}; struct AiThinkingStruct *savedAi = gAiThinkingStruct;
    gAiThinkingStruct = &ai; gBattleTypeFlags = BATTLE_TYPE_TRAINER; RecordedBattle_Init(B_RECORD_MODE_RECORDING);
    struct BoxPokemon expectedBox = record.parties[B_TRAINER_PLAYER][0].box;
    gParties[B_TRAINER_PLAYER][0].box = record.parties[B_TRAINER_PLAYER][0].box;
    RecordedBattle_SaveParties(); gParties[B_TRAINER_PLAYER][0] = original;
    CHECK(!RhhModsImport(legacyState, legacyStateSize) && !ModsNatureFormatEnabled()); free(legacyState);
    CHECK(MoveRecordedBattleToSaveData() && !NativeSaveSpecialRead(SECTOR_ID_RECORDED_BATTLE, sector, sizeof(encoded)));
    memcpy(&encoded, sector, sizeof(encoded)); unsigned recordedFormat = 99;
    CHECK((encoded.battleFlags & BATTLE_TYPE_RECORDED_INVALID)
        && !NativeNatureRecordingDecode(&encoded, &record, &recordedFormat) && recordedFormat == 1
        && !memcmp(&record.parties[B_TRAINER_PLAYER][0].box, &expectedBox, sizeof(expectedBox)));
    ResetTasks(); gMain.savedCallback = NULL; PlayRecordedBattle(CB2_Overworld);
    CHECK(NativeRecordedBattleNatureMode() == 1 && !ModsNatureFormatEnabled()
        && GetNature(&gParties[B_TRAINER_PLAYER][0]) == altered);
    for (u32 frame = 0; frame < 128; ++frame) RunTasks();
    CHECK(gMain.savedCallback); gMain.savedCallback();
    CHECK(NativeRecordedBattleNatureMode() == -1 && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    gAiThinkingStruct = savedAi; gBattleTypeFlags = 0;
    SDL_Quit(); NativeSaveClose();
    puts("nature recording: legacy and versioned playback provenance passed");
}
