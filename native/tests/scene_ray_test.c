/* Geometry edge cases shared by the scene trace and HD-2D light blocker. */
#include "scene_ray.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "scene ray:%d: %s\n", __LINE__, #x); exit(1); } } while (0)
struct Cells { unsigned char seen[7][7]; int stop; };
static int Visit(int32_t x, int32_t y, void *user)
{
    struct Cells *cells = user;
    if (x >= -3 && x <= 3 && y >= -3 && y <= 3) cells->seen[y + 3][x + 3] = 1;
    return cells->stop;
}
int main(void)
{
    struct RhhSceneQuad roof = {.vertex = {{0, 0, 8}, {8, 0, 8}, {8, 8, 8}, {0, 8, 8}}};
    float a[3] = {4, 4, 16}, b[3] = {4, 4, 0}, fraction = -1;
    uint64_t pixel = UINT64_C(1) << (4 * 8 + 4);
    CHECK(RhhSceneRayQuad(&roof, a, b, pixel, 1, &fraction) && fraction > .499f && fraction < .501f);
    CHECK(!RhhSceneRayQuad(&roof, a, b, pixel >> 1, 1, NULL));
    CHECK(RhhSceneRayQuad(&roof, a, b, 0, 0, NULL));
    a[2] = 8; CHECK(!RhhSceneRayQuad(&roof, a, b, pixel, 1, NULL)); /* source endpoint */
    a[2] = 16; b[2] = 8; CHECK(!RhhSceneRayQuad(&roof, a, b, pixel, 1, NULL)); /* receiver endpoint */
    b[2] = 16; CHECK(!RhhSceneRayQuad(&roof, a, b, pixel, 1, NULL)); /* parallel */
    struct RhhSceneQuad wall = {.vertex = {{0, 0, 32}, {8, 0, 32}, {8, 0, 16}, {0, 0, 16}}};
    float c[3] = {4, -8, 20}, d[3] = {4, 8, 20};
    uint64_t wallPixel = UINT64_C(1) << (6 * 8 + 4);
    CHECK(RhhSceneRayQuad(&wall, c, d, wallPixel, 1, &fraction) && fraction > .499f && fraction < .501f);
    CHECK(RhhSceneRayQuad(&wall, d, c, wallPixel, 1, NULL));
    CHECK(!RhhSceneRayQuad(&wall, c, d, wallPixel >> 8, 1, NULL));
    c[0] = d[0] = 9; CHECK(!RhhSceneRayQuad(&wall, c, d, UINT64_MAX, 1, NULL));
    memset(&roof, 0, sizeof(roof)); CHECK(!RhhSceneRayQuad(&roof, a, b, UINT64_MAX, 1, NULL));
    struct Cells cells = {0};
    CHECK(!RhhSceneRayCells(16, 0, 0, 16, Visit, &cells));
    CHECK(cells.seen[3][4] && cells.seen[4][3] && cells.seen[3][3]);
    memset(&cells, 0, sizeof(cells));
    CHECK(!RhhSceneRayCells(-17, -17, 17, 17, Visit, &cells));
    CHECK(cells.seen[1][1] && cells.seen[3][3] && cells.seen[4][4]);
    cells.stop = 7; CHECK(RhhSceneRayCells(0, 0, 16, 16, Visit, &cells) == 7);
    puts("scene rays: transparent cutouts, walls, roofs, endpoints and grid corners passed");
    return 0;
}
