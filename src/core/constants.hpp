#pragma once

namespace ollygon {

// coordinate System: Z-up, right-handed:
// +X = right
// +Y = forward
// +Z = up
// rotation: right-hand rule around axes:
// front-facing tris are anticlockwise winding when viewed from outside

constexpr float ALMOST_ZERO = 1e-8f;
constexpr float PI = 3.14159265359f;
constexpr float DEG_TO_RAD = PI / 180.0f;

} // namespace ollygon
