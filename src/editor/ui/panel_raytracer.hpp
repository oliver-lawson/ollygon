#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QElapsedTimer>
#include <QImage>
#include <QComboBox> //TODO maybe forward dec some of these..
#include "core/scene.hpp"
#include "core/camera.hpp"
#include "okaytracer/raytracer.hpp"

namespace ollygon {

// for now, a literal pop-out window.  TODO: experiment with it being a dockable panel?
class RaytracerWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit RaytracerWindow(QWidget* parent = nullptr);
    ~RaytracerWindow() override;

    void set_scene(const Scene* scene);
    void set_camera(const Camera* camera);

    void start_render();
    void stop_render();

private slots:
    void on_render_clicked();
    void on_stop_clicked();
    void update_render();

private:
    void setup_ui();
    void update_display();

    const Scene* scene;
    const Camera* camera;

    okaytracer::Raytracer raytracer;
    okaytracer::RenderConfig render_config;

    QLabel* image_label;
    QImage display_image;

    QPushButton* render_button;
    QPushButton* stop_button;
    QSpinBox* samples_spinbox;
    QSpinBox* bounces_spinbox;
    QSpinBox* width_spinbox;
    QSpinBox* height_spinbox;
    QLabel* progress_label;
    QComboBox* backend_combo;

    QTimer* update_timer;

    QElapsedTimer render_timer;
    QLabel* time_label;
};

} // namespace ollygon
