#include "raytracer.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

namespace ollygon {
namespace okaytracer {

Raytracer::Raytracer()
    : rendering(false)
    , current_sample(0)
    {}

Raytracer::~Raytracer() {
    stop_render();
}

void Raytracer::start_render(const RenderScene& new_scene, const Camera& new_camera, const RenderConfig& new_config) {
    scene = new_scene;
    camera = new_camera;
    config = new_config;
    
    pixels.resize(config.width * config.height * 3, 0.0f);
    sample_buffer.resize(config.width * config.height * 3, 0.0f);

    rendering = true;
    current_sample = 0;
}

void Raytracer::stop_render()
{
    rendering = false;
}

float Raytracer::get_progress() const
{
    if (config.samples_per_pixel == 0) return 1.0f;
    return float(current_sample) / float(config.samples_per_pixel);
}

void Raytracer::render_one_sample() {
    if (!rendering || current_sample >= config.samples_per_pixel) {
        rendering = false;
        return;
    }

    std::fill(sample_buffer.begin(), sample_buffer.end(), 0.0f);

    CameraBasis basis = compute_camera_basis();

    // split image into horizontal tiles
    const int tile_size = 64; //TODO profile
    int tiles_x = (config.width + tile_size - 1) / tile_size;
    int tiles_y = (config.height + tile_size - 1) / tile_size;

    std::vector<std::thread> threads;

    for (int ty = 0; ty < tiles_y; ++ty) {
        for (int tx = 0; tx < tiles_x; ++tx) {
            int start_x = tx * tile_size;
            int end_x = std::min(start_x + tile_size, config.width);
            int start_y = ty * tile_size;
            int end_y = std::min(start_y + tile_size, config.height);

            threads.emplace_back([this, start_x, end_x, start_y, end_y, basis]() {
                render_tile(start_x, end_x, start_y, end_y, basis);
                });
        }
    }

    for (auto& t : threads) {
        t.join();
    }

    // accumulate
    float weight = 1.0f / float(current_sample + 1);
    for (size_t i = 0; i < pixels.size(); i++) {
        pixels[i] = pixels[i] * (1.0f - weight) + sample_buffer[i] * weight;
    }
    current_sample++;

    if (current_sample >= config.samples_per_pixel) {
        rendering = false;
    }
}

void Raytracer::render_tile(int start_x, int end_x, int start_y, int end_y, const CameraBasis& basis) {
    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            // seed rng deterministically per pixel
            uint64_t pixel_rng = hash_pixel(x, y, config.seed + current_sample);

            float px = float(x) + random_float(pixel_rng);
            float py = float(y) + random_float(pixel_rng);

            // convert u,v to pixel position with jitter
            Vec3 pixel_centre = basis.viewport_upper_left
                + basis.pixel_delta_u * px
                - basis.pixel_delta_v * py;

            Vec3 ray_dir = (pixel_centre - basis.camera_pos).normalised();
            Ray ray(basis.camera_pos, ray_dir);

            Colour pixel_colour = ray_colour(ray, config.max_bounces, pixel_rng);

            int pixel_index = (y * config.width + x) * 3;
            sample_buffer[pixel_index + 0] += pixel_colour.r;
            sample_buffer[pixel_index + 1] += pixel_colour.g;
            sample_buffer[pixel_index + 2] += pixel_colour.b;
        }
    }
}

