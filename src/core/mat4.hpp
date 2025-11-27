#pragma once

#include "vec3.hpp"
#include "vec4.hpp"

#include <cmath>
#include "constants.hpp"
#include <QMatrix4x4> // for converting out. unfortunately makes this not Qt-independent

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
        for (int i = 0; i < 16; i++) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static Mat4 translate(float x, float y, float z) {
        Mat4 mat;
        mat.m[12] = x;
        mat.m[13] = y;
        mat.m[14] = z;
        return mat;
    }

    // transform a point (applies translation)
    Vec3 transform_point(const Vec3& p) const {
        float x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
        float y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
        float z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
        float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];

        if (w != 1.0f && w != 0.0f) {
            return Vec3(x / w, y / w, z / w);
        }
        return Vec3(x, y, z);
    }

    // transform a direction (ignores translation)
    Vec3 transform_direction(const Vec3& d) const {
        float x = m[0] * d.x + m[4] * d.y + m[8] * d.z;
        float y = m[1] * d.x + m[5] * d.y + m[9] * d.z;
        float z = m[2] * d.x + m[6] * d.y + m[10] * d.z;
        return Vec3(x, y, z);
    }

    static Mat4 scale(float x, float y, float z) {
        Mat4 mat;
        mat.m[0] = x;
        mat.m[5] = y;
        mat.m[10] = z;
        return mat;
    }

    static Mat4 rotate_x(float angle_rad) {
        Mat4 mat;
        float c = std::cos(angle_rad);
        float s = std::sin(angle_rad);
        mat.m[5] = c;
        mat.m[6] = s;
        mat.m[9] = -s;
        mat.m[10] = c;
        return mat;
    }

    static Mat4 rotate_y(float angle_rad) {
        Mat4 mat;
        float c = std::cos(angle_rad);
        float s = std::sin(angle_rad);
        mat.m[0] = c;
        mat.m[2] = -s;
        mat.m[8] = s;
        mat.m[10] = c;
        return mat;
    }

    static Mat4 rotate_z(float angle_rad) {
        Mat4 mat;
        float c = std::cos(angle_rad);
        float s = std::sin(angle_rad);
        mat.m[0] = c;
        mat.m[1] = s;
        mat.m[4] = -s;
        mat.m[5] = c;
        return mat;
    }

    // create rotation matrix from euler angles in rads
    //  applies in order: Z, Y, X
    static Mat4 rotate_euler(float x_rad, float y_rad, float z_rad) {
        return rotate_z(z_rad) * rotate_y(y_rad) * rotate_x(x_rad);
    }

    // matrix inverse, affine transform only. specialised inversion - assumes a TRS
    Mat4 inverse() const {
        Mat4 inv;

        // extract scale from upper 3x3 (length of each column)
        float sx = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
        float sy = std::sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
        float sz = std::sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);

        // check for zero scale (avoid division by zero)
        if (sx < ALMOST_ZERO) sx = 1.0f;
        if (sy < ALMOST_ZERO) sy = 1.0f;
        if (sz < ALMOST_ZERO) sz = 1.0f;

        // inverse scale
        float inv_sx = 1.0f / sx;
        float inv_sy = 1.0f / sy;
        float inv_sz = 1.0f / sz;

        // transpose of normalised rotation, then apply inverse scale
        inv.m[0] = (m[0] / sx) * inv_sx;
        inv.m[1] = (m[4] / sy) * inv_sx;
        inv.m[2] = (m[8] / sz) * inv_sx;
        inv.m[3] = 0;

        inv.m[4] = (m[1] / sx) * inv_sy;
        inv.m[5] = (m[5] / sy) * inv_sy;
        inv.m[6] = (m[9] / sz) * inv_sy;
        inv.m[7] = 0;

        inv.m[8] = (m[2] / sx) * inv_sz;
        inv.m[9] = (m[6] / sy) * inv_sz;
        inv.m[10] = (m[10] / sz) * inv_sz;
        inv.m[11] = 0;

        // inverse translation
        inv.m[12] = -(inv.m[0] * m[12] + inv.m[4] * m[13] + inv.m[8] * m[14]);
        inv.m[13] = -(inv.m[1] * m[12] + inv.m[5] * m[13] + inv.m[9] * m[14]);
        inv.m[14] = -(inv.m[2] * m[12] + inv.m[6] * m[13] + inv.m[10] * m[14]);
        inv.m[15] = 1;

        return inv;
    }

    Mat4 inverse_general_row_major() const
    {
        // standard cofactor expansion method for 4x4 matrix inversion
        // row-major layout: m[row * 4 + col]

        // calculate cofactors for first row to get determinant
        float c00 = m[5] * (m[10] * m[15] - m[11] * m[14]) -
            m[6] * (m[9] * m[15] - m[11] * m[13]) +
            m[7] * (m[9] * m[14] - m[10] * m[13]);

        float c01 = m[4] * (m[10] * m[15] - m[11] * m[14]) -
            m[6] * (m[8] * m[15] - m[11] * m[12]) +
            m[7] * (m[8] * m[14] - m[10] * m[12]);

        float c02 = m[4] * (m[9] * m[15] - m[11] * m[13]) -
            m[5] * (m[8] * m[15] - m[11] * m[12]) +
            m[7] * (m[8] * m[13] - m[9] * m[12]);

        float c03 = m[4] * (m[9] * m[14] - m[10] * m[13]) -
            m[5] * (m[8] * m[14] - m[10] * m[12]) +
            m[6] * (m[8] * m[13] - m[9] * m[12]);

        float det = m[0] * c00 - m[1] * c01 + m[2] * c02 - m[3] * c03;

        // check for singular matrix
        if (det == 0.0f) [[unlikely]] {
            return Mat4{};//identity
        }

        float inv_det = 1.0f / det;

        Mat4 result;

        // calculate all cofactors and build adjugate matrix, then scale by inverse determinant
        result.m[0] = c00 * inv_det;
        result.m[1] = -(m[1] * (m[10] * m[15] - m[11] * m[14]) -
            m[2] * (m[9] * m[15] - m[11] * m[13]) +
            m[3] * (m[9] * m[14] - m[10] * m[13])) * inv_det;
        result.m[2] = (m[1] * (m[6] * m[15] - m[7] * m[14]) -
            m[2] * (m[5] * m[15] - m[7] * m[13]) +
            m[3] * (m[5] * m[14] - m[6] * m[13])) * inv_det;
        result.m[3] = -(m[1] * (m[6] * m[11] - m[7] * m[10]) -
            m[2] * (m[5] * m[11] - m[7] * m[9]) +
            m[3] * (m[5] * m[10] - m[6] * m[9])) * inv_det;

        result.m[4] = -c01 * inv_det;
        result.m[5] = (m[0] * (m[10] * m[15] - m[11] * m[14]) -
            m[2] * (m[8] * m[15] - m[11] * m[12]) +
            m[3] * (m[8] * m[14] - m[10] * m[12])) * inv_det;
        result.m[6] = -(m[0] * (m[6] * m[15] - m[7] * m[14]) -
            m[2] * (m[4] * m[15] - m[7] * m[12]) +
            m[3] * (m[4] * m[14] - m[6] * m[12])) * inv_det;
        result.m[7] = (m[0] * (m[6] * m[11] - m[7] * m[10]) -
            m[2] * (m[4] * m[11] - m[7] * m[8]) +
            m[3] * (m[4] * m[10] - m[6] * m[8])) * inv_det;

        result.m[8] = c02 * inv_det;
        result.m[9] = -(m[0] * (m[9] * m[15] - m[11] * m[13]) -
            m[1] * (m[8] * m[15] - m[11] * m[12]) +
            m[3] * (m[8] * m[13] - m[9] * m[12])) * inv_det;
        result.m[10] = (m[0] * (m[5] * m[15] - m[7] * m[13]) -
            m[1] * (m[4] * m[15] - m[7] * m[12]) +
            m[3] * (m[4] * m[13] - m[5] * m[12])) * inv_det;
        result.m[11] = -(m[0] * (m[5] * m[11] - m[7] * m[9]) -
            m[1] * (m[4] * m[11] - m[7] * m[8]) +
            m[3] * (m[4] * m[9] - m[5] * m[8])) * inv_det;

        result.m[12] = -c03 * inv_det;
        result.m[13] = (m[0] * (m[9] * m[14] - m[10] * m[13]) -
            m[1] * (m[8] * m[14] - m[10] * m[12]) +
            m[2] * (m[8] * m[13] - m[9] * m[12])) * inv_det;
        result.m[14] = -(m[0] * (m[5] * m[14] - m[6] * m[13]) -
            m[1] * (m[4] * m[14] - m[6] * m[12]) +
            m[2] * (m[4] * m[13] - m[5] * m[12])) * inv_det;
        result.m[15] = (m[0] * (m[5] * m[10] - m[6] * m[9]) -
            m[1] * (m[4] * m[10] - m[6] * m[8]) +
            m[2] * (m[4] * m[9] - m[5] * m[8])) * inv_det;

        return result;
    }

    Mat4 inverse_general_column_major() const
    {
        // standard cofactor expansion method for 4x4 matrix inversion
        // column-major layout: m[col * 4 + row]

        // calculate cofactors for first row to get determinant
        float c00 = m[5] * (m[10] * m[15] - m[11] * m[14]) -
            m[9] * (m[6] * m[15] - m[7] * m[14]) +
            m[13] * (m[6] * m[11] - m[7] * m[10]);

        float c01 = m[4] * (m[10] * m[15] - m[11] * m[14]) -
            m[8] * (m[6] * m[15] - m[7] * m[14]) +
            m[12] * (m[6] * m[11] - m[7] * m[10]);

        float c02 = m[4] * (m[9] * m[15] - m[11] * m[13]) -
            m[8] * (m[5] * m[15] - m[7] * m[13]) +
            m[12] * (m[5] * m[11] - m[7] * m[9]);

        float c03 = m[4] * (m[9] * m[14] - m[10] * m[13]) -
            m[8] * (m[5] * m[14] - m[6] * m[13]) +
            m[12] * (m[5] * m[10] - m[6] * m[9]);

        float det = m[0] * c00 - m[1] * c01 + m[2] * c02 - m[3] * c03;

        // check for singular matrix
        if (det == 0.0f) [[unlikely]] {
            return Mat4{};//identity
        }

        float inv_det = 1.0f / det;

        Mat4 result;

        // calculate all cofactors and build adjugate matrix, then scale by inverse determinant
        result.m[0] = c00 * inv_det;
        result.m[1] = -c01 * inv_det;
        result.m[2] = c02 * inv_det;
        result.m[3] = -c03 * inv_det;

        result.m[4] = -(m[1] * (m[10] * m[15] - m[11] * m[14]) -
            m[9] * (m[2] * m[15] - m[3] * m[14]) +
            m[13] * (m[2] * m[11] - m[3] * m[10])) * inv_det;
        result.m[5] = (m[0] * (m[10] * m[15] - m[11] * m[14]) -
            m[8] * (m[2] * m[15] - m[3] * m[14]) +
            m[12] * (m[2] * m[11] - m[3] * m[10])) * inv_det;
        result.m[6] = -(m[0] * (m[9] * m[15] - m[11] * m[13]) -
            m[8] * (m[1] * m[15] - m[3] * m[13]) +
            m[12] * (m[1] * m[11] - m[3] * m[9])) * inv_det;
        result.m[7] = (m[0] * (m[9] * m[14] - m[10] * m[13]) -
            m[8] * (m[1] * m[14] - m[2] * m[13]) +
            m[12] * (m[1] * m[10] - m[2] * m[9])) * inv_det;

        result.m[8] = (m[1] * (m[6] * m[15] - m[7] * m[14]) -
            m[5] * (m[2] * m[15] - m[3] * m[14]) +
            m[13] * (m[2] * m[7] - m[3] * m[6])) * inv_det;
        result.m[9] = -(m[0] * (m[6] * m[15] - m[7] * m[14]) -
            m[4] * (m[2] * m[15] - m[3] * m[14]) +
            m[12] * (m[2] * m[7] - m[3] * m[6])) * inv_det;
        result.m[10] = (m[0] * (m[5] * m[15] - m[7] * m[13]) -
            m[4] * (m[1] * m[15] - m[3] * m[13]) +
            m[12] * (m[1] * m[7] - m[3] * m[5])) * inv_det;
        result.m[11] = -(m[0] * (m[5] * m[14] - m[6] * m[13]) -
            m[4] * (m[1] * m[14] - m[2] * m[13]) +
            m[12] * (m[1] * m[6] - m[2] * m[5])) * inv_det;

        result.m[12] = -(m[1] * (m[6] * m[11] - m[7] * m[10]) -
            m[5] * (m[2] * m[11] - m[3] * m[10]) +
            m[9] * (m[2] * m[7] - m[3] * m[6])) * inv_det;
        result.m[13] = (m[0] * (m[6] * m[11] - m[7] * m[10]) -
            m[4] * (m[2] * m[11] - m[3] * m[10]) +
            m[8] * (m[2] * m[7] - m[3] * m[6])) * inv_det;
        result.m[14] = -(m[0] * (m[5] * m[11] - m[7] * m[9]) -
            m[4] * (m[1] * m[11] - m[3] * m[9]) +
            m[8] * (m[1] * m[7] - m[3] * m[5])) * inv_det;
        result.m[15] = (m[0] * (m[5] * m[10] - m[6] * m[9]) -
            m[4] * (m[1] * m[10] - m[2] * m[9]) +
            m[8] * (m[1] * m[6] - m[2] * m[5])) * inv_det;

        return result;
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

    // == coord conversion helpers ==

    // for eg ollygon z-up -> openGL y-up
    static Mat4 swizzle_z_up_and_y_up() {
        Mat4 mat;
        // swap y and z
        mat.m[5] = 0; mat.m[6] = 1;  //y row
        mat.m[9] = 1; mat.m[10] = 0; //z row
        return mat;
    }

    float* floats_row_major() const {
        static float row_major[16];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                row_major[i * 4 + j] = m[j * 4 + i];  // transpose
            }
        }
        return row_major;
    }

    QMatrix4x4 to_qmatrix() const {
        return QMatrix4x4(
            m[0], m[4], m[8], m[12],
            m[1], m[5], m[9], m[13],
            m[2], m[6], m[10], m[14],
            m[3], m[7], m[11], m[15]
        );
    }

    QMatrix4x4 to_qmatrix_column_major() const {
        // keeps our column-major mat, though QMatrix4x4 internally is row-major so not sure what this may be useful for yet
        return QMatrix4x4(
            m[0], m[1], m[2], m[3],
            m[4], m[5], m[6], m[7],
            m[8], m[9], m[10], m[11],
            m[12], m[13], m[14], m[15]
        );
    }

    const float* floats() const { return m; }

    // == vec4 ops ==

    Vec4 operator*(const Vec4& v) const {
        return Vec4(
            m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
            m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w
        );
    }

};
} // namespace ollygon
