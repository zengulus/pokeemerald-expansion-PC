static void CheckContentDescriptions(const char *module)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins());
    const struct RhhContentDisplayService *display = (const void *)RhhModsHostAPI()->get_service(
        "rhh:service/content_display", RHH_SERVICE_VERSION(1, 3));
    CHECK(display && display->header.struct_size >= RHH_CONTENT_DISPLAY_SERVICE_V1_3_SIZE
        && RHH_CONTENT_DISPLAY_SERVICE_V1_2_SIZE < RHH_CONTENT_DISPLAY_SERVICE_V1_3_SIZE
        && RhhModsHostAPI()->get_service("rhh:service/content_display", RHH_SERVICE_VERSION(1, 2)) == &display->header
        && !RhhModsHostAPI()->get_service("rhh:service/content_display", RHH_SERVICE_VERSION(1, 4)));
    char text[4096], saved[sizeof(text)]; memset(text, 0x5a, sizeof(text)); memcpy(saved, text, sizeof(text));
    RhhContentKey tackle = RhhContentKeyForId(RHH_MOVE, MOVE_TACKLE);
    CHECK(display->description(RHH_MOVE, tackle, text, sizeof(text)) && !memcmp(text, saved, sizeof(text)));
    CHECK(!RhhModsLoadLibrary(module) && !RhhModsFinalize()); RhhModsSetRunContext(123, 456, 0);
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(!display->description(RHH_MOVE, tackle, text, sizeof(text))
        && !strcmp(text, "Charges the foe with a full-\nbody tackle."));
    size_t length = strlen(text);
    memset(text, 0x5a, sizeof(text)); memcpy(saved, text, sizeof(text));
    CHECK(display->description(RHH_MOVE, tackle, text, length) && !memcmp(text, saved, sizeof(text))
        && !display->description(RHH_MOVE, tackle, text, length + 1));
    CHECK(!display->description(RHH_ABILITY, RhhContentKeyForId(RHH_ABILITY, ABILITY_LEVITATE), text, sizeof(text))
        && !strcmp(text, "Not hit by Ground attacks."));
    CHECK(!display->description(RHH_ITEM, RhhContentKeyForId(RHH_ITEM, ITEM_POTION), text, sizeof(text))
        && !strcmp(text, "Restores the HP of\na Pokemon by\n20 points."));
    memset(text, 0x5a, sizeof(text)); memcpy(saved, text, sizeof(text));
    CHECK(display->description(RHH_SPECIES, RhhContentKeyForId(RHH_SPECIES, SPECIES_TORCHIC), text, sizeof(text))
        && display->description(RHH_MOVE, 0, text, sizeof(text))
        && display->description(RHH_ITEM, tackle, text, sizeof(text))
        && display->description(RHH_MOVE, tackle, text, 0)
        && display->description(RHH_MOVE, tackle, NULL, 1) && !memcmp(text, saved, sizeof(text)));
    u32 moves = 0, abilities = 0, items = 0;
    for (u32 i = 1; i < MOVES_COUNT_ALL; ++i) {
        RhhContentKey key = RhhContentKeyForId(RHH_MOVE, i);
        if (!key || !gMovesInfo[i].description || gMovesInfo[i].effect == EFFECT_PLACEHOLDER) continue;
        if (display->description(RHH_MOVE, key, text, sizeof(text))) { fprintf(stderr, "move description failed: %u\n", i); exit(1); }
        ++moves;
    }
    for (u32 i = 1; i < ABILITIES_COUNT; ++i) {
        RhhContentKey key = RhhContentKeyForId(RHH_ABILITY, i);
        if (!key || !gAbilitiesInfo[i].description) continue;
        if (display->description(RHH_ABILITY, key, text, sizeof(text))) { fprintf(stderr, "ability description failed: %u\n", i); exit(1); }
        ++abilities;
    }
    for (u32 i = 1; i < ITEMS_COUNT; ++i) {
        RhhContentKey key = RhhContentKeyForId(RHH_ITEM, i);
        if (!key || !gItemsInfo[i].description || !gItemsInfo[i].name) continue;
        if (display->description(RHH_ITEM, key, text, sizeof(text))) { fprintf(stderr, "item description failed: %u\n", i); exit(1); }
        ++items;
    }
    CHECK(moves > 500 && abilities > 100 && items > 100
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&event, &decision); RhhModsUnloadLibraries();
    puts("content descriptions: builtins, registered content and bounded copies passed");
}
