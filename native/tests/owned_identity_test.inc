#include "constants/party_menu.h"
#include "battle_controllers.h"
#include "party_menu.h"
#include "trade.h"
#include "link.h"
#include "union_room.h"
#include "constants/union_room.h"
#include "save.h"
#include "script_pokemon_util.h"
#include "item.h"
#include "egg_hatch.h"
#include "field_specials.h"
#include "owned_identity_ledger.h"
#include "follower_npc.h"
#include "union_room_battle.h"
#include "cable_club.h"
#include "frontier_util.h"
#include "constants/frontier_util.h"
#include "constants/battle_dome.h"
#include "battle_pyramid.h"
#include "constants/battle_pyramid.h"
#include "constants/battle_partner.h"
#include "battle_factory.h"
#include "constants/battle_factory.h"
#include "battle_tent.h"
#include "constants/battle_tent.h"
#include "debug.h"
#include "mystery_event_script.h"
#include "mail.h"
#include "recorded_battle.h"
#include "daycare.h"
#include "util.h"
bool8 MEScrCmd_givepokemon(struct ScriptContext *ctx);
static u32 GiveMysteryEventTestMon(const struct Pokemon *mon)
{
    struct { struct Pokemon mon; struct Mail mail; } payload = {.mon = *mon};
    _Static_assert(offsetof(typeof(payload), mail) == sizeof(struct Pokemon), "mystery event payload layout");
    const u8 script[4] = {0};
    struct ScriptContext context = {.scriptPtr = script, .data = {(uintptr_t)&payload, 0, 0, 0}};
    CHECK(!MEScrCmd_givepokemon(&context));
    return context.data[2];
}
#ifdef RHH_OWNED_IDENTITY_TEST
bool32 ModsOwnedStorageTestOpen(void);
void ModsOwnedStorageTestClose(void);
extern u16 gSaveAttemptStatus;
void CallBattleDomeFunction(void);
bool32 ModsOwnedStorageTestPrepareRelease(u32 location, u32 box, u32 slot, bool32 moving);
void ModsOwnedStorageTestRelease(void);
bool32 ModsOwnedStorageTestPick(u32 location, u32 box, u32 slot);
bool32 ModsOwnedStorageTestPlace(u32 location, u32 box, u32 slot, bool32 shift);
bool32 ModsOwnedStorageTestGroupMove(u32 sourceBox, u32 destinationBox);
bool32 ModsOwnedPartyTestSwap(u32 first, u32 second);
bool32 ModsOwnedFusionTestStep(u32 cause, u32 primary, u32 secondary, u32 storage, u32 species);
bool32 ModsOwnedPartyBattleTestOrder(bool32 battleOrder);
bool32 ModsOwnedPartyBattleTestSummary(bool32 begin);
bool32 ModsOwnedPartyBattleTestSwitch(u32 first, u32 second);
void ModsOwnedFactoryTestCopy(bool32 swap, const struct Pokemon *mon);
void ModsOwnedCatchTestStep(bool32 place, u32 selected);
bool32 ModsOwnedTradeTestParty(u32 slot, bool32 link);
bool32 ModsOwnedTradeTestPartyEvolve(u32 slot, enum Species species);
bool32 ModsOwnedTradeTestBeginPc(u32 box, u32 slot);
bool32 ModsOwnedTradeTestFinishPc(void);
bool32 ModsOwnedTradeTestSyncPc(void);
bool32 ModsOwnedLinkTradeTestBegin(u32 slot);
bool32 ModsOwnedLinkTradeTestBeginAt(u32 slot, u32 partnerSlot);
void ModsOwnedLinkTradeTestConfirm(void);
void ModsOwnedLinkTradeTestEnd(void);
void ModsOwnedLinkTradeTestSaved(bool32 saved);
void ModsOwnedLinkTradeTestCompleteSave(void);
bool8 ModsOwnedLinkTradeTestBeginSave(void);
void ModsOwnedEvolutionTestCommit(u32 partySlot, enum Species species, bool32 trade);
void ModsOwnedEvolutionTestSplit(u32 partySlot, enum Species before, enum Species after);
int NativeOwnedTrackerTestRemember(struct NativeOwnedLedgerSlot slot);
void ModsOwnedSkyTestRestore(u8 participatedPokemon);
void ModsOwnedFollowerTestPrepare(void);
void ModsOwnedFollowerTestRestore(void);
void ModsOwnedCatchTestFollower(bool32 enabled);
bool32 ModsOwnedFollowerCatchTestMenu(bool32 begin);
#endif
#ifdef RHH_OWNED_IDENTITY_TEST
static void CheckLedgerSlot(struct NativeOwnedLedgerSlot slot, struct NativeOwnedId expected)
{
    struct NativeOwnedId got = {0, 0};
    CHECK(!NativeOwnedLedgerRead(456, slot, &got) && got.high == expected.high && got.low == expected.low);
}
static struct NativeOwnedId GetLedgerSlot(struct NativeOwnedLedgerSlot slot)
{
    struct NativeOwnedId id = {0, 0};
    CHECK(!NativeOwnedLedgerRead(456, slot, &id) && (id.high || id.low)); return id;
}
static bool32 DifferentLedgerId(struct NativeOwnedId a, struct NativeOwnedId b)
{ return a.high != b.high || a.low != b.low; }
static void InitOwnedLedgerWithoutObservers(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsFinalize() && !NativeSaveOpen(path));
    RhhModsSetRunContext(123, 456, 0);
    CHECK(!RhhModsHasHook(RHH_OWNED_CATCH_PLACED) && !RhhModsHasHook(RHH_BREED_EGG_CREATED)
        && !RhhModsHasHook(RHH_EGG_HATCHED) && !RhhModsHasHook(RHH_OWNED_INGAME_TRADE_COMPLETED)
        && !RhhModsHasHook(RHH_OWNED_LINK_TRADE_SAVED));
}
static bool32 sAuditOpen, sAuditOverlay, sAuditInputSeen, sAuditInputActive;
static RhhContentKey sAuditUiOwner;
static const struct RhhUiService *sAuditUi;
static void AuditUiDraw(const struct RhhUiCanvas *canvas, void *user) { (void)canvas; (void)user; }
static void AuditUiInput(const struct RhhUiInput *input, void *user)
{
    (void)input; (void)user; sAuditInputSeen = TRUE; sAuditInputActive = NativeOwnedTrackerActive(456);
    if (sAuditOverlay) { sAuditUi->overlay_close(sAuditUiOwner); gMain.callback2 = NULL; }
    else sAuditUi->close(sAuditUiOwner);
}
static void AuditUpdate(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    if (!sAuditOpen) return;
    if (sAuditOverlay)
    {
        const struct RhhUiOverlay overlay = {sizeof(overlay), 0, sAuditUiOwner, AuditUiInput, AuditUiDraw, NULL, NULL};
        CHECK(!sAuditUi->overlay_open(&overlay));
    }
    else
    {
        const struct RhhUiPanel panel = {sizeof(panel), 0, sAuditUiOwner, AuditUiInput, AuditUiDraw, NULL, NULL};
        CHECK(!sAuditUi->open(&panel));
    }
    sAuditOpen = FALSE;
}
static const struct RhhModHook sAuditHooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, AuditUpdate}
};
static const struct RhhModDefinition sAuditMod = {.struct_size = sizeof(sAuditMod), .abi_version = RHH_MOD_ABI_V1,
    .id = "test:owned_audit", .hooks = sAuditHooks, .hook_count = 1};
