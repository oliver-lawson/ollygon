#pragma once

#include <QMainWindow>
#include <QDockWidget>
#include <QTreeWidget>
#include "core/scene.hpp"
#include "core/selection_handler.hpp"
#include "panel_scene_hierarchy.hpp"

namespace ollygon {

class PanelViewport;
class PropertiesPanel;
class PanelSceneHierarchy;

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

    Scene scene;
    SelectionHandler selection_handler;

    PanelViewport* viewport;
    PropertiesPanel* properties_panel;
    QDockWidget* scene_dock;
    PanelSceneHierarchy* scene_hierarchy;
};

} // namespace ollygon
