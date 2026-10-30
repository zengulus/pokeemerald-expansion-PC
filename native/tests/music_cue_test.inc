/* Exercise deferred battle music requests with real battle and audio state. */
static const struct RhhMusicService *sCueService;
static RhhContentKey sCueOwner, sCueTrack, sCueOverrideTrack;
static u32 sCueRequestEvent, sCueRequests, sCueResultCount;
static int sCueRequestStatus, sCueBusyStatus;
static struct RhhMusicBattleCueResult sLastCueResult;
static void RequestTestBattleCue(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (event->kind != sCueRequestEvent) return;
    struct RhhMusicBattleCueRequest request = {.struct_size = sizeof(request), .requester = sCueOwner, .track = sCueTrack};
    sCueRequestStatus = sCueService->queue_battle_cue(&request);
    if (sCueRequestStatus == RHH_MUSIC_CUE_QUEUED) sCueBusyStatus = sCueService->queue_battle_cue(&request);
    ++sCueRequests; sCueRequestEvent = 0;
}
static void SelectTestBattleCue(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    if (sCueOverrideTrack) decision->value = sCueOverrideTrack;
}
static void ObserveTestBattleCue(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhMusicBattleCueResult value = {.struct_size = sizeof(value)};
    struct RhhMusicBattleCueResult small = {.struct_size = 1, .outcome = 99}, saved = small;
    CHECK(!sCueService->battle_cue_result(&value) && sCueService->battle_cue_result(&small)
        && !memcmp(&small, &saved, sizeof(small)) && event->event_key == value.requester
        && event->subject == value.requested && event->args[0] == value.outcome);
    sLastCueResult = value; ++sCueResultCount;
    (void)Random(); (void)Random2();
}
static const struct RhhModMusic sCueMusic = {sizeof(struct RhhModMusic), 0,
    "test:music/cue_missing", "Missing cue", "missing.mid", "fonts/gen4.sf2"};
static const struct RhhModHook sCueHooks[] = {
    {sizeof(struct RhhModHook), RHH_BATTLE_STARTED, RHH_OBSERVE, 0, NULL, RequestTestBattleCue},
    {sizeof(struct RhhModHook), RHH_BATTLER_ENTERED, RHH_OBSERVE, 0, NULL, RequestTestBattleCue},
    {sizeof(struct RhhModHook), RHH_MUSIC_SELECT, RHH_TRANSFORM, 0, SelectTestBattleCue, NULL},
    {sizeof(struct RhhModHook), RHH_MUSIC_BATTLE_CUE_FINISHED, RHH_OBSERVE, 0, NULL, ObserveTestBattleCue}
};
static const struct RhhModDefinition sCueMod = {.struct_size = sizeof(sCueMod), .abi_version = RHH_MOD_ABI_MUSIC,
    .id = "test:battle_music_cue", .hooks = sCueHooks, .hook_count = ARRAY_COUNT(sCueHooks), .music = &sCueMusic, .music_count = 1};
