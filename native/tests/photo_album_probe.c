#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhPhotoAlbumService *sAlbum;
static const struct RhhPhotoService *sPhoto;
static const struct RhhPhotoScoreService *sScore;
static void Score(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    struct RhhPhotoScoreContext context = {.struct_size = sizeof(context)};
    static unsigned char pixels[RHH_PHOTO_RGB_BYTES];
    if (sScore->context(&context) || context.capture.ticket != event->ordinal
        || context.score.base != decision->value || !(context.score.flags & RHH_PHOTO_SCORE_ESTIMATED)
        || sPhoto->copy_rgb(pixels, sizeof(pixels))
        || (context.habitat_available && context.habitat.map != context.capture.sight.map)
        || (context.time_available && context.time.run_id != context.capture.sight.target_individual.run_id))
    { puts("ALBUM_PROBE_BAD"); return; }
    decision->value = context.capture.frame == 3 ? RHH_PHOTO_SCORE_MAX + 1 : context.capture.frame == 6 ? 0 : 777;
}
static void ScoreConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhPhotoScoreContext context = {.struct_size = sizeof(context)};
    if (sScore->context(&context)) { puts("ALBUM_PROBE_BAD"); return; }
    decision->value = 5; /* Constraint cannot rewrite the transform. */
    if (context.capture.frame == 2 || context.capture.frame == 5) decision->allowed = 0;
}
static void Captured(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhPhotoScoreContext score = {.struct_size = sizeof(score)};
    if (sScore->context(&score) || score.capture.ticket != event->ordinal
        || (score.capture.frame == 2 || score.capture.frame == 5 ? score.score.flags != 0
            : (!(score.score.flags & RHH_PHOTO_SCORE_AVAILABLE)
                || score.score.value != (score.capture.frame == 3 || score.capture.frame == 10
                    ? score.score.base : score.capture.frame == 6 ? 0 : 777))))
    { puts("ALBUM_PROBE_BAD"); return; }
#ifdef PHOTO_REWARD_SKIP_FRAME4
    if (score.capture.frame == 4 || score.capture.frame == 5) { puts("ALBUM_PROBE_SKIPPED"); return; }
#endif
    struct RhhPhotoAlbumEntry entry = {.struct_size = sizeof(entry)};
    int status = sAlbum->store(event->ordinal, &entry);
    if (status == RHH_ALBUM_FULL) { puts("ALBUM_PROBE_FULL"); return; }
    if (status == RHH_ALBUM_UNAVAILABLE) { puts("ALBUM_PROBE_UNAVAILABLE"); return; }
    static unsigned char original[RHH_PHOTO_RGB_BYTES], stored[RHH_PHOTO_RGB_BYTES];
    if (status || !entry.id.serial || !entry.id.run_id || !entry.individual.serial
        || sPhoto->copy_rgb(original, sizeof(original)) || sAlbum->copy_rgb(&entry.id, stored, sizeof(stored))
        || memcmp(original, stored, sizeof(stored)) || sAlbum->store(event->ordinal, &entry) != RHH_ALBUM_DUPLICATE)
    { puts("ALBUM_PROBE_BAD"); return; }
    puts("ALBUM_PROBE_STORED");
}
static void Changed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhPhotoAlbumEntry entry = {.struct_size = sizeof(entry)}, found = {.struct_size = sizeof(found)};
    if (sAlbum->context(&entry) || entry.id.serial != event->ordinal
        || (event->kind == RHH_PHOTO_STORED && (sAlbum->find(&entry.id, &found)
            || sAlbum->remove(&entry.id) != RHH_ALBUM_UNAVAILABLE))
        || (event->kind == RHH_PHOTO_REMOVED && !sAlbum->find(&entry.id, &found)))
        puts("ALBUM_PROBE_BAD");
    else puts(event->kind == RHH_PHOTO_STORED ? "ALBUM_PROBE_INSERTED" : "ALBUM_PROBE_REMOVED");
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_PHOTO_SCORE, RHH_TRANSFORM, 0, Score, NULL},
    {sizeof(struct RhhModHook), RHH_PHOTO_SCORE, RHH_CONSTRAINT, 0, ScoreConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_PHOTO_CAPTURED, RHH_OBSERVE, 0, NULL, Captured},
    {sizeof(struct RhhModHook), RHH_PHOTO_STORED, RHH_OBSERVE, 0, NULL, Changed},
    {sizeof(struct RhhModHook), RHH_PHOTO_REMOVED, RHH_OBSERVE, 0, NULL, Changed}
};
static const struct RhhServiceRequirement sRequirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/photo_album"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/photo_capture"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 0), RHH_SERVICE_REQUIRED, 0, "rhh:service/photo_score"}
};
static const struct RhhModDefinition sDefinition = {.struct_size = sizeof(sDefinition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:photo_album_probe", .hook_count = sizeof(sHooks) / sizeof(*sHooks), .hooks = sHooks,
    .service_requirement_count = sizeof(sRequirements) / sizeof(*sRequirements), .service_requirements = sRequirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sAlbum = (const void *)host->get_service("rhh:service/photo_album", RHH_SERVICE_VERSION(1, 0));
    sPhoto = (const void *)host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 0));
    sScore = (const void *)host->get_service("rhh:service/photo_score", RHH_SERVICE_VERSION(1, 0));
    return sAlbum && sPhoto && sScore && sAlbum->header.struct_size >= sizeof(*sAlbum) ? &sDefinition : NULL;
}
