#pragma once

#include "colour.hpp"

namespace ollygon {

enum class MaterialType {
    Lambertian,
    Metal,
    Dielectric,
    Emissive,
    Chequerboard,
    MaterialTypeCount
};

struct Material {
    MaterialType type;

    Colour albedo;
    Colour emission;
    float roughness;
    float ior;

    //TEMP chequerrboard specific
    Colour chequerboard_colour_a;
    Colour chequerboard_colour_b;
    Colour chequerboard_scale;

    //openGL rendering hints
    float metallic;
    float specular;

    Material()
        : type(MaterialType::Lambertian)
        , albedo(0.7f, 0.7f, 0.7f)
        , emission(0, 0, 0)
        , roughness(0.0f)
        , ior(1.5f)
        , chequerboard_colour_a(1, 1, 1)
        , chequerboard_colour_b(0.2f, 0.2f, 0.2f)
        , chequerboard_scale(1.0f)
        , metallic(0.0f)
        , specular(0.0f)
    {}

    //TEMP factory methods for common mats
    static Material lambertian(const Colour& _albedo) {
        Material mat;
        mat.type = MaterialType::Lambertian;
        mat.albedo = _albedo;
        mat.metallic = 0.0f;
        mat.specular = 0.2f;
    }

    static Material metal(const Colour& albedo, float roughness = 0.0f) {
        Material mat;
        mat.type = MaterialType::Metal;
        mat.albedo = albedo;
        mat.roughness = roughness;
        mat.metallic = 1.0f;
        mat.specular = 1.0f;
        return mat;
    }

    static Material dielectric(float ior) {
        Material mat;
        mat.type = MaterialType::Dielectric;
        mat.ior = ior;
        mat.albedo = Colour(1, 1, 1);
        mat.metallic = 0.0f;
        mat.specular = 1.0f;
        return mat;
    }

    static Material emissive(const Colour& emission) {
        Material mat;
        mat.type = MaterialType::Emissive;
        mat.emission = emission;
        mat.albedo = emission;
        mat.metallic = 0.0f;
        mat.specular = 0.0f;
        return mat;
    }

    static Material chequerboard(const Colour& colour_a, const Colour& colour_b, float scale = 1.0f) {
        Material mat;
        mat.type = MaterialType::Chequerboard;
        mat.chequerboard_colour_a = colour_a;
        mat.chequerboard_colour_b = colour_b;
        mat.chequerboard_scale = scale;
        mat.metallic = 0.0f;
        mat.specular = 0.2f;
        return mat;
    }
};

} // namespace ollygon
