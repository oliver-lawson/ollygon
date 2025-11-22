#pragma once

#include <QMainWindow>
#include <QDockWidget>
#include <QTreeWidget>
#include "core/scene.hpp"
#include "core/selection_handler.hpp"
#include "panel_scene_hierarchy.hpp"
#include "panel_scene_settings.hpp"
#include "toolbar_edit_mode.hpp"

namespace ollygon {

class PanelViewport;
class PropertiesPanel;
class PanelSceneHierarchy;
class RaytracerWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void setup_ui();
    void setup_scene_cornell_box();
    void setup_scene_stress_test();
    void create_dock_widgets();
    void create_menus();
    void setup_shortcuts();
    void on_delete_pressed();

    void save_scene();
    void save_scene_as();
    void load_scene();

    void refresh_scene_ui();
    
    void show_raytracer_window();

    Scene scene;
    SelectionHandler selection_handler;
    EditModeManager edit_mode_manager;

    PanelViewport* viewport;
    PropertiesPanel* properties_panel;
    QDockWidget* scene_dock;
    PanelSceneHierarchy* scene_hierarchy;
    RaytracerWindow* raytracer_window;
    PanelSceneSettings* scene_settings_panel;

    QString current_filepath;
};

} // namespace ollygon
