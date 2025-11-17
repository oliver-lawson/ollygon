#pragma once

#include "vec3.hpp"
#include "colour.hpp"
#include <vector>
#include <string>
#include <memory>

//////////////////////////////////////////////////////////
// Geometry system:
// - Geo: triangle mesh data (vertices, indices etc)
// - Primitive: analytic shapes (cuboid, sphere, quads etc)
//
// both live in local space, transformed by SceneNode
//
//////////////////////////////////////////////////////////

namespace ollygon {

// == geo ==

// vertex data for triangle meshes. putting outside Geometry() for future uses
// elsewhere, eg I imagine we'll want to share that but not the whole of Geometry()
// with raytracer? or serialisers.  Geometry() may get big.
struct Vertex { 

    Vec3 position;
    Vec3 normal;
    //future: Vec2 uv, Vec3 tangent, etc

    Vertex() : position(0, 0, 0), normal(0, 1, 0) {}
    Vertex(const Vec3& _pos, const Vec3& _norm)
        : position(_pos), normal(_norm) {}

};

class Geo {
public:
    Geo() = default;
    ~Geo() = default;

    // vertex/index data
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices; //triplets for tris

    //optional metadata
    std::string source_file; // TODO path to .gltf etc
    
    // helpers for building geo
    void add_vertex(const Vertex& v) { verts.push_back(v); }
    void add_vertex(const Vec3& pos, const Vec3& norm) {
        verts.emplace_back(pos, norm);
    }

    void add_tri(uint32_t i0, uint32_t i1, uint32_t i2) {
        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);
    }

    // geo info

    size_t vertex_count() const { return verts.size(); }
    size_t tri_count() const { return indices.size() / 3; } // TEMP
    bool is_empty() const { return verts.empty() || indices.empty(); }

    //TODO bvh
    bool intersect_ray(
        const Vec3& ray_origin,
        const Vec3& ray_dir,
        float& t_out,
        Vec3& normal_out,
        uint32_t& tri_index_out
    ) const;

    void clear() {
        verts.clear();
        indices.clear();
        source_file.clear();
    }

    // for rendering
    //outputs pos(3)+norm(3) per vert for GPU upload
    void generate_render_data(
        std::vector<float>& vertex_data,
        std::vector<uint32_t>& index_data
    ) const;

private:
    bool intersect_tri(
        const Vec3& ray_origin,
        const Vec3& ray_dir,
        uint32_t tri_index,
        float& t_out,
        Vec3& normal_out
    ) const;
};

// TODO GeoFactory? std::unique_prt<Geo> from_sphere(sphere args); etc?
// & create_teapot(); etc

// == primitives ==
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
