#include "battle_gimmick.h"
#include "battle_ai_util.h"
#include "battle_ai_switch.h"
#include "battle_terastal.h"
#include "battle_dynamax.h"
#include "battle_z_move.h"
#include "battle_script_commands.h"
#include "battle_factory.h"
#include "battle_dome.h"
#include "battle_pike.h"
#include "battle_frontier.h"
#include "constants/battle_factory.h"
#include "constants/battle_frontier_mons.h"
#include "constants/battle_pike.h"
#include "util.h"
#include "pokemon_summary_screen.h"
#include "pokedex.h"
#include "constants/rgb.h"
#include "type_icons.h"
extern const u32 *GetIndicatorSpriteSrc(enum BattlerId battler);
extern void BS_TryThirdType(void);
extern void BS_TryReflectType(void);
extern void BS_TrySoak(void);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
extern void RhhTestSummaryTypeIcon(u8 spriteId, enum Type type);
extern void RhhTestSummarySpeciesTypes(const u8 spriteIds[4], enum Species species);
extern void RhhTestSummarySpeciesTypeFrames(const u8 spriteIds[4], enum Species species, u32 frames);
extern void RhhTestAdvanceBattleTypeIcon(struct Sprite *sprite);
extern bool32 RhhTestSummaryTypeCleanup(void);
extern enum Type RhhTestSummaryDynamicType(struct Pokemon *mon, enum Move move);
extern enum Type RhhTestMoveSelectionType(enum BattlerId battler, enum Move move);
#endif

static void CheckCustomTypeBadge(u32 crystalSlot)
{
    ResetSpriteData(); FreeAllSpritePalettes();
    LoadCompressedSpriteSheet(&gSpriteSheet_MoveTypes);
    u8 a = CreateSprite(&gSpriteTemplate_MoveTypes, 10, 10, 2);
    u8 b = CreateSprite(&gSpriteTemplate_MoveTypes, 50, 10, 2);
    CHECK(a < MAX_SPRITES && b < MAX_SPRITES);
    struct Sprite *first = &gSprites[a], *second = &gSprites[b];
    u16 originalTile = first->sheetTileStart;
    CHECK(ModsSetCustomTypeIcon(first, crystalSlot, FALSE));
    u16 badgeTile = first->sheetTileStart, tag = GetSpriteTileTagByTileStart(badgeTile);
    u8 badgePalette = first->oam.paletteNum;
    CHECK(tag != TAG_NONE && badgeTile != originalTile && badgePalette < 12
        && gPlttBufferUnfaded[OBJ_PLTT_ID(badgePalette) + 2] == RGB(16, 21, 28));
    const u8 *pixels = (const u8 *)OBJ_VRAM0 + badgeTile * TILE_SIZE_4BPP;
    u32 ink = 0;
    for (u32 i = 0; i < 256; ++i)
        if ((pixels[i] & 15) >= 4 || (pixels[i] >> 4) >= 4) ++ink;
    CHECK(ink > 0); /* The badge contains rendered text, not only a colored rectangle. */
    const char *preview = getenv("RHH_BADGE_PREVIEW");
    if (preview)
    {
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 320, 160, 32, SDL_PIXELFORMAT_ARGB8888);
        CHECK(surface);
        for (u32 y = 0; y < 16; ++y) for (u32 x = 0; x < 32; ++x)
        {
            u32 tile = (y / 8) * 4 + x / 8;
            u8 packed = pixels[tile * 32 + (y % 8) * 4 + (x % 8) / 2];
            u8 index = x & 1 ? packed >> 4 : packed & 15;
            u16 color = gPlttBufferUnfaded[OBJ_PLTT_ID(badgePalette) + index];
            Uint32 mapped = SDL_MapRGB(surface->format, (color & 31) * 255 / 31,
                ((color >> 5) & 31) * 255 / 31, ((color >> 10) & 31) * 255 / 31);
            SDL_Rect rect = {x * 10, y * 10, 10, 10}; SDL_FillRect(surface, &rect, mapped);
        }
        CHECK(!SDL_SaveBMP(surface, preview)); SDL_FreeSurface(surface);
    }
    CHECK(ModsSetCustomTypeIcon(second, crystalSlot, FALSE) && second->sheetTileStart == badgeTile
        && second->oam.paletteNum == badgePalette);
    ModsClearCustomTypeIcon(first);
    CHECK(first->anims == gSpriteTemplate_MoveTypes.anims && first->sheetTileStart == originalTile
        && GetSpriteTileStartByTag(tag) == badgeTile);
    ModsClearCustomTypeIcon(second);
    CHECK(GetSpriteTileStartByTag(tag) == TAG_NONE && IndexOfSpritePaletteTag(tag) == 0xff);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    RhhTestSummaryTypeIcon(a, crystalSlot);
    CHECK(first->sheetTileStart != originalTile && first->oam.paletteNum < 12
        && gPlttBufferUnfaded[OBJ_PLTT_ID(first->oam.paletteNum) + 2] == RGB(16, 21, 28));
    RhhTestSummaryTypeIcon(a, TYPE_FIRE);
    CHECK(first->sheetTileStart == originalTile && first->animNum == TYPE_FIRE);
#endif
    for (u32 i = 0; i < 40; ++i)
        CHECK(ModsSetCustomTypeIcon(first, RHH_RUNTIME_TYPE_FIRST + 1 + i, FALSE));
    tag = GetSpriteTileTagByTileStart(first->sheetTileStart);
    badgePalette = first->oam.paletteNum;
    CHECK(tag != TAG_NONE);
    /* A screen may reserve this palette after the badge was created. */
    gReservedSpritePaletteCount = badgePalette + 1;
    ResetSpriteData();
    CHECK(GetSpriteTileStartByTag(tag) == TAG_NONE && GetSpritePaletteTagByPaletteNum(badgePalette) == TAG_NONE);
    gReservedSpritePaletteCount = 0;
    LoadCompressedSpriteSheet(&gSpriteSheet_MoveTypes);
    a = CreateSprite(&gSpriteTemplate_MoveTypes, 10, 10, 2);
    CHECK(a < MAX_SPRITES); first = &gSprites[a]; originalTile = first->sheetTileStart;
    u16 colors[16] = {0};
    for (u32 i = 0; i < 12; ++i)
    {
        struct SpritePalette occupied = {.data = colors, .tag = 0xe000 + i};
        LoadSpritePaletteInSlot(&occupied, i);
    }
    CHECK(!ModsSetCustomTypeIcon(first, crystalSlot, FALSE) && first->sheetTileStart == originalTile);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    RhhTestSummaryTypeIcon(a, crystalSlot);
    CHECK(first->sheetTileStart == originalTile && first->animNum == TYPE_MYSTERY);
#endif
    ResetSpriteData(); FreeAllSpritePalettes(); ModsReclaimCustomTypeIcons();
}
struct LegacyTypeService {
    struct RhhServiceHeader header;
    int (*chart)(RhhContentKey, RhhContentKey, uint32_t, struct RhhTypeMatchup *);
    int (*context)(struct RhhTypeMatchup *);
    int (*species)(RhhContentKey, struct RhhTypeList *);
    int (*battler)(uint32_t, struct RhhTypeList *);
};
static RhhContentKey sRuntimeAttackKey, sRuntimeDefenseKey;
static u32 sRuntimeNestedSlot, sRuntimeNestedChecks;
static void CheckRuntimeNestedChart(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->event_key == sRuntimeAttackKey && event->subject == sRuntimeDefenseKey)
    {
        /* Recursive engine queries use the unmodified entry, as for built-ins. */
        CHECK(GetTypeModifier(sRuntimeNestedSlot, TYPE_FIRE) == UQ_4_12(2));
        ++sRuntimeNestedChecks;
    }
}
static unsigned sTeraTestMode, sTeraTestCalls;
static bool32 sLegacyImpure;
static void CheckLegacyCategoryPurity(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (sLegacyImpure) { (void)Random(); (void)Random2(); }
}
static RhhContentKey sTeraTestCrystal;
static void CheckTeraTypeRule(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    const struct RhhTypeService *types = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/types", RHH_SERVICE_VERSION(1, 10));
    struct RhhOwnedMonSnapshot smallOwned = {.struct_size = sizeof(smallOwned) - 1, .slot = 97};
    struct RhhOwnedMonSnapshot savedOwned = smallOwned;
    CHECK(types && types->tera_owned_context(&smallOwned) && !memcmp(&smallOwned, &savedOwned, sizeof(smallOwned)));
    struct RhhTeraTypeContext context = {.struct_size = sizeof(context)};
    struct RhhTeraTypeContext shortContext = {.struct_size = sizeof(context) - 1, .battler = 98};
    struct RhhExtendedTypeList nested = {.struct_size = sizeof(nested), .count = 99};
    CHECK(types && types->tera_type_context(&shortContext) && shortContext.battler == 98
        && !types->tera_type_context(&context) && context.battler == event->args[0]
        && context.species == event->event_key && context.base_type == event->subject && context.battle_id
        && types->battler_list(context.battler, &nested) && nested.count == 99);
    ++sTeraTestCalls;
    if (sTeraTestMode == 1 || sTeraTestMode == 3 || sTeraTestMode == 4) decision->value = sTeraTestCrystal;
    if (sTeraTestMode == 2) decision->value = 42;
}
static void CheckTeraTypeConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sTeraTestMode == 3) decision->allowed = FALSE;
    decision->value = 42; /* constraints cannot rewrite the proposal */
}
static const struct RhhModHook sRuntimeTestHooks[] = {
    {sizeof(struct RhhModHook), RHH_TYPE_MATCHUP, RHH_TRANSFORM, 0, CheckRuntimeNestedChart, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_TERA_TYPE, RHH_TRANSFORM, 0, CheckTeraTypeRule, NULL},
    {sizeof(struct RhhModHook), RHH_BATTLE_TERA_TYPE, RHH_CONSTRAINT, 0, CheckTeraTypeConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_LEGACY_CATEGORY, RHH_TRANSFORM, 0, CheckLegacyCategoryPurity, NULL},
};
static const struct RhhModDefinition sRuntimeTestMod = {
    .struct_size = sizeof(sRuntimeTestMod), .abi_version = RHH_MOD_ABI_V1, .id = "test:runtime_type_check",
    .hook_count = ARRAY_COUNT(sRuntimeTestHooks), .hooks = sRuntimeTestHooks
};
static void CheckConversion2CustomType(u32 crystalSlot)
{
    struct BattlePokemon savedBattler = gBattleMons[0];
    enum BattlerId savedAttacker = gBattlerAttacker, savedTarget = gBattlerTarget;
    u16 landed = gLastLandedMoves[0], hitType = gLastHitByType[0];
    u16 resulting = gLastResultingMoves[1], usedType = gLastUsedMoveType[1];
    u32 flags = gBattleTypeFlags; const u8 *savedScript = gBattlescriptCurrInstr;
    rng_value_t savedRng = gRngValue;
    u8 savedText[TEXT_BUFF_ARRAY_COUNT]; memcpy(savedText, gBattleTextBuff1, sizeof(savedText));
    u8 failed = 0;
    struct __attribute__((packed)) { u8 opcode; const u8 *fail; } cmd = {
        B_SCR_OP_SETTYPETORANDOMRESISTANCE, &failed};
    gBattlerAttacker = 0; gBattlerTarget = 1;
    gLastLandedMoves[0] = gLastResultingMoves[1] = MOVE_FLAMETHROWER;
    gLastHitByType[0] = gLastUsedMoveType[1] = TYPE_FIRE;
    SET_BATTLER_TYPE(0, TYPE_NORMAL);
    for (u32 type = 0; type < NUMBER_OF_MON_TYPES; ++type)
    {
        u32 matchup = GetTypeModifier(TYPE_FIRE, type);
        if ((matchup == UQ_4_12(0) || matchup == UQ_4_12(0.5)) && !IS_BATTLER_OF_TYPE(0, type))
            gBattleMons[0].extraTypes[gBattleMons[0].extraTypeCount++] = type;
    }
    struct BattlePokemon allBuiltins = gBattleMons[0];
    u8 candidates[BATTLE_TYPE_SET_CAPACITY];
    CHECK(ModsConversion2Types(TYPE_FIRE, candidates, ARRAY_COUNT(candidates)) == 1 && candidates[0] == crystalSlot);
    gBattlescriptCurrInstr = (const u8 *)&cmd;
    gBattleScriptingCommandsTable[B_SCR_OP_SETTYPETORANDOMRESISTANCE]();
    CHECK(gBattlescriptCurrInstr == (const u8 *)&cmd + sizeof(cmd)
        && gBattleMons[0].types[0] == crystalSlot && !gBattleMons[0].extraTypeCount
        && gBattleTextBuff1[0] == B_BUFF_PLACEHOLDER_BEGIN && gBattleTextBuff1[1] == B_BUFF_TYPE
        && gBattleTextBuff1[2] == crystalSlot);
    gBattleMons[0] = allBuiltins; gBattleMons[0].extraTypes[gBattleMons[0].extraTypeCount++] = crystalSlot;
    gBattlescriptCurrInstr = (const u8 *)&cmd;
    gBattleScriptingCommandsTable[B_SCR_OP_SETTYPETORANDOMRESISTANCE]();
    CHECK(gBattlescriptCurrInstr == &failed && IS_BATTLER_OF_TYPE(0, crystalSlot));
    gBattleMons[0] = allBuiltins; gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(!ModsConversion2Types(TYPE_FIRE, candidates, ARRAY_COUNT(candidates)));
    gBattlescriptCurrInstr = (const u8 *)&cmd;
    gBattleScriptingCommandsTable[B_SCR_OP_SETTYPETORANDOMRESISTANCE]();
    CHECK(gBattlescriptCurrInstr == &failed);
    gBattleTypeFlags = flags;
    /* With no custom candidate, the original random draws and result remain
     * identical, even though the type catalog is populated. */
    SET_BATTLER_TYPE(0, TYPE_NORMAL);
    gLastHitByType[0] = gLastUsedMoveType[1] = TYPE_GRASS;
    CHECK(!ModsConversion2Types(TYPE_GRASS, candidates, ARRAY_COUNT(candidates)));
    u32 resist = 0, expected = TYPE_NONE;
    for (u32 type = 0; type < NUMBER_OF_MON_TYPES; ++type)
    {
        u32 matchup = GetTypeModifier(TYPE_GRASS, type);
        if (matchup == UQ_4_12(0) || matchup == UQ_4_12(0.5)) resist |= 1u << type;
    }
    rng_value_t beforeDraws = gRngValue;
    while (resist)
    {
        u32 type = Random() % NUMBER_OF_MON_TYPES;
        if (resist & (1u << type))
        {
            if (IS_BATTLER_OF_TYPE(0, type)) resist &= ~(1u << type);
            else { expected = type; break; }
        }
    }
    CHECK(expected != TYPE_NONE); rng_value_t afterDraws = gRngValue; gRngValue = beforeDraws;
    gBattlescriptCurrInstr = (const u8 *)&cmd;
    gBattleScriptingCommandsTable[B_SCR_OP_SETTYPETORANDOMRESISTANCE]();
    CHECK(gBattlescriptCurrInstr == (const u8 *)&cmd + sizeof(cmd) && gBattleMons[0].types[0] == expected
        && !memcmp(&gRngValue, &afterDraws, sizeof(afterDraws)));
    /* The independent chart mod makes Flying resisted by 40 custom types.
     * Fill the battler with all but one, including overflow beyond three. */
    SET_BATTLER_TYPE(0, TYPE_NORMAL);
    gLastHitByType[0] = gLastUsedMoveType[1] = TYPE_FLYING;
    u32 customCount = ModsConversion2Types(TYPE_FLYING, candidates, ARRAY_COUNT(candidates));
    CHECK(customCount == 40);
    u32 remaining = candidates[customCount - 1];
    for (u32 type = 0; type < NUMBER_OF_MON_TYPES; ++type)
    {
        u32 matchup = GetTypeModifier(TYPE_FLYING, type);
        if ((matchup == UQ_4_12(0) || matchup == UQ_4_12(0.5)) && !IS_BATTLER_OF_TYPE(0, type))
            gBattleMons[0].extraTypes[gBattleMons[0].extraTypeCount++] = type;
    }
    for (u32 i = 0; i + 1 < customCount; ++i)
        gBattleMons[0].extraTypes[gBattleMons[0].extraTypeCount++] = candidates[i];
    CHECK(gBattleMons[0].extraTypeCount > 3 && !IS_BATTLER_OF_TYPE(0, remaining));
    gBattlescriptCurrInstr = (const u8 *)&cmd;
    gBattleScriptingCommandsTable[B_SCR_OP_SETTYPETORANDOMRESISTANCE]();
    CHECK(gBattlescriptCurrInstr == (const u8 *)&cmd + sizeof(cmd)
        && gBattleMons[0].types[0] == remaining && !gBattleMons[0].extraTypeCount
        && gBattleTextBuff1[2] == remaining);
    gBattleMons[0] = savedBattler; gBattlerAttacker = savedAttacker; gBattlerTarget = savedTarget;
    gLastLandedMoves[0] = landed; gLastHitByType[0] = hitType;
    gLastResultingMoves[1] = resulting; gLastUsedMoveType[1] = usedType;
    gBattleTypeFlags = flags; gBattlescriptCurrInstr = savedScript; gRngValue = savedRng;
    memcpy(gBattleTextBuff1, savedText, sizeof(savedText));
}

