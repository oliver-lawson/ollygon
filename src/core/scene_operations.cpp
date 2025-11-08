#include "scene_operations.hpp"
#include <algorithm>

namespace ollygon {

bool SceneOperations::delete_node(Scene* scene, SceneNode* node) {
    if (!scene || !node) return false;

    if (node == scene->get_root()) return false;

    return remove_node_recursive(scene->get_root(), node);
}

bool SceneOperations::remove_node_recursive(SceneNode* parent, SceneNode* target) {
    // check any direct children first
    auto& children = parent->children;
    auto it = std::find_if(children.begin(), children.end(),
        [target](const std::unique_ptr<SceneNode>& child) {
            return child.get() == target;
        });
    
    if (it != children.end()) {
        children.erase(it);
        return true;
    }

    // recurse into children
    for (auto& child : children) {
        if (remove_node_recursive(child.get(), target)) {
            return true;
        }
    }
    return false;
}

// == create stuff ==

std::unique_ptr<SceneNode> SceneOperations::create_sphere(const std::string& name) {
    auto node = std::make_unique<SceneNode>(name);
    node->node_type = NodeType::Primitive;
    node->primitive = std::make_unique<SpherePrimitive>(0.5f);
    node->albedo = Colour(0.7f, 0.7f, 0.7f);
    return node;
}

std::unique_ptr<SceneNode> SceneOperations::create_cuboid(const std::string& name) {
    auto node = std::make_unique<SceneNode>(name);
    node->node_type = NodeType::Primitive;
    node->primitive = std::make_unique<CuboidPrimitive>(Vec3(1.0f, 1.0f, 1.0f));
    node->albedo = Colour(0.7f, 0.7f, 0.7f);
    return node;
}

std::unique_ptr<SceneNode> SceneOperations::create_quad(const std::string& name) {
    auto node = std::make_unique<SceneNode>(name);
    node->node_type = NodeType::Primitive;
    node->primitive = std::make_unique<QuadPrimitive>(
        Vec3(0.5f, 0, 0),
        Vec3(0, 0.5f, 0)
    );
    node->albedo = Colour(0.7f, 0.7f, 0.7f);
    return node;
}

std::unique_ptr<SceneNode> SceneOperations::create_empty(const std::string& name) {
    auto node = std::make_unique<SceneNode>(name);
    node->node_type = NodeType::Empty;
    node->albedo = Colour(0.7f, 0.7f, 0.7f);
    return node;
}

std::unique_ptr<SceneNode> SceneOperations::create_mesh(const std::string& name) {
    auto node = std::make_unique<SceneNode>(name);
    node->node_type = NodeType::Mesh;
    node->geo = std::make_unique<Geo>();
    node->albedo = Colour(0.7f, 0.7f, 0.7f);

    //empty mesh for now
    return node;
}

std::unique_ptr<SceneNode> SceneOperations::create_point_light(const std::string& name) {
    auto node = std::make_unique<SceneNode>(name);
    node->node_type = NodeType::Light;
    node->light = std::make_unique<Light>();
    node->light->type = LightType::Point;
    node->light->colour = Colour(1.0f, 1.0f, 1.0f);
    node->light->intensity = 10.0f;
    node->albedo = Colour(1.0f, 1.0f, 1.0f);
    return node;
}

std::unique_ptr<SceneNode> SceneOperations::create_area_light(const std::string& name) {
    auto node = std::make_unique<SceneNode>(name);
    node->node_type = NodeType::Light;
    node->light = std::make_unique<Light>();
    node->light->type = LightType::Area;
    node->light->colour = Colour(1.0f, 1.0f, 1.0f);
    node->light->intensity = 1.0f;
    node->light->is_area_light = true;

    // area lights need geometry
    node->primitive = std::make_unique<QuadPrimitive>(
        Vec3(0.5f, 0, 0),
        Vec3(0, 0, 0.5f)
    );
    node->albedo = node->light->colour;
    return node;
}


} // namespace ollygon
