#include "panel_viewport.hpp"
#include <QOpenGLShader>
#include <QWheelEvent>
#include <QResizeEvent>
#include <cmath>
#include <iostream>
#include "panel_scene_hierarchy.hpp"

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
        , toolbar(nullptr)
    {
        setMouseTracking(true);
    }

    PanelViewport::~PanelViewport() {
        makeCurrent();
        vao.destroy();
        vbo.destroy();
        ebo.destroy();
        delete shader_program;

        sky_vao.destroy();
        sky_vbo.destroy();
        sky_ebo.destroy();
        delete sky_shader_program;

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

    void PanelViewport::set_edit_mode_manager(EditModeManager* manager) {
        edit_mode_manager = manager;

        // create toolbar if we don't have one yet
        if (!toolbar && manager) {
            toolbar = new ToolbarEditMode(manager, this);
            toolbar->setStyleSheet(
                "QToolBar { border: none; background: transparent; }"
                "QPushButton { "
                "  padding: 6px 12px; "
                "  background: rgba(40, 40, 40, 200); "
                "  border: 1px solid rgba(80, 80, 80, 200); "
                "  border-radius: 4px; "
                "  color: #ddd; "
                "}"
                "QPushButton:hover { "
                "  background: rgba(60, 60, 60, 220); "
                "  border: 1px solid rgba(100, 100, 100, 220); "
                "}"
                "QPushButton:checked { "
                "  background: rgba(255, 140, 0, 200); "
                "  border: 1px solid rgba(255, 160, 30, 220); "
                "  color: white; "
                "}"
                "QPushButton:disabled { "
                "  background: rgba(30, 30, 30, 150); "
                "  color: #666; "
                "}"
            );
            position_toolbar();
        }

        // connect to mode changes for viewport updates
        if (manager) {
            connect(manager, &EditModeManager::mode_changed,
                this, [this]() { update(); });
        }
    }

    void PanelViewport::initializeGL() {
        initializeOpenGLFunctions();

        // use scene sky bottom colour for clear colour
        if (scene) {
            const Sky& sky = scene->get_sky();
            glClearColor(sky.colour_bottom.r, sky.colour_bottom.g, sky.colour_bottom.b, 1.0f);
        }
        else {
            glClearColor(1.0f, 0.1f, 0.1f, 1.0f); // error red
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE); // TODO add control
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // == object shader ==
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
        out vec3 frag_local_pos;

        void main() {
            frag_local_pos = position;
            frag_pos = vec3(model * vec4(position, 1.0));
            frag_normal = mat3(transpose(inverse(model))) * normal;
            gl_Position = projection * view * vec4(frag_pos, 1.0);
        }
    )";

        const char* fragment_shader = R"(
        #version 330 core
        in vec3 frag_normal;	
        in vec3 frag_pos;
        in vec3 frag_local_pos;

        uniform vec3 light_pos;
        uniform vec3 view_pos;
        uniform bool is_selected;

        // material properties
        uniform int material_type;  // 0=lambertian, 1=metal, 2=dielec, 3=emissive, 4=chequer
        uniform vec3 albedo;
        uniform vec3 emission;
        uniform float roughness;
        uniform float metallic;
        uniform vec3 chequer_colour_a;
        uniform vec3 chequer_colour_b;
        uniform float chequer_scale;

        out vec4 FragColor;

        vec3 get_material_colour() {
            if (material_type == 4) { // chequer
                //vec3 scaled_pos = frag_local_pos * chequer_scale;
                vec3 scaled_pos = frag_pos * chequer_scale;
                float pattern = mod(floor(scaled_pos.x) + floor(scaled_pos.y) + floor(scaled_pos.z), 2.0);
                return mix(chequer_colour_a, chequer_colour_b, pattern);
            }
            if (material_type ==2){ // dielectric override TEMP
                return vec3(0.8,0.8,0.8);
            }
            return albedo;
        }

        // gamma correction: fixed to gamma = 2.0 as in raytracer
        vec3 gamma_correct(vec3 colour) {
            return sqrt(colour);
        }

        void main() {
            vec3 base_colour = get_material_colour();
            vec3 norm = normalize(frag_normal);

            // emissive-only mats just glow
            if (material_type == 3) {
                vec3 result = emission;
                if (is_selected) {
                    result = mix(result, vec3(1.0, 0.9, 0.4), 0.3);
                }
                result = gamma_correct(result);
                FragColor = vec4(result, 1.0);
                return;
            }
            
            // basic shared lighting for non-emissives
            vec3 light_dir = normalize(light_pos - frag_pos);
            float norm_dot_lightdir = max(dot(norm, light_dir), 0.0);
            vec3 view_dir = normalize(view_pos - frag_pos);
            vec3 halfway = normalize(light_dir + view_dir);
            
            // diffuse
            vec3 diffuse = norm_dot_lightdir * base_colour;
            
            // specular
            float spec_strength = 0.0;
            if (material_type == 1) { // metal
                float roughness_sq = roughness * roughness;
                float spec_power = mix(128.0, 8.0, roughness_sq);
                spec_strength = pow(max(dot(norm, halfway), 0.0), spec_power) * (1.0 - roughness);
            } else if (material_type == 2) { // dielectric
                spec_strength = pow(max(dot(norm, halfway), 0.0), 64.0) * 0.8;
            }

            vec3 specular = vec3(spec_strength);
            
            vec3 ambient = base_colour * 0.3;

            vec3 result = ambient + diffuse + specular;

            // alpha for dielectrics
            float alpha = 1.0;
            if (material_type == 2) {
                alpha = 0.78;
            }
            
            // highlight selected objects
            if (is_selected) {
                result = mix(result, vec3(1.0, 0.9, 0.4), 0.3);
            }

            result = gamma_correct(result);
            
            FragColor = vec4(result, alpha);
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

        // == sky shader ==

        const char* sky_vertex_shader = R"(
        #version 330 core
        layout(location = 0) in vec2 position;

        out vec3 world_direction;

        uniform mat4 inv_view_projection;

        void main() {
            // convert to clip space
            vec4 clip_pos = vec4(position, 1.0, 1.0);
    
            // convert to ws dir
            vec4 world_pos = inv_view_projection * clip_pos;
            world_direction = normalize(world_pos.xyz / world_pos.w);
    
            gl_Position = clip_pos;
        }
        )";

        const char* sky_fragment_shader = R"(
        #version 330 core
        in vec3 world_direction;
        out vec4 FragColor;

        uniform vec3 sky_bottom_colour;
        uniform vec3 sky_top_colour;
        uniform float sky_bottom_height;
        uniform float sky_top_height;

        // gamma correction: fixed to gamma = 2.0 as in raytracer
        vec3 gamma_correct(vec3 colour) {
            return sqrt(colour);
        }

        void main() {
            // must match Sky::sample() !
            float t = (world_direction.z + 1.0) * 0.5;
    
            vec3 sky_colour;
            if (t <= sky_bottom_height) {
                sky_colour = sky_bottom_colour;
            } else if (t >= sky_top_height) {
                sky_colour = sky_top_colour;
            } else {
                float range = sky_top_height - sky_bottom_height;
                float blend = (t - sky_bottom_height) / range;
                sky_colour = mix(sky_bottom_colour, sky_top_colour, blend);
            }

            sky_colour = gamma_correct(sky_colour);
    
            FragColor = vec4(sky_colour, 1.0);
        }
        )";

        sky_shader_program = new QOpenGLShaderProgram(this);
        sky_shader_program->addShaderFromSourceCode(QOpenGLShader::Vertex, sky_vertex_shader);
        sky_shader_program->addShaderFromSourceCode(QOpenGLShader::Fragment, sky_fragment_shader);
        sky_shader_program->link();

        // make fullscreen quad for this
        float sky_quad_vertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f,  1.0f, -1.0f,  1.0f };
        unsigned int sky_quad_indices[] = { 0, 1, 2, 0, 2, 3 };

        sky_vao.create();
        sky_vao.bind();

        sky_vbo = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        sky_vbo.create();
        sky_vbo.bind();
        sky_vbo.allocate(sky_quad_vertices, sizeof(sky_quad_vertices));

        sky_ebo = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
        sky_ebo.create();
        sky_ebo.bind();
        sky_ebo.allocate(sky_quad_indices, sizeof(sky_quad_indices));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

        sky_vao.release();
    }

    void PanelViewport::resizeGL(int w, int h) {
        glViewport(0, 0, w, h);
        camera.set_aspect((float)w / (float)h);
    }

    void PanelViewport::rebuild_scene_geometry() {
        if (!scene || !geometry_dirty) return;

        scene_verts.clear();
        scene_indices.clear();
        geometry_ranges.clear();

        std::cout << "\n== Rebuilding Scene Geometry ==" << std::endl;

        // walk scene and accumulate all geometry, tracking ranges
        std::function<void(SceneNode*)> collect_geometry = [&](SceneNode* node) {
            if (!node->visible) return;

            bool has_renderable_object = false;
            std::vector<float> node_verts;
            std::vector<unsigned int> node_indices;

            //prims
            if (node->primitive && (node->node_type == NodeType::Primitive || node->node_type == NodeType::Light)) {
                node->primitive->generate_mesh(node_verts, node_indices);
                has_renderable_object = true;
            }
            // meshes
            else if (node->geo && (node->node_type == NodeType::Mesh)) {
                node->geo->generate_render_data(node_verts, node_indices);
                has_renderable_object = true;
            }

            if (has_renderable_object) {
                unsigned int index_start = scene_indices.size();
                unsigned int vert_start = scene_verts.size() / 6;

                //TEMP
                std::cout << "NODE: " << node->name
                    << " | vertex start: " << vert_start
                    << " | index start: " << index_start << std::endl;

                std::cout << "  generated " << (node_verts.size() / 6) << " verts, "
                    << node_indices.size() << " indices" << std::endl;

                // offset indices to account for existing verts
                for (unsigned int idx : node_indices) {
                    scene_indices.push_back(idx + vert_start);
                }

                // append verts
                scene_verts.insert(scene_verts.end(), node_verts.begin(), node_verts.end());

                // record range for this node
                GeometryRange range;
                range.index_offset = index_start;
                range.index_count = node_indices.size();
                geometry_ranges[node] = range;

                // TEMP
                std::cout << "  RANGE: offset=" << range.index_offset << " count=" << range.index_count << std::endl;
            }

            for (auto& child : node->children) {
                collect_geometry(child.get());
            }
            };

        collect_geometry(scene->get_root());

        std::cout << "total verts: " << (scene_verts.size() / 6) << " | total indices: " << scene_indices.size() << std::endl;

        // upload to GPU
        vao.bind();

        vbo.bind();
        vbo.allocate(scene_verts.data(), scene_verts.size() * sizeof(float));

        ebo.bind();
        ebo.allocate(scene_indices.data(), scene_indices.size() * sizeof(unsigned int));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

        vao.release();

        geometry_dirty = false;
    }

    void PanelViewport::render_sky_background() {
        if (!scene) return;

        const Sky& sky = scene->get_sky();

        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);

        sky_shader_program->bind();

        // calculate combined view-projection matrix and its inverse
        Mat4 view = camera.get_view_matrix();
        Mat4 projection = camera.get_projection_matrix();
        Mat4 view_projection = projection * view;
        Mat4 inv_view_projection = view_projection.inverse_general_row_major();

        // pass the inverse view-projection matrix
        sky_shader_program->setUniformValue("inv_view_projection", inv_view_projection.to_qmatrix());

        // sky properties
        sky_shader_program->setUniformValue("sky_bottom_colour",
            QVector3D(sky.colour_bottom.r, sky.colour_bottom.g, sky.colour_bottom.b));
        sky_shader_program->setUniformValue("sky_top_colour",
            QVector3D(sky.colour_top.r, sky.colour_top.g, sky.colour_top.b));
        sky_shader_program->setUniformValue("sky_bottom_height", sky.bottom_height);
        sky_shader_program->setUniformValue("sky_top_height", sky.top_height);

        sky_vao.bind();
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        sky_vao.release();

        sky_shader_program->release();

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    void PanelViewport::paintGL() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!scene) return;
        
        if (scene) {
            const Sky& sky = scene->get_sky();
            glClearColor(sky.colour_bottom.r, sky.colour_bottom.g, sky.colour_bottom.b, 1.0f);

            render_sky_background();
        }

        rebuild_scene_geometry();

        shader_program->bind();

        Mat4 view = camera.get_view_matrix();
        Mat4 projection = camera.get_projection_matrix();
        Vec3 cam_pos = camera.get_pos();

        shader_program->setUniformValue("view", view.to_qmatrix());
        shader_program->setUniformValue("projection", projection.to_qmatrix());
        // TEMP
        // light pos is baked to cornell area light pos for now
        shader_program->setUniformValue("light_pos", QVector3D(2.775f, 2.775f, 5.54f));
        shader_program->setUniformValue("view_pos", QVector3D(cam_pos.x, cam_pos.y, cam_pos.z));

        vao.bind();

        // render each node. opaque first
        render_node(scene->get_root(), false);
        // then render the transparent ones
        glDepthMask(GL_FALSE);
        render_node(scene->get_root(), true);
        glDepthMask(GL_TRUE);

        vao.release();
        shader_program->release();
    }

    void PanelViewport::render_node(SceneNode* node, bool render_transparent) {
        if (!node || !node->visible) return;

        bool is_selected = selection_handler && (selection_handler->get_selected() == node);

        bool has_renderable_object = (node->primitive && (node->node_type == NodeType::Primitive || node->node_type == NodeType::Light)) ||
            (node->geo && node->node_type == NodeType::Mesh);

        if (has_renderable_object) {
            // is this a transparent object?
            bool is_transparent = (node->material.type == MaterialType::Dielectric);

            // skip if transparency doesn't match
            if (is_transparent != render_transparent) {
                //recurse to children
                for (auto& child : node->children) {
                    render_node(child.get(), render_transparent);
                }
                return;
            }

            // check if we have geo for this node
            auto it = geometry_ranges.find(node);
            if (it != geometry_ranges.end()) {
                const GeometryRange& range = it->second;

                Mat4 model = node->transform.to_matrix();
                Mat4 model_y_up = Mat4::swizzle_z_up_and_y_up() * model; //convert to openGL

                shader_program->setUniformValue("model", model.to_qmatrix());

                // set mat properties
                shader_program->setUniformValue("material_type", static_cast<int>(node->material.type));
                shader_program->setUniformValue("albedo",
                    QVector3D(node->material.albedo.r, node->material.albedo.g, node->material.albedo.b));
                shader_program->setUniformValue("emission",
                    QVector3D(node->material.emission.r, node->material.emission.g, node->material.emission.b));
                shader_program->setUniformValue("roughness", node->material.roughness);
                shader_program->setUniformValue("metallic", node->material.metallic);

                // chequerboard properties
                shader_program->setUniformValue("chequer_colour_a",
                    QVector3D(node->material.chequerboard_colour_a.r,
                        node->material.chequerboard_colour_a.g,
                        node->material.chequerboard_colour_a.b));
                shader_program->setUniformValue("chequer_colour_b",
                    QVector3D(node->material.chequerboard_colour_b.r,
                        node->material.chequerboard_colour_b.g,
                        node->material.chequerboard_colour_b.b));
                shader_program->setUniformValue("chequer_scale", node->material.chequerboard_scale);

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
            render_node(child.get(), render_transparent);
        }
    }

    void PanelViewport::position_toolbar() {
        if (!toolbar) return;

        // top-left with some padding for now
        int padding = 8;
        toolbar->move(padding, padding);
        toolbar->adjustSize();
        toolbar->raise();// on top!
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

                float aspect_ratio = float(width()) / float(height());
                float fov_rad = camera.get_fov_degs() * DEG_TO_RAD;
                float viewport_half_height = std::tan(fov_rad * 0.5f);
                float viewport_half_width = viewport_half_height * aspect_ratio;

                Vec3 ray_dir = forward + right * (x_ndc * viewport_half_width)
                                       + up * (y_ndc * viewport_half_height);
                ray_dir = ray_dir.normalised();

                //don't just select - utilise mode system
                if (edit_mode_manager) {
                    if (edit_mode_manager->is_object_mode()) {
                        selection_handler->raycast_select(scene, camera.get_pos(), ray_dir);
                    }
                    else {
                        //component edit mode - raycast against select object's geo
                        //TODO: implement vert/edge/face selection here
                        selection_handler->raycast_select(scene, camera.get_pos(), ray_dir);
                    }
                }
                else { // no edit_mode_manager
                    //fallback to object selection.  or maybe should fail more loudly?
                    selection_handler->raycast_select(scene, camera.get_pos(), ray_dir);
                }
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
            emit camera_moved();
            update();
        }
        else if (event->buttons() & Qt::MiddleButton) {
            if (event->modifiers() & Qt::ShiftModifier) {
                controller->zoom(delta.y() * -0.05f);
            }
            else {
                controller->pan(-delta.x(), delta.y());
            }
            emit camera_moved();
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
        emit camera_moved();
        update();
        event->accept();
    }

    void PanelViewport::resizeEvent(QResizeEvent* event) {
        QOpenGLWidget::resizeEvent(event);
        position_toolbar();
    }

} // namespace ollygon
