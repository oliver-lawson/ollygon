#pragma once

namespace ollygon {

struct Vec4 {

    float x,y,z,w;

    // == constructors ==
    Vec4() : x(0), y(0), z(0), w(1) {}
    Vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    Vec4(float n) : x(n), y(n), z(n), w(n) {}

};

} // namespace ollygon
