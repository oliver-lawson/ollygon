#include "geometry.hpp"
#include <cmath>
#include <limits>
#include <algorithm>

namespace ollygon{

// == Geo ==

bool Geo::intersect_ray(const Vec3& ray_origin, const Vec3& ray_dir, float& t_out, Vec3& normal_out, uint32_t& tri_index_out) const
{
    float closest_t = std::numeric_limits<float>::max();
    bool hit = false;

    //brute force intersect all tris TODO: BVH/etc later
    for (size_t i = 0; i < indices.size() / 3; i++) {
        float t;
        Vec3 normal;
        if (intersect_tri(ray_origin, ray_dir, i, t, normal)) {
            if (t < closest_t) {
                closest_t = t;
                normal_out = normal;
                tri_index_out = i;
                hit = true;
            }
        }
    }

    if (hit) {
        t_out = closest_t;
        return true;
    }
    return false;
}

bool Geo::intersect_tri(const Vec3& ray_origin, const Vec3& ray_dir, uint32_t tri_index, float& t_out, Vec3& normal_out) const
{
    // Moeller-Trumbore intersection algorithm
    uint32_t i0 = indices[tri_index * 3];
    uint32_t i1 = indices[tri_index * 3 + 1];
    uint32_t i2 = indices[tri_index * 3 + 2];

    Vec3 v0 = verts[i0].position;
    Vec3 v1 = verts[i1].position;
    Vec3 v2 = verts[i2].position;

    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;

    Vec3 h = Vec3::cross(ray_dir, edge2);
    float a = Vec3::dot(edge1, h);

    if (std::abs(a) < 1e-8f) {
        return false;  // ray parallel to triangle
    }

    float f = 1.0f / a;
    Vec3 s = ray_origin - v0;
    float u = f * Vec3::dot(s, h);

    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    Vec3 q = Vec3::cross(s, edge1);
    float v = f * Vec3::dot(ray_dir, q);

    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    float t = f * Vec3::dot(edge2, q);

    if (t < 0.001f) {  // avoid self-intersection
        return false;
    }

    t_out = t;

    // interpolate normal using barycentric coords
    float w = 1.0f - u - v;
    normal_out = verts[i0].normal * w +
        verts[i1].normal * u +
        verts[i2].normal * v;
    normal_out = normal_out.normalised();

    return true;
}

void Geo::generate_render_data(std::vector<float>& vertex_data, std::vector<uint32_t>& index_data) const
{
    vertex_data.clear();
    index_data.clear();

    //interleave position and normal: [x,y,z nx,ny,nz, ...]
    for (const auto& v : verts) {
        vertex_data.push_back(v.position.x);
        vertex_data.push_back(v.position.y);
        vertex_data.push_back(v.position.z);
        vertex_data.push_back(v.normal.x);
        vertex_data.push_back(v.normal.y);
        vertex_data.push_back(v.normal.z);
    }

    index_data = indices;
}


// == Primitives ==

// == Sphere ==

void SpherePrimitive::generate_mesh(std::vector<float>& verts, std::vector<unsigned int>& indices) const
{
    const int segments = 32;
    const int rings = 16;

    size_t vertex_start = verts.size() / 6;

    // generate verts
    for (int ring = 0; ring <= rings; ++ring) {
        float phi = 3.14159f * float(ring) / float(rings);
        for (int seg = 0; seg <= segments; ++seg) {
            float theta = 2.0f * 3.14159f * float(seg) / float(segments);

            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);

            // position
            verts.push_back(x * radius);
            verts.push_back(y * radius);
            verts.push_back(z * radius);

            // normal (normalised position for unit sphere)
            verts.push_back(x); 
            verts.push_back(y);
            verts.push_back(z);
        }
    }

    // generate indices
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            unsigned int current = vertex_start + ring * (segments + 1) + seg;
            unsigned int next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }
}

bool SpherePrimitive::intersect_ray(const Vec3& ray_origin, const Vec3& ray_dir, float& t_out, Vec3& normal_out) const {
    // sphere is at origin in local space (transform applied externally)
    Vec3 oc = ray_origin;
    float a = Vec3::dot(ray_dir, ray_dir);
    float b = 2.0f * Vec3::dot(oc, ray_dir);
    float c = Vec3::dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) return false;

    float t = (-b - std::sqrtf(discriminant)) / (2.0f * a);
    if (t < 0.001f) {  // avoid self-intersection
        t = (-b + std::sqrtf(discriminant)) / (2.0f * a);
        if (t < 0.001f) {
            return false;
        }
    }

    t_out = t;
    Vec3 hit_point = ray_origin + ray_dir * t;
    normal_out = (hit_point / radius).normalised();
    return true;
}

// == Quad ==
void QuadPrimitive::generate_mesh(
    std::vector<float>& verts,
    std::vector<unsigned int>& indices
) const {
    Vec3 normal = Vec3::cross(u, v).normalised();

    size_t vertex_start = verts.size() / 6;

    // quad corners from centre, spanning -uv,+u & -v,+v
    Vec3 corners[4] = {
        -u - v,
        u - v,
        u + v,
        -u + v
    };

    for (int i = 0; i < 4; ++i) {
        verts.push_back(corners[i].x);
        verts.push_back(corners[i].y);
        verts.push_back(corners[i].z);
        verts.push_back(normal.x);
        verts.push_back(normal.y);
        verts.push_back(normal.z);
    }

    // two tris
    indices.push_back(vertex_start + 0);
    indices.push_back(vertex_start + 1);
    indices.push_back(vertex_start + 2);

    indices.push_back(vertex_start + 0);
    indices.push_back(vertex_start + 2);
    indices.push_back(vertex_start + 3);
}

