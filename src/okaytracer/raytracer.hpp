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

    RenderConfig()
        : width(600)
        , height(600)
        , samples_per_pixel(10)
        , max_bounces(8)
    {
    }
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

    void render_tile(int start_row, int end_row);

private:
    bool intersect(const Ray& ray, float t_min, float t_max, Intersection& rec) const;
    bool intersect_sphere(const RenderPrimitive& prim, const Ray& ray, float t_min, float t_max, Intersection& rec) const;
    bool intersect_quad(const RenderPrimitive& prim, const Ray& ray, float t_min, float t_max, Intersection& rec) const;
    bool intersect_cuboid(const RenderPrimitive& prim, const Ray& ray, float t_min, float t_max, Intersection& rec) const;
    bool intersect_triangle(const RenderPrimitive& prim, const Ray& ray, float t_min, float t_max, Intersection& rec) const;

    Colour ray_colour(const Ray& ray, int depth) const;
    bool scatter(const Ray& ray_in, const Intersection& rec, Colour& attenuation, Ray& scattered) const;

    // mat scattering funcs
    bool scatter_lambertian(const Ray& rain_in, const Intersection& rec, Colour& attenuation, Ray& scattered) const;
    bool scatter_metal(const Ray& rain_in, const Intersection& rec, Colour& attenuation, Ray& scattered) const;
    bool scatter_dielectric(const Ray& rain_in, const Intersection& rec, Colour& attenuation, Ray& scattered) const;
    Colour get_chequerboard_colour(const Vec3& point, const Material& mat) const;

    Vec3 random_in_unit_sphere() const;
    Vec3 random_unit_vector() const;
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

    mutable std::mt19937 rng; //TODO might this be too slow?
    mutable std::uniform_real_distribution<float> dist;

    int num_threads = std::thread::hardware_concurrency();
};

} // namespace okaytracer
} // namespace ollygon
