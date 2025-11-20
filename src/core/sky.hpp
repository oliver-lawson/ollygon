#pragma once

#include "colour.hpp"
#include "vec3.hpp"
#include <QJsonObject>

namespace ollygon {

// forward decs
class Scene;

////
//skies

// simple two colour gradient sky
struct Sky {
    Colour colour_bottom;
    Colour colour_top;
    float bottom_height; // these are normalised [0,1]
    float top_height;

    Sky() //default
        : colour_bottom(0.5f, 0.7f, 1.0f)  // light blue
        , colour_top(0.05f, 0.05f, 0.2f)   // dark blue
        , bottom_height(0.0f)
        , top_height(1.0f)
    {}

    // sample sky colour given a direction vector
    Colour sample(const Vec3& direction) const {
        //remap and clamp t from [-1,1] to [0,1]
        float t = (direction.z + 1.0f) * 0.5f;
        
        if (t <= bottom_height) {
            return colour_bottom;
        }
        if (t >= top_height) {
            return colour_top;
        }

        float range = top_height - bottom_height;
        float blend = (t - bottom_height) / range;

        return Colour(
            colour_bottom.r * (1.0f - blend) + colour_top.r * blend,
            colour_bottom.g * (1.0f - blend) + colour_top.g * blend,
            colour_bottom.b * (1.0f - blend) + colour_top.b * blend
        );
    }


    // factory methods for sky presets
    static Sky default_sky() {
        Sky sky;
        sky.colour_bottom = Colour(0.5f, 0.7f, 1.0f);
        sky.colour_top = Colour(0.05f, 0.05f, 0.2f);
        sky.bottom_height = 0.0f;
        sky.top_height = 1.0f;
        return sky;
    }

    static Sky cornell_dark() {
        Sky sky;
        sky.colour_bottom = Colour(0.05f, 0.09f, 0.25f);
        sky.colour_top = Colour(0.005f, 0.005f, 0.005f);
        sky.bottom_height = 0.0f;
        sky.top_height = 1.0f;
        return sky;
    }

    static Sky sunset() {
        Sky sky;
        sky.colour_bottom = Colour(1.0f, 0.5f, 0.2f);
        sky.colour_top = Colour(0.2f, 0.1f, 0.5f);
        sky.bottom_height = 0.0f;
        sky.top_height = 0.7f;
        return sky;
    }
};

} // namespace ollygon