static void CheckExtendedPikeStatus(void)
{
    struct Pokemon savedParty[PARTY_SIZE]; memcpy(savedParty, gParties[B_TRAINER_PLAYER], sizeof(savedParty));
    u8 savedCount = gPartiesCount[B_TRAINER_PLAYER];
    u8 savedHint = gSaveBlock2Ptr->frontier.pikeHintedRoomType;
    u8 savedRoom = gSaveBlock2Ptr->frontier.pikeHintedRoomIndex;
    u8 savedBattle = gSaveBlock2Ptr->frontier.curChallengeBattleNum;
    u16 savedFunction = gSpecialVar_0x8004, savedRoomVar = gSpecialVar_0x8007;
    rng_value_t savedRng = gRngValue, candidate = savedRng, selected = savedRng;
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(savedParty));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_RATTATA, 20, 5678, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 1;
    gSaveBlock2Ptr->frontier.pikeHintedRoomType = PIKE_ROOM_STATUS;
    gSaveBlock2Ptr->frontier.pikeHintedRoomIndex = 0;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 0;
    gSpecialVar_0x8004 = BATTLE_PIKE_FUNC_SET_ROOM_TYPE; gSpecialVar_0x8007 = 0;
    bool32 found = FALSE;
    for (u32 i = 0; i < 256 && !found; ++i)
    {
        gRngValue = candidate; Random(); candidate = gRngValue;
        gRngValue = candidate; u8 slots[FRONTIER_PARTY_SIZE] = {0, 1, 2};
        Shuffle(slots, ARRAY_COUNT(slots), sizeof(slots[0]));
        u32 first = Random() % 100, second = Random() % 100;
        if (first < 35 && second >= 35 && second < 60) { selected = candidate; found = TRUE; }
    }
    CHECK(found);
    RhhModsSetRunContext(0, 0, 0); gRngValue = selected; CallBattlePikeFunction();
    CHECK(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_STATUS) == STATUS1_TOXIC_POISON);
    u32 clear = 0; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_STATUS, &clear);
    RhhModsSetRunContext(123, 456, 0); gRngValue = selected; CallBattlePikeFunction();
    u32 status = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_STATUS);
    CHECK(status == STATUS1_FREEZE || status == STATUS1_FROSTBITE);
    memcpy(gParties[B_TRAINER_PLAYER], savedParty, sizeof(savedParty)); gPartiesCount[B_TRAINER_PLAYER] = savedCount;
    gSaveBlock2Ptr->frontier.pikeHintedRoomType = savedHint;
    gSaveBlock2Ptr->frontier.pikeHintedRoomIndex = savedRoom;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = savedBattle;
    gSpecialVar_0x8004 = savedFunction; gSpecialVar_0x8007 = savedRoomVar; gRngValue = savedRng;
}
static void CheckTypeChart(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsLoadLibrary(module));
    CHECK(!RhhModsRegister(&sRuntimeTestMod) && !RhhModsFinalize());
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhTypeService *types = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 0));
    CHECK(types && types->header.struct_size >= sizeof(*types)
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 1)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 2)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 3)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 4)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 5)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 6)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 7)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 8)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 9)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 10)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 11)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 12)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 13)) == &types->header
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 14)) == &types->header
        && RHH_TYPE_SERVICE_V1_13_SIZE < RHH_TYPE_SERVICE_V1_14_SIZE
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 15)) == &types->header
        && RHH_TYPE_SERVICE_V1_14_SIZE < RHH_TYPE_SERVICE_V1_15_SIZE
        && host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 16)) == &types->header
        && RHH_TYPE_SERVICE_V1_15_SIZE < RHH_TYPE_SERVICE_V1_16_SIZE
        && !host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 17)));
    const struct LegacyTypeService *legacy = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 0));
    CHECK(legacy && sizeof(*legacy) == RHH_TYPE_SERVICE_V1_0_SIZE);
    struct RhhTypeStabContext unavailable = {.struct_size = sizeof(unavailable), .base = 99};
    CHECK(types->stab_context(&unavailable) && unavailable.base == 99);
    struct RhhExtendedTypeStabContext unavailableList = {.struct_size = sizeof(unavailableList), .base = 99};
    CHECK(types->stab_context_list(&unavailableList) && unavailableList.base == 99);
    struct RhhPublicBattlerTypes unavailablePublic = {.struct_size = sizeof(unavailablePublic), .battler = 99};
    struct RhhPublicBattlerTypes savedUnavailablePublic = unavailablePublic;
    CHECK(types->public_battler_types(0, &unavailablePublic)
        && !memcmp(&unavailablePublic, &savedUnavailablePublic, sizeof(unavailablePublic)));
    CheckBattles(); /* Existing battle behavior stays unchanged without an active run. */
    CHECK(Effectiveness(MOVE_EARTHQUAKE, SPECIES_PIDGEOT, ABILITY_NONE, ABILITY_NONE) == UQ_4_12(0));
    RhhContentKey fire = host->content_key("rhh:type/fire"), grass = host->content_key("rhh:type/grass");
    RhhContentKey water = host->content_key("rhh:type/water"), rock = host->content_key("rhh:type/rock");
    RhhContentKey ghost = host->content_key("rhh:type/ghost"), normal = host->content_key("rhh:type/normal");
    RhhContentKey flying = host->content_key("rhh:type/flying"), ground = host->content_key("rhh:type/ground");
    RhhContentKey crystal = host->content_key("test:type/crystal");
    u32 crystalSlot = 999, invalidSlot = 999;
    CHECK(!RhhModsCustomTypeSlot(crystal, &crystalSlot) && crystalSlot == RHH_RUNTIME_TYPE_FIRST
        && RhhModsCustomTypeKey(crystalSlot) == crystal);
    CHECK(GetMoveType(MOVE_POUND) == TYPE_NORMAL);
    const u8 crystalName[] = {CHAR_C, CHAR_r, CHAR_y, CHAR_s, CHAR_t, CHAR_a, CHAR_l, EOS};
    const u8 genericCrystal[] = {CHAR_a, CHAR_SPACE, CHAR_C, CHAR_r, CHAR_y, CHAR_s, CHAR_t, CHAR_a, CHAR_l,
        CHAR_SPACE, CHAR_m, CHAR_o, CHAR_v, CHAR_e, EOS};
    CHECK(!StringCompare(GetTypeName(crystalSlot), crystalName)
        && !StringCompare(GetTypeGenericName(crystalSlot), genericCrystal)
        && GetTypeName(255) == gTypesInfo[TYPE_MYSTERY].name);
    CheckCustomTypeBadge(crystalSlot);
    sRuntimeNestedSlot = crystalSlot; sRuntimeAttackKey = crystal; sRuntimeDefenseKey = fire; sRuntimeNestedChecks = 0;
    CHECK(RhhModsCustomTypeSlot(42, &invalidSlot) && invalidSlot == 999
        && !RhhModsCustomTypeKey(RHH_RUNTIME_TYPE_FIRST - 1) && !RhhModsCustomTypeKey(255));
    const struct RhhTypeCatalogService *catalog = (const void *)host->get_service("rhh:service/type_catalog", RHH_SERVICE_VERSION(1, 0));
    CHECK(catalog && catalog->count() == 41);
    struct RhhTypeDefinition typeDefinition = {.struct_size = sizeof(typeDefinition)};
    CHECK(!catalog->at(0, &typeDefinition) && typeDefinition.key == crystal
        && !strcmp(typeDefinition.id, "test:type/crystal") && !strcmp(typeDefinition.label, "Crystal")
        && typeDefinition.rgb == 0x86aee0);
    u32 engineType = 999; CHECK(host->resolve(RHH_TYPE, crystal, &engineType) && engineType == 999);
    struct RhhTypeMatchup chart = {.struct_size = sizeof(chart)}, unchanged = chart;
    CHECK(!legacy->chart(fire, grass, 0, &chart) && chart.base == RHH_TYPE_ONE * 2);
    CHECK(!types->chart(fire, grass, 0, &chart) && chart.base == RHH_TYPE_ONE * 2
        && chart.adjusted == chart.base && (chart.flags & RHH_TYPE_MATCHUP_EXCLUDED));
    CHECK(!types->chart(crystal, fire, 0, &chart) && chart.base == RHH_TYPE_ONE * 2
        && chart.adjusted == chart.base && (chart.flags & RHH_TYPE_MATCHUP_EXCLUDED));
    CHECK(GetTypeModifier(crystalSlot, TYPE_FIRE) == UQ_4_12(2)
        && GetTypeModifier(TYPE_FIRE, crystalSlot) == UQ_4_12(0.5));
    struct RhhTypeComposition inactive = {.struct_size = sizeof(inactive)};
    CHECK(!types->compose(crystal, &fire, 1, 0, &inactive)
        && inactive.effective == RHH_TYPE_ONE * 2 && (inactive.flags & RHH_TYPE_MATCHUP_EXCLUDED));
    CHECK(!types->species(host->content_key("rhh:species/charizard"), &(struct RhhTypeList){.struct_size = sizeof(struct RhhTypeList)}));
    chart = unchanged; CHECK(types->chart(1, grass, 0, &chart) && !memcmp(&chart, &unchanged, sizeof(chart)));
    CHECK(types->chart(fire, grass, 2, &chart) && !memcmp(&chart, &unchanged, sizeof(chart)));
    CHECK(types->context(&chart) && !memcmp(&chart, &unchanged, sizeof(chart)));
    RhhModsSetRunContext(123, 456, 0); rng_value_t rng = gRngValue;
    u8 *legacyMode = RhhModsState("test:type_chart"); CHECK(legacyMode);
    struct RhhLegacyTypeCategoryResult legacyCategory = {.struct_size = sizeof(legacyCategory)};
    struct RhhLegacyTypeCategoryResult shortLegacy = {.struct_size = 1, .category = 77}, oldShortLegacy = shortLegacy;
    CHECK(types->legacy_category(crystal, &shortLegacy) && !memcmp(&shortLegacy, &oldShortLegacy, sizeof(shortLegacy))
        && types->legacy_category(fire, &legacyCategory) && types->legacy_category_context(&legacyCategory));
    *legacyMode = 1;
    CHECK(!types->legacy_category(crystal, &legacyCategory) && legacyCategory.type == crystal
        && legacyCategory.category == RHH_TYPE_CATEGORY_SPECIAL && !legacyCategory.flags);
    rng_value_t savedLegacyRng = gRngValue, savedLegacyRng2 = gRng2Value;
    sLegacyImpure = TRUE; CHECK(!types->legacy_category(crystal, &legacyCategory)
        && legacyCategory.category == RHH_TYPE_CATEGORY_MOVE_DEFAULT && (legacyCategory.flags & RHH_TYPE_CATEGORY_IMPURE)
        && !memcmp(&savedLegacyRng, &gRngValue, sizeof(savedLegacyRng)) && !memcmp(&savedLegacyRng2, &gRng2Value, sizeof(savedLegacyRng2)));
    sLegacyImpure = FALSE; *legacyMode = 3;
    CHECK(!types->legacy_category(crystal, &legacyCategory) && legacyCategory.category == RHH_TYPE_CATEGORY_MOVE_DEFAULT
        && (legacyCategory.flags & RHH_TYPE_CATEGORY_INVALID));
    *legacyMode = 4;
    CHECK(!types->legacy_category(crystal, &legacyCategory) && legacyCategory.category == RHH_TYPE_CATEGORY_MOVE_DEFAULT
        && (legacyCategory.flags & RHH_TYPE_CATEGORY_DENIED));
    *legacyMode = 1;
    struct BattleStruct *beforeCategoryBattle = gBattleStruct; bool32 beforeCategoryInBattle = gMain.inBattle;
    gBattleStruct = NULL; gMain.inBattle = FALSE;
    CHECK(GetBattleMoveCategory(MOVE_POUND) == (B_PHYSICAL_SPECIAL_SPLIT < GEN_4 ? DAMAGE_CATEGORY_SPECIAL : DAMAGE_CATEGORY_PHYSICAL)
        && GetBattleMoveCategory(MOVE_GROWL) == DAMAGE_CATEGORY_STATUS);
    gBattleStruct = beforeCategoryBattle; gMain.inBattle = beforeCategoryInBattle;
    *legacyMode = 0;
    CHECK(GetMoveType(MOVE_POUND) == crystalSlot && GetMoveType(MOVE_SCRATCH) == TYPE_NORMAL
        && GetMoveType(MOVE_GROWL) == TYPE_NORMAL && GetMoveType(MOVE_STRUGGLE) == gMovesInfo[MOVE_STRUGGLE].type);
    const struct RhhMoveService *moves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 0));
    CHECK(moves && moves->header.struct_size >= sizeof(*moves)
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 1)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 2)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 3)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 4)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 5)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 6)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 7)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 8)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 9)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 10)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 11)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 12)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 13)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 14)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 15)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 16)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 17)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 18)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 19)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 20)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 21)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 22)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 23)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 24)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 25)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 26)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 27)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 28)) == &moves->header
        && host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 29)) == &moves->header
        && !host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 55)));
    struct RhhMoveInfo moveInfo = {.struct_size = sizeof(moveInfo)};
    CHECK(!moves->info(host->content_key("rhh:move/pound"), &moveInfo)
        && moveInfo.move == host->content_key("rhh:move/pound") && moveInfo.type == crystal
        && moveInfo.category == RHH_MOVE_PHYSICAL && moveInfo.target == RHH_MOVE_TARGET_SELECTED
        && moveInfo.power == 40 && moveInfo.accuracy == 100 && moveInfo.pp == 35
        && moveInfo.priority == 0 && (moveInfo.flags & RHH_MOVE_CONTACT));
    struct RhhMoveInfo unchangedMove = moveInfo;
    CHECK(moves->info(42, &moveInfo) && !memcmp(&moveInfo, &unchangedMove, sizeof(moveInfo)));
    moveInfo.struct_size = sizeof(moveInfo) - 1; unchangedMove = moveInfo;
    CHECK(moves->info(host->content_key("rhh:move/pound"), &moveInfo)
        && !memcmp(&moveInfo, &unchangedMove, sizeof(moveInfo)));
    RhhContentKey bulbasaur = host->content_key("rhh:species/bulbasaur");
    for (u32 kind = RHH_LEARNSET_LEVEL; kind <= RHH_LEARNSET_EGG; ++kind)
    {
        u32 count = 999; CHECK(!moves->learnset_count(bulbasaur, kind, &count) && count);
        for (u32 i = 0; i < count; ++i)
        {
            struct RhhLearnsetEntry entry = {.struct_size = sizeof(entry)};
            u32 expectedMove = kind == RHH_LEARNSET_LEVEL ? GetSpeciesLevelUpLearnset(SPECIES_BULBASAUR)[i].move
                : kind == RHH_LEARNSET_TEACHABLE ? GetSpeciesTeachableLearnset(SPECIES_BULBASAUR)[i]
                : GetSpeciesEggMoves(SPECIES_BULBASAUR)[i];
            CHECK(!moves->learnset_at(bulbasaur, kind, i, &entry) && entry.species == bulbasaur
                && entry.move == RhhContentKeyForId(RHH_MOVE, expectedMove) && entry.kind == kind
                && entry.level == (kind == RHH_LEARNSET_LEVEL ? GetSpeciesLevelUpLearnset(SPECIES_BULBASAUR)[i].level : 0));
        }
        struct RhhLearnsetEntry entry = {.struct_size = sizeof(entry), .level = 123}, unchangedEntry = entry;
        CHECK(moves->learnset_at(bulbasaur, kind, count, &entry) && !memcmp(&entry, &unchangedEntry, sizeof(entry)));
    }
    u32 badCount = 123;
    CHECK(moves->learnset_count(42, RHH_LEARNSET_LEVEL, &badCount) && badCount == 123
        && moves->learnset_count(bulbasaur, 0, &badCount) && badCount == 123);
    /* Every enabled species' published entry must resolve, including forms
     * and repeated moves; otherwise a successful count would be misleading. */
    for (u32 species = 1; species < NUM_SPECIES; ++species)
    {
        RhhContentKey speciesKey = RhhContentKeyForId(RHH_SPECIES, species);
        struct RhhContentInfo metadata = {.struct_size = sizeof(metadata)};
        if (!speciesKey || species == SPECIES_EGG || RhhContentInfoFor(RHH_SPECIES, speciesKey, &metadata)
            || !(metadata.flags & RHH_CONTENT_ENABLED)) continue;
        for (u32 kind = RHH_LEARNSET_LEVEL; kind <= RHH_LEARNSET_EGG; ++kind)
        {
            u32 count; CHECK(!moves->learnset_count(speciesKey, kind, &count));
            for (u32 i = 0; i < count; ++i)
            {
                struct RhhLearnsetEntry entry = {.struct_size = sizeof(entry)};
                if (moves->learnset_at(speciesKey, kind, i, &entry))
                { fprintf(stderr, "learnset resolution failed: species %u kind %u entry %u\n", species, kind, i); exit(1); }
                CHECK(entry.species == speciesKey && entry.move);
            }
        }
    }
    struct RhhTypeList customSpecies = {.struct_size = sizeof(customSpecies)};
    CHECK(!types->species(host->content_key("rhh:species/metapod"), &customSpecies)
        && customSpecies.count == 1 && customSpecies.types[0] == crystal);
    CHECK(!types->species(host->content_key("rhh:species/kakuna"), &customSpecies)
        && customSpecies.count == 2 && customSpecies.types[0] == host->content_key("rhh:type/bug")
        && customSpecies.types[1] == host->content_key("rhh:type/poison"));
    CHECK(!types->species(host->content_key("rhh:species/caterpie"), &customSpecies)
        && customSpecies.count == 3 && customSpecies.types[0] == fire
        && customSpecies.types[1] == flying && customSpecies.types[2] == crystal);
    CHECK(CalcPartyMonTypeEffectivenessMultiplier(MOVE_FLAMETHROWER, SPECIES_METAPOD, ABILITY_NONE) == UQ_4_12(0.5));
    CHECK(IsSpeciesOfType(SPECIES_CATERPIE, crystalSlot)
        && PokedexSpeciesMatchesTypes(SPECIES_CATERPIE, crystalSlot, TYPE_FIRE));
    CHECK(PokedexSpeciesMatchesTypes(SPECIES_ABRA, crystalSlot, TYPE_FIRE)
        && PokedexSpeciesMatchesTypes(SPECIES_PIDGEY, TYPE_WATER, crystalSlot)
        && PokedexSpeciesMatchesTypes(SPECIES_METAPOD, crystalSlot, crystalSlot)
        && !PokedexSpeciesMatchesTypes(SPECIES_ABRA, crystalSlot, crystalSlot));
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_PIDGEY, EFFECTIVENESS_MODE_GOOD) == 8
        && GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_PIDGEY, EFFECTIVENESS_MODE_BAD) == -4
        && GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_PIDGEY, EFFECTIVENESS_MODE_AI_VS_AI) == 20);
    CHECK(GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_DIGLETT, EFFECTIVENESS_MODE_GOOD) == 16
        && GetDomeTypeEffectivenessPoints(MOVE_ROCK_SLIDE, SPECIES_DIGLETT, EFFECTIVENESS_MODE_BAD) == -8
        && GetDomeTypeEffectivenessPoints(MOVE_FLAMETHROWER, SPECIES_PIDGEY, EFFECTIVENESS_MODE_BAD) == 6);
    CheckExtendedPikeStatus();
    u16 savedFactory[FRONTIER_PARTY_SIZE]; memcpy(savedFactory, gFrontierTempParty, sizeof(savedFactory));
    u16 savedFactoryFunction = gSpecialVar_0x8004, savedFactoryResult = gSpecialVar_Result;
    const struct TrainerMon *savedFactoryMons = gFacilityTrainerMons;
    u8 savedString[sizeof(gStringVar1)]; memcpy(savedString, gStringVar1, sizeof(savedString));
    for (u32 i = 0; i < FRONTIER_PARTY_SIZE; ++i) gFrontierTempParty[i] = FRONTIER_MON_METAPOD;
    gSpecialVar_0x8004 = BATTLE_FACTORY_FUNC_GET_OPPONENT_MON_TYPE; CallBattleFactoryFunction();
    CHECK(gSpecialVar_Result == FACTORY_CUSTOM_TYPE_HINT && !StringCompare(gStringVar1, crystalName));
    gFrontierTempParty[0] = FRONTIER_MON_METAPOD; gFrontierTempParty[1] = FRONTIER_MON_ABRA;
    gFrontierTempParty[2] = FRONTIER_MON_PIDGEY; CallBattleFactoryFunction();
    CHECK(gSpecialVar_Result == FACTORY_CUSTOM_TYPE_HINT && !StringCompare(gStringVar1, crystalName));
    memcpy(gFrontierTempParty, savedFactory, sizeof(savedFactory)); memcpy(gStringVar1, savedString, sizeof(savedString));
    gSpecialVar_0x8004 = savedFactoryFunction; gSpecialVar_Result = savedFactoryResult; gFacilityTrainerMons = savedFactoryMons;
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    ResetSpriteData(); FreeAllSpritePalettes(); LoadCompressedSpriteSheet(&gSpriteSheet_MoveTypes);
    u8 summarySprites[4];
    for (u32 i = 0; i < 4; ++i) { summarySprites[i] = CreateSprite(&gSpriteTemplate_MoveTypes, 0, 0, 2); CHECK(summarySprites[i] < MAX_SPRITES); }
    u16 summaryBaseTile = gSprites[summarySprites[2]].sheetTileStart;
    RhhTestSummarySpeciesTypes(summarySprites, SPECIES_CATERPIE);
    CHECK(gSprites[summarySprites[0]].animNum == TYPE_FIRE && gSprites[summarySprites[1]].animNum == TYPE_FLYING
        && !gSprites[summarySprites[2]].invisible && gSprites[summarySprites[2]].sheetTileStart != summaryBaseTile
        && gPlttBufferUnfaded[OBJ_PLTT_ID(gSprites[summarySprites[2]].oam.paletteNum) + 2] == RGB(16, 21, 28));
    RhhTestSummarySpeciesTypeFrames(summarySprites, SPECIES_PIDGEY, 0);
