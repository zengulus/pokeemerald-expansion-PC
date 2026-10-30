#include "party_menu.h"
#include "mod_ui.h"
#include "evolution_scene.h"
static void CheckExperienceAward(const char *module, u32 mode)
{
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); cgb_audio_init(42060); AgbMain();
    ClearSav1(); ClearSav2(); ClearSav3(); SetSaveBlocksPointers(0);
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    gMain.inBattle = FALSE; gMain.callback2 = CB2_Overworld; memset(&gPaletteFade, 0, sizeof(gPaletteFade));
    ScriptContext_Init(); UnlockPlayerFieldControls();
    u32 species = mode == 4 || mode == 5 ? SPECIES_CATERPIE : SPECIES_TORCHIC;
    u32 initialLevel = mode == 4 || mode == 5 ? 6 : 5;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], species, initialLevel, 12345, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    const u16 oldMoves[MAX_MON_MOVES] = {MOVE_SCRATCH, MOVE_GROWL, MOVE_TACKLE, MOVE_WATER_GUN};
    for (u32 i = 0; i < MAX_MON_MOVES; ++i) SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], mode == 1 ? oldMoves[i] : MOVE_NONE, i);
    const struct RhhOwnedMonService *owned = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 29));
    CHECK(owned);
    struct RhhModEvent capEvent = {.struct_size = sizeof(capEvent), .kind = RHH_MOD_UPDATE, .args = {77, 0, 0, 0}};
    struct RhhModDecision capDecision = {.struct_size = sizeof(capDecision), .allowed = 1};
    if (mode == 7) RhhModsDispatch(&capEvent, &capDecision);
    struct RhhExperienceAwardRequest request = {.struct_size = sizeof(request), .amount = 1,
        .owner = RhhContentKeyOf("test:experience_award"), .expected.struct_size = sizeof(request.expected)};
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
    CHECK(!owned->party_at(0, &mon) && !owned->experience(&mon, &request.expected));
    struct Pokemon before = gParties[B_TRAINER_PLAYER][0];
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    struct RhhExperienceAwardResult result = {.struct_size = sizeof(result)};
    struct RhhExperienceAwardResult outside = {.struct_size = sizeof(outside), .status = 99}, unchanged = outside;
    CHECK(owned->experience_award_context(&outside) && !memcmp(&outside, &unchanged, sizeof(outside)));
    request.expected.experience++;
    CHECK(owned->award_experience(&request, &result) == RHH_EXPERIENCE_AWARD_STALE
        && !memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before)));
    request.expected.experience--;
    CHECK(owned->award_experience(&request, &result) == RHH_EXPERIENCE_AWARD_DENIED
        && !strcmp(result.reason, "test denial") && !memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    request.amount = mode == 3 ? 2 : mode == 6 ? 799 : mode == 4 || mode == 5
        ? gExperienceTables[gSpeciesInfo[species].growthRate][7] - request.expected.experience : 800;
    CHECK(owned->award_experience(&request, &result) == RHH_EXPERIENCE_AWARD_PENDING && result.ticket
        && !memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before)));
    u64 ticket = result.ticket;
    CHECK(owned->award_experience(&request, &result) == RHH_EXPERIENCE_AWARD_UNAVAILABLE);
    if (mode == 8) RhhModsDispatch(&capEvent, &capDecision);
    if (mode == 2)
    {
        u32 exp = request.expected.experience + 1;
        SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP, &exp);
    }
    if (mode == 5) { gCB2_AfterEvolution = NULL; gSpecialVar_Result = EVO_EVENT_IMPOSSIBLE; }
    unsigned frames = 0;
    while (frames++ < 10000)
    {
        u16 keys = mode == 5 && gCB2_AfterEvolution && gSpecialVar_Result != EVO_EVENT_INTERRUPTED
            ? B_BUTTON : frames % 12 == 0 ? A_BUTTON : 0;
        PartyTestFrame(keys);
        m4aSoundMain(); m4aSoundVSync();
        result.struct_size = sizeof(result);
        CHECK(!owned->experience_award_status(ticket, &result));
        if (result.finished) break;
        if (result.status == RHH_EXPERIENCE_AWARD_OK)
            CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL) == result.experience.after_level);
    }
    CHECK(frames < 10000 && result.finished);
    outside = (struct RhhExperienceAwardResult){.struct_size = sizeof(outside), .status = 99}; unchanged = outside;
    CHECK(owned->experience_award_status(ticket + 1, &outside) && !memcmp(&outside, &unchanged, sizeof(outside)));
    if (mode == 2)
        CHECK(result.status == RHH_EXPERIENCE_AWARD_STALE
            && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP) == request.expected.experience + 1);
    else if (mode == 6)
        CHECK(result.status == RHH_EXPERIENCE_AWARD_DENIED && !strcmp(result.reason, "late denial")
            && !memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before)));
    else if (mode == 8)
        CHECK(result.status == RHH_EXPERIENCE_AWARD_STALE
            && !memcmp(&before, &gParties[B_TRAINER_PLAYER][0], sizeof(before)));
    else
        CHECK(result.status == RHH_EXPERIENCE_AWARD_OK
            && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_EXP) == result.experience.after_exp
            && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL) == result.experience.after_level);
    if (mode == 4) CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_METAPOD);
    if (mode == 5) CHECK(gSpecialVar_Result == EVO_EVENT_INTERRUPTED
        && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_CATERPIE);
    if (mode == 7) CHECK(result.experience.after_level == 6
        && result.experience.after_exp == gExperienceTables[gSpeciesInfo[SPECIES_TORCHIC].growthRate][6]);
    if (mode == 3)
    {
        CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TORCHIC, MAX_LEVEL, 12345, OTID_STRUCT_PRESET(54321), 31);
        CHECK(!owned->party_at(0, &mon) && !owned->experience(&mon, &request.expected));
        gMain.callback2 = CB2_Overworld; memset(&gPaletteFade, 0, sizeof(gPaletteFade));
        ScriptContext_Init(); UnlockPlayerFieldControls(); request.amount = 100;
        CHECK(owned->award_experience(&request, &outside) == RHH_EXPERIENCE_AWARD_NO_GAIN);
        outside.struct_size = sizeof(outside);
        CHECK(!owned->experience_award_status(ticket, &outside) && outside.finished && outside.status == RHH_EXPERIENCE_AWARD_OK);
    }
    if (mode == 0 || mode == 1)
    {
        const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(SPECIES_TORCHIC);
        u32 count = 0, last = MOVE_NONE;
        for (u32 i = 0; learnset[i].move != LEVEL_UP_MOVE_END; ++i)
            if (learnset[i].level > initialLevel && learnset[i].level <= result.experience.after_level)
            {
                if (mode == 0) CHECK(count < MAX_MON_MOVES
                    && GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + count) == learnset[i].move);
                last = learnset[i].move; ++count;
            }
        CHECK(count >= 2);
        if (mode == 1)
        {
            bool32 found = FALSE;
            for (u32 i = 0; i < MAX_MON_MOVES; ++i)
                if (GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MOVE1 + i) == last) found = TRUE;
            CHECK(found);
        }
    }
    RhhModsUnloadLibraries(); SDL_Quit();
    puts("experience award: validated menu flow completed");
}
