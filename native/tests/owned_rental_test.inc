#ifdef RHH_OWNED_IDENTITY_TEST
void CallSlateportTentFunction(void);
static void CheckOwnedRentalLedger(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    for (u32 i = 0; i < 3; ++i)
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][i], SPECIES_TORCHIC, 20, 100 + i, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][2] = gParties[B_TRAINER_PLAYER][0];
    gPartiesCount[B_TRAINER_PLAYER] = 3;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    struct NativeOwnedId ids[3];
    for (u32 i = 0; i < 3; ++i) ids[i] = GetLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i});
    CHECK(DifferentLedgerId(ids[0], ids[2]));
    gFlashMemoryPresent = TRUE; Save_ResetSaveCounters(); SavePlayerParty();
    CHECK(!HandleSavingData(SAVE_NORMAL) && !gDamagedSaveSectors);
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 37));
    CHECK(mons && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_37_SIZE);
    gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    for (u32 i = 0; i < ARRAY_COUNT(gSaveBlock2Ptr->frontier.rentalMons); ++i)
        gSaveBlock2Ptr->frontier.rentalMons[i] = (struct RentalMon){.monId = i, .personality = 10000 + i, .ivs = 10};
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_SELECT_RENT_MONS; CallBattleFactoryFunction();
    CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_RENTAL
        && !gPartiesCount[B_TRAINER_PLAYER]);
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_SET_PARTIES; CallBattleFactoryFunction(); CalculatePlayerPartyCount();
    CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_RENTAL
        && gPartiesCount[B_TRAINER_PLAYER] == FRONTIER_PARTY_SIZE && !NativeOwnedTrackerAudit());
    typeof(gPokemonStoragePtr) storage = gPokemonStoragePtr; gPokemonStoragePtr = NULL;
    CHECK(NativeOwnedTrackerAudit() == 1 && NativeOwnedTrackerActive(456)); gPokemonStoragePtr = storage;
    CHECK(NativeSaveBegin() && NativeOwnedTrackerActive(456));
    struct RhhOwnedMonSnapshot snapshot = {.struct_size = sizeof(snapshot), .slot = 99}, unchanged = snapshot;
    u32 field = 99;
    CHECK(mons->party_at(0, &snapshot) && !memcmp(&snapshot, &unchanged, sizeof(snapshot))
        && mons->battle_view_field_slot(0, &field) && field == 99);
    struct BattleStruct battle = {0}; struct BattleStruct *oldBattleStruct = gBattleStruct;
    bool8 oldBattle = gMain.inBattle; u32 oldFlags = gBattleTypeFlags;
    struct PartyMenu oldMenu = gPartyMenu; u8 oldOrder[sizeof(gBattlePartyCurrentOrder)];
    memcpy(oldOrder, gBattlePartyCurrentOrder, sizeof(oldOrder));
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_FACTORY | BATTLE_TYPE_TRAINER;
    gPartyMenu.layout = PARTY_LAYOUT_SINGLE;
    gBattlePartyCurrentOrder[0] = 0x01; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456)
        && mons->party_view() == RHH_OWNED_PARTY_RENTAL && mons->battle_view_field_slot(0, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1) && NativeOwnedTrackerActive(456));
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = oldBattleStruct;
    gPartyMenu = oldMenu; memcpy(gBattlePartyCurrentOrder, oldOrder, sizeof(oldOrder));
    /* Rental swaps and recreation cannot affect the saved originals. */
    CopyMon(&gParties[B_TRAINER_PLAYER][0], &gParties[B_TRAINER_OPPONENT_A][0], sizeof(struct Pokemon));
    CHECK(NativeOwnedTrackerActive(456));
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_SET_PARTIES; CallBattleFactoryFunction(); CalculatePlayerPartyCount();
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    SaveGameFrontier();
    CHECK(gSaveAttemptStatus == SAVE_STATUS_OK && NativeOwnedTrackerActive(456)
        && mons->party_view() == RHH_OWNED_PARTY_RENTAL && !NativeOwnedTrackerAudit());
    LoadPlayerParty(); CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_FIELD_ORDER);
    for (u32 i = 0; i < 3; ++i) CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, ids[i]);
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveRestoreMods(gSaveCounter) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); gSpecialVar_0x8004 = SLATEPORT_TENT_FUNC_SELECT_RENT_MONS; CallSlateportTentFunction();
    CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_RENTAL
        && !gPartiesCount[B_TRAINER_PLAYER]);
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_SET_PARTIES;
    CallBattleFactoryFunction(); CalculatePlayerPartyCount(); CHECK(NativeOwnedTrackerActive(456));
    /* The saved original can change while OT/personality remain untouched. */
    GetSavedPlayerPartyMon(1)->hp ^= 1;
    CHECK(NativeOwnedTrackerAudit() == -1 && !NativeOwnedTrackerActive(456));
    GetSavedPlayerPartyMon(1)->hp ^= 1;
    LoadPlayerParty(); CHECK(mons->party_view() == RHH_OWNED_PARTY_FIELD_ORDER);
    CHECK(!NativeSaveRestoreMods(gSaveCounter) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); gSpecialVar_0x8004 = SLATEPORT_TENT_FUNC_SELECT_RENT_MONS; CallSlateportTentFunction();
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_SET_PARTIES;
    CallBattleFactoryFunction(); CalculatePlayerPartyCount(); CHECK(NativeOwnedTrackerActive(456));
    /* An unreported write to the held original party must not be mistaken for a rental edit. */
    u32 changed = GetMonData(GetSavedPlayerPartyMon(0), MON_DATA_PERSONALITY) ^ 1;
    SetMonData(GetSavedPlayerPartyMon(0), MON_DATA_PERSONALITY, &changed);
    CHECK(!NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_RENTAL);
    LoadPlayerParty(); CHECK(mons->party_view() == RHH_OWNED_PARTY_FIELD_ORDER);
    NativeSaveClose(); RhhModsShutdown(); puts("owned rental ledger: replacement, save, restore and unsafe backup write passed");
}
#endif