static void CheckOwnedLiveAudit(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sAuditMod) && !RhhModsFinalize() && !NativeSaveOpen(path));
    RhhModsSetRunContext(123, 456, 0); CHECK(!RhhModsHasHook(RHH_OWNED_MON_CHANGED));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon mon, replacement;
    CreateMonWithIVs(&mon, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    CreateMonWithIVs(&replacement, SPECIES_TORCHIC, 20, 98765, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = mon; gParties[B_TRAINER_PLAYER][1] = replacement;
    gPartiesCount[B_TRAINER_PLAYER] = 2;
    gPokemonStoragePtr->boxes[2][5] = mon.box; gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
    gPokemonStoragePtr->fusions[0] = mon;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(784, TRUE));
    rng_value_t rng = gRngValue, rng2 = gRng2Value; struct Pokemon copy = gParties[B_TRAINER_PLAYER][0];
    CHECK(!NativeOwnedTrackerAudit() && !memcmp(&copy, &gParties[B_TRAINER_PLAYER][0], sizeof(copy))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    u8 friendship = 50; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_FRIENDSHIP, &friendship);
    CHECK(!NativeOwnedTrackerAudit() && NativeOwnedTrackerActive(456));
    CHECK(ModsOwnedPartyTestSwap(0, 1) && !NativeOwnedTrackerAudit()
        && ModsOwnedPartyTestSwap(0, 1) && !NativeOwnedTrackerAudit());
    gMain.inBattle = TRUE; gPokemonStoragePtr->boxes[2][5] = replacement.box;
    CHECK(NativeOwnedTrackerAudit() == 1 && NativeOwnedTrackerActive(456));
    gPokemonStoragePtr->boxes[2][5] = mon.box; gMain.inBattle = FALSE;
    CHECK(!NativeOwnedTrackerAudit() && ModsOwnedStorageTestOpen());
    gPokemonStoragePtr->boxes[2][5] = replacement.box;
    CHECK(NativeOwnedTrackerAudit() == 1 && NativeOwnedTrackerActive(456));
    gPokemonStoragePtr->boxes[2][5] = mon.box; ModsOwnedStorageTestClose(); CHECK(!NativeOwnedTrackerAudit());
    CHECK(!NativeSaveBegin()); gPokemonStoragePtr->boxes[2][5] = replacement.box;
    CHECK(NativeOwnedTrackerAudit() == 1 && NativeOwnedTrackerActive(456));
    gPokemonStoragePtr->boxes[2][5] = mon.box; CHECK(NativeSaveEnd(785, FALSE) && !NativeOwnedTrackerAudit());
    for (u32 which = 0; which < 4; ++which)
    {
        if (which == 0) gParties[B_TRAINER_PLAYER][0] = replacement;
        else if (which == 1) gPokemonStoragePtr->boxes[2][5] = replacement.box;
        else if (which == 2) gSaveBlock1Ptr->daycare.mons[0].mon = replacement.box;
        else gPokemonStoragePtr->fusions[0] = replacement;
        CHECK(NativeOwnedTrackerAudit() == -1 && !NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; gPokemonStoragePtr->boxes[2][5] = mon.box;
        gSaveBlock1Ptr->daycare.mons[0].mon = mon.box; gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    for (u32 projection = 0; projection < 2; ++projection)
    {
        SavePlayerParty();
        if (!projection) NativeOwnedTrackSkyBegin(1);
        else { const u8 order[] = {1}; NativeOwnedTrackUnionBegin(order, 1); }
        CHECK(NativeOwnedTrackerActive(456));
        ZeroMonData(&gSaveBlock1Ptr->playerParty[0]);
        CHECK(!NativeOwnedTrackerActive(456));
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    /* A stale backup can share OT and personality with a different copy.
     * Reject both selected and unselected mismatches before projection. */
    for (u32 projection = 0; projection < 2; ++projection)
    for (u32 slot = 0; slot < 2; ++slot)
    {
        SavePlayerParty(); GetSavedPlayerPartyMon(slot)->hp ^= 1;
        if (!projection) { const u8 order[] = {1}; NativeOwnedTrackUnionBegin(order, 1); }
        else NativeOwnedTrackRentalBegin();
        CHECK(!NativeOwnedTrackerActive(456));
        SavePlayerParty(); CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    sAuditUi = (const void *)RhhModsHostAPI()->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 3));
    sAuditUiOwner = RhhModsHostAPI()->content_key("test:owned_audit"); CHECK(sAuditUi && sAuditUiOwner);
    struct RhhModEvent tick = {.struct_size = sizeof(tick), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    sAuditOpen = TRUE; RhhModsDispatch(&tick, &decision); CHECK(sAuditUi->active());
    gParties[B_TRAINER_PLAYER][0] = replacement;
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); cgb_audio_init(42060); m4aSoundInit(); NativeFrame(); SDL_Quit();
    CHECK(sAuditInputSeen && !sAuditInputActive && !NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    RhhUiFilterKeys(0); ScriptContext_Init(); UnlockPlayerFieldControls();
    gMain.callback2 = CB2_Overworld; gPaletteFade.active = gPaletteFade.y = 0;
    sAuditOverlay = sAuditOpen = TRUE; sAuditInputSeen = sAuditInputActive = FALSE;
    RhhModsDispatch(&tick, &decision); CHECK(sAuditUi->overlay_active());
    gParties[B_TRAINER_PLAYER][0] = replacement;
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); cgb_audio_init(42060); m4aSoundInit(); NativeFrame(); SDL_Quit();
    CHECK(sAuditInputSeen && !sAuditInputActive && !NativeOwnedTrackerActive(456));
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned live audit: stable views, unsupported changes and UI ordering passed");
}
static void CheckOwnedAuditCapacity(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = mon; gPartiesCount[B_TRAINER_PLAYER] = 1;
    for (u32 box = 0; box < TOTAL_BOXES_COUNT; ++box)
        for (u32 slot = 0; slot < IN_BOX_COUNT; ++slot) gPokemonStoragePtr->boxes[box][slot] = mon.box;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    u64 start = SDL_GetPerformanceCounter();
    for (u32 i = 0; i < 120; ++i) CHECK(!NativeOwnedTrackerAudit());
    double milliseconds = 1000.0 * (SDL_GetPerformanceCounter() - start) / SDL_GetPerformanceFrequency();
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerStop(456));
    NativeSaveClose(); RhhModsUnloadLibraries();
    printf("owned live audit: full storage 120 checks %.1f ms\n", milliseconds);
}
static void CheckOwnedCreationAudit(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon mon, temporary;
    CreateMonWithIVs(&mon, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = mon; gPartiesCount[B_TRAINER_PLAYER] = 1;
    gPokemonStoragePtr->boxes[2][5] = mon.box; gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
    gPokemonStoragePtr->fusions[0] = mon;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(784, TRUE));
    CreateMonWithIVs(&temporary, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    for (u32 which = 0; which < 6; ++which)
    {
        if (which == 0)
            CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
        else if (which == 1)
        {
            gMain.inBattle = TRUE;
            CreateMon(&gParties[B_TRAINER_PLAYER][4], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321));
            gMain.inBattle = FALSE;
        }
        else if (which == 2)
            CreateBoxMon(&gPokemonStoragePtr->boxes[2][5], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321));
        else if (which == 3)
            CreateBoxMon(&gPokemonStoragePtr->boxes[3][5], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321));
        else if (which == 4)
            CreateBoxMon(&gSaveBlock1Ptr->daycare.mons[0].mon, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321));
        else
            CreateMon(&gPokemonStoragePtr->fusions[0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321));
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; ZeroMonData(&gParties[B_TRAINER_PLAYER][4]);
        gPokemonStoragePtr->boxes[2][5] = mon.box; ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][5]);
        gSaveBlock1Ptr->daycare.mons[0].mon = mon.box; gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!NativeSaveBegin());
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(785, TRUE) && !NativeSaveTransactionActive());
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    SavePlayerParty();
    CreateMon(&gSaveBlock1Ptr->playerParty[0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321));
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    for (u32 projection = 0; projection < 2; ++projection)
    {
        SavePlayerParty();
        if (!projection) NativeOwnedTrackSkyBegin(1);
        else { const u8 order[] = {1}; NativeOwnedTrackUnionBegin(order, 1); }
        CHECK(NativeOwnedTrackerActive(456));
        CreateMon(&gSaveBlock1Ptr->playerParty[0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321));
        CHECK(!NativeOwnedTrackerActive(456));
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!NativeSaveBegin()); ZeroPlayerPartyMons();
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(786, TRUE));
    gParties[B_TRAINER_PLAYER][0] = mon; gPartiesCount[B_TRAINER_PLAYER] = 1;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    struct RhhOwnedRelease release;
    CHECK(ModsOwnedReleaseCapture(&gParties[B_TRAINER_PLAYER][0].box, RHH_MON_PARTY, 0, 0, FALSE, &release));
    ZeroMonDataWithOwnedReceipt(&gParties[B_TRAINER_PLAYER][0]); gPartiesCount[B_TRAINER_PLAYER] = 0;
    ModsOwnedReleaseCommitted(&release); CHECK(NativeOwnedTrackerActive(456));
    ZeroPlayerPartyMons(); CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    CHECK(!NativeOwnedTrackerStop(456));
    /* Recorded playback replaces all player slots by assignment. Even with
     * an empty tracked party, or a byte-identical empty recording, those
     * writes must fail closed rather than bypassing the clear guard. */
    for (u32 scenario = 0; scenario < 2; ++scenario)
    {
        u64 run = 457 + scenario;
        memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
        gPartiesCount[B_TRAINER_PLAYER] = 0; RhhModsSetRunContext(123, run, 0);
        CHECK(!NativeOwnedLedgerStart(run) && !NativeOwnedTrackerSeed(run) && !NativeOwnedTrackerStart(run));
        struct RecordedBattleSave recording = {0};
        if (scenario) recording.parties[B_TRAINER_PLAYER][0] = mon;
        if (scenario) CHECK(!NativeSaveBegin());
        SetPartiesFromRecordedSave(&recording);
        CHECK(!NativeOwnedTrackerActive(run));
        if (scenario) CHECK(NativeSaveEnd(785, TRUE) && !NativeSaveTransactionActive());
        CHECK(!memcmp(&gParties[B_TRAINER_PLAYER][0], &recording.parties[B_TRAINER_PLAYER][0], sizeof(mon)));
    }
    /* The normal playback lifecycle keeps the original party in a checked
     * buffer and exposes the recording as a non-owned party view. */
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    gParties[B_TRAINER_PLAYER][0] = mon; gPartiesCount[B_TRAINER_PLAYER] = 1;
    RhhModsSetRunContext(123, 459, 0);
    CHECK(!NativeOwnedLedgerStart(459) && !NativeOwnedTrackerSeed(459) && !NativeOwnedTrackerStart(459));
    struct NativeOwnedLedgerSlot partySlot = {NATIVE_OWNED_PARTY, 0, 0};
    struct NativeOwnedId idBefore, idAfter; CHECK(!NativeOwnedLedgerRead(459, partySlot, &idBefore));
    struct Pokemon backup[PARTY_SIZE]; memcpy(backup, gParties[B_TRAINER_PLAYER], sizeof(backup));
    const struct RhhOwnedMonService *owned = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 38));
    CHECK(owned); ModsOwnedRecordedPartyBegin(backup, 1);
    struct RhhOwnedMonSnapshot snap = {.struct_size = sizeof(snap)};
    CHECK(NativeOwnedTrackerActive(459) && owned->party_view() == RHH_OWNED_PARTY_RECORDED && owned->party_at(0, &snap));
    struct RecordedBattleSave playback = {0}; playback.parties[B_TRAINER_PLAYER][0] = mon;
    playback.parties[B_TRAINER_PLAYER][1] = mon; SetPartiesFromRecordedSave(&playback);
    CHECK(NativeOwnedTrackerActive(459) && !NativeOwnedTrackerAudit()
        && !NativeOwnedLedgerRead(459, partySlot, &idAfter) && !DifferentLedgerId(idBefore, idAfter));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_PIDGEY, 10, 987, OTID_STRUCT_PRESET(123), 9);
    CHECK(NativeOwnedTrackerActive(459) && !NativeOwnedTrackerAudit());
    ModsOwnedRecordedPartyBeforeRestore(backup); memcpy(gParties[B_TRAINER_PLAYER], backup, sizeof(backup));
    gPartiesCount[B_TRAINER_PLAYER] = 1; ModsOwnedRecordedPartyAfterRestore();
    CHECK(NativeOwnedTrackerActive(459) && owned->party_view() == RHH_OWNED_PARTY_FIELD_ORDER
        && !owned->party_at(0, &snap) && !NativeOwnedTrackerAudit()
        && !NativeOwnedLedgerRead(459, partySlot, &idAfter) && !DifferentLedgerId(idBefore, idAfter));
    ModsOwnedRecordedPartyBegin(backup, 1); CHECK(NativeOwnedTrackerActive(459));
    backup[0].box.nickname[0] ^= 1;
    ModsOwnedRecordedPartyBeforeRestore(backup); CHECK(!NativeOwnedTrackerActive(459));
    backup[0].box.nickname[0] ^= 1; ModsOwnedRecordedPartyAfterRestore();
    /* Exercise the real playback entry and end callback against a valid
     * special save sector, rather than only calling projection helpers. */
    RhhModsSetRunContext(123, 460, 0);
    CHECK(!NativeOwnedLedgerStart(460) && !NativeOwnedTrackerSeed(460) && !NativeOwnedTrackerStart(460));
    CHECK(!NativeOwnedLedgerRead(460, partySlot, &idBefore));
    struct RecordedBattleSave savedRecording = {0};
    savedRecording.parties[B_TRAINER_PLAYER][0] = mon; savedRecording.battleFlags = BATTLE_TYPE_TRAINER;
    savedRecording.checksum = CalcByteArraySum((void *)&savedRecording, sizeof(savedRecording) - 4);
    gFlashMemoryPresent = TRUE;
    CHECK(!NativeSaveSpecialWrite(SECTOR_ID_RECORDED_BATTLE, &savedRecording, sizeof(savedRecording)));
    ResetTasks(); cgb_audio_init(42060); m4aSoundInit();
    PlayRecordedBattle(CB2_Overworld);
    CHECK(NativeOwnedTrackerActive(460) && owned->party_view() == RHH_OWNED_PARTY_RECORDED && owned->party_at(0, &snap));
    for (u32 frame = 0; frame < 128; ++frame) RunTasks();
    CHECK(gMain.savedCallback); gMain.savedCallback();
    CHECK(NativeOwnedTrackerActive(460) && owned->party_view() == RHH_OWNED_PARTY_FIELD_ORDER
        && gPartiesCount[B_TRAINER_PLAYER] == 1 && !owned->party_at(0, &snap) && !NativeOwnedTrackerAudit()
        && !NativeOwnedLedgerRead(460, partySlot, &idAfter) && !DifferentLedgerId(idBefore, idAfter));
    CHECK(!NativeOwnedTrackerStop(460));
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned creation audit: unreported identical and empty-slot creations fail closed");
}
static void OwnedRawControllerComplete(enum BattlerId battler) { (void)battler; }
static void CheckOwnedCopyAudit(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon mon, temporary;
    CreateMonWithIVs(&mon, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = mon; gPartiesCount[B_TRAINER_PLAYER] = 1;
    gPokemonStoragePtr->boxes[2][5] = mon.box; gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
    CreateMonWithIVs(&temporary, SPECIES_DITTO, 20, 45678, OTID_STRUCT_PRESET(12345), 31);
    gSaveBlock1Ptr->daycare.mons[1].mon = temporary.box;
    gPokemonStoragePtr->fusions[0] = mon;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(784, TRUE));
    const struct RhhOwnedMonService *memoryOwned = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 44));
    CHECK(memoryOwned && memoryOwned->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_44_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 47)));
    /* Successful nature codec outputs must not silently replace an owned
     * slot; checked in-place transforms preserve the individual. */
    unsigned natural, overridden, required;
    struct BoxPokemon converted = mon.box, natureCorrupt = mon.box;
    struct Pokemon convertedParty = mon;
    CHECK(!NativeNatureStorageRead(&mon.box, &natural, &overridden)
        && !NativeNatureLinkExport(&gParties[B_TRAINER_PLAYER][0].box, 1, &gParties[B_TRAINER_PLAYER][0].box)
        && !NativeNatureLinkImport(&gParties[B_TRAINER_PLAYER][0].box, 1, &gParties[B_TRAINER_PLAYER][0].box, &required)
        && !NativeNaturePartyLinkExport(gParties[B_TRAINER_PLAYER], 1, 1, gParties[B_TRAINER_PLAYER])
        && !NativeNaturePartyLinkImport(gParties[B_TRAINER_PLAYER], 1, 1, gParties[B_TRAINER_PLAYER], &required)
        && !NativeNatureStorageWrite(&gParties[B_TRAINER_PLAYER][0].box, (natural + 1) % NUM_NATURES)
        && !NativeNatureStorageWrite(&gParties[B_TRAINER_PLAYER][0].box, natural)
        && NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    natureCorrupt.secure.raw[0] ^= 1;
    CHECK(NativeNatureStorageRead(&natureCorrupt, &gParties[B_TRAINER_PLAYER][0].box.personality, &overridden)
        && NativeNatureLinkExport(&natureCorrupt, 1, &gParties[B_TRAINER_PLAYER][0].box)
        && NativeNatureLinkImport(&natureCorrupt, 1, &gParties[B_TRAINER_PLAYER][0].box, &required)
        && NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    for (u32 which = 0; which < 9; ++which) {
        if (which == 0) CHECK(!NativeNatureStorageRead(&mon.box, &gParties[B_TRAINER_PLAYER][0].box.personality, &overridden));
        else if (which == 1) CHECK(!NativeNatureStorageRead(&mon.box, &natural, &gPokemonStoragePtr->boxes[2][5].personality));
        else if (which == 2) CHECK(!NativeNatureLinkExport(&mon.box, 1, &gParties[B_TRAINER_PLAYER][0].box));
        else if (which == 3) CHECK(!NativeNatureLinkImport(&mon.box, 1, &gParties[B_TRAINER_PLAYER][0].box, &required));
        else if (which == 4) CHECK(!NativeNatureLinkImport(&mon.box, 1, &converted, &gPokemonStoragePtr->fusions[0].box.personality));
        else if (which == 5) CHECK(!NativeNaturePartyLinkExport(&mon, 1, 1, &gParties[B_TRAINER_PLAYER][0]));
        else if (which == 6) CHECK(!NativeNaturePartyLinkImport(&mon, 1, 1, &gParties[B_TRAINER_PLAYER][0], &required));
        else if (which == 7) CHECK(!NativeNaturePartyLinkImport(&mon, 1, 1, &convertedParty, &gPokemonStoragePtr->fusions[0].box.personality));
        else CHECK(!NativeNatureLinkExport(&mon.box, 1, &gPokemonStoragePtr->boxes[3][5]));
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; gPokemonStoragePtr->boxes[2][5] = mon.box;
        ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][5]); gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!NativeSaveBegin() && !NativeNatureLinkExport(&mon.box, 1, &gParties[B_TRAINER_PLAYER][0].box)
        && !NativeOwnedTrackerActive(456) && NativeSaveEnd(785, TRUE) && !NativeSaveTransactionActive());
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* Native CPU, DMA and decompression writes can otherwise bypass the
     * Pokémon copy receipts, including byte-identical slot replacements. */
    struct Pokemon memoryScratch;
    CpuSet(&mon, &memoryScratch, CPU_SET_32BIT | (sizeof(mon) / 4));
    CpuSet(&gParties[B_TRAINER_PLAYER][0], &gParties[B_TRAINER_PLAYER][0], CPU_SET_32BIT | (sizeof(mon) / 4));
    DmaSet(3, &gParties[B_TRAINER_PLAYER][0], &gParties[B_TRAINER_PLAYER][0],
           ((u32)(DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (sizeof(mon) / 4));
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    const u8 lz[] = {0x10, 3, 0, 0, 0, 1, 2, 3};
    const u8 rl[] = {0x30, 3, 0, 0, 0x80, 0};
    for (u32 which = 0; which < 8; ++which) {
        if (which == 0) CpuSet(&mon, &gParties[B_TRAINER_PLAYER][0], CPU_SET_32BIT | (sizeof(mon) / 4));
        else if (which == 1) CpuFastSet(&mon.box, &gPokemonStoragePtr->boxes[2][5], 8);
        else if (which == 2) CpuSet(&gParties[B_TRAINER_PLAYER][0], &gParties[B_TRAINER_PLAYER][0], CPU_SET_32BIT | CPU_SET_SRC_FIXED | 4);
        else if (which == 3) DmaSet(3, &mon.box, &gSaveBlock1Ptr->daycare.mons[0].mon,
                                    ((u32)(DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (sizeof(mon.box) / 4));
        else if (which == 4) {
            DmaSet(0, &mon, &gParties[B_TRAINER_PLAYER][0],
                   ((u32)(DMA_ENABLE | DMA_32BIT | DMA_START_HBLANK | DMA_SRC_INC | DMA_DEST_INC) << 16) | (sizeof(mon) / 4));
            CHECK(NativeOwnedTrackerActive(456)); NativeRunDma(2);
        }
        else if (which == 5) DmaSet(3, (u8 *)&mon + sizeof(mon) - 4,
            (u8 *)&gParties[B_TRAINER_PLAYER][0] + sizeof(mon) - 4,
            ((u32)(DMA_ENABLE | DMA_32BIT | DMA_SRC_DEC | DMA_DEST_DEC) << 16) | (sizeof(mon) / 4));
        else if (which == 6) LZ77UnCompWram((const u32 *)lz, &gPokemonStoragePtr->boxes[3][5]);
        else RLUnCompWram((const u32 *)rl, &gPokemonStoragePtr->fusions[0]);
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; gPokemonStoragePtr->boxes[2][5] = mon.box;
        ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][5]); gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
        gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    struct Pokemon repeatedSource[2] = {mon, mon};
    DmaSet(0, repeatedSource, &gParties[B_TRAINER_PLAYER][0],
           ((u32)(DMA_ENABLE | DMA_32BIT | DMA_START_HBLANK | DMA_REPEAT | DMA_SRC_INC | DMA_DEST_RELOAD) << 16) | (sizeof(mon) / 4));
    CHECK(NativeOwnedTrackerActive(456)); NativeRunDma(2); CHECK(!NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    NativeRunDma(2); CHECK(!NativeOwnedTrackerActive(456)); DmaStop(0);
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* Both Factory screens can replace a slot with identical bytes. Without a
     * verified rental projection that still loses individual provenance. */
    for (u32 swap = 0; swap < 2; ++swap)
    {
        ModsOwnedFactoryTestCopy(swap, &mon);
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    /* The same engine writes are expected while the original party is
     * protected by a checked rental projection. */
    SavePlayerParty(); NativeOwnedTrackRentalBegin();
    CHECK(NativeOwnedTrackerActive(456));
    ModsOwnedFactoryTestCopy(FALSE, &mon);
    ModsOwnedFactoryTestCopy(TRUE, &mon);
    CHECK(NativeOwnedTrackerActive(456));
    NativeOwnedTrackRentalBeforeRestore(); NativeOwnedTrackBeforePartyLoad();
    memcpy(gParties[B_TRAINER_PLAYER], gSaveBlock1Ptr->playerParty, sizeof(gParties[B_TRAINER_PLAYER]));
    gPartiesCount[B_TRAINER_PLAYER] = *GetSavedPlayerPartyCount();
    NativeOwnedTrackRentalRestored();
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    /* Name getters also write to caller-owned buffers. An alias into owned
     * storage is an unreported write, even when source and destination agree. */
    u8 name[POKEMON_NAME_LENGTH + 1];
    GetBoxMonData(&mon.box, MON_DATA_NICKNAME, name);
    GetBoxMonData(&mon.box, MON_DATA_OT_NAME, name);
    GetBoxMonData(&mon.box, MON_DATA_SPECIES, (u8 *)&gParties[B_TRAINER_PLAYER][0].box);
    GetMonNicknameVanilla(&mon, name); GetBoxMonNickname(&mon.box, name);
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    for (u32 target = 0; target < 6; ++target)
    {
        if (target == 0) GetBoxMonData(&mon.box, MON_DATA_NICKNAME, gParties[B_TRAINER_PLAYER][0].box.nickname);
        else if (target == 1) GetBoxMonData(&gParties[B_TRAINER_PLAYER][0].box, MON_DATA_OT_NAME, (u8 *)&gParties[B_TRAINER_PLAYER][0].box);
        else if (target == 2) GetBoxMonData(&mon.box, MON_DATA_NICKNAME10, (u8 *)&gPokemonStoragePtr->boxes[3][5]);
        else if (target == 3) GetBoxMonData(&mon.box, MON_DATA_OT_NAME, (u8 *)&gPokemonStoragePtr->boxes[2][5]);
        else if (target == 4) GetBoxMonData(&mon.box, MON_DATA_NICKNAME, (u8 *)&gSaveBlock1Ptr->daycare.mons[0].mon);
        else GetBoxMonData(&mon.box, MON_DATA_NICKNAME10, (u8 *)&gPokemonStoragePtr->fusions[0]);
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; gPokemonStoragePtr->boxes[2][5] = mon.box;
        ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][5]); gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
        gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    for (u32 target = 0; target < 6; ++target)
    {
        if (target == 0) GetMonNicknameVanilla(&mon, (u8 *)&gParties[B_TRAINER_PLAYER][0].box);
        else if (target == 1) GetBoxMonNickname(&gParties[B_TRAINER_PLAYER][0].box, gParties[B_TRAINER_PLAYER][0].box.nickname);
        else if (target == 2) GetMonNicknameVanilla(&mon, (u8 *)&gPokemonStoragePtr->boxes[3][5]);
        else if (target == 3) GetBoxMonNickname(&mon.box, (u8 *)&gPokemonStoragePtr->boxes[2][5]);
        else if (target == 4) GetMonNicknameVanilla(&mon, (u8 *)&gSaveBlock1Ptr->daycare.mons[0].mon);
        else GetBoxMonNickname(&mon.box, (u8 *)&gPokemonStoragePtr->fusions[0]);
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; gPokemonStoragePtr->boxes[2][5] = mon.box;
        ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][5]); gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
        gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    /* An empty nickname writes just EOS. A buffer ending immediately before
     * storage must not be treated as overlapping the first box. */
    struct Pokemon emptyNickname = mon; u8 blank[POKEMON_NAME_LENGTH + 1]; memset(blank, EOS, sizeof(blank));
    SetMonData(&emptyNickname, MON_DATA_NICKNAME, blank);
    u8 *beforeBoxes = (u8 *)gPokemonStoragePtr + offsetof(struct PokemonStorage, boxes) - 1, beforeByte = *beforeBoxes;
    GetBoxMonNickname(&emptyNickname.box, beforeBoxes); *beforeBoxes = beforeByte;
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    /* Other helpers with output pointers must guard their own writes too. */
    struct BattleTowerPokemon tower;
    u32 tera = 0; ConvertPokemonToBattleTowerPokemon(&mon, &tower);
    CHECK(GetBoxMonStoredTeraType(&mon.box, &tera) && NativeOwnedTrackerActive(456));
    struct BoxPokemon corrupt = mon.box; corrupt.checksum ^= 1;
    CHECK(!GetBoxMonStoredTeraType(&corrupt, &gParties[B_TRAINER_PLAYER][0].box.personality)
        && NativeOwnedTrackerActive(456));
    CHECK(GetBoxMonStoredTeraType(&mon.box, &gParties[B_TRAINER_PLAYER][0].box.personality)
        && !NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    _Static_assert(sizeof(struct BattleTowerPokemon) <= sizeof(struct Pokemon), "tower test output fits party slot");
    ConvertPokemonToBattleTowerPokemon(&mon, (struct BattleTowerPokemon *)&gParties[B_TRAINER_PLAYER][0]);
    CHECK(!NativeOwnedTrackerActive(456)); gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    struct Pokemon egg; u32 eggSpecies; u8 eggParents[DAYCARE_MON_COUNT];
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    CHECK(!GetDaycareExactEggPreview(&egg, &gParties[B_TRAINER_PLAYER][0].box.personality, eggParents)
        && NativeOwnedTrackerActive(456));
    FlagSet(FLAG_PENDING_DAYCARE_EGG);
    rng_value_t eggRng = gRngValue, eggRng2 = gRng2Value;
    CHECK(GetDaycareExactEggPreview(&egg, &eggSpecies, eggParents) && NativeOwnedTrackerActive(456));
    gRngValue = eggRng; gRng2Value = eggRng2;
    for (u32 target = 0; target < 3; ++target)
    {
        struct Pokemon *eggOut = target == 0 ? &gParties[B_TRAINER_PLAYER][0] : &egg;
        u32 *speciesOut = target == 1 ? &gParties[B_TRAINER_PLAYER][0].box.personality : &eggSpecies;
        u8 *parentsOut = target == 2 ? (u8 *)&gParties[B_TRAINER_PLAYER][0].box : eggParents;
        CHECK(GetDaycareExactEggPreview(eggOut, speciesOut, parentsOut) && !NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
        gRngValue = eggRng; gRng2Value = eggRng2;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    struct DaycareIVRules ivRules; u8 weights[3], roles[DAYCARE_MON_COUNT] = {0, 1};
    u16 previewMoves[MAX_MON_MOVES]; s32 abilityParent; u32 abilitySlot, abilityPercent;
    GetDaycareNatureSourceWeights(&gSaveBlock1Ptr->daycare, GEN_6, weights);
    GetDaycareIVRules(&gSaveBlock1Ptr->daycare, &ivRules);
    GetDaycareBallSourceWeights(&gSaveBlock1Ptr->daycare, GEN_6, weights);
    GetDaycareAbilityInheritance(&gSaveBlock1Ptr->daycare, GEN_6, &abilityParent, &abilitySlot, &abilityPercent);
    CHECK(GetDaycareMovePreview(&gSaveBlock1Ptr->daycare, SPECIES_TORCHIC, roles, previewMoves)
        && GetDaycareOffspringCandidate(&gSaveBlock1Ptr->daycare, FALSE, roles) == SPECIES_TORCHIC
        && NativeOwnedTrackerActive(456));
    /* Invalid move previews return before touching the aliased output. */
    CHECK(!GetDaycareMovePreview(&gSaveBlock1Ptr->daycare, SPECIES_NONE, roles,
                                 (u16 *)&gParties[B_TRAINER_PLAYER][0].box)
        && NativeOwnedTrackerActive(456));
    for (u32 target = 0; target < 8; ++target)
    {
        void *alias = &gParties[B_TRAINER_PLAYER][0].box;
        if (target == 0) GetDaycareNatureSourceWeights(&gSaveBlock1Ptr->daycare, GEN_6, alias);
        else if (target == 1) GetDaycareIVRules(&gSaveBlock1Ptr->daycare, alias);
        else if (target == 2) GetDaycareBallSourceWeights(&gSaveBlock1Ptr->daycare, GEN_6, alias);
        else if (target == 3) GetDaycareAbilityInheritance(&gSaveBlock1Ptr->daycare, GEN_6, alias, &abilitySlot, &abilityPercent);
        else if (target == 4) GetDaycareAbilityInheritance(&gSaveBlock1Ptr->daycare, GEN_6, &abilityParent, alias, &abilityPercent);
        else if (target == 5) GetDaycareAbilityInheritance(&gSaveBlock1Ptr->daycare, GEN_6, &abilityParent, &abilitySlot, alias);
        else if (target == 6) CHECK(GetDaycareMovePreview(&gSaveBlock1Ptr->daycare, SPECIES_TORCHIC, roles, alias));
        else (void)GetDaycareOffspringCandidate(&gSaveBlock1Ptr->daycare, FALSE, alias);
        CHECK(!NativeOwnedTrackerActive(456)); gParties[B_TRAINER_PLAYER][0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!GiveEggFromDaycareChecked(&gParties[B_TRAINER_PLAYER][0].box.personality) && NativeOwnedTrackerActive(456));
    FlagSet(FLAG_PENDING_DAYCARE_EGG); u32 receivedSlot = PARTY_SIZE;
    CHECK(GiveEggFromDaycareChecked(&receivedSlot) && receivedSlot == 1 && NativeOwnedTrackerActive(456)
        && !NativeOwnedTrackerAudit() && !NativeOwnedTrackerStop(456));
    memset(&gParties[B_TRAINER_PLAYER][1], 0, sizeof(struct Pokemon)); gPartiesCount[B_TRAINER_PLAYER] = 1;
    gRngValue = eggRng; gRng2Value = eggRng2;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    FlagSet(FLAG_PENDING_DAYCARE_EGG);
    CHECK(GiveEggFromDaycareChecked(&gParties[B_TRAINER_PLAYER][0].box.personality) && !NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    memset(&gParties[B_TRAINER_PLAYER][1], 0, sizeof(struct Pokemon)); gPartiesCount[B_TRAINER_PLAYER] = 1;
    gRngValue = eggRng; gRng2Value = eggRng2;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    CHECK(!NativeSaveBegin());
    GetBoxMonData(&mon.box, MON_DATA_OT_NAME, (u8 *)&gParties[B_TRAINER_PLAYER][0].box);
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(785, TRUE) && !NativeSaveTransactionActive());
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    CopyMon(&temporary, &mon, sizeof(mon));
    CopyMon(&gParties[B_TRAINER_OPPONENT_A][0], &mon, sizeof(mon));
    CopyMon(&gParties[B_TRAINER_PLAYER][0], &gParties[B_TRAINER_PLAYER][0], sizeof(mon));
    BoxMonToMon(&mon.box, &temporary);
    BoxMonToMon(&mon.box, &gParties[B_TRAINER_OPPONENT_A][0]);
    BoxMonToMon(&gParties[B_TRAINER_PLAYER][0].box, &gParties[B_TRAINER_PLAYER][0]);
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    BoxMonToMon(&mon.box, &gParties[B_TRAINER_PLAYER][0]);
    CHECK(!NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    BoxMonToMon(&mon.box, &gParties[B_TRAINER_PLAYER][4]);
    CHECK(!NativeOwnedTrackerActive(456));
    ZeroMonData(&gParties[B_TRAINER_PLAYER][4]);
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    SetBoxMonAt(2, 5, &gPokemonStoragePtr->boxes[2][5]);
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    SetBoxMonAt(2, 5, &mon.box);
    CHECK(!NativeOwnedTrackerActive(456));
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    SetBoxMonAt(3, 5, &mon.box);
    CHECK(!NativeOwnedTrackerActive(456));
    ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][5]);
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    ZeroBoxMonAt(3, 5);
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    ZeroBoxMonAt(2, 5);
    CHECK(!NativeOwnedTrackerActive(456));
    gPokemonStoragePtr->boxes[2][5] = mon.box;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* Raw clears in each owned location must not disappear between audits. */
    ZeroMonData(&temporary);
    ZeroMonData(&gParties[B_TRAINER_OPPONENT_A][0]);
    ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][5]);
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    for (u32 which = 0; which < 4; ++which)
    {
        if (which == 0) ZeroMonData(&gParties[B_TRAINER_PLAYER][0]);
        else if (which == 1) ZeroBoxMonData(&gPokemonStoragePtr->boxes[2][5]);
        else if (which == 2) ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon);
        else ZeroMonData(&gPokemonStoragePtr->fusions[0]);
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon;
        gPokemonStoragePtr->boxes[2][5] = mon.box;
        gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
        gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    /* An apparently empty slot with a binding is already inconsistent. */
    gPokemonStoragePtr->boxes[2][5].hasSpecies = FALSE;
    ZeroBoxMonAt(2, 5);
    CHECK(!NativeOwnedTrackerActive(456));
    gPokemonStoragePtr->boxes[2][5] = mon.box;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    CHECK(!NativeSaveBegin()); ZeroBoxMonAt(2, 5);
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(785, TRUE) && !NativeSaveTransactionActive());
    gPokemonStoragePtr->boxes[2][5] = mon.box;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* An unreported party reload cannot reuse identities, even for equal data. */
    SavePlayerParty(); LoadPlayerParty(); CHECK(!NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); RestorePlayerPartyAfterTutorialDenial(); CHECK(!NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); CHECK(!NativeSaveBegin()); LoadPlayerParty();
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(785, TRUE) && !NativeSaveTransactionActive());
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* The controller's raw write bypasses Pokémon setters, even when it
       copies an identical byte. Opponent and empty writes are harmless. */
    struct BattleResources resources = {0}, *oldResources = gBattleResources;
    struct BattleSpriteInfo spriteInfo[MAX_BATTLERS_COUNT] = {0};
    struct BattleSpriteData spriteData = {.battlerData = spriteInfo}, *oldSpriteData = gBattleSpritesDataPtr;
    void (*oldEnd0)(enum BattlerId) = gBattlerControllerEndFuncs[0];
    void (*oldEnd1)(enum BattlerId) = gBattlerControllerEndFuncs[1];
    u32 oldFlags = gBattleTypeFlags; bool32 oldBattle = gMain.inBattle;
    u16 oldIndex0 = gBattlerPartyIndexes[0], oldIndex1 = gBattlerPartyIndexes[1];
    gBattleResources = &resources; gBattleSpritesDataPtr = &spriteData;
    gBattlerControllerEndFuncs[0] = gBattlerControllerEndFuncs[1] = OwnedRawControllerComplete;
    gBattlerPartyIndexes[0] = gBattlerPartyIndexes[1] = 0; gBattleTypeFlags = 0; gMain.inBattle = TRUE;
    gParties[B_TRAINER_OPPONENT_A][0] = mon;
    /* Form notifications normally repeat the already committed species. A
       changed controller payload has no independent form-change receipt. */
    u16 species = SPECIES_TORCHIC;
    resources.bufferA[0][1] = REQUEST_SPECIES_BATTLE;
    memcpy(&resources.bufferA[0][3], &species, sizeof(species));
    BtlController_HandleSetMonData(0); CHECK(NativeOwnedTrackerActive(456));
    species = SPECIES_PIDGEY; memcpy(&resources.bufferA[0][3], &species, sizeof(species));
    BtlController_HandleSetMonData(0); CHECK(!NativeOwnedTrackerActive(456));
    gMain.inBattle = FALSE; gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456)); gMain.inBattle = TRUE;
    _Static_assert(sizeof(struct Pokemon) < 256, "raw controller offset width");
    struct Pokemon monBefore = gParties[B_TRAINER_PLAYER][0], nextBefore = gParties[B_TRAINER_PLAYER][1];
    resources.bufferA[0][1] = sizeof(struct Pokemon); resources.bufferA[0][2] = 1;
    resources.bufferA[0][3] = 0xA5;
    BtlController_HandleSetRawMonData(0);
    CHECK(NativeOwnedTrackerActive(456) && !memcmp(&nextBefore, &gParties[B_TRAINER_PLAYER][1], sizeof(nextBefore)));
    resources.bufferA[0][1] = sizeof(struct Pokemon) - 1; resources.bufferA[0][2] = 2;
    resources.bufferA[0][4] = 0x5A;
    BtlController_HandleSetRawMonData(0);
    CHECK(NativeOwnedTrackerActive(456) && !memcmp(&monBefore, &gParties[B_TRAINER_PLAYER][0], sizeof(monBefore))
        && !memcmp(&nextBefore, &gParties[B_TRAINER_PLAYER][1], sizeof(nextBefore)));
    u32 nickOffset = offsetof(struct Pokemon, box) + offsetof(struct BoxPokemon, nickname);
    CHECK(nickOffset < 256);
    resources.bufferA[0][1] = resources.bufferA[1][1] = nickOffset;
    resources.bufferA[0][3] = resources.bufferA[1][3] = mon.box.nickname[0];
    resources.bufferA[0][2] = 0; BtlController_HandleSetRawMonData(0); CHECK(NativeOwnedTrackerActive(456));
    resources.bufferA[1][2] = 1; BtlController_HandleSetRawMonData(1); CHECK(NativeOwnedTrackerActive(456));
    resources.bufferA[0][2] = 1; BtlController_HandleSetRawMonData(0); CHECK(!NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattlerPartyIndexes[0] = oldIndex0; gBattlerPartyIndexes[1] = oldIndex1;
    gBattleResources = oldResources; gBattleSpritesDataPtr = oldSpriteData;
    gBattlerControllerEndFuncs[0] = oldEnd0; gBattlerControllerEndFuncs[1] = oldEnd1;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* Link-battle self echoes are safe only if the complete byte sequence is
       unchanged, including empty slots and fields outside the identity witness. */
    struct Pokemon echoed[2]; memcpy(echoed, gParties[B_TRAINER_PLAYER], sizeof(echoed));
    NativeOwnedTrackLocalEcho(gParties[B_TRAINER_PLAYER], echoed, sizeof(echoed));
    NativeOwnedTrackLocalEcho(&gParties[B_TRAINER_PLAYER][2], &gParties[B_TRAINER_PLAYER][2], sizeof(mon));
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    /* The later multi-battle block contains only the third party slot. */
    NativeOwnedTrackLocalEcho(&gParties[B_TRAINER_PLAYER][2], &mon, sizeof(mon));
    CHECK(!NativeOwnedTrackerActive(456));
    ZeroMonData(&gParties[B_TRAINER_PLAYER][2]);
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    echoed[0].box.nickname[0] ^= 1;
    NativeOwnedTrackLocalEcho(gParties[B_TRAINER_PLAYER], echoed, sizeof(echoed));
    CHECK(!NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    echoed[0] = mon; echoed[1] = mon;
    NativeOwnedTrackLocalEcho(gParties[B_TRAINER_PLAYER], echoed, sizeof(echoed));
    CHECK(!NativeOwnedTrackerActive(456));
    ZeroMonData(&gParties[B_TRAINER_PLAYER][1]);
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    for (u32 which = 0; which < 7; ++which)
    {
        if (which == 0) CopyMon(&gParties[B_TRAINER_PLAYER][0], &mon, sizeof(mon));
        else if (which == 1) CopyMon(&gParties[B_TRAINER_PLAYER][4], &mon, sizeof(mon));
        else if (which == 2) CopyMon(&gPokemonStoragePtr->boxes[2][5], &mon.box, sizeof(mon.box));
        else if (which == 3) CopyMon(&gPokemonStoragePtr->boxes[3][5], &mon.box, sizeof(mon.box));
        else if (which == 4) CopyMon(&gSaveBlock1Ptr->daycare.mons[0].mon, &mon.box, sizeof(mon.box));
        else if (which == 5) CopyMon(&gPokemonStoragePtr->fusions[0], &mon, sizeof(mon));
        else CopyMon(&gParties[B_TRAINER_PLAYER][0].box.nickname[0], &mon.box.nickname[0], 1);
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; ZeroMonData(&gParties[B_TRAINER_PLAYER][4]);
        gPokemonStoragePtr->boxes[2][5] = mon.box; ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][5]);
        gSaveBlock1Ptr->daycare.mons[0].mon = mon.box; gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(CopyMonToPC(&mon) == MON_GIVEN_TO_PC && !NativeOwnedTrackerActive(456));
    ZeroBoxMonData(GetBoxedMonPtr(gSpecialVar_MonBoxId, gSpecialVar_MonBoxPos));
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    CHECK(!NativeSaveBegin()); CopyMon(&gParties[B_TRAINER_PLAYER][0], &mon, sizeof(mon));
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(785, TRUE) && !NativeSaveTransactionActive());
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); CopyMon(&gSaveBlock1Ptr->playerParty[0], &mon, sizeof(mon));
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    for (u32 projection = 0; projection < 2; ++projection)
    {
        SavePlayerParty();
        if (!projection) NativeOwnedTrackSkyBegin(1);
        else { const u8 order[] = {1}; NativeOwnedTrackUnionBegin(order, 1); }
        CHECK(NativeOwnedTrackerActive(456));
        CopyMon(&gSaveBlock1Ptr->playerParty[0], &mon, sizeof(mon)); CHECK(!NativeOwnedTrackerActive(456));
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!NativeOwnedTrackerStop(456)); NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned copy audit: unreported identical, partial and empty-slot copies fail closed");
}
static void CheckOwnedIdentityWriteAudit(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon mon, temporary, formMon;
    CreateMonWithIVs(&mon, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    CreateMonWithIVs(&formMon, SPECIES_CASTFORM_SUNNY, 20, 9999, OTID_STRUCT_PRESET(8888), 31);
    temporary = mon; gParties[B_TRAINER_PLAYER][0] = mon; gPartiesCount[B_TRAINER_PLAYER] = 1;
    gPokemonStoragePtr->boxes[2][5] = mon.box; gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
    gPokemonStoragePtr->boxes[2][6] = formMon.box;
    gPokemonStoragePtr->fusions[0] = mon;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    CHECK(TryBoxMonFormChange(&gPokemonStoragePtr->boxes[2][6], FORM_CHANGE_END_BATTLE)
        && GetBoxMonData(&gPokemonStoragePtr->boxes[2][6], MON_DATA_SPECIES) == SPECIES_CASTFORM_NORMAL
        && NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(784, TRUE));
    /* A corrupt checksum makes ordinary getters and setters mark the source
       as a bad egg. Restoring its bytes immediately must not hide the write. */
    struct BoxPokemon untracked = mon.box; untracked.checksum ^= 1;
    CHECK(GetBoxMonData(&untracked, MON_DATA_SPECIES) == SPECIES_EGG && NativeOwnedTrackerActive(456));
    untracked = mon.box; untracked.checksum ^= 1;
    u8 testFriendship = 1; SetBoxMonData(&untracked, MON_DATA_FRIENDSHIP, &testFriendship);
    CHECK(untracked.isBadEgg && NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_OPPONENT_A][0] = mon; gParties[B_TRAINER_OPPONENT_A][0].box.checksum ^= 1;
    CHECK(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) == SPECIES_EGG
        && NativeOwnedTrackerActive(456));
    for (u32 setter = 0; setter < 2; ++setter)
    for (u32 location = 0; location < 4; ++location)
    {
        struct BoxPokemon *target = location == 0 ? &gParties[B_TRAINER_PLAYER][0].box
            : location == 1 ? &gPokemonStoragePtr->boxes[2][5]
            : location == 2 ? &gSaveBlock1Ptr->daycare.mons[0].mon : &gPokemonStoragePtr->fusions[0].box;
        target->checksum ^= 1;
        if (setter) SetBoxMonData(target, MON_DATA_FRIENDSHIP, &testFriendship);
        else CHECK(GetBoxMonData(target, MON_DATA_SPECIES) == SPECIES_EGG);
        CHECK(!NativeOwnedTrackerActive(456));
        *target = mon.box;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!NativeSaveBegin()); gParties[B_TRAINER_PLAYER][0].box.checksum ^= 1;
    SetBoxMonData(&gParties[B_TRAINER_PLAYER][0].box, MON_DATA_FRIENDSHIP, &testFriendship);
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(785, TRUE));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    u32 personality = mon.box.personality, ot = mon.box.otId;
    u8 occupied = 1;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_PERSONALITY, &personality);
    SetBoxMonData(&gPokemonStoragePtr->boxes[2][5], MON_DATA_OT_ID, &ot);
    SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_SANITY_HAS_SPECIES, &occupied);
    CHECK(UpdateBoxMonOtId(&gPokemonStoragePtr->fusions[0].box, ot));
    u8 changedName[PLAYER_NAME_LENGTH]; memcpy(changedName, mon.box.otName, sizeof(changedName));
    changedName[0] ^= 1;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_OT_NAME, mon.box.otName);
    SetBoxMonData(&temporary.box, MON_DATA_OT_NAME, changedName);
    CHECK(NativeOwnedTrackerActive(456));
    for (u32 which = 0; which < 4; ++which)
    {
        if (which == 0) SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_OT_NAME, changedName);
        else if (which == 1) SetBoxMonData(&gPokemonStoragePtr->boxes[2][5], MON_DATA_OT_NAME, changedName);
        else if (which == 2) SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_OT_NAME, changedName);
        else SetMonData(&gPokemonStoragePtr->fusions[0], MON_DATA_OT_NAME, changedName);
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; gPokemonStoragePtr->boxes[2][5] = mon.box;
        gSaveBlock1Ptr->daycare.mons[0].mon = mon.box; gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!NativeSaveBegin()); SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_OT_NAME, changedName);
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(785, TRUE));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    gPokemonStoragePtr->boxes[2][5].otName[0] ^= 1;
    CHECK(NativeOwnedTrackerAudit() && !NativeOwnedTrackerActive(456));
    gPokemonStoragePtr->boxes[2][5] = mon.box;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    u32 other = ot ^ 0x87654321;
    CHECK(UpdateBoxMonOtId(&temporary.box, other));
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_PERSONALITY, &other);
    u16 sameSpecies = SPECIES_TORCHIC, otherSpecies = SPECIES_PIDGEY;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES, &sameSpecies);
    SetBoxMonData(&temporary.box, MON_DATA_SPECIES, &otherSpecies);
    CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    for (u32 which = 0; which < 4; ++which)
    {
        if (which == 0) SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES, &otherSpecies);
        else if (which == 1) SetBoxMonData(&gPokemonStoragePtr->boxes[2][5], MON_DATA_SPECIES, &otherSpecies);
        else if (which == 2) SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_SPECIES, &otherSpecies);
        else SetMonData(&gPokemonStoragePtr->fusions[0], MON_DATA_SPECIES, &otherSpecies);
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; gPokemonStoragePtr->boxes[2][5] = mon.box;
        gSaveBlock1Ptr->daycare.mons[0].mon = mon.box; gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!NativeSaveBegin()); SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES, &otherSpecies);
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(785, TRUE) && !NativeSaveTransactionActive());
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    for (u32 which = 0; which < 8; ++which)
    {
        u32 changed = personality ^ 0x12345678;
        u8 clear = 0; u16 empty = SPECIES_NONE;
        if (which == 0) SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_PERSONALITY, &changed);
        else if (which == 1) SetBoxMonData(&gPokemonStoragePtr->boxes[2][5], MON_DATA_OT_ID, &other);
        else if (which == 2) CHECK(UpdateBoxMonOtId(&gSaveBlock1Ptr->daycare.mons[0].mon, other));
        else if (which == 3) UpdateMonPersonality(&gPokemonStoragePtr->fusions[0].box, changed);
        else if (which == 4) SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES, &empty);
        else if (which == 5) SetBoxMonData(&gPokemonStoragePtr->boxes[2][5], MON_DATA_SANITY_HAS_SPECIES, &clear);
        else if (which == 6) SetBoxMonData(&gPokemonStoragePtr->boxes[3][5], MON_DATA_OT_ID, &other);
        else SetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_PERSONALITY, &changed);
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; gPokemonStoragePtr->boxes[2][5] = mon.box;
        ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][5]);
        gSaveBlock1Ptr->daycare.mons[0].mon = mon.box; gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!NativeSaveBegin()); SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_OT_ID, &other);
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(785, TRUE) && !NativeSaveTransactionActive());
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    for (u32 projection = 0; projection < 2; ++projection)
    {
        SavePlayerParty();
        if (!projection) NativeOwnedTrackSkyBegin(1);
        else { const u8 order[] = {1}; NativeOwnedTrackUnionBegin(order, 1); }
        CHECK(NativeOwnedTrackerActive(456));
        SetMonData(&gSaveBlock1Ptr->playerParty[0], MON_DATA_OT_ID, &other);
        CHECK(!NativeOwnedTrackerActive(456));
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    /* A repeated hatch script on a non-egg cannot authorize a foreign OT. */
    CHECK(!NativeOwnedTrackHatchBefore(0) && NativeOwnedTrackerActive(456));
    u32 foreignOt = READ_OTID_FROM_SAVE ^ 0x55aa55aa;
    CHECK(UpdateBoxMonOtId(&gParties[B_TRAINER_PLAYER][0].box, foreignOt)
        && !NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* Even inside a hatch, only the player's OT rekey is authorized. */
    u8 egg = TRUE, hatched = FALSE;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &egg);
    CHECK(NativeOwnedTrackHatchBefore(0));
    CHECK(UpdateBoxMonOtId(&gParties[B_TRAINER_PLAYER][0].box, foreignOt)
        && !NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* A direct personality write must not be accepted as a hatch rekey. */
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &egg);
    CHECK(NativeOwnedTrackHatchBefore(0));
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &hatched);
    gParties[B_TRAINER_PLAYER][0].box.personality ^= 1;
    NativeOwnedTrackHatchAfter(0); CHECK(!NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* Merely writing the expected OT without the checked rekey is not enough. */
    CHECK(mon.box.otId != READ_OTID_FROM_SAVE);
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &egg);
    CHECK(NativeOwnedTrackHatchBefore(0));
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &hatched);
    gParties[B_TRAINER_PLAYER][0].box.otId = READ_OTID_FROM_SAVE;
    NativeOwnedTrackHatchAfter(0); CHECK(!NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* Hatching may change only the trainer name, even when the OT ID stays the same. */
    u8 savedName[PLAYER_NAME_LENGTH], savedId[sizeof(gSaveBlock2Ptr->playerTrainerId)];
    memcpy(savedName, gSaveBlock2Ptr->playerName, sizeof(savedName));
    memcpy(savedId, gSaveBlock2Ptr->playerTrainerId, sizeof(savedId));
    memcpy(gSaveBlock2Ptr->playerName, changedName, sizeof(changedName));
    memcpy(gSaveBlock2Ptr->playerTrainerId, &ot, sizeof(savedId));
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &egg);
    CHECK(NativeOwnedTrackHatchBefore(0));
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &hatched);
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_OT_NAME, changedName);
    NativeOwnedTrackHatchAfter(0); CHECK(NativeOwnedTrackerActive(456) && !NativeOwnedTrackerAudit());
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* A raw write to the expected name cannot impersonate the checked hatch update. */
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &egg);
    CHECK(NativeOwnedTrackHatchBefore(0));
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &hatched);
    memcpy(gParties[B_TRAINER_PLAYER][0].box.otName, changedName, sizeof(changedName));
    NativeOwnedTrackHatchAfter(0); CHECK(!NativeOwnedTrackerActive(456));
    memcpy(gSaveBlock2Ptr->playerName, savedName, sizeof(savedName));
    memcpy(gSaveBlock2Ptr->playerTrainerId, savedId, sizeof(savedId));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    /* Transition receipts omit trainer names, so verify both physical ends
     * against the witness while the original slot binding still exists. */
    for (u32 which = 0; which < 14; ++which)
    {
        struct RhhDaycareTransfer daycare;
        struct RhhFusionTransfer fusion;
        struct RhhOwnedEvolution evolution;
        struct RhhOwnedGift gift;
        struct RhhPartyReorder reorder;
        struct RhhCatchPlacement catchPlacement;
        temporary = mon;
        if (which == 0)
        {
            gSaveBlock1Ptr->daycare.mons[0].mon.otName[0] ^= 1;
            gSaveBlock1Ptr->daycare.mons[0].mon.secure.raw[0] ^= 1;
            CHECK(ModsDaycareTransferCapture(&gSaveBlock1Ptr->daycare.mons[0].mon, RHH_DAYCARE_WITHDRAW,
                RHH_MON_DAYCARE, 0, 0, RHH_MON_PARTY, 0, 1, &daycare));
            CHECK(!(daycare.flags & RHH_DAYCARE_TRANSFER_BEFORE_VALID));
        }
        else if (which == 1)
        {
            CHECK(ModsDaycareTransferCapture(&gSaveBlock1Ptr->daycare.mons[0].mon, RHH_DAYCARE_WITHDRAW,
                RHH_MON_DAYCARE, 0, 0, RHH_MON_PARTY, 0, 1, &daycare));
            temporary.box.otName[0] ^= 1; ModsDaycareTransferCommitted(&temporary.box, &daycare);
        }
        else if (which == 2 || which == 3)
        {
            if (which == 2) gPokemonStoragePtr->fusions[0].box.otName[0] ^= 1;
            else gParties[B_TRAINER_PLAYER][0].box.otName[0] ^= 1;
            CHECK(ModsFusionTransferCapture(&gPokemonStoragePtr->fusions[0].box, RHH_FUSION_RESTORE,
                RHH_MON_FUSION_STORAGE, 0, 0, &fusion));
        }
        else if (which == 4 || which == 5)
        {
            CHECK(ModsFusionTransferCapture(&gPokemonStoragePtr->fusions[0].box, RHH_FUSION_RESTORE,
                RHH_MON_FUSION_STORAGE, 0, 0, &fusion));
            if (which == 4) temporary.box.otName[0] ^= 1;
            else gParties[B_TRAINER_PLAYER][0].box.otName[0] ^= 1;
            ModsFusionTransferCommitted(&temporary.box, RHH_MON_PARTY, 0, 1, &fusion);
        }
        else if (which == 6 || which == 7)
        {
            if (which == 6) gParties[B_TRAINER_PLAYER][0].box.otName[0] ^= 1;
            CHECK(ModsOwnedEvolutionCapture(&gParties[B_TRAINER_PLAYER][0], 0, FALSE, &evolution));
            if (which == 7)
            {
                gParties[B_TRAINER_PLAYER][0].box.otName[0] ^= 1;
                ModsOwnedEvolutionCommitted(&gParties[B_TRAINER_PLAYER][0], &evolution);
            }
        }
        else if (which == 8)
        {
            CHECK(ModsOwnedGiftCapture(&temporary.box, RHH_GIFT_SCRIPTED, &gift));
            gParties[B_TRAINER_PLAYER][0].box.otName[0] ^= 1;
            ModsOwnedGiftDestination(&gParties[B_TRAINER_PLAYER][0].box, RHH_MON_PARTY, 0, 0, &gift);
        }
        else if (which == 9 || which == 10)
        {
            CHECK(ModsPartyReorderBegin(RHH_PARTY_REORDER_SWAP, &reorder));
            if (which == 9) gParties[B_TRAINER_PLAYER][0].box.otName[0] ^= 1;
            ModsPartyReorderAdd(&reorder, 0, 1);
            if (which == 10)
            {
                gParties[B_TRAINER_PLAYER][1] = mon;
                gParties[B_TRAINER_PLAYER][1].box.otName[0] ^= 1;
                ModsPartyReorderCommitted(&reorder);
            }
        }
        else if (which == 11)
        {
            gMain.inBattle = TRUE; gBattleTypeFlags = 0;
            CHECK(ModsCatchPlacementCapture(&gParties[B_TRAINER_PLAYER][0].box,
                RHH_CATCH_DISPLACED_PARTY, 0, &catchPlacement));
            temporary.box.otName[0] ^= 1;
            ModsCatchPlacementCommitted(&temporary.box, RHH_MON_BOX, 3, 0, &catchPlacement);
            gMain.inBattle = FALSE;
        }
        else
        {
            if (which == 12) gParties[B_TRAINER_PLAYER][0].box.otName[0] ^= 1;
            CHECK(ModsOwnedEvolutionSplitCapture(&gParties[B_TRAINER_PLAYER][0], 0, 1, &evolution));
            if (which == 13)
            {
                gParties[B_TRAINER_PLAYER][0].box.otName[0] ^= 1;
                ModsOwnedEvolutionCommitted(&temporary, &evolution);
            }
        }
        CHECK(!NativeOwnedTrackerActive(456));
        gParties[B_TRAINER_PLAYER][0] = mon; memset(&gParties[B_TRAINER_PLAYER][1], 0, sizeof(mon));
        gSaveBlock1Ptr->daycare.mons[0].mon = mon.box; gPokemonStoragePtr->fusions[0] = mon;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!NativeOwnedTrackerStop(456)); NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned identity write audit: transient identity and occupancy changes fail closed");
}
static void CheckOwnedSkyLedger(const char *path, const char *module)
{
    if (module)
    {
        InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); RhhModsSetRunContext(123, 456, 0);
    }
    else InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon ground, flying;
    CreateMonWithIVs(&ground, SPECIES_TORCHIC, 20, 11111, OTID_STRUCT_PRESET(54321), 31);
    CreateMonWithIVs(&flying, SPECIES_PIDGEY, 20, 22222, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = ground; gParties[B_TRAINER_PLAYER][1] = flying;
    gParties[B_TRAINER_PLAYER][2] = ground; gParties[B_TRAINER_PLAYER][3] = flying;
    gPartiesCount[B_TRAINER_PLAYER] = 4;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    struct NativeOwnedId ids[4];
    for (u32 i = 0; i < 4; ++i) ids[i] = GetLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i});
    CHECK(DifferentLedgerId(ids[1], ids[3]));
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 20));
    CHECK(mons && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_20_SIZE);
    PreparePartyForSkyBattle();
    CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_SKY_BATTLE);
    if (module)
    {
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        RhhModsDispatch(&event, &decision);
    }
    u32 field = 99; CHECK(!mons->sky_party_field_slot(0, &field) && field == 1);
    CHECK(!mons->sky_party_field_slot(1, &field) && field == 3);
    field = 99; CHECK(mons->sky_party_field_slot(2, &field) && field == 99);
    struct RhhOwnedMonSnapshot unavailable = {.struct_size = sizeof(unavailable), .identity = 99}, old = unavailable;
    CHECK(mons->party_at(0, &unavailable) && !memcmp(&unavailable, &old, sizeof(old)));
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query), .matches = 99}, oldQuery = query;
    CHECK(mons->identity_query(0, &query) && !memcmp(&query, &oldQuery, sizeof(query)));
    CHECK(NativeSaveBegin() && NativeOwnedTrackerActive(456));
    struct BattleStruct battle = {0}; struct BattleStruct *previousBattle = gBattleStruct;
    bool8 previousInBattle = gMain.inBattle; u32 previousFlags = gBattleTypeFlags;
    struct PartyMenu previousMenu = gPartyMenu; u8 previousOrder[sizeof(gBattlePartyCurrentOrder)];
    memcpy(previousOrder, gBattlePartyCurrentOrder, sizeof(previousOrder));
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattleTypeFlags = 0; gPartyMenu.layout = PARTY_LAYOUT_SINGLE;
    gBattlePartyCurrentOrder[0] = 0x10; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE));
    u32 first, second; CHECK(!mons->sky_party_field_slot(0, &first) && !mons->sky_party_field_slot(1, &second));
    CHECK((first == 1 && second == 3) || (first == 3 && second == 1));
    field = 99; CHECK(!mons->battle_view_field_slot(0, &field) && field == first);
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1));
    CHECK(!mons->sky_party_field_slot(0, &field) && field == second);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gMain.inBattle = previousInBattle; gBattleTypeFlags = previousFlags; gBattleStruct = previousBattle;
    gPartyMenu = previousMenu; memcpy(gBattlePartyCurrentOrder, previousOrder, sizeof(previousOrder));
    gParties[B_TRAINER_PLAYER][0].hp = 1; gParties[B_TRAINER_PLAYER][1].hp = 2;
    ModsOwnedSkyTestRestore((1u << 1) | (1u << 3));
    CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_FIELD_ORDER
        && gParties[B_TRAINER_PLAYER][1].hp == 1 && gParties[B_TRAINER_PLAYER][3].hp == 2);
    for (u32 i = 0; i < 4; ++i) CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, ids[i]);
    CHECK(!NativeOwnedTrackerStop(456));
    PreparePartyForSkyBattle(); CHECK(mons->party_view() == RHH_OWNED_PARTY_SKY_BATTLE);
    CHECK(!mons->sky_party_field_slot(0, &field) && field == 1);
    ModsOwnedSkyTestRestore((1u << 1) | (1u << 3));
    CHECK(!NativeOwnedTrackerStart(456) && !NativeOwnedTrackerStop(456));
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(792, TRUE));
    CHECK(!NativeSaveRestoreMods(792) && !NativeOwnedTrackerStart(456));
    PreparePartyForSkyBattle(); gParties[B_TRAINER_PLAYER][0] = ground;
    ModsOwnedSkyTestRestore((1u << 1) | (1u << 3)); CHECK(!NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = ground; gParties[B_TRAINER_PLAYER][1] = flying;
    gParties[B_TRAINER_PLAYER][2] = ground; gParties[B_TRAINER_PLAYER][3] = flying;
    gPartiesCount[B_TRAINER_PLAYER] = 4;
    CHECK(!NativeSaveRestoreMods(792) && !NativeOwnedTrackerStart(456));
    PreparePartyForSkyBattle(); *GetSavedPlayerPartyMon(3) = ground;
    ModsOwnedSkyTestRestore((1u << 1) | (1u << 3)); CHECK(!NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = ground; gParties[B_TRAINER_PLAYER][1] = flying;
    gParties[B_TRAINER_PLAYER][2] = ground; gParties[B_TRAINER_PLAYER][3] = flying;
    gPartiesCount[B_TRAINER_PLAYER] = 4;
    CHECK(!NativeSaveRestoreMods(792) && !NativeOwnedTrackerStart(456));
    PreparePartyForSkyBattle();
    ModsOwnedSkyPartyBeforeRestore((1u << 1) | (1u << 3)); CHECK(NativeOwnedTrackerActive(456));
    LoadPlayerParty(); CHECK(NativeOwnedTrackerActive(456));
    LoadPlayerParty(); CHECK(!NativeOwnedTrackerActive(456));
    ModsOwnedSkyPartyAfterRestore();
    gParties[B_TRAINER_PLAYER][0] = ground; gParties[B_TRAINER_PLAYER][1] = flying;
    gParties[B_TRAINER_PLAYER][2] = ground; gParties[B_TRAINER_PLAYER][3] = flying;
    gPartiesCount[B_TRAINER_PLAYER] = 4;
    CHECK(!NativeSaveRestoreMods(792) && !NativeOwnedTrackerStart(456));
    PreparePartyForSkyBattle();
    ModsOwnedSkyPartyBeforeRestore((1u << 1) | (1u << 3)); CHECK(NativeOwnedTrackerActive(456));
    /* Both selected Pidgey have identical identity fields; only the first
       reduced slot may be copied to field slot 1. */
    SavePlayerPartyMon(1, &gParties[B_TRAINER_PLAYER][1]);
    CHECK(!NativeOwnedTrackerActive(456));
    ModsOwnedSkyTestRestore((1u << 1) | (1u << 3));
    /* Matching OT/personality does not prove a saved copy is the original.
     * Catch both selected and unselected backup changes before restoration. */
    for (u32 slot = 0; slot < 2; ++slot)
    {
        CHECK(!NativeSaveRestoreMods(792) && !NativeOwnedTrackerStart(456));
        PreparePartyForSkyBattle(); CHECK(NativeOwnedTrackerActive(456));
        GetSavedPlayerPartyMon(slot)->hp ^= 1;
        ModsOwnedSkyPartyBeforeRestore((1u << 1) | (1u << 3));
        CHECK(!NativeOwnedTrackerActive(456));
        GetSavedPlayerPartyMon(slot)->hp ^= 1;
        ModsOwnedSkyTestRestore((1u << 1) | (1u << 3));
    }
    CHECK(!NativeSaveRestoreMods(792) && !NativeOwnedTrackerStart(456));
    struct Pokemon fieldBackup[PARTY_SIZE]; memcpy(fieldBackup, gParties[B_TRAINER_PLAYER], sizeof(fieldBackup));
    ModsOwnedSkyPartyBegin((1u << 1) | (1u << 3)); SavePlayerParty();
    GetSavedPlayerPartyMon(0)->hp ^= 1;
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(fieldBackup));
    gParties[B_TRAINER_PLAYER][0] = fieldBackup[1]; gParties[B_TRAINER_PLAYER][1] = fieldBackup[3];
    gPartiesCount[B_TRAINER_PLAYER] = 2;
    ModsOwnedSkyPartyPrepared(); CHECK(!NativeOwnedTrackerActive(456));
    GetSavedPlayerPartyMon(0)->hp ^= 1;
    ModsOwnedSkyTestRestore((1u << 1) | (1u << 3));
    for (u32 mode = 0; mode < 2; ++mode)
    {
        CHECK(!NativeSaveRestoreMods(792) && !NativeOwnedTrackerStart(456));
        memcpy(fieldBackup, gParties[B_TRAINER_PLAYER], sizeof(fieldBackup));
        PreparePartyForSkyBattle(); ModsOwnedSkyPartyBeforeRestore((1u << 1) | (1u << 3));
        CHECK(NativeOwnedTrackerActive(456));
        GetSavedPlayerPartyMon(0)->hp ^= 1;
        if (!mode) { SavePlayerPartyMon(1, &gParties[B_TRAINER_PLAYER][0]); CHECK(!NativeOwnedTrackerActive(456)); }
        else { LoadPlayerParty(); CHECK(!NativeOwnedTrackerActive(456)); }
        GetSavedPlayerPartyMon(0)->hp ^= 1;
        if (!mode) ModsOwnedSkyTestRestore((1u << 1) | (1u << 3));
        else {
            memcpy(gParties[B_TRAINER_PLAYER], fieldBackup, sizeof(fieldBackup)); gPartiesCount[B_TRAINER_PLAYER] = 4;
            ModsOwnedSkyPartyAfterRestore();
        }
    }
    CHECK(!NativeSaveRestoreMods(792) && !NativeOwnedTrackerStart(456));
    memcpy(fieldBackup, gParties[B_TRAINER_PLAYER], sizeof(fieldBackup));
    ModsOwnedSkyPartyBegin((1u << 1) | (1u << 3)); SavePlayerParty();
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(fieldBackup));
    gParties[B_TRAINER_PLAYER][0] = fieldBackup[1]; gParties[B_TRAINER_PLAYER][1] = fieldBackup[3];
    gParties[B_TRAINER_PLAYER][0].hp ^= 1; /* Same identity fields as its selected source. */
    gPartiesCount[B_TRAINER_PLAYER] = 2;
    ModsOwnedSkyPartyPrepared(); CHECK(!NativeOwnedTrackerActive(456));
    ModsOwnedSkyTestRestore((1u << 1) | (1u << 3));
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned sky ledger: projection, battle menu and restoration passed");
}
static void CheckOwnedFollowerLedger(const char *path, const char *module)
{
    if (module)
    {
        InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); RhhModsSetRunContext(123, 456, 0);
    }
    else InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gParties[B_TRAINER_PARTNER], 0, sizeof(gParties[B_TRAINER_PARTNER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_TORCHIC, 20, 11111, OTID_STRUCT_PRESET(54321), 31);
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = mon;
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE;
    gParties[B_TRAINER_PLAYER][1].hp = gParties[B_TRAINER_PLAYER][5].hp = 0;
    bool8 egg = TRUE; SetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_IS_EGG, &egg);
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    struct NativeOwnedId ids[PARTY_SIZE];
    for (u32 i = 0; i < PARTY_SIZE; ++i) ids[i] = GetLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i});
    CHECK(DifferentLedgerId(ids[0], ids[2]) && DifferentLedgerId(ids[2], ids[4]));
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 22));
    CHECK(mons && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_22_SIZE);
    if (module) RhhModsObserveOwnedMons();
    ModsOwnedFollowerTestPrepare();
    CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_FOLLOWER_BATTLE);
    if (module)
    {
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        RhhModsDispatch(&event, &decision);
        RhhModsObserveOwnedMons();
    }
    u32 field = 99;
    CHECK(!mons->follower_party_field_slot(0, &field) && field == 0);
    CHECK(!mons->follower_party_field_slot(1, &field) && field == 2);
    CHECK(!mons->follower_party_field_slot(2, &field) && field == 4);
    field = 99; CHECK(mons->follower_party_field_slot(3, &field) && field == 99);
    CHECK(mons->sky_party_field_slot(0, &field) && field == 99);
    struct RhhOwnedMonSnapshot unavailable = {.struct_size = sizeof(unavailable), .identity = 99}, old = unavailable;
    CHECK(mons->party_at(0, &unavailable) && !memcmp(&unavailable, &old, sizeof(old)));
    CHECK(NativeSaveBegin() && NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0].hp = 1; gParties[B_TRAINER_PLAYER][1].hp = 2; gParties[B_TRAINER_PLAYER][2].hp = 3;
    ModsOwnedFollowerTestRestore();
    CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_FIELD_ORDER
        && gParties[B_TRAINER_PLAYER][0].hp == 1 && gParties[B_TRAINER_PLAYER][2].hp == 2
        && gParties[B_TRAINER_PLAYER][4].hp == 3 && gParties[B_TRAINER_PLAYER][1].hp == 0);
    for (u32 i = 0; i < PARTY_SIZE; ++i) CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, ids[i]);
    CHECK(!NativeOwnedTrackerStop(456));
    ModsOwnedFollowerTestPrepare(); CHECK(mons->party_view() == RHH_OWNED_PARTY_FOLLOWER_BATTLE);
    CHECK(!mons->follower_party_field_slot(1, &field) && field == 2);
    ModsOwnedFollowerTestRestore(); CHECK(!NativeOwnedTrackerStart(456) && !NativeOwnedTrackerStop(456));
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(793, TRUE));
    CHECK(!NativeSaveRestoreMods(793) && !NativeOwnedTrackerStart(456));
    struct Pokemon fieldParty[PARTY_SIZE]; memcpy(fieldParty, gParties[B_TRAINER_PLAYER], sizeof(fieldParty));
    ModsOwnedFollowerTestPrepare();
    struct Pokemon partner = mon; /* Same OT/personality as the player's Pokémon. */
    for (u32 i = 0; i < MULTI_PARTY_SIZE; ++i) gParties[B_TRAINER_PARTNER][i] = partner;
    struct BattleStruct battle = {0}; struct BattleStruct *previousBattle = gBattleStruct;
    bool8 previousInBattle = gMain.inBattle; u32 previousFlags = gBattleTypeFlags;
    struct PartyMenu previousMenu = gPartyMenu; u8 previousOrder[sizeof(gBattlePartyCurrentOrder)];
    memcpy(previousOrder, gBattlePartyCurrentOrder, sizeof(previousOrder));
    gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    gPartyMenu.layout = PARTY_LAYOUT_MULTI;
    gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(!mons->follower_party_field_slot(0, &field) && field == 0);
    field = 99; CHECK(mons->follower_party_field_slot(1, &field) && field == 99);
    CHECK(!mons->follower_party_field_slot(2, &field) && field == 2);
    CHECK(!mons->follower_party_field_slot(3, &field) && field == 4);
    field = 99; CHECK(mons->follower_party_field_slot(4, &field) && field == 99);
    CHECK(mons->follower_party_field_slot(5, &field) && field == 99);
    CHECK(!mons->battle_view_field_slot(2, &field) && field == 2);
    struct Pokemon distinctPartner;
    CreateMonWithIVs(&distinctPartner, SPECIES_PIDGEY, 20, 22222, OTID_STRUCT_PRESET(98765), 31);
    for (u32 i = 0; i < MULTI_PARTY_SIZE; ++i) gParties[B_TRAINER_PARTNER][i] = distinctPartner;
    CHECK(ModsOwnedPartyBattleTestSummary(TRUE));
    CHECK(!mons->follower_party_field_slot(0, &field) && field == 0);
    CHECK(!mons->follower_party_field_slot(2, &field) && field == 2);
    CHECK(!mons->follower_party_field_slot(3, &field) && field == 4);
    field = 99; CHECK(mons->follower_party_field_slot(1, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSummary(FALSE) && NativeOwnedTrackerActive(456));
    for (u32 i = 0; i < MULTI_PARTY_SIZE; ++i) gParties[B_TRAINER_PARTNER][i] = partner;
    gPartyMenu.layout = PARTY_LAYOUT_SINGLE;
    field = 99; CHECK(mons->follower_party_field_slot(0, &field) && field == 99);
    gPartyMenu.layout = PARTY_LAYOUT_MULTI;
    CHECK(NativeOwnedTrackerActive(456));
    if (module)
    {
        struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE, .args = {1, 0, 0, 0}};
        struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
        RhhModsDispatch(&event, &decision); RhhModsObserveOwnedMons();
    }
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 2) && NativeOwnedTrackerActive(456));
    CHECK(!mons->follower_party_field_slot(0, &field) && field == 2);
    CHECK(!mons->follower_party_field_slot(2, &field) && field == 0);
    CHECK(ModsOwnedPartyBattleTestSwitch(2, 3) && NativeOwnedTrackerActive(456));
    CHECK(!mons->follower_party_field_slot(2, &field) && field == 4);
    CHECK(ModsOwnedPartyBattleTestSwitch(1, 4) && NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->follower_party_field_slot(1, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gMain.inBattle = previousInBattle; gBattleTypeFlags = previousFlags; gBattleStruct = previousBattle;
    gPartyMenu = previousMenu; memcpy(gBattlePartyCurrentOrder, previousOrder, sizeof(previousOrder));
    ModsOwnedFollowerTestRestore();
    CHECK(NativeOwnedTrackerActive(456));
    for (u32 i = 0; i < PARTY_SIZE; ++i)
    {
        CHECK(gParties[B_TRAINER_PLAYER][i].hp == fieldParty[i].hp);
        CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, ids[i]);
    }
    /* The summary is read-only, but exposes a temporarily rearranged party.
       A change that keeps OT/personality must still revoke the slot view. */
    for (u32 check = 0; check < 18; ++check)
    {
        ModsOwnedFollowerTestPrepare();
        gBattleStruct = &battle; gMain.inBattle = TRUE;
        gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
        gPartyMenu.layout = PARTY_LAYOUT_MULTI;
        gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
        CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && ModsOwnedPartyBattleTestSummary(TRUE));
        CHECK(NativeOwnedTrackerAudit() == 1 && NativeOwnedTrackerActive(456));
        u32 summarySlot = check && check < 3 ? check + 1 : 0;
        if (check < 3) gParties[B_TRAINER_PLAYER][summarySlot].hp ^= 1;
        else if (check >= 6)
        {
            struct Pokemon savedSummary = gParties[B_TRAINER_PLAYER][summarySlot];
            if (check == 6) GetBoxMonData(&distinctPartner.box, MON_DATA_OT_NAME, (u8 *)&gParties[B_TRAINER_PLAYER][summarySlot].box);
            else if (check == 7) CopyMon(&gParties[B_TRAINER_PLAYER][summarySlot], &savedSummary, sizeof(savedSummary));
            else if (check == 8) ZeroBoxMonData(&gParties[B_TRAINER_PLAYER][summarySlot].box);
            else if (check == 9) GetMonNicknameVanilla(&distinctPartner, (u8 *)&gParties[B_TRAINER_PLAYER][summarySlot].box);
            else if (check == 10) GetBoxMonNickname(&distinctPartner.box, (u8 *)&gParties[B_TRAINER_PLAYER][summarySlot].box);
            else if (check == 11) CHECK(GetBoxMonStoredTeraType(&distinctPartner.box, &gParties[B_TRAINER_PLAYER][summarySlot].box.personality));
            else if (check == 12) ConvertPokemonToBattleTowerPokemon(&distinctPartner, (struct BattleTowerPokemon *)&gParties[B_TRAINER_PLAYER][summarySlot]);
            else if (check == 13) GetDaycareIVRules(&gSaveBlock1Ptr->daycare, (struct DaycareIVRules *)&gParties[B_TRAINER_PLAYER][summarySlot]);
            else if (check == 14) GetDaycareNatureSourceWeights(&gSaveBlock1Ptr->daycare, GEN_6, (u8 *)&gParties[B_TRAINER_PLAYER][summarySlot]);
            else if (check == 15) {
                u32 ability, percent;
                GetDaycareAbilityInheritance(&gSaveBlock1Ptr->daycare, GEN_6,
                    (s32 *)&gParties[B_TRAINER_PLAYER][summarySlot].box.personality, &ability, &percent);
            }
            else {
                struct BoxPokemon *source = &gParties[B_TRAINER_PLAYER][summarySlot].box;
                source->checksum ^= 1;
                if (check == 16) CHECK(GetBoxMonData(source, MON_DATA_SPECIES) == SPECIES_EGG);
                else { u8 friendship = 1; SetBoxMonData(source, MON_DATA_FRIENDSHIP, &friendship); }
            }
            CHECK(!NativeOwnedTrackerActive(456));
            gParties[B_TRAINER_PLAYER][summarySlot] = savedSummary;
        }
        else if (check == 4)
        {
            u8 friendship = GetMonData(&gParties[B_TRAINER_PLAYER][summarySlot], MON_DATA_FRIENDSHIP);
            SetBoxMonData(&gParties[B_TRAINER_PLAYER][summarySlot].box, MON_DATA_FRIENDSHIP, &friendship);
        }
        else
        {
            if (check == 5)
            {
                u16 partnerHp = gParties[B_TRAINER_PLAYER][1].hp;
                SetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_HP, &partnerHp);
                CHECK(NativeOwnedTrackerActive(456));
            }
            u16 hp = gParties[B_TRAINER_PLAYER][summarySlot].hp;
            u16 requestedHp = check == 3 ? hp ^ 1 : hp;
            SetMonData(&gParties[B_TRAINER_PLAYER][summarySlot], MON_DATA_HP, &requestedHp);
            if (check == 3)
            {
                CHECK(!NativeOwnedTrackerActive(456));
                SetMonData(&gParties[B_TRAINER_PLAYER][summarySlot], MON_DATA_HP, &hp);
            }
        }
        if (!check) CHECK(NativeOwnedTrackerAudit() == -1);
        else if (check == 1) { field = 99; CHECK(mons->follower_party_field_slot(2, &field) && field == 99); }
        else if (check == 2) CHECK(ModsOwnedPartyBattleTestSummary(FALSE));
        CHECK(!NativeOwnedTrackerActive(456));
        if (check != 2)
        {
            field = 99; CHECK(mons->follower_party_field_slot(2, &field) && field == 99);
            if (check < 3) gParties[B_TRAINER_PLAYER][summarySlot].hp ^= 1;
            CHECK(ModsOwnedPartyBattleTestSummary(FALSE));
        }
        CHECK(ModsOwnedPartyBattleTestOrder(FALSE));
        gMain.inBattle = previousInBattle; gBattleTypeFlags = previousFlags; gBattleStruct = previousBattle;
        gPartyMenu = previousMenu; memcpy(gBattlePartyCurrentOrder, previousOrder, sizeof(previousOrder));
        ModsOwnedFollowerTestRestore(); memcpy(gParties[B_TRAINER_PLAYER], fieldParty, sizeof(fieldParty));
        CHECK(!NativeSaveRestoreMods(793) && !NativeOwnedTrackerStart(456));
    }
    /* The public slot projection must also fail closed without the optional
     * identity tracker, even if the aliased write is immediately undone. */
    CHECK(!NativeOwnedTrackerStop(456));
    for (u32 writer = 0; writer < 4; ++writer)
    {
        ModsOwnedFollowerTestPrepare();
        gBattleStruct = &battle; gMain.inBattle = TRUE;
        gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
        gPartyMenu.layout = PARTY_LAYOUT_MULTI;
        gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
        CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && ModsOwnedPartyBattleTestSummary(TRUE));
        struct Pokemon beforeAliasedWrite = gParties[B_TRAINER_PLAYER][0];
        if (!writer) GetBoxMonData(&distinctPartner.box, MON_DATA_OT_NAME, (u8 *)&gParties[B_TRAINER_PLAYER][0].box);
        else if (writer == 1) {
            CpuSet(&gParties[B_TRAINER_PLAYER][0], &gParties[B_TRAINER_PLAYER][0], CPU_SET_32BIT | (sizeof(beforeAliasedWrite) / 4));
            CHECK(!mons->follower_party_field_slot(0, &field) && field == 0);
            CpuSet(&beforeAliasedWrite, &gParties[B_TRAINER_PLAYER][0], CPU_SET_32BIT | (sizeof(beforeAliasedWrite) / 4));
        }
        else if (writer == 2) {
            unsigned overridden;
            CHECK(!NativeNatureStorageRead(&distinctPartner.box, &gParties[B_TRAINER_PLAYER][0].box.personality, &overridden));
        }
        else {
            unsigned nature, overridden;
            CHECK(!NativeNatureStorageRead(&beforeAliasedWrite.box, &nature, &overridden)
                && !NativeNatureStorageWrite(&gParties[B_TRAINER_PLAYER][0].box, nature));
        }
        gParties[B_TRAINER_PLAYER][0] = beforeAliasedWrite;
        field = 99; CHECK(mons->follower_party_field_slot(0, &field) && field == 99);
        CHECK(ModsOwnedPartyBattleTestSummary(FALSE) && ModsOwnedPartyBattleTestOrder(FALSE));
        gMain.inBattle = previousInBattle; gBattleTypeFlags = previousFlags; gBattleStruct = previousBattle;
        gPartyMenu = previousMenu; memcpy(gBattlePartyCurrentOrder, previousOrder, sizeof(previousOrder));
        ModsOwnedFollowerTestRestore(); memcpy(gParties[B_TRAINER_PLAYER], fieldParty, sizeof(fieldParty));
    }
    for (u32 setter = 0; setter < 2; ++setter)
    {
        ModsOwnedFollowerTestPrepare();
        gBattleStruct = &battle; gMain.inBattle = TRUE;
        gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
        gPartyMenu.layout = PARTY_LAYOUT_MULTI;
        gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
        CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && ModsOwnedPartyBattleTestSummary(TRUE));
        struct Pokemon saved = gParties[B_TRAINER_PLAYER][0];
        struct BoxPokemon *source = &gParties[B_TRAINER_PLAYER][0].box; source->checksum ^= 1;
        if (setter) { u8 friendship = 1; SetBoxMonData(source, MON_DATA_FRIENDSHIP, &friendship); }
        else CHECK(GetBoxMonData(source, MON_DATA_SPECIES) == SPECIES_EGG);
        gParties[B_TRAINER_PLAYER][0] = saved;
        field = 99; CHECK(mons->follower_party_field_slot(0, &field) && field == 99);
        CHECK(ModsOwnedPartyBattleTestSummary(FALSE) && ModsOwnedPartyBattleTestOrder(FALSE));
        gMain.inBattle = previousInBattle; gBattleTypeFlags = previousFlags; gBattleStruct = previousBattle;
        gPartyMenu = previousMenu; memcpy(gBattlePartyCurrentOrder, previousOrder, sizeof(previousOrder));
        ModsOwnedFollowerTestRestore(); memcpy(gParties[B_TRAINER_PLAYER], fieldParty, sizeof(fieldParty));
    }
    CHECK(!NativeOwnedTrackerStart(456));
    ModsOwnedFollowerTestPrepare();
    gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    gPartyMenu.layout = PARTY_LAYOUT_MULTI;
    gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1) && !NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->follower_party_field_slot(0, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE));
    field = 99; CHECK(mons->follower_party_field_slot(0, &field) && field == 99);
    gMain.inBattle = previousInBattle; gBattleTypeFlags = previousFlags; gBattleStruct = previousBattle;
    gPartyMenu = previousMenu; memcpy(gBattlePartyCurrentOrder, previousOrder, sizeof(previousOrder));
    ModsOwnedFollowerTestRestore(); memcpy(gParties[B_TRAINER_PLAYER], fieldParty, sizeof(fieldParty));
    CHECK(!NativeSaveRestoreMods(793) && !NativeOwnedTrackerStart(456));
    ModsOwnedFollowerTestPrepare();
    gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    gPartyMenu.layout = PARTY_LAYOUT_MULTI;
    gBattlePartyCurrentOrder[0] = 0x01; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && !NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->follower_party_field_slot(0, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE));
    field = 99; CHECK(mons->follower_party_field_slot(0, &field) && field == 99);
    gMain.inBattle = previousInBattle; gBattleTypeFlags = previousFlags; gBattleStruct = previousBattle;
    gPartyMenu = previousMenu; memcpy(gBattlePartyCurrentOrder, previousOrder, sizeof(previousOrder));
    ModsOwnedFollowerTestRestore();
    memcpy(gParties[B_TRAINER_PLAYER], fieldParty, sizeof(fieldParty));
    CHECK(!NativeSaveRestoreMods(793) && !NativeOwnedTrackerStart(456));
    ModsOwnedFollowerTestPrepare();
    gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    gPartyMenu.layout = PARTY_LAYOUT_MULTI;
    gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    ModsOwnedBattlePartyViewReset(); CHECK(!NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->follower_party_field_slot(0, &field) && field == 99);
    gMain.inBattle = previousInBattle; gBattleTypeFlags = previousFlags; gBattleStruct = previousBattle;
    gPartyMenu = previousMenu; memcpy(gBattlePartyCurrentOrder, previousOrder, sizeof(previousOrder));
    ModsOwnedFollowerTestRestore(); memcpy(gParties[B_TRAINER_PLAYER], fieldParty, sizeof(fieldParty));
    CHECK(!NativeSaveRestoreMods(793) && !NativeOwnedTrackerStart(456));
    ModsOwnedFollowerTestPrepare(); gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 3;
    ModsOwnedFollowerTestRestore(); CHECK(!NativeOwnedTrackerActive(456));
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned follower ledger: selection, mapping and restoration passed");
}
/* The isolated enabled-configuration build runs the actual follower entry
 * points and predicate; the ordinary build exercises the same catch machinery
 * through the test override because followers are disabled by default. */
