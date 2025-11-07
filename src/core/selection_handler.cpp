#include "selection_handler.hpp"
#include "mat4.hpp"
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

    if (node->locked || !node->visible) return false;

    // build transform matrices once
    Mat4 translate = Mat4::translate(
        node->transform.position.x,
        node->transform.position.y,
        node->transform.position.z
    );
    Mat4 scale = Mat4::scale(
        node->transform.scale.x,
        node->transform.scale.y,
        node->transform.scale.z
    );
    Mat4 model = translate * scale;
    Mat4 inv_model = model.inverse();

    // transform ray to local space
    Vec3 local_origin = inv_model.transform_point(ray_origin);
    Vec3 local_dir = inv_model.transform_direction(ray_dir).normalised();

    float t;
    Vec3 normal;

    // test primitive if present
    if (node->primitive && node->node_type == NodeType::Primitive) {
        if (node->primitive->intersect_ray(local_origin, local_dir, t, normal)) {
            if (t < closest_t) {
                closest_t = t;
                hit_node = node;
                hit_anything = true;
            }
        }
    }
    // test mesh if present
    else if (node->geo && node->node_type == NodeType::Mesh) {
        uint32_t tri_index;
        if (node->geo->intersect_ray(local_origin, local_dir, t, normal, tri_index)) {
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