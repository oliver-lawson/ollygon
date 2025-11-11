#pragma once

#include "../core/vec3.hpp"
#include "../core/colour.hpp"
#include "../core/material.hpp"

namespace ollygon {
namespace okaytracer {

struct Ray {
    Vec3 origin;
    Vec3 direction;

    Ray() : origin(0, 0, 0), direction(0, 0, 1) {}
    Ray(const Vec3& o, const Vec3& d) : origin(o), direction(d) {}

    Vec3 at(float t) const {
        return origin + direction * t;
    }
};

struct Intersection { // bit better naming than HitRecord I think
    Vec3 point;
    Vec3 normal;
    float t; //distance, as in t used in lerps. convention in pbrt/shirley
    bool front_face;

    Material material;

    Intersection() : t(0), front_face(true) {}

    // sets normal to always point against ray
    void set_face_normal(const Ray& ray, const Vec3& outward_normal) {
        front_face = Vec3::dot(ray.direction, outward_normal) < 0;
        normal = front_face ? outward_normal : outward_normal * -1.0f;
    }
};

} // namespace okaytracer
} // namespace ollygon
