#pragma once

#include "vec3.hpp"
#include <cmath>

namespace ollygon {

// column-major 4x4 matrix
struct Mat4 {

    float m[16];

    // constructors
    Mat4() {
        identity();
    }

    // operators

    // functions
    void identity() {
        for (int i=0;i<16;i++) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static Mat4 translate(float x, float y, float z) {
        Mat4 mat;
        mat.m[12] = x;
        mat.m[13] = y;
        mat.m[14] = z;
        return mat;
    }

    static Mat4 scale(float x, float y, float z) {
        Mat4 mat;
        mat.m[0] = x;
        mat.m[5] = y;
        mat.m[10] = z;
        return mat;
    }
    
    static Mat4 perspective(float fov_y_rad, float aspect, float near, float far) {
        Mat4 mat;
        for (int i = 0; i < 16; ++i) mat.m[i] = 0.0f;
        
        float tan_half_fov = std::tan(fov_y_rad / 2.0f);
        mat.m[0] = 1.0f / (aspect * tan_half_fov);
        mat.m[5] = 1.0f / tan_half_fov;
        mat.m[10] = -(far + near) / (far - near);
        mat.m[11] = -1.0f;
        mat.m[14] = -(2.0f * far * near) / (far - near);
        
        return mat;
    }

    static Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = (target - eye).normalised();
        Vec3 s = Vec3::cross(f, up).normalised();
        Vec3 u = Vec3::cross(s, f);

        Mat4 mat;
        mat.m[0] = s.x;
        mat.m[4] = s.y;
        mat.m[8] = s.z;
        mat.m[1] = u.x;
        mat.m[5] = u.y;
        mat.m[9] = u.z;
        mat.m[2] = -f.x;
        mat.m[6] = -f.y;
        mat.m[10] = -f.z;
        mat.m[12] = -Vec3::dot(s, eye);
        mat.m[13] = -Vec3::dot(u, eye);
        mat.m[14] = Vec3::dot(f, eye);

        return mat;
    }
    
    Mat4 operator*(const Mat4& other) const {
        Mat4 result;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                result.m[col * 4 + row] = 0;
                for (int k = 0; k < 4; ++k) {
                    result.m[col * 4 + row] += m[k * 4 + row] * other.m[col * 4 + k];
                }
            }
        }
        return result;
    }

    const float* floats() const { return m; }
};

} // namespace ollygon