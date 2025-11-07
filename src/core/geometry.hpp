#pragma once

#include "vec3.hpp"
#include "colour.hpp"
#include <vector>
#include <memory>

namespace ollygon {

// == geo ==
// TODO

// 
// all primitivees are analytic shapes defined in local space
// viewport openGL: tessellate into tris
// raytracer/click: optimal analytical ray calculations
// 
enum class PrimitiveType {
    Sphere,
    Quad,
    Cuboid,
    PrimitiveCount
};

class Primitive {
public:
    virtual ~Primitive() = default;

    virtual PrimitiveType get_type() const = 0;

    // generates tri mesh data for viewport rendering (local space)
    virtual void generate_mesh(
        std::vector<float>& verts, //pos(3) + norm(3) per v
        std::vector<unsigned int>& indices
    ) const = 0; // lets keep these pure virtual/abstract & const
                 // (note to self as I forget the func()=0; syntax)

    // analytic raytracing intersection (local space)
    virtual bool intersect_ray(
        const Vec3& ray_origin,
        const Vec3& ray_dir,
        float& t_out,
        Vec3& normal_out
    ) const = 0;
};

class SpherePrimitive : public Primitive {
public:
    float radius;

    explicit SpherePrimitive(float r = 1.0f) : radius(r) {}

    PrimitiveType get_type() const override { return PrimitiveType::Sphere; }

    void generate_mesh(
        std::vector<float>& verts,
        std::vector<unsigned int>& indices
    ) const override;

    bool intersect_ray(
        const Vec3& ray_origin,
        const Vec3& ray_dir,
        float& t_out,
        Vec3& normal_out
    ) const override;
};

// quad geometry - two edge vectors from centre
// quad spanning from -u,-v to +u,+v
class QuadPrimitive : public Primitive {
public:
    Vec3 u, v;  // half-width edge vectors

    QuadPrimitive(const Vec3& u_vec, const Vec3& v_vec)
        : u(u_vec), v(v_vec) {}

    PrimitiveType get_type() const override { return PrimitiveType::Quad; }

    void generate_mesh(
        std::vector<float>& verts,
        std::vector<unsigned int>& indices
    ) const override;

    bool intersect_ray(
        const Vec3& ray_origin,
        const Vec3& ray_dir,
        float& t_out,
        Vec3& normal_out
    ) const override;
};

// cuboid primitive - defined by extents from centre
// eg extents(1,1,2) creates a tall cuboid at centre
class CuboidPrimitive : public Primitive {
public:
    Vec3 extents;

    CuboidPrimitive(const Vec3& _extents = Vec3(1,1,1))
        : extents(_extents) {}

    PrimitiveType get_type() const override { return PrimitiveType::Cuboid; }

    void generate_mesh(
        std::vector<float>& verts,
        std::vector<unsigned int>& indices
    ) const override;

    bool intersect_ray(
        const Vec3& ray_origin,
        const Vec3& ray_dir,
        float& t_out,
        Vec3& normal_out
    ) const override;
};

}
