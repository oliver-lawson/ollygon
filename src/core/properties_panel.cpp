#include "properties_panel.hpp"
#include <QHBoxLayout>
#include <QMainWindow>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>

namespace ollygon {

PropertiesPanel::PropertiesPanel(SelectionHandler* selection, QWidget* parent)
    : QDockWidget("Properties", parent)
    , selection_handler(selection)
    , current_node(nullptr)
    , camera(nullptr)
    , camera_group(nullptr)
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    setMinimumWidth(230);

    content_widget = new QWidget();
    main_layout = new QVBoxLayout(content_widget);
    main_layout->setSpacing(8);
    main_layout->setContentsMargins(8, 8, 8, 8);
    setWidget(content_widget);

    // connect to selection changes
    if (selection_handler) {
        connect(selection_handler, &SelectionHandler::selection_changed,
            this, &PropertiesPanel::on_selection_changed);
    }

    rebuild_ui(nullptr); //first draw of properties, even with no selection
    return;
}

void PropertiesPanel::refresh_from_node() {
    if (!current_node) return;
    rebuild_ui(current_node); // could be more granular later, but our ui is very simple for now
}

void PropertiesPanel::refresh_camera_properties()
{
    if (!camera) return;
    
    // if camera group doesn't exist yet, do a full rebuild
    if (!camera_group) {
        rebuild_ui(current_node);
        return;
    }

    //update position,target, fov values
    // TEMP
    // this is a bit rough, we're rebuilding the whole cam section and
    // could be optimised to only update spinbox values later
    create_camera_controls(main_layout);
}

void PropertiesPanel::on_selection_changed(SceneNode* node) {
    current_node = node;
    rebuild_ui(node);
}

