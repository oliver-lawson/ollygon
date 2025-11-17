#pragma once

#include "core/vec3.hpp"
#include "core/colour.hpp"
#include "core/geometry.hpp"
#include "core/material.hpp"
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

enum class NodeType {
    Empty,
    Mesh,
    Primitive,
    Light,
    Camera,
    NodeTypeCount // I doubt we'll need such counts, but it stops having to
                  // see git comma edits on unchanged lines, and better than
                  // ugly comma-prefixed list I think
};

enum class LightType {
    Point,
    Directional,
    Area,
    LightTypeCount
};

struct Light {
    LightType type;
    Colour colour;
    float intensity;

    //area light specific:
    bool is_area_light;

    Light()
        : type(LightType::Point)
        , colour(Colour(0.9f, 0.7f, 0.05f))
        , intensity(1.0f)
        , is_area_light(false)
    {}
};

class SceneNode {
public:
    std::string name;
    Transform transform;
    NodeType node_type;
    bool visible;
    bool locked;

    // will be set depending on node_type between:
    std::unique_ptr<Primitive> primitive;
    std::unique_ptr<Geo> geo;

    Material material;

    // TODO: std::unique_ptr<Geometry> geometry;

    std::unique_ptr<Light> light;

    //hierarchy
    std::vector<std::unique_ptr<SceneNode>> children;
    SceneNode* parent;

    explicit SceneNode(const std::string& _name = "Node")
        : name(_name)
        , node_type(NodeType::Empty)
        , parent(nullptr)
        , visible(true)
        , locked(false)
    {}

    //helper to add child & set parent pointer
    void add_child(std::unique_ptr<SceneNode> child) {
        child->parent = this;
        children.push_back(std::move(child));
    }

    // get WS pos (accounting for parent transforms)
    Vec3 get_world_position() const {
        Vec3 pos = transform.position;
        SceneNode* p = parent;
        while (p) {
            pos = pos + p->transform.position; // TEMP - do properly later
            p = p->parent;
        }
        return pos;
    }
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
