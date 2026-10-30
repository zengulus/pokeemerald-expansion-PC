#include "global.h"
#include "pokemon.h"
#include "battle.h"
#include "daycare.h"
#include "string_util.h"
#include "caps.h"
#include "mail.h"
#include "pokemon_storage_system.h"
#include "event_data.h"
#include "random.h"
#include "main.h"
#include "egg_hatch.h"
#include "text.h"
#include "menu.h"
#include "international_string_util.h"
#include "script.h"
#include "strings.h"
#include "task.h"
#include "window.h"
#include "party_menu.h"
#include "list_menu.h"
#include "overworld.h"
#include "ow_abilities.h"
#include "item.h"
#include "regions.h"
#include "malloc.h"
#include "constants/form_change_types.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/party_menu.h"
#include "constants/region_map_sections.h"
#ifdef NATIVE
#include "mods.h"
#include "owned_identity_ledger.h"
#endif

#define IS_DITTO(species) (gSpeciesInfo[species].eggGroups[0] == EGG_GROUP_DITTO || gSpeciesInfo[species].eggGroups[1] == EGG_GROUP_DITTO)

static void ClearDaycareMonMail(struct DaycareMail *mail);
static void SetInitialEggData(struct Pokemon *mon, enum Species species, struct DayCare *daycare);
static void DaycarePrintMonInfo(u8 windowId, u32 daycareSlotId, u8 y);
static u8 ModifyBreedingScoreForOvalCharm(u8 score);
static bool32 IsEggPending(struct DayCare *daycare);
static void AlterEggSpeciesWithIncenseItem(enum Species *species, struct DayCare *daycare);

static const struct WindowTemplate sDaycareLevelMenuWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 15,
    .tilemapTop = 1,
    .width = 14,
    .height = 6,
    .paletteNum = 15,
    .baseBlock = 8
};

// Indices here are assigned by Task_HandleDaycareLevelMenuInput to VAR_RESULT,
// which is copied to VAR_0x8004 and used as an index for GetDaycareCostAndPrepareString
static const struct ListMenuItem sLevelMenuItems[] =
{
    {gText_ExpandedPlaceholder_Empty, 0},
    {gText_ExpandedPlaceholder_Empty, 1},
    {gText_Exit, DAYCARE_LEVEL_MENU_EXIT}
};

static const struct ListMenuTemplate sDaycareListMenuLevelTemplate =
{
    .items = sLevelMenuItems,
    .moveCursorFunc = ListMenuDefaultCursorMoveFunc,
    .itemPrintFunc = DaycarePrintMonInfo,
    .totalItems = 3,
    .maxShowed = 3,
    .windowId = 0,
    .header_X = 0,
    .item_X = 8,
    .cursor_X = 0,
    .upText_Y = 1,
    .cursorPal = 2,
    .fillValue = 1,
    .cursorShadowPal = 3,
    .lettersSpacing = 1,
    .itemVerticalPadding = 0,
    .scrollMultiple = LIST_NO_MULTIPLE_SCROLL,
    .fontId = FONT_NORMAL,
    .cursorKind = CURSOR_BLACK_ARROW
};

static const struct {
  enum Species currSpecies;
  enum Item item;
  enum Species babySpecies;
} sIncenseBabyTable[] =
{
    // Regular offspring,   Item,              Incense Offspring
    { SPECIES_WOBBUFFET,    ITEM_LAX_INCENSE,  SPECIES_WYNAUT },
    { SPECIES_MARILL,       ITEM_SEA_INCENSE,  SPECIES_AZURILL },
    { SPECIES_SNORLAX,      ITEM_FULL_INCENSE, SPECIES_MUNCHLAX },
    { SPECIES_CHANSEY,      ITEM_LUCK_INCENSE, SPECIES_HAPPINY },
    { SPECIES_MR_MIME,      ITEM_ODD_INCENSE,  SPECIES_MIME_JR },
    { SPECIES_CHIMECHO,     ITEM_PURE_INCENSE, SPECIES_CHINGLING },
    { SPECIES_SUDOWOODO,    ITEM_ROCK_INCENSE, SPECIES_BONSLY },
    { SPECIES_ROSELIA,      ITEM_ROSE_INCENSE, SPECIES_BUDEW },
    { SPECIES_MANTINE,      ITEM_WAVE_INCENSE, SPECIES_MANTYKE },
};

static const u8 *const sCompatibilityMessages[] =
{
    gDaycareText_GetAlongVeryWell,
    gDaycareText_GetAlong,
    gDaycareText_DontLikeOther,
    gDaycareText_PlayOther
};

// SetMonData reads the complete fixed-width nickname, including after EOS.
static const u8 sJapaneseEggNickname[POKEMON_NAME_LENGTH] = _("タマゴ"); // "tamago" ("egg" in Japanese)

#ifdef NATIVE
static void DaycareGuardOutput(const void *dest, size_t size)
{
    ModsOwnedBattlePartySummaryBeforeBytes(dest, size);
    NativeOwnedTrackUnreportedCopy(dest, NULL, size);
}
#endif
u8 *GetMonNicknameVanilla(struct Pokemon *mon, u8 *dest)
{
    u8 nickname[POKEMON_NAME_BUFFER_SIZE];
    GetMonData(mon, MON_DATA_NICKNAME, nickname);
#ifdef NATIVE
    DaycareGuardOutput(dest, VANILLA_POKEMON_NAME_LENGTH);
#endif
    return StringCopyN(dest, nickname, VANILLA_POKEMON_NAME_LENGTH);
}

u8 *GetBoxMonNickname(struct BoxPokemon *mon, u8 *dest)
{
    u8 nickname[POKEMON_NAME_BUFFER_SIZE];
    GetBoxMonData(mon, MON_DATA_NICKNAME, nickname);
#ifdef NATIVE
    size_t length = min(StringLength(nickname), POKEMON_NAME_LENGTH) + 1;
    DaycareGuardOutput(dest, length);
#endif
    return StringCopy_Nickname(dest, nickname);
}

u8 CountPokemonInDaycare(struct DayCare *daycare)
{
    u8 i, count;
    count = 0;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES) != 0)
            count++;
    }

    return count;
}

void InitDaycareMailRecordMixing(struct DayCare *daycare, struct RecordMixingDaycareMail *mixMail)
{
    u8 i;
    u8 numDaycareMons = 0;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES) != SPECIES_NONE)
        {
            numDaycareMons++;
            if (GetBoxMonData(&daycare->mons[i].mon, MON_DATA_HELD_ITEM) == ITEM_NONE)
                mixMail->cantHoldItem[i] = FALSE;
            else
                mixMail->cantHoldItem[i] = TRUE;
        }
        else
        {
            // Daycare slot empty
            mixMail->cantHoldItem[i] = TRUE;
        }
    }

    mixMail->numDaycareMons = numDaycareMons;
}

s8 Daycare_FindEmptySpot(struct DayCare *daycare)
{
    u8 i;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES) == SPECIES_NONE)
            return i;
    }

    return -1;
}

static void TransferEggMovesFromBoxmonToBoxmon(struct BoxPokemon *receiver, struct BoxPokemon *giver)
{
    enum Species receiverSpecies = GetBoxMonData(receiver, MON_DATA_SPECIES);
    enum Species giverSpecies = GetBoxMonData(giver, MON_DATA_SPECIES);

    if (GetBoxMonData(receiver, MON_DATA_MOVE4) != MOVE_NONE)
        return;

    if (GetConfig(EGG_MOVE_TRANSFER) == GEN_8 && GET_BASE_SPECIES_ID(receiverSpecies) != GET_BASE_SPECIES_ID(giverSpecies))
        return;

    if (GetConfig(EGG_MOVE_TRANSFER) >= GEN_9 && GetBoxMonData(receiver, MON_DATA_HELD_ITEM) != ITEM_MIRROR_HERB)
        return;

    enum Species eggSpecies = GetEggSpecies(receiverSpecies);
    const u16 *eggMoveLearnset = GetSpeciesEggMoves(eggSpecies);
    for (u32 i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move moveToGive = GetBoxMonData(giver, MON_DATA_MOVE1 + i);
        if (moveToGive == MOVE_NONE)
            break;
        if (BoxMonKnowsMove(receiver, moveToGive))
            continue;
        for (u32 j = 0; eggMoveLearnset[j] != MOVE_UNAVAILABLE; j++)
        {
            if (moveToGive == eggMoveLearnset[j])
            {
                GiveMoveToBoxMon(receiver, moveToGive);
                if (GetBoxMonData(receiver, MON_DATA_MOVE4) != MOVE_NONE)
                    return;
                break;
            }
        }
    }
}

static void TransferEggMoves(struct DayCare *daycare)
{
    TransferEggMovesFromBoxmonToBoxmon(&daycare->mons[0].mon, &daycare->mons[1].mon);
    TransferEggMovesFromBoxmonToBoxmon(&daycare->mons[1].mon, &daycare->mons[0].mon);
}

#ifdef NATIVE
static bool32 DaycareTransferLocation(struct DaycareMon *mon, u32 *daycare, u32 *slot)
{
    for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
        if (mon == &gSaveBlock1Ptr->daycare.mons[i]) { *daycare = 0; *slot = i; return TRUE; }
#if IS_FRLG
    if (mon == &gSaveBlock1Ptr->route5DayCareMon) { *daycare = 1; *slot = 0; return TRUE; }
#endif
    return FALSE;
}
static bool32 DaycarePartySlot(struct Pokemon *mon, u32 *slot)
{
    for (u32 i = 0; i < PARTY_SIZE; ++i)
        if (mon == &gParties[B_TRAINER_PLAYER][i]) { *slot = i; return TRUE; }
    return FALSE;
}
#endif