static void OwnedCatchFollowerPrepare(void)
{
#if FNPC_ENABLE_NPC_FOLLOWERS && FNPC_FLAG_PARTNER_WILD_BATTLES && B_MULTI_HALF_TEAMS
    u32 flags = gBattleTypeFlags;
    SetFollowerNPCData(FNPC_DATA_IN_PROGRESS, TRUE);
    SetFollowerNPCData(FNPC_DATA_BATTLE_PARTNER, PARTNER_STEVEN);
    CHECK(IsNPCFollowerWildBattle());
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    PrepareForFollowerNPCBattle();
    gBattleTypeFlags = flags;
#else
    ModsOwnedFollowerTestPrepare();
#endif
}
static void OwnedCatchFollowerRestore(void)
{
#if FNPC_ENABLE_NPC_FOLLOWERS && FNPC_FLAG_PARTNER_WILD_BATTLES && B_MULTI_HALF_TEAMS
    u32 flags = gBattleTypeFlags;
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    RestorePartyAfterFollowerNPCBattle();
    gBattleTypeFlags = flags;
#else
    ModsOwnedFollowerTestRestore();
#endif
}
static void OwnedCatchFollowerOverride(bool32 enabled)
{
#if FNPC_ENABLE_NPC_FOLLOWERS && FNPC_FLAG_PARTNER_WILD_BATTLES && B_MULTI_HALF_TEAMS
    if (enabled) CHECK(IsNPCFollowerWildBattle());
#else
    ModsOwnedCatchTestFollower(enabled);
#endif
}
static void CheckOwnedFollowerCatchLedger(const char *path, const char *module)
{
    if (module)
    {
        InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path)); RhhModsSetRunContext(123, 456, 0);
    }
    else InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_TORCHIC, 20, 11111, OTID_STRUCT_PRESET(54321), 31);
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = mon;
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE;
    gParties[B_TRAINER_PLAYER][1].hp = gParties[B_TRAINER_PLAYER][5].hp = 0;
    bool8 egg = TRUE; SetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_IS_EGG, &egg);
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    struct NativeOwnedLedgerSlot party2 = {NATIVE_OWNED_PARTY, 0, 2}, party5 = {NATIVE_OWNED_PARTY, 0, 5};
    struct NativeOwnedLedgerSlot box0 = {NATIVE_OWNED_BOX, 0, 0}, box1 = {NATIVE_OWNED_BOX, 0, 1};
    struct NativeOwnedLedgerSlot box2 = {NATIVE_OWNED_BOX, 0, 2};
    struct NativeOwnedId selected = GetLedgerSlot(party2), unselected = GetLedgerSlot(party5), empty = {0, 0};
    OwnedCatchFollowerPrepare();
    gParties[B_TRAINER_PLAYER][0].hp = 1; gParties[B_TRAINER_PLAYER][1].hp = 2; gParties[B_TRAINER_PLAYER][2].hp = 3;
    struct BattleStruct battle = {0}; struct BattleStruct *previousBattle = gBattleStruct;
    bool8 oldBattle = gMain.inBattle; u32 oldFlags = gBattleTypeFlags;
    gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    gBattlersCount = 2; gBattlerPositions[0] = 0; gBattlerPositions[1] = 1;
    gBattlerPartyIndexes[0] = gBattlerPartyIndexes[1] = 0; gAbsentBattlerFlags = 0; gBattleMons[0].hp = gBattleMons[1].hp = 1;
    OwnedCatchFollowerOverride(TRUE);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_POOCHYENA, 20, 33333, OTID_STRUCT_PRESET(777), 31);
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 23));
    CHECK(mons && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_23_SIZE);
    ModsOwnedCatchTestStep(FALSE, PARTY_SIZE + 1);
    CHECK(IsFollowerCatchFullParty() && NativeOwnedTrackerActive(456));
    CHECK(!FollowerCatchCanSend(0) && FollowerCatchCanSend(2) && FollowerCatchCanSend(5));
    u32 field = 99; CHECK(mons->follower_party_field_slot(0, &field) && field == 99);
    struct PartyMenu previousMenu = gPartyMenu; u8 previousOrder[sizeof(gBattlePartyCurrentOrder)];
    memcpy(previousOrder, gBattlePartyCurrentOrder, sizeof(previousOrder));
    gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedFollowerCatchTestMenu(TRUE) && NativeOwnedTrackerActive(456));
    for (u32 i = 0; i < PARTY_SIZE; ++i)
    { field = 99; CHECK(!mons->battle_view_field_slot(i, &field) && field == i); }
    CHECK(ModsOwnedFollowerCatchTestMenu(FALSE) && NativeOwnedTrackerActive(456));
    CHECK(gBattlePartyCurrentOrder[0] == 0x03 && gBattlePartyCurrentOrder[1] == 0x12 && gBattlePartyCurrentOrder[2] == 0x45);
    gPartyMenu = previousMenu; memcpy(gBattlePartyCurrentOrder, previousOrder, sizeof(previousOrder));
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    CHECK(!IsFollowerCatchFullParty() && NativeOwnedTrackerActive(456));
    struct NativeOwnedId boxedCaught = GetLedgerSlot(box0);
    CHECK(DifferentLedgerId(boxedCaught, unselected)); CheckLedgerSlot(party5, unselected);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_POOCHYENA, 20, 55555, OTID_STRUCT_PRESET(999), 31);
    battle.partyState[B_SIDE_PLAYER][5].changedSpecies = SPECIES_MUDKIP;
    ModsOwnedCatchTestStep(FALSE, 5);
    CHECK(IsFollowerCatchFullParty() && NativeOwnedTrackerActive(456));
    CheckLedgerSlot(party5, empty); CheckLedgerSlot(box1, unselected);
    CHECK(battle.partyState[B_SIDE_PLAYER][5].changedSpecies == SPECIES_MUDKIP);
    CHECK(gParties[B_TRAINER_PLAYER][0].hp == 1 && gParties[B_TRAINER_PLAYER][2].hp == 2
        && gParties[B_TRAINER_PLAYER][4].hp == 3);
    CHECK(NativeSaveBegin());
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    CHECK(!IsFollowerCatchFullParty() && NativeOwnedTrackerActive(456));
    struct NativeOwnedId caught = GetLedgerSlot(party5);
    CHECK(DifferentLedgerId(caught, unselected)); CheckLedgerSlot(box1, unselected); CheckLedgerSlot(party2, selected);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_ZIGZAGOON, 20, 44444, OTID_STRUCT_PRESET(888), 31);
    battle.partyState[B_SIDE_PLAYER][1].changedSpecies = SPECIES_COMBUSKEN;
    battle.partyState[B_SIDE_PLAYER][2].changedSpecies = SPECIES_MUDKIP;
    ModsOwnedCatchTestStep(FALSE, 2); CheckLedgerSlot(box2, selected); CheckLedgerSlot(party2, empty);
    CHECK(battle.partyState[B_SIDE_PLAYER][1].changedSpecies == SPECIES_NONE
        && battle.partyState[B_SIDE_PLAYER][2].changedSpecies == SPECIES_MUDKIP);
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    CHECK(!IsFollowerCatchFullParty() && NativeOwnedTrackerActive(456));
    struct NativeOwnedId newSelected = GetLedgerSlot(party2);
    CHECK(DifferentLedgerId(newSelected, selected)); CheckLedgerSlot(box2, selected); CheckLedgerSlot(party5, caught);
    OwnedCatchFollowerOverride(FALSE); gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = previousBattle;
    OwnedCatchFollowerRestore(); CHECK(NativeOwnedTrackerActive(456));
    CHECK(gParties[B_TRAINER_PLAYER][0].hp == 1 && gParties[B_TRAINER_PLAYER][4].hp == 3
        && GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON
        && GetMonData(&gParties[B_TRAINER_PLAYER][5], MON_DATA_SPECIES) == SPECIES_POOCHYENA);
    CheckLedgerSlot(party2, newSelected); CheckLedgerSlot(party5, caught); CheckLedgerSlot(box0, boxedCaught);
    CheckLedgerSlot(box1, unselected); CheckLedgerSlot(box2, selected);
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveBegin() && !NativeSaveEnd(794, TRUE));
    CHECK(!NativeSaveRestoreMods(794)); CheckLedgerSlot(party2, newSelected); CheckLedgerSlot(box2, selected);
    RhhModsSetRunContext(123, 457, 0); CHECK(!NativeOwnedLedgerStart(457));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    for (u32 i = 0; i < 4; ++i) gParties[B_TRAINER_PLAYER][i] = mon;
    gParties[B_TRAINER_PLAYER][1].hp = 0; gPartiesCount[B_TRAINER_PLAYER] = 4;
    CHECK(!NativeOwnedTrackerSeed(457) && !NativeOwnedTrackerStart(457));
    OwnedCatchFollowerPrepare(); battle = (struct BattleStruct){0}; gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    gBattlersCount = 2; gBattlerPositions[0] = 0; gBattlerPositions[1] = 1;
    gBattlerPartyIndexes[0] = gBattlerPartyIndexes[1] = 0; gAbsentBattlerFlags = 0; gBattleMons[0].hp = gBattleMons[1].hp = 1;
    OwnedCatchFollowerOverride(TRUE);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_ZIGZAGOON, 20, 66666, OTID_STRUCT_PRESET(777), 31);
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    struct NativeOwnedId appended;
    CHECK(!IsFollowerCatchFullParty() && NativeOwnedTrackerActive(457)
        && !NativeOwnedLedgerRead(457, (struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, 4}, &appended)
        && (appended.high || appended.low));
    OwnedCatchFollowerOverride(FALSE); gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = previousBattle;
    OwnedCatchFollowerRestore(); CHECK(NativeOwnedTrackerActive(457) && gPartiesCount[B_TRAINER_PLAYER] == 5
        && GetMonData(&gParties[B_TRAINER_PLAYER][4], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON);
    CHECK(!NativeOwnedTrackerStop(457));
    RhhModsSetRunContext(123, 458, 0); CHECK(!NativeOwnedLedgerStart(458));
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = mon;
    gParties[B_TRAINER_PLAYER][1].hp = gParties[B_TRAINER_PLAYER][5].hp = 0;
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE;
    for (u32 box = 0; box < TOTAL_BOXES_COUNT; ++box)
        for (u32 slot = 0; slot < IN_BOX_COUNT; ++slot) gPokemonStoragePtr->boxes[box][slot] = mon.box;
    CHECK(!NativeOwnedTrackerSeed(458) && !NativeOwnedTrackerStart(458));
    uint64_t beforeRevision, afterRevision;
    CHECK(!NativeOwnedLedgerRevision(458, &beforeRevision));
    OwnedCatchFollowerPrepare(); battle = (struct BattleStruct){0}; gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    gBattlersCount = 2; gBattlerPositions[0] = 0; gBattlerPositions[1] = 1;
    gBattlerPartyIndexes[0] = gBattlerPartyIndexes[1] = 0; gAbsentBattlerFlags = 0; gBattleMons[0].hp = gBattleMons[1].hp = 1;
    OwnedCatchFollowerOverride(TRUE);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_ZIGZAGOON, 20, 77777, OTID_STRUCT_PRESET(888), 31);
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    CHECK(!IsFollowerCatchFullParty() && NativeOwnedTrackerActive(458)
        && !NativeOwnedLedgerRevision(458, &afterRevision) && afterRevision == beforeRevision);
    OwnedCatchFollowerOverride(FALSE); gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = previousBattle;
    OwnedCatchFollowerRestore(); CHECK(NativeOwnedTrackerActive(458)); CHECK(!NativeOwnedTrackerStop(458));
    /* After a follower catch, a selected copy must still match the full party
     * from which the reduced party was rebuilt, even with matching IDs. */
    RhhModsSetRunContext(123, 459, 0); CHECK(!NativeOwnedLedgerStart(459));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = mon;
    gParties[B_TRAINER_PLAYER][1].hp = gParties[B_TRAINER_PLAYER][5].hp = 0;
    egg = TRUE; SetMonData(&gParties[B_TRAINER_PLAYER][3], MON_DATA_IS_EGG, &egg);
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE;
    CHECK(!NativeOwnedTrackerSeed(459) && !NativeOwnedTrackerStart(459));
    OwnedCatchFollowerPrepare(); battle = (struct BattleStruct){0}; gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER;
    gBattlersCount = 2; gBattlerPositions[0] = 0; gBattlerPositions[1] = 1;
    gBattlerPartyIndexes[0] = gBattlerPartyIndexes[1] = 0; gAbsentBattlerFlags = 0; gBattleMons[0].hp = gBattleMons[1].hp = 1;
    OwnedCatchFollowerOverride(TRUE);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_POOCHYENA, 20, 88888, OTID_STRUCT_PRESET(777), 31);
    ModsOwnedCatchTestStep(FALSE, PARTY_SIZE + 1); CHECK(IsFollowerCatchFullParty() && NativeOwnedTrackerActive(459));
    ModsOwnedFollowerCatchBeforeReduce(); CHECK(NativeOwnedTrackerActive(459));
    struct Pokemon fullParty[PARTY_SIZE]; memcpy(fullParty, gParties[B_TRAINER_PLAYER], sizeof(fullParty));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(fullParty));
    gParties[B_TRAINER_PLAYER][0] = fullParty[0]; gParties[B_TRAINER_PLAYER][1] = fullParty[2];
    gParties[B_TRAINER_PLAYER][2] = fullParty[4]; gParties[B_TRAINER_PLAYER][1].hp ^= 1;
    gPartiesCount[B_TRAINER_PLAYER] = 3; ModsOwnedFollowerCatchReduced(); CHECK(!NativeOwnedTrackerActive(459));
    OwnedCatchFollowerOverride(FALSE); gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = previousBattle;
    OwnedCatchFollowerRestore();
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned follower-catch ledger: full party and selected party reconciliation passed");
}
static void CheckOwnedConsistencyLedger(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = mon; gPartiesCount[B_TRAINER_PLAYER] = 1;
    gPokemonStoragePtr->boxes[2][5] = mon.box;
    gSaveBlock1Ptr->daycare.mons[0].mon = mon.box; gPokemonStoragePtr->fusions[0] = mon;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456));
    struct NativeOwnedLedgerSlot party = {NATIVE_OWNED_PARTY, 0, 0};
    struct NativeOwnedId partyId = GetLedgerSlot(party);
    ZeroMonData(&gParties[B_TRAINER_PLAYER][0]); CHECK(NativeOwnedTrackerStart(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    ZeroBoxMonData(&gPokemonStoragePtr->boxes[2][5]); CHECK(NativeOwnedTrackerStart(456));
    gPokemonStoragePtr->boxes[2][5] = mon.box;
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon); CHECK(NativeOwnedTrackerStart(456));
    gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
    ZeroMonData(&gPokemonStoragePtr->fusions[0]); CHECK(NativeOwnedTrackerStart(456));
    gPokemonStoragePtr->fusions[0] = mon;
    gPokemonStoragePtr->boxes[3][1] = mon.box; CHECK(NativeOwnedTrackerStart(456));
    ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][1]);
    CHECK(!NativeOwnedTrackerStart(456) && !NativeSaveBegin() && !NativeSaveEnd(784, TRUE));
    gMain.inBattle = TRUE; CHECK(NativeSaveBegin() && NativeOwnedTrackerActive(456)); gMain.inBattle = FALSE;
    gPokemonStoragePtr->boxes[3][1] = mon.box;
    CHECK(NativeSaveBegin() && !NativeOwnedTrackerActive(456) && !NativeSaveTransactionActive());
    CheckLedgerSlot(party, (struct NativeOwnedId){0, 0});
    ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][1]);
    CHECK(!NativeSaveRestoreMods(784)); CheckLedgerSlot(party, partyId);
    CHECK(!NativeOwnedTrackerStart(456)); ZeroMonData(&gParties[B_TRAINER_PLAYER][0]);
    CHECK(!NativeOwnedTrackerActive(456) && !NativeSaveTransactionActive());
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon);
    CHECK(NativeOwnedTrackerStop(456) && !NativeOwnedTrackerActive(456));
    gSaveBlock1Ptr->daycare.mons[0].mon = mon.box;
    CHECK(!NativeSaveRestoreMods(784)); CheckLedgerSlot(party, partyId);
    CHECK(!NativeOwnedTrackerStart(456) && !NativeSaveBegin());
    gPokemonStoragePtr->boxes[3][1] = mon.box;
    CHECK(NativeSaveEnd(785, TRUE) && !NativeOwnedTrackerActive(456) && !NativeSaveTransactionActive());
    ZeroBoxMonData(&gPokemonStoragePtr->boxes[3][1]);
    CHECK(!NativeSaveRestoreMods(784)); CheckLedgerSlot(party, partyId);
    CHECK(!NativeOwnedTrackerStart(456) && !NativeSaveBegin());
    CHECK(NativeOwnedTrackerStop(456) && NativeSaveEnd(786, TRUE));
    CHECK(!NativeSaveRestoreMods(784)); CheckLedgerSlot(party, partyId);
    CHECK(!NativeOwnedTrackerStart(456) && !NativeSaveRestoreMods(784));
    CHECK(!NativeOwnedTrackerActive(456)); CheckLedgerSlot(party, partyId);
    CHECK(!NativeOwnedTrackerStart(456) && !NativeSaveBegin() && !NativeSaveRestoreMods(784));
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(787, TRUE));
    CHECK(!NativeSaveRestoreMods(784)); CheckLedgerSlot(party, partyId);
    struct Pokemon replacement; CreateMonWithIVs(&replacement, SPECIES_TORCHIC, 20, 98765, OTID_STRUCT_PRESET(54321), 31);
    CHECK(!NativeOwnedTrackerStart(456));
    gPokemonStoragePtr->boxes[2][5] = replacement.box;
    CHECK(NativeSaveBegin() && !NativeOwnedTrackerActive(456));
    gPokemonStoragePtr->boxes[2][5] = mon.box;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456) && !NativeSaveBegin());
    gParties[B_TRAINER_PLAYER][0] = replacement;
    CHECK(NativeSaveEnd(788, TRUE) && !NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0] = mon;
    CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    u8 friendship = 50; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_FRIENDSHIP, &friendship);
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(789, TRUE) && NativeOwnedTrackerActive(456));
    CHECK(!NativeOwnedTrackerStop(456));
    RhhModsSetRunContext(123, 457, 0); CHECK(!NativeOwnedLedgerStart(457));
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(791, TRUE));
    RhhModsSetRunContext(123, 456, 0);
    CHECK(!NativeSaveRestoreMods(789) && !NativeOwnedTrackerStart(456));
    /* A checked gift must identify its actual destination and finish in the
     * same safe context. Even a byte-identical replacement is a new mon. */
    for (u32 which = 0; which < 5; ++which)
    {
        struct Pokemon same = gParties[B_TRAINER_PLAYER][0];
        struct RhhOwnedGift gift;
        CHECK(ModsOwnedGiftCapture(&same.box, RHH_GIFT_SCRIPTED, &gift));
        if (which == 0)
            ModsOwnedGiftDestination(&gParties[B_TRAINER_PLAYER][0].box, RHH_MON_PARTY, 0, 1, &gift);
        else if (which == 1)
            ModsOwnedGiftDestination(&gPokemonStoragePtr->boxes[2][5], RHH_MON_BOX, 2, 6, &gift);
        else
        {
            ModsOwnedGiftDestination(&gParties[B_TRAINER_PLAYER][0].box, RHH_MON_PARTY, 0, 0, &gift);
            CHECK(NativeOwnedTrackerActive(456));
            if (which == 2)
                ModsOwnedGiftCommitted(&gParties[B_TRAINER_PLAYER][1].box, &gift);
            else if (which == 3)
            {
                gParties[B_TRAINER_PLAYER][0] = same; gMain.inBattle = TRUE;
                ModsOwnedGiftCommitted(&gParties[B_TRAINER_PLAYER][0].box, &gift); gMain.inBattle = FALSE;
            }
            else
            {
                CHECK(!NativeSaveBegin()); gParties[B_TRAINER_PLAYER][0] = same;
                ModsOwnedGiftCommitted(&gParties[B_TRAINER_PLAYER][0].box, &gift);
                CHECK(NativeSaveEnd(790, TRUE) && !NativeSaveTransactionActive());
            }
        }
        CHECK(!NativeOwnedTrackerActive(456));
        CHECK(!NativeSaveRestoreMods(789) && !NativeOwnedTrackerStart(456));
    }
    /* A receipt must name the physical destination even for identical copies,
     * and an intervening unsafe context must invalidate the ledger. */
    for (u32 kind = 0; kind < 4; ++kind)
    {
        for (u32 unsafe = 0; unsafe < 2; ++unsafe)
        {
            struct Pokemon same = gParties[B_TRAINER_PLAYER][0];
            struct RhhDaycareTransfer daycare;
            struct RhhFusionTransfer fusion;
            struct RhhOwnedEvolution evolution;
            struct RhhCatchPlacement placement;
            if (kind == 0)
            {
                CHECK(ModsDaycareTransferCapture(&gSaveBlock1Ptr->daycare.mons[0].mon, RHH_DAYCARE_WITHDRAW,
                    RHH_MON_DAYCARE, 0, 0, RHH_MON_PARTY, 0, 0, &daycare));
                if (unsafe) gMain.inBattle = TRUE;
                ModsDaycareTransferCommitted(unsafe ? &gParties[B_TRAINER_PLAYER][0].box : &same.box, &daycare);
            }
            else if (kind == 1)
            {
                CHECK(ModsFusionTransferCapture(&gPokemonStoragePtr->fusions[0].box, RHH_FUSION_RESTORE,
                    RHH_MON_FUSION_STORAGE, 0, 0, &fusion));
                if (unsafe) gMain.inBattle = TRUE;
                ModsFusionTransferCommitted(unsafe ? &gParties[B_TRAINER_PLAYER][0].box : &same.box,
                                             RHH_MON_PARTY, 0, 0, &fusion);
            }
            else if (kind == 2)
            {
                CHECK(ModsOwnedEvolutionCapture(&gParties[B_TRAINER_PLAYER][0], 0, FALSE, &evolution));
                if (unsafe) { gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_LINK; }
                ModsOwnedEvolutionCommitted(unsafe ? &gParties[B_TRAINER_PLAYER][0] : &same, &evolution);
            }
            else
            {
                gMain.inBattle = TRUE; gBattleTypeFlags = 0;
                CHECK(ModsCatchPlacementCapture(&gParties[B_TRAINER_PLAYER][0].box,
                    RHH_CATCH_DISPLACED_PARTY, 0, &placement));
                if (unsafe) gBattleTypeFlags = BATTLE_TYPE_LINK;
                ModsCatchPlacementCommitted(unsafe ? &gPokemonStoragePtr->boxes[2][5] : &same.box,
                                             RHH_MON_BOX, 2, 5, &placement);
            }
            gMain.inBattle = FALSE; gBattleTypeFlags = 0;
            CHECK(!NativeOwnedTrackerActive(456));
            CHECK(!NativeSaveRestoreMods(789) && !NativeOwnedTrackerStart(456));
        }
    }
    struct RhhDaycareTransfer interrupted;
    CHECK(ModsDaycareTransferCapture(&gSaveBlock1Ptr->daycare.mons[0].mon, RHH_DAYCARE_WITHDRAW,
        RHH_MON_DAYCARE, 0, 0, RHH_MON_PARTY, 0, 0, &interrupted));
    CHECK(!NativeSaveBegin());
    ModsDaycareTransferCommitted(&gParties[B_TRAINER_PLAYER][0].box, &interrupted);
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(790, TRUE) && !NativeSaveTransactionActive());
    CHECK(!NativeSaveRestoreMods(789) && !NativeOwnedTrackerStart(456));
    struct Pokemon tradeCopy = gParties[B_TRAINER_PLAYER][0];
    struct RhhIngameTrade trade;
    CHECK(!ModsIngameTradeCapture(&tradeCopy.box, &tradeCopy.box, RHH_MON_PARTY, 0, 0, &trade));
    CHECK(NativeOwnedTrackerActive(456));
    for (u32 which = 0; which < 3; ++which)
    {
        CHECK(ModsIngameTradeCapture(&gParties[B_TRAINER_PLAYER][0].box, &tradeCopy.box,
                                     RHH_MON_PARTY, 0, 0, &trade));
        if (which == 1) gMain.inBattle = TRUE;
        if (which == 2) CHECK(!NativeSaveBegin());
        ModsIngameTradeCommitted(which == 0 ? &tradeCopy.box : &gParties[B_TRAINER_PLAYER][0].box,
                                  RHH_MON_PARTY, 0, 0, &trade);
        gMain.inBattle = FALSE;
        CHECK(!NativeOwnedTrackerActive(456));
        if (which == 2) CHECK(NativeSaveEnd(790, TRUE) && !NativeSaveTransactionActive());
        CHECK(!NativeSaveRestoreMods(789) && !NativeOwnedTrackerStart(456));
    }
    struct RhhPartyReorder reorder;
    CHECK(ModsPartyReorderBegin(RHH_PARTY_REORDER_SWAP, &reorder));
    ModsPartyReorderCommitted(&reorder); CHECK(NativeOwnedTrackerActive(456));
    CHECK(ModsPartyReorderBegin(RHH_PARTY_REORDER_SWAP, &reorder));
    reorder.struct_size = 0; ModsPartyReorderCommitted(&reorder);
    CHECK(!NativeOwnedTrackerActive(456));
    CHECK(!NativeSaveRestoreMods(789) && !NativeOwnedTrackerStart(456));
    CHECK(ModsPartyReorderBegin(RHH_PARTY_REORDER_SWAP, &reorder));
    ModsPartyReorderAdd(&reorder, 0, 1); gMain.inBattle = TRUE;
    ModsPartyReorderCommitted(&reorder); gMain.inBattle = FALSE;
    CHECK(!NativeOwnedTrackerActive(456));
    CHECK(!NativeSaveRestoreMods(789) && !NativeOwnedTrackerStart(456));
    MainCallback previous = gMain.callback2; CHECK(ModsOwnedStorageTestOpen());
    gPokemonStoragePtr->boxes[2][5] = replacement.box;
    CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 2, 5) && !NativeOwnedTrackerActive(456));
    ModsOwnedStorageTestClose(); SetMainCallback2(previous);
    gPokemonStoragePtr->boxes[2][5] = mon.box;
    CHECK(!NativeSaveRestoreMods(789) && !NativeOwnedTrackerStart(456));
    for (u32 i = 0; i < 500; ++i)
        CHECK(GiveScriptedMonToPlayer(&replacement, 0) == MON_GIVEN_TO_PARTY && NativeOwnedTrackerActive(456));
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(790, TRUE) && !NativeOwnedTrackerStop(456));
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned consistency ledger: activation and save occupancy checks passed");
}
static void CheckOwnedConsistencyReload(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon original, replacement;
    CreateMonWithIVs(&original, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    CreateMonWithIVs(&replacement, SPECIES_TORCHIC, 20, 98765, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = original; gPartiesCount[B_TRAINER_PLAYER] = 1;
    gPokemonStoragePtr->boxes[2][5] = original.box;
    gSaveBlock1Ptr->daycare.mons[0].mon = original.box; gPokemonStoragePtr->fusions[0] = original;
    CHECK(NativeOwnedTrackerStart(456)); /* Same occupied slot, wrong saved individual. */
    gParties[B_TRAINER_PLAYER][0] = replacement;
    gParties[B_TRAINER_PLAYER][0].box.otName[0] ^= 1;
    CHECK(NativeOwnedTrackerStart(456)); /* Same OT and personality, different trainer name. */
    gParties[B_TRAINER_PLAYER][0] = replacement;
    CHECK(!NativeOwnedTrackerStart(456) && NativeOwnedTrackerActive(456));
    CHECK(!NativeOwnedTrackerStop(456));
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned consistency reload: saved witnesses reject substituted slots");
}
static void CheckOwnedConsistencyV1(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon original, replacement;
    CreateMonWithIVs(&original, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    CreateMonWithIVs(&replacement, SPECIES_TORCHIC, 20, 98765, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = replacement; gPartiesCount[B_TRAINER_PLAYER] = 1;
    gPokemonStoragePtr->boxes[2][5] = original.box;
    gSaveBlock1Ptr->daycare.mons[0].mon = original.box; gPokemonStoragePtr->fusions[0] = original;
    CHECK(!NativeOwnedTrackerStart(456));
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(792, TRUE));
    gParties[B_TRAINER_PLAYER][0].box.otName[0] ^= 1;
    CHECK(NativeOwnedTrackerAudit() && !NativeOwnedTrackerActive(456));
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned consistency v1: migrated names are checked");
}
static void CheckOwnedConsistencyLegacy(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon original, replacement;
    CreateMonWithIVs(&original, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    CreateMonWithIVs(&replacement, SPECIES_TORCHIC, 20, 98765, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = replacement; gPartiesCount[B_TRAINER_PLAYER] = 1;
    gPokemonStoragePtr->boxes[2][5] = original.box;
    gSaveBlock1Ptr->daycare.mons[0].mon = original.box; gPokemonStoragePtr->fusions[0] = original;
    struct NativeOwnedId id;
    CHECK(!NativeOwnedLedgerRead(456, (struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, 0}, &id)
        && (id.high || id.low));
    CHECK(NativeOwnedTrackerStart(456)); /* Missing witnesses cannot establish provenance. */
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned consistency legacy: missing witnesses remain untrusted");
}
static void CheckOwnedConsistencyReject(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsFinalize());
    CHECK(NativeSaveOpen(path)); NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned consistency: invalid witness archive rejected");
}
static void CheckOwnedConsistencyRollback(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon original, replacement;
    CreateMonWithIVs(&original, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    CreateMonWithIVs(&replacement, SPECIES_TORCHIC, 20, 98765, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = replacement; gPartiesCount[B_TRAINER_PLAYER] = 1;
    gPokemonStoragePtr->boxes[2][5] = original.box;
    gSaveBlock1Ptr->daycare.mons[0].mon = original.box; gPokemonStoragePtr->fusions[0] = original;
    struct NativeOwnedLedgerSlot party = {NATIVE_OWNED_PARTY, 0, 0};
    struct NativeOwnedId saved = GetLedgerSlot(party);
    CHECK(!NativeOwnedTrackerStart(456) && NativeSaveRestoreMods(789));
    CHECK(!NativeOwnedTrackerActive(456)); CheckLedgerSlot(party, saved);
    CHECK(!NativeOwnedTrackerStart(456) && !NativeOwnedTrackerStop(456));
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned consistency: rejected generation preserves current witnesses");
}
static void CheckOwnedTradeLedger(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon original; CreateMonWithIVs(&original, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_OPPONENT_A][0] = original;
    gPartiesCount[B_TRAINER_PLAYER] = 1; gPokemonStoragePtr->boxes[2][5] = original.box;
    struct NativeOwnedLedgerSlot party = {NATIVE_OWNED_PARTY, 0, 0}, source = {NATIVE_OWNED_BOX, 2, 5};
    struct NativeOwnedLedgerSlot held = {NATIVE_OWNED_TRADE, 0, 0};
    struct NativeOwnedId empty = {0, 0};
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    struct NativeOwnedId originalParty = GetLedgerSlot(party), originalBox = GetLedgerSlot(source);
    CHECK(ModsOwnedTradeTestPartyEvolve(0, SPECIES_COMBUSKEN) && NativeOwnedTrackerActive(456));
    struct NativeOwnedId receivedParty = GetLedgerSlot(party);
    CHECK(DifferentLedgerId(receivedParty, originalParty) && DifferentLedgerId(receivedParty, originalBox));
    CHECK(ModsOwnedTradeTestBeginPc(2, 5)); CheckLedgerSlot(source, empty); CheckLedgerSlot(held, originalBox);
    CHECK(NativeSaveBegin());
    gParties[B_TRAINER_OPPONENT_A][0] = original;
    CHECK(ModsOwnedTradeTestFinishPc() && NativeOwnedTrackerActive(456));
    struct NativeOwnedId receivedBox = GetLedgerSlot(held);
    CHECK(DifferentLedgerId(receivedBox, originalBox) && DifferentLedgerId(receivedBox, receivedParty));
    u32 box, slot; CHECK(ModsOwnedTradePcDestination(&box, &slot));
    struct NativeOwnedLedgerSlot destination = {NATIVE_OWNED_BOX, box, slot};
    CheckLedgerSlot(destination, empty);
    ModsOwnedEvolutionTestCommit(PC_MON_CHOSEN, SPECIES_COMBUSKEN, TRUE);
    CHECK(NativeOwnedTrackerActive(456)); CheckLedgerSlot(held, receivedBox);
    CHECK(ModsOwnedTradeTestSyncPc() && NativeOwnedTrackerActive(456));
    CheckLedgerSlot(held, empty); CheckLedgerSlot(destination, receivedBox);
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveBegin() && !NativeSaveEnd(780, TRUE));
    CHECK(!NativeSaveRestoreMods(780) && !NativeOwnedTrackerStart(456));
    CheckLedgerSlot(party, receivedParty); CheckLedgerSlot(destination, receivedBox);
    gReceivedRemoteLinkPlayers = TRUE; gLinkErrorOccurred = FALSE; gWirelessCommType = 0;
    gParties[B_TRAINER_OPPONENT_A][2] = original;
    CHECK(ModsOwnedLinkTradeTestBeginAt(0, 2) && NativeOwnedTrackerActive(456));
    struct NativeOwnedId incoming = GetLedgerSlot(party);
    CHECK(DifferentLedgerId(incoming, receivedParty)); CheckLedgerSlot(held, receivedParty);
    CHECK(NativeSaveBegin()); ModsOwnedLinkTradeTestConfirm();
    gFlashMemoryPresent = FALSE; CHECK(ModsOwnedLinkTradeTestBeginSave());
    CHECK(!NativeOwnedTrackerActive(456)); ModsOwnedLinkTradeTestEnd();
    CHECK(!NativeSaveRestoreMods(780)); CheckLedgerSlot(party, receivedParty);
    CHECK(!NativeOwnedTrackerStart(456));
    /* A slot changed after link confirmation must not become a saved receipt
     * or retain the incoming binding, even if OT/personality still match. */
    struct Pokemon savedPlayer = gParties[B_TRAINER_PLAYER][0];
    for (u32 changed = 0; changed < 2; ++changed)
    {
        gParties[B_TRAINER_OPPONENT_A][0] = original;
        CHECK(ModsOwnedLinkTradeTestBegin(0) && NativeOwnedTrackerActive(456));
        ModsOwnedLinkTradeTestConfirm();
        if (!changed) { u8 iv = 7; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV, &iv); }
        else gParties[B_TRAINER_PLAYER][0].box.otName[0] ^= 1;
        gFlashMemoryPresent = TRUE; Save_ResetSaveCounters();
        CHECK(!ModsOwnedLinkTradeTestBeginSave() && NativeSaveTransactionActive() && !NativeOwnedTrackerActive(456));
        CHECK(NativeSaveEnd(785, FALSE)); ModsOwnedLinkTradeTestEnd();
        gParties[B_TRAINER_PLAYER][0] = savedPlayer;
        CHECK(!NativeSaveRestoreMods(780) && !NativeOwnedTrackerStart(456));
    }
    gParties[B_TRAINER_OPPONENT_A][0] = original;
    CHECK(ModsOwnedLinkTradeTestBegin(0)); incoming = GetLedgerSlot(party);
    ModsOwnedLinkTradeTestConfirm(); gFlashMemoryPresent = TRUE; Save_ResetSaveCounters();
    CHECK(!ModsOwnedLinkTradeTestBeginSave() && NativeSaveTransactionActive());
    bool32 finishedSave = FALSE;
    for (u32 i = 0; i < NUM_SECTORS_PER_SLOT + 2 && !finishedSave; ++i) finishedSave = LinkFullSave_WriteSector();
    CHECK(finishedSave); LinkFullSave_ReplaceLastSector(); ModsOwnedLinkTradeTestCompleteSave();
    CHECK(!NativeSaveTransactionActive() && !gDamagedSaveSectors && NativeOwnedTrackerActive(456));
    CheckLedgerSlot(party, incoming); CheckLedgerSlot(held, empty); ModsOwnedLinkTradeTestEnd();
    gParties[B_TRAINER_OPPONENT_A][0] = original;
    CHECK(ModsOwnedLinkTradeTestBegin(0)); incoming = GetLedgerSlot(party);
    ModsOwnedLinkTradeTestConfirm(); gFlashMemoryPresent = TRUE; Save_ResetSaveCounters();
    CHECK(!ModsOwnedLinkTradeTestBeginSave() && NativeSaveTransactionActive());
    CheckLedgerSlot(held, empty); CHECK(NativeSaveEnd(781, FALSE));
    ModsOwnedLinkTradeTestSaved(FALSE); CHECK(!NativeOwnedTrackerActive(456)); ModsOwnedLinkTradeTestEnd();
    CHECK(!NativeSaveRestoreMods(780)); CheckLedgerSlot(party, receivedParty);
    CHECK(!NativeOwnedTrackerStart(456));
    gParties[B_TRAINER_OPPONENT_A][0] = original;
    CHECK(ModsOwnedLinkTradeTestBegin(0)); incoming = GetLedgerSlot(party);
    ModsOwnedLinkTradeTestConfirm(); ModsOwnedEvolutionTestCommit(0, SPECIES_COMBUSKEN, TRUE);
    CHECK(NativeOwnedTrackerActive(456)); CheckLedgerSlot(party, incoming);
    CHECK(!ModsOwnedLinkTradeTestBeginSave() && NativeSaveTransactionActive());
    CHECK(!NativeSaveEnd(782, TRUE)); ModsOwnedLinkTradeTestEnd(); NativeSaveClose();
    CHECK(!NativeSaveOpen(path) && !NativeSaveRestoreMods(782));
    CheckLedgerSlot(party, incoming); CheckLedgerSlot(destination, receivedBox);
    RhhModsSetRunContext(123, 456, 0);
    CHECK(!NativeOwnedTrackerStart(456)); gParties[B_TRAINER_OPPONENT_A][0] = original;
    CHECK(ModsOwnedLinkTradeTestBegin(0) && NativeOwnedTrackerActive(456));
    ModsOwnedLinkTradeTestEnd(); CHECK(!NativeOwnedTrackerActive(456));
    CHECK(!NativeSaveRestoreMods(782)); CheckLedgerSlot(party, incoming);
    /* Link evolution must be tracked for the saved receipt even when no mod
     * observes evolution and the experimental identity tracker is inactive. */
    const struct RhhOwnedMonService *owned = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 46));
    CHECK(owned && owned->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_46_SIZE);
    u32 priorHistory = owned->link_trade_history_count();
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_OPPONENT_A][0] = original;
    CHECK(ModsOwnedLinkTradeTestBegin(0)); ModsOwnedLinkTradeTestConfirm();
    ModsOwnedEvolutionTestCommit(0, SPECIES_COMBUSKEN, TRUE);
    CHECK(!ModsOwnedLinkTradeTestBeginSave() && !NativeSaveEnd(783, TRUE));
    ModsOwnedLinkTradeTestSaved(TRUE); CHECK(owned->link_trade_history_count() == priorHistory + 1);
    ModsOwnedLinkTradeTestEnd();
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned trade ledger: PC staging, link commit and rollback passed");
}
static void CheckOwnedTradeCopyGuard(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon mon; CreateMonWithIVs(&mon, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = mon; gParties[B_TRAINER_OPPONENT_A][0] = mon;
    gPartiesCount[B_TRAINER_PLAYER] = 1; gPokemonStoragePtr->boxes[2][5] = mon.box;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(784, TRUE));
    for (u32 which = 0; which < 2; ++which)
    {
        CHECK(ModsOwnedTradeTestBeginPc(2, 5)); gParties[B_TRAINER_OPPONENT_A][0] = mon;
        CHECK(ModsOwnedTradeTestFinishPc() && NativeOwnedTrackerActive(456));
        u32 box, slot; CHECK(ModsOwnedTradePcDestination(&box, &slot));
        if (!which) UpdateMonPersonality(&gParties[B_TRAINER_OPPONENT_A][TRADEMON_FROM_PC].box, 98765);
        else UpdateMonPersonality(&gPokemonStoragePtr->boxes[box][slot], 98765);
        CHECK(ModsOwnedTradeTestSyncPc() && !NativeOwnedTrackerActive(456));
        ZeroBoxMonData(&gPokemonStoragePtr->boxes[box][slot]); gPokemonStoragePtr->boxes[2][5] = mon.box;
        CHECK(!NativeSaveRestoreMods(784) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!NativeOwnedTrackerStop(456)); NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned trade copy: changed staged or provisional Pokemon invalidates tracking");
}
static void CheckOwnedEggLedger(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon original, ditto;
    CreateMonWithIVs(&original, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    CreateMonWithIVs(&ditto, SPECIES_DITTO, 20, 67890, OTID_STRUCT_PRESET(777), 31);
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_PLAYER][2] = original;
    gPartiesCount[B_TRAINER_PLAYER] = 3;
    gSaveBlock1Ptr->daycare.mons[0].mon = original.box; gSaveBlock1Ptr->daycare.mons[1].mon = ditto.box;
    struct NativeOwnedLedgerSlot p0 = {NATIVE_OWNED_PARTY, 0, 0}, p1 = {NATIVE_OWNED_PARTY, 0, 1};
    struct NativeOwnedLedgerSlot p2 = {NATIVE_OWNED_PARTY, 0, 2}, day0 = {NATIVE_OWNED_DAYCARE, 0, 0};
    struct NativeOwnedLedgerSlot day1 = {NATIVE_OWNED_DAYCARE, 0, 1};
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456));
    struct NativeOwnedId a = GetLedgerSlot(p0), b = GetLedgerSlot(p2);
    struct NativeOwnedId parent0 = GetLedgerSlot(day0), parent1 = GetLedgerSlot(day1);
    CHECK(DifferentLedgerId(a, b) && !NativeOwnedTrackerStart(456));
    CHECK(GetDaycareCompatibilityScoreBase(&gSaveBlock1Ptr->daycare) != PARENTS_INCOMPATIBLE);
    gSaveBlock2Ptr->playerTrainerId[0] = 0x44; gSaveBlock2Ptr->playerTrainerId[1] = 0x33;
    gSaveBlock2Ptr->playerTrainerId[2] = 0x22; gSaveBlock2Ptr->playerTrainerId[3] = 0x11;
    FlagSet(FLAG_PENDING_DAYCARE_EGG); u32 received = PARTY_SIZE;
    CHECK(GiveEggFromDaycareChecked(&received) && received == 2 && NativeOwnedTrackerActive(456));
    CheckLedgerSlot(p0, a); CheckLedgerSlot(p1, b); CheckLedgerSlot(day0, parent0); CheckLedgerSlot(day1, parent1);
    struct NativeOwnedId egg = GetLedgerSlot(p2);
    CHECK(DifferentLedgerId(egg, a) && DifferentLedgerId(egg, b) && DifferentLedgerId(egg, parent0)
        && GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_IS_EGG));
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_OT_ID) == 0x11223344);
    gSaveBlock2Ptr->playerName[0] ^= 1; /* Hatch under a different trainer name as well as ID. */
    gSaveBlock2Ptr->playerTrainerId[0] = 0x78; gSaveBlock2Ptr->playerTrainerId[1] = 0x56;
    gSaveBlock2Ptr->playerTrainerId[2] = 0x34; gSaveBlock2Ptr->playerTrainerId[3] = 0x12;
    gSaveBlock1Ptr->location.mapGroup = 1; gSaveBlock1Ptr->location.mapNum = 0;
    rng_value_t beforeHatch = gRngValue; gSpecialVar_0x8004 = 2; ScriptHatchMon();
    CHECK(NativeOwnedTrackerActive(456) && !GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_IS_EGG)
        && GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_OT_ID) == 0x12345678
        && !memcmp(&beforeHatch, &gRngValue, sizeof(beforeHatch)));
    CheckLedgerSlot(p2, egg); ScriptHatchMon(); CheckLedgerSlot(p2, egg);
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveBegin() && !NativeSaveEnd(779, 1));
    CHECK(!NativeSaveRestoreMods(779)); CheckLedgerSlot(p2, egg); CheckLedgerSlot(day0, parent0);
    CHECK(!NativeOwnedTrackerStart(456));
    CHECK(!NativeSaveBegin()); gSpecialVar_0x8004 = 2; ScriptHatchMon();
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(781, 1));
    CheckLedgerSlot(p2, (struct NativeOwnedId){0, 0});
    CHECK(!NativeSaveRestoreMods(779)); CheckLedgerSlot(p2, egg); CHECK(!NativeOwnedTrackerStart(456));
    CHECK(!NativeSaveBegin() && GiveScriptedMonToPlayer(&original, 0) == MON_GIVEN_TO_PARTY);
    CHECK(!NativeOwnedTrackerActive(456)); CHECK(NativeSaveEnd(782, 1));
    CHECK(!NativeSaveRestoreMods(779)); CheckLedgerSlot(p0, a); CHECK(!NativeOwnedTrackerStart(456));
    struct NativeOwnedLedgerSlot p3 = {NATIVE_OWNED_PARTY, 0, 3}; struct NativeOwnedId stale;
    uint64_t revision; CHECK(!NativeOwnedLedgerRevision(456, &revision) && !NativeOwnedLedgerMint(456, revision, p3, &stale));
    FlagSet(FLAG_PENDING_DAYCARE_EGG); CHECK(GiveEggFromDaycareChecked(&received) && received == 3);
    CHECK(!NativeOwnedTrackerActive(456)); CheckLedgerSlot(p2, (struct NativeOwnedId){0, 0});
    CHECK(!NativeSaveRestoreMods(779)); ZeroMonData(&gParties[B_TRAINER_PLAYER][3]); gPartiesCount[B_TRAINER_PLAYER] = 3;
    CHECK(!NativeOwnedTrackerStart(456));
    gParties[B_TRAINER_PLAYER][5] = original; bool8 isEgg = TRUE;
    SetMonData(&gParties[B_TRAINER_PLAYER][5], MON_DATA_IS_EGG, &isEgg);
    gSpecialVar_0x8004 = 5; ScriptHatchMon();
    CHECK(!NativeOwnedTrackerActive(456)); CheckLedgerSlot(p2, (struct NativeOwnedId){0, 0});
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned egg ledger: daycare creation, compaction, hatch and fail-closed passed");
}
static void CheckOwnedCatchLedger(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon original; CreateMonWithIVs(&original, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = original;
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE; VarSet(VAR_PC_BOX_TO_SEND_MON, 0);
    struct NativeOwnedLedgerSlot p1 = {NATIVE_OWNED_PARTY, 0, 1};
    struct NativeOwnedLedgerSlot box0 = {NATIVE_OWNED_BOX, 0, 0}, box1 = {NATIVE_OWNED_BOX, 0, 1};
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456));
    struct NativeOwnedId displaced = GetLedgerSlot(p1); CHECK(!NativeOwnedTrackerStart(456));
    struct BattleStruct battle = {0}; struct BattleStruct *previousBattle = gBattleStruct;
    bool8 oldBattle = gMain.inBattle; u32 oldFlags = gBattleTypeFlags;
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattleTypeFlags = 0;
    gBattlersCount = 2; gBattlerPositions[0] = 0; gBattlerPositions[1] = 1;
    gBattlerPartyIndexes[0] = gBattlerPartyIndexes[1] = 0; gAbsentBattlerFlags = 0; gBattleMons[1].hp = 1;
    struct NativeOwnedId fieldIds[PARTY_SIZE];
    for (u32 i = 0; i < PARTY_SIZE; ++i)
        fieldIds[i] = GetLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i});
    gPartyMenu.layout = PARTY_LAYOUT_SINGLE;
    gBattlePartyCurrentOrder[0] = 0x10; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && ModsOwnedPartyBattleTestSwitch(0, 1)
        && ModsOwnedPartyBattleTestSwitch(1, 2) && ModsOwnedPartyBattleTestOrder(FALSE));
    CHECK(NativeOwnedTrackerActive(456));
    for (u32 i = 0; i < PARTY_SIZE; ++i)
        CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, fieldIds[i]);
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_TORCHIC, 20, 33333, OTID_STRUCT_PRESET(777), 31);
    ModsOwnedCatchTestStep(FALSE, 1); CheckLedgerSlot(box0, displaced); CheckLedgerSlot(p1, (struct NativeOwnedId){0, 0});
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    struct NativeOwnedId acquired = GetLedgerSlot(p1); CHECK(DifferentLedgerId(acquired, displaced) && NativeOwnedTrackerActive(456));
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_TORCHIC, 20, 33333, OTID_STRUCT_PRESET(777), 31);
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    struct NativeOwnedId boxed = GetLedgerSlot(box1);
    CHECK(DifferentLedgerId(boxed, acquired) && DifferentLedgerId(boxed, displaced) && NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = previousBattle;
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveBegin() && !NativeSaveEnd(780, 1));
    CHECK(!NativeSaveRestoreMods(780)); CheckLedgerSlot(box0, displaced); CheckLedgerSlot(box1, boxed); CheckLedgerSlot(p1, acquired);
    CHECK(!NativeOwnedTrackerStart(456));
    struct Pokemon savedParty[PARTY_SIZE]; memcpy(savedParty, gParties[B_TRAINER_PLAYER], sizeof(savedParty));
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattleTypeFlags = 0; gPartyMenu.layout = PARTY_LAYOUT_SINGLE;
    gBattlePartyCurrentOrder[0] = 0x10; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && ModsOwnedPartyTestSwap(0, 1) && ModsOwnedPartyBattleTestOrder(FALSE));
    CHECK(!NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = previousBattle;
    memcpy(gParties[B_TRAINER_PLAYER], savedParty, sizeof(savedParty));
    CHECK(!NativeSaveRestoreMods(780) && !NativeOwnedTrackerStart(456));
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    ModsOwnedCatchTestStep(FALSE, 2); CHECK(!NativeOwnedTrackerActive(456)); CheckLedgerSlot(box0, (struct NativeOwnedId){0, 0});
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = previousBattle;
    RhhModsSetRunContext(123, 457, 0); CHECK(!NativeOwnedLedgerStart(457));
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = original;
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE;
    for (u32 box = 0; box < TOTAL_BOXES_COUNT; ++box)
        for (u32 slot = 0; slot < IN_BOX_COUNT; ++slot) gPokemonStoragePtr->boxes[box][slot] = original.box;
    CHECK(!NativeOwnedTrackerSeed(457) && !NativeOwnedTrackerStart(457));
    uint64_t revision, afterRevision; struct NativeOwnedId prior, after;
    CHECK(!NativeOwnedLedgerRevision(457, &revision) && !NativeOwnedLedgerRead(457, p1, &prior));
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattleTypeFlags = 0;
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_TORCHIC, 20, 33333, OTID_STRUCT_PRESET(777), 31);
    ModsOwnedCatchTestStep(FALSE, 2); ModsOwnedCatchTestStep(TRUE, PARTY_SIZE);
    CHECK(NativeOwnedTrackerActive(457) && !NativeOwnedLedgerRevision(457, &afterRevision)
        && revision == afterRevision && !NativeOwnedLedgerRead(457, p1, &after) && !DifferentLedgerId(prior, after));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = previousBattle;
    CHECK(!NativeOwnedTrackerStop(457));
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned catch ledger: displacement, acquisition and unsafe battle invalidation passed");
}
static void CheckOwnedWorldLedger(const char *path)
{
    InitModsForTest(); CHECK(!NativeSaveOpen(path)); RhhModsSetRunContext(123, 456, 0);
    CHECK(!RhhModsHasHook(RHH_PARTY_REORDERED) && !RhhModsHasHook(RHH_OWNED_DAYCARE_TRANSFERRED)
        && !RhhModsHasHook(RHH_OWNED_FUSION_TRANSFERRED) && !RhhModsHasHook(RHH_OWNED_GIFT_RECEIVED)
        && !RhhModsHasHook(RHH_OWNED_EVOLUTION_COMMITTED));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    struct Pokemon original; CreateMonWithIVs(&original, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_PLAYER][1] = original;
    gPartiesCount[B_TRAINER_PLAYER] = 2; gPokemonStoragePtr->boxes[2][3] = original.box;
    gPokemonStoragePtr->fusions[0] = original; gPokemonStoragePtr->fusions[0].box.secure.raw[0] ^= 1;
    struct NativeOwnedLedgerSlot p0 = {NATIVE_OWNED_PARTY, 0, 0}, p1 = {NATIVE_OWNED_PARTY, 0, 1};
    struct NativeOwnedLedgerSlot p2 = {NATIVE_OWNED_PARTY, 0, 2}, p3 = {NATIVE_OWNED_PARTY, 0, 3};
    struct NativeOwnedLedgerSlot box = {NATIVE_OWNED_BOX, 2, 3};
    struct NativeOwnedLedgerSlot day0 = {NATIVE_OWNED_DAYCARE, 0, 0}, day1 = {NATIVE_OWNED_DAYCARE, 0, 1};
    struct NativeOwnedLedgerSlot fusion0 = {NATIVE_OWNED_FUSION, 0, 0}, fusion1 = {NATIVE_OWNED_FUSION, 0, 1};
    struct NativeOwnedId empty = {0, 0};
    CHECK(!NativeOwnedLedgerStart(456));
    bool8 oldBattle = gMain.inBattle; gMain.inBattle = TRUE;
    CHECK(NativeOwnedTrackerSeed(456)); gMain.inBattle = oldBattle;
    rng_value_t beforeSeed = gRngValue;
    CHECK(!NativeOwnedTrackerSeed(456) && !memcmp(&beforeSeed, &gRngValue, sizeof(beforeSeed)));
    struct NativeOwnedId a = GetLedgerSlot(p0), b = GetLedgerSlot(p1), c = GetLedgerSlot(box), damaged = GetLedgerSlot(fusion0);
    CHECK(DifferentLedgerId(a, b) && DifferentLedgerId(a, c) && DifferentLedgerId(b, c)
        && DifferentLedgerId(damaged, a) && NativeOwnedTrackerSeed(456));
    CHECK(!NativeOwnedTrackerStart(456));
    CHECK(ModsOwnedPartyTestSwap(0, 1)); CheckLedgerSlot(p0, b); CheckLedgerSlot(p1, a);
    gSpecialVar_0x8004 = 0; StoreSelectedPokemonInDaycare();
    CheckLedgerSlot(day0, b); CheckLedgerSlot(p0, a); CheckLedgerSlot(p1, empty);
    gSpecialVar_0x8004 = PC_MON_CHOSEN; gSpecialVar_MonBoxId = 2; gSpecialVar_MonBoxPos = 3;
    StoreSelectedPokemonInDaycare(); CheckLedgerSlot(day1, c); CheckLedgerSlot(box, empty);
    gSpecialVar_0x8004 = 0; CHECK(TakePokemonFromDaycare() == SPECIES_TORCHIC);
    CheckLedgerSlot(p0, a); CheckLedgerSlot(p1, b); CheckLedgerSlot(day0, c); CheckLedgerSlot(day1, empty);
    CHECK(ModsOwnedFusionTestStep(RHH_FUSION_STORE, 0, 1, 1, SPECIES_COMBUSKEN));
    CheckLedgerSlot(p0, a); CheckLedgerSlot(p1, empty); CheckLedgerSlot(fusion1, b);
    CHECK(ModsOwnedFusionTestStep(RHH_FUSION_RESTORE, 0, 1, 1, SPECIES_TORCHIC));
    CheckLedgerSlot(p0, a); CheckLedgerSlot(p1, b); CheckLedgerSlot(fusion1, empty);
    CHECK(GiveScriptedMonToPlayer(&original, 1) == MON_GIVEN_TO_PARTY);
    struct NativeOwnedId gift = GetLedgerSlot(p1); CHECK(DifferentLedgerId(gift, b));
    struct Pokemon nincada; CreateMonWithIVs(&nincada, SPECIES_NINCADA, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    CHECK(GiveScriptedMonToPlayer(&nincada, 0) == MON_GIVEN_TO_PARTY);
    struct NativeOwnedId parent = GetLedgerSlot(p0); CHECK(DifferentLedgerId(parent, a));
    CHECK(AddBagItem(ITEM_POKE_BALL, 1)); ModsOwnedEvolutionTestCommit(0, SPECIES_NINJASK, FALSE);
    CheckLedgerSlot(p0, parent); ModsOwnedEvolutionTestSplit(0, SPECIES_NINCADA, SPECIES_NINJASK);
    struct NativeOwnedId split = GetLedgerSlot(p2);
    CHECK(DifferentLedgerId(split, parent) && NativeOwnedTrackerActive(456));
    CHECK(ScriptGiveEgg(SPECIES_TORCHIC) == MON_GIVEN_TO_PARTY);
    struct NativeOwnedId egg = GetLedgerSlot(p3);
    CHECK(DifferentLedgerId(egg, parent) && DifferentLedgerId(egg, split));
    struct NativeOwnedLedgerSlot p4 = {NATIVE_OWNED_PARTY, 0, 4}, p5 = {NATIVE_OWNED_PARTY, 0, 5};
    CHECK(GiveMysteryEventTestMon(&original) == MEVENT_STATUS_SUCCESS && gPartiesCount[B_TRAINER_PLAYER] == 5);
    struct NativeOwnedId mystery = GetLedgerSlot(p4);
    CHECK(DifferentLedgerId(mystery, gift) && DifferentLedgerId(mystery, parent) && NativeOwnedTrackerActive(456));
    CheckLedgerSlot(p5, empty);
    uint64_t beforeFull, afterFull; CHECK(!NativeOwnedLedgerRevision(456, &beforeFull));
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE;
    CHECK(GiveMysteryEventTestMon(&original) == MEVENT_STATUS_FAILURE && NativeOwnedTrackerActive(456)
        && !NativeOwnedLedgerRevision(456, &afterFull) && beforeFull == afterFull);
    gPartiesCount[B_TRAINER_PLAYER] = 5;
    struct NativeOwnedLedgerSlot debugA = {NATIVE_OWNED_BOX, 2, 4}, debugB = {NATIVE_OWNED_BOX, 2, 5};
    CHECK(DebugStoreBoxMon(2, 4, &original.box) && DebugStoreBoxMon(2, 5, &original.box));
    struct NativeOwnedId debugIdA = GetLedgerSlot(debugA), debugIdB = GetLedgerSlot(debugB);
    CHECK(DifferentLedgerId(debugIdA, debugIdB) && DifferentLedgerId(debugIdA, parent)
        && !DebugStoreBoxMon(2, 4, &original.box) && NativeOwnedTrackerActive(456));
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveBegin() && !NativeSaveEnd(778, 1));
    CHECK(!NativeSaveRestoreMods(778)); CheckLedgerSlot(p0, parent); CheckLedgerSlot(p1, gift);
    CheckLedgerSlot(p2, split); CheckLedgerSlot(p3, egg); CheckLedgerSlot(day0, c); CheckLedgerSlot(fusion0, damaged);
    CheckLedgerSlot(p4, mystery); CheckLedgerSlot(p5, empty);
    CheckLedgerSlot(debugA, debugIdA); CheckLedgerSlot(debugB, debugIdB);
    CHECK(!NativeOwnedTrackerStart(456));
    /* If a debug write occurs while capture is unsafe, do not retain a
     * ledger that silently lacks the new individual. */
    gMain.inBattle = TRUE;
    CHECK(DebugStoreBoxMon(2, 6, &original.box) && !NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle;
    memset(&gPokemonStoragePtr->boxes[2][6], 0, sizeof(struct BoxPokemon));
    CHECK(!NativeSaveRestoreMods(778) && !NativeOwnedTrackerStart(456));
    CHECK(ModsOwnedFusionTestStep(RHH_FUSION_STORE, 0, 1, 0, SPECIES_COMBUSKEN));
    CHECK(!NativeOwnedTrackerActive(456)); CheckLedgerSlot(p0, empty); CheckLedgerSlot(day0, empty);
    /* A sparse party can have its temporary last slot occupied. Preserve the
     * engine's overwrite behavior while retiring that individual's ID. */
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_PLAYER][5] = original;
    gPartiesCount[B_TRAINER_PLAYER] = 2; RhhModsSetRunContext(123, 457, 0);
    CHECK(!NativeOwnedLedgerStart(457) && !NativeOwnedTrackerSeed(457) && !NativeOwnedTrackerStart(457));
    struct NativeOwnedId keep, replaced, received, tail;
    CHECK(!NativeOwnedLedgerRead(457, p0, &keep) && !NativeOwnedLedgerRead(457, p5, &replaced));
    CHECK(GiveMysteryEventTestMon(&original) == MEVENT_STATUS_SUCCESS && NativeOwnedTrackerActive(457)
        && gPartiesCount[B_TRAINER_PLAYER] == 2);
    CHECK(!NativeOwnedLedgerRead(457, p1, &received) && !NativeOwnedLedgerRead(457, p5, &tail)
        && DifferentLedgerId(received, replaced) && DifferentLedgerId(received, keep) && !tail.high && !tail.low);
    uint64_t revision; struct NativeOwnedId expected[] = {keep, received};
    CHECK(!NativeOwnedLedgerRevision(457, &revision) && !NativeOwnedLedgerMatches(457, revision, expected, 2));
    gMain.inBattle = TRUE;
    CHECK(GiveMysteryEventTestMon(&original) == MEVENT_STATUS_SUCCESS && !NativeOwnedTrackerActive(457));
    gMain.inBattle = oldBattle;
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned world ledger: seed, reorder, daycare, fusion, gift and evolution passed");
}
static void CheckOwnedUnionLedger(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    for (u32 i = 0; i < 3; ++i)
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][i], SPECIES_TORCHIC, 20, 100 + i, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 3;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    struct NativeOwnedId ids[3];
    for (u32 i = 0; i < 3; ++i) ids[i] = GetLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i});
    SavePlayerParty(); CHECK(!NativeSaveBegin() && !NativeSaveEnd(794, TRUE));
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 31));
    CHECK(mons && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_31_SIZE);
    const u8 order[] = {3, 1}; struct Pokemon selected[] = {gParties[B_TRAINER_PLAYER][2], gParties[B_TRAINER_PLAYER][0]};
    gSelectedOrderFromParty[0] = order[0]; gSelectedOrderFromParty[1] = order[1];
    PrepareUnionRoomBattleParty(); CalculatePlayerPartyCount();
    CHECK(mons->party_view() == RHH_OWNED_PARTY_UNION_BATTLE && NativeOwnedTrackerActive(456));
    struct RhhOwnedMonSnapshot unavailable = {.struct_size = sizeof(unavailable), .identity = 99}, unchanged = unavailable;
    CHECK(mons->party_at(0, &unavailable) && !memcmp(&unavailable, &unchanged, sizeof(unavailable)));
    CHECK(!memcmp(gParties[B_TRAINER_PLAYER], selected, sizeof(selected)) && gPartiesCount[B_TRAINER_PLAYER] == 2);
    u32 field = 99;
    CHECK(!mons->union_party_field_slot(0, &field) && field == 2);
    CHECK(!mons->union_party_field_slot(1, &field) && field == 0);
    field = 99; CHECK(mons->union_party_field_slot(2, &field) && field == 99);
    CHECK(NativeSaveBegin() && NativeOwnedTrackerActive(456));
    bool8 oldBattle = gMain.inBattle; u32 oldFlags = gBattleTypeFlags;
    struct PartyMenu oldMenu = gPartyMenu; u8 oldOrder[sizeof(gBattlePartyCurrentOrder)];
    memcpy(oldOrder, gBattlePartyCurrentOrder, sizeof(oldOrder));
    gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_LINK | BATTLE_TYPE_TRAINER;
    gPartyMenu.layout = PARTY_LAYOUT_SINGLE;
    gBattlePartyCurrentOrder[0] = 0x01; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(!mons->union_party_field_slot(0, &field) && field == 2);
    CHECK(!mons->battle_view_field_slot(1, &field) && field == 0);
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1) && NativeOwnedTrackerActive(456));
    CHECK(!mons->union_party_field_slot(0, &field) && field == 0);
    CHECK(!mons->battle_view_field_slot(1, &field) && field == 2);
    field = 99; CHECK(mons->union_party_field_slot(2, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    CHECK(!mons->union_party_field_slot(0, &field) && field == 2);
    CHECK(!mons->union_party_field_slot(1, &field) && field == 0);
    CHECK(NativeSaveBegin() && NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gPartyMenu = oldMenu;
    memcpy(gBattlePartyCurrentOrder, oldOrder, sizeof(oldOrder));
    ModsOwnedUnionPartyBeforeRestore(); LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_FIELD_ORDER);
    for (u32 i = 0; i < 3; ++i) CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, ids[i]);
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveBegin() && !NativeSaveEnd(795, TRUE));
    SavePlayerParty(); PrepareUnionRoomBattleParty(); CalculatePlayerPartyCount();
    CHECK(!NativeOwnedTrackerActive(456) && !mons->union_party_field_slot(0, &field) && field == 2
        && !mons->union_party_field_slot(1, &field) && field == 0);
    ModsOwnedUnionPartyBeforeRestore(); LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    CHECK(!NativeSaveRestoreMods(795) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedUnionPartyBegin(order, ARRAY_COUNT(order));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memcpy(gParties[B_TRAINER_PLAYER], selected, sizeof(selected)); gPartiesCount[B_TRAINER_PLAYER] = 2;
    ModsOwnedUnionPartyPrepared(); CHECK(NativeOwnedTrackerActive(456));
    gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_LINK | BATTLE_TYPE_DOUBLE;
    gPartyMenu.layout = PARTY_LAYOUT_SINGLE;
    gBattlePartyCurrentOrder[0] = 0x01; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->union_party_field_slot(0, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gPartyMenu = oldMenu;
    memcpy(gBattlePartyCurrentOrder, oldOrder, sizeof(oldOrder));
    ModsOwnedUnionPartyBeforeRestore(); LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    CHECK(NativeOwnedTrackerActive(456));
    /* Detect byte changes hidden by matching OT/personality, including a
     * change after restore authorization and before the actual load. */
    for (u32 mode = 0; mode < 4; ++mode)
    {
        CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveRestoreMods(795) && !NativeOwnedTrackerStart(456));
        SavePlayerParty(); ModsOwnedUnionPartyBegin(order, ARRAY_COUNT(order));
        memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
        memcpy(gParties[B_TRAINER_PLAYER], selected, sizeof(selected)); gPartiesCount[B_TRAINER_PLAYER] = 2;
        ModsOwnedUnionPartyPrepared(); CHECK(NativeOwnedTrackerActive(456));
        u32 changedSlot = mode ? 1 : 0; /* slot 1 was not selected */
        if (mode == 2) { ModsOwnedUnionPartyBeforeRestore(); CHECK(NativeOwnedTrackerActive(456)); }
        GetSavedPlayerPartyMon(changedSlot)->hp ^= 1;
        if (mode == 2) { LoadPlayerParty(); CHECK(!NativeOwnedTrackerActive(456)); }
        else if (mode == 3) { SavePlayerPartyMon(2, &gParties[B_TRAINER_PLAYER][0]); CHECK(!NativeOwnedTrackerActive(456)); }
        else { ModsOwnedUnionPartyBeforeRestore(); CHECK(!NativeOwnedTrackerActive(456)); }
        GetSavedPlayerPartyMon(changedSlot)->hp ^= 1;
        if (mode != 2) LoadPlayerParty();
        else gParties[B_TRAINER_PLAYER][changedSlot] = *GetSavedPlayerPartyMon(changedSlot);
        ModsOwnedUnionPartyAfterRestore();
        CHECK(!NativeSaveRestoreMods(795) && !NativeOwnedTrackerStart(456));
    }
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveRestoreMods(795) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedUnionPartyBegin(order, ARRAY_COUNT(order));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memcpy(gParties[B_TRAINER_PLAYER], selected, sizeof(selected)); gPartiesCount[B_TRAINER_PLAYER] = 2;
    ModsOwnedUnionPartyPrepared(); CHECK(NativeOwnedTrackerActive(456));
    GetSavedPlayerPartyMon(0)->box.otId ^= 1;
    field = 99; CHECK(mons->union_party_field_slot(1, &field) && field == 99);
    ModsOwnedUnionPartyBeforeRestore(); CHECK(!NativeOwnedTrackerActive(456));
    LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    gParties[B_TRAINER_PLAYER][0].box.otId ^= 1;
    CHECK(!NativeSaveRestoreMods(795) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); const u8 invalid[] = {1, 1}; ModsOwnedUnionPartyBegin(invalid, ARRAY_COUNT(invalid));
    CHECK(!NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_UNION_BATTLE);
    field = 99; CHECK(mons->union_party_field_slot(0, &field) && field == 99);
    LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    CHECK(mons->party_view() == RHH_OWNED_PARTY_FIELD_ORDER);
    CHECK(!NativeSaveRestoreMods(795) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedUnionPartyBegin(order, ARRAY_COUNT(order));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    gParties[B_TRAINER_PLAYER][0] = selected[1]; gParties[B_TRAINER_PLAYER][1] = selected[0];
    gPartiesCount[B_TRAINER_PLAYER] = 2; ModsOwnedUnionPartyPrepared(); CHECK(!NativeOwnedTrackerActive(456));
    LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    CHECK(!NativeSaveRestoreMods(795) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedUnionPartyBegin(order, ARRAY_COUNT(order));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memcpy(gParties[B_TRAINER_PLAYER], selected, sizeof(selected)); gPartiesCount[B_TRAINER_PLAYER] = 2;
    gParties[B_TRAINER_PLAYER][0].hp ^= 1; /* Same OT/personality, different copy. */
    ModsOwnedUnionPartyPrepared(); CHECK(!NativeOwnedTrackerActive(456));
    LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    CHECK(!NativeSaveRestoreMods(795) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedUnionPartyBegin(order, ARRAY_COUNT(order));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memcpy(gParties[B_TRAINER_PLAYER], selected, sizeof(selected)); gPartiesCount[B_TRAINER_PLAYER] = 2;
    ModsOwnedUnionPartyPrepared(); CHECK(NativeOwnedTrackerActive(456));
    *GetSavedPlayerPartyCount() = 2;
    field = 99; CHECK(mons->union_party_field_slot(0, &field) && field == 99);
    ModsOwnedUnionPartyBeforeRestore(); CHECK(!NativeOwnedTrackerActive(456));
    *GetSavedPlayerPartyCount() = 3; LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    CHECK(!NativeSaveRestoreMods(795) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedUnionPartyBegin(order, ARRAY_COUNT(order));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memcpy(gParties[B_TRAINER_PLAYER], selected, sizeof(selected)); gPartiesCount[B_TRAINER_PLAYER] = 2;
    ModsOwnedUnionPartyPrepared(); CHECK(NativeOwnedTrackerActive(456));
    gPokemonStoragePtr->boxes[0][0] = selected[0].box;
    ModsOwnedUnionPartyBeforeRestore(); CHECK(!NativeOwnedTrackerActive(456));
    ZeroBoxMonData(&gPokemonStoragePtr->boxes[0][0]); LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    CHECK(!NativeSaveRestoreMods(795) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedUnionPartyBegin(order, ARRAY_COUNT(order));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memcpy(gParties[B_TRAINER_PLAYER], selected, sizeof(selected)); gPartiesCount[B_TRAINER_PLAYER] = 2;
    ModsOwnedUnionPartyPrepared(); gMain.inBattle = TRUE;
    NativeOwnedTrackerUnsupported(); CHECK(!NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned union ledger: selected party, restoration and fail-closed passed");
}
static void CheckOwnedLinkMultiLedger(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    for (u32 i = 0; i < 5; ++i)
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][i], SPECIES_TORCHIC, 20, 100 + i, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 5;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    struct NativeOwnedId ids[5];
    for (u32 i = 0; i < 5; ++i) ids[i] = GetLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i});
    SavePlayerParty(); CHECK(!NativeSaveBegin() && !NativeSaveEnd(796, TRUE));
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 32));
    CHECK(mons && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_32_SIZE);
    memset(gSelectedOrderFromParty, 0, sizeof(gSelectedOrderFromParty));
    gSelectedOrderFromParty[0] = 5; gSelectedOrderFromParty[1] = 2; gSelectedOrderFromParty[2] = 4;
    PrepareCableClubMultiBattleParty();
    CHECK(mons->party_view() == RHH_OWNED_PARTY_LINK_MULTI_BATTLE && NativeOwnedTrackerActive(456));
    u32 field = 99;
    CHECK(!mons->link_multi_party_field_slot(0, &field) && field == 4);
    CHECK(!mons->link_multi_party_field_slot(1, &field) && field == 1);
    CHECK(!mons->link_multi_party_field_slot(2, &field) && field == 3);
    field = 99; CHECK(mons->link_multi_party_field_slot(3, &field) && field == 99);
    CHECK(mons->union_party_field_slot(0, &field) && field == 99);
    CHECK(NativeSaveBegin() && NativeOwnedTrackerActive(456));
    struct Pokemon partner = gParties[B_TRAINER_PLAYER][0];
    for (u32 i = 0; i < MULTI_PARTY_SIZE; ++i) gParties[B_TRAINER_PARTNER][i] = partner;
    struct BattleStruct battle = {0}; struct BattleStruct *oldBattleStruct = gBattleStruct;
    bool8 oldBattle = gMain.inBattle; u32 oldFlags = gBattleTypeFlags; u8 oldBattler = gBattlerInMenuId;
    struct PartyMenu oldMenu = gPartyMenu; u8 oldOrder[sizeof(gBattlePartyCurrentOrder)];
    memcpy(oldOrder, gBattlePartyCurrentOrder, sizeof(oldOrder));
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattlerInMenuId = 0;
    gBattleTypeFlags = BATTLE_TYPE_LINK | BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI;
    gPartyMenu.layout = PARTY_LAYOUT_MULTI;
    gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(!mons->link_multi_party_field_slot(0, &field) && field == 4);
    CHECK(!mons->battle_view_field_slot(2, &field) && field == 1);
    CHECK(!mons->link_multi_party_field_slot(3, &field) && field == 3);
    field = 99; CHECK(mons->link_multi_party_field_slot(1, &field) && field == 99);
    CHECK(mons->link_multi_party_field_slot(4, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSummary(TRUE));
    CHECK(!mons->link_multi_party_field_slot(2, &field) && field == 1);
    field = 99; CHECK(mons->link_multi_party_field_slot(1, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSummary(FALSE));
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 2) && NativeOwnedTrackerActive(456));
    CHECK(!mons->link_multi_party_field_slot(0, &field) && field == 1);
    CHECK(!mons->link_multi_party_field_slot(2, &field) && field == 4);
    CHECK(ModsOwnedPartyBattleTestSwitch(1, 4));
    field = 99; CHECK(mons->link_multi_party_field_slot(1, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gBattlerInMenuId = BIT_FLANK;
    gBattlePartyCurrentOrder[0] = 0x30; gBattlePartyCurrentOrder[1] = 0x45; gBattlePartyCurrentOrder[2] = 0x12;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(!mons->link_multi_party_field_slot(0, &field) && field == 4);
    CHECK(!mons->link_multi_party_field_slot(2, &field) && field == 1);
    CHECK(!mons->link_multi_party_field_slot(3, &field) && field == 3);
    field = 99; CHECK(mons->link_multi_party_field_slot(1, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 2) && NativeOwnedTrackerActive(456));
    CHECK(!mons->link_multi_party_field_slot(0, &field) && field == 1);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gBattlerInMenuId = 0;
    gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1) && NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->link_multi_party_field_slot(0, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = oldBattleStruct;
    gBattlerInMenuId = oldBattler; gPartyMenu = oldMenu; memcpy(gBattlePartyCurrentOrder, oldOrder, sizeof(oldOrder));
    ModsOwnedUnionPartyBeforeRestore(); LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_FIELD_ORDER);
    for (u32 i = 0; i < 5; ++i) CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, ids[i]);
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveBegin() && !NativeSaveEnd(797, TRUE));
    SavePlayerParty(); PrepareCableClubMultiBattleParty();
    CHECK(!mons->link_multi_party_field_slot(0, &field) && field == 4);
    ModsOwnedUnionPartyBeforeRestore(); LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    CHECK(!NativeSaveRestoreMods(797) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ReducePlayerPartyToSelectedMons();
    CHECK(!NativeOwnedTrackerActive(456));
    LoadPlayerParty();
    CHECK(!NativeSaveRestoreMods(797) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); gSelectedOrderFromParty[1] = 0;
    PrepareCableClubMultiBattleParty(); CHECK(!NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->link_multi_party_field_slot(0, &field) && field == 99);
    LoadPlayerParty(); ModsOwnedUnionPartyAfterRestore();
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned link-multi ledger: selected party, mixed menu and restoration passed");
}
static void ModsOwnedFrontierTestPrepare(u32 count)
{
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_PARTY_ORDER;
    gSpecialVar_0x8005 = count;
    CallFrontierUtilFunc();
}
static void ModsOwnedFrontierTestSaveSelected(void)
{
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SAVE_PARTY;
    CallFrontierUtilFunc();
}
static void CheckOwnedFrontierLedger(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    for (u32 i = 0; i < 5; ++i)
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][i], SPECIES_TORCHIC, 20, 100 + i, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][4] = gParties[B_TRAINER_PLAYER][0];
    gPartiesCount[B_TRAINER_PLAYER] = 5;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    struct NativeOwnedId ids[5];
    for (u32 i = 0; i < 5; ++i) ids[i] = GetLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i});
    CHECK(DifferentLedgerId(ids[0], ids[4]));
    gFlashMemoryPresent = TRUE; Save_ResetSaveCounters();
    CHECK(!HandleSavingData(SAVE_NORMAL) && !gDamagedSaveSectors);
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 33));
    CHECK(mons && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_33_SIZE);
    memset(gSaveBlock2Ptr->frontier.selectedPartyMons, 0, sizeof(gSaveBlock2Ptr->frontier.selectedPartyMons));
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 5;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 2;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 4;
    ModsOwnedFrontierTestPrepare(3);
    CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_FRONTIER);
    u32 field = 99;
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 4);
    CHECK(!mons->frontier_party_field_slot(1, &field) && field == 1);
    CHECK(!mons->frontier_party_field_slot(2, &field) && field == 3);
    field = 99; CHECK(mons->frontier_party_field_slot(3, &field) && field == 99);
    CHECK(NativeSaveBegin() && NativeOwnedTrackerActive(456));
    CHECK(ModsOwnedPartyTestSwap(0, 2) && NativeOwnedTrackerActive(456));
    gParties[B_TRAINER_PLAYER][0].hp = 1;
    ModsOwnedFrontierTestSaveSelected();
    CHECK(NativeOwnedTrackerActive(456) && GetSavedPlayerPartyMon(3)->hp == 1);
    CHECK(gSaveBlock2Ptr->frontier.selectedPartyMons[0] == 4 && gSaveBlock2Ptr->frontier.selectedPartyMons[1] == 2
        && gSaveBlock2Ptr->frontier.selectedPartyMons[2] == 5);
    CHECK(ModsOwnedPartyTestSwap(0, 2) && NativeOwnedTrackerActive(456));
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_POUND, 0);
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_RESET_SKETCH_MOVES; CallFrontierUtilFunc();
    CHECK(NativeOwnedTrackerActive(456) && GetMonData(GetSavedPlayerPartyMon(4), MON_DATA_MOVE1) == MOVE_SKETCH);
    CHECK(gSaveBlock2Ptr->frontier.selectedPartyMons[0] == 5 && gSaveBlock2Ptr->frontier.selectedPartyMons[2] == 4);
    struct BattleStruct battle = {0}; struct BattleStruct *oldBattleStruct = gBattleStruct;
    bool8 oldBattle = gMain.inBattle; u32 oldFlags = gBattleTypeFlags; u8 oldBattler = gBattlerInMenuId;
    struct PartyMenu oldMenu = gPartyMenu; u8 oldOrder[sizeof(gBattlePartyCurrentOrder)];
    memcpy(oldOrder, gBattlePartyCurrentOrder, sizeof(oldOrder));
    gBattleStruct = &battle; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_BATTLE_TOWER | BATTLE_TYPE_TRAINER;
    gPartyMenu.layout = PARTY_LAYOUT_SINGLE;
    gBattlePartyCurrentOrder[0] = 0x01; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1) && NativeOwnedTrackerActive(456));
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 1);
    CHECK(!mons->battle_view_field_slot(1, &field) && field == 4);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gBattleTypeFlags = BATTLE_TYPE_BATTLE_TOWER | BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE;
    gPartyMenu.layout = PARTY_LAYOUT_DOUBLE;
    gBattlePartyCurrentOrder[0] = 0x10; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 1);
    CHECK(!mons->frontier_party_field_slot(1, &field) && field == 4);
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1) && NativeOwnedTrackerActive(456));
    CHECK(!mons->battle_view_field_slot(0, &field) && field == 4);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = oldBattleStruct;
    gPartyMenu = oldMenu; memcpy(gBattlePartyCurrentOrder, oldOrder, sizeof(oldOrder));
    gParties[B_TRAINER_PLAYER][0].hp = 1;
    ModsOwnedFrontierTestSaveSelected(); CHECK(NativeOwnedTrackerActive(456) && GetSavedPlayerPartyMon(4)->hp == 1);
    gParties[B_TRAINER_PLAYER][0].hp = 2; /* A pending battle change survives the temporary save. */
    SaveGameFrontier();
    CHECK(gSaveAttemptStatus == SAVE_STATUS_OK && NativeOwnedTrackerActive(456) && gParties[B_TRAINER_PLAYER][0].hp == 2);
    CHECK(mons->party_view() == RHH_OWNED_PARTY_FRONTIER && !mons->frontier_party_field_slot(0, &field) && field == 4);
    LoadPlayerParty(); CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_FIELD_ORDER);
    CHECK(gParties[B_TRAINER_PLAYER][4].hp == 1);
    for (u32 i = 0; i < 5; ++i) CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, ids[i]);
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveRestoreMods(gSaveCounter) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedFrontierTestPrepare(3);
    struct Pokemon partner = gParties[B_TRAINER_PLAYER][0];
    for (u32 i = 0; i < MULTI_PARTY_SIZE; ++i) gParties[B_TRAINER_PARTNER][i] = partner;
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattlerInMenuId = 0;
    gBattleTypeFlags = BATTLE_TYPE_BATTLE_TOWER | BATTLE_TYPE_TRAINER | BATTLE_TYPE_MULTI | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_INGAME_PARTNER | BATTLE_TYPE_TWO_OPPONENTS;
    gPartyMenu.layout = PARTY_LAYOUT_MULTI;
    gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 4);
    CHECK(!mons->frontier_party_field_slot(2, &field) && field == 1);
    CHECK(!mons->frontier_party_field_slot(3, &field) && field == 3);
    field = 99; CHECK(mons->frontier_party_field_slot(1, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSummary(TRUE));
    CHECK(!mons->frontier_party_field_slot(2, &field) && field == 1);
    field = 99; CHECK(mons->frontier_party_field_slot(1, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSummary(FALSE));
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 2) && NativeOwnedTrackerActive(456));
    CHECK(!mons->battle_view_field_slot(0, &field) && field == 1);
    CHECK(ModsOwnedPartyBattleTestSwitch(1, 4) && NativeOwnedTrackerActive(456));
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gBattleTypeFlags = BATTLE_TYPE_BATTLE_TOWER | BATTLE_TYPE_TRAINER | BATTLE_TYPE_MULTI | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_LINK | BATTLE_TYPE_TOWER_LINK_MULTI;
    gBattlerInMenuId = BIT_FLANK;
    gBattlePartyCurrentOrder[0] = 0x30; gBattlePartyCurrentOrder[1] = 0x45; gBattlePartyCurrentOrder[2] = 0x12;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 4);
    CHECK(!mons->frontier_party_field_slot(2, &field) && field == 1);
    field = 99; CHECK(mons->frontier_party_field_slot(1, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 2) && NativeOwnedTrackerActive(456));
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 1);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gBattleTypeFlags = BATTLE_TYPE_BATTLE_TOWER | BATTLE_TYPE_TRAINER | BATTLE_TYPE_MULTI | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_INGAME_PARTNER;
    gPartyMenu.layout = PARTY_LAYOUT_MULTI_FULL;
    gBattlePartyCurrentOrder[0] = 0x10; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 1);
    CHECK(!mons->frontier_party_field_slot(1, &field) && field == 4);
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1) && NativeOwnedTrackerActive(456));
    CHECK(!mons->battle_view_field_slot(0, &field) && field == 4);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gPartyMenu.layout = PARTY_LAYOUT_MULTI_FULL_PARTNER;
    gBattlePartyCurrentOrder[0] = 0x10; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->frontier_party_field_slot(0, &field) && field == 99);
    CHECK(mons->battle_view_field_slot(1, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1) && NativeOwnedTrackerActive(456));
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gPartyMenu.layout = PARTY_LAYOUT_MULTI_FULL;
    gBattlePartyCurrentOrder[0] = 0x01; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 4);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = oldBattleStruct; gBattlerInMenuId = oldBattler;
    gPartyMenu = oldMenu; memcpy(gBattlePartyCurrentOrder, oldOrder, sizeof(oldOrder));
    LoadPlayerParty(); CHECK(NativeOwnedTrackerActive(456));
    for (u32 i = 0; i < 5; ++i) CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, ids[i]);
    CHECK(RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 34)) == &mons->header);
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattlerInMenuId = 0;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_MULTI | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_INGAME_PARTNER;
    gPartyMenu.layout = PARTY_LAYOUT_MULTI_FULL;
    gBattlePartyCurrentOrder[0] = 0x10; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(mons->party_view() == RHH_OWNED_PARTY_BATTLE_MENU);
    CHECK(!mons->battle_view_field_slot(0, &field) && field == 1);
    CHECK(!mons->battle_view_field_slot(1, &field) && field == 0);
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1) && NativeOwnedTrackerActive(456));
    CHECK(!mons->battle_view_field_slot(0, &field) && field == 0);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gPartyMenu.layout = PARTY_LAYOUT_MULTI_FULL_PARTNER;
    gBattlePartyCurrentOrder[0] = 0x10; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->battle_view_field_slot(0, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1) && NativeOwnedTrackerActive(456));
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = oldBattleStruct; gBattlerInMenuId = oldBattler;
    gPartyMenu = oldMenu; memcpy(gBattlePartyCurrentOrder, oldOrder, sizeof(oldOrder));
    for (u32 i = 0; i < 5; ++i) CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, ids[i]);
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 0;
    SavePlayerParty(); ModsOwnedFrontierTestPrepare(2);
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattlerInMenuId = 0;
    gBattleTypeFlags = BATTLE_TYPE_BATTLE_TOWER | BATTLE_TYPE_TRAINER | BATTLE_TYPE_MULTI | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_INGAME_PARTNER;
    gPartyMenu.layout = PARTY_LAYOUT_MULTI;
    gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 4);
    CHECK(!mons->frontier_party_field_slot(2, &field) && field == 1);
    field = 99; CHECK(mons->frontier_party_field_slot(3, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSummary(TRUE));
    field = 99; CHECK(mons->frontier_party_field_slot(1, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestSummary(FALSE));
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = oldBattleStruct; gBattlerInMenuId = oldBattler;
    gPartyMenu = oldMenu; memcpy(gBattlePartyCurrentOrder, oldOrder, sizeof(oldOrder));
    LoadPlayerParty(); CHECK(NativeOwnedTrackerActive(456));
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 4;
    SavePlayerParty(); ModsOwnedFrontierTestPrepare(3);
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattlerInMenuId = 0;
    gBattleTypeFlags = BATTLE_TYPE_BATTLE_TOWER | BATTLE_TYPE_TRAINER | BATTLE_TYPE_MULTI | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_INGAME_PARTNER;
    gPartyMenu.layout = PARTY_LAYOUT_MULTI;
    gBattlePartyCurrentOrder[0] = 0x03; gBattlePartyCurrentOrder[1] = 0x12; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && NativeOwnedTrackerActive(456));
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1) && !NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->frontier_party_field_slot(0, &field) && field == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = oldBattleStruct; gBattlerInMenuId = oldBattler;
    gPartyMenu = oldMenu; memcpy(gBattlePartyCurrentOrder, oldOrder, sizeof(oldOrder));
    LoadPlayerParty(); CHECK(!NativeSaveRestoreMods(gSaveCounter) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedFrontierTestPrepare(3);
    SavePlayerPartyMon(0, &gParties[B_TRAINER_PLAYER][0]);
    CHECK(!NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->frontier_party_field_slot(0, &field) && field == 99);
    LoadPlayerParty();
    CHECK(!NativeSaveRestoreMods(gSaveCounter) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedFrontierTestPrepare(3);
    const u8 sameOrder[] = {1, 2, 3}; ModsOwnedFrontierPartyReselect(sameOrder, 3, 3);
    gParties[B_TRAINER_PLAYER][0].hp ^= 1;
    ModsOwnedUnionPartyPrepared(); CHECK(!NativeOwnedTrackerActive(456));
    LoadPlayerParty();
    /* A resume receipt is bound to the captured bytes and exact slot mapping;
     * abandoning it cannot leave an apparently valid running tracker. */
    for (u32 mode = 0; mode < 4; ++mode)
    {
        CHECK(!NativeSaveRestoreMods(gSaveCounter) && !NativeOwnedTrackerStart(456));
        SavePlayerParty(); ModsOwnedFrontierTestPrepare(3); CHECK(NativeOwnedTrackerActive(456));
        u8 capturedOrder[MAX_FRONTIER_PARTY_SIZE] = {0};
        u32 capturedCount = ModsOwnedFrontierPartySelection(capturedOrder, ARRAY_COUNT(capturedOrder));
        struct Pokemon capturedParty[PARTY_SIZE]; memcpy(capturedParty, gParties[B_TRAINER_PLAYER], sizeof(capturedParty));
        CHECK(capturedCount == 3); NativeOwnedTrackFrontierCapture(capturedOrder, capturedCount, capturedParty);
        CHECK(NativeOwnedTrackerActive(456)); LoadPlayerParty(); CHECK(NativeOwnedTrackerActive(456));
        if (mode == 2) { CHECK(NativeOwnedTrackerStop(456) && !NativeOwnedTrackerActive(456)); continue; }
        ModsOwnedFrontierPartyBegin(capturedOrder, capturedCount); CHECK(NativeOwnedTrackerActive(456));
        if (mode == 3) {
            memcpy(gParties[B_TRAINER_PLAYER], capturedParty, sizeof(capturedParty)); gPartiesCount[B_TRAINER_PLAYER] = capturedCount;
            ModsOwnedUnionPartyPrepared(); CHECK(!NativeOwnedTrackerActive(456)); LoadPlayerParty(); continue;
        }
        if (!mode) capturedParty[0].hp ^= 1; else capturedOrder[0] = capturedOrder[1];
        NativeOwnedTrackFrontierResume(capturedOrder, capturedCount, capturedParty);
        CHECK(!NativeOwnedTrackerActive(456)); ModsOwnedUnionPartyAfterRestore();
    }
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned frontier ledger: selection, battle, save and restoration passed");
}
static void CheckOwnedDomeLedger(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    for (u32 i = 0; i < 5; ++i)
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][i], SPECIES_TORCHIC, 20, 100 + i, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][4] = gParties[B_TRAINER_PLAYER][0];
    gPartiesCount[B_TRAINER_PLAYER] = 5;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    struct NativeOwnedId ids[5];
    for (u32 i = 0; i < 5; ++i) ids[i] = GetLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i});
    CHECK(DifferentLedgerId(ids[0], ids[4]));
    gFlashMemoryPresent = TRUE; Save_ResetSaveCounters(); CHECK(!HandleSavingData(SAVE_NORMAL) && !gDamagedSaveSectors);
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 33));
    CHECK(mons);
    memset(gSaveBlock2Ptr->frontier.selectedPartyMons, 0, sizeof(gSaveBlock2Ptr->frontier.selectedPartyMons));
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 5;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 2;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 4;
    SavePlayerParty(); ModsOwnedFrontierTestPrepare(3);
    memset(gSelectedOrderFromParty, 0, sizeof(gSelectedOrderFromParty));
    gSelectedOrderFromParty[0] = 3; gSelectedOrderFromParty[1] = 1;
    gSpecialVar_0x8004 = BATTLE_DOME_FUNC_REDUCE_PARTY; CallBattleDomeFunction();
    CHECK(NativeOwnedTrackerActive(456) && mons->party_view() == RHH_OWNED_PARTY_FRONTIER);
    u32 field = 99;
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 3);
    CHECK(!mons->frontier_party_field_slot(1, &field) && field == 4);
    field = 99; CHECK(mons->frontier_party_field_slot(2, &field) && field == 99);
    ModsOwnedFrontierTestSaveSelected(); CHECK(NativeOwnedTrackerActive(456));
    CHECK(gSaveBlock2Ptr->frontier.selectedPartyMons[0] == 5 && gSaveBlock2Ptr->frontier.selectedPartyMons[1] == 2
        && gSaveBlock2Ptr->frontier.selectedPartyMons[2] == 4);
    struct BattleStruct battle = {0}; struct BattleStruct *oldBattleStruct = gBattleStruct;
    bool8 oldBattle = gMain.inBattle; u32 oldFlags = gBattleTypeFlags;
    struct PartyMenu oldMenu = gPartyMenu; u8 oldOrder[sizeof(gBattlePartyCurrentOrder)];
    memcpy(oldOrder, gBattlePartyCurrentOrder, sizeof(oldOrder));
    gBattleStruct = &battle; gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_DOME | BATTLE_TYPE_TRAINER;
    gPartyMenu.layout = PARTY_LAYOUT_SINGLE;
    gBattlePartyCurrentOrder[0] = 0x01; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && ModsOwnedPartyBattleTestSwitch(0, 1) && NativeOwnedTrackerActive(456));
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 4);
    CHECK(!mons->battle_view_field_slot(1, &field) && field == 3);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && NativeOwnedTrackerActive(456));
    gMain.inBattle = oldBattle; gBattleTypeFlags = oldFlags; gBattleStruct = oldBattleStruct;
    gPartyMenu = oldMenu; memcpy(gBattlePartyCurrentOrder, oldOrder, sizeof(oldOrder));
    gParties[B_TRAINER_PLAYER][0].hp = 1;
    gSpecialVar_0x8004 = BATTLE_DOME_FUNC_RESET_SKETCH; CallBattleDomeFunction();
    CHECK(NativeOwnedTrackerActive(456) && GetSavedPlayerPartyMon(3)->hp == 1);
    SaveGameFrontier(); CHECK(gSaveAttemptStatus == SAVE_STATUS_OK && NativeOwnedTrackerActive(456));
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 3);
    LoadPlayerParty(); CHECK(NativeOwnedTrackerActive(456) && gParties[B_TRAINER_PLAYER][3].hp == 1);
    for (u32 i = 0; i < 5; ++i) CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, ids[i]);
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveRestoreMods(gSaveCounter) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedFrontierTestPrepare(3);
    memset(gSelectedOrderFromParty, 0, sizeof(gSelectedOrderFromParty));
    gSelectedOrderFromParty[0] = 1; gSelectedOrderFromParty[1] = 1;
    gSpecialVar_0x8004 = BATTLE_DOME_FUNC_REDUCE_PARTY; CallBattleDomeFunction();
    CHECK(!NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->frontier_party_field_slot(0, &field) && field == 99);
    LoadPlayerParty(); NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned dome ledger: nested selection, battle, save and restore passed");
}
static void CheckOwnedPyramidLedger(const char *path)
{
    InitOwnedLedgerWithoutObservers(path);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    memset(gPokemonStoragePtr->fusions, 0, sizeof(gPokemonStoragePtr->fusions));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(gSaveBlock1Ptr->daycare));
    for (u32 i = 0; i < 5; ++i)
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][i], SPECIES_TORCHIC, 20, 100 + i, OTID_STRUCT_PRESET(54321), 31);
    gParties[B_TRAINER_PLAYER][4] = gParties[B_TRAINER_PLAYER][0];
    gPartiesCount[B_TRAINER_PLAYER] = 5;
    CHECK(!NativeOwnedLedgerStart(456) && !NativeOwnedTrackerSeed(456) && !NativeOwnedTrackerStart(456));
    struct NativeOwnedId ids[5];
    for (u32 i = 0; i < 5; ++i) ids[i] = GetLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i});
    CHECK(DifferentLedgerId(ids[0], ids[4]));
    gFlashMemoryPresent = TRUE; Save_ResetSaveCounters(); CHECK(!HandleSavingData(SAVE_NORMAL) && !gDamagedSaveSectors);
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 34));
    CHECK(mons);
    memset(gSaveBlock2Ptr->frontier.selectedPartyMons, 0, sizeof(gSaveBlock2Ptr->frontier.selectedPartyMons));
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 5;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 2;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 4;
    SavePlayerParty(); ModsOwnedFrontierTestPrepare(3);
    CHECK(ModsOwnedPartyTestSwap(0, 2) && NativeOwnedTrackerActive(456));
    u32 field = 99;
    CHECK(!mons->frontier_party_field_slot(0, &field) && field == 3);
    CHECK(!mons->frontier_party_field_slot(1, &field) && field == 1);
    CHECK(!mons->frontier_party_field_slot(2, &field) && field == 4);
    for (u32 i = 0; i < 3; ++i) gParties[B_TRAINER_PLAYER][i].hp = i + 1;
    gSpecialVar_0x8004 = BATTLE_PYRAMID_FUNC_RESTORE_PARTY; CallBattlePyramidFunction();
    CHECK(NativeOwnedTrackerActive(456));
    CHECK(GetSavedPlayerPartyMon(3)->hp == 1 && GetSavedPlayerPartyMon(1)->hp == 2 && GetSavedPlayerPartyMon(4)->hp == 3);
    CHECK(gSaveBlock2Ptr->frontier.selectedPartyMons[0] == 4 && gSaveBlock2Ptr->frontier.selectedPartyMons[1] == 2
        && gSaveBlock2Ptr->frontier.selectedPartyMons[2] == 5);
    LoadPlayerParty(); CHECK(NativeOwnedTrackerActive(456));
    CHECK(gParties[B_TRAINER_PLAYER][3].hp == 1 && gParties[B_TRAINER_PLAYER][1].hp == 2 && gParties[B_TRAINER_PLAYER][4].hp == 3);
    for (u32 i = 0; i < 5; ++i) CheckLedgerSlot((struct NativeOwnedLedgerSlot){NATIVE_OWNED_PARTY, 0, i}, ids[i]);
    CHECK(!HandleSavingData(SAVE_LINK) && !gDamagedSaveSectors && NativeOwnedTrackerActive(456));
    CHECK(!NativeOwnedTrackerStop(456) && !NativeSaveRestoreMods(gSaveCounter) && !NativeOwnedTrackerStart(456));
    SavePlayerParty(); ModsOwnedFrontierTestPrepare(3);
    CHECK(ModsOwnedPartyTestSwap(0, 4) && !NativeOwnedTrackerActive(456));
    field = 99; CHECK(mons->frontier_party_field_slot(0, &field) && field == 99);
    LoadPlayerParty(); NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned pyramid ledger: party swap, duplicate species, restore and save passed");
}
static void CheckOwnedPcLedger(const char *path)
{
    InitModsForTest(); CHECK(!NativeSaveOpen(path)); RhhModsSetRunContext(123, 456, 0);
    CHECK(!RhhModsHasHook(RHH_OWNED_MON_PICKED_UP) && !RhhModsHasHook(RHH_OWNED_MON_MOVED)
        && !RhhModsHasHook(RHH_OWNED_MON_RELEASED) && !RhhModsHasHook(RHH_OWNED_STORAGE_UNREADABLE));
    CHECK(!NativeOwnedLedgerStart(456));
    struct NativeOwnedLedgerSlot party0 = {NATIVE_OWNED_PARTY, 0, 0}, box01 = {NATIVE_OWNED_BOX, 0, 1};
    struct NativeOwnedLedgerSlot box00 = {NATIVE_OWNED_BOX, 0, 0}, box12 = {NATIVE_OWNED_BOX, 1, 2};
    struct NativeOwnedLedgerSlot box16 = {NATIVE_OWNED_BOX, 1, 6}, box17 = {NATIVE_OWNED_BOX, 1, 7};
    struct NativeOwnedLedgerSlot cursor = {NATIVE_OWNED_CURSOR, 0, 0};
    struct NativeOwnedLedgerSlot box03 = {NATIVE_OWNED_BOX, 0, 3};
    struct NativeOwnedId a, b, c, d, e, empty = {0, 0};
    struct Pokemon original;
    CreateMonWithIVs(&original, SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    gParties[B_TRAINER_PLAYER][0] = original; gPartiesCount[B_TRAINER_PLAYER] = 1;
    gPokemonStoragePtr->boxes[1][2] = original.box;
    CHECK(!NativeOwnedTrackerSeed(456));
    a = GetLedgerSlot(party0); b = GetLedgerSlot(box12);
    CHECK((a.high != b.high || a.low != b.low) && !NativeOwnedTrackerStart(456));
    MainCallback previous = gMain.callback2; CHECK(ModsOwnedStorageTestOpen());
    CHECK(ModsOwnedStorageTestPick(RHH_MON_PARTY, 0, 0)); CheckLedgerSlot(party0, empty); CheckLedgerSlot(cursor, a);
    CHECK(NativeSaveBegin() && !NativeSaveTransactionActive());
    gParties[B_TRAINER_PLAYER][0] = original;
    uint64_t revision; CHECK(!NativeOwnedLedgerRevision(456, &revision));
    CHECK(!NativeOwnedLedgerMint(456, revision, party0, &c));
    CHECK(!NativeOwnedTrackerTestRemember(party0));
    CHECK(ModsOwnedStorageTestPlace(RHH_MON_BOX, 0, 1, FALSE)); CheckLedgerSlot(box01, a); CheckLedgerSlot(cursor, empty);
    CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 0, 1) && ModsOwnedStorageTestPlace(RHH_MON_BOX, 1, 2, TRUE));
    CheckLedgerSlot(box12, a); CheckLedgerSlot(cursor, b);
    CHECK(ModsOwnedStorageTestPlace(RHH_MON_BOX, 0, 1, FALSE)); CheckLedgerSlot(box01, b); CheckLedgerSlot(cursor, empty);
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_PARTY, 0, 0, FALSE)); ModsOwnedStorageTestRelease();
    CheckLedgerSlot(party0, empty);
    gPokemonStoragePtr->boxes[0][0] = original.box;
    CHECK(!NativeOwnedLedgerRevision(456, &revision) && !NativeOwnedLedgerMint(456, revision, box00, &d));
    CHECK(!NativeOwnedTrackerTestRemember(box00));
    CHECK(ModsOwnedStorageTestGroupMove(0, 1)); CheckLedgerSlot(box16, d); CheckLedgerSlot(box17, b);
    CheckLedgerSlot(box00, empty); CheckLedgerSlot(box01, empty);
    gPokemonStoragePtr->boxes[0][3] = original.box; gPokemonStoragePtr->boxes[0][3].secure.raw[0] ^= 1;
    CHECK(!NativeOwnedLedgerRevision(456, &revision) && !NativeOwnedLedgerMint(456, revision, box03, &e));
    CHECK(!NativeOwnedTrackerTestRemember(box03));
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_BOX, 0, 3, TRUE)); CheckLedgerSlot(cursor, e);
    ModsOwnedStorageTestRelease(); CheckLedgerSlot(cursor, empty); CHECK(NativeOwnedTrackerActive(456));
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_BOX, 1, 2, TRUE)); ModsOwnedStorageTestRelease();
    CheckLedgerSlot(box12, empty); CheckLedgerSlot(cursor, empty);
    CHECK(!NativeOwnedTrackerStop(456)); ModsOwnedStorageTestClose(); SetMainCallback2(previous);
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(777, 1));
    CHECK(!NativeSaveRestoreMods(777)); CheckLedgerSlot(box16, d); CheckLedgerSlot(box17, b);
    CHECK(!NativeOwnedTrackerStart(456) && ModsOwnedStorageTestOpen());
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_BOX, 1, 6, FALSE));
    CHECK(!NativeSaveBegin()); CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 1, 6));
    CHECK(!NativeOwnedTrackerActive(456) && NativeSaveEnd(778, 1));
    CheckLedgerSlot(box16, empty); ModsOwnedStorageTestRelease(); ModsOwnedStorageTestClose(); SetMainCallback2(previous);
    CHECK(!NativeSaveRestoreMods(777)); CheckLedgerSlot(box16, d); CheckLedgerSlot(box17, b);
    gPokemonStoragePtr->boxes[1][6] = original.box;
    ZeroBoxMonData(&gPokemonStoragePtr->boxes[0][3]);
    CHECK(!NativeOwnedTrackerStart(456) && ModsOwnedStorageTestOpen());
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_BOX, 1, 6, FALSE));
    CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 1, 6));
    gPokemonStoragePtr->boxes[0][3] = original.box; /* Occupied without a ledger binding. */
    CHECK(ModsOwnedStorageTestPlace(RHH_MON_BOX, 0, 3, TRUE));
    CHECK(!NativeOwnedTrackerActive(456)); CheckLedgerSlot(box16, empty); CheckLedgerSlot(box17, empty);
    ModsOwnedStorageTestRelease(); ModsOwnedStorageTestClose(); SetMainCallback2(previous);
    CHECK(!NativeSaveRestoreMods(777)); CheckLedgerSlot(box16, d); CheckLedgerSlot(box17, b);
    gPokemonStoragePtr->boxes[1][6] = original.box;
    ZeroBoxMonData(&gPokemonStoragePtr->boxes[0][3]);
    CHECK(!NativeOwnedTrackerStart(456) && ModsOwnedStorageTestOpen());
    gPokemonStoragePtr->boxes[0][3] = original.box; /* No corresponding ledger binding. */
    CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 0, 3));
    CHECK(!NativeOwnedTrackerActive(456)); CheckLedgerSlot(box16, empty); CheckLedgerSlot(box17, empty);
    ModsOwnedStorageTestClose(); SetMainCallback2(previous);
    /* A corrupt secure payload makes the public snapshot unreadable, but the
     * raw OT/personality witness can still detect a substituted Pokémon. */
    CHECK(!NativeSaveRestoreMods(777));
    gPokemonStoragePtr->boxes[1][6] = original.box;
    gPokemonStoragePtr->boxes[1][7] = original.box;
    ZeroBoxMonData(&gPokemonStoragePtr->boxes[0][3]);
    CHECK(!NativeOwnedTrackerStart(456) && ModsOwnedStorageTestOpen());
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_BOX, 1, 6, FALSE));
    gPokemonStoragePtr->boxes[1][6].otId ^= 1;
    CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 1, 6));
    CHECK(!NativeOwnedTrackerActive(456));
    ModsOwnedStorageTestClose(); SetMainCallback2(previous);

    CHECK(!NativeSaveRestoreMods(777));
    gPokemonStoragePtr->boxes[1][6] = original.box;
    gPokemonStoragePtr->boxes[1][7] = original.box;
    CHECK(!NativeOwnedTrackerStart(456) && ModsOwnedStorageTestOpen());
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_BOX, 1, 6, FALSE));
    CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 1, 6));
    gPokemonStoragePtr->boxes[1][7].otId ^= 1;
    CHECK(ModsOwnedStorageTestPlace(RHH_MON_BOX, 1, 7, TRUE));
    CHECK(!NativeOwnedTrackerActive(456));
    ModsOwnedStorageTestClose(); SetMainCallback2(previous);

    CHECK(!NativeSaveRestoreMods(777));
    gPokemonStoragePtr->boxes[1][6] = original.box;
    gPokemonStoragePtr->boxes[1][7] = original.box;
    CHECK(!NativeOwnedTrackerStart(456) && ModsOwnedStorageTestOpen());
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_BOX, 1, 6, FALSE));
    gPokemonStoragePtr->boxes[1][6].otId ^= 1;
    ModsOwnedStorageTestRelease(); CHECK(!NativeOwnedTrackerActive(456));
    ModsOwnedStorageTestClose(); SetMainCallback2(previous);
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned PC ledger: pickup, reused slots, shift, group, release and fail-closed passed");
}
#endif
static void CheckOwnedIdentity(const char *path, const char *module)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path));
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 19));
    CHECK(mons && mons->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_19_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 3)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 4)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 5)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 6)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 7)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 8)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 9)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 10)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 11)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 12)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 13)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 14)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 15)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 16)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 17)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 18)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 20)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 21)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 22)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 23)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 24)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 25)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 26)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 27)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 28)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 29)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 30)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 31)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 32)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 33)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 34)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 35)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 36)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 37)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 39)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 40)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 41)) == &mons->header
        && RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 42)) == &mons->header
        && !RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 47))
        && RHH_OWNED_MON_SERVICE_V1_41_SIZE < RHH_OWNED_MON_SERVICE_V1_42_SIZE);
    struct RhhOwnedRelease outside = {.struct_size = sizeof(outside), .source_slot = 999}, oldOutside = outside;
    CHECK(mons->release_context(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    CHECK(mons->daycare_count() == (IS_FRLG ? 2 : 1) && mons->daycare_slots(0) == DAYCARE_MON_COUNT
        && mons->daycare_slots(1) == (IS_FRLG ? 1 : 0) && !mons->daycare_slots(99)
        && mons->fusion_slots() == MAX_FUSION_STORAGE);
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query), .matches = 99}, old = query;
    struct RhhOwnedIdentityOccurrences occurrences = {.struct_size = sizeof(occurrences)}, previousOccurrences;
    struct RhhOwnedIdentityOccurrences smallOccurrences = {.struct_size = 1, .matches = 99}, oldSmallOccurrences = smallOccurrences;
    CHECK(!mons->identity_occurrences(1, &occurrences) && occurrences.identity == 1
        && !occurrences.matches && !occurrences.count && !occurrences.flags
        && mons->identity_occurrences(1, &smallOccurrences) && !memcmp(&smallOccurrences, &oldSmallOccurrences, sizeof(smallOccurrences)));
    CHECK(!mons->identity_query(1, &query) && !query.matches && !query.unique_valid);
    RhhModsSetRunContext(123, 456, 0);
    struct Pokemon party[PARTY_SIZE]; memcpy(party, gParties[B_TRAINER_PLAYER], sizeof(party));
    u8 partyCount = gPartiesCount[B_TRAINER_PLAYER];
    struct BoxPokemon boxed = gPokemonStoragePtr->boxes[0][0];
    u8 currentBox = gPokemonStoragePtr->currentBox;
    struct Pokemon fusion = gPokemonStoragePtr->fusions[0]; struct DayCare daycare = gSaveBlock1Ptr->daycare;
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party)); memset(&gPokemonStoragePtr->boxes[0][0], 0, sizeof(boxed));
    memset(&gPokemonStoragePtr->fusions[0], 0, sizeof(fusion)); memset(&gSaveBlock1Ptr->daycare, 0, sizeof(daycare));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)}; CHECK(!mons->party_at(0, &mon));
    rng_value_t rng = gRngValue; struct Pokemon original = gParties[B_TRAINER_PLAYER][0];
    CHECK(!mons->identity_query(mon.identity, &query) && query.matches == 1 && query.unique_valid
        && query.party_matches == 1 && query.unique.location == RHH_MON_PARTY && query.unique.identity == mon.identity
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&original, &gParties[B_TRAINER_PLAYER][0], sizeof(original)));
    CHECK(!mons->identity_occurrences(mon.identity, &occurrences) && occurrences.matches == 1 && occurrences.count == 1
        && occurrences.entries[0].location == RHH_MON_PARTY && occurrences.entries[0].slot == 0
        && occurrences.entries[0].species == mon.species && !occurrences.entries[0].flags);
    previousOccurrences = occurrences;
    old = query; CHECK(!NativeSaveBegin() && NativeSaveTransactionActive());
    CHECK(mons->identity_query(mon.identity, &query) && !memcmp(&query, &old, sizeof(query)));
    CHECK(mons->identity_occurrences(mon.identity, &occurrences) && !memcmp(&occurrences, &previousOccurrences, sizeof(occurrences)));
    struct RhhOwnedMonSnapshot view = mon, savedView = view;
    CHECK(mons->daycare_at(0, 0, &view) && !memcmp(&view, &savedView, sizeof(view)));
    CHECK(mons->fusion_at(0, &view) && !memcmp(&view, &savedView, sizeof(view)));
    CHECK(NativeSaveEnd(1, FALSE) && !NativeSaveTransactionActive());
