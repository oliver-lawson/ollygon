#include "raytracer.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

namespace ollygon {
namespace okaytracer {

Raytracer::Raytracer()
    : rendering(false)
    , current_sample(0)
    , rng(std::random_device{}())
    , dist(0.0f, 1.0f)
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

    // split image into horizontal tiles
    int tiles = num_threads;
    int rows_per_tile = config.height / tiles;

    std::vector<std::thread> threads;

    for (int tile = 0; tile < tiles; tile++) {
        int start_row = tile * rows_per_tile;
        int end_row = (tile == tiles - 1) ? config.height : start_row + rows_per_tile;

        threads.emplace_back([this, start_row, end_row]() {
            render_tile(start_row, end_row);
            });
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

void Raytracer::render_tile(int start_row, int end_row) {
    // ray generation + tracing loop here

    for (int j = start_row; j < end_row; j++) {
        for (int i = 0; i < config.width; i++) {
            float u = (float(i) + dist(rng)) / float(config.width - 1);
            float v = (float(j) + dist(rng)) / float(config.height- 1);

            //generate ray from camera
            Vec3 forward = (camera.get_target() - camera.get_pos()).normalised();
            Vec3 right = Vec3::cross(forward, camera.get_up()).normalised();
            Vec3 up = Vec3::cross(right, forward);

            float aspect = float(config.width) / float(config.height);
            float fov_rad = 45.0f * 3.1459f / 180.0f;
            float h = std::tan(fov_rad * 0.5f);
            float viewport_height = 2.0f * h;
            float viewport_width = viewport_height * aspect;

            Vec3 viewport_u = right * viewport_width;
            Vec3 viewport_v = up * viewport_height;

            Vec3 pixel_delta_u = viewport_u / float(config.width);
            Vec3 pixel_delta_v = viewport_v / float(config.height);

            Vec3 viewport_upper_left = camera.get_pos() + forward - viewport_u * 0.5f + viewport_v * 0.5f;

            Vec3 pixel_centre = viewport_upper_left + pixel_delta_u * (float(i) + 0.5f) - pixel_delta_v * (float(j) + 0.5f);

            Vec3 ray_dir = (pixel_centre - camera.get_pos()).normalised();
            Ray ray(camera.get_pos(), ray_dir);

            Colour pixel_colour = ray_colour(ray, config.max_bounces);

            int idx = (j * config.width + i) * 3;
            sample_buffer[idx + 0] = pixel_colour.r;
            sample_buffer[idx + 1] = pixel_colour.g;
            sample_buffer[idx + 2] = pixel_colour.b;
        }
    }
}

// == private methods ==
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
        case RenderPrimitive::Type::Cuboid:
            hit = intersect_cuboid(prim, ray, t_min, closest_so_far, temp_rec);
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

    if (std::abs(denom) < 1e-8f) return false;

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

bool Raytracer::intersect_cuboid( const RenderPrimitive& prim, const Ray& ray, float t_min, float t_max, Intersection& rec ) const
{
    Vec3 inv_dir = Vec3(
        std::abs(ray.direction.x) > 1e-6f ? 1.0f / ray.direction.x : 1e6f,
        std::abs(ray.direction.y) > 1e-6f ? 1.0f / ray.direction.y : 1e6f,
        std::abs(ray.direction.z) > 1e-6f ? 1.0f / ray.direction.z : 1e6f
    );

    float t1 = (prim.cuboid_min.x - ray.origin.x) * inv_dir.x;
    float t2 = (prim.cuboid_max.x - ray.origin.x) * inv_dir.x;
    float t3 = (prim.cuboid_min.y - ray.origin.y) * inv_dir.y;
    float t4 = (prim.cuboid_max.y - ray.origin.y) * inv_dir.y;
    float t5 = (prim.cuboid_min.z - ray.origin.z) * inv_dir.z;
    float t6 = (prim.cuboid_max.z - ray.origin.z) * inv_dir.z;

    float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
    float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

    if (tmax < 0.0f || tmin > tmax || tmin < t_min || tmin > t_max) {
        return false;
    }

    rec.t = tmin;
    rec.point = ray.at(rec.t);
    rec.material = prim.material;

    //determine face normal
    const float epsilon = 0.0001f;
    if (std::abs(tmin - t1) < epsilon) rec.set_face_normal(ray, Vec3(-1, 0, 0));
    else if (std::abs(tmin - t2) < epsilon) rec.set_face_normal(ray, Vec3(1, 0, 0));
    else if (std::abs(tmin - t3) < epsilon) rec.set_face_normal(ray, Vec3(0, -1, 0));
    else if (std::abs(tmin - t4) < epsilon) rec.set_face_normal(ray, Vec3(0, 1, 0));
    else if (std::abs(tmin - t5) < epsilon) rec.set_face_normal(ray, Vec3(0, 0, -1));
    else rec.set_face_normal(ray, Vec3(0, 0, 1));

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

    if (std::abs(a) < 1e-8f) {
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
Colour Raytracer::ray_colour(const Ray& ray, int depth) const
{
    if (depth <= 0) return Colour(0, 0, 0);

    Intersection rec;
    //TODO shirley-style "interval" here? maybe call it RayRange/t_range
    if (intersect(ray, 0.001f, std::numeric_limits<float>::infinity(), rec)) {
       
        if (rec.material.type == MaterialType::Emissive) {
            return rec.material.emission;
        }

        // russian roulette termination of rays.  on cornell box, about +11% perf
        if (depth < config.max_bounces - 2) {  // after a few bounces
            float p = std::max(rec.material.albedo.r,
                std::max(rec.material.albedo.g, rec.material.albedo.b));
            if (dist(rng) > p) {
                return Colour(0, 0, 0);  // terminate early
            }
            // boost surviving rays
            rec.material.albedo = rec.material.albedo / p;
        }

        // scatter
        Ray scattered;
        Colour attenuation;

        if (scatter(ray, rec, attenuation, scattered)) {
            Colour bounced = ray_colour(scattered, depth - 1);
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
    Vec3 unit_dir = ray.direction.normalised();
    float t = 0.5f * (unit_dir.y + 1.0f);
    Colour white(1.0f, 1.0f, 1.0f);
    Colour blue(0.5f, 0.7f, 1.0f);

    return (white * (1.0f - t) + blue * t) * 0.05f;
}

bool Raytracer::scatter(const Ray& ray_in, const Intersection& rec, Colour& attenuation, Ray& scattered) const
{
    switch (rec.material.type)
    {
        case MaterialType::Lambertian:
            return scatter_lambertian(ray_in, rec, attenuation, scattered);
        case MaterialType::Metal:
            return scatter_metal(ray_in, rec, attenuation, scattered);
        case MaterialType::Dielectric:
            return scatter_dielectric(ray_in, rec, attenuation, scattered);
        case MaterialType::Chequerboard:
            attenuation = get_chequerboard_colour(rec.point, rec.material);
            scattered = Ray(rec.point, rec.normal + random_unit_vector());
            return true;
        default:
            return false;
    }
}

bool Raytracer::scatter_lambertian(const Ray& ray_in, const Intersection& rec, Colour& attenuation, Ray& scattered) const
{
    Vec3 scatter_dir = rec.normal + random_unit_vector();

    // catch degenerate scatter direction
    if (std::abs(scatter_dir.x) < 1e-8f &&
        std::abs(scatter_dir.y) < 1e-8f &&
        std::abs(scatter_dir.z) < 1e-8f) {
        scatter_dir = rec.normal;
    }

    scattered = Ray(rec.point, scatter_dir.normalised());
    attenuation = rec.material.albedo;

    return true;
}

bool Raytracer::scatter_metal(const Ray& ray_in, const Intersection& rec, Colour& attenuation, Ray& scattered) const
{
    Vec3 reflected = reflect(ray_in.direction.normalised(), rec.normal);

    //TEMP adding roughness by perturbing reflection 
    //TODO: read pbrt microfacet roughness chapter
    Vec3 fuzz = random_unit_vector() * rec.material.roughness;
    scattered = Ray(rec.point, (reflected + fuzz).normalised());
    attenuation = rec.material.albedo;

    return Vec3::dot(scattered.direction, rec.normal) > 0;
}

bool Raytracer::scatter_dielectric(const Ray& ray_in, const Intersection& rec, Colour& attenuation, Ray& scattered) const
{
    attenuation = Colour(1, 1, 1);
    float refraction_ratio = rec.front_face ? (1.0f / rec.material.ior) : rec.material.ior;

    Vec3 unit_dir = ray_in.direction.normalised();
    float cos_theta = std::min(Vec3::dot(unit_dir * -1.0f, rec.normal), 1.0f);
    float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

    bool cannot_refract = refraction_ratio * sin_theta > 1.0f;
    Vec3 direction;

    // schlick approx
    if (cannot_refract || reflectance(cos_theta, refraction_ratio) > dist(rng)) {
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

Vec3 Raytracer::random_in_unit_sphere() const
{
    while (true) {
        Vec3 p(
            dist(rng) * 2.0f - 1.0f,
            dist(rng) * 2.0f - 1.0f,
            dist(rng) * 2.0f - 1.0f
        );
        if (Vec3::dot(p, p) < 1.0f) {
            return p;
        }
    }
}

Vec3 Raytracer::random_unit_vector() const
{
    return random_in_unit_sphere().normalised();
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
