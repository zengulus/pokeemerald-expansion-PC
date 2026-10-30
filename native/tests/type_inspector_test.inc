#include "mod_ui.h"
static void TypeInspectorPress(u16 key) { RhhUiAdvance(0); RhhUiAdvance(key); }
static void CheckTypeInspector(const char *inspector, const char *fixture)
{
    RhhModsInit(); CHECK(!ModsRegisterBuiltins() && !RhhModsLoadLibrary(inspector)
        && !RhhModsLoadLibrary(fixture) && !RhhModsFinalize());
    RhhModsSetRunContext(123, 456, 0);
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhTypeService *types = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 6));
    const struct RhhTypeCatalogService *catalog = (const void *)host->get_service("rhh:service/type_catalog", RHH_SERVICE_VERSION(1, 1));
    CHECK(types && catalog);
    for (u32 i = 0; i < host->content_count(RHH_TYPE); ++i)
    {
        struct RhhContentInfo content = {.struct_size = sizeof(content)};
        struct RhhTypeDisplayInfo builtin = {.struct_size = sizeof(builtin)};
        CHECK(!host->content_at(RHH_TYPE, i, &content) && !catalog->describe(content.canonical_key, &builtin)
            && builtin.key == content.canonical_key && builtin.id[0] && builtin.label[0] && !builtin.flags);
    }
    struct RhhExtendedTypeList list = {.struct_size = sizeof(list)};
    struct RhhTypeDisplayInfo info = {.struct_size = sizeof(info)};
    CHECK(!types->species_list(host->content_key("rhh:species/ekans"), &list) && list.count == 7
        && !catalog->describe(list.types[5], &info) && !strcmp(info.label, "Luminous Crystal Prism"));
    memset(gParties[B_TRAINER_PLAYER], 0, sizeof(gParties[B_TRAINER_PLAYER]));
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_EKANS, 15, 12345, OTID_STRUCT_PRESET(54321), 31);
    CreateMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_TORCHIC, 15, 12346, OTID_STRUCT_PRESET(54321), 31);
    gPartiesCount[B_TRAINER_PLAYER] = 2;
    struct Pokemon before[PARTY_SIZE]; memcpy(before, gParties[B_TRAINER_PLAYER], sizeof(before));
    rng_value_t rng = gRngValue, rng2 = gRng2Value;
    CHECK(RhhUiPreparePartyMenu(0) == 1 && !RhhUiOpenPartyMenu());
    TypeInspectorPress(RHH_UI_A); CHECK(RhhUiActive());
    static u16 frame[DISPLAY_WIDTH * DISPLAY_HEIGHT], first[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    RhhUiRender(frame); memcpy(first, frame, sizeof(frame));
    TypeInspectorPress(RHH_UI_DOWN); RhhUiRender(frame); CHECK(memcmp(first, frame, sizeof(frame)));
    TypeInspectorPress(RHH_UI_DOWN); RhhUiRender(frame); CHECK(!memcmp(first, frame, sizeof(frame)));
    TypeInspectorPress(RHH_UI_UP); RhhUiRender(frame); CHECK(memcmp(first, frame, sizeof(frame)));
    TypeInspectorPress(RHH_UI_RIGHT); RhhUiRender(frame); CHECK(memcmp(first, frame, sizeof(frame)));
    TypeInspectorPress(RHH_UI_LEFT); RhhUiRender(frame); CHECK(!memcmp(first, frame, sizeof(frame)));
    TypeInspectorPress(RHH_UI_B); CHECK(!RhhUiActive());
    CHECK(!memcmp(before, gParties[B_TRAINER_PLAYER], sizeof(before)) && !memcmp(&rng, &gRngValue, sizeof(rng))
        && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    RhhModsUnloadLibraries(); puts("type inspector: full labels, paging, party selection and purity passed");
}