#ifdef RHH_OWNED_IDENTITY_TEST
    MainCallback previousCallback = gMain.callback2;
    CHECK(ModsOwnedStorageTestOpen()); old = query;
    CHECK(mons->identity_query(mon.identity, &query) && !memcmp(&query, &old, sizeof(query)));
    CHECK(mons->identity_occurrences(mon.identity, &occurrences) && !memcmp(&occurrences, &previousOccurrences, sizeof(occurrences)));
    CHECK(mons->daycare_at(0, 0, &view) && !memcmp(&view, &savedView, sizeof(view)));
    CHECK(mons->fusion_at(0, &view) && !memcmp(&view, &savedView, sizeof(view)));
    ModsOwnedStorageTestClose(); SetMainCallback2(previousCallback);
    CHECK(!mons->identity_query(mon.identity, &query) && query.unique_valid);
#endif
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1}; RhhModsDispatch(&event, &decision);
    struct RhhOwnedIdentityQuery small = {.struct_size = sizeof(small) - 1, .matches = 99}, oldSmall = small;
    CHECK(mons->identity_query(mon.identity, &small) && !memcmp(&small, &oldSmall, sizeof(small)));
    gPokemonStoragePtr->boxes[0][0] = original.box;
    gSaveBlock1Ptr->daycare.mons[0].mon = original.box;
    gPokemonStoragePtr->fusions[0] = original;
    CHECK(!mons->identity_query(mon.identity, &query) && query.matches == 4 && !query.unique_valid
        && query.party_matches == 1 && query.box_matches == 1 && query.daycare_matches == 1 && query.fusion_matches == 1);
    CHECK(!mons->identity_occurrences(mon.identity, &occurrences) && occurrences.identity == mon.identity
        && occurrences.matches == query.matches && occurrences.count == 4 && !occurrences.flags
        && occurrences.entries[0].location == RHH_MON_PARTY && occurrences.entries[1].location == RHH_MON_BOX
        && occurrences.entries[2].location == RHH_MON_DAYCARE && occurrences.entries[3].location == RHH_MON_FUSION_STORAGE);
    for (u32 i = 0; i < occurrences.count; ++i)
        CHECK(!occurrences.entries[i].flags && occurrences.entries[i].species == mon.species && !occurrences.entries[i].box && !occurrences.entries[i].slot);
    /* A full PC of identical records remains ordered and bounded in one census. */
    struct BoxPokemon (*savedBoxes)[IN_BOX_COUNT] = malloc(sizeof(gPokemonStoragePtr->boxes)); CHECK(savedBoxes);
    memcpy(savedBoxes, gPokemonStoragePtr->boxes, sizeof(gPokemonStoragePtr->boxes));
    for (u32 b = 0; b < TOTAL_BOXES_COUNT; ++b)
        for (u32 s = 0; s < IN_BOX_COUNT; ++s) gPokemonStoragePtr->boxes[b][s] = original.box;
    CHECK(!mons->identity_query(mon.identity, &query) && !mons->identity_occurrences(mon.identity, &occurrences)
        && occurrences.matches == query.matches && occurrences.matches == 3 + TOTAL_BOXES_COUNT * IN_BOX_COUNT
        && occurrences.count == occurrences.matches && !occurrences.flags);
    for (u32 b = 0; b < TOTAL_BOXES_COUNT; ++b)
        for (u32 s = 0; s < IN_BOX_COUNT; ++s)
        { const struct RhhOwnedIdentityOccurrence *entry = &occurrences.entries[1 + b * IN_BOX_COUNT + s];
          CHECK(entry->location == RHH_MON_BOX && entry->box == b && entry->slot == s && entry->species == mon.species); }
    memcpy(gPokemonStoragePtr->boxes, savedBoxes, sizeof(gPokemonStoragePtr->boxes)); free(savedBoxes);
    memset(&gPokemonStoragePtr->boxes[0][0], 0, sizeof(boxed));
    struct RhhOwnedMonSnapshot legacy = {.struct_size = sizeof(legacy)};
    CHECK(!mons->find(mon.identity, &legacy) && legacy.location == RHH_MON_PARTY);
    CHECK(!mons->identity_query(mon.identity, &query) && query.matches == 3 && !query.unique_valid);
    gPokemonStoragePtr->fusions[0].box.secure.raw[0] ^= 1;
    struct Pokemon corrupt = gPokemonStoragePtr->fusions[0];
    CHECK(!mons->identity_query(mon.identity, &query) && query.matches == 3 && !query.unique_valid
        && (query.flags & RHH_OWNED_IDENTITY_CORRUPT) && !memcmp(&corrupt, &gPokemonStoragePtr->fusions[0], sizeof(corrupt)));
    CHECK(!mons->identity_occurrences(mon.identity, &occurrences) && occurrences.matches == 3 && occurrences.count == 3
        && occurrences.entries[2].location == RHH_MON_FUSION_STORAGE
        && occurrences.entries[2].flags == RHH_OWNED_OCCURRENCE_UNREADABLE && !occurrences.entries[2].species
        && !memcmp(&corrupt, &gPokemonStoragePtr->fusions[0], sizeof(corrupt)));
    CHECK(mons->fusion_at(0, &view) && !memcmp(&view, &savedView, sizeof(view))
        && !memcmp(&corrupt, &gPokemonStoragePtr->fusions[0], sizeof(corrupt)));
    gPartiesCount[B_TRAINER_PLAYER] = 0; memset(&gSaveBlock1Ptr->daycare, 0, sizeof(daycare));
    CHECK(!mons->identity_query(mon.identity, &query) && query.matches == 1 && !query.unique_valid
        && (query.flags & RHH_OWNED_IDENTITY_CORRUPT));
    memset(&gPokemonStoragePtr->fusions[0], 0, sizeof(fusion)); gSaveBlock1Ptr->daycare.mons[0].mon = original.box;
    CHECK(!mons->identity_query(mon.identity, &query) && query.matches == 1 && query.unique_valid
        && query.unique.location == RHH_MON_DAYCARE && query.unique.slot == 0);
    CHECK(!mons->daycare_at(0, 0, &view) && view.identity == mon.identity
        && view.location == RHH_MON_DAYCARE && view.box == 0 && view.slot == 0);
    savedView = view;
    CHECK(mons->daycare_at(0, DAYCARE_MON_COUNT, &view) && !memcmp(&view, &savedView, sizeof(view)));
    CHECK(mons->daycare_at(99, 0, &view) && !memcmp(&view, &savedView, sizeof(view)));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(daycare)); gPokemonStoragePtr->fusions[0] = original;
    CHECK(!mons->identity_query(mon.identity, &query) && query.matches == 1 && query.unique_valid
        && query.unique.location == RHH_MON_FUSION_STORAGE && query.unique.slot == 0);
    CHECK(!mons->fusion_at(0, &view) && view.identity == mon.identity
        && view.location == RHH_MON_FUSION_STORAGE && view.slot == 0);
    savedView = view;
    CHECK(mons->fusion_at(MAX_FUSION_STORAGE, &view) && !memcmp(&view, &savedView, sizeof(view)));
    struct RhhOwnedMonSnapshot smallView = {.struct_size = sizeof(smallView) - 1}, oldSmallView = smallView;
    CHECK(mons->fusion_at(0, &smallView) && !memcmp(&smallView, &oldSmallView, sizeof(smallView)));
    struct PokemonStorage *storage = gPokemonStoragePtr; gPokemonStoragePtr = NULL; old = query;
    previousOccurrences = occurrences;
    CHECK(mons->identity_query(mon.identity, &query) && !memcmp(&query, &old, sizeof(query))
        && mons->identity_occurrences(mon.identity, &occurrences) && !memcmp(&occurrences, &previousOccurrences, sizeof(occurrences))); gPokemonStoragePtr = storage;
    struct SaveBlock1 *save = gSaveBlock1Ptr; gSaveBlock1Ptr = NULL;
    CHECK(mons->identity_query(mon.identity, &query) && !memcmp(&query, &old, sizeof(query))
        && mons->identity_occurrences(mon.identity, &occurrences) && !memcmp(&occurrences, &previousOccurrences, sizeof(occurrences))); gSaveBlock1Ptr = save;
    bool8 inBattle = gMain.inBattle; u32 battleFlags = gBattleTypeFlags;
    gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_FACTORY;
    CHECK(mons->identity_query(mon.identity, &query) && !memcmp(&query, &old, sizeof(query))
        && mons->identity_occurrences(mon.identity, &occurrences) && !memcmp(&occurrences, &previousOccurrences, sizeof(occurrences)));
    gMain.inBattle = inBattle; gBattleTypeFlags = battleFlags;