void PropertiesPanel::rebuild_ui(SceneNode* node) {
    // disconnect all camera connections before clearing
    for (auto& conn : camera_connections) {
        disconnect(conn);
    }
    camera_connections.clear();

    // clear existing widgets
    QLayoutItem* item;
    while ((item = main_layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    // important: camera_group was just deleted above, so null it out
    camera_group = nullptr;

    if (!node) {
        main_layout->addWidget(new QLabel("No selection"));
        main_layout->addStretch();

        //cam controls always at bottom for now
        if (camera) { create_camera_controls(main_layout); }
        return;
    }

    // node name
    QLabel* name_label = new QLabel(QString("<b>%1</b>").arg(QString::fromStdString(node->name)));
    name_label->setStyleSheet("font-size: 12pt;");
    main_layout->addWidget(name_label);

    // visible/locked checkboxes
    QCheckBox* visible_check = new QCheckBox("Visible");
    visible_check->setChecked(node->visible);
    connect(visible_check, &QCheckBox::toggled, [node, this](bool checked) {
        node->visible = checked;
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
        emit properties_changed();
    });
    main_layout->addWidget(visible_check);


    QCheckBox* lock_check = new QCheckBox("Locked");
    lock_check->setChecked(node->locked);
    connect(lock_check, &QCheckBox::toggled, [node, this](bool checked) {
        node->locked = checked;
        /// not sure if we'll need to emit this just yet?
        // have added "this" to lambda capture, clean that if we remove this
        emit properties_changed();
    });
    main_layout->addWidget(lock_check);

    // type-specific controls
    switch (node->node_type) {
    case NodeType::Mesh:
        create_transform_controls(node, main_layout);
        create_material_controls(node, main_layout);
        break;
    case NodeType::Primitive:
        create_transform_controls(node, main_layout);
        create_material_controls(node, main_layout);
        break;
    case NodeType::Light:
        create_transform_controls(node, main_layout);
        create_light_controls(node, main_layout);
        break;
    case NodeType::Empty:
        create_transform_controls(node, main_layout);
        break;
    }

    main_layout->addStretch();

    //cam controls always at bottom for now
    if (camera) { create_camera_controls(main_layout); }
}

void PropertiesPanel::create_transform_controls(SceneNode* node, QVBoxLayout* layout) {
    QGroupBox* transform_group = new QGroupBox("Transform");
    transform_group->setStyleSheet(
        "QGroupBox {"
        "    background-color: #191919;"
        "    border: 1px solid #2f2f2f;" //feels *just* embossed enough. #333 too much!
        "    border-radius: 5px;"
        "    margin-top: 1ex;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "    color: #777;"
        "}"
    );
    QGridLayout* grid = new QGridLayout(transform_group);
    grid->setSpacing(4);
    grid->setContentsMargins(7,10,7,7);

    add_vec3_row("Position", node->transform.position, -100.0f, 100.0f, 0.01f, grid, 0);
    add_vec3_row("Rotation", node->transform.rotation, -180.0f, 180.0f, 0.1f, grid, 1);
    add_vec3_row("Scale", node->transform.scale, 0.01f, 10.0f, 0.01f, grid, 2);
    //TEMP no rot until we support it

    layout->addWidget(transform_group);
}

void PropertiesPanel::create_material_controls(SceneNode* node, QVBoxLayout* layout) {
    QGroupBox* material_group = new QGroupBox("Material");
    material_group->setStyleSheet(
        "QGroupBox {"
        "    background-color: #191919;"
        "    border: 1px solid #2f2f2f;"
        "    border-radius: 5px;"
        "    margin-top: 1ex;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "    color: #777;"
        "}"
    );
    QGridLayout* grid = new QGridLayout(material_group);
    grid->setSpacing(4);
    grid->setContentsMargins(7, 10, 7, 7);

    int row = 0;

    //mat type dropdown
    QLabel* type_label = new QLabel("Type");
    type_label->setMinimumWidth(60);
    grid->addWidget(type_label, row, 0);

    QComboBox* type_combo = new QComboBox();
    type_combo->addItem("Lambertian", static_cast<int>(MaterialType::Lambertian));
    type_combo->addItem("Metal", static_cast<int>(MaterialType::Metal));
    type_combo->addItem("Dielectric", static_cast<int>(MaterialType::Dielectric));
    type_combo->addItem("Emissive", static_cast<int>(MaterialType::Emissive));
    type_combo->addItem("Chequerboard", static_cast<int>(MaterialType::Chequerboard));

    type_combo->setCurrentIndex(static_cast<int>(node->material.type));

    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
        [node, this](int index) {
            node->material.type = static_cast<MaterialType>(index);
            // doing it this way to defer ui rebuild until after signal completes, 
            // otherwise was getting deleted memory reads from QComboBox being destroyed whist
            // still in its signal handler.  bc of rebuild_ui happening deleting all widgets
            // including this one sending the signal. seems to be safe
            // with Qt::QueuedConnection
            QMetaObject::invokeMethod(this, [node, this]() {
                rebuild_ui(node);
                if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
                    if (auto* viewport = main_win->centralWidget()) {
                        viewport->update();
                    }
                }
            }, Qt::QueuedConnection);
        });
    grid->addWidget(type_combo, row++, 1, 1, 3);

    // albedo (for most material types)
    if (node->material.type != MaterialType::Dielectric) {
        add_colour_row("Albedo", node->material.albedo, 0.0f, 1.0f, 0.01f, grid, row++);

        // sync with legacy albedo field TEMP
        connect(this, &PropertiesPanel::properties_changed, [node]() {
            node->albedo = node->material.albedo;
            });
    }

    // emission
    if (node->material.type == MaterialType::Emissive) {
        add_colour_row("Emission", node->material.emission, 0.0f, 50.0f, 0.1f, grid, row++);
    }

    // metal roughness
    if (node->material.type == MaterialType::Metal) {
        add_float_row("Roughness", node->material.roughness, 0.0f, 1.0f, 0.01f, grid, row++);
    }

    // IOR
    if (node->material.type == MaterialType::Dielectric) {
        add_float_row("IOR", node->material.ior, 1.0f, 3.0f, 0.01f, grid, row++);
    }

    // chequerboard specific
    if (node->material.type == MaterialType::Chequerboard) {
        add_colour_row("Colour A", node->material.chequerboard_colour_a, 0.0f, 1.0f, 0.01f, grid, row++);
        add_colour_row("Colour B", node->material.chequerboard_colour_b, 0.0f, 1.0f, 0.01f, grid, row++);
        add_float_row("Scale", node->material.chequerboard_scale, 0.1f, 20.0f, 0.1f, grid, row++);
    }

    layout->addWidget(material_group);
}