static void QueueTestBattleSwitch(RhhContentKey track)
{
    sCueTrack = track; sCueRequestEvent = RHH_BATTLER_ENTERED;
    ModsBattleSwitchPrepared(1); ModsBattleSwitchEntered(1);
    CHECK(!sCueRequestEvent && sCueRequestStatus == RHH_MUSIC_CUE_QUEUED && sCueBusyStatus == RHH_MUSIC_CUE_BUSY);
}
static void CheckMusicBattleCue(const char *root)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsRegister(&sCueMod) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    NativeMusicSetAssetRoot(root); cgb_audio_init(42060); m4aSoundInit();
    sCueService = (const void *)RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 6));
    sCueOwner = RhhContentKeyOf("test:battle_music_cue");
    CHECK(sCueService && RHH_MUSIC_SERVICE_V1_5_SIZE < RHH_MUSIC_SERVICE_V1_6_SIZE
        && !RhhModsHostAPI()->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 9)));
    struct RhhMusicBattleCueResult outside = {.struct_size = sizeof(outside), .outcome = 99}, saved = outside;
    struct RhhMusicBattleCueRequest request = {.struct_size = sizeof(request), .requester = sCueOwner};
    CHECK(sCueService->battle_cue_result(&outside) && !memcmp(&outside, &saved, sizeof(outside))
        && sCueService->queue_battle_cue(&request) == RHH_MUSIC_CUE_UNAVAILABLE);
    request.struct_size = 1; CHECK(sCueService->queue_battle_cue(&request) == RHH_MUSIC_CUE_INVALID);
    request.struct_size = sizeof(request); request.track = RhhContentKeyOf("test:unknown_cue");
    CHECK(sCueService->queue_battle_cue(&request) == RHH_MUSIC_CUE_INVALID); request.track = 0;
    struct BattleResources resources = {0}; struct BattleScriptsStack scripts = {0};
    struct AiLogicData ai = {0}; struct AiPartyData parties = {0}; struct BattleHistory history = {0};
    resources.battleScriptsStack = &scripts; gBattleResources = &resources;
    gAiLogicData = &ai; gAiPartyData = &parties; gBattleHistory = &history;
    PrepareTurnBattle(); gBattlersCount = 2; gBattleTypeFlags &= ~BATTLE_TYPE_DOUBLE;
    PlayBGM(MUS_VS_WILD); RhhContentKey original = RhhContentKeyForId(RHH_MUSIC, MUS_VS_WILD);
    sCueTrack = RhhContentKeyForId(RHH_MIDI, 13); sCueRequestEvent = RHH_BATTLE_STARTED;
    sCueRequests = sCueResultCount = 0; sCueOverrideTrack = 0;
    ModsBattleBegin(); CHECK(sCueRequests == 1 && sCueRequestStatus == RHH_MUSIC_CUE_QUEUED && sCueBusyStatus == RHH_MUSIC_CUE_BUSY);
    PlaySE(SE_SELECT); struct SongHeader *se1 = gMPlayInfo_SE1.songHeader, *se2 = gMPlayInfo_SE2.songHeader;
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    NativeMusicFlushBattleCue();
    CHECK(sCueResultCount == 1 && sLastCueResult.outcome == RHH_MUSIC_CUE_PLAYED
        && sLastCueResult.requested == sCueTrack && sLastCueResult.actual == sCueTrack && sLastCueResult.original == original
        && sLastCueResult.after_sequence > sLastCueResult.before_sequence && sCueService->current() == sCueTrack
        && gMPlayInfo_SE1.songHeader == se1 && gMPlayInfo_SE2.songHeader == se2);
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    QueueTestBattleSwitch(RhhContentKeyForId(RHH_MIDI, 14));
    PlayBGM(MUS_VS_TRAINER); NativeMusicFlushBattleCue();
    CHECK(sCueResultCount == 2 && sLastCueResult.outcome == RHH_MUSIC_CUE_STALE
        && !sLastCueResult.actual && !NativeMusicActive());
    QueueTestBattleSwitch(RhhContentKeyOf("test:music/cue_missing")); NativeMusicFlushBattleCue();
    CHECK(sCueResultCount == 3 && sLastCueResult.outcome == RHH_MUSIC_CUE_FALLBACK
        && sLastCueResult.actual == RhhContentKeyForId(RHH_MUSIC, MUS_VS_TRAINER));
    QueueTestBattleSwitch(RhhContentKeyForId(RHH_MIDI, 13)); sCueOverrideTrack = RhhContentKeyForId(RHH_MIDI, 14);
    NativeMusicFlushBattleCue(); sCueOverrideTrack = 0;
    CHECK(sCueResultCount == 4 && sLastCueResult.outcome == RHH_MUSIC_CUE_OVERRIDDEN
        && sLastCueResult.actual == RhhContentKeyForId(RHH_MIDI, 14));
    QueueTestBattleSwitch(0); NativeMusicFlushBattleCue();
    CHECK(sCueResultCount == 5 && sLastCueResult.outcome == RHH_MUSIC_CUE_PLAYED
        && sLastCueResult.actual == RhhContentKeyForId(RHH_MUSIC, MUS_VS_TRAINER));
    QueueTestBattleSwitch(RhhContentKeyForId(RHH_MIDI, 13));
    gMPlayInfo_BGM.status |= MUSICPLAYER_STATUS_PAUSE; NativeMusicFlushBattleCue();
    CHECK(sCueResultCount == 6 && sLastCueResult.outcome == RHH_MUSIC_CUE_STALE); gMPlayInfo_BGM.status &= ~MUSICPLAYER_STATUS_PAUSE;
    gBattleTypeFlags |= BATTLE_TYPE_LINK; sCueTrack = RhhContentKeyForId(RHH_MIDI, 13); sCueRequestEvent = RHH_BATTLER_ENTERED;
    ModsBattleSwitchPrepared(1); ModsBattleSwitchEntered(1);
    CHECK(!sCueRequestEvent && sCueRequestStatus == RHH_MUSIC_CUE_UNAVAILABLE); gBattleTypeFlags &= ~BATTLE_TYPE_LINK;
    QueueTestBattleSwitch(RhhContentKeyForId(RHH_MIDI, 13)); ModsBattleReset(); NativeMusicFlushBattleCue();
    CHECK(sCueResultCount == 7 && sLastCueResult.outcome == RHH_MUSIC_CUE_STALE);
    ModsBattleBegin();
    QueueTestBattleSwitch(RhhContentKeyForId(RHH_MIDI, 13));
    RhhModsSetRunContext(123, 457, 0); NativeMusicFlushBattleCue(); CHECK(sCueResultCount == 7);
    ModsBattleReset(); gMain.inBattle = FALSE; gBattleResources = NULL; gAiLogicData = NULL; gAiPartyData = NULL; gBattleHistory = NULL;
    NativeMusicShutdown(); RhhModsUnloadLibraries();
    puts("battle music cue: deferred selection, conflicts, stale state and fallback passed");
}
