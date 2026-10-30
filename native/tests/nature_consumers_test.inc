#include "link_rfu.h"
static void CheckNatureConsumers(const char *path)
{
    InitModsForTest(); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    RhhModsSetRunContext(123, 456, 0);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr, 0, sizeof(*gPokemonStoragePtr));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 10);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    u32 natural = GetNatureFromPersonality(gParties[B_TRAINER_PLAYER][0].box.personality);
    u8 mint = (natural + 2) % NUM_NATURES;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HIDDEN_NATURE, &mint);
    struct Pokemon original = gParties[B_TRAINER_PLAYER][0], altered = original;
    u32 target = (natural + 1) % NUM_NATURES;
    CHECK(!NativeNatureStorageWrite(&altered.box, target) && GetNature(&altered) == natural);
    CHECK(!ModsEnableNatureFormat() && ModsNatureFormatEnabled());
    MainCallback savedCallback = gMain.callback2; gLinkErrorOccurred = FALSE;
    OpenLink(); CHECK(gLinkErrorOccurred && gMain.callback2 == CB2_LinkError && !NativeLinkSessionOpen());
    gLinkErrorOccurred = FALSE; CHECK(!SendBlock(0, &altered, sizeof(altered)) && gLinkErrorOccurred);
    gLinkErrorOccurred = FALSE; CHECK(!Rfu_InitBlockSend((const u8 *)&altered, sizeof(altered)) && gLinkErrorOccurred);
    gLinkErrorOccurred = FALSE; InitRFUAPI(); CHECK(gLinkErrorOccurred && gMain.callback2 == CB2_LinkError);
    gLinkErrorOccurred = FALSE; SetMainCallback2(savedCallback);
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct Pokemon before = altered;
    CHECK(GetNature(&altered) == target && GetNature(&original) == natural
        && GetMonData(&altered, MON_DATA_HIDDEN_NATURE) == mint && !memcmp(&before, &altered, sizeof(before)));
    RhhModsSetRunContext(0, 0, 0); CHECK(GetNature(&altered) == natural);
    RhhModsSetRunContext(123, 456, 0); CHECK(GetNature(&altered) == target);
    struct Pokemon corrupt = altered; corrupt.box.secure.raw[0] ^= 1; before = corrupt;
    CHECK(GetNature(&corrupt) == natural && !memcmp(&before, &corrupt, sizeof(before)));
    gReceivedRemoteLinkPlayers = TRUE; CHECK(GetNature(&altered) == natural); gReceivedRemoteLinkPlayers = FALSE;
    gWirelessCommType = TRUE; CHECK(GetNature(&altered) == natural); gWirelessCommType = FALSE;
    gBattleTypeFlags = BATTLE_TYPE_LINK; CHECK(GetNature(&altered) == natural); gBattleTypeFlags = 0;
    CHECK(GetMonFlavorRelation(&altered, FLAVOR_SPICY) == GetFlavorRelationByNature(target, FLAVOR_SPICY));
    gParties[B_TRAINER_PLAYER][0] = altered;
    const struct RhhOwnedMonService *owned = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 0));
    struct RhhOwnedMonSnapshot snapshot = {.struct_size = sizeof(snapshot)};
    CHECK(owned && !owned->party_at(0, &snapshot) && snapshot.nature == target && snapshot.stat_nature == mint);
    struct BattlePokemon battle = {.personality = altered.box.personality, .otId = altered.box.otId};
    CaptureBattleMonNature(&battle, &altered);
    CHECK((battle.nativeNatureFlags & NATIVE_BATTLE_NATURE_SNAPSHOT) && battle.nativeNature == target
        && (battle.nativeNatureFlags & NATIVE_BATTLE_NATURE_STAT_VALID) && battle.nativeStatNature == mint);
    gBattleMons[0] = battle; CHECK(GetBattlerNature(0) == target);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    struct Pokemon ditto;
    CreateMonWithIVs(&ditto, SPECIES_DITTO, 20, 45678, OTID_STRUCT_PRESET(54322), 10);
    rng = gRngValue; rng2 = gRng2Value;
    gSaveBlock1Ptr->daycare.mons[0].mon = altered.box;
    gSaveBlock1Ptr->daycare.mons[1].mon = ditto.box;
    u16 item = ITEM_EVERSTONE; SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HELD_ITEM, &item);
    CHECK(GetConfig(NATURE_INHERITANCE) >= GEN_5 && GetChildNature(&gSaveBlock1Ptr->daycare) == target);
    const struct RhhBreedingService *breeding = (const void *)RhhModsHostAPI()->get_service("rhh:service/breeding", RHH_SERVICE_VERSION(1, 9));
    struct RhhBreedingOffer offer = {.struct_size = sizeof(offer)};
    struct RhhBreedingNatureOdds odds = {.struct_size = sizeof(odds)};
    CHECK(breeding && !breeding->offer(&offer) && !breeding->nature_odds(&offer, &odds)
        && offer.parents[0].mon.nature == target && offer.parents[0].mon.stat_nature == mint
        && odds.weights[target] == 100);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    NativeSaveClose(); puts("nature consumers: format gates, battle snapshot and breeding passed");
}
