#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhPhotoService *sPhoto;
static const struct RhhPhotoVisibilityService *sVisibility;
static const struct RhhPhotoScoreService *sScore;
static const struct RhhWildActorService *sActors;
static const struct RhhWorldRenderService *sRender;
static const struct RhhModHostV1 *sHost;
#ifdef RHH_TEST_PHOTO_ARCHIVE
static const struct RhhModArchiveService *sArchive;
static RhhContentKey sArchiveOwner, sArchiveKey;
static int ArchiveWrite(const void *bytes, uint32_t size, struct RhhModArchiveRecord *out)
{
    struct RhhModArchiveRecord existing = {.struct_size = sizeof(existing)};
    uint64_t revision = sArchive->find(sArchiveOwner, sArchiveKey, &existing) ? 0 : existing.revision;
    return sArchive->put(sArchiveOwner, sArchiveKey, revision, bytes, size, out);
}
#endif
static int sUsePresenter = 1;
static int sPresenterPhotoMask;
static unsigned sDepthTexture;
static int CheckRender(const struct RhhPhotoCapture *capture)
{
    if (capture->frame == 17)
    {
        uint32_t mode = 99, presented = 99;
        if (sPhoto->request_mode(&mode) || mode != RHH_PHOTO_REQUEST_ORDINARY_GEOMETRIC
            || sPhoto->requires_presenter(&presented) || presented) return -1;
        struct RhhPhotoExposure e = {.struct_size = sizeof(e), .ticket = 999}, before = e;
        if (!sVisibility->geometric_exposure(&e) || memcmp(&e, &before, sizeof(e))) return -1;
    }
    struct RhhPhotoRenderContext render = {.struct_size = sizeof(render)};
    struct RhhPhotoRenderContext tiny = {.struct_size = 1, .ticket = 999}, before = tiny;
    if (sPhoto->render_context(&render) || render.ticket != capture->ticket || render.frame != capture->frame
        || render.backend != capture->backend || !sPhoto->render_context(&tiny) || memcmp(&tiny, &before, sizeof(tiny))) return -1;
    RhhContentKey variant = 999;
    if (sPhoto->presenter_variant(&variant) || variant != (render.presenter ? sHost->content_key("test:photo/variant") : 0)) return -1;
    if (capture->backend == RHH_PHOTO_BACKEND_SOFTWARE)
        return render.presenter || render.environment_shader || render.flags || render.pass_count ? -1 : 0;
    unsigned frame = capture->frame;
    RhhContentKey presenter = sHost->content_key("test:photo/world");
    int expectedPresenter = frame == 16 || (frame >= 26 && frame <= 29) || frame == 31 || frame == 32 || frame == 33 || frame == 40;
    RhhContentKey effect = frame == 25 || frame == 28 ? sHost->content_key("test:photo/effect")
        : (frame >= 30 && frame <= 33) || frame == 40 ? sHost->content_key("test:photo/effect_safe") : 0;
    if (render.presenter != (expectedPresenter ? presenter : 0) || render.environment_shader != effect
        || render.flags != (frame == 31 ? RHH_PHOTO_RENDER_DEPTH_TEXTURE : 0)
        || render.pass_count != (frame == 32 || frame == 33 ? 1u : 0u)) return -1;
    if (render.pass_count && (render.passes[0].id != sHost->content_key("test:photo/pass")
        || render.passes[0].shader != sHost->content_key(frame == 32 ? "test:photo/effect_safe" : "test:photo/effect"))) return -1;
    return 0;
}
static int CheckDepth(unsigned frame, uint64_t ticket)
{
    struct RhhPhotoDepth depth = {.struct_size = sizeof(depth)}, before = depth;
    static uint16_t samples[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
    samples[0] = 12345;
    if (frame != 31)
        return !sVisibility->depth(&depth) || memcmp(&depth, &before, sizeof(depth))
            || !sVisibility->copy_depth(samples, RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT) || samples[0] != 12345 ? -1 : 0;
    struct RhhPhotoDepth tiny = {.struct_size = 1, .ticket = 999}, unchanged = tiny;
    if (sVisibility->depth(&depth) || depth.ticket != ticket || depth.mask_quality != RHH_PHOTO_MASK_EXACT
        || depth.visible_pixels != 32 || depth.min_depth != 10000 || depth.max_depth != 50000
        || depth.depth_sum != 16u * 10000u + 16u * 50000u
        || !sVisibility->depth(&tiny) || memcmp(&tiny, &unchanged, sizeof(tiny))
        || !sVisibility->copy_depth(samples, RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT - 1) || samples[0] != 12345
        || sVisibility->copy_depth(samples, RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT)) return -1;
    for (unsigned y = 0; y < RHH_PHOTO_HEIGHT; ++y)
        for (unsigned x = 0; x < RHH_PHOTO_WIDTH; ++x)
        {
            unsigned expected = y >= 16 && y < 24 && x >= 16 && x < 20 ? (y < 20 ? 10000 : 50000) : 0;
            if (samples[y * RHH_PHOTO_WIDTH + x] != expected) return -1;
        }
    return 0;
}
static int HasVisibility(unsigned frame)
{ return frame == 17 || (frame >= 19 && frame <= 24) || frame == 27 || (frame >= 30 && frame <= 32); }
static int LeftVisible(unsigned frame)
{ return frame == 17 || frame == 24 || frame == 27 || (frame >= 30 && frame <= 32); }
static int CheckComposition(unsigned frame, uint64_t ticket)
{
    struct RhhPhotoComposition out = {.struct_size = sizeof(out)};
    struct RhhPhotoComposition tiny = {.struct_size = 1, .ticket = 999}, before = tiny;
    if (sVisibility->composition(&out) || out.ticket != ticket
        || !sVisibility->composition(&tiny) || memcmp(&tiny, &before, sizeof(tiny))) return -1;
    struct RhhPhotoDistribution silhouette = {0}, visible = {0};
    if (frame == 38 || frame == 39)
    {
        silhouette.pixels = 64; silhouette.x2_sum = 10240; silhouette.y2_sum = frame == 38 ? 6912 : 13824;
        if (frame == 38) silhouette.cells[0] = silhouette.cells[1] = silhouette.cells[3] = silhouette.cells[4] = 16;
        else { silhouette.cells[3] = silhouette.cells[4] = 12; silhouette.cells[6] = silhouette.cells[7] = 20; }
        visible = silhouette;
    }
    else if (frame == 23)
    {
        silhouette.pixels = visible.pixels = 32;
        silhouette.x2_sum = visible.x2_sum = 15232; silhouette.y2_sum = visible.y2_sum = 1280;
        silhouette.cells[2] = visible.cells[2] = 32;
    }
    else
    {
        silhouette.pixels = 64; silhouette.x2_sum = silhouette.y2_sum = 2560; silhouette.cells[0] = 64;
        if (frame != 20)
        { visible.pixels = 32; visible.x2_sum = 1152; visible.y2_sum = 1280; visible.cells[0] = 32; }
    }
    return memcmp(&out.silhouette, &silhouette, sizeof(silhouette)) || memcmp(&out.visible, &visible, sizeof(visible)) ? -1 : 0;
}
static int CheckExtraFootprint(unsigned frame, uint64_t ticket)
{
    struct RhhPhotoVisibility visibility = {.struct_size = sizeof(visibility)};
    struct RhhPhotoFootprint footprint = {.struct_size = sizeof(footprint)};
    if (sVisibility->context(&visibility)) return -1;
    if (frame == 40)
    {
        struct RhhPhotoFootprint before = footprint;
        return !sVisibility->footprint(&footprint) || memcmp(&footprint, &before, sizeof(footprint)) ? -1 : 0;
    }
    static const int bounds[][4] = {{-4, 16, 4, 24}, {16, -4, 24, 4},
                                    {16, 156, 24, 164}, {236, 156, 244, 164},
                                    {76, 50, 84, 58}, {76, 104, 84, 112}};
    const int *b = bounds[frame - 34];
    unsigned inside = frame >= 38 ? 64 : frame == 37 ? 16 : 32;
    return sVisibility->footprint(&footprint) || footprint.ticket != ticket
        || visibility.total_pixels != inside || footprint.total_pixels != 64 || footprint.inside_pixels != inside
        || footprint.left != b[0] || footprint.top != b[1] || footprint.right != b[2] || footprint.bottom != b[3]
        || (frame >= 38 && CheckComposition(frame, ticket)) ? -1 : 0;
}
static void Score(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
#ifdef RHH_TEST_PHOTO_ARCHIVE
    struct RhhModArchiveRecord denied = {.struct_size = sizeof(denied)}, beforeDenied = denied;
    if (!ArchiveWrite("x", 1, &denied) || memcmp(&denied, &beforeDenied, sizeof(denied)))
    { puts("PHOTO_PROBE_BAD_ARCHIVE_PREVIEW"); return; }
    puts("PHOTO_PROBE_ARCHIVE_PREVIEW_BLOCKED");
#endif
    struct RhhPhotoScoreContext score = {.struct_size = sizeof(score)};
    struct RhhPhotoVisibility visibility = {.struct_size = sizeof(visibility)};
    struct RhhPhotoExposure exposure = {.struct_size = sizeof(exposure)};
    struct RhhPhotoDetail detail = {.struct_size = sizeof(detail)};
    if (sScore->context(&score) || CheckRender(&score.capture)) { puts("PHOTO_PROBE_BAD_RENDER"); return; }
    if (CheckDepth(score.capture.frame, score.capture.ticket)) { puts("PHOTO_PROBE_BAD_DEPTH"); return; }
    if (score.capture.frame == 17 && CheckComposition(17, score.capture.ticket))
    { puts("PHOTO_PROBE_BAD_COMPOSITION"); return; }
    if ((score.capture.frame >= 34 && score.capture.frame <= 39) || score.capture.frame == 40)
    { if (CheckExtraFootprint(score.capture.frame, score.capture.ticket)) puts("PHOTO_PROBE_BAD_EXTRA_FOOTPRINT"); return; }
    if (HasVisibility(score.capture.frame))
    {
        if (sVisibility->context(&visibility) || sVisibility->exposure(&exposure) || sVisibility->detail(&detail)
            || exposure.ticket != score.capture.ticket || exposure.visible_pixels != visibility.visible_pixels
            || detail.ticket != score.capture.ticket
            || visibility.total_pixels != (score.capture.frame == 23 ? 32u : 64u)
            || visibility.visible_pixels != (score.capture.frame == 20 ? 0 : 32))
        { puts("PHOTO_PROBE_BAD"); return; }
        decision->value = decision->value * visibility.visible_pixels / visibility.total_pixels;
    }
    else if (!sVisibility->context(&visibility) || !sVisibility->exposure(&exposure) || !sVisibility->detail(&detail)) puts("PHOTO_PROBE_BAD");
}
static void Gate(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)event; (void)state;
    struct RhhPhotoCapture context = {.struct_size = sizeof(context)};
    static unsigned char pixels[RHH_PHOTO_RGB_BYTES];
    if (sPhoto->context(&context) || !sPhoto->copy_rgb(pixels, sizeof(pixels))) { puts("PHOTO_PROBE_BAD"); return; }
    if (context.sight.distance == 1)
    { decision->allowed = 0; strcpy(decision->reason, "too close"); puts("PHOTO_PROBE_DENIED"); }
}
static void Captured(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    static unsigned char pixels[RHH_PHOTO_RGB_BYTES + 1];
    struct RhhPhotoCapture c = {.struct_size = sizeof(c)};
    if (sPhoto->context(&c) || c.ticket != event->ordinal || c.status || c.width != 240 || c.height != 160
        || c.stride != 720 || c.sight.target_individual.serial == 0
        || !sPhoto->copy_rgb(pixels, RHH_PHOTO_RGB_BYTES - 1)
        || sPhoto->copy_rgb(pixels, RHH_PHOTO_RGB_BYTES)) { puts("PHOTO_PROBE_BAD"); return; }
#ifdef RHH_TEST_PHOTO_ARCHIVE
    static unsigned char restored[RHH_PHOTO_RGB_BYTES];
    struct RhhModArchiveRecord record = {.struct_size = sizeof(record)};
    if (ArchiveWrite(pixels, RHH_PHOTO_RGB_BYTES, &record) || record.size != RHH_PHOTO_RGB_BYTES
        || sArchive->copy(sArchiveOwner, sArchiveKey, record.revision, restored, sizeof(restored))
        || memcmp(restored, pixels, sizeof(restored)))
    { puts("PHOTO_PROBE_BAD_ARCHIVE_CAPTURE"); return; }
    puts("PHOTO_PROBE_ARCHIVE_OK");
#endif
    if (CheckRender(&c)) { puts("PHOTO_PROBE_BAD_RENDER"); return; }
    if (CheckDepth(c.frame, c.ticket)) { puts("PHOTO_PROBE_BAD_DEPTH"); return; }
    if (c.frame == 31) puts("PHOTO_PROBE_DEPTH");
    if ((c.frame >= 34 && c.frame <= 39) || c.frame == 40)
    {
        if (CheckExtraFootprint(c.frame, c.ticket)) puts("PHOTO_PROBE_BAD_EXTRA_FOOTPRINT");
        else printf("PHOTO_PROBE_EXTRA_FOOTPRINT %u\n", c.frame);
        return;
    }
    if (pixels[0] || pixels[1] || pixels[2] != 255
        || (c.backend == RHH_PHOTO_BACKEND_SOFTWARE && (pixels[3] || pixels[4] || pixels[5] != 255))
        || (c.backend == RHH_PHOTO_BACKEND_OPENGL && (c.frame == 16 || (c.frame >= 26 && c.frame <= 29)) && (pixels[3] != 255 || pixels[4] || pixels[5]))
        || (c.backend == RHH_PHOTO_BACKEND_OPENGL && c.frame == 30 && (pixels[3] || pixels[4] || pixels[5] < 127 || pixels[5] > 128))
        || (c.backend == RHH_PHOTO_BACKEND_OPENGL && c.frame == 31 && (pixels[3] < 127 || pixels[3] > 128 || pixels[4] || pixels[5]))
        || (c.backend == RHH_PHOTO_BACKEND_OPENGL && c.frame == 32 && (pixels[3] < 63 || pixels[3] > 64 || pixels[4] || pixels[5]))
        || (c.backend == RHH_PHOTO_BACKEND_OPENGL && c.frame == 24 && (pixels[3] || pixels[4] || pixels[5] != 255)))
    { puts("PHOTO_PROBE_BAD"); return; }
    struct RhhPhotoVisibility visibility = {.struct_size = sizeof(visibility)};
    struct RhhPhotoExposure exposure = {.struct_size = sizeof(exposure)};
    struct RhhPhotoDetail detail = {.struct_size = sizeof(detail)};
    struct RhhPhotoFraming framing = {.struct_size = sizeof(framing)};
    struct RhhPhotoComposition composition = {.struct_size = sizeof(composition)};
    static unsigned char mask[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
    static unsigned char silhouette[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
    if (HasVisibility(c.frame))
    {
        struct RhhPhotoScoreContext score = {.struct_size = sizeof(score)};
        if (sVisibility->context(&visibility) || sVisibility->exposure(&exposure) || sVisibility->detail(&detail)
            || exposure.ticket != c.ticket || detail.ticket != c.ticket
            || visibility.ticket != c.ticket
            || visibility.total_pixels != (c.frame == 23 ? 32u : 64u) || visibility.visible_pixels != (c.frame == 20 ? 0u : 32u)
            || sScore->context(&score) || score.score.value != (c.frame == 20 ? 0 : c.frame == 23 ? score.score.base : score.score.base / 2)
            || !sVisibility->copy_mask(mask, sizeof(mask) - 1) || sVisibility->copy_mask(mask, sizeof(mask))
            || !sVisibility->copy_silhouette(silhouette, sizeof(silhouette) - 1)
            || sVisibility->copy_silhouette(silhouette, sizeof(silhouette)) || sVisibility->framing(&framing)
            || framing.ticket != c.ticket)
        { puts("PHOTO_PROBE_BAD"); return; }
        for (unsigned y = 0; y < RHH_PHOTO_HEIGHT; ++y)
            for (unsigned x = 0; x < RHH_PHOTO_WIDTH; ++x)
                if (mask[y * RHH_PHOTO_WIDTH + x] != (c.frame != 20 && y >= 16 && y < 24
                    && x >= (c.frame == 23 ? 236u : LeftVisible(c.frame) ? 16u : 20u)
                    && x < (c.frame == 23 ? 240u : LeftVisible(c.frame) ? 20u : 24u))
                    || silhouette[y * RHH_PHOTO_WIDTH + x] != (y >= 16 && y < 24
                    && x >= (c.frame == 23 ? 236u : 16u) && x < (c.frame == 23 ? 240u : 24u)))
                { puts("PHOTO_PROBE_BAD"); return; }
        struct RhhPhotoBounds expectedSilhouette = {c.frame == 23 ? 236u : 16u, 16, c.frame == 23 ? 240u : 24u, 24};
        struct RhhPhotoBounds expectedVisible = c.frame == 20 ? (struct RhhPhotoBounds){0}
            : (struct RhhPhotoBounds){c.frame == 23 ? 236u : LeftVisible(c.frame) ? 16u : 20u, 16,
                c.frame == 23 ? 240u : LeftVisible(c.frame) ? 20u : 24u, 24};
        if (memcmp(&framing.silhouette, &expectedSilhouette, sizeof(expectedSilhouette))
            || memcmp(&framing.visible, &expectedVisible, sizeof(expectedVisible)))
        { puts("PHOTO_PROBE_BAD"); return; }
        if (c.frame == 17 || c.frame == 20 || c.frame == 23 || c.frame == 27)
        {
            if (CheckComposition(c.frame, c.ticket)) { puts("PHOTO_PROBE_BAD_COMPOSITION"); return; }
            puts("PHOTO_PROBE_COMPOSITION");
        }
        if (c.frame == 17 || c.frame == 23 || c.frame == 27 || c.frame == 31)
        {
            struct RhhPhotoFootprint footprint = {.struct_size = sizeof(footprint)};
            struct RhhPhotoFootprint small = {.struct_size = 1, .ticket = 999}, unchanged = small;
            if (sVisibility->footprint(&footprint) || footprint.ticket != c.ticket
                || footprint.total_pixels != 64 || footprint.inside_pixels != (c.frame == 23 ? 32u : 64u)
                || footprint.left != (c.frame == 23 ? 236 : 16) || footprint.top != 16
                || footprint.right != (c.frame == 23 ? 244 : 24) || footprint.bottom != 24
                || !sVisibility->footprint(&small) || memcmp(&small, &unchanged, sizeof(small)))
            { puts("PHOTO_PROBE_BAD_FOOTPRINT"); return; }
            puts("PHOTO_PROBE_FOOTPRINT");
        }
        unsigned count = 0, sum = 0, minimum = 0, maximum = 0, dark = 0, bright = 0;
        for (unsigned i = 0; i < RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT; ++i)
        {
            if (!mask[i]) continue;
            unsigned luma = (54u * pixels[3 * i] + 183u * pixels[3 * i + 1] + 19u * pixels[3 * i + 2]) / 256u;
            if (!count || luma < minimum) minimum = luma;
            if (luma > maximum) maximum = luma;
            ++count; sum += luma; dark += luma <= 16; bright += luma >= 240;
        }
        if (exposure.visible_pixels != count || exposure.mean_luma != (count ? sum / count : 0)
            || exposure.min_luma != minimum || exposure.max_luma != maximum
            || exposure.dark_pixels != dark || exposure.bright_pixels != bright
            || (c.frame == 21 && (exposure.dark_pixels != 32 || exposure.mean_luma != 0))
            || (c.frame == 22 && (exposure.bright_pixels != 32 || exposure.mean_luma != 255)))
        { puts("PHOTO_PROBE_BAD"); return; }
        unsigned pairs = 0, deltas = 0, strong = 0, maxDelta = 0;
        for (unsigned y = 0; y < RHH_PHOTO_HEIGHT; ++y)
            for (unsigned x = 0; x < RHH_PHOTO_WIDTH; ++x)
            {
                unsigned i = y * RHH_PHOTO_WIDTH + x;
                if (!mask[i]) continue;
                for (unsigned d = 0; d < 2; ++d)
                {
                    if ((!d && x + 1 == RHH_PHOTO_WIDTH) || (d && y + 1 == RHH_PHOTO_HEIGHT)) continue;
                    unsigned j = i + (d ? RHH_PHOTO_WIDTH : 1);
                    if (!mask[j]) continue;
                    unsigned a = (54u * pixels[3*i] + 183u * pixels[3*i+1] + 19u * pixels[3*i+2]) / 256u;
                    unsigned b = (54u * pixels[3*j] + 183u * pixels[3*j+1] + 19u * pixels[3*j+2]) / 256u;
                    unsigned delta = a > b ? a - b : b - a;
                    ++pairs; deltas += delta; strong += delta >= 32;
                    if (delta > maxDelta) maxDelta = delta;
                }
            }
        if (detail.neighbor_pairs != pairs || detail.luma_delta_sum != deltas || detail.high_contrast_pairs != strong
            || detail.max_luma_delta != maxDelta || (c.frame == 17 && (pairs != 52 || deltas != 24 * 237 || strong != 24 || maxDelta != 237)))
        { puts("PHOTO_PROBE_BAD_DETAIL"); return; }
        struct RhhPhotoDetail tinyDetail = {.struct_size = 1, .ticket = 999}, beforeDetail = tinyDetail;
        if (!sVisibility->detail(&tinyDetail) || memcmp(&tinyDetail, &beforeDetail, sizeof(tinyDetail)))
        { puts("PHOTO_PROBE_BAD_DETAIL"); return; }
        if (c.frame == 17) puts("PHOTO_PROBE_DETAIL");
        struct RhhPhotoExposure tiny = {.struct_size = 1, .mean_luma = 999}, before = tiny;
        if (!sVisibility->exposure(&tiny) || memcmp(&tiny, &before, sizeof(tiny)))
        { puts("PHOTO_PROBE_BAD"); return; }
        struct RhhPhotoFraming small = {.struct_size = 1, .ticket = 999}, unchanged = small;
        if (!sVisibility->framing(&small) || memcmp(&small, &unchanged, sizeof(small)))
        { puts("PHOTO_PROBE_BAD"); return; }
        puts("PHOTO_PROBE_VISIBILITY");
        if (c.frame == 24) puts("PHOTO_PROBE_GL_VISIBILITY");
        if (c.frame == 27) puts("PHOTO_PROBE_GL_PRESENTER_MASK");
        if (c.frame == 30) puts("PHOTO_PROBE_GL_SAFE_EFFECT");
        if (c.frame == 31) puts("PHOTO_PROBE_GL_SAFE_EFFECT_PRESENTER");
        if (c.frame == 32) puts("PHOTO_PROBE_GL_SAFE_PASS");
    }
    else if (!sVisibility->context(&visibility) || !sVisibility->copy_mask(mask, sizeof(mask))
             || !sVisibility->exposure(&exposure) || !sVisibility->copy_silhouette(silhouette, sizeof(silhouette))
             || !sVisibility->framing(&framing) || !sVisibility->composition(&composition) || !sVisibility->detail(&detail))
    { puts("PHOTO_PROBE_BAD"); return; }
    struct RhhPhotoRequest request = {.struct_size = sizeof(request), .owner = c.owner,
        .actor_handle = c.sight.target_wild.handle, .max_distance = 4};
    struct RhhPhotoCapture nested = {.struct_size = sizeof(nested)};
    if (sPhoto->request(&request, &nested) != RHH_PHOTO_UNAVAILABLE) { puts("PHOTO_PROBE_BAD"); return; }
    printf("PHOTO_PROBE_CAPTURED %u\n", c.backend);
    if (c.frame == 16) sUsePresenter = 0;
    if (c.frame == 25) { sUsePresenter = 1; puts("PHOTO_PROBE_GL_EFFECT_UNAVAILABLE"); }
    if (c.frame == 26) { sPresenterPhotoMask = 1; puts("PHOTO_PROBE_GL_PRESENTER_UNAVAILABLE"); }
    if (c.frame == 28) { sPresenterPhotoMask = 2; puts("PHOTO_PROBE_GL_PRESENTER_EFFECT_UNAVAILABLE"); }
    if (c.frame == 29) { sPresenterPhotoMask = 1; sUsePresenter = 0; puts("PHOTO_PROBE_GL_PRESENTER_INVALID"); }
    if (c.frame == 30) sUsePresenter = 1;
    if (c.frame == 33) { sPresenterPhotoMask = 3; puts("PHOTO_PROBE_GL_UNSAFE_PASS"); }
}
static void Failed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
#ifdef RHH_TEST_PHOTO_ARCHIVE
    struct RhhModArchiveRecord denied = {.struct_size = sizeof(denied)}, beforeDenied = denied;
    if (!ArchiveWrite("x", 1, &denied) || memcmp(&denied, &beforeDenied, sizeof(denied)))
    { puts("PHOTO_PROBE_BAD_ARCHIVE_FAILURE"); return; }
    puts("PHOTO_PROBE_ARCHIVE_FAILURE_BLOCKED");
#endif
    struct RhhPhotoCapture c = {.struct_size = sizeof(c)};
    struct RhhPhotoRenderContext render = {.struct_size = sizeof(render), .ticket = 999}, before = render;
    RhhContentKey variant = 999;
    static unsigned char pixels[RHH_PHOTO_RGB_BYTES];
    if (sPhoto->context(&c) || (c.status != RHH_PHOTO_STALE && c.status != RHH_PHOTO_RENDER_FAILED)
        || !sPhoto->copy_rgb(pixels, sizeof(pixels))
        || !sPhoto->render_context(&render) || memcmp(&render, &before, sizeof(render))
        || !sPhoto->presenter_variant(&variant) || variant != 999)
        puts("PHOTO_PROBE_BAD");
    else printf("PHOTO_PROBE_FAILED %u\n", c.status);
}
static void SelectWorld(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{ (void)event; (void)state; if (sUsePresenter) decision->value = sHost->content_key("test:photo/world"); }
static void OfferPass(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state; (void)decision;
    if (event->ordinal != 32 && event->ordinal != 33) return;
    struct RhhWorldRenderPass pass = {.struct_size = sizeof(pass), .id = sHost->content_key("test:photo/pass"),
        .shader = sHost->content_key(event->ordinal == 32 ? "test:photo/effect_safe" : "test:photo/effect")};
    if (sRender->offer(&pass)) puts("PHOTO_PROBE_BAD");
    if (event->ordinal == 32)
    {
        pass.id = sHost->content_key("test:photo/bad_pass");
        pass.shader = sHost->content_key("test:photo/effect_invalid");
        pass.priority = 1;
        if (sRender->offer(&pass)) puts("PHOTO_PROBE_BAD");
    }
}
static void Tick(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhPhotoCapture capture = {.struct_size = sizeof(capture)};
    if (sActors->at(1, &actor)) { puts("PHOTO_PROBE_BAD"); return; }
    struct RhhPhotoRequest request = {.struct_size = sizeof(request), .owner = sHost->content_key("test:photo_probe"),
        .actor_handle = actor.handle, .max_distance = 4, .checks = RHH_PERCEPTION_CHECK_COLLISION};
    if (sPhoto->request(&request, &capture) || capture.ticket != 1) puts("PHOTO_PROBE_BAD");
    else puts("PHOTO_PROBE_REQUESTED");
}
static int DrawWorld(const struct RhhWorldFrame *frame, const void *state)
{
    (void)state;
    if (frame->struct_size >= RHH_WORLD_FRAME_V1_3_SIZE && frame->photo_target
        && frame->photo_target->struct_size >= RHH_WORLD_PHOTO_TARGET_V1_3_SIZE && frame->photo_target->render_variant)
        *frame->photo_target->render_variant = sHost->content_key("test:photo/variant");
#ifdef _WIN32
#define RHH_GL_CALL __stdcall
#else
#define RHH_GL_CALL
#endif
    void (RHH_GL_CALL *color)(float, float, float, float) = frame->gl_proc("glClearColor");
    void (RHH_GL_CALL *clear)(unsigned) = frame->gl_proc("glClear");
    if (!color || !clear) return -1;
    if (frame->frame == 31 || frame->frame == 32)
    {
        void (RHH_GL_CALL *gen)(int, unsigned *) = frame->gl_proc("glGenTextures");
        void (RHH_GL_CALL *del)(int, const unsigned *) = frame->gl_proc("glDeleteTextures");
        void (RHH_GL_CALL *bind)(unsigned, unsigned) = frame->gl_proc("glBindTexture");
        void (RHH_GL_CALL *param)(unsigned, unsigned, int) = frame->gl_proc("glTexParameteri");
        void (RHH_GL_CALL *image)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *) = frame->gl_proc("glTexImage2D");
        if (!gen || !del || !bind || !param || !image) return -1;
        if (frame->frame == 31)
        {
            static uint16_t samples[RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT];
            for (unsigned y = 0; y < RHH_PHOTO_HEIGHT; ++y)
                for (unsigned x = 0; x < RHH_PHOTO_WIDTH; ++x)
                    samples[y * RHH_PHOTO_WIDTH + x] = (159 - y < 20 ? 10000 : 50000);
            gen(1, &sDepthTexture); bind(0x0de1, sDepthTexture);
            param(0x0de1, 0x2801, 0x2600); param(0x0de1, 0x2800, 0x2600);
            image(0x0de1, 0, 0x81a5, 240, 160, 0, 0x1902, 0x1403, samples);
            *frame->output_depth_texture = sDepthTexture;
        }
        else if (sDepthTexture) { del(1, &sDepthTexture); sDepthTexture = 0; }
    }
    if (sPresenterPhotoMask && frame->struct_size >= RHH_WORLD_FRAME_V1_3_SIZE && frame->photo_target)
    {
        const struct RhhWorldPhotoTarget *photo = frame->photo_target;
        if (photo->struct_size < sizeof(*photo) || photo->actor_slot != 1 || !photo->ticket
            || !photo->visible || !photo->silhouette || !photo->valid) return -1;
        memset(photo->visible, 0, RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT);
        memset(photo->silhouette, 0, RHH_PHOTO_WIDTH * RHH_PHOTO_HEIGHT);
        for (unsigned y = 16; y < 24; ++y) for (unsigned x = 16; x < 24; ++x)
        {
            photo->silhouette[y * RHH_PHOTO_WIDTH + x] = 1;
            photo->visible[y * RHH_PHOTO_WIDTH + x] = x < 20;
        }
        if (sPresenterPhotoMask == 2) photo->visible[0] = 2;
        if (photo->struct_size >= RHH_WORLD_PHOTO_TARGET_V1_1_SIZE && photo->footprint
            && photo->footprint->struct_size >= sizeof(*photo->footprint))
            *photo->footprint = (struct RhhPhotoFootprint){.struct_size = sizeof(*photo->footprint),
                .total_pixels = sPresenterPhotoMask == 3 ? 32 : 64, .inside_pixels = 64,
                .left = 16, .top = 16, .right = 24, .bottom = 24,
                .ticket = photo->ticket};
        *photo->valid = 1;
    }
    color(1, 0, 0, 1); clear(0x4000); return 0;
}
static const struct RhhModHook sHooks[] = {
    {sizeof(struct RhhModHook), RHH_PHOTO_SCORE, RHH_TRANSFORM, 0, Score, NULL},
    {sizeof(struct RhhModHook), RHH_CAN_PHOTO, RHH_CONSTRAINT, 0, Gate, NULL},
    {sizeof(struct RhhModHook), RHH_PHOTO_CAPTURED, RHH_OBSERVE, 0, NULL, Captured},
    {sizeof(struct RhhModHook), RHH_PHOTO_FAILED, RHH_OBSERVE, 0, NULL, Failed},
    {sizeof(struct RhhModHook), RHH_WORLD_PRESENTATION, RHH_TRANSFORM, 0, SelectWorld, NULL},
    {sizeof(struct RhhModHook), RHH_WORLD_RENDER_PASSES, RHH_CONSTRAINT, 0, OfferPass, NULL},
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Tick}
};
static const struct RhhWorldPresenter sPresenter = {sizeof(sPresenter), 0, "test:photo/world", DrawWorld, NULL};
static const struct RhhModShader sShaders[] = {
    {sizeof(struct RhhModShader), 0, "test:photo/effect",
     "#version 120\nuniform sampler2D uScene; varying vec2 vUv; void main(){gl_FragColor=texture2D(uScene,vUv);}"},
    {sizeof(struct RhhModShader), 0, "test:photo/effect_safe",
     "#version 120\nuniform sampler2D uScene; varying vec2 vUv; void main(){gl_FragColor=vec4(texture2D(uScene,vUv).rgb*.5,1.);}"},
    {sizeof(struct RhhModShader), 0, "test:photo/effect_invalid", "#version 120\nthis is not a shader"}
};
static const struct RhhModShaderPhotoPolicy sPhotoPolicy = {
    sizeof(sPhotoPolicy), RHH_SHADER_PHOTO_PRESERVES_POSITION, "test:photo/effect_safe"
};
static const struct RhhServiceRequirement sReq[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 4), RHH_SERVICE_REQUIRED, 0, "rhh:service/photo_capture"},
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 4), RHH_SERVICE_REQUIRED, 0, "rhh:service/wild_actor"}
#ifdef RHH_TEST_PHOTO_ARCHIVE
    , {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 3), RHH_SERVICE_REQUIRED, 0, "rhh:service/mod_archive"}