#if P_SHOW_TERA_TYPE >= GEN_9
    CHECK(gSprites[summarySprites[0]].animNum == TYPE_FIRE && gSprites[summarySprites[1]].animNum == TYPE_FLYING
        && gSprites[summarySprites[2]].animNum == TYPE_GRASS && gSprites[summarySprites[3]].animNum == TYPE_ELECTRIC);
    RhhTestSummarySpeciesTypeFrames(summarySprites, SPECIES_PIDGEY, 90);
    CHECK(!gSprites[summarySprites[0]].invisible && gSprites[summarySprites[0]].sheetTileStart != summaryBaseTile
        && gSprites[summarySprites[1]].animNum == TYPE_WATER && gSprites[summarySprites[2]].invisible
        && gSprites[summarySprites[3]].animNum == TYPE_ELECTRIC);
    RhhTestSummarySpeciesTypeFrames(summarySprites, SPECIES_PIDGEY, 180);
    CHECK(gSprites[summarySprites[0]].animNum == TYPE_FIRE && gSprites[summarySprites[3]].animNum == TYPE_ELECTRIC);
#else
    CHECK(gSprites[summarySprites[0]].animNum == TYPE_FIRE && gSprites[summarySprites[1]].animNum == TYPE_FLYING
        && gSprites[summarySprites[2]].animNum == TYPE_GRASS && !gSprites[summarySprites[3]].invisible
        && gSprites[summarySprites[3]].sheetTileStart != summaryBaseTile);
    RhhTestSummarySpeciesTypeFrames(summarySprites, SPECIES_PIDGEY, 90);
    CHECK(!gSprites[summarySprites[0]].invisible && gSprites[summarySprites[0]].animNum == TYPE_WATER
        && gSprites[summarySprites[1]].invisible && gSprites[summarySprites[2]].invisible
        && gSprites[summarySprites[3]].invisible);
    RhhTestSummarySpeciesTypeFrames(summarySprites, SPECIES_PIDGEY, 180);
    CHECK(gSprites[summarySprites[0]].animNum == TYPE_FIRE && !gSprites[summarySprites[3]].invisible
        && gSprites[summarySprites[3]].sheetTileStart != summaryBaseTile);