uint64_t Raytracer::hash_pixel(int x, int y, uint64_t seed) const {
    // Murmurhash based
    uint64_t h = seed;
    h ^= x + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= y + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

CameraBasis Raytracer::compute_camera_basis() const {
    Vec3 forward = (camera.get_target() - camera.get_pos()).normalised();
    Vec3 right = Vec3::cross(forward, camera.get_up()).normalised();
    Vec3 up = Vec3::cross(right, forward);

    float aspect = float(config.width) / float(config.height);
    float fov_rad = camera.get_fov_degs() * DEG_TO_RAD;
    float h = std::tan(fov_rad * 0.5f);
    float viewport_height = 2.0f * h;
    float viewport_width = viewport_height * aspect;

    Vec3 viewport_u = right * viewport_width;
    Vec3 viewport_v = up * viewport_height;

    CameraBasis basis;
    basis.viewport_upper_left = camera.get_pos() + forward - viewport_u * 0.5f + viewport_v * 0.5f;
    basis.pixel_delta_u = viewport_u / float(config.width);
    basis.pixel_delta_v = viewport_v / float(config.height);
    basis.camera_pos = camera.get_pos();
    return basis;
}

// == render methods ==
bool Raytracer::intersect(const Ray& ray, float t_min, float t_max, Intersection& rec) const
{
    Intersection temp_rec;
    bool hit_anything = false;
    float closest_so_far = t_max;

    for (const auto& prim : scene.primitives) {
        bool hit = false;

        switch (prim.type) {
        case RenderPrimitive::Type::Sphere:
            hit = intersect_sphere(prim, ray, t_min, closest_so_far, temp_rec);
            break;
        case RenderPrimitive::Type::Quad:
            hit = intersect_quad(prim, ray, t_min, closest_so_far, temp_rec);
            break;
        case RenderPrimitive::Type::Triangle:
            hit = intersect_triangle(prim, ray, t_min, closest_so_far, temp_rec);
            break;
        }

        if (hit) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }

    return hit_anything;
}

// == intersect prims ==

bool Raytracer::intersect_sphere(const RenderPrimitive& prim, const Ray& ray, float t_min, float t_max, Intersection& rec ) const
{
    Vec3 oc = ray.origin - prim.centre;
    float a = Vec3::dot(ray.direction, ray.direction);
    float half_b = Vec3::dot(oc, ray.direction);
    float c = Vec3::dot(oc, oc) - prim.radius * prim.radius;
    float discriminant = half_b * half_b - a * c;

    if (discriminant < 0.0f) return false;

    float sqrtd = std::sqrt(discriminant);
    float root = (-half_b - sqrtd) / a;

    if (root < t_min || root > t_max) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || root > t_max) {
            return false;
        }
    }

    rec.t = root;
    rec.point = ray.at(rec.t);
    Vec3 outward_normal = (rec.point - prim.centre) / prim.radius;
    rec.set_face_normal(ray, outward_normal);
    rec.material = prim.material;

    return true;
}

bool Raytracer::intersect_quad( const RenderPrimitive& prim, const Ray& ray, float t_min, float t_max, Intersection& rec ) const
{
    float denom = Vec3::dot(prim.quad_normal, ray.direction);

    if (std::abs(denom) < ALMOST_ZERO) return false;

    float t = Vec3::dot(prim.quad_corner - ray.origin, prim.quad_normal) / denom;

    if (t < t_min || t > t_max) return false;

    Vec3 hit_point = ray.at(t);
    Vec3 hit_vec = hit_point - prim.quad_corner;

    float u = Vec3::dot(hit_vec, prim.quad_u) / Vec3::dot(prim.quad_u, prim.quad_u);
    float v = Vec3::dot(hit_vec, prim.quad_v) / Vec3::dot(prim.quad_v, prim.quad_v);

    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
        return false;
    }

    rec.t = t;
    rec.point = hit_point;
    rec.set_face_normal(ray, prim.quad_normal);
    rec.material = prim.material;

    return true;
}

