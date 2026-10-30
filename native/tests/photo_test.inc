extern void DrawFrame(u16 *pixels);
static void CheckPhotoCapture(const char *module, bool32 useGl)
{
    SetupCancellationFixture(module);
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhPhotoService *photo = (const void *)host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 0));
    const struct RhhWildActorService *actors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    const struct RhhPhotoVisibilityService *visibility = (const void *)host->get_service("rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 0));
    CHECK(photo && actors && visibility && host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 1))
        && host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 2))
        && host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 3))
        && host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 4))
        && host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 5))
        && !host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 6))
        && photo->header.struct_size >= RHH_PHOTO_SERVICE_V1_5_SIZE);
    struct RhhPhotoRenderContext render = {.struct_size = sizeof(render), .ticket = 999}, savedRender = render;
    CHECK(photo->render_context(&render) && !memcmp(&render, &savedRender, sizeof(render)));
    RhhContentKey variant = 999;
    CHECK(photo->presenter_variant(&variant) && variant == 999);
    CHECK(host->get_service("rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 1))
        && host->get_service("rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 2))
        && host->get_service("rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 3))
        && host->get_service("rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 4))
        && host->get_service("rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 5))
        && host->get_service("rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 6))
        && host->get_service("rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 7))
        && host->get_service("rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 8))
        && !host->get_service("rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 9))
        && visibility->header.struct_size >= RHH_PHOTO_VISIBILITY_SERVICE_V1_8_SIZE);
    struct RhhPhotoExposure geometricExposure = {.struct_size = sizeof(geometricExposure), .ticket = 999}, savedGeometricExposure = geometricExposure;
    CHECK(visibility->geometric_exposure(&geometricExposure) && !memcmp(&geometricExposure, &savedGeometricExposure, sizeof(geometricExposure)));
    struct RhhPhotoDetail detail = {.struct_size = sizeof(detail), .ticket = 999}, savedDetail = detail;
    CHECK(visibility->detail(&detail) && !memcmp(&detail, &savedDetail, sizeof(detail)));
    struct RhhPhotoComposition composition = {.struct_size = sizeof(composition), .ticket = 999}, savedComposition = composition;
    CHECK(visibility->composition(&composition) && !memcmp(&composition, &savedComposition, sizeof(composition)));
    struct RhhPhotoExposure exposure = {.struct_size = sizeof(exposure), .mean_luma = 999}, savedExposure = exposure;
    CHECK(visibility->exposure(&exposure) && !memcmp(&exposure, &savedExposure, sizeof(exposure)));
    struct RhhPhotoFraming framing = {.struct_size = sizeof(framing), .ticket = 999}, savedFraming = framing;
    CHECK(visibility->framing(&framing) && !memcmp(&framing, &savedFraming, sizeof(framing)));
    struct ObjectEvent *wild = &gObjectEvents[1]; memset(wild, 0, sizeof(*wild));
    wild->active = TRUE; wild->trainerType = TRAINER_TYPE_OW_WILD_ENCOUNTER; wild->localId = LOCALID_OW_ENCOUNTER_END;
    wild->mapGroup = MAP_GROUP(MAP_ROUTE103); wild->mapNum = MAP_NUM(MAP_ROUTE103);
    wild->graphicsId = OBJ_EVENT_GFX_SPECIES(POOCHYENA); wild->trainerRange_berryTreeId = 5;
    wild->currentCoords.x = wild->previousCoords.x = MAP_OFFSET + 3;
    wild->currentCoords.y = wild->previousCoords.y = MAP_OFFSET + 1; wild->spriteId = 1; gSprites[1].inUse = TRUE;
    ModsWildActorSpawned(wild, NULL);
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)}; CHECK(!actors->at(1, &actor));
    struct RhhPhotoRequest request = {.struct_size = sizeof(request), .owner = RhhContentKeyOf("test:photo_probe"),
        .actor_handle = actor.handle, .checks = RHH_PERCEPTION_CHECK_COLLISION, .max_distance = 4};
    struct RhhPhotoCapture capture = {.struct_size = sizeof(capture)};
    CHECK(photo->request(&request, &capture) == RHH_PHOTO_UNAVAILABLE);
    RhhModsSetPersistence(AcceptPerceptionJournal); RhhModsSetRunContext(123, 456, 1); ModsWildActorsUpdate();
    struct RhhWildActorIndividual id = {.struct_size = sizeof(id)}; CHECK(!actors->individual(actor.handle, &id));
    wild->offScreen = TRUE; CHECK(photo->request(&request, &capture) == RHH_PHOTO_NOT_VISIBLE); wild->offScreen = FALSE;
    rng_value_t gateRng = gRngValue, gateRng2 = gRng2Value; sPhotoGateImpure = TRUE;
    wild->currentCoords.x = MAP_OFFSET + 2;
    CHECK(photo->request(&request, &capture) == RHH_PHOTO_DENIED && !strcmp(capture.reason, "too close")
        && !memcmp(&gateRng, &gRngValue, sizeof(gateRng)) && !memcmp(&gateRng2, &gRng2Value, sizeof(gateRng2)));
    wild->currentCoords.x = MAP_OFFSET + 3;
    CHECK(photo->request(&request, &capture) == RHH_PHOTO_UNAVAILABLE
        && !memcmp(&gateRng, &gRngValue, sizeof(gateRng)) && !memcmp(&gateRng2, &gRng2Value, sizeof(gateRng2)));
    sPhotoGateImpure = FALSE;
    static u16 pixels[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
    for (u32 i = 0; i < ARRAY_COUNT(pixels); ++i) pixels[i] = RGB(0, 0, 31);
    static u8 rgb[RHH_PHOTO_RGB_BYTES];
    CHECK(photo->context(&capture) && photo->copy_rgb(rgb, sizeof(rgb)));
    rng_value_t rng = gRngValue; sTrackEncounterStarts = TRUE; sEncounterStartCount = 0;
    struct RhhModEvent tick = {.struct_size = sizeof(tick), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision tickDecision = {.struct_size = sizeof(tickDecision), .allowed = 1};
    RhhModsDispatch(&tick, &tickDecision);
    struct RhhPhotoCapture nested = {.struct_size = sizeof(nested)};
    CHECK(photo->request(&request, &nested) == RHH_PHOTO_UNAVAILABLE);
    RhhModsPhotoPresent(pixels, FALSE, TRUE, 11);
    CHECK(photo->context(&capture) && photo->copy_rgb(rgb, sizeof(rgb)));
    RhhModsPhotoPresent(pixels, FALSE, TRUE, 12);
    CHECK(!photo->request(&request, &capture) && capture.ticket == 2);
    wild->currentCoords.x = MAP_OFFSET + 4;
    RhhModsPhotoPresent(pixels, FALSE, TRUE, 13); wild->currentCoords.x = MAP_OFFSET + 3;
    CHECK(!photo->request(&request, &capture) && capture.ticket == 3);
    RhhModsPhotoPresent(pixels, FALSE, FALSE, 14);
    CHECK(!photo->request(&request, &capture) && capture.ticket == 4);
    RhhModsPhotoPresent(pixels, TRUE, TRUE, 15); /* no OpenGL context */
    SDL_Window *window = NULL;
    if (useGl)
    {
        CHECK(!SDL_Init(SDL_INIT_VIDEO)); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        window = SDL_CreateWindow("Photo test", 0, 0, 480, 320, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        CHECK(window && !NativeShaderInit(window));
        CHECK(!photo->request(&request, &capture) && capture.ticket == 5);
        memset(gNativeShaderSceneMask, 255, sizeof(gNativeShaderSceneMask)); gNativeShaderSceneMask[0] = 0;
        static u16 overlay[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
        static u8 preview[RHH_PHOTO_RGB_BYTES], clean[RHH_PHOTO_RGB_BYTES];
        overlay[0] = RGB(0, 31, 0) | 0x8000;
        overlay[ARRAY_COUNT(overlay) - 1] = RGB(0, 0, 31) | 0x8000;
        struct NativeBattleEnvironment env = {0}; NativeShaderPresentWorldOverlay(pixels, &env, 16, overlay);
        CHECK(!NativeShaderCapturePreview(preview) && !NativeShaderCapture(clean));
        CHECK(preview[0] == 0 && preview[1] == 255 && preview[2] == 0
            && preview[3] == 255 && preview[4] == 0 && preview[5] == 0
            && preview[RHH_PHOTO_RGB_BYTES - 3] == 0 && preview[RHH_PHOTO_RGB_BYTES - 2] == 0
            && preview[RHH_PHOTO_RGB_BYTES - 1] == 255
            && clean[0] == 0 && clean[1] == 0 && clean[2] == 255);
        RhhModsPhotoPresent(pixels, TRUE, TRUE, 16);
        /* Keep the context for a later, unmodified 2D-world capture. */
    }
    /* Draw the real software frame: two 8x8 objects overlap by four columns.
     * The sprite list is mapped through BuildOamBuffer and LoadOam. */
    ResetSpriteData();
    for (u32 i = 0; i < 128; ++i) gMain.oamBuffer[i] = gDummyOamData;
    memset(gNativeVram + 0x10000, 0x11, 32);
    ((u16 *)gNativePalette)[0] = RGB(0, 0, 31);
    ((u16 *)gNativePalette)[0x101] = RGB(0, 0, 31);
    ((u16 *)gNativePalette)[0x102] = RGB(31, 31, 31);
    memset(gNativeVram + 0x10000, 0x21, 32); /* alternating blue and white pixels */
    REG_DISPCNT = DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP;
    REG_BLDCNT = 0; REG_IME = 0;
    gSprites[1].inUse = TRUE; gSprites[1].x = 16; gSprites[1].y = 16;
    gSprites[1].oam.priority = 1;
    gSprites[2].inUse = TRUE; gSprites[2].x = 20; gSprites[2].y = 16;
    gSprites[2].oam.priority = 0;
    CHECK(!photo->request_geometric(&request, &capture));
    BuildOamBuffer(); LoadOam(); DrawFrame(pixels);
    RhhModsPhotoPresent(pixels, FALSE, TRUE, 17);
    memset(gNativeVram + 0x10000, 0x11, 32);
    struct RhhPhotoVisibility result = {.struct_size = sizeof(result)};
    CHECK(visibility->context(&result));
    CHECK(!photo->request(&request, &capture));
    BuildOamBuffer(); LoadOam();
    /* Direct OAM writes invalidate the ownership evidence. */
    for (u32 i = 0; i < 128; ++i)
        if (((struct OamData *)OAM)[i].priority == 1 && ((struct OamData *)OAM)[i].x == 16)
            ((struct OamData *)OAM)[i].x = 17;
    DrawFrame(pixels); RhhModsPhotoPresent(pixels, FALSE, TRUE, 18);
    /* A hardware window hides the left half independently of sprite priority. */
    gSprites[2].inUse = FALSE;
    REG_DISPCNT |= DISPCNT_WIN0_ON;
    REG_WIN0H = (16 << 8) | 20; REG_WIN0V = (16 << 8) | 24;
    REG_WININ = 0; REG_WINOUT = 1 << 4;
    CHECK(!photo->request(&request, &capture));
    BuildOamBuffer(); LoadOam(); DrawFrame(pixels); RhhModsPhotoPresent(pixels, FALSE, TRUE, 19);
    REG_WIN0H = (16 << 8) | 24;
    CHECK(!photo->request(&request, &capture));
    BuildOamBuffer(); LoadOam(); DrawFrame(pixels); RhhModsPhotoPresent(pixels, FALSE, TRUE, 20);
    REG_WIN0H = (16 << 8) | 20;
    ((u16 *)gNativePalette)[0x101] = RGB(0, 0, 0);
    CHECK(!photo->request(&request, &capture));
    BuildOamBuffer(); LoadOam(); DrawFrame(pixels); RhhModsPhotoPresent(pixels, FALSE, TRUE, 21);
    ((u16 *)gNativePalette)[0x101] = RGB(31, 31, 31);
    CHECK(!photo->request(&request, &capture));
    BuildOamBuffer(); LoadOam(); DrawFrame(pixels); RhhModsPhotoPresent(pixels, FALSE, TRUE, 22);
    REG_DISPCNT &= ~DISPCNT_WIN0_ON;
    gSprites[1].x = 236;
    CHECK(!photo->request(&request, &capture));
    BuildOamBuffer(); LoadOam(); DrawFrame(pixels); RhhModsPhotoPresent(pixels, FALSE, TRUE, 23);
    if (useGl)
    {
        gSprites[1].x = 16; gSprites[2].inUse = TRUE;
        ((u16 *)gNativePalette)[0x101] = RGB(0, 0, 31);
        CHECK(!photo->request(&request, &capture));
        BuildOamBuffer(); LoadOam(); DrawFrame(pixels);
        struct NativeBattleEnvironment env = {0};
        NativeShaderPresentWorld(pixels, &env, 24);
        struct RhhPhotoRenderContext actual = {.struct_size = sizeof(actual)}, untouched = actual;
        CHECK(!NativeShaderPhotoRenderInfo(capture.ticket, pixels, 24, &actual, NULL));
        CHECK(NativeShaderPhotoRenderInfo(capture.ticket + 1, pixels, 24, &untouched, NULL)
            && untouched.ticket == 0);
        NativeShaderPresent(pixels, &env, 24);
        CHECK(NativeShaderPhotoRenderInfo(capture.ticket, pixels, 24, &untouched, NULL)
            && untouched.ticket == 0);
        NativeShaderPresentWorld(pixels, &env, 24);
        RhhModsPhotoPresent(pixels, TRUE, TRUE, 24);
        CHECK(!photo->request(&request, &capture));
        BuildOamBuffer(); LoadOam(); DrawFrame(pixels);
        env.shader = RhhContentKeyOf("test:photo/effect");
        NativeShaderPresentWorld(pixels, &env, 25);
        RhhModsPhotoPresent(pixels, TRUE, TRUE, 25);
        CHECK(!photo->request(&request, &capture));
        BuildOamBuffer(); LoadOam(); DrawFrame(pixels);
        memset(gNativeShaderSceneMask, 255, sizeof(gNativeShaderSceneMask)); gNativeShaderSceneMask[0] = 0;
        env.shader = 0;
        NativeShaderPresentWorld(pixels, &env, 26);
        RhhModsPhotoPresent(pixels, TRUE, TRUE, 26);
        for (u32 frame = 27; frame <= 29; ++frame)
        {
            CHECK(!photo->request(&request, &capture));
            BuildOamBuffer(); LoadOam(); DrawFrame(pixels);
            memset(gNativeShaderSceneMask, 255, sizeof(gNativeShaderSceneMask)); gNativeShaderSceneMask[0] = 0;
            env.shader = frame == 28 ? RhhContentKeyOf("test:photo/effect") : 0;
            NativeShaderPresentWorld(pixels, &env, frame);
            RhhModsPhotoPresent(pixels, TRUE, TRUE, frame);
        }
        for (u32 frame = 30; frame <= 33; ++frame)
        {
            CHECK(!photo->request(&request, &capture));
            BuildOamBuffer(); LoadOam(); DrawFrame(pixels);
            memset(gNativeShaderSceneMask, 255, sizeof(gNativeShaderSceneMask)); gNativeShaderSceneMask[0] = 0;
            env.shader = RhhContentKeyOf("test:photo/effect_safe");
            NativeShaderPresentWorld(pixels, &env, frame);
            RhhModsPhotoPresent(pixels, TRUE, TRUE, frame);
        }
        CHECK(!photo->request(&request, &capture));
        BuildOamBuffer(); LoadOam(); DrawFrame(pixels);
        NativeShaderPresentWorld(pixels, &env, 40);
        RhhModsPhotoPresent(pixels, TRUE, TRUE, 40);
        NativeShaderShutdown(); SDL_DestroyWindow(window); SDL_Quit();
    }
    gSprites[2].inUse = FALSE;
    static const s16 positions[][2] = {{-4, 16}, {16, -4}, {16, 156}, {236, 156}, {76, 50}, {76, 104}};
    for (u32 i = 0; i < ARRAY_COUNT(positions); ++i)
    {
        gSprites[1].x = positions[i][0]; gSprites[1].y = positions[i][1];
        CHECK(!photo->request(&request, &capture));
        BuildOamBuffer(); LoadOam(); DrawFrame(pixels);
        RhhModsPhotoPresent(pixels, FALSE, TRUE, 34 + i);
    }
    CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !sEncounterStartCount);
    sTrackEncounterStarts = FALSE; free(gBackupMapLayout.map); gBackupMapLayout.map = NULL;
    RhhModsUnloadLibraries(); puts("photo: software, invalidation and composited frame delivery passed");
}