#endif
    CHECK(RhhTestSummaryTypeCleanup());
    ResetSpriteData(); FreeAllSpritePalettes();
#endif
    struct Pokemon sample; struct BattlePokemon sampleBattle;
    CreateMonWithIVs(&sample, SPECIES_METAPOD, 20, 4321, OTID_STRUCT_PRESET(54321), 31);
    PokemonToBattleMon(&sample, &sampleBattle);
    CHECK(sampleBattle.types[0] == crystalSlot && sampleBattle.types[1] == crystalSlot
        && sampleBattle.types[2] == TYPE_MYSTERY);
    CHECK(GetOverworldTypeEffectiveness(&sample, TYPE_FIRE) == UQ_4_12(0.5));
    CreateMonWithIVs(&sample, SPECIES_WEEDLE, 20, 4322, OTID_STRUCT_PRESET(54321), 31);
    CHECK(GetDynamicMoveType(&sample, MOVE_REVELATION_DANCE, 0, ABILITY_NONE, HOLD_EFFECT_NONE, MON_OUTSIDE_BATTLE)
        == crystalSlot);
    struct Pokemon formMove;
    CreateMonWithIVs(&formMove, SPECIES_OGERPON_WELLSPRING, 20, 4328, OTID_STRUCT_PRESET(54321), 31);
    CHECK(GetDynamicMoveType(&formMove, MOVE_IVY_CUDGEL, 0, ABILITY_NONE, HOLD_EFFECT_NONE, MON_OUTSIDE_BATTLE) == crystalSlot);
    struct Pokemon bullMove;
    CreateMonWithIVs(&bullMove, SPECIES_TAUROS_PALDEA_BLAZE, 20, 4329, OTID_STRUCT_PRESET(54321), 31);
    CHECK(GetDynamicMoveType(&bullMove, MOVE_RAGING_BULL, 0, ABILITY_NONE, HOLD_EFFECT_NONE, MON_OUTSIDE_BATTLE) == crystalSlot);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    struct BattleStruct *savedBattleStruct = gBattleStruct; gBattleStruct = NULL;
    CHECK(RhhTestSummaryDynamicType(&formMove, MOVE_IVY_CUDGEL) == crystalSlot);
    CHECK(RhhTestSummaryDynamicType(&bullMove, MOVE_RAGING_BULL) == crystalSlot);
    CHECK(RhhTestSummaryDynamicType(&sample, MOVE_REVELATION_DANCE) == crystalSlot);
    CHECK(RhhTestSummaryDynamicType(&sample, MOVE_POUND) == crystalSlot);
    CHECK(RhhTestSummaryDynamicType(&sample, MOVE_TACKLE) == GetMoveType(MOVE_TACKLE));
    struct Pokemon ordinary; CreateMonWithIVs(&ordinary, SPECIES_CHARIZARD, 20, 4324, OTID_STRUCT_PRESET(54321), 31);
    CHECK(RhhTestSummaryDynamicType(&ordinary, MOVE_REVELATION_DANCE) == GetMoveType(MOVE_REVELATION_DANCE));
    gBattleStruct = savedBattleStruct;
#endif
    CHECK(host->content_count(RHH_TYPE) == NUMBER_OF_MON_TYPES - 1);
    for (u32 a = 1; a < NUMBER_OF_MON_TYPES; ++a)
        for (u32 d = 1; d < NUMBER_OF_MON_TYPES; ++d)
        {
            RhhContentKey ak = RhhContentKeyForId(RHH_TYPE, a), dk = RhhContentKeyForId(RHH_TYPE, d);
            CHECK(ak && dk && !types->chart(ak, dk, 0, &chart));
            CHECK(chart.base == gTypeEffectivenessTable[a][d] && chart.effective == GetTypeModifier(a, d));
            u32 expected = chart.adjusted == 0 || chart.adjusted == RHH_TYPE_ONE / 2 ? RHH_TYPE_ONE * 2
                : chart.adjusted == RHH_TYPE_ONE * 2 ? RHH_TYPE_ONE / 2 : RHH_TYPE_ONE;
            CHECK(!types->chart(ak, dk, RHH_TYPE_MATCHUP_INVERSE, &chart) && chart.effective == expected
                && ModsTypeMatchup(a, d, chart.base, TRUE) == chart.adjusted);
        }
    CHECK(!types->chart(fire, grass, 0, &chart) && chart.base == RHH_TYPE_ONE * 2 && chart.adjusted == RHH_TYPE_ONE / 2);
    CHECK(!types->chart(crystal, fire, 0, &chart) && chart.base == RHH_TYPE_ONE * 2
        && chart.adjusted == RHH_TYPE_ONE && chart.effective == RHH_TYPE_ONE
        && chart.attack == crystal && chart.defense == fire);
    u32 runtime = 999;
    CHECK(GetTypeModifier(crystalSlot, TYPE_FIRE) == UQ_4_12(1)
        && GetTypeModifier(TYPE_FIRE, crystalSlot) == UQ_4_12(0.5)
        && GetTypeModifier(255, TYPE_FIRE) == UQ_4_12(1)
        && ModsRuntimeTypeMatchup(crystalSlot, TYPE_WATER, TRUE, &runtime) && runtime == UQ_4_12(2));
    CHECK(sRuntimeNestedChecks);
    CHECK(!types->chart(fire, crystal, 0, &chart) && chart.base == RHH_TYPE_ONE / 2 && chart.effective == RHH_TYPE_ONE / 2);
    CHECK(!types->chart(water, crystal, 0, &chart) && chart.base == RHH_TYPE_ONE && chart.effective == RHH_TYPE_ONE);
    CHECK(!types->chart(crystal, water, RHH_TYPE_MATCHUP_INVERSE, &chart) && chart.base == RHH_TYPE_ONE / 2
        && chart.adjusted == RHH_TYPE_ONE / 2 && chart.effective == RHH_TYPE_ONE * 2);
    RhhContentKey defenses[40];
    for (u32 i = 0; i < 40; ++i) {
        char id[32]; snprintf(id, sizeof(id), "test:type/extra_%02u", i);
        defenses[i] = host->content_key(id);
    }
    /* Exercise the actual hazard calculation with more than three types.  The
     * first twenty are weak to Crystal and the last twenty resist it. */
    enum Type hazardTypes[42], reverseHazardTypes[40];
    for (u32 i = 0; i < 40; ++i)
    {
        u32 slot; CHECK(!RhhModsCustomTypeSlot(defenses[i], &slot));
        hazardTypes[i] = slot;
    }
    for (u32 i = 0; i < 40; ++i) reverseHazardTypes[i] = hazardTypes[39 - i];
    hazardTypes[40] = hazardTypes[0]; hazardTypes[41] = TYPE_MYSTERY;
    enum TypeSideHazard crystalHazard = (enum TypeSideHazard)crystalSlot;
    CHECK(GetStealthHazardDamageForTypes(crystalHazard, hazardTypes, 20, 1024) == 1024);
    CHECK(GetStealthHazardDamageForTypes(crystalHazard, hazardTypes + 20, 20, 1024) == 1);
    CHECK(GetStealthHazardDamageForTypes(crystalHazard, hazardTypes, 42, 1024) == 128);
    CHECK(GetStealthHazardDamageForTypes(crystalHazard, reverseHazardTypes, 40, 1024) == 128);
    CHECK(GetTypeModifierForTypes(crystalSlot, hazardTypes, 20) == 0xffffffffu);
    CHECK(GetTypeModifierForTypes(crystalSlot, hazardTypes + 20, 20) == 1);
    CHECK(GetTypeModifierForTypes(crystalSlot, hazardTypes, 42) == UQ_4_12(1));
    CHECK(GetTypeModifierForTypes(crystalSlot, reverseHazardTypes, 40) == UQ_4_12(1));
    CHECK(GetStealthHazardDamageForTypes(crystalHazard, NULL, 0, 1024) == 128);
    CHECK(GetStealthHazardDamageForTypes(crystalHazard, hazardTypes, 40, 0) == 0);
    const enum Type immuneHazardTypes[] = {TYPE_WATER, TYPE_GROUND, TYPE_FLYING, TYPE_GROUND};
    CHECK(GetStealthHazardDamageForTypes((enum TypeSideHazard)TYPE_ELECTRIC,
        immuneHazardTypes, ARRAY_COUNT(immuneHazardTypes), 1024) == 0);
    struct RhhTypeComposition composition = {.struct_size = sizeof(composition)};
    CHECK(!types->compose(crystal, NULL, 0, 0, &composition)
        && !composition.count && composition.effective == RHH_TYPE_ONE);
    CHECK(!types->compose(crystal, defenses, 20, 0, &composition)
        && composition.count == 20 && composition.effective == UINT32_MAX
        && (composition.flags & RHH_TYPE_COMPOSITION_SATURATED));
    CHECK(!types->compose(crystal, defenses + 20, 13, 0, &composition)
        && composition.effective == 1 && (composition.flags & RHH_TYPE_COMPOSITION_SATURATED));
    CHECK(!types->compose(crystal, defenses, 40, 0, &composition)
        && composition.count == 40 && composition.effective == RHH_TYPE_ONE
        && !(composition.flags & RHH_TYPE_COMPOSITION_SATURATED));
    RhhContentKey reversed[40];
    for (u32 i = 0; i < 40; ++i) reversed[i] = defenses[39 - i];
    CHECK(!types->compose(crystal, reversed, 40, 0, &composition)
        && composition.effective == RHH_TYPE_ONE && !(composition.flags & RHH_TYPE_COMPOSITION_SATURATED));
    CHECK(!types->compose(crystal, defenses, 20, RHH_TYPE_MATCHUP_INVERSE, &composition)
        && composition.effective == 1 && (composition.flags & RHH_TYPE_COMPOSITION_SATURATED));
    RhhContentKey mixed[] = {fire, water, fire, rock};
    CHECK(!types->compose(crystal, mixed, 4, 0, &composition) && composition.count == 3
        && composition.effective == RHH_TYPE_ONE / 2);
    RhhContentKey immunity[] = {normal, host->content_key("rhh:type/dark")};
    CHECK(!types->compose(ghost, immunity, 2, 0, &composition) && !composition.effective);
    CHECK(!types->compose(ghost, immunity, 2, RHH_TYPE_MATCHUP_INVERSE, &composition)
        && composition.effective == RHH_TYPE_ONE * 4);
    struct RhhTypeComposition savedComposition = composition;
    RhhContentKey invalid[] = {fire, 42};
    CHECK(types->compose(crystal, invalid, 2, 0, &composition)
        && !memcmp(&composition, &savedComposition, sizeof(composition)));
    CHECK(types->compose(crystal, NULL, 1, 0, &composition)
        && types->compose(crystal, defenses, RHH_TYPE_COMPOSE_LIMIT + 1, 0, &composition)
        && types->compose(crystal, defenses, 1, 8, &composition)
        && !memcmp(&composition, &savedComposition, sizeof(composition)));
    RhhContentKey repeated[RHH_TYPE_COMPOSE_LIMIT];
    for (u32 i = 0; i < RHH_TYPE_COMPOSE_LIMIT; ++i) repeated[i] = water;
    CHECK(!types->compose(crystal, repeated, RHH_TYPE_COMPOSE_LIMIT, 0, &composition)
        && composition.count == 1 && composition.effective == RHH_TYPE_ONE / 2);
    savedComposition = composition;
    RhhContentKey placeholder = host->content_key("rhh:type/mystery");
    CHECK(types->compose(crystal, &placeholder, 1, 0, &composition)
        && !memcmp(&composition, &savedComposition, sizeof(composition)));
    chart = unchanged; CHECK(types->chart(crystal, 42, 0, &chart) && !memcmp(&chart, &unchanged, sizeof(chart)));
    CHECK(!types->chart(ground, flying, 0, &chart) && !chart.base && chart.effective == RHH_TYPE_ONE);
    CHECK(!types->chart(water, rock, 0, &chart) && chart.effective == RHH_TYPE_ONE * 2); /* invalid transform */
    CHECK(!types->chart(ghost, normal, 0, &chart) && !chart.effective); /* denial is sticky */
    CHECK(GetTypeModifier(TYPE_NONE, TYPE_GRASS) == UQ_4_12(1));
    struct RhhTypeList list = {.struct_size = sizeof(list)};
    CHECK(!types->species(host->content_key("rhh:species/charizard"), &list) && list.count == 2
        && list.types[0] == fire && list.types[1] == flying);
    CHECK(!types->species(host->content_key("rhh:species/poochyena"), &list) && list.count == 1);
    struct RhhTypeList saved = list; CHECK(types->species(1, &list) && !memcmp(&saved, &list, sizeof(list)));
    CHECK(types->battler(0, &list) && !memcmp(&saved, &list, sizeof(list)));
    struct BattleResources resources = {0}; struct AiLogicData ai = {0}; struct AiPartyData aiParties = {0};
    struct AiThinkingStruct thinking = {0}; struct BattleHistory history = {0};
    gAiThinkingStruct = &thinking; gBattleHistory = &history;
    memset(&sBattle, 0, sizeof(sBattle)); gBattleStruct = &sBattle; gBattleResources = &resources;
    gAiLogicData = &ai; gAiPartyData = &aiParties; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER; gBattlersCount = 2; gAbsentBattlerFlags = 0;
    for (u32 i = 0; i < 2; ++i)
    {
        CreateMonWithIVs(&gParties[i][0], i ? SPECIES_CHARIZARD : SPECIES_TORCHIC, 20, 1000 + i, OTID_STRUCT_PRESET(54321 + i), 31);
        gPartiesCount[i] = 1; gBattlerPositions[i] = i; gBattlerPartyIndexes[i] = 0;
        PokemonToBattleMon(&gParties[i][0], &gBattleMons[i]);
    }
    ModsBattleBegin();
    *legacyMode = 1; gBattleStruct->dynamicMoveType = crystalSlot;
    CHECK(GetBattleMoveCategory(MOVE_POUND) == (B_PHYSICAL_SPECIAL_SPLIT < GEN_4 ? DAMAGE_CATEGORY_SPECIAL : DAMAGE_CATEGORY_PHYSICAL)
        && GetBattleMoveCategory(MOVE_GROWL) == DAMAGE_CATEGORY_STATUS);
    gBattleStruct->dynamicMoveCategory = DAMAGE_CATEGORY_PHYSICAL;
    CHECK(GetBattleMoveCategory(MOVE_POUND) == DAMAGE_CATEGORY_PHYSICAL);
    gBattleStruct->dynamicMoveCategory = DAMAGE_CATEGORY_NONE;
    *legacyMode = 2;
    CHECK(GetBattleMoveCategory(MOVE_FLAMETHROWER) == (B_PHYSICAL_SPECIAL_SPLIT < GEN_4 ? DAMAGE_CATEGORY_PHYSICAL : DAMAGE_CATEGORY_SPECIAL));
    *legacyMode = 1;
    if (B_PHYSICAL_SPECIAL_SPLIT < GEN_4)
    {
        u16 oldAttack = gBattleMons[0].attack, oldSpAttack = gBattleMons[0].spAttack;
        u16 oldDefense = gBattleMons[1].defense, oldSpDefense = gBattleMons[1].spDefense;
        gBattleMons[0].attack = 25; gBattleMons[0].spAttack = 200;
        gBattleMons[1].defense = gBattleMons[1].spDefense = 100;
        struct AiCalcValues legacyAi = {.move = MOVE_POUND}; rng_value_t beforeCategoryAi = gRngValue;
        gBattleStruct->dynamicMoveType = TYPE_NONE;
        struct SimulatedDamage specialDamage = AI_CalcDamage(&legacyAi, 0, 1);
        gRngValue = beforeCategoryAi; *legacyMode = 2;
        struct SimulatedDamage physicalDamage = AI_CalcDamage(&legacyAi, 0, 1);
        CHECK(specialDamage.maximum > physicalDamage.maximum && physicalDamage.maximum);
        gRngValue = beforeCategoryAi; gBattleMons[0].attack = oldAttack; gBattleMons[0].spAttack = oldSpAttack;
        gBattleMons[1].defense = oldDefense; gBattleMons[1].spDefense = oldSpDefense;
        *legacyMode = 1; gBattleStruct->dynamicMoveType = crystalSlot;
    }
    gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(!types->legacy_category(crystal, &legacyCategory) && (legacyCategory.flags & RHH_TYPE_CATEGORY_EXCLUDED)
        && legacyCategory.category == RHH_TYPE_CATEGORY_MOVE_DEFAULT
        && GetBattleMoveCategory(MOVE_POUND) == DAMAGE_CATEGORY_PHYSICAL);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; gBattleStruct->dynamicMoveType = TYPE_NONE; *legacyMode = 0;
    CheckConversion2CustomType(crystalSlot);
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][0];
        u8 old = GetMonData(mon, MON_DATA_TERA_TYPE), fireTera = TYPE_FIRE;
        SetMonData(mon, MON_DATA_TERA_TYPE, &fireTera);
        sTeraTestCrystal = crystal; sTeraTestCalls = 0; sTeraTestMode = 1;
        rng_value_t beforeTeraRng = gRngValue;
        CHECK(GetBattlerTeraType(0) == crystalSlot && sTeraTestCalls);
        CHECK(GetMonData(mon, MON_DATA_TERA_TYPE) == TYPE_FIRE);
        CHECK(GetTeraTypeRGB(crystalSlot) == RGB(16, 21, 28));
        SetActiveGimmick(0, GIMMICK_TERA);
        struct BattleTypeList teraTypes; GetBattlerTypeList(0, FALSE, &teraTypes);
        CHECK(teraTypes.count == 1 && teraTypes.types[0] == crystalSlot);
        CHECK(GetDynamicMoveType(mon, MOVE_TERA_BLAST, 0, ABILITY_NONE, HOLD_EFFECT_NONE, MON_IN_BATTLE) == crystalSlot);
        CHECK(GetIndicatorSpriteSrc(0));
        sTeraTestMode = 2; CHECK(GetBattlerTeraType(0) == TYPE_FIRE);
        sTeraTestMode = 3; CHECK(GetBattlerTeraType(0) == TYPE_FIRE);
        sTeraTestMode = 4; CHECK(GetBattlerTeraType(0) == crystalSlot);
        unsigned calls = sTeraTestCalls; gBattleTypeFlags |= BATTLE_TYPE_LINK;
        CHECK(GetBattlerTeraType(0) == TYPE_FIRE && sTeraTestCalls == calls);
        gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
        struct RhhTeraTypeContext unavailableTera = {.struct_size = sizeof(unavailableTera), .battler = 99};
        CHECK(types->tera_type_context(&unavailableTera) && unavailableTera.battler == 99);
        CHECK(!memcmp(&gRngValue, &beforeTeraRng, sizeof(beforeTeraRng)));
        sTeraTestMode = 0;
        struct Pokemon other = *mon; u8 waterTera = TYPE_WATER;
        SetMonData(&other, MON_DATA_TERA_TYPE, &waterTera);
        CHECK(GetDynamicMoveType(&other, MOVE_TERA_BLAST, 0, ABILITY_NONE, HOLD_EFFECT_NONE, MON_IN_BATTLE) == TYPE_WATER);
        SetActiveGimmick(0, GIMMICK_NONE); SetMonData(mon, MON_DATA_TERA_TYPE, &old);
    }
    CHECK(GetMoveType(MOVE_POUND) == crystalSlot);
    SetTypeBeforeUsingMove(MOVE_POUND, 0, ABILITY_NONE, HOLD_EFFECT_NONE);
    CHECK(GetBattleMoveType(MOVE_POUND) == crystalSlot);
    struct BattlePokemon moveTypeDefender = gBattleMons[1];
    SET_BATTLER_TYPE(1, TYPE_WATER);
    struct DamageContext assignedMove = {.battlerAtk = 0, .battlerDef = 1, .move = MOVE_POUND,
        .baseMove = MOVE_POUND, .chosenMove = MOVE_POUND, .moveType = GetBattleMoveType(MOVE_POUND)};
    CHECK(CalcTypeEffectivenessMultiplier(&assignedMove) == UQ_4_12(0.5));
    gBattleMons[1] = moveTypeDefender;
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    CHECK(RhhTestMoveSelectionType(0, MOVE_POUND) == crystalSlot);
    enum Ability savedPreviewAbility = gBattleMons[0].ability;
    enum Type savedPreviewType = gBattleStruct->dynamicMoveType;
    SetActiveGimmick(0, GIMMICK_DYNAMAX); gBattleStruct->dynamicMoveType = TYPE_FIRE;
    gBattleMons[0].ability = ABILITY_NORMALIZE;
    CHECK(RhhTestMoveSelectionType(0, MOVE_POUND) == TYPE_NORMAL);
    gBattleMons[0].ability = ABILITY_NONE;
    CHECK(RhhTestMoveSelectionType(0, MOVE_POUND) == crystalSlot);
    gBattleMons[0].volatiles.electrified = TRUE;
    CHECK(RhhTestMoveSelectionType(0, MOVE_POUND) == TYPE_ELECTRIC);
    CHECK(gBattleStruct->dynamicMoveType == TYPE_FIRE);
    gBattleMons[0].volatiles.electrified = FALSE; gBattleMons[0].ability = savedPreviewAbility;
    SetActiveGimmick(0, GIMMICK_NONE); gBattleStruct->dynamicMoveType = savedPreviewType;
