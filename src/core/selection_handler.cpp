#include "selection_handler.hpp"
#include "mat4.hpp"
#include <limits>
#include <algorithm>

namespace ollygon {

SelectionHandler::SelectionHandler(QObject* parent)
    : QObject(parent)
    , selected_node(nullptr)
{}

void SelectionHandler::set_selected(SceneNode* node) {
    if (selected_node == node) return;

    selected_node = node;
    component_selection.clear();
    emit selection_changed(node);
    emit component_selection_changed();
}

void SelectionHandler::clear_selection() {
    set_selected(nullptr);
}

void SelectionHandler::clear_component_selection() {
    component_selection.clear();
    emit component_selection_changed();
}

void SelectionHandler::set_component_selection(const ComponentSelection& new_selection) {
    component_selection = new_selection;
    emit component_selection_changed();
}

bool SelectionHandler::raycast_select_moded( Scene* scene, const Vec3& ray_origin, const Vec3& ray_dir, EditMode mode, bool add_to_selection ) {
    if (mode == EditMode::Object) {
        // object mode - standard raycast
        float closest_t = std::numeric_limits<float>::max(); //TODO constants this
        SceneNode* hit_node = nullptr;

        raycast_anything_get_scenenode(scene->get_root(), ray_origin, ray_dir, closest_t, hit_node);

        if (hit_node) {
            set_selected(hit_node);
            return true;
        }
        return false;
    }
    // component modes - need an object selected first
    if (!selected_node) {
        // no object selected, try selecting one
        return raycast_select_moded(scene, ray_origin, ray_dir, EditMode::Object, false);
    }
    // only meshes support component selection
    if (selected_node->node_type != NodeType::Mesh || !selected_node->geo) {
        return false;
    }

    bool component_hit = false;

    if (!add_to_selection) component_selection.clear();

    switch (mode) {
    case EditMode::Vertex: {
        uint32_t vertex_index;
        float dist;
        if (raycast_vertex(selected_node, ray_origin, ray_dir, vertex_index, dist)) {
            if (add_to_selection) {
                // toggle selection
                if (component_selection.vertices.count(vertex_index)) {
                    component_selection.vertices.erase(vertex_index);
                }
                else { component_selection.vertices.insert(vertex_index); }
            }
            else { component_selection.vertices.insert(vertex_index); }
            component_hit = true;
        }
        break;
    }
    case EditMode::Edge: {
        uint32_t v1, v2;
        float dist;
        if (raycast_edge(selected_node, ray_origin, ray_dir, v1, v2, dist)) {
            uint32_t hash = edge_hash(v1, v2, selected_node->geo->vertex_count());
            if (add_to_selection) {
                if (component_selection.edges.count(hash)) {
                    component_selection.edges.erase(hash);
                }
                else { component_selection.edges.insert(hash); }
            }
            else { component_selection.edges.insert(hash); }
            component_hit = true;
        }
        break;
    }
    case EditMode::Face: {
        uint32_t face_index;
        float t;
        if (raycast_face(selected_node, ray_origin, ray_dir, face_index, t)) {
            if (add_to_selection) {
                if (component_selection.faces.count(face_index)) {
                    component_selection.faces.erase(face_index);
                }
                else { component_selection.faces.insert(face_index); }
            }
            else { component_selection.faces.insert(face_index); }
            component_hit = true;
        }
        break;
    }
    default:
        break;
    }

    if (component_hit) emit component_selection_changed();

    return component_hit;
}

bool SelectionHandler::raycast_anything_get_scenenode( SceneNode* node, const Vec3& ray_origin, const Vec3& ray_dir, float& closest_t, SceneNode*& hit_node )
{
    bool hit_anything = false; //prims OR mesh (or more in future..)

    if (node->locked || !node->visible) return false;

    // build transform
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
        if (raycast_anything_get_scenenode(child.get(), ray_origin, ray_dir, closest_t, hit_node)) {
            hit_anything = true;
        }
    }

    return hit_anything;
}

