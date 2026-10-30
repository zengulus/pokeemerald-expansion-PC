#include "scene_ray.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "billboard trace:%d: %s\n", __LINE__, #x); exit(1); } } while (0)

int main(void)
{
    uint8_t rgba[8 * 8 * 4] = {0};
    struct RhhSceneBillboardTraceRequest request = {.struct_size = sizeof(request), .flags = RHH_SCENE_TRACE_ALPHA,
        .width = 8, .height = 8, .stride = 32, .rgba_size = sizeof(rgba), .rgba = rgba,
        .from = {0, -5, 4}, .to = {0, 5, 4},
        .corners = {{-4, 0, 8}, {4, 0, 8}, {4, 0, 0}, {-4, 0, 0}}};
    struct RhhSceneBillboardTraceResult result = {.struct_size = sizeof(result)};
    CHECK(!RhhSceneTraceBillboard(&request, &result) && result.status == RHH_SCENE_TRACE_CLEAR
        && result.fraction_millionths == 1000000 && result.pixel_x == UINT32_MAX && result.pixel_y == UINT32_MAX);
    rgba[(4 * 8 + 4) * 4 + 3] = 127;
    CHECK(!RhhSceneTraceBillboard(&request, &result) && result.status == RHH_SCENE_TRACE_CLEAR);
    rgba[(4 * 8 + 4) * 4 + 3] = 128;
    CHECK(!RhhSceneTraceBillboard(&request, &result) && result.status == RHH_SCENE_TRACE_BLOCKED
        && result.fraction_millionths == 500000 && result.u_millionths == 500000 && result.v_millionths == 500000
        && result.pixel_x == 4 && result.pixel_y == 4);
    request.from[1] = 5; request.to[1] = -5;
    CHECK(!RhhSceneTraceBillboard(&request, &result) && result.status == RHH_SCENE_TRACE_BLOCKED);
    request.from[1] = 0;
    CHECK(!RhhSceneTraceBillboard(&request, &result) && result.status == RHH_SCENE_TRACE_CLEAR);
    request.from[1] = -5; request.to[1] = 5; request.from[0] = request.to[0] = 4;
    request.from[2] = request.to[2] = 0; rgba[(7 * 8 + 7) * 4 + 3] = 255;
    CHECK(!RhhSceneTraceBillboard(&request, &result) && result.status == RHH_SCENE_TRACE_BLOCKED
        && result.u_millionths == 1000000 && result.v_millionths == 1000000 && result.pixel_x == 7 && result.pixel_y == 7);
    request.from[0] = request.to[0] = 5;
    CHECK(!RhhSceneTraceBillboard(&request, &result) && result.status == RHH_SCENE_TRACE_CLEAR);
    request.flags = 0; request.rgba = NULL; request.stride = request.rgba_size = 0;
    request.from[0] = request.to[0] = 6; request.from[2] = request.to[2] = 4;
    float skew[4][3] = {{0, 0, 8}, {8, 0, 8}, {12, 0, 0}, {4, 0, 0}};
    memcpy(request.corners, skew, sizeof(skew));
    CHECK(!RhhSceneTraceBillboard(&request, &result) && result.status == RHH_SCENE_TRACE_BLOCKED
        && result.u_millionths == 500000 && result.v_millionths == 500000);
    struct RhhSceneBillboardTraceResult saved = result;
    request.corners[2][0] += 1;
    CHECK(RhhSceneTraceBillboard(&request, &result) && !memcmp(&result, &saved, sizeof(saved)));
    request.corners[2][0] -= 1; request.from[0] = NAN;
    CHECK(RhhSceneTraceBillboard(&request, &result) && !memcmp(&result, &saved, sizeof(saved)));
    request.from[0] = 6; request.flags = RHH_SCENE_TRACE_ALPHA; request.rgba = rgba; request.stride = 32; request.rgba_size = sizeof(rgba) - 1;
    CHECK(RhhSceneTraceBillboard(&request, &result) && !memcmp(&result, &saved, sizeof(saved)));
    request.rgba_size = sizeof(rgba); request.stride = 31;
    CHECK(RhhSceneTraceBillboard(&request, &result) && !memcmp(&result, &saved, sizeof(saved)));
    request.stride = 32; request.width = 129;
    CHECK(RhhSceneTraceBillboard(&request, &result) && !memcmp(&result, &saved, sizeof(saved)));
    /* Double-size affine rasters can be 128x128. Exercise padded rows, the
     * alpha threshold, the last pixel and the exact buffer boundary. */
    uint8_t large[128 * 516] = {0};
    request.width = request.height = RHH_SCENE_BILLBOARD_MAX_DIMENSION;
    request.stride = 516; request.rgba_size = sizeof(large); request.rgba = large;
    large[64 * 516 + 64 * 4 + 3] = 128;
    CHECK(!RhhSceneTraceBillboard(&request, &result) && result.status == RHH_SCENE_TRACE_BLOCKED
        && result.pixel_x == 64 && result.pixel_y == 64);
    request.from[0] = request.to[0] = 12; request.from[2] = request.to[2] = 0;
    large[127 * 516 + 127 * 4 + 3] = 255;
    CHECK(!RhhSceneTraceBillboard(&request, &result) && result.status == RHH_SCENE_TRACE_BLOCKED
        && result.pixel_x == 127 && result.pixel_y == 127);
    saved = result; request.rgba_size = 127 * 516 + 128 * 4 - 1;
    CHECK(RhhSceneTraceBillboard(&request, &result) && !memcmp(&result, &saved, sizeof(saved)));
    request.rgba_size++; CHECK(!RhhSceneTraceBillboard(&request, &result) && result.pixel_x == 127);
    saved = result; request.height = 129;
    CHECK(RhhSceneTraceBillboard(&request, &result) && !memcmp(&result, &saved, sizeof(saved)));
    puts("billboard trace: alpha, perspective, skew, endpoints and invalid input passed");
    return 0;
}
