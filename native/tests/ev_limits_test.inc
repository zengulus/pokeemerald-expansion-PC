static void CheckEvLimits(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 30, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1; struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][0];
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhEvLimitService *limits = (const void *)host->get_service("rhh:service/ev_limits", RHH_SERVICE_VERSION(1, 0));
    const struct RhhOwnedMonService *owned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 4));
    const struct RhhMonEditService *edit = (const void *)host->get_service("rhh:service/mon_edit", RHH_SERVICE_VERSION(1, 2));
    CHECK(limits && owned && edit && host->get_service("rhh:service/ev_limits", RHH_SERVICE_VERSION(1, 1)) == &limits->header
        && !host->get_service("rhh:service/ev_limits", RHH_SERVICE_VERSION(1, 2)));
    struct RhhEvLimits view = {.struct_size = sizeof(view), .total = 999}, old = view;
    CHECK(limits->context(&view) && !memcmp(&view, &old, sizeof(view)));
    struct RhhOwnedMonSnapshot snapshot = {.struct_size = sizeof(snapshot)}; CHECK(!owned->party_at(0, &snapshot));
    u8 *mode = RhhModsState("test:ev_limits"); CHECK(mode);
    CHECK(!limits->query(&snapshot, &view) && view.total == MAX_TOTAL_EVS && view.per_stat[0] == MAX_PER_STAT_EVS);
    *mode = 1; CHECK(!limits->query(&snapshot, &view) && view.total == 700 && view.per_stat[0] == 255);
    struct RhhMonEditRequest request = {.struct_size = sizeof(request), .fields = RHH_EDIT_EVS, .expected = snapshot,
        .evs = {255, 255, 0, 90, 0, 0}};
    struct RhhMonEditResult result = {.struct_size = sizeof(result)};
    struct Pokemon original = *mon; rng_value_t rng = gRngValue;
    CHECK(edit->preview(&request, &result) == RHH_EDIT_OK && !memcmp(mon, &original, sizeof(*mon))
        && edit->commit(&request, &result) == RHH_EDIT_OK && GetMonData(mon, MON_DATA_HP_EV) == 255
        && GetMonData(mon, MON_DATA_ATK_EV) == 255 && GetMonData(mon, MON_DATA_SPEED_EV) == 90);
    view.total = 1234; old = view;
    CHECK(limits->query(&snapshot, &view) && !memcmp(&view, &old, sizeof(view)));
    CHECK(!owned->party_at(0, &snapshot));
    *mode = 2; CHECK(!limits->query(&snapshot, &view) && view.total == 40 && view.per_stat[0] == 8);
    request.expected = snapshot; request.evs[0] = 9; request.evs[1] = 0; request.evs[3] = 0;
    CHECK(edit->commit(&request, &result) == RHH_EDIT_INVALID);
    request.evs[0] = 8; request.evs[1] = 33;
    CHECK(edit->commit(&request, &result) == RHH_EDIT_INVALID);
    request.evs[1] = 32; CHECK(edit->commit(&request, &result) == RHH_EDIT_OK);
    gMain.inBattle = TRUE; gBattleTypeFlags = 0;
    MonGainEVs(mon, SPECIES_ZIGZAGOON); CHECK(!GetMonData(mon, MON_DATA_SPEED_EV));
    gMain.inBattle = FALSE; u8 seven = 7; SetMonData(mon, MON_DATA_HP_EV, &seven);
    CHECK(!ExecuteTableBasedItemEffect(mon, ITEM_HP_UP, 0, 0) && GetMonData(mon, MON_DATA_HP_EV) == 8);
    CHECK(!owned->party_at(0, &snapshot));
    *mode = 3; CHECK(!limits->query(&snapshot, &view) && view.total == MAX_TOTAL_EVS && view.per_stat[0] == MAX_PER_STAT_EVS);
    *mode = 4; CHECK(!limits->query(&snapshot, &view) && view.total == 300 && view.per_stat[0] == 10
        && view.per_stat[1] == 255);
    *mode = 1; u8 zero = 0, near = 252;
    for (u32 i = 0; i < NUM_STATS; ++i) SetMonData(mon, MON_DATA_HP_EV + i, &zero);
    SetMonData(mon, MON_DATA_SPEED_EV, &near); gMain.inBattle = TRUE; gBattleTypeFlags = 0;
    MonGainEVs(mon, SPECIES_ZIGZAGOON); CHECK(GetMonData(mon, MON_DATA_SPEED_EV) == 253);
    SetMonData(mon, MON_DATA_SPEED_EV, &near); gBattleTypeFlags = BATTLE_TYPE_LINK;
    MonGainEVs(mon, SPECIES_ZIGZAGOON); CHECK(GetMonData(mon, MON_DATA_SPEED_EV) == 252);
    gMain.inBattle = FALSE; gBattleTypeFlags = 0; SetMonData(mon, MON_DATA_SPEED_EV, &zero);
    SetMonData(mon, MON_DATA_HP_EV, &near);
    if (I_VITAMIN_EV_CAP >= GEN_8)
        CHECK(!ExecuteTableBasedItemEffect(mon, ITEM_HP_UP, 0, 0) && GetMonData(mon, MON_DATA_HP_EV) == 255);
    gPokemonStoragePtr->boxes[0][0] = mon->box; memset(mon, 0, sizeof(*mon)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    CHECK(!owned->box_at(0, 0, &snapshot) && !limits->query(&snapshot, &view)
        && view.total == 700 && view.per_stat[0] == 255);
    request.expected = snapshot; memset(request.evs, 0, sizeof(request.evs));
    request.evs[0] = 255; request.evs[1] = 255; request.evs[3] = 90;
    CHECK(edit->commit(&request, &result) == RHH_EDIT_OK
        && GetBoxMonData(&gPokemonStoragePtr->boxes[0][0], MON_DATA_ATK_EV) == 255);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)));
    RhhModsUnloadLibraries(); puts("EV limits: editing, battle, items, constraints and fallback passed");
}