#endif
    gBattleStruct->dynamicMoveType = TYPE_NONE;
    SetTypeBeforeUsingMove(MOVE_POUND, 0, ABILITY_NORMALIZE, HOLD_EFFECT_NONE);
    CHECK(GetBattleMoveType(MOVE_POUND) == TYPE_NORMAL);
    gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(GetMoveType(MOVE_POUND) == TYPE_NORMAL);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    gBattleTypeFlags |= BATTLE_TYPE_RECORDED;
    CHECK(GetMoveType(MOVE_POUND) == TYPE_NORMAL);
    gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED;
    gBattleStruct->dynamicMoveType = TYPE_NONE;
    gBattleStruct->battlerState[0].ateBoost = FALSE;
    /* Form-dependent moves use the mod-resolved species list, not the
     * battler's temporary type or the unmodified species table. */
    struct Pokemon savedFormParty = gParties[B_TRAINER_PLAYER][0];
    struct BattlePokemon savedFormBattler = gBattleMons[0];
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_OGERPON_WELLSPRING, 20, 8111, OTID_STRUCT_PRESET(54321), 31);
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
    SET_BATTLER_TYPE(0, TYPE_FIRE);
    CHECK(GetDynamicMoveType(&gParties[B_TRAINER_PLAYER][0], MOVE_IVY_CUDGEL, 0,
        ABILITY_NONE, HOLD_EFFECT_NONE, MON_IN_BATTLE) == crystalSlot);
    SetTypeBeforeUsingMove(MOVE_IVY_CUDGEL, 0, ABILITY_NONE, HOLD_EFFECT_NONE);
    CHECK(GetBattleMoveType(MOVE_IVY_CUDGEL) == crystalSlot);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    CHECK(RhhTestMoveSelectionType(0, MOVE_IVY_CUDGEL) == crystalSlot);
#endif
    gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(GetDynamicMoveType(&gParties[B_TRAINER_PLAYER][0], MOVE_IVY_CUDGEL, 0,
        ABILITY_NONE, HOLD_EFFECT_NONE, MON_IN_BATTLE) == TYPE_WATER);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    CHECK(RhhTestMoveSelectionType(0, MOVE_IVY_CUDGEL) == TYPE_WATER);
#endif
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_TAUROS_PALDEA_BLAZE, 20, 8112, OTID_STRUCT_PRESET(54321), 31);
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
    CHECK(GetDynamicMoveType(&gParties[B_TRAINER_PLAYER][0], MOVE_RAGING_BULL, 0,
        ABILITY_NONE, HOLD_EFFECT_NONE, MON_IN_BATTLE) == crystalSlot);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    CHECK(RhhTestMoveSelectionType(0, MOVE_RAGING_BULL) == crystalSlot);