bool Raytracer::intersect_triangle( const RenderPrimitive& prim, const Ray& ray, float t_min, float t_max, Intersection& rec ) const
{
    //TEMP copied over from geometry.cpp TODO:unify
    // Moeller-Trumbore intersection algorithm
    Vec3 edge1 = prim.tri_v1 - prim.tri_v0;
    Vec3 edge2 = prim.tri_v2 - prim.tri_v0;

    Vec3 h = Vec3::cross(ray.direction, edge2);
    float a = Vec3::dot(edge1, h);

    if (std::abs(a) < ALMOST_ZERO) {
        return false;  // ray parallel to triangle
    }

    float f = 1.0f / a;
    Vec3 s = ray.origin - prim.tri_v0;
    float u = f * Vec3::dot(s, h);

    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    Vec3 q = Vec3::cross(s, edge1);
    float v = f * Vec3::dot(ray.direction, q);

    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    float t = f * Vec3::dot(edge2, q);

    if (t < t_min || t > t_max) {
        return false;
    }

    rec.t = t;
    rec.point = ray.at(rec.t);

    // interpolate normal using barycentric coords
    float w = 1.0f - u - v;
    Vec3 interpolated_normal = prim.tri_n0 * w + prim.tri_n1 * u + prim.tri_n2 * v;
    rec.set_face_normal(ray, interpolated_normal.normalised());

    rec.material = prim.material;

    return true;
}

// note..recursive, not sure if this will be a problem for gpu accel later
Colour Raytracer::ray_colour(const Ray& ray, int depth, uint64_t& rng) const
{
    if (depth <= 0) return Colour(0, 0, 0);

    Intersection rec;
    //TODO shirley-style "interval" here? maybe call it RayRange/t_range
    if (intersect(ray, 0.001f, std::numeric_limits<float>::infinity(), rec)) {
       
        if (rec.material.type == MaterialType::Emissive) {
            return rec.material.emission;
        }

        // russian roulette termination of rays.  on cornell box, about +11% perf
        if (depth < 4) {  // after a few bounces
            float p = std::max(rec.material.albedo.r,
                std::max(rec.material.albedo.g, rec.material.albedo.b));
            if (random_float(rng) > p) {
                return Colour(0, 0, 0);  // terminate early
            }
            // boost surviving rays
            rec.material.albedo = rec.material.albedo / p;
        }

        // scatter
        Ray scattered;
        Colour attenuation;

        if (scatter(ray, rec, attenuation, scattered, rng)) {
            Colour bounced = ray_colour(scattered, depth - 1, rng);
            return Colour(
                attenuation.r * bounced.r,
                attenuation.g * bounced.g,
                attenuation.b * bounced.b
            );
        }

        return Colour(0, 0, 0);
    }

    // bg/TEMP sky gradient, dark for cool renders
    //TODO: unified world bg settings
    Vec3 unit_dir = ray.direction;
    float t = 0.5f * (unit_dir.y + 1.0f);
    Colour white(1.0f, 1.0f, 1.0f);
    Colour blue(0.5f, 0.7f, 1.0f);

    return (white * (1.0f - t) + blue * t) * 0.005f;
}

bool Raytracer::scatter(const Ray& ray_in, const Intersection& rec, Colour& attenuation, Ray& scattered, uint64_t& rng) const
{
    switch (rec.material.type)
    {
        case MaterialType::Lambertian:
            return scatter_lambertian(ray_in, rec, attenuation, scattered, rng);
        case MaterialType::Metal:
            return scatter_metal(ray_in, rec, attenuation, scattered, rng);
        case MaterialType::Dielectric:
            return scatter_dielectric(ray_in, rec, attenuation, scattered, rng);
        case MaterialType::Chequerboard:
            attenuation = get_chequerboard_colour(rec.point, rec.material);
            scattered = Ray(rec.point, rec.normal + random_unit_vector(rng));
            return true;
        default:
            return false;
    }
}

bool Raytracer::scatter_lambertian(const Ray& ray_in, const Intersection& rec, Colour& attenuation, Ray& scattered, uint64_t& rng) const
{
    Vec3 scatter_dir = rec.normal + random_unit_vector(rng);

    // catch degenerate scatter direction
    if (std::abs(scatter_dir.x) < ALMOST_ZERO &&
        std::abs(scatter_dir.y) < ALMOST_ZERO &&
        std::abs(scatter_dir.z) < ALMOST_ZERO) {
        scatter_dir = rec.normal;
    }

    scattered = Ray(rec.point, scatter_dir.normalised());
    attenuation = rec.material.albedo;

    return true;
}

