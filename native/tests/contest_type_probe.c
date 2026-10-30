#include "mod_api.h"

static const struct RhhTypeService *sTypes;
static RhhContentKey sTorchic, sGengar, sFire, sWater, sGrass, sGhost;

static void Propose(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    struct RhhExtendedTypeList types = {.struct_size = sizeof(types), .count = 4,
                                        .types = {sFire, sWater, sGrass, sGhost}};
    if (event->subject == sGengar)
    { types.count = 3; types.types[3] = 0; }
    else if (event->subject != sTorchic) return;
    sTypes->offer_species_types_list(&types);
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_SPECIES_TYPES, RHH_TRANSFORM, 0, Propose, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 6), RHH_SERVICE_REQUIRED, 0, "rhh:service/types"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:contest_types",
    .hook_count = 1, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service || !host->content_key) return NULL;
    sTypes = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 6));
    sTorchic = host->content_key("rhh:species/torchic"); sGengar = host->content_key("rhh:species/gengar");
    sFire = host->content_key("rhh:type/fire"); sWater = host->content_key("rhh:type/water");
    sGrass = host->content_key("rhh:type/grass"); sGhost = host->content_key("rhh:type/ghost");
    return sTypes && sTorchic && sGengar && sFire && sWater && sGrass && sGhost ? &definition : NULL;
}
