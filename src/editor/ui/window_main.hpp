#pragma once

#include <QMainWindow>
#include <QDockWidget>
#include "core/scene.hpp"

namespace ollygon {

class PanelViewport;

class MainWindow : public QMainWindow {
    Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow() override;

    private:
        void setup_ui();
        void create_dock_widgets();
        void create_menus();

        Scene scene;
        PanelViewport* viewport;
        QDockWidget* properties_dock;
        QDockWidget* scene_dock;
    };

} // namespace ollygon
