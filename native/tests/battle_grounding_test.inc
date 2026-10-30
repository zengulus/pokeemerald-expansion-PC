#include "battle_ai_util.h"
#include "constants/battle.h"
#include "constants/item_effects.h"

static void CheckBattleGrounding(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(module) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhBattleGroundingService *service = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/battle_grounding", RHH_SERVICE_VERSION(1, 0));
    CHECK(service && service->header.struct_size >= RHH_BATTLE_GROUNDING_SERVICE_V1_1_SIZE
        && RHH_BATTLE_GROUNDING_SERVICE_V1_0_SIZE < RHH_BATTLE_GROUNDING_SERVICE_V1_1_SIZE
        && RHH_BATTLE_GROUNDING_SERVICE_V1_1_SIZE < RHH_BATTLE_GROUNDING_SERVICE_V1_2_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/battle_grounding", RHH_SERVICE_VERSION(1, 1)) == &service->header
        && RhhModsHostAPI()->get_service("rhh:service/battle_grounding", RHH_SERVICE_VERSION(1, 2)) == &service->header
        && !RhhModsHostAPI()->get_service("rhh:service/battle_grounding", RHH_SERVICE_VERSION(1, 3)));
    struct RhhBattleGrounding view = {.struct_size = sizeof(view), .flags = 777}, saved = view;
    struct RhhBattleGrounding small = {.struct_size = 1, .flags = 777}, savedSmall = small;
    CHECK(service->live(0, &view) && !memcmp(&view, &saved, sizeof(view)) && service->ai(0, &view)
        && !memcmp(&view, &saved, sizeof(view)) && service->live(0, NULL)
        && service->preview_live(0, RHH_TERRAIN_GRASSY, &view) && !memcmp(&view, &saved, sizeof(view))
        && service->preview_ai(0, RHH_TERRAIN_GRASSY, &view) && !memcmp(&view, &saved, sizeof(view))
        && service->entry_ai(&view) && !memcmp(&view, &saved, sizeof(view))
        && service->preview_entry_ai(RHH_TERRAIN_GRASSY, &view) && !memcmp(&view, &saved, sizeof(view)));
    u32 *settings = RhhModsState("test:battle_grounding"); CHECK(settings);
    struct BattleResources resources = {0}; struct AiLogicData ai = {0};
    struct BattleHistory history = {0}; struct AiPartyData parties = {0};
    gBattleResources = &resources; gAiLogicData = &ai; gBattleHistory = &history; gAiPartyData = &parties;
    PrepareTurnBattle(); gFieldStatuses = 0; memset(&gFieldTimers, 0, sizeof(gFieldTimers)); ModsBattleBegin();
    gFieldTimers.terrain = B_TERRAIN_GRASSY;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    gPotentialItemEffectBattler = 3;
    CHECK(service->live(0, &small) && !memcmp(&small, &savedSmall, sizeof(small))
        && service->live(99, &view) && !memcmp(&view, &saved, sizeof(view))
        && !service->live(0, &view) && view.terrain == RHH_TERRAIN_GRASSY && view.engine_terrain == B_TERRAIN_GRASSY
        && (view.flags & (RHH_BATTLE_GROUNDING_GROUNDED | RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED))
            == (RHH_BATTLE_GROUNDING_GROUNDED | RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED)
        && view.run_id == 456 && view.battle_id && gPotentialItemEffectBattler == 3);
    struct FieldTimer oldTimers = gFieldTimers;
    struct RhhBattleGrounding preview = {.struct_size = sizeof(preview), .flags = 777}, oldPreview = preview;
    CHECK(service->preview_live(0, 999, &preview) && !memcmp(&preview, &oldPreview, sizeof(preview))
        && service->preview_live(99, RHH_TERRAIN_GRASSY, &preview) && !memcmp(&preview, &oldPreview, sizeof(preview))
        && service->preview_live(0, RHH_TERRAIN_GRASSY, &small) && !memcmp(&small, &savedSmall, sizeof(small))
        && service->preview_ai(0, RHH_TERRAIN_GRASSY, &preview) && !memcmp(&preview, &oldPreview, sizeof(preview)));
    const u32 engineTerrains[] = {B_TERRAIN_NONE, B_TERRAIN_GRASSY, B_TERRAIN_MISTY, B_TERRAIN_ELECTRIC, B_TERRAIN_PSYCHIC};
    for (u32 terrain = RHH_TERRAIN_NONE; terrain <= RHH_TERRAIN_PSYCHIC; ++terrain)
        CHECK(!service->preview_live(0, terrain, &preview) && preview.terrain == terrain
            && preview.engine_terrain == engineTerrains[terrain] && (preview.flags & RHH_BATTLE_GROUNDING_SCENARIO)
            && (preview.flags & RHH_BATTLE_GROUNDING_GROUNDED)
            && !!(preview.flags & RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED) == (terrain != RHH_TERRAIN_NONE)
            && !memcmp(&oldTimers, &gFieldTimers, sizeof(oldTimers)) && !gFieldStatuses);
    CHECK(!service->live(1, &view) && !(view.flags & (RHH_BATTLE_GROUNDING_GROUNDED | RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED)));
    CHECK(!service->preview_live(1, RHH_TERRAIN_PSYCHIC, &preview)
        && !(preview.flags & (RHH_BATTLE_GROUNDING_GROUNDED | RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED)));
    gFieldStatuses |= STATUS_FIELD_GRAVITY;
    CHECK(!service->live(1, &view) && (view.flags & RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED));
    gFieldStatuses = 0; gBattleMons[1].item = ITEM_IRON_BALL;
    CHECK(!service->live(1, &view) && (view.flags & RHH_BATTLE_GROUNDING_GROUNDED));
    gFieldStatuses |= STATUS_FIELD_MAGIC_ROOM;
    CHECK(!service->live(1, &view) && !(view.flags & RHH_BATTLE_GROUNDING_GROUNDED));
    gFieldStatuses = 0; gBattleMons[1].item = ITEM_NONE;
    gBattleMons[0].ability = ABILITY_LEVITATE;
    CHECK(!service->live(0, &view) && !(view.flags & RHH_BATTLE_GROUNDING_GROUNDED));
    gBattleMons[0].volatiles.gastroAcid = TRUE;
    CHECK(!service->live(0, &view) && (view.flags & RHH_BATTLE_GROUNDING_GROUNDED));
    gBattleMons[0].volatiles.gastroAcid = FALSE; gBattleMons[0].ability = ABILITY_NONE;
    gBattleMons[0].volatiles.semiInvulnerable = STATE_UNDERGROUND;
    CHECK(!service->live(0, &view) && (view.flags & RHH_BATTLE_GROUNDING_GROUNDED)
        && (view.flags & RHH_BATTLE_GROUNDING_SEMI_INVULNERABLE) && !(view.flags & RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED));
    CHECK(!service->preview_live(0, RHH_TERRAIN_ELECTRIC, &preview)
        && (preview.flags & RHH_BATTLE_GROUNDING_SEMI_INVULNERABLE) && !(preview.flags & RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED));
    gBattleMons[0].volatiles.semiInvulnerable = STATE_NONE; gFieldTimers.terrain = B_TERRAIN_NONE;
    CHECK(!service->live(0, &view) && (view.flags & RHH_BATTLE_GROUNDING_GROUNDED)
        && !(view.flags & RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED));
    saved = view; gAbsentBattlerFlags |= 1;
    CHECK(service->live(0, &view) && !memcmp(&view, &saved, sizeof(view)));
    gAbsentBattlerFlags &= ~1; ai.aiPredictionInProgress = TRUE;
    CHECK(service->live(0, &view) && !memcmp(&view, &saved, sizeof(view)));
    ai.aiPredictionInProgress = FALSE; gBattleTypeFlags |= BATTLE_TYPE_LINK;
    CHECK(service->live(0, &view) && !memcmp(&view, &saved, sizeof(view)));
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK; gFieldTimers.terrain = B_TERRAIN_GRASSY;
    settings[0] = 0; settings[1] = RHH_BATTLE_GROUNDING_GROUNDED | RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED
        | RHH_BATTLE_GROUNDING_AI | RHH_BATTLE_GROUNDING_ABILITY_NONE_OR_UNKNOWN | RHH_BATTLE_GROUNDING_ITEM_NONE_OR_UNKNOWN;
    settings[2] = settings[1]; settings[3] = RHH_TERRAIN_GRASSY;
    gBattleMons[0].ability = ABILITY_LEVITATE;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 0, 100) == 100);
    ai.abilities[0] = ABILITY_LEVITATE;
    settings[2] = RHH_BATTLE_GROUNDING_AI | RHH_BATTLE_GROUNDING_ITEM_NONE_OR_UNKNOWN;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 0, 100) == 100);
    ai.abilities[0] = ABILITY_NONE; ai.holdEffects[0] = HOLD_EFFECT_AIR_BALLOON;
    settings[2] = RHH_BATTLE_GROUNDING_AI | RHH_BATTLE_GROUNDING_ABILITY_NONE_OR_UNKNOWN;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 0, 100) == 100);
    ai.abilities[0] = ABILITY_LEVITATE; ai.holdEffects[0] = HOLD_EFFECT_IRON_BALL;
    settings[2] = RHH_BATTLE_GROUNDING_AI | RHH_BATTLE_GROUNDING_GROUNDED | RHH_BATTLE_GROUNDING_TERRAIN_AFFECTED;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 0, 100) == 100);
    gBattleMons[0].volatiles.semiInvulnerable = STATE_UNDERGROUND;
    settings[1] |= RHH_BATTLE_GROUNDING_SEMI_INVULNERABLE;
    settings[2] = RHH_BATTLE_GROUNDING_AI | RHH_BATTLE_GROUNDING_GROUNDED | RHH_BATTLE_GROUNDING_SEMI_INVULNERABLE;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 0, 100) == 100);
    gBattleMons[0].volatiles.semiInvulnerable = STATE_NONE; settings[1] &= ~RHH_BATTLE_GROUNDING_SEMI_INVULNERABLE;
    ai.abilities[0] = ABILITY_NONE; ai.holdEffects[0] = HOLD_EFFECT_NONE;
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_ZOROARK, 20, 1111, OTID_STRUCT_PRESET(5555), 31);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_PIDGEY, 20, 2222, OTID_STRUCT_PRESET(5555), 31);
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
    gBattleStruct->illusion[0] = (struct Illusion){.state = ILLUSION_ON, .mon = &gParties[B_TRAINER_PLAYER][1]};
    struct BattlePokemon before = gBattleMons[0]; struct Illusion illusion = gBattleStruct->illusion[0];
    settings[2] = RHH_BATTLE_GROUNDING_AI | RHH_BATTLE_GROUNDING_ABILITY_NONE_OR_UNKNOWN | RHH_BATTLE_GROUNDING_ITEM_NONE_OR_UNKNOWN;
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 0, 100) == 100
        && !memcmp(&before, &gBattleMons[0], sizeof(before)) && !memcmp(&illusion, &gBattleStruct->illusion[0], sizeof(illusion)));
    history.abilities[0] = ABILITY_ILLUSION; settings[2] = settings[1];
    CHECK(ModsAiMoveScore(1, 0, MOVE_SCRATCH, 0, 100) == 100
        && !memcmp(&before, &gBattleMons[0], sizeof(before)) && !memcmp(&illusion, &gBattleStruct->illusion[0], sizeof(illusion))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gAiLogicData = NULL;
    gBattleHistory = NULL; gAiPartyData = NULL; RhhModsUnloadLibraries();
    puts("battle grounding: live and AI terrain eligibility passed");
}
