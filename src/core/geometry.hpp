#pragma once

#include "vec3.hpp"
#include "colour.hpp"
#include <vector>
#include <memory>

namespace ollygon {

// == base geometry system ==
// ideally almost all our meshes are considered as geo for now,
// but later I want to add implicit meshes, SDFs etc.
// so why not keep a native "sphere" or so too.  It should certainly
// make for faster and nicer raytracing elements & faster serialisation
// at the very least.
//
// so for now lets keep references to both approaches in the sphere etc
//
class Geometry {
public:
    virtual ~Geometry() = default;

    // generates tri mesh data
    virtual void generate_mesh(
        std::vector<float>& verts, //pos(3) + norm(3) per v
        std::vector<unsigned int>& indices
    ) const = 0; // lets keep these pure virtual/abstract & const
                 // (note to self as I forget the func()=0; syntax)

    // for raytracing intersection tests
    virtual bool intersect_ray(
        const Vec3& ray_origin,
        const Vec3& ray_dir,
        float& t_out,
        Vec3& normal_out
    ) const = 0;
};

class SphereGeometry : public Geometry {
public:
    float radius;

    explicit SphereGeometry(float r = 1.0f) : radius(r) {}

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

// quad geometry - defined by corner and two edge vectors
class QuadGeometry : public Geometry {
public:
    Vec3 corner;
    Vec3 u, v;  // two edge vectors from corner
                // copied from Peter Shirley's geometry
                // TEMP - i'm not a fan of this, but it will do for a quick
                // cornell box setup

    QuadGeometry(const Vec3& c, const Vec3& u_vec, const Vec3& v_vec)
        : corner(c), u(u_vec), v(v_vec) {
    }

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

// box geometry - TEMP simple axis--aligned for now
class BoxGeometry : public Geometry {
public:
    Vec3 min;
    Vec3 max;

    BoxGeometry(const Vec3& min_pt, const Vec3& max_pt)
        : min(min_pt), max(max_pt) {}

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
