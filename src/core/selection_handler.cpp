#include "selection_handler.hpp"
#include <limits>

namespace ollygon {

SelectionHandler::SelectionHandler(QObject* parent)
    : QObject(parent)
    , selected_node(nullptr)
{
}

void SelectionHandler::set_selected(SceneNode* node) {

    if (selected_node == node) return; // already selected

    selected_node = node;
    emit selection_changed(node);
}

void SelectionHandler::clear_selection() {
    set_selected(nullptr);
}

bool SelectionHandler::raycast_select(Scene* scene, const Vec3& ray_origin, const Vec3& ray_dir) {
    float closest_t = std::numeric_limits<float>::max();
    SceneNode* hit_node = nullptr;

    raycast_node(scene->get_root(), ray_origin, ray_dir, closest_t, hit_node);

    if (hit_node) {
        set_selected(hit_node);
        return true;
    }

    return false;
}

bool SelectionHandler::raycast_node(SceneNode* node, const Vec3& ray_origin, const Vec3& ray_dir, float& closest_t, SceneNode*& hit_node) {
    bool hit_anything = false;

    // test geometry if present
    if (node->geometry && node->node_type == NodeType::Mesh) {
        // transform ray to local space
        Vec3 local_origin = ray_origin - node->transform.position;
        // TODO: proper inverse transform when we add rotation/scale

        float t;
        Vec3 normal;
        if (node->geometry->intersect_ray(local_origin, ray_dir, t, normal)) {
            if (t < closest_t) {
                closest_t = t;
                hit_node = node;
                hit_anything = true;
            }
        }
    }

    // test children recursively
    for (auto& child : node->children) {
        if (raycast_node(child.get(), ray_origin, ray_dir, closest_t, hit_node)) {
            hit_anything = true;
        }
    }

    return hit_anything;
}

} // namespace ollygon