bool Raytracer::scatter_metal(const Ray& ray_in, const Intersection& rec, Colour& attenuation, Ray& scattered, uint64_t& rng) const
{
    Vec3 reflected = reflect(ray_in.direction.normalised(), rec.normal);

    //TEMP adding roughness by perturbing reflection 
    //TODO: read pbrt microfacet roughness chapter
    Vec3 fuzz = random_unit_vector(rng) * rec.material.roughness;
    scattered = Ray(rec.point, (reflected + fuzz).normalised());
    attenuation = rec.material.albedo;

    return Vec3::dot(scattered.direction, rec.normal) > 0;
}

bool Raytracer::scatter_dielectric(const Ray& ray_in, const Intersection& rec, Colour& attenuation, Ray& scattered, uint64_t& rng) const
{
    attenuation = Colour(1, 1, 1);
    float refraction_ratio = rec.front_face ? (1.0f / rec.material.ior) : rec.material.ior;

    Vec3 unit_dir = ray_in.direction.normalised();
    float cos_theta = std::min(Vec3::dot(unit_dir * -1.0f, rec.normal), 1.0f);
    float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

    bool cannot_refract = refraction_ratio * sin_theta > 1.0f;
    Vec3 direction;

    // schlick approx
    if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_float(rng)) {
        direction = reflect(unit_dir, rec.normal);
    }
    else {
        direction = refract(unit_dir, rec.normal, refraction_ratio);
    }

    scattered = Ray(rec.point, direction);
    return true;
}

Colour Raytracer::get_chequerboard_colour(const Vec3& point, const Material& mat) const
{
    float scale = mat.chequerboard_scale;
    int xi = int(std::floor(point.x * scale));
    int yi = int(std::floor(point.y * scale));
    int zi = int(std::floor(point.z * scale));

    bool is_even = (xi + yi + zi) % 2 == 0;

    return is_even ? mat.chequerboard_colour_a : mat.chequerboard_colour_b;
}

Vec3 Raytracer::random_in_unit_sphere(uint64_t& rng) const
{
    while (true) {
        Vec3 p(
            random_float(rng) * 2.0f - 1.0f,
            random_float(rng) * 2.0f - 1.0f,
            random_float(rng) * 2.0f - 1.0f
        );
        if (Vec3::dot(p, p) < 1.0f) {
            return p;
        }
    }
}

Vec3 Raytracer::random_unit_vector(uint64_t& rng) const
{
    return random_in_unit_sphere(rng).normalised();
}

Vec3 Raytracer::reflect(const Vec3& v, const Vec3& n) const
{
    return v - n * (Vec3::dot(v, n) * 2.0f);
}

Vec3 Raytracer::refract(const Vec3& uv, const Vec3& n, float etai_over_etat) const
{
    // snell's law.  shirley's implementation/naming
    float cos_theta = std::min(Vec3::dot(uv * -1.0f, n), 1.0f);
    Vec3 r_out_perp = (uv + n * cos_theta) * etai_over_etat;
    Vec3 r_out_parallel = n * (-std::sqrt(std::abs(1.0f - Vec3::dot(r_out_perp, r_out_perp))));
    return r_out_perp + r_out_parallel;
}

float Raytracer::reflectance(float cosine, float ior_ratio) const {
    // Schlick's approximation for fresnel reflectance
    // ior_ratio = n1/n2 (eg air/glass = 1.0/1.5)
    float r0 = (1.0f - ior_ratio) / (1.0f + ior_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * std::pow((1.0f - cosine), 5.0f);
}


} // namespace okaytracer
} // namespace ollygon