#endif
    gBattleTypeFlags |= BATTLE_TYPE_RECORDED;
    CHECK(GetDynamicMoveType(&gParties[B_TRAINER_PLAYER][0], MOVE_RAGING_BULL, 0,
        ABILITY_NONE, HOLD_EFFECT_NONE, MON_IN_BATTLE) == TYPE_FIRE);
    gBattleTypeFlags &= ~BATTLE_TYPE_RECORDED;
    gParties[B_TRAINER_PLAYER][0] = savedFormParty; gBattleMons[0] = savedFormBattler;
    gBattleStruct->dynamicMoveType = TYPE_NONE;
    /* Forty weaknesses to Fighting and forty resistances to Flying cancel.
     * Flying Press must not clamp the first half before the second half. */
    struct Pokemon beforeWideOpponent = gParties[B_TRAINER_OPPONENT_A][0];
    struct BattlePokemon beforeWideBattler = gBattleMons[1];
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_EKANS, 20, 7777, OTID_STRUCT_PRESET(54321), 31);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    CHECK(gBattleMons[1].extraTypeCount == 37);
    struct DamageContext wideMove = {.battlerAtk = 0, .battlerDef = 1, .move = MOVE_FLYING_PRESS,
        .baseMove = MOVE_FLYING_PRESS, .chosenMove = MOVE_FLYING_PRESS, .moveType = TYPE_FIGHTING};
    CHECK(CalcTypeEffectivenessMultiplier(&wideMove) == UQ_4_12(1));
    wideMove.aiCalc = TRUE; CHECK(CalcTypeEffectivenessMultiplier(&wideMove) == UQ_4_12(1));
    wideMove.aiCalc = FALSE; wideMove.updateFlags = TRUE;
    gBattleStruct->moveResultFlags[1] = MOVE_RESULT_SUPER_EFFECTIVE | MOVE_RESULT_DOESNT_AFFECT_FOE;
    CHECK(CalcTypeEffectivenessMultiplier(&wideMove) == UQ_4_12(1)
        && !(gBattleStruct->moveResultFlags[1] & (MOVE_RESULT_SUPER_EFFECTIVE | MOVE_RESULT_DOESNT_AFFECT_FOE)));
    wideMove.updateFlags = FALSE;
    wideMove.move = wideMove.baseMove = wideMove.chosenMove = MOVE_CLOSE_COMBAT;
    CHECK(CalcTypeEffectivenessMultiplier(&wideMove) == 0xffffffffu);
    gParties[B_TRAINER_OPPONENT_A][0] = beforeWideOpponent;
    gBattleMons[1] = beforeWideBattler;
    struct Pokemon savedOpponent = gParties[B_TRAINER_OPPONENT_A][0];
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_CATERPIE, 20, 4323, OTID_STRUCT_PRESET(54321), 31);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    CHECK(gBattleMons[1].types[0] == TYPE_FIRE && gBattleMons[1].types[1] == TYPE_FLYING
        && gBattleMons[1].types[2] == crystalSlot);
    /* A public four-type species offer must reach the live battler and the
     * same damage, hazard, membership and type-change paths as ordinary types. */
    struct RhhExtendedTypeList fullTypes = {.struct_size = sizeof(fullTypes)};
    RhhContentKey abra = host->content_key("rhh:species/abra");
    CHECK(!types->species_list(abra, &fullTypes) && fullTypes.count == 4
        && fullTypes.types[0] == fire && fullTypes.types[1] == flying
        && fullTypes.types[2] == grass && fullTypes.types[3] == crystal);
    struct RhhTypeList tooSmall = {.struct_size = sizeof(tooSmall), .count = 99}, unchangedSmall = tooSmall;
    CHECK(types->species(abra, &tooSmall) && !memcmp(&tooSmall, &unchangedSmall, sizeof(tooSmall)));
    struct BattleTypeList speciesList;
    GetSpeciesTypeList(SPECIES_ABRA, &speciesList);
    CHECK(speciesList.count == 4 && speciesList.types[3] == crystalSlot && IsSpeciesOfType(SPECIES_ABRA, crystalSlot));
    struct Pokemon previousOpponent = gParties[B_TRAINER_OPPONENT_A][0];
    struct BattlePokemon previousBattler = gBattleMons[1];
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_ABRA, 20, 4325, OTID_STRUCT_PRESET(54321), 31);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    CHECK(gBattleMons[1].extraTypeCount == 1 && gBattleMons[1].extraTypes[0] == crystalSlot
        && IS_BATTLER_OF_TYPE(1, crystalSlot) && IsBattlerRawType(1, crystalSlot)
        && !IsBattlerRawType(1, TYPE_WATER));
    struct RhhPublicBattlerTypes publicView = {.struct_size = sizeof(publicView)};
    struct RhhPublicBattlerTypes publicTiny = {.struct_size = 1, .battler = 99}, savedPublicTiny = publicTiny;
    CHECK(types->public_battler_types(1, &publicTiny) && !memcmp(&publicTiny, &savedPublicTiny, sizeof(publicTiny))
        && types->public_battler_types(MAX_BATTLERS_COUNT, &publicTiny));
    typeof(gBattleStruct->illusion[1]) untouchedIllusion = gBattleStruct->illusion[1];
    gBattleStruct->illusion[1].state = ILLUSION_NOT_SET;
    typeof(gBattleStruct->illusion[1]) uncachedIllusion = gBattleStruct->illusion[1];
    rng_value_t publicRng = gRngValue, publicRng2 = gRng2Value;
    CHECK(!types->public_battler_types(1, &publicView) && publicView.battler == 1 && publicView.battle_id
        && !memcmp(&uncachedIllusion, &gBattleStruct->illusion[1], sizeof(uncachedIllusion))
        && !memcmp(&publicRng, &gRngValue, sizeof(publicRng)) && !memcmp(&publicRng2, &gRng2Value, sizeof(publicRng2)));
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    CHECK(!publicView.flags && publicView.types.count == 4 && publicView.types.types[3] == crystal);
#else
    CHECK(publicView.flags == RHH_PUBLIC_BATTLER_TYPES_UNKNOWN && !publicView.types.count);
#endif
    gBattleStruct->illusion[1] = untouchedIllusion;
    struct BattleTypeList publicTypes;
    GetBattlerPublicTypeList(1, &publicTypes);
    CHECK(publicTypes.count == 4 && publicTypes.types[3] == crystalSlot);
    struct Pokemon disguise;
    CreateMonWithIVs(&disguise, SPECIES_PIDGEY, 20, 9876, OTID_STRUCT_PRESET(54321), 31);
    typeof(gBattleStruct->illusion[1]) oldIllusion = gBattleStruct->illusion[1];
    gBattleStruct->illusion[1].state = ILLUSION_ON; gBattleStruct->illusion[1].mon = &disguise;
    GetBattlerPublicTypeList(1, &publicTypes);
    CHECK(publicTypes.count == 5 && publicTypes.types[3] == crystalSlot && publicTypes.types[4] == TYPE_WATER);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    CHECK(!types->public_battler_types(1, &publicView) && !publicView.flags && publicView.types.count == 5
        && publicView.types.types[3] == crystal && publicView.types.types[4] == water);
#endif
    u8 previousController = gBattlerBattleController[1];
    u8 previousAttackerController = gBattlerBattleController[0];
    gBattlerBattleController[1] = BATTLE_CONTROLLER_PLAYER;
    gBattlerBattleController[0] = BATTLE_CONTROLLER_OPPONENT;
    struct BattleTypeList knownTypes;
    AI_GetKnownBattlerTypeList(0, 1, &knownTypes);
    SaveBattlerData(1); SetBattlerData(1);
    struct BattleTypeList aiTypes;
    GetBattlerRawTypeList(1, &aiTypes);
    CHECK(BattleTypeListsEqual(&knownTypes, &aiTypes)
        && aiTypes.count == 5 && aiTypes.types[3] == crystalSlot && aiTypes.types[4] == TYPE_WATER);
    RestoreBattlerData(1); GetBattlerRawTypeList(1, &aiTypes);
    CHECK(aiTypes.count == 4 && aiTypes.types[3] == crystalSlot);
    CHECK(AI_GetMoveEffectiveness(MOVE_WATER_GUN, 0, 1) == UQ_4_12(0.5));
    CHECK(AI_GetMoveEffectiveness(MOVE_POUND, 0, 1) == UQ_4_12(0.5));
    gBattleMons[1].volatiles.roostActive = TRUE;
    GetBattlerPublicTypeList(1, &publicTypes);
    CHECK(publicTypes.count == 4 && !BattleTypeListContains(&publicTypes, TYPE_FLYING)
        && publicTypes.types[3] == TYPE_WATER);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    CHECK(!types->public_battler_types(1, &publicView) && publicView.types.count == 4
        && publicView.types.types[3] == water);
#endif
    gBattleMons[1].volatiles.roostActive = FALSE;
    RemoveBattlerType(1, crystalSlot);
    GetBattlerPublicTypeList(1, &publicTypes);
    CHECK(publicTypes.count == 3 && !BattleTypeListContains(&publicTypes, TYPE_WATER));
    AI_GetKnownBattlerTypeList(0, 1, &knownTypes);
    SaveBattlerData(1); SetBattlerData(1);
    GetBattlerRawTypeList(1, &aiTypes);
    CHECK(BattleTypeListsEqual(&aiTypes, &publicTypes) && BattleTypeListsEqual(&knownTypes, &aiTypes));
    RestoreBattlerData(1); GetBattlerRawTypeList(1, &aiTypes);
    CHECK(BattleTypeListsEqual(&aiTypes, &publicTypes));
    CHECK(AI_GetMoveEffectiveness(MOVE_WATER_GUN, 0, 1) == UQ_4_12(1));
    GetBattlerRawTypeList(1, &aiTypes); CHECK(BattleTypeListsEqual(&aiTypes, &publicTypes));
    gBattlerBattleController[1] = previousController;
    gBattlerBattleController[0] = previousAttackerController;
    gBattleStruct->illusion[1] = oldIllusion;
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    ResetSpriteData(); FreeAllSpritePalettes();
    LoadTypeIcons(B_BATTLER_0);
    struct Sprite *pageIcons[3] = {0};
    for (u32 i = 0; i < MAX_SPRITES; ++i)
        if (gSprites[i].inUse && gSprites[i].tMonPosition == B_POSITION_OPPONENT_LEFT
            && gSprites[i].tTypeIconSlot < 3) pageIcons[gSprites[i].tTypeIconSlot] = &gSprites[i];
    CHECK(pageIcons[0] && pageIcons[1] && pageIcons[2] && pageIcons[0]->animNum == TYPE_FIRE
        && pageIcons[1]->animNum == TYPE_FLYING && pageIcons[2]->animNum == TYPE_GRASS);
    for (u32 frame = 0; frame < 90; ++frame)
        for (u32 i = 0; i < 3; ++i) RhhTestAdvanceBattleTypeIcon(pageIcons[i]);
    CHECK(!pageIcons[0]->invisible && pageIcons[0]->tTypeIconType == crystalSlot
        && gPlttBufferUnfaded[OBJ_PLTT_ID(pageIcons[0]->oam.paletteNum) + 2] == RGB(16, 21, 28)
        && pageIcons[1]->invisible && pageIcons[2]->invisible);
    RemoveBattlerType(1, crystalSlot);
    for (u32 frame = 0; frame < 15; ++frame)
        for (u32 i = 0; i < 3; ++i) RhhTestAdvanceBattleTypeIcon(pageIcons[i]);
    CHECK(!pageIcons[0]->invisible && !pageIcons[1]->invisible && !pageIcons[2]->invisible
        && pageIcons[0]->animNum == TYPE_FIRE && pageIcons[2]->animNum == TYPE_GRASS);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    ResetSpriteData(); FreeAllSpritePalettes(); ModsReclaimCustomTypeIcons();
