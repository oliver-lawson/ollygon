#pragma once

#include <cmath>

namespace ollygon {

struct Vec3 {

    float x,y,z;

    // == constructors ==
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    Vec3(float n) : x(n), y(n), z(n) {}

    // == operators ==
    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    Vec3 operator*(const Vec3& other) const {
        return Vec3(x * other.x, y * other.y, z * other.z);
    }

    Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }

    Vec3 operator/(float scalar) const {
        return Vec3(x / scalar, y / scalar, z / scalar);
    }

    // == compounds ==
    Vec3& operator+=(float n) {
        x += n; y += n; z += n;
        return *this;
    }

    Vec3& operator-=(float n) {
        x -= n; y -= n; z -= n;
        return *this;
    }

    Vec3& operator*=(float n) {
        x *= n; y *= n; z *= n;
        return *this;
    }

    Vec3& operator/=(float n){
        x /= n; y /= n; z /= n;
        return *this; 
    }

    // == unaries ==
    Vec3 operator-() const {
        return Vec3(-x, -y, -z);
    }

    // == functions ==
    float length() const {
        return std::sqrt(x*x + y*y + z*z);
    }

    Vec3 normalised() const {
        float len = length();
        return len > 0 ? (*this) / len : Vec3(0,0,0);
    }

    static float dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return Vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }
};


} // namespace ollygon
