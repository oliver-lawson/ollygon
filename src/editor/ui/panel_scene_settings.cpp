#include "panel_scene_settings.hpp"
#include "core/properties_panel.hpp"
#include <QMainWindow>

namespace ollygon {

PanelSceneSettings::PanelSceneSettings(QWidget* parent)
    : QDockWidget("Scene Settings", parent)
    , scene(nullptr)
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumWidth(230);

    content_widget = new QWidget();
    main_layout = new QVBoxLayout(content_widget);
    main_layout->setSpacing(8);
    main_layout->setContentsMargins(8, 8, 8, 8);
    setWidget(content_widget);

    rebuild_ui();
}

void PanelSceneSettings::set_scene(Scene* new_scene)
{
    scene = new_scene;
    rebuild_ui();
}

void PanelSceneSettings::refresh_ui()
{
    rebuild_ui();
}

void PanelSceneSettings::rebuild_ui()
{
    // clear existing widgets
    QLayoutItem* item;
    while ((item = main_layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (!scene) {
        main_layout->addWidget(new QLabel("No scene loaded"));
        main_layout->addStretch();
        return;
    }

    create_sky_controls(main_layout);
    
    main_layout->addStretch();
}

void PanelSceneSettings::create_sky_controls(QVBoxLayout* layout)
{
    QGroupBox* sky_group = new QGroupBox("Sky");
    sky_group->setStyleSheet(
        //TODO: unify stylesheets externally
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
    QGridLayout* grid = new QGridLayout(sky_group);
    grid->setSpacing(4);
    grid->setContentsMargins(7, 10, 7, 7);

    int row = 0;

    // preset dropdown
    QLabel* preset_label = new QLabel("Preset");
    preset_label->setMinimumWidth(60);
    grid->addWidget(preset_label, row, 0);
    QComboBox* preset_combo = new QComboBox();
    preset_combo->addItem("Default", 0);
    preset_combo->addItem("Cornell Dark", 1);
    preset_combo->addItem("Sunset", 2);
    preset_combo->addItem("Custom", 3);
    preset_combo->setCurrentIndex(0);

    connect(preset_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [this](int index) {
            if (!scene) return;

            switch (index) {
            case 0:
                scene->get_sky() = Sky::default_sky();
                break;
            case 1:
                scene->get_sky() = Sky::cornell_dark();
                break;
            case 2:
                scene->get_sky() = Sky::sunset();
                break;
            case 3:
                // custom - keep current values
                break;
            }

            // immediately update viewport
            if (auto* main_win = qobject_cast<QMainWindow*>(window())) {
                if (auto* viewport = main_win->centralWidget()) {
                    viewport->update();
                }
            }
            emit settings_changed();

            // defer UI rebuild to avoid deleting combo during signal
            QMetaObject::invokeMethod(this, [this]() {
                rebuild_ui();
                }, Qt::QueuedConnection);
        });
    grid->addWidget(preset_combo, row++, 1, 1, 3);

    add_colour_row("Bottom Colour", scene->get_sky().colour_bottom, 0.0f, 1.0f, 0.01f, grid, row++);
    add_colour_row("Top Colour", scene->get_sky().colour_top, 0.0f, 1.0f, 0.01f, grid, row++);
    add_float_row("Bottom Height", scene->get_sky().bottom_height, 0.0f, 1.0f, 0.01f, grid, row++);
    add_float_row("Top Height", scene->get_sky().top_height, 0.0f, 1.0f, 0.01f, grid, row++);

    layout->addWidget(sky_group);
}

void PanelSceneSettings::add_colour_row(
    const QString& label,
    Colour& colour,
    float min_val, float max_val, float speed,
    QGridLayout* grid, int row)
{
    QLabel* label_widget = new QLabel(label);
    label_widget->setMinimumWidth(60);
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
        emit settings_changed();
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
        emit settings_changed();
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
        emit settings_changed();
    });

    BorderOverlay* overlay = new BorderOverlay(wrapper);
    wrapper->installEventFilter(new ResizeFilter());

    grid->addWidget(wrapper, row, 1, 1, 3);
}

void PanelSceneSettings::add_float_row(
    const QString& label,
    float& value,
    float min_val, float max_val, float speed,
    QGridLayout* grid, int row)
{
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
        emit settings_changed();
    });
    grid->addWidget(spin_box, row, 1, 1, 3);
}

} // namespace ollygon
