#pragma once

#include <QMainWindow>
#include <QDockWidget>
#include <QTreeWidget>
#include "core/scene.hpp"
#include "core/selection_handler.hpp"

namespace ollygon {

class PanelViewport;
class PropertiesPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void setup_ui();
    void setup_cornell_box();
    void create_dock_widgets();
    void create_menus();
    void populate_scene_tree();

    Scene scene;
    SelectionHandler selection_handler;

    PanelViewport* viewport;
    PropertiesPanel* properties_panel;
    QDockWidget* scene_dock;
    QTreeWidget* scene_tree;
};

} // namespace ollygon