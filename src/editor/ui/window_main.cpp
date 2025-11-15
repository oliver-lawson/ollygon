#include "window_main.hpp"
#include "panel_viewport.hpp"
#include "panel_raytracer.hpp"
#include "core/properties_panel.hpp"
#include "core/scene_operations.hpp"
#include "core/serialisation.hpp"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QShortcut>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QApplication>

namespace ollygon {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , viewport(nullptr)
    , properties_panel(nullptr)
    , scene_dock(nullptr)
    , scene_hierarchy(nullptr)
    , raytracer_window(nullptr)
{
    setWindowTitle("ollygon");
    resize(1280, 720);

    setup_cornell_box();
    setup_ui();
    create_menus();
    show_raytracer_window();
    setup_shortcuts();
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

    float room_size = 5.55f;
    float half_room = room_size * 0.5f;

    auto left_wall = std::make_unique<SceneNode>("Left Wall");
    left_wall->node_type = NodeType::Primitive;
    left_wall->primitive = std::make_unique<QuadPrimitive>(
        Vec3(0, 0, -half_room),
        Vec3(0, half_room, 0)
    );
    left_wall->transform.position = Vec3(0, half_room, -half_room);
    left_wall->material = Material::lambertian(green);
    left_wall->albedo = green;
    scene.get_root()->add_child(std::move(left_wall));

    auto right_wall = std::make_unique<SceneNode>("Right Wall");
    right_wall->node_type = NodeType::Primitive;
    right_wall->primitive = std::make_unique<QuadPrimitive>(
        Vec3(0, half_room, 0),
        Vec3(0, 0, -half_room)
    );
    right_wall->transform.position = Vec3(room_size, half_room, -half_room);
    right_wall->material = Material::lambertian(red);
    right_wall->albedo = red;
    scene.get_root()->add_child(std::move(right_wall));

    auto floor = std::make_unique<SceneNode>("Floor");
    floor->node_type = NodeType::Primitive;
    floor->primitive = std::make_unique<QuadPrimitive>(
        Vec3(-half_room, 0, 0),
        Vec3(0, 0, half_room)
    );
    floor->transform.position = Vec3(half_room, 0, -half_room);
    floor->material = Material::lambertian(white);
    floor->albedo = white;
    scene.get_root()->add_child(std::move(floor));

    auto ceiling = std::make_unique<SceneNode>("Ceiling");
    ceiling->node_type = NodeType::Primitive;
    ceiling->primitive = std::make_unique<QuadPrimitive>(
        Vec3(half_room, 0, 0),
        Vec3(0, 0, half_room)
    );
    ceiling->transform.position = Vec3(half_room, room_size, -half_room);
    ceiling->material = Material::lambertian(white);
    ceiling->albedo = white;
    scene.get_root()->add_child(std::move(ceiling));

    auto back_wall = std::make_unique<SceneNode>("Back Wall");
    back_wall->node_type = NodeType::Primitive;
    back_wall->primitive = std::make_unique<QuadPrimitive>(
        Vec3(0, half_room, 0),
        Vec3(-half_room, 0, 0)
    );
    back_wall->transform.position = Vec3(half_room, half_room, -room_size);
    back_wall->material = Material::lambertian(white);
    back_wall->albedo = white;
    scene.get_root()->add_child(std::move(back_wall));

    auto light_node = std::make_unique<SceneNode>("Area Light");
    light_node->node_type = NodeType::Light;
    light_node->light = std::make_unique<Light>();
    light_node->light->type = LightType::Area;
    light_node->light->colour = light_emission;
    light_node->light->intensity = 1.0f;
    light_node->light->is_area_light = true;

    light_node->primitive = std::make_unique<QuadPrimitive>(
        Vec3(0.65f, 0, 0),
        Vec3(0, 0, 0.525f)
    );
    light_node->transform.position = Vec3(2.775f, 5.54f, -2.775f);
    light_node->material = Material::emissive(light_emission);
    light_node->albedo = light_emission;
    scene.get_root()->add_child(std::move(light_node));

    auto tall_box = std::make_unique<SceneNode>("Tall Box");
    tall_box->node_type = NodeType::Primitive;
    tall_box->primitive = std::make_unique<CuboidPrimitive>(
        Vec3(1.65f, 3.3f, 1.65f)
    );
    tall_box->transform.position = Vec3(2.075f, 1.65f, -3.775f);
    tall_box->material = Material::lambertian(orange);
    tall_box->albedo = orange;
    scene.get_root()->add_child(std::move(tall_box));

    auto short_box = std::make_unique<SceneNode>("Short Box");
    short_box->node_type = NodeType::Primitive;
    short_box->primitive = std::make_unique<CuboidPrimitive>(
        Vec3(1.65f, 1.65f, 1.65f)
    );
    short_box->transform.position = Vec3(3.425f, 0.825f, -1.475f);
    short_box->material = Material::chequerboard(yellow, red, 4.0f);
    short_box->albedo = yellow;
    scene.get_root()->add_child(std::move(short_box));

    auto sphere = std::make_unique<SceneNode>("Sphere");
    sphere->node_type = NodeType::Primitive;
    sphere->primitive = std::make_unique<SpherePrimitive>(0.50f);
    sphere->transform.position = Vec3(3.425f, 2.150f, -1.475f);
    sphere->material = Material::metal(Colour(0.19f, 0.18f, 0.9f));
    sphere->albedo = Colour(0.19f, 0.18f, 0.9f);
    scene.get_root()->add_child(std::move(sphere));

    auto sphere2 = std::make_unique<SceneNode>("Sphere2");
    sphere2->node_type = NodeType::Primitive;
    sphere2->primitive = std::make_unique<SpherePrimitive>(0.50f);
    sphere2->transform.position = Vec3(1.415f, 3.480f, -2.335f);
    sphere2->transform.scale = 2.0f;
    sphere2->material = Material::dielectric(2.85f);
    scene.get_root()->add_child(std::move(sphere2));

    auto test_quad = std::make_unique<SceneNode>("Quad test (mesh)");
    test_quad->node_type = NodeType::Mesh;
    test_quad->geo = std::make_unique<Geo>();

    Vec3 corners[4] = {
        Vec3(1.4f,  0.0f,  0.4f),
        Vec3(-0.1f, -0.1f,  0.1f),
        Vec3(0.8f,  1.3f,  0.2f),
        Vec3(0.1f,  1.1f,  0.1f)
    };
    Vec3 normal = Vec3(0, 1.0f, 0);

    for (int i = 0; i < 4; i++) {
        test_quad->geo->add_vertex(corners[i], normal);
    }

    test_quad->geo->add_tri(2, 1, 0);
    test_quad->geo->add_tri(2, 3, 1);
    test_quad->material = Material::lambertian(Colour(0.07f, 0.01f, 0.95f));
    test_quad->albedo = Colour(0.9f, 0.01f, 0.95f);
    test_quad->transform.position = Vec3(1.5f, 3.5f, -3.5f);

    scene.get_root()->add_child(std::move(test_quad));
}

void MainWindow::create_dock_widgets() {
    // properties panel
    properties_panel = new PropertiesPanel(&selection_handler, this);
    addDockWidget(Qt::RightDockWidgetArea, properties_panel);

    // scene hierarchy panel
    scene_dock = new QDockWidget("Scene", this);
    scene_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    scene_hierarchy = new PanelSceneHierarchy();
    scene_hierarchy->set_scene(&scene);
    scene_hierarchy->set_selection_handler(&selection_handler);

    //// connect us up to signals from them
    // visible/locked toggled on properties?:
    connect(properties_panel, &PropertiesPanel::properties_changed, scene_hierarchy->tree, &SceneHierarchyTree::refresh_display);
    // scene modified from hierarchy and needs to update over onto properties?:
    connect(scene_hierarchy, &PanelSceneHierarchy::scene_modified, properties_panel, &PropertiesPanel::refresh_from_node);
    // visible/locked toggled, or items added/deleted on hierarchy?:
    connect(scene_hierarchy, &PanelSceneHierarchy::scene_modified, [this]() {
        viewport->mark_geometry_dirty();
        viewport->update();
        });

    // connect node creation/deletion
    connect(scene_hierarchy, &PanelSceneHierarchy::node_created, this, [this](SceneNode* node) {
        selection_handler.set_selected(node);
        });
    connect(scene_hierarchy, &PanelSceneHierarchy::node_deleted, this, [this]() {
        selection_handler.clear_selection();
        });

    scene_dock->setWidget(scene_hierarchy);
    addDockWidget(Qt::LeftDockWidgetArea, scene_dock);
}

void MainWindow::create_menus() {
    QMenu* file_menu = menuBar()->addMenu("&File");

    QAction* load_action = new QAction("&Open...", this);
    load_action->setShortcut(QKeySequence::Open);
    connect(load_action, &QAction::triggered, this, &MainWindow::load_scene);
    file_menu->addAction(load_action);

    QAction* save_action = new QAction("&Save", this);
    save_action->setShortcut(QKeySequence::Save);
    connect(save_action, &QAction::triggered, this, &MainWindow::save_scene);
    file_menu->addAction(save_action);

    QAction* save_as_action = new QAction("Save &As...", this);
    save_as_action->setShortcut(QKeySequence::SaveAs);
    connect(save_as_action, &QAction::triggered, this, &MainWindow::save_scene_as);
    file_menu->addAction(save_as_action);

    file_menu->addSeparator();

    QAction* exit_action = new QAction("E&xit", this);
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &QMainWindow::close);
    file_menu->addAction(exit_action);

