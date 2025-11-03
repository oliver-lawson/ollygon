#pragma once

#include "vec3.hpp"

namespace ollygon {

enum class CameraMode {
    Orbit,
    UE
};

class CameraController {
public:
    CameraController();
    
    // orbit mode: rotate around target
    void orbit(float delta_yaw, float delta_pitch);
    void zoom(float delta);
    void pan(float delta_x, float delta_y);
    
    // UE mode: move camera position
    void move_forward(float amount);
    void move_right(float amount);
    void move_up(float amount);
    
    // mode switching
    void set_mode(CameraMode mode) { current_mode = mode; }
    CameraMode get_mode() const { return current_mode; }
    
    // getters for camera
    Vec3 get_position() const;
    Vec3 get_target() const { return target; }
    Vec3 get_up() const { return Vec3(0, 1, 0); }
    
    void set_target(const Vec3& new_target) { target = new_target; }
    
private:
    void update_position_from_angles();
    
    CameraMode current_mode;
    
    // orbit
    Vec3 target;
    float distance;
    float yaw;
    float pitch;
    
    // UE
    Vec3 position;
    Vec3 forward;
    Vec3 right;
    Vec3 up;
};

} // namespace ollygon