#endif
    CHECK(!types->battler_list(1, &fullTypes) && fullTypes.count == 4 && fullTypes.types[3] == crystal);
    CHECK(types->battler(1, &tooSmall) && !memcmp(&tooSmall, &unchangedSmall, sizeof(tooSmall)));
    struct DamageContext extendedStab = {.battlerAtk = 1, .battlerDef = 0, .move = MOVE_TACKLE,
        .baseMove = MOVE_TACKLE, .chosenMove = MOVE_TACKLE, .moveType = TYPE_NORMAL,
        .typeEffectivenessModifier = UQ_4_12(1)};
    CHECK(ApplyModifiersAfterDmgRoll(&extendedStab, 100) == 200);
    extendedStab.aiCalc = TRUE; CHECK(ApplyModifiersAfterDmgRoll(&extendedStab, 100) == 200);
    extendedStab.aiCalc = FALSE; extendedStab.move = extendedStab.baseMove = extendedStab.chosenMove = MOVE_WATER_GUN;
    extendedStab.moveType = TYPE_WATER;
    CHECK(ApplyModifiersAfterDmgRoll(&extendedStab, 100) == 100); /* Constraint denial is sticky. */
    struct BattlePokemon previousAttacker = gBattleMons[0];
    enum BattlerId previousAttackerId = gBattlerAttacker, previousTargetId = gBattlerTarget;
    enum Move previousMove = gCurrentMove;
    enum Move previousChosenMove = gChosenMove;
    const u8 *previousScript = gBattlescriptCurrInstr;
    u8 failedScript = 0;
    struct __attribute__((packed)) { u8 opcode; void (*func)(void); const u8 *fail; } command =
        {0, BS_TryThirdType, &failedScript};
    gBattlerAttacker = 0; gBattlerTarget = 1; gCurrentMove = MOVE_TRICK_OR_TREAT;
    struct ChooseMoveStruct choice = {0};
    memcpy(choice.monTypes, gBattleMons[1].types, sizeof(choice.monTypes));
    CHECK(!IsMoveSelectionCurseGhost(1, &choice));
    gBattlescriptCurrInstr = (const u8 *)&command; BS_TryThirdType();
    CHECK(gBattlescriptCurrInstr == (const u8 *)&command + sizeof(command)
        && gBattleMons[1].extraTypeCount == 2 && IS_BATTLER_OF_TYPE(1, TYPE_GHOST)
        && gBattleMons[1].addedTypeValid && gBattleMons[1].addedType == TYPE_GHOST);
    /* Replace the added type rather than accumulating it. The inherent
     * overflow type and the other three active types must be retained. */
    gBattleMons[1].types[2] = TYPE_WATER;
    gCurrentMove = MOVE_FORESTS_CURSE;
    gBattlescriptCurrInstr = (const u8 *)&command; BS_TryThirdType();
    CHECK(gBattlescriptCurrInstr == (const u8 *)&command + sizeof(command)
        && gBattleMons[1].extraTypeCount == 2 && IS_BATTLER_OF_TYPE(1, TYPE_GRASS)
        && IS_BATTLER_OF_TYPE(1, TYPE_WATER) && IS_BATTLER_OF_TYPE(1, crystalSlot)
        && !IS_BATTLER_OF_TYPE(1, TYPE_GHOST) && gBattleMons[1].addedType == TYPE_GRASS);
    gCurrentMove = MOVE_TRICK_OR_TREAT;
    gBattlescriptCurrInstr = (const u8 *)&command; BS_TryThirdType();
    CHECK(gBattleMons[1].extraTypeCount == 2 && IS_BATTLER_OF_TYPE(1, TYPE_GHOST)
        && !IS_BATTLER_OF_TYPE(1, TYPE_GRASS) && gBattleMons[1].addedType == TYPE_GHOST);
    gBattleMons[1].types[2] = TYPE_GRASS;
    SaveBattlerData(1); SET_BATTLER_TYPE(1, TYPE_NORMAL); RestoreBattlerData(1);
    CHECK(gBattleMons[1].addedTypeValid && gBattleMons[1].addedType == TYPE_GHOST
        && IS_BATTLER_OF_TYPE(1, crystalSlot));
    struct BattlePokemon beforeCapacity = gBattleMons[1];
    memset(gBattleMons[1].extraTypes, TYPE_ROCK, sizeof(gBattleMons[1].extraTypes));
    gBattleMons[1].types[2] = TYPE_WATER;
    gBattleMons[1].extraTypeCount = sizeof(gBattleMons[1].extraTypes);
    gBattleMons[1].extraTypes[gBattleMons[1].extraTypeCount - 1] = TYPE_GHOST;
    gCurrentMove = MOVE_FORESTS_CURSE;
    gBattlescriptCurrInstr = (const u8 *)&command; BS_TryThirdType();
    CHECK(gBattlescriptCurrInstr == (const u8 *)&command + sizeof(command)
        && gBattleMons[1].extraTypeCount == sizeof(gBattleMons[1].extraTypes)
        && gBattleMons[1].extraTypes[gBattleMons[1].extraTypeCount - 1] == TYPE_GRASS);
    gBattleMons[1].addedTypeValid = FALSE; gCurrentMove = MOVE_TRICK_OR_TREAT;
    gBattlescriptCurrInstr = (const u8 *)&command; BS_TryThirdType();
    CHECK(gBattlescriptCurrInstr == &failedScript && !gBattleMons[1].addedTypeValid
        && gBattleMons[1].extraTypes[gBattleMons[1].extraTypeCount - 1] == TYPE_GRASS);
    gBattleMons[1] = beforeCapacity;
    CHECK(IsMoveSelectionCurseGhost(1, &choice));
    /* The preview must use the same effective type as Curse and its Z effect,
     * including an overflow Ghost type, non-Ghost Tera and Ghost Tera. */
    u8 curseOldTera = GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE);
    u8 curseTera = TYPE_WATER;
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE, &curseTera);
    SetActiveGimmick(1, GIMMICK_TERA);
    CHECK(!IS_BATTLER_OF_TYPE(1, TYPE_GHOST) && !IsMoveSelectionCurseGhost(1, &choice));
    RemoveBattlerType(1, TYPE_GHOST); /* Terastallization preserves the underlying types. */
    curseTera = TYPE_GHOST;
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE, &curseTera);
    CHECK(IS_BATTLER_OF_TYPE(1, TYPE_GHOST) && IsMoveSelectionCurseGhost(1, &choice));
    SetActiveGimmick(1, GIMMICK_NONE);
    RemoveBattlerType(1, TYPE_GHOST);
    CHECK(!IS_BATTLER_OF_TYPE(1, TYPE_GHOST) && !IsMoveSelectionCurseGhost(1, &choice));
    SetActiveGimmick(1, GIMMICK_TERA);
    CHECK(IS_BATTLER_OF_TYPE(1, TYPE_GHOST) && IsMoveSelectionCurseGhost(1, &choice));
    SetActiveGimmick(1, GIMMICK_NONE);
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE, &curseOldTera);
    gBattleMons[1] = beforeCapacity;
    u32 previousBattleFlags = gBattleTypeFlags; gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(!IsMoveSelectionCurseGhost(1, &choice));
    gBattleTypeFlags = previousBattleFlags;
    gBattlescriptCurrInstr = (const u8 *)&command; BS_TryThirdType();
    CHECK(gBattlescriptCurrInstr == &failedScript && gBattleMons[1].extraTypeCount == 2);
    command.func = BS_TryReflectType; gBattlescriptCurrInstr = (const u8 *)&command; BS_TryReflectType();
    CHECK(gBattlescriptCurrInstr == (const u8 *)&command + sizeof(command)
        && IS_BATTLER_OF_TYPE(0, crystalSlot) && IS_BATTLER_OF_TYPE(0, TYPE_GHOST)
        && gBattleMons[0].extraTypeCount == 2 && gBattleMons[0].addedTypeValid
        && gBattleMons[0].addedType == TYPE_GHOST);
    struct BattlePokemon beforeCopiedThird = gBattleMons[1];
    gBattleMons[1].types[0] = TYPE_FIRE; gBattleMons[1].types[1] = TYPE_WATER;
    gBattleMons[1].types[2] = TYPE_ROCK; gBattleMons[1].extraTypeCount = 0;
    gBattleMons[1].addedTypeValid = FALSE;
    gBattlescriptCurrInstr = (const u8 *)&command; BS_TryReflectType();
    CHECK(IS_BATTLER_OF_TYPE(0, TYPE_ROCK) && !gBattleMons[0].addedTypeValid);
    gBattlerAttacker = 1; gBattlerTarget = 0; command.func = BS_TryThirdType;
    gBattlescriptCurrInstr = (const u8 *)&command; BS_TryThirdType();
    CHECK(gBattlescriptCurrInstr == (const u8 *)&command + sizeof(command)
        && IS_BATTLER_OF_TYPE(0, TYPE_FIRE) && IS_BATTLER_OF_TYPE(0, TYPE_WATER)
        && IS_BATTLER_OF_TYPE(0, TYPE_ROCK) && IS_BATTLER_OF_TYPE(0, TYPE_GHOST));
    gBattlerAttacker = 0; gBattlerTarget = 1; gBattleMons[1] = beforeCopiedThird;
    gBattleMons[0] = previousAttacker;
    RemoveBattlerType(1, TYPE_GHOST);
    CHECK(gBattleMons[1].extraTypeCount == 1 && !IS_BATTLER_OF_TYPE(1, TYPE_GHOST)
        && !gBattleMons[1].addedTypeValid);
    RemoveBattlerType(1, TYPE_FIRE); RemoveBattlerType(1, TYPE_FLYING);
    command.func = BS_TryReflectType; gBattlescriptCurrInstr = (const u8 *)&command; BS_TryReflectType();
    GetBattlerTypeList(0, FALSE, &aiTypes);
    CHECK(aiTypes.count == 2 && aiTypes.types[0] == TYPE_GRASS && aiTypes.types[1] == crystalSlot
        && !BattleTypeListContains(&aiTypes, TYPE_NORMAL));
    gBattleMons[0] = previousAttacker;
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    command.func = BS_TrySoak; gCurrentMove = MOVE_SOAK;
    gBattlescriptCurrInstr = (const u8 *)&command; BS_TrySoak();
    CHECK(gBattlescriptCurrInstr == (const u8 *)&command + sizeof(command)
        && !gBattleMons[1].extraTypeCount && IS_BATTLER_OF_TYPE(1, TYPE_WATER)
        && !IS_BATTLER_OF_TYPE(1, crystalSlot));
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    const u8 transformCommand[] = {B_SCR_OP_TRANSFORMDATAEXECUTION};
    gCurrentMove = MOVE_TRANSFORM; gBattleControllerExecFlags = 0;
    gBattlescriptCurrInstr = transformCommand; gBattleScriptingCommandsTable[B_SCR_OP_TRANSFORMDATAEXECUTION]();
    CHECK(gBattleMons[0].volatiles.transformed && gBattleMons[0].extraTypeCount == 1
        && IS_BATTLER_OF_TYPE(0, crystalSlot));
    gBattleMons[0] = previousAttacker;
    struct BattlePokemon switchData; PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &switchData);
    memcpy(&resources.bufferB[1][4], &switchData, BATTLE_POKEMON_TRANSFER_SIZE);
    gBattleStruct->monToSwitchIntoId[1] = 0; gBattleControllerExecFlags = 0;
    const u8 switchCommand[] = {B_SCR_OP_SWITCHINDATAUPDATE, BS_TARGET};
    gBattlescriptCurrInstr = switchCommand; gBattleScriptingCommandsTable[B_SCR_OP_SWITCHINDATAUPDATE]();
    CHECK(gBattleMons[1].species == SPECIES_ABRA && gBattleMons[1].extraTypeCount == 1
        && IS_BATTLER_OF_TYPE(1, crystalSlot));
    ModsBattleSwitchEntered(1);
    gBattlerAttacker = previousAttackerId; gBattlerTarget = previousTargetId;
    gCurrentMove = previousMove; gChosenMove = previousChosenMove; gBattlescriptCurrInstr = previousScript;
    struct DamageContext fourTypeDamage = {.battlerAtk = 0, .battlerDef = 1, .move = MOVE_FLAMETHROWER,
        .baseMove = MOVE_FLAMETHROWER, .chosenMove = MOVE_FLAMETHROWER, .moveType = TYPE_FIRE};
    CHECK(CalcTypeEffectivenessMultiplier(&fourTypeDamage) == UQ_4_12(0.125));
    CHECK(GetStealthHazardDamage(TYPE_SIDE_HAZARD_POINTED_STONES, 1) == gBattleMons[1].maxHP / 2);
    gBattleMons[1].volatiles.roostActive = TRUE;
    CHECK(!IS_BATTLER_OF_TYPE(1, TYPE_FLYING) && IS_BATTLER_OF_TYPE(1, crystalSlot));
    CHECK(!types->battler_list(1, &fullTypes) && fullTypes.count == 3 && fullTypes.types[2] == crystal);
    gBattleMons[1].volatiles.roostActive = FALSE;
    u8 previousTera = GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE), waterTera = TYPE_WATER;
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE, &waterTera);
    SetActiveGimmick(1, GIMMICK_TERA);
    CHECK(!types->battler_list(1, &fullTypes) && fullTypes.count == 1 && fullTypes.types[0] == water);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    CHECK(!types->public_battler_types(1, &publicView) && publicView.types.count == 1
        && publicView.types.types[0] == water);
#endif
    GetBattlerTypeList(1, TRUE, &speciesList);
    CHECK(speciesList.count == 4 && speciesList.types[3] == crystalSlot);
    extendedStab.move = extendedStab.baseMove = extendedStab.chosenMove = MOVE_TACKLE;
    extendedStab.moveType = TYPE_NORMAL;
    CHECK(ApplyModifiersAfterDmgRoll(&extendedStab, 100) == 200);
    SetActiveGimmick(1, GIMMICK_NONE);
    SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE, &previousTera);
    RemoveBattlerType(1, crystalSlot);
    CHECK(!IS_BATTLER_OF_TYPE(1, crystalSlot) && !gBattleMons[1].extraTypeCount);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    RemoveBattlerType(1, TYPE_FIRE); RemoveBattlerType(1, TYPE_FLYING); RemoveBattlerType(1, TYPE_GRASS);
    CHECK(GetDynamicMoveType(&gParties[B_TRAINER_OPPONENT_A][0], MOVE_REVELATION_DANCE, 1,
        ABILITY_NONE, HOLD_EFFECT_NONE, MON_IN_BATTLE) == crystalSlot);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    SET_BATTLER_TYPE(1, TYPE_WATER);
    CHECK(!gBattleMons[1].extraTypeCount && IS_BATTLER_OF_TYPE(1, TYPE_WATER)
        && !IS_BATTLER_OF_TYPE(1, crystalSlot));
    gParties[B_TRAINER_OPPONENT_A][0] = previousOpponent;
    gBattleMons[1] = previousBattler;
    CHECK(IsTypeStellarBoosted(0, crystalSlot));
    ExpendTypeStellarBoost(0, crystalSlot);
    CHECK(!IsTypeStellarBoosted(0, crystalSlot) && IsTypeStellarBoosted(1, crystalSlot));
    CHECK(!types->battler(1, &list) && list.count == 3
        && list.types[0] == fire && list.types[1] == flying && list.types[2] == crystal);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    ResetSpriteData(); FreeAllSpritePalettes();
    LoadTypeIcons(B_BATTLER_0);
    u32 customIcons = 0; struct Sprite *customIcon = NULL;
    for (u32 i = 0; i < MAX_SPRITES; ++i)
    {
        struct Sprite *sprite = &gSprites[i];
        if (sprite->inUse && sprite->tMonPosition == B_POSITION_OPPONENT_LEFT
            && sprite->oam.paletteNum < 12
            && gPlttBufferUnfaded[OBJ_PLTT_ID(sprite->oam.paletteNum) + 2] == RGB(16, 21, 28))
        { ++customIcons; customIcon = sprite; }
    }
    CHECK(customIcons == 1 && customIcon);
    u16 customTag = GetSpriteTileTagByTileStart(customIcon->sheetTileStart);
    customIcon->tHideIconTimer = NUM_FRAMES_HIDE_TYPE_ICON;
    customIcon->callback(customIcon);
    CHECK(!customIcon->inUse && GetSpriteTileStartByTag(customTag) == TAG_NONE
        && GetSpriteTileStartByTag(TYPE_ICON_TAG) != TAG_NONE);
    ResetSpriteData(); FreeAllSpritePalettes(); ModsReclaimCustomTypeIcons();
