#include <optix.h>
#include <vector_types.h>

// MUST MATCH optix_types.hpp exactly
struct Vec3 {
    float x, y, z;

    __device__ Vec3() : x(0), y(0), z(0) {}
    __device__ Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    __device__ Vec3(float3 v) : x(v.x), y(v.y), z(v.z) {}

    __device__ Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
    __device__ Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
    __device__ Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    __device__ Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }

    __device__ static float dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    __device__ static Vec3 cross(const Vec3& a, const Vec3& b) {
        return Vec3(a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
    }

    __device__ float length() const {
        return sqrtf(x * x + y * y + z * z);
    }

    __device__ Vec3 normalised() const {
        float len = length();
        return len > 1e-8f ? (*this) / len : Vec3(0, 0, 0);
    }

    __device__ float3 to_float3() const {
        return make_float3(x, y, z);
    }
};

struct Colour {
    float r, g, b;

    __device__ Colour() : r(0), g(0), b(0) {}
    __device__ Colour(float r_, float g_, float b_) : r(r_), g(g_), b(b_) {}

    __device__ Colour operator*(float s) const { return Colour(r * s, g * s, b * s); }
    __device__ Colour operator*(const Colour& c) const { return Colour(r * c.r, g * c.g, b * c.b); }
    __device__ Colour operator+(const Colour& c) const { return Colour(r + c.r, g + c.g, b + c.b); }
    __device__ Colour operator/(float s) const { return Colour(r / s, g / s, b / s); }
};

enum class MaterialType : int {
    Lambertian = 0,
    Metal = 1,
    Dielectric = 2,
    Emissive = 3,
    Chequerboard = 4
};

struct Material {
    MaterialType type;
    Colour albedo;
    Colour emission;
    float roughness;
    float ior;
    Colour chequerboard_colour_a;
    Colour chequerboard_colour_b;
    float chequerboard_scale;
};

enum class PrimitiveType : int {
    Sphere = 0,
    Quad = 1,
    Triangle = 2,
    Cuboid = 3
};

struct RenderPrimitive {
    PrimitiveType type;
    Vec3 centre;
    float radius;
    Vec3 quad_corner;
    Vec3 quad_u;
    Vec3 quad_v;
    Vec3 quad_normal;
    Vec3 tri_v0, tri_v1, tri_v2;
    Vec3 tri_n0, tri_n1, tri_n2;
    Vec3 cuboid_extents;
    Material material;
};

struct Params {
    float* output_buffer;
    int width;
    int height;
    int current_sample;
    int max_bounces;
    unsigned long long seed;

    float3 camera_pos;
    float3 viewport_upper_left;
    float3 pixel_delta_u;
    float3 pixel_delta_v;

    OptixTraversableHandle handle;
    RenderPrimitive* primitives;
    int primitive_count;
};

extern "C" {
    __constant__ Params params;
}

////
// == RNG ==

__device__ float random_float(unsigned long long& state) {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return float((state * 0x2545F4914F6CDD1DULL) >> 33) / float(1ULL << 31);
}

__device__ unsigned long long hash_pixel(int x, int y, unsigned long long seed) {
    unsigned long long h = seed;
    h ^= x + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= y + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

__device__ Vec3 random_in_unit_sphere(unsigned long long& rng) {
    while (true) {
        Vec3 p(
            random_float(rng) * 2.0f - 1.0f,
            random_float(rng) * 2.0f - 1.0f,
            random_float(rng) * 2.0f - 1.0f
        );
        if (Vec3::dot(p, p) < 1.0f) return p;
    }
}

__device__ Vec3 random_unit_vector(unsigned long long& rng) {
    return random_in_unit_sphere(rng).normalised();
}

__device__ Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - n * (Vec3::dot(v, n) * 2.0f);
}

__device__ Vec3 refract(const Vec3& uv, const Vec3& n, float etai_over_etat) {
    float cos_theta = fminf(Vec3::dot(uv * -1.0f, n), 1.0f);
    Vec3 r_out_perp = (uv + n * cos_theta) * etai_over_etat;
    Vec3 r_out_parallel = n * (-sqrtf(fabsf(1.0f - Vec3::dot(r_out_perp, r_out_perp))));
    return r_out_perp + r_out_parallel;
}