static void StorePokemonInDaycareInternal(struct Pokemon *mon, struct DaycareMon *daycareMon, bool32 fromPc)
{
    (void)fromPc;
#ifdef NATIVE
    struct RhhDaycareTransfer transfer;
    u32 daycare = 0, slot = 0, sourceSlot = 0, sourceLocation = RHH_MON_PARTY, sourceBox = 0;
    bool32 sourceKnown = DaycarePartySlot(mon, &sourceSlot);
    if (!sourceKnown && fromPc)
    {
        sourceLocation = RHH_MON_BOX; sourceBox = gSpecialVar_MonBoxId; sourceSlot = gSpecialVar_MonBoxPos;
        sourceKnown = TRUE;
    }
    bool32 notify = sourceKnown && !daycareMon->mon.hasSpecies && DaycareTransferLocation(daycareMon, &daycare, &slot)
        && ModsDaycareTransferCapture(&mon->box, RHH_DAYCARE_DEPOSIT, sourceLocation, sourceBox, sourceSlot,
                                      RHH_MON_DAYCARE, daycare, slot, &transfer);
#endif
    if (MonHasMail(mon))
    {
        u8 mailId;

        StringCopy(daycareMon->mail.otName, gSaveBlock2Ptr->playerName);
        GetMonNicknameVanilla(mon, daycareMon->mail.monName);
        StripExtCtrlCodes(daycareMon->mail.monName);
        daycareMon->mail.gameLanguage = GAME_LANGUAGE;
        daycareMon->mail.monLanguage = GetMonData(mon, MON_DATA_LANGUAGE);
        mailId = GetMonData(mon, MON_DATA_MAIL);
        daycareMon->mail.message = gSaveBlock1Ptr->mail[mailId];
        TakeMailFromMon(mon);
    }

    TryFormChange(mon, FORM_CHANGE_DEPOSIT, B_TRAINER_PLAYER);

    daycareMon->mon = mon->box;
    daycareMon->steps = 0;
#ifdef NATIVE
    if (notify) ZeroMonDataWithOwnedReceipt(mon);
    else
#endif
    ZeroMonData(mon);
#ifdef NATIVE
    if (notify) ModsDaycareTransferCommitted(&daycareMon->mon, &transfer);
    else NativeOwnedTrackerUnsupported();
#endif
    CompactPartySlots();
    CalculatePlayerPartyCount();
}

void StorePokemonInDaycare(struct Pokemon *mon, struct DaycareMon *daycareMon)
{
    StorePokemonInDaycareInternal(mon, daycareMon, FALSE);
}

static void StorePokemonInEmptyDaycareSlot(struct Pokemon *mon, struct DayCare *daycare, bool32 fromPc)
{
    s8 slotId = Daycare_FindEmptySpot(daycare);
    assertf(slotId >= 0, "Trying to store pokemon in already full daycare")
    {
        return;
    }
    StorePokemonInDaycareInternal(mon, &daycare->mons[slotId], fromPc);
    // Transfer egg moves if there are at least 2 pokemon in the daycare
    if (GetConfig(EGG_MOVE_TRANSFER) >= GEN_8 && slotId >= 1)
        TransferEggMoves(daycare);
}

void StoreSelectedPokemonInDaycare(void)
{
    struct Pokemon *mon;
    if (gSpecialVar_0x8004 == PC_MON_CHOSEN)
    {
        mon = Alloc(sizeof(struct Pokemon));
#ifdef NATIVE
        if (Daycare_FindEmptySpot(&gSaveBlock1Ptr->daycare) >= 0) RemoveSelectedPcMonWithOwnedReceipt(mon);
        else
#endif
        RemoveSelectedPcMon(mon);
    }
    else
    {
        mon = &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004];
    }
    StorePokemonInEmptyDaycareSlot(mon, &gSaveBlock1Ptr->daycare, gSpecialVar_0x8004 == PC_MON_CHOSEN);
    if (gSpecialVar_0x8004 == PC_MON_CHOSEN)
        Free(mon);
}

// Shifts the second daycare Pokémon slot into the first slot.
static void ShiftDaycareSlots(struct DayCare *daycare)
{
    // This condition is only satisfied when the player takes out the first Pokémon from the daycare.
    if (GetBoxMonData(&daycare->mons[1].mon, MON_DATA_SPECIES) != SPECIES_NONE
        && GetBoxMonData(&daycare->mons[0].mon, MON_DATA_SPECIES) == SPECIES_NONE)
    {
#ifdef NATIVE
        struct RhhDaycareTransfer transfer;
        u32 daycareId, sourceSlot;
        bool32 notify = DaycareTransferLocation(&daycare->mons[1], &daycareId, &sourceSlot)
            && ModsDaycareTransferCapture(&daycare->mons[1].mon, RHH_DAYCARE_SHIFT, RHH_MON_DAYCARE,
                                          daycareId, sourceSlot, RHH_MON_DAYCARE, daycareId, 0, &transfer);
#endif
        daycare->mons[0].mon = daycare->mons[1].mon;
#ifdef NATIVE
        if (notify) ZeroBoxMonDataWithOwnedReceipt(&daycare->mons[1].mon);
        else
#endif
        ZeroBoxMonData(&daycare->mons[1].mon);

        daycare->mons[0].mail = daycare->mons[1].mail;
        daycare->mons[0].steps = daycare->mons[1].steps;
        daycare->mons[1].steps = 0;
        ClearDaycareMonMail(&daycare->mons[1].mail);
#ifdef NATIVE
        if (notify) ModsDaycareTransferCommitted(&daycare->mons[0].mon, &transfer);
        else NativeOwnedTrackerUnsupported();
#endif
    }
}

static void ApplyDaycareExperience(struct Pokemon *mon)
{
    s32 i;
    bool8 firstMove;
    enum Move learnedMove;

    for (i = 0; i < MAX_LEVEL; i++)
    {
        // Add the mon's gained daycare experience level by level until it can't level up anymore.
        if (TryIncrementMonLevel(mon))
        {
            // Teach the mon new moves it learned while in the daycare.
            firstMove = TRUE;
            while ((learnedMove = MonTryLearningNewMove(mon, firstMove)) != 0)
            {
                firstMove = FALSE;
                if (learnedMove == MON_HAS_MAX_MOVES)
                    DeleteFirstMoveAndGiveMoveToMon(mon, gMoveToLearn);
            }
        }
        else
        {
            break;
        }
    }

    // Re-calculate the mons stats at its new level.
    CalculateMonStats(mon);
}

static u32 GetExpAtLevelCap(struct Pokemon *mon)
{
    return gExperienceTables[gSpeciesInfo[GetMonData(mon, MON_DATA_SPECIES)].growthRate][GetCurrentLevelCap()];
}

static u16 TakeSelectedPokemonFromDaycare(struct DaycareMon *daycareMon)
{
    u32 experience;
    struct Pokemon pokemon;
#ifdef NATIVE
    struct RhhDaycareTransfer transfer;
    u32 daycare, slot;
    bool32 notify = !gParties[B_TRAINER_PLAYER][PARTY_SIZE - 1].box.hasSpecies
        && DaycareTransferLocation(daycareMon, &daycare, &slot)
        && ModsDaycareTransferCapture(&daycareMon->mon, RHH_DAYCARE_WITHDRAW, RHH_MON_DAYCARE, daycare, slot,
                                      RHH_MON_PARTY, 0, PARTY_SIZE - 1, &transfer);
#endif

    GetBoxMonNickname(&daycareMon->mon, gStringVar1);
    BoxMonToMon(&daycareMon->mon, &pokemon);

    TryFormChange(&pokemon, FORM_CHANGE_WITHDRAW, B_TRAINER_PLAYER);

    if (GetMonData(&pokemon, MON_DATA_LEVEL) < GetCurrentLevelCap())
    {
        experience = GetMonData(&pokemon, MON_DATA_EXP) + daycareMon->steps;
        u32 maxExp = GetExpAtLevelCap(&pokemon);
        if (experience > maxExp)
            experience = maxExp;
        SetMonData(&pokemon, MON_DATA_EXP, &experience);
        ApplyDaycareExperience(&pokemon);
    }

    gParties[B_TRAINER_PLAYER][PARTY_SIZE - 1] = pokemon;
    if (daycareMon->mail.message.itemId)
    {
        GiveMailToMon(&gParties[B_TRAINER_PLAYER][PARTY_SIZE - 1], &daycareMon->mail.message);
        ClearDaycareMonMail(&daycareMon->mail);
    }

#ifdef NATIVE
    if (notify) ZeroBoxMonDataWithOwnedReceipt(&daycareMon->mon);
    else
#endif
    ZeroBoxMonData(&daycareMon->mon);
    daycareMon->steps = 0;
#ifdef NATIVE
    if (notify) ModsDaycareTransferCommitted(&gParties[B_TRAINER_PLAYER][PARTY_SIZE - 1].box, &transfer);
    else NativeOwnedTrackerUnsupported();
#endif
    CompactPartySlots();
    CalculatePlayerPartyCount();
    return GetMonData(&pokemon, MON_DATA_SPECIES);
}

static u16 TakeSelectedPokemonMonFromDaycareShiftSlots(struct DayCare *daycare, u8 slotId)
{
    enum Species species = TakeSelectedPokemonFromDaycare(&daycare->mons[slotId]);
    ShiftDaycareSlots(daycare);
    return species;
}

u16 TakePokemonFromDaycare(void)
{
    return TakeSelectedPokemonMonFromDaycareShiftSlots(&gSaveBlock1Ptr->daycare, gSpecialVar_0x8004);
}

static u8 GetLevelAfterDaycareSteps(struct BoxPokemon *mon, u32 steps)
{
    struct BoxPokemon tempMon = *mon;

    u32 experience = GetBoxMonData(mon, MON_DATA_EXP) + steps;
    SetBoxMonData(&tempMon, MON_DATA_EXP,  &experience);
    return GetLevelFromBoxMonExp(&tempMon);
}

static u8 GetNumLevelsGainedFromSteps(struct DaycareMon *daycareMon)
{
    u8 levelBefore;
    u8 levelAfter;

    levelBefore = GetLevelFromBoxMonExp(&daycareMon->mon);
    levelAfter = GetLevelAfterDaycareSteps(&daycareMon->mon, daycareMon->steps);
    if (levelAfter > GetCurrentLevelCap())
        levelAfter = GetCurrentLevelCap();
    return levelAfter - levelBefore;
}