#ifdef RHH_OWNED_IDENTITY_TEST
    /* Exercise the real post-confirmation PC release path. Picking up a
     * Pokémon emits no release, even when its source is subsequently reused. */
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(daycare));
    memset(&gPokemonStoragePtr->fusions[0], 0, sizeof(fusion));
    gParties[B_TRAINER_PLAYER][0] = original; gPartiesCount[B_TRAINER_PLAYER] = 2;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_ZIGZAGOON, 20, 67890, OTID_STRUCT_PRESET(777), 31);
    gPokemonStoragePtr->boxes[0][0] = original.box;
    u8 *releaseState = RhhModsState("test:owned_identity"); CHECK(releaseState && ModsOwnedStorageTestOpen());
    releaseState[0] = 1;
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_PARTY, 0, 0, FALSE));
    ModsOwnedStorageTestRelease(); CHECK(releaseState[1] == 1 && !GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SANITY_HAS_SPECIES));
    releaseState[0] = 2;
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_BOX, 0, 0, FALSE));
    ModsOwnedStorageTestRelease(); CHECK(releaseState[1] == 2 && !GetBoxMonData(&gPokemonStoragePtr->boxes[0][0], MON_DATA_SANITY_HAS_SPECIES));
    gPokemonStoragePtr->boxes[0][0] = original.box; releaseState[0] = 3;
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_BOX, 0, 0, TRUE) && releaseState[1] == 2);
    gPokemonStoragePtr->boxes[0][0] = gParties[B_TRAINER_PLAYER][1].box;
    ModsOwnedStorageTestRelease(); CHECK(releaseState[1] == 3
        && GetBoxMonData(&gPokemonStoragePtr->boxes[0][0], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON);
    gParties[B_TRAINER_PLAYER][0] = original; releaseState[0] = 4;
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_PARTY, 0, 0, TRUE) && releaseState[1] == 3);
    gParties[B_TRAINER_PLAYER][0] = gParties[B_TRAINER_PLAYER][1];
    ModsOwnedStorageTestRelease(); CHECK(releaseState[1] == 4
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_ZIGZAGOON);
    struct RhhOwnedMove outsideMove = {.struct_size = sizeof(outsideMove), .source_slot = 999}, savedMove = outsideMove;
    CHECK(mons->move_context(&outsideMove) && !memcmp(&outsideMove, &savedMove, sizeof(outsideMove)));
    struct Pokemon clone = original; u8 hpIv = 27; SetMonData(&clone, MON_DATA_HP_IV, &hpIv);
    gParties[B_TRAINER_PLAYER][0] = original; gPartiesCount[B_TRAINER_PLAYER] = 2;
    memset(&gPokemonStoragePtr->boxes[0][1], 0, sizeof(struct BoxPokemon));
    memset(&gPokemonStoragePtr->boxes[1][2], 0, sizeof(struct BoxPokemon));
    memset(&gPokemonStoragePtr->boxes[1][6], 0, 2 * sizeof(struct BoxPokemon));
    releaseState[2] = 1; CHECK(ModsOwnedStorageTestPick(RHH_MON_PARTY, 0, 0) && !releaseState[3]);
    gParties[B_TRAINER_PLAYER][0] = clone; /* The original source slot has been reused. */
    CHECK(ModsOwnedStorageTestPlace(RHH_MON_BOX, 0, 1, FALSE) && releaseState[3] == 1);
    gPokemonStoragePtr->boxes[1][2] = clone.box; releaseState[2] = 2;
    CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 0, 1) && releaseState[3] == 1);
    CHECK(ModsOwnedStorageTestPlace(RHH_MON_BOX, 1, 2, TRUE) && releaseState[3] == 2);
    releaseState[2] = 3;
    CHECK(ModsOwnedStorageTestPlace(RHH_MON_BOX, 0, 1, FALSE) && releaseState[3] == 3);
    gPokemonStoragePtr->boxes[0][0] = original.box; releaseState[2] = 4;
    CHECK(ModsOwnedStorageTestGroupMove(0, 1) && releaseState[3] == 5);
    CHECK(GetBoxMonData(&gPokemonStoragePtr->boxes[1][6], MON_DATA_HP_IV) == 31
        && GetBoxMonData(&gPokemonStoragePtr->boxes[1][7], MON_DATA_HP_IV) == 27);
    gPokemonStoragePtr->boxes[0][2] = clone.box; gPokemonStoragePtr->boxes[0][2].secure.raw[0] ^= 1;
    releaseState[2] = 5; CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 1, 6) && releaseState[3] == 5);
    CHECK(ModsOwnedStorageTestPlace(RHH_MON_BOX, 0, 2, TRUE) && releaseState[3] == 6);
    ModsOwnedStorageTestRelease(); /* Corrupt held data has no legacy release snapshot. */
    gPokemonStoragePtr->boxes[0][2] = clone.box; gPokemonStoragePtr->boxes[0][2].secure.raw[0] ^= 1;
    CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 0, 2));
    ModsOwnedStorageTestRelease();
    gPokemonStoragePtr->boxes[0][2] = clone.box; gPokemonStoragePtr->boxes[0][2].secure.raw[0] ^= 1;
    memset(&gPokemonStoragePtr->boxes[0][3], 0, sizeof(struct BoxPokemon));
    CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 0, 2));
    CHECK(ModsOwnedStorageTestPlace(RHH_MON_BOX, 0, 3, FALSE));
    CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 0, 3));
    gPokemonStoragePtr->boxes[0][4] = clone.box; gPokemonStoragePtr->boxes[0][4].secure.raw[0] ^= 1;
    CHECK(ModsOwnedStorageTestPlace(RHH_MON_BOX, 0, 4, TRUE));
    ModsOwnedStorageTestRelease();
    gPokemonStoragePtr->boxes[0][0] = clone.box; gPokemonStoragePtr->boxes[0][0].secure.raw[0] ^= 1;
    memset(&gPokemonStoragePtr->boxes[0][1], 0, sizeof(struct BoxPokemon));
    memset(&gPokemonStoragePtr->boxes[1][6], 0, 2 * sizeof(struct BoxPokemon));
    CHECK(ModsOwnedStorageTestGroupMove(0, 1));
    gPokemonStoragePtr->boxes[0][2] = clone.box; gPokemonStoragePtr->boxes[0][2].secure.raw[0] ^= 1;
    CHECK(ModsOwnedStorageTestPrepareRelease(RHH_MON_BOX, 0, 2, FALSE));
    ModsOwnedStorageTestRelease();
    gPokemonStoragePtr->boxes[0][5] = clone.box; gPokemonStoragePtr->boxes[0][5].secure.raw[0] ^= 1;
    gPokemonStoragePtr->boxes[0][4] = original.box;
    CHECK(ModsOwnedStorageTestPick(RHH_MON_BOX, 0, 5));
    CHECK(ModsOwnedStorageTestPlace(RHH_MON_BOX, 0, 4, TRUE));
    releaseState[0] = 5; ModsOwnedStorageTestRelease(); CHECK(releaseState[1] == 5);
    ModsOwnedStorageTestClose(); SetMainCallback2(previousCallback);
    struct RhhOwnedStorageUnreadable outsideUnreadable = {.struct_size = sizeof(outsideUnreadable), .cause = 999}, savedUnreadable = outsideUnreadable;
    CHECK(mons->storage_unreadable_context(&outsideUnreadable) && !memcmp(&outsideUnreadable, &savedUnreadable, sizeof(outsideUnreadable)));
    struct RhhOwnedPickup outsidePickup = {.struct_size = sizeof(outsidePickup), .source_slot = 999}, savedPickup = outsidePickup;
    CHECK(mons->pickup_context(&outsidePickup) && !memcmp(&outsidePickup, &savedPickup, sizeof(outsidePickup)));
    CHECK(mons->release_context(&outside) && !memcmp(&outside, &oldOutside, sizeof(outside)));
    CHECK(mons->move_context(&outsideMove) && !memcmp(&outsideMove, &savedMove, sizeof(outsideMove)));
    struct RhhOwnedGift outsideGift = {.struct_size = sizeof(outsideGift), .cause = 999}, savedGift = outsideGift;
    CHECK(mons->gift_context(&outsideGift) && !memcmp(&outsideGift, &savedGift, sizeof(outsideGift)));
    struct PokemonStorage *savedStorage = malloc(sizeof(*savedStorage)); CHECK(savedStorage);
    *savedStorage = *gPokemonStoragePtr;
    u16 savedBoxVar = VarGet(VAR_PC_BOX_TO_SEND_MON);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party)); gPartiesCount[B_TRAINER_PLAYER] = 0;
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    VarSet(VAR_PC_BOX_TO_SEND_MON, 0);
    CHECK(ScriptGiveMon(SPECIES_TORCHIC, 20, ITEM_NONE) == MON_GIVEN_TO_PARTY);
    CHECK(GiveScriptedMonToPlayer(&clone, 0) == MON_GIVEN_TO_PARTY);
    gParties[B_TRAINER_PLAYER][1] = original; gParties[B_TRAINER_PLAYER][1].box.secure.raw[0] ^= 1;
    CHECK(GiveScriptedMonToPlayer(&original, 1) == MON_GIVEN_TO_PARTY);
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = original;
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE;
    CHECK(ScriptGiveMon(SPECIES_TORCHIC, 20, ITEM_NONE) == MON_GIVEN_TO_PC);
    ZeroMonData(&gParties[B_TRAINER_PLAYER][5]); gPartiesCount[B_TRAINER_PLAYER] = 5;
    CHECK(ScriptGiveEgg(SPECIES_TORCHIC) == MON_GIVEN_TO_PARTY);
    gParties[B_TRAINER_PLAYER][5] = original; gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE;
    CHECK(ScriptGiveEgg(SPECIES_TORCHIC) == MON_GIVEN_TO_PC);
    for (u32 box = 0; box < TOTAL_BOXES_COUNT; ++box)
        for (u32 pos = 0; pos < IN_BOX_COUNT; ++pos) gPokemonStoragePtr->boxes[box][pos] = original.box;
    CHECK(ScriptGiveMon(SPECIES_TORCHIC, 20, ITEM_NONE) == MON_CANT_GIVE);
    CHECK(ScriptGiveEgg(SPECIES_TORCHIC) == MON_CANT_GIVE);
    memset(&gPokemonStoragePtr->boxes[0][4], 0, sizeof(struct BoxPokemon));
    memset(&gPokemonStoragePtr->boxes[0][5], 0, sizeof(struct BoxPokemon));
    CHECK(DebugStoreBoxMon(0, 4, &original.box) && DebugStoreBoxMon(0, 5, &original.box));
    CHECK(!DebugStoreBoxMon(0, 4, &original.box) && !DebugStoreBoxMon(TOTAL_BOXES_COUNT, 0, &original.box));
    ZeroMonData(&gParties[B_TRAINER_PLAYER][5]); gPartiesCount[B_TRAINER_PLAYER] = 5;
    CHECK(GiveMysteryEventTestMon(&original) == MEVENT_STATUS_SUCCESS && gPartiesCount[B_TRAINER_PLAYER] == PARTY_SIZE);
    CHECK(GiveMysteryEventTestMon(&original) == MEVENT_STATUS_FAILURE);
    *gPokemonStoragePtr = *savedStorage; free(savedStorage); VarSet(VAR_PC_BOX_TO_SEND_MON, savedBoxVar);
    CHECK(mons->gift_context(&outsideGift) && !memcmp(&outsideGift, &savedGift, sizeof(outsideGift)));
    struct RhhOwnedEvolution outsideEvolution = {.struct_size = sizeof(outsideEvolution), .cause = 999}, savedEvolution = outsideEvolution;
    CHECK(mons->evolution_context(&outsideEvolution) && !memcmp(&outsideEvolution, &savedEvolution, sizeof(outsideEvolution)));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_NINCADA, 20, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1; CHECK(AddBagItem(ITEM_POKE_BALL, 3));
    ModsOwnedEvolutionTestCommit(0, SPECIES_NINJASK, FALSE);
    ModsOwnedEvolutionTestSplit(0, SPECIES_NINCADA, SPECIES_NINJASK);
    CHECK(gPartiesCount[B_TRAINER_PLAYER] == 2
        && GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES) == SPECIES_SHEDINJA);
    gParties[B_TRAINER_PLAYER][1] = original; gParties[B_TRAINER_PLAYER][1].box.secure.raw[0] ^= 1;
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    ModsOwnedEvolutionTestSplit(0, SPECIES_NINCADA, SPECIES_NINJASK);
    CHECK(gPartiesCount[B_TRAINER_PLAYER] == 2
        && GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES) == SPECIES_SHEDINJA);
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE;
    ModsOwnedEvolutionTestSplit(0, SPECIES_NINCADA, SPECIES_NINJASK);
    gPartiesCount[B_TRAINER_PLAYER] = 1; gParties[B_TRAINER_PLAYER][0] = original;
    gParties[B_TRAINER_OPPONENT_A][0] = original;
    u8 oldRemotePlayers = gReceivedRemoteLinkPlayers; gReceivedRemoteLinkPlayers = TRUE;
    CHECK(ModsOwnedLinkTradeTestBegin(0));
    ModsOwnedEvolutionTestCommit(0, SPECIES_COMBUSKEN, TRUE);
    ModsOwnedLinkTradeTestEnd(); gReceivedRemoteLinkPlayers = oldRemotePlayers;
    CHECK(mons->evolution_context(&outsideEvolution) && !memcmp(&outsideEvolution, &savedEvolution, sizeof(outsideEvolution)));
    struct RhhPartyReorder outsideReorder = {.struct_size = sizeof(outsideReorder), .cause = 999}, savedReorder = outsideReorder;
    CHECK(mons->party_reorder_context(&outsideReorder) && !memcmp(&outsideReorder, &savedReorder, sizeof(outsideReorder)));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party));
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_PLAYER][1] = clone;
    gPartiesCount[B_TRAINER_PLAYER] = 2; releaseState[4] = 1;
    CHECK(!mons->identity_query(mon.identity, &query));
    CHECK(ModsOwnedPartyTestSwap(0, 1) && releaseState[5] == 1);
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV) == 27);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party));
    gParties[B_TRAINER_PLAYER][2] = original; gParties[B_TRAINER_PLAYER][4] = clone;
    gPartiesCount[B_TRAINER_PLAYER] = 5; releaseState[4] = 2;
    CompactPartySlots(); CHECK(releaseState[5] == 2);
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV) == 31
        && GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_HP_IV) == 27
        && !GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_SANITY_HAS_SPECIES));
    CompactPartySlots(); CHECK(releaseState[5] == 2);
    gMain.inBattle = TRUE; CHECK(ModsOwnedPartyTestSwap(0, 1) && releaseState[5] == 2); gMain.inBattle = inBattle;
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party));
    gParties[B_TRAINER_PLAYER][1] = original;
    gParties[B_TRAINER_PLAYER][1].box.secure.raw[0] ^= 1;
    releaseState[4] = 3; CompactPartySlots(); CHECK(releaseState[5] == 3);
    CHECK(mons->party_reorder_context(&outsideReorder) && !memcmp(&outsideReorder, &savedReorder, sizeof(outsideReorder)));
    struct RhhDaycareTransfer outsideTransfer = {.struct_size = sizeof(outsideTransfer), .cause = 999}, savedTransfer = outsideTransfer;
    CHECK(mons->daycare_transfer_context(&outsideTransfer) && !memcmp(&outsideTransfer, &savedTransfer, sizeof(outsideTransfer)));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(daycare));
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_PLAYER][1] = clone;
    gPartiesCount[B_TRAINER_PLAYER] = 2; gSpecialVar_0x8004 = 0; releaseState[4] = 4;
    StoreSelectedPokemonInDaycare();
    CHECK(releaseState[7] == 1 && releaseState[5] == 4
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV) == 27);
    gPokemonStoragePtr->boxes[2][3] = clone.box;
    gSpecialVar_0x8004 = PC_MON_CHOSEN; gSpecialVar_MonBoxId = 2; gSpecialVar_MonBoxPos = 3;
    StoreSelectedPokemonInDaycare();
    CHECK(releaseState[7] == 2 && releaseState[5] == 4
        && !gPokemonStoragePtr->boxes[2][3].hasSpecies);
    gSpecialVar_0x8004 = 0; releaseState[4] = 5;
    CHECK(TakePokemonFromDaycare() == SPECIES_TORCHIC && releaseState[7] == 4 && releaseState[5] == 5);
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_HP_IV) == 31
        && GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_HP_IV) == 27);
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(daycare));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party));
    gParties[B_TRAINER_PLAYER][0] = original;
    gParties[B_TRAINER_PLAYER][0].box.secure.raw[0] ^= 1;
    gPartiesCount[B_TRAINER_PLAYER] = 1; gSpecialVar_0x8004 = 0;
    StoreSelectedPokemonInDaycare(); CHECK(releaseState[7] == 5);
    struct RhhFusionTransfer outsideFusion = {.struct_size = sizeof(outsideFusion), .cause = 999}, savedFusion = outsideFusion;
    CHECK(mons->fusion_transfer_context(&outsideFusion) && !memcmp(&outsideFusion, &savedFusion, sizeof(outsideFusion)));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party));
    memset(&gPokemonStoragePtr->fusions[0], 0, sizeof(fusion));
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_PLAYER][1] = clone;
    gParties[B_TRAINER_PLAYER][2] = original; gPartiesCount[B_TRAINER_PLAYER] = 3; releaseState[4] = 6;
    CHECK(ModsOwnedFusionTestStep(RHH_FUSION_STORE, 0, 1, 0, SPECIES_COMBUSKEN)
        && releaseState[8] == 1 && releaseState[5] == 6);
    CHECK(ModsOwnedFusionTestStep(RHH_FUSION_RESTORE, 0, 1, 0, SPECIES_TORCHIC)
        && releaseState[8] == 2 && releaseState[5] == 6);
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][2], MON_DATA_HP_IV) == 27);
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = original;
    gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE; gPokemonStoragePtr->fusions[0] = clone;
    u16 fusedSpecies = SPECIES_COMBUSKEN; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES, &fusedSpecies);
    CHECK(ModsOwnedFusionTestStep(RHH_FUSION_RESTORE, 0, 1, 0, SPECIES_TORCHIC) && releaseState[8] == 3);
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party));
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_PLAYER][1] = clone;
    gParties[B_TRAINER_PLAYER][1].box.secure.raw[0] ^= 1; gPartiesCount[B_TRAINER_PLAYER] = 2;
    CHECK(ModsOwnedFusionTestStep(RHH_FUSION_STORE, 0, 1, 0, SPECIES_COMBUSKEN) && releaseState[8] == 4);
    /* A destination overwrite is not a safe transfer receipt. */
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_PLAYER][1] = clone;
    gPartiesCount[B_TRAINER_PLAYER] = 2; gPokemonStoragePtr->fusions[0] = original;
    CHECK(ModsOwnedFusionTestStep(RHH_FUSION_STORE, 0, 1, 0, SPECIES_COMBUSKEN) && releaseState[8] == 4);
    CHECK(mons->fusion_transfer_context(&outsideFusion) && !memcmp(&outsideFusion, &savedFusion, sizeof(outsideFusion)));
    CHECK(mons->daycare_transfer_context(&outsideTransfer) && !memcmp(&outsideTransfer, &savedTransfer, sizeof(outsideTransfer)));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party));
    memset(&gPokemonStoragePtr->fusions[0], 0, sizeof(fusion));
    memset(&gSaveBlock1Ptr->daycare, 0, sizeof(daycare));
    struct Pokemon battleFirst; CreateMonWithIVs(&battleFirst, SPECIES_TORCHIC, 20, 101010, OTID_STRUCT_PRESET(6789), 31);
    struct Pokemon battleClone = battleFirst; hpIv = 27; SetMonData(&battleClone, MON_DATA_HP_IV, &hpIv);
    gParties[B_TRAINER_PLAYER][0] = battleFirst; gParties[B_TRAINER_PLAYER][1] = battleClone;
    gPartiesCount[B_TRAINER_PLAYER] = 2; gPokemonStoragePtr->boxes[2][5] = battleFirst.box;
    CHECK(mons->party_view() == RHH_OWNED_PARTY_FIELD_ORDER);
    RhhModsObserveOwnedMons(); CHECK(releaseState[9] == 1);
    struct PartyMenu savedMenu = gPartyMenu;
    u8 savedOrder[sizeof(gBattlePartyCurrentOrder)]; memcpy(savedOrder, gBattlePartyCurrentOrder, sizeof(savedOrder));
    gPartyMenu.layout = PARTY_LAYOUT_SINGLE; gMain.inBattle = TRUE; gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    gBattlePartyCurrentOrder[0] = 0x10; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    u32 fieldSlot = 99; CHECK(mons->battle_view_field_slot(0, &fieldSlot) && fieldSlot == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE) && mons->party_view() == RHH_OWNED_PARTY_BATTLE_MENU);
    CHECK(!mons->battle_view_field_slot(0, &fieldSlot) && fieldSlot == 1);
    CHECK(!mons->battle_view_field_slot(1, &fieldSlot) && fieldSlot == 0);
    CHECK(!mons->battle_view_field_slot(5, &fieldSlot) && fieldSlot == 5);
    fieldSlot = 99; CHECK(mons->battle_view_field_slot(PARTY_SIZE, &fieldSlot) && fieldSlot == 99
                          && mons->battle_view_field_slot(0, NULL));
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV) == 27);
    struct RhhOwnedMonSnapshot unavailable = {.struct_size = sizeof(unavailable), .identity = 99}, savedUnavailable = unavailable;
    CHECK(mons->party_at(0, &unavailable) && !memcmp(&unavailable, &savedUnavailable, sizeof(unavailable)));
    u64 battleIdentity = ((u64)6789 << 32) | 101010;
    CHECK(mons->find(battleIdentity, &unavailable) && !memcmp(&unavailable, &savedUnavailable, sizeof(unavailable)));
    old = query; CHECK(mons->identity_query(battleIdentity, &query) && !memcmp(&query, &old, sizeof(query)));
    CHECK(!mons->box_at(2, 5, &unavailable) && unavailable.identity == battleIdentity);
    RhhModsObserveOwnedMons(); CHECK(releaseState[9] == 1);
    CHECK(ModsOwnedPartyBattleTestSwitch(0, 1));
    CHECK(!mons->battle_view_field_slot(0, &fieldSlot) && fieldSlot == 0);
    CHECK(!mons->battle_view_field_slot(1, &fieldSlot) && fieldSlot == 1);
    CHECK(ModsOwnedPartyBattleTestSwitch(1, 2));
    CHECK(!mons->battle_view_field_slot(1, &fieldSlot) && fieldSlot == 2);
    CHECK(!mons->battle_view_field_slot(2, &fieldSlot) && fieldSlot == 1);
    CHECK(ModsOwnedPartyTestSwap(0, 1));
    CHECK(!mons->battle_view_field_slot(0, &fieldSlot) && fieldSlot == 2);
    CHECK(!mons->battle_view_field_slot(1, &fieldSlot) && fieldSlot == 0);
    CHECK(ModsOwnedPartyTestSwap(0, 1));
    RhhModsObserveOwnedMons(); CHECK(releaseState[9] == 1);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE) && mons->party_view() == RHH_OWNED_PARTY_FIELD_ORDER);
    fieldSlot = 99; CHECK(mons->battle_view_field_slot(0, &fieldSlot) && fieldSlot == 99);
    CHECK(!memcmp(&gParties[B_TRAINER_PLAYER][0], &battleFirst, sizeof(battleFirst))
        && !memcmp(&gParties[B_TRAINER_PLAYER][1], &battleClone, sizeof(battleClone)));
    CHECK(!mons->identity_query(battleIdentity, &query) && query.matches == 3 && !query.unique_valid);
    RhhModsObserveOwnedMons(); CHECK(releaseState[9] == 1);
    gBattlePartyCurrentOrder[0] = 0x00; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE)); fieldSlot = 99;
    CHECK(mons->battle_view_field_slot(0, &fieldSlot) && fieldSlot == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE));
    gParties[B_TRAINER_PLAYER][0] = battleFirst; gParties[B_TRAINER_PLAYER][1] = battleClone;
    gBattlePartyCurrentOrder[0] = 0x10; gBattlePartyCurrentOrder[1] = 0x23; gBattlePartyCurrentOrder[2] = 0x45;
    gBattleTypeFlags = BATTLE_TYPE_INGAME_PARTNER;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE)); fieldSlot = 99;
    CHECK(mons->battle_view_field_slot(0, &fieldSlot) && fieldSlot == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE));
    gBattleTypeFlags = BATTLE_TYPE_FRONTIER;
    CHECK(ModsOwnedPartyBattleTestOrder(TRUE)); fieldSlot = 99;
    CHECK(mons->battle_view_field_slot(0, &fieldSlot) && fieldSlot == 99);
    CHECK(ModsOwnedPartyBattleTestOrder(FALSE));
    gPartyMenu = savedMenu; memcpy(gBattlePartyCurrentOrder, savedOrder, sizeof(savedOrder));
    gMain.inBattle = inBattle; gBattleTypeFlags = battleFlags;
    struct RhhCatchPlacement outsideCatch = {.struct_size = sizeof(outsideCatch), .cause = 999}, savedCatch = outsideCatch;
    CHECK(mons->catch_placement_context(&outsideCatch) && !memcmp(&outsideCatch, &savedCatch, sizeof(outsideCatch)));
    struct BattleStruct catchBattle = {0}; struct BattleStruct *previousBattle = gBattleStruct;
    gBattleStruct = &catchBattle; gMain.inBattle = TRUE; gBattleTypeFlags = 0;
    gBattlersCount = 2; gBattlerPositions[0] = 0; gBattlerPositions[1] = 1;
    gBattlerPartyIndexes[0] = gBattlerPartyIndexes[1] = 0; gAbsentBattlerFlags = 0; gBattleMons[1].hp = 1;
    for (u32 i = 0; i < PARTY_SIZE; ++i) gParties[B_TRAINER_PLAYER][i] = original;
    gParties[B_TRAINER_PLAYER][1] = clone; gPartiesCount[B_TRAINER_PLAYER] = PARTY_SIZE;
    gSaveBlock2Ptr->playerTrainerId[0] = 0x31; gSaveBlock2Ptr->playerTrainerId[1] = 0xd4;
    gSaveBlock2Ptr->playerTrainerId[2] = gSaveBlock2Ptr->playerTrainerId[3] = 0;
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_TORCHIC, 20, 33333, OTID_STRUCT_PRESET(777), 31);
    bool32 forceShiny = TRUE; SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_IS_SHINY, &forceShiny);
    bool32 caughtShiny = GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_IS_SHINY);
    ModsOwnedCatchTestStep(FALSE, 1); CHECK(releaseState[10] == 1 && !gParties[B_TRAINER_PLAYER][1].box.hasSpecies);
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE); CHECK(releaseState[10] == 2
        && GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_PERSONALITY) == 33333
        && GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES) == SPECIES_TORCHIC
        && GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_HP_IV) == 31
        && GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_IS_SHINY) == caughtShiny
        && !GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SANITY_IS_BAD_EGG));
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_TORCHIC, 20, 33333, OTID_STRUCT_PRESET(777), 31);
    forceShiny = FALSE; SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_IS_SHINY, &forceShiny);
    ModsOwnedCatchTestStep(TRUE, PARTY_SIZE); CHECK(releaseState[10] == 3
        && GetBoxMonData(&gPokemonStoragePtr->boxes[gSpecialVar_MonBoxId][gSpecialVar_MonBoxPos], MON_DATA_SPECIES) == SPECIES_TORCHIC
        && !GetBoxMonData(&gPokemonStoragePtr->boxes[gSpecialVar_MonBoxId][gSpecialVar_MonBoxPos], MON_DATA_IS_SHINY));
    gParties[B_TRAINER_PLAYER][1] = clone; gParties[B_TRAINER_PLAYER][1].box.secure.raw[0] ^= 1;
    ModsOwnedCatchTestStep(FALSE, 1); CHECK(releaseState[10] == 4);
    ModsOwnedCatchTestStep(FALSE, PARTY_SIZE + 1); CHECK(releaseState[10] == 4);
    for (u32 box = 0; box < TOTAL_BOXES_COUNT; ++box)
        for (u32 slot = 0; slot < IN_BOX_COUNT; ++slot) gPokemonStoragePtr->boxes[box][slot] = original.box;
    gParties[B_TRAINER_PLAYER][2] = original;
    ModsOwnedCatchTestStep(FALSE, 2); CHECK(releaseState[10] == 4 && gParties[B_TRAINER_PLAYER][2].box.hasSpecies);
    memset(&gPokemonStoragePtr->boxes[0][0], 0, sizeof(struct BoxPokemon));
    gPokemonStoragePtr->currentBox = 0; gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    ModsOwnedCatchTestStep(FALSE, 2); CHECK(releaseState[10] == 4 && !gParties[B_TRAINER_PLAYER][2].box.hasSpecies);
    CHECK(mons->catch_placement_context(&outsideCatch) && !memcmp(&outsideCatch, &savedCatch, sizeof(outsideCatch)));
    gBattleStruct = previousBattle; gMain.inBattle = inBattle; gBattleTypeFlags = battleFlags;
    struct RhhIngameTrade outsideTrade = {.struct_size = sizeof(outsideTrade), .source_slot = 999}, savedTrade = outsideTrade;
    CHECK(mons->ingame_trade_context(&outsideTrade) && !memcmp(&outsideTrade, &savedTrade, sizeof(outsideTrade)));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(party));
    gParties[B_TRAINER_PLAYER][0] = original; gPartiesCount[B_TRAINER_PLAYER] = 1;
    gParties[B_TRAINER_OPPONENT_A][0] = clone;
    struct RhhIngameTrade invalidTrade;
    CHECK(!ModsIngameTradeCapture(&original.box, &clone.box, RHH_MON_PARTY, 0, 0, &invalidTrade));
    CHECK(ModsIngameTradeCapture(&gParties[B_TRAINER_PLAYER][0].box, &clone.box,
                                 RHH_MON_PARTY, 0, 0, &invalidTrade));
    ModsIngameTradeCommitted(&clone.box, RHH_MON_PARTY, 0, 0, &invalidTrade);
    CHECK(releaseState[11] == 0);
    CHECK(ModsOwnedTradeTestParty(0, FALSE) && releaseState[11] == 1
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV) == 27);
    memset(gPokemonStoragePtr->boxes, 0, sizeof(gPokemonStoragePtr->boxes));
    gPokemonStoragePtr->currentBox = 0; gPokemonStoragePtr->boxes[2][5] = clone.box;
    gParties[B_TRAINER_OPPONENT_A][0] = original;
    CHECK(ModsOwnedTradeTestBeginPc(2, 5) && ModsOwnedTradePcHeld());
    CHECK(mons->trade_state() == RHH_OWNED_TRADE_PC_HELD);
    old = query; CHECK(mons->identity_query(mon.identity, &query) && !memcmp(&query, &old, sizeof(query)));
    unavailable = savedUnavailable; CHECK(mons->find(mon.identity, &unavailable)
        && !memcmp(&unavailable, &savedUnavailable, sizeof(unavailable)));
    RhhModsObserveOwnedMons(); CHECK(releaseState[9] == 1);
    gSpecialVar_MonBoxId = 1; gSpecialVar_MonBoxPos = 8;
    CHECK(ModsOwnedTradeTestFinishPc() && ModsOwnedTradePcHeld() && releaseState[11] == 2);
    ModsOwnedEvolutionTestCommit(PC_MON_CHOSEN, SPECIES_COMBUSKEN, TRUE);
    CHECK(ModsOwnedTradeTestSyncPc() && !ModsOwnedTradePcHeld()
        && GetBoxMonData(&gPokemonStoragePtr->boxes[0][0], MON_DATA_SPECIES) == SPECIES_COMBUSKEN);
    gParties[B_TRAINER_PLAYER][1] = original; gParties[B_TRAINER_PLAYER][1].box.secure.raw[0] ^= 1;
    gParties[B_TRAINER_OPPONENT_A][0] = original; gPartiesCount[B_TRAINER_PLAYER] = 2;
    CHECK(ModsOwnedTradeTestParty(1, FALSE) && releaseState[11] == 3);
    gParties[B_TRAINER_OPPONENT_A][0] = original;
    CHECK(ModsOwnedTradeTestParty(0, TRUE) && releaseState[11] == 3);
    CHECK(mons->ingame_trade_context(&outsideTrade) && !memcmp(&outsideTrade, &savedTrade, sizeof(outsideTrade)));
    struct RhhLinkTrade outsideLink = {.struct_size = sizeof(outsideLink), .party_slot = 999}, savedLink = outsideLink;
    CHECK(mons->link_trade_context(&outsideLink) && !memcmp(&outsideLink, &savedLink, sizeof(outsideLink)));
    struct RhhSavedLinkTrade receipt = {.struct_size = sizeof(receipt), .serial = 999}, oldReceipt = receipt;
    CHECK(!mons->link_trade_history_count() && mons->link_trade_history_at(0, &receipt)
        && !memcmp(&receipt, &oldReceipt, sizeof(receipt))
        && mons->link_trade_receipt_context(&receipt) && !memcmp(&receipt, &oldReceipt, sizeof(receipt)));
    struct RhhSavedLinkTrade smallReceipt = {.struct_size = 1, .serial = 999}, oldSmallReceipt = smallReceipt;
    CHECK(mons->link_trade_history_at(0, &smallReceipt) && !memcmp(&smallReceipt, &oldSmallReceipt, sizeof(smallReceipt)));
    struct RhhLinkHistoryCheckpoint initial = {.struct_size = sizeof(initial), .run_id = 456};
    struct RhhLinkHistoryReconcile sync = {.struct_size = sizeof(sync)};
    CHECK(!mons->link_trade_reconcile(&initial, &sync) && sync.status == RHH_LINK_HISTORY_UP_TO_DATE && !sync.count);
    struct RhhLinkHistoryReconcile unchangedSync = {.struct_size = sizeof(unchangedSync), .status = 999}, savedSync = unchangedSync;
    struct RhhLinkHistoryCheckpoint invalidCheckpoint = initial; invalidCheckpoint.reserved = 1;
    CHECK(mons->link_trade_reconcile(&invalidCheckpoint, &unchangedSync) && !memcmp(&unchangedSync, &savedSync, sizeof(savedSync)));
    invalidCheckpoint = initial; invalidCheckpoint.digest[0] = 1;
    CHECK(mons->link_trade_reconcile(&invalidCheckpoint, &unchangedSync) && !memcmp(&unchangedSync, &savedSync, sizeof(savedSync)));
    struct RhhLinkHistoryCheckpoint smallCheckpoint = {.struct_size = 1, .serial = 999}, savedSmallCheckpoint = smallCheckpoint;
    CHECK(mons->link_trade_checkpoint(0, &smallCheckpoint) && !memcmp(&smallCheckpoint, &savedSmallCheckpoint, sizeof(smallCheckpoint)));
    bool8 savedRemote = gReceivedRemoteLinkPlayers, savedError = gLinkErrorOccurred;
    u8 savedWireless = gWirelessCommType, savedActivity = gPlayerCurrActivity;
    RhhModsSetRunContext(123, 456, 1);
    gReceivedRemoteLinkPlayers = TRUE; gLinkErrorOccurred = FALSE; gWirelessCommType = 0;
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_OPPONENT_A][0] = clone;
    CHECK(ModsOwnedLinkTradeTestBegin(0) && mons->trade_state() == RHH_OWNED_TRADE_LINK_ACTIVE);
    CHECK(!releaseState[12]); gLinkErrorOccurred = TRUE; ModsOwnedLinkTradeTestConfirm();
    gLinkErrorOccurred = FALSE; ModsOwnedLinkTradeTestConfirm(); ModsOwnedLinkTradeTestSaved(TRUE); CHECK(!releaseState[12]);
    ModsOwnedLinkTradeTestEnd(); CHECK(mons->trade_state() == RHH_OWNED_TRADE_IDLE);
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_OPPONENT_A][0] = clone;
    CHECK(ModsOwnedLinkTradeTestBegin(0)); ModsOwnedLinkTradeTestConfirm();
    gFlashMemoryPresent = FALSE; CHECK(ModsOwnedLinkTradeTestBeginSave());
    ModsOwnedLinkTradeTestSaved(TRUE); CHECK(!releaseState[12] && !mons->link_trade_history_count());
    ModsOwnedLinkTradeTestEnd();
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_OPPONENT_A][0] = clone;
    CHECK(ModsOwnedLinkTradeTestBegin(0)); ModsOwnedLinkTradeTestConfirm();
    gFlashMemoryPresent = TRUE; Save_ResetSaveCounters(); CHECK(!ModsOwnedLinkTradeTestBeginSave() && NativeSaveTransactionActive());
    CHECK(NativeSaveEnd(927, FALSE)); ModsOwnedLinkTradeTestSaved(FALSE);
    CHECK(!releaseState[12] && !mons->link_trade_history_count()); ModsOwnedLinkTradeTestEnd();
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_OPPONENT_A][0] = clone;
    CHECK(ModsOwnedLinkTradeTestBegin(0) && mons->trade_state() == RHH_OWNED_TRADE_LINK_ACTIVE);
    unavailable = savedUnavailable; CHECK(mons->party_at(0, &unavailable) && !memcmp(&unavailable, &savedUnavailable, sizeof(unavailable)));
    CHECK(mons->find(mon.identity, &unavailable) && !memcmp(&unavailable, &savedUnavailable, sizeof(unavailable)));
    old = query; CHECK(mons->identity_query(mon.identity, &query) && !memcmp(&query, &old, sizeof(query)));
    RhhModsObserveOwnedMons(); CHECK(releaseState[9] == 1 && !releaseState[12]);
    ModsOwnedLinkTradeTestConfirm(); CHECK(!releaseState[12]);
    ModsOwnedLinkTradeTestConfirm(); CHECK(!releaseState[12]);
    u32 journalSize; CHECK(!NativeSaveGetJournal(456, &journalSize));
    CHECK(!ModsOwnedLinkTradeTestBeginSave() && !mons->link_trade_history_count());
    bool32 finishedSave = FALSE;
    for (u32 i = 0; i < NUM_SECTORS_PER_SLOT + 2 && !finishedSave; ++i) finishedSave = LinkFullSave_WriteSector();
    CHECK(finishedSave); LinkFullSave_ReplaceLastSector();
    u32 savedTradeGeneration = gSaveCounter;
    ModsOwnedLinkTradeTestCompleteSave(); CHECK(releaseState[12] == 1 && !NativeSaveTransactionActive()
        && !gDamagedSaveSectors && mons->link_trade_history_count() == 1);
    CHECK(!mons->link_trade_history_at(0, &receipt) && receipt.serial == 1 && receipt.trade.run_id == 456);
    ModsOwnedLinkTradeTestSaved(TRUE); CHECK(releaseState[12] == 1);
    releaseState[12] = 0; CHECK(!NativeSaveRestoreMods(savedTradeGeneration));
    releaseState = RhhModsState("test:owned_identity"); CHECK(releaseState && !releaseState[12]
        && mons->link_trade_history_count() == 1);
    const void *journal = NativeSaveGetJournal(456, &journalSize);
    CHECK(journal && !RhhModsMergeJournal(journal, journalSize) && releaseState[12] == 1);
    ModsOwnedLinkTradeTestEnd(); CHECK(mons->trade_state() == RHH_OWNED_TRADE_IDLE);
    gWirelessCommType = 1; gPlayerCurrActivity = ACTIVITY_29;
    gParties[B_TRAINER_PLAYER][0] = clone; gParties[B_TRAINER_OPPONENT_A][0] = original;
    CHECK(ModsOwnedLinkTradeTestBegin(0)); ModsOwnedLinkTradeTestConfirm(); CHECK(releaseState[12] == 1);
    ModsOwnedEvolutionTestCommit(0, SPECIES_COMBUSKEN, TRUE);
    CHECK(!ModsOwnedLinkTradeTestBeginSave() && !NativeSaveEnd(929, TRUE));
    ModsOwnedLinkTradeTestSaved(TRUE); CHECK(releaseState[12] == 2 && mons->link_trade_history_count() == 2);
    ModsOwnedLinkTradeTestEnd();
    gWirelessCommType = 0; gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_OPPONENT_A][0] = clone;
    CHECK(ModsOwnedLinkTradeTestBegin(0)); hpIv = 12;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV, &hpIv);
    ModsOwnedLinkTradeTestConfirm(); ModsOwnedLinkTradeTestSaved(TRUE); CHECK(releaseState[12] == 2); ModsOwnedLinkTradeTestEnd();
    for (u32 altered = 0; altered < 2; ++altered)
    {
        gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_OPPONENT_A][0] = clone;
        CHECK(ModsOwnedLinkTradeTestBegin(0)); ModsOwnedLinkTradeTestConfirm();
        if (!altered) { hpIv = 12; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP_IV, &hpIv); }
        else { u16 species = SPECIES_COMBUSKEN; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES, &species); }
        CHECK(!ModsOwnedLinkTradeTestBeginSave() && NativeSaveTransactionActive());
        CHECK(NativeSaveEnd(930, FALSE)); ModsOwnedLinkTradeTestSaved(TRUE);
        CHECK(releaseState[12] == 2 && mons->link_trade_history_count() == 2); ModsOwnedLinkTradeTestEnd();
    }
    gParties[B_TRAINER_PLAYER][0] = original; gParties[B_TRAINER_OPPONENT_A][0] = clone;
    CHECK(ModsOwnedLinkTradeTestBegin(0)); ModsOwnedLinkTradeTestConfirm();
    CHECK(!ModsOwnedLinkTradeTestBeginSave() && mons->link_trade_history_count() == 2);
    finishedSave = FALSE;
    for (u32 i = 0; i < NUM_SECTORS_PER_SLOT + 2 && !finishedSave; ++i) finishedSave = LinkFullSave_WriteSector();
    CHECK(finishedSave); LinkFullSave_ReplaceLastSector();
    u32 crashGeneration = gSaveCounter;
    LinkFullSave_SetLastSectorSignature(); CHECK(!NativeSaveTransactionActive() && !gDamagedSaveSectors);
    CHECK(releaseState[12] == 2 && mons->link_trade_history_count() == 2);
    ModsOwnedLinkTradeTestEnd(); NativeSaveClose();
    CHECK(!NativeSaveOpen(path) && !NativeSaveRestoreMods(crashGeneration));
    RhhModsSetRunContext(123, 456, 1); releaseState = RhhModsState("test:owned_identity"); CHECK(releaseState);
    CHECK(mons->link_trade_history_count() == 3 && !mons->link_trade_history_at(2, &receipt)
        && receipt.serial == 3 && receipt.trade.run_id == 456 && !receipt.trade.sequence
        && receipt.trade.transport == RHH_LINK_TRADE_CABLE && receipt.trade.flags == 7
        && receipt.trade.sent.ivs[RHH_STAT_HP] == 31 && receipt.trade.received_before.ivs[RHH_STAT_HP] == 27
        && receipt.trade.received_after.ivs[RHH_STAT_HP] == 27);
    struct RhhLinkHistoryCheckpoint second = {.struct_size = sizeof(second)}, third = {.struct_size = sizeof(third)};
    CHECK(!mons->link_trade_checkpoint(1, &second) && !mons->link_trade_checkpoint(2, &third));
    CHECK(!mons->link_trade_reconcile(&initial, &sync) && sync.status == RHH_LINK_HISTORY_READY && !sync.next_index);
    CHECK(!mons->link_trade_reconcile(&second, &sync) && sync.status == RHH_LINK_HISTORY_READY && sync.next_index == 2);
    CHECK(!mons->link_trade_reconcile(&third, &sync) && sync.status == RHH_LINK_HISTORY_UP_TO_DATE);
    CHECK(!NativeSaveRestoreMods(929) && mons->link_trade_history_count() == 2);
    CHECK(!mons->link_trade_reconcile(&third, &sync) && sync.status == RHH_LINK_HISTORY_AHEAD);
    CHECK(!mons->link_trade_reconcile(&second, &sync) && sync.status == RHH_LINK_HISTORY_UP_TO_DATE);
    struct RhhLinkTrade branch = receipt.trade;
    CHECK(ModsLinkTradeHistoryStage(RHH_LINK_TRADE_WIRELESS, &branch)); ModsLinkTradeHistoryFinish(TRUE);
    CHECK(!mons->link_trade_reconcile(&third, &sync) && sync.status == RHH_LINK_HISTORY_DIVERGED);
    CHECK(!NativeSaveRestoreMods(crashGeneration) && mons->link_trade_history_count() == 3);
    CHECK(!mons->link_trade_reconcile(&third, &sync) && sync.status == RHH_LINK_HISTORY_UP_TO_DATE);
    CHECK(!mons->link_trade_history_at(2, &receipt));
    for (u32 i = 0; i < RHH_LINK_TRADE_HISTORY_CAPACITY; ++i)
    {
        CHECK(ModsLinkTradeHistoryStage(RHH_LINK_TRADE_CABLE, &receipt.trade));
        ModsLinkTradeHistoryFinish(TRUE);
    }
    CHECK(mons->link_trade_history_count() == RHH_LINK_TRADE_HISTORY_CAPACITY);
    CHECK(!mons->link_trade_reconcile(&third, &sync) && sync.status == RHH_LINK_HISTORY_READY && !sync.next_index);
    CHECK(!mons->link_trade_reconcile(&second, &sync) && sync.status == RHH_LINK_HISTORY_GAP);
    struct RhhLinkHistoryCheckpoint altered = third; altered.digest[0] ^= 1;
    CHECK(!mons->link_trade_reconcile(&altered, &sync) && sync.status == RHH_LINK_HISTORY_DIVERGED);
    CHECK(!mons->link_trade_history_at(0, &receipt) && receipt.serial == 4);
    CHECK(!mons->link_trade_history_at(RHH_LINK_TRADE_HISTORY_CAPACITY - 1, &receipt)
        && receipt.serial == RHH_LINK_TRADE_HISTORY_CAPACITY + 3);
    oldReceipt = receipt; CHECK(mons->link_trade_history_at(RHH_LINK_TRADE_HISTORY_CAPACITY, &receipt)
        && !memcmp(&receipt, &oldReceipt, sizeof(receipt)));
    struct RhhLinkHistoryCheckpoint retained = {.struct_size = sizeof(retained)}, reloaded = {.struct_size = sizeof(reloaded)};
    CHECK(!mons->link_trade_checkpoint(0, &retained));
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(930, TRUE));
    NativeSaveClose(); CHECK(!NativeSaveOpen(path) && !NativeSaveRestoreMods(930));
    RhhModsSetRunContext(123, 456, 1);
    CHECK(mons->link_trade_history_count() == RHH_LINK_TRADE_HISTORY_CAPACITY
        && !mons->link_trade_history_at(0, &receipt) && receipt.serial == 4
        && !mons->link_trade_checkpoint(0, &reloaded) && !memcmp(&retained, &reloaded, sizeof(retained)));
    CHECK(!mons->link_trade_reconcile(&third, &sync) && sync.status == RHH_LINK_HISTORY_READY);
    RhhModsSetRunContext(123, 457, 1); CHECK(!mons->link_trade_history_count()
        && !mons->link_trade_reconcile(&third, &sync) && sync.status == RHH_LINK_HISTORY_WRONG_RUN);
    RhhModsSetRunContext(123, 456, 1); CHECK(mons->link_trade_history_count() == RHH_LINK_TRADE_HISTORY_CAPACITY);
    gReceivedRemoteLinkPlayers = savedRemote; gLinkErrorOccurred = savedError;
    gWirelessCommType = savedWireless; gPlayerCurrActivity = savedActivity;
    CHECK(mons->link_trade_context(&outsideLink) && !memcmp(&outsideLink, &savedLink, sizeof(outsideLink)));
