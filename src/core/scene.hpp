#pragma once

#include "core/vec3.hpp"
#include "core/colour.hpp"
#include <memory>
#include <vector>
#include <string>

namespace ollygon {

struct Transform {
    Vec3 position;
    Vec3 rotation; //euler angles
    Vec3 scale;

    Transform() : position(0,0,0), rotation(0,0,0), scale(1,1,1) {}
};

struct Sphere {
    float radius;
    Colour albedo;

    Sphere(float _radius = 1.0f) : radius(_radius), albedo(0.7f,0.7f,0.7f) {}
};

class SceneNode {
    public:
        std::string name;
        Transform transform;
        std::unique_ptr<Sphere> sphere; // TEMP, just spheres to test with
        std::vector<std::unique_ptr<SceneNode>> children;

        SceneNode(const std::string& name = "Node") : name(name) {}
};

// editor-facing scene
class Scene {
    public:
        Scene() {
            root = std::make_unique<SceneNode>("Root");
        }

        SceneNode* get_root() {return root.get();}
        const SceneNode* get_root() const {return root.get();}

    private:
        std::unique_ptr<SceneNode> root;
};



} // namespace ollygon