static u8 GetNumLevelsGainedForDaycareMon(struct DaycareMon *daycareMon)
{
    u8 numLevelsGained = GetNumLevelsGainedFromSteps(daycareMon);
    ConvertIntToDecimalStringN(gStringVar2, numLevelsGained, STR_CONV_MODE_LEFT_ALIGN, 2);
    GetBoxMonNickname(&daycareMon->mon, gStringVar1);
    return numLevelsGained;
}

static u32 PrepareDaycareCostStringForSelectedMon(struct DaycareMon *daycareMon)
{
    u32 cost;

    u8 numLevelsGained = GetNumLevelsGainedFromSteps(daycareMon);
    GetBoxMonNickname(&daycareMon->mon, gStringVar1);
    cost = 100 + 100 * numLevelsGained;
    ConvertIntToDecimalStringN(gStringVar2, cost, STR_CONV_MODE_LEFT_ALIGN, 5);
    return cost;
}

static u16 PrepareDaycareCostStringForMon(struct DayCare *daycare, u8 slotId)
{
    return PrepareDaycareCostStringForSelectedMon(&daycare->mons[slotId]);
}

void GetDaycareCostAndPrepareString(void)
{
    gSpecialVar_0x8005 = PrepareDaycareCostStringForMon(&gSaveBlock1Ptr->daycare, gSpecialVar_0x8004);
}

static void UNUSED Debug_AddDaycareSteps(u16 numSteps)
{
    gSaveBlock1Ptr->daycare.mons[0].steps += numSteps;
    gSaveBlock1Ptr->daycare.mons[1].steps += numSteps;
}

u8 GetNumLevelsGainedFromDaycare(void)
{
    if (GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[gSpecialVar_0x8004].mon, MON_DATA_SPECIES) != 0)
        return GetNumLevelsGainedForDaycareMon(&gSaveBlock1Ptr->daycare.mons[gSpecialVar_0x8004]);

    return 0;
}

static bool32 _CheckDaycareMonReceivedMail(struct DayCare *daycare, u32 slot)
{
    u8 nickname[max(32, POKEMON_NAME_BUFFER_SIZE)];
    struct DaycareMon *daycareMon = &daycare->mons[slot];

    GetBoxMonNickname(&daycareMon->mon, nickname);
    if (daycareMon->mail.message.itemId != ITEM_NONE
        && (StringCompareWithoutExtCtrlCodes(nickname, daycareMon->mail.monName) != 0
         || StringCompareWithoutExtCtrlCodes(gSaveBlock2Ptr->playerName, daycareMon->mail.otName) != 0))
    {
        StringCopy(gStringVar1, nickname);
        TVShowConvertInternationalString(gStringVar2, daycareMon->mail.otName, daycareMon->mail.gameLanguage);
        TVShowConvertInternationalString(gStringVar3, daycareMon->mail.monName, daycareMon->mail.monLanguage);
        return TRUE;
    }
    return FALSE;
}

bool32 CheckDaycareMonReceivedMail(void)
{
    return _CheckDaycareMonReceivedMail(&gSaveBlock1Ptr->daycare, gSpecialVar_0x8004);
}

static void ClearDaycareMonMail(struct DaycareMail *mail)
{
    s32 i;

    for (i = 0; i < PLAYER_NAME_LENGTH + 1; i++)
        mail->otName[i] = 0;
    for (i = 0; i < VANILLA_POKEMON_NAME_LENGTH + 1; i++)
        mail->monName[i] = 0;

    ClearMail(&mail->message);
}

// Determines what the species of an Egg would be based on the given species.
// It determines this by working backwards through the evolution chain of the
// given species.
enum Species GetEggSpecies(enum Species species)
{
    int i, j, k;
    bool8 found;

    // Working backwards up to 5 times seems arbitrary, since the maximum number
    // of times would only be 3 for 3-stage evolutions.
    for (i = 0; i < 5; i++)
    {
        found = FALSE;
        for (j = 1; j < NUM_SPECIES; j++)
        {
            if (!IsSpeciesEnabled(j))
                continue;
            const struct Evolution *evolutions = GetSpeciesEvolutions(j);
            if (evolutions == NULL)
                continue;
            for (k = 0; evolutions[k].method != EVOLUTIONS_END; k++)
            {
                if (SanitizeSpeciesId(evolutions[k].targetSpecies) == species)
                {
                    species = j;
                    found = TRUE;
                    break;
                }
            }

            if (found)
                break;
        }

        if (j == NUM_SPECIES)
            break;
    }

    return species;
}

static u32 GetEligibleParentToPassNatureVanilla(struct DayCare *daycare)
{
    u32 slot = DAYCARE_MON_COUNT;
    u32 dittoCount = 0;
    u32 i;
    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetBoxMonGender(&daycare->mons[i].mon) == MON_FEMALE)
            slot = i;
    }

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        enum Species species = GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES);
        if (IS_DITTO(species))
        {
            slot = i;
            dittoCount++;
        }
    }

    if (slot == DAYCARE_MON_COUNT)
        return DAYCARE_MON_COUNT;

    if (dittoCount == DAYCARE_MON_COUNT)
        slot = RandomPercentage(RNG_DAYCARE_PICK_NATURE_PARENT, 50);

    if (GetBoxMonData(&daycare->mons[slot].mon, MON_DATA_HELD_ITEM) != ITEM_EVERSTONE)
        return DAYCARE_MON_COUNT;

    if (RandomPercentage(RNG_DAYCARE_NATURE_INHERITANCE, 50))
        return slot;

    return DAYCARE_MON_COUNT;
}

static u32 GetEligibleParentToPassNatureGen4(struct DayCare *daycare)
{
    for (u32 i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetItemHoldEffect(GetBoxMonData(&daycare->mons[i].mon, MON_DATA_HELD_ITEM)) == HOLD_EFFECT_PREVENT_EVOLVE)
        {
            if (RandomPercentage(RNG_DAYCARE_NATURE_INHERITANCE, 50))
                return i;
        }
    }
    return DAYCARE_MON_COUNT;
}

static u32 GetEligibleParentToPassNatureGen5(struct DayCare *daycare)
{
    u32 slot = 0;
    u32 everstoneCount = 0;

    for (u32 i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetItemHoldEffect(GetBoxMonData(&daycare->mons[i].mon, MON_DATA_HELD_ITEM)) != HOLD_EFFECT_PREVENT_EVOLVE)
            continue;

        slot = i;
        everstoneCount++;
    }

    if (everstoneCount == 0)
        return DAYCARE_MON_COUNT;

    if (everstoneCount == DAYCARE_MON_COUNT)
        slot = RandomPercentage(RNG_DAYCARE_PICK_NATURE_PARENT, 50);

    return slot;
}

u32 GetChildNature(struct DayCare *daycare)
{
    u32 slot;

    if (GetConfig(NATURE_INHERITANCE) < GEN_3)
        return NATURE_RANDOM;

    if (GetConfig(NATURE_INHERITANCE) == GEN_3)
        slot = GetEligibleParentToPassNatureVanilla(daycare);
    else if (GetConfig(NATURE_INHERITANCE) == GEN_4)
        slot = GetEligibleParentToPassNatureGen4(daycare);
    else
        slot = GetEligibleParentToPassNatureGen5(daycare);

    if (slot == DAYCARE_MON_COUNT)
        return NATURE_RANDOM;

    return GetBoxMonNature(&daycare->mons[slot].mon);
}

#ifdef NATIVE
void GetDaycareNatureSourceWeights(const struct DayCare *daycare, u32 generation, u8 weights[3])
{
    struct DayCare copy = *daycare;
    bool32 eligible[DAYCARE_MON_COUNT] = {FALSE, FALSE};
    DaycareGuardOutput(weights, 3 * sizeof(*weights));
    weights[0] = weights[1] = 0; weights[2] = 4;
    if (generation < GEN_3) return;
    if (generation == GEN_3)
    {
        u32 slot = DAYCARE_MON_COUNT, dittoCount = 0;
        for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
            if (GetBoxMonGender(&copy.mons[i].mon) == MON_FEMALE) slot = i;
        for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
            if (IS_DITTO(GetBoxMonData(&copy.mons[i].mon, MON_DATA_SPECIES))) { slot = i; ++dittoCount; }
        if (dittoCount == DAYCARE_MON_COUNT)
        {
            for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
                if (GetBoxMonData(&copy.mons[i].mon, MON_DATA_HELD_ITEM) == ITEM_EVERSTONE) weights[i] = 1;
        }
        else if (slot < DAYCARE_MON_COUNT && GetBoxMonData(&copy.mons[slot].mon, MON_DATA_HELD_ITEM) == ITEM_EVERSTONE)
            weights[slot] = 2;
    }
    else
    {
        for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
            eligible[i] = GetItemHoldEffect(GetBoxMonData(&copy.mons[i].mon, MON_DATA_HELD_ITEM)) == HOLD_EFFECT_PREVENT_EVOLVE;
        if (generation == GEN_4)
        {
            if (eligible[0]) weights[0] = 2;
            if (eligible[1]) weights[1] = eligible[0] ? 1 : 2;
        }
        else if (eligible[0] && eligible[1]) weights[0] = weights[1] = 2;
        else if (eligible[0]) weights[0] = 4;
        else if (eligible[1]) weights[1] = 4;
    }
    weights[2] = 4 - weights[0] - weights[1];
}
#endif

static enum Species DetermineFinalEggSpecies(struct DayCare *daycare, u8 *parentSlots);