#endif
    memcpy(gParties[B_TRAINER_PLAYER], party, sizeof(party)); gPartiesCount[B_TRAINER_PLAYER] = partyCount;
    gPokemonStoragePtr->boxes[0][0] = boxed; gPokemonStoragePtr->currentBox = currentBox;
    gPokemonStoragePtr->fusions[0] = fusion; gSaveBlock1Ptr->daycare = daycare;
    RhhModsSetRunContext(0, 0, 0); NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned identity: hidden locations, corrupt collisions and safe queries passed");
}

static void CheckOwnedHistoryRead(const char *path, const char *module)
{
    InitModsForTestWithLibrary(module); CHECK(!NativeSaveOpen(path) && !NativeSaveRestoreMods(930));
    RhhModsSetRunContext(123, 456, 1);
    const struct RhhOwnedMonService *mons = (const void *)RhhModsHostAPI()->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 14));
    CHECK(mons && mons->link_trade_history_count() == RHH_LINK_TRADE_HISTORY_CAPACITY);
    struct RhhSavedLinkTrade receipt = {.struct_size = sizeof(receipt)};
    struct RhhLinkHistoryCheckpoint first = {.struct_size = sizeof(first)}, after = {.struct_size = sizeof(after)};
    struct RhhLinkHistoryCheckpoint initial = {.struct_size = sizeof(initial), .run_id = 456};
    struct RhhLinkHistoryReconcile sync = {.struct_size = sizeof(sync)};
    CHECK(!mons->link_trade_history_at(0, &receipt) && receipt.serial == 4
        && !mons->link_trade_checkpoint(0, &first) && first.serial == 4
        && !mons->link_trade_reconcile(&initial, &sync) && sync.status == RHH_LINK_HISTORY_GAP
        && !mons->link_trade_reconcile(&first, &sync) && sync.status == RHH_LINK_HISTORY_READY && sync.next_index == 1);
    CHECK(!NativeSaveBegin() && !NativeSaveEnd(931, TRUE)); NativeSaveClose();
    CHECK(!NativeSaveOpen(path) && !NativeSaveRestoreMods(931)); RhhModsSetRunContext(123, 456, 1);
    CHECK(mons->link_trade_history_count() == RHH_LINK_TRADE_HISTORY_CAPACITY
        && !mons->link_trade_checkpoint(0, &after) && !memcmp(&first, &after, sizeof(first)));
    NativeSaveClose(); RhhModsUnloadLibraries();
    puts("owned link history: legacy migration and stable checkpoints passed");
}