bool QuadPrimitive::intersect_ray(
    const Vec3& ray_origin,
    const Vec3& ray_dir,
    float& t_out,
    Vec3& normal_out
) const {
    Vec3 n = Vec3::cross(u, v).normalised();
    float denom = Vec3::dot(n, ray_dir);

    if (std::abs(denom) < 1e-6f) return false;  // parallel

    // quad is centred at origin in local space
    float t = -Vec3::dot(ray_origin, n) / denom;

    if (t < 0.001f) return false;

    Vec3 hit_point = ray_origin + ray_dir * t;

    // check if inside quad using parametric coords
    // quad spans from -u to +u and -v to +v
    float u_len_sq = Vec3::dot(u, u);
    float v_len_sq = Vec3::dot(v, v);
    float u_param = Vec3::dot(hit_point, u) / u_len_sq;
    float v_param = Vec3::dot(hit_point, v) / v_len_sq;

    if (u_param < -1.0f || u_param > 1.0f || v_param < -1.0f || v_param > 1.0f) {
        return false;
    }

    t_out = t;
    normal_out = n;
    return true;
}

// == Box ==
void CuboidPrimitive::generate_mesh(
    std::vector<float>& verts,
    std::vector<unsigned int>& indices
) const {
    size_t vertex_start = verts.size() / 6;

    Vec3 h = extents / 2; // half the extents to draw these offsets around 0

    Vec3 corners[8] = {
        Vec3(-h.x, -h.y, -h.z),
        Vec3(h.x, -h.y, -h.z),
        Vec3(h.x,  h.y, -h.z),
        Vec3(-h.x,  h.y, -h.z),
        Vec3(-h.x, -h.y,  h.z),
        Vec3(h.x, -h.y,  h.z),
        Vec3(h.x,  h.y,  h.z),
        Vec3(-h.x,  h.y,  h.z)
    };

    struct Face {
        int indices[4];
        Vec3 normal;
    };

    Face faces[6] = {
        {{0, 1, 2, 3}, Vec3(0, 0, -1)},  // front
        {{5, 4, 7, 6}, Vec3(0, 0, 1)},   // back
        {{4, 0, 3, 7}, Vec3(-1, 0, 0)},  // left
        {{1, 5, 6, 2}, Vec3(1, 0, 0)},   // right
        {{4, 5, 1, 0}, Vec3(0, -1, 0)},  // bottom
        {{3, 2, 6, 7}, Vec3(0, 1, 0)}    // top
    };

    for (int f = 0; f < 6; ++f) {
        for (int i = 0; i < 4; ++i) {
            Vec3 corner = corners[faces[f].indices[i]];
            verts.push_back(corner.x);
            verts.push_back(corner.y);
            verts.push_back(corner.z);
            verts.push_back(faces[f].normal.x);
            verts.push_back(faces[f].normal.y);
            verts.push_back(faces[f].normal.z);
        }

        unsigned int base = vertex_start + f * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
}

bool CuboidPrimitive::intersect_ray(
    const Vec3& ray_origin,
    const Vec3& ray_dir,
    float& t_out,
    Vec3& normal_out
)const {
    // slab method for AABB centred at origin
    Vec3 inv_dir = Vec3(
        std::abs(ray_dir.x) > 1e-6f ? 1.0f / ray_dir.x : 1e6f,
        std::abs(ray_dir.y) > 1e-6f ? 1.0f / ray_dir.y : 1e6f,
        std::abs(ray_dir.z) > 1e-6f ? 1.0f / ray_dir.z : 1e6f
    );

    Vec3 h = extents / 2;
    float t1 = (-h.x - ray_origin.x) * inv_dir.x;
    float t2 = (h.x - ray_origin.x) * inv_dir.x;
    float t3 = (-h.y - ray_origin.y) * inv_dir.y;
    float t4 = (h.y - ray_origin.y) * inv_dir.y;
    float t5 = (-h.z - ray_origin.z) * inv_dir.z;
    float t6 = (h.z - ray_origin.z) * inv_dir.z;

    float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
    float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

    if (tmax < 0.0f || tmin > tmax || tmin < 0.001f) {
        return false;
    }

    t_out = tmin;

    // determine which face was hit for normal
    const float epsilon = 0.0001f;
    if (std::abs(tmin - t1) < epsilon) normal_out = Vec3(-1, 0, 0);
    else if (std::abs(tmin - t2) < epsilon) normal_out = Vec3(1, 0, 0);
    else if (std::abs(tmin - t3) < epsilon) normal_out = Vec3(0, -1, 0);
    else if (std::abs(tmin - t4) < epsilon) normal_out = Vec3(0, 1, 0);
    else if (std::abs(tmin - t5) < epsilon) normal_out = Vec3(0, 0, -1);
    else normal_out = Vec3(0, 0, 1);

    return true;
}
} // namespace ollygon
