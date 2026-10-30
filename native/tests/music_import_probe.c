#include "mod_api.h"
#include <stdio.h>
static const struct RhhMusicImportService *imports;
static const struct RhhMusicService *music;
static void Changed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhMusicImportResult result = {.struct_size = sizeof(result)};
    struct RhhMusicImportResult nested = {.struct_size = sizeof(nested)};
    if (imports->context(&result) || !result.changed || result.status != RHH_MUSIC_IMPORT_OK
        || result.track.key != event->event_key || result.requester != event->subject
        || imports->remove(result.requester, result.track.key, &nested) != RHH_MUSIC_IMPORT_UNAVAILABLE)
        puts("MUSIC_IMPORT_BAD_EVENT");
    else if (event->kind == RHH_MUSIC_IMPORTED)
    {
        struct RhhMusicInstrumentInspection view = {.struct_size = sizeof(view)};
        struct RhhMusicNoteCoverage coverage = {.struct_size = sizeof(coverage)};
        if (music->inspect_instruments(result.track.key, &view) || view.kind != RHH_MUSIC_TRACK_IMPORTED
            || view.availability != RHH_MUSIC_READY || !(view.flags & RHH_MUSIC_INSTRUMENT_MIDI) || !view.total_notes
            || music->inspect_note_coverage(result.track.key, &coverage) || coverage.kind != RHH_MUSIC_TRACK_IMPORTED
            || coverage.availability != RHH_MUSIC_READY || coverage.total_notes != view.total_notes
            || coverage.covered_notes + coverage.uncovered_notes != coverage.total_notes)
            puts("MUSIC_IMPORT_BAD_INSTRUMENTS");
        else puts("MUSIC_IMPORT_ADDED");
    }
    else puts("MUSIC_IMPORT_REMOVED");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MUSIC_IMPORTED, RHH_OBSERVE, 0, NULL, Changed},
    {sizeof(struct RhhModHook), RHH_MUSIC_IMPORT_REMOVED, RHH_OBSERVE, 0, NULL, Changed}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/music_imports"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 8), RHH_SERVICE_REQUIRED, 0, "rhh:service/music"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:music_import_probe", .hook_count = 2, .hooks = hooks,
    .service_requirement_count = 2, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || !host->get_service) return NULL;
    imports = (const void *)host->get_service("rhh:service/music_imports", RHH_SERVICE_VERSION(1, 0));
    music = (const void *)host->get_service("rhh:service/music", RHH_SERVICE_VERSION(1, 8));
    return imports && imports->header.struct_size >= sizeof(*imports)
        && music && music->header.struct_size >= RHH_MUSIC_SERVICE_V1_8_SIZE ? &definition : NULL;
}
