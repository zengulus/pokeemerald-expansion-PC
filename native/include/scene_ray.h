#ifndef RHH_SCENE_RAY_H
#define RHH_SCENE_RAY_H
/* Shared geometry-only helpers for scene queries and native presenters. */
#include "mod_api.h"
#include <math.h>
#include <stdint.h>

typedef int (*RhhSceneRayVisit)(int32_t x, int32_t y, void *user);
int RhhSceneTraceBillboard(const struct RhhSceneBillboardTraceRequest *request,
                           struct RhhSceneBillboardTraceResult *out);

/* A supercover walk. Neighboring cells are included because walls can lie on
 * either side of a tile boundary and a ray may pass through a grid corner. */
static inline int RhhSceneRayCells(float x0, float y0, float x1, float y1, RhhSceneRayVisit visit, void *user)
{
    int32_t x = (int32_t)floorf(x0 / 16.f), y = (int32_t)floorf(y0 / 16.f);
    int32_t endX = (int32_t)floorf(x1 / 16.f), endY = (int32_t)floorf(y1 / 16.f);
    double dx = (double)x1 - x0, dy = (double)y1 - y0;
    int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
    double tx = sx ? (((sx > 0 ? (x + 1) * 16.0 : x * 16.0) - x0) / dx) : 1e30;
    double ty = sy ? (((sy > 0 ? (y + 1) * 16.0 : y * 16.0) - y0) / dy) : 1e30;
    double stepX = sx ? 16.0 / fabs(dx) : 1e30, stepY = sy ? 16.0 / fabs(dy) : 1e30;
    for (unsigned steps = 0; steps < 2048; ++steps)
    {
        for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox)
        {
            int result = visit(x + ox, y + oy, user);
            if (result) return result;
        }
        if (x == endX && y == endY) return 0;
        if (x == endX) tx = 1e30;
        if (y == endY) ty = 1e30;
        if (fabs(tx - ty) < 1e-10) { x += sx; y += sy; tx += stepX; ty += stepY; }
        else if (tx < ty) { x += sx; tx += stepX; }
        else { y += sy; ty += stepY; }
    }
    return -1;
}

/* Quads emitted by world_scene have orthogonal edges and matching UV corners.
 * Endpoint intersections are excluded to avoid self-shadowing. alpha is an
 * 8x8 top-left-origin bit mask, sampled with the renderer's nearest filtering. */
static inline int RhhSceneRayQuad(const struct RhhSceneQuad *quad, const float from[3], const float to[3],
                                  uint64_t alpha, int alphaTest, float *fraction)
{
    double a[3], b[3], d[3], origin[3];
    for (unsigned i = 0; i < 3; ++i) {
        a[i] = quad->vertex[1][i] - quad->vertex[0][i];
        b[i] = quad->vertex[3][i] - quad->vertex[0][i];
        d[i] = (double)to[i] - from[i]; origin[i] = quad->vertex[0][i] - from[i];
    }
    double n[3] = {a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0]};
    double den = n[0]*d[0] + n[1]*d[1] + n[2]*d[2];
    if (fabs(den) < 1e-9) return 0;
    double t = (n[0]*origin[0] + n[1]*origin[1] + n[2]*origin[2]) / den;
    if (t <= .0001 || t >= .9999) return 0;
    double p[3], aa = 0, bb = 0, u = 0, v = 0;
    for (unsigned i = 0; i < 3; ++i) {
        p[i] = from[i] + t * d[i] - quad->vertex[0][i];
        aa += a[i]*a[i]; bb += b[i]*b[i]; u += p[i]*a[i]; v += p[i]*b[i];
    }
    if (aa < 1e-9 || bb < 1e-9) return 0;
    u /= aa; v /= bb;
    if (u < -1e-6 || u > 1.000001 || v < -1e-6 || v > 1.000001) return 0;
    if (alphaTest) {
        unsigned px = u <= 0 ? 0 : u >= 1 ? 7 : (unsigned)(u * 8);
        unsigned py = v <= 0 ? 0 : v >= 1 ? 7 : (unsigned)(v * 8);
        if (!(alpha & (UINT64_C(1) << (py * 8 + px)))) return 0;
    }
    if (fraction) *fraction = (float)t;
    return 1;
}
#endif