__device__ float reflectance(float cosine, float ior_ratio) {
    float r0 = (1.0f - ior_ratio) / (1.0f + ior_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf((1.0f - cosine), 5.0f);
}

__device__ Colour get_chequerboard_colour(const Vec3& point, const Material& mat) {
    float scale = mat.chequerboard_scale;
    int xi = int(floorf(point.x * scale));
    int yi = int(floorf(point.y * scale));
    int zi = int(floorf(point.z * scale));
    bool is_even = (xi + yi + zi) % 2 == 0;
    return is_even ? mat.chequerboard_colour_a : mat.chequerboard_colour_b;
}

////////
// == Intersect Functions ==

__device__ bool intersect_sphere(
    const RenderPrimitive& prim,
    const Vec3& ray_origin,
    const Vec3& ray_dir,
    float t_min,
    float t_max,
    float& t_out,
    Vec3& normal_out
) {
    Vec3 oc = ray_origin - prim.centre;
    float a = Vec3::dot(ray_dir, ray_dir);
    float half_b = Vec3::dot(oc, ray_dir);
    float c = Vec3::dot(oc, oc) - prim.radius * prim.radius;
    float discriminant = half_b * half_b - a * c;

    if (discriminant < 0.0f) return false;

    float sqrtd = sqrtf(discriminant);
    float root = (-half_b - sqrtd) / a;

    if (root < t_min || root > t_max) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || root > t_max) return false;
    }

    t_out = root;
    Vec3 hit_point = ray_origin + ray_dir * root;
    normal_out = (hit_point - prim.centre) / prim.radius;
    return true;
}

__device__ bool intersect_quad(
    const RenderPrimitive& prim,
    const Vec3& ray_origin,
    const Vec3& ray_dir,
    float t_min,
    float t_max,
    float& t_out,
    Vec3& normal_out
) {
    float denom = Vec3::dot(prim.quad_normal, ray_dir);
    if (fabsf(denom) < 1e-8f) return false;

    float t = Vec3::dot(prim.quad_corner - ray_origin, prim.quad_normal) / denom;
    if (t < t_min || t > t_max) return false;

    Vec3 hit_point = ray_origin + ray_dir * t;
    Vec3 hit_vec = hit_point - prim.quad_corner;

    float u = Vec3::dot(hit_vec, prim.quad_u) / Vec3::dot(prim.quad_u, prim.quad_u);
    float v = Vec3::dot(hit_vec, prim.quad_v) / Vec3::dot(prim.quad_v, prim.quad_v);

    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return false;

    t_out = t;
    normal_out = prim.quad_normal;
    return true;
}

__device__ bool intersect_triangle(
    const RenderPrimitive& prim,
    const Vec3& ray_origin,
    const Vec3& ray_dir,
    float t_min,
    float t_max,
    float& t_out,
    Vec3& normal_out
) {
    Vec3 edge1 = prim.tri_v1 - prim.tri_v0;
    Vec3 edge2 = prim.tri_v2 - prim.tri_v0;

    Vec3 h = Vec3::cross(ray_dir, edge2);
    float a = Vec3::dot(edge1, h);

    if (fabsf(a) < 1e-8f) return false;

    float f = 1.0f / a;
    Vec3 s = ray_origin - prim.tri_v0;
    float u = f * Vec3::dot(s, h);

    if (u < 0.0f || u > 1.0f) return false;

    Vec3 q = Vec3::cross(s, edge1);
    float v = f * Vec3::dot(ray_dir, q);

    if (v < 0.0f || u + v > 1.0f) return false;

    float t = f * Vec3::dot(edge2, q);
    if (t < t_min || t > t_max) return false;

    t_out = t;
    float w = 1.0f - u - v;
    normal_out = (prim.tri_n0 * w + prim.tri_n1 * u + prim.tri_n2 * v).normalised();
    return true;
}