static void _TriggerPendingDaycareEgg(struct DayCare *daycare)
{
    if (!FlagGet(FLAG_PENDING_DAYCARE_EGG))
    {
        /* The variant is fixed when the egg becomes pending, so previews and
         * acceptance see the same species. Ordinary pairs consume no extra
         * RNG, and an already pending egg is never rerolled. */
        struct DayCare copy = *daycare;
        if (GetBoxMonData(&copy.mons[0].mon, MON_DATA_SANITY_HAS_SPECIES)
            && GetBoxMonData(&copy.mons[1].mon, MON_DATA_SANITY_HAS_SPECIES))
        {
            u8 slots[2] = {0, 1};
            copy.offspringPersonality &= ~EGG_GENDER_MALE;
            enum Species first = DetermineFinalEggSpecies(&copy, slots);
            slots[0] = 0; slots[1] = 1;
            copy.offspringPersonality |= EGG_GENDER_MALE;
            enum Species second = DetermineFinalEggSpecies(&copy, slots);
            if (first != second)
                daycare->offspringPersonality = (daycare->offspringPersonality & ~EGG_GENDER_MALE)
                    | (Random() & EGG_GENDER_MALE);
        }
    }
    FlagSet(FLAG_PENDING_DAYCARE_EGG);
}

void TriggerPendingDaycareEgg(void)
{
#ifdef NATIVE
    bool32 before = FlagGet(FLAG_PENDING_DAYCARE_EGG);
#endif
    _TriggerPendingDaycareEgg(&gSaveBlock1Ptr->daycare);
#ifdef NATIVE
    if (!before) ModsBreedingPendingChanged(&gSaveBlock1Ptr->daycare, RHH_DAYCARE_PENDING_MANUAL, FALSE, TRUE);
#endif
}

#ifdef NATIVE
void TriggerPendingDaycareEggForMod(void)
{
    bool32 before = FlagGet(FLAG_PENDING_DAYCARE_EGG);
    _TriggerPendingDaycareEgg(&gSaveBlock1Ptr->daycare);
    if (!before) ModsBreedingPendingChanged(&gSaveBlock1Ptr->daycare, RHH_DAYCARE_PENDING_REQUEST, FALSE, TRUE);
}
#endif

void GetDaycareIVRules(struct DayCare *daycare, struct DaycareIVRules *out)
{
#ifdef NATIVE
    DaycareGuardOutput(out, sizeof(*out));
#endif
    *out = (struct DaycareIVRules){.inheritedCount = 3, .powerStats = {NUM_STATS, NUM_STATS}};
    for (u32 i = 0; i < DAYCARE_MON_COUNT; ++i)
    {
        enum Item item = GetBoxMonData(&daycare->mons[i].mon, MON_DATA_HELD_ITEM);
        if (item == ITEM_DESTINY_KNOT) out->inheritedCount = 5;
        if (GetItemHoldEffect(item) == HOLD_EFFECT_POWER_ITEM)
        {
            out->powerStats[i] = GetItemSecondaryId(item);
            ++out->powerItemCount;
        }
    }
}

void InheritIVs(struct Pokemon *egg, struct DayCare *daycare)
{
    u32 i, iv, slot;
    enum Stat powerStat;
    u32 start = 0;
    struct DaycareIVRules rules;
    u8 selectedIvs[NUM_STATS] = {0};
    u8 availableIVs[NUM_STATS];
    GetDaycareIVRules(daycare, &rules);
    u32 howManyIVs = rules.inheritedCount;
#ifdef NATIVE
    howManyIVs = ModsBreedingIvCount(daycare, howManyIVs);
#endif
    u32 randParents = RandomUniform(RNG_DAYCARE_PICK_IVS_PARENT, 0, howManyIVs == NUM_STATS ? 63 : 31);
    u32 randIv = RandomUniform(RNG_DAYCARE_INHERITED_STATS, 0, 719); // 6! is the maximum number of stat combination, -1 because 0 is included

    for (i = 0; i < NUM_STATS; i++)
    {
        availableIVs[i] = i;
    }

    if (howManyIVs && rules.powerItemCount > 0)
    {
        slot = rules.powerStats[1] < NUM_STATS ? 1 : 0;
        if (rules.powerItemCount == 2)
        {
            slot = randParents & 1;
            randParents >>= 1;
        }
        powerStat = rules.powerStats[slot];
        iv = GetBoxMonData(&daycare->mons[slot].mon, MON_DATA_HP_IV + powerStat);
        SetMonData(egg, MON_DATA_HP_IV + powerStat, &iv);
        RemoveIVIndexFromList(availableIVs, powerStat);
        start++;
    }

    for (i = start; i < howManyIVs; i++)
    {
        u32 index = randIv % (NUM_STATS - i);
        randIv = randIv / (NUM_STATS - i);
        selectedIvs[i] = availableIVs[index];
        RemoveIVIndexFromList(availableIVs, index);
    }

    for (i = start; i < howManyIVs; i++)
    {
        slot = randParents & 1;
        randParents >>= 1;
        iv = GetBoxMonData(&daycare->mons[slot].mon, MON_DATA_HP_IV + selectedIvs[i]);
        SetMonData(egg, MON_DATA_HP_IV + selectedIvs[i], &iv);
    }

}

/* Two equally weighted units: parent 0, parent 1, or the default Poké Ball.
 * Shared with the native breeding preview so generation and odds agree. */
void GetDaycareBallSourceWeights(const struct DayCare *daycare, u32 generation, u8 weights[3])
{
    struct DayCare copy = *daycare;
#ifdef NATIVE
    DaycareGuardOutput(weights, 3 * sizeof(*weights));
#endif
    weights[0] = weights[1] = 0; weights[2] = 2;
    if (generation >= GEN_7)
    {
        s32 ballSlot = -1;
        for (u32 i = 0; i < DAYCARE_MON_COUNT; i++)
        {
            if (GetBoxMonGender(&copy.mons[i].mon) == MON_FEMALE)
                ballSlot = i;
            enum Species species = GetBoxMonData(&copy.mons[i].mon, MON_DATA_SPECIES);
            if (ballSlot < 0 && !IS_DITTO(species))
                ballSlot = i;
        }
        enum Species species0 = GetBoxMonData(&copy.mons[0].mon, MON_DATA_SPECIES);
        enum Species species1 = GetBoxMonData(&copy.mons[1].mon, MON_DATA_SPECIES);
        if (ballSlot < 0)
            ballSlot = 0;
        if (GET_BASE_SPECIES_ID(species0) == GET_BASE_SPECIES_ID(species1))
        { weights[0] = weights[1] = 1; weights[2] = 0; }
        else { weights[ballSlot] = 2; weights[2] = 0; }
    }
    else if (generation == GEN_6)
    {
        for (u32 i = 0; i < DAYCARE_MON_COUNT; i++)
            if (GetBoxMonGender(&copy.mons[i].mon) == MON_FEMALE)
            { weights[0] = weights[1] = 0; weights[i] = 2; weights[2] = 0; }
    }
}

static void InheritPokeball(struct Pokemon *egg, struct DayCare *daycare)
{
    u8 weights[3]; GetDaycareBallSourceWeights(daycare, GetConfig(BALL_INHERITANCE), weights);
    enum PokeBall inheritBall = BALL_POKE;
    if (weights[0] && weights[1])
        inheritBall = GetBoxMonData(&daycare->mons[RandomPercentage(RNG_DAYCARE_PICK_BALL_PARENT, 50)].mon, MON_DATA_POKEBALL);
    else if (weights[0] || weights[1])
        inheritBall = GetBoxMonData(&daycare->mons[weights[1] ? 1 : 0].mon, MON_DATA_POKEBALL);
    if (inheritBall == BALL_MASTER || inheritBall == BALL_CHERISH || inheritBall == BALL_STRANGE)
        inheritBall = BALL_POKE;
    SetMonData(egg, MON_DATA_POKEBALL, &inheritBall);
}

void GetDaycareAbilityInheritance(const struct DayCare *daycare, u32 generation, s32 *parent, u32 *ability, u32 *percent)
{
    struct DayCare copy = *daycare;
#ifdef NATIVE
    DaycareGuardOutput(parent, sizeof(*parent));
    DaycareGuardOutput(ability, sizeof(*ability));
    DaycareGuardOutput(percent, sizeof(*percent));
#endif
    *parent = -1; *ability = 0; *percent = 0;
    if (generation < GEN_5)
        return;
    if (generation == GEN_5)
    {
        u32 gender0 = GetBoxMonGender(&copy.mons[0].mon);
        u32 gender1 = GetBoxMonGender(&copy.mons[1].mon);
        if (gender0 == MON_FEMALE && gender1 == MON_MALE)
            *parent = 0;
        else if (gender1 == MON_FEMALE && gender0 == MON_MALE)
            *parent = 1;
    }
    else
    {
        s32 abilitySlot = -1;
        for (u32 i = 0; i < DAYCARE_MON_COUNT; i++)
        {
            if (GetBoxMonGender(&copy.mons[i].mon) == MON_FEMALE)
                abilitySlot = i;
            enum Species species = GetBoxMonData(&copy.mons[i].mon, MON_DATA_SPECIES);
            if (abilitySlot < 0 && !IS_DITTO(species))
                abilitySlot = i;
        }
        if (abilitySlot < 0)
            abilitySlot = 0;
        *parent = abilitySlot;
    }
    if (*parent < 0)
        return;
    *ability = GetBoxMonData(&copy.mons[*parent].mon, MON_DATA_ABILITY_NUM);
    *percent = *ability == 2 ? (generation == GEN_5 ? 80 : 60) : *ability < 2 ? 80 : 100;
}

void InheritAbility(struct Pokemon *egg, struct DayCare *daycare)
{
    s32 parent; u32 ability, percent;
    GetDaycareAbilityInheritance(daycare, GetConfig(ABILITY_INHERITANCE), &parent, &ability, &percent);
    if (parent < 0) return;
    /* Preserve the engine's no-draw behavior for an out-of-range parent slot. */
    if (ability < 3 && !RandomPercentage(RNG_DAYCARE_ABILITY_INHERITANCE, percent)) return;
    SetMonData(egg, MON_DATA_ABILITY_NUM, &ability);
}

#define ADD_OR_REPLACE_MOVE(move) if (GiveMoveToMon(egg, move) == MON_HAS_MAX_MOVES) {DeleteFirstMoveAndGiveMoveToMon(egg, move);}

