#include "mod_api.h"
#include <stdio.h>

#ifndef TEST_INDEX
#error TEST_INDEX must identify the test module
#endif

static char sId[32];
static struct RhhModDefinition sDefinition = {
    .struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_V1,
    .state_schema = 1, .state_size = 1
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host) return NULL;
    snprintf(sId, sizeof(sId), "test:loader%03u", (unsigned)TEST_INDEX);
    sDefinition.id = sId;
    return &sDefinition;
}
