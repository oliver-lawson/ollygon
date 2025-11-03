#include "window_main.hpp"
#include "panel_viewport.hpp"
#include "core/properties_panel.hpp"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QVBoxLayout>

namespace ollygon {

    MainWindow::MainWindow(QWidget* parent)
        : QMainWindow(parent)
        , viewport(nullptr)
        , properties_panel(nullptr)
        , scene_dock(nullptr)
        , scene_tree(nullptr)
    {
        setWindowTitle("ollygon");
        resize(1280, 720);

        setup_cornell_box();
        setup_ui();
        create_menus();
    }

    MainWindow::~MainWindow() = default;

    void MainWindow::setup_ui() {
        viewport = new PanelViewport(this);
        viewport->set_scene(&scene);
        viewport->set_selection_handler(&selection_handler);
        setCentralWidget(viewport);

        create_dock_widgets();
    }

    void MainWindow::setup_cornell_box() {
        // cornell box dimensions from Shirley, flipped 180deg and recentred on a different ocrner
        Colour red(0.65f, 0.05f, 0.05f);
        Colour white(0.73f, 0.73f, 0.73f);
        Colour green(0.12f, 0.45f, 0.15f);
        Colour light_emission(15.0f, 15.0f, 15.0f);
        Colour orange(1.0f, 0.6f, 0.2f);
        Colour yellow(1.0f, 0.9f, 0.3f);

        auto left_wall = std::make_unique<SceneNode>("Left Wall");
        left_wall->node_type = NodeType::Mesh;
        left_wall->geometry = std::make_unique<QuadGeometry>(
            Vec3(0, 0, 0),
            Vec3(0, 5.55f, 0),
            Vec3(0, 0, -5.55f)
        );
        left_wall->albedo = green;
        scene.get_root()->add_child(std::move(left_wall));

        auto right_wall = std::make_unique<SceneNode>("Right Wall");
        right_wall->node_type = NodeType::Mesh;
        right_wall->geometry = std::make_unique<QuadGeometry>(
            Vec3(5.55f, 0, 0),
            Vec3(0, 5.55f, 0),
            Vec3(0, 0, -5.55f)
        );
        right_wall->albedo = red;
        scene.get_root()->add_child(std::move(right_wall));

        auto floor = std::make_unique<SceneNode>("Floor");
        floor->node_type = NodeType::Mesh;
        floor->geometry = std::make_unique<QuadGeometry>(
            Vec3(5.55f, 0, 0),
            Vec3(-5.55f, 0, 0),
            Vec3(0, 0, -5.55f)
        );
        floor->albedo = white;
        scene.get_root()->add_child(std::move(floor));

        auto ceiling = std::make_unique<SceneNode>("Ceiling");
        ceiling->node_type = NodeType::Mesh;
        ceiling->geometry = std::make_unique<QuadGeometry>(
            Vec3(0, 5.55f, -5.55f),
            Vec3(5.55f, 0, 0),
            Vec3(0, 0, 5.55f)
        );
        ceiling->albedo = white;
        scene.get_root()->add_child(std::move(ceiling));

        auto back_wall = std::make_unique<SceneNode>("Back Wall");
        back_wall->node_type = NodeType::Mesh;
        back_wall->geometry = std::make_unique<QuadGeometry>(
            Vec3(5.55f, 0, -5.55f),
            Vec3(-5.55f, 0, 0),
            Vec3(0, 5.55f, 0)
        );
        back_wall->albedo = white;
        scene.get_root()->add_child(std::move(back_wall));

        auto light_node = std::make_unique<SceneNode>("Area Light");
        light_node->node_type = NodeType::Light;
        light_node->light = std::make_unique<Light>();
        light_node->light->type = LightType::Area;
        light_node->light->colour = light_emission;
        light_node->light->intensity = 1.0f;
        light_node->light->is_area_light = true;

        light_node->geometry = std::make_unique<QuadGeometry>(
            Vec3(2.12f, 5.54f, -3.32f),
            Vec3(1.30f, 0, 0),
            Vec3(0, 0, 1.05f)
        );
        light_node->albedo = light_emission;
        scene.get_root()->add_child(std::move(light_node));

        auto tall_box = std::make_unique<SceneNode>("Tall Box");
        tall_box->node_type = NodeType::Mesh;
        tall_box->geometry = std::make_unique<BoxGeometry>(
            Vec3(1.25f, 0, -4.60f),
            Vec3(2.90f, 3.30f, -2.95f)
        );
        tall_box->albedo = orange;
        scene.get_root()->add_child(std::move(tall_box));

        auto short_box = std::make_unique<SceneNode>("Short Box");
        short_box->node_type = NodeType::Mesh;
        short_box->geometry = std::make_unique<BoxGeometry>(
            Vec3(2.60f, 0, -2.30f),
            Vec3(4.25f, 1.65f, -0.65f)
        );
        short_box->albedo = yellow;
        scene.get_root()->add_child(std::move(short_box));

        auto sphere = std::make_unique<SceneNode>("Sphere");
        sphere->node_type = NodeType::Mesh;
        sphere->geometry = std::make_unique<SphereGeometry>(0.50f);
        sphere->transform.position = Vec3(3.425f, 2.150f, -1.475f);
        sphere->albedo = Colour(0.9f, 0.9f, 0.9f);
        scene.get_root()->add_child(std::move(sphere));
    }

