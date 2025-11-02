#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include "core/scene.hpp"
#include "core/camera.hpp"

namespace ollygon {

class PanelViewport : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit PanelViewport(QWidget* parent = nullptr);
    ~PanelViewport() override;

    void set_scene(Scene* new_scene);
    Camera* get_camera() { return &camera; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void create_sphere_geo(); //TEMP

    Scene* scene;
    Camera camera;

    QOpenGLShaderProgram* shader_program;
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    QOpenGLBuffer ebo;

    int sphere_index_count; //TEMP
};

} // namespace ollygon
