#include "mod_api.h"
#include <stdio.h>
#include <string.h>

static const struct RhhOwnedMonService *sOwned;
static const struct RhhMoveService *sMoves;
static const struct RhhTypeService *sTypes;
static RhhContentKey sPoochyena, sFire, sGrass;
static int sCapEnabled;
static int sCheckingNested, sNestedCalls, sIndirectCalls;
static int sCheckingStats, sStatsCalls;
static struct RhhOwnedMonSnapshot sStatsMon;
static struct RhhOwnedExperience sNestedExperience;
static void NestedMatchup(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (!sCheckingNested) return;
    struct RhhExperienceMovePage page = {.struct_size = sizeof(page), .total = 999}, original = page;
    if (!sOwned->preview_experience_moves(&sNestedExperience, UINT32_MAX, 0, &page)
        || memcmp(&page, &original, sizeof(page))) puts("OWNED_EXPERIENCE_PROBE_BAD");
    ++sIndirectCalls;
}
static void Learnset(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (!sCheckingNested) return;
    struct RhhExperienceMovePage page = {.struct_size = sizeof(page), .total = 999}, original = page;
    if (!sOwned->preview_experience_moves(&sNestedExperience, UINT32_MAX, 0, &page)
        || memcmp(&page, &original, sizeof(page))) puts("OWNED_EXPERIENCE_PROBE_BAD");
    struct RhhTypeMatchup matchup = {.struct_size = sizeof(matchup)};
    if (sTypes->chart(sFire, sGrass, 0, &matchup)) puts("OWNED_EXPERIENCE_PROBE_BAD");
    ++sNestedCalls;
}
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (event->args[0] == 98)
    {
        sStatsMon = (struct RhhOwnedMonSnapshot){.struct_size = sizeof(sStatsMon)};
        if (sOwned->header.struct_size < RHH_OWNED_MON_SERVICE_V1_39_SIZE || sOwned->box_at(0, 0, &sStatsMon))
            puts("OWNED_STATS_PROBE_BAD");
        else sCheckingStats = 1;
        return;
    }
    if (event->args[0] == 99)
    {
        sCapEnabled = 1;
        struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
        sNestedExperience = (struct RhhOwnedExperience){.struct_size = sizeof(sNestedExperience)};
        struct RhhExperienceMovePage page = {.struct_size = sizeof(page)};
        if (sOwned->party_at(0, &mon) || sOwned->experience(&mon, &sNestedExperience))
        { puts("OWNED_EXPERIENCE_PROBE_BAD"); return; }
        sCheckingNested = 1; sNestedCalls = sIndirectCalls = 0;
        int status = sOwned->preview_experience_moves(&sNestedExperience, UINT32_MAX, 0, &page);
        if (status || !sNestedCalls || !sIndirectCalls || page.experience.after_level != 6)
            puts("OWNED_EXPERIENCE_PROBE_BAD");
        sNestedCalls = sIndirectCalls = 0;
        uint32_t count = 0;
        if (sMoves->learnset_count(sPoochyena, RHH_LEARNSET_LEVEL, &count) || !count
            || !sNestedCalls || !sIndirectCalls) puts("OWNED_EXPERIENCE_PROBE_BAD");
        sCheckingNested = 0;
        return;
    }
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
    struct RhhOwnedExperience exp = {.struct_size = sizeof(exp)};
    struct RhhExperienceMovePage page = {.struct_size = sizeof(page)};
    if (sOwned->party_at(0, &mon) || sOwned->experience(&mon, &exp) || mon.level != 5
        || exp.mon.identity != mon.identity || exp.experience != exp.level_start_exp
        || !exp.to_next_level || exp.to_next_level != exp.next_level_exp - exp.experience
        || sOwned->preview_experience_moves(&exp, exp.to_next_level, 0, &page)
        || page.experience.before_level != 5 || page.experience.after_level != 6)
        puts("OWNED_EXPERIENCE_PROBE_BAD");
    else puts("OWNED_EXPERIENCE_PROBE_OK");
}
static void Cap(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; if (sCapEnabled) decision->value = 6; }
static void Stats(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state; (void)decision;
    if (!sCheckingStats) return;
    sCheckingStats = 0;
    struct RhhOwnedStats nested = {.struct_size = sizeof(nested), .current_hp = 999}, before = nested;
    if (!sOwned->stats(&sStatsMon, &nested) || memcmp(&nested, &before, sizeof(nested)))
        puts("OWNED_STATS_PROBE_BAD");
    else if (!sStatsCalls++) puts("OWNED_STATS_PROBE_OK");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_LEVEL_CAP, RHH_TRANSFORM, 0, Cap, NULL},
    {sizeof(struct RhhModHook), RHH_LEARNSET_ENTRY, RHH_TRANSFORM, 0, Learnset, NULL},
    {sizeof(struct RhhModHook), RHH_TYPE_MATCHUP, RHH_CONSTRAINT, 0, NestedMatchup, NULL},
    {sizeof(struct RhhModHook), RHH_BASE_STATS, RHH_TRANSFORM, 0, Stats, NULL}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 28), RHH_SERVICE_REQUIRED, 0, "rhh:service/owned_mon"}
};
static const struct RhhModDefinition definition = {
    .struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES, .id = "test:owned_experience",
    .hook_count = 5, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements
};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    sOwned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 28));
    sMoves = (const void *)host->get_service("rhh:service/moves", RHH_SERVICE_VERSION(1, 0));
    sTypes = (const void *)host->get_service("rhh:service/types", RHH_SERVICE_VERSION(1, 0));
    if (!host->content_key) return NULL;
    sPoochyena = host->content_key("rhh:species/poochyena");
    sFire = host->content_key("rhh:type/fire"); sGrass = host->content_key("rhh:type/grass");
    return sOwned && sMoves && sTypes && sPoochyena && sFire && sGrass
        && sOwned->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_28_SIZE ? &definition : NULL;
}
