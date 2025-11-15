#include "panel_raytracer.hpp"
#include "okaytracer/render_scene.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QColorSpace>
#include <cmath>

namespace ollygon {

RaytracerWindow::RaytracerWindow(QWidget* parent)
    : QMainWindow(parent)
    , scene(nullptr)
    , camera(nullptr)
{
    setWindowTitle("ollygon - Raytracer");

    //defaults
    render_config.width = 600;
    render_config.height = 600;
    render_config.samples_per_pixel = 100;
    render_config.max_bounces = 8;

    setup_ui();

    //timer for progressive updates
    update_timer = new QTimer(this);
    connect(update_timer, &QTimer::timeout, this, &RaytracerWindow::update_render);
}

RaytracerWindow::~RaytracerWindow() {
    stop_render();
}

void RaytracerWindow::setup_ui() {
    QWidget* central = new QWidget(this);
    QVBoxLayout* main_layout = new QVBoxLayout(central);

    // render settings control panel
    QGroupBox* controls_group = new QGroupBox("Render Settings");
    QHBoxLayout* controls_layout = new QHBoxLayout(controls_group);

    width_spinbox = new QSpinBox();
    width_spinbox->setRange(64, 4096);
    width_spinbox->setValue(render_config.width);
    width_spinbox->setSuffix(" px");

    height_spinbox = new QSpinBox();
    height_spinbox->setRange(64, 4096);
    height_spinbox->setValue(render_config.height);
    height_spinbox->setSuffix(" px");

    controls_layout->addWidget(width_spinbox);
    controls_layout->addWidget(new QLabel("x"));
    controls_layout->addWidget(height_spinbox);

    QLabel* samples_label = new QLabel("Samples:");
    samples_spinbox = new QSpinBox();
    samples_spinbox->setRange(1, 10000);
    samples_spinbox->setValue(render_config.samples_per_pixel);
    controls_layout->addWidget(samples_label);
    controls_layout->addWidget(samples_spinbox);

    QLabel* bounces_label = new QLabel("Max Bounces:");
    bounces_spinbox = new QSpinBox();
    bounces_spinbox->setRange(1, 500);
    bounces_spinbox->setValue(render_config.max_bounces);
    controls_layout->addWidget(bounces_label);
    controls_layout->addWidget(bounces_spinbox);

    controls_layout->addStretch();

    render_button = new QPushButton("Render");
    stop_button = new QPushButton("Stop");
    stop_button->setEnabled(false);

    connect(render_button, &QPushButton::clicked, this, &RaytracerWindow::on_render_clicked);
    connect(stop_button, &QPushButton::clicked, this, &RaytracerWindow::on_stop_clicked);

    controls_layout->addWidget(render_button);
    controls_layout->addWidget(stop_button);

    progress_label = new QLabel("Ready");
    controls_layout->addWidget(progress_label);

    time_label = new QLabel("Time: 0.0s");
    controls_layout->addWidget(time_label);

    main_layout->addWidget(controls_group);

    // image display
    QScrollArea* scroll_area = new QScrollArea();
    scroll_area->setWidgetResizable(false);
    scroll_area->setAlignment(Qt::AlignCenter);

    image_label = new QLabel();
    image_label->setScaledContents(false);
    image_label->setAlignment(Qt::AlignCenter);

    scroll_area->setWidget(image_label);
    main_layout->addWidget(scroll_area, 1);

    setCentralWidget(central);
}

void RaytracerWindow::set_scene(const Scene* new_scene) {
    scene = new_scene;
}

void RaytracerWindow::set_camera(const Camera* new_camera) {
    camera = new_camera;
}

void RaytracerWindow::on_render_clicked() {
    start_render();
}

void RaytracerWindow::on_stop_clicked() {
    stop_render();
}

void RaytracerWindow::start_render() {
    if (!scene || !camera) {
        progress_label->setText("Error: No scene or camera");
        return;
    }

    // read config from ui
    render_config.width = width_spinbox->value();
    render_config.height = height_spinbox->value();
    render_config.samples_per_pixel = samples_spinbox->value();
    render_config.max_bounces = bounces_spinbox->value();

    //convert scene
    okaytracer::RenderScene render_scene = okaytracer::RenderScene::from_scene(scene);

    // start raytracer!
    raytracer.start_render(render_scene, *camera, render_config);

    // prepare display image
    display_image = QImage(render_config.width, render_config.height, QImage::Format_RGB32);
    display_image.setColorSpace(QColorSpace::SRgb);

    render_button->setEnabled(false);
    stop_button->setEnabled(true);
    width_spinbox->setEnabled(false);
    height_spinbox->setEnabled(false);
    samples_spinbox->setEnabled(false);
    bounces_spinbox->setEnabled(false);

    render_timer.start();

    update_timer->start(33); //30fps
}

void RaytracerWindow::stop_render() {
    raytracer.stop_render();
    update_timer->stop();

    render_button->setEnabled(true);
    stop_button->setEnabled(false);
    width_spinbox->setEnabled(true);
    height_spinbox->setEnabled(true);
    samples_spinbox->setEnabled(true);
    bounces_spinbox->setEnabled(true);

    qint64 elapsed_ms = render_timer.elapsed();
    float elapsed_sec = elapsed_ms / 1000.0f;
    progress_label->setText("Stopped");
    time_label->setText(QString("Time: %1s (stopped)").arg(elapsed_sec, 0, 'f', 2));
}

void RaytracerWindow::update_render() {
    if (!raytracer.is_rendering()) {
        stop_render();
        progress_label->setText("Complete!");
        
        qint64 elapsed_ms = render_timer.elapsed();
        float elapsed_sec = elapsed_ms / 1000.0f;
        progress_label->setText(QString("Complete! (%1 samples)").arg(render_config.samples_per_pixel));
        time_label->setText(QString("Time: %1s").arg(elapsed_sec, 0, 'f', 2));

        return;
    }

    raytracer.render_one_sample();

    // update progress...might be better on less samples?
    float progress = raytracer.get_progress();

    qint64 elapsed_ms = render_timer.elapsed();
    float elapsed_sec = elapsed_ms / 1000.0f;
    progress_label->setText(QString("Rendering... %1%").arg(int(progress * 100)));
    time_label->setText(QString("Time: %1s").arg(elapsed_sec, 0, 'f', 1));

    update_display();
}

void RaytracerWindow::update_display() {
    const std::vector<float>& pixels = raytracer.get_pixels();

    if (pixels.empty()) return;

    int width = raytracer.get_width();
    int height = raytracer.get_height();

    // convert float RGB to QImage - same method as Shirley's PPM
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            int idx = (j * width + i) * 3;

            // gamma correction (gamma = 2.0)
            float r = std::sqrt(std::clamp(pixels[idx + 0], 0.0f, 1.0f));
            float g = std::sqrt(std::clamp(pixels[idx + 1], 0.0f, 1.0f));
            float b = std::sqrt(std::clamp(pixels[idx + 2], 0.0f, 1.0f));

            int ir = int(r * 255.99f);
            int ig = int(g * 255.99f);
            int ib = int(b * 255.99f);

            display_image.setPixel(i, j, qRgb(ir, ig, ib));
        }
    }

    image_label->setPixmap(QPixmap::fromImage(display_image));
    image_label->adjustSize();
}

} // namespace ollygon