    QMenu* view_menu = menuBar()->addMenu("&View");
    view_menu->addAction(properties_panel->toggleViewAction());
    view_menu->addAction(scene_dock->toggleViewAction());

    QMenu* render_menu = menuBar()->addMenu("&Render");
    QAction* show_raytracer_action = new QAction("Show &Raytracer", this);
    connect(show_raytracer_action, &QAction::triggered, this, &MainWindow::show_raytracer_window);
    render_menu->addAction(show_raytracer_action);
}

void MainWindow::setup_shortcuts() {
    // delete selected node
    QShortcut* delete_shortcut = new QShortcut(QKeySequence::Delete, this);
    connect(delete_shortcut, &QShortcut::activated, this, &MainWindow::on_delete_pressed);
}

void MainWindow::on_delete_pressed() {
    SceneNode* selected = selection_handler.get_selected();
    if (!selected || selected == scene.get_root()) return;

    if (SceneOperations::delete_node(&scene, selected)) {
        scene_hierarchy->rebuild_tree();
        selection_handler.clear_selection();
        viewport->mark_geometry_dirty();
        viewport->update();
    }
}

void MainWindow::save_scene() {
    if (current_filepath.isEmpty()) {
        save_scene_as();
        return;
    }

    if (SceneSerialiser::save_scene(&scene, viewport->get_camera(), current_filepath)) {
        setWindowTitle("ollygon - " + current_filepath);
    }
    else {
        QMessageBox::warning(this, "Save Failed", "Failed to save scene to file.");
    }
}

