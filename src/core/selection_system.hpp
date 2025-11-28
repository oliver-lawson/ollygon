#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>
#include "selection_handler.hpp"
#include "selection_modes.hpp"
#include "edit_mode.hpp"
#include "camera.hpp"
#include "scene.hpp"

namespace ollygon {

//handles all selection logic - raycasting, box select etc
//viewport calls this
class SelectionSystem : public QObject {
    Q_OBJECT

public:
    explicit SelectionSystem(QObject* parent = nullptr);

    void set_selection_handler(SelectionHandler* handler) { selection_handler = handler; }
    void set_edit_mode_manager(EditModeManager* manager) { edit_mode_manager = manager; }

    SelectionMode get_selection_mode() const { return selection_mode; }
    void set_selection_mode(SelectionMode new_mode);

    // interaction - called from viewport
    void handle_mouse_press(Scene* scene, const Camera& camera, const QPoint& pos, int viewport_width, int viewport_height, Qt::KeyboardModifiers modifiers);
    void handle_mouse_move(Scene* scene, const Camera& camera, const QPoint& pos, int viewport_width, int viewport_height);
    void handle_mouse_release(Scene* scene, const Camera& camera, const QPoint& pos, int viewport_width, int viewport_height);

    // box select state (for rendering)
    bool is_box_selecting() const { return box_selecting; }
    QRect get_box_select_rect() const;

signals:
    void selection_mode_changed(SelectionMode mode);
    void box_select_state_changed(); // for viewport to update

private:
    SelectionHandler* selection_handler;
    EditModeManager* edit_mode_manager;
    SelectionMode selection_mode;

    // click select state
    void perform_click_select( Scene* scene, const Camera& camera, const QPoint& pos, int viewport_width, int viewport_height, bool add_to_selection );

    // box select state
    bool box_selecting;
    QPoint box_start;
    QPoint box_end;

    void start_box_select(const QPoint& pos);
    void update_box_select(Scene* scene, const Camera& camera, const QPoint& pos, int viewport_width, int viewport_height);
    void finish_box_select();

    void calculate_box_selection(Scene* scene, const Camera& camera, int viewport_width, int viewport_height);

    // helpers
    Vec3 screen_to_ray( const Camera& camera, const QPoint& screen_pos, int viewport_width, int viewport_height ) const;
    bool world_to_screen(const Camera& camera, const Vec3& world_pos, int viewport_width, int viewport_height, float& screen_x, float& screen_y) const;

    bool line_segment_intersects_box(const Camera& camera, const Vec3& p1, const Vec3& p2, int viewport_width, int viewport_height) const;

    bool is_point_in_box( const Camera& camera, const Vec3& world_pos, int viewport_width, int viewport_height ) const;

    void collect_nodes_in_box( SceneNode* node, const Camera& camera, int viewport_width, int viewport_height, std::vector<SceneNode*>& out_nodes );

};

} // namespace ollygon
