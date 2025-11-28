// core/selection_system.cpp
#include "selection_system.hpp"
#include "mat4.hpp"
#include <algorithm>

namespace ollygon {

SelectionSystem::SelectionSystem(QObject* parent)
    : QObject(parent)
    , selection_handler(nullptr)
    , edit_mode_manager(nullptr)
    , selection_mode(SelectionMode::Click)
    , box_selecting(false)
{
}

void SelectionSystem::set_selection_mode(SelectionMode new_mode) {
    if (selection_mode == new_mode) return;

    selection_mode = new_mode;
    emit selection_mode_changed(new_mode);
}

void SelectionSystem::handle_mouse_press(Scene* scene, const Camera& camera, const QPoint& pos, int viewport_width, int viewport_height, Qt::KeyboardModifiers modifiers)
{
    if (!selection_handler || !edit_mode_manager) return;

    switch (selection_mode)
    {
    case SelectionMode::Click:
        perform_click_select(scene, camera, pos, viewport_width, viewport_height, modifiers & Qt::ShiftModifier);
        break;
    case SelectionMode::Box:
        start_box_select(pos);
        break;
    case SelectionMode::Lasso:
    case SelectionMode::Paint:
    default:
        break;
    }
}

void SelectionSystem::handle_mouse_move(Scene* scene, const Camera& camera, const QPoint& pos, int viewport_width, int viewport_height)
{
    if (box_selecting) {
        update_box_select(scene, camera, pos, viewport_width, viewport_height);
    }
}

void SelectionSystem::handle_mouse_release(Scene* scene, const Camera& camera, const QPoint& pos, int viewport_width, int viewport_height)
{
    if (box_selecting) {
        finish_box_select();
    }
}

QRect SelectionSystem::get_box_select_rect() const
{
    return QRect(
        std::min(box_start.x(), box_end.x()),
        std::min(box_start.y(), box_end.y()),
        std::abs(box_end.x() - box_start.x()),
        std::abs(box_end.y() - box_start.y())
    );
}

void SelectionSystem::perform_click_select(Scene* scene, const Camera& camera, const QPoint& pos, int viewport_width, int viewport_height, bool add_to_selection)
{
    Vec3 ray_dir = screen_to_ray(camera, pos, viewport_width, viewport_height);

    selection_handler->raycast_select_moded(scene, camera.get_pos(), ray_dir, edit_mode_manager->get_mode(), add_to_selection);
}

void SelectionSystem::start_box_select(const QPoint& pos)
{
    box_selecting = true;
    box_start = pos;
    box_end = pos;
    emit box_select_state_changed();
}

void SelectionSystem::update_box_select(Scene* scene, const Camera& camera, const QPoint& pos, int viewport_width, int viewport_height)
{
    box_end = pos;
    calculate_box_selection(scene, camera, viewport_width, viewport_height);
    emit box_select_state_changed();
}

void SelectionSystem::finish_box_select()
{
    box_selecting = false;
    emit box_select_state_changed();
    // selection already applied during drag, nothing more to do
}

void SelectionSystem::calculate_box_selection(Scene* scene, const Camera& camera, int viewport_width, int viewport_height)
{
    if (!selection_handler || !edit_mode_manager || !scene) return;

    EditMode mode = edit_mode_manager->get_mode();

    if (mode == EditMode::Object) {
        std::vector<SceneNode*> nodes_in_box;
        collect_nodes_in_box(scene->get_root(), camera, viewport_width, viewport_height, nodes_in_box);
        selection_handler->set_selection(nodes_in_box);
        return;
    }

    SceneNode* selected = selection_handler->get_selected_node();
    if (!selected || selected->node_type != NodeType::Mesh || !selected->geo) {
        return;
    }

    Mat4 model = selected->transform.to_matrix();

    ComponentSelection new_selection;

    switch (mode) {
    case EditMode::Vertex: {
        for (uint32_t i = 0; i < selected->geo->verts.size(); ++i) {
            Vec3 world_pos = model.transform_point(selected->geo->verts[i].position);
            if (is_point_in_box(camera, world_pos, viewport_width, viewport_height)) {
                new_selection.vertices.insert(i);
            }
        }
        break;
    }
    case EditMode::Edge: {
        std::unordered_set<uint32_t> tested_edges;
        uint32_t vertex_count = selected->geo->vertex_count();

        for (size_t i = 0; i < selected->geo->indices.size(); i += 3) {
            uint32_t i0 = selected->geo->indices[i];
            uint32_t i1 = selected->geo->indices[i + 1];
            uint32_t i2 = selected->geo->indices[i + 2];

            uint32_t edges[3][2] = { {i0, i1}, {i1, i2}, {i2, i0} };

            for (int e = 0; e < 3; ++e) {
                uint32_t v1 = edges[e][0];
                uint32_t v2 = edges[e][1];

                if (v1 > v2) std::swap(v1, v2);
                uint32_t hash = v1 * vertex_count + v2;
                if (tested_edges.count(hash)) continue;
                tested_edges.insert(hash);

                Vec3 p1 = model.transform_point(selected->geo->verts[v1].position);
                Vec3 p2 = model.transform_point(selected->geo->verts[v2].position);

                if (line_segment_intersects_box(camera, p1, p2, viewport_width, viewport_height)) {
                    new_selection.edges.insert(hash);
                }
            }
        }
        break;
    }
    case EditMode::Face: {
        for (uint32_t face_idx = 0; face_idx < selected->geo->tri_count(); ++face_idx) {
            uint32_t base = face_idx * 3;

            uint32_t i0 = selected->geo->indices[base];
            uint32_t i1 = selected->geo->indices[base + 1];
            uint32_t i2 = selected->geo->indices[base + 2];

            Vec3 p0 = model.transform_point(selected->geo->verts[i0].position);
            Vec3 p1 = model.transform_point(selected->geo->verts[i1].position);
            Vec3 p2 = model.transform_point(selected->geo->verts[i2].position);

            if (is_point_in_box(camera, p0, viewport_width, viewport_height) ||
                is_point_in_box(camera, p1, viewport_width, viewport_height) ||
                is_point_in_box(camera, p2, viewport_width, viewport_height) ||
                line_segment_intersects_box(camera, p0, p1, viewport_width, viewport_height) ||
                line_segment_intersects_box(camera, p1, p2, viewport_width, viewport_height) ||
                line_segment_intersects_box(camera, p2, p0, viewport_width, viewport_height)) {
                new_selection.faces.insert(face_idx);
            }
        }
        break;
    }
    default:
        break;
    }

    selection_handler->set_component_selection(new_selection);
}

Vec3 SelectionSystem::screen_to_ray(const Camera& camera, const QPoint& screen_pos, int viewport_width, int viewport_height) const
{
    float x_ndc = (2.0f * screen_pos.x()) / viewport_width - 1.0f;
    float y_ndc = 1.0f - (2.0f * screen_pos.y()) / viewport_height;

    Vec3 forward = (camera.get_target() - camera.get_pos()).normalised();
    Vec3 right = Vec3::cross(forward, camera.get_up()).normalised();
    Vec3 up = Vec3::cross(right, forward);

    float aspect_ratio = float(viewport_width) / float(viewport_height);
    float fov_rad = camera.get_fov_degs() * DEG_TO_RAD;
    float viewport_half_height = std::tan(fov_rad * 0.5f);
    float viewport_half_width = viewport_half_height * aspect_ratio;

    Vec3 ray_dir = forward + right * (x_ndc * viewport_half_width)
        + up * (y_ndc * viewport_half_height);
    return ray_dir.normalised();
}

bool SelectionSystem::world_to_screen(const Camera& camera, const Vec3& world_pos, int viewport_width, int viewport_height, float& screen_x, float& screen_y) const
{
    Mat4 view = camera.get_view_matrix();
    Mat4 projection = camera.get_projection_matrix();
    Mat4 vp = projection * view;

    Vec4 clip = vp * Vec4(world_pos.x, world_pos.y, world_pos.z, 1.0f);

    if (clip.w <= 0.0f) return false;  // behind camera

    float x_ndc = clip.x / clip.w;
    float y_ndc = clip.y / clip.w;
    float z_ndc = clip.z / clip.w;

    if (z_ndc < -1.0f || z_ndc > 1.0f) return false;

    screen_x = (x_ndc + 1.0f) * 0.5f * viewport_width;
    screen_y = (1.0f - y_ndc) * 0.5f * viewport_height;

    return true;
}

bool SelectionSystem::is_point_in_box(const Camera& camera, const Vec3& world_pos, int viewport_width, int viewport_height) const
{
    float screen_x, screen_y;
    if (!world_to_screen(camera, world_pos, viewport_width, viewport_height, screen_x, screen_y)) {
        return false;
    }

    QRect box = get_box_select_rect();
    return screen_x >= box.left() && screen_x <= box.right() &&
        screen_y >= box.top() && screen_y <= box.bottom();
}

bool SelectionSystem::line_segment_intersects_box(const Camera& camera, const Vec3& p1, const Vec3& p2, int viewport_width, int viewport_height) const
{
    float s1_x, s1_y, s2_x, s2_y;
    bool p1_valid = world_to_screen(camera, p1, viewport_width, viewport_height, s1_x, s1_y);
    bool p2_valid = world_to_screen(camera, p2, viewport_width, viewport_height, s2_x, s2_y);

    // if both points behind camera, no intersection
    if (!p1_valid && !p2_valid) return false;

    QRect box = get_box_select_rect();
    float box_left = box.left();
    float box_right = box.right();
    float box_top = box.top();
    float box_bottom = box.bottom();

    // if either endpoint is in box, we intersect
    if (p1_valid && s1_x >= box_left && s1_x <= box_right && s1_y >= box_top && s1_y <= box_bottom) {
        return true;
    }
    if (p2_valid && s2_x >= box_left && s2_x <= box_right && s2_y >= box_top && s2_y <= box_bottom) {
        return true;
    }

    // if only one point valid, can't do proper line test
    if (!p1_valid || !p2_valid) return false;

    // check if line segment intersects any edge of the box
    // using parametric line-segment intersection
    auto line_intersects_segment = [](float x1, float y1, float x2, float y2,
        float x3, float y3, float x4, float y4) -> bool {
            float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
            if (std::abs(denom) < 1e-6f) return false;

            float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
            float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

            return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
        };

    // test against all four box edges
    if (line_intersects_segment(s1_x, s1_y, s2_x, s2_y, box_left, box_top, box_right, box_top)) return true;
    if (line_intersects_segment(s1_x, s1_y, s2_x, s2_y, box_right, box_top, box_right, box_bottom)) return true;
    if (line_intersects_segment(s1_x, s1_y, s2_x, s2_y, box_left, box_bottom, box_right, box_bottom)) return true;
    if (line_intersects_segment(s1_x, s1_y, s2_x, s2_y, box_left, box_top, box_left, box_bottom)) return true;

    return false;
}

void SelectionSystem::collect_nodes_in_box( SceneNode* node, const Camera& camera, int viewport_width, int viewport_height, std::vector<SceneNode*>& out_nodes)
{
    if (!node || !node->visible || node->locked) return;

    // check primitives
    if (node->primitive && (node->node_type == NodeType::Primitive || node->node_type == NodeType::Light)) {
        // get primitive vertices in world space and check if any edge touches box
        std::vector<float> verts;
        std::vector<unsigned int> indices;
        node->primitive->generate_mesh(verts, indices);

        Mat4 model = node->transform.to_matrix();

        // check if any triangle edge intersects the box
        for (size_t i = 0; i < indices.size(); i += 3) {
            Vec3 p0(verts[indices[i] * 6], verts[indices[i] * 6 + 1], verts[indices[i] * 6 + 2]);
            Vec3 p1(verts[indices[i + 1] * 6], verts[indices[i + 1] * 6 + 1], verts[indices[i + 1] * 6 + 2]);
            Vec3 p2(verts[indices[i + 2] * 6], verts[indices[i + 2] * 6 + 1], verts[indices[i + 2] * 6 + 2]);

            p0 = model.transform_point(p0);
            p1 = model.transform_point(p1);
            p2 = model.transform_point(p2);

            if (is_point_in_box(camera, p0, viewport_width, viewport_height) ||
                is_point_in_box(camera, p1, viewport_width, viewport_height) ||
                is_point_in_box(camera, p2, viewport_width, viewport_height) ||
                line_segment_intersects_box(camera, p0, p1, viewport_width, viewport_height) ||
                line_segment_intersects_box(camera, p1, p2, viewport_width, viewport_height) ||
                line_segment_intersects_box(camera, p2, p0, viewport_width, viewport_height)) {
                out_nodes.push_back(node);
                break;  // found a hit, no need to check more triangles
            }
        }
    }
    // check meshes
    else if (node->geo && node->node_type == NodeType::Mesh) {
        Mat4 model = node->transform.to_matrix();

        for (size_t i = 0; i < node->geo->indices.size(); i += 3) {
            uint32_t i0 = node->geo->indices[i];
            uint32_t i1 = node->geo->indices[i + 1];
            uint32_t i2 = node->geo->indices[i + 2];

            Vec3 p0 = model.transform_point(node->geo->verts[i0].position);
            Vec3 p1 = model.transform_point(node->geo->verts[i1].position);
            Vec3 p2 = model.transform_point(node->geo->verts[i2].position);

            if (is_point_in_box(camera, p0, viewport_width, viewport_height) ||
                is_point_in_box(camera, p1, viewport_width, viewport_height) ||
                is_point_in_box(camera, p2, viewport_width, viewport_height) ||
                line_segment_intersects_box(camera, p0, p1, viewport_width, viewport_height) ||
                line_segment_intersects_box(camera, p1, p2, viewport_width, viewport_height) ||
                line_segment_intersects_box(camera, p2, p0, viewport_width, viewport_height)) {
                out_nodes.push_back(node);
                break;
            }
        }
    }

    for (auto& child : node->children) {
        collect_nodes_in_box(child.get(), camera, viewport_width, viewport_height, out_nodes);
    }
}

} // namespace ollygon