static void GiveParentEggMoves(struct Pokemon *egg, enum Move *parentMoves, enum Species species)
{
    const u16 *eggMoveLearnset = GetSpeciesEggMoves(species);
    for (u32 i = 0; i < MAX_MON_MOVES; i++)
    {
        if (parentMoves[i] == MOVE_NONE)
            break;
        for (u32 j = 0; eggMoveLearnset[j] != MOVE_UNAVAILABLE; j++)
        {
            if (parentMoves[i] == eggMoveLearnset[j])
            {
                ADD_OR_REPLACE_MOVE(parentMoves[i])
                break;
            }
        }
    }
}

static void GiveParentTmMoves(struct Pokemon *egg, enum Move *parentMoves, enum Species species)
{
    for (u32 i = 0; i < MAX_MON_MOVES; i++)
    {
        if (parentMoves[i] == MOVE_NONE)
            break;
        for (u32 j = 0; j < NUM_ALL_MACHINES; j++)
        {
            enum Move moveId = GetTMHMMoveId(j + 1);
            if (parentMoves[i] == moveId)
            {
                if (CanLearnTeachableMove(species, moveId))
                    ADD_OR_REPLACE_MOVE(parentMoves[i])
                break;
            }
        }
    }
}

static void GiveParentSharedLevelUpMoves(struct Pokemon *egg, enum Move *fatherMoves, enum Move *motherMoves, enum Species species)
{
    u32 i, j;
    enum Move sharedMoves[MAX_MON_MOVES] = {0};
    u32 numSharedMoves = 0;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (fatherMoves[i] == MOVE_NONE)
            break;
        for (j = 0; j < MAX_MON_MOVES; j++)
        {
            if (motherMoves[j] == MOVE_NONE)
                break;
            if (fatherMoves[i] == motherMoves[j])
            {
                sharedMoves[numSharedMoves++] = fatherMoves[i];
                break;
            }
        }
    }

    const struct LevelUpMove *levelupLearnset = GetSpeciesLevelUpLearnset(species);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (sharedMoves[i] == MOVE_NONE)
            break;
        for (j = 0; levelupLearnset[j].move != LEVEL_UP_MOVE_END; j++)
        {
            if (sharedMoves[i] == levelupLearnset[j].move)
            {
                ADD_OR_REPLACE_MOVE(sharedMoves[i])
                break;
            }
        }
    }
}

static const struct {
  enum Species offspring;
  enum Item item;
  enum Move move;
} sBreedingSpecialMoveItemTable[] =
{
    // Offspring,    Item,            Move
    { SPECIES_PICHU, ITEM_LIGHT_BALL, MOVE_VOLT_TACKLE },
};

static void GiveMoveIfParentHeldItem(struct Pokemon *egg, struct BoxPokemon *father, struct BoxPokemon *mother, enum Species species)
{
    enum Item motherItem = GetBoxMonData(mother, MON_DATA_HELD_ITEM);
    enum Item fatherItem = GetBoxMonData(father, MON_DATA_HELD_ITEM);

    for (u32 i = 0; i < ARRAY_COUNT(sBreedingSpecialMoveItemTable); i++)
    {
        if (sBreedingSpecialMoveItemTable[i].offspring == species
            && (motherItem == sBreedingSpecialMoveItemTable[i].item ||
                fatherItem == sBreedingSpecialMoveItemTable[i].item))
        {
            ADD_OR_REPLACE_MOVE(sBreedingSpecialMoveItemTable[i].move)
        }
    }
}

static void BuildEggMoveset(struct Pokemon *egg, struct BoxPokemon *father, struct BoxPokemon *mother)
{
    enum Species eggSpecies = GetMonData(egg, MON_DATA_SPECIES); 
    enum Move fatherMoves[MAX_MON_MOVES];
    enum Move motherMoves[MAX_MON_MOVES];

    for (u32 i = 0; i < MAX_MON_MOVES; i++)
    {
        fatherMoves[i] = GetBoxMonData(father, MON_DATA_MOVE1 + i);
        motherMoves[i] = GetBoxMonData(mother, MON_DATA_MOVE1 + i);
    }

    if (P_MOVE_INHERITANCE < GEN_6)
    {
        GiveMonInitialMoveset(egg);
        GiveParentEggMoves(egg, fatherMoves, eggSpecies);
        GiveParentTmMoves(egg, fatherMoves, eggSpecies);
        GiveParentSharedLevelUpMoves(egg, fatherMoves, motherMoves, eggSpecies);
        GiveMoveIfParentHeldItem(egg, father, mother, eggSpecies);
    }
    else
    {
        GiveMonInitialMoveset(egg);
        GiveParentSharedLevelUpMoves(egg, fatherMoves, motherMoves, eggSpecies);
        GiveParentEggMoves(egg, fatherMoves, eggSpecies);
        GiveParentEggMoves(egg, motherMoves, eggSpecies);
        GiveMoveIfParentHeldItem(egg, father, mother, eggSpecies);
    }
}

#ifdef NATIVE
bool32 GetDaycareMovePreview(const struct DayCare *daycare, enum Species species, const u8 *parentSlots, u16 moves[4])
{
    if (!daycare || !parentSlots || !moves || !species || species >= NUM_SPECIES
        || parentSlots[0] >= DAYCARE_MON_COUNT || parentSlots[1] >= DAYCARE_MON_COUNT
        || parentSlots[0] == parentSlots[1]) return FALSE;
    struct DayCare copy = *daycare;
    struct Pokemon egg = {0};
    /* A fixed identity and box-only construction avoid all egg RNG and stat
     * side effects. BuildEggMoveset reads species, level and copied parents. */
    CreateBoxMon(&egg.box, species, EGG_HATCH_LEVEL, 0, OTID_STRUCT_PRESET(1));
    BuildEggMoveset(&egg, &copy.mons[parentSlots[1]].mon, &copy.mons[parentSlots[0]].mon);
    u16 result[MAX_MON_MOVES];
    for (u32 i = 0; i < MAX_MON_MOVES; ++i) result[i] = GetMonData(&egg, MON_DATA_MOVE1 + i);
    DaycareGuardOutput(moves, sizeof(result));
    memcpy(moves, result, sizeof(result));
    return TRUE;
}
#endif

static void RemoveEggFromDayCare(struct DayCare *daycare)
{
    FlagClear(FLAG_PENDING_DAYCARE_EGG);
    daycare->stepCounter = 0;
}

void RejectEggFromDayCare(void)
{
#ifdef NATIVE
    bool32 before = FlagGet(FLAG_PENDING_DAYCARE_EGG);
#endif
    RemoveEggFromDayCare(&gSaveBlock1Ptr->daycare);
#ifdef NATIVE
    if (before) ModsBreedingPendingChanged(&gSaveBlock1Ptr->daycare, RHH_DAYCARE_PENDING_REJECTED, TRUE, FALSE);
#endif
}

static void AlterEggSpeciesWithIncenseItem(enum Species *species, struct DayCare *daycare)
{
    u32 i;
    enum Item motherItem, fatherItem;
    motherItem = GetBoxMonData(&daycare->mons[0].mon, MON_DATA_HELD_ITEM);
    fatherItem = GetBoxMonData(&daycare->mons[1].mon, MON_DATA_HELD_ITEM);

    for (i = 0; i < ARRAY_COUNT(sIncenseBabyTable); i++)
    {
        if (sIncenseBabyTable[i].babySpecies == *species && motherItem != sIncenseBabyTable[i].item && fatherItem != sIncenseBabyTable[i].item)
        {
            *species = sIncenseBabyTable[i].currSpecies;
            break;
        }
    }
}

STATIC_ASSERT(P_SCATTERBUG_LINE_FORM_BREED == SPECIES_SCATTERBUG_ICY_SNOW || (P_SCATTERBUG_LINE_FORM_BREED >= SPECIES_SCATTERBUG_POLAR && P_SCATTERBUG_LINE_FORM_BREED <= SPECIES_SCATTERBUG_POKEBALL), ScatterbugLineFormBreedMustBeAValidScatterbugForm);

