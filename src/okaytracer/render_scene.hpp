#pragma once

#include "../core/vec3.hpp"
#include "../core/colour.hpp"
#include "../core/scene.hpp"
#include "../core/material.hpp"
#include <vector>

namespace ollygon {
namespace okaytracer {

// flattened primitive for raytracing
struct RenderPrimitive {
    enum class Type {
        Sphere,
        Quad,
        Cuboid,
        Triangle
    };

    Type type;

    //sphere data
    Vec3 centre;
    float radius;

    //quad data
    Vec3 quad_corner;
    Vec3 quad_u;
    Vec3 quad_v;
    Vec3 quad_normal;

    //cuboid data
    Vec3 cuboid_min;
    Vec3 cuboid_max;

    //tri data (world space)
    Vec3 tri_v0, tri_v1, tri_v2;
    Vec3 tri_n0, tri_n1, tri_n2;

    // material
    Material material;

    RenderPrimitive() : type(Type::Sphere), radius(1.0f) {}
};

// flattened scene optimised for raytracing
class RenderScene {
public:
    std::vector<RenderPrimitive> primitives;

    static RenderScene from_scene(const Scene* scene); //convert

private:
    static void add_node_recursive(
        const SceneNode* node,
        std::vector<RenderPrimitive>& render_prims
    );

    static RenderPrimitive create_sphere_primitive(
        const SceneNode* node,
        const SpherePrimitive* sphere
    );
    static RenderPrimitive create_quad_primitive(
        const SceneNode* node,
        const QuadPrimitive* quad
    );
    static RenderPrimitive create_cuboid_primitive(
        const SceneNode* node,
        const CuboidPrimitive* cuboid
    );

    static void add_mesh_primitives(
        const SceneNode* node,
        const Geo* geo,
        std::vector<RenderPrimitive>& prims
    );


};

} // namespace okaytracer
} // namespace ollygon