#endif
};
static const struct RhhModDefinition sDef = {.struct_size = sizeof(sDef), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:photo_probe", .hook_count = 7, .hooks = sHooks, .shader_count = 3, .shaders = sShaders,
    .shader_photo_policy_count = 1, .shader_photo_policies = &sPhotoPolicy,
    .world_presenter_count = 1, .world_presenters = &sPresenter,
    .service_requirement_count = sizeof(sReq) / sizeof(sReq[0]), .service_requirements = sReq};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    sHost = host; sPhoto = (const void *)host->get_service("rhh:service/photo_capture", RHH_SERVICE_VERSION(1, 4));
    sVisibility = (const void *)host->get_service("rhh:service/photo_visibility", RHH_SERVICE_VERSION(1, 8));
    sScore = (const void *)host->get_service("rhh:service/photo_score", RHH_SERVICE_VERSION(1, 0));
    sActors = (const void *)host->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 4));
    sRender = (const void *)host->get_service("rhh:service/world_render", RHH_SERVICE_VERSION(1, 0));
#ifdef RHH_TEST_PHOTO_ARCHIVE
    sArchive = (const void *)host->get_service("rhh:service/mod_archive", RHH_SERVICE_VERSION(1, 3));
    sArchiveOwner = host->content_key("test:photo_probe"); sArchiveKey = host->content_key("test:photo/archive");
    if (!sArchive || !sArchiveOwner || !sArchiveKey) return NULL;
#endif
    return sActors && sPhoto && sVisibility && sScore && sRender && sPhoto->header.struct_size >= sizeof(*sPhoto) ? &sDef : NULL;
}
