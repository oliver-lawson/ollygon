#pragma once

#include <cstdint>

namespace ollygon {
namespace okaytracer {

// GPU-compatible POD types - must match CUDA kernel definitions exactly

struct GpuVec3 {
    float x, y, z;
};

struct GpuColour {
    float r, g, b;
};

enum class GpuMaterialType : int {
    Lambertian = 0,
    Metal = 1,
    Dielectric = 2,
    Emissive = 3,
    Chequerboard = 4
};

struct GpuMaterial {
    GpuMaterialType type;
    GpuColour albedo;
    GpuColour emission;
    float roughness;
    float ior;
    GpuColour chequerboard_colour_a;
    GpuColour chequerboard_colour_b;
    float chequerboard_scale;
};

struct GpuSky {
    GpuColour colour_bottom;
    GpuColour colour_top;
    float bottom_height;
    float top_height;
};

enum class GpuPrimitiveType : int {
    Sphere = 0,
    Quad = 1,
    Triangle = 2,
    Cuboid = 3
};

struct GpuRenderPrimitive {
    GpuPrimitiveType type;

    // sphere data
    GpuVec3 centre;
    float radius;

    // quad data
    GpuVec3 quad_corner;
    GpuVec3 quad_u;
    GpuVec3 quad_v;
    GpuVec3 quad_normal;

    // tri data
    GpuVec3 tri_v0, tri_v1, tri_v2;
    GpuVec3 tri_n0, tri_n1, tri_n2;

    // cuboid data
    GpuVec3 cuboid_extents;

    GpuMaterial material;
};

} // namespace okaytracer
} // namespace ollygon
