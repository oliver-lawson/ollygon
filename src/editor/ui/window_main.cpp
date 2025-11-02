#include "window_main.hpp"
#include "panel_viewport.hpp"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QTreeWidget>

namespace ollygon {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , viewport(nullptr)
    , properties_dock(nullptr)
    , scene_dock(nullptr)
{
    setWindowTitle("ollygon");
    resize(1280, 720);

    // TEMP add our test sphere
    auto sphere_node = std::make_unique<SceneNode>("Test Sphere");
    sphere_node->sphere = std::make_unique<Sphere>(1.0f);
    sphere_node->transform.position = Vec3();
    sphere_node->sphere->albedo = Colour(0.9f, 0.5f, 0.1f);
    scene.get_root()->children.push_back(std::move(sphere_node));

    setup_ui();
    create_menus();
}

MainWindow::~MainWindow() = default;

void MainWindow::setup_ui() {
    viewport = new PanelViewport(this);
    viewport->set_scene(&scene);
    setCentralWidget(viewport);

    create_dock_widgets();
}

void MainWindow::create_dock_widgets() {
    // properties panel
    properties_dock = new QDockWidget("Properties", this);
    properties_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget* properties_widget = new QWidget();
    QVBoxLayout* properties_layout = new QVBoxLayout(properties_widget);

    // TEMP we just use oru test sphere for now
    SceneNode* sphere_node = !scene.get_root()->children.empty() ? scene.get_root()->children[0].get() : nullptr;

    if (sphere_node && sphere_node->sphere) {
        properties_layout->addWidget(new QLabel("position"));

        auto add_slider = [&](const QString& label, float& value, float min, float max) {
            QHBoxLayout* row = new QHBoxLayout();
            row->addWidget(new QLabel(label));
            QSlider* slider = new QSlider(Qt::Horizontal);
            slider->setRange(int(min * 100), int(max * 100));
            slider->setValue(int(value * 1000));

            connect(slider, &QSlider::valueChanged, [this, &value, slider]() {
                value = slider->value() / 100.0f;
                viewport->update();
            });

            row->addWidget(slider);
            properties_layout->addLayout(row);
         };
        add_slider("X:", sphere_node->transform.position.x, -10.0f, 10.0f);
        add_slider("Y:", sphere_node->transform.position.y, -10.0f, 10.0f);
        add_slider("Z:", sphere_node->transform.position.z, -10.0f, 10.0f);

        properties_layout->addSpacing(15);

        //colour controls
        properties_layout->addWidget(new QLabel("Albedo"));
        add_slider("R:", sphere_node->sphere->albedo.r, 0.0f, 1.0f);
        add_slider("G:", sphere_node->sphere->albedo.g, 0.0f, 1.0f);
        add_slider("B:", sphere_node->sphere->albedo.b, 0.0f, 1.0f);

        //properties_layout->addSpacing(15);
        properties_layout->addStretch();
        properties_dock->setWidget(properties_widget);
        addDockWidget(Qt::RightDockWidgetArea, properties_dock);

        /////
        //scene hierarchy panel on left
        //TEMP just a mvp, this will all be moved and redone
        scene_dock = new QDockWidget("Scene", this);
        scene_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

        QTreeWidget* scene_tree = new QTreeWidget();
        scene_tree->setHeaderLabel("Scene Hierarchy");

        //recursive walk through
        std::function<QTreeWidgetItem*(SceneNode*, QTreeWidgetItem*)> add_node =
            [&](SceneNode* node, QTreeWidgetItem* parent) -> QTreeWidgetItem* {
            QTreeWidgetItem* item = parent
                ? new QTreeWidgetItem(parent)
                : new QTreeWidgetItem(scene_tree);
            item->setText(0, QString::fromStdString(node->name));

            for (auto& child : node->children) {
                add_node(child.get(), item);
            }

            return item;
        };

        // populate
        add_node(scene.get_root(), nullptr);
        scene_tree->expandAll();

        scene_dock->setWidget(scene_tree);
        addDockWidget(Qt::LeftDockWidgetArea, scene_dock);
    }
}

void MainWindow::create_menus() {
    QMenu* file_menu = menuBar()->addMenu("&File");

    QAction* exit_action = new QAction("E&xit", this);
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &QMainWindow::close);
    file_menu->addAction(exit_action);

    QMenu* view_menu = menuBar()->addMenu("&View");
    view_menu->addAction(properties_dock->toggleViewAction());
    view_menu->addAction(scene_dock->toggleViewAction());
}

} // namespace ollygon