#endif
    struct DamageContext customDefender = {.battlerAtk = B_BATTLER_0, .battlerDef = B_BATTLER_1,
        .move = MOVE_FLAMETHROWER, .baseMove = MOVE_FLAMETHROWER, .chosenMove = MOVE_FLAMETHROWER,
        .moveType = TYPE_FIRE};
    struct DamageContext wideContext = customDefender;
    wideContext.move = wideContext.baseMove = wideContext.chosenMove = MOVE_REVELATION_DANCE;
    wideContext.moveType = crystalSlot;
    CHECK(GetTypeEffectivenessForTypes(&wideContext, UQ_4_12(1), hazardTypes, 20) == 0xffffffffu);
    CHECK(GetTypeEffectivenessForTypes(&wideContext, UQ_4_12(1), hazardTypes + 20, 20) == 1);
    CHECK(GetTypeEffectivenessForTypes(&wideContext, UQ_4_12(1), hazardTypes, 42) == UQ_4_12(1));
    CHECK(GetTypeEffectivenessForTypes(&wideContext, UQ_4_12(1), reverseHazardTypes, 40) == UQ_4_12(1));
    wideContext.move = wideContext.baseMove = wideContext.chosenMove = MOVE_THUNDERBOLT;
    wideContext.moveType = TYPE_ELECTRIC;
    CHECK(!GetTypeEffectivenessForTypes(&wideContext, UQ_4_12(1), immuneHazardTypes, ARRAY_COUNT(immuneHazardTypes)));
    wideContext.holdEffects[B_BATTLER_1] = HOLD_EFFECT_RING_TARGET;
    CHECK(GetTypeEffectivenessForTypes(&wideContext, UQ_4_12(1), immuneHazardTypes, ARRAY_COUNT(immuneHazardTypes)) == UQ_4_12(4));
    wideContext.move = wideContext.baseMove = wideContext.chosenMove = MOVE_TACKLE;
    wideContext.moveType = TYPE_NORMAL;
    wideContext.typeEffectivenessModifier = 0xffffffffu;
    CHECK(ApplyModifiersAfterDmgRoll(&wideContext, 1) > 1000000);
    CHECK(CalcTypeEffectivenessMultiplier(&customDefender) == UQ_4_12(0.25));
    customDefender.aiCalc = TRUE;
    CHECK(CalcTypeEffectivenessMultiplier(&customDefender) == UQ_4_12(0.25));
    CHECK(AI_GetBattlerTypeMatchup(0, 1) == UQ_4_12(0.5));
    CHECK(GetStealthHazardDamage(TYPE_SIDE_HAZARD_POINTED_STONES, 1) == gBattleMons[1].maxHP / 2);
    enum Type savedAttackerTypes[3]; memcpy(savedAttackerTypes, gBattleMons[0].types, sizeof(savedAttackerTypes));
    gBattleMons[0].types[0] = gBattleMons[0].types[1] = crystalSlot;
    gBattleMons[0].types[2] = TYPE_MYSTERY;
    CHECK(GetDynamicMoveType(&gParties[B_TRAINER_PLAYER][0], MOVE_REVELATION_DANCE, 0,
        ABILITY_NONE, HOLD_EFFECT_NONE, MON_IN_BATTLE) == crystalSlot);
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    CHECK(RhhTestMoveSelectionType(0, MOVE_REVELATION_DANCE) == crystalSlot);
#endif
    SetTypeBeforeUsingMove(MOVE_REVELATION_DANCE, 0, ABILITY_NONE, HOLD_EFFECT_NONE);
    CHECK(GetBattleMoveType(MOVE_REVELATION_DANCE) == crystalSlot
        && GetBattleMoveCategory(MOVE_REVELATION_DANCE) == GetMoveCategory(MOVE_REVELATION_DANCE));
    CHECK(GetMaxMove(0, MOVE_REVELATION_DANCE) == MOVE_MAX_STRIKE);
    gBattleStruct->dynamicMoveType = crystalSlot;
    CHECK(GetTypeBasedZMove(MOVE_WEATHER_BALL) == MOVE_BREAKNECK_BLITZ);
    gBattleStruct->dynamicMoveType = TYPE_NONE;
    struct DamageContext customStab = {.battlerAtk = B_BATTLER_0, .battlerDef = B_BATTLER_1,
        .move = MOVE_TACKLE, .baseMove = MOVE_TACKLE, .chosenMove = MOVE_TACKLE,
        .moveType = crystalSlot, .typeEffectivenessModifier = UQ_4_12(1)};
    u8 oldStellarTera = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE), stellarTera = TYPE_STELLAR;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE, &stellarTera);
    gBattleStruct->stellarCustomBoostFlags[B_TRAINER_PLAYER][0] = 0;
    SetActiveGimmick(0, GIMMICK_TERA);
    CHECK(GetTeraMultiplier(&customStab) == UQ_4_12(2));
    ExpendTypeStellarBoost(0, crystalSlot);
    CHECK(GetTeraMultiplier(&customStab) == UQ_4_12(1.5));
    SetActiveGimmick(0, GIMMICK_NONE);
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE, &oldStellarTera);
    CHECK(ApplyModifiersAfterDmgRoll(&customStab, 100) == 150);
    memcpy(gBattleMons[0].types, savedAttackerTypes, sizeof(savedAttackerTypes));
#ifdef RHH_BATTLE_TYPE_ICON_TEST
    CHECK(RhhTestMoveSelectionType(0, MOVE_REVELATION_DANCE) == GetMoveType(MOVE_REVELATION_DANCE));
#endif
    gParties[B_TRAINER_OPPONENT_A][0] = savedOpponent;
    gBattleMons[1].species = SPECIES_CHARIZARD;
    gBattleMons[1].types[0] = TYPE_FIRE; gBattleMons[1].types[1] = TYPE_FLYING; gBattleMons[1].types[2] = TYPE_GRASS;
    CHECK(AI_GetBattlerTypeMatchup(0, 1) == UQ_4_12(0.5)); /* Modded Fire/Grass chart applies to switch AI. */
    CHECK(!types->battler(1, &list) && list.count == 3 && list.types[0] == fire && list.types[1] == flying && list.types[2] == grass);
    gBattleMons[1].volatiles.roostActive = TRUE;
    CHECK(!types->battler(1, &list) && list.count == 2 && list.types[0] == fire && list.types[1] == grass);
    gBattleMons[1].volatiles.roostActive = FALSE;
    u8 tera = TYPE_WATER; SetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_TERA_TYPE, &tera);
    SetActiveGimmick(1, GIMMICK_TERA);
    CHECK(!types->battler(1, &list) && list.count == 1 && list.types[0] == water);
    CHECK(AI_GetBattlerTypeMatchup(0, 1) == UQ_4_12(1)); /* Tera replaces the defender's types. */
    SetActiveGimmick(1, GIMMICK_NONE);
    gBattleMons[1].types[2] = TYPE_FIRE; CHECK(!types->battler(1, &list) && list.count == 2);
    struct DamageContext stab = {.battlerAtk = B_BATTLER_0, .battlerDef = B_BATTLER_1,
        .move = MOVE_FLAMETHROWER, .baseMove = MOVE_FLAMETHROWER, .chosenMove = MOVE_FLAMETHROWER,
        .moveType = TYPE_FIRE, .typeEffectivenessModifier = UQ_4_12(1)};
    CHECK(ApplyModifiersAfterDmgRoll(&stab, 100) == 200);
    stab.aiCalc = TRUE; CHECK(ApplyModifiersAfterDmgRoll(&stab, 100) == 200); stab.aiCalc = FALSE;
    struct AiCalcValues aiValues = {.move = MOVE_FLAMETHROWER}; rng_value_t beforeAi = gRngValue;
    RhhModsSetRunContext(123, 0, 0);
    struct SimulatedDamage originalAi = AI_CalcDamage(&aiValues, B_BATTLER_0, B_BATTLER_1);
    gRngValue = beforeAi; RhhModsSetRunContext(123, 456, 0);
    struct SimulatedDamage modifiedAi = AI_CalcDamage(&aiValues, B_BATTLER_0, B_BATTLER_1);
    CHECK(originalAi.maximum && modifiedAi.maximum > originalAi.maximum);
    gRngValue = beforeAi;
    u8 fireTera = TYPE_FIRE; SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_TERA_TYPE, &fireTera);
    SetActiveGimmick(0, GIMMICK_TERA); stab.abilities[0] = ABILITY_ADAPTABILITY;
    CHECK(ApplyModifiersAfterDmgRoll(&stab, 100) == 200); SetActiveGimmick(0, GIMMICK_NONE); stab.abilities[0] = ABILITY_NONE;
    stab.move = stab.baseMove = stab.chosenMove = MOVE_TACKLE; stab.moveType = TYPE_NORMAL;
    CHECK(ApplyModifiersAfterDmgRoll(&stab, 100) == 100); /* Invalid transform falls back. */
    stab.move = stab.baseMove = stab.chosenMove = MOVE_WATER_GUN; stab.moveType = TYPE_WATER;
    CHECK(ApplyModifiersAfterDmgRoll(&stab, 100) == 100); /* Sticky denial preserves base. */
    stab.move = stab.baseMove = stab.chosenMove = MOVE_STRUGGLE; stab.moveType = TYPE_NORMAL;
    CHECK(ApplyModifiersAfterDmgRoll(&stab, 100) == 100); /* Struggle never invokes the hook. */
    CHECK(Effectiveness(MOVE_FLAMETHROWER, SPECIES_VENUSAUR, ABILITY_NONE, ABILITY_NONE) == UQ_4_12(0.5));
    CHECK(Effectiveness(MOVE_EARTHQUAKE, SPECIES_PIDGEOT, ABILITY_NONE, ABILITY_NONE) == UQ_4_12(1));
    CHECK(Effectiveness(MOVE_EARTHQUAKE, SPECIES_PIDGEOT, ABILITY_NONE, ABILITY_LEVITATE) == UQ_4_12(0));
    struct DamageContext balloon = {.battlerAtk = B_BATTLER_0, .battlerDef = B_BATTLER_1,
        .move = MOVE_EARTHQUAKE, .baseMove = MOVE_EARTHQUAKE, .chosenMove = MOVE_EARTHQUAKE, .moveType = TYPE_GROUND};
    balloon.holdEffects[B_BATTLER_1] = HOLD_EFFECT_AIR_BALLOON;
    CHECK(CalcTypeEffectivenessMultiplier(&balloon) == UQ_4_12(0));
    CHECK(Effectiveness(MOVE_POUND, SPECIES_GENGAR, ABILITY_SCRAPPY, ABILITY_NONE) == UQ_4_12(1));
    CHECK(CalcPartyMonTypeEffectivenessMultiplier(MOVE_FLAMETHROWER, SPECIES_VENUSAUR, ABILITY_NONE) == UQ_4_12(0.5));
    CHECK(CalcPartyMonTypeEffectivenessMultiplier(MOVE_EARTHQUAKE, SPECIES_PIDGEOT, ABILITY_NONE) == UQ_4_12(1));
    CHECK(CalcPartyMonTypeEffectivenessMultiplier(MOVE_EARTHQUAKE, SPECIES_PIDGEOT, ABILITY_LEVITATE) == UQ_4_12(0));
    CHECK(GetStealthHazardDamageByTypesAndHP(TYPE_SIDE_HAZARD_POINTED_STONES, TYPE_FLYING, TYPE_FLYING, 160) == 20);
    gBattleTypeFlags = BATTLE_TYPE_LINK;
    u8 excludedSpeciesTypes[3] = {TYPE_BUG, TYPE_BUG, TYPE_MYSTERY};
    ModsSpeciesTypes(SPECIES_CATERPIE, excludedSpeciesTypes);
    CHECK(excludedSpeciesTypes[0] == TYPE_BUG && excludedSpeciesTypes[1] == TYPE_BUG
        && excludedSpeciesTypes[2] == TYPE_MYSTERY);
    CHECK(!types->chart(fire, grass, 0, &chart) && (chart.flags & RHH_TYPE_MATCHUP_EXCLUDED) && chart.effective == RHH_TYPE_ONE * 2);
    CHECK(!types->chart(crystal, fire, 0, &chart) && (chart.flags & RHH_TYPE_MATCHUP_EXCLUDED)
        && chart.base == RHH_TYPE_ONE * 2 && chart.adjusted == chart.base);
    CHECK(!types->compose(crystal, &fire, 1, 0, &composition)
        && composition.effective == RHH_TYPE_ONE * 2 && (composition.flags & RHH_TYPE_MATCHUP_EXCLUDED));
    CHECK(Effectiveness(MOVE_FLAMETHROWER, SPECIES_VENUSAUR, ABILITY_NONE, ABILITY_NONE) == UQ_4_12(2));
    gBattleMons[0].types[0] = gBattleMons[0].types[1] = TYPE_FIRE;
    stab.move = stab.baseMove = stab.chosenMove = MOVE_FLAMETHROWER; stab.moveType = TYPE_FIRE;
    CHECK(ApplyModifiersAfterDmgRoll(&stab, 100) == 150);
    gBattleTypeFlags = BATTLE_TYPE_RECORDED;
    CHECK(GetTypeModifier(TYPE_FIRE, TYPE_GRASS) == UQ_4_12(2));
    CHECK(!types->chart(crystal, fire, 0, &chart) && (chart.flags & RHH_TYPE_MATCHUP_EXCLUDED)
        && chart.adjusted == RHH_TYPE_ONE * 2);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)));
    ModsBattleReset(); gMain.inBattle = FALSE; RhhModsUnloadLibraries();
    puts("type chart: stable keys, engine damage, AI chart, hazards, inverse and link exclusions passed");
}

static const struct RhhModType sEarlierSaveType = {
    sizeof(struct RhhModType), 0xb08040, 0, 0, "aaa:type/amber", "Amber", NULL
};
static const struct RhhModDefinition sEarlierSaveMod = {
    .struct_size = sizeof(sEarlierSaveMod), .abi_version = RHH_MOD_ABI_TYPES, .id = "aaa:earlier_type",
    .type_count = 1, .types = &sEarlierSaveType
};