void MainWindow::save_scene_as() {
    QString filepath = QFileDialog::getSaveFileName(
        this,
        "Save Scene",
        "",
        "ollygon scene (*.oly);;All Files (*)"
    );

    if (filepath.isEmpty()) return;

    // add extension if missing
    if (!filepath.endsWith(".oly")) {
        filepath += ".oly";
    }

    current_filepath = filepath;
    save_scene();
}

void MainWindow::load_scene() {
    QString filepath = QFileDialog::getOpenFileName(
        this,
        "Open Scene",
        "",
        "ollygon Scene (*.oly);;All Files (*)"
    );

    if (filepath.isEmpty()) return;

    if (SceneSerialiser::load_scene(&scene, viewport->get_camera(), filepath)) {
        current_filepath = filepath;
        setWindowTitle("ollygon - " + filepath);
        refresh_scene_ui();
    }
    else {
        QMessageBox::warning(this, "Load Failed", "Failed to load scene from file.");
    }
}

void MainWindow::refresh_scene_ui() {
    scene_hierarchy->rebuild_tree();
    selection_handler.clear_selection();
    viewport->mark_geometry_dirty();
    viewport->update();
}

void MainWindow::show_raytracer_window()
{
    if (!raytracer_window) {
        raytracer_window = new RaytracerWindow();
        raytracer_window->set_scene(&scene);
        raytracer_window->set_camera(viewport->get_camera());
    }
    raytracer_window->resize(900, 720);

    raytracer_window->show();
    raytracer_window->raise();
    raytracer_window->activateWindow();
}

} // namespace ollygon