bool SelectionHandler::raycast_vertex( SceneNode* node, const Vec3& ray_origin, const Vec3& ray_dir, uint32_t& vertex_index, float& closest_dist )
{
    if (!node->geo || node->geo->verts.empty()) return false;

    // build world transform
    Mat4 model = node->transform.to_matrix();

    // screen-space selection threshold (in world units)
    // TODO: make this screen-space accurate with projection? test out after obj import
    const float selection_radius = 0.15f;

    closest_dist = std::numeric_limits<float>::max(); //TODO constants
    bool found = false;

    for (uint32_t i = 0; i < node->geo->verts.size(); ++i) { //TODO accelerate
        Vec3 world_pos = model.transform_point(node->geo->verts[i].position);

        // compute distance from ray to point
        Vec3 to_point = world_pos - ray_origin;
        float t = Vec3::dot(to_point, ray_dir);

        if (t < 0.0f) continue; // behind camera! maybe a slightly bigger small number sensible?
                                // TODO: revisit after camera min/max distants implemented

        Vec3 closest_point_on_ray = ray_origin + ray_dir * t;
        float dist = (world_pos - closest_point_on_ray).length();

        if (dist < selection_radius && dist < closest_dist) {
            closest_dist = dist;
            vertex_index = i;
            found = true;
        }
    }

    return found;
}

bool SelectionHandler::raycast_edge( SceneNode* node, const Vec3& ray_origin, const Vec3& ray_dir, uint32_t& v1_index, uint32_t& v2_index, float& closest_dist )
{
    if (!node->geo || node->geo->indices.empty()) return false;

    Mat4 model = node->transform.to_matrix();

    const float selection_radius = 0.15f;
    closest_dist = std::numeric_limits<float>::max(); //TODO constants
    bool found = false;

    // iterate through all edges in triangles
    std::unordered_set<uint32_t> tested_edges;
    for (size_t i = 0; i < node->geo->indices.size(); i += 3) {
        uint32_t i0 = node->geo->indices[i];
        uint32_t i1 = node->geo->indices[i + 1];
        uint32_t i2 = node->geo->indices[i + 2];

        // test three edges of triangle
        uint32_t edges[3][2] = { {i0, i1}, {i1, i2}, {i2, i0} };

        for (int e = 0; e < 3; ++e) {
            uint32_t v1 = edges[e][0];
            uint32_t v2 = edges[e][1];

            // avoid testing same edge twice
            uint32_t hash = edge_hash(v1, v2, node->geo->vertex_count());
            if (tested_edges.count(hash)) continue;
            tested_edges.insert(hash);

            Vec3 p1 = model.transform_point(node->geo->verts[v1].position);
            Vec3 p2 = model.transform_point(node->geo->verts[v2].position);

            // compute distance from ray to line segment
            Vec3 edge_vec = p2 - p1;
            Vec3 w = ray_origin - p1;

            float a = Vec3::dot(ray_dir, ray_dir);
            float b = Vec3::dot(ray_dir, edge_vec);
            float c = Vec3::dot(edge_vec, edge_vec);
            float d = Vec3::dot(ray_dir, w);
            float e_dot = Vec3::dot(edge_vec, w);

            float denom = a * c - b * b;
            if (std::abs(denom) < 1e-6f) continue;

            float s = (b * e_dot - c * d) / denom;
            float t = (a * e_dot - b * d) / denom;

            // clamp t to [0,1] for line segment
            t = std::clamp(t, 0.0f, 1.0f);

            if (s < 0.0f) continue; // behind camera

            Vec3 point_on_ray = ray_origin + ray_dir * s;
            Vec3 point_on_edge = p1 + edge_vec * t;

            float dist = (point_on_ray - point_on_edge).length();

            if (dist < selection_radius && dist < closest_dist) {
                closest_dist = dist;
                v1_index = v1;
                v2_index = v2;
                found = true;
            }
        }
    }

    return found;
}

bool SelectionHandler::raycast_face( SceneNode* node, const Vec3& ray_origin, const Vec3& ray_dir, uint32_t& face_index, float& closest_t )
{
    if (!node->geo || node->geo->indices.empty()) return false;

    // build transform
    Mat4 model = node->transform.to_matrix();
    Mat4 inv_model = model.inverse();

    // transform ray to local space
    Vec3 local_origin = inv_model.transform_point(ray_origin);
    Vec3 local_dir = inv_model.transform_direction(ray_dir).normalised();

    closest_t = std::numeric_limits<float>::max();//TODO constants
    Vec3 normal;
    uint32_t tri_index;

    // use existing mesh intersection
    if (node->geo->intersect_ray(local_origin, local_dir, closest_t, normal, tri_index)) {
        face_index = tri_index;
        return true;
    }

    return false;
}

} // namespace ollygon
