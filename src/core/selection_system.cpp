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
    case ollygon::SelectionMode::Click:perform_click_select(scene, camera, pos, viewport_width, viewport_height, modifiers & Qt::ShiftModifier);
        break;
    case ollygon::SelectionMode::Box:
        start_box_select(pos);
        break;
    case ollygon::SelectionMode::Lasso:
    case ollygon::SelectionMode::Paint:
    default:
        break;
    }
}

void SelectionSystem::handle_mouse_move(const QPoint& pos)
{
    if (box_selecting) update_box_select(pos);
}

void SelectionSystem::handle_mouse_release(Scene* scene, const Camera& camera, const QPoint& pos, int viewport_width, int viewport_height)
{
    if (box_selecting) finish_box_select(scene, camera, viewport_width, viewport_height);
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

void SelectionSystem::update_box_select(const QPoint& pos)
{
    box_end = pos;
    emit box_select_state_changed();
}

void SelectionSystem::finish_box_select(Scene* scene, const Camera& camera, int viewport_width, int viewport_height)
{
    box_selecting = false;
    emit box_select_state_changed();

    if (!selection_handler || !edit_mode_manager || !scene) return;

    EditMode mode = edit_mode_manager->get_mode();

    if (mode == EditMode::Object) {
        // TODO: implement object box/multiple select
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
        for (size_t i = 0; i < selected->geo->indices.size(); i += 3) {
            uint32_t i0 = selected->geo->indices[i];
            uint32_t i1 = selected->geo->indices[i + 1];
            uint32_t i2 = selected->geo->indices[i + 2];

            uint32_t edges[3][2] = { {i0, i1}, {i1, i2}, {i2, i0} };

            for (int e = 0; e < 3; ++e) {
                uint32_t v1 = edges[e][0];
                uint32_t v2 = edges[e][1];

                if (v1 > v2) std::swap(v1, v2);
                uint32_t hash = v1 * selected->geo->vertex_count() + v2;
                if (tested_edges.count(hash)) continue;
                tested_edges.insert(hash);

                Vec3 p1 = model.transform_point(selected->geo->verts[v1].position);
                Vec3 p2 = model.transform_point(selected->geo->verts[v2].position);

                if (is_point_in_box(camera, p1, viewport_width, viewport_height) &&
                    is_point_in_box(camera, p2, viewport_width, viewport_height)) {
                    new_selection.edges.insert(hash);
                }
            }
        }
        break;
    }
    case EditMode::Face: {
        for (uint32_t face_idx = 0; face_idx < selected->geo->tri_count(); ++face_idx) {
            uint32_t base = face_idx * 3;

            bool all_in_box = true;
            for (int i = 0; i < 3; ++i) {
                uint32_t v_idx = selected->geo->indices[base + i];
                Vec3 world_pos = model.transform_point(selected->geo->verts[v_idx].position);
                if (!is_point_in_box(camera, world_pos, viewport_width, viewport_height)) {
                    all_in_box = false;
                    break;
                }
            }

            if (all_in_box) {
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

bool SelectionSystem::is_point_in_box(const Camera& camera, const Vec3& world_pos, int viewport_width, int viewport_height) const
{
    Mat4 view = camera.get_view_matrix();
    Mat4 projection = camera.get_projection_matrix();
    Mat4 vp = projection * view;

    Vec4 clip = Vec4(world_pos.x, world_pos.y, world_pos.z, 1.0f);
    clip = vp * clip;

    if (clip.w == 0.0f) return false;

    float x_ndc = clip.x / clip.w;
    float y_ndc = clip.y / clip.w;
    float z_ndc = clip.z / clip.w;

    if (z_ndc < -1.0f || z_ndc > 1.0f) return false;

    float screen_x = (x_ndc + 1.0f) * 0.5f * viewport_width;
    float screen_y = (1.0f - y_ndc) * 0.5f * viewport_height;

    QRect box = get_box_select_rect();

    return screen_x >= box.left() && screen_x <= box.right() &&
        screen_y >= box.top() && screen_y <= box.bottom();
}

} // namespace ollygon
