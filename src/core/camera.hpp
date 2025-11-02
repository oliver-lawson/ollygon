#pragma once
#include "vec3.hpp"
#include "mat4.hpp"


namespace ollygon {

class Camera {
public:
    Camera()
        : position (0,4,5)
        , target (0,0,0)
        , up (0,1,0)
        , fov_y(45.0f)
        , aspect(4.0f / 3.0f)
        , near_plane(0.1f)
        , far_plane(100.0f)
    {}

    Vec3 get_pos() const {
        return position;
    }
    Vec3 get_target() const {
        return target;
    }
    Vec3 get_up() const {
        return up;
    }

    Mat4 get_view_matrix() const {
        return Mat4::look_at(position, target, up);
    }

    Mat4 get_projection_matrix() const {
        return Mat4::perspective(fov_y * 3.14159f / 180.0f, aspect, near_plane, far_plane);
    }

    void set_aspect(float new_aspect) { aspect = new_aspect; }

private:
    Vec3 position;
    Vec3 target;
    Vec3 up;

    float fov_y;
    float aspect;
    float near_plane;
    float far_plane;
};

} // namespace ollygon