static enum Species DetermineEggSpeciesAndParentSlots(struct DayCare *daycare, u8 *parentSlots)
{
    u32 i;
    enum Species species[DAYCARE_MON_COUNT];
    enum Species eggSpecies;
    bool32 hasMotherEverstone, hasFatherEverstone, motherIsForeign, fatherIsForeign;
    bool32 motherEggSpecies, fatherEggSpecies;
    enum Region currentRegion = GetCurrentRegion();

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        species[i] = GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES);
        if (IS_DITTO(species[i]))
        {
            parentSlots[0] = i ^ 1;
            parentSlots[1] = i;
        }
        else if (GetBoxMonGender(&daycare->mons[i].mon) == MON_FEMALE)
        {
            parentSlots[0] = i;
            parentSlots[1] = i ^ 1;
        }
    }

    motherEggSpecies = GetEggSpecies(species[parentSlots[0]]);
    fatherEggSpecies = GetEggSpecies(species[parentSlots[1]]);
    hasMotherEverstone = GetItemHoldEffect(GetBoxMonData(&daycare->mons[parentSlots[0]].mon, MON_DATA_HELD_ITEM)) == HOLD_EFFECT_PREVENT_EVOLVE;
    hasFatherEverstone = GetItemHoldEffect(GetBoxMonData(&daycare->mons[parentSlots[1]].mon, MON_DATA_HELD_ITEM)) == HOLD_EFFECT_PREVENT_EVOLVE;
    motherIsForeign = IsSpeciesForeignRegionalForm(motherEggSpecies, currentRegion);
    fatherIsForeign = IsSpeciesForeignRegionalForm(fatherEggSpecies, currentRegion);

    if (hasMotherEverstone)
        eggSpecies = motherEggSpecies;
    else if (fatherIsForeign && hasFatherEverstone)
        eggSpecies = fatherEggSpecies;
    else if (motherIsForeign)
        eggSpecies = GetRegionalFormByRegion(motherEggSpecies, currentRegion);
    else
        eggSpecies = motherEggSpecies;

    if (eggSpecies == SPECIES_NIDORAN_F && daycare->offspringPersonality & EGG_GENDER_MALE)
        eggSpecies = SPECIES_NIDORAN_M;
    else if (eggSpecies == SPECIES_ILLUMISE && daycare->offspringPersonality & EGG_GENDER_MALE)
        eggSpecies = SPECIES_VOLBEAT;
    else if (P_NIDORAN_M_DITTO_BREED >= GEN_5 && eggSpecies == SPECIES_NIDORAN_M && !(daycare->offspringPersonality & EGG_GENDER_MALE))
        eggSpecies = SPECIES_NIDORAN_F;
    else if (P_NIDORAN_M_DITTO_BREED >= GEN_5 && eggSpecies == SPECIES_VOLBEAT && !(daycare->offspringPersonality & EGG_GENDER_MALE))
        eggSpecies = SPECIES_ILLUMISE;
    else if (eggSpecies == SPECIES_MANAPHY)
        eggSpecies = SPECIES_PHIONE;
    else if (GET_BASE_SPECIES_ID(eggSpecies) == SPECIES_ROTOM)
        eggSpecies = SPECIES_ROTOM;
    else if (GET_BASE_SPECIES_ID(eggSpecies) == SPECIES_SCATTERBUG)
        eggSpecies = P_SCATTERBUG_LINE_FORM_BREED;
    else if (GET_BASE_SPECIES_ID(eggSpecies) == SPECIES_FURFROU)
        eggSpecies = SPECIES_FURFROU;
    else if (eggSpecies == SPECIES_SINISTEA_ANTIQUE)
        eggSpecies = SPECIES_SINISTEA_PHONY;
    else if (eggSpecies == SPECIES_POLTCHAGEIST_ARTISAN)
        eggSpecies = SPECIES_POLTCHAGEIST_COUNTERFEIT;
    // To avoid single-stage Totem Pokémon to breed more of themselves.
    else if (eggSpecies == SPECIES_MIMIKYU_TOTEM_DISGUISED)
        eggSpecies = SPECIES_MIMIKYU_DISGUISED;
    else if (eggSpecies == SPECIES_TOGEDEMARU_TOTEM)
        eggSpecies = SPECIES_TOGEDEMARU;

    // Make Ditto the "mother" slot if the other daycare mon is male.
    if (IS_DITTO(species[parentSlots[1]]) && GetBoxMonGender(&daycare->mons[parentSlots[0]].mon) != MON_FEMALE)
    {
        u8 ditto = parentSlots[1];
        parentSlots[1] = parentSlots[0];
        parentSlots[0] = ditto;
    }

    return eggSpecies;
}

static enum Species DetermineFinalEggSpecies(struct DayCare *daycare, u8 *parentSlots)
{
    enum Species species = DetermineEggSpeciesAndParentSlots(daycare, parentSlots);
    if (P_INCENSE_BREEDING < GEN_9)
        AlterEggSpeciesWithIncenseItem(&species, daycare);
    return species;
}

#ifdef NATIVE
enum Species GetDaycareOffspringCandidate(struct DayCare *daycare, bool32 maleVariant, u8 *parentSlots)
{
    struct DayCare copy = *daycare;
    if (maleVariant) copy.offspringPersonality |= EGG_GENDER_MALE;
    else copy.offspringPersonality &= ~EGG_GENDER_MALE;
    DaycareGuardOutput(parentSlots, DAYCARE_MON_COUNT * sizeof(*parentSlots));
    parentSlots[0] = 0; parentSlots[1] = 1;
    return DetermineFinalEggSpecies(&copy, parentSlots);
}
#endif

static void ConstructDaycareEgg(struct DayCare *daycare, struct Pokemon *egg, u32 compatibility,
                                u32 *originalSpecies, u8 parentSlots[DAYCARE_MON_COUNT])
{
    enum Species species = DetermineFinalEggSpecies(daycare, parentSlots);
    *originalSpecies = species;
#ifdef NATIVE
    species = ModsBreedingOffspring(daycare, species, compatibility, parentSlots);
#else
    (void)compatibility;
#endif
    SetInitialEggData(egg, species, daycare);
#ifdef NATIVE
    u8 initialIVs[NUM_STATS];
    for (u32 i = 0; i < NUM_STATS; ++i) initialIVs[i] = GetMonData(egg, MON_DATA_HP_IV + i);
#endif
    InheritIVs(egg, daycare);
#ifdef NATIVE
    ModsBreedingInheritIVs(daycare, egg, *originalSpecies, compatibility, parentSlots, initialIVs);
#endif
    InheritPokeball(egg, daycare);
    BuildEggMoveset(egg, &daycare->mons[parentSlots[1]].mon, &daycare->mons[parentSlots[0]].mon);
#ifdef NATIVE
    ModsBreedingInheritMoves(daycare, egg, *originalSpecies, compatibility, parentSlots);
#endif
    InheritAbility(egg, daycare);
#ifdef NATIVE
    ModsBreedingEggTraits(daycare, egg, *originalSpecies, compatibility, parentSlots);
#endif
}

#ifdef NATIVE
bool32 GetDaycareExactEggPreview(struct Pokemon *egg, u32 *originalSpecies, u8 parentSlots[DAYCARE_MON_COUNT])
{
    if (!egg || !originalSpecies || !parentSlots || !gSaveBlock1Ptr || !IsEggPending(&gSaveBlock1Ptr->daycare)) return FALSE;
    struct DayCare *daycare = &gSaveBlock1Ptr->daycare;
    u32 compatibility = GetDaycareCompatibilityScore(daycare);
    if (compatibility == PARENTS_INCOMPATIBLE) return FALSE;
    DaycareGuardOutput(egg, sizeof(*egg));
    DaycareGuardOutput(originalSpecies, sizeof(*originalSpecies));
    DaycareGuardOutput(parentSlots, DAYCARE_MON_COUNT * sizeof(*parentSlots));
    parentSlots[0] = 0; parentSlots[1] = 1;
    ConstructDaycareEgg(daycare, egg, compatibility, originalSpecies, parentSlots);
    bool8 isEgg = TRUE; SetMonData(egg, MON_DATA_IS_EGG, &isEgg);
    return TRUE;
}
#endif

static bool32 _GiveEggFromDaycare(struct DayCare *daycare, u32 *receivedSlot
#ifdef NATIVE
    , bool32 (*validate)(struct Pokemon *, u32, const u8 *, void *), void *context
#endif
    )
{
    struct Pokemon egg;
    u32 originalSpecies;
    u8 parentSlots[DAYCARE_MON_COUNT] = {0, 1};
    bool8 isEgg;
    u8 compatibility = GetDaycareCompatibilityScore(daycare);

    if (compatibility == PARENTS_INCOMPATIBLE)
        return FALSE;

    u32 partyCount = 0;
    for (u32 slot = 0; slot < PARTY_SIZE; ++slot)
        if (GetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_SANITY_HAS_SPECIES)) ++partyCount;
    if (partyCount == PARTY_SIZE) return FALSE;

#ifdef NATIVE
    struct ModsBreedingRngScope rngScope;
    ModsBreedingRngBegin(daycare, &rngScope);
#endif
    ConstructDaycareEgg(daycare, &egg, compatibility, &originalSpecies, parentSlots);
#ifdef NATIVE
    ModsBreedingRngEnd(&rngScope);
#endif

    isEgg = TRUE;
    SetMonData(&egg, MON_DATA_IS_EGG, &isEgg);
#ifdef NATIVE
    if (validate && !validate(&egg, originalSpecies, parentSlots, context)) return FALSE;
#endif
    CompactPartySlots();
    gParties[B_TRAINER_PLAYER][partyCount] = egg;
    CalculatePlayerPartyCount();
#ifdef NATIVE
    bool32 wasPending = FlagGet(FLAG_PENDING_DAYCARE_EGG);
#endif
    RemoveEggFromDayCare(daycare);
#ifdef NATIVE
    NativeOwnedTrackEggCreated(partyCount);
    ModsBreedingEggCreated(daycare, &gParties[B_TRAINER_PLAYER][partyCount], originalSpecies,
                           compatibility, parentSlots, partyCount, wasPending,
                           gSaveBlock1Ptr && daycare == &gSaveBlock1Ptr->daycare ? ModsCommitDaycareOrdinal() : 0);
    if (wasPending) ModsBreedingPendingChanged(daycare, RHH_DAYCARE_PENDING_ACCEPTED, TRUE, FALSE);
#endif
    if (receivedSlot)
    {
#ifdef NATIVE
        DaycareGuardOutput(receivedSlot, sizeof(*receivedSlot));
#endif
        *receivedSlot = partyCount;
    }
    return TRUE;
}

void CreateEgg(struct Pokemon *mon, enum Species species, bool8 setHotSpringsLocation)
{
    u8 metLevel;
    enum PokeBall ball;
    enum Language language;
    metloc_u8_t metLocation;
    u8 isEgg;

    CreateRandomMonWithIVs(mon, species, EGG_HATCH_LEVEL, USE_RANDOM_IVS);
    metLevel = 0;
    ball = BALL_POKE;
    language = LANGUAGE_JAPANESE;
    SetMonData(mon, MON_DATA_POKEBALL, &ball);
    SetMonData(mon, MON_DATA_NICKNAME, sJapaneseEggNickname);
    SetMonData(mon, MON_DATA_FRIENDSHIP, &gSpeciesInfo[species].eggCycles);
    SetMonData(mon, MON_DATA_MET_LEVEL, &metLevel);
    SetMonData(mon, MON_DATA_LANGUAGE, &language);
    if (setHotSpringsLocation)
    {
        metLocation = METLOC_SPECIAL_EGG;
        SetMonData(mon, MON_DATA_MET_LOCATION, &metLocation);
    }

    isEgg = TRUE;
    SetMonData(mon, MON_DATA_IS_EGG, &isEgg);
}

