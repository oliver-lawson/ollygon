#pragma once

#include <QDockWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include "scene.hpp"
#include "selection_handler.hpp"
#include "drag_spin_box.hpp"

namespace ollygon {

class PropertiesPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit PropertiesPanel(SelectionHandler* selection, QWidget* parent = nullptr);

private slots:
    void on_selection_changed(SceneNode* node);

private:
    void rebuild_ui(SceneNode* node);
    void create_transform_controls(SceneNode* node, QVBoxLayout* layout);
    void create_mesh_controls(SceneNode* node, QVBoxLayout* layout);
    void create_light_controls(SceneNode* node, QVBoxLayout* layout);

    void add_vec3_row(const QString& label, Vec3& vec, float min_val, float max_val, float speed, QGridLayout* grid, int row);

    void add_colour_row(const QString& label, Colour& colour, float min_val, float max_val, float speed, QGridLayout* grid, int row);

    void add_float_row(const QString& label, float& value, float min_val, float max_val, float speed, QGridLayout* grid, int row);

    SelectionHandler* selection_handler;
    QWidget* content_widget;
    QVBoxLayout* main_layout;
    SceneNode* current_node;
};

} // namespace ollygon