    void MainWindow::create_dock_widgets() {
        // properties panel
        properties_panel = new PropertiesPanel(&selection_handler, this);
        addDockWidget(Qt::RightDockWidgetArea, properties_panel);

        // scene hierarchy panel
        scene_dock = new QDockWidget("Scene", this);
        scene_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

        scene_tree = new QTreeWidget();
        scene_tree->setHeaderLabel("Scene Hierarchy");

        // connect tree selection to selection handler
        connect(scene_tree, &QTreeWidget::itemClicked, [this](QTreeWidgetItem* item, int column) {
            SceneNode* node = static_cast<SceneNode*>(item->data(0, Qt::UserRole).value<void*>());
            selection_handler.set_selected(node);
            });

        populate_scene_tree();

        scene_tree->expandAll();
        scene_dock->setWidget(scene_tree);
        addDockWidget(Qt::LeftDockWidgetArea, scene_dock);
    }

    void MainWindow::populate_scene_tree() {
        scene_tree->clear();

        std::function<QTreeWidgetItem* (SceneNode*, QTreeWidgetItem*)> add_node =
            [&](SceneNode* node, QTreeWidgetItem* parent) -> QTreeWidgetItem* {
            QTreeWidgetItem* item = parent
                ? new QTreeWidgetItem(parent)
                : new QTreeWidgetItem(scene_tree);

            QString display_name = QString::fromStdString(node->name);

            // add type indicator
            switch (node->node_type) {
            case NodeType::Mesh:
                display_name += " [Mesh]";
                break;
            case NodeType::Light:
                display_name += " [Light]";
                break;
            case NodeType::Empty:
                break;
            }

            item->setText(0, display_name);
            item->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(node)));

            for (auto& child : node->children) {
                add_node(child.get(), item);
            }

            return item;
            };

        add_node(scene.get_root(), nullptr);
    }

    void MainWindow::create_menus() {
        QMenu* file_menu = menuBar()->addMenu("&File");

        QAction* exit_action = new QAction("E&xit", this);
        exit_action->setShortcut(QKeySequence::Quit);
        connect(exit_action, &QAction::triggered, this, &QMainWindow::close);
        file_menu->addAction(exit_action);

        QMenu* view_menu = menuBar()->addMenu("&View");
        view_menu->addAction(properties_panel->toggleViewAction());
        view_menu->addAction(scene_dock->toggleViewAction());
    }

} // namespace ollygon