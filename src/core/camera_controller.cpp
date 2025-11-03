#include "camera_controller.hpp"
#include <cmath>
#include <algorithm>

namespace ollygon {

CameraController::CameraController()
    : current_mode(CameraMode::Orbit)
    , target(2.775f, 2.775f, -2.775f)  // centre of cornell box default
    , distance(8.0f)
    , yaw(0.0f)
    , pitch(0.0f)
    , position(0, 4, 5)
    , forward(0, 0, -1)
    , right(1, 0, 0)
    , up(0, 1, 0)
{
    update_position_from_angles();
}

void CameraController::orbit(float delta_yaw, float delta_pitch) {
    yaw += delta_yaw;
    pitch += delta_pitch;
    
    pitch = std::clamp(pitch, -89.0f, 89.0f); //avoids gimbal lock/going "over the top"
    
    update_position_from_angles();
}

void CameraController::zoom(float delta) {
    distance -= delta;
    distance = std::clamp(distance, 0.5f, 50.0f);
    
    update_position_from_angles();
}

void CameraController::pan(float delta_x, float delta_y) {
    Vec3 to_camera = (position - target).normalised();
    Vec3 right = Vec3::cross(Vec3(0, 1, 0), to_camera).normalised();
    Vec3 up = Vec3::cross(to_camera, right);

    float pan_speed = distance * 0.001f; // this feels about right
    
    target = target + right * (delta_x * pan_speed);
    target = target + up * (delta_y * pan_speed);
    
    update_position_from_angles();
}

void CameraController::move_forward(float amount) {
    position = position + forward * amount;
}

void CameraController::move_right(float amount) {
    position = position + right * amount;
}

void CameraController::move_up(float amount) {
    position = position + up * amount;
}

Vec3 CameraController::get_position() const {
    if (current_mode == CameraMode::Orbit) {
        return position;  // already calculated from angles
    } else {
        return position;
    }
}

void CameraController::update_position_from_angles() {
    // convert spherical coords to cartesian
    float yaw_rad = yaw * 3.14159f / 180.0f;
    float pitch_rad = pitch * 3.14159f / 180.0f;
    
    float x = distance * std::cos(pitch_rad) * std::sin(yaw_rad);
    float y = distance * std::sin(pitch_rad);
    float z = distance * std::cos(pitch_rad) * std::cos(yaw_rad);
    
    position = target + Vec3(x, y, z);
}

} // namespace ollygon
