#include "panel_viewport.hpp"
#include <QOpenGLShader>
#include <cmath>
#include <vector>
#include <functional>

namespace ollygon {

PanelViewport::PanelViewport(QWidget* parent)
    : QOpenGLWidget(parent)
    , scene(nullptr)
    , shader_program(nullptr)
    , vbo(QOpenGLBuffer::VertexBuffer)
    , ebo(QOpenGLBuffer::IndexBuffer)
    , sphere_index_count(0)
{
}

PanelViewport::~PanelViewport() {
    makeCurrent();
    vao.destroy();
    vbo.destroy();
    ebo.destroy();
    delete shader_program;
    doneCurrent();
}

void PanelViewport::set_scene(Scene* new_scene) {
    scene = new_scene;
    update();
}

void PanelViewport::initializeGL() {
    initializeOpenGLFunctions();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    // TEMP basic shaders
    // TODO: decouple actual scene and use this just as GL wrapper panel handler
    shader_program = new QOpenGLShaderProgram(this);

    const char* vertex_shader = R"(
        #version 330 core
        layout(location = 0) in vec3 position;
        layout(location = 1) in vec3 normal;
        
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        out vec3 frag_normal;
        out vec3 frag_pos;

        void main() {
            frag_pos = vec3(model * vec4(position, 1.0));
            frag_normal = mat3(transpose(inverse(model))) * normal;
            gl_Position = projection * view * vec4(frag_pos, 1.0);
        }
    )";

    const char* fragment_shader = R"(
        #version 330 core
        in vec3 frag_normal;	
        in vec3 frag_pos;

        uniform vec3 object_colour;
        uniform vec3 light_pos;
        uniform vec3 view_pos;

        out vec4 FragColor;

        void main() {
            vec3 norm = normalize(frag_normal);
            vec3 light_dir = normalize(light_pos - frag_pos);

            float n_dot_dir = max(dot(norm, light_dir), 0.0);
            vec3 diffuse = n_dot_dir * object_colour;

            vec3 ambient = object_colour * 0.3;

            vec3 result = ambient + diffuse;
            FragColor = vec4(result, 1.0);
        }
    )";
    shader_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader);
    shader_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_shader);
    shader_program->link();

    create_sphere_geo();
}

void PanelViewport::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    camera.set_aspect((float)w / (float)h);
}

void PanelViewport::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!scene) return;

    shader_program->bind();

    Mat4 view = camera.get_view_matrix();
    Mat4 projection = camera.get_projection_matrix();
    Vec3 cam_pos = camera.get_pos();

    shader_program->setUniformValue("view", QMatrix4x4(view.floats()).transposed());
    shader_program->setUniformValue("projection", QMatrix4x4(projection.floats()).transposed());
    shader_program->setUniformValue("light_pos", QVector3D(6.0f, 6.0f, 6.0f));
    shader_program->setUniformValue("view_pos", QVector3D(cam_pos.x, cam_pos.y, cam_pos.z));

    vao.bind();

    // TEMP render our test spheres
    std::function<void(SceneNode*)> render_node = [&](SceneNode* node) {
        if (node->sphere) { // TEMP
            Mat4 model, scale;
            model = Mat4::translate(node->transform.position.x, node->transform.position.y, node->transform.position.z);
            scale = Mat4::scale(node->transform.scale.x, node->transform.scale.y, node->transform.scale.z);
            model = scale * model;

            shader_program->setUniformValue("model", QMatrix4x4(model.floats()).transposed());
            shader_program->setUniformValue("object_colour", QVector3D(node->sphere->albedo.r, node->sphere->albedo.g, node->sphere->albedo.b));

            glDrawElements(GL_TRIANGLES, sphere_index_count, GL_UNSIGNED_INT, 0);
        }

        for (auto& child : node->children) {
            render_node(child.get());
        }
    };

    render_node(scene->get_root());

    vao.release();
    shader_program->release();
}

void PanelViewport::create_sphere_geo() {
    // TEMP for mvp
    const int segments = 32;
    const int rings = 16;
    std::vector<float> verts;
    std::vector<unsigned int> indices;

    // sphere verts
    for (int ring = 0; ring <= rings; ring++)
    {
        float phi = 3.1459f * float(ring) / float(rings);
        for (int seg = 0; seg <= segments; seg++)
        {
            float theta = 2.0f * 3.14159f * float(seg) / float(segments);
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);

            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
            verts.push_back(x); // normals
            verts.push_back(y);
            verts.push_back(z);
        }
    }

    // generate indices
    for (int ring = 0; ring < rings; ring++)
    {
        for (int seg = 0; seg < segments; seg++)
        {
            int current = ring * (segments + 1) + seg;
            int next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next+1);
        }
    }

    sphere_index_count = indices.size();

    vao.create();
    vao.bind();

    vbo.create();
    vbo.bind();
    vbo.allocate(verts.data(), verts.size() * sizeof(float));

    ebo.create();
    ebo.bind();
    ebo.allocate(indices.data(), indices.size() * sizeof(unsigned int));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    vao.release();
}

} // namespace ollygon