__device__ bool intersect_cuboid(
    const RenderPrimitive& prim,
    const Vec3& ray_origin,
    const Vec3& ray_dir,
    float t_min,
    float t_max,
    float& t_out,
    Vec3& normal_out
) {
    // aabb slab with cuboid centred at prim.centre
    // i'm not using this as we lack rotation, but leaving for possible use later
    Vec3 inv_dir = Vec3(
        fabsf(ray_dir.x) > 1e-6f ? 1.0f / ray_dir.x : 1e6f,
        fabsf(ray_dir.y) > 1e-6f ? 1.0f / ray_dir.y : 1e6f,
        fabsf(ray_dir.z) > 1e-6f ? 1.0f / ray_dir.z : 1e6f
    );

    Vec3 h = prim.cuboid_extents * 0.5f;
    Vec3 box_min = prim.centre - h;
    Vec3 box_max = prim.centre + h;

    float t1 = (box_min.x - ray_origin.x) * inv_dir.x;
    float t2 = (box_max.x - ray_origin.x) * inv_dir.x;
    float t3 = (box_min.y - ray_origin.y) * inv_dir.y;
    float t4 = (box_max.y - ray_origin.y) * inv_dir.y;
    float t5 = (box_min.z - ray_origin.z) * inv_dir.z;
    float t6 = (box_max.z - ray_origin.z) * inv_dir.z;

    float tmin = fmaxf(fmaxf(fminf(t1, t2), fminf(t3, t4)), fminf(t5, t6));
    float tmax = fminf(fminf(fmaxf(t1, t2), fmaxf(t3, t4)), fmaxf(t5, t6));

    if (tmax < 0.0f || tmin > tmax || tmin < t_min || tmin > t_max) {
        return false;
    }

    t_out = tmin;

    // determine which face was hit
    const float epsilon = 0.0001f;
    if (fabsf(tmin - t1) < epsilon) normal_out = Vec3(-1, 0, 0);
    else if (fabsf(tmin - t2) < epsilon) normal_out = Vec3(1, 0, 0);
    else if (fabsf(tmin - t3) < epsilon) normal_out = Vec3(0, -1, 0);
    else if (fabsf(tmin - t4) < epsilon) normal_out = Vec3(0, 1, 0);
    else if (fabsf(tmin - t5) < epsilon) normal_out = Vec3(0, 0, -1);
    else normal_out = Vec3(0, 0, 1);

    return true;
}

/////////
// == OptiX Programs ==

extern "C" __global__ void __intersection__is() {
    const int prim_idx = optixGetPrimitiveIndex();
    const RenderPrimitive& prim = params.primitives[prim_idx];

    const float3 ray_orig = optixGetObjectRayOrigin();
    const float3 ray_dir = optixGetObjectRayDirection();
    const float t_min = optixGetRayTmin();
    const float t_max = optixGetRayTmax();

    Vec3 origin(ray_orig);
    Vec3 direction(ray_dir);

    float t;
    Vec3 normal;
    bool hit = false;

    switch (prim.type) {
    case PrimitiveType::Sphere:
        hit = intersect_sphere(prim, origin, direction, t_min, t_max, t, normal);
        break;
    case PrimitiveType::Quad:
        hit = intersect_quad(prim, origin, direction, t_min, t_max, t, normal);
        break;
    case PrimitiveType::Triangle:
        hit = intersect_triangle(prim, origin, direction, t_min, t_max, t, normal);
        break;
    case PrimitiveType::Cuboid:
        hit = intersect_cuboid(prim, origin, direction, t_min, t_max, t, normal);
        break;
    }

    if (hit) {
        optixReportIntersection(
            t, 0,
            __float_as_uint(normal.x),
            __float_as_uint(normal.y),
            __float_as_uint(normal.z)
        );
    }
}