void PropertiesPanel::create_light_controls(SceneNode* node, QVBoxLayout* layout) {
    if (!node->light) return;

    QGroupBox* light_group = new QGroupBox("Light");
    light_group->setStyleSheet(
        "QGroupBox {"
        "    background-color: #191919;"
        "    border: 1px solid #2f2f2f;"
        "    border-radius: 5px;"
        "    margin-top: 1ex;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "    color: #777;"
        "}"
    );
    QGridLayout* grid = new QGridLayout(light_group);
    grid->setSpacing(4);
    grid->setContentsMargins(7, 10, 7, 7);

    QString type_str;
    switch (node->light->type) {
    case LightType::Point: type_str = "Point"; break;
    case LightType::Directional: type_str = "Directional"; break;
    case LightType::Area: type_str = "Area"; break;
    }
    QLabel* type_label = new QLabel("Type: " + type_str);
    grid->addWidget(type_label, 0, 0, 1, 4);

    add_colour_row("Colour", node->light->colour, 0.0f, 50.0f, 0.1f, grid, 1);
    add_float_row("Intensity", node->light->intensity, 0.0f, 100.0f, 0.1f, grid, 2);

    layout->addWidget(light_group);
}

void PropertiesPanel::create_camera_controls(QVBoxLayout* layout)
{
    if (!camera) return;

    // remove old camera group if it exists
    if (camera_group) {
        layout->removeWidget(camera_group);
        delete camera_group;
        camera_group = nullptr;
    }

    camera_group = new QGroupBox("Camera");
    camera_group->setStyleSheet(
        "QGroupBox {"
        "    background-color: #191919;"
        "    border: 1px solid #2f2f2f;"
        "    border-radius: 5px;"
        "    margin-top: 1ex;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "    color: #777;"
        "}"
    );
    QGridLayout* grid = new QGridLayout(camera_group);
    grid->setSpacing(4);
    grid->setContentsMargins(7, 10, 7, 7);

    CameraController* controller = camera->get_controller();
    int row = 0;

    // position TEMP make editable
    Vec3 cam_pos = camera->get_pos();
    QLabel* pos_label = new QLabel("Position");
    pos_label->setMinimumWidth(60);
    grid->addWidget(pos_label, row, 0);

    QLabel* pos_value = new QLabel(QString("X: %1  Y: %2  Z: %3")
        .arg(cam_pos.x, 0, 'f', 2)
        .arg(cam_pos.y, 0, 'f', 2)
        .arg(cam_pos.z, 0, 'f', 2));
    pos_value->setStyleSheet("color: #999; font-size: 9pt;");
    grid->addWidget(pos_value, row++, 1, 1, 3);

    // target
    Vec3 target = controller->get_target();
    QLabel* target_label = new QLabel("Target");
    target_label->setMinimumWidth(60);
    grid->addWidget(target_label, row, 0);

    QWidget* target_wrapper = new QWidget();
    target_wrapper->setMinimumHeight(24);
    target_wrapper->setMaximumHeight(24);

    DragSpinBox* target_x = new DragSpinBox(target_wrapper);
    target_x->set_range(-100.0f, 100.0f);
    target_x->set_speed(0.01f);
    target_x->set_value(target.x);
    target_x->set_letter(SpinBoxLetter::X);
    camera_connections.push_back(connect(target_x, &DragSpinBox::value_changed, [controller, this](float value) {
        Vec3 new_target = controller->get_target();
        new_target.x = value;
        controller->set_target(new_target);
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
    }));

    DragSpinBox* target_y = new DragSpinBox(target_wrapper);
    target_y->set_range(-100.0f, 100.0f);
    target_y->set_speed(0.01f);
    target_y->set_value(target.y);
    target_y->set_letter(SpinBoxLetter::Y);
    camera_connections.push_back(connect(target_y, &DragSpinBox::value_changed, [controller, this](float value) {
        Vec3 new_target = controller->get_target();
        new_target.y = value;
        controller->set_target(new_target);
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
    }));

    DragSpinBox* target_z = new DragSpinBox(target_wrapper);
    target_z->set_range(-100.0f, 100.0f);
    target_z->set_speed(0.01f);
    target_z->set_value(target.z);
    target_z->set_letter(SpinBoxLetter::Z);
    camera_connections.push_back(connect(target_z, &DragSpinBox::value_changed, [controller, this](float value) {
        Vec3 new_target = controller->get_target();
        new_target.z = value;
        controller->set_target(new_target);
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
    }));

    BorderOverlay* target_overlay = new BorderOverlay(target_wrapper);
    target_wrapper->installEventFilter(new ResizeFilter());
    grid->addWidget(target_wrapper, row++, 1, 1, 3);

    // orbit parameters (distance, yaw, pitch)
    QLabel* dist_label = new QLabel("Distance");
    dist_label->setMinimumWidth(60);
    grid->addWidget(dist_label, row, 0);

    DragSpinBox* dist_spin = new DragSpinBox();
    dist_spin->set_range(0.01f, 1000.0f);
    dist_spin->set_speed(0.05f);
    dist_spin->set_value(controller->get_distance());
    camera_connections.push_back(connect(dist_spin, &DragSpinBox::value_changed, [controller, this](float value) {
        controller->set_orbit_params(
            controller->get_yaw(),
            controller->get_pitch(),
            value
        );
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
    }));
    grid->addWidget(dist_spin, row++, 1, 1, 3);

    // yaw
    QLabel* yaw_label = new QLabel("Yaw");
    yaw_label->setMinimumWidth(60);
    grid->addWidget(yaw_label, row, 0);

    DragSpinBox* yaw_spin = new DragSpinBox();
    yaw_spin->set_range(-180.0f, 180.0f);
    yaw_spin->set_speed(0.5f);
    yaw_spin->set_value(controller->get_yaw());
    camera_connections.push_back(connect(yaw_spin, &DragSpinBox::value_changed, [controller, this](float value) {
        controller->set_orbit_params(
            value,
            controller->get_pitch(),
            controller->get_distance()
        );
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
    }));
    grid->addWidget(yaw_spin, row++, 1, 1, 3);

    // pitch
    QLabel* pitch_label = new QLabel("Pitch");
    pitch_label->setMinimumWidth(60);
    grid->addWidget(pitch_label, row, 0);

    DragSpinBox* pitch_spin = new DragSpinBox();
    pitch_spin->set_range(-89.0f, 89.0f);
    pitch_spin->set_speed(0.5f);
    pitch_spin->set_value(controller->get_pitch());
    camera_connections.push_back(connect(pitch_spin, &DragSpinBox::value_changed, [controller, this](float value) {
        controller->set_orbit_params(
            controller->get_yaw(),
            value,
            controller->get_distance()
        );
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
        }));
    grid->addWidget(pitch_spin, row++, 1, 1, 3);

    // FOV
    QLabel* fov_label = new QLabel("FOV");
    fov_label->setMinimumWidth(60);
    grid->addWidget(fov_label, row, 0);

    DragSpinBox* fov_spin = new DragSpinBox();
    fov_spin->set_range(1.0f, 179.0f);
    fov_spin->set_speed(0.5f);
    fov_spin->set_value(camera->get_fov());
    camera_connections.push_back(connect(fov_spin, &DragSpinBox::value_changed, [this](float value) {
        camera->set_fov(value);
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
        }));
    grid->addWidget(fov_spin, row++, 1, 1, 3);

    layout->addWidget(camera_group);
}

