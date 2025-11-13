#pragma once

#include <cmath>

namespace ollygon {

struct Colour {
    float r,g,b;

    Colour() : r(0), g(0), b(0) {}
    Colour(float _r, float _g, float _b) : r(_r), g(_g), b(_b) {}
    Colour(float v) : r(v), g(v), b(v) {}

    Colour operator+(const Colour& other) const {
        return Colour(r + other.r, g + other.g, b + other.b);
    }
    
    Colour operator-(const Colour& other) const {
        return Colour(r - other.r, g - other.g, b - other.b);
    }

    Colour operator*(const Colour& other) const {
        return Colour(r * other.r, g * other.g, b * other.b);
    }

    Colour operator/(const Colour& other) const {
        return Colour(r / other.r, g / other.g, b / other.b);
    }

    void clamp() {
        if (r < 0) r = 0; if (r > 1) r =1;
        if (g < 0) g = 0; if (g > 1) g =1;
        if (b < 0) b = 0; if (b > 1) b =1;
    }
};

} // namespace ollygon
