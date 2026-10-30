#include "battle_controllers.h"
#include "constants/trainers.h"
static void CheckBattleLifecycle(const char *path)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins()); CHECK(!RhhModsLoadLibrary(path)); CHECK(!RhhModsFinalize());
    const struct RhhBattleService *service = (const void *)RhhModsHostAPI()->get_service("rhh:service/battle", RHH_SERVICE_VERSION(1, 0));
    CHECK(service && service->header.struct_size >= sizeof(*service));
    struct RhhBattleSnapshot view = {.struct_size = sizeof(view), .turn = 999};
    struct RhhBattleEventContext context = {.struct_size = sizeof(context), .battler = 999};
    struct RhhBattleNature nature = {.struct_size = sizeof(nature), .nature = 999}, savedNature = nature;
    CHECK(service->current(&view) && view.turn == 999 && service->context(&context) && context.battler == 999);
    CHECK(service->nature(0, &nature) && !memcmp(&nature, &savedNature, sizeof(nature)));
    RhhModsSetRunContext(123, 456, 0);
    for (unsigned owner = 0; owner < MAX_BATTLE_TRAINERS; ++owner) {
        CreateMonWithIVs(&gParties[owner][0], owner & 1 ? SPECIES_ZIGZAGOON : SPECIES_TORCHIC,
                         20, 1000 + owner, OTID_STRUCT_PRESET(54321 + owner), 31);
        gPartiesCount[owner] = 1;
    }
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_MUDKIP, 20, 2001, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 2;
    u8 mintNature = NATURE_ADAMANT;
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HIDDEN_NATURE, &mintNature);
    struct BattleStruct battle = {0}; struct BattleResources resources = {0}; struct AiLogicData ai = {0};
    struct AiPartyData aiParties = {0}; aiParties.mons[B_TRAINER_PLAYER][1].wasSentInBattle = TRUE;
    gBattleStruct = &battle; gBattleResources = &resources; gAiLogicData = &ai; gAiPartyData = &aiParties; gMain.inBattle = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER; gBattlersCount = 2; gAbsentBattlerFlags = 0;
    TRAINER_BATTLE_PARAM.opponentA = TRAINER_ROXANNE_1; TRAINER_BATTLE_PARAM.opponentB = 0xffff;
    for (unsigned i = 0; i < MAX_BATTLERS_COUNT; ++i) { gBattlerPositions[i] = i; gBattlerPartyIndexes[i] = 0; }
    PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][0], &gBattleMons[0]);
    PokemonToBattleMon(&gParties[B_TRAINER_OPPONENT_A][0], &gBattleMons[1]);
    rng_value_t rng = gRngValue;
    gBattleOutcome = 0; gBattleTurnCounter = 0; ModsBattleBegin();
    CHECK(!service->current(&view) && view.phase == RHH_BATTLE_PHASE_ACTIVE && view.run_id == 456
        && view.battle_id && view.flags == RHH_BATTLE_TRAINER && view.participant_count == 2
        && view.opponent_trainers[0] == RhhContentKeyForId(RHH_TRAINER, TRAINER_ROXANNE_1));
    u64 firstBattle = view.battle_id;
    CHECK(!service->nature(0, &nature) && nature.battle_id == firstBattle
        && (nature.flags & (RHH_BATTLE_NATURE_SNAPSHOT | RHH_BATTLE_NATURE_STAT_AVAILABLE))
            == (RHH_BATTLE_NATURE_SNAPSHOT | RHH_BATTLE_NATURE_STAT_AVAILABLE)
        && nature.nature == GetNature(&gParties[B_TRAINER_PLAYER][0]) && nature.stat_nature == NATURE_ADAMANT
        && nature.nature_key && nature.stat_nature_key);
    struct RhhModEvent pureEvent = {.struct_size = sizeof(pureEvent), .kind = RHH_TYPE_MATCHUP};
    struct RhhModDecision pureDecision = {.struct_size = sizeof(pureDecision), .allowed = 1};
    RhhModsDispatch(&pureEvent, &pureDecision);
    savedNature = nature; struct RhhBattleNature tinyNature = {.struct_size = 1, .nature = 99}, beforeTinyNature = tinyNature;
    CHECK(service->nature(0, &tinyNature) && !memcmp(&tinyNature, &beforeTinyNature, sizeof(tinyNature))
        && service->nature(MAX_BATTLERS_COUNT, &nature) && !memcmp(&nature, &savedNature, sizeof(nature)));
    struct BattlePokemon savedOpponent = gBattleMons[1];
    gBattleMons[1].personality ^= 1;
    CHECK(!service->nature(1, &nature) && nature.flags == RHH_BATTLE_NATURE_PERSONALITY_FALLBACK
        && nature.nature == GetNatureFromPersonality(gBattleMons[1].personality) && !nature.stat_nature_key);
    gBattleMons[1] = savedOpponent;
    struct Pokemon savedParty = gParties[B_TRAINER_OPPONENT_A][0];
    gBattleMons[1].nativeNatureFlags = 0; gParties[B_TRAINER_OPPONENT_A][0].box.secure.raw[0] ^= 1;
    struct Pokemon corruptParty = gParties[B_TRAINER_OPPONENT_A][0];
    CHECK(!service->nature(1, &nature) && nature.flags == RHH_BATTLE_NATURE_PERSONALITY_FALLBACK
        && !memcmp(&corruptParty, &gParties[B_TRAINER_OPPONENT_A][0], sizeof(corruptParty)));
    gBattleMons[1] = savedOpponent; gParties[B_TRAINER_OPPONENT_A][0] = savedParty;
    struct RhhBattleSnapshot shortView = {.struct_size = sizeof(shortView) - 1, .turn = 999};
    CHECK(service->current(&shortView) && shortView.turn == 999);
    CHECK(view.participants[0].owner == RHH_BATTLE_OWNER_PLAYER && view.participants[0].position == RHH_BATTLE_PLAYER_LEFT
        && view.participants[0].identity == ((u64)54321 << 32 | 1000)
        && view.participants[1].owner == RHH_BATTLE_OWNER_OPPONENT_A
        && view.participants[1].trainer == view.opponent_trainers[0]);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && service->context(&context));
    /* Real switch-data and animation opcodes: redraw-only animations must not emit another entry. */
    struct BattlePokemon next; PokemonToBattleMon(&gParties[B_TRAINER_PLAYER][1], &next);
    memcpy(&resources.bufferB[0][4], &next, BATTLE_POKEMON_TRANSFER_SIZE);
    gBattlerPartyIndexes[0] = 1; gBattleControllerExecFlags = 0; gCurrentMove = MOVE_TACKLE;
    const u8 update[] = {B_SCR_OP_SWITCHINDATAUPDATE, BS_BATTLER_0};
    gBattlescriptCurrInstr = update; gBattleScriptingCommandsTable[B_SCR_OP_SWITCHINDATAUPDATE]();
    view.struct_size = sizeof(view); CHECK(!service->current(&view) && view.participant_count == 1);
    nature = (struct RhhBattleNature){.struct_size = sizeof(nature)}; CHECK(service->nature(0, &nature));
    const u8 animation[] = {B_SCR_OP_SWITCHINANIM, BS_BATTLER_0, 0, 0};
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = animation;
    gBattleScriptingCommandsTable[B_SCR_OP_SWITCHINANIM]();
    view.struct_size = sizeof(view); CHECK(!service->current(&view) && view.participant_count == 2
        && view.participants[0].party_slot == 1 && view.participants[0].identity == ((u64)54321 << 32 | 2001));
    CHECK(!service->nature(0, &nature) && (nature.flags & RHH_BATTLE_NATURE_SNAPSHOT)
        && nature.nature == GetNature(&gParties[B_TRAINER_PLAYER][1]));
    gBattleControllerExecFlags = 0; gBattlescriptCurrInstr = animation;
    gBattleScriptingCommandsTable[B_SCR_OP_SWITCHINANIM]();
    gBattleOutcome = B_OUTCOME_WON; ModsBattleEnd(gBattleOutcome, FALSE);
    view.struct_size = sizeof(view); CHECK(service->current(&view)); ModsBattleEnd(gBattleOutcome, FALSE);
    /* A two-trainer double battle maps each battler to the right party and trainer. */
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI
        | BATTLE_TYPE_INGAME_PARTNER | BATTLE_TYPE_TWO_OPPONENTS;
    TRAINER_BATTLE_PARAM.opponentB = TRAINER_BRAWLY_1; gBattlersCount = 4; gAbsentBattlerFlags = 0;
    gBattleOutcome = 0; gBattleTurnCounter = 0;
    for (unsigned i = 0; i < MAX_BATTLERS_COUNT; ++i) {
        gBattlerPartyIndexes[i] = 0; PokemonToBattleMon(&gParties[i][0], &gBattleMons[i]);
    }
    ModsBattleBegin(); view.struct_size = sizeof(view); CHECK(!service->current(&view));
    CHECK(view.battle_id > firstBattle && view.participant_count == 4
        && (view.flags & (RHH_BATTLE_DOUBLE | RHH_BATTLE_HAS_PARTNER | RHH_BATTLE_TWO_OPPONENTS))
            == (RHH_BATTLE_DOUBLE | RHH_BATTLE_HAS_PARTNER | RHH_BATTLE_TWO_OPPONENTS)
        && view.opponent_trainers[1] == RhhContentKeyForId(RHH_TRAINER, TRAINER_BRAWLY_1));
    for (unsigned i = 0; i < 4; ++i) CHECK(view.participants[i].owner == i && view.participants[i].party_slot == 0);
    CHECK(view.participants[3].trainer == view.opponent_trainers[1]);
    gBattleOutcome = B_OUTCOME_PLAYER_TELEPORTED; ModsBattleEnd(gBattleOutcome, TRUE);
    CHECK(service->current(&view) && service->context(&context));
    /* In a wild double, both battlers on each side share one party. */
    CreateMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][1], SPECIES_ZIGZAGOON, 20, 3001, OTID_STRUCT_PRESET(55555), 31);
    gPartiesCount[B_TRAINER_OPPONENT_A] = 2;
    gBattleTypeFlags = BATTLE_TYPE_DOUBLE; gBattlersCount = 4; gAbsentBattlerFlags = 0;
    gBattleOutcome = 0; gBattleTurnCounter = 0;
    for (unsigned i = 0; i < 4; ++i) {
        gBattlerPartyIndexes[i] = i / 2;
        PokemonToBattleMon(&gParties[i & 1][i / 2], &gBattleMons[i]);
    }
    ModsBattleBegin(); view.struct_size = sizeof(view); CHECK(!service->current(&view));
    CHECK(view.participant_count == 4 && view.flags == RHH_BATTLE_DOUBLE
        && !view.opponent_trainers[0] && !view.opponent_trainers[1]);
    for (unsigned i = 0; i < 4; ++i)
        CHECK(view.participants[i].owner == (i & 1) && view.participants[i].party_slot == i / 2);
    gBattleOutcome = B_OUTCOME_CAUGHT; ModsBattleEnd(gBattleOutcome, FALSE);
    /* Facility trainer ordinals are unrelated to the static trainer catalog. */
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_BATTLE_TOWER; gBattlersCount = 2;
    gBattlerPartyIndexes[0] = gBattlerPartyIndexes[1] = 0;
    gBattleOutcome = 0; gBattleTurnCounter = 0; ModsBattleBegin();
    view.struct_size = sizeof(view); CHECK(!service->current(&view)
        && (view.flags & RHH_BATTLE_FACILITY) && !view.opponent_trainers[0] && !view.participants[1].trainer);
    gBattleOutcome = B_OUTCOME_LOST; ModsBattleEnd(gBattleOutcome, FALSE);
    /* Link controllers determine local ownership; remote IDs are not static trainers. */
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_LINK;
    gBattlerBattleController[0] = BATTLE_CONTROLLER_PLAYER;
    gBattlerBattleController[1] = BATTLE_CONTROLLER_LINK_OPPONENT;
    gBattleOutcome = 0; ModsBattleBegin(); view.struct_size = sizeof(view);
    CHECK(!service->current(&view) && (view.flags & RHH_BATTLE_LINK) && !view.opponent_trainers[0]
        && view.participants[0].owner == RHH_BATTLE_OWNER_PLAYER
        && view.participants[1].owner == RHH_BATTLE_OWNER_OPPONENT_A);
    nature = (struct RhhBattleNature){.struct_size = sizeof(nature), .nature = 99}; savedNature = nature;
    CHECK(service->nature(0, &nature) && !memcmp(&nature, &savedNature, sizeof(nature)));
    gBattleOutcome = B_OUTCOME_DREW | B_OUTCOME_LINK_BATTLE_RAN; ModsBattleEnd(gBattleOutcome, FALSE);
    RhhModsSetRunContext(0, 0, 0); ModsBattleBegin(); CHECK(service->current(&view));
    gMain.inBattle = FALSE; gBattleResources = NULL; gBattleStruct = NULL; gAiLogicData = NULL; gAiPartyData = NULL; RhhModsUnloadLibraries();
    puts("battle lifecycle: trainer rosters, actual switch opcodes, double parties and outcomes passed");
}
