#pragma once
#include "vec3.hpp"
#include "mat4.hpp"
#include "camera_controller.hpp"

namespace ollygon {

class Camera {
public:
    // defaults.  transform is handled by its scene transform externally.
    Camera()
        : fov_y(40.0f)
        , aspect(4.0f / 3.0f)
        , near_plane(0.1f)
        , far_plane(100.0f)
    {}

    Vec3 get_pos() const {
        return controller.get_position();
    }

    Vec3 get_target() const {
        return controller.get_target();
    }

    Vec3 get_up() const {
        return controller.get_up();
    }

    Mat4 get_view_matrix() const {
        return Mat4::look_at(get_pos(), get_target(), get_up());
    }

    Mat4 get_projection_matrix() const {
        return Mat4::perspective(fov_y * DEG_TO_RAD, aspect, near_plane, far_plane);
    }

    void set_aspect(float new_aspect) { aspect = new_aspect; }

    float get_fov_degs() const { return fov_y; }
    void set_fov_degs(float new_fov) { fov_y = new_fov; }

    CameraController* get_controller() { return &controller; }
    const CameraController* get_controller() const { return &controller; }

private:
    CameraController controller;

    float fov_y;
    float aspect;
    float near_plane;
    float far_plane;
};

} // namespace ollygon