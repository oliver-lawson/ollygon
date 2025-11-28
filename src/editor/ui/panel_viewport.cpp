#include "panel_viewport.hpp"
#include <QOpenGLShader>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QPainter>
#include <cmath>
#include <iostream>
#include "panel_scene_hierarchy.hpp"
#include "core/selection_system.hpp"

namespace ollygon {

    PanelViewport::PanelViewport(QWidget* parent)
        : QOpenGLWidget(parent)
        , scene(nullptr)
        , shader_program(nullptr)
        , vbo(QOpenGLBuffer::VertexBuffer)
        , ebo(QOpenGLBuffer::IndexBuffer)
        , selection_handler(nullptr)
        , selection_system(nullptr)
        , edit_mode_manager(nullptr)
        , geometry_dirty(true)
        , is_camera_dragging(false)
        , toolbar_edit_mode(nullptr)
        , toolbar_selection_mode(nullptr)
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
            connect(handler, &SelectionHandler::component_selection_changed,
                this, [this]() { update(); });
        }
    }

    void PanelViewport::set_edit_mode_manager(EditModeManager* manager) {
        edit_mode_manager = manager;

        if (edit_mode_manager && selection_handler && !selection_system) {
            selection_system = new SelectionSystem(this);
            selection_system->set_selection_handler(selection_handler);
            selection_system->set_edit_mode_manager(edit_mode_manager);

            connect(selection_system, &SelectionSystem::box_select_state_changed,
                this, [this]() { update(); });
        }

        // create toolbar if we don't have one yet
        if (!toolbar_edit_mode && manager) {
            toolbar_edit_mode = new ToolbarEditMode(manager, selection_handler, this);
            toolbar_edit_mode->setStyleSheet(
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
        }

        if (!toolbar_selection_mode && selection_system) {
            toolbar_selection_mode = new ToolbarSelectionMode(selection_system, this);
            toolbar_selection_mode->setStyleSheet(toolbar_edit_mode->styleSheet());
        }

        position_toolbars();
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

        // == component highlight shader ==

        component_shader_program = new QOpenGLShaderProgram(this);

        const char* component_vertex_shader = R"(
        #version 330 core
        layout(location = 0) in vec3 position;
        
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        
        void main() {
            gl_Position = projection * view * model * vec4(position, 1.0);
        }
    )";

        const char* component_fragment_shader = R"(
        #version 330 core
        out vec4 FragColor;
        
        uniform vec3 highlight_colour;
        
        void main() {
            FragColor = vec4(highlight_colour, 1.0);
        }
    )";

        component_shader_program->addShaderFromSourceCode(
            QOpenGLShader::Vertex, component_vertex_shader);
        component_shader_program->addShaderFromSourceCode(
            QOpenGLShader::Fragment, component_fragment_shader);
        component_shader_program->link();

        component_vao.create();
        component_vbo.create();

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

    // == edit draws ==

    void PanelViewport::render_component_selection()
    {
        if (!selection_handler || !edit_mode_manager) return;

        SceneNode* selected = selection_handler->get_selected_node();
        if (!selected || selected->node_type != NodeType::Mesh || !selected->geo) {
            return;
        }

        const ComponentSelection& comp_sel = selection_handler->get_component_selection();
        if (comp_sel.is_empty()) return;

        Mat4 model = selected->transform.to_matrix();
        Mat4 view = camera.get_view_matrix();
        Mat4 projection = camera.get_projection_matrix();

        component_shader_program->bind();
        component_shader_program->setUniformValue("model", model.to_qmatrix());
        component_shader_program->setUniformValue("view", view.to_qmatrix());
        component_shader_program->setUniformValue("projection", projection.to_qmatrix());

        EditMode mode = edit_mode_manager->get_mode();

        QVector3D colour;
        switch (mode) {
        case EditMode::Vertex:
            colour = QVector3D(1.0f, 0.7f, 0.0f);
            break;
        case EditMode::Edge:
            colour = QVector3D(1.0f, 1.0f, 0.0f);
            break;
        case EditMode::Face:
            colour = QVector3D(0.2f, 0.77f, 1.0f);
            break;
        default:
            colour = QVector3D(1.0f, 1.0f, 1.0f);
            break;
        }
        component_shader_program->setUniformValue("highlight_colour", colour);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (mode == EditMode::Vertex) {
            render_selected_vertices(selected->geo.get(), comp_sel.vertices);
        }
        else if (mode == EditMode::Edge) {
            render_selected_edges(selected->geo.get(), comp_sel.edges);
        }
        else if (mode == EditMode::Face) {
            render_selected_faces(selected->geo.get(), comp_sel.faces);
        }

        glEnable(GL_DEPTH_TEST);
        component_shader_program->release();
    }

    void PanelViewport::render_selected_vertices( const Geo* geo, const std::unordered_set<uint32_t>& selected_verts )
    {
        if (selected_verts.empty()) return;

        std::vector<float> points;
        for (uint32_t idx : selected_verts) {
            if (idx < geo->verts.size()) {
                const Vec3& pos = geo->verts[idx].position;
                points.push_back(pos.x); points.push_back(pos.y); points.push_back(pos.z);
            }
        }

        component_vao.bind();
        component_vbo.bind();
        component_vbo.allocate(points.data(), points.size() * sizeof(float));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

        glPointSize(10.0f);
        //TODO: figure a better size when we have complex meshes imported
        glDrawArrays(GL_POINTS, 0, points.size() / 3);

        component_vao.release();
    }

    void PanelViewport::render_selected_edges( const Geo* geo, const std::unordered_set<uint32_t>& selected_edges )
    {
        if (selected_edges.empty()) return;

        std::vector<float> lines;
        uint32_t vertex_count = geo->vertex_count();

        for (uint32_t hash : selected_edges) {
            uint32_t v1 = hash / vertex_count;
            uint32_t v2 = hash % vertex_count;

            if (v1 < geo->verts.size() && v2 < geo->verts.size()) {
                const Vec3& p1 = geo->verts[v1].position;
                const Vec3& p2 = geo->verts[v2].position;

                lines.push_back(p1.x); lines.push_back(p1.y); lines.push_back(p1.z);
                lines.push_back(p2.x); lines.push_back(p2.y); lines.push_back(p2.z);
            }
        }

        component_vao.bind();
        component_vbo.bind();
        component_vbo.allocate(lines.data(), lines.size() * sizeof(float));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

        glLineWidth(3.0f);
        glDrawArrays(GL_LINES, 0, lines.size() / 3);

        component_vao.release();
    }

    void PanelViewport::render_selected_faces( const Geo* geo, const std::unordered_set<uint32_t>& selected_faces )
    {
        if (selected_faces.empty()) return;

        std::vector<float> tris;

        for (uint32_t face_idx : selected_faces) {
            uint32_t base = face_idx * 3;
            if (base + 2 < geo->indices.size()) {
                for (int i = 0; i < 3; ++i) {
                    uint32_t v_idx = geo->indices[base + i];
                    if (v_idx < geo->verts.size()) {
                        const Vec3& pos = geo->verts[v_idx].position;
                        tris.push_back(pos.x);
                        tris.push_back(pos.y);
                        tris.push_back(pos.z);
                    }
                }
            }
        }

        component_vao.bind();
        component_vbo.bind();
        component_vbo.allocate(tris.data(), tris.size() * sizeof(float));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

        //TODO: add transparency param?
        component_shader_program->setUniformValue("highlight_colour", QVector3D(0.0f, 0.8f, 1.0f));
        glDrawArrays(GL_TRIANGLES, 0, tris.size() / 3);

        component_vao.release();
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

        // render extras on top
        render_component_selection();
        render_box_select_overlay();
    }

    void PanelViewport::render_node(SceneNode* node, bool render_transparent) {
        if (!node || !node->visible) return;

        bool is_selected = selection_handler && selection_handler->is_selected(node);

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

    void PanelViewport::render_box_select_overlay() {
        if (!selection_system || !selection_system->is_box_selecting()) return;

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        QPainter painter(this);
        painter.beginNativePainting();
        painter.endNativePainting();//reset GL state for QPainter

        painter.setRenderHint(QPainter::Antialiasing);

        QRect box = selection_system->get_box_select_rect();

        // transparent fill
        painter.fillRect(box, QColor(255, 140, 0, 30));
        // border
        painter.setPen(QPen(QColor(255, 140, 0), 2));
        painter.drawRect(box);

        painter.end();

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    void PanelViewport::position_toolbars() {
        if (!toolbar_edit_mode || !toolbar_selection_mode) return;

        int padding = 8;
        int y_pos = padding;

        // edit mode toolbar at top
        toolbar_edit_mode->move(padding, y_pos);
        toolbar_edit_mode->raise();

        // selection mode toolbar below it
        y_pos += toolbar_edit_mode->height() + 4;
        toolbar_selection_mode->move(padding, y_pos);
        toolbar_selection_mode->raise();
    }

    void PanelViewport::mousePressEvent(QMouseEvent* event) {
        if (!scene) return;

        last_mouse_pos = event->pos();

        // left mouse = selection
        if (event->button() == Qt::LeftButton && selection_system)
        {
            selection_system->handle_mouse_press(
                scene, camera, event->pos(), width(), height(), event->modifiers()
            );
            return;
        }

        // right/middle mouse = camera
        if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
            is_camera_dragging = true;
            setCursor(Qt::ClosedHandCursor);
        }
    }

    void PanelViewport::mouseMoveEvent(QMouseEvent* event)
    {
        if (selection_system && selection_system->is_box_selecting()) {
            selection_system->handle_mouse_move(scene, camera, event->pos(), width(), height());
            update();
            return;
        }

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
        if (event->button() == Qt::LeftButton && selection_system) {
            selection_system->handle_mouse_release(
                scene, camera, event->pos(), width(), height()
            );
            return;
        }

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
        position_toolbars();
    }

} // namespace ollygon
