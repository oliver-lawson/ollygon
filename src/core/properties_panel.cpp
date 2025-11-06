#include "properties_panel.hpp"
#include <QHBoxLayout>
#include <QMainWindow>
#include <QGroupBox>
#include <QCheckBox>

namespace ollygon {

PropertiesPanel::PropertiesPanel(SelectionHandler* selection, QWidget* parent)
    : QDockWidget("Properties", parent)
    , selection_handler(selection)
    , current_node(nullptr)
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

    main_layout->addWidget(new QLabel("No selection"));
    main_layout->addStretch();
}

void PropertiesPanel::refresh_from_node() {
    if (!current_node) return;
    rebuild_ui(current_node); // could be more granular later, but our ui is very simple for now
}

void PropertiesPanel::on_selection_changed(SceneNode* node) {
    current_node = node;
    rebuild_ui(node);
}

void PropertiesPanel::rebuild_ui(SceneNode* node) {
    // clear existing widgets
    QLayoutItem* item;
    while ((item = main_layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (!node) {
        main_layout->addWidget(new QLabel("No selection"));
        main_layout->addStretch();
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
        create_mesh_controls(node, main_layout);
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
    add_vec3_row("Scale", node->transform.scale, 0.01f, 10.0f, 0.01f, grid, 1);
    //TEMP no rot until we support it

    layout->addWidget(transform_group);
}

void PropertiesPanel::create_mesh_controls(SceneNode* node, QVBoxLayout* layout) {
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

    add_colour_row("Albedo", node->albedo, 0.0f, 1.0f, 0.01f, grid, 0);

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
        });
    grid->addWidget(spin_box, row, 1, 1, 3);  // span 3 columns
}

} // namespace ollygon