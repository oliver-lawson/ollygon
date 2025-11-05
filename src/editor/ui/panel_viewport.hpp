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
    Camera* get_camera() { return &camera; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void rebuild_scene_geometry();
    void render_node(SceneNode* node);

    Scene* scene;
    Camera camera;
    SelectionHandler* selection_handler;

    QOpenGLShaderProgram* shader_program;
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    QOpenGLBuffer ebo;

    std::vector<float> scene_vertices;
    std::vector<unsigned int> scene_indices;
    std::unordered_map<SceneNode*, GeometryRange> geometry_ranges;
    bool geometry_dirty;

    // camera controlling
    bool is_camera_dragging;
    QPoint last_mouse_pos;
};

} // namespace ollygon