static void SetInitialEggData(struct Pokemon *mon, enum Species species, struct DayCare *daycare)
{
    u32 personality;
    u8 metLevel;
    u8 language;

    u32 nature = GetChildNature(daycare);
#ifdef NATIVE
    nature = ModsBreedingNature(daycare, nature);
#endif
    personality = GetMonPersonality(species, MON_GENDER_RANDOM, nature, RANDOM_UNOWN_LETTER);
    CreateMonWithIVs(mon, species, EGG_HATCH_LEVEL, personality, OTID_STRUCT_PLAYER_ID, USE_RANDOM_IVS);
    metLevel = 0;
    language = LANGUAGE_JAPANESE;
    SetMonData(mon, MON_DATA_NICKNAME, sJapaneseEggNickname);
    SetMonData(mon, MON_DATA_FRIENDSHIP, &gSpeciesInfo[species].eggCycles);
    SetMonData(mon, MON_DATA_MET_LEVEL, &metLevel);
    SetMonData(mon, MON_DATA_LANGUAGE, &language);
}

void GiveEggFromDaycare(void)
{
    _GiveEggFromDaycare(&gSaveBlock1Ptr->daycare, NULL
#ifdef NATIVE
        , NULL, NULL
#endif
        );
}

#ifdef NATIVE
bool32 GiveEggFromDaycareChecked(u32 *partySlot)
{
    if (!gSaveBlock1Ptr || !partySlot || !IsEggPending(&gSaveBlock1Ptr->daycare)) return FALSE;
    return _GiveEggFromDaycare(&gSaveBlock1Ptr->daycare, partySlot, NULL, NULL);
}
bool32 GiveEggFromDaycareValidated(u32 *partySlot,
    bool32 (*validate)(struct Pokemon *, u32, const u8 *, void *), void *context)
{
    if (!gSaveBlock1Ptr || !partySlot || !validate || !IsEggPending(&gSaveBlock1Ptr->daycare)) return FALSE;
    return _GiveEggFromDaycare(&gSaveBlock1Ptr->daycare, partySlot, validate, context);
}
#endif

static void _IncrementDaycareSteps(struct DayCare *daycare)
{
    u32 daycareParentCount = 0;
    for (u32 i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SANITY_HAS_SPECIES))
        {
            daycare->mons[i].steps++;
            daycareParentCount++;
        }
    }

    if (daycareParentCount != DAYCARE_MON_COUNT || IsEggPending(daycare))
        return;

    if ((daycare->mons[1].steps & 0xFF) == 0xFF)
    {
        u8 compatibility = ModifyBreedingScoreForOvalCharm(GetDaycareCompatibilityScore(daycare));
        if (RandomPercentage(RNG_DAYCARE_MAKE_EGG, compatibility))
        {
            _TriggerPendingDaycareEgg(daycare);
#ifdef NATIVE
            ModsBreedingPendingChanged(daycare, RHH_DAYCARE_PENDING_WALK, FALSE, TRUE);
#endif
        }
    }
}

void IncrementDaycareSteps(void)
{
#if IS_FRLG
    if (GetBoxMonData(&gSaveBlock1Ptr->route5DayCareMon.mon, MON_DATA_SANITY_HAS_SPECIES))
        gSaveBlock1Ptr->route5DayCareMon.steps++;
#endif

    _IncrementDaycareSteps(&gSaveBlock1Ptr->daycare);
}

static inline u32 GetEggCycleLength(void)
{
    switch (P_EGG_CYCLE_LENGTH)
    {
    case GEN_1:
    case GEN_2:
    case GEN_3:
    case GEN_7:
        return 256;
    case GEN_4:
        return 255;
    case GEN_5:
    case GEN_6:
        return 257;
    case GEN_8:
    default:
        return 128;
    }
}

static u32 GetEggCyclesToSubtract(void)
{
    u32 result = 1;
    if (DoesPartyHaveIncubatorMon())
        result += 1;
    return result;
}
#ifdef NATIVE
u32 NativeDaycareHatchCycleLength(void) { return GetEggCycleLength(); }
u32 NativeDaycareHatchCycleDecrement(void) { return GetEggCyclesToSubtract(); }
#endif

static bool32 TryToHatchEgg(struct DayCare *daycare)
{
    daycare->stepCounter++;
    if (daycare->stepCounter < GetEggCycleLength())
        return FALSE;
    daycare->stepCounter = 0;

    u32 eggCycles;
    u8 toSub = GetEggCyclesToSubtract();

    for (u32 i = 0; i < gPartiesCount[B_TRAINER_PLAYER]; i++)
    {
        if (!GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_IS_EGG))
            continue;
        if (GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_SANITY_IS_BAD_EGG))
            continue;

        eggCycles = GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_FRIENDSHIP);
        if (eggCycles != 0)
        {
#ifdef NATIVE
            u32 remaining;
            if (!ModsAdjustEggCycles(i, eggCycles, toSub, &remaining))
                continue;
            struct ModsEggCycleCapture cycleCapture;
            ModsCaptureEggCycle(i, eggCycles, toSub, remaining, &cycleCapture);
            eggCycles = remaining;
#else
            if (eggCycles >= toSub)
                eggCycles -= toSub;
            else
                eggCycles -= 1;
#endif

            SetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_FRIENDSHIP, &eggCycles);
#ifdef NATIVE
            ModsEggCycleCommitted(&cycleCapture);
#endif
        }
        else
        {
#ifdef NATIVE
            if (!ModsCanHatchEgg(i))
                continue;
#endif
            gSpecialVar_0x8004 = i;
            return TRUE;
        }
    }
    return FALSE;
}

bool8 ShouldEggHatch(void)
{
    return TryToHatchEgg(&gSaveBlock1Ptr->daycare);
}

static bool32 IsEggPending(struct DayCare *daycare)
{
    return (FlagGet(FLAG_PENDING_DAYCARE_EGG));
}

// gStringVar1 = first mon's nickname
// gStringVar2 = second mon's nickname
// gStringVar3 = first mon trainer's name
static void _GetDaycareMonNicknames(struct DayCare *daycare)
{
    u8 otName[max(12, PLAYER_NAME_LENGTH + 1)];
    if (GetBoxMonData(&daycare->mons[0].mon, MON_DATA_SPECIES) != 0)
    {
        GetBoxMonNickname(&daycare->mons[0].mon, gStringVar1);
        GetBoxMonData(&daycare->mons[0].mon, MON_DATA_OT_NAME, otName);
        StringCopy(gStringVar3, otName);
    }

    if (GetBoxMonData(&daycare->mons[1].mon, MON_DATA_SPECIES) != 0)
    {
        GetBoxMonNickname(&daycare->mons[1].mon, gStringVar2);
    }
}

enum Species GetSelectedMonNicknameAndSpecies(void)
{
    struct BoxPokemon *boxmon = GetSelectedBoxMonFromPcOrParty();
    GetBoxMonNickname(boxmon, gStringVar1);
    return GetBoxMonData(boxmon, MON_DATA_SPECIES);
}

void GetDaycareMonNicknames(void)
{
    _GetDaycareMonNicknames(&gSaveBlock1Ptr->daycare);
}

u8 GetDaycareState(void)
{
    u8 numMons;
    if (IsEggPending(&gSaveBlock1Ptr->daycare))
    {
        return DAYCARE_EGG_WAITING;
    }

    numMons = CountPokemonInDaycare(&gSaveBlock1Ptr->daycare);
    if (numMons != 0)
    {
        return numMons + 1; // DAYCARE_ONE_MON or DAYCARE_TWO_MONS
    }

    return DAYCARE_NO_MONS;
}

// Determine if the two given egg group lists contain any of the
// same egg groups.
static bool8 EggGroupsOverlap(u16 *eggGroups1, u16 *eggGroups2)
{
    s32 i, j;

    for (i = 0; i < EGG_GROUPS_PER_MON; i++)
    {
        for (j = 0; j < EGG_GROUPS_PER_MON; j++)
        {
            if (eggGroups1[i] == eggGroups2[j])
                return TRUE;
        }
    }

    return FALSE;
}

#ifdef NATIVE
u8 GetDaycareCompatibilityScoreBase(struct DayCare *daycare)
#else
u8 GetDaycareCompatibilityScore(struct DayCare *daycare)
#endif
{
    u32 i;
    u16 eggGroups[DAYCARE_MON_COUNT][EGG_GROUPS_PER_MON];
    enum Species species[DAYCARE_MON_COUNT];
    u32 trainerIds[DAYCARE_MON_COUNT];
    u32 genders[DAYCARE_MON_COUNT];

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        u32 personality;

        species[i] = GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES);
        trainerIds[i] = GetBoxMonData(&daycare->mons[i].mon, MON_DATA_OT_ID);
        personality = GetBoxMonData(&daycare->mons[i].mon, MON_DATA_PERSONALITY);
        genders[i] = GetGenderFromSpeciesAndPersonality(species[i], personality);
        eggGroups[i][0] = gSpeciesInfo[species[i]].eggGroups[0];
        eggGroups[i][1] = gSpeciesInfo[species[i]].eggGroups[1];
    }

    // check unbreedable egg group
    if (eggGroups[0][0] == EGG_GROUP_NO_EGGS_DISCOVERED || eggGroups[1][0] == EGG_GROUP_NO_EGGS_DISCOVERED)
        return PARENTS_INCOMPATIBLE;
    // two Ditto can't breed
    if (eggGroups[0][0] == EGG_GROUP_DITTO && eggGroups[1][0] == EGG_GROUP_DITTO)
        return PARENTS_INCOMPATIBLE;

    // one parent is Ditto
    if (eggGroups[0][0] == EGG_GROUP_DITTO || eggGroups[1][0] == EGG_GROUP_DITTO)
    {
        if (trainerIds[0] == trainerIds[1])
            return PARENTS_LOW_COMPATIBILITY;

        return PARENTS_MED_COMPATIBILITY;
    }
    // neither parent is Ditto
    else
    {
        if (genders[0] == genders[1])
            return PARENTS_INCOMPATIBLE;
        if (genders[0] == MON_GENDERLESS || genders[1] == MON_GENDERLESS)
            return PARENTS_INCOMPATIBLE;
        if (!EggGroupsOverlap(eggGroups[0], eggGroups[1]))
            return PARENTS_INCOMPATIBLE;

        if (species[0] == species[1])
        {
            if (trainerIds[0] == trainerIds[1])
                return PARENTS_MED_COMPATIBILITY; // same species, same trainer

            return PARENTS_MAX_COMPATIBILITY; // same species, different trainers
        }
        else
        {
            if (trainerIds[0] != trainerIds[1])
                return PARENTS_MED_COMPATIBILITY; // different species, different trainers

            return PARENTS_LOW_COMPATIBILITY; // different species, same trainer
        }
    }
}