void PropertiesPanel::add_vec3_row(const QString& label, Vec3& vec, float min_val, float max_val, float speed, QGridLayout* grid, int row) {
    QLabel* label_widget = new QLabel(label);
    label_widget->setMinimumWidth(50);
    grid->addWidget(label_widget, row, 0);
    grid->setHorizontalSpacing(0);

    QWidget* wrapper = new QWidget();
    wrapper->setMinimumHeight(24);
    wrapper->setMaximumHeight(24);

    DragSpinBox* x_box = new DragSpinBox(wrapper);
    x_box->set_range(min_val, max_val);
    x_box->set_speed(speed);
    x_box->set_value(vec.x);
    x_box->set_letter(SpinBoxLetter::X);
    connect(x_box, &DragSpinBox::value_changed, [&vec, this](float value) {
        vec.x = value;
        // trigger viewport update
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
        emit properties_changed();
    });

    DragSpinBox* y_box = new DragSpinBox(wrapper);
    y_box->set_range(min_val, max_val);
    y_box->set_speed(speed);
    y_box->set_value(vec.y);
    y_box->set_letter(SpinBoxLetter::Y);
    connect(y_box, &DragSpinBox::value_changed, [&vec, this](float value) {
        vec.y = value;
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
        emit properties_changed();
    });

    DragSpinBox* z_box = new DragSpinBox(wrapper);
    z_box->set_range(min_val, max_val);
    z_box->set_speed(speed);
    z_box->set_value(vec.z);
    z_box->set_letter(SpinBoxLetter::Z);
    connect(z_box, &DragSpinBox::value_changed, [&vec, this](float value) {
        vec.z = value;
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
        emit properties_changed();
    });

    BorderOverlay* overlay = new BorderOverlay(wrapper);
    wrapper->installEventFilter(new ResizeFilter());

    grid->addWidget(wrapper, row, 1, 1, 3);
}

