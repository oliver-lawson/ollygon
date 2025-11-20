#pragma once

#include <QDockWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include "core/scene.hpp"
#include "core/drag_spin_box.hpp"

namespace ollygon {

// forward decs
class BorderOverlay;
class ResizeFilter;

class PanelSceneSettings : public QDockWidget {
    Q_OBJECT

public:
    explicit PanelSceneSettings(QWidget* parent = nullptr);
    
    void set_scene(Scene* new_scene);
    void refresh_ui();

signals:
    void settings_changed();

private:
    void rebuild_ui();
    void create_sky_controls(QVBoxLayout* layout);
    
    void add_colour_row(
        const QString& label,
        Colour& colour,
        float min_val, float max_val, float speed,
        QGridLayout* grid, int row
    );
    
    void add_float_row(
        const QString& label,
        float& value,
        float min_val, float max_val, float speed,
        QGridLayout* grid, int row
    );
    
    Scene* scene;
    QWidget* content_widget;
    QVBoxLayout* main_layout;
};

} // namespace ollygon
