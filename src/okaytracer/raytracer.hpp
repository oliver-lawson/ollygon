#pragma once

#include "ray.hpp"
#include "render_scene.hpp"
#include "../core/camera.hpp"
#include <vector>
#include <cstdint>
#include <random>
#include <thread>
#include <mutex>


namespace ollygon {
namespace okaytracer {

struct RenderConfig {
    int width;
    int height;
    int samples_per_pixel;
    int max_bounces;
    uint64_t seed;

    RenderConfig()
        : width(600)
        , height(600)
        , samples_per_pixel(1000)
        , max_bounces(7)
        , seed(1)
    {}
};

struct CameraBasis {
    Vec3 viewport_upper_left;
    Vec3 pixel_delta_u;
    Vec3 pixel_delta_v;
    Vec3 camera_pos;
};

class Raytracer {
public:
    Raytracer();
    ~Raytracer();

    void start_render(const RenderScene& scene, const Camera& camera, const RenderConfig& new_config);

    void stop_render();
    bool is_rendering() const { return rendering; }
    float get_progress() const;

    const std::vector<float>& get_pixels() const { return pixels; }
    int get_width() const { return config.width; }
    int get_height() const { return config.height; }

    void render_one_sample();

    void render_tile(int start_x, int end_x, int start_y, int end_y, const CameraBasis& basis);

    uint64_t hash_pixel(int x, int y, uint64_t seed) const;

private:
    CameraBasis compute_camera_basis() const;

    bool intersect(const Ray& ray, float t_min, float t_max, Intersection& rec) const;
    bool intersect_sphere(const RenderPrimitive& prim, const Ray& ray, float t_min, float t_max, Intersection& rec) const;
    bool intersect_quad(const RenderPrimitive& prim, const Ray& ray, float t_min, float t_max, Intersection& rec) const;
    bool intersect_triangle(const RenderPrimitive& prim, const Ray& ray, float t_min, float t_max, Intersection& rec) const;

    Colour ray_colour(const Ray& ray, int depth, uint64_t& rng) const;
    bool scatter(const Ray& ray_in, const Intersection& rec, Colour& attenuation, Ray& scattered, uint64_t& rng) const;

    // mat scattering funcs
    bool scatter_lambertian(const Ray& rain_in, const Intersection& rec, Colour& attenuation, Ray& scattered, uint64_t& rng) const;
    bool scatter_metal(const Ray& rain_in, const Intersection& rec, Colour& attenuation, Ray& scattered, uint64_t& rng) const;
    bool scatter_dielectric(const Ray& rain_in, const Intersection& rec, Colour& attenuation, Ray& scattered, uint64_t& rng) const;
    Colour get_chequerboard_colour(const Vec3& point, const Material& mat) const;

    Vec3 random_in_unit_sphere(uint64_t& rng) const;
    Vec3 random_unit_vector(uint64_t& rng) const;
    Vec3 reflect(const Vec3& v, const Vec3& n) const;
    Vec3 refract(const Vec3& v, const Vec3& n, float etai_over_etat) const;
    float reflectance(float cosine, float ref_idx) const;

    RenderScene scene;
    Camera camera;
    RenderConfig config;

    std::vector<float> pixels;
    std::vector<float> sample_buffer;

    bool rendering;
    int current_sample;

    float random_float(uint64_t& state) const {
        // from https://en.wikipedia.org/wiki/Xorshift with added state by reference to force thread-locality
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return float((state * 0x2545F4914F6CDD1DULL) >> 33) / float(1ULL << 31);
    }

    int num_threads = std::thread::hardware_concurrency();
};

} // namespace okaytracer
} // namespace ollygon
