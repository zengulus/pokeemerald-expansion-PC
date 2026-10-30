#include "mod_api.h"
static const struct RhhTypeService *sTypes;
static RhhContentKey sSpecies, sKeys[7];
static void Types(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->event_key != sSpecies) return;
    struct RhhExtendedTypeList list = {.struct_size = sizeof(list), .count = 7};
    for (unsigned i = 0; i < list.count; ++i) list.types[i] = sKeys[i];
    sTypes->offer_species_types_list(&list);
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_SPECIES_TYPES, RHH_TRANSFORM, 0, Types, NULL}
};
static const struct RhhModType sCustom = {
    sizeof(struct RhhModType), 0x86aee0, 0, 0, "test:type/long_crystal", "Luminous Crystal Prism", NULL
};
static const struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_TYPES, .id = "test:type_inspector",
    .hook_count = 1, .hooks = sHooks, .type_count = 1, .types = &sCustom
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->content_key || !host->get_service) return NULL;
    sTypes = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 6));
    if (!sTypes) return NULL;
    sSpecies = host->content_key("rhh:species/ekans");
    const char *names[] = {"rhh:type/fire", "rhh:type/flying", "rhh:type/grass", "rhh:type/poison",
        "rhh:type/steel", "test:type/long_crystal", "rhh:type/water"};
    for (unsigned i = 0; i < 7; ++i) sKeys[i] = host->content_key(names[i]);
    return &sDefinition;
}
