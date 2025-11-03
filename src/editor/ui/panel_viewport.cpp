#include "panel_viewport.hpp"
#include <QOpenGLShader>
#include <QWheelEvent>
#include <cmath>
#include <iostream>

namespace ollygon {

    PanelViewport::PanelViewport(QWidget* parent)
        : QOpenGLWidget(parent)
        , scene(nullptr)
        , shader_program(nullptr)
        , vbo(QOpenGLBuffer::VertexBuffer)
        , ebo(QOpenGLBuffer::IndexBuffer)
        , selection_handler(nullptr)
        , geometry_dirty(true)
        , is_camera_dragging(false)
    {
        setMouseTracking(true);
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
        geometry_dirty = true;
        update();
    }

    void PanelViewport::set_selection_handler(SelectionHandler* handler) {
        selection_handler = handler;

        // connect to selection changes for redraw trigger
        if (handler) {
            connect(handler, &SelectionHandler::selection_changed,
                this, [this]() { update(); });
        }
    }

    void PanelViewport::initializeGL() {
        initializeOpenGLFunctions();
        //glClearColor(1.0f, 0.1f, 0.1f, 1.0f); // the "something is very wrong" colour

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // TEMP the "normal background" colour.  TODO: tie this to a proper world setting synced across to ot

        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_CULL_FACE); // TEMP not yet

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
        uniform bool is_selected;

        out vec4 FragColor;