extern "C" __global__ void __closesthit__ch() {
    // payload layout: we pass 15 values (0-14) for now, 32 uints available for future use (pre-Wavefront restructure)
    // 0-1: RNG state (64-bit split into two 32-bit)
    // 2: scatter event (0=bounced, 1=cancelled, 2=missed)
    // 3-5: scattered origin (xyz)
    // 6-8: scattered direction (xyz)
    // 9-11: attenuation colour (rgb)
    // 12-14: emission colour (rgb)

    unsigned int p0 = optixGetPayload_0();
    unsigned int p1 = optixGetPayload_1();
    unsigned int p2 = optixGetPayload_2();
    unsigned int p3 = optixGetPayload_3();
    unsigned int p4 = optixGetPayload_4();
    unsigned int p5 = optixGetPayload_5();
    unsigned int p6 = optixGetPayload_6();
    unsigned int p7 = optixGetPayload_7();
    unsigned int p8 = optixGetPayload_8();
    unsigned int p9 = optixGetPayload_9();
    unsigned int p10 = optixGetPayload_10();
    unsigned int p11 = optixGetPayload_11();
    unsigned int p12 = optixGetPayload_12();
    unsigned int p13 = optixGetPayload_13();
    unsigned int p14 = optixGetPayload_14();

    const int prim_idx = optixGetPrimitiveIndex();
    const RenderPrimitive& prim = params.primitives[prim_idx];

    const float3 ray_dir_f3 = optixGetWorldRayDirection();
    const float3 ray_orig_f3 = optixGetWorldRayOrigin();
    const float t_hit = optixGetRayTmax();

    Vec3 ray_dir(ray_dir_f3);
    Vec3 ray_origin(ray_orig_f3);

    Vec3 normal(
        __uint_as_float(optixGetAttribute_0()),
        __uint_as_float(optixGetAttribute_1()),
        __uint_as_float(optixGetAttribute_2())
    );

    Vec3 hit_point = ray_origin + ray_dir * t_hit;

    bool front_face = Vec3::dot(ray_dir, normal) < 0;
    Vec3 surface_normal = front_face ? normal : normal * -1.0f;
    
    // reconstruct 64-bit RNG from two 32-bit payloads
    unsigned long long rand_state = ((unsigned long long)p1 << 32) | p0; //p0 = low, p1=high

    /////
    // == Mat Payloads ==
    
    // special emissive case
    if (prim.material.type == MaterialType::Emissive) {
        p2 = 1; // cancelled
        p12 = __float_as_uint(prim.material.emission.r);
        p13 = __float_as_uint(prim.material.emission.g);
        p14 = __float_as_uint(prim.material.emission.b);

        optixSetPayload_0(p0);
        optixSetPayload_1(p1);
        optixSetPayload_2(p2);
        optixSetPayload_12(p12);
        optixSetPayload_13(p13);
        optixSetPayload_14(p14);
        return;
    }

    Vec3 scatter_dir;
    Colour attenuation;
    bool scattered = false;

    switch (prim.material.type) {
    case MaterialType::Lambertian: {
        scatter_dir = surface_normal + random_unit_vector(rand_state);
        attenuation = prim.material.albedo;
        scattered = true;
        break;
    }
    case MaterialType::Metal: {
        Vec3 reflected = reflect(ray_dir.normalised(), surface_normal);
        Vec3 fuzz = random_unit_vector(rand_state) * prim.material.roughness;
        scatter_dir = (reflected + fuzz).normalised();
        attenuation = prim.material.albedo;
        scattered = (Vec3::dot(scatter_dir, surface_normal) > 0);
        break;
    }
    case MaterialType::Dielectric: {
        attenuation = Colour(1, 1, 1);
        float refraction_ratio = front_face ? (1.0f / prim.material.ior) : prim.material.ior;
        Vec3 unit_dir = ray_dir.normalised();
        float cos_theta = fminf(Vec3::dot(unit_dir * -1.0f, surface_normal), 1.0f);
        float sin_theta = sqrtf(1.0f - cos_theta * cos_theta);
        bool cannot_refract = refraction_ratio * sin_theta > 1.0f;

        if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_float(rand_state))
        {
            scatter_dir = reflect(unit_dir, surface_normal);
        }
        else
        {
            scatter_dir = refract(unit_dir, surface_normal, refraction_ratio);
        }
        scattered = true;
        break;
    }
    case MaterialType::Chequerboard: {
        scatter_dir = surface_normal + random_unit_vector(rand_state);
        attenuation = get_chequerboard_colour(hit_point, prim.material);
        scattered = true;
        break;
    }
    }

    if (scattered) {
        p2 = 0; // bounced
        p3 = __float_as_uint(hit_point.x);
        p4 = __float_as_uint(hit_point.y);
        p5 = __float_as_uint(hit_point.z);
        p6 = __float_as_uint(scatter_dir.x);
        p7 = __float_as_uint(scatter_dir.y);
        p8 = __float_as_uint(scatter_dir.z);
        p9 = __float_as_uint(attenuation.r);
        p10 = __float_as_uint(attenuation.g);
        p11 = __float_as_uint(attenuation.b);
    }
    else {
        p2 = 1; // cancelled
    }

    // write back rng state
    p0 = (unsigned int)(rand_state & 0xFFFFFFFF);
    p1 = (unsigned int)(rand_state >> 32);

    optixSetPayload_0(p0);
    optixSetPayload_1(p1);
    optixSetPayload_2(p2);
    optixSetPayload_3(p3);
    optixSetPayload_4(p4);
    optixSetPayload_5(p5);
    optixSetPayload_6(p6);
    optixSetPayload_7(p7);
    optixSetPayload_8(p8);
    optixSetPayload_9(p9);
    optixSetPayload_10(p10);
    optixSetPayload_11(p11);
    optixSetPayload_12(p12);
    optixSetPayload_13(p13);
    optixSetPayload_14(p14);
}

