#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMouseEvent>
#include <unordered_map>
#include "core/scene.hpp"
#include "core/camera.hpp"
#include "core/selection_handler.hpp"
#include "core/edit_mode.hpp"
#include "toolbar_edit_mode.hpp"

namespace ollygon {

// tracks where each node's geometry lives in the combined buffer
struct GeometryRange {
    unsigned int index_offset;
    unsigned int index_count;
};

class PanelViewport : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit PanelViewport(QWidget* parent = nullptr);
    ~PanelViewport() override;

    void set_scene(Scene* new_scene);
    void set_selection_handler(SelectionHandler* handler);
    void set_edit_mode_manager(EditModeManager* manager);
    Camera* get_camera() { return &camera; }

    void mark_geometry_dirty() { geometry_dirty = true; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuild_scene_geometry();
    void render_sky_background();
    void render_node(SceneNode* node, bool render_transparent);
    void position_toolbar();

    Scene* scene;
    Camera camera;
    SelectionHandler* selection_handler;
    EditModeManager* edit_mode_manager;

    QOpenGLShaderProgram* shader_program;
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    QOpenGLBuffer ebo;

    QOpenGLShaderProgram* sky_shader_program;
    QOpenGLVertexArrayObject sky_vao;
    QOpenGLBuffer sky_vbo;
    QOpenGLBuffer sky_ebo;

    std::vector<float> scene_verts;
    std::vector<unsigned int> scene_indices;
    std::unordered_map<SceneNode*, GeometryRange> geometry_ranges;
    bool geometry_dirty;

    // camera controlling
    bool is_camera_dragging;
    QPoint last_mouse_pos;
    // UI overlay
    ToolbarEditMode* toolbar;

signals:
    void camera_moved();
};


} // namespace ollygon