#ifdef NATIVE
u8 GetDaycareCompatibilityScore(struct DayCare *daycare)
{
    u8 base = GetDaycareCompatibilityScoreBase(daycare);
    return ModsBreedingCompatibility(daycare, base);
}
#endif

static u8 GetDaycareCompatibilityScoreFromSave(void)
{
    // Changed to also store result for scripts
    gSpecialVar_Result = GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare);
    return gSpecialVar_Result;
}

void SetDaycareCompatibilityString(void)
{
    u8 whichString;
    u8 relationshipScore;

    relationshipScore = GetDaycareCompatibilityScoreFromSave();
    whichString = 0;
    if (relationshipScore == PARENTS_INCOMPATIBLE)
        whichString = 3;
    if (relationshipScore == PARENTS_LOW_COMPATIBILITY)
        whichString = 2;
    if (relationshipScore == PARENTS_MED_COMPATIBILITY)
        whichString = 1;
    if (relationshipScore == PARENTS_MAX_COMPATIBILITY)
        whichString = 0;

    StringCopy(gStringVar4, sCompatibilityMessages[whichString]);
}

bool8 NameHasGenderSymbol(const u8 *name, u8 genderRatio)
{
    u8 i;
    u8 symbolsCount[GENDER_COUNT];
    symbolsCount[MALE] = symbolsCount[FEMALE] = 0;

    for (i = 0; name[i] != EOS; i++)
    {
        if (name[i] == CHAR_MALE)
            symbolsCount[MALE]++;
        if (name[i] == CHAR_FEMALE)
            symbolsCount[FEMALE]++;
    }

    if (genderRatio == MON_MALE   && symbolsCount[MALE] != 0 && symbolsCount[FEMALE] == 0)
        return TRUE;
    if (genderRatio == MON_FEMALE && symbolsCount[FEMALE] != 0 && symbolsCount[MALE] == 0)
        return TRUE;

    return FALSE;
}

static u8 *AppendGenderSymbol(u8 *name, u8 gender)
{
    if (gender == MON_MALE)
    {
        if (!NameHasGenderSymbol(name, MON_MALE))
            return StringAppend(name, gText_MaleSymbol4);
    }
    else if (gender == MON_FEMALE)
    {
        if (!NameHasGenderSymbol(name, MON_FEMALE))
            return StringAppend(name, gText_FemaleSymbol4);
    }

    return StringAppend(name, gText_GenderlessSymbol);
}

static u8 *AppendMonGenderSymbol(u8 *name, struct BoxPokemon *boxMon)
{
    return AppendGenderSymbol(name, GetBoxMonGender(boxMon));
}

static void DaycareAddTextPrinter(u8 windowId, const u8 *text, u32 x, u32 y)
{
    struct TextPrinterTemplate printer;

    printer.currentChar = text;
    printer.type = WINDOW_TEXT_PRINTER;
    printer.windowId = windowId;
    printer.fontId = FONT_NORMAL;
    printer.x = x;
    printer.y = y;
    printer.currentX = x;
    printer.currentY = y;
    gTextFlags.useAlternateDownArrow = 0;
    printer.letterSpacing = 0;
    printer.lineSpacing = 1;
    printer.color.accent = 1;
    printer.color.foreground = 2;
    printer.color.background = 1;
    printer.color.shadow = 3;

    AddTextPrinter(&printer, TEXT_SKIP_DRAW, NULL);
}

static void DaycarePrintMonNickname(struct DayCare *daycare, u8 windowId, u32 daycareSlotId, u32 y)
{
    u8 nickname[POKEMON_NAME_BUFFER_SIZE];
    GetBoxMonNickname(&daycare->mons[daycareSlotId].mon, nickname);
    AppendMonGenderSymbol(nickname, &daycare->mons[daycareSlotId].mon);
    DaycareAddTextPrinter(windowId, nickname, 8, y);
}

static void DaycarePrintMonLvl(struct DayCare *daycare, u8 windowId, u32 daycareSlotId, u32 y)
{
    u8 level;
    u32 x;
    u8 lvlText[12];
    u8 intText[8];

    StringCopy(lvlText, gText_Lv);
    level = GetLevelAfterDaycareSteps(&daycare->mons[daycareSlotId].mon, daycare->mons[daycareSlotId].steps);
    ConvertIntToDecimalStringN(intText, level, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringAppend(lvlText, intText);
    x = GetStringRightAlignXOffset(FONT_NORMAL, lvlText, 112);
    DaycareAddTextPrinter(windowId, lvlText, x, y);
}

static void DaycarePrintMonInfo(u8 windowId, u32 daycareSlotId, u8 y)
{
    if (daycareSlotId < (unsigned) DAYCARE_MON_COUNT)
    {
        DaycarePrintMonNickname(&gSaveBlock1Ptr->daycare, windowId, daycareSlotId, y);
        DaycarePrintMonLvl(&gSaveBlock1Ptr->daycare, windowId, daycareSlotId, y);
    }
}

#define tMenuListTaskId     data[0]
#define tWindowId           data[1]

static void Task_HandleDaycareLevelMenuInput(u8 taskId)
{
    u32 input = ListMenu_ProcessInput(gTasks[taskId].tMenuListTaskId);

    if (JOY_NEW(A_BUTTON))
    {
        switch (input)
        {
        case 0:
        case 1:
            gSpecialVar_Result = input;
            break;
        case DAYCARE_LEVEL_MENU_EXIT:
            gSpecialVar_Result = DAYCARE_EXITED_LEVEL_MENU;
            break;
        }
        DestroyListMenuTask(gTasks[taskId].tMenuListTaskId, NULL, NULL);
        ClearStdWindowAndFrame(gTasks[taskId].tWindowId, TRUE);
        RemoveWindow(gTasks[taskId].tWindowId);
        DestroyTask(taskId);
        ScriptContext_Enable();
    }
    else if (JOY_NEW(B_BUTTON))
    {
        gSpecialVar_Result = DAYCARE_EXITED_LEVEL_MENU;
        DestroyListMenuTask(gTasks[taskId].tMenuListTaskId, NULL, NULL);
        ClearStdWindowAndFrame(gTasks[taskId].tWindowId, TRUE);
        RemoveWindow(gTasks[taskId].tWindowId);
        DestroyTask(taskId);
        ScriptContext_Enable();
    }
}

void ShowDaycareLevelMenu(void)
{
    struct ListMenuTemplate menuTemplate;
    u8 windowId;
    u8 listMenuTaskId;
    u8 daycareMenuTaskId;

    windowId = AddWindow(&sDaycareLevelMenuWindowTemplate);
    DrawStdWindowFrame(windowId, FALSE);

    menuTemplate = sDaycareListMenuLevelTemplate;
    menuTemplate.windowId = windowId;
    listMenuTaskId = ListMenuInit(&menuTemplate, 0, 0);

    CopyWindowToVram(windowId, COPYWIN_FULL);

    daycareMenuTaskId = CreateTask(Task_HandleDaycareLevelMenuInput, 3);
    gTasks[daycareMenuTaskId].tMenuListTaskId = listMenuTaskId;
    gTasks[daycareMenuTaskId].tWindowId = windowId;
}

#undef tMenuListTaskId
#undef tWindowId

void ChooseSendDaycareMon(void)
{
    ChooseMonForDaycare();
    gMain.savedCallback = CB2_ReturnToField;
}

static u8 ModifyBreedingScoreForOvalCharm(u8 score)
{
    if (CheckBagHasItem(ITEM_OVAL_CHARM, 1))
    {
        switch (score)
        {
        case 20:
            return 40;
        case 50:
            return 80;
        case 70:
            return 88;
        }
    }

    return score;
}
#ifdef NATIVE
u32 NativeDaycareEggChance(u32 score) { return ModifyBreedingScoreForOvalCharm(score); }
#endif

// Route 5 Daycare

void PutMonInRoute5Daycare(void)
{
#if IS_FRLG
    u8 monIdx = GetCursorSelectionMonId();
    StorePokemonInDaycare(&gParties[B_TRAINER_PLAYER][monIdx], &gSaveBlock1Ptr->route5DayCareMon);
#endif
}

void GetCostToWithdrawRoute5DaycareMon(void)
{
#if IS_FRLG
    u16 cost = PrepareDaycareCostStringForSelectedMon(&gSaveBlock1Ptr->route5DayCareMon);
#else
    u16 cost = 100;
#endif
    gSpecialVar_0x8005 = cost;
}

bool8 IsThereMonInRoute5Daycare(void)
{
#if IS_FRLG
    if (GetBoxMonData(&gSaveBlock1Ptr->route5DayCareMon.mon, MON_DATA_SPECIES) != SPECIES_NONE)
        return TRUE;
#endif

    return FALSE;
}

u8 GetNumLevelsGainedForRoute5DaycareMon(void)
{
#if IS_FRLG
    return GetNumLevelsGainedForDaycareMon(&gSaveBlock1Ptr->route5DayCareMon);
#else
    return 0;
#endif
}

u16 TakePokemonFromRoute5Daycare(void)
{
#if IS_FRLG
    return TakeSelectedPokemonFromDaycare(&gSaveBlock1Ptr->route5DayCareMon);
#else
    return SPECIES_NONE;
#endif
}