        void main() {
            vec3 norm = normalize(frag_normal);
            vec3 light_dir = normalize(light_pos - frag_pos);

            float n_dot_dir = max(dot(norm, light_dir), 0.0);
            vec3 diffuse = n_dot_dir * object_colour;

            vec3 ambient = object_colour * 0.3;

            vec3 result = ambient + diffuse;
            
            // highlight selected objects
            if (is_selected) {
                result = mix(result, vec3(1.0, 0.9, 0.4), 0.3);
            }
            
            FragColor = vec4(result, 1.0);
        }
    )";

        shader_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader);
        shader_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_shader);
        shader_program->link();

        vao.create();
        vao.bind();
        vbo.create();
        ebo.create();
        vao.release();
    }

    void PanelViewport::resizeGL(int w, int h) {
        glViewport(0, 0, w, h);
        camera.set_aspect((float)w / (float)h);
    }

    void PanelViewport::rebuild_scene_geometry() {
        if (!scene || !geometry_dirty) return;

        scene_vertices.clear();
        scene_indices.clear();
        geometry_ranges.clear();

        std::cout << "\n== Rebuilding Scene Geometry ==" << std::endl;

        // walk scene and accumulate all geometry, tracking ranges
        std::function<void(SceneNode*)> collect_geometry = [&](SceneNode* node) {
            if (node->geometry && (node->node_type == NodeType::Mesh || node->node_type == NodeType::Light)) {
                unsigned int index_start = scene_indices.size();
                unsigned int vert_start = scene_vertices.size() / 6;

                //TEMP
                std::cout << "NODE: " << node->name
                    << " | vertex start: " << vert_start
                    << " | index start: " << index_start << std::endl;

                // generate mesh for this node
                std::vector<float> node_vertices;
                std::vector<unsigned int> node_indices;
                node->geometry->generate_mesh(node_vertices, node_indices);

                std::cout << "  generated " << (node_vertices.size() / 6) << " vertices, "
                    << node_indices.size() << " indices" << std::endl;

                // offset indices to account for existing verts
                for (unsigned int idx : node_indices) {
                    scene_indices.push_back(idx + vert_start);
                }

                // append vertices
                scene_vertices.insert(scene_vertices.end(), node_vertices.begin(), node_vertices.end());

                // record range for this node
                GeometryRange range;
                range.index_offset = index_start;
                range.index_count = node_indices.size();
                geometry_ranges[node] = range;

                // TEMP until I can get cornell working..
                std::cout << "  RANGE: offset=" << range.index_offset << " count=" << range.index_count << std::endl;
            }

            for (auto& child : node->children) {
                collect_geometry(child.get());
            }
            };

        collect_geometry(scene->get_root());

        std::cout << "total verts: " << (scene_vertices.size() / 6) << " | total indices: " << scene_indices.size() << std::endl;

        // upload to GPU
        vao.bind();

        vbo.bind();
        vbo.allocate(scene_vertices.data(), scene_vertices.size() * sizeof(float));

        ebo.bind();
        ebo.allocate(scene_indices.data(), scene_indices.size() * sizeof(unsigned int));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

        vao.release();

        geometry_dirty = false;
    }

    void PanelViewport::paintGL() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!scene) return;

        rebuild_scene_geometry();

        shader_program->bind();

        Mat4 view = camera.get_view_matrix();
        Mat4 projection = camera.get_projection_matrix();
        Vec3 cam_pos = camera.get_pos();

        shader_program->setUniformValue("view", QMatrix4x4(view.floats()).transposed());
        shader_program->setUniformValue("projection", QMatrix4x4(projection.floats()).transposed());
        shader_program->setUniformValue("light_pos", QVector3D(3.43f, 5.54f, 3.32f));
        shader_program->setUniformValue("view_pos", QVector3D(cam_pos.x, cam_pos.y, cam_pos.z));

        vao.bind();

        // render each node
        render_node(scene->get_root());

        vao.release();
        shader_program->release();
    }

    void PanelViewport::render_node(SceneNode* node) {
        if (!node) return;

        bool is_selected = selection_handler && (selection_handler->get_selected() == node);

        if ((node->node_type == NodeType::Mesh || node->node_type == NodeType::Light) && node->geometry) {
            // check if we have geo for this node
            auto it = geometry_ranges.find(node);
            if (it != geometry_ranges.end()) {
                const GeometryRange& range = it->second;

                Mat4 scale = Mat4::scale(
                    node->transform.scale.x,
                    node->transform.scale.y,
                    node->transform.scale.z
                );

                Mat4 model = Mat4::translate(
                    node->transform.position.x,
                    node->transform.position.y,
                    node->transform.position.z
                );

                model = scale * model;

                shader_program->setUniformValue("model", QMatrix4x4(model.floats()).transposed());
                shader_program->setUniformValue("object_colour",
                    QVector3D(node->albedo.r, node->albedo.g, node->albedo.b));
                shader_program->setUniformValue("is_selected", is_selected);

                glDrawElements(
                    GL_TRIANGLES,
                    range.index_count,
                    GL_UNSIGNED_INT,
                    (void*)(range.index_offset * sizeof(unsigned int))
                );
            }
        }

        for (auto& child : node->children) {
            render_node(child.get());
        }
    }

    void PanelViewport::mousePressEvent(QMouseEvent* event) {
        if (!scene) return;

        last_mouse_pos = event->pos();

        // left mouse = selection
        if (event->button() == Qt::LeftButton) {
            if (event->modifiers() == Qt::NoModifier && selection_handler) {
                // convert screen to normalised device coords
                float x_ndc = (2.0f * event->pos().x()) / width() - 1.0f;
                float y_ndc = 1.0f - (2.0f * event->pos().y()) / height();

                // compute ray direction
                Vec3 forward = (camera.get_target() - camera.get_pos()).normalised();
                Vec3 right = Vec3::cross(forward, camera.get_up()).normalised();
                Vec3 up = Vec3::cross(right, forward);

                float aspect = float(width()) / float(height());
                float fov_rad = 45.0f * 3.14159f / 180.0f; //TEMP
                float h = std::tan(fov_rad * 0.5f);
                float w = h * aspect;

                Vec3 ray_dir = forward + right * (x_ndc * w) + up * (y_ndc * h);
                ray_dir = ray_dir.normalised();

                selection_handler->raycast_select(scene, camera.get_pos(), ray_dir);
            }
        }

        // right/middle mouse = camera
        if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
            is_camera_dragging = true;
            setCursor(Qt::ClosedHandCursor);
        }
    }

    void PanelViewport::mouseMoveEvent(QMouseEvent* event) {
        if (!is_camera_dragging) return;

        QPoint delta = event->pos() - last_mouse_pos;
        last_mouse_pos = event->pos();

        CameraController* controller = camera.get_controller();

        if (event->buttons() & Qt::RightButton) {
            float sensitivity = 0.3f;
            controller->orbit(-delta.x() * sensitivity, delta.y() * sensitivity);
            update();
        }
        else if (event->buttons() & Qt::MiddleButton) {
            if (event->modifiers() & Qt::ShiftModifier) {
                controller->zoom(delta.y() * -0.05f);
            }
            else {
                controller->pan(-delta.x(), delta.y());
            }
            update();
        }
    }

    void PanelViewport::mouseReleaseEvent(QMouseEvent* event) {
        if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
            is_camera_dragging = false;
            setCursor(Qt::ArrowCursor);
        }
    }

    void PanelViewport::wheelEvent(QWheelEvent* event) {
        CameraController* controller = camera.get_controller();
        float delta = event->angleDelta().y() > 0 ? 0.5f : -0.5f;
        controller->zoom(delta);
        update();
        event->accept();
    }

} // namespace ollygon
