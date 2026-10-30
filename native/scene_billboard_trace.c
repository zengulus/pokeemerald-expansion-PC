/* Pure geometry helper for presenter-supplied sprite billboards. */
#include "scene_ray.h"
#include <float.h>

int RhhSceneTraceBillboard(const struct RhhSceneBillboardTraceRequest *request,
                           struct RhhSceneBillboardTraceResult *out)
{
    if (!request || request->struct_size < sizeof(*request) || !out || out->struct_size < sizeof(*out)
        || (request->flags & ~RHH_SCENE_TRACE_ALPHA) || !request->width || request->width > RHH_SCENE_BILLBOARD_MAX_DIMENSION
        || !request->height || request->height > RHH_SCENE_BILLBOARD_MAX_DIMENSION || request->reserved[0] || request->reserved[1]) return -1;
    if (request->flags & RHH_SCENE_TRACE_ALPHA)
    {
        uint64_t needed = (uint64_t)(request->height - 1) * request->stride + (uint64_t)request->width * 4;
        if (!request->rgba || request->stride < request->width * 4 || request->stride > 4096
            || needed > request->rgba_size) return -1;
    }
    else if (request->rgba || request->stride || request->rgba_size) return -1;

    double a[3], b[3], d[3], origin[3], aa = 0, ab = 0, bb = 0, scale = 1, position = 1;
    for (unsigned i = 0; i < 3; ++i)
    {
        if (!isfinite(request->from[i]) || !isfinite(request->to[i])
            || fabs(request->from[i]) > 1048576 || fabs(request->to[i]) > 1048576) return -1;
        for (unsigned j = 0; j < 4; ++j)
        {
            double v = request->corners[j][i];
            if (!isfinite(v) || fabs(v) > 1048576) return -1;
            if (fabs(v) > position) position = fabs(v);
        }
        a[i] = (double)request->corners[1][i] - request->corners[0][i];
        b[i] = (double)request->corners[3][i] - request->corners[0][i];
        d[i] = (double)request->to[i] - request->from[i];
        origin[i] = (double)request->corners[0][i] - request->from[i];
        aa += a[i] * a[i]; ab += a[i] * b[i]; bb += b[i] * b[i];
        if (fabs(a[i]) > scale) scale = fabs(a[i]);
        if (fabs(b[i]) > scale) scale = fabs(b[i]);
    }
    for (unsigned i = 0; i < 3; ++i)
        if (fabs((double)request->corners[2][i] - request->corners[0][i] - a[i] - b[i])
            > scale * 0.0001 + position * 4 * FLT_EPSILON) return -1;
    double det = aa * bb - ab * ab;
    if (aa < 1e-9 || bb < 1e-9 || det <= aa * bb * 1e-12) return -1;

    struct RhhSceneBillboardTraceResult result = {.struct_size = sizeof(result), .status = RHH_SCENE_TRACE_CLEAR,
        .fraction_millionths = 1000000, .pixel_x = UINT32_MAX, .pixel_y = UINT32_MAX};
    double n[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    double den = n[0] * d[0] + n[1] * d[1] + n[2] * d[2];
    if (fabs(den) > 1e-9)
    {
        double t = (n[0] * origin[0] + n[1] * origin[1] + n[2] * origin[2]) / den;
        if (t > .0001 && t < .9999)
        {
            double pa = 0, pb = 0;
            for (unsigned i = 0; i < 3; ++i)
            {
                double p = request->from[i] + t * d[i] - request->corners[0][i];
                pa += p * a[i]; pb += p * b[i];
            }
            double u = (pa * bb - pb * ab) / det, v = (pb * aa - pa * ab) / det;
            if (u >= -1e-6 && u <= 1.000001 && v >= -1e-6 && v <= 1.000001)
            {
                u = fmax(0, fmin(1, u)); v = fmax(0, fmin(1, v));
                uint32_t x = u >= 1 ? request->width - 1 : (uint32_t)(u * request->width);
                uint32_t y = v >= 1 ? request->height - 1 : (uint32_t)(v * request->height);
                if (!(request->flags & RHH_SCENE_TRACE_ALPHA) || request->rgba[(size_t)y * request->stride + x * 4 + 3] >= 128)
                {
                    result.status = RHH_SCENE_TRACE_BLOCKED; result.fraction_millionths = (uint32_t)(t * 1000000 + .5);
                    result.u_millionths = (uint32_t)(u * 1000000 + .5); result.v_millionths = (uint32_t)(v * 1000000 + .5);
                    result.pixel_x = x; result.pixel_y = y;
                }
            }
        }
    }
    *out = result; return 0;
}