void PropertiesPanel::add_colour_row(const QString& label, Colour& colour, float min_val, float max_val, float speed, QGridLayout* grid, int row) {
    QLabel* label_widget = new QLabel(label);
    label_widget->setMinimumWidth(50);
    grid->addWidget(label_widget, row, 0);
    grid->setHorizontalSpacing(0);

    QWidget* wrapper = new QWidget();
    wrapper->setMinimumHeight(24);
    wrapper->setMaximumHeight(24);

    DragSpinBox* r_box = new DragSpinBox(wrapper);
    r_box->set_range(min_val, max_val);
    r_box->set_speed(speed);
    r_box->set_value(colour.r);
    r_box->set_letter(SpinBoxLetter::R);
    connect(r_box, &DragSpinBox::value_changed, [&colour, this](float value) {
        colour.r = value;
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
        emit properties_changed();
        });

    DragSpinBox* g_box = new DragSpinBox(wrapper);
    g_box->set_range(min_val, max_val);
    g_box->set_speed(speed);
    g_box->set_value(colour.g);
    g_box->set_letter(SpinBoxLetter::G);
    connect(g_box, &DragSpinBox::value_changed, [&colour, this](float value) {
        colour.g = value;
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
        emit properties_changed();
        });

    DragSpinBox* b_box = new DragSpinBox(wrapper);
    b_box->set_range(min_val, max_val);
    b_box->set_speed(speed);
    b_box->set_value(colour.b);
    b_box->set_letter(SpinBoxLetter::B);
    connect(b_box, &DragSpinBox::value_changed, [&colour, this](float value) {
        colour.b = value;
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
        emit properties_changed();
        });

    BorderOverlay* overlay = new BorderOverlay(wrapper);
    wrapper->installEventFilter(new ResizeFilter());

    grid->addWidget(wrapper, row, 1, 1, 3);
}

void PropertiesPanel::add_float_row(const QString& label, float& value, float min_val, float max_val, float speed, QGridLayout* grid, int row) {
    QLabel* label_widget = new QLabel(label);
    label_widget->setMinimumWidth(60);
    grid->addWidget(label_widget, row, 0);

    DragSpinBox* spin_box = new DragSpinBox();
    spin_box->set_range(min_val, max_val);
    spin_box->set_speed(speed);
    spin_box->set_value(value);
    connect(spin_box, &DragSpinBox::value_changed, [&value, this](float new_value) {
        value = new_value;
        if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
            if (auto* viewport = main_win->centralWidget()) {
                viewport->update();
            }
        }
        emit properties_changed();
        });
    grid->addWidget(spin_box, row, 1, 1, 3);  // span 3 columns
}

} // namespace ollygon