extern "C" __global__ void __miss__ms() {
    unsigned int p2 = optixGetPayload_2();
    p2 = 2; // missed
    optixSetPayload_2(p2);
}

extern "C" __global__ void __raygen__rg() {
    const uint3 idx = optixGetLaunchIndex();
    const int x = idx.x;
    const int y = idx.y;

    if (x >= params.width || y >= params.height) return;

    unsigned long long pixel_rng = hash_pixel(x, y, params.seed + params.current_sample);

    float px = float(x) + random_float(pixel_rng);
    float py = float(y) + random_float(pixel_rng);

    Vec3 camera_pos(params.camera_pos);
    Vec3 viewport_upper_left(params.viewport_upper_left);
    Vec3 pixel_delta_u(params.pixel_delta_u);
    Vec3 pixel_delta_v(params.pixel_delta_v);

    Vec3 pixel_centre = viewport_upper_left + pixel_delta_u * px - pixel_delta_v * py;
    Vec3 ray_dir = (pixel_centre - camera_pos).normalised();

    Colour throughput(1.0f, 1.0f, 1.0f);
    Colour accumulated(0.0f, 0.0f, 0.0f);

    Vec3 current_origin = camera_pos;
    Vec3 current_dir = ray_dir;

    for (int depth = 0; depth < params.max_bounces; depth++) {
        // split rng into two 32-bit values
        unsigned int p0 = (unsigned int)(pixel_rng & 0xFFFFFFFF);
        unsigned int p1 = (unsigned int)(pixel_rng >> 32);
        unsigned int p2 = 2; // scatter event = missed
        unsigned int p3 = 0, p4 = 0, p5 = 0; // scattered origin
        unsigned int p6 = 0, p7 = 0, p8 = 0; // scattered dir
        unsigned int p9 = 0, p10 = 0, p11 = 0; // attenuation
        unsigned int p12 = 0, p13 = 0, p14 = 0; // emission

        optixTrace(
            params.handle,
            current_origin.to_float3(),
            current_dir.to_float3(),
            0.001f,
            1e16f,
            0.0f,
            OptixVisibilityMask(255),
            OPTIX_RAY_FLAG_NONE,
            0, 1, 0,
            p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14
        );

        //reconstruct rng
        pixel_rng = ((unsigned long long)p1 << 32) | p0;

        int scatter_event = p2;
        float3 scattered_origin = make_float3(
            __uint_as_float(p3),
            __uint_as_float(p4),
            __uint_as_float(p5)
        );
        float3 scattered_direction = make_float3(
            __uint_as_float(p6),
            __uint_as_float(p7),
            __uint_as_float(p8)
        );
        Colour attenuation(
            __uint_as_float(p9),
            __uint_as_float(p10),
            __uint_as_float(p11)
        );
        Colour emission(
            __uint_as_float(p12),
            __uint_as_float(p13),
            __uint_as_float(p14)
        );

        if (scatter_event == 2)
        {   // missed
            Vec3 unit_dir = current_dir;
            float t = 0.5f * (unit_dir.z + 1.0f);
            Colour white(1.0f, 1.0f, 1.0f);
            Colour blue(0.5f, 0.7f, 1.0f);
            Colour sky = (white * (1.0f - t) + blue * t) * 0.005f;
            accumulated = accumulated + throughput * sky;
            break;
        }
        else if (scatter_event == 1)
        {   // cancelled (emissive)
            accumulated = accumulated + throughput * emission;
            break;
        }
        else
        {   // bounced
            throughput = throughput * attenuation;

            // russian roulette
            if (depth >= 3) {
                float p = fmaxf(throughput.r, fmaxf(throughput.g, throughput.b));
                if (random_float(pixel_rng) > p) break;
                throughput = throughput / p;
            }

            current_origin = Vec3(scattered_origin);
            current_dir = Vec3(scattered_direction).normalised();
        }
    }

    int pixel_index = (y * params.width + x) * 3;
    params.output_buffer[pixel_index + 0] = accumulated.r;
    params.output_buffer[pixel_index + 1] = accumulated.g;
    params.output_buffer[pixel_index + 2] = accumulated.b